// ======================================================================
// \title Os/RawTime.hpp
// \brief STM32F4 TIM2 microsecond-resolution implementation of Os::RawTime
// ======================================================================
#ifndef FPRIME_STM32_OS_RAWTIME_HPP
#define FPRIME_STM32_OS_RAWTIME_HPP

#include <Os/RawTime.hpp>
#include <cstdint>

namespace Os {
namespace Stm32 {
namespace RawTime {

struct Stm32RawTimeHandle : public RawTimeHandle {
    //! Microseconds elapsed since Stm32_Tim2ClockInit() was called, assembled from the free-running
    //! TIM2 counter (1 MHz) and its 32-bit overflow counter. See tim2_clock.h.
    std::uint64_t m_microseconds = 0;
};

class Stm32RawTime final : public RawTimeInterface {
  public:
    Stm32RawTime() = default;
    ~Stm32RawTime() override = default;

    RawTimeHandle* getHandle() override;
    Status now() override;
    Status getTimeInterval(const Os::RawTime& other, Fw::TimeInterval& interval) const override;
    Fw::SerializeStatus serializeTo(Fw::SerialBufferBase& buffer,
                                    Fw::Endianness mode = Fw::Endianness::BIG) const override;
    Fw::SerializeStatus deserializeFrom(Fw::SerialBufferBase& buffer,
                                        Fw::Endianness mode = Fw::Endianness::BIG) override;

  private:
    Stm32RawTimeHandle m_handle;
};

}  // namespace RawTime
}  // namespace Stm32
}  // namespace Os

#endif  // FPRIME_STM32_OS_RAWTIME_HPP
