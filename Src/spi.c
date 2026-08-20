#include <stdint.h>
#include "stm32f407xx.h"
#include "spi.h"

static void spi_park_clock(void);

void spi_init(void) {
    // Enable GPIOA and SPI1 clocks
    RCC_AHB1ENR |= RCC_AHB1ENR_GPIOAEN; // enable GPIOA clock (PA5, PA6, PA7 for SPI)
    RCC_AHB1ENR |= RCC_AHB1ENR_GPIOEEN; // enable GPIOE clock (PE2, PE3 chip selects)
    RCC_APB2ENR |= RCC_APB2ENR_SPI1EN;  // enable SPI1 clock
    volatile unsigned int tmp = RCC_APB2ENR; (void) tmp; // readback to ensure clock is enabled

    // Configure PA5 (SCK), PA6 (MISO), PA7 (MOSI) for alternate function mode (AF5 for SPI)
    GPIOA_MODER &= ~((0x3 << 10) | (0x3 << 12) | (0x3 << 14)); // clear bits [11:10], [13:12], [15:14] for PA5, PA6, PA7
    GPIOA_MODER |= ((0x2 << 10) | (0x2 << 12) | (0x2 << 14));  // set PA5, PA6, PA7 to alternate function mode

    GPIOA_AFRL &= ~((0xF << 20) | (0xF << 24) | (0xF << 28));   // clear bits [23:20], [27:24], [31:28] for AFRL5, AFRL6, AFRL7
    GPIOA_AFRL |= ((0x5 << 20) | (0x5 << 24) | (0x5 << 28));    // set AFRL5, AFRL6, AFRL7 to AF5 (SPI1_SCK, SPI1_MISO, SPI1_MOSI)

    GPIOA_OSPEEDR |= ((0x3 << 10) | (0x3 << 12) | (0x3 << 14)); // set PA5, PA6, PA7 to very high speed

    /* Every slave on SPI1 needs its CS actively driven high when idle — a CS
     * left floating lets that chip drive MISO against whoever is selected.
     * PE2 = ADXL345 (keep in sync with adxl345.h), PE3 = onboard LIS3DSH. */
    GPIOE_MODER &= ~((0x3 << 4) | (0x3 << 6)); // clear bits [5:4] PE2, [7:6] PE3
    GPIOE_MODER |= ((0x1 << 4) | (0x1 << 6));  // both to output mode
    GPIOE_OSPEEDR |= (0x3 << 4);               // PE2 very high speed
    GPIOE_BSRR = (1 << 2) | (1 << 3);          // both deselected; PE3 stays parked

    /* SPI1 config — configure while SPE=0, enable last */
    SPI1_CR1 = 0;                  /* clean slate */
    SPI1_CR1 |= (1 << 2);          /* MSTR = master */
    SPI1_CR1 |= (1 << 1);          /* CPOL = 1  ┐ mode 3 */
    SPI1_CR1 |= (1 << 0);          /* CPHA = 1  ┘ */
    /* PCLK2 = 16 MHz (HSI, no PLL — see SystemInit); /32 keeps SCK well under
     * the ADXL345's 5 MHz ceiling with margin for breadboard wiring. */
    SPI1_CR1 |= (4 << 3);          /* BR = 100 → PCLK2/32 = 500 kHz */
    SPI1_CR1 |= (1 << 9);          /* SSM = 1  (software slave mgmt) */
    SPI1_CR1 |= (1 << 8);          /* SSI = 1  (internal NSS high — no MODF) */
    /* DFF bit 11 = 0 → 8-bit frames; LSBFIRST bit 7 = 0 → MSB first */

    /* Enable the peripheral — SPE last, after all config is set */
    SPI1_CR1 |= (1 << 6);          /* SPE = 1 */

    spi_park_clock();              /* settle SCK before anyone asserts CS */
}

uint8_t spi_transfer(uint8_t data) {
    uint32_t timeout = 10000; // simple timeout to avoid infinite loop
    // Wait until TXE (transmit buffer empty)
    while (!(SPI1_SR & (1 << 1)) && --timeout);
    // Send data
    SPI1_DR = data;
    timeout = 10000; // reset timeout for RXNE wait
    // Wait until RXNE (receive buffer not empty)
    while (!(SPI1_SR & (1 << 0)) && --timeout);
    uint8_t rx = SPI1_DR;              // reading DR also clears RXNE

    /* RXNE means the last bit was sampled, not that the frame is over. Callers
     * raise CS as soon as this returns, so drain the shifter first — cutting
     * the clock mid-edge desyncs the slave's bit counter for the NEXT frame. */
    timeout = 10000;
    while (!(SPI1_SR & (1 << 1)) && --timeout);   // TXE: TX buffer drained
    timeout = 10000;
    while ((SPI1_SR & (1 << 7)) && --timeout);    // BSY: shifter idle
    return rx;
}

/* Park SCK at its CPOL idle level before the first CS assertion. Straight out
 * of reset the pin sits low, and a slave that sees CS go active while SCK is
 * still settling counts the rising edge as a clock. */
static void spi_park_clock(void) {
    (void)spi_transfer(0xFF);          /* CS is inactive here, so nobody listens */
}