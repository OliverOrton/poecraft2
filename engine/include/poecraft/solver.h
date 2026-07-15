#ifndef POECRAFT_SOLVER_H
#define POECRAFT_SOLVER_H

#include <stddef.h>
#include <stdint.h>

#include "poecraft/api.h"
#include "poecraft/item_state.h"
#include "poecraft/result.h"
#include "poecraft/simulator.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Crafting solver and calculation engine surface (solver phases S1-S5).
 *
 * A solver handle pins one session and one goal specification, owns the
 * enumerated action registry, the abstract state table, and the
 * price-independent distribution cache. Like an action context it belongs
 * to one thread at a time; the underlying session may be shared.
 *
 * Goal specification JSON:
 *
 *   {
 *     "version": "v1",              // optional
 *     "rarity": "rare",             // required finished rarity
 *     "slots": [                    // 1..8 required mods
 *       {"group": "<group key>", "min_tier": 0},
 *       {"family_mod_key": "<mod key>", "min_tier": 1}
 *     ],
 *     "actions": ["chaos", "exalt", "restart"]   // optional candidate
 *   }                               // subset; omitted = full registry
 *
 * Costs are currency-quantity vectors dotted with a pc_economy price table
 * at solve time (same key vocabulary as strategy operations, plus "base"
 * for the synthetic restart action).
 */
#define PC_SOLVER_MAX_GOAL_SLOTS 8

typedef struct pc_solver* pc_solver_handle;

pc_result pc_solver_create(
    pc_session_handle session,
    const char* goal_json,
    size_t goal_json_size,
    pc_solver_handle* out_solver,
    pc_error_info* out_error);

void pc_solver_destroy(pc_solver_handle solver);

/* --- action registry introspection ------------------------------------------ */

pc_result pc_solver_action_count(
    pc_solver_handle solver,
    uint32_t* out_count,
    pc_error_info* out_error);

/* String fields are owned by the solver and valid until it is destroyed.
 * The cost_keys pointer array itself is transient: it stays valid only
 * until the next pc_solver_get_action_info call on the same thread. */
typedef struct pc_solver_action_info {
    uint32_t struct_size;
    uint32_t abi_version;
    uint32_t action_index;
    const char* id;           /* canonical action id, e.g. "essence:<key>" */
    const char* display_name;
    int32_t transition_kind;  /* 0 deterministic, 1 single-slot, 2 reforge,
                                 3 special */
    int32_t synthetic;        /* 1 for restart */
    uint32_t cost_key_count;
    const char* const* cost_keys; /* quantity vector; repeats mean count */
} pc_solver_action_info;

pc_result pc_solver_get_action_info(
    pc_solver_handle solver,
    uint32_t action_index,
    pc_solver_action_info* out_info,
    pc_error_info* out_error);

/* Resolve a canonical action id. Returns PC_RESULT_NOT_FOUND when absent. */
pc_result pc_solver_find_action(
    pc_solver_handle solver,
    const char* action_id,
    uint32_t* out_index,
    pc_error_info* out_error);

/* Candidate action indices the goal was scoped to (the full registry when
 * the goal spec named none). Query-required-count buffer pattern. */
pc_result pc_solver_candidates(
    pc_solver_handle solver,
    uint32_t* out_indices,
    uint32_t capacity,
    uint32_t* out_count,
    pc_error_info* out_error);

/* --- calculation engine (Calculator backend) --------------------------------- */

/* One abstract successor class. state_id is stable for this solver handle
 * and can be passed to pc_solver_state_value after a solve. */
typedef struct pc_calc_outcome {
    uint32_t struct_size;
    uint32_t abi_version;
    uint32_t state_id;
    double probability;
    uint8_t rarity;
    uint8_t prefix_count;
    uint8_t suffix_count;
    uint8_t influence_bits;
    uint32_t flags;        /* abstract mechanic flags */
    uint32_t blocked_mask; /* per goal slot */
    uint8_t slot_status[PC_SOLVER_MAX_GOAL_SLOTS]; /* 0 absent, 1 below
                                                      tier, 2 satisfied */
} pc_calc_outcome;

typedef struct pc_calc_summary {
    uint32_t struct_size;
    uint32_t abi_version;
    int32_t supported; /* 0: no exact evaluator for this action yet */
    int32_t legal;     /* action legality in the item's abstract state */
    uint32_t entry_count;
    double slot_satisfied_probability[PC_SOLVER_MAX_GOAL_SLOTS];
    /* Probability of satisfying the goal's rarity and slot threshold
     * together. This is the same predicate used by pc_solver_solve. */
    double success_probability;
} pc_calc_summary;

/*
 * Exact outcome distribution for one action applied to a concrete item
 * (e.g. the live Emulator item): "odds before you click". Uses the
 * query-required-count buffer pattern; results are cached per abstract
 * state and survive price edits.
 */
pc_result pc_calc_action_outcomes(
    pc_solver_handle solver,
    const pc_item_state* item,
    uint32_t action_index,
    pc_calc_outcome* entries,
    uint32_t capacity,
    uint32_t* out_count,
    pc_calc_summary* out_summary,
    pc_error_info* out_error);

/* --- exact compiled-strategy evaluation ------------------------------------ */

typedef struct pc_strategy_eval_options {
    uint32_t struct_size;
    uint32_t abi_version;
    double epsilon;       /* <= 0 uses the default 1e-12 */
    uint32_t max_sweeps;  /* 0 uses the default 100000 */
    uint32_t max_states;  /* 0 uses the default 100000 */
    uint32_t max_pairs;   /* 0 uses the default 1000000 */
    uint32_t max_transitions; /* 0 uses the default 10000000 stored row entries */
    uint32_t top_classes_per_node; /* 0 uses the default 16 */
} pc_strategy_eval_options;

typedef struct pc_strategy_eval_work* pc_strategy_eval_work_handle;

typedef enum pc_strategy_eval_phase {
    PC_STRATEGY_EVAL_PHASE_DISCOVERY = 1,
    PC_STRATEGY_EVAL_PHASE_SOLVING = 2,
    PC_STRATEGY_EVAL_PHASE_FALLBACK = 3,
    PC_STRATEGY_EVAL_PHASE_FINALIZATION = 4,
    PC_STRATEGY_EVAL_PHASE_DONE = 5
} pc_strategy_eval_phase;

typedef struct pc_strategy_eval_progress {
    uint32_t struct_size;
    uint32_t abi_version;
    int32_t phase; /* pc_strategy_eval_phase */
    int32_t done;
    uint64_t discovered_pairs;
    uint64_t pending_pairs;
    uint64_t solved_sccs;
    uint64_t total_sccs;
    uint64_t fallback_sweeps;
    double residual;
} pc_strategy_eval_progress;

/*
 * Stateful exact evaluation. begin pins the compiled strategy; step performs
 * at most max_work_items bounded units and reports honest phase/count
 * progress; finish is available only after progress.done is true. Destroy is
 * always safe and is the cancellation/abandon path.
 */
pc_result pc_strategy_eval_begin(
    pc_strategy_handle strategy,
    const pc_strategy_eval_options* options,
    pc_strategy_eval_work_handle* out_work,
    pc_error_info* out_error);

pc_result pc_strategy_eval_step(
    pc_strategy_eval_work_handle work,
    uint32_t max_work_items,
    pc_strategy_eval_progress* out_progress,
    pc_error_info* out_error);

pc_result pc_strategy_eval_finish(
    pc_strategy_eval_work_handle work,
    char* buffer,
    size_t capacity,
    size_t* out_length,
    pc_error_info* out_error);

void pc_strategy_eval_destroy(pc_strategy_eval_work_handle work);

/*
 * Evaluate a compiled strategy as an exact absorbing Markov chain over
 * (graph node, abstract item state). The result is JSON v1 and uses the
 * query-required-count buffer pattern: out_length always receives the full
 * byte length, and a provided buffer is nul-terminated when capacity > 0.
 * Unsupported action/condition vocabulary returns
 * PC_RESULT_UNSUPPORTED_FEATURE with every detected graph gap in out_error.
 */
pc_result pc_strategy_evaluate(
    pc_strategy_handle strategy,
    const pc_strategy_eval_options* options,
    char* buffer,
    size_t capacity,
    size_t* out_length,
    pc_error_info* out_error);

/* --- DP solve ----------------------------------------------------------------- */

typedef struct pc_solve_options {
    uint32_t struct_size;
    uint32_t abi_version;
    double epsilon;       /* <= 0 uses the default 1e-9 */
    uint32_t max_states;  /* 0 uses the default 100000 */
    uint32_t max_sweeps;  /* 0 uses the default 100000 */
} pc_solve_options;

typedef struct pc_solve_summary {
    uint32_t struct_size;
    uint32_t abi_version;
    int32_t converged;
    uint32_t start_state;
    double start_value; /* expected remaining cost from the start item */
    uint32_t expanded_states;
    uint32_t sweeps;
    double residual;
    uint32_t skipped_action_count; /* unpriced or unsupported actions the
                                      solve planned without */
} pc_solve_summary;

typedef enum pc_solve_phase {
    PC_SOLVE_PHASE_EXPANDING = 1,
    PC_SOLVE_PHASE_ITERATING = 2,
    PC_SOLVE_PHASE_DONE = 3
} pc_solve_phase;

typedef struct pc_solve_progress {
    uint32_t struct_size;
    uint32_t abi_version;
    int32_t phase; /* pc_solve_phase */
    int32_t done;
    uint32_t expanded_states;
    uint32_t sweeps;
    double residual;
    double start_value_bound;
} pc_solve_progress;

/* Stateful solve surface. begin snapshots the economy and resets the latest
 * result; step performs bounded expansion/sweep work; finish extracts and
 * stores the policy after progress.done; abandon discards partial work. */
pc_result pc_solver_solve_begin(
    pc_solver_handle solver,
    const pc_item_state* start_item,
    pc_economy_handle economy,
    const pc_solve_options* options,
    pc_error_info* out_error);

pc_result pc_solver_solve_step(
    pc_solver_handle solver,
    uint32_t max_work_items,
    pc_solve_progress* out_progress,
    pc_error_info* out_error);

pc_result pc_solver_solve_finish(
    pc_solver_handle solver,
    pc_solve_summary* out_summary,
    pc_error_info* out_error);

void pc_solver_solve_abandon(pc_solver_handle solver);

/*
 * Synchronous value iteration from the start item. The economy supplies
 * the price table (a null economy is invalid: costs are required).
 * Results are stored on the solver for the query calls below; a new solve
 * replaces them. Distribution caches are reused across solves, so a price
 * edit re-solves cheaply.
 */
pc_result pc_solver_solve(
    pc_solver_handle solver,
    const pc_item_state* start_item,
    pc_economy_handle economy,
    const pc_solve_options* options,
    pc_solve_summary* out_summary,
    pc_error_info* out_error);

/*
 * Value and policy action for one abstract state from the latest solve.
 * out_action_id receives NULL for goal/terminal states. The id points into
 * solver-owned storage. Returns PC_RESULT_NOT_FOUND before any solve.
 */
pc_result pc_solver_state_value(
    pc_solver_handle solver,
    uint32_t state_id,
    double* out_value,
    const char** out_action_id,
    pc_error_info* out_error);

/* Project a concrete item to its abstract state id (interning it). */
pc_result pc_solver_project_item(
    pc_solver_handle solver,
    const pc_item_state* item,
    uint32_t* out_state_id,
    pc_error_info* out_error);

/*
 * Compile the latest solve's policy into ordinary strategy JSON (see
 * docs/crafting-solver-plan.md, Policy To Strategy Graph). Query-required-
 * count buffer pattern: out_length always receives the full length.
 * Returns PC_RESULT_UNSUPPORTED_FEATURE when the policy needs condition
 * types the strategy vocabulary does not have yet.
 */
pc_result pc_solver_compile_strategy(
    pc_solver_handle solver,
    char* buffer,
    size_t capacity,
    size_t* out_length,
    pc_error_info* out_error);

/* Latest solve's per-state ML corpus records (JSON lines). Same buffer
 * pattern as pc_solver_compile_strategy. */
pc_result pc_solver_solve_log(
    pc_solver_handle solver,
    char* buffer,
    size_t capacity,
    size_t* out_length,
    pc_error_info* out_error);

/* Versioned solver telemetry JSON for the current handle and latest solve.
 * Available before solving (registry/layout fields are populated), during a
 * stepped solve (partial counters advance), after abandon (the last partial
 * snapshot is retained), and after solve/compile. Fields the current
 * algorithm does not provide are explicit JSON nulls with an availability
 * reason; no sentinel values stand in for missing metrics. */
pc_result pc_solver_telemetry(
    pc_solver_handle solver,
    char* buffer,
    size_t capacity,
    size_t* out_length,
    pc_error_info* out_error);

#ifdef __cplusplus
}
#endif

#endif
