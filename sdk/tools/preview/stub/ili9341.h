#pragma once
#include <stdint.h>
void ili9341_init(void);
void ili9341_blit_be(int x, int y, int w, int h, const uint16_t *fb, int stride);
