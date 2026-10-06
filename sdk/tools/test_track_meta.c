// PC test for sd_player/track_meta.c and the MP3 decode/seek path.
//   cc -O2 -I../sd_player -I../third_party/minimp3 test_track_meta.c ../sd_player/track_meta.c -lm -o test_track_meta
//   ./test_track_meta file1.mp3 file2.wav ...
#define MINIMP3_IMPLEMENTATION
#define MINIMP3_ONLY_MP3
#define MINIMP3_NO_SIMD
#include "minimp3.h"
#include "track_meta.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t file_read_at(void *ctx, uint32_t off, void *buf, uint32_t n) {
    FILE *f = ctx;
    if (fseek(f, off, SEEK_SET)) return 0;
    return (uint32_t)fread(buf, 1, n, f);
}

// Decode from `off` to the end of the audio; returns decoded sample frames.
static uint64_t decode_from(FILE *f, const track_meta_t *m, uint32_t off, int *first_rate) {
    static mp3dec_t d; mp3dec_init(&d);
    static uint8_t in[8192]; static int16_t pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
    uint32_t len = 0, pos = 0, at = off; uint64_t frames = 0; *first_rate = 0;
    for (;;) {
        if (len - pos < 2048 && at < m->audio_end) {
            memmove(in, in + pos, len - pos); len -= pos; pos = 0;
            uint32_t want = sizeof in - len; if (want > m->audio_end - at) want = m->audio_end - at;
            uint32_t got = file_read_at(f, at, in + len, want); len += got; at += got;
        }
        if (pos >= len) break;
        mp3dec_frame_info_t info;
        int s = mp3dec_decode_frame(&d, in + pos, (int)(len - pos), pcm, &info);
        if (!info.frame_bytes) { if (at >= m->audio_end) break; pos = len; continue; }
        pos += info.frame_bytes;
        if (s > 0) { frames += s; if (!*first_rate) *first_rate = info.hz; }
    }
    return frames;
}

int main(int argc, char **argv) {
    int fails = 0;
    for (int i = 1; i < argc; i++) {
        FILE *f = fopen(argv[i], "rb");
        if (!f) { printf("%s: cannot open\n", argv[i]); fails++; continue; }
        fseek(f, 0, SEEK_END);
        track_reader_t r = { f, file_read_at, (uint32_t)ftell(f) };
        const char *name = strrchr(argv[i], '/') ? strrchr(argv[i], '/') + 1 : argv[i];
        track_meta_t m;
        bool ok = track_meta_read(&r, name, &m);
        printf("== %s\n   ok=%d fmt=%s rate=%u ch=%u dur=%u ms kbps=%u toc=%d audio=[%u,%u)\n   title='%s' artist='%s' album='%s'\n",
               name, ok, m.format == TRACK_MP3 ? "MP3" : m.format == TRACK_WAV ? "WAV" : "?", m.sample_rate, m.channels,
               m.duration_ms, m.bitrate_kbps, m.has_toc, m.audio_off, m.audio_end, m.title, m.artist, m.album);
        if (!ok) { fails++; fclose(f); continue; }
        if (m.art_len) {
            uint8_t h[2]; file_read_at(f, m.art_off, h, 2);
            printf("   art: %u bytes at %u, starts %02X%02X %s\n", m.art_len, m.art_off, h[0], h[1], (h[0] == 0xFF && h[1] == 0xD8) ? "(JPEG ok)" : "(BAD)");
            if (!(h[0] == 0xFF && h[1] == 0xD8)) fails++;
        }
        if (m.duration_ms < 29000 || m.duration_ms > 31000) { printf("   FAIL duration\n"); fails++; }
        if (m.format == TRACK_MP3) {
            int hz;
            uint64_t all = decode_from(f, &m, m.audio_off, &hz);
            uint32_t dec_ms = (uint32_t)(all * 1000 / (hz ? hz : 1));
            printf("   full decode: %llu frames = %u ms at %d Hz\n", (unsigned long long)all, dec_ms, hz);
            if (dec_ms < 29000 || dec_ms > 31500) { printf("   FAIL decode length\n"); fails++; }
            uint32_t off = track_seek_offset(&m, 15000);
            uint64_t rest = decode_from(f, &m, off, &hz);
            uint32_t rest_ms = (uint32_t)(rest * 1000 / (hz ? hz : 1));
            printf("   seek 15 s -> byte %u, remaining %u ms (expect ~15000)\n", off, rest_ms);
            if (rest_ms < 13500 || rest_ms > 16500) { printf("   FAIL seek\n"); fails++; }
        } else {
            uint32_t off = track_seek_offset(&m, 15000);
            uint32_t rest_ms = (uint32_t)((uint64_t)(m.audio_end - off) / m.block_align * 1000 / m.sample_rate);
            printf("   bits=%u align=%u seek 15 s -> byte %u, remaining %u ms, aligned=%d\n", m.bits, m.block_align, off, rest_ms,
                   (off - m.audio_off) % m.block_align == 0);
            if (rest_ms < 14900 || rest_ms > 15100 || (off - m.audio_off) % m.block_align) { printf("   FAIL seek\n"); fails++; }
        }
        fclose(f);
    }
    printf(fails ? "\n%d FAILURE(S)\n" : "\nALL PASSED\n", fails);
    return fails != 0;
}
