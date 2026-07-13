#include <stdint.h>
#include "stm32f407xx.h"
#include "i2c.h"
#include "bmp280.h"

int bmp280_read_id(uint8_t *id) {
    return i2c_read_register(BMP280_ADDR, BMP280_REG_ID, id);
}