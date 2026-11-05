####
# F prime CMakeLists.txt:
####
if (FPRIME_PLATFORM MATCHES "^stm32" AND NOT STM32_FORCE_STUBBED_DRIVERS)
    set(STM32_GPIO_DRIVER_IMPL_SOURCE "${CMAKE_CURRENT_LIST_DIR}/Stm32GpioDriver.cpp")
    set(STM32_GPIO_DRIVER_IMPL_DEPENDS FprimeStm32)
else()
    set(STM32_GPIO_DRIVER_IMPL_SOURCE "${CMAKE_CURRENT_LIST_DIR}/Stm32GpioDriverStub.cpp")
    set(STM32_GPIO_DRIVER_IMPL_DEPENDS FprimeStm32Config)
endif()

register_fprime_module(
  AUTOCODER_INPUTS
    "${CMAKE_CURRENT_LIST_DIR}/Stm32GpioDriver.fpp"
  SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/Stm32GpioDriverCommon.cpp"
    "${STM32_GPIO_DRIVER_IMPL_SOURCE}"
  HEADERS
    "${CMAKE_CURRENT_LIST_DIR}/Stm32GpioDriver.hpp"
  DEPENDS
    ${STM32_GPIO_DRIVER_IMPL_DEPENDS}
)

register_fprime_ut(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/Stm32GpioDriver.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/Stm32GpioDriverCommon.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/Stm32GpioDriverStub.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/Stm32GpioDriverTestMain.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/Stm32GpioDriverTester.cpp"
    DEPENDS
        STest # For rules-based testing
    UT_AUTO_HELPERS
)
