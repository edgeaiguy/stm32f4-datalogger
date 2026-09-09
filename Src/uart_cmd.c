/* uart_cmd.c — interrupt-driven UART command line for runtime configuration.
 *
 * The ISR only ever copies one byte into a ring buffer; it never blocks and
 * never touches TX. Everything that can wait — echo, line assembly, parsing,
 * printing responses — happens in uart_cmd_poll(), called from the main loop.
 * That split is the whole design: an ISR that calls a blocking TX write (as
 * uart2_write_byte() does, spinning on TXE) would stall every interrupt at
 * its priority or above for however long the terminal takes to drain.
 */
#include <stdint.h>
#include "stm32f407xx.h"
#include "uart2.h"
#include "uart_cmd.h"

#define RXBUF_SIZE  64u   /* power of two, so wraparound is a mask not a mod */
#define LINE_MAX    40u   /* generous for "rate 4294967295" plus margin */

/* Single-producer (ISR), single-consumer (uart_cmd_poll) ring buffer. Each
 * side only ever writes its own index and reads the other's, so this needs no
 * locking on a single core with no cache — the volatile qualifiers are enough
 * to stop the compiler from caching either index across the ISR/poll boundary
 * (and the project builds at -O0, so there is no reordering risk to reason
 * about even in principle). */
static volatile uint8_t  rx_buf[RXBUF_SIZE];
static volatile uint32_t rx_head;      /* written only by the ISR */
static volatile uint32_t rx_tail;      /* written only by uart_cmd_poll() */
static volatile uint32_t rx_dropped;   /* bytes lost because the ring was full */

static char     line[LINE_MAX];
static uint32_t line_len;

void uart_cmd_init(void) {
    USART2_CR1 |= (1 << 5);            /* RXNEIE: interrupt on RX-not-empty */
    NVIC_ISER1 |= (1U << (38 - 32));   /* IRQ38 = USART2, lives in ISER1 (IRQ32-63) */
}

void USART2_IRQHandler(void) {
    if (USART2_SR & (1 << 5)) {        /* RXNE */
        /* Reading DR fetches the byte and clears RXNE (and ORE, if it was
         * set — RM0090: ORE clears on an SR-then-DR read, which checking the
         * flag above and reading DR here already performs). */
        uint8_t byte = (uint8_t)USART2_DR;
        uint32_t next_head = (rx_head + 1) & (RXBUF_SIZE - 1);
        if (next_head != rx_tail) {
            rx_buf[rx_head] = byte;
            rx_head = next_head;
        } else {
            rx_dropped++;             /* consumer isn't keeping up; drop, don't block */
        }
    }
}

static int word_is(const char *word, uint32_t len, const char *kw) {
    uint32_t i = 0;
    while (kw[i]) {
        if (i >= len || word[i] != kw[i]) return 0;
        i++;
    }
    return i == len;
}

/* Decimal only, no sign, no leading/trailing junk - deliberately strict so a
 * typo produces a clear "usage" message instead of a silently wrong number. */
static int parse_uint(const char *s, uint32_t *out) {
    if (*s < '0' || *s > '9') return 0;
    uint32_t v = 0;
    while (*s >= '0' && *s <= '9') {
        v = v * 10 + (uint32_t)(*s - '0');
        s++;
    }
    if (*s != '\0') return 0;
    *out = v;
    return 1;
}

static void print_help(void) {
    uart2_write_string("commands:\r\n"
                        "  rate <hz>   - set sample rate\r\n"
                        "  start       - resume logging\r\n"
                        "  stop        - pause logging (sensors keep running)\r\n"
                        "  status      - print current state\r\n");
}

/* Tokenizes "<keyword> [arg]" out of a NUL-terminated, already-trimmed-of-CR/LF
 * line, and either fills *out and returns 1 (an actionable command), or
 * handles the line itself (help, blank, malformed, unrecognised) and returns
 * 0. Syntax feedback belongs here, next to the grammar it is checking;
 * whether a value is in range is a semantic question for the caller. */
static int handle_line(const char *ln, uart_cmd_t *out) {
    const char *p = ln;
    while (*p == ' ') p++;
    if (*p == '\0') return 0;                    /* blank line: ignore silently */

    const char *kw = p;
    uint32_t kw_len = 0;
    while (kw[kw_len] && kw[kw_len] != ' ') kw_len++;
    p += kw_len;
    while (*p == ' ') p++;                        /* p now points at the argument, if any */

    if (word_is(kw, kw_len, "rate")) {
        uint32_t v;
        if (*p == '\0' || !parse_uint(p, &v)) {
            uart2_write_string("usage: rate <hz>\r\n");
            return 0;
        }
        out->id  = UART_CMD_RATE;
        out->arg = (int32_t)v;
        return 1;
    }
    if (word_is(kw, kw_len, "start"))  { out->id = UART_CMD_START;  return 1; }
    if (word_is(kw, kw_len, "stop"))   { out->id = UART_CMD_STOP;   return 1; }
    if (word_is(kw, kw_len, "status")) { out->id = UART_CMD_STATUS; return 1; }
    if (word_is(kw, kw_len, "help") || word_is(kw, kw_len, "?")) {
        print_help();
        return 0;
    }

    uart2_write_string("unknown command (try 'help')\r\n");
    return 0;
}

int uart_cmd_poll(uart_cmd_t *out) {
    if (rx_dropped) {
        uart2_printf("\r\n(dropped %u byte(s), input buffer was full)\r\n",
                      (unsigned)rx_dropped);
        rx_dropped = 0;
    }

    while (rx_tail != rx_head) {
        uint8_t byte = rx_buf[rx_tail];
        rx_tail = (rx_tail + 1) & (RXBUF_SIZE - 1);

        if (byte == '\r' || byte == '\n') {
            if (line_len == 0) continue;          /* swallow the \n half of \r\n */
            uart2_write_string("\r\n");
            line[line_len] = '\0';
            int got = handle_line(line, out);
            line_len = 0;
            if (got) return 1;
            continue;
        }

        if (byte == 0x7F || byte == '\b') {        /* DEL or backspace */
            if (line_len > 0) {
                line_len--;
                uart2_write_string("\b \b");       /* erase the character visually */
            }
            continue;
        }

        if (byte < 0x20 || byte > 0x7E) continue;  /* drop other control bytes */

        if (line_len < LINE_MAX - 1) {
            line[line_len++] = (char)byte;
            uart2_write_byte((char)byte);          /* local echo */
        } else {
            uart2_write_string("\r\n(line too long, discarded)\r\n");
            line_len = 0;
        }
    }
    return 0;
}
