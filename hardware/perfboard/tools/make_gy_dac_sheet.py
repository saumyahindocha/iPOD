"""GY-PCM5102 DAC wiring sheet (the replacement for the dead Adafruit board). Writes HTML with inline SVG;
render to PDF with Playwright. Pin map = sdk/common/pod_pins.h: DIN GP13 (pin 17), BCK GP14 (pin 19), LRCK GP15 (pin 20)."""
import sys
out = sys.argv[1]
RED, BLK, GRN, GREY, BLUE = "#c0392b", "#222", "#2e8b57", "#999", "#1f5fa8"

def pinbox(x, y, label, col, sub=""):
    t = f'<rect x="{x-20}" y="{y-20}" width="40" height="40" rx="4" fill="#fff" stroke="{col}" stroke-width="2"/>'
    t += f'<circle cx="{x}" cy="{y}" r="6" fill="#fff" stroke="{col}" stroke-width="2"/>'
    t += f'<text x="{x}" y="{y+34}" font-size="13" text-anchor="middle" font-weight="bold" fill="{col}">{label}</text>'
    if sub: t += f'<text x="{x}" y="{y+48}" font-size="10" text-anchor="middle" fill="#555">{sub}</text>'
    return t

s = ['<svg xmlns="http://www.w3.org/2000/svg" width="186mm" height="160mm" viewBox="0 0 744 640" font-family="Helvetica,Arial,sans-serif">']
# ---- DAC, component side, jack at the top, header at the bottom
bx, by, bw, bh = 40, 70, 260, 150
s.append(f'<text x="{bx+bw/2}" y="{by-40}" font-size="15" text-anchor="middle" font-weight="bold">GY-PCM5102, seen from the component side</text>')
s.append(f'<text x="{bx+bw/2}" y="{by-22}" font-size="11" text-anchor="middle" fill="#555">(go by the names printed on the board, not by position)</text>')
s.append(f'<rect x="{bx}" y="{by}" width="{bw}" height="{bh}" rx="8" fill="#eef5ee" stroke="#2c3e50" stroke-width="2"/>')
s.append(f'<rect x="{bx+bw-26}" y="{by+bh/2-16}" width="46" height="32" rx="6" fill="#333"/><circle cx="{bx+bw+20}" cy="{by+bh/2}" r="9" fill="#777"/>')
s.append(f'<text x="{bx+bw+50}" y="{by+bh/2-30}" font-size="11" text-anchor="middle">jack end</text>')
s.append(f'<text x="{bx+bw+50}" y="{by+bh/2-17}" font-size="11" text-anchor="middle">(top wall)</text>')
s.append(f'<rect x="{bx+60}" y="{by+40}" width="60" height="60" rx="3" fill="#222"/><text x="{bx+90}" y="{by+75}" font-size="11" text-anchor="middle" fill="#fff">PCM5102A</text>')
names = [("VIN", RED, "3V3"), ("GND", BLK, "GND"), ("LCK", BLUE, "GP15"), ("DIN", "#bcbd22", "GP13"),
         ("BCK", "#8c564b", "GP14"), ("SCK", BLK, "GND")]
hx0, hy = bx + 20, by + bh - 22
for i, (n, c, sub) in enumerate(names):
    x = hx0 + 14 + i * 40
    s.append(f'<circle cx="{x}" cy="{hy}" r="7" fill="#c9a24a" stroke="#8a6a20" stroke-width="1"/>')
    s.append(f'<text x="{x}" y="{hy-14}" font-size="12" text-anchor="middle" font-weight="bold" fill="{c}">{n}</text>')
    s.append(f'<line x1="{x}" y1="{hy+8}" x2="{x}" y2="{hy+48}" stroke="{c}" stroke-width="2.5"/>')
    s.append(f'<text x="{x}" y="{hy+62}" font-size="10" text-anchor="middle" fill="{c}" font-weight="bold">→ {sub}</text>')
s.append(f'<text x="{bx+bw/2}" y="{by+bh+84}" font-size="11" text-anchor="middle" fill="#555">6-pin header: 5 wires to the Pico, SCK tied to GND</text>')
s.append(f'<text x="{bx+bw/2}" y="{by+bh+98}" font-size="11" text-anchor="middle" fill="#555">(the chip then makes its own master clock from BCK)</text>')

# ---- back of the board: four solder jumpers
jx, jy = 40, 420
s.append(f'<text x="{jx}" y="{jy-30}" font-size="14" font-weight="bold">Back of the board: four solder bridges.</text>')
s.append(f'<text x="{jx}" y="{jy-13}" font-size="13" font-weight="bold" fill="{RED}">Without no. 3 the DAC is mute.</text>')
jumpers = [("1", "FLT", "L", "normal filter"), ("2", "DEMP", "L", "de-emphasis off"), ("3", "XSMT", "H", "un-mute (the one that matters)"), ("4", "FMT", "L", "I2S format")]
for i, (num, name, side, why) in enumerate(jumpers):
    y = jy + 14 + i * 30
    s.append(f'<text x="{jx}" y="{y+5}" font-size="12" font-weight="bold">{num}  {name}</text>')
    for k, lab in enumerate(("L", "H")):
        x = jx + 90 + k * 46
        on = (lab == side)
        s.append(f'<rect x="{x}" y="{y-9}" width="18" height="18" rx="2" fill="{"#f6d5d1" if on else "#fff"}" stroke="{RED if on else GREY}" stroke-width="{2 if on else 1}"/>')
        s.append(f'<rect x="{x+22}" y="{y-6}" width="12" height="12" fill="{"#aaa"}" />')
        if on: s.append(f'<rect x="{x+6}" y="{y-5}" width="24" height="10" rx="3" fill="{RED}" fill-opacity="0.7"/>')
        s.append(f'<text x="{x+9}" y="{y+4}" font-size="10" text-anchor="middle">{lab}</text>')
    s.append(f'<text x="{jx+200}" y="{y+5}" font-size="11" fill="#555">{side}: {why}</text>')
for i, line in enumerate(["Each jumper: a centre pad with an H pad one side, an L pad the other, numbered 1–4.",
                          "A blob of solder across centre+L (centre+H for no. 3).",
                          "Boards often ship with 1, 2, 4 bridged and no. 3 open: check all four with a magnifier."]):
    s.append(f'<text x="{jx}" y="{jy+140+i*14}" font-size="10.5" fill="#555">{line}</text>')

# ---- Pico end: pins 17..20 of the GP0 row + 3V3
px0, py = 420, 100
s.append(f'<text x="{px0+110}" y="{py-40}" font-size="15" text-anchor="middle" font-weight="bold">Pico end of each wire</text>')
s.append(f'<text x="{px0+110}" y="{py-22}" font-size="11" text-anchor="middle" fill="#555">GP0 row, last four pins at the antenna end</text>')
s.append(f'<text x="{px0+110}" y="{py-9}" font-size="11" text-anchor="middle" fill="#555">(perfboard row 8, columns Q R S T)</text>')
for k, (pn, gp, hole, col, lab) in enumerate([("17", "GP13", "Q8", "#bcbd22", "DIN"), ("18", "GND", "R8", BLK, "GND"), ("19", "GP14", "S8", "#8c564b", "BCK"), ("20", "GP15", "T8", BLUE, "LCK")]):
    x = px0 + k * 72 + 20
    s.append(pinbox(x, py + 20, f"pin {pn}", col, f"{gp} · {hole}"))
    s.append(f'<text x="{x}" y="{py+78}" font-size="12" text-anchor="middle" font-weight="bold" fill="{col}">↑ {lab}</text>')

s.append(pinbox(px0 + 20, py + 160, "3V3", RED, "pin 36 · R10"))
s.append(f'<text x="{px0+20}" y="{py+218}" font-size="12" text-anchor="middle" font-weight="bold" fill="{RED}">↑ VIN</text>')
s.append(f'<text x="{px0+60}" y="{py+150}" font-size="10.5" fill="#555">R10 = the screen socket\'s VCC hole (3V3 net);</text>')
s.append(f'<text x="{px0+60}" y="{py+164}" font-size="10.5" fill="#555">Pico pin 36 (5th from the USB end) is the same net.</text>')
s.append(f'<text x="{px0+60}" y="{py+178}" font-size="10.5" fill="#555">Silent on 3.3 V? Move VIN to VSYS (pin 39).</text>')

# ---- table
ty = 380
s.append(f'<text x="{px0}" y="{ty}" font-size="14" font-weight="bold">The six connections</text>')
rows = [("VIN", "3V3 (Pico pin 36 / hole R10)", RED), ("GND", "GND (Pico pin 18 / hole R8)", BLK), ("LCK", "GP15 (Pico pin 20 / hole T8)", BLUE),
        ("DIN", "GP13 (Pico pin 17 / hole Q8)", "#bcbd22"), ("BCK", "GP14 (Pico pin 19 / hole S8)", "#8c564b"), ("SCK", "GND (short to the GND pin beside it)", BLK)]
for i, (a, b, c) in enumerate(rows):
    y = ty + 24 + i * 22
    s.append(f'<text x="{px0}" y="{y}" font-size="12.5" font-weight="bold" fill="{c}">{a}</text><text x="{px0+48}" y="{y}" font-size="12.5">→  {b}</text>')
s.append(f'<text x="{px0}" y="{ty+170}" font-size="10.5" fill="#555">Test with pod_dac_test.uf2 (beeps L / R / both), then pod.uf2.</text>')
s.append('</svg>')

html = f"""<!doctype html><html><head><meta charset="utf-8"><style>@page{{size:A4;margin:12mm}}body{{font-family:Helvetica,Arial,sans-serif;font-size:10.5pt;color:#111}}
h1{{font-size:17pt;margin:0 0 2mm}}p{{margin:1.5mm 0}}ol{{margin:1mm 0 0 5mm;padding-left:3mm}}li{{margin-bottom:1mm}}</style></head><body>
<h1>Pod — GY-PCM5102 DAC wiring</h1>
<p>Replacement for the Adafruit board. Firmware pin map (sdk/common/pod_pins.h): <b>DIN = GP13 (pin 17), BCK = GP14 (pin 19), LCK/WSEL = GP15 (pin 20)</b>.
Same three Pico pins as before, so the old wires 14–16 can be reused if they are still there; only the DAC end changes.</p>
{"".join(s)}
<p><b>Mounting for the Rev B case:</b> top-left of the perfboard, board edge x 3.8–21.3 (the SD socket is to its right, the corner screw block to its left),
jack end sticking <b>3.5 mm past the perfboard's top edge</b>, module about 4 mm above the perfboard so the jack centre is ~8 mm up. Easiest: solder a
6-pin male header into row 33 columns N–S (x 5–18), seat the DAC's header on it leaving the plastic spacer 1.5 mm proud, solder, trim. Tell me the measured
perfboard-top-to-jack-centre height and I update <i>JACK_H</i> if it is not 8 ± 1.5.</p>
<p><b>Order of work:</b></p>
<ol><li>Bridge the four pads on the back first (3 → H, the rest → L), with the board loose on the bench.</li>
<li>Short SCK to GND on the header (a solder bridge between the two adjacent pins, or one short wire).</li>
<li>Solder the five wires: VIN, GND, LCK, DIN, BCK, as above. Meter check: DAC GND beeps to Pico pin 18; DAC VIN reads 3.3 V with the Pico powered; no beep between VIN and GND.</li>
<li>Flash <b>pod_dac_test.uf2</b>, plug in headphones. Silence → recheck jumper 3 and that LCK/BCK/DIN are not swapped (the pin-finder firmware blinks GP13 at 1 Hz, GP14 fast, GP15 slow).</li></ol>
</body></html>"""
open(out, "w").write(html)
print("ok")
