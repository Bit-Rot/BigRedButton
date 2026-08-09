#include "OctoOdyssey/OctoPawn.h"
#include "PartyButtons.h"
#include "OctoOdyssey/OctoTuningSubsystem.h"
#include "Components/SphereComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
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

    // ---- SK_Okto: the shipping visual ------------------------------------
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> OctoMeshFinder(TEXT("/Game/OctoOdyssey/SK_Okto.SK_Okto"));

    OctoMesh = CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("OctoMesh"));
    OctoMesh->SetupAttachment(RootComponent);
    if (OctoMeshFinder.Object)
    {
        OctoMesh->SetSkinnedAssetAndUpdate(OctoMeshFinder.Object);
    }
    OctoMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    OctoMesh->BodyInstance.bAutoWeld = false; // as with every visual here — NoCollision alone doesn't stop autoweld
    OctoMesh->SetCastShadow(false);

    // ---- Body visual (NoCollision — BodySphere handles all collision) ----
    BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
    BodyMesh->SetupAttachment(RootComponent);
    BodyMesh->SetStaticMesh(SphereMesh);
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
    HandMeshes.SetNum(OctoArm::NumArms);

    for (int32 i = 0; i < OctoArm::NumArms; i++)
    {
        UCapsuleComponent* Collider = CreateDefaultSubobject<UCapsuleComponent>(*FString::Printf(TEXT("ArmCollider%d"), i));
        Collider->SetupAttachment(RootComponent);
        Collider->SetCollisionProfileName(TEXT("PhysicsActor"));
        // bAutoWeld defaults true — this is a QueryAndPhysics, non-simulating
        // child of a simulating parent, so it welds into BodySphere's shape
        // union at physics-state creation (see the ctor comment on
        // BodySphere->SetSimulatePhysics above).
        ArmColliders[i] = Collider;

        UStaticMeshComponent* Mesh = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("ArmMesh%d"), i));
        Mesh->SetupAttachment(RootComponent); // attaches to the ROOT, not the collider — see class comment
        Mesh->SetStaticMesh(CylinderMesh);
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

        // ---- Hand: a ball on the capsule's outer cap, one hue per arm ----
        UStaticMeshComponent* Hand = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("HandMesh%d"), i));
        Hand->SetupAttachment(RootComponent); // like the shaft, attaches to the ROOT — see class comment
        Hand->SetStaticMesh(SphereMesh);
        // No relative rotation — a sphere is rotationally symmetric.
        Hand->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Hand->BodyInstance.bAutoWeld = false;
        Hand->SetCastShadow(false);
        if (BasicMaterial)
        {
            if (UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BasicMaterial, this))
            {
                MID->SetVectorParameterValue(TEXT("Color"), OctoArm::HandColor(i, OctoArm::NumArms));
                Hand->SetMaterial(0, MID);
            }
        }
        HandMeshes[i] = Hand;
    }

    // Every size, offset and scale comes from Tuning — at this point the shipping
    // defaults, since no subsystem exists yet. OnConstruction runs this again
    // with the tuned values, still before any physics state is created.
    RebuildGeometry();
}

void AOctoPawn::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    // No subsystem outside a game world (editor viewport, CDO) — the C++
    // defaults already in Tuning are the right answer there.
    if (const UOctoTuningSubsystem* TuningSubsystem = UOctoTuningSubsystem::Get(this))
    {
        Tuning = TuningSubsystem->GetTuning();
    }

    RebuildGeometry();

    // The arm colliders WELD into BodySphere, and a welded shape takes its
    // material from the child component's own BodyInstance at the moment the
    // shape is created. Setting it here — before registration — bakes it in;
    // doing it afterwards would mean fighting the weld. ConfigureBodyPhysics
    // handles the body sphere (and re-pushes these) once we're live.
    ApplySurfaceMaterial();
}

void AOctoPawn::RebuildGeometry()
{
    if (!BodySphere) { return; }

    const float R  = Tuning.SphereRadius;
    const float Ar = Tuning.ArmRadius;
    const float Ah = Tuning.ArmHalfHeight;

    // Init* rather than Set*: this only ever runs before the physics state
    // exists (constructor / OnConstruction), so there is no body setup to
    // refresh and nothing to mark dirty.
    BodySphere->InitSphereRadius(R);

    if (OctoMesh)
    {
        // SK_Okto is authored at a ZERO arm-angle offset (Arm1 -> +Z), so the whole
        // mesh is rolled to meet the colliders rather than the asset being re-exported
        // whenever this value is tuned. Because every bone target is expressed along
        // that bone's own measured rest direction, this roll is the ONLY place
        // ArmAngleOffsetDegrees enters the skeletal path — ApplyArmMeshPose never
        // mentions it.
        OctoMesh->SetRelativeRotation(FRotator(0.f, 0.f, Tuning.ArmAngleOffsetDegrees));
    }

    ApplyMeshVisibility();

    if (BodyMesh)
    {
        // /Engine/BasicShapes/Sphere is ~100cm (1m) in diameter; scale factor == desired diameter in meters.
        BodyMesh->SetRelativeScale3D(FVector(R * 2.f / 100.f));
    }

    for (int32 i = 0; i < OctoArm::NumArms; i++)
    {
        const FRotator RelRot = GetArmRelativeRotation(i);
        const FVector  ArmDir = GetArmLocalDirection(i);

        // Rest pose: Extension == 0. Tick takes over from the next frame.
        if (ArmColliders.IsValidIndex(i) && ArmColliders[i])
        {
            ArmColliders[i]->InitCapsuleSize(Ar, Ah);
            ArmColliders[i]->SetRelativeRotation(RelRot);
            ArmColliders[i]->SetRelativeLocation(ArmDir * OctoArm::CapsuleCenterOffset(R, Ar, Ah, 0.f));
        }

        if (ArmMeshes.IsValidIndex(i) && ArmMeshes[i])
        {
            // /Engine/BasicShapes/Cylinder is 100cm diameter x 100cm tall, centered on its own origin, long axis local Z.
            //
            // The shaft is deliberately NOT the capsule's shape: it is drawn thinner
            // (ArmMeshRadiusScale) and shorter (OctoArm::ArmMeshLength stops it at the hand
            // center rather than the capsule tip), so it has its own center offset. The
            // collider above is unaffected by both.
            const float MeshRadius = Ar * Tuning.ArmMeshRadiusScale;
            ArmMeshes[i]->SetRelativeScale3D(FVector(MeshRadius * 2.f / 100.f,
                                                     MeshRadius * 2.f / 100.f,
                                                     OctoArm::ArmMeshLength(Ar, Ah) / 100.f));
            ArmMeshes[i]->SetRelativeRotation(RelRot);
            ArmMeshes[i]->SetRelativeLocation(ArmDir * OctoArm::ArmMeshCenterOffset(R, Ar, Ah, 0.f));
        }

        if (HandMeshes.IsValidIndex(i) && HandMeshes[i])
        {
            // Radius == ArmRadius exactly, so the hand's surface IS the capsule cap's surface.
            HandMeshes[i]->SetRelativeScale3D(FVector(Ar * 2.f / 100.f));
            HandMeshes[i]->SetRelativeLocation(ArmDir * OctoArm::HandCenterOffset(R, Ar, Ah, 0.f));
        }
    }
}

void AOctoPawn::ApplyMeshVisibility()
{
    const bool bShowPrototype = Tuning.bShowPrototypeMeshes;

    if (OctoMesh)
    {
        OctoMesh->SetVisibility(true);
    }

    // The prototype shapes are hidden, not deleted, and TickArm keeps moving them
    // either way: they are drawn from the same OctoArm:: offsets the colliders use,
    // so switching them on is the direct check that SK_Okto is tracking collision.
    if (BodyMesh)
    {
        BodyMesh->SetVisibility(bShowPrototype);
    }
    for (const TObjectPtr<UStaticMeshComponent>& Mesh : ArmMeshes)
    {
        if (Mesh) { Mesh->SetVisibility(bShowPrototype); }
    }
    for (const TObjectPtr<UStaticMeshComponent>& Mesh : HandMeshes)
    {
        if (Mesh) { Mesh->SetVisibility(bShowPrototype); }
    }
}

void AOctoPawn::BeginPlay()
{
    Super::BeginPlay();
    ConfigureBodyPhysics();

    // Only now do the poseable mesh's bone arrays exist (AllocateTransformData runs
    // on registration), so this is the earliest the rig can be measured. A failure
    // leaves ArmBones empty, which every pose write below treats as "leave SK_Okto
    // at its rest pose" — the octopus renders fully extended but still plays.
    //
    // The arms and the head are measured independently and neither gates the other:
    // an export that renamed a head bone should still animate eight working arms.
    if (OctoMesh && OctoMesh->GetSkinnedAsset())
    {
        if (const USkeletalMesh* Skeletal = Cast<USkeletalMesh>(OctoMesh->GetSkinnedAsset()))
        {
            OctoSkeleton::BuildArmBones(*Skeletal, ArmBones);
            OctoSkeleton::BuildBodyBones(*Skeletal, BodyBones);
        }
    }

    // Hit events are half of the squash trigger (the other half is the head's own
    // sweep). This is an event flag, not geometry: it touches no shape and cannot
    // break the weld, so unlike RebuildGeometry it is safe here.
    if (BodySphere)
    {
        BodySphere->SetNotifyRigidBodyCollision(true);
        BodySphere->OnComponentHit.AddDynamic(this, &AOctoPawn::OnBodyHit);
    }

    if (ArmBones.IsEmpty() && !BodyBones.IsValid())
    {
        UE_LOG(LogPartyButtons, Warning,
            TEXT("AOctoPawn: no usable SK_Okto rig — the skeletal mesh will not follow the arms."));
        return;
    }

    // A component's tick carries NO implicit dependency on its actor's, so without
    // this the skeletal refresh can run before TickArms has written the frame's pose
    // and the mesh trails the colliders by a frame — 20cm at ExtendSpeed 1200. The
    // prototype meshes never had this problem: SetRelativeLocation applies at once.
    OctoMesh->PrimaryComponentTick.AddPrerequisite(this, PrimaryActorTick);

    // Retracted is the octopus's actual starting state; without this it would spawn
    // at the reference pose (fully extended) for one frame.
    for (int32 i = 0; i < OctoArm::NumArms; i++)
    {
        ApplyArmMeshPose(i, Arms[i].Extension);
    }

    // Seed the head particle ON its anchor. Left uninitialised it would spring in from
    // the world origin on frame one, which at spawn height reads as the octopus whipping
    // its head up off the floor for no reason.
    ResetBodySpring();

    OctoMesh->MarkRefreshTransformDirty();
}

void AOctoPawn::ResetBodySpring()
{
    SquashAmount   = 0.f;
    SquashVelocity = 0.f;

    if (!OctoMesh || !BodyBones.IsValid())
    {
        return;
    }

    PrevHeadTargetWorld = OctoMesh->GetComponentTransform().TransformPosition(BodyBones.TipRestCS);
    HeadSpring.Reset(PrevHeadTargetWorld);

    // Put the chain back on its reference pose so a disabled or reset spring leaves no
    // trace of whatever bend was up at the time.
    TArray<FTransform>& Pose = OctoMesh->BoneSpaceTransforms;
    for (int32 i = 0; i < BodyBones.ChainIndices.Num(); i++)
    {
        const int32 BoneIndex = BodyBones.ChainIndices[i];
        if (Pose.IsValidIndex(BoneIndex))
        {
            Pose[BoneIndex] = BodyBones.RefLocal[i];
        }
    }
}

void AOctoPawn::ApplyArmMeshPose(int32 ArmIndex, float Extension)
{
    if (!OctoMesh || !ArmBones.IsValidIndex(ArmIndex)) { return; }

    const FOctoArmBones& Bones = ArmBones[ArmIndex];

    TArray<FTransform>& Pose = OctoMesh->BoneSpaceTransforms;
    if (!Pose.IsValidIndex(Bones.ChainRootIndex) || !Pose.IsValidIndex(Bones.HandIndex)) { return; }

    // Where this arm's tip has to be. The SAME expression that places the hand
    // sphere and the arm capsule's outer cap, so mesh and collider cannot drift.
    const float TipOffset = OctoArm::HandCenterOffset(
        Tuning.SphereRadius, Tuning.ArmRadius, Tuning.ArmHalfHeight, Extension);

    // ---- Arm{N}: stretch the chain ----------------------------------------
    //
    // Written to the chain ROOT only. A bone's scale composes into its children, so
    // one write moves Arm{N}_001, Arm{N}_002 and the chain tip together; scaling all
    // three would compound to s^3 and overshoot badly.
    const float Scale = OctoArm::ArmChainLengthScale(Bones.ChainOriginOffset, Bones.RestChainLength, TipOffset);
    Pose[Bones.ChainRootIndex].SetScale3D(OctoArm::LengthAxisScale(Bones.LengthAxis, Scale));

    // ---- Hand{N}: place it on the tip -------------------------------------
    //
    // The hand hangs off Root rather than off the chain, so the stretch above does
    // not carry it — it is positioned outright. Rotation and scale are left at their
    // reference values: the hand is a ball, and it is meant to stay the collider's size.
    const FVector TargetCS = Bones.RestDirection * TipOffset;
    Pose[Bones.HandIndex].SetTranslation(Bones.HandParentRestCS.InverseTransformPosition(TargetCS));
}

void AOctoPawn::TickBodySpring(float DeltaSeconds)
{
    if (!OctoMesh || !BodyBones.IsValid() || DeltaSeconds <= 0.f)
    {
        return;
    }

    if (!Tuning.bBodySpring)
    {
        // Cheap and total: restore the reference pose and park the particle on its anchor,
        // so toggling the spring back on mid-round starts from rest instead of from a stale
        // deflection the player never saw accumulate.
        ResetBodySpring();
        return;
    }

    // The MESH component's transform, not the actor's — the component carries the
    // ArmAngleOffsetDegrees roll (see RebuildGeometry), so the actor's transform would
    // place the head 22.5 degrees off its own bones.
    const FTransform MeshTransform = OctoMesh->GetComponentTransform();
    const FVector    TargetWorld   = MeshTransform.TransformPosition(BodyBones.TipRestCS);

    // Differentiated rather than read off the body: this picks up the octopus's ROLL for
    // free, and roll is what swings the head hardest. An explicit v + w x r would have to
    // rediscover that, and would drift out of step with whatever moved the actor.
    const FVector TargetVelocity = (TargetWorld - PrevHeadTargetWorld) / DeltaSeconds;
    PrevHeadTargetWorld = TargetWorld;

    const FVector SweepStart = HeadSpring.Position;

    // Droop plus air drag. The drag term is what makes the head trail CONSISTENTLY while the
    // octopus travels, rather than only reacting when it changes direction — see
    // OctoBody::DragAccel and FOctoTuning::BodyAirDrag.
    const FVector ExtraAccel =
        FVector(0.f, 0.f, Tuning.WorldGravityZ * Tuning.BodyGravityScale) +
        OctoBody::DragAccel(TargetVelocity, Tuning.BodyAirDrag, Tuning.BodySpringFrequencyHz);

    OctoBody::StepSpring(
        HeadSpring, TargetWorld, TargetVelocity,
        Tuning.BodySpringFrequencyHz, Tuning.BodySpringDamping, ExtraAccel, DeltaSeconds);

    if (Tuning.bHeadCollision)
    {
        ResolveHeadCollision(SweepStart);
    }

    // Pin the particle to the play plane. See the declaration for why: X motion on a chain
    // aimed down the camera's own axis reads as scaling, and a front face pushing along X
    // could otherwise park the head off-plane for good.
    HeadSpring.Position.X = TargetWorld.X;
    HeadSpring.Velocity.X = 0.f;

    OctoBody::StepSquash(
        SquashAmount, SquashVelocity,
        Tuning.HeadSquashFrequencyHz, Tuning.HeadSquashDamping, DeltaSeconds);

    const FVector HeadCS     = MeshTransform.InverseTransformPosition(HeadSpring.Position);
    const FVector Deflection = OctoBody::ConstrainDeflection(
        (HeadCS - BodyBones.TipRestCS) * Tuning.BodyLagScale, Tuning.BodyMaxDeflection);

    TArray<FTransform> ChainPose;
    FTransform         TipCS = FTransform::Identity;
    OctoBody::BuildChainBend(
        BodyBones, BodyBones.TipRestCS + Deflection,
        Tuning.BodyMaxBendDegrees, Tuning.BodyBendTaper, Tuning.bHeadTipRigid, ChainPose, &TipCS);

    TArray<FTransform>& Pose = OctoMesh->BoneSpaceTransforms;
    for (int32 i = 0; i < BodyBones.ChainIndices.Num(); i++)
    {
        const int32 BoneIndex = BodyBones.ChainIndices[i];
        if (ChainPose.IsValidIndex(i) && Pose.IsValidIndex(BoneIndex))
        {
            Pose[BoneIndex] = ChainPose[i];
        }
    }

    // ---- Squash: the tip's scale, on top of the bend ----------------------
    //
    // Written last and multiplied in, because BuildChainBend restores the reference scale
    // on every bone it touches — assigning it before the bend would silently do nothing.
    if (Tuning.bHeadSquash && SquashAmount > 0.f)
    {
        const int32 TipIndex = BodyBones.ChainIndices.Last();
        if (Pose.IsValidIndex(TipIndex))
        {
            // The squash axes are the TIP BONE's, so the normal has to leave component
            // space through the tip's POSED rotation — through its reference one, a bent
            // head would squash along the wrong axis.
            const FVector NormalInBone = TipCS.GetRotation().UnrotateVector(SquashNormalCS);
            Pose[TipIndex].SetScale3D(
                Pose[TipIndex].GetScale3D() * OctoBody::SquashScale(NormalInBone, SquashAmount));
        }
    }
}

void AOctoPawn::ResolveHeadCollision(const FVector& StartWorld)
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    // See FOctoTuning::HeadCollisionOffsetX — the head bone sits ~85cm out of the play
    // plane, so where this sphere is tested is a tuning decision, not a fixed one.
    const FVector Offset(Tuning.HeadCollisionOffsetX, 0.f, 0.f);
    const FVector Start = StartWorld + Offset;
    const FVector End   = HeadSpring.Position + Offset;

    if ((End - Start).IsNearlyZero())
    {
        return;
    }

    // Same object types as the arm sweep, and the same reason for QueryParams ignoring this
    // actor: without it the head would collide with the octopus's own arm capsules.
    FCollisionObjectQueryParams ObjectParams;
    ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
    ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(OctoHeadSweep), /*bTraceComplex=*/false, this);

    FHitResult Hit;
    const bool bBlocked = World->SweepSingleByObjectType(
        Hit, Start, End, FQuat::Identity, ObjectParams,
        FCollisionShape::MakeSphere(Tuning.SphereRadius), QueryParams);

    if (!bBlocked)
    {
        return;
    }

    // Already inside something: let the spring pull itself out rather than solving a
    // penetration we have no good answer for. Fighting it here is how a head gets stuck.
    if (Hit.bStartPenetrating)
    {
        return;
    }

    HeadSpring.Position = Hit.Location - Offset;

    const FVector Normal      = Hit.ImpactNormal;
    const float   NormalSpeed = static_cast<float>(FVector::DotProduct(HeadSpring.Velocity, Normal));
    const FVector Tangential  = HeadSpring.Velocity - Normal * NormalSpeed;

    // Only motion INTO the surface bounces; motion already leaving it is left alone, so a
    // head skimming out of a contact isn't yanked back down.
    const float OutwardSpeed = (NormalSpeed < 0.f)
        ? -NormalSpeed * FMath::Clamp(Tuning.HeadCollisionRestitution, 0.f, 1.f)
        : NormalSpeed;

    HeadSpring.Velocity =
        Tangential * (1.f - FMath::Clamp(Tuning.HeadCollisionFriction, 0.f, 1.f)) + Normal * OutwardSpeed;

    if (NormalSpeed < 0.f)
    {
        AddSquashImpulse(Normal, -NormalSpeed);
    }
}

void AOctoPawn::AddSquashImpulse(const FVector& WorldNormal, float Speed)
{
    if (!Tuning.bHeadSquash || !OctoMesh || Tuning.HeadSquashFullSpeed <= 0.f)
    {
        return;
    }

    const float Target = Tuning.HeadSquashMax * FMath::Clamp(Speed / Tuning.HeadSquashFullSpeed, 0.f, 1.f);

    // Strongest hit wins for the frame. A softer graze arriving right after a hard landing
    // must not reset the wobble the landing started — it has nothing new to say.
    if (Target <= SquashAmount)
    {
        return;
    }

    SquashAmount   = Target;
    SquashVelocity = 0.f;
    SquashNormalCS = OctoMesh->GetComponentTransform().InverseTransformVectorNoScale(WorldNormal).GetSafeNormal();
}

void AOctoPawn::OnBodyHit(UPrimitiveComponent* /*HitComponent*/, AActor* /*OtherActor*/,
                          UPrimitiveComponent* /*OtherComponent*/, FVector NormalImpulse, const FHitResult& Hit)
{
    // Impulse over mass is the speed the contact actually took out of the body, which is a
    // far better measure of "hit it hard" than the raw closing speed — a graze along a floor
    // carries plenty of speed and almost no impulse.
    const float Mass = FMath::Max(1.f, Tuning.BodyMassKg);
    AddSquashImpulse(Hit.ImpactNormal, static_cast<float>(NormalImpulse.Size()) / Mass);
}

void AOctoPawn::ConfigureBodyPhysics()
{
    // CreateDOFLock (invoked via SetConstraintMode) anchors the constraint
    // at the body's CURRENT world location and early-outs unless the body
    // is already dynamic. By BeginPlay, SpawnActor has placed us at our
    // final spawn transform AND SetSimulatePhysics(true) already ran in the
    // constructor, so both preconditions hold.
    //
    // The DOF lock is the one thing here that must NOT be re-applied later,
    // which is why ApplyLiveTuning calls the pieces below rather than this.
    BodySphere->SetConstraintMode(EDOFMode::YZPlane);

    ApplyBodyPhysicsTuning();
    ApplySurfaceMaterial();
}

void AOctoPawn::ApplyBodyPhysicsTuning()
{
    if (!BodySphere) { return; }

    BodySphere->SetMassOverrideInKg(NAME_None, Tuning.BodyMassKg, true);
    BodySphere->SetLinearDamping(Tuning.LinearDamping);
    BodySphere->SetAngularDamping(Tuning.AngularDamping);
    BodySphere->SetUseCCD(Tuning.bUseCCD);
    if (FBodyInstance* BI = BodySphere->GetBodyInstance())
    {
        BI->SetMaxDepenetrationVelocity(Tuning.MaxDepenetrationVelocity);
    }
}

void AOctoPawn::ApplySurfaceMaterial()
{
    if (!Tuning.bOverridePhysicalMaterial)
    {
        // Leave the Chaos defaults alone. Nothing to un-apply: the only way
        // bOverridePhysicalMaterial can go true->false is via the dev menu,
        // whose Accept path reloads the course anyway.
        return;
    }

    if (!SurfaceMaterial)
    {
        // The project ships no UPhysicalMaterial assets, so there is nothing to
        // load — build one and keep it for the pawn's lifetime.
        SurfaceMaterial = NewObject<UPhysicalMaterial>(this, NAME_None, RF_Transient);
    }

    if (!SurfaceMaterial) { return; }

    SurfaceMaterial->Friction       = Tuning.SurfaceFriction;
    SurfaceMaterial->StaticFriction = Tuning.SurfaceStaticFriction;
    SurfaceMaterial->Restitution    = Tuning.SurfaceRestitution;

    if (BodySphere)
    {
        BodySphere->SetPhysMaterialOverride(SurfaceMaterial);
    }
    for (const TObjectPtr<UCapsuleComponent>& Collider : ArmColliders)
    {
        if (Collider)
        {
            Collider->SetPhysMaterialOverride(SurfaceMaterial);
        }
    }
}

void AOctoPawn::ApplyLiveTuning(const FOctoTuning& NewTuning)
{
    // Geometry fields are copied but NOT acted on: RebuildGeometry may only run
    // before the physics state exists (see the class comment). They take effect
    // on the next spawn, which is exactly what the menu's Accept button forces.
    Tuning = NewTuning;

    ApplyBodyPhysicsTuning();
    ApplySurfaceMaterial();

    // Visibility is a render flag, not geometry — it touches no shape and no weld,
    // so unlike RebuildGeometry it can be previewed mid-round.
    ApplyMeshVisibility();
}

void AOctoPawn::SetPhysicsFrozen(bool bFrozen)
{
    if (bPhysicsFrozen == bFrozen) { return; }
    bPhysicsFrozen = bFrozen;

    // Deliberately NOT SetSimulatePhysics(false/true): that tears down and
    // rebuilds the body instance, and this body is a welded shape union (the
    // eight arm capsules) carrying a DOF lock that CreateDOFLock will only
    // re-anchor under conditions this class already documents as delicate. A
    // weld that failed to re-form would silently stop the arms colliding at
    // all. Switching gravity off and parking the body is just as still, and
    // touches nothing structural.
    BodySphere->SetEnableGravity(!bFrozen);

    if (bFrozen)
    {
        // Drop every held arm: presses that arrive during the intro are the
        // player dismissing the tutorial, not steering the octopus. The game
        // mode replays whatever is still held once the round goes live (see
        // AOctoGameMode::ShouldReplayHeldButtonsOnStart).
        for (int32 i = 0; i < OctoArm::NumArms; i++)
        {
            Arms[i].bPressed = false;
        }

        BodySphere->SetPhysicsLinearVelocity(FVector::ZeroVector);
        BodySphere->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);

        // Tick early-outs while frozen, so a head left mid-wobble would hang there for the
        // whole intro. Park it — "frozen" should look frozen.
        ResetBodySpring();
        if (OctoMesh && BodyBones.IsValid())
        {
            OctoMesh->MarkRefreshTransformDirty();
        }
        return;
    }

    // Re-anchor before the first live tick. PrevHeadTargetWorld is stale by however far
    // the octopus was moved during the intro, and the spring differentiates that gap into
    // an anchor velocity — a respawn would otherwise launch the head across the level.
    ResetBodySpring();

    // Nothing has touched the octopus during the intro, so it's very likely
    // asleep by now — gravity alone won't wake it.
    BodySphere->WakeRigidBody();
}

void AOctoPawn::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (bPhysicsFrozen) { return; }

    TickArms(DeltaSeconds);
    TickBodySpring(DeltaSeconds);

    // One flush for every bone written this frame — marking dirty per writer would re-walk
    // the whole bone hierarchy nine times a frame for exactly the same result.
    if (OctoMesh && (!ArmBones.IsEmpty() || BodyBones.IsValid()))
    {
        OctoMesh->MarkRefreshTransformDirty();
    }
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
    return OctoArm::ArmDirectionLocal(ArmIndex, OctoArm::NumArms, Tuning.ArmAngleOffsetDegrees);
}

FRotator AOctoPawn::GetArmRelativeRotation(int32 ArmIndex) const
{
    const float ThetaDeg = Tuning.ArmAngleOffsetDegrees + 360.f * static_cast<float>(ArmIndex) / static_cast<float>(OctoArm::NumArms);
    return FRotator(0.f, 0.f, ThetaDeg);
}

void AOctoPawn::TickArms(float DeltaSeconds)
{
    // Every arm's push-off is measured against the SAME frame-start velocity,
    // and none is applied until all eight have been stepped. Applying them as
    // they're found would make the result depend on arm order: an octopus
    // wedged in a gap with two opposing arms planted would see each arm in turn
    // "correct" the velocity the other just set, and buzz. Batched, opposing
    // pushes cancel — which is right, since a wedged octopus shouldn't move.
    const FVector FrameStartVelocity = BodySphere->GetPhysicsLinearVelocity();

    TArray<FPendingPush> Pushes;
    Pushes.Reserve(OctoArm::NumArms);

    for (int32 i = 0; i < OctoArm::NumArms; i++)
    {
        TickArm(i, DeltaSeconds, FrameStartVelocity, Pushes);
    }

    // The pose is NOT flushed here — Tick does it once after TickBodySpring has written
    // the head chain too.

    if (Pushes.IsEmpty()) { return; }

    for (const FPendingPush& Push : Pushes)
    {
        if (Tuning.bPushAtImpactPoint)
        {
            BodySphere->AddImpulseAtLocation(Push.Impulse, Push.Location);
        }
        else
        {
            // Through the centre of mass: no torque, so a corner push launches
            // cleanly instead of tumbling the body.
            BodySphere->AddImpulse(Push.Impulse);
        }
    }

    BodySphere->SetPhysicsLinearVelocity(
        OctoArm::ClampLaunchVelocity(BodySphere->GetPhysicsLinearVelocity(), Tuning.MaxLaunchSpeed));

    // Angular counterpart. Off-centre pushes are the whole reason the octopus
    // tumbles, and AngularDamping alone takes seconds to bleed a hard spin off.
    if (Tuning.MaxAngularSpeedDeg > 0.f)
    {
        BodySphere->SetPhysicsAngularVelocityInDegrees(
            OctoArm::ClampLaunchVelocity(BodySphere->GetPhysicsAngularVelocityInDegrees(), Tuning.MaxAngularSpeedDeg));
    }
}

void AOctoPawn::TickArm(int32 ArmIndex, float DeltaSeconds, const FVector& FrameStartVelocity, TArray<FPendingPush>& OutPushes)
{
    FOctoArmState& Arm = Arms[ArmIndex];

    const float R  = Tuning.SphereRadius;
    const float Ar = Tuning.ArmRadius;
    const float Ah = Tuning.ArmHalfHeight;

    const float SafeMax  = FMath::Min(Tuning.ArmMaxExtension, OctoArm::MaxSafeExtension(R, Ar, Ah));
    const float E        = Arm.Extension;
    const float EDesired = OctoArm::StepExtension(E, Arm.bPressed, Tuning.ExtendSpeed, Tuning.RetractSpeed, SafeMax, DeltaSeconds);

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

        const float StartOffset = OctoArm::CapsuleCenterOffset(R, Ar, Ah, E);
        const float EndOffset   = OctoArm::CapsuleCenterOffset(R, Ar, Ah, EDesired);
        const FVector Start = Origin + WorldDir * StartOffset;
        const FVector End   = Origin + WorldDir * EndOffset;

        FCollisionObjectQueryParams ObjectParams;
        ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
        ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);
        if (Tuning.bSweepPhysicsBodies)
        {
            // Off by default: with this channel absent an arm passes straight
            // through physics props instead of pushing off them. QueryParams
            // already ignores this actor, so we can't plant an arm on ourselves.
            ObjectParams.AddObjectTypesToQuery(ECC_PhysicsBody);
        }

        FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(OctoArmSweep), /*bTraceComplex=*/false, this);

        const float SweepRadius     = FMath::Max(1.f, Ar - Tuning.ArmSweepSkin);
        const float SweepHalfHeight = FMath::Max(SweepRadius, Ah - Tuning.ArmSweepSkin);

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
    const FVector ArmDir = GetArmLocalDirection(ArmIndex);

    if (ArmMeshes.IsValidIndex(ArmIndex) && ArmMeshes[ArmIndex])
    {
        const float MeshOffset = OctoArm::ArmMeshCenterOffset(R, Ar, Ah, EAchieved);
        ArmMeshes[ArmIndex]->SetRelativeLocation(ArmDir * MeshOffset);
    }

    if (HandMeshes.IsValidIndex(ArmIndex) && HandMeshes[ArmIndex])
    {
        const float HandOffset = OctoArm::HandCenterOffset(R, Ar, Ah, EAchieved);
        HandMeshes[ArmIndex]->SetRelativeLocation(ArmDir * HandOffset);
    }

    // SK_Okto follows the same achieved extension. TickArms flushes the pose once
    // all eight arms have been written.
    ApplyArmMeshPose(ArmIndex, EAchieved);

    // ---- Push-off: hold the body to the arm's own extension speed ----
    //
    // Applied EVERY tick the arm is planted and still trying to extend, not
    // just on the free->blocked transition — an absolute arm doesn't stop
    // pushing halfway through its stroke. It self-limits: the moment the body
    // leaves the surface the sweep above finds nothing, bBlocked goes false,
    // and the push ends. Once the stroke runs out (EAchieved == SafeMax) the
    // fully-extended arm holds the body up as ordinary welded collision
    // geometry instead, which is why no separate "sustain force" exists.
    if (bBlocked && Arm.bPressed && EAchieved < SafeMax)
    {
        const FVector PushDir = -GetActorQuat().RotateVector(GetArmLocalDirection(ArmIndex));
        const float Deficit = OctoArm::PushOffSpeedDeficit(FrameStartVelocity, PushDir, Tuning.ExtendSpeed);

        if (Deficit > 0.f)
        {
            // Scaling the impulse by mass is what makes the launch depend on
            // arm SPEED alone. AddImpulseAtLocation gives dV = J/m and
            // dW = I^-1 (r x J); with J proportional to m and I proportional
            // to m, mass cancels from both — so retuning BodyMassKg changes how
            // the octopus takes hits without touching how high it flies. The
            // impact point (rather than the centre of mass) is the default:
            // it's what makes a corner push tumble the body — see
            // FOctoTuning::bPushAtImpactPoint.
            OutPushes.Add({ PushDir * (Deficit * Tuning.BodyMassKg), Hit.ImpactPoint });
        }

        // ---- Grip: the friction the push-off cannot provide ----
        //
        // PushOffSpeedDeficit only ever adds speed ALONG the arm and leaves
        // motion across the contact untouched, which is what makes pushing off
        // feel slippery. Contact friction can't take up the slack either: a
        // planted arm's collider is teleported by ApplyArmColliderExtension, so
        // Chaos sees a moved shape carrying no shape velocity and the tangential
        // constraint has almost nothing to bite on during the stroke.
        //
        // So model it directly — bleed off the velocity perpendicular to the
        // arm while it's planted. Batched into OutPushes with everything else so
        // opposing arms still cancel rather than fight (see TickArms).
        if (Tuning.ArmGripFraction > 0.f)
        {
            const FVector Tangential = FVector::VectorPlaneProject(FrameStartVelocity, PushDir);
            if (!Tangential.IsNearlyZero())
            {
                OutPushes.Add({ -Tangential * (Tuning.ArmGripFraction * Tuning.BodyMassKg), Hit.ImpactPoint });
            }
        }
    }

    Arm.Extension = EAchieved;
}

void AOctoPawn::ApplyArmColliderExtension(int32 ArmIndex, float Extension)
{
    if (!ArmColliders.IsValidIndex(ArmIndex) || !ArmColliders[ArmIndex]) { return; }

    const float Offset = OctoArm::CapsuleCenterOffset(
        Tuning.SphereRadius, Tuning.ArmRadius, Tuning.ArmHalfHeight, Extension);
    ArmColliders[ArmIndex]->SetRelativeLocation(GetArmLocalDirection(ArmIndex) * Offset);
}
