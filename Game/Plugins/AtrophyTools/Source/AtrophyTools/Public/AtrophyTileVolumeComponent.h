#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "AtrophyTileVolumeComponent.generated.h"

/**
 * UAtrophyTileVolumeComponent
 *
 * Root component of AAtrophyTileMeshActor. It carries no data of its own: the
 * tiling parameters live on the actor. It exists so the editor module has a
 * component class to register FAtrophyTileVolumeVisualizer against (component
 * visualizers are keyed by component class), which is what draws the volume
 * outline and the per-face drag handles when the actor is selected.
 */
UCLASS(ClassGroup = (Atrophy), meta = (BlueprintSpawnableComponent = "false"))
class ATROPHYTOOLS_API UAtrophyTileVolumeComponent : public USceneComponent
{
    GENERATED_BODY()

public:
    UAtrophyTileVolumeComponent();
};
