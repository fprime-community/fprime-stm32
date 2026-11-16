// ======================================================================
// \title  Stm32I2cDriverTester.cpp
// \author ivanlara
// \brief  cpp file for Stm32I2cDriver component test harness implementation class
// ======================================================================

#include "Stm32I2cDriverTester.hpp"

#include <Fw/Time/TimeInterval.hpp>
#include <Os/Task.hpp>

// Stub_* globals defined in Stm32I2cDriverStub.cpp -- declared here so this
// Tester can inject HAL boundary success/failure and inspect exactly what
// Stm32I2cDriverCommon.cpp drove through it, instead of only exercising the
// stub's fixed defaults.
extern bool Stub_hwOpenSucceeds;
extern Drv::I2cStatus Stub_hwMasterTransmitStatus;
extern Drv::I2cStatus Stub_hwMasterReceiveStatus;
extern U8 Stub_readResponseData[32];
extern Stm32::I2cInstance Stub_lastOpenedInstance;
extern Stm32::I2cBusSpeed Stub_lastRequestedBusSpeed;
extern U16 Stub_lastDevAddress;
extern U8 Stub_lastWriteData[32];
extern U16 Stub_lastWriteLen;
extern U16 Stub_lastReadLen;
extern Stm32::TransferMode Stub_lastOpenedMode;
extern I32 Stub_hwStartAsyncTransmitStatus;
extern I32 Stub_hwStartAsyncReceiveStatus;
extern bool Stub_hwAbortAsyncTransactionCalled;
extern bool Stub_hwFinishAsyncReceiveCalled;
extern bool Stub_hwIsBusy;

namespace Stm32 {

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

Stm32I2cDriverTester ::Stm32I2cDriverTester()
    : Stm32I2cDriverGTestBase("Stm32I2cDriverTester", Stm32I2cDriverTester::MAX_HISTORY_SIZE),
      component("Stm32I2cDriver") {
    this->resetStubState();
    this->initComponents();
    this->connectPorts();
}

Stm32I2cDriverTester ::~Stm32I2cDriverTester() {
    this->component.deinit();
}

void Stm32I2cDriverTester ::resetStubState() {
    Stub_hwOpenSucceeds = true;
    Stub_hwMasterTransmitStatus = Drv::I2cStatus::I2C_OK;
    Stub_hwMasterReceiveStatus = Drv::I2cStatus::I2C_OK;
    for (FwSizeType i = 0; i < sizeof(Stub_readResponseData); i++) {
        Stub_readResponseData[i] = 0;
    }
    Stub_lastOpenedInstance = I2cInstance::I2c1;
    Stub_lastRequestedBusSpeed = I2cBusSpeed::Fast;
    Stub_lastDevAddress = 0;
    for (FwSizeType i = 0; i < sizeof(Stub_lastWriteData); i++) {
        Stub_lastWriteData[i] = 0;
    }
    Stub_lastWriteLen = 0;
    Stub_lastReadLen = 0;
    Stub_lastOpenedMode = Stm32::TransferMode::POLLED;
    Stub_hwStartAsyncTransmitStatus = 0;
    Stub_hwStartAsyncReceiveStatus = 0;
    Stub_hwAbortAsyncTransactionCalled = false;
    Stub_hwFinishAsyncReceiveCalled = false;
    Stub_hwIsBusy = false;
}

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

void Stm32I2cDriverTester ::testOpenSuccessFast() {
    const Fw::Success status = this->component.open(I2cInstance::I2c1, I2cBusSpeed::Fast);
    ASSERT_EQ(status, Fw::Success::SUCCESS);
    ASSERT_EQ(Stub_lastOpenedInstance, I2cInstance::I2c1);
    ASSERT_EQ(Stub_lastRequestedBusSpeed, I2cBusSpeed::Fast);
    ASSERT_EVENTS_PortOpened_SIZE(1);
    ASSERT_EVENTS_PortOpened(0, "Fast");
}

void Stm32I2cDriverTester ::testOpenSuccessStandard() {
    const Fw::Success status = this->component.open(I2cInstance::I2c1, I2cBusSpeed::Standard);
    ASSERT_EQ(status, Fw::Success::SUCCESS);
    ASSERT_EQ(Stub_lastRequestedBusSpeed, I2cBusSpeed::Standard);
    ASSERT_EVENTS_PortOpened_SIZE(1);
    ASSERT_EVENTS_PortOpened(0, "Standard");
}

void Stm32I2cDriverTester ::testOpenSuccessFastPlus() {
    const Fw::Success status = this->component.open(I2cInstance::I2c1, I2cBusSpeed::FastPlus);
    ASSERT_EQ(status, Fw::Success::SUCCESS);
    ASSERT_EQ(Stub_lastRequestedBusSpeed, I2cBusSpeed::FastPlus);
    ASSERT_EVENTS_PortOpened_SIZE(1);
    ASSERT_EVENTS_PortOpened(0, "FastPlus");
}

void Stm32I2cDriverTester ::testOpenFailure() {
    Stub_hwOpenSucceeds = false;

    const Fw::Success status = this->component.open(I2cInstance::I2c1);
    ASSERT_EQ(status, Fw::Success::FAILURE);
    ASSERT_EVENTS_PortOpened_SIZE(0);

    // m_opened must have stayed false.
    U8 backing[4] = {0};
    Fw::Buffer buffer(backing, sizeof(backing));
    ASSERT_EQ(this->invoke_to_write(0, 0x50, buffer), Drv::I2cStatus::I2C_OPEN_ERR);
}

void Stm32I2cDriverTester ::testOpenDefaultsToPolled() {
    const Fw::Success status = this->component.open(I2cInstance::I2c1);
    ASSERT_EQ(status, Fw::Success::SUCCESS);
    ASSERT_EQ(Stub_lastOpenedMode, Stm32::TransferMode::POLLED);
}

void Stm32I2cDriverTester ::testOpenDma() {
    const Fw::Success status = this->component.open(I2cInstance::I2c1, I2cBusSpeed::Fast, Stm32::TransferMode::DMA);
    ASSERT_EQ(status, Fw::Success::SUCCESS);
    ASSERT_EQ(Stub_lastOpenedMode, Stm32::TransferMode::DMA);
}

void Stm32I2cDriverTester ::testWriteAsyncSuccess() {
    (void)this->component.open(I2cInstance::I2c1, I2cBusSpeed::Fast, Stm32::TransferMode::DMA);

    U8 data[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    Fw::Buffer buffer(data, sizeof(data));
    this->invoke_to_writeAsync(0, 0x50, buffer);

    // hwStartAsyncTransmit() ran synchronously within the request (the
    // stub has no real ISR), but the completion must not be delivered
    // until poll() runs -- same "never from inside the request call"
    // contract as Stm32SpiDriver::spiWriteReadAsync.
    ASSERT_FROM_PORT_HISTORY_SIZE(0);
    ASSERT_EQ(Stub_lastDevAddress, 0x50);
    ASSERT_EQ(Stub_lastWriteLen, 4u);

    this->component.signalDmaComplete();
    this->component.poll();
    ASSERT_from_writeAsyncComplete_SIZE(1);
    ASSERT_from_writeAsyncComplete(0, buffer, Drv::I2cStatus::I2C_OK);
}

void Stm32I2cDriverTester ::testReadAsyncSuccess() {
    (void)this->component.open(I2cInstance::I2c1, I2cBusSpeed::Fast, Stm32::TransferMode::DMA);
    Stub_readResponseData[0] = 0x11;
    Stub_readResponseData[1] = 0x22;

    U8 backing[2] = {0};
    Fw::Buffer buffer(backing, sizeof(backing));
    this->invoke_to_readAsync(0, 0x50, buffer);
    ASSERT_FROM_PORT_HISTORY_SIZE(0);

    this->component.signalDmaComplete();
    this->component.poll();
    ASSERT_from_readAsyncComplete_SIZE(1);
    ASSERT_from_readAsyncComplete(0, buffer, Drv::I2cStatus::I2C_OK);
    ASSERT_EQ(backing[0], 0x11);
    ASSERT_EQ(backing[1], 0x22);
    ASSERT_TRUE(Stub_hwFinishAsyncReceiveCalled);
}

void Stm32I2cDriverTester ::testWriteAsyncFailure() {
    (void)this->component.open(I2cInstance::I2c1, I2cBusSpeed::Fast, Stm32::TransferMode::DMA);
    Stub_hwStartAsyncTransmitStatus = 1;  // any non-zero (non-HAL_OK) status

    U8 data[1] = {0x00};
    Fw::Buffer buffer(data, sizeof(data));
    this->invoke_to_writeAsync(0, 0x50, buffer);

    // A HAL failure to even START the transaction is reported immediately
    // (there is nothing for poll() to observe later).
    ASSERT_from_writeAsyncComplete_SIZE(1);
    ASSERT_from_writeAsyncComplete(0, buffer, Drv::I2cStatus::I2C_WRITE_ERR);
    ASSERT_EVENTS_HalError_SIZE(1);
}

void Stm32I2cDriverTester ::testReadAsyncFailure() {
    (void)this->component.open(I2cInstance::I2c1, I2cBusSpeed::Fast, Stm32::TransferMode::DMA);
    Stub_hwStartAsyncReceiveStatus = 1;

    U8 backing[1] = {0};
    Fw::Buffer buffer(backing, sizeof(backing));
    this->invoke_to_readAsync(0, 0x50, buffer);
    ASSERT_from_readAsyncComplete_SIZE(1);
    ASSERT_from_readAsyncComplete(0, buffer, Drv::I2cStatus::I2C_READ_ERR);
    ASSERT_EVENTS_HalError_SIZE(1);
}

void Stm32I2cDriverTester ::testWriteAsyncBusyRejectsSecondRequest() {
    (void)this->component.open(I2cInstance::I2c1, I2cBusSpeed::Fast, Stm32::TransferMode::DMA);

    U8 data1[1] = {0x10};
    Fw::Buffer buffer1(data1, sizeof(data1));
    this->invoke_to_writeAsync(0, 0x50, buffer1);
    ASSERT_FROM_PORT_HISTORY_SIZE(0);  // first request still in flight

    U8 data2[1] = {0x20};
    Fw::Buffer buffer2(data2, sizeof(data2));
    this->invoke_to_writeAsync(0, 0x51, buffer2);

    // The second request is refused synchronously -- never waiting on the
    // first transaction's completion.
    ASSERT_from_writeAsyncComplete_SIZE(1);
    ASSERT_from_writeAsyncComplete(0, buffer2, Drv::I2cStatus::I2C_OTHER_ERR);

    // The first request's own completion still arrives normally.
    this->component.signalDmaComplete();
    this->component.poll();
    ASSERT_from_writeAsyncComplete_SIZE(2);
    ASSERT_from_writeAsyncComplete(1, buffer1, Drv::I2cStatus::I2C_OK);
}

void Stm32I2cDriverTester ::testWriteAsyncTimeout() {
    (void)this->component.open(I2cInstance::I2c1, I2cBusSpeed::Fast, Stm32::TransferMode::DMA);

    U8 data[1] = {0x00};
    Fw::Buffer buffer(data, sizeof(data));
    this->invoke_to_writeAsync(0, 0x50, buffer);
    ASSERT_FROM_PORT_HISTORY_SIZE(0);

    // The stub never calls signalDmaComplete() on its own, so the
    // transaction is still "in flight" after TRANSACTION_TIMEOUT_MS (10ms)
    // elapses; wait comfortably longer, matching
    // Stm32SpiDriverTester::testSpiWriteReadAsyncTimeout()'s pattern.
    (void)Os::Task::delay(Fw::TimeInterval(0, 20000));  // 20 ms
    this->component.poll();

    ASSERT_from_writeAsyncComplete_SIZE(1);
    ASSERT_from_writeAsyncComplete(0, buffer, Drv::I2cStatus::I2C_WRITE_ERR);
    ASSERT_TRUE(Stub_hwAbortAsyncTransactionCalled);
    ASSERT_EVENTS_HalError_SIZE(1);
}

void Stm32I2cDriverTester ::testWriteAsyncBeforeOpen() {
    U8 data[1] = {0x00};
    Fw::Buffer buffer(data, sizeof(data));
    this->invoke_to_writeAsync(0, 0x50, buffer);
    ASSERT_from_writeAsyncComplete_SIZE(1);
    ASSERT_from_writeAsyncComplete(0, buffer, Drv::I2cStatus::I2C_OPEN_ERR);
}

void Stm32I2cDriverTester ::testReadAsyncBeforeOpen() {
    U8 backing[1] = {0};
    Fw::Buffer buffer(backing, sizeof(backing));
    this->invoke_to_readAsync(0, 0x50, buffer);
    ASSERT_from_readAsyncComplete_SIZE(1);
    ASSERT_from_readAsyncComplete(0, buffer, Drv::I2cStatus::I2C_OPEN_ERR);
}

void Stm32I2cDriverTester ::testWriteReadAsyncBeforeOpen() {
    U8 wbacking[1] = {0};
    U8 rbacking[1] = {0};
    Fw::Buffer writeBuffer(wbacking, sizeof(wbacking));
    Fw::Buffer readBuffer(rbacking, sizeof(rbacking));
    this->invoke_to_writeReadAsync(0, 0x50, writeBuffer, readBuffer);
    ASSERT_from_writeReadAsyncComplete_SIZE(1);
    ASSERT_from_writeReadAsyncComplete(0, writeBuffer, readBuffer, Drv::I2cStatus::I2C_OPEN_ERR);
}

void Stm32I2cDriverTester ::testWriteAsyncRejectedWhenNotDma() {
    (void)this->component.open(I2cInstance::I2c1, I2cBusSpeed::Fast, Stm32::TransferMode::POLLED);

    U8 data[1] = {0x00};
    Fw::Buffer buffer(data, sizeof(data));
    this->invoke_to_writeAsync(0, 0x50, buffer);
    ASSERT_from_writeAsyncComplete_SIZE(1);
    ASSERT_from_writeAsyncComplete(0, buffer, Drv::I2cStatus::I2C_OTHER_ERR);
}

void Stm32I2cDriverTester ::testWriteReadAsyncSuccess() {
    (void)this->component.open(I2cInstance::I2c1, I2cBusSpeed::Fast, Stm32::TransferMode::DMA);
    Stub_readResponseData[0] = 0x99;

    U8 wdata[1] = {0x10};
    U8 rbacking[1] = {0};
    Fw::Buffer writeBuffer(wdata, sizeof(wdata));
    Fw::Buffer readBuffer(rbacking, sizeof(rbacking));
    this->invoke_to_writeReadAsync(0, 0x50, writeBuffer, readBuffer);
    ASSERT_FROM_PORT_HISTORY_SIZE(0);

    // Write phase completes -- poll() must start the read phase, not
    // deliver yet (only one completion for the whole operation).
    this->component.signalDmaComplete();
    this->component.poll();
    ASSERT_FROM_PORT_HISTORY_SIZE(0);
    ASSERT_EQ(Stub_lastReadLen, 1u);

    this->component.signalDmaComplete();
    this->component.poll();
    ASSERT_from_writeReadAsyncComplete_SIZE(1);
    ASSERT_from_writeReadAsyncComplete(0, writeBuffer, readBuffer, Drv::I2cStatus::I2C_OK);
    ASSERT_EQ(rbacking[0], 0x99);
}

void Stm32I2cDriverTester ::testWriteReadAsyncWriteFailureShortCircuits() {
    (void)this->component.open(I2cInstance::I2c1, I2cBusSpeed::Fast, Stm32::TransferMode::DMA);
    Stub_hwStartAsyncTransmitStatus = 1;
    Stub_lastReadLen = 999;  // sentinel: must stay untouched if the read phase never starts

    U8 wdata[1] = {0x10};
    U8 rbacking[1] = {0};
    Fw::Buffer writeBuffer(wdata, sizeof(wdata));
    Fw::Buffer readBuffer(rbacking, sizeof(rbacking));
    this->invoke_to_writeReadAsync(0, 0x50, writeBuffer, readBuffer);
    ASSERT_from_writeReadAsyncComplete_SIZE(1);
    ASSERT_from_writeReadAsyncComplete(0, writeBuffer, readBuffer, Drv::I2cStatus::I2C_WRITE_ERR);
    ASSERT_EQ(Stub_lastReadLen, 999u);
}

void Stm32I2cDriverTester ::testWriteReadAsyncReadPhaseFailure() {
    (void)this->component.open(I2cInstance::I2c1, I2cBusSpeed::Fast, Stm32::TransferMode::DMA);
    Stub_hwStartAsyncReceiveStatus = 1;

    U8 wdata[1] = {0x10};
    U8 rbacking[1] = {0};
    Fw::Buffer writeBuffer(wdata, sizeof(wdata));
    Fw::Buffer readBuffer(rbacking, sizeof(rbacking));
    this->invoke_to_writeReadAsync(0, 0x50, writeBuffer, readBuffer);

    // Write phase completes successfully; the read phase's own HAL start
    // then fails, delivering immediately rather than waiting on poll().
    this->component.signalDmaComplete();
    this->component.poll();
    ASSERT_from_writeReadAsyncComplete_SIZE(1);
    ASSERT_from_writeReadAsyncComplete(0, writeBuffer, readBuffer, Drv::I2cStatus::I2C_READ_ERR);
}

void Stm32I2cDriverTester ::testPollNoOpWhenIdle() {
    (void)this->component.open(I2cInstance::I2c1, I2cBusSpeed::Fast, Stm32::TransferMode::POLLED);
    // No async request was ever issued on this POLLED-mode instance --
    // poll() must be a harmless no-op, confirming it is safe to call
    // unconditionally regardless of an instance's mode.
    this->component.poll();
    ASSERT_FROM_PORT_HISTORY_SIZE(0);
}

void Stm32I2cDriverTester ::testWriteBeforeOpen() {
    U8 backing[4] = {1, 2, 3, 4};
    Fw::Buffer buffer(backing, sizeof(backing));
    ASSERT_EQ(this->invoke_to_write(0, 0x50, buffer), Drv::I2cStatus::I2C_OPEN_ERR);
}

void Stm32I2cDriverTester ::testReadBeforeOpen() {
    U8 backing[4] = {0};
    Fw::Buffer buffer(backing, sizeof(backing));
    ASSERT_EQ(this->invoke_to_read(0, 0x50, buffer), Drv::I2cStatus::I2C_OPEN_ERR);
}

void Stm32I2cDriverTester ::testWriteReadBeforeOpen() {
    U8 wbacking[2] = {0};
    U8 rbacking[2] = {0};
    Fw::Buffer writeBuffer(wbacking, sizeof(wbacking));
    Fw::Buffer readBuffer(rbacking, sizeof(rbacking));
    ASSERT_EQ(this->invoke_to_writeRead(0, 0x50, writeBuffer, readBuffer), Drv::I2cStatus::I2C_OPEN_ERR);
}

void Stm32I2cDriverTester ::testWriteSuccess() {
    (void)this->component.open(I2cInstance::I2c1);

    U8 data[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    Fw::Buffer buffer(data, sizeof(data));
    const Drv::I2cStatus status = this->invoke_to_write(0, 0x50, buffer);
    ASSERT_EQ(status, Drv::I2cStatus::I2C_OK);
    ASSERT_EQ(Stub_lastDevAddress, 0x50);
    ASSERT_EQ(Stub_lastWriteLen, 4u);
    for (FwSizeType i = 0; i < 4; i++) {
        ASSERT_EQ(Stub_lastWriteData[i], data[i]);
    }
}

void Stm32I2cDriverTester ::testWriteFailure() {
    (void)this->component.open(I2cInstance::I2c1);
    Stub_hwMasterTransmitStatus = Drv::I2cStatus::I2C_ADDRESS_ERR;

    U8 data[1] = {0x00};
    Fw::Buffer buffer(data, sizeof(data));
    const Drv::I2cStatus status = this->invoke_to_write(0, 0x50, buffer);
    ASSERT_EQ(status, Drv::I2cStatus::I2C_ADDRESS_ERR);
}

void Stm32I2cDriverTester ::testWriteLargeBufferCapped() {
    (void)this->component.open(I2cInstance::I2c1);

    // Larger than Stub_lastWriteData's 32-byte capture buffer.
    U8 data[40];
    for (FwSizeType i = 0; i < sizeof(data); i++) {
        data[i] = static_cast<U8>(i);
    }
    Fw::Buffer buffer(data, sizeof(data));
    const Drv::I2cStatus status = this->invoke_to_write(0, 0x50, buffer);
    ASSERT_EQ(status, Drv::I2cStatus::I2C_OK);

    // The stub must truncate its capture rather than overrun its own
    // 32-byte buffer; the real HAL call underneath would see the full 40.
    ASSERT_EQ(Stub_lastWriteLen, 32u);
    for (FwSizeType i = 0; i < 32; i++) {
        ASSERT_EQ(Stub_lastWriteData[i], data[i]);
    }
}

void Stm32I2cDriverTester ::testReadSuccess() {
    (void)this->component.open(I2cInstance::I2c1);
    Stub_readResponseData[0] = 0x11;
    Stub_readResponseData[1] = 0x22;

    U8 backing[2] = {0};
    Fw::Buffer buffer(backing, sizeof(backing));
    const Drv::I2cStatus status = this->invoke_to_read(0, 0x50, buffer);
    ASSERT_EQ(status, Drv::I2cStatus::I2C_OK);
    ASSERT_EQ(Stub_lastDevAddress, 0x50);
    ASSERT_EQ(Stub_lastReadLen, 2u);
    ASSERT_EQ(backing[0], 0x11);
    ASSERT_EQ(backing[1], 0x22);
}

void Stm32I2cDriverTester ::testReadFailure() {
    (void)this->component.open(I2cInstance::I2c1);
    Stub_hwMasterReceiveStatus = Drv::I2cStatus::I2C_READ_ERR;

    U8 backing[1] = {0};
    Fw::Buffer buffer(backing, sizeof(backing));
    const Drv::I2cStatus status = this->invoke_to_read(0, 0x50, buffer);
    ASSERT_EQ(status, Drv::I2cStatus::I2C_READ_ERR);
}

void Stm32I2cDriverTester ::testReadLargeBufferCapped() {
    (void)this->component.open(I2cInstance::I2c1);
    for (FwSizeType i = 0; i < sizeof(Stub_readResponseData); i++) {
        Stub_readResponseData[i] = static_cast<U8>(0x40 + i);
    }

    // Larger than Stub_readResponseData's 32-byte canned response.
    U8 backing[40];
    for (FwSizeType i = 0; i < sizeof(backing); i++) {
        backing[i] = 0xAA;  // sentinel: bytes past the 32-byte response must stay untouched
    }
    Fw::Buffer buffer(backing, sizeof(backing));
    const Drv::I2cStatus status = this->invoke_to_read(0, 0x50, buffer);
    ASSERT_EQ(status, Drv::I2cStatus::I2C_OK);

    // The full requested length is still reported to the caller...
    ASSERT_EQ(Stub_lastReadLen, 40u);
    // ...but only the first 32 bytes were actually supplied by the stub.
    for (FwSizeType i = 0; i < 32; i++) {
        ASSERT_EQ(backing[i], Stub_readResponseData[i]);
    }
    for (FwSizeType i = 32; i < 40; i++) {
        ASSERT_EQ(backing[i], 0xAA);
    }
}

void Stm32I2cDriverTester ::testWriteReadSuccess() {
    (void)this->component.open(I2cInstance::I2c1);
    Stub_readResponseData[0] = 0x99;

    U8 wdata[1] = {0x10};  // e.g. a sensor register address
    U8 rbacking[1] = {0};
    Fw::Buffer writeBuffer(wdata, sizeof(wdata));
    Fw::Buffer readBuffer(rbacking, sizeof(rbacking));
    const Drv::I2cStatus status = this->invoke_to_writeRead(0, 0x50, writeBuffer, readBuffer);
    ASSERT_EQ(status, Drv::I2cStatus::I2C_OK);
    ASSERT_EQ(Stub_lastDevAddress, 0x50);
    ASSERT_EQ(Stub_lastWriteLen, 1u);
    ASSERT_EQ(Stub_lastWriteData[0], 0x10);
    ASSERT_EQ(Stub_lastReadLen, 1u);
    ASSERT_EQ(rbacking[0], 0x99);
}

void Stm32I2cDriverTester ::testWriteReadTransmitFailureShortCircuits() {
    (void)this->component.open(I2cInstance::I2c1);
    Stub_hwMasterTransmitStatus = Drv::I2cStatus::I2C_WRITE_ERR;
    Stub_lastReadLen = 999;  // sentinel: must stay untouched if hwMasterReceive never runs

    U8 wdata[1] = {0x10};
    U8 rbacking[1] = {0};
    Fw::Buffer writeBuffer(wdata, sizeof(wdata));
    Fw::Buffer readBuffer(rbacking, sizeof(rbacking));
    const Drv::I2cStatus status = this->invoke_to_writeRead(0, 0x50, writeBuffer, readBuffer);
    ASSERT_EQ(status, Drv::I2cStatus::I2C_WRITE_ERR);
    ASSERT_EQ(Stub_lastReadLen, 999u);
}

void Stm32I2cDriverTester ::testWriteReadReceiveFailure() {
    (void)this->component.open(I2cInstance::I2c1);
    Stub_hwMasterReceiveStatus = Drv::I2cStatus::I2C_READ_ERR;

    U8 wdata[1] = {0x10};
    U8 rbacking[1] = {0};
    Fw::Buffer writeBuffer(wdata, sizeof(wdata));
    Fw::Buffer readBuffer(rbacking, sizeof(rbacking));
    const Drv::I2cStatus status = this->invoke_to_writeRead(0, 0x50, writeBuffer, readBuffer);
    ASSERT_EQ(status, Drv::I2cStatus::I2C_READ_ERR);
    ASSERT_EQ(Stub_lastWriteLen, 1u);  // the write half did run before the failing read
}

void Stm32I2cDriverTester ::testTwoInstancesDoNotInterfere() {
    (void)this->component.open(I2cInstance::I2c1, I2cBusSpeed::Fast, Stm32::TransferMode::POLLED);

    // A second, freestanding instance on a different bus, opened in DMA
    // mode -- m_transferMode (and the DMA-only registry entry it causes)
    // must be per-instance state. Before the multi-instance fix, the real
    // HAL boundary cached the resolved handle in file-static state shared
    // by every Stm32I2cDriver in the process -- opening this second
    // instance would have silently repointed the first instance's own
    // subsequent transactions.
    Stm32I2cDriver secondComponent("Stm32I2cDriverSecond");
    const Fw::Success secondOpenStatus = secondComponent.open(I2cInstance::I2c2, I2cBusSpeed::Fast, Stm32::TransferMode::DMA);
    ASSERT_EQ(secondOpenStatus, Fw::Success::SUCCESS);

    // The first instance must still be fully functional, unaffected by the
    // second instance's later open(), and still POLLED -- it blocks
    // through hwMasterTransmit() rather than the second instance's
    // DMA/async state.
    U8 backing[2] = {0xAA, 0xBB};
    Fw::Buffer buffer(backing, sizeof(backing));
    const Drv::I2cStatus status = this->invoke_to_write(0, 0x50, buffer);
    ASSERT_EQ(status, Drv::I2cStatus::I2C_OK);
}

}  // namespace Stm32
