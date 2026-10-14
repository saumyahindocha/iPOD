"""Pod case: lower tray + upper bezel. CadQuery. All mm.
Coordinates: X 0..BW left->right as Ash holds it (Pico USB on the right),
Y 0..BL bottom->top (Pico end at the bottom, headphone jack at the top),
Z up from the OUTSIDE of the tray floor.
"""
import cadquery as cq, sys, os

# ---------------- board + stack (measured / planned)
BW, BL = 52.0, 109.0          # perfboard
GAP = 0.6                      # clearance each side around the board
WALL = 2.0
FLOOR = 2.0
UNDER = 3.5                    # solder-side wire clearance under the board
PCB = 1.6                      # perfboard thickness
SOCKET = 8.5                   # screen sockets (perfboard top -> screen PCB bottom)
SCR_PCB = 1.6
GLASS = 4.0                    # screen PCB top -> touch panel top
BEZEL_OVER = 1.5               # bezel material above the glass
LIP_H = 3.0                    # bezel lip that drops inside the tray
FIT = 0.25                     # lip clearance

Z_BOARD_BOT = FLOOR + UNDER                 # 5.5
Z_BOARD_TOP = Z_BOARD_BOT + PCB             # 7.1
Z_SCR_BOT = Z_BOARD_TOP + SOCKET            # 15.6
Z_SCR_TOP = Z_SCR_BOT + SCR_PCB             # 17.2
Z_GLASS = Z_SCR_TOP + GLASS                 # 21.2
TRAY_H = Z_SCR_TOP + 1.0                    # tray rim just above the screen PCB: 18.2
BEZEL_H = Z_GLASS + BEZEL_OVER - TRAY_H     # visible bezel height above the rim
TOTAL_H = TRAY_H + BEZEL_H

IW, IL = BW + 2 * GAP, BL + 2 * GAP         # inner cavity
OW, OL = IW + 2 * WALL, IL + 2 * WALL       # outer
R_OUT = 4.0                                 # corner radius

# ---------------- features (user frame)
# screen module: x 1..51 (centred), y 23..109. Window = touch-panel area.
WIN_W, WIN_H = 45.0, 60.0
WIN_CX, WIN_CY = BW / 2, 66.5
# Pico micro-USB: right wall, centred on the Pico's width
PICO_USB_Y, PICO_USB_W = 11.5, 12.0
PICO_USB_Z0 = Z_SCR_BOT - 1.0               # plug overmold: 1 mm below the Pico's underside
PICO_USB_Z1 = PICO_USB_Z0 + 6.8
# headphone jack: top wall, left side
JACK_X, JACK_W = 10.0, 9.0
JACK_Z0, JACK_Z1 = Z_BOARD_TOP + 3.0, Z_BOARD_TOP + 12.0
# TP4056 USB-C: top wall, right side
TPC_X, TPC_W = 38.5, 12.0
TPC_Z0, TPC_Z1 = Z_BOARD_TOP + 1.0, Z_BOARD_TOP + 7.0
# microSD: left wall (the module's card holder faces the perfboard, card goes in from the side)
SD_Y0, SD_Y1 = 59.0, 81.0
SD_Z0, SD_Z1 = Z_SCR_BOT - 3.2, Z_SCR_BOT + 0.3
# power switch: left wall, between the Pico strip and the screen
SW_Y, SW_W = 26.0, 9.0
SW_Z0, SW_Z1 = Z_BOARD_TOP + 2.0, Z_BOARD_TOP + 6.0

def rrect(w, l, r):
    return cq.Workplane("XY").rect(w, l).extrude(1).edges("|Z").fillet(r).faces(">Z").wires().toPending()

def box_xy(w, l, h, r):
    return cq.Workplane("XY").rect(w, l).extrude(h).edges("|Z").fillet(r)

def cut_wall_slot(solid, side, c, w, z0, z1, through=WALL + 1.0):
    """Rectangular opening in an outer wall. side: 'L','R','T','B'. c = centre along the wall."""
    h = z1 - z0
    if side in ("L", "R"):
        x = -through / 2 if side == "L" else OW - through / 2  # wall thickness region
        cutter = cq.Workplane("XY").box(through + 2, w, h).translate((x + through / 2, c, z0 + h / 2))
    else:
        y = -through / 2 if side == "B" else OL - through / 2
        cutter = cq.Workplane("XY").box(w, through + 2, h).translate((c, y + through / 2, z0 + h / 2))
    return solid.cut(cutter)

# Place everything so the BOARD origin (0,0) sits at (WALL+GAP, WALL+GAP) in case coords.
OFF = WALL + GAP

def tray():
    outer = box_xy(OW, OL, TRAY_H, R_OUT).translate((OW / 2, OL / 2, 0))
    inner = box_xy(IW, IL, TRAY_H, R_OUT - WALL).translate((OW / 2, OL / 2, FLOOR))
    t = outer.cut(inner)
    # board support rails along both long walls and both short walls (board rests at Z_BOARD_BOT)
    rail_w = 1.6
    for x in (WALL, OW - WALL - rail_w):
        t = t.union(cq.Workplane("XY").box(rail_w, IL - 2, UNDER).translate((x + rail_w / 2, OL / 2, FLOOR + UNDER / 2)))
    for y in (WALL, OL - WALL - rail_w):
        t = t.union(cq.Workplane("XY").box(IW - 2, rail_w, UNDER).translate((OW / 2, y + rail_w / 2, FLOOR + UNDER / 2)))
    # four corner pads to keep the board from sliding (board corners sit between them)
    # openings
    t = cut_wall_slot(t, "R", OFF + PICO_USB_Y, PICO_USB_W, PICO_USB_Z0, TRAY_H + 1)      # notch, bezel closes it
    t = cut_wall_slot(t, "T", OFF + JACK_X, JACK_W, JACK_Z0, JACK_Z1)
    t = cut_wall_slot(t, "T", OFF + TPC_X, TPC_W, TPC_Z0, TPC_Z1)
    t = cut_wall_slot(t, "L", OFF + (SD_Y0 + SD_Y1) / 2, SD_Y1 - SD_Y0, SD_Z0, SD_Z1)
    t = cut_wall_slot(t, "L", OFF + SW_Y, SW_W, SW_Z0, SW_Z1)
    # snap recesses: 6 small pockets on the inner wall just below the rim for the bezel's bumps
    for (x, y, ax) in snap_points():
        pocket = cq.Workplane("XY").box(3.0 if ax == "y" else 1.0, 1.0 if ax == "y" else 3.0, 1.2)
        t = t.cut(pocket.translate((x, y, TRAY_H - LIP_H + 1.0)))
    return t

def snap_points():
    pts = []
    for y in (OL * 0.25, OL * 0.75):
        pts.append((WALL + 0.4, y, "x")); pts.append((OW - WALL - 0.4, y, "x"))
    pts.append((OW * 0.5, WALL + 0.4, "y")); pts.append((OW * 0.5, OL - WALL - 0.4, "y"))
    return pts

def bezel():
    # cap: outer shell from TRAY_H up, plus a lip that drops LIP_H into the tray
    cap = box_xy(OW, OL, BEZEL_H, R_OUT).translate((OW / 2, OL / 2, TRAY_H))
    lip = box_xy(IW - 2 * FIT, IL - 2 * FIT, LIP_H, R_OUT - WALL).translate((OW / 2, OL / 2, TRAY_H - LIP_H))
    lip_hollow = box_xy(IW - 2 * FIT - 2 * 1.6, IL - 2 * FIT - 2 * 1.6, LIP_H + 1, 1.0).translate((OW / 2, OL / 2, TRAY_H - LIP_H - 0.5))
    b = cap.union(lip.cut(lip_hollow))
    # hollow under the cap down to the glass top (leave BEZEL_OVER of material on top)
    cavity = box_xy(IW - 2 * FIT - 0.01, IL - 2 * FIT - 0.01, TOTAL_H - BEZEL_OVER - (TRAY_H - LIP_H), R_OUT - WALL)
    b = b.cut(cavity.translate((OW / 2, OL / 2, TRAY_H - LIP_H - 0.001)))
    # screen window with a small chamfer
    win = cq.Workplane("XY").rect(WIN_W, WIN_H).extrude(BEZEL_OVER + 2).edges("|Z").fillet(2.0)
    b = b.cut(win.translate((OFF + WIN_CX, OFF + WIN_CY, TOTAL_H - BEZEL_OVER - 1)))
    # close the Pico USB notch: the bezel wall comes down there (lip already does, since the notch is cut only in the tray)
    # bumps on the lip matching the tray pockets
    for (x, y, ax) in snap_points():
        bump = cq.Workplane("XY").box(2.6 if ax == "y" else 0.7, 0.7 if ax == "y" else 2.6, 1.0)
        dx = 0.35 if ax == "x" and x < OW / 2 else (-0.35 if ax == "x" else 0)
        dy = 0.35 if ax == "y" and y < OL / 2 else (-0.35 if ax == "y" else 0)
        b = b.union(bump.translate((x + dx, y + dy, TRAY_H - LIP_H + 1.0)))
    # pass-through for the Pico USB plug in the bezel lip/wall
    b = cut_wall_slot(b, "R", OFF + PICO_USB_Y, PICO_USB_W, PICO_USB_Z0, PICO_USB_Z1, through=WALL + 2.5)
    # pass-through for the jack (if taller than the tray rim) and USB-C (never reaches the rim) - jack top
    if JACK_Z1 > TRAY_H - LIP_H:
        b = cut_wall_slot(b, "T", OFF + JACK_X, JACK_W, JACK_Z0, JACK_Z1, through=WALL + 2.5)
    return b

if __name__ == "__main__":
    out = sys.argv[1]; os.makedirs(out, exist_ok=True)
    t, b = tray(), bezel()
    cq.exporters.export(t, f"{out}/pod_tray.step"); cq.exporters.export(t, f"{out}/pod_tray.stl")
    cq.exporters.export(b, f"{out}/pod_bezel.step"); cq.exporters.export(b, f"{out}/pod_bezel.stl")
    asm = t.union(b)
    cq.exporters.export(asm, f"{out}/pod_case_assembled.step")
    # previews
    for name, obj, d in (("tray_iso", t, (1, -1, 0.8)), ("tray_top", t, (0, 0, 1)), ("bezel_iso", b, (1, -1, 0.8)),
                         ("bezel_under", b, (0, 0, -1)), ("asm_iso", asm, (1, -1, 0.8)), ("asm_right", asm, (1, 0, 0)),
                         ("asm_top_end", asm, (0, 1, 0)), ("asm_left", asm, (-1, 0, 0))):
        cq.exporters.export(obj, f"{out}/{name}.svg", opt={"projectionDir": d, "width": 900, "height": 700,
                                                           "showHidden": False, "strokeWidth": 0.4})
    print("outer", OW, OL, TOTAL_H, "tray_h", TRAY_H, "bezel_h", BEZEL_H, "Zglass", Z_GLASS)
