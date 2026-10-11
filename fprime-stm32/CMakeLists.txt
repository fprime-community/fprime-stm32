####
# fprime-stm32 is a reusable submodule: it only supplies hardware-agnostic
# OSAL delegates and drivers on top of a `FprimeStm32` target. The consuming
# project owns the actual hardware/HAL build (see
# FprimeStm32BaremetalReference/Hardware/CMakeLists.txt) so that swapping to
# different hardware never requires forking this library.
####

add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Drv/config")

####
# Fully hardware-agnostic infra every STM32 bare-metal project needs: the
# fixed-pool bootstrap allocator + GNU ld `--wrap` traps enforcing the JPL
# no-heap-after-bootstrap rule. Not gated by FPRIME_PLATFORM -- this has zero
# chip coupling. OBJECT, not STATIC: `__wrap_malloc`/`__wrap_free` are only
# ever reached through the linker's `--wrap` symbol substitution, invisible
# to CMake's dependency graph, so a STATIC archive can land before whatever
# needs it on the link line and leave the wrap symbols unresolved. An OBJECT
# library's .o files are unconditionally added to the final link instead.
####
add_library(FprimeStm32Allocator OBJECT
    "${CMAKE_CURRENT_LIST_DIR}/Allocator/BootstrapAllocator.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Allocator/MallocWrappers.cpp"
)
target_link_libraries(FprimeStm32Allocator PUBLIC Fw_Types Os_Baremetal_OverrideNewDelete)
if (DEFINED FPRIME_STM32_BOOTSTRAP_POOL_SIZE)
    target_compile_definitions(FprimeStm32Allocator PRIVATE
        FPRIME_STM32_BOOTSTRAP_POOL_SIZE=${FPRIME_STM32_BOOTSTRAP_POOL_SIZE}
    )
endif()
target_link_options(FprimeStm32Allocator INTERFACE
    "-Wl,--wrap=malloc"
    "-Wl,--wrap=calloc"
    "-Wl,--wrap=realloc"
    "-Wl,--wrap=free"
    "-Wl,--wrap=_malloc_r"
    "-Wl,--wrap=_calloc_r"
    "-Wl,--wrap=_realloc_r"
    "-Wl,--wrap=_free_r"
)

####
# Core-level (ARM architecture, not ST-family) helpers, grouped by core.
# CacheMaintenance.hpp only needs Cortex-M7's cache-maintenance intrinsics --
# valid for any Cortex-M7 part, not just STM32H7 -- so it isn't gated by
# FPRIME_PLATFORM either.
####
add_library(FprimeStm32CoreCortexM7 INTERFACE)
target_include_directories(FprimeStm32CoreCortexM7 INTERFACE "${CMAKE_CURRENT_LIST_DIR}/Core/CortexM7")

add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Os")

add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Drv")
