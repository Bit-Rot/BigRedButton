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

Test suite: `PartyButtons.Input.Dispatch.*`
- `BuildsSixteenDistinctActions` — 16 non-null distinct actions + 16-mapping IMC
- `RoutesEachButtonToItsIndex` — IndexOfByKey[k] == k for all 16; FInputActionInstance pathway
- `RejectsUnknownAction` — unknown/null action → INDEX_NONE

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

---

## Kill stuck editor processes (before rebuild)

A stuck editor holds the plugin DLL and causes `LNK1104` on the next build.

```bash
taskkill /F /IM UnrealEditor.exe /T 2>/dev/null; taskkill /F /IM UnrealEditor-Cmd.exe /T 2>/dev/null
```
