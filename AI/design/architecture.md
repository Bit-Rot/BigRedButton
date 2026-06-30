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

## Alternatives considered and rejected

| Alternative | Rejected because |
|-------------|-----------------|
| Hand-authored `.uasset` IA_Button* files | Not autonomous; binary; requires editor GUI or Python asset factory (fragile across 5.x versions) |
| Blueprint-only controller | C++ is the cleaner handoff target per the spec |
| 16 separate Enhanced Input Actions per-key (no IMC) | Same as our approach, just without the IMC — we keep the IMC to stay compatible with the spec's B3 and to allow the designer override path |
| Lemontree "Enhanced Raw Input" plugin | Adds a third-party dependency; stock RawInput is sufficient for this POC |
