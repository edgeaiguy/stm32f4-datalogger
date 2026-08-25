#include <stdint.h>
#include "stm32f407xx.h"
#include "spi.h"

/* Two buses, one driver core. SPI1 is shared (ADXL345 + onboard LIS3DSH);
 * SPI2 is private to the SD card, whose module drives MISO even when
 * deselected and so cannot share a bus with anything. */

#define SPI_SR_RXNE  (1 << 0)
#define SPI_SR_TXE   (1 << 1)
#define SPI_SR_BSY   (1 << 7)
#define SPI_CR1_SPE  (1 << 6)

static uint8_t xfer(spi_regs_t *spi, uint8_t data) {
    uint32_t timeout = 10000; // simple timeout to avoid infinite loop
    while (!(spi->SR & SPI_SR_TXE) && --timeout);
    spi->DR = data;
    timeout = 10000;
    while (!(spi->SR & SPI_SR_RXNE) && --timeout);
    uint8_t rx = spi->DR;              // reading DR also clears RXNE

    /* RXNE means the last bit was sampled, not that the frame is over. Callers
     * raise CS as soon as this returns, so drain the shifter first — cutting
     * the clock mid-edge desyncs the slave's bit counter for the NEXT frame. */
    timeout = 10000;
    while (!(spi->SR & SPI_SR_TXE) && --timeout);
    timeout = 10000;
    while ((spi->SR & SPI_SR_BSY) && --timeout);
    return rx;
}

/* BR can only be written with the peripheral disabled, and disabling it mid-frame
 * truncates whatever is in the shifter — so drain both directions first. */
static void set_baudrate(spi_regs_t *spi, uint32_t br) {
    uint32_t timeout = 10000;
    while ((spi->SR & SPI_SR_RXNE) && --timeout) { (void)spi->DR; }
    timeout = 10000;
    while ((spi->SR & SPI_SR_BSY) && --timeout);

    spi->CR1 &= ~SPI_CR1_SPE;
    spi->CR1 = (spi->CR1 & ~(0x7 << 3)) | ((br & 0x7) << 3);
    spi->CR1 |= SPI_CR1_SPE;
}

/* Mode 3, 8-bit, MSB first, software NSS. Caller sets BR and enables SPE. */
static void configure_mode3(spi_regs_t *spi, uint32_t br) {
    spi->CR1 = 0;                  /* clean slate, SPE off */
    spi->CR1 |= (1 << 2);          /* MSTR = master */
    spi->CR1 |= (1 << 1);          /* CPOL = 1  ┐ mode 3 */
    spi->CR1 |= (1 << 0);          /* CPHA = 1  ┘ */
    spi->CR1 |= (br << 3);
    spi->CR1 |= (1 << 9);          /* SSM = 1  (software slave mgmt) */
    spi->CR1 |= (1 << 8);          /* SSI = 1  (internal NSS high — no MODF) */
    /* DFF bit 11 = 0 → 8-bit frames; LSBFIRST bit 7 = 0 → MSB first */
    spi->CR1 |= SPI_CR1_SPE;       /* enable last, after all config is set */
}

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

    GPIOA_PUPDR &= ~(0x3 << 12);  // clear bits [13:12] for PA6
    GPIOA_PUPDR |= (0x1 << 12);   // pull-up on MISO: keeps the line defined when no slave drives it

    /* Every slave on SPI1 needs its CS actively driven high when idle — a CS
     * left floating lets that chip drive MISO against whoever is selected.
     * PE2 = ADXL345 (adxl345.h), PE3 = onboard LIS3DSH. */
    GPIOE_MODER &= ~((0x3 << 4) | (0x3 << 6)); // clear PE2, PE3
    GPIOE_MODER |= ((0x1 << 4) | (0x1 << 6));  // both to output mode
    GPIOE_OTYPER &= ~((1 << 2) | (1 << 3));    // push-pull
    GPIOE_OSPEEDR |= (0x3 << 4);               // PE2 very high speed
    GPIOE_BSRR = (1 << 2) | (1 << 3);          // both deselected; PE3 stays parked

    configure_mode3(SPI1_REGS, SPI_BR_DIV32);
}

void spi2_init(void) {
    RCC_AHB1ENR |= RCC_AHB1ENR_GPIOBEN; // PB13/14/15 carry SPI2
    RCC_AHB1ENR |= RCC_AHB1ENR_GPIOEEN; // PE4 is the SD chip select
    RCC_APB1ENR |= RCC_APB1ENR_SPI2EN;
    volatile unsigned int tmp = RCC_APB1ENR; (void) tmp;

    // PB13 (SCK), PB14 (MISO), PB15 (MOSI) to alternate function mode
    GPIOB_MODER &= ~((0x3 << 26) | (0x3 << 28) | (0x3 << 30));
    GPIOB_MODER |= ((0x2 << 26) | (0x2 << 28) | (0x2 << 30));

    // AFRH covers pins 8-15, so PB13/14/15 sit at nibbles 5/6/7
    GPIOB_AFRH &= ~((0xF << 20) | (0xF << 24) | (0xF << 28));
    GPIOB_AFRH |= ((0x5 << 20) | (0x5 << 24) | (0x5 << 28));   // AF5 = SPI2

    GPIOB_OSPEEDR |= ((0x3 << 26) | (0x3 << 28) | (0x3 << 30));

    GPIOB_PUPDR &= ~(0x3 << 28);  // clear bits [29:28] for PB14
    GPIOB_PUPDR |= (0x1 << 28);   // pull-up on MISO

    /* PE4 = SD card CS. Only slave on this bus, but it still parks high so the
     * card sees the 74 wake-up clocks deselected. */
    GPIOE_MODER &= ~(0x3 << 8);
    GPIOE_MODER |= (0x1 << 8);
    GPIOE_OTYPER &= ~(1 << 4);
    GPIOE_OSPEEDR |= (0x3 << 8);
    GPIOE_BSRR = (1 << 4);

    /* PCLK1 is also 16 MHz (HSI, no PLL, no APB prescaler), so the divider
     * constants mean the same rates here as on SPI1. */
    configure_mode3(SPI2_REGS, SPI_BR_DIV64);
}

uint8_t spi_transfer(uint8_t data) {
    return xfer(SPI1_REGS, data);
}

uint8_t spi2_transfer(uint8_t data) {
    return xfer(SPI2_REGS, data);
}

void spi2_set_baudrate(uint32_t br) {
    set_baudrate(SPI2_REGS, br);
}

