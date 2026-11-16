// ======================================================================
// \title  Stm32SpiDriverTester.hpp
// \author ivanlara
// \brief  hpp file for Stm32SpiDriver component test harness implementation class
// ======================================================================

#ifndef Stm32_Stm32SpiDriverTester_HPP
#define Stm32_Stm32SpiDriverTester_HPP

#include "fprime-stm32/Drv/STM32SpiDriver/Stm32SpiDriver.hpp"
#include "fprime-stm32/Drv/STM32SpiDriver/Stm32SpiDriverGTestBase.hpp"

namespace Stm32 {

class Stm32SpiDriverTester final : public Stm32SpiDriverGTestBase {
  public:
    // ----------------------------------------------------------------------
    // Constants
    // ----------------------------------------------------------------------

    // Maximum size of histories storing events, telemetry, and port outputs
    static const FwSizeType MAX_HISTORY_SIZE = 10;

    // Instance ID supplied to the component instance under test
    static const FwEnumStoreType TEST_INSTANCE_ID = 0;

  public:
    // ----------------------------------------------------------------------
    // Construction and destruction
    // ----------------------------------------------------------------------

    //! Construct object Stm32SpiDriverTester
    Stm32SpiDriverTester();

    //! Destroy object Stm32SpiDriverTester
    ~Stm32SpiDriverTester();

  public:
    // ----------------------------------------------------------------------
    // Tests
    // ----------------------------------------------------------------------

    //! open() succeeds, forwards instance/CS to the HAL boundary, and emits PortOpened
    void testOpenSuccess();

    //! open() reports FAILURE and leaves the driver unopened when the HAL
    //! boundary's init fails (injected via Stub_hwOpenSucceeds)
    void testOpenFailure();

    //! open() without an explicit mode argument defaults to POLLED (backward compatibility)
    void testOpenDefaultsToPolled();

    //! open() forwards an explicit DMA mode request to the HAL boundary
    void testOpenDma();

    //! spiWriteReadAsync() starts a transfer and returns without delivering
    //! a completion; poll() later delivers it with the exact tx/rx bytes,
    //! never from inside the request call
    void testSpiWriteReadAsyncSuccess();

    //! spiWriteReadAsync() reports a HAL boundary failure to START the
    //! transfer immediately (synchronously) and emits HalError
    void testSpiWriteReadAsyncFailure();

    //! A second spiWriteReadAsync() request while one is already in flight
    //! is refused immediately with SPI_OTHER_ERR; the first request's own
    //! completion still arrives normally via a later poll()
    void testSpiWriteReadAsyncBusyRejectsSecondRequest();

    //! poll() recovers via its own watchdog and reports SPI_OTHER_ERR when
    //! the completion ISR never fires
    void testSpiWriteReadAsyncTimeout();

    //! spiWriteReadAsync() before open() reports SPI_OPEN_ERR immediately
    void testSpiWriteReadAsyncBeforeOpen();

    //! Issuing spiWriteReadAsync() on a POLLED-mode instance is refused
    //! immediately with SPI_OTHER_ERR -- a POLLED instance never registers
    //! for ISR callback routing, so starting a DMA transfer on one would
    //! otherwise hang until poll()'s watchdog
    void testSpiWriteReadAsyncRejectedWhenNotDma();

    //! poll() is a harmless no-op on an instance with no in-flight async
    //! transfer (including a POLLED-mode instance, which never starts one)
    void testPollNoOpWhenIdle();

    //! Re-opening the same component instance with a different mode switches
    //! its transfer behavior -- proves TransferMode is per-open() instance
    //! state, not shared/global
    void testReopenSwitchesMode();

    //! SpiWriteRead()/SpiReadWrite() before open() report/return SPI_OPEN_ERR
    void testSpiWriteReadBeforeOpen();
    void testSpiReadWriteBeforeOpen();

    //! SpiWriteRead() asserts CS active for the duration of the transfer and
    //! forwards the exact write bytes / returns the exact read bytes
    void testSpiWriteReadSuccess();

    //! SpiWriteRead() propagates a HAL boundary failure status and emits HalError
    void testSpiWriteReadFailure();

    //! SpiReadWrite() performs the same transfer as SpiWriteRead() but discards the status
    void testSpiReadWriteSuccess();

    //! A write buffer larger than the stub's capture buffer is truncated, not overrun
    void testSpiWriteReadLargeBufferCapped();

  private:
    // ----------------------------------------------------------------------
    // Test support
    // ----------------------------------------------------------------------

    //! Reset every Stub_* global back to its documented default so each
    //! test starts from a known, hermetic state regardless of run order --
    //! these are shared, process-wide globals (see Stm32SpiDriverStub.cpp),
    //! not per-Tester state.
    void resetStubState();

  private:
    // ----------------------------------------------------------------------
    // Helper functions
    // ----------------------------------------------------------------------

    //! Connect ports
    void connectPorts();

    //! Initialize components
    void initComponents();

  private:
    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    //! The component under test
    Stm32SpiDriver component;
};

}  // namespace Stm32

#endif
