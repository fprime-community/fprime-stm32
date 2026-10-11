module DataProductsConfig {
    constant BASE_ID = 0x04000000

    module QueueSizes {
        constant dpCat                  = 4
        constant dpMgr                  = 4
        constant dpWriter               = 4
        constant dpBufferAccumulator   = 4
        constant dpBufferManager       = 4
    }

    module StackSizes {
        constant dpCat                = 1024
        constant dpMgr                = 1024
        constant dpWriter             = 1024
        constant dpBufferAccumulator  = 1024
        constant dpBufferManager      = 1024
    }

    module Priorities {
        constant dpCat                = 24
        constant dpMgr                = 23
        constant dpWriter             = 22
        constant dpBufferAccumulator  = 21
        constant dpBufferManager      = 21
    }

    module CpuAffinities {
        constant dpCat                = Os.TASK_DEFAULT
        constant dpMgr                = Os.TASK_DEFAULT
        constant dpWriter             = Os.TASK_DEFAULT
        constant dpBufferAccumulator  = Os.TASK_DEFAULT
    }

    module BufferAccumulator {
        constant allocatorId   = 301
        constant maxNumBuffers = 2
    }

    module BuffMgr {
        constant dpBufferStoreSize  = 1024
        constant dpBufferStoreCount = 2
        constant dpBufferManagerId  = 300
    }

    module Paths {
        constant dpDir   = "./DpCat"
        constant dpState = "./DpCat/DpState.dat"
    }
}
