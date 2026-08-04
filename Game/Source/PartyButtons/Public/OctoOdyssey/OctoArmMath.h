#pragma once

#include "CoreMinimal.h"

/**
 * OctoArm — pure, world-free gameplay math for the OctoOdyssey minigame
 * (AOctoPawn / AOctoGameMode).
 *
 * Kept as free functions with no engine-object dependency so they're
 * unit-testable without a world, an actor, or any UObject overhead — mirrors
 * PartyDuel:: (PartyDuelMath.h) and PartyFlow::GetRoute().
 *
 * The octopus lives entirely in the Y-Z plane (X locked); every direction
 * this namespace produces has X == 0, and "roll" always means rotation about
 * the actor's local X axis (FRotator::Roll).
 */
namespace OctoArm
{
    /** Fixed arm count — one per party button used by this minigame (buttons 0..7). */
    inline constexpr int32 NumArms = 8;

    /**
     * Body-local unit direction of arm ArmIndex in the Y-Z plane (X always 0).
     * Arm i sits at angle (AngleOffsetDegrees + 360*i/ArmCount), measured from
     * +Z the same way FRotator::Roll rotates — see ArmDirectionWorld's comment
     * for why this makes "arms rotate with the body" trivial.
     * ArmIndex wraps (both directions) via modulo on ArmCount.
     */
    PARTYBUTTONS_API FVector ArmDirectionLocal(int32 ArmIndex, int32 ArmCount, float AngleOffsetDegrees);

    /**
     * World-space direction of arm ArmIndex when the body has been rolled
     * BodyRollDegrees about its local X axis. Contractually equal to
     * FRotator(0, 0, BodyRollDegrees).RotateVector(ArmDirectionLocal(...)) —
     * pinned by PartyButtons.Octo.ArmMath.ArmDirectionWorldMatchesRollRotation
     * so an engine change to the Roll convention fails loudly here instead of
     * silently steering the octopus sideways.
     */
    PARTYBUTTONS_API FVector ArmDirectionWorld(int32 ArmIndex, int32 ArmCount, float AngleOffsetDegrees, float BodyRollDegrees);

    /**
     * Distance from the body origin to the arm capsule's CENTER at the given
     * extension: SphereRadius + CapsuleRadius - CapsuleHalfHeight + Extension.
     * At Extension == 0 this places the capsule's tip exactly at
     * SphereRadius + CapsuleRadius (rest protrusion == capsule radius).
     */
    PARTYBUTTONS_API float CapsuleCenterOffset(float SphereRadius, float CapsuleRadius, float CapsuleHalfHeight, float Extension);

    /**
     * Largest Extension that keeps the capsule's inner cap inside the sphere
     * (2*CapsuleHalfHeight - CapsuleRadius) — i.e. no gap ever opens between
     * arm and body. This is normally the binding constraint, tighter than
     * "extension < sphere diameter".
     */
    PARTYBUTTONS_API float MaxSafeExtension(float SphereRadius, float CapsuleRadius, float CapsuleHalfHeight);

    /**
     * Advance an arm's extension by DeltaSeconds: moves toward MaxExtension at
     * ExtendSpeed while bPressed, toward 0 at RetractSpeed otherwise. Result is
     * clamped to [0, MaxExtension]. DeltaSeconds <= 0 is a no-op.
     */
    PARTYBUTTONS_API float StepExtension(float CurrentExtension, bool bPressed, float ExtendSpeed, float RetractSpeed, float MaxExtension, float DeltaSeconds);

    /**
     * Fraction of a desired extension delta that world geometry prevented, in
     * [0, 1]: 1 - AchievedDelta / DesiredDelta, clamped. Returns 0 if
     * DesiredDelta <= 0 (nothing was being attempted, so nothing was blocked —
     * mirrors PartyDuel::ChargeToSpeed's degenerate-range handling, no
     * divide-by-zero).
     */
    PARTYBUTTONS_API float BlockedFraction(float DesiredDelta, float AchievedDelta);

    /**
     * Launch impulse magnitude (kg*cm/s) for a free->blocked arm transition:
     * BlockedFrac * ArmSpeed * ImpulsePerUnitSpeed, clamped to [0, MaxImpulse].
     * Zero when BlockedFrac is zero (a fully-unobstructed extension launches
     * nothing — see AOctoPawn::TickArms).
     */
    PARTYBUTTONS_API float LaunchImpulseMagnitude(float BlockedFrac, float ArmSpeed, float ImpulsePerUnitSpeed, float MaxImpulse);

    /** Clamp Velocity's magnitude to MaxSpeed, preserving direction. Zero-safe (no NaN on a zero vector). */
    PARTYBUTTONS_API FVector ClampLaunchVelocity(const FVector& Velocity, float MaxSpeed);
}
