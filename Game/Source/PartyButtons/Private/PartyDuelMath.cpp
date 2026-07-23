#include "PartyDuelMath.h"

namespace PartyDuel
{

EReleaseAction ClassifyRelease(float HeldSeconds, float TapWindowSeconds, float MinChargeSeconds)
{
    if (HeldSeconds <= TapWindowSeconds)
    {
        return EReleaseAction::Reflect;
    }
    if (HeldSeconds < MinChargeSeconds)
    {
        return EReleaseAction::Cancel;
    }
    return EReleaseAction::Shoot;
}

float ChargeToSpeed(
    float HeldSeconds,
    float MinChargeSeconds,
    float MaxChargeSeconds,
    float SpeedAtMinCharge,
    float SpeedAtMaxCharge)
{
    if (MaxChargeSeconds <= MinChargeSeconds)
    {
        // Degenerate range — nothing to interpolate over.
        return SpeedAtMaxCharge;
    }

    const float Clamped = FMath::Clamp(HeldSeconds, MinChargeSeconds, MaxChargeSeconds);
    const float Alpha   = (Clamped - MinChargeSeconds) / (MaxChargeSeconds - MinChargeSeconds);
    return FMath::Lerp(SpeedAtMinCharge, SpeedAtMaxCharge, Alpha);
}

FVector Reflect(const FVector& V, const FVector& N)
{
    return V - 2.0f * FVector::DotProduct(V, N) * N;
}

} // namespace PartyDuel
