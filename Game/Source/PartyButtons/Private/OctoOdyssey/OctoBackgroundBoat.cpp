#include "OctoOdyssey/OctoBackgroundBoat.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "WaterBodyActor.h"
#include "WaterBodyComponent.h"

float OctoBoat::WrapDistance(float Distance, float SplineLength, bool bClosedLoop, bool& bOutBackward)
{
    bOutBackward = false;
    if (SplineLength <= KINDA_SMALL_NUMBER)
    {
        return 0.f;
    }

    if (bClosedLoop)
    {
        const float Wrapped = FMath::Fmod(Distance, SplineLength);
        return Wrapped < 0.f ? Wrapped + SplineLength : Wrapped;
    }

    // Open spline: ping-pong over a period of two lengths.
    const float Period = 2.f * SplineLength;
    float T = FMath::Fmod(Distance, Period);
    if (T < 0.f)
    {
        T += Period;
    }
    if (T > SplineLength)
    {
        bOutBackward = true;
        return Period - T;
    }
    return T;
}

AOctoBackgroundBoat::AOctoBackgroundBoat()
{
    PrimaryActorTick.bCanEverTick = true;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

    Path = CreateDefaultSubobject<USplineComponent>(TEXT("Path"));
    Path->SetupAttachment(RootComponent);
    // Default route: a ~30 m circle. Four auto-tangent points on a closed loop
    // is close enough to round for a boat on the horizon.
    const float Radius = 1500.f;
    Path->SetSplinePoints(
        { FVector(Radius, 0.f, 0.f), FVector(0.f, Radius, 0.f), FVector(-Radius, 0.f, 0.f), FVector(0.f, -Radius, 0.f) },
        ESplineCoordinateSpace::Local, false);
    Path->SetClosedLoop(true, true);

    BoatPivot = CreateDefaultSubobject<USceneComponent>(TEXT("BoatPivot"));
    BoatPivot->SetupAttachment(RootComponent);

    Hull = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Hull"));
    Hull->SetupAttachment(BoatPivot);
    Hull->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Hull->BodyInstance.bAutoWeld = false; // NoCollision alone doesn't stop autoweld — see AOctoPawn's class comment
    Hull->SetGenerateOverlapEvents(false);
}

void AOctoBackgroundBoat::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    // Re-seed and re-place on every edit so tuning in the details panel shows up
    // immediately at the start pose.
    Elapsed = 0.f;
    ResolvedWater.Reset();
    UpdateBoat(0.f, true);
}

void AOctoBackgroundBoat::BeginPlay()
{
    Super::BeginPlay();

    Elapsed = 0.f;
    ResolvedWater.Reset();
    UpdateBoat(0.f, true);
}

void AOctoBackgroundBoat::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    UpdateBoat(DeltaSeconds, false);
}

AWaterBody* AOctoBackgroundBoat::ResolveWaterBody()
{
    if (WaterBody)
    {
        return WaterBody;
    }
    if (ResolvedWater.IsValid())
    {
        return ResolvedWater.Get();
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return nullptr;
    }

    AWaterBody* Fallback = nullptr;
    for (TActorIterator<AWaterBody> It(World); It; ++It)
    {
        if (It->GetWaterBodyType() == EWaterBodyType::Ocean)
        {
            ResolvedWater = *It;
            return *It;
        }
        if (!Fallback)
        {
            Fallback = *It;
        }
    }
    ResolvedWater = Fallback;
    return Fallback;
}

float AOctoBackgroundBoat::SampleWaterZ(const FVector& WorldLocation, float FallbackZ) const
{
    const AWaterBody* Water = ResolvedWater.Get();
    const UWaterBodyComponent* WaterComp = Water ? Water->GetWaterBodyComponent() : nullptr;
    if (!WaterComp)
    {
        return FallbackZ;
    }

    const TValueOrError<FWaterBodyQueryResult, EWaterBodyQueryError> Result = WaterComp->TryQueryWaterInfoClosestToWorldLocation(
        WorldLocation, EWaterBodyQueryFlags::ComputeLocation | EWaterBodyQueryFlags::IncludeWaves);
    if (!Result.HasValue())
    {
        return FallbackZ;
    }

    const FWaterBodyQueryResult& Query = Result.GetValue();
    const float PlaneZ = Query.GetWaterPlaneLocation().Z;
    return PlaneZ + (Query.GetWaterSurfaceLocation().Z - PlaneZ) * WaveHeightScale;
}

void AOctoBackgroundBoat::UpdateBoat(float DeltaSeconds, bool bSnap)
{
    if (!Path || !BoatPivot)
    {
        return;
    }

    if (bSnap && bRandomizePhase)
    {
        // Name-seeded so the phases are stable across editor rebuilds and PIE.
        FRandomStream Stream(static_cast<int32>(GetTypeHash(GetFName())));
        PhaseBob = Stream.FRandRange(0.f, UE_TWO_PI);
        PhasePitch = Stream.FRandRange(0.f, UE_TWO_PI);
        PhaseRoll = Stream.FRandRange(0.f, UE_TWO_PI);
    }
    else if (bSnap)
    {
        PhaseBob = PhasePitch = PhaseRoll = 0.f;
    }

    Elapsed += DeltaSeconds;

    // ---- Position and heading along the spline -------------------------------
    const float Length = Path->GetSplineLength();
    const float Direction = bReverse ? -1.f : 1.f;
    const float Travelled = StartPhase * Length + Direction * Speed * Elapsed;

    bool bBackward = false;
    const float Dist = OctoBoat::WrapDistance(Travelled, Length, Path->IsClosedLoop(), bBackward);
    const float Heading = (bReverse != bBackward) ? -1.f : 1.f;

    const FVector PathLocation = Path->GetLocationAtDistanceAlongSpline(Dist, ESplineCoordinateSpace::World);
    FVector Forward = Path->GetDirectionAtDistanceAlongSpline(Dist, ESplineCoordinateSpace::World) * Heading;
    Forward.Z = 0.f;
    Forward = Forward.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
    const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
    const float Yaw = Forward.Rotation().Yaw;

    // Turn rate (deg/s) from the heading change over a short step ahead.
    constexpr float TurnProbe = 50.f;
    bool bUnused = false;
    const float AheadDist = OctoBoat::WrapDistance(Dist + TurnProbe * Heading, Length, Path->IsClosedLoop(), bUnused);
    const FVector AheadForward = Path->GetDirectionAtDistanceAlongSpline(AheadDist, ESplineCoordinateSpace::World) * Heading;
    const float YawAhead = FVector(AheadForward.X, AheadForward.Y, 0.f).Rotation().Yaw;
    const float TurnRate = FMath::FindDeltaAngleDegrees(Yaw, YawAhead) / TurnProbe * Speed;
    // Positive FRotator roll puts the right side down, and positive yaw rate is a right turn.
    const float Bank = FMath::Clamp(TurnRate * BankPerTurnRate, -MaxBank, MaxBank);

    // ---- Water ---------------------------------------------------------------
    float WaterZ = PathLocation.Z;
    float WavePitch = 0.f;
    float WaveRoll = 0.f;
    if (bFollowWater && ResolveWaterBody())
    {
        const FVector HalfL = Forward * (HullLength * 0.5f);
        const FVector HalfW = Right * (HullWidth * 0.5f);
        const float Fallback = PathLocation.Z;
        const float BowZ = SampleWaterZ(PathLocation + HalfL, Fallback);
        const float SternZ = SampleWaterZ(PathLocation - HalfL, Fallback);
        const float StbdZ = SampleWaterZ(PathLocation + HalfW, Fallback);
        const float PortZ = SampleWaterZ(PathLocation - HalfW, Fallback);

        WaterZ = 0.25f * (BowZ + SternZ + StbdZ + PortZ);
        // Nose up when the bow is higher; right side down when starboard is lower.
        WavePitch = FMath::RadiansToDegrees(FMath::Atan2(BowZ - SternZ, HullLength)) * WaveTiltScale;
        WaveRoll = FMath::RadiansToDegrees(FMath::Atan2(PortZ - StbdZ, HullWidth)) * WaveTiltScale;
    }

    // ---- Smooth toward the target, then layer the sway on top ----------------
    const float TargetZ = WaterZ + WaterLineOffset;
    const float TargetPitch = WavePitch;
    const float TargetRoll = WaveRoll + Bank;

    if (bSnap || HeightSmoothing <= 0.f)
    {
        CurrentZ = TargetZ;
    }
    else
    {
        CurrentZ = FMath::FInterpTo(CurrentZ, TargetZ, DeltaSeconds, HeightSmoothing);
    }

    if (bSnap || TiltSmoothing <= 0.f)
    {
        CurrentPitch = TargetPitch;
        CurrentRoll = TargetRoll;
    }
    else
    {
        CurrentPitch = FMath::FInterpTo(CurrentPitch, TargetPitch, DeltaSeconds, TiltSmoothing);
        CurrentRoll = FMath::FInterpTo(CurrentRoll, TargetRoll, DeltaSeconds, TiltSmoothing);
    }

    const float Bob = BobAmplitude * FMath::Sin(UE_TWO_PI * Elapsed / BobPeriod + PhaseBob);
    const float SwayPitch = PitchAmplitude * FMath::Sin(UE_TWO_PI * Elapsed / PitchPeriod + PhasePitch);
    const float SwayRoll = RollAmplitude * FMath::Sin(UE_TWO_PI * Elapsed / RollPeriod + PhaseRoll);

    const FVector Location(PathLocation.X, PathLocation.Y, CurrentZ + Bob);
    const FRotator Rotation(CurrentPitch + SwayPitch, Yaw, CurrentRoll + SwayRoll);
    BoatPivot->SetWorldLocationAndRotation(Location, Rotation);
}
