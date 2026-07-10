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

int i2c_start(void) {
  // Send start condition
  I2C1_CR1 |= (1 << 8);  // START = 1

  uint32_t timeout = 10000;
  while (!(I2C1_SR1 & (1 << 0))) {  // wait for SB flag
      if (--timeout == 0) return -1;
  }
  return 0;
}

void i2c_stop(void) {
  // Send stop condition
  I2C1_CR1 |= (1 << 9);  // STOP = 1
}

int i2c_send_address(uint8_t addr, uint8_t rw) {
  // Send address with R/W bit
  I2C1_DR = (addr << 1) | (rw & 0x1);  // LSB is R/W bit

  uint32_t timeout = 10000;
  while (!(I2C1_SR1 & (1 << 1))) {  // wait for ADDR flag
      if (--timeout == 0) return -1;
  }
  // Clear ADDR flag by reading SR1 and SR2
  volatile uint32_t temp = I2C1_SR1;
  temp = I2C1_SR2;
  (void) temp; // prevent unused variable warning
  return 0;
}

int i2c_write_byte(uint8_t data) {
  // Send data byte
  I2C1_DR = data;

  uint32_t timeout = 10000;
  while (!(I2C1_SR1 & (1 << 7))) {  // wait for TxE flag
      if (--timeout == 0) return -1;
  }
  return 0;
}

int i2c_read_byte(uint8_t *data, int ack) {
  if (ack) {
    I2C1_CR1 |= (1 << 10);  // ACK = 1
  } else {
    I2C1_CR1 &= ~(1 << 10); // ACK = 0
  }

  uint32_t timeout = 10000;
  while (!(I2C1_SR1 & (1 << 6))) {  // wait for RxNE flag
      if (--timeout == 0) return -1;
  }
  *data = I2C1_DR; // read received byte
  return 0;
}

int i2c_write_register(uint8_t dev_addr, uint8_t reg_addr, uint8_t data) {
    if (i2c_start() != 0) return -1;
    if (i2c_send_address(dev_addr, 0) != 0) return -1;  // 0 = write
    if (i2c_write_byte(reg_addr) != 0) return -1;
    if (i2c_write_byte(data) != 0) return -1;
    i2c_stop();
    return 0;
}

int i2c_read_register(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data) {
    if (i2c_start() != 0) return -1;
    if (i2c_send_address(dev_addr, 0) != 0) return -1;  // write phase
    if (i2c_write_byte(reg_addr) != 0) return -1;

    if (i2c_start() != 0) return -1;                    // repeated start
    if (i2c_send_address(dev_addr, 1) != 0) return -1;  // read phase
    if (i2c_read_byte(data, 0) != 0) return -1;         // NACK = last byte
    i2c_stop();
    return 0;
}