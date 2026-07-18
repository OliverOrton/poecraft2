#ifndef POECRAFT_SIMULATOR_H
#define POECRAFT_SIMULATOR_H

#include <stddef.h>
#include <stdint.h>

#include "poecraft/api.h"
#include "poecraft/item_state.h"
#include "poecraft/result.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Cold-path compilation of persistent strategy JSON. The strategy is resolved
 * against one immutable session, so stable base/mod/group keys become dense
 * runtime ids once and every binding executes the same compiled graph.
 */
pc_result pc_strategy_compile_json(
    pc_session_handle session,
    const char* strategy_json,
    size_t strategy_json_size,
    pc_strategy_handle* out_strategy,
    pc_error_info* out_error);

void pc_strategy_destroy(pc_strategy_handle strategy);

/*
 * Economy JSON v1:
 *   {"version":"v1","id":"optional","prices":{"chaos":1.0,...}}
 *
 * Canonical parameterized keys are "essence:<metadata-key>",
 * "fossil:<metadata-key>", and "resonator:<socket-count>". Fossil operations
 * sum every selected fossil plus the matching resonator. Prices are
 * chaos-equivalent.
 */
pc_result pc_economy_load_json(
    const char* economy_json,
    size_t economy_json_size,
    pc_economy_handle* out_economy,
    pc_error_info* out_error);

void pc_economy_destroy(pc_economy_handle economy);

typedef enum pc_terminal_kind {
    PC_TERMINAL_SUCCESS = 0,
    PC_TERMINAL_FAILURE = 1,
    PC_TERMINAL_STOP = 2
} pc_terminal_kind;

typedef enum pc_strategy_node_kind {
    PC_STRATEGY_NODE_START = 0,
    PC_STRATEGY_NODE_OPERATION = 1,
    PC_STRATEGY_NODE_ROUTER = 2,
    PC_STRATEGY_NODE_TERMINAL = 3
} pc_strategy_node_kind;

typedef enum pc_simulation_failure_reason {
    PC_SIM_FAILURE_NONE = 0,
    PC_SIM_FAILURE_TERMINAL = 1,
    PC_SIM_FAILURE_ACTION_LIMIT = 2,
    PC_SIM_FAILURE_COST_LIMIT = 3,
    PC_SIM_FAILURE_STEP_LIMIT = 4,
    PC_SIM_FAILURE_NO_MATCHING_EDGE = 5,
    PC_SIM_FAILURE_ACTION_NOT_APPLIED = 6,
    PC_SIM_FAILURE_MISSING_PRICE = 7
} pc_simulation_failure_reason;

typedef enum pc_cost_status {
    PC_COST_DISABLED = 0,
    PC_COST_COMPLETE = 1,
    PC_COST_INCOMPLETE = 2
} pc_cost_status;

/*
 * Options are fixed when the first chunk is run and must remain identical for
 * later chunks. max_cost_per_run <= 0 disables the cost limit. A zero
 * max_graph_steps_per_run selects an engine default derived from the action
 * limit and graph size.
 */
typedef struct pc_simulation_options {
    uint32_t struct_size;
    uint32_t abi_version;
    uint64_t target_runs;
    uint64_t seed;
    uint32_t max_actions_per_run;
    uint32_t max_graph_steps_per_run;
    double max_cost_per_run;
    uint32_t retained_trace_count;
    uint32_t max_trace_entries;
    uint32_t retained_success_count;
    uint32_t retained_failure_count;
} pc_simulation_options;

typedef struct pc_simulation_progress {
    uint32_t struct_size;
    uint32_t abi_version;
    uint64_t completed_runs;
    uint64_t target_runs;
    int32_t finished;
} pc_simulation_progress;

typedef struct pc_simulation_summary {
    uint32_t struct_size;
    uint32_t abi_version;
    uint64_t completed_runs;
    uint64_t success_count;
    uint64_t failure_count;
    uint64_t stop_count;
    uint64_t total_actions;
    uint64_t action_limit_count;
    uint64_t cost_limit_count;
    uint64_t step_limit_count;
    uint64_t no_matching_edge_count;
    uint64_t action_not_applied_count;
    uint64_t missing_price_run_count;
    uint64_t costed_action_count;
    uint64_t missing_price_action_count;
    double known_total_cost;
    int32_t cost_status; /* pc_cost_status */
    uint64_t seed;
    uint64_t target_runs;
} pc_simulation_summary;

typedef struct pc_trace_entry {
    uint32_t struct_size;
    uint32_t abi_version;
    uint32_t step_index;
    const char* node_id;
    int32_t node_kind; /* pc_strategy_node_kind */
    int32_t action_type; /* pc_action_type, or -1 */
    int32_t action_applied;
    const char* matched_edge_id; /* empty for terminal/failed transition */
    uint64_t cumulative_actions;
    double known_cumulative_cost;
    int32_t cost_complete;
    int32_t terminal_kind; /* pc_terminal_kind, or -1 */
    int32_t failure_reason; /* pc_simulation_failure_reason */
    pc_item_state item;
} pc_trace_entry;

typedef struct pc_simulation_example {
    uint32_t struct_size;
    uint32_t abi_version;
    int32_t terminal_kind; /* pc_terminal_kind */
    int32_t failure_reason; /* pc_simulation_failure_reason */
    const char* terminal_node_id;
    uint64_t action_count;
    double known_total_cost;
    int32_t cost_complete;
    pc_item_state item;
} pc_simulation_example;

typedef struct pc_failure_summary_entry {
    uint32_t struct_size;
    uint32_t abi_version;
    int32_t failure_reason; /* pc_simulation_failure_reason */
    const char* node_id;
    const char* detail;
    uint64_t count;
} pc_failure_summary_entry;

typedef struct pc_action_distribution_entry {
    uint32_t struct_size;
    uint32_t abi_version;
    const char* node_id;
    int32_t action_type; /* pc_action_type */
    uint64_t count;
} pc_action_distribution_entry;

typedef struct pc_price_key_entry {
    uint32_t struct_size;
    uint32_t abi_version;
    const char* key;
    uint64_t missing_count;
} pc_price_key_entry;

typedef struct pc_action_descriptor_sample_entry {
    uint32_t struct_size;
    uint32_t abi_version;
    const char* action_id;
    uint64_t count;
} pc_action_descriptor_sample_entry;

typedef struct pc_material_sample_entry {
    uint32_t struct_size;
    uint32_t abi_version;
    const char* price_key;
    uint64_t count;
} pc_material_sample_entry;

pc_result pc_simulator_create(
    pc_session_handle session,
    pc_strategy_handle strategy,
    pc_economy_handle economy,
    pc_simulator_handle* out_simulator,
    pc_error_info* out_error);

/*
 * Complete at most max_completed_runs. Browser workers call bounded chunks and
 * yield between calls for progress/cancellation. A single run is still bounded
 * by the action/step/cost limits above.
 */
pc_result pc_simulator_run_chunk(
    pc_simulator_handle simulator,
    const pc_simulation_options* options,
    uint32_t max_completed_runs,
    pc_simulation_progress* out_progress,
    pc_error_info* out_error);

pc_result pc_simulator_get_summary(
    pc_simulator_handle simulator,
    pc_simulation_summary* out_summary,
    pc_error_info* out_error);

/*
 * Variable-length queries use the required-count pattern. Returned string
 * pointers are owned by the simulator and remain valid until the next mutating
 * run_chunk call or simulator destruction.
 */
pc_result pc_simulator_missing_price_query(
    pc_simulator_handle simulator,
    pc_price_key_entry* entries,
    uint32_t entry_capacity,
    uint32_t* out_entry_count,
    pc_error_info* out_error);

pc_result pc_simulator_get_trace_count(
    pc_simulator_handle simulator,
    uint32_t* out_trace_count,
    pc_error_info* out_error);

pc_result pc_simulator_trace_query(
    pc_simulator_handle simulator,
    uint32_t trace_index,
    pc_trace_entry* entries,
    uint32_t entry_capacity,
    uint32_t* out_entry_count,
    pc_error_info* out_error);

pc_result pc_simulator_get_example_count(
    pc_simulator_handle simulator,
    int32_t terminal_kind,
    uint32_t* out_example_count,
    pc_error_info* out_error);

pc_result pc_simulator_example_query(
    pc_simulator_handle simulator,
    int32_t terminal_kind,
    uint32_t example_index,
    pc_simulation_example* out_example,
    pc_error_info* out_error);

pc_result pc_simulator_failure_summary_query(
    pc_simulator_handle simulator,
    pc_failure_summary_entry* entries,
    uint32_t entry_capacity,
    uint32_t* out_entry_count,
    pc_error_info* out_error);

pc_result pc_simulator_action_distribution_query(
    pc_simulator_handle simulator,
    pc_action_distribution_entry* entries,
    uint32_t entry_capacity,
    uint32_t* out_entry_count,
    pc_error_info* out_error);

/* S8.4 sampled evidence. These are raw Simulator counters aggregated by the
 * same stable descriptor ids and price keys used by exact accounting. */
pc_result pc_simulator_action_descriptor_distribution_query(
    pc_simulator_handle simulator,
    pc_action_descriptor_sample_entry* entries,
    uint32_t entry_capacity,
    uint32_t* out_entry_count,
    pc_error_info* out_error);

pc_result pc_simulator_material_distribution_query(
    pc_simulator_handle simulator,
    pc_material_sample_entry* entries,
    uint32_t entry_capacity,
    uint32_t* out_entry_count,
    pc_error_info* out_error);

void pc_simulator_destroy(pc_simulator_handle simulator);

#ifdef __cplusplus
}
#endif

#endif
