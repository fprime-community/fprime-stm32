module MpuImu {

    constant BASE_ID = 0xE0000000

    module QueueSizes {
        constant imuManager  = 10
    }

    instance imuDriver: Stm32.Stm32I2cDriver base id MpuImu.BASE_ID + 0x00002000 {
        phase Fpp.ToCpp.Phases.configComponents """
        (void) MpuImu::imuDriver.open(Stm32::I2cInstance::I2c1, Stm32::I2cBusSpeed::Fast);
        """
    }
}
