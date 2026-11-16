// ======================================================================
// \title  Stm32SpiDriverStub.cpp
// \brief  HAL boundary stand-in for host-native unit tests. No SPI5
//         hardware exists on the build host, so every call here operates
//         on injectable/observable stub state instead of real registers --
//         no HAL/CMSIS include, no register access. A unit test drives
//         success/failure via the extern stub state below instead of
//         relying on real HAL_SPI_* return codes.
// ======================================================================

#include <fprime-stm32/Drv/STM32SpiDriver/Stm32SpiDriver.hpp>

// Injectable stub state for unit tests
extern bool Stub_hwOpenSucceeds = true;               // simulates MX_SPIn_Init()
extern I32 Stub_hwTransmitReceiveStatus = 0;          // HAL_StatusTypeDef hwTransmitReceive() reports (0 == HAL_OK)
extern U8 Stub_rxResponseData[32] = {0};          // bytes hwTransmitReceive() copies into the caller's read buffer
extern I32 Stub_hwStartAsyncTransferStatus = 0;       // HAL_StatusTypeDef hwStartAsyncTransfer() reports (0 == HAL_OK)
extern bool Stub_hwAbortAsyncTransferCalled = false;  // observable: true after hwAbortAsyncTransfer() runs
extern bool Stub_hwFinishAsyncTransferCalled = false; // observable: true after hwFinishAsyncTransfer() runs
extern bool Stub_hwIsBusy = false;                    // simulates a sibling instance holding the shared peripheral busy

// Observable stub state for unit tests
extern Stm32::SpiInstance Stub_lastOpenedInstance = Stm32::SpiInstance::Spi5;  // instance most recently passed to open()
extern Stm32::GpioPort Stub_lastCsPort = Stm32::GpioPort::A;   // csPort most recently passed to open()
extern U16 Stub_lastCsPin = 0;                                 // csPin most recently passed to open()
extern Stm32::TransferMode Stub_lastOpenedMode = Stm32::TransferMode::POLLED;  // mode most recently passed to open()
extern bool Stub_csActiveDuringTransfer = false;  // true iff hwSetCs(true) was in effect when hwTransmitReceive() ran
extern U8 Stub_lastTxData[32] = {0};
extern FwSizeType Stub_lastTxLen = 0;

namespace {
//! Tracks whether the stub's most recent hwSetCs() call selected the
//! device, so hwTransmitReceive() can record whether it ran while CS was
//! actually active -- lets a test catch a driver that forgets to assert CS
//! before transferring.
bool s_csActive = false;
}  // namespace

namespace Stm32 {

Fw::Success Stm32SpiDriver ::open(SpiInstance instance,
                                   Stm32::GpioPort csPort,
                                   U16 csPin,
                                   U32 timeoutMs,
                                   TransferMode mode) {
    Stub_lastOpenedInstance = instance;
    Stub_lastCsPort = csPort;
    Stub_lastCsPin = csPin;
    Stub_lastOpenedMode = mode;
    if (!Stub_hwOpenSucceeds) {
        return Fw::Success::FAILURE;
    }
    this->m_instance = instance;
    this->m_csPort = csPort;
    this->m_csPin = csPin;
    this->m_timeoutMs = timeoutMs;
    this->m_transferMode = mode;
    this->m_opened = true;

    Fw::LogStringArg _instanceArg(instance == Stm32::SpiInstance::Spi1 ? "Spi1" :
                                  instance == Stm32::SpiInstance::Spi2 ? "Spi2" :
                                  instance == Stm32::SpiInstance::Spi3 ? "Spi3" :
                                  instance == Stm32::SpiInstance::Spi4 ? "Spi4" :
                                  instance == Stm32::SpiInstance::Spi5 ? "Spi5" :
                                  instance == Stm32::SpiInstance::Spi6 ? "Spi6" : "Unknown");
    this->log_ACTIVITY_HI_PortOpened(_instanceArg);
    return Fw::Success::SUCCESS;
}

void Stm32SpiDriver ::hwSetCs(bool active) {
    s_csActive = active;
}

bool Stm32SpiDriver ::hwIsBusy() {
    return Stub_hwIsBusy;
}

I32 Stm32SpiDriver ::hwTransmitReceive(const U8* txData, U8* rxData, FwSizeType size) {
    Stub_csActiveDuringTransfer = s_csActive;

    const FwSizeType captured = (size < sizeof(Stub_lastTxData)) ? size : sizeof(Stub_lastTxData);
    for (FwSizeType i = 0; i < captured; i++) {
        Stub_lastTxData[i] = txData[i];
    }
    Stub_lastTxLen = size;

    const FwSizeType toCopy = (size < sizeof(Stub_rxResponseData)) ? size : sizeof(Stub_rxResponseData);
    for (FwSizeType i = 0; i < toCopy; i++) {
        rxData[i] = Stub_rxResponseData[i];
    }

    return Stub_hwTransmitReceiveStatus;
}

I32 Stm32SpiDriver ::hwStartAsyncTransfer(const U8* txData, U8* rxData, FwSizeType size) {
    Stub_csActiveDuringTransfer = s_csActive;

    const FwSizeType captured = (size < sizeof(Stub_lastTxData)) ? size : sizeof(Stub_lastTxData);
    for (FwSizeType i = 0; i < captured; i++) {
        Stub_lastTxData[i] = txData[i];
    }
    Stub_lastTxLen = size;

    if (Stub_hwStartAsyncTransferStatus == 0) {
        const FwSizeType toCopy = (size < sizeof(Stub_rxResponseData)) ? size : sizeof(Stub_rxResponseData);
        for (FwSizeType i = 0; i < toCopy; i++) {
            rxData[i] = Stub_rxResponseData[i];
        }
    }

    return Stub_hwStartAsyncTransferStatus;
}

void Stm32SpiDriver ::hwAbortAsyncTransfer() {
    Stub_hwAbortAsyncTransferCalled = true;
}

void Stm32SpiDriver ::hwFinishAsyncTransfer(U8* rxData, FwSizeType size) {
    (void)rxData;
    (void)size;
    Stub_hwFinishAsyncTransferCalled = true;
}

}  // namespace Stm32
