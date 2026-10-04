#ifndef POECRAFT_RECOMBINATION_SOLVER_H
#define POECRAFT_RECOMBINATION_SOLVER_H
#include "poecraft/recombination.h"
#ifdef __cplusplus
extern "C" {
#endif
#define PC_RECOMBINATION_SOLVER_VERSION 2u
typedef struct pc_recombination_solver* pc_recombination_solver_handle;
typedef struct pc_recombination_acquisition {
    const char* id;
    const char* source_kind; /* purchase | completed_feeder */
    const char* quote_identity;
    const pc_item_state* item; /* exact represented spec in request session */
    double total_cost_chaos; /* base + setup + all failures/retries + cleanup */
    uint32_t cost_complete;
    /* checked_feeder only: exact immutable child and paid start, never latest Stash */
    const char* feeder_strategy_id;
    const char* feeder_revision;
    const char* feeder_document_json;
    const char* feeder_output_contract_id;
    double feeder_paid_start_cost_chaos;
} pc_recombination_acquisition;
typedef struct pc_recombination_solver_options {
    uint32_t struct_size, abi_version, solver_version;
    const char* model_id; /* explicit native v1 ordinary or bounded v2 model ID */
    const char* price_identity;
    const char* goal_set_json;
    size_t goal_set_json_size;
    const pc_recombination_acquisition* acquisitions;
    uint32_t acquisition_count;
    const pc_item_state* initial_items; /* zero, one or two distinct physical items */
    const double* initial_item_costs; /* explicit input costs, included once */
    uint32_t initial_item_count;
    double recombination_cost_chaos;
    uint32_t recombination_cost_complete; /* caller quote includes gold/dust */
    uint32_t max_items, max_states, max_policy_iterations; /* 0 uses defaults */
    uint64_t max_work; /* 0 uses bounded native default */
    const char* scenario_id; /* required only for explicit analysis-only v3 */
    double prefix_first_a, prefix_first_b; /* no inferred default */
    uint32_t (*cancelled)(void* user); /* real native work/discovery interruption */
    void* cancellation_user;
    uint32_t allow_incomplete_costs; /* explicit feasible-resource report; no priced optimization/export */
    const char* feeder_economy_json;
    size_t feeder_economy_json_size;
} pc_recombination_solver_options;

/* Read-only: no RNG, acquisition, inventory or Builder mutation. The selected
 * session pins ONE base and level. Full item specs retain filler/blocker mods,
 * rolls and carrier properties; duplicate spec IDs do not alias physical items.
 * Gold/dust/attempt and all acquisition costs must be explicitly complete.
 * Costs are caller declarations, never certified by a provenance string.
 * Discovery/work caps refuse instead of dropping positive probability.
 * Returned policy is a bounded proposal evaluated under the declared model.
 * Neither hidden game odds nor global crafting optimality are certified. */
pc_result pc_recombination_solver_create(pc_session_handle session,
    const pc_recombination_solver_options* options,
    pc_recombination_solver_handle* out_solver, pc_error_info* out_error);
void pc_recombination_solver_destroy(pc_recombination_solver_handle solver);
/* Query required length, excludes NUL; immutable precomputed result. */
pc_result pc_recombination_solver_result_json(pc_recombination_solver_handle solver,
    char* buffer, size_t buffer_size, size_t* out_length, pc_error_info* out_error);
/* Independent session ownership, including every result spec's dense mapping. */
/* Restricted checked export bundle: Builder document, pinned scenario economy,
 * independent full-item/control evaluation. Unchecked child quotes and v3 refuse. */
pc_result pc_recombination_solver_export_json(pc_recombination_solver_handle solver,
    char* buffer, size_t buffer_size, size_t* out_length, pc_error_info* out_error);
pc_result pc_recombination_solver_check_export_json(pc_recombination_solver_handle solver,
    const char* strategy_json, size_t strategy_json_size,
    char* buffer, size_t buffer_size, size_t* out_length, pc_error_info* out_error);
pc_result pc_recombination_solver_session(pc_recombination_solver_handle solver,
    pc_session_handle* out_session, pc_error_info* out_error);
pc_result pc_recombination_solver_item(pc_recombination_solver_handle solver,
    uint32_t item_spec_id, pc_item_state* out_item, pc_error_info* out_error);
#ifdef __cplusplus
}
#endif
#endif
