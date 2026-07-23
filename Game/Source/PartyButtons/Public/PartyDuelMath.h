#pragma once

#include "CoreMinimal.h"

/**
 * PartyDuel — pure, world-free gameplay math for the Reflex Rumble minigame
 * (APartyArenaGameMode / APartyDuelPawn / APartyBullet).
 *
 * Kept as free functions with no engine-object dependency so they're
 * unit-testable without a world, an actor, or any UObject overhead — mirrors
 * PartyFlow::GetRoute() and APartyInputController::IsHoldGesture().
 */
namespace PartyDuel
{
    /** What NotifyReleased() should do, based on how long the button was held. */
    enum class EReleaseAction : uint8
    {
        Reflect, // very quick tap: opens the reflect window
        Cancel,  // held past the tap window but under MinChargeSeconds: no shot
        Shoot,   // held at least MinChargeSeconds: fire a bullet
    };

    /**
     * Classify a button release by held duration:
     *   HeldSeconds <= TapWindowSeconds                      -> Reflect
     *   TapWindowSeconds  <  HeldSeconds  <  MinChargeSeconds -> Cancel
     *   HeldSeconds >= MinChargeSeconds                      -> Shoot
     */
    PARTYBUTTONS_API EReleaseAction ClassifyRelease(float HeldSeconds, float TapWindowSeconds, float MinChargeSeconds);

    /**
     * Map a charge duration to bullet speed via a clamped linear interpolation.
     * HeldSeconds is clamped to [MinChargeSeconds, MaxChargeSeconds] before mapping,
     * so a release right at MinChargeSeconds gives SpeedAtMinCharge and a release at
     * or beyond MaxChargeSeconds gives SpeedAtMaxCharge.
     */
    PARTYBUTTONS_API float ChargeToSpeed(
        float HeldSeconds,
        float MinChargeSeconds,
        float MaxChargeSeconds,
        float SpeedAtMinCharge,
        float SpeedAtMaxCharge);

    /**
     * Reflect vector V off a surface with (assumed unit-length) normal N:
     *   V' = V - 2 * (V . N) * N
     * Callers are responsible for normalizing N; V need not be normalized
     * (its magnitude — the speed — is preserved by this formula).
     */
    PARTYBUTTONS_API FVector Reflect(const FVector& V, const FVector& N);
}
