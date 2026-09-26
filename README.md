# iPOD — a DIY handheld music player

> A pocket-sized modern iPod built from scratch: Raspberry Pi Pico 2 W, Bluetooth streaming, hi-fi I2S DAC, and a touchscreen that shows album art.

A pocket-sized "modern iPod" built around the Raspberry Pi Pico 2 W. Pod receives audio from a phone over Bluetooth, plays it through a hi-fi I2S DAC, and shows album art on a touchscreen.

**Status:** breadboard prototyping. A custom PCB comes after full testing and optimisation on the breadboard.

## Goals

- **Bluetooth A2DP sink (core requirement):** phone → Pico 2 W → DAC → headphones
- **Now Playing screen** with full-bleed album art fading into an accent colour, and centred track text ("Poster" layout)
- **Later:** microSD playback and a full LVGL touchscreen UI

## Hardware

| Part | Role |
|---|---|
| Raspberry Pi Pico 2 W | Main controller, Bluetooth Classic |
| Adafruit PCM5102 I2S DAC | Audio output |
| 2.8" 240×320 SPI touch display | UI and album art |
| microSD slot (planned) | Local playback |

The Pico 2 W replaced an earlier ESP32-S3 plan because the ESP32-S3 has no Bluetooth Classic, which A2DP needs.

## Wiring

### Display + touch (shared SPI0)

| Display pin | Pico 2 W |
|---|---|
| VCC | 3V3 (pin 36) |
| GND | GND |
| CS | GP17 |
| RESET | GP21 |
| DC | GP16 |
| SDI (MOSI) + T_DIN | GP19 |
| SCK + T_CLK | GP18 |
| LED | GP13 |
| SDO (MISO) | not connected |
| T_CS | GP22 |
| T_DO | GP20 |
| T_IRQ | GP26 |

### PCM5102 DAC (I2S)

| DAC pin | Pico 2 W |
|---|---|
| VIN | 3V3 (pin 36) |
| GND | GND (pin 13) |
| DIN | GP9 |
| BCK | GP10 |
| WSEL (LRCK) | GP11 |
| MCK, DE, FIL, MU, FM | not connected |

### microSD (planned)

SD_CS → GP7; shares GP18 (SCK), GP19 (MOSI) and GP20 (MISO) with the display bus.

## Repository layout

```
firmware/   Pico 2 W source code
hardware/   schematics, wiring photos, later the PCB
docs/       notes and datasheets
PROGRESS.md dated build log
```

## Progress

See [PROGRESS.md](PROGRESS.md).
