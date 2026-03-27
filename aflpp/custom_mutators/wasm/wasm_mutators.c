#include "afl-fuzz.h"
#include "mutator_func.h"
#include "swam_exploit.h"
#include "swam_markers.h"
#include "swam_strip.h"
#include "swam_scheduler.h"
#include "swam_profile.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <binaryen-c.h>

#ifdef SWAM_DEBUG
static FILE* swam_debug_log = NULL;
#define SWAM_LOG(fmt, ...) do { \
    if (swam_debug_log) { \
        fprintf(swam_debug_log, "[SWAM] " fmt "\n", ##__VA_ARGS__); \
        fflush(swam_debug_log); \
    } \
} while (0)
#else
#define SWAM_LOG(fmt, ...) ((void)0)
#endif

#define SWAM_UNKNOWN_NOTRAP_SAVE_CAP 32U
#define SWAM_BUG_SAVE_CAP 3U

enum {
    SWAM_BUG_TAG_BYPASS = 0,
    SWAM_BUG_TAG_FALSE_TRAP = 1,
};

static uint32_t swam_unknown_saved_trap[SWAM_SCENARIO_COUNT];

/* Binaryen's C API in our vendored build intentionally re-throws
   wasm::ParseException from BinaryenModuleRead() so that embedding
   applications can decide how to handle parse failures. afl-fuzz calls
   into this custom mutator via a C ABI, so we must not let any C++
   exception escape (it would abort the fuzzer process). */
BinaryenModuleRef safe_BinaryenModuleRead(char* input, size_t inputSize);
int safe_BinaryenModuleAllocateAndWrite(BinaryenModuleRef module,
                                        BinaryenModuleAllocateAndWriteResult* result);

static BinaryenModuleRef swam_clone_module(BinaryenModuleRef module) {

    BinaryenModuleAllocateAndWriteResult clone_bytes;
    BinaryenModuleRef clone = NULL;

    if (module == NULL) return NULL;

    memset(&clone_bytes, 0, sizeof(clone_bytes));
    if (!safe_BinaryenModuleAllocateAndWrite(module, &clone_bytes)) {
        return NULL;
    }

    clone = safe_BinaryenModuleRead(clone_bytes.binary, clone_bytes.binaryBytes);
    free(clone_bytes.binary);

    return clone;

}

wasm_mutator_t *afl_custom_init(afl_state_t *afl, unsigned int seed) {

    srand(seed);

    wasm_mutator_t *data = calloc(1, sizeof(wasm_mutator_t));
    if (!data) {

        perror("afl_custom_init calloc");
        return NULL;

    }

    if ((data->mutated_out_buf = (u8 *)calloc(MAX_FILE, sizeof(char))) == NULL) {

        perror("afl_custom_init calloc");
        free(data);
        return NULL;

    }


    if ((data->name_out_buf = (u8 *)calloc(256, sizeof(char))) == NULL) {

        perror("afl_custom_init calloc");
        free(data->mutated_out_buf);
        free(data);
        return NULL;

    }

    wasm_init(data);
    data->afl = afl;
    swam_markers_init();
    data->swam_scheduler = swam_scheduler_create(swam_scenario_count());

    memset(swam_unknown_saved_trap, 0, sizeof(swam_unknown_saved_trap));

#ifdef SWAM_DEBUG
    {
        const char* log_path = getenv("SWAM_DEBUG_LOG");
        if (!log_path) log_path = "/tmp/swam_debug.log";
        swam_debug_log = fopen(log_path, "a");
    }
    SWAM_LOG("=== SWAM debug session started ===");
#endif

    /* Apply runtime capability profile from SWAM_RUNTIME env var. */
    {
        const char* runtime_env = getenv("SWAM_RUNTIME");

        if (runtime_env != NULL && strcmp(runtime_env, "all") != 0) {
            const SwamRuntimeProfile* profile = swam_profile_lookup(runtime_env);

            if (profile != NULL) {
                swam_scheduler_set_profile(data->swam_scheduler, profile);
                SWAM_LOG("runtime profile: %s (proposals=0x%x, active=%u/%u)",
                         profile->name, profile->proposals,
                         swam_scheduler_active_count(data->swam_scheduler),
                         swam_scenario_count());
            } else {
                SWAM_LOG("WARNING: unknown runtime '%s', using all proposals",
                         runtime_env);
            }
        }
    }

    return data;

}

int32_t afl_custom_init_trim(wasm_mutator_t *data, uint8_t *buf, size_t buf_size) {

    /* we disable trimming saved testcases that are introduced by
       our custom mutators. */

    return 0;

}

size_t afl_custom_trim(wasm_mutator_t *data, uint8_t **out_buf) {

    return 0;

}

int32_t afl_custom_post_trim(wasm_mutator_t *data, int success) {

    return 0;

}


const char *afl_custom_describe(wasm_mutator_t *data, size_t max_description_len) {

    snprintf((char*)data->name_out_buf, max_description_len,
             "br:%u,int:%u,opt:%u,cal:%u,rec:%u,spl:%u,swam:%u",
             data->br_target_cnt, data->interest_cnt, data->operator_cnt,
             data->call_target_cnt, data->recurse_cnt, data->splice_cnt,
             data->swam_exploit_cnt);

    return (const char*)data->name_out_buf;

}

static void swam_save_bug(wasm_mutator_t* data, const SwamScenario* scenario,
                          const char* tag) {

    afl_state_t* afl = data->afl;
    char dir_path[512];
    char file_path[1024];
    static uint32_t bug_counter = 0;

    if (!data->swam_pending_module || data->swam_pending_module_size == 0) return;

    snprintf(dir_path, sizeof(dir_path), "%s/swam_bugs", afl->out_dir);
    mkdir(dir_path, 0755);

    snprintf(file_path, sizeof(file_path),
             "%s/id:%06u,scenario:%03u,layout:%u,expected:%u,tag:%s.wasm",
             dir_path, bug_counter++, scenario->id, scenario->layout,
             scenario->expected, tag);

    FILE* f = fopen(file_path, "wb");
    if (f) {
        fwrite(data->swam_pending_module, 1, data->swam_pending_module_size, f);
        fclose(f);
    }

}

static void swam_save_unknown(wasm_mutator_t* data, const SwamScenario* scenario,
                              const char* tag) {

    afl_state_t* afl = data->afl;
    char dir_path[512];
    char file_path[1024];
    static uint32_t unknown_counter = 0;

    if (!data->swam_pending_module || data->swam_pending_module_size == 0) return;

    snprintf(dir_path, sizeof(dir_path), "%s/swam_unknowns", afl->out_dir);
    mkdir(dir_path, 0755);

    snprintf(file_path, sizeof(file_path),
             "%s/id:%06u,scenario:%03u,layout:%u,expected:%u,tag:%s.wasm",
             dir_path, unknown_counter++, scenario->id, scenario->layout,
             scenario->expected, tag);

    FILE* f = fopen(file_path, "wb");
    if (f) {
        fwrite(data->swam_pending_module, 1, data->swam_pending_module_size, f);
        fclose(f);
    }

}

#ifdef SWAM_DEBUG
static void swam_log_unknown_summary(const wasm_mutator_t* data) {

    for (uint32_t scenario_idx = 0; scenario_idx < swam_scenario_count();
         ++scenario_idx) {
        uint32_t total_hits = data->swam_unknown_total[scenario_idx];

        if (total_hits == 0) continue;

        const SwamScenario* scenario = swam_get_scenario(scenario_idx + 1U);
        if (!scenario) continue;

        SWAM_LOG("unknown totals: scenario %u: %u hits (%u saved notrap, %u saved trap)",
                 scenario->id, total_hits,
                 data->swam_unknown_saved_notrap[scenario_idx],
                 swam_unknown_saved_trap[scenario_idx]);
    }

}

static void swam_log_scheduler_summary(const wasm_mutator_t* data,
                                       uint32_t scenario_id) {

    uint32_t selected = 0U;
    uint32_t entered = 0U;
    uint32_t completed = 0U;
    double entered_pct = 0.0;
    double completed_pct = 0.0;

    if (data == NULL || data->swam_scheduler == NULL || scenario_id == 0U) {
        return;
    }

    selected =
      swam_scheduler_scenario_selected(data->swam_scheduler, scenario_id);
    entered = swam_scheduler_scenario_reached(data->swam_scheduler, scenario_id);
    completed =
      swam_scheduler_scenario_completed(data->swam_scheduler, scenario_id);

    if (selected > 0U) {
        entered_pct = 100.0 * (double)entered / (double)selected;
        completed_pct = 100.0 * (double)completed / (double)selected;
    }

    SWAM_LOG("stats: scenario=%u selected=%u entered=%u (%.2f%%) completed=%u (%.2f%%) bug_hits=%u",
             scenario_id, selected, entered, entered_pct, completed,
             completed_pct, data->swam_bug_hits_total);

}
#endif

static void swam_scheduler_record(wasm_mutator_t* data, uint32_t scenario_id,
                                  SwamSchedOutcome outcome) {

    if (data == NULL) return;

    swam_scheduler_update(data->swam_scheduler, scenario_id, outcome);

#ifdef SWAM_DEBUG
    if (data->swam_scheduler == NULL || scenario_id == 0U) return;
    if ((swam_scheduler_total_selected(data->swam_scheduler) % 10000ULL) != 0U) {
        return;
    }

    swam_log_scheduler_summary(data, scenario_id);
#endif

}

static void swam_maybe_save_bug(wasm_mutator_t* data,
                                const SwamScenario* scenario,
                                int tag_idx,
                                const char* tag_name) {

    uint32_t scenario_idx = 0U;

    if (data == NULL || scenario == NULL) return;

    data->swam_bug_hits_total++;

    if (!data->swam_pending_module || data->swam_pending_module_size == 0) return;

    scenario_idx = scenario->id - 1U;
    if (data->swam_saved_bug_count[scenario_idx][tag_idx] >= SWAM_BUG_SAVE_CAP) {
        return;
    }

    data->swam_saved_bug_count[scenario_idx][tag_idx]++;
    swam_save_bug(data, scenario, tag_name);

}

static void swam_oracle_check(wasm_mutator_t* data) {

    uint32_t before = 0, after = 0;
    bool valid = false;

    swam_markers_read(&before, &after, &valid);
    SWAM_LOG("oracle: scenario=%u before=%u after=%u valid=%d",
             data->swam_last_scenario_id, before, after, (int)valid);

    if (!valid) {
        /* Harness didn't write markers — module may have failed validation,
           crashed before reaching the probe, or the runtime doesn't support
           marker globals. */
        swam_scheduler_record(data, data->swam_last_scenario_id,
                              SWAM_SCHED_NOT_REACHED);
        goto done;
    }

    const SwamScenario* scenario = swam_get_scenario(data->swam_last_scenario_id);
    if (!scenario) goto done;

    /* Classify probe outcome from marker values:
       - before == scenario_id && after == scenario_id  -> PASSED (no trap)
       - before == scenario_id && after != scenario_id  -> TRAPPED
       - before != scenario_id                          -> NOT_REACHED */
    if (before != scenario->id) {
        swam_scheduler_record(data, data->swam_last_scenario_id,
                              SWAM_SCHED_NOT_REACHED);
        goto done;
    }

    bool trapped = (after != scenario->id);

    swam_scheduler_record(data, data->swam_last_scenario_id,
                          trapped ? SWAM_SCHED_REACHED_TRAPPED
                                  : SWAM_SCHED_REACHED_COMPLETED);

    if (scenario->expected == SWAM_EXPECT_TRAP && !trapped) {
        swam_maybe_save_bug(data, scenario, SWAM_BUG_TAG_BYPASS, "bypass");
    } else if (scenario->expected == SWAM_EXPECT_NO_TRAP && trapped) {
        swam_maybe_save_bug(data, scenario, SWAM_BUG_TAG_FALSE_TRAP,
                            "false_trap");
    } else if (scenario->expected == SWAM_EXPECT_UNKNOWN) {
        uint32_t scenario_idx = scenario->id - 1U;

        data->swam_unknown_total[scenario_idx]++;
        if (trapped) {
            /* Suspicious: UNKNOWN scenario trapped — always save */
            swam_save_unknown(data, scenario, "unknown_trap");
            swam_unknown_saved_trap[scenario_idx]++;
        } else {
            /* Ordinary NO_TRAP unknown — save up to cap */
            if (data->swam_unknown_saved_notrap[scenario_idx] <
                SWAM_UNKNOWN_NOTRAP_SAVE_CAP) {
                swam_save_unknown(data, scenario, "unknown_notrap");
                data->swam_unknown_saved_notrap[scenario_idx]++;
            }
        }
    }

done:
    free(data->swam_pending_module);
    data->swam_pending_module = NULL;
    data->swam_pending_module_size = 0;

}

size_t afl_custom_fuzz(wasm_mutator_t *data, uint8_t *buf, size_t buf_size,
                       u8 **out_buf, uint8_t *add_buf,
                       size_t add_buf_size,  // add_buf can be NULL
                       size_t max_size) {

    afl_state_t* afl = data->afl;
    /* The original guard skipped non-custom queue entries, but with
       AFL_CUSTOM_MUTATOR_ONLY=1 the initial corpus also needs to be
       mutated. Otherwise AFL++ never generates new testcases and the UI
       appears stuck at "custom mutate 0". */

    /* === SWAM: Online oracle — process markers from PREVIOUS execution === */
    if (data->swam_last_was_exploit) {
        swam_oracle_check(data);
        data->swam_last_was_exploit = false;
    }
    /* === end SWAM oracle === */

    BinaryenFeatures enabled_features = BinaryenFeatureMVP() | BinaryenFeatureBulkMemory() | BinaryenFeatureSignExt() |
                                        BinaryenFeatureMutableGlobals() | BinaryenFeatureNontrappingFPToInt() |
                                        BinaryenFeatureReferenceTypes() | BinaryenFeatureMultivalue() | BinaryenFeatureSIMD128();

    BinaryenModuleRef wasm_module_1 = safe_BinaryenModuleRead((char*)buf, buf_size);

    if (!wasm_module_1) {

        /* If the input cannot be parsed as Wasm at all, fall back to returning
           the original buffer, but still obey the max_size contract so we never
           write or report more bytes than AFL++ asked for. */
        size_t out_size = buf_size;
        if (out_size > max_size) {
            out_size = max_size;
        }

        memcpy(data->mutated_out_buf, buf, out_size);
        *out_buf = data->mutated_out_buf;
        return out_size;

    }

    BinaryenModuleSetFeatures(wasm_module_1, enabled_features);

    bool swam_exploit_ok = false;
    const SwamScenario* swam_scenario = NULL;
    BinaryenModuleRef swam_exploit_module = NULL;
    BinaryenModuleRef output_module = wasm_module_1;
    if (rand_below(afl, 2) == 0) {

        {
            uint32_t sched_id = swam_scheduler_select(data->swam_scheduler, data->afl);
            swam_scenario = swam_get_scenario(sched_id);
        }
        SWAM_LOG("exploit: scenario=%u op=%d layout=%d expected=%d",
                 swam_scenario ? swam_scenario->id : 0,
                 swam_scenario ? (int)swam_scenario->op_kind : -1,
                 swam_scenario ? (int)swam_scenario->layout : -1,
                 swam_scenario ? (int)swam_scenario->expected : -1);
        if (swam_scenario) {
            const char* fail_reason = "unknown";

            swam_exploit_module = swam_clone_module(wasm_module_1);
            if (swam_exploit_module != NULL) {
                BinaryenModuleSetFeatures(swam_exploit_module, enabled_features);
                swam_strip_markers(swam_exploit_module);
                if (swam_instantiate_scenario(data,
                                              swam_exploit_module,
                                              swam_scenario,
                                              &fail_reason)) {
                    swam_exploit_ok = true;
                    output_module = swam_exploit_module;
                    data->swam_exploit_cnt++;
                    SWAM_LOG("exploit: instantiate OK (cnt=%u)", data->swam_exploit_cnt);
                } else {
                    SWAM_LOG("exploit: instantiate FAIL reason=%s -> discard clone, fallback to explore",
                             fail_reason);
                    swam_scheduler_record(data, swam_scenario->id,
                                          SWAM_SCHED_INSTANTIATE_FAIL);
                    BinaryenModuleDispose(swam_exploit_module);
                    swam_exploit_module = NULL;
                }
            } else {
                SWAM_LOG("exploit: clone FAIL -> fallback to explore");
            }

        }

    }

    BinaryenModuleRef wasm_module_2 = NULL;
    list_t fragment_pool;
    memset(&fragment_pool, 0, sizeof(list_t));

    if (!swam_exploit_ok) {

        bool use_splice = (add_buf != NULL && rand_below(afl, 2));

        wasm_module_2 =
            (use_splice) ? safe_BinaryenModuleRead((char*)add_buf, add_buf_size) : NULL;
        /* If the splice partner does not parse as valid Wasm, fall back to
           a single-input mutation run. In our Binaryen build, the underlying
           BinaryenModuleRead prints parse errors and throws ParseException;
           safe_BinaryenModuleRead catches and returns NULL. */
        if (use_splice && wasm_module_2) {
            BinaryenModuleSetFeatures(wasm_module_2, enabled_features);
        } else {
            use_splice = false;
            wasm_module_2 = NULL;
        }

        u32 base_cycles = use_splice ? CUSTOM_SPLICE_HAVOC : CUSTOM_HAVOC_CYCLES;
        unsigned long long stage_max_ull =
            (unsigned long long)base_cycles * afl->queue_cur->perf_score / afl->havoc_div;
        u32 stage_cur = 0;
        u32 stage_max = (u32)(stage_max_ull >> 8);
        if (unlikely(stage_max < HAVOC_MIN)) { stage_max = HAVOC_MIN; }

        data->splice_cnt = 0;
        data->recurse_cnt = 0;
        data->interest_cnt = 0;
        data->operator_cnt = 0;
        data->br_target_cnt = 0;
        data->call_target_cnt = 0;

        if (use_splice) {
            wasm_fragmentize_module(wasm_module_2, &fragment_pool);
        }

        /* Default to 1 so the stage counter always makes forward progress.
           Some mutators can legitimately return 0 when they cannot find a
           suitable mutation (e.g., no calls/branches in a tiny seed). If we
           let mutation_cnt stay 0, the for-loop increment becomes 0 and AFL++
           will spin forever in the first custom stage. */
        u32 mutation_cnt = 1;
        u32 mutator_cnt = (use_splice) ? 6 : 5;
        for (stage_cur = 0; stage_cur < stage_max; stage_cur += mutation_cnt) {

            u32 mutator_choice = rand_below(afl, mutator_cnt);

            switch (mutator_choice) {
                case 0:
                    mutation_cnt = wasm_call_target_function_mutate(data, wasm_module_1);
                    data->call_target_cnt += mutation_cnt;
                    break;
                case 1:
                    mutation_cnt = wasm_interest_values_mutate(data, wasm_module_1);
                    data->interest_cnt += mutation_cnt;
                    break;
                case 2:
                    mutation_cnt = wasm_branch_target_mutate(data, wasm_module_1);
                    data->br_target_cnt += mutation_cnt;
                    break;
                case 3:
                    mutation_cnt = wasm_operators_mutate(data, wasm_module_1);
                    data->operator_cnt += mutation_cnt;
                    break;
                case 4:
                    mutation_cnt = wasm_recursive_mutate(data, wasm_module_1);
                    data->recurse_cnt += mutation_cnt;
                    break;
                case 5:
                    mutation_cnt = wasm_splicing_mutate(data, wasm_module_1, wasm_module_2,
                                                        &fragment_pool);
                    data->splice_cnt += mutation_cnt;
                    break;
            }

            /* If the chosen mutator could not make a change, move the stage
               forward by one to avoid an infinite loop. */
            if (mutation_cnt == 0) mutation_cnt = 1;

        }

    }

    BinaryenModuleAllocateAndWriteResult res_module;
    memset(&res_module, 0, sizeof(res_module));
    if (!safe_BinaryenModuleAllocateAndWrite(output_module, &res_module)) {

        /* Serialization failed inside Binaryen; keep the original input to
           avoid emitting malformed or truncated Wasm. */
        size_t out_size = buf_size;
        if (out_size > max_size) {
            out_size = max_size;
        }
        memcpy(data->mutated_out_buf, buf, out_size);
        *out_buf = data->mutated_out_buf;

        if (fragment_pool.element_total_count) LIST_FOREACH_CLEAR(&fragment_pool, fragGrp, {free(el->frags); free(el);});
        if (wasm_module_2) BinaryenModuleDispose(wasm_module_2);
        if (swam_exploit_module) BinaryenModuleDispose(swam_exploit_module);
        BinaryenModuleDispose(wasm_module_1);
        wasm_reset_name_grp(data);

        return out_size;

    }

    /* BinaryenModuleAllocateAndWrite can legally return a module that is
       larger than the original input. However, afl-fuzz assumes that a
       custom mutator respects the provided max_size limit and also that
       any internal buffers are large enough to hold the result.
       Previously we unconditionally memcpy() res_module.binaryBytes bytes
       into mutated_out_buf (allocated with MAX_FILE bytes), which could
       corrupt the heap when Binaryen's output exceeded MAX_FILE / max_size.
       That heap corruption later manifested as seemingly unrelated crashes
       inside libstdc++ / Binaryen (e.g., ParseException::dump).

       To avoid this, clamp the output size to max_size (which is derived
       from MAX_FILE) and, if the Binaryen output is too large, fall back
       to returning the original input instead of a truncated wasm module. */

    size_t out_size = res_module.binaryBytes;

    if (out_size > max_size) {

        /* Too large to fit into the fuzzing buffer – keep the original. */
        if (buf_size > max_size) {
            buf_size = max_size;
        }
        memcpy(data->mutated_out_buf, buf, buf_size);
        *out_buf = data->mutated_out_buf;
        out_size = buf_size;

    } else {

        memcpy(data->mutated_out_buf, res_module.binary, out_size);
        *out_buf = data->mutated_out_buf;

    }

    if (swam_exploit_ok && out_size == res_module.binaryBytes && swam_scenario != NULL) {

        free(data->swam_pending_module);
        data->swam_pending_module = malloc(out_size);
        if (data->swam_pending_module) {
            memcpy(data->swam_pending_module, data->mutated_out_buf, out_size);
            data->swam_pending_module_size = out_size;
            swam_markers_reset();
            data->swam_last_was_exploit = true;
            data->swam_last_scenario_id = swam_scenario->id;
        } else {
            data->swam_pending_module_size = 0;
        }

    }

    if (fragment_pool.element_total_count) LIST_FOREACH_CLEAR(&fragment_pool, fragGrp, {free(el->frags); free(el);});
    if (wasm_module_2) BinaryenModuleDispose(wasm_module_2);
    if (swam_exploit_module) BinaryenModuleDispose(swam_exploit_module);
    BinaryenModuleDispose(wasm_module_1);
    wasm_reset_name_grp(data);
    free(res_module.binary);

    return out_size;

}

void afl_custom_splice_optout(wasm_mutator_t *data) {

    /* This function is never called, just needs to be present to opt out of splicing.
       In AFL_SWAM mode, splicing requires queue entries to have custom==1 flag set,
       which can cause infinite loops if no such entries exist. We use our own
       wasm_splicing_mutate() instead. */
    (void)data;

}

void afl_custom_deinit(wasm_mutator_t *data) {

    wasm_deinit(data);
    swam_markers_cleanup();
    swam_scheduler_destroy(data->swam_scheduler);

#ifdef SWAM_DEBUG
    swam_log_unknown_summary(data);
    SWAM_LOG("=== SWAM debug session ended (exploits: %u) ===", data->swam_exploit_cnt);
    if (swam_debug_log) {
        fclose(swam_debug_log);
        swam_debug_log = NULL;
    }
#endif

    free(data->swam_pending_module);
    free(data->mutated_out_buf);
    free(data->name_out_buf);
    free(data);

}
