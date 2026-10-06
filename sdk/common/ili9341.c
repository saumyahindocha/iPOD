#include "ili9341.h"
#include "pod_pins.h"
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "pod_spi_lock.h"

static inline void cs(bool active) { gpio_put(POD_TFT_CS_PIN, !active); }
static inline void dc(bool data)   { gpio_put(POD_TFT_DC_PIN, data); }

static void write_cmd(uint8_t c) {
    dc(false); cs(true);
    spi_write_blocking(POD_TFT_SPI, &c, 1);
    cs(false);
}

static void write_data(const uint8_t *d, size_t n) {
    dc(true); cs(true);
    spi_write_blocking(POD_TFT_SPI, d, n);
    cs(false);
}

static void cmd_args(uint8_t c, const uint8_t *d, size_t n) {
    write_cmd(c);
    if (n) write_data(d, n);
}

static void set_window(int x1, int y1, int x2, int y2) {
    uint8_t ca[4] = { x1 >> 8, x1 & 0xFF, x2 >> 8, x2 & 0xFF };
    uint8_t ra[4] = { y1 >> 8, y1 & 0xFF, y2 >> 8, y2 & 0xFF };
    cmd_args(0x2A, ca, 4);   // CASET
    cmd_args(0x2B, ra, 4);   // PASET
    write_cmd(0x2C);         // RAMWR
}

void ili9341_backlight(bool on) { gpio_put(POD_TFT_LED_PIN, on); }

void ili9341_set_madctl(uint8_t m) { pod_spi_lock(); cmd_args(0x36, &m, 1); pod_spi_unlock(); }

void ili9341_init(void) {
    pod_spi_lock();
    spi_init(POD_TFT_SPI, POD_TFT_BAUD);
    spi_set_format(POD_TFT_SPI, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(POD_TFT_SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(POD_TFT_MOSI_PIN, GPIO_FUNC_SPI);

    const uint pins[] = { POD_TFT_CS_PIN, POD_TFT_DC_PIN, POD_TFT_RST_PIN, POD_TFT_LED_PIN };
    for (unsigned i = 0; i < 4; i++) { gpio_init(pins[i]); gpio_set_dir(pins[i], GPIO_OUT); }
    gpio_put(POD_TFT_CS_PIN, 1);
    gpio_put(POD_TFT_LED_PIN, 0);

    // hardware reset
    gpio_put(POD_TFT_RST_PIN, 1); sleep_ms(5);
    gpio_put(POD_TFT_RST_PIN, 0); sleep_ms(20);
    gpio_put(POD_TFT_RST_PIN, 1); sleep_ms(150);

    write_cmd(0x01); sleep_ms(150);                       // SWRESET
    // Standard ILI9341 power/gamma sequence (same values as Adafruit/TFT_eSPI)
    static const uint8_t init[] = {
        0xEF, 3, 0x03, 0x80, 0x02,
        0xCF, 3, 0x00, 0xC1, 0x30,
        0xED, 4, 0x64, 0x03, 0x12, 0x81,
        0xE8, 3, 0x85, 0x00, 0x78,
        0xCB, 5, 0x39, 0x2C, 0x00, 0x34, 0x02,
        0xF7, 1, 0x20,
        0xEA, 2, 0x00, 0x00,
        0xC0, 1, 0x23,             // PWCTR1
        0xC1, 1, 0x10,             // PWCTR2
        0xC5, 2, 0x3E, 0x28,       // VMCTR1
        0xC7, 1, 0x86,             // VMCTR2
        0x36, 1, 0x48,             // MADCTL: portrait, BGR
        0x3A, 1, 0x55,             // COLMOD: 16 bit/pixel
        0xB1, 2, 0x00, 0x18,       // frame rate 79 Hz
        0xB6, 3, 0x08, 0x82, 0x27, // display function control
        0xF2, 1, 0x00,             // 3-gamma off
        0x26, 1, 0x01,             // gamma curve 1
        0xE0, 15, 0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00,
        0xE1, 15, 0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F,
        0x00                       // end
    };
    const uint8_t *p = init;
    while (*p) {
        uint8_t c = *p++, n = *p++;
        cmd_args(c, p, n);
        p += n;
    }
    write_cmd(0x11); sleep_ms(120);                       // SLPOUT
    write_cmd(0x29); sleep_ms(20);                        // DISPON
    pod_spi_unlock();
    ili9341_backlight(true);
}

void ili9341_fill_rect(int x, int y, int w, int h, uint16_t color) {
    if (w <= 0 || h <= 0) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > ILI9341_W) w = ILI9341_W - x;
    if (y + h > ILI9341_H) h = ILI9341_H - y;
    if (w <= 0 || h <= 0) return;

    pod_spi_lock();
    set_window(x, y, x + w - 1, y + h - 1);
    uint8_t line[2 * ILI9341_W];
    for (int i = 0; i < w; i++) { line[2 * i] = color >> 8; line[2 * i + 1] = color & 0xFF; }
    dc(true); cs(true);
    for (int r = 0; r < h; r++) spi_write_blocking(POD_TFT_SPI, line, 2 * w);
    cs(false);
    pod_spi_unlock();
}

void ili9341_fill(uint16_t color) { ili9341_fill_rect(0, 0, ILI9341_W, ILI9341_H, color); }

void ili9341_flush(int x1, int y1, int x2, int y2, const uint16_t *px) {
    pod_spi_lock();
    set_window(x1, y1, x2, y2);
    size_t n = (size_t)(x2 - x1 + 1) * (size_t)(y2 - y1 + 1);
    uint8_t chunk[128];
    dc(true); cs(true);
    while (n) {
        size_t k = n > sizeof(chunk) / 2 ? sizeof(chunk) / 2 : n;
        for (size_t i = 0; i < k; i++) { chunk[2 * i] = px[i] >> 8; chunk[2 * i + 1] = px[i] & 0xFF; }
        spi_write_blocking(POD_TFT_SPI, chunk, 2 * k);
        px += k; n -= k;
    }
    cs(false);
    pod_spi_unlock();
}

// Sent in bands of BLIT_BAND rows, releasing the shared SPI bus in between, so a
// full-screen update never blocks the SD card (and therefore audio) for more than
// a few milliseconds.
#define BLIT_BAND 24
void ili9341_blit_be(int x, int y, int w, int h, const uint16_t *fb, int stride) {
    if (w <= 0 || h <= 0) return;
    for (int y0 = y; y0 < y + h; y0 += BLIT_BAND) {
        int bh = (y + h - y0) < BLIT_BAND ? (y + h - y0) : BLIT_BAND;
        const uint16_t *row = fb + (size_t)y0 * (size_t)stride + (size_t)x;
        pod_spi_lock();
        set_window(x, y0, x + w - 1, y0 + bh - 1);
        dc(true); cs(true);
        if (w == stride) {
            spi_write_blocking(POD_TFT_SPI, (const uint8_t *)row, (size_t)w * (size_t)bh * 2);
        } else {
            for (int r = 0; r < bh; r++, row += stride)
                spi_write_blocking(POD_TFT_SPI, (const uint8_t *)row, (size_t)w * 2);
        }
        cs(false);
        pod_spi_unlock();
    }
}
