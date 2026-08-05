# Deferred Manual Work

Tasks below require physical hardware or human interaction and cannot be completed autonomously.
Each item has a single-line completion instruction.

---

## 1. Flash the Teensy firmware

**Requires:** Teensy 4.0, Arduino IDE + Teensyduino, Bounce2 library installed.

**Instruction:** Follow `Firmware/README.md` → flash `Firmware/PartyButtons.ino` with
Tools → USB Type = "Joystick", Tools → CPU Speed = 600 MHz, then click Upload.

---

## 2. Confirm all 16 buttons in `joy.cpl`

**Requires:** Flashed Teensy plugged into PC.

**Instruction:** Run `joy.cpl` → select the Teensy device → Properties → press each physical
button and confirm exactly one corresponding indicator lights without ghosting.

---

## 3. Read the real VID + PID and update `DefaultInput.ini`

**Requires:** Flashed Teensy plugged into PC.

**Instruction:** Device Manager → HID devices → right-click the Teensy joystick → Properties →
Details → Hardware IDs → note `VID_xxxx&PID_yyyy`. Open
`Game/Config/DefaultInput.ini` and confirm `VendorID`/`ProductID` match.
Teensy VID is almost always `0x16C0`. Observed PID for the "Joystick" USB Type on
this project's Teensy 4.1 is `0x0482` (already set). Different USB Types may yield a
different PID.

---

## 4. PIE smoke test (all 16 buttons, real hardware)

**Requires:** Flashed Teensy connected **before** launching the editor (stock RawInput has no
hotplug; the device must be present at editor startup).

**Instruction:** Connect Teensy → open the project in the editor → Play In Editor →
press each of the 16 physical buttons one at a time → confirm:
- `LogPartyInput: Party button pressed: player N` appears in the Output Log for each press.
- The matching cell in the 4×4 HUD indicator grid lights up green.
- No duplicate indices, no missed presses, no ghosting.

---

## 5. Fallback: Unreal doesn't enumerate the device

Only attempt if the PIE smoke test shows no `LogPartyInput` events despite correct `joy.cpl`.

**Option A (trim USB descriptor):** Follow the spec's §2 "Fallback" note — edit
`usb_desc.h` in the Teensyduino core to limit the HID descriptor to 16 buttons / 0 axes.

**Option B (Lemontree plugin):** Replace the stock RawInput plugin with the free
Lemontree "Enhanced Raw Input" plugin (GitHub) — drop-in replacement, supports hotplug,
auto-maps device buttons, Enhanced-Input compatible. Removes the need for the
`DefaultInput.ini` block.

---

## 7. Add physical "main button" (button 17) to the Teensy

**Requires:** Physical main button wired to a new digital input on the Teensy.

**Instruction:** Wire a new big red button (the host/MC control) to an unused digital pin.
In `Firmware/PartyButtons.ino`, add a 17th button call alongside the 16 player buttons:
`Joystick.button(17, ...)`. This exposes `GenericUSBController_Button17` to RawInput,
which maps to `MainButtonAction` in `APartyInputController::BuildButtonInputs()`.
Until this is done, the main button only works via the keyboard **Enter** key (dev emulation).

---

## 9. Optional: enable serial debug output

**Instruction:** In `Firmware/PartyButtons.ino`, change `#define SERIAL_DEBUG 0` to `1`,
change Tools → USB Type = "Serial + Joystick", re-flash, open the Serial Monitor at
115200 baud, and press buttons to confirm "Button N down/up" messages.
