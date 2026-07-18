#include "tests.hpp"

#include "../src/json.hpp"
#include "../src/solver_internal.hpp"
#include "poecraft/bitset.h"
#include "poecraft/item_state.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

using namespace poecraft;
using namespace poecraft::solver;

namespace {

/* Same eight-mod weighted universe as test_solver_calc.cpp. */
std::shared_ptr<SessionImpl> make_solve_session() {
    auto data = std::make_shared<DataImpl>();
    data->mod_global_ids = {0, 1, 2, 3, 4, 5, 6, 7};

    auto session = std::make_shared<SessionImpl>();
    session->data = data;
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

bool near(double a, double b, double tolerance = 1e-6) {
    return std::fabs(a - b) < tolerance;
}

bool valid_json_object(const std::string& text) {
    try {
        return json::Parser(text.data(), text.size()).parse().type ==
               json::Type::Object;
    } catch (const std::exception&) {
        return false;
    }
}

SolveResult solve_stepped(
    CalcContext& calc,
    const pc_item_state& start,
    const std::unordered_map<std::string, double>& prices,
    const std::uint32_t budget) {
    SolveWork work(calc, start, prices);
    std::uint32_t progress_events = 0;
    while (!work.progress().done) {
        work.step(budget);
        ++progress_events;
    }
    PC_CHECK(progress_events >= 2);
    return work.finish();
}

bool identical_solve(
    const SolveResult& left,
    const SolveResult& right) {
    return left.converged == right.converged &&
           left.start_state == right.start_state &&
           left.values == right.values && left.policy == right.policy &&
           left.expanded == right.expanded &&
           left.goal_states == right.goal_states &&
           left.policy_reachable == right.policy_reachable &&
           left.unveil_preferences == right.unveil_preferences &&
           left.option_unveil_preferences ==
               right.option_unveil_preferences &&
           left.diagnostics.skipped_missing_price ==
               right.diagnostics.skipped_missing_price &&
           left.diagnostics.skipped_unsupported ==
               right.diagnostics.skipped_unsupported &&
           left.diagnostics.expanded_states ==
               right.diagnostics.expanded_states &&
           left.diagnostics.sweeps == right.diagnostics.sweeps &&
           left.diagnostics.residual == right.diagnostics.residual &&
           left.diagnostics.state_cap_hit ==
               right.diagnostics.state_cap_hit;
}

void place(pc_item_state* item, int side, std::uint32_t mod_id,
           std::uint16_t group) {
    pc_item_add_mod(item, side, mod_id, group, 0, nullptr);
}

/*
 * Alt-spam analytic gate. Goal: family 100 at tier 1 on a MAGIC item.
 * Transmute and alteration share one outcome distribution here, with goal
 * hit probability p (hand-derived in the S3 suite):
 *   p = 1/2 * (100/1100) + 1/2 * (100/1100 + (600/1100) * (100/500))
 * so with unit action costs V(any magic non-goal) = 1/p and
 * V(empty normal) = 1 + (1-p)/p = 1/p as well.
 */
void run_alt_spam_tests() {
    auto session = make_solve_session();
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
    {
        const std::unordered_map<std::string, double> prices{
            {"transmute", 1.0}, {"alteration", 1.0}, {"base", 10.0}};
        const SolveResult result = solve(calc, start, prices);
        PC_CHECK(result.converged);
        PC_CHECK(result.diagnostics.skipped_missing_price.empty());
        PC_CHECK(result.diagnostics.skipped_unsupported.empty());
        PC_CHECK(near(result.values[result.start_state], 1.0 / p));
        PC_CHECK(result.policy[result.start_state] == transmute);
        PC_CHECK(result.diagnostics.preservation_rows_considered > 0);
        PC_CHECK(result.diagnostics.certified_disposable_rows > 0);
        PC_CHECK(result.diagnostics.preservation_rows_retained ==
                 result.diagnostics.preservation_rows_considered);
        PC_CHECK(result.diagnostics.preservation_rows_pruned == 0);
        bool retained_uncertain = false;
        for (const std::string& witness :
             result.diagnostics.preservation_witnesses) {
            PC_CHECK(valid_json_object(witness));
            retained_uncertain |=
                witness.find(
                    "no_exact_dominance_or_equivalence_proof") !=
                std::string::npos;
        }
        PC_CHECK(retained_uncertain);

        /* Any magic non-goal state alt-spams at the same expected cost. */
        pc_item_state below;
        pc_item_clear(&below);
        below.rarity = PC_RARITY_MAGIC;
        place(&below, PC_SIDE_PREFIX, 1, 10);
        const std::uint32_t below_state = calc.intern_item(below);
        PC_CHECK(near(result.values[below_state], 1.0 / p));
        PC_CHECK(result.policy[below_state] == alteration);

        /* Goal terminal: value zero, no action. */
        pc_item_state done;
        pc_item_clear(&done);
        done.rarity = PC_RARITY_MAGIC;
        place(&done, PC_SIDE_PREFIX, 0, 10);
        const std::uint32_t goal_state = calc.intern_item(done);
        PC_CHECK(result.goal_states[goal_state] == 1);
        PC_CHECK(result.values[goal_state] == 0.0);
        PC_CHECK(result.policy[goal_state] == kNoId);

        /* Solve log: one record per expanded state. */
        const std::string log = serialize_solve_log(calc, result);
        std::size_t lines = 0;
        for (char c : log) {
            if (c == '\n') ++lines;
        }
        PC_CHECK(lines == result.diagnostics.expanded_states);
        PC_CHECK(log.find("\"action\":\"transmute\"") != std::string::npos);

        const std::unordered_map<std::string, double> subset_prices{
            {"transmute", 1.0}, {"alteration", 1.0}};
        const SolveResult subset = solve(calc, start, subset_prices);
        PC_CHECK(subset.converged);
        PC_CHECK(subset.diagnostics.skipped_missing_price.size() == 1);
        PC_CHECK(subset.diagnostics.priced_scanned_actions == 2);
        PC_CHECK(subset.diagnostics.supported_priced_actions == 2);
        const std::string subset_telemetry = serialize_solver_telemetry(
            calc, &subset, nullptr, std::nullopt, nullptr);
        PC_CHECK(valid_json_object(subset_telemetry));
        PC_CHECK(subset_telemetry.find(
                     "\"status\":\"exact_supported_priced_subset\"") !=
                 std::string::npos);
        PC_CHECK(subset_telemetry.find(
                     "\"full_request_status\":\"incomplete_action_subset\"") !=
                 std::string::npos);

        SolveOptions capped_options;
        capped_options.max_states = 1;
        const SolveResult capped = solve(calc, start, prices, capped_options);
        PC_CHECK(!capped.converged);
        PC_CHECK(capped.diagnostics.state_cap_hit);
        const std::string capped_telemetry = serialize_solver_telemetry(
            calc, &capped, nullptr, std::nullopt, nullptr);
        PC_CHECK(valid_json_object(capped_telemetry));
        PC_CHECK(capped_telemetry.find(
                     "\"status\":\"incomplete_state_cap\"") !=
                 std::string::npos);
        PC_CHECK(capped_telemetry.find(
                     "\"value\":{\"start\":null") !=
                 std::string::npos);
        PC_CHECK(capped_telemetry.find("\"raw_start_bound\":") !=
                 std::string::npos);

        for (const std::uint32_t budget : {1u, 2u, 7u, 64u, 4096u}) {
            const SolveResult stepped =
                solve_stepped(calc, start, prices, budget);
            PC_CHECK(identical_solve(result, stepped));
            PC_CHECK(stepped.diagnostics.preservation_witnesses ==
                     result.diagnostics.preservation_witnesses);
        }

        SolveOptions diagnostic_caps;
        diagnostic_caps.max_diagnostic_samples = 1;
        diagnostic_caps.max_telemetry_json_bytes = 64;
        const SolveResult diagnostic_capped =
            solve(calc, start, prices, diagnostic_caps);
        PC_CHECK(diagnostic_capped.diagnostics.action_inclusion_reasons.size() <=
                 1);
        PC_CHECK(diagnostic_capped.diagnostics.preservation_witnesses.size() <=
                 1);
        PC_CHECK(
            diagnostic_capped.diagnostics.preservation_witnesses_omitted > 0);
        PC_CHECK(
            diagnostic_capped.diagnostics.diagnostics_retained_bytes_estimate >
            0);
        bool telemetry_capped = false;
        try {
            (void)serialize_solver_telemetry(
                calc, &diagnostic_capped, nullptr, std::nullopt, nullptr);
        } catch (const std::length_error& ex) {
            telemetry_capped = std::string(ex.what()).find(
                                   "max_telemetry_json_bytes") !=
                               std::string::npos;
        }
        PC_CHECK(telemetry_capped);
    }

    /* Evaluator support is diagnostic, not an applied filter: a priced
     * unsupported descriptor remains in the scanned vector and is observed
     * state-by-state, while the converged value is qualified to the usable
     * supported/priced subset. */
    {
        ActionRegistry unsupported_registry = registry;
        ActionDescriptor unsupported;
        unsupported.id = "unsupported-test-action";
        unsupported.display_name = "Unsupported Test Action";
        unsupported.params.type = static_cast<ActionType>(999);
        unsupported.kind = TransitionKind::Special;
        unsupported.cost_keys = {"fracture"};
        const std::uint32_t unsupported_index =
            static_cast<std::uint32_t>(unsupported_registry.actions.size());
        unsupported_registry.index_by_id.emplace(
            unsupported.id, unsupported_index);
        unsupported_registry.actions.push_back(std::move(unsupported));
        CalcContext unsupported_calc(
            session, goal, std::move(unsupported_registry),
            {transmute, alteration, restart, unsupported_index});
        const std::unordered_map<std::string, double> prices{
            {"transmute", 1.0},
            {"alteration", 1.0},
            {"base", 10.0},
            {"fracture", 1.0}};
        const SolveResult result = solve(unsupported_calc, start, prices);
        PC_CHECK(result.converged);
        PC_CHECK(result.diagnostics.evaluator_supported_actions == 3);
        PC_CHECK(result.diagnostics.priced_scanned_actions == 4);
        PC_CHECK(result.diagnostics.supported_priced_actions == 3);
        PC_CHECK(result.diagnostics.skipped_unsupported.size() == 1);
        const std::string telemetry = serialize_solver_telemetry(
            unsupported_calc, &result, nullptr, std::nullopt, nullptr);
        PC_CHECK(telemetry.find("\"evaluator_supported\":3") !=
                 std::string::npos);
        PC_CHECK(telemetry.find("\"priced_scanned\":4") !=
                 std::string::npos);
        PC_CHECK(telemetry.find("\"supported_priced\":3") !=
                 std::string::npos);
        PC_CHECK(telemetry.find("\"unsupported_observed\":1") !=
                 std::string::npos);
        PC_CHECK(telemetry.find(
                     "\"status\":\"exact_supported_priced_subset\"") !=
                 std::string::npos);
    }

    /* Forced-bad state: a corrupted start can only restart, so its value
     * is the fresh base plus the clean-base value. */
    {
        const std::unordered_map<std::string, double> prices{
            {"transmute", 1.0}, {"alteration", 1.0}, {"base", 10.0}};
        pc_item_state corrupted = start;
        corrupted.item_flags = PC_ITEM_CORRUPTED;
        const SolveResult result = solve(calc, corrupted, prices);
        PC_CHECK(result.converged);
        PC_CHECK(near(result.values[result.start_state], 10.0 + 1.0 / p));
        PC_CHECK(result.policy[result.start_state] == restart);
    }

    /* Price flip: expensive alterations make restart the optimal recovery
     * from any junk magic state. Analytically V(normal) = (2-p)/p and
     * V(magic non-goal) = 1 + V(normal) = 2/p. */
    {
        const std::unordered_map<std::string, double> prices{
            {"transmute", 1.0}, {"alteration", 100.0}, {"base", 1.0}};
        const SolveResult result = solve(calc, start, prices);
        PC_CHECK(result.converged);
        PC_CHECK(near(result.values[result.start_state], (2.0 - p) / p));
        PC_CHECK(result.policy[result.start_state] == transmute);

        pc_item_state below;
        pc_item_clear(&below);
        below.rarity = PC_RARITY_MAGIC;
        place(&below, PC_SIDE_PREFIX, 1, 10);
        const std::uint32_t below_state = calc.intern_item(below);
        PC_CHECK(near(result.values[below_state], 2.0 / p));
        PC_CHECK(result.policy[below_state] == restart);
        PC_CHECK(result.diagnostics.preservation_rows_pruned > 0);
        PC_CHECK(result.diagnostics.preservation_rows_retained > 0);
        bool saw_bound = false;
        bool saw_disposable = false;
        for (const std::string& witness :
             result.diagnostics.preservation_witnesses) {
            PC_CHECK(valid_json_object(witness));
            saw_bound |= witness.find(
                             "candidate_lower_bound > "
                             "restart_route_upper_bound") !=
                         std::string::npos;
            saw_disposable |= witness.find(
                                  "genuine_restart_state_identity") !=
                              std::string::npos;
        }
        PC_CHECK(saw_bound);
        PC_CHECK(saw_disposable);
        const std::string telemetry = serialize_solver_telemetry(
            calc, &result, nullptr, std::nullopt, nullptr);
        PC_CHECK(valid_json_object(telemetry));
        PC_CHECK(telemetry.find("\"preservation_witnesses\":[{") !=
                 std::string::npos);

        /* Exhaustive-oracle comparison: disabling only S8.2 control leaves
         * the same cheapest value and selected policy cost. */
        SolveOptions oracle_options;
        oracle_options.preservation_control = false;
        const SolveResult oracle =
            solve(calc, start, prices, oracle_options);
        PC_CHECK(oracle.converged);
        PC_CHECK(near(
            oracle.values[oracle.start_state],
            result.values[result.start_state], 1e-9));
        PC_CHECK(near(
            oracle.values[below_state], result.values[below_state], 1e-9));
        PC_CHECK(oracle.policy[oracle.start_state] ==
                 result.policy[result.start_state]);
        PC_CHECK(oracle.policy[below_state] == result.policy[below_state]);
        PC_CHECK(oracle.diagnostics.preservation_rows_considered == 0);
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

/* Toy solve on the real artifact: a one-mod rare goal with the basic
 * currency set converges, respects the restart bound, and is
 * deterministic across repeated solves. */
void run_artifact_solve_tests(const char* artifact_dir) {
    if (artifact_dir == nullptr) {
        std::printf("solver solve artifact suite skipped (missing path)\n");
        return;
    }
    const std::string dir = artifact_dir;
    std::string manifest_text;
    std::string strings_text;
    std::string game_text;
    if (!read_text_file(dir + "/manifest.json", manifest_text) ||
        !read_text_file(dir + "/strings.json", strings_text) ||
        !read_text_file(dir + "/game-data.json", game_text)) {
        std::printf("solver solve artifact suite skipped (unreadable)\n");
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
        std::printf("solver solve artifact suite: %s\n", ex.what());
        PC_CHECK(false);
        return;
    }

    ActionRegistry registry = build_action_registry(*session);
    for (const ActionDescriptor& action : registry.actions) {
        if (action.params.type == ActionType::Fossil ||
            action.params.type == ActionType::Essence) {
            PC_CHECK(action.preservation.destructive_renewal);
            PC_CHECK(action.preservation.preserves_fractured_affixes);
            PC_CHECK(!action.preservation.respects_prefix_lock);
            PC_CHECK(!action.preservation.respects_suffix_lock);
            PC_CHECK(!action.preservation.respects_cannot_roll_attack);
            PC_CHECK(!action.preservation.respects_cannot_roll_caster);
        }
    }
    GoalSpec goal;
    GoalSlot slot;
    for (std::uint32_t mod = 0; mod < session->mod_count; ++mod) {
        if (session->gen_type[mod] == 0 &&
            pc_bitset_test(session->normal_random_roll_mask.data(), mod) &&
            pc_bitset_test(session->positive_base_weight_mask.data(), mod)) {
            slot.group_id = session->primary_group[mod];
            break;
        }
    }
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
    const SolveResult first = solve(calc, start, prices);
    const CalcTelemetry first_telemetry = calc.telemetry();
    PC_CHECK(first.converged);
    PC_CHECK(first.diagnostics.skipped_missing_price.empty());
    PC_CHECK(first.diagnostics.skipped_unsupported.empty());
    const double start_value = first.values[first.start_state];
    PC_CHECK(start_value > 0.0);

    /* Restart bounds the start value: buying a base and crafting from
     * clean can never be cheaper than the optimum from here. */
    pc_item_state clean;
    pc_item_clear(&clean);
    const std::uint32_t clean_state = calc.intern_item(clean);
    PC_CHECK(first.values[clean_state] < 5.0 + start_value + 1e-9);
    PC_CHECK(start_value <= 5.0 + first.values[clean_state] + 1e-9);
    PC_CHECK(first.policy[first.start_state] != kNoId);

    /* Identical inputs must yield identical values and policy. */
    const SolveResult second = solve(calc, start, prices);
    const CalcTelemetry second_telemetry = calc.telemetry();
    PC_CHECK(second.values.size() == first.values.size());
    bool identical = second.values.size() == first.values.size();
    for (std::size_t i = 0; identical && i < first.values.size(); ++i) {
        identical = first.values[i] == second.values[i] &&
                    first.policy[i] == second.policy[i];
    }
    PC_CHECK(identical);
    PC_CHECK(second.diagnostics.discovered_states ==
             first.diagnostics.discovered_states);
    PC_CHECK(second.diagnostics.frontier_states ==
             first.diagnostics.frontier_states);
    PC_CHECK(second.diagnostics.goal_states == first.diagnostics.goal_states);
    PC_CHECK(second.diagnostics.policy_reachable_states ==
             first.diagnostics.policy_reachable_states);
    PC_CHECK(!first.diagnostics.transition_cache_reused);
    PC_CHECK(second.diagnostics.transition_cache_reused);
    PC_CHECK(first_telemetry.state_action_rows > 0);
    PC_CHECK(second_telemetry.state_action_rows == 0);
    PC_CHECK(second_telemetry.transition_entries == 0);

    auto repriced = prices;
    for (auto& [unused_key, price] : repriced) {
        (void)unused_key;
        price *= 2.0;
    }
    const SolveResult price_only = solve(calc, start, repriced);
    const CalcTelemetry price_only_telemetry = calc.telemetry();
    PC_CHECK(price_only.converged);
    PC_CHECK(price_only.diagnostics.transition_cache_reused);
    PC_CHECK(price_only_telemetry.state_action_rows == 0);
    PC_CHECK(price_only_telemetry.transition_entries == 0);
    PC_CHECK(std::abs(
                 price_only.values[price_only.start_state] -
                 start_value * 2.0) < 1e-6);
    for (const std::uint32_t budget : {1u, 7u, 128u}) {
        const SolveResult stepped = solve_stepped(calc, start, prices, budget);
        PC_CHECK(identical_solve(first, stepped));
    }

    /* Real-artifact S8.2 reduction gate. Reprice only Chaos far above the
     * genuine Restart route. Progressed rare carriers must drop Chaos with a
     * strict bound witness, while the controlled and exhaustive envelopes
     * retain the same exact value and selected policy cost. */
    auto expensive_chaos = prices;
    expensive_chaos["chaos"] = 1000000.0;
    const SolveResult controlled = solve(calc, start, expensive_chaos);
    SolveOptions exhaustive_options;
    exhaustive_options.preservation_control = false;
    const SolveResult exhaustive =
        solve(calc, start, expensive_chaos, exhaustive_options);
    PC_CHECK(controlled.converged);
    PC_CHECK(exhaustive.converged);
    PC_CHECK(near(
        controlled.values[controlled.start_state],
        exhaustive.values[exhaustive.start_state], 1e-9));
    PC_CHECK(controlled.policy[controlled.start_state] ==
             exhaustive.policy[exhaustive.start_state]);
    PC_CHECK(controlled.diagnostics.preservation_rows_pruned > 0);
    PC_CHECK(controlled.diagnostics.preservation_rows_retained > 0);
    bool saw_real_chaos_bound = false;
    for (const std::string& witness :
         controlled.diagnostics.preservation_witnesses) {
        saw_real_chaos_bound |=
            witness.find("\"action\":\"chaos\"") != std::string::npos &&
            witness.find(
                "candidate_lower_bound > restart_route_upper_bound") !=
                std::string::npos;
    }
    PC_CHECK(saw_real_chaos_bound);
    std::printf(
        "solver solve S8.2 real control: %u considered, %u retained, "
        "%u pruned, V(start)=%.6f\n",
        controlled.diagnostics.preservation_rows_considered,
        controlled.diagnostics.preservation_rows_retained,
        controlled.diagnostics.preservation_rows_pruned,
        controlled.values[controlled.start_state]);

    std::printf(
        "solver solve artifact: %u states, %u sweeps, V(start)=%.3f\n",
        first.diagnostics.expanded_states, first.diagnostics.sweeps,
        start_value);
}

} // namespace

void run_solver_solve_tests(const char* artifact_dir) {
    run_alt_spam_tests();
    run_artifact_solve_tests(artifact_dir);
}
