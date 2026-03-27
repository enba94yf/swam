#include "swam_scheduler.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>

#include "swam_profile.h"

#if defined(__has_include)
#if __has_include("afl-fuzz.h")
#include "afl-fuzz.h"
#define SWAM_HAVE_AFL_FUZZ_H 1
#endif
#endif

#ifndef SWAM_HAVE_AFL_FUZZ_H
typedef void afl_state_t;
extern uint32_t rand_below(afl_state_t *afl, uint32_t limit);
#endif

/* Per-scenario statistics. */
typedef struct {
    uint32_t n_selected;
    uint32_t n_reached;
    uint32_t n_completed;
    uint32_t n_instantiate_fail;
} SwamScenarioStats;

struct SwamScheduler {
    SwamScenarioStats *stats;   /* array of size num_scenarios */
    uint32_t           num_scenarios;
    uint64_t           total_selected; /* sum of all n_selected */
    uint32_t           proposals_mask; /* active proposal filter */
    const SwamRuntimeProfile *profile; /* optional runtime-specific exclusions */
};

static uint32_t swam_scheduler_rand_below(void *afl, uint32_t limit) {

    if (limit <= 1U) return 0U;

#ifdef SWAM_HAVE_AFL_FUZZ_H
    if (afl != NULL) {
        return rand_below((afl_state_t *)afl, limit);
    }
#else
    (void)afl;
#endif

    return (uint32_t)(rand() % limit);

}

SwamScheduler *swam_scheduler_create(uint32_t num_scenarios) {

    SwamScheduler *sched = NULL;

    if (num_scenarios == 0U) return NULL;

    sched = calloc(1, sizeof(SwamScheduler));
    if (sched == NULL) return NULL;

    sched->stats = calloc(num_scenarios, sizeof(SwamScenarioStats));
    if (sched->stats == NULL) {
        free(sched);
        return NULL;
    }

    sched->num_scenarios = num_scenarios;
    sched->proposals_mask = SWAM_PROPOSALS_ALL;
    sched->profile = NULL;

    return sched;

}

void swam_scheduler_set_proposals(SwamScheduler *sched, uint32_t proposals_mask) {

    if (sched == NULL) return;

    sched->proposals_mask = proposals_mask;
    sched->profile = NULL;

}

void swam_scheduler_set_profile(SwamScheduler *sched,
                                const SwamRuntimeProfile *profile) {

    if (sched == NULL) return;

    sched->profile = profile;
    sched->proposals_mask =
        (profile != NULL) ? profile->proposals : SWAM_PROPOSALS_ALL;

}

/* Check if scenario (1-based ID) passes the proposal filter. */
static bool swam_scenario_passes_filter(const SwamScheduler *sched,
                                        uint32_t scenario_id) {

    const SwamScenario *sc = swam_get_scenario(scenario_id);
    uint32_t            required = 0U;

    if (sched == NULL || sc == NULL) return false;

    required = swam_opclass_required_proposal(sc->op_class);
    if ((sched->proposals_mask & required) == 0U) return false;
    if (sched->profile != NULL &&
        !swam_profile_supports_opclass(sched->profile, sc->op_class)) {
        return false;
    }

    return true;

}

uint32_t swam_scheduler_active_count(const SwamScheduler *sched) {

    uint32_t count = 0U;

    if (sched == NULL) return 0U;

    for (uint32_t i = 0; i < sched->num_scenarios; i++) {
        if (swam_scenario_passes_filter(sched, i + 1U)) {
            count++;
        }
    }

    return count;

}

uint32_t swam_scheduler_select(SwamScheduler *sched, void *afl) {

    uint32_t best_idx = 0U;
    uint32_t best_count = 0U;
    double   best_score = -DBL_MAX;
    double   log_N = 0.0;

    if (sched == NULL || sched->num_scenarios == 0U) return 1U;

    /* Try all never-selected scenarios first, with random tie-breaking. */
    for (uint32_t i = 0; i < sched->num_scenarios; i++) {
        if (!swam_scenario_passes_filter(sched, i + 1U)) continue;
        if (sched->stats[i].n_selected != 0U) continue;

        best_count++;
        if (best_count == 1U ||
            swam_scheduler_rand_below(afl, best_count) == 0U) {
            best_idx = i;
        }
    }

    if (best_count > 0U) return best_idx + 1U;

    /* ε-greedy (ε=0.1): with probability 1/10, pick a random active scenario
       instead of UCB1.  Breaks phase-locking on small catalogs where many
       scenarios converge to nearly identical UCB1 scores and one wins
       deterministically for thousands of consecutive iterations.  */
    {
        uint32_t active = swam_scheduler_active_count(sched);
        if (active > 0U && swam_scheduler_rand_below(afl, 10U) == 0U) {
            uint32_t pick = swam_scheduler_rand_below(afl, active);
            for (uint32_t i = 0; i < sched->num_scenarios; i++) {
                if (!swam_scenario_passes_filter(sched, i + 1U)) continue;
                if (pick == 0U) return i + 1U;
                pick--;
            }
        }
    }

    if (sched->total_selected > 1U) {
        log_N = log((double)sched->total_selected);
    }

    best_count = 0U;

    for (uint32_t i = 0; i < sched->num_scenarios; i++) {

        SwamScenarioStats *s = &sched->stats[i];
        double             score = 0.0;

        if (!swam_scenario_passes_filter(sched, i + 1U)) continue;

        if (s->n_selected >= 10U &&
            (double)s->n_instantiate_fail / (double)s->n_selected > 0.9) {
            continue;
        }

        if (s->n_selected >= 100U && s->n_reached == 0U) {
            continue;
        }

        {
            double reach_rate =
                (double)s->n_reached / (double)s->n_selected;
            double exploit_term = 1.0 - reach_rate;
            double explore_term = 0.0;

            if (log_N > 0.0) {
                explore_term =
                    sqrt(2.0 * log_N / (double)s->n_selected);
            }

            score = exploit_term + explore_term;
        }

        if (score > best_score + DBL_EPSILON) {
            best_score = score;
            best_idx = i;
            best_count = 1U;
        } else if (fabs(score - best_score) <= DBL_EPSILON) {
            best_count++;
            if (swam_scheduler_rand_below(afl, best_count) == 0U) {
                best_idx = i;
            }
        }
    }

    if (best_count == 0U) {
        /* All scenarios suppressed -- fall back to uniform random among active. */
        uint32_t active = swam_scheduler_active_count(sched);

        if (active == 0U) return 0U;

        uint32_t pick = swam_scheduler_rand_below(afl, active);

        for (uint32_t i = 0; i < sched->num_scenarios; i++) {
            if (!swam_scenario_passes_filter(sched, i + 1U)) continue;
            if (pick == 0U) return i + 1U;
            pick--;
        }

        return 0U;
    }

    return best_idx + 1U;

}

void swam_scheduler_update(SwamScheduler *sched, uint32_t scenario_id,
                           SwamSchedOutcome outcome) {

    uint32_t           idx = 0U;
    SwamScenarioStats *s = NULL;

    if (sched == NULL) return;
    if (scenario_id < 1U || scenario_id > sched->num_scenarios) return;

    idx = scenario_id - 1U;
    s = &sched->stats[idx];

    s->n_selected++;
    sched->total_selected++;

    switch (outcome) {
        case SWAM_SCHED_INSTANTIATE_FAIL:
            s->n_instantiate_fail++;
            break;
        case SWAM_SCHED_NOT_REACHED:
            break;
        case SWAM_SCHED_REACHED_TRAPPED:
            s->n_reached++;
            break;
        case SWAM_SCHED_REACHED_COMPLETED:
            s->n_reached++;
            s->n_completed++;
            break;
    }

}

uint32_t swam_scheduler_scenario_selected(const SwamScheduler *sched,
                                          uint32_t scenario_id) {

    if (sched == NULL) return 0U;
    if (scenario_id < 1U || scenario_id > sched->num_scenarios) return 0U;

    return sched->stats[scenario_id - 1U].n_selected;

}

uint32_t swam_scheduler_scenario_reached(const SwamScheduler *sched,
                                         uint32_t scenario_id) {

    if (sched == NULL) return 0U;
    if (scenario_id < 1U || scenario_id > sched->num_scenarios) return 0U;

    return sched->stats[scenario_id - 1U].n_reached;

}

uint32_t swam_scheduler_scenario_completed(const SwamScheduler *sched,
                                           uint32_t scenario_id) {

    if (sched == NULL) return 0U;
    if (scenario_id < 1U || scenario_id > sched->num_scenarios) return 0U;

    return sched->stats[scenario_id - 1U].n_completed;

}

uint64_t swam_scheduler_total_selected(const SwamScheduler *sched) {

    if (sched == NULL) return 0U;

    return sched->total_selected;

}

void swam_scheduler_destroy(SwamScheduler *sched) {

    if (sched == NULL) return;

    free(sched->stats);
    free(sched);

}
