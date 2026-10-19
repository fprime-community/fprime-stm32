####
# STM32H7 bare-metal platform.
####

set(FPRIME_USE_BAREMETAL_SCHEDULER ON CACHE BOOL
    "Use cooperative bare-metal task scheduling" FORCE)
set(FPRIME_USE_POSIX OFF CACHE INTERNAL "STM32H7 has no POSIX layer" FORCE)
set(FPRIME_HAS_SOCKETS OFF CACHE INTERNAL "STM32H7 has no socket layer" FORCE)
set(FPRIME_STM32_BOOTSTRAP_POOL_SIZE 131072 CACHE INTERNAL
    "STM32H7 bootstrap pool size (bytes)" FORCE)

add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Platform")

register_fprime_config(
    PlatformStm32h7
    INTERFACE
    CHOOSES_IMPLEMENTATIONS
        Os_Cpu_Baremetal
        Os_Memory_Baremetal
        Os_Task_Stm32H7
        Os_File_Baremetal_MicroFs
        Os_Console_Stub
        Os_Mutex_Stm32H7
        Os_Queue_Stm32H7
        Os_RawTime_Stm32H7
        Os_CountingSemaphore_Stub
        Fw_StringFormat_snprintf
        Fw_StringScan_sscanf
    BASE_CONFIG
)

target_compile_definitions(PlatformStm32h7 INTERFACE
    TGT_OS_TYPE_STM32H7
)
