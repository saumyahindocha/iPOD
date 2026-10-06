// SD card mode.
//
// Core 0 runs this file: it browses folders on the card, decodes MP3 (minimp3)
// or WAV, applies volume and feeds the PCM5102 through pico_audio_i2s.
// Core 1 still owns the screen and touch (pod_display); the SD card shares SPI0
// with them through pod_spi_lock, and the display releases the bus every 24 rows,
// so the 8 KB read-ahead buffer below easily covers any wait.
#include "sd_player.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

#include "pico/stdlib.h"
#include "hardware/watchdog.h"
#include "hardware/dma.h"
#include "pico/audio_i2s.h"

#include "ff.h"
#include "pod_display.h"
#include "pod_battery.h"
#include "track_meta.h"

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_ONLY_MP3
#define MINIMP3_NO_SIMD
#include "minimp3.h"

#define SAMPLES_PER_BUFFER  1152
#define INBUF_SIZE          8192
#define PATH_MAX_LEN        256
#define NAME_MAX_LEN        256

// ------------------------------------------------------------------ state
static FATFS fs;
static FIL file;
static bool file_open;
static track_meta_t meta;

static char browse_dir[PATH_MAX_LEN] = "";      // folder shown in the library ("" = card root)
static char play_dir[PATH_MAX_LEN];             // folder the current track came from
static char play_name[NAME_MAX_LEN];            // current track's file name
static bool showing_now;                        // which screen core 1 is on

static enum { ST_IDLE, ST_PLAYING, ST_PAUSED } state = ST_IDLE;
static uint32_t base_ms;                        // track position at the last seek
static uint64_t frames_out;                     // sample frames handed to I2S since then
static int volume_pct = 50;
static volatile int32_t gain_q15;
static int scrub_dir;                           // press-and-hold on skip: -1 / +1
static uint32_t next_scrub_ms;

// decoder
static mp3dec_t mp3;
static uint8_t inbuf[INBUF_SIZE];
static uint32_t in_len, in_pos, file_pos;
static bool in_eof;
static int16_t pcm[MINIMP3_MAX_SAMPLES_PER_FRAME * 2];   // room for mono -> stereo expansion
static int pcm_frames, pcm_pos;

// audio out
static audio_format_t audio_format = { .format = AUDIO_BUFFER_FORMAT_PCM_S16, .sample_freq = 44100, .channel_count = 2 };
static audio_buffer_format_t producer_format = { .format = &audio_format, .sample_stride = 4 };
static audio_buffer_pool_t *pool;

// ------------------------------------------------------------------ helpers
static void join(char *out, size_t cap, const char *dir, const char *name) {
    snprintf(out, cap, "%s/%s", dir, name);
}

static void set_volume(int pct) {
    volume_pct = pct < 0 ? 0 : pct > 100 ? 100 : pct;
    uint32_t v = (uint32_t)volume_pct * 127 / 100;              // same curve as Bluetooth mode
    gain_q15 = (int32_t)(v * v * 32768u / (127u * 127u));
    pod_display_set_volume(volume_pct);
}

static uint32_t position_ms(void) {
    uint32_t rate = meta.sample_rate ? meta.sample_rate : 44100;
    return base_ms + (uint32_t)(frames_out * 1000 / rate);
}

static void push_progress(void) {
    pod_display_set_progress(position_ms(), meta.duration_ms, state == ST_PLAYING);
}

static void audio_init(void) {
    pool = audio_new_producer_pool(&producer_format, 4, SAMPLES_PER_BUFFER);
    audio_i2s_config_t config = {
        .data_pin = PICO_AUDIO_I2S_DATA_PIN,
        .clock_pin_base = PICO_AUDIO_I2S_CLOCK_PIN_BASE,
        .dma_channel = (int8_t)dma_claim_unused_channel(true),
        .pio_sm = 0,
    };
    dma_channel_unclaim(config.dma_channel);    // audio_i2s_setup claims it again (pico-extras issue #48)
    if (!audio_i2s_setup(&audio_format, &config)) panic("Pod: I2S setup failed");
    audio_i2s_connect(pool);
    audio_i2s_set_enabled(true);
}

// ------------------------------------------------------------ file reading
static uint32_t fat_read_at(void *ctx, uint32_t off, void *buf, uint32_t n) {
    FIL *f = ctx;
    UINT got = 0;
    if (f_lseek(f, off) != FR_OK || f_read(f, buf, n, &got) != FR_OK) return 0;
    return got;
}

static void decoder_reset(uint32_t from) {
    mp3dec_init(&mp3);
    in_len = in_pos = 0;
    file_pos = from;
    in_eof = false;
    pcm_frames = pcm_pos = 0;
}

static void refill(void) {
    if (in_eof || in_len - in_pos >= 2048) return;
    memmove(inbuf, inbuf + in_pos, in_len - in_pos);
    in_len -= in_pos; in_pos = 0;
    uint32_t want = INBUF_SIZE - in_len;
    if (want > meta.audio_end - file_pos) want = meta.audio_end - file_pos;
    uint32_t got = want ? fat_read_at(&file, file_pos, inbuf + in_len, want) : 0;
    in_len += got; file_pos += got;
    if (got < want || file_pos >= meta.audio_end) in_eof = true;
}

// Decode the next chunk into pcm[] (always stereo). Returns false at end of track.
static bool decode_more(void) {
    pcm_frames = pcm_pos = 0;
    if (meta.format == TRACK_WAV) {
        uint32_t frames = 1152;
        uint32_t bytes = frames * meta.block_align;
        if (bytes > INBUF_SIZE) { frames = INBUF_SIZE / meta.block_align; bytes = frames * meta.block_align; }
        if (bytes > meta.audio_end - file_pos) { frames = (meta.audio_end - file_pos) / meta.block_align; bytes = frames * meta.block_align; }
        if (!frames) return false;
        uint32_t got = fat_read_at(&file, file_pos, inbuf, bytes);
        file_pos += got;
        frames = got / meta.block_align;
        int step = meta.bits / 8;
        for (uint32_t i = 0; i < frames; i++) {
            const uint8_t *s = inbuf + i * meta.block_align;
            int16_t l = (int16_t)(s[step - 2] | s[step - 1] << 8);        // top 16 bits (16- or 24-bit)
            int16_t r = meta.channels == 2 ? (int16_t)(s[2 * step - 2] | s[2 * step - 1] << 8) : l;
            pcm[2 * i] = l; pcm[2 * i + 1] = r;
        }
        pcm_frames = (int)frames;
        return frames > 0;
    }
    for (int guard = 0; guard < 64; guard++) {
        refill();
        if (in_pos >= in_len) return false;
        mp3dec_frame_info_t info;
        int samples = mp3dec_decode_frame(&mp3, inbuf + in_pos, (int)(in_len - in_pos), pcm, &info);
        if (!info.frame_bytes) {                // need more data, or nothing decodable left
            if (in_eof) return false;
            in_pos = in_len;                    // drop garbage and read on
            continue;
        }
        in_pos += (uint32_t)info.frame_bytes;
        if (samples <= 0) continue;             // skipped a non-audio frame
        if (info.hz && (uint32_t)info.hz != audio_format.sample_freq) {
            printf("Pod: sample rate %d Hz\n", info.hz);
            audio_format.sample_freq = (uint32_t)info.hz;      // pico_audio_i2s re-divides its clock
            meta.sample_rate = (uint32_t)info.hz;
        }
        if (info.channels == 1)                 // expand mono to stereo, back to front
            for (int i = samples - 1; i >= 0; i--) { pcm[2 * i + 1] = pcm[i]; pcm[2 * i] = pcm[i]; }
        pcm_frames = samples;
        return true;
    }
    return false;
}

// Fill every free I2S buffer. Returns false when the track has finished.
static bool feed_audio(void) {
    audio_buffer_t *ab;
    bool alive = true;
    while ((ab = take_audio_buffer(pool, false)) != NULL) {
        int16_t *out = (int16_t *)ab->buffer->bytes;
        uint32_t n = 0, max = ab->max_sample_count;
        while (n < max && alive) {
            if (pcm_pos >= pcm_frames && !decode_more()) { alive = false; break; }
            uint32_t k = (uint32_t)(pcm_frames - pcm_pos);
            if (k > max - n) k = max - n;
            memcpy(out + 2 * n, pcm + 2 * pcm_pos, k * 4);
            pcm_pos += (int)k; n += k;
        }
        if (n < max) memset(out + 2 * n, 0, (max - n) * 4);
        int32_t g = gain_q15;
        for (uint32_t i = 0; i < 2 * max; i++) out[i] = (int16_t)(((int32_t)out[i] * g) >> 15);
        ab->sample_count = max;
        give_audio_buffer(pool, ab);
        frames_out += n;
        if (!alive) break;
    }
    return alive;
}

// ----------------------------------------------------------- folder listing
static bool skip_entry(const FILINFO *fi) {
    return (fi->fattrib & (AM_HID | AM_SYS)) || fi->fname[0] == '.' ||
           !strcmp(fi->fname, "System Volume Information");
}

static int list_highlight_for(const char *dir) {
    if (!play_name[0] || strcmp(dir, play_dir)) return -1;
    static char name[NAME_MAX_LEN];
    for (int i = 0; pod_display_list_get(i, name, sizeof name, NULL); i++)
        if (!strcmp(name, play_name)) return i;
    return -1;
}

static void show_folder(const char *dir) {
    if (dir != browse_dir) { strncpy(browse_dir, dir, sizeof browse_dir - 1); browse_dir[sizeof browse_dir - 1] = 0; }
    const char *title = browse_dir[0] ? strrchr(browse_dir, '/') + 1 : "SD card";
    pod_display_list_begin(title, true);         // at the root, back goes home
    DIR d;
    FILINFO fi;
    bool full = false;
    if (f_opendir(&d, browse_dir[0] ? browse_dir : "/") == FR_OK) {
        for (int pass = 0; pass < 2 && !full; pass++) {      // folders first, then songs
            f_rewinddir(&d);
            while (f_readdir(&d, &fi) == FR_OK && fi.fname[0]) {
                if (skip_entry(&fi)) continue;
                bool is_dir = fi.fattrib & AM_DIR;
                if (pass == 0 ? !is_dir : (is_dir || !track_is_audio_name(fi.fname))) continue;
                if (!pod_display_list_add(fi.fname, is_dir)) { full = true; break; }
            }
        }
        f_closedir(&d);
    }
    pod_display_list_sort();
    if (full) pod_display_toast("Folder too big \xC2\xB7 showing the first part");
    pod_display_list_end(list_highlight_for(browse_dir));
    showing_now = false;
}

// Next / previous audio file after `from` in play_dir, in the same order as the library list.
static bool neighbour_track(const char *from, int dir, char *out, size_t cap) {
    DIR d; FILINFO fi;
    bool found = false;
    if (f_opendir(&d, play_dir[0] ? play_dir : "/") != FR_OK) return false;
    while (f_readdir(&d, &fi) == FR_OK && fi.fname[0]) {
        if (skip_entry(&fi) || (fi.fattrib & AM_DIR) || !track_is_audio_name(fi.fname)) continue;
        int c = strcasecmp(fi.fname, from);
        if (dir > 0 ? c <= 0 : c >= 0) continue;
        if (!found || (dir > 0 ? strcasecmp(fi.fname, out) < 0 : strcasecmp(fi.fname, out) > 0)) {
            strncpy(out, fi.fname, cap - 1); out[cap - 1] = 0;
            found = true;
        }
    }
    f_closedir(&d);
    return found;
}

// ------------------------------------------------------------- cover art
static void load_art(void) {
    size_t cap;
    uint8_t *buf = pod_display_art_begin(&cap);
    size_t len = 0;
    if (meta.art_len && meta.art_len <= cap) {
        if (fat_read_at(&file, meta.art_off, buf, meta.art_len) == meta.art_len) len = meta.art_len;
    } else {                                         // no usable embedded art: try folder images
        static const char *names[] = { "cover.jpg", "folder.jpg", "Cover.jpg", "Folder.jpg", "front.jpg" };
        for (unsigned i = 0; i < sizeof names / sizeof names[0] && !len; i++) {
            static char path[PATH_MAX_LEN + 16];
            join(path, sizeof path, play_dir, names[i]);
            FIL f;
            if (f_open(&f, path, FA_READ) != FR_OK) continue;
            FSIZE_t sz = f_size(&f);
            UINT got = 0;
            if (sz > 0 && sz <= cap && f_read(&f, buf, (UINT)sz, &got) == FR_OK && got == sz) len = got;
            f_close(&f);
        }
        if (!len && meta.art_len) printf("Pod: cover art is %lu bytes, over the %u KB limit\n", (unsigned long)meta.art_len, POD_ART_MAX_BYTES / 1024);
    }
    pod_display_art_commit(len);
}

// --------------------------------------------------------------- tracks
static bool open_track(const char *dir, const char *name) {
    if (file_open) { f_close(&file); file_open = false; }
    static char path[PATH_MAX_LEN + NAME_MAX_LEN];
    snprintf(path, sizeof path, "%s/%s", dir, name);
    if (f_open(&file, path, FA_READ) != FR_OK) { pod_display_toast("Can't open that file"); state = ST_IDLE; return false; }
    file_open = true;
    track_reader_t rd = { &file, fat_read_at, (uint32_t)f_size(&file) };
    if (!track_meta_read(&rd, name, &meta)) {
        pod_display_toast("Not a playable MP3 / WAV");
        f_close(&file); file_open = false;
        state = ST_IDLE;
        return false;
    }
    strncpy(play_dir, dir, sizeof play_dir - 1); play_dir[sizeof play_dir - 1] = 0;
    strncpy(play_name, name, sizeof play_name - 1); play_name[sizeof play_name - 1] = 0;
    printf("Pod: playing %s (%s %lu Hz, %lu ms)\n", path, meta.format == TRACK_MP3 ? "MP3" : "WAV",
           (unsigned long)meta.sample_rate, (unsigned long)meta.duration_ms);

    pod_display_set_title((const uint8_t *)meta.title, strlen(meta.title));
    pod_display_set_artist((const uint8_t *)meta.artist, strlen(meta.artist));
    pod_display_set_album((const uint8_t *)meta.album, strlen(meta.album));
    load_art();

    if (meta.format == TRACK_WAV) audio_format.sample_freq = meta.sample_rate;
    decoder_reset(meta.audio_off);
    base_ms = 0; frames_out = 0;
    state = ST_PLAYING;
    pod_display_set_playing(true);
    push_progress();
    return true;
}

static void seek_to(uint32_t ms) {
    if (!file_open) return;
    if (meta.duration_ms && ms >= meta.duration_ms) ms = meta.duration_ms - 1;
    decoder_reset(track_seek_offset(&meta, ms));
    base_ms = ms; frames_out = 0;
    push_progress();
}

static void skip_track(int dir, bool user) {
    if (!play_name[0]) return;
    if (dir < 0 && file_open && position_ms() > 3000) { seek_to(0); return; }   // like an iPod: first restart the song
    static char from[NAME_MAX_LEN], next[NAME_MAX_LEN], d[PATH_MAX_LEN];
    strcpy(from, play_name);
    strcpy(d, play_dir);
    for (int tries = 0; tries < 16; tries++) {          // skip over files that won't open
        if (!neighbour_track(from, dir, next, sizeof next)) break;
        if (open_track(d, next)) {
            if (!showing_now) pod_display_show_list(list_highlight_for(browse_dir));
            return;
        }
        strcpy(from, next);
    }
    if (!file_open && !open_track(d, play_name)) return;   // nothing else playable: stay on this one
    if (dir < 0) { seek_to(0); return; }
    state = ST_PAUSED;                                   // end of the folder
    seek_to(0);
    pod_display_set_playing(false);
    if (!user) pod_display_toast("End of folder");
}

// ------------------------------------------------------------------ input
static void go_home(void) {
    printf("Pod: back to the home screen\n");
    sleep_ms(30);
    watchdog_reboot(0, 0, 10);
    while (true) tight_loop_contents();
}

static void handle_input(void) {
    int v = pod_display_take_volume();
    if (v >= 0) set_volume(v);

    pod_cmd_t c;
    while ((c = pod_display_take_command()) != POD_CMD_NONE) {
        switch (c) {
            case POD_CMD_PLAYPAUSE:
                if (state == ST_PLAYING) state = ST_PAUSED;
                else if (state == ST_PAUSED) state = ST_PLAYING;
                pod_display_set_playing(state == ST_PLAYING);
                push_progress();
                break;
            case POD_CMD_NEXT: skip_track(+1, true); break;
            case POD_CMD_PREV: skip_track(-1, true); break;
            case POD_CMD_FF_START:  scrub_dir = +1; next_scrub_ms = 0; break;
            case POD_CMD_REW_START: scrub_dir = -1; next_scrub_ms = 0; break;
            case POD_CMD_SEEK_STOP: scrub_dir = 0; break;
            case POD_CMD_SEEK:
                seek_to((uint32_t)((uint64_t)meta.duration_ms * (uint32_t)pod_display_take_seek() / 1000));
                break;
            case POD_CMD_LIST_SELECT: {
                static char name[NAME_MAX_LEN];
                bool is_dir;
                if (!pod_display_list_get(pod_display_take_list_index(), name, sizeof name, &is_dir)) break;
                if (is_dir) {
                    static char d[PATH_MAX_LEN];
                    join(d, sizeof d, browse_dir, name);
                    show_folder(d);
                } else if (open_track(browse_dir, name)) {
                    pod_display_show_now_playing();
                    showing_now = true;
                }
                break;
            }
            case POD_CMD_NOW_PLAYING:
                if (file_open) { pod_display_show_now_playing(); showing_now = true; }
                break;
            case POD_CMD_BACK:
                if (showing_now) {
                    show_folder(browse_dir);
                } else if (browse_dir[0]) {
                    static char d[PATH_MAX_LEN];
                    strcpy(d, browse_dir);
                    *strrchr(d, '/') = 0;                // up one folder ("" = root)
                    show_folder(d);
                } else {
                    go_home();
                }
                break;
            case POD_CMD_HOME: go_home(); break;
            default: break;
        }
    }

    // press-and-hold on the skip buttons: jump 5 s every 250 ms
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (scrub_dir && file_open && now >= next_scrub_ms) {
        int64_t t = (int64_t)position_ms() + scrub_dir * 5000;
        seek_to(t < 0 ? 0 : (uint32_t)t);
        next_scrub_ms = now + 250;
    }
}

// ------------------------------------------------------------------- main
void sd_player_main(void) {
    audio_init();
    set_volume(volume_pct);

    FRESULT fr = f_mount(&fs, "", 1);
    if (fr != FR_OK) {
        printf("Pod: SD mount failed (%d)\n", fr);
        pod_display_list_begin("No SD card", true);
        pod_display_list_end(-1);
        pod_display_toast(fr == FR_NO_FILESYSTEM ? "Card isn't FAT32 / exFAT" : "Insert a card, then tap back");
        showing_now = false;
        while (true) {                               // only "back" (home) is possible from here
            handle_input();
            sleep_ms(20);
        }
    }
    show_folder("");

    uint32_t last_progress = 0, last_battery = 0;
    while (true) {
        handle_input();
        bool busy = false;
        if (state == ST_PLAYING) {
            if (!feed_audio()) skip_track(+1, false);
            busy = true;
        }
        uint32_t now = to_ms_since_boot(get_absolute_time());
        if (state == ST_PLAYING && now - last_progress >= 1000) { push_progress(); last_progress = now; }
        if (now - last_battery >= 5000) { pod_battery_poll(); last_battery = now; }
        if (busy) sleep_us(500); else sleep_ms(5);
    }
}
