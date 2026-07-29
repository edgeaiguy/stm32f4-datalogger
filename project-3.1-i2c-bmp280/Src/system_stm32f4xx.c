#include "stm32f407xx.h"

/* Called by Reset_Handler before __libc_init_array and main.
 * The startup file declares this .weak, so if we don't define it the call is
 * silently replaced with a nop. */
void SystemInit(void) {
  /* The FPU is disabled at reset. We compile with -mfloat-abi=hard, so both our
   * code and newlib contain FPU instructions - executing one with CP10/CP11
   * disabled raises a NOCP UsageFault that escalates to HardFault. */
  SCB_CPACR |= SCB_CPACR_FPU_EN;

  /* Clock setup stays at reset default: HSI, 16 MHz, no PLL.
   * uart2_init() and i2c_init() both assume a 16 MHz APB1. */
}
