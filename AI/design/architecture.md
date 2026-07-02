# PartyInput Plugin — Architecture

## Goal

Route 16 physical USB HID buttons through Unreal Enhanced Input to a single
indexed dispatch point: `APartyInputController::HandleButtonPressed(int32 PlayerIndex)`.
Game logic past that point is out of scope.

---

## Key design decision: runtime-built input objects

The spec (Component B2/B3) proposes hand-authored `.uasset` files for the 16
`IA_Button*` Input Actions and one `IMC_Buttons` Input Mapping Context.

**We instead build these objects at runtime in C++**, in
`APartyInputController::BuildButtonInputs()`.

### Why

| Reason | Detail |
|--------|--------|
| **Fully autonomous** | No binary `.uasset` files means no editor Python, no hand-click asset creation, no content browser. The whole plugin builds + tests headlessly from source. |
| **Identical behaviour** | The spec notes that stable button identity comes from *array order* (`element 0 = player 1`). Runtime-built objects preserve this: `ButtonActions[k]` is always player k. |
| **Testable** | Automation tests call `BuildButtonInputs()` and use `IndexOfByKey` to verify dispatch — no world, no actor, no subsystem needed. |
| **Designer override path preserved** | `ButtonMappingContext` and `ButtonActions` are `EditDefaultsOnly UPROPERTY`s. A designer can assign real assets; `BuildButtonInputs()` skips if already populated. |

### What gets built

```
for k in 0..15:
    ButtonActions[k] = NewObject<UInputAction>(RF_Transient)
    ButtonActions[k].ValueType = EInputActionValueType::Boolean

ButtonMappingContext = NewObject<UInputMappingContext>(RF_Transient)
for k in 0..15:
    ButtonMappingContext.MapKey(ButtonActions[k], FKey("GenericUSBController_Button{k+1}"))
```

All objects are `RF_Transient` — they live only in memory, never saved to disk.

---

## Player model

One `PlayerController`, one device, 16 buttons. `PlayerIndex` is a dispatch
index (0 = button 1 = "player 1 pressed"), **not** a `ULocalPlayer`.
Do not create 16 `ULocalPlayer`s. Enhanced Input is built for one device per
local player; with one device exposing 16 buttons, the correct pattern is one
controller that fans out by index.

---

## Data flow

```
Physical button N pressed
  → Teensy firmware: Joystick.button(N, 1)   [USB HID, ~1 ms latency]
  → Windows: HID joystick button N
  → Unreal RawInput plugin: maps to GenericUSBController_ButtonN key
  → Enhanced Input: fires IA_Button(N-1) (0-indexed in array) Started event
  → APartyInputController::OnAnyButton(FInputActionInstance)
  → ResolveActionIndex(Instance.GetSourceAction())   → k  (0-based)
  → HandleButtonPressed(k)
  → OnButtonPressed.Broadcast(k)   → HUD, GameMode, ...
```

---

## File map

| File | Role |
|------|------|
| `Plugins/PartyInput/Source/PartyInput/Public/PartyInputController.h` | Public interface, delegate declaration, handoff point |
| `Plugins/PartyInput/Source/PartyInput/Private/PartyInputController.cpp` | BuildButtonInputs, BeginPlay, SetupInputComponent, OnAnyButton, HandleButtonPressed |
| `Plugins/PartyInput/Source/PartyInput/Private/Tests/PartyInputDispatchTest.cpp` | Headless automation tests, no hardware required |
| `Game/Config/DefaultInput.ini` | RawInput VID/PID → GenericUSBController_Button1..16 mapping |
| `Firmware/PartyButtons.ino` | Teensy firmware (Component A) |

---

## What the game owner wires up

1. Set `APartyButtonsGameMode` (or their own GameMode) to use `APartyInputController`.
2. Optionally subclass `APartyInputController` and override `HandleButtonPressed`.
3. Or bind to `OnButtonPressed` from the HUD / GameMode.
4. Connect the Teensy **before** launching (stock RawInput has no hotplug).
5. Fill in the real `ProductID` in `DefaultInput.ini` (see `AI/deferred-manual-work.md`).

---

## Game flow & player model (extended — added with map-flow scaffold)

### Load-bearing game concepts (encode these, never violate them)

- **16 big red arcade buttons = 16 players.** Every physical button is permanently
  assigned to one player (button N = player N). `PlayerIndex` is 0-based.
- **One PlayerController, fan-out by index.** Never create 16 `ULocalPlayer`s.
  `PlayerIndex` is a dispatch index, not an engine local player.
- **A 17th "main button"** (the host/MC control) sits outside the 16-player set.
  Dev key: **Enter**. Physical: `GenericUSBController_Button17` (requires Teensy firmware update).
  Tap = activate/confirm selected menu option. Long-press (≥0.6s) = go back.
- **"Player registered"** means the player pressed their button during the Lobby
  countdown window. Registration is per-session and cleared on reset.

### Phase map flow

```
L_Main (startup)
  → L_MainMenu  (Play / Settings)
       → L_Settings (stub)
       → L_Lobby (players join via 1s countdown)
            → L_LevelSelect (Roulette Rush — picks the next minigame)
                 → L_GameA..L_GameP (minigame map)
                      → back to L_LevelSelect (or L_Results if session complete)
  → L_Results (leaderboard)
       → L_MainMenu
```

Session ends when `GamesPlayed >= GamesPerSession` (default 10, configurable).

### Architecture layers

| Layer | Component | Survives map travel? |
|---|---|---|
| Input dispatch | `APartyInputController` (plugin) | Yes — per-world but rebuilt the same |
| Session data | `FPartySessionState` in `UPartySessionSubsystem` | **Yes** — GameInstance subsystem |
| Phase logic | `APartyGameModeBase` subclasses (one per phase/map) | No — torn down on travel |
| Routing | `PartyFlow::GetRoute(EPartyPhase)` (namespace, code only) | N/A |
| Rendering | `APartyFlowHUD` (single HUD, phase-switches draw logic) | No — rebuilt each map |

### Travel wiring

Map travel uses `UGameplayStatics::OpenLevel(world, MapName, true, "?game=...")`.
The `?game=` option uses the **reflected class name** (no `A` prefix):
- `APartyLobbyGameMode` → `"?game=/Script/PartyButtons.PartyLobbyGameMode"`
- Getting this wrong silently falls back to `GlobalDefaultGameMode` with no error.

Phase is always written to the subsystem **before** `OpenLevel` so the destination
HUD's first `DrawHUD` reads the correct phase immediately. All travel calls are
deferred one tick (via `SetTimerForNextTick`) to avoid calling `OpenLevel` from
inside `BeginPlay` or a delegate handler.

### New files (added with map-flow scaffold)

| File | Role |
|---|---|
| `Game/Source/PartyButtons/Public/PartyTypes.h` | `EPartyPhase`, `FPartyGameInfo`, `FPartyPhaseRoute` |
| `Game/Source/PartyButtons/Public/PartySessionState.h` + `Private/` | Pure world-free data model; all testable logic |
| `Game/Source/PartyButtons/Public/PartySessionSubsystem.h` + `Private/` | `UGameInstanceSubsystem` persistence wrapper |
| `Game/Source/PartyButtons/Public/PartyFlowRouter.h` + `Private/` | Central `PartyFlow::GetRoute(EPartyPhase)` routing table |
| `Game/Source/PartyButtons/Public/PartyGameModeBase.h` + `Private/` | Abstract base; delegate binding; travel helpers |
| `Game/Source/PartyButtons/Public/PartyMain/MainMenu/Settings/Lobby/LevelSelect/Minigame/ResultsGameMode.h` | One phase GameMode each |
| `Game/Source/PartyButtons/Public/PartyFlowHUD.h` + `Private/` | Single phase-switching canvas HUD |
| `Plugins/PartyInput/Source/PartyInput/Private/Tests/PartyInputMainButtonTest.cpp` | Plugin tests: main button invariant, mappings, tap/hold |
| `Game/Source/PartyButtons/Private/Tests/PartySessionStateTest.cpp` | Session state + roster unit tests |
| `Game/Source/PartyButtons/Private/Tests/PartyFlowRouterTest.cpp` | Routing table tests (incl. class-path pitfall guard) |

---

## Alternatives considered and rejected

| Alternative | Rejected because |
|-------------|-----------------|
| Hand-authored `.uasset` IA_Button* files | Not autonomous; binary; requires editor GUI or Python asset factory (fragile across 5.x versions) |
| Blueprint-only controller | C++ is the cleaner handoff target per the spec |
| 16 separate Enhanced Input Actions per-key (no IMC) | Same as our approach, just without the IMC — we keep the IMC to stay compatible with the spec's B3 and to allow the designer override path |
| Lemontree "Enhanced Raw Input" plugin | Adds a third-party dependency; stock RawInput is sufficient for this POC |
