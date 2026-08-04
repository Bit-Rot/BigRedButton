#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OctoSpawnPoint.generated.h"

class UArrowComponent;

/**
 * AOctoSpawnPoint
 *
 * Pure placeable marker — drop one into L_GameC to mark where AOctoGameMode
 * spawns the octopus. No logic; AOctoGameMode::BeginPlay finds it via
 * TActorIterator and reads its location (with X forced to the play plane —
 * see AOctoGameMode's class comment for why that matters).
 *
 * The UArrowComponent exists purely so the marker is easy to grab and orient
 * in the editor viewport; the octopus itself never rotates off the X axis,
 * so the arrow's facing is not read by gameplay code.
 */
UCLASS()
class PARTYBUTTONS_API AOctoSpawnPoint : public AActor
{
    GENERATED_BODY()

public:
    AOctoSpawnPoint();

private:
    UPROPERTY(VisibleAnywhere, Category = "Octo")
    TObjectPtr<UArrowComponent> Arrow;
};
