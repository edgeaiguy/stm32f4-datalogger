#include <stdint.h>
#include "stm32f407xx.h"
#include "i2c.h"

void i2c_init(void) {
  // enable GPIOB and I2C1 clocks
  RCC_AHB1ENR |= RCC_AHB1ENR_GPIOBEN; // enable GPIOB clock (PB6 and PB7 for I2C)
  RCC_APB1ENR |= RCC_APB1ENR_I2C1EN; // enable I2C1 clock
  // readback to ensure clock is enabled before proceeding
  volatile unsigned int tmp = RCC_APB1ENR; (void) tmp;

  // configure PB6 and PB7 for alternate function mode (AF4 for I2C)
  GPIOB_MODER &= ~(0x3 << 12); // clear bits [13:12] for PB6
  GPIOB_MODER &= ~(0x3 << 14); // clear bits [15:14] for PB7
  GPIOB_MODER |= (0x2 << 12); // set PB6 to alternate function mode
  GPIOB_MODER |= (0x2 << 14); // set PB7 to alternate function mode

  GPIOB_OTYPER |= (1 << 6); // set PB6 to open-drain
  GPIOB_OTYPER |= (1 << 7); // set PB7 to open-drain

  GPIOB_OSPEEDR |= (0x2 << 12); // set PB6 to high speed
  GPIOB_OSPEEDR |= (0x2 << 14); // set PB7 to high speed

  GPIOB_PUPDR &= ~(0x3 << 12); // clear bits [13:12] for PB6
  GPIOB_PUPDR &= ~(0x3 << 14); // clear bits [15:14] for PB7
  GPIOB_PUPDR |= (0x1 << 12); // set PB6 to pull-up
  GPIOB_PUPDR |= (0x1 << 14); // set PB7 to pull-up

  GPIOB_AFRL &= ~(0xF << 24); // clear bits [27:24] for AFRL6
  GPIOB_AFRL &= ~(0xF << 28); // clear bits [31:28] for AFRL7
  GPIOB_AFRL |= (0x4 << 24); // set AFRL6 to AF4 (I2C1_SCL)
  GPIOB_AFRL |= (0x4 << 28); // set AFRL7 to AF4 (I2C1_SDA)

  // disable I2C1 before configuring timing registers
  I2C1_CR1 &= ~(1 << 0);  // PE = 0

  // set APB1 clock frequency (MHz) in CR2
  I2C1_CR2 = 16;  // 16 MHz if on HSI
  
  // set CCR for 100kHz standard mode
  I2C1_CCR = 80;  // at 16 MHz

  // set maximum rise time
  I2C1_TRISE = 17;  // at 16 MHz

  // enable I2C1
  I2C1_CR1 |= (1 << 0);  // PE = 1
}