#include "tests.hpp"

#include "../src/handles_internal.hpp"
#include "../src/solver_internal.hpp"
#include "../src/solver_refinement.hpp"
#include "../src/solver_segmented_vector.hpp"
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

void run_segmented_vector_tests() {
    solve_detail::SegmentedVector<std::uint32_t, 4> values;
    PC_CHECK(values.empty());
    PC_CHECK(values.owned_bytes() == 0);
    for (std::uint32_t value = 0; value < 10; ++value) {
        values.push_back(value * 3);
    }
    PC_CHECK(values.size() == 10);
    PC_CHECK(values.capacity() == 12);
    PC_CHECK(values.owned_bytes() >= 12 * sizeof(std::uint32_t));
    for (std::uint32_t value = 0; value < 10; ++value) {
        PC_CHECK(values.at(value) == value * 3);
    }
    bool rejected = false;
    try {
        (void)values.at(10);
    } catch (const std::out_of_range&) {
        rejected = true;
    }
    PC_CHECK(rejected);

    solve_detail::SegmentedVector<std::uint32_t, 4> moved =
        std::move(values);
    PC_CHECK(values.empty());
    PC_CHECK(moved.size() == 10);
    PC_CHECK(moved.at(9) == 27);
    moved.assign(7, 42);
    PC_CHECK(moved.size() == 7);
    PC_CHECK(moved.capacity() == 8);
    for (std::size_t index = 0; index < moved.size(); ++index) {
        PC_CHECK(moved[index] == 42);
    }
    moved.release();
    PC_CHECK(moved.empty());
    PC_CHECK(moved.owned_bytes() == 0);
}

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

const StrategyEvalNode* node_result(
    const StrategyEvalResult& result,
    const std::string& id) {
    const auto found = std::find_if(
        result.nodes.begin(), result.nodes.end(),
        [&](const StrategyEvalNode& node) { return node.id == id; });
    return found == result.nodes.end() ? nullptr : &*found;
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

std::string affix_observation_signature_condition(
        const RefinementFeature feature,
        const std::string& value,
        const std::uint32_t count_observation_count = 0,
        const std::string&
            count_observation_membership_by_mod = "[]") {
    const RefinementFeatureMask features =
        refinement_feature(feature);
    return
        std::string{
            "{\"type\":\"observation_signature\",\"version\":1,"
            "\"requirement\":{\"item_features\":0,"
            "\"modifier_tag_ids\":[],\"affix_observations\":["
            "{\"features\":"} +
        std::to_string(features) +
        ",\"selector\":{\"required_affix_traits\":0,"
        "\"forbidden_affix_traits\":0,\"required_item_traits\":0,"
        "\"forbidden_item_traits\":0,\"required_tag_ids\":[]}}]},"
        "\"signature\":[{\"feature\":" +
        std::to_string(static_cast<std::uint8_t>(feature)) +
        ",\"subject\":0,\"value\":[\"" + value +
        "\"]}],\"goal_status_tier_class_by_mod\":[],"
        "\"count_observation_count\":" +
        std::to_string(count_observation_count) +
        ",\"count_observation_membership_by_mod\":" +
        count_observation_membership_by_mod + "}";
}

std::string observation_router_strategy(
        const std::string& name,
        const std::string& prefix_mod,
        const std::string& routing_edges) {
    return shell(
        name, "rare",
        R"JSON({"id":"start","kind":"start"},
{"id":"success","kind":"terminal","terminal":"success"},
{"id":"failure","kind":"terminal","terminal":"failure"})JSON",
        routing_edges,
        ",\"prefixes\":[\"" + prefix_mod + "\"]");
}

void expect_observation_program_refusal(
        const std::shared_ptr<const SessionImpl>& session,
        const std::string& condition,
        const std::string& expected) {
    const std::string strategy = observation_router_strategy(
        "invalid observation program", "mod0",
        std::string{
            "{\"id\":\"route\",\"from\":\"start\","
            "\"to\":\"success\",\"priority\":0,\"condition\":"} +
            condition +
            "},{\"id\":\"miss\",\"from\":\"start\","
            "\"to\":\"failure\",\"priority\":999,"
            "\"is_default\":true}");
    bool refused = false;
    try {
        (void)compile(session, strategy);
    } catch (const std::exception& error) {
        refused =
            std::string(error.what()).find(expected) !=
            std::string::npos;
    }
    PC_CHECK(refused);
}

std::string replace_once(
        std::string value,
        const std::string& before,
        const std::string& after) {
    const std::size_t position = value.find(before);
    PC_CHECK(position != std::string::npos);
    if (position != std::string::npos) {
        value.replace(position, before.size(), after);
    }
    return value;
}

pc_item_state observation_item(
        const SessionImpl& session,
        const std::uint32_t mod) {
    pc_item_state item;
    pc_item_clear(&item);
    item.rarity = PC_RARITY_RARE;
    PC_CHECK(
        pc_item_add_mod(
            &item, PC_SIDE_PREFIX, mod,
            session.primary_group.at(mod), 0, nullptr) ==
        PC_RESULT_OK);
    return item;
}

void run_observation_signature_condition_tests() {
    auto session = make_eval_session();
    const std::string exclusion_g10 =
        affix_observation_signature_condition(
            RefinementFeature::ModifierExclusionSignature,
            "0000000000000003");
    const std::string exclusion_g11 =
        affix_observation_signature_condition(
            RefinementFeature::ModifierExclusionSignature,
            "0000000000000004");

    /* Modifier ids 0 and 1 are distinct raw ids but have the same complete
     * group-exclusion effect. The shared observer must route them
     * identically, while mod 2 remains distinguishable. */
    const auto equal_strategy = compile(
        session,
        observation_router_strategy(
            "equal semantic exclusion signature", "mod0",
            std::string{
                "{\"id\":\"equal\",\"from\":\"start\","
                "\"to\":\"success\",\"priority\":0,\"condition\":"} +
                exclusion_g10 +
                "},{\"id\":\"miss\",\"from\":\"start\","
                "\"to\":\"failure\",\"priority\":999,"
                "\"is_default\":true}"));
    const CompiledCondition& equal_condition =
        equal_strategy->nodes.at(equal_strategy->start_node)
            .edges.front().condition;
    PC_CHECK(evaluate_compiled_condition(
        equal_condition, *session, observation_item(*session, 0)));
    PC_CHECK(evaluate_compiled_condition(
        equal_condition, *session, observation_item(*session, 1)));
    PC_CHECK(!evaluate_compiled_condition(
        equal_condition, *session, observation_item(*session, 2)));

    for (const char* const mod : {"mod0", "mod1"}) {
        const auto strategy = compile(
            session,
            observation_router_strategy(
                std::string{"equivalent raw modifier route "} + mod,
                mod,
                std::string{
                    "{\"id\":\"equal\",\"from\":\"start\","
                    "\"to\":\"success\",\"priority\":0,"
                    "\"condition\":"} +
                    exclusion_g10 +
                    "},{\"id\":\"miss\",\"from\":\"start\","
                    "\"to\":\"failure\",\"priority\":999,"
                    "\"is_default\":true}"));
        const StrategyEvalResult exact =
            evaluate_strategy(*strategy);
        const SimulationSummaryInternal simulated =
            simulate(session, strategy, 64, 20260731);
        PC_CHECK(exact.converged);
        PC_CHECK(near(exact.success_probability, 1.0));
        PC_CHECK(near(exact.failure_probability, 0.0));
        PC_CHECK(simulated.completed_runs == 64);
        PC_CHECK(simulated.success_count == 64);
        PC_CHECK(simulated.failure_count == 0);
        check_reference_parity(*strategy, exact);
    }

    const auto separated_strategy = compile(
        session,
        observation_router_strategy(
            "different semantic exclusion signature", "mod2",
            std::string{
                "{\"id\":\"wrong\",\"from\":\"start\","
                "\"to\":\"failure\",\"priority\":0,\"condition\":"} +
                exclusion_g10 +
                "},{\"id\":\"right\",\"from\":\"start\","
                "\"to\":\"success\",\"priority\":1,\"condition\":" +
                exclusion_g11 +
                "},{\"id\":\"miss\",\"from\":\"start\","
                "\"to\":\"failure\",\"priority\":999,"
                "\"is_default\":true}"));
    const StrategyEvalResult separated_exact =
        evaluate_strategy(*separated_strategy);
    const SimulationSummaryInternal separated_simulated =
        simulate(session, separated_strategy, 64, 20260732);
    PC_CHECK(separated_exact.converged);
    PC_CHECK(near(separated_exact.success_probability, 1.0));
    PC_CHECK(near(separated_exact.failure_probability, 0.0));
    PC_CHECK(separated_simulated.success_count == 64);
    PC_CHECK(separated_simulated.failure_count == 0);
    PC_CHECK(near(
        edge_value(separated_exact, "wrong"), 0.0));
    PC_CHECK(near(
        edge_value(separated_exact, "right"), 1.0));
    check_reference_parity(
        *separated_strategy, separated_exact);

    /* An omitted all-zero membership entry is normalized to the declared
     * observation width, not to an empty vector. This is the exact parser,
     * simulator, and evaluator parity guard for sparse count context. */
    const std::string zero_count_membership =
        affix_observation_signature_condition(
            RefinementFeature::CountObservationMembership,
            "0000000000000000", 1,
            R"JSON([{"mod_key":"mod0","value":["0000000000000001"]}])JSON");
    const auto count_strategy = compile(
        session,
        observation_router_strategy(
            "all-zero count membership normalization", "mod1",
            std::string{
                "{\"id\":\"zero\",\"from\":\"start\","
                "\"to\":\"success\",\"priority\":0,\"condition\":"} +
                zero_count_membership +
                "},{\"id\":\"miss\",\"from\":\"start\","
                "\"to\":\"failure\",\"priority\":999,"
                "\"is_default\":true}"));
    const StrategyEvalResult count_exact =
        evaluate_strategy(*count_strategy);
    const SimulationSummaryInternal count_simulated =
        simulate(session, count_strategy, 64, 20260733);
    PC_CHECK(count_exact.converged);
    PC_CHECK(near(count_exact.success_probability, 1.0));
    PC_CHECK(count_simulated.success_count == 64);
    check_reference_parity(*count_strategy, count_exact);

    /* The v1 program is closed and canonical at admission. Overflow is
     * checked before narrowing into its u16/u8 selector fields. */
    expect_observation_program_refusal(
        session,
        replace_once(
            exclusion_g10, "\"version\":1", "\"version\":2"),
        "unsupported observation_signature version");
    expect_observation_program_refusal(
        session,
        replace_once(
            exclusion_g10, "\"modifier_tag_ids\":[]",
            "\"modifier_tag_ids\":[2,1]"),
        "must be sorted and unique");
    expect_observation_program_refusal(
        session,
        replace_once(
            exclusion_g10,
            "\"required_affix_traits\":0",
            "\"required_affix_traits\":65536"),
        "selector traits are invalid");
    expect_observation_program_refusal(
        session,
        replace_once(
            exclusion_g10,
            "\"required_item_traits\":0",
            "\"required_item_traits\":256"),
        "selector traits are invalid");

    const std::string duplicate_signature = replace_once(
        exclusion_g10,
        "\"signature\":[{\"feature\":21,\"subject\":0,"
        "\"value\":[\"0000000000000003\"]}]",
        "\"signature\":[{\"feature\":21,\"subject\":0,"
        "\"value\":[\"0000000000000003\"]},"
        "{\"feature\":21,\"subject\":0,"
        "\"value\":[\"0000000000000003\"]}]");
    expect_observation_program_refusal(
        session, duplicate_signature,
        "signature is not canonical");

    const std::string duplicate_requirement = replace_once(
        exclusion_g10,
        "\"affix_observations\":[{\"features\":2097152,"
        "\"selector\":{\"required_affix_traits\":0,"
        "\"forbidden_affix_traits\":0,\"required_item_traits\":0,"
        "\"forbidden_item_traits\":0,\"required_tag_ids\":[]}}]",
        "\"affix_observations\":[{\"features\":2097152,"
        "\"selector\":{\"required_affix_traits\":0,"
        "\"forbidden_affix_traits\":0,\"required_item_traits\":0,"
        "\"forbidden_item_traits\":0,\"required_tag_ids\":[]}},"
        "{\"features\":134217728,\"selector\":{"
        "\"required_affix_traits\":0,"
        "\"forbidden_affix_traits\":0,\"required_item_traits\":0,"
        "\"forbidden_item_traits\":0,\"required_tag_ids\":[]}}]");
    expect_observation_program_refusal(
        session, duplicate_requirement,
        "requirement is not canonical");

    const std::string malformed_count_context = replace_once(
        zero_count_membership,
        "\"value\":[\"0000000000000001\"]",
        "\"value\":[]");
    expect_observation_program_refusal(
        session, malformed_count_context,
        "invalid semantic membership vector");
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
    StrategyEvalOptions raw_family_options = options;
    raw_family_options.use_exact_exchangeable_family_compression = false;
    const StrategyEvalResult raw_family_exact =
        evaluate_strategy(*loop_strategy, raw_family_options);
    PC_CHECK(exact.converged);
    PC_CHECK(raw_family_exact.converged);
    const StrategyEvalOperationRowCensus& compressed_census =
        exact.operation_row_census;
    PC_CHECK(compressed_census.materialized_rows > 0);
    PC_CHECK(compressed_census.replayable_rows > 0);
    PC_CHECK(compressed_census.stable_shared_rows > 0);
    PC_CHECK(compressed_census.unique_stable_kernels > 0);
    PC_CHECK(compressed_census.direct_repeat_rows > 0);
    PC_CHECK(compressed_census.local_gated_route_rows == 0);
    PC_CHECK(compressed_census.goal_progress_gated_rows > 0);
    PC_CHECK(compressed_census.full_physical_rows == 0);
    PC_CHECK(compressed_census.exact_outcome_entries > 0);
    PC_CHECK(compressed_census.routed_transitions > 0);
    PC_CHECK(compressed_census.exact_outcome_payload_bytes > 0);
    PC_CHECK(compressed_census.unique_stable_kernel_payload_bytes > 0);
    PC_CHECK(compressed_census.routed_payload_bytes > 0);
    PC_CHECK(compressed_census.replay_route_token_bytes > 0);
    PC_CHECK(compressed_census.replay_route_result_authorities > 0);
    PC_CHECK(compressed_census.compact_attribution_rows > 0);
    PC_CHECK(compressed_census.compact_attribution_edges > 0);
    PC_CHECK(
        compressed_census.replay_route_token_bytes <=
        compressed_census.projected_u32_route_tokens_bytes);
    PC_CHECK(
        compressed_census.projected_u32_route_tokens_bytes ==
        compressed_census.exact_outcome_entries *
            sizeof(std::uint32_t));
    /* Both runs are independent exact evaluations. The default evaluator may
     * compress only proved exchangeable junk families; disabling that proof
     * reconstructs the physical family frontier. Raw probability-bit hashes
     * retain their distinct summation paths; the compiler's witness/runtime
     * contract checks each path bitwise elsewhere. Here require all public
     * mass, cost, and edge semantics to agree at the evaluator tolerance while
     * proving the compressed work path is actually exercised. */
    PC_CHECK(exact.reforge_gated_first_kernel_bits_hash != 0);
    PC_CHECK(raw_family_exact.reforge_gated_first_kernel_bits_hash != 0);
    PC_CHECK(
        exact.reforge_gated_first_kernel_bits_hash !=
        raw_family_exact.reforge_gated_first_kernel_bits_hash);
    std::printf(
        "solver eval exchangeable-family A/B: compressed_hash=%016llx "
        "raw_hash=%016llx\n",
        static_cast<unsigned long long>(
            exact.reforge_gated_first_kernel_bits_hash),
        static_cast<unsigned long long>(
            raw_family_exact.reforge_gated_first_kernel_bits_hash));
    PC_CHECK(near(
        exact.success_probability,
        raw_family_exact.success_probability, 1e-12));
    PC_CHECK(near(
        exact.failure_probability,
        raw_family_exact.failure_probability, 1e-12));
    PC_CHECK(near(
        exact.stop_probability,
        raw_family_exact.stop_probability, 1e-12));
    PC_CHECK(near(
        exact.action_not_applied_probability,
        raw_family_exact.action_not_applied_probability, 1e-12));
    PC_CHECK(near(
        exact.no_matching_edge_probability,
        raw_family_exact.no_matching_edge_probability, 1e-12));
    PC_CHECK(near(
        exact.unresolved_probability,
        raw_family_exact.unresolved_probability, 1e-12));
    PC_CHECK(near(
        exact.expected_actions,
        raw_family_exact.expected_actions, 1e-12));
    PC_CHECK(near(
        exact.known_expected_cost,
        raw_family_exact.known_expected_cost, 1e-12));
    PC_CHECK(near(
        exact.total_expected_cost,
        raw_family_exact.total_expected_cost, 1e-12));
    PC_CHECK(near(
        exact.occupancy_expected_reward,
        raw_family_exact.occupancy_expected_reward, 1e-12));
    PC_CHECK(exact.cost_complete == raw_family_exact.cost_complete);
    PC_CHECK(
        exact.expected_consumption.size() ==
        raw_family_exact.expected_consumption.size());
    for (const auto& [key, quantity] : exact.expected_consumption) {
        PC_CHECK(near(
            quantity,
            raw_family_exact.expected_consumption.at(key), 1e-12));
    }
    PC_CHECK(exact.edges.size() == raw_family_exact.edges.size());
    for (const StrategyEvalEdge& edge : exact.edges) {
        PC_CHECK(near(
            edge.expected_traversals,
            edge_value(raw_family_exact, edge.id), 1e-12));
    }
    const double compressed_goal_probability =
        edge_value(exact, "hit") / exact.expected_actions;
    const double compressed_retry_probability =
        edge_value(exact, "repeat") / exact.expected_actions;
    const double raw_goal_probability =
        edge_value(raw_family_exact, "hit") /
        raw_family_exact.expected_actions;
    const double raw_retry_probability =
        edge_value(raw_family_exact, "repeat") /
        raw_family_exact.expected_actions;
    PC_CHECK(near(
        compressed_goal_probability, raw_goal_probability, 1e-12));
    PC_CHECK(near(
        compressed_retry_probability, raw_retry_probability, 1e-12));
    PC_CHECK(near(compressed_goal_probability, p, 1e-12));
    PC_CHECK(near(compressed_retry_probability, 1.0 - p, 1e-12));
    PC_CHECK(near(
        compressed_goal_probability + compressed_retry_probability,
        1.0, 1e-12));
    PC_CHECK(
        exact.reforge_effort.physical_families_built ==
        raw_family_exact.reforge_effort.physical_families_built);
    PC_CHECK(
        exact.reforge_effort.roll_buckets_built <
        raw_family_exact.reforge_effort.roll_buckets_built);
    PC_CHECK(
        exact.reforge_effort.frontier_nodes <
        raw_family_exact.reforge_effort.frontier_nodes);
    PC_CHECK(
        exact.reforge_logical_work_v1 <
        raw_family_exact.reforge_logical_work_v1);
    PC_CHECK(std::all_of(
        raw_family_exact.reforge_row_samples.begin(),
        raw_family_exact.reforge_row_samples.end(),
        [](const ReforgeRowTelemetry& row) {
            return row.owner == ReforgeRowOwner::ExactEvaluation;
        }));
    PC_CHECK(exact.reforge_work > 0);
    PC_CHECK(exact.reforge_logical_work_v1 > 0);
    PC_CHECK(exact.reforge_effort.rows_completed > 0);
    PC_CHECK(!exact.reforge_row_samples.empty());
    PC_CHECK(std::all_of(
        exact.reforge_row_samples.begin(),
        exact.reforge_row_samples.end(),
        [](const ReforgeRowTelemetry& row) {
            return row.owner == ReforgeRowOwner::ExactEvaluation;
        }));
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

    /* The general compiler uses this local router when the canonical
     * zero-progress retry basin selects a different policy region. Record the
     * exact structural candidate and the full physical row it continues to
     * use. Structure alone cannot grant compact-kernel numeric authority. */
    const auto local_gated_strategy = compile(
        session,
        shell(
            "local gated chaos loop", "rare",
            R"JSON({"id":"start","kind":"start"},
{"id":"policy_route_root","kind":"router"},
{"id":"chaos","kind":"operation","operation":{"type":"chaos","params":{}},"accounting_roles":["retry_action"]},
{"id":"chaos_gated_route","kind":"router"},
{"id":"success","kind":"terminal","terminal":"success"})JSON",
            R"JSON({"id":"begin","from":"start","to":"policy_route_root","priority":0,"condition":{"type":"always"}},
{"id":"policy_hit","from":"policy_route_root","to":"success","priority":0,"condition":{"type":"has_mod_family","family_mod_key":"mod0","min_tier":1}},
{"id":"policy_roll","from":"policy_route_root","to":"chaos","priority":999,"is_default":true},
{"id":"roll_route","from":"chaos","to":"chaos_gated_route","priority":999,"is_default":true},
{"id":"zero_progress","from":"chaos_gated_route","to":"chaos","priority":0,"condition":{"type":"not","conditions":[{"type":"has_mod_family","family_mod_key":"mod0","min_tier":1}]},"accounting_roles":["retry"]},
{"id":"made_progress","from":"chaos_gated_route","to":"policy_route_root","priority":999,"is_default":true})JSON"));
    StrategyEvalOptions local_gated_options = options;
    local_gated_options.review_projection_json.clear();
    const StrategyEvalResult local_gated_full =
        evaluate_strategy(
            *local_gated_strategy, local_gated_options);
    const StrategyEvalOperationRowCensus& local_gated_census =
        local_gated_full.operation_row_census;
    PC_CHECK(local_gated_census.local_gated_route_rows > 0);
    PC_CHECK(local_gated_census.local_gated_route_proved_rows ==
             local_gated_census.local_gated_route_rows);
    PC_CHECK(local_gated_full.converged);
    PC_CHECK(local_gated_full.cost_complete);
    PC_CHECK(near(
        local_gated_full.success_probability,
        exact.success_probability, 1e-12));
    PC_CHECK(near(
        local_gated_full.total_expected_cost,
        exact.total_expected_cost, 1e-10));
    PC_CHECK(
        local_gated_full.operation_row_census.goal_progress_gated_rows == 0);
    PC_CHECK(
        local_gated_full.operation_row_census
            .local_gated_full_outcome_entries > 0);
    const std::string local_gated_report =
        serialize_strategy_eval(local_gated_full);
    PC_CHECK(local_gated_report.find(
                 "\"local_gated_route_proof\":{\"proved_rows\":") !=
             std::string::npos);
    PC_CHECK(local_gated_report.find(
                 "\"local_gated_full_rows\":{\"outcome_entries\":") !=
             std::string::npos);
    check_reference_parity(
        *local_gated_strategy, local_gated_full, local_gated_options);

    /* A semantically equivalent but non-canonical condition is deliberately
     * rejected by the structural proof. Naming alone never grants authority
     * to collapse a physical distribution. */
    const auto rejected_local_gated_strategy = compile(
        session,
        shell(
            "rejected local gated chaos loop", "rare",
            R"JSON({"id":"start","kind":"start"},
{"id":"policy_route_root","kind":"router"},
{"id":"chaos","kind":"operation","operation":{"type":"chaos","params":{}}},
{"id":"chaos_gated_route","kind":"router"},
{"id":"success","kind":"terminal","terminal":"success"})JSON",
            R"JSON({"id":"begin","from":"start","to":"policy_route_root","priority":0,"condition":{"type":"always"}},
{"id":"policy_hit","from":"policy_route_root","to":"success","priority":0,"condition":{"type":"has_mod_family","family_mod_key":"mod0","min_tier":1}},
{"id":"policy_roll","from":"policy_route_root","to":"chaos","priority":999,"is_default":true},
{"id":"roll_route","from":"chaos","to":"chaos_gated_route","priority":999,"is_default":true},
{"id":"zero_progress","from":"chaos_gated_route","to":"chaos","priority":0,"condition":{"type":"not","conditions":[{"type":"not","conditions":[{"type":"not","conditions":[{"type":"has_mod_family","family_mod_key":"mod0","min_tier":1}]}]}]}},
{"id":"made_progress","from":"chaos_gated_route","to":"policy_route_root","priority":999,"is_default":true})JSON"));
    const StrategyEvalResult rejected_local_gated =
        evaluate_strategy(
            *rejected_local_gated_strategy, local_gated_options);
    PC_CHECK(rejected_local_gated.converged);
    PC_CHECK(near(
        rejected_local_gated.total_expected_cost,
        local_gated_full.total_expected_cost, 1e-10));
    PC_CHECK(
        rejected_local_gated.operation_row_census
            .local_gated_route_condition_rejections > 0);
    PC_CHECK(
        rejected_local_gated.operation_row_census
            .goal_progress_gated_rows == 0);
    PC_CHECK(
        rejected_local_gated.operation_row_census
            .local_gated_full_outcome_entries > 0);

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
    PC_CHECK(
        bytes.find("\"operation_row_census\":{") !=
        std::string::npos);
    PC_CHECK(
        bytes.find("\"projected_u32_route_tokens_bytes\":") !=
        std::string::npos);
    PC_CHECK(bytes.find(
                 "\"reforge_resource_accounting\":"
                 "{\"schema_version\":2") != std::string::npos);
    PC_CHECK(bytes.find("\"owner\":\"exact_evaluation\"") !=
             std::string::npos);
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

    /* Exact count routing may select one concrete member inside an occupied
     * goal tier-status partition. The goal class token, not the shared status
     * byte, supplies that contribution. */
    GoalSpec any_tier_goal;
    GoalSlot any_tier_family;
    any_tier_family.family_id = 100;
    any_tier_family.min_tier = 0;
    any_tier_goal.slots.push_back(any_tier_family);
    CountObservation exact_mod_observation;
    exact_mod_observation.ids = {0};
    CalcContext exact_goal_count(
        session, any_tier_goal, registry, actions,
        false, true, true, std::nullopt, {exact_mod_observation});
    CompiledCondition exact_mod_count;
    exact_mod_count.kind = ConditionKind::ModCount;
    exact_mod_count.mod_ids = {0};
    exact_mod_count.min_value = 1;
    exact_mod_count.max_value = 1;
    const auto exact_goal_state = [&](const std::uint32_t mod) {
        pc_item_state exact_item;
        pc_item_clear(&exact_item);
        exact_item.rarity = PC_RARITY_MAGIC;
        exact_item.prefix_count = 1;
        exact_item.prefixes[0].mod_id = mod;
        exact_item.prefixes[0].group_id = 10;
        return exact_goal_count.intern_item(exact_item);
    };
    const std::uint32_t exact_zero = exact_goal_state(0);
    const std::uint32_t exact_one = exact_goal_state(1);
    PC_CHECK(evaluate_abstract_condition(
        exact_mod_count, *session, exact_goal_count.layout(),
        exact_goal_count.state(exact_zero)));
    PC_CHECK(!evaluate_abstract_condition(
        exact_mod_count, *session, exact_goal_count.layout(),
        exact_goal_count.state(exact_one)));
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

void run_destructive_refinement_cycle_test() {
    auto session = make_eval_session();

    /*
     * Keep three effective prefix groups:
     *   g10 = mod0/mod1, g11 = mod2, g12 = mod3
     * and four suffix groups:
     *   g20..g23 = mod5..mod8.
     *
     * This produces 94 strict Chaos layouts. Of those, 52 non-satisfied,
     * non-full layouts reach Exalt. Exalt misses produce 22 exact layouts
     * that return to Chaos. A strict (node,state) evaluator therefore needs
     * 1 start + 52 Exalt + 23 Chaos = 76 pairs. Contract-guided destruction
     * collapse needs at most 60, so max_pairs=64 is a deliberate red/green
     * boundary.
     */
    pc_bitset_zero(
        session->normal_random_roll_mask.data(), session->words);
    pc_bitset_zero(
        session->positive_spawn_weight_mask.data(), session->words);
    pc_bitset_zero(
        session->positive_base_weight_mask.data(), session->words);
    pc_bitset_zero(
        session->influence_masks[0].data(), session->words);
    session->base_spawn_weight.assign(session->mod_count, 0);
    session->base_roll_weight.assign(session->mod_count, 0);
    for (const std::uint32_t mod :
         {0u, 1u, 2u, 3u, 5u, 6u, 7u, 8u}) {
        pc_bitset_set(session->normal_random_roll_mask.data(), mod);
        pc_bitset_set(session->positive_spawn_weight_mask.data(), mod);
        pc_bitset_set(session->positive_base_weight_mask.data(), mod);
        pc_bitset_set(session->influence_masks[0].data(), mod);
        session->base_spawn_weight[mod] = 100;
        session->base_roll_weight[mod] = 100;
    }

    const auto strategy = compile(
        session,
        shell(
            "strict destructive chaos exalt cycle", "rare",
            R"JSON({"id":"start","kind":"start"},
{"id":"chaos","kind":"operation","operation":{"type":"chaos","params":{}}},
{"id":"exalt","kind":"operation","operation":{"type":"exalt","params":{}}},
{"id":"success","kind":"terminal","terminal":"success"})JSON",
            R"JSON({"id":"begin","from":"start","to":"chaos","priority":0,"condition":{"type":"always"}},
{"id":"chaos_hit","from":"chaos","to":"success","priority":0,"condition":{"type":"has_mod_family","family_mod_key":"mod0","min_tier":1}},
{"id":"chaos_to_exalt","from":"chaos","to":"exalt","priority":1,"condition":{"type":"any","conditions":[{"type":"open_prefix_count","min":1,"max":3},{"type":"open_suffix_count","min":1,"max":3}]}},
{"id":"chaos_full_retry","from":"chaos","to":"chaos","priority":999,"is_default":true},
{"id":"exalt_hit","from":"exalt","to":"success","priority":0,"condition":{"type":"has_mod_family","family_mod_key":"mod0","min_tier":1}},
{"id":"exalt_retry","from":"exalt","to":"chaos","priority":999,"is_default":true})JSON"));

    StrategyEvalOptions options;
    options.epsilon = 1e-12;
    options.max_sweeps = 10000;
    options.max_states = 128;
    options.max_pairs = 64;
    options.max_transitions = 1024;
    options.max_owned_bytes = 64ull * 1024ull * 1024ull;

    StrategyEvalWork work(strategy, options);
    while (!work.progress().done) {
        work.step(1024);
    }
    const StrategyEvalProgress progress = work.progress();
    PC_CHECK(progress.pending_pairs == 0);

    const StrategyEvalResult& exact = work.result();
    const StrategyEvalResult& completed_diagnostic =
        work.diagnostic_result();
    PC_CHECK(completed_diagnostic.raw_pairs_discovered == 76);
    PC_CHECK(completed_diagnostic.refined_pairs == 57);
    PC_CHECK(exact.operation_row_census.materialized_rows > 0);
    PC_CHECK(exact.operation_row_census.state_local_rows > 0);
    PC_CHECK(exact.operation_row_census.exact_outcome_entries > 0);
    PC_CHECK(
        progress.discovered_pairs == exact.refined_pairs);
    PC_CHECK(exact.raw_pairs_discovered == 76);
    PC_CHECK(exact.refined_pairs == 57);
    PC_CHECK(exact.refined_pair_limit == options.max_pairs);
    PC_CHECK(exact.pair_discovery_index_peak_bytes > 0);
    PC_CHECK(exact.transition_via_owned_bytes > 0);
    using LegacyPairKey = std::tuple<
        std::uint32_t, std::uint32_t, std::uint32_t,
        std::uint32_t>;
    using LegacyPairIndexValue =
        std::pair<const LegacyPairKey, std::uint32_t>;
    const std::uint64_t legacy_pair_index_bytes =
        exact.raw_pairs_discovered *
        (sizeof(LegacyPairIndexValue) + 3 * sizeof(void*));
    /* The discovery link carrier allocates one fixed segment even for this
     * deliberately tiny graph. Bound that floor explicitly; large graphs
     * still retain four bytes per link instead of a tree node per key. */
    const std::uint64_t segmented_link_floor =
        solve_detail::SegmentedVector<std::uint32_t>::
            projected_owned_bytes(exact.raw_pairs_discovered);
    PC_CHECK(
        exact.pair_discovery_index_peak_bytes <=
        segmented_link_floor + legacy_pair_index_bytes);
    PC_CHECK(
        exact.boundary_subphase == StrategyEvalSubphase::Done);
    PC_CHECK(exact.stage_timings.model_setup_ns > 0);
    PC_CHECK(exact.stage_timings.observation_preparation_ns > 0);
    PC_CHECK(exact.stage_timings.pair_discovery_ns > 0);
    PC_CHECK(exact.stage_timings.pair_interning_ns > 0);
    PC_CHECK(exact.stage_timings.exact_kernel_ns > 0);
    PC_CHECK(exact.stage_timings.pair_refinement_ns > 0);
    PC_CHECK(exact.stage_timings.component_construction_ns > 0);
    PC_CHECK(exact.stage_timings.component_solve_ns > 0);
    PC_CHECK(exact.stage_timings.finalization_ns > 0);
    PC_CHECK(
        exact.stage_timings.pair_interning_ns <=
        exact.stage_timings.pair_discovery_ns);
    PC_CHECK(
        exact.stage_timings.exact_kernel_ns <=
        exact.stage_timings.pair_discovery_ns);
    PC_CHECK(
        exact.raw_pairs_discovered > exact.refined_pairs);
    PC_CHECK(exact.converged);
    PC_CHECK(exact.sweeps == 0);
    PC_CHECK(near(exact.success_probability, 1.0, 1e-10));
    PC_CHECK(near(exact.failure_probability, 0.0, 1e-12));
    PC_CHECK(near(exact.action_not_applied_probability, 0.0, 1e-12));
    PC_CHECK(near(exact.no_matching_edge_probability, 0.0, 1e-12));
    PC_CHECK(exact.max_mass_conservation_error < 1e-10);

    StrategyEvalWork single_step_work(strategy, options);
    while (!single_step_work.progress().done) {
        single_step_work.step(1);
    }
    const StrategyEvalResult single_step =
        single_step_work.take_result();
    PC_CHECK(single_step.raw_pairs_discovered ==
             exact.raw_pairs_discovered);
    PC_CHECK(single_step.refined_pairs == exact.refined_pairs);
    PC_CHECK(
        single_step.pair_discovery_index_peak_bytes ==
        exact.pair_discovery_index_peak_bytes);
    PC_CHECK(single_step.converged == exact.converged);
    PC_CHECK(near(
        single_step.success_probability,
        exact.success_probability, 1e-12));
    PC_CHECK(near(
        single_step.total_expected_cost,
        exact.total_expected_cost, 1e-12));
    PC_CHECK(single_step.edges.size() == exact.edges.size());
    PC_CHECK(single_step.action_totals.size() ==
             exact.action_totals.size());

    /* The public result is stored as doubles even though component solving
     * uses WideFloat. A sub-double caller tolerance must not reject a sound
     * exact attribution merely because the returned coordinates round to
     * representable doubles. */
    StrategyEvalOptions sub_double_options = options;
    sub_double_options.epsilon = 1e-30;
    const StrategyEvalResult sub_double =
        evaluate_strategy(*strategy, sub_double_options);
    PC_CHECK(sub_double.converged);
    PC_CHECK(near(sub_double.success_probability, 1.0, 1e-10));
    PC_CHECK(sub_double.raw_pairs_discovered == 76);
    PC_CHECK(sub_double.refined_pairs == 57);

    /* A cap that exactly admits the collision-safe compact discovery peak may
     * stop on the first refinement allocation. Replay recipes keep the pair
     * index as exact successor authority through quotient conversion and raw
     * attribution, so it no longer retires at closure. The stop must still be
     * classified as refinement rather than pair discovery. */
    StrategyEvalWork discovery_memory(strategy, options);
    while (discovery_memory.progress().pending_pairs != 0) {
        discovery_memory.step(1);
    }
    PC_CHECK(discovery_memory.progress().pending_pairs == 0);
    const std::uint64_t discovery_memory_cap = std::max(
        discovery_memory.live_owned_bytes(),
        discovery_memory.peak_owned_bytes());
    StrategyEvalOptions partition_memory_options = options;
    partition_memory_options.max_owned_bytes = discovery_memory_cap;
    StrategyEvalWork partition_memory(
        strategy, partition_memory_options);
    while (partition_memory.progress().pending_pairs != 0) {
        partition_memory.step(1);
    }
    bool partition_memory_capped = false;
    bool partition_memory_reached_refinement = false;
    try {
        while (!partition_memory.progress().done) {
            partition_memory.step(1);
            if (partition_memory.diagnostic_result().boundary_subphase ==
                StrategyEvalSubphase::PairRefinement) {
                partition_memory_reached_refinement = true;
            }
        }
    } catch (const std::length_error& error) {
        partition_memory_capped =
            std::string(error.what()).find("max_owned_bytes") !=
            std::string::npos;
    }
    const StrategyEvalResult& partition_diagnostic =
        partition_memory.diagnostic_result();
    PC_CHECK(
        partition_memory_reached_refinement ||
        partition_memory.progress().done ||
        partition_diagnostic.boundary_subphase ==
            StrategyEvalSubphase::PairRefinement ||
        partition_diagnostic.boundary_subphase ==
            StrategyEvalSubphase::ComponentConstruction);
    if (partition_memory_capped) {
        PC_CHECK(
            partition_memory_reached_refinement ||
            partition_diagnostic.boundary_subphase ==
                StrategyEvalSubphase::PairRefinement ||
            partition_diagnostic.boundary_subphase ==
                StrategyEvalSubphase::ComponentConstruction);
    } else {
        PC_CHECK(partition_memory.progress().done);
    }
    PC_CHECK(partition_diagnostic.raw_pairs_discovered == 76);
    PC_CHECK(
        partition_diagnostic.refined_pair_limit ==
        partition_memory_options.max_pairs);
    PC_CHECK(
        partition_diagnostic.stage_timings.pair_discovery_ns > 0);
    PC_CHECK(
        partition_diagnostic.stage_timings.pair_refinement_ns > 0);

    const StrategyEvalActionTotal* chaos =
        action_total(exact, "chaos");
    const StrategyEvalActionTotal* exalt =
        action_total(exact, "exalt");
    PC_CHECK(chaos != nullptr);
    PC_CHECK(exalt != nullptr);

    /* The behavioral quotient still collapses Chaos to four carriers, but
     * retained accounting must disaggregate its visits onto all 23 exact
     * input states rather than assigning them to a representative. Exalt
     * retains all 52 strict exclusion layouts in both views. This graph also
     * exercises directed source->cycle SCC attribution order. */
    PC_CHECK(chaos != nullptr && chaos->reachable_states == 23);
    PC_CHECK(exalt != nullptr && exalt->reachable_states == 52);
    PC_CHECK(
        exalt != nullptr &&
        exact.occupancy_states.size() >= exalt->reachable_states);
    PC_CHECK(
        chaos != nullptr && exalt != nullptr &&
        exalt->reachable_states > 2 * chaos->reachable_states);

    PC_CHECK(edge_value(exact, "chaos_to_exalt") > 0.0);
    PC_CHECK(edge_value(exact, "chaos_full_retry") > 0.0);
    PC_CHECK(edge_value(exact, "exalt_retry") > 0.0);
    PC_CHECK(near(
        edge_value(exact, "chaos_hit") +
            edge_value(exact, "exalt_hit"),
        1.0, 1e-10));
    PC_CHECK(exact.expected_consumption.at("chaos") > 1.0);
    PC_CHECK(exact.expected_consumption.at("exalt") > 0.0);

    check_reference_parity(*strategy, exact, options);

    StrategyEvalOptions quotient_guard = options;
    quotient_guard.max_pairs = 56;
    bool quotient_cap_failed = false;
    try {
        (void)evaluate_strategy(*strategy, quotient_guard);
    } catch (const std::length_error& ex) {
        quotient_cap_failed =
            std::string(ex.what()).find("max_pairs") !=
            std::string::npos;
    }
    PC_CHECK(quotient_cap_failed);

    StrategyEvalOptions observation_round_guard = options;
    observation_round_guard.max_sweeps = 1;
    bool observation_round_cap_failed = false;
    try {
        StrategyEvalWork capped(strategy, observation_round_guard);
        (void)capped;
    } catch (const std::length_error& ex) {
        observation_round_cap_failed =
            std::string(ex.what()).find("max_sweeps") !=
            std::string::npos;
    }
    PC_CHECK(observation_round_cap_failed);

    StrategyEvalWork observation_memory(strategy, options);
    const std::uint64_t observation_live =
        observation_memory.live_owned_bytes();
    const std::uint64_t observation_peak =
        observation_memory.peak_owned_bytes();
    PC_CHECK(observation_peak >= observation_live);
    if (observation_peak > observation_live) {
        StrategyEvalOptions observation_memory_guard = options;
        observation_memory_guard.max_owned_bytes = observation_peak - 1;
        bool observation_memory_cap_failed = false;
        try {
            StrategyEvalWork capped(strategy, observation_memory_guard);
            (void)capped;
        } catch (const std::length_error& ex) {
            observation_memory_cap_failed =
                std::string(ex.what()).find("max_owned_bytes") !=
                std::string::npos;
        }
        PC_CHECK(observation_memory_cap_failed);
    }
}

void run_observation_partition_delayed_split_tests() {
    auto session = make_eval_session();
    ActionRegistry registry = build_action_registry(*session);
    const std::uint32_t fracture =
        registry.index_by_id.at("fracture");
    const std::uint32_t scour =
        registry.index_by_id.at("scour");

    /*
     * Fracture produces four states that differ only in which affix is
     * fractured. With no downstream condition, Scour's observation
     * signature intentionally omits that preserved identity. Its raw exact
     * kernel nevertheless differs: each state keeps a different fractured
     * modifier. Equal observations therefore cannot authorize row reuse.
     */
    GoalSpec empty_goal;
    std::vector<std::uint64_t> authored_mods(session->words, 0);
    for (const std::uint32_t mod : {0u, 2u, 5u, 6u}) {
        pc_bitset_set(authored_mods.data(), mod);
    }
    CalcContext strict(
        session, empty_goal, registry, {fracture, scour},
        true, false, true, std::nullopt, {}, false,
        authored_mods, true);
    pc_item_state carrier;
    pc_item_clear(&carrier);
    carrier.rarity = PC_RARITY_RARE;
    PC_CHECK(
        pc_item_add_mod(
            &carrier, PC_SIDE_PREFIX, 0,
            session->primary_group[0], 0, nullptr) ==
        PC_RESULT_OK);
    PC_CHECK(
        pc_item_add_mod(
            &carrier, PC_SIDE_PREFIX, 2,
            session->primary_group[2], 0, nullptr) ==
        PC_RESULT_OK);
    PC_CHECK(
        pc_item_add_mod(
            &carrier, PC_SIDE_SUFFIX, 5,
            session->primary_group[5], 0, nullptr) ==
        PC_RESULT_OK);
    PC_CHECK(
        pc_item_add_mod(
            &carrier, PC_SIDE_SUFFIX, 6,
            session->primary_group[6], 0, nullptr) ==
        PC_RESULT_OK);

    refinement::SelectedAction selected;
    selected.action_id = scour;
    selected.semantic_key = {1};
    selected.contract = registry.actions[scour].refinement;
    std::vector<refinement::StableKey> signatures;
    std::vector<std::vector<OutcomeEntry>> kernels;
    const std::array<std::pair<int, std::uint32_t>, 4>
        fractured_slots{{
            {PC_SIDE_PREFIX, 0},
            {PC_SIDE_PREFIX, 1},
            {PC_SIDE_SUFFIX, 0},
            {PC_SIDE_SUFFIX, 1},
        }};
    for (const auto& [side, index] : fractured_slots) {
        pc_item_state exact = carrier;
        pc_mod_slot* slot =
            side == PC_SIDE_PREFIX
                ? &exact.prefixes[index]
                : &exact.suffixes[index];
        slot->flags |= PC_MOD_SLOT_FRACTURED;
        const std::uint32_t state = strict.intern_item(exact);
        const auto signature =
            refinement::canonical_operation_state_signature(
                *session, strict.layout(), strict.state(state),
                selected);
        PC_CHECK(signature.has_value());
        if (signature.has_value()) {
            signatures.push_back(*signature);
        }
        const OutcomeDistribution& kernel =
            strict.outcomes(state, scour);
        PC_CHECK(kernel.supported);
        PC_CHECK(kernel.entries.size() == 1);
        PC_CHECK(
            kernel.entries.size() == 1 &&
            near(kernel.entries.front().probability, 1.0));
        kernels.push_back(kernel.entries);
    }
    PC_CHECK(signatures.size() == 4);
    PC_CHECK(signatures[0] == signatures[1]);
    PC_CHECK(signatures[2] == signatures[3]);
    PC_CHECK(signatures[0] != signatures[2]);
    PC_CHECK(kernels.size() == 4);
    for (std::size_t i = 0; i < kernels.size(); ++i) {
        for (std::size_t j = i + 1; j < kernels.size(); ++j) {
            PC_CHECK(kernels[i] != kernels[j]);
        }
    }

    /*
     * A downstream fractured-family observer must keep the hit carrier apart
     * even though every Scour input begins in the same observation class and
     * the raw one-step kernels differ only beyond that class. This is the
     * delayed-split guard: refinement must be split-only to a fixed point.
     */
    const auto observed_strategy = compile(
        session,
        shell(
            "downstream observer fracture scour", "rare",
            R"JSON({"id":"start","kind":"start"},
{"id":"fracture","kind":"operation","operation":{"type":"fracture","params":{}}},
{"id":"scour","kind":"operation","operation":{"type":"scour","params":{}}},
{"id":"success","kind":"terminal","terminal":"success"},
{"id":"failure","kind":"terminal","terminal":"failure"})JSON",
            R"JSON({"id":"begin","from":"start","to":"fracture","priority":0,"condition":{"type":"always"}},
{"id":"fractured","from":"fracture","to":"scour","priority":0,"condition":{"type":"always"}},
{"id":"observed_hit","from":"scour","to":"success","priority":0,"condition":{"type":"has_mod_family","family_mod_key":"mod0","fractured":true}},
{"id":"observed_miss","from":"scour","to":"failure","priority":999,"is_default":true})JSON",
            R"JSON(,"prefixes":["mod0","mod2"],"suffixes":["mod5","mod6"])JSON"));
    const StrategyEvalResult observed =
        evaluate_strategy(*observed_strategy);
    PC_CHECK(observed.converged);
    PC_CHECK(near(observed.success_probability, 0.25, 1e-12));
    PC_CHECK(near(observed.failure_probability, 0.75, 1e-12));
    PC_CHECK(near(observed.expected_actions, 2.0, 1e-12));
    const StrategyEvalActionTotal* fracture_total =
        action_total(observed, "fracture");
    const StrategyEvalActionTotal* scour_total =
        action_total(observed, "scour");
    PC_CHECK(fracture_total != nullptr);
    PC_CHECK(scour_total != nullptr);
    PC_CHECK(
        fracture_total != nullptr &&
        fracture_total->reachable_states == 1);
    PC_CHECK(
        scour_total != nullptr &&
        scour_total->reachable_states == 3);
    /*
     * Semantic-strict discovery may canonicalize observer-equivalent modifier
     * ids before the closed partition sees them.  The partition may merge
     * further, but it must never manufacture more classes than discovered
     * semantic pairs.
     */
    PC_CHECK(
        observed.raw_pairs_discovered >=
        observed.refined_pairs);
    PC_CHECK(
        edge_value(observed, "observed_hit") > 0.0);
    PC_CHECK(
        edge_value(observed, "observed_miss") > 0.0);
    check_reference_parity(
        *observed_strategy, observed, StrategyEvalOptions{});

    /*
     * With an unconditional terminal continuation, same-side fractured
     * identities have equal immediate behavior and equal probability into
     * the same absorption category. They must merge despite their unequal
     * concrete Scour successor ids, while the prefix/suffix distinction that
     * Scour observes and preserves remains exact.
     */
    const auto unconditional_strategy = compile(
        session,
        shell(
            "unconditional fracture scour collapse", "rare",
            R"JSON({"id":"start","kind":"start"},
{"id":"fracture","kind":"operation","operation":{"type":"fracture","params":{}}},
{"id":"scour","kind":"operation","operation":{"type":"scour","params":{}}},
{"id":"success","kind":"terminal","terminal":"success"})JSON",
            R"JSON({"id":"begin","from":"start","to":"fracture","priority":0,"condition":{"type":"always"}},
{"id":"fractured","from":"fracture","to":"scour","priority":0,"condition":{"type":"always"}},
{"id":"done","from":"scour","to":"success","priority":0,"condition":{"type":"always"}})JSON",
            R"JSON(,"prefixes":["mod0","mod2"],"suffixes":["mod5","mod6"])JSON"));
    const StrategyEvalResult unconditional =
        evaluate_strategy(*unconditional_strategy);
    PC_CHECK(unconditional.converged);
    PC_CHECK(near(
        unconditional.success_probability, 1.0, 1e-12));
    PC_CHECK(near(
        unconditional.failure_probability, 0.0, 1e-12));
    PC_CHECK(near(
        unconditional.expected_actions, 2.0, 1e-12));
    const StrategyEvalActionTotal* unconditional_scour =
        action_total(unconditional, "scour");
    if (unconditional_scour == nullptr ||
        unconditional_scour->reachable_states != 2 ||
        scour_total == nullptr ||
        scour_total->reachable_states != 3 ||
        unconditional.raw_pairs_discovered <
            unconditional.refined_pairs) {
        std::printf(
            "fracture/scour partition diagnostics: observed_scour=%u "
            "unconditional_scour=%u unconditional_raw=%u "
            "unconditional_refined=%u\n",
            scour_total == nullptr
                ? 0u
                : scour_total->reachable_states,
            unconditional_scour == nullptr
                ? 0u
                : unconditional_scour->reachable_states,
            unconditional.raw_pairs_discovered,
            unconditional.refined_pairs);
    }
    PC_CHECK(unconditional_scour != nullptr);
    PC_CHECK(
        unconditional_scour != nullptr &&
        unconditional_scour->reachable_states == 2);
    PC_CHECK(
        scour_total != nullptr &&
        scour_total->reachable_states == 3);
    PC_CHECK(
        unconditional.raw_pairs_discovered >=
        unconditional.refined_pairs);
    PC_CHECK(unconditional.refined_pairs == 4);
    const auto success_node = std::find_if(
        unconditional.nodes.begin(), unconditional.nodes.end(),
        [](const StrategyEvalNode& node) {
            return node.id == "success";
        });
    PC_CHECK(success_node != unconditional.nodes.end());
    PC_CHECK(
        success_node != unconditional.nodes.end() &&
        success_node->classes.size() == 2);
    PC_CHECK(near(
        edge_value(unconditional, "fractured"), 1.0, 1e-12));
    PC_CHECK(near(
        edge_value(unconditional, "done"), 1.0, 1e-12));
    check_reference_parity(
        *unconditional_strategy, unconditional,
        StrategyEvalOptions{});
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
    PC_CHECK(result.raw_pairs_discovered < 512);
    const double routed_mass = edge_value(result, "miss");
    PC_CHECK(routed_mass > 0.0);
    for (int i = 0; i < kRouterCount; ++i) {
        const std::string route = "route_" + std::to_string(i);
        const std::string router = "router_" + std::to_string(i);
        PC_CHECK(near(edge_value(result, route), routed_mass, 1e-9));
        const StrategyEvalNode* retained = node_result(result, router);
        PC_CHECK(retained != nullptr);
        PC_CHECK(
            retained != nullptr &&
            near(retained->expected_visits, routed_mass, 1e-9));
        PC_CHECK(retained != nullptr && !retained->classes.empty());
    }

    const auto direct = compile(
        session,
        shell(
            "large fallback direct oracle", "rare",
            R"JSON({"id":"start","kind":"start"},
{"id":"chaos","kind":"operation","operation":{"type":"chaos","params":{}}},
{"id":"success","kind":"terminal","terminal":"success"})JSON",
            R"JSON({"id":"begin","from":"start","to":"chaos","priority":0,"condition":{"type":"always"}},
{"id":"hit","from":"chaos","to":"success","priority":0,"condition":{"type":"has_mod_family","family_mod_key":"mod0","min_tier":1}},
{"id":"miss","from":"chaos","to":"chaos","priority":999,"is_default":true})JSON"));
    const StrategyEvalResult direct_result =
        evaluate_strategy(*direct, options);
    PC_CHECK(near(
        result.success_probability,
        direct_result.success_probability, 1e-9));
    PC_CHECK(near(
        result.expected_actions,
        direct_result.expected_actions, 1e-9));
    PC_CHECK(near(
        result.expected_consumption.at("chaos"),
        direct_result.expected_consumption.at("chaos"), 1e-9));

    StrategyEvalWork single_step_work(large, options);
    while (!single_step_work.progress().done) {
        single_step_work.step(1);
    }
    const StrategyEvalResult single_step =
        single_step_work.take_result();
    PC_CHECK(single_step.raw_pairs_discovered ==
             result.raw_pairs_discovered);
    PC_CHECK(near(
        single_step.success_probability,
        result.success_probability, 1e-9));
    PC_CHECK(near(
        single_step.expected_actions,
        result.expected_actions, 1e-9));
    PC_CHECK(single_step.edges.size() == result.edges.size());
    for (const StrategyEvalEdge& edge : result.edges) {
        PC_CHECK(near(
            edge_value(single_step, edge.id),
            edge.expected_traversals, 1e-9));
    }

    const auto multiple_roots = compile(
        session,
        shell(
            "multiple deterministic route roots", "rare",
            R"JSON({"id":"start","kind":"start"},
{"id":"chaos","kind":"operation","operation":{"type":"chaos","params":{}}},
{"id":"route_a","kind":"router"},
{"id":"route_b","kind":"router"},
{"id":"success","kind":"terminal","terminal":"success"})JSON",
            R"JSON({"id":"begin","from":"start","to":"chaos","priority":0,"condition":{"type":"always"}},
{"id":"hit","from":"chaos","to":"success","priority":0,"condition":{"type":"has_mod_family","family_mod_key":"mod0","min_tier":1}},
{"id":"branch_a","from":"chaos","to":"route_a","priority":1,"condition":{"type":"has_mod_family","family_mod_key":"mod2","min_tier":1}},
{"id":"branch_b","from":"chaos","to":"route_b","priority":999,"is_default":true},
{"id":"return_a","from":"route_a","to":"chaos","priority":0,"condition":{"type":"always"}},
{"id":"return_b","from":"route_b","to":"chaos","priority":0,"condition":{"type":"always"}})JSON"));
    const StrategyEvalResult multiple =
        evaluate_strategy(*multiple_roots, options);
    PC_CHECK(multiple.converged);
    PC_CHECK(edge_value(multiple, "branch_a") > 0.0);
    PC_CHECK(edge_value(multiple, "branch_b") > 0.0);
    PC_CHECK(near(
        edge_value(multiple, "branch_a"),
        edge_value(multiple, "return_a"), 1e-9));
    PC_CHECK(near(
        edge_value(multiple, "branch_b"),
        edge_value(multiple, "return_b"), 1e-9));
    const StrategyEvalNode* route_a = node_result(multiple, "route_a");
    const StrategyEvalNode* route_b = node_result(multiple, "route_b");
    PC_CHECK(route_a != nullptr && route_b != nullptr);
    PC_CHECK(
        route_a != nullptr &&
        near(
            route_a->expected_visits,
            edge_value(multiple, "branch_a"), 1e-9));
    PC_CHECK(
        route_b != nullptr &&
        near(
            route_b->expected_visits,
            edge_value(multiple, "branch_b"), 1e-9));
    check_reference_parity(*multiple_roots, multiple, options);

    /* Cross the collision-safe trace index's initial 64 buckets at its
     * checked average chain of six. Every one-node router has a distinct
     * exact trace; the evaluator must retain one trace copy, rehash its
     * compact link index, and preserve all route/accounting flow. */
    constexpr int kTraceBoundaryCount = 385;
    std::ostringstream trace_nodes;
    trace_nodes << R"JSON({"id":"start","kind":"start"},
{"id":"success","kind":"terminal","terminal":"success"})JSON";
    std::ostringstream trace_edges;
    trace_edges << R"JSON({"id":"begin","from":"start","to":"restart_0","priority":0,"condition":{"type":"always"}})JSON";
    for (int i = 0; i < kTraceBoundaryCount; ++i) {
        trace_nodes
            << ",{\"id\":\"restart_" << i
            << "\",\"kind\":\"operation\",\"operation\":{\"type\":\"restart\",\"params\":{}}}"
            << ",{\"id\":\"trace_router_" << i
            << "\",\"kind\":\"router\"}";
        trace_edges
            << ",{\"id\":\"enter_router_" << i
            << "\",\"from\":\"restart_" << i
            << "\",\"to\":\"trace_router_" << i
            << "\",\"priority\":0,\"condition\":{\"type\":\"always\"}}"
            << ",{\"id\":\"leave_router_" << i
            << "\",\"from\":\"trace_router_" << i
            << "\",\"to\":\""
            << (i + 1 == kTraceBoundaryCount
                    ? std::string("success")
                    : "restart_" + std::to_string(i + 1))
            << "\",\"priority\":0,\"condition\":{\"type\":\"always\"}}";
    }
    const auto trace_boundary = compile(
        session,
        shell(
            "deterministic route trace index boundary", "normal",
            trace_nodes.str(), trace_edges.str()));
    const StrategyEvalResult trace_result =
        evaluate_strategy(*trace_boundary, options);
    PC_CHECK(trace_result.converged);
    PC_CHECK(trace_result.raw_pairs_discovered ==
             kTraceBoundaryCount + 1);
    PC_CHECK(near(
        trace_result.expected_actions,
        static_cast<double>(kTraceBoundaryCount), 1e-12));
    PC_CHECK(near(
        trace_result.expected_consumption.at("base"),
        static_cast<double>(kTraceBoundaryCount), 1e-12));
    PC_CHECK(near(
        edge_value(trace_result, "enter_router_384"), 1.0, 1e-12));
    PC_CHECK(near(
        edge_value(trace_result, "leave_router_384"), 1.0, 1e-12));
    check_reference_parity(*trace_boundary, trace_result, options);

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
        failed =
            std::string(ex.what()).find("max_states") !=
                std::string::npos ||
            std::string(ex.what()).find(
                "max_discovered_states") !=
                std::string::npos;
    }
    PC_CHECK(failed);

    StrategyEvalOptions transition_guard;
    transition_guard.max_transitions = 1;
    failed = false;
    std::string transition_diagnostic;
    try {
        (void)evaluate_strategy(*pair_guard, transition_guard);
    } catch (const std::length_error& ex) {
        transition_diagnostic = ex.what();
        failed =
            transition_diagnostic.find("max_transitions") !=
            std::string::npos;
    }
    PC_CHECK(failed);
    PC_CHECK(
        transition_diagnostic.find("pair_start=") != std::string::npos);
    PC_CHECK(
        transition_diagnostic.find("pair_operation=") !=
        std::string::npos);
    PC_CHECK(
        transition_diagnostic.find("deterministic_expanded=") !=
        std::string::npos);
    PC_CHECK(
        transition_diagnostic.find("policy_state_target_match=") !=
        std::string::npos);
    PC_CHECK(
        transition_diagnostic.find("deterministic_route_traces=") !=
        std::string::npos);
    PC_CHECK(
        transition_diagnostic.find("calc_owned=") != std::string::npos);
    PC_CHECK(
        transition_diagnostic.find("operation_rows=") !=
        std::string::npos);
    PC_CHECK(
        transition_diagnostic.find("projected_u32_route_tokens=") !=
        std::string::npos);
    PC_CHECK(
        transition_diagnostic.find("operation_action_census=[") !=
        std::string::npos);
    PC_CHECK(
        transition_diagnostic.find("replay_route_token_payload=") !=
        std::string::npos);
    PC_CHECK(
        transition_diagnostic.find("replay_route_result_authorities=") !=
        std::string::npos);

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
    PC_CHECK(
        std::string(error.message).find("max_states") !=
            std::string::npos ||
        std::string(error.message).find(
            "max_discovered_states") !=
            std::string::npos);
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

void run_modifier_offer_resolution_tests() {
    ActionRegistry registry;
    ActionDescriptor first;
    first.id = "test:offer:first";
    first.params.type = ActionType::Unveil;
    first.params.target_tag_id = 1;
    first.refinement.outcome_observation =
        RefinementOutcomeObservation::ModifierOffer;
    ActionDescriptor second = first;
    second.id = "test:offer:second";
    second.params.target_tag_id = 2;
    registry.index_by_id.emplace(first.id, 0);
    registry.actions.push_back(first);
    registry.index_by_id.emplace(second.id, 1);
    registry.actions.push_back(second);

    StrategyNode node;
    node.kind = StrategyNodeKind::Operation;
    node.action_type = static_cast<int>(ActionType::Unveil);
    node.action = second.params;
    node.action.mod_id = 7; /* sampled concrete choice, not template identity */
    PC_CHECK(resolve_strategy_action(node, registry) == 1);
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
    stage("segmented vector", [&] { run_segmented_vector_tests(); });
    stage("closed form", [&] { run_closed_form_tests(); });
    stage("modifier offer resolution", [&] {
        run_modifier_offer_resolution_tests();
    });
    stage("observation signature condition", [&] {
        run_observation_signature_condition_tests();
    });
    stage("destructive refinement cycle", [&] {
        run_destructive_refinement_cycle_test();
    });
    stage("observation partition delayed split", [&] {
        run_observation_partition_delayed_split_tests();
    });
    stage("condition parity", [&] { run_condition_parity_tests(); });
    stage("mc gate", [&] { run_mc_gate(); });
    stage("simulator semantics", [&] { run_simulator_semantics_tests(); });
    stage("refusals", [&] { run_refusal_and_unresolved_tests(); });
    stage("scale and fallback", [&] { run_scale_and_fallback_tests(); });
    stage("c abi", [&] { run_c_abi_tests(); });
    stage("artifact", [&] { run_artifact_and_registry_tests(artifact_dir); });
}
