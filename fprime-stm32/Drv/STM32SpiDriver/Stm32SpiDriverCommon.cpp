// ======================================================================
// \title  Stm32SpiDriverCommon.cpp
// \author ivanlara
// \brief  Hardware-independent port handler logic for the STM32H7 SPI
//         blocking master driver: shared by the real (stm32h7) and stubbed
//         (host UT) builds. Contains no HAL/CMSIS dependency -- every
//         register touch is behind hwSetCs()/hwTransmitReceive() (declared
//         in Stm32SpiDriver.hpp) or open() itself, implemented in
//         Stm32SpiDriver.cpp (real) / Stm32SpiDriverStub.cpp (host).
// ======================================================================

#include <fprime-stm32/Drv/STM32SpiDriver/Stm32SpiDriver.hpp>
#include <Fw/Types/Assert.hpp>

namespace Stm32 {

// ----------------------------------------------------------------------
// Construction, initialization, and destruction
// ----------------------------------------------------------------------

Stm32SpiDriver ::Stm32SpiDriver(const char* const compName)
    : Stm32SpiDriverComponentBase(compName),
      m_instance(SpiInstance::Spi5),
      m_csPort(Stm32::GpioPort::A),
      m_csPin(0),
      m_timeoutMs(10),
      m_transferMode(TransferMode::POLLED),
      m_opened(false),
      m_dmaBusy(false),
      m_dmaErrorCode(0),
      m_asyncPending(false),
      m_asyncPortNum(0),
      m_asyncWriteBuffer(),
      m_asyncReadBuffer() {}

Stm32SpiDriver ::~Stm32SpiDriver() {}

void Stm32SpiDriver ::signalDmaComplete() {
    this->m_dmaErrorCode = 0;
    this->m_dmaBusy = false;
}

void Stm32SpiDriver ::signalDmaError(U32 errorCode) {
    this->m_dmaErrorCode = errorCode;
    this->m_dmaBusy = false;
}

// ----------------------------------------------------------------------
// Handler implementations for user-defined typed input ports
// ----------------------------------------------------------------------

// Chip-select handling and buffer checks stay mode-agnostic here --
// hwTransmitReceive() (Stm32SpiDriver.cpp real / Stm32SpiDriverStub.cpp
// host) is the only place that branches on m_transferMode, so this handler
// behaves identically whether the instance was opened POLLED or DMA (see
// docs/sdd.md).
Drv::SpiStatus Stm32SpiDriver ::SpiWriteRead_handler(FwIndexType portNum, Fw::Buffer& writeBuffer, Fw::Buffer& readBuffer) {
    if (!this->m_opened) {
        return Drv::SpiStatus::SPI_OPEN_ERR;
    }
    FW_ASSERT(writeBuffer.getData() != nullptr);
    FW_ASSERT(readBuffer.getData() != nullptr);
    FW_ASSERT(writeBuffer.getSize() == readBuffer.getSize(), static_cast<FwAssertArgType>(writeBuffer.getSize()),
              static_cast<FwAssertArgType>(readBuffer.getSize()));

    this->hwSetCs(true);
    const I32 status = this->hwTransmitReceive(writeBuffer.getData(), readBuffer.getData(), writeBuffer.getSize());
    this->hwSetCs(false);

    if (status != 0) {  // 0 == HAL_OK
        Fw::LogStringArg _op("TransmitReceive");
        this->log_WARNING_HI_HalError(_op, status);
        return Drv::SpiStatus::SPI_WRITE_ERR;
    }
    return Drv::SpiStatus::SPI_OK;
}

void Stm32SpiDriver ::SpiReadWrite_handler(FwIndexType portNum, Fw::Buffer& writeBuffer, Fw::Buffer& readBuffer) {
    this->lock();
    (void)this->SpiWriteRead_handler(portNum, writeBuffer, readBuffer);
    this->unLock();
}

// ----------------------------------------------------------------------
// Asynchronous (DMA-mode) SPI contract: request starts the transfer and
// returns immediately; poll() drains the ISR-latched outcome and delivers
// it via spiWriteReadAsyncComplete_out(), never from this handler and never
// from an ISR -- same completion-delivery contract Samd21::SpiDriver
// documents for its own SpiWriteRead/SpiWriteReadComplete pair.
// ----------------------------------------------------------------------

void Stm32SpiDriver ::spiWriteReadAsync_handler(FwIndexType portNum, Fw::Buffer& writeBuffer, Fw::Buffer& readBuffer) {
    if (!this->m_opened) {
        this->spiWriteReadAsyncComplete_out(portNum, writeBuffer, readBuffer, Drv::SpiStatus::SPI_OPEN_ERR);
        return;
    }
    // A POLLED-mode instance never registers in s_spiRegistry for ISR
    // callback routing (see open()), so starting a DMA transfer on one
    // would hang until poll()'s own watchdog times it out -- reject
    // immediately instead (same hardening as Stm32I2cDriver's async ports).
    if (this->m_transferMode != TransferMode::DMA) {
        this->spiWriteReadAsyncComplete_out(portNum, writeBuffer, readBuffer, Drv::SpiStatus::SPI_OTHER_ERR);
        return;
    }
    FW_ASSERT(writeBuffer.getData() != nullptr);
    FW_ASSERT(readBuffer.getData() != nullptr);
    FW_ASSERT(writeBuffer.getSize() == readBuffer.getSize(), static_cast<FwAssertArgType>(writeBuffer.getSize()),
              static_cast<FwAssertArgType>(readBuffer.getSize()));

    if (this->m_asyncPending) {
        this->spiWriteReadAsyncComplete_out(portNum, writeBuffer, readBuffer, Drv::SpiStatus::SPI_OTHER_ERR);
        return;
    }

    this->m_asyncPortNum = portNum;
    this->m_asyncWriteBuffer = writeBuffer;
    this->m_asyncReadBuffer = readBuffer;
    (void)this->m_asyncDmaStart.now();

    this->hwSetCs(true);
    this->m_asyncPending = true;
    this->m_dmaBusy = true;
    this->m_dmaErrorCode = 0;
    const I32 status =
        this->hwStartAsyncTransfer(writeBuffer.getData(), readBuffer.getData(), writeBuffer.getSize());
    if (status != 0) {  // 0 == HAL_OK
        this->hwSetCs(false);
        this->m_asyncPending = false;
        this->m_dmaBusy = false;
        Fw::LogStringArg _op("TransmitReceive_DMA");
        this->log_WARNING_HI_HalError(_op, status);
        this->spiWriteReadAsyncComplete_out(portNum, writeBuffer, readBuffer, Drv::SpiStatus::SPI_WRITE_ERR);
    }
    // On success, hwStartAsyncTransfer started the transfer; poll() delivers
    // the completion once signalDmaComplete()/signalDmaError() latches it.
}

void Stm32SpiDriver ::poll() {
    // m_asyncPending spans from request-accepted until this method has
    // delivered the completion -- distinct from m_dmaBusy, which only
    // tracks whether the ISR (signalDmaComplete()/signalDmaError()) has
    // latched an outcome yet. Without that split, a second poll() call
    // after delivery would see the transfer "done" and re-deliver the same
    // (stale) buffers.
    if (!this->m_asyncPending) {
        return;
    }

    Drv::SpiStatus status;
    if (!this->m_dmaBusy) {
        status = (this->m_dmaErrorCode == 0) ? Drv::SpiStatus::SPI_OK : Drv::SpiStatus::SPI_OTHER_ERR;
    } else {
        Os::RawTime now;
        (void)now.now();
        U32 elapsedUs = 0;
        (void)now.getDiffUsec(this->m_asyncDmaStart, elapsedUs);
        if (elapsedUs < (this->m_timeoutMs * 1000U)) {
            return;  // still in flight, not yet past this transfer's watchdog
        }
        this->hwAbortAsyncTransfer();
        this->m_dmaBusy = false;
        this->log_WARNING_HI_HalError(Fw::LogStringArg("TransmitReceive_DMA"), 3 /* HAL_TIMEOUT */);
        status = Drv::SpiStatus::SPI_OTHER_ERR;
    }

    this->m_asyncPending = false;
    this->hwSetCs(false);
    Fw::Buffer writeBuffer = this->m_asyncWriteBuffer;
    Fw::Buffer readBuffer = this->m_asyncReadBuffer;
    if (status == Drv::SpiStatus::SPI_OK) {
        this->hwFinishAsyncTransfer(readBuffer.getData(), readBuffer.getSize());
    }
    this->spiWriteReadAsyncComplete_out(this->m_asyncPortNum, writeBuffer, readBuffer, status);
}

}  // namespace Stm32