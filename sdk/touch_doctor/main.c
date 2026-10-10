// pod_touch_doctor - on-screen touch diagnosis, no computer needed.
// Shows, live on the display:
//   - T_IRQ level (Pico pin 5 / GP3) and how many times it went low
//   - whether the XPT2046 answers on SPI (checked by reading with MISO pulled
//     up and then down: a chip that answers gives the same numbers both ways,
//     a disconnected line just follows the pull)
//   - raw X / Y / Z, and a plain-language verdict naming the wire to check.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "ili9341.h"
#include "xpt2046.h"
#include "pod_pins.h"
#include "font8x8_basic.h"

#define BLACK  RGB565(0, 0, 0)
#define WHITE  RGB565(255, 255, 255)
#define GREEN  RGB565(40, 220, 90)
#define RED    RGB565(255, 60, 60)
#define YELLOW RGB565(255, 220, 0)
#define GREY   RGB565(150, 150, 150)
#define CYAN   RGB565(0, 220, 255)

static uint16_t glyph[16 * 16];

static void draw_char(int x, int y, char c, uint16_t fg, uint16_t bg) {
    const unsigned char *g = font8x8_basic[(unsigned char)c & 0x7F];
    for (int r = 0; r < 8; r++)
        for (int b = 0; b < 8; b++) {
            uint16_t col = (g[r] >> b) & 1 ? fg : bg;
            glyph[(2 * r) * 16 + 2 * b] = glyph[(2 * r) * 16 + 2 * b + 1] = col;
            glyph[(2 * r + 1) * 16 + 2 * b] = glyph[(2 * r + 1) * 16 + 2 * b + 1] = col;
        }
    ili9341_flush(x, y, x + 15, y + 15, glyph);
}

// One text line (15 characters of 16 px), padded so old text is overwritten.
static void line(int row, const char *s, uint16_t fg) {
    char buf[16];
    snprintf(buf, sizeof buf, "%-15s", s);
    for (int i = 0; i < 15; i++) draw_char(i * 16, row * 16, buf[i], fg, BLACK);
}

static uint16_t xfer(uint8_t cmd) {
    uint8_t tx[3] = { cmd, 0, 0 }, rx[3];
    spi_write_read_blocking(POD_SPI, tx, rx, 3);
    return (uint16_t)(((rx[1] << 8) | rx[2]) >> 3);
}

typedef struct { uint16_t z1, z2, x, y; } raw4_t;

static raw4_t read_raw(void) {
    raw4_t r;
    spi_set_baudrate(POD_SPI, POD_TOUCH_BAUD);
    gpio_put(POD_TOUCH_CS_PIN, 0);
    r.z1 = xfer(0xB0); r.z2 = xfer(0xC0); r.x = xfer(0xD0); r.y = xfer(0x90);
    xfer(0x80);                                   // power down, PENIRQ enabled
    gpio_put(POD_TOUCH_CS_PIN, 1);
    spi_set_baudrate(POD_SPI, POD_TFT_BAUD);
    return r;
}

int main(void) {
    stdio_init_all();
    ili9341_init();
    xpt2046_init();
    ili9341_fill(BLACK);
    line(0, "TOUCH DOCTOR", CYAN);
    line(1, "press & hold", GREY);
    line(2, "the screen", GREY);

    int low_count = 0, xmin = 4095, xmax = 0;
    bool was_low = false;
    while (true) {
        bool low = !gpio_get(POD_TOUCH_IRQ_PIN);
        if (low && !was_low) low_count++;
        was_low = low;

        // Answer test: same read with MISO pulled up, then pulled down.
        gpio_pull_up(POD_SPI_MISO_PIN);   sleep_us(50);
        raw4_t up = read_raw();
        gpio_pull_down(POD_SPI_MISO_PIN); sleep_us(50);
        raw4_t dn = read_raw();
        gpio_disable_pulls(POD_SPI_MISO_PIN);
        bool answers = abs((int)up.z1 - (int)dn.z1) < 400 && abs((int)up.z2 - (int)dn.z2) < 400 &&
                       !(up.z1 == 0 && up.z2 == 0) && !(up.z1 == 4095 && up.z2 == 4095);

        int z = (int)up.z1 + 4095 - (int)up.z2;
        bool pressed_reading = answers && up.z1 > 30;
        if (pressed_reading) { if (up.x < xmin) xmin = up.x; if (up.x > xmax) xmax = up.x; }

        char s[32];
        line(4, "IRQ pin5 GP3", WHITE);
        line(5, low ? " LOW = pressed" : " HIGH", low ? GREEN : GREY);
        snprintf(s, sizeof s, " went low: %d", low_count); line(6, s, WHITE);
        line(8, "CHIP pin6/pin7", WHITE);
        line(9, answers ? " ANSWERS" : " NO ANSWER", answers ? GREEN : RED);
        snprintf(s, sizeof s, "X%5u Y%5u", up.x, up.y); line(10, s, WHITE);
        snprintf(s, sizeof s, "Z%5d", answers ? z : 0); line(11, s, WHITE);

        const char *h[6] = { "", "", "", "", "", "" };
        uint16_t hc = YELLOW;
        if (!answers) {
            h[0] = "Check wire:"; h[1] = "Pico pin 7 to"; h[2] = "scr pin11 T_CS";
            h[3] = "and link:"; h[4] = "Pico pin 6 to"; h[5] = "scr pin13 T_DO";
        } else if (low_count == 0) {
            h[0] = "Chip OK."; h[1] = "Press screen."; h[2] = "If IRQ stays";
            h[3] = "HIGH, check:"; h[4] = "Pico pin 5 to"; h[5] = "scr pin14 IRQ";
        } else if (xmax - xmin > 600) {
            h[0] = "HARDWARE OK"; h[1] = "Touch works."; h[2] = "Tell Claude:"; h[3] = "software fix";
            hc = GREEN;
        } else {
            h[0] = "IRQ + chip OK."; h[1] = "Press in all"; h[2] = "4 corners,"; h[3] = "firmly.";
        }
        for (int i = 0; i < 6; i++) line(13 + i, h[i], hc);
        sleep_ms(100);
    }
}
