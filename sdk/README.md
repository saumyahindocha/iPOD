# Pod bring-up kit: Pico 2 W + PCM5102 + 2.8" ILI9341/XPT2046

Pod's main firmware for **the breadboard wiring below**, built against **Pico SDK 2.3.1** and
**pico-extras sdk-2.3.1** for board `pico2_w`. They compile cleanly, and the
screen renderer was checked pixel-for-pixel on a PC, but they have **not been run
on real hardware yet**. Your breadboard is the first test.

| Firmware (`pod-sdk-firmware` artifact in Actions) | What it tests |
|---|---|
| `pod_display_test.uf2` | Screen + touch only: colours, geometry, touch calibration, paint |
| `pod_dac_test.uf2` | DAC only: quiet 440 Hz tone, LEFT → RIGHT → BOTH → silence |
| `pod.uf2` | **The main firmware.** Home screen → **Phone** (iPhone → Bluetooth → DAC with album art) or **SD card** (MP3/WAV player with library browser). Poster Now Playing screen, volume slider, seeking, battery icon |

---

## 1. Wiring (single source of truth: `common/pod_pins.h`)

Pico 2 W physical pin numbers are in brackets. GND pins: 3, 8, 13, 18, 23, 28, 33, 38.
**Never use GP23, GP24, GP25 or GP29.** The radio uses them.

### Display + touch (both on SPI0)
| Module pin | Pico 2 W | Note |
|---|---|---|
| VCC | 3V3 OUT (36) | If the screen is dim or flickers, move to VBUS (40) |
| GND | GND | |
| CS | GP17 (22) | |
| RESET | **GP21 (27)** | ⚠ changed (was GP20) |
| DC | GP16 (21) | |
| SDI(MOSI) | GP19 (25) | same breadboard row as T_DIN |
| SCK | GP18 (24) | same breadboard row as T_CLK |
| LED | GP13 (17) | backlight (PWM dimming later) |
| SDO(MISO) | **not connected** | ⚠ remove this wire |
| T_CLK | GP18 (24) | shared with SCK |
| T_CS | GP22 (29) | |
| T_DIN | GP19 (25) | shared with SDI |
| T_DO | **GP20 (26)** | ⚠ changed (was GP21) |
| T_IRQ | GP26 (31) | |

**Why the two changes:** on the RP2350, SPI0's data-in (RX) can only be on GP0,
GP4, GP16 or GP20, and GP21 isn't one of them. So T_DO moves to GP20 and RESET
moves to GP21. They're neighbouring pins (26 and 27), so you just swap the two
jumpers. The display's own SDO stays disconnected, because on these modules it
doesn't release the line and would corrupt touch readings on a shared bus.

### PCM5102 DAC (Adafruit)
| DAC pin | Pico 2 W |
|---|---|
| VIN | 3V3 OUT (36) |
| GND | GND (13, next to GP9/GP10) |
| DIN | GP9 (12) |
| BCK | GP10 (14) |
| WSEL | GP11 (15) (must be BCK + 1) |
| MCK, DE, FIL, MU, FM | leave unconnected |

Listening: DAC jack → powered speaker / AUX input (line level). Earbuds work at
low volume but aren't representative.

### SD card slot (on the back of the display module, shares SPI0)
| SD pin (4-pin header next to the card slot) | Pico 2 W |
|---|---|
| SD_CS | GP7 (10) |
| SD_SCK | GP18 (24), same row as SCK / T_CLK |
| SD_MOSI | GP19 (25), same row as SDI / T_DIN |
| SD_MISO | GP20 (26), same row as T_DO |

### Battery fuel gauge (optional now, built into the PCB later)
A MAX17048 breakout (SparkFun / Adafruit) with a 1-cell LiPo: SDA → GP4 (6), SCL → GP5 (7),
3V3, GND, and the cell on its battery connector. Without it, the battery icon stays hidden.

## 2. Before powering up (5 minutes, prevents most problems)
1. **Unplug USB.** Multimeter on resistance/continuity: 3V3 (36) to GND must NOT
   beep (no short). VBUS (40) to GND must NOT beep.
2. Continuity-check every jumper from the module pin to the Pico pin, especially
   the swapped GP20/GP21 pair and the shared SCK/T_CLK and SDI/T_DIN rows.
3. Install a serial monitor: VS Code + Raspberry Pi Pico extension (built-in
   Serial Monitor), PuTTY, or the Arduino IDE Serial Monitor. The Pico appears
   as a COM port once running (any baud setting works over USB).

**Flashing any UF2:** hold **BOOTSEL**, plug in USB, release, then drag the
`.uf2` onto the `RP2350` drive. The board reboots by itself. Open the serial
monitor within ~1 s to catch the first lines.

---

## 3. Test sequence: one part at a time

### Step 1: screen + touch → `pod_display_test.uf2`
Pass criteria:
- Full-screen RED, GREEN, BLUE, WHITE, BLACK in that order (serial names each one).
- A white border with coloured corners: RED top-left, GREEN top-right, BLUE bottom-left, WHITE bottom-right.
- Tap the 4 yellow crosshairs. Serial prints a calibration block.
- Paint mode: dots land under the stylus. The red box clears the screen.

If it fails:
- White screen: SCK/MOSI/CS/DC/RESET wiring.
- Black screen: LED or VCC.
- Red and blue swapped: change `0x48` to `0x40` in `common/ili9341.c`.
- Touch dead: T_CS/T_DO/T_IRQ wiring. Check that the display's SDO is really disconnected.
- Speckles or garbage: lower `POD_TFT_BAUD` to 20 MHz in `pod_pins.h`.

### Step 2: DAC → `pod_dac_test.uf2`
A quiet tone plays: left ear, right ear, both, silence, repeating.
- Left and right swapped: BCK and WSEL are swapped.
- Silence: check DIN/BCK/WSEL and VIN.

### Step 3: Bluetooth + Poster UI → `pod.uf2`, choose **Phone**
1. **First boot runs touch setup:** tap the 4 dots. The calibration is saved to
   flash. To redo it later, **hold a finger on the screen while plugging in USB**.
2. The **home screen** (SH logo, wallpaper, two glass buttons) asks Phone or SD card: tap **Phone**. The idle screen reads **"Waiting for phone"**.
   Tapping **Pod** (top-left) at any time goes back to the home screen.
3. On the iPhone, go to Settings → Bluetooth and tap **"Pod xx:xx:…"**. If you
   paired an earlier Pod build, first open it there and choose **"Forget This
   Device"**, because iOS caches the old feature list.
4. Play something in Spotify. Within about 1–2 s you should see:
   - the album art full-bleed, fading into a colour taken from the cover
   - title, artist, the moving progress bar with elapsed and remaining time
   - serial lines: `Cover Art : connection established`, then `downloading thumbnail`, then
     `album art 200x200 … decoded`, then `frame rendered in N ms`
5. Tap **prev / play-pause / next** at the bottom of the screen. Spotify should
   respond. Serial: `Pod: touch command N sent`.
   **Press and hold** prev / next to rewind / fast-forward (release to stop).
   How far it scrubs depends on the app; Bluetooth has no "jump to time" command.
6. Move the iPhone volume slider. "Vol NN%" updates and the loudness changes.
7. **Tap the album art** to open the Pod's volume slider and drag it. The sound changes
   and the iPhone's volume slider follows. It closes by itself after 3 s.

---

### Home screen wallpaper (optional)
The home screen shows a full-screen picture behind the SH logo and the two glass buttons. Without one,
it shows generated garnet-and-blue art. To install your own (it lives in its own flash area, so
firmware updates don't erase it, and the picture never goes into the repo):

```
python3 tools/make_wallpaper.py my_photo.jpg --focus 0.55 --preview check.png
```
`--focus` picks the vertical crop (0 = top, 1 = bottom); look at `check.png`, then flash
`wallpaper.uf2` like firmware (BOOTSEL, drag). Flash `pod.uf2` again afterwards if needed; the two
never overwrite each other.

### Step 4: SD card player → `pod.uf2`, choose **SD card**
**Card:** FAT32 or exFAT, any size. Copy MP3 (CBR or VBR) or WAV (16/24-bit) files; folders are fine.
For cover art, embed a **baseline JPEG up to 32 KB** (about 300–500 px) in the MP3, or put
`cover.jpg` / `folder.jpg` in the album folder. Progressive JPEGs and PNGs show the placeholder.

1. Tap **SD card** on the home screen. The library lists folders first, then songs, A–Z.
   Drag to scroll, tap a folder to open it, **<** (top-left) to go up; at the top level it goes home.
2. Tap a song: it plays and the Poster screen shows its title, artist and cover.
   Serial: `Pod: SD card ready`, `Pod: playing /…/song.mp3 (MP3 44100 Hz, … ms)`.
3. **Drag the knob on the progress bar** to jump anywhere in the song.
4. **Hold ⏭ / ⏮** to scrub 5 s at a time; tap to skip. ⏮ restarts the song if more than 3 s in.
5. Tap the art for the volume slider. **< Library** (top-left) goes back to the list;
   the ▶ in the list header returns to the song.
6. At the end of a song the next one in the folder plays; at the end of the folder it stops.

Troubleshooting: "No SD card" → check SD_CS (GP7) and the three shared wires; "Card isn't FAT32 /
exFAT" → reformat; crackles while the screen redraws → report it (the display releases the bus every
24 rows, so this shouldn't happen).

### Battery icon and low-battery warning
With the gauge fitted, the status bar shows the percentage and a battery icon (green while
charging). Below **15 %** a "Battery low" message pops up once; it re-arms after charging.

## 4. Robustness tests (do all of these before the PCB)
| Test | Expected |
|---|---|
| 30-minute continuous play | No dropouts, clicks or resets |
| Skip tracks 10× quickly | Art and text catch up to the final track |
| Walk out of range (~10 m, a wall), come back | Audio stops, then reconnects |
| iPhone Bluetooth off/on | Pod returns to "Waiting…", then reconnects |
| Unplug and replug Pod | iPhone reconnects without re-pairing (keys are in flash) |
| Phone call during playback | Music pauses; call audio stays on the iPhone (call audio over Bluetooth isn't supported, by design) |
| Track with no artwork / podcast | Placeholder note is shown, no crash |
| Hindi/Devanagari title | Shows `?`. Expected for now (font has Latin only) |

## 5. Measure & tune (numbers to bring to the PCB design)
- **Screen SPI speed:** raise `POD_TFT_BAUD` from 30 MHz to 40 MHz, then 62.5 MHz. Keep the highest
  speed with zero glitches. Short PCB traces will allow the top speed.
- **Render time:** from `frame rendered in N ms` on serial. Target < 150 ms.
- **Audio glitches during track changes** (when art is drawn): there should be none,
  because drawing runs on the second core. Report it if you hear any.
- **Current draw:** use a USB power meter, or a multimeter in series with 5 V. Measure
  idle, streaming, and streaming with the backlight off. These numbers size the
  battery and regulator.
- **Bluetooth range:** in open space and through one wall. It depends on antenna
  placement, which matters for the PCB/enclosure.

Write these numbers down; they are the inputs to the PCB design phase.

---

## What's in the firmware

**SD mode** (`sd_player/`): FatFs (read-only, FAT32 + exFAT) over a small SPI SD driver; minimp3 for MP3,
WAV 16/24-bit; ID3v2.2–2.4 + ID3v1 + WAV INFO tags; Xing/VBRI headers for exact duration and VBR
seeking. `tools/test_track_meta.c` tests this on a PC against real files; `tools/preview/` renders the
screens on a PC.

**Phone mode** (vs. upstream `a2dp_sink_demo`)
All changes are marked `Pod patch`.
1. **Software volume.** Upstream ignores volume, which makes the iPhone slider do nothing at full scale.
2. **Sample-rate change handling** (44.1/48 kHz) so audio never plays at the wrong pitch.
3. **Album art over Bluetooth** (AVRCP Cover Art), downloaded to RAM automatically on every track change.
4. **Option C Poster UI on core 1:** 240×240 art, smoothstep fade into an accent
   colour, dithered gradients, anti-aliased Inter text, live progress, touch
   controls. Core 0 keeps Bluetooth and audio. Touch calibration is stored in flash.
5. Device name "Pod", logs over USB serial.

## Rebuilding

**On GitHub (normal way):** every push that changes `sdk/` runs the **Build SDK firmware**
action. Download the `pod-sdk-firmware` artifact from the run and drag the `.uf2` onto the Pico.

**Locally:**
Install the **Raspberry Pi Pico** VS Code extension (SDK 2.3.1 + toolchain), then:
```
git clone -b sdk-2.3.1 https://github.com/raspberrypi/pico-extras.git
cmake -B build -G Ninja -DPICO_SDK_PATH=... -DPICO_EXTRAS_PATH=... -DCMAKE_BUILD_TYPE=Release
ninja -C build
```
Fonts are pre-generated. To change sizes or weights, edit `tools/gen_fonts.py` and run
`python3 tools/gen_fonts.py` (needs Pillow).

## Credits / licences
Inter font: SIL Open Font License (`third_party/inter/LICENSE.txt`). TJpgDec
by ChaN (`third_party/tjpgd/LICENSE.txt`). BTstack via the Pico SDK (Raspberry
Pi's licence covers RP2xxx use). 8x8 font in the test app: public domain.
