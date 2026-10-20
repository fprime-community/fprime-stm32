module Bmp280 {
    module SubtopologyConfig {
        constant BASE_ID = 0xD0000000
    }

    #instance bmpDriver: Stm32.Stm32SpiDriver base id Bmp280.SubtopologyConfig.BASE_ID + 0x00002000
}
