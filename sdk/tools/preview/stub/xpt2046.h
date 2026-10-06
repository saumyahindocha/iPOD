#pragma once
#include <stdint.h>
#include <stdbool.h>
typedef struct { uint16_t x, y, z; } xpt2046_raw_t;
static inline void xpt2046_init(void) {}
static inline bool xpt2046_irq_active(void) { return false; }
static inline bool xpt2046_read(xpt2046_raw_t *o) { (void)o; return false; }
