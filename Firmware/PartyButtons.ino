// PartyButtons.ino — Teensy 4.0 firmware
//
// Presents 16 momentary arcade buttons as a USB HID Joystick with 16 buttons.
// Each press fires Joystick.button(i+1, 1); each release fires Joystick.button(i+1, 0).
//
// Requirements:
//   Board:   Teensy 4.0 (Tools → Board → Teensyduino → Teensy 4.0)
//   CPU:     600 MHz  (Tools → CPU Speed → 600 MHz)
//   USB:     Tools → USB Type → "Joystick"  (or "Serial + Joystick" while debugging)
//   Library: Bounce2  (Sketch → Include Library → Manage Libraries → Bounce2)
//
// Acceptance test: Windows joy.cpl (Set up USB game controllers → Properties)
// should show 16 button indicators; pressing each physical button lights exactly
// one corresponding indicator with no ghosting or cross-talk.

#include <Bounce2.h>

// ---- Config ----------------------------------------------------------------

// GPIO pins for buttons 1..16.
// Pin 13 is skipped (onboard LED). Adjust for your actual wiring.
const uint8_t BUTTON_PINS[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 14, 15, 16 };

// Debounce window in milliseconds.
const uint16_t DEBOUNCE_MS = 5;

// Set to 1 to enable serial debug output (requires USB Type = "Serial + Joystick").
// Set to 0 (default) for pure Joystick mode.
#define SERIAL_DEBUG 0

// ---- State -----------------------------------------------------------------

Bounce buttons[16];

// ---- Arduino lifecycle -----------------------------------------------------

void setup()
{
    for (uint8_t i = 0; i < 16; i++)
    {
        buttons[i].attach(BUTTON_PINS[i], INPUT_PULLUP);
        buttons[i].interval(DEBOUNCE_MS);
    }

#if SERIAL_DEBUG
    Serial.begin(115200);
    while (!Serial) {}  // wait for USB serial (remove if non-blocking startup needed)
    Serial.println("PartyButtons: serial debug active.");
#endif
}

void loop()
{
    for (uint8_t i = 0; i < 16; i++)
    {
        buttons[i].update();

        if (buttons[i].fell())   // HIGH→LOW = pressed (INPUT_PULLUP: pressed = LOW)
        {
            Joystick.button(i + 1, 1);
#if SERIAL_DEBUG
            Serial.printf("Button %u down\n", i + 1);
#endif
        }

        if (buttons[i].rose())   // LOW→HIGH = released
        {
            Joystick.button(i + 1, 0);
#if SERIAL_DEBUG
            Serial.printf("Button %u up\n", i + 1);
#endif
        }
    }
    // No delay() — Teensy native HID auto-sends at USB frame rate (~1 ms).
}
