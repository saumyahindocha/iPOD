# Pod perfboard build guide: generates perf.html (4 A4 pages)
P = 2.54; COLS = 20; ROWS = 43
W = COLS * P; H = ROWS * P
L = "ABCDEFGHIJKLMNOPQRST"
SC = 2.0  # drawing scale (mm on paper per mm of board)

def hx(c, back=False):
    i = L.index(c) if isinstance(c, str) else c - 1
    x = P / 2 + i * P
    return W - x if back else x
def hy(r): return P / 2 + (r - 1) * P
def hole(h):  # "E10" -> (col, row)
    return h[0], int(h[1:])

PICO_TOP = {1:"VBUS",2:"VSYS",3:"GND",4:"3V3_EN",5:"3V3",6:"ADC_VREF",7:"GP28",8:"AGND",9:"GP27",10:"GP26",
            11:"RUN",12:"GP22",13:"GND",14:"GP21",15:"GP20",16:"GP19",17:"GP18",18:"GND",19:"GP17",20:"GP16"}
PICO_BOT = {1:"GP0",2:"GP1",3:"GND",4:"GP2",5:"GP3",6:"GP4",7:"GP5",8:"GND",9:"GP6",10:"GP7",11:"GP8",12:"GP9",
            13:"GND",14:"GP10",15:"GP11",16:"GP12",17:"GP13",18:"GND",19:"GP14",20:"GP15"}
J3 = ["VCC","GND","CS","RESET","DC","SDI","SCK","LED","SDO","T_CLK","T_CS","T_DIN","T_DO","T_IRQ"]  # pin1 at col R(18), leftwards
J4 = ["SD_CS","SD_MOSI","SD_MISO","SD_SCK"]  # pin1 at col L(12), leftwards

# wires on the back: (no, from, to, label, colour)
WIRES = [
 (1,"E1","R10","3V3 → screen VCC","#d62728"),
 (2,"R8","Q10","GND → screen GND","#222222"),
 (3,"J8","G10","GP7 → T_DIN (MOSI)","#2ca02c"),
 (4,"G10","M10","T_DIN → SDI (MOSI)","#2ca02c"),
 (5,"I10","L10","T_CLK → SCK","#ff7f0e"),
 (6,"G8","H10","GP5 → T_CS","#8c564b"),
 (7,"I10","I42","T_CLK → SD_SCK","#ff7f0e"),
 (8,"F10","J42","T_DO → SD_MISO","#e377c2"),
 (9,"M10","K42","SDI → SD_MOSI","#2ca02c"),
 (10,"L8","L42","GP9 → SD_CS","#1f77b4"),
 (11,"G6","G1","battery sense → GP28","#9467bd"),
 (12,"R10","N34","3V3 → DAC VIN","#d62728"),
 (13,"R8","N35","GND → DAC GND","#222222"),
 (14,"T8","N36","GP15 → DAC WSEL","#17becf"),
 (15,"Q8","N37","GP13 → DAC DIN","#bcbd22"),
 (16,"S8","N38","GP14 → DAC BCK","#8c564b"),
]
DAC_JP2 = [("N34","VIN"),("N35","GND"),("N36","WSEL"),("N37","DIN"),("N38","BCK"),("N39","Lout")]
DAC_JP1 = [("T35","DE"),("T36","FIL"),("T37","MCK"),("T38","MU"),("T39","FM"),("T40","3V")]
BRIDGES = [("B1","B2"),("B6","B7"),("B6","C6"),("G6","H6"),("H1","H2"),("C7","C8")]
LINKS = ["E","F","I","K","N","O","P"]   # row 8 -> row 9 -> row 10, bare wire
NOLINK = ["G","H","J","L","M","Q","R"]  # top and bottom differ here: never join

def board_svg(back=False, show_wires=False):
    s = []
    m = 9  # margin in board-mm for labels
    vw, vh = W + 2*m, H + 2*m
    s.append(f'<svg xmlns="http://www.w3.org/2000/svg" width="{vw*SC}mm" height="{vh*SC}mm" viewBox="{-m} {-m} {vw} {vh}" font-family="Helvetica,Arial,sans-serif">')
    s.append(f'<rect x="0" y="0" width="{W}" height="{H}" fill="{"#e9dcc0" if back else "#f3ead6"}" stroke="#7a6a4a" stroke-width="0.3"/>')
    # zones (front-view positions, mirrored on back)
    def rect(c1, r1, c2, r2, **kw):
        xa, xb = hx(c1, back), hx(c2, back)
        x0, x1 = min(xa, xb) - P/2, max(xa, xb) + P/2
        y0, y1 = hy(r1) - P/2, hy(r2) + P/2
        attrs = " ".join(f'{k.replace("_","-")}="{v}"' for k, v in kw.items())
        s.append(f'<rect x="{x0}" y="{y0}" width="{x1-x0}" height="{y1-y0}" {attrs}/>')
        return x0, y0, x1, y1
    if not back:
        x0,y0,x1,y1 = rect("N",22,"T",33, fill="#ffffff", fill_opacity="0.55", stroke="#999", stroke_width="0.25", stroke_dasharray="1,0.8")
        s.append(f'<text x="{(x0+x1)/2}" y="{(y0+y1)/2-1.5}" font-size="1.9" text-anchor="middle" fill="#666">screen\'s SD holder</text>')
        s.append(f'<text x="{(x0+x1)/2}" y="{(y0+y1)/2+1}" font-size="1.9" text-anchor="middle" fill="#666">above: keep low</text>')
        x0,y0,x1,y1 = rect("A",32,"G",43, fill="#cfe3f7", stroke="#1f5fa8", stroke_width="0.35")
        s.append(f'<text x="{(x0+x1)/2}" y="{(y0+y1)/2-2}" font-size="2.2" text-anchor="middle" fill="#1f5fa8" font-weight="bold">TP4056</text>')
        s.append(f'<text x="{(x0+x1)/2}" y="{(y0+y1)/2+0.8}" font-size="1.8" text-anchor="middle" fill="#1f5fa8">charger, stuck on</text>')
        s.append(f'<text x="{(x0+x1)/2}" y="{(y0+y1)/2+3.3}" font-size="1.8" text-anchor="middle" fill="#1f5fa8">USB-C ↓ out of edge</text>')
        dx0, dx1 = hx("N") - 2.54, hx("T") + 2.54
        dy0, dy1 = hy(34) - 6.35, H + 0.6
        s.append(f'<rect x="{dx0}" y="{dy0}" width="{dx1-dx0}" height="{dy1-dy0}" rx="1" fill="#e3d4f2" fill-opacity="0.85" stroke="#6a3d9a" stroke-width="0.35"/>')
        s.append(f'<rect x="{(hx("N")+hx("T"))/2-3}" y="{H-4.5}" width="6" height="6.5" rx="0.6" fill="#444"/>')
        s.append(f'<text x="{(hx("N")+hx("T"))/2}" y="{hy(31.6)}" font-size="2.0" text-anchor="middle" fill="#6a3d9a" font-weight="bold">DAC</text>')
        s.append(f'<text x="{(hx("N")+hx("T"))/2}" y="{H+3.2}" font-size="1.4" text-anchor="middle" fill="#6a3d9a">jack</text>')
    # holes
    for c in range(1, COLS+1):
        for r in range(1, ROWS+1):
            s.append(f'<circle cx="{hx(c,back)}" cy="{hy(r)}" r="0.55" fill="#c9a24a" stroke="#8a6a20" stroke-width="0.12"/>')
            s.append(f'<circle cx="{hx(c,back)}" cy="{hy(r)}" r="0.22" fill="#fff"/>')
    for h, n in DAC_JP2 + DAC_JP1:
        c, r = hole(h)
        used = n in ("VIN","GND","WSEL","DIN","BCK")
        col = "#c0392b" if used else "#8e7cc3"
        s.append(f'<rect x="{hx(c,back)-1.0}" y="{hy(r)-1.0}" width="2.0" height="2.0" fill="none" stroke="{col}" stroke-width="0.35"/>')
        toM = (c == "N")
        lx = hx(c,back) + (1.6 if toM else -1.6)
        anchor = "start" if toM else "end"
        s.append(f'<text x="{lx}" y="{hy(r)+0.5}" font-size="1.2" text-anchor="{anchor}" fill="{col}" font-weight="{"bold" if used else "normal"}">{n}</text>')
    # column / row labels
    for i, ch in enumerate(L):
        s.append(f'<text x="{hx(ch,back)}" y="-2.6" font-size="1.7" text-anchor="middle" fill="#555">{ch}</text>')
        s.append(f'<text x="{hx(ch,back)}" y="{H+4.2}" font-size="1.7" text-anchor="middle" fill="#555">{ch}</text>')
    for r in range(1, ROWS+1):
        if r in (1,2,5,6,7,8,10,15,20,25,30,35,40,42,43) or r % 5 == 0:
            s.append(f'<text x="-2.2" y="{hy(r)+0.6}" font-size="1.6" text-anchor="end" fill="#555">{r}</text>')
            s.append(f'<text x="{W+2.2}" y="{hy(r)+0.6}" font-size="1.6" fill="#555">{r}</text>')
    # sockets
    def socket(row, c_from, c_to, color):
        xa, xb = hx(c_from, back), hx(c_to, back)
        s.append(f'<rect x="{min(xa,xb)-1.27}" y="{hy(row)-1.27}" width="{abs(xa-xb)+2.54}" height="2.54" fill="none" stroke="{color}" stroke-width="0.45"/>')
    socket(1, 1, 20, "#111"); socket(8, 1, 20, "#111")
    socket(10, 5, 18, "#c0392b"); socket(42, 9, 12, "#c0392b")
    # Pico outline (front only)
    if not back:
        s.append(f'<rect x="{hx(1)-1.37}" y="{hy(1)-1.6}" width="51" height="21" rx="1" fill="#2e8b57" fill-opacity="0.13" stroke="#2e8b57" stroke-width="0.35" stroke-dasharray="1.2,0.6"/>')
        s.append(f'<rect x="-4.6" y="{hy(4.5)-3.8}" width="5.6" height="7.6" fill="#bbb" stroke="#666" stroke-width="0.2"/>')
        s.append(f'<text x="-1.8" y="{hy(4.5)+0.6}" font-size="1.5" text-anchor="middle" fill="#333">USB</text>')
        s.append(f'<rect x="{hx(17)-1.27}" y="{hy(2)-1.0}" width="{3*P+2.54}" height="{6*P-0.4}" fill="none" stroke="#2e8b57" stroke-width="0.25" stroke-dasharray="0.6,0.5"/>')
        s.append(f'<text x="{hx(18.5) if False else hx("R")+1.27}" y="{hy(4)+0.6}" font-size="1.7" text-anchor="middle" fill="#2e8b57" font-weight="bold">antenna</text>')
        s.append(f'<text x="{hx("R")+1.27}" y="{hy(5)+0.9}" font-size="1.5" text-anchor="middle" fill="#2e8b57">nothing here</text>')
        s.append(f'<text x="{hx("M")}" y="{hy(4.8)}" font-size="2.6" text-anchor="middle" fill="#2e8b57" font-weight="bold">Pico 2 W</text>')
        # parts under the Pico
        s.append(f'<line x1="{hx("B")}" y1="{hy(2)}" x2="{hx("B")}" y2="{hy(6)}" stroke="#333" stroke-width="0.35"/>')
        s.append(f'<rect x="{hx("B")-0.9}" y="{hy(3)}" width="1.8" height="{P*2}" rx="0.4" fill="#222"/>')
        s.append(f'<rect x="{hx("B")-0.9}" y="{hy(3)}" width="1.8" height="0.7" fill="#ddd"/>')
        s.append(f'<text x="{hx("B")-1.5}" y="{hy(4.6)}" font-size="1.5" text-anchor="end" fill="#000">D1</text>')
        s.append(f'<line x1="{hx("C")}" y1="{hy(6)}" x2="{hx("G")}" y2="{hy(6)}" stroke="#333" stroke-width="0.35"/>')
        s.append(f'<rect x="{hx("D")-0.4}" y="{hy(6)-0.8}" width="{P*2+0.8}" height="1.6" rx="0.6" fill="#d9b88a" stroke="#7a5a2a" stroke-width="0.15"/>')
        s.append(f'<text x="{hx("E")}" y="{hy(6)+2.3}" font-size="1.5" text-anchor="middle">R1 100k</text>')
        s.append(f'<line x1="{hx("H")}" y1="{hy(2)}" x2="{hx("H")}" y2="{hy(6)}" stroke="#333" stroke-width="0.35"/>')
        s.append(f'<rect x="{hx("H")-0.8}" y="{hy(2.6)}" width="1.6" height="{P*2.8}" rx="0.6" fill="#d9b88a" stroke="#7a5a2a" stroke-width="0.15"/>')
        s.append(f'<text x="{hx("H")+1.3}" y="{hy(3.9)}" font-size="1.5">R2</text>')
        s.append(f'<text x="{hx("H")+1.3}" y="{hy(4.6)}" font-size="1.5">100k</text>')
        # landing holes for external wires
        for h, t in (("B7","to switch"),("C7","to TP4056 OUT−")):
            c, r = hole(h)
            s.append(f'<circle cx="{hx(c)}" cy="{hy(r)}" r="0.9" fill="none" stroke="#d35400" stroke-width="0.35"/>')
    # pin names
    def pin_labels(row, names, start_col, step, above):
        for k, n in enumerate(names):
            c = start_col + k*step
            y = hy(row) + (-1.75 if above else 2.9)
            fill = "#c0392b" if n in ("GND","AGND") else ("#b03a00" if n in ("VSYS","3V3","VBUS") else "#333")
            s.append(f'<text x="{hx(c,back)}" y="{y}" font-size="1.05" text-anchor="end" transform="rotate(-90 {hx(c,back)} {y})" fill="{fill}">{n}</text>' if above else
                     f'<text x="{hx(c,back)}" y="{y}" font-size="1.05" text-anchor="start" transform="rotate(-90 {hx(c,back)} {y}) translate(-{len(n)*0.62+0.3} 0)" fill="{fill}">{n}</text>')
    if not back:
        for k, n in enumerate(J3):
            c = 18 - k
            s.append(f'<text x="{hx(c)}" y="{hy(10)+1.7}" font-size="1.05" text-anchor="end" transform="rotate(-90 {hx(c)} {hy(10)+1.7})" fill="#c0392b">{n}</text>')
        for k, n in enumerate(J4):
            c = 12 - k
            s.append(f'<text x="{hx(c)}" y="{hy(42)-1.7}" font-size="1.05" text-anchor="start" transform="rotate(-90 {hx(c)} {hy(42)-1.7})" fill="#c0392b">{n}</text>')
        s.append(f'<text x="{hx("A")}" y="{hy(12.3)}" font-size="1.7" fill="#c0392b" font-weight="bold">screen</text>')
        s.append(f'<text x="{hx("A")}" y="{hy(13.1)}" font-size="1.7" fill="#c0392b" font-weight="bold">socket</text>')
        s.append(f'<text x="{(hx("I")+hx("L"))/2}" y="{H+7.2}" font-size="1.7" text-anchor="middle" fill="#c0392b" font-weight="bold">SD socket</text>')
    if show_wires:
        # bridges
        for a, b in BRIDGES:
            (ca, ra), (cb, rb) = hole(a), hole(b)
            s.append(f'<line x1="{hx(ca,True)}" y1="{hy(ra)}" x2="{hx(cb,True)}" y2="{hy(rb)}" stroke="#7f8c8d" stroke-width="1.5" stroke-linecap="round"/>')
        for c in LINKS:
            s.append(f'<line x1="{hx(c,True)}" y1="{hy(8)}" x2="{hx(c,True)}" y2="{hy(10)}" stroke="#555" stroke-width="1.5" stroke-linecap="round"/>')
        for c in NOLINK:
            x, y = hx(c,True), hy(9)
            s.append(f'<path d="M{x-0.8},{y-0.8} L{x+0.8},{y+0.8} M{x-0.8},{y+0.8} L{x+0.8},{y-0.8}" stroke="#e00" stroke-width="0.4"/>')
        # part leads seen from the back
        for a, b, t in (("B2","B6","D1"),("C6","G6","R1"),("H2","H6","R2")):
            (ca, ra), (cb, rb) = hole(a), hole(b)
            for (c, r) in ((ca, ra), (cb, rb)):
                s.append(f'<circle cx="{hx(c,True)}" cy="{hy(r)}" r="0.75" fill="#7f8c8d"/>')
        # wires
        for no, a, b, lab, col in WIRES:
            (ca, ra), (cb, rb) = hole(a), hole(b)
            x1, y1, x2, y2 = hx(ca,True), hy(ra), hx(cb,True), hy(rb)
            if cb == "N" and rb >= 34:
                lane = hy(32.4) + (no - 12) * 0.55
                xe = hx("N",True) - 1.6 - (no - 12) * 0.45
                d = f"M{x1},{y1} L{x1},{lane} L{xe},{lane} L{xe},{y2} L{x2},{y2}"; tx, ty = x1, hy(30.5) - (no-12)*2.6
            elif rb == 42 and ca != cb:
                ymid = hy(39.6) + (no - 7) * 0.65
                d = f"M{x1},{y1} L{x1},{ymid} L{x2},{ymid} L{x2},{y2}"; tx, ty = x2, y2 - 3.2
            elif ra == rb == 10:
                depth = 1.9 if no == 5 else 3.6
                d = f"M{x1},{y1} C{x1},{y1+depth*1.3} {x2},{y2+depth*1.3} {x2},{y2}"; tx, ty = (x1+x2)/2, y1 + depth
            else:
                d = f"M{x1},{y1} L{x2},{y2}"
                if rb == 10: tx, ty = x2, hy(10) + 5.6
                elif rb == 42: tx, ty = x2, y2 - 3.2
                else: tx, ty = (x1+x2)/2 + 1.3, (y1+y2)/2
            s.append(f'<path d="{d}" fill="none" stroke="{col}" stroke-width="0.55" stroke-linejoin="round"/>')
            for (x, y) in ((x1, y1), (x2, y2)):
                s.append(f'<circle cx="{x}" cy="{y}" r="0.6" fill="{col}"/>')
            if rb == 10 and ra != 10:
                s.append(f'<line x1="{x2}" y1="{y2}" x2="{tx}" y2="{ty}" stroke="{col}" stroke-width="0.2" stroke-dasharray="0.4,0.4"/>')
            s.append(f'<circle cx="{tx}" cy="{ty}" r="1.1" fill="#fff" stroke="{col}" stroke-width="0.35"/>')
            s.append(f'<text x="{tx}" y="{ty+0.5}" font-size="1.35" text-anchor="middle" font-weight="bold" fill="{col}">{no}</text>')
        NET8 = {"E":"T_IRQ","F":"MISO","I":"SCK","K":"LED","N":"DC","O":"RST","P":"CS","G":"GP5","J":"GP7","L":"GP9","Q":"DIN","R":"GND","S":"BCK","T":"WSEL","C":"GND","H":"GND","M":"GND"}
        for c, n in NET8.items():
            x = hx(c,True)
            s.append(f'<text x="{x}" y="{hy(8)-1.4}" font-size="0.95" text-anchor="start" transform="rotate(-90 {x} {hy(8)-1.4})" fill="#111">{n}</text>')
        # landing labels
        s.append(f'<text x="{W+1.5}" y="{hy(7)+0.6}" font-size="1.4" fill="#d35400">B7 switch</text>')
        s.append(f'<text x="{W+1.5}" y="{hy(8)+2.4}" font-size="1.4" fill="#d35400">C7 OUT−</text>')
        for h in ("B7","C7"):
            c, r = hole(h)
            s.append(f'<circle cx="{hx(c,True)}" cy="{hy(r)}" r="1.0" fill="none" stroke="#d35400" stroke-width="0.35"/>')
        # sockets labels on back
        s.append(f'<text x="{W/2}" y="{hy(1)-1.7}" font-size="1.5" text-anchor="middle" fill="#111">Pico socket (top row, pin 40 … pin 21)</text>')
        s.append(f'<text x="{hx("R",True)-2}" y="{hy(10)+0.6}" font-size="1.5" text-anchor="end" fill="#c0392b">screen</text>')
        s.append(f'<text x="{hx("L",True)-2}" y="{hy(42)+0.6}" font-size="1.5" text-anchor="end" fill="#c0392b">SD</text>')
        # DAC + TP4056 ghost
        for (c1,r1,c2,r2,t,col) in (("A",32,"G",43,"TP4056 (on front)","#1f5fa8"),):
            xa, xb = hx(c1,True), hx(c2,True)
            x0, x1 = min(xa,xb)-P/2, max(xa,xb)+P/2
            s.append(f'<rect x="{x0}" y="{hy(r1)-P/2}" width="{x1-x0}" height="{hy(r2)-hy(r1)+P}" fill="none" stroke="{col}" stroke-width="0.3" stroke-dasharray="1,0.7"/>')
            s.append(f'<text x="{(x0+x1)/2}" y="{(hy(r1)+hy(r2))/2}" font-size="1.8" text-anchor="middle" fill="{col}">{t}</text>')
    s.append('</svg>')
    return "\n".join(s)

def ext_svg():
    # schematic of off-board wiring
    s = ['<svg xmlns="http://www.w3.org/2000/svg" width="180mm" height="120mm" viewBox="0 0 180 120" font-family="Helvetica,Arial,sans-serif" font-size="3.2">']
    def box(x, y, w, h, title, col, sub=""):
        s.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="2" fill="#fff" stroke="{col}" stroke-width="0.7"/>')
        s.append(f'<text x="{x+w/2}" y="{y+5.5}" text-anchor="middle" font-weight="bold" fill="{col}">{title}</text>')
        if sub: s.append(f'<text x="{x+w/2}" y="{y+10}" text-anchor="middle" font-size="2.6" fill="#555">{sub}</text>')
    def pin(x, y, t, anchor="start"):
        s.append(f'<circle cx="{x}" cy="{y}" r="0.9" fill="#333"/>')
        dx = 2 if anchor == "start" else -2
        s.append(f'<text x="{x+dx}" y="{y+1.1}" text-anchor="{anchor}" font-size="2.8">{t}</text>')
    def wire(pts, col, w=1.0):
        s.append(f'<polyline points="{pts}" fill="none" stroke="{col}" stroke-width="{w}" stroke-linejoin="round"/>')
    # battery
    box(5, 15, 40, 30, "LiPo battery", "#333", "3.7 V, 1000 mAh")
    pin(45, 26, "+  (red)", "end"); pin(45, 36, "−  (black)", "end")
    # TP4056
    box(70, 10, 42, 48, "TP4056 USB-C", "#1f5fa8", "with protection")
    pin(70, 26, "B+"); pin(70, 36, "B−")
    pin(112, 22, "OUT+", "end"); pin(112, 44, "OUT−", "end")
    s.append('<text x="91" y="54" text-anchor="middle" font-size="2.6" fill="#1f5fa8">USB-C here (charging)</text>')
    wire("45,26 70,26", "#d62728", 1.2); wire("45,36 70,36", "#222", 1.2)
    s.append('<text x="57.5" y="23.5" text-anchor="middle" font-size="2.5">JST PH2.0 plug</text>')
    # switch
    box(122, 6, 26, 26, "Slide switch", "#333", "SS12D00")
    pin(126, 26, "middle"); pin(140, 26, "side")
    wire("112,22 117,22 117,30 126,30 126,26", "#d62728", 1.2)
    # board landing
    box(122, 62, 54, 52, "Perfboard", "#2e8b57", "landing holes")
    pin(130, 75, "B7  → D1 → VSYS"); pin(130, 90, "C7  = GND")
    wire("140,26 140,22 160,22 160,50 128,50 128,75 130,75", "#d62728", 1.2)
    wire("112,44 116,44 116,90 130,90", "#222", 1.2)
    s.append('<text x="164" y="40" font-size="2.5" fill="#d62728" transform="rotate(90 164 40)">switched battery +</text>')
    s.append('<text x="5" y="62" font-size="2.6" fill="#555">Thick lines: 24 AWG silicone (red = +, black = −).  Thin lines: 30 AWG wire-wrap wire.</text>')
    s.append('</svg>')
    return "\n".join(s)

front = board_svg(False)
SC = 1.75
backv = board_svg(True, True)
SC = 2.0
ext = ext_svg()

wire_rows = "".join(f"<tr><td><b style=\"color:{c}\">{n}</b></td><td>{a}</td><td>{b}</td><td>{t}</td></tr>" for n, a, b, t, c in WIRES)

H = []  # (hole, what, connection)
top = {1:"VBUS",2:"VSYS",3:"GND",4:"3V3_EN",5:"3V3",6:"ADC_VREF",7:"GP28",8:"AGND",9:"GP27",10:"GP26",11:"RUN",12:"GP22",13:"GND",14:"GP21",15:"GP20",16:"GP19",17:"GP18",18:"GND",19:"GP17",20:"GP16"}
pinno_top = {c: 41-c for c in range(1,21)}
conn1 = {"B":"solder bridge to B2","E":"wire 1 starts here","G":"wire 11 ends here","H":"solder bridge to H2"}
for c in range(1,21):
    ch=L[c-1]
    if ch in conn1: H.append((f"{ch}1", f"Pico pin {pinno_top[c]} ({top[c]})", conn1[ch]))
H.append(("rest of row 1", "Pico socket pins", "socket solder joint only"))
H += [("B2","D1, band end","bridge to B1"),("H2","R2 + 100 nF","bridge to H1"),
      ("B6","D1, plain end","bridges to B7 and C6"),("C6","R1","bridge to B6"),("G6","R1","bridge to H6; wire 11 starts"),
      ("H6","R2 + 100 nF","bridge to G6"),("B7","red wire from switch","bridge to B6"),("C7","black wire from TP4056 OUT−","bridge to C8")]
bot = {1:"GP0",2:"GP1",3:"GND",4:"GP2",5:"GP3",6:"GP4",7:"GP5",8:"GND",9:"GP6",10:"GP7",11:"GP8",12:"GP9",13:"GND",14:"GP10",15:"GP11",16:"GP12",17:"GP13",18:"GND",19:"GP14",20:"GP15"}
conn8 = {"C":"bridge to C7","E":"LINK to E10","F":"LINK to F10","G":"wire 6 starts","I":"LINK to I10","J":"wire 3 starts","K":"LINK to K10","L":"wire 10 starts",
         "N":"LINK to N10","O":"LINK to O10","P":"LINK to P10","Q":"wire 15 starts","R":"wires 2 and 13 start","S":"wire 16 starts","T":"wire 14 starts"}
for c in range(1,21):
    ch=L[c-1]
    if ch in conn8: H.append((f"{ch}8", f"Pico pin {c} ({bot[c]})", conn8[ch]))
H.append(("A8, B8, D8, H8, M8", "Pico socket pins", "socket solder joint only"))
for ch in "EFIKNOP":
    H.append((f"{ch}9", "link only", f"part of the {ch}8–{ch}10 link"))
H.append(("other row 9", "<b>empty</b>", "<b>never solder G9, H9, J9, L9, M9, Q9, R9</b>"))
conn10 = {"E":"LINK from E8","F":"LINK from F8; wire 8 starts","G":"wires 3 and 4","H":"wire 6 ends","I":"LINK from I8; wires 5 and 7",
          "J":"<b>nothing</b> (SDO stays unconnected)","K":"LINK from K8","L":"wire 5 ends","M":"wires 4 and 9","N":"LINK from N8","O":"LINK from O8",
          "P":"LINK from P8","Q":"wire 2 ends","R":"wire 1 ends; wire 12 starts"}
for k, n in enumerate(J3):
    ch=L[17-k]; H.append((f"{ch}10", f"screen pin {k+1} ({n})", conn10[ch]))
for k, n in enumerate(J4):
    ch=L[11-k]; H.append((f"{ch}42", f"SD pin {k+1} ({n})", {"L":"wire 10 ends","K":"wire 9 ends","J":"wire 8 ends","I":"wire 7 ends"}[ch]))
for h, n in DAC_JP2:
    H.append((h, f"DAC pin {n}", {"VIN":"wire 12 ends","GND":"wire 13 ends","WSEL":"wire 14 ends","DIN":"wire 15 ends","BCK":"wire 16 ends","Lout":"<b>nothing</b>"}[n]))
H.append(("T35–T40", "DAC pins DE, FIL, MCK, MU, FM, 3V", "<b>nothing</b>: solder joint only"))
def maprows(lst):
    return "".join(f"<tr><td>☐</td><td><b>{h}</b></td><td>{w}</td><td>{c}</td></tr>" for h,w,c in lst)
half=(len(H)+1)//2
hm1, hm2 = maprows(H[:half]), maprows(H[half:])
html = f'''<!doctype html><html><head><meta charset="utf-8"><style>
@page {{ size: A4; margin: 11mm 12mm; }}
body {{ font-family: Helvetica, Arial, sans-serif; font-size: 10pt; line-height: 1.38; color:#111; }}
h1 {{ font-size: 17pt; margin: 0 0 2mm; }} h2 {{ font-size: 13pt; margin: 0 0 2mm; }}
.page {{ page-break-after: always; }} .page:last-child {{ page-break-after: auto; }}
table {{ border-collapse: collapse; width: 100%; font-size: 9pt; }}
td, th {{ border: 0.2mm solid #bbb; padding: 0.8mm 1.6mm; text-align: left; vertical-align: top; }}
th {{ background: #eee; }}
.two {{ display: flex; gap: 6mm; }} .note {{ font-size: 9pt; color: #333; }}
ol, ul {{ margin: 1mm 0 2mm 5mm; padding-left: 3mm; }} li {{ margin-bottom: 0.8mm; }}
</style></head><body>


<div class="page">
<h1>Pod perfboard: solder side</h1>
<div class="two"><div>{backv}</div>
<div class="note" style="width:82mm">
<p>This is the board <b>turned over</b>: column A is on the right. All of this is on the solder side.</p>
<p><b>Dark grey bars (rows 8 to 10): 7 links.</b> In columns E, F, I, K, N, O and P, the Pico pin sits directly above the
screen pin it drives. Lay a cut-off resistor leg across holes 8, 9 and 10 and solder it. No wire needed.</p>
<p><b>Red ✕ (row 9): never join these.</b> In columns G, H, J, L, M, Q and R, the top and bottom are different signals
(R8 is GND and R10 is 3.3 V, for example).</p>
<p><b>Light grey bars:</b> solder bridges between neighbouring holes under the Pico: B1–B2, B6–B7, B6–C6, G6–H6, H1–H2, C7–C8.</p>
<p><b>Coloured lines: real wires</b>, 30 AWG insulated wire-wrap wire. <b>16</b> in all: 11 for the screen and battery, 5 for the DAC.</p>
<table style="font-size:7.6pt"><tr><th>#</th><th>From</th><th>To</th><th>Signal</th></tr>{wire_rows}</table>
<p style="font-size:8.5pt">100 nF capacitor: across R2 (H2 and H6).</p>
</div></div>
</div>

<div class="page">
<h2>Hole-by-hole map</h2>
<p class="note">Every hole that gets solder, row by row. Tick each one as you do it. Any hole not listed stays empty.</p>
<div class="two">
<table style="font-size:8pt"><tr><th></th><th>Hole</th><th>What's in it</th><th>Joins to</th></tr>{hm1}</table>
<table style="font-size:8pt"><tr><th></th><th>Hole</th><th>What's in it</th><th>Joins to</th></tr>{hm2}</table>
</div>
</div>

<div class="page">
<h2>Front view: where everything sits</h2>
<div class="two"><div>{front}</div>
<div class="note" style="width:62mm">
<p><b>Holes</b> are named by column letter (A–T, left to right on the front) and row number (1–43, top to bottom).
Example: <b>B7</b>.</p>
<p><b>Orange rings:</b> B7 and C7, where the switch wire and the TP4056 OUT− wire come in.</p>
<p><b>Black boxes:</b> two 1×20 sockets for the Pico, rows 1 and 8, all 20 columns. USB end on the left.</p>
<p><b>Red boxes:</b> screen sockets. 1×14 in row 10, columns E–R. 1×4 in row 42, columns I–L.</p>
<p><b>Under the Pico</b> (it sits 8.5 mm up, on its sockets): D1 from B2 (band end) to B6, R1 from C6 to G6, R2 from H2 to H6.
Lay them flat.</p>
<p><b>Antenna end (right):</b> no wires or parts on the front under it.</p>
<p><b>Bottom left:</b> TP4056, stuck on with foam tape, USB-C poking 1 mm past the bottom edge.</p>
<p><b>Bottom right:</b> DAC, jack down at the bottom edge. Its wire row goes in column N (N34–N39), the other row in column T (T35–T40). It overhangs the right edge by about 1 mm.</p>
<p><b>Grey dashed area:</b> the screen's SD-card holder sits above it. Keep it clear.</p>
<p>Everything under the screen must stay under <b>10 mm</b> tall.</p>
<p><b>Cutting the board:</b> score along a line of holes on both sides, 6–8 times with the box cutter against the ruler,
then snap it over a table edge and sand. You need 20 holes across and 43 down.</p>
</div></div>

</div>

<div class="page">
<h2>Off-board wires: battery, charger, switch</h2>
{ext}
<h2 style="margin-top:4mm">Build order</h2>
<ol>
<li>Cut the board. Solder the two Pico sockets (plug the Pico in first so they line up, then solder).</li>
<li>Solder the 1×14 screen socket in row 10. For the 1×4 SD socket: plug it onto the screen's SD pins, plug the
screen into the 1×14 socket, push the 1×4 through row 42, then solder. This lines it up exactly.</li>
<li>D1, R1, R2, the capacitor and the six solder bridges.</li>
<li>The 7 links, then wires 1–10. <b>Test:</b> Pico in, screen on, USB from the laptop only (no battery). Home screen, touch and SD card should work.</li>
<li>DAC: pins through N34–N39 and T35–T40, solder, then wires 12–16. <b>Check first:</b> N35 must beep to the jack’s outer metal tab (that pin is GND). <b>Test:</b> play music from the SD card on headphones.</li>
<li>Wire 11, then the TP4056, switch and battery wires. <b>Before plugging in the battery:</b> with the multimeter on
continuity, check B7 to C7 does <b>not</b> beep, and check red really goes to B+.</li>
<li>Plug in the battery, switch on. Kapton tape over the back, then stick the battery on with foam tape.</li>
</ol>
</div>

<div class="page">
<h2>Shopping list</h2>
<table>
<tr><th>Item</th><th>Qty</th><th>Notes</th></tr>
<tr><td>Perfboard, <b>double-sided, plated holes</b>, 2.54 mm, 9 × 15 cm</td><td>1</td><td>green FR-4, not brown paper board. Cut to 20 × 43 holes.</td></tr>
<tr><td>Female header strip 1 × 40, 2.54 mm (8.5 mm tall)</td><td>3</td><td>Pico 2 × 20, screen 1 × 14, SD 1 × 4. Each cut wastes one pin.</td></tr>
<tr><td>TP4056 charger module, <b>USB-C, with protection</b></td><td>1–2</td><td>must have 6 pads: IN+, IN−, B+, B−, OUT+, OUT−</td></tr>
<tr><td>LiPo 3.7 V, 1000 mAh, size 503450 (5 × 34 × 50 mm)</td><td>1</td><td>with protection board and JST PH2.0 plug</td></tr>
<tr><td>JST PH2.0 2-pin socket with wires</td><td>1</td><td>matches the battery's plug</td></tr>
<tr><td>Slide switch SS12D00 (3 pins)</td><td>2</td><td>power switch, fitted in the case wall</td></tr>
<tr><td>Schottky diode 1N5819</td><td>2</td><td>D1</td></tr>
<tr><td>Resistor 100 kΩ, ¼ W</td><td>3</td><td>R1, R2</td></tr>
<tr><td>Ceramic capacitor 100 nF ("104")</td><td>2</td><td>soldered across R2</td></tr>
<tr><td>Wire-wrap wire, 30 AWG (Kynar), 2–3 colours</td><td>1 roll each</td><td>all signal wires on the back</td></tr>
<tr><td>Silicone wire, 24 AWG stranded, red + black</td><td>1 m each</td><td>battery, charger, switch</td></tr>
<tr><td>Solder 0.6–0.8 mm with flux, flux pen, desolder wick</td><td></td><td></td></tr>
<tr><td>Kapton tape, double-sided foam tape, 2 mm heat-shrink</td><td></td><td>insulate the back before the battery goes on</td></tr>
</table>
<p class="note">Tools: temperature-controlled iron (330–350 °C), flush cutters, a stripper that handles 30 AWG,
<b>multimeter</b>, box cutter and steel ruler.</p>
<h2>Which wire where</h2>
<ul>
<li><b>Signals (the 11 numbered wires):</b> 30 AWG wire-wrap wire, single core. Thin, stays where you bend it, and its
insulation doesn't melt back when you solder next to it.</li>
<li><b>Battery, charger, switch:</b> 24 AWG stranded silicone. Flexible, carries over 1 A. Red for +, black for −.</li>
<li><b>Neighbouring holes</b> (marked grey on the back view): join with a blob of solder, or a scrap of cut-off resistor lead.</li>
<li>No Dupont jumper wires in the final build: they're tall and work loose.</li>
</ul>
<h2>Battery and charging</h2>
<ul>
<li>The battery only connects to the <b>TP4056</b> (B+ / B−), never straight to the Pico.</li>
<li>TP4056 <b>OUT+ → power switch → D1 → Pico VSYS</b>. D1 stops the Pico's USB from pushing 5 V into the battery,
so you can plug in the Pico's micro-USB to flash firmware at any time.</li>
<li>Charge through the TP4056's USB-C. Red LED = charging, blue/green = full. It charges at 1 A, which is fine for a
1000 mAh cell. For a proper full charge, switch the Pod off while it charges.</li>
<li>R1 and R2 halve the battery voltage so GP28 can read it. The firmware needs a small update to show the
percentage from this; until then the battery icon stays hidden and everything else works.</li>
<li><b>Check the plug polarity</b> before connecting: cheap PH2.0 batteries are sometimes wired backwards. Red must
reach B+. Never cut both battery wires at once.</li>
</ul>

</div>
</body></html>'''
open(__import__("sys").argv[1], "w").write(html)
