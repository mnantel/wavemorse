# MorseKey shield (v2)

A snap-on shield for the Waveshare ESP32-S3-LCD-1.47B with the **same
footprint as the board itself (36.37 × 20.32 mm)**: one 3.5mm jack that
serves both an iambic paddle (TRS plug) and a straight key (TS mono plug —
the firmware auto-detects it), an optional piezo sidetone, and female
headers the ESP32 board (male pins soldered pointing down) plugs into.

The jack opens at the antenna end, its bushing protruding past the edge the
same way the USB-C does at the other end. Renders: `render-top.png` /
`render-bottom.png`.

## Ordering (JLCPCB, PCBWay, OSH Park…)

Upload `morsekey-shield-gerbers.zip` to the fab's quote page. All defaults
are fine: 2 layers, 1.6 mm FR-4, HASL, any color, smallest quantity
(usually 5). Cost is a few dollars plus shipping.

**Before ordering — the paper test:** print `placement-1to1.pdf` at 100%
scale (no "fit to page"), hold the ESP32 board's header holes over the
drawing, and confirm every hole lines up. This catches any dimension error
while it's still free.

## Bill of materials

| Qty | Part | Note |
|-----|------|------|
| 2 | 1×9 female header socket, 2.54 mm | standard 8.5 mm height |
| 2 | 1×9 male pin header, 2.54 mm | solder to the ESP32 board, pins down |
| 1 | CUI SJ1-3523N 3.5 mm TRS jack | through-hole; DigiKey CP1-3523N-ND |
| 1 | 12 mm piezo buzzer, 7.6 mm pin spacing (e.g. TDK PS1240) | optional — sidetone |
| 2 | M2 screw + standoff | optional — case mounting, USB end corners |

## Connections (all done by the PCB)

| Net | Header pin | Goes to |
|-----|-----------|---------|
| DIT | GPIO2 | jack tip |
| DAH | GPIO3 | jack ring |
| PIEZO | GPIO5 | piezo + |
| GND | both G pins | jack sleeve, piezo −, ground pour |

GPIO4 (SKEY) stays free on the header for a dedicated straight-key input if
you ever want a second jack off-board.

**Paddle vs straight key:** a TRS (stereo) plug = paddle on tip/ring. A TS
(mono) straight-key plug grounds the ring permanently; the firmware detects
this (at power-up, or after 8 s of continuous "dah") and turns the tip into
a straight-key input — the status bar shows `SKEY`. Unplug it and paddle
mode returns automatically.

## Assembly

1. Solder the male pin headers to the ESP32 board pointing **down** (away
   from the screen).
2. Solder female sockets, the jack, and optionally the piezo to the shield
   top. The "USB" silk label marks the USB-C end.
3. Snap the ESP32 board in, screen up, USB-C over the "USB" label.

## Regenerating

`generate_shield.py` builds the .kicad_pcb from scratch (see its header for
the KiCad python invocation), then:

```sh
kicad-cli pcb drc --refill-zones --save-board --severity-error --exit-code-violations -o drc.rpt morsekey-shield.kicad_pcb
kicad-cli pcb export gerbers --check-zones -l "F.Cu,B.Cu,F.Paste,B.Paste,F.Silkscreen,B.Silkscreen,F.Mask,B.Mask,Edge.Cuts" -o gerbers/ morsekey-shield.kicad_pcb
kicad-cli pcb export drill --format excellon --excellon-separate-th -o gerbers/ morsekey-shield.kicad_pcb
```
