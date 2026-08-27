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
 *     "action_mode": "goal_relevant", // optional product action control
 *     "fossil_mode": "goal_relevant", // optional bounded product mode
 *     "requested_fossil_actions": [    // optional extra materialized ids
 *       "fossil:<key>+<key>"
 *     ],
 *     "disabled_action_families": [    // optional restricted envelope
 *       "temporary_bench", "imprint"
 *     ],
 *     "actions": ["chaos", "exalt", "restart"],  // optional primitives
 *     "options": [                  // optional fixed solver programs
 *       {"type":"scour_alchemy"},
 *       {"type":"eldritch_side_intent","side":"prefix",
 *        "action":"eldritch_exalt",
 *        "setup":["eldritch_ichor:1","eldritch_ember:2"]},
 *       {"type":"protected_side","side":"suffix",
 *        "action":"harvest_reforge:defences"},
 *       {"type":"multimod_finish",
 *        "bench_crafts":["bench:<mod key>","bench:<mod key>"]},
 *       {"type":"renewal","actions":["chaos"],
 *        "until":{"goal_slots":[0,1],"min_satisfied":1}},
 *       {"type":"renewal","actions":["veiled_chaos","unveil"],
 *        "until":{"goal_slots":[0],"min_satisfied":1}},
 *       {"type":"protected_repeat","side":"prefix",
 *        "action":"harvest_reforge:defences",
 *        "until":{"goal_slots":[0,1],"min_satisfied":2}},
 *       {"type":"fracture_prepare","preparation":["chaos"],
 *        "carrier_goal_slot":0}
 *     ]
 *   }                               // omitted actions = generated registry;
 *                                   // [] = options only
 *
 * Imprint retry stages are not authored in `options`. Goal-relevant solves
 * discover bounded exact attempt/restore kernels automatically at reachable
 * magic carriers where native checkpoint creation is legal.
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
    /* S8.2 symbolic carrier-property masks. Bits: 0 goal families,
     * 1 satisfied subset, 2 junk/blockers, 3 crafted, 4 fractured,
     * 5 prefixes, 6 suffixes, 7 active protection. */
    uint32_t can_preserve;
    uint32_t can_destroy;
    uint32_t can_create;
    uint32_t can_make_unreachable;
    int32_t destructive_renewal;
    int32_t preserves_fractured_affixes;
    int32_t respects_prefix_lock;
    int32_t respects_suffix_lock;
    int32_t respects_cannot_roll_attack;
    int32_t respects_cannot_roll_caster;
    /* Stable native solve-envelope family name. Membership is engine-owned;
     * callers may present it but must not infer it from id prefixes. */
    const char* family;
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

/* --- natural-T1 corpus feasibility ----------------------------------------- */

typedef enum pc_goal_feasibility_status {
    PC_GOAL_FEASIBILITY_UNKNOWN = 0,
    PC_GOAL_FEASIBILITY_FEASIBLE = 1,
    PC_GOAL_FEASIBILITY_INFEASIBLE = 2
} pc_goal_feasibility_status;

typedef enum pc_goal_feasibility_reason {
    PC_GOAL_FEASIBILITY_REASON_NONE = 0,
    PC_GOAL_FEASIBILITY_REASON_ALREADY_SATISFIED = 1,
    PC_GOAL_FEASIBILITY_REASON_NATURAL_REFORGE_WITNESS = 2,
    PC_GOAL_FEASIBILITY_REASON_NO_NATURAL_T1 = 3,
    PC_GOAL_FEASIBILITY_REASON_SLOT_CAPACITY = 4,
    PC_GOAL_FEASIBILITY_REASON_GROUP_CONFLICT = 5,
    PC_GOAL_FEASIBILITY_REASON_UNSUPPORTED_START = 6,
    PC_GOAL_FEASIBILITY_REASON_NO_ADMITTED_REFORGE = 7
} pc_goal_feasibility_reason;

/* Engine-owned three-way query used by seeded natural-T1 corpus generation.
 * A FEASIBLE result is backed by an admitted ordinary reforge with a
 * positive-weight, capacity-safe, group-compatible T1 assignment. An
 * INFEASIBLE result is a structural proof. Every unsupported start or proof
 * gap returns UNKNOWN; bounded solve exhaustion is never consulted.
 *
 * witness_action_id is owned by the solver. Unused witness_mod_ids entries
 * are UINT32_MAX. slot_* arrays correspond to the resolved goal slots. */
typedef struct pc_goal_feasibility {
    uint32_t struct_size;
    uint32_t abi_version;
    int32_t status; /* pc_goal_feasibility_status */
    int32_t reason; /* pc_goal_feasibility_reason */
    uint32_t goal_slot_count;
    uint32_t required_slot_count;
    uint32_t eligible_slot_count;
    uint32_t natural_pool_mod_count;
    uint32_t natural_prefix_mod_count;
    uint32_t natural_suffix_mod_count;
    uint64_t natural_pool_weight;
    uint64_t natural_prefix_weight;
    uint64_t natural_suffix_weight;
    uint32_t witness_action_index;
    const char* witness_action_id;
    uint32_t witness_mod_ids[PC_SOLVER_MAX_GOAL_SLOTS];
    uint32_t slot_natural_mod_counts[PC_SOLVER_MAX_GOAL_SLOTS];
    uint64_t slot_natural_weights[PC_SOLVER_MAX_GOAL_SLOTS];
    double slot_single_draw_probabilities[PC_SOLVER_MAX_GOAL_SLOTS];
} pc_goal_feasibility;

pc_result pc_solver_goal_feasibility(
    pc_solver_handle solver,
    const pc_item_state* start_item,
    pc_goal_feasibility* out_feasibility,
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
    /* Optional S8.4 accounting context. The evaluator retains the immutable
     * economy snapshot for stateful work. Review JSON is copied during begin
     * and remains non-executable display grouping metadata. */
    pc_economy_handle economy;
    const char* review_projection_json;
    size_t review_projection_json_size;
    int32_t include_success_normalized;
    uint64_t max_owned_bytes; /* 0 uses the default 512 MiB */
    uint64_t max_output_json_bytes; /* 0 uses the default 64 MiB */
} pc_strategy_eval_options;

typedef struct pc_native_memory_stats {
    uint32_t struct_size;
    uint32_t abi_version;
    uint64_t live_owned_bytes;
    uint64_t peak_owned_bytes;
    uint64_t serialized_output_bytes;
} pc_native_memory_stats;

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

pc_result pc_strategy_eval_memory_stats(
    pc_strategy_eval_work_handle work,
    pc_native_memory_stats* out_stats,
    pc_error_info* out_error);

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
    uint32_t max_states;  /* 0 uses the default 200000 */
    uint32_t max_sweeps;  /* 0 uses the default 100000 */
    uint32_t max_discovered_states;
    uint32_t max_expanded_states;
    uint64_t max_state_action_rows;
    uint64_t max_transitions;
    /* Stable V1-equivalent logical reforge search envelope. Evaluator-specific
     * V1/V2/V3 effort is reported separately in solver telemetry. */
    uint64_t max_reforge_work;
    uint64_t max_solver_owned_bytes;
    uint32_t max_compiled_nodes;
    uint32_t max_compiled_edges;
    uint64_t max_strategy_json_bytes;
    uint32_t max_diagnostic_samples;
    uint64_t max_telemetry_json_bytes;
    /* Additive diagnostic controls. Zero selects exact quotient mode with
     * coarse telemetry. Strict-state mode is an oracle, never an
     * approximation switch. */
    uint32_t solver_flags;
    /* Product stopping targets. Non-positive values disable the target.
     * These are observed only after a complete focused lower/upper round and
     * never participate in Bellman comparisons or exact closure. */
    double max_absolute_optimality_gap;
    double max_relative_optimality_gap;
    /* Optional deterministic work boundary for post-solve attempts to
     * certify or strictly lift a cheaper policy. Zero inherits
     * max_discovered_states. A verified executable fallback is retained when
     * this optional improvement budget is exhausted. */
    uint32_t max_policy_refinement_states;
    /* Versioned native-owned option bundle. Default/zero preserves the
     * historical field-by-field C ABI behavior. Profile-owned fields may be
     * overridden only when the corresponding override bit is present. */
    uint32_t solve_profile;
    uint32_t solve_profile_override_mask;
} pc_solve_options;

typedef enum pc_solve_profile {
    PC_SOLVE_PROFILE_DEFAULT = 0,
    PC_SOLVE_PROFILE_CALCULATOR_PRODUCT_V1 = 1
} pc_solve_profile;

typedef enum pc_solve_profile_override {
    PC_SOLVE_PROFILE_OVERRIDE_GOAL_PROGRESS_GATED_REFORGES = 1u << 0,
    PC_SOLVE_PROFILE_OVERRIDE_ECONOMIC_RESTART = 1u << 1,
    PC_SOLVE_PROFILE_OVERRIDE_IMPRINT_PROGRAMS = 1u << 2,
    PC_SOLVE_PROFILE_OVERRIDE_HIGH_IMPACT_EXECUTABLE_UPPERS = 1u << 3,
    PC_SOLVE_PROFILE_OVERRIDE_POLICY_REFINEMENT_STATES = 1u << 4
} pc_solve_profile_override;

typedef enum pc_solver_flag {
    PC_SOLVER_FLAG_FULL_EVIDENCE = 1u << 0,
    PC_SOLVER_FLAG_STRICT_STATES = 1u << 1,
    PC_SOLVER_FLAG_DISABLE_KERNEL_REUSE = 1u << 2,
    /* Exact only within the disclosed zero-progress-reroll restriction:
     * terminal reforge outcomes are grouped, zero-progress outcomes enter a
     * destructive-reforge-only retry basin, and partial progress remains
     * exact. The default unrestricted solver is unchanged. */
    PC_SOLVER_FLAG_GOAL_PROGRESS_GATED_REFORGES = 1u << 3,
    /* Remove ordinary economic abandonment from the Bellman action scope.
     * Exact mechanic-owned replacement recovery remains available. */
    PC_SOLVER_FLAG_DISABLE_ECONOMIC_RESTART = 1u << 4,
    /* Exclude generated Imprint checkpoint/retry programs from the requested
     * solve action scope. Omitted/default behavior continues to consider
     * them. Other automatic families are unaffected. */
    PC_SOLVER_FLAG_DISABLE_IMPRINT_PROGRAMS = 1u << 5,
    /* Public value for the exact operator-major delayed-action scheduler.
     * The former high diagnostic bit remains accepted for compatibility. */
    PC_SOLVER_FLAG_HIGH_IMPACT_EXECUTABLE_UPPERS = 1u << 6
} pc_solver_flag;

typedef enum pc_solve_policy_status {
    PC_SOLVE_POLICY_NONE = 0,
    PC_SOLVE_POLICY_BOUNDED_FEASIBLE = 1,
    PC_SOLVE_POLICY_BOUNDED_NEAR_OPTIMAL = 2,
    PC_SOLVE_POLICY_EXACT = 3
} pc_solve_policy_status;

typedef enum pc_solve_termination {
    PC_SOLVE_TERMINATION_NONE = 0,
    PC_SOLVE_TERMINATION_REFUSED_RESOURCE_CAP = 1,
    PC_SOLVE_TERMINATION_TARGET_GAP = 2,
    PC_SOLVE_TERMINATION_EXACT_CLOSED = 3,
    PC_SOLVE_TERMINATION_NO_EXECUTABLE_POLICY = 4,
    PC_SOLVE_TERMINATION_NUMERICAL_STABILITY = 5,
    /* The stepped host stopped open discovery and requested normal bounded
     * policy finalization. This is not a resource cap or exact closure. */
    PC_SOLVE_TERMINATION_REQUESTED_BOUNDED_FINISH = 6
} pc_solve_termination;

/* Precise stopping cause is independent of policy availability. A capped
 * solve can therefore return either a bounded policy or no policy while
 * retaining the same stop cause. cap_hit_mask preserves every cap when more
 * than one was observed; stop_cause names the first deterministic cause. */
typedef enum pc_solve_stop_cause {
    PC_SOLVE_STOP_NONE = 0,
    PC_SOLVE_STOP_EXACT_CLOSED = 1,
    PC_SOLVE_STOP_TARGET_GAP = 2,
    PC_SOLVE_STOP_STATE_CAP = 3,
    PC_SOLVE_STOP_TRANSITION_CAP = 4,
    PC_SOLVE_STOP_MEMORY_CAP = 5,
    PC_SOLVE_STOP_SWEEP_CAP = 6,
    PC_SOLVE_STOP_REFORGE_WORK_CAP = 7,
    PC_SOLVE_STOP_STATE_ACTION_ROW_CAP = 8,
    PC_SOLVE_STOP_COMPILED_OUTPUT_CAP = 9,
    PC_SOLVE_STOP_OTHER_RESOURCE_CAP = 10,
    PC_SOLVE_STOP_NO_EXECUTABLE_POLICY = 11,
    PC_SOLVE_STOP_NUMERICAL_STABILITY = 12,
    PC_SOLVE_STOP_REQUESTED_BOUNDED_FINISH = 13
} pc_solve_stop_cause;

typedef enum pc_solve_cap_hit {
    PC_SOLVE_CAP_STATE = 1u << 0,
    PC_SOLVE_CAP_TRANSITIONS = 1u << 1,
    PC_SOLVE_CAP_MEMORY = 1u << 2,
    PC_SOLVE_CAP_SWEEPS = 1u << 3,
    PC_SOLVE_CAP_REFORGE_WORK = 1u << 4,
    PC_SOLVE_CAP_STATE_ACTION_ROWS = 1u << 5,
    PC_SOLVE_CAP_COMPILED_OUTPUT = 1u << 6,
    PC_SOLVE_CAP_OTHER = 1u << 31
} pc_solve_cap_hit;

typedef enum pc_solve_gap_target {
    PC_SOLVE_GAP_TARGET_NONE = 0,
    PC_SOLVE_GAP_TARGET_ABSOLUTE = 1,
    PC_SOLVE_GAP_TARGET_RELATIVE = 2,
    PC_SOLVE_GAP_TARGET_BOTH = 3
} pc_solve_gap_target;

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
    int32_t policy_available;
    int32_t policy_status; /* pc_solve_policy_status */
    int32_t termination; /* pc_solve_termination */
    double lower_bound;
    double upper_bound;
    double evaluated_policy_cost;
    double absolute_optimality_gap;
    double relative_optimality_gap; /* infinity when lower_bound <= 0 */
    double requested_absolute_optimality_gap;
    double requested_relative_optimality_gap;
    int32_t target_met;
    int32_t target_fired; /* pc_solve_gap_target */
    int32_t stop_cause; /* pc_solve_stop_cause; independent of policy */
    uint32_t cap_hit_mask; /* pc_solve_cap_hit bitset; every observed cap */
    uint32_t registry_action_count;
    uint32_t candidate_action_count;
    uint32_t evaluator_supported_action_count;
    uint32_t supported_priced_action_count;
    uint32_t skipped_missing_price_count;
    uint32_t skipped_unsupported_count;
} pc_solve_summary;

typedef enum pc_solve_phase {
    PC_SOLVE_PHASE_EXPANDING = 1,
    PC_SOLVE_PHASE_ITERATING = 2,
    PC_SOLVE_PHASE_DONE = 3,
    PC_SOLVE_PHASE_REFINING = 4,
    PC_SOLVE_PHASE_COMPILING = 5,
    PC_SOLVE_PHASE_CERTIFYING = 6
} pc_solve_phase;

typedef enum pc_solve_incumbent_kind {
    PC_SOLVE_INCUMBENT_NONE = 0,
    PC_SOLVE_INCUMBENT_CONSTRUCTIVE_FALLBACK = 1,
    PC_SOLVE_INCUMBENT_PROGRESSIVE_FRACTURE = 2,
    PC_SOLVE_INCUMBENT_DESTRUCTIVE_RENEWAL = 3,
    PC_SOLVE_INCUMBENT_DIRECT_EXECUTABLE_ROW = 4,
    PC_SOLVE_INCUMBENT_PARTIAL_UPPER_FALLBACK = 5,
    PC_SOLVE_INCUMBENT_PARTIAL_UPPER_PROGRESSIVE_FRACTURE = 6,
    PC_SOLVE_INCUMBENT_PARTIAL_UPPER_DESTRUCTIVE_RENEWAL = 7,
    PC_SOLVE_INCUMBENT_OTHER = 8
} pc_solve_incumbent_kind;

typedef struct pc_solve_progress {
    uint32_t struct_size;
    uint32_t abi_version;
    int32_t phase; /* pc_solve_phase */
    int32_t done;
    uint32_t expanded_states;
    uint32_t sweeps;
    double residual;
    double start_value_bound;
    double lower_bound;
    double upper_bound;
    double absolute_optimality_gap;
    double relative_optimality_gap; /* infinity when lower_bound <= 0 */
    /* B4 bounded-run observation fields. These are read-only snapshots and
     * never participate in Bellman comparisons or stopping decisions. */
    uint32_t focused_round;
    int32_t incumbent_kind; /* pc_solve_incumbent_kind */
    uint32_t discovered_states;
    uint32_t frontier_states;
    uint64_t state_action_rows;
    uint64_t transition_entries;
    /* Consumed V1-equivalent logical envelope, comparable to
     * pc_solve_options.max_reforge_work for every evaluator version. */
    uint64_t reforge_work;
    uint64_t live_owned_bytes;
    uint64_t peak_owned_bytes;
    /* Append-only native finalization progress. These counters are
     * observational and never participate in proof comparisons. */
    uint64_t finalization_work_items;
    uint32_t refinement_states;
    uint32_t refinement_kernels;
    uint64_t refinement_transitions;
    uint32_t refinement_rounds;
    uint32_t refinement_classes;
    uint64_t certification_discovered_pairs;
    uint64_t certification_pending_pairs;
    uint64_t certification_solved_sccs;
    uint64_t certification_total_sccs;
} pc_solve_progress;

/* Stateful solve surface. After non-null argument validation, begin releases
 * and resets the latest result before constructing replacement work, so a
 * later failed begin does not restore the previous result or its
 * compiled-strategy cache. step performs bounded expansion/sweep work; finish
 * transfers the already-certified policy after progress.done; abandon
 * discards partial retained refinement/certification work. */
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

/* Request a truthful anytime result from an active stepped solve. The next
 * step stops open graph/action discovery at its cooperative boundary and
 * enters the ordinary compile/certify/evaluate finalizer. This call does not
 * make an exactness claim and is distinct from abandon/cancellation. */
pc_result pc_solver_solve_request_bounded_finish(
    pc_solver_handle solver,
    pc_error_info* out_error);

pc_result pc_solver_solve_finish(
    pc_solver_handle solver,
    pc_solve_summary* out_summary,
    pc_error_info* out_error);

void pc_solver_solve_abandon(pc_solver_handle solver);

/*
 * Synchronous value iteration from the start item. The economy supplies
 * the price table (a null economy is invalid: costs are required).
 * Results are stored on the solver for the query calls below. A new solve
 * releases the previous result and compiled-strategy cache after non-null
 * argument validation and before replacement work begins; a later failed
 * replacement does not restore them. The active solve stores one sparse
 * transition graph and may reuse collision-checked exact action kernels
 * within the compatible session, goal layout, and action vocabulary.
 */
pc_result pc_solver_solve(
    pc_solver_handle solver,
    const pc_item_state* start_item,
    pc_economy_handle economy,
    const pc_solve_options* options,
    pc_solve_summary* out_summary,
    pc_error_info* out_error);

/*
 * Value and policy operator for one abstract state from the latest solve.
 * Primitive ids retain the action-registry vocabulary; fixed option ids begin
 * with "option:". out_action_id receives NULL for goal/terminal states. The
 * id points into solver-owned storage. A policy-guided exact strategy can
 * split one coarse state into subclasses with different values or actions;
 * in that case this coarse-state query returns
 * PC_RESULT_UNSUPPORTED_FEATURE and pc_solver_compile_strategy is the
 * executable policy authority. Returns PC_RESULT_NOT_FOUND before any solve.
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
 * docs/solver/crafting-solver-plan.md, Policy To Strategy Graph). Query-required-
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

/* Selected retained allocations owned by this solver handle. Unlike process
 * RSS or the Emscripten heap size, live_owned_bytes falls when solve work and
 * retained result/output owners are released. */
pc_result pc_solver_memory_stats(
    pc_solver_handle solver,
    pc_native_memory_stats* out_stats,
    pc_error_info* out_error);

#ifdef __cplusplus
}
#endif

#endif
