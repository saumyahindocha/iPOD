# Pod — PCB Rev A design spec

This is the locked-in design for the first Pod board, built from the decisions made on 7 Oct 2026.
It is meant to be followed while drawing the schematic in KiCad: one section per schematic sheet,
with every connection written out. The KiCad project lives in `hardware/pod-pcb/`.

## Decisions

| # | Decision | Choice |
|---|---|---|
| 1 | Main chip | **RP2350A chip + Raspberry Pi RM2 radio module**, all on our board |
| 2 | Screen | **Keep the 2.8" 240×320 resistive module** (ILI9341 + XPT2046 + microSD slot) |
| 3 | Screen mounting | **The red module plugs into sockets on our PCB** (no new screen to buy) |
| 4 | Battery | **1000 mAh single-cell LiPo, ~5 mm thick** (503450 size, with protection board) |
| 5 | Controls | **Play/pause + wake button** and an **iPod-style power/hold slide switch** |

Everything else (USB-C charging, power path, fuel gauge, 3.3 V regulator, DAC, headphone jack,
16 MB flash, 8 MB PSRAM) is on our board too.

**Size:** about **55 × 100 × 20 mm**. The board is the module's 86 × 50 mm footprint plus an 8 mm
"antenna tab" at the top (see Layout). The module sits on 8.5 mm sockets; the battery fits in that
gap beside the electronics, which keeps the Pod about 20 mm thick.

## Firmware compatibility

Every pin the breadboard uses stays the same, so `pod.uf2` runs on the board unchanged. New on the
board: PSRAM, charger status pins, the button, headphone detect and a DAC mute pin; the firmware gets
small additions for these after bring-up.

---

## Parts list (Rev A)

Prefer JLCPCB "basic" parts where an equivalent exists (cheaper assembly). Look up each MPN on
jlcpcb.com/parts and put its LCSC number in the part's `LCSC` field in KiCad.

| Ref | Part | MPN (example) | Package | Notes |
|---|---|---|---|---|
| U5 | Microcontroller | RP2350A | QFN-60 7×7 | JLC assembles |
| U6 | 16 MB QSPI flash | W25Q128JVSIQ | SOIC-8 | |
| U7 | 8 MB QSPI PSRAM | APS6404L-3SQR-SN | SOIC-8 | CS on GP8 |
| Y1 | 12 MHz crystal | ABM8-272-T3 | 3.2×2.5 | as in Raspberry Pi's minimal design |
| M1 | Bluetooth radio | Raspberry Pi RM2 | 16.5×14.5 castellated | |
| U2 | Charger + power path | BQ24074RGTR | VQFN-16 3×3 | |
| U3 | Fuel gauge | MAX17048G+T10 | TDFN-8 2×2 | firmware already supports it |
| U4 | 3.3 V buck-boost | TPS63802DLAR | VSON-10 3×2 | |
| L1 | 0.47 µH inductor | XFL4015-471ME | 4×4 | for U4 |
| U8 | Audio DAC | PCM5102APWR | TSSOP-20 | |
| U1 | USB ESD | USBLC6-2SC6 | SOT-23-6 | |
| J1 | USB-C socket | GCT USB4105-GF-A (or HRO TYPE-C-31-M-12) | 16-pin SMD | |
| J2 | Battery connector | JST S2B-PH-SM4-TB | PH 2.0 mm | **check your battery's polarity** |
| J3 | Screen socket | 1×14 female header, 2.54 mm, 8.5 mm tall | THT | module's main pins |
| J4 | SD socket | 1×4 female header, 2.54 mm, 8.5 mm tall | THT | module's SD pins |
| J5 | Headphone jack | CUI SJ-43514-SMT-TR | SMD, with detect switch | |
| SW1 | Power/hold switch | MSK-12C02 (or C&K JS102011JAQN) | SPDT side slide | |
| SW2 | Play/pause/wake | side-push SMD tactile (e.g. Alps SKRTLAE010) | SMD | on the board edge |
| SW3, SW4 | BOOTSEL, RUN | small SMD tactile | SMD | programming/reset |
| FB1 | Ferrite bead | 600 Ω @ 100 MHz | 0603 | DAC analog supply |
| — | Resistors, capacitors | values per sheet below | 0402 (bulk caps 0603/0805) | |
| BT1 | Battery | 1000 mAh LiPo, 503450, with protection PCB and JST-PH lead | | off-board |

---

## Sheet 1 — Power

**Nets:** `VBUS` (USB 5 V), `VSYS` (charger output, 3.6–4.4 V), `VBAT` (cell), `3V3`, `GND`.

**J1 USB-C**
- VBUS pins (A4, A9, B4, B9) → `VBUS`. GND pins (A1, A12, B1, B12) and shell → `GND`.
- CC1 (A5) → R1 **5.1 kΩ** → GND. CC2 (B5) → R2 **5.1 kΩ** → GND. *(Without these, USB-C chargers give 0 V.)*
- D+ (A6 and B6 tied) → `USB_DP_C`. D− (A7 and B7 tied) → `USB_DM_C`. SBU1/SBU2 not connected.

**U1 USBLC6-2SC6 (ESD)** — I/O1 ↔ `USB_DP_C`, I/O2 ↔ `USB_DM_C` (both flow-through), VBUS pin → `VBUS`, GND → `GND`.

**U2 BQ24074 (charger with power path)**
- IN → `VBUS`, with C1 **4.7 µF** to GND.
- OUT → `VSYS`, with C2 **10 µF** to GND. *(Runs the Pod from USB while the cell charges separately.)*
- BAT → `VBAT`, with C3 **10 µF** to GND.
- ISET → R4 **1.78 kΩ** → GND → charge current = 890 / 1780 = **0.5 A** (0.5 C for 1000 mAh).
- ILIM → R5 **1.1 kΩ** → GND → input limit = 1610 / 1100 ≈ **1.46 A**.
- EN2 → `VBUS`, EN1 → GND → "input limit set by ILIM resistor".
- CE → GND (charging always enabled). SYSOFF → GND. TMR → leave open (default safety timers).
- ITERM → R6 **3.0 kΩ** → GND → charge ends at 0.03 × 3000 / 1780 ≈ **50 mA**.
- TS → R3 **10 kΩ** → GND (use the battery's NTC instead if it has a third wire).
- CHG (open-drain, low = charging) → `CHG_N` → GP28, pull-up R7 **100 kΩ** to 3V3.
- PGOOD (open-drain, low = USB present) → `PGOOD_N` → GP27, pull-up R8 **100 kΩ** to 3V3.

**J2 battery** — pin + → `VBAT`, pin − → `GND`. LiPo leads have no standard polarity: check before plugging in.

**U3 MAX17048 (fuel gauge)**
- VDD → `VBAT` with C4 **1 µF** to GND. CELL → `VBAT`. GND → `GND`. CTG → GND. QSTRT → GND.
- SDA → `I2C_SDA` (GP4), SCL → `I2C_SCL` (GP5); pull-ups R9, R10 **4.7 kΩ** to 3V3.
- ALRT (open-drain) → `GAUGE_ALRT_N` → GP2, pull-up R11 **10 kΩ** to 3V3.

**SW1 power/hold switch (SPDT)** — common → `REG_EN`; ON throw → `VSYS`; OFF throw → `GND`.
*(Off = the regulator is disabled: µA drain. The charger still works when off.)*

**U4 TPS63802 (3.3 V buck-boost)**
- VIN → `VSYS`, C5 **10 µF** to GND. EN → `REG_EN`.
- L1 **0.47 µH** between pins L1 and L2.
- VOUT → `3V3`, C6 **22 µF** + C7 **22 µF** to GND.
- FB divider: R12 **510 kΩ** from VOUT to FB, R13 **91 kΩ** from FB to GND → 0.5 × (1 + 510/91) = **3.30 V**.
- MODE → GND (power-save mode for battery life; tie to 3V3 for forced PWM if you ever hear regulator noise in the audio).
- PG → `REG_PG` → GP3 with R14 **100 kΩ** to 3V3 (optional; can be left unconnected).
- AGND and GND → `GND`, joined at the chip.

## Sheet 2 — MCU (RP2350A, flash, PSRAM, USB)

**Copy Raspberry Pi's "RP2350 minimal design" for this sheet exactly** (KiCad files from the
*Hardware design with RP2350* guide): the chip's supply pins and their 100 nF decoupling, the core
regulator (VREG_VIN, VREG_AVDD filter, VREG_LX inductor to DVDD, DVDD capacitors), the 12 MHz crystal
with its load capacitors and series resistor, the flash, BOOTSEL and RUN. This is the proven part.

Changes from the minimal design:
- **U6 flash:** W25Q128JVS (16 MB) on QSPI_SS / SCLK / SD0–SD3.
- **U7 PSRAM:** APS6404L shares QSPI_SCLK and QSPI_SD0–SD3 with the flash; its CE# → **GP8**
  (RP2350 XIP chip-select 1), pull-up R15 **10 kΩ** to 3V3, 100 nF decoupling.
- **USB:** USB_DP → R16 **27 Ω** → `USB_DP_C`; USB_DM → R17 **27 Ω** → `USB_DM_C`.
- **SW3 BOOTSEL:** QSPI_SS → R18 **1 kΩ** → SW3 → GND. **SW4 RUN:** RUN → SW4 → GND.
- **SWD test pads:** SWCLK, SWDIO, 3V3, GND.
- **Test pads** on spare GPIOs: GP0, GP14, GP15.

**GPIO map** (all other GPIOs are in sheets 3–5):

| GPIO | Net | | GPIO | Net |
|---|---|---|---|---|
| 0 | test pad | | 16 | `LCD_DC` |
| 1 | `HP_DET` (jack inserted) | | 17 | `LCD_CS` |
| 2 | `GAUGE_ALRT_N` | | 18 | `SPI0_SCK` (screen, touch, SD) |
| 3 | `REG_PG` | | 19 | `SPI0_MOSI` |
| 4 | `I2C_SDA` | | 20 | `SPI0_MISO` (touch, SD) |
| 5 | `I2C_SCL` | | 21 | `LCD_RST` |
| 6 | `BTN_PLAY_N` | | 22 | `TOUCH_CS` |
| 7 | `SD_CS` | | 23 | `RM2_ON` |
| 8 | `PSRAM_CS` | | 24 | `RM2_DATA` |
| 9 | `I2S_DIN` | | 25 | `RM2_CS` |
| 10 | `I2S_BCK` | | 26 | `TOUCH_IRQ` |
| 11 | `I2S_LRCK` | | 27 | `PGOOD_N` |
| 12 | `DAC_XSMT` (mute) | | 28 | `CHG_N` |
| 13 | `LCD_LED` (backlight PWM) | | 29 | `RM2_CLK` |
| 14, 15 | test pads | | | |

## Sheet 3 — Radio (RM2)

From the RM2 datasheet:
- Pin 16 Vin → `3V3` (3.0–4.8 V allowed; 3.3 V keeps it above the 3.2 V needed for full RF performance).
  Pin 14 VDDIO → `3V3`. Decoupling: C **10 µF** + **100 nF** at pin 16, **100 nF** at pin 14.
- Pins 1, 4, 7, 11, 15, 21 → GND.
- Pin 3 gSPI SCLK → `RM2_CLK` (GP29).
- Pin 5 gSPI data in → `RM2_DATA` (GP24) directly.
- Pin 6 gSPI data out → R19 **470 Ω** → `RM2_DATA` (GP24).
- Pin 10 nIRQ → R20 **10 kΩ** → `RM2_DATA` (GP24).
- Pin 9 gSPI CS → `RM2_CS` (GP25).
- Pins 12 (Wi-Fi on) and 13 (Bluetooth on) → `RM2_ON` (GP23).
- Pins 2, 19, 20 (no connect) and 8, 17, 18 (radio GPIOs) → not connected.
- **Antenna:** place the RM2 on the antenna tab with its antenna end at the board edge. Keep-out
  under and around the antenna on **every layer** (no copper, no vias), per the RM2 footprint drawing.

## Sheet 4 — Audio (PCM5102A + jack)

- Pin 1 CPVDD → `3V3`, 100 nF + 10 µF. Pin 3 CPGND → GND.
- Pins 2 CAPP / 4 CAPM: **2.2 µF** between them. Pin 5 VNEG → **2.2 µF** to GND.
- Pin 8 AVDD → FB1 ferrite from `3V3`, then 10 µF + 100 nF to GND. Pin 9 AGND → GND.
- Pin 20 DVDD → `3V3`, 100 nF + 10 µF. Pin 19 DGND → GND. Pin 18 LDOO → **1 µF** to GND.
- Pin 12 SCK → GND (internal PLL makes the master clock from BCK, as on the breadboard).
- Pin 13 BCK ← `I2S_BCK` (GP10). Pin 14 DIN ← `I2S_DIN` (GP9). Pin 15 LRCK ← `I2S_LRCK` (GP11).
- Pins 10 DEMP, 11 FLT, 16 FMT → GND.
- Pin 17 XSMT ← `DAC_XSMT` (GP12), R21 **10 kΩ** to GND, so the DAC is muted until the firmware unmutes
  it. This removes the pop at power-on and between tracks.
- Pin 6 OUTL → R22 **470 Ω** → jack tip, **2.2 nF** to GND. Pin 7 OUTR → R23 **470 Ω** → jack ring, **2.2 nF** to GND.
- **J5 jack:** sleeve → GND; detect switch → `HP_DET` (GP1, internal pull-up).
- The PCM5102A is a line-level output; it drives earbuds at modest volume. If it's too quiet once
  built, a TPA6132A2 headphone amp can go between the DAC and the jack on Rev B.

## Sheet 5 — Screen sockets and controls

**J3 1×14 socket for the red module** (pin order as printed on the module, left to right):

| J3 pin | Module | Net |
|---|---|---|
| 1 | VCC | `3V3` |
| 2 | GND | `GND` |
| 3 | CS | `LCD_CS` (GP17) |
| 4 | RESET | `LCD_RST` (GP21) |
| 5 | DC | `LCD_DC` (GP16) |
| 6 | SDI (MOSI) | `SPI0_MOSI` (GP19) |
| 7 | SCK | `SPI0_SCK` (GP18) |
| 8 | LED | `LCD_LED` (GP13) |
| 9 | SDO (MISO) | **not connected** (the screen doesn't release the line) |
| 10 | T_CLK | `SPI0_SCK` |
| 11 | T_CS | `TOUCH_CS` (GP22) |
| 12 | T_DIN | `SPI0_MOSI` |
| 13 | T_DO | `SPI0_MISO` (GP20) |
| 14 | T_IRQ | `TOUCH_IRQ` (GP26) |

**J4 1×4 socket for the module's SD pins** — SD_CS → `SD_CS` (GP7), SD_MOSI → `SPI0_MOSI`,
SD_MISO → `SPI0_MISO`, SD_SCK → `SPI0_SCK`. Match the pin order printed next to the module's card slot.

**SW2 play/pause/wake** — one side → `BTN_PLAY_N` (GP6, internal pull-up), other side → GND; 100 nF across it.

**Mounting** — four M3 holes matching the module's corner holes; M3 nylon standoffs, the same height as
the sockets, hold the module firmly so the sockets don't carry the screen's weight.

---

## Layout

```
          50 mm
   ┌──────────────────┐ ─┐
   │  RM2 + antenna   │  │ 8 mm antenna tab: no copper under the antenna,
   │  ░░░keep-out░░░  │  │ nothing above it (it sticks out past the screen module)
   ├──────────────────┤ ─┤
   │  ○            ○  │  │
   │   J3 (14-pin)    │  │ Screen module plugs in here (86 × 50 mm)
   │ ┌──────────────┐ │  │
   │ │   BATTERY    │ │  │ Battery sits in the 8.5 mm gap under the module
   │ │  34 × 50 mm  │ │  │ (top side of our board, held with foam tape)
   │ └──────────────┘ │  │
   │ RP2350 flash     │  │ Electronics on the top side beside the battery,
   │ PSRAM  BQ24074   │  │ all under ~4 mm so they clear the module
   │ TPS63802  DAC    │  │
   │  ○   J4 (SD)  ○  │  │
   └──[USB-C][jack]───┘ ─┘ Bottom edge: USB-C, headphone jack; right edge: SW1, SW2
                         94 mm total
```

- **Why the antenna tab:** the screen module's copper directly above the RM2 would block Bluetooth.
  The tab puts the antenna beyond the module, so range stays like the Pico 2 W's.
- **4 layers, 0.8 mm:** layer 1 signals + parts, layer 2 solid GND, layer 3 3V3/VSYS power,
  layer 4 signals. The bottom side stays flat (no parts), so the Pod sits flush in the case.
- **Keep the area under the module's microSD slot clear** (no battery, no tall parts) so the card
  can slide in from the module's edge.
- **Measure your module before drawing the outline:** outline, the four hole centres, and the
  positions of the 14-pin and 4-pin headers (calipers, or a photo against a ruler). The board must
  match them exactly.

## Routing rules

- USB D+/D−: 90 Ω differential pair, short, equal length, over solid ground.
- RP2350 core regulator, TPS63802 and their inductors: copy the datasheet layouts; keep those
  loops tiny and away from the DAC.
- PCM5102A near the jack; analog ground returns kept away from the regulator's switching node.
- Decoupling capacitor next to every power pin; vias straight down to the ground plane.
- QSPI (flash + PSRAM) short and close to the RP2350.
- Ground via stitching along the board edges; none in the antenna keep-out.
- JLCPCB 4-layer rules: 0.1 mm track/space minimum (use 0.15 mm where there's room), 0.3 mm vias.

## KiCad, step by step

1. Install **KiCad 9**. Open `hardware/pod-pcb/pod-pcb.kicad_pro` from this repo. It already has the
   five sheets above, each with its checklist printed on the sheet, the 50 × 94 mm board outline,
   the module and antenna keep-out marked, and JLCPCB-friendly design rules.
2. Get the RP2350 minimal design KiCad files from Raspberry Pi and copy that circuit into Sheet 2.
   Get the RM2 footprint and symbol from Raspberry Pi as well.
3. For other parts, use KiCad's libraries, or **easyeda2kicad** (Python tool) to pull symbol +
   footprint + 3D model by LCSC number.
4. Draw **one sheet at a time**, push it to GitHub, and I'll review it against this spec before you
   move on. Start with Sheet 1, Power.
5. Run ERC until clean. Assign footprints. Update the PCB from the schematic.
6. Placement first (connectors, sockets, holes, RM2 fixed by the case), then routing.
7. DRC clean, check the 3D view, export Gerbers + drill + BOM + CPL, order **assembled** boards at
   JLCPCB (5 pcs).
8. Export a STEP model of the board for the case in Fusion 360.

## Bring-up plan

1. **No battery yet.** USB from a USB power meter: check `VSYS`, then flip SW1 and check `3V3`.
   Current should be small (tens of mA).
2. Hold BOOTSEL, plug in: the RP2350 drive appears. Flash `pod.uf2` (built for the same pins).
3. Plug in the screen module: home screen. Then touch, DAC, SD, Bluetooth.
4. Battery: check polarity, plug in, watch the charger LED/`CHG_N`, then the percentage on screen.

## Enclosure (after the PCB)

1. Import the board's STEP file, the screen module and battery into Fusion 360.
2. Front bezel holds the module's glass; back shell holds the board; four M2 screws or snap fits.
3. Openings: USB-C and jack (bottom), SW1 and SW2 (side), and a slot at the module's SD end so the
   microSD card can go in and out (the card slot is on the module's underside, at that edge).
4. Plastic over the antenna tab (no metal paint or inserts there). PETG or resin prints first,
   then a nicer final print.
