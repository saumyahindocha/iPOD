// Minimal ILI9341 driver (SPI, RGB565, portrait 240x320).
// Written so its ili9341_flush() maps 1:1 onto an LVGL v9 flush callback later.
#pragma once
#include <stdint.h>
#include <stdbool.h>

#define ILI9341_W 240
#define ILI9341_H 320

// RGB565 helpers
#define RGB565(r, g, b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))

void ili9341_init(void);
void ili9341_backlight(bool on);
// madctl: 0x48 = portrait, BGR panel (most red 2.8" modules). If red shows as
// blue, flip bit 0x08; if the image is mirrored, flip 0x40 / 0x80.
void ili9341_set_madctl(uint8_t madctl);
void ili9341_fill_rect(int x, int y, int w, int h, uint16_t color);
void ili9341_fill(uint16_t color);
// Push a block of big-endian-ready RGB565 pixels (w*h) into a window.
// px is in native little-endian uint16 order; the driver swaps bytes.
void ili9341_flush(int x1, int y1, int x2, int y2, const uint16_t *px);
// Push a rectangle straight out of a framebuffer whose pixels are already
// stored big-endian (byte-swapped RGB565), with the given row stride in pixels.
void ili9341_blit_be(int x, int y, int w, int h, const uint16_t *fb, int stride);
