# Project name
TARGET = awc-pwm-timer

# Toolchain
CC = arm-none-eabi-gcc
AS = arm-none-eabi-gcc
LD = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE = arm-none-eabi-size

# Sources
C_SOURCES = Src/main.c Src/syscalls.c Src/sysmem.c Src/uart2.c
ASM_SOURCES = Startup/startup_stm32f407vgtx.s

# Linker script
LDSCRIPT = STM32F407VGTX_FLASH.ld

# MCU flags
MCU = -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard

# Compiler flags
CFLAGS = $(MCU) -Wall -g -O0 -std=gnu11
CFLAGS += -DSTM32F407xx -I./Inc

# Linker flags
LDFLAGS = $(MCU) -T$(LDSCRIPT) --specs=nosys.specs -Wl,-Map=build/$(TARGET).map

# Build directory
BUILD_DIR = build

# Object files
C_OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
ASM_OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(ASM_SOURCES:.s=.o)))

all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).bin
	$(SIZE) $(BUILD_DIR)/$(TARGET).elf

$(BUILD_DIR)/%.o: Src/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: Startup/%.s | $(BUILD_DIR)
	$(AS) $(MCU) -c $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(C_OBJECTS) $(ASM_OBJECTS)
	$(LD) $(LDFLAGS) $^ -o $@

$(BUILD_DIR)/$(TARGET).bin: $(BUILD_DIR)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

flash: $(BUILD_DIR)/$(TARGET).elf
	openocd -f board/stm32f4discovery.cfg -c "program $< verify reset exit"

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all flash clean
