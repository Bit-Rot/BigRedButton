#pragma once

#include "CoreMinimal.h"
#include "OctoOdyssey/OctoTriggerVolume.h"
#include "OctoCheckpoint.generated.h"

class AOctoCheckpoint;
class UArrowComponent;

/** Broadcast when an AOctoPawn touches a checkpoint. Carries the checkpoint itself — the GameMode stores it. */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnOctoCheckpointReached, AOctoCheckpoint* /*Checkpoint*/);

/**
 * AOctoCheckpoint
 *
 * Placeable respawn point. Touching any of its shapes makes it the run's LATEST
 * checkpoint; dying afterwards puts the octopus back at its arrow rather than at
 * the course start.
 *
 * Two components, two different jobs, and they are deliberately independent:
 *
 *   Trigger  WHERE IT IS ARMED. One box by default, plus any extra boxes added
 *            per instance in the editor — see AOctoTriggerVolume. A checkpoint
 *            that only covers the main path is a checkpoint players skip, so
 *            being able to bolt a second box onto the ledge above it without
 *            touching code is the point.
 *
 *   Arrow    WHERE THE OCTOPUS COMES BACK. Drag it off the box to respawn
 *            players somewhere other than the middle of the trigger — above the
 *            plateau rather than inside the gate, past the hazard rather than
 *            back in front of it. This mirrors AOctoSpawnPoint exactly, which is
 *            the whole idea: a checkpoint IS a spawn point that arms itself.
 *
 * LATEST, NOT FURTHEST. Walking back through an earlier checkpoint makes that
 * one current again. This is what the game reads as: the checkpoint you last
 * touched is where you are, and it means a player who backtracks to try a
 * different line respawns where they are standing rather than being thrown
 * forward to a spot they have retreated from.
 *
 * The arrow's FACING is not read by gameplay — the octopus is pinned to the play
 * plane and never spawns rotated (AOctoGameMode::RespawnOcto spawns at a
 * location only). It is there to make the marker easy to grab and read in the
 * viewport, exactly as on AOctoSpawnPoint. The arrow's X is not read either: the
 * respawn is forced onto the course's play plane, see OctoRespawn::ResolveLocation.
 */
UCLASS()
class PARTYBUTTONS_API AOctoCheckpoint : public AOctoTriggerVolume
{
    GENERATED_BODY()

public:
    AOctoCheckpoint();

    FOnOctoCheckpointReached OnReached;

    /** World location of the Arrow — where a run that died after this checkpoint resumes. */
    FVector GetRespawnLocation() const;

protected:
    virtual void NotifyOctoTouched(AOctoPawn* Octo) override;

private:
    UPROPERTY(VisibleAnywhere, Category = "Octo")
    TObjectPtr<UArrowComponent> Arrow;
};
