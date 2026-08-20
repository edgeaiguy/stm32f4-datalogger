#include <stdio.h>
#include "stm32f407xx.h"
#include "uart2.h"
#include "systick.h"
#include "i2c.h"
#include "bmp280.h"
#include "spi.h"
#include "adxl345.h"

#define SAMPLE_INTERVAL_MS 1000

void bmp280_bringup(void) {
    printf("Initializing BMP280...\r\n");

    uint8_t id = 0;
    if (bmp280_read_id(&id) != 0) {
        printf("ERROR: I2C read failed\r\n");
        while (1);
    }

    printf("BMP280 ID: 0x%02X\r\n", id);
    if (id != BMP280_CHIP_ID) {
        printf("Unexpected chip ID (expected 0x%02X)\r\n", BMP280_CHIP_ID);
        while (1);
    }

    bmp280_calib_t calib;
    if (bmp280_init(&calib) != 0) {
        printf("ERROR: calibration read failed\r\n");
        while (1);
    }

    printf("calib: T1=%u T2=%d T3=%d\r\n", calib.dig_T1, calib.dig_T2, calib.dig_T3);
    printf("       P1=%u P2=%d P3=%d P4=%d P5=%d\r\n",
           calib.dig_P1, calib.dig_P2, calib.dig_P3, calib.dig_P4, calib.dig_P5);
    printf("       P6=%d P7=%d P8=%d P9=%d\r\n",
           calib.dig_P6, calib.dig_P7, calib.dig_P8, calib.dig_P9);

    // Schedule against absolute deadlines rather than delaying after each sample.
    // A measurement takes ~45 ms, so "read, then wait 1000 ms" would drift to a
    // 1045 ms period; advancing a deadline keeps the cadence at exactly 1000 ms.
    uint32_t next_sample = systick_millis();

    while (1) {
        // Signed difference so this stays correct across the counter wrap: it asks
        // "is now still before the deadline?" rather than comparing magnitudes.
        while ((int32_t)(systick_millis() - next_sample) < 0) {}
        next_sample += SAMPLE_INTERVAL_MS;

        uint32_t t_ms = systick_millis();
        int32_t temp_c100;
        uint32_t press_q24_8;

        if (bmp280_read(&calib, &temp_c100, &press_q24_8) != 0) {
            printf("ERROR: measurement failed\r\n");
            continue;  // back to the deadline wait, so a failure does not break cadence
        }

        // split the fixed-point values into integer and fractional parts so we
        // never have to pull float formatting into printf
        int32_t t = temp_c100;
        const char *sign = (t < 0) ? "-" : "";
        if (t < 0) t = -t;

        uint32_t pa = press_q24_8 >> 8;  // Q24.8 -> whole Pa

        printf("[%lu.%03lu] T = %s%ld.%02ld C   P = %lu.%02lu hPa   (%lu Pa)\r\n",
               (unsigned long)(t_ms / 1000), (unsigned long)(t_ms % 1000),
               sign, (long)(t / 100), (long)(t % 100),
               (unsigned long)(pa / 100), (unsigned long)(pa % 100),
               (unsigned long)pa);
    }
}

int main(void) {
    systick_init();
    uart2_init();
    //i2c_init();
    spi_init();

    printf("Initializing ADXL345...\r\n");

    /* Five DEVID reads, not one: on a shared bus a lone 0xE5 can be luck, while
     * five identical reads mean every other slave really is parked off MISO. */
    int stable = 1;
    printf("DEVID:");
    for (int i = 0; i < 5; i++) {
        uint8_t id = adxl345_read_register(ADXL345_DEVID_REG);
        printf(" 0x%02X", id);
        if (id != ADXL345_DEVID) stable = 0;
    }
    printf("   (expect 0x%02X)\r\n", ADXL345_DEVID);

    if (!stable) {
        printf("ADXL345 not responding — check CS parking in spi_init()\r\n");
        while (1);
    }

    adxl345_init();   /* now safe to configure: DATA_FORMAT + POWER_CTL */

    int16_t x, y, z;
    uint32_t next_sample = systick_millis();

    while (1) {
        while ((int32_t)(systick_millis() - next_sample) < 0) {}
        next_sample += 200;   /* 5 Hz for bring-up */

        adxl345_read_xyz(&x, &y, &z);

        /* ±2g full-res → 256 LSB/g. Integer milli-g, no float path. */
        int xm = (x * 1000) / 256;
        int ym = (y * 1000) / 256;
        int zm = (z * 1000) / 256;
        printf("X:%5d  Y:%5d  Z:%5d  (mg)\r\n", xm, ym, zm);
    }
}
