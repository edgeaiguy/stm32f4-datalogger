#include <stdint.h>
#include <stdarg.h>
#include "stm32f407xx.h"
#include "fmt.h"
#include "uart2.h"

/* enable UART2 clocks and configure pins */
void uart2_init(void) {
  RCC_AHB1ENR |= (1 << 0); // Set bit 0 in AHB1 bus to enable GPIOA clock. Note: this enables clock for PA0 - PA15
  RCC_APB1ENR |= (1 << 17); // set bit 17 in the APB1 bus to enable USART2 clock
  // Readback after clock enable (force CPU to wait for write to complete)
  volatile unsigned int tmp = RCC_AHB1ENR; (void) tmp; // void tmp tells compiler to ignore unused var

  GPIOA_MODER &= ~(0x3 << 4); // clear bits [5:4] for PA2 (USART2_TX)
  GPIOA_MODER &= ~(0x3 << 6); // clear bits [7:6] for PA3 (USART2_RX)
  GPIOA_MODER |= (0x2 << 4); // set PA2 to 10 (alternate function mode)
  GPIOA_MODER |= (0x2 << 6); // set PA3 to 10 (alternate function mode)

  GPIOA_AFRL &= ~(0xF << 8); // clear bits [11:8] for AFRL2 --> do we need to clear these bits?
  GPIOA_AFRL |= (0x7 << 8); // set AFRL2 to AF7 (USART2_TX)
  GPIOA_AFRL &= ~(0xF << 12); // clear bits [15:12] for AFRL3
  GPIOA_AFRL |= (0x7 << 12); // set AFRL3 to AF7 (USART2_RX)

  USART2_BRR = 0x008B; // USARTDIV = 16 MHz / (16 * 115200) = 8.6805. mantissa = 0x0080, fraction = 0xB (0.6805 * 16). write directly into register
  USART2_CR1 |= (1 << 13) | (1 << 3) | (1 << 2); // enable UE (USART Enable), TE (Transmitter Enable), and RE (Receiver Enable) - RX interrupt itself is armed separately by uart_cmd_init()
}

/* perform UART tranmission: 10 bits per transmission (start/stop bits + 1 byte of data)*/
void uart2_write_byte(char ch) {
  // wait until TXE flag in SR is set (bit 7). then write ch to DR
  while (!(USART2_SR & (1 << 7))) {}
  USART2_DR = (unsigned char)ch;
}

/* loop through each character in a string for UART transmission */
void uart2_write_string(const char *str) {
  // note: *str is 'falsy' when hitting the null terminator ('\0'), ending the loop
  while (*str) {
    uart2_write_byte(*str++);
  }
}

static void emit_to_uart(char c, void *ctx) {
  (void)ctx;
  uart2_write_byte(c);
}

void uart2_printf(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  fmt_vformat(emit_to_uart, NULL, fmt, args);
  va_end(args);
}