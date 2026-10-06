// track_meta - everything Pod needs to know about a music file before playing it:
// format, where the audio starts and ends, sample rate, duration, tags and cover art.
// Pure C with a small reader interface, so it is unit-tested on a PC (tools/test_track_meta.c).
#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    void *ctx;
    // Read n bytes at absolute offset off; returns bytes actually read.
    uint32_t (*read_at)(void *ctx, uint32_t off, void *buf, uint32_t n);
    uint32_t size;
} track_reader_t;

typedef enum { TRACK_UNKNOWN = 0, TRACK_MP3, TRACK_WAV } track_format_t;

#define TRACK_TEXT 128

typedef struct {
    track_format_t format;
    char title[TRACK_TEXT], artist[TRACK_TEXT], album[TRACK_TEXT];   // UTF-8
    uint32_t art_off, art_len;      // embedded JPEG cover (0 = none)
    uint32_t audio_off, audio_end;  // byte range holding audio frames / PCM
    uint32_t sample_rate;
    uint8_t channels;
    uint32_t duration_ms;
    // MP3
    uint32_t bitrate_kbps;          // first frame (CBR estimate)
    bool has_toc;
    uint8_t toc[100];               // Xing seek table
    uint32_t vbr_bytes;             // bytes covered by the TOC
    // WAV
    uint16_t block_align, bits;
} track_meta_t;

// Fill m from the file. `filename` (UTF-8, no path) is used as the title when
// the file has no tags. Returns false if the file isn't a playable MP3 / WAV.
bool track_meta_read(const track_reader_t *r, const char *filename, track_meta_t *m);

// Absolute byte offset to start decoding from to land at time `ms`.
uint32_t track_seek_offset(const track_meta_t *m, uint32_t ms);

// True if the name ends in .mp3 or .wav (case-insensitive).
bool track_is_audio_name(const char *name);
