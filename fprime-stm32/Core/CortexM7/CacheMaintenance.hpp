// ======================================================================
// \title  CacheMaintenance.hpp
// \author ivanlara
// \brief  D-cache clean/invalidate wrappers for Cortex-M7 DMA buffers
// ======================================================================
#ifndef FPRIME_STM32_CACHE_MAINTENANCE_HPP
#define FPRIME_STM32_CACHE_MAINTENANCE_HPP

#include <cstddef>
#include <cstdint>

// CubeMX generates `main.h` under this exact name for every STM32 family, and
// it chains to the family HAL/device/CMSIS-core headers with the right
// pre-defines (__FPU_PRESENT, __DCACHE_PRESENT, etc.) already set up -- unlike
// including the family-named HAL header (e.g. stm32h7xx_hal.h) directly, this
// keeps this Cortex-M7-generic helper portable across STM32 families.
#include "main.h"

#include <Fw/Types/Assert.hpp>

namespace Stm32 {

//! DTCM RAM base/size on an STM32H753 (see the project linker script,
//! e.g. STM32H753xx_FLASH.ld). The DTCM bus is not reachable by any DMA
//! controller on this chip, unlike the default AXI SRAM region -- a buffer
//! handed to a `_DMA` HAL call must never live here.
#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
constexpr std::uintptr_t DTCM_RAM_BASE = 0x20000000U;
constexpr std::uintptr_t DTCM_RAM_SIZE = 0x20000U;  // 128 KiB

//! Assert that [addr, addr + size) does not overlap DTCM RAM, i.e. it is
//! safe to hand to a DMA-backed HAL call. This is a programmer invariant
//! (the caller's buffer placement, not externally supplied data), so a
//! violation is a build/configuration bug, not a runtime input to guard
//! against gracefully.
inline void AssertDmaSafe(const void* addr, std::size_t size) {
    FW_ASSERT(addr != nullptr);
    const auto start = reinterpret_cast<std::uintptr_t>(addr);
    const auto end = start + size;
    const bool overlapsDtcm = (start < DTCM_RAM_BASE + DTCM_RAM_SIZE) && (end > DTCM_RAM_BASE);
    FW_ASSERT(!overlapsDtcm, static_cast<FwAssertArgType>(start));
}

//! Clean D-cache over [addr, addr + size) before starting a memory-to-peripheral
//! DMA transfer, so the DMA controller reads data the CPU has actually written.
//! addr/size must be 32-byte aligned/sized to avoid touching unrelated data,
//! since the cache line granularity is fixed at 32 bytes on the Cortex-M7.
inline void CleanDCacheForDma(const void* addr, std::size_t size) {
    SCB_CleanDCache_by_Addr(const_cast<uint32_t*>(reinterpret_cast<const uint32_t*>(addr)),
                            static_cast<int32_t>(size));
}

//! Invalidate D-cache over [addr, addr + size) after a peripheral-to-memory
//! DMA transfer completes, so the CPU reads data the DMA controller actually
//! wrote instead of a stale cache line. Same alignment requirement as above.
inline void InvalidateDCacheForDma(void* addr, std::size_t size) {
    SCB_InvalidateDCache_by_Addr(reinterpret_cast<uint32_t*>(addr), static_cast<int32_t>(size));
}
#else
inline void AssertDmaSafe(const void* addr, std::size_t size) {
    (void)size;
    FW_ASSERT(addr != nullptr);
}

inline void CleanDCacheForDma(const void* addr, std::size_t size) {
    (void)addr;
    (void)size;
}

inline void InvalidateDCacheForDma(void* addr, std::size_t size) {
    (void)addr;
    (void)size;
}
#endif

}  // namespace Stm32

#endif
