"""Pod case Rev B: lower tray + upper bezel, screwed together from underneath (heads hidden
under the rubber feet).  CadQuery.  All mm.

Coordinates (board frame): X 0..BW left->right as Ash holds it (Pico USB on the right),
Y 0..BL bottom->top (Pico end at the bottom, headphone jack at the top),
Z up from the OUTSIDE of the tray floor.  Case coords = board coords + (OFF_X, OFF_Y).
"""
import cadquery as cq, sys, os, math

# ---------------- board + stack (video: Pico sits flat on the perfboard, screen on 8.5 mm sockets)
BW, BL = 52.0, 109.0
GAP = 0.8            # clearance each side of the board (its corners are sharp)
WALL = 1.8
FLOOR = 1.6
UNDER = 3.0          # wire / lead clearance under the board (press the loops flat, trim leads)
PCB = 1.6
SOCKET = 8.5         # perfboard top -> screen PCB bottom (the two 1x14 sockets)
SCR_PCB = 1.6
GLASS = 4.0          # screen PCB top -> touch-panel top
BEZEL_OVER = 1.5     # bezel plate thickness over the glass
END = 5.4            # extra cavity length at each end: screw blocks + jack / wire room
FIT = 0.2

Z_BOARD_BOT = FLOOR + UNDER            # 4.6
Z_BOARD_TOP = Z_BOARD_BOT + PCB        # 6.2
Z_SCR_BOT = Z_BOARD_TOP + SOCKET       # 14.7
Z_SCR_TOP = Z_SCR_BOT + SCR_PCB        # 16.3
Z_GLASS = Z_SCR_TOP + GLASS            # 20.3
TRAY_H = Z_GLASS                       # tray rim level with the glass; bezel is a plate on top
TOTAL_H = TRAY_H + BEZEL_OVER          # 21.8

IW, IL = BW + 2 * GAP, BL + 2 * GAP + 2 * END
OW, OL = IW + 2 * WALL, IL + 2 * WALL
R_OUT = 3.0
OFF_X, OFF_Y = WALL + GAP, WALL + GAP + END

# ---------------- screws: 4 corner blocks in the tray, pegs on the bezel, M2 x 8 from below
BLK_X, BLK_Y = 6.2, 6.6               # block footprint from the outer faces (merged into the corner walls)
BLK_TOP = TRAY_H - 1.3                # below the bezel's locating lip
PEG_D, BORE_D = 3.4, 3.8              # bezel peg / tray bore
PILOT_D, PILOT_DEEP = 1.6, 7.0        # pilot hole in the peg for an M2 self-tapping screw
SCREW_CLR = 2.3                       # through-hole in the floor
CSK_D, CSK_H = 4.4, 1.1               # countersink for the screw head
FOOT_D, FOOT_H = 7.0, 0.5             # shallow recess for a stick-on rubber foot over the head
SCREW_IN_X, SCREW_IN_Y = 3.9, 4.2     # screw axis from the outer faces (2.0 / 2.3 mm of plastic outside the bore)
def screw_points():
    return [(SCREW_IN_X, SCREW_IN_Y), (OW - SCREW_IN_X, SCREW_IN_Y), (SCREW_IN_X, OL - SCREW_IN_Y), (OW - SCREW_IN_X, OL - SCREW_IN_Y)]

# ---------------- features (board frame unless noted)
# screen module (video): bottom edge ~7 mm below its pin row (row 10, y 22.9) -> y 15..101; glass y ~25.5..94.5
SCR_Y0, SCR_Y1 = 15.0, 101.0
WIN_W, WIN_H = 46.0, 60.0            # bezel window (2.8" active area 43.2 x 57.6 + ~1.3 mm margin)
WIN_CX, WIN_CY = BW / 2, 60.0
# Pico micro-USB, right wall.  Pico flat on the board: socket z = board top + 1.0 .. + 3.6
PICO_USB_Y, PICO_USB_W, PICO_USB_H = 10.2, 12.0, 8.0
PICO_USB_ZC = Z_BOARD_TOP + 2.3
# headphone jack, top wall, ROUND.  Jack centre JACK_H above the perfboard (DAC on bent pins / header).
JACK_X, JACK_D, JACK_H = 12.5, 8.2, 8.0
DAC_X0, DAC_W, DAC_OVER = 3.8, 17.5, 3.5   # GY-PCM5102 footprint x 3.8..21.3, jack end 3.5 past the board edge
JACK_ZC = Z_BOARD_TOP + JACK_H
# TP4056 Type-C (29 x 17.3 x 4.3) lies on the board under the screen, right side, socket out the RIGHT wall
TPC_X0, TPC_Y0 = 34.7, 74.0           # module footprint x 34.7..52, y 74..103
TPC_USB_Y, TPC_USB_W, TPC_USB_H = TPC_Y0 + 14.5, 11.0, 7.0
TPC_USB_ZC = Z_BOARD_TOP + 3.6        # tape 1.0 + PCB 1.0 + socket 3.2/2
# microSD (the screen module's own holder, under the screen PCB, left side)
SD_Y0, SD_Y1 = 58.0, 82.0
SD_Z0, SD_Z1 = Z_SCR_BOT - 3.5, Z_SCR_BOT + 1.0
# right-angle slide switch 13 x 6.5 x 5.5 at the board's left edge, lever out the LEFT wall
SW_Y, SW_W, SW_Z0, SW_Z1 = 31.0, 10.0, Z_BOARD_TOP + 0.8, Z_BOARD_TOP + 5.3
# battery 403048 (30 x 48 x 4) between board and screen
BAT_X0, BAT_Y0, BAT_W, BAT_L = 19.0, 25.0, 30.0, 48.0
PORT_RECESS = 1.0                     # outside recess around USB ports so plugs seat fully

def box_xy(w, l, h, r):
    return cq.Workplane("XY").box(w, l, h, centered=(True, True, False)).edges("|Z").fillet(r)

def side_cutter(side, c, w, h, zc, r=1.2, depth=WALL + 2, outset=1.0):
    """rounded-rectangle hole through a side wall. side L/R: c along Y; T/B: c along X. zc = centre height"""
    if side in ("L", "R"):
        cut = cq.Workplane("YZ").center(c, zc).rect(w, h).extrude(depth).edges("|X").fillet(r)
        x = -outset if side == "L" else OW - depth + outset
        return cut.translate((x, 0, 0))
    cut = cq.Workplane("XZ").center(c, zc).rect(w, h).extrude(-depth).edges("|Y").fillet(r)  # XZ normal is -Y; extrude(-d) goes +Y
    y = -outset if side == "B" else OL - depth + outset
    return cut.translate((0, y, 0))

def recess(side, c, w, h, zc, deep=PORT_RECESS):
    return side_cutter(side, c, w + 3.0, h + 3.0, zc, r=2.0, depth=deep + 1.0, outset=1.0)

def tray():
    outer = box_xy(OW, OL, TRAY_H, R_OUT).translate((OW / 2, OL / 2, 0))
    t = outer.cut(box_xy(IW, IL, TRAY_H, R_OUT - WALL).translate((OW / 2, OL / 2, FLOOR)))
    # board support rails (segments avoid the Pico / DAC solder stubs in columns A and T)
    rail_w = 2.4                                        # from the wall face over the board edge by 1.6
    segs_left = [(3.5, 17.5), (22.0, 84.0), (101.5, 109.0)]
    segs_right = [(3.5, 17.5), (22.0, 109.0)]
    for (x0, segs) in ((WALL, segs_left), (OW - WALL - rail_w, segs_right)):
        for (y0, y1) in segs:
            t = t.union(cq.Workplane("XY").box(rail_w, y1 - y0, UNDER, centered=False)
                        .translate((x0, OFF_Y + y0, FLOOR)))
    # corner screw blocks with bores
    for (cx, cy) in screw_points():
        bx = BLK_X / 2 if cx < OW / 2 else OW - BLK_X / 2
        by = BLK_Y / 2 if cy < OL / 2 else OL - BLK_Y / 2
        blk = cq.Workplane("XY").box(BLK_X, BLK_Y, BLK_TOP - FLOOR, centered=(True, True, False)).translate((bx, by, FLOOR))
        t = t.union(blk.intersect(outer))
        t = t.cut(cq.Workplane("XY").circle(BORE_D / 2).extrude(TRAY_H).translate((cx, cy, FLOOR)))
        t = t.cut(cq.Workplane("XY").circle(SCREW_CLR / 2).extrude(FLOOR + 1).translate((cx, cy, -0.5)))
        csk = cq.Workplane("XY").circle(CSK_D / 2).workplane(offset=CSK_H).circle(SCREW_CLR / 2).loft()
        t = t.cut(csk.translate((cx, cy, 0)))
        t = t.cut(cq.Workplane("XY").circle(FOOT_D / 2).extrude(FOOT_H + 0.5).translate((cx, cy, -0.5)))
    # openings
    t = t.cut(side_cutter("R", OFF_Y + PICO_USB_Y, PICO_USB_W, PICO_USB_H, PICO_USB_ZC))
    t = t.cut(recess("R", OFF_Y + PICO_USB_Y, PICO_USB_W, PICO_USB_H, PICO_USB_ZC))
    t = t.cut(side_cutter("R", OFF_Y + TPC_USB_Y, TPC_USB_W, TPC_USB_H, TPC_USB_ZC, r=2.0))
    t = t.cut(recess("R", OFF_Y + TPC_USB_Y, TPC_USB_W, TPC_USB_H, TPC_USB_ZC))
    jack = cq.Workplane("XZ").center(OFF_X + JACK_X, JACK_ZC).circle(JACK_D / 2).extrude(-(WALL + 2)).translate((0, OL - WALL - 1, 0))
    t = t.cut(jack)
    jack_cb = cq.Workplane("XZ").center(OFF_X + JACK_X, JACK_ZC).circle(JACK_D / 2 + 1.3).extrude(-1.0).translate((0, OL - WALL - 0.001 - 0.0, 0))
    t = t.cut(jack_cb.translate((0, -0.0, 0)))                     # inside counterbore: jack barrel can enter the wall
    t = t.cut(side_cutter("L", OFF_Y + (SD_Y0 + SD_Y1) / 2, SD_Y1 - SD_Y0, SD_Z1 - SD_Z0, (SD_Z0 + SD_Z1) / 2, r=1.5))
    t = t.cut(side_cutter("L", OFF_Y + SW_Y, SW_W, SW_Z1 - SW_Z0, (SW_Z0 + SW_Z1) / 2, r=1.0))
    return t

def bezel():
    b = box_xy(OW, OL, BEZEL_OVER, R_OUT).translate((OW / 2, OL / 2, TRAY_H))
    # locating lip 1.0 wide x 1.2 deep inside the tray wall
    lip_o = box_xy(IW - 2 * FIT, IL - 2 * FIT, 1.2, R_OUT - WALL).translate((OW / 2, OL / 2, TRAY_H - 1.2))
    lip_i = box_xy(IW - 2 * FIT - 2.0, IL - 2 * FIT - 2.0, 1.4, R_OUT - WALL - 1.0).translate((OW / 2, OL / 2, TRAY_H - 1.3))
    b = b.union(lip_o.cut(lip_i))
    # window with a small chamfer on the outside
    win = cq.Workplane("XY").rect(WIN_W, WIN_H).extrude(BEZEL_OVER + 2).edges("|Z").fillet(2.0).translate((OFF_X + WIN_CX, OFF_Y + WIN_CY, TRAY_H - 1))
    b = b.cut(win)
    try:
        b = b.faces(">Z").edges("not %CIRCLE").edges(cq.selectors.BoxSelector((OFF_X + WIN_CX - WIN_W, OFF_Y + WIN_CY - WIN_H, TOTAL_H - 0.1), (OFF_X + WIN_CX + WIN_W, OFF_Y + WIN_CY + WIN_H, TOTAL_H + 0.1))).chamfer(0.5)
    except Exception:
        pass
    # four pegs down into the tray's corner bores, pilot hole for the screw in the end
    peg_len = TRAY_H - FLOOR - 0.5
    for (cx, cy) in screw_points():
        peg = cq.Workplane("XY").circle(PEG_D / 2).extrude(peg_len).translate((cx, cy, TRAY_H - peg_len))
        b = b.union(peg)
        b = b.cut(cq.Workplane("XY").circle(PILOT_D / 2).extrude(PILOT_DEEP + 0.5).translate((cx, cy, TRAY_H - peg_len - 0.5)))
    return b

if __name__ == "__main__":
    out = sys.argv[1]; os.makedirs(out, exist_ok=True)
    t, b = tray(), bezel()
    cq.exporters.export(t, f"{out}/pod_tray.step"); cq.exporters.export(t, f"{out}/pod_tray.stl", tolerance=0.02, angularTolerance=0.1)
    cq.exporters.export(b, f"{out}/pod_bezel.step"); cq.exporters.export(b, f"{out}/pod_bezel.stl", tolerance=0.02, angularTolerance=0.1)
    cq.exporters.export(t.union(b), f"{out}/pod_case_assembled.step")
    print(f"outer {OW:.1f} x {OL:.1f} x {TOTAL_H:.1f}  tray_h {TRAY_H:.1f}  Zglass {Z_GLASS:.1f}  jack z {JACK_ZC:.1f}")
