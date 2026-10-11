// ======================================================================
// \title Os/Task.hpp
// \brief Cooperative STM32F4 implementation of Os::Task
// ======================================================================
#ifndef FPRIME_STM32_OS_TASK_HPP
#define FPRIME_STM32_OS_TASK_HPP

#include <Os/Task.hpp>
#include <fprime-baremetal/Os/Baremetal/Task.hpp>

namespace Os {
namespace Stm32 {
namespace Task {

using Stm32TaskHandle = Os::Baremetal::BaremetalTaskHandle;

class Stm32Task final : public TaskInterface {
  public:
    Stm32Task() = default;
    ~Stm32Task() override = default;

    void onStart() override;
    Status join() override;
    void suspend(SuspensionType suspensionType) override;
    void resume() override;
    Status _delay(const Fw::TimeInterval& interval) override;
    bool isCooperative() override;
    TaskHandle* getHandle() override;
    Status start(const Arguments& arguments) override;

  private:
    Stm32TaskHandle m_handle;
};

}  // namespace Task
}  // namespace Stm32
}  // namespace Os

#endif  // FPRIME_STM32_OS_TASK_HPP