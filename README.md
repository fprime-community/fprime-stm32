# F´ STM32H7 Platform Support

This repository contains the STM32H7 hardware, OSAL, and driver support used by the
bare-metal F´ reference deployment. It targets the STM32H753XI-EVAL2 evaluation
board and is intended to be reusable as a Git submodule.

The implementation is designed for a bare-metal cyclic executive:

- No FreeRTOS or other operating system is required.
- `Os::Task` is cooperative and creates no thread.
- `Os::Mutex` provides bounded critical sections by preserving and restoring
  Cortex-M7 `PRIMASK`.
- `Os::Queue` is a fixed-depth, interrupt-safe FIFO allocated during startup.
- `Os::RawTime` provides a race-safe microsecond clock from a free-running TIM2
  counter and overflow interrupt.
- DMA buffers are placed in DMA-accessible AXI SRAM rather than DTCM.

## Supported hardware

The current configuration is for the STM32H753XIH6 on the
STM32H753XI-EVAL2:

- Cortex-M7, 2 MiB Flash, 1 MiB RAM
- 25 MHz HSE input and a 480 MHz PLL1 system clock
- USART1 through the embedded ST-LINK-V3E VCP
- USART1 TX on PB14 and RX on PB15
- Direct user LEDs on PF10 (LED1, green) and PA4 (LED3, red)
- TIM2 as the 1 MHz, 32-bit free-running time source

LED2 and LED4 are routed through the external MFX I2C expander and are not
used by the direct bare-metal GPIO examples.

## Contents

- `Os/Stm32H7`: STM32H7 bare-metal delegates for Task, Mutex, Queue, and RawTime
- `Drv/STM32GpioDriver`: passive GPIO input/output driver
- `Drv/STM32Timer`: channel-2 output-compare tick driver, instance-selectable
  (`Stm32::TimerInstance::Tim1`/`Tim2`/`Tim3`/`Tim4`/`...`) via `open()`
- `Drv/STM32UartDriver`: USART1 DMA-backed byte-stream driver
- `Drv/STM32I2cDriver`: blocking/polled I2C master driver (`Drv.I2c`)
- `Drv/STM32SpiDriver`: blocking/polled SPI master driver (`Drv.Spi`), no
  DMA/interrupts, multi-instance-safe (different bus + chip-select per instance)
- `Drv/config`: driver-tuning headers (e.g. `UartDriverConfig.hpp`) and
  `Stm32Config.hpp`, the per-peripheral-instance enable/disable switchboard
  used by every driver below (see "Enabling and selecting peripheral
  instances")
- `Allocator`: fixed-pool bootstrap allocator + newlib `--wrap` traps
  enforcing the no-heap-after-bootstrap rule -- fully hardware-agnostic, so
  every project gets it without hand-rolling one
- `Core/CortexM7`: ARM-core-level helpers (D-cache maintenance and the
  DMA-safe-buffer assertion), grouped by core rather than by ST family since
  they only depend on which Cortex-M core is in use. The next family added
  (e.g. STM32F4, Cortex-M4) needs its own `Core/CortexM4` with the same
  function names and no-op bodies -- see "Adding a new chip family" below.

This library is hardware-agnostic: it contains no CubeMX-generated code and
no board-specific source. It only expects a CMake target named `FprimeStm32`
to already exist -- built and exposed by the *consuming* project, along with
its public include paths for `main.h`, `stm32h7xx_hal_conf.h`, and the rest
of the CubeMX/HAL headers. In this repository that target is defined in
`FprimeStm32BaremetalReference/Hardware/CMakeLists.txt`, which builds the actual
CubeMX-generated project (regenerable in place from its own `.ioc` file) plus
a handful of hand-written, project-specific clock/tick-source glue (which
timer backs `Os::RawTime`, the exact PLL/oscillator sequence -- these are
peripheral-*role* choices baked into one board's `.ioc`, not family-wide
constants, so they stay project-owned even though they rarely change). This
split means retargeting this library to different hardware never requires
forking it -- only editing the consuming project's own `Hardware/` directory.
The real interrupt implementation (`stm32h7xx_it.c`) must be linked directly
into each deployment executable, rather than only through the `FprimeStm32`
static library, so its strong handlers override the startup file's weak
`Default_Handler` aliases -- see the NOTE in
`FprimeStm32BaremetalReference/Hardware/CMakeLists.txt`.

### Enabling and selecting peripheral instances

`Stm32UartDriver`, `Stm32I2cDriver`, and `STM32Timer` all resolve their
peripheral instance (`USART1_UART_INSTANCE`, `I2C1_INSTANCE`, `TIM2_INSTANCE`,
...) through `#define`s in `Drv/config/Stm32Config.hpp`, each defaulting to
`false` except the instances this reference board already uses. A driver's
`toHalHandle(instance)` returns `nullptr` for a `false` instance, and the
calling code immediately `FW_ASSERT`s on that `nullptr` — selecting a
disabled instance in an `open()` call is a build-time configuration mistake,
not a runtime condition to gracefully handle.

The library's copy of `Stm32Config.hpp` is the default; a consuming project
overrides it by placing its own copy at the same relative path under its
`settings.ini`-configured `config_directory` (this project's override lives
at `FprimeStm32BaremetalReference/config/fprime-stm32/Stm32Config.hpp`). When
retargeting this library to a new board, edit only the override copy — enable
the instances your `.ioc` actually configured, and pass the matching enum
value (`Stm32::I2cInstance::I2c1`, `Stm32::TimerInstance::Tim2`, ...) to the
driver's `open()` call from the topology's `configureTopology()`. Each
sensor's own `docs/sdd.md` under `lib/fprime-sensors` documents this
enable-then-select pairing for its specific port.

Enabling more than one instance of the same macro (e.g. both
`USART1_UART_INSTANCE` and `USART2_UART_INSTANCE`) and adding a second
`Stm32UartDriver`/`Stm32I2cDriver`/`Stm32SpiDriver`/`STM32Timer` instance to
the topology is fully supported: every driver in this library resolves its
HAL pointer(s) from HAL-free per-instance state on every call rather than
caching them in file-static state shared across instances, so two
simultaneously-open instances of the same driver type (different bus,
different chip-select, different physical peripheral) never interfere with
each other — this is what lets a CubeSat team add, say, a second UART for
an Iridium modem without touching the existing ground-link UART, or a
second I2C bus for a second temperature sensor.

### Adding a new chip family

`Os/Stm32H7` and the `Drv/STM32*` drivers only reach HAL functionality
through CubeMX's own per-peripheral headers (`main.h`, `gpio.h`, `usart.h`,
`i2c.h`, `tim.h`, `dma.h`) -- CubeMX generates these under the *same* names
for every STM32 family, unlike the family-named umbrella headers
(`stm32h7xx_hal.h`, `stm32f4xx_hal.h`, ...). Never include a family-named HAL
header directly from library code; include the matching CubeMX per-peripheral
header instead (it chains to the right family headers with the right
pre-defines already set up). This is what keeps `Drv/STM32*` family-portable
without any per-family duplication.

`Os/Stm32H7` itself *is* family-specific (its name says so) because OSAL
backends for different families are mutually exclusive per build. To add a
new family: create `Os/Stm32<Family>/` alongside it with the same four
`register_os_implementation` calls (`SUFFIX` = the new family name), and add
an `elseif (FPRIME_PLATFORM STREQUAL "stm32<family>")` branch in
`Os/CMakeLists.txt`. The `Drv/STM32*/CMakeLists.txt` real/stub selection
already matches any `stm32*` platform, so no change is needed there.

#### Checklist: adding a new family (e.g. STM32F4)

STM32F4 is the next planned family. It's a useful worst-case example because
it differs from STM32H7 in exactly the place this library's DMA support
leans on hardest: **Cortex-M4 has no data cache at all** (not just "cache
disabled" -- `SCB_CleanDCache_by_Addr`/`SCB_InvalidateDCache_by_Addr` don't
exist in the M4 CMSIS core header, so calling them is a compile error, not a
logical no-op), and F4's memory map has no `DTCM`/`AXI_SRAM` split -- just
plain SRAM uniformly reachable by DMA. Every item below either doesn't apply
to F4 for that reason, or exists specifically to stay correct across a family
that has no cache and no DMA-excluded RAM region.

**1. Toolchain & CMake platform**
- [ ] Add `cmake/toolchain/stm32f4.cmake` with the family's own compiler flags
      (`-mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard`, vs. H7's
      `-mcpu=cortex-m7 -mfpu=fpv5-d16`) and chip define (e.g.
      `-DSTM32F407xx` in place of `-DSTM32H753xx`).
- [ ] Add `cmake/platform/stm32f4.cmake` mirroring `stm32h7.cmake`'s module
      wiring.
- [ ] Add an `elseif (FPRIME_PLATFORM STREQUAL "stm32f4")` branch in
      `Os/CMakeLists.txt` (see above) -- the `Drv/STM32*/CMakeLists.txt`
      `FPRIME_PLATFORM MATCHES "^stm32"` conditionals already match
      `stm32f4` with no changes needed.

**2. Vendor HAL / CMSIS / linker** (project-owned, like today's
`Hardware/stm32h753_hal/`, but required for the library to compile against)
- [ ] Vendor `STM32F4xx_HAL_Driver` + CMSIS `Device/ST/STM32F4xx`, generated
      from a CubeMX `.ioc` for the chosen F4 part, under the consuming
      project's `Hardware/` tree.
- [ ] Verify the vendored HAL version actually exposes
      `HAL_UARTEx_ReceiveToIdle_DMA`/`HAL_UARTEx_RxEventCallback` if
      DMA-mode UART is wanted on F4 -- this API landed in the HAL at a
      specific version per family; don't assume parity with the H7 package.
- [ ] Write a new linker script for the F4 part's actual flash/RAM layout.
      There is no `AXI_SRAM`/`DTCM_RAM` split on F4 (just `SRAM1`/`SRAM2`/...),
      so this README's "Memory and placement" table needs an F4-specific
      variant, not a reused one.
- [ ] Give `PlatformMemory.hpp`'s `ATTR_DTCM_BSS` macro an F4 definition that
      compiles to nothing (plain `.bss` placement) -- F4 has no DTCM bus for
      it to point at.

**3. Core-level abstraction (cache + DMA-safety) -- the part this family
actually changes**
- [ ] Create `Core/CortexM4/CacheMaintenance.hpp` alongside
      `Core/CortexM7/CacheMaintenance.hpp`, exporting the *same* three
      function names (`CleanDCacheForDma`, `InvalidateDCacheForDma`,
      `AssertDmaSafe`) in the same `Stm32` namespace, so no driver `.cpp`
      ever needs an `#ifdef` -- only the CMake include path changes:
  - `CleanDCacheForDma`/`InvalidateDCacheForDma` become empty inline
    no-ops. Do not `#include` or otherwise reuse the M7 version -- the
    `SCB_*DCache_by_Addr` calls it wraps don't exist on M4.
  - `AssertDmaSafe` becomes a no-op that always passes -- there's no
    DTCM-vs-SRAM split on F4, so there's no "unsafe for DMA" address range
    to assert against.
- [ ] Add a sibling `FprimeStm32CoreCortexM4` interface CMake target
      (mirroring `FprimeStm32CoreCortexM7`'s single
      `target_include_directories` line) pointing at `Core/CortexM4`.
- [ ] In every `Drv/STM32*Driver/CMakeLists.txt`'s real-target `DEPENDS`,
      select the core target *by family*, not by the generic `^stm32` match
      used for everything else (e.g. `stm32h7` depends on
      `FprimeStm32CoreCortexM7`, `stm32f4` on `FprimeStm32CoreCortexM4`).

**4. OS abstraction layer**
- [ ] Create `Os/Stm32F4/` per the four-call pattern above.
- [ ] Check whether `Os::RawTime`/`Os::Mutex`'s actual implementations are
      Cortex-M-generic (DWT cycle counter, `PRIMASK`) rather than
      H7-specific before copying them -- if they're already core-generic,
      factor the shared logic into an `Os/CortexM/` implementation both
      `Stm32H7` and `Stm32F4` depend on instead of duplicating it.

**5. Driver instance wiring** (`toHalHandle()`/`callInstanceInit()`/`hw*()`
in each driver's real `.cpp`)
- [ ] Confirm the HAL struct/function names (`SPI_HandleTypeDef`,
      `HAL_SPI_TransmitReceive`, `HAL_SPI_TransmitReceive_DMA`, ...) are
      identical between the F4 and H7 HAL packages -- they are, by ST's
      design, so these methods need **no changes** once the matching
      CubeMX-generated `spi.c`/`i2c.c`/`usart.c` externs exist for the F4
      project.
- [ ] Check each driver's instance-enable macros in `Stm32Config.hpp`
      against the chosen F4 part's actual peripheral count (fewer
      SPI/I2C/USART instances than H753 is common) -- leave the ones that
      don't exist `false` forever; the enum itself doesn't need to shrink.
- [ ] Verify the F4 HAL's `__HAL_LINKDMA()` convention populates
      `hdmatx`/`hdmarx` the same way H7's does -- if so, the
      `hdmatx != nullptr`/`hdmarx != nullptr` safety assert in
      `Stm32SpiDriver`/`Stm32I2cDriver::open()` works unmodified.

**6. Documentation**
- [ ] Revisit each driver's `docs/sdd.md` for claims that were implicitly
      H7-specific (e.g. "D-cache maintenance," AXI SRAM wording) now that a
      second family's DMA path exists.
- [ ] Add an F4 entry to this README's "Supported hardware" section once a
      concrete evaluation board is targeted.

**7. Verification**
- [ ] `fprime-util build stm32f4` from a project with the F4 `Hardware/`
      tree wired -- confirms the cross-compile, the new `Core/CortexM4`
      headers, and the new OS backend all link.
- [ ] Host-native `fprime-util check` needs **no changes** for a new
      family: `<Driver>Stub.cpp` has zero HAL/CMSIS dependency already, so
      the UT suite is family-agnostic by construction.
- [ ] Smoke-test at least one DMA-mode and one POLLED-mode instance on real
      F4 hardware before trusting the no-op cache functions silently -- on a
      cache-less core, a successful build already proves more than it would
      on H7 (there's no `SCB_*DCache_by_Addr` call left to have gotten
      wrong), but the DMA data path itself still needs a real hardware pass.

## Integration and build

The parent F´ project selects this platform through its `stm32h7` CMake
configuration. From the parent project:

```shell
source fprime-venv/bin/activate
fprime-util generate -f
fprime-util build -j"$(nproc)"
```

To build the reference deployment directly after generation:

```shell
ninja -C build-fprime-stm32h7 ReferenceDeployment
```

The deployment must call the hardware initialization routines in this order:

1. `HAL_Init()`
2. `FprimeStm32_ClockInit()`
3. `SCB_EnableICache()` and `SCB_EnableDCache()`
4. `Stm32_Tim2ClockInit()`
5. topology setup and the cyclic executive

The clock initialization is required before configuring TIM2. The cache
initialization is required before using the DMA cache-maintenance helpers;
Cortex-M7 cache tags and data are undefined at reset.

## USART1 DMA ground link

`Drv::Stm32UartDriver` provides a non-blocking USART1 DMA transport over PB14/PB15
through the ST-LINK-V3E VCP. It uses fixed-size TX and RX rings, aligned DMA
staging buffers, idle-line detection for RX, and a polled DMA state machine so
the cyclic executive is never blocked waiting for serial I/O.

The driver was validated on the physical STM32H753XI-EVAL2 with:

- 115200 baud, 8N1 operation
- DMA on both transmit and receive
- 183 KiB transmitted in 180 seconds with zero TX/RX error counts
- 6,144 bytes captured from the VCP in 6.0 seconds, matching the driver's
  internal byte counter
- 96 injected uplink bytes received and drained without buffer leaks
- CCSDS Space Packet framing through the `ComCcsds` subtopology
- A continuous 21+ minute `fprime-gds` session with command uplink, telemetry
  downlink, and event downlink

The GDS validation decoded more than 12,000 telemetry samples at approximately
10 samples per second. The command round trip included `CMD_NO_OP`, string
commands, and an oversized string that correctly returned `FORMAT_ERROR`.

The ground-link tests above were performed before the September 3 clock-tree
fix, while the board was still running from the approximately 64 MHz HSI. The
USART baud rate self-adjusted from the live peripheral clock query, so the link
remained valid, but an extended ground-link soak at the corrected PLL clock is
still required.

## I2C bus (`Stm32I2cDriver`)

`Stm32::Stm32I2cDriver` implements the framework's `Drv.I2c` interface
(guarded, synchronous `write`/`read`/`writeRead` ports, each returning
`Drv::I2cStatus` directly) against I2C1 on PB6/PB7 (SCL/SDA). Each instance
is opened in one `Stm32::TransferMode`, chosen per-instance at `open()`:

- **`POLLED`** (the default): every `HAL_I2C_Master_Transmit`/`_Receive` call
  is a single blocking HAL call, bounded by a fixed 10 ms watchdog passed as
  the HAL's own `Timeout` parameter. This is the right choice for a small,
  low-rate sensor like the MPU-6050 — a real I2C transaction at 400 kHz is
  sub-millisecond, and the cyclic executive has no thread to free up by not
  blocking.
- **`DMA`**: `write`/`read`/`writeRead` are not served; the instance instead
  serves `writeAsync`/`readAsync`/`writeReadAsync` — a request starts the
  matching `HAL_I2C_Master_*_DMA()` call and returns immediately, and `poll()`
  (called every cyclic-executive pass in `Main.cpp`) drains the ISR-latched
  completion and invokes the matching `*AsyncComplete` port. This is the
  right choice for anything that must not block the executive while a
  transaction is in flight.

`writeRead`/`writeReadAsync` share one real limitation: each is two
back-to-back DMA/blocking calls (STOP then START), not a single
electrically-held repeated START. This is safe on this single-master bus
(nothing else can interrupt the sequence) but will not work for a sensor that
strictly requires the bus held across the register-address write (loses its
internal pointer on STOP) — that would need the sequential IT/DMA API with
a held repeated START, not yet implemented here.

`open(instance, busSpeed, mode)` selects the peripheral (`I2cInstance::I2c1`
today; `I2c2`/`I2c3`/`I2c4` are declared for other boards but not yet
CubeMX-configured), a bus speed preset (`I2cBusSpeed::Standard`/`Fast`/
`FastPlus` — CubeMX-computed `Timing` register values for this project's
actual D2PCLK1 clock, since the H7 I2C peripheral has no runtime baud-rate
formula the way UART does), and the transfer mode (default `POLLED`). It
follows the same `Common`/`Real`/`Stub` HAL boundary convention described
below, with one variant: `open()` itself is implemented directly in
`Stm32I2cDriver.cpp`/`Stm32I2cDriverStub.cpp` rather than delegating to a
private `hwOpen()`. See [`Stm32I2cDriver`'s own
`docs/sdd.md`](Drv/STM32I2cDriver/docs/sdd.md) for the full async/`poll()`
design and its known DMA limitation (I2C1's current CubeMX config has no DMA
request attached, so `DMA` mode asserts rather than running until that's
regenerated).

Validated live against a real MPU-6050 IMU in `POLLED` mode (wake-up register
write, then repeated 14-byte accel/gyro/temp reads) — see "Adding a sensor"
below for how that's wired without any application code touching the I2C bus
directly.

### Adding a sensor

Connect a sensor's ports directly to `Stm32I2cDriver`'s `write`/`read`/
`writeRead` — if the sensor component already imports the framework's
`Drv.I2c`/`Drv.I2cWriteRead` port types (check its `.fpp`), no adapter
component is needed. This is exactly how the MPU-6050 was wired:
`fprime-sensors`' `MpuImu.ImuManager` (a `queued` component with its own
internal reset/enable/configure/read state machine) declares `busWrite:
Drv.I2c` / `busWriteRead: Drv.I2cWriteRead` output ports, connected straight
to `i2cDriver.write`/`i2cDriver.writeRead` in `Top/topology.fpp`. Its standard
command/event/telemetry/param/time ports wire themselves via the topology's
existing pattern-graph specifiers (`command connections instance
CdhCore.cmdDisp`, etc.) — the only manual connections needed were the two I2C
ports and a rate-group tick into its `run` port.

**Don't use the sensor library's own bundled example `Subtopology`
(`MpuImuSubtopologyConfig.fpp` etc.), and don't add the whole library via
`settings.ini`'s `library_locations`.** Those bundled Subtopologies hardcode a
`Drv.Linux*Driver` instance type (e.g. `Drv.LinuxI2cDriver` for `MpuImu`,
`Drv.LinuxSpiDriver` for `Bmp280`). Since Linux-only driver modules are
skipped outright on `stm32h7` — not just their C++ target, the type doesn't
exist in the fpp model at all — `fpp-to-cpp` fails with `"symbol Drv is not
defined"` the instant any bundled Subtopology config gets registered, whether
or not the deployment references it. Instead, register only the specific
modules actually needed (for `MpuImu`: `Helpers`, `MpuImu/Types`,
`MpuImu/Ports`, `MpuImu/Components`) directly via `add_fprime_subdirectory` in
the project's top-level `CMakeLists.txt`, skipping each family's own
top-level `CMakeLists.txt` (that's what pulls in `Subtopology`). Because the
library's own sources `#include "fprime-sensors/..."` (paths relative to the
library root), also add `lib/fprime-sensors` as a plain
`include_directories()` root — both the source tree and
`${CMAKE_CURRENT_BINARY_DIR}/lib/fprime-sensors`, where fpp generates the
matching `*Ac.hpp` headers when the library isn't registered through
`library_locations`.

## SPI bus (`Stm32SpiDriver`)

`Stm32::Stm32SpiDriver` implements the framework's `Drv.Spi` interface
(guarded `SpiWriteRead`, returning `Drv::SpiStatus`, plus the deprecated
void-returning `SpiReadWrite`) against SPI5 on PF6/PF7/PF8/PF9
(NSS/SCK/MISO/MOSI). Each instance is opened in one `Stm32::TransferMode`,
chosen per-instance at `open()`, mirroring `Stm32I2cDriver`:

- **`POLLED`** (the default): `SpiWriteRead`/`SpiReadWrite` perform a single
  blocking `HAL_SPI_TransmitReceive()` call. Sensor payloads on this bus (6-8
  bytes for the BMP280) are small enough that this is simpler and cheaper
  than setting up DMA-safe AXI SRAM buffers and Cortex-M7 D-cache maintenance
  for a transfer that's over before either would matter.
- **`DMA`**: `SpiWriteRead`/`SpiReadWrite` are not served; the instance
  instead serves `spiWriteReadAsync` — a request starts
  `HAL_SPI_TransmitReceive_DMA()` and returns immediately, and `poll()`
  (called every cyclic-executive pass in `Main.cpp`) drains the ISR-latched
  completion and invokes `spiWriteReadAsyncComplete`. This is the right
  choice for anything that must not block the executive while a transfer is
  in flight (e.g. bulk flash access on a different SPI instance, or a radio).

SPI5 is configured `NSS_SOFT` in this project's CubeMX setup — the HAL never
drives a chip-select pin on its own — so this driver owns the CS GPIO
directly rather than treating it as the peripheral's problem: `open(instance,
csPort, csPin, timeoutMs)` configures the pin as a push-pull output (idle
high) alongside `MX_SPIn_Init()`, and every `SpiWriteRead`/`SpiReadWrite` call
drives it low for the exact duration of the `HAL_SPI_TransmitReceive()` call.

`Stm32SpiDriver` stores only HAL-free state on each component instance (the
`SpiInstance` enum, the chip-select `Stm32::GpioPort`+pin, the timeout) and
re-resolves the real HAL pointers from that state on every call, matching
the same per-instance approach every STM32 driver in this library now uses
(see "Enabling and selecting peripheral instances" above). This is
deliberate, not incidental: a BMP280 and any future SPI sensor on a
different bus or chip-select need two simultaneously-open `Stm32SpiDriver`
instances that never share mutable state.

`open(instance, csPort, csPin, timeoutMs, mode)` selects the peripheral
(`SpiInstance::Spi5` today; `Spi1`-`Spi4`/`Spi6` are declared for other
boards but not yet CubeMX-configured), the chip-select pin using the same
`Stm32::GpioPort` vocabulary `Stm32GpioDriver::open()` uses, a
per-transaction watchdog (default 10 ms, matching `Stm32I2cDriver`'s
convention), and the transfer mode (default `POLLED`). It follows the same
`Common`/`Real`/`Stub` HAL boundary convention as every other driver here —
see [`Stm32SpiDriver`'s own `docs/sdd.md`](Drv/STM32SpiDriver/docs/sdd.md)
for the full design, including why `spiWriteReadAsync`'s port type and
`Drv.AsyncSpiCfg` config are hand-declared locally (this project's pinned
`fprime` version doesn't have them) and its known DMA limitation (SPI5's
current CubeMX config has no DMA request attached, so `DMA` mode asserts
rather than running until that's regenerated).

Validated live against a real BMP280 pressure/temperature sensor in
`POLLED` mode — wired via the exact "Adding a sensor" pattern above
(`BmpManager` declares an `output port spiWriteRead: Drv.SpiWriteRead`,
matching this driver's port type directly with no adapter needed).

## Hardware validation

The complete reference topology has been run on the physical board with the
non-blocking cyclic executive and cooperative dispatch enabled. Validation
included:

- Topology setup and all active-component queues created successfully
- No hits on assertion, abort, exit, HardFault, BusFault, or fatal-handler
  breakpoints during the recorded endurance runs
- PF10 LED activity, driven exclusively through `Drv::Stm32GpioDriver` (wired
  into `instances.fpp`/`topology.fpp` and opened from `configureTopology()`),
  confirmed via a GDS-based integration test (`led_integration_tests.py`)
  rather than manual `GPIOF_ODR` register polling
- TIM2 measured at approximately 997.9 kHz over an undisturbed 30-second
  interval after PLL clock initialization
- The 100 Hz timer tick and rate-group tick counters remained synchronized
- USART1 DMA continued transmitting correctly after the corrected clock was
  enabled

The clock-tree bug was fixed by calling `FprimeStm32_ClockInit()` after
`HAL_Init()`. Before that change, TIM2 advanced at approximately 269.5 kHz,
which matched the unconfigured 64 MHz HSI divided by the intended TIM2
prescaler. The corrected implementation selects PLL1 and restores the intended
480 MHz system clock.

## Memory and placement

The STM32 linker configuration defines these regions:

| Region | Capacity |
| --- | ---: |
| `FLASH` | 2 MiB |
| `AXI_SRAM` | 512 KiB |
| `DTCM_RAM` | 128 KiB |

CPU-only framework state can be placed in DTCM, while DMA-visible buffers and
the bootstrap allocation pool remain in AXI SRAM. The latest recorded
`baremetal-size stm32h7` result for the complete reference deployment was:

| Region | Used | Remaining |
| --- | ---: | ---: |
| Flash (`.text`+`.data`) | 645,708 bytes | 69.2% |
| AXI SRAM `.bss` | 261,308 bytes | 50.1% |
| DTCM `.dtcm_bss` | 8,584 bytes | 93.5% |
| Bootstrap pool | 112,656 of 131,072 bytes | 14.1% |

Flash and AXI SRAM `.bss` grew modestly from the Week 8 `led`/`gpioDriver`
topology wiring and the Week 9 `Common`/`Real`/`Stub` driver split (a few new
members per driver); DTCM `.dtcm_bss` is unchanged byte-for-byte. The
bootstrap-pool row is a runtime allocation count rather than a static ELF
section, so it's carried over from the last hardware run and still needs
live re-verification.

The reference deployment locks the bootstrap allocator after topology setup and
wraps the C heap symbols so post-initialization allocations assert instead of
silently using an unbounded heap. New components should be evaluated against
both the AXI SRAM margin and the remaining bootstrap-pool capacity.

## Host unit testing (`fprime-util check`)

Every driver under `Drv/` splits into three files sharing one HAL-free
header, so `fprime-util check` can compile and run its GTest unit test on
the host (x86_64 Linux) without any ARM/CMSIS toolchain:

- `<Driver>Common.cpp` — hardware-independent logic (validation, ring
  buffers, state machines, event/telemetry emission). Always built, on
  every platform. This is where unit tests get real coverage.
- `<Driver>.cpp` — the real implementation, built only for the `stm32h7`
  target. Every HAL/CMSIS touch (register access, `HAL_*` calls, ISR
  callbacks) lives here behind a small set of private boundary methods
  (named `hw*`) declared in the header. This is the only file allowed to
  `#include` a vendor CMSIS/HAL header.
- `<Driver>Stub.cpp` — built only for host unit tests. Implements the same
  `hw*` boundary methods with fixed, no-HAL-dependency behavior (e.g.
  "always succeeds," a settable fake counter). Never included in a
  flight build.

Each driver's `CMakeLists.txt` always registers the production module
(`register_fprime_module`/`register_fprime_library`) — only the choice of
`<Driver>.cpp` vs `<Driver>Stub.cpp` (and the matching `DEPENDS`) is
platform-conditional — and always registers `register_fprime_ut` (never
gated by `restrict_platforms`, which would make the UT target itself
unreachable and `fprime-util check` fail with `NoTargetFoundException`).
`FprimeStm32` (the real vendor HAL static library) and its `Os/`
subdirectory are gated to the `stm32h7` target in this directory's own
`CMakeLists.txt`.

**Adding a new driver:** don't add `#ifdef BUILD_UT`/`#ifndef` to
production code. If the driver only needs HAL calls that map cleanly onto
a boundary method, follow the `Common`/`Real`/`Stub` split above (copy an
existing driver's `CMakeLists.txt`), storing only a HAL-free instance enum
as a component member and re-resolving the real HAL pointer via
`toHalHandle()` on every call — never cache it in file-static state,
which would make two simultaneously-open instances of the driver clobber
each other (see "Enabling and selecting peripheral instances" above). If a
routine is genuinely hard to fake (e.g. an ISR callback with no
user-context pointer, like `HAL_UART_TxCpltCallback`), do what
`Stm32UartDriver`/`STM32Timer` do: route it through a public
`signalX()`/`hwArmY()` method on the component so a unit test can call it
directly to simulate the hardware event, and — since the callback receives
only a raw HAL handle, not an instance — keep a small fixed-size registry
table (`{handle, component}` pairs, sized to the instance enum's own
cardinality, indexed by instance) in the real `.cpp` only, with the
callback doing a short linear scan to resolve which live component
instance owns the handle it was given.

Verification commands:

```sh
fprime-util generate --ut -f   # regenerate the host/native UT build cache
fprime-util check              # from a driver's directory: build + run its UT
fprime-util check --coverage   # same, plus a line/function/branch coverage report
```

## Known follow-up work

- Repeat the extended USART1 DMA and GDS soak at the corrected 480 MHz clock.
- Validate TIM2 rollover, interrupt masking, and long-duration stability.
- Continue hardware-in-the-loop automation for the STM32 target.
- Add real persistent file support for the MicroFs-backed services; the current
  conservative configuration recognizes only `/bin<N>/file<M>` paths.
- Re-verify the bootstrap-pool usage figure in "Memory and placement" live on
  hardware; it's a runtime allocation count, not a static ELF section, so it
  couldn't be refreshed by the host-only `baremetal-size` re-measurement.
- `Stm32I2cDriver` has no `test/ut/` files yet, despite already following the
  `Common`/`Real`/`Stub` split (`register_fprime_ut` is scaffolded but
  commented out in its `CMakeLists.txt`) — write them.
- Run the full I2C exit-criterion soak on real hardware: 1,000 iterations at
  400 kHz with explicit event evidence for every injected error path (NACK,
  timeout, bus error), not just confirmed-working normal operation.
- If a sensor needs a true repeated START (loses its register pointer across
  a STOP), upgrade `writeRead` to the sequential IT/DMA API with I2C1's NVIC
  interrupt enabled — the current STOP-then-START is safe on this
  single-master bus but isn't electrically a repeated START.
