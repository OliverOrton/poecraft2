#include "tests.hpp"

#include "poecraft/api.h"
#include "poecraft/item_state.h"
#include "poecraft/session.h"
#include "poecraft/simulator.h"
#include "poecraft/solver.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

std::string solver_telemetry_json(
    pc_solver_handle solver,
    pc_error_info* error) {
    size_t length = 0;
    PC_CHECK(pc_solver_telemetry(solver, nullptr, 0, &length, error) ==
             PC_RESULT_OK);
    std::string json(length + 1, '\0');
    PC_CHECK(pc_solver_telemetry(solver, json.data(), json.size(), &length,
                                 error) == PC_RESULT_OK);
    json.resize(length);
    return json;
}

/*
 * End-to-end exercise of the solver C ABI using only public API calls:
 * load data, build a session, derive a goal from session mod info, query
 * Calculator outcomes, solve against an economy, compile the policy, and
 * verify the compiled strategy through the public simulator.
 */
void run_public_solver_gate(const char* artifact_dir) {
    const std::string dir = artifact_dir;

    pc_error_info error;
    pc_error_info_init(&error);
    pc_data_handle data = nullptr;
    if (pc_data_load_file((dir + "/manifest.json").c_str(), &data, &error) !=
        PC_RESULT_OK) {
        std::printf("solver api suite: data load failed: %s\n",
                    error.message);
        PC_CHECK(false);
        return;
    }
    pc_session_options session_options;
    session_options.struct_size = sizeof(session_options);
    session_options.abi_version = PC_ABI_VERSION;
    session_options.base_metadata_path =
        "Metadata/Items/Armours/BodyArmours/BodyInt17";
    session_options.item_level = 86;
    pc_session_handle session = nullptr;
    PC_CHECK(pc_session_create(data, &session_options, &session, &error) ==
             PC_RESULT_OK);
    if (session == nullptr) {
        pc_data_destroy(data);
        return;
    }

    pc_action_context_options context_options;
    context_options.struct_size = sizeof(context_options);
    context_options.abi_version = PC_ABI_VERSION;
    context_options.seed = 7;
    pc_action_context_handle context = nullptr;
    PC_CHECK(pc_action_context_create(session, &context_options, &context,
                                      &error) == PC_RESULT_OK);

    pc_item_state item;
    pc_item_init_options item_options;
    item_options.struct_size = sizeof(item_options);
    item_options.abi_version = PC_ABI_VERSION;
    item_options.rarity = PC_RARITY_RARE;
    item_options.with_implicits = 0;
    PC_CHECK(pc_item_init(session, &item_options, &item, &error) ==
             PC_RESULT_OK);

    /* Goal: the family of the first prefix in the live normal pool. */
    std::vector<pc_pool_entry> pool(4096);
    uint32_t pool_count = 0;
    PC_CHECK(pc_action_context_debug_pool(
                 context, &item, 0, pool.data(),
                 static_cast<uint32_t>(pool.size()), &pool_count,
                 &error) == PC_RESULT_OK);
    PC_CHECK(pool_count > 0);
    pc_mod_info mod_info;
    PC_CHECK(pc_session_get_mod_info(session, pool[0].session_mod_id,
                                     &mod_info, &error) == PC_RESULT_OK);

    const std::string invalid_goal_json =
        std::string("{\"version\":\"v1\",\"slots\":["
                    "{\"family_mod_key\":\"") +
        mod_info.key +
        "\"}],\"min_satisfied_slots\":2}";
    pc_solver_handle invalid_solver = nullptr;
    PC_CHECK(pc_solver_create(session, invalid_goal_json.c_str(),
                              invalid_goal_json.size(), &invalid_solver,
                              &error) == PC_RESULT_INVALID_ARGUMENT);
    PC_CHECK(invalid_solver == nullptr);

    const std::string options_only_goal_json =
        std::string("{\"version\":\"v1\",\"rarity\":\"rare\",\"slots\":["
                    "{\"family_mod_key\":\"") +
        mod_info.key +
        "\",\"min_tier\":0}],\"actions\":[],\"options\":["
        "{\"type\":\"scour_alchemy\"}]}";
    pc_solver_handle options_only_solver = nullptr;
    PC_CHECK(pc_solver_create(
                 session, options_only_goal_json.c_str(),
                 options_only_goal_json.size(), &options_only_solver,
                 &error) == PC_RESULT_OK);
    if (options_only_solver != nullptr) {
        uint32_t primitive_candidate_count = 1;
        PC_CHECK(pc_solver_candidates(
                     options_only_solver, nullptr, 0,
                     &primitive_candidate_count, &error) == PC_RESULT_OK);
        PC_CHECK(primitive_candidate_count == 0);
        const std::string option_telemetry =
            solver_telemetry_json(options_only_solver, &error);
        PC_CHECK(option_telemetry.find(
                     "\"planner\":{\"registry\":") != std::string::npos);
        PC_CHECK(option_telemetry.find(
                     "\"candidate\":1,\"fixed_options\":1") !=
                 std::string::npos);
        pc_solver_destroy(options_only_solver);
    }

    const std::string automatic_goal_json =
        std::string("{\"version\":\"v1\",\"rarity\":\"rare\",\"slots\":["
                    "{\"family_mod_key\":\"") +
        mod_info.key +
        "\",\"min_tier\":0}],\"action_mode\":\"goal_relevant\"}";
    pc_solver_handle automatic_solver = nullptr;
    PC_CHECK(pc_solver_create(
                 session, automatic_goal_json.c_str(),
                 automatic_goal_json.size(), &automatic_solver,
                 &error) == PC_RESULT_OK);
    if (automatic_solver != nullptr) {
        const std::string automatic_telemetry =
            solver_telemetry_json(automatic_solver, &error);
        PC_CHECK(automatic_telemetry.find(
                     "\"automatic_candidates\":{\"enabled\":true") !=
                 std::string::npos);
        const std::size_t automatic_begin = automatic_telemetry.find(
            "\"automatic_candidates\":{");
        const std::size_t automatic_rows = automatic_telemetry.find(
            "\"rows\":", automatic_begin);
        PC_CHECK(automatic_begin != std::string::npos);
        PC_CHECK(automatic_rows != std::string::npos);
        PC_CHECK(automatic_telemetry.substr(
                     automatic_begin, automatic_rows - automatic_begin)
                     .find("\"dependency_primitives\":") !=
                 std::string::npos);
        pc_solver_destroy(automatic_solver);
    }

    const std::string goal_json =
        std::string("{\"version\":\"v1\",\"rarity\":\"rare\",\"slots\":["
                    "{\"family_mod_key\":\"") +
        mod_info.key +
        "\",\"min_tier\":0}],\"actions\":[\"transmute\",\"augment\","
        "\"alteration\",\"regal\",\"alchemy\",\"chaos\",\"exalt\","
        "\"annul\",\"scour\",\"restart\"]}";
    pc_solver_handle solver = nullptr;
    PC_CHECK(pc_solver_create(session, goal_json.c_str(), goal_json.size(),
                              &solver, &error) == PC_RESULT_OK);
    if (solver == nullptr) {
        std::printf("solver api suite: create failed: %s\n", error.message);
        pc_action_context_destroy(context);
        pc_session_destroy(session);
        pc_data_destroy(data);
        return;
    }

    /* Registry introspection covers the full session, not the candidate
     * subset: the subset only scopes the abstraction and solve. */
    uint32_t action_count = 0;
    PC_CHECK(pc_solver_action_count(solver, &action_count, &error) ==
             PC_RESULT_OK);
    PC_CHECK(action_count > 10);
    const std::string create_telemetry =
        solver_telemetry_json(solver, &error);
    PC_CHECK(create_telemetry.find(
                 "\"version\":\"solver_telemetry_v1\"") !=
             std::string::npos);
    PC_CHECK(create_telemetry.find(
                 "\"candidate\":10,\"evaluator_supported\":null") !=
             std::string::npos);
    PC_CHECK(create_telemetry.find("\"priced_scanned\":null") !=
             std::string::npos);
    uint32_t exalt_index = 0;
    PC_CHECK(pc_solver_find_action(solver, "exalt", &exalt_index, &error) ==
             PC_RESULT_OK);
    uint32_t bogus = 0;
    PC_CHECK(pc_solver_find_action(solver, "no-such-action", &bogus,
                                   &error) == PC_RESULT_NOT_FOUND);
    pc_solver_action_info action_info;
    PC_CHECK(pc_solver_get_action_info(solver, exalt_index, &action_info,
                                       &error) == PC_RESULT_OK);
    PC_CHECK(std::string(action_info.id) == "exalt");
    PC_CHECK(action_info.cost_key_count == 1);
    PC_CHECK(std::string(action_info.cost_keys[0]) == "exalt");
    PC_CHECK(action_info.synthetic == 0);
    PC_CHECK(pc_solver_action_count(solver, &action_count, &error) ==
             PC_RESULT_OK);
    bool saw_fossil_metadata = false;
    bool saw_essence_metadata = false;
    bool saw_respecting_reforge = false;
    for (uint32_t action_index = 0; action_index < action_count;
         ++action_index) {
        pc_solver_action_info info{};
        PC_CHECK(pc_solver_get_action_info(
                     solver, action_index, &info, &error) == PC_RESULT_OK);
        const std::string id = info.id;
        if (id == "chaos") {
            saw_respecting_reforge = true;
            PC_CHECK(info.destructive_renewal == 1);
            PC_CHECK(info.respects_prefix_lock == 1);
            PC_CHECK(info.respects_suffix_lock == 1);
            PC_CHECK(info.respects_cannot_roll_attack == 1);
            PC_CHECK(info.respects_cannot_roll_caster == 1);
        }
        if (id.rfind("fossil:", 0) == 0 && !saw_fossil_metadata) {
            saw_fossil_metadata = true;
            PC_CHECK(info.destructive_renewal == 1);
            PC_CHECK(info.preserves_fractured_affixes == 1);
            PC_CHECK(info.respects_prefix_lock == 0);
            PC_CHECK(info.respects_suffix_lock == 0);
            PC_CHECK(info.respects_cannot_roll_attack == 0);
            PC_CHECK(info.respects_cannot_roll_caster == 0);
            PC_CHECK((info.can_destroy & (1u << 1)) != 0);
            PC_CHECK((info.can_preserve & (1u << 4)) != 0);
        }
        if (id.rfind("essence:", 0) == 0 && !saw_essence_metadata) {
            saw_essence_metadata = true;
            PC_CHECK(info.destructive_renewal == 1);
            PC_CHECK(info.preserves_fractured_affixes == 1);
            PC_CHECK(info.respects_prefix_lock == 0);
            PC_CHECK(info.respects_suffix_lock == 0);
            PC_CHECK(info.respects_cannot_roll_attack == 0);
            PC_CHECK(info.respects_cannot_roll_caster == 0);
        }
    }
    const std::string fossil_goal_json =
        std::string("{\"version\":\"v1\",\"rarity\":\"rare\",\"slots\":["
                    "{\"family_mod_key\":\"") +
        mod_info.key +
        "\",\"min_tier\":0}],\"actions\":["
        "\"fossil:Metadata/Items/Currency/CurrencyDelveCraftingCold\"]}";
    pc_solver_handle fossil_solver = nullptr;
    PC_CHECK(pc_solver_create(
                 session, fossil_goal_json.c_str(), fossil_goal_json.size(),
                 &fossil_solver, &error) == PC_RESULT_OK);
    if (fossil_solver != nullptr) {
        uint32_t fossil_action = 0;
        PC_CHECK(pc_solver_find_action(
                     fossil_solver,
                     "fossil:Metadata/Items/Currency/CurrencyDelveCraftingCold",
                     &fossil_action, &error) == PC_RESULT_OK);
        pc_solver_action_info info{};
        PC_CHECK(pc_solver_get_action_info(
                     fossil_solver, fossil_action, &info, &error) ==
                 PC_RESULT_OK);
        saw_fossil_metadata = true;
        PC_CHECK(info.destructive_renewal == 1);
        PC_CHECK(info.preserves_fractured_affixes == 1);
        PC_CHECK(info.respects_prefix_lock == 0);
        PC_CHECK(info.respects_suffix_lock == 0);
        PC_CHECK(info.respects_cannot_roll_attack == 0);
        PC_CHECK(info.respects_cannot_roll_caster == 0);
        PC_CHECK((info.can_destroy & (1u << 1)) != 0);
        PC_CHECK((info.can_preserve & (1u << 4)) != 0);
        pc_solver_destroy(fossil_solver);
    }
    PC_CHECK(saw_fossil_metadata);
    PC_CHECK(saw_essence_metadata);
    PC_CHECK(saw_respecting_reforge);

    /*
     * A product solver owns a coarse planning parent, but Calculator queries
     * must still use the exact primitive Fracture observer. Four concrete
     * affixes therefore remain four uniform physical outcomes through the
     * public C ABI.
    */
    pc_item_state fracture_item = item;
    std::uint32_t fracture_prefixes = 0;
    std::uint32_t fracture_suffixes = 0;
    std::vector<pc_pool_entry> fracture_pool(4096);
    while (fracture_prefixes + fracture_suffixes < 4) {
        std::uint32_t fracture_pool_count = 0;
        PC_CHECK(pc_action_context_debug_pool(
                     context, &fracture_item, -1, fracture_pool.data(),
                     static_cast<std::uint32_t>(fracture_pool.size()),
                     &fracture_pool_count, &error) == PC_RESULT_OK);
        bool added = false;
        for (std::uint32_t i = 0; i < fracture_pool_count; ++i) {
            const int side = fracture_pool[i].generation_type;
            if ((side == PC_SIDE_PREFIX && fracture_prefixes >= 2) ||
                (side == PC_SIDE_SUFFIX && fracture_suffixes >= 2)) {
                continue;
            }
            const std::uint16_t group = static_cast<std::uint16_t>(
                fracture_pool[i].primary_group_id);
            if (pc_item_add_mod(
                    &fracture_item, side,
                    fracture_pool[i].session_mod_id, group, 0,
                    nullptr) != PC_RESULT_OK) {
                continue;
            }
            if (side == PC_SIDE_PREFIX) {
                ++fracture_prefixes;
            } else {
                ++fracture_suffixes;
            }
            added = true;
            break;
        }
        if (!added) break;
    }
    PC_CHECK(fracture_prefixes + fracture_suffixes == 4);
    const std::string fracture_goal_json =
        std::string("{\"version\":\"v1\",\"rarity\":\"rare\",\"slots\":["
                    "{\"family_mod_key\":\"") +
        mod_info.key +
        "\",\"min_tier\":0}],\"action_mode\":\"goal_relevant\"}";
    pc_solver_handle fracture_solver = nullptr;
    PC_CHECK(pc_solver_create(
                 session, fracture_goal_json.c_str(),
                 fracture_goal_json.size(), &fracture_solver,
                 &error) == PC_RESULT_OK);
    if (fracture_solver != nullptr) {
        std::uint32_t fracture_action = 0;
        PC_CHECK(pc_solver_find_action(
                     fracture_solver, "fracture", &fracture_action,
                     &error) == PC_RESULT_OK);
        std::array<pc_calc_outcome, 8> fracture_outcomes{};
        std::uint32_t fracture_outcome_count = 0;
        pc_calc_summary fracture_summary{};
        PC_CHECK(pc_calc_action_outcomes(
                     fracture_solver, &fracture_item, fracture_action,
                     fracture_outcomes.data(),
                     static_cast<std::uint32_t>(
                         fracture_outcomes.size()),
                     &fracture_outcome_count, &fracture_summary,
                     &error) == PC_RESULT_OK);
        PC_CHECK(fracture_summary.supported == 1);
        PC_CHECK(fracture_summary.legal == 1);
        PC_CHECK(fracture_outcome_count == 4);
        double fracture_probability = 0.0;
        for (std::uint32_t i = 0;
             i < fracture_outcome_count; ++i) {
            PC_CHECK(std::fabs(
                         fracture_outcomes[i].probability - 0.25) <
                     1e-12);
            fracture_probability += fracture_outcomes[i].probability;
        }
        PC_CHECK(std::fabs(fracture_probability - 1.0) < 1e-12);
        pc_solver_destroy(fracture_solver);
    }

    /* Calculator: exact exalt odds on the empty rare. */
    std::vector<pc_calc_outcome> outcomes(256);
    uint32_t outcome_count = 0;
    pc_calc_summary calc_summary;
    PC_CHECK(pc_calc_action_outcomes(
                 solver, &item, exalt_index, outcomes.data(),
                 static_cast<uint32_t>(outcomes.size()), &outcome_count,
                 &calc_summary, &error) == PC_RESULT_OK);
    PC_CHECK(calc_summary.supported == 1);
    PC_CHECK(calc_summary.legal == 1);
    PC_CHECK(outcome_count > 0 && outcome_count <= outcomes.size());
    double total_probability = 0.0;
    for (uint32_t i = 0; i < outcome_count; ++i) {
        total_probability += outcomes[i].probability;
        PC_CHECK(outcomes[i].rarity == PC_RARITY_RARE);
        PC_CHECK(outcomes[i].prefix_count + outcomes[i].suffix_count == 1);
    }
    PC_CHECK(std::fabs(total_probability - 1.0) < 1e-9);
    PC_CHECK(calc_summary.slot_satisfied_probability[0] > 0.0);
    PC_CHECK(calc_summary.slot_satisfied_probability[0] < 1.0);
    PC_CHECK(std::fabs(calc_summary.success_probability -
                       calc_summary.slot_satisfied_probability[0]) < 1e-9);

    /* Solve against a public economy. */
    const char* economy_json =
        "{\"version\":\"v1\",\"id\":\"test\",\"prices\":{"
        "\"transmute\":0.1,\"augment\":0.5,\"alteration\":0.2,"
        "\"regal\":1.0,\"alchemy\":0.5,\"chaos\":1.0,\"exalt\":20.0,"
        "\"annul\":3.0,\"scour\":0.5,\"base\":5.0}}";
    pc_economy_handle economy = nullptr;
    PC_CHECK(pc_economy_load_json(economy_json, std::strlen(economy_json),
                                  &economy, &error) == PC_RESULT_OK);

    pc_solve_summary solve_summary;
    PC_CHECK(pc_solver_solve(solver, &item, economy, nullptr,
                             &solve_summary, &error) == PC_RESULT_OK);
    PC_CHECK(solve_summary.converged == 1);
    PC_CHECK(solve_summary.policy_available == 1);
    PC_CHECK(solve_summary.policy_status == PC_SOLVE_POLICY_EXACT);
    PC_CHECK(solve_summary.termination ==
             PC_SOLVE_TERMINATION_EXACT_CLOSED);
    PC_CHECK(solve_summary.lower_bound == solve_summary.start_value);
    PC_CHECK(solve_summary.upper_bound == solve_summary.start_value);
    PC_CHECK(solve_summary.evaluated_policy_cost ==
             solve_summary.start_value);
    PC_CHECK(solve_summary.absolute_optimality_gap == 0.0);
    PC_CHECK(solve_summary.relative_optimality_gap == 0.0);
    PC_CHECK(solve_summary.start_value > 0.0);
    PC_CHECK(solve_summary.skipped_action_count == 0);

    /* The stepped ABI uses the same solver state machine. Exercise abandon,
     * then complete with deliberately tiny and uneven budgets. */
    PC_CHECK(pc_solver_solve_begin(solver, &item, economy, nullptr, &error) ==
             PC_RESULT_OK);
    pc_solve_progress solve_progress{};
    PC_CHECK(pc_solver_solve_step(solver, 1, &solve_progress, &error) ==
             PC_RESULT_OK);
    PC_CHECK(solve_progress.phase != PC_SOLVE_PHASE_DONE);
    PC_CHECK(solve_progress.expanded_states >= 1);
    PC_CHECK(solve_progress.lower_bound >= 0.0);
    const std::string expanded_fragment =
        "\"expanded\":" + std::to_string(solve_progress.expanded_states);
    const std::string live_telemetry =
        solver_telemetry_json(solver, &error);
    PC_CHECK(live_telemetry.find(
                 "\"execution\":{\"status\":\"in_progress\"") !=
             std::string::npos);
    PC_CHECK(live_telemetry.find(expanded_fragment) != std::string::npos);
    PC_CHECK(live_telemetry.find("\"priced_scanned\":10") !=
             std::string::npos);
    PC_CHECK(live_telemetry.find("\"policy_reachable\":null") !=
             std::string::npos);
    pc_solver_solve_abandon(solver);
    const std::string abandoned_telemetry =
        solver_telemetry_json(solver, &error);
    PC_CHECK(abandoned_telemetry.find(
                 "\"execution\":{\"status\":\"abandoned\"") !=
             std::string::npos);
    PC_CHECK(abandoned_telemetry.find("\"status\":\"abandoned\"") !=
             std::string::npos);
    PC_CHECK(abandoned_telemetry.find(expanded_fragment) !=
             std::string::npos);
    PC_CHECK(pc_solver_solve_step(solver, 1, &solve_progress, &error) ==
             PC_RESULT_NOT_FOUND);

    PC_CHECK(pc_solver_solve_begin(solver, &item, economy, nullptr, &error) ==
             PC_RESULT_OK);
    uint32_t step_count = 0;
    do {
        const uint32_t budget = step_count % 3 == 0 ? 1 : 7;
        PC_CHECK(pc_solver_solve_step(solver, budget, &solve_progress,
                                      &error) == PC_RESULT_OK);
        PC_CHECK(solve_progress.expanded_states > 0);
        PC_CHECK(solve_progress.start_value_bound >= 0.0);
        ++step_count;
    } while (!solve_progress.done);
    PC_CHECK(step_count >= 2);
    PC_CHECK(solve_progress.phase == PC_SOLVE_PHASE_DONE);
    PC_CHECK(solve_progress.discovered_states >=
             solve_progress.expanded_states);
    PC_CHECK(solve_progress.state_action_rows > 0);
    PC_CHECK(solve_progress.transition_entries > 0);
    PC_CHECK(solve_progress.live_owned_bytes > 0);
    PC_CHECK(solve_progress.peak_owned_bytes >=
             solve_progress.live_owned_bytes);
    PC_CHECK(solve_progress.lower_bound == solve_summary.start_value);
    PC_CHECK(solve_progress.upper_bound == solve_summary.start_value);

    pc_solve_summary stepped_summary{};
    PC_CHECK(pc_solver_solve_finish(solver, &stepped_summary, &error) ==
             PC_RESULT_OK);
    PC_CHECK(stepped_summary.converged == solve_summary.converged);
    PC_CHECK(stepped_summary.start_state == solve_summary.start_state);
    PC_CHECK(stepped_summary.start_value == solve_summary.start_value);
    PC_CHECK(stepped_summary.expanded_states == solve_summary.expanded_states);
    PC_CHECK(stepped_summary.sweeps == solve_summary.sweeps);
    PC_CHECK(stepped_summary.residual == solve_summary.residual);
    PC_CHECK(stepped_summary.skipped_action_count ==
             solve_summary.skipped_action_count);
    PC_CHECK(stepped_summary.policy_available ==
             solve_summary.policy_available);
    PC_CHECK(stepped_summary.policy_status == solve_summary.policy_status);
    PC_CHECK(stepped_summary.termination == solve_summary.termination);
    PC_CHECK(stepped_summary.lower_bound == solve_summary.lower_bound);
    PC_CHECK(stepped_summary.upper_bound == solve_summary.upper_bound);
    PC_CHECK(stepped_summary.evaluated_policy_cost ==
             solve_summary.evaluated_policy_cost);
    solve_summary = stepped_summary;
    const std::string solved_telemetry =
        solver_telemetry_json(solver, &error);
    PC_CHECK(solved_telemetry.find("\"status\":\"exact_abstract\"") !=
             std::string::npos);
    PC_CHECK(solved_telemetry.find(
                 "\"start_status\":\"exact_abstract_within_tolerance\"") !=
             std::string::npos);
    PC_CHECK(solved_telemetry.find(
                 "\"policy_result\":{\"available\":true,\"status\":\"exact\"") !=
             std::string::npos);
    PC_CHECK(solved_telemetry.find("\"state_action_rows\":0") ==
             std::string::npos);
    PC_CHECK(solved_telemetry.find("\"available\":false") !=
             std::string::npos);
    PC_CHECK(solved_telemetry.find("\"action_analysis\":{") !=
             std::string::npos);
    PC_CHECK(solved_telemetry.find(
                 "\"non_use_is_pruning_certificate\":false") !=
             std::string::npos);
    PC_CHECK(solved_telemetry.find("\"search_cost\":[") !=
             std::string::npos);

    uint32_t start_state = 0;
    PC_CHECK(pc_solver_project_item(solver, &item, &start_state, &error) ==
             PC_RESULT_OK);
    PC_CHECK(start_state == solve_summary.start_state);
    double start_value = 0.0;
    const char* start_action = nullptr;
    PC_CHECK(pc_solver_state_value(solver, start_state, &start_value,
                                   &start_action, &error) == PC_RESULT_OK);
    PC_CHECK(start_value == solve_summary.start_value);
    PC_CHECK(start_action != nullptr);

    /* Solve log: one line per expanded state. */
    size_t log_length = 0;
    PC_CHECK(pc_solver_solve_log(solver, nullptr, 0, &log_length, &error) ==
             PC_RESULT_OK);
    PC_CHECK(log_length > 0);
    std::string log(log_length + 1, '\0');
    PC_CHECK(pc_solver_solve_log(solver, log.data(), log.size(),
                                 &log_length, &error) == PC_RESULT_OK);
    std::size_t lines = 0;
    for (char c : log) {
        if (c == '\n') ++lines;
    }
    PC_CHECK(lines == solve_summary.expanded_states);

    /* Compile the policy and verify it through the public simulator. */
    size_t strategy_length = 0;
    PC_CHECK(pc_solver_compile_strategy(solver, nullptr, 0,
                                        &strategy_length, &error) ==
             PC_RESULT_OK);
    PC_CHECK(strategy_length > 0);
    std::string strategy_json(strategy_length + 1, '\0');
    PC_CHECK(pc_solver_compile_strategy(solver, strategy_json.data(),
                                        strategy_json.size(),
                                        &strategy_length, &error) ==
             PC_RESULT_OK);
    const std::string compiled_telemetry =
        solver_telemetry_json(solver, &error);
    PC_CHECK(compiled_telemetry.find("\"available\":true") !=
             std::string::npos);
    PC_CHECK(compiled_telemetry.find(
                 "\"strategy_json_bytes\":" +
                 std::to_string(strategy_length)) != std::string::npos);

    pc_strategy_handle strategy = nullptr;
    PC_CHECK(pc_strategy_compile_json(session, strategy_json.c_str(),
                                      strategy_length, &strategy, &error) ==
             PC_RESULT_OK);
    pc_simulator_handle simulator = nullptr;
    PC_CHECK(pc_simulator_create(session, strategy, economy, &simulator,
                                 &error) == PC_RESULT_OK);
    pc_simulation_options simulation_options{};
    simulation_options.struct_size = sizeof(simulation_options);
    simulation_options.abi_version = PC_ABI_VERSION;
    simulation_options.target_runs = 10000;
    simulation_options.seed = 20260707;
    simulation_options.max_actions_per_run = 100000;
    pc_simulation_progress progress{};
    PC_CHECK(pc_simulator_run_chunk(simulator, &simulation_options, 10000,
                                    &progress, &error) == PC_RESULT_OK);
    PC_CHECK(progress.finished == 1);
    pc_simulation_summary simulation_summary{};
    PC_CHECK(pc_simulator_get_summary(simulator, &simulation_summary,
                                      &error) == PC_RESULT_OK);
    PC_CHECK(simulation_summary.completed_runs == 10000);
    PC_CHECK(simulation_summary.success_count ==
             simulation_summary.completed_runs);
    PC_CHECK(simulation_summary.missing_price_run_count == 0);
    const double empirical =
        simulation_summary.known_total_cost /
        static_cast<double>(simulation_summary.completed_runs);
    std::printf(
        "solver api gate: V=%.4f empirical=%.4f (%u states, policy %s)\n",
        solve_summary.start_value, empirical, solve_summary.expanded_states,
        start_action);
    PC_CHECK(std::fabs(empirical - solve_summary.start_value) < 0.25);

    pc_simulator_destroy(simulator);
    pc_strategy_destroy(strategy);
    pc_economy_destroy(economy);
    pc_solver_destroy(solver);
    pc_action_context_destroy(context);
    pc_session_destroy(session);
    pc_data_destroy(data);
}

void run_natural_t1_feasibility_gate(const char* artifact_dir) {
    pc_error_info error;
    pc_error_info_init(&error);
    pc_data_handle data = nullptr;
    const std::string manifest =
        std::string(artifact_dir) + "/manifest.json";
    PC_CHECK(pc_data_load_file(manifest.c_str(), &data, &error) ==
             PC_RESULT_OK);
    if (data == nullptr) return;
    PC_CHECK(std::strlen(pc_engine_compiler()) > 0);

    pc_session_options options{};
    options.struct_size = sizeof(options);
    options.abi_version = PC_ABI_VERSION;
    options.base_metadata_path =
        "Metadata/Items/Armours/Helmets/HelmetDex11";
    options.item_level = 86;
    pc_session_handle session = nullptr;
    PC_CHECK(pc_session_create(data, &options, &session, &error) ==
             PC_RESULT_OK);
    if (session == nullptr) {
        pc_data_destroy(data);
        return;
    }

    auto item = [&](const std::uint8_t rarity) {
        pc_item_state value{};
        pc_item_init_options init{};
        init.struct_size = sizeof(init);
        init.abi_version = PC_ABI_VERSION;
        init.rarity = rarity;
        init.with_implicits = 0;
        PC_CHECK(pc_item_init(session, &init, &value, &error) ==
                 PC_RESULT_OK);
        return value;
    };
    auto query = [&](const std::string& goal,
                     const pc_item_state& start) {
        pc_goal_feasibility feasibility{};
        pc_solver_handle solver = nullptr;
        PC_CHECK(pc_solver_create(session, goal.c_str(), goal.size(),
                                  &solver, &error) == PC_RESULT_OK);
        if (solver != nullptr) {
            PC_CHECK(pc_solver_goal_feasibility(
                         solver, &start, &feasibility, &error) ==
                     PC_RESULT_OK);
            pc_solver_destroy(solver);
        }
        return feasibility;
    };

    const std::string natural_three =
        R"({"version":"v1","rarity":"rare","action_mode":"goal_relevant","min_satisfied_slots":3,"slots":[{"family_mod_key":"ItemFoundRarityIncreasePrefix3","min_tier":1},{"family_mod_key":"IncreasedLife9","min_tier":1},{"family_mod_key":"ChaosResist6","min_tier":1}]})";
    const pc_goal_feasibility feasible =
        query(natural_three, item(PC_RARITY_RARE));
    PC_CHECK(feasible.status == PC_GOAL_FEASIBILITY_FEASIBLE);
    PC_CHECK(feasible.reason ==
             PC_GOAL_FEASIBILITY_REASON_NATURAL_REFORGE_WITNESS);
    PC_CHECK(feasible.goal_slot_count == 3);
    PC_CHECK(feasible.eligible_slot_count == 3);
    PC_CHECK(feasible.natural_pool_mod_count > 0);
    PC_CHECK(feasible.natural_pool_weight > 0);
    PC_CHECK(feasible.witness_action_index != UINT32_MAX);
    for (std::uint32_t slot = 0; slot < 3; ++slot) {
        PC_CHECK(feasible.witness_mod_ids[slot] != UINT32_MAX);
        PC_CHECK(feasible.slot_natural_mod_counts[slot] > 0);
        PC_CHECK(feasible.slot_single_draw_probabilities[slot] > 0.0);
    }

    const std::string bench_goal =
        R"({"version":"v1","rarity":"rare","action_mode":"goal_relevant","min_satisfied_slots":1,"slots":[{"family_mod_key":"EinharMasterColdResist3__","min_tier":1}]})";
    const pc_goal_feasibility no_natural =
        query(bench_goal, item(PC_RARITY_RARE));
    PC_CHECK(no_natural.status == PC_GOAL_FEASIBILITY_INFEASIBLE);
    PC_CHECK(no_natural.reason ==
             PC_GOAL_FEASIBILITY_REASON_NO_NATURAL_T1);

    const std::string four_prefixes =
        R"({"version":"v1","rarity":"rare","action_mode":"goal_relevant","min_satisfied_slots":4,"slots":[{"family_mod_key":"ItemFoundRarityIncreasePrefix3","min_tier":1},{"family_mod_key":"IncreasedLife9","min_tier":1},{"family_mod_key":"LocalIncreasedEvasionRating8","min_tier":1},{"family_mod_key":"LocalIncreasedEvasionRatingPercent7","min_tier":1}]})";
    const pc_goal_feasibility capacity =
        query(four_prefixes, item(PC_RARITY_RARE));
    PC_CHECK(capacity.status == PC_GOAL_FEASIBILITY_INFEASIBLE);
    PC_CHECK(capacity.reason ==
             PC_GOAL_FEASIBILITY_REASON_SLOT_CAPACITY);

    const pc_goal_feasibility unknown =
        query(natural_three, item(PC_RARITY_MAGIC));
    PC_CHECK(unknown.status == PC_GOAL_FEASIBILITY_UNKNOWN);
    PC_CHECK(unknown.reason ==
             PC_GOAL_FEASIBILITY_REASON_UNSUPPORTED_START);

    pc_session_destroy(session);
    pc_data_destroy(data);
}

} // namespace

void run_solver_api_tests(const char* artifact_dir) {
    if (artifact_dir == nullptr) {
        std::printf("solver api suite skipped (missing path)\n");
        return;
    }
    run_public_solver_gate(artifact_dir);
    run_natural_t1_feasibility_gate(artifact_dir);
}

void run_solver_feasibility_tests(const char* artifact_dir) {
    if (artifact_dir == nullptr) {
        std::printf("solver feasibility suite skipped (missing path)\n");
        return;
    }
    run_natural_t1_feasibility_gate(artifact_dir);
}
