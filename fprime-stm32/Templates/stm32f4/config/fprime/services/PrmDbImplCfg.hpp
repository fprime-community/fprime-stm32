/*
 * PrmDbImplCfg.hpp
 *
 * Project override for Svc::PrmDb sizing.
 *
 * This deployment currently declares zero `param` entries across its
 * topology, so retain only the minimum non-zero database capacity. Increase
 * this when parameters are added.
 */

#ifndef PRMDB_PRMDBLIMPLCFG_HPP_
#define PRMDB_PRMDBLIMPLCFG_HPP_

// Anonymous namespace for configuration parameters
namespace {

enum {
    PRMDB_NUM_DB_ENTRIES = 1,     // !< Number of entries in the parameter database (none currently configured)
    PRMDB_ENTRY_DELIMITER = 0xA5  // !< Byte value that should precede each parameter in file; sanity check against
                                  // file integrity. Should match ground system.
};

}

#endif /* PRMDB_PRMDBLIMPLCFG_HPP_ */
