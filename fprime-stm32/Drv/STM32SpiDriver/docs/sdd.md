# Stm32::Stm32SpiDriver

## 1. Introduction

`Stm32SpiDriver` is a bare-metal implementation of the [`Drv.Spi`](../../../fprime/Drv/Interfaces/Spi.fpp) interface for an STM32H7 SPI peripheral, selected per instance at `open()` time. It is a raw bus-master driver only: there is no sensor-specific register model layered on top -- a sensor component (or a topology-level test command) is expected to drive this component's ports directly, framing its own register-address/data bytes into the write buffer.

Each instance is opened in exactly one transfer mode, chosen by the caller at `open()` time:

- **`Stm32::TransferMode::POLLED`** (the default): `SpiWriteRead`/`SpiReadWrite` perform a single blocking `HAL_SPI_TransmitReceive()` call and return once the transfer is physically complete. No DMA setup, no cache-coherence concern -- the right choice for small, low-rate sensors (e.g. the BMP280).
- **`Stm32::TransferMode::DMA`**: `SpiWriteRead`/`SpiReadWrite` are **not** served in this mode (see §3.4) -- the instance instead serves `spiWriteReadAsync`, a request/completion port pair. A request starts `HAL_SPI_TransmitReceive_DMA()` and returns immediately; the transfer completes asynchronously, and `poll()` -- called every cyclic-executive pass, not from a rate group -- drains the ISR-latched outcome and invokes the completion port. This is the appropriate mode for anything that must not block the cyclic executive while a transfer is in flight: bulk flash access, a radio, or any other higher-rate/latency-sensitive client.

A single component instance is one or the other, fixed for its lifetime after `open()` -- there is no port that works in both modes.

## 2. Requirements

| Name | Description | Validation |
|---|---|---|
| STM32-SPI-COMP-001 | Shall implement the `Drv.Spi` interface | inspection |
| STM32-SPI-COMP-002 | Shall configure a caller-selected SPI instance (via `MX_SPIn_Init()`) and a caller-selected chip-select GPIO pin at `open()`, and reject transactions issued before `open()` | inspection |
| STM32-SPI-COMP-003 | Shall drive the configured chip-select pin low for the duration of each transfer and high otherwise | inspection |
| STM32-SPI-COMP-004 | Shall perform a blocking full-duplex transfer of equal-sized write/read buffers via `SpiWriteRead`/`SpiReadWrite` when opened `POLLED`, bounded by a caller-selected transaction watchdog | inspection |
| STM32-SPI-COMP-005 | Shall support multiple simultaneous instances (different SPI peripheral and/or chip-select pin per instance, independently POLLED or DMA) with no shared mutable HAL state between them | inspection |
| STM32-SPI-COMP-006 | Shall select POLLED (default) or DMA transfer per instance at `open()` time | inspection, test |
| STM32-SPI-COMP-007 | When opened DMA, shall serve `spiWriteReadAsync` requests by starting `HAL_SPI_TransmitReceive_DMA()` and returning immediately, delivering exactly one completion via `spiWriteReadAsyncComplete` per request, from `poll()`, never from an ISR and never from inside the request call | inspection, test |
| STM32-SPI-COMP-008 | A DMA-mode transfer shall clean the D-cache over the write buffer before, and invalidate it over the read buffer after, the transfer, and shall assert both buffers are reachable by the DMA controller (not DTCM) | inspection |
| STM32-SPI-COMP-009 | `poll()` shall recover a DMA-mode transfer that exceeds its watchdog by aborting it and delivering an error completion, and shall be a harmless no-op when no transfer is in flight (including on a POLLED-mode instance) | inspection, test |

## 3. Design

### 3.1 Port model

`import Drv.Spi` provides `SpiWriteRead` (guarded, returns a `Drv::SpiStatus`) and the deprecated `SpiReadWrite` (plain synchronous, no return value -- forwards to `SpiWriteRead` and discards the status, matching `Drv::LinuxSpiDriver`'s own handling of this port). Both ports take the same `ref writeBuffer: Fw.Buffer, ref readBuffer: Fw.Buffer` pair; the two buffers must be the same size, since a SPI transfer is inherently full-duplex (one byte clocked out for every byte clocked in).

Alongside those, `Stm32SpiDriver.fpp` hand-declares a second, asynchronous port pair:

- `spiWriteReadAsync` (guarded input, `Drv.SpiWriteReadRequest`) -- the request. No return value.
- `spiWriteReadAsyncComplete` (output, `Drv.SpiWriteReadCallback`) -- the completion, invoked once per request from `poll()`.

These are hand-declared (array-sized by a locally-defined `Drv.AsyncSpiCfg.SpiDriverPorts` constant) rather than pulled in via `import Drv.AsyncGuardedSpi`, for two reasons specific to this project:

1. **Name collision.** `Drv.Spi` and `Drv.AsyncGuardedSpi` both declare a guarded port literally named `SpiWriteRead`; FPP rejects importing both into one component as a duplicate port name.
2. **This project's pinned `fprime` version (v4.4.1) doesn't have `Drv.AsyncGuardedSpi`/`Drv.AsyncSpi` at all** -- those interfaces, the `Drv.AsyncSpiCfg` config module, and the `Drv.SpiWriteReadRequest`/`Drv.SpiWriteReadCallback` port types were added to `lib/fprime` in a later commit than this project's pin. Rather than upgrading the framework pin, `Drv.AsyncSpiCfg.SpiDriverPorts` and the two port types are defined locally in [`AsyncSpiCfg.fpp`](../STM32SpiDriver/AsyncSpiCfg.fpp), matching the real upstream interface's shape byte-for-byte (so a future framework upgrade that does add them can simply delete this file and switch to `import Drv.AsyncGuardedSpi`). One difference: v4.4.1's `Drv.SpiStatus` enum has no `SPI_BUSY_ERR` value, so this driver reports "busy" (another transfer already in flight) and "wrong mode" (see §3.3) with the existing `SPI_OTHER_ERR` instead.

Both `SpiWriteRead`/`SpiReadWrite` and `spiWriteReadAsync` are deliberately `guarded`, not just `sync`: F´'s generated `*_handlerBase()` wrapper calls `this->lock()` before invoking the handler and `this->unLock()` after, serializing concurrent callers into this component's internal state. For `SpiWriteRead`/`SpiReadWrite` this lock is held for the entire blocking POLLED transfer -- harmless, since nothing else needs to run while it blocks. For `spiWriteReadAsync` the lock is held only long enough to start the DMA transfer and return; the long-running wait for completion happens later, in `poll()`, which is **not** called from inside any guarded port and therefore never holds this lock across the DMA transfer itself. This is what makes `poll()` safe to run on every cyclic-executive pass without starving other guarded callers (see §3.6 for why an earlier design -- blocking inside the guarded call -- was fatal).

The only event is `HalError`, emitted from the HAL boundary whenever a blocking or DMA-start SPI HAL call does not return `HAL_OK`.

### 3.2 Common/Real/Stub split

Following the convention in `lib/fprime-stm32/README.md`, this driver splits into three files sharing one HAL-free header (`Stm32SpiDriver.hpp`):

- `Stm32SpiDriverCommon.cpp` -- port handler logic and the `poll()` state machine, all hardware-independent. Always built.
- `Stm32SpiDriver.cpp` -- the real HAL boundary (`open`/`hwSetCs`/`hwTransmitReceive`/`hwStartAsyncTransfer`/`hwAbortAsyncTransfer`/`hwFinishAsyncTransfer`), built only for the `stm32h7` target. The only file allowed to include `spi.h`/call `HAL_SPI_*`/`HAL_GPIO_*`.
- `Stm32SpiDriverStub.cpp` -- the same boundary methods against injectable/observable stub state, built for host UT and any non-`stm32h7` build. Never included in a flight build.

### 3.3 `open(instance, csPort, csPin, timeoutMs, mode)`

`open()` takes an `SpiInstance` (`Spi1`-`Spi6`, mirroring `Stm32I2cDriver`'s `I2cInstance` switch), a chip-select GPIO identifier (`Stm32::GpioPort` + pin -- the same vocabulary `Stm32GpioDriver::open()` uses, so no new "which pin" type is invented), a per-transaction timeout in milliseconds (default 10 ms, matching `Stm32I2cDriver`'s watchdog convention), and a `Stm32::TransferMode` (default `POLLED`, trailing both other params so every existing call site keeps compiling and keeps its current, polled behavior unchanged). It calls the CubeMX-generated `MX_SPIn_Init()` for the selected instance, then configures the chip-select pin as a push-pull output, idle HIGH (deselected) -- SPI5 is configured `NSS_SOFT` in this project's CubeMX setup, so the HAL never drives any CS pin on its own; this driver owns that GPIO directly, the same way it owns the SPI peripheral.

When `mode` is `DMA`, `open()` additionally registers `{halHandle, this}` into a small fixed-size table (`s_spiRegistry`, sized to `SpiInstance`'s cardinality) so `HAL_SPI_TxRxCpltCallback()`/`HAL_SPI_ErrorCallback()` -- plain HAL callbacks with no instance/context argument -- can route back to the correct component instance, the same registry pattern `Stm32UartDriver` uses for its own ISR callbacks (see its `docs/sdd.md` §3.7). A POLLED instance is never registered -- this is why `spiWriteReadAsync` rejects every request on a POLLED instance (see below): without a registry entry, the completion ISR would have nothing to call back into, and the request would silently run to its own watchdog timeout on every attempt instead of failing fast.

Only `Spi5` has a CubeMX-generated handle/init function in this project today -- selecting another instance asserts in the real HAL boundary (`Stm32SpiDriver.cpp`'s `toHalHandle()`/`callInstanceInit()`). Adding a second instance for a different board only requires generating that peripheral's CubeMX config and one switch case in each of those two functions; the header, `Common.cpp`, and the `Stub` are already instance-agnostic.

**Multi-instance safety**: this driver stores only HAL-free state on the component itself (the `SpiInstance` enum, the `GpioPort`+pin, the timeout, the transfer mode) and re-resolves the actual HAL pointers from that state on every `hwSetCs()`/`hwTransmitReceive()`/`hwStartAsyncTransfer()` call, matching the same per-instance approach every STM32 driver in this library now uses. This is deliberate: a BMP280 and any future SPI sensor on a different bus or chip-select need two simultaneously-open `Stm32SpiDriver` instances that never share mutable state -- and the two instances may independently be POLLED or DMA.

`MX_SPIn_Init()` traps in `Error_Handler()` on failure rather than returning a status, matching every other `MX_*_Init()` in this project, so `open()` cannot observe a failure there.

### 3.4 `SpiWriteRead`/`SpiReadWrite` (POLLED mode)

The handler (`Stm32SpiDriverCommon.cpp`) asserts both of the caller's `Fw::Buffer`s are non-null and the same size, drives chip-select low, calls `hwTransmitReceive()` (a single blocking `HAL_SPI_TransmitReceive()` call bounded by the `open()`-time timeout), and drives chip-select high again -- unconditionally, even on a HAL failure, so a failed transfer never leaves the device selected. `SpiWriteRead`/`SpiReadWrite` are available regardless of the instance's `TransferMode`, but a HAL call issued against a handle whose peripheral is mid-DMA-transfer on a *sibling* instance sharing the same `SpiInstance` (different chip-select, same bus) can observe `HAL_BUSY`; `hwTransmitReceive()` retries within its own timeout budget in that case rather than failing outright, since this is a microseconds-scale race, not a real error.

### 3.5 `spiWriteReadAsync`/`poll()` (DMA mode)

`spiWriteReadAsync_handler()` (`Stm32SpiDriverCommon.cpp`):

1. Returns `SPI_OPEN_ERR` immediately if the instance hasn't been `open()`ed.
2. Returns `SPI_OTHER_ERR` immediately if the instance was not opened with `TransferMode::DMA` (see §3.3 for why this instance was never registered for ISR routing) -- and the same `SPI_OTHER_ERR` if a transfer is already in flight on this instance.
3. Otherwise, drives chip-select low, calls `hwStartAsyncTransfer()` (`Stm32::AssertDmaSafe()` on both buffers, `Stm32::CleanDCacheForDma()` on the write buffer, `HAL_SPI_TransmitReceive_DMA()`), and returns. A HAL failure to even *start* the transfer is reported immediately via `spiWriteReadAsyncComplete_out()` with `SPI_WRITE_ERR`; a successful start leaves the transfer in flight for `poll()` to observe later -- **never** from inside this handler call.

`poll()` is called every cyclic-executive pass (see `Main.cpp`'s `while(true)` loop, not from a rate group, so completions are serviced with minimal latency) and is a no-op whenever no `spiWriteReadAsync` request is outstanding -- including, trivially, on a POLLED-mode instance, which never sets its in-flight flag. When a transfer is outstanding:

- If the ISR (`HAL_SPI_TxRxCpltCallback()`/`HAL_SPI_ErrorCallback()`) has already latched an outcome, `poll()` picks it up directly.
- Otherwise it checks the per-transfer watchdog (an `Os::RawTime`-timed comparison against the `open()`-time timeout); if still within budget it returns and tries again next pass; if exceeded, it calls `hwAbortAsyncTransfer()` (`HAL_SPI_Abort()` -- completes via the same ISR) and reports `SPI_OTHER_ERR`.

On a successful completion, `poll()` drives chip-select high, runs `hwFinishAsyncTransfer()` (`Stm32::InvalidateDCacheForDma()` on the read buffer), and invokes `spiWriteReadAsyncComplete_out()` with the original write/read buffers and the final status -- exactly once per accepted request.

**Known limitation**: this project's current CubeMX configuration for SPI5 has no DMA request attached (`Hardware/stm32h753_hal/Core/Src/spi.c`'s `HAL_SPI_MspInit()` never calls `__HAL_LINKDMA()`), so `open(..., TransferMode::DMA)` on Spi5 will fail an `hdmatx`/`hdmarx` assertion in `open()` today rather than silently dereferencing a null DMA handle during a transfer. Using DMA mode on real hardware (e.g. for bulk flash access on SPI1, or the BMP280 on SPI5) requires regenerating the CubeMX project with a DMA request added to that instance first -- the same prerequisite §3.3 already documents for enabling an instance beyond SPI5 at all.

### 3.6 Why DMA mode is a separate async port, not a blocking wait inside the guarded call

An earlier version of this driver served DMA transfers through `SpiWriteRead` itself: `hwTransmitReceive()` would start `HAL_SPI_TransmitReceive_DMA()` and then busy-wait for completion *inside* the guarded port call, temporarily calling `this->unLock()` before the wait and `this->lock()` after -- because on this bare-metal target, `lock()`/`unLock()` resolve to `Os::Mutex::take()`/`release()` (`Os/Stm32H7/Mutex.cpp`), which preserve/restore `PRIMASK` around `__disable_irq()`. Left locked for the whole call, the SPI/DMA completion interrupt that the busy-wait depends on could never fire, so the wait would spin to its own timeout on every single DMA transfer, unconditionally -- this is exactly the bug `Stm32I2cDriver`'s identical DMA path hit the first time it was run on real hardware.

The unlock/relock trick made that work, but only by relying on this executive being cooperative and non-preemptive (nothing but an ISR can run while the port is momentarily unlocked, and this codebase's ISRs never make F´ port calls) -- a fragile invariant to depend on for every future DMA-mode caller. The current design removes the dependency entirely: `spiWriteReadAsync_handler()` only *starts* the transfer and returns, so the guarded lock is held for a bounded, short duration, never across the DMA wait. `poll()`, which performs the actual wait/recovery, runs outside any guarded port, so it can never be blocked by -- or block -- a concurrent guarded caller. This is the same shape `Stm32UartDriver` already used for its own DMA-backed TX/RX ring buffer, and the same shape `Stm32I2cDriver`'s own async ports now use (see its `docs/sdd.md` §3.6).

## 4. Usage

```cpp
// configureTopology(): POLLED is the default -- unchanged from before TransferMode existed.
spiDriver.open(Stm32::SpiInstance::Spi5, Stm32::GpioPort::F, /* csPin */ 6);

Fw::Buffer writeBuffer(regAddrAndDummyBytes, size);
Fw::Buffer readBuffer(responseBytes, size);
Drv::SpiStatus status = spiDriver.get_SpiWriteRead_InputPort(0)->invoke(writeBuffer, readBuffer);

// A second instance on a different bus, e.g. bulk flash access on SPI1,
// opted into DMA explicitly (requires that instance's CubeMX DMA request
// to be wired first -- see the §3.5 known limitation):
flashSpiDriver.open(Stm32::SpiInstance::Spi1, Stm32::GpioPort::A, /* csPin */ 4,
                     /* timeoutMs */ 50, Stm32::TransferMode::DMA);

// Issuing a DMA-mode request: no return value, completion arrives later.
flashSpiDriver.get_spiWriteReadAsync_InputPort(0)->invoke(writeBuffer, readBuffer);
// ... the client component's own spiWriteReadAsyncComplete input port handler
//     receives (writeBuffer, readBuffer, status) once poll() delivers it.

// Main.cpp's cyclic-executive loop must call poll() every pass for any
// DMA-mode instance -- a no-op if nothing is in flight, so safe to call
// unconditionally even for a POLLED-mode instance:
flashSpiDriver.poll();
```

Note that the caller owns the buffers passed to each port; the driver does not retain or deallocate them, and `writeBuffer`/`readBuffer` must be the same size (SPI's full-duplex contract -- one byte out for every byte in) for every port, sync or async.

To use a different SPI instance: enable it in `Stm32Config.hpp` (its `SPIn_INSTANCE` macro), regenerate the CubeMX project with that peripheral configured, then pass the matching `Stm32::SpiInstance` value to `open()` along with that instance's own chip-select pin.
