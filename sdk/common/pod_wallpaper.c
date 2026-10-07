#include "pod_wallpaper.h"
#include <string.h>
#include "pico/stdlib.h"

typedef struct {
    char magic[4];                 // "PODW"
    uint16_t w, h;
    uint32_t len, sum;             // pixel bytes and their byte-sum
} wallpaper_hdr_t;

const uint16_t *pod_wallpaper(void) {
    static int state;              // 0 = not checked, 1 = valid, -1 = none
    const uint8_t *base = (const uint8_t *)(XIP_BASE + POD_WALLPAPER_OFFSET);
    const wallpaper_hdr_t *h = (const wallpaper_hdr_t *)base;
    const uint8_t *px = base + 32;
    if (!state) {
        state = -1;
        if (!memcmp(h->magic, "PODW", 4) && h->w == 240 && h->h == 320 && h->len == 240u * 320u * 2u) {
            uint32_t s = 0;
            for (uint32_t i = 0; i < h->len; i++) s += px[i];
            if (s == h->sum) state = 1;
        }
    }
    return state > 0 ? (const uint16_t *)px : NULL;
}
