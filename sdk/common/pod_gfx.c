#include "pod_gfx.h"
#include <math.h>
#include <string.h>

uint16_t gfx_fb[GFX_W * GFX_H] __attribute__((aligned(4)));

void gfx_fill_rect(int x, int y, int w, int h, rgb_t c) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > GFX_W) w = GFX_W - x;
    if (y + h > GFX_H) h = GFX_H - y;
    if (w <= 0 || h <= 0) return;
    uint16_t v = gfx_pack(c);
    for (int j = y; j < y + h; j++) {
        uint16_t *p = &gfx_fb[j * GFX_W + x];
        for (int i = 0; i < w; i++) p[i] = v;
    }
}

// 4x4 ordered dither: hides RGB565 banding in gradients, fades and scrims.
static const uint8_t bayer4[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};

static inline uint16_t pack_dither(rgb_t c, int x, int y) {
    int d = bayer4[y & 3][x & 3];                     // 0..15
    int r = c.r + (d >> 1), g = c.g + (d >> 2), b = c.b + (d >> 1);   // < 1 LSB of 5/6/5 bits
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    return gfx_pack((rgb_t){(uint8_t)r, (uint8_t)g, (uint8_t)b});
}

void gfx_hline_dither(int y, rgb_t c) {
    if ((unsigned)y >= GFX_H) return;
    for (int x = 0; x < GFX_W; x++) gfx_fb[y * GFX_W + x] = pack_dither(c, x, y);
}

void gfx_blend_px(int x, int y, rgb_t c, int a) {
    if ((unsigned)x >= GFX_W || (unsigned)y >= GFX_H || a <= 0) return;
    uint16_t *p = &gfx_fb[y * GFX_W + x];
    if (a >= 256) { *p = gfx_pack(c); return; }
    *p = pack_dither(gfx_mix(gfx_unpack(*p), c, a), x, y);
}

static inline int cov(float d) {           // signed distance (px, negative inside) -> alpha 0..256
    float a = 0.5f - d;
    if (a <= 0) return 0;
    if (a >= 1) return 256;
    return (int)(a * 256);
}

void gfx_capsule(int x0, int x1, int yc, float r, rgb_t c) {
    int ylo = (int)floorf(yc - r - 1), yhi = (int)ceilf(yc + r + 1);
    int xlo = (int)floorf(x0 - r - 1), xhi = (int)ceilf(x1 + r + 1);
    for (int y = ylo; y <= yhi; y++)
        for (int x = xlo; x <= xhi; x++) {
            float px = x + 0.5f, py = y + 0.5f;
            float qx = px < x0 ? x0 : (px > x1 ? x1 : px);
            float d = sqrtf((px - qx) * (px - qx) + (py - yc) * (py - yc)) - r;
            gfx_blend_px(x, y, c, cov(d));
        }
}

void gfx_circle(float cx, float cy, float r, rgb_t c) {
    for (int y = (int)(cy - r - 1); y <= (int)(cy + r + 1); y++)
        for (int x = (int)(cx - r - 1); x <= (int)(cx + r + 1); x++) {
            float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
            gfx_blend_px(x, y, c, cov(sqrtf(dx * dx + dy * dy) - r));
        }
}

static float edge(float ax, float ay, float bx, float by, float px, float py) {
    return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
}

void gfx_triangle(float x0, float y0, float x1, float y1, float x2, float y2, rgb_t c) {
    if (edge(x0, y0, x1, y1, x2, y2) < 0) { float t = x1; x1 = x2; x2 = t; t = y1; y1 = y2; y2 = t; }
    int xl = (int)floorf(fminf(x0, fminf(x1, x2))), xr = (int)ceilf(fmaxf(x0, fmaxf(x1, x2)));
    int yt = (int)floorf(fminf(y0, fminf(y1, y2))), yb = (int)ceilf(fmaxf(y0, fmaxf(y1, y2)));
    for (int y = yt; y <= yb; y++)
        for (int x = xl; x <= xr; x++) {
            int n = 0;   // 4x4 supersampling
            for (int sy = 0; sy < 4; sy++)
                for (int sx = 0; sx < 4; sx++) {
                    float px = x + (sx + 0.5f) / 4, py = y + (sy + 0.5f) / 4;
                    if (edge(x0, y0, x1, y1, px, py) >= 0 && edge(x1, y1, x2, y2, px, py) >= 0 &&
                        edge(x2, y2, x0, y0, px, py) >= 0) n++;
                }
            gfx_blend_px(x, y, c, n * 16);
        }
}

// ---------------------------------------------------------------- text
static uint32_t next_cp(const char **s) {
    const uint8_t *p = (const uint8_t *)*s;
    uint32_t c = *p;
    int n = c < 0x80 ? 1 : (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : (c & 0xF8) == 0xF0 ? 4 : 1;
    uint32_t cp = n == 1 ? c : n == 2 ? (c & 0x1F) : n == 3 ? (c & 0x0F) : (c & 0x07);
    for (int i = 1; i < n; i++) {
        if ((p[i] & 0xC0) != 0x80) { *s += i; return '?'; }
        cp = (cp << 6) | (p[i] & 0x3F);
    }
    *s += n;
    return cp;
}

static const pod_glyph_t *find(const pod_font_t *f, uint32_t cp) {
    int lo = 0, hi = f->count - 1;
    while (lo <= hi) {
        int m = (lo + hi) / 2;
        if (f->glyphs[m].cp == cp) return &f->glyphs[m];
        if (f->glyphs[m].cp < cp) lo = m + 1; else hi = m - 1;
    }
    return cp == '?' ? NULL : find(f, '?');   // unsupported (e.g. Devanagari) -> '?'
}

int gfx_text_width(const pod_font_t *f, const char *s) {
    int w16 = 0;
    while (*s) { const pod_glyph_t *g = find(f, next_cp(&s)); if (g) w16 += g->adv16; }
    return (w16 + 8) / 16;
}

void gfx_text(const pod_font_t *f, int x, int y, const char *s, rgb_t c, int align) {
    if (align) x -= gfx_text_width(f, s) / (align == 1 ? 2 : 1);
    int pen16 = x * 16;
    while (*s) {
        const pod_glyph_t *g = find(f, next_cp(&s));
        if (!g) continue;
        int gx = (pen16 + 8) / 16 + g->xoff, gy = y + g->yoff;
        const uint8_t *bm = f->bitmap + g->off;
        int stride = (g->w + 1) / 2;
        for (int j = 0; j < g->h; j++)
            for (int i = 0; i < g->w; i++) {
                uint8_t b = bm[j * stride + i / 2];
                int a = (i & 1) ? (b & 0x0F) : (b >> 4);
                if (a) gfx_blend_px(gx + i, gy + j, c, a * 256 / 15);
            }
        pen16 += g->adv16;
    }
}

void gfx_fit(const pod_font_t *f, const char *s, int max_w, char *out, int cap) {
    int n = (int)strlen(s);
    if (n >= cap) n = cap - 1;
    memcpy(out, s, (size_t)n); out[n] = 0;
    if (gfx_text_width(f, out) <= max_w) return;
    static const char ell[] = "\xE2\x80\xA6";          // U+2026 ellipsis
    while (n > 0) {
        do { n--; } while (n > 0 && (((uint8_t)out[n]) & 0xC0) == 0x80);   // step back one UTF-8 char
        while (n > 0 && out[n - 1] == ' ') n--;
        if (n + 4 > cap) continue;
        memcpy(out + n, ell, 4);
        if (gfx_text_width(f, out) <= max_w) return;
    }
    memcpy(out, ell, 4);
}

// ---------------------------------------------------------------- image
void gfx_scale_image(const uint16_t *src, int sw, int sh, int stride, int dx, int dy, int dw, int dh) {
    for (int y = 0; y < dh; y++) {
        int fy = ((2 * y + 1) * sh * 128) / dh - 128;          // 8.8 fixed source y (pixel centres)
        if (fy < 0) fy = 0;
        int y0 = fy >> 8, ty = fy & 0xFF, y1 = y0 + 1 < sh ? y0 + 1 : sh - 1;
        for (int x = 0; x < dw; x++) {
            int fx = ((2 * x + 1) * sw * 128) / dw - 128;
            if (fx < 0) fx = 0;
            int x0 = fx >> 8, tx = fx & 0xFF, x1 = x0 + 1 < sw ? x0 + 1 : sw - 1;
            uint16_t p[4] = { src[y0 * stride + x0], src[y0 * stride + x1], src[y1 * stride + x0], src[y1 * stride + x1] };
            int ch[3];
            for (int k = 0; k < 3; k++) {
                int sh_ = k == 0 ? 11 : k == 1 ? 5 : 0, m = k == 1 ? 0x3F : 0x1F;
                int a = (p[0] >> sh_) & m, b = (p[1] >> sh_) & m, c = (p[2] >> sh_) & m, d = (p[3] >> sh_) & m;
                int top = a * (256 - tx) + b * tx, bot = c * (256 - tx) + d * tx;
                ch[k] = (top * (256 - ty) + bot * ty + 32768) >> 16;
            }
            uint16_t v = (uint16_t)((ch[0] << 11) | (ch[1] << 5) | ch[2]);
            int X = dx + x, Y = dy + y;
            if ((unsigned)X < GFX_W && (unsigned)Y < GFX_H) gfx_fb[Y * GFX_W + X] = (uint16_t)((v >> 8) | (v << 8));
        }
    }
}
