#pragma once

#include "CoreMinimal.h"
#include "OctoOdyssey/OctoTriggerVolume.h"
#include "OctoKillVolume.generated.h"

/** Broadcast every time an AOctoPawn touches a kill volume. Carries the course it belongs to. */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnOctoKillVolumeTouched, EOctoCourse /*Course*/);

/**
 * AOctoKillVolume
 *
 * Placeable "touch this and die" hazard. Overlapping it with the octopus
 * broadcasts OnTouched, and AOctoGameMode respawns the run at its last
 * checkpoint (or at the course start if it has none).
 *
 * Deliberately NOT one-shot, unlike AOctoGoalFlag. A flag ends a run and so can
 * latch until the next EnterCourse re-arms it; a hazard has to work on the
 * second attempt at the same jump, and the fifth. The guard against a single
 * death firing twice lives in AOctoGameMode instead (its pending-respawn flag),
 * which is also where it has to live: several shapes on this actor, or several
 * volumes side by side, are one death between them and only the GameMode can
 * see that.
 *
 * A killed octopus respawns with the CLOCK STILL RUNNING — dying costs time, not
 * a run. See AOctoGameMode::RespawnOcto.
 *
 * See AOctoTriggerVolume for the shape rules, including the per-instance extra
 * colliders that let one hazard cover an awkward shape.
 */
UCLASS()
class PARTYBUTTONS_API AOctoKillVolume : public AOctoTriggerVolume
{
    GENERATED_BODY()

public:
    AOctoKillVolume();

    FOnOctoKillVolumeTouched OnTouched;

protected:
    virtual void NotifyOctoTouched(AOctoPawn* Octo) override;
};
