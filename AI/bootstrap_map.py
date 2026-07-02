"""
bootstrap_map.py — headless Unreal Editor Python script.

Creates all PartyButtons maps as empty levels under /Game/Maps/ and saves them.
Safe to run multiple times: each map is skipped if it already exists.

Run via:
  UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=<this file>
  -unattended -nopause -NoSplash -log

Maps created:
  L_ButtonTest      — original debug HUD / input test map (kept for reference)
  L_Main            — startup: resets session and immediately redirects to MainMenu
  L_MainMenu        — Play / Settings menu
  L_Settings        — settings stub
  L_Lobby           — players join via countdown
  L_LevelSelect     — Roulette Rush level-picker
  L_Results         — end-of-session leaderboard
  L_GameA..L_GameP  — 16 minigame maps (one per game in the roster)

After running, update DefaultEngine.ini GameDefaultMap to /Game/Maps/L_Main
(already done; see Config/DefaultEngine.ini).
"""

import unreal

# All maps to create, in logical order.
# The 16 game maps (A–P) correspond to roster entries 0..15.
MAP_PATHS = [
    "/Game/Maps/L_ButtonTest",    # keep existing debug input-test map
    "/Game/Maps/L_Main",
    "/Game/Maps/L_MainMenu",
    "/Game/Maps/L_Settings",
    "/Game/Maps/L_Lobby",
    "/Game/Maps/L_LevelSelect",
    "/Game/Maps/L_Results",
] + [f"/Game/Maps/L_Game{chr(ord('A') + i)}" for i in range(16)]  # L_GameA..L_GameP


def main():
    unreal.EditorAssetLibrary.make_directory("/Game/Maps")

    created = 0
    skipped = 0

    for path in MAP_PATHS:
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            unreal.log(f"[bootstrap_map] {path} already exists — skipping.")
            skipped += 1
            continue

        # new_level() opens the newly-created level as the current level,
        # so save_current_level() must be called immediately inside the loop.
        result = unreal.EditorLevelLibrary.new_level(path)
        if result:
            unreal.EditorLevelLibrary.save_current_level()
            unreal.log(f"[bootstrap_map] Created and saved {path}")
            created += 1
        else:
            unreal.log_error(f"[bootstrap_map] Failed to create {path}")

    unreal.log(f"[bootstrap_map] Done: {created} created, {skipped} skipped, {len(MAP_PATHS)} total.")


main()
