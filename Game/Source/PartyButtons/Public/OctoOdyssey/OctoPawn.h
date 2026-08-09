#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Containers/StaticArray.h"
#include "OctoOdyssey/OctoArmMath.h"
#include "OctoOdyssey/OctoSkeleton.h"
#include "OctoOdyssey/OctoTuning.h"
#include "OctoPawn.generated.h"

class USphereComponent;
class UCapsuleComponent;
class UStaticMeshComponent;
class UPoseableMeshComponent;
class UPhysicalMaterial;

/**
 * AOctoPawn
 *
 * The shared physics octopus for OctoOdyssey. NOT possessed — per the
 * project's player model (see APartyDuelPawn's class comment), AOctoGameMode
 * calls NotifyArmPressed()/NotifyArmReleased() directly instead of routing
 * input through possession. Up to 8 players each own one arm.
 *
 * Component hierarchy:
 *   BodySphere (root, the ONLY simulating body)
 *     +- ArmCollider[0..7] : UCapsuleComponent      welded (autoweld) into BodySphere's rigid body
 *     +- OctoMesh          : UPoseableMeshComponent SK_Okto, the shipping visual
 *     +- BodyMesh          : UStaticMeshComponent   prototype visual, hidden by default
 *     +- ArmMesh[0..7]     : UStaticMeshComponent   prototype visual, hidden by default
 *     +- HandMesh[0..7]    : UStaticMeshComponent   prototype visual, hidden by default
 *
 * Every visual attaches to the ROOT, not to the arm colliders, and disables
 * autoweld explicitly (NoCollision alone does not stop a mesh component from
 * welding — see BodyInstance.bAutoWeld below). Nothing in the collision path
 * touches a mesh, which is what made the skeletal-mesh swap a pure addition:
 * the 17 prototype shapes are still built and still updated, just invisible
 * unless Tuning.bShowPrototypeMeshes turns them back on to check the skeleton
 * against the collision it is supposed to be tracking.
 *
 * Skeletal posing: COLLISION IS THE SOURCE OF TRUTH. The bones follow the
 * colliders, never the reverse. SK_Okto's reference pose is the octopus at full
 * extension with a zero arm-angle offset, so posing an arm is two writes —
 * a length scale on Arm{N} and a position on Hand{N} — both derived from the
 * same OctoArm:: offsets the colliders use, and both driven by the ACHIEVED
 * extension so the mesh can never render an arm through a wall the collider
 * stopped at. See ApplyArmMeshPose and OctoSkeleton.h for the rig contract.
 *
 * "Arms are one unchangeable unit; no outside force may change their
 * relative position" is satisfied by construction: welded shapes are
 * entries in the root particle's shape union and have no independent
 * degree of freedom. A prismatic constraint per arm could be violated under
 * heavy contact; a welded union entry cannot.
 *
 * Motion: BodySphere simulates physics with EDOFMode::YZPlane, which locks
 * world-X translation and Y/Z rotation (swing), leaving Y/Z translation and
 * X rotation (twist/roll) free — exactly "moves in the Y-Z plane, rotates
 * only about X."
 *
 * Arm extension mechanism: moving a collider does NOT touch the physics
 * particle's velocity (Chaos only sees a new overlap, resolved by damped,
 * frame-rate-dependent pushout — unusable as a launch source). So the two
 * jobs are split: ApplyArmColliderExtension moves REAL collision geometry
 * (for propping/blocking/rolling — see TickArm's swept clamp, which never
 * lets the collider teleport into penetration), while TickArm applies an
 * explicit push-off for launching. ApplyArmColliderExtension is the one
 * function to swap if the collider-move mechanism itself ever needs to change
 * (e.g. to the FPhysicsInterface::SetLocalTransform fallback noted in its
 * comment).
 *
 * The arms are ABSOLUTE: no external force may slow an extending arm down, so
 * a planted arm extending at ExtendSpeed throws the body away from the contact
 * at exactly ExtendSpeed. TickArm implements that as a velocity FLOOR rather
 * than a force — how high the octopus flies is set by how fast its arms move,
 * not by how hard they push, and not by how heavy it is. This deliberately
 * replaced an earlier force/impulse model whose numbers could not beat gravity
 * (two planted arms managed 900 cm/s^2 against gravity's 980) and in which
 * ExtendSpeed had no effect on launch power at all.
 *
 * Tuning: every number this class uses lives in FOctoTuning (OctoTuning.h), not
 * in per-property members here. Values split into two groups by WHEN they can be
 * applied:
 *
 *   Geometry (radii, half-height, angles) is baked into the components by
 *   RebuildGeometry, which must run BEFORE the physics state is created —
 *   i.e. from the constructor or from OnConstruction, never later. Resizing a
 *   shape after it has welded into BodySphere's shape union risks silently
 *   breaking the weld, and a broken weld means the arms stop colliding at all
 *   with no visible symptom. OnConstruction is the window the dev tuning menu
 *   uses: AActor::PostSpawnInitialize runs ExecuteConstruction before
 *   PostActorConstruction registers the components, so applying tuning there is
 *   exactly equivalent to having edited FOctoTuning's defaults.
 *
 *   Everything else (mass, damping, clamps, surface, arm speeds, grip) goes
 *   through ApplyLiveTuning and can change mid-round, which is what lets the
 *   menu preview a value while you are actually playing.
 */
UCLASS()
class PARTYBUTTONS_API AOctoPawn : public APawn
{
    GENERATED_BODY()

public:
    AOctoPawn();

    /** Forwarded from AOctoGameMode::OnGameplayButton. */
    void NotifyArmPressed(int32 ArmIndex);

    /** Forwarded from AOctoGameMode::OnGameplayButtonReleased. */
    void NotifyArmReleased(int32 ArmIndex);

    /**
     * Park/release the octopus for the intro overlay (see
     * APartyMinigameGameMode::SetGameplayFrozen). Suspending the actor's TICK
     * would not be enough — that stops the arms but leaves the body falling
     * under gravity — so this switches gravity off and zeroes its velocities
     * instead. See the implementation for why it deliberately avoids
     * SetSimulatePhysics.
     */
    void SetPhysicsFrozen(bool bFrozen);

    /**
     * Re-apply everything that can change without rebuilding the body: mass,
     * damping, CCD, depenetration cap and the surface material. Geometry fields
     * in NewTuning are stored but NOT applied — see the class comment for why
     * they can only be baked in before the physics state exists.
     *
     * Called by the dev tuning menu on every value change, so the octopus can be
     * felt out mid-round.
     */
    void ApplyLiveTuning(const FOctoTuning& NewTuning);

    /** The values this pawn was built with. Read-only outside ApplyLiveTuning. */
    const FOctoTuning& GetTuning() const { return Tuning; }

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    /**
     * Pulls the live tuning from UOctoTuningSubsystem (if there is one) and bakes
     * its geometry into the components. Runs before component registration, which
     * is the only safe window for that — see the class comment.
     */
    virtual void OnConstruction(const FTransform& Transform) override;

    /**
     * Every tunable this pawn uses. Defaults come from FOctoTuning's member
     * initialisers; OnConstruction overwrites the whole struct from the tuning
     * subsystem when one exists.
     */
    UPROPERTY(EditDefaultsOnly, Category = "Octo")
    FOctoTuning Tuning;

private:
    struct FOctoArmState
    {
        bool  bPressed  = false;
        float Extension = 0.f;
    };

    /** One arm's push-off for this frame, applied by TickArms after every arm has been stepped. */
    struct FPendingPush
    {
        FVector Impulse;
        FVector Location;
    };

    void TickArms(float DeltaSeconds);

    /**
     * Steps one arm and, if it is planted against something, appends this
     * frame's push-off to OutPushes. Never touches the body's velocity itself —
     * see TickArms for why every arm's push must be computed against the same
     * frame-start velocity.
     */
    void TickArm(int32 ArmIndex, float DeltaSeconds, const FVector& FrameStartVelocity, TArray<FPendingPush>& OutPushes);

    /** One-time BeginPlay setup: the DOF lock, then everything ApplyLiveTuning also does. */
    void ConfigureBodyPhysics();

    /** Mass override, damping, CCD and depenetration cap — the re-appliable subset. */
    void ApplyBodyPhysicsTuning();

    /**
     * Size and place every component from Tuning's geometry fields. Called from
     * the constructor and again from OnConstruction — and from NOWHERE else, for
     * the weld reason in the class comment.
     */
    void RebuildGeometry();

    /**
     * Build (or update) the transient UPhysicalMaterial from Tuning's surface
     * fields and push it onto the body and every arm collider. No-op when
     * bOverridePhysicalMaterial is false, which leaves the Chaos defaults in
     * place — the behaviour before surface tuning existed.
     */
    void ApplySurfaceMaterial();

    /**
     * Moves ArmColliders[ArmIndex] to the pose for the given Extension (a
     * welded UnWeld+Weld geometry rebuild — see class comment). THE one
     * function to swap for the FPhysicsInterface::SetLocalTransform
     * fallback if profiling ever shows this too expensive: cache 9 shape
     * handles via GetAllShapes_AssumesLocked in BeginPlay, then call
     * FPhysicsInterface::SetLocalTransform inside FPhysicsCommand::ExecuteWrite
     * (requires adding "PhysicsCore"/"Chaos" to PartyButtons.Build.cs).
     */
    void ApplyArmColliderExtension(int32 ArmIndex, float Extension);

    /**
     * Pose SK_Okto's arm ArmIndex for the given extension: a length scale on the
     * Arm{N} chain root (which propagates down the chain, so the two bones below it
     * need no write of their own) and a component-space position for Hand{N}.
     *
     * Does NOT flush the pose — TickArms marks the component dirty once after all
     * eight arms rather than eight times.
     */
    void ApplyArmMeshPose(int32 ArmIndex, float Extension);

    /**
     * Show SK_Okto, and show or hide the 17 prototype shapes per
     * Tuning.bShowPrototypeMeshes. Visibility touches no physics, so unlike
     * RebuildGeometry this is safe to call at any time.
     */
    void ApplyMeshVisibility();

    /** Body-local direction of arm ArmIndex (X always 0) — see OctoArm::ArmDirectionLocal. */
    FVector GetArmLocalDirection(int32 ArmIndex) const;

    /** Relative rotation of arm ArmIndex's capsule/mesh (FRotator(0,0,Theta), fixed for the arm's lifetime). */
    FRotator GetArmRelativeRotation(int32 ArmIndex) const;

    UPROPERTY(VisibleAnywhere, Category = "Octo")
    TObjectPtr<USphereComponent> BodySphere;

    /**
     * SK_Okto. Poseable rather than a full USkeletalMeshComponent on purpose: the
     * octopus has no authored animation at all — every bone it moves is computed
     * from the collision state — so an anim graph would be an empty layer to route
     * around. UPoseableMeshComponent takes bone-space writes directly, which keeps
     * the whole rig in C++ and unit-testable (OctoArm::ArmChainLengthScale).
     */
    UPROPERTY(VisibleAnywhere, Category = "Octo")
    TObjectPtr<UPoseableMeshComponent> OctoMesh;

    UPROPERTY(VisibleAnywhere, Category = "Octo")
    TObjectPtr<UStaticMeshComponent> BodyMesh;

    UPROPERTY(VisibleAnywhere, Category = "Octo")
    TArray<TObjectPtr<UCapsuleComponent>> ArmColliders;

    UPROPERTY(VisibleAnywhere, Category = "Octo")
    TArray<TObjectPtr<UStaticMeshComponent>> ArmMeshes;

    /**
     * "Hands" — a ball at each arm's tip, radius ArmRadius, drawn coincident with the
     * arm capsule's outer cap so its visible surface is exactly the collider's tip
     * surface. One distinct hue per arm (OctoArm::HandColor).
     */
    UPROPERTY(VisibleAnywhere, Category = "Octo")
    TArray<TObjectPtr<UStaticMeshComponent>> HandMeshes;

    /**
     * Transient material carrying Tuning's friction/restitution, shared by the
     * body sphere and all 8 arm colliders. Created on first use — the project
     * ships no UPhysicalMaterial assets, so there is nothing to load.
     */
    UPROPERTY(Transient)
    TObjectPtr<UPhysicalMaterial> SurfaceMaterial;

    TStaticArray<FOctoArmState, OctoArm::NumArms> Arms;

    /**
     * SK_Okto's arms as measured from its reference skeleton, resolved once in
     * BeginPlay (the bone arrays do not exist at construction time). Empty means
     * the mesh is missing or its rig does not match, in which case the octopus
     * renders at its rest pose rather than crashing — see OctoSkeleton::BuildArmBones.
     */
    TArray<FOctoArmBones> ArmBones;

    /** True while the intro overlay has the round paused — see SetPhysicsFrozen. */
    bool bPhysicsFrozen = false;
};
