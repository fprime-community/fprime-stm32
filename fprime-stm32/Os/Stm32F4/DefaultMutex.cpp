// ======================================================================
// \title Os/DefaultMutex.cpp
// \brief Select the STM32F4 Os::Mutex delegate
// ======================================================================
#include <Os/Delegate.hpp>
#include <Os/Mutex.hpp>
#include "Mutex.hpp"

namespace Os {

MutexInterface* MutexInterface::getDelegate(MutexHandleStorage& alignedNewMemory) {
    return Delegate::makeDelegate<MutexInterface, Stm32::Mutex::Stm32Mutex>(alignedNewMemory);
}

}  // namespace Os
