#ifndef UART2_H
#define UART2_H

#include <stdint.h>

void uart2_init(void);
void uart2_write_byte(char ch);
void uart2_write_string(const char *str);
void uart2_write_int(int32_t value);
void uart2_write_hex(uint32_t value);
void uart2_write_uint(uint32_t value);
void uart2_printf(const char *fmt, ...);

#endif // UART2_H