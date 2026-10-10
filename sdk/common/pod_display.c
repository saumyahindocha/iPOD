#include "pod_display.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/mutex.h"
#include "pico/flash.h"
#include "hardware/flash.h"
#include "hardware/sync.h"

#include "ili9341.h"
#include "xpt2046.h"
#include "pod_gfx.h"
#include "tjpgd.h"
#include "pod_logo.h"
#include "pod_wallpaper.h"

// =================================================================== layout
// Option C "Poster": 240x240 full-bleed art fading into an accent colour.
#define ART_H        240
#define FADE_TOP     150            // art starts fading into the base colour here
#define SCRIM_H      24             // darkened strip behind the status bar
#define TITLE_Y      204
#define ARTIST_Y     231
#define TEXT_MAX_W   216
#define CTRL_Y       256            // everything below is the controls strip
#define BAR_Y        264            // progress bar centre line
#define BAR_X0       20
#define BAR_X1       220
#define TIME_Y       271
#define BTN_Y        300            // transport row centre
#define BTN_R        17
#define STATUS_TAP_W 120            // left part of the status bar is a button (Home / Library)

// Library list
#define LIST_HDR_H   44
#define ROW_H        40

static const rgb_t WHITE = {255, 255, 255};
static const rgb_t IDLE_BASE = {34, 34, 40};
static const rgb_t LOW_RED = {235, 72, 72};
static const rgb_t CHARGE_GREEN = {80, 210, 120};

#define TXT_LEN 160

enum { SCREEN_NOW = 0, SCREEN_LIST = 1 };

// ================================================================== mailbox
enum {
    D_STATUS = 1u << 0, D_TEXT = 1u << 1, D_PLAY = 1u << 2, D_VOL = 1u << 3,
    D_ART = 1u << 4, D_ART_CLEAR = 1u << 5, D_PROGRESS = 1u << 6,
    D_BATT = 1u << 7, D_TOAST = 1u << 8, D_SCREEN = 1u << 9, D_LIST = 1u << 10,
};

typedef struct {
    char status[40];
    char title[TXT_LEN], artist[TXT_LEN], album[TXT_LEN];
    bool playing;
    int volume;
    uint32_t pos_ms, len_ms;
    uint64_t pos_stamp_us;
    size_t art_len;
    int battery;                    // -1 = no gauge
    bool charging;
    char toast[48];
    int screen;
    int list_highlight;
} pod_state_t;

static mutex_t mbox_lock;
static uint32_t mbox_dirty;
static pod_state_t mbox;
static uint8_t mbox_art[POD_ART_MAX_BYTES];
static bool sd_mode;                // fixed before core 1 starts

// Touch -> core 0 command queue (single producer: core 1, single consumer: core 0)
#define CMDQ_LEN 8
static volatile uint8_t cmdq[CMDQ_LEN];
static volatile uint32_t cmdq_w, cmdq_r;
static volatile int mbox_vol_req = -1;       // latest volume set on the Pod's slider
static volatile uint32_t mbox_vol_seq;       // bumped on every slider change (core 0 tracks what it applied)
static volatile int mbox_seek_req;           // permille, for POD_CMD_SEEK
static volatile int mbox_list_req;           // row, for POD_CMD_LIST_SELECT

static void push_cmd(pod_cmd_t c) {
    uint32_t w = cmdq_w;
    if (w - cmdq_r >= CMDQ_LEN) return;     // full: drop (only happens if core 0 stalls)
    cmdq[w % CMDQ_LEN] = (uint8_t)c;
    __dmb();
    cmdq_w = w + 1;
}

// Library list: names in one pool. Own lock, because core 1 reads it while drawing.
static mutex_t list_lock;
static char list_pool[POD_LIST_POOL];
static uint16_t list_off[POD_LIST_MAX];
static uint8_t list_folder[POD_LIST_MAX];
static int list_n, list_used;
static char list_title[64];
static bool list_can_back;

// core-1-private
static pod_state_t cur;
static uint8_t jpeg_work[POD_ART_MAX_BYTES];
#define ART_MAX 200
static uint16_t art_rgb[ART_MAX * ART_MAX];          // decoded cover, native RGB565
static int art_w, art_h;
static bool has_art;
static rgb_t base = {34, 34, 40};

// ============================================================ touch + calib
typedef struct {
    uint32_t magic;
    uint8_t swap_xy, pad[3];
    int32_t ax_l, ax_r, ay_t, ay_b;                  // raw at screen x=20/220, y=20/300
    uint32_t check;
} touch_cal_t;

#define CAL_MAGIC  0x506F6444u                       // "PodD" (bumped for the perfboard: old breadboard values are ignored)
#define CAL_OFFSET (PICO_FLASH_SIZE_BYTES - 8 * FLASH_SECTOR_SIZE)   // well clear of BTstack's keys
static touch_cal_t cal;

static bool cal_plausible(const touch_cal_t *c) {
    return abs((int)c->ax_r - (int)c->ax_l) >= 800 && abs((int)c->ay_b - (int)c->ay_t) >= 800;
}

static uint32_t cal_sum(const touch_cal_t *c) {
    return c->magic ^ (uint32_t)c->swap_xy * 0x9E3779B9u ^ (uint32_t)c->ax_l ^ ((uint32_t)c->ax_r << 8) ^
           ((uint32_t)c->ay_t << 16) ^ ((uint32_t)c->ay_b << 24) ^ 0xA5A5A5A5u;
}

static bool cal_load(void) {
    const touch_cal_t *f = (const touch_cal_t *)(XIP_BASE + CAL_OFFSET);
    if (f->magic != CAL_MAGIC || f->check != cal_sum(f)) return false;
    if (!cal_plausible(f)) return false;
    cal = *f;
    return true;
}

// Runs on core 0 during boot, BEFORE core 1 is launched (core 1 is still parked
// in the boot ROM, not executing from flash), so disabling interrupts on this
// core is all that's needed for a safe flash write.
static void cal_save(void) {
    static uint8_t page[FLASH_PAGE_SIZE] __attribute__((aligned(4)));
    memset(page, 0xFF, sizeof page);
    cal.magic = CAL_MAGIC;
    cal.check = cal_sum(&cal);
    memcpy(page, &cal, sizeof cal);
    uint32_t irq = save_and_disable_interrupts();
    flash_range_erase(CAL_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(CAL_OFFSET, page, FLASH_PAGE_SIZE);
    restore_interrupts(irq);
    printf("Pod: touch calibration %s\n", cal_load() ? "saved to flash" : "NOT saved (verify failed)");
}

static bool map_touch(const xpt2046_raw_t *r, int *sx, int *sy) {
    int a = cal.swap_xy ? r->y : r->x, b = cal.swap_xy ? r->x : r->y;
    if (cal.ax_r == cal.ax_l || cal.ay_b == cal.ay_t) return false;
    *sx = 20 + (a - cal.ax_l) * 200 / (cal.ax_r - cal.ax_l);
    *sy = 20 + (b - cal.ay_t) * 280 / (cal.ay_b - cal.ay_t);
    return true;
}

static void blit_all(void) { ili9341_blit_be(0, 0, GFX_W, GFX_H, gfx_fb, GFX_W); }
static void blit_rows(int y, int h) { ili9341_blit_be(0, y, GFX_W, h, gfx_fb, GFX_W); }

#include "font8x8_basic.h"
// Live raw touch readout at the bottom of the setup screen (same as pod_touch_setup).
static void raw_char(int x, int y, char c, uint16_t fg) {
    static uint16_t g16[16 * 16];
    const unsigned char *g = font8x8_basic[(unsigned char)c & 0x7F];
    for (int r = 0; r < 8; r++)
        for (int b = 0; b < 8; b++) {
            uint16_t col = (g[r] >> b) & 1 ? fg : 0;
            g16[(2 * r) * 16 + 2 * b] = g16[(2 * r) * 16 + 2 * b + 1] = col;
            g16[(2 * r + 1) * 16 + 2 * b] = g16[(2 * r + 1) * 16 + 2 * b + 1] = col;
        }
    ili9341_flush(x, y, x + 15, y + 15, g16);
}
static void raw_line(int y, const char *s, uint16_t fg) {
    char buf[16]; snprintf(buf, sizeof buf, "%-15s", s);
    for (int i = 0; i < 15; i++) raw_char(i * 16, y, buf[i], fg);
}
static void show_raw(const xpt2046_raw_t *r, bool pressed) {
    char b[32];
    snprintf(b, sizeof b, "X%4u Y%4u", r->x, r->y);
    raw_line(GFX_H - 36, b, RGB565(150, 150, 150));
    snprintf(b, sizeof b, "Z%4d %4d %s", xpt2046_last_z1, xpt2046_last_z2, pressed ? "ON" : "--");
    raw_line(GFX_H - 18, b, pressed ? RGB565(40, 220, 90) : RGB565(150, 150, 150));
}

// Small square in the top-right corner: green while a touch is seen, grey otherwise.
static void touch_led(bool on) {
    ili9341_fill_rect(GFX_W - 18, 6, 12, 12, on ? RGB565(40, 220, 90) : RGB565(90, 90, 96));
}

// Pressure-only tap capture (same method as pod_touch_setup): wait for a press,
// collect readings until 6 unpressed samples in a row, wait for a clear lift,
// return the median position.
static xpt2046_raw_t wait_tap(void) {
    while (true) {
        uint16_t xs[40], ys[40];
        int n = 0, off = 0;
        xpt2046_raw_t r;
        touch_led(false);
        for (int k = 0; ; k++) {
            xpt2046_raw_t s;
            bool p = xpt2046_sample_raw(&s);
            if (k % 5 == 0 || p) show_raw(&s, p);
            if (p) break;
            sleep_ms(10);
        }
        touch_led(true);
        sleep_ms(40);
        while (off < 6 && n < 40) {
            if (xpt2046_read(&r)) { xs[n] = r.x; ys[n] = r.y; n++; off = 0; } else off++;
            sleep_ms(10);
        }
        off = 0;
        while (off < 8) { off = xpt2046_irq_active() ? 0 : off + 1; sleep_ms(10); }
        touch_led(false);
        if (n < 4) continue;
        for (int i = 0; i < n - 1; i++) for (int j = i + 1; j < n; j++) {
            if (xs[j] < xs[i]) { uint16_t q = xs[i]; xs[i] = xs[j]; xs[j] = q; }
            if (ys[j] < ys[i]) { uint16_t q = ys[i]; ys[i] = ys[j]; ys[j] = q; }
        }
        return (xpt2046_raw_t){ xs[n / 2], ys[n / 2], 0 };
    }
}

static void calibrate_once(void);
static void calibrate(void) {
    while (true) {
        calibrate_once();
        if (cal_plausible(&cal)) return;
        printf("Pod: calibration looked wrong (x %ld..%ld, y %ld..%ld), retrying\n",
               (long)cal.ax_l, (long)cal.ax_r, (long)cal.ay_t, (long)cal.ay_b);
        gfx_fill_rect(0, 0, GFX_W, GFX_H, IDLE_BASE);
        gfx_text(&pod_font_title, 120, 140, "Let's try again", WHITE, 1);
        gfx_text(&pod_font_body, 120, 170, "Tap each dot's centre", (rgb_t){190, 190, 196}, 1);
        blit_all();
        sleep_ms(1500);
    }
}

static void calibrate_once(void) {
    const int tx[4] = {20, 220, 220, 20}, ty[4] = {20, 20, 300, 300};
    xpt2046_raw_t raw[4];
    printf("Pod: touch calibration - tap each dot (fingernail or stylus)\n");
    for (int i = 0; i < 4; i++) {
        gfx_fill_rect(0, 0, GFX_W, GFX_H, IDLE_BASE);
        gfx_text(&pod_font_title, 120, 120, "Touch setup", WHITE, 1);
        gfx_text(&pod_font_body, 120, 150, "Tap the dot precisely", (rgb_t){190, 190, 196}, 1);
        char step[16]; snprintf(step, sizeof step, "%d of 4", i + 1);
        gfx_text(&pod_font_small, 120, 172, step, (rgb_t){140, 140, 150}, 1);
        gfx_circle(tx[i] + 0.5f, ty[i] + 0.5f, 9, (rgb_t){70, 70, 80});
        gfx_circle(tx[i] + 0.5f, ty[i] + 0.5f, 4, WHITE);
        blit_all();
        raw[i] = wait_tap();
        printf("Pod:   point %d raw x=%u y=%u\n", i + 1, raw[i].x, raw[i].y);
    }
    int dx_x = (raw[1].x + raw[2].x) - (raw[0].x + raw[3].x); if (dx_x < 0) dx_x = -dx_x;
    int dx_y = (raw[1].y + raw[2].y) - (raw[0].y + raw[3].y); if (dx_y < 0) dx_y = -dx_y;
    cal.swap_xy = dx_y > dx_x;
    #define A(i) (cal.swap_xy ? raw[i].y : raw[i].x)
    #define B(i) (cal.swap_xy ? raw[i].x : raw[i].y)
    cal.ax_l = (A(0) + A(3)) / 2; cal.ax_r = (A(1) + A(2)) / 2;
    cal.ay_t = (B(0) + B(1)) / 2; cal.ay_b = (B(2) + B(3)) / 2;
    #undef A
    #undef B
    printf("Pod: calibration swap=%d x:%ld..%ld y:%ld..%ld\n", cal.swap_xy,
           (long)cal.ax_l, (long)cal.ax_r, (long)cal.ay_t, (long)cal.ay_b);
}

// ============================================================== rendering
static void round_rect(int x, int y, int w, int h, int r, rgb_t c) {
    gfx_fill_rect(x + r, y, w - 2 * r, h, c);
    gfx_fill_rect(x, y + r, r, h - 2 * r, c);
    gfx_fill_rect(x + w - r, y + r, r, h - 2 * r, c);
    gfx_circle(x + r, y + r, r, c);
    gfx_circle(x + w - r - 1, y + r, r, c);
    gfx_circle(x + r, y + h - r - 1, r, c);
    gfx_circle(x + w - r - 1, y + h - r - 1, r, c);
}

static void draw_chevron_left(int x, int y, int h, rgb_t c) {   // "<" made of two thin triangles
    float m = y + h / 2.0f;
    gfx_triangle(x, m, x + h / 2.0f + 1, y, x + h / 2.0f + 3, y + 1.5f, c);
    gfx_triangle(x + 1.2f, m - 1.2f, x + h / 2.0f + 3, y + 1.5f, x + 2.5f, m, c);
    gfx_triangle(x, m, x + h / 2.0f + 3, y + h - 1.5f, x + h / 2.0f + 1, y + h, c);
    gfx_triangle(x + 2.5f, m, x + h / 2.0f + 3, y + h - 1.5f, x + 1.2f, m + 1.2f, c);
}

static void draw_chevron_right(int x, int y, int h, rgb_t c) {  // ">"
    float m = y + h / 2.0f, r = x + h / 2.0f + 3;
    gfx_triangle(r, m, x + 2, y, x, y + 1.5f, c);
    gfx_triangle(r - 1.2f, m - 1.2f, x, y + 1.5f, r - 2.5f, m, c);
    gfx_triangle(r, m, x, y + h - 1.5f, x + 2, y + h, c);
    gfx_triangle(r - 2.5f, m, x, y + h - 1.5f, r - 1.2f, m + 1.2f, c);
}

// Battery icon with its right edge at xr; returns the x where it starts.
static int draw_battery(int xr, int y) {
    if (cur.battery < 0) return xr;
    int w = 20, h = 10, x = xr - w - 2;
    bool low = cur.battery < 15 && !cur.charging;
    rgb_t fill = cur.charging ? CHARGE_GREEN : low ? LOW_RED : WHITE;
    gfx_fill_rect(x, y, w, h, WHITE);                       // outline
    gfx_fill_rect(x + 1, y + 1, w - 2, h - 2, (rgb_t){0, 0, 0});
    int lvl = (w - 4) * cur.battery / 100;
    if (lvl < 1 && cur.battery > 0) lvl = 1;
    gfx_fill_rect(x + 2, y + 2, lvl, h - 4, fill);
    gfx_fill_rect(x + w, y + 3, 2, h - 6, WHITE);           // terminal nub
    char pct[8]; snprintf(pct, sizeof pct, "%d%%", cur.battery);
    int tw = gfx_text_width(&pod_font_status, pct);
    gfx_text(&pod_font_status, x - 4, y - 2, pct, WHITE, 2);
    return x - 4 - tw;
}

// House icon, 15 x 13, top-left corner at (x, y).
static void draw_house_icon(int x, int y, rgb_t c) {
    gfx_triangle(x - 1, y + 6.5f, x + 7.5f, y - 0.5f, x + 16, y + 6.5f, c);   // roof
    gfx_fill_rect(x + 2, y + 6, 11, 7, c);                                     // walls
    gfx_fill_rect(x + 6, y + 9, 3, 4, (rgb_t){0, 0, 0});                       // door
}

// Status bar: [house] Home button, then the status text (SD mode: "< Library" button).
static void draw_status_bar(const char *left, bool sd) {
    draw_house_icon(10, 5, WHITE);
    gfx_fill_rect(33, 5, 1, 13, (rgb_t){150, 150, 160});                      // divider
    int x = 42;
    if (sd) { draw_chevron_left(41, 6, 11, WHITE); x = 54; }
    gfx_text(&pod_font_status, x, 5, left, WHITE, 0);
    int xr = draw_battery(GFX_W - 8, 6);
    if (cur.screen == SCREEN_NOW && cur.volume >= 0) {
        char v[16]; snprintf(v, sizeof v, "Vol %d%%", cur.volume);
        gfx_text(&pod_font_status, xr - (cur.battery >= 0 ? 10 : 2), 5, v, WHITE, 2);
    }
}

static void draw_skip(float cx, float cy, float sz, rgb_t c, bool fwd) {
    float s = fwd ? 1.f : -1.f;
    for (int k = 0; k < 2; k++) {
        float x = cx + s * (k == 0 ? -sz * 0.45f : sz * 0.05f);
        gfx_triangle(x, cy - sz / 2, x, cy + sz / 2, x + s * sz * 0.45f, cy, c);
    }
    float bx = cx + s * 0.5f * sz;
    gfx_fill_rect((int)(fwd ? bx : bx - 2), (int)(cy - sz / 2), 2, (int)sz, c);
}

static void draw_play_glyph(float cx, float cy, float sz, rgb_t c, bool paused_icon) {
    if (paused_icon) {
        int w = (int)(sz * 0.28f + 0.5f);
        gfx_fill_rect((int)(cx - sz * 0.42f), (int)(cy - sz / 2), w, (int)sz, c);
        gfx_fill_rect((int)(cx + sz * 0.42f) - w, (int)(cy - sz / 2), w, (int)sz, c);
    } else {
        gfx_triangle(cx - sz * 0.32f, cy - sz / 2, cx - sz * 0.32f, cy + sz / 2, cx + sz * 0.55f, cy, c);
    }
}

static void fmt_time(char *o, size_t n, uint32_t s, bool neg) { snprintf(o, n, "%s%lu:%02lu", neg ? "-" : "", (unsigned long)(s / 60), (unsigned long)(s % 60)); }


static uint32_t shown_pos_s = 0xFFFFFFFF;
static int hl_zone = POD_CMD_NONE;           // skip button being held (seeking)
static int64_t seek_preview_ms = -1;         // SD mode: position under the finger while dragging

static uint32_t current_pos_ms(void) {
    uint64_t p = cur.pos_ms;
    if (cur.playing && cur.len_ms) p += (time_us_64() - cur.pos_stamp_us) / 1000;
    if (cur.len_ms && p > cur.len_ms) p = cur.len_ms;
    return (uint32_t)p;
}

static void draw_controls(void) {
    gfx_fill_rect(0, CTRL_Y, GFX_W, GFX_H - CTRL_Y, base);
    rgb_t track = gfx_mix(base, WHITE, 80);
    gfx_capsule(BAR_X0, BAR_X1, BAR_Y, 2.0f, track);
    uint32_t pos = seek_preview_ms >= 0 ? (uint32_t)seek_preview_ms : current_pos_ms();
    shown_pos_s = pos / 1000;
    if (cur.len_ms) {
        int xp = BAR_X0 + (int)((uint64_t)(BAR_X1 - BAR_X0) * pos / cur.len_ms);
        if (xp > BAR_X0) gfx_capsule(BAR_X0, xp, BAR_Y, seek_preview_ms >= 0 ? 3.0f : 2.0f, WHITE);
        if (sd_mode) gfx_circle(xp, BAR_Y, seek_preview_ms >= 0 ? 7 : 4, WHITE);   // seekable: show a knob
        char a[12], b[12];
        fmt_time(a, sizeof a, pos / 1000, false);
        fmt_time(b, sizeof b, (cur.len_ms - pos + 999) / 1000, true);
        rgb_t tc = gfx_mix(WHITE, base, 40);
        gfx_text(&pod_font_small, BAR_X0, TIME_Y, a, tc, 0);
        gfx_text(&pod_font_small, BAR_X1, TIME_Y, b, tc, 2);
    }
    rgb_t ic = gfx_mix(WHITE, base, 16);
    if (hl_zone == POD_CMD_PREV) gfx_circle(72, BTN_Y, BTN_R, gfx_mix(base, WHITE, 70));
    if (hl_zone == POD_CMD_NEXT) gfx_circle(168, BTN_Y, BTN_R, gfx_mix(base, WHITE, 70));
    draw_skip(72, BTN_Y, 14, ic, false);
    draw_skip(168, BTN_Y, 14, ic, true);
    gfx_circle(120, BTN_Y, BTN_R, WHITE);
    draw_play_glyph(120, BTN_Y, 13, base, cur.playing);   // show "pause" while playing
}

static void draw_placeholder_art(void) {
    for (int y = 0; y < ART_H; y++)
        gfx_hline_dither(y, gfx_mix((rgb_t){70, 70, 84}, IDLE_BASE, y * 256 / ART_H));
    rgb_t n = {120, 120, 136};                         // eighth note
    gfx_fill_rect(128, 58, 7, 72, n);
    gfx_triangle(135, 58, 135, 80, 160, 84, n);
    gfx_circle(116, 128, 15, n);
}

static void render_now(void) {
    // 1. art (cover-cropped to a 240x240 square) or placeholder
    if (has_art) {
        int s = art_w < art_h ? art_w : art_h;
        const uint16_t *src = art_rgb + ((art_h - s) / 2) * art_w + (art_w - s) / 2;
        gfx_scale_image(src, s, s, art_w, 0, 0, GFX_W, ART_H);
    } else {
        draw_placeholder_art();
    }
    // 2. solid accent below the art, 3. fade the art into it
    gfx_fill_rect(0, ART_H, GFX_W, GFX_H - ART_H, base);
    for (int y = FADE_TOP; y < ART_H; y++) {
        int t = (y - FADE_TOP) * 256 / (ART_H - FADE_TOP);
        t = t * t * (768 - 2 * t) / 65536;              // smoothstep, 0..256
        for (int x = 0; x < GFX_W; x++) gfx_blend_px(x, y, base, t);
    }
    // 4. top scrim for the status bar
    for (int y = 0; y < SCRIM_H; y++)
        for (int x = 0; x < GFX_W; x++) gfx_blend_px(x, y, (rgb_t){0, 0, 0}, 128 * (SCRIM_H - y) / SCRIM_H);
    // 5. status bar (in SD mode the left side is a "< Library" button)
    draw_status_bar(cur.status[0] ? cur.status : "Pod", sd_mode);
    // 6. title + artist, centred with ellipsis
    char buf[TXT_LEN + 4];
    gfx_fit(&pod_font_title, cur.title[0] ? cur.title : "Not playing", TEXT_MAX_W, buf, sizeof buf);
    gfx_text(&pod_font_title, 120, TITLE_Y, buf, WHITE, 1);
    gfx_fit(&pod_font_body, cur.artist, TEXT_MAX_W, buf, sizeof buf);
    gfx_text(&pod_font_body, 120, ARTIST_Y, buf, gfx_mix(WHITE, base, 28), 1);
    // 7. progress + transport
    draw_controls();
}

// ============================================================ library list
static int list_scroll;                       // pixels scrolled down
static int list_pressed = -1;                 // row under the finger

static int list_max_scroll(void) {
    int m = list_n * ROW_H - (GFX_H - LIST_HDR_H);
    return m > 0 ? m : 0;
}

static void draw_folder_icon(int x, int y, rgb_t c) {
    gfx_fill_rect(x, y + 2, 8, 3, c);
    round_rect(x, y + 4, 18, 13, 2, c);
}

static void draw_note_icon(int x, int y, rgb_t c) {
    gfx_fill_rect(x + 9, y, 2, 13, c);
    gfx_triangle(x + 11, y, x + 11, y + 5, x + 17, y + 6, c);
    gfx_circle(x + 7, y + 13, 4, c);
}

static void render_list(void) {
    mutex_enter_blocking(&list_lock);
    if (list_scroll > list_max_scroll()) list_scroll = list_max_scroll();
    if (list_scroll < 0) list_scroll = 0;
    gfx_fill_rect(0, LIST_HDR_H, GFX_W, GFX_H - LIST_HDR_H, IDLE_BASE);
    rgb_t dim = {150, 150, 160}, line = {52, 52, 60};
    int first = list_scroll / ROW_H;
    for (int i = first; i < list_n; i++) {
        int y = LIST_HDR_H + i * ROW_H - list_scroll;
        if (y >= GFX_H) break;
        if (i == list_pressed || i == cur.list_highlight)
            gfx_fill_rect(0, y, GFX_W, ROW_H, i == list_pressed ? (rgb_t){70, 70, 82} : (rgb_t){48, 48, 58});
        if (list_folder[i]) draw_folder_icon(14, y + 11, dim);
        else draw_note_icon(14, y + 11, i == cur.list_highlight ? WHITE : dim);
        char buf[TXT_LEN + 4];
        gfx_fit(&pod_font_body, list_pool + list_off[i], 180, buf, sizeof buf);
        gfx_text(&pod_font_body, 44, y + 12, buf, WHITE, 0);
        if (list_folder[i]) draw_chevron_right(GFX_W - 20, y + 15, 10, dim);
        gfx_fill_rect(44, y + ROW_H - 1, GFX_W - 44, 1, line);
    }
    if (list_n == 0) {
        gfx_text(&pod_font_body, 120, 140, "No music here", dim, 1);
        gfx_text(&pod_font_small, 120, 162, "Copy MP3 or WAV files to the card", dim, 1);
    }
    // header drawn last so rows scroll underneath it
    gfx_fill_rect(0, 0, GFX_W, LIST_HDR_H, (rgb_t){26, 26, 31});
    gfx_fill_rect(0, LIST_HDR_H - 1, GFX_W, 1, line);
    char t[80];
    gfx_fit(&pod_font_title, list_title, 130, t, sizeof t);
    gfx_text(&pod_font_title, 120, 13, t, WHITE, 1);
    if (list_can_back) draw_chevron_left(10, 15, 14, WHITE);
    draw_house_icon(GFX_W - 30, 15, WHITE);   // Home, always
    if (cur.title[0])                         // something is loaded: shortcut back to it
        draw_play_glyph(GFX_W - 62, 22, 11, WHITE, false);
    mutex_exit(&list_lock);
}

// ================================================================== toast
static uint32_t toast_until_ms;
#define TOAST_Y 30
#define TOAST_H 28

static void draw_toast(void) {
    int w = gfx_text_width(&pod_font_body, cur.toast) + 28;
    if (w > GFX_W - 16) w = GFX_W - 16;
    int x = (GFX_W - w) / 2;
    round_rect(x, TOAST_Y, w, TOAST_H, 13, (rgb_t){20, 20, 24});
    round_rect(x + 1, TOAST_Y + 1, w - 2, TOAST_H - 2, 12, (rgb_t){58, 58, 66});
    gfx_text(&pod_font_body, GFX_W / 2, TOAST_Y + 7, cur.toast, WHITE, 1);
}

static void render_screen(void) {
    if (cur.screen == SCREEN_LIST) render_list(); else render_now();
    if (toast_until_ms) draw_toast();
}

// ============================================================ JPEG decode
typedef struct { const uint8_t *data; size_t len, pos; int w, h; } jctx_t;

static size_t j_in(JDEC *jd, uint8_t *buf, size_t n) {
    jctx_t *c = (jctx_t *)jd->device;
    if (c->pos + n > c->len) n = c->len - c->pos;
    if (buf) memcpy(buf, c->data + c->pos, n);
    c->pos += n;
    return n;
}

static int j_out(JDEC *jd, void *bitmap, JRECT *r) {
    jctx_t *c = (jctx_t *)jd->device;
    const uint16_t *px = (const uint16_t *)bitmap;
    int w = r->right - r->left + 1;
    for (int y = r->top; y <= r->bottom; y++)
        for (int x = r->left; x <= r->right; x++) {
            uint16_t v = px[(y - r->top) * w + (x - r->left)];
            if (x < c->w && y < c->h) art_rgb[y * c->w + x] = v;
        }
    return 1;
}

static uint8_t jd_pool[TJPGD_WORKSPACE_SIZE + 512] __attribute__((aligned(4)));

static bool decode_art(size_t len) {
    JDEC jd;
    memset(&jd, 0, sizeof jd);
    jctx_t c = { .data = jpeg_work, .len = len };
    JRESULT r = jd_prepare(&jd, j_in, jd_pool, sizeof jd_pool, &c);
    if (r != JDR_OK) {
        printf("Pod: JPEG header error %d%s\n", r, r == JDR_FMT3 ? " (progressive JPEG - unsupported)" : "");
        return false;
    }
    uint8_t s = 0;
    while (s < 3 && ((jd.width >> s) > ART_MAX || (jd.height >> s) > ART_MAX)) s++;
    c.w = jd.width >> s; c.h = jd.height >> s;
    if (c.w < 1 || c.h < 1) return false;
    r = jd_decomp(&jd, j_out, s);
    if (r != JDR_OK) { printf("Pod: JPEG decode error %d\n", r); return false; }
    art_w = c.w; art_h = c.h;

    // accent colour = average of the cover, darkened, luminance-capped so white text stays readable
    uint32_t sr = 0, sg = 0, sb = 0, n = (uint32_t)(art_w * art_h);
    for (uint32_t i = 0; i < n; i++) {
        uint16_t v = art_rgb[i];
        sr += (v >> 11) & 0x1F; sg += (v >> 5) & 0x3F; sb += v & 0x1F;
    }
    int R = (int)(sr * 255 / 31 / n) / 2, G = (int)(sg * 255 / 63 / n) / 2, B = (int)(sb * 255 / 31 / n) / 2;
    int L = (R * 77 + G * 150 + B * 29) >> 8;
    if (L > 80) { R = R * 80 / L; G = G * 80 / L; B = B * 80 / L; }
    base = (rgb_t){ (uint8_t)R, (uint8_t)G, (uint8_t)B };
    printf("Pod: album art %ux%u (%u bytes) decoded at 1/%d\n", jd.width, jd.height, (unsigned)len, 1 << s);
    return true;
}

// ========================================================= volume overlay
// Tap the album art to open a volume slider over it; drag to set the volume.
// It closes by itself after a few seconds without a touch.
#define OV_Y0      92
#define OV_H       72
#define OV_X0      24
#define OV_X1      216
#define OV_BAR_Y   (OV_Y0 + 46)
#define OV_TIMEOUT_MS 3000

static uint16_t ov_save[GFX_W * OV_H];          // the art under the overlay, restored on every redraw
static bool ov_visible;
static int ov_vol = 50;
static uint32_t ov_last_ms;

static void ov_capture(void) { memcpy(ov_save, gfx_fb + OV_Y0 * GFX_W, sizeof ov_save); }

static void ov_draw(void) {
    memcpy(gfx_fb + OV_Y0 * GFX_W, ov_save, sizeof ov_save);
    for (int y = 0; y < OV_H; y++) {             // dark band with soft top/bottom edges
        int a = 170;
        if (y < 10) a = a * y / 10;
        if (y > OV_H - 11) a = a * (OV_H - 1 - y) / 10;
        for (int x = 0; x < GFX_W; x++) gfx_blend_px(x, OV_Y0 + y, (rgb_t){0, 0, 0}, a);
    }
    char pct[8]; snprintf(pct, sizeof pct, "%d%%", ov_vol);
    gfx_text(&pod_font_body, OV_X0, OV_Y0 + 12, "Volume", WHITE, 0);
    gfx_text(&pod_font_body, OV_X1, OV_Y0 + 12, pct, WHITE, 2);
    gfx_capsule(OV_X0, OV_X1, OV_BAR_Y, 3.0f, (rgb_t){110, 110, 118});
    int xp = OV_X0 + (OV_X1 - OV_X0) * ov_vol / 100;
    if (xp > OV_X0) gfx_capsule(OV_X0, xp, OV_BAR_Y, 3.0f, WHITE);
    gfx_circle(xp, OV_BAR_Y, 8, WHITE);
}

static void redraw_all(void) {
    render_screen();
    if (ov_visible && cur.screen == SCREEN_NOW) { ov_capture(); ov_draw(); }
    blit_all();
}

static void ov_show(void) {
    ov_visible = true;
    ov_vol = cur.volume >= 0 ? cur.volume : 50;
    render_screen();
    ov_capture();
    ov_draw();
    blit_all();
}

static void ov_hide(void) {
    ov_visible = false;
    redraw_all();                                // also refreshes "Vol NN%" in the status bar
}

static int ov_vol_from_x(int x) {
    int v = (x - OV_X0) * 100 / (OV_X1 - OV_X0);
    return v < 0 ? 0 : v > 100 ? 100 : v;
}

// ================================================================ core 1
enum { K_NONE = 0, K_ZONE, K_ART, K_VOL_DRAG, K_SEEK, K_STATUS, K_HOME, K_LIST, K_LIST_BACK, K_LIST_NOW };
#define TOP_TAP_H 52                             // generous: the top edge is where resistive touch is least precise
#define HOLD_MS 450                              // press-and-hold on skip = fast-forward / rewind
#define TAP_SLOP 10                              // pixels a tap may wander before it becomes a scroll

static int zone_at(int x, int y) {
    if (y < BTN_Y - 26) return POD_CMD_NONE;
    if (x < 96) return POD_CMD_PREV;
    if (x <= 144) return POD_CMD_PLAYPAUSE;
    return POD_CMD_NEXT;
}

static int64_t seek_ms_from_x(int x) {
    if (x < BAR_X0) x = BAR_X0;
    if (x > BAR_X1) x = BAR_X1;
    return (int64_t)cur.len_ms * (x - BAR_X0) / (BAR_X1 - BAR_X0);
}

static void core1_main(void) {
    flash_safe_execute_core_init();          // core 0 may write BT pairing keys to flash
    uint32_t last_touch_ms = 0;
    bool down = false, seeking = false, moved = false;
    int kind = K_NONE, zone = POD_CMD_NONE;
    uint32_t press_ms = 0;
    int press_x = 0, press_y = 0, scroll_at_press = 0;

    while (true) {
        uint32_t dirty = 0;
        size_t art_len = 0;
        mutex_enter_blocking(&mbox_lock);
        if (mbox_dirty) {
            dirty = mbox_dirty;
            mbox_dirty = 0;
            memcpy(&cur, &mbox, sizeof cur);
            if (dirty & D_ART) { art_len = mbox.art_len; memcpy(jpeg_work, mbox_art, art_len); }
        }
        mutex_exit(&mbox_lock);
        uint32_t now = to_ms_since_boot(get_absolute_time());

        if (dirty & D_ART_CLEAR) { has_art = false; base = IDLE_BASE; }
        if (dirty & D_ART) {
            has_art = decode_art(art_len);
            if (!has_art) base = IDLE_BASE;
        }
        if (dirty & D_TOAST) toast_until_ms = now + 4000;
        if (dirty & D_SCREEN) {
            if (cur.screen != SCREEN_NOW) ov_visible = false;
            if (cur.screen == SCREEN_LIST && cur.list_highlight >= 0) {      // bring the playing row into view
                int y = cur.list_highlight * ROW_H;
                if (y < list_scroll || y + ROW_H > list_scroll + GFX_H - LIST_HDR_H)
                    list_scroll = y - (GFX_H - LIST_HDR_H) / 2;
            }
        }
        if (dirty & D_LIST) list_scroll = 0;

        bool full = dirty & (D_ART | D_ART_CLEAR | D_TEXT | D_STATUS | D_BATT | D_TOAST | D_SCREEN | D_LIST);
        if (full) {
            absolute_time_t t0 = get_absolute_time();
            redraw_all();
            if (dirty & D_ART) printf("Pod: frame rendered in %lld ms\n", absolute_time_diff_us(t0, get_absolute_time()) / 1000);
        } else if (dirty & D_VOL) {
            if (cur.screen == SCREEN_NOW) {
                if (ov_visible) {
                    if (kind != K_VOL_DRAG && cur.volume >= 0) { ov_vol = cur.volume; ov_draw(); blit_rows(OV_Y0, OV_H); }
                } else {
                    redraw_all();
                }
            }
        } else if (cur.screen == SCREEN_NOW && kind != K_SEEK &&
                   (dirty & (D_PLAY | D_PROGRESS) ||
                    (cur.playing && cur.len_ms && current_pos_ms() / 1000 != shown_pos_s))) {
            draw_controls();                      // tick the progress bar once a second
            blit_rows(CTRL_Y, GFX_H - CTRL_Y);
        }
        if (toast_until_ms && (int32_t)(now - toast_until_ms) >= 0) { toast_until_ms = 0; redraw_all(); }

        // ---- touch
        if (now - last_touch_ms >= 20) {
            last_touch_ms = now;
            xpt2046_raw_t r;
            int sx = 0, sy = 0;
            bool irq = xpt2046_irq_active();
            bool got = irq && xpt2046_read(&r) && map_touch(&r, &sx, &sy);
            bool was_down = down;
            if (got) down = true;
            else if (!irq) down = false;          // irq on but too light to read: keep previous state

            if (down && !was_down && got) {       // ---- finger lands
                press_ms = now; press_x = sx; press_y = sy;
                seeking = false; moved = false;
                if (cur.screen == SCREEN_LIST) {
                    if (sy < LIST_HDR_H) {
                        kind = sx >= GFX_W - 46 ? K_HOME
                             : (sx >= GFX_W - 90 && cur.title[0]) ? K_LIST_NOW
                             : (sx < 80 && list_can_back) ? K_LIST_BACK : K_NONE;
                    } else {
                        kind = K_LIST;
                        scroll_at_press = list_scroll;
                        list_pressed = (sy - LIST_HDR_H + list_scroll) / ROW_H;
                        if (list_pressed >= list_n) list_pressed = -1;
                        else redraw_all();
                    }
                } else if (ov_visible && sy >= OV_Y0 && sy < OV_Y0 + OV_H) {
                    kind = K_VOL_DRAG;
                } else if (sy < TOP_TAP_H && sx < (sd_mode ? 38 : STATUS_TAP_W)) {
                    kind = K_HOME;                // house icon (phone mode: the whole left side)
                } else if (sd_mode && sy < TOP_TAP_H && sx < 150) {
                    kind = K_STATUS;              // "< Library"
                } else if (sd_mode && cur.len_ms && sy >= BAR_Y - 14 && sy <= TIME_Y + 14) {
                    kind = K_SEEK;
                } else if (sy >= BTN_Y - 26) {
                    kind = K_ZONE; zone = zone_at(sx, sy);
                } else if (sy >= TOP_TAP_H && sy < TITLE_Y - 8) {
                    kind = K_ART;
                } else {
                    kind = K_NONE;
                }
            }

            if (down && got) {                    // ---- finger held / moving
                if (abs(sx - press_x) > TAP_SLOP || abs(sy - press_y) > TAP_SLOP) moved = true;
                if (kind == K_VOL_DRAG) {
                    int v = ov_vol_from_x(sx);
                    if (v != ov_vol) {
                        ov_vol = v;
                        mbox_vol_req = v; __dmb(); mbox_vol_seq++;
                        ov_draw(); blit_rows(OV_Y0, OV_H);
                    }
                    ov_last_ms = now;
                } else if (kind == K_SEEK) {
                    seek_preview_ms = seek_ms_from_x(sx);
                    draw_controls();
                    blit_rows(CTRL_Y, GFX_H - CTRL_Y);
                } else if (kind == K_LIST && moved) {
                    int ns = scroll_at_press - (sy - press_y);
                    if (ns < 0) ns = 0;
                    if (ns > list_max_scroll()) ns = list_max_scroll();
                    if (ns != list_scroll || list_pressed >= 0) {
                        list_scroll = ns; list_pressed = -1;
                        render_list(); blit_rows(LIST_HDR_H, GFX_H - LIST_HDR_H);
                    }
                } else if (kind == K_ZONE && !seeking && now - press_ms >= HOLD_MS &&
                           (zone == POD_CMD_PREV || zone == POD_CMD_NEXT) && zone_at(sx, sy) == zone) {
                    seeking = true;
                    push_cmd(zone == POD_CMD_NEXT ? POD_CMD_FF_START : POD_CMD_REW_START);
                    hl_zone = zone;
                    draw_controls();
                    blit_rows(CTRL_Y, GFX_H - CTRL_Y);
                }
            }

            if (!down && was_down) {              // ---- finger lifts
                if (kind == K_ZONE) {
                    if (seeking) {
                        push_cmd(POD_CMD_SEEK_STOP);
                        hl_zone = POD_CMD_NONE;
                        draw_controls();
                        blit_rows(CTRL_Y, GFX_H - CTRL_Y);
                    } else if (zone != POD_CMD_NONE) {
                        push_cmd((pod_cmd_t)zone);
                        if (zone == POD_CMD_PLAYPAUSE) {      // instant visual feedback
                            cur.playing = !cur.playing;
                            cur.pos_ms = current_pos_ms(); cur.pos_stamp_us = time_us_64();
                            draw_controls();
                            blit_rows(CTRL_Y, GFX_H - CTRL_Y);
                        }
                    }
                } else if (kind == K_SEEK) {
                    if (seek_preview_ms >= 0 && cur.len_ms) {
                        mbox_seek_req = (int)(seek_preview_ms * 1000 / cur.len_ms);
                        push_cmd(POD_CMD_SEEK);
                        cur.pos_ms = (uint32_t)seek_preview_ms; cur.pos_stamp_us = time_us_64();
                    }
                    seek_preview_ms = -1;
                    draw_controls();
                    blit_rows(CTRL_Y, GFX_H - CTRL_Y);
                } else if (kind == K_ART) {
                    if (ov_visible) ov_hide(); else { ov_show(); ov_last_ms = now; }
                } else if (kind == K_STATUS) {
                    push_cmd(POD_CMD_BACK);
                } else if (kind == K_HOME) {
                    push_cmd(POD_CMD_HOME);
                    strcpy(cur.toast, "Going home\xE2\x80\xA6");   // instant feedback while the Pod restarts
                    toast_until_ms = now + 3000;
                    redraw_all();
                } else if (kind == K_LIST) {
                    if (!moved && list_pressed >= 0) { mbox_list_req = list_pressed; push_cmd(POD_CMD_LIST_SELECT); }
                    list_pressed = -1;
                    redraw_all();
                } else if (kind == K_LIST_BACK) {
                    push_cmd(POD_CMD_BACK);
                } else if (kind == K_LIST_NOW) {
                    push_cmd(POD_CMD_NOW_PLAYING);
                }
                kind = K_NONE;
                seeking = false;
            }
        }
        if (ov_visible && !down && now - ov_last_ms > OV_TIMEOUT_MS) ov_hide();
        sleep_ms(5);
    }
}

// ============================================================== home screen
// Runs on core 0 before core 1 starts, so it can draw and read touch directly.
// Wallpaper (if installed) full-bleed, soft scrims, SH monogram, and two
// frosted-glass tiles that blur whatever is behind them.
#define HOME_TILE_Y   212
#define HOME_TILE_H   92
#define HOME_TILE_W   102
static const int home_tile_x[2] = { 14, 124 };
static const rgb_t GARNET = {196, 32, 72};
static const rgb_t BLAUGRANA_BLUE = {44, 96, 214};

static void draw_phone_icon(int cx, int cy, rgb_t c, rgb_t bg) {
    round_rect(cx - 6, cy - 10, 12, 20, 3, c);
    gfx_fill_rect(cx - 4, cy - 7, 8, 13, bg);
    gfx_fill_rect(cx - 2, cy + 7, 4, 1, bg);
}

static void draw_sd_icon(int cx, int cy, rgb_t c, rgb_t bg) {
    round_rect(cx - 7, cy - 9, 14, 18, 2, c);
    gfx_triangle(cx + 2, cy - 10, cx + 8, cy - 10, cx + 8, cy - 4, bg);         // clipped corner
    for (int i = 0; i < 3; i++) gfx_fill_rect(cx - 4 + i * 3, cy - 6, 2, 4, bg); // contacts
}

static void draw_logo(int x, int y, rgb_t c) {
    for (int j = 0; j < POD_LOGO_SIZE; j++)
        for (int i = 0; i < POD_LOGO_SIZE; i++) {
            int a = pod_logo_alpha[j * POD_LOGO_SIZE + i];
            if (a) gfx_blend_px(x + i, y + j, c, a);
        }
}

// Letter-spaced text (for small caps labels like "POD").
static void text_tracked(const pod_font_t *f, int x, int y, const char *s, rgb_t c, int track) {
    char ch[2] = {0, 0};
    for (; *s; s++) { ch[0] = *s; gfx_text(f, x, y, ch, c, 0); x += gfx_text_width(f, ch) + track; }
}

// Fallback art when no wallpaper is installed: deep garnet and blue light blooms.
static void draw_home_fallback(void) {
    for (int y = 0; y < GFX_H; y++)
        for (int x = 0; x < GFX_W; x++) {
            int d1 = (x - 40) * (x - 40) + (y - 60) * (y - 60);
            int d2 = (x - 210) * (x - 210) + (y - 190) * (y - 190);
            int a1 = 256 - d1 / 110; if (a1 < 0) a1 = 0;
            int a2 = 256 - d2 / 130; if (a2 < 0) a2 = 0;
            rgb_t c = {14, 14, 20};
            c = gfx_mix(c, GARNET, a1 * 3 / 4);
            c = gfx_mix(c, BLAUGRANA_BLUE, a2 * 3 / 5);
            static const uint8_t bayer[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
            int dz = bayer[y & 3][x & 3] / 2;                 // ordered dither hides 16-bit colour banding
            c = (rgb_t){ (uint8_t)(c.r + dz > 255 ? 255 : c.r + dz), (uint8_t)(c.g + dz / 2 > 255 ? 255 : c.g + dz / 2),
                         (uint8_t)(c.b + dz > 255 ? 255 : c.b + dz) };
            gfx_fb[y * GFX_W + x] = gfx_pack(c);
        }
}

// Separable box blur of an RGB block (in place), two passes = soft "frosted" look.
static void blur_rgb(rgb_t *p, int w, int h, int r) {
    rgb_t line[GFX_W > GFX_H ? GFX_W : GFX_H];
    for (int pass = 0; pass < 2; pass++) {
        for (int y = 0; y < h; y++) {                       // horizontal
            for (int x = 0; x < w; x++) line[x] = p[y * w + x];
            for (int x = 0; x < w; x++) {
                int R = 0, G = 0, B = 0, n = 0;
                for (int k = -r; k <= r; k++) { int xx = x + k; if (xx < 0) xx = 0; if (xx >= w) xx = w - 1; R += line[xx].r; G += line[xx].g; B += line[xx].b; n++; }
                p[y * w + x] = (rgb_t){ (uint8_t)(R / n), (uint8_t)(G / n), (uint8_t)(B / n) };
            }
        }
        for (int x = 0; x < w; x++) {                       // vertical
            for (int y = 0; y < h; y++) line[y] = p[y * w + x];
            for (int y = 0; y < h; y++) {
                int R = 0, G = 0, B = 0, n = 0;
                for (int k = -r; k <= r; k++) { int yy = y + k; if (yy < 0) yy = 0; if (yy >= h) yy = h - 1; R += line[yy].r; G += line[yy].g; B += line[yy].b; n++; }
                p[y * w + x] = (rgb_t){ (uint8_t)(R / n), (uint8_t)(G / n), (uint8_t)(B / n) };
            }
        }
    }
}

// Anti-aliased coverage of a rounded rectangle at pixel (i, j), 0..256.
static int rr_cover(int i, int j, int w, int h, int r) {
    float cx = i + 0.5f, cy = j + 0.5f, dx = 0, dy = 0;
    if (cx < r) dx = r - cx; else if (cx > w - r) dx = cx - (w - r);
    if (cy < r) dy = r - cy; else if (cy > h - r) dy = cy - (h - r);
    if (dx == 0 && dy == 0) return 256;
    float d = r - __builtin_sqrtf(dx * dx + dy * dy) + 0.5f;
    return d <= 0 ? 0 : d >= 1 ? 256 : (int)(d * 256);
}

// Frosted backdrop of one tile. Borrows the volume overlay's buffer, which is
// never in use while the home screen is up (saves 28 KB of RAM).
_Static_assert(sizeof(rgb_t) * HOME_TILE_W * HOME_TILE_H <= sizeof ov_save, "home tile scratch too big");
#define home_bg ((rgb_t *)ov_save)

static void draw_glass_tile(int t, bool pressed) {
    int x0 = home_tile_x[t], y0 = HOME_TILE_Y, w = HOME_TILE_W, h = HOME_TILE_H, r = 16;
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++) home_bg[j * w + i] = gfx_unpack(gfx_fb[(y0 + j) * GFX_W + x0 + i]);
    blur_rgb(home_bg, w, h, 7);
    rgb_t tint = pressed ? (rgb_t){255, 255, 255} : (rgb_t){210, 214, 228};
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++) {
            int cov = rr_cover(i, j, w, h, r);
            if (!cov) continue;
            rgb_t c = gfx_mix(home_bg[j * w + i], (rgb_t){0, 0, 0}, 70);    // darken a touch
            c = gfx_mix(c, tint, pressed ? 70 : 34);                          // milky glass
            int edge = rr_cover(i - 1, j - 1, w - 2, h - 2, r - 1);           // 1 px light rim
            if (edge < 256) c = gfx_mix(c, (rgb_t){255, 255, 255}, (256 - edge) * 90 / 256);
            if (j < h / 2) c = gfx_mix(c, (rgb_t){255, 255, 255}, (h / 2 - j) * 18 / (h / 2));   // top sheen
            gfx_blend_px(x0 + i, y0 + j, c, cov);
        }
    rgb_t accent = t == 0 ? GARNET : BLAUGRANA_BLUE;
    gfx_circle(x0 + 28, y0 + 28, 15, accent);
    if (t == 0) draw_phone_icon(x0 + 28, y0 + 28, WHITE, accent); else draw_sd_icon(x0 + 28, y0 + 28, WHITE, accent);
    draw_chevron_right(x0 + w - 22, y0 + 22, 11, (rgb_t){235, 235, 240});
    gfx_text(&pod_font_title, x0 + 13, y0 + 49, t == 0 ? "Phone" : "SD card", WHITE, 0);
    gfx_text(&pod_font_small, x0 + 14, y0 + 73, t == 0 ? "Bluetooth stream" : "Saved music", (rgb_t){220, 222, 232}, 0);
}

static void render_home(int pressed) {
    // 1. wallpaper, or generated art
    const uint16_t *wp = pod_wallpaper();
    if (wp) memcpy(gfx_fb, wp, sizeof gfx_fb);
    else draw_home_fallback();
    // 2. scrims: top for the logo, bottom for the tiles (smoothstep)
    for (int y = 0; y < 76; y++) {
        int a = 150 * (76 - y) / 76;
        for (int x = 0; x < GFX_W; x++) gfx_blend_px(x, y, (rgb_t){0, 0, 0}, a);
    }
    for (int y = 172; y < GFX_H; y++) {
        int t = (y - 172) * 256 / (GFX_H - 172);
        t = t * t * (768 - 2 * t) / 65536;
        for (int x = 0; x < GFX_W; x++) gfx_blend_px(x, y, (rgb_t){6, 6, 10}, t * 215 / 256);
    }
    // 3. brand: SH monogram + letter-spaced wordmark
    draw_logo(14, 12, WHITE);
    text_tracked(&pod_font_status, 62, 18, "POD", WHITE, 3);
    gfx_text(&pod_font_small, 62, 32, "Music, your way", (rgb_t){200, 200, 210}, 0);
    draw_battery(GFX_W - 12, 16);
    // 4. tiles
    text_tracked(&pod_font_small, 16, 192, "CHOOSE A SOURCE", (rgb_t){215, 215, 225}, 2);
    draw_glass_tile(0, pressed == 0);
    draw_glass_tile(1, pressed == 1);
}

pod_source_t pod_display_home(void) {
    cur.battery = mbox.battery;
    cur.charging = mbox.charging;
    render_home(-1);
    blit_all();
    int pressed = -1;
    int dbg = 0;
    while (true) {
        xpt2046_raw_t r;
        int sx, sy;
        // Debug readout (bottom of the home screen): raw touch numbers and the mapped position.
        if (++dbg % 6 == 0) {
            xpt2046_raw_t d; bool p = xpt2046_sample_raw(&d);
            int mx = -1, my = -1; if (p) map_touch(&d, &mx, &my);
            char b[32];
            snprintf(b, sizeof b, "X%4u Y%4u %s", d.x, d.y, p ? "ON" : "--");
            raw_line(GFX_H - 36, b, p ? RGB565(40, 220, 90) : RGB565(150, 150, 150));
            snprintf(b, sizeof b, "Z%4d %4d>%3d,%3d", xpt2046_last_z1, xpt2046_last_z2, mx, my);
            raw_line(GFX_H - 18, b, RGB565(150, 150, 150));
        }
        if (xpt2046_irq_active() && xpt2046_read(&r) && map_touch(&r, &sx, &sy)) {
            int p = -1;
            if (sy >= HOME_TILE_Y - 6 && sy < HOME_TILE_Y + HOME_TILE_H + 6)
                for (int t = 0; t < 2; t++)
                    if (sx >= home_tile_x[t] - 4 && sx < home_tile_x[t] + HOME_TILE_W + 4) p = t;
            if (p != pressed) { pressed = p; render_home(pressed); blit_all(); }
        } else if (!xpt2046_irq_active() && pressed >= 0) {
            pod_source_t src = pressed == 0 ? POD_SOURCE_PHONE : POD_SOURCE_SD;
            printf("Pod: home -> %s\n", src == POD_SOURCE_PHONE ? "Phone" : "SD card");
            return src;
        }
        sleep_ms(15);
    }
}

// ============================================================== core 0 API
void pod_display_boot(void) {
    mutex_init(&mbox_lock);
    mutex_init(&list_lock);
    ili9341_init();
    xpt2046_init();
    bool have = cal_load();

    // Finger held on the screen for 1.5 s at power-up = recalibrate. (A quick tap while the
    // Pod restarts after "Home" must not trigger it.)
    bool force = xpt2046_irq_active();
    for (int i = 0; force && i < 150; i++) { sleep_ms(10); force = xpt2046_irq_active(); }
    if (!have || force) {
        for (int i = 0; xpt2046_irq_active() && i < 300; i++) sleep_ms(10);
        sleep_ms(200);
        calibrate();
        cal_save();
    } else {
        printf("Pod: touch calibration loaded (hold the screen while powering up to redo it)\n");
    }
    mbox.volume = -1;
    mbox.battery = -1;
    mbox.list_highlight = -1;
    mbox.screen = SCREEN_NOW;
    strcpy(mbox.status, "Pod");
    strcpy(mbox.title, "Waiting for phone");
    strcpy(mbox.artist, "Pair \xE2\x80\x9CPod\xE2\x80\x9D in Bluetooth settings");
    memcpy(&cur, &mbox, sizeof cur);
}

void pod_display_set_sd_mode(bool on) {
    sd_mode = on;
    if (on) {
        strcpy(mbox.status, "Library");
        strcpy(mbox.title, "SD card");
        strcpy(mbox.artist, "Reading your music\xE2\x80\xA6");
    }
}

void pod_display_start(void) {
    memcpy(&cur, &mbox, sizeof cur);
    render_screen();
    blit_all();
    multicore_launch_core1(core1_main);
}

#define SET(flag, body) do { mutex_enter_blocking(&mbox_lock); body; mbox_dirty |= (flag); mutex_exit(&mbox_lock); } while (0)

static void set_str(char *dst, const uint8_t *u, size_t n) {
    if (n > TXT_LEN - 1) {                      // truncate on a UTF-8 character boundary
        n = TXT_LEN - 1;
        while (n > 0 && (u[n] & 0xC0) == 0x80) n--;
    }
    memcpy(dst, u, n);
    dst[n] = 0;
}

void pod_display_set_status(const char *s) {
    SET(D_STATUS, { strncpy(mbox.status, s, sizeof mbox.status - 1); mbox.status[sizeof mbox.status - 1] = 0; });
}
void pod_display_set_title(const uint8_t *u, size_t n)  { SET(D_TEXT, set_str(mbox.title, u, n)); }
void pod_display_set_artist(const uint8_t *u, size_t n) { SET(D_TEXT, set_str(mbox.artist, u, n)); }
void pod_display_set_album(const uint8_t *u, size_t n)  { SET(0,      set_str(mbox.album, u, n)); }
void pod_display_set_volume(int pct) { SET(D_VOL, mbox.volume = pct); }

void pod_display_set_playing(bool p) {
    SET(D_PLAY, {
        uint64_t now = time_us_64();
        if (mbox.playing && mbox.len_ms) mbox.pos_ms += (uint32_t)((now - mbox.pos_stamp_us) / 1000);
        mbox.pos_stamp_us = now;
        mbox.playing = p;
    });
}

void pod_display_set_progress(uint32_t pos_ms, uint32_t len_ms, bool playing) {
    SET(D_PROGRESS, { mbox.pos_ms = pos_ms; mbox.len_ms = len_ms; mbox.playing = playing; mbox.pos_stamp_us = time_us_64(); });
}

void pod_display_clear_art(void) { SET(D_ART_CLEAR, { mbox_dirty &= ~(uint32_t)D_ART; }); }

void pod_display_clear_track(void) {
    SET(D_ART_CLEAR | D_TEXT | D_PLAY, {
        mbox_dirty &= ~(uint32_t)D_ART;
        strcpy(mbox.title, "Waiting for phone");
        strcpy(mbox.artist, "Pair \xE2\x80\x9CPod\xE2\x80\x9D in Bluetooth settings");
        mbox.album[0] = 0; mbox.playing = false; mbox.len_ms = 0; mbox.pos_ms = 0;
    });
}

void pod_display_set_art(const uint8_t *jpeg, size_t len) {
    if (len == 0 || len > POD_ART_MAX_BYTES) return;
    SET(D_ART, { memcpy(mbox_art, jpeg, len); mbox.art_len = len; });
}

uint8_t *pod_display_art_begin(size_t *cap) {
    mutex_enter_blocking(&mbox_lock);
    *cap = POD_ART_MAX_BYTES;
    return mbox_art;
}

void pod_display_art_commit(size_t len) {
    if (len > 0 && len <= POD_ART_MAX_BYTES) {
        mbox.art_len = len;
        mbox_dirty = (mbox_dirty & ~(uint32_t)D_ART_CLEAR) | D_ART;
    } else {
        mbox_dirty = (mbox_dirty & ~(uint32_t)D_ART) | D_ART_CLEAR;
    }
    mutex_exit(&mbox_lock);
}

void pod_display_set_battery(int pct, bool charging) {
    mutex_enter_blocking(&mbox_lock);
    if (mbox.battery != pct || mbox.charging != charging) {
        mbox.battery = pct; mbox.charging = charging;
        mbox_dirty |= D_BATT;
    }
    mutex_exit(&mbox_lock);
}

void pod_display_toast(const char *s) {
    SET(D_TOAST, { strncpy(mbox.toast, s, sizeof mbox.toast - 1); mbox.toast[sizeof mbox.toast - 1] = 0; });
}

// ---- library list
void pod_display_list_begin(const char *title, bool can_go_back) {
    mutex_enter_blocking(&list_lock);
    list_n = 0; list_used = 0;
    strncpy(list_title, title, sizeof list_title - 1);
    list_title[sizeof list_title - 1] = 0;
    list_can_back = can_go_back;
    mutex_exit(&list_lock);
}

bool pod_display_list_add(const char *name, bool is_folder) {
    size_t n = strlen(name) + 1;
    mutex_enter_blocking(&list_lock);
    bool ok = list_n < POD_LIST_MAX && list_used + n <= POD_LIST_POOL;
    if (ok) {
        memcpy(list_pool + list_used, name, n);
        list_off[list_n] = (uint16_t)list_used;
        list_folder[list_n] = is_folder;
        list_used += (int)n;
        list_n++;
    }
    mutex_exit(&list_lock);
    return ok;
}

void pod_display_list_sort(void) {
    mutex_enter_blocking(&list_lock);
    for (int i = 1; i < list_n; i++) {          // insertion sort: lists are small and mostly in order
        uint16_t off = list_off[i];
        uint8_t fol = list_folder[i];
        int j = i - 1;
        while (j >= 0 && (list_folder[j] < fol ||
               (list_folder[j] == fol && strcasecmp(list_pool + list_off[j], list_pool + off) > 0))) {
            list_off[j + 1] = list_off[j];
            list_folder[j + 1] = list_folder[j];
            j--;
        }
        list_off[j + 1] = off;
        list_folder[j + 1] = fol;
    }
    mutex_exit(&list_lock);
}

void pod_display_list_end(int highlight) {
    SET(D_LIST | D_SCREEN, { mbox.screen = SCREEN_LIST; mbox.list_highlight = highlight; });
}

int pod_display_list_count(void) { return list_n; }

bool pod_display_list_get(int i, char *out, size_t cap, bool *is_folder) {
    mutex_enter_blocking(&list_lock);
    bool ok = i >= 0 && i < list_n;
    if (ok) {
        strncpy(out, list_pool + list_off[i], cap - 1);
        out[cap - 1] = 0;
        if (is_folder) *is_folder = list_folder[i];
    }
    mutex_exit(&list_lock);
    return ok;
}

void pod_display_show_list(int highlight) {
    SET(D_SCREEN, { mbox.screen = SCREEN_LIST; mbox.list_highlight = highlight; });
}

void pod_display_show_now_playing(void) { SET(D_SCREEN, { mbox.screen = SCREEN_NOW; }); }

// ---- input
pod_cmd_t pod_display_take_command(void) {
    uint32_t r = cmdq_r;
    if (r == cmdq_w) return POD_CMD_NONE;
    pod_cmd_t c = (pod_cmd_t)cmdq[r % CMDQ_LEN];
    __dmb();
    cmdq_r = r + 1;
    return c;
}

int pod_display_take_volume(void) {
    static uint32_t applied_seq;
    uint32_t seq = mbox_vol_seq;
    if (seq == applied_seq) return -1;
    __dmb();
    applied_seq = seq;
    return mbox_vol_req;
}

int pod_display_take_seek(void) { return mbox_seek_req; }
int pod_display_take_list_index(void) { return mbox_list_req; }
