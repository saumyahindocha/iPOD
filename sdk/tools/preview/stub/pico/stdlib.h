#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
typedef uint64_t absolute_time_t;
extern uint64_t host_now_us;
static inline uint64_t time_us_64(void) { return host_now_us; }
static inline absolute_time_t get_absolute_time(void) { return host_now_us; }
static inline uint32_t to_ms_since_boot(absolute_time_t t) { return (uint32_t)(t / 1000); }
static inline int64_t absolute_time_diff_us(absolute_time_t a, absolute_time_t b) { return (int64_t)(b - a); }
static inline void sleep_ms(uint32_t ms) { host_now_us += ms * 1000ull; }
#define XIP_BASE 0
#define PICO_FLASH_SIZE_BYTES (4 * 1024 * 1024)
