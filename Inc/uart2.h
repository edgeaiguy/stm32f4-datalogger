#ifndef UART2_H
#define UART2_H

void uart2_init(void);
void uart2_write_byte(char ch);
void uart2_write_string(const char *str);
void uart2_printf(const char *fmt, ...);

#endif // UART2_H