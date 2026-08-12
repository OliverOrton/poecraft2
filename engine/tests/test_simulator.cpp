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

std::string metamod_renewal_strategy(bool fossil) {
    const std::string operation =
        fossil
            ? R"JSON({"type":"fossil","params":{"fossils":["Metadata/Items/Currency/CurrencyDelveCraftingRandom"]}})JSON"
            : R"JSON({"type":"essence","params":{"essence_key":"Metadata/Items/Currency/CurrencyEssenceAnguish2"}})JSON";
    return std::string(R"JSON({
  "version":"v1",
  "name":"S8.2 metamod-ignoring renewal",
  "start_node_id":"start",
  "base_state":{
    "base_key":"Metadata/Items/Armours/BodyArmours/BodyInt17",
    "item_level":86,
    "rarity":"rare",
    "prefixes":[
      {"mod_key":"DexMasterItemGenerationCannotChangeSuffixes","crafted":true},
      {"mod_key":"LocalIncreasedEnergyShield11","fractured":true}
    ],
    "suffixes":[
      {"mod_key":"StrMasterItemGenerationCannotChangePrefixes","crafted":true}
    ]
  },
  "nodes":[
    {"id":"start","kind":"start"},
    {"id":"renew","kind":"operation","operation":)JSON") +
           operation + R"JSON(},
    {"id":"success","kind":"terminal","terminal":"success"}
  ],
  "edges":[
    {"id":"begin","from":"start","to":"renew"},
    {"id":"done","from":"renew","to":"success"}
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

std::string family_tier_strategy(
    const std::string& family_key,
    const std::string& item_mod_key,
    int generation_type,
    int min_tier,
    bool item_fractured = false,
    bool require_fractured = false) {
    const char* side = generation_type == PC_SIDE_PREFIX ? "prefixes" : "suffixes";
    const std::string item_flags =
        item_fractured ? R"JSON(,"fractured":true)JSON" : "";
    const std::string condition_flags =
        require_fractured ? R"JSON(,"fractured":true)JSON" : "";
    return std::string(R"JSON({
  "version":"v1",
  "name":"Modifier family tier",
  "start_node_id":"start",
  "base_state":{
    "base_key":"Metadata/Items/Armours/BodyArmours/BodyInt17",
    "item_level":86,
    "rarity":"rare",
    ")JSON") +
           side + R"JSON(": [{"mod_key":")JSON" + item_mod_key +
           R"JSON(")JSON" + item_flags + R"JSON(}]
  },
  "nodes":[
    {"id":"start","kind":"start"},
    {"id":"success","kind":"terminal","terminal":"success"},
    {"id":"failure","kind":"terminal","terminal":"failure"}
  ],
  "edges":[
    {"id":"match","from":"start","to":"success","priority":0,
     "condition":{"type":"has_mod_family","family_mod_key":")JSON" +
           family_key + R"JSON(","min_tier":)JSON" +
           std::to_string(min_tier) + condition_flags + R"JSON(}},
    {"id":"fallback","from":"start","to":"failure","priority":999,
     "is_default":true}
  ]
})JSON";
}

std::string influence_exalt_strategy(const std::string& influence) {
    return std::string(R"JSON({
  "version":"v1",
  "name":"Influence Exalt alias",
  "start_node_id":"start",
  "base_state":{
    "base_key":"Metadata/Items/Armours/BodyArmours/BodyInt17",
    "item_level":86,
    "rarity":"rare"
  },
  "nodes":[
    {"id":"start","kind":"start"},
    {"id":"craft","kind":"operation","operation":{
      "type":"influence_exalt","params":{"influence":")JSON") +
           influence + R"JSON("}}},
    {"id":"success","kind":"terminal","terminal":"success"}
  ],
  "edges":[
    {"id":"begin","from":"start","to":"craft"},
    {"id":"done","from":"craft","to":"success"}
  ]
})JSON";
}

std::string family_count_strategy(
    const std::string& family_key,
    const std::string& item_mod_key,
    int generation_type) {
    const char* side = generation_type == PC_SIDE_PREFIX ? "prefixes" : "suffixes";
    return std::string(R"JSON({
  "version":"v1",
  "name":"Modifier family count",
  "start_node_id":"start",
  "base_state":{
    "base_key":"Metadata/Items/Armours/BodyArmours/BodyInt17",
    "item_level":86,
    "rarity":"rare",
    ")JSON") +
           side + R"JSON(": [{"mod_key":")JSON" + item_mod_key +
           R"JSON("}]
  },
  "nodes":[
    {"id":"start","kind":"start"},
    {"id":"success","kind":"terminal","terminal":"success"},
    {"id":"failure","kind":"terminal","terminal":"failure"}
  ],
  "edges":[
    {"id":"match","from":"start","to":"success","priority":0,
     "condition":{"type":"mod_family_count","family_mod_keys":[")JSON" +
           family_key + R"JSON("],"min":1,"max":1}},
    {"id":"fallback","from":"start","to":"failure","priority":999,
     "is_default":true}
  ]
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
    std::uint32_t distribution_count = 0;
    PC_CHECK(pc_simulator_action_distribution_query(
                 simulator, nullptr, 0, &distribution_count, &error) ==
             PC_RESULT_BUFFER_TOO_SMALL);
    PC_CHECK(distribution_count == 1);
    pc_action_distribution_entry distribution{};
    PC_CHECK(pc_simulator_action_distribution_query(
                 simulator, &distribution, 1, &distribution_count, &error) ==
             PC_RESULT_OK);
    PC_CHECK(std::string(distribution.node_id) == "chaos");
    PC_CHECK(distribution.action_type == PC_ACTION_CHAOS);
    PC_CHECK(distribution.count == summary.total_actions);

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

    /* Legacy internal names parse, but accounting is normalized to the
     * canonical public currency key. Elder and Shaper remain generic
     * influences rather than synthetic currency operations. */
    {
        const std::string alias_json =
            influence_exalt_strategy("adjudicator");
        pc_strategy_handle alias_strategy = nullptr;
        PC_CHECK(pc_strategy_compile_json(
                     session, alias_json.data(), alias_json.size(),
                     &alias_strategy, &error) == PC_RESULT_OK);
        const std::string influence_economy_json =
            R"JSON({"version":"v1","prices":{"influence_exalt:warlord":7.0}})JSON";
        pc_economy_handle influence_economy = nullptr;
        PC_CHECK(pc_economy_load_json(
                     influence_economy_json.data(),
                     influence_economy_json.size(), &influence_economy,
                     &error) == PC_RESULT_OK);
        pc_simulator_handle alias_simulator = nullptr;
        PC_CHECK(pc_simulator_create(
                     session, alias_strategy, influence_economy,
                     &alias_simulator, &error) == PC_RESULT_OK);
        auto alias_options = options(1);
        PC_CHECK(pc_simulator_run_chunk(
                     alias_simulator, &alias_options, 1, &progress,
                     &error) == PC_RESULT_OK);
        PC_CHECK(pc_simulator_get_summary(
                     alias_simulator, &summary, &error) == PC_RESULT_OK);
        PC_CHECK(summary.success_count == 1);
        PC_CHECK(summary.cost_status == PC_COST_COMPLETE);
        PC_CHECK(std::abs(summary.known_total_cost - 7.0) < 1e-12);
        std::uint32_t material_count = 0;
        PC_CHECK(pc_simulator_material_distribution_query(
                     alias_simulator, nullptr, 0, &material_count,
                     &error) == PC_RESULT_BUFFER_TOO_SMALL);
        PC_CHECK(material_count == 1);
        pc_material_sample_entry material{};
        PC_CHECK(pc_simulator_material_distribution_query(
                     alias_simulator, &material, 1, &material_count,
                     &error) == PC_RESULT_OK);
        PC_CHECK(std::string(material.price_key) ==
                 "influence_exalt:warlord");
        PC_CHECK(material.count == 1);
        pc_simulator_destroy(alias_simulator);
        pc_economy_destroy(influence_economy);
        pc_strategy_destroy(alias_strategy);

        for (const char* unsupported : {"elder", "shaper"}) {
            const std::string unsupported_json =
                influence_exalt_strategy(unsupported);
            pc_strategy_handle unsupported_strategy = nullptr;
            PC_CHECK(pc_strategy_compile_json(
                         session, unsupported_json.data(),
                         unsupported_json.size(), &unsupported_strategy,
                         &error) == PC_RESULT_DATA_ERROR);
            PC_CHECK(unsupported_strategy == nullptr);
        }
    }

    /* S8.2 owner correction through the compiled Simulator path: both
     * renewal operations destroy the two ordinary lock crafts, while the
     * independent fractured affix survives. */
    std::uint32_t prefix_lock_id = UINT32_MAX;
    std::uint32_t suffix_lock_id = UINT32_MAX;
    std::uint32_t fractured_id = UINT32_MAX;
    std::uint32_t session_mod_count = 0;
    PC_CHECK(pc_session_get_mod_count(
                 session, &session_mod_count, &error) == PC_RESULT_OK);
    for (std::uint32_t id = 0; id < session_mod_count; ++id) {
        pc_mod_info info{};
        info.struct_size = sizeof(info);
        info.abi_version = PC_ABI_VERSION;
        PC_CHECK(pc_session_get_mod_info(session, id, &info, &error) ==
                 PC_RESULT_OK);
        const std::string key = info.key;
        if (key == "StrMasterItemGenerationCannotChangePrefixes") {
            prefix_lock_id = id;
        } else if (key ==
                   "DexMasterItemGenerationCannotChangeSuffixes") {
            suffix_lock_id = id;
        } else if (key == "LocalIncreasedEnergyShield11") {
            fractured_id = id;
        }
    }
    PC_CHECK(prefix_lock_id != UINT32_MAX);
    PC_CHECK(suffix_lock_id != UINT32_MAX);
    PC_CHECK(fractured_id != UINT32_MAX);
    for (const bool fossil : {false, true}) {
        const std::string renewal_json =
            metamod_renewal_strategy(fossil);
        pc_strategy_handle renewal_strategy = nullptr;
        PC_CHECK(pc_strategy_compile_json(
                     session, renewal_json.data(), renewal_json.size(),
                     &renewal_strategy, &error) == PC_RESULT_OK);
        pc_simulator_handle renewal_simulator = nullptr;
        PC_CHECK(pc_simulator_create(
                     session, renewal_strategy, nullptr,
                     &renewal_simulator, &error) == PC_RESULT_OK);
        auto renewal_options = options(1);
        renewal_options.retained_success_count = 1;
        PC_CHECK(pc_simulator_run_chunk(
                     renewal_simulator, &renewal_options, 1, &progress,
                     &error) == PC_RESULT_OK);
        pc_simulation_example renewal_example{};
        PC_CHECK(pc_simulator_example_query(
                     renewal_simulator, PC_TERMINAL_SUCCESS, 0,
                     &renewal_example, &error) == PC_RESULT_OK);
        bool saw_fractured = false;
        bool saw_lock = false;
        const auto inspect = [&](const pc_mod_slot* slots,
                                 std::uint8_t count) {
            for (std::uint8_t i = 0; i < count; ++i) {
                saw_lock |= slots[i].mod_id == prefix_lock_id ||
                            slots[i].mod_id == suffix_lock_id;
                saw_fractured |=
                    slots[i].mod_id == fractured_id &&
                    (slots[i].flags & PC_MOD_SLOT_FRACTURED) != 0;
            }
        };
        inspect(renewal_example.item.prefixes,
                renewal_example.item.prefix_count);
        inspect(renewal_example.item.suffixes,
                renewal_example.item.suffix_count);
        PC_CHECK(!saw_lock);
        PC_CHECK(saw_fractured);
        pc_simulator_destroy(renewal_simulator);
        pc_strategy_destroy(renewal_strategy);
    }

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

    // A selected display family can require Tn or better. T2 satisfies T2+
    // but not T1+, matching the old editor's minimum-tier behavior without
    // relying on required-level heuristics.
    std::uint32_t mod_count = 0;
    PC_CHECK(pc_session_get_mod_count(session, &mod_count, &error) ==
             PC_RESULT_OK);
    std::uint32_t selected_family = UINT32_MAX;
    int selected_side = -1;
    std::string tier_one_key;
    std::string tier_two_key;
    for (std::uint32_t i = 0; i < mod_count && tier_two_key.empty(); ++i) {
        pc_mod_info info{};
        info.struct_size = sizeof(info);
        info.abi_version = PC_ABI_VERSION;
        PC_CHECK(pc_session_get_mod_info(session, i, &info, &error) ==
                 PC_RESULT_OK);
        if ((info.generation_type == PC_SIDE_PREFIX ||
             info.generation_type == PC_SIDE_SUFFIX) &&
            info.family_tier_index == 2) {
            selected_family = info.family_id;
            selected_side = info.generation_type;
            tier_two_key = info.key;
        }
    }
    for (std::uint32_t i = 0;
         i < mod_count && selected_family != UINT32_MAX &&
         tier_one_key.empty();
         ++i) {
        pc_mod_info info{};
        info.struct_size = sizeof(info);
        info.abi_version = PC_ABI_VERSION;
        PC_CHECK(pc_session_get_mod_info(session, i, &info, &error) ==
                 PC_RESULT_OK);
        if (info.family_id == selected_family &&
            info.family_tier_index == 1) {
            tier_one_key = info.key;
        }
    }
    PC_CHECK(!tier_one_key.empty());
    PC_CHECK(!tier_two_key.empty());
    if (!tier_one_key.empty() && !tier_two_key.empty()) {
        for (int threshold : {2, 1}) {
            const std::string tier_json = family_tier_strategy(
                tier_one_key, tier_two_key, selected_side, threshold);
            pc_strategy_handle tier_strategy = nullptr;
            PC_CHECK(pc_strategy_compile_json(
                         session, tier_json.data(), tier_json.size(),
                         &tier_strategy, &error) == PC_RESULT_OK);
            pc_simulator_handle tier_simulator = nullptr;
            PC_CHECK(pc_simulator_create(
                         session, tier_strategy, nullptr, &tier_simulator,
                         &error) == PC_RESULT_OK);
            auto tier_options = options(1);
            PC_CHECK(pc_simulator_run_chunk(
                         tier_simulator, &tier_options, 1, &progress,
                         &error) == PC_RESULT_OK);
            PC_CHECK(pc_simulator_get_summary(
                         tier_simulator, &summary, &error) == PC_RESULT_OK);
            PC_CHECK(
                threshold == 2 ? summary.success_count == 1
                               : summary.failure_count == 1);
            pc_simulator_destroy(tier_simulator);
            pc_strategy_destroy(tier_strategy);
        }
        for (bool item_fractured : {false, true}) {
            const std::string fractured_json = family_tier_strategy(
                tier_one_key,
                tier_two_key,
                selected_side,
                2,
                item_fractured,
                true);
            pc_strategy_handle fractured_strategy = nullptr;
            PC_CHECK(pc_strategy_compile_json(
                         session,
                         fractured_json.data(),
                         fractured_json.size(),
                         &fractured_strategy,
                         &error) == PC_RESULT_OK);
            pc_simulator_handle fractured_simulator = nullptr;
            PC_CHECK(pc_simulator_create(
                         session,
                         fractured_strategy,
                         nullptr,
                         &fractured_simulator,
                         &error) == PC_RESULT_OK);
            auto fractured_options = options(1);
            PC_CHECK(pc_simulator_run_chunk(
                         fractured_simulator,
                         &fractured_options,
                         1,
                         &progress,
                         &error) == PC_RESULT_OK);
            PC_CHECK(pc_simulator_get_summary(
                         fractured_simulator,
                         &summary,
                         &error) == PC_RESULT_OK);
            PC_CHECK(
                item_fractured ? summary.success_count == 1
                               : summary.failure_count == 1);
            pc_simulator_destroy(fractured_simulator);
            pc_strategy_destroy(fractured_strategy);
        }
        const std::string family_count_json = family_count_strategy(
            tier_one_key, tier_two_key, selected_side);
        pc_strategy_handle family_count_graph = nullptr;
        PC_CHECK(pc_strategy_compile_json(
                     session,
                     family_count_json.data(),
                     family_count_json.size(),
                     &family_count_graph,
                     &error) == PC_RESULT_OK);
        pc_simulator_handle family_count_simulator = nullptr;
        PC_CHECK(pc_simulator_create(
                     session,
                     family_count_graph,
                     nullptr,
                     &family_count_simulator,
                     &error) == PC_RESULT_OK);
        auto family_count_options = options(1);
        PC_CHECK(pc_simulator_run_chunk(
                     family_count_simulator,
                     &family_count_options,
                     1,
                     &progress,
                     &error) == PC_RESULT_OK);
        PC_CHECK(pc_simulator_get_summary(
                     family_count_simulator,
                     &summary,
                     &error) == PC_RESULT_OK);
        PC_CHECK(summary.success_count == 1);
        pc_simulator_destroy(family_count_simulator);
        pc_strategy_destroy(family_count_graph);
    }

    pc_economy_destroy(economy);
    pc_strategy_destroy(strategy);
    pc_session_destroy(session);
    pc_data_destroy(data);
}
