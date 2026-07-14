#include "tests.hpp"

#include "../src/handles_internal.hpp"
#include "../src/solver_internal.hpp"
#include "poecraft/bitset.h"
#include "poecraft/solver.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace poecraft;
using namespace poecraft::solver;

namespace {

bool near(double a, double b, double tolerance = 1e-9) {
    return std::fabs(a - b) <= tolerance;
}

std::shared_ptr<SessionImpl> make_eval_session() {
    auto data = std::make_shared<DataImpl>();
    data->mod_global_ids = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    data->strings = {
        "", "synthetic/base", "mod0", "mod1", "mod2", "mod3",
        "mod4", "mod5", "mod6", "mod7", "mod8", "mod9",
        "g10", "g11", "g12", "g13", "g20", "g21", "g22",
        "g23", "g24"};
    data->base_count = 1;
    data->base_metadata_path_sid = {1};
    data->mod_key_sid = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    for (std::uint32_t mod = 0; mod < 10; ++mod) {
        data->mod_pos_by_key.emplace(
            data->strings[data->mod_key_sid[mod]], mod);
    }
    data->group_key_sids.assign(25, 0);
    const std::pair<std::uint32_t, std::uint32_t> groups[] = {
        {10, 12}, {11, 13}, {12, 14}, {13, 15}, {20, 16},
        {21, 17}, {22, 18}, {23, 19}, {24, 20}};
    for (const auto& [group, sid] : groups) {
        data->group_key_sids[group] = sid;
        data->group_id_by_key.emplace(data->strings[sid], group);
    }

    auto session = std::make_shared<SessionImpl>();
    session->data = data;
    session->base_index = 0;
    session->item_level = 1;
    session->mod_count = 10;
    session->words = pc_bitset_words(10);
    session->global_index = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    session->gen_type = {0, 0, 0, 0, 0, 1, 1, 1, 1, 1};
    session->primary_group = {10, 10, 11, 12, 13, 20, 21, 22, 23, 24};
    session->required_level.assign(10, 1);
    session->group_offsets = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    session->group_ids = {10, 10, 11, 12, 13, 20, 21, 22, 23, 24};
    session->family_id = {100, 100, 101, 102, 103,
                          104, 105, 106, 107, 108};
    session->family_tier_index = {1, 2, 1, 1, 1, 1, 1, 1, 1, 1};
    session->metamod_type.assign(10, -1);
    session->special_kind.assign(10, -1);
    session->flags.assign(10, 0);
    session->influence_code.assign(10, -1);
    session->class_offsets.assign(11, 0);
    session->rare_affix_cap = 3;
    session->base_spawn_weight = {100, 100, 100, 100, 100,
                                  100, 100, 100, 100, 400};
    session->base_gen_pct.assign(10, 100);
    session->base_roll_weight = session->base_spawn_weight;
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
    session->implicit_tag_masks.clear();
    session->group_masks.assign(25, {});
    session->influence_masks.assign(1, std::vector<std::uint64_t>(words, 0));
    for (std::uint32_t mod = 0; mod < 10; ++mod) {
        pc_bitset_set(session->normal_random_roll_mask.data(), mod);
        pc_bitset_set(session->positive_spawn_weight_mask.data(), mod);
        pc_bitset_set(session->positive_base_weight_mask.data(), mod);
        pc_bitset_set(session->influence_masks[0].data(), mod);
        pc_bitset_set((mod < 5 ? session->prefix_mask : session->suffix_mask)
                          .data(), mod);
        auto& group = session->group_masks[session->primary_group[mod]];
        if (group.empty()) group.assign(words, 0);
        pc_bitset_set(group.data(), mod);
    }
    return session;
}

std::shared_ptr<StrategyImpl> compile(
    const std::shared_ptr<const SessionImpl>& session,
    const std::string& json) {
    return compile_strategy_json(session, json.c_str(), json.size());
}

std::string shell(
    const std::string& name,
    const std::string& rarity,
    const std::string& nodes,
    const std::string& edges,
    const std::string& explicit_fields = std::string()) {
    return std::string("{\"version\":\"v1\",\"name\":\"") + name +
           "\",\"start_node_id\":\"start\",\"base_state\":{"
           "\"base_key\":\"synthetic/base\",\"item_level\":1,"
           "\"rarity\":\"" + rarity + "\"" + explicit_fields +
           "},\"nodes\":[" + nodes + "],\"edges\":[" + edges + "]}";
}

SimulationSummaryInternal simulate(
    const std::shared_ptr<const SessionImpl>& session,
    const std::shared_ptr<StrategyImpl>& strategy,
    std::uint64_t runs,
    std::uint64_t seed,
    SimulatorImpl* out_simulator = nullptr) {
    SimulatorImpl local;
    SimulatorImpl& simulator = out_simulator == nullptr ? local : *out_simulator;
    simulator.session = session;
    simulator.strategy = strategy;
    simulator.action_counts.assign(strategy->nodes.size(), 0);
    SimulationOptionsInternal options;
    options.target_runs = runs;
    options.seed = seed;
    options.max_actions_per_run = 10000;
    options.max_graph_steps_per_run = 100000;
    run_simulator_chunk(
        simulator, options, static_cast<std::uint32_t>(runs));
    return simulator.summary;
}

double edge_value(const StrategyEvalResult& result, const std::string& id) {
    for (const StrategyEvalEdge& edge : result.edges) {
        if (edge.id == id) return edge.expected_traversals;
    }
    return -1.0;
}

void run_closed_form_tests() {
    auto session = make_eval_session();

    const std::string deterministic = shell(
        "deterministic", "rare",
        R"JSON({"id":"start","kind":"start"},
{"id":"restart","kind":"operation","operation":{"type":"restart","params":{}}},
{"id":"success","kind":"terminal","terminal":"success"})JSON",
        R"JSON({"id":"begin","from":"start","to":"restart","priority":0,"condition":{"type":"always"}},
{"id":"done","from":"restart","to":"success","priority":0,"condition":{"type":"always"}})JSON");
    const auto deterministic_strategy = compile(session, deterministic);
    const StrategyEvalResult straight = evaluate_strategy(*deterministic_strategy);
    PC_CHECK(straight.converged);
    PC_CHECK(straight.targets.empty());
    PC_CHECK(near(straight.success_probability, 1.0));
    PC_CHECK(near(straight.expected_actions, 1.0));
    PC_CHECK(near(straight.expected_consumption.at("base"), 1.0));
    PC_CHECK(near(edge_value(straight, "begin"), 1.0));
    PC_CHECK(near(edge_value(straight, "done"), 1.0));
    PC_CHECK(straight.max_mass_conservation_error < 1e-12);

    const std::string loop = shell(
        "chaos loop", "rare",
        R"JSON({"id":"start","kind":"start"},
{"id":"chaos","kind":"operation","operation":{"type":"chaos","params":{}}},
{"id":"success","kind":"terminal","terminal":"success"})JSON",
        R"JSON({"id":"begin","from":"start","to":"chaos","priority":0,"condition":{"type":"always"}},
{"id":"hit","from":"chaos","to":"success","priority":0,"condition":{"type":"has_mod_family","family_mod_key":"mod0","min_tier":1}},
{"id":"repeat","from":"chaos","to":"chaos","priority":999,"is_default":true})JSON");
    const auto loop_strategy = compile(session, loop);

    ActionRegistry registry = build_action_registry(*session);
    GoalSpec goal;
    GoalSlot target;
    target.family_id = 100;
    target.min_tier = 1;
    goal.slots.push_back(target);
    const std::uint32_t chaos = registry.index_by_id.at("chaos");
    CalcContext calc(session, goal, registry, {chaos});
    pc_item_state start;
    pc_item_clear(&start);
    start.rarity = PC_RARITY_RARE;
    const OutcomeDistribution& one_roll =
        calc.outcomes(calc.intern_item(start), chaos);
    double p = 0.0;
    for (const OutcomeEntry& entry : one_roll.entries) {
        if (calc.state(entry.state).slot_status[0] ==
            static_cast<std::uint8_t>(GoalSlotStatus::Satisfied)) {
            p += entry.probability;
        }
    }
    PC_CHECK(p > 0.0 && p < 1.0);

    StrategyEvalOptions options;
    options.epsilon = 1e-13;
    const StrategyEvalResult exact = evaluate_strategy(*loop_strategy, options);
    PC_CHECK(exact.converged);
    PC_CHECK(std::fabs(exact.success_probability - 1.0) < 1e-9);
    PC_CHECK(std::fabs(exact.expected_actions - 1.0 / p) < 1e-9);
    PC_CHECK(std::fabs(edge_value(exact, "hit") - 1.0) < 1e-9);
    PC_CHECK(std::fabs(edge_value(exact, "repeat") - (1.0 - p) / p) <
             1e-9);
    PC_CHECK(std::fabs(
                 exact.expected_consumption.at("chaos") - 1.0 / p) <
             1e-9);
    PC_CHECK(exact.max_mass_conservation_error < 1e-10);

    const std::string bytes = serialize_strategy_eval(exact);
    PC_CHECK(bytes == serialize_strategy_eval(evaluate_strategy(
                          *loop_strategy, options)));
}

void run_condition_parity_tests() {
    auto session = make_eval_session();
    ActionRegistry registry = build_action_registry(*session);
    GoalSpec goal;
    GoalSlot family;
    family.family_id = 100;
    family.min_tier = 1;
    GoalSlot group;
    group.group_id = 20;
    goal.slots = {family, group};
    std::vector<std::uint32_t> actions;
    for (const char* id : {"transmute", "alteration", "regal", "chaos",
                           "exalt"}) {
        actions.push_back(registry.index_by_id.at(id));
    }
    CalcContext calc(session, goal, registry, actions);
    CalcContext strict_calc(
        session, goal, registry, actions,
        false, /* non-empty goal */
        true,  /* normal candidate default */
        true   /* exact group-exclusion effects */);

    /* The compact solver abstraction collapses all non-target suffixes.
     * Exact graph evaluation must keep their distinct group effects: the
     * 400-weight g24 family cannot stand in for a 100-weight family. */
    const AbstractLayout& compact = calc.layout();
    const AbstractLayout& strict = strict_calc.layout();
    PC_CHECK(compact.junk_class_by_mod[6] == compact.junk_class_by_mod[9]);
    PC_CHECK(strict.junk_class_by_mod[6] != strict.junk_class_by_mod[9]);
    for (std::uint32_t mod = 6; mod <= 9; ++mod) {
        const std::uint32_t junk = strict.junk_class_by_mod[mod];
        PC_CHECK(junk != kNoId);
        PC_CHECK(pc_bitset_test(
            strict.junk_classes[junk].exclusion_effect_mask.data(), mod));
    }

    pc_item_state item;
    pc_item_clear(&item);
    item.rarity = PC_RARITY_RARE;
    const std::uint32_t initial = calc.intern_item(item);
    (void)calc.outcomes(initial, registry.index_by_id.at("chaos"));
    const std::uint32_t after_chaos = calc.state_count();
    for (std::uint32_t state = 0; state < after_chaos; ++state) {
        if (calc.state(state).rarity == PC_RARITY_RARE) {
            (void)calc.outcomes(state, registry.index_by_id.at("exalt"));
        }
    }

    CompiledCondition has_family;
    has_family.kind = ConditionKind::HasModFamily;
    has_family.family_id = 100;
    has_family.min_value = 1;
    CompiledCondition has_family_any = has_family;
    has_family_any.min_value = 0;
    CompiledCondition has_group;
    has_group.kind = ConditionKind::HasModGroup;
    has_group.group_id = 20;
    CompiledCondition rarity;
    rarity.kind = ConditionKind::RarityIs;
    rarity.min_value = PC_RARITY_RARE;
    CompiledCondition open_prefix;
    open_prefix.kind = ConditionKind::OpenPrefixCount;
    open_prefix.min_value = 1;
    open_prefix.max_value = 2;
    CompiledCondition open_suffix = open_prefix;
    open_suffix.kind = ConditionKind::OpenSuffixCount;
    CompiledCondition prefix_count;
    prefix_count.kind = ConditionKind::PrefixCountRange;
    prefix_count.min_value = 1;
    prefix_count.max_value = 2;
    CompiledCondition suffix_count = prefix_count;
    suffix_count.kind = ConditionKind::SuffixCountRange;
    CompiledCondition all;
    all.kind = ConditionKind::All;
    all.children = {rarity, has_family};
    CompiledCondition any;
    any.kind = ConditionKind::Any;
    any.children = {has_group, has_family};
    CompiledCondition negated;
    negated.kind = ConditionKind::Not;
    negated.children = {has_group};
    CompiledCondition threshold;
    threshold.kind = ConditionKind::AtLeast;
    threshold.min_value = 2;
    threshold.children = {rarity, has_family, has_group};
    const std::vector<CompiledCondition> conditions = {
        has_family, has_family_any, has_group, rarity, open_prefix,
        open_suffix, prefix_count, suffix_count, all, any, negated,
        threshold};

    for (std::uint32_t state = 0; state < calc.state_count(); ++state) {
        pc_item_state representative;
        if (!calc.materialize(state, representative)) continue;
        for (const CompiledCondition& condition : conditions) {
            PC_CHECK(evaluate_abstract_condition(
                         condition, *session, calc.layout(),
                         calc.state(state)) ==
                     evaluate_compiled_condition(
                         condition, *session, representative));
        }
    }
}

void run_mc_gate() {
    auto session = make_eval_session();
    const std::string json = shell(
        "rich exact vs mc", "normal",
        R"JSON({"id":"start","kind":"start"},
{"id":"transmute","kind":"operation","operation":{"type":"transmute","params":{}}},
{"id":"alteration","kind":"operation","operation":{"type":"alteration","params":{}}},
{"id":"regal","kind":"operation","operation":{"type":"regal","params":{}}},
{"id":"exalt","kind":"operation","operation":{"type":"exalt","params":{}}},
{"id":"scour","kind":"operation","operation":{"type":"scour","params":{}}},
{"id":"restart","kind":"operation","operation":{"type":"restart","params":{}}},
{"id":"success","kind":"terminal","terminal":"success"})JSON",
        R"JSON({"id":"begin","from":"start","to":"transmute","priority":0,"condition":{"type":"always"}},
{"id":"transmute_hit","from":"transmute","to":"regal","priority":0,"condition":{"type":"has_mod_family","family_mod_key":"mod0","min_tier":1}},
{"id":"transmute_miss","from":"transmute","to":"alteration","priority":999,"is_default":true},
{"id":"alt_hit","from":"alteration","to":"regal","priority":0,"condition":{"type":"has_mod_family","family_mod_key":"mod0","min_tier":1}},
{"id":"alt_repeat","from":"alteration","to":"alteration","priority":999,"is_default":true},
{"id":"regal_hit","from":"regal","to":"exalt","priority":0,"condition":{"type":"has_mod_group","group":"g20"}},
{"id":"regal_miss","from":"regal","to":"scour","priority":999,"is_default":true},
{"id":"exalt_done","from":"exalt","to":"success","priority":0,"condition":{"type":"always"}},
{"id":"scour_restart","from":"scour","to":"restart","priority":0,"condition":{"type":"always"}},
{"id":"restart_roll","from":"restart","to":"transmute","priority":0,"condition":{"type":"always"}})JSON");
    const auto strategy = compile(session, json);
    StrategyEvalOptions options;
    options.epsilon = 1e-12;
    const StrategyEvalResult exact = evaluate_strategy(*strategy, options);
    PC_CHECK(exact.converged);
    PC_CHECK(exact.success_probability > 1.0 - 1e-9);
    PC_CHECK(exact.max_mass_conservation_error < 1e-9);

    SimulatorImpl simulator;
    const std::uint64_t runs = 30000;
    const SimulationSummaryInternal mc =
        simulate(session, strategy, runs, 20260714, &simulator);
    PC_CHECK(mc.completed_runs == runs);
    PC_CHECK(mc.success_count == runs);
    PC_CHECK(mc.action_limit_count == 0);
    PC_CHECK(mc.step_limit_count == 0);
    const double observed_actions =
        static_cast<double>(mc.total_actions) / runs;
    const double action_tolerance =
        5.0 * std::sqrt(std::max(1.0, exact.expected_actions) / runs) +
        0.03 * std::max(1.0, exact.expected_actions);
    std::printf(
        "solver eval mc actions: exact=%.6f observed=%.6f tol=%.6f\n",
        exact.expected_actions, observed_actions, action_tolerance);
    PC_CHECK(std::fabs(observed_actions - exact.expected_actions) <
             action_tolerance);

    for (std::size_t i = 0; i < strategy->nodes.size(); ++i) {
        const StrategyNode& node = strategy->nodes[i];
        if (node.kind != StrategyNodeKind::Operation) continue;
        const std::string key = node.price_keys.front();
        const double expected = exact.expected_consumption.at(key);
        const double observed =
            static_cast<double>(simulator.action_counts[i]) / runs;
        const double tolerance =
            5.0 * std::sqrt(std::max(1.0, expected) / runs) +
            0.04 * std::max(1.0, expected);
        std::printf(
            "solver eval mc %s: exact=%.6f observed=%.6f tol=%.6f\n",
            key.c_str(), expected, observed, tolerance);
        PC_CHECK(std::fabs(observed - expected) < tolerance);
    }
}

void run_simulator_semantics_tests() {
    auto session = make_eval_session();
    const std::string full_item =
        R"JSON(,"prefixes":[{"mod_key":"mod0"},{"mod_key":"mod2"},{"mod_key":"mod3"}],"suffixes":[{"mod_key":"mod5"},{"mod_key":"mod6"},{"mod_key":"mod7"}])JSON";
    const auto illegal = compile(
        session,
        shell(
            "illegal", "rare",
            R"JSON({"id":"start","kind":"start"},
{"id":"exalt","kind":"operation","operation":{"type":"exalt","params":{}}},
{"id":"success","kind":"terminal","terminal":"success"})JSON",
            R"JSON({"id":"begin","from":"start","to":"exalt","priority":0,"condition":{"type":"always"}},
{"id":"done","from":"exalt","to":"success","priority":0,"condition":{"type":"always"}})JSON",
            full_item));
    const StrategyEvalResult illegal_exact = evaluate_strategy(*illegal);
    PC_CHECK(near(illegal_exact.action_not_applied_probability, 1.0));
    PC_CHECK(near(illegal_exact.expected_actions, 1.0));
    PC_CHECK(illegal_exact.expected_consumption.count("exalt") == 0);
    const SimulationSummaryInternal illegal_mc =
        simulate(session, illegal, 1, 1);
    PC_CHECK(illegal_mc.action_not_applied_count == 1);
    PC_CHECK(illegal_mc.total_actions == 1);

    const auto no_edge = compile(
        session,
        shell(
            "no edge", "normal",
            R"JSON({"id":"start","kind":"start"},
{"id":"success","kind":"terminal","terminal":"success"})JSON",
            ""));
    const StrategyEvalResult no_edge_exact = evaluate_strategy(*no_edge);
    PC_CHECK(near(no_edge_exact.no_matching_edge_probability, 1.0));
    PC_CHECK(simulate(session, no_edge, 1, 2).no_matching_edge_count == 1);

    const auto ordering = compile(
        session,
        shell(
            "ordering", "normal",
            R"JSON({"id":"start","kind":"start"},
{"id":"success","kind":"terminal","terminal":"success"},
{"id":"failure","kind":"terminal","terminal":"failure"},
{"id":"stop","kind":"terminal","terminal":"stop"})JSON",
            R"JSON({"id":"fallback","from":"start","to":"failure","priority":0,"is_default":true},
{"id":"early","from":"start","to":"stop","priority":1,"condition":{"type":"always"}},
{"id":"late","from":"start","to":"success","priority":5,"condition":{"type":"always"}})JSON"));
    const StrategyEvalResult ordering_exact = evaluate_strategy(*ordering);
    PC_CHECK(near(ordering_exact.stop_probability, 1.0));
    PC_CHECK(near(edge_value(ordering_exact, "early"), 1.0));
    PC_CHECK(near(edge_value(ordering_exact, "fallback"), 0.0));
    PC_CHECK(simulate(session, ordering, 1, 3).stop_count == 1);
}

void expect_refusal(
    const std::shared_ptr<const SessionImpl>& session,
    const std::string& json,
    const std::vector<std::string>& needles) {
    const auto strategy = compile(session, json);
    bool refused = false;
    try {
        (void)evaluate_strategy(*strategy);
    } catch (const StrategyEvalUnsupported& ex) {
        refused = true;
        const std::string message = ex.what();
        for (const std::string& needle : needles) {
            PC_CHECK(message.find(needle) != std::string::npos);
        }
    }
    PC_CHECK(refused);
}

void run_refusal_and_unresolved_tests() {
    auto session = make_eval_session();
    /* Make unsupported descriptor families present for this preflight-only
     * suite without changing the transition universe used by the exact/MC
     * tests. */
    session->veiled_prefix_mod_id = 0;
    session->eldritch_eligible = true;
    session->eldritch_searing_tier_mod_ids.resize(5);
    session->eldritch_eater_tier_mod_ids.resize(5);
    session->eldritch_searing_tier_mod_ids[1] = {0};
    session->eldritch_eater_tier_mod_ids[1] = {5};
    expect_refusal(
        session,
        shell(
            "unsupported ops", "rare",
            R"JSON({"id":"start","kind":"start"},
{"id":"veil_node","kind":"operation","operation":{"type":"veiled_chaos","params":{}}},
{"id":"eldritch_node","kind":"operation","operation":{"type":"eldritch_exalt","params":{}}},
{"id":"fracture_node","kind":"operation","operation":{"type":"fracture","params":{}}},
{"id":"success","kind":"terminal","terminal":"success"})JSON",
            R"JSON({"id":"only","from":"start","to":"success","priority":0,"condition":{"type":"always"}})JSON"),
        {"veil_node", "eldritch_node", "fracture_node"});

    expect_refusal(
        session,
        shell(
            "fractured condition", "rare",
            R"JSON({"id":"start","kind":"start"},{"id":"success","kind":"terminal","terminal":"success"})JSON",
            R"JSON({"id":"fractured_edge","from":"start","to":"success","priority":0,"condition":{"type":"has_mod_family","family_mod_key":"mod0","min_tier":1,"fractured":true}})JSON"),
        {"fractured_edge", "fractured=true"});

    expect_refusal(
        session,
        shell(
            "tier conflict", "rare",
            R"JSON({"id":"start","kind":"start"},{"id":"success","kind":"terminal","terminal":"success"})JSON",
            R"JSON({"id":"tier_one","from":"start","to":"success","priority":0,"condition":{"type":"has_mod_family","family_mod_key":"mod0","min_tier":1}},
{"id":"tier_two","from":"start","to":"success","priority":1,"condition":{"type":"has_mod_family","family_mod_key":"mod0","min_tier":2}})JSON"),
        {"tier_one", "tier_two", "align the tiers"});

    expect_refusal(
        session,
        shell(
            "too many", "rare",
            R"JSON({"id":"start","kind":"start"},{"id":"success","kind":"terminal","terminal":"success"})JSON",
            R"JSON({"id":"too_many","from":"start","to":"success","priority":0,"condition":{"type":"any","conditions":[
{"type":"has_mod_family","family_mod_key":"mod0"},{"type":"has_mod_family","family_mod_key":"mod2"},{"type":"has_mod_family","family_mod_key":"mod3"},
{"type":"has_mod_family","family_mod_key":"mod4"},{"type":"has_mod_family","family_mod_key":"mod5"},{"type":"has_mod_family","family_mod_key":"mod6"},
{"type":"has_mod_family","family_mod_key":"mod7"},{"type":"has_mod_family","family_mod_key":"mod8"},{"type":"has_mod_family","family_mod_key":"mod9"}]}})JSON"),
        {"too_many", "9 distinct"});

    expect_refusal(
        session,
        shell(
            "overlap", "rare",
            R"JSON({"id":"start","kind":"start"},{"id":"success","kind":"terminal","terminal":"success"})JSON",
            R"JSON({"id":"overlap_edge","from":"start","to":"success","priority":0,"condition":{"type":"all","conditions":[
{"type":"has_mod_family","family_mod_key":"mod0"},{"type":"has_mod_group","group":"g10"}]}})JSON"),
        {"overlap_edge", "overlapping"});

    const auto cycle = compile(
        session,
        shell(
            "cycle", "normal",
            R"JSON({"id":"start","kind":"start"},{"id":"loop","kind":"router"})JSON",
            R"JSON({"id":"enter","from":"start","to":"loop","priority":0,"condition":{"type":"always"}},
{"id":"spin","from":"loop","to":"loop","priority":0,"condition":{"type":"always"}})JSON"));
    StrategyEvalOptions cycle_options;
    cycle_options.max_sweeps = 5;
    const StrategyEvalResult unresolved =
        evaluate_strategy(*cycle, cycle_options);
    PC_CHECK(!unresolved.converged);
    PC_CHECK(near(unresolved.residual_mass, 1.0));
    PC_CHECK(unresolved.unresolved_by_node.size() == 1);
    PC_CHECK(unresolved.unresolved_by_node[0].node_id == "loop");
    PC_CHECK(near(unresolved.unresolved_by_node[0].mass, 1.0));
    PC_CHECK(unresolved.max_mass_conservation_error < 1e-12);
}

void run_c_abi_tests() {
    auto session = make_eval_session();
    const auto strategy_impl = compile(
        session,
        shell(
            "abi", "rare",
            R"JSON({"id":"start","kind":"start"},
{"id":"restart","kind":"operation","operation":{"type":"restart","params":{}}},
{"id":"success","kind":"terminal","terminal":"success"})JSON",
            R"JSON({"id":"begin","from":"start","to":"restart","priority":0,"condition":{"type":"always"}},
{"id":"done","from":"restart","to":"success","priority":0,"condition":{"type":"always"}})JSON"));
    pc_strategy strategy;
    strategy.impl = strategy_impl;
    pc_error_info error;
    pc_error_info_init(&error);
    std::size_t length = 0;
    PC_CHECK(pc_strategy_evaluate(
                 &strategy, nullptr, nullptr, 0, &length, &error) ==
             PC_RESULT_OK);
    PC_CHECK(length > 0);
    std::string json(length + 1, '\0');
    PC_CHECK(pc_strategy_evaluate(
                 &strategy, nullptr, json.data(), json.size(), &length,
                 &error) == PC_RESULT_OK);
    PC_CHECK(json.find("\"version\":\"v1\"") != std::string::npos);
    PC_CHECK(json.find("\"success\":1") != std::string::npos);
    PC_CHECK(json.find("\"key\":\"base\"") != std::string::npos);

    pc_strategy_eval_options bad{};
    bad.struct_size = sizeof(bad);
    bad.abi_version = PC_ABI_VERSION + 1;
    PC_CHECK(pc_strategy_evaluate(
                 &strategy, &bad, nullptr, 0, &length, &error) ==
             PC_RESULT_INVALID_ARGUMENT);
}

bool read_text_file(const std::string& path, std::string& out) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) return false;
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    out = buffer.str();
    return true;
}

std::shared_ptr<SessionImpl> load_artifact_session(const char* artifact_dir) {
    if (artifact_dir == nullptr) return nullptr;
    const std::string dir = artifact_dir;
    std::string manifest;
    std::string strings;
    std::string game;
    if (!read_text_file(dir + "/manifest.json", manifest) ||
        !read_text_file(dir + "/strings.json", strings) ||
        !read_text_file(dir + "/game-data.json", game)) {
        return nullptr;
    }
    auto data = load_data_impl(manifest, strings, game);
    const auto base = data->base_by_path.find(
        "Metadata/Items/Armours/BodyArmours/BodyInt17");
    if (base == data->base_by_path.end()) return nullptr;
    auto session = std::make_shared<SessionImpl>();
    session->data = data;
    session->base_index = base->second;
    session->item_level = 86;
    build_session(*session);
    return session;
}

std::string mod_key(const SessionImpl& session, std::uint32_t mod) {
    const DataImpl& data = *session.data;
    return data.string_at(data.mod_key_sid[session.global_index[mod]]);
}

std::string action_type_name(ActionType type) {
    switch (type) {
    case ActionType::Transmute: return "transmute";
    case ActionType::Augment: return "augment";
    case ActionType::Alteration: return "alteration";
    case ActionType::Regal: return "regal";
    case ActionType::Alchemy: return "alchemy";
    case ActionType::Chaos: return "chaos";
    case ActionType::Exalt: return "exalt";
    case ActionType::Annul: return "annul";
    case ActionType::Scour: return "scour";
    case ActionType::Essence: return "essence";
    case ActionType::Fossil: return "fossil";
    case ActionType::Bench: return "bench";
    case ActionType::VeiledChaos: return "veiled_chaos";
    case ActionType::VeiledExalt: return "veiled_exalt";
    case ActionType::Unveil: return "unveil";
    case ActionType::HarvestReforge: return "harvest_reforge";
    case ActionType::HarvestAugment: return "harvest_augment";
    case ActionType::HarvestResist: return "harvest_resist";
    case ActionType::EldritchEmber: return "eldritch_ember";
    case ActionType::EldritchIchor: return "eldritch_ichor";
    case ActionType::EldritchExalt: return "eldritch_exalt";
    case ActionType::EldritchChaos: return "eldritch_chaos";
    case ActionType::EldritchAnnul: return "eldritch_annul";
    case ActionType::InfluenceExalt: return "influence_exalt";
    case ActionType::Fracture: return "fracture";
    }
    return "";
}

std::string operation_json(
    const SessionImpl& session,
    const ActionDescriptor& action) {
    const DataImpl& data = *session.data;
    if (action.synthetic) return "{\"type\":\"restart\",\"params\":{}}";
    const std::string type = action_type_name(action.params.type);
    std::string params;
    switch (action.params.type) {
    case ActionType::Essence:
        params = "\"essence_key\":\"" + data.string_at(
                     data.essence_key_sids[action.params.essence_index]) +
                 "\"";
        break;
    case ActionType::Fossil:
        params = "\"fossils\":[";
        for (std::size_t i = 0; i < action.params.fossil_indices.size(); ++i) {
            if (i != 0) params += ',';
            params += "\"" + data.string_at(
                data.fossil_key_sids[action.params.fossil_indices[i]]) + "\"";
        }
        params += ']';
        break;
    case ActionType::Bench:
        params = "\"mod_key\":\"" + mod_key(session, action.params.mod_id) +
                 "\"";
        break;
    case ActionType::Unveil: {
        std::uint32_t choice = kNoId;
        pc_bitset_for_each(
            session.unveiled_mask.data(), session.words,
            [&](std::size_t bit) {
                if (choice == kNoId) choice = static_cast<std::uint32_t>(bit);
            });
        if (choice == kNoId) return std::string();
        params = "\"mod_key\":\"" + mod_key(session, choice) + "\"";
        break;
    }
    case ActionType::HarvestReforge:
    case ActionType::HarvestAugment:
        params = "\"target_tag\":\"" +
                 data.tag_name_by_id.at(action.params.target_tag_id) + "\"";
        break;
    case ActionType::HarvestResist:
        params = "\"source_tag\":\"" +
                 data.tag_name_by_id.at(action.params.source_tag_id) +
                 "\",\"target_tag\":\"" +
                 data.tag_name_by_id.at(action.params.target_tag_id) + "\"";
        break;
    case ActionType::EldritchEmber:
    case ActionType::EldritchIchor:
        params = "\"tier\":" + std::to_string(action.params.tier);
        break;
    case ActionType::InfluenceExalt:
        params = "\"influence\":\"" + data.influence_name_by_code.at(
                     action.params.influence_code) + "\"";
        break;
    default:
        break;
    }
    return "{\"type\":\"" + type + "\",\"params\":{" + params + "}}";
}

void run_artifact_and_registry_tests(const char* artifact_dir) {
    std::shared_ptr<SessionImpl> session;
    try {
        session = load_artifact_session(artifact_dir);
    } catch (const std::exception& ex) {
        std::printf("solver eval artifact suite: %s\n", ex.what());
        PC_CHECK(false);
        return;
    }
    if (session == nullptr) {
        std::printf("solver eval artifact suite skipped (unreadable)\n");
        return;
    }

    /* Vaal Regalia fixture gate, including the public query ABI. */
    const auto strategy = compile(
        session,
        std::string(R"JSON({"version":"v1","name":"artifact eval","start_node_id":"start","base_state":{"base_key":"Metadata/Items/Armours/BodyArmours/BodyInt17","item_level":86,"rarity":"rare"},"nodes":[
{"id":"start","kind":"start"},{"id":"restart","kind":"operation","operation":{"type":"restart","params":{}}},{"id":"success","kind":"terminal","terminal":"success"}],"edges":[
{"id":"begin","from":"start","to":"restart","priority":0,"condition":{"type":"always"}},{"id":"done","from":"restart","to":"success","priority":0,"condition":{"type":"always"}}]})JSON"));
    const StrategyEvalResult artifact_result = evaluate_strategy(*strategy);
    PC_CHECK(near(artifact_result.success_probability, 1.0));
    PC_CHECK(near(artifact_result.expected_consumption.at("base"), 1.0));
    pc_strategy strategy_handle;
    strategy_handle.impl = strategy;
    pc_error_info error;
    pc_error_info_init(&error);
    std::size_t length = 0;
    PC_CHECK(pc_strategy_evaluate(
                 &strategy_handle, nullptr, nullptr, 0, &length, &error) ==
             PC_RESULT_OK);
    PC_CHECK(length > 100);

    /* One-step exact-vs-MC gate on the Vaal Regalia pool. */
    std::uint32_t target_mod = kNoId;
    for (std::uint32_t mod = 0; mod < session->mod_count; ++mod) {
        if (session->gen_type[mod] == 0 &&
            pc_bitset_test(session->normal_random_roll_mask.data(), mod) &&
            pc_bitset_test(session->positive_base_weight_mask.data(), mod)) {
            target_mod = mod;
            break;
        }
    }
    PC_CHECK(target_mod != kNoId);
    if (target_mod != kNoId) {
        const std::string target_key = mod_key(*session, target_mod);
        const std::string exalt_json =
            std::string(R"JSON({"version":"v1","name":"artifact exalt","start_node_id":"start","base_state":{"base_key":"Metadata/Items/Armours/BodyArmours/BodyInt17","item_level":86,"rarity":"rare"},"nodes":[
{"id":"start","kind":"start"},{"id":"exalt","kind":"operation","operation":{"type":"exalt","params":{}}},{"id":"success","kind":"terminal","terminal":"success"},{"id":"failure","kind":"terminal","terminal":"failure"}],"edges":[
{"id":"begin","from":"start","to":"exalt","priority":0,"condition":{"type":"always"}},{"id":"hit","from":"exalt","to":"success","priority":0,"condition":{"type":"has_mod_family","family_mod_key":")JSON") +
            target_key +
            R"JSON(","min_tier":0}},{"id":"miss","from":"exalt","to":"failure","priority":999,"is_default":true}]})JSON";
        const auto exalt_strategy = compile(session, exalt_json);
        const StrategyEvalResult exalt_exact =
            evaluate_strategy(*exalt_strategy);
        PC_CHECK(exalt_exact.converged);
        PC_CHECK(near(exalt_exact.expected_actions, 1.0));
        PC_CHECK(near(exalt_exact.expected_consumption.at("exalt"), 1.0));
        const std::uint64_t runs = 50000;
        const SimulationSummaryInternal exalt_mc =
            simulate(session, exalt_strategy, runs, 20260715);
        const double observed =
            static_cast<double>(exalt_mc.success_count) / runs;
        const double sigma = std::sqrt(
            exalt_exact.success_probability *
            (1.0 - exalt_exact.success_probability) / runs);
        PC_CHECK(std::fabs(observed - exalt_exact.success_probability) <
                 5.0 * sigma + 0.002);
    }

    ActionRegistry registry = build_action_registry(*session);
    /* Every descriptor, including every fossil loadout, resolves by its
     * compiled ActionParameters. */
    for (std::uint32_t i = 0; i < registry.actions.size(); ++i) {
        const ActionDescriptor& descriptor = registry.actions[i];
        StrategyNode node;
        node.kind = StrategyNodeKind::Operation;
        node.action = descriptor.params;
        node.action_type = descriptor.synthetic
                               ? kStrategyRestartOperation
                               : static_cast<int>(descriptor.params.type);
        PC_CHECK(resolve_strategy_action(node, registry) == i);
    }

    /* One simulator-compiled representative per operation type pins its
     * price keys against the registry. Prefer a four-fossil loadout. */
    std::map<int, std::uint32_t> representative;
    std::uint32_t restart = kNoId;
    for (std::uint32_t i = 0; i < registry.actions.size(); ++i) {
        const ActionDescriptor& descriptor = registry.actions[i];
        if (descriptor.synthetic) {
            restart = i;
            continue;
        }
        const int type = static_cast<int>(descriptor.params.type);
        const auto found = representative.find(type);
        if (found == representative.end() ||
            (descriptor.params.type == ActionType::Fossil &&
             descriptor.params.fossil_indices.size() >
                 registry.actions[found->second].params.fossil_indices.size())) {
            representative[type] = i;
        }
    }
    if (restart != kNoId) representative[kStrategyRestartOperation] = restart;
    std::size_t compiled_types = 0;
    for (const auto& [type, index] : representative) {
        const ActionDescriptor& descriptor = registry.actions[index];
        const std::string operation = operation_json(*session, descriptor);
        if (operation.empty()) continue;
        const std::string json =
            std::string(R"JSON({"version":"v1","name":"price parity","start_node_id":"start","base_state":{"base_key":"Metadata/Items/Armours/BodyArmours/BodyInt17","item_level":86,"rarity":"rare"},"nodes":[
{"id":"start","kind":"start"},{"id":"op","kind":"operation","operation":)JSON") +
            operation +
            R"JSON(},{"id":"success","kind":"terminal","terminal":"success"}],"edges":[
{"id":"begin","from":"start","to":"op","priority":0,"condition":{"type":"always"}},{"id":"done","from":"op","to":"success","priority":0,"condition":{"type":"always"}}]})JSON";
        const auto compiled = compile(session, json);
        const StrategyNode& node = compiled->nodes[1];
        PC_CHECK(node.price_keys == descriptor.cost_keys);
        PC_CHECK(resolve_strategy_action(node, registry) == index);
        if (descriptor.params.type == ActionType::Fossil) {
            PC_CHECK(descriptor.params.fossil_indices.size() == 4);
            PC_CHECK(node.price_keys.back() == "resonator:4");
        }
        ++compiled_types;
    }
    PC_CHECK(compiled_types >= 20);
}

} // namespace

void run_solver_eval_tests(const char* artifact_dir) {
    const auto stage = [](const char* name, const auto& fn) {
        try {
            fn();
        } catch (const std::exception& ex) {
            std::printf("solver eval %s: %s\n", name, ex.what());
            PC_CHECK(false);
        }
    };
    stage("closed form", [&] { run_closed_form_tests(); });
    stage("condition parity", [&] { run_condition_parity_tests(); });
    stage("mc gate", [&] { run_mc_gate(); });
    stage("simulator semantics", [&] { run_simulator_semantics_tests(); });
    stage("refusals", [&] { run_refusal_and_unresolved_tests(); });
    stage("c abi", [&] { run_c_abi_tests(); });
    stage("artifact", [&] { run_artifact_and_registry_tests(artifact_dir); });
}
