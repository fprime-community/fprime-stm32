module Stm32 {

  passive component Stm32SpiDriver {

    import Drv.Spi

    # ----------------------------------------------------------------------
    # Asynchronous (DMA-mode) SPI contract
    # ----------------------------------------------------------------------

    @ Asynchronous counterpart to the Drv.Spi SpiWriteRead port above, for
    @ instances opened with TransferMode::DMA. Hand-declared instead of
    @ `import Drv.AsyncGuardedSpi` because both Drv.Spi and
    @ Drv.AsyncGuardedSpi declare a guarded port literally named
    @ `SpiWriteRead`; FPP rejects importing both into one component as a
    @ duplicate port name. Port types and array sizing come from upstream
    @ Drv.SpiWriteReadRequest/Drv.SpiWriteReadCallback/Drv.AsyncSpiCfg, so
    @ any client wired against Drv.AsyncGuardedSpi (e.g. Samd21::SpiDriver's
    @ clients) connects here identically -- only the port names differ.
    @ Every request is answered with exactly one SpiWriteReadComplete
    @ invocation (SPI_OK; SPI_OTHER_ERR if a transfer is already in flight on
    @ this instance; SPI_OTHER_ERR on a HAL/DMA error or watchdog timeout),
    @ delivered from poll(), never from an ISR and never from inside the
    @ request call itself.
    guarded input port spiWriteReadAsync: [Drv.AsyncSpiCfg.SpiDriverPorts] Drv.SpiWriteReadRequest

    ###### Port below must be connected for any instance opened with TransferMode::DMA ######

    @ Invoked when an spiWriteReadAsync transaction completes (see above)
    output port spiWriteReadAsyncComplete: [Drv.AsyncSpiCfg.SpiDriverPorts] Drv.SpiWriteReadCallback

    # ----------------------------------------------------------------------
    # Special ports
    # ----------------------------------------------------------------------

    event port Log

    text event port LogText

    time get port Time

    # ----------------------------------------------------------------------
    # Events
    # ----------------------------------------------------------------------

    include "Events.fppi"

  }

}
