# Pod — build log

Newest entries at the top. Each entry: what was done, what broke, what's next.

## 2026-10-08 (later) — Switched to a perfboard build

**Done**
- JLCPCB quote for the assembled PCB came to about $90, too much for one prototype; the PCB design stays in the repo for later
- Planned a perfboard version with the same layout and the same Pico pins (`docs/perfboard_build.pdf`): Pico 2 W in sockets at the top, screen on sockets below it, TP4056 USB-C charger and the Adafruit PCM5102 DAC at the bottom edge, 1000 mAh LiPo on the back
- Power: TP4056 OUT+ → slide switch → 1N5819 → VSYS, so the Pico's USB can stay plugged in for flashing; 100k/100k divider on GP28 for the battery level

**Next**
- Buy parts, build, test screen/touch/SD on USB power first, then audio, then battery
- Firmware: read battery voltage on GP28 when no MAX17048 is fitted

## 2026-10-08 — PCB routed and exported

**Done**
- Switched the board to carry the Pico 2 W itself (soldered on through its headers) instead of a bare RP2350; dropped the TPS63802 regulator and USB ESD chip (USB-C is charge-only)
- All four schematic sheets drawn and reviewed: Power, Pico, Audio (PCM5102A + PJ-342 jack with headphone detect), Screen (14-pin and 4-pin sockets, play button)
- PCB 52 × 109 mm, 2 layers: placed, autorouted with hand-drawn power stubs between the Pico pins, GND pours on both layers, antenna notch and keep-out. DRC clean apart from the expected notch item
- Exported Gerbers, BOM (23 lines, all LCSC) and pick-and-place; checked drill sizes, outline, copper and positions

**Fixed in review**
- Many routing problems: power nets too wide to pass between header pins (power width 0.4 mm plus 0.3 mm stubs), GND islands, duplicate vias, stitching vias with no copper, pours not rebuilt
- Gerber outline had a stray 6.35 mm line from the jack footprint, 2.2 mm inside the bottom edge, which the fab would have milled as a slot; removed from the outline file

**Next**
- Paper 1:1 fit test with the Pico and screen module, then order from JLCPCB (top-side assembly; hand-solder the Pico and screen sockets)
- Firmware: headphone detect, charge/power-good status, play button

## 2026-10-07 (evening) — Power sheet drawn in EasyEDA

**Done**
- Moved PCB design to EasyEDA Pro (online); wrote a beginner guide with click-by-click checklists (`docs/easyeda-guide.md`)
- Sheet 1 (Power) drawn and reviewed: USB-C + ESD, BQ24074 charger, battery connector, power switch, TPS63802 3.3 V regulator, MAX17048 gauge
- Resistor values changed to standard JLCPCB Basic parts: ISET 1.8 kΩ (0.49 A charging), ILIM 1.2 kΩ (1.34 A input limit), regulator feedback 56 kΩ / 10 kΩ (still 3.30 V)

**Fixed in review**
- Charger IN, the second OUT pin and the second BAT pin were unconnected; the two 22 µF output capacitors were in series; the gauge's 1 µF capacitor was missing; regulator PG label added

**Changed**
- Switched the board to Ash's own **Pico 2 W**, soldered on, instead of a bare RP2350A + RM2 radio: no chip, flash, PSRAM, crystal, radio or 3.3 V regulator to design, and 2 layers instead of 4
- The Pico sits in a 23 mm strip above the screen module with a 9 × 14 mm cut-out under its antenna (Raspberry Pi's carrier-board rule); board 52 × 109 mm, Pod about 57 × 115 × 24 mm
- Power: charger output feeds the Pico's VSYS; the power switch grounds the Pico's 3V3_EN; the Pico's 3V3 pin powers the screen, DAC and gauge. USB-C charges, the Pico's micro-USB programs
- Same GPIOs as the breadboard, so `pod.uf2` runs unchanged

- All four schematic pages drawn in EasyEDA and reviewed: Power, Pico (as two 1×20 through-hole headers J6/J7, LCSC C50981), Audio (PCM5102A, PJ-342 jack C668606 with detect on its switch contact), Screen (sockets C5307340 / C2897367, side button C530670)

**Next**
- Export the BOM for a part check; convert to PCB and place parts using the layout in `docs/pcb-plan.md`

## 2026-10-07 (later) — Home screen, SD player, battery

**Done (built and tested on a PC, waiting for the SD card + wires to test on hardware)**
- Home screen: choose **Phone** or **SD card**; tapping the top-left corner returns to it
- Home screen redesign: full-screen wallpaper (installed separately as `wallpaper.uf2` via `sdk/tools/make_wallpaper.py`, kept out of the repo), SH monogram + POD wordmark, frosted-glass buttons that blur the picture behind them, garnet/blue accents; generated fallback art when no wallpaper is installed
- SD card player in `sdk/sd_player/`: library browser (folders first, A–Z, drag to scroll), MP3 + WAV, tags and cover art (embedded JPEG or cover.jpg), drag-to-seek, hold-to-scrub, auto-advance
- Track reader tested on a PC with CBR/VBR MP3, MPEG-2 mono, ID3v2.3/2.4 (UTF-8 and UTF-16), WAV 16/24-bit: tags, covers, durations and seek positions all correct
- SPI bus lock so the screen, touch and SD card share SPI0; full-screen updates release the bus every 24 rows
- Battery icon + "Battery low" warning below 15 %, from a MAX17048 fuel gauge (hidden when none is fitted)
- Going home was fiddly (tiny target at the top edge; SD mode needed backing out of every folder): added a house-icon Home button with a large touch area on Now Playing (both modes) and in the library header, a "Going home…" message, and recalibration now needs a 1.5 s hold at power-up so a quick tap during the restart can't trigger it
- Main firmware renamed to `pod.uf2`; core-0 stack raised to 4 KB for the MP3 decoder and FatFs

**PCB Rev A decisions**
- RP2350A chip + Raspberry Pi RM2 radio on our own board; keep the 2.8" resistive screen module, plugged into sockets; 1000 mAh LiPo (503450); play/pause/wake button + power/hold slide switch
- Everything else on the board: USB-C + BQ24074 charger with power path, MAX17048 gauge, TPS63802 3.3 V buck-boost, PCM5102A DAC + jack, 16 MB flash, 8 MB PSRAM
- Board 50 × 94 mm (module footprint + 8 mm antenna tab so the screen doesn't block Bluetooth)
- Screen module measured from a photo (holes, header and SD holder positions); print-out check template in `docs/screen_template_1to1.pdf`
- The module's SD holder sits in the gap under it, so the 1000 mAh battery moved to the underside of our board; Pod about 55 × 100 × 24 mm
- Spec with every connection in `docs/pcb-plan.md`; KiCad project started in `hardware/pod-pcb` (five sheets with checklists, 4-layer board outline, rules)

**Next**
- Print the 1:1 template and check it against the module
- Draw Sheet 1 (Power) in KiCad, push for review
- Wire the SD header (CS GP7, SCK GP18, MOSI GP19, MISO GP20), test SD mode

## 2026-10-07 — First firmware running

**Done**
- Rebuilt the breadboard circuit cleanly against the final pin map (README)
- Set up a GitHub Actions build: every push compiles all sketches in `firmware/` for the Pico 2 W and publishes `.uf2` files to flash by drag-and-drop
- `blink` works: build and flashing pipeline confirmed
- `tone_test` works: steady 440 Hz tone through the PCM5102 DAC, so the I2S wiring (GP9/GP10/GP11) is confirmed
- `bt_sink` works: iPhone pairs with "Pod" and streams Spotify through the DAC to headphones (core requirement met)
- `display_test` works: screen and touch both respond
- Calibrated touch from corner readings: both axes run backwards (dots were landing in the opposite corner)
- Touch needed a hard press: the XPT2046_Touchscreen library has a fixed pressure cutoff of 300. Wrote `PodTouch`, our own driver with an adjustable threshold (set to 60 after testing), extra averaging and the calibration built in
- `now_playing` works: Bluetooth audio, track info on screen and touch controls together (first combined firmware); only album art is missing
- Recovered the full Poster firmware from an earlier design session (Pico SDK, C, with album art over AVRCP Cover Art) and added it as `sdk/`, with its own GitHub Actions build. Touch threshold lowered from 300 to 60 there too
- Pod-side controls in `sdk/`: tap the album art for a volume slider (synced with the iPhone via AVRCP absolute volume), press-and-hold prev/next to rewind/fast-forward (built, awaiting hardware test)

**Fixed**
- Breadboard wiring didn't match the plan: T_DO was on GP21, RESET on GP20, MOSI on GP21 and display MISO connected. Moved to RESET → GP21, T_DO → GP20, MOSI → GP19, MISO disconnected (GP21 isn't a valid SPI0 data pin)
- Local Arduino IDE compiles failed because the laptop's antivirus removed parts of the compiler toolchain; moved compiling to GitHub Actions

**Next**
- Test the `sdk/` Poster firmware on hardware: album art download, colour fade, progress bar, volume, touch controls
- Run the robustness and measurement tests in sdk/README.md before PCB design
- Planned: Bluetooth output to AirPods for SD music

## 2026-09-26 — Repository started

**Done**
- Settled on the Raspberry Pi Pico 2 W (Bluetooth Classic for A2DP), replacing the earlier ESP32-S3 plan
- Display, touch and PCM5102 DAC wired on the breadboard
- Chose the "Poster" Now Playing layout: full-bleed album art fading into an accent colour

**Fixed**
- Display RESET and touch T_DO were swapped: RESET is now GP21 and T_DO is GP20
- Display SDO/MISO left disconnected so it doesn't fight the touch controller on the shared bus

**Next**
- Get Bluetooth A2DP streaming working from the phone to the DAC
- Bring up the display and touch
- Buy a microSD card for the local playback track
