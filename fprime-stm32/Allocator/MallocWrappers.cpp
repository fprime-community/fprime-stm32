// ======================================================================
// \title  MallocWrappers.cpp
// \brief  GNU ld `--wrap` traps for the C heap allocation family
//
// The linker is configured (see CMakeLists.txt) with:
//   -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc -Wl,--wrap=free
//
// This redirects every call to malloc/calloc/realloc/free in the final
// image -- including calls made from newlib internals and any third-party
// code -- to the __wrap_* definitions below instead of the real libc
// implementations (still reachable as __real_*). Per JPL flight software
// rules, no dynamic C heap allocation is permitted once the flight
// software is running; every call reaching here after bootstrap is
// therefore a bring-up bug and must halt execution immediately via
// FW_ASSERT rather than silently allocating or returning nullptr.
//
// One legitimate exception exists: libstdc++'s libsupc++ (specifically
// eh_alloc.cc's __cxa_allocate_exception() emergency pool) calls raw
// C malloc() -- not operator new -- from a global static constructor
// that runs during __libc_init_array(), i.e. strictly *before* main()
// and therefore before Stm32::lockBootstrapAllocator() is
// called. This is a one-time, bounded, toolchain-internal allocation
// (see Stm32::getBootstrapAllocator()), not an
// application-level dynamic allocation, so __wrap_malloc forwards it to
// the same StrictStaticAllocator bootstrap pool already used by
// OverrideNewDelete for operator new. Once the pool is locked (right
// before the cyclic loop starts), any further malloc() call -- from
// application code or otherwise -- still hits FW_ASSERT below via the
// allocator's own "!m_locked" assertion.
// ======================================================================
#include <fprime-stm32/Allocator/BootstrapAllocator.hpp>

#include <Fw/Types/Assert.hpp>
#include <Fw/Types/MemAllocator.hpp>

#include <cstddef>
#include <reent.h>

extern "C" {

void* __wrap_malloc(std::size_t size) {
    FW_ASSERT(size > 0, static_cast<FwAssertArgType>(size));
    FwSizeType allocSize = static_cast<FwSizeType>(size);
    bool recoverable = false;
    // Forwards to the bootstrap allocator; this itself FW_ASSERTs if the
    // pool is locked, exhausted, or misaligned -- see BootstrapAllocator.cpp.
    return Stm32::getBootstrapAllocator().allocate(0, allocSize, recoverable);
}

void* __wrap_calloc(std::size_t num, std::size_t size) {
    FW_ASSERT(0, static_cast<FwAssertArgType>(num), static_cast<FwAssertArgType>(size));
    return nullptr;
}

void* __wrap_realloc(void* ptr, std::size_t new_size) {
    FW_ASSERT(0, static_cast<FwAssertArgType>(reinterpret_cast<PlatformPointerCastType>(ptr)),
              static_cast<FwAssertArgType>(new_size));
    return nullptr;
}

void __wrap_free(void* ptr) {
    FW_ASSERT(0, static_cast<FwAssertArgType>(reinterpret_cast<PlatformPointerCastType>(ptr)));
}

void* __wrap__malloc_r(struct _reent* reent, std::size_t size) {
    (void)reent;
    return __wrap_malloc(size);
}

void* __wrap__calloc_r(struct _reent* reent, std::size_t num, std::size_t size) {
    (void)reent;
    return __wrap_calloc(num, size);
}

void* __wrap__realloc_r(struct _reent* reent, void* ptr, std::size_t newSize) {
    (void)reent;
    return __wrap_realloc(ptr, newSize);
}

void __wrap__free_r(struct _reent* reent, void* ptr) {
    (void)reent;
    __wrap_free(ptr);
}

}  // extern "C"
