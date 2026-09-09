#ifndef UART_CMD_H
#define UART_CMD_H

#include <stdint.h>

typedef enum {
    UART_CMD_RATE,     /* arg = requested sample rate in Hz */
    UART_CMD_START,
    UART_CMD_STOP,
    UART_CMD_STATUS,
} uart_cmd_id_t;

typedef struct {
    uart_cmd_id_t id;
    int32_t       arg;
} uart_cmd_t;

/* Enables the USART2 receiver interrupt and its NVIC line. Call once, after
 * uart2_init(). */
void uart_cmd_init(void);

/* Drains whatever bytes have arrived since the last call and assembles them
 * into command lines. Non-blocking: returns immediately if nothing is
 * pending. Call once per main-loop iteration.
 *
 * Returns 1 and fills *out when a complete, recognised command line was
 * parsed. Returns 0 otherwise - nothing arrived, the line is still being
 * typed, or the line was blank / malformed / unrecognised (the parser has
 * already printed feedback for those cases, so the caller has nothing to do). */
int uart_cmd_poll(uart_cmd_t *out);

#endif // UART_CMD_H
