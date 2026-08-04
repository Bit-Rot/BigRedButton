#include "OctoOdyssey/OctoPawn.h"
#include "PartyButtons.h"
#include "Components/SphereComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/HitResult.h"
#include "WorldCollision.h"
#include "CollisionQueryParams.h"

AOctoPawn::AOctoPawn()
{
    PrimaryActorTick.bCanEverTick = true;
    AutoPossessPlayer = EAutoReceiveInput::Disabled; // never possessed — see class comment
    AutoPossessAI     = EAutoPossessAI::Disabled;

    // ---- Body ------------------------------------------------------------
    BodySphere = CreateDefaultSubobject<USphereComponent>(TEXT("BodySphere"));
    RootComponent = BodySphere;
    BodySphere->InitSphereRadius(SphereRadius);
    BodySphere->SetCollisionProfileName(TEXT("PhysicsActor")); // QueryAndPhysics, object type PhysicsBody
    BodySphere->SetGenerateOverlapEvents(true); // so AOctoGoalFlag's Trigger sees us

    // Flagged to simulate HERE, before the arm colliders/meshes below are
    // created and attached — welding is evaluated per-child at the moment
    // ITS OWN physics state is created, against the parent's simulate
    // intent at that time. Doing this any later (e.g. in BeginPlay) risks
    // the arm colliders creating independent, unwelded bodies instead of
    // joining BodySphere's union. SetConstraintMode (the DOF lock) is the
    // one call that DOES have to wait for BeginPlay — see BeginPlay's comment.
    BodySphere->SetSimulatePhysics(true);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMeshFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMeshFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

    UStaticMesh* SphereMesh          = SphereMeshFinder.Object;
    UStaticMesh* CylinderMesh        = CylinderMeshFinder.Object;
    UMaterialInterface* BasicMaterial = BasicMaterialFinder.Object;

    // ---- Body visual (NoCollision — BodySphere handles all collision) ----
    BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
    BodyMesh->SetupAttachment(RootComponent);
    BodyMesh->SetStaticMesh(SphereMesh);
    // /Engine/BasicShapes/Sphere is ~100cm (1m) in diameter; scale factor == desired diameter in meters.
    BodyMesh->SetRelativeScale3D(FVector(SphereRadius * 2.f / 100.f));
    BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BodyMesh->BodyInstance.bAutoWeld = false; // NoCollision alone does not stop autoweld
    BodyMesh->SetCastShadow(false);
    if (BasicMaterial)
    {
        if (UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BasicMaterial, this))
        {
            MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.65f, 0.15f, 0.55f));
            BodyMesh->SetMaterial(0, MID);
        }
    }

    // ---- Arms --------------------------------------------------------------
    ArmColliders.SetNum(OctoArm::NumArms);
    ArmMeshes.SetNum(OctoArm::NumArms);

    for (int32 i = 0; i < OctoArm::NumArms; i++)
    {
        const FRotator RelRot = GetArmRelativeRotation(i);
        // Rest pose: Extension == 0.
        const float RestOffset = OctoArm::CapsuleCenterOffset(SphereRadius, ArmRadius, ArmHalfHeight, 0.f);
        const FVector RestLocation = GetArmLocalDirection(i) * RestOffset;

        UCapsuleComponent* Collider = CreateDefaultSubobject<UCapsuleComponent>(*FString::Printf(TEXT("ArmCollider%d"), i));
        Collider->SetupAttachment(RootComponent);
        Collider->InitCapsuleSize(ArmRadius, ArmHalfHeight);
        Collider->SetCollisionProfileName(TEXT("PhysicsActor"));
        Collider->SetRelativeRotation(RelRot);
        Collider->SetRelativeLocation(RestLocation);
        // bAutoWeld defaults true — this is a QueryAndPhysics, non-simulating
        // child of a simulating parent, so it welds into BodySphere's shape
        // union at physics-state creation (see the ctor comment on
        // BodySphere->SetSimulatePhysics above).
        ArmColliders[i] = Collider;

        UStaticMeshComponent* Mesh = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("ArmMesh%d"), i));
        Mesh->SetupAttachment(RootComponent); // attaches to the ROOT, not the collider — see class comment
        Mesh->SetStaticMesh(CylinderMesh);
        // /Engine/BasicShapes/Cylinder is 100cm diameter x 100cm tall, centered on its own origin, long axis local Z.
        Mesh->SetRelativeScale3D(FVector(ArmRadius * 2.f / 100.f, ArmRadius * 2.f / 100.f, ArmHalfHeight * 2.f / 100.f));
        Mesh->SetRelativeRotation(RelRot);
        Mesh->SetRelativeLocation(RestLocation);
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Mesh->BodyInstance.bAutoWeld = false;
        Mesh->SetCastShadow(false);
        if (BasicMaterial)
        {
            if (UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BasicMaterial, this))
            {
                MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.85f, 0.35f, 0.70f));
                Mesh->SetMaterial(0, MID);
            }
        }
        ArmMeshes[i] = Mesh;
    }
}

void AOctoPawn::BeginPlay()
{
    Super::BeginPlay();

    // CreateDOFLock (invoked via SetConstraintMode) anchors the constraint
    // at the body's CURRENT world location and early-outs unless the body
    // is already dynamic. By BeginPlay, SpawnActor has placed us at our
    // final spawn transform AND SetSimulatePhysics(true) already ran in the
    // constructor, so both preconditions hold.
    BodySphere->SetConstraintMode(EDOFMode::YZPlane);
    BodySphere->SetMassOverrideInKg(NAME_None, BodyMassKg, true);
    BodySphere->SetLinearDamping(LinearDamping);
    BodySphere->SetAngularDamping(AngularDamping);
    BodySphere->SetUseCCD(true);
    if (FBodyInstance* BI = BodySphere->GetBodyInstance())
    {
        BI->SetMaxDepenetrationVelocity(MaxDepenetrationVelocity);
    }
}

void AOctoPawn::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    TickArms(DeltaSeconds);
}

void AOctoPawn::NotifyArmPressed(int32 ArmIndex)
{
    if (ArmIndex < 0 || ArmIndex >= OctoArm::NumArms) { return; }
    Arms[ArmIndex].bPressed = true;
}

void AOctoPawn::NotifyArmReleased(int32 ArmIndex)
{
    if (ArmIndex < 0 || ArmIndex >= OctoArm::NumArms) { return; }
    Arms[ArmIndex].bPressed = false;
}

FVector AOctoPawn::GetArmLocalDirection(int32 ArmIndex) const
{
    return OctoArm::ArmDirectionLocal(ArmIndex, OctoArm::NumArms, ArmAngleOffsetDegrees);
}

FRotator AOctoPawn::GetArmRelativeRotation(int32 ArmIndex) const
{
    const float ThetaDeg = ArmAngleOffsetDegrees + 360.f * static_cast<float>(ArmIndex) / static_cast<float>(OctoArm::NumArms);
    return FRotator(0.f, 0.f, ThetaDeg);
}

void AOctoPawn::TickArms(float DeltaSeconds)
{
    for (int32 i = 0; i < OctoArm::NumArms; i++)
    {
        TickArm(i, DeltaSeconds);
    }
}

void AOctoPawn::TickArm(int32 ArmIndex, float DeltaSeconds)
{
    FOctoArmState& Arm = Arms[ArmIndex];

    const float SafeMax = FMath::Min(ArmMaxExtension, OctoArm::MaxSafeExtension(SphereRadius, ArmRadius, ArmHalfHeight));
    const float E        = Arm.Extension;
    const float EDesired = OctoArm::StepExtension(E, Arm.bPressed, ExtendSpeed, RetractSpeed, SafeMax, DeltaSeconds);

    float EAchieved  = EDesired;
    bool  bBlocked   = false;
    FHitResult Hit;

    // Only an EXTENDING arm can penetrate something — retraction moves out
    // of contact and never needs to sweep (see class comment).
    if (EDesired > E && GetWorld())
    {
        const FVector LocalDir   = GetArmLocalDirection(ArmIndex);
        const FVector WorldDir   = GetActorQuat().RotateVector(LocalDir);
        const FQuat   ArmWorldQuat = GetActorQuat() * GetArmRelativeRotation(ArmIndex).Quaternion();
        const FVector Origin     = GetActorLocation();

        const float StartOffset = OctoArm::CapsuleCenterOffset(SphereRadius, ArmRadius, ArmHalfHeight, E);
        const float EndOffset   = OctoArm::CapsuleCenterOffset(SphereRadius, ArmRadius, ArmHalfHeight, EDesired);
        const FVector Start = Origin + WorldDir * StartOffset;
        const FVector End   = Origin + WorldDir * EndOffset;

        FCollisionObjectQueryParams ObjectParams;
        ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
        ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);

        FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(OctoArmSweep), /*bTraceComplex=*/false, this);

        const float SweepRadius     = FMath::Max(1.f, ArmRadius - ArmSweepSkin);
        const float SweepHalfHeight = FMath::Max(SweepRadius, ArmHalfHeight - ArmSweepSkin);

        bBlocked = GetWorld()->SweepSingleByObjectType(
            Hit, Start, End, ArmWorldQuat, ObjectParams,
            FCollisionShape::MakeCapsule(SweepRadius, SweepHalfHeight), QueryParams);

        if (bBlocked)
        {
            EAchieved = E + Hit.Distance;
        }
    }

    ApplyArmColliderExtension(ArmIndex, EAchieved);

    // Visuals follow the ACHIEVED extension, not the desired one, so they
    // always match collision (no gap ever opens between arm and body,
    // no visible clipping through world geometry).
    if (ArmMeshes.IsValidIndex(ArmIndex) && ArmMeshes[ArmIndex])
    {
        const float MeshOffset = OctoArm::CapsuleCenterOffset(SphereRadius, ArmRadius, ArmHalfHeight, EAchieved);
        ArmMeshes[ArmIndex]->SetRelativeLocation(GetArmLocalDirection(ArmIndex) * MeshOffset);
    }

    // ---- Launch (once, on the free->blocked transition) and sustain ----
    if (bBlocked && !Arm.bBlockedLastTick)
    {
        const float BlockedFrac = OctoArm::BlockedFraction(EDesired - E, EAchieved - E);
        const float ImpulsePerUnitSpeed = ExtendSpeed > 0.f ? (ArmLaunchImpulse / ExtendSpeed) : 0.f;
        const float J = OctoArm::LaunchImpulseMagnitude(BlockedFrac, ExtendSpeed, ImpulsePerUnitSpeed, ArmLaunchImpulse);

        if (J > 0.f)
        {
            const FVector WorldDir = GetActorQuat().RotateVector(GetArmLocalDirection(ArmIndex));
            BodySphere->AddImpulseAtLocation(-WorldDir * J, Hit.ImpactPoint);
            BodySphere->SetPhysicsLinearVelocity(
                OctoArm::ClampLaunchVelocity(BodySphere->GetPhysicsLinearVelocity(), MaxLaunchSpeed));
        }
    }
    if (bBlocked && Arm.bPressed && EAchieved < SafeMax)
    {
        const FVector WorldDir = GetActorQuat().RotateVector(GetArmLocalDirection(ArmIndex));
        BodySphere->AddForceAtLocation(-WorldDir * ArmSustainForce, Hit.ImpactPoint);
    }

    Arm.Extension        = EAchieved;
    Arm.bBlockedLastTick  = bBlocked;
}

void AOctoPawn::ApplyArmColliderExtension(int32 ArmIndex, float Extension)
{
    if (!ArmColliders.IsValidIndex(ArmIndex) || !ArmColliders[ArmIndex]) { return; }

    const float Offset = OctoArm::CapsuleCenterOffset(SphereRadius, ArmRadius, ArmHalfHeight, Extension);
    ArmColliders[ArmIndex]->SetRelativeLocation(GetArmLocalDirection(ArmIndex) * Offset);
}
