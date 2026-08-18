#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "OctoOdyssey/OctoRespawn.h"

// --------------------------------------------------------------------------
// PartyButtons.Octo.Respawn.*
//
// The two world-free decisions behind dying: where the octopus comes back, and
// whether it has fallen out of the world. Everything else about a death (which
// volume fired, when the pawn is safe to destroy, the grace window) needs a
// world and is tested by playing the game.
// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOctoRespawnFallsBackToCourseStart,
    "PartyButtons.Octo.Respawn.FallsBackToCourseStart",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOctoRespawnFallsBackToCourseStart::RunTest(const FString& Parameters)
{
    // A run that has touched no checkpoint restarts where it began.
    const FVector Start(-3000.0, 300.0, 200.0);

    const FVector Location = OctoRespawn::ResolveLocation(
        Start, FVector(999.0, 999.0, 999.0), /*bHasCheckpoint=*/false, Start.X);

    TestTrue(FString::Printf(TEXT("No checkpoint respawns at the course start (%s)"), *Location.ToString()),
        Location.Equals(Start, 0.001));

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOctoRespawnPrefersTheCheckpoint,
    "PartyButtons.Octo.Respawn.PrefersTheCheckpoint",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOctoRespawnPrefersTheCheckpoint::RunTest(const FString& Parameters)
{
    const FVector Start(0.0, 300.0, 200.0);
    const FVector Checkpoint(0.0, 4300.0, 500.0);

    const FVector Location = OctoRespawn::ResolveLocation(Start, Checkpoint, /*bHasCheckpoint=*/true, Start.X);

    TestTrue(FString::Printf(TEXT("A touched checkpoint wins over the start (%s)"), *Location.ToString()),
        Location.Equals(Checkpoint, 0.001));

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOctoRespawnPinsToThePlayPlane,
    "PartyButtons.Octo.Respawn.PinsToThePlayPlane",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOctoRespawnPinsToThePlayPlane::RunTest(const FString& Parameters)
{
    // The regression guard that matters. Three islands share one level, and the
    // camera frames ONE X (AOctoCamera::SetPlayPlaneX). A checkpoint nudged off
    // that plane in the editor — trivially easy with a free-move gizmo — must not
    // be able to respawn the octopus somewhere the camera is not looking.
    constexpr double PlaneX = -6000.0;

    const FVector Start(PlaneX, 300.0, 200.0);
    const FVector DraggedCheckpoint(PlaneX + 812.0, 3600.0, 550.0);

    const FVector Location = OctoRespawn::ResolveLocation(
        Start, DraggedCheckpoint, /*bHasCheckpoint=*/true, static_cast<float>(PlaneX));

    TestTrue(FString::Printf(TEXT("X is forced back onto the play plane (%.3f)"), Location.X),
        FMath::IsNearlyEqual(Location.X, PlaneX, 0.001));

    TestTrue(TEXT("...and Y/Z still come from the checkpoint"),
        FMath::IsNearlyEqual(Location.Y, DraggedCheckpoint.Y, 0.001) &&
        FMath::IsNearlyEqual(Location.Z, DraggedCheckpoint.Z, 0.001));

    // The same must hold for the fallback, whose own X is the plane already —
    // so this one is a no-op that would only fail if the pin were skipped for it.
    const FVector Fallback = OctoRespawn::ResolveLocation(
        FVector(PlaneX + 40.0, 300.0, 200.0), FVector::ZeroVector, /*bHasCheckpoint=*/false,
        static_cast<float>(PlaneX));

    TestTrue(FString::Printf(TEXT("The course start is pinned too (%.3f)"), Fallback.X),
        FMath::IsNearlyEqual(Fallback.X, PlaneX, 0.001));

    return true;
}

// --------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOctoRespawnKillFloorIsStrictlyBelow,
    "PartyButtons.Octo.Respawn.KillFloorIsStrictlyBelow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOctoRespawnKillFloorIsStrictlyBelow::RunTest(const FString& Parameters)
{
    constexpr float FloorZ = -500.f;

    TestFalse(TEXT("Well above the floor is alive"),
        OctoRespawn::IsBelowKillFloor(FVector(0.0, 0.0, 200.0), FloorZ));

    // Strict, so a floor placed level with a block's surface does not kill the
    // octopus sitting on it — an off-by-one here is a course that kills you on
    // spawn and looks like a physics bug.
    TestFalse(TEXT("Exactly on the floor is alive"),
        OctoRespawn::IsBelowKillFloor(FVector(0.0, 0.0, FloorZ), FloorZ));

    TestTrue(TEXT("A hair below the floor is dead"),
        OctoRespawn::IsBelowKillFloor(FVector(0.0, 0.0, FloorZ - 0.01), FloorZ));

    // Nothing but Z is consulted — the plane is infinite, which is the whole
    // reason AOctoKillFloor is a number and not a volume.
    TestTrue(TEXT("An octopus thrown miles sideways still falls past it"),
        OctoRespawn::IsBelowKillFloor(FVector(1.0e6, -4.0e6, FloorZ - 1.0), FloorZ));

    // The shipped default catches an unattended fall in a course with no floor
    // placed, and is far enough down that no island can reach it.
    TestTrue(TEXT("The default floor is well below any island"),
        OctoRespawn::DefaultKillFloorZ < -5000.f);
    TestTrue(TEXT("...and a fall reaches it"),
        OctoRespawn::IsBelowKillFloor(FVector(0.0, 0.0, -20000.0), OctoRespawn::DefaultKillFloorZ));

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
