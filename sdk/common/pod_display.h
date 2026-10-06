// pod_display - Pod's screen: home picker, "Option C / Poster" Now Playing,
// SD library list, volume overlay, battery icon and pop-up messages.
//
// Core 0 runs Bluetooth / audio and only calls the pod_display_*() API below.
// Setters copy into a mutex-protected mailbox and return in microseconds;
// all drawing, JPEG decoding and touch handling happen on core 1, so the
// screen can never stall audio.
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define POD_ART_MAX_BYTES (32 * 1024)   // AVRCP thumbnails: 200x200 JPEG, typically 8-25 KB

typedef enum {
    POD_CMD_NONE = 0, POD_CMD_PREV, POD_CMD_PLAYPAUSE, POD_CMD_NEXT,
    POD_CMD_FF_START, POD_CMD_REW_START, POD_CMD_SEEK_STOP,   // press-and-hold on next / prev
    POD_CMD_SEEK,          // SD mode: progress bar tapped/dragged -> pod_display_take_seek()
    POD_CMD_HOME,          // phone mode: "Pod" in the status bar tapped -> back to the home screen
    POD_CMD_BACK,          // SD mode: back from Now Playing to the library, or up one folder
    POD_CMD_LIST_SELECT,   // SD mode: a library row tapped -> pod_display_take_list_index()
    POD_CMD_NOW_PLAYING,   // SD mode: "Now Playing" tapped in the library header
} pod_cmd_t;

typedef enum { POD_SOURCE_PHONE = 1, POD_SOURCE_SD = 2 } pod_source_t;

// ---- start-up (core 0, in this order) ----------------------------------------
// Init display + touch, run first-boot touch calibration (or when a finger is on
// the screen at power-up), save it to flash.
void pod_display_boot(void);
// Show the home screen and wait for the user to pick Phone or SD card.
pod_source_t pod_display_home(void);
// SD mode: progress bar becomes seekable, status-bar tap means "back to library".
void pod_display_set_sd_mode(bool on);
// Hand the display over to core 1.
void pod_display_start(void);

// ---- Now Playing ----------------------------------------------------------------
void pod_display_set_status(const char *s);         // "Waiting for phone", "Connected", "Library", ...
void pod_display_set_title(const uint8_t *utf8, size_t len);
void pod_display_set_artist(const uint8_t *utf8, size_t len);
void pod_display_set_album(const uint8_t *utf8, size_t len);
void pod_display_set_playing(bool playing);
void pod_display_set_volume(int percent);           // 0..100, -1 = unknown
void pod_display_set_progress(uint32_t pos_ms, uint32_t len_ms, bool playing);
void pod_display_clear_track(void);                 // disconnect: back to the idle screen
void pod_display_clear_art(void);                   // track without artwork
void pod_display_set_art(const uint8_t *jpeg, size_t len);
// Zero-copy album art: write up to *cap bytes of JPEG into the returned buffer,
// then commit (len 0 = no art). Holds the display mailbox in between, so keep it short.
uint8_t *pod_display_art_begin(size_t *cap);
void pod_display_art_commit(size_t len);

// ---- status bar + messages ----------------------------------------------------
void pod_display_set_battery(int percent, bool charging);   // -1 hides the icon
void pod_display_toast(const char *utf8);                   // pop-up message for ~4 s

// ---- SD library list (core 0) -------------------------------------------------
#define POD_LIST_MAX   256
#define POD_LIST_POOL  10240          // bytes of names (UTF-8)
void pod_display_list_begin(const char *title, bool can_go_back);
bool pod_display_list_add(const char *utf8_name, bool is_folder);   // false when full
void pod_display_list_sort(void);                                  // folders first, then A-Z (case-insensitive)
void pod_display_list_end(int highlight);                          // shows the list; -1 = none
int  pod_display_list_count(void);
bool pod_display_list_get(int index, char *out, size_t cap, bool *is_folder);
void pod_display_show_list(int highlight);                         // back to the list
void pod_display_show_now_playing(void);

// ---- input (core 0) -----------------------------------------------------------
// Next queued command (POD_CMD_NONE if none). Call in a loop until NONE.
pod_cmd_t pod_display_take_command(void);
// Latest volume (0..100) set on the Pod's slider, or -1 if unchanged since the last call.
int pod_display_take_volume(void);
// Position for the last POD_CMD_SEEK, in 1/1000 of the track.
int pod_display_take_seek(void);
// Row for the last POD_CMD_LIST_SELECT.
int pod_display_take_list_index(void);
