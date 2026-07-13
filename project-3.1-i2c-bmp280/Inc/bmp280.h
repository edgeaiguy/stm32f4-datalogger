#ifndef BMP280_H
#define BMP280_H

#include <stdint.h>

#define BMP280_ADDR        0x76   // default I2C address (SDO tied to GND)
#define BMP280_REG_ID      0xD0   // chip ID register
#define BMP280_CHIP_ID     0x58   // expected value

int bmp280_read_id(uint8_t *id);

#endif