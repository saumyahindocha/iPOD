#include "pod_display.h"

#include <stdio.h>
#include <string.h>

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

static const rgb_t WHITE = {255, 255, 255};
static const rgb_t IDLE_BASE = {34, 34, 40};

#define TXT_LEN 160

// ================================================================== mailbox
enum {
    D_STATUS = 1u << 0, D_TEXT = 1u << 1, D_PLAY = 1u << 2, D_VOL = 1u << 3,
    D_ART = 1u << 4, D_ART_CLEAR = 1u << 5, D_PROGRESS = 1u << 6,
};

typedef struct {
    char status[40];
    char title[TXT_LEN], artist[TXT_LEN], album[TXT_LEN];
    bool playing;
    int volume;
    uint32_t pos_ms, len_ms;
    uint64_t pos_stamp_us;
    size_t art_len;
} pod_state_t;

static mutex_t mbox_lock;
static uint32_t mbox_dirty;
static pod_state_t mbox;
static uint8_t mbox_art[POD_ART_MAX_BYTES];

// Touch -> core 0 command queue (single producer: core 1, single consumer: core 0)
#define CMDQ_LEN 8
static volatile uint8_t cmdq[CMDQ_LEN];
static volatile uint32_t cmdq_w, cmdq_r;
static volatile int mbox_vol_req = -1;       // latest volume set on the Pod's slider
static volatile uint32_t mbox_vol_seq;       // bumped on every slider change (core 0 tracks what it applied)

static void push_cmd(pod_cmd_t c) {
    uint32_t w = cmdq_w;
    if (w - cmdq_r >= CMDQ_LEN) return;     // full: drop (only happens if core 0 stalls)
    cmdq[w % CMDQ_LEN] = (uint8_t)c;
    __dmb();
    cmdq_w = w + 1;
}

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

#define CAL_MAGIC  0x506F6443u                       // "PodC"
#define CAL_OFFSET (PICO_FLASH_SIZE_BYTES - 8 * FLASH_SECTOR_SIZE)   // well clear of BTstack's keys
static touch_cal_t cal;

static uint32_t cal_sum(const touch_cal_t *c) {
    return c->magic ^ (uint32_t)c->swap_xy * 0x9E3779B9u ^ (uint32_t)c->ax_l ^ ((uint32_t)c->ax_r << 8) ^
           ((uint32_t)c->ay_t << 16) ^ ((uint32_t)c->ay_b << 24) ^ 0xA5A5A5A5u;
}

static bool cal_load(void) {
    const touch_cal_t *f = (const touch_cal_t *)(XIP_BASE + CAL_OFFSET);
    if (f->magic != CAL_MAGIC || f->check != cal_sum(f) || f->ax_l == f->ax_r || f->ay_t == f->ay_b) return false;
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

static xpt2046_raw_t wait_tap(void) {
    while (true) {
        uint32_t sx = 0, sy = 0; int n = 0;
        xpt2046_raw_t r;
        while (!xpt2046_irq_active()) sleep_ms(5);
        sleep_ms(30);
        while (xpt2046_irq_active()) {
            if (n < 64 && xpt2046_read(&r)) { sx += r.x; sy += r.y; n++; }
            sleep_ms(10);
        }
        sleep_ms(150);
        if (n >= 3) return (xpt2046_raw_t){ (uint16_t)(sx / n), (uint16_t)(sy / n), 0 };
    }
}

static void calibrate(void) {
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

static uint32_t current_pos_ms(void) {
    uint64_t p = cur.pos_ms;
    if (cur.playing && cur.len_ms) p += (time_us_64() - cur.pos_stamp_us) / 1000;
    if (cur.len_ms && p > cur.len_ms) p = cur.len_ms;
    return (uint32_t)p;
}

static int hl_zone = POD_CMD_NONE;           // skip button being held (seeking)

static void draw_controls(void) {
    gfx_fill_rect(0, CTRL_Y, GFX_W, GFX_H - CTRL_Y, base);
    rgb_t track = gfx_mix(base, WHITE, 80);
    gfx_capsule(BAR_X0, BAR_X1, BAR_Y, 2.0f, track);
    uint32_t pos = current_pos_ms();
    shown_pos_s = pos / 1000;
    if (cur.len_ms) {
        int xp = BAR_X0 + (int)((uint64_t)(BAR_X1 - BAR_X0) * pos / cur.len_ms);
        if (xp > BAR_X0) gfx_capsule(BAR_X0, xp, BAR_Y, 2.0f, WHITE);
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

static void render_full(void) {
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
    // 5. status bar
    gfx_text(&pod_font_status, 10, 5, cur.status[0] ? cur.status : "Pod", WHITE, 0);
    if (cur.volume >= 0) {
        char v[16]; snprintf(v, sizeof v, "Vol %d%%", cur.volume);
        gfx_text(&pod_font_status, GFX_W - 10, 5, v, WHITE, 2);
    }
    // 6. title + artist, centred with ellipsis
    char buf[TXT_LEN + 4];
    gfx_fit(&pod_font_title, cur.title[0] ? cur.title : "Not playing", TEXT_MAX_W, buf, sizeof buf);
    gfx_text(&pod_font_title, 120, TITLE_Y, buf, WHITE, 1);
    gfx_fit(&pod_font_body, cur.artist, TEXT_MAX_W, buf, sizeof buf);
    gfx_text(&pod_font_body, 120, ARTIST_Y, buf, gfx_mix(WHITE, base, 28), 1);
    // 7. progress + transport
    draw_controls();
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

static void ov_show(void) {
    render_full();
    ov_capture();
    ov_vol = cur.volume >= 0 ? cur.volume : 50;
    ov_visible = true;
    ov_draw();
    blit_all();
}

static void ov_hide(void) {
    ov_visible = false;
    render_full();                               // also refreshes "Vol NN%" in the status bar
    blit_all();
}

static int ov_vol_from_x(int x) {
    int v = (x - OV_X0) * 100 / (OV_X1 - OV_X0);
    return v < 0 ? 0 : v > 100 ? 100 : v;
}

// ================================================================ core 1
enum { K_NONE = 0, K_ZONE, K_ART, K_VOL_DRAG };
#define HOLD_MS 450                              // press-and-hold on skip = fast-forward / rewind

static int zone_at(int x, int y) {
    if (y < BTN_Y - 26) return POD_CMD_NONE;
    if (x < 96) return POD_CMD_PREV;
    if (x <= 144) return POD_CMD_PLAYPAUSE;
    return POD_CMD_NEXT;
}

static void core1_main(void) {
    flash_safe_execute_core_init();          // core 0 may write BT pairing keys to flash
    uint32_t last_touch_ms = 0;
    bool down = false, seeking = false;
    int kind = K_NONE, zone = POD_CMD_NONE;
    uint32_t press_ms = 0;

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

        if (dirty & D_ART_CLEAR) { has_art = false; base = IDLE_BASE; }
        if (dirty & D_ART) {
            has_art = decode_art(art_len);
            if (!has_art) base = IDLE_BASE;
        }
        if (dirty & (D_ART | D_ART_CLEAR | D_TEXT | D_STATUS)) {
            absolute_time_t t0 = get_absolute_time();
            render_full();
            if (ov_visible) { ov_capture(); ov_draw(); }
            blit_all();
            if (dirty & D_ART) printf("Pod: frame rendered in %lld ms\n", absolute_time_diff_us(t0, get_absolute_time()) / 1000);
        } else if (dirty & D_VOL) {
            if (ov_visible) {
                if (kind != K_VOL_DRAG && cur.volume >= 0) { ov_vol = cur.volume; ov_draw(); blit_rows(OV_Y0, OV_H); }
            } else {
                render_full();
                blit_all();
            }
        }
        if (!(dirty & (D_ART | D_ART_CLEAR | D_TEXT | D_STATUS))) {
            if (dirty & (D_PLAY | D_PROGRESS) ||
                (cur.playing && cur.len_ms && current_pos_ms() / 1000 != shown_pos_s)) {
                draw_controls();                  // tick the progress bar once a second
                blit_rows(CTRL_Y, GFX_H - CTRL_Y);
            }
        }

        // ---- touch
        uint32_t now = to_ms_since_boot(get_absolute_time());
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
                press_ms = now;
                seeking = false;
                if (ov_visible && sy >= OV_Y0 && sy < OV_Y0 + OV_H) {
                    kind = K_VOL_DRAG;
                } else if (sy >= BTN_Y - 26) {
                    kind = K_ZONE; zone = zone_at(sx, sy);
                } else if (sy >= SCRIM_H && sy < TITLE_Y - 8) {
                    kind = K_ART;
                } else {
                    kind = K_NONE;
                }
            }

            if (down && got) {                    // ---- finger held / moving
                if (kind == K_VOL_DRAG) {
                    int v = ov_vol_from_x(sx);
                    if (v != ov_vol) {
                        ov_vol = v;
                        mbox_vol_req = v; __dmb(); mbox_vol_seq++;
                        ov_draw(); blit_rows(OV_Y0, OV_H);
                    }
                    ov_last_ms = now;
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
                } else if (kind == K_ART) {
                    if (ov_visible) ov_hide(); else { ov_show(); ov_last_ms = now; }
                }
                kind = K_NONE;
                seeking = false;
            }
        }
        if (ov_visible && !down && now - ov_last_ms > OV_TIMEOUT_MS) ov_hide();
        sleep_ms(5);
    }
}

// ============================================================== core 0 API
void pod_display_boot(void) {
    mutex_init(&mbox_lock);
    ili9341_init();
    xpt2046_init();
    bool have = cal_load();
    bool force = xpt2046_irq_active();       // finger on the screen at power-up = recalibrate
    if (!have || force) {
        if (force) { while (xpt2046_irq_active()) sleep_ms(10); sleep_ms(200); }
        calibrate();
        cal_save();
    } else {
        printf("Pod: touch calibration loaded (hold the screen while powering up to redo it)\n");
    }
    mbox.volume = -1;
    strcpy(mbox.status, "Pod");
    strcpy(mbox.title, "Waiting for phone");
    strcpy(mbox.artist, "Pair \xE2\x80\x9CPod\xE2\x80\x9D in Bluetooth settings");
    memcpy(&cur, &mbox, sizeof cur);
    render_full();
    blit_all();
}

void pod_display_start(void) { multicore_launch_core1(core1_main); }

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
