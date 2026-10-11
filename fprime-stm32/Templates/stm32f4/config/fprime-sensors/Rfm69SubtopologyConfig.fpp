module Rfm69 {
    module SubtopologyConfig {
        constant BASE_ID = 0xC0000000
    }

    #instance spiDriver: Stm32.Stm32SpiDriver base id Rfm69.SubtopologyConfig.BASE_ID + 0x00002000

    instance resetGpio: Stm32.Stm32GpioDriver base id Rfm69.SubtopologyConfig.BASE_ID + 0x00005000

    instance rfm69Sim: Rfm69.Rfm69Sim base id Rfm69.SubtopologyConfig.BASE_ID + 0x00003000
}
