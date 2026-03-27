#ifndef SWAM_PROBE_H
#define SWAM_PROBE_H

#include "binaryen-c.h"
#include "swam_scenarios.h"

#ifdef __cplusplus
extern "C" {
#endif

void swam_add_marker_globals(BinaryenModuleRef mod);

BinaryenExpressionRef swam_build_probe(BinaryenModuleRef mod,
                                       const SwamScenario* scenario,
                                       BinaryenIndex addr_local_idx,
                                       const char* mem_name,
                                       bool is_memory64);

#ifdef __cplusplus
}
#endif

#endif  // SWAM_PROBE_H
