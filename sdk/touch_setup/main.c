// pod_touch_setup - calibrate the touchscreen and SAVE it for pod.uf2.
//
// 1. Tap the 4 dots (corners). Raw numbers are shown live at the bottom the
//    whole time, so if a tap isn't registering you can see why.
// 2. Test: a cross follows your finger. Check it lands where you touch.
// 3. Tap SAVE (writes the calibration where pod.uf2 reads it) or REDO.
// Press/release is decided from the pressure reading only (no T_IRQ).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "ili9341.h"
#include "xpt2046.h"
#include "pod_pins.h"
#include "font8x8_basic.h"

#define BLACK  RGB565(0, 0, 0)
#define WHITE  RGB565(255, 255, 255)
#define GREEN  RGB565(40, 220, 90)
#define RED    RGB565(255, 60, 60)
#define YELLOW RGB565(255, 220, 0)
#define GREY   RGB565(140, 140, 140)
#define CYAN   RGB565(0, 220, 255)
#define DKGREY RGB565(50, 50, 55)

// ---- must match common/pod_display.c exactly
typedef struct {
    uint32_t magic;
    uint8_t swap_xy, pad[3];
    int32_t ax_l, ax_r, ay_t, ay_b;
    uint32_t check;
} touch_cal_t;
#define CAL_MAGIC  0x506F6444u
#define CAL_OFFSET (PICO_FLASH_SIZE_BYTES - 8 * FLASH_SECTOR_SIZE)
static uint32_t cal_sum(const touch_cal_t *c) {
    return c->magic ^ (uint32_t)c->swap_xy * 0x9E3779B9u ^ (uint32_t)c->ax_l ^ ((uint32_t)c->ax_r << 8) ^
           ((uint32_t)c->ay_t << 16) ^ ((uint32_t)c->ay_b << 24) ^ 0xA5A5A5A5u;
}

// ---- text (8x8 font, doubled)
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
static void text(int x, int y, const char *s, uint16_t fg, uint16_t bg) {
    for (; *s && x <= 224; s++, x += 16) draw_char(x, y, *s, fg, bg);
}
static void text_line(int row, const char *s, uint16_t fg) {
    char buf[16]; snprintf(buf, sizeof buf, "%-15s", s); text(0, row * 16, buf, fg, BLACK);
}

// ---- raw touch
static uint16_t xfer(uint8_t cmd) {
    uint8_t tx[3] = { cmd, 0, 0 }, rx[3];
    spi_write_read_blocking(POD_SPI, tx, rx, 3);
    return (uint16_t)(((rx[1] << 8) | rx[2]) >> 3);
}
typedef struct { int x, y, z1, z2; bool pressed; } sample_t;
static sample_t sample(void) {
    sample_t s;
    spi_set_baudrate(POD_SPI, POD_TOUCH_BAUD);
    gpio_put(POD_TOUCH_CS_PIN, 0);
    s.z1 = xfer(0xB0); s.z2 = xfer(0xC0);
    int xs[5], ys[5];
    xfer(0xD0);
    for (int i = 0; i < 5; i++) { xs[i] = xfer(0xD0); ys[i] = xfer(0x90); }
    xfer(0x80);
    gpio_put(POD_TOUCH_CS_PIN, 1);
    spi_set_baudrate(POD_SPI, POD_TFT_BAUD);
    for (int i = 0; i < 4; i++) for (int j = i + 1; j < 5; j++) {
        if (xs[j] < xs[i]) { int t = xs[i]; xs[i] = xs[j]; xs[j] = t; }
        if (ys[j] < ys[i]) { int t = ys[i]; ys[i] = ys[j]; ys[j] = t; }
    }
    s.x = xs[2]; s.y = ys[2];
    s.pressed = s.z1 >= 15;
    return s;
}
static void show_raw(const sample_t *s) {
    char b[32];
    snprintf(b, sizeof b, "X%4d Y%4d", s->x, s->y);       text_line(18, b, GREY);
    snprintf(b, sizeof b, "Z%4d %4d %s", s->z1, s->z2, s->pressed ? "ON" : "--");
    text_line(19, b, s->pressed ? GREEN : GREY);
}

// Wait for a tap; return the median position of the steady part of the press.
static sample_t wait_tap(void) {
    while (true) {
        sample_t s;
        do { s = sample(); show_raw(&s); sleep_ms(10); } while (!s.pressed);
        int xs[40], ys[40], n = 0, off = 0;
        sleep_ms(40);
        while (off < 6 && n < 40) {                     // until 6 unpressed samples in a row
            s = sample();
            if (s.pressed) { xs[n] = s.x; ys[n] = s.y; n++; off = 0; } else off++;
            sleep_ms(10);
        }
        show_raw(&s);
        do { s = sample(); sleep_ms(10); off = s.pressed ? 0 : off + 1; } while (off < 8);   // wait for lift
        if (n < 4) continue;                            // too short: try again
        for (int i = 0; i < n - 1; i++) for (int j = i + 1; j < n; j++) {
            if (xs[j] < xs[i]) { int t = xs[i]; xs[i] = xs[j]; xs[j] = t; }
            if (ys[j] < ys[i]) { int t = ys[i]; ys[i] = ys[j]; ys[j] = t; }
        }
        return (sample_t){ xs[n / 2], ys[n / 2], 0, 0, true };
    }
}

static touch_cal_t cal;
static bool map_pt(int rx, int ry, int *sx, int *sy) {
    int a = cal.swap_xy ? ry : rx, b = cal.swap_xy ? rx : ry;
    if (cal.ax_r == cal.ax_l || cal.ay_b == cal.ay_t) return false;
    *sx = 20 + (a - cal.ax_l) * 200 / (cal.ax_r - cal.ax_l);
    *sy = 20 + (b - cal.ay_t) * 280 / (cal.ay_b - cal.ay_t);
    return true;
}
static void cross(int x, int y, uint16_t c) {
    ili9341_fill_rect(x - 12, y - 1, 25, 3, c);
    ili9341_fill_rect(x - 1, y - 12, 3, 25, c);
}

static void calibrate(void) {
    const int tx[4] = {20, 220, 220, 20}, ty[4] = {20, 20, 300, 300};
    sample_t raw[4];
    ili9341_fill(BLACK);
    for (int i = 0; i < 4; i++) {
        text_line(7, "TOUCH SETUP", CYAN);
        char b[16]; snprintf(b, sizeof b, "Tap dot %d of 4", i + 1); text_line(9, b, WHITE);
        cross(tx[i], ty[i], YELLOW);
        raw[i] = wait_tap();
        cross(tx[i], ty[i], BLACK);
        cross(tx[i], ty[i], DKGREY);
    }
    int dx_x = abs((raw[1].x + raw[2].x) - (raw[0].x + raw[3].x));
    int dx_y = abs((raw[1].y + raw[2].y) - (raw[0].y + raw[3].y));
    cal.swap_xy = dx_y > dx_x;
    #define A(i) (cal.swap_xy ? raw[i].y : raw[i].x)
    #define B(i) (cal.swap_xy ? raw[i].x : raw[i].y)
    cal.ax_l = (A(0) + A(3)) / 2; cal.ax_r = (A(1) + A(2)) / 2;
    cal.ay_t = (B(0) + B(1)) / 2; cal.ay_b = (B(2) + B(3)) / 2;
}

static void save(void) {
    static uint8_t page[FLASH_PAGE_SIZE] __attribute__((aligned(4)));
    memset(page, 0xFF, sizeof page);
    cal.magic = CAL_MAGIC;
    cal.check = cal_sum(&cal);
    memcpy(page, &cal, sizeof cal);
    uint32_t irq = save_and_disable_interrupts();
    flash_range_erase(CAL_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(CAL_OFFSET, page, FLASH_PAGE_SIZE);
    restore_interrupts(irq);
}

int main(void) {
    stdio_init_all();
    ili9341_init();
    xpt2046_init();
    while (true) {
        calibrate();
        bool plausible = abs(cal.ax_r - cal.ax_l) >= 300 && abs(cal.ay_b - cal.ay_t) >= 300;

        // ---- test screen: cross follows the finger; SAVE / REDO buttons
        ili9341_fill(BLACK);
        text_line(0, plausible ? "TEST: draw on" : "LOOKS WRONG", plausible ? WHITE : RED);
        text_line(1, plausible ? "the screen" : "tap REDO", plausible ? WHITE : RED);
        char b[32];
        snprintf(b, sizeof b, "sw%d x%d-%d", cal.swap_xy, (int)cal.ax_l, (int)cal.ax_r); text_line(2, b, GREY);
        snprintf(b, sizeof b, "   y%d-%d", (int)cal.ay_t, (int)cal.ay_b);            text_line(3, b, GREY);
        ili9341_fill_rect(10, 236, 100, 40, plausible ? GREEN : DKGREY);
        text(28, 248, "SAVE", BLACK, plausible ? GREEN : DKGREY);
        ili9341_fill_rect(130, 236, 100, 40, YELLOW);
        text(148, 248, "REDO", BLACK, YELLOW);
        int lastx = -100, lasty = -100;
        while (true) {
            sample_t s = sample();
            show_raw(&s);
            int sx, sy;
            if (s.pressed && map_pt(s.x, s.y, &sx, &sy)) {
                if (sy >= 236 && sy < 276 && sx >= 130 && sx < 230) break;                  // REDO
                if (plausible && sy >= 236 && sy < 276 && sx >= 10 && sx < 110) {            // SAVE
                    save();
                    ili9341_fill(BLACK);
                    text_line(7, "SAVED!", GREEN);
                    text_line(9, "Now flash", WHITE);
                    text_line(10, "pod.uf2", WHITE);
                    while (true) sleep_ms(1000);
                }
                if (sy > 70 && sy < 230 && sx >= 0 && sx < 240) {
                    if (lastx > -100) cross(lastx, lasty, BLACK);
                    cross(sx, sy, CYAN);
                    ili9341_fill_rect(sx - 1, sy - 1, 3, 3, WHITE);
                    lastx = sx; lasty = sy;
                }
            }
            sleep_ms(15);
        }
    }
}
