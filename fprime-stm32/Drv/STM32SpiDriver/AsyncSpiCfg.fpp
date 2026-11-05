module Drv {
    module AsyncSpiCfg {
        @ Number of async SPI driver ports, matching Drv.AsyncI2cCfg.I2cDriverPorts' convention
        @ (lib/fprime's own Drv.AsyncSpi/AsyncGuardedSpi interfaces, and the AsyncSpiCfg module and
        @ SpiWriteReadRequest/SpiWriteReadCallback port types that configure them, aren't present in
        @ this project's pinned fprime v4.4.1 -- Stm32SpiDriver hand-declares its own spiWriteReadAsync
        @ port against this constant and the port types declared below instead).
        @ Each client of the SPI driver should reserve one port index for its request/completion port pair.
        constant SpiDriverPorts = 10
    }
}

module Drv {
    @ Request an asynchronous SPI write/read transaction (no return value, like Drv.I2cRequest).
    @ Not present in this project's pinned fprime v4.4.1 (added upstream in a later Drv.AsyncSpi
    @ commit) -- declared here instead, matching Drv.SpiWriteRead's own ref-buffer signature.
    @ Exactly one Drv.SpiWriteReadCallback invocation follows every request, carrying the outcome in
    @ its status; a request the driver cannot take (e.g. SPI_OTHER_ERR while another transaction is
    @ in flight, SPI_OPEN_ERR before configuration) is reported the same way.
    port SpiWriteReadRequest(
                              ref writeBuffer: Fw.Buffer @< Buffer with data to write
                              ref readBuffer:  Fw.Buffer @< Buffer to read into, size set by the caller
                            )

    @ Completion callback for an asynchronous SPI write/read transaction.
    @ Both buffers are handed back with the data pointer and size they had in the request.
    @ Drivers must invoke this port from a non-interrupt context.
    port SpiWriteReadCallback(
                               ref writeBuffer: Fw.Buffer @< Write buffer passed in the request
                               ref readBuffer:  Fw.Buffer @< Read buffer passed in the request, holding received data
                               status: Drv.SpiStatus @< Transaction result status
                             )
}
