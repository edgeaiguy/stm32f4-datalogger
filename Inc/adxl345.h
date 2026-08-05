#ifndef ADXL345_H
#define ADXL345_H

#include <stdint.h>
#include "stm32f407xx.h"

#define ADXL345_CS_HIGH()  (GPIOE_BSRR = (1 << 3))        /* deselect */
#define ADXL345_CS_LOW()   (GPIOE_BSRR = (1 << (3 + 16))) /* select   */

void    adxl345_init(void);
uint8_t adxl345_read_register(uint8_t reg);
void    adxl345_write_register(uint8_t reg, uint8_t value);
void    adxl345_read_xyz(int16_t *x, int16_t *y, int16_t *z);

#endif