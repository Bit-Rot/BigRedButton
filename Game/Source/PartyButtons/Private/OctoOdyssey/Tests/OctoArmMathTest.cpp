#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "OctoOdyssey/OctoArmMath.h"
#include "OctoOdyssey/OctoTuning.h"

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
    FOctoHandSphereCoincidesWithCapsuleCap,
    "PartyButtons.Octo.ArmMath.HandSphereCoincidesWithCapsuleCap",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOctoHandSphereCoincidesWithCapsuleCap::RunTest(const FString& Parameters)
{
    // THE property that matters for the hand: drawn at radius r, its outer surface is
    // exactly the capsule's tip, so the visible ball and the collider agree everywhere.
    for (float R : { 30.f, 50.f, 80.f })
    {
        for (float r : { 8.f, 12.f, 20.f })
        {
            for (float HH : { 20.f, 30.f, 45.f })
            {
                for (float E : { 0.f, 7.f, 40.f })
                {
                    const float HandSurface  = OctoArm::HandCenterOffset(R, r, HH, E) + r;
                    const float CapsuleTip   = OctoArm::CapsuleCenterOffset(R, r, HH, E) + HH;
                    TestTrue(FString::Printf(TEXT("Hand surface == capsule tip (R=%.0f r=%.0f HH=%.0f E=%.0f)"), R, r, HH, E),
                        FMath::IsNearlyEqual(HandSurface, CapsuleTip, 0.001f));
                }
            }
        }
    }

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOctoHandCenterOffsetIsSphereRadiusPlusExtension,
    "PartyButtons.Octo.ArmMath.HandCenterOffsetIsSphereRadiusPlusExtension",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOctoHandCenterOffsetIsSphereRadiusPlusExtension::RunTest(const FString& Parameters)
{
    // The closed form documented on HandCenterOffset: independent of both capsule
    // dimensions, so at rest the hand center sits exactly ON the body's surface.
    for (float R : { 30.f, 50.f, 80.f })
    {
        for (float r : { 8.f, 12.f, 20.f })
        {
            for (float HH : { 20.f, 30.f, 45.f })
            {
                for (float E : { 0.f, 7.f, 40.f })
                {
                    TestTrue(FString::Printf(TEXT("HandCenterOffset == R + E (R=%.0f r=%.0f HH=%.0f E=%.0f)"), R, r, HH, E),
                        FMath::IsNearlyEqual(OctoArm::HandCenterOffset(R, r, HH, E), R + E, 0.001f));
                }
            }
        }
    }

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOctoArmMeshSpansInnerTipToHandCenter,
    "PartyButtons.Octo.ArmMath.ArmMeshSpansInnerTipToHandCenter",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOctoArmMeshSpansInnerTipToHandCenter::RunTest(const FString& Parameters)
{
    // The shaft is shortened at the TIP end only: its far end stops at the hand center
    // (so it can't poke out through the front of the ball) while its inner end still
    // matches the capsule's, keeping it buried in the body sphere.
    for (float R : { 30.f, 50.f, 80.f })
    {
        for (float r : { 8.f, 12.f, 20.f })
        {
            for (float HH : { 20.f, 30.f, 45.f })
            {
                for (float E : { 0.f, 7.f, 40.f })
                {
                    const float Center   = OctoArm::ArmMeshCenterOffset(R, r, HH, E);
                    const float Half     = 0.5f * OctoArm::ArmMeshLength(r, HH);
                    const FString Where  = FString::Printf(TEXT("(R=%.0f r=%.0f HH=%.0f E=%.0f)"), R, r, HH, E);

                    TestTrue(FString::Printf(TEXT("Shaft far end == hand center %s"), *Where),
                        FMath::IsNearlyEqual(Center + Half, OctoArm::HandCenterOffset(R, r, HH, E), 0.001f));
                    TestTrue(FString::Printf(TEXT("Shaft inner end == capsule inner end %s"), *Where),
                        FMath::IsNearlyEqual(Center - Half, OctoArm::CapsuleCenterOffset(R, r, HH, E) - HH, 0.001f));
                }
            }
        }
    }

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOctoHandColorsAreDistinctAndFullySaturated,
    "PartyButtons.Octo.ArmMath.HandColorsAreDistinctAndFullySaturated",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOctoHandColorsAreDistinctAndFullySaturated::RunTest(const FString& Parameters)
{
    TArray<FLinearColor> Colors;
    for (int32 i = 0; i < OctoArm::NumArms; i++)
    {
        Colors.Add(OctoArm::HandColor(i, OctoArm::NumArms));
    }

    for (int32 i = 0; i < Colors.Num(); i++)
    {
        const FLinearColor& C = Colors[i];

        // Full saturation and value: one channel pegged at 1, another at 0.
        const float MaxChannel = FMath::Max3(C.R, C.G, C.B);
        const float MinChannel = FMath::Min3(C.R, C.G, C.B);
        TestTrue(FString::Printf(TEXT("Arm %d is at full value (max channel == 1)"), i),
            FMath::IsNearlyEqual(MaxChannel, 1.f, 0.001f));
        TestTrue(FString::Printf(TEXT("Arm %d is fully saturated (min channel == 0)"), i),
            FMath::IsNearlyEqual(MinChannel, 0.f, 0.001f));
        TestTrue(FString::Printf(TEXT("Arm %d is opaque"), i), FMath::IsNearlyEqual(C.A, 1.f, 0.001f));

        // Visually distinct from every other arm.
        for (int32 j = i + 1; j < Colors.Num(); j++)
        {
            const FLinearColor& D = Colors[j];
            const float Separation = FMath::Abs(C.R - D.R) + FMath::Abs(C.G - D.G) + FMath::Abs(C.B - D.B);
            TestTrue(FString::Printf(TEXT("Arms %d and %d are visually distinct"), i, j), Separation > 0.25f);
        }
    }

    // Same wrap convention as ArmDirectionLocal, and degenerate counts don't divide by zero.
    TestEqual(TEXT("Index wraps forward"), OctoArm::HandColor(OctoArm::NumArms, OctoArm::NumArms), Colors[0]);
    TestEqual(TEXT("Index wraps backward"), OctoArm::HandColor(-1, OctoArm::NumArms), Colors[OctoArm::NumArms - 1]);
    TestEqual(TEXT("Zero arm count is safe"), OctoArm::HandColor(0, 0), FLinearColor::White);

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOctoExtensionInvariantsHold,
    "PartyButtons.Octo.ArmMath.ExtensionInvariantsHold",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOctoExtensionInvariantsHold::RunTest(const FString& Parameters)
{
    // A worked example, independent of the shipping numbers: 2*30 - 12 == 48.
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

    // The ACTUAL shipping defaults, read from FOctoTuning rather than restated as
    // literals — the literals that used to live here silently drifted out of date
    // and stopped guarding anything.
    const FOctoTuning Defaults;
    TestTrue(TEXT("Default ArmMaxExtension <= MaxSafeExtension"),
        Defaults.ArmMaxExtension <= OctoArm::MaxSafeExtension(
            Defaults.SphereRadius, Defaults.ArmRadius, Defaults.ArmHalfHeight));
    TestTrue(TEXT("Default ArmMaxExtension < sphere diameter"),
        Defaults.ArmMaxExtension < 2.f * Defaults.SphereRadius);

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOctoStepExtensionRampsAndClamps,
    "PartyButtons.Octo.ArmMath.StepExtensionRampsAndClamps",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOctoStepExtensionRampsAndClamps::RunTest(const FString& Parameters)
{
    // Arbitrary values chosen so the step arithmetic below comes out exact —
    // deliberately NOT the shipping defaults, which live in FOctoTuning.
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
    FOctoPushOffSpeedDeficitOnlyEverAdds,
    "PartyButtons.Octo.ArmMath.PushOffSpeedDeficitOnlyEverAdds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOctoPushOffSpeedDeficitOnlyEverAdds::RunTest(const FString& Parameters)
{
    constexpr float ArmSpeed = 900.f;
    const FVector   Up(0.f, 0.f, 1.f); // push direction: away from a floor contact

    // At rest, a planted arm has to supply the whole extension speed.
    TestEqual(TEXT("At rest -> full ArmSpeed"),
        OctoArm::PushOffSpeedDeficit(FVector::ZeroVector, Up, ArmSpeed), ArmSpeed);

    // Already rising, but slower than the arm extends: top up the difference.
    TestEqual(TEXT("Rising at 300 -> tops up to ArmSpeed"),
        OctoArm::PushOffSpeedDeficit(FVector(0.f, 0.f, 300.f), Up, ArmSpeed), 600.f);

    // Already outrunning the arm: NEVER brake. This is the property that lets
    // several arms compose and stops a player killing their own momentum.
    TestEqual(TEXT("Rising faster than ArmSpeed -> 0, not negative"),
        OctoArm::PushOffSpeedDeficit(FVector(0.f, 0.f, 1500.f), Up, ArmSpeed), 0.f);
    TestEqual(TEXT("Rising at exactly ArmSpeed -> 0"),
        OctoArm::PushOffSpeedDeficit(FVector(0.f, 0.f, ArmSpeed), Up, ArmSpeed), 0.f);

    // Falling INTO the contact: the deficit exceeds ArmSpeed, because that
    // downward momentum has to be reversed as well as replaced.
    TestEqual(TEXT("Falling at 400 -> ArmSpeed + 400"),
        OctoArm::PushOffSpeedDeficit(FVector(0.f, 0.f, -400.f), Up, ArmSpeed), ArmSpeed + 400.f);

    // Motion perpendicular to the push is irrelevant — sliding along a wall is
    // neither helped nor hindered by an arm planted into it.
    TestEqual(TEXT("Perpendicular motion is ignored"),
        OctoArm::PushOffSpeedDeficit(FVector(0.f, 5000.f, 0.f), Up, ArmSpeed), ArmSpeed);

    // Degenerate arm speeds never produce a push.
    TestEqual(TEXT("Zero ArmSpeed -> 0"),
        OctoArm::PushOffSpeedDeficit(FVector::ZeroVector, Up, 0.f), 0.f);
    TestEqual(TEXT("Negative ArmSpeed -> 0"),
        OctoArm::PushOffSpeedDeficit(FVector::ZeroVector, Up, -100.f), 0.f);

    // Never negative, for any velocity along the push axis.
    bool bNonNegative = true;
    for (float Vz = -2000.f; Vz <= 2000.f; Vz += 137.f)
    {
        if (OctoArm::PushOffSpeedDeficit(FVector(0.f, 0.f, Vz), Up, ArmSpeed) < 0.f)
        {
            bNonNegative = false;
        }
    }
    TestTrue(TEXT("PushOffSpeedDeficit is never negative"), bNonNegative);

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

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOctoArmChainLengthScaleLandsTheTip,
    "PartyButtons.Octo.ArmMath.ArmChainLengthScaleLandsTheTip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOctoArmChainLengthScaleLandsTheTip::RunTest(const FString& Parameters)
{
    // SK_Okto's measured rig, so this test fails for the same reason the real one
    // would. PartyButtons.Octo.Skeleton.ReachMatchesTuning pins these to the asset.
    constexpr float ChainOrigin = 31.928f;
    constexpr float RestLength  = 78.072f;

    const FOctoTuning Tuning;
    const float SafeMax = FMath::Min(
        Tuning.ArmMaxExtension,
        OctoArm::MaxSafeExtension(Tuning.SphereRadius, Tuning.ArmRadius, Tuning.ArmHalfHeight));

    // The defining property: whatever the scale is, the tip lands on the target.
    for (float E = 0.f; E <= SafeMax + 0.001f; E += SafeMax / 8.f)
    {
        const float Target = OctoArm::HandCenterOffset(Tuning.SphereRadius, Tuning.ArmRadius, Tuning.ArmHalfHeight, E);
        const float Scale  = OctoArm::ArmChainLengthScale(ChainOrigin, RestLength, Target);

        TestTrue(
            FString::Printf(TEXT("Extension %.1f puts the tip at %.3f"), E, Target),
            FMath::IsNearlyEqual(ChainOrigin + RestLength * Scale, Target, 0.01f));
    }

    // Reference pose == full extension, so no stretch at all there.
    const float FullTarget = OctoArm::HandCenterOffset(Tuning.SphereRadius, Tuning.ArmRadius, Tuning.ArmHalfHeight, SafeMax);
    TestTrue(TEXT("Full extension is scale 1"),
        FMath::IsNearlyEqual(OctoArm::ArmChainLengthScale(ChainOrigin, RestLength, FullTarget), 1.f, 0.001f));

    // Retracted is emphatically NOT a scale to zero — the chain root is already
    // inside the body, so the tip only has to reach the sphere's surface.
    const float RetractedScale = OctoArm::ArmChainLengthScale(ChainOrigin, RestLength, Tuning.SphereRadius);
    TestTrue(TEXT("Retracted scale is strictly positive"), RetractedScale > 0.f);
    TestTrue(TEXT("Retracted scale is well under 1"), RetractedScale < 0.2f);
    TestTrue(TEXT("Retracted tip lands on the body surface"),
        FMath::IsNearlyEqual(ChainOrigin + RestLength * RetractedScale, Tuning.SphereRadius, 0.01f));

    // Monotonic: a longer target is never a shorter arm.
    float Previous = -1.f;
    for (float E = 0.f; E <= SafeMax; E += 5.f)
    {
        const float Scale = OctoArm::ArmChainLengthScale(ChainOrigin, RestLength,
            OctoArm::HandCenterOffset(Tuning.SphereRadius, Tuning.ArmRadius, Tuning.ArmHalfHeight, E));
        TestTrue(FString::Printf(TEXT("Scale grows with extension at %.1f"), E), Scale > Previous);
        Previous = Scale;
    }

    // ---- Degenerate inputs ------------------------------------------------

    TestEqual(TEXT("A zero-length chain scales by 1 rather than dividing by zero"),
        OctoArm::ArmChainLengthScale(ChainOrigin, 0.f, 100.f), 1.f);
    TestEqual(TEXT("A negative chain length scales by 1"),
        OctoArm::ArmChainLengthScale(ChainOrigin, -5.f, 100.f), 1.f);

    // A target behind the chain root would mirror the arm back through the body.
    TestEqual(TEXT("A target inside the chain root clamps to 0"),
        OctoArm::ArmChainLengthScale(ChainOrigin, RestLength, ChainOrigin - 10.f), 0.f);

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOctoArmLengthAxisScaleOnlyTouchesItsAxis,
    "PartyButtons.Octo.ArmMath.LengthAxisScaleOnlyTouchesItsAxis",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOctoArmLengthAxisScaleOnlyTouchesItsAxis::RunTest(const FString& Parameters)
{
    // SK_Okto's arms come in on local -Y; the sign must not matter to a scale, or
    // half the octopus would invert.
    const FVector NegY = OctoArm::LengthAxisScale(FVector(0.f, -1.f, 0.f), 0.25f);
    const FVector PosY = OctoArm::LengthAxisScale(FVector(0.f,  1.f, 0.f), 0.25f);

    TestTrue(TEXT("-Y scales only Y"), NegY.Equals(FVector(1.f, 0.25f, 1.f), 0.001f));
    TestTrue(TEXT("Axis sign does not matter"), NegY.Equals(PosY, 0.001f));

    // Thickness is preserved — an extending arm gets longer, not fatter.
    for (const FVector& Axis : { FVector::XAxisVector, FVector::YAxisVector, FVector::ZAxisVector })
    {
        for (const float Scale : { 0.f, 0.1f, 1.f, 3.f })
        {
            const FVector Result = OctoArm::LengthAxisScale(Axis, Scale);
            for (int32 Component = 0; Component < 3; Component++)
            {
                const bool bIsLengthAxis = !FMath::IsNearlyZero(Axis[Component]);
                TestTrue(
                    FString::Printf(TEXT("Axis %s scale %.2f leaves component %d %s"),
                        *Axis.ToString(), Scale, Component, bIsLengthAxis ? TEXT("scaled") : TEXT("at 1")),
                    FMath::IsNearlyEqual(Result[Component], bIsLengthAxis ? Scale : 1.f, 0.001f));
            }
        }
    }

    // Scale 1 is the identity, which is what makes the reference pose a no-op.
    TestTrue(TEXT("Scale 1 is the identity"),
        OctoArm::LengthAxisScale(FVector(0.f, -1.f, 0.f), 1.f).Equals(FVector::OneVector, 0.001f));

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
