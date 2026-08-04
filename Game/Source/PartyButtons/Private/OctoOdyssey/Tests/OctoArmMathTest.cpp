#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "OctoOdyssey/OctoArmMath.h"

// --------------------------------------------------------------------------
// PartyButtons.Octo.ArmMath.*
//
// Tests for the pure, world-free OctoArm:: gameplay math used by AOctoPawn
// (the OctoOdyssey minigame). No world, no actor, no engine subsystem —
// mirrors PartyDuelMathTest.cpp's coverage style.
// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOctoArmDirectionsAreUnitAndPlanar,
    "PartyButtons.Octo.ArmMath.ArmDirectionsAreUnitAndPlanar",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOctoArmDirectionsAreUnitAndPlanar::RunTest(const FString& Parameters)
{
    const TArray<float> Offsets = { 0.f, 22.5f, 45.f, -30.f, 90.f };
    const TArray<float> Rolls   = { 0.f, 15.f, -45.f, 90.f, 180.f };

    for (const float Offset : Offsets)
    {
        for (int32 i = 0; i < OctoArm::NumArms; i++)
        {
            const FVector Local = OctoArm::ArmDirectionLocal(i, OctoArm::NumArms, Offset);
            TestTrue(FString::Printf(TEXT("Local[%d] X is exactly 0 (Offset %.1f)"), i, Offset),
                Local.X == 0.f);
            TestTrue(FString::Printf(TEXT("Local[%d] is unit length (Offset %.1f)"), i, Offset),
                FMath::IsNearlyEqual(Local.Size(), 1.f, 0.001f));

            for (const float Roll : Rolls)
            {
                const FVector World = OctoArm::ArmDirectionWorld(i, OctoArm::NumArms, Offset, Roll);
                TestTrue(FString::Printf(TEXT("World[%d] X is exactly 0 (Offset %.1f, Roll %.1f)"), i, Offset, Roll),
                    World.X == 0.f);
                TestTrue(FString::Printf(TEXT("World[%d] is unit length (Offset %.1f, Roll %.1f)"), i, Offset, Roll),
                    FMath::IsNearlyEqual(World.Size(), 1.f, 0.001f));
            }
        }
    }

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOctoArmDirectionsAreEvenlySpaced,
    "PartyButtons.Octo.ArmMath.ArmDirectionsAreEvenlySpaced",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOctoArmDirectionsAreEvenlySpaced::RunTest(const FString& Parameters)
{
    constexpr float Offset = 22.5f;

    // Consecutive arms are 45 degrees apart -> dot product of unit vectors == cos(45).
    const float Cos45 = FMath::Cos(FMath::DegreesToRadians(45.f));
    for (int32 i = 0; i < OctoArm::NumArms; i++)
    {
        const FVector A = OctoArm::ArmDirectionLocal(i,     OctoArm::NumArms, Offset);
        const FVector B = OctoArm::ArmDirectionLocal(i + 1, OctoArm::NumArms, Offset);
        TestTrue(FString::Printf(TEXT("Arms %d and %d are 45 degrees apart"), i, i + 1),
            FMath::IsNearlyEqual(FVector::DotProduct(A, B), Cos45, 0.001f));
    }

    // Opposite arms (i and i+4) are antipodal.
    for (int32 i = 0; i < OctoArm::NumArms; i++)
    {
        const FVector A = OctoArm::ArmDirectionLocal(i,     OctoArm::NumArms, Offset);
        const FVector B = OctoArm::ArmDirectionLocal(i + 4, OctoArm::NumArms, Offset);
        TestTrue(FString::Printf(TEXT("Arms %d and %d are antipodal"), i, i + 4),
            FMath::IsNearlyEqual(FVector::DotProduct(A, B), -1.f, 0.001f));
    }

    // Index wraps in both directions.
    TestTrue(TEXT("Arm 8 == arm 0"),
        OctoArm::ArmDirectionLocal(8, OctoArm::NumArms, Offset).Equals(
        OctoArm::ArmDirectionLocal(0, OctoArm::NumArms, Offset), 0.001f));
    TestTrue(TEXT("Arm -1 == arm 7"),
        OctoArm::ArmDirectionLocal(-1, OctoArm::NumArms, Offset).Equals(
        OctoArm::ArmDirectionLocal(7, OctoArm::NumArms, Offset), 0.001f));
    TestTrue(TEXT("Arm -8 == arm 0"),
        OctoArm::ArmDirectionLocal(-8, OctoArm::NumArms, Offset).Equals(
        OctoArm::ArmDirectionLocal(0, OctoArm::NumArms, Offset), 0.001f));

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOctoArmDirectionWorldMatchesRollRotation,
    "PartyButtons.Octo.ArmMath.ArmDirectionWorldMatchesRollRotation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOctoArmDirectionWorldMatchesRollRotation::RunTest(const FString& Parameters)
{
    // Highest-value test in this suite: pins the engine's FRotator Roll
    // convention that the whole arm-geometry design rests on (see
    // OctoArmMath.h's ArmDirectionWorld comment). If UE ever changes how
    // FRotator(0,0,Roll) rotates +Z, this fails loudly here instead of the
    // octopus silently steering sideways in-game.
    constexpr float Offset = 22.5f;

    for (int32 i = 0; i < OctoArm::NumArms; i++)
    {
        const FVector Local = OctoArm::ArmDirectionLocal(i, OctoArm::NumArms, Offset);

        for (float Roll = -180.f; Roll <= 180.f; Roll += 15.f)
        {
            const FVector Expected = FRotator(0.f, 0.f, Roll).RotateVector(Local);
            const FVector Actual   = OctoArm::ArmDirectionWorld(i, OctoArm::NumArms, Offset, Roll);

            TestTrue(FString::Printf(TEXT("Arm %d at Roll %.1f matches FRotator::RotateVector"), i, Roll),
                Actual.Equals(Expected, 0.001f));
        }
    }

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOctoArmDirectionWorldRollAddsToOffset,
    "PartyButtons.Octo.ArmMath.ArmDirectionWorldRollAddsToOffset",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOctoArmDirectionWorldRollAddsToOffset::RunTest(const FString& Parameters)
{
    // Encodes "arms rotate with the body": rolling the body by R is
    // indistinguishable from having built the octopus with R added to its
    // arm angle offset in the first place.
    for (int32 i = 0; i < OctoArm::NumArms; i++)
    {
        for (float Offset : { 0.f, 22.5f, -10.f })
        {
            for (float Roll : { 0.f, 30.f, -60.f, 200.f })
            {
                const FVector A = OctoArm::ArmDirectionWorld(i, OctoArm::NumArms, Offset, Roll);
                const FVector B = OctoArm::ArmDirectionWorld(i, OctoArm::NumArms, Offset + Roll, 0.f);
                TestTrue(FString::Printf(TEXT("Arm %d: Offset %.1f + Roll %.1f == Offset %.1f + Roll 0"), i, Offset, Roll, Offset + Roll),
                    A.Equals(B, 0.001f));
            }
        }
    }

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOctoCapsuleCenterOffsetRestPutsTipAtRadiusPlusProtrusion,
    "PartyButtons.Octo.ArmMath.CapsuleCenterOffsetRestPutsTipAtRadiusPlusProtrusion",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOctoCapsuleCenterOffsetRestPutsTipAtRadiusPlusProtrusion::RunTest(const FString& Parameters)
{
    constexpr float SphereRadius  = 50.f;
    constexpr float CapsuleRadius = 12.f;
    constexpr float HalfHeight    = 30.f;

    const float RestOffset = OctoArm::CapsuleCenterOffset(SphereRadius, CapsuleRadius, HalfHeight, 0.f);
    TestEqual(TEXT("Rest center offset == R + r - HH"), RestOffset, SphereRadius + CapsuleRadius - HalfHeight);

    const float RestTip = RestOffset + HalfHeight;
    TestEqual(TEXT("Rest tip distance == SphereRadius + CapsuleRadius (protrusion == capsule radius)"),
        RestTip, SphereRadius + CapsuleRadius);

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOctoCapsuleCenterOffsetIsLinearInExtension,
    "PartyButtons.Octo.ArmMath.CapsuleCenterOffsetIsLinearInExtension",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOctoCapsuleCenterOffsetIsLinearInExtension::RunTest(const FString& Parameters)
{
    constexpr float SphereRadius  = 50.f;
    constexpr float CapsuleRadius = 12.f;
    constexpr float HalfHeight    = 30.f;

    const float Base = OctoArm::CapsuleCenterOffset(SphereRadius, CapsuleRadius, HalfHeight, 0.f);
    for (float E : { 1.f, 10.f, 25.f, 45.f })
    {
        const float Offset = OctoArm::CapsuleCenterOffset(SphereRadius, CapsuleRadius, HalfHeight, E);
        TestTrue(FString::Printf(TEXT("Offset(%.1f) - Offset(0) == %.1f"), E, E),
            FMath::IsNearlyEqual(Offset - Base, E, 0.001f));
    }

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOctoExtensionInvariantsHold,
    "PartyButtons.Octo.ArmMath.ExtensionInvariantsHold",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOctoExtensionInvariantsHold::RunTest(const FString& Parameters)
{
    // Shipping defaults (see AOctoPawn tunables).
    TestEqual(TEXT("MaxSafeExtension(50,12,30) == 48"), OctoArm::MaxSafeExtension(50.f, 12.f, 30.f), 48.f);

    // Over a grid of plausible (R, r, HH), the inner cap never pokes outside the sphere at MaxSafeExtension.
    for (float R : { 30.f, 50.f, 80.f })
    {
        for (float r : { 8.f, 12.f, 20.f })
        {
            for (float HH : { 20.f, 30.f, 40.f })
            {
                const float MaxSafe   = OctoArm::MaxSafeExtension(R, r, HH);
                const float Offset    = OctoArm::CapsuleCenterOffset(R, r, HH, MaxSafe);
                const float InnerCap  = Offset - HH;
                TestTrue(FString::Printf(TEXT("Inner cap stays inside sphere (R=%.0f r=%.0f HH=%.0f)"), R, r, HH),
                    InnerCap <= R + KINDA_SMALL_NUMBER);
            }
        }
    }

    // Shipping default ArmMaxExtension (45) respects both stated invariants.
    constexpr float SphereRadius  = 50.f;
    constexpr float ArmMaxExtension = 45.f;
    TestTrue(TEXT("Default ArmMaxExtension <= MaxSafeExtension"),
        ArmMaxExtension <= OctoArm::MaxSafeExtension(SphereRadius, 12.f, 30.f));
    TestTrue(TEXT("Default ArmMaxExtension < sphere diameter"),
        ArmMaxExtension < 2.f * SphereRadius);

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOctoStepExtensionRampsAndClamps,
    "PartyButtons.Octo.ArmMath.StepExtensionRampsAndClamps",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOctoStepExtensionRampsAndClamps::RunTest(const FString& Parameters)
{
    constexpr float ExtendSpeed  = 450.f;
    constexpr float RetractSpeed = 300.f;
    constexpr float MaxExtension = 45.f;

    // Extending from 0 at 450 cm/s for 0.1s reaches exactly 45 (saturating, not overshooting).
    const float Extended = OctoArm::StepExtension(0.f, true, ExtendSpeed, RetractSpeed, MaxExtension, 0.1f);
    TestTrue(TEXT("0.1s of extension reaches MaxExtension"), FMath::IsNearlyEqual(Extended, MaxExtension, 0.01f));

    // A much longer step still saturates at exactly MaxExtension, never overshoots.
    const float LongExtend = OctoArm::StepExtension(0.f, true, ExtendSpeed, RetractSpeed, MaxExtension, 10.f);
    TestEqual(TEXT("Long extension step clamps to MaxExtension"), LongExtend, MaxExtension);

    // Releasing from MaxExtension at 300 cm/s for 0.15s reaches exactly 0.
    const float Retracted = OctoArm::StepExtension(MaxExtension, false, ExtendSpeed, RetractSpeed, MaxExtension, 0.15f);
    TestTrue(TEXT("0.15s of retraction reaches 0"), FMath::IsNearlyEqual(Retracted, 0.f, 0.01f));

    // Never goes negative.
    const float LongRetract = OctoArm::StepExtension(MaxExtension, false, ExtendSpeed, RetractSpeed, MaxExtension, 10.f);
    TestEqual(TEXT("Long retraction step clamps to 0"), LongRetract, 0.f);

    // Dt == 0 is a no-op.
    TestEqual(TEXT("Dt == 0 leaves extension unchanged (pressed)"),
        OctoArm::StepExtension(20.f, true, ExtendSpeed, RetractSpeed, MaxExtension, 0.f), 20.f);
    TestEqual(TEXT("Dt == 0 leaves extension unchanged (released)"),
        OctoArm::StepExtension(20.f, false, ExtendSpeed, RetractSpeed, MaxExtension, 0.f), 20.f);

    // Monotonic in both directions.
    float Prev = 0.f;
    bool bMonotonicUp = true;
    for (float T = 0.f; T <= 0.1f; T += 0.01f)
    {
        const float E = OctoArm::StepExtension(0.f, true, ExtendSpeed, RetractSpeed, MaxExtension, T);
        if (E < Prev - KINDA_SMALL_NUMBER) { bMonotonicUp = false; }
        Prev = E;
    }
    TestTrue(TEXT("StepExtension is monotonically non-decreasing while extending"), bMonotonicUp);

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOctoBlockedFractionBounds,
    "PartyButtons.Octo.ArmMath.BlockedFractionBounds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOctoBlockedFractionBounds::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Fully blocked (achieved 0 of desired) -> 1"), OctoArm::BlockedFraction(10.f, 0.f), 1.f);
    TestEqual(TEXT("Fully unblocked (achieved == desired) -> 0"), OctoArm::BlockedFraction(10.f, 10.f), 0.f);
    TestEqual(TEXT("Half blocked -> 0.5"), OctoArm::BlockedFraction(10.f, 5.f), 0.5f);

    // Zero or negative desired delta never divides by zero — mirrors
    // PartyDuel::ChargeToSpeed's degenerate-range handling.
    TestEqual(TEXT("Zero desired delta -> 0"), OctoArm::BlockedFraction(0.f, 0.f), 0.f);
    TestEqual(TEXT("Negative desired delta -> 0"), OctoArm::BlockedFraction(-5.f, 0.f), 0.f);

    // Achieved overshooting desired (shouldn't happen in practice) clamps to 0, not negative.
    TestEqual(TEXT("Achieved > desired clamps to 0"), OctoArm::BlockedFraction(10.f, 15.f), 0.f);

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOctoLaunchImpulseIsZeroWhenUnblockedAndClamped,
    "PartyButtons.Octo.ArmMath.LaunchImpulseIsZeroWhenUnblockedAndClamped",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOctoLaunchImpulseIsZeroWhenUnblockedAndClamped::RunTest(const FString& Parameters)
{
    constexpr float ArmSpeed        = 450.f;
    constexpr float ImpulsePerSpeed = 20.f;
    constexpr float MaxImpulse      = 9000.f;

    TestEqual(TEXT("BlockedFrac 0 -> impulse 0"),
        OctoArm::LaunchImpulseMagnitude(0.f, ArmSpeed, ImpulsePerSpeed, MaxImpulse), 0.f);

    // Monotonically non-decreasing in BlockedFrac.
    float Prev = 0.f;
    bool bMonotonic = true;
    for (float Frac = 0.f; Frac <= 1.f; Frac += 0.1f)
    {
        const float J = OctoArm::LaunchImpulseMagnitude(Frac, ArmSpeed, ImpulsePerSpeed, MaxImpulse);
        if (J < Prev - KINDA_SMALL_NUMBER) { bMonotonic = false; }
        Prev = J;
    }
    TestTrue(TEXT("LaunchImpulseMagnitude is monotonically non-decreasing in BlockedFrac"), bMonotonic);

    // Never exceeds MaxImpulse.
    const float Extreme = OctoArm::LaunchImpulseMagnitude(1.f, 10000.f, 1000.f, MaxImpulse);
    TestEqual(TEXT("Impulse clamps to MaxImpulse"), Extreme, MaxImpulse);

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOctoClampLaunchVelocityPreservesDirection,
    "PartyButtons.Octo.ArmMath.ClampLaunchVelocityPreservesDirection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOctoClampLaunchVelocityPreservesDirection::RunTest(const FString& Parameters)
{
    constexpr float MaxSpeed = 1400.f;

    // Under the cap: unchanged.
    const FVector Under(0.f, 100.f, 50.f);
    TestTrue(TEXT("Under the cap is unchanged"), OctoArm::ClampLaunchVelocity(Under, MaxSpeed).Equals(Under, 0.01f));

    // Over the cap: magnitude clamps to MaxSpeed, direction unchanged.
    const FVector Over(0.f, 2000.f, 1000.f);
    const FVector Clamped = OctoArm::ClampLaunchVelocity(Over, MaxSpeed);
    TestTrue(TEXT("Over the cap clamps to MaxSpeed"), FMath::IsNearlyEqual(Clamped.Size(), MaxSpeed, 0.01f));
    TestTrue(TEXT("Over the cap preserves direction"), Clamped.GetSafeNormal().Equals(Over.GetSafeNormal(), 0.001f));

    // Zero vector -> zero, no NaN.
    const FVector Zero = OctoArm::ClampLaunchVelocity(FVector::ZeroVector, MaxSpeed);
    TestTrue(TEXT("Zero vector clamps to zero, no NaN"), Zero.Equals(FVector::ZeroVector, 0.001f) && !Zero.ContainsNaN());

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
