// ======================================================================
// \title  Stm32SpiDriverTester.cpp
// \author ivanlara
// \brief  cpp file for Stm32SpiDriver component test harness implementation class
// ======================================================================

#include "Stm32SpiDriverTester.hpp"

#include <Fw/Time/TimeInterval.hpp>
#include <Os/Task.hpp>

// Stub_* globals defined in Stm32SpiDriverStub.cpp -- declared here so this
// Tester can inject HAL boundary success/failure and inspect exactly what
// Stm32SpiDriverCommon.cpp drove through it, instead of only exercising the
// stub's fixed defaults.
extern bool Stub_hwOpenSucceeds;
extern I32 Stub_hwTransmitReceiveStatus;
extern U8 Stub_rxResponseData[32];
extern I32 Stub_hwStartAsyncTransferStatus;
extern bool Stub_hwAbortAsyncTransferCalled;
extern bool Stub_hwFinishAsyncTransferCalled;
extern bool Stub_hwIsBusy;
extern Stm32::SpiInstance Stub_lastOpenedInstance;
extern Stm32::GpioPort Stub_lastCsPort;
extern U16 Stub_lastCsPin;
extern bool Stub_csActiveDuringTransfer;
extern U8 Stub_lastTxData[32];
extern FwSizeType Stub_lastTxLen;
extern Stm32::TransferMode Stub_lastOpenedMode;

namespace Stm32 {

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

Stm32SpiDriverTester ::Stm32SpiDriverTester()
    : Stm32SpiDriverGTestBase("Stm32SpiDriverTester", Stm32SpiDriverTester::MAX_HISTORY_SIZE),
      component("Stm32SpiDriver") {
    this->resetStubState();
    this->initComponents();
    this->connectPorts();
}

Stm32SpiDriverTester ::~Stm32SpiDriverTester() {
    this->component.deinit();
}

void Stm32SpiDriverTester ::resetStubState() {
    Stub_hwOpenSucceeds = true;
    Stub_hwTransmitReceiveStatus = 0;
    for (FwSizeType i = 0; i < sizeof(Stub_rxResponseData); i++) {
        Stub_rxResponseData[i] = 0;
    }
    Stub_hwStartAsyncTransferStatus = 0;
    Stub_hwAbortAsyncTransferCalled = false;
    Stub_hwFinishAsyncTransferCalled = false;
    Stub_hwIsBusy = false;
    Stub_lastOpenedInstance = SpiInstance::Spi5;
    Stub_lastCsPort = Stm32::GpioPort::A;
    Stub_lastCsPin = 0;
    Stub_csActiveDuringTransfer = false;
    for (FwSizeType i = 0; i < sizeof(Stub_lastTxData); i++) {
        Stub_lastTxData[i] = 0;
    }
    Stub_lastTxLen = 0;
    Stub_lastOpenedMode = Stm32::TransferMode::POLLED;
}

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

void Stm32SpiDriverTester ::testOpenSuccess() {
    const Fw::Success status = this->component.open(SpiInstance::Spi5, Stm32::GpioPort::F, 6);
    ASSERT_EQ(status, Fw::Success::SUCCESS);
    ASSERT_EQ(Stub_lastOpenedInstance, SpiInstance::Spi5);
    ASSERT_EQ(Stub_lastCsPort, Stm32::GpioPort::F);
    ASSERT_EQ(Stub_lastCsPin, 6u);
    ASSERT_EVENTS_PortOpened_SIZE(1);
    ASSERT_EVENTS_PortOpened(0, "Spi5");
}

void Stm32SpiDriverTester ::testOpenFailure() {
    Stub_hwOpenSucceeds = false;
    const Fw::Success status = this->component.open(SpiInstance::Spi5, Stm32::GpioPort::F, 6);
    ASSERT_EQ(status, Fw::Success::FAILURE);
    ASSERT_EVENTS_PortOpened_SIZE(0);
}

void Stm32SpiDriverTester ::testOpenDefaultsToPolled() {
    const Fw::Success status = this->component.open(SpiInstance::Spi5, Stm32::GpioPort::F, 6);
    ASSERT_EQ(status, Fw::Success::SUCCESS);
    ASSERT_EQ(Stub_lastOpenedMode, Stm32::TransferMode::POLLED);
}

void Stm32SpiDriverTester ::testOpenDma() {
    const Fw::Success status =
        this->component.open(SpiInstance::Spi5, Stm32::GpioPort::F, 6, 10, Stm32::TransferMode::DMA);
    ASSERT_EQ(status, Fw::Success::SUCCESS);
    ASSERT_EQ(Stub_lastOpenedMode, Stm32::TransferMode::DMA);
}

void Stm32SpiDriverTester ::testSpiWriteReadAsyncSuccess() {
    (void)this->component.open(SpiInstance::Spi5, Stm32::GpioPort::F, 6, 10, Stm32::TransferMode::DMA);
    Stub_rxResponseData[0] = 0x58;
    Stub_rxResponseData[1] = 0x11;

    U8 wdata[2] = {0xD0, 0x00};
    U8 rbacking[2] = {0};
    Fw::Buffer writeBuffer(wdata, sizeof(wdata));
    Fw::Buffer readBuffer(rbacking, sizeof(rbacking));
    this->invoke_to_spiWriteReadAsync(0, writeBuffer, readBuffer);

    // hwStartAsyncTransfer() ran synchronously within the request (the stub
    // has no real ISR), but the completion must not be delivered until
    // poll() runs -- this is the "never from inside the request call"
    // contract spiWriteReadAsync shares with Samd21::SpiDriver.
    ASSERT_FROM_PORT_HISTORY_SIZE(0);
    ASSERT_TRUE(Stub_csActiveDuringTransfer);
    ASSERT_EQ(Stub_lastTxLen, 2u);
    ASSERT_EQ(Stub_lastTxData[0], 0xD0);

    // signalDmaComplete() (the target of the real ISR trampoline) is what
    // frees poll() to deliver the completion.
    this->component.signalDmaComplete();
    this->component.poll();
    ASSERT_from_spiWriteReadAsyncComplete_SIZE(1);
    ASSERT_from_spiWriteReadAsyncComplete(0, writeBuffer, readBuffer, Drv::SpiStatus::SPI_OK);
    ASSERT_EQ(rbacking[0], 0x58);
    ASSERT_EQ(rbacking[1], 0x11);
    ASSERT_TRUE(Stub_hwFinishAsyncTransferCalled);
}

void Stm32SpiDriverTester ::testSpiWriteReadAsyncFailure() {
    (void)this->component.open(SpiInstance::Spi5, Stm32::GpioPort::F, 6, 10, Stm32::TransferMode::DMA);
    Stub_hwStartAsyncTransferStatus = 1;  // any non-zero (non-HAL_OK) status

    U8 wdata[1] = {0xD0};
    U8 rbacking[1] = {0};
    Fw::Buffer writeBuffer(wdata, sizeof(wdata));
    Fw::Buffer readBuffer(rbacking, sizeof(rbacking));
    this->invoke_to_spiWriteReadAsync(0, writeBuffer, readBuffer);

    // A HAL failure to even START the transfer is reported immediately
    // (there is nothing for poll() to observe later).
    ASSERT_from_spiWriteReadAsyncComplete_SIZE(1);
    ASSERT_from_spiWriteReadAsyncComplete(0, writeBuffer, readBuffer, Drv::SpiStatus::SPI_WRITE_ERR);
    ASSERT_EVENTS_HalError_SIZE(1);
    ASSERT_EVENTS_HalError(0, "TransmitReceive_DMA", 1);
}

void Stm32SpiDriverTester ::testSpiWriteReadAsyncBusyRejectsSecondRequest() {
    (void)this->component.open(SpiInstance::Spi5, Stm32::GpioPort::F, 6, 10, Stm32::TransferMode::DMA);

    U8 wdata1[1] = {0xD0};
    U8 rbacking1[1] = {0};
    Fw::Buffer writeBuffer1(wdata1, sizeof(wdata1));
    Fw::Buffer readBuffer1(rbacking1, sizeof(rbacking1));
    this->invoke_to_spiWriteReadAsync(0, writeBuffer1, readBuffer1);
    ASSERT_FROM_PORT_HISTORY_SIZE(0);  // first request still in flight

    U8 wdata2[1] = {0xD1};
    U8 rbacking2[1] = {0};
    Fw::Buffer writeBuffer2(wdata2, sizeof(wdata2));
    Fw::Buffer readBuffer2(rbacking2, sizeof(rbacking2));
    this->invoke_to_spiWriteReadAsync(0, writeBuffer2, readBuffer2);

    // The second request is refused synchronously -- never from an ISR,
    // never waiting on the first transfer's completion.
    ASSERT_from_spiWriteReadAsyncComplete_SIZE(1);
    ASSERT_from_spiWriteReadAsyncComplete(0, writeBuffer2, readBuffer2, Drv::SpiStatus::SPI_OTHER_ERR);

    // The first request's own completion still arrives normally.
    this->component.signalDmaComplete();
    this->component.poll();
    ASSERT_from_spiWriteReadAsyncComplete_SIZE(2);
    ASSERT_from_spiWriteReadAsyncComplete(1, writeBuffer1, readBuffer1, Drv::SpiStatus::SPI_OK);
}

void Stm32SpiDriverTester ::testSpiWriteReadAsyncTimeout() {
    (void)this->component.open(SpiInstance::Spi5, Stm32::GpioPort::F, 6, 10, Stm32::TransferMode::DMA);

    U8 wdata[1] = {0xD0};
    U8 rbacking[1] = {0};
    Fw::Buffer writeBuffer(wdata, sizeof(wdata));
    Fw::Buffer readBuffer(rbacking, sizeof(rbacking));
    this->invoke_to_spiWriteReadAsync(0, writeBuffer, readBuffer);

    this->component.poll();  // kicks off the transfer; m_dmaBusy is already true
    ASSERT_FROM_PORT_HISTORY_SIZE(0);

    // The stub never calls signalDmaComplete() on its own, so the transfer
    // is still "in flight" after the open()-configured 10ms m_timeoutMs
    // elapses; wait comfortably longer than that, matching
    // Stm32UartDriverTester::testTxWatchdogTimeout()'s pattern.
    (void)Os::Task::delay(Fw::TimeInterval(0, 20000));  // 20 ms
    this->component.poll();

    ASSERT_from_spiWriteReadAsyncComplete_SIZE(1);
    ASSERT_from_spiWriteReadAsyncComplete(0, writeBuffer, readBuffer, Drv::SpiStatus::SPI_OTHER_ERR);
    ASSERT_TRUE(Stub_hwAbortAsyncTransferCalled);
    ASSERT_EVENTS_HalError_SIZE(1);
}

void Stm32SpiDriverTester ::testSpiWriteReadAsyncBeforeOpen() {
    U8 wdata[1] = {0xD0};
    U8 rbacking[1] = {0};
    Fw::Buffer writeBuffer(wdata, sizeof(wdata));
    Fw::Buffer readBuffer(rbacking, sizeof(rbacking));
    this->invoke_to_spiWriteReadAsync(0, writeBuffer, readBuffer);
    ASSERT_from_spiWriteReadAsyncComplete_SIZE(1);
    ASSERT_from_spiWriteReadAsyncComplete(0, writeBuffer, readBuffer, Drv::SpiStatus::SPI_OPEN_ERR);
}

void Stm32SpiDriverTester ::testSpiWriteReadAsyncRejectedWhenNotDma() {
    (void)this->component.open(SpiInstance::Spi5, Stm32::GpioPort::F, 6, 10, Stm32::TransferMode::POLLED);

    U8 wdata[1] = {0xD0};
    U8 rbacking[1] = {0};
    Fw::Buffer writeBuffer(wdata, sizeof(wdata));
    Fw::Buffer readBuffer(rbacking, sizeof(rbacking));
    this->invoke_to_spiWriteReadAsync(0, writeBuffer, readBuffer);
    ASSERT_from_spiWriteReadAsyncComplete_SIZE(1);
    ASSERT_from_spiWriteReadAsyncComplete(0, writeBuffer, readBuffer, Drv::SpiStatus::SPI_OTHER_ERR);
}

void Stm32SpiDriverTester ::testPollNoOpWhenIdle() {
    (void)this->component.open(SpiInstance::Spi5, Stm32::GpioPort::F, 6, 10, Stm32::TransferMode::POLLED);
    // No spiWriteReadAsync request was ever issued on this POLLED-mode
    // instance -- poll() must be a harmless no-op, confirming it is safe
    // to call unconditionally regardless of an instance's mode.
    this->component.poll();
    ASSERT_FROM_PORT_HISTORY_SIZE(0);
}

void Stm32SpiDriverTester ::testReopenSwitchesMode() {
    (void)this->component.open(SpiInstance::Spi5, Stm32::GpioPort::F, 6, 10, Stm32::TransferMode::POLLED);
    Stub_rxResponseData[0] = 0x11;
    U8 wdata1[1] = {0xD0};
    U8 rbacking1[1] = {0};
    Fw::Buffer writeBuffer1(wdata1, sizeof(wdata1));
    Fw::Buffer readBuffer1(rbacking1, sizeof(rbacking1));
    ASSERT_EQ(this->invoke_to_SpiWriteRead(0, writeBuffer1, readBuffer1), Drv::SpiStatus::SPI_OK);
    ASSERT_EQ(rbacking1[0], 0x11);

    // Re-open the SAME component instance in DMA mode and inject a HAL
    // start failure -- a driver that kept the old POLLED mode cached
    // somewhere other than m_transferMode would route this request through
    // the blocking SpiWriteRead path instead of spiWriteReadAsync.
    (void)this->component.open(SpiInstance::Spi5, Stm32::GpioPort::F, 6, 10, Stm32::TransferMode::DMA);
    Stub_hwStartAsyncTransferStatus = 1;
    U8 wdata2[1] = {0xD1};
    U8 rbacking2[1] = {0};
    Fw::Buffer writeBuffer2(wdata2, sizeof(wdata2));
    Fw::Buffer readBuffer2(rbacking2, sizeof(rbacking2));
    this->invoke_to_spiWriteReadAsync(0, writeBuffer2, readBuffer2);
    ASSERT_from_spiWriteReadAsyncComplete_SIZE(1);
    ASSERT_from_spiWriteReadAsyncComplete(0, writeBuffer2, readBuffer2, Drv::SpiStatus::SPI_WRITE_ERR);
}

void Stm32SpiDriverTester ::testSpiWriteReadBeforeOpen() {
    U8 wbacking[4] = {1, 2, 3, 4};
    U8 rbacking[4] = {0};
    Fw::Buffer writeBuffer(wbacking, sizeof(wbacking));
    Fw::Buffer readBuffer(rbacking, sizeof(rbacking));
    ASSERT_EQ(this->invoke_to_SpiWriteRead(0, writeBuffer, readBuffer), Drv::SpiStatus::SPI_OPEN_ERR);
}

void Stm32SpiDriverTester ::testSpiReadWriteBeforeOpen() {
    U8 wbacking[4] = {1, 2, 3, 4};
    U8 rbacking[4] = {0};
    Fw::Buffer writeBuffer(wbacking, sizeof(wbacking));
    Fw::Buffer readBuffer(rbacking, sizeof(rbacking));
    // SpiReadWrite has no return value: absence of a crash and an
    // unaffected read buffer confirm the OPEN_ERR short-circuit ran.
    this->invoke_to_SpiReadWrite(0, writeBuffer, readBuffer);
    ASSERT_EQ(rbacking[0], 0u);
}

void Stm32SpiDriverTester ::testSpiWriteReadSuccess() {
    (void)this->component.open(SpiInstance::Spi5, Stm32::GpioPort::F, 6);
    Stub_rxResponseData[0] = 0x58;  // BMP280 chip-id-register style response
    Stub_rxResponseData[1] = 0x11;

    U8 wdata[2] = {0xD0, 0x00};
    U8 rbacking[2] = {0};
    Fw::Buffer writeBuffer(wdata, sizeof(wdata));
    Fw::Buffer readBuffer(rbacking, sizeof(rbacking));
    const Drv::SpiStatus status = this->invoke_to_SpiWriteRead(0, writeBuffer, readBuffer);
    ASSERT_EQ(status, Drv::SpiStatus::SPI_OK);

    // Chip-select must have been asserted for the exact duration of the
    // transfer -- a driver that forgets to call hwSetCs(true) before
    // transferring would leave this false.
    ASSERT_TRUE(Stub_csActiveDuringTransfer);
    ASSERT_EQ(Stub_lastTxLen, 2u);
    ASSERT_EQ(Stub_lastTxData[0], 0xD0);
    ASSERT_EQ(Stub_lastTxData[1], 0x00);
    ASSERT_EQ(rbacking[0], 0x58);
    ASSERT_EQ(rbacking[1], 0x11);
}

void Stm32SpiDriverTester ::testSpiWriteReadFailure() {
    (void)this->component.open(SpiInstance::Spi5, Stm32::GpioPort::F, 6);
    Stub_hwTransmitReceiveStatus = 1;  // any non-zero (non-HAL_OK) status

    U8 wdata[1] = {0xD0};
    U8 rbacking[1] = {0};
    Fw::Buffer writeBuffer(wdata, sizeof(wdata));
    Fw::Buffer readBuffer(rbacking, sizeof(rbacking));
    const Drv::SpiStatus status = this->invoke_to_SpiWriteRead(0, writeBuffer, readBuffer);
    ASSERT_EQ(status, Drv::SpiStatus::SPI_WRITE_ERR);
    ASSERT_EVENTS_HalError_SIZE(1);
    ASSERT_EVENTS_HalError(0, "TransmitReceive", 1);
}

void Stm32SpiDriverTester ::testSpiReadWriteSuccess() {
    (void)this->component.open(SpiInstance::Spi5, Stm32::GpioPort::F, 6);
    Stub_rxResponseData[0] = 0x42;

    U8 wdata[1] = {0xD0};
    U8 rbacking[1] = {0};
    Fw::Buffer writeBuffer(wdata, sizeof(wdata));
    Fw::Buffer readBuffer(rbacking, sizeof(rbacking));
    this->invoke_to_SpiReadWrite(0, writeBuffer, readBuffer);

    ASSERT_TRUE(Stub_csActiveDuringTransfer);
    ASSERT_EQ(rbacking[0], 0x42);
}

void Stm32SpiDriverTester ::testSpiWriteReadLargeBufferCapped() {
    (void)this->component.open(SpiInstance::Spi5, Stm32::GpioPort::F, 6);

    // Larger than Stub_lastTxData's 32-byte capture buffer.
    U8 wdata[40];
    U8 rbacking[40] = {0};
    for (FwSizeType i = 0; i < sizeof(wdata); i++) {
        wdata[i] = static_cast<U8>(i);
    }
    Fw::Buffer writeBuffer(wdata, sizeof(wdata));
    Fw::Buffer readBuffer(rbacking, sizeof(rbacking));
    const Drv::SpiStatus status = this->invoke_to_SpiWriteRead(0, writeBuffer, readBuffer);
    ASSERT_EQ(status, Drv::SpiStatus::SPI_OK);

    // The stub must truncate its capture rather than overrun its own
    // 32-byte buffer; the real HAL call underneath would see the full 40.
    ASSERT_EQ(Stub_lastTxLen, 40u);
    for (FwSizeType i = 0; i < 32; i++) {
        ASSERT_EQ(Stub_lastTxData[i], wdata[i]);
    }
}

}  // namespace Stm32
