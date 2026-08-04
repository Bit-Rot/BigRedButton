#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "PartyDuelAI.h"
#include "PartyDuelMath.h"

// --------------------------------------------------------------------------
// PartyButtons.DuelAI.*
//
// Tests for the pure, world-free PartyDuelAI:: decision math used by
// APartyArenaGameMode::TickAI (the Reflex Rumble AI opponent). No world, no
// actor, no engine subsystem — mirrors PartyDuelMathTest.cpp's coverage style.
// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPartyDuelAIBearingAndDelta,
    "PartyButtons.DuelAI.BearingAndSignedDelta",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPartyDuelAIBearingAndDelta::RunTest(const FString& Parameters)
{
    // BearingDegXY matches UE's yaw convention: 0 = +X, 90 = +Y, 180/-180 = -X, -90 = -Y.
    TestTrue(TEXT("Bearing to +X is 0"),
        FMath::IsNearlyEqual(PartyDuelAI::BearingDegXY(FVector2D::ZeroVector, FVector2D(1.f, 0.f)), 0.f, 0.01f));
    TestTrue(TEXT("Bearing to +Y is 90"),
        FMath::IsNearlyEqual(PartyDuelAI::BearingDegXY(FVector2D::ZeroVector, FVector2D(0.f, 1.f)), 90.f, 0.01f));
    TestTrue(TEXT("Bearing to -Y is -90"),
        FMath::IsNearlyEqual(PartyDuelAI::BearingDegXY(FVector2D::ZeroVector, FVector2D(0.f, -1.f)), -90.f, 0.01f));

    // SignedAngleDeltaDeg — shortest signed rotation, including wraparound.
    TestTrue(TEXT("350 -> 10 is +20 (wraps forward)"),
        FMath::IsNearlyEqual(PartyDuelAI::SignedAngleDeltaDeg(350.f, 10.f), 20.f, 0.01f));
    TestTrue(TEXT("10 -> 350 is -20 (wraps backward)"),
        FMath::IsNearlyEqual(PartyDuelAI::SignedAngleDeltaDeg(10.f, 350.f), -20.f, 0.01f));
    TestTrue(TEXT("45 -> 90 is +45 (no wrap needed)"),
        FMath::IsNearlyEqual(PartyDuelAI::SignedAngleDeltaDeg(45.f, 90.f), 45.f, 0.01f));

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPartyDuelAITimeToAlign,
    "PartyButtons.DuelAI.TimeToAlignWraps",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPartyDuelAITimeToAlign::RunTest(const FString& Parameters)
{
    constexpr float SpinRate = 90.f; // deg/s, matches APartyDuelPawn's IdleSpinRateDegPerSec default

    // Straightforward forward case: 90 degrees to go at 90 deg/s -> 1 second.
    TestTrue(TEXT("0 -> 90 at 90deg/s takes 1s"),
        FMath::IsNearlyEqual(PartyDuelAI::TimeToAlignSeconds(0.f, SpinRate, 90.f), 1.f, 0.01f));

    // Wraparound: facing at 350, bearing at 10 -> only 20 degrees to go (not -340).
    TestTrue(TEXT("350 -> 10 wraps to 20 degrees, ~0.222s"),
        FMath::IsNearlyEqual(PartyDuelAI::TimeToAlignSeconds(350.f, SpinRate, 10.f), 20.f / SpinRate, 0.01f));

    // Already aligned -> 0 seconds.
    TestTrue(TEXT("Already aligned is 0s"),
        FMath::IsNearlyEqual(PartyDuelAI::TimeToAlignSeconds(90.f, SpinRate, 90.f), 0.f, 0.01f));

    // Negative (clockwise) spin direction is respected.
    TestTrue(TEXT("Clockwise spin: 90 -> 0 takes 1s at -90deg/s"),
        FMath::IsNearlyEqual(PartyDuelAI::TimeToAlignSeconds(90.f, -SpinRate, 0.f), 1.f, 0.01f));

    // Just-passed bearing wraps to almost a full lap rather than going negative.
    const float JustPassed = PartyDuelAI::TimeToAlignSeconds(91.f, SpinRate, 90.f);
    TestTrue(TEXT("Just-passed bearing wraps forward (near a full lap), not negative"),
        JustPassed > 3.9f && JustPassed < 4.0f);

    // Not spinning -> sentinel.
    TestTrue(TEXT("Zero spin rate returns the negative sentinel"),
        PartyDuelAI::TimeToAlignSeconds(0.f, 0.f, 90.f) < 0.f);

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPartyDuelAIRicochetIdentity,
    "PartyButtons.DuelAI.RicochetBearingIsPhysicallyValid",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPartyDuelAIRicochetIdentity::RunTest(const FString& Parameters)
{
    const FBox2D Box(FVector2D(-500.f, -500.f), FVector2D(500.f, 500.f));
    const FVector2D Shooter(-300.f, -300.f);
    const FVector2D Target(300.f, -300.f);

    float OutBearingDeg = 0.f;
    const bool bValid = PartyDuelAI::PlanSingleBounceBearing(Shooter, Target, Box, PartyDuelAI::EBounceWall::North, OutBearingDeg);
    TestTrue(TEXT("North-wall bounce between two well-inside points is geometrically valid"), bValid);

    // Hand-derived mirror of Target across the North wall (y = Box.Max.Y = 500):
    // mirrored Y = 2*500 - (-300) = 1300, X unchanged. The aim bearing should
    // point straight at that mirror image (the classic billiard aiming rule).
    const FVector2D Mirrored(300.f, 1300.f);
    const float ExpectedBearing = PartyDuelAI::BearingDegXY(Shooter, Mirrored);
    TestTrue(TEXT("Bearing aims at the wall-mirrored target"),
        FMath::IsNearlyEqual(OutBearingDeg, ExpectedBearing, 0.01f));

    // Independent physical check, via the already-tested PartyDuel::Reflect law:
    // walk from Shooter along OutBearingDeg, find where it crosses the wall's
    // y=500 line, reflect that direction off the wall normal, and confirm the
    // resulting ray actually points at the REAL (unmirrored) Target.
    const float Rad = FMath::DegreesToRadians(OutBearingDeg);
    const FVector2D Dir(FMath::Cos(Rad), FMath::Sin(Rad));
    TestTrue(TEXT("Aim direction has a positive Y component (travels toward the North wall)"), Dir.Y > 0.f);

    const float T = (500.f - Shooter.Y) / Dir.Y;
    const FVector2D CrossPoint = Shooter + Dir * T;

    const FVector PreBounce3D(Dir.X, Dir.Y, 0.f);
    const FVector WallNormal(0.f, 1.f, 0.f);
    const FVector PostBounce3D = PartyDuel::Reflect(PreBounce3D, WallNormal);
    const FVector2D PostBounceDir = FVector2D(PostBounce3D.X, PostBounce3D.Y).GetSafeNormal();

    const FVector2D ToTargetDir = (Target - CrossPoint).GetSafeNormal();
    TestTrue(TEXT("Post-bounce direction points at the real target"),
        PostBounceDir.Equals(ToTargetDir, 0.01f));

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPartyDuelAIRicochetMiss,
    "PartyButtons.DuelAI.RicochetBearingFailsWhenWallTooNarrow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPartyDuelAIRicochetMiss::RunTest(const FString& Parameters)
{
    // Same North-wall geometry as the identity test, but the box's X extent
    // (+-150) is narrower than where the bounce would actually land (X=200,
    // hand-derived below) — the wall doesn't reach far enough to catch it.
    const FBox2D NarrowBox(FVector2D(-150.f, -500.f), FVector2D(150.f, 500.f));
    const FVector2D Shooter(-100.f, -400.f);
    const FVector2D Target(400.f, -100.f);

    float OutBearingDeg = 0.f;
    const bool bValid = PartyDuelAI::PlanSingleBounceBearing(Shooter, Target, NarrowBox, PartyDuelAI::EBounceWall::North, OutBearingDeg);
    TestFalse(TEXT("Bounce landing outside the wall's extent is rejected"), bValid);

    // A shooter sitting exactly on the wall's own line is also degenerate (no meaningful crossing).
    const FBox2D Box(FVector2D(-500.f, -500.f), FVector2D(500.f, 500.f));
    const FVector2D OnTheWall(0.f, 500.f);
    float Unused = 0.f;
    TestFalse(TEXT("Shooter on the wall's own line is rejected"),
        PartyDuelAI::PlanSingleBounceBearing(OnTheWall, Target, Box, PartyDuelAI::EBounceWall::North, Unused));

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPartyDuelAITimeToImpact,
    "PartyButtons.DuelAI.TimeToImpactSeconds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPartyDuelAITimeToImpact::RunTest(const FString& Parameters)
{
    // Head-on: bullet at origin moving +X at 100 units/s toward a target at X=500, combined radius 10.
    // Impact when the bullet center reaches X=490 -> t = 490/100 = 4.9s.
    {
        const float T = PartyDuelAI::TimeToImpactSeconds(FVector2D(0.f, 0.f), FVector2D(100.f, 0.f), FVector2D(500.f, 0.f), 10.f);
        TestTrue(TEXT("Head-on approach gives the correct impact time"), FMath::IsNearlyEqual(T, 4.9f, 0.01f));
    }

    // Already overlapping -> 0.
    {
        const float T = PartyDuelAI::TimeToImpactSeconds(FVector2D(500.f, 0.f), FVector2D(0.f, 0.f), FVector2D(505.f, 0.f), 10.f);
        TestTrue(TEXT("Already overlapping returns 0"), FMath::IsNearlyEqual(T, 0.f, 0.01f));
    }

    // Moving directly away -> never hits.
    {
        const float T = PartyDuelAI::TimeToImpactSeconds(FVector2D(0.f, 0.f), FVector2D(-100.f, 0.f), FVector2D(500.f, 0.f), 10.f);
        TestTrue(TEXT("Receding bullet returns the negative sentinel"), T < 0.f);
    }

    // Stationary and not already overlapping -> never hits.
    {
        const float T = PartyDuelAI::TimeToImpactSeconds(FVector2D(0.f, 0.f), FVector2D::ZeroVector, FVector2D(500.f, 0.f), 10.f);
        TestTrue(TEXT("Stationary, non-overlapping bullet returns the negative sentinel"), T < 0.f);
    }

    // Trajectory passes far to the side -> misses entirely (closest approach exceeds the combined radius).
    {
        const float T = PartyDuelAI::TimeToImpactSeconds(FVector2D(0.f, -1000.f), FVector2D(100.f, 0.f), FVector2D(500.f, 500.f), 10.f);
        TestTrue(TEXT("Trajectory passing well clear of the target returns the negative sentinel"), T < 0.f);
    }

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPartyDuelAIAimJitter,
    "PartyButtons.DuelAI.AimJitterDeterministicAndBounded",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPartyDuelAIAimJitter::RunTest(const FString& Parameters)
{
    // Zero std-dev is a no-op (perfect aim).
    FRandomStream ZeroRng(1);
    TestEqual(TEXT("Zero std-dev returns the bearing unchanged"),
        PartyDuelAI::ApplyAimJitter(45.f, 0.f, ZeroRng), 45.f);

    // Same seed -> same jitter (deterministic, for reproducible AI behavior/tests).
    FRandomStream RngA(1234);
    FRandomStream RngB(1234);
    const float JitterA = PartyDuelAI::ApplyAimJitter(10.f, 5.f, RngA);
    const float JitterB = PartyDuelAI::ApplyAimJitter(10.f, 5.f, RngB);
    TestEqual(TEXT("Identical seeds produce identical jitter"), JitterA, JitterB);

    // Bounded (sum of 3 Uniform(-1,1) samples can't exceed magnitude 3) and roughly centered on the input.
    FRandomStream Rng(99);
    constexpr float StdDev = 5.f;
    constexpr float Bearing = 10.f;
    constexpr int32 NumSamples = 2000;
    double Sum = 0.0;
    bool bAllBounded = true;
    for (int32 i = 0; i < NumSamples; i++)
    {
        const float Sample = PartyDuelAI::ApplyAimJitter(Bearing, StdDev, Rng);
        if (Sample < Bearing - 3.f * StdDev - 0.01f || Sample > Bearing + 3.f * StdDev + 0.01f)
        {
            bAllBounded = false;
        }
        Sum += Sample;
    }
    TestTrue(TEXT("All jitter samples stay within the +-3*StdDev bound"), bAllBounded);

    const double Mean = Sum / NumSamples;
    TestTrue(TEXT("Jitter is roughly centered on the input bearing"), FMath::Abs(Mean - Bearing) < 1.0);

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPartyDuelAIChargeFrac,
    "PartyButtons.DuelAI.HoldSecondsForChargeFracRoundTrips",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPartyDuelAIChargeFrac::RunTest(const FString& Parameters)
{
    constexpr float MinCharge = 0.5f;
    constexpr float MaxCharge = 2.0f;
    constexpr float MinSpeed  = 400.f;
    constexpr float MaxSpeed  = 1600.f;

    const float Fracs[] = { 0.f, 0.25f, 0.5f, 0.75f, 1.f };
    for (float Frac : Fracs)
    {
        const float HeldSeconds = PartyDuelAI::HoldSecondsForChargeFrac(Frac, MinCharge, MaxCharge);
        TestTrue(TEXT("Held duration stays within [MinCharge, MaxCharge]"),
            HeldSeconds >= MinCharge - 0.001f && HeldSeconds <= MaxCharge + 0.001f);

        // Feeding the resulting hold duration back through PartyDuel::ChargeToSpeed
        // should reproduce the same fraction's speed — the two functions agree on
        // what a given charge fraction means.
        const float Speed = PartyDuel::ChargeToSpeed(HeldSeconds, MinCharge, MaxCharge, MinSpeed, MaxSpeed);
        const float ExpectedSpeed = FMath::Lerp(MinSpeed, MaxSpeed, Frac);
        TestTrue(TEXT("Round-tripped speed matches the requested charge fraction"),
            FMath::IsNearlyEqual(Speed, ExpectedSpeed, 0.5f));
    }

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
