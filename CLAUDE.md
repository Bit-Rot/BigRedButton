# PartyButtons — Repo Orientation for Claude Code

## What this is

A proof of concept for a 16-button arcade controller (Teensy 4.0 USB HID Joystick)
driving Unreal Enhanced Input, where each physical button gets a stable identity
(player/dispatch index). The handoff point is
`APartyInputController::HandleButtonPressed(int32 PlayerIndex)`.
Game logic past that point is out of scope *for the plugin*.

Above the plugin, the repo is becoming a **container of independent games** rather
than the single 16-minigame party session it started as. Each game owns its own
level, rules and scoring:

- **Party session** — `L_Main` → `L_MainMenu` → `L_Lobby` → `L_LevelSelect` →
  `L_GameA..P` → `L_Results`. Shared `APartyMinigameGameMode`, one winner per game.
- **OctoOdyssey** — `L_OctoOdyssey`, standalone. Own main menu, a normal and a hard
  course, and two persistent top-ten scoreboards. No map travel inside it: the
  three areas are one level and the camera blends between them. See
  `AOctoGameMode`'s class comment.

## Quick links

- Spec: `C:\Users\BitRot\Downloads\16-button-controller-unreal-spec.md`
- Architecture decisions: `AI/design/architecture.md`
- Build/test/run commands: `AI/reference/autonomous-validation.md`
- Hardware-gated steps: `AI/deferred-manual-work.md`

## Project layout

```
Game/                     UE 5.7 test project (PartyButtons.uproject)
  Config/DefaultInput.ini RawInput VID/PID config (placeholder PID — update with real device)
  Config/OctoTuning.ini   OctoOdyssey game-feel overrides (written by the Tab dev menu)
  Config/OctoScores.ini   OctoOdyssey top-ten tables (written on every name entry)
  Plugins/PartyInput/     THE REUSABLE PLUGIN — this is the deliverable
  Source/PartyButtons/    Demo glue: GameMode + HUD
    OctoOdyssey/          The OctoOdyssey game, Public/ + Private/ subfolder
Firmware/                 Teensy firmware (PartyButtons.ino)
AI/                       AI-generated docs, reference, and level-build scripts
  build_octo_odyssey.py   Builds L_OctoOdyssey (menu island + both courses)
Docs/                     Human-authored docs
Assets/                   Offline assets (models, images) for import into engine
```

## Level-build scripts

Levels are built by headless Python, not by hand in the editor. Re-run after
changing a block table:

```
"C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" \
  "C:/Users/BitRot/BigRedButton/Game/PartyButtons.uproject" \
  -run=pythonscript -script="C:/Users/BitRot/BigRedButton/AI/build_octo_odyssey.py" \
  -unattended -nopause -NoSplash -stdout -log 2>&1
```

Idempotent by rebuild (every actor is tagged `OctoCourse` and cleared first), so
**editor gizmo tweaks to a tagged actor are lost on the next run** — move the
numbers in the script. Note `unreal.log()` and `print()` are invisible under
`-run=pythonscript`; only `log_warning`/`log_error` reach the log.

## Engine

- **Version:** UE 5.7.4 at `C:/Program Files/Epic Games/UE_5.7/`
- **Toolchain:** MSVC v14.44+ (VS 2022 17.14+)
- **Key plugins:** EnhancedInput (engine, enabled by default), RawInput (Experimental, enabled in .uproject)

## Key design decision

Actions and the IMC are built **at runtime in C++** (`BuildButtonInputs()`), not as binary
`.uasset` files. This keeps the plugin fully autonomous (no editor GUI, no Python asset
scripts). Designer assets can be plugged in via `EditDefaultsOnly` UPROPERTY overrides.

## Build

```
"C:/Program Files/Epic Games/UE_5.7/Engine/Build/BatchFiles/Build.bat" \
  PartyButtonsEditor Win64 Development \
  -Project="C:/Users/BitRot/BigRedButton/Game/PartyButtons.uproject" -WaitMutex -FromMsBuild 2>&1
```

Timeout 600000ms. **Exit code 0 ≠ success** — always grep for `error C` lines.

## Test

```
"C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" \
  "C:/Users/BitRot/BigRedButton/Game/PartyButtons.uproject" \
  -ExecCmds="Automation RunTests PartyButtons.; Quit" \
  -unattended -nopause -nullrhi -NoSplash -stdout -FORCELOGFLUSH \
  -TestExit="Automation Test Queue Empty" \
  -ReportExportPath="C:/Users/BitRot/BigRedButton/Game/Saved/Automation/Reports" -log 2>&1
```

Pass condition: `Tests Failed: 0`, `succeeded > 0` in JSON report.

## Conventions (inherited from Anatidae)

- All modules use Public/Private split
- `bWarningsAsErrors = true`
- Test naming: `PartyButtons.<Subsystem>.<Category>.<TestName>`
- Log category: `LogPartyInput` (plugin), `LogPartyButtons` (game module)
- `EAutomationTestFlags::EditorContext | ProductFilter` for all tests
- Test source: `Private/Tests/`, gated on `#if WITH_DEV_AUTOMATION_TESTS`
