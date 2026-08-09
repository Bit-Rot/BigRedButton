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
     * Distance from the body origin to the arm's HAND sphere center at the given
     * extension — the center of the capsule's OUTER hemisphere cap, i.e.
     * CapsuleCenterOffset(...) + (CapsuleHalfHeight - CapsuleRadius). A sphere of
     * radius CapsuleRadius drawn here coincides exactly with that cap, so the hand's
     * visible surface IS the collider's tip surface.
     *
     * Algebraically identical to SphereRadius + Extension — the hand center rides the
     * body's own surface at rest and tracks extension 1:1. Pinned by
     * PartyButtons.Octo.ArmMath.HandCenterOffsetIsSphereRadiusPlusExtension.
     */
    PARTYBUTTONS_API float HandCenterOffset(float SphereRadius, float CapsuleRadius, float CapsuleHalfHeight, float Extension);

    /**
     * Length of the arm cylinder MESH: 2*CapsuleHalfHeight - CapsuleRadius. Runs from
     * the capsule's inner tip to the HAND CENTER rather than to the capsule's extreme
     * tip, so the shaft's flat end stays buried inside the hand sphere instead of
     * poking out of its front (the shaft is drawn thinner than the capsule — see
     * AOctoPawn::ArmMeshRadiusScale — so a full-length shaft would emerge as a disc).
     *
     * Numerically equal to MaxSafeExtension for the same radius/half-height. That is a
     * coincidence of the algebra, not a shared concept — do not merge them.
     */
    PARTYBUTTONS_API float ArmMeshLength(float CapsuleRadius, float CapsuleHalfHeight);

    /**
     * Distance from the body origin to the arm cylinder MESH's center. Sits
     * CapsuleRadius/2 inward of the capsule's center, because the mesh is shortened at
     * the tip end only (see ArmMeshLength) while its inner end still matches the
     * capsule's.
     */
    PARTYBUTTONS_API float ArmMeshCenterOffset(float SphereRadius, float CapsuleRadius, float CapsuleHalfHeight, float Extension);

    /**
     * Hand color for arm ArmIndex: ArmCount hues spaced equidistantly around the wheel
     * at full saturation and value, so every arm is individually identifiable at a
     * glance. ArmIndex wraps (both directions) like ArmDirectionLocal.
     */
    PARTYBUTTONS_API FLinearColor HandColor(int32 ArmIndex, int32 ArmCount);

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
     * Extra speed (cm/s) a planted arm must still add along PushDirection to
     * satisfy the absolute-arm rule: an arm extending at ArmSpeed against
     * immovable geometry moves the body away from that contact at ArmSpeed.
     *
     * Returns max(0, ArmSpeed - BodyVelocity·PushDirection) — that clamp to
     * zero is the whole design. A push may only ever ADD speed, never subtract
     * it, so pushes from several arms compose instead of fighting, and a player
     * can never kill their own momentum by planting an arm into a surface they
     * are already flying away from. Motion perpendicular to PushDirection is
     * untouched; motion INTO the contact yields a deficit above ArmSpeed, which
     * is correct — that momentum has to be reversed as well as replaced.
     *
     * PushDirection is assumed unit-length. ArmSpeed <= 0 always yields 0.
     */
    PARTYBUTTONS_API float PushOffSpeedDeficit(const FVector& BodyVelocity, const FVector& PushDirection, float ArmSpeed);

    /** Clamp Velocity's magnitude to MaxSpeed, preserving direction. Zero-safe (no NaN on a zero vector). */
    PARTYBUTTONS_API FVector ClampLaunchVelocity(const FVector& Velocity, float MaxSpeed);
}
