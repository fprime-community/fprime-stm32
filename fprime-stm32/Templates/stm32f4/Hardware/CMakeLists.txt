####
# Board hardware support for the FprimeBaremetalReference project.
#
# This is deliberately owned by the project, not by the fprime-stm32
# submodule: fprime-stm32 only expects a target named `FprimeStm32` to exist
# and only supplies hardware-agnostic OSAL delegates/drivers on top of it.
# Swapping to different hardware means editing this directory (and
# regenerating stm32f401_hal/ from its .ioc in CubeMX), never forking the
# submodule.
####

if (FPRIME_PLATFORM STREQUAL "stm32f4")
    set(FPRIME_STM32_HAL_SOURCES
        "config/src/stm32f4_clock.c"
        "config/src/tim2_clock.cpp"
        "stm32f401_hal/Core/Src/gpio.c"
        "stm32f401_hal/Core/Src/dma.c"
        "stm32f401_hal/Core/Src/tim.c"
        "stm32f401_hal/Core/Src/usart.c"
        "stm32f401_hal/Core/Src/stm32f4xx_it.c"
        "stm32f401_hal/Core/Src/stm32f4xx_hal_msp.c"
        "stm32f401_hal/Core/Src/sysmem.c"
        "stm32f401_hal/Core/Src/syscalls.c"
        "stm32f401_hal/startup_stm32f401xe.s"
        "stm32f401_hal/Core/Src/system_stm32f4xx.c"
        "stm32f401_hal/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim.c"
        "stm32f401_hal/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim_ex.c"
        "stm32f401_hal/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc.c"
        "stm32f401_hal/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc_ex.c"
        "stm32f401_hal/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash.c"
        "stm32f401_hal/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ex.c"
        "stm32f401_hal/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ramfunc.c"
        "stm32f401_hal/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c"
        "stm32f401_hal/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma_ex.c"
        "stm32f401_hal/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma.c"
        "stm32f401_hal/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr.c"
        "stm32f401_hal/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr_ex.c"
        "stm32f401_hal/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_cortex.c"
        "stm32f401_hal/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c"
        "stm32f401_hal/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_exti.c"
        "stm32f401_hal/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_uart.c"
    )

    add_library(FprimeStm32 STATIC ${FPRIME_STM32_HAL_SOURCES})

    target_include_directories(FprimeStm32 PUBLIC
        "${PROJECT_SOURCE_DIR}"
        "${CMAKE_BINARY_DIR}"
        "${FPRIME_PROJECT_ROOT}"
        "${CMAKE_CURRENT_BINARY_DIR}"
        "${CMAKE_CURRENT_LIST_DIR}/config/include"
        "${CMAKE_CURRENT_LIST_DIR}/stm32f401_hal/Core/Inc"
        "${CMAKE_CURRENT_LIST_DIR}/stm32f401_hal/Drivers/CMSIS/Include"
        "${CMAKE_CURRENT_LIST_DIR}/stm32f401_hal/Drivers/CMSIS/Device/ST/STM32F4xx/Include"
        "${CMAKE_CURRENT_LIST_DIR}/stm32f401_hal/Drivers/STM32F4xx_HAL_Driver/Inc"
    )

    target_compile_definitions(FprimeStm32 PUBLIC STM32F401xE USE_HAL_DRIVER)

    target_compile_options(FprimeStm32 PRIVATE -ffunction-sections -fdata-sections)

    target_link_libraries(FprimeStm32 PUBLIC PlatformStm32f4)

    set(FPRIME_STM32_LINKER_SCRIPT
        "${CMAKE_CURRENT_LIST_DIR}/linker/STM32F401XX_FLASH.ld" CACHE INTERNAL "")
    set(FPRIME_STM32_STARTUP_SOURCE
        "${CMAKE_CURRENT_LIST_DIR}/startup/startup_stm32f401xe.s" CACHE INTERNAL "")
    set(FPRIME_STM32_IT_SOURCE
        "${CMAKE_CURRENT_LIST_DIR}/stm32f401_hal/Core/Src/stm32f4xx_it.c" CACHE INTERNAL "")
endif()