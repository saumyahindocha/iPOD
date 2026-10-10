import sys
P=8  # px per 2.54 mm in drawing units
def pin(x,y,label,cls,side):
    col={"wire":"#c0392b","nc":"#999","gnd":"#222"}[cls]
    fill={"wire":"#f6d5d1","nc":"#eee","gnd":"#ddd"}[cls]
    t=f'<rect x="{x-9}" y="{y-9}" width="18" height="18" rx="2" fill="{fill}" stroke="{col}" stroke-width="1.5"/><circle cx="{x}" cy="{y}" r="4" fill="#fff" stroke="{col}" stroke-width="1.5"/>'
    anchor="end" if side=="L" else "start"
    dx=-16 if side=="L" else 16
    weight="bold" if cls!="nc" else "normal"
    t+=f'<text x="{x+dx}" y="{y+5}" font-size="14" text-anchor="{anchor}" font-weight="{weight}" fill="{col}">{label}</text>'
    return t
s=['<svg xmlns="http://www.w3.org/2000/svg" width="180mm" height="150mm" viewBox="0 0 720 600" font-family="Helvetica,Arial,sans-serif">']
# DAC board outline (solder-side view, jack end at bottom)
bx,by,bw,bh=110,60,210,330
s.append(f'<rect x="{bx}" y="{by}" width="{bw}" height="{bh}" rx="10" fill="#1d2b22" fill-opacity="0.08" stroke="#2c3e50" stroke-width="2" stroke-dasharray="6,4"/>')
s.append(f'<text x="{bx+bw/2}" y="{by-14}" font-size="15" text-anchor="middle" font-weight="bold">DAC pins, seen from the solder side</text>')
s.append(f'<rect x="{bx+bw/2-32}" y="{by+bh-10}" width="64" height="44" rx="6" fill="#333"/><circle cx="{bx+bw/2}" cy="{by+bh+16}" r="12" fill="#666"/>')
s.append(f'<text x="{bx+bw/2}" y="{by+bh+56}" font-size="13" text-anchor="middle">headphone jack end (board edge)</text>')
left=["DE","FIL","MCK","MU","FM","3V"]
right=[("VIN","wire"),("GND","gnd"),("WSEL","wire"),("DIN","wire"),("BCK","wire"),("Lout","nc")]
pitch=42
lx, rx = bx+40, bx+bw-40
for i,n in enumerate(left):
    s.append(pin(lx, by+60+pitch*(i+1), n, "nc", "L"))
for i,(n,c) in enumerate(right):
    s.append(pin(rx, by+60+pitch*i, n, c, "R"))
s.append(f'<text x="{lx}" y="{by+34}" font-size="12" text-anchor="middle" fill="#777">6-pin row</text>')
s.append(f'<text x="{lx}" y="{by+48}" font-size="12" text-anchor="middle" fill="#777">no wires</text>')
s.append(f'<text x="{rx}" y="{by+34}" font-size="12" text-anchor="middle" fill="#c0392b">wire row</text>')
# Pico rows
px0=420
s.append(f'<text x="{px0+120}" y="46" font-size="15" text-anchor="middle" font-weight="bold">Where each wire goes on the Pico</text>')
def picorow(y, pins, title):
    s.append(f'<text x="{px0}" y="{y-22}" font-size="13" fill="#555">{title}</text>')
    for k,(n,lab,cls) in enumerate(pins):
        x=px0+k*48
        col={"wire":"#c0392b","gnd":"#222","nc":"#aaa"}[cls]
        s.append(f'<rect x="{x}" y="{y}" width="40" height="40" rx="3" fill="#fff" stroke="{col}" stroke-width="{2 if cls!="nc" else 1}"/>')
        s.append(f'<text x="{x+20}" y="{y+17}" font-size="12" text-anchor="middle" font-weight="bold" fill="{col}">{n}</text>')
        s.append(f'<text x="{x+20}" y="{y+33}" font-size="10" text-anchor="middle" fill="{col}">{lab}</text>')
picorow(110, [("40","VBUS","nc"),("39","VSYS","nc"),("38","GND","nc"),("37","3V3_EN","nc"),("36","3V3","wire")], "Row starting at pin 40 (USB end) →")
s.append(f'<text x="{px0+4*48+20}" y="168" font-size="12" text-anchor="middle" fill="#c0392b">← VIN</text>')
picorow(250, [("17","GP13","wire"),("18","GND","gnd"),("19","GP14","wire"),("20","GP15","wire")], "Last 4 pins of the GP0 row (antenna end)")
for k,t in enumerate(["DIN","GND","BCK","WSEL"]):
    s.append(f'<text x="{px0+k*48+20}" y="308" font-size="12" text-anchor="middle" fill="{"#222" if t=="GND" else "#c0392b"}">↑ {t}</text>')
s.append(f'<text x="{px0+4*48+6}" y="276" font-size="12" fill="#555">end</text>')
# wire table
rows=[("VIN","Pico pin 36 (3V3)","5th pin from the USB end, row with pin 40"),
      ("GND","Pico pin 18 (GND)","3rd pin from the antenna end, GP0 row"),
      ("WSEL","Pico pin 20 (GP15)","last pin at the antenna end, GP0 row"),
      ("DIN","Pico pin 17 (GP13)","4th pin from the antenna end, GP0 row"),
      ("BCK","Pico pin 19 (GP14)","2nd pin from the antenna end, GP0 row")]
y=360
s.append(f'<text x="{px0-40}" y="{y}" font-size="14" font-weight="bold">The 5 wires</text>')
for i,(a,b,c) in enumerate(rows):
    yy=y+26+i*38
    s.append(f'<text x="{px0-40}" y="{yy}" font-size="13" font-weight="bold" fill="#c0392b">{i+1}. DAC {a}  →  {b}</text>')
    s.append(f'<text x="{px0-22}" y="{yy+16}" font-size="11" fill="#555">{c}</text>')
s.append('</svg>')
svg="\n".join(s)
html=f'''<!doctype html><html><head><meta charset="utf-8"><style>
@page {{ size: A4; margin: 12mm; }} body {{ font-family: Helvetica, Arial, sans-serif; font-size: 10.5pt; line-height: 1.4; }}
h1 {{ font-size: 17pt; margin: 0 0 2mm; }} h2 {{ font-size: 12.5pt; margin: 4mm 0 1mm; }} ol,ul {{ margin: 1mm 0 0 5mm; padding-left: 3mm; }} li {{ margin-bottom: 1mm; }}
</style></head><body>
<h1>Pod: DAC wiring</h1>
<p>Taken from Adafruit's own board files, matched to your photos. Turn the board over so the <b>headphone jack end points
towards you</b>: the row of 6 pins on the <b>left</b> needs no wires; the row on the <b>right</b> has the 5 pins that do.</p>
{svg}
<h2>Before soldering: one 10-second check</h2>
<ol>
<li>Multimeter on continuity (beep). One probe on the <b>second pin from the top</b> of the right-hand row.</li>
<li>Other probe on the outer metal tab of the headphone jack. It must <b>beep</b>: that pin is GND, so the row is the right way round.</li>
<li>If it doesn't beep, stop and send me a photo before wiring.</li>
</ol>
<h2>Tips</h2>
<ul>
<li>Cut the bent pins down to about 2 mm, then solder each one to its own pad only. Bent pins lying across neighbouring pads can short.</li>
<li>Solder the six left-row pins and Lout too (for strength), but connect nothing to them. The <b>3V</b> pin especially must stay unconnected.</li>
<li>These Pico pins match the current firmware (pod.uf2): DIN = GP13, BCK = GP14, WSEL = GP15.</li>
</ul>
</body></html>'''
open(sys.argv[1],"w").write(html)
