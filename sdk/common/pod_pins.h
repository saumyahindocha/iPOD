// pod_pins.h - single source of truth for the breadboard wiring (Pod breadboard).
// Change a pin here, rebuild, and every firmware follows.
//
// Pico 2 W GPIO 23, 24, 25 and 29 are used internally by the CYW43439
// radio - never use them.
#pragma once

// ---- PCM5102 I2S DAC (PIO0, pico-extras audio_i2s) --------------------
// BCK must be exactly one GPIO below LRCK (PIO side-set pins are consecutive).
#define POD_I2S_DIN_PIN        9   // Pico pin 12 -> DAC DIN
#define POD_I2S_BCK_PIN       10   // Pico pin 14 -> DAC BCK
#define POD_I2S_LRCK_PIN      11   // Pico pin 15 -> DAC WSEL   (= BCK + 1)

// ---- Display + touch share hardware SPI0 ------------------------------
// Only GP16/GP20 (or GP0/GP4) can be SPI0 RX on RP2350, so touch T_DO is on
// GP20 and the display RESET is on GP21 (the two wires are swapped
// relative to the first wiring plan).
#define POD_SPI                spi0
#define POD_SPI_SCK_PIN       18   // Pico pin 24 -> SCK  + T_CLK
#define POD_SPI_MOSI_PIN      19   // Pico pin 25 -> SDI(MOSI) + T_DIN
#define POD_SPI_MISO_PIN      20   // Pico pin 26 -> T_DO (display SDO NOT connected)

#define POD_TFT_CS_PIN        17   // Pico pin 22 -> CS
#define POD_TFT_DC_PIN        16   // Pico pin 21 -> DC
#define POD_TFT_RST_PIN       21   // Pico pin 27 -> RESET
#define POD_TFT_LED_PIN       13   // Pico pin 17 -> LED (backlight, PWM-capable)
#define POD_TFT_BAUD   (30 * 1000 * 1000)   // try 40-62 MHz once stable; drop to 20 MHz if you see glitches

#define POD_TOUCH_CS_PIN      22   // Pico pin 29 -> T_CS
#define POD_TOUCH_IRQ_PIN     26   // Pico pin 31 -> T_IRQ (low while pressed)
#define POD_TOUCH_BAUD   (2 * 1000 * 1000)  // XPT2046 max ~2.5 MHz

// ---- Reserved for later: SD slot on the display module (shares SPI0) --
// SD_SCK -> GP18, SD_MOSI -> GP19, SD_MISO -> GP20, SD_CS -> GP7
#define POD_SD_CS_PIN          7   // Pico pin 10

// Compatibility aliases used by the drivers
#define POD_TFT_SPI       POD_SPI
#define POD_TFT_SCK_PIN   POD_SPI_SCK_PIN
#define POD_TFT_MOSI_PIN  POD_SPI_MOSI_PIN
#define POD_TOUCH_SPI     POD_SPI
