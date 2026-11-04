// ======================================================================
// \title  Stm32SpiDriver.hpp
// \author ivanlara
// \brief  hpp file for the STM32H7 SPI blocking/polled master driver
// ======================================================================

#ifndef Stm32_Stm32SpiDriver_HPP
#define Stm32_Stm32SpiDriver_HPP

#include "fprime-stm32/Drv/STM32SpiDriver/Stm32SpiDriverComponentAc.hpp"
#include "fprime-stm32/Drv/STM32GpioDriver/Stm32GpioDriver.hpp"
#include <Fw/Types/BasicTypes.hpp>
#include <Fw/Types/SuccessEnumAc.hpp>
#include <Fw/Types/Assert.hpp>
#include <Os/RawTime.hpp>
#include <config/Stm32Config.hpp>
#include <config/Stm32TransferMode.hpp>

namespace Stm32 {

//! SPI peripheral identifier, covering every instance present on an STM32H753.
//! Only Spi5 has a CubeMX-generated MX_SPIn_Init()/handle in this project
//! today (see Hardware/stm32h753_hal/Core/Src/spi.c) -- the others are
//! declared so a different board/peripheral selection only requires adding
//! that CubeMX config and one switch case in Stm32SpiDriver.cpp, not
//! touching this header, Common.cpp, or the Stub.
enum class SpiInstance { Spi1, Spi2, Spi3, Spi4, Spi5, Spi6 };

class Stm32SpiDriver final : public Stm32SpiDriverComponentBase {
  public:
    //! Construct Stm32SpiDriver object
    Stm32SpiDriver(const char* const compName  //!< The component name
    );

    //! Destroy Stm32SpiDriver object
    ~Stm32SpiDriver();

    //! Configure the selected SPI instance (via the CubeMX-generated
    //! MX_SPIn_Init()) and the chip-select GPIO pin this instance drives
    //! around every transfer (SPI5 is configured NSS_SOFT -- the HAL never
    //! toggles a CS pin on its own), then mark the driver ready to accept
    //! transactions. Must be called once from configureTopology(), before
    //! any SpiWriteRead/SpiReadWrite port invocation.
    //! \param instance which SPI peripheral this driver instance owns
    //! \param csPort GPIO port of the chip-select pin this instance drives
    //! \param csPin GPIO pin number of the chip-select pin
    //! \param timeoutMs per-transaction blocking watchdog passed to every
    //!        HAL_SPI_TransmitReceive()/HAL_SPI_TransmitReceive_DMA() call
    //! \param mode POLLED (default, backward-compatible) issues a single
    //!        blocking HAL_SPI_TransmitReceive() per transfer from
    //!        SpiWriteRead/SpiReadWrite. DMA instead serves
    //!        spiWriteReadAsync: a request starts HAL_SPI_TransmitReceive_DMA()
    //!        and returns immediately, with poll() draining the ISR-latched
    //!        completion and invoking spiWriteReadAsyncComplete -- see
    //!        poll()'s own doc comment. D-cache maintenance and the
    //!        DMA-safe-buffer check happen around every DMA-mode transfer
    //!        either way.
    Fw::Success open(SpiInstance instance,
                      Stm32::GpioPort csPort,
                      U16 csPin,
                      U32 timeoutMs = 10,
                      TransferMode mode = TransferMode::POLLED);

    //! One bounded step of the DMA state machine: consume the ISR-latched
    //! completion/error flag, run cache maintenance, invoke the completion
    //! callback, and recover a stuck transfer past its watchdog. Called
    //! every cyclic-executive pass (not from a rate group) so DMA
    //! completions are serviced with minimal latency -- same contract as
    //! Stm32UartDriver::poll(). A POLLED-mode instance's poll() call is a
    //! no-op (m_dmaBusy is never set), so it is safe to call unconditionally
    //! regardless of this instance's mode.
    void poll();

    // ----------------------------------------------------------------------
    // ISR signal surface: called by the real HAL callback trampoline (free
    // functions with no user-context pointer) on the stm32h7 target when
    // this instance is open in DMA mode. A unit test may also call these
    // directly to simulate a hardware event, since no ISR exists on the
    // host.
    // ----------------------------------------------------------------------

    //! Signal that the in-flight TransmitReceive DMA transfer completed successfully.
    void signalDmaComplete();

    //! Signal that USART/SPI DMA latched the given HAL error code.
    void signalDmaError(U32 errorCode);

  private:
    //! Drive the chip-select pin. `active` selects the device (CS low);
    //! !active idles the bus (CS high). Re-resolves the HAL GPIO port from
    //! m_csPort on every call rather than caching a HAL pointer, so that
    //! multiple Stm32SpiDriver instances (different bus + CS per sensor)
    //! never share any HAL-typed state.
    void hwSetCs(bool active);

    //! POLLED mode only: a single blocking HAL_SPI_TransmitReceive() of
    //! `size` bytes, bounded by m_timeoutMs. Returns the raw
    //! HAL_StatusTypeDef value (0 == HAL_OK); does not toggle chip-select --
    //! callers bracket this with hwSetCs(). Re-resolves the HAL SPI handle
    //! from m_instance on every call, for the same multi-instance-safety
    //! reason as hwSetCs().
    I32 hwTransmitReceive(const U8* txData, U8* rxData, FwSizeType size);

    //! DMA mode only: clean the D-cache over [txData, txData + size) and
    //! start an HAL_SPI_TransmitReceive_DMA() transfer, returning
    //! immediately. Returns the raw HAL_StatusTypeDef value (0 == HAL_OK);
    //! does not toggle chip-select, does not wait for completion -- poll()
    //! observes the ISR-latched outcome later. Re-resolves the HAL SPI
    //! handle from m_instance on every call.
    I32 hwStartAsyncTransfer(const U8* txData, U8* rxData, FwSizeType size);

    //! DMA mode only: abort an in-flight TransmitReceive DMA transfer
    //! (watchdog recovery). The abort itself completes via the same
    //! completion ISR as a normal transfer.
    void hwAbortAsyncTransfer();

    //! DMA mode only: invalidate the D-cache over [rxData, rxData + size)
    //! after a successful TransmitReceive DMA transfer, so the CPU reads
    //! what the DMA controller actually wrote.
    void hwFinishAsyncTransfer(U8* rxData, FwSizeType size);

    // ----------------------------------------------------------------------
    // Handler implementations for user-defined typed input ports
    // ----------------------------------------------------------------------

    //! Guarded synchronous full-duplex transfer. Returns SPI_OPEN_ERR before open().
    Drv::SpiStatus SpiWriteRead_handler(FwIndexType portNum, Fw::Buffer& writeBuffer, Fw::Buffer& readBuffer) override;

    //! DEPRECATED synchronous full-duplex transfer (same operation, no return value).
    //! Forwards to SpiWriteRead_handler() and discards the status, matching
    //! Drv::LinuxSpiDriver's convention for this deprecated port.
    void SpiReadWrite_handler(FwIndexType portNum, Fw::Buffer& writeBuffer, Fw::Buffer& readBuffer) override;

    //! Asynchronous full-duplex transfer request (DMA-mode instances only).
    //! No return value: the outcome is delivered later via
    //! spiWriteReadAsyncComplete_out(), from poll(), never from this call.
    //! Immediately replies SPI_OTHER_ERR if a transfer is already in flight
    //! on this instance, or SPI_OPEN_ERR before open().
    void spiWriteReadAsync_handler(FwIndexType portNum, Fw::Buffer& writeBuffer, Fw::Buffer& readBuffer) override;

    SpiInstance m_instance;
    Stm32::GpioPort m_csPort;
    U16 m_csPin;
    U32 m_timeoutMs;
    TransferMode m_transferMode;
    bool m_opened;

    //! Completion/error state latched by signalDmaComplete()/signalDmaError()
    //! and drained by poll() (DMA mode only).
    volatile bool m_dmaBusy;
    U32 m_dmaErrorCode;

    //! True from an accepted spiWriteReadAsync request until poll() has
    //! delivered its completion. Distinct from m_dmaBusy (ISR-cleared,
    //! tracks only whether the transfer itself finished) so a second
    //! poll() call after delivery does not re-deliver stale buffers.
    bool m_asyncPending;

    //! In-flight async transaction state, valid while m_asyncPending is
    //! true. portNum identifies which spiWriteReadAsync/
    //! spiWriteReadAsyncComplete array slot to reply on; the buffers are
    //! handed back as-is to the completion callback (same convention as
    //! Drv::SpiWriteReadCallback).
    FwIndexType m_asyncPortNum;
    Fw::Buffer m_asyncWriteBuffer;
    Fw::Buffer m_asyncReadBuffer;
    Os::RawTime m_asyncDmaStart;
};

}  // namespace Stm32

#endif  // Stm32_Stm32SpiDriver_HPP
