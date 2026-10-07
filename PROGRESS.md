# Pod — build log

Newest entries at the top. Each entry: what was done, what broke, what's next.

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
- Board 50 × 94 mm (module footprint + 8 mm antenna tab so the screen doesn't block Bluetooth); Pod about 55 × 100 × 20 mm
- Spec with every connection in `docs/pcb-plan.md`; KiCad project started in `hardware/pod-pcb` (five sheets with checklists, 4-layer board outline, rules)

**Next**
- Measure the screen module (outline, holes, header positions) and fix the board outline
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
