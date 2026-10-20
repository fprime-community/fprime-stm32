// ======================================================================
// \title Os/Task.cpp
// \brief Cooperative STM32F4 implementation of Os::Task
// ======================================================================
#include <Fw/Types/Assert.hpp>
#include <Os/Task.hpp>
#include "Task.hpp"

namespace Os {
namespace Stm32 {
namespace Task {

void Stm32Task::onStart() {}

TaskInterface::Status Stm32Task::join() {
    return Status::OP_OK;
}

void Stm32Task::suspend(SuspensionType suspensionType) {
    (void)suspensionType;
    this->m_handle.m_enabled = false;
}

void Stm32Task::resume() {
    this->m_handle.m_enabled = true;
}

TaskInterface::Status Stm32Task::_delay(const Fw::TimeInterval& interval) {
    (void)interval;
    return Status::NOT_SUPPORTED;
}

bool Stm32Task::isCooperative() {
    return true;
}

TaskHandle* Stm32Task::getHandle() {
    return &this->m_handle;
}

TaskInterface::Status Stm32Task::start(const Arguments& arguments) {
    FW_ASSERT(arguments.m_routine != nullptr);
    this->m_handle.m_routine = arguments.m_routine;
    this->m_handle.m_argument = arguments.m_routine_argument;
    this->m_handle.m_enabled = true;
    this->m_handle.m_routine(this->m_handle.m_argument);
    return Status::OP_OK;
}

}  // namespace Task
}  // namespace Stm32
}  // namespace Os