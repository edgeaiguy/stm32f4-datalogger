#ifndef SDCARD_H
#define SDCARD_H

#include <stdint.h>
#include "stm32f407xx.h"

/* CS is on PE4, configured in spi2_init(). Keep the pin number in sync. */
#define SD_CS_HIGH()  (GPIOE_BSRR = (1 << 4))        /* deselect */
#define SD_CS_LOW()   (GPIOE_BSRR = (1 << (4 + 16))) /* select   */

#define SD_BLOCK_SIZE 512

typedef enum {
    SD_TYPE_UNKNOWN = 0,
    SD_TYPE_V1,        /* SDSC v1.x, byte addressing */
    SD_TYPE_V2,        /* SDSC v2,   byte addressing */
    SD_TYPE_V2_HC,     /* SDHC/SDXC, block addressing */
} sd_type_t;

/* Run the card through CMD0 -> CMD8 -> ACMD41 -> ready and leave SCK fast.
 * Returns 0 on success, negative on failure. */
int sdcard_init(void);

const char *sdcard_type_name(void);

/* Non-zero once sdcard_init() has identified a card. */
int sdcard_ready(void);

/* Read one 512-byte block. buf must have room for SD_BLOCK_SIZE bytes. */
int sdcard_read_block(uint32_t lba, uint8_t *buf);

/* Write one 512-byte block. Destroys whatever was at that LBA — there is no
 * undo, and low LBAs hold the partition table and filesystem metadata. */
int sdcard_write_block(uint32_t lba, const uint8_t *buf);

#endif // SDCARD_H
