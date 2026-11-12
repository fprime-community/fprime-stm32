####
# F Prime CMakeLists.txt:
####
if (FPRIME_PLATFORM MATCHES "^stm32" AND NOT STM32_FORCE_STUBBED_DRIVERS)
    set(STM32_SPI_DRIVER_IMPL_SOURCE "${CMAKE_CURRENT_LIST_DIR}/Stm32SpiDriver.cpp")
    set(STM32_SPI_DRIVER_IMPL_DEPENDS Os FprimeStm32Config FprimeStm32 FprimeStm32CoreCortexM7)
else()
    set(STM32_SPI_DRIVER_IMPL_SOURCE "${CMAKE_CURRENT_LIST_DIR}/Stm32SpiDriverStub.cpp")
    set(STM32_SPI_DRIVER_IMPL_DEPENDS FprimeStm32Config)
endif()

register_fprime_module(
  AUTOCODER_INPUTS
    "${CMAKE_CURRENT_LIST_DIR}/Stm32SpiDriver.fpp"
  SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/Stm32SpiDriverCommon.cpp"
    "${STM32_SPI_DRIVER_IMPL_SOURCE}"
  HEADERS
    "${CMAKE_CURRENT_LIST_DIR}/Stm32SpiDriver.hpp"
  DEPENDS
    ${STM32_SPI_DRIVER_IMPL_DEPENDS}
    fprime-stm32_Drv_STM32GpioDriver
)

### Unit Tests ###
register_fprime_ut(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/Stm32SpiDriver.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/Stm32SpiDriverCommon.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Stm32SpiDriverStub.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/Stm32SpiDriverTestMain.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/Stm32SpiDriverTester.cpp"
    DEPENDS
        STest # For rules-based testing
        fprime-stm32_Drv_STM32GpioDriver
    UT_AUTO_HELPERS
)