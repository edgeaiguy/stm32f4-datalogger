#include <stdio.h>
#include "stm32f407xx.h"
#include "uart2.h"
#include "i2c.h"
#include "bmp280.h"

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
    if (id == BMP280_CHIP_ID) {
        printf("Chip ID matches! I2C is working.\r\n");
    } else {
        printf("Unexpected chip ID (expected 0x58)\r\n");
    }

    while (1);
}