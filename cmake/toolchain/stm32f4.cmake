####
# GNU Arm Embedded toolchain for the STM32F4 series (e.g. Nucleo-F446RE)
####

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(FPRIME_PLATFORM stm32f4 CACHE INTERNAL "F Prime platform" FORCE)

set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
set(CMAKE_OBJCOPY arm-none-eabi-objcopy)
set(CMAKE_SIZE arm-none-eabi-size)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Nucleo-F446RE uses a Cortex-M4 with a single-precision FPU
set(STM32F4_CPU_FLAGS "-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard")

set(CMAKE_C_FLAGS_INIT
    "${STM32F4_CPU_FLAGS} -ffunction-sections -fdata-sections -g3 -gdwarf-4"
)
set(CMAKE_CXX_FLAGS_INIT
    "${STM32F4_CPU_FLAGS} -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti -fno-threadsafe-statics -g3 -gdwarf-4 -include${CMAKE_CURRENT_LIST_DIR}/stm32f4-format-macros.h"
)
set(CMAKE_ASM_FLAGS_INIT "${STM32F4_CPU_FLAGS} -g3 -gdwarf-4")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)