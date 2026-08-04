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

float BlockedFraction(float DesiredDelta, float AchievedDelta)
{
    if (DesiredDelta <= 0.f)
    {
        // Nothing was being attempted (retracting, or already at the target) — nothing was blocked.
        return 0.f;
    }
    return FMath::Clamp(1.f - AchievedDelta / DesiredDelta, 0.f, 1.f);
}

float LaunchImpulseMagnitude(float BlockedFrac, float ArmSpeed, float ImpulsePerUnitSpeed, float MaxImpulse)
{
    return FMath::Clamp(BlockedFrac * ArmSpeed * ImpulsePerUnitSpeed, 0.f, MaxImpulse);
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
