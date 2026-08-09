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
     * Swap in new camera values. Every field the camera reads is safe to change
     * mid-round (nothing here is baked into a component at construction), so the
     * dev tuning menu calls this on every keypress for a live preview.
     */
    void ApplyLiveTuning(const FOctoTuning& NewTuning);

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    /**
     * Camera values, pulled from UOctoTuningSubsystem in BeginPlay. Note
     * PlayPlaneX lives here too: the camera and AOctoGameMode used to hold
     * independent copies, so editing one silently framed the wrong plane.
     */
    UPROPERTY(EditDefaultsOnly, Category = "Octo|Camera")
    FOctoTuning Tuning;

private:
    UPROPERTY(VisibleAnywhere, Category = "Octo|Camera")
    TObjectPtr<UCameraComponent> CameraComponent;

    TWeakObjectPtr<AOctoPawn> FollowTarget;
};
