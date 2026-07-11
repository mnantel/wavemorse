#pragma once

// ---------------------------------------------------------------------------
// MorseKey configuration — Waveshare ESP32-S3-LCD-1.47
// ---------------------------------------------------------------------------

// Paddle inputs (edge header, active LOW, internal pull-ups).
// Wire a 3.5mm TRS jack: tip -> DIT, ring -> DAH, sleeve -> GND.
// Note: on the 1.47B variant GPIO1 is the battery ADC and GPIO12/13 are IMU
// interrupts; GPIO2-GPIO11 are the free header pins.
#define PIN_DIT 2
#define PIN_DAH 3

// Piezo sidetone pin. The MorseKey shield has a piezo footprint wired to
// GPIO5; harmless if unpopulated. Enable/disable at runtime in the menu.
#define PIN_SIDETONE 5
#define SIDETONE_HZ 600

// Onboard peripherals (fixed by the board, do not change).
#define PIN_LCD_MOSI 45
#define PIN_LCD_SCK 40
#define PIN_LCD_CS 42
#define PIN_LCD_DC 41
#define PIN_LCD_RST 39
// Backlight is GPIO46 on the USB-C ESP32-S3-LCD-1.47B (this board).
// On the USB-A ESP32-S3-LCD-1.47 it is GPIO48 instead.
#define PIN_LCD_BL 46
#define PIN_BOOT_BTN 0
#define PIN_RGB_LED 38

// MIDI notes sent to Morse-It. These match Morse-It's documented adapter
// defaults (https://pacolabs.com/iOS/MorseIt/MIDI/): dit = 18, dah = 38,
// straight key = 18, channel 0 - so the app needs little or no note
// assignment. Reassignable in the app if you change them here.
#define MIDI_CH 0
#define NOTE_DIT 18 // paddle mode: dit contact
#define NOTE_DAH 38 // paddle mode: dah contact
#define NOTE_KEY 18 // keyer mode / straight key: locally keyed output

// Defaults (changeable at runtime in the settings menu, saved to flash).
#define DEFAULT_WPM 20
#define DEFAULT_IAMBIC_B false   // false = mode A
#define DEFAULT_MIDI_PADDLE true // true = send raw paddle, false = send keyed output
#define DEFAULT_SWAP false       // swap dit/dah paddles
#define DEFAULT_TONE true        // piezo sidetone

// Debounce time for paddle contacts, in ms.
#define PADDLE_DEBOUNCE_MS 5
