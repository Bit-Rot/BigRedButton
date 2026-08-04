#include "OctoOdyssey/OctoCamera.h"
#include "OctoOdyssey/OctoPawn.h"
#include "Camera/CameraComponent.h"

AOctoCamera::AOctoCamera()
{
    PrimaryActorTick.bCanEverTick = true;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

    CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    CameraComponent->SetupAttachment(RootComponent);
    // Relative transform stays identity — FRotator::ZeroRotator looks along
    // +X, which IS the side-on view of the Y-Z play plane. Never rotated.
    CameraComponent->SetFieldOfView(CameraFieldOfView);
    CameraComponent->bConstrainAspectRatio = false;
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
        PlayPlaneX - CameraDistanceX,
        P.Y + LeadY,
        FMath::Max(P.Z + CameraHeightOffset, MinCameraZ));

    SetActorLocation(FMath::VInterpTo(GetActorLocation(), Goal, DeltaSeconds, FollowInterpSpeed));
}
