// FatFs glue for Pod's SPI SD card driver (read-only).
#include "ff.h"
#include "diskio.h"
#include "sd_card.h"

DSTATUS disk_status(BYTE pdrv) {
    if (pdrv) return STA_NOINIT;
    return sd_card_ready() ? 0 : STA_NOINIT;
}

DSTATUS disk_initialize(BYTE pdrv) {
    if (pdrv) return STA_NOINIT;
    return (sd_card_ready() || sd_card_init()) ? 0 : STA_NOINIT;
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count) {
    if (pdrv || !sd_card_ready()) return RES_NOTRDY;
    return sd_card_read(buff, (uint32_t)sector, count) ? RES_OK : RES_ERROR;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff) {
    if (pdrv) return RES_PARERR;
    switch (cmd) {
        case CTRL_SYNC:        return RES_OK;
        case GET_SECTOR_SIZE:  *(WORD *)buff = 512; return RES_OK;
        case GET_BLOCK_SIZE:   *(DWORD *)buff = 1;  return RES_OK;
        default:               return RES_PARERR;
    }
}
