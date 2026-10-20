/*
 * DpCatalogCfg.hpp
 *
 * Project override for Svc::DpCatalog sizing.
 *
 * The framework default (127 tracked files) vastly exceeds what the
 * bare-metal `Os::Baremetal::MicroFs` RAM filesystem can hold in this
 * deployment (currently three MicroFs files). Keep one additional catalog
 * slot for growth without reserving the framework default of 127 entries.
 */

#ifndef SVC_DPCATALOG_CONFIG_HPP_
#define SVC_DPCATALOG_CONFIG_HPP_
#include <Fw/FPrimeBasicTypes.hpp>

namespace Svc {
// Sets the maximum number of directories where
// data products can be stored. The array passed
// to the initializer for DpCatalog cannot exceed
// this size.
static const FwIndexType DP_MAX_DIRECTORIES = 2;
static const FwIndexType DP_MAX_FILES = 4;
}  // namespace Svc

#endif /* SVC_DPCATALOG_CONFIG_HPP_ */
