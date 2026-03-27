#ifndef SWAM_PROFILE_H
#define SWAM_PROFILE_H

#include <stdbool.h>
#include <stdint.h>

#include "swam_scenarios.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Wasm proposal bitmask. */
typedef enum {
    SWAM_PROPOSAL_MVP = 1U << 0,       /* core spec */
    SWAM_PROPOSAL_THREADS = 1U << 1,   /* atomics, shared memory */
    SWAM_PROPOSAL_SIMD = 1U << 2,      /* SIMD v128 */
    SWAM_PROPOSAL_BULK = 1U << 3,      /* bulk memory operations */
    SWAM_PROPOSAL_MEMORY64 = 1U << 4,  /* 64-bit memory */
    SWAM_PROPOSAL_MULTI_MEM = 1U << 5, /* multiple memories */
} SwamProposal;

/* Marker IPC mechanism (informational). */
typedef enum {
    SWAM_IPC_SHM,
    SWAM_IPC_FILE,
} SwamIpcKind;

/* Runtime capability profile. */
typedef struct {
    const char *name;       /* e.g. "wasm3" */
    uint32_t proposals;     /* OR of SwamProposal values */
    const SwamOpClass *excluded_opclasses; /* runtime-specific unsupported op classes */
    uint32_t excluded_opclass_count;
    SwamIpcKind marker_ipc; /* how markers are communicated */
    const char *cli_notes;  /* human-readable CLI reference */
} SwamRuntimeProfile;

/* All proposals enabled (used as default when no profile is set). */
#define SWAM_PROPOSALS_ALL 0xFFFFFFFFU

/* Look up a runtime profile by name. Returns NULL if not found.
   The special name "all" returns NULL (caller should use SWAM_PROPOSALS_ALL). */
const SwamRuntimeProfile *swam_profile_lookup(const char *name);

/* Derive the required proposal bitmask for a given op_class.
   Returns SWAM_PROPOSAL_MVP for core ops, SWAM_PROPOSAL_THREADS for atomic ops,
   SWAM_PROPOSAL_SIMD for SIMD ops, SWAM_PROPOSAL_BULK for bulk ops. */
uint32_t swam_opclass_required_proposal(SwamOpClass op_class);

/* Check whether a runtime profile supports an op class after applying both
   proposal bits and any runtime-specific exclusions. */
bool swam_profile_supports_opclass(const SwamRuntimeProfile *profile,
                                   SwamOpClass op_class);

#ifdef __cplusplus
}
#endif

#endif /* SWAM_PROFILE_H */
