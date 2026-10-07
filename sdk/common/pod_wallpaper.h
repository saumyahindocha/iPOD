// pod_wallpaper - home-screen picture stored in its own flash area.
// Written by tools/make_wallpaper.py (wallpaper.uf2), so it isn't part of the
// firmware image or the repo, and survives firmware updates.
#pragma once
#include <stdint.h>

#define POD_WALLPAPER_OFFSET 0x300000      // 3 MB into the 4 MB flash; firmware is ~0.7 MB

// 240x320 pixels in the framebuffer's byte order, or NULL if none is installed.
const uint16_t *pod_wallpaper(void);
