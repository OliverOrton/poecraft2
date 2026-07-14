#include "tests.hpp"

#include "../src/solver_internal.hpp"
#include "poecraft/bitset.h"
#include "poecraft/item_state.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

using namespace poecraft;
using namespace poecraft::solver;

namespace {

/*
 * The eight-mod weighted universe again, extended with the identity data
 * (mod keys, group keys, base path) that compiled strategy conditions
 * reference.
 */
std::shared_ptr<SessionImpl> make_compile_session() {
    auto data = std::make_shared<DataImpl>();
    data->mod_global_ids = {0, 1, 2, 3, 4, 5, 6, 7};
    data->strings = {"",       "synthetic/base", "mod0", "mod1", "mod2",
                     "mod3",   "mod4",           "mod5", "mod6", "mod7",
                     "g10",    "g11",            "g12",  "g13",  "g20",
                     "g21",    "g22"};
    data->base_count = 1;
    data->base_metadata_path_sid = {1};
    data->mod_key_sid = {2, 3, 4, 5, 6, 7, 8, 9};
    for (std::uint32_t mod = 0; mod < 8; ++mod) {
        data->mod_pos_by_key.emplace(
            data->strings[data->mod_key_sid[mod]], mod);
    }
    data->group_key_sids.assign(23, 0);
    const auto group_key = [&](std::uint32_t group, std::uint32_t sid) {
        data->group_key_sids[group] = sid;
        data->group_id_by_key.emplace(data->strings[sid], group);
    };
    group_key(10, 10);
    group_key(11, 11);
    group_key(12, 12);
    group_key(13, 13);
    group_key(20, 14);
    group_key(21, 15);
    group_key(22, 16);

    auto session = std::make_shared<SessionImpl>();
    session->data = data;
    session->base_index = 0;
    session->item_level = 1;
    session->mod_count = 8;
    session->words = pc_bitset_words(8);
    session->global_index = {0, 1, 2, 3, 4, 5, 6, 7};
    session->gen_type = {0, 0, 0, 0, 0, 1, 1, 1};
    session->primary_group = {10, 10, 10, 12, 13, 20, 21, 22};
    session->required_level = {1, 1, 1, 1, 1, 1, 1, 1};
    session->group_offsets = {0, 1, 2, 4, 5, 6, 7, 8, 9};
    session->group_ids = {10, 10, 10, 11, 12, 13, 20, 21, 22};
    session->family_id = {100, 100, 101, 102, 103, 104, 105, 106};
    session->family_tier_index = {1, 2, 1, 1, 1, 1, 1, 1};
    session->metamod_type.assign(8, -1);
    session->special_kind.assign(8, -1);
    session->flags.assign(8, 0);
    session->influence_code.assign(8, -1);
    session->class_offsets = {0, 0, 0, 0, 1, 2, 4, 6, 7};
    session->class_tag_ids = {1, 2, 3, 6, 4, 6, 5};
    session->rare_affix_cap = 3;
    session->base_spawn_weight = {100, 100, 100, 100, 100, 100, 100, 400};
    session->base_gen_pct.assign(8, 100);
    session->base_roll_weight = session->base_spawn_weight;
    for (std::uint32_t mod = 0; mod < 8; ++mod) {
        session->session_id_by_global_id.emplace(mod, mod);
    }

    const std::size_t words = session->words;
    session->normal_random_roll_mask.assign(words, 0);
    session->positive_spawn_weight_mask.assign(words, 0);
    session->positive_base_weight_mask.assign(words, 0);
    session->prefix_mask.assign(words, 0);
    session->suffix_mask.assign(words, 0);
    session->unveiled_mask.assign(words, 0);
    session->implicit_tag_masks.assign(7, {});
    session->group_masks.assign(23, {});
    session->influence_masks.assign(1, std::vector<std::uint64_t>(words, 0));
    for (std::uint32_t mod = 0; mod < 8; ++mod) {
        pc_bitset_set(session->normal_random_roll_mask.data(), mod);
        pc_bitset_set(session->positive_spawn_weight_mask.data(), mod);
        pc_bitset_set(session->positive_base_weight_mask.data(), mod);
        pc_bitset_set(session->influence_masks[0].data(), mod);
        pc_bitset_set((mod < 5 ? session->prefix_mask : session->suffix_mask)
                          .data(),
                      mod);
        const std::uint32_t begin = session->group_offsets[mod];
        const std::uint32_t end = session->group_offsets[mod + 1];
        for (std::uint32_t i = begin; i < end; ++i) {
            auto& mask = session->group_masks[session->group_ids[i]];
            if (mask.empty()) mask.assign(words, 0);
            pc_bitset_set(mask.data(), mod);
        }
        const std::uint32_t class_begin = session->class_offsets[mod];
        const std::uint32_t class_end = session->class_offsets[mod + 1];
        for (std::uint32_t i = class_begin; i < class_end; ++i) {
            auto& mask = session->implicit_tag_masks[
                session->class_tag_ids[i]];
            if (mask.empty()) mask.assign(words, 0);
            pc_bitset_set(mask.data(), mod);
        }
    }
    return session;
}

/* Compile the strategy JSON and run it through the native simulator. */
SimulationSummaryInternal run_compiled(
    std::shared_ptr<const SessionImpl> session,
    const std::string& strategy_json,
    const std::unordered_map<std::string, double>& prices,
    std::uint64_t runs,
    std::uint64_t seed) {
    auto strategy = compile_strategy_json(
        session, strategy_json.c_str(), strategy_json.size());
    auto economy = std::make_shared<EconomyImpl>();
    economy->id = "test";
    for (const auto& [key, value] : prices) {
        economy->prices.emplace(key, value);
    }
    SimulatorImpl simulator;
    simulator.session = session;
    simulator.strategy = strategy;
    simulator.economy = economy;
    simulator.action_counts.assign(strategy->nodes.size(), 0);

    SimulationOptionsInternal options;
    options.target_runs = runs;
    options.seed = seed;
    options.max_actions_per_run = 100000;
    run_simulator_chunk(simulator, options,
                        static_cast<std::uint32_t>(runs));
    return simulator.summary;
}

void run_synthetic_gate() {
    auto session = make_compile_session();
    ActionRegistry registry = build_action_registry(*session);
    GoalSpec goal;
    GoalSlot slot;
    slot.family_id = 100;
    slot.min_tier = 1;
    goal.slots.push_back(slot);
    goal.rarity = PC_RARITY_MAGIC;
    const std::uint32_t transmute = registry.index_by_id.at("transmute");
    const std::uint32_t alteration = registry.index_by_id.at("alteration");
    const std::uint32_t restart = registry.index_by_id.at("restart");
    CalcContext calc(session, goal, registry,
                     {transmute, alteration, restart});

    const double p =
        0.5 * (100.0 / 1100.0) +
        0.5 * (100.0 / 1100.0 + (600.0 / 1100.0) * (100.0 / 500.0));

    pc_item_state start;
    pc_item_clear(&start);

    /* Alt-spam policy: solve -> compile -> simulate must reproduce
     * V(start) = 1/p empirically. */
    {
        const std::unordered_map<std::string, double> prices{
            {"transmute", 1.0}, {"alteration", 1.0}, {"base", 10.0}};
        const SolveResult solved = solve(calc, start, prices);
        PC_CHECK(solved.converged);
        const std::string json =
            compile_policy_strategy_json(calc, solved, "alt-spam");
        PC_CHECK(json.find("\"type\":\"restart\"") == std::string::npos);
        PC_CHECK(json.find("\"expected_cost\":") != std::string::npos);

        const SimulationSummaryInternal summary =
            run_compiled(session, json, prices, 50000, 42);
        PC_CHECK(summary.completed_runs == 50000);
        PC_CHECK(summary.success_count == summary.completed_runs);
        PC_CHECK(summary.missing_price_run_count == 0);
        const double mean =
            summary.known_total_cost /
            static_cast<double>(summary.completed_runs);
        const double expected = solved.values[solved.start_state];
        std::printf("solver compile alt-spam: V=%.4f empirical=%.4f\n",
                    expected, mean);
        PC_CHECK(std::fabs(mean - expected) < 0.15);
        PC_CHECK(std::fabs(expected - 1.0 / p) < 1e-6);
    }

    /* Price flip: the compiled strategy must include restart operations
     * and still land on V(start) = (2-p)/p. */
    {
        const std::unordered_map<std::string, double> prices{
            {"transmute", 1.0}, {"alteration", 100.0}, {"base", 1.0}};
        const SolveResult solved = solve(calc, start, prices);
        PC_CHECK(solved.converged);
        const std::string json =
            compile_policy_strategy_json(calc, solved, "restart-heavy");
        PC_CHECK(json.find("\"type\":\"restart\"") != std::string::npos);

        const SimulationSummaryInternal summary =
            run_compiled(session, json, prices, 30000, 4242);
        PC_CHECK(summary.completed_runs == 30000);
        PC_CHECK(summary.success_count == summary.completed_runs);
        const double mean =
            summary.known_total_cost /
            static_cast<double>(summary.completed_runs);
        const double expected = solved.values[solved.start_state];
        std::printf("solver compile restart: V=%.4f empirical=%.4f\n",
                    expected, mean);
        PC_CHECK(std::fabs(mean - expected) < 0.4);
        PC_CHECK(std::fabs(expected - (2.0 - p) / p) < 1e-6);
    }

    /* Vocabulary gaps must throw, not mis-compile: a corrupted start
     * carries flags no condition can test. */
    {
        const std::unordered_map<std::string, double> prices{
            {"transmute", 1.0}, {"alteration", 1.0}, {"base", 10.0}};
        pc_item_state corrupted = start;
        corrupted.item_flags = PC_ITEM_CORRUPTED;
        const SolveResult solved = solve(calc, corrupted, prices);
        PC_CHECK(solved.converged);
        bool threw = false;
        try {
            compile_policy_strategy_json(calc, solved, "gap");
        } catch (const std::runtime_error&) {
            threw = true;
        }
        PC_CHECK(threw);
    }

    /* A partial slot threshold compiles to the simulator's native
     * at_least condition instead of silently reverting to all slots. */
    {
        GoalSpec threshold_goal;
        GoalSlot life;
        life.family_id = 100;
        life.min_tier = 1;
        GoalSlot fire_res;
        fire_res.family_id = 104;
        threshold_goal.slots = {life, fire_res};
        threshold_goal.rarity = PC_RARITY_MAGIC;
        threshold_goal.min_satisfied_slots = 1;
        CalcContext threshold_calc(
            session, threshold_goal, registry,
            {transmute, alteration, restart});
        const std::unordered_map<std::string, double> prices{
            {"transmute", 1.0}, {"alteration", 1.0}, {"base", 10.0}};
        const SolveResult solved = solve(threshold_calc, start, prices);
        PC_CHECK(solved.converged);
        const std::string json = compile_policy_strategy_json(
            threshold_calc, solved, "one-of-two");
        PC_CHECK(json.find("\"type\":\"at_least\",\"count\":1") !=
                 std::string::npos);
        const SimulationSummaryInternal summary =
            run_compiled(session, json, prices, 2000, 777);
        PC_CHECK(summary.completed_runs == 2000);
        PC_CHECK(summary.success_count == summary.completed_runs);
    }
}

bool read_text_file(const std::string& path, std::string& out) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) return false;
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    out = buffer.str();
    return true;
}

/* End-to-end gate on the real artifact: pinned one-mod goal, basic
 * currency plus restart, simulate-vs-V(start). */
void run_artifact_gate(const char* artifact_dir) {
    if (artifact_dir == nullptr) {
        std::printf("solver compile artifact suite skipped (missing path)\n");
        return;
    }
    const std::string dir = artifact_dir;
    std::string manifest_text;
    std::string strings_text;
    std::string game_text;
    if (!read_text_file(dir + "/manifest.json", manifest_text) ||
        !read_text_file(dir + "/strings.json", strings_text) ||
        !read_text_file(dir + "/game-data.json", game_text)) {
        std::printf("solver compile artifact suite skipped (unreadable)\n");
        return;
    }
    std::shared_ptr<DataImpl> data;
    std::shared_ptr<SessionImpl> session;
    try {
        data = load_data_impl(manifest_text, strings_text, game_text);
        const auto base = data->base_by_path.find(
            "Metadata/Items/Armours/BodyArmours/BodyInt17");
        PC_CHECK(base != data->base_by_path.end());
        if (base == data->base_by_path.end()) return;
        session = std::make_shared<SessionImpl>();
        session->data = data;
        session->base_index = base->second;
        session->item_level = 86;
        build_session(*session);
    } catch (const std::exception& ex) {
        std::printf("solver compile artifact suite: %s\n", ex.what());
        PC_CHECK(false);
        return;
    }

    ActionRegistry registry = build_action_registry(*session);

    /* Pick a goal group whose members are all single-group so no blocker
     * states arise (hybrid blockers need conditions v2). */
    GoalSpec goal;
    GoalSlot slot;
    for (std::uint32_t mod = 0; mod < session->mod_count &&
                                slot.group_id == kNoId; ++mod) {
        if (session->gen_type[mod] != 0 ||
            !pc_bitset_test(session->normal_random_roll_mask.data(), mod) ||
            !pc_bitset_test(session->positive_base_weight_mask.data(),
                            mod)) {
            continue;
        }
        const std::uint32_t group = session->primary_group[mod];
        if (group >= session->group_masks.size() ||
            session->group_masks[group].empty()) {
            continue;
        }
        bool clean = true;
        pc_bitset_for_each(
            session->group_masks[group].data(), session->words,
            [&](std::size_t member) {
                if (session->group_offsets[member + 1] -
                        session->group_offsets[member] !=
                    1) {
                    clean = false;
                }
            });
        if (clean) slot.group_id = group;
    }
    PC_CHECK(slot.group_id != kNoId);
    if (slot.group_id == kNoId) return;
    goal.slots.push_back(slot);

    std::vector<std::uint32_t> candidates;
    for (const char* id : {"transmute", "augment", "alteration", "regal",
                           "alchemy", "chaos", "exalt", "annul", "scour",
                           "restart"}) {
        candidates.push_back(registry.index_by_id.at(id));
    }
    CalcContext calc(session, goal, registry, candidates);
    const std::unordered_map<std::string, double> prices{
        {"transmute", 0.1}, {"augment", 0.5}, {"alteration", 0.2},
        {"regal", 1.0},     {"alchemy", 0.5}, {"chaos", 1.0},
        {"exalt", 20.0},    {"annul", 3.0},   {"scour", 0.5},
        {"base", 5.0}};

    pc_item_state start;
    pc_item_clear(&start);
    start.rarity = PC_RARITY_RARE;
    const SolveResult solved = solve(calc, start, prices);
    PC_CHECK(solved.converged);
    const double expected = solved.values[solved.start_state];

    const std::string json =
        compile_policy_strategy_json(calc, solved, "artifact-toy");
    const SimulationSummaryInternal summary =
        run_compiled(session, json, prices, 20000, 987654321);
    PC_CHECK(summary.completed_runs == 20000);
    PC_CHECK(summary.success_count == summary.completed_runs);
    PC_CHECK(summary.missing_price_run_count == 0);
    const double mean = summary.known_total_cost /
                        static_cast<double>(summary.completed_runs);
    std::printf(
        "solver compile artifact: V=%.4f empirical=%.4f over %llu runs\n",
        expected, mean,
        static_cast<unsigned long long>(summary.completed_runs));
    PC_CHECK(std::fabs(mean - expected) < 0.25);
}

} // namespace

void run_solver_compile_tests(const char* artifact_dir) {
    run_synthetic_gate();
    run_artifact_gate(artifact_dir);
}
