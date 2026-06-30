"""
bootstrap_map.py — headless Unreal Editor Python script.

Creates /Game/Maps/L_ButtonTest.umap (an empty level) and saves it.
Run once via:
  UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=<this file>
  -unattended -nopause -NoSplash -log

After running, L_ButtonTest will appear in Content/Maps/ and will be
loadable as the GameDefaultMap / EditorStartupMap set in DefaultEngine.ini.
"""

import unreal

MAP_PATH = "/Game/Maps/L_ButtonTest"

def main():
    # Check if the map already exists
    if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        unreal.log(f"[bootstrap_map] {MAP_PATH} already exists — skipping creation.")
        return

    # Create the Maps directory if it doesn't exist
    unreal.EditorAssetLibrary.make_directory("/Game/Maps")

    # Create and save an empty level
    # In UE5, EditorLevelLibrary.new_level creates a level at the given path
    result = unreal.EditorLevelLibrary.new_level(MAP_PATH)
    if result:
        unreal.log(f"[bootstrap_map] Created {MAP_PATH}")
        # Save current level
        unreal.EditorLevelLibrary.save_current_level()
        unreal.log(f"[bootstrap_map] Saved {MAP_PATH}")
    else:
        unreal.log_error(f"[bootstrap_map] Failed to create {MAP_PATH}")

main()
