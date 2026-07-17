#include "tests.hpp"

#include "../src/solver_internal.hpp"
#include "poecraft/bitset.h"
#include "poecraft/item_state.h"

#include <algorithm>
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
 * The eight ordinary-mod weighted universe again, plus dedicated prefix and
 * suffix veiled placeholders and the identity data (mod keys, group keys,
 * base path) that compiled strategy conditions reference.
 */
std::shared_ptr<SessionImpl> make_compile_session() {
    auto data = std::make_shared<DataImpl>();
    data->mod_global_ids = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    data->spawn_offsets = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    data->spawn_tag_ids.assign(10, 0);
    data->spawn_weights = {
        100, 100, 100, 100, 100, 100, 100, 400, 100, 100};
    data->mod_gen_type_code.assign(10, 0);
    data->strings = {"",       "synthetic/base", "mod0", "mod1", "mod2",
                     "mod3",   "mod4",           "mod5", "mod6", "mod7",
                     "g10",    "g11",            "g12",  "g13",  "g20",
                     "g21",    "g22",            "mod8", "mod9",
                     "g30",    "g31"};
    data->base_count = 1;
    data->base_metadata_path_sid = {1};
    data->mod_key_sid = {2, 3, 4, 5, 6, 7, 8, 9, 17, 18};
    for (std::uint32_t mod = 0; mod < 10; ++mod) {
        data->mod_pos_by_key.emplace(
            data->strings[data->mod_key_sid[mod]], mod);
    }
    data->group_key_sids.assign(32, 0);
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
    group_key(30, 19);
    group_key(31, 20);

    auto session = std::make_shared<SessionImpl>();
    session->data = data;
    session->base_index = 0;
    session->item_level = 1;
    session->mod_count = 10;
    session->words = pc_bitset_words(10);
    session->global_index = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    session->gen_type = {0, 0, 0, 0, 0, 1, 1, 1, 0, 1};
    session->primary_group = {10, 10, 10, 12, 13, 20, 21, 22, 30, 31};
    session->required_level.assign(10, 1);
    session->group_offsets = {0, 1, 2, 4, 5, 6, 7, 8, 9, 10, 11};
    session->group_ids = {10, 10, 10, 11, 12, 13, 20, 21, 22, 30, 31};
    session->family_id = {
        100, 100, 101, 102, 103, 104, 105, 106, 107, 108};
    session->family_tier_index.assign(10, 1);
    session->family_tier_index[1] = 2;
    session->metamod_type.assign(10, -1);
    session->special_kind.assign(10, -1);
    session->flags.assign(10, 0);
    session->influence_code.assign(10, -1);
    session->class_offsets = {0, 0, 0, 0, 1, 2, 4, 6, 7, 7, 7};
    session->class_tag_ids = {1, 2, 3, 6, 4, 6, 5};
    session->rare_affix_cap = 3;
    session->base_spawn_weight = {
        100, 100, 100, 100, 100, 100, 100, 400, 100, 100};
    session->base_gen_pct.assign(10, 100);
    session->base_roll_weight = session->base_spawn_weight;
    session->effective_base_tag_ids = {0};
    for (std::uint32_t mod = 0; mod < 10; ++mod) {
        session->session_id_by_global_id.emplace(mod, mod);
    }

    const std::size_t words = session->words;
    session->normal_random_roll_mask.assign(words, 0);
    session->positive_spawn_weight_mask.assign(words, 0);
    session->positive_base_weight_mask.assign(words, 0);
    session->prefix_mask.assign(words, 0);
    session->suffix_mask.assign(words, 0);
    session->unveiled_mask.assign(words, 0);
    session->unveiled_generic_mask.assign(words, 0);
    session->implicit_tag_masks.assign(7, {});
    session->group_masks.assign(32, {});
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
    for (std::uint32_t mod = 0; mod < 8; ++mod) {
        pc_bitset_set(session->unveiled_mask.data(), mod);
        pc_bitset_set(session->unveiled_generic_mask.data(), mod);
    }
    session->veiled_prefix_mod_id = 8;
    session->veiled_suffix_mod_id = 9;
    session->eldritch_eligible = true;
    session->eldritch_searing_tier_mod_ids.resize(5);
    session->eldritch_eater_tier_mod_ids.resize(5);
    session->eldritch_searing_tier_mod_ids[1] = {0};
    session->eldritch_eater_tier_mod_ids[1] = {5};
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
    for (const FailureSummaryInternal& failure :
         simulator.failure_summaries) {
        std::printf(
            "compiled strategy failure: node=%s reason=%d count=%llu detail=%s\n",
            failure.node_id.c_str(), failure.failure_reason,
            static_cast<unsigned long long>(failure.count),
            failure.detail.c_str());
    }
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
        PolicyCompilationTelemetry compilation;
        const std::string json = compile_policy_strategy_json(
            calc, solved, "alt-spam", &compilation);
        PC_CHECK(json.find("\"type\":\"restart\"") == std::string::npos);
        PC_CHECK(json.find("\"expected_cost\":") != std::string::npos);
        PC_CHECK(compilation.working_states > 0);
        PC_CHECK(compilation.nodes > 0);
        PC_CHECK(compilation.edges > compilation.nodes);
        PC_CHECK(compilation.strategy_json_bytes == json.size());

        const SimulationSummaryInternal summary =
            run_compiled(session, json, prices, 10000, 42);
        PC_CHECK(summary.completed_runs == 10000);
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
            run_compiled(session, json, prices, 10000, 4242);
        PC_CHECK(summary.completed_runs == 10000);
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

    /* Flagged states compile to exact item-flag guards. A corrupted start
     * must route through restart and still verify against V(start). */
    {
        const std::unordered_map<std::string, double> prices{
            {"transmute", 1.0}, {"alteration", 1.0}, {"base", 10.0}};
        pc_item_state corrupted = start;
        corrupted.item_flags = PC_ITEM_CORRUPTED;
        const SolveResult solved = solve(calc, corrupted, prices);
        PC_CHECK(solved.converged);
        const std::string json =
            compile_policy_strategy_json(calc, solved, "flagged restart");
        PC_CHECK(json.find("\"type\":\"item_flag\",\"flag\":\"corrupted\"") !=
                 std::string::npos);
        const SimulationSummaryInternal summary =
            run_compiled(session, json, prices, 10000, 9001);
        PC_CHECK(summary.success_count == summary.completed_runs);
        const double mean = summary.known_total_cost /
                            static_cast<double>(summary.completed_runs);
        std::printf("solver compile flagged: V=%.4f empirical=%.4f\n",
                    solved.values[solved.start_state], mean);
        PC_CHECK(std::fabs(mean - solved.values[solved.start_state]) < 0.25);
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
            run_compiled(session, json, prices, 10000, 777);
        PC_CHECK(summary.completed_runs == 10000);
        PC_CHECK(summary.success_count == summary.completed_runs);
    }

    /* A tag-discriminating layout must retain separate junk identities in
     * ordinary strategy conditions, then route every sampled result exactly
     * instead of hitting the off-policy terminal. The descriptor decoration
     * isolates the former compiler refusal without adding a second expensive
     * reforge solve to the repository gate. */
    {
        ActionRegistry tagged_registry = registry;
        tagged_registry.actions[transmute].discriminating_tag_ids = {3};
        CalcContext tagged_calc(
            session, goal, std::move(tagged_registry),
            {transmute, restart});
        PC_CHECK(!tagged_calc.layout().discriminating_tag_ids.empty());
        const std::unordered_map<std::string, double> prices{
            {"transmute", 1.0}, {"base", 10.0}};
        const SolveResult solved = solve(tagged_calc, start, prices);
        PC_CHECK(solved.converged);
        const std::string json = compile_policy_strategy_json(
            tagged_calc, solved, "tag-discriminating transmute");
        PC_CHECK(json.find("\"type\":\"mod_family_count\"") !=
                 std::string::npos);
        const SimulationSummaryInternal summary =
            run_compiled(session, json, prices, 10000, 8675310);
        PC_CHECK(summary.success_count == summary.completed_runs);
        PC_CHECK(summary.no_matching_edge_count == 0);
        const double expected = solved.values[solved.start_state];
        const double mean = summary.known_total_cost /
                            static_cast<double>(summary.completed_runs);
        std::printf(
            "solver compile tagged: V=%.4f empirical=%.4f\n",
            expected, mean);
        PC_CHECK(std::fabs(mean - expected) < 2.0);
    }

    /* S5/S6 headline gate: six distinct all-T1 slots solve, compile with
     * group-tier and junk-count guards, and simulate at V(start). */
    {
        GoalSpec perfect;
        for (std::uint32_t group : {11u, 12u, 13u, 20u, 21u, 22u}) {
            GoalSlot wanted;
            wanted.group_id = group;
            wanted.min_tier = 1;
            perfect.slots.push_back(wanted);
        }
        perfect.rarity = PC_RARITY_RARE;
        const std::uint32_t chaos = registry.index_by_id.at("chaos");
        CalcContext perfect_calc(
            session, perfect, registry, {chaos, restart});
        const std::unordered_map<std::string, double> prices{
            {"chaos", 1.0}, {"base", 10.0}};
        pc_item_state rare;
        pc_item_clear(&rare);
        rare.rarity = PC_RARITY_RARE;
        const SolveResult solved = solve(perfect_calc, rare, prices);
        PC_CHECK(solved.converged);
        const std::string json = compile_policy_strategy_json(
            perfect_calc, solved, "six-slot all-T1 perfect item");
        PC_CHECK(json.find("\"type\":\"mod_family_count\"") !=
                 std::string::npos);
        PC_CHECK(json.find("\"type\":\"has_mod_group\"") !=
                 std::string::npos);
        PC_CHECK(json.find("\"min_tier\":1") != std::string::npos);
        const SimulationSummaryInternal summary =
            run_compiled(session, json, prices, 10000, 8675309);
        PC_CHECK(summary.success_count == summary.completed_runs);
        const double expected = solved.values[solved.start_state];
        const double mean = summary.known_total_cost /
                            static_cast<double>(summary.completed_runs);
        std::printf(
            "solver compile perfect item: V=%.4f empirical=%.4f (%u states)\n",
            expected, mean, perfect_calc.state_count());
        PC_CHECK(std::fabs(mean - expected) < 2.0);
    }

    /* Unveil is a sampled offer followed by a zero-cost policy choice. The
     * compiler emits preference-ordered option guards and concrete unveil
     * operations; simulation must reproduce the Bellman value. */
    {
        GoalSpec unveil_goal;
        GoalSlot life;
        life.family_id = 100;
        life.min_tier = 1;
        unveil_goal.slots.push_back(life);
        unveil_goal.rarity = PC_RARITY_RARE;
        const std::uint32_t veiled_exalt =
            registry.index_by_id.at("veiled_exalt");
        const std::uint32_t unveil = registry.index_by_id.at("unveil");
        CalcContext unveil_calc(
            session, unveil_goal, registry,
            {veiled_exalt, unveil, restart});
        const std::unordered_map<std::string, double> prices{
            {"veiled_exalt", 1.0}, {"base", 10.0}};
        pc_item_state rare;
        pc_item_clear(&rare);
        rare.rarity = PC_RARITY_RARE;
        const SolveResult solved = solve(unveil_calc, rare, prices);
        PC_CHECK(solved.converged);
        const std::string json = compile_policy_strategy_json(
            unveil_calc, solved, "policy-selected unveil");
        PC_CHECK(json.find("\"type\":\"has_unveil_option\"") !=
                 std::string::npos);
        PC_CHECK(json.find("\"type\":\"unveil\"") !=
                 std::string::npos);
        const SimulationSummaryInternal summary =
            run_compiled(session, json, prices, 10000, 1234567);
        PC_CHECK(summary.success_count == summary.completed_runs);
        PC_CHECK(summary.missing_price_run_count == 0);
        const double expected = solved.values[solved.start_state];
        const double mean = summary.known_total_cost /
                            static_cast<double>(summary.completed_runs);
        std::printf(
            "solver compile unveil: V=%.4f empirical=%.4f success=%llu failure=%llu noedge=%llu unapplied=%llu\n",
            expected, mean,
            static_cast<unsigned long long>(summary.success_count),
            static_cast<unsigned long long>(summary.failure_count),
            static_cast<unsigned long long>(summary.no_matching_edge_count),
            static_cast<unsigned long long>(summary.action_not_applied_count));
        PC_CHECK(std::fabs(mean - expected) < 0.25);
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

    /* S7.3 fixed options expose exact, finite primitive programs. These are
     * authored here but run only with the plan-level native suite. */
    {
        GoalSpec scour_goal = goal;
        FixedOptionSpec option;
        option.kind = FixedOptionKind::ScourAlchemy;
        scour_goal.fixed_options.push_back(option);
        CalcContext option_calc(
            session, scour_goal, registry, {}, false, false);
        pc_item_state rare;
        pc_item_clear(&rare);
        rare.rarity = PC_RARITY_RARE;
        const std::uint32_t state = option_calc.intern_item(rare);
        const std::uint32_t op =
            static_cast<std::uint32_t>(registry.actions.size());
        const OptionKernel& kernel = option_calc.option_kernel(state, op);
        PC_CHECK(kernel.supported);
        PC_CHECK(kernel.legal);
        PC_CHECK(kernel.terminates_almost_surely);
        PC_CHECK(kernel.expected_primitive_actions == 2.0);
        PC_CHECK(!kernel.exits.empty());
    }

    {
        GoalSpec eldritch_goal = goal;
        FixedOptionSpec option;
        option.kind = FixedOptionKind::EldritchSideIntent;
        option.side = PC_SIDE_PREFIX;
        option.action_id = "eldritch_exalt";
        option.setup_action_ids = {"eldritch_ember:1"};
        eldritch_goal.fixed_options.push_back(option);
        CalcContext option_calc(
            session, eldritch_goal, registry, {}, false, false);
        pc_item_state rare;
        pc_item_clear(&rare);
        rare.rarity = PC_RARITY_RARE;
        const OptionKernel& kernel = option_calc.option_kernel(
            option_calc.intern_item(rare),
            static_cast<std::uint32_t>(registry.actions.size()));
        PC_CHECK(kernel.supported);
        PC_CHECK(kernel.legal);
        PC_CHECK(kernel.expected_primitive_actions == 2.0);
        PC_CHECK(!kernel.exits.empty());
        for (const OutcomeEntry& exit : kernel.exits) {
            const AbstractState& state = option_calc.state(exit.state);
            PC_CHECK(state.searing_exarch_tier >
                     state.eater_of_worlds_tier);
        }
    }

    {
        GoalSpec protected_goal = goal;
        FixedOptionSpec option;
        option.kind = FixedOptionKind::ProtectedSide;
        option.side = PC_SIDE_PREFIX;
        option.action_id = "chaos";
        protected_goal.fixed_options.push_back(option);
        CalcContext option_calc(
            session, protected_goal, registry, {}, false, false);
        pc_item_state rare;
        pc_item_clear(&rare);
        rare.rarity = PC_RARITY_RARE;
        const OptionKernel& kernel = option_calc.option_kernel(
            option_calc.intern_item(rare),
            static_cast<std::uint32_t>(registry.actions.size()));
        PC_CHECK(kernel.supported);
        PC_CHECK(kernel.legal);
        PC_CHECK(kernel.expected_primitive_actions == 2.0);
        PC_CHECK(!kernel.exits.empty());
    }

    /* S7.4 renewal normalizes only certified same-kernel failures to the
     * entry state; the compiled graph expands that normalization back into
     * an ordinary primitive retry router. */
    {
        GoalSpec renewal_goal = goal;
        FixedOptionSpec option;
        option.kind = FixedOptionKind::Renewal;
        option.program_action_ids = {"chaos"};
        option.exit_goal_slots = {0};
        option.exit_min_satisfied = 1;
        renewal_goal.fixed_options.push_back(option);
        CalcContext option_calc(
            session, renewal_goal, registry, {}, false, false);
        pc_item_state rare;
        pc_item_clear(&rare);
        rare.rarity = PC_RARITY_RARE;
        const std::uint32_t state = option_calc.intern_item(rare);
        const std::uint32_t op =
            static_cast<std::uint32_t>(registry.actions.size());
        const OptionKernel& kernel = option_calc.option_kernel(state, op);
        PC_CHECK(kernel.supported);
        PC_CHECK(kernel.legal);
        PC_CHECK(kernel.terminates_almost_surely);
        PC_CHECK(kernel.expected_primitive_actions == 1.0);
        PC_CHECK(!kernel.retry_states.empty());
        PC_CHECK(kernel.observation_choice_groups.empty());

        const SolveResult solved = solve(
            option_calc, rare, {{"chaos", 1.0}});
        PC_CHECK(solved.converged);
        PC_CHECK(solved.policy[solved.start_state] == op);
        const std::string strategy = compile_policy_strategy_json(
            option_calc, solved, "renewal-chaos");
        PC_CHECK(strategy.find("\"type\":\"chaos\"") !=
                 std::string::npos);
        PC_CHECK(strategy.find("_retry") != std::string::npos);
    }

    /* Validate the real-artifact ProtectedRepeat vocabulary without expanding
     * the unbounded full-pool lock-plus-chaos kernel in a contract unit. The
     * performance corpus owns full reforge expansion and resource caps. */
    {
        GoalSpec protected_goal = goal;
        FixedOptionSpec option;
        option.kind = FixedOptionKind::ProtectedRepeat;
        option.side = PC_SIDE_PREFIX;
        option.action_id = "chaos";
        option.exit_goal_slots = {0};
        option.exit_min_satisfied = 1;
        protected_goal.fixed_options.push_back(option);
        CalcContext option_calc(
            session, protected_goal, registry, {}, false, false);
        const PlannerOperator& planner =
            option_calc.operators().at(registry.actions.size());
        PC_CHECK(planner.kind == PlannerOperatorKind::FixedOption);
        PC_CHECK(planner.option_kind == FixedOptionKind::ProtectedRepeat);
        PC_CHECK(planner.primitive_program.size() == 2);
        PC_CHECK(planner.id.find("option:protected_repeat:prefix:chaos") == 0);
    }

    /* Observation-aware renewal keeps the sampled Unveil set for Bellman
     * choice and for the compiled has_unveil_option routers. Use the bounded
     * exact synthetic pool; full canonical Veiled Chaos expansion is a
     * performance-corpus responsibility. */
    auto observed_session = make_compile_session();
    ActionRegistry observed_registry = build_action_registry(*observed_session);
    std::uint32_t unveiled_goal_mod = kNoId;
    if (!observed_session->unveiled_generic_mask.empty()) {
        pc_bitset_for_each(
            observed_session->unveiled_generic_mask.data(),
            observed_session->words,
            [&](std::size_t bit) {
                if (unveiled_goal_mod == kNoId) {
                    unveiled_goal_mod = static_cast<std::uint32_t>(bit);
                }
            });
    }
    if (unveiled_goal_mod != kNoId) {
        GoalSpec unveil_goal;
        GoalSlot unveil_slot;
        unveil_slot.family_id =
            observed_session->family_id[unveiled_goal_mod];
        unveil_goal.slots.push_back(unveil_slot);
        FixedOptionSpec option;
        option.kind = FixedOptionKind::Renewal;
        option.program_action_ids = {"veiled_chaos", "unveil"};
        option.exit_goal_slots = {0};
        option.exit_min_satisfied = 1;
        unveil_goal.fixed_options.push_back(option);
        CalcContext option_calc(
            observed_session, unveil_goal, observed_registry, {}, false,
            false);
        pc_item_state rare;
        pc_item_clear(&rare);
        rare.rarity = PC_RARITY_RARE;
        const std::uint32_t state = option_calc.intern_item(rare);
        const std::uint32_t op =
            static_cast<std::uint32_t>(observed_registry.actions.size());
        const OptionKernel& kernel = option_calc.option_kernel(state, op);
        PC_CHECK(kernel.supported);
        PC_CHECK(kernel.legal);
        PC_CHECK(!kernel.observation_choice_groups.empty());
        PC_CHECK(!kernel.observation_choice_options.empty());

        const SolveResult solved = solve(
            option_calc, rare, {{"veiled_chaos", 1.0}});
        PC_CHECK(solved.converged);
        PC_CHECK(!solved.option_unveil_preferences[state].empty());
        const std::string strategy = compile_policy_strategy_json(
            option_calc, solved, "observed-unveil-renewal");
        PC_CHECK(strategy.find("has_unveil_option") != std::string::npos);
        PC_CHECK(strategy.find("\"type\":\"veiled_chaos\"") !=
                 std::string::npos);
    }

    /* Pick one ordinary crafted prefix and suffix with no group conflict, then
     * force the deterministic Multimod option as the only candidate. */
    std::uint32_t finish_prefix = kNoId;
    std::uint32_t finish_suffix = kNoId;
    const auto conflicts = [&](std::uint32_t left, std::uint32_t right) {
        for (std::uint32_t a = session->group_offsets[left];
             a < session->group_offsets[left + 1]; ++a) {
            for (std::uint32_t b = session->group_offsets[right];
                 b < session->group_offsets[right + 1]; ++b) {
                if (session->group_ids[a] == session->group_ids[b]) {
                    return true;
                }
            }
        }
        return false;
    };

    /* Fracture preparation keys success to the exact satisfying carrier.
     * Wrong-carrier results remain exits, a fractured carrier still
     * satisfies its ordinary goal slot, and Eldritch implicits survive. Keep
     * this exact carrier test on the bounded synthetic modifier universe. */
    {
        auto fracture_session = make_compile_session();
        ActionRegistry fracture_registry =
            build_action_registry(*fracture_session);
        GoalSpec fracture_goal;
        GoalSlot carrier_slot;
        carrier_slot.family_id = 100;
        fracture_goal.slots.push_back(carrier_slot);
        FixedOptionSpec option;
        option.kind = FixedOptionKind::FracturePrepare;
        option.program_action_ids = {"chaos"};
        option.carrier_goal_slot = 0;
        fracture_goal.fixed_options.push_back(option);
        CalcContext option_calc(
            fracture_session, fracture_goal, fracture_registry, {}, false,
            false);
        pc_item_state ready;
        pc_item_clear(&ready);
        ready.rarity = PC_RARITY_RARE;
        ready.searing_exarch_tier = 1;
        for (const std::uint32_t mod : {0u, 3u, 5u, 6u}) {
            PC_CHECK(pc_item_add_mod(
                         &ready, fracture_session->gen_type[mod], mod,
                         static_cast<std::uint16_t>(
                             fracture_session->primary_group[mod]),
                         0, nullptr) == PC_RESULT_OK);
        }
        const std::uint32_t state = option_calc.intern_item(ready);
        const OptionKernel& kernel = option_calc.option_kernel(
            state,
            static_cast<std::uint32_t>(fracture_registry.actions.size()));
        PC_CHECK(kernel.supported);
        PC_CHECK(kernel.legal);
        PC_CHECK(kernel.entry_continues);
        PC_CHECK(kernel.expected_primitive_actions == 1.0);
        double carrier_probability = 0.0;
        for (const OutcomeEntry& exit : kernel.exits) {
            const AbstractState& fractured = option_calc.state(exit.state);
            PC_CHECK(fractured.slot_status[0] ==
                     static_cast<std::uint8_t>(
                         GoalSlotStatus::Satisfied));
            PC_CHECK(fractured.searing_exarch_tier == 1);
            if ((fractured.fractured_goal_mask & 1u) != 0) {
                carrier_probability += exit.probability;
            }
        }
        PC_CHECK(std::fabs(carrier_probability - 0.25) < 1e-12);

        pc_item_state influenced = ready;
        influenced.generic_influence_bits = 1;
        const OptionKernel& influenced_kernel = option_calc.option_kernel(
            option_calc.intern_item(influenced),
            static_cast<std::uint32_t>(fracture_registry.actions.size()));
        PC_CHECK(!influenced_kernel.legal);
        for (const ActionDescriptor& action : fracture_registry.actions) {
            if (action.params.type == ActionType::InfluenceExalt) {
                pc_item_state fractured_item = ready;
                fractured_item.prefixes[0].flags |= PC_MOD_SLOT_FRACTURED;
                PC_CHECK(!action_legal(
                    *fracture_session, action,
                    option_calc.state(
                        option_calc.intern_item(fractured_item))));
                break;
            }
        }
    }

    for (std::uint32_t index = 0; index < registry.actions.size(); ++index) {
        const ActionDescriptor& action = registry.actions[index];
        if (action.params.type != ActionType::Bench ||
            action.params.mod_id >= session->metamod_type.size() ||
            session->metamod_type[action.params.mod_id] >= 0) {
            continue;
        }
        if (session->gen_type[action.params.mod_id] == PC_SIDE_PREFIX &&
            finish_prefix == kNoId) {
            finish_prefix = index;
        }
        if (session->gen_type[action.params.mod_id] == PC_SIDE_SUFFIX &&
            finish_prefix != kNoId &&
            !conflicts(registry.actions[finish_prefix].params.mod_id,
                       action.params.mod_id)) {
            finish_suffix = index;
            break;
        }
    }
    PC_CHECK(finish_prefix != kNoId);
    PC_CHECK(finish_suffix != kNoId);
    if (finish_prefix != kNoId && finish_suffix != kNoId) {
        GoalSpec finish_goal;
        for (const std::uint32_t action_index :
             {finish_prefix, finish_suffix}) {
            GoalSlot finish_slot;
            finish_slot.family_id = session->family_id[
                registry.actions[action_index].params.mod_id];
            finish_goal.slots.push_back(finish_slot);
        }
        finish_goal.rarity = PC_RARITY_RARE;
        FixedOptionSpec option;
        option.kind = FixedOptionKind::MultimodFinish;
        option.bench_craft_ids = {
            registry.actions[finish_prefix].id,
            registry.actions[finish_suffix].id};
        finish_goal.fixed_options.push_back(option);
        CalcContext option_calc(
            session, finish_goal, registry, {}, false, false);

        const PlannerOperator& planner =
            option_calc.operators().at(registry.actions.size());
        PC_CHECK(planner.kind == PlannerOperatorKind::FixedOption);
        PC_CHECK(planner.primitive_program.size() == 3);
        std::unordered_map<std::string, double> finish_prices;
        double expected_cost = 0.0;
        for (const auto& [key, quantity] : planner.resource_quantities) {
            const double price =
                static_cast<double>(finish_prices.size() + 1);
            finish_prices[key] = price;
            expected_cost += quantity * price;
        }

        pc_item_state start;
        pc_item_clear(&start);
        start.rarity = PC_RARITY_RARE;
        const SolveResult solved = solve(option_calc, start, finish_prices);
        PC_CHECK(solved.converged);
        PC_CHECK(solved.policy[solved.start_state] ==
                 static_cast<std::uint32_t>(registry.actions.size()));
        PC_CHECK(std::fabs(solved.values[solved.start_state] - expected_cost) <
                 1e-9);
        const std::string strategy = compile_policy_strategy_json(
            option_calc, solved, "fixed-multimod-finish");
        PC_CHECK(strategy.find("option:multimod") == std::string::npos);
        PC_CHECK(strategy.find("_o1") != std::string::npos);
        PC_CHECK(strategy.find("_o2") != std::string::npos);
    }

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
        run_compiled(session, json, prices, 10000, 987654321);
    PC_CHECK(summary.completed_runs == 10000);
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
