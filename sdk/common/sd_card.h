// sd_card - minimal SD / SDHC / SDXC driver over SPI (read-only).
// Shares SPI0 with the display and touch; every transaction takes pod_spi_lock.
#pragma once
#include <stdbool.h>
#include <stdint.h>

// Call once (core 0). Returns true if a card answered and is ready.
bool sd_card_init(void);
bool sd_card_ready(void);
// Read `count` 512-byte sectors starting at `lba` into buf.
bool sd_card_read(uint8_t *buf, uint32_t lba, uint32_t count);
