#include "track_meta.h"
#include <string.h>
#include <ctype.h>

// ------------------------------------------------------------------ helpers
static uint32_t be32(const uint8_t *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }
static uint32_t le32(const uint8_t *p) { return (uint32_t)p[3] << 24 | (uint32_t)p[2] << 16 | (uint32_t)p[1] << 8 | p[0]; }
static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[1] << 8 | p[0]); }
static uint32_t syncsafe(const uint8_t *p) { return (uint32_t)(p[0] & 0x7F) << 21 | (uint32_t)(p[1] & 0x7F) << 14 | (uint32_t)(p[2] & 0x7F) << 7 | (p[3] & 0x7F); }

static bool rd(const track_reader_t *r, uint32_t off, void *buf, uint32_t n) {
    return off <= r->size && n <= r->size - off && r->read_at(r->ctx, off, buf, n) == n;
}

bool track_is_audio_name(const char *name) {
    size_t n = strlen(name);
    if (n < 5 || name[n - 4] != '.') return false;
    char e[4] = { (char)tolower((unsigned char)name[n - 3]), (char)tolower((unsigned char)name[n - 2]), (char)tolower((unsigned char)name[n - 1]), 0 };
    return !strcmp(e, "mp3") || !strcmp(e, "wav");
}

// Append one Unicode code point as UTF-8, never overflowing (cap includes the terminator).
static void put_cp(char *d, size_t *n, size_t cap, uint32_t cp) {
    char t[4]; size_t k;
    if (cp < 0x80) { t[0] = (char)cp; k = 1; }
    else if (cp < 0x800) { t[0] = (char)(0xC0 | cp >> 6); t[1] = (char)(0x80 | (cp & 0x3F)); k = 2; }
    else if (cp < 0x10000) { t[0] = (char)(0xE0 | cp >> 12); t[1] = (char)(0x80 | (cp >> 6 & 0x3F)); t[2] = (char)(0x80 | (cp & 0x3F)); k = 3; }
    else { t[0] = (char)(0xF0 | cp >> 18); t[1] = (char)(0x80 | (cp >> 12 & 0x3F)); t[2] = (char)(0x80 | (cp >> 6 & 0x3F)); t[3] = (char)(0x80 | (cp & 0x3F)); k = 4; }
    if (*n + k >= cap) return;
    memcpy(d + *n, t, k); *n += k; d[*n] = 0;
}

// ID3 text with an encoding byte: 0 Latin-1, 1 UTF-16 + BOM, 2 UTF-16BE, 3 UTF-8.
static void id3_text(char *dst, size_t cap, uint8_t enc, const uint8_t *p, size_t len) {
    size_t n = 0; dst[0] = 0;
    if (enc == 0) {
        for (size_t i = 0; i < len && p[i]; i++) put_cp(dst, &n, cap, p[i]);
    } else if (enc == 3) {
        size_t i = 0;                                           // copy whole characters only
        while (i < len && p[i]) {
            size_t k = p[i] < 0x80 ? 1 : p[i] < 0xE0 ? 2 : p[i] < 0xF0 ? 3 : 4;
            if (i + k > len || n + k >= cap) break;
            memcpy(dst + n, p + i, k); n += k; i += k;
        }
        dst[n] = 0;
    } else {
        bool be = enc == 2;
        size_t i = 0;
        if (enc == 1 && len >= 2) {
            if (p[0] == 0xFF && p[1] == 0xFE) { be = false; i = 2; }
            else if (p[0] == 0xFE && p[1] == 0xFF) { be = true; i = 2; }
        }
        for (; i + 1 < len; i += 2) {
            uint32_t u = be ? (uint32_t)(p[i] << 8 | p[i + 1]) : (uint32_t)(p[i + 1] << 8 | p[i]);
            if (!u) break;
            if (u >= 0xD800 && u < 0xDC00 && i + 3 < len) {      // surrogate pair
                uint32_t lo = be ? (uint32_t)(p[i + 2] << 8 | p[i + 3]) : (uint32_t)(p[i + 3] << 8 | p[i + 2]);
                if (lo >= 0xDC00 && lo < 0xE000) { u = 0x10000 + ((u - 0xD800) << 10) + (lo - 0xDC00); i += 2; }
            }
            put_cp(dst, &n, cap, u);
        }
    }
    while (n && (dst[n - 1] == ' ')) dst[--n] = 0;               // ID3v1-style padding
}

// Skip a NUL-terminated string in the given encoding; returns bytes consumed (incl. terminator).
static size_t skip_str(uint8_t enc, const uint8_t *p, size_t len) {
    size_t i = 0;
    if (enc == 1 || enc == 2) { while (i + 1 < len && (p[i] || p[i + 1])) i += 2; return i + 2 <= len ? i + 2 : len; }
    while (i < len && p[i]) i++;
    return i + 1 <= len ? i + 1 : len;
}

// ------------------------------------------------------------------- ID3v2
#define FRAME_PEEK 512

static void parse_id3v2(const track_reader_t *r, track_meta_t *m) {
    uint8_t h[10];
    if (!rd(r, 0, h, 10) || memcmp(h, "ID3", 3)) return;
    uint8_t ver = h[3], flags = h[5];
    uint32_t tag_size = syncsafe(h + 6);
    uint32_t end = 10 + tag_size + ((flags & 0x10) ? 10 : 0);   // footer
    m->audio_off = end;
    if (ver < 2 || ver > 4 || (flags & 0x80)) return;            // unsynchronised tags: skip parsing, still skip the tag
    uint32_t pos = 10;
    if ((flags & 0x40) && ver >= 3) {                            // extended header
        uint8_t e[4];
        if (!rd(r, pos, e, 4)) return;
        pos += ver == 4 ? syncsafe(e) : be32(e) + 4;
    }
    bool got_front = false;
    uint32_t tag_end = 10 + tag_size;
    while (pos + (ver == 2 ? 6 : 10) <= tag_end) {
        uint8_t fh[10];
        char id[5] = {0};
        uint32_t fsize, hdr;
        if (ver == 2) {
            if (!rd(r, pos, fh, 6)) return;
            memcpy(id, fh, 3); fsize = (uint32_t)fh[3] << 16 | fh[4] << 8 | fh[5]; hdr = 6;
        } else {
            if (!rd(r, pos, fh, 10)) return;
            memcpy(id, fh, 4); fsize = ver == 4 ? syncsafe(fh + 4) : be32(fh + 4); hdr = 10;
            if (fh[9] & 0x0C) { pos += hdr + fsize; continue; }  // compressed / encrypted frame
        }
        if (!id[0] || fsize == 0 || pos + hdr + fsize > tag_end) break;   // padding reached
        uint32_t body = pos + hdr;
        static uint8_t buf[FRAME_PEEK];          // static: keeps the small core-0 stack free
        uint32_t peek = fsize < FRAME_PEEK ? fsize : FRAME_PEEK;
        char *dst = NULL;
        if (!strcmp(id, "TIT2") || !strcmp(id, "TT2")) dst = m->title;
        else if (!strcmp(id, "TPE1") || !strcmp(id, "TP1")) dst = m->artist;
        else if (!strcmp(id, "TALB") || !strcmp(id, "TAL")) dst = m->album;
        if (dst && peek > 1 && rd(r, body, buf, peek)) {
            id3_text(dst, TRACK_TEXT, buf[0], buf + 1, peek - 1);
        } else if ((!strcmp(id, "APIC") || !strcmp(id, "PIC")) && peek > 4 && rd(r, body, buf, peek)) {
            uint8_t enc = buf[0];
            size_t i = 1;
            bool jpeg;
            if (ver == 2) {                                       // "JPG"/"PNG" format code
                jpeg = !memcmp(buf + 1, "JPG", 3) || !memcmp(buf + 1, "jpg", 3);
                i = 4;
            } else {
                const char *mime = (const char *)buf + 1;
                jpeg = !strncmp(mime, "image/jpeg", 10) || !strncmp(mime, "image/jpg", 9) || !strncmp(mime, "image/JPEG", 10);
                i += skip_str(0, buf + 1, peek - 1);
            }
            if (i >= peek) { pos = body + fsize; continue; }
            uint8_t ptype = buf[i++];
            i += skip_str(enc, buf + i, peek - i);
            if (i + 2 <= peek && buf[i] == 0xFF && buf[i + 1] == 0xD8) jpeg = true;   // trust the bytes
            if (jpeg && i < fsize && (!m->art_len || (ptype == 3 && !got_front))) {
                m->art_off = body + (uint32_t)i;
                m->art_len = fsize - (uint32_t)i;
                if (ptype == 3) got_front = true;
            }
        }
        pos = body + fsize;
    }
}

// ------------------------------------------------------------------- ID3v1
static void parse_id3v1(const track_reader_t *r, track_meta_t *m) {
    if (r->size < 128) return;
    uint8_t t[128];
    if (!rd(r, r->size - 128, t, 128) || memcmp(t, "TAG", 3)) return;
    if (m->audio_end > r->size - 128) m->audio_end = r->size - 128;
    if (!m->title[0])  id3_text(m->title,  TRACK_TEXT, 0, t + 3, 30);
    if (!m->artist[0]) id3_text(m->artist, TRACK_TEXT, 0, t + 33, 30);
    if (!m->album[0])  id3_text(m->album,  TRACK_TEXT, 0, t + 63, 30);
}

// --------------------------------------------------------------- MP3 frames
typedef struct { uint32_t bitrate, rate, len, spf; uint8_t mpeg1, mono; } mp3_hdr_t;

static bool mp3_header(const uint8_t *p, mp3_hdr_t *h) {
    static const uint16_t br1[16] = {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0};
    static const uint16_t br2[16] = {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0};
    static const uint32_t sr[3] = {44100, 48000, 32000};
    if (p[0] != 0xFF || (p[1] & 0xE0) != 0xE0) return false;
    int ver = (p[1] >> 3) & 3, layer = (p[1] >> 1) & 3, bri = p[2] >> 4, sri = (p[2] >> 2) & 3;
    if (ver == 1 || layer != 1 || bri == 0 || bri == 15 || sri == 3) return false;   // Layer III only
    h->mpeg1 = ver == 3;
    h->bitrate = h->mpeg1 ? br1[bri] : br2[bri];
    h->rate = sr[sri] >> (ver == 3 ? 0 : ver == 2 ? 1 : 2);
    h->spf = h->mpeg1 ? 1152 : 576;
    h->len = (h->mpeg1 ? 144000u : 72000u) * h->bitrate / h->rate + ((p[2] >> 1) & 1);
    h->mono = (p[3] >> 6) == 3;
    return true;
}

static bool parse_mp3(const track_reader_t *r, track_meta_t *m) {
    // Find the first frame whose successor is also a valid frame (avoids false syncs).
    static uint8_t buf[2048];                    // static: keeps the small core-0 stack free
    uint32_t scan_end = m->audio_off + 65536;
    for (uint32_t base = m->audio_off; base < scan_end && base + 4 < m->audio_end; base += sizeof buf - 3) {
        uint32_t n = m->audio_end - base < sizeof buf ? m->audio_end - base : sizeof buf;
        if (!rd(r, base, buf, n)) return false;
        for (uint32_t i = 0; i + 4 <= n; i++) {
            mp3_hdr_t h, h2;
            if (!mp3_header(buf + i, &h)) continue;
            uint32_t off = base + i;
            uint8_t nx[4];
            if (off + h.len + 4 <= m->audio_end && !(rd(r, off + h.len, nx, 4) && mp3_header(nx, &h2) && h2.rate == h.rate)) continue;

            m->format = TRACK_MP3;
            m->sample_rate = h.rate;
            m->channels = h.mono ? 1 : 2;
            m->bitrate_kbps = h.bitrate;
            m->audio_off = off;
            uint32_t frames = 0;

            // Xing / Info (VBR and LAME CBR) header inside the first frame
            uint32_t side = h.mpeg1 ? (h.mono ? 17 : 32) : (h.mono ? 9 : 17);
            uint8_t x[120];
            if (rd(r, off + 4 + side, x, sizeof x) && (!memcmp(x, "Xing", 4) || !memcmp(x, "Info", 4))) {
                uint32_t fl = be32(x + 4), k = 8;
                if (fl & 1) { frames = be32(x + k); k += 4; }
                if (fl & 2) { m->vbr_bytes = be32(x + k); k += 4; }
                if ((fl & 4) && k + 100 <= sizeof x) { memcpy(m->toc, x + k, 100); m->has_toc = true; }
                m->audio_off = off + h.len;                  // the Info frame itself is silent
            } else if (rd(r, off + 36, x, 18) && !memcmp(x, "VBRI", 4)) {
                frames = be32(x + 14);
                m->audio_off = off + h.len;
            }
            if (!m->vbr_bytes) m->vbr_bytes = m->audio_end - m->audio_off;
            if (frames) m->duration_ms = (uint32_t)((uint64_t)frames * h.spf * 1000 / h.rate);
            else m->duration_ms = (uint32_t)((uint64_t)(m->audio_end - m->audio_off) * 8 / h.bitrate);
            return true;
        }
    }
    return false;
}

// --------------------------------------------------------------------- WAV
static bool parse_wav(const track_reader_t *r, track_meta_t *m) {
    uint8_t h[12];
    if (!rd(r, 0, h, 12) || memcmp(h, "RIFF", 4) || memcmp(h + 8, "WAVE", 4)) return false;
    uint32_t pos = 12;
    bool fmt_ok = false;
    while (pos + 8 <= r->size) {
        uint8_t c[8];
        if (!rd(r, pos, c, 8)) break;
        uint32_t len = le32(c + 4), body = pos + 8;
        if (!memcmp(c, "fmt ", 4) && len >= 16) {
            uint8_t f[40];
            if (!rd(r, body, f, len < 40 ? len : 40)) return false;
            uint16_t tag = le16(f);
            if (tag == 0xFFFE && len >= 40) tag = le16(f + 24);  // WAVE_FORMAT_EXTENSIBLE -> sub-format
            m->channels = (uint8_t)le16(f + 2);
            m->sample_rate = le32(f + 4);
            m->block_align = le16(f + 12);
            m->bits = le16(f + 14);
            fmt_ok = tag == 1 && (m->bits == 16 || m->bits == 24) && (m->channels == 1 || m->channels == 2) &&
                     m->sample_rate >= 8000 && m->sample_rate <= 48000 && m->block_align == m->channels * m->bits / 8;
        } else if (!memcmp(c, "data", 4)) {
            m->audio_off = body;
            m->audio_end = body + len > r->size ? r->size : body + len;
        } else if (!memcmp(c, "LIST", 4) && len > 4) {             // INFO tags: INAM, IART, IPRD
            uint8_t t[4];
            if (rd(r, body, t, 4) && !memcmp(t, "INFO", 4)) {
                uint32_t p = body + 4;
                while (p + 8 <= body + len) {
                    uint8_t s[8]; if (!rd(r, p, s, 8)) break;
                    uint32_t sl = le32(s + 4);
                    char *dst = !memcmp(s, "INAM", 4) ? m->title : !memcmp(s, "IART", 4) ? m->artist : !memcmp(s, "IPRD", 4) ? m->album : NULL;
                    if (dst) {
                        uint8_t v[TRACK_TEXT]; uint32_t k = sl < sizeof v ? sl : sizeof v;
                        if (rd(r, p + 8, v, k)) id3_text(dst, TRACK_TEXT, 3, v, k);
                    }
                    p += 8 + sl + (sl & 1);
                }
            }
        }
        pos = body + len + (len & 1);
    }
    if (!fmt_ok || !m->audio_off || m->audio_end <= m->audio_off) return false;
    m->format = TRACK_WAV;
    m->duration_ms = (uint32_t)((uint64_t)(m->audio_end - m->audio_off) / m->block_align * 1000 / m->sample_rate);
    return true;
}

// ------------------------------------------------------------------- public
bool track_meta_read(const track_reader_t *r, const char *filename, track_meta_t *m) {
    memset(m, 0, sizeof *m);
    m->audio_end = r->size;
    bool ok;
    size_t n = strlen(filename);
    bool wav_name = n > 4 && (filename[n - 1] | 32) == 'v' && (filename[n - 2] | 32) == 'a' && (filename[n - 3] | 32) == 'w';
    if (wav_name) {
        ok = parse_wav(r, m);
    } else {
        parse_id3v2(r, m);
        parse_id3v1(r, m);
        ok = parse_mp3(r, m);
    }
    if (!m->title[0]) {                                   // fall back to the file name, minus extension
        size_t k = n < TRACK_TEXT ? n : TRACK_TEXT - 1;
        memcpy(m->title, filename, k); m->title[k] = 0;
        char *dot = strrchr(m->title, '.');
        if (dot && dot != m->title) *dot = 0;
    }
    return ok;
}

uint32_t track_seek_offset(const track_meta_t *m, uint32_t ms) {
    if (!m->duration_ms || ms == 0) return m->audio_off;
    if (ms >= m->duration_ms) ms = m->duration_ms - 1;
    if (m->format == TRACK_WAV) {
        uint64_t frame = (uint64_t)ms * m->sample_rate / 1000;
        return m->audio_off + (uint32_t)(frame * m->block_align);
    }
    uint32_t span = m->audio_end - m->audio_off;
    uint64_t off;
    if (m->has_toc) {                                     // Xing TOC: 100 points, each 0..255 of vbr_bytes
        uint32_t pm = (uint32_t)((uint64_t)ms * 100000 / m->duration_ms);   // 1/1000 percent
        uint32_t i = pm / 1000, frac = pm % 1000;
        uint32_t a = m->toc[i], b = i < 99 ? m->toc[i + 1] : 256;
        if (b < a) b = a;
        uint64_t f = (uint64_t)a * 1000 + (uint64_t)(b - a) * frac;          // 0..256000
        off = f * m->vbr_bytes / 256000;
        if (off > span) off = span;
    } else {
        off = (uint64_t)span * ms / m->duration_ms;
    }
    return m->audio_off + (uint32_t)off;
}
