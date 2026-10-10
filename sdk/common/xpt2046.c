#include "xpt2046.h"
#include "pod_pins.h"
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "pod_spi_lock.h"

// Touch shares SPI0 with the display. Every transaction drops the bus to
// touch speed and restores display speed afterwards. Only ever call this
// from the core that also draws (core 1), so the two never interleave.

#define CMD_X  0xD0
#define CMD_Y  0x90
#define CMD_Z1 0xB0
#define CMD_Z2 0xC0

#define Z_THRESHOLD 60     // lower = lighter touch; 300 needed a firm press (tuned on hardware 2026-10-07)

void xpt2046_init(void) {
    // An SD card in the slot shares MISO; hold its chip-select high until SD mode uses it.
    gpio_init(POD_SD_CS_PIN);
    gpio_set_dir(POD_SD_CS_PIN, GPIO_OUT);
    gpio_put(POD_SD_CS_PIN, 1);

    // SPI0 itself (and SCK/MOSI) is initialised by ili9341_init(); add RX.
    gpio_set_function(POD_SPI_MISO_PIN, GPIO_FUNC_SPI);

    gpio_init(POD_TOUCH_CS_PIN);
    gpio_set_dir(POD_TOUCH_CS_PIN, GPIO_OUT);
    gpio_put(POD_TOUCH_CS_PIN, 1);

    gpio_init(POD_TOUCH_IRQ_PIN);
    gpio_set_dir(POD_TOUCH_IRQ_PIN, GPIO_IN);
    gpio_pull_up(POD_TOUCH_IRQ_PIN);
}

static uint16_t xfer(uint8_t cmd) {
    uint8_t tx[3] = { cmd, 0, 0 }, rx[3];
    spi_write_read_blocking(POD_SPI, tx, rx, 3);
    return (uint16_t)(((rx[1] << 8) | rx[2]) >> 3);
}

extern int xpt2046_last_z1, xpt2046_last_z2;

// One transaction, exactly as in pod_touch_setup (verified on the perfboard):
// Z1, Z2, then 5 X/Y pairs, median taken. Pressed = pressure, not T_IRQ.
static bool sample(xpt2046_raw_t *out) {
    int xs[5], ys[5];
    pod_spi_lock();
    spi_set_baudrate(POD_SPI, POD_TOUCH_BAUD);
    gpio_put(POD_TOUCH_CS_PIN, 0);
    int z1 = xfer(CMD_Z1), z2 = xfer(CMD_Z2);
    xfer(CMD_X);
    for (int i = 0; i < 5; i++) { xs[i] = xfer(CMD_X); ys[i] = xfer(CMD_Y); }
    xfer(0x80);
    gpio_put(POD_TOUCH_CS_PIN, 1);
    spi_set_baudrate(POD_SPI, POD_TFT_BAUD);
    pod_spi_unlock();
    for (int i = 0; i < 4; i++) for (int j = i + 1; j < 5; j++) {
        if (xs[j] < xs[i]) { int q = xs[i]; xs[i] = xs[j]; xs[j] = q; }
        if (ys[j] < ys[i]) { int q = ys[i]; ys[i] = ys[j]; ys[j] = q; }
    }
    int z = z1 + 4095 - z2;
    out->x = (uint16_t)xs[2];
    out->y = (uint16_t)ys[2];
    out->z = (uint16_t)(z < 0 ? 0 : z);
    xpt2046_last_z1 = z1; xpt2046_last_z2 = z2;
    return z1 > 20 && z >= Z_THRESHOLD;
}

int xpt2046_last_z1, xpt2046_last_z2;

bool xpt2046_irq_active(void) { xpt2046_raw_t r; return sample(&r); }
bool xpt2046_read(xpt2046_raw_t *out) { return sample(out); }
bool xpt2046_irq_line_low(void) { return !gpio_get(POD_TOUCH_IRQ_PIN); }
bool xpt2046_sample_raw(xpt2046_raw_t *out) { return sample(out); }
