# Autonomous Validation — PartyButtons

All commands target UE 5.7.4 at `C:/Program Files/Epic Games/UE_5.7/`.

---

## 1. Generate project files

Run once after adding or renaming source files, or after first checkout:

```bash
"C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.exe" \
  -projectfiles \
  -project="C:/Users/BitRot/BigRedButton/Game/PartyButtons.uproject" \
  -game -rocket -progress
```

Expected: `Result: Succeeded`.

---

## 2. Build the editor target

```bash
"C:/Program Files/Epic Games/UE_5.7/Engine/Build/BatchFiles/Build.bat" \
  PartyButtonsEditor Win64 Development \
  -Project="C:/Users/BitRot/BigRedButton/Game/PartyButtons.uproject" \
  -WaitMutex -FromMsBuild 2>&1 \
  | grep -E "error C|error:|fatal error|Build FAILED|Total execution|Compile \[x64\]|Link \[x64\]|Result:"
```

Run via Bash tool with `timeout: 600000`.

**Important:** UBT exit code 0 does NOT guarantee a clean build. Always grep for
`error C` lines. A clean build shows `Result: Succeeded` with no `error C` lines.

---

## 3. Run automation tests (headless, no hardware)

```bash
"C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" \
  "C:/Users/BitRot/BigRedButton/Game/PartyButtons.uproject" \
  -ExecCmds="Automation RunTests PartyButtons.; Quit" \
  -unattended -nopause -nullrhi -NoSplash -stdout -FORCELOGFLUSH \
  -TestExit="Automation Test Queue Empty" \
  -ReportExportPath="C:/Users/BitRot/BigRedButton/Game/Saved/Automation/Reports" \
  -log 2>&1 \
  | grep -E "LogAutomationController|Test Completed|Tests Passed|Tests Failed|FAIL|Error:"
```

Pass condition: `Tests Failed: 0` and `succeeded > 0` in the JSON report at
`Game/Saved/Automation/Reports/index.json`.

Test suites (non-exhaustive — grep the report for the full list):
- `PartyButtons.Input.Dispatch.*` — 16 non-null distinct actions + 16-mapping
  IMC, IndexOfByKey[k] == k routing, unknown-action rejection
- `PartyButtons.Input.MainButton.*` — button-17 invariant, mappings, tap/hold
- `PartyButtons.Session.*`, `PartyButtons.Duel.*`, `PartyButtons.Flow.*` — game-module unit tests
- `PartyButtons.Octo.ArmMath.*` (11 tests) — OctoOdyssey's pure arm-geometry
  math: direction/spacing, the engine-Roll-convention pin
  (`ArmDirectionWorldMatchesRollRotation`), extension invariants, launch impulse
- `PartyButtons.Octo.Roster.SlotTwoIsOctoOdyssey` — roster slot 2 wiring guard

**Known pre-existing failure (unrelated to any of the above):**
`PartyButtons.Input.Dispatch.MapsKeyboardEmulationKeys`
(`PartyInputDispatchTest.cpp`) asserts a stale keyboard-emulation layout that
no longer matches `PartyInputController.cpp`'s shipping table. If this is the
ONLY failure in the report, that's expected — not a regression from your change.

---

## 4. Bootstrap the demo map (one-time, headless Python)

Creates `Content/Maps/L_ButtonTest.umap` using the UE editor Python scripting:

```bash
"C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" \
  "C:/Users/BitRot/BigRedButton/Game/PartyButtons.uproject" \
  -run=pythonscript \
  -script="C:/Users/BitRot/BigRedButton/AI/bootstrap_map.py" \
  -unattended -nopause -NoSplash -log 2>&1 \
  | grep -E "error|Error|warning|saved|L_ButtonTest"
```

Run once; the map is a binary `.umap` asset. Re-run only if the map is deleted.

---

## 4b. Build the OctoOdyssey course (headless Python, re-runnable)

Places the L_GameC greybox course (blocks, goal flag, spawn point, lights) —
requires the PartyButtons module to already be built (spawns
`unreal.OctoGoalFlag`/`unreal.OctoSpawnPoint`, only bound once those classes
compile). Idempotent by rebuild: every run clears its own tagged actors
first, so it's safe to re-run after tuning `AI/build_octo_course.py`'s
`BLOCKS`/`LIGHTS` tables.

```bash
"C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" \
  "C:/Users/BitRot/BigRedButton/Game/PartyButtons.uproject" \
  -run=pythonscript \
  -script="C:/Users/BitRot/BigRedButton/AI/build_octo_course.py" \
  -unattended -nopause -NoSplash -log 2>&1 \
  | grep -E "error|Error|warning|saved|build_octo_course"
```

If this fails with an unknown-commandlet/module error, `PythonScriptPlugin`
may need adding to `PartyButtons.uproject`'s `Plugins` array (it is
`EnabledByDefault: false` at the engine level; `bootstrap_map.py` working
implies it's already active for this project, but check first).

---

## 5. Headless game launch (verify demo loads, no hardware needed)

```bash
"C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor.exe" \
  "C:/Users/BitRot/BigRedButton/Game/PartyButtons.uproject" \
  -game -log -windowed -ResX=1280 -ResY=720 -NoSplash \
  -unattended 2>&1 | head -100
```

Or use the run-and-grep pattern for unattended verification:
Look for `LogPartyInput` lines and `LogPartyButtons` startup — confirms GameMode +
HUD wired correctly.

### 5b. Headless launch straight into L_GameC (OctoOdyssey)

Bypasses the whole menu/lobby flow by passing the map + `?game=` option
directly on the command line — the same mechanism `AOctoGameMode::ReloadCourse`
uses internally:

```bash
"C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor.exe" \
  "C:/Users/BitRot/BigRedButton/Game/PartyButtons.uproject" \
  "L_GameC?game=/Script/PartyButtons.OctoGameMode" \
  -game -log -windowed -ResX=1280 -ResY=720 -NoSplash -unattended 2>&1 | head -150
```

Expect a `LogPartyButtons: ... running game 2 — Octo Odyssey.` line and no
`could not find roster entry` warning — that warning's absence confirms the
`?game=` class path string is correct (getting it wrong silently falls back
to `GlobalDefaultGameMode` with no error — see `PartyFlowRouter.h`).

---

## Kill stuck editor processes (before rebuild)

A stuck editor holds the plugin DLL and causes `LNK1104` on the next build.

```bash
taskkill /F /IM UnrealEditor.exe /T 2>/dev/null; taskkill /F /IM UnrealEditor-Cmd.exe /T 2>/dev/null
```
