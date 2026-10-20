// ======================================================================
// \title  Os/Queue.hpp
// \brief  Single-threaded, interrupt-safe FIFO implementation of Os::Queue
//
// This bare-metal cyclic executive has no RTOS scheduler and therefore no
// blocking primitive to wait on. Every queue is a fixed-depth ring buffer
// whose backing storage is allocated once (via `new`, which is routed
// through `Os::Baremetal::OverrideNewDelete` to the deployment's bootstrap
// static pool -- see fprime-stm32/Allocator/BootstrapAllocator.cpp) during
// `create()`, which only ever runs during `setupTopology()`, i.e. before
// `Stm32::lockBootstrapAllocator()` is called. No memory is
// ever allocated or freed once the cyclic loop starts.
//
// `BlockingType::BLOCKING` degrades to an immediate FULL/EMPTY return
// since there is no other task to yield to; callers already tolerate this
// per F' convention (active component message loops treat FULL on send as
// an error to report, and this queue is never expected to run at depth in
// a properly-sized deployment).
//
// Message priority is not used to reorder delivery: messages are returned
// strictly in FIFO (send) order regardless of `priority`, though the
// priority value supplied at `send()` is preserved and returned by
// `receive()`. This matches the needs of a cooperative, single-threaded
// dispatcher where all messages in a given queue are drained every cycle.
// ======================================================================
#ifndef FPRIME_STM32_OS_QUEUE_HPP
#define FPRIME_STM32_OS_QUEUE_HPP

#include <Os/Queue.hpp>

namespace Os {
namespace Stm32 {
namespace Queue {

struct Stm32QueueHandle : public QueueHandle {};

class Stm32Queue final : public QueueInterface {
  public:
    Stm32Queue() = default;
    ~Stm32Queue() override = default;

    Status create(FwEnumStoreType id,
                  const Fw::ConstStringBase& name,
                  FwSizeType depth,
                  FwSizeType messageSize) override;

    void teardown() override;

    Status send(const U8* buffer, FwSizeType size, FwQueuePriorityType priority, BlockingType blockType) override;

    Status receive(U8* destination,
                   FwSizeType capacity,
                   BlockingType blockType,
                   FwSizeType& actualSize,
                   FwQueuePriorityType& priority) override;

    FwSizeType getMessagesAvailable() const override;
    FwSizeType getMessageHighWaterMark() const override;
    QueueHandle* getHandle() override;

  private:
    U8* m_payload = nullptr;              //!< depth * messageSize byte ring buffer, from bootstrap pool
    FwSizeType* m_sizes = nullptr;        //!< actual size stored in each slot
    FwQueuePriorityType* m_priorities = nullptr;  //!< priority stored in each slot
    FwSizeType m_depth = 0;               //!< number of slots
    FwSizeType m_messageSize = 0;         //!< maximum message size (bytes per slot)
    FwSizeType m_head = 0;                //!< next slot to write (send)
    FwSizeType m_tail = 0;                //!< next slot to read (receive)
    FwSizeType m_count = 0;               //!< number of messages currently stored
    FwSizeType m_highWaterMark = 0;       //!< maximum m_count ever observed
    Stm32QueueHandle m_handle;
};

}  // namespace Queue
}  // namespace Stm32
}  // namespace Os

#endif  // FPRIME_STM32_OS_QUEUE_HPP
