// ======================================================================
// \title Os/DefaultTask.cpp
// \brief Select the STM32F4 cooperative Os::Task delegate
// ======================================================================
#include <Os/Delegate.hpp>
#include <Os/Task.hpp>
#include "Task.hpp"

namespace Os {

TaskInterface* TaskInterface::getDelegate(TaskHandleStorage& alignedNewMemory) {
    return Delegate::makeDelegate<TaskInterface, Stm32::Task::Stm32Task>(alignedNewMemory);
}

}  // namespace Os
