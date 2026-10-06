#pragma once
#define FLASH_SECTOR_SIZE 4096
#define FLASH_PAGE_SIZE 256
static inline void flash_range_erase(unsigned o, unsigned n) { (void)o; (void)n; }
static inline void flash_range_program(unsigned o, const void *p, unsigned n) { (void)o; (void)p; (void)n; }
