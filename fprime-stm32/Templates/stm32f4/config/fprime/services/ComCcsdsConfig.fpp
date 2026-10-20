module ComCcsdsConfig {
    constant BASE_ID = 0x02000000

    module QueueSizes {
        constant comQueue    = 16
        constant aggregator  = 4
    }

    module StackSizes {
        constant comQueue   = 1024
        constant aggregator = 1024
    }

    module Priorities {
        constant aggregator = 30
        constant comQueue   = 29
    }

    module CpuAffinities {
        constant aggregator = Os.TASK_DEFAULT
        constant comQueue   = Os.TASK_DEFAULT
    }

    module QueueDepths {
        constant events = 8
        constant tlm    = 8
        constant file   = 4
    }

    module QueuePriorities {
        constant events = 0
        constant tlm    = 2
        constant file   = 1
    }

    module Aggregator {
        constant aggregationSize = Svc.Ccsds.TmDataFieldSize
        constant enablePacketSpanning = false
    }

    module BuffMgr {
        constant frameAccumulatorSize = 1024
        constant commsBuffSize        = 256
        constant commsFileBuffSize    = 512
        constant commsBuffCount       = 4
        constant commsFileBuffCount   = 2
        constant commsBuffMgrId       = 200
    }
}
