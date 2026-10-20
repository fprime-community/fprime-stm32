// ======================================================================
// \title  PlatformMemory.hpp
// \author ivanlara
// \brief  header file containing memory section definitions for STM32 platform
// ======================================================================

#ifndef FPRIME_STM32_PLATFORM_MEMORY_HPP
#define FPRIME_STM32_PLATFORM_MEMORY_HPP

#if defined(__GNUC__) || defined(__clang__)
#define ATTR_DTCM_BSS __attribute__((section(".dtcm_bss"), aligned(4)))
#else
#define ATTR_DTCM_BSS
#endif

#endif