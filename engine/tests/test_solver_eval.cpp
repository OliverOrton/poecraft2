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
    data->mod_global_ids = {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    data->strings = {
        "", "synthetic/base", "mod0", "mod1", "mod2", "mod3",
        "mod4", "mod5", "mod6", "mod7", "mod8", "mod9",
        "g10", "g11", "g12", "g13", "g20", "g21", "g22",
        "g23", "g24", "bench_prefix_lock", "bench_multimod",
        "bench_finish_prefix", "bench_suffix_lock", "bench_blocker",
        "bench_finish_suffix", "g25", "g26", "g27", "g28", "g29",
        "g30"};
    data->base_count = 1;
    data->base_metadata_path_sid = {1};
    data->mod_key_sid = {
        2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 21, 22, 23, 24, 25, 26};
    for (std::uint32_t mod = 0; mod < 16; ++mod) {
        data->mod_pos_by_key.emplace(
            data->strings[data->mod_key_sid[mod]], mod);
    }
    data->group_key_sids.assign(31, 0);
    const std::pair<std::uint32_t, std::uint32_t> groups[] = {
        {10, 12}, {11, 13}, {12, 14}, {13, 15}, {20, 16},
        {21, 17}, {22, 18}, {23, 19}, {24, 20}, {25, 27},
        {26, 28}, {27, 29}, {28, 30}, {29, 31}, {30, 32}};
    for (const auto& [group, sid] : groups) {
        data->group_key_sids[group] = sid;
        data->group_id_by_key.emplace(data->strings[sid], group);
    }

    auto session = std::make_shared<SessionImpl>();
    session->data = data;
    session->base_index = 0;
    session->item_level = 1;
    session->mod_count = 16;
    session->words = pc_bitset_words(16);
    session->global_index = {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    session->gen_type = {
        0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1};
    session->primary_group = {
        10, 10, 11, 12, 13, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30};
    session->required_level.assign(16, 1);
    session->group_offsets = {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    session->group_ids = {
        10, 10, 11, 12, 13, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30};
    session->family_id = {
        100, 100, 101, 102, 103, 104, 105, 106,
        107, 108, 109, 110, 111, 112, 113, 114};
    session->family_tier_index = {
        1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    session->metamod_type.assign(16, -1);
    data->metamod_multimod_code = 1;
    data->metamod_prefixes_locked_code = 2;
    data->metamod_suffixes_locked_code = 3;
    data->metamod_no_attack_code = 4;
    data->metamod_no_caster_code = 5;
    session->metamod_type[10] = data->metamod_prefixes_locked_code;
    session->metamod_type[13] = data->metamod_suffixes_locked_code;
    session->metamod_type[11] = data->metamod_multimod_code;
    session->special_kind.assign(16, -1);
    session->flags.assign(16, 0);
    session->bench_mod_ids = {10, 11, 12, 13, 14, 15};
    for (const std::uint32_t mod : session->bench_mod_ids) {
        session->flags[mod] |= 1u << 1;
    }
    session->influence_code.assign(16, -1);
    session->class_offsets.assign(17, 0);
    session->rare_affix_cap = 3;
    session->base_spawn_weight = {
        100, 100, 100, 100, 100, 100, 100, 100,
        100, 400, 0, 0, 0, 0, 0, 0};
    session->base_gen_pct.assign(16, 100);
    session->base_roll_weight = session->base_spawn_weight;
    for (std::uint32_t mod = 0; mod < 16; ++mod) {
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
    session->group_masks.assign(31, {});
    session->influence_masks.assign(1, std::vector<std::uint64_t>(words, 0));
    for (std::uint32_t mod = 0; mod < 16; ++mod) {
        if (mod < 10) {
            pc_bitset_set(session->normal_random_roll_mask.data(), mod);
            pc_bitset_set(session->positive_spawn_weight_mask.data(), mod);
            pc_bitset_set(session->positive_base_weight_mask.data(), mod);
        }
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

const StrategyEvalActionTotal* action_total(
    const StrategyEvalResult& result,
    const std::string& id) {
    const auto found = std::find_if(
        result.action_totals.begin(), result.action_totals.end(),
        [&](const StrategyEvalActionTotal& action) { return action.id == id; });
    return found == result.action_totals.end() ? nullptr : &*found;
}

const StrategyEvalMaterialTotal* material_total(
    const StrategyEvalResult& result,
    const std::string& key) {
    const auto found = std::find_if(
        result.material_totals.begin(), result.material_totals.end(),
        [&](const StrategyEvalMaterialTotal& material) {
            return material.price_key == key;
        });
    return found == result.material_totals.end() ? nullptr : &*found;
}

void check_reference_parity(
    const StrategyImpl& strategy,
    const StrategyEvalResult& exact,
    const StrategyEvalOptions& options = {}) {
    const StrategyEvalResult reference =
        evaluate_strategy_forward_reference_for_test(strategy, options);
    PC_CHECK(std::fabs(
                 exact.success_probability - reference.success_probability) <
             1e-9);
    PC_CHECK(std::fabs(
                 exact.failure_probability - reference.failure_probability) <
             1e-9);
    PC_CHECK(std::fabs(exact.stop_probability - reference.stop_probability) <
             1e-9);
    PC_CHECK(std::fabs(
                 exact.action_not_applied_probability -
                 reference.action_not_applied_probability) < 1e-9);
    PC_CHECK(std::fabs(
                 exact.no_matching_edge_probability -
                 reference.no_matching_edge_probability) < 1e-9);
    PC_CHECK(std::fabs(exact.expected_actions - reference.expected_actions) <
             1e-9);
    PC_CHECK(exact.expected_consumption.size() ==
             reference.expected_consumption.size());
    for (const auto& [key, quantity] : exact.expected_consumption) {
        PC_CHECK(std::fabs(
                     quantity - reference.expected_consumption.at(key)) <
                 1e-9);
    }
    PC_CHECK(exact.edges.size() == reference.edges.size());
    for (const StrategyEvalEdge& edge : exact.edges) {
        PC_CHECK(std::fabs(
                     edge.expected_traversals -
                     edge_value(reference, edge.id)) < 1e-9);
    }
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
    PC_CHECK(action_total(straight, "restart") != nullptr);
    PC_CHECK(material_total(straight, "base") != nullptr);
    PC_CHECK(material_total(straight, "base") != nullptr &&
             !material_total(straight, "base")->priced);
    PC_CHECK(!straight.pricing_enabled);
    PC_CHECK(near(
        straight.technique_totals.at("restart_actions"), 1.0));
    PC_CHECK(near(
        straight.technique_totals.at("base_consumptions"), 1.0));
    PC_CHECK(near(edge_value(straight, "begin"), 1.0));
    PC_CHECK(near(edge_value(straight, "done"), 1.0));
    PC_CHECK(straight.max_mass_conservation_error < 1e-12);
    check_reference_parity(*deterministic_strategy, straight);

    const std::string loop = shell(
        "chaos loop", "rare",
        R"JSON({"id":"start","kind":"start"},
{"id":"chaos","kind":"operation","operation":{"type":"chaos","params":{}},"accounting_roles":["retry_action"]},
{"id":"success","kind":"terminal","terminal":"success"})JSON",
        R"JSON({"id":"begin","from":"start","to":"chaos","priority":0,"condition":{"type":"always"}},
{"id":"hit","from":"chaos","to":"success","priority":0,"condition":{"type":"has_mod_family","family_mod_key":"mod0","min_tier":1}},
{"id":"repeat","from":"chaos","to":"chaos","priority":999,"is_default":true,"accounting_roles":["retry"]})JSON");
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
    auto loop_economy = std::make_shared<EconomyImpl>();
    loop_economy->id = "s8.4-loop-prices";
    loop_economy->prices = {{"chaos", 2.0}};
    options.economy = loop_economy;
    options.review_projection_json = R"JSON({
      "schema_version":"solver_review_projection_v1",
      "raw_strategy":{"execution_authority":"raw_strategy_only"},
      "sections":[
        {"id":"setup","label":"Setup","role":"setup","raw_references":[{"node_id":"start"},{"edge_id":"begin"}]},
        {"id":"rolling","label":"Retry rolling","role":"rolling","raw_references":[{"node_id":"chaos"},{"edge_id":"hit"},{"edge_id":"repeat"}]},
        {"id":"finish","label":"Finished","role":"finishing","raw_references":[{"node_id":"success"}]}
      ]
    })JSON";
    const StrategyEvalResult exact = evaluate_strategy(*loop_strategy, options);
    PC_CHECK(exact.converged);
    PC_CHECK(std::fabs(exact.success_probability - 1.0) < 1e-10);
    PC_CHECK(std::fabs(exact.expected_actions - 1.0 / p) < 1e-10);
    PC_CHECK(std::fabs(edge_value(exact, "hit") - 1.0) < 1e-10);
    PC_CHECK(std::fabs(edge_value(exact, "repeat") - (1.0 - p) / p) <
             1e-10);
    PC_CHECK(std::fabs(
                 exact.expected_consumption.at("chaos") - 1.0 / p) <
             1e-10);
    PC_CHECK(exact.max_mass_conservation_error < 1e-10);
    PC_CHECK(exact.sweeps == 0);
    const StrategyEvalActionTotal* chaos_total = action_total(exact, "chaos");
    PC_CHECK(chaos_total != nullptr);
    PC_CHECK(chaos_total != nullptr &&
             near(chaos_total->expected_visits, 1.0 / p, 1e-10));
    PC_CHECK(chaos_total != nullptr && chaos_total->nodes.size() == 1);
    const StrategyEvalMaterialTotal* chaos_material =
        material_total(exact, "chaos");
    PC_CHECK(chaos_material != nullptr && chaos_material->priced);
    PC_CHECK(chaos_material != nullptr &&
             near(chaos_material->cost_contribution, 2.0 / p, 1e-10));
    PC_CHECK(near(exact.known_expected_cost, 2.0 / p, 1e-10));
    PC_CHECK(exact.cost_complete);
    PC_CHECK(exact.occupancy_states.size() >= 2);
    PC_CHECK(!exact.occupancy.empty());
    double chaos_occupancy = 0.0;
    double occupancy_reward = 0.0;
    for (const StrategyEvalOccupancyEntry& entry : exact.occupancy) {
        PC_CHECK(entry.state < exact.occupancy_states.size());
        PC_CHECK(entry.action != kNoId);
        PC_CHECK(entry.reward_complete);
        if (entry.action == chaos) {
            chaos_occupancy += entry.expected_visits;
        }
        occupancy_reward +=
            entry.expected_applied * entry.immediate_reward;
    }
    PC_CHECK(near(chaos_occupancy, 1.0 / p, 1e-10));
    PC_CHECK(exact.occupancy_reward_complete);
    PC_CHECK(near(exact.occupancy_expected_reward, 2.0 / p, 1e-10));
    PC_CHECK(near(occupancy_reward, exact.known_expected_cost, 1e-10));
    PC_CHECK(near(exact.occupancy_reward_difference, 0.0, 1e-10));
    PC_CHECK(near(
        exact.technique_totals.at("retry_actions"), 1.0 / p, 1e-10));
    PC_CHECK(near(
        exact.technique_totals.at("retry_count"), (1.0 - p) / p,
        1e-10));
    PC_CHECK(exact.review_sections.size() == 3);
    PC_CHECK(near(
        exact.review_sections[1].expected_actions, 1.0 / p, 1e-10));
    PC_CHECK(near(
        material_total(exact, "chaos")->expected_quantity, 1.0 / p,
        1e-10));
    PC_CHECK(near(exact.section_actions_difference, 0.0, 1e-12));
    PC_CHECK(near(
        exact.section_material_differences.at("chaos"), 0.0, 1e-12));
    PC_CHECK(near(exact.action_descriptor_visits_difference, 0.0, 1e-12));
    PC_CHECK(near(exact.node_operation_visits_difference, 0.0, 1e-12));
    PC_CHECK(near(
        exact.material_quantity_differences.at("chaos"), 0.0, 1e-12));
    SimulatorImpl loop_simulator;
    const std::uint64_t accounting_runs = 10000;
    (void)simulate(
        session, loop_strategy, accounting_runs, 20260718, &loop_simulator);
    const double sampled_chaos =
        static_cast<double>(
            loop_simulator.action_descriptor_counts.at("chaos")) /
        accounting_runs;
    const double sampled_material =
        static_cast<double>(loop_simulator.material_counts.at("chaos")) /
        accounting_runs;
    std::printf(
        "solver S8.4 accounting loop: runs=%llu seed=%llu p=%.12f "
        "exact_actions=%.12f sampled_actions=%.12f "
        "exact_material=%.12f sampled_material=%.12f\n",
        static_cast<unsigned long long>(accounting_runs),
        static_cast<unsigned long long>(20260718), p, 1.0 / p,
        sampled_chaos, 1.0 / p, sampled_material);
    PC_CHECK(std::fabs(sampled_chaos - 1.0 / p) < 0.08 / p + 0.02);
    PC_CHECK(near(sampled_chaos, sampled_material, 1e-12));
    PC_CHECK(loop_simulator.options.seed == 20260718);
    check_reference_parity(*loop_strategy, exact, options);

    auto two_state_session = make_eval_session();
    pc_bitset_zero(
        two_state_session->normal_random_roll_mask.data(),
        two_state_session->words);
    pc_bitset_zero(
        two_state_session->positive_spawn_weight_mask.data(),
        two_state_session->words);
    pc_bitset_zero(
        two_state_session->positive_base_weight_mask.data(),
        two_state_session->words);
    pc_bitset_zero(
        two_state_session->influence_masks[0].data(),
        two_state_session->words);
    two_state_session->base_spawn_weight.assign(10, 0);
    two_state_session->base_roll_weight.assign(10, 0);
    for (const std::uint32_t mod : {0u, 5u}) {
        pc_bitset_set(two_state_session->normal_random_roll_mask.data(), mod);
        pc_bitset_set(
            two_state_session->positive_spawn_weight_mask.data(), mod);
        pc_bitset_set(
            two_state_session->positive_base_weight_mask.data(), mod);
        pc_bitset_set(two_state_session->influence_masks[0].data(), mod);
        two_state_session->base_spawn_weight[mod] = 100;
        two_state_session->base_roll_weight[mod] = 100;
    }
    const auto two_node_strategy = compile(
        two_state_session,
        shell(
            "two state cyclic", "magic",
            R"JSON({"id":"start","kind":"start"},
{"id":"augment","kind":"operation","operation":{"type":"augment","params":{}}},
{"id":"annul","kind":"operation","operation":{"type":"annul","params":{}}},
{"id":"success","kind":"terminal","terminal":"success"})JSON",
            R"JSON({"id":"begin","from":"start","to":"augment","priority":0,"condition":{"type":"always"}},
{"id":"hit","from":"augment","to":"success","priority":0,"condition":{"type":"has_mod_family","family_mod_key":"mod0","min_tier":1}},
{"id":"miss","from":"augment","to":"annul","priority":999,"is_default":true},
{"id":"retry","from":"annul","to":"augment","priority":0,"condition":{"type":"always"}})JSON"));
    const StrategyEvalResult two_node = evaluate_strategy(*two_node_strategy);
    PC_CHECK(two_node.converged);
    PC_CHECK(std::fabs(two_node.success_probability - 1.0) < 1e-10);
    PC_CHECK(std::fabs(two_node.expected_actions - 3.0) < 1e-10);
    PC_CHECK(std::fabs(edge_value(two_node, "hit") - 1.0) < 1e-10);
    PC_CHECK(std::fabs(edge_value(two_node, "miss") - 1.0) < 1e-10);
    PC_CHECK(std::fabs(edge_value(two_node, "retry") - 1.0) < 1e-10);
    PC_CHECK(std::fabs(
                 two_node.expected_consumption.at("augment") - 2.0) <
             1e-10);
    PC_CHECK(std::fabs(
                 two_node.expected_consumption.at("annul") - 1.0) <
             1e-10);
    PC_CHECK(two_node.sweeps == 0);
    check_reference_parity(*two_node_strategy, two_node);

    const auto fracture_strategy = compile(
        session,
        shell(
            "fracture accounting", "rare",
            R"JSON({"id":"start","kind":"start"},
{"id":"prepare","kind":"operation","operation":{"type":"bench","params":{"mod_key":"bench_finish_prefix"}},"accounting_roles":["fracture_preparation"]},
{"id":"fracture","kind":"operation","operation":{"type":"fracture","params":{}}},
{"id":"cleanup","kind":"operation","operation":{"type":"remove_crafted_modifiers","params":{}}},
{"id":"restart","kind":"operation","operation":{"type":"restart","params":{}}},
{"id":"success","kind":"terminal","terminal":"success"},
{"id":"failure","kind":"terminal","terminal":"failure"})JSON",
            R"JSON({"id":"begin","from":"start","to":"prepare","priority":0,"condition":{"type":"all","conditions":[{"type":"has_mod_family","family_mod_key":"mod0"},{"type":"has_mod_family","family_mod_key":"mod2"},{"type":"has_mod_family","family_mod_key":"mod5"},{"type":"has_mod_family","family_mod_key":"mod6"}]}},
{"id":"prepared","from":"prepare","to":"fracture","priority":0,"condition":{"type":"always"}},
{"id":"fracture_hit","from":"fracture","to":"success","priority":0,"condition":{"type":"has_mod_family","family_mod_key":"mod0","fractured":true}},
{"id":"fracture_miss","from":"fracture","to":"cleanup","priority":999,"is_default":true},
{"id":"cleaned","from":"cleanup","to":"restart","priority":0,"condition":{"type":"always"}},
{"id":"recovered","from":"restart","to":"failure","priority":0,"condition":{"type":"always"}})JSON",
            R"JSON(,"prefixes":["mod0","mod2"],"suffixes":["mod5","mod6"])JSON"));
    StrategyEvalOptions fracture_options;
    auto fracture_economy = std::make_shared<EconomyImpl>();
    fracture_economy->id = "s8.4-fracture-prices";
    fracture_economy->prices = {
        {"bench:bench_finish_prefix", 3.0}, {"fracture", 10.0},
        {"scour", 1.0}, {"base", 5.0}};
    fracture_options.economy = fracture_economy;
    fracture_options.include_success_normalized = true;
    fracture_options.review_projection_json = R"JSON({
      "schema_version":"solver_review_projection_v1",
      "raw_strategy":{"execution_authority":"raw_strategy_only"},
      "sections":[
        {"id":"setup","label":"Setup","role":"setup","raw_references":[{"node_id":"start"},{"edge_id":"begin"}]},
        {"id":"fracture","label":"Fracture","role":"fracture","raw_references":[{"node_id":"prepare"},{"node_id":"fracture"},{"edge_id":"prepared"},{"edge_id":"fracture_hit"},{"edge_id":"fracture_miss"}]},
        {"id":"recovery","label":"Recovery","role":"recovery","raw_references":[{"node_id":"cleanup"},{"node_id":"restart"},{"edge_id":"cleaned"},{"edge_id":"recovered"}]},
        {"id":"success","label":"Success","role":"finishing","raw_references":[{"node_id":"success"}]},
        {"id":"failure","label":"Failure","role":"recovery","raw_references":[{"node_id":"failure"}]}
      ]
    })JSON";
    const StrategyEvalResult fracture_accounting =
        evaluate_strategy(*fracture_strategy, fracture_options);
    PC_CHECK(near(fracture_accounting.success_probability, 0.2, 1e-12));
    PC_CHECK(near(fracture_accounting.failure_probability, 0.8, 1e-12));
    PC_CHECK(near(fracture_accounting.expected_actions, 3.6, 1e-12));
    PC_CHECK(near(
        fracture_accounting.expected_consumption.at(
            "bench:bench_finish_prefix"),
        1.0));
    PC_CHECK(near(
        fracture_accounting.expected_consumption.at("fracture"), 1.0));
    PC_CHECK(near(
        fracture_accounting.expected_consumption.at(
            "scour"),
        0.8, 1e-12));
    PC_CHECK(near(
        fracture_accounting.expected_consumption.at("base"), 0.8,
        1e-12));
    PC_CHECK(near(fracture_accounting.total_expected_cost, 17.8, 1e-12));
    PC_CHECK(fracture_accounting.success_normalized_enabled);
    PC_CHECK(near(
        fracture_accounting.expected_actions /
            fracture_accounting.success_probability,
        18.0, 1e-12));
    PC_CHECK(near(
        fracture_accounting.total_expected_cost /
            fracture_accounting.success_probability,
        89.0, 1e-12));
    PC_CHECK(near(
        fracture_accounting.technique_totals.at(
            "fracture_preparation_actions"),
        1.0));
    PC_CHECK(near(
        fracture_accounting.technique_totals.at("fracture_actions"),
        1.0));
    PC_CHECK(near(
        fracture_accounting.technique_totals.at(
            "crafted_mod_cleanup_or_replacement_actions"),
        0.8, 1e-12));
    PC_CHECK(near(
        fracture_accounting.technique_totals.at("restart_actions"),
        0.8, 1e-12));
    PC_CHECK(near(fracture_accounting.section_actions_difference, 0.0));
    PC_CHECK(fracture_accounting.review_sections.size() == 5);
    PC_CHECK(near(
        fracture_accounting.review_sections[1].expected_actions, 2.0));
    PC_CHECK(near(
        fracture_accounting.review_sections[2].expected_actions, 1.6,
        1e-12));
    for (const auto& [unused, difference] :
         fracture_accounting.section_material_differences) {
        (void)unused;
        PC_CHECK(near(difference, 0.0, 1e-12));
    }
    auto missing_fracture_economy = std::make_shared<EconomyImpl>(
        *fracture_economy);
    missing_fracture_economy->id = "s8.4-missing-fracture";
    missing_fracture_economy->prices.erase("fracture");
    fracture_options.economy = missing_fracture_economy;
    const StrategyEvalResult missing_price =
        evaluate_strategy(*fracture_strategy, fracture_options);
    PC_CHECK(!missing_price.cost_complete);
    PC_CHECK(material_total(missing_price, "fracture") != nullptr);
    PC_CHECK(material_total(missing_price, "fracture") != nullptr &&
             !material_total(missing_price, "fracture")->priced);
    PC_CHECK(near(missing_price.known_expected_cost, 7.8, 1e-12));
    const std::string missing_json = serialize_strategy_eval(missing_price);
    PC_CHECK(missing_json.find("\"status\":\"incomplete\"") !=
             std::string::npos);
    PC_CHECK(missing_json.find("\"missing_price_keys\":[\"fracture\"]") !=
             std::string::npos);
    PC_CHECK(missing_json.find("\"total_expected_cost\":null") !=
             std::string::npos);

    const auto technique_strategy = compile(
        session,
        shell(
            "bench technique accounting", "rare",
            R"JSON({"id":"start","kind":"start"},
{"id":"blocker","kind":"operation","operation":{"type":"bench","params":{"mod_key":"bench_blocker"}},"accounting_roles":["temporary_blocker"]},
{"id":"blocker_cleanup","kind":"operation","operation":{"type":"remove_crafted_modifiers","params":{}}},
{"id":"protect_once","kind":"operation","operation":{"type":"bench","params":{"mod_key":"bench_prefix_lock"}},"accounting_roles":["protection_setup"]},
{"id":"protect_cleanup_once","kind":"operation","operation":{"type":"remove_crafted_modifiers","params":{}}},
{"id":"protect_again","kind":"operation","operation":{"type":"bench","params":{"mod_key":"bench_prefix_lock"}},"accounting_roles":["protection_setup"]},
{"id":"protect_cleanup_again","kind":"operation","operation":{"type":"remove_crafted_modifiers","params":{}}},
{"id":"multimod","kind":"operation","operation":{"type":"bench","params":{"mod_key":"bench_multimod"}},"accounting_roles":["multimod_setup"]},
{"id":"finish_one","kind":"operation","operation":{"type":"bench","params":{"mod_key":"bench_finish_prefix"}},"accounting_roles":["multimod_finish","permanent_goal_bench","deterministic_finish"]},
{"id":"finish_two","kind":"operation","operation":{"type":"bench","params":{"mod_key":"bench_finish_suffix"}},"accounting_roles":["multimod_finish","permanent_goal_bench","deterministic_finish"]},
{"id":"success","kind":"terminal","terminal":"success"})JSON",
            R"JSON({"id":"begin","from":"start","to":"blocker","priority":0,"condition":{"type":"always"}},
{"id":"blocker_used","from":"blocker","to":"blocker_cleanup","priority":0,"condition":{"type":"always"}},
{"id":"blocker_cleaned","from":"blocker_cleanup","to":"protect_once","priority":0,"condition":{"type":"always"}},
{"id":"protected_once","from":"protect_once","to":"protect_cleanup_once","priority":0,"condition":{"type":"always"}},
{"id":"reapply","from":"protect_cleanup_once","to":"protect_again","priority":0,"condition":{"type":"always"},"accounting_roles":["protection_reapplication"]},
{"id":"protected_again","from":"protect_again","to":"protect_cleanup_again","priority":0,"condition":{"type":"always"}},
{"id":"protection_cleaned","from":"protect_cleanup_again","to":"multimod","priority":0,"condition":{"type":"always"}},
{"id":"multimod_ready","from":"multimod","to":"finish_one","priority":0,"condition":{"type":"always"}},
{"id":"finish_one_done","from":"finish_one","to":"finish_two","priority":0,"condition":{"type":"always"}},
{"id":"finished","from":"finish_two","to":"success","priority":0,"condition":{"type":"always"}})JSON"));
    StrategyEvalOptions technique_options;
    auto technique_economy = std::make_shared<EconomyImpl>();
    technique_economy->id = "s8.4-technique-prices";
    technique_economy->prices = {
        {"bench:bench_prefix_lock", 4.0},
        {"bench:bench_multimod", 5.0},
        {"bench:bench_blocker", 2.0},
        {"bench:bench_finish_prefix", 6.0},
        {"bench:bench_finish_suffix", 7.0},
        {"scour", 1.0}};
    technique_options.economy = technique_economy;
    technique_options.review_projection_json = R"JSON({
      "schema_version":"solver_review_projection_v1",
      "raw_strategy":{"execution_authority":"raw_strategy_only"},
      "sections":[
        {"id":"setup","label":"Setup","role":"setup","raw_references":[{"node_id":"start"},{"edge_id":"begin"}]},
        {"id":"blocking","label":"Temporary block","role":"temporary_blocking","raw_references":[{"node_id":"blocker"},{"node_id":"blocker_cleanup"},{"edge_id":"blocker_used"},{"edge_id":"blocker_cleaned"}]},
        {"id":"protection","label":"Protection","role":"protection","raw_references":[{"node_id":"protect_once"},{"node_id":"protect_cleanup_once"},{"node_id":"protect_again"},{"node_id":"protect_cleanup_again"},{"edge_id":"protected_once"},{"edge_id":"reapply"},{"edge_id":"protected_again"},{"edge_id":"protection_cleaned"}]},
        {"id":"finishing","label":"Multimod finish","role":"finishing","raw_references":[{"node_id":"multimod"},{"node_id":"finish_one"},{"node_id":"finish_two"},{"edge_id":"multimod_ready"},{"edge_id":"finish_one_done"},{"edge_id":"finished"}]},
        {"id":"success","label":"Success","role":"finishing","raw_references":[{"node_id":"success"}]}
      ]
    })JSON";
    const StrategyEvalResult technique_accounting =
        evaluate_strategy(*technique_strategy, technique_options);
    PC_CHECK(near(technique_accounting.expected_actions, 9.0));
    PC_CHECK(near(technique_accounting.total_expected_cost, 31.0));
    PC_CHECK(near(
        technique_accounting.technique_totals.at(
            "temporary_blocker_applications"),
        1.0));
    PC_CHECK(near(
        technique_accounting.technique_totals.at("protection_setup_actions"),
        2.0));
    PC_CHECK(near(
        technique_accounting.technique_totals.at(
            "protection_reapplications"),
        1.0));
    PC_CHECK(near(
        technique_accounting.technique_totals.at("retry_count"), 1.0));
    PC_CHECK(near(
        technique_accounting.technique_totals.at("multimod_setup_actions"),
        1.0));
    PC_CHECK(near(
        technique_accounting.technique_totals.at(
            "multimod_finishing_bench_actions"),
        2.0));
    PC_CHECK(near(
        technique_accounting.technique_totals.at(
            "permanent_goal_bench_finishes"),
        2.0));
    PC_CHECK(near(
        technique_accounting.technique_totals.at(
            "deterministic_finishing_actions"),
        2.0));
    PC_CHECK(near(
        technique_accounting.technique_totals.at(
            "crafted_mod_cleanup_or_replacement_actions"),
        3.0));
    const StrategyEvalActionTotal* repeated_protection =
        action_total(technique_accounting, "bench:bench_prefix_lock");
    PC_CHECK(repeated_protection != nullptr);
    PC_CHECK(repeated_protection != nullptr &&
             near(repeated_protection->expected_visits, 2.0));
    PC_CHECK(repeated_protection != nullptr &&
             repeated_protection->nodes.size() == 2);
    PC_CHECK(repeated_protection != nullptr &&
             repeated_protection->reachable_states > 0);
    PC_CHECK(repeated_protection != nullptr &&
             !repeated_protection->regions.empty());
    const StrategyEvalActionTotal* shared_cleanup = action_total(
        technique_accounting, "remove_crafted_modifiers");
    PC_CHECK(shared_cleanup != nullptr &&
             near(shared_cleanup->expected_visits, 3.0));
    PC_CHECK(shared_cleanup != nullptr && shared_cleanup->nodes.size() == 3);
    const StrategyEvalActionTotal* finish_prefix = action_total(
        technique_accounting, "bench:bench_finish_prefix");
    const StrategyEvalActionTotal* finish_suffix = action_total(
        technique_accounting, "bench:bench_finish_suffix");
    PC_CHECK(finish_prefix != nullptr && finish_suffix != nullptr);
    PC_CHECK(finish_prefix != nullptr &&
             near(finish_prefix->expected_visits, 1.0));
    PC_CHECK(finish_suffix != nullptr &&
             near(finish_suffix->expected_visits, 1.0));
    PC_CHECK(near(technique_accounting.section_actions_difference, 0.0));
    PC_CHECK(technique_accounting.review_sections.size() == 5);
    PC_CHECK(near(
        technique_accounting.review_sections[1].expected_actions, 2.0));
    PC_CHECK(near(
        technique_accounting.review_sections[2].expected_actions, 4.0));
    PC_CHECK(near(
        technique_accounting.review_sections[3].expected_actions, 3.0));
    for (const auto& [unused, difference] :
         technique_accounting.section_material_differences) {
        (void)unused;
        PC_CHECK(near(difference, 0.0, 1e-12));
    }

    const std::string bytes = serialize_strategy_eval(exact);
    const std::string accounting_json =
        serialize_strategy_eval(technique_accounting);
    PC_CHECK(accounting_json.find("\"reachable_regions\":") !=
             std::string::npos);
    PC_CHECK(accounting_json.find("\"probability_of_any_use\":") !=
             std::string::npos);
    PC_CHECK(accounting_json.find("\"known_cost_share\":") !=
             std::string::npos);
    PC_CHECK(accounting_json.find("\"goal_progress\":") !=
             std::string::npos);
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
    CountObservation family_count_observation;
    family_count_observation.by_family = true;
    family_count_observation.ids = {100};
    CountObservation mod_count_observation;
    mod_count_observation.ids = {6};
    CalcContext count_calc(
        session, goal, registry, actions,
        false, /* non-empty goal */
        true,  /* normal candidate default */
        true,  /* exact group-exclusion effects */
        std::nullopt,
        {family_count_observation, mod_count_observation});

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
    CompiledCondition mod_count;
    mod_count.kind = ConditionKind::ModCount;
    mod_count.mod_ids = {6};
    mod_count.min_value = 0;
    mod_count.max_value = 1;
    CompiledCondition family_count;
    family_count.kind = ConditionKind::ModFamilyCount;
    family_count.family_ids = {100};
    family_count.min_value = 1;
    family_count.max_value = 1;
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

    pc_item_state counted_item;
    pc_item_clear(&counted_item);
    counted_item.rarity = PC_RARITY_RARE;
    counted_item.prefix_count = 1;
    counted_item.prefixes[0].mod_id = 0; /* family 100 goal member */
    counted_item.suffix_count = 1;
    counted_item.suffixes[0].mod_id = 6; /* observed junk member */
    counted_item.suffixes[0].flags = PC_MOD_SLOT_CRAFTED;
    const std::uint32_t counted_state = count_calc.intern_item(counted_item);
    for (CompiledCondition condition : {mod_count, family_count}) {
        PC_CHECK(evaluate_abstract_condition(
                     condition, *session, count_calc.layout(),
                     count_calc.state(counted_state)) ==
                 evaluate_compiled_condition(
                     condition, *session, counted_item));
        condition.required_flags = PC_MOD_SLOT_CRAFTED;
        PC_CHECK(evaluate_abstract_condition(
                     condition, *session, count_calc.layout(),
                     count_calc.state(counted_state)) ==
                 evaluate_compiled_condition(
                     condition, *session, counted_item));
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
    check_reference_parity(*strategy, exact, options);

    SimulatorImpl simulator;
    const std::uint64_t runs = 10000;
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
    check_reference_parity(*illegal, illegal_exact);
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
    check_reference_parity(*no_edge, no_edge_exact);
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
    check_reference_parity(*ordering, ordering_exact);
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
    /* Make the special descriptor families present for this preflight-only
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
    PC_CHECK(unresolved.sweeps == 0);

    const auto router_cycle = compile(
        session,
        shell(
            "router cycle", "normal",
            R"JSON({"id":"start","kind":"start"},{"id":"a","kind":"router"},{"id":"b","kind":"router"})JSON",
            R"JSON({"id":"enter","from":"start","to":"a","priority":0,"condition":{"type":"always"}},
{"id":"a_to_b","from":"a","to":"b","priority":0,"condition":{"type":"always"}},
{"id":"b_to_a","from":"b","to":"a","priority":0,"condition":{"type":"always"}})JSON"));
    const StrategyEvalResult router_unresolved =
        evaluate_strategy(*router_cycle);
    PC_CHECK(!router_unresolved.converged);
    PC_CHECK(near(router_unresolved.residual_mass, 1.0));
    PC_CHECK(router_unresolved.sweeps == 0);
    PC_CHECK(router_unresolved.unresolved_by_node.size() == 1);
    PC_CHECK(router_unresolved.unresolved_by_node[0].node_id == "a");
    PC_CHECK(near(router_unresolved.unresolved_by_node[0].mass, 1.0));
}

void run_scale_and_fallback_tests() {
    auto session = make_eval_session();
    std::ostringstream nodes;
    nodes << R"JSON({"id":"start","kind":"start"},
{"id":"chaos","kind":"operation","operation":{"type":"chaos","params":{}}},
{"id":"success","kind":"terminal","terminal":"success"})JSON";
    constexpr int kRouterCount = 65;
    for (int i = 0; i < kRouterCount; ++i) {
        nodes << ",{\"id\":\"router_" << i
              << "\",\"kind\":\"router\"}";
    }
    std::ostringstream edges;
    edges << R"JSON({"id":"begin","from":"start","to":"chaos","priority":0,"condition":{"type":"always"}},
{"id":"hit","from":"chaos","to":"success","priority":0,"condition":{"type":"has_mod_family","family_mod_key":"mod0","min_tier":1}},
{"id":"miss","from":"chaos","to":"router_0","priority":999,"is_default":true})JSON";
    for (int i = 0; i < kRouterCount; ++i) {
        edges << ",{\"id\":\"route_" << i << "\",\"from\":\"router_"
              << i << "\",\"to\":\""
              << (i + 1 == kRouterCount
                      ? std::string("chaos")
                      : "router_" + std::to_string(i + 1))
              << "\",\"priority\":0,\"condition\":{\"type\":\"always\"}}";
    }
    const auto large = compile(
        session, shell("large fallback", "rare", nodes.str(), edges.str()));
    StrategyEvalOptions options;
    options.epsilon = 1e-10;
    const StrategyEvalResult result = evaluate_strategy(*large, options);
    PC_CHECK(result.converged);
    PC_CHECK(result.sweeps == 0);
    PC_CHECK(std::fabs(result.success_probability - 1.0) < 1e-9);
    PC_CHECK(result.max_mass_conservation_error < 1e-9);
    check_reference_parity(*large, result, options);

    const auto pair_guard = compile(
        session,
        shell(
            "pair guard", "rare",
            R"JSON({"id":"start","kind":"start"},
{"id":"restart","kind":"operation","operation":{"type":"restart","params":{}}},
{"id":"success","kind":"terminal","terminal":"success"})JSON",
            R"JSON({"id":"begin","from":"start","to":"restart","priority":0,"condition":{"type":"always"}},
{"id":"done","from":"restart","to":"success","priority":0,"condition":{"type":"always"}})JSON"));
    StrategyEvalOptions guarded;
    guarded.max_pairs = 1;
    bool failed = false;
    try {
        (void)evaluate_strategy(*pair_guard, guarded);
    } catch (const std::length_error& ex) {
        failed = std::string(ex.what()).find("max_pairs") != std::string::npos;
    }
    PC_CHECK(failed);

    StrategyEvalOptions state_guard;
    state_guard.max_states = 1;
    failed = false;
    try {
        (void)evaluate_strategy(*pair_guard, state_guard);
    } catch (const std::length_error& ex) {
        failed = std::string(ex.what()).find("max_states") != std::string::npos;
    }
    PC_CHECK(failed);

    StrategyEvalOptions transition_guard;
    transition_guard.max_transitions = 1;
    failed = false;
    try {
        (void)evaluate_strategy(*pair_guard, transition_guard);
    } catch (const std::length_error& ex) {
        failed =
            std::string(ex.what()).find("max_transitions") != std::string::npos;
    }
    PC_CHECK(failed);

    StrategyEvalOptions memory_guard;
    memory_guard.max_owned_bytes = 1;
    failed = false;
    try {
        (void)evaluate_strategy(*pair_guard, memory_guard);
    } catch (const std::length_error& ex) {
        failed =
            std::string(ex.what()).find("max_owned_bytes") != std::string::npos;
    }
    PC_CHECK(failed);
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
    const std::size_t unpriced_length = length;
    PC_CHECK(json.find("\"version\":\"v1\"") != std::string::npos);
    PC_CHECK(json.find("\"success\":1") != std::string::npos);
    PC_CHECK(json.find("\"key\":\"base\"") != std::string::npos);
    PC_CHECK(json.find("\"accounting\":{") != std::string::npos);
    PC_CHECK(json.find("\"action_descriptor_visits_difference\":0") !=
             std::string::npos);

    pc_economy economy;
    economy.impl = std::make_shared<EconomyImpl>();
    economy.impl->id = "c-abi-accounting";
    economy.impl->prices = {{"base", 5.0}};
    pc_strategy_eval_options priced_options{};
    priced_options.struct_size = sizeof(priced_options);
    priced_options.abi_version = PC_ABI_VERSION;
    priced_options.economy = &economy;
    length = 0;
    PC_CHECK(pc_strategy_evaluate(
                 &strategy, &priced_options, nullptr, 0, &length,
                 &error) == PC_RESULT_OK);
    std::string priced_json(length + 1, '\0');
    PC_CHECK(pc_strategy_evaluate(
                 &strategy, &priced_options, priced_json.data(),
                 priced_json.size(), &length, &error) == PC_RESULT_OK);
    priced_json.resize(length);
    PC_CHECK(priced_json.find("\"economy_id\":\"c-abi-accounting\"") !=
             std::string::npos);
    PC_CHECK(priced_json.find("\"total_expected_cost\":5") !=
             std::string::npos);

    pc_strategy_eval_work_handle work = nullptr;
    PC_CHECK(pc_strategy_eval_begin(
                 &strategy, nullptr, &work, &error) == PC_RESULT_OK);
    PC_CHECK(work != nullptr);
    pc_strategy_eval_progress progress{};
    std::vector<int32_t> phases;
    do {
        PC_CHECK(pc_strategy_eval_step(
                     work, 1, &progress, &error) == PC_RESULT_OK);
        phases.push_back(progress.phase);
    } while (!progress.done);
    PC_CHECK(!phases.empty());
    PC_CHECK(phases.front() == PC_STRATEGY_EVAL_PHASE_DISCOVERY);
    PC_CHECK(phases.back() == PC_STRATEGY_EVAL_PHASE_DONE);
    pc_native_memory_stats memory_before_finish{};
    PC_CHECK(pc_strategy_eval_memory_stats(
                 work, &memory_before_finish, &error) == PC_RESULT_OK);
    PC_CHECK(memory_before_finish.live_owned_bytes > 0);
    PC_CHECK(memory_before_finish.peak_owned_bytes >=
             memory_before_finish.live_owned_bytes);
    std::size_t stepped_length = 0;
    PC_CHECK(pc_strategy_eval_finish(
                 work, nullptr, 0, &stepped_length, &error) == PC_RESULT_OK);
    std::string stepped(stepped_length + 1, '\0');
    PC_CHECK(pc_strategy_eval_finish(
                 work, stepped.data(), stepped.size(), &stepped_length,
                 &error) == PC_RESULT_OK);
    stepped.resize(stepped_length);
    json.resize(unpriced_length);
    PC_CHECK(stepped == json);
    pc_native_memory_stats memory_after_finish{};
    PC_CHECK(pc_strategy_eval_memory_stats(
                 work, &memory_after_finish, &error) == PC_RESULT_OK);
    PC_CHECK(memory_after_finish.serialized_output_bytes >= stepped_length);
    PC_CHECK(memory_after_finish.live_owned_bytes >=
             memory_before_finish.live_owned_bytes);
    pc_strategy_eval_destroy(work);

    pc_strategy_eval_options output_limit{};
    output_limit.struct_size = sizeof(output_limit);
    output_limit.abi_version = PC_ABI_VERSION;
    output_limit.max_output_json_bytes = 64;
    length = 0;
    PC_CHECK(pc_strategy_evaluate(
                 &strategy, &output_limit, nullptr, 0, &length, &error) ==
             PC_RESULT_CAPACITY_EXCEEDED);
    PC_CHECK(
        std::string(error.message).find("max_output_json_bytes") !=
        std::string::npos);

    pc_strategy_eval_options pair_limit{};
    pair_limit.struct_size = sizeof(pair_limit);
    pair_limit.abi_version = PC_ABI_VERSION;
    pair_limit.max_pairs = 1;
    work = nullptr;
    PC_CHECK(pc_strategy_eval_begin(
                 &strategy, &pair_limit, &work, &error) == PC_RESULT_OK);
    pc_result step_result = PC_RESULT_OK;
    progress = {};
    while (step_result == PC_RESULT_OK && !progress.done) {
        step_result = pc_strategy_eval_step(work, 1, &progress, &error);
    }
    PC_CHECK(step_result == PC_RESULT_CAPACITY_EXCEEDED);
    PC_CHECK(std::string(error.message).find("max_pairs") != std::string::npos);
    pc_strategy_eval_destroy(work);

    pc_strategy_eval_options state_limit{};
    state_limit.struct_size = sizeof(state_limit);
    state_limit.abi_version = PC_ABI_VERSION;
    state_limit.max_states = 1;
    work = nullptr;
    PC_CHECK(pc_strategy_eval_begin(
                 &strategy, &state_limit, &work, &error) == PC_RESULT_OK);
    step_result = PC_RESULT_OK;
    progress = {};
    while (step_result == PC_RESULT_OK && !progress.done) {
        step_result = pc_strategy_eval_step(work, 1, &progress, &error);
    }
    PC_CHECK(step_result == PC_RESULT_CAPACITY_EXCEEDED);
    PC_CHECK(std::string(error.message).find("max_states") != std::string::npos);
    PC_CHECK(std::string(error.message).find("bad_alloc") == std::string::npos);
    pc_strategy_eval_destroy(work);

    pc_strategy_eval_options transition_limit{};
    transition_limit.struct_size = sizeof(transition_limit);
    transition_limit.abi_version = PC_ABI_VERSION;
    transition_limit.max_transitions = 1;
    work = nullptr;
    PC_CHECK(pc_strategy_eval_begin(
                 &strategy, &transition_limit, &work, &error) == PC_RESULT_OK);
    step_result = PC_RESULT_OK;
    progress = {};
    while (step_result == PC_RESULT_OK && !progress.done) {
        step_result = pc_strategy_eval_step(work, 1, &progress, &error);
    }
    PC_CHECK(step_result == PC_RESULT_CAPACITY_EXCEEDED);
    PC_CHECK(
        std::string(error.message).find("max_transitions") !=
        std::string::npos);
    PC_CHECK(std::string(error.message).find("bad_alloc") == std::string::npos);
    pc_strategy_eval_destroy(work);

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
    case ActionType::RemoveCraftedModifiers:
        return "remove_crafted_modifiers";
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
    stage("scale and fallback", [&] { run_scale_and_fallback_tests(); });
    stage("c abi", [&] { run_c_abi_tests(); });
    stage("artifact", [&] { run_artifact_and_registry_tests(artifact_dir); });
}
