import sys, matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, FancyBboxPatch, Circle
sys.path.insert(0, ".")
import pod_case as P
out = sys.argv[1]

def dim(ax, x0, x1, y, text, off=2.5, color="#555"):
    ax.annotate("", (x0, y), (x1, y), arrowprops=dict(arrowstyle="<->", color=color, lw=0.8))
    ax.text((x0 + x1) / 2, y + off, text, ha="center", va="bottom", fontsize=7, color=color)
def vdim(ax, x, y0, y1, text, off=1.5, color="#555"):
    ax.annotate("", (x, y0), (x, y1), arrowprops=dict(arrowstyle="<->", color=color, lw=0.8))
    ax.text(x + off, (y0 + y1) / 2, text, ha="left", va="center", fontsize=7, color=color, rotation=90)

ox, oy = P.OFF_X, P.OFF_Y
fig, axs = plt.subplots(2, 3, figsize=(11.7, 8.3))
ax = axs[0, 0]; ax.set_title("Plan, looking into the tray (board coordinates)", fontsize=9)
ax.add_patch(FancyBboxPatch((-ox, -oy), P.OW, P.OL, boxstyle="round,pad=0,rounding_size=3", fc="#eef", ec="#333", lw=1))
ax.add_patch(Rectangle((0, 0), P.BW, P.BL, fc="#d9b88a", ec="#7a5a2a", lw=0.8))
ax.add_patch(Rectangle((1, P.SCR_Y0), 50, 86, fc="none", ec="#c0392b", lw=0.8, ls="--")); ax.text(26, 97, "screen module 50 × 86", ha="center", fontsize=6.5, color="#c0392b")
ax.add_patch(Rectangle((0.5, -0.3), 51, 21, fc="#cfe8d5", ec="#2e8b57", lw=0.8)); ax.text(26, 10, "Pico 2 W (USB →)", ha="center", va="center", fontsize=7, color="#2e8b57")
ax.add_patch(Rectangle((P.BAT_X0, P.BAT_Y0), P.BAT_W, P.BAT_L, fc="#fde9b0", ec="#b8860b", lw=0.8)); ax.text(34, 49, "battery 403048\n30 × 48 × 4\n(under the screen)", ha="center", va="center", fontsize=6.5, color="#8a6a00")
ax.add_patch(Rectangle((P.DAC_X0, P.BL + P.DAC_OVER - 31), P.DAC_W, 31, fc="#e3d4f2", ec="#6a3d9a", lw=0.8)); ax.text(P.DAC_X0 + P.DAC_W / 2, 95, "DAC\nGY-PCM5102\njack ↑", ha="center", va="center", fontsize=6.5, color="#6a3d9a")
ax.add_patch(Rectangle((P.TPC_X0, P.TPC_Y0), 17.3, 29, fc="#cfe3f7", ec="#1f5fa8", lw=0.8)); ax.text(P.TPC_X0 + 8.6, P.TPC_Y0 + 14.5, "TP4056\nUSB-C →", ha="center", va="center", fontsize=6.5, color="#1f5fa8")
ax.add_patch(Rectangle((21.5, 103.5), 10.2, 2.6, fc="#333", ec="#000", lw=0.5)); ax.text(26.6, 100, "SD socket", ha="center", fontsize=5.5, color="#333")
ax.add_patch(Rectangle((0, P.SW_Y - 6.5), 6.5, 13, fc="#ddd", ec="#555", lw=0.6)); ax.text(-3, P.SW_Y, "switch", ha="right", va="center", fontsize=6, color="#555")
ax.add_patch(Rectangle((1, P.SD_Y0), 18, P.SD_Y1 - P.SD_Y0, fc="none", ec="#999", lw=0.6, ls=":")); ax.text(10, 70, "SD holder\n(on screen)", ha="center", va="center", fontsize=6, color="#777")
ax.add_patch(Rectangle((P.WIN_CX - P.WIN_W/2, P.WIN_CY - P.WIN_H/2), P.WIN_W, P.WIN_H, fc="none", ec="#1f5fa8", lw=1.4)); ax.text(26, 33, f"bezel window {P.WIN_W:g} × {P.WIN_H:g}", ha="center", va="center", fontsize=7, color="#1f5fa8")
for (cx, cy) in P.screw_points():
    ax.add_patch(Circle((cx - ox, cy - oy), P.BORE_D / 2, fc="#fff", ec="#000", lw=0.7))
ax.text(P.BW / 2, -oy - 5, "screws ×4 from underneath", ha="center", fontsize=6.5)
ax.set_xlim(-10, P.BW + 10); ax.set_ylim(-14, P.BL + 12); ax.set_aspect("equal"); ax.axis("off")
dim(ax, -ox, P.BW + ox, -oy - 1.5, f"{P.OW:.1f} outer (board 52)", off=-5.5)
vdim(ax, P.BW + ox + 1.5, -oy, P.BL + oy, f"{P.OL:.1f} outer (board 109)")

def wall(ax, title, length, slots, circles=()):
    ax.set_title(title, fontsize=9)
    ax.add_patch(Rectangle((0, 0), length, P.TRAY_H, fc="#dde", ec="#333", lw=1))
    ax.add_patch(Rectangle((0, P.TRAY_H), length, P.BEZEL_OVER, fc="#9aa", ec="#333", lw=1))
    ax.axhline(P.Z_BOARD_BOT, color="#b8860b", lw=0.6, ls="--"); ax.text(length + 1, P.Z_BOARD_BOT, "board", fontsize=6, va="center", color="#b8860b")
    ax.axhline(P.Z_SCR_BOT, color="#c0392b", lw=0.6, ls="--"); ax.text(length + 1, P.Z_SCR_BOT, "screen PCB", fontsize=6, va="center", color="#c0392b")
    for (c, w, zc, h, lab) in slots:
        ax.add_patch(FancyBboxPatch((c - w/2, zc - h/2), w, h, boxstyle="round,pad=0,rounding_size=1.2", fc="#fff", ec="#1f5fa8", lw=1))
        ax.text(c, zc + h/2 + 0.8, lab, ha="center", va="bottom", fontsize=6.5, color="#1f5fa8")
        dim(ax, c - w/2, c + w/2, -2.5, f"{w:g}", off=-3.5)
        ax.text(c, -8.5, f"centre {c:.1f}", ha="center", fontsize=6, color="#555")
        vdim(ax, c + w/2 + 0.5, zc - h/2, zc + h/2, f"z {zc-h/2:.1f}–{zc+h/2:.1f}", off=0.6)
    for (c, d, zc, lab) in circles:
        ax.add_patch(Circle((c, zc), d/2, fc="#fff", ec="#1f5fa8", lw=1))
        ax.text(c, zc + d/2 + 0.8, lab, ha="center", va="bottom", fontsize=6.5, color="#1f5fa8")
        ax.text(c, -8.5, f"centre {c:.1f}", ha="center", fontsize=6, color="#555")
        ax.text(c + d/2 + 0.6, zc, f"Ø{d:g}, z {zc:.1f}", fontsize=6.5, va="center", color="#555")
    ax.set_xlim(-3, length + 16); ax.set_ylim(-11, P.TOTAL_H + 6); ax.set_aspect("equal"); ax.axis("off")

wall(axs[0, 1], "Top end wall (jack), from outside", P.OW, [], [(ox + P.JACK_X, P.JACK_D, P.JACK_ZC, "3.5 mm jack")])
wall(axs[0, 2], "Right wall (Pico USB, USB-C charge), from outside", P.OL,
     [(oy + P.PICO_USB_Y, P.PICO_USB_W, P.PICO_USB_ZC, P.PICO_USB_H, "Pico micro-USB"), (oy + P.TPC_USB_Y, P.TPC_USB_W, P.TPC_USB_ZC, P.TPC_USB_H, "USB-C charge")])
wall(axs[1, 0], "Left wall (switch, microSD), from outside (mirror of plan)", P.OL,
     [(P.OL - (oy + (P.SD_Y0 + P.SD_Y1)/2), P.SD_Y1 - P.SD_Y0, (P.SD_Z0 + P.SD_Z1)/2, P.SD_Z1 - P.SD_Z0, "microSD"), (P.OL - (oy + P.SW_Y), P.SW_W, (P.SW_Z0 + P.SW_Z1)/2, P.SW_Z1 - P.SW_Z0, "power switch")])
ax = axs[1, 1]; ax.set_title("Stack (section), mm from the outside of the floor", fontsize=9)
layers = [("floor", 0, P.FLOOR, "#aaa"), ("wire clearance", P.FLOOR, P.Z_BOARD_BOT, "#fff"), ("perfboard", P.Z_BOARD_BOT, P.Z_BOARD_TOP, "#d9b88a"),
          ("sockets / Pico / battery / TP4056", P.Z_BOARD_TOP, P.Z_SCR_BOT, "#fff"), ("screen PCB", P.Z_SCR_BOT, P.Z_SCR_TOP, "#f4b6b6"),
          ("LCD + touch glass", P.Z_SCR_TOP, P.Z_GLASS, "#cde"), ("bezel plate", P.Z_GLASS, P.TOTAL_H, "#9aa")]
for name, z0, z1, c in layers:
    ax.add_patch(Rectangle((0, z0), 30, z1 - z0, fc=c, ec="#333", lw=0.6)); ax.text(31, (z0 + z1)/2, f"{name}  ({z0:.1f}–{z1:.1f})", fontsize=7, va="center")
ax.axhline(P.TRAY_H, color="#1f5fa8", lw=1); ax.text(-1, P.TRAY_H, f"tray rim {P.TRAY_H:.1f}", fontsize=7, ha="right", va="center", color="#1f5fa8")
ax.set_xlim(-14, 80); ax.set_ylim(-2, P.TOTAL_H + 3); ax.set_aspect("equal"); ax.axis("off")
ax = axs[1, 2]; ax.axis("off")
ax.text(0, 1, "Key numbers", fontsize=9, weight="bold", va="top")
txt = f"""Outer: {P.OW:.1f} × {P.OL:.1f} × {P.TOTAL_H:.1f} mm, corner radius {P.R_OUT:g}
Walls {P.WALL:g}, floor {P.FLOOR:g}, bezel plate {P.BEZEL_OVER:g}
Board clearance {P.GAP:g} per side, {P.END:g} at each end
Wire clearance under the board {P.UNDER:g}
Tray rim at the glass top ({P.TRAY_H:.1f}); bezel lip 1.0 × 1.2
Four screw blocks in the corners, bore Ø{P.BORE_D:g};
bezel pegs Ø{P.PEG_D:g}, pilot Ø{P.PILOT_D:g} × {P.PILOT_DEEP:g} for M2 × 8
self-tapping screws from below, countersunk,
covered by Ø8 rubber feet (recess Ø{P.FOOT_D:g} × {P.FOOT_H:g})
Jack hole Ø{P.JACK_D:g}, centre {P.JACK_H:g} above the perfboard
Window {P.WIN_W:g} × {P.WIN_H:g}, centre ({P.WIN_CX:g}, {P.WIN_CY:g}) board coords
USB ports: 1.0 mm outside recess so plugs seat fully"""
ax.text(0, 0.9, txt, fontsize=7.2, va="top", family="monospace")
fig.tight_layout(); fig.savefig(f"{out}/drawings.png", dpi=170); plt.close(fig)

# 1:1 templates (A4): bezel outline + window; floor outline with screw holes
fig = plt.figure(figsize=(8.27, 11.69)); ax = fig.add_axes([0, 0, 1, 1]); ax.set_xlim(0, 210); ax.set_ylim(0, 297); ax.set_aspect("equal"); ax.axis("off")
x0, y0 = 25, 120
ax.add_patch(FancyBboxPatch((x0, y0), P.OW, P.OL, boxstyle="round,pad=0,rounding_size=3", fc="none", ec="#000", lw=0.8))
ax.add_patch(Rectangle((x0 + ox, y0 + oy), P.BW, P.BL, fc="none", ec="#999", lw=0.5, ls="--"))
ax.add_patch(Rectangle((x0 + ox + P.WIN_CX - P.WIN_W/2, y0 + oy + P.WIN_CY - P.WIN_H/2), P.WIN_W, P.WIN_H, fc="none", ec="#1f5fa8", lw=1))
ax.add_patch(Rectangle((x0 + ox + 1, y0 + oy + P.SCR_Y0), 50, 86, fc="none", ec="#c0392b", lw=0.5, ls=":"))
ax.text(x0 + P.OW / 2, y0 + P.OL + 3, "BEZEL (top)", ha="center", fontsize=8)
x1 = 120
ax.add_patch(FancyBboxPatch((x1, y0), P.OW, P.OL, boxstyle="round,pad=0,rounding_size=3", fc="none", ec="#000", lw=0.8))
ax.add_patch(Rectangle((x1 + ox, y0 + oy), P.BW, P.BL, fc="none", ec="#999", lw=0.5, ls="--"))
for (cx, cy) in P.screw_points():
    ax.add_patch(Circle((x1 + P.OW - cx, y0 + cy), P.SCREW_CLR / 2, fc="none", ec="#000", lw=0.6))
    ax.add_patch(Circle((x1 + P.OW - cx, y0 + cy), P.FOOT_D / 2, fc="none", ec="#999", lw=0.4, ls=":"))
ax.text(x1 + P.OW / 2, y0 + P.OL + 3, "FLOOR (underneath, mirrored) with screw holes", ha="center", fontsize=8)
ax.plot([x0, x0 + 100], [100, 100], color="k", lw=0.8)
for i in range(11): ax.plot([x0 + i*10]*2, [100, 103 if i % 5 == 0 else 101.5], color="k", lw=0.6)
ax.text(x0, 95, "100 mm check line", fontsize=8)
ax.text(x0, 85, "Pod case Rev B, 1:1. Print at 100 %. Lay the bezel outline on the device with the dashed board outline on the perfboard:\nthe blue window must frame the touch panel with 1–2 mm of glass hidden under the bezel on every side.", fontsize=8, va="top")
ax.text(x0 + ox + 26, y0 + oy + 10, "Pico end (bottom)", ha="center", fontsize=7, color="#555")
fig.savefig(f"{out}/templates_1to1.pdf"); plt.close(fig)
print("drawings ok")
