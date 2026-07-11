// MorseKey — iambic paddle to USB MIDI for Morse-It, with live decode on LCD.
//
// Board:  Waveshare ESP32-S3-LCD-1.47 (USB-A stick with 1.47" ST7789)
// Paddle: 3.5mm TRS jack -> tip GPIO1 (dit), ring GPIO2 (dah), sleeve GND
// Phone:  plugs into iPhone 15 via a USB-C (male) to USB-A (female) adapter;
//         enumerates as a USB MIDI device that Morse-It can use directly.
//
// BOOT button opens the settings menu (speed, iambic A/B, MIDI mode,
// paddle swap, sidetone). In the menu: dit = next item, dah or BOOT tap =
// change value, BOOT held ~1s = save and exit.
//
// MIDI modes:
//   PADDLE (default) - raw paddle contacts are sent as two MIDI notes;
//                      Morse-It runs its own iambic keyer (set its key type
//                      to Iambic Paddle and assign the two notes).
//   KEYER            - this device does the keying and sends the result as
//                      one note; set Morse-It to Straight Key.
//
// Arduino IDE: board "ESP32S3 Dev Module", USB Mode "USB-OTG (TinyUSB)",
// USB CDC On Boot "Enabled", Flash 16MB, PSRAM "OPI PSRAM".

#include <Preferences.h>
#include <USB.h>
#include <USBMIDI.h>
#include "tusb.h"

#include "config.h"
#include "decoder.h"
#include "display.h"
#include "keyer.h"

static const uint8_t WPM_STEPS[] = {10, 12, 15, 18, 20, 22, 25, 28, 30};

USBMIDI midi;
Keyer keyer;
Decoder decoder;
Display display;
Preferences prefs;

struct Settings {
  uint8_t wpmIndex = 4; // 20 wpm
  bool iambicB = DEFAULT_IAMBIC_B;
  bool midiPaddle = DEFAULT_MIDI_PADDLE;
  bool swap = DEFAULT_SWAP;
  bool tone = DEFAULT_TONE;
} settings;

bool menuOpen = false;
uint8_t menuSel = 0;

volatile bool usbMounted = false;
bool lastDitSent = false, lastDahSent = false;
bool gfxOk = false;

// A mono (TS) straight-key plug in the paddle jack grounds the ring (the
// physical DAH line) permanently; when detected, the tip acts as a straight
// key until the ring opens again.
bool straightJack = false;
uint32_t ringSince = 0, ringOpenSince = 0;

struct StraightKey {
  bool down = false;
  uint32_t t = 0, downAt = 0;
};
StraightKey skPaddle;

uint8_t currentWpm() { return WPM_STEPS[settings.wpmIndex]; }

// --- callbacks -------------------------------------------------------------

void toneOn() {
#if PIN_SIDETONE
  if (settings.tone)
    ledcWriteTone(PIN_SIDETONE, SIDETONE_HZ);
#endif
}

void toneOff() {
#if PIN_SIDETONE
  ledcWriteTone(PIN_SIDETONE, 0);
#endif
}

void handleKeyDown(char element) {
  decoder.onElement(element, millis());
  rgbLedWrite(PIN_RGB_LED, 64, 0, 0);
  toneOn();
  if (!settings.midiPaddle)
    midi.noteOn(NOTE_KEY, 127, MIDI_CH);
}

void handleKeyUp() {
  idleLed();
  toneOff();
  if (!settings.midiPaddle)
    midi.noteOff(NOTE_KEY, 0, MIDI_CH);
}

void handleChar(char c) {
  display.append(c);
  Serial.write(c);
}

void handlePattern(const char *pat) { display.pattern(pat); }

void idleLed() {
  if (settings.midiPaddle)
    rgbLedWrite(PIN_RGB_LED, 0, 6, 8); // dim cyan: paddle mode
  else
    rgbLedWrite(PIN_RGB_LED, 0, 8, 0); // dim green: keyer mode
}

// --- settings --------------------------------------------------------------

void applySettings() {
  if (settings.swap)
    keyer.begin(PIN_DAH, PIN_DIT, PADDLE_DEBOUNCE_MS);
  else
    keyer.begin(PIN_DIT, PIN_DAH, PADDLE_DEBOUNCE_MS);
  keyer.setWpm(currentWpm());
  keyer.setIambicB(settings.iambicB);
  decoder.setWpm(currentWpm());
  display.status(currentWpm(), settings.iambicB, settings.midiPaddle,
                 usbMounted, straightJack);
  idleLed();
}

// Shared straight-key input handler: mirrors the contact to MIDI, sounds
// the sidetone, and classifies press length as dit/dah for local decode.
void pollStraightKey(StraightKey &sk, bool raw, uint32_t now) {
  if (raw == sk.down || now - sk.t < PADDLE_DEBOUNCE_MS)
    return;
  sk.down = raw;
  sk.t = now;
  if (sk.down) {
    sk.downAt = now;
    midi.noteOn(NOTE_KEY, 127, MIDI_CH);
    rgbLedWrite(PIN_RGB_LED, 64, 0, 0);
    toneOn();
  } else {
    midi.noteOff(NOTE_KEY, 0, MIDI_CH);
    idleLed();
    toneOff();
    uint32_t dur = now - sk.downAt;
    decoder.onElement(dur < 2u * (1200u / currentWpm()) ? '.' : '-', now);
  }
}

void saveSettings() {
  prefs.putUChar("wpmIdx", settings.wpmIndex);
  prefs.putBool("iambicB", settings.iambicB);
  prefs.putBool("midiPdl", settings.midiPaddle);
  prefs.putBool("swap", settings.swap);
  prefs.putBool("tone", settings.tone);
}

void loadSettings() {
  settings.wpmIndex = prefs.getUChar("wpmIdx", settings.wpmIndex);
  if (settings.wpmIndex >= sizeof(WPM_STEPS))
    settings.wpmIndex = 2;
  settings.iambicB = prefs.getBool("iambicB", settings.iambicB);
  settings.midiPaddle = prefs.getBool("midiPdl", settings.midiPaddle);
  settings.swap = prefs.getBool("swap", settings.swap);
  settings.tone = prefs.getBool("tone", settings.tone);
}

// --- settings menu -----------------------------------------------------------
// BOOT opens the menu. Inside: dit paddle = next item, dah paddle or a BOOT
// tap = change value, BOOT held ~1s = save and exit. (Works with a straight
// key too: dit navigates, BOOT changes.)

void menuRender() {
  display.menu(menuSel, currentWpm(), settings.iambicB, settings.midiPaddle,
               settings.swap, settings.tone);
}

void menuChange() {
  switch (menuSel) {
  case 0: settings.wpmIndex = (settings.wpmIndex + 1) % sizeof(WPM_STEPS); break;
  case 1: settings.iambicB = !settings.iambicB; break;
  case 2: settings.midiPaddle = !settings.midiPaddle; break;
  case 3: settings.swap = !settings.swap; break;
  case 4: settings.tone = !settings.tone; break;
  }
  menuRender();
}

void menuEnter() {
  // silence anything in flight before taking over the paddles
  if (lastDitSent) { midi.noteOff(NOTE_DIT, 0, MIDI_CH); lastDitSent = false; }
  if (lastDahSent) { midi.noteOff(NOTE_DAH, 0, MIDI_CH); lastDahSent = false; }
  midi.noteOff(NOTE_KEY, 0, MIDI_CH);
  toneOff();
  idleLed();
  menuOpen = true;
  menuSel = 0;
  menuRender();
}

void menuExit() {
  menuOpen = false;
  saveSettings();
  display.clearScreen(); // wipe menu remnants before repainting the UI
  applySettings();
  display.pattern(decoder.pattern());
  display.refreshText();
}

// simple debounced falling-edge detector for menu navigation
bool fellLow(uint8_t pin, bool &state, uint32_t &t, uint32_t now) {
  bool raw = digitalRead(pin) == LOW;
  if (raw != state && now - t >= PADDLE_DEBOUNCE_MS * 4) {
    state = raw;
    t = now;
    return raw;
  }
  return false;
}

void pollButton(uint32_t now) {
  static bool down = false;
  static uint32_t tDown = 0;
  static bool consumed = false; // hold action already fired for this press

  bool pressed = digitalRead(PIN_BOOT_BTN) == LOW;
  if (pressed && !down) {
    down = true;
    tDown = now;
    consumed = false;
  } else if (pressed && menuOpen && !consumed && now - tDown >= 800) {
    // save+exit fires as soon as the hold threshold is reached - no need
    // to wait for the release
    consumed = true;
    menuExit();
  } else if (!pressed && down) {
    down = false;
    if (consumed || now - tDown < 30)
      return;
    if (!menuOpen)
      menuEnter();
    else
      menuChange();
  }
}

// --- main ------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  prefs.begin("morsekey");
  loadSettings();

  pinMode(PIN_BOOT_BTN, INPUT_PULLUP);
#if PIN_SIDETONE
  ledcAttach(PIN_SIDETONE, SIDETONE_HZ, 10);
  ledcWriteTone(PIN_SIDETONE, 0);
#endif

  gfxOk = display.begin();
  Serial.printf("MorseKey boot, gfx=%d\n", gfxOk);

  // pins are assigned in applySettings() (paddle swap is a runtime setting)
  keyer.onKeyDown = handleKeyDown;
  keyer.onKeyUp = handleKeyUp;
  decoder.onChar = handleChar;
  decoder.onPatternChange = handlePattern;

  // Mono plug already inserted at power-up: ring is grounded from the start.
  delay(20);
  straightJack = digitalRead(PIN_DAH) == LOW;

  applySettings();

  // Note: with "USB CDC On Boot" enabled the USB stack is already running
  // here, so the product name can't be set at runtime — flash.sh bakes it
  // in with -DUSB_PRODUCT instead.
  midi.begin();
  USB.begin();
}

void loop() {
  uint32_t now = millis();

  // Settings menu takes over the paddles for navigation.
  if (menuOpen) {
    pollButton(now);
    static bool navDit = false, navDah = false;
    static uint32_t tDit = 0, tDah = 0;
    if (fellLow(settings.swap ? PIN_DAH : PIN_DIT, navDit, tDit, now)) {
      menuSel = (menuSel + 1) % 5;
      menuRender();
    }
    if (fellLow(settings.swap ? PIN_DIT : PIN_DAH, navDah, tDah, now))
      menuChange();
    return;
  }

  // Detect a mono plug in the paddle jack: ring (physical DAH pin, ignoring
  // paddle swap) grounded for 8s straight - far longer than any run of
  // dahs - or already grounded at power-up (checked in setup).
  bool ringGrounded = digitalRead(PIN_DAH) == LOW;
  if (!straightJack) {
    if (ringGrounded) {
      if (!ringSince)
        ringSince = now ? now : 1;
      else if (now - ringSince > 8000) {
        straightJack = true;
        if (lastDitSent) { midi.noteOff(NOTE_DIT, 0, MIDI_CH); lastDitSent = false; }
        if (lastDahSent) { midi.noteOff(NOTE_DAH, 0, MIDI_CH); lastDahSent = false; }
        applySettings();
      }
    } else {
      ringSince = 0;
    }
  } else {
    // exit when the ring opens (plug removed/changed) for 250ms
    if (ringGrounded) {
      ringOpenSince = 0;
    } else {
      if (!ringOpenSince)
        ringOpenSince = now ? now : 1;
      else if (now - ringOpenSince > 250) {
        straightJack = false;
        ringSince = ringOpenSince = 0;
        applySettings();
      }
    }
  }

  if (straightJack) {
    // tip (physical DIT pin) is the straight key; iambic keyer is idle
    pollStraightKey(skPaddle, digitalRead(PIN_DIT) == LOW, now);
  } else {
    keyer.poll(now);
  }

  decoder.poll(now, (!straightJack && keyer.keyed()) || skPaddle.down);
  pollButton(now);

  // Paddle passthrough: mirror debounced contacts as MIDI note on/off so
  // Morse-It's own iambic keyer does the timing and sidetone.
  if (settings.midiPaddle && !straightJack) {
    bool dit = keyer.ditPressed();
    bool dah = keyer.dahPressed();
    if (dit != lastDitSent) {
      dit ? midi.noteOn(NOTE_DIT, 127) : midi.noteOff(NOTE_DIT, 0, MIDI_CH);
      lastDitSent = dit;
    }
    if (dah != lastDahSent) {
      dah ? midi.noteOn(NOTE_DAH, 127) : midi.noteOff(NOTE_DAH, 0, MIDI_CH);
      lastDahSent = dah;
    }
  }

  // Debug heartbeat on the serial console.
  static uint32_t lastBeat = 0;
  if (now - lastBeat > 2000) {
    lastBeat = now;
    Serial.printf("alive gfx=%d usb=%d dit=%d dah=%d\n", gfxOk, usbMounted,
                  keyer.ditPressed(), keyer.dahPressed());
  }

  // Refresh the status bar when the USB connection state changes.
  usbMounted = tud_mounted() && !tud_suspended();
  static bool lastUsb = false;
  if (usbMounted != lastUsb) {
    lastUsb = usbMounted;
    display.status(currentWpm(), settings.iambicB, settings.midiPaddle,
                   usbMounted);
  }
}
