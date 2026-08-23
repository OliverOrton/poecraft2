#include "tests.hpp"

#include "poecraft/api.h"
#include "poecraft/bestiary.h"
#include "poecraft/item_state.h"
#include "poecraft/session.h"
#include "poecraft/simulator.h"
#include "poecraft/solver.h"

#include "../src/json.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

namespace json = poecraft::json;

std::string read_solver_api_fixture(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::runtime_error(
            "could not read solver API fixture: " + path.string());
    }
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

json::Value parse_solver_api_fixture(const std::string& text) {
    return json::Parser(text.data(), text.size()).parse();
}

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

std::string solver_economy_json(
    const std::map<std::string, double>& prices) {
    std::string json =
        "{\"version\":\"v1\",\"id\":\"product-action-matrix\","
        "\"prices\":{";
    bool first = true;
    for (const auto& [key, value] : prices) {
        if (!first) json.push_back(',');
        first = false;
        json += '\"' + key + "\":" + std::to_string(value);
    }
    json += "}}";
    return json;
}

std::string compile_and_exact_evaluate_public_policy(
    pc_session_handle session,
    pc_solver_handle solver,
    pc_economy_handle economy,
    pc_error_info* error) {
    std::size_t strategy_length = 0;
    PC_CHECK(pc_solver_compile_strategy(
                 solver, nullptr, 0, &strategy_length,
                 error) == PC_RESULT_OK);
    std::string strategy_json(strategy_length + 1, '\0');
    PC_CHECK(pc_solver_compile_strategy(
                 solver, strategy_json.data(), strategy_json.size(),
                 &strategy_length, error) == PC_RESULT_OK);
    strategy_json.resize(strategy_length);
    pc_strategy_handle strategy = nullptr;
    PC_CHECK(pc_strategy_compile_json(
                 session, strategy_json.c_str(), strategy_json.size(),
                 &strategy, error) == PC_RESULT_OK);
    if (strategy != nullptr) {
        pc_strategy_eval_options eval_options{};
        eval_options.struct_size = sizeof(eval_options);
        eval_options.abi_version = PC_ABI_VERSION;
        eval_options.economy = economy;
        std::size_t eval_length = 0;
        PC_CHECK(pc_strategy_evaluate(
                     strategy, &eval_options, nullptr, 0, &eval_length,
                     error) == PC_RESULT_OK);
        std::string eval_json(eval_length + 1, '\0');
        PC_CHECK(pc_strategy_evaluate(
                     strategy, &eval_options, eval_json.data(),
                     eval_json.size(), &eval_length,
                     error) == PC_RESULT_OK);
        PC_CHECK(eval_json.find("\"success\":1") != std::string::npos);
        pc_strategy_destroy(strategy);
    }
    return strategy_json;
}

void run_public_product_imprint_current_gate(const char* artifact_dir) {
    namespace fs = std::filesystem;
    pc_error_info error;
    pc_error_info_init(&error);

    fs::path repo_root = fs::absolute(fs::path(artifact_dir)).lexically_normal();
    for (int parent = 0; parent < 3; ++parent) {
        repo_root = repo_root.parent_path();
    }
    try {
        const fs::path fixture_path = repo_root / "fixtures" / "bestiary" /
            "v1" / "solver-product-imprint-current.json";
        const std::string fixture_text = read_solver_api_fixture(fixture_path);
        const json::Value fixture = parse_solver_api_fixture(fixture_text);
        PC_CHECK(fixture.at("schema_version").as_string() ==
                 "solver_product_imprint_current_v1");
        const json::Value& economy_spec = fixture.at("economy");
        const fs::path snapshot_path = repo_root /
            economy_spec.at("snapshot_relative_path").as_string();
        const std::string snapshot_text =
            read_solver_api_fixture(snapshot_path);
        const json::Value snapshot = parse_solver_api_fixture(snapshot_text);
        PC_CHECK(snapshot.at("id").as_string() ==
                 economy_spec.at("expected_snapshot_id").as_string());
        const json::Value& metadata = snapshot.at("metadata");
        const json::Value& expected_coverage =
            economy_spec.at("expected_coverage");
        PC_CHECK(metadata.at("price_count").as_int() ==
                 expected_coverage.at("priced").as_int());
        PC_CHECK(static_cast<std::int64_t>(
                     metadata.at("missing_keys").array.size()) ==
                 expected_coverage.at("missing").as_int());
        PC_CHECK(static_cast<std::int64_t>(
                     metadata.at("low_confidence_keys").array.size()) ==
                 expected_coverage.at("low_confidence").as_int());

        std::map<std::string, double> snapshot_prices;
        for (const auto& [key, value] : snapshot.at("prices").object) {
            snapshot_prices.emplace(key, value.as_number());
        }
        std::map<std::string, std::string> snapshot_sources;
        for (const auto& [key, value] : snapshot.at("sources").object) {
            snapshot_sources.emplace(key, value.as_string());
        }
        std::map<std::string, std::uint32_t> expected_components;
        std::uint32_t expected_component_count = 0;
        for (const json::Value& component :
             economy_spec.at("recipe_components").array) {
            const std::string key = component.at("price_key").as_string();
            const std::uint32_t quantity = static_cast<std::uint32_t>(
                component.at("quantity").as_int());
            expected_components[key] = quantity;
            expected_component_count += quantity;
            PC_CHECK(snapshot_prices.contains(key));
            PC_CHECK(snapshot_sources.contains(key));
            if (snapshot_prices.contains(key)) {
                PC_CHECK(std::fabs(
                             snapshot_prices.at(key) -
                             component.at("chaos_value").as_number()) <
                         1e-12);
            }
            if (snapshot_sources.contains(key)) {
                PC_CHECK(snapshot_sources.at(key) ==
                         component.at("source").as_string());
            }
        }
        PC_CHECK(expected_component_count == 4);
        PC_CHECK(snapshot_sources.at("beast:craicic-croaker") == "quote");
        PC_CHECK(snapshot_sources.at("beast:rare") == "owner_default");

        pc_economy_handle economy = nullptr;
        PC_CHECK(pc_economy_load_json(
                     snapshot_text.c_str(), snapshot_text.size(), &economy,
                     &error) == PC_RESULT_OK);
        if (economy == nullptr) return;

        pc_data_handle data = nullptr;
        const std::string manifest =
            std::string(artifact_dir) + "/manifest.json";
        PC_CHECK(pc_data_load_file(
                     manifest.c_str(), &data, &error) == PC_RESULT_OK);
        if (data == nullptr) {
            pc_economy_destroy(economy);
            return;
        }

        std::uint32_t imprint_action = 0;
        PC_CHECK(pc_bestiary_find_action(
                     data, "bestiary:imprint", &imprint_action,
                     &error) == PC_RESULT_OK);
        pc_bestiary_action_info imprint_info{};
        PC_CHECK(pc_bestiary_get_action_info(
                     data, imprint_action, &imprint_info,
                     &error) == PC_RESULT_OK);
        PC_CHECK(imprint_info.solver_available == 1);
        PC_CHECK(imprint_info.cost_key_count == expected_component_count);
        std::map<std::string, std::uint32_t> actual_components;
        for (std::uint32_t index = 0;
             index < imprint_info.cost_key_count; ++index) {
            ++actual_components[imprint_info.cost_keys[index]];
        }
        PC_CHECK(actual_components == expected_components);

        const json::Value& session_spec = fixture.at("session");
        pc_session_options session_options{};
        session_options.struct_size = sizeof(session_options);
        session_options.abi_version = PC_ABI_VERSION;
        const std::string base_path =
            session_spec.at("base_metadata_path").as_string();
        session_options.base_metadata_path = base_path.c_str();
        session_options.item_level = static_cast<std::uint32_t>(
            session_spec.at("item_level").as_int());
        pc_session_handle session = nullptr;
        PC_CHECK(pc_session_create(
                     data, &session_options, &session,
                     &error) == PC_RESULT_OK);
        if (session == nullptr) {
            pc_data_destroy(data);
            pc_economy_destroy(economy);
            return;
        }
        pc_action_context_options context_options{};
        context_options.struct_size = sizeof(context_options);
        context_options.abi_version = PC_ABI_VERSION;
        context_options.seed = 20260809;
        pc_action_context_handle context = nullptr;
        PC_CHECK(pc_action_context_create(
                     session, &context_options, &context,
                     &error) == PC_RESULT_OK);

        pc_item_init_options item_options{};
        item_options.struct_size = sizeof(item_options);
        item_options.abi_version = PC_ABI_VERSION;
        item_options.rarity = PC_RARITY_MAGIC;
        item_options.with_implicits = 0;
        pc_item_state start{};
        PC_CHECK(pc_item_init(
                     session, &item_options, &start,
                     &error) == PC_RESULT_OK);
        start.generic_influence_bits = static_cast<std::uint8_t>(
            fixture.at("start").at("generic_influence_bits").as_int());

        std::vector<pc_pool_entry> pool(4096);
        std::uint32_t pool_count = 0;
        PC_CHECK(pc_action_context_debug_pool(
                     context, &start, 0, pool.data(),
                     static_cast<std::uint32_t>(pool.size()), &pool_count,
                     &error) == PC_RESULT_OK);
        struct WeightedFamily {
            std::uint64_t weight = 0;
            std::uint32_t representative = UINT32_MAX;
            pc_mod_info info{};
        };
        std::map<std::uint32_t, WeightedFamily> prefix_families;
        for (std::uint32_t index = 0; index < pool_count; ++index) {
            if (pool[index].final_weight == 0 ||
                pool[index].generation_type != PC_SIDE_PREFIX) {
                continue;
            }
            pc_mod_info info{};
            PC_CHECK(pc_session_get_mod_info(
                         session, pool[index].session_mod_id, &info,
                         &error) == PC_RESULT_OK);
            if (info.reach_influence >= 0) continue;
            WeightedFamily& family = prefix_families[info.family_id];
            family.weight += pool[index].final_weight;
            if (family.representative == UINT32_MAX) {
                family.representative = pool[index].session_mod_id;
                family.info = info;
            }
        }
        PC_CHECK(!prefix_families.empty());
        const auto carrier = std::min_element(
            prefix_families.begin(), prefix_families.end(),
            [](const auto& left, const auto& right) {
                if (left.second.weight != right.second.weight) {
                    return left.second.weight < right.second.weight;
                }
                return left.first < right.first;
            });
        if (carrier == prefix_families.end()) {
            pc_action_context_destroy(context);
            pc_session_destroy(session);
            pc_data_destroy(data);
            pc_economy_destroy(economy);
            return;
        }
        /* The regular native/WASM benchmark freezes this dynamic gate's
         * current-artifact projection. Keep the keys tied here so an artifact
         * change cannot silently turn the benchmark into a different craft. */
        PC_CHECK(std::string(carrier->second.info.key) ==
                 fixture.at("expected")
                     .at("selected_carrier_mod_key").as_string());
        PC_CHECK(pc_item_add_mod(
                     &start, PC_SIDE_PREFIX,
                     carrier->second.representative,
                     static_cast<std::uint16_t>(
                         carrier->second.info.primary_group_id),
                     0, nullptr) == PC_RESULT_OK);

        pool_count = 0;
        PC_CHECK(pc_action_context_debug_pool(
                     context, &start, 1, pool.data(),
                     static_cast<std::uint32_t>(pool.size()), &pool_count,
                     &error) == PC_RESULT_OK);
        std::map<std::uint32_t, WeightedFamily> suffix_families;
        for (std::uint32_t index = 0; index < pool_count; ++index) {
            if (pool[index].final_weight == 0 ||
                pool[index].generation_type != PC_SIDE_SUFFIX) {
                continue;
            }
            pc_mod_info info{};
            PC_CHECK(pc_session_get_mod_info(
                         session, pool[index].session_mod_id, &info,
                         &error) == PC_RESULT_OK);
            if (info.reach_influence >= 0) continue;
            WeightedFamily& family = suffix_families[info.family_id];
            family.weight += pool[index].final_weight;
            if (family.representative == UINT32_MAX) {
                family.representative = pool[index].session_mod_id;
                family.info = info;
            }
        }
        PC_CHECK(!suffix_families.empty());
        const auto target = std::max_element(
            suffix_families.begin(), suffix_families.end(),
            [](const auto& left, const auto& right) {
                if (left.second.weight != right.second.weight) {
                    return left.second.weight < right.second.weight;
                }
                return left.first > right.first;
            });
        if (target == suffix_families.end()) {
            pc_action_context_destroy(context);
            pc_session_destroy(session);
            pc_data_destroy(data);
            pc_economy_destroy(economy);
            return;
        }
        PC_CHECK(std::string(target->second.info.key) ==
                 fixture.at("expected")
                     .at("selected_target_mod_key").as_string());

        pc_bestiary_craft_state craft_state{};
        PC_CHECK(pc_bestiary_state_init(
                     &start, 1, &craft_state, &error) == PC_RESULT_OK);
        pc_bestiary_action_request create_request{
            sizeof(pc_bestiary_action_request), PC_ABI_VERSION,
            "bestiary:imprint"};
        pc_bestiary_calculation create_calc{};
        PC_CHECK(pc_bestiary_calculate_action(
                     data, &craft_state, &create_request, &create_calc,
                     &error) == PC_RESULT_OK);
        PC_CHECK(create_calc.result.applied == 1);
        PC_CHECK(create_calc.result.consumed_price_key_count == 4);

        const std::string goal =
            std::string(
                "{\"version\":\"v1\",\"rarity\":\"rare\","
                "\"action_mode\":\"goal_relevant\","
                "\"fossil_mode\":\"goal_relevant\","
                "\"min_satisfied_slots\":2,\"slots\":[{"
                "\"family_mod_key\":\"") +
            carrier->second.info.key +
            "\",\"min_tier\":0},{\"family_mod_key\":\"" +
            target->second.info.key +
            "\",\"min_tier\":0}],\"actions\":[\"augment\","
            "\"regal\"]}";
        pc_solver_handle solver = nullptr;
        PC_CHECK(pc_solver_create(
                     session, goal.c_str(), goal.size(), &solver,
                     &error) == PC_RESULT_OK);
        if (solver == nullptr) {
            pc_action_context_destroy(context);
            pc_session_destroy(session);
            pc_data_destroy(data);
            pc_economy_destroy(economy);
            return;
        }
        const std::string create_telemetry =
            solver_telemetry_json(solver, &error);
        PC_CHECK(create_telemetry.find(
                     "\"product_admission\":{\"goal_filtering\":true") !=
                 std::string::npos);
        PC_CHECK(create_telemetry.find(
                     "\"explicit_envelope\":true") !=
                 std::string::npos);

        const json::Value& overrides =
            economy_spec.at("forced_winner_overrides");
        for (const json::Value& override_row : overrides.array) {
            const std::string key =
                override_row.at("price_key").as_string();
            PC_CHECK(!key.starts_with("beast:"));
            PC_CHECK(snapshot_prices.contains(key));
            if (snapshot_prices.contains(key)) {
                PC_CHECK(std::fabs(
                             snapshot_prices.at(key) -
                             override_row.at("snapshot_chaos_value")
                                 .as_number()) <
                         1e-12);
                snapshot_prices[key] =
                    override_row.at("forced_chaos_value").as_number();
            }
        }
        if (!overrides.array.empty()) {
            pc_economy_destroy(economy);
            economy = nullptr;
            const std::string forced_economy =
                solver_economy_json(snapshot_prices);
            PC_CHECK(pc_economy_load_json(
                         forced_economy.c_str(), forced_economy.size(),
                         &economy, &error) == PC_RESULT_OK);
        }

        const json::Value& limits = fixture.at("solve_limits");
        pc_solve_options solve_options{};
        solve_options.struct_size = sizeof(solve_options);
        solve_options.abi_version = PC_ABI_VERSION;
        solve_options.max_discovered_states = static_cast<std::uint32_t>(
            limits.at("max_discovered_states").as_int());
        solve_options.max_expanded_states = static_cast<std::uint32_t>(
            limits.at("max_expanded_states").as_int());
        solve_options.max_state_action_rows = static_cast<std::uint64_t>(
            limits.at("max_state_action_rows").as_int());
        solve_options.max_transitions = static_cast<std::uint64_t>(
            limits.at("max_transitions").as_int());
        solve_options.max_reforge_work = static_cast<std::uint64_t>(
            limits.at("max_reforge_work").as_int());
        solve_options.max_solver_owned_bytes = static_cast<std::uint64_t>(
            limits.at("max_solver_owned_bytes").as_int());
        solve_options.max_diagnostic_samples = static_cast<std::uint32_t>(
            limits.at("max_diagnostic_samples").as_int());
        solve_options.max_telemetry_json_bytes = static_cast<std::uint64_t>(
            limits.at("max_telemetry_json_bytes").as_int());
        pc_solve_summary summary{};
        const pc_result solve_result = pc_solver_solve(
            solver, &start, economy, &solve_options, &summary, &error);
        PC_CHECK(solve_result == PC_RESULT_OK);
        if (solve_result != PC_RESULT_OK) {
            std::printf(
                "solver API current Imprint solve failed: result=%d "
                "error=%d message=%s\n",
                static_cast<int>(solve_result), error.code,
                error.message);
            pc_solver_destroy(solver);
            pc_action_context_destroy(context);
            pc_session_destroy(session);
            pc_data_destroy(data);
            pc_economy_destroy(economy);
            return;
        }
        const std::string solved_telemetry =
            solver_telemetry_json(solver, &error);
        const json::Value telemetry =
            parse_solver_api_fixture(solved_telemetry);
        const json::Value& imprint_telemetry = telemetry
            .at("action_control").at("automatic_candidates")
            .at("by_kind").at("imprint");
        std::printf(
            "solver API current Imprint solve: policy=%d termination=%d "
            "stop=%d caps=%u states=%u candidates=%lld eligible=%lld "
            "missing=%lld\n",
            summary.policy_available, summary.termination,
            summary.stop_cause, summary.cap_hit_mask,
            summary.expanded_states,
            static_cast<long long>(
                imprint_telemetry.at("candidates").as_int()),
            static_cast<long long>(
                imprint_telemetry.at("eligible").as_int()),
            static_cast<long long>(
                imprint_telemetry.at("missing_price").as_int()));
        PC_CHECK(imprint_telemetry.at("candidates").as_int() > 0);
        PC_CHECK(imprint_telemetry.at("eligible").as_int() > 0);
        PC_CHECK(imprint_telemetry.at("missing_price").as_int() == 0);
        PC_CHECK(summary.policy_available == 1);
        if (summary.policy_available != 1) {
            pc_solver_destroy(solver);
            pc_action_context_destroy(context);
            pc_session_destroy(session);
            pc_data_destroy(data);
            pc_economy_destroy(economy);
            return;
        }

        std::size_t strategy_length = 0;
        PC_CHECK(pc_solver_compile_strategy(
                     solver, nullptr, 0, &strategy_length,
                     &error) == PC_RESULT_OK);
        std::string strategy_json(strategy_length + 1, '\0');
        PC_CHECK(pc_solver_compile_strategy(
                     solver, strategy_json.data(), strategy_json.size(),
                     &strategy_length, &error) == PC_RESULT_OK);
        strategy_json.resize(strategy_length);
        const json::Value compiled_document =
            parse_solver_api_fixture(strategy_json);
        std::set<std::string> compiled_operations;
        for (const json::Value& node : compiled_document.at("nodes").array) {
            const json::Value* operation = node.find("operation");
            if (operation != nullptr) {
                compiled_operations.insert(
                    operation->at("type").as_string());
            }
        }
        std::printf("solver API current Imprint compiled operations:");
        for (const std::string& operation : compiled_operations) {
            std::printf(" %s", operation.c_str());
        }
        std::printf("\n");
        PC_CHECK(compiled_operations.contains("bestiary:imprint"));
        PC_CHECK(compiled_operations.contains(
            "bestiary:restore_imprint"));

        pc_strategy_handle strategy = nullptr;
        PC_CHECK(pc_strategy_compile_json(
                     session, strategy_json.c_str(), strategy_json.size(),
                     &strategy, &error) == PC_RESULT_OK);
        pc_strategy_eval_options eval_options{};
        eval_options.struct_size = sizeof(eval_options);
        eval_options.abi_version = PC_ABI_VERSION;
        eval_options.economy = economy;
        std::size_t eval_length = 0;
        PC_CHECK(pc_strategy_evaluate(
                     strategy, &eval_options, nullptr, 0, &eval_length,
                     &error) == PC_RESULT_OK);
        std::string eval_text(eval_length + 1, '\0');
        PC_CHECK(pc_strategy_evaluate(
                     strategy, &eval_options, eval_text.data(),
                     eval_text.size(), &eval_length,
                     &error) == PC_RESULT_OK);
        eval_text.resize(eval_length);
        const json::Value exact = parse_solver_api_fixture(eval_text);
        PC_CHECK(exact.at("converged").as_bool());
        PC_CHECK(std::fabs(
                     exact.at("terminals").at("success").as_number() -
                     1.0) < 1e-12);
        const json::Value& accounting = exact.at("accounting");
        PC_CHECK(accounting.at("pricing").at("status").as_string() ==
                 "complete");
        PC_CHECK(accounting.at("pricing").at("missing_price_keys")
                     .array.empty());
        double expected_croakers = 0.0;
        double expected_rares = 0.0;
        for (const json::Value& row :
             exact.at("expected_consumption").array) {
            const std::string key = row.at("key").as_string();
            if (key == "beast:craicic-croaker") {
                expected_croakers = row.at("quantity").as_number();
            } else if (key == "beast:rare") {
                expected_rares = row.at("quantity").as_number();
            }
        }
        PC_CHECK(expected_croakers > 0.0);
        PC_CHECK(std::fabs(
                     expected_rares - 3.0 * expected_croakers) <
                 1e-8);

        pc_simulator_handle simulator = nullptr;
        PC_CHECK(pc_simulator_create(
                     session, strategy, economy, &simulator,
                     &error) == PC_RESULT_OK);
        const json::Value& simulation = fixture.at("simulation");
        pc_simulation_options simulation_options{};
        simulation_options.struct_size = sizeof(simulation_options);
        simulation_options.abi_version = PC_ABI_VERSION;
        simulation_options.target_runs = static_cast<std::uint64_t>(
            simulation.at("runs").as_int());
        simulation_options.seed = static_cast<std::uint64_t>(
            simulation.at("seed").as_int());
        simulation_options.max_actions_per_run =
            static_cast<std::uint64_t>(
                simulation.at("max_actions_per_run").as_int());
        pc_simulation_progress progress{};
        PC_CHECK(pc_simulator_run_chunk(
                     simulator, &simulation_options,
                     static_cast<std::uint32_t>(
                         simulation_options.target_runs),
                     &progress, &error) == PC_RESULT_OK);
        PC_CHECK(progress.finished == 1);
        pc_simulation_summary simulation_summary{};
        PC_CHECK(pc_simulator_get_summary(
                     simulator, &simulation_summary,
                     &error) == PC_RESULT_OK);
        PC_CHECK(simulation_summary.completed_runs ==
                 simulation_options.target_runs);
        PC_CHECK(simulation_summary.success_count ==
                 static_cast<std::uint64_t>(
                     simulation.at("expected_successes").as_int()));
        PC_CHECK(simulation_summary.failure_count ==
                 static_cast<std::uint64_t>(
                     simulation.at("expected_failures").as_int()));
        PC_CHECK(simulation_summary.missing_price_run_count ==
                 static_cast<std::uint64_t>(
                     simulation.at("expected_missing_price_runs").as_int()));

        std::uint32_t material_count = 0;
        PC_CHECK(pc_simulator_material_distribution_query(
                     simulator, nullptr, 0, &material_count,
                     &error) == PC_RESULT_BUFFER_TOO_SMALL);
        std::vector<pc_material_sample_entry> materials(material_count);
        PC_CHECK(pc_simulator_material_distribution_query(
                     simulator, materials.data(), material_count,
                     &material_count, &error) == PC_RESULT_OK);
        std::map<std::string, std::uint64_t> material_counts;
        for (const pc_material_sample_entry& material : materials) {
            material_counts[material.price_key] = material.count;
        }
        PC_CHECK(material_counts["beast:craicic-croaker"] >=
                 simulation_options.target_runs);
        PC_CHECK(material_counts["beast:rare"] ==
                 3 * material_counts["beast:craicic-croaker"]);
        std::printf(
            "solver API current Imprint: V=%.6f creates=%llu runs=%llu\n",
            summary.start_value,
            static_cast<unsigned long long>(
                material_counts["beast:craicic-croaker"]),
            static_cast<unsigned long long>(
                simulation_summary.completed_runs));

        pc_simulator_destroy(simulator);
        pc_strategy_destroy(strategy);
        pc_solver_destroy(solver);
        pc_action_context_destroy(context);
        pc_session_destroy(session);
        pc_data_destroy(data);
        pc_economy_destroy(economy);
    } catch (const std::exception& exception) {
        std::printf("solver API current Imprint fixture: %s\n",
                    exception.what());
        PC_CHECK(false);
    }
}

void run_public_product_eldritch_gate(const char* artifact_dir) {
    pc_error_info error;
    pc_error_info_init(&error);
    pc_data_handle data = nullptr;
    const std::string manifest =
        std::string(artifact_dir) + "/manifest.json";
    PC_CHECK(pc_data_load_file(
                 manifest.c_str(), &data, &error) == PC_RESULT_OK);
    if (data == nullptr) return;

    pc_session_options session_options{};
    session_options.struct_size = sizeof(session_options);
    session_options.abi_version = PC_ABI_VERSION;
    session_options.base_metadata_path =
        "Metadata/Items/Armours/BodyArmours/BodyInt17";
    session_options.item_level = 86;
    pc_session_handle session = nullptr;
    PC_CHECK(pc_session_create(
                 data, &session_options, &session, &error) == PC_RESULT_OK);
    if (session == nullptr) {
        pc_data_destroy(data);
        return;
    }

    pc_action_context_options context_options{};
    context_options.struct_size = sizeof(context_options);
    context_options.abi_version = PC_ABI_VERSION;
    context_options.seed = 17;
    pc_action_context_handle context = nullptr;
    PC_CHECK(pc_action_context_create(
                 session, &context_options, &context, &error) == PC_RESULT_OK);
    pc_item_state empty{};
    pc_item_init_options item_options{};
    item_options.struct_size = sizeof(item_options);
    item_options.abi_version = PC_ABI_VERSION;
    item_options.rarity = PC_RARITY_RARE;
    item_options.with_implicits = 0;
    PC_CHECK(pc_item_init(
                 session, &item_options, &empty, &error) == PC_RESULT_OK);

    std::vector<pc_pool_entry> pool(4096);
    std::uint32_t pool_count = 0;
    PC_CHECK(pc_action_context_debug_pool(
                 context, &empty, -1, pool.data(),
                 static_cast<std::uint32_t>(pool.size()), &pool_count,
                 &error) == PC_RESULT_OK);
    std::uint32_t prefix_goal = UINT32_MAX;
    std::uint32_t suffix_goal = UINT32_MAX;
    pc_mod_info prefix_info{};
    pc_mod_info suffix_info{};
    pc_mod_info multimod_prefix_info{};
    bool found_multimod_prefix = false;
    std::vector<std::pair<std::uint32_t, pc_mod_info>> prefix_junk;
    std::vector<std::pair<std::uint32_t, pc_mod_info>> suffix_junk;
    std::uint32_t session_mod_count = 0;
    PC_CHECK(pc_session_get_mod_count(
                 session, &session_mod_count, &error) == PC_RESULT_OK);
    std::map<std::uint32_t, std::uint8_t> family_side_masks;
    for (std::uint32_t mod = 0; mod < session_mod_count; ++mod) {
        pc_mod_info info{};
        PC_CHECK(pc_session_get_mod_info(
                     session, mod, &info, &error) == PC_RESULT_OK);
        if (info.generation_type == PC_SIDE_PREFIX) {
            family_side_masks[info.family_id] |= 1u;
        } else if (info.generation_type == PC_SIDE_SUFFIX) {
            family_side_masks[info.family_id] |= 2u;
        }
        if (std::string(info.key) == "EinharMasterColdResist3__") {
            suffix_goal = mod;
            suffix_info = info;
        } else if (std::string(info.key) ==
                   "EinharMasterIncreasedLife5_") {
            multimod_prefix_info = info;
            found_multimod_prefix = true;
        }
    }
    for (std::uint32_t i = 0; i < pool_count; ++i) {
        pc_mod_info info{};
        PC_CHECK(pc_session_get_mod_info(
                     session, pool[i].session_mod_id, &info,
                     &error) == PC_RESULT_OK);
        const std::uint8_t family_sides = family_side_masks[info.family_id];
        if (pool[i].generation_type == PC_SIDE_PREFIX &&
            family_sides == 1u && prefix_goal == UINT32_MAX) {
            prefix_goal = pool[i].session_mod_id;
            prefix_info = info;
        } else if (pool[i].generation_type == PC_SIDE_PREFIX &&
                   prefix_junk.size() < 3 && found_multimod_prefix &&
                   info.family_id != multimod_prefix_info.family_id &&
                   info.primary_group_id !=
                       multimod_prefix_info.primary_group_id &&
                   std::none_of(
                       prefix_junk.begin(), prefix_junk.end(),
                       [&](const auto& existing) {
                           return existing.second.primary_group_id ==
                               info.primary_group_id;
                       })) {
            prefix_junk.emplace_back(pool[i].session_mod_id, info);
        } else if (pool[i].generation_type == PC_SIDE_SUFFIX &&
                   suffix_junk.size() < 3 && suffix_goal != UINT32_MAX &&
                   info.family_id != suffix_info.family_id &&
                   info.primary_group_id != suffix_info.primary_group_id &&
                   std::none_of(
                       suffix_junk.begin(), suffix_junk.end(),
                       [&](const auto& existing) {
                           return existing.second.primary_group_id ==
                               info.primary_group_id;
                       })) {
            suffix_junk.emplace_back(pool[i].session_mod_id, info);
        }
        if (prefix_goal != UINT32_MAX && suffix_goal != UINT32_MAX &&
            prefix_junk.size() == 3 && suffix_junk.size() == 3) {
            break;
        }
    }
    PC_CHECK(prefix_goal != UINT32_MAX);
    PC_CHECK(suffix_goal != UINT32_MAX);
    PC_CHECK(suffix_junk.size() == 3);
    PC_CHECK(prefix_junk.size() == 3);
    PC_CHECK(found_multimod_prefix);
    if (prefix_goal == UINT32_MAX || suffix_goal == UINT32_MAX ||
        prefix_junk.size() != 3 || suffix_junk.size() != 3) {
        pc_action_context_destroy(context);
        pc_session_destroy(session);
        pc_data_destroy(data);
        return;
    }

    const std::string goal =
        std::string(
            "{\"version\":\"v1\",\"rarity\":\"rare\","
            "\"action_mode\":\"goal_relevant\","
            "\"fossil_mode\":\"goal_relevant\","
            "\"min_satisfied_slots\":1,\"slots\":["
            "{\"family_mod_key\":\"") +
        suffix_info.key + "\",\"min_tier\":0}]}";
    pc_solver_handle solver = nullptr;
    PC_CHECK(pc_solver_create(
                 session, goal.c_str(), goal.size(), &solver,
                 &error) == PC_RESULT_OK);
    if (solver == nullptr) {
        pc_action_context_destroy(context);
        pc_session_destroy(session);
        pc_data_destroy(data);
        return;
    }

    std::uint32_t candidate_count = 0;
    PC_CHECK(pc_solver_candidates(
                 solver, nullptr, 0, &candidate_count,
                 &error) == PC_RESULT_OK);
    std::vector<std::uint32_t> candidates(candidate_count);
    PC_CHECK(pc_solver_candidates(
                 solver, candidates.data(), candidate_count,
                 &candidate_count, &error) == PC_RESULT_OK);
    PC_CHECK(candidate_count == 19);
    std::set<std::string> candidate_ids;
    for (const std::uint32_t candidate : candidates) {
        pc_solver_action_info info{};
        PC_CHECK(pc_solver_get_action_info(
                     solver, candidate, &info, &error) == PC_RESULT_OK);
        candidate_ids.insert(info.id);
    }
    for (const char* dependency : {
             "eldritch_ember:1", "eldritch_ichor:1",
             "eldritch_exalt", "eldritch_chaos",
             "eldritch_annul"}) {
        std::uint32_t index = 0;
        PC_CHECK(pc_solver_find_action(
                     solver, dependency, &index, &error) == PC_RESULT_OK);
        PC_CHECK(!candidate_ids.contains(dependency));
    }
    std::uint32_t corruption_essence = 0;
    PC_CHECK(pc_solver_find_action(
                 solver,
                 "essence:Metadata/Items/Currency/CurrencyEssenceHorror1",
                 &corruption_essence, &error) == PC_RESULT_NOT_FOUND);
    const std::string create_telemetry =
        solver_telemetry_json(solver, &error);
    PC_CHECK(create_telemetry.find(
                 "\"product_admission\":{\"goal_filtering\":true") !=
             std::string::npos);
    PC_CHECK(create_telemetry.find(
                 "\"automatic_eldritch_side_dependency\":") !=
             std::string::npos);
    PC_CHECK(create_telemetry.find("\"layout_primitives\":19") !=
             std::string::npos);
    PC_CHECK(create_telemetry.find(
                 "\"fossil_loadouts\":{\"possible\":15275,"
                 "\"generated\":0,\"deferred\":15275") !=
             std::string::npos);
    PC_CHECK(create_telemetry.find(
                 "\"filtered_corruption_only_essence\":4") !=
             std::string::npos);

    pc_item_state start = empty;
    PC_CHECK(pc_item_add_mod(
                 &start, PC_SIDE_PREFIX, prefix_goal,
                 static_cast<std::uint16_t>(prefix_info.primary_group_id),
                 0, nullptr) == PC_RESULT_OK);
    for (const auto& [mod, info] : suffix_junk) {
        PC_CHECK(pc_item_add_mod(
                     &start, PC_SIDE_SUFFIX, mod,
                     static_cast<std::uint16_t>(info.primary_group_id),
                     0, nullptr) == PC_RESULT_OK);
    }
    start.searing_exarch_tier = 1;
    start.eater_of_worlds_tier = 2;

    std::map<std::string, double> forced_prices{
        {"eldritch_chaos", 1.0},
        {"eldritch_annul", 1.0},
        {"eldritch_ember:1", 0.1},
        {"eldritch_ember:2", 0.1},
        {"eldritch_ember:3", 0.1},
        {"eldritch_ember:4", 0.1},
        {"eldritch_ichor:1", 0.1},
        {"eldritch_ichor:2", 0.1},
        {"eldritch_ichor:3", 0.1},
        {"eldritch_ichor:4", 0.1}};
    for (const std::uint32_t candidate : candidates) {
        pc_solver_action_info info{};
        PC_CHECK(pc_solver_get_action_info(
                     solver, candidate, &info, &error) == PC_RESULT_OK);
        if (!std::string(info.id).starts_with("bench:")) continue;
        for (std::uint32_t key = 0; key < info.cost_key_count; ++key) {
            forced_prices[info.cost_keys[key]] = 0.1;
        }
    }
    forced_prices["base"] = 1000.0;
    const std::string economy_json = solver_economy_json(forced_prices);
    pc_economy_handle economy = nullptr;
    PC_CHECK(pc_economy_load_json(
                 economy_json.c_str(), economy_json.size(), &economy,
                 &error) == PC_RESULT_OK);
    pc_solve_options solve_options{};
    solve_options.struct_size = sizeof(solve_options);
    solve_options.abi_version = PC_ABI_VERSION;
    solve_options.max_discovered_states = 200000;
    solve_options.max_expanded_states = 25000;
    solve_options.max_state_action_rows = 300000;
    solve_options.max_transitions = 10000000;
    solve_options.max_reforge_work = 100000000;
    solve_options.max_solver_owned_bytes = 512ull * 1024ull * 1024ull;
    solve_options.max_diagnostic_samples = 64;
    solve_options.max_telemetry_json_bytes = 64ull * 1024ull * 1024ull;
    solve_options.solver_flags =
        PC_SOLVER_FLAG_GOAL_PROGRESS_GATED_REFORGES |
        PC_SOLVER_FLAG_DISABLE_ECONOMIC_RESTART |
        PC_SOLVER_FLAG_DISABLE_IMPRINT_PROGRAMS;
    pc_solve_summary summary{};
    PC_CHECK(pc_solver_solve(
                 solver, &start, economy, &solve_options, &summary,
                 &error) == PC_RESULT_OK);
    PC_CHECK(summary.policy_available == 1);
    const std::string solved_telemetry =
        solver_telemetry_json(solver, &error);
    PC_CHECK(solved_telemetry.find(
                 "\"candidate_kind\":\"eldritch_side\"") !=
             std::string::npos);
    PC_CHECK(solved_telemetry.find(
                 "\"automatic_candidates\":{\"enabled\":true,"
                 "\"imprint_programs_considered\":false,"
                 "\"operators\":6,\"dependency_primitives\":2") !=
             std::string::npos);
    PC_CHECK(solved_telemetry.find(
                 "excluded:automatic_imprint_programs:caller_action_scope") !=
             std::string::npos);
    PC_CHECK(solved_telemetry.find(
                 "\"zero_off_policy\":true") != std::string::npos);
    PC_CHECK(solved_telemetry.find(
                 "\"start_scope\":\"zero_progress_reroll_and_no_"
                 "economic_restart_restrictions\"") !=
             std::string::npos);

    std::size_t strategy_length = 0;
    PC_CHECK(pc_solver_compile_strategy(
                 solver, nullptr, 0, &strategy_length,
                 &error) == PC_RESULT_OK);
    std::string strategy_json(strategy_length + 1, '\0');
    PC_CHECK(pc_solver_compile_strategy(
                 solver, strategy_json.data(), strategy_json.size(),
                 &strategy_length, &error) == PC_RESULT_OK);
    PC_CHECK(strategy_json.find("eldritch_annul") != std::string::npos ||
             strategy_json.find("eldritch_chaos") != std::string::npos);
    PC_CHECK(strategy_json.find(
                 "\"solver_policy_scope\":\"zero_progress_reroll_and_"
                 "no_economic_restart_restrictions\"") !=
             std::string::npos);
    PC_CHECK(strategy_json.find(
                 "\"solver_imprint_programs_considered\":false") !=
             std::string::npos);
    pc_strategy_handle strategy = nullptr;
    PC_CHECK(pc_strategy_compile_json(
                 session, strategy_json.c_str(), strategy_length,
                 &strategy, &error) == PC_RESULT_OK);
    pc_strategy_eval_options eval_options{};
    eval_options.struct_size = sizeof(eval_options);
    eval_options.abi_version = PC_ABI_VERSION;
    eval_options.economy = economy;
    std::size_t eval_length = 0;
    PC_CHECK(pc_strategy_evaluate(
                 strategy, &eval_options, nullptr, 0, &eval_length,
                 &error) == PC_RESULT_OK);
    std::string eval_json(eval_length + 1, '\0');
    PC_CHECK(pc_strategy_evaluate(
                 strategy, &eval_options, eval_json.data(), eval_json.size(),
                 &eval_length, &error) == PC_RESULT_OK);
    PC_CHECK(eval_json.find("\"success\":1") != std::string::npos);

    pc_simulator_handle simulator = nullptr;
    PC_CHECK(pc_simulator_create(
                 session, strategy, economy, &simulator,
                 &error) == PC_RESULT_OK);
    pc_simulation_options simulation_options{};
    simulation_options.struct_size = sizeof(simulation_options);
    simulation_options.abi_version = PC_ABI_VERSION;
    simulation_options.target_runs = 10000;
    simulation_options.seed = 20260808;
    simulation_options.max_actions_per_run = 100000;
    pc_simulation_progress progress{};
    PC_CHECK(pc_simulator_run_chunk(
                 simulator, &simulation_options, 10000, &progress,
                 &error) == PC_RESULT_OK);
    pc_simulation_summary simulation{};
    PC_CHECK(pc_simulator_get_summary(
                 simulator, &simulation, &error) == PC_RESULT_OK);
    PC_CHECK(simulation.completed_runs == 10000);
    PC_CHECK(simulation.success_count == simulation.completed_runs);
    PC_CHECK(simulation.failure_count == 0);
    PC_CHECK(simulation.missing_price_run_count == 0);
    std::printf(
        "solver API product Eldritch: V=%.6f states=%u runs=%llu\n",
        summary.start_value, summary.expanded_states,
        static_cast<unsigned long long>(simulation.completed_runs));

    pc_simulator_destroy(simulator);
    pc_strategy_destroy(strategy);

    std::map<std::string, double> missing_eldritch_prices = forced_prices;
    missing_eldritch_prices.erase("eldritch_annul");
    missing_eldritch_prices.erase("eldritch_chaos");
    const std::string missing_economy_json =
        solver_economy_json(missing_eldritch_prices);
    pc_economy_handle missing_economy = nullptr;
    PC_CHECK(pc_economy_load_json(
                 missing_economy_json.c_str(), missing_economy_json.size(),
                 &missing_economy, &error) == PC_RESULT_OK);
    pc_solve_summary missing_summary{};
    PC_CHECK(pc_solver_solve(
                 solver, &start, missing_economy, &solve_options,
                 &missing_summary, &error) == PC_RESULT_OK);
    PC_CHECK(missing_summary.policy_available == 0);
    const std::string missing_telemetry =
        solver_telemetry_json(solver, &error);
    PC_CHECK(missing_telemetry.find(
                 "automatic_candidate_missing_price") !=
             std::string::npos);
    PC_CHECK(missing_summary.skipped_missing_price_count > 0);
    pc_economy_destroy(missing_economy);

    pc_item_state influenced_start = start;
    influenced_start.generic_influence_bits = 1;
    pc_solve_summary influenced_summary{};
    PC_CHECK(pc_solver_solve(
                 solver, &influenced_start, economy, &solve_options,
                 &influenced_summary, &error) == PC_RESULT_OK);
    PC_CHECK(influenced_summary.policy_available == 0);
    const std::string influenced_telemetry =
        solver_telemetry_json(solver, &error);
    PC_CHECK(influenced_telemetry.find(
                 "setup_or_first_step_illegal") !=
                 std::string::npos ||
             influenced_telemetry.find(
                 "eldritch_side_influenced_carrier_illegal") !=
                 std::string::npos);

    const std::string prefix_eldritch_goal =
        std::string(
            "{\"version\":\"v1\",\"rarity\":\"rare\","
            "\"action_mode\":\"goal_relevant\","
            "\"fossil_mode\":\"goal_relevant\","
            "\"min_satisfied_slots\":2,\"slots\":["
            "{\"family_mod_key\":\"") +
        prefix_info.key +
        "\",\"min_tier\":0},{\"family_mod_key\":\"" +
        suffix_info.key + "\",\"min_tier\":0}]}";
    pc_solver_handle prefix_solver = nullptr;
    PC_CHECK(pc_solver_create(
                 session, prefix_eldritch_goal.c_str(),
                 prefix_eldritch_goal.size(), &prefix_solver,
                 &error) == PC_RESULT_OK);
    if (prefix_solver != nullptr) {
        pc_item_state prefix_start = empty;
        PC_CHECK(pc_item_add_mod(
                     &prefix_start, PC_SIDE_SUFFIX, suffix_goal,
                     static_cast<std::uint16_t>(
                         suffix_info.primary_group_id),
                     0, nullptr) == PC_RESULT_OK);
        prefix_start.searing_exarch_tier = 2;
        prefix_start.eater_of_worlds_tier = 1;
        std::map<std::string, double> prefix_prices{
            {"base", 1000.0},
            {"eldritch_exalt", 0.001},
            {"eldritch_chaos", 100.0},
            {"eldritch_annul", 100.0},
            {"eldritch_ember:1", 0.1},
            {"eldritch_ember:2", 0.1},
            {"eldritch_ember:3", 0.1},
            {"eldritch_ember:4", 0.1},
            {"eldritch_ichor:1", 0.1},
            {"eldritch_ichor:2", 0.1},
            {"eldritch_ichor:3", 0.1},
            {"eldritch_ichor:4", 0.1}};
        std::uint32_t prefix_candidate_count = 0;
        PC_CHECK(pc_solver_candidates(
                     prefix_solver, nullptr, 0, &prefix_candidate_count,
                     &error) == PC_RESULT_OK);
        std::vector<std::uint32_t> prefix_candidates(
            prefix_candidate_count);
        PC_CHECK(pc_solver_candidates(
                     prefix_solver, prefix_candidates.data(),
                     prefix_candidate_count, &prefix_candidate_count,
                     &error) == PC_RESULT_OK);
        for (const std::uint32_t candidate : prefix_candidates) {
            pc_solver_action_info info{};
            PC_CHECK(pc_solver_get_action_info(
                         prefix_solver, candidate, &info,
                         &error) == PC_RESULT_OK);
            if (!std::string(info.id).starts_with("bench:")) continue;
            for (std::uint32_t key = 0; key < info.cost_key_count; ++key) {
                prefix_prices[info.cost_keys[key]] = 0.1;
            }
        }
        const std::string prefix_economy_json =
            solver_economy_json(prefix_prices);
        pc_economy_handle prefix_economy = nullptr;
        PC_CHECK(pc_economy_load_json(
                     prefix_economy_json.c_str(), prefix_economy_json.size(),
                     &prefix_economy, &error) == PC_RESULT_OK);
        pc_solve_summary prefix_summary{};
        PC_CHECK(pc_solver_solve(
                     prefix_solver, &prefix_start, prefix_economy,
                     &solve_options, &prefix_summary,
                     &error) == PC_RESULT_OK);
        PC_CHECK(prefix_summary.policy_available == 1);
        const std::string prefix_telemetry =
            solver_telemetry_json(prefix_solver, &error);
        PC_CHECK(prefix_telemetry.find(
                     "\"eldritch_side\":{\"candidates\":") !=
                 std::string::npos);
        PC_CHECK(prefix_telemetry.find(
                     "\"zero_off_policy\":true") !=
                  std::string::npos);
        const std::string prefix_strategy =
            compile_and_exact_evaluate_public_policy(
                session, prefix_solver, prefix_economy, &error);
        PC_CHECK(prefix_strategy.find("eldritch_exalt") !=
                 std::string::npos);
        pc_strategy_handle prefix_compiled = nullptr;
        PC_CHECK(pc_strategy_compile_json(
                     session, prefix_strategy.c_str(),
                     prefix_strategy.size(), &prefix_compiled,
                     &error) == PC_RESULT_OK);
        if (prefix_compiled != nullptr) {
            pc_simulator_handle prefix_simulator = nullptr;
            PC_CHECK(pc_simulator_create(
                         session, prefix_compiled, prefix_economy,
                         &prefix_simulator, &error) == PC_RESULT_OK);
            pc_simulation_options prefix_simulation_options{};
            prefix_simulation_options.struct_size =
                sizeof(prefix_simulation_options);
            prefix_simulation_options.abi_version = PC_ABI_VERSION;
            prefix_simulation_options.target_runs = 10000;
            prefix_simulation_options.seed = 20260809;
            prefix_simulation_options.max_actions_per_run = 100000;
            pc_simulation_progress prefix_progress{};
            PC_CHECK(pc_simulator_run_chunk(
                         prefix_simulator, &prefix_simulation_options,
                         10000, &prefix_progress, &error) == PC_RESULT_OK);
            pc_simulation_summary prefix_simulation{};
            PC_CHECK(pc_simulator_get_summary(
                         prefix_simulator, &prefix_simulation,
                         &error) == PC_RESULT_OK);
            PC_CHECK(prefix_simulation.completed_runs == 10000);
            PC_CHECK(prefix_simulation.success_count == 10000);
            PC_CHECK(prefix_simulation.failure_count == 0);
            PC_CHECK(prefix_simulation.missing_price_run_count == 0);
            pc_simulator_destroy(prefix_simulator);
            pc_strategy_destroy(prefix_compiled);
        }
        pc_economy_destroy(prefix_economy);
        pc_solver_destroy(prefix_solver);
    }

    const std::string dependency_matrix_goal =
        std::string(
            "{\"version\":\"v1\",\"rarity\":\"rare\","
            "\"action_mode\":\"goal_relevant\","
            "\"fossil_mode\":\"goal_relevant\","
            "\"min_satisfied_slots\":2,\"slots\":["
            "{\"family_mod_key\":\"") +
        prefix_info.key +
        "\",\"min_tier\":0},{\"family_mod_key\":\"" +
        suffix_junk.front().second.key + "\",\"min_tier\":0}]}";
    pc_solver_handle dependency_solver = nullptr;
    PC_CHECK(pc_solver_create(
                 session, dependency_matrix_goal.c_str(),
                 dependency_matrix_goal.size(), &dependency_solver,
                 &error) == PC_RESULT_OK);
    if (dependency_solver != nullptr) {
        std::uint32_t dependency_candidate_count = 0;
        PC_CHECK(pc_solver_candidates(
                     dependency_solver, nullptr, 0,
                     &dependency_candidate_count, &error) == PC_RESULT_OK);
        std::vector<std::uint32_t> dependency_candidates(
            dependency_candidate_count);
        PC_CHECK(pc_solver_candidates(
                     dependency_solver, dependency_candidates.data(),
                     dependency_candidate_count,
                     &dependency_candidate_count, &error) == PC_RESULT_OK);
        std::set<std::uint32_t> dependency_candidate_set(
            dependency_candidates.begin(), dependency_candidates.end());
        for (const char* id : {
                 "bench:DexMasterItemGenerationCannotChangeSuffixes",
                 "bench:StrMasterItemGenerationCannotChangePrefixes",
                 "bench:IntMasterItemGenerationCannotRollAttackAffixes",
                 "bench:StrDexMasterItemGenerationCannotRollCasterAffixes",
                 "remove_crafted_modifiers"}) {
            std::uint32_t index = 0;
            PC_CHECK(pc_solver_find_action(
                         dependency_solver, id, &index,
                         &error) == PC_RESULT_OK);
            PC_CHECK(!dependency_candidate_set.contains(index));
        }
        const std::string dependency_telemetry =
            solver_telemetry_json(dependency_solver, &error);
        PC_CHECK(dependency_telemetry.find(
                     "automatic_protected_side_dependency") !=
                 std::string::npos);
        PC_CHECK(dependency_telemetry.find(
                     "automatic_cannot_roll_dependency") !=
                 std::string::npos);
        PC_CHECK(dependency_telemetry.find(
                     "automatic_temporary_bench_dependency") !=
                 std::string::npos);
        pc_solver_destroy(dependency_solver);
    }
    if (found_multimod_prefix) {
        const std::string multimod_goal =
            std::string(
                "{\"version\":\"v1\",\"rarity\":\"rare\","
                "\"action_mode\":\"goal_relevant\","
                "\"fossil_mode\":\"goal_relevant\","
                "\"min_satisfied_slots\":2,\"slots\":["
                "{\"family_mod_key\":\"") +
            multimod_prefix_info.key +
            "\",\"min_tier\":0},{\"family_mod_key\":\"" +
            suffix_info.key + "\",\"min_tier\":0}]}";
        pc_solver_handle multimod_solver = nullptr;
        PC_CHECK(pc_solver_create(
                     session, multimod_goal.c_str(), multimod_goal.size(),
                     &multimod_solver, &error) == PC_RESULT_OK);
        if (multimod_solver != nullptr) {
            std::uint32_t multimod = 0;
            PC_CHECK(pc_solver_find_action(
                         multimod_solver,
                         "bench:StrIntMasterItemGenerationCanHaveMultipleCraftedMods",
                         &multimod, &error) == PC_RESULT_OK);
            std::uint32_t multimod_candidate_count = 0;
            PC_CHECK(pc_solver_candidates(
                         multimod_solver, nullptr, 0,
                         &multimod_candidate_count,
                         &error) == PC_RESULT_OK);
            std::vector<std::uint32_t> multimod_candidates(
                multimod_candidate_count);
            PC_CHECK(pc_solver_candidates(
                         multimod_solver, multimod_candidates.data(),
                         multimod_candidate_count,
                         &multimod_candidate_count,
                         &error) == PC_RESULT_OK);
            PC_CHECK(std::find(
                         multimod_candidates.begin(),
                         multimod_candidates.end(), multimod) ==
                     multimod_candidates.end());
            const std::string multimod_telemetry =
                solver_telemetry_json(multimod_solver, &error);
            PC_CHECK(multimod_telemetry.find(
                         "automatic_multimod_dependency") !=
                     std::string::npos);
            pc_solver_destroy(multimod_solver);
        }
    }

    pc_solve_summary bench_summary{};
    PC_CHECK(pc_solver_solve(
                 solver, &empty, economy, &solve_options,
                 &bench_summary, &error) == PC_RESULT_OK);
    PC_CHECK(bench_summary.policy_available == 1);
    const std::string bench_strategy =
        compile_and_exact_evaluate_public_policy(
            session, solver, economy, &error);
    (void)bench_strategy;
    PC_CHECK(bench_summary.start_value < 1.0);

    pc_economy_destroy(economy);
    pc_solver_destroy(solver);
    pc_action_context_destroy(context);
    pc_session_destroy(session);

    pc_session_options ring_options{};
    ring_options.struct_size = sizeof(ring_options);
    ring_options.abi_version = PC_ABI_VERSION;
    ring_options.base_metadata_path =
        "Metadata/Items/Rings/Ring1";
    ring_options.item_level = 86;
    pc_session_handle ring_session = nullptr;
    PC_CHECK(pc_session_create(
                 data, &ring_options, &ring_session, &error) == PC_RESULT_OK);
    if (ring_session != nullptr) {
        pc_action_context_handle ring_context = nullptr;
        PC_CHECK(pc_action_context_create(
                     ring_session, &context_options, &ring_context,
                     &error) == PC_RESULT_OK);
        pc_item_state ring_item{};
        PC_CHECK(pc_item_init(
                     ring_session, &item_options, &ring_item,
                     &error) == PC_RESULT_OK);
        std::uint32_t ring_pool_count = 0;
        PC_CHECK(pc_action_context_debug_pool(
                     ring_context, &ring_item, -1, pool.data(),
                     static_cast<std::uint32_t>(pool.size()),
                     &ring_pool_count, &error) == PC_RESULT_OK);
        PC_CHECK(ring_pool_count > 0);
        if (ring_pool_count > 0) {
            pc_mod_info ring_goal_mod{};
            PC_CHECK(pc_session_get_mod_info(
                         ring_session, pool[0].session_mod_id,
                         &ring_goal_mod, &error) == PC_RESULT_OK);
            const std::string ring_goal =
                std::string(
                    "{\"version\":\"v1\",\"rarity\":\"rare\","
                    "\"action_mode\":\"goal_relevant\","
                    "\"fossil_mode\":\"goal_relevant\",\"slots\":["
                    "{\"family_mod_key\":\"") +
                ring_goal_mod.key + "\",\"min_tier\":0}]}";
            pc_solver_handle ring_solver = nullptr;
            PC_CHECK(pc_solver_create(
                         ring_session, ring_goal.c_str(), ring_goal.size(),
                         &ring_solver, &error) == PC_RESULT_OK);
            if (ring_solver != nullptr) {
                std::uint32_t eldritch = 0;
                PC_CHECK(pc_solver_find_action(
                             ring_solver, "eldritch_annul", &eldritch,
                             &error) == PC_RESULT_NOT_FOUND);
                PC_CHECK(pc_solver_find_action(
                             ring_solver, "eldritch_exalt", &eldritch,
                             &error) == PC_RESULT_NOT_FOUND);
                const std::string ring_telemetry =
                    solver_telemetry_json(ring_solver, &error);
                PC_CHECK(ring_telemetry.find(
                             "automatic_eldritch_side_dependency") ==
                         std::string::npos);
                pc_solver_destroy(ring_solver);
            }
        }
        pc_action_context_destroy(ring_context);
        pc_session_destroy(ring_session);
    }
    pc_data_destroy(data);
}

void run_public_product_reforge_family_gate(const char* artifact_dir) {
    pc_error_info error;
    pc_error_info_init(&error);
    pc_data_handle data = nullptr;
    const std::string manifest =
        std::string(artifact_dir) + "/manifest.json";
    PC_CHECK(pc_data_load_file(
                 manifest.c_str(), &data, &error) == PC_RESULT_OK);
    if (data == nullptr) return;

    pc_session_options session_options{};
    session_options.struct_size = sizeof(session_options);
    session_options.abi_version = PC_ABI_VERSION;
    session_options.base_metadata_path =
        "Metadata/Items/Armours/BodyArmours/BodyInt17";
    session_options.item_level = 86;
    pc_session_handle session = nullptr;
    PC_CHECK(pc_session_create(
                 data, &session_options, &session, &error) == PC_RESULT_OK);
    if (session == nullptr) {
        pc_data_destroy(data);
        return;
    }
    pc_action_context_options context_options{};
    context_options.struct_size = sizeof(context_options);
    context_options.abi_version = PC_ABI_VERSION;
    context_options.seed = 20260808;
    pc_action_context_handle context = nullptr;
    PC_CHECK(pc_action_context_create(
                 session, &context_options, &context,
                 &error) == PC_RESULT_OK);
    pc_item_init_options item_options{};
    item_options.struct_size = sizeof(item_options);
    item_options.abi_version = PC_ABI_VERSION;
    item_options.rarity = PC_RARITY_RARE;
    item_options.with_implicits = 0;
    pc_item_state start{};
    PC_CHECK(pc_item_init(
                 session, &item_options, &start,
                 &error) == PC_RESULT_OK);

    pc_solve_options solve_options{};
    solve_options.struct_size = sizeof(solve_options);
    solve_options.abi_version = PC_ABI_VERSION;
    solve_options.max_discovered_states = 200000;
    solve_options.max_expanded_states = 25000;
    solve_options.max_state_action_rows = 300000;
    solve_options.max_transitions = 10000000;
    solve_options.max_reforge_work = 100000000;
    solve_options.max_solver_owned_bytes = 512ull * 1024ull * 1024ull;
    solve_options.max_diagnostic_samples = 64;
    solve_options.max_telemetry_json_bytes = 64ull * 1024ull * 1024ull;
    solve_options.solver_flags =
        PC_SOLVER_FLAG_GOAL_PROGRESS_GATED_REFORGES;

    const auto run_forced_winner = [&](const std::string& goal,
                                       const std::string& action_id,
                                       const std::string& strategy_token) {
        std::string solve_goal = goal;
        const std::string product_fossil_mode =
            ",\"fossil_mode\":\"goal_relevant\"";
        if (const std::size_t fossil_mode =
                solve_goal.find(product_fossil_mode);
            fossil_mode != std::string::npos) {
            solve_goal.erase(fossil_mode, product_fossil_mode.size());
        }
        PC_CHECK(!solve_goal.empty() && solve_goal.back() == '}');
        if (!solve_goal.empty() && solve_goal.back() == '}') {
            solve_goal.pop_back();
            solve_goal += ",\"actions\":[\"" + action_id +
                          "\",\"chaos\",\"restart\"]}";
        }
        pc_solver_handle solver = nullptr;
        PC_CHECK(pc_solver_create(
                     session, solve_goal.c_str(), solve_goal.size(), &solver,
                     &error) == PC_RESULT_OK);
        if (solver == nullptr) return;
        std::uint32_t action = 0;
        PC_CHECK(pc_solver_find_action(
                     solver, action_id.c_str(), &action,
                     &error) == PC_RESULT_OK);
        std::uint32_t candidate_count = 0;
        PC_CHECK(pc_solver_candidates(
                     solver, nullptr, 0, &candidate_count,
                     &error) == PC_RESULT_OK);
        std::vector<std::uint32_t> candidates(candidate_count);
        PC_CHECK(pc_solver_candidates(
                     solver, candidates.data(), candidate_count,
                     &candidate_count, &error) == PC_RESULT_OK);
        PC_CHECK(std::find(
                     candidates.begin(), candidates.end(), action) !=
                 candidates.end());
        pc_solver_action_info info{};
        PC_CHECK(pc_solver_get_action_info(
                     solver, action, &info, &error) == PC_RESULT_OK);
        std::uint32_t outcome_count = 0;
        pc_calc_summary outcome_summary{};
        PC_CHECK(pc_calc_action_outcomes(
                     solver, &start, action, nullptr, 0, &outcome_count,
                     &outcome_summary, &error) == PC_RESULT_OK);
        PC_CHECK(outcome_summary.supported == 1);
        PC_CHECK(outcome_summary.legal == 1);
        PC_CHECK(outcome_summary.success_probability > 0.0);
        std::map<std::string, double> prices{
            {"base", 1.0e6}, {"chaos", 1.0e6}};
        for (std::uint32_t key = 0; key < info.cost_key_count; ++key) {
            prices[info.cost_keys[key]] = 1.0;
        }
        const std::string economy_json = solver_economy_json(prices);
        pc_economy_handle economy = nullptr;
        PC_CHECK(pc_economy_load_json(
                     economy_json.c_str(), economy_json.size(), &economy,
                     &error) == PC_RESULT_OK);
        pc_solve_summary summary{};
        PC_CHECK(pc_solver_solve(
                     solver, &start, economy, &solve_options, &summary,
                     &error) == PC_RESULT_OK);
        PC_CHECK(summary.policy_available == 1);
        PC_CHECK(std::isfinite(summary.start_value));
        PC_CHECK(summary.start_value < 1.0e6);
        if (summary.policy_available == 1) {
            const std::string strategy =
                compile_and_exact_evaluate_public_policy(
                    session, solver, economy, &error);
            PC_CHECK(strategy.find(strategy_token) != std::string::npos);
        }
        pc_economy_destroy(economy);
        pc_solver_destroy(solver);
    };

    const std::string energy_shield_goal =
        R"({"version":"v1","rarity":"rare","action_mode":"goal_relevant","fossil_mode":"goal_relevant","min_satisfied_slots":1,"slots":[{"family_mod_key":"LocalIncreasedEnergyShield11","min_tier":1}]})";
    pc_solver_handle filter_solver = nullptr;
    PC_CHECK(pc_solver_create(
                 session, energy_shield_goal.c_str(),
                 energy_shield_goal.size(), &filter_solver,
                 &error) == PC_RESULT_OK);
    if (filter_solver != nullptr) {
        std::uint32_t unused = 0;
        PC_CHECK(pc_solver_find_action(
                     filter_solver, "harvest_reforge:defences", &unused,
                     &error) == PC_RESULT_OK);
        PC_CHECK(pc_solver_find_action(
                     filter_solver, "harvest_reforge:fire", &unused,
                     &error) == PC_RESULT_NOT_FOUND);
        PC_CHECK(pc_solver_find_action(
                     filter_solver,
                     "fossil:Metadata/Items/Currency/CurrencyDelveCraftingDefences",
                     &unused, &error) == PC_RESULT_OK);
        PC_CHECK(pc_solver_find_action(
                     filter_solver,
                     "essence:Metadata/Items/Currency/CurrencyEssenceHorror1",
                     &unused, &error) == PC_RESULT_NOT_FOUND);
        const std::string telemetry =
            solver_telemetry_json(filter_solver, &error);
        PC_CHECK(telemetry.find("candidate_goal_tag_harvest") !=
                 std::string::npos);
        PC_CHECK(telemetry.find("filtered_harvest_without_goal_tag") !=
                 std::string::npos);
        PC_CHECK(telemetry.find(
                     "candidate_bounded_goal_relevant_fossil") !=
                 std::string::npos);
        PC_CHECK(telemetry.find("\"layout_primitives\":17") !=
                 std::string::npos);
        PC_CHECK(telemetry.find(
                     "\"fossil_loadouts\":{\"possible\":15275,"
                     "\"generated\":4,\"deferred\":15271") !=
                 std::string::npos);
        PC_CHECK(telemetry.find("filtered_corruption_only_essence") !=
                 std::string::npos);
        pc_solver_destroy(filter_solver);
    }
    run_forced_winner(
        energy_shield_goal, "harvest_reforge:defences",
        "harvest_reforge");
    run_forced_winner(
        energy_shield_goal,
        "fossil:Metadata/Items/Currency/CurrencyDelveCraftingDefences",
        "fossil");

    pc_item_state essence_item{};
    item_options.rarity = PC_RARITY_NORMAL;
    PC_CHECK(pc_item_init(
                 session, &item_options, &essence_item,
                 &error) == PC_RESULT_OK);
    pc_action_request essence_request{};
    essence_request.struct_size = sizeof(essence_request);
    essence_request.abi_version = PC_ABI_VERSION;
    essence_request.action_type = PC_ACTION_ESSENCE;
    essence_request.essence_key =
        "Metadata/Items/Currency/CurrencyEssenceAnguish2";
    pc_action_result essence_result{};
    PC_CHECK(pc_apply_action(
                 context, &essence_item, &essence_request, &essence_result,
                 &error) == PC_RESULT_OK);
    PC_CHECK(essence_result.applied == 1);
    std::string essence_goal_key;
    const auto inspect_essence_mod = [&](const pc_mod_slot& slot) {
        if (!essence_goal_key.empty() || slot.mod_id == PC_MOD_NONE) return;
        pc_mod_info info{};
        PC_CHECK(pc_session_get_mod_info(
                     session, slot.mod_id, &info,
                     &error) == PC_RESULT_OK);
        if (info.reach_kind == PC_MOD_REACH_ESSENCE) {
            essence_goal_key = info.key;
        }
    };
    for (std::uint8_t i = 0; i < essence_item.prefix_count; ++i) {
        inspect_essence_mod(essence_item.prefixes[i]);
    }
    for (std::uint8_t i = 0; i < essence_item.suffix_count; ++i) {
        inspect_essence_mod(essence_item.suffixes[i]);
    }
    PC_CHECK(!essence_goal_key.empty());
    if (!essence_goal_key.empty()) {
        const std::string essence_goal =
            std::string(
                "{\"version\":\"v1\",\"rarity\":\"rare\","
                "\"action_mode\":\"goal_relevant\","
                "\"fossil_mode\":\"goal_relevant\","
                "\"min_satisfied_slots\":1,\"slots\":[{"
                "\"family_mod_key\":\"") +
            essence_goal_key + "\",\"min_tier\":1}]}";
        pc_solver_handle essence_solver = nullptr;
        PC_CHECK(pc_solver_create(
                     session, essence_goal.c_str(), essence_goal.size(),
                     &essence_solver, &error) == PC_RESULT_OK);
        std::string relevant_essence;
        if (essence_solver != nullptr) {
            std::uint32_t essence_candidate_count = 0;
            PC_CHECK(pc_solver_candidates(
                         essence_solver, nullptr, 0,
                         &essence_candidate_count,
                         &error) == PC_RESULT_OK);
            std::vector<std::uint32_t> essence_candidates(
                essence_candidate_count);
            PC_CHECK(pc_solver_candidates(
                         essence_solver, essence_candidates.data(),
                         essence_candidate_count,
                         &essence_candidate_count,
                         &error) == PC_RESULT_OK);
            for (const std::uint32_t candidate : essence_candidates) {
                pc_solver_action_info info{};
                PC_CHECK(pc_solver_get_action_info(
                             essence_solver, candidate, &info,
                             &error) == PC_RESULT_OK);
                if (std::string(info.id).starts_with("essence:")) {
                    relevant_essence = info.id;
                    break;
                }
            }
            const std::string essence_telemetry =
                solver_telemetry_json(essence_solver, &error);
            PC_CHECK(essence_telemetry.find(
                         "candidate_exact_essence_goal") !=
                     std::string::npos);
            PC_CHECK(essence_telemetry.find(
                         "filtered_essence_without_exact_goal_mod") !=
                     std::string::npos);
            pc_solver_destroy(essence_solver);
        }
        PC_CHECK(!relevant_essence.empty());
        if (!relevant_essence.empty()) {
            run_forced_winner(
                essence_goal, relevant_essence, "essence");
        }
    }

    pc_action_context_destroy(context);
    pc_session_destroy(session);
    pc_data_destroy(data);
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
    std::set<std::string> influence_exalt_ids;
    for (uint32_t action_index = 0; action_index < action_count;
         ++action_index) {
        pc_solver_action_info info{};
        PC_CHECK(pc_solver_get_action_info(
                     solver, action_index, &info, &error) == PC_RESULT_OK);
        const std::string id = info.id;
        if (id.rfind("influence_exalt:", 0) == 0) {
            influence_exalt_ids.insert(id);
            PC_CHECK(info.cost_key_count == 1);
            PC_CHECK(std::string(info.cost_keys[0]) == id);
        }
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
    PC_CHECK(influence_exalt_ids == std::set<std::string>({
                 "influence_exalt:crusader",
                 "influence_exalt:hunter",
                 "influence_exalt:redeemer",
                 "influence_exalt:warlord"}));
    for (const char* unavailable : {
             "influence_exalt:adjudicator",
             "influence_exalt:basilisk",
             "influence_exalt:elder",
             "influence_exalt:eyrie",
             "influence_exalt:shaper"}) {
        uint32_t unavailable_index = 0;
        PC_CHECK(pc_solver_find_action(
                     solver, unavailable, &unavailable_index, &error) ==
                 PC_RESULT_NOT_FOUND);
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
    PC_CHECK(solve_summary.policy_available == 1);
    const std::string initial_solve_telemetry =
        solver_telemetry_json(solver, &error);
    const bool policy_guided_refined =
        initial_solve_telemetry.find(
            "\"solution_scope\":"
            "\"policy_guided_exact_refinement_bounded\"") !=
        std::string::npos;
    PC_CHECK(
        solve_summary.converged ==
        (policy_guided_refined ? 0 : 1));
    PC_CHECK(
        solve_summary.policy_status ==
        (policy_guided_refined
             ? PC_SOLVE_POLICY_BOUNDED_FEASIBLE
             : PC_SOLVE_POLICY_EXACT));
    PC_CHECK(solve_summary.termination ==
             PC_SOLVE_TERMINATION_EXACT_CLOSED);
    PC_CHECK(solve_summary.stop_cause == PC_SOLVE_STOP_EXACT_CLOSED);
    PC_CHECK(solve_summary.cap_hit_mask == 0);
    PC_CHECK(solve_summary.upper_bound == solve_summary.start_value);
    PC_CHECK(solve_summary.evaluated_policy_cost ==
             solve_summary.start_value);
    if (policy_guided_refined) {
        PC_CHECK(solve_summary.lower_bound == 0.0);
        PC_CHECK(
            solve_summary.absolute_optimality_gap ==
            solve_summary.start_value);
        PC_CHECK(std::isinf(
            solve_summary.relative_optimality_gap));
    } else {
        PC_CHECK(
            solve_summary.lower_bound ==
            solve_summary.start_value);
        PC_CHECK(
            solve_summary.absolute_optimality_gap == 0.0);
        PC_CHECK(
            solve_summary.relative_optimality_gap == 0.0);
    }
    PC_CHECK(solve_summary.start_value > 0.0);
    PC_CHECK(solve_summary.skipped_action_count == 0);
    PC_CHECK(solve_summary.registry_action_count >=
             solve_summary.candidate_action_count);
    PC_CHECK(solve_summary.candidate_action_count >=
             solve_summary.evaluator_supported_action_count);
    PC_CHECK(solve_summary.evaluator_supported_action_count >=
             solve_summary.supported_priced_action_count);
    PC_CHECK(solve_summary.skipped_action_count ==
             solve_summary.skipped_missing_price_count +
                 solve_summary.skipped_unsupported_count);

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

    /* Requested bounded finish is a publication boundary, not cancellation
     * and not a fabricated resource cap. It must remain cooperative even
     * when requested during a partially expanded carrier. */
    static_assert(PC_SOLVE_TERMINATION_REQUESTED_BOUNDED_FINISH == 6);
    static_assert(PC_SOLVE_STOP_REQUESTED_BOUNDED_FINISH == 13);
    PC_CHECK(pc_solver_solve_begin(solver, &item, economy, nullptr, &error) ==
             PC_RESULT_OK);
    PC_CHECK(pc_solver_solve_step(solver, 1, &solve_progress, &error) ==
             PC_RESULT_OK);
    PC_CHECK(!solve_progress.done);
    PC_CHECK(pc_solver_solve_request_bounded_finish(solver, &error) ==
             PC_RESULT_OK);
    const std::string requested_telemetry =
        solver_telemetry_json(solver, &error);
    PC_CHECK(requested_telemetry.find(
                 "\"requested_bounded_finish\":true") !=
             std::string::npos);
    PC_CHECK(pc_solver_solve_step(solver, 1, &solve_progress, &error) ==
             PC_RESULT_OK);
    PC_CHECK(solve_progress.phase == PC_SOLVE_PHASE_REFINING ||
             solve_progress.phase == PC_SOLVE_PHASE_COMPILING ||
             solve_progress.phase == PC_SOLVE_PHASE_CERTIFYING ||
             solve_progress.done);
    if (solve_progress.done) {
        pc_solve_summary requested_summary{};
        PC_CHECK(pc_solver_solve_finish(
                     solver, &requested_summary, &error) == PC_RESULT_OK);
        PC_CHECK(requested_summary.converged == 0);
        PC_CHECK(requested_summary.termination ==
                 PC_SOLVE_TERMINATION_REQUESTED_BOUNDED_FINISH);
        PC_CHECK(requested_summary.stop_cause ==
                 PC_SOLVE_STOP_REQUESTED_BOUNDED_FINISH);
        PC_CHECK(requested_summary.cap_hit_mask == 0);
    } else {
        pc_solver_solve_abandon(solver);
    }
    PC_CHECK(pc_solver_solve_request_bounded_finish(solver, &error) ==
             PC_RESULT_NOT_FOUND);

    /* Finalization phases are append-only ABI values and remain abandonable;
     * Done is reserved for an already-certified, packaging-only result. */
    static_assert(PC_SOLVE_PHASE_EXPANDING == 1);
    static_assert(PC_SOLVE_PHASE_ITERATING == 2);
    static_assert(PC_SOLVE_PHASE_DONE == 3);
    static_assert(PC_SOLVE_PHASE_REFINING == 4);
    static_assert(PC_SOLVE_PHASE_COMPILING == 5);
    static_assert(PC_SOLVE_PHASE_CERTIFYING == 6);
    PC_CHECK(pc_solver_solve_begin(solver, &item, economy, nullptr, &error) ==
             PC_RESULT_OK);
    bool reached_retained_finalization = false;
    for (std::uint32_t step = 0; step < 100000; ++step) {
        PC_CHECK(pc_solver_solve_step(
                     solver, 1, &solve_progress, &error) ==
                 PC_RESULT_OK);
        reached_retained_finalization =
            solve_progress.phase == PC_SOLVE_PHASE_REFINING ||
            solve_progress.phase == PC_SOLVE_PHASE_COMPILING ||
            solve_progress.phase == PC_SOLVE_PHASE_CERTIFYING;
        if (reached_retained_finalization || solve_progress.done) break;
    }
    PC_CHECK(reached_retained_finalization);
    PC_CHECK(!solve_progress.done);
    pc_solver_solve_abandon(solver);
    PC_CHECK(pc_solver_solve_step(solver, 1, &solve_progress, &error) ==
             PC_RESULT_NOT_FOUND);

    PC_CHECK(pc_solver_solve_begin(solver, &item, economy, nullptr, &error) ==
             PC_RESULT_OK);
    uint32_t step_count = 0;
    bool saw_refining = false;
    bool saw_compiling = false;
    bool saw_certifying = false;
    do {
        const uint32_t budget = step_count % 3 == 0 ? 1 : 7;
        PC_CHECK(pc_solver_solve_step(solver, budget, &solve_progress,
                                      &error) == PC_RESULT_OK);
        PC_CHECK(solve_progress.expanded_states > 0);
        PC_CHECK(solve_progress.start_value_bound >= 0.0);
        saw_refining |=
            solve_progress.phase == PC_SOLVE_PHASE_REFINING;
        saw_compiling |=
            solve_progress.phase == PC_SOLVE_PHASE_COMPILING;
        saw_certifying |=
            solve_progress.phase == PC_SOLVE_PHASE_CERTIFYING;
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
    PC_CHECK(saw_refining || saw_compiling || saw_certifying);
    PC_CHECK(solve_progress.finalization_work_items > 0);
    PC_CHECK(solve_progress.certification_discovered_pairs > 0);
    /* Done includes exact refinement and independent policy evaluation;
     * finish below only transfers the already-certified result. */
    PC_CHECK(solve_progress.lower_bound >= 0.0);
    PC_CHECK(
        solve_progress.upper_bound >=
        solve_progress.lower_bound);

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
    PC_CHECK(stepped_summary.stop_cause == solve_summary.stop_cause);
    PC_CHECK(stepped_summary.cap_hit_mask == solve_summary.cap_hit_mask);
    PC_CHECK(stepped_summary.registry_action_count ==
             solve_summary.registry_action_count);
    PC_CHECK(stepped_summary.candidate_action_count ==
             solve_summary.candidate_action_count);
    PC_CHECK(stepped_summary.evaluator_supported_action_count ==
             solve_summary.evaluator_supported_action_count);
    PC_CHECK(stepped_summary.supported_priced_action_count ==
             solve_summary.supported_priced_action_count);
    PC_CHECK(stepped_summary.skipped_missing_price_count ==
             solve_summary.skipped_missing_price_count);
    PC_CHECK(stepped_summary.skipped_unsupported_count ==
             solve_summary.skipped_unsupported_count);
    PC_CHECK(stepped_summary.lower_bound == solve_summary.lower_bound);
    PC_CHECK(stepped_summary.upper_bound == solve_summary.upper_bound);
    PC_CHECK(stepped_summary.evaluated_policy_cost ==
             solve_summary.evaluated_policy_cost);
    solve_summary = stepped_summary;
    const std::string solved_telemetry =
        solver_telemetry_json(solver, &error);
    PC_CHECK(solved_telemetry.find(
                 "\"transition_bits_hash\":\"067ac4c6510645e6\"") !=
             std::string::npos);
    PC_CHECK(solved_telemetry.find(
                 "\"policy_bits_hash\":\"bfcb25789b4f99ae\"") !=
             std::string::npos);
    const bool stepped_policy_guided_refined =
        solved_telemetry.find(
            "\"solution_scope\":"
            "\"policy_guided_exact_refinement_bounded\"") !=
        std::string::npos;
    const bool retained_exact_strategy =
        solved_telemetry.find(
            "\"publication\":{\"status\":\"exact_core_policy\","
            "\"candidate_kind\":\"direct_compiled_core_policy\"}") !=
            std::string::npos ||
        solved_telemetry.find(
            "\"candidate_kind\":\"policy_guided_exact_refinement\"}") !=
            std::string::npos;
    PC_CHECK(
        stepped_policy_guided_refined ==
        policy_guided_refined);
    if (policy_guided_refined) {
        PC_CHECK(solved_telemetry.find(
                     "\"status\":\"bounded_feasible\"") !=
                 std::string::npos);
        PC_CHECK(solved_telemetry.find(
                     "\"start_status\":"
                     "\"bounded_feasible_certificate\"") !=
                 std::string::npos);
        PC_CHECK(solved_telemetry.find(
                     "\"policy_result\":{\"available\":true,"
                     "\"status\":\"bounded_feasible\"") !=
                 std::string::npos);
    } else {
        PC_CHECK(solved_telemetry.find(
                     "\"status\":\"exact_abstract\"") !=
                 std::string::npos);
        PC_CHECK(solved_telemetry.find(
                     "\"start_status\":"
                     "\"exact_abstract_within_tolerance\"") !=
                 std::string::npos);
        PC_CHECK(solved_telemetry.find(
                     "\"policy_result\":{\"available\":true,"
                     "\"status\":\"exact\"") !=
                 std::string::npos);
        PC_CHECK(solved_telemetry.find(
                     "\"global_lower_bound_certified\":true,"
                     "\"lower_bound_provenance\":"
                     "\"exact_policy_closure\"") !=
                 std::string::npos);
    }
    const json::Value solved_telemetry_document =
        parse_solver_api_fixture(solved_telemetry);
    const json::Value& retained_work =
        solved_telemetry_document.at("work");
    PC_CHECK(retained_work.at("state_action_rows").as_int() > 0);
    PC_CHECK(retained_work.at("transition_entries").as_int() > 0);
    const json::Value& automatic_admission_work =
        solved_telemetry_document.at("action_control")
            .at("automatic_candidates")
            .at("admission_phases")
            .at("work");
    PC_CHECK(
        automatic_admission_work.at("discovered_states").as_int() == 0);
    PC_CHECK(
        automatic_admission_work.at("state_action_rows").as_int() == 0);
    PC_CHECK(
        automatic_admission_work.at("transition_entries").as_int() == 0);
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
    constexpr double kUntouchedValue = -12345.0;
    constexpr const char* kUntouchedAction = "untouched";
    double start_value = kUntouchedValue;
    const char* start_action = kUntouchedAction;
    const pc_result state_value_result =
        pc_solver_state_value(
            solver, start_state, &start_value,
            &start_action, &error);
    if (retained_exact_strategy) {
        PC_CHECK(
            state_value_result ==
            PC_RESULT_UNSUPPORTED_FEATURE);
        PC_CHECK(error.code == PC_RESULT_UNSUPPORTED_FEATURE);
        PC_CHECK(std::strstr(
                     error.message,
                     "per-state coarse policy queries are unavailable") !=
                 nullptr);
        PC_CHECK(start_value == kUntouchedValue);
        PC_CHECK(start_action == kUntouchedAction);
    } else {
        PC_CHECK(state_value_result == PC_RESULT_OK);
        PC_CHECK(start_value == solve_summary.start_value);
        PC_CHECK(start_action != nullptr);
    }

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

    /*
     * Compile the policy and verify it through the public simulator. A
     * refined publication already owns its exact JSON inside SolveResult;
     * the buffer query must not duplicate that document into the ordinary
     * handle cache.
     */
    const std::string refinement_prefix =
        "\"policy_refinement\":{\"triggers\":";
    const std::size_t refinement_position =
        solved_telemetry.find(refinement_prefix);
    PC_CHECK(refinement_position != std::string::npos);
    const bool policy_refinement_triggered =
        solved_telemetry.find(
            "\"policy_refinement\":{\"triggers\":0") ==
        std::string::npos;
    PC_CHECK(solved_telemetry.find("\"resource_cap\":null") !=
             std::string::npos);
    if (policy_refinement_triggered) {
        PC_CHECK(solved_telemetry.find(
                     "\"status\":\"complete\","
                     "\"resource_cap\":null") !=
                 std::string::npos);
        PC_CHECK(solved_telemetry.find(
                     "\"fixed_point\":{\"checked\":true,"
                     "\"complete\":true,"
                     "\"lumpability_checked\":true,"
                     "\"lumpable\":true") !=
                 std::string::npos);
        PC_CHECK(solved_telemetry.find(
                     "\"class_policy\":{\"checked\":true,"
                     "\"proper\":true}") !=
                 std::string::npos);
    } else {
        PC_CHECK(solved_telemetry.find(
                     "\"policy_refinement\":{\"triggers\":0,"
                     "\"status\":\"direct_core_policy_exact\","
                     "\"resource_cap\":null") !=
                 std::string::npos);
        PC_CHECK(solved_telemetry.find(
                     "\"publication\":{"
                     "\"status\":\"exact_core_policy\","
                     "\"candidate_kind\":"
                     "\"direct_compiled_core_policy\"}") !=
                 std::string::npos);
    }
    PC_CHECK(solved_telemetry.find(
                 "\"compiled_assertion\":{\"checked\":true,"
                 "\"proper\":true,\"zero_off_policy\":true,"
                 "\"cost_reconciled\":true}") !=
             std::string::npos);
    pc_native_memory_stats memory_before_compile{};
    PC_CHECK(pc_solver_memory_stats(
                 solver, &memory_before_compile, &error) ==
             PC_RESULT_OK);
    size_t strategy_length = 0;
    PC_CHECK(pc_solver_compile_strategy(solver, nullptr, 0,
                                        &strategy_length, &error) ==
             PC_RESULT_OK);
    PC_CHECK(strategy_length > 0);
    pc_native_memory_stats memory_after_length_query{};
    PC_CHECK(pc_solver_memory_stats(
                 solver, &memory_after_length_query, &error) ==
             PC_RESULT_OK);
    if (retained_exact_strategy) {
        PC_CHECK(
            memory_after_length_query.serialized_output_bytes ==
            memory_before_compile.serialized_output_bytes);
    } else {
        PC_CHECK(
            memory_after_length_query.serialized_output_bytes >
            memory_before_compile.serialized_output_bytes);
    }
    std::string strategy_json(strategy_length + 1, '\0');
    PC_CHECK(pc_solver_compile_strategy(solver, strategy_json.data(),
                                        strategy_json.size(),
                                        &strategy_length, &error) ==
             PC_RESULT_OK);
    pc_native_memory_stats memory_after_strategy_copy{};
    PC_CHECK(pc_solver_memory_stats(
                 solver, &memory_after_strategy_copy, &error) ==
             PC_RESULT_OK);
    PC_CHECK(
        memory_after_strategy_copy.serialized_output_bytes ==
        memory_after_length_query.serialized_output_bytes);
    const std::string compiled_telemetry =
        solver_telemetry_json(solver, &error);
    PC_CHECK(compiled_telemetry.find("\"available\":true") !=
             std::string::npos);
    PC_CHECK(compiled_telemetry.find(
                 "\"strategy_json_bytes\":" +
                 std::to_string(strategy_length)) != std::string::npos);
    PC_CHECK(compiled_telemetry.find(
                 "\"peak_owned_bytes\":") != std::string::npos);

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
    const char* policy_label =
        retained_exact_strategy ? "exact-router" : start_action;
    std::printf(
        "solver api gate: V=%.4f empirical=%.4f (%u states, policy %s)\n",
        solve_summary.start_value, empirical, solve_summary.expanded_states,
        policy_label);
    PC_CHECK(std::fabs(empirical - solve_summary.start_value) < 0.25);

    pc_simulator_destroy(simulator);
    pc_strategy_destroy(strategy);

    /*
     * Starting replacement invalidates the prior result and releases an
     * ordinary compiled cache before new SolveWork is constructed. A refined
     * publication never populated that cache, so its serialized-output count
     * remains flat instead of growing.
     */
    PC_CHECK(pc_solver_solve_begin(
                 solver, &item, economy, nullptr, &error) ==
             PC_RESULT_OK);
    pc_native_memory_stats replacement_memory{};
    PC_CHECK(pc_solver_memory_stats(
                 solver, &replacement_memory, &error) ==
             PC_RESULT_OK);
    PC_CHECK(
        replacement_memory.serialized_output_bytes <=
        memory_after_strategy_copy.serialized_output_bytes);
    if (!retained_exact_strategy) {
        PC_CHECK(
            replacement_memory.serialized_output_bytes <
            memory_after_strategy_copy.serialized_output_bytes);
    }
    pc_solver_solve_abandon(solver);
    size_t unavailable_strategy_length = 0;
    PC_CHECK(pc_solver_compile_strategy(
                 solver, nullptr, 0, &unavailable_strategy_length,
                 &error) == PC_RESULT_NOT_FOUND);

    /* A no-policy cap keeps the cap as the public stopping cause. */
    pc_solve_options capped_options{};
    capped_options.struct_size = sizeof(capped_options);
    capped_options.abi_version = PC_ABI_VERSION;
    capped_options.max_states = 1;
    pc_solve_summary capped_summary{};
    PC_CHECK(pc_solver_solve(
                 solver, &item, economy, &capped_options,
                 &capped_summary, &error) == PC_RESULT_OK);
    PC_CHECK(capped_summary.converged == 0);
    PC_CHECK(capped_summary.policy_available == 0);
    PC_CHECK(capped_summary.policy_status == PC_SOLVE_POLICY_NONE);
    PC_CHECK(capped_summary.termination ==
             PC_SOLVE_TERMINATION_REFUSED_RESOURCE_CAP);
    PC_CHECK(capped_summary.stop_cause == PC_SOLVE_STOP_STATE_CAP);
    PC_CHECK(
        (capped_summary.cap_hit_mask & PC_SOLVE_CAP_STATE) != 0);

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
    run_public_product_imprint_current_gate(artifact_dir);
    run_public_product_eldritch_gate(artifact_dir);
    run_public_product_reforge_family_gate(artifact_dir);
    run_natural_t1_feasibility_gate(artifact_dir);
}

void run_solver_feasibility_tests(const char* artifact_dir) {
    if (artifact_dir == nullptr) {
        std::printf("solver feasibility suite skipped (missing path)\n");
        return;
    }
    run_natural_t1_feasibility_gate(artifact_dir);
}
