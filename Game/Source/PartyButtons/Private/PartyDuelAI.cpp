#include "PartyDuelAI.h"

namespace PartyDuelAI
{

FDuelAIParams EasyParams()
{
    FDuelAIParams P;
    P.AimErrorStdDevDeg       = 14.f;
    P.AimToleranceDeg         = 4.f;
    P.FireProbability         = 0.5f;
    P.RicochetProbability     = 0.08f;
    P.MaxRicochetBounces      = 1;
    P.PreferredChargeFrac     = 0.45f;
    P.ReactionLatencySeconds  = 0.45f;
    P.BlockWhenThreatenedProb = 0.25f;
    P.BlockThreatHorizonSeconds = 0.25f;
    return P;
}

FDuelAIParams MediumParams()
{
    // Defaults on FDuelAIParams already encode the believable-and-beatable
    // profile; spelled out here anyway so the three tiers read side by side.
    FDuelAIParams P;
    P.AimErrorStdDevDeg       = 7.f;
    P.AimToleranceDeg         = 3.f;
    P.FireProbability         = 0.7f;
    P.RicochetProbability     = 0.2f;
    P.MaxRicochetBounces      = 1;
    P.PreferredChargeFrac     = 0.6f;
    P.ReactionLatencySeconds  = 0.2f;
    P.BlockWhenThreatenedProb = 0.5f;
    P.BlockThreatHorizonSeconds = 0.35f;
    return P;
}

FDuelAIParams HardParams()
{
    FDuelAIParams P;
    P.AimErrorStdDevDeg       = 3.f;
    P.AimToleranceDeg         = 2.f;
    P.FireProbability         = 0.9f;
    P.RicochetProbability     = 0.35f;
    P.MaxRicochetBounces      = 2;
    P.PreferredChargeFrac     = 0.75f;
    P.ReactionLatencySeconds  = 0.15f;
    P.BlockWhenThreatenedProb = 0.75f;
    P.BlockThreatHorizonSeconds = 0.45f;
    return P;
}

float BearingDegXY(const FVector2D& From, const FVector2D& To)
{
    const FVector2D Delta = To - From;
    return FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X));
}

float SignedAngleDeltaDeg(float FromDeg, float ToDeg)
{
    float Delta = FMath::Fmod(ToDeg - FromDeg, 360.f);
    if (Delta > 180.f)       { Delta -= 360.f; }
    else if (Delta < -180.f) { Delta += 360.f; }
    return Delta;
}

float TimeToAlignSeconds(float FacingDeg, float SpinRateDegPerSec, float BearingDeg)
{
    if (FMath::IsNearlyZero(SpinRateDegPerSec))
    {
        return -1.f; // not spinning — will never align on its own
    }

    float Delta = FMath::Fmod(BearingDeg - FacingDeg, 360.f);
    if (SpinRateDegPerSec > 0.f)
    {
        if (Delta < 0.f) { Delta += 360.f; }
    }
    else
    {
        if (Delta > 0.f) { Delta -= 360.f; }
    }
    return Delta / SpinRateDegPerSec; // always >= 0 given the sign handling above
}

namespace
{
    FVector2D MirrorPointOverWall(const FVector2D& P, const FBox2D& Box, EBounceWall Wall)
    {
        switch (Wall)
        {
        case EBounceWall::North: return FVector2D(P.X, 2.f * Box.Max.Y - P.Y);
        case EBounceWall::South: return FVector2D(P.X, 2.f * Box.Min.Y - P.Y);
        case EBounceWall::East:  return FVector2D(2.f * Box.Max.X - P.X, P.Y);
        case EBounceWall::West:  return FVector2D(2.f * Box.Min.X - P.X, P.Y);
        default:                 return P;
        }
    }

    /**
     * Finds where segment A->B crosses Wall's line, and whether that crossing
     * falls within both the segment (strictly between A and B) and the wall's
     * finite extent along BounceBox. Shared by the single- and double-bounce
     * planners below.
     */
    bool FindWallCrossing(const FVector2D& A, const FVector2D& B, const FBox2D& Box, EBounceWall Wall, FVector2D& OutCrossPoint)
    {
        float WallCoord = 0.f;
        bool bHorizontal = false; // true for North/South: the wall is a horizontal line at a fixed Y
        switch (Wall)
        {
        case EBounceWall::North: WallCoord = Box.Max.Y; bHorizontal = true;  break;
        case EBounceWall::South: WallCoord = Box.Min.Y; bHorizontal = true;  break;
        case EBounceWall::East:  WallCoord = Box.Max.X; bHorizontal = false; break;
        case EBounceWall::West:  WallCoord = Box.Min.X; bHorizontal = false; break;
        }

        const float AAxis = bHorizontal ? A.Y : A.X;
        const float BAxis = bHorizontal ? B.Y : B.X;
        const float AxisDelta = BAxis - AAxis;
        if (FMath::IsNearlyZero(AxisDelta))
        {
            return false; // segment runs parallel to the wall — no meaningful crossing
        }

        const float T = (WallCoord - AAxis) / AxisDelta;
        if (T <= 0.f || T >= 1.f)
        {
            return false; // crossing isn't strictly between A and B
        }

        OutCrossPoint = A + (B - A) * T;

        const float CrossOther = bHorizontal ? OutCrossPoint.X : OutCrossPoint.Y;
        const float ExtentMin  = bHorizontal ? Box.Min.X : Box.Min.Y;
        const float ExtentMax  = bHorizontal ? Box.Max.X : Box.Max.Y;
        return CrossOther >= ExtentMin && CrossOther <= ExtentMax;
    }

    void ShuffleWallOrder(FRandomStream& Rng, EBounceWall (&Order)[4])
    {
        for (int32 i = 3; i > 0; i--)
        {
            const int32 j = Rng.RandRange(0, i);
            Swap(Order[i], Order[j]);
        }
    }
} // anonymous namespace

bool PlanSingleBounceBearing(
    const FVector2D& Shooter, const FVector2D& Target, const FBox2D& BounceBox,
    EBounceWall Wall, float& OutBearingDeg)
{
    const FVector2D Mirrored = MirrorPointOverWall(Target, BounceBox, Wall);

    FVector2D CrossPoint;
    if (!FindWallCrossing(Shooter, Mirrored, BounceBox, Wall, CrossPoint))
    {
        return false;
    }

    OutBearingDeg = BearingDegXY(Shooter, Mirrored);
    return true;
}

bool PlanRicochetBearing(
    const FVector2D& Shooter, const FVector2D& Target, const FBox2D& BounceBox,
    int32 Bounces, FRandomStream& Rng, float& OutBearingDeg)
{
    EBounceWall AllWalls[4] = { EBounceWall::North, EBounceWall::South, EBounceWall::East, EBounceWall::West };
    ShuffleWallOrder(Rng, AllWalls);

    if (Bounces <= 1)
    {
        for (EBounceWall Wall : AllWalls)
        {
            if (PlanSingleBounceBearing(Shooter, Target, BounceBox, Wall, OutBearingDeg))
            {
                return true;
            }
        }
        return false;
    }

    // Two bounces: WallA is hit first, WallB second. Standard billiard
    // "unfolding" — mirror Target across WallB then WallA to get the straight
    // -line aim point, then validate both real-space crossings in sequence:
    // Shooter -> WallA (aiming at the twice-mirrored point), then from that
    // crossing -> WallB (aiming at the once-mirrored point).
    EBounceWall SecondOrder[4] = { EBounceWall::North, EBounceWall::South, EBounceWall::East, EBounceWall::West };
    ShuffleWallOrder(Rng, SecondOrder);

    for (EBounceWall WallA : AllWalls)
    {
        for (EBounceWall WallB : SecondOrder)
        {
            if (WallB == WallA) { continue; }

            const FVector2D OnceMirrored  = MirrorPointOverWall(Target, BounceBox, WallB);
            const FVector2D TwiceMirrored = MirrorPointOverWall(OnceMirrored, BounceBox, WallA);

            FVector2D CrossA;
            if (!FindWallCrossing(Shooter, TwiceMirrored, BounceBox, WallA, CrossA)) { continue; }

            FVector2D CrossB;
            if (!FindWallCrossing(CrossA, OnceMirrored, BounceBox, WallB, CrossB)) { continue; }

            OutBearingDeg = BearingDegXY(Shooter, TwiceMirrored);
            return true;
        }
    }
    return false;
}

float TimeToImpactSeconds(
    const FVector2D& BulletPos, const FVector2D& BulletVel,
    const FVector2D& Self, float CombinedRadius)
{
    const FVector2D D = BulletPos - Self;
    const float RadiusSq = CombinedRadius * CombinedRadius;

    if (D.SizeSquared() <= RadiusSq)
    {
        return 0.f; // already overlapping
    }

    const float A = FVector2D::DotProduct(BulletVel, BulletVel);
    if (FMath::IsNearlyZero(A))
    {
        return -1.f; // stationary and not already overlapping — never hits
    }

    const float B = 2.f * FVector2D::DotProduct(D, BulletVel);
    const float C = D.SizeSquared() - RadiusSq;

    const float Disc = B * B - 4.f * A * C;
    if (Disc < 0.f)
    {
        return -1.f; // trajectory never comes within CombinedRadius
    }

    const float SqrtDisc = FMath::Sqrt(Disc);
    const float T0 = (-B - SqrtDisc) / (2.f * A);
    const float T1 = (-B + SqrtDisc) / (2.f * A);

    if (T1 < 0.f)
    {
        return -1.f; // both roots in the past — it already went by
    }
    return FMath::Max(T0, 0.f);
}

float ApplyAimJitter(float BearingDeg, float StdDevDeg, FRandomStream& Rng)
{
    if (StdDevDeg <= 0.f)
    {
        return BearingDeg;
    }

    // Sum of 3 iid Uniform(-1,1) samples: Var(Uniform(-1,1)) = 1/3, so the sum
    // has variance 1 (std = 1) — an Irwin-Hall-style bounded stand-in for a
    // unit Gaussian that avoids the rare wild outliers a single raw sample
    // could produce, without pulling in a full normal-distribution sampler.
    const float Sum = Rng.FRandRange(-1.f, 1.f) + Rng.FRandRange(-1.f, 1.f) + Rng.FRandRange(-1.f, 1.f);
    return BearingDeg + Sum * StdDevDeg;
}

float HoldSecondsForChargeFrac(float Frac, float MinChargeSeconds, float MaxChargeSeconds)
{
    return FMath::Lerp(MinChargeSeconds, MaxChargeSeconds, FMath::Clamp(Frac, 0.f, 1.f));
}

} // namespace PartyDuelAI
