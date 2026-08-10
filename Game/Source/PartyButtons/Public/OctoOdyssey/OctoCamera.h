#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OctoOdyssey/OctoTuning.h"
#include "OctoCamera.generated.h"

class UCameraComponent;
class AOctoPawn;

/**
 * AOctoCamera
 *
 * Side-on follow camera for OctoOdyssey. The project never possesses pawns
 * (see APartyDuelPawn's class comment for the pattern this mirrors), so the
 * view comes from APlayerController::SetViewTarget on this actor — exactly
 * what APartyArenaGameMode::SpawnArena does with APartyArena's top-down
 * camera, just oriented for a side-on 2.5D view instead.
 *
 * The camera's relative rotation is IDENTITY and is never touched:
 * FRotator::ZeroRotator looks along +X, which is exactly the side-on view of
 * the Y-Z play plane the octopus is confined to. Only position follows.
 */
UCLASS()
class PARTYBUTTONS_API AOctoCamera : public AActor
{
    GENERATED_BODY()

public:
    AOctoCamera();

    UCameraComponent* GetCamera() const { return CameraComponent; }

    /** Begin following Target. Call once after both actors exist. */
    void SetFollowTarget(AOctoPawn* Target);

    /**
     * The X plane this run is being played on — the camera parks CameraDistanceX
     * in front of it.
     *
     * This used to be FOctoTuning::PlayPlaneX and nothing else, because there was
     * one course in one map and therefore one plane. OctoOdyssey now stacks a menu
     * island and TWO courses along X in a single level, so the plane is a property
     * of the course you picked, not of the game. AOctoGameMode reads it off the
     * course's AOctoSpawnPoint and pushes it here, which is also what guarantees
     * the octopus and the camera cannot end up framing different planes — they are
     * now two consumers of one number rather than two copies of it.
     */
    void SetPlayPlaneX(float InPlaneX);

    /**
     * Jump straight to the follow position, skipping the interp.
     *
     * Call this the moment a run starts. The view blend out of the menu camera
     * (APlayerController::SetViewTargetWithBlend) reads THIS actor's transform
     * every frame of the blend, so without a snap the blend would target a camera
     * that is itself still interping in from wherever it spawned — two smooth
     * moves fighting each other, arriving late.
     */
    void SnapToTarget();

    /**
     * Swap in new camera values. Every field the camera reads is safe to change
     * mid-round (nothing here is baked into a component at construction), so the
     * dev tuning menu calls this on every keypress for a live preview.
     */
    void ApplyLiveTuning(const FOctoTuning& NewTuning);

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    /**
     * Camera values, pulled from UOctoTuningSubsystem in BeginPlay. PlayPlaneX
     * lives in here as the FALLBACK plane only — the plane actually framed is
     * ActivePlaneX, which AOctoGameMode sets per course (see SetPlayPlaneX).
     */
    UPROPERTY(EditDefaultsOnly, Category = "Octo|Camera")
    FOctoTuning Tuning;

private:
    /**
     * Where the camera wants to be this frame, given the target's position.
     * Shared by Tick (which interps toward it) and SnapToTarget (which teleports
     * to it) so the framing rule exists once.
     */
    FVector ComputeGoal() const;

    /** The plane this run is on. Seeded from Tuning.PlayPlaneX, overridden per course. */
    float ActivePlaneX = 0.f;

    UPROPERTY(VisibleAnywhere, Category = "Octo|Camera")
    TObjectPtr<UCameraComponent> CameraComponent;

    TWeakObjectPtr<AOctoPawn> FollowTarget;
};
