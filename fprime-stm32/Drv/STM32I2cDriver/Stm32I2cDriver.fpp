module Stm32 {

  passive component Stm32I2cDriver {

    import Drv.I2c

    # ----------------------------------------------------------------------
    # Asynchronous (DMA-mode) I2C contract
    # ----------------------------------------------------------------------

    @ Asynchronous counterparts to the Drv.I2c write/read/writeRead ports
    @ above, for instances opened with TransferMode::DMA. Hand-declared
    @ instead of `import Drv.AsyncGuardedI2c` because both Drv.I2c and
    @ Drv.AsyncGuardedI2c declare guarded ports literally named `write`,
    @ `read`, and `writeRead`; FPP rejects importing both into one component
    @ as duplicate port names (same reasoning as Stm32SpiDriver's
    @ spiWriteReadAsync -- see its own .fpp comment). Port types and array
    @ sizing are identical to Drv.AsyncGuardedI2c's, so any client wired
    @ against that interface connects here identically -- only the port
    @ names differ.
    @ Every request is answered with exactly one matching completion
    @ invocation (I2C_OK; I2C_OTHER_ERR if a transaction is already in
    @ flight on this instance, if this instance was not opened with
    @ TransferMode::DMA, or on a HAL/DMA error or watchdog timeout),
    @ delivered from poll(), never from an ISR.
    guarded input port writeAsync: [Drv.AsyncI2cCfg.I2cDriverPorts] Drv.I2cRequest

    guarded input port readAsync: [Drv.AsyncI2cCfg.I2cDriverPorts] Drv.I2cRequest

    @ writeRead has no single-transaction HAL equivalent on this bus (see
    @ Stm32I2cDriverCommon.cpp's note on why it can't hold the bus with one
    @ electrically-held repeated START): the write phase and read phase run
    @ as two back-to-back async transfers across separate poll() calls, but
    @ the caller still sees exactly one request and one completion.
    guarded input port writeReadAsync: [Drv.AsyncI2cCfg.I2cDriverPorts] Drv.I2cWriteReadRequest

    ###### Ports below must be connected for any instance opened with TransferMode::DMA ######

    output port writeAsyncComplete: [Drv.AsyncI2cCfg.I2cDriverPorts] Drv.I2cCallback

    output port readAsyncComplete: [Drv.AsyncI2cCfg.I2cDriverPorts] Drv.I2cCallback

    output port writeReadAsyncComplete: [Drv.AsyncI2cCfg.I2cDriverPorts] Drv.I2cWriteReadCallback

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
