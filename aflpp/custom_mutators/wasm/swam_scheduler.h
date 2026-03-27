#ifndef SWAM_SCHEDULER_H
#define SWAM_SCHEDULER_H

#include <stdbool.h>
#include <stdint.h>

#include "swam_scenarios.h"
#include "swam_profile.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque scheduler state. */
typedef struct SwamScheduler SwamScheduler;

/* Outcome of a single exploit iteration, reported to the scheduler. */
typedef enum {
    SWAM_SCHED_INSTANTIATE_FAIL, /* swam_instantiate_scenario returned false */
    SWAM_SCHED_NOT_REACHED,      /* markers invalid or before != scenario_id */
    SWAM_SCHED_REACHED_TRAPPED,   /* before == scenario_id, after != scenario_id */
    SWAM_SCHED_REACHED_COMPLETED, /* before == scenario_id, after == scenario_id */
} SwamSchedOutcome;

/* Create scheduler for `num_scenarios` scenarios. Returns NULL on failure. */
SwamScheduler *swam_scheduler_create(uint32_t num_scenarios);

/* Set the active proposal bitmask. Scenarios whose op_class requires a
   proposal not in the mask are skipped during selection. Pass
   SWAM_PROPOSALS_ALL (0xFFFFFFFF) to disable filtering (default).
   Call this once after swam_scheduler_create(). */
void swam_scheduler_set_proposals(SwamScheduler *sched, uint32_t proposals_mask);

/* Set an optional runtime profile. This preserves proposal filtering and also
   applies any profile-specific opclass exclusions. Passing NULL clears the
   profile-specific exclusions. */
void swam_scheduler_set_profile(SwamScheduler *sched,
                                const SwamRuntimeProfile *profile);

/* Return the number of scenarios that pass the proposal filter.
   Useful for debug logging. */
uint32_t swam_scheduler_active_count(const SwamScheduler *sched);

/* Select the next scenario using UCB1 scoring. Returns scenario ID (1-based).
   `afl` is used for tie-breaking randomness. */
uint32_t swam_scheduler_select(SwamScheduler *sched, void *afl);

/* Update statistics for a scenario after an exploit iteration. */
void swam_scheduler_update(SwamScheduler *sched, uint32_t scenario_id,
                           SwamSchedOutcome outcome);

/* Query how many times a scenario has been selected.
   Returns 0 if sched is NULL or scenario_id is out of range. */
uint32_t swam_scheduler_scenario_selected(const SwamScheduler *sched,
                                          uint32_t scenario_id);

/* Query how many times a scenario's probe has been reached.
   Returns 0 if sched is NULL or scenario_id is out of range. */
uint32_t swam_scheduler_scenario_reached(const SwamScheduler *sched,
                                         uint32_t scenario_id);

/* Query how many times a scenario's probe completed without trapping.
   Returns 0 if sched is NULL or scenario_id is out of range. */
uint32_t swam_scheduler_scenario_completed(const SwamScheduler *sched,
                                           uint32_t scenario_id);

/* Query the total number of scheduler updates across all scenarios. */
uint64_t swam_scheduler_total_selected(const SwamScheduler *sched);

/* Free scheduler state. */
void swam_scheduler_destroy(SwamScheduler *sched);

#ifdef __cplusplus
}
#endif

#endif /* SWAM_SCHEDULER_H */
