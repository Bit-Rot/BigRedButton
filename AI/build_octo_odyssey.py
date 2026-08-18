"""
build_octo_odyssey.py — headless Unreal Editor Python script.

Builds /Game/Maps/L_OctoOdyssey: the ONE level that now holds all of Octo
Odyssey — the main-menu island, the normal course and the hard course — as real,
editor-draggable actors.

This supersedes build_octo_course.py, which built the single course into
/Game/Maps/L_GameC back when Octo Odyssey was slot 2 of the 16-minigame party
session. That script is left in place and still works; L_GameC still exists and
now runs the generic minigame GameMode. See AOctoGameMode's class comment for
why the game left the session.

Everything the script inherits from build_octo_course.py it inherits for a
reason. Each of these cost a debugging session, so do not "simplify" them:

  - spawn_actor_from_CLASS, then assign the mesh. spawn_actor_from_object routes
    through UPlacementSubsystem, which in a -run=pythonscript commandlet has no
    registered asset factories and dereferences null (EXCEPTION_ACCESS_VIOLATION
    in FindAssetFactoryFromAssetData) on the very first block.
  - unreal.Rotator's Python constructor order is (roll, pitch, yaw), NOT
    FRotator's C++ (Pitch, Yaw, Roll). Always pass by keyword.
  - EditorLevelLibrary for level new/load/save, despite every function on it
    being UE_DEPRECATED(5.0). LevelEditorSubsystem's equivalents reach into the
    interactive Level Editor UI framework and crash (null-pointer access
    violation in EditorFramework.dll) when run headlessly with no Level Editor
    tab open. Actor and asset work uses the modern subsystems.

IDEMPOTENT BY REBUILD: every actor created here is tagged "OctoCourse", and each
run destroys every existing actor with that tag before spawning fresh ones. Safe
to re-run after editing the tables below — you get a clean rebuild, not
duplicates. The corollary is that hand-edits made in the editor to a tagged actor
are LOST on the next run: move the numbers here, not just the gizmo.

Prerequisite: the PartyButtons module must be compiled. This script spawns
unreal.OctoGoalFlag, unreal.OctoSpawnPoint, unreal.OctoViewPoint,
unreal.OctoCheckpoint, unreal.OctoKillVolume and unreal.OctoKillFloor, whose
Python bindings only exist once those UCLASS()-reflected C++ classes have built.

Run via:
  UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=<this file>
  -unattended -nopause -NoSplash -log

A note on logging, because it is not what you would expect: under
-run=pythonscript, unreal.log() and plain print() produce NOTHING in the log —
not with -log, not with -LogCmds="LogPython Log", not with global=verbose. Only
unreal.log_warning and unreal.log_error come through. So the lines that report
what this run actually DID go through log_warning on purpose (they are prefixed
[build_octo_odyssey] and are not warnings about anything), and unreal.log is kept
only for per-actor detail nobody reads. A level-build script whose successful run
is indistinguishable from a run that silently did nothing is not much of a
script — which is the state build_octo_course.py was quietly in.
"""

import unreal

MAP_PATH = "/Game/Maps/L_OctoOdyssey"
COURSE_TAG = "OctoCourse"

CUBE_MESH_PATH = "/Engine/BasicShapes/Cube.Cube"

# ---------------------------------------------------------------------------
# Where the three islands sit along X
#
# Each island is a Y-Z playfield on its own X plane, and its camera parks
# CAMERA_DISTANCE_X in FRONT of that plane (at island_x - CAMERA_DISTANCE_X)
# looking +X. That gives one hard constraint:
#
#     ISLAND_SPACING > CAMERA_DISTANCE_X + ISLAND_DEPTH_X
#
# Violate it and an island lands ON another island's camera — the camera ends up
# inside a ground slab and that course renders as a wall of grey. The originally
# sketched -2000 / -4000 spacing does exactly that: the normal course's camera
# sits at X = -2000, which would be the middle of a menu island placed there.
#
# 3000 clears it with room to spare. Retune freely, but keep the inequality:
#
#     Island       plane X      its camera X    sees
#     Normal            0            -2000      normal course, nothing in front
#     Main menu     -3000            -5000      menu island, NORMAL COURSE BEHIND
#     Hard          -6000            -8000      hard course, nothing in front
#
# These values are the STARTING POINT baked into the level. Once it is built,
# the spawn points, goal flags and the menu view point are all draggable actors —
# nudging those is the intended way to fine-tune, and nothing in C++ hard-codes a
# position. Re-running this script resets them to what is written here.
# ---------------------------------------------------------------------------

CAMERA_DISTANCE_X = 2000.0   # must match FOctoTuning::CameraDistanceX
ISLAND_DEPTH_X    = 525.0    # blocks span X in [-125, 400] — see the block table below
ISLAND_SPACING    = 3000.0

NORMAL_ISLAND_X = 0.0
MENU_ISLAND_X   = NORMAL_ISLAND_X - ISLAND_SPACING
HARD_ISLAND_X   = MENU_ISLAND_X - ISLAND_SPACING

assert ISLAND_SPACING > CAMERA_DISTANCE_X + ISLAND_DEPTH_X, (
    "Island spacing must clear the camera standoff plus the island's own depth, "
    "or one island's geometry will sit on another island's camera.")

# ---------------------------------------------------------------------------
# Block tables
#
# (label, location (x,y,z), rotation (pitch,yaw,roll), scale (sx,sy,sz))
# Scale is in metres — the engine Cube is a 100cm cube, so scale == size in metres.
# Locations are LOCAL to the island; the island's X is added at spawn time.
#
# Every playfield block is centred at local X = 137.5 with an X-scale of 5.25m,
# i.e. it spans local X in [-125, 400]. The back face is where it always was; the
# front face reaches forward past the play plane to catch SK_Okto's head sphere,
# which sits ~85cm out of the plane TOWARD the camera. That front overhang is
# DELIBERATE and it costs something: at contact points the ground draws over the
# front of the body. The escape hatch is FOctoTuning::HeadCollisionOffsetX — set
# it to ~85 and the head is tested in the play plane instead, at which point every
# block here can go back to (200.0, ..., 4.0, ...) and the occlusion goes away.
# Pick one; do not half-do both.
#
# Ground top surface is local Z = 0. Everything is on 100cm increments.
# Ascending ramps use negative Roll, descending use positive Roll (derived from
# FRotator(0,0,Roll) mapping +Z -> (0, sin Roll, cos Roll) — see OctoArmMath.h's
# header comment for the same convention applied to arms).
# ---------------------------------------------------------------------------

NORMAL_BLOCKS = [
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

# PLACEHOLDER. A harder variant of the same shapes: a wider pit with no floor to
# bail out onto, a steeper climb, a taller stair, and a longer run to the flag.
# It is deliberately built from the same vocabulary as the normal course so the
# difficulty difference reads as "the same game, turned up" rather than a
# different game — but it has had no playtesting at all, and tuning it is exactly
# the kind of look-at-it work this level is being built to enable.
HARD_BLOCKS = [
    ("H_Ground_A",        (137.5, 1200.0, -100.0), (0.0, 0.0, 0.0),   (5.25, 24.0, 2.0)),
    ("H_Ground_B",        (137.5, 5200.0, -100.0), (0.0, 0.0, 0.0),   (5.25, 20.0, 2.0)),
    ("H_Backdrop",        (500.0, 3200.0, 900.0),  (0.0, 0.0, 0.0),   (2.0, 68.0, 18.0)),
    ("H_StartWall",       (137.5, -100.0, 300.0),  (0.0, 0.0, 0.0),   (5.25, 2.0, 6.0)),
    ("H_EndWall",         (137.5, 6900.0, 300.0),  (0.0, 0.0, 0.0),   (5.25, 2.0, 6.0)),
    # Steeper than the normal course's 14 degrees, and twice as long.
    ("H_Ramp_Up22",       (137.5, 1900.0, 150.0),  (0.0, 0.0, -22.0), (5.25, 10.0, 0.5)),
    ("H_HighPlateau",     (137.5, 2700.0, 350.0),  (0.0, 0.0, 0.0),   (5.25, 6.0, 2.0)),
    # The gap. No pit floor to land on — falling in means falling past the level,
    # which is what the abandon-run hold on the main button is for.
    ("H_Ledge",           (137.5, 3600.0, 350.0),  (0.0, 0.0, 0.0),   (5.25, 3.0, 2.0)),
    ("H_Pillar_A",        (137.5, 4100.0, 200.0),  (0.0, 0.0, 0.0),   (2.0,  1.5, 8.0)),
    ("H_Pillar_B",        (137.5, 4500.0, 350.0),  (0.0, 0.0, 0.0),   (2.0,  1.5, 11.0)),
    ("H_Stair_1",         (137.5, 5000.0, 100.0),  (0.0, 0.0, 0.0),   (5.25, 2.0, 2.0)),
    ("H_Stair_2",         (137.5, 5300.0, 300.0),  (0.0, 0.0, 0.0),   (5.25, 2.0, 6.0)),
    ("H_Stair_3",         (137.5, 5600.0, 500.0),  (0.0, 0.0, 0.0),   (5.25, 2.0, 10.0)),
    ("H_Overhang",        (137.5, 6100.0, 750.0),  (0.0, 0.0, 0.0),   (5.25, 6.0, 1.0)),
    ("H_GoalPedestal",    (137.5, 6700.0, 350.0),  (0.0, 0.0, 0.0),   (5.25, 2.0, 8.0)),
]

# PLACEHOLDER. The menu island: somewhere for the fixed camera to look at, with
# the normal course visible in the distance behind it. Nothing is walked on here,
# so it is shaped for the SHOT, not for play — a flat plinth with a bit of
# silhouette so the frame is not just sky.
MENU_BLOCKS = [
    ("M_Ground",          (137.5, 3000.0, -100.0), (0.0, 0.0, 0.0),   (5.25, 24.0, 2.0)),
    ("M_Plinth",          (137.5, 3000.0, 100.0),  (0.0, 0.0, 0.0),   (4.0,  8.0, 2.0)),
    ("M_Pillar_L",        (137.5, 2100.0, 300.0),  (0.0, 0.0, 0.0),   (1.5,  1.5, 6.0)),
    ("M_Pillar_R",        (137.5, 3900.0, 300.0),  (0.0, 0.0, 0.0),   (1.5,  1.5, 6.0)),
    ("M_Arch",            (137.5, 3000.0, 620.0),  (0.0, 0.0, 0.0),   (1.5, 20.0, 0.4)),
]

# (label, local location, rotation) — pitch,yaw,roll.
NORMAL_SPAWN = ("OctoSpawn_Normal", (0.0, 300.0, 200.0), (0.0, 0.0, 0.0))
NORMAL_FLAG  = ("GoalFlag_Normal",  (0.0, 5900.0, 200.0), (0.0, 0.0, 0.0))

HARD_SPAWN = ("OctoSpawn_Hard", (0.0, 300.0, 200.0),  (0.0, 0.0, 0.0))
HARD_FLAG  = ("GoalFlag_Hard",  (0.0, 6700.0, 800.0), (0.0, 0.0, 0.0))

# ---------------------------------------------------------------------------
# Checkpoints and hazards
#
# (label, local location, half-extent). Local X is 0 — the same plane as the
# course's spawn point — and the X HALF-EXTENT is what covers the octopus's
# depth: SK_Okto's head sphere sits ~85cm out of the play plane toward the
# camera, and the blocks reach from local X -125 to 400, so 400 spans the lot.
#
# A checkpoint respawns at its ARROW, which these place at the actor origin.
# Every one below therefore sits a comfortable drop ABOVE the surface the player
# is expected to be standing on, not level with it — respawning inside a block
# is a physics ejection, not a respawn. Drag the arrow off the box in the editor
# to separate the two.
#
# CHECKPOINTS ARE LATEST-TOUCHED, NOT FURTHEST-REACHED (see AOctoCheckpoint), so
# spacing them is a design call: too dense and backtracking one metre undoes
# progress, too sparse and a death costs a minute of climbing.
# ---------------------------------------------------------------------------

NORMAL_CHECKPOINTS = [
    ("CP_Normal_Plateau", (0.0, 2500.0, 400.0), (400.0, 100.0, 500.0)),  # Plateau top is Z 200
    ("CP_Normal_Boxes",   (0.0, 4300.0, 500.0), (400.0, 100.0, 500.0)),  # Box_B top is Z 300
]

HARD_CHECKPOINTS = [
    ("CP_Hard_Plateau", (0.0, 2700.0, 650.0), (400.0, 100.0, 500.0)),  # H_HighPlateau top is Z 450
    ("CP_Hard_Ledge",   (0.0, 3600.0, 650.0), (400.0, 100.0, 500.0)),  # H_Ledge top is Z 450 — the far side of the gap
    ("CP_Hard_Stairs",  (0.0, 5000.0, 400.0), (400.0, 100.0, 600.0)),  # H_Stair_1 top is Z 200
]

# THESE CHANGE HOW THE COURSES PLAY, and both are one line to delete.
#
# The normal course's pit used to be a soft landing — its PitFloor is a metre
# below the ground line and you could climb back out. This makes it lethal, on
# the grounds that a pit you can sit in is not an obstacle. The floor is still
# there to land on; it just kills now.
#
# The hard course's gap already had no floor at all, so falling in meant a long
# drop to the kill floor. This shortens that to about half a second.
NORMAL_KILL_VOLUMES = [
    ("Kill_Normal_Pit", (0.0, 3400.0, -50.0), (400.0, 200.0, 50.0)),  # sits on PitFloor's Z -100 surface
]

HARD_KILL_VOLUMES = [
    ("Kill_Hard_Gap", (0.0, 3225.0, 0.0), (400.0, 225.0, 400.0)),  # spans Y 3000..3450, under both ledges
]

# (label, local location). KillZ is deliberately LEFT AT THE C++ DEFAULT
# (OctoRespawn::DefaultKillFloorZ, -10000) — this is the "fell out of the world"
# catcher, not a hazard, and leaving it unset here means changing that default in
# C++ actually reaches the level.
#
# The cost of that default is a ~4.5-second fall before the respawn, which is
# dead time on a running clock. If that becomes annoying, set kill_z on these to
# a few hundred below the lowest block (about -1500 for the normal course, -1000
# for the hard one) — but note the hazards above are what catch the two falls a
# player actually makes, so this only fires when the octopus leaves the level
# sideways.
#
# The actor's own POSITION means nothing to gameplay; it is placed under the
# start of each course purely so it is findable in the viewport.
NORMAL_KILL_FLOOR = ("KillFloor_Normal", (0.0, 300.0, -600.0))
HARD_KILL_FLOOR   = ("KillFloor_Hard",   (0.0, 300.0, -600.0))

# The menu camera. Local X is negative because the camera stands OFF the island,
# in front of it, looking back along +X — which is also the direction the normal
# course lies in, so the course fills the background. Identity rotation looks +X;
# see AOctoViewPoint's class comment.
MENU_VIEWPOINT = ("MenuCamera", (-CAMERA_DISTANCE_X, 3000.0, 400.0), (0.0, 0.0, 0.0), 60.0)

# (label, local location, rotation, intensity). One pair per island so a course
# three kilometres from the others is not lit edge-on.
LIGHTS = [
    ("KeyLight",  (0.0, 3000.0, 2000.0), (-50.0, 200.0, 0.0), 4.0),
    ("FillLight", (0.0, 3000.0, 2000.0), (-20.0, 20.0, 0.0),  1.0),
]


def make_rotator(pitch, yaw, roll):
    # unreal.Rotator's Python constructor order is (roll, pitch, yaw) —
    # NOT FRotator's C++ order (Pitch, Yaw, Roll). Always pass by keyword
    # here to avoid silently swapping axes.
    return unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw)


def offset(location, island_x):
    """Island-local coordinates -> world."""
    return (location[0] + island_x, location[1], location[2])


def clear_existing_course(actor_subsystem):
    existing = [a for a in actor_subsystem.get_all_level_actors() if a.actor_has_tag(COURSE_TAG)]
    if existing:
        actor_subsystem.destroy_actors(existing)
        unreal.log_warning(f"[build_octo_odyssey] Cleared {len(existing)} existing tagged actor(s).")


def spawn_block(actor_subsystem, cube_mesh, label, location, rotation, scale):
    # spawn_actor_from_CLASS, not spawn_actor_from_OBJECT — see the module docstring.
    actor = actor_subsystem.spawn_actor_from_class(
        unreal.StaticMeshActor, unreal.Vector(*location), make_rotator(*rotation))
    if not actor:
        unreal.log_error(f"[build_octo_odyssey] Failed to spawn block '{label}'.")
        return None

    mesh_component = actor.get_component_by_class(unreal.StaticMeshComponent)
    if not mesh_component:
        unreal.log_error(f"[build_octo_odyssey] Block '{label}' has no static mesh component.")
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
        unreal.log_error(f"[build_octo_odyssey] Failed to spawn '{label}'.")
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


def spawn_trigger_volume(actor_subsystem, actor_class, label, location, extent, course_enum):
    """
    One AOctoTriggerVolume subclass — a checkpoint or a kill volume.

    Setting trigger_extent goes through PostEditChangeProperty, which re-runs
    AOctoTriggerVolume::OnConstruction and pushes the new half-extent into the
    box. Without that round trip the actor would save with its class-default box
    and the number written above would be a lie.

    Extra colliders are an EDITOR-ONLY affordance by design (see
    AOctoTriggerVolume): if one of these needs a second box, add it in the
    Details panel — but remember this script destroys and rebuilds every tagged
    actor, so a hand-added component is lost on the next run just like a gizmo
    nudge is.
    """
    actor = spawn_class(actor_subsystem, actor_class, label, location, (0.0, 0.0, 0.0))
    if not actor:
        return None

    actor.set_editor_property("course", course_enum)
    actor.set_editor_property("trigger_extent", unreal.Vector(*extent))
    return actor


def build_course(actor_subsystem, cube_mesh, island_x, blocks, spawn, flag, course_enum, suffix,
                 checkpoints, kill_volumes, kill_floor):
    """One playable island: its geometry, its spawn point, its goal flag and its hazards."""
    spawned = 0

    for label, location, rotation, scale in blocks:
        if spawn_block(actor_subsystem, cube_mesh, label, offset(location, island_x), rotation, scale):
            spawned += 1

    # The spawn point's X IS the play plane for this course: AOctoGameMode reads
    # it straight through (no longer overwriting it with FOctoTuning::PlayPlaneX)
    # and hands the same number to the camera. Two courses on two planes in one
    # level is the whole reason that changed.
    label, location, rotation = spawn
    spawn_actor = spawn_class(actor_subsystem, unreal.OctoSpawnPoint, label, offset(location, island_x), rotation)
    if spawn_actor:
        spawn_actor.set_editor_property("course", course_enum)
        spawned += 1

    label, location, rotation = flag
    flag_actor = spawn_class(actor_subsystem, unreal.OctoGoalFlag, label, offset(location, island_x), rotation)
    if flag_actor:
        flag_actor.set_editor_property("course", course_enum)
        spawned += 1

    for label, location, extent in checkpoints:
        if spawn_trigger_volume(actor_subsystem, unreal.OctoCheckpoint, label,
                                offset(location, island_x), extent, course_enum):
            spawned += 1

    for label, location, extent in kill_volumes:
        if spawn_trigger_volume(actor_subsystem, unreal.OctoKillVolume, label,
                                offset(location, island_x), extent, course_enum):
            spawned += 1

    # kill_z is left at its C++ default on purpose — see the table above.
    label, location = kill_floor
    floor_actor = spawn_class(actor_subsystem, unreal.OctoKillFloor, label, offset(location, island_x), (0.0, 0.0, 0.0))
    if floor_actor:
        floor_actor.set_editor_property("course", course_enum)
        spawned += 1

    for label, location, rotation, intensity in LIGHTS:
        if spawn_light(actor_subsystem, f"{label}_{suffix}", offset(location, island_x), rotation, intensity):
            spawned += 1

    return spawned


def build_menu_island(actor_subsystem, cube_mesh, island_x):
    spawned = 0

    for label, location, rotation, scale in MENU_BLOCKS:
        if spawn_block(actor_subsystem, cube_mesh, label, offset(location, island_x), rotation, scale):
            spawned += 1

    label, location, rotation, fov = MENU_VIEWPOINT
    view_point = spawn_class(actor_subsystem, unreal.OctoViewPoint, label, offset(location, island_x), rotation)
    if view_point:
        view_point.set_editor_property("view_role", unreal.OctoViewRole.MAIN_MENU)
        view_point.set_editor_property("field_of_view", fov)
        spawned += 1

    for label, location, rotation, intensity in LIGHTS:
        if spawn_light(actor_subsystem, f"{label}_Menu", offset(location, island_x), rotation, intensity):
            spawned += 1

    return spawned


def set_default_game_mode():
    """
    Stamp AOctoGameMode onto the level's WorldSettings.

    Without this, opening L_OctoOdyssey directly falls back to
    GlobalDefaultGameMode SILENTLY — no error, no warning, just a level with the
    party session's GameMode and none of the menu (the same ?game= pitfall
    documented in PartyFlowRouter.h). AOctoHUD deliberately falls through to the
    party HUD when that happens so the symptom is visible instead of a blank
    screen, but the fix belongs here.

    GameplayStatics.get_all_actors_of_class, NOT
    EditorActorSubsystem.get_all_level_actors: WorldSettings does not appear in
    the latter's results at all (it is level infrastructure, not a placed actor),
    so searching there finds nothing and the GameMode silently never gets set —
    which is the exact failure this function exists to prevent.
    """
    editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = editor_subsystem.get_editor_world() if editor_subsystem else None
    if not world:
        unreal.log_error("[build_octo_odyssey] No editor world — cannot set the GameMode override.")
        return False

    found = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.WorldSettings)
    if not found:
        unreal.log_error(
            "[build_octo_odyssey] No WorldSettings actor found — the level will fall back to "
            "GlobalDefaultGameMode and show the party HUD instead of the Octo menu.")
        return False

    found[0].set_editor_property("default_game_mode", unreal.OctoGameMode)
    unreal.log_warning("[build_octo_odyssey] WorldSettings GameMode Override set to OctoGameMode.")
    return True


def main():
    asset_subsystem = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    # EditorLevelLibrary, not LevelEditorSubsystem — see module docstring.
    if asset_subsystem.does_asset_exist(MAP_PATH):
        if not unreal.EditorLevelLibrary.load_level(MAP_PATH):
            unreal.log_error(f"[build_octo_odyssey] Failed to load {MAP_PATH}.")
            return
    else:
        if not unreal.EditorLevelLibrary.new_level(MAP_PATH):
            unreal.log_error(f"[build_octo_odyssey] Failed to create {MAP_PATH}.")
            return
        unreal.log_warning(f"[build_octo_odyssey] Created {MAP_PATH}.")

    clear_existing_course(actor_subsystem)

    cube_mesh = unreal.load_asset(CUBE_MESH_PATH)
    if not cube_mesh:
        unreal.log_error(f"[build_octo_odyssey] Failed to load {CUBE_MESH_PATH}.")
        return

    spawned = 0
    spawned += build_course(actor_subsystem, cube_mesh, NORMAL_ISLAND_X,
                            NORMAL_BLOCKS, NORMAL_SPAWN, NORMAL_FLAG,
                            unreal.OctoCourse.NORMAL, "Normal",
                            NORMAL_CHECKPOINTS, NORMAL_KILL_VOLUMES, NORMAL_KILL_FLOOR)
    spawned += build_menu_island(actor_subsystem, cube_mesh, MENU_ISLAND_X)
    spawned += build_course(actor_subsystem, cube_mesh, HARD_ISLAND_X,
                            HARD_BLOCKS, HARD_SPAWN, HARD_FLAG,
                            unreal.OctoCourse.HARD, "Hard",
                            HARD_CHECKPOINTS, HARD_KILL_VOLUMES, HARD_KILL_FLOOR)

    # Not tagged, so the rebuild above never destroys it — WorldSettings is part
    # of the level itself, not part of the course.
    set_default_game_mode()

    if not unreal.EditorLevelLibrary.save_current_level():
        unreal.log_error(f"[build_octo_odyssey] Failed to save {MAP_PATH}.")
        return

    unreal.log_warning(
        f"[build_octo_odyssey] Done: spawned and saved {spawned} actor(s) into {MAP_PATH} "
        f"(menu X={MENU_ISLAND_X}, normal X={NORMAL_ISLAND_X}, hard X={HARD_ISLAND_X}).")


main()
