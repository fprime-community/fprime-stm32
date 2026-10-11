####
# STM32F4 bare-metal platform.
####

set(FPRIME_USE_BAREMETAL_SCHEDULER ON CACHE BOOL
    "Use cooperative bare-metal task scheduling" FORCE)
set(FPRIME_USE_POSIX OFF CACHE INTERNAL "STM32F4 has no POSIX layer" FORCE)
set(FPRIME_HAS_SOCKETS OFF CACHE INTERNAL "STM32F4 has no socket layer" FORCE)
set(FPRIME_STM32_BOOTSTRAP_POOL_SIZE 32768 CACHE INTERNAL
    "STM32F4 bootstrap pool size (bytes)" FORCE)

add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Platform")

register_fprime_config(
    PlatformStm32f4
    INTERFACE
    CHOOSES_IMPLEMENTATIONS
        Os_Cpu_Baremetal
        Os_Memory_Baremetal
        Os_Task_Stm32F4
        Os_File_Baremetal_MicroFs
        Os_Console_Stub
        Os_Mutex_Stm32F4
        Os_Queue_Stm32F4
        Os_RawTime_Stm32F4
        Os_CountingSemaphore_Stub
        Fw_StringFormat_snprintf
        Fw_StringScan_sscanf
    BASE_CONFIG
)

target_compile_definitions(PlatformStm32f4 INTERFACE
    TGT_OS_TYPE_STM32F4
    FPRIME_STM32_UART_TX_RING_SIZE=2048
    FPRIME_STM32_UART_RX_RING_SIZE=2048
)
