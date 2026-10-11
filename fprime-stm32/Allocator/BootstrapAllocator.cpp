// ======================================================================
// \title  BootstrapAllocator.cpp
// \brief  Fixed-pool allocator used during deployment initialization
// ======================================================================
#include <fprime-stm32/Allocator/BootstrapAllocator.hpp>

#include <Fw/Types/Assert.hpp>
#include <fprime-baremetal/Os/OverrideNewDelete/OverrideNewDelete.hpp>

#include <cstddef>
#include <cstdint>
#include <new>

namespace {

class StrictStaticAllocator final : public Fw::MemAllocator {
  public:
    StrictStaticAllocator(void* pool, const FwSizeType poolSize)
        : m_poolStart(reinterpret_cast<std::uintptr_t>(pool)),
          m_poolEnd(m_poolStart + poolSize),
          m_next(m_poolStart),
          m_locked(false) {}

    void* allocate(const FwEnumStoreType identifier,
                   FwSizeType& size,
                   bool& recoverable,
                   const FwSizeType alignment = alignof(std::max_align_t)) override {
        (void)identifier;
        recoverable = false;

        FW_ASSERT(!m_locked);
        FW_ASSERT(size > 0);
        FW_ASSERT(alignment > 0);
        FW_ASSERT((alignment & (alignment - 1U)) == 0);

        const std::uintptr_t alignedAddress =
            (m_next + alignment - 1U) & ~(static_cast<std::uintptr_t>(alignment) - 1U);
        const std::uintptr_t allocationEnd = alignedAddress + size;

        FW_ASSERT(allocationEnd >= alignedAddress);
        FW_ASSERT(allocationEnd <= m_poolEnd);

        m_next = allocationEnd;
        return reinterpret_cast<void*>(alignedAddress);
    }

    void deallocate(const FwEnumStoreType identifier, void* ptr) override {
        (void)identifier;
        FW_ASSERT(ptr != nullptr);
    }

    void lock() {
        m_locked = true;
    }

  private:
    std::uintptr_t m_poolStart;
    std::uintptr_t m_poolEnd;
    std::uintptr_t m_next;
    bool m_locked;
};

#ifndef FPRIME_STM32_BOOTSTRAP_POOL_SIZE
#define FPRIME_STM32_BOOTSTRAP_POOL_SIZE (128U * 1024U)
#endif

constexpr FwSizeType STATIC_HEAP_POOL_SIZE = FPRIME_STM32_BOOTSTRAP_POOL_SIZE;

alignas(std::max_align_t) U8 staticHeapPool[STATIC_HEAP_POOL_SIZE];
alignas(StrictStaticAllocator) U8 staticAllocatorStorage[sizeof(StrictStaticAllocator)];
StrictStaticAllocator* staticAllocator = nullptr;

}  // namespace

extern "C" void Stm32_registerBootstrapAllocator() {
    FW_ASSERT(staticAllocator == nullptr);
    staticAllocator = new (staticAllocatorStorage) StrictStaticAllocator(staticHeapPool, sizeof(staticHeapPool));
    (void)Os::Baremetal::OverrideNewDelete::registerMemAllocator(staticAllocator);
}

namespace Stm32 {

Fw::MemAllocator& getBootstrapAllocator() {
    FW_ASSERT(staticAllocator != nullptr);
    return *staticAllocator;
}

void lockBootstrapAllocator() {
    FW_ASSERT(staticAllocator != nullptr);
    staticAllocator->lock();
}

}  // namespace Stm32
