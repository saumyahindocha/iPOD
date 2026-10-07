---
pagetitle: Pod PCB in EasyEDA Pro — beginner guide
---

# Pod PCB in EasyEDA Pro — beginner guide

This guide takes you from a blank EasyEDA account to a finished, reviewed **Power sheet**, then
shows how the rest of the board follows. Every value and connection comes from `docs/pcb-plan.md`
(the spec). This guide explains how to draw it, click by click.

**How we work together:** you draw one sheet, export it as a PDF, and send it to me. I check every
connection against the spec, and you fix anything I find before starting the next sheet. Fixing a
mistake in the schematic takes a minute. After the boards are made, the same mistake means ordering
them again.

> Menu names below come from EasyEDA Pro's own documentation. EasyEDA updates its menus from time
> to time. If a name differs slightly, look for the nearest match. Every menu item also shows its
> keyboard shortcut on the right, so check there instead of memorising shortcuts from this guide.

---

## Part A — The ideas you need first (5 minutes)

| Word | What it means |
|---|---|
| **Schematic** | The circuit drawing: which pin connects to which. No sizes or positions, just connections. |
| **PCB** | The physical board: where each part sits and where the copper tracks run. EasyEDA builds it from the schematic. |
| **Symbol** | How a part looks in the schematic (a box with named pins). |
| **Footprint** | The copper pads the part is soldered onto on the PCB. Every symbol is linked to one. |
| **Net** | One electrical connection. Every pin on the same net is joined by copper. |
| **Net label** | A name written on a wire, for example `VBAT`. Two wires with the same label are connected even with no line drawn between them, which keeps the drawing tidy. |
| **Net flag** | A ground or power symbol. Every GND flag is the same `GND` net. |
| **Designator** | The part's ID: R1, C3, U2. |
| **LCSC number** | The stock number JLCPCB uses to assemble the part, for example `C12345`. Parts from EasyEDA's library already carry one. |
| **Basic / Extended part** | JLCPCB keeps *Basic* parts loaded on its machines, so they cost nothing extra. Each *Extended* part adds about $3 to the order. Use Basic for resistors and capacitors. |
| **Decoupling capacitor** | A small capacitor from a chip's power pin to GND that smooths its supply. "C1 10 µF to GND" means one capacitor pin on the net and the other pin on GND. |
| **Pull-up resistor** | A resistor from a signal to 3V3, so the signal reads high unless something pulls it low. |
| **ERC / DRC** | Automatic checks: ERC finds schematic mistakes (an unconnected pin, say) and DRC finds board mistakes (tracks too close). |

**The one rule that matters most:** two points connect only when a wire *ends exactly on a pin*
(the pin end shows a dot or highlights), or when both carry the same net name. A wire that only
crosses a pin is not connected.

---

## Part B — Set up (10 minutes)

### B1. Open EasyEDA Pro

1. Go to **pro.easyeda.com** and sign in. A JLCPCB or LCSC account works here, and you'll use the
   same account to order.
2. Use **EasyEDA Pro**, not "Std Edition". Pro supports multi-page schematics and imports KiCad
   files. If the editor asks you to choose a mode, pick the **online / cloud** mode, so your
   project saves to your account.
3. Use a laptop with a mouse, not a touchpad, in Chrome or Edge.

### B2. Create the project

1. **File → New → Project** (or **New Project** on the start page).
2. Name it **Pod Rev A**, leave the rest, and click **Save**.
3. The left panel shows the project tree: a **Board**, containing a **Schematic** and a **PCB**.
   The Schematic can hold several **pages**.

### B3. Make the 4 pages

1. In the left panel, right-click the schematic → **New Schematic** / **New Page** (EasyEDA calls
   them "schematic pages"). Make 4 pages in total.
2. Right-click each page → **Rename**: `1 Power`, `2 Pico`, `3 Audio`, `4 Screen`.
3. All four pages belong to **one** schematic, so a net named `3V3` on page 1 is the same net as
   `3V3` on page 2. We'll confirm this later when the PCB shows every part linked up.

### B4. Learn the editor (try each of these once on a blank page)

| What | How |
|---|---|
| Zoom | mouse wheel |
| Move the view | hold the right mouse button and drag (or the middle button) |
| Select | left-click; drag a box to select several |
| Move a part | drag it, or select it and use the arrow keys |
| Rotate while placing or moving | **Space** (or **R**; check the menu) |
| Cancel the current tool | **Esc** or a right-click |
| Undo / redo | **Ctrl+Z** / **Ctrl+Y** |
| Delete | select → **Delete** |
| Save | **Ctrl+S** (do this often) |
| Properties | select something; the **right-hand panel** shows its name, value, designator, footprint and LCSC number |

**Bottom panel:** the **Library** (part search) and the check results open here. You can drag the
panel taller.

---

## Part C — Placing parts and drawing connections

Practise each step once on a page, then delete everything before starting the real drawing.

### C1. Find and place a part

1. Open the library: **Place → Devices** (or the **Library** tab in the bottom panel).
2. In the search box, type the **part number (MPN)**, for example `BQ24074RGTR`, and press Enter.
3. Click a result. EasyEDA shows the symbol, the footprint, the LCSC number, stock, and whether
   it's **Basic** or **Extended**. Check:
   - the **package** matches the spec (BQ24074 → VQFN-16), and
   - JLCPCB has **stock**: a Basic or Extended part with a JLCPCB number and stock above zero.
4. Click **Place**, move the mouse to the page, and left-click to drop it. Press **Esc** to stop
   placing more copies.

**Resistors and capacitors:** don't place a generic resistor and type in its value; it would have
no LCSC number. Search the exact value and size instead:

- `1.8k 0402` → pick a **Basic** 1 % 0402 result.
- `10uF 0603` → pick a Basic 0603 capacitor rated for **10 V or more**.

Once you have one 5.1 kΩ resistor placed, **copy and paste** it (Ctrl+C, Ctrl+V) for the second.
The copy keeps the LCSC number, and EasyEDA gives it a new designator.

**Sizes in this design:** resistors 0402; 100 nF and 1 µF capacitors 0402; 4.7 µF and 10 µF
capacitors 0603; 22 µF capacitors 0805.

### C2. Draw a wire

1. **Place → Wire**. The shortcut is shown next to it, usually **W** or **Alt+W**.
2. Click on a pin's **end point** to start, click to add corners, and click on the other pin's end
   to finish. Right-click to stop.
3. Check that each wire end shows the connection marker. A wire end lying near a pin, but not on
   it, is a classic beginner mistake. ERC will catch it, but it's quicker to look as you go.

### C3. Name a wire with a net label (the tidy way)

Instead of drawing long wires across the page, draw a **short stub** from the pin and put a label
on it:

1. Draw a short wire (2–3 grid squares) out from the pin.
2. **Place → Net Label**. Press **Tab** before clicking to type the name, for example `VBUS`.
3. Click on the stub wire to drop the label. The label's corner must sit **on the wire**.
4. Do the same at the other end with the **exact same name**. They are now connected.

Names must match **exactly**, including capitals: `VBAT` ≠ `Vbat`. Use only the names from the
spec.

### C4. Ground and power symbols

- **GND:** **Place → Net Flag** → choose **GND** and put it at the end of a short stub. Each GND
  flag joins the `GND` net, so use as many as you like.
- **Power nets** (`3V3`, `VSYS`, `VBAT`, `VBUS`): either use a power net flag and **rename it** to
  the exact net name in the right panel, or simply use a **net label**. Both work; pick one style
  and use it throughout. Net labels are the safest choice, because a power flag's default name
  (like `VCC`) would quietly create a wrong net.

### C5. Mark unused pins

Pins the spec says to leave open (BQ24074 **TMR**, USB-C **SBU1/SBU2**): **Place → No Connect
Flag** and click the pin end. This tells ERC the pin was left open on purpose.

### C6. The exposed pad

Several chips (BQ24074, MAX17048) have a metal pad underneath, shown as an extra pin
named **EP**, **PAD**, **PowerPAD** or **thermal pad**. Connect it to **GND** unless the spec says
otherwise.

---

## Part D — Drawing Sheet 1: Power, step by step

Open page **1 Power**. Draw it in four groups from left to right, as the power flows:
**USB-C → charger and battery → power switch → fuel gauge.** Leave space between the
groups.

Tick each line as you draw it. When a chip's pin name in EasyEDA differs from the spec, match by
the pin's **function** in the chip's datasheet (click the part → the datasheet link in the right
panel) and tell me when you send the PDF.

### D1. USB-C socket (J1)

Search `TYPE-C-31-M-12` (or `USB4105-GF-A`). Choose a **16-pin** version, not a 6-pin
charge-only socket and not a 24-pin socket.

Some symbols join pins with the same job into one pin, labelled for example `A4B9`, or simply
`VBUS`. That's fine: connect each pin that exists.

- [ ] All **VBUS** pins (A4, A9, B4, B9) → label `VBUS`
- [ ] All **GND** pins (A1, A12, B1, B12) and the **shell / SHIELD / EH** pins → `GND`
- [ ] **CC1** (A5) → R1 **5.1 kΩ** → GND
- [ ] **CC2** (B5) → R2 **5.1 kΩ** → GND (one resistor each; never share one)
- [ ] **D+** (A6, B6), **D−** (A7, B7), **SBU1**, **SBU2** → No Connect flag. The USB-C socket
      only charges; programming goes through the Pico's own micro-USB.

*Why the CC resistors:* a USB-C charger outputs 0 V until it sees 5.1 kΩ on CC. Leaving them out
is the most common USB-C mistake.

### D2. Charger (U2)

Search `BQ24074RGTR` (VQFN-16). Put it in the middle of the page with room around it, because
nine of its pins get a part.

- [ ] **IN** → `VBUS`, and C1 **4.7 µF** from IN to GND
- [ ] **OUT** → `VSYS`, and C2 **10 µF** from OUT to GND
- [ ] **BAT** → `VBAT`, and C3 **10 µF** from BAT to GND
- [ ] **ISET** → R4 **1.8 kΩ** → GND (sets about 0.5 A charging)
- [ ] **ILIM** → R5 **1.2 kΩ** → GND (sets the 1.34 A input limit)
- [ ] **ITERM** → R6 **3.0 kΩ** → GND (charging stops at 50 mA)
- [ ] **TS** → R3 **10 kΩ** → GND
- [ ] **EN2** → `VBUS`
- [ ] **EN1** → GND
- [ ] **CE** → GND
- [ ] **SYSOFF** → GND
- [ ] **TMR** → No Connect flag
- [ ] **CHG** → label `CHG_N`, and R7 **100 kΩ** from `CHG_N` to `3V3`
- [ ] **PGOOD** → label `PGOOD_N`, and R8 **100 kΩ** from `PGOOD_N` to `3V3`
- [ ] **VSS** and the exposed pad → GND

**How to draw a "pull-up":** put the resistor vertically. Its top pin goes to a stub labelled `3V3`,
and its bottom pin goes to a stub labelled `CHG_N`. A third stub labelled `CHG_N` sits on the chip
pin. All three are now correctly joined, with no long wires.

### D3. Battery connector (J2)

Search `S2B-PH-SM4-TB` (JST-PH, 2 pins, side-entry SMD).

- [ ] Pin 1 → `VBAT`
- [ ] Pin 2 → `GND`

On the page, put a text note next to it: **"check battery polarity"**: **Place → Text**. LiPo
cells come wired either way round. Plugging in a reversed one destroys the charger.

### D4. Power/hold switch (SW1)

Search `MSK-12C02`. It has three main pins: one in the middle (common) and one at each side, plus
mounting pins.

- [ ] **Middle (common)** → label `3V3_EN`
- [ ] **One side** (OFF) → `GND`
- [ ] **Other side** (ON) → No Connect flag. The Pico holds 3V3_EN high by itself, so "on" just
      means "not grounded".
- [ ] Mounting/shell pins → GND (or No Connect)

### D5. Fuel gauge (U3)

Search `MAX17048G+T10` (TDFN-8).

- [ ] **VDD** → `VBAT`, and C4 **1 µF** from VDD to GND
- [ ] **CELL** → `VBAT`
- [ ] **GND**, **CTG**, **QSTRT** and the exposed pad → GND
- [ ] **SDA** → label `I2C_SDA`, with R9 **4.7 kΩ** to `3V3`
- [ ] **SCL** → label `I2C_SCL`, with R10 **4.7 kΩ** to `3V3`
- [ ] **ALRT** → label `GAUGE_ALRT_N`, with R11 **10 kΩ** to `3V3`

Labels like `CHG_N`, `PGOOD_N`, `I2C_SDA`, `I2C_SCL`, `GAUGE_ALRT_N`, `VSYS`, `3V3` and `3V3_EN`
go nowhere on this page. That's expected: they connect to the Pico on page 2.

### D6. Tidy up

1. **Designators:** if numbers are messy or duplicated, run **Design → Annotate** (the wording
   might be "Annotate Designator"). Then compare the resistors and capacitors with the spec. Matching the
   spec's numbers makes my review faster, but it isn't essential: I check by connection.
2. **Values visible:** every resistor and capacitor should show its value on the page. If one
   doesn't, select it and switch on the value in the right panel.
3. **Text notes:** add a title at the top of the page: **Place → Text** → "Power: USB-C,
   charger, battery, power switch, fuel gauge".

### D7. Check it

1. **Design → Check DRC** (for a schematic this runs the electrical check, ERC). Results show in
   the bottom panel. Click a result to jump to it.
2. What to fix, and what to leave:

| Message (roughly) | Meaning | Action |
|---|---|---|
| Pin not connected | a pin has nothing on it | connect it, or add a No Connect flag if the spec says open |
| Net has only one pin / single-pin net | a label is spelled differently from its partner, or a wire end missed a pin | fix the spelling or the wire. Exception: labels that continue on page 2 (`CHG_N`, `I2C_SDA`, …) will show this until page 2 exists, so leave those |
| Duplicate designator | two parts share a name | run Annotate |
| Part has no footprint | rare with library parts | tell me which part |

3. **Ctrl+S.**

### D8. Send it to me

1. **Export → PDF/Image** (in some versions under **File → Export**). Choose **PDF**, select
   **all pages** (or only page 1 for now), and use **colour** if offered.
2. Also export the **BOM**: **Export → Bill of Materials (BOM)**, as a CSV or Excel file. This
   lets me check every package and LCSC number.
3. Send both here. I'll reply with a checklist of anything to change.

---

## Part D2 — Drawing Sheet 2: the Pico 2 W

The board carries your own Pico 2 W, soldered on. Its chip, flash, radio and 3.3 V regulator are
already on the Pico, so this page is just the Pico and a label on each pin.

**First, tidy the pages:** right-click **2. MCU** → **Rename** → `2. Pico`. Right-click
**3. Radio** → **Delete**. Rename the last two to `3. Audio` and `4. Screen`.

### P1. Place the Pico (as two 20-pin headers)

EasyEDA's Pico parts only have flat surface pads, and your Pico has header pins that need holes.
So the Pico is drawn as **two straight 1×20, 2.54 mm through-hole headers**:

1. In the library search box (**LCSC Electronics** tab), type **`C50981`**: BOOMELE 2.54-1*20P,
   a straight 1×20 through-hole pin header. Place it twice and name them **J6** and **J7**.
2. Set **Add into BOM → No** on both (they're only holes for the Pico's own pins).
3. Add a text note: "J6 = Pico pins 1–20, J7 = Pico pins 21–40".
4. **J6 pin n = Pico pin n. J7 pin n = Pico pin 20 + n.** Label them from the tables below using
   the Pico pin numbers.
5. On the board: J6 pin 1 at (1.87, 20.39) running right; J7 pin 1 at (50.13, 2.61) running left
   (rows 17.78 mm apart, matching the Pico).

### P2. Power pins

- [ ] **VSYS** (pin 39) → `VSYS`
- [ ] **VBUS** (pin 40) → `VBUS`
- [ ] **3V3** (pin 36, "3V3(OUT)") → `3V3`, with a **10 µF 0603** capacitor from `3V3` to GND
- [ ] **3V3_EN** (pin 37) → `3V3_EN`
- [ ] **Every GND pin** (3, 8, 13, 18, 23, 28, 38) and **AGND** (33) → `GND`

### P3. GPIO labels

One short stub and label on each pin. The names must match exactly, because they join the Pico to
the other pages.

| Pin | Pico | Label | | Pin | Pico | Label |
|---|---|---|---|---|---|---|
| 2 | GP1 | `HP_DET` | | 21 | GP16 | `LCD_DC` |
| 4 | GP2 | `GAUGE_ALRT_N` | | 22 | GP17 | `LCD_CS` |
| 6 | GP4 | `I2C_SDA` | | 24 | GP18 | `SPI0_SCK` |
| 7 | GP5 | `I2C_SCL` | | 25 | GP19 | `SPI0_MOSI` |
| 9 | GP6 | `BTN_PLAY_N` | | 26 | GP20 | `SPI0_MISO` |
| 10 | GP7 | `SD_CS` | | 27 | GP21 | `LCD_RST` |
| 12 | GP9 | `I2S_DIN` | | 29 | GP22 | `TOUCH_CS` |
| 14 | GP10 | `I2S_BCK` | | 31 | GP26 | `TOUCH_IRQ` |
| 15 | GP11 | `I2S_LRCK` | | 32 | GP27 | `PGOOD_N` |
| 16 | GP12 | `DAC_XSMT` | | 34 | GP28 | `CHG_N` |
| 17 | GP13 | `LCD_LED` | | | | |

These are the same pins your breadboard uses, so the firmware runs unchanged.

### P4. Unused pins

- [ ] No Connect flag on **GP0** (1), **GP3** (5), **GP8** (11), **GP14** (19), **GP15** (20),
      **RUN** (30), **ADC_VREF** (35), and the 3 debug pins (SWCLK, GND, SWDIO) if the symbol
      has them.

### P5. Check and send

1. **Design → Annotate**, then **Design → Check DRC**. The "single pin" warnings for `CHG_N`,
   `I2C_SDA` and so on from the Power page should now be gone, because both ends exist.
2. **Back on the Power page**, make the two changes from the new plan, if you haven't yet:
   - delete the 3.3 V regulator group (TPS63802, inductor, its capacitors and the 56k/10k/100k
     resistors) and the USBLC6-2SC6;
   - USB-C data pins and SBU pins → No Connect; switch common → `3V3_EN`, one side `GND`, the
     other side No Connect.
3. Send the PDF of both pages, plus the **BOM**.

---

## Part D3 — Drawing Sheet 3: Audio, step by step

Open page **3 Audio**. It has two groups: the **DAC** (it turns the Pico's digital music into an
analog signal) and the **headphone jack**.

**Parts on this page** (all 0402 Basic unless noted):

| Value | Count | Notes |
|---|---|---|
| 100 nF | 3 | |
| 10 µF **0603** | 3 | rated 10 V or more |
| 2.2 µF | 2 | rated 10 V or more |
| 1 µF | 1 | |
| 2.2 nF | 2 | type C0G / NP0 |
| 470 Ω | 2 | |
| 10 kΩ | 1 | |
| 4.7 kΩ | 1 | |
| Ferrite bead 600 Ω @ 100 MHz, **0603** | 1 | search **`C1002`** (Sunlord GZ1608D601TF, Basic) |

### A1. The DAC (U8)

Search `PCM5102APWR` (TSSOP-20). Its pins are numbered 1–20; the names below match the symbol.

**Power**

- [ ] **CPVDD** (1) → `3V3`, with **100 nF** and **10 µF** to GND
- [ ] **CPGND** (3) → GND
- [ ] **CAPP** (2) ↔ **CAPM** (4): one **2.2 µF** capacitor *between these two pins* (neither end to GND)
- [ ] **VNEG** (5) → **2.2 µF** → GND
- [ ] **AVDD** (8) → net `AVDD_DAC`. Then the **ferrite bead** from `3V3` to `AVDD_DAC`, and
      **10 µF** + **100 nF** from `AVDD_DAC` to GND. *(The bead keeps digital noise out of the
      analog supply.)*
- [ ] **AGND** (9) → GND
- [ ] **DVDD** (20) → `3V3`, with **100 nF** and **10 µF** to GND
- [ ] **DGND** (19) → GND
- [ ] **LDOO** (18) → **1 µF** → GND (nothing else on this pin)

**Digital inputs from the Pico**

- [ ] **BCK** (13) → `I2S_BCK`
- [ ] **DIN** (14) → `I2S_DIN`
- [ ] **LRCK** (15) → `I2S_LRCK`
- [ ] **SCK** (12) → GND *(the DAC makes its own clock, exactly as on your breadboard)*
- [ ] **XSMT** (17) → `DAC_XSMT`, and **10 kΩ** from `DAC_XSMT` to GND *(muted until the
      firmware un-mutes it: no pop at power-on)*

**Settings pins**

- [ ] **DEMP** (10), **FLT** (11), **FMT** (16) → GND

**Outputs**

- [ ] **OUTL** (6) → **470 Ω** → label `HP_L`, and **2.2 nF** from `HP_L` to GND
- [ ] **OUTR** (7) → **470 Ω** → label `HP_R`, and **2.2 nF** from `HP_R` to GND

### A2. The headphone jack (J5)

Search **`C668606`** (SHOU HAN **PJ-342**). Pins, from its datasheet:

- [ ] Pins **5** and **2** (ground contacts) → GND
- [ ] Pin **6** (left spring) → `HP_L`
- [ ] Pin **3** (right spring) → `HP_R`
- [ ] Pin **7** (switch contact under spring 6) → **4.7 kΩ** → `HP_DET`
- [ ] Pin **4** (switch contact under spring 3) → No Connect flag

### A3. Check and send

1. **Design → Annotate**, then **Design → Check DRC**. `I2S_BCK`, `I2S_DIN`, `I2S_LRCK`,
   `DAC_XSMT` and `HP_DET` now have both ends, so their warnings should go.
2. Send the PDF (all pages) and the **BOM**.

---

## Part D4 — Drawing Sheet 4: Screen, step by step

Open page **4 Screen**. This page has the two sockets the red screen module plugs into, and the
play/pause button. The mounting holes are added later, on the board layout.

### S1. Main screen socket (J3, 14 pins)

Search **`C5307340`** (ZHOURI **PM2.54-1\*14**: 1×14 female header, 2.54 mm, 8.5 mm tall). Name it
**J3**. Pin 1 is the module's **VCC** pin, the first one printed on the module's header.

| J3 pin | Module pin | Net |
|---|---|---|
| 1 | VCC | `3V3` |
| 2 | GND | `GND` |
| 3 | CS | `LCD_CS` |
| 4 | RESET | `LCD_RST` |
| 5 | DC | `LCD_DC` |
| 6 | SDI (MOSI) | `SPI0_MOSI` |
| 7 | SCK | `SPI0_SCK` |
| 8 | LED | `LCD_LED` |
| 9 | SDO (MISO) | **No Connect** (the screen doesn't let go of this line, so it stays off the shared bus) |
| 10 | T_CLK | `SPI0_SCK` |
| 11 | T_CS | `TOUCH_CS` |
| 12 | T_DIN | `SPI0_MOSI` |
| 13 | T_DO | `SPI0_MISO` |
| 14 | T_IRQ | `TOUCH_IRQ` |

Pins 7 and 10 share `SPI0_SCK`, and pins 6 and 12 share `SPI0_MOSI`. That's intended: the screen,
touch and SD card take turns on one bus, exactly as on your breadboard.

### S2. SD card socket (J4, 4 pins)

Search **`C2897367`** (HCTL **PM254-1-04-Z-8.5**: 1×4 female header, 2.54 mm, 8.5 mm tall). Name it
**J4**. Check the order printed next to the SD pins on **your** module; on the MSP2807 it is:

| J4 pin | Module pin | Net |
|---|---|---|
| 1 | SD_CS | `SD_CS` |
| 2 | SD_MOSI | `SPI0_MOSI` |
| 3 | SD_MISO | `SPI0_MISO` |
| 4 | SD_SCK | `SPI0_SCK` |

If your module prints them in a different order, follow the module and tell me.

### S3. Play/pause button (SW2)

Search **`C530670`** (Kinghelm **KH-3635-CAJ**: a small side-push button that sits on the board edge;
if it's out of stock, `C502303` works too).

- [ ] One side of the button → `BTN_PLAY_N`
- [ ] Other side → `GND`
- [ ] **100 nF** (0402) from `BTN_PLAY_N` to GND, which smooths out contact bounce
- [ ] If the symbol has 4 pins, they're joined in pairs: wire one pair to each side (check the
      datasheet drawing, or send me a screenshot)

### S4. Check and send

1. **Design → Annotate**, then **Design → Check DRC**. With all four pages drawn, every label now
   has both ends, so the "single pin net" warnings should be gone. Send me a screenshot of any
   that remain.
2. Send the **PDF of all four pages** and the **BOM**. After that review we move to the board
   layout.

---

## Part G — From schematic to board (after all four pages are reviewed)

### G1. Make the PCB

1. **Design → Update/Convert Schematic to PCB**. A dialog lists every part and net. Click
   **Apply Changes**.
2. The PCB opens with every part piled to one side, joined by thin straight lines (the
   **ratsnest**): the connections still to be made.
3. **Check:** the Pico should have ratsnest lines to parts on all the other pages. That confirms
   the pages are linked.

### G2. Board settings

1. **Layers:** **Tools → Layer Manager** → **2 copper layers**, thickness **1.6 mm**.
2. **Design rules:** **Design → Design Rule**: track width 0.25 mm for signals, **0.5 mm** for power
   nets (`VBUS`, `VSYS`, `VBAT`, `3V3`, `GND`), clearance 0.2 mm, vias 0.3 mm drill / 0.6 mm pad.
3. **Units:** mm.

### G3. Draw the outline (with the antenna notch)

The board is 52 × 109 mm with a 9 × 14 mm notch in the right edge, under the Pico's antenna. Draw
it as one shape: select the **Board Outline** layer, **Place → Board Outline** → **polygon**, and
click these corners in order (or type them in the right panel):

(0, 0) → (52, 0) → (52, 4.5) → (42.5, 4.5) → (42.5, 18.5) → (52, 18.5) → (52, 109) → (0, 109) → back to (0, 0)

If EasyEDA's Y axis counts **upward**, use negative Y values instead (0, −4.5 and so on). Either way,
the outline must look like the drawing in the spec, with the notch near the top-right corner.

### G4. Place the fixed parts by typing coordinates

Select each part and type its X / Y and rotation into the right panel:

| Part | Position |
|---|---|
| **Pico 2 W** | centre at **(26.0, 11.5)**, rotated so the **USB end is at the left edge**. Pin 1 ends up bottom-left at (1.87, 20.39). Its antenna end sits over the notch |
| 4 mounting holes (**Place → Hole**, 2.7 mm) | (3.7, 29.9), (48.3, 29.9), (3.7, 106.0), (48.3, 106.0) |
| J3 (14-pin socket) | pin 1 at **(45.0, 25.0)**, the other pins running to the **left** |
| J4 (4-pin SD socket) | pin 1 at **(29.8, 105.9)**, the other pins running to the left |
| J1 USB-C | bottom edge, left, around x 12, with the opening at the edge |
| J5 jack | bottom edge, right, around x 38 |
| SW1, SW2 | left edge, around y 45 and y 60, with the slider and button sticking out past the edge |

Then print `docs/screen_template_1to1.pdf` and lay it against the module to double-check the screen
positions, and look at **View → 3D**.

### G5. Keep-outs

- **Antenna:** no copper on either layer within about 2 mm of the notch: **Place → Keep-out Region
  / Prohibited Region**, all layers.
- **Nothing under the Pico** except its own pads.
- **Under the module's SD holder** (x 33–51, y 55.5–83.8): no part taller than 1 mm.
- **Bottom side:** no parts at all; the battery sticks there.

### G6. Place everything else

Charger with its capacitors right next to its pins, gauge next to the battery connector, DAC next
to the jack. Then send me a screenshot **before routing**. Placement decides most of how well the
board works.

### G7. Route (I'll walk you through this when we get there)

Power tracks first (0.5 mm), then signals. Then fill the bottom layer with a solid GND copper pour
and the top layer with GND around the parts. Run **Design → Check DRC** until it's clean.

---

## Part H — Ordering (after the final review)

1. **Export → PCB Fabrication Files (Gerber)**, **Export → Bill of Materials (BOM)**, and
   **Export → Pick and Place File**. Send all three to me for a final check.
2. **Order → PCB Order** sends the design to JLCPCB. Choose **2 layers, 1.6 mm**, 5 boards, and
   switch on **PCB Assembly → top side**.
3. JLCPCB then shows each part on a picture of the board. **Check every chip's pin 1 dot and
   rotation** before paying. Leave out the Pico and the two screen sockets; you solder those
   yourself.

---

## Getting a review

| When | Send |
|---|---|
| after each schematic page | **schematic PDF** (Export → PDF/Image) + **BOM** |
| before routing | screenshot of the placed board, plus the 3D view |
| before ordering | Gerber zip, BOM, pick-and-place file |

Now and then, also save the project into the repo: **File → Export → EasyEDA Pro project
(.epro)** → put it in `hardware/pod-pcb-easyeda/` on GitHub. The board design is then versioned
alongside the firmware.

## When you're stuck

- A part shows no stock: send me the MPN, and I'll find an equivalent.
- A pin name doesn't match the spec: take a screenshot of the symbol and send it.
- Something won't connect: zoom right in. The wire end is usually a grid square away from the pin.
- EasyEDA's own manual: **prodocs.easyeda.com** (search box at the top).
