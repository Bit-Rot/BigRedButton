#include "OctoOdyssey/OctoBodySpring.h"

namespace OctoBody
{

namespace
{
    /**
     * Clamp the frame, then split it into stable substeps. Takes DeltaSeconds by reference
     * because callers need the CLAMPED total to divide by — using the raw one would make the
     * substeps longer than MaxSubstep, which is the thing this exists to prevent.
     */
    int32 SubstepCount(float& InOutDeltaSeconds)
    {
        InOutDeltaSeconds = FMath::Min(InOutDeltaSeconds, MaxFrameTime);
        const int32 Steps = FMath::CeilToInt(InOutDeltaSeconds / MaxSubstep);
        return FMath::Clamp(Steps, 1, MaxSubstepsPerFrame);
    }
}

void StepSpring(
    FHeadSpringState& State,
    const FVector& Target,
    const FVector& TargetVelocity,
    float FrequencyHz,
    float DampingRatio,
    const FVector& ExtraAccel,
    float DeltaSeconds)
{
    // First frame: snap. Springing in from wherever the struct was zeroed would fling the
    // head across the level on spawn.
    if (!State.bInitialized)
    {
        State.Reset(Target);
        return;
    }

    if (DeltaSeconds <= 0.f)
    {
        return;
    }

    // A zero frequency is "no spring", not a division by zero. Pinning the head to the
    // anchor is the sane reading — the alternative is a head that drifts away forever.
    if (FrequencyHz <= UE_KINDA_SMALL_NUMBER)
    {
        State.Reset(Target);
        return;
    }

    float Dt = DeltaSeconds;
    const int32 Steps = SubstepCount(Dt);
    const float Step  = Dt / static_cast<float>(Steps);

    const float Omega = 2.f * UE_PI * FrequencyHz;
    const float Zeta  = FMath::Max(0.f, DampingRatio);

    // ---- Integrate in the ANCHOR's frame ----------------------------------
    //
    // Offset and RelVel are measured against the anchor, and the anchor's own motion across
    // the frame is subtracted analytically rather than stepped alongside the particle. That
    // is not a tidiness choice, it is the correctness one: stepping the anchor forward by
    // TargetVelocity * Step while the particle advances by its own freshly-updated velocity
    // integrates the two with different schemes, and under sustained acceleration the
    // mismatch accumulates without bound — enough to send the head DRIFTING PAST the body it
    // is supposed to trail. Removing the anchor's motion in closed form leaves nothing for
    // that error to accumulate in.
    //
    // The anchor is straight-line over the frame by construction: the caller measures
    // TargetVelocity as (Target - PrevTarget) / DeltaSeconds, so Target - TargetVelocity * Dt
    // IS where the anchor started. Its acceleration therefore never appears here — it enters
    // as the frame-to-frame CHANGE in TargetVelocity, which lands in RelVel below. That is
    // what produces the lag when the octopus accelerates and the flail when it stops dead.
    FVector Offset = State.Position - (Target - TargetVelocity * Dt);
    FVector RelVel = State.Velocity - TargetVelocity;

    for (int32 i = 0; i < Steps; i++)
    {
        const FVector Accel = -(Omega * Omega) * Offset - (2.f * Zeta * Omega) * RelVel + ExtraAccel;

        // Semi-implicit: velocity first, then position from the NEW velocity. Explicit Euler
        // gains energy every step and would eventually shake the head apart on its own.
        RelVel += Accel * Step;
        Offset += RelVel * Step;
    }

    State.Position = Target + Offset;
    State.Velocity = TargetVelocity + RelVel;

    if (State.Position.ContainsNaN() || State.Velocity.ContainsNaN())
    {
        State.Reset(Target);
    }
}

FVector ConstrainDeflection(const FVector& Deflection, float MaxDeflection)
{
    FVector Planar(0.f, Deflection.Y, Deflection.Z);

    if (MaxDeflection > 0.f)
    {
        const float Size = static_cast<float>(Planar.Size());
        if (Size > MaxDeflection)
        {
            Planar *= (MaxDeflection / Size);
        }
    }

    return Planar;
}

void BendWeights(int32 NumJoints, float Taper, TArray<float>& OutWeights)
{
    OutWeights.Reset();
    if (NumJoints <= 0)
    {
        return;
    }

    OutWeights.SetNum(NumJoints);

    // Exponent 0 gives every joint a weight of 1 (an even bend); rising to 4 crowds the bend
    // into the joints nearest the head. The curve is arbitrary, but it is monotonic in both
    // the joint index and the taper, which is all the caller and the tests rely on.
    const float Exponent = FMath::Clamp(Taper, 0.f, 1.f) * 4.f;

    float Total = 0.f;
    for (int32 i = 0; i < NumJoints; i++)
    {
        OutWeights[i] = FMath::Pow(static_cast<float>(i + 1) / static_cast<float>(NumJoints), Exponent);
        Total += OutWeights[i];
    }

    if (Total <= UE_KINDA_SMALL_NUMBER)
    {
        for (float& Weight : OutWeights)
        {
            Weight = 1.f / static_cast<float>(NumJoints);
        }
        return;
    }

    for (float& Weight : OutWeights)
    {
        Weight /= Total;
    }
}

void BuildChainBend(
    const FOctoBodyBones& Bones,
    const FVector& TargetTipCS,
    float MaxBendDegrees,
    float Taper,
    TArray<FTransform>& OutBoneSpace,
    FTransform* OutTipCS)
{
    OutBoneSpace.Reset();

    if (!Bones.IsValid())
    {
        if (OutTipCS) { *OutTipCS = FTransform::Identity; }
        return;
    }

    const int32   NumBones = Bones.ChainIndices.Num();
    const FVector Base     = Bones.RefCS[0].GetLocation();

    FVector WantDir = (TargetTipCS - Base).GetSafeNormal();
    if (WantDir.IsNearlyZero())
    {
        WantDir = Bones.RestDir;
    }

    FQuat Bend = FQuat::FindBetweenNormals(Bones.RestDir, WantDir);

    // Clamp the TOTAL before distributing it. Clamping each joint's share instead would let
    // the composed angle creep past the limit as joints were added to the chain.
    {
        FVector       Axis  = FVector::ZAxisVector;
        FQuat::FReal  Angle = 0.;
        Bend.ToAxisAndAngle(Axis, Angle);

        const FQuat::FReal MaxAngle = FMath::DegreesToRadians(FMath::Max(0.f, MaxBendDegrees));
        if (Angle > MaxAngle)
        {
            Bend = FQuat(Axis, MaxAngle);
        }
    }

    // One weight per JOINT — the tip is a leaf and carries no rotation of its own.
    TArray<float> Weights;
    BendWeights(NumBones - 1, Taper, Weights);

    OutBoneSpace.SetNum(NumBones);

    FTransform ParentCS   = Bones.RootParentRefCS;
    float      Cumulative = 0.f;

    for (int32 i = 0; i < NumBones; i++)
    {
        // Cumulative, so the base is unrotated and the tip receives the whole bend.
        const FQuat Rotation = FQuat::Slerp(FQuat::Identity, Bend, FMath::Clamp(Cumulative, 0.f, 1.f));

        FTransform PosedCS;
        PosedCS.SetRotation(Rotation * Bones.RefCS[i].GetRotation());
        PosedCS.SetTranslation(Base + Rotation.RotateVector(Bones.RefCS[i].GetLocation() - Base));
        PosedCS.SetScale3D(Bones.RefCS[i].GetScale3D());

        // Exact against ComponentSpace[i] == BoneSpace[i] * ComponentSpace[Parent], the rule
        // OctoSkeleton::ComposeRefPoseComponentSpace composes the reference pose with.
        OutBoneSpace[i] = PosedCS.GetRelativeTransform(ParentCS);

        if (i == NumBones - 1 && OutTipCS)
        {
            *OutTipCS = PosedCS;
        }

        ParentCS = PosedCS;
        if (Weights.IsValidIndex(i))
        {
            Cumulative += Weights[i];
        }
    }
}

FVector SquashScale(const FVector& NormalInBoneSpace, float Amount)
{
    if (FMath::IsNearlyZero(Amount))
    {
        return FVector::OneVector;
    }

    const FVector Normal = NormalInBoneSpace.GetSafeNormal();
    if (Normal.IsNearlyZero())
    {
        return FVector::OneVector;
    }

    // The scale is axis-aligned, so only how much of the normal lies on each axis matters —
    // its sign does not (squashing "down" and "up" an axis are the same deformation).
    const FVector Axes = Normal.GetAbs();
    const float   S    = FMath::Clamp(Amount, -0.95f, 0.95f);

    FVector Scale = FVector::OneVector;
    for (int32 k = 0; k < 3; k++)
    {
        // Compress along the normal, bulge across it — a cheap stand-in for conserving volume.
        Scale[k] = 1.f - S * Axes[k] + 0.5f * S * (1.f - Axes[k]);
    }

    return Scale.ComponentMax(FVector(0.2)).ComponentMin(FVector(3.0));
}

void StepSquash(float& Amount, float& Velocity, float FrequencyHz, float DampingRatio, float DeltaSeconds)
{
    if (DeltaSeconds <= 0.f)
    {
        return;
    }

    if (FrequencyHz <= UE_KINDA_SMALL_NUMBER)
    {
        Amount   = 0.f;
        Velocity = 0.f;
        return;
    }

    float Dt = DeltaSeconds;
    const int32 Steps = SubstepCount(Dt);
    const float Step  = Dt / static_cast<float>(Steps);

    const float Omega = 2.f * UE_PI * FrequencyHz;
    const float Zeta  = FMath::Max(0.f, DampingRatio);

    for (int32 i = 0; i < Steps; i++)
    {
        const float Accel = -(Omega * Omega) * Amount - (2.f * Zeta * Omega) * Velocity;
        Velocity += Accel * Step;
        Amount   += Velocity * Step;
    }

    if (!FMath::IsFinite(Amount) || !FMath::IsFinite(Velocity))
    {
        Amount   = 0.f;
        Velocity = 0.f;
        return;
    }

    // Ringing this small is invisible; snapping it off stops the head carrying a permanent
    // sub-perceptible scale and keeps the "is it at rest" test in the pawn honest.
    if (FMath::Abs(Amount) < 0.001f && FMath::Abs(Velocity) < 0.01f)
    {
        Amount   = 0.f;
        Velocity = 0.f;
    }
}

} // namespace OctoBody
