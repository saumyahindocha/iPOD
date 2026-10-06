#include "xpt2046.h"
#include "pod_pins.h"
#include "pico/stdlib.h"
#include "hardware/spi.h"

// Touch shares SPI0 with the display. Every transaction drops the bus to
// touch speed and restores display speed afterwards. Only ever call this
// from the core that also draws (core 1), so the two never interleave.

#define CMD_X  0xD0
#define CMD_Y  0x90
#define CMD_Z1 0xB0
#define CMD_Z2 0xC0

#define Z_THRESHOLD 60     // lower = lighter touch; 300 needed a firm press (tuned on hardware 2026-10-07)

void xpt2046_init(void) {
    // SPI0 itself (and SCK/MOSI) is initialised by ili9341_init(); add RX.
    gpio_set_function(POD_SPI_MISO_PIN, GPIO_FUNC_SPI);

    gpio_init(POD_TOUCH_CS_PIN);
    gpio_set_dir(POD_TOUCH_CS_PIN, GPIO_OUT);
    gpio_put(POD_TOUCH_CS_PIN, 1);

    gpio_init(POD_TOUCH_IRQ_PIN);
    gpio_set_dir(POD_TOUCH_IRQ_PIN, GPIO_IN);
    gpio_pull_up(POD_TOUCH_IRQ_PIN);
}

bool xpt2046_irq_active(void) { return !gpio_get(POD_TOUCH_IRQ_PIN); }

static uint16_t xfer(uint8_t cmd) {
    uint8_t tx[3] = { cmd, 0, 0 }, rx[3];
    spi_write_read_blocking(POD_SPI, tx, rx, 3);
    return (uint16_t)(((rx[1] << 8) | rx[2]) >> 3);
}

bool xpt2046_read(xpt2046_raw_t *out) {
    spi_set_baudrate(POD_SPI, POD_TOUCH_BAUD);
    gpio_put(POD_TOUCH_CS_PIN, 0);
    uint16_t z1 = xfer(CMD_Z1), z2 = xfer(CMD_Z2);
    int z = (int)z1 + 4095 - (int)z2;
    bool ok = z >= Z_THRESHOLD;
    uint16_t xs[4], ys[4];
    if (ok) {
        xfer(CMD_X);
        for (int i = 0; i < 4; i++) { xs[i] = xfer(CMD_X); ys[i] = xfer(CMD_Y); }
    }
    xfer(0x80);   // power down, re-arm PENIRQ
    gpio_put(POD_TOUCH_CS_PIN, 1);
    spi_set_baudrate(POD_SPI, POD_TFT_BAUD);
    if (!ok) return false;

    for (int i = 0; i < 3; i++) for (int j = i + 1; j < 4; j++) {
        if (xs[j] < xs[i]) { uint16_t t = xs[i]; xs[i] = xs[j]; xs[j] = t; }
        if (ys[j] < ys[i]) { uint16_t t = ys[i]; ys[i] = ys[j]; ys[j] = t; }
    }
    out->x = (xs[1] + xs[2]) / 2;
    out->y = (ys[1] + ys[2]) / 2;
    out->z = (uint16_t)z;
    return true;
}
