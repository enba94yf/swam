#include "swam_profile.h"

#include <string.h>

/* Built-in runtime profiles.
   Sources: official documentation + empirical testing.

   | Runtime      | MVP | THREADS | SIMD | BULK | MEM64 | MULTI_MEM | IPC  |
   |--------------|-----|---------|------|------|-------|-----------|------|
   | wasm3        |  Y  |    N    |  N   |  Y   |   N   |     N     | shm  |
   | wamr         |  Y  |    Y    |  Y   |  Y   |   Y   |     N     | shm  |
   | wasmtime     |  Y  |    Y    |  Y   |  Y   |   Y   |     Y     | shm  |
   | wasmer       |  Y  |    Y    |  Y   |  Y   |   N   |     N     | shm  |
   | wasm-interp  |  Y  |    Y    |  Y   |  Y   |   Y   |     Y     | shm  |
   | v8           |  Y  |    Y    |  Y   |  Y   |   Y   |     Y     | file |
   | jsc          |  Y  |    Y    |  Y   |  Y   |   N   |     N     | file |
   | spidermonkey |  Y  |    Y    |  Y   |  Y   |   N   |     N     | file |
*/

static const SwamOpClass wasm3_excluded_opclasses[] = {
    SWAM_OPCLASS_BULK_INIT,
};

static const SwamRuntimeProfile swam_profiles[] = {
    {
        "wasm3",
        SWAM_PROPOSAL_MVP | SWAM_PROPOSAL_BULK,
        wasm3_excluded_opclasses,
        (uint32_t)(sizeof(wasm3_excluded_opclasses) /
                   sizeof(wasm3_excluded_opclasses[0])),
        SWAM_IPC_SHM,
        "--func main @@",
    },
    {
        "wamr",
        SWAM_PROPOSAL_MVP | SWAM_PROPOSAL_THREADS | SWAM_PROPOSAL_SIMD |
            SWAM_PROPOSAL_BULK | SWAM_PROPOSAL_MEMORY64,
        NULL,
        0U,
        SWAM_IPC_SHM,
        "--function main @@",
    },
    {
        "wasmtime",
        SWAM_PROPOSAL_MVP | SWAM_PROPOSAL_THREADS | SWAM_PROPOSAL_SIMD |
            SWAM_PROPOSAL_BULK | SWAM_PROPOSAL_MEMORY64 |
            SWAM_PROPOSAL_MULTI_MEM,
        NULL,
        0U,
        SWAM_IPC_SHM,
        "run --invoke main (stdin)",
    },
    {
        "wasmer",
        SWAM_PROPOSAL_MVP | SWAM_PROPOSAL_THREADS | SWAM_PROPOSAL_SIMD |
            SWAM_PROPOSAL_BULK,
        NULL,
        0U,
        SWAM_IPC_SHM,
        "run --cranelift --enable-bulk-memory --enable-simd ...",
    },
    {
        "wasm-interp",
        SWAM_PROPOSAL_MVP | SWAM_PROPOSAL_THREADS | SWAM_PROPOSAL_SIMD |
            SWAM_PROPOSAL_BULK | SWAM_PROPOSAL_MEMORY64 |
            SWAM_PROPOSAL_MULTI_MEM,
        NULL,
        0U,
        SWAM_IPC_SHM,
        "--enable-all --run-all-exports @@",
    },
    {
        "v8",
        SWAM_PROPOSAL_MVP | SWAM_PROPOSAL_THREADS | SWAM_PROPOSAL_SIMD |
            SWAM_PROPOSAL_BULK | SWAM_PROPOSAL_MEMORY64 |
            SWAM_PROPOSAL_MULTI_MEM,
        NULL,
        0U,
        SWAM_IPC_FILE,
        "src/v8_harness.js -- @@",
    },
    {
        "jsc",
        SWAM_PROPOSAL_MVP | SWAM_PROPOSAL_THREADS | SWAM_PROPOSAL_SIMD |
            SWAM_PROPOSAL_BULK,
        NULL,
        0U,
        SWAM_IPC_FILE,
        "src/jsc_harness.js -- @@",
    },
    {
        "spidermonkey",
        SWAM_PROPOSAL_MVP | SWAM_PROPOSAL_THREADS | SWAM_PROPOSAL_SIMD |
            SWAM_PROPOSAL_BULK,
        NULL,
        0U,
        SWAM_IPC_FILE,
        "src/spidermonkey_harness.js @@",
    },
};

#define SWAM_NUM_PROFILES (sizeof(swam_profiles) / sizeof(swam_profiles[0]))

const SwamRuntimeProfile *swam_profile_lookup(const char *name) {

    if (name == NULL || strcmp(name, "all") == 0) return NULL;

    for (size_t i = 0; i < SWAM_NUM_PROFILES; i++) {
        if (strcmp(swam_profiles[i].name, name) == 0) {
            return &swam_profiles[i];
        }
    }

    return NULL;

}

uint32_t swam_opclass_required_proposal(SwamOpClass op_class) {

    switch (op_class) {
        case SWAM_OPCLASS_LOAD:
        case SWAM_OPCLASS_STORE:
        case SWAM_OPCLASS_MEMORY_GROW:
            return SWAM_PROPOSAL_MVP;

        case SWAM_OPCLASS_ATOMIC_LOAD:
        case SWAM_OPCLASS_ATOMIC_STORE:
        case SWAM_OPCLASS_ATOMIC_RMW:
        case SWAM_OPCLASS_ATOMIC_CMPXCHG:
        case SWAM_OPCLASS_ATOMIC_WAIT:
        case SWAM_OPCLASS_ATOMIC_NOTIFY:
            return SWAM_PROPOSAL_THREADS;

        case SWAM_OPCLASS_SIMD_LOAD:
        case SWAM_OPCLASS_SIMD_LOAD_LANE:
        case SWAM_OPCLASS_SIMD_STORE:
        case SWAM_OPCLASS_SIMD_STORE_LANE:
            return SWAM_PROPOSAL_SIMD;

        case SWAM_OPCLASS_BULK_COPY:
        case SWAM_OPCLASS_BULK_FILL:
        case SWAM_OPCLASS_BULK_INIT:
            return SWAM_PROPOSAL_BULK;

        default:
            return SWAM_PROPOSAL_MVP;
    }

}

bool swam_profile_supports_opclass(const SwamRuntimeProfile *profile,
                                   SwamOpClass op_class) {

    if (profile == NULL) return true;
    if ((profile->proposals & swam_opclass_required_proposal(op_class)) == 0U) {
        return false;
    }

    for (uint32_t i = 0; i < profile->excluded_opclass_count; i++) {
        if (profile->excluded_opclasses[i] == op_class) {
            return false;
        }
    }

    return true;

}
