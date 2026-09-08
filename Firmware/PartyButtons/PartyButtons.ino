// PartyButtons.ino — Teensy 4.0 / 4.1 firmware
//
// Presents the arcade buttons as a USB HID Joystick.
//
// The slot index in BUTTON_PINS[] IS the dispatch identity — slot i reports as
// Joystick button (i + 1), which Unreal's RawInput surfaces as
// GenericUSBController_Button(i + 1). APartyInputController maps buttons 1-16 to
// players 1-16 and button 17 to the main/host button, so slot order here is a
// contract with the game, not a convenience. Never compact this table: leaving
// unwired slots as PIN_UNUSED is what keeps the white button on 17.
//
// Requirements:
//   Board:   Teensy 4.0 or 4.1 (Tools → Board → Teensyduino)
//   CPU:     600 MHz  (Tools → CPU Speed → 600 MHz)
//   USB:     Tools → USB Type → any option containing "Joystick".
//            Use a "Serial + ..." variant when SERIAL_DEBUG is 1.
//   Library: Bounce2  (Sketch → Include Library → Manage Libraries → Bounce2)
//
// Acceptance test: Windows joy.cpl (Set up USB game controllers → Properties)
// should light exactly one indicator per physical button, with no ghosting.
// The eight red buttons light indicators 1-8; the white button lights 17.

#include <Bounce2.h>

// ---- Config ----------------------------------------------------------------

// Marks a slot with no button wired to it. Slots keep their position so the
// joystick button numbering stays fixed; they are skipped everywhere below.
const uint8_t PIN_UNUSED = 255;

// Slot i -> Joystick button (i + 1). Pressed = LOW (INPUT_PULLUP).
// Pin 13 is reserved (onboard LED). Every real entry must be a distinct pin.
const uint8_t BUTTON_PINS[] = {
  // Players 1-8 — the eight red buttons.
  2, 3, 4, 5, 6, 7, 8, 9,

  // Players 9-16 — reserved for the full 16-button cabinet, not yet wired.
  PIN_UNUSED, PIN_UNUSED, PIN_UNUSED, PIN_UNUSED,
  PIN_UNUSED, PIN_UNUSED, PIN_UNUSED, PIN_UNUSED,

  // Button 17 — the white start/select button (host/MC control).
  0,
};

// Derived from the table above — never hardcode this.
const uint8_t NUM_BUTTONS = sizeof(BUTTON_PINS) / sizeof(BUTTON_PINS[0]);

// Debounce window in milliseconds.
const uint16_t DEBOUNCE_MS = 5;

// Onboard LED. Lit whenever any button is held — proves the sketch is running
// and the wiring is good without involving Windows at all.
const uint8_t LED_PIN = 13;

// Set to 1 to enable serial debug output (requires a "Serial + ... + Joystick"
// USB Type). Set to 0 for pure Joystick mode.
#define SERIAL_DEBUG 0

// ---- State -----------------------------------------------------------------

// Parallel to BUTTON_PINS. Entries for PIN_UNUSED slots are never attached or
// updated — a default-constructed Bounce reads pin 0, so polling them would
// mirror the white button onto every reserved slot.
Bounce buttons[NUM_BUTTONS];

// ---- Arduino lifecycle -----------------------------------------------------

void setup() {
  for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
    if (BUTTON_PINS[i] == PIN_UNUSED) {
      continue;
    }
    buttons[i].attach(BUTTON_PINS[i], INPUT_PULLUP);
    buttons[i].interval(DEBOUNCE_MS);
  }

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

#if SERIAL_DEBUG
  // Note: no `while (!Serial)` — that would stall the joystick until a serial
  // monitor attaches, which looks exactly like dead firmware.
  Serial.begin(115200);
  Serial.println("PartyButtons: serial debug active.");
#endif
}

void loop() {
  bool anyHeld = false;

  for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
    if (BUTTON_PINS[i] == PIN_UNUSED) {
      continue;
    }

    buttons[i].update();

    if (buttons[i].fell())  // HIGH→LOW = pressed (INPUT_PULLUP: pressed = LOW)
    {
      Joystick.button(i + 1, 1);
#if SERIAL_DEBUG
      Serial.printf("Button %u down (pin %u)\n", i + 1, BUTTON_PINS[i]);
#endif
    }

    if (buttons[i].rose())  // LOW→HIGH = released
    {
      Joystick.button(i + 1, 0);
#if SERIAL_DEBUG
      Serial.printf("Button %u up (pin %u)\n", i + 1, BUTTON_PINS[i]);
#endif
    }

    if (buttons[i].read() == LOW) {
      anyHeld = true;
    }
  }

  digitalWrite(LED_PIN, anyHeld ? HIGH : LOW);

  // No delay() — Teensy native HID auto-sends at USB frame rate (~1 ms).
}
