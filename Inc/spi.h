#ifndef SPI_H
#define SPI_H

#include <stdint.h>

/* BR field values for SPI1_CR1[5:3]. PCLK2 = 16 MHz (HSI, no PLL). */
#define SPI_BR_DIV4    1   /* 4 MHz    — SD data phase, under the ADXL's 5 MHz ceiling */
#define SPI_BR_DIV8    2   /* 2 MHz    — fallback if 4 MHz is marginal on this wiring */
#define SPI_BR_DIV32   4   /* 500 kHz  — default, ADXL345 */
#define SPI_BR_DIV64   5   /* 250 kHz  — SD init, inside the card's 100–400 kHz window */

void spi_init(void);
uint8_t spi_transfer(uint8_t data);

/* Retune SCK for the device about to be selected. Only safe between
 * transactions — it briefly disables the peripheral. */
void spi_set_baudrate(uint32_t br);

#endif // SPI_H
