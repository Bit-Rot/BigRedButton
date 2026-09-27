#include "AtrophyTileVolumeComponent.h"

UAtrophyTileVolumeComponent::UAtrophyTileVolumeComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetMobility(EComponentMobility::Static);
}
