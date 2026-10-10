// Minimal XPT2046 resistive-touch driver (SPI). Returns raw 12-bit readings;
// calibration to screen pixels is done by the caller (see display_test).
#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef struct { uint16_t x, y, z; } xpt2046_raw_t;

void xpt2046_init(void);
bool xpt2046_irq_active(void);            // true while the panel is pressed (from the pressure reading)
bool xpt2046_irq_line_low(void);          // raw T_IRQ line level (low = pressed), for diagnostics
bool xpt2046_read(xpt2046_raw_t *out);    // averaged read; false if pressure too low
