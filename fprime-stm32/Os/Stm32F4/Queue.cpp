// ======================================================================
// \title  Os/Queue.cpp
// \brief  Single-threaded, interrupt-safe FIFO implementation of Os::Queue
// ======================================================================
#include "Queue.hpp"
#include <Fw/Types/Assert.hpp>
// main.h chains to the family CMSIS-core header with the right pre-defines
// already set up (__FPU_PRESENT, etc.) -- keeps this file portable across
// STM32 families instead of naming a vendor device header directly.
#include <main.h>

#include <cstring>

namespace Os {
namespace Stm32 {
namespace Queue {

QueueInterface::Status Stm32Queue::create(FwEnumStoreType id,
                                          const Fw::ConstStringBase& name,
                                          FwSizeType depth,
                                          FwSizeType messageSize) {
    (void)id;
    (void)name;

    if (this->m_payload != nullptr) {
        return Status::ALREADY_CREATED;
    }
    FW_ASSERT(depth > 0, static_cast<FwAssertArgType>(depth));
    FW_ASSERT(messageSize > 0, static_cast<FwAssertArgType>(messageSize));

    // These `new[]` calls are routed through Os::Baremetal::OverrideNewDelete to the
    // deployment's bootstrap static pool. They only ever run during setupTopology(),
    // before the pool is locked -- see the class-level comment in Queue.hpp.
    this->m_payload = new U8[depth * messageSize];
    this->m_sizes = new FwSizeType[depth];
    this->m_priorities = new FwQueuePriorityType[depth];

    this->m_depth = depth;
    this->m_messageSize = messageSize;
    this->m_head = 0;
    this->m_tail = 0;
    this->m_count = 0;
    this->m_highWaterMark = 0;
    return Status::OP_OK;
}

void Stm32Queue::teardown() {
    // No-op: queues in this bare-metal cyclic executive live for the entire
    // program and their storage was allocated from a bump allocator that
    // never reclaims memory, so there is nothing to release here.
}

QueueInterface::Status Stm32Queue::send(const U8* buffer,
                                        FwSizeType size,
                                        FwQueuePriorityType priority,
                                        QueueInterface::BlockingType blockType) {
    (void)blockType;  // No scheduler to block on; see class-level comment in Queue.hpp
    FW_ASSERT(buffer != nullptr);
    if (this->m_payload == nullptr) {
        return Status::UNINITIALIZED;
    }
    if (size > this->m_messageSize) {
        return Status::SIZE_MISMATCH;
    }

    const U32 primask = __get_PRIMASK();
    __disable_irq();
    if (this->m_count >= this->m_depth) {
        __set_PRIMASK(primask);
        return Status::FULL;
    }

    const FwSizeType slot = this->m_head;
    (void)memcpy(&this->m_payload[slot * this->m_messageSize], buffer, size);
    this->m_sizes[slot] = size;
    this->m_priorities[slot] = priority;
    this->m_head = (slot + 1U) % this->m_depth;
    this->m_count++;
    if (this->m_count > this->m_highWaterMark) {
        this->m_highWaterMark = this->m_count;
    }
    __set_PRIMASK(primask);
    return Status::OP_OK;
}

QueueInterface::Status Stm32Queue::receive(U8* destination,
                                           FwSizeType capacity,
                                           QueueInterface::BlockingType blockType,
                                           FwSizeType& actualSize,
                                           FwQueuePriorityType& priority) {
    (void)blockType;  // No scheduler to block on; see class-level comment in Queue.hpp
    FW_ASSERT(destination != nullptr);
    if (this->m_payload == nullptr) {
        return Status::UNINITIALIZED;
    }

    const U32 primask = __get_PRIMASK();
    __disable_irq();
    if (this->m_count == 0) {
        __set_PRIMASK(primask);
        return Status::EMPTY;
    }

    const FwSizeType slot = this->m_tail;
    const FwSizeType msgSize = this->m_sizes[slot];
    if (msgSize > capacity) {
        __set_PRIMASK(primask);
        return Status::SIZE_MISMATCH;
    }
    (void)memcpy(destination, &this->m_payload[slot * this->m_messageSize], msgSize);
    actualSize = msgSize;
    priority = this->m_priorities[slot];
    this->m_tail = (slot + 1U) % this->m_depth;
    this->m_count--;
    __set_PRIMASK(primask);
    return Status::OP_OK;
}

FwSizeType Stm32Queue::getMessagesAvailable() const {
    return this->m_count;
}

FwSizeType Stm32Queue::getMessageHighWaterMark() const {
    return this->m_highWaterMark;
}

QueueHandle* Stm32Queue::getHandle() {
    return &this->m_handle;
}

}  // namespace Queue
}  // namespace Stm32
}  // namespace Os
