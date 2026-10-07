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

### B3. Make the 5 pages

1. In the left panel, right-click the schematic → **New Schematic** / **New Page** (EasyEDA calls
   them "schematic pages"). Make 5 pages in total.
2. Right-click each page → **Rename**: `1 Power`, `2 MCU`, `3 Radio`, `4 Audio`, `5 Screen`.
3. All five pages belong to **one** schematic, so a net named `3V3` on page 1 is the same net as
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

Several chips (BQ24074, TPS63802, MAX17048) have a metal pad underneath, shown as an extra pin
named **EP**, **PAD**, **PowerPAD** or **thermal pad**. Connect it to **GND** unless the spec says
otherwise.

---

## Part D — Drawing Sheet 1: Power, step by step

Open page **1 Power**. Draw it in five groups from left to right, as the power flows:
**USB-C → protection → charger → switch and regulator → fuel gauge.** Leave space between the
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
- [ ] **D+** (A6 *and* B6) → label `USB_DP_C`
- [ ] **D−** (A7 *and* B7) → label `USB_DM_C`
- [ ] **SBU1**, **SBU2** → No Connect flag

*Why the CC resistors:* a USB-C charger outputs 0 V until it sees 5.1 kΩ on CC. Leaving them out
is the most common USB-C mistake.

### D2. USB protection (U1)

Search `USBLC6-2SC6`. It has 6 pins.

- [ ] Both **I/O1** pins (1 and 6) → `USB_DP_C`
- [ ] Both **I/O2** pins (3 and 4) → `USB_DM_C`
- [ ] **VBUS** (pin 5) → `VBUS`
- [ ] **GND** (pin 2) → `GND`

The MCU sheet will later connect `USB_DP_C` / `USB_DM_C` to the RP2350's USB pins through 27 Ω
resistors. That's not part of this sheet.

### D3. Charger (U2)

Search `BQ24074RGTR` (VQFN-16). Put it in the middle of the page with room around it, because
nine of its pins get a part.

- [ ] **IN** → `VBUS`, and C1 **4.7 µF** from IN to GND
- [ ] **OUT** → `VSYS`, and C2 **10 µF** from OUT to GND
- [ ] **BAT** → `VBAT`, and C3 **10 µF** from BAT to GND
- [ ] **ISET** → R4 **1.8 kΩ** → GND (sets about 0.5 A charging)
- [ ] **ILIM** → R5 **1.1 kΩ** → GND (sets the 1.46 A input limit)
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

### D4. Battery connector (J2)

Search `S2B-PH-SM4-TB` (JST-PH, 2 pins, side-entry SMD).

- [ ] Pin 1 → `VBAT`
- [ ] Pin 2 → `GND`

On the page, put a text note next to it: **"check battery polarity"**: **Place → Text**. LiPo
cells come wired either way round. Plugging in a reversed one destroys the charger.

### D5. Power/hold switch (SW1)

Search `MSK-12C02`. It has three main pins: one in the middle (common) and one at each side, plus
mounting pins.

- [ ] **Middle (common)** → label `REG_EN`
- [ ] **One side** (ON) → `VSYS`
- [ ] **Other side** (OFF) → `GND`
- [ ] Mounting/shell pins → GND (or No Connect)

### D6. 3.3 V regulator (U4) and its inductor (L1)

Search `TPS63802DLAR` (VSON-10) and `XFL4015-471MEC`. If that inductor isn't stocked, search
`0.47uH 4x4` and pick one rated **3 A or more**.

- [ ] **VIN** → `VSYS`, and C5 **10 µF** from VIN to GND
- [ ] **EN** → `REG_EN`
- [ ] **L1** pin of the chip → one end of the inductor L1
- [ ] **L2** pin of the chip → the other end of L1
- [ ] **VOUT** → `3V3`, with C6 **22 µF** and C7 **22 µF** from `3V3` to GND
- [ ] R12 **510 kΩ** from **VOUT** to **FB**
- [ ] R13 **91 kΩ** from **FB** to GND (with R12, this sets 3.30 V)
- [ ] **MODE** → GND
- [ ] **PG** → label `REG_PG`, and R14 **100 kΩ** from `REG_PG` to `3V3`
- [ ] **GND**, **AGND** and the exposed pad → GND

**Draw the FB divider with real wires, not labels:** VOUT → R12 → a junction → R13 → GND, with a
wire from the junction to FB. It's easier to read and check.

### D7. Fuel gauge (U3)

Search `MAX17048G+T10` (TDFN-8).

- [ ] **VDD** → `VBAT`, and C4 **1 µF** from VDD to GND
- [ ] **CELL** → `VBAT`
- [ ] **GND**, **CTG**, **QSTRT** and the exposed pad → GND
- [ ] **SDA** → label `I2C_SDA`, with R9 **4.7 kΩ** to `3V3`
- [ ] **SCL** → label `I2C_SCL`, with R10 **4.7 kΩ** to `3V3`
- [ ] **ALRT** → label `GAUGE_ALRT_N`, with R11 **10 kΩ** to `3V3`

Labels like `CHG_N`, `PGOOD_N`, `REG_PG`, `I2C_SDA`, `I2C_SCL` and `GAUGE_ALRT_N` go nowhere on
this page. That's expected: they connect to the RP2350 on page 2.

### D8. Tidy up

1. **Designators:** if numbers are messy or duplicated, run **Design → Annotate** (the wording
   might be "Annotate Designator"). Then compare R1–R14 and C1–C7 with the spec. Matching the
   spec's numbers makes my review faster, but it isn't essential: I check by connection.
2. **Values visible:** every resistor and capacitor should show its value on the page. If one
   doesn't, select it and switch on the value in the right panel.
3. **Text notes:** add a title at the top of the page: **Place → Text** → "Power: USB-C,
   charger, 3.3 V regulator, fuel gauge".

### D9. Check it

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

### D10. Send it to me

1. **Export → PDF/Image** (in some versions under **File → Export**). Choose **PDF**, select
   **all pages** (or only page 1 for now), and use **colour** if offered.
2. Also export the **BOM**: **Export → Bill of Materials (BOM)**, as a CSV or Excel file. This
   lets me check every package and LCSC number.
3. Send both here. I'll reply with a checklist of anything to change.

---

## Part E — The other four sheets (same method)

Use the same steps: place the chip, then go through its spec section line by line, with stubs and
labels. Draw them in this order, sending a PDF after each one:

| Page | What's on it | Beginner tips |
|---|---|---|
| **2 MCU** | RP2350A, flash, PSRAM, crystal, USB, BOOTSEL and RUN buttons | The hardest page, because the RP2350 has 60 pins. Copy Raspberry Pi's **"RP2350 minimal design"** schematic (in their *Hardware design with RP2350* PDF) exactly, then add PSRAM and the GPIO net labels from the spec's GPIO map. Draw one block at a time: power pins and capacitors, then the core regulator, then the crystal, then flash, then USB, then the GPIO labels. You can send me partial PDFs. |
| **3 Radio** | RM2 module | The RM2 is probably **not** in EasyEDA's library, so you'll need to make its symbol and footprint (see Part F). Do this page after page 2. |
| **4 Audio** | PCM5102A DAC, headphone jack | Straightforward: about 12 capacitors and resistors, all listed in the spec. |
| **5 Screen** | the two sockets for the screen module, the play button, mounting holes | Use generic **1×14** and **1×4 female header, 2.54 mm** parts. Pin 1 of each socket must get the net from the spec's pin table. Mounting holes are placed on the PCB in Part G, not here. |

---

## Part F — Making a part that isn't in the library (the RM2)

1. First, search the library for `RM2` and `Raspberry Pi RM2`. Someone may have shared one in the
   **user-contributed** library section. If you find one, send me its pin list before using it.
2. If not, the easiest route is to **import Raspberry Pi's KiCad files** for the RM2 (they publish
   them with the RM2 datasheet): **File → Import → KiCad**, and choose the symbol/footprint
   library files.
3. If importing doesn't work, make it by hand. **File → New → Symbol**: draw a rectangle and add
   21 pins named as in the RM2 datasheet. Then **File → New → Footprint**: 21 castellated pads at
   the datasheet's positions on a 16.5 × 14.5 mm outline. This is fiddly. Send me the datasheet
   page and I'll write you the exact pad coordinates.
4. JLCPCB likely won't stock the RM2, so you'll solder it by hand after the boards arrive.
   Castellated pads are among the easiest parts to hand-solder.

---

## Part G — From schematic to board (after all five pages are reviewed)

Do this only after I've checked all five pages. Changes are still possible later, but much easier
before layout starts.

### G1. Make the PCB

1. **Design → Update/Convert Schematic to PCB**. A dialog lists every part and net. Click
   **Apply Changes**.
2. The PCB opens with every part piled to one side, joined by thin straight lines (the
   **ratsnest**). The lines show which pads must be connected, not tracks yet.
3. **Check:** a part on page 2 with a `3V3` pin should have a ratsnest line to parts on page 1.
   That confirms the pages are linked.

### G2. Board settings

1. **Layers:** **Tools → Layer Manager** → **4 copper layers**. Then set the stack-up to JLCPCB's
   standard 4-layer stack with **0.8 mm thickness**: inner layer 1 = **GND**, inner layer 2 =
   **power**.
2. **Design rules:** **Design → Design Rule** (or *Rules Manager*). Set the minimum track width and
   spacing to 0.1 mm. Use 0.15 mm for signals, 0.4 mm for power, and vias of 0.3 mm drill / 0.45 mm
   pad.
3. **Units:** set them to **mm** (top toolbar or settings).

### G3. Draw the outline

1. Select the **Board Outline** layer, then **Place → Board Outline** → rectangle. Draw it, then
   type the exact size in the right panel: **50 × 94 mm**, with a **3 mm** corner radius.
2. Put the outline's top-left corner at **(0, 0)**, so all the positions in the spec can be typed
   in directly.

### G4. Place the fixed parts first, by typing coordinates

For each of these, select the part and type its **X / Y** and rotation into the right panel. The
positions are in `docs/pcb-plan.md` → *Layout*:

- the 4 **mounting holes** (**Place → Hole**, 2.7 mm diameter for M2.5 screws),
- **J3** (the 14-pin socket; **pin 1 is on the right**, because the module is mirrored when it
  faces down onto our board) and **J4** (the 4-pin SD socket),
- **J1** USB-C bottom-left, **J5** jack bottom-right, **SW1** and **SW2** on the left edge, the
  **RM2** on the 8 mm antenna tab at the top.

Then check the screen fit: print `docs/screen_template_1to1.pdf` and lay it against the module. In
EasyEDA, **View → 3D** shows the board in 3D.

### G5. Keep-outs

- **No copper under the RM2 antenna, on any layer:** **Place → Keep-out Region / Prohibited
  Region**, covering the antenna end of the tab, applied to all layers.
- **Nothing tall under the screen module's SD holder** (right side, 32–61 mm below the tab).
- **No parts on the bottom side**, because the battery sticks there.

### G6. Place everything else

Move each group next to the chip it belongs to, in this order: the RP2350 with its flash, PSRAM,
crystal and decoupling capacitors (each capacitor next to its pin), then the charger, then the
regulator with its inductor and capacitors tight together, then the DAC near the jack and away from
the regulator. Then send me a screenshot of the layout **before routing**. Placement decides most
of how well the board works.

### G7. Route (I'll walk you through this when we get there)

The USB pair first (**Route → Differential Pair**, 90 Ω), then wide power tracks, then the rest.
Finally, pour GND copper on the top and bottom layers. Run **Design → Check DRC** until it's clean.

---

## Part H — Ordering (after the final review)

1. **Export → PCB Fabrication Files (Gerber)**, **Export → Bill of Materials (BOM)**, and
   **Export → Pick and Place File**. Send all three to me for a final check.
2. **Order → PCB Order** sends the design to JLCPCB directly. On JLCPCB's page, choose
   **4 layers, 0.8 mm, ENIG**, 5 boards, and switch on **PCB Assembly → top side**.
3. JLCPCB then shows each part on a picture of the board. **Check every chip's pin 1 dot and
   rotation** before paying. Parts it can't assemble (probably the RM2) are left off. You'll
   solder those yourself.

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
