// pod_display_test - ILI9341 + XPT2046 bring-up, no Bluetooth, no LVGL.
//
// 1. Colour check: RED, GREEN, BLUE, WHITE, BLACK full-screen, 700 ms each.
//    (If you see BLUE when serial says RED, the panel is RGB not BGR: change
//     MADCTL 0x48 -> 0x40 in ili9341.c.)
// 2. Geometry check: 1 px white border + coloured corner squares
//    (top-left RED, top-right GREEN, bottom-left BLUE, bottom-right WHITE).
// 3. Touch calibration: tap the YELLOW crosshair each time it appears
//    (4 corners). Calibration constants are printed over USB serial -
//    copy them; the LVGL port will need them.
// 4. Paint mode: drag a fingernail/stylus to draw. Tap the red box
//    (bottom-right) to clear. Raw readings stream to serial.

#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "ili9341.h"
#include "xpt2046.h"
#include "pod_pins.h"

#define RED    RGB565(255, 0, 0)
#define GREEN  RGB565(0, 255, 0)
#define BLUE   RGB565(0, 0, 255)
#define WHITE  RGB565(255, 255, 255)
#define BLACK  RGB565(0, 0, 0)
#define YELLOW RGB565(255, 255, 0)
#define CYAN   RGB565(0, 255, 255)

typedef struct {
    bool swap_xy;
    int ax_at_left, ax_at_right;   // raw value of the axis mapped to screen X at x=20 / x=220
    int ay_at_top,  ay_at_bottom;  // raw value of the axis mapped to screen Y at y=20 / y=300
} touch_cal_t;

static void crosshair(int x, int y, uint16_t c) {
    ili9341_fill_rect(x - 10, y, 21, 1, c);
    ili9341_fill_rect(x, y - 10, 1, 21, c);
    ili9341_fill_rect(x - 1, y - 1, 3, 3, c);
}

static xpt2046_raw_t wait_for_tap(void) {
    // wait for a press, average up to 64 good readings until release, retry
    // if the press was too light to produce any valid reading
    while (true) {
        xpt2046_raw_t r;
        uint32_t sx = 0, sy = 0;
        int n = 0;
        while (!xpt2046_irq_active()) sleep_ms(5);
        sleep_ms(30);  // debounce
        while (xpt2046_irq_active()) {
            if (n < 64 && xpt2046_read(&r)) { sx += r.x; sy += r.y; n++; }
            sleep_ms(10);
        }
        sleep_ms(150);
        if (n >= 3) return (xpt2046_raw_t){ (uint16_t)(sx / n), (uint16_t)(sy / n), 0 };
        printf("[touch] press too light, tap again\n");
    }
}

static bool map_touch(const touch_cal_t *c, const xpt2046_raw_t *r, int *sx, int *sy) {
    int a = c->swap_xy ? r->y : r->x;
    int b = c->swap_xy ? r->x : r->y;
    if (c->ax_at_right == c->ax_at_left || c->ay_at_bottom == c->ay_at_top) return false;
    *sx = 20 + (a - c->ax_at_left) * 200 / (c->ax_at_right - c->ax_at_left);
    *sy = 20 + (b - c->ay_at_top) * 280 / (c->ay_at_bottom - c->ay_at_top);
    if (*sx < 0) *sx = 0;
    if (*sx >= ILI9341_W) *sx = ILI9341_W - 1;
    if (*sy < 0) *sy = 0;
    if (*sy >= ILI9341_H) *sy = ILI9341_H - 1;
    return true;
}

int main(void) {
    stdio_init_all();
    sleep_ms(1500);
    printf("\n[pod_display_test] SPI0 shared: SCK=GP%d MOSI=GP%d MISO(T_DO)=GP%d\n",
           POD_SPI_SCK_PIN, POD_SPI_MOSI_PIN, POD_SPI_MISO_PIN);
    printf("[pod_display_test] TFT  CS=GP%d DC=GP%d RST=GP%d LED=GP%d\n",
           POD_TFT_CS_PIN, POD_TFT_DC_PIN, POD_TFT_RST_PIN, POD_TFT_LED_PIN);
    printf("[pod_display_test] Touch CS=GP%d IRQ=GP%d\n", POD_TOUCH_CS_PIN, POD_TOUCH_IRQ_PIN);

    ili9341_init();
    xpt2046_init();

    // ---- 1. colour check
    const struct { uint16_t c; const char *n; } seq[] = {
        {RED, "RED"}, {GREEN, "GREEN"}, {BLUE, "BLUE"}, {WHITE, "WHITE"}, {BLACK, "BLACK"}};
    for (unsigned i = 0; i < 5; i++) {
        printf("[colour] screen should now be %s\n", seq[i].n);
        absolute_time_t t0 = get_absolute_time();
        ili9341_fill(seq[i].c);
        printf("         full-screen fill took %lld us\n", absolute_time_diff_us(t0, get_absolute_time()));
        sleep_ms(700);
    }

    // ---- 2. geometry check
    printf("[geometry] white border; corners TL=RED TR=GREEN BL=BLUE BR=WHITE\n");
    ili9341_fill_rect(0, 0, ILI9341_W, 1, WHITE);
    ili9341_fill_rect(0, ILI9341_H - 1, ILI9341_W, 1, WHITE);
    ili9341_fill_rect(0, 0, 1, ILI9341_H, WHITE);
    ili9341_fill_rect(ILI9341_W - 1, 0, 1, ILI9341_H, WHITE);
    ili9341_fill_rect(4, 4, 30, 30, RED);
    ili9341_fill_rect(ILI9341_W - 34, 4, 30, 30, GREEN);
    ili9341_fill_rect(4, ILI9341_H - 34, 30, 30, BLUE);
    ili9341_fill_rect(ILI9341_W - 34, ILI9341_H - 34, 30, 30, WHITE);
    sleep_ms(3000);

    // ---- 3. calibration
    ili9341_fill(BLACK);
    const int tx[4] = {20, 220, 220, 20}, ty[4] = {20, 20, 300, 300};
    const char *tn[4] = {"top-left", "top-right", "bottom-right", "bottom-left"};
    xpt2046_raw_t raw[4];
    printf("[touch] calibration: tap each yellow crosshair (fingernail/stylus works best)\n");
    for (int i = 0; i < 4; i++) {
        crosshair(tx[i], ty[i], YELLOW);
        raw[i] = wait_for_tap();
        crosshair(tx[i], ty[i], BLACK);
        printf("[touch] %-12s raw x=%4u y=%4u\n", tn[i], raw[i].x, raw[i].y);
    }
    touch_cal_t cal;
    // Moving left->right on screen: which raw axis changed more?
    int dx_raw_x = abs((raw[1].x + raw[2].x) - (raw[0].x + raw[3].x));
    int dx_raw_y = abs((raw[1].y + raw[2].y) - (raw[0].y + raw[3].y));
    cal.swap_xy = dx_raw_y > dx_raw_x;
    #define A(i) (cal.swap_xy ? raw[i].y : raw[i].x)
    #define B(i) (cal.swap_xy ? raw[i].x : raw[i].y)
    cal.ax_at_left   = (A(0) + A(3)) / 2;
    cal.ax_at_right  = (A(1) + A(2)) / 2;
    cal.ay_at_top    = (B(0) + B(1)) / 2;
    cal.ay_at_bottom = (B(2) + B(3)) / 2;
    printf("\n[touch] ==== CALIBRATION - save these for the LVGL port ====\n");
    printf("#define TOUCH_SWAP_XY      %d\n", cal.swap_xy);
    printf("#define TOUCH_RAW_X_AT_20  %d   /* screen x = 20  */\n", cal.ax_at_left);
    printf("#define TOUCH_RAW_X_AT_220 %d   /* screen x = 220 */\n", cal.ax_at_right);
    printf("#define TOUCH_RAW_Y_AT_20  %d   /* screen y = 20  */\n", cal.ay_at_top);
    printf("#define TOUCH_RAW_Y_AT_300 %d   /* screen y = 300 */\n", cal.ay_at_bottom);
    printf("[touch] ====================================================\n\n");

    // ---- 4. paint mode
    ili9341_fill(BLACK);
    ili9341_fill_rect(ILI9341_W - 30, ILI9341_H - 30, 30, 30, RED);   // clear button
    printf("[paint] draw with a stylus; tap the red box to clear\n");
    uint32_t last_print = 0;
    while (true) {
        xpt2046_raw_t r;
        if (xpt2046_irq_active() && xpt2046_read(&r)) {
            int sx, sy;
            if (map_touch(&cal, &r, &sx, &sy)) {
                if (sx >= ILI9341_W - 30 && sy >= ILI9341_H - 30) {
                    ili9341_fill(BLACK);
                    ili9341_fill_rect(ILI9341_W - 30, ILI9341_H - 30, 30, 30, RED);
                    sleep_ms(300);
                } else {
                    ili9341_fill_rect(sx - 1, sy - 1, 3, 3, CYAN);
                }
                uint32_t now = to_ms_since_boot(get_absolute_time());
                if (now - last_print > 100) {
                    printf("[paint] raw x=%4u y=%4u z=%4u -> screen (%3d,%3d)\n", r.x, r.y, r.z, sx, sy);
                    last_print = now;
                }
            }
        }
        sleep_ms(5);
    }
}
