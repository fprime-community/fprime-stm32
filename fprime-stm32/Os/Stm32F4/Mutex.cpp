// ======================================================================
// \title Os/Mutex.cpp
// \brief Cortex-M4 critical-section implementation of Os::Mutex
// ======================================================================
#include "Mutex.hpp"
// main.h chains to the family CMSIS-core header with the right pre-defines
// already set up (__FPU_PRESENT, etc.) -- keeps this file portable across
// STM32 families instead of naming a vendor device header directly.
#include <main.h>

namespace Os {
namespace Stm32 {
namespace Mutex {

MutexHandle* Stm32Mutex::getHandle() {
    return &this->m_handle;
}

MutexInterface::Status Stm32Mutex::take() {
    const U32 primask = __get_PRIMASK();
    __disable_irq();
    if (this->m_handle.m_taken) {
        __set_PRIMASK(primask);
        return Status::ERROR_BUSY;
    }

    this->m_handle.m_primask = primask;
    this->m_handle.m_taken = true;
    return Status::OP_OK;
}

MutexInterface::Status Stm32Mutex::release() {
    if (!this->m_handle.m_taken) {
        return Status::ERROR_OTHER;
    }

    const U32 primask = this->m_handle.m_primask;
    this->m_handle.m_taken = false;
    __set_PRIMASK(primask);
    return Status::OP_OK;
}

}  // namespace Mutex
}  // namespace Stm32
}  // namespace Os