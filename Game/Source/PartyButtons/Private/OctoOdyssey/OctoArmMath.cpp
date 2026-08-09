#include "OctoOdyssey/OctoArmMath.h"

namespace OctoArm
{

FVector ArmDirectionLocal(int32 ArmIndex, int32 ArmCount, float AngleOffsetDegrees)
{
    if (ArmCount <= 0) { return FVector(0.f, 0.f, 1.f); }

    // Wrap ArmIndex into [0, ArmCount) — handles negative indices too.
    const int32 Wrapped = ((ArmIndex % ArmCount) + ArmCount) % ArmCount;

    const float ThetaDeg = AngleOffsetDegrees + 360.f * static_cast<float>(Wrapped) / static_cast<float>(ArmCount);
    const float ThetaRad = FMath::DegreesToRadians(ThetaDeg);

    // Matches FRotator(0,0,Roll)'s mapping of +Z -> (0, sin Roll, cos Roll):
    // the arm capsule's local +Z axis IS the arm axis at relative rotation
    // FRotator(0, 0, ThetaDeg), no extra alignment math needed at the callsite.
    return FVector(0.f, FMath::Sin(ThetaRad), FMath::Cos(ThetaRad));
}

FVector ArmDirectionWorld(int32 ArmIndex, int32 ArmCount, float AngleOffsetDegrees, float BodyRollDegrees)
{
    // Body roll simply adds to the arm's angle — rotation composition for
    // this family of directions is angle addition (verified against the
    // engine's FRotator(0,0,Roll).RotateVector by
    // PartyButtons.Octo.ArmMath.ArmDirectionWorldMatchesRollRotation).
    return ArmDirectionLocal(ArmIndex, ArmCount, AngleOffsetDegrees + BodyRollDegrees);
}

float CapsuleCenterOffset(float SphereRadius, float CapsuleRadius, float CapsuleHalfHeight, float Extension)
{
    return SphereRadius + CapsuleRadius - CapsuleHalfHeight + Extension;
}

float HandCenterOffset(float SphereRadius, float CapsuleRadius, float CapsuleHalfHeight, float Extension)
{
    // Written as "capsule center, then out to the cap center" rather than as the
    // simplified SphereRadius + Extension, so the geometric intent survives a future
    // change to CapsuleCenterOffset. The two forms are pinned equal by a test.
    return CapsuleCenterOffset(SphereRadius, CapsuleRadius, CapsuleHalfHeight, Extension)
         + (CapsuleHalfHeight - CapsuleRadius);
}

float ArmMeshLength(float CapsuleRadius, float CapsuleHalfHeight)
{
    // Inner tip (capsule center - HalfHeight) to hand center (capsule center +
    // HalfHeight - CapsuleRadius).
    return 2.f * CapsuleHalfHeight - CapsuleRadius;
}

float ArmMeshCenterOffset(float SphereRadius, float CapsuleRadius, float CapsuleHalfHeight, float Extension)
{
    // Shortened at the tip end only, so the mesh center sits half the removed length
    // (CapsuleRadius/2) inward of the capsule center.
    return CapsuleCenterOffset(SphereRadius, CapsuleRadius, CapsuleHalfHeight, Extension)
         - 0.5f * CapsuleRadius;
}

float ArmChainLengthScale(float ChainOriginOffset, float RestChainLength, float TargetTipOffset)
{
    // A chain with no length can't be stretched into one — leave it alone rather
    // than divide by zero. BuildArmBones rejects such a skeleton up front, so this
    // is a backstop, not a supported configuration.
    if (RestChainLength <= UE_KINDA_SMALL_NUMBER) { return 1.f; }

    // Clamped at 0: a target inside the chain root would otherwise mirror the arm
    // back through the body.
    return FMath::Max(0.f, (TargetTipOffset - ChainOriginOffset) / RestChainLength);
}

FVector LengthAxisScale(const FVector& LengthAxis, float Scale)
{
    // Component-wise: 1 + |axis| * (Scale - 1). For an axis-aligned LengthAxis that
    // is exactly (1,1,1) with Scale substituted into the one live component.
    return FVector::OneVector + LengthAxis.GetAbs() * (Scale - 1.f);
}

FLinearColor HandColor(int32 ArmIndex, int32 ArmCount)
{
    if (ArmCount <= 0) { return FLinearColor::White; }

    // Same wrap convention as ArmDirectionLocal — handles negative indices too.
    const int32 Wrapped = ((ArmIndex % ArmCount) + ArmCount) % ArmCount;

    // Hue in degrees, saturation and value both pegged to 1 for maximum separability.
    // Built in float rather than via MakeFromHSV8 because that helper quantises hue to
    // a byte spanning 0..360 over 0..255, which would space 8 arms 45.18deg apart
    // instead of exactly 45deg. FLinearColor's HSV convention is (R=hue degrees,
    // G=saturation, B=value).
    const float HueDegrees = 360.f * static_cast<float>(Wrapped) / static_cast<float>(ArmCount);
    return FLinearColor(HueDegrees, 1.f, 1.f).HSVToLinearRGB();
}

float MaxSafeExtension(float SphereRadius, float CapsuleRadius, float CapsuleHalfHeight)
{
    // Binding constraint: the capsule's inner cap (offset - HalfHeight) must
    // stay inside the sphere (<= SphereRadius). Solving
    //   (SphereRadius + CapsuleRadius - CapsuleHalfHeight + E) - CapsuleHalfHeight <= SphereRadius
    // for E gives 2*CapsuleHalfHeight - CapsuleRadius — independent of SphereRadius.
    return 2.f * CapsuleHalfHeight - CapsuleRadius;
}

float StepExtension(float CurrentExtension, bool bPressed, float ExtendSpeed, float RetractSpeed, float MaxExtension, float DeltaSeconds)
{
    if (DeltaSeconds <= 0.f) { return CurrentExtension; }

    const float Speed = bPressed ? ExtendSpeed : -RetractSpeed;
    return FMath::Clamp(CurrentExtension + Speed * DeltaSeconds, 0.f, MaxExtension);
}

float PushOffSpeedDeficit(const FVector& BodyVelocity, const FVector& PushDirection, float ArmSpeed)
{
    if (ArmSpeed <= 0.f) { return 0.f; }

    // Only the component along the push matters — a body sliding sideways
    // along a wall is neither helped nor hindered by an arm planted into it.
    return FMath::Max(0.f, ArmSpeed - (BodyVelocity | PushDirection));
}

FVector ClampLaunchVelocity(const FVector& Velocity, float MaxSpeed)
{
    const float SpeedSq = Velocity.SizeSquared();
    if (MaxSpeed < 0.f || SpeedSq <= FMath::Square(MaxSpeed))
    {
        return Velocity;
    }
    return Velocity.GetSafeNormal() * MaxSpeed; // zero vector -> GetSafeNormal returns zero, no NaN
}

} // namespace OctoArm
