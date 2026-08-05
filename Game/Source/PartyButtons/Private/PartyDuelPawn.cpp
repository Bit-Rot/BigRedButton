#include "PartyDuelPawn.h"
#include "PartyButtons.h"
#include "PartyDuelMath.h"
#include "PartyBullet.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/ArrowComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/HitResult.h"

APartyDuelPawn::APartyDuelPawn()
{
    PrimaryActorTick.bCanEverTick = true;

    SphereComponent = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere"));
    RootComponent = SphereComponent;
    SphereComponent->InitSphereRadius(SphereRadiusMeters * 100.f);
    SphereComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    SphereComponent->SetCollisionObjectType(ECC_Pawn);
    SphereComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
    SphereComponent->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap); // bullets
    SphereComponent->SetGenerateOverlapEvents(true);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMeshFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    MeshComponent->SetupAttachment(RootComponent);
    MeshComponent->SetStaticMesh(SphereMeshFinder.Object);
    // /Engine/BasicShapes/Sphere is ~100cm (1m) in diameter; scale to 2*SphereRadiusMeters meters.
    MeshComponent->SetRelativeScale3D(FVector(SphereRadiusMeters * 2.f));
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision); // SphereComponent handles all collision
    if (BasicMaterialFinder.Object)
    {
        // Init() swaps this for a per-player tinted MID once PlayerIndex is known.
        MeshComponent->SetMaterial(0, BasicMaterialFinder.Object);
    }

    FacingArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("FacingArrow"));
    FacingArrow->SetupAttachment(RootComponent);
    FacingArrow->SetHiddenInGame(false); // must be visible in-game to show facing
    FacingArrow->ArrowSize   = 2.f;
    FacingArrow->ArrowLength = SphereRadiusMeters * 100.f * 1.25f; // half of the original 2.5x length
    FacingArrow->SetRelativeLocation(FVector(SphereRadiusMeters * 100.f, 0.f, 0.f));

    BulletClass = APartyBullet::StaticClass();
}

void APartyDuelPawn::Init(int32 InPlayerIndex, const FLinearColor& InColor)
{
    PlayerIndex = InPlayerIndex;
    BaseColor   = InColor;

    // Cache the MID so state transitions can retint the body (base ↔ reflect flash).
    BodyMID = MeshComponent->CreateAndSetMaterialInstanceDynamic(0);
    ApplyBodyColor(/*bReflectFlash=*/false);

    FacingArrow->SetArrowColor(InColor);
}

void APartyDuelPawn::ApplyBodyColor(bool bReflectFlash)
{
    if (!BodyMID) { return; }

    FLinearColor Target;
    if (bReflectFlash)
    {
        Target = ReflectFlashColor;
    }
    else
    {
        // Fade the base tint toward black proportional to accumulated damage
        // so remaining HP reads at a glance without a separate health bar.
        // Full HP -> full tint. On the killing hit Die() runs before this
        // helper does, so we never render a fully-black living pawn.
        const int32 Cap        = FMath::Max(1, MaxHits);
        const int32 Remaining  = FMath::Clamp(Cap - HitsTaken, 0, Cap);
        const float Brightness = static_cast<float>(Remaining) / static_cast<float>(Cap);
        Target = FLinearColor(
            BaseColor.R * Brightness,
            BaseColor.G * Brightness,
            BaseColor.B * Brightness,
            BaseColor.A);
    }

    // BasicShapeMaterial exposes "Color"; harmless no-op if a designer swaps
    // in a base material that names its input differently. We also poke
    // "EmissiveColor" so a genuinely emissive base material glows during the
    // window without breaking the fallback tint path.
    BodyMID->SetVectorParameterValue(TEXT("Color"),         Target);
    BodyMID->SetVectorParameterValue(TEXT("EmissiveColor"), bReflectFlash ? Target : FLinearColor::Black);
}

void APartyDuelPawn::BeginPlay()
{
    Super::BeginPlay();
    EnterIdle();
}

void APartyDuelPawn::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!bAlive) { return; }

    if (State == EDuelState::Idle || State == EDuelState::ReflectWindow)
    {
        AddActorLocalRotation(FRotator(0.f, IdleSpinRateDegPerSec * DeltaSeconds, 0.f));
    }

    if (State == EDuelState::ReflectWindow && GetWorld() && GetWorld()->GetTimeSeconds() >= ReflectWindowEndTime)
    {
        EnterIdle();
    }
}

void APartyDuelPawn::EnterIdle()
{
    State = EDuelState::Idle;
    ApplyBodyColor(/*bReflectFlash=*/false);
}

void APartyDuelPawn::NotifyPressed()
{
    if (!bAlive || State == EDuelState::Charging) { return; }

    State          = EDuelState::Charging;
    PressWorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
}

void APartyDuelPawn::NotifyReleased()
{
    if (!bAlive || State != EDuelState::Charging) { return; }

    const double Now         = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    const float HeldSeconds  = static_cast<float>(Now - PressWorldTime);

    switch (PartyDuel::ClassifyRelease(HeldSeconds, TapWindowSeconds, MinChargeSeconds))
    {
    case PartyDuel::EReleaseAction::Reflect:
        State                = EDuelState::ReflectWindow;
        ReflectWindowEndTime = Now + ReflectActiveSeconds;
        ApplyBodyColor(/*bReflectFlash=*/true);
        break;

    case PartyDuel::EReleaseAction::Shoot:
        FireBullet(HeldSeconds);
        EnterIdle();
        break;

    case PartyDuel::EReleaseAction::Cancel:
    default:
        EnterIdle();
        break;
    }
}

void APartyDuelPawn::FireBullet(float HeldSeconds)
{
    if (!BulletClass || !GetWorld()) { return; }

    const float Speed = PartyDuel::ChargeToSpeed(
        HeldSeconds, MinChargeSeconds, MaxChargeSeconds, MinBulletSpeed, MaxBulletSpeed);

    const FVector Facing  = GetActorForwardVector();
    const FVector SpawnLoc = GetActorLocation() + Facing * (SphereRadiusMeters * 100.f * 1.5f);

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    if (APartyBullet* Bullet = GetWorld()->SpawnActor<APartyBullet>(BulletClass, SpawnLoc, GetActorRotation(), Params))
    {
        Bullet->Fire(Facing, Speed, this);
    }
}

bool APartyDuelPawn::NotifyHitByBullet(APartyBullet* Bullet, const FHitResult& Hit)
{
    if (!bAlive || !Bullet) { return false; }

    if (State == EDuelState::ReflectWindow)
    {
        FVector Normal = Hit.ImpactNormal;
        Normal.Z = 0.f; // the whole game plays out in the XY plane
        if (Normal.IsNearlyZero())
        {
            // Overlap-only hits can carry a zero normal (e.g. spawned already
            // overlapping); fall back to the radial direction from this
            // pawn's center to the bullet.
            Normal = (Bullet->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
        }
        if (Normal.IsNearlyZero())
        {
            Normal = GetActorForwardVector();
        }

        const FVector OutVelocity = PartyDuel::Reflect(Bullet->GetVelocity(), Normal.GetSafeNormal());
        Bullet->Redirect(OutVelocity, this);
        return true;
    }

    ++HitsTaken;
    UE_LOG(LogPartyButtons, Log,
        TEXT("APartyDuelPawn: player %d took hit %d/%d."),
        PlayerIndex + 1, HitsTaken, FMath::Max(1, MaxHits));

    if (HitsTaken >= FMath::Max(1, MaxHits))
    {
        Die();
    }
    else
    {
        // Refresh the base tint so the damage fade is visible immediately.
        // (EnterIdle would do the same, but we may currently be Charging.)
        ApplyBodyColor(/*bReflectFlash=*/false);
    }
    return false;
}

void APartyDuelPawn::Die()
{
    if (!bAlive) { return; }
    bAlive = false;

    UE_LOG(LogPartyButtons, Log, TEXT("APartyDuelPawn: player %d died."), PlayerIndex + 1);

    OnDied.Broadcast(PlayerIndex);
    Destroy();
}
