// SD card over SPI, following the classic MMC/SD SPI-mode sequence
// (CMD0 -> CMD8 -> ACMD41 -> CMD58), as in ChaN's FatFs sample drivers.
#include "sd_card.h"
#include "pod_pins.h"
#include "pod_spi_lock.h"

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"

#define SD_INIT_BAUD   (400 * 1000)
#define SD_FAST_BAUD   (20 * 1000 * 1000)   // breadboard-safe; try 25 MHz on the PCB

#define CMD0   0      // GO_IDLE_STATE
#define CMD8   8      // SEND_IF_COND
#define CMD12  12     // STOP_TRANSMISSION
#define CMD16  16     // SET_BLOCKLEN
#define CMD17  17     // READ_SINGLE_BLOCK
#define CMD18  18     // READ_MULTIPLE_BLOCK
#define CMD55  55     // APP_CMD
#define CMD58  58     // READ_OCR
#define ACMD41 (0x80 | 41)   // SD_SEND_OP_COND

static bool ready;
static bool block_addressing;     // SDHC/SDXC use block numbers, old SDSC cards use bytes
static uint32_t fast_baud = SD_INIT_BAUD;

static inline uint8_t xfer(uint8_t b) {
    uint8_t r;
    spi_write_read_blocking(POD_SPI, &b, &r, 1);
    return r;
}

static bool wait_ready(uint32_t ms) {
    absolute_time_t until = make_timeout_time_ms(ms);
    do { if (xfer(0xFF) == 0xFF) return true; } while (!time_reached(until));
    return false;
}

static void deselect(void) {
    gpio_put(POD_SD_CS_PIN, 1);
    xfer(0xFF);                   // one extra clock so the card lets go of MISO (shared with touch)
}

static bool select_card(void) {
    gpio_put(POD_SD_CS_PIN, 0);
    xfer(0xFF);
    if (wait_ready(500)) return true;
    deselect();
    return false;
}

static void bus_take(uint32_t baud) {
    pod_spi_lock();
    spi_set_baudrate(POD_SPI, baud);
}

static void bus_give(void) {
    spi_set_baudrate(POD_SPI, POD_TFT_BAUD);
    pod_spi_unlock();
}

// Send a command; returns the R1 response (0xFF on timeout). Card stays selected.
static uint8_t send_cmd(uint8_t cmd, uint32_t arg) {
    if (cmd & 0x80) {             // ACMDn = CMD55 + CMDn
        cmd &= 0x7F;
        uint8_t r = send_cmd(CMD55, 0);
        if (r > 1) return r;
    }
    deselect();
    if (cmd == CMD0) { gpio_put(POD_SD_CS_PIN, 0); xfer(0xFF); }
    else if (!select_card()) return 0xFF;

    uint8_t frame[6] = { (uint8_t)(0x40 | cmd), (uint8_t)(arg >> 24), (uint8_t)(arg >> 16),
                         (uint8_t)(arg >> 8), (uint8_t)arg, 0x01 };
    if (cmd == CMD0) frame[5] = 0x95;      // only these two need a valid CRC in SPI mode
    if (cmd == CMD8) frame[5] = 0x87;
    spi_write_blocking(POD_SPI, frame, 6);
    if (cmd == CMD12) xfer(0xFF);          // skip the stuff byte

    uint8_t r = 0xFF;
    for (int n = 0; n < 10; n++) { r = xfer(0xFF); if (!(r & 0x80)) break; }
    return r;
}

static bool read_block(uint8_t *buf) {
    absolute_time_t until = make_timeout_time_ms(200);
    uint8_t token;
    do { token = xfer(0xFF); } while (token == 0xFF && !time_reached(until));
    if (token != 0xFE) return false;
    static const uint8_t ff[512] = { [0 ... 511] = 0xFF };
    spi_write_read_blocking(POD_SPI, ff, buf, 512);
    xfer(0xFF); xfer(0xFF);                // CRC (ignored)
    return true;
}

bool sd_card_init(void) {
    ready = false;
    gpio_init(POD_SD_CS_PIN);
    gpio_set_dir(POD_SD_CS_PIN, GPIO_OUT);
    gpio_put(POD_SD_CS_PIN, 1);

    bus_take(SD_INIT_BAUD);
    for (int i = 0; i < 10; i++) xfer(0xFF);   // >= 74 clocks with CS high to wake the card

    uint8_t type = 0;
    if (send_cmd(CMD0, 0) == 1) {               // card is in idle state
        absolute_time_t until = make_timeout_time_ms(1000);
        if (send_cmd(CMD8, 0x1AA) == 1) {       // SD v2
            uint8_t ocr[4];
            for (int i = 0; i < 4; i++) ocr[i] = xfer(0xFF);
            if (ocr[2] == 0x01 && ocr[3] == 0xAA) {
                while (!time_reached(until) && send_cmd(ACMD41, 1u << 30)) ;   // HCS: we handle SDHC
                if (!time_reached(until) && send_cmd(CMD58, 0) == 0) {
                    for (int i = 0; i < 4; i++) ocr[i] = xfer(0xFF);
                    block_addressing = (ocr[0] & 0x40) != 0;
                    type = 2;
                }
            }
        } else {                                // SD v1
            while (!time_reached(until) && send_cmd(ACMD41, 0)) ;
            if (!time_reached(until) && send_cmd(CMD16, 512) == 0) { block_addressing = false; type = 1; }
        }
    }
    deselect();
    bus_give();

    if (!type) { printf("Pod: no SD card found\n"); return false; }
    fast_baud = SD_FAST_BAUD;
    ready = true;
    printf("Pod: SD card ready (%s)\n", block_addressing ? "SDHC/SDXC" : "SDSC");
    return true;
}

bool sd_card_ready(void) { return ready; }

bool sd_card_read(uint8_t *buf, uint32_t lba, uint32_t count) {
    if (!ready || !count) return false;
    uint32_t addr = block_addressing ? lba : lba * 512;
    bool ok = true;
    bus_take(fast_baud);
    if (count == 1) {
        ok = send_cmd(CMD17, addr) == 0 && read_block(buf);
    } else if (send_cmd(CMD18, addr) == 0) {
        while (count--) { if (!read_block(buf)) { ok = false; break; } buf += 512; }
        send_cmd(CMD12, 0);
    } else {
        ok = false;
    }
    deselect();
    bus_give();
    return ok;
}
