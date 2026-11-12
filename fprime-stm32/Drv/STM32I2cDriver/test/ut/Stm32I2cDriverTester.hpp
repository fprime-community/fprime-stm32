// ======================================================================
// \title  Stm32I2cDriverTester.hpp
// \author ivanlara
// \brief  hpp file for Stm32I2cDriver component test harness implementation class
// ======================================================================

#ifndef Stm32_Stm32I2cDriverTester_HPP
#define Stm32_Stm32I2cDriverTester_HPP

#include "fprime-stm32/Drv/STM32I2cDriver/Stm32I2cDriver.hpp"
#include "fprime-stm32/Drv/STM32I2cDriver/Stm32I2cDriverGTestBase.hpp"

namespace Stm32 {

class Stm32I2cDriverTester final : public Stm32I2cDriverGTestBase {
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

    //! Construct object Stm32I2cDriverTester
    Stm32I2cDriverTester();

    //! Destroy object Stm32I2cDriverTester
    ~Stm32I2cDriverTester();

  public:
    // ----------------------------------------------------------------------
    // Tests
    // ----------------------------------------------------------------------

    //! open() at the default (Fast) speed succeeds and emits PortOpened
    void testOpenSuccessFast();

    //! open() at Standard speed succeeds and reports the right speed string
    void testOpenSuccessStandard();

    //! open() at FastPlus speed succeeds and reports the right speed string
    void testOpenSuccessFastPlus();

    //! open() reports FAILURE and leaves the driver unopened when the HAL
    //! boundary's init fails (injected via Stub_hwOpenSucceeds)
    void testOpenFailure();

    //! open() without an explicit mode argument defaults to POLLED (backward compatibility)
    void testOpenDefaultsToPolled();

    //! open() forwards an explicit DMA mode request to the HAL boundary
    void testOpenDma();

    //! writeAsync()/readAsync() start a transaction and return without
    //! delivering a completion; poll() later delivers it with the exact
    //! bytes, never from inside the request call
    void testWriteAsyncSuccess();
    void testReadAsyncSuccess();

    //! writeAsync()/readAsync()/writeReadAsync() report a HAL boundary
    //! failure to START the transaction immediately (synchronously) and
    //! emit HalError
    void testWriteAsyncFailure();
    void testReadAsyncFailure();

    //! A second async request while one is already in flight is refused
    //! immediately with I2C_OTHER_ERR; the first request's own completion
    //! still arrives normally via a later poll()
    void testWriteAsyncBusyRejectsSecondRequest();

    //! poll() recovers via its own watchdog and reports an error when the
    //! completion ISR never fires
    void testWriteAsyncTimeout();

    //! writeAsync()/readAsync()/writeReadAsync() before open() all report
    //! I2C_OPEN_ERR immediately
    void testWriteAsyncBeforeOpen();
    void testReadAsyncBeforeOpen();
    void testWriteReadAsyncBeforeOpen();

    //! Issuing writeAsync()/readAsync()/writeReadAsync() on a POLLED-mode
    //! instance is refused immediately with I2C_OTHER_ERR -- a POLLED
    //! instance never registers for ISR callback routing, so starting a
    //! DMA transaction on one would otherwise hang until poll()'s watchdog
    void testWriteAsyncRejectedWhenNotDma();

    //! writeReadAsync() runs the write phase, then (once it completes) the
    //! read phase, delivering exactly one combined completion
    void testWriteReadAsyncSuccess();

    //! writeReadAsync() short-circuits on a failed write phase: the read
    //! phase must never start
    void testWriteReadAsyncWriteFailureShortCircuits();

    //! writeReadAsync() propagates a failure from the read phase after a
    //! successful write phase
    void testWriteReadAsyncReadPhaseFailure();

    //! poll() is a harmless no-op on an instance with no in-flight async
    //! transaction (including a POLLED-mode instance, which never starts one)
    void testPollNoOpWhenIdle();

    //! write()/read()/writeRead() before open() all report I2C_OPEN_ERR
    void testWriteBeforeOpen();
    void testReadBeforeOpen();
    void testWriteReadBeforeOpen();

    //! write() forwards the exact address and bytes to the HAL boundary
    void testWriteSuccess();

    //! write() propagates a HAL boundary failure status unchanged
    void testWriteFailure();

    //! A write buffer larger than the stub's capture buffer is truncated,
    //! not overrun
    void testWriteLargeBufferCapped();

    //! read() forwards the exact address/length and returns the bytes the
    //! HAL boundary supplied
    void testReadSuccess();

    //! read() propagates a HAL boundary failure status unchanged
    void testReadFailure();

    //! A read request larger than the stub's canned response is only
    //! partially filled, not overrun; the requested length is still
    //! reported in full
    void testReadLargeBufferCapped();

    //! writeRead() performs the write then the read to the same address
    void testWriteReadSuccess();

    //! writeRead() short-circuits on a failed write: the read must never run
    void testWriteReadTransmitFailureShortCircuits();

    //! writeRead() propagates a failure from the read half after a
    //! successful write
    void testWriteReadReceiveFailure();

    //! Two simultaneously-open Stm32I2cDriver instances (different buses,
    //! and -- now -- independently POLLED or DMA) don't share any mutable
    //! state: opening a second instance on I2c2 in DMA mode after the first
    //! is already open on I2c1 in POLLED mode must not disturb the first
    //! instance's own ability to transact.
    void testTwoInstancesDoNotInterfere();

  private:
    // ----------------------------------------------------------------------
    // Test support
    // ----------------------------------------------------------------------

    //! Reset every Stub_* global back to its documented default so each
    //! test starts from a known, hermetic state regardless of run order --
    //! these are shared, process-wide globals (see Stm32I2cDriverStub.cpp),
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
    Stm32I2cDriver component;
};

}  // namespace Stm32

#endif
