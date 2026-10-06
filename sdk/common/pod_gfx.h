// pod_gfx - tiny software renderer for a 240x320 RGB565 framebuffer.
// Pixels are stored BIG-ENDIAN (byte-swapped) so the framebuffer can be sent
// to the ILI9341 with one SPI write. Everything is anti-aliased.
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "pod_fonts.h"

#define GFX_W 240
#define GFX_H 320

typedef struct { uint8_t r, g, b; } rgb_t;

extern uint16_t gfx_fb[GFX_W * GFX_H];

static inline uint16_t gfx_pack(rgb_t c) {
    uint16_t v = (uint16_t)(((c.r & 0xF8) << 8) | ((c.g & 0xFC) << 3) | (c.b >> 3));
    return (uint16_t)((v >> 8) | (v << 8));
}
static inline rgb_t gfx_unpack(uint16_t be) {
    uint16_t v = (uint16_t)((be >> 8) | (be << 8));
    rgb_t c = { (uint8_t)(((v >> 11) & 0x1F) * 255 / 31), (uint8_t)(((v >> 5) & 0x3F) * 255 / 63), (uint8_t)((v & 0x1F) * 255 / 31) };
    return c;
}
static inline rgb_t gfx_mix(rgb_t a, rgb_t b, int t256) {   // t=0 -> a, 256 -> b
    rgb_t o = { (uint8_t)(a.r + ((b.r - a.r) * t256 >> 8)), (uint8_t)(a.g + ((b.g - a.g) * t256 >> 8)),
                (uint8_t)(a.b + ((b.b - a.b) * t256 >> 8)) };
    return o;
}

void gfx_fill_rect(int x, int y, int w, int h, rgb_t c);
void gfx_hline_dither(int y, rgb_t c);                               // full-width row, ordered dither
void gfx_blend_px(int x, int y, rgb_t c, int a256);
void gfx_capsule(int x0, int x1, int yc, float r, rgb_t c);          // horizontal rounded bar
void gfx_circle(float cx, float cy, float r, rgb_t c);
void gfx_triangle(float x0, float y0, float x1, float y1, float x2, float y2, rgb_t c);

// Text: y is the TOP of the line box. align: 0 left, 1 centre, 2 right (x is the anchor).
int  gfx_text_width(const pod_font_t *f, const char *utf8);
void gfx_text(const pod_font_t *f, int x, int y, const char *utf8, rgb_t c, int align);
// Copy utf8 into out, truncating with an ellipsis so it fits max_w pixels.
void gfx_fit(const pod_font_t *f, const char *utf8, int max_w, char *out, int out_cap);

// Bilinear-scale an RGB565 (native-endian) image (row stride in pixels) into the framebuffer rect.
void gfx_scale_image(const uint16_t *src, int sw, int sh, int stride, int dx, int dy, int dw, int dh);
