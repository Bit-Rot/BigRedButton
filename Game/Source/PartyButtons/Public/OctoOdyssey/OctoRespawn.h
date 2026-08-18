#pragma once

#include "CoreMinimal.h"

/**
 * OctoRespawn — pure, world-free rules for where a killed octopus comes back.
 *
 * Same shape as OctoArm (OctoArmMath.h) and OctoBody (OctoBodySpring.h): free
 * functions with no engine-object dependency, so the two decisions that actually
 * matter — "checkpoint or course start?" and "is it past the floor?" — can be
 * unit-tested without a world, a pawn or a placed volume. AOctoGameMode is the
 * only caller.
 *
 * There is very little here on purpose. Everything else about dying (which
 * volume fired, which checkpoint was last touched, when the pawn is safe to
 * destroy) needs a world and lives in AOctoGameMode.
 */
namespace OctoRespawn
{
    /**
     * The floor under the whole game, used when a course has no AOctoKillFloor
     * placed. Deliberately far below any island rather than just below the
     * lowest block: this is the "fell out of the world and is never coming
     * back" catcher, not a hazard. A course that wants a hazard height places
     * an AOctoKillFloor and sets its own.
     */
    inline constexpr float DefaultKillFloorZ = -10000.f;

    /**
     * Where the octopus respawns.
     *
     * X is forced to PlayPlaneX and is NOT taken from the checkpoint. The play
     * plane is the one number the pawn and the camera share (see
     * AOctoCamera::SetPlayPlaneX), so a checkpoint nudged off it in the editor
     * would otherwise respawn the octopus somewhere the camera does not frame —
     * the exact desync AOctoGameMode::FindSpawnLocation exists to prevent for
     * the course start. Checkpoints get the same treatment for the same reason.
     */
    inline FVector ResolveLocation(
        const FVector& CourseStart,
        const FVector& CheckpointLocation,
        bool           bHasCheckpoint,
        float          PlayPlaneX)
    {
        FVector Location = bHasCheckpoint ? CheckpointLocation : CourseStart;
        Location.X = PlayPlaneX;
        return Location;
    }

    /**
     * Has the octopus fallen past the kill plane?
     *
     * Strictly below, so a kill floor placed exactly at a block's surface does
     * not kill something resting on it.
     */
    inline bool IsBelowKillFloor(const FVector& Location, float KillFloorZ)
    {
        return Location.Z < KillFloorZ;
    }
}
