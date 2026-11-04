// ======================================================================
// \title  Stm32I2cDriver.hpp
// \author ivanlara
// \brief  hpp file for the STM32H7 I2C1 blocking master driver
// ======================================================================

#ifndef Stm32_Stm32I2cDriver_HPP
#define Stm32_Stm32I2cDriver_HPP

#include "fprime-stm32/Drv/STM32I2cDriver/Stm32I2cDriverComponentAc.hpp"
#include <Fw/Types/BasicTypes.hpp>
#include <Fw/Types/DirectionEnumAc.hpp>
#include <Fw/Types/SuccessEnumAc.hpp>
#include <Fw/Types/LogicEnumAc.hpp>
#include <Fw/Types/Assert.hpp>
#include <Os/RawTime.hpp>
#include <config/Stm32Config.hpp>
#include <config/Stm32TransferMode.hpp>

namespace Stm32 {

//! I2C peripheral identifier, covering every instance present on an STM32H753.
//! Only I2c1 has a CubeMX-generated MX_I2Cn_Init()/handle in this project
//! today (see src/i2c.c/include/i2c.h) -- the others are declared so a
//! different board/peripheral selection only requires adding that CubeMX
//! config and one switch case in Stm32I2cDriver.cpp, not touching this
//! header, Common.cpp, or the Stub.
enum class I2cInstance { I2c1, I2c2, I2c3, I2c4 };

//! Supported I2C bus clock presets. Unlike UART's baud rate, the STM32H7's
//! I2C Timing register has no closed-form runtime formula in the HAL -- each
//! value is a CubeMX-computed constant for this project's actual peripheral
//! clock (D2PCLK1), not derived at runtime, so only the presets CubeMX has
//! actually generated for this project are offered here.
enum class I2cBusSpeed { Standard, Fast, FastPlus };

class Stm32I2cDriver final : public Stm32I2cDriverComponentBase {
  public:
    //! Construct Stm32I2cDriver object
    Stm32I2cDriver(const char* const compName  //!< The component name
    );

    //! Destroy Stm32I2cDriver object
    ~Stm32I2cDriver();

    //! Configure the selected I2C instance (via the CubeMX-generated
    //! MX_I2Cn_Init(), then apply the requested bus speed preset) and mark
    //! the driver ready to accept transactions. Must be called once from
    //! configureTopology(), before any write/read/writeRead port
    //! invocation.
    //! \param instance which I2C peripheral this driver instance owns
    //! \param busSpeed requested bus clock preset (default matches this
    //!        project's current 400 kHz Fast-mode configuration)
    //! \param mode POLLED (default) issues a single blocking
    //!        HAL_I2C_Master_Transmit/Receive() per transaction from
    //!        write/read/writeRead. DMA instead serves writeAsync/readAsync/
    //!        writeReadAsync: a request starts HAL_I2C_Master_Transmit_DMA()/
    //!        HAL_I2C_Master_Receive_DMA() and returns immediately, with
    //!        poll() draining the ISR-latched completion and invoking the
    //!        matching *Complete port -- see poll()'s own doc comment.
    //!        D-cache maintenance and the DMA-safe-buffer check happen
    //!        around every DMA-mode transfer either way.
    //! Runs MX_I2Cn_Init() for the selected instance and applies the requested
    //! bus speed preset (re-running HAL_I2C_Init() if it differs from
    //! CubeMX's baked-in default). NVIC configuration only happens when
    //! `mode` is DMA (needed for the I2C event/error IRQ that signals DMA
    //! completion). Implemented directly in Stm32I2cDriver.cpp (real) /
    //! Stm32I2cDriverStub.cpp (host), since it is the HAL boundary itself.
    Fw::Success open(I2cInstance instance, I2cBusSpeed busSpeed = I2cBusSpeed::Fast,
                      TransferMode mode = TransferMode::POLLED);

    //! One bounded step of the DMA state machine: consume the ISR-latched
    //! completion/error flag, run cache maintenance, advance a two-phase
    //! writeReadAsync to its read half, invoke the matching completion
    //! callback, and recover a stuck transaction past its watchdog. Called
    //! every cyclic-executive pass (not from a rate group) so DMA
    //! completions are serviced with minimal latency -- same contract as
    //! Stm32SpiDriver::poll()/Stm32UartDriver::poll(). A POLLED-mode
    //! instance's poll() call is a no-op (m_asyncPending is never set), so
    //! it is safe to call unconditionally regardless of this instance's mode.
    void poll();

    // ----------------------------------------------------------------------
    // ISR signal surface: called by the real HAL callback trampoline (free
    // functions with no user-context pointer) on the stm32h7 target when
    // this instance is open in DMA mode. A unit test may also call these
    // directly to simulate a hardware event, since no ISR exists on the
    // host.
    // ----------------------------------------------------------------------

    //! Signal that the in-flight Transmit/Receive DMA transaction completed successfully.
    void signalDmaComplete();

    //! Signal that the I2C/DMA latched the given HAL error code.
    void signalDmaError(U32 errorCode);

  private:
    //! Bounded per-transaction watchdog passed to every blocking HAL_I2C_*
    //! call (Checklist Week 10: "10 ms transaction watchdog"), and to every
    //! async transaction's poll()-observed completion.
    static constexpr U32 TRANSACTION_TIMEOUT_MS = 10;

    //! POLLED mode only: a single blocking HAL_I2C_Master_Transmit() of
    //! `len` bytes to `devAddress`, bounded by TRANSACTION_TIMEOUT_MS.
    //! Returns I2C_OK, I2C_ADDRESS_ERR (address-phase NACK), or
    //! I2C_WRITE_ERR; emits HalError itself on failure, since only this
    //! method knows the raw HAL_StatusTypeDef.
    Drv::I2cStatus hwMasterTransmit(U16 devAddress, U8* data, U16 len);

    //! POLLED mode only: a single blocking HAL_I2C_Master_Receive() of
    //! `len` bytes from `devAddress`, bounded by TRANSACTION_TIMEOUT_MS.
    //! Returns I2C_OK, I2C_ADDRESS_ERR, or I2C_READ_ERR. Same
    //! error-reporting convention as hwMasterTransmit().
    Drv::I2cStatus hwMasterReceive(U16 devAddress, U8* data, U16 len);

    //! DMA mode only: clean the D-cache over [data, data + len) and start an
    //! HAL_I2C_Master_Transmit_DMA() transaction, returning immediately.
    //! Returns the raw HAL_StatusTypeDef value (0 == HAL_OK); poll()
    //! observes the ISR-latched outcome later.
    I32 hwStartAsyncTransmit(U16 devAddress, U8* data, U16 len);

    //! DMA mode only: assert `data` is DMA-safe and start an
    //! HAL_I2C_Master_Receive_DMA() transaction, returning immediately.
    //! Returns the raw HAL_StatusTypeDef value (0 == HAL_OK).
    I32 hwStartAsyncReceive(U16 devAddress, U8* data, U16 len);

    //! DMA mode only: abort an in-flight Transmit/Receive DMA transaction
    //! (watchdog recovery). The abort itself completes via the same
    //! completion ISR as a normal transaction.
    void hwAbortAsyncTransaction(U16 devAddress);

    //! DMA mode only: invalidate the D-cache over [data, data + len) after a
    //! successful Receive DMA transaction, so the CPU reads what the DMA
    //! controller actually wrote.
    void hwFinishAsyncReceive(U8* data, FwSizeType len);

    // ----------------------------------------------------------------------
    // Handler implementations for user-defined typed input ports
    // ----------------------------------------------------------------------

    //! Guarded synchronous write. Returns I2C_OPEN_ERR before open().
    Drv::I2cStatus write_handler(const FwIndexType portNum, U32 addr, Fw::Buffer& serBuffer) override;

    //! Guarded synchronous read. Returns I2C_OPEN_ERR before open().
    Drv::I2cStatus read_handler(const FwIndexType portNum, U32 addr, Fw::Buffer& serBuffer) override;

    //! Guarded write-then-read. NOTE: implemented as two back-to-back
    //! blocking HAL_I2C_Master_Transmit()/HAL_I2C_Master_Receive() calls
    //! (STOP then START), not a single electrically-held repeated START --
    //! see Stm32I2cDriverCommon.cpp for why that is acceptable on this
    //! single-master bus and what would need to change to hold the bus.
    Drv::I2cStatus writeRead_handler(const FwIndexType portNum,
                                      U32 addr,
                                      Fw::Buffer& writeBuffer,
                                      Fw::Buffer& readBuffer) override;

    // ----------------------------------------------------------------------
    // Asynchronous (DMA-mode) I2C contract: a request starts the transfer
    // and returns immediately; poll() drains the ISR-latched outcome and
    // delivers it via the matching *AsyncComplete port, never from this
    // handler and never from an ISR -- same completion-delivery contract
    // Stm32SpiDriver documents for spiWriteReadAsync/poll().
    // ----------------------------------------------------------------------

    //! Asynchronous write request (DMA-mode instances only). No return
    //! value: the outcome is delivered later via writeAsyncComplete_out(),
    //! from poll(). Immediately replies I2C_OTHER_ERR if a transaction is
    //! already in flight on this instance or this instance is not open in
    //! DMA mode, or I2C_OPEN_ERR before open().
    void writeAsync_handler(const FwIndexType portNum, U32 addr, Fw::Buffer& buffer) override;

    //! Asynchronous read request (DMA-mode instances only). Same immediate-
    //! rejection/delivery contract as writeAsync_handler().
    void readAsync_handler(const FwIndexType portNum, U32 addr, Fw::Buffer& buffer) override;

    //! Asynchronous write-then-read request (DMA-mode instances only). The
    //! write half starts immediately; poll() starts the read half once the
    //! write completes, then delivers the combined outcome via
    //! writeReadAsyncComplete_out(). Same immediate-rejection contract as
    //! writeAsync_handler().
    void writeReadAsync_handler(const FwIndexType portNum,
                                 U32 addr,
                                 Fw::Buffer& writeBuffer,
                                 Fw::Buffer& readBuffer) override;

    //! Which async operation (if any) is in flight, so poll() knows which
    //! *AsyncComplete port to invoke and -- for WRITE_READ -- which phase to
    //! run next. IDLE means no async request is outstanding (including on a
    //! POLLED-mode instance, which never starts one).
    enum class AsyncOp { IDLE, WRITE, READ, WRITE_READ_WRITE_PHASE, WRITE_READ_READ_PHASE };

    I2cInstance m_instance;
    TransferMode m_transferMode;

    bool m_opened;

    //! Completion/error state latched by signalDmaComplete()/signalDmaError()
    //! and consumed by hwMasterTransmit()/hwMasterReceive()'s DMA-mode busy-wait,
    //! or drained by poll() for the async ports.
    volatile bool m_dmaBusy;
    U32 m_dmaErrorCode;

    //! True from an accepted async request until poll() has delivered its
    //! completion. Distinct from m_dmaBusy (ISR-cleared, tracks only
    //! whether the in-flight phase itself finished) so a second poll() call
    //! after delivery does not re-deliver stale buffers.
    bool m_asyncPending;
    AsyncOp m_asyncOp;

    //! In-flight async transaction state, valid while m_asyncPending is
    //! true. portNum identifies which request/completion array slot to
    //! reply on; devAddress is re-used for the read phase of a
    //! WRITE_READ_WRITE_PHASE -> WRITE_READ_READ_PHASE transition; the
    //! buffers are handed back as-is to the completion callback.
    FwIndexType m_asyncPortNum;
    U16 m_asyncDevAddress;
    Fw::Buffer m_asyncWriteBuffer;
    Fw::Buffer m_asyncReadBuffer;
    Os::RawTime m_asyncDmaStart;
};

}  // namespace Stm32

#endif // FPRIME_STM32_DRV_STM32I2CDRIVER_STM32I2CDRIVER_HPP