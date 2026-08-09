#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "OctoOdyssey/OctoBodySpring.h"
#include "OctoOdyssey/OctoSkeleton.h"

// --------------------------------------------------------------------------
// PartyButtons.Octo.BodySpring.*
//
// Tests for the pure, world-free OctoBody:: head-spring math. No world, no
// actor, no skeletal mesh — the chain is synthesised here, so these run and
// fail for reasons that have nothing to do with the state of SK_Okto.
// (PartyButtons.Octo.Skeleton.BodyChainResolves is the test that pins the
// asset itself.)
// --------------------------------------------------------------------------

namespace
{
    /** SK_Okto's real head chain, component space, base to tip — see Skeleton.DumpRestPose. */
    const TArray<FVector> OktoChainCS = {
        FVector(  0.000, 0.0, 0.0),
        FVector(-31.987, 0.0, 0.0),
        FVector(-58.523, 0.0, 0.0),
        FVector(-85.059, 0.0, 0.0),
    };

    /**
     * Build a chain the way OctoSkeleton::BuildBodyBones would, from component-space
     * positions plus a shared bone orientation. The orientation is deliberately NOT identity
     * in most callers: an identity chain would pass a bone-space conversion that quietly
     * ignored rotation, which is exactly the bug worth catching.
     */
    FOctoBodyBones MakeChain(const TArray<FVector>& PositionsCS, const FQuat& BoneRotation)
    {
        FOctoBodyBones Bones;

        for (int32 i = 0; i < PositionsCS.Num(); i++)
        {
            Bones.ChainIndices.Add(i + 2); // SK_Okto's Body starts at index 2; the value is arbitrary here
            Bones.RefCS.Add(FTransform(BoneRotation, PositionsCS[i]));
        }

        Bones.RootParentRefCS = FTransform::Identity;

        for (int32 i = 0; i < Bones.RefCS.Num(); i++)
        {
            const FTransform& Parent = (i == 0) ? Bones.RootParentRefCS : Bones.RefCS[i - 1];
            Bones.RefLocal.Add(Bones.RefCS[i].GetRelativeTransform(Parent));
        }

        Bones.TipRestCS = PositionsCS.Last();

        const FVector BaseToTip = Bones.TipRestCS - PositionsCS[0];
        Bones.RestDir     = BaseToTip.GetSafeNormal();
        Bones.ChainLength = static_cast<float>(BaseToTip.Size());

        return Bones;
    }

    FOctoBodyBones MakeOktoChain()
    {
        // Yaw -90 puts local +Y onto component +X, which reproduces the real asset: it is
        // what turns SK_Okto's local (0, -31.987, 0) bone offsets into component-space -X.
        return MakeChain(OktoChainCS, FRotator(0.f, -90.f, 0.f).Quaternion());
    }

    /**
     * Re-compose bone-space transforms into the tip's component-space transform using the
     * SAME rule as OctoSkeleton::ComposeRefPoseComponentSpace. This is what makes the
     * round-trip test meaningful: it checks BuildChainBend against the engine's composition
     * convention rather than against its own arithmetic.
     */
    FTransform ComposeTip(const FOctoBodyBones& Bones, const TArray<FTransform>& BoneSpace)
    {
        FTransform ComponentSpace = Bones.RootParentRefCS;
        for (const FTransform& Local : BoneSpace)
        {
            ComponentSpace = Local * ComponentSpace;
        }
        return ComponentSpace;
    }

    float AngleBetweenDegrees(const FVector& A, const FVector& B)
    {
        const double Dot = FVector::DotProduct(A.GetSafeNormal(), B.GetSafeNormal());
        return FMath::RadiansToDegrees(static_cast<float>(FMath::Acos(FMath::Clamp(Dot, -1.0, 1.0))));
    }
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOctoBodySpringSettlesOnTarget,
    "PartyButtons.Octo.BodySpring.SpringSettlesOnTarget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOctoBodySpringSettlesOnTarget::RunTest(const FString& Parameters)
{
    const FVector Anchor(0.0, 0.0, 100.0);

    OctoBody::FHeadSpringState State;
    State.Reset(Anchor + FVector(0.0, 60.0, 0.0));

    for (int32 i = 0; i < 600; i++) // 10s at 60Hz
    {
        OctoBody::StepSpring(State, Anchor, FVector::ZeroVector, 2.5f, 0.35f, FVector::ZeroVector, 1.f / 60.f);
    }

    TestTrue(
        FString::Printf(TEXT("A displaced head returns to a stationary anchor (residual %.4f)"),
            (State.Position - Anchor).Size()),
        (State.Position - Anchor).Size() < 0.1);

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOctoBodySpringSnapsWhenUninitialised,
    "PartyButtons.Octo.BodySpring.SpringSnapsWhenUninitialised",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOctoBodySpringSnapsWhenUninitialised::RunTest(const FString& Parameters)
{
    // A zeroed struct must not spring in from the world origin — on spawn that reads as the
    // octopus whipping its head up off the floor for no reason.
    const FVector Anchor(500.0, -200.0, 1000.0);

    OctoBody::FHeadSpringState State;
    OctoBody::StepSpring(State, Anchor, FVector::ZeroVector, 2.5f, 0.35f, FVector::ZeroVector, 1.f / 60.f);

    TestTrue(TEXT("The first step snaps to the anchor"), State.Position.Equals(Anchor, 0.001));
    TestTrue(TEXT("...and marks itself initialised"), State.bInitialized);
    TestTrue(TEXT("...carrying no velocity"), State.Velocity.IsNearlyZero());

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOctoBodySpringLagsAcceleration,
    "PartyButtons.Octo.BodySpring.SpringLagsAcceleration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOctoBodySpringLagsAcceleration::RunTest(const FString& Parameters)
{
    // The headline behaviour: an anchor accelerating along +Y leaves the head trailing at -Y.
    constexpr float Accel = 800.f;
    constexpr float Dt    = 1.f / 60.f;

    OctoBody::FHeadSpringState State;
    State.Reset(FVector::ZeroVector);

    float   Time   = 0.f;
    FVector Anchor = FVector::ZeroVector;

    for (int32 i = 0; i < 120; i++) // 2s
    {
        const FVector Previous = Anchor;

        Time  += Dt;
        Anchor = FVector(0.0, 0.5 * Accel * Time * Time, 0.0);

        // The AVERAGE velocity over the frame, which is what AOctoPawn measures and what
        // StepSpring's contract requires — see its comment.
        OctoBody::StepSpring(State, Anchor, (Anchor - Previous) / Dt, 2.5f, 0.35f, FVector::ZeroVector, Dt);
    }

    const FVector Deflection = State.Position - Anchor;

    TestTrue(
        FString::Printf(TEXT("The head trails an accelerating body (deflection Y %.3f, expected negative)"), Deflection.Y),
        Deflection.Y < -1.0);

    // Steady state under constant acceleration is -a/omega^2. This once came out POSITIVE —
    // the head drifting steadily past the body it was supposed to trail — because the anchor
    // was stepped forward with a different integration scheme than the particle. Assert the
    // magnitude, not just the sign, so that failure mode cannot come back wearing a sign that
    // happens to look right.
    const float Omega    = 2.f * UE_PI * 2.5f;
    const float Expected = Accel / (Omega * Omega);
    TestTrue(
        FString::Printf(TEXT("...by a/omega^2, within 25%% (%.3f vs %.3f)"), -Deflection.Y, Expected),
        FMath::Abs(static_cast<float>(-Deflection.Y) - Expected) < Expected * 0.25f);

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOctoBodySpringDoesNotDriftOnAGlide,
    "PartyButtons.Octo.BodySpring.SpringDoesNotDriftOnAGlide",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOctoBodySpringDoesNotDriftOnAGlide::RunTest(const FString& Parameters)
{
    // Constant velocity is not acceleration, so there is nothing for the head to lag: it
    // must sit exactly on its anchor no matter how long the glide runs. The direct regression
    // guard on integrating the anchor's motion rather than removing it in closed form — that
    // bug showed up here as a deflection growing without bound.
    constexpr float Speed = 600.f;
    constexpr float Dt    = 1.f / 60.f;

    OctoBody::FHeadSpringState State;
    State.Reset(FVector::ZeroVector);

    FVector Anchor = FVector::ZeroVector;
    const FVector AnchorVelocity(0.0, Speed, 0.0);

    for (int32 i = 0; i < 600; i++) // 10s — long enough for any drift to be obvious
    {
        Anchor += AnchorVelocity * Dt;
        OctoBody::StepSpring(State, Anchor, AnchorVelocity, 2.5f, 0.35f, FVector::ZeroVector, Dt);
    }

    const FVector Deflection = State.Position - Anchor;
    TestTrue(
        FString::Printf(TEXT("A 10s glide leaves no deflection (%.5f)"), Deflection.Size()),
        Deflection.Size() < 0.01);

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOctoBodySpringOvershootsOnAbruptStop,
    "PartyButtons.Octo.BodySpring.SpringOvershootsOnAbruptStop",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOctoBodySpringOvershootsOnAbruptStop::RunTest(const FString& Parameters)
{
    // "Violently flailing when coming to an abrupt halt" — the head must carry its momentum
    // PAST the anchor and swing back, not ease in from behind.
    constexpr float Speed = 900.f;
    constexpr float Dt    = 1.f / 60.f;

    const FVector Anchor(0.0, 0.0, 0.0);

    OctoBody::FHeadSpringState State;
    State.Reset(Anchor);
    State.Velocity = FVector(0.0, Speed, 0.0); // the body just stopped dead; the head has not

    double MaxAhead   = 0.0;
    double MaxBehind  = 0.0;

    for (int32 i = 0; i < 300; i++) // 5s
    {
        OctoBody::StepSpring(State, Anchor, FVector::ZeroVector, 2.5f, 0.35f, FVector::ZeroVector, Dt);

        const double Y = (State.Position - Anchor).Y;
        MaxAhead  = FMath::Max(MaxAhead,  Y);
        MaxBehind = FMath::Min(MaxBehind, Y);
    }

    TestTrue(FString::Printf(TEXT("The head throws past the anchor (%.3f)"), MaxAhead), MaxAhead > 10.0);
    TestTrue(FString::Printf(TEXT("...then swings back through it (%.3f)"), MaxBehind), MaxBehind < -1.0);
    TestTrue(TEXT("...and ends at rest on it"), (State.Position - Anchor).Size() < 0.1);

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOctoBodySpringStableAtLargeTimestep,
    "PartyButtons.Octo.BodySpring.SpringStableAtLargeTimestep",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOctoBodySpringStableAtLargeTimestep::RunTest(const FString& Parameters)
{
    // The substepping guarantee. At the stiff end of the slider a single unsubstepped
    // half-second step would send the head to infinity.
    const FVector Anchor = FVector::ZeroVector;

    OctoBody::FHeadSpringState State;
    State.Reset(FVector(0.0, 80.0, 0.0));

    for (int32 i = 0; i < 20; i++)
    {
        OctoBody::StepSpring(State, Anchor, FVector::ZeroVector, 8.f, 0.05f, FVector::ZeroVector, 0.5f);

        TestTrue(FString::Printf(TEXT("Step %d stays finite"), i), !State.Position.ContainsNaN());
        TestTrue(
            FString::Printf(TEXT("Step %d stays bounded (%.3f)"), i, State.Position.Size()),
            State.Position.Size() < 1000.0);
    }

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOctoBodyDeflectionIsPlanarAndClamped,
    "PartyButtons.Octo.BodySpring.DeflectionIsPlanarAndClamped",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOctoBodyDeflectionIsPlanarAndClamped::RunTest(const FString& Parameters)
{
    const FVector Raw(120.0, 30.0, -40.0);

    const FVector Clamped = OctoBody::ConstrainDeflection(Raw, 25.f);
    TestTrue(TEXT("X is dropped — the head stays in the play plane"), Clamped.X == 0.0);
    TestTrue(FString::Printf(TEXT("Magnitude is clamped (%.3f)"), Clamped.Size()),
        FMath::IsNearlyEqual(Clamped.Size(), 25.0, 0.001));
    TestTrue(TEXT("Direction in-plane is preserved"),
        Clamped.GetSafeNormal().Equals(FVector(0.0, 30.0, -40.0).GetSafeNormal(), 0.001));

    const FVector Unclamped = OctoBody::ConstrainDeflection(Raw, 0.f);
    TestTrue(TEXT("A zero cap disables the clamp rather than collapsing the head"),
        Unclamped.Equals(FVector(0.0, 30.0, -40.0), 0.001));

    const FVector Under = OctoBody::ConstrainDeflection(FVector(0.0, 3.0, 4.0), 25.f);
    TestTrue(TEXT("A deflection under the cap is untouched"), Under.Equals(FVector(0.0, 3.0, 4.0), 0.001));

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOctoBodyBendWeightsAreNormalised,
    "PartyButtons.Octo.BodySpring.BendWeightsAreNormalised",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOctoBodyBendWeightsAreNormalised::RunTest(const FString& Parameters)
{
    const TArray<float> Tapers = { 0.f, 0.25f, 0.5f, 0.7f, 1.f };

    for (const float Taper : Tapers)
    {
        TArray<float> Weights;
        OctoBody::BendWeights(3, Taper, Weights);

        TestEqual(FString::Printf(TEXT("One weight per joint (taper %.2f)"), Taper), Weights.Num(), 3);

        float Total = 0.f;
        for (int32 i = 0; i < Weights.Num(); i++)
        {
            Total += Weights[i];
            if (i > 0)
            {
                // Non-decreasing toward the head is what keeps the chain's base — where the
                // eight rigid arms emerge — the least disturbed part of the body.
                TestTrue(
                    FString::Printf(TEXT("Weight %d >= weight %d (taper %.2f)"), i, i - 1, Taper),
                    Weights[i] >= Weights[i - 1] - UE_KINDA_SMALL_NUMBER);
            }
        }

        TestTrue(FString::Printf(TEXT("Weights sum to 1 (taper %.2f, got %.5f)"), Taper, Total),
            FMath::IsNearlyEqual(Total, 1.f, 0.0001f));
    }

    TArray<float> Even;
    OctoBody::BendWeights(3, 0.f, Even);
    TestTrue(TEXT("Taper 0 spreads the bend evenly"),
        FMath::IsNearlyEqual(Even[0], Even[2], 0.0001f));

    TArray<float> Tapered;
    OctoBody::BendWeights(3, 1.f, Tapered);
    TestTrue(FString::Printf(TEXT("Taper 1 crowds the bend into the head (%.3f vs %.3f)"), Tapered[2], Tapered[0]),
        Tapered[2] > Tapered[0] * 4.f);

    TArray<float> Empty;
    OctoBody::BendWeights(0, 0.5f, Empty);
    TestEqual(TEXT("A zero-joint chain yields no weights"), Empty.Num(), 0);

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOctoBodyChainBendRoundTrips,
    "PartyButtons.Octo.BodySpring.ChainBendRoundTrips",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOctoBodyChainBendRoundTrips::RunTest(const FString& Parameters)
{
    const FOctoBodyBones Bones = MakeOktoChain();

    TestTrue(TEXT("The synthesised chain is valid"), Bones.IsValid());
    TestTrue(FString::Printf(TEXT("...and 85.06cm long (%.3f)"), Bones.ChainLength),
        FMath::IsNearlyEqual(Bones.ChainLength, 85.059f, 0.01f));

    // ---- Zero deflection must be a perfect no-op --------------------------
    {
        TArray<FTransform> Pose;
        OctoBody::BuildChainBend(Bones, Bones.TipRestCS, 55.f, 0.7f, Pose);

        TestEqual(TEXT("One transform per chain bone"), Pose.Num(), Bones.ChainIndices.Num());
        for (int32 i = 0; i < Pose.Num(); i++)
        {
            TestTrue(
                FString::Printf(TEXT("Bone %d reproduces the reference pose exactly"), i),
                Pose[i].Equals(Bones.RefLocal[i], 0.001));
        }
    }

    // ---- A real deflection lands the tip on the arc -----------------------
    const TArray<FVector> Deflections = {
        FVector(0.0,  40.0,   0.0),
        FVector(0.0, -15.0,  25.0),
        FVector(0.0,   0.0, -30.0),
    };

    for (const FVector& Deflection : Deflections)
    {
        const FVector Target = Bones.TipRestCS + Deflection;

        TArray<FTransform> Pose;
        FTransform         TipCS = FTransform::Identity;
        OctoBody::BuildChainBend(Bones, Target, 180.f, 0.7f, Pose, &TipCS);

        const FTransform Composed = ComposeTip(Bones, Pose);
        const FVector    Base     = Bones.RefCS[0].GetLocation();
        const FVector    WantDir  = (Target - Base).GetSafeNormal();
        const FVector    Expected = Base + WantDir * Bones.ChainLength;

        // The composition check: BuildChainBend's bone-space output, re-composed the way the
        // engine composes it, has to agree with the component-space transform it reported.
        TestTrue(
            FString::Printf(TEXT("Deflection %s: the re-composed tip matches the reported one"), *Deflection.ToString()),
            Composed.GetLocation().Equals(TipCS.GetLocation(), 0.01));

        // The chain is inextensible, so the tip lands on the arc — pointing at the target,
        // at the rest length, NOT at the target itself.
        TestTrue(
            FString::Printf(TEXT("Deflection %s: the tip lands on the arc (%s vs %s)"),
                *Deflection.ToString(), *Composed.GetLocation().ToString(), *Expected.ToString()),
            Composed.GetLocation().Equals(Expected, 0.01));

        TestTrue(
            FString::Printf(TEXT("Deflection %s: the chain keeps its length"), *Deflection.ToString()),
            FMath::IsNearlyEqual((Composed.GetLocation() - Base).Size(), Bones.ChainLength, 0.01));
    }

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOctoBodyBendRespectsMaxAngle,
    "PartyButtons.Octo.BodySpring.BendRespectsMaxAngle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOctoBodyBendRespectsMaxAngle::RunTest(const FString& Parameters)
{
    const FOctoBodyBones Bones = MakeOktoChain();
    const FVector        Base  = Bones.RefCS[0].GetLocation();

    const TArray<float> Caps = { 0.f, 15.f, 30.f, 55.f };

    for (const float MaxDegrees : Caps)
    {
        // Ask for far more bend than allowed — a deflection twice the chain's own length.
        const FVector Target = Bones.TipRestCS + FVector(0.0, 200.0, 0.0);

        TArray<FTransform> Pose;
        OctoBody::BuildChainBend(Bones, Target, MaxDegrees, 0.7f, Pose);

        const FVector Posed  = ComposeTip(Bones, Pose).GetLocation() - Base;
        const float   Actual = AngleBetweenDegrees(Bones.RestDir, Posed);

        TestTrue(
            FString::Printf(TEXT("Bend is capped at %.1f degrees (got %.3f)"), MaxDegrees, Actual),
            Actual <= MaxDegrees + 0.1f);

        // ...and actually reaches the cap, so the clamp isn't quietly killing the bend.
        TestTrue(
            FString::Printf(TEXT("...and reaches it (%.3f)"), Actual),
            Actual >= MaxDegrees - 0.1f);
    }

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOctoBodyChainBendHandlesInvalidRig,
    "PartyButtons.Octo.BodySpring.ChainBendHandlesInvalidRig",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOctoBodyChainBendHandlesInvalidRig::RunTest(const FString& Parameters)
{
    // The fail-soft contract: an unresolved rig produces no writes, not a crash and not a
    // garbage pose. AOctoPawn leans on this to render a rigid head after a bad re-export.
    const FOctoBodyBones Empty;

    TArray<FTransform> Pose;
    FTransform         TipCS = FTransform::Identity;
    OctoBody::BuildChainBend(Empty, FVector(1.0, 2.0, 3.0), 55.f, 0.7f, Pose, &TipCS);

    TestEqual(TEXT("An invalid rig writes no bones"), Pose.Num(), 0);
    TestTrue(TEXT("...and reports an identity tip"), TipCS.Equals(FTransform::Identity, 0.001));

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOctoBodySquashDeformsAlongTheNormal,
    "PartyButtons.Octo.BodySpring.SquashDeformsAlongTheNormal",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOctoBodySquashDeformsAlongTheNormal::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("Zero squash is exactly unit scale"),
        OctoBody::SquashScale(FVector::ZAxisVector, 0.f).Equals(FVector::OneVector, 0.0001));

    TestTrue(TEXT("A degenerate normal is unit scale, not a division by zero"),
        OctoBody::SquashScale(FVector::ZeroVector, 0.5f).Equals(FVector::OneVector, 0.0001));

    // A floor landing: compress on Z, bulge on X and Y.
    const FVector Floor = OctoBody::SquashScale(FVector::ZAxisVector, 0.4f);
    TestTrue(FString::Printf(TEXT("Compressed along the normal (%.3f)"), Floor.Z), Floor.Z < 0.95);
    TestTrue(FString::Printf(TEXT("Bulged across it on X (%.3f)"), Floor.X), Floor.X > 1.0);
    TestTrue(FString::Printf(TEXT("Bulged across it on Y (%.3f)"), Floor.Y), Floor.Y > 1.0);
    TestTrue(TEXT("The two across-axes bulge equally"), FMath::IsNearlyEqual(Floor.X, Floor.Y, 0.0001));

    // Sign must not matter — squashing "down" and "up" an axis are the same deformation.
    TestTrue(TEXT("The normal's sign is irrelevant"),
        OctoBody::SquashScale(-FVector::ZAxisVector, 0.4f).Equals(Floor, 0.0001));

    // A wall: same deformation, rotated onto Y.
    const FVector Wall = OctoBody::SquashScale(FVector::YAxisVector, 0.4f);
    TestTrue(TEXT("A wall hit compresses on Y instead"), FMath::IsNearlyEqual(Wall.Y, Floor.Z, 0.0001));

    // Even an absurd tuning must not turn the head inside out.
    const FVector Extreme = OctoBody::SquashScale(FVector::ZAxisVector, 5.f);
    TestTrue(TEXT("Extreme squash never inverts the head"), Extreme.GetMin() > 0.0);

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOctoBodySquashRingsBackToRest,
    "PartyButtons.Octo.BodySpring.SquashRingsBackToRest",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOctoBodySquashRingsBackToRest::RunTest(const FString& Parameters)
{
    float Amount   = 0.35f;
    float Velocity = 0.f;
    bool  bWentNegative = false;

    for (int32 i = 0; i < 300; i++) // 5s at 60Hz
    {
        OctoBody::StepSquash(Amount, Velocity, 6.f, 0.25f, 1.f / 60.f);
        bWentNegative = bWentNegative || (Amount < -0.001f);
    }

    TestTrue(TEXT("The squash wobbles (overshoots into stretch)"), bWentNegative);
    TestTrue(FString::Printf(TEXT("...and settles at rest (%.5f)"), Amount), Amount == 0.f);
    TestTrue(TEXT("...with no residual velocity"), Velocity == 0.f);

    // A large step must not ring up instead of down.
    float Big = 0.35f;
    float BigVelocity = 0.f;
    OctoBody::StepSquash(Big, BigVelocity, 15.f, 0.05f, 0.5f);
    TestTrue(FString::Printf(TEXT("A hitched frame stays bounded (%.5f)"), Big), FMath::Abs(Big) < 1.f);

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
