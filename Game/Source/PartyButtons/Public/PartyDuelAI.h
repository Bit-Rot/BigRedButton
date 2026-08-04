#pragma once

#include "CoreMinimal.h"
#include "Math/Box2D.h"
#include "Math/RandomStream.h"

/**
 * PartyDuelAI — pure, world-free AI decision math for the Reflex Rumble minigame
 * (APartyArenaGameMode's TickAI). Mirrors PartyDuelMath: free functions with no
 * engine-object dependency so they're unit-testable without a world, an actor,
 * or any UObject overhead.
 *
 * This namespace only answers questions ("what bearing hits this ricochet?",
 * "when does this bullet reach me?"); it never touches a pawn or issues input.
 * The GameMode's TickAI is the only thing allowed to call
 * OnPlayerButton()/OnPlayerButtonReleased() — see APartyArenaGameMode's class
 * comment for why that boundary matters (AI can't do anything a human couldn't).
 */
namespace PartyDuelAI
{
    /**
     * All AI behavior tunables in one place. Bundled (rather than loose
     * UPROPERTYs like the old random-timer AI) so a whole behavior profile can
     * be swapped by difficulty tier — see EasyParams()/MediumParams()/HardParams().
     */
    struct PARTYBUTTONS_API FDuelAIParams
    {
        /** Std-dev (degrees) of Gaussian-ish jitter applied to the intended aim bearing — the tunable miss rate. */
        float AimErrorStdDevDeg = 7.f;

        /** How close (degrees) the spinning facing must be to the committed, jittered bearing before pressing. */
        float AimToleranceDeg = 3.f;

        /** Chance, each time the AI is free to plan a new shot, that it actually commits to firing. */
        float FireProbability = 0.7f;

        /** Chance a committed shot is planned as a wall-bounce ricochet instead of a direct shot. */
        float RicochetProbability = 0.2f;

        /** Number of wall bounces to plan for when a shot is a ricochet (1 or 2). */
        int32 MaxRicochetBounces = 1;

        /** Preferred charge fraction (0 = MinChargeSeconds/slowest, 1 = MaxChargeSeconds/fastest), jittered a little at use. */
        float PreferredChargeFrac = 0.6f;

        /** Base "thinking" delay between one AI decision and the next — keeps the AI from re-deciding every frame. */
        float ReactionLatencySeconds = 0.2f;

        /** Chance, when a bullet is on course to hit within BlockThreatHorizonSeconds, that the AI actually blocks it. */
        float BlockWhenThreatenedProb = 0.5f;

        /** How far ahead (seconds) an incoming bullet must be detected to be worth reacting to. */
        float BlockThreatHorizonSeconds = 0.35f;
    };

    /** Wide aim error, long reaction latency, low fire cadence, rare ricochets, low block chance. */
    PARTYBUTTONS_API FDuelAIParams EasyParams();

    /** Believable-and-beatable default: visible misses, human-ish reaction, moderate everything. */
    PARTYBUTTONS_API FDuelAIParams MediumParams();

    /** Tight aim, quick reactions, eager cadence, frequent ricochets, high block chance. */
    PARTYBUTTONS_API FDuelAIParams HardParams();

    /** Bearing in degrees (UE yaw convention: 0 = +X, 90 = +Y) from From to To. */
    PARTYBUTTONS_API float BearingDegXY(const FVector2D& From, const FVector2D& To);

    /** Shortest signed angle (degrees, in (-180, 180]) that rotates FromDeg onto ToDeg. */
    PARTYBUTTONS_API float SignedAngleDeltaDeg(float FromDeg, float ToDeg);

    /**
     * Seconds until a facing spinning at SpinRateDegPerSec (signed; sign gives
     * spin direction) next lands on BearingDeg. Wraps at a full revolution.
     * Returns a negative sentinel if SpinRateDegPerSec is ~0 (not spinning, so
     * it will never align on its own).
     */
    PARTYBUTTONS_API float TimeToAlignSeconds(float FacingDeg, float SpinRateDegPerSec, float BearingDeg);

    /** Which arena wall a ricochet bounces off. */
    enum class EBounceWall : uint8
    {
        North, // +Y
        South, // -Y
        East,  // +X
        West,  // -X
    };

    /**
     * Classic billiard "mirror" aiming: reflects Target across Wall (a line of
     * BounceBox) and aims straight at the mirror image, so the real bullet
     * bounces off Wall and continues on to Target. Returns false if Shooter
     * sits on Wall's line, or the straight-line crossing with Wall doesn't
     * land within BounceBox's extent along that wall (i.e. the wall doesn't
     * reach far enough to catch this particular bounce).
     */
    PARTYBUTTONS_API bool PlanSingleBounceBearing(
        const FVector2D& Shooter, const FVector2D& Target, const FBox2D& BounceBox,
        EBounceWall Wall, float& OutBearingDeg);

    /**
     * Plans a ricochet shot from Shooter to Target using 1 or 2 wall bounces
     * (Bounces clamped to that range), trying wall combinations in an order
     * randomized by Rng and returning the first geometrically valid one.
     * Returns false only in the degenerate case where no combination is valid
     * (shouldn't happen for two points well inside a convex, fully-walled
     * arena, but callers should still fall back to a direct shot on failure).
     */
    PARTYBUTTONS_API bool PlanRicochetBearing(
        const FVector2D& Shooter, const FVector2D& Target, const FBox2D& BounceBox,
        int32 Bounces, FRandomStream& Rng, float& OutBearingDeg);

    /**
     * Time (seconds, >= 0) until a bullet at BulletPos moving at BulletVel
     * first comes within CombinedRadius of Self, assuming it travels in a
     * straight line (no bounce prediction — matches a human's near-term
     * reflex read of an incoming shot). Returns 0 if already overlapping, or
     * a negative sentinel if it will never come that close on its current
     * heading (including if it's stationary and not already overlapping).
     */
    PARTYBUTTONS_API float TimeToImpactSeconds(
        const FVector2D& BulletPos, const FVector2D& BulletVel,
        const FVector2D& Self, float CombinedRadius);

    /**
     * Perturbs BearingDeg by a bounded, roughly-Gaussian random offset with
     * the given standard deviation (degrees) — the tunable aim-error/miss
     * knob. StdDevDeg <= 0 returns BearingDeg unchanged (perfect aim).
     */
    PARTYBUTTONS_API float ApplyAimJitter(float BearingDeg, float StdDevDeg, FRandomStream& Rng);

    /**
     * Maps a desired charge fraction (0..1, clamped) to a hold duration in
     * [MinChargeSeconds, MaxChargeSeconds] — the AI-side counterpart to
     * PartyDuel::ChargeToSpeed, letting the AI pick "how hard to charge" in
     * the same units it presses/releases in.
     */
    PARTYBUTTONS_API float HoldSecondsForChargeFrac(float Frac, float MinChargeSeconds, float MaxChargeSeconds);
}
