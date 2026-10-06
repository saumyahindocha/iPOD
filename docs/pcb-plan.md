# Pod — PCB, battery and enclosure plan

Goal: turn the breadboard into a **pocket device about the size of an iPod nano**, with a
rechargeable battery, USB-C charging, a battery percentage on screen, and a 3D-printed case.

Target envelope (with a 2.0" screen): **about 42 × 60 × 11 mm**, 8–12 hours of playback.

---

## 1. Decisions to make first

### 1a. Main chip: Pico 2 W module, or the bare chip?

| | **A. Pico 2 W soldered onto our board** | **B. RP2350 chip + RM2 radio module (recommended)** |
|---|---|---|
| Size of the "brain" | 51 × 21 mm (the whole Pico) | about 22 × 25 mm |
| Difficulty | Easiest: no RF or chip-level design | Moderate: copy Raspberry Pi's official minimal RP2350 design |
| Bluetooth | Built in, certified antenna | RM2 is Raspberry Pi's own certified CYW43439 module (same radio as the Pico 2 W) |
| Firmware changes | None | None (same chip, same radio pins GP23/24/25/29) |
| Assembly | Hand-solderable | JLCPCB assembles the fine-pitch parts |

The Pico module alone is wider than a 2.0" screen, so **B is the way to hit "really compact"**.
The firmware we have runs unchanged on B. If you'd rather de-risk the first board, do one
quick Rev A with the Pico 2 W, then Rev B with the bare chip.

### 1b. Screen: keep the 2.8" module or move to a bare 2.0" panel?

The 2.8" module (86 × 50 mm) is the single biggest part, and its resistive touch is what needs a firm press.
**Recommended: a bare 2.0" (or 2.4") 240 × 320 IPS panel with an FPC ribbon, ST7789 controller and
capacitive touch (CST816-type, I2C).** Same resolution, so the whole Poster UI stays pixel-identical;
the firmware changes are a small ST7789 init and an I2C touch driver. Capacitive touch fixes the
sensitivity issue for good, and IPS looks far better at angles.

### 1c. Memory: add PSRAM

The firmware already uses ~95 % of the RP2350's RAM. The RP2350 supports an external **8 MB QSPI
PSRAM** (APS6404L, SOIC-8/USON-8) on its second chip-select for ~$1. That gives room for larger
cover art, a cached music library, Unicode (Hindi) fonts and smoother scrolling. **Put it on the board.**
Also use a **16 MB flash** (W25Q128) instead of 2–4 MB.

---

## 2. Battery, USB-C charging and battery percentage — what's needed

| Function | Part (suggested) | Why |
|---|---|---|
| USB-C socket | 16-pin USB-C receptacle (e.g. GCT USB4105, HRO TYPE-C-31-M-12) | Charging + USB data (firmware updates, future "USB drive" mode) |
| USB-C "give me 5 V" | 2 × 5.1 kΩ resistors, CC1 and CC2 to GND | Without these, USB-C chargers supply nothing |
| USB protection | USBLC6-2SC6 (data) + TVS on VBUS | Survives static and cheap chargers |
| Charger with power path | **TI BQ24074** (standalone, 1.5 A max) | Plays music while charging, charges the cell separately; CHG and PGOOD pins tell the firmware "charging" / "plugged in" |
| Battery | 1-cell 3.7 V LiPo pouch **with protection circuit**, e.g. 403048 (≈500 mAh) or 503450 (≈1000 mAh) | Protection board prevents over-discharge/short |
| Battery percentage | **MAX17048** fuel gauge (2 × 2 mm, I2C) | Accurate % without a sense resistor. **The firmware already supports it** (SDA GP4, SCL GP5) |
| 3.3 V supply | **TI TPS63802** buck-boost | Uses the full battery range (3.0–4.2 V) efficiently; an LDO would cut out early |
| On/off | Small slide switch on the regulator's EN pin (an iPod-style "hold" switch) | Off really means off (µA drain) |

**Charge current:** set BQ24074 to ~0.5 C (250 mA for 500 mAh, 500 mA for 1000 mAh). Charging time ≈ 2 hours.

**Battery life estimate** (to be confirmed with the measurements in `sdk/README.md` §5):

| Mode | Estimated draw | 500 mAh | 1000 mAh |
|---|---|---|---|
| Phone (Bluetooth in, screen on) | 90–110 mA | ~5 h | ~10 h |
| SD card, screen on | 60–80 mA | ~7 h | ~13 h |
| SD card, screen off (auto-off) | 35–45 mA | ~12 h | ~24 h |

**Low-battery behaviour (firmware done):** icon in the status bar (green while charging), "Battery low"
pop-up once below 15 %. To add on the PCB: hook the BQ24074 CHG pin to a GPIO (`POD_CHG_STAT_PIN`) for
exact charging status, and shut down cleanly at ~3 %.

### Try it on the breadboard now (optional, ~₹1,500)

1. **MAX17048 breakout** (SparkFun "LiPo Fuel Gauge" or Adafruit #5580): SDA → GP4, SCL → GP5, 3V3, GND.
2. **A small protected LiPo** (500–1000 mAh) plugged into the gauge's battery connector.
3. **USB-C LiPo charger board** (e.g. Adafruit #4410, or a TP4056 Type-C module *with* protection).
4. Charger output → Pico **VSYS (pin 39) through a Schottky diode** (e.g. 1N5817). The Pico already has a
   diode from VBUS, so USB and battery can both be connected safely.

The battery icon and the 15 % warning then work immediately with the current firmware.

---

## 3. Block diagram

```
USB-C ──┬── ESD ── D+/D- ─────────────────────────────┐
        └── VBUS ── BQ24074 charger ── SYS ── switch ── TPS63802 ── 3.3 V ──┐
                        │   │                                            │
                     LiPo cell ── MAX17048 gauge (I2C)                   │
                                                                         ▼
  ┌──────────── RP2350A + 16 MB flash + 8 MB PSRAM + 12 MHz crystal ─────────────┐
  │  SPI0 → ST7789 2.0" IPS (FPC)      I2C0 → touch (CST816) + fuel gauge        │
  │  SPI1 → microSD (own bus!)         I2S (PIO) → PCM5102A → 3.5 mm jack        │
  │  GPIO → 3 side buttons, charge status, headphone detect, backlight PWM       │
  │  GP23/24/25/29 → RM2 Bluetooth module (antenna at the board edge)            │
  └───────────────────────────────────────────────────────────────────────────────┘
```

**Audio output:** the PCM5102A is a line-level DAC (made for ≥1 kΩ loads). It drives earbuds at modest
volume; if your breadboard listening test says it's too quiet or harsh, add a tiny headphone amplifier
(TI TPA6132A2, ground-centred, 3 × 3 mm) between the DAC and the jack.

Moving the SD card to its **own SPI bus** (SPI1) removes all sharing with the screen. It's a one-line
change in `pod_pins.h` / `sd_card.c`.

### Proposed pin map (keeps today's firmware working)

| GPIO | Function | | GPIO | Function |
|---|---|---|---|---|
| 2 | Button: volume − | | 16 | Display DC |
| 3 | Button: volume + | | 17 | Display CS |
| 4 | I2C0 SDA (touch + gauge) | | 18 | SPI0 SCK (display) |
| 5 | I2C0 SCL | | 19 | SPI0 MOSI (display) |
| 6 | Button: play/pause / wake | | 20 | (free; was touch MISO) |
| 7 | SD CS | | 21 | Display RESET |
| 8 | Headphone detect | | 22 | Touch RESET |
| 9 | I2S DIN → PCM5102A | | 26 | Touch INT |
| 10 | I2S BCK | | 27 | Charger PGOOD (USB present) |
| 11 | I2S LRCK | | 28 | Charger CHG (charging) |
| 12 | SPI1 MISO (SD) | | 23, 24, 25, 29 | RM2 radio (fixed) |
| 13 | Backlight PWM | | 0 | PSRAM chip-select (XIP_CS1, only GP0/8/19 can do this) |
| 14 | SPI1 SCK (SD) | | | |
| 15 | SPI1 MOSI (SD) | | | |

---

## 4. Board shape and stack-up

- **4-layer, 0.8 mm thick** (cheap at JLCPCB now). Layers: signals / solid GND / 3.3 V + power / signals.
- **Stacked layout**, like a phone: screen on top → PCB under it → battery under the PCB.
  - Screen ~2.5 mm + PCB 0.8 mm + parts 1.5 mm + battery 4–5 mm + two 1 mm walls ≈ **10–11 mm thick**.
- Board outline ≈ **38 × 56 mm**: the screen's outline, with USB-C, the 3.5 mm jack and microSD on the
  bottom edge (like an iPod), buttons on the right edge, and the **RM2 antenna at the top edge** with a
  copper keep-out underneath (no ground pour, no screws, no metal nearby).

---

## 5. KiCad, step by step

1. **Set up.** Install KiCad 9. Create the project in this repo under `hardware/pod-pcb/` so it's versioned
   with everything else (commit often; the build log gets a PCB section).
2. **Start from Raspberry Pi's files.** Download the *RP2350 minimal design* KiCad example and the RM2
   footprint/symbol from Raspberry Pi. Copy the RP2350 core (chip, flash, crystal, internal-regulator
   inductor, decoupling, USB, BOOTSEL/RUN) into your schematic exactly. This is the "known good" part.
3. **Parts library.** Prefer JLCPCB "basic" parts (cheaper assembly). Install the *easyeda2kicad* tool to pull
   symbols, footprints and 3D models by LCSC part number, so the BOM matches what JLCPCB stocks.
4. **Schematic in sheets** (one per block, each reviewed on its own): Power · MCU · Radio · Audio ·
   Display & touch · Storage · Buttons. Put the LCSC number in a field on every part.
5. **Electrical rules check (ERC)** until it's clean. Then I review the schematic with you before layout:
   most board re-spins come from schematic mistakes, not routing.
6. **Outline first.** Draw the board outline and fix the positions of the screen connector, USB-C, jack,
   microSD, buttons and antenna; these are dictated by the case.
7. **Placement:** RP2350 core tight together; regulator inductor and its capacitors close to the chip
   (copy the datasheet layout); PCM5102A near the jack, away from the regulator.
8. **Routing rules:** USB D+/D- as a 90 Ω differential pair, short and matched; solid ground plane on
   layer 2 under everything; short I2S lines; decoupling capacitor next to every power pin; via-stitch
   ground around the board edge; nothing under the antenna.
9. **DRC with JLCPCB's rules**, check the 3D view, then export Gerbers, drill files, BOM and
   placement (CPL) files. Order **assembled** boards (JLCPCB PCBA), qty 5.
10. **Export a STEP model** of the assembled board for the case design.

### Bring-up plan for the first boards
1. Before the battery: power over USB from a current-limited supply/meter. Check 3.3 V, check current.
2. Hold BOOTSEL and plug in: the RP2350 drive must appear. Flash `blink`-equivalent, then the full firmware.
3. Screen → touch → DAC → SD → Bluetooth → gauge, one at a time (the same order as on the breadboard).
4. Then the battery: charge, discharge, check the percentage tracks the real level.

---

## 6. Enclosure (after the PCB)

1. In **Fusion 360**, import the board's STEP file, plus the screen and battery models.
2. Two-part shell: front bezel holding the screen (glued or clipped), back shell holding battery and board,
   joined with 4 small screws (M1.6) or snap fits.
3. Cut-outs on the bottom for USB-C, jack and microSD; flexible "living hinge" or separate key caps for the
   side buttons; a plastic (not metal) area over the antenna.
4. Print in PETG or resin; iterate fit on cheap prints before a final nicer one (or SLA/MJF from JLC3DP).

---

## 7. Order of work

1. Finish breadboard tests (SD mode, battery breakout optional) and record the measurements in
   `sdk/README.md` §5: current draw, SPI speed, Bluetooth range.
2. Decide 1a (A or B) and 1b (screen). Order a 2.0" capacitive panel early and get it working on the
   breadboard; the firmware port is small.
3. Rough case envelope in Fusion → board outline → KiCad schematic → review → layout → order.
4. Bring-up → case → final assembly.
