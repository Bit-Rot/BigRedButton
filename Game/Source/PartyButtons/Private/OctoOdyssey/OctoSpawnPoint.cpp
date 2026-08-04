#include "OctoOdyssey/OctoSpawnPoint.h"
#include "Components/ArrowComponent.h"

AOctoSpawnPoint::AOctoSpawnPoint()
{
    PrimaryActorTick.bCanEverTick = false;

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

    Arrow = CreateDefaultSubobject<UArrowComponent>(TEXT("Arrow"));
    Arrow->SetupAttachment(RootComponent);
    Arrow->SetHiddenInGame(true);
    Arrow->ArrowSize = 2.f;
    Arrow->ArrowColor = FColor(255, 200, 0);
}
