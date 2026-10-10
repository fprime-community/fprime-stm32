// ======================================================================
// \title Os/DefaultRawTime.cpp
// \brief Select the STM32F4 TIM2 Os::RawTime delegate
// ======================================================================
#include <config/RawTimeSource.hpp>
#include <Os/Delegate.hpp>
#include <Os/RawTime.hpp>
#include "RawTime.hpp"

namespace Os {

RawTimeInterface* RawTimeInterface::getDelegate(RawTimeHandleStorage& alignedNewMemory,
                                                const RawTimeInterface* toCopy,
                                                RawTimeSource source) {
    (void)source;
    return Delegate::makeDelegate<RawTimeInterface, Stm32::RawTime::Stm32RawTime, RawTimeHandleStorage>(
        alignedNewMemory, toCopy);
}

}  // namespace Os
