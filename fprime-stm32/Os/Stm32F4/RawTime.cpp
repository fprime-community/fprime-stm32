// ======================================================================
// \title Os/RawTime.cpp
// \brief STM32F4 TIM2 microsecond-resolution implementation of Os::RawTime
// ======================================================================
#include "RawTime.hpp"
#include <tim2_clock.h>

namespace Os {
namespace Stm32 {
namespace RawTime {

RawTimeHandle* Stm32RawTime::getHandle() {
    return &this->m_handle;
}

RawTimeInterface::Status Stm32RawTime::now() {
    this->m_handle.m_microseconds = Stm32_GetSystemMicroseconds();
    return Status::OP_OK;
}

RawTimeInterface::Status Stm32RawTime::getTimeInterval(const Os::RawTime& other,
                                                        Fw::TimeInterval& interval) const {
    const std::uint64_t otherMicroseconds =
        static_cast<Stm32RawTimeHandle*>(const_cast<Os::RawTime&>(other).getHandle())->m_microseconds;
    const std::uint64_t elapsedMicroseconds = this->m_handle.m_microseconds - otherMicroseconds;
    interval.set(static_cast<U32>(elapsedMicroseconds / 1000000ULL),
                 static_cast<U32>(elapsedMicroseconds % 1000000ULL));
    return Status::OP_OK;
}

Fw::SerializeStatus Stm32RawTime::serializeTo(Fw::SerialBufferBase& buffer, Fw::Endianness mode) const {
    static_assert(Stm32RawTime::SERIALIZED_SIZE >= 2 * sizeof(U32),
                  "Stm32RawTime requires at least two U32 serialization fields");
    // Split the 64-bit microsecond count into whole seconds and the remaining sub-second
    // microseconds, matching the seconds/microseconds wire format expected by F' time services.
    const U32 seconds = static_cast<U32>(this->m_handle.m_microseconds / 1000000ULL);
    const U32 microseconds = static_cast<U32>(this->m_handle.m_microseconds % 1000000ULL);
    Fw::SerializeStatus status = buffer.serializeFrom(seconds, mode);
    if (status != Fw::SerializeStatus::FW_SERIALIZE_OK) {
        return status;
    }
    return buffer.serializeFrom(microseconds, mode);
}

Fw::SerializeStatus Stm32RawTime::deserializeFrom(Fw::SerialBufferBase& buffer, Fw::Endianness mode) {
    U32 seconds = 0;
    U32 microseconds = 0;
    Fw::SerializeStatus status = buffer.deserializeTo(seconds, mode);
    if (status != Fw::SerializeStatus::FW_SERIALIZE_OK) {
        return status;
    }
    status = buffer.deserializeTo(microseconds, mode);
    if (status != Fw::SerializeStatus::FW_SERIALIZE_OK) {
        return status;
    }
    if (microseconds >= 1000000U) {
        return Fw::SerializeStatus::FW_DESERIALIZE_FORMAT_ERROR;
    }
    this->m_handle.m_microseconds = (static_cast<std::uint64_t>(seconds) * 1000000ULL) + microseconds;
    return Fw::SerializeStatus::FW_SERIALIZE_OK;
}

}  // namespace RawTime
}  // namespace Stm32
}  // namespace Os