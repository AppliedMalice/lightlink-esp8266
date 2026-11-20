#!/usr/bin/env python3
"""Generate the LightLink ESP8266 interface board (placement, nets, outline, rules, silkscreen).
Routing is done afterwards by Freerouting (see route.sh / post.py)."""
import sys
import pcbnew
from pcbnew import VECTOR2I, FromMM as mm

OUT = sys.argv[1]
FP = "/usr/share/kicad/footprints/"
W, H = 100.0, 64.0
VERSION = "v1.0"

b = pcbnew.BOARD()
b.SetCopperLayerCount(2)

# ---------- design rules (comfortably inside PCBWay standard capability) ----------
ds = b.GetDesignSettings()
nc = ds.m_NetSettings.m_DefaultNetClass
nc.SetTrackWidth(mm(0.4))
nc.SetClearance(mm(0.2))
nc.SetViaDiameter(mm(0.9))
nc.SetViaDrill(mm(0.45))
ds.m_TrackMinWidth = mm(0.2)
ds.m_ViasMinSize = mm(0.6)
ds.m_MinThroughDrill = mm(0.3)
ds.m_CopperEdgeClearance = mm(0.4)
ds.m_HoleClearance = mm(0.25)
ds.m_HoleToHoleMin = mm(0.25)
ds.m_SolderMaskMinWidth = mm(0.1)

nets = {}
def net(name):
    if name not in nets:
        n = pcbnew.NETINFO_ITEM(b, name)
        b.Add(n)
        nets[name] = n
    return nets[name]

def place(lib, name, ref, x, y, rot=0, value="", strip_silk=False, hide_ref=False, strip_crtyd=False):
    fp = pcbnew.FootprintLoad(FP + lib + ".pretty", name)
    fp.SetFPID(pcbnew.LIB_ID(lib, name))
    fp.SetReference(ref)
    fp.SetValue(value or name)
    fp.Value().SetVisible(False)
    fp.SetPosition(VECTOR2I(mm(x), mm(y)))
    fp.SetOrientationDegrees(rot)
    if strip_silk:
        for g in fp.GraphicalItems():
            if g.GetLayer() == pcbnew.F_SilkS:
                g.SetLayer(pcbnew.F_Fab)   # keep it off the silkscreen
            if strip_crtyd and g.GetLayer() == pcbnew.F_CrtYd:
                g.SetLayer(pcbnew.F_Fab)   # dual-footprint rows overlap on purpose
    if hide_ref:
        fp.Reference().SetVisible(False)
    b.Add(fp)
    return fp

def connect(fp, padnum, netname):
    for p in fp.Pads():
        if p.GetNumber() == str(padnum):
            p.SetNet(net(netname))

# ---------- NodeMCU: dual footprint, v2 "Amica" (0.9") and v3 "LoLin" (1.1") ----------
MCU_CX = 17.0
P1Y = H - 2.6 - 14 * 2.54          # pin 1 row (antenna end); pin 15 is 2.6 mm from the bottom edge
rows = {
    "L3": MCU_CX - 13.97, "L2": MCU_CX - 11.43,   # A0 side (v3 outer, v2 inner)
    "R2": MCU_CX + 11.43, "R3": MCU_CX + 13.97,   # D0 side
}
left_map = {10: "GND", 14: "GND", 15: "VIN"}
right_map = {1: "D0", 2: "D1", 3: "D2", 4: "D3", 5: "D4", 7: "GND", 8: "D5", 9: "D6",
             10: "D7", 11: "D8", 14: "GND"}
mcu = {}
for key, x in rows.items():
    fp = place("Connector_PinHeader_2.54mm", "PinHeader_1x15_P2.54mm_Vertical",
               "MCU_" + key, x, P1Y, strip_silk=True, hide_ref=True, strip_crtyd=key.endswith("3"),
               value="NodeMCU " + ("v3 LoLin" if key.endswith("3") else "v2 Amica"))
    mcu[key] = fp
    for pin, n in (left_map if key[0] == "L" else right_map).items():
        connect(fp, pin, n)

# ---------- 74HCT245 x2 (DIP-20 in sockets) ----------
def hct(ref, x, y, band):
    u = place("Package_DIP", "DIP-20_W7.62mm_Socket", ref, x, y, value="74HCT245")
    m = {1: "+5V", 20: "+5V", 10: "GND", 19: "GND", 9: "GND",
         2: "D5", 3: "D8", 4: "D0", 5: "D3", 6: "D4"}
    m[7], m[8] = ("D7", "D6") if band == "TOP" else ("D1", "D2")
    p = band
    m.update({18: f"{p}_SRCK_U", 17: f"{p}_RCK_U", 16: f"{p}_ROW_A", 15: f"{p}_ROW_B",
              14: f"{p}_ROW_C", 13: f"{p}_RED", 12: f"{p}_GREEN"})
    for pin, n in m.items():
        connect(u, pin, n)
    return u

U1 = hct("U1", 45.0, 6.0, "TOP")
U2 = hct("U2", 45.0, 35.0, "BOT")

# ---------- series resistors (SRCK, RCK) ----------
def res(ref, x, y, a, bnet, val):
    r = place("Resistor_THT", "R_Axial_DIN0207_L6.3mm_D2.5mm_P10.16mm_Horizontal", ref, x, y, value=val)
    connect(r, 1, a); connect(r, 2, bnet)
    return r

res("R1", 58.0, 11.08, "TOP_SRCK_U", "TOP_SRCK", "33R")
res("R2", 58.0, 16.0, "TOP_RCK_U", "TOP_RCK", "33R")
res("R3", 58.0, 40.08, "BOT_SRCK_U", "BOT_SRCK", "33R")
res("R4", 58.0, 45.0, "BOT_RCK_U", "BOT_RCK", "33R")

# ---------- ribbon headers (shrouded 2x5, original LightLink cables) ----------
def ribbon(ref, x, y, p):
    j = place("Connector_IDC", "IDC-Header_2x05_P2.54mm_Vertical", ref, x, y,
              value=("TOP" if p == "TOP" else "BOTTOM") + " band ribbon", hide_ref=True)
    m = {1: f"{p}_RCK", 2: f"{p}_RED", 3: f"{p}_ROW_C", 4: f"{p}_SRCK", 5: f"{p}_ROW_A",
         6: f"{p}_ROW_B", 7: f"{p}_SRCK", 9: f"{p}_GREEN", 10: "GND"}   # pin 8 not connected
    for pin, n in m.items():
        connect(j, pin, n)
    return j

J1 = ribbon("J1", 84.0, 8.0, "TOP")
J2 = ribbon("J2", 84.0, 37.0, "BOT")

# ---------- power input, jumper, caps, LED ----------
J3 = place("TerminalBlock_Phoenix", "TerminalBlock_Phoenix_MKDS-1,5-2-5.08_1x02_P5.08mm_Horizontal",
           "J3", 62.0, 58.0, value="5V IN", hide_ref=True)
connect(J3, 1, "+5V"); connect(J3, 2, "GND")

JP1 = place("Connector_PinHeader_2.54mm", "PinHeader_1x02_P2.54mm_Vertical", "JP1", 35.5, 47.0, rot=90,
            value="VIN link", hide_ref=True)
connect(JP1, 1, "+5V"); connect(JP1, 2, "VIN")

C1 = place("Capacitor_THT", "C_Disc_D5.0mm_W2.5mm_P5.00mm", "C1", 57.0, 3.2, value="100nF")
connect(C1, 1, "+5V"); connect(C1, 2, "GND")
C2 = place("Capacitor_THT", "C_Disc_D5.0mm_W2.5mm_P5.00mm", "C2", 57.0, 32.2, value="100nF")
connect(C2, 1, "+5V"); connect(C2, 2, "GND")
C3 = place("Capacitor_THT", "CP_Radial_D6.3mm_P2.50mm", "C3", 36.5, 57.5, value="100uF")
connect(C3, 1, "+5V"); connect(C3, 2, "GND")

D1 = place("LED_THT", "LED_D3.0mm", "D1", 88.0, 56.0, value="PWR", hide_ref=True)
connect(D1, 1, "GND"); connect(D1, 2, "LED_A")      # pad 1 = cathode
R5 = place("Resistor_THT", "R_Axial_DIN0207_L6.3mm_D2.5mm_P10.16mm_Horizontal", "R5", 77.0, 61.0, value="1k")
connect(R5, 1, "+5V"); connect(R5, 2, "LED_A")

for i, (x, y) in enumerate([(3.5, 3.5), (96.0, 4.0), (96.0, 60.0), (39.0, 3.5)]):
    place("MountingHole", "MountingHole_3.2mm_M3", f"H{i+1}", x, y, hide_ref=True, value="M3")

# ---------- board outline ----------
def seg(layer, x1, y1, x2, y2, w=0.1):
    s = pcbnew.PCB_SHAPE(b)
    s.SetShape(pcbnew.SHAPE_T_SEGMENT)
    s.SetStart(VECTOR2I(mm(x1), mm(y1))); s.SetEnd(VECTOR2I(mm(x2), mm(y2)))
    s.SetLayer(layer); s.SetWidth(mm(w))
    b.Add(s)

def rect(layer, x1, y1, x2, y2, w=0.15):
    seg(layer, x1, y1, x2, y1, w); seg(layer, x2, y1, x2, y2, w)
    seg(layer, x2, y2, x1, y2, w); seg(layer, x1, y2, x1, y1, w)

rect(pcbnew.Edge_Cuts, 0, 0, W, H, 0.1)

# ---------- antenna keepout: no copper under the ESP-12 antenna ----------
ko = pcbnew.ZONE(b)
ko.SetIsRuleArea(True)
ko.SetDoNotAllowCopperPour(True); ko.SetDoNotAllowTracks(True); ko.SetDoNotAllowVias(True)
ko.SetDoNotAllowPads(False); ko.SetDoNotAllowFootprints(False)
ls = pcbnew.LSET(); ls.AddLayer(pcbnew.F_Cu); ls.AddLayer(pcbnew.B_Cu); ko.SetLayerSet(ls)
ol = ko.Outline(); ol.NewOutline()
for x, y in [(0, 0), (34.0, 0), (34.0, P1Y - 2.4), (0, P1Y - 2.4)]:
    ol.Append(mm(x), mm(y))
ko.SetZoneName("antenna_keepout")
b.Add(ko)

# ---------- silkscreen ----------
def text(s, x, y, size=1.0, layer=pcbnew.F_SilkS, bold=False, rot=0, just="center"):
    t = pcbnew.PCB_TEXT(b)
    t.SetText(s); t.SetPosition(VECTOR2I(mm(x), mm(y)))
    t.SetLayer(layer)
    t.SetTextSize(VECTOR2I(mm(size), mm(size)))
    t.SetTextThickness(mm(size * (0.2 if bold else 0.15)))
    t.SetTextAngleDegrees(rot)
    if just == "left": t.SetHorizJustify(pcbnew.GR_TEXT_H_ALIGN_LEFT)
    if just == "right": t.SetHorizJustify(pcbnew.GR_TEXT_H_ALIGN_RIGHT)
    b.Add(t)

S = pcbnew.F_SilkS
# NodeMCU outlines (approximate module bodies) and labels
P15Y = P1Y + 14 * 2.54
rect(S, MCU_CX - 15.5, P1Y - 11.0, MCU_CX + 15.5, P15Y + 2.0, 0.15)      # v3 body (antenna overhangs keepout)
text("NodeMCU v2 or v3 (ESP8266)", MCU_CX, 8.0, 1.2, bold=True)
text("v2: inner rows   v3: outer rows", MCU_CX, 10.6, 0.9)
text("ANTENNA: no copper", MCU_CX, 13.0, 0.9)
text("A0", MCU_CX - 8.6, P1Y, 0.9)
text("D0", MCU_CX + 8.6, P1Y, 0.9)
text("VIN", MCU_CX - 8.0, P15Y, 0.9)
text("USB this edge", MCU_CX, P15Y - 2.6, 0.9)
for i, lab in enumerate(["D0", "D1", "D2", "D3", "D4", "3V3", "GND", "D5", "D6", "D7", "D8", "RX", "TX", "GND", "3V3"]):
    if i in (0, 14): continue
    text(lab, MCU_CX + 8.6, P1Y + i * 2.54, 0.8)
for i, lab in enumerate(["A0", "RSV", "RSV", "SD3", "SD2", "SD1", "CMD", "SD0", "CLK", "GND", "3V3", "EN", "RST", "GND", "VIN"]):
    if i in (0, 14): continue
    text(lab, MCU_CX - 8.6, P1Y + i * 2.54, 0.8)

text("LightLink ESP8266", 67.0, 20.5, 1.3, bold=True)
text("interface " + VERSION, 67.0, 22.8, 1.0)
text("74HCT245 x2, 5V logic", 67.0, 25.0, 0.8)
text("J1 TOP band", 84.0 + 1.27, 1.5, 1.0, bold=True)
text("J2 BOTTOM band", 84.0 + 1.27, 30.5, 1.0, bold=True)
text("JP1 VIN LINK", 33.6, 44.6, 0.8, bold=True, just="left")
text("off for USB", 33.6, 49.8, 0.8, just="left")
text("J3", 58.3, 52.0, 1.0, bold=True, just="right")
text("+5V", 62.0, 52.0, 1.0, bold=True)
text("GND", 67.08, 52.0, 1.0, bold=True)
text("D1 PWR", 89.3, 53.4, 0.8)

b.Save(OUT)
print("saved", OUT, "P1Y", round(P1Y, 2))
