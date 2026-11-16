// ======================================================================
// \title  Stm32I2cDriverCommon.cpp
// \author ivanlara
// \brief  Hardware-independent port handler logic for the STM32H7 I2C
//         blocking master driver: shared by the real (stm32h7) and stubbed
//         (host UT) builds. Contains no HAL/CMSIS dependency -- every
//         register touch is behind hwMasterTransmit()/hwMasterReceive()
//         (declared in Stm32I2cDriver.hpp) or open() itself, implemented in
//         Stm32I2cDriver.cpp (real) / Stm32I2cDriverStub.cpp (host).
// ======================================================================

#include <fprime-stm32/Drv/STM32I2cDriver/Stm32I2cDriver.hpp>
#include <Fw/Types/Assert.hpp>
#include <Os/RawTime.hpp>

namespace Stm32 {

//! Wait for a sibling Stm32I2cDriver instance on the same physical peripheral
void Stm32I2cDriver ::waitWhileBusy() {
    Os::RawTime waitStart;
    (void)waitStart.now();
    const U32 timeoutUs = TRANSACTION_TIMEOUT_MS * 1000U;
    while (this->hwIsBusy()) {
        Os::RawTime now;
        (void)now.now();
        U32 elapsedUs = 0;
        (void)now.getDiffUsec(waitStart, elapsedUs);
        if (elapsedUs >= timeoutUs) {
            break;
        }
    }
}

// ----------------------------------------------------------------------
// Construction, initialization, and destruction
// ----------------------------------------------------------------------

Stm32I2cDriver ::Stm32I2cDriver(const char* const compName)
    : Stm32I2cDriverComponentBase(compName),
      m_instance(I2cInstance::I2c1),
      m_transferMode(TransferMode::POLLED),
      m_opened(false),
      m_dmaBusy(false),
      m_dmaErrorCode(0),
      m_asyncPending(false),
      m_asyncOp(AsyncOp::IDLE),
      m_asyncPortNum(0),
      m_asyncDevAddress(0),
      m_asyncWriteBuffer(),
      m_asyncReadBuffer() {}

Stm32I2cDriver ::~Stm32I2cDriver() {}

void Stm32I2cDriver ::signalDmaComplete() {
    this->m_dmaErrorCode = 0;
    this->m_dmaBusy = false;
}

void Stm32I2cDriver ::signalDmaError(U32 errorCode) {
    this->m_dmaErrorCode = errorCode;
    this->m_dmaBusy = false;
}

// ----------------------------------------------------------------------
// Handler implementations for user-defined typed input ports
// ----------------------------------------------------------------------

Drv::I2cStatus Stm32I2cDriver ::write_handler(const FwIndexType portNum, U32 addr, Fw::Buffer& serBuffer) {
    if (!this->m_opened) {
        return Drv::I2cStatus::I2C_OPEN_ERR;
    }
    FW_ASSERT(serBuffer.getData() != nullptr);
    FW_ASSERT_NO_OVERFLOW(addr, U16);
    FW_ASSERT_NO_OVERFLOW(serBuffer.getSize(), U16);

    this->waitWhileBusy();
    return this->hwMasterTransmit(static_cast<U16>(addr), serBuffer.getData(), static_cast<U16>(serBuffer.getSize()));
}

Drv::I2cStatus Stm32I2cDriver ::read_handler(const FwIndexType portNum, U32 addr, Fw::Buffer& serBuffer) {
    if (!this->m_opened) {
        return Drv::I2cStatus::I2C_OPEN_ERR;
    }
    FW_ASSERT(serBuffer.getData() != nullptr);
    FW_ASSERT_NO_OVERFLOW(addr, U16);
    FW_ASSERT_NO_OVERFLOW(serBuffer.getSize(), U16);

    this->waitWhileBusy();
    return this->hwMasterReceive(static_cast<U16>(addr), serBuffer.getData(), static_cast<U16>(serBuffer.getSize()));
}

// I2C1 is configured in blocking/polled mode only (no NVIC event/error IRQ
// enabled for I2C1 -- see HAL_I2C_MspInit() in src/i2c.c), so there is no
// sequential-transfer (HAL_I2C_Master_Seq_*_IT with I2C_FIRST_FRAME/
// I2C_LAST_FRAME) path available to hold the bus between the write and the
// read with a single electrically-held repeated START.
Drv::I2cStatus Stm32I2cDriver ::writeRead_handler(const FwIndexType portNum,
                                                    U32 addr,
                                                    Fw::Buffer& writeBuffer,
                                                    Fw::Buffer& readBuffer) {
    if (!this->m_opened) {
        return Drv::I2cStatus::I2C_OPEN_ERR;
    }
    FW_ASSERT(writeBuffer.getData() != nullptr);
    FW_ASSERT(readBuffer.getData() != nullptr);
    FW_ASSERT_NO_OVERFLOW(addr, U16);
    FW_ASSERT_NO_OVERFLOW(writeBuffer.getSize(), U16);
    FW_ASSERT_NO_OVERFLOW(readBuffer.getSize(), U16);

    const U16 devAddress = static_cast<U16>(addr);

    this->waitWhileBusy();
    const Drv::I2cStatus writeStatus =
        this->hwMasterTransmit(devAddress, writeBuffer.getData(), static_cast<U16>(writeBuffer.getSize()));
    if (Drv::I2cStatus::I2C_OK != writeStatus) {
        return writeStatus;
    }

    return this->hwMasterReceive(devAddress, readBuffer.getData(), static_cast<U16>(readBuffer.getSize()));
}

// ----------------------------------------------------------------------
// Asynchronous (DMA-mode) I2C contract: a request starts the transfer and
// returns immediately; poll() drains the ISR-latched outcome and delivers
// it via the matching *AsyncComplete port, never from an ISR -- see
// Stm32I2cDriver.hpp's own doc comments on writeAsync_handler()/poll().
// ----------------------------------------------------------------------

void Stm32I2cDriver ::writeAsync_handler(const FwIndexType portNum, U32 addr, Fw::Buffer& buffer) {
    if (!this->m_opened) {
        this->writeAsyncComplete_out(portNum, buffer, Drv::I2cStatus::I2C_OPEN_ERR);
        return;
    }
    // A POLLED-mode instance never registers for ISR callback routing (see
    // open()), so starting a DMA transaction on one would hang until
    // poll()'s own watchdog times it out -- reject immediately instead.
    if ((this->m_transferMode != TransferMode::DMA) || this->m_asyncPending) {
        this->writeAsyncComplete_out(portNum, buffer, Drv::I2cStatus::I2C_OTHER_ERR);
        return;
    }
    // Reject immediately if the hardware is busy, rather than spinning or waiting.
    if (this->hwIsBusy()) {
        this->writeAsyncComplete_out(portNum, buffer, Drv::I2cStatus::I2C_OTHER_ERR);
        return;
    }
    FW_ASSERT(buffer.getData() != nullptr);
    FW_ASSERT_NO_OVERFLOW(addr, U16);
    FW_ASSERT_NO_OVERFLOW(buffer.getSize(), U16);

    this->m_asyncPortNum = portNum;
    this->m_asyncDevAddress = static_cast<U16>(addr);
    this->m_asyncWriteBuffer = buffer;
    (void)this->m_asyncDmaStart.now();

    this->m_asyncPending = true;
    this->m_asyncOp = AsyncOp::WRITE;
    this->m_dmaBusy = true;
    this->m_dmaErrorCode = 0;
    const I32 status =
        this->hwStartAsyncTransmit(this->m_asyncDevAddress, buffer.getData(), static_cast<U16>(buffer.getSize()));
    if (status != 0) {  // 0 == HAL_OK
        this->m_asyncPending = false;
        this->m_dmaBusy = false;
        Fw::LogStringArg _op("Master_Transmit_DMA");
        this->log_WARNING_HI_HalError(_op, addr, status);
        this->writeAsyncComplete_out(portNum, buffer, Drv::I2cStatus::I2C_WRITE_ERR);
    }
    // On success, hwStartAsyncTransmit started the transaction; poll()
    // delivers the completion once signalDmaComplete()/signalDmaError()
    // latches it.
}

void Stm32I2cDriver ::readAsync_handler(const FwIndexType portNum, U32 addr, Fw::Buffer& buffer) {
    if (!this->m_opened) {
        this->readAsyncComplete_out(portNum, buffer, Drv::I2cStatus::I2C_OPEN_ERR);
        return;
    }
    if ((this->m_transferMode != TransferMode::DMA) || this->m_asyncPending) {
        this->readAsyncComplete_out(portNum, buffer, Drv::I2cStatus::I2C_OTHER_ERR);
        return;
    }
    if (this->hwIsBusy()) {
        this->readAsyncComplete_out(portNum, buffer, Drv::I2cStatus::I2C_OTHER_ERR);
        return;
    }
    FW_ASSERT(buffer.getData() != nullptr);
    FW_ASSERT_NO_OVERFLOW(addr, U16);
    FW_ASSERT_NO_OVERFLOW(buffer.getSize(), U16);

    this->m_asyncPortNum = portNum;
    this->m_asyncDevAddress = static_cast<U16>(addr);
    this->m_asyncReadBuffer = buffer;
    (void)this->m_asyncDmaStart.now();

    this->m_asyncPending = true;
    this->m_asyncOp = AsyncOp::READ;
    this->m_dmaBusy = true;
    this->m_dmaErrorCode = 0;
    const I32 status =
        this->hwStartAsyncReceive(this->m_asyncDevAddress, buffer.getData(), static_cast<U16>(buffer.getSize()));
    if (status != 0) {  // 0 == HAL_OK
        this->m_asyncPending = false;
        this->m_dmaBusy = false;
        Fw::LogStringArg _op("Master_Receive_DMA");
        this->log_WARNING_HI_HalError(_op, addr, status);
        this->readAsyncComplete_out(portNum, buffer, Drv::I2cStatus::I2C_READ_ERR);
    }
}

void Stm32I2cDriver ::writeReadAsync_handler(const FwIndexType portNum,
                                              U32 addr,
                                              Fw::Buffer& writeBuffer,
                                              Fw::Buffer& readBuffer) {
    if (!this->m_opened) {
        this->writeReadAsyncComplete_out(portNum, writeBuffer, readBuffer, Drv::I2cStatus::I2C_OPEN_ERR);
        return;
    }
    if ((this->m_transferMode != TransferMode::DMA) || this->m_asyncPending) {
        this->writeReadAsyncComplete_out(portNum, writeBuffer, readBuffer, Drv::I2cStatus::I2C_OTHER_ERR);
        return;
    }
    if (this->hwIsBusy()) {
        this->writeReadAsyncComplete_out(portNum, writeBuffer, readBuffer, Drv::I2cStatus::I2C_OTHER_ERR);
        return;
    }
    FW_ASSERT(writeBuffer.getData() != nullptr);
    FW_ASSERT(readBuffer.getData() != nullptr);
    FW_ASSERT_NO_OVERFLOW(addr, U16);
    FW_ASSERT_NO_OVERFLOW(writeBuffer.getSize(), U16);
    FW_ASSERT_NO_OVERFLOW(readBuffer.getSize(), U16);

    this->m_asyncPortNum = portNum;
    this->m_asyncDevAddress = static_cast<U16>(addr);
    this->m_asyncWriteBuffer = writeBuffer;
    this->m_asyncReadBuffer = readBuffer;
    (void)this->m_asyncDmaStart.now();

    this->m_asyncPending = true;
    this->m_asyncOp = AsyncOp::WRITE_READ_WRITE_PHASE;
    this->m_dmaBusy = true;
    this->m_dmaErrorCode = 0;
    const I32 status = this->hwStartAsyncTransmit(this->m_asyncDevAddress, writeBuffer.getData(),
                                                   static_cast<U16>(writeBuffer.getSize()));
    if (status != 0) {  // 0 == HAL_OK
        this->m_asyncPending = false;
        this->m_dmaBusy = false;
        Fw::LogStringArg _op("Master_Transmit_DMA");
        this->log_WARNING_HI_HalError(_op, addr, status);
        this->writeReadAsyncComplete_out(portNum, writeBuffer, readBuffer, Drv::I2cStatus::I2C_WRITE_ERR);
    }
}

void Stm32I2cDriver ::poll() {
    // m_asyncPending spans from request-accepted until this method has
    // delivered the completion -- distinct from m_dmaBusy, which only
    // tracks whether the ISR (signalDmaComplete()/signalDmaError()) has
    // latched an outcome for the *current phase*. Without that split, a
    // second poll() call after delivery would see the transaction "done"
    // and re-deliver the same (stale) buffers.
    if (!this->m_asyncPending) {
        return;
    }

    bool phaseOk;
    if (!this->m_dmaBusy) {
        phaseOk = (this->m_dmaErrorCode == 0);
    } else {
        Os::RawTime now;
        (void)now.now();
        U32 elapsedUs = 0;
        (void)now.getDiffUsec(this->m_asyncDmaStart, elapsedUs);
        if (elapsedUs < (TRANSACTION_TIMEOUT_MS * 1000U)) {
            return;  // still in flight, not yet past this transaction's watchdog
        }
        this->hwAbortAsyncTransaction(this->m_asyncDevAddress);
        this->m_dmaBusy = false;
        this->log_WARNING_HI_HalError(Fw::LogStringArg("Master_DMA"), this->m_asyncDevAddress, 3 /* HAL_TIMEOUT */);
        phaseOk = false;
    }

    // The write half of a writeReadAsync completing successfully starts
    // the read half instead of delivering -- the caller only sees one
    // completion for the whole two-phase operation, same as the blocking
    // writeRead_handler()'s back-to-back calls above.
    if (phaseOk && (this->m_asyncOp == AsyncOp::WRITE_READ_WRITE_PHASE)) {
        this->m_asyncOp = AsyncOp::WRITE_READ_READ_PHASE;
        (void)this->m_asyncDmaStart.now();
        this->m_dmaBusy = true;
        this->m_dmaErrorCode = 0;
        const I32 status = this->hwStartAsyncReceive(this->m_asyncDevAddress, this->m_asyncReadBuffer.getData(),
                                                       static_cast<U16>(this->m_asyncReadBuffer.getSize()));
        if (status != 0) {  // 0 == HAL_OK
            this->m_dmaBusy = false;
            Fw::LogStringArg _op("Master_Receive_DMA");
            this->log_WARNING_HI_HalError(_op, this->m_asyncDevAddress, status);
            this->m_asyncPending = false;
            this->writeReadAsyncComplete_out(this->m_asyncPortNum, this->m_asyncWriteBuffer, this->m_asyncReadBuffer,
                                              Drv::I2cStatus::I2C_READ_ERR);
        }
        return;
    }

    this->m_asyncPending = false;
    const AsyncOp op = this->m_asyncOp;
    const FwIndexType portNum = this->m_asyncPortNum;
    Fw::Buffer writeBuffer = this->m_asyncWriteBuffer;
    Fw::Buffer readBuffer = this->m_asyncReadBuffer;

    if (op == AsyncOp::WRITE) {
        const Drv::I2cStatus status = phaseOk ? Drv::I2cStatus::I2C_OK : Drv::I2cStatus::I2C_WRITE_ERR;
        this->writeAsyncComplete_out(portNum, writeBuffer, status);
    } else {
        // READ and WRITE_READ_READ_PHASE both finish with a receive --
        // invalidate the D-cache on success so the CPU reads what the DMA
        // controller actually wrote.
        if (phaseOk) {
            this->hwFinishAsyncReceive(readBuffer.getData(), readBuffer.getSize());
        }
        const Drv::I2cStatus status = phaseOk ? Drv::I2cStatus::I2C_OK : Drv::I2cStatus::I2C_READ_ERR;
        if (op == AsyncOp::READ) {
            this->readAsyncComplete_out(portNum, readBuffer, status);
        } else {
            this->writeReadAsyncComplete_out(portNum, writeBuffer, readBuffer, status);
        }
    }
}

}  // namespace Stm32