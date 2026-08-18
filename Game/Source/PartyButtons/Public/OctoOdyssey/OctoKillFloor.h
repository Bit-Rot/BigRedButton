#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OctoOdyssey/OctoRespawn.h"
#include "OctoOdyssey/OctoTypes.h"
#include "OctoKillFloor.generated.h"

class UArrowComponent;

/**
 * AOctoKillFloor
 *
 * The bottom of a course: an infinite horizontal plane at KillZ. The octopus
 * falling below it is killed and respawned at its last checkpoint.
 *
 * A PLANE, NOT A VOLUME — which is why this is not an AOctoTriggerVolume. An
 * overlap box has finite size, and the one thing this has to catch is exactly
 * the case where the octopus has left the level entirely: launched sideways off
 * a ledge, wedged out through a seam, dropped down the hard course's gap. A box
 * big enough to be sure of catching that is a box you have to keep resizing
 * every time the course grows. A Z test costs one comparison per frame and can
 * never be missed, no matter how fast the octopus is moving or how far off the
 * course it has gone — no sweep, no tunnelling.
 *
 * The consequence, which is the trade: nothing here detects anything. There is
 * no collision on this actor at all. AOctoGameMode reads KillZ off the placed
 * floor for the course being played and tests the octopus's Z in its own Tick.
 * So this actor's own POSITION is meaningless to gameplay — only KillZ is read.
 * It is placed somewhere visible purely so it can be found and edited in the
 * viewport, and the arrow points down at the plane it stands for.
 *
 * A course with no kill floor placed still has one: AOctoGameMode falls back to
 * OctoRespawn::DefaultKillFloorZ, so an octopus can never fall forever.
 */
UCLASS()
class PARTYBUTTONS_API AOctoKillFloor : public AActor
{
    GENERATED_BODY()

public:
    AOctoKillFloor();

    EOctoCourse GetCourse() const { return Course; }

    /** World Z of the kill plane. Below this is death. */
    float GetKillZ() const { return KillZ; }

protected:
    /** Which course this floor is the bottom of. See EOctoCourse. */
    UPROPERTY(EditAnywhere, Category = "Octo")
    EOctoCourse Course = EOctoCourse::Normal;

    /**
     * World Z of the kill plane — an ABSOLUTE height, not an offset from this
     * actor. Moving the actor does nothing; change this number.
     *
     * The default is far below any island on purpose: as shipped this is the
     * "fell out of the world" catcher and nothing else, so a course can be built
     * without thinking about it. Raise it to just under the lowest block to turn
     * a long fall into a quick one, which is usually what a course actually
     * wants — a four-second drop before the respawn is dead time.
     */
    UPROPERTY(EditAnywhere, Category = "Octo")
    float KillZ = OctoRespawn::DefaultKillFloorZ;

private:
    /**
     * Editor handle only, pointed straight down. Hidden in game like every other
     * marker arrow in this game — there is nothing to see here at runtime,
     * because the plane this actor stands for is nowhere near it.
     */
    UPROPERTY(VisibleAnywhere, Category = "Octo")
    TObjectPtr<UArrowComponent> Arrow;
};
