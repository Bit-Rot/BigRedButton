#include "OctoOdyssey/OctoCamera.h"
#include "OctoOdyssey/OctoPawn.h"
#include "OctoOdyssey/OctoTuningSubsystem.h"
#include "Camera/CameraComponent.h"

AOctoCamera::AOctoCamera()
{
    PrimaryActorTick.bCanEverTick = true;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

    CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    CameraComponent->SetupAttachment(RootComponent);
    // Relative transform stays identity — FRotator::ZeroRotator looks along
    // +X, which IS the side-on view of the Y-Z play plane. Never rotated.
    CameraComponent->bConstrainAspectRatio = false;
    // FOV is deliberately NOT set here: the constructor runs long before any
    // tuning exists. BeginPlay applies it.
}

void AOctoCamera::BeginPlay()
{
    Super::BeginPlay();

    if (const UOctoTuningSubsystem* TuningSubsystem = UOctoTuningSubsystem::Get(this))
    {
        Tuning = TuningSubsystem->GetTuning();
    }

    ApplyLiveTuning(Tuning);
}

void AOctoCamera::ApplyLiveTuning(const FOctoTuning& NewTuning)
{
    Tuning = NewTuning;

    if (CameraComponent)
    {
        CameraComponent->SetFieldOfView(Tuning.CameraFieldOfView);
    }
}

void AOctoCamera::SetFollowTarget(AOctoPawn* Target)
{
    FollowTarget = Target;
}

void AOctoCamera::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    AOctoPawn* Target = FollowTarget.Get();
    if (!Target) { return; }

    const FVector P = Target->GetActorLocation();
    const FVector Goal(
        Tuning.PlayPlaneX - Tuning.CameraDistanceX,
        P.Y + Tuning.LeadY,
        FMath::Max(P.Z + Tuning.CameraHeightOffset, Tuning.MinCameraZ));

    SetActorLocation(FMath::VInterpTo(GetActorLocation(), Goal, DeltaSeconds, Tuning.FollowInterpSpeed));
}
