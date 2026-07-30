#include <stdio.h>
#include "stm32f407xx.h"
#include "uart2.h"
#include "i2c.h"
#include "bmp280.h"

/* Crude blocking delay. Approximate only - it is a spin loop, so it shifts with
 * optimization flags and clock changes. Replace with SysTick once the logger
 * needs real timestamps. */
static void delay_ms(uint32_t ms) {
    for (uint32_t i = 0; i < ms; i++) {
        for (volatile uint32_t j = 0; j < 1600; j++);
    }
}

int main(void) {
    uart2_init();
    i2c_init();

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

    while (1) {
        int32_t temp_c100;
        uint32_t press_q24_8;

        if (bmp280_read(&calib, &temp_c100, &press_q24_8) != 0) {
            printf("ERROR: measurement failed\r\n");
            delay_ms(1000);
            continue;
        }

        // split the fixed-point values into integer and fractional parts so we
        // never have to pull float formatting into printf
        int32_t t = temp_c100;
        const char *sign = (t < 0) ? "-" : "";
        if (t < 0) t = -t;

        uint32_t pa = press_q24_8 >> 8;  // Q24.8 -> whole Pa

        printf("T = %s%ld.%02ld C   P = %lu.%02lu hPa   (%lu Pa)\r\n",
               sign, (long)(t / 100), (long)(t % 100),
               (unsigned long)(pa / 100), (unsigned long)(pa % 100),
               (unsigned long)pa);

        delay_ms(1000);
    }
}
