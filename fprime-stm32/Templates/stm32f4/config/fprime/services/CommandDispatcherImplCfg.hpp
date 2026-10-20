/**
 * \file
 * \brief Project override for Svc::CommandDispatcher table sizing
 *
 * Size the dispatch table to the core deployment's current 17 command opcodes.
 * Update this value when commands are added.
 */

#ifndef CMDDISPATCHER_COMMANDDISPATCHERIMPLCFG_HPP_
#define CMDDISPATCHER_COMMANDDISPATCHERIMPLCFG_HPP_

#include <Fw/FPrimeBasicTypes.hpp>

enum {
    // Must be >= number of opcodes registered in the topology (17 today).
    // Seven spare entries allow limited command growth.
    CMD_DISPATCHER_DISPATCH_TABLE_SIZE = 24,  // !< The size of the table holding opcodes to dispatch
    // Number of commands that may be in-progress (awaiting a response)
    // simultaneously. Five sequence ports are configured; eight entries
    // allow limited overlap without the framework's larger default table.
    CMD_DISPATCHER_SEQUENCER_TABLE_SIZE = 8,  // !< The size of the table holding commands in progress
};

namespace Svc {
namespace CmdDispatcherCfg {

constexpr bool EXECUTE_WHEN_SEQUENCE_TABLE_FULL_DEFAULT = false;

//! Include command opcodes in events when true.
//! When false, opcode fields are set to the maximum FwOpcodeType value.
constexpr bool IncludeCommandOpcodesInEvents = true;

constexpr FwOpcodeType getEventOpcode(const FwOpcodeType opcode) {
    return IncludeCommandOpcodesInEvents ? opcode : std::numeric_limits<FwOpcodeType>::max();
}

}  // namespace CmdDispatcherCfg
}  // namespace Svc

#endif /* CMDDISPATCHER_COMMANDDISPATCHERIMPLCFG_HPP_ */
