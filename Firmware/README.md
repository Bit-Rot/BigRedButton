# PartyButtons Firmware — Teensy 4.0

Flashes the Teensy 4.0 to appear as a 16-button USB HID Joystick.

## Prerequisites

1. **Arduino IDE** (1.8.x or 2.x): https://www.arduino.cc/en/software
2. **Teensyduino**: https://www.pjrc.com/teensy/td_download.html  
   Install on top of Arduino IDE. Adds Teensy board support + Joystick library.
3. **Bounce2** library:  
   Sketch → Include Library → Manage Libraries → search "Bounce2" → Install

## Wiring

Each of the 16 arcade buttons connects:
- One terminal → GPIO pin (see `BUTTON_PINS[]` in `PartyButtons.ino`, default skips pin 13)
- Other terminal → common GND rail

Pins default to `INPUT_PULLUP`; buttons are pressed-LOW (active low).

## Flash procedure

1. Open `PartyButtons.ino` in Arduino IDE.
2. **Tools → Board** → Teensyduino → **Teensy 4.0**
3. **Tools → CPU Speed** → **600 MHz**
4. **Tools → USB Type** → **Joystick**  
   (Use "Serial + Joystick" + set `#define SERIAL_DEBUG 1` while debugging.)
5. Verify pin assignments in `BUTTON_PINS[]` match your wiring.
6. Click **Upload** (Ctrl+U). Teensy Loader will flash automatically.

## Acceptance test

After flashing and plugging in:

1. Open **Start → Run → `joy.cpl`** (Set up USB game controllers).
2. Select the Teensy device → **Properties**.
3. Press each physical button; exactly one corresponding button indicator should
   light on screen. Release should clear immediately. No ghosting or cross-talk.
4. Note the **VID + PID** from Device Manager:  
   Device Manager → HID devices → right-click Teensy → Properties →
   Details → Hardware IDs → value like `HID\VID_16C0&PID_0486`  
   → Update `Game/Config/DefaultInput.ini` `ProductID=` accordingly.

## Troubleshooting

- **No device appears**: confirm USB Type = Joystick in Arduino IDE, not Serial-only.
- **Phantom presses**: increase `DEBOUNCE_MS` (try 10–20 ms).
- **Unreal doesn't enumerate the device**: follow the "Fallback" notes in
  `AI/deferred-manual-work.md` (trim USB descriptor to 16 buttons / 0 axes).
