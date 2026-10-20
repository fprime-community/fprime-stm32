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
      m_dmaErrorCode(0) {}

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

}  // namespace Stm32