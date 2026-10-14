import sys, matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, FancyBboxPatch
sys.path.insert(0, ".")
import pod_case as P
out = sys.argv[1]

def dim(ax, x0, x1, y, text, off=2.5, color="#555"):
    ax.annotate("", (x0, y), (x1, y), arrowprops=dict(arrowstyle="<->", color=color, lw=0.8))
    ax.text((x0 + x1) / 2, y + off, text, ha="center", va="bottom", fontsize=7, color=color)
def vdim(ax, x, y0, y1, text, off=1.5, color="#555"):
    ax.annotate("", (x, y0), (x, y1), arrowprops=dict(arrowstyle="<->", color=color, lw=0.8))
    ax.text(x + off, (y0 + y1) / 2, text, ha="left", va="center", fontsize=7, color=color, rotation=90)

# ---- Sheet 1: plan view (inside the tray, board frame) + 4 wall elevations
fig, axs = plt.subplots(2, 3, figsize=(11.7, 8.3))
ax = axs[0, 0]; ax.set_title("Plan view, looking into the tray (board coordinates)", fontsize=9)
ax.add_patch(FancyBboxPatch((-P.OFF, -P.OFF), P.OW, P.OL, boxstyle="round,pad=0,rounding_size=4", fc="#eef", ec="#333", lw=1))
ax.add_patch(Rectangle((0, 0), P.BW, P.BL, fc="#d9b88a", ec="#7a5a2a", lw=0.8))
ax.add_patch(Rectangle((1, 23), 50, 86, fc="none", ec="#c0392b", lw=0.8, ls="--")); ax.text(26, 100, "screen module", ha="center", fontsize=7, color="#c0392b")
ax.add_patch(Rectangle((1, 1), 50, 21, fc="#cfe8d5", ec="#2e8b57", lw=0.8)); ax.text(26, 11.5, "Pico 2 W (USB →)", ha="center", va="center", fontsize=7, color="#2e8b57")
ax.add_patch(Rectangle((20, 30), 30, 48, fc="#fde9b0", ec="#b8860b", lw=0.8)); ax.text(35, 54, "battery\n30 × 48 × 4\n(under the screen)", ha="center", va="center", fontsize=6.5, color="#8a6a00")
ax.add_patch(Rectangle((2, 86), 17, 23, fc="#e3d4f2", ec="#6a3d9a", lw=0.8)); ax.text(10.5, 97, "DAC\njack ↑", ha="center", va="center", fontsize=6.5, color="#6a3d9a")
ax.add_patch(Rectangle((30, 83), 17, 26, fc="#cfe3f7", ec="#1f5fa8", lw=0.8)); ax.text(38.5, 96, "TP4056\nUSB-C ↑", ha="center", va="center", fontsize=6.5, color="#1f5fa8")
ax.add_patch(Rectangle((1, 55.5), 18, 28.3, fc="none", ec="#999", lw=0.6, ls=":")); ax.text(10, 70, "SD holder\n(on screen)", ha="center", va="center", fontsize=6, color="#777")
ax.add_patch(Rectangle((P.WIN_CX - P.WIN_W/2, P.WIN_CY - P.WIN_H/2), P.WIN_W, P.WIN_H, fc="none", ec="#1f5fa8", lw=1.4)); ax.text(26, 40, "bezel window 45 × 60", ha="center", va="center", fontsize=7, color="#1f5fa8")
ax.set_xlim(-8, P.BW + 8); ax.set_ylim(-8, P.BL + 8); ax.set_aspect("equal"); ax.axis("off")
dim(ax, 0, P.BW, -4, "52 board / 57.2 outer", off=-6)
vdim(ax, P.BW + 3, 0, P.BL, "109 board / 114.2 outer")

def wall(ax, title, length, slots, ticks):
    ax.set_title(title, fontsize=9)
    ax.add_patch(Rectangle((0, 0), length, P.TRAY_H, fc="#dde", ec="#333", lw=1))
    ax.add_patch(Rectangle((0, P.TRAY_H), length, P.BEZEL_H, fc="#9aa", ec="#333", lw=1))
    ax.axhline(P.Z_BOARD_BOT, color="#b8860b", lw=0.6, ls="--"); ax.text(length + 1, P.Z_BOARD_BOT, "board", fontsize=6, va="center", color="#b8860b")
    ax.axhline(P.Z_SCR_BOT, color="#c0392b", lw=0.6, ls="--"); ax.text(length + 1, P.Z_SCR_BOT, "screen PCB", fontsize=6, va="center", color="#c0392b")
    for (c, w, z0, z1, lab) in slots:
        z1 = min(z1, P.TOTAL_H)
        ax.add_patch(Rectangle((c - w/2, z0), w, z1 - z0, fc="#fff", ec="#1f5fa8", lw=1))
        ax.text(c, z1 + 0.8, lab, ha="center", va="bottom", fontsize=6.5, color="#1f5fa8")
        dim(ax, c - w/2, c + w/2, -2.5, f"{w:g}", off=-3.5)
        ax.text(c, -8.5, f"centre {c:g}", ha="center", fontsize=6, color="#555")
        vdim(ax, c + w/2 + 0.5, z0, z1, f"z {z0:.1f}–{z1:.1f}", off=0.6)
    ax.set_xlim(-3, length + 14); ax.set_ylim(-11, P.TOTAL_H + 6); ax.set_aspect("equal"); ax.axis("off")

o = P.OFF
wall(axs[0, 1], "Top end wall (jack + USB-C), seen from outside", P.OW,
     [(o + P.JACK_X, P.JACK_W, P.JACK_Z0, P.JACK_Z1, "3.5 mm jack"), (o + P.TPC_X, P.TPC_W, P.TPC_Z0, P.TPC_Z1, "USB-C charge")], None)
wall(axs[0, 2], "Right wall (Pico USB), seen from outside", P.OL,
     [(o + P.PICO_USB_Y, P.PICO_USB_W, P.PICO_USB_Z0, P.PICO_USB_Z1, "Pico micro-USB")], None)
wall(axs[1, 0], "Left wall (SD + switch), seen from outside (mirror of plan)", P.OL,
     [(P.OL - (o + (P.SD_Y0 + P.SD_Y1)/2), P.SD_Y1 - P.SD_Y0, P.SD_Z0, P.SD_Z1, "microSD"), (P.OL - (o + P.SW_Y), P.SW_W, P.SW_Z0, P.SW_Z1, "power switch")], None)
ax = axs[1, 1]; ax.set_title("Stack (section), mm from the outside of the floor", fontsize=9)
layers = [("floor", 0, P.FLOOR, "#aaa"), ("wire clearance", P.FLOOR, P.Z_BOARD_BOT, "#fff"), ("perfboard", P.Z_BOARD_BOT, P.Z_BOARD_TOP, "#d9b88a"),
          ("sockets / Pico / battery", P.Z_BOARD_TOP, P.Z_SCR_BOT, "#fff"), ("screen PCB", P.Z_SCR_BOT, P.Z_SCR_TOP, "#f4b6b6"),
          ("LCD + touch glass", P.Z_SCR_TOP, P.Z_GLASS, "#cde"), ("bezel over glass", P.Z_GLASS, P.TOTAL_H, "#9aa")]
for name, z0, z1, c in layers:
    ax.add_patch(Rectangle((0, z0), 30, z1 - z0, fc=c, ec="#333", lw=0.6)); ax.text(31, (z0 + z1)/2, f"{name}  ({z0:.1f}–{z1:.1f})", fontsize=7, va="center")
ax.axhline(P.TRAY_H, color="#1f5fa8", lw=1); ax.text(-1, P.TRAY_H, "tray rim 18.2", fontsize=7, ha="right", va="center", color="#1f5fa8")
ax.set_xlim(-14, 75); ax.set_ylim(-2, P.TOTAL_H + 3); ax.set_aspect("equal"); ax.axis("off")
ax = axs[1, 2]; ax.axis("off")
ax.text(0, 1, "Key numbers", fontsize=9, weight="bold", va="top")
txt = f"""Outer: {P.OW:.1f} × {P.OL:.1f} × {P.TOTAL_H:.1f} mm, corner radius 4
Walls 2.0, floor 2.0, bezel top 1.2 over the glass
Board clearance 0.6 per side; lip fit 0.25 per side
Tray height {P.TRAY_H:.1f}; bezel lip drops {P.LIP_H:.0f} into the tray
Board rests on 1.6 mm rails, 3.5 above the floor
Bezel window {P.WIN_W:.0f} × {P.WIN_H:.0f}, centre ({P.WIN_CX:.0f}, {P.WIN_CY:.1f}) board coords
Six snap bumps on the lip / pockets in the tray
No external screws: snap-fit; stack is clamped
between the rails and the bezel's underside."""
ax.text(0, 0.9, txt, fontsize=7.5, va="top", family="monospace")
fig.tight_layout(); fig.savefig(f"{out}/drawings.png", dpi=170); plt.close(fig)

# ---- 1:1 bezel template (A4): outer outline + window, to lay over the screen
fig = plt.figure(figsize=(8.27, 11.69)); ax = fig.add_axes([0, 0, 1, 1]); ax.set_xlim(0, 210); ax.set_ylim(0, 297); ax.set_aspect("equal"); ax.axis("off")
x0, y0 = 40, 120
ax.add_patch(FancyBboxPatch((x0, y0), P.OW, P.OL, boxstyle="round,pad=0,rounding_size=4", fc="none", ec="#000", lw=0.8))
ax.add_patch(Rectangle((x0 + o, y0 + o), P.BW, P.BL, fc="none", ec="#999", lw=0.5, ls="--"))
ax.add_patch(Rectangle((x0 + o + P.WIN_CX - P.WIN_W/2, y0 + o + P.WIN_CY - P.WIN_H/2), P.WIN_W, P.WIN_H, fc="none", ec="#1f5fa8", lw=1))
ax.add_patch(Rectangle((x0 + o + 1, y0 + o + 23), 50, 86, fc="none", ec="#c0392b", lw=0.5, ls=":"))
ax.plot([x0, x0 + 100], [100, 100], color="k", lw=0.8)
for i in range(11): ax.plot([x0 + i*10]*2, [100, 103 if i % 5 == 0 else 101.5], color="k", lw=0.6)
ax.text(x0, 95, "100 mm check line", fontsize=8)
ax.text(x0, y0 + P.OL + 6, "Pod bezel, 1:1. Print at 100%. Lay it on the screen with the dashed board outline on the perfboard:\nthe blue window must frame the touch panel with 1–2 mm of glass hidden under the bezel on every side.", fontsize=8, va="bottom")
ax.text(x0 + o + 26, y0 + o + 11.5, "Pico end (bottom)", ha="center", fontsize=7, color="#555")
fig.savefig(f"{out}/bezel_template_1to1.pdf"); plt.close(fig)
print("drawings ok")
