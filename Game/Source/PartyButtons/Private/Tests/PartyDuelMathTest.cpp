#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "PartyDuelMath.h"

// --------------------------------------------------------------------------
// PartyButtons.Duel.Math.*
//
// Tests for the pure, world-free PartyDuel:: gameplay math used by
// APartyDuelPawn / APartyBullet (the Reflex Rumble minigame). No world, no
// actor, no engine subsystem — mirrors PartyInputMainButtonTest.cpp's
// IsHoldGesture coverage style.
// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPartyDuelClassifyRelease,
    "PartyButtons.Duel.Math.ClassifyReleaseBoundaries",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPartyDuelClassifyRelease::RunTest(const FString& Parameters)
{
    constexpr float TapWindow  = 0.05f;
    constexpr float MinCharge  = 0.5f;

    // Reflect band: [0, TapWindow].
    TestTrue(TEXT("0.0s is Reflect"),
        PartyDuel::ClassifyRelease(0.0f, TapWindow, MinCharge) == PartyDuel::EReleaseAction::Reflect);
    TestTrue(TEXT("0.05s (== TapWindow) is Reflect"),
        PartyDuel::ClassifyRelease(0.05f, TapWindow, MinCharge) == PartyDuel::EReleaseAction::Reflect);

    // Cancel band: (TapWindow, MinCharge).
    TestTrue(TEXT("0.06s is Cancel"),
        PartyDuel::ClassifyRelease(0.06f, TapWindow, MinCharge) == PartyDuel::EReleaseAction::Cancel);
    TestTrue(TEXT("0.3s is Cancel"),
        PartyDuel::ClassifyRelease(0.3f, TapWindow, MinCharge) == PartyDuel::EReleaseAction::Cancel);
    TestTrue(TEXT("0.499s is Cancel"),
        PartyDuel::ClassifyRelease(0.499f, TapWindow, MinCharge) == PartyDuel::EReleaseAction::Cancel);

    // Shoot band: [MinCharge, +inf).
    TestTrue(TEXT("0.5s (== MinCharge) is Shoot"),
        PartyDuel::ClassifyRelease(0.5f, TapWindow, MinCharge) == PartyDuel::EReleaseAction::Shoot);
    TestTrue(TEXT("1.0s is Shoot"),
        PartyDuel::ClassifyRelease(1.0f, TapWindow, MinCharge) == PartyDuel::EReleaseAction::Shoot);
    TestTrue(TEXT("10.0s (held way past max) is still Shoot"),
        PartyDuel::ClassifyRelease(10.0f, TapWindow, MinCharge) == PartyDuel::EReleaseAction::Shoot);

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPartyDuelChargeToSpeed,
    "PartyButtons.Duel.Math.ChargeToSpeedMonotonicAndClamped",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPartyDuelChargeToSpeed::RunTest(const FString& Parameters)
{
    constexpr float MinCharge = 0.5f;
    constexpr float MaxCharge = 2.0f;
    constexpr float MinSpeed  = 400.f;
    constexpr float MaxSpeed  = 1600.f;

    // Endpoints map exactly.
    TestEqual(TEXT("MinCharge -> MinSpeed"),
        PartyDuel::ChargeToSpeed(MinCharge, MinCharge, MaxCharge, MinSpeed, MaxSpeed), MinSpeed);
    TestEqual(TEXT("MaxCharge -> MaxSpeed"),
        PartyDuel::ChargeToSpeed(MaxCharge, MinCharge, MaxCharge, MinSpeed, MaxSpeed), MaxSpeed);

    // Midpoint is halfway.
    const float Mid = PartyDuel::ChargeToSpeed(1.25f, MinCharge, MaxCharge, MinSpeed, MaxSpeed);
    TestTrue(TEXT("Midpoint charge is close to the midpoint speed"),
        FMath::IsNearlyEqual(Mid, (MinSpeed + MaxSpeed) * 0.5f, 0.01f));

    // Clamped below MinCharge and above MaxCharge.
    TestEqual(TEXT("Below MinCharge clamps to MinSpeed"),
        PartyDuel::ChargeToSpeed(0.0f, MinCharge, MaxCharge, MinSpeed, MaxSpeed), MinSpeed);
    TestEqual(TEXT("Above MaxCharge clamps to MaxSpeed"),
        PartyDuel::ChargeToSpeed(10.0f, MinCharge, MaxCharge, MinSpeed, MaxSpeed), MaxSpeed);

    // Monotonically non-decreasing across the range.
    float Prev = PartyDuel::ChargeToSpeed(MinCharge, MinCharge, MaxCharge, MinSpeed, MaxSpeed);
    bool bMonotonic = true;
    for (float T = MinCharge; T <= MaxCharge; T += 0.1f)
    {
        const float Speed = PartyDuel::ChargeToSpeed(T, MinCharge, MaxCharge, MinSpeed, MaxSpeed);
        if (Speed < Prev - KINDA_SMALL_NUMBER) { bMonotonic = false; }
        Prev = Speed;
    }
    TestTrue(TEXT("ChargeToSpeed is monotonically non-decreasing"), bMonotonic);

    // Degenerate range (Max <= Min) doesn't divide by zero — just returns the max-side speed.
    TestEqual(TEXT("Degenerate range returns SpeedAtMaxCharge"),
        PartyDuel::ChargeToSpeed(1.0f, 1.0f, 1.0f, MinSpeed, MaxSpeed), MaxSpeed);

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPartyDuelReflect,
    "PartyButtons.Duel.Math.ReflectLaw",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPartyDuelReflect::RunTest(const FString& Parameters)
{
    // Straight-on hit against a flat wall (normal along +X): X-component flips, Y unaffected.
    {
        const FVector V(-100.f, 50.f, 0.f); // traveling in -X, drifting +Y
        const FVector N(1.f, 0.f, 0.f);
        const FVector R = PartyDuel::Reflect(V, N);
        TestTrue(TEXT("Straight hit: X flips"), FMath::IsNearlyEqual(R.X, 100.f, 0.01f));
        TestTrue(TEXT("Straight hit: Y unchanged"), FMath::IsNearlyEqual(R.Y, 50.f, 0.01f));
    }

    // Speed (magnitude) is preserved by a reflection.
    {
        const FVector V(30.f, -40.f, 0.f); // magnitude 50
        const FVector N = FVector(1.f, 1.f, 0.f).GetSafeNormal();
        const FVector R = PartyDuel::Reflect(V, N);
        TestTrue(TEXT("Reflection preserves speed"), FMath::IsNearlyEqual(R.Size(), V.Size(), 0.01f));
    }

    // A vector already parallel to the surface (perpendicular to the normal) is unaffected.
    {
        const FVector V(0.f, 75.f, 0.f); // moving along the wall
        const FVector N(1.f, 0.f, 0.f);
        const FVector R = PartyDuel::Reflect(V, N);
        TestTrue(TEXT("Parallel-to-wall velocity is unchanged"), R.Equals(V, 0.01f));
    }

    // Reflecting twice off the same normal returns the original vector.
    {
        const FVector V(20.f, 35.f, 0.f);
        const FVector N(0.f, 1.f, 0.f);
        const FVector R2 = PartyDuel::Reflect(PartyDuel::Reflect(V, N), N);
        TestTrue(TEXT("Double reflection returns the original vector"), R2.Equals(V, 0.01f));
    }

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
