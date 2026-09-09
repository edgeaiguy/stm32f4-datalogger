# Project 3 — Bare-Metal Multi-Sensor Data Logger

> Three-bus sensor logger on an STM32F407: I2C barometer, SPI accelerometer, and
> an SD card on its own dedicated SPI peripheral, writing timestamped CSV via
> FatFs with a runtime UART command interface. No HAL, no CMSIS.

**Status:** Complete (tag `v3.0`)
**Board:** STM32F407G-DISC1
**Toolchain:** arm-none-eabi-gcc · hand-rolled Makefile · OpenOCD + gdb-multiarch
**No HAL / No CMSIS** — all drivers written at the register level.

---

## What it does

Samples a BMP280 (temperature, pressure) over I2C1 and an ADXL345
(3-axis acceleration) over SPI1 on a fixed tick, then writes every sample as a
row of CSV onto a FAT32 SD card over SPI2. Each row carries both an RTC wall-clock
timestamp for human orientation and a monotonic millisecond counter for ordering.
A USART2 command line runs alongside the sample loop, so sample rate, start/stop,
and live status can be changed at runtime without a reflash. Everything from the
bus drivers up is written directly against the reference manual.

## Hardware

| Component | Interface | Notes |
|-----------|-----------|-------|
| STM32F407G-DISC1 | — | Host MCU, HSI 16 MHz, no PLL |
| BMP280 | I2C1 — PB6 SCL / PB7 SDA (AF4) | Addr `0x76` (SDO→GND), 100 kHz standard mode |
| ADXL345 | SPI1 — PA5 SCK / PA6 MISO / PA7 MOSI (AF5) | CS on **PE2**, mode 3, 500 kHz (fPCLK/32) |
| LIS3DSH (onboard) | SPI1 | CS on **PE3**, **parked high, unused** — see design decisions |
| microSD breakout | SPI2 — PB13 SCK / PB14 MISO / PB15 MOSI (AF5) | CS on **PE4**, 250 kHz init → 4 MHz data |
| USART2 → ST-LINK VCP | PA2 TX / PA3 RX (AF7) | 115200 8N1, console + command line |

Wiring diagram: `docs/wiring.png` · BOM: `docs/bom.md`

## Architecture

![architecture](docs/architecture.png)

**Three buses, deliberately.** The topology is the point of this project. I2C1
carries the BMP280 alone. SPI1 is a genuinely shared bus — the ADXL345 breakout
plus the Discovery's onboard LIS3DSH, which is soldered to the same pins whether
you want it or not. SPI2 carries nothing but the SD card. That third bus is not
symmetry for its own sake; it is the resolution of the project's hardest bug
(below).

**Chip-select topology.** Every slave on a shared bus needs its CS actively
driven high when idle, because a floating CS lets that chip drive MISO against
whoever is actually selected. `spi_init()` configures PE2 (ADXL345) and PE3
(LIS3DSH) as push-pull outputs and parks both high at boot; PE3 is then never
touched again. PE4 (SD) parks high in `spi2_init()` so the card sees its 74
wake-up clocks deselected.

**Bus layer vs. device layer.** `spi.c`/`i2c.c` own the peripherals, the GPIO
alternate-function setup, and the byte-level transfer primitives — nothing in
them knows what a sensor is. `spi.c` drives both SPI peripherals through one
shared `xfer()` core, parameterized by a register pointer, so the SD bus and the
sensor bus share a code path without sharing wires. Above that,
`bmp280.c`/`adxl345.c`/`sdcard.c` know register maps, command sequences, and
compensation math, and call down through the bus API. FatFs sits on top of
`sdcard.c` via a thin `diskio.c` shim.

**Data flow.** `main.c` runs an absolute-deadline tick loop. Each tick reads the
ADXL345 (microsecond-scale SPI) and, on a decimated subset of ticks, the BMP280
(a ~43 ms blocking conversion, so it is held to ~1 Hz regardless of the
accelerometer rate). Samples are packed into a `datalog_row_t` and handed to
`datalog.c`, which formats the CSV line and appends it through FatFs, syncing to
the card every 25 rows. The UART command line is a side channel polled once per
tick, feeding rate/start/stop changes back into the same loop.

## Key design decisions

- **The SD card gets its own SPI peripheral.** The original design shared SPI1
  between the ADXL345 and the card, with CS discipline expected to keep them
  apart. It did not: the breakout's level-shifter drives MISO even while
  deselected, holding the line and corrupting every ADXL345 read that followed SD
  traffic. This is not fixable in firmware — a slave that ignores its own CS
  cannot be multiplexed. Rather than add a bus buffer to work around cheap
  hardware, the card moved to SPI2, where it is the only device. Bring-up asserts
  the fix rather than assuming it: `sdcard_bringup()` re-runs the ADXL345 DEVID
  check *after* SD init, and halts if it fails.

- **The onboard LIS3DSH is parked, not ignored.** On a Discovery board it shares
  SPI1 whether or not the design wants it. Leaving PE3 unconfigured leaves the CS
  floating, which put a second driver on MISO and made ADXL345 reads
  intermittent. Driving PE3 permanently high at init costs two lines and removes
  the contention.

- **Bring-up validates against constants known in advance.** ADXL345 DEVID is
  always `0xE5`; BMP280 chip ID is always `0x58`; SD block 0 ends in `0x55AA`. A
  correct read of a value you already knew proves the whole path — pins, AF
  mapping, mode, bit order — with no external instrumentation. The ADXL345 check
  reads DEVID **five** times, because on a contended bus a single correct byte
  can be luck.

- **`spi_transfer()` drains BSY before returning.** RXNE means the last bit was
  *sampled*, not that the frame is over. Callers raise CS immediately on return,
  and cutting the clock mid-edge desynchronizes the slave's bit counter for the
  *next* frame — a bug that presents as every-other-read corruption.

- **BMP280 in forced mode, decimated in software.** Forced mode takes one
  measurement and returns to sleep, which suits an event-driven logger better
  than free-running normal mode, and makes the IIR filter useless (it only helps
  where consecutive samples can be averaged), so it stays off. The cost is a
  ~43 ms blocking conversion, polled on both `status.measuring` and
  `ctrl_meas.mode` because the former is racy at the start of a conversion. The
  loop therefore drives it at ~1 Hz via `env_decimate`, derived from the
  requested rate so the barometer's cadence stays fixed as the accelerometer's
  changes.

- **Absent samples are written empty, never held.** When the barometer did not
  run on a given tick, the CSV's temperature and pressure fields are written as
  empty cells rather than repeating the last reading. A held value would be
  indistinguishable from a fresh measurement downstream; an empty cell is what
  pandas and Excel already read as missing.

- **Fixed-point end to end, no float path.** Temperature is hundredths of a
  degree C, pressure is Pa in Q24.8 (the datasheet's 64-bit compensation
  variant — the 32-bit one trades ~1 Pa of accuracy for no reason we have), and
  acceleration is integer milli-g from the ±2g full-resolution 256 LSB/g scale.
  Every printed value is split into integer and fraction parts by hand.

- **newlib's `printf`/`snprintf` removed entirely.** Because nothing ever
  formats a float, newlib's formatter was pure size cost — and
  `--specs=nosys.specs` links the full `vfprintf` family rather than the
  float-free `iprintf` variant, so calling `printf` at all pulls in `_dtoa_r`
  regardless of what the format strings ask for. Replaced with `fmt.c`, a
  ~150-line formatter covering exactly the subset used: width, zero-padding,
  signed/unsigned decimal, hex, strings. Both the console output in `main.c` and
  the CSV formatting in `datalog.c` had to move for the saving to be real, since
  `snprintf` shares the same machinery. Result: **44% less flash** (table below).
  Confirmed with `arm-none-eabi-nm` that `vfprintf`, `vfiprintf`, `_dtoa_r`,
  `_svfprintf_r` and `__ssprint_r` are gone from the linked binary. Removing the
  retarget layer also deleted a latent hard fault: `__io_getchar` was declared
  `weak` and never defined, so any call into `_read` would have jumped to
  address 0.

- **`f_sync()` every 25 rows, not every row.** FatFs buffers writes, so nothing
  is on the card until a sync — but a sync rewrites the FAT and directory entry,
  real SD overhead that can stall unpredictably on card housekeeping. Every 25
  rows bounds what a card-yank loses while amortizing that cost across 25
  samples instead of risking a stall that overruns the tick.

- **RTC on LSI, with the drift accepted and stated.** The Discovery has no
  32.768 kHz crystal fitted, so `rtc_init()` tries LSE, times out, and falls back
  to the internal RC oscillator — spec'd at 17–47 kHz, which means the derived
  1 Hz is nominal and the calendar gains roughly an hour a day. That is a
  deliberate trade: `t_ms` carries all relative timing, and the wall clock is for
  ordering and human orientation only. The firmware says so at boot rather than
  presenting a wrong time as authoritative.

- **Sample-rate limit is evidenced, not guessed.** `RATE_HZ_MAX` is 20. At
  115200 baud a printed sample line costs several ms of blocked TX, and
  `datalog_write_row()` periodically pays for an `f_sync()`; both eat a shrinking
  tick budget as the rate climbs. The loop counts overruns so the claim is
  measurable, and going higher would need evidence rather than optimism.

## Build & flash

```sh
make              # → build/stm32f4-datalogger.elf + .bin, prints arm-none-eabi-size
make flash        # openocd -f board/stm32f4discovery.cfg -c "program ... verify reset exit"
make clean
```

**Prerequisites:** `arm-none-eabi-gcc`, `openocd`, `gdb-multiarch`. Built at
`-O0` with hard-float (`-mfpu=fpv4-sp-d16 -mfloat-abi=hard`); the FPU is enabled
in `SystemInit()`, without which the hard-float ABI's instructions hard fault.
FatFs R0.16 vendored under `Lib/fatfs/` (LFN off, code page 437, single volume).

**Gotchas:** the card must be **FAT32** — `datalog_open()` retries visibly rather
than failing silently if the mount does not take. Console is USART2 on the
ST-LINK virtual COM port at 115200 8N1.

**Runtime commands** (type into the serial console):

| Command | Effect |
|---------|--------|
| `rate <n>` | Set sample rate, 1–20 Hz. Barometer stays at ~1 Hz. |
| `start` / `stop` | Resume / pause logging. Sensors keep running while stopped. |
| `status` | State, rate, active filename, overrun count, RTC time. |

**Output:** `LOGnnnnn.CSV` at the card root, probing upward for the first unused
name so a reset never clobbers the previous session. Columns:
`timestamp,t_ms,temp_c,press_hpa,accel_x_mg,accel_y_mg,accel_z_mg`.

## Results

| Metric | Value | How measured |
|--------|-------|--------------|
| Flash footprint | **37200 B** (35840 text + 1360 data) | `arm-none-eabi-size` on `build/*.elf` |
| RAM (static) | 4144 B bss | same |
| Flash saved by `fmt.c` | **29512 B (44%)** | `arm-none-eabi-size`, before/after, same build otherwise |
| Tick overruns | _pending_ | `status` command after a stated run length, at 5 Hz and at 20 Hz |
| Sustained logging run | _pending_ | Rows written / duration / resulting file size on card |
| Bus timing | _pending_ | Logic analyzer captures on I2C1, SPI1, SPI2 |

### newlib removal, before/after

| | text | data | bss | flash (text+data) |
|---|---:|---:|---:|---:|
| before | 64988 | 1724 | 4148 | 66712 B |
| after  | 35840 | 1360 | 4144 | 37200 B |
| **saved** | | | | **29512 B (~28.8 KiB, 44%)** |

- Captures: `docs/*-capture.png` (annotated)
- Proof it ran: `docs/setup.jpg`, `docs/serial-output.png`

> **Assets pending:** `docs/` is not yet populated. Outstanding —
> `architecture.png`, `wiring.png`, `bom.md`, logic analyzer captures for the
> three buses, `setup.jpg`, `serial-output.png`.

## Known limitations

- **Everything blocks.** The BMP280 conversion busy-polls for ~43 ms with the CPU
  doing nothing else, and SD writes block the loop. No sample is ever dropped —
  every tick still reads the accelerometer and writes its row — but a barometer
  tick pushes the following ticks late, which shows up as bunched timestamps and
  an incremented overrun counter. This is precisely what an RTOS restructuring
  buys back, and is the motivation for the next project in the series.

- **`DATALOG_SYNC_ROWS` no longer means what its comment says.** It is a
  compile-time 25, chosen when the tick was a fixed 200 ms (so, 5 s of data). Now
  that the rate is runtime-adjustable, 25 rows is 5 s only at 5 Hz — at 20 Hz it
  is 1.25 s, syncing four times more often than intended. It should be derived
  from the active rate.

- **Wall-clock time is approximate.** LSI drift, roughly an hour a day. Ordering
  and relative timing come from `t_ms`; the timestamp column should not be
  treated as absolute.

- **No CRC checking on SD data transfers.** CRC is sent where the card requires
  it during init (`CMD0`) and ignored thereafter, which SPI mode permits but
  which means a corrupted block read is not detected.

- **Rate ceiling is 20 Hz** and is imposed by console TX plus sync cost, not by
  either sensor. Dropping the per-sample console line would raise it
  considerably.

- **The LIS3DSH is wired but unused.** It is held deselected, not driven. A
  second accelerometer on an already-proven bus is the obvious next sensor.

- **Single log file per session, no rotation, no free-space check.** A full card
  surfaces as a short write, reported but not recovered from.

## References

- **RM0090** — STM32F4 reference manual (I2C, SPI, RTC, USART, GPIO/AF)
- **UM1472** — STM32F4 Discovery user manual (onboard LIS3DSH, pin usage)
- **DS8626** — STM32F407 datasheet (alternate function tables)
- **BMP280 datasheet** (Bosch) — §3.11.3 compensation formulas, §3.8.1 conversion times
- **ADXL345 datasheet** (Analog Devices) — SPI framing, `DATA_FORMAT`, `POWER_CTL`
- **SD Physical Layer Simplified Specification** — SPI mode command set, init sequence
- **FatFs** R0.16 by ChaN — <http://elm-chan.org/fsw/ff/>
