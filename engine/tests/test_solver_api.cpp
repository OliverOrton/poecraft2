#include "tests.hpp"

#include "poecraft/api.h"
#include "poecraft/item_state.h"
#include "poecraft/session.h"
#include "poecraft/simulator.h"
#include "poecraft/solver.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

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
    PC_CHECK(solve_summary.start_value > 0.0);
    PC_CHECK(solve_summary.skipped_action_count == 0);

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
    simulation_options.target_runs = 20000;
    simulation_options.seed = 20260707;
    simulation_options.max_actions_per_run = 100000;
    pc_simulation_progress progress{};
    PC_CHECK(pc_simulator_run_chunk(simulator, &simulation_options, 20000,
                                    &progress, &error) == PC_RESULT_OK);
    PC_CHECK(progress.finished == 1);
    pc_simulation_summary simulation_summary{};
    PC_CHECK(pc_simulator_get_summary(simulator, &simulation_summary,
                                      &error) == PC_RESULT_OK);
    PC_CHECK(simulation_summary.completed_runs == 20000);
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

} // namespace

void run_solver_api_tests(const char* artifact_dir) {
    if (artifact_dir == nullptr) {
        std::printf("solver api suite skipped (missing path)\n");
        return;
    }
    run_public_solver_gate(artifact_dir);
}
