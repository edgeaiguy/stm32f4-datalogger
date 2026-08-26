/* diskio.c — FatFs glue onto the block primitives in sdcard.c.
 *
 * There is only one drive, so pdrv is validated and otherwise ignored. */
#include "ff.h"
#include "diskio.h"
#include "sdcard.h"

#define DRV_SD  0

DSTATUS disk_status(BYTE pdrv) {
    if (pdrv != DRV_SD) return STA_NOINIT;
    return sdcard_ready() ? 0 : STA_NOINIT;
}

/* The card is brought up before FatFs mounts, so this reports state rather
 * than re-running the init sequence. */
DSTATUS disk_initialize(BYTE pdrv) {
    return disk_status(pdrv);
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count) {
    if (pdrv != DRV_SD) return RES_PARERR;
    if (!sdcard_ready()) return RES_NOTRDY;

    for (UINT i = 0; i < count; i++) {
        if (sdcard_read_block((uint32_t)sector + i, buff + i * SD_BLOCK_SIZE) != 0) {
            return RES_ERROR;
        }
    }
    return RES_OK;
}

DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count) {
    if (pdrv != DRV_SD) return RES_PARERR;
    if (!sdcard_ready()) return RES_NOTRDY;

    for (UINT i = 0; i < count; i++) {
        if (sdcard_write_block((uint32_t)sector + i, buff + i * SD_BLOCK_SIZE) != 0) {
            return RES_ERROR;
        }
    }
    return RES_OK;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff) {
    if (pdrv != DRV_SD) return RES_PARERR;

    switch (cmd) {
    case CTRL_SYNC:
        /* sdcard_write_block() already waits out the card's busy line before
         * returning, so nothing is left in flight here. */
        return RES_OK;

    case GET_SECTOR_SIZE:
        *(WORD *)buff = SD_BLOCK_SIZE;
        return RES_OK;

    /* GET_SECTOR_COUNT and GET_BLOCK_SIZE are only consulted when FF_USE_MKFS
     * is 1; reporting them would mean parsing the CSD via CMD9. */
    default:
        return RES_PARERR;
    }
}
