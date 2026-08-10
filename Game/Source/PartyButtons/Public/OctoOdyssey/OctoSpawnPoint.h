#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OctoOdyssey/OctoTypes.h"
#include "OctoSpawnPoint.generated.h"

class UArrowComponent;

/**
 * AOctoSpawnPoint
 *
 * Pure placeable marker — drop one per course into L_OctoOdyssey to mark where
 * AOctoGameMode spawns the octopus. No logic; AOctoGameMode::EnterCourse finds
 * the one whose Course matches the mode the players picked via TActorIterator and
 * reads its location (with X forced to the play plane — see AOctoGameMode's class
 * comment for why that matters).
 *
 * This IS the editor handle for where a course starts: drag it, save the level,
 * done. Nothing in code hard-codes a start position beyond a last-resort fallback.
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

    EOctoCourse GetCourse() const { return Course; }

protected:
    /**
     * Which course this marker starts. Both courses live in ONE level now, so
     * this — not the map name — is what tells them apart. See EOctoCourse.
     */
    UPROPERTY(EditAnywhere, Category = "Octo")
    EOctoCourse Course = EOctoCourse::Normal;

private:
    UPROPERTY(VisibleAnywhere, Category = "Octo")
    TObjectPtr<UArrowComponent> Arrow;
};
