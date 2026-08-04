#include "stm32f407xx.h"
#include "adxl345.h"

#define ADXL345_CS_HIGH()  (GPIOE_BSRR = (1 << 3))        /* deselect */
#define ADXL345_CS_LOW()   (GPIOE_BSRR = (1 << (3 + 16))) /* select   */

uint8_t adxl345_read_register(uint8_t reg)
{
    ADXL345_CS_LOW();
    spi_transfer(reg | 0x80);          /* MSB set = read */
    uint8_t value = spi_transfer(0xFF); /* dummy byte clocks out the response */
    ADXL345_CS_HIGH();
    return value;
}

void adxl345_write_register(uint8_t reg, uint8_t data)
{
    ADXL345_CS_LOW();
    spi_transfer(reg);                 /* MSB clear = write */
    spi_transfer(data);
    ADXL345_CS_HIGH();
}