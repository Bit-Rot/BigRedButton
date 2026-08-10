#include "OctoOdyssey/OctoViewPoint.h"
#include "Camera/CameraComponent.h"

AOctoViewPoint::AOctoViewPoint()
{
    PrimaryActorTick.bCanEverTick = false;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

    CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    CameraComponent->SetupAttachment(RootComponent);
    // Relative transform stays identity: the ACTOR's transform is the shot. That
    // keeps "drag it in the viewport" as the whole authoring story — a relative
    // offset here would mean the camera is not where the gizmo says it is.
    CameraComponent->bConstrainAspectRatio = false;
}

void AOctoViewPoint::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    // In OnConstruction rather than BeginPlay so the editor viewport's camera
    // preview shows the framing you are actually going to get.
    if (CameraComponent)
    {
        CameraComponent->SetFieldOfView(FieldOfView);
    }
}
