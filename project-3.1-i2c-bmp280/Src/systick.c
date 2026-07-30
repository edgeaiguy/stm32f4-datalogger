#include <stdint.h>
#include "stm32f407xx.h"
#include "systick.h"

/* Written by the interrupt, read by main. volatile so the compiler reloads it
 * from memory each time instead of caching it in a register - without it, the
 * spin loop in delay_ms() would read a stale copy forever. */
static volatile uint32_t ms_ticks = 0;

void systick_init(void) {
  // 16 MHz / 16000 = 1 kHz. LOAD is the reload value, so it counts LOAD+1 cycles.
  STK_LOAD = 15999;
  STK_VAL = 0;  // clear the current value so the first period is a full 1 ms
  STK_CTRL = STK_CTRL_CLKSOURCE | STK_CTRL_TICKINT | STK_CTRL_ENABLE;
}

/* Overrides the weak alias to Default_Handler in the startup file. The vector
 * table entry already points here - no NVIC setup needed, SysTick is a core
 * exception rather than a peripheral IRQ. */
void SysTick_Handler(void) {
  ms_ticks++;
}

uint32_t systick_millis(void) {
  return ms_ticks;  // a single aligned 32-bit load, so no tearing to guard against
}

void delay_ms(uint32_t ms) {
  uint32_t start = systick_millis();
  // <= rather than < because we do not know how far into the current millisecond
  // we started: a plain < would return up to 1 ms early, which matters when this
  // is used for hardware settling times.
  while ((systick_millis() - start) <= ms) {}
}
