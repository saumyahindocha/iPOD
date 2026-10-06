# Pod — build log

Newest entries at the top. Each entry: what was done, what broke, what's next.

## 2026-10-07 — First firmware running

**Done**
- Rebuilt the breadboard circuit cleanly against the final pin map (README)
- Set up a GitHub Actions build: every push compiles all sketches in `firmware/` for the Pico 2 W and publishes `.uf2` files to flash by drag-and-drop
- `blink` works: build and flashing pipeline confirmed
- `tone_test` works: steady 440 Hz tone through the PCM5102 DAC, so the I2S wiring (GP9/GP10/GP11) is confirmed
- Added `bt_sink`: Bluetooth A2DP receiver that shows up as "Pod"

**Fixed**
- Breadboard wiring didn't match the plan: T_DO was on GP21, RESET on GP20, MOSI on GP21 and display MISO connected. Moved to RESET → GP21, T_DO → GP20, MOSI → GP19, MISO disconnected (GP21 isn't a valid SPI0 data pin)
- Local Arduino IDE compiles failed because the laptop's antivirus removed parts of the compiler toolchain; moved compiling to GitHub Actions

**Next**
- Test `bt_sink` with the iPhone: pairing, audio quality, pause/resume
- Bring up the display and touch

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
