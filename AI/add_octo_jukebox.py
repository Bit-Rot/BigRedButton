"""
add_octo_jukebox.py — runs INSIDE the open Unreal editor.

  python AI/tools/ue_remote.py AI/add_octo_jukebox.py

Imports the Octo Odyssey music and places an AJukeBox in /Game/Maps/L_OctoOdyssey
under the World Outliner folder "Scene".

Deliberately NOT part of build_octo_odyssey.py. That script clears and rebuilds
every actor tagged "OctoCourse"; the jukebox is level dressing the user owns
(it lives in their Scene folder, next to hand-placed actors), so it is left
untagged and this script only ever touches the one jukebox it finds or makes.

Idempotent: re-running re-imports the WAV over the existing SoundWave (picking
up a re-edited loop) and rewrites the existing jukebox's track list instead of
spawning a second jukebox. The level edit is one undo transaction.

Saving: the map is saved only if it had no unsaved changes before this ran —
otherwise the user's own in-progress edits would be saved along with ours
without them choosing to. In that case it is left dirty with a note.

Reflection only — no unreal.JukeBox / unreal.JukeBoxTrack. When AJukeBox
arrives by Live Coding (the normal case: see AI/tools/ue_livecode.py) the class
is live but the editor's Python module has no wrapper for it until the next
restart. So the class is found by path, and tracks go in through the
AddTrack UFUNCTION via call_method rather than by building FJukeBoxTrack
structs, which Python cannot construct without a wrapper.

Paths are absolute: ue_remote.py sends this file as text, so __file__ is unset.
"""

import os
import unreal

REPO_ROOT = "C:/Users/BitRot/BigRedButton"
MAP_PATH = "/Game/Maps/L_OctoOdyssey"
OUTLINER_FOLDER = "Scene"
JUKEBOX_LABEL = "JukeBox"
JUKEBOX_CLASS_PATH = "/Script/PartyButtons.JukeBox"
MUSIC_SOUND_CLASS = "/Game/Audio/Mix/SC_Music"

# (track name, source WAV, content folder, asset name, starting volume)
# The track name is what game code keys on — AOctoGameMode::MusicTrackName.
TRACKS = [
    ("TwoLeftSocks",
     f"{REPO_ROOT}/Assets/Music/Tropical/2_BossaNova/congusbongus_TwoLeftSocks_LOOP_Seamless.wav",
     "/Game/OctoOdyssey/Audio/Music",
     "SW_TwoLeftSocks",
     # Matches AOctoGameMode::MenuMusicVolume. The game mode snaps to that on
     # BeginPlay anyway; this just makes the Details panel tell the same story.
     0.15),
]


def import_sound(source, folder, asset_name):
    if not os.path.isfile(source):
        unreal.log_error(f"[add_octo_jukebox] Missing source audio: {source}")
        return None

    task = unreal.AssetImportTask()
    task.filename = source
    task.destination_path = folder
    task.destination_name = asset_name
    task.automated = True
    task.replace_existing = True
    task.save = False  # saved below, after Looping is set
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    path = f"{folder}/{asset_name}"
    sound = unreal.load_asset(path)
    if not sound:
        unreal.log_error(f"[add_octo_jukebox] Import produced nothing at {path}.")
        return None

    # The loop lives on the asset — AJukeBox never forces it (see FJukeBoxTrack::Sound).
    sound.set_editor_property("looping", True)
    # Route through the mix tree AI/build_audio_assets.py builds, when it exists.
    if unreal.EditorAssetLibrary.does_asset_exist(MUSIC_SOUND_CLASS):
        sound.set_editor_property("sound_class_object", unreal.load_asset(MUSIC_SOUND_CLASS))
    unreal.EditorAssetLibrary.save_loaded_asset(sound, only_if_is_dirty=False)
    print(f"Imported {os.path.basename(source)} -> {path} (looping, {sound.get_editor_property('duration'):.2f}s).")
    return sound


def current_map_path():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    return world.get_outermost().get_name() if world else ""


def place_jukebox(jukebox_class, sounds):
    if current_map_path() != MAP_PATH:
        unreal.log_error(f"[add_octo_jukebox] Open {MAP_PATH} first (current: {current_map_path()}) — "
                         "not switching levels under the user.")
        return

    was_dirty = any(p.get_name() == MAP_PATH for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages())

    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    with unreal.ScopedEditorTransaction("Add Octo JukeBox"):
        existing = [a for a in actor_subsystem.get_all_level_actors() if a.get_class() == jukebox_class]
        if existing:
            jukebox = existing[0]
            if len(existing) > 1:
                unreal.log_warning(f"[add_octo_jukebox] {len(existing)} jukeboxes in the level — updating the "
                                   "first; AOctoGameMode only ever uses one.")
            print(f"Updating existing jukebox '{jukebox.get_actor_label()}'.")
        else:
            jukebox = actor_subsystem.spawn_actor_from_class(jukebox_class, unreal.Vector(0, 0, 0))
            if not jukebox:
                unreal.log_error("[add_octo_jukebox] Failed to spawn the jukebox.")
                return
            jukebox.set_actor_label(JUKEBOX_LABEL)
            print("Spawned a new jukebox.")

        jukebox.set_folder_path(OUTLINER_FOLDER)

        # Rewrite, not merge: TRACKS above is the whole list.
        jukebox.modify()
        jukebox.set_editor_property("tracks", [])
        for name, sound, volume in sounds:
            jukebox.call_method("AddTrack", (name, sound, volume, True))

    if was_dirty:
        unreal.log_warning(f"[add_octo_jukebox] {MAP_PATH} already had unsaved changes — jukebox added "
                           "but the map was NOT saved. Save it yourself.")
        return

    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    print(f"Done: jukebox in '{OUTLINER_FOLDER}' with tracks {[n for n, _, _ in sounds]}; {MAP_PATH} saved.")


def main():
    sounds = []
    for name, source, folder, asset_name, volume in TRACKS:
        sound = import_sound(source, folder, asset_name)
        if not sound:
            return
        sounds.append((name, sound, volume))

    jukebox_class = unreal.load_class(None, JUKEBOX_CLASS_PATH)
    if not jukebox_class:
        unreal.log_warning("[add_octo_jukebox] This editor has no AJukeBox — audio imported, jukebox NOT "
                           "placed. Run AI/tools/ue_livecode.py, then re-run this.")
        return

    place_jukebox(jukebox_class, sounds)


main()
