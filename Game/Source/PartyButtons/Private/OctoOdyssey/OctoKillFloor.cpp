#include "OctoOdyssey/OctoKillFloor.h"
#include "Components/ArrowComponent.h"

AOctoKillFloor::AOctoKillFloor()
{
    PrimaryActorTick.bCanEverTick = false; // AOctoGameMode does the Z test — see the class comment

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

    Arrow = CreateDefaultSubobject<UArrowComponent>(TEXT("Arrow"));
    Arrow->SetupAttachment(RootComponent);
    Arrow->SetHiddenInGame(true);
    Arrow->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f)); // an arrow's +X is its length, so pitch it to point down
    Arrow->ArrowSize = 3.f;
    Arrow->ArrowColor = FColor(220, 40, 40);
}
