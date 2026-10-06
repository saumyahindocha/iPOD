// pod_display - "Option C / Poster" Now Playing screen.
//
// Core 0 runs Bluetooth + audio and only calls the pod_display_*() API below.
// Setters copy into a mutex-protected mailbox and return in microseconds;
// all drawing, JPEG decoding and touch handling happen on core 1, so the
// screen can never stall audio.
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define POD_ART_MAX_BYTES (32 * 1024)   // AVRCP thumbnails: 200x200 JPEG, typically 8-25 KB

typedef enum { POD_CMD_NONE = 0, POD_CMD_PREV, POD_CMD_PLAYPAUSE, POD_CMD_NEXT } pod_cmd_t;

// Core 0, before Bluetooth starts: init display + touch, run first-boot touch
// calibration (or when a finger is on the screen at power-up), save it to flash.
void pod_display_boot(void);
// Core 0: hand the display over to core 1.
void pod_display_start(void);

void pod_display_set_status(const char *s);         // "Waiting for phone", "Connected", ...
void pod_display_set_title(const uint8_t *utf8, size_t len);
void pod_display_set_artist(const uint8_t *utf8, size_t len);
void pod_display_set_album(const uint8_t *utf8, size_t len);
void pod_display_set_playing(bool playing);
void pod_display_set_volume(int percent);           // 0..100, -1 = unknown
void pod_display_set_progress(uint32_t pos_ms, uint32_t len_ms, bool playing);
void pod_display_clear_track(void);                 // disconnect: back to the idle screen
void pod_display_clear_art(void);                   // track without artwork
void pod_display_set_art(const uint8_t *jpeg, size_t len);

// Core 0: fetch a transport command from a touch tap (POD_CMD_NONE if none).
pod_cmd_t pod_display_take_command(void);
