#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
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

protected:
    virtual void Tick(float DeltaSeconds) override;

    /** World X the octopus is locked to (see AOctoPawn) — the camera sits CameraDistanceX behind it. */
    UPROPERTY(EditDefaultsOnly, Category = "Octo|Camera")
    float PlayPlaneX = 0.f;

    UPROPERTY(EditDefaultsOnly, Category = "Octo|Camera")
    float CameraDistanceX = 2000.f;

    UPROPERTY(EditDefaultsOnly, Category = "Octo|Camera")
    float CameraHeightOffset = 150.f;

    /** Bias the frame toward +Y (the direction of the goal) rather than centering exactly on the octopus. */
    UPROPERTY(EditDefaultsOnly, Category = "Octo|Camera")
    float LeadY = 250.f;

    /** Don't let the camera sink below this Z, e.g. if the octopus falls into a pit. */
    UPROPERTY(EditDefaultsOnly, Category = "Octo|Camera")
    float MinCameraZ = 200.f;

    UPROPERTY(EditDefaultsOnly, Category = "Octo|Camera")
    float FollowInterpSpeed = 4.f;

    UPROPERTY(EditDefaultsOnly, Category = "Octo|Camera")
    float CameraFieldOfView = 55.f;

private:
    UPROPERTY(VisibleAnywhere, Category = "Octo|Camera")
    TObjectPtr<UCameraComponent> CameraComponent;

    TWeakObjectPtr<AOctoPawn> FollowTarget;
};
