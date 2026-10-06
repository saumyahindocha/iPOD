// Build (from sdk/tools/preview):
//   cc -O1 -w -Istub -I../../common -I../../third_party/tjpgd preview.c ../../common/pod_gfx.c ../../common/pod_fonts.c ../../third_party/tjpgd/tjpgd.c -lm -o preview
//   ./preview cover.jpg      -> writes 1_home.ppm ... 6_library_playing.ppm
// Renders Pod's screens on a PC into PPM images (uses the real pod_display.c).
#include "../../common/pod_display.c"
#include <stdlib.h>
uint64_t host_now_us = 1000000;
void xpt2046_init(void) {}
bool xpt2046_irq_active(void) { return false; }
bool xpt2046_read(xpt2046_raw_t *o) { (void)o; return false; }
static uint16_t screen[240 * 320];
void ili9341_init(void) {}
void ili9341_blit_be(int x, int y, int w, int h, const uint16_t *fb, int stride) {
    for (int r = 0; r < h; r++) for (int c = 0; c < w; c++) screen[(y + r) * 240 + x + c] = fb[(y + r) * stride + x + c];
}
static void save(const char *name) {
    FILE *f = fopen(name, "wb"); fprintf(f, "P6 240 320 255\n");
    for (int i = 0; i < 240 * 320; i++) { rgb_t c = gfx_unpack(screen[i]); fputc(c.r, f); fputc(c.g, f); fputc(c.b, f); }
    fclose(f);
}
static void load_art(const char *path) {
    FILE *f = fopen(path, "rb"); size_t n = fread(jpeg_work, 1, sizeof jpeg_work, f); fclose(f);
    has_art = decode_art(n);
}
int main(int argc, char **argv) {
    cur.volume = -1; cur.battery = 72; cur.charging = false; cur.list_highlight = -1;
    // 1. home
    render_home(-1); blit_all(); save("1_home.ppm");
    // 2. SD library list
    sd_mode = true;
    pod_display_list_begin("SD card", true);
    const char *names[] = {"Bollywood", "Coke Studio", "Lo-fi", "Aaj Ki Raat.mp3", "Kesariya.mp3", "Marine Drive at 2 AM.mp3",
                           "Night Trains - Harbour Lights - Extended Mix.mp3", "Tum Hi Ho.mp3", "voice memo.wav"};
    for (int i = 0; i < 9; i++) pod_display_list_add(names[i], i < 3);
    pod_display_list_sort();
    cur.screen = SCREEN_LIST; cur.title[0] = 0;
    render_screen(); blit_all(); save("2_library.ppm");
    // 3. SD now playing with art, seekable bar, battery + volume
    cur.screen = SCREEN_NOW; strcpy(cur.status, "Library");
    strcpy(cur.title, "Marine Drive at 2 AM"); strcpy(cur.artist, "Harbour Lights");
    cur.volume = 45; cur.len_ms = 214000; cur.pos_ms = 83000; cur.playing = true; cur.pos_stamp_us = host_now_us;
    if (argc > 1) load_art(argv[1]);
    render_screen(); blit_all(); save("3_now_playing_sd.ppm");
    // 4. volume overlay
    ov_show(); save("4_volume.ppm");
    ov_visible = false;
    // 5. low battery toast + phone mode
    sd_mode = false; strcpy(cur.status, "Connected"); cur.battery = 14;
    strcpy(cur.toast, "Battery low \xC2\xB7 14%"); toast_until_ms = 99999999;
    render_screen(); blit_all(); save("5_low_battery_phone.ppm");
    // 6. list with now-playing highlight + charging
    toast_until_ms = 0; cur.battery = 63; cur.charging = true; sd_mode = true;
    cur.screen = SCREEN_LIST; cur.list_highlight = 5;
    render_screen(); blit_all(); save("6_library_playing.ppm");
    puts("ok");
    return 0;
}
