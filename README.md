# WaveMorse

Turn a **Waveshare ESP32-S3-LCD-1.47B** (USB-C board with the 1.47" screen)
into a USB morse key for the **Morse-It** iOS app: plug an iambic paddle
(or straight key) into the ESP32, plug the ESP32 into an iPhone, and key
away — with live decode on the built-in LCD.

- **Firmware** (`morsekey/`): iambic keyer (Curtis A/B), USB MIDI device
  the phone recognizes natively (TinyMIDI-style), on-screen decode of what
  you send, straight-key auto-detection, optional piezo sidetone.
- **Shield PCB** (`shield/`): a fab-ready snap-on board with the same
  footprint as the ESP32 board — 3.5mm jack, piezo, female headers.
  Gerbers included; see [shield/README.md](shield/README.md).

| Shield top | Shield bottom |
|---|---|
| ![shield top](shield/render-top.png) | ![shield bottom](shield/render-bottom.png) |

(The non-B USB-A variant of the board also works — change `PIN_LCD_BL` to
48 and the paddle pins to 1/2 in `morsekey/config.h`.)

Plugged into an iPhone 15, the phone powers the board and Morse-It sees it as
a MIDI input device — the same mechanism the N6ARA TinyMIDI uses.

## Parts list

Everything needed for a complete key. The shield parts are all
through-hole — no surface-mount soldering.

**Core**

| Qty | Part | Where | Note |
|-----|------|-------|------|
| 1 | Waveshare **ESP32-S3-LCD-1.47B** (USB-C variant) | Waveshare store, Amazon | the "B" board; the USB-A stick also works with pin changes (below) |
| 1 | USB-C to USB-C **data** cable | — | short and known-good; see the cable note below |
| 1 | Iambic or single-lever paddle with a 3.5 mm TRS plug, or a straight key with a TS (mono) plug | — | developed with a ZN-SL single-lever paddle |

**Shield** (optional, but makes the key a tidy snap-together unit — see
[shield/README.md](shield/README.md) for ordering the PCB and assembly)

| Qty | Part | DigiKey P/N | Note |
|-----|------|-------------|------|
| 1 | Shield PCB (`shield/morsekey-shield-gerbers.zip`) | JLCPCB, PCBWay, OSH Park… | 2 layers, 1.6 mm, all fab defaults |
| 1 | Same Sky SJ1-3523N 3.5 mm TRS jack | **CP1-3523N-ND** | through-hole, right angle |
| 1 | Same Sky CPT-1207-5LTH-T piezo transducer | **2223-CPT-1207-5LTH-T-ND** | optional sidetone; ø12.5 mm, 5.0 mm pitch |
| 2 | 1×9 female header socket, 2.54 mm (Sullins PPTC091LFBN-RC) | **S7007-ND** | on the shield |
| 2 | 1×9 male pin header, 2.54 mm, breakaway | **S1012EC-09-ND** | soldered to the ESP32 board, pins down |
| 2 | M2 screw + standoff | — | optional, for case mounting |

Piezo alternates on the same footprint: TDK **PS1240P02BT**
(445-2525-1-ND) or Same Sky **CPT-1206-3TH-T** (2223-CPT-1206-3TH-T-ND).
Any ~12 mm two-pin piezo *transducer* (externally driven, not a
self-oscillating "indicator"/active buzzer) with 5.0 mm pin spacing fits.
A passive transducer has no electrical polarity — if it has a "+" mark or
longer lead, put that in the square pad (GPIO5) by convention.

No resistors or capacitors are needed: the paddle inputs use the ESP32's
internal pull-ups, and the piezo is driven directly by PWM.

**About the cable:** a worn or charge-grade cable can carry enough data
to enumerate (the device name shows up) but not to flash or run MIDI —
symptoms are `No serial data received` on upload, or the board appearing
without a serial port. If anything USB acts strange, swap the cable first.

## Wiring the paddle

The paddle connects to the board's edge header. With a 3.5mm TRS jack
(standard paddle wiring):

| Paddle / jack        | Board header pin |
|----------------------|------------------|
| Tip (left paddle)    | **GPIO2** (dit)  |
| Ring (right paddle)  | **GPIO3** (dah)  |
| Sleeve (common)      | **GND**          |

All three are on the edge header (follow the silkscreen labels GND, 2, 3).
The inputs use internal pull-ups; the paddle just shorts them to ground — no
resistors needed. If dit/dah feel reversed, flip "Paddle: Swapped" in the
settings menu (or swap the wires). Free header GPIOs on this board are
2–11; avoid 1 (battery ADC) and 12/13 (IMU interrupts).

## Connecting to the iPhone

A plain **USB-C to USB-C cable** — the phone powers the board (~100 mA,
well within what the port supplies). The board enumerates as a MIDI device
named **MorseKey**.

## Morse-It setup

Two ways to use it — pick one in the settings menu ("MIDI" item):

**PADDLE mode (default, recommended).** The board sends the raw paddle
contacts as two MIDI notes (dit = note 18, dah = note 38, channel 1 —
Morse-It's documented defaults) and Morse-It runs
its own iambic keyer, sidetone and speed:
1. In Morse-It, set the key type to **Iambic Paddle**, matching the A/B
   mode shown on the board (default: A).
2. In the settings, under the hardware/MIDI interface section, select the
   **MorseKey** MIDI device and assign the two notes to the dit and dah
   areas (use the app's "learn" function: tap the field, press a paddle).
3. Set Morse-It's keyer speed to match the WPM shown on the board's screen
   so the on-screen decode matches what the app hears.

**KEYER mode.** The board does the iambic keying itself and sends the result
as a single on/off note (note 18) — configure Morse-It as a **Straight Key**.
What Morse-It receives is then exactly what the board's screen shows, and
speed is controlled on the board.

## Controls

**BOOT quick press** opens the on-screen settings menu; **BOOT held ~1 s**
(outside the menu) toggles iambic A/B directly — the status bar reflects it
immediately. In the menu, navigate with the paddle:

| Input in menu     | Effect                                   |
|-------------------|------------------------------------------|
| Dit paddle        | Next item                                |
| Dah paddle / BOOT tap | Change the selected value            |
| BOOT held ~1 s    | Save and exit                            |

Settings: speed (10–30 WPM), iambic A/B, MIDI mode (Paddle/Keyer), paddle
swap, sidetone on/off. All persist across power cycles. With a straight
key plugged in, navigate with the key and change values with BOOT taps.

The screen shows a centered status bar (WPM, iambic mode, MIDI mode, USB
state), the in-progress dit/dah pattern in yellow, and three lines of
decoded text: finished words in white, the word being keyed in cyan, and a
blinking cursor marking where the next character lands. The RGB LED is red while keying, dim
cyan at idle in PADDLE mode, dim green in KEYER mode. Unrecognized patterns
decode as `#`.

## Building and flashing

With [arduino-cli](https://arduino.github.io/arduino-cli/):

```sh
arduino-cli config add board_manager.additional_urls \
  https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32
arduino-cli lib install "GFX Library for Arduino"
./flash.sh
```

Or in the Arduino IDE, open `morsekey/morsekey.ino` with these Tools
settings: board **ESP32S3 Dev Module**, USB Mode **USB-OTG (TinyUSB)**
(required for MIDI), USB CDC On Boot **Enabled**, Flash Size **16MB**,
PSRAM **OPI PSRAM**. Install the "GFX Library for Arduino" library.
(IDE builds enumerate with the generic name "ESP32S3_DEV" — the "MorseKey"
device name comes from a `-DUSB_PRODUCT` flag that `flash.sh` sets; either
name works fine in Morse-It.)

A brand-new board's factory firmware ignores the automatic reset, so the
first flash needs the bootloader forced: unplug, hold **BOOT**, plug back
in, release BOOT after a second or two (the screen stays dark), then run
`./flash.sh`. After a forced flash, unplug/replug once to start the
firmware. From then on `./flash.sh` resets the board itself; if an upload
occasionally fails with `No serial data received`, just run it again. With
several USB serial devices attached, pass the port explicitly:
`./flash.sh /dev/cu.usbmodemXXXX`.

## Configuration

Runtime settings live in the on-screen menu. Compile-time options in
`morsekey/config.h`: paddle/piezo pins, MIDI note numbers, sidetone pitch,
defaults, and the paddle debounce time.

**Debounce:** a contact change is accepted only after the line has been
free of edges for `PADDLE_DEBOUNCE_MS` (6 ms). Edges are timestamped by
interrupt, so even sub-millisecond bounce is seen. 6 ms was measured on a
ZN-SL: press bounce lasts up to ~12 ms with quiet gaps under 4 ms inside
it, and switching one contact induces brief glitches on the other line.
A noisier paddle may need a little more; much more starts to add audible
latency.

## Shield PCB

`shield/` contains a manufacturable snap-on shield (KiCad design, Gerbers,
1:1 placement PDF) with the same footprint as the board: one 3.5mm jack
that takes either a paddle (TRS) or a straight key (TS mono — auto-detected,
status bar shows `SKEY`), plus an optional piezo sidetone on GPIO5. See
`shield/README.md` for ordering and assembly.

## Possible extensions

- **BLE MIDI**: the ESP32-S3 has Bluetooth; Morse-It also accepts BLE MIDI,
  which would make the key wireless (the board has a LiPo charger and BAT
  connector).
- **IMU gestures**: the onboard QMI8658 accelerometer could open the menu
  on a double-tap.
