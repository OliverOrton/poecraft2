#include "tests.hpp"

#include <poecraft/api.h>
#include <poecraft/session.h>
#include <poecraft/simulator.h>

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace {

const char* kBase = "Metadata/Items/Armours/BodyArmours/BodyInt17";

std::string repeat_strategy(int required_prefixes) {
    return std::string(R"JSON({
  "version":"v1",
  "name":"Chaos until prefix count",
  "start_node_id":"start",
  "base_state":{
    "base_key":"Metadata/Items/Armours/BodyArmours/BodyInt17",
    "item_level":86,
    "rarity":"rare"
  },
  "nodes":[
    {"id":"start","kind":"start"},
    {"id":"chaos","kind":"operation","operation":{"type":"chaos","params":{}}},
    {"id":"success","kind":"terminal","terminal":"success"},
    {"id":"failure","kind":"terminal","terminal":"failure","reason":"test failure"}
  ],
  "edges":[
    {"id":"begin","from":"start","to":"chaos","priority":0,
     "condition":{"type":"always"}},
    {"id":"done","from":"chaos","to":"success","priority":0,
     "condition":{"type":"prefix_count_range","min":)JSON") +
           std::to_string(required_prefixes) +
           R"JSON(,"max":)JSON" + std::to_string(required_prefixes) +
           R"JSON(}},
    {"id":"repeat","from":"chaos","to":"chaos","priority":999,
     "is_default":true}
  ]
})JSON";
}

const char* condition_strategy() {
    return R"JSON({
  "version":"v1",
  "name":"Initial condition vocabulary",
  "start_node_id":"start",
  "base_state":{
    "base_key":"Metadata/Items/Armours/BodyArmours/BodyInt17",
    "item_level":86,
    "rarity":"rare"
  },
  "nodes":[
    {"id":"start","kind":"start"},
    {"id":"success","kind":"terminal","terminal":"success"}
  ],
  "edges":[{
    "id":"conditions","from":"start","to":"success","priority":0,
    "condition":{"type":"all","conditions":[
      {"type":"rarity_is","rarity":"rare"},
      {"type":"open_prefix_count","min":3,"max":3},
      {"type":"open_suffix_count","min":3,"max":3},
      {"type":"prefix_count_range","min":0,"max":0},
      {"type":"suffix_count_range","min":0,"max":0},
      {"type":"not","conditions":[
        {"type":"has_mod_group","group":"AdditionalArrows"}
      ]},
      {"type":"any","conditions":[
        {"type":"always"},
        {"type":"has_mod_group","group":"AdditionalArrows"}
      ]},
      {"type":"at_least","count":1,"conditions":[
        {"type":"always"},
        {"type":"has_mod_group","group":"AdditionalArrows"}
      ]}
    ]}
  }]
})JSON";
}

pc_simulation_options options(std::uint64_t runs) {
    pc_simulation_options value{};
    value.struct_size = sizeof(value);
    value.abi_version = PC_ABI_VERSION;
    value.target_runs = runs;
    value.seed = 42;
    value.max_actions_per_run = 100;
    value.max_graph_steps_per_run = 0;
    value.max_cost_per_run = 0.0;
    value.retained_trace_count = 3;
    value.max_trace_entries = 128;
    value.retained_success_count = 2;
    value.retained_failure_count = 2;
    return value;
}

} // namespace

void run_simulator_tests(const char* artifact_dir) {
    if (artifact_dir == nullptr) return;

    const std::string manifest =
        std::string(artifact_dir) + "/manifest.json";
    pc_error_info error{};
    pc_error_info_init(&error);
    pc_data_handle data = nullptr;
    PC_CHECK(pc_data_load_file(manifest.c_str(), &data, &error) ==
             PC_RESULT_OK);
    if (data == nullptr) return;

    pc_session_options session_options{};
    session_options.struct_size = sizeof(session_options);
    session_options.abi_version = PC_ABI_VERSION;
    session_options.base_metadata_path = kBase;
    session_options.item_level = 86;
    pc_session_handle session = nullptr;
    PC_CHECK(pc_session_create(data, &session_options, &session, &error) ==
             PC_RESULT_OK);
    if (session == nullptr) {
        pc_data_destroy(data);
        return;
    }

    const std::string json = repeat_strategy(3);
    pc_strategy_handle strategy = nullptr;
    PC_CHECK(pc_strategy_compile_json(
                 session, json.data(), json.size(), &strategy, &error) ==
             PC_RESULT_OK);

    const std::string economy_json =
        R"JSON({"version":"v1","id":"test","prices":{"chaos":2.0}})JSON";
    pc_economy_handle economy = nullptr;
    PC_CHECK(pc_economy_load_json(
                 economy_json.data(), economy_json.size(), &economy, &error) ==
             PC_RESULT_OK);

    pc_simulator_handle simulator = nullptr;
    PC_CHECK(pc_simulator_create(
                 session, strategy, economy, &simulator, &error) ==
             PC_RESULT_OK);
    auto sim_options = options(25);
    pc_simulation_progress progress{};
    do {
        PC_CHECK(pc_simulator_run_chunk(
                     simulator, &sim_options, 7, &progress, &error) ==
                 PC_RESULT_OK);
    } while (!progress.finished);
    PC_CHECK(progress.completed_runs == 25);

    pc_simulation_summary summary{};
    PC_CHECK(pc_simulator_get_summary(simulator, &summary, &error) ==
             PC_RESULT_OK);
    PC_CHECK(summary.success_count == 25);
    PC_CHECK(summary.failure_count == 0);
    PC_CHECK(summary.total_actions > summary.completed_runs);
    PC_CHECK(summary.cost_status == PC_COST_COMPLETE);
    PC_CHECK(std::abs(
                 summary.known_total_cost -
                 static_cast<double>(summary.total_actions) * 2.0) <
             0.0001);

    std::uint32_t trace_count = 0;
    PC_CHECK(pc_simulator_get_trace_count(
                 simulator, &trace_count, &error) == PC_RESULT_OK);
    PC_CHECK(trace_count == 3);
    std::uint32_t trace_size = 0;
    PC_CHECK(pc_simulator_trace_query(
                 simulator, 0, nullptr, 0, &trace_size, &error) ==
             PC_RESULT_BUFFER_TOO_SMALL);
    std::vector<pc_trace_entry> trace(trace_size);
    PC_CHECK(pc_simulator_trace_query(
                 simulator, 0, trace.data(), trace.size(), &trace_size,
                 &error) == PC_RESULT_OK);
    PC_CHECK(trace_size >= 3);
    PC_CHECK(std::string(trace.front().node_id) == "start");
    PC_CHECK(std::string(trace.front().matched_edge_id) == "begin");
    PC_CHECK(trace.back().terminal_kind == PC_TERMINAL_SUCCESS);

    std::uint32_t example_count = 0;
    PC_CHECK(pc_simulator_get_example_count(
                 simulator, PC_TERMINAL_SUCCESS, &example_count, &error) ==
             PC_RESULT_OK);
    PC_CHECK(example_count == 2);
    pc_simulation_example example{};
    PC_CHECK(pc_simulator_example_query(
                 simulator, PC_TERMINAL_SUCCESS, 0, &example, &error) ==
             PC_RESULT_OK);
    PC_CHECK(example.item.prefix_count == 3);

    pc_simulator_destroy(simulator);

    // Missing prices do not block an unbudgeted run, but cost status and the
    // canonical missing key remain queryable.
    const std::string empty_economy =
        R"JSON({"version":"v1","prices":{}})JSON";
    pc_economy_handle missing_economy = nullptr;
    PC_CHECK(pc_economy_load_json(
                 empty_economy.data(), empty_economy.size(),
                 &missing_economy, &error) == PC_RESULT_OK);
    pc_simulator_handle missing_simulator = nullptr;
    PC_CHECK(pc_simulator_create(
                 session, strategy, missing_economy, &missing_simulator,
                 &error) == PC_RESULT_OK);
    auto missing_options = options(2);
    PC_CHECK(pc_simulator_run_chunk(
                 missing_simulator, &missing_options, 2, &progress, &error) ==
             PC_RESULT_OK);
    PC_CHECK(pc_simulator_get_summary(
                 missing_simulator, &summary, &error) == PC_RESULT_OK);
    PC_CHECK(summary.cost_status == PC_COST_INCOMPLETE);
    PC_CHECK(summary.missing_price_action_count > 0);
    std::uint32_t missing_count = 0;
    PC_CHECK(pc_simulator_missing_price_query(
                 missing_simulator, nullptr, 0, &missing_count, &error) ==
             PC_RESULT_BUFFER_TOO_SMALL);
    PC_CHECK(missing_count == 1);
    pc_price_key_entry missing{};
    PC_CHECK(pc_simulator_missing_price_query(
                 missing_simulator, &missing, 1, &missing_count, &error) ==
             PC_RESULT_OK);
    PC_CHECK(std::string(missing.key) == "chaos");
    pc_simulator_destroy(missing_simulator);

    pc_simulator_handle budget_simulator = nullptr;
    PC_CHECK(pc_simulator_create(
                 session, strategy, economy, &budget_simulator, &error) ==
             PC_RESULT_OK);
    auto budget_options = options(2);
    budget_options.max_cost_per_run = 1.0;
    PC_CHECK(pc_simulator_run_chunk(
                 budget_simulator, &budget_options, 2, &progress, &error) ==
             PC_RESULT_OK);
    PC_CHECK(pc_simulator_get_summary(
                 budget_simulator, &summary, &error) == PC_RESULT_OK);
    PC_CHECK(summary.cost_limit_count == 2);
    PC_CHECK(summary.total_actions == 0);
    pc_simulator_destroy(budget_simulator);

    pc_simulator_handle missing_budget_simulator = nullptr;
    PC_CHECK(pc_simulator_create(
                 session, strategy, missing_economy,
                 &missing_budget_simulator, &error) == PC_RESULT_OK);
    auto missing_budget_options = options(1);
    missing_budget_options.max_cost_per_run = 10.0;
    PC_CHECK(pc_simulator_run_chunk(
                 missing_budget_simulator, &missing_budget_options, 1,
                 &progress, &error) == PC_RESULT_OK);
    PC_CHECK(pc_simulator_get_summary(
                 missing_budget_simulator, &summary, &error) == PC_RESULT_OK);
    PC_CHECK(summary.failure_count == 1);
    PC_CHECK(summary.total_actions == 0);
    PC_CHECK(summary.missing_price_run_count == 1);
    pc_simulator_destroy(missing_budget_simulator);
    pc_economy_destroy(missing_economy);

    // An impossible success condition is terminated by the run-wide action
    // limit and summarized without retaining every failed run.
    const std::string limited_json = repeat_strategy(4);
    pc_strategy_handle limited_strategy = nullptr;
    PC_CHECK(pc_strategy_compile_json(
                 session, limited_json.data(), limited_json.size(),
                 &limited_strategy, &error) == PC_RESULT_OK);
    pc_simulator_handle limited_simulator = nullptr;
    PC_CHECK(pc_simulator_create(
                 session, limited_strategy, economy, &limited_simulator,
                 &error) == PC_RESULT_OK);
    auto limited_options = options(3);
    limited_options.max_actions_per_run = 2;
    PC_CHECK(pc_simulator_run_chunk(
                 limited_simulator, &limited_options, 3, &progress, &error) ==
             PC_RESULT_OK);
    PC_CHECK(pc_simulator_get_summary(
                 limited_simulator, &summary, &error) == PC_RESULT_OK);
    PC_CHECK(summary.failure_count == 3);
    PC_CHECK(summary.action_limit_count == 3);
    std::uint32_t failure_count = 0;
    PC_CHECK(pc_simulator_failure_summary_query(
                 limited_simulator, nullptr, 0, &failure_count, &error) ==
             PC_RESULT_BUFFER_TOO_SMALL);
    PC_CHECK(failure_count == 1);
    pc_failure_summary_entry failure{};
    PC_CHECK(pc_simulator_failure_summary_query(
                 limited_simulator, &failure, 1, &failure_count, &error) ==
             PC_RESULT_OK);
    PC_CHECK(failure.failure_reason == PC_SIM_FAILURE_ACTION_LIMIT);

    pc_simulator_destroy(limited_simulator);
    pc_strategy_destroy(limited_strategy);

    pc_strategy_handle condition_graph = nullptr;
    const std::string conditions_json = condition_strategy();
    PC_CHECK(pc_strategy_compile_json(
                 session, conditions_json.data(), conditions_json.size(),
                 &condition_graph, &error) == PC_RESULT_OK);
    pc_simulator_handle condition_simulator = nullptr;
    PC_CHECK(pc_simulator_create(
                 session, condition_graph, nullptr, &condition_simulator,
                 &error) == PC_RESULT_OK);
    auto condition_options = options(1);
    condition_options.retained_trace_count = 1;
    PC_CHECK(pc_simulator_run_chunk(
                 condition_simulator, &condition_options, 1, &progress,
                 &error) == PC_RESULT_OK);
    PC_CHECK(pc_simulator_get_summary(
                 condition_simulator, &summary, &error) == PC_RESULT_OK);
    PC_CHECK(summary.success_count == 1);
    PC_CHECK(summary.total_actions == 0);
    PC_CHECK(summary.cost_status == PC_COST_DISABLED);
    pc_simulator_destroy(condition_simulator);
    pc_strategy_destroy(condition_graph);

    pc_economy_destroy(economy);
    pc_strategy_destroy(strategy);
    pc_session_destroy(session);
    pc_data_destroy(data);
}
