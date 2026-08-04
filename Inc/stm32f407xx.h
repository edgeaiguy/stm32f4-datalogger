// NOTE: this file is cheap to include. Include at will

/* Clock enable */
#define RCC_BASE  0x40023800UL // RCC base address. Note: end all hex address defines with UL (unsigned long)
#define RCC_AHB1ENR  (*(volatile unsigned int *)(RCC_BASE + 0x30)) // AHB1ENR is at offset 0x30. GPIOs lives here
#define RCC_AHB1ENR_GPIOBEN (1U << 1) // GPIOBEN bit is bit 1 in AHB1ENR
#define RCC_AHB1ENR_GPIOEEN (1U << 4) // GPIOEEN bit is bit 4 in AHB1ENR
#define RCC_APB1ENR (*(volatile unsigned int *)(RCC_BASE + 0x40)) // APB1ENR is at offset 0x40. UART2/I2C1 lives here.
#define RCC_APB1ENR_I2C1EN  (1U << 21) // I2C1EN bit is bit 21 in APB1ENR
#define RCC_APB2ENR (*(volatile unsigned int *)(RCC_BASE + 0x44)) // APB2ENR is at offset 0x44. SYSCFG lives here
#define RCC_APB2ENR_SPI1EN (1U << 12) // SPI1EN bit is bit 12 in APB2ENR

/* GPIOA */
#define GPIOA_BASE 0x40020000UL // GPIOA base address. PA2 and PA3 (for UART comms) live here, also PA0 blue button
#define GPIOA_MODER (*(volatile unsigned int *)(GPIOA_BASE + 0x00)) //GPIOA_MODER is the first register in GPIOA
#define GPIOA_AFRL (*(volatile unsigned int *)(GPIOA_BASE + 0x20)) // also define the alternate function low register offset
#define GPIOA_PUPDR (*(volatile unsigned int *)(GPIOA_BASE + 0x0C)) // pull-up pull-down register (for button)
/* GPIOB */
#define GPIOB_BASE 0x40020400UL // GPIOB base address. PB6 and PB7 (for I2C comms) live here
#define GPIOB_MODER (*(volatile unsigned int *)(GPIOB_BASE + 0x00)) // GPIOB_MODER is the first register in GPIOB
#define GPIOB_OTYPER  (*(volatile unsigned int *)(GPIOB_BASE + 0x04))
#define GPIOB_OSPEEDR (*(volatile unsigned int *)(GPIOB_BASE + 0x08))
#define GPIOB_PUPDR   (*(volatile unsigned int *)(GPIOB_BASE + 0x0C))
#define GPIOB_AFRL (*(volatile unsigned int *)(GPIOB_BASE + 0x20)) // also define the alternate function low register offset
/* GPIOD */
#define GPIOD_BASE  0x40020C00UL // GPIOD base address (from memory map). LEDs live here. UL = unsigned long, compiler treats it as 32-bit value rather than signed integer
#define GPIOD_MODER (*(volatile unsigned int *)(GPIOD_BASE + 0x00))// GPIOD_MODER is the first register in GPIOD
#define GPIOD_ODR (*(volatile unsigned int *)(GPIOD_BASE + 0x14)) // GPIOD_ODR (output data register) is at offset 0x14
#define GPIOD_BSRR (*(volatile unsigned int *)(GPIOD_BASE + 0x18)) // GPIOD_BSRR (bit set/reset register) is at offset 0x18
#define GPIOD_AFRH (*(volatile unsigned int *)(GPIOD_BASE + 0x24)) // alternate function high (GPIOD pins 8-15) register offset
/* ---- GPIOE (AHB1, base 0x4002_1000) ---- */
#define GPIOE_BASE      0x40021000UL
#define GPIOE_MODER     (*(volatile uint32_t *)(GPIOE_BASE + 0x00))
#define GPIOE_OTYPER    (*(volatile uint32_t *)(GPIOE_BASE + 0x04))
#define GPIOE_OSPEEDR   (*(volatile uint32_t *)(GPIOE_BASE + 0x08))
#define GPIOE_PUPDR     (*(volatile uint32_t *)(GPIOE_BASE + 0x0C))
#define GPIOE_IDR       (*(volatile uint32_t *)(GPIOE_BASE + 0x10))
#define GPIOE_ODR       (*(volatile uint32_t *)(GPIOE_BASE + 0x14))
#define GPIOE_BSRR      (*(volatile uint32_t *)(GPIOE_BASE + 0x18))

/* USART2 */
#define USART2_BASE 0x40004400UL // USART2 base address (from memory map)
#define USART2_BRR (*(volatile unsigned int *)(USART2_BASE + 0x08)) // BRR (baud rate register)
#define USART2_CR1 (*(volatile unsigned int *)(USART2_BASE + 0x0C)) // CR1 (control register 1)
#define USART2_SR (*(volatile unsigned int *)(USART2_BASE + 0x00)) // SR (status register)
#define USART2_DR (*(volatile unsigned int *)(USART2_BASE + 0x04)) // DR (data register)

/* SYSCFG */
#define SYSCFG_BASE 0x40013800UL // SYSCFG base address
#define SYSCFG_EXTICR1 (*(volatile unsigned int *)(SYSCFG_BASE + 0x08)) // EXTICR1 (external interrupt control register)
/* EXTI */
#define EXTI_BASE 0x40013C00UL // EXTI base address
#define EXTI_IMR (*(volatile unsigned int *)(EXTI_BASE + 0x00)) // interrupt mask register
#define EXTI_RTSR (*(volatile unsigned int *)(EXTI_BASE + 0x08)) // rising trigger selection register
#define EXTI_FTSR (*(volatile unsigned int *)(EXTI_BASE + 0x0C)) // falling trigger selection register
#define EXTI_PR (*(volatile unsigned int *)(EXTI_BASE + 0x14)) // pending register

/* NVIC & SysTick: found in programming manual */
#define NVIC_BASE 0xE000E100UL // NVIC base address --> in 'System Control Space', same for all Cortex-M4 chips
#define NVIC_ISER0 (*(volatile unsigned int *)(NVIC_BASE + 0x00)) // interrupt set enable register
#define NVIC_IPR (*(volatile unsigned int *)(NVIC_BASE + 0x300)) // interrupt priority register
#define SYSTICK_BASE 0xE000E010UL // SysTack base address
#define STK_CTRL (*(volatile unsigned int *)(SYSTICK_BASE + 0x00)) // SysTick control and status register
#define STK_LOAD (*(volatile unsigned int *)(SYSTICK_BASE + 0x04)) // SysTick reload value register
#define STK_VAL (*(volatile unsigned int *)(SYSTICK_BASE + 0x08)) // SysTick current value register

/* TIMERS */
#define TIM2_BASE 0x40000000UL // timer 2
#define TIM2_PSC (*(volatile unsigned int *)(TIM2_BASE + 0x28)) // TIM2 prescaler register
#define TIM2_ARR (*(volatile unsigned int *)(TIM2_BASE + 0x2C)) // TIM2 auto-reload register
#define TIM2_CCR1 (*(volatile unsigned int *)(TIM2_BASE + 0x34)) // TIM2 capture/compare register 1
#define TIM2_CCMR1 (*(volatile unsigned int *)(TIM2_BASE + 0x18)) // TIM2 capture/compare mode register 1
#define TIM2_CCER (*(volatile unsigned int *)(TIM2_BASE + 0x20)) // TIM2 capture/compare enable register
#define TIM2_CR1 (*(volatile unsigned int *)(TIM2_BASE + 0x00)) // TIM2 control register 1
#define TIM2_CR2 (*(volatile unsigned int *)(TIM2_BASE + 0x04)) // TIM2 control register 1
#define TIM2_CNT (*(volatile unsigned int *)(TIM2_BASE + 0x24))
#define TIM4_BASE 0x40000800UL // timer 4
#define TIM4_PSC (*(volatile unsigned int *)(TIM4_BASE + 0x28)) // TIM4 prescaler register
#define TIM4_ARR (*(volatile unsigned int *)(TIM4_BASE + 0x2C)) // TIM4 auto-reload register
#define TIM4_CCR1 (*(volatile unsigned int *)(TIM4_BASE + 0x34)) // TIM4 capture/compare register 1
#define TIM4_CCMR1 (*(volatile unsigned int *)(TIM4_BASE + 0x18)) // TIM4 capture/compare mode register 1
#define TIM4_CCER (*(volatile unsigned int *)(TIM4_BASE + 0x20)) // TIM4 capture/compare enable register
#define TIM4_CR1 (*(volatile unsigned int *)(TIM4_BASE + 0x00)) // TIM4 control register 1

/* ADC */
#define ADC1_BASE 0x40012000UL // analog 2 digital converter 1
#define ADC1_SQR3 (*(volatile unsigned int *)(ADC1_BASE + 0x34)) // regular sequence register 3
#define ADC1_SQR1 (*(volatile unsigned int *)(ADC1_BASE + 0x2C)) // regular sequence register 1
#define ADC1_SMPR2 (*(volatile unsigned int *)(ADC1_BASE + 0x10)) // sample time register
#define ADC1_CR1 (*(volatile unsigned int *)(ADC1_BASE + 0x04)) // control register 1
#define ADC1_CR2 (*(volatile unsigned int *)(ADC1_BASE + 0x08)) // control register 2
#define ADC1_SR (*(volatile unsigned int *)(ADC1_BASE + 0x00)) // status register
#define ADC1_DR (*(volatile unsigned int *)(ADC1_BASE + 0x4C)) // data register

/* SCB (System Control Block) - part of the Cortex-M4 core, not a peripheral */
#define SCB_BASE 0xE000ED00UL // SCB base address
#define SCB_CPACR (*(volatile unsigned int *)(SCB_BASE + 0x88)) // coprocessor access control register (FPU enable)
#define SCB_CPACR_FPU_EN (0xFU << 20) // full access to CP10 and CP11 (the FPU)

/* SysTick - also part of the Cortex-M4 core, described in the programming manual
 * (PM0214) rather than the reference manual */
#define SYSTICK_BASE 0xE000E010UL // SysTick base address
#define STK_CTRL (*(volatile unsigned int *)(SYSTICK_BASE + 0x00)) // control and status register
#define STK_LOAD (*(volatile unsigned int *)(SYSTICK_BASE + 0x04)) // reload value register
#define STK_VAL  (*(volatile unsigned int *)(SYSTICK_BASE + 0x08)) // current value register

#define STK_CTRL_ENABLE    (1U << 0) // start counting
#define STK_CTRL_TICKINT   (1U << 1) // raise the SysTick exception when the counter reloads
#define STK_CTRL_CLKSOURCE (1U << 2) // 1 = processor clock, 0 = processor clock / 8

/* I2C */
#define I2C1_BASE 0x40005400UL // I2C1 base address
#define I2C1_CR1 (*(volatile unsigned int *)(I2C1_BASE + 0x00)) // I2C1 control register 1
#define I2C1_CR2 (*(volatile unsigned int *)(I2C1_BASE + 0x04)) // I2C1 control register 2
#define I2C1_OAR1 (*(volatile unsigned int *)(I2C1_BASE + 0x08)) // I2C1 own address register 1
#define I2C1_OAR2 (*(volatile unsigned int *)(I2C1_BASE + 0x0C)) // I2C1 own address register 2
#define I2C1_DR (*(volatile unsigned int *)(I2C1_BASE + 0x10)) // I2C1 data register
#define I2C1_SR1 (*(volatile unsigned int *)(I2C1_BASE + 0x14)) // I2C1 status register 1
#define I2C1_SR2 (*(volatile unsigned int *)(I2C1_BASE + 0x18)) // I2C1 status register 2
#define I2C1_CCR (*(volatile unsigned int *)(I2C1_BASE + 0x1C)) // I2C1 clock control register
#define I2C1_TRISE (*(volatile unsigned int *)(I2C1_BASE + 0x20)) // I2C1 TRISE register
#define I2C1_FLTR (*(volatile unsigned int *)(I2C1_BASE + 0x24)) // I2C1 FLTR register

/* I2C1_CR1 bits */
#define I2C_CR1_PE    (1U << 0)  // peripheral enable
#define I2C_CR1_START (1U << 8)  // generate start condition
#define I2C_CR1_STOP  (1U << 9)  // generate stop condition
#define I2C_CR1_ACK   (1U << 10) // 1 = ACK the received byte, 0 = NACK
#define I2C_CR1_POS   (1U << 11) // 0 = ACK bit applies to the byte in the shift register, 1 = to the next one

/* I2C1_SR1 bits */
#define I2C_SR1_SB    (1U << 0)  // start condition generated
#define I2C_SR1_ADDR  (1U << 1)  // address sent and acknowledged
#define I2C_SR1_BTF   (1U << 2)  // byte transfer finished (DR and shift register both full)
#define I2C_SR1_RXNE  (1U << 6)  // DR holds a received byte
#define I2C_SR1_TXE   (1U << 7)  // DR is empty and ready for the next byte
#define I2C_SR1_AF    (1U << 10) // acknowledge failure: the slave did not respond

/* I2C1_SR2 bits */
#define I2C_SR2_BUSY  (1U << 1)  // a transfer is in progress on the bus

/* ---- SPI1 (APB2, base 0x4001_3000) ---- */
#define SPI1_BASE       0x40013000UL
#define SPI1_CR1        (*(volatile uint32_t *)(SPI1_BASE + 0x00))
#define SPI1_CR2        (*(volatile uint32_t *)(SPI1_BASE + 0x04))
#define SPI1_SR         (*(volatile uint32_t *)(SPI1_BASE + 0x08))
#define SPI1_DR         (*(volatile uint32_t *)(SPI1_BASE + 0x0C))