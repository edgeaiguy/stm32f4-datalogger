#ifndef ADXL345_H
#define ADXL345_H

#include <stdint.h>

uint8_t adxl345_read_register(uint8_t reg);
void adxl345_write_register(uint8_t reg, uint8_t value);

#endif