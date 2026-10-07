# Pod PCB in EasyEDA — step by step

How to build the Rev A board in EasyEDA, following the spec in `docs/pcb-plan.md`
(every connection and value is written there; this guide is the *how*).

**How we work:** you draw in EasyEDA, one block at a time. After each block, send me the exports
listed in step 6 and I check them against the spec before you move on. Mistakes are cheap to fix in
the schematic and expensive once boards are made.

---

## 1. Set up

1. Use **EasyEDA Pro** (pro.easyeda.com, free). It has multi-sheet schematics and imports KiCad files;
   EasyEDA Standard is older and more limited.
2. New project: **Pod Rev A**.
3. Board settings: **4 layers, 0.8 mm thickness, ENIG**. Design rules: JLCPCB 4-layer defaults
   (0.1 mm min track/space; use 0.15 mm normally, 0.4 mm for power; vias 0.3 mm drill / 0.45 mm pad,
   or 0.2/0.4 where space is tight).
4. Add **5 schematic sheets**: Power, MCU, Radio, Audio, Screen & controls. Connect sheets with
   **net labels / net ports** using the exact net names from the spec (`VBUS`, `VSYS`, `VBAT`, `3V3`,
   `GND`, `SPI0_SCK`, `I2S_BCK`, ...). Same name = same wire, on any sheet.

## 2. Find parts (EasyEDA's library is the JLCPCB/LCSC library)

Open the library panel, search by the **part number (MPN)** below, check the package matches, and
place it. Each part then has its LCSC number attached automatically, which is what JLCPCB uses to
assemble. Prefer results marked **Basic** (lower assembly fee) for resistors and capacitors.

| Search for | What it is | Check the package |
|---|---|---|
| RP2350A | microcontroller | QFN-60 |
| W25Q128JVSIQ | 16 MB flash | SOIC-8 |
| APS6404L-3SQR | 8 MB PSRAM | SOIC-8 |
| 12MHz crystal 3225 (e.g. ABM8-272-T3, or a JLC basic 12 MHz 3225) | crystal | 3.2 × 2.5 mm |
| BQ24074RGTR | charger with power path | VQFN-16 |
| MAX17048G+T10 | fuel gauge | TDFN-8 |
| TPS63802DLAR | 3.3 V buck-boost | VSON-10 |
| XFL4015-471MEC (or any 0.47 µH, ≥3 A, 4×4 mm) | inductor for TPS63802 | 4 × 4 mm |
| PCM5102APWR | audio DAC | TSSOP-20 |
| USBLC6-2SC6 | USB ESD protection | SOT-23-6 |
| TYPE-C-31-M-12 (HRO) | USB-C socket, 16-pin | SMD |
| S2B-PH-SM4-TB | battery connector JST-PH 2-pin | SMD |
| PJ-320A or SJ-43514 | 3.5 mm jack with detect | check it has a switch pin |
| MSK-12C02 | power/hold slide switch | SMD side |
| TS-1187A / SKRT side-push tactile | play/pause button | SMD side-push |
| 1x14 female header 2.54 mm, and 1x4 | sockets for the screen module | through-hole |
| Raspberry Pi RM2 | Bluetooth radio | 21-pin castellated module |

If a part isn't in the library (the **RM2** may not be), make it with EasyEDA's symbol and
footprint editors from the RM2 datasheet (21 castellated pads, 16.5 × 14.5 mm), or import
Raspberry Pi's KiCad RM2 footprint (File → Import → KiCad). JLCPCB can't assemble the RM2 unless
they stock it; if not, you solder it by hand (castellated pads are easy to hand-solder).

## 3. Draw the schematic, one sheet at a time

Follow each sheet in `docs/pcb-plan.md` **line by line**. Tick each line off as you draw it.

| Order | Sheet | Tip |
|---|---|---|
| 1 | **Power** | Start here: USB-C → ESD → BQ24074 → switch → TPS63802 → 3V3; fuel gauge on VBAT. Double-check the CC 5.1 kΩ resistors and the FB divider (510 k / 91 k). |
| 2 | **MCU** | Copy Raspberry Pi's *RP2350 minimal design* exactly (from *Hardware design with RP2350*): power pins, decoupling, core regulator inductor, crystal, flash, BOOTSEL, RUN. Then add PSRAM (CS → GP8) and the GPIO net labels from the GPIO map. |
| 3 | **Radio** | RM2 with the 470 Ω and 10 kΩ resistors to GP24, decoupling at Vin and VDDIO. |
| 4 | **Audio** | PCM5102A exactly as listed; XSMT to GP12 with 10 kΩ to GND. |
| 5 | **Screen & controls** | The 14-pin and 4-pin sockets with the pin-to-net table; play/pause button; 4 M2.5 mounting holes. |

Run **Design → Check DRC/ERC** on the schematic after each sheet.

## 4. Lay out the board

1. **Outline:** 50 × 94 mm with 3 mm corner radius. The top 8 mm is the antenna tab; the screen module
   area is the 50 × 86 mm below it.
2. **Fixed positions first** (from `docs/pcb-plan.md` → Layout, measured from your module; on our board
   they're mirrored, because the module faces down onto it):
   - M2.5 holes, J3 (14-pin, **pin 1 VCC on the right**), J4 (4-pin SD, centred at the bottom).
   - USB-C bottom-left, jack bottom-right, SW1 and SW2 on the left edge, RM2 on the antenna tab.
   - **Keep-out:** no copper under the RM2 antenna (all layers), nothing tall under the module's
     SD holder (right side, 32–61 mm below the tab).
3. **Then the rest:** RP2350 with flash, PSRAM and crystal tight around it; charger, regulator and
   inductor together; DAC near the jack, away from the regulator.
4. **Layers:** L1 parts and signals, L2 solid GND (no tracks), L3 power, L4 signals.
   **No parts on the bottom**: the battery sticks there.
5. **Route** USB D+/D− as a 90 Ω differential pair (EasyEDA's differential pair tool), power with
   wide tracks, then everything else. Copy the datasheet layouts for TPS63802 and the RP2350
   core regulator. Pour GND on L1 and L4 and stitch with vias, except under the antenna.
6. Run board DRC until clean, check the 3D view (the screen module should line up with the
   sockets and holes).

## 5. Order

In EasyEDA Pro: **Fabrication → PCB fabrication file (Gerber)**, then order at JLCPCB with
**PCB Assembly (top side)**, 5 boards. Check the BOM/placement preview on JLCPCB's site (every
part's rotation and position) before paying. Parts JLCPCB doesn't stock (possibly the RM2): leave
them out of assembly and solder them yourself.

## 6. Getting a review (after every sheet, and before ordering)

Send me any of these:
- **Schematic PDF** (File → Export → PDF) — easiest; I check every connection against the spec.
- **Netlist** (File → Export → Netlist) — lets me check connections automatically.
- **BOM** (Export → BOM) — I check parts, packages and values.
- **Before ordering:** the Gerber zip and the board's 3D view / screenshots of each layer.

Also save the EasyEDA project file (File → Export → EasyEDA Pro project, `.epro`) into the repo at
`hardware/pod-pcb-easyeda/` now and then, so the design is versioned on GitHub with the firmware.
