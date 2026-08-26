/* sdcard.c — raw SD/SDHC access over SPI2, a bus private to the card */
#include <stdint.h>
#include "stm32f407xx.h"
#include "spi.h"
#include "systick.h"
#include "sdcard.h"

/* --- command indices --- */
#define CMD0     0   /* GO_IDLE_STATE     */
#define CMD8     8   /* SEND_IF_COND      */
#define CMD16   16   /* SET_BLOCKLEN      */
#define CMD17   17   /* READ_SINGLE_BLOCK */
#define CMD24   24   /* WRITE_BLOCK       */
#define CMD55   55   /* APP_CMD           */
#define CMD58   58   /* READ_OCR          */
#define ACMD41  41   /* SD_SEND_OP_COND   */

/* --- R1 response bits --- */
#define R1_READY        0x00
#define R1_IDLE         0x01
#define R1_ILLEGAL_CMD  0x04

#define SD_DATA_TOKEN   0xFE  /* precedes a 512-byte payload, both directions */

/* The card answers a write payload with xxx0sss1; sss = 010 means accepted. */
#define SD_RESP_MASK     0x1F
#define SD_RESP_ACCEPTED 0x05

/* CRC7 is only verified while the card is still in idle, so the two commands
 * that run before it leaves idle carry real values and the rest send a dummy. */
#define CRC_CMD0   0x95
#define CRC_CMD8   0x87
#define CRC_DUMMY  0x01

#define INIT_TIMEOUT_MS  1000  /* ACMD41 is spec'd to take up to a second */
#define READ_TIMEOUT_MS   200
#define BUSY_TIMEOUT_MS   500  /* a flash program cycle, generously bounded */

static sd_type_t card_type = SD_TYPE_UNKNOWN;
static int block_addressed;   /* SDHC/SDXC take an LBA; SDSC take a byte offset */

static uint8_t sd_rx(void) {
    return spi2_transfer(0xFF);
}

static void sd_select(void) {
    SD_CS_LOW();
}

/* While the card programs flash it holds DO low and ignores everything sent to
 * it. Waiting it out is what keeps the *next* command from failing. */
static int sd_wait_not_busy(uint32_t timeout_ms) {
    uint32_t deadline = systick_millis() + timeout_ms;
    while ((int32_t)(systick_millis() - deadline) < 0) {
        if (sd_rx() == 0xFF) return 0;
    }
    return -1;
}

/* Deselect, then clock eight more bits. The card needs them to finish its
 * internal state transition before the next command frame. */
static void sd_deselect(void) {
    SD_CS_HIGH();
    (void)sd_rx();
}

/* Send a 6-byte command frame; return R1, or 0xFF if the card never answered. */
static uint8_t sd_command(uint8_t cmd, uint32_t arg, uint8_t crc) {
    (void)sd_rx();                      /* one idle byte ahead of the frame */

    spi2_transfer(0x40 | cmd);           /* start bits 01 | index */
    spi2_transfer((uint8_t)(arg >> 24));
    spi2_transfer((uint8_t)(arg >> 16));
    spi2_transfer((uint8_t)(arg >> 8));
    spi2_transfer((uint8_t)arg);
    spi2_transfer(crc);

    /* R1 is the first byte back with the MSB clear, within 8 or so. */
    for (int i = 0; i < 10; i++) {
        uint8_t r = sd_rx();
        if ((r & 0x80) == 0) return r;
    }
    return 0xFF;
}

/* An app command is CMD55 followed by the real one. */
static uint8_t sd_acommand(uint8_t cmd, uint32_t arg) {
    uint8_t r = sd_command(CMD55, 0, CRC_DUMMY);
    if (r > R1_IDLE) return r;          /* 0x00 or 0x01 are both fine here */
    return sd_command(cmd, arg, CRC_DUMMY);
}

int sdcard_init(void) {
    card_type = SD_TYPE_UNKNOWN;
    block_addressed = 0;

    spi2_set_baudrate(SPI_BR_DIV64);     /* 250 kHz — the card demands a slow clock to wake */

    /* 80 clocks with CS parked high is what puts the card into SPI mode. */
    SD_CS_HIGH();
    for (int i = 0; i < 10; i++) (void)sd_rx();

    sd_select();

    /* CMD0: soft reset into idle. Retried — the first frame after the wake-up
     * clocks is commonly swallowed. */
    uint8_t r = 0xFF;
    for (int i = 0; i < 10; i++) {
        r = sd_command(CMD0, 0, CRC_CMD0);
        if (r == R1_IDLE) break;
    }
    if (r != R1_IDLE) { sd_deselect(); return -1; }

    /* CMD8: a v2 card echoes the 0x1AA check pattern back in the R7 tail; a v1
     * card rejects the command outright. */
    int v2 = 0;
    r = sd_command(CMD8, 0x000001AA, CRC_CMD8);
    if (r == R1_IDLE) {
        uint8_t r7[4];
        for (int i = 0; i < 4; i++) r7[i] = sd_rx();
        if (r7[2] != 0x01 || r7[3] != 0xAA) { sd_deselect(); return -2; }
        v2 = 1;
    } else if (!(r & R1_ILLEGAL_CMD)) {
        sd_deselect();
        return -2;
    }

    /* ACMD41 until the card leaves idle. HCS announces that we understand high
     * capacity; a v1 card has to see a zero argument instead. */
    uint32_t deadline = systick_millis() + INIT_TIMEOUT_MS;
    do {
        r = sd_acommand(ACMD41, v2 ? 0x40000000UL : 0);
        if (r == R1_READY) break;
    } while ((int32_t)(systick_millis() - deadline) < 0);
    if (r != R1_READY) { sd_deselect(); return -3; }

    if (v2) {
        /* CMD58: OCR bit 30 (CCS) separates block- from byte-addressed cards. */
        r = sd_command(CMD58, 0, CRC_DUMMY);
        if (r != R1_READY) { sd_deselect(); return -4; }
        uint8_t ocr[4];
        for (int i = 0; i < 4; i++) ocr[i] = sd_rx();
        block_addressed = (ocr[0] & 0x40) != 0;
        card_type = block_addressed ? SD_TYPE_V2_HC : SD_TYPE_V2;
    } else {
        card_type = SD_TYPE_V1;
    }

    /* Byte-addressed cards can come up with a block length other than 512. */
    if (!block_addressed && sd_command(CMD16, SD_BLOCK_SIZE, CRC_DUMMY) != R1_READY) {
        sd_deselect();
        return -5;
    }

    sd_deselect();
    spi2_set_baudrate(SPI_BR_DIV4);      /* 4 MHz for the data phase */
    return 0;
}

int sdcard_read_block(uint32_t lba, uint8_t *buf) {
    if (card_type == SD_TYPE_UNKNOWN) return -1;

    uint32_t addr = block_addressed ? lba : lba * SD_BLOCK_SIZE;

    sd_select();
    /* Self-heal after a write whose busy poll timed out. */
    if (sd_wait_not_busy(BUSY_TIMEOUT_MS) != 0) { sd_deselect(); return -4; }

    if (sd_command(CMD17, addr, CRC_DUMMY) != R1_READY) {
        sd_deselect();
        return -2;
    }

    /* The card holds DO high until its data is ready, then sends 0xFE. Any other
     * non-0xFF byte is an error token. */
    uint8_t token = 0xFF;
    uint32_t deadline = systick_millis() + READ_TIMEOUT_MS;
    while ((int32_t)(systick_millis() - deadline) < 0) {
        token = sd_rx();
        if (token != 0xFF) break;
    }
    if (token != SD_DATA_TOKEN) {
        sd_deselect();
        return -3;
    }

    for (uint32_t i = 0; i < SD_BLOCK_SIZE; i++) buf[i] = sd_rx();

    (void)sd_rx();   /* 16-bit CRC, discarded — CRC checking is off in SPI mode */
    (void)sd_rx();

    sd_deselect();
    return 0;
}

int sdcard_write_block(uint32_t lba, const uint8_t *buf) {
    if (card_type == SD_TYPE_UNKNOWN) return -1;

    uint32_t addr = block_addressed ? lba : lba * SD_BLOCK_SIZE;

    sd_select();
    if (sd_wait_not_busy(BUSY_TIMEOUT_MS) != 0) { sd_deselect(); return -5; }

    if (sd_command(CMD24, addr, CRC_DUMMY) != R1_READY) {
        sd_deselect();
        return -2;
    }

    (void)sd_rx();                      /* one idle byte before the token */
    spi2_transfer(SD_DATA_TOKEN);
    for (uint32_t i = 0; i < SD_BLOCK_SIZE; i++) spi2_transfer(buf[i]);
    spi2_transfer(0xFF);                /* CRC placeholder — checking is off */
    spi2_transfer(0xFF);

    uint8_t resp = sd_rx();
    if ((resp & SD_RESP_MASK) != SD_RESP_ACCEPTED) {
        sd_deselect();
        return -3;
    }

    /* Programming starts here, not at the response token. Returning before it
     * finishes is the classic way to make the next command fail instead. */
    if (sd_wait_not_busy(BUSY_TIMEOUT_MS) != 0) {
        sd_deselect();
        return -4;
    }

    sd_deselect();
    return 0;
}

int sdcard_ready(void) {
    return card_type != SD_TYPE_UNKNOWN;
}

const char *sdcard_type_name(void) {
    switch (card_type) {
        case SD_TYPE_V1:    return "SDSC v1 (byte addressed)";
        case SD_TYPE_V2:    return "SDSC v2 (byte addressed)";
        case SD_TYPE_V2_HC: return "SDHC/SDXC (block addressed)";
        default:            return "unknown";
    }
}
