#include <stdio.h>
#include "stm32f407xx.h"
#include "uart2.h"
#include "systick.h"
#include "i2c.h"
#include "bmp280.h"
#include "spi.h"
#include "adxl345.h"
#include "sdcard.h"

#define TICK_MS       200   /* accelerometer cadence: 5 Hz */
#define ENV_DECIMATE  5     /* barometer runs every 5th tick: 1 Hz */

/* Prove the I2C link and load factory calibration. Halts on failure: there is
 * nothing worth logging from a sensor that never answered. */
static void bmp280_bringup(bmp280_calib_t *calib) {
    printf("Initializing BMP280...\r\n");

    uint8_t id = 0;
    if (bmp280_read_id(&id) != 0) {
        printf("ERROR: I2C read failed\r\n");
        while (1);
    }

    printf("BMP280 ID: 0x%02X (expect 0x%02X)\r\n", id, BMP280_CHIP_ID);
    if (id != BMP280_CHIP_ID) {
        printf("Unexpected chip ID\r\n");
        while (1);
    }

    if (bmp280_init(calib) != 0) {
        printf("ERROR: calibration read failed\r\n");
        while (1);
    }

    printf("calib: T1=%u T2=%d T3=%d\r\n", calib->dig_T1, calib->dig_T2, calib->dig_T3);
    printf("       P1=%u P2=%d P3=%d P4=%d P5=%d\r\n",
           calib->dig_P1, calib->dig_P2, calib->dig_P3, calib->dig_P4, calib->dig_P5);
    printf("       P6=%d P7=%d P8=%d P9=%d\r\n",
           calib->dig_P6, calib->dig_P7, calib->dig_P8, calib->dig_P9);
}

/* Five DEVID reads, not one: on a shared bus a lone 0xE5 can be luck, while
 * five identical reads mean every other slave really is parked off MISO.
 * Split out so it can be re-run as a regression check after SD traffic. */
static int adxl345_devid_ok(void) {
    int stable = 1;
    printf("DEVID:");
    for (int i = 0; i < 5; i++) {
        uint8_t id = adxl345_read_register(ADXL345_DEVID_REG);
        printf(" 0x%02X", id);
        if (id != ADXL345_DEVID) stable = 0;
    }
    printf("   (expect 0x%02X)\r\n", ADXL345_DEVID);
    return stable;
}

static void adxl345_bringup(void) {
    printf("Initializing ADXL345...\r\n");

    if (!adxl345_devid_ok()) {
        printf("ADXL345 not responding — check CS parking in spi_init()\r\n");
        while (1);
    }

    adxl345_init();   /* safe to configure now: DATA_FORMAT + POWER_CTL */
}

static void sdcard_bringup(void) {
    printf("Initializing SD card...\r\n");

    int rc = sdcard_init();
    if (rc != 0) {
        printf("ERROR: sdcard_init failed (%d)\r\n", rc);
        while (1);
    }
    printf("SD card: %s\r\n", sdcard_type_name());

    /* Block 0's 0x55AA signature is the DEVID trick again — a fixed constant
     * known in advance, so a successful read proves itself. */
    static uint8_t block[SD_BLOCK_SIZE];
    rc = sdcard_read_block(0, block);
    if (rc != 0) {
        printf("ERROR: block 0 read failed (%d)\r\n", rc);
        while (1);
    }

    printf("block 0 signature: 0x%02X%02X (expect 0x55AA)\r\n", block[510], block[511]);
    if (block[510] != 0x55 || block[511] != 0xAA) {
        printf("No boot signature — card may be unformatted, but the read path worked\r\n");
    }

    /* SD init retuned the prescaler to 4 MHz and ran traffic across a bus the
     * ADXL shares. Prove the ADXL still answers before trusting the sample loop. */
    printf("post-SD ");
    if (!adxl345_devid_ok()) {
        printf("ADXL345 lost after SD init — either the module holds MISO when\r\n"
               "deselected (move it to SPI2), or 4 MHz is too fast for this wiring\r\n"
               "(drop SPI_BR_DIV4 to SPI_BR_DIV8 in sdcard_init)\r\n");
        while (1);
    }
}

int main(void) {
    systick_init();
    uart2_init();
    i2c_init();
    spi_init();

    bmp280_calib_t calib;
    bmp280_bringup(&calib);
    adxl345_bringup();
    sdcard_bringup();

    /* Held between barometer ticks so every line carries a full record. */
    int32_t  temp_c100 = 0;
    uint32_t press_q24_8 = 0;
    int env_valid = 0;

    uint32_t tick = 0;
    uint32_t next_sample = systick_millis();

    while (1) {
        // Signed difference so this stays correct across the counter wrap: it asks
        // "is now still before the deadline?" rather than comparing magnitudes.
        while ((int32_t)(systick_millis() - next_sample) < 0) {}
        next_sample += TICK_MS;

        uint32_t t_ms = systick_millis();

        /* Accelerometer first. It is a microsecond-scale SPI read, so taking it
         * ahead of the barometer's ~43 ms blocking conversion keeps motion
         * samples on an even cadence instead of jittering by whether this tick
         * happened to include an environmental read. */
        int16_t x, y, z;
        adxl345_read_xyz(&x, &y, &z);

        if (tick % ENV_DECIMATE == 0) {
            if (bmp280_read(&calib, &temp_c100, &press_q24_8) == 0) {
                env_valid = 1;
            } else {
                printf("WARN: BMP280 measurement failed\r\n");
            }
        }
        tick++;

        /* ±2g full-res → 256 LSB/g. Integer milli-g, no float path. */
        int xm = (x * 1000) / 256;
        int ym = (y * 1000) / 256;
        int zm = (z * 1000) / 256;

        printf("[%lu.%03lu] ",
               (unsigned long)(t_ms / 1000), (unsigned long)(t_ms % 1000));

        if (env_valid) {
            // split the fixed-point values into integer and fractional parts so we
            // never have to pull float formatting into printf
            int32_t t = temp_c100;
            const char *sign = (t < 0) ? "-" : "";
            if (t < 0) t = -t;

            uint32_t pa = press_q24_8 >> 8;  // Q24.8 -> whole Pa

            printf("T=%s%ld.%02ld C  P=%lu.%02lu hPa  ",
                   sign, (long)(t / 100), (long)(t % 100),
                   (unsigned long)(pa / 100), (unsigned long)(pa % 100));
        } else {
            printf("T=  --.-- C  P= ---.-- hPa  ");
        }

        printf("X:%5d Y:%5d Z:%5d mg\r\n", xm, ym, zm);
    }
}
