#include "PartyImpactAudioComponent.h"
#include "PartyAudioMath.h"
#include "PartyAudioSubsystem.h"
#include "PartySoundEvent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

UPartyImpactAudioComponent::UPartyImpactAudioComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UPartyImpactAudioComponent::SetTarget(UPrimitiveComponent* Target)
{
    RequestedTarget = Target;
    if (HasBegunPlay())
    {
        Bind();
    }
}

void UPartyImpactAudioComponent::AddTarget(UPrimitiveComponent* Extra)
{
    if (Extra)
    {
        ExtraTargets.AddUnique(Extra);
        if (HasBegunPlay())
        {
            Bind();
        }
    }
}

void UPartyImpactAudioComponent::SetImpactSpeedRange(float MinSpeed, float MaxSpeed)
{
    MinImpactSpeed = FMath::Max(0.f, MinSpeed);
    MaxImpactSpeed = FMath::Max(MinImpactSpeed, MaxSpeed);
}

float UPartyImpactAudioComponent::GetSecondsSinceContact() const
{
    const UWorld* World = GetWorld();
    return World ? static_cast<float>(World->GetTimeSeconds() - LastContactTime) : TNumericLimits<float>::Max();
}

void UPartyImpactAudioComponent::BeginPlay()
{
    Super::BeginPlay();
    // Per component, so two crates of the same kind each get their own cooldown
    // and shuffle bag.
    SourceKey = FName(TEXT("Impact"), static_cast<int32>(GetUniqueID()));
    Bind();
}

void UPartyImpactAudioComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Unbind();
    Super::EndPlay(EndPlayReason);
}

void UPartyImpactAudioComponent::Bind()
{
    Unbind();

    UPrimitiveComponent* Primary = RequestedTarget.Get();
    if (!Primary && GetOwner())
    {
        Primary = Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent());
    }

    TArray<UPrimitiveComponent*, TInlineAllocator<16>> Targets;
    if (Primary)
    {
        Targets.Add(Primary);
    }
    for (const TWeakObjectPtr<UPrimitiveComponent>& Extra : ExtraTargets)
    {
        if (UPrimitiveComponent* Component = Extra.Get())
        {
            Targets.AddUnique(Component);
        }
    }

    for (UPrimitiveComponent* Target : Targets)
    {
        Target->SetNotifyRigidBodyCollision(true);
        Target->OnComponentHit.AddUniqueDynamic(this, &UPartyImpactAudioComponent::HandleHit);
        BoundTargets.Add(Target);
    }
}

void UPartyImpactAudioComponent::Unbind()
{
    for (const TWeakObjectPtr<UPrimitiveComponent>& Bound : BoundTargets)
    {
        if (UPrimitiveComponent* Target = Bound.Get())
        {
            Target->OnComponentHit.RemoveDynamic(this, &UPartyImpactAudioComponent::HandleHit);
        }
    }
    BoundTargets.Reset();
}

EPhysicalSurface UPartyImpactAudioComponent::ResolveSurface(const FHitResult& Hit, const UPrimitiveComponent* OtherComponent) const
{
    // Our own material can come back in Hit.PhysMaterial depending on which
    // side of the contact pair the solver reported first; it tells us nothing
    // about what was hit, so skip it.
    const UPhysicalMaterial* Material = Hit.PhysMaterial.Get();
    for (const TWeakObjectPtr<UPrimitiveComponent>& Bound : BoundTargets)
    {
        const UPrimitiveComponent* Target = Bound.Get();
        if (Material && Target && Target->BodyInstance.GetSimplePhysicalMaterial() == Material)
        {
            Material = nullptr;
            break;
        }
    }

    if (!Material && OtherComponent)
    {
        // Render material first: that is where level art carries its physical
        // material (PM_Wood on the wood material set, etc.), so every mesh
        // using it reports the right surface with no per-actor setup.
        // Face-accurate when the hit came from complex collision; slot 0 otherwise
        // (simple collision reports no face), which is right for single-material art.
        int32 SectionIndex = 0;
        const UMaterialInterface* Render = Hit.FaceIndex >= 0
            ? OtherComponent->GetMaterialFromCollisionFaceIndex(Hit.FaceIndex, SectionIndex)
            : nullptr;
        if (!Render && OtherComponent->GetNumMaterials() > 0)
        {
            Render = OtherComponent->GetMaterial(0);
        }
        if (Render)
        {
            Material = Render->GetPhysicalMaterial();
        }
        if (!Material || Material == GEngine->DefaultPhysMaterial)
        {
            if (const UPhysicalMaterial* Simple = OtherComponent->BodyInstance.GetSimplePhysicalMaterial())
            {
                Material = Simple;
            }
        }
    }

    return Material ? UPhysicalMaterial::DetermineSurfaceType(Material) : SurfaceType_Default;
}

void UPartyImpactAudioComponent::HandleHit(UPrimitiveComponent* HitComponent, AActor* /*OtherActor*/,
                                           UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit)
{
    const UWorld* World = GetWorld();
    if (!World || !HitComponent)
    {
        return;
    }

    const double Now = World->GetTimeSeconds();
    LastContactTime = Now;

    // A welded shape has no mass of its own; the body that actually moved is its weld parent.
    float Mass = MassOverrideKg;
    if (Mass <= 0.f)
    {
        const FBodyInstance* Body = HitComponent->GetBodyInstance();
        Mass = Body && Body->WeldParent ? Body->WeldParent->GetBodyMass() : HitComponent->GetMass();
    }
    Mass = FMath::Max(0.001f, Mass);
    // Resting contact reports, every frame, the impulse that holds the body up
    // against gravity for that frame: g * dt of speed. Take it off, or the same
    // body lying still "hits" harder the lower the frame rate (at 30 fps that's
    // 33 cm/s of phantom impact; in a hitching editor, hundreds).
    const float RestingSpeed = FMath::Abs(World->GetGravityZ()) * World->GetDeltaSeconds();
    const float DeltaSpeed = FMath::Max(0.f, static_cast<float>(NormalImpulse.Size()) / Mass - RestingSpeed);
    const float Intensity = PartyAudio::NormalizeIntensity(DeltaSpeed, MinImpactSpeed, MaxImpactSpeed);
    if (DeltaSpeed < MinImpactSpeed)
    {
        return;
    }

    const bool bRearmed = (Now - LastSoundTime) >= RetriggerSeconds
                       || Intensity >= LastSoundIntensity + RetriggerIntensityJump;
    if (!bRearmed)
    {
        return;
    }

    UPartyAudioSubsystem* Audio = UPartyAudioSubsystem::Get(this);
    if (!Audio)
    {
        return;
    }

    LastSoundTime = Now;
    LastSoundIntensity = Intensity;

    const FVector Where = Hit.ImpactPoint;

    Audio->PlayEvent(SelfEvent, Where, Intensity, SourceKey, VolumeMultiplier);

    const TObjectPtr<UPartySoundEvent>* Surface = SurfaceEvents.Find(ResolveSurface(Hit, OtherComponent));
    Audio->PlayEvent(Surface ? Surface->Get() : DefaultSurfaceEvent.Get(), Where, Intensity, SourceKey, VolumeMultiplier);
}
