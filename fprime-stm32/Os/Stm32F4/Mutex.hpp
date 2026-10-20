// ======================================================================
// \title Os/Mutex.hpp
// \brief Cortex-M4 critical-section implementation of Os::Mutex
// ======================================================================
#ifndef FPRIME_STM32_OS_MUTEX_HPP
#define FPRIME_STM32_OS_MUTEX_HPP

#include <Os/Mutex.hpp>

namespace Os {
namespace Stm32 {
namespace Mutex {

struct Stm32MutexHandle : public MutexHandle {
    U32 m_primask = 0;
    bool m_taken = false;
};

class Stm32Mutex final : public MutexInterface {
  public:
    Stm32Mutex() = default;
    ~Stm32Mutex() override = default;

    MutexHandle* getHandle() override;
    Status take() override;
    Status release() override;

  private:
    Stm32MutexHandle m_handle;
};

}  // namespace Mutex
}  // namespace Stm32
}  // namespace Os

#endif  // FPRIME_STM32_OS_MUTEX_HPP