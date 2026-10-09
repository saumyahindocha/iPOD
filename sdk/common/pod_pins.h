// pod_pins.h - single source of truth for the Pod wiring (perfboard build, Oct 2026).
// The pins were re-chosen for the perfboard so that 7 of the screen's pins sit
// directly below the Pico pin that drives them (one short solder link each).
// See docs/perfboard_build.pdf.
// Change a pin here, rebuild, and every firmware follows.
//
// Pico 2 W GPIO 23, 24, 25 and 29 are used internally by the CYW43439
// radio - never use them.
#pragma once

// ---- PCM5102 I2S DAC (PIO0, pico-extras audio_i2s) --------------------
// BCK must be exactly one GPIO below LRCK (PIO side-set pins are consecutive).
// Keep PICO_AUDIO_I2S_DATA_PIN / _CLOCK_PIN_BASE in CMakeLists.txt in step.
#define POD_I2S_DIN_PIN       13   // Pico pin 17 -> DAC DIN
#define POD_I2S_BCK_PIN       14   // Pico pin 19 -> DAC BCK
#define POD_I2S_LRCK_PIN      15   // Pico pin 20 -> DAC WSEL   (= BCK + 1)

// ---- Display + touch share hardware SPI0 ------------------------------
// SPI0 on GP4 (RX), GP6 (SCK), GP7 (TX). Display SDO stays unconnected.
#define POD_SPI                spi0
#define POD_SPI_SCK_PIN        6   // Pico pin 9  -> SCK + T_CLK + SD_SCK
#define POD_SPI_MOSI_PIN       7   // Pico pin 10 -> SDI(MOSI) + T_DIN + SD_MOSI
#define POD_SPI_MISO_PIN       4   // Pico pin 6  -> T_DO + SD_MISO (display SDO NOT connected)

#define POD_TFT_CS_PIN        12   // Pico pin 16 -> CS
#define POD_TFT_DC_PIN        10   // Pico pin 14 -> DC
#define POD_TFT_RST_PIN       11   // Pico pin 15 -> RESET
#define POD_TFT_LED_PIN        8   // Pico pin 11 -> LED (backlight, PWM-capable)
#define POD_TFT_BAUD   (30 * 1000 * 1000)   // try 40-62 MHz once stable; drop to 20 MHz if you see glitches

#define POD_TOUCH_CS_PIN       5   // Pico pin 7  -> T_CS
#define POD_TOUCH_IRQ_PIN      3   // Pico pin 5  -> T_IRQ (low while pressed)
#define POD_TOUCH_BAUD   (2 * 1000 * 1000)  // XPT2046 max ~2.5 MHz

// ---- SD slot on the display module (shares SPI0) ----------------------
#define POD_SD_CS_PIN          9   // Pico pin 12 -> SD_CS

// Compatibility aliases used by the drivers
#define POD_TFT_SPI       POD_SPI
#define POD_TFT_SCK_PIN   POD_SPI_SCK_PIN
#define POD_TFT_MOSI_PIN  POD_SPI_MOSI_PIN
#define POD_TOUCH_SPI     POD_SPI

// ---- Battery level ----------------------------------------------------
// Perfboard: equal-value divider (100k or 150k each) from the switched battery to GP28 (ADC2).
// A MAX17048 breakout on I2C0 (GP0/GP1) is still used instead if it answers.
#define POD_VBAT_ADC_PIN      28   // Pico pin 34, reads battery / 2
#define POD_VBAT_DIVIDER       2
#define POD_I2C               i2c0
#define POD_I2C_SDA_PIN        0   // Pico pin 1 (optional MAX17048)
#define POD_I2C_SCL_PIN        1   // Pico pin 2
// Charger "charging" status (e.g. BQ25180 /CHG, active low). -1 = not wired yet.
#define POD_CHG_STAT_PIN      -1
#define POD_LOW_BATTERY_PCT   15
