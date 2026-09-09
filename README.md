# stm32f4-datalogger
Multi-sensor data logger on STM32F4 — BMP280 (I2C), LIS3DH (SPI), SD card logging, RTC timestamps, UART command interface. Bare-metal, no HAL.

## Design notes

### Removing newlib's printf/snprintf

Every value this firmware logs is already run through a manual integer/fraction
split (temperature, pressure, milli-g) specifically so nothing needs float
formatting. That made newlib's `printf`/`snprintf` — and the `_write`/`__io_putchar`
retargeting layer wired up early in the project to back them — pure size cost
for a capability never used. Replaced with a ~150-line hand-rolled formatter
(`fmt.c`) supporting exactly the subset actually printed: width, zero-padding,
signed/unsigned decimal, hex, strings — no floats, no locale, no variadic
surface beyond what's called.

Measured with `arm-none-eabi-size`, same build otherwise:

| | text | data | bss | flash (text+data) |
|---|---:|---:|---:|---:|
| before | 64988 | 1724 | 4148 | 66712 B |
| after  | 35840 | 1360 | 4144 | 37200 B |
| **saved** | | | | **29512 B (~28.8 KiB, 44%)** |

Two things worth stating plainly rather than leaving implicit:

- **Nothing in this firmware ever printed a float**, yet `_dtoa_r` — newlib's
  float-to-string converter — was linked into the *original* build anyway.
  `--specs=nosys.specs` links the full `vfprintf` family rather than the
  float-free `iprintf` variant, so calling `printf`/`snprintf` at all pulls in
  float support regardless of what format strings actually ask for.
- **`snprintf` shares the same formatter as `printf`.** Both `main.c`'s console
  output and the CSV row/filename formatting in `datalog.c` had to move onto
  the replacement for the number above to be honest — removing only one and
  leaving the other would have kept the shared machinery linked in and
  understated the real cost.

Confirmed post-change with `arm-none-eabi-nm`: `vfprintf`, `vfiprintf`,
`_dtoa_r`, `_svfprintf_r`, and `__ssprint_r` no longer appear in the linked
binary. A nice side effect of removing the retargeting layer: `__io_getchar`
had been declared `weak` but never defined anywhere in the codebase — had
anything ever actually called `_read` (nothing did), it would have jumped to
address 0 and hard-faulted. That latent landmine is gone along with the size.
