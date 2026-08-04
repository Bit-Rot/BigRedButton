#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Containers/StaticArray.h"
#include "OctoOdyssey/OctoArmMath.h"
#include "OctoPawn.generated.h"

class USphereComponent;
class UCapsuleComponent;
class UStaticMeshComponent;

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
 *     +- ArmCollider[0..7] : UCapsuleComponent   welded (autoweld) into BodySphere's rigid body
 *     +- BodyMesh          : UStaticMeshComponent  visual only, NoCollision
 *     +- ArmMesh[0..7]     : UStaticMeshComponent  visual only, NoCollision
 *
 * Arm meshes attach to the ROOT, not to the arm colliders, and every visual
 * mesh disables autoweld explicitly (NoCollision alone does not stop a
 * UStaticMeshComponent from welding — see BodyInstance.bAutoWeld below).
 * This is what makes swapping to a skeletal mesh later a matter of deleting
 * 9 mesh components and adding one USkeletalMeshComponent driven from the
 * same OctoArm:: math — nothing in the collision path touches a mesh.
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
 * lets the collider teleport into penetration), while TickArm fires an
 * explicit impulse on the free->blocked transition (for launching — see
 * ArmLaunchImpulse). ApplyArmColliderExtension is the one function to swap
 * if the collider-move mechanism itself ever needs to change (e.g. to the
 * FPhysicsInterface::SetLocalTransform fallback noted in its comment).
 */
UCLASS()
class PARTYBUTTONS_API AOctoPawn : public APawn
{
    GENERATED_BODY()

public:
    AOctoPawn();

    /** Forwarded from AOctoGameMode::OnPlayerButton. */
    void NotifyArmPressed(int32 ArmIndex);

    /** Forwarded from AOctoGameMode::OnPlayerButtonReleased. */
    void NotifyArmReleased(int32 ArmIndex);

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    // ---- Geometry tunables (baked into components at construction — see
    // PartyArena's class comment for why this codebase tunes these by
    // editing the C++ default rather than via a Blueprint class-defaults
    // override) ----

    UPROPERTY(EditDefaultsOnly, Category = "Octo|Geometry")
    float SphereRadius = 50.f;

    UPROPERTY(EditDefaultsOnly, Category = "Octo|Geometry")
    float ArmRadius = 12.f;

    /** UE capsule half-height INCLUDES the hemisphere caps (total capsule length = 2*ArmHalfHeight). */
    UPROPERTY(EditDefaultsOnly, Category = "Octo|Geometry")
    float ArmHalfHeight = 30.f;

    /** Clamped in TickArm to OctoArm::MaxSafeExtension(SphereRadius, ArmRadius, ArmHalfHeight) as a hard safety backstop. */
    UPROPERTY(EditDefaultsOnly, Category = "Octo|Geometry")
    float ArmMaxExtension = 45.f;

    /** Angle of arm 0 from +Z; arm i sits at AngleOffset + 360*i/8. 22.5 keeps no arm pointing straight down. */
    UPROPERTY(EditDefaultsOnly, Category = "Octo|Geometry")
    float ArmAngleOffsetDegrees = 22.5f;

    UPROPERTY(EditDefaultsOnly, Category = "Octo|Geometry")
    float ExtendSpeed = 450.f;

    UPROPERTY(EditDefaultsOnly, Category = "Octo|Geometry")
    float RetractSpeed = 300.f;

    /** Shrinks the swept collision test so an arm already resting on a surface doesn't self-trigger. */
    UPROPERTY(EditDefaultsOnly, Category = "Octo|Geometry")
    float ArmSweepSkin = 1.f;

    // ---- Physics tunables ----

    /** Fixed mass regardless of arm extension — every arm move triggers UpdateMassProperties; without this override total mass would breathe and impulse tuning would drift. */
    UPROPERTY(EditDefaultsOnly, Category = "Octo|Physics")
    float BodyMassKg = 40.f;

    UPROPERTY(EditDefaultsOnly, Category = "Octo|Physics")
    float LinearDamping = 0.2f;

    UPROPERTY(EditDefaultsOnly, Category = "Octo|Physics")
    float AngularDamping = 0.5f;

    /** Impulse (kg*cm/s) applied once on a fully-blocked arm's free->blocked transition. */
    UPROPERTY(EditDefaultsOnly, Category = "Octo|Physics")
    float ArmLaunchImpulse = 9000.f;

    /** Sustained force (kg*cm/s^2) applied while an arm stays blocked and still trying to extend — lets two "planted" arms lift the body. */
    UPROPERTY(EditDefaultsOnly, Category = "Octo|Physics")
    float ArmSustainForce = 18000.f;

    UPROPERTY(EditDefaultsOnly, Category = "Octo|Physics")
    float MaxLaunchSpeed = 1400.f;

    /** Per-body backstop in case the swept clamp is ever bypassed. */
    UPROPERTY(EditDefaultsOnly, Category = "Octo|Physics")
    float MaxDepenetrationVelocity = 150.f;

private:
    struct FOctoArmState
    {
        bool  bPressed         = false;
        float Extension        = 0.f;
        bool  bBlockedLastTick = false;
    };

    void TickArms(float DeltaSeconds);
    void TickArm(int32 ArmIndex, float DeltaSeconds);

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

    /** Body-local direction of arm ArmIndex (X always 0) — see OctoArm::ArmDirectionLocal. */
    FVector GetArmLocalDirection(int32 ArmIndex) const;

    /** Relative rotation of arm ArmIndex's capsule/mesh (FRotator(0,0,Theta), fixed for the arm's lifetime). */
    FRotator GetArmRelativeRotation(int32 ArmIndex) const;

    UPROPERTY(VisibleAnywhere, Category = "Octo")
    TObjectPtr<USphereComponent> BodySphere;

    UPROPERTY(VisibleAnywhere, Category = "Octo")
    TObjectPtr<UStaticMeshComponent> BodyMesh;

    UPROPERTY(VisibleAnywhere, Category = "Octo")
    TArray<TObjectPtr<UCapsuleComponent>> ArmColliders;

    UPROPERTY(VisibleAnywhere, Category = "Octo")
    TArray<TObjectPtr<UStaticMeshComponent>> ArmMeshes;

    TStaticArray<FOctoArmState, OctoArm::NumArms> Arms;
};
