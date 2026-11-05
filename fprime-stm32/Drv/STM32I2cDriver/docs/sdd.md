# Stm32::Stm32I2cDriver

## 1. Introduction

`Stm32I2cDriver` is a bare-metal implementation of the [`Drv.I2c`](../../../fprime/Drv/Interfaces/I2c.fpp) interface for an STM32H7 I2C peripheral, selected per instance at `open()` time. It is a raw bus-master driver only: there is no sensor-specific register model layered on top -- a sensor component (or a topology-level test command) is expected to drive this component's ports directly with the sensor's own register addressing convention.

Each instance is opened in exactly one transfer mode, chosen by the caller at `open()` time:

- **`Stm32::TransferMode::POLLED`** (the default): `write`/`read`/`writeRead` perform a single blocking `HAL_I2C_Master_Transmit()`/`HAL_I2C_Master_Receive()` call and return once the transaction is physically complete. The right choice for small, low-rate sensors (e.g. the MPU-6050 IMU).
- **`Stm32::TransferMode::DMA`**: `write`/`read`/`writeRead` are **not** served in this mode (see §3.4) -- the instance instead serves `writeAsync`/`readAsync`/`writeReadAsync`, three request/completion port pairs. A request starts the matching `HAL_I2C_Master_*_DMA()` call and returns immediately; the transaction completes asynchronously, and `poll()` -- called every cyclic-executive pass, not from a rate group -- drains the ISR-latched outcome and invokes the matching completion port. This is the appropriate mode for anything that must not block the cyclic executive while a transaction is in flight.

A single component instance is one or the other, fixed for its lifetime after `open()` -- there is no port that works in both modes.

## 2. Requirements

| Name | Description | Validation |
|---|---|---|
| STM32-I2C-COMP-001 | Shall implement the `Drv.I2c` interface | inspection |
| STM32-I2C-COMP-002 | Shall configure a caller-selected I2C instance (via `MX_I2Cn_Init()`) at a caller-selected bus speed preset, and reject transactions issued before `open()` | inspection |
| STM32-I2C-COMP-003 | Shall perform a blocking write to a 7-bit slave address via `write`, bounded by a 10 ms transaction watchdog, when opened `POLLED` | inspection, test |
| STM32-I2C-COMP-004 | Shall perform a blocking read from a 7-bit slave address via `read`, bounded by a 10 ms transaction watchdog, when opened `POLLED` | inspection, test |
| STM32-I2C-COMP-005 | Shall perform a blocking write followed by a read for the same address via `writeRead` without requiring a second port invocation, when opened `POLLED` | inspection |
| STM32-I2C-COMP-006 | Shall distinguish an address-phase NACK (`I2C_ADDRESS_ERR`) from a data-phase/bus failure (`I2C_WRITE_ERR`/`I2C_READ_ERR`) | inspection |
| STM32-I2C-COMP-007 | When opened DMA, shall serve `writeAsync`/`readAsync`/`writeReadAsync` requests by starting the matching `HAL_I2C_Master_*_DMA()` call(s) and returning immediately, delivering exactly one completion via the matching `*AsyncComplete` port per request, from `poll()`, never from an ISR and never from inside the request call | inspection, test |
| STM32-I2C-COMP-008 | `writeReadAsync` shall run its write phase and read phase as two DMA transactions across separate `poll()` passes, delivering exactly one combined completion for the whole operation | inspection, test |
| STM32-I2C-COMP-009 | A DMA-mode transaction shall clean the D-cache over a write buffer before, and invalidate it over a read buffer after, the transfer, and shall assert the buffer is reachable by the DMA controller (not DTCM) | inspection |
| STM32-I2C-COMP-010 | `poll()` shall recover a DMA-mode transaction that exceeds its watchdog by aborting it and delivering an error completion, and shall be a harmless no-op when no transaction is in flight (including on a POLLED-mode instance, or a request rejected for not being DMA-mode) | inspection, test |

## 3. Design

### 3.1 Port model

`import Drv.I2c` provides `write`/`read`/`writeRead` as guarded, synchronous input ports; each returns a `Drv::I2cStatus` to the caller directly, so there is no completion port and no telemetry for these three.

Alongside those, `Stm32I2cDriver.fpp` hand-declares three asynchronous request/completion port pairs:

- `writeAsync` / `writeAsyncComplete` (`Drv.I2cRequest` / `Drv.I2cCallback`)
- `readAsync` / `readAsyncComplete` (`Drv.I2cRequest` / `Drv.I2cCallback`)
- `writeReadAsync` / `writeReadAsyncComplete` (`Drv.I2cWriteReadRequest` / `Drv.I2cWriteReadCallback`)

These are hand-declared (array-sized by `Drv.AsyncI2cCfg.I2cDriverPorts`, which **is** present in this project's pinned `fprime` version -- unlike SPI's equivalent constant, see `Stm32SpiDriver`'s own `docs/sdd.md` §3.1) rather than pulled in via `import Drv.AsyncGuardedI2c`, for the same reason as `Stm32SpiDriver`'s `spiWriteReadAsync`: `Drv.I2c` and `Drv.AsyncGuardedI2c` both declare guarded ports literally named `write`, `read`, and `writeRead`; FPP rejects importing both into one component as duplicate port names.

All six ports (three synchronous, three asynchronous) are `guarded`, not just `sync`: F´'s generated `*_handlerBase()` wrapper calls `this->lock()` before invoking the handler and `this->unLock()` after, serializing concurrent callers into this component's internal state. For the synchronous ports this lock is held for the entire blocking POLLED transaction -- harmless, since nothing else needs to run while it blocks. For the async ports the lock is held only long enough to start the DMA transaction and return; the wait for completion happens later, in `poll()`, which is **not** called from inside any guarded port and therefore never holds this lock across the DMA wait (see §3.6 for why an earlier design -- blocking inside the guarded call -- was fatal).

The only event is `HalError`, emitted from the HAL boundary whenever a blocking or DMA-start `HAL_I2C_*` call does not return `HAL_OK`.

### 3.2 Common/Real/Stub split

Following the convention in `lib/fprime-stm32/README.md`, this driver splits into three files sharing one HAL-free header (`Stm32I2cDriver.hpp`):

- `Stm32I2cDriverCommon.cpp` -- port handler logic, `open()`'s HAL-free bookkeeping, and the `poll()` state machine, all hardware-independent. Always built.
- `Stm32I2cDriver.cpp` -- the real HAL boundary (`open`/`hwMasterTransmit`/`hwMasterReceive`/`hwStartAsyncTransmit`/`hwStartAsyncReceive`/`hwAbortAsyncTransaction`/`hwFinishAsyncReceive`), built only for the `stm32h7` target. The only file allowed to include `i2c.h`/call `HAL_I2C_*`.
- `Stm32I2cDriverStub.cpp` -- the same boundary methods against injectable/observable stub state, built for host UT and any non-`stm32h7` build. Never included in a flight build.

### 3.3 `open(instance, busSpeed, mode)`

`open()` takes an `I2cInstance` (`I2c1`/`I2c2`/`I2c3`/`I2c4`, mirroring `Stm32SpiDriver`'s `SpiInstance` switch), an `I2cBusSpeed` preset (`Standard`/`Fast`/`FastPlus`, defaulting to `Fast` to match this project's current configuration), and a `Stm32::TransferMode` (default `POLLED`, trailing both other params so every existing call site keeps compiling and keeps its current, polled behavior unchanged). It calls the CubeMX-generated `MX_I2Cn_Init()` for the selected instance (I2C1/GPIO/clock configuration in `Hardware/stm32h753_hal/Core/Src/i2c.c`), then applies the requested speed by overwriting `Init.Timing` and re-running `HAL_I2C_Init()` if it differs from what `MX_I2Cn_Init()` just applied.

When `mode` is `DMA`, `open()` additionally calls `MX_DMA_Init()`, asserts that CubeMX has actually linked a DMA stream to this instance (`halHandle->hdmatx`/`hdmarx` non-null -- see the warning in §3.5), and registers `{halHandle, this}` into a small fixed-size table (`s_i2cRegistry`, sized to `I2cInstance`'s cardinality) so `HAL_I2C_MasterTxCpltCallback()`/`HAL_I2C_MasterRxCpltCallback()`/`HAL_I2C_ErrorCallback()` -- plain HAL callbacks with no instance/context argument -- can route back to the correct component instance, the same registry pattern `Stm32UartDriver` uses for its own ISR callbacks (see its `docs/sdd.md` §3.7). A POLLED instance is never registered -- this is why `writeAsync_handler()`/`readAsync_handler()`/`writeReadAsync_handler()` reject every request on a POLLED instance (see §3.5): without a registry entry, the completion ISR would have nothing to call back into, and the request would silently run to its own watchdog timeout on every attempt instead of failing fast.

Only `I2c1` has a CubeMX-generated handle/init function in this project today -- selecting `I2c2`/`I2c3`/`I2c4` asserts in the real HAL boundary (`Stm32I2cDriver.cpp`'s `toHalHandle()`/`callInstanceInit()`). Adding a second instance for a different board only requires generating that peripheral's CubeMX config and one switch case in each of those two functions; the header, `Common.cpp`, and the `Stub` are already instance-agnostic.

Unlike UART's `BaudRate` (computed at runtime by `HAL_UART_Init()` from the peripheral clock), the STM32H7 I2C `Timing` register has no closed-form runtime formula in the HAL. `I2cBusSpeed`'s presets map to CubeMX-computed constants for this project's actual D2PCLK1 clock (`0x00B03FDB` for Fast, `0x307075B1` for Standard, `0x0050174F` for FastPlus). A new preset can only be added once CubeMX has actually generated its Timing value for this clock tree -- do not hand-derive one.

`MX_I2Cn_Init()` traps in `Error_Handler()` on failure rather than returning a status, matching every other `MX_*_Init()` in this project, so `open()` cannot observe a failure there -- only the speed-override `HAL_I2C_Init()` call (when the requested preset differs from CubeMX's default) has a real failure path.

**Multi-instance safety**: `open()` stores only the HAL-free `I2cInstance` enum and the requested `TransferMode` as component members; every `hw*` boundary method re-resolves `toHalHandle(this->m_instance)` fresh on every call, rather than caching the resolved `I2C_HandleTypeDef*` in file-static state. This is what lets two `Stm32I2cDriver` instances -- e.g. one for an IMU on I2C1 and a second for a temperature sensor on I2C2 -- be open at the same time, independently POLLED or DMA, with no shared mutable state between them.

### 3.4 `write`/`read`/`writeRead` (POLLED mode)

Each handler asserts the caller's `Fw::Buffer`(s) are non-null and fit in a `U16` (the HAL's `Size`/`DevAddress` parameter width), then calls the corresponding `hwMasterTransmit()`/`hwMasterReceive()` boundary method -- each a single blocking `HAL_I2C_Master_Transmit()`/`HAL_I2C_Master_Receive()` call bounded by `TRANSACTION_TIMEOUT_MS`. On failure, `HAL_I2C_GetError() & HAL_I2C_ERROR_AF` distinguishes "no device answered" (`I2C_ADDRESS_ERR`) from a data-phase/bus failure (`I2C_WRITE_ERR`/`I2C_READ_ERR`).

`writeRead` issues a transmit immediately followed by a receive to the same address -- a STOP and a fresh START, not a single electrically-held repeated START. Because this bus has exactly one master, a STOP-then-START is safe for the common case (nothing else can interrupt the sequence); a sensor that strictly requires the bus held across the register-address write (loses its internal pointer on STOP) will not work correctly until this is upgraded to a sequential-transfer HAL call. `writeReadAsync`'s own two-phase design (§3.5) has the identical limitation for the same reason.

### 3.5 `writeAsync`/`readAsync`/`writeReadAsync`/`poll()` (DMA mode)

Each `*Async_handler()` (`Stm32I2cDriverCommon.cpp`):

1. Returns `I2C_OPEN_ERR` immediately (via its matching `*Complete_out()`) if the instance hasn't been `open()`ed.
2. Returns `I2C_OTHER_ERR` immediately if the instance was not opened with `TransferMode::DMA` (see §3.3), or if a transaction is already in flight on this instance.
3. Otherwise starts the transaction via `hwStartAsyncTransmit()`/`hwStartAsyncReceive()` (`Stm32::AssertDmaSafe()` on the buffer, `Stm32::CleanDCacheForDma()` on a write, `HAL_I2C_Master_Transmit_DMA()`/`HAL_I2C_Master_Receive_DMA()`) and returns. A HAL failure to even *start* is reported immediately; a successful start leaves the transaction in flight for `poll()` to observe later -- **never** from inside the handler call.

`writeReadAsync` is the one with extra state: it starts the write phase immediately (`AsyncOp::WRITE_READ_WRITE_PHASE`), and `poll()` -- on seeing that phase complete successfully -- starts the read phase (`AsyncOp::WRITE_READ_READ_PHASE`) using the same device address, rather than delivering a completion yet. Only the read phase's outcome (or an early write-phase failure) produces the single `writeReadAsyncComplete_out()` call the caller sees.

`poll()` is called every cyclic-executive pass (see `Main.cpp`'s `while(true)` loop, not from a rate group) and is a no-op whenever no async request is outstanding -- including, trivially, on a POLLED-mode instance, which never sets its in-flight flag. When a transaction is outstanding:

- If the ISR (`HAL_I2C_MasterTxCpltCallback()`/`HAL_I2C_MasterRxCpltCallback()`/`HAL_I2C_ErrorCallback()`) has already latched an outcome, `poll()` picks it up directly.
- Otherwise it checks the per-transaction watchdog (`TRANSACTION_TIMEOUT_MS`, `Os::RawTime`-timed); if still within budget it returns and tries again next pass; if exceeded, it calls `hwAbortAsyncTransaction()` (`HAL_I2C_Master_Abort_IT()` -- completes via the same ISR) and reports an error.

On a successful completion that involves a receive (`READ`, or `WRITE_READ`'s read phase), `poll()` runs `hwFinishAsyncReceive()` (`Stm32::InvalidateDCacheForDma()`) before invoking the matching completion port with the final status -- exactly once per accepted request.

**Known limitation**: this project's current CubeMX configuration for I2C1 has no DMA request attached (`Hardware/stm32h753_hal/Core/Src/i2c.c`'s `HAL_I2C_MspInit()` never calls `__HAL_LINKDMA()`), so `open(..., TransferMode::DMA)` on I2C1 will fail its `hdmatx`/`hdmarx` assertion today rather than silently dereferencing a null DMA handle during a transaction. Using DMA mode on real hardware requires regenerating the CubeMX project with a DMA request added to the target I2C instance first -- the same prerequisite §3.3 already documents for enabling an instance beyond I2C1 at all.

## 4. Usage

```cpp
// configureTopology(): POLLED is the default -- unchanged from before TransferMode existed.
i2cDriver.open(Stm32::I2cInstance::I2c1, Stm32::I2cBusSpeed::Fast);

// raw sensor probe/register read, e.g. from a test command or adapter component:
Drv::I2cStatus status = i2cDriver.get_write_InputPort(0)->invoke(sensorAddr, regAddrBuffer);
status = i2cDriver.get_writeRead_InputPort(0)->invoke(sensorAddr, regAddrBuffer, readDataBuffer);

// A second instance on a different bus opted into DMA explicitly (requires
// that instance's CubeMX DMA request to be wired first -- see the §3.5
// known limitation):
tempSensorDriver.open(Stm32::I2cInstance::I2c2, Stm32::I2cBusSpeed::Fast, Stm32::TransferMode::DMA);

// Issuing a DMA-mode request: no return value, completion arrives later.
tempSensorDriver.get_writeReadAsync_InputPort(0)->invoke(sensorAddr, regAddrBuffer, readDataBuffer);
// ... the client component's own writeReadAsyncComplete input port handler
//     receives (writeBuffer, readBuffer, status) once poll() delivers it
//     (after both the write and read phases have run).

// Main.cpp's cyclic-executive loop must call poll() every pass for any
// DMA-mode instance -- a no-op if nothing is in flight, so safe to call
// unconditionally even for a POLLED-mode instance:
tempSensorDriver.poll();
```

Note that the caller owns the buffers passed to each port; the driver does not retain or deallocate them.

To use a different I2C instance: enable it in `Stm32Config.hpp` (its `I2Cn_Instance` macro), regenerate the CubeMX project with that peripheral configured, then pass the matching `Stm32::I2cInstance` value to `open()` along with the matching `Stm32::I2cBusSpeed`.
