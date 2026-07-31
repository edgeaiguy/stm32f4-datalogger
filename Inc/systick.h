#ifndef SYSTICK_H
#define SYSTICK_H

#include <stdint.h>

/* Start a 1 ms tick. Assumes the core is running at 16 MHz (HSI, no PLL) - the
 * reload value has to change if the clock does. */
void systick_init(void);

/* Milliseconds since systick_init(). Wraps after ~49.7 days; compare with
 * unsigned subtraction (now - then) and the wrap takes care of itself. */
uint32_t systick_millis(void);

/* Block for at least ms milliseconds. */
void delay_ms(uint32_t ms);

#endif // SYSTICK_H
