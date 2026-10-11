####
# Project configuration
#
# Keep the framework defaults as the base configuration and layer the
# bare-metal library configuration on top of them.
####

# Project-level configuration overrides must be processed after the framework
# and bare-metal base configurations so they replace the default files.
register_fprime_config(
    ReferenceDeploymentConfig
    CONFIGURATION_OVERRIDES
        "${CMAKE_CURRENT_LIST_DIR}/fprime/FpConfig.h"
        "${CMAKE_CURRENT_LIST_DIR}/fprime/FpConstants.fpp"
        "${CMAKE_CURRENT_LIST_DIR}/fprime/AcConstants.fpp"
        "${CMAKE_CURRENT_LIST_DIR}/fprime/services/ComCcsdsConfig.fpp"
        "${CMAKE_CURRENT_LIST_DIR}/fprime/dataproducts/DataProductsConfig.fpp"
        "${CMAKE_CURRENT_LIST_DIR}/fprime/services/TlmChanImplCfg.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/fprime/services/CommandDispatcherImplCfg.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/fprime/services/PrmDbImplCfg.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/fprime/dataproducts/DpCatalogCfg.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/fprime-stm32/Stm32Config.hpp"
    INTERFACE
)
