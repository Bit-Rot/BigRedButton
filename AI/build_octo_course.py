"""
build_octo_course.py — headless Unreal Editor Python script.

Places the OctoOdyssey greybox course (ground, ramps, boxes, goal flag, spawn
point, lights) into /Game/Maps/L_GameC as real, editor-draggable actors — the
same "drop it into the scene, playable at editor time" approach the project
uses everywhere else, extended from AI/bootstrap_map.py's empty-level
creation to actually filling one in.

Unlike bootstrap_map.py (skip-if-exists), this script is IDEMPOTENT BY
REBUILD: every actor it creates is tagged "OctoCourse", and each run first
destroys every existing actor with that tag before spawning fresh ones. This
means it's safe to re-run after tuning the layout below — you get a clean
rebuild, not duplicates.

Prerequisite: the PartyButtons module must already be compiled — this script
spawns unreal.OctoGoalFlag and unreal.OctoSpawnPoint, whose Python bindings
only exist once those UCLASS()-reflected C++ classes have built.

Uses the UE 5.7 editor subsystem APIs (EditorActorSubsystem,
EditorAssetSubsystem) for actor/asset work rather than the deprecated
EditorLevelLibrary that bootstrap_map.py used — every one of that library's
functions is UE_DEPRECATED(5.0). The one exception is level load/save:
LevelEditorSubsystem.load_level()/save_current_level() reach into the
interactive Level Editor UI framework and crash (null-pointer access
violation in EditorFramework.dll) when run headlessly via -run=pythonscript,
which has no Level Editor tab open. EditorLevelLibrary's older
new_level()/load_level()/save_current_level() go through
UEditorLoadingAndSavingUtils instead and work fine in this exact headless
context — proven by bootstrap_map.py already using them successfully — so
level load/save deliberately keeps using the deprecated API here.

Coordinate conventions (see Public/OctoOdyssey/OctoGameMode.h /
AOctoCamera's class comment):
  - The octopus is pinned to X = 0 (the "play plane"); the camera sits at
    X = -2000 looking +X. Playfield geometry spans X in [-125, 400].
  - That front face at -125 is DELIBERATE and it costs something. Geometry
    used to stop at X = 0, coplanar with the play plane, precisely so it
    could never occlude the octopus. But SK_Okto's head chain runs along
    -X: the head sphere sits ~85cm out of the play plane, toward the
    camera, spanning X in [-125, -45]. Against a course that stopped at 0
    the head swept through empty space and collided with nothing at all.
    Reaching it means bringing geometry forward past the octopus, so at
    contact points the ground now draws over the front of the body.
    The escape hatch is FOctoTuning::HeadCollisionOffsetX: set it to ~85
    and the head is tested in the play plane instead, at which point every
    block here can go back to (200.0, ..., 4.0, ...) and the occlusion
    goes away. Pick one; do not half-do both.
  - Ground top surface is Z = 0. Everything is on 100cm increments.
  - Ascending ramps use negative Roll, descending use positive Roll (derived
    from FRotator(0,0,Roll) mapping +Z -> (0, sin Roll, cos Roll) — see
    OctoArmMath.h's header comment for the same convention applied to arms).

Run via:
  UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=<this file>
  -unattended -nopause -NoSplash -log
"""

import unreal

MAP_PATH = "/Game/Maps/L_GameC"
COURSE_TAG = "OctoCourse"

CUBE_MESH_PATH = "/Engine/BasicShapes/Cube.Cube"

# (label, location (x,y,z), rotation (pitch,yaw,roll), scale (sx,sy,sz))
# Scale is in meters — the engine Cube is a 100cm cube, so scale == size in meters.
#
# Every playfield block is centred at X = 137.5 with an X-scale of 5.25m, i.e. it spans
# X in [-125, 400]: the back face is where it always was, and the front face reaches far
# enough forward to catch the head sphere (see the coordinate conventions above). The
# ramps are rolled about X, which leaves their X extent untouched. Backdrop is the one
# exception — at X = 500 it is behind everything and has no head to catch.
BLOCKS = [
    ("Ground_A",          (137.5, 1600.0, -100.0), (0.0, 0.0, 0.0),   (5.25, 32.0, 2.0)),
    ("PitFloor",          (137.5, 3400.0, -200.0), (0.0, 0.0, 0.0),   (5.25, 4.0, 2.0)),
    ("Ground_B",          (137.5, 4800.0, -100.0), (0.0, 0.0, 0.0),   (5.25, 24.0, 2.0)),
    ("Backdrop",          (500.0, 3000.0, 800.0),  (0.0, 0.0, 0.0),   (2.0, 60.0, 16.0)),
    ("StartWall",         (137.5, -100.0, 300.0),  (0.0, 0.0, 0.0),   (5.25, 2.0, 6.0)),
    ("EndWall",           (137.5, 6100.0, 300.0),  (0.0, 0.0, 0.0),   (5.25, 2.0, 6.0)),
    ("Step_1m",           (137.5, 1000.0, 50.0),   (0.0, 0.0, 0.0),   (5.25, 4.0, 1.0)),
    ("Ramp_Up14",         (137.5, 1900.0, 100.0),  (0.0, 0.0, -14.0), (5.25, 8.0, 0.5)),
    ("Plateau",           (137.5, 2500.0, 100.0),  (0.0, 0.0, 0.0),   (5.25, 6.0, 2.0)),
    ("Box_A",             (137.5, 4000.0, 50.0),   (0.0, 0.0, 0.0),   (5.25, 2.0, 1.0)),
    ("Box_B",             (137.5, 4300.0, 150.0),  (0.0, 0.0, 0.0),   (5.25, 2.0, 3.0)),
    ("Box_C",             (137.5, 4600.0, 250.0),  (0.0, 0.0, 0.0),   (5.25, 2.0, 5.0)),
    ("Ramp_Down14",       (137.5, 5100.0, 300.0),  (0.0, 0.0, 14.0),  (5.25, 6.0, 0.5)),
    ("Ceiling_Squeeze",   (137.5, 5500.0, 250.0),  (0.0, 0.0, 0.0),   (5.25, 4.0, 1.0)),
    ("GoalPedestal",      (137.5, 5900.0, 50.0),   (0.0, 0.0, 0.0),   (5.25, 2.0, 1.0)),
]

# (label, location, rotation) — pitch,yaw,roll.
GOAL_FLAG = ("GoalFlag", (0.0, 5900.0, 200.0), (0.0, 0.0, 0.0))
SPAWN_POINT = ("OctoSpawn", (0.0, 300.0, 200.0), (0.0, 0.0, 0.0))

# (label, location, rotation, intensity)
LIGHTS = [
    ("KeyLight",  (0.0, 3000.0, 2000.0), (-50.0, 200.0, 0.0), 4.0),
    ("FillLight", (0.0, 3000.0, 2000.0), (-20.0, 20.0, 0.0),  1.0),
]


def make_rotator(pitch, yaw, roll):
    # unreal.Rotator's Python constructor order is (roll, pitch, yaw) —
    # NOT FRotator's C++ order (Pitch, Yaw, Roll). Always pass by keyword
    # here to avoid silently swapping axes.
    return unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw)


def clear_existing_course(actor_subsystem):
    existing = [a for a in actor_subsystem.get_all_level_actors() if a.actor_has_tag(COURSE_TAG)]
    if existing:
        actor_subsystem.destroy_actors(existing)
        unreal.log(f"[build_octo_course] Cleared {len(existing)} existing tagged actor(s).")


def spawn_block(actor_subsystem, cube_mesh, label, location, rotation, scale):
    # spawn_actor_from_CLASS, not spawn_actor_from_OBJECT. Handing a mesh asset to
    # spawn_actor_from_object routes through UPlacementSubsystem, which in a
    # -run=pythonscript commandlet has no registered asset factories and dereferences
    # null: FindAssetFactoryFromAssetData, EXCEPTION_ACCESS_VIOLATION reading 0x40, hard
    # crash on the very first block. Spawning the actor class and assigning the mesh
    # afterwards reaches the same result without touching the placement path at all.
    # (spawn_actor_from_class is what spawn_class/spawn_light already use, and they
    # have always worked here — which is the clue.)
    actor = actor_subsystem.spawn_actor_from_class(
        unreal.StaticMeshActor, unreal.Vector(*location), make_rotator(*rotation))
    if not actor:
        unreal.log_error(f"[build_octo_course] Failed to spawn block '{label}'.")
        return None

    mesh_component = actor.get_component_by_class(unreal.StaticMeshComponent)
    if not mesh_component:
        unreal.log_error(f"[build_octo_course] Block '{label}' has no static mesh component.")
        return None
    mesh_component.set_static_mesh(cube_mesh)

    actor.set_actor_scale3d(unreal.Vector(*scale))
    actor.set_actor_label(label)
    actor.tags = [COURSE_TAG]
    return actor


def spawn_class(actor_subsystem, actor_class, label, location, rotation):
    actor = actor_subsystem.spawn_actor_from_class(
        actor_class, unreal.Vector(*location), make_rotator(*rotation))
    if not actor:
        unreal.log_error(f"[build_octo_course] Failed to spawn '{label}'.")
        return None
    actor.set_actor_label(label)
    actor.tags = [COURSE_TAG]
    return actor


def spawn_light(actor_subsystem, label, location, rotation, intensity):
    light = spawn_class(actor_subsystem, unreal.DirectionalLight, label, location, rotation)
    if not light:
        return None

    light_component = light.get_component_by_class(unreal.DirectionalLightComponent)
    if light_component:
        # Movable so the level needs no lighting build (same reasoning as
        # APartyArena::SunLight — see PartyArena.cpp).
        light_component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
        light_component.set_editor_property("intensity", intensity)
    return light


def main():
    asset_subsystem = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    if not asset_subsystem.does_asset_exist(MAP_PATH):
        unreal.log_error(
            f"[build_octo_course] {MAP_PATH} does not exist — run bootstrap_map.py first.")
        return

    # EditorLevelLibrary, not LevelEditorSubsystem — see module docstring.
    if not unreal.EditorLevelLibrary.load_level(MAP_PATH):
        unreal.log_error(f"[build_octo_course] Failed to load {MAP_PATH}.")
        return

    clear_existing_course(actor_subsystem)

    cube_mesh = unreal.load_asset(CUBE_MESH_PATH)
    if not cube_mesh:
        unreal.log_error(f"[build_octo_course] Failed to load {CUBE_MESH_PATH}.")
        return

    spawned = 0
    for label, location, rotation, scale in BLOCKS:
        if spawn_block(actor_subsystem, cube_mesh, label, location, rotation, scale):
            spawned += 1

    flag_label, flag_loc, flag_rot = GOAL_FLAG
    if spawn_class(actor_subsystem, unreal.OctoGoalFlag, flag_label, flag_loc, flag_rot):
        spawned += 1

    spawn_label, spawn_loc, spawn_rot = SPAWN_POINT
    if spawn_class(actor_subsystem, unreal.OctoSpawnPoint, spawn_label, spawn_loc, spawn_rot):
        spawned += 1

    for label, location, rotation, intensity in LIGHTS:
        if spawn_light(actor_subsystem, label, location, rotation, intensity):
            spawned += 1

    if not unreal.EditorLevelLibrary.save_current_level():
        unreal.log_error(f"[build_octo_course] Failed to save {MAP_PATH}.")
        return

    unreal.log(f"[build_octo_course] Done: spawned and saved {spawned} actor(s) into {MAP_PATH}.")


main()
