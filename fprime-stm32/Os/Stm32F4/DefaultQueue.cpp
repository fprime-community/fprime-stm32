// ======================================================================
// \title  Os/DefaultQueue.cpp
// \brief  Select the STM32F4 Os::Queue delegate
// ======================================================================
#include <Os/Delegate.hpp>
#include <Os/Queue.hpp>
#include "Queue.hpp"

namespace Os {

QueueInterface* QueueInterface::getDelegate(QueueHandleStorage& aligned_placement_new_memory) {
    return Delegate::makeDelegate<QueueInterface, Stm32::Queue::Stm32Queue>(aligned_placement_new_memory);
}

}  // namespace Os
