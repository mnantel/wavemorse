#!/usr/bin/env python3
"""Generate the MorseKey shield PCB for the Waveshare ESP32-S3-LCD-1.47B.

Run with KiCad's bundled python (provides the pcbnew module):
  ~/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3 generate_shield.py

Geometry source: Waveshare's dimensioned drawing of the board back
(ESP32-S3-LCD-1.47B-details-size.jpg):
  - ESP32 board 36.37 x 20.32 mm, 2x9 header, 2.54 mm pitch
  - header rows 17.78 mm apart (1.27 mm from each long edge)
  - first pin 1.97 mm from the short edge
The drawing shows the board's underside, which is what mates with the
shield top - so the shield is laid out as that drawing MIRRORED about the
vertical axis: USB-C end on the left, antenna end (jacks) on the right.

Shield: 36.37 x 20.32 mm - the exact ESP32 board footprint. A single
SJ1-3523N jack (12 x 11 mm body, fits between the header rows) opens at the
antenna end, bushing protruding like the USB-C does at the other end. It
serves both paddle (TRS) and straight key (TS mono - firmware auto-detects
the permanently grounded ring). Piezo sits between the rows left of the
jack.

Header rows as seen on the shield top, left to right:
  row A (y=1.27):  VBUS G 3V3 0 2 3 4 5 6
  row B (y=19.05): TXD RXD BAT G 11 10 9 8 7
"""

import os
import sys

import pcbnew
from pcbnew import VECTOR2I

MM = pcbnew.FromMM

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "morsekey-shield.kicad_pcb")
FOOTPRINT_DIR = os.path.expanduser(
    "~/Applications/KiCad/KiCad.app/Contents/SharedSupport/footprints")

# --- geometry (mm) ----------------------------------------------------------
SHIELD_LEN = 36.37          # = ESP32 board footprint
SHIELD_W = 20.32
ROW_A_Y = 1.27
ROW_B_Y = 19.05
PITCH = 2.54
PIN_END_X = SHIELD_LEN - 1.97             # 34.40 (pin nearest antenna end)
PIN_START_X = PIN_END_X - 8 * PITCH       # 14.08

ROW_A = ["VBUS", "G", "3V3", "0", "2", "3", "4", "5", "6"]
ROW_B = ["TXD", "RXD", "BAT", "G", "11", "10", "9", "8", "7"]
HEADER_NETS = {"2": "DIT", "3": "DAH", "4": "SKEY", "5": "PIEZO", "G": "GND"}

JACK_FACE_X = SHIELD_LEN            # jack body front face at the right edge
JACK_CY = SHIELD_W / 2              # 10.16, centered between the rows

# The buzzer footprint anchors at pad 1; pads at +0 and +7.6, body center
# at +3.8. Anchor at 13.2 puts the body at x 10.4..23.6, clear of the jack.
PIEZO_X, PIEZO_Y = 13.2, SHIELD_W / 2

TRACK_W = 0.35

# --- board & helpers ---------------------------------------------------------
board = pcbnew.CreateEmptyBoard()

# Header pads sit 1.27 mm from the long edges (same as the ESP32 board
# itself), leaving 0.42 mm copper-to-edge. Relax the default 0.5 mm rule to
# JLCPCB's 0.3 mm capability.
board.GetDesignSettings().m_CopperEdgeClearance = MM(0.3)

nets = {}
def net(name):
    if name not in nets:
        n = pcbnew.NETINFO_ITEM(board, name)
        board.Add(n)
        nets[name] = n
    return nets[name]

def load_fp(lib, name):
    fp = pcbnew.FootprintLoad(os.path.join(FOOTPRINT_DIR, lib + ".pretty"), name)
    if fp is None:
        sys.exit(f"footprint not found: {lib}/{name}")
    return fp

def place(fp, ref, x_mm, y_mm, rot_deg):
    fp.SetReference(ref)
    fp.SetPosition(VECTOR2I(MM(x_mm), MM(y_mm)))
    fp.SetOrientationDegrees(rot_deg)
    board.Add(fp)
    return fp

def pad_named(fp, name):
    for p in fp.Pads():
        if p.GetNumber() == str(name):
            return p
    sys.exit(f"{fp.GetReference()}: no pad {name}")

def pad_xy(fp, name):
    p = pad_named(fp, name).GetPosition()
    return round(pcbnew.ToMM(p.x), 3), round(pcbnew.ToMM(p.y), 3)

def poly_route(points, netname, layer=pcbnew.F_Cu, width=TRACK_W):
    for a, b in zip(points, points[1:]):
        if a == b:
            continue
        t = pcbnew.PCB_TRACK(board)
        t.SetStart(VECTOR2I(MM(a[0]), MM(a[1])))
        t.SetEnd(VECTOR2I(MM(b[0]), MM(b[1])))
        t.SetLayer(layer)
        t.SetWidth(MM(width))
        t.SetNet(net(netname))
        board.Add(t)

def add_via(x, y, netname):
    v = pcbnew.PCB_VIA(board)
    v.SetPosition(VECTOR2I(MM(x), MM(y)))
    v.SetDrill(MM(0.4))
    v.SetWidth(MM(0.8))
    v.SetNet(net(netname))
    board.Add(v)

def outline_rect(x1, y1, x2, y2, layer=pcbnew.Edge_Cuts, width=0.1):
    pts = [(x1, y1), (x2, y1), (x2, y2), (x1, y2)]
    for i in range(4):
        a, b = pts[i], pts[(i + 1) % 4]
        seg = pcbnew.PCB_SHAPE(board, pcbnew.SHAPE_T_SEGMENT)
        seg.SetStart(VECTOR2I(MM(a[0]), MM(a[1])))
        seg.SetEnd(VECTOR2I(MM(b[0]), MM(b[1])))
        seg.SetLayer(layer)
        seg.SetWidth(MM(width))
        board.Add(seg)

def silk_text(txt, x, y, size=1.0, rot=0, layer=pcbnew.F_SilkS):
    t = pcbnew.PCB_TEXT(board)
    t.SetText(txt)
    t.SetPosition(VECTOR2I(MM(x), MM(y)))
    t.SetLayer(layer)
    t.SetTextSize(VECTOR2I(MM(size), MM(size)))
    t.SetTextThickness(MM(0.15))
    t.SetTextAngleDegrees(rot)
    if layer == pcbnew.B_SilkS:
        t.SetMirrored(True)
    board.Add(t)

# --- outline -------------------------------------------------------------------
outline_rect(0, 0, SHIELD_LEN, SHIELD_W)

# --- header sockets -----------------------------------------------------------
for ref, labels, y in (("J1", ROW_A, ROW_A_Y), ("J2", ROW_B, ROW_B_Y)):
    fp = load_fp("Connector_PinSocket_2.54mm", "PinSocket_1x09_P2.54mm_Vertical")
    place(fp, ref, PIN_START_X, y, 90)
    for i, label in enumerate(labels):
        pad = pad_named(fp, i + 1)
        expect = VECTOR2I(MM(PIN_START_X + i * PITCH), MM(y))
        got = pad.GetPosition()
        if abs(got.x - expect.x) > 10 or abs(got.y - expect.y) > 10:  # nm
            sys.exit(f"{ref} pad {i+1} at {got}, expected {expect}")
        if label in HEADER_NETS:
            pad.SetNet(net("GND" if label == "G" else HEADER_NETS[label]))
        off = 2.6 if y < SHIELD_W / 2 else -2.6
        silk_text(label, PIN_START_X + i * PITCH, y + off, 0.7, 90)

hdr = {lab: (PIN_START_X + i * PITCH, ROW_A_Y) for i, lab in enumerate(ROW_A)}

# --- 3.5mm jacks (CUI SJ1-3523N) ----------------------------------------------
# Footprint at rotation 0: body on F.Fab spans y -7.7..3.3, bushing 3.3..6.3
# (opening faces +y). Pads: S(0,0) T(5,-5) R(-5,-5). Rotate so the opening
# faces +x, then shift so the body front face sits at the shield edge.
def place_jack(ref, cy):
    fp = load_fp("Connector_Audio", "Jack_3.5mm_CUI_SJ1-3523N_Horizontal")
    place(fp, ref, 0, 0, 0)
    # find rotation that puts the opening (courtyard max) toward +x
    for rot in (90, -90):
        fp.SetOrientationDegrees(rot)
        bb = fp.GetCourtyard(pcbnew.F_CrtYd).BBox()
        if bb.GetWidth() > bb.GetHeight():
            s = pad_named(fp, "S").GetPosition()
            t = pad_named(fp, "T").GetPosition()
            if s.x > t.x:  # sleeve pad is nearest the opening -> +x side
                break
    bb = fp.GetCourtyard(pcbnew.F_CrtYd).BBox()
    # courtyard right = fab 6.6; the footprint intends the board edge at its
    # collar line (fab 4.5) - put that at the shield edge, bushing outside
    dx = MM(JACK_FACE_X + (6.6 - 4.5)) - bb.GetRight()
    dy = MM(cy) - bb.GetCenter().y
    fp.Move(VECTOR2I(dx, dy))
    # drop the footprint's own Edge.Cuts marker segments - the shield outline
    # is drawn separately and the duplicates would malform it
    for item in [g for g in fp.GraphicalItems()
                 if g.GetLayer() == pcbnew.Edge_Cuts]:
        fp.Remove(item)
    return fp

j_paddle = place_jack("JP", JACK_CY)

pad_named(j_paddle, "S").SetNet(net("GND"))
pad_named(j_paddle, "T").SetNet(net("DIT"))
pad_named(j_paddle, "R").SetNet(net("DAH"))

# jack label on the bottom silk - the top is hidden under the jack body
silk_text("PADDLE / KEY", 29.5, JACK_CY, 1.1, 0, pcbnew.B_SilkS)

# --- piezo ---------------------------------------------------------------------
piezo = load_fp("Buzzer_Beeper", "Buzzer_12x9.5RM7.6")
place(piezo, "BZ1", PIEZO_X, PIEZO_Y, 0)
pad_named(piezo, 1).SetNet(net("PIEZO"))
pad_named(piezo, 2).SetNet(net("GND"))

# --- mounting holes (M2, left corners, clear of everything) --------------------
for ref, x, y in (("H1", 2.5, 2.5), ("H2", 2.5, SHIELD_W - 2.5)):
    fp = load_fp("MountingHole", "MountingHole_2.2mm_M2")
    place(fp, ref, x, y, 0)

# --- routing --------------------------------------------------------------------
tp = pad_xy(j_paddle, "T")   # tip
rp = pad_xy(j_paddle, "R")   # ring
sp = pad_xy(j_paddle, "S")   # sleeve
print("pads: T", tp, "R", rp, "S", sp)
if tp[1] >= rp[1]:
    sys.exit("unexpected jack orientation: tip pad is not the upper one")

pz1 = pad_xy(piezo, 1)
dit_pin, dah_pin, pz_pin = hdr["2"], hdr["3"], hdr["5"]

# DIT: straight drop from pin 2 to the tip pad row, short jog right
poly_route([dit_pin, (dit_pin[0], tp[1]), tp], "DIT")

# DAH: around the tip pad - jog right between the tip and sleeve pad
# columns, drop to the ring row, approach the ring pad from the right
jog_x = (tp[0] + sp[0]) / 2
poly_route([dah_pin, (dah_pin[0], 3.2), (jog_x, 3.2), (jog_x, rp[1]), rp],
           "DAH")

# PIEZO on the back layer: down from pin 5, across above the jack pads,
# down the piezo pad-1 column
poly_route([pz_pin, (pz_pin[0], 3.3), (pz1[0], 3.3), pz1], "PIEZO",
           pcbnew.B_Cu)

# The pour stays clear of the thin edge strips (starved-thermal slivers), so
# tie both header G pads to the piezo GND pad explicitly; the piezo pad
# bridges into the pour.
pz2 = pad_xy(piezo, 2)
poly_route([(16.62, ROW_A_Y), (16.62, pz2[1]), pz2, (pz2[0], 17.0),
            (21.7, 17.0), (21.7, ROW_B_Y)], "GND", pcbnew.F_Cu, 0.5)

# --- GND pour on B.Cu -------------------------------------------------------------
zone = pcbnew.ZONE(board)
zone.SetLayer(pcbnew.B_Cu)
zone.SetNet(net("GND"))
# inset from the long edges: the strip between header pads and edge is too
# narrow to fill and only produces starved-thermal slivers
pts = pcbnew.VECTOR_VECTOR2I([
    VECTOR2I(MM(0), MM(2.6)), VECTOR2I(MM(SHIELD_LEN), MM(2.6)),
    VECTOR2I(MM(SHIELD_LEN), MM(17.7)), VECTOR2I(MM(0), MM(17.7)),
])
zone.AddPolygon(pts)
zone.SetMinThickness(MM(0.25))
board.Add(zone)

# --- silkscreen ----------------------------------------------------------------
silk_text("MorseKey shield v2", 8.0, 10.16, 0.9, 90)
silk_text("USB", 3.6, 10.16, 0.7, 90)

# --- save ------------------------------------------------------------------------
# Zone fill is done afterwards by `kicad-cli pcb drc --refill-zones
# --save-board` (ZONE_FILLER segfaults in headless pcbnew on macOS).
pcbnew.SaveBoard(OUT, board)
print("wrote", OUT)
