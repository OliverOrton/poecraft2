#include "tests.hpp"
#include "../src/recombination_solver.hpp"
#include "../src/handles_internal.hpp"
#include "../src/json.hpp"
#include "poecraft/recombination_solver.h"
#include <cmath>
#include <cstring>
#include <limits>

void run_recombination_solver_tests(const char* artifact_dir) {
    using namespace poecraft;
    // Physical duplicates affect the count row; selecting removes full groups.
    std::vector<RecombOccurrence> pool{
        {0, 0, 10, 0, 100, {1}}, {1, 0, 10, 0, 100, {1}}, {1, 1, 11, 1, 100, {2}}};
    double duplicate_success = 0;
    for (const auto& outcome : enumerate_random_recomb_side(pool))
        if (outcome.occurrences.size() == 2) duplicate_success += outcome.probability;
    PC_CHECK(std::abs(duplicate_success - .6) < 1e-10);
    pool[1] = {0, 1, 12, 2, 100, {3}};
    double filler_success = 0;
    for (const auto& outcome : enumerate_random_recomb_side(pool)) {
        bool a = false, b = false;
        for (auto index : outcome.occurrences) { a |= pool[index].global_mod_id == 10; b |= pool[index].global_mod_id == 11; }
        if (a && b) filler_success += outcome.probability;
    }
    PC_CHECK(std::abs(filler_success - (.1 + .5/3)) < 1e-10 && filler_success < .333);
    pool[0].groups.push_back(77); pool[2].groups.push_back(77);
    for (const auto& outcome : enumerate_random_recomb_side(pool)) {
        bool a = false, b = false;
        for (auto index : outcome.occurrences) { a |= pool[index].global_mod_id == 10; b |= pool[index].global_mod_id == 11; }
        PC_CHECK(!(a && b)); // secondary group membership blocks, not only primary
    }
    if (!artifact_dir) { PC_CHECK(false); return; }
    pc_error_info error{}; pc_data_handle data = nullptr;
    const auto manifest = std::string(artifact_dir) + "/manifest.json";
    PC_CHECK(pc_data_load_file(manifest.c_str(), &data, &error) == PC_RESULT_OK);
    if (!data) return;
    pc_session_options options{}; options.struct_size = sizeof(options); options.abi_version = PC_ABI_VERSION;
    options.base_metadata_path = "Metadata/Items/Rings/Ring1"; options.item_level = 80;
    pc_session_handle session = nullptr;
    PC_CHECK(pc_session_create(data, &options, &session, &error) == PC_RESULT_OK);
    if (!session) { pc_data_destroy(data); return; }
    std::vector<unsigned> mods;
    const auto& s = *session->impl;
    for (unsigned m = 0; m < s.mod_count && mods.size() < 2; ++m)
        if (s.gen_type[m] == 0 && !s.flags[m] && s.special_kind[m] < 0 &&
            s.metamod_type[m] < 0 && s.influence_code[m] <= 0 && s.base_spawn_weight[m] > 0) {
            bool overlap = false;
            for (auto old : mods) for (auto i = s.group_offsets[m]; i < s.group_offsets[m+1]; ++i)
                for (auto j = s.group_offsets[old]; j < s.group_offsets[old+1]; ++j)
                    if (s.group_ids[i] == s.group_ids[j]) overlap = true;
            if (!overlap) mods.push_back(m);
        }
    PC_CHECK(mods.size() == 2);
    if (mods.size() != 2) { pc_session_destroy(session); pc_data_destroy(data); return; }
    pc_item_state a{}, b{}, ab{};
    pc_item_clear(&a); pc_item_clear(&b); pc_item_clear(&ab);
    a.rarity = b.rarity = PC_RARITY_MAGIC; ab.rarity = PC_RARITY_RARE;
    for (unsigned i = 0; i < 2; ++i) {
        auto& item = i ? b : a;
        PC_CHECK(pc_item_add_mod(&item, PC_SIDE_PREFIX, mods[i], static_cast<uint16_t>(s.primary_group[mods[i]]), 0, nullptr) == PC_RESULT_OK);
        PC_CHECK(pc_item_add_mod(&ab, PC_SIDE_PREFIX, mods[i], static_cast<uint16_t>(s.primary_group[mods[i]]), 0, nullptr) == PC_RESULT_OK);
    }
    // Native pair preparation must accept the magic alt-built feeder pathway.
    const auto pair = prepare_random_recomb_pair({"a", "a", session->impl, a}, {"b", "b", session->impl, b});
    double mass = 0, success = 0;
    for (const auto& outcome : enumerate_random_recomb_pair(pair)) {
        mass += outcome.probability;
        const auto item = materialize_random_recomb_outcome(pair, outcome);
        PC_CHECK(item.rarity == PC_RARITY_RARE);
        if (item.prefix_count == 2) success += outcome.probability;
    }
    PC_CHECK(std::abs(mass - 1) < 1e-10 && std::abs(success - .333) < 1e-10);
    const auto key = [&](unsigned id) { return s.data->string_at(s.data->mod_key_sid.at(s.global_index.at(id))); };
    const auto goal = std::string(R"({"version":"calculator_goal_set_v1","actions":[],"goals":[{"id":"target","goal":{"version":"v1","rarity":"rare","slots":[{"family_mod_key":")") +
        key(mods[0]) + R"(","min_tier":)" + std::to_string(s.family_tier_index[mods[0]]) +
        R"(},{"family_mod_key":")" + key(mods[1]) + R"(","min_tier":)" +
        std::to_string(s.family_tier_index[mods[1]]) + R"(}]}}]})";
    RecombSolverRequest request; request.session = session->impl; request.price_identity = "fixture-prices";
    request.goal_set_json = goal; request.recombination_cost_chaos = 1; request.recombination_cost_complete = true;
    request.acquisitions = {{"a", "purchase", "quote-a", a, 1, true},
        {"b", "completed_feeder", "quote-b-full-retry-cost", b, 1, true},
        {"finished", "completed_feeder", "quote-finished-multimod-comparison", ab, 20, true}};
    const auto solved = solve_random_recomb_inventory(request);
    PC_CHECK(solved.search_converged && solved.cost_complete && !solved.global_optimality_claim);
    PC_CHECK(std::abs(solved.expected_recombinations - 1/.333) < 1e-7);
    // One first acquisition plus one complementary fresh item per attempt.
    PC_CHECK(std::abs(solved.expected_acquisitions[0] + solved.expected_acquisitions[1] - (1 + 1/.333)) < 1e-7);
    PC_CHECK(solved.expected_acquisitions[2] == 0);
    PC_CHECK(std::abs(solved.expected_cost_chaos - (1 + 2/.333)) < 1e-7);
    PC_CHECK(solved.expected_cost_chaos < 3/.333); // beats discarding both failure inputs
    for (const auto& decision : solved.policy) {
        double row_mass = 0; for (auto [next, p] : decision.outcomes) { PC_CHECK(next < solved.inventories.size() && p > 0); row_mass += p; }
        PC_CHECK(std::abs(row_mass - 1) < 1e-10);
    }
    auto cheaper_finish = request; cheaper_finish.acquisitions[2].total_cost_chaos = 2;
    const auto finished = solve_random_recomb_inventory(cheaper_finish);
    PC_CHECK(std::abs(finished.expected_cost_chaos - 2) < 1e-9 && finished.expected_recombinations == 0 &&
        finished.expected_acquisitions[2] == 1);
    const auto refusal = [&](RecombSolverRequest invalid) {
        bool refused = false; try { (void)solve_random_recomb_inventory(invalid); } catch (const std::exception&) { refused = true; }
        PC_CHECK(refused);
    };
    auto invalid = request; invalid.recombination_cost_complete = false; refusal(invalid);
    invalid = request; invalid.acquisitions[0].cost_complete = false; refusal(invalid);
    invalid = request; invalid.acquisitions[0].total_cost_chaos = std::numeric_limits<double>::quiet_NaN(); refusal(invalid);
    invalid = request; invalid.max_items = 1; refusal(invalid);
    invalid = request; invalid.max_states = 1; refusal(invalid);
    invalid = request; invalid.max_work = 1; refusal(invalid);
    invalid = request; invalid.cancelled = [] { return true; }; refusal(invalid);
    invalid = request; invalid.initial_items = {a}; invalid.initial_item_costs = {}; refusal(invalid);
    invalid = request; invalid.goal_set_json = R"({"version":"calculator_goal_set_v1","goals":[{"id":"numeric","goal":{"version":"v1","slots":[],"rolled_stat_total":123}}]})"; refusal(invalid);
    invalid = request; invalid.acquisitions[0].total_cost_chaos = 0; invalid.acquisitions[1].total_cost_chaos = 0;
    invalid.acquisitions[2].total_cost_chaos = 1; invalid.recombination_cost_chaos = 0;
    const auto zero = solve_random_recomb_inventory(invalid);
    PC_CHECK(zero.expected_cost_chaos == 0 && std::isfinite(zero.expected_recombinations) && zero.expected_recombinations > 0);
    pc_recombination_acquisition offers[] = {
        {"a", "purchase", "quote-a", &a, 1, 1}, {"b", "completed_feeder", "quote-b", &b, 1, 1},
        {"finished", "completed_feeder", "quote-finished", &ab, 20, 1}};
    pc_recombination_solver_options input{};
    input.struct_size = sizeof(input); input.abi_version = PC_ABI_VERSION; input.solver_version = PC_RECOMBINATION_SOLVER_VERSION;
    input.model_id = kRandomRecombModel; input.price_identity = "fixture-prices";
    input.goal_set_json = goal.data(); input.goal_set_json_size = goal.size();
    input.acquisitions = offers; input.acquisition_count = 3; input.recombination_cost_chaos = 1; input.recombination_cost_complete = 1;
    pc_recombination_solver_handle handle = nullptr;
    const auto before_a = a, before_b = b;
    PC_CHECK(pc_recombination_solver_create(session, &input, &handle, &error) == PC_RESULT_OK);
    PC_CHECK(std::memcmp(&a, &before_a, sizeof(a)) == 0 && std::memcmp(&b, &before_b, sizeof(b)) == 0);
    if (handle) {
        size_t length = 0;
        PC_CHECK(pc_recombination_solver_result_json(handle, nullptr, 0, &length, &error) == PC_RESULT_BUFFER_TOO_SMALL);
        std::vector<char> text(length + 1);
        PC_CHECK(pc_recombination_solver_result_json(handle, text.data(), text.size(), &length, &error) == PC_RESULT_OK);
        const auto parsed = json::Parser(text.data(), length).parse();
        PC_CHECK(parsed.at("economic_inputs_declared").as_bool() && !parsed.at("native_feeder_cost_certified").as_bool());
        PC_CHECK(std::abs(parsed.at("expected_cost_chaos").as_number() - solved.expected_cost_chaos) < 1e-8);
        pc_session_handle mapping = nullptr;
        PC_CHECK(pc_recombination_solver_session(handle, &mapping, &error) == PC_RESULT_OK);
        pc_item_state exact{};
        PC_CHECK(pc_recombination_solver_item(handle, 0, &exact, &error) == PC_RESULT_OK);
        pc_recombination_solver_destroy(handle); handle = nullptr;
        pc_base_info info{};
        PC_CHECK(pc_session_get_base_info(mapping, &info, &error) == PC_RESULT_OK && info.item_level == 80);
        pc_session_destroy(mapping);
    }
    input.model_id = "wrong-model";
    PC_CHECK(pc_recombination_solver_create(session, &input, &handle, &error) == PC_RESULT_INVALID_ARGUMENT && !handle);
    pc_recombination_solver_destroy(handle); pc_session_destroy(session); pc_data_destroy(data);
}
