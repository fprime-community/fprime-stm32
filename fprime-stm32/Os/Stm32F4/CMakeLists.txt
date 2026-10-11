
####
# STM32F4 bare-metal OSAL implementations. OSAL backends are family-specific
# and mutually exclusive, so this subdirectory is only ever relevant on the
# stm32F4 platform.
####
restrict_platforms(stm32f4)

register_os_implementation(Task Stm32F4 FprimeStm32 fprime-baremetal_Os_TaskRunner)
register_os_implementation(Mutex Stm32F4 FprimeStm32)
register_os_implementation(RawTime Stm32F4 FprimeStm32)
register_os_implementation(Queue Stm32F4 FprimeStm32)
