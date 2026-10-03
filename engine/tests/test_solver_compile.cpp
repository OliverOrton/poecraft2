#include "tests.hpp"

#include "../src/solver_internal.hpp"
#include "../src/handles_internal.hpp"
#include "../src/solver_condition_expr.hpp"
#include "../src/solver_policy_refinement.hpp"
#include "../src/solver_policy_route.hpp"
#include "../src/solver_solve_types.hpp"
#include "../src/solver_compile_contracts.hpp"
#include "../src/solver_finder.hpp"
#include "../src/solver_selective_completion.hpp"
#include "../src/solver_diagnostic_options.hpp"
#include "../src/json.hpp"
#include "../src/solver_dirty_guidance.hpp"
#include "poecraft/bitset.h"
#include "poecraft/item_state.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <memory>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

using namespace poecraft;
using namespace poecraft::solver;

namespace {

std::shared_ptr<SessionImpl> make_compile_session();

void run_condition_expr_tests() {
    const ConditionExpr a = ConditionExpr::opaque(
        "{\"type\":\"item_flag\",\"flag\":\"a\"}");
    const ConditionExpr b = ConditionExpr::opaque(
        "{\"type\":\"item_flag\",\"flag\":\"b\"}");
    PC_CHECK(ConditionExpr::all({}).kind() ==
             ConditionExpr::Kind::Always);
    PC_CHECK(ConditionExpr::any({}).kind() ==
             ConditionExpr::Kind::Never);
    PC_CHECK(ConditionExpr::all({a}).json() == a.json());
    PC_CHECK(ConditionExpr::any({b}).json() == b.json());
    PC_CHECK(ConditionExpr::all({
                 ConditionExpr::always(), a, a,
                 ConditionExpr::all({b, a})})
                 .json() ==
             "{\"type\":\"all\",\"conditions\":[" + a.json() +
                 "," + b.json() + "]}");
    PC_CHECK(ConditionExpr::any({
                 ConditionExpr::never(), a, a,
                 ConditionExpr::any({b, a})})
                 .json() ==
             "{\"type\":\"any\",\"conditions\":[" + a.json() +
                 "," + b.json() + "]}");
    PC_CHECK(
        ConditionExpr::negate(a).json() ==
        "{\"type\":\"not\",\"conditions\":[" + a.json() + "]}");
    PC_CHECK(
        ConditionExpr::at_least(1, {a, b}).json() ==
        "{\"type\":\"at_least\",\"count\":1,\"conditions\":[" +
            a.json() + "," + b.json() + "]}");
}

PolicyRouteEdge partition_edge(
    std::string to,
    const ConditionExpr& condition,
    const std::size_t feature,
    const std::uint64_t value,
    const bool disjoint = true) {
    return {
        std::move(to), condition, {feature, value}, disjoint};
}

void run_policy_route_coalescing_tests() {
    const ConditionExpr a = ConditionExpr::opaque(
        "{\"type\":\"item_flag\",\"flag\":\"a\"}");
    const ConditionExpr b = ConditionExpr::opaque(
        "{\"type\":\"item_flag\",\"flag\":\"b\"}");
    const ConditionExpr c = ConditionExpr::opaque(
        "{\"type\":\"item_flag\",\"flag\":\"c\"}");
    const std::vector<PolicyRouteEdge> mixed =
        coalesce_disjoint_policy_route_edges({
            partition_edge("same", a, 4, 0),
            partition_edge("other", b, 4, 1),
            partition_edge("same", c, 4, 2)});
    PC_CHECK(mixed.size() == 2);
    PC_CHECK(mixed[0].to == "same");
    PC_CHECK(mixed[1].to == "other");
    PC_CHECK(mixed[0].condition.json() ==
             "{\"type\":\"any\",\"conditions\":[" + a.json() +
                 "," + c.json() + "]}");
    PC_CHECK(mixed[1].condition.json() == b.json());

    const auto no_proof = coalesce_disjoint_policy_route_edges({
        partition_edge("same", a, 4, 0, false),
        partition_edge("same", b, 4, 1)});
    PC_CHECK(no_proof.size() == 2);
    const auto different_features =
        coalesce_disjoint_policy_route_edges({
            partition_edge("same", a, 4, 0),
            partition_edge("same", b, 5, 1)});
    PC_CHECK(different_features.size() == 2);
    const auto repeated_value = coalesce_disjoint_policy_route_edges({
        partition_edge("same", a, 4, 0),
        partition_edge("same", b, 4, 0)});
    PC_CHECK(repeated_value.size() == 2);

    const auto adjacent_overlap =
        coalesce_priority_safe_policy_route_edges({
            partition_edge("same", a, 4, 0, false),
            partition_edge("same", b, 9, 0, false),
            partition_edge("other", c, 7, 0, false)});
    PC_CHECK(adjacent_overlap.size() == 2);
    PC_CHECK(adjacent_overlap[0].condition.kind() ==
             ConditionExpr::Kind::Any);
    const auto interleaved_overlap =
        coalesce_priority_safe_policy_route_edges({
            partition_edge("same", a, 4, 0, false),
            partition_edge("other", b, 9, 0, false),
            partition_edge("same", c, 7, 0, false)});
    PC_CHECK(interleaved_overlap.size() == 3);
}

void run_finder_request_binding_tests() {
    auto session = make_compile_session();
    ActionRegistry registry = build_action_registry(*session);
    const auto chaos = registry.index_by_id.at("chaos");
    GoalSpec goal;
    goal.rarity = PC_RARITY_RARE;
    GoalSlot wanted;
    wanted.family_id = session->family_id.at(5);
    wanted.min_tier = 1;
    goal.slots.push_back(wanted);
    CalcContext calc(session, goal, registry, {chaos});
    pc_item_state start;
    pc_item_clear(&start);
    start.rarity = PC_RARITY_RARE;
    const SolveOptions limits;
    const std::string graph = compile_finder_candidate_json(
        calc, start, {chaos}, limits);
    auto prepared = prepare_finder_candidate(calc, session, start, graph);
    PC_CHECK(prepared.ready());

    GoalSpec coverage_goal = goal;
    coverage_goal.terminal.extras = ExtraExplicitPolicy::Allow;
    CalcContext coverage_calc(
        session, coverage_goal, registry, {chaos});
    const std::string coverage_graph = compile_finder_candidate_json(
        coverage_calc, start, {chaos}, limits);
    PC_CHECK(compile_finder_goal_condition(coverage_calc) !=
        compile_finder_goal_condition(calc));
    PC_CHECK(prepare_finder_candidate(
        coverage_calc, session, start, coverage_graph).ready());
    PC_CHECK(!prepare_finder_candidate(
        coverage_calc, session, start, graph).ready());
    PC_CHECK(!prepare_finder_candidate(
        calc, session, start, coverage_graph).ready());

    pc_item_state changed_start = start;
    changed_start.searing_exarch_tier = 1;
    PC_CHECK(!prepare_finder_candidate(
        calc, session, changed_start, graph).ready());

    std::string fake_goal = graph;
    const std::string exact = compile_finder_goal_condition(calc);
    const auto goal_at = fake_goal.find(exact);
    PC_CHECK(goal_at != std::string::npos);
    fake_goal.replace(goal_at, exact.size(), "{\"type\":\"always\"}");
    PC_CHECK(!prepare_finder_candidate(
        calc, session, start, fake_goal).ready());

    std::string expanded_scope = graph;
    const auto action_at = expanded_scope.find("\"type\":\"chaos\"");
    PC_CHECK(action_at != std::string::npos);
    expanded_scope.replace(action_at, 14, "\"type\":\"exalt\"");
    PC_CHECK(!prepare_finder_candidate(
        calc, session, start, expanded_scope).ready());

    std::string one_attempt = graph;
    const auto nodes_end = one_attempt.find("],\"edges\":[");
    PC_CHECK(nodes_end != std::string::npos);
    one_attempt.insert(nodes_end,
        ",{\"id\":\"bad\",\"kind\":\"terminal\","
        "\"terminal\":\"failure\"}");
    const std::string retry_target =
        "\"id\":\"advance0\",\"from\":\"stage0\",\"to\":\"stage0\"";
    const auto retry_at = one_attempt.find(retry_target);
    PC_CHECK(retry_at != std::string::npos);
    one_attempt.replace(retry_at, retry_target.size(),
        "\"id\":\"advance0\",\"from\":\"stage0\",\"to\":\"bad\"");
    auto finite = prepare_finder_candidate(
        calc, session, start, one_attempt);
    PC_CHECK(finite.ready());
    StrategyEvalOptions priced_options;
    auto prices = std::make_shared<EconomyImpl>();
    prices->prices = {{"chaos", 1.0}};
    priced_options.economy = prices;
    const auto priced = evaluate_strategy(
        *finite.strategy, priced_options);
    PC_CHECK(priced.cost_complete);
    prices->prices.clear();
    const auto unpriced = evaluate_strategy(
        *finite.strategy, priced_options);
    PC_CHECK(!unpriced.cost_complete);
    PC_CHECK(!finder_evaluation_accepted(unpriced));

    GoalSpec finished_goal;
    finished_goal.rarity = PC_RARITY_RARE;
    pc_item_state finished;
    pc_item_clear(&finished);
    finished.rarity = PC_RARITY_RARE;
    for (const std::uint32_t mod : {3u, 4u, 5u, 6u}) {
        GoalSlot slot;
        slot.family_id = session->family_id[mod];
        slot.min_tier = 1;
        finished_goal.slots.push_back(slot);
        PC_CHECK(pc_item_add_mod(
            &finished, session->gen_type[mod], mod,
            static_cast<std::uint16_t>(session->primary_group[mod]),
            0, nullptr) == PC_RESULT_OK);
    }
    CalcContext complete(session, finished_goal, registry, {chaos});
    const std::string terminal_graph =
        "{\"version\":\"v1\",\"name\":\"finder valid root\","
        "\"base_state\":{\"base_key\":\"synthetic/base\","
        "\"item_level\":1,\"rarity\":\"rare\","
        "\"with_implicits\":false,\"prefixes\":[\"mod3\",\"mod4\"],"
        "\"suffixes\":[\"mod5\",\"mod6\"]},"
        "\"start_node_id\":\"start\",\"nodes\":["
        "{\"id\":\"start\",\"kind\":\"start\"},"
        "{\"id\":\"goal\",\"kind\":\"terminal\","
        "\"terminal\":\"success\"},"
        "{\"id\":\"bad\",\"kind\":\"terminal\","
        "\"terminal\":\"failure\"}],\"edges\":["
        "{\"id\":\"hit\",\"from\":\"start\",\"to\":\"goal\","
        "\"priority\":0,\"condition\":" +
        compile_finder_goal_condition(complete) +
        "},{\"id\":\"miss\",\"from\":\"start\",\"to\":\"bad\","
        "\"priority\":1,\"is_default\":true}]}";
    auto trusted = prepare_finder_candidate(
        complete, session, finished, terminal_graph);
    PC_CHECK(trusted.ready());
    StrategyEvalOptions checked_options;
    checked_options.economy = std::make_shared<EconomyImpl>();
    const auto evaluation = evaluate_strategy(
        *trusted.strategy, checked_options);
    PC_CHECK(finder_evaluation_accepted(evaluation));
    PC_CHECK(evaluation.total_expected_cost == 0.0);
    // The evaluator follows the graph's success terminal even on a dirty
    // item. Only request-bound preparation may give that mass solver authority.
    GoalSpec dirty_request = finished_goal;
    dirty_request.slots.pop_back();
    dirty_request.terminal.extras = ExtraExplicitPolicy::Allow;
    CalcContext dirty_calc(session, dirty_request, registry, {chaos});
    GoalSpec clean_request = dirty_request;
    clean_request.terminal.extras = ExtraExplicitPolicy::ForbidUnmatched;
    CalcContext clean_calc(session, clean_request, registry, {chaos});
    std::string dirty_graph = terminal_graph;
    const std::string finished_condition =
        compile_finder_goal_condition(complete);
    const auto condition_at = dirty_graph.find(finished_condition);
    PC_CHECK(condition_at != std::string::npos);
    if (condition_at != std::string::npos)
        dirty_graph.replace(condition_at, finished_condition.size(),
            compile_finder_goal_condition(dirty_calc));
    PC_CHECK(compiled_success_ingress_matches_request(
        dirty_calc, dirty_graph));
    PC_CHECK(!compiled_success_ingress_matches_request(
        clean_calc, dirty_graph));
    auto dirty_prepared = prepare_finder_candidate(
        dirty_calc, session, finished, dirty_graph);
    PC_CHECK(dirty_prepared.ready());
    PC_CHECK(!prepare_finder_candidate(
        clean_calc, session, finished, dirty_graph).ready());
    if (dirty_prepared.ready()) {
        const auto dirty_eval = evaluate_strategy(
            *dirty_prepared.strategy, checked_options);
        PC_CHECK(finder_evaluation_accepted(dirty_eval));
        PC_CHECK(dirty_eval.success_probability == 1.0);
        PC_CHECK(dirty_eval.total_expected_cost == 0.0);
    }
    pc_item_state explicit_implicit_start = finished;
    explicit_implicit_start.implicit_count = 1;
    explicit_implicit_start.implicits[0].mod_id = 2;
    explicit_implicit_start.implicits[0].group_id =
        static_cast<std::uint16_t>(session->primary_group[2]);
    explicit_implicit_start.implicits[0].flags = PC_MOD_SLOT_ELDRITCH;
    const std::string explicit_implicit_graph = compile_finder_candidate_json(
        dirty_calc, explicit_implicit_start, {}, limits);
    PC_CHECK(explicit_implicit_graph.find("\"implicits\":[") !=
        std::string::npos);
    const std::string no_implicit_graph = compile_finder_candidate_json(
        dirty_calc, finished, {}, limits);
    PC_CHECK(no_implicit_graph.find("\"implicits\":[]") !=
        std::string::npos);
    PC_CHECK(prepare_finder_candidate(
        dirty_calc, session, explicit_implicit_start,
        explicit_implicit_graph).ready());
    PolicyFinderWork dirty_root_finder(
        dirty_calc, session, finished, {}, limits);
    for (int i = 0; i < 10000 && !dirty_root_finder.progress().done; ++i)
        dirty_root_finder.step(1024);
    PC_CHECK(dirty_root_finder.progress().done);
    PC_CHECK(dirty_root_finder.best().has_value());
    if (dirty_root_finder.best().has_value())
        PC_CHECK(dirty_root_finder.best()->expected_cost == 0.0);
    PC_CHECK(!clean_calc.is_goal_state(
        clean_calc.state(clean_calc.intern_item(finished))));

    GoalSpec any_two = dirty_request;
    any_two.slots.push_back(finished_goal.slots.back());
    any_two.min_satisfied_slots = 2;
    CalcContext any_two_calc(session, any_two, registry, {chaos});
    PC_CHECK(compile_finder_goal_condition(any_two_calc) !=
        compile_finder_goal_condition(dirty_calc));
    PC_CHECK(!compiled_success_ingress_matches_request(
        any_two_calc, dirty_graph));
    GoalSpec observed_b;
    observed_b.rarity = PC_RARITY_RARE;
    observed_b.slots.push_back(finished_goal.slots.back());
    observed_b.terminal.extras = ExtraExplicitPolicy::Allow;
    CalcContext b_calc(session, observed_b, registry, {chaos});
    const auto base_end = dirty_graph.find(",\"start_node_id\"");
    PC_CHECK(base_end != std::string::npos);
    if (base_end != std::string::npos) {
        const std::string graph_base = dirty_graph.substr(0, base_end);
        const auto observed_graph = [&](const CalcContext& request) {
            return graph_base +
                ",\"start_node_id\":\"start\",\"nodes\":["
                "{\"id\":\"start\",\"kind\":\"start\"},"
                "{\"id\":\"observe\",\"kind\":\"router\"},"
                "{\"id\":\"gate\",\"kind\":\"router\"},"
                "{\"id\":\"goal\",\"kind\":\"terminal\","
                "\"terminal\":\"success\"},"
                "{\"id\":\"bad\",\"kind\":\"terminal\","
                "\"terminal\":\"failure\"}],\"edges\":["
                "{\"id\":\"enter\",\"from\":\"start\","
                "\"to\":\"observe\",\"priority\":0,\"is_default\":true},"
                "{\"id\":\"observe_b\",\"from\":\"observe\","
                "\"to\":\"gate\",\"priority\":0,\"condition\":" +
                compile_finder_goal_condition(b_calc) + "},"
                "{\"id\":\"observe_other\",\"from\":\"observe\","
                "\"to\":\"gate\",\"priority\":1,\"is_default\":true},"
                "{\"id\":\"hit\",\"from\":\"gate\","
                "\"to\":\"goal\",\"priority\":0,\"condition\":" +
                compile_finder_goal_condition(request) + "},"
                "{\"id\":\"miss\",\"from\":\"gate\","
                "\"to\":\"bad\",\"priority\":1,\"is_default\":true}]}";
        };
        const std::string observed_dirty = observed_graph(dirty_calc);
        PC_CHECK(compiled_success_ingress_matches_request(
            dirty_calc, observed_dirty));
        auto observed_prepared = prepare_finder_candidate(
            dirty_calc, session, finished, observed_dirty);
        PC_CHECK(observed_prepared.ready());
        if (observed_prepared.ready()) {
            const auto observed_eval = evaluate_strategy(
                *observed_prepared.strategy, checked_options);
            PC_CHECK(finder_evaluation_accepted(observed_eval));
            PC_CHECK(observed_eval.success_probability == 1.0);
        }
        const std::string observed_any_two = observed_graph(any_two_calc);
        PC_CHECK(compiled_success_ingress_matches_request(
            any_two_calc, observed_any_two));
        PC_CHECK(!compiled_success_ingress_matches_request(
            dirty_calc, observed_any_two));
        auto any_two_prepared = prepare_finder_candidate(
            any_two_calc, session, finished, observed_any_two);
        PC_CHECK(any_two_prepared.ready());
        if (any_two_prepared.ready()) {
            const auto any_two_eval = evaluate_strategy(
                *any_two_prepared.strategy, checked_options);
            PC_CHECK(finder_evaluation_accepted(any_two_eval));
            PC_CHECK(any_two_eval.success_probability == 1.0);
        }
    }
    CalcContext complete_without_actions(session, finished_goal, registry, {});
    PolicyFinderWork completed_finder(
        complete_without_actions, session, finished, {}, limits);
    for (int i = 0; i < 10000 && !completed_finder.progress().done; ++i)
        completed_finder.step(1024);
    PC_CHECK(completed_finder.progress().done);
    PC_CHECK(completed_finder.best().has_value());
    if (completed_finder.best().has_value()) {
        PC_CHECK(completed_finder.best()->expected_cost == 0.0);
        PC_CHECK(prepare_finder_candidate(
            complete_without_actions, session, finished,
            completed_finder.best()->strategy_json).ready());
    }

    /* First genuinely generated from-root policy: alteration renews a magic
     * item until the native exact one-affix goal is reached. */
    const auto alteration = registry.index_by_id.at("alteration");
    GoalSpec magic_goal;
    magic_goal.rarity = PC_RARITY_MAGIC;
    GoalSlot prefix_wanted;
    prefix_wanted.family_id = session->family_id.at(3);
    prefix_wanted.min_tier = 1;
    magic_goal.slots.push_back(prefix_wanted);
    CalcContext magic_calc(session, magic_goal, registry, {alteration});
    pc_item_state magic_start;
    pc_item_clear(&magic_start);
    magic_start.rarity = PC_RARITY_MAGIC;
    PolicyFinderWork finder(
        magic_calc, session, magic_start, {{"alteration", 1.0}}, limits);
    for (int i = 0; i < 10000 && !finder.progress().done; ++i)
        finder.step(1024);
    PC_CHECK(finder.progress().done);
    PC_CHECK(finder.best().has_value());
    if (finder.best().has_value()) {
        PC_CHECK(finder.best()->expected_cost > 0.0);
        PC_CHECK(prepare_finder_candidate(
            magic_calc, session, magic_start,
            finder.best()->strategy_json).ready());
    }
    GoalSpec magic_coverage = magic_goal;
    magic_coverage.terminal.extras = ExtraExplicitPolicy::Allow;
    CalcContext coverage_magic_calc(
        session, magic_coverage, registry, {alteration});
    PolicyFinderWork coverage_magic_finder(
        coverage_magic_calc, session, magic_start,
        {{"alteration", 1.0}}, limits);
    for (int i = 0; i < 10000 &&
         !coverage_magic_finder.progress().done; ++i)
        coverage_magic_finder.step(1024);
    PC_CHECK(coverage_magic_finder.progress().done);
    PC_CHECK(coverage_magic_finder.best().has_value());
    if (coverage_magic_finder.best().has_value()) {
        const auto& winner = *coverage_magic_finder.best();
        PC_CHECK(winner.expected_cost > 0.0);
        PC_CHECK(prepare_finder_candidate(
            coverage_magic_calc, session, magic_start,
            winner.strategy_json).ready());
        PC_CHECK(!prepare_finder_candidate(
            magic_calc, session, magic_start,
            winner.strategy_json).ready());
    }
    PolicyFinderWork finish_with_winner(
        magic_calc, session, magic_start, {{"alteration", 1.0}}, limits);
    for (int i = 0; i < 10000 &&
         !finish_with_winner.best().has_value() &&
         !finish_with_winner.progress().done; ++i)
        finish_with_winner.step(1024);
    PC_CHECK(finish_with_winner.best().has_value());
    if (finish_with_winner.best().has_value()) {
        const std::string retained =
            finish_with_winner.best()->strategy_json;
        finish_with_winner.request_bounded_finish();
        finish_with_winner.step(1);
        PC_CHECK(finish_with_winner.progress().done);
        PC_CHECK(finish_with_winner.best().has_value());
        PC_CHECK(finish_with_winner.best()->strategy_json == retained);
    }
    GoalSpec suffix_goal;
    suffix_goal.rarity = PC_RARITY_MAGIC;
    GoalSlot suffix_wanted;
    suffix_wanted.family_id = session->family_id.at(5);
    suffix_wanted.min_tier = 1;
    suffix_goal.slots.push_back(suffix_wanted);
    CalcContext suffix_calc(session, suffix_goal, registry, {alteration});
    PolicyFinderWork suffix_finder(
        suffix_calc, session, magic_start, {{"alteration", 2.0}}, limits);
    for (int i = 0; i < 10000 && !suffix_finder.progress().done; ++i)
        suffix_finder.step(1024);
    PC_CHECK(suffix_finder.progress().done);
    PC_CHECK(suffix_finder.best().has_value());
    if (suffix_finder.best().has_value())
        PC_CHECK(suffix_finder.best()->expected_cost > 0.0);
    const auto transmute = registry.index_by_id.at("transmute");
    CalcContext staged_calc(
        session, magic_goal, registry, {transmute, alteration});
    pc_item_state normal_start;
    pc_item_clear(&normal_start);
    normal_start.rarity = PC_RARITY_NORMAL;
    PolicyFinderWork staged_finder(staged_calc, session, normal_start,
        {{"transmute", 1.0}, {"alteration", 1.0}}, limits);
    for (int i = 0; i < 10000 && !staged_finder.progress().done; ++i)
        staged_finder.step(1024);
    PC_CHECK(staged_finder.progress().done);
    PC_CHECK(staged_finder.best().has_value());
    if (staged_finder.best().has_value())
        PC_CHECK(staged_finder.best()->strategy_json.find("stage1") !=
                 std::string::npos);
    PolicyFinderWork unpriced_finder(
        magic_calc, session, magic_start, {}, limits);
    unpriced_finder.step(1);
    PC_CHECK(unpriced_finder.progress().done);
    PC_CHECK(!unpriced_finder.best().has_value());
    PolicyFinderWork cancelled_finder(
        magic_calc, session, magic_start, {{"alteration", 1.0}}, limits);
    cancelled_finder.request_bounded_finish();
    cancelled_finder.step(1);
    PC_CHECK(cancelled_finder.progress().done);
    PC_CHECK(!cancelled_finder.best().has_value());
    SolveOptions tiny_memory = limits;
    tiny_memory.max_solver_owned_bytes = 1;
    bool capacity_refused = false;
    try {
        PolicyFinderWork too_small(
            magic_calc, session, magic_start,
            {{"alteration", 1.0}}, tiny_memory);
    } catch (const std::length_error&) {
        capacity_refused = true;
    }
    PC_CHECK(capacity_refused);
    GoalSpec clean_three;
    clean_three.rarity = PC_RARITY_RARE;
    for (const std::uint32_t mod : {2u, 3u, 4u}) {
        GoalSlot slot;
        slot.family_id = session->family_id.at(mod);
        slot.min_tier = 1;
        clean_three.slots.push_back(slot);
    }
    const auto annul = registry.index_by_id.at("annul");
    CalcContext cleanup_calc(
        session, clean_three, registry, {chaos, annul});
    const std::string cleanup_graph = compile_finder_candidate_json(
        cleanup_calc, start, {chaos, annul}, limits, true);
    auto cleanup_prepared = prepare_finder_candidate(
        cleanup_calc, session, start, cleanup_graph);
    PC_CHECK(cleanup_prepared.ready());
    auto cleanup_prices = std::make_shared<EconomyImpl>();
    cleanup_prices->prices = {{"chaos", 1.0}, {"annul", 1.0}};
    StrategyEvalOptions cleanup_options;
    cleanup_options.economy = cleanup_prices;
    const std::string impossible_single = compile_finder_candidate_json(
        cleanup_calc, start, {chaos}, limits);
    auto single_prepared = prepare_finder_candidate(
        cleanup_calc, session, start, impossible_single);
    PC_CHECK(single_prepared.ready());
    const auto single_eval = evaluate_strategy(
        *single_prepared.strategy, cleanup_options);
    PC_CHECK(!finder_evaluation_accepted(single_eval));
    PC_CHECK(!single_eval.converged);
    PC_CHECK(single_eval.unresolved_probability > 0.0);
    const auto cleanup_eval = evaluate_strategy(
        *cleanup_prepared.strategy, cleanup_options);
    PC_CHECK(finder_evaluation_accepted(cleanup_eval));
    FinderControlGraph conditional;
    conditional.entry = 0;
    conditional.nodes = {
        {FinderControlKind::TestGoal, kNoId, 5, 1},
        {FinderControlKind::TestSlot, 0, 2, 4},
        {FinderControlKind::TestAffixCountAtLeast4, kNoId, 3, 4},
        {FinderControlKind::RunPrimitive, annul, kNoId, kNoId, 0},
        {FinderControlKind::RunPrimitive, chaos, kNoId, kNoId, 0},
        {FinderControlKind::GoalTerminal},
    };
    const std::string conditional_graph = compile_finder_control_json(
        cleanup_calc, start, conditional, limits);
    auto conditional_prepared = prepare_finder_candidate(
        cleanup_calc, session, start, conditional_graph);
    PC_CHECK(conditional_prepared.ready());
    if (conditional_prepared.ready()) {
        const auto conditional_eval = evaluate_strategy(
            *conditional_prepared.strategy, cleanup_options);
        PC_CHECK(finder_evaluation_accepted(conditional_eval));
    }
    conditional.nodes[1].kind = FinderControlKind::Hole;
    bool hole_refused = false;
    try {
        (void)compile_finder_control_json(cleanup_calc, start, conditional, limits);
    } catch (const std::invalid_argument&) { hole_refused = true; }
    PC_CHECK(hole_refused);
    const auto scour = registry.index_by_id.at("scour");
    const auto alchemy = registry.index_by_id.at("alchemy");
    CalcContext program_calc(
        session, clean_three, registry, {chaos, annul, scour, alchemy});
    FinderControlGraph staged_control;
    staged_control.entry = 0;
    staged_control.nodes = {
        {FinderControlKind::TestGoal, kNoId, 7, 1},
        {FinderControlKind::TestSlot, 0, 2, 5},
        {FinderControlKind::TestAffixCountAtLeast4, kNoId, 3, 4},
        {FinderControlKind::RunPrimitive, annul, kNoId, kNoId, 0},
        {FinderControlKind::RunPrimitive, chaos, kNoId, kNoId, 0},
        {FinderControlKind::TestSlot, 1, 6, 4},
        {FinderControlKind::RunScourAlchemy, kNoId, kNoId, kNoId, 0},
        {FinderControlKind::GoalTerminal},
    };
    const std::string staged_graph = compile_finder_control_json(
        program_calc, start, staged_control, limits);
    PC_CHECK(staged_graph.find("_o1") != std::string::npos);
    auto staged_prepared = prepare_finder_candidate(
        program_calc, session, start, staged_graph);
    PC_CHECK(staged_prepared.ready());
    auto staged_prices = std::make_shared<EconomyImpl>();
    staged_prices->prices = {{"chaos", 1.0}, {"annul", 1.0},
        {"scour", 1.0}, {"alchemy", 1.0}};
    StrategyEvalOptions staged_options;
    staged_options.economy = staged_prices;
    if (staged_prepared.ready()) {
        const auto staged_eval = evaluate_strategy(
            *staged_prepared.strategy, staged_options);
        PC_CHECK(finder_evaluation_accepted(staged_eval));
    }
    PolicyFinderWork program_finder(program_calc, session, start,
        staged_prices->prices, limits);
    for (int i = 0; i < 10000 && !program_finder.progress().done; ++i)
        program_finder.step(1024);
    PC_CHECK(program_finder.progress().done);
    PC_CHECK(program_finder.best().has_value());
    PC_CHECK(program_finder.progress().checked >= 4);
    const std::string candidate_telemetry = program_finder.telemetry_json();
    const auto candidate_report = json::Parser(
        candidate_telemetry.data(), candidate_telemetry.size()).parse();
    bool checked_native_program = false;
    for (const auto& record : candidate_report.at("candidates").as_array()) {
        if (record.at("native_program").boolean &&
            record.at("status").string != "queued" &&
            !record.at("parent").string.empty())
            checked_native_program = true;
    }
    PC_CHECK(checked_native_program);
    GoalSpec other_three;
    other_three.rarity = PC_RARITY_RARE;
    for (const std::uint32_t mod : {3u, 4u, 5u}) {
        GoalSlot slot;
        slot.family_id = session->family_id.at(mod);
        slot.min_tier = 1;
        other_three.slots.push_back(slot);
    }
    CalcContext other_program_calc(session, other_three, registry,
        {chaos, annul, scour, alchemy});
    const std::string other_graph = compile_finder_control_json(
        other_program_calc, start, staged_control, limits);
    PC_CHECK(other_graph != staged_graph);
    auto other_prepared = prepare_finder_candidate(
        other_program_calc, session, start, other_graph);
    PC_CHECK(other_prepared.ready());
    if (other_prepared.ready()) {
        const auto other_eval = evaluate_strategy(
            *other_prepared.strategy, staged_options);
        PC_CHECK(finder_evaluation_accepted(other_eval));
    }
    CalcContext no_program_scope(session, clean_three, registry,
        {chaos, annul});
    PC_CHECK(!prepare_finder_candidate(
        no_program_scope, session, start, staged_graph).ready());
    // Dependency-only Eldritch steps are accepted only with the exact
    // compiler-owned occurrence from an admitted native programme.
    GoalSpec retention_goal;
    retention_goal.rarity = PC_RARITY_RARE;
    retention_goal.automatic_candidates = true;
    GoalSlot retention_prefix;
    retention_prefix.family_id = session->family_id[3];
    retention_prefix.min_tier = 1;
    retention_goal.slots.push_back(retention_prefix);
    GoalSlot retention_suffix;
    retention_suffix.family_id = session->family_id[5];
    retention_suffix.min_tier = 1;
    retention_goal.slots.push_back(retention_suffix);
    CalcContext retention_calc(session, retention_goal, registry, {chaos});
    pc_item_state held = start;
    PC_CHECK(pc_item_add_mod(&held, PC_SIDE_PREFIX, 3,
        session->primary_group[3], 0, nullptr) == PC_RESULT_OK);
    PC_CHECK(pc_item_add_mod(&held, PC_SIDE_SUFFIX, 6,
        session->primary_group[6], 0, nullptr) == PC_RESULT_OK);
    const auto held_state = retention_calc.intern_item(held);
    std::unordered_map<std::string, double> retention_prices{
        {"chaos", 10.0}, {"eldritch_annul", 2.0},
        {"eldritch_chaos", 3.0}, {"eldritch_ichor:1", 1.0}};
    AutomaticAdmissionLimits retention_limits;
    retention_limits.max_state_action_rows = 10000;
    retention_limits.max_transitions = 100000;
    retention_limits.max_solver_owned_bytes = 256ull * 1024ull * 1024ull;
    retention_limits.max_imprint_program_depth = 3;
    retention_limits.max_imprint_program_work = 256;
    retention_limits.prices = &retention_prices;
    const auto admitted = retention_calc.admit_state_local_automatic_candidates(
        held_state, retention_limits);
    std::uint32_t retained_option = kNoId;
    for (const auto index : admitted.admitted_operators) {
        const auto& option = retention_calc.operators().at(index);
        if (option.option_kind == FixedOptionKind::EldritchSideIntent &&
            option.intended_side == PC_SIDE_SUFFIX &&
            option.primitive_program.size() == 2 &&
            registry.actions.at(option.primitive_program.back()).params.type ==
                ActionType::EldritchAnnul) {
            retained_option = index;
            break;
        }
    }
    PC_CHECK(retained_option != kNoId);
    if (retained_option != kNoId) {
        FinderControlGraph retention_control;
        retention_control.entry = 0;
        retention_control.programs.push_back({
            retained_option, held_state, 1u});
        retention_control.nodes = {
            {FinderControlKind::TestGoal, kNoId, 4, 1},
            {FinderControlKind::TestSlot, 0, 2, 3},
            {FinderControlKind::RunNativeProgram, 0, kNoId, kNoId, 0},
            {FinderControlKind::RunPrimitive, chaos, kNoId, kNoId, 0},
            {FinderControlKind::GoalTerminal},
        };
        const std::string retention_graph = compile_finder_control_json(
            retention_calc, start, retention_control, limits);
        PC_CHECK(retention_graph.find("_o1") != std::string::npos);
        PC_CHECK(!prepare_finder_candidate(
            retention_calc, session, start, retention_graph).ready());
        PC_CHECK(prepare_finder_candidate(
            retention_calc, session, start, retention_graph,
            &retention_control).ready());
        std::string reordered = retention_graph;
        const auto step_at = reordered.find(
            "\"from\":\"c2\",\"to\":\"c2_o1\"");
        PC_CHECK(step_at != std::string::npos);
        if (step_at != std::string::npos)
            reordered.replace(step_at,
                std::string("\"from\":\"c2\",\"to\":\"c2_o1\"").size(),
                "\"from\":\"c2\",\"to\":\"c3\"");
        PC_CHECK(!prepare_finder_candidate(
            retention_calc, session, start, reordered,
            &retention_control).ready());
        FinderControlGraph false_source = retention_control;
        false_source.programs[0].admitted_state =
            retention_calc.intern_item(start);
        bool false_source_refused = false;
        try {
            (void)compile_finder_control_json(
                retention_calc, start, false_source, limits);
        } catch (const std::invalid_argument&) {
            false_source_refused = true;
        }
        PC_CHECK(false_source_refused);
    }
    GoalSpec acquired_prefixes;
    acquired_prefixes.rarity = PC_RARITY_RARE;
    acquired_prefixes.automatic_candidates = true;
    for (const std::uint32_t mod : {2u, 3u, 4u}) {
        GoalSlot slot;
        slot.family_id = session->family_id[mod];
        slot.min_tier = 1;
        acquired_prefixes.slots.push_back(slot);
    }
    CalcContext from_root(session, acquired_prefixes, registry, {chaos});
    PolicyFinderWork retention_finder(from_root, session, start,
        retention_prices, limits, FinderRankingMode::Heuristic,
        FinderGrammarMode::ConditionalRetention);
    for (int step = 0; step < 10000 &&
         !retention_finder.progress().done; ++step)
        retention_finder.step(1024);
    PC_CHECK(retention_finder.progress().done);
    PC_CHECK(retention_finder.best().has_value());
    if (retention_finder.best().has_value()) {
        const auto& winner = *retention_finder.best();
        PC_CHECK(winner.expected_cost > 0.0);
        PC_CHECK(winner.native_control.has_value());
        PC_CHECK(winner.strategy_json.find("eldritch_annul") !=
            std::string::npos);
        PC_CHECK(!prepare_finder_candidate(
            from_root, session, start, winner.strategy_json).ready());
        PC_CHECK(prepare_finder_candidate(
            from_root, session, start, winner.strategy_json,
            &*winner.native_control).ready());
    }
    const auto scores = score_finder_sketch_batch({
        {1.0, 1.0, 3, false, true},
        {1.0, 1.0, 3, true, false},
        {std::numeric_limits<double>::quiet_NaN(), 1.0, 3,
            true, false}});
    PC_CHECK(scores.size() == 3);
    PC_CHECK(scores[0] < scores[1]);
    PC_CHECK(std::isfinite(scores[2]));
    PolicyFinderWork cleanup_finder(cleanup_calc, session, start,
        {{"chaos", 1.0}, {"annul", 1.0}}, limits);
    PC_CHECK(cleanup_finder.progress().pending_holes > 0);
    for (int i = 0; i < 10000 && !cleanup_finder.progress().done; ++i)
        cleanup_finder.step(1024);
    PC_CHECK(cleanup_finder.progress().done);
    PC_CHECK(cleanup_finder.best().has_value());
    PC_CHECK(cleanup_finder.progress().pending_holes == 0);
    PC_CHECK(cleanup_finder.progress().checked >= 3);
    PC_CHECK(cleanup_finder.progress().refused == 1);
    if (cleanup_finder.best().has_value())
        PC_CHECK(cleanup_finder.best()->expected_cost <=
            cleanup_eval.total_expected_cost + 1e-8);
}

void run_finder_default_success_regression() {
    auto session = make_compile_session();
    ActionRegistry registry = build_action_registry(*session);
    const auto chaos = registry.index_by_id.at("chaos");
    GoalSpec goal;
    goal.rarity = PC_RARITY_RARE;
    GoalSlot wanted;
    wanted.family_id = session->family_id.at(5);
    wanted.min_tier = 1;
    goal.slots.push_back(wanted);
    CalcContext calc(session, goal, registry, {chaos});
    pc_item_state start;
    pc_item_clear(&start);
    start.rarity = PC_RARITY_RARE;
    PC_CHECK(!calc.is_goal_state(calc.state(calc.intern_item(start))));

    const std::string graph =
        "{\"version\":\"v1\",\"name\":\"default goal decoration\","
        "\"base_state\":{\"base_key\":\"synthetic/base\","
        "\"item_level\":1,\"rarity\":\"rare\",\"with_implicits\":false,"
        "\"prefixes\":[],\"suffixes\":[]},"
        "\"start_node_id\":\"start\",\"nodes\":["
        "{\"id\":\"start\",\"kind\":\"start\"},"
        "{\"id\":\"goal\",\"kind\":\"terminal\",\"terminal\":\"success\"}],"
        "\"edges\":[{\"id\":\"decorated_default\",\"from\":\"start\","
        "\"to\":\"goal\",\"priority\":0,\"is_default\":true,"
        "\"condition\":" + compile_finder_goal_condition(calc) + "}]}";
    const auto compiled = compile_strategy_json(
        session, graph.data(), graph.size());
    PC_CHECK(compiled->nodes.at(compiled->start_node).edges.size() == 1);
    const auto& edge = compiled->nodes.at(compiled->start_node).edges.front();
    PC_CHECK(edge.is_default);
    PC_CHECK(edge.condition.kind == ConditionKind::Always);
    PC_CHECK(!prepare_finder_candidate(calc, session, start, graph).ready());
}

void report_compile_solve_issue(
        const char* label,
        const SolveResult& solved) {
    if (solved.converged) return;
    std::printf(
        "solver compile issue [%s]: available=%u termination=%u "
        "publication=%s evaluation=%s compatibility=%s "
        "states=%u/%u\n",
        label,
        solved.policy_available ? 1u : 0u,
        static_cast<unsigned>(solved.termination),
        solved.diagnostics.policy_publication_failure_reason.c_str(),
        solved.diagnostics.policy_evaluation_failure.c_str(),
        solved.diagnostics.policy_compatibility_reason.c_str(),
        solved.diagnostics.expanded_states,
        solved.diagnostics.discovered_states);
}

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

StrategyEvalResult evaluate_compiled(
    std::shared_ptr<const SessionImpl> session,
    const std::string& strategy_json,
    const std::unordered_map<std::string, double>& prices) {
    auto strategy = compile_strategy_json(
        session, strategy_json.c_str(), strategy_json.size());
    auto economy = std::make_shared<EconomyImpl>();
    economy->id = "test";
    economy->prices = prices;
    StrategyEvalOptions options;
    options.economy = economy;
    return evaluate_strategy(*strategy, options);
}

/* The compiler follows the engine-owned observed-choice contract carried by
 * the admitted exact mechanic descriptor. */
void run_future_observed_choice_compile_test() {
    auto session = make_compile_session();
    ActionRegistry registry = build_action_registry(*session);
    const std::uint32_t unveil =
        registry.index_by_id.at("unveil");
    ActionDescriptor future = registry.actions.at(unveil);
    future.id = "test:future_observed_modifier_offer";
    future.display_name = "future observed modifier offer";
    canonicalize_and_validate_action_refinement_contract(
        *session, future);
    PC_CHECK(action_observes_modifier_offer(future));
    const std::uint32_t future_index =
        static_cast<std::uint32_t>(registry.actions.size());
    registry.index_by_id.emplace(future.id, future_index);
    registry.actions.push_back(std::move(future));

    GoalSpec goal;
    goal.rarity = PC_RARITY_RARE;
    GoalSlot wanted;
    wanted.family_id = session->family_id.at(5);
    wanted.min_tier = 1;
    goal.slots.push_back(wanted);
    CalcContext calc(
        session, goal, registry, {future_index}, false, true, true);
    pc_item_state start;
    pc_item_clear(&start);
    start.rarity = PC_RARITY_RARE;
    const std::uint32_t start_state = calc.intern_item(start);
    PC_CHECK(!calc.is_goal_state(calc.state(start_state)));

    SolveResult authored;
    authored.converged = true;
    authored.policy_available = true;
    authored.policy_status = SolvePolicyStatus::Exact;
    authored.termination = SolveTermination::ExactClosed;
    authored.start_state = start_state;
    authored.has_exact_start_item = true;
    authored.exact_start_item = start;
    const std::size_t state_count = calc.state_count();
    authored.values.assign(state_count, 1.0);
    authored.policy.assign(state_count, PolicyOperatorRef{});
    authored.expanded.assign(state_count, 1);
    authored.goal_states.assign(state_count, 0);
    authored.policy_reachable.assign(state_count, 0);
    authored.unveil_preferences.resize(state_count);
    authored.option_unveil_preferences.resize(state_count);
    authored.policy[start_state] = {
        PlannerOperatorKind::Primitive, future_index};
    authored.policy_reachable[start_state] = 1;
    authored.unveil_preferences[start_state] = {0, 3};

    const std::string strategy = compile_policy_strategy_json(
        calc, authored, "future-observed-choice");
    PC_CHECK(strategy.find("has_unveil_option") != std::string::npos);
    PC_CHECK(strategy.find("\"type\":\"unveil\"") !=
             std::string::npos);
    PC_CHECK(strategy.find("\"mod_key\":\"mod0\"") !=
             std::string::npos);
    PC_CHECK(strategy.find("\"mod_key\":\"mod3\"") !=
             std::string::npos);
}

struct StructuredRouteFixture {
    std::shared_ptr<SessionImpl> session;
    ActionRegistry registry;
    std::unique_ptr<CalcContext> calc;
    SolveResult solved;
    refinement::RefinedPolicyCompileRouting routing;
    std::uint32_t left_state = kNoId;
    std::uint32_t right_state = kNoId;
    std::uint32_t goal_state = kNoId;
    std::uint32_t scour = kNoId;
    std::uint32_t chaos = kNoId;
    std::uint32_t renewal = kNoId;
};

pc_item_state compile_item(
        const SessionImpl& session,
        const std::uint32_t mod,
        const std::int8_t side) {
    pc_item_state item;
    pc_item_clear(&item);
    item.rarity = PC_RARITY_MAGIC;
    PC_CHECK(
        pc_item_add_mod(
            &item, side, mod, session.primary_group.at(mod), 0,
            nullptr) == PC_RESULT_OK);
    return item;
}

refinement::ObservationRequirement exclusion_requirement() {
    refinement::ObservationRequirement requirement;
    RefinementAffixObservation observation;
    observation.features = refinement_feature(
        RefinementFeature::ModifierExclusionSignature);
    requirement.affix_observations.push_back(observation);
    return refinement::canonical_observation_requirement(
        std::move(requirement));
}

refinement::FeatureSignature observed_signature(
        const CalcContext& calc,
        const std::uint32_t state,
        const refinement::ObservationRequirement& requirement) {
    const refinement::AbstractFeatureExtraction extraction =
        refinement::extract_strict_abstract_features(
            calc.session(), calc.layout(), calc.state(state),
            requirement);
    PC_CHECK(extraction.complete());
    return refinement::observe_features(
        extraction.features, requirement);
}

refinement::SelectedAction compile_selected_operator(
        const CalcContext& calc,
        const std::uint32_t operator_index) {
    refinement::SelectedAction selected;
    selected.action_id = operator_index;
    selected.semantic_key = {
        0x7465737464656331ull}; /* "testdec1" */
    const PlannerOperator& planner =
        calc.operators().at(operator_index);
    const std::vector<std::uint64_t> operator_key =
        planner_operator_semantic_key(planner);
    selected.semantic_key.insert(
        selected.semantic_key.end(),
        operator_key.begin(), operator_key.end());
    if (planner.kind == PlannerOperatorKind::Primitive) {
        selected.contract =
            calc.registry()
                .actions.at(planner.primitive_action)
                .refinement;
    }
    return selected;
}

StructuredRouteFixture make_structured_route_fixture() {
    StructuredRouteFixture fixture;
    fixture.session = make_compile_session();
    fixture.registry = build_action_registry(*fixture.session);
    fixture.scour = fixture.registry.index_by_id.at("scour");
    fixture.chaos = fixture.registry.index_by_id.at("chaos");
    const std::uint32_t restart =
        fixture.registry.index_by_id.at("restart");

    GoalSpec goal;
    GoalSlot wanted;
    wanted.family_id = fixture.session->family_id.at(7);
    wanted.min_tier = 1;
    goal.slots.push_back(wanted);
    goal.rarity = PC_RARITY_MAGIC;
    FixedOptionSpec renewal;
    renewal.kind = FixedOptionKind::Renewal;
    renewal.program_action_ids = {"alteration"};
    renewal.exit_goal_slots = {0};
    renewal.exit_min_satisfied = 1;
    goal.fixed_options.push_back(renewal);
    fixture.calc = std::make_unique<CalcContext>(
        fixture.session, goal, fixture.registry,
        std::vector<std::uint32_t>{
            fixture.scour, fixture.chaos, restart},
        false, true, true);
    fixture.renewal =
        static_cast<std::uint32_t>(
            fixture.registry.actions.size());
    PC_CHECK(
        fixture.renewal < fixture.calc->operators().size());
    PC_CHECK(
        fixture.calc->operators()[fixture.renewal].kind ==
        PlannerOperatorKind::FixedOption);

    const pc_item_state left =
        compile_item(*fixture.session, 0, PC_SIDE_PREFIX);
    const pc_item_state right =
        compile_item(*fixture.session, 3, PC_SIDE_PREFIX);
    const pc_item_state goal_item =
        compile_item(*fixture.session, 7, PC_SIDE_SUFFIX);
    fixture.left_state = fixture.calc->intern_item(left);
    fixture.right_state = fixture.calc->intern_item(right);
    fixture.goal_state = fixture.calc->intern_item(goal_item);
    PC_CHECK(fixture.left_state != fixture.right_state);
    PC_CHECK(fixture.left_state != fixture.goal_state);
    PC_CHECK(fixture.right_state != fixture.goal_state);

    const std::size_t state_count = fixture.calc->state_count();
    fixture.solved.start_state = fixture.left_state;
    fixture.solved.has_exact_start_item = true;
    fixture.solved.exact_start_item = left;
    fixture.solved.policy_available = true;
    fixture.solved.policy_status = SolvePolicyStatus::Exact;
    fixture.solved.values.assign(state_count, 1.0);
    fixture.solved.policy.assign(state_count, PolicyOperatorRef{});
    fixture.solved.expanded.assign(state_count, 1);
    fixture.solved.goal_states.assign(state_count, 0);
    fixture.solved.policy_reachable.assign(state_count, 0);
    fixture.solved.behavioral_representative_by_state.resize(
        state_count);
    for (std::uint32_t state = 0; state < state_count; ++state) {
        fixture.solved.behavioral_representative_by_state[state] =
            state;
    }
    for (const std::uint32_t state :
         {fixture.left_state, fixture.right_state}) {
        fixture.solved.policy[state] =
            PolicyOperatorRef{fixture.scour};
        fixture.solved.policy_reachable[state] = 1;
    }
    fixture.solved.goal_states[fixture.goal_state] = 1;
    fixture.solved.policy_reachable[fixture.goal_state] = 1;

    const refinement::StableKey working_parent_key =
        exact_abstract_state_key(
            fixture.calc->state(fixture.left_state), 0);
    const refinement::StableKey terminal_parent_key =
        exact_abstract_state_key(
            fixture.calc->state(fixture.goal_state), 0);

    const refinement::ObservationRequirement requirement =
        exclusion_requirement();
    fixture.routing.classes.resize(3);
    const auto working_class =
        [&](const std::uint32_t class_id,
            const std::uint32_t state) {
            refinement::RefinedPolicyCompileClass policy_class;
            policy_class.class_id = class_id;
            policy_class.coarse_state = 0;
            policy_class.coarse_state_key = working_parent_key;
            policy_class.representative_state = state;
            policy_class.strict_members = {state};
            policy_class.required_observations = requirement;
            policy_class.observation_signature =
                observed_signature(
                    *fixture.calc, state, requirement);
            policy_class.selected_action =
                compile_selected_operator(
                    *fixture.calc, fixture.scour);
            policy_class.action_cost = 1.0;
            policy_class.transitions = {{2, 1.0}};
            return policy_class;
        };
    fixture.routing.classes[0] =
        working_class(0, fixture.left_state);
    fixture.routing.classes[1] =
        working_class(1, fixture.right_state);
    fixture.routing.classes[2].class_id = 2;
    fixture.routing.classes[2].coarse_state = 1;
    fixture.routing.classes[2].coarse_state_key = terminal_parent_key;
    fixture.routing.classes[2].representative_state =
        fixture.goal_state;
    fixture.routing.classes[2].strict_members = {
        fixture.goal_state};
    fixture.routing.classes[2].terminal = true;
    fixture.routing.parents = {
        {0, working_parent_key,
         fixture.calc->state(fixture.left_state)},
        {1, terminal_parent_key,
         fixture.calc->state(fixture.goal_state)}};
    fixture.routing.parent_layout = &fixture.calc->layout();
    return fixture;
}

StructuredRouteFixture make_structured_fixed_route_fixture() {
    StructuredRouteFixture fixture =
        make_structured_route_fixture();
    const OptionKernel& left_kernel =
        fixture.calc->option_kernel(
            fixture.left_state, fixture.renewal);
    const OptionKernel& right_kernel =
        fixture.calc->option_kernel(
            fixture.right_state, fixture.renewal);
    PC_CHECK(left_kernel.supported && left_kernel.legal);
    PC_CHECK(right_kernel.supported && right_kernel.legal);

    const std::size_t state_count =
        fixture.calc->state_count();
    const std::size_t old_state_count =
        fixture.solved.values.size();
    fixture.solved.values.resize(
        state_count,
        std::numeric_limits<double>::infinity());
    fixture.solved.policy.resize(state_count);
    fixture.solved.expanded.resize(state_count, 0);
    fixture.solved.goal_states.resize(state_count, 0);
    fixture.solved.policy_reachable.resize(state_count, 0);
    fixture.solved.unveil_preferences.resize(state_count);
    fixture.solved.option_unveil_preferences.resize(
        state_count);
    fixture.solved.behavioral_representative_by_state.resize(
        state_count);
    for (std::uint32_t state =
             static_cast<std::uint32_t>(old_state_count);
         state < state_count; ++state) {
        fixture.solved.behavioral_representative_by_state[state] =
            state;
    }
    for (const std::uint32_t state :
         {fixture.left_state, fixture.right_state}) {
        fixture.solved.policy[state] = PolicyOperatorRef{
            PlannerOperatorKind::FixedOption,
            fixture.renewal};
        fixture.solved.values[state] = 1.0;
        fixture.solved.expanded[state] = 1;
    }
    fixture.solved.policy_reachable[fixture.left_state] = 1;
    fixture.solved.policy_reachable[fixture.right_state] = 0;
    fixture.solved.behavioral_representative_by_state[
        fixture.right_state] = fixture.left_state;

    refinement::RefinedPolicyCompileClass working;
    working.class_id = 0;
    working.coarse_state = 0;
    working.coarse_state_key =
        fixture.routing.parents[0].coarse_state_key;
    working.representative_state = fixture.left_state;
    working.strict_members = {
        fixture.left_state, fixture.right_state};
    working.selected_action =
        compile_selected_operator(
            *fixture.calc, fixture.renewal);
    working.action_cost = 1.0;
    working.transitions = {{1, 1.0}};

    refinement::RefinedPolicyCompileClass terminal;
    terminal.class_id = 1;
    terminal.coarse_state = 1;
    terminal.coarse_state_key =
        fixture.routing.parents[1].coarse_state_key;
    terminal.representative_state = fixture.goal_state;
    terminal.strict_members = {fixture.goal_state};
    terminal.terminal = true;

    fixture.routing.classes = {
        std::move(working), std::move(terminal)};
    return fixture;
}

template <typename Mutator>
void expect_structured_route_refusal(
        Mutator mutate,
        const std::string& expected) {
    StructuredRouteFixture fixture =
        make_structured_route_fixture();
    mutate(fixture);
    bool refused = false;
    try {
        (void)compile_policy_strategy_json(
            *fixture.calc, fixture.solved,
            "invalid structured observation route", nullptr,
            std::numeric_limits<std::uint64_t>::max(),
            &fixture.routing);
    } catch (const std::exception& error) {
        refused =
            std::string(error.what()).find(expected) !=
            std::string::npos;
        if (!refused) {
            std::printf(
                "structured route refusal mismatch: expected=%s "
                "actual=%s\n",
                expected.c_str(), error.what());
        }
    }
    if (!refused) {
        std::printf(
            "structured route refusal was accepted: expected=%s\n",
            expected.c_str());
    }
    PC_CHECK(refused);
}

void run_structured_observation_route_tests() {
    {
        StructuredRouteFixture fixture =
            make_structured_route_fixture();
        fixture.solved.policy[fixture.right_state] =
            PolicyOperatorRef{fixture.chaos};
        fixture.routing.classes[1].selected_action =
            compile_selected_operator(
                *fixture.calc, fixture.chaos);
        PolicyCompilationTelemetry telemetry;
        const std::string strategy =
            compile_policy_strategy_json(
                *fixture.calc, fixture.solved,
                "structured observation route", &telemetry,
                std::numeric_limits<std::uint64_t>::max(),
                &fixture.routing);
        PC_CHECK(
            strategy.find("\"type\":\"observation_signature\"") !=
            std::string::npos);
        PC_CHECK(
            strategy.find("\"version\":1") != std::string::npos);
        PC_CHECK(
            strategy.find("\"id\":\"refined_parent_0\"") !=
            std::string::npos);
        PC_CHECK(
            strategy.find("\"id\":\"refined_parent_1\"") ==
            std::string::npos);
        PC_CHECK(
            telemetry.peak_owned_bytes >=
            strategy.capacity() + 1);
        PC_CHECK(
            telemetry.complete_peak_owned_bytes ==
            telemetry.peak_owned_bytes);
        PC_CHECK(
            telemetry.complete_peak_owned_bytes >=
            telemetry.previously_accounted_peak_owned_bytes);
        PC_CHECK(telemetry.total_condition_bytes > 0);
        PC_CHECK(telemetry.max_condition_bytes > 0);
        PC_CHECK(telemetry.behavioral_classes > 0);
        PC_CHECK(
            compile_strategy_json(
                fixture.session, strategy.data(), strategy.size()) !=
            nullptr);

        /* Gate 4 enforces the complete audit. A limit equal to the historic
         * partial estimate must reject when complete ownership is higher. */
        PolicyCompilationTelemetry corrected_cap;
        bool corrected_capped = false;
        try {
            (void)compile_policy_strategy_json(
                *fixture.calc, fixture.solved,
                "structured observation route", &corrected_cap,
                std::numeric_limits<std::uint64_t>::max(),
                &fixture.routing,
                telemetry.previously_accounted_peak_owned_bytes);
        } catch (const SolverResourceLimit& error) {
            corrected_capped =
                error.cap_name() == "max_solver_owned_bytes";
        }
        PC_CHECK(corrected_capped);
        PC_CHECK(corrected_cap.cap_hit == "max_solver_owned_bytes");
        PC_CHECK(
            corrected_cap.complete_peak_owned_bytes >
            telemetry.previously_accounted_peak_owned_bytes);

        StructuredRouteFixture remapped =
            make_structured_route_fixture();
        remapped.solved.policy[remapped.right_state] =
            PolicyOperatorRef{remapped.chaos};
        remapped.routing.classes[1].selected_action =
            compile_selected_operator(
                *remapped.calc, remapped.chaos);
        for (refinement::RefinedPolicyCompileClass& policy_class :
             remapped.routing.classes) {
            policy_class.coarse_state =
                policy_class.terminal ? 37 : 91;
        }
        remapped.routing.parents[0].coarse_state = 91;
        remapped.routing.parents[1].coarse_state = 37;
        const std::string remapped_strategy =
            compile_policy_strategy_json(
                *remapped.calc, remapped.solved,
                "structured observation route", nullptr,
                std::numeric_limits<std::uint64_t>::max(),
                &remapped.routing);
        PC_CHECK(remapped_strategy == strategy);
    }

    {
        StructuredRouteFixture fixture =
            make_structured_route_fixture();
        PolicyCompilationTelemetry telemetry;
        bool capped = false;
        try {
            (void)compile_policy_strategy_json(
                *fixture.calc, fixture.solved,
                "structured condition memory cap", &telemetry,
                std::numeric_limits<std::uint64_t>::max(),
                &fixture.routing, 1);
        } catch (const SolverResourceLimit& error) {
            capped =
                error.cap_name() == "max_solver_owned_bytes";
        }
        PC_CHECK(capped);
        PC_CHECK(
            telemetry.cap_hit == "max_solver_owned_bytes");
        PC_CHECK(telemetry.peak_owned_bytes > 1);
    }

    /* Raw carrier identity is irrelevant when the serialized semantic
     * observation and selected operation are identical. Such duplicate
     * classes are deliberately accepted, even when their local value or
     * projected class row differs: the router re-observes after the action. */
    {
        StructuredRouteFixture fixture =
            make_structured_route_fixture();
        refinement::ObservationRequirement none;
        for (std::uint32_t policy_class = 0;
             policy_class < 2; ++policy_class) {
            fixture.routing.classes[policy_class]
                .required_observations = none;
            fixture.routing.classes[policy_class]
                .observation_signature.clear();
        }
        fixture.routing.classes[1].action_cost = 9.0;
        fixture.routing.classes[1].transitions = {{2, 0.25}};
        const std::string strategy =
            compile_policy_strategy_json(
                *fixture.calc, fixture.solved,
                "equivalent duplicate structured routes", nullptr,
                std::numeric_limits<std::uint64_t>::max(),
                &fixture.routing);
        PC_CHECK(
            strategy.find("\"type\":\"observation_signature\"") ==
            std::string::npos);
    }

    expect_structured_route_refusal(
        [](StructuredRouteFixture& fixture) {
            fixture.routing.classes[1].class_id = 0;
        },
        "sidecar is not canonical");
    expect_structured_route_refusal(
        [](StructuredRouteFixture& fixture) {
            std::reverse(
                fixture.routing.classes[0].strict_members.begin(),
                fixture.routing.classes[0].strict_members.end());
            fixture.routing.classes[0].strict_members.push_back(
                fixture.left_state);
        },
        "sidecar is not canonical");
    expect_structured_route_refusal(
        [](StructuredRouteFixture& fixture) {
            fixture.routing.classes[0].coarse_state_key.clear();
            fixture.routing.classes[1].coarse_state_key.clear();
        },
        "lost its canonical coarse parent");
    expect_structured_route_refusal(
        [](StructuredRouteFixture& fixture) {
            fixture.routing.classes[1].coarse_state_key = {9};
        },
        "lost its canonical coarse parent");
    expect_structured_route_refusal(
        [](StructuredRouteFixture& fixture) {
            fixture.routing.classes[1].coarse_state = 2;
        },
        "lost its canonical coarse parent");
    {
        StructuredRouteFixture fixture =
            make_structured_route_fixture();
        AbstractState retry =
            fixture.calc->state(fixture.left_state);
        retry.goal_progress_retry_basin = 1;
        const std::uint32_t retry_state =
            fixture.calc->intern_state(retry);
        const std::size_t state_count = fixture.calc->state_count();
        fixture.solved.values.resize(state_count, 1.0);
        fixture.solved.policy.resize(state_count);
        fixture.solved.expanded.resize(state_count, 0);
        fixture.solved.goal_states.resize(state_count, 0);
        fixture.solved.policy_reachable.resize(state_count, 0);
        fixture.solved.behavioral_representative_by_state.resize(
            state_count, kNoId);
        fixture.solved.policy[retry_state] =
            PolicyOperatorRef{fixture.scour};
        fixture.solved.expanded[retry_state] = 1;
        fixture.solved.behavioral_representative_by_state[retry_state] =
            fixture.left_state;
        fixture.routing.classes[0].strict_members.push_back(
            retry_state);
        std::sort(
            fixture.routing.classes[0].strict_members.begin(),
            fixture.routing.classes[0].strict_members.end());
        fixture.solved.policy[fixture.right_state] =
            PolicyOperatorRef{fixture.chaos};
        fixture.routing.classes[1].selected_action =
            compile_selected_operator(*fixture.calc, fixture.chaos);
        const std::string strategy = compile_policy_strategy_json(
            *fixture.calc, fixture.solved,
            "structured route with hidden retry-basin member", nullptr,
            std::numeric_limits<std::uint64_t>::max(),
            &fixture.routing);
        PC_CHECK(!strategy.empty());
    }

    /* Identical/overlapping observations may not choose a different
     * executable semantic operation. */
    expect_structured_route_refusal(
        [](StructuredRouteFixture& fixture) {
            refinement::ObservationRequirement none;
            for (std::uint32_t policy_class = 0;
                 policy_class < 2; ++policy_class) {
                fixture.routing.classes[policy_class]
                    .required_observations = none;
                fixture.routing.classes[policy_class]
                    .observation_signature.clear();
            }
            fixture.solved.policy[fixture.right_state] =
                PolicyOperatorRef{fixture.chaos};
            fixture.routing.classes[1].selected_action =
                compile_selected_operator(
                    *fixture.calc, fixture.chaos);
        },
        "indistinguishable_refined_policy_actions");

    /* The state-local semantic decision key is part of executable identity,
     * even when the imported operator itself is unchanged. */
    expect_structured_route_refusal(
        [](StructuredRouteFixture& fixture) {
            refinement::ObservationRequirement none;
            for (std::uint32_t policy_class = 0;
                 policy_class < 2; ++policy_class) {
                fixture.routing.classes[policy_class]
                    .required_observations = none;
                fixture.routing.classes[policy_class]
                    .observation_signature.clear();
            }
            fixture.routing.classes[1]
                .selected_action->semantic_key.push_back(999);
        },
        "indistinguishable_refined_policy_actions");

    /* One broad predicate and one exact predicate overlap on the left
     * carrier. The overlap is safe only while both select the same action. */
    {
        StructuredRouteFixture fixture =
            make_structured_route_fixture();
        fixture.routing.classes[0].required_observations = {};
        fixture.routing.classes[0].observation_signature.clear();
        const std::string strategy =
            compile_policy_strategy_json(
                *fixture.calc, fixture.solved,
                "overlapping equivalent structured routes", nullptr,
                std::numeric_limits<std::uint64_t>::max(),
                &fixture.routing);
        PC_CHECK(!strategy.empty());
    }
    expect_structured_route_refusal(
        [](StructuredRouteFixture& fixture) {
            fixture.routing.classes[0].required_observations = {};
            fixture.routing.classes[0]
                .observation_signature.clear();
            fixture.solved.policy[fixture.right_state] =
                PolicyOperatorRef{fixture.chaos};
            fixture.routing.classes[1].selected_action =
                compile_selected_operator(
                    *fixture.calc, fixture.chaos);
        },
        "overlapping_refined_policy_actions");

    /* A represented strict carrier must still match at least one route. */
    expect_structured_route_refusal(
        [](StructuredRouteFixture& fixture) {
            fixture.routing.classes[0].observation_signature =
                fixture.routing.classes[1].observation_signature;
            fixture.solved.policy[fixture.right_state] =
                PolicyOperatorRef{fixture.chaos};
            fixture.routing.classes[1].selected_action =
                compile_selected_operator(
                    *fixture.calc, fixture.chaos);
        },
        "indistinguishable_refined_policy_actions");

    expect_structured_route_refusal(
        [](StructuredRouteFixture& fixture) {
            fixture.routing.classes[1].strict_members.insert(
                fixture.routing.classes[1].strict_members.begin(),
                fixture.left_state);
        },
        "overlaps strict member classes");

    /* A fixed operator is routed by its imported strict operator identity,
     * while every hidden class member independently proves the exact retry
     * recipe emitted for the representative. */
    {
        StructuredRouteFixture fixture =
            make_structured_fixed_route_fixture();
        const std::string strategy =
            compile_policy_strategy_json(
                *fixture.calc, fixture.solved,
                "structured fixed renewal route", nullptr,
                std::numeric_limits<std::uint64_t>::max(),
                &fixture.routing);
        PC_CHECK(
            strategy.find("_retry") != std::string::npos);
        PC_CHECK(
            strategy.find("\"type\":\"alteration\"") !=
            std::string::npos);
    }
    {
        StructuredRouteFixture fixture =
            make_structured_fixed_route_fixture();
        fixture.solved.policy[fixture.right_state] =
            PolicyOperatorRef{fixture.chaos};
        bool refused = false;
        try {
            (void)compile_policy_strategy_json(
                *fixture.calc, fixture.solved,
                "fixed operator mismatch", nullptr,
                std::numeric_limits<std::uint64_t>::max(),
                &fixture.routing);
        } catch (const std::exception& error) {
            refused =
                std::string(error.what()).find(
                    "member operator disagrees") !=
                std::string::npos;
        }
        PC_CHECK(refused);
    }
    {
        StructuredRouteFixture fixture =
            make_structured_fixed_route_fixture();
        ObservedUnveilPreference unexpected;
        unexpected.observation_state =
            fixture.right_state;
        fixture.solved.option_unveil_preferences[
            fixture.right_state].push_back(
                std::move(unexpected));
        bool refused = false;
        try {
            (void)compile_policy_strategy_json(
                *fixture.calc, fixture.solved,
                "fixed choice-sidecar mismatch", nullptr,
                std::numeric_limits<std::uint64_t>::max(),
                &fixture.routing);
        } catch (const std::exception& error) {
            refused =
                std::string(error.what()).find(
                    "unexpected choice sidecar") !=
                std::string::npos;
        }
        PC_CHECK(refused);
    }
}

void run_closed_coarse_certification_domain_test() {
    StructuredRouteFixture fixture =
        make_structured_route_fixture();
    const pc_item_state mapped_item =
        compile_item(*fixture.session, 4, PC_SIDE_PREFIX);
    const std::uint32_t mapped_state =
        fixture.calc->intern_item(mapped_item);
    const pc_item_state losing_item =
        compile_item(*fixture.session, 2, PC_SIDE_PREFIX);
    const std::uint32_t losing_state =
        fixture.calc->intern_item(losing_item);
    pc_item_state mapped_goal_item =
        compile_item(*fixture.session, 7, PC_SIDE_SUFFIX);
    mapped_goal_item.suffixes[0].flags |= PC_MOD_SLOT_FRACTURED;
    const std::uint32_t mapped_goal_state =
        fixture.calc->intern_item(mapped_goal_item);
    PC_CHECK(
        fixture.calc->is_goal_state(
            fixture.calc->state(mapped_goal_state)));
    const std::size_t state_count = fixture.calc->state_count();
    fixture.solved.values.resize(state_count, 1.0);
    fixture.solved.policy.resize(state_count);
    fixture.solved.expanded.resize(state_count, 0);
    fixture.solved.goal_states.resize(state_count, 0);
    fixture.solved.policy_reachable.resize(state_count, 0);
    fixture.solved.unveil_preferences.resize(state_count);
    fixture.solved.option_unveil_preferences.resize(state_count);
    fixture.solved.behavioral_representative_by_state.resize(
        state_count, kNoId);
    fixture.solved.behavioral_representative_by_state[mapped_state] =
        fixture.right_state;
    fixture.solved.behavioral_representative_by_state[losing_state] =
        losing_state;
    fixture.solved.behavioral_representative_by_state[mapped_goal_state] =
        fixture.goal_state;
    fixture.solved.expanded[losing_state] = 1;
    fixture.solved.values[losing_state] =
        solve_detail::kValueCeiling;
    fixture.solved.policy_reachable[fixture.right_state] = 0;
    fixture.solved.policy[fixture.right_state] =
        PolicyOperatorRef{fixture.chaos};
    PolicyRefinementTelemetry& closure =
        fixture.solved.diagnostics.policy_refinement;
    closure.pre_extraction_non_goal_closed = true;
    closure.coarse_action_envelope_closed = true;
    closure.coarse_discovery_closed = true;

    const std::uint64_t unlimited =
        std::numeric_limits<std::uint64_t>::max();
    const std::string reachable_only = compile_policy_strategy_json(
        *fixture.calc, fixture.solved,
        "reachable quotient policy", nullptr, unlimited, nullptr,
        unlimited, PolicyRouteDefaultMode::CertificationFailClosed);
    PC_CHECK(
        reachable_only.find("\"type\":\"chaos\"") ==
        std::string::npos);
    fixture.solved.refined_policy_artifact.strategy_json =
        reachable_only;
    fixture.solved.refined_policy_artifact.policy_route_default_mode =
        "certification_fail_closed";
    /* A retained reachable-only artifact is not a cache hit for the
     * assertion-owned closed-domain request. */
    fixture.solved.refined_policy_artifact
        .closed_coarse_domain_route_states = 0;

    PolicyCompilationTelemetry certification;
    const std::string expanded = compile_policy_strategy_json(
        *fixture.calc, fixture.solved,
        "closed coarse certification policy", &certification,
        unlimited, nullptr, unlimited,
        PolicyRouteDefaultMode::CertificationFailClosed, true);
    PC_CHECK(
        expanded.find("\"type\":\"chaos\"") !=
        std::string::npos);
    PC_CHECK(certification.working_states == 2);
    PC_CHECK(certification.closed_coarse_domain_added_states == 1);
    /* Only behavioral representatives carry sparse Bellman goal bits. The
     * physical goal member must be accepted, but never routed as policy
     * work. */
    PC_CHECK(fixture.solved.goal_states[mapped_goal_state] == 0);
    PC_CHECK(certification.closed_coarse_domain_route_states == 3);
    PC_CHECK(certification.policy_route_root_default_edges == 1);
    PC_CHECK(certification.policy_route_internal_default_edges > 0);
    PC_CHECK(
        certification.policy_route_default_edges ==
        certification.policy_route_root_default_edges +
            certification.policy_route_internal_default_edges);

    PolicyCompilationTelemetry product;
    const std::string paired = compile_policy_strategy_json(
        *fixture.calc, fixture.solved,
        "closed coarse certification policy", &product, unlimited, nullptr,
        unlimited, PolicyRouteDefaultMode::ProductSafeRestart, true);
    PC_CHECK(paired == expanded);
    PC_CHECK(
        product.closed_coarse_domain_added_states ==
        certification.closed_coarse_domain_added_states);
    PC_CHECK(
        product.closed_coarse_domain_route_states ==
        certification.closed_coarse_domain_route_states);
}

void run_policy_description_test() {
    auto session = make_compile_session();
    ActionRegistry registry = build_action_registry(*session);
    GoalSpec goal;
    GoalSlot slot;
    slot.family_id = 100;
    slot.min_tier = 1;
    goal.slots.push_back(slot);
    goal.rarity = PC_RARITY_MAGIC;
    pc_item_state start;
    pc_item_clear(&start);
    for (unsigned scope = 0; scope < 8; ++scope) {
        CalcContext calc(session, goal, registry,
            {registry.index_by_id.at("transmute"),
             registry.index_by_id.at("alteration")});
        SolveOptions options;
        options.goal_progress_gated_reforges = (scope & 1) != 0;
        options.allow_economic_restart = (scope & 2) != 0;
        options.consider_imprint_programs = (scope & 4) != 0;
        SolveResult solved = solve(calc, start,
            {{"transmute", 1.0}, {"alteration", 1.0}, {"base", 10.0}}, options);
        PC_CHECK(solved.converged);
        // Exercise the metadata producer, not the already retained graph.
        solved.refined_policy_artifact = {};
        for (const bool bounded : {false, true}) {
            solved.policy_status = bounded ? SolvePolicyStatus::BoundedFeasible
                                           : SolvePolicyStatus::Exact;
            const std::string json = compile_policy_strategy_json(
                calc, solved, "policy description");
            PC_CHECK(json.find(
                "compilation does not establish policy optimality") !=
                std::string::npos);
            PC_CHECK(json.find("Exact within") == std::string::npos);
            // A preliminary Exact status may be classified bounded later.
            PC_CHECK(json.find("solver reports exact closure") == std::string::npos);
            PC_CHECK(json.find("independently evaluated") == std::string::npos);
            PC_CHECK(json.find("\"solver_policy_scope\":") != std::string::npos);
            // Parse the whole graph, including the new unrestricted description.
            PC_CHECK(compile_strategy_json(session, json.c_str(), json.size()) != nullptr);
        }
    }
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

    /* Only the one-affix goal roll is terminal. A two-affix roll containing
     * the requested family plus unrelated junk must continue. */
    const double p = 0.5 * (100.0 / 1100.0);

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
        PC_CHECK(!solved.options.goal_progress_gated_reforges);
        PC_CHECK(json.find(
                     "\"solver_policy_scope\":\"unrestricted\"") !=
                 std::string::npos);
        PC_CHECK(json.find("\"type\":\"restart\"") == std::string::npos);
        PC_CHECK(json.find("\"expected_cost\":") != std::string::npos);
        PC_CHECK(compilation.working_states > 0);
        PC_CHECK(compilation.behavioral_classes >=
                 compilation.policy_regions);
        PC_CHECK(compilation.nodes > 0);
        PC_CHECK(compilation.edges > compilation.nodes);
        PC_CHECK(json.find("\"type\":\"transmute\"") !=
                 std::string::npos);
        PC_CHECK(json.find("\"type\":\"alteration\"") !=
                 std::string::npos);
        const std::string route_source =
            json.find("\"id\":\"policy_route_root\"") !=
                    std::string::npos
                ? "policy_route_root"
                : "router";
        PC_CHECK(json.find(
                     "\"from\":\"" + route_source +
                     "\",\"to\":\"offpolicy\"") !=
                 std::string::npos);
        std::size_t region_route_count = 0;
        std::size_t route_at = 0;
        while ((route_at = json.find(
                    "\"from\":\"" + route_source +
                        "\",\"to\":\"s",
                    route_at)) !=
               std::string::npos) {
            ++region_route_count;
            ++route_at;
        }
        PC_CHECK(
            region_route_count == compilation.policy_regions ||
            (compilation.policy_route_nondefault_edges > 0 &&
             compilation.policy_route_distinct_targets >=
                 compilation.policy_regions));
        PC_CHECK(compilation.strategy_json_bytes == json.size());
        PC_CHECK(
            compilation.complete_peak_owned_bytes ==
            compilation.peak_owned_bytes);
        PC_CHECK(
            compilation.complete_peak_owned_bytes >=
            compilation.previously_accounted_peak_owned_bytes);
        PC_CHECK(compilation.total_condition_bytes > 0);
        PC_CHECK(compilation.max_condition_bytes > 0);

        const double expected = solved.values[solved.start_state];
        const StrategyEvalResult exact =
            evaluate_compiled(session, json, prices);
        PC_CHECK(exact.converged);
        PC_CHECK(exact.cost_complete);
        PC_CHECK(exact.success_probability > 1.0 - 1e-9);
        PC_CHECK(exact.failure_probability < 1e-12);
        PC_CHECK(exact.action_not_applied_probability < 1e-12);
        PC_CHECK(exact.no_matching_edge_probability < 1e-12);
        PC_CHECK(exact.unresolved_probability < 1e-12);
        PC_CHECK(std::fabs(
                     exact.total_expected_cost - expected) < 1e-9);
        const SimulationSummaryInternal summary =
            run_compiled(session, json, prices, 10000, 42);
        PC_CHECK(summary.completed_runs == 10000);
        PC_CHECK(summary.success_count == summary.completed_runs);
        PC_CHECK(summary.missing_price_run_count == 0);
        const double mean =
            summary.known_total_cost /
            static_cast<double>(summary.completed_runs);
        std::printf("solver compile alt-spam: V=%.4f empirical=%.4f\n",
                    expected, mean);
        PC_CHECK(std::fabs(mean - expected) < 0.15);
        PC_CHECK(std::fabs(expected - 1.0 / p) < 1e-6);

        /* A quotient member can retain a non-authoritative infinite cached
         * value after its proper fixed policy has been evaluated at the
         * root. That must only suppress presentation metadata; it must not
         * turn the executable strategy document into invalid JSON. */
        SolveResult nonfinite_annotation = solved;
        std::uint32_t non_start_working_state = kNoId;
        for (std::uint32_t state = 0;
             state < nonfinite_annotation.values.size(); ++state) {
            if (state != nonfinite_annotation.start_state &&
                nonfinite_annotation.policy_reachable[state] &&
                !nonfinite_annotation.goal_states[state]) {
                non_start_working_state = state;
                break;
            }
        }
        PC_CHECK(non_start_working_state != kNoId);
        if (non_start_working_state != kNoId) {
            nonfinite_annotation.values[non_start_working_state] =
                std::numeric_limits<double>::infinity();
            const std::string annotation_json =
                compile_policy_strategy_json(
                    calc, nonfinite_annotation,
                    "nonfinite-annotation");
            PC_CHECK(annotation_json.find(":inf") == std::string::npos);
            PC_CHECK(annotation_json.find(":nan") == std::string::npos);
            auto annotation_strategy = compile_strategy_json(
                session, annotation_json.data(), annotation_json.size());
            PC_CHECK(annotation_strategy != nullptr);
            const StrategyEvalResult annotation_exact =
                evaluate_compiled(session, annotation_json, prices);
            PC_CHECK(annotation_exact.converged);
            PC_CHECK(annotation_exact.cost_complete);
            PC_CHECK(std::fabs(
                         annotation_exact.total_expected_cost - expected) <
                     1e-9);
        }

        /* The strict-state oracle takes the exact decision-DAG compiler path
         * rather than the completed behavioral-quotient predicates. It must
         * remain executable and preserve the same start value. */
        SolveOptions strict_options;
        strict_options.full_evidence = true;
        strict_options.strict_states = true;
        const SolveResult strict = solve(calc, start, prices, strict_options);
        PC_CHECK(strict.converged);
        PC_CHECK(strict.behavioral_representative_by_state.empty());
        PC_CHECK(std::fabs(
                     strict.values[strict.start_state] - expected) < 1e-9);
        PolicyCompilationTelemetry strict_compilation;
        const std::string strict_json = compile_policy_strategy_json(
            calc, strict, "alt-spam-strict", &strict_compilation);
        PC_CHECK(strict_json.find("\"id\":\"policy_route_") !=
                 std::string::npos);
        PC_CHECK(strict_compilation.policy_regions > 0);
        PC_CHECK(strict_compilation.nodes <= strict.options.max_compiled_nodes);
        std::printf(
            "solver compile alt-spam strict: %llu regions, %llu condition "
            "bytes, %llu JSON bytes\n",
            static_cast<unsigned long long>(
                strict_compilation.policy_regions),
            static_cast<unsigned long long>(
                strict_compilation.total_condition_bytes),
            static_cast<unsigned long long>(
                strict_compilation.strategy_json_bytes));
        auto strict_strategy = compile_strategy_json(
            session, strict_json.data(), strict_json.size());
        PC_CHECK(strict_strategy != nullptr);
        const StrategyEvalResult strict_exact =
            evaluate_compiled(session, strict_json, prices);
        PC_CHECK(strict_exact.converged);
        PC_CHECK(strict_exact.cost_complete);
        PC_CHECK(strict_exact.success_probability > 1.0 - 1e-9);
        PC_CHECK(strict_exact.failure_probability < 1e-12);
        PC_CHECK(strict_exact.action_not_applied_probability < 1e-12);
        PC_CHECK(strict_exact.no_matching_edge_probability < 1e-12);
        PC_CHECK(strict_exact.unresolved_probability < 1e-12);
        PC_CHECK(std::fabs(
                     strict_exact.total_expected_cost - expected) < 1e-9);
        const SimulationSummaryInternal strict_summary =
            run_compiled(session, strict_json, prices, 10000, 420042);
        PC_CHECK(strict_summary.completed_runs == 10000);
        PC_CHECK(strict_summary.success_count == 10000);
        PC_CHECK(strict_summary.failure_count == 0);
        PC_CHECK(strict_summary.no_matching_edge_count == 0);
    }

    /* A gated-capable destructive reforge whose exact row has no retry mass
     * is an ordinary act-then-return region. Two such carrier states must
     * share the emitted operation even though their exact state predicates
     * differ. A third Scour state prevents the uniform-renewal fast path and
     * makes this a direct regression for general policy-region grouping. */
    {
        GoalSpec two_slot_goal;
        two_slot_goal.rarity = PC_RARITY_RARE;
        for (const std::uint32_t family : {100u, 101u}) {
            GoalSlot wanted;
            wanted.family_id = family;
            wanted.min_tier = 1;
            two_slot_goal.slots.push_back(wanted);
        }
        const std::uint32_t chaos = registry.index_by_id.at("chaos");
        const std::uint32_t scour = registry.index_by_id.at("scour");
        CalcContext zero_retry_calc(
            session, two_slot_goal, registry, {chaos, scour});
        const auto fractured_carrier = [&](const std::uint32_t junk_mod) {
            pc_item_state item;
            pc_item_clear(&item);
            item.rarity = PC_RARITY_RARE;
            PC_CHECK(pc_item_add_mod(
                         &item, PC_SIDE_PREFIX, 0,
                         static_cast<std::uint16_t>(
                             session->primary_group[0]),
                         PC_MOD_SLOT_FRACTURED, nullptr) == PC_RESULT_OK);
            PC_CHECK(pc_item_add_mod(
                         &item, session->gen_type[junk_mod], junk_mod,
                         static_cast<std::uint16_t>(
                             session->primary_group[junk_mod]),
                         0, nullptr) == PC_RESULT_OK);
            return item;
        };
        const pc_item_state left_item = fractured_carrier(3);
        const pc_item_state right_item = fractured_carrier(4);
        const std::uint32_t left = zero_retry_calc.intern_item(left_item);
        const std::uint32_t right = zero_retry_calc.intern_item(right_item);
        const OutcomeDistribution& left_kernel =
            zero_retry_calc.outcomes(left, chaos, true);
        const OutcomeDistribution& right_kernel =
            zero_retry_calc.outcomes(right, chaos, true);
        PC_CHECK(left_kernel.supported);
        PC_CHECK(right_kernel.supported);
        PC_CHECK(left_kernel.goal_progress_gated);
        PC_CHECK(right_kernel.goal_progress_gated);
        PC_CHECK(left_kernel.gated_retry_probability == 0.0);
        PC_CHECK(right_kernel.gated_retry_probability == 0.0);

        pc_item_state scour_item;
        pc_item_clear(&scour_item);
        scour_item.rarity = PC_RARITY_RARE;
        PC_CHECK(pc_item_add_mod(
                     &scour_item, PC_SIDE_PREFIX, 3,
                     static_cast<std::uint16_t>(
                         session->primary_group[3]),
                     0, nullptr) == PC_RESULT_OK);
        const std::uint32_t scour_state =
            zero_retry_calc.intern_item(scour_item);

        SolveResult authored;
        authored.policy_available = true;
        authored.policy_status = SolvePolicyStatus::BoundedFeasible;
        authored.termination = SolveTermination::NumericalStability;
        authored.start_state = left;
        authored.has_exact_start_item = true;
        authored.exact_start_item = left_item;
        authored.evaluated_policy_cost = 10.0;
        authored.upper_bound = 10.0;
        authored.options.goal_progress_gated_reforges = true;
        authored.options.allow_economic_restart = false;
        const std::size_t state_count = zero_retry_calc.state_count();
        authored.values.assign(state_count, 1.0);
        authored.policy.resize(state_count);
        authored.expanded.assign(state_count, 0);
        authored.goal_states.assign(state_count, 0);
        authored.policy_reachable.assign(state_count, 0);
        authored.unveil_preferences.resize(state_count);
        authored.option_unveil_preferences.resize(state_count);
        for (std::uint32_t state = 0; state < state_count; ++state) {
            authored.goal_states[state] =
                zero_retry_calc.is_goal_state(
                    zero_retry_calc.state(state));
        }
        for (const std::uint32_t state : {left, right}) {
            authored.policy[state] = PolicyOperatorRef{chaos};
            authored.policy_reachable[state] = 1;
            authored.expanded[state] = 1;
        }
        authored.policy[scour_state] = PolicyOperatorRef{scour};
        authored.policy_reachable[scour_state] = 1;
        authored.expanded[scour_state] = 1;

        PolicyCompilationTelemetry compilation;
        const std::string json = compile_policy_strategy_json(
            zero_retry_calc, authored,
            "zero-retry renewal region sharing", &compilation);
        PC_CHECK(compilation.policy_regions == 2);
        PC_CHECK(compilation.primitive_region_nodes == 2);
        PC_CHECK(compilation.local_gated_route_nodes == 0);
        PC_CHECK(json.find("\"type\":\"chaos\"") !=
                 std::string::npos);
        PC_CHECK(json.find("\"type\":\"scour\"") !=
                 std::string::npos);
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

        SolveResult bounded = solved;
        bounded.policy_status = SolvePolicyStatus::BoundedFeasible;
        bounded.refined_policy_artifact = {};
        PolicyCompilationTelemetry product_telemetry;
        const std::string product = compile_policy_strategy_json(
            calc, bounded, "bounded pair", &product_telemetry);
        PolicyCompilationTelemetry certification_telemetry;
        const std::string certification = compile_policy_strategy_json(
            calc, bounded, "bounded pair",
            &certification_telemetry,
            std::numeric_limits<std::uint64_t>::max(), nullptr,
            std::numeric_limits<std::uint64_t>::max(),
            PolicyRouteDefaultMode::CertificationFailClosed);
        PC_CHECK(
            product_telemetry.policy_route_default_mode ==
            "product_safe_restart");
        PC_CHECK(product_telemetry.policy_route_default_edges > 0);
        PC_CHECK(product_telemetry.policy_route_root_default_edges == 1);
        PC_CHECK(
            product_telemetry.policy_route_default_edges ==
            product_telemetry.policy_route_root_default_edges +
                product_telemetry.policy_route_refined_parent_default_edges +
                product_telemetry.policy_route_internal_default_edges);
        PC_CHECK(
            product_telemetry.policy_route_restart_default_edges ==
            product_telemetry.policy_route_default_edges);
        PC_CHECK(
            certification_telemetry.policy_route_default_mode ==
            "certification_fail_closed");
        PC_CHECK(
            certification_telemetry.policy_route_offpolicy_default_edges ==
            certification_telemetry.policy_route_default_edges);
        PC_CHECK(
            certification_telemetry.policy_route_default_edges ==
            certification_telemetry.policy_route_root_default_edges +
                certification_telemetry
                    .policy_route_refined_parent_default_edges +
                certification_telemetry
                    .policy_route_internal_default_edges);
        PC_CHECK(
            product_telemetry.policy_route_default_edges ==
            certification_telemetry.policy_route_default_edges);
        PC_CHECK(product_telemetry.nodes == certification_telemetry.nodes);
        PC_CHECK(product_telemetry.edges == certification_telemetry.edges);
        std::string normalized_product = product;
        const std::string product_target =
            "\"to\":\"bounded_default_restart\"";
        const std::string certification_target =
            "\"to\":\"offpolicy\"";
        std::size_t replaced_defaults = 0;
        std::size_t offset = 0;
        while ((offset = normalized_product.find(
                    product_target, offset)) != std::string::npos) {
            normalized_product.replace(
                offset, product_target.size(), certification_target);
            offset += certification_target.size();
            ++replaced_defaults;
        }
        PC_CHECK(
            replaced_defaults ==
            product_telemetry.policy_route_default_edges);
        PC_CHECK(normalized_product == certification);
        PC_CHECK(product != certification);

        SolveResult current_carrier_only = bounded;
        current_carrier_only.options.allow_economic_restart = false;
        PolicyCompilationTelemetry restricted_product_telemetry;
        const std::string restricted_product =
            compile_policy_strategy_json(
                calc, current_carrier_only, "bounded current carrier",
                &restricted_product_telemetry);
        PolicyCompilationTelemetry restricted_certification_telemetry;
        const std::string restricted_certification =
            compile_policy_strategy_json(
                calc, current_carrier_only, "bounded current carrier",
                &restricted_certification_telemetry,
                std::numeric_limits<std::uint64_t>::max(), nullptr,
                std::numeric_limits<std::uint64_t>::max(),
                PolicyRouteDefaultMode::CertificationFailClosed);
        PC_CHECK(
            restricted_product_telemetry.policy_route_default_mode ==
            "product_fail_closed_no_economic_restart");
        PC_CHECK(
            restricted_product_telemetry
                .policy_route_offpolicy_default_edges ==
            restricted_product_telemetry.policy_route_default_edges);
        PC_CHECK(
            restricted_product_telemetry
                .policy_route_restart_default_edges == 0);
        PC_CHECK(
            restricted_product.find("bounded_default_restart") ==
            std::string::npos);
        PC_CHECK(restricted_product == restricted_certification);
        PC_CHECK(
            restricted_product_telemetry.policy_route_default_edges ==
            restricted_certification_telemetry
                .policy_route_default_edges);
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

    /* A tag-discriminating layout must still route every sampled result
     * exactly. Q3 may prove a junk identity unobservable under this deliberately
     * tiny action set, so the executable policy—not a redundant serialized
     * mod-count predicate—is the contract. */
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
        PC_CHECK(solved.diagnostics.observation_signature_mismatches == 0);
        PC_CHECK(solved.diagnostics.strict_discovered_states >=
                 solved.diagnostics.quotient_states);
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
     * exact group-tier guards, and simulate at V(start). Unobserved junk
     * identities may be removed by the exact outer quotient. */
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
        PolicyCompilationTelemetry compilation;
        const std::string json = compile_policy_strategy_json(
            perfect_calc, solved, "six-slot all-T1 perfect item",
            &compilation);
        PC_CHECK(compilation.behavioral_classes >
                 compilation.policy_regions);
        PC_CHECK(compilation.policy_regions == 1);
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
            "solver compile perfect item: V=%.4f empirical=%.4f "
            "(%u states, %llu classes, %llu regions, %llu condition "
            "bytes, %llu JSON bytes)\n",
            expected, mean, perfect_calc.state_count(),
            static_cast<unsigned long long>(
                compilation.behavioral_classes),
            static_cast<unsigned long long>(compilation.policy_regions),
            static_cast<unsigned long long>(
                compilation.total_condition_bytes),
            static_cast<unsigned long long>(
                compilation.strategy_json_bytes));
        PC_CHECK(std::fabs(mean - expected) < 2.0);
    }

    /* Unveil is a sampled offer followed by a zero-cost policy choice. The
     * compiler emits preference-ordered option guards and concrete unveil
     * operations; simulation must reproduce the Bellman value. */
    {
        pc_bitset_zero(
            session->unveiled_generic_mask.data(), session->words);
        pc_bitset_zero(
            session->unveiled_mask.data(), session->words);
        for (const std::uint32_t mod : {0u, 3u, 4u}) {
            pc_bitset_set(session->unveiled_generic_mask.data(), mod);
            pc_bitset_set(session->unveiled_mask.data(), mod);
        }
        GoalSpec unveil_goal;
        GoalSlot life;
        life.family_id = 100;
        life.min_tier = 1;
        unveil_goal.slots.push_back(life);
        for (const std::uint32_t family : {104u, 105u, 106u}) {
            GoalSlot retained;
            retained.family_id = family;
            retained.min_tier = 1;
            unveil_goal.slots.push_back(retained);
        }
        unveil_goal.rarity = PC_RARITY_RARE;
        const std::uint32_t veiled_exalt =
            registry.index_by_id.at("veiled_exalt");
        const std::uint32_t unveil = registry.index_by_id.at("unveil");
        const std::uint32_t annul = registry.index_by_id.at("annul");
        CalcContext unveil_calc(
            session, unveil_goal, registry,
            {veiled_exalt, unveil, annul, restart});
        const std::unordered_map<std::string, double> prices{
            {"veiled_exalt", 1.0}, {"annul", 0.1},
            {"base", 10.0}};
        pc_item_state rare;
        pc_item_clear(&rare);
        rare.rarity = PC_RARITY_RARE;
        for (const std::uint32_t mod : {5u, 6u, 7u}) {
            PC_CHECK(pc_item_add_mod(
                &rare, PC_SIDE_SUFFIX, mod,
                session->primary_group[mod], 0, nullptr) == PC_RESULT_OK);
        }
        const SolveResult solved = solve(unveil_calc, rare, prices);
        report_compile_solve_issue("policy-selected unveil", solved);
        PC_CHECK(solved.converged);
        const std::string json = compile_policy_strategy_json(
            unveil_calc, solved, "policy-selected unveil");
        PC_CHECK(json.find("\"type\":\"has_unveil_option\"") !=
                 std::string::npos);
        PC_CHECK(json.find("\"type\":\"unveil\"") !=
                 std::string::npos);
        PC_CHECK(json.find("\"type\":\"prefix_count_range\"") !=
                 std::string::npos);
        const StrategyEvalResult exact =
            evaluate_compiled(session, json, prices);
        PC_CHECK(exact.converged);
        PC_CHECK(exact.cost_complete);
        PC_CHECK(exact.success_probability > 1.0 - 1e-9);
        PC_CHECK(exact.failure_probability < 1e-12);
        PC_CHECK(exact.action_not_applied_probability < 1e-12);
        PC_CHECK(exact.no_matching_edge_probability < 1e-12);
        PC_CHECK(exact.unresolved_probability < 1e-12);
        PC_CHECK(std::fabs(
                     exact.total_expected_cost -
                     solved.evaluated_policy_cost) < 1e-7);
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

    /* A serialized imported start may already contain an unobserved Veiled
     * placeholder. The compiler must preserve that slot flag, Calculator
     * mode must integrate its offer distribution exactly, and Simulator mode
     * must materialize one offer set per run before routing. */
    {
        GoalSpec unveil_goal;
        GoalSlot life;
        life.family_id = 100;
        life.min_tier = 1;
        unveil_goal.slots.push_back(life);
        for (const std::uint32_t family : {104u, 105u, 106u}) {
            GoalSlot retained;
            retained.family_id = family;
            retained.min_tier = 1;
            unveil_goal.slots.push_back(retained);
        }
        unveil_goal.rarity = PC_RARITY_RARE;
        const std::uint32_t veiled_exalt =
            registry.index_by_id.at("veiled_exalt");
        const std::uint32_t unveil = registry.index_by_id.at("unveil");
        const std::uint32_t annul = registry.index_by_id.at("annul");
        CalcContext unveil_calc(
            session, unveil_goal, registry,
            {veiled_exalt, unveil, annul, restart});
        pc_item_state veiled_start;
        pc_item_clear(&veiled_start);
        veiled_start.rarity = PC_RARITY_RARE;
        for (const std::uint32_t mod : {5u, 6u, 7u}) {
            PC_CHECK(pc_item_add_mod(
                &veiled_start, PC_SIDE_SUFFIX, mod,
                session->primary_group[mod], 0, nullptr) == PC_RESULT_OK);
        }
        PC_CHECK(
            pc_item_add_mod(
                &veiled_start, PC_SIDE_PREFIX, 8, 30,
                PC_MOD_SLOT_VEILED, nullptr) == PC_RESULT_OK);
        const std::unordered_map<std::string, double> prices{
            {"veiled_exalt", 1.0}, {"annul", 0.1},
            {"base", 10.0}};
        const SolveResult solved =
            solve(unveil_calc, veiled_start, prices);
        report_compile_solve_issue("imported veiled start", solved);
        PC_CHECK(solved.converged);
        const std::string json = compile_policy_strategy_json(
            unveil_calc, solved, "imported veiled start");
        PC_CHECK(json.find("\"mod_key\":\"mod8\",\"veiled\":true") !=
                 std::string::npos);
        PC_CHECK(json.find("\"type\":\"has_unveil_option\"") !=
                 std::string::npos);
        const StrategyEvalResult exact =
            evaluate_compiled(session, json, prices);
        PC_CHECK(exact.converged);
        PC_CHECK(exact.cost_complete);
        PC_CHECK(exact.success_probability > 1.0 - 1e-9);
        PC_CHECK(exact.failure_probability < 1e-12);
        PC_CHECK(exact.action_not_applied_probability < 1e-12);
        PC_CHECK(exact.no_matching_edge_probability < 1e-12);
        PC_CHECK(exact.unresolved_probability < 1e-12);
        PC_CHECK(std::fabs(
                     exact.total_expected_cost -
                     solved.evaluated_policy_cost) < 1e-7);
        const SimulationSummaryInternal summary =
            run_compiled(session, json, prices, 10000, 7654321);
        PC_CHECK(summary.success_count == summary.completed_runs);
        PC_CHECK(summary.failure_count == 0);
        PC_CHECK(summary.no_matching_edge_count == 0);
        PC_CHECK(summary.action_not_applied_count == 0);
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

    /* The registry and compiler share the public currency vocabulary even
     * though the compiled data retains RePoE's internal influence enum. */
    const std::set<std::string> expected_influence_exalts{
        "influence_exalt:crusader",
        "influence_exalt:hunter",
        "influence_exalt:redeemer",
        "influence_exalt:warlord", "influence_exalt:shaper", "influence_exalt:elder"};
    std::set<std::string> actual_influence_exalts;
    for (const ActionDescriptor& action : registry.actions) {
        if (action.params.type == ActionType::InfluenceExalt) {
            actual_influence_exalts.insert(action.id);
            PC_CHECK(action.cost_keys ==
                     std::vector<std::string>{action.id});
        }
    }
    PC_CHECK(actual_influence_exalts == expected_influence_exalts);

    {
        const std::uint32_t warlord_action =
            registry.index_by_id.at("influence_exalt:warlord");
        const int warlord_code =
            data->influence_exalt_code_by_name.at("warlord");
        GoalSpec influence_goal;
        influence_goal.rarity = PC_RARITY_RARE;
        GoalSlot influence_slot;
        for (std::uint32_t mod = 0; mod < session->mod_count; ++mod) {
            if (session->influence_code[mod] == warlord_code) {
                influence_slot.family_id = session->family_id[mod];
                break;
            }
        }
        PC_CHECK(influence_slot.family_id != kNoId);
        if (influence_slot.family_id != kNoId) {
            influence_goal.slots.push_back(influence_slot);
            CalcContext influence_calc(
                session, influence_goal, registry, {warlord_action});
            pc_item_state rare;
            pc_item_clear(&rare);
            rare.rarity = PC_RARITY_RARE;
            const std::uint32_t start_state =
                influence_calc.intern_item(rare);
            const OutcomeDistribution& outcomes = influence_calc.outcomes(
                start_state, warlord_action, true);
            PC_CHECK(outcomes.supported);
            PC_CHECK(!outcomes.entries.empty());

            SolveResult authored;
            authored.converged = true;
            authored.policy_available = true;
            authored.policy_status = SolvePolicyStatus::Exact;
            authored.termination = SolveTermination::ExactClosed;
            authored.start_state = start_state;
            authored.has_exact_start_item = true;
            authored.exact_start_item = rare;
            const std::size_t state_count = influence_calc.state_count();
            authored.values.assign(state_count, 1.0);
            authored.policy.assign(state_count, PolicyOperatorRef{});
            authored.expanded.assign(state_count, 0);
            authored.goal_states.assign(state_count, 0);
            authored.policy_reachable.assign(state_count, 0);
            authored.policy[start_state] = {
                PlannerOperatorKind::Primitive, warlord_action};
            authored.expanded[start_state] = 1;
            authored.policy_reachable[start_state] = 1;
            for (const OutcomeEntry& outcome : outcomes.entries) {
                if (influence_calc.is_goal_state(
                        influence_calc.state(outcome.state))) {
                    authored.goal_states[outcome.state] = 1;
                }
            }
            const std::string influence_strategy =
                compile_policy_strategy_json(
                    influence_calc, authored,
                    "canonical-influence-exalt");
            PC_CHECK(influence_strategy.find(
                         "\"type\":\"influence_exalt\","
                         "\"influence\":\"warlord\"") !=
                     std::string::npos);
            PC_CHECK(influence_strategy.find(
                         "\"influence\":\"adjudicator\"") ==
                     std::string::npos);
            auto compiled_influence = compile_strategy_json(
                session, influence_strategy.data(),
                influence_strategy.size());
            bool saw_canonical_price = false;
            for (const StrategyNode& node : compiled_influence->nodes) {
                if (node.action.type != ActionType::InfluenceExalt) continue;
                PC_CHECK(node.price_keys == std::vector<std::string>{
                             "influence_exalt:warlord"});
                saw_canonical_price = true;
            }
            PC_CHECK(saw_canonical_price);
        }
    }

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
        const solve_detail::DirtyRowRewards reward{3.0,kernel.expected_primitive_actions};
        PC_CHECK(reward.proposal_reward(0.5)==4.0); // two primitives, one macro
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
     * an ordinary primitive retry router. A Magic Alteration can terminate
     * with exactly one requested affix; a Rare Chaos roll cannot. */
    {
        GoalSpec renewal_goal = goal;
        renewal_goal.rarity = PC_RARITY_MAGIC;
        FixedOptionSpec option;
        option.kind = FixedOptionKind::Renewal;
        option.program_action_ids = {"alteration"};
        option.exit_goal_slots = {0};
        option.exit_min_satisfied = 1;
        renewal_goal.fixed_options.push_back(option);
        CalcContext option_calc(
            session, renewal_goal, registry, {}, false, false);
        pc_item_state magic;
        pc_item_clear(&magic);
        magic.rarity = PC_RARITY_MAGIC;
        const std::uint32_t state = option_calc.intern_item(magic);
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
            option_calc, magic, {{"alteration", 1.0}});
        report_compile_solve_issue("renewal alteration", solved);
        PC_CHECK(solved.converged);
        PC_CHECK(solved.policy[solved.start_state] == op);
        const std::string strategy = compile_policy_strategy_json(
            option_calc, solved, "renewal-alteration");
        PC_CHECK(strategy.find("\"type\":\"alteration\"") !=
                 std::string::npos);
        PC_CHECK(strategy.find("_retry") != std::string::npos);
        const auto evaluated=evaluate_compiled(session,strategy,{{"alteration",1.0}});
        PC_CHECK(evaluated.converged && evaluated.cost_complete);
        double self=0;
        for (const auto& exit:kernel.exits) if (exit.state==state || exit.state==kNoId) self+=exit.probability;
        const double count=kernel.expected_primitive_actions/(1.0-self);
        PC_CHECK(count>1.0);
        PC_CHECK(std::abs(evaluated.expected_actions-count)<1e-8*count);
        PC_CHECK(std::abs(evaluated.total_expected_cost-count)<1e-8*count);
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
    pc_bitset_zero(
        observed_session->normal_random_roll_mask.data(),
        observed_session->words);
    pc_bitset_zero(
        observed_session->positive_spawn_weight_mask.data(),
        observed_session->words);
    pc_bitset_zero(
        observed_session->positive_base_weight_mask.data(),
        observed_session->words);
    for (const std::uint32_t mod : {5u, 6u, 7u}) {
        pc_bitset_set(
            observed_session->normal_random_roll_mask.data(), mod);
        pc_bitset_set(
            observed_session->positive_spawn_weight_mask.data(), mod);
        pc_bitset_set(
            observed_session->positive_base_weight_mask.data(), mod);
    }
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
        unveil_slot.min_tier = 1;
        unveil_goal.slots.push_back(unveil_slot);
        for (const std::uint32_t family : {104u, 105u, 106u}) {
            GoalSlot permanent;
            permanent.family_id = family;
            permanent.min_tier = 1;
            unveil_goal.slots.push_back(permanent);
        }
        unveil_goal.rarity = PC_RARITY_RARE;
        FixedOptionSpec option;
        option.kind = FixedOptionKind::Renewal;
        option.program_action_ids = {"veiled_chaos", "unveil"};
        option.exit_goal_slots = {0, 1, 2, 3};
        option.exit_min_satisfied = 4;
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
        for (const OutcomeChoiceGroup& group :
             kernel.observation_choice_groups) {
            PC_CHECK(group.observation_state != kNoId);
        }

        const SolveResult solved = solve(
            option_calc, rare, {{"veiled_chaos", 1.0}});
        PC_CHECK(solved.converged);
        PC_CHECK(!solved.option_unveil_preferences[state].empty());
        PC_CHECK(!solved.behavioral_representative_by_state.empty());
        std::map<std::uint32_t, std::set<std::uint32_t>>
            offered_mods_by_projected_successor;
        for (const ObservedUnveilPreference& observation :
             solved.option_unveil_preferences[state]) {
            for (const ObservedUnveilChoice& choice : observation.choices) {
                PC_CHECK(
                    choice.successor_state <
                    solved.behavioral_representative_by_state.size());
                if (choice.successor_state >=
                    solved.behavioral_representative_by_state.size()) {
                    continue;
                }
                offered_mods_by_projected_successor[
                    solved.behavioral_representative_by_state[
                        choice.successor_state]]
                    .insert(choice.mod_id);
            }
        }
        bool projected_successor_has_distinct_offered_mods = false;
        std::size_t max_offered_mods_per_projected_successor = 0;
        std::uint32_t projected_collision_classes = 0;
        for (const auto& [unused_successor, offered_mods] :
             offered_mods_by_projected_successor) {
            (void)unused_successor;
            max_offered_mods_per_projected_successor = std::max(
                max_offered_mods_per_projected_successor,
                offered_mods.size());
            if (offered_mods.size() > 1) {
                projected_successor_has_distinct_offered_mods = true;
                ++projected_collision_classes;
            }
        }
        PC_CHECK(projected_successor_has_distinct_offered_mods);
        const std::string strategy = compile_policy_strategy_json(
            option_calc, solved, "observed-unveil-renewal");
        PC_CHECK(strategy.find("has_unveil_option") != std::string::npos);
        PC_CHECK(strategy.find("\"type\":\"veiled_chaos\"") !=
                 std::string::npos);
        std::set<std::uint32_t> compiled_offered_mods;
        for (const ObservedUnveilPreference& observation :
             solved.option_unveil_preferences[state]) {
            for (const ObservedUnveilChoice& choice : observation.choices) {
                compiled_offered_mods.insert(choice.mod_id);
            }
        }
        PC_CHECK(compiled_offered_mods.size() > 1);
        for (const std::uint32_t mod_id : compiled_offered_mods) {
            const std::string marker =
                "\"mod_key\":\"" +
                observed_session->data->string_at(
                    observed_session->data->mod_key_sid.at(mod_id)) +
                "\"";
            PC_CHECK(strategy.find(marker) != std::string::npos);
        }
        const StrategyEvalResult exact = evaluate_compiled(
            observed_session, strategy, {{"veiled_chaos", 1.0}});
        std::printf(
            "solver observed unveil exact: cost=%.12f solver=%.12f "
            "success=%.12f failure=%.12f unapplied=%.12f "
            "noedge=%.12f unresolved=%.12f\n",
            exact.total_expected_cost, solved.evaluated_policy_cost,
            exact.success_probability, exact.failure_probability,
            exact.action_not_applied_probability,
            exact.no_matching_edge_probability,
            exact.unresolved_probability);
        for (const StrategyEvalFailure& failure :
             exact.failures_by_node) {
            std::printf(
                "solver observed unveil failure: node=%s reason=%s "
                "probability=%.12f\n",
                failure.node_id.c_str(), failure.reason.c_str(),
                failure.probability);
        }
        PC_CHECK(exact.converged);
        PC_CHECK(exact.cost_complete);
        PC_CHECK(exact.success_probability > 1.0 - 1e-9);
        PC_CHECK(exact.failure_probability < 1e-12);
        PC_CHECK(exact.action_not_applied_probability < 1e-12);
        PC_CHECK(exact.no_matching_edge_probability < 1e-12);
        PC_CHECK(exact.unresolved_probability < 1e-12);
        PC_CHECK(std::fabs(
                     exact.total_expected_cost -
                     solved.evaluated_policy_cost) < 1e-7);
        const SimulationSummaryInternal observed_summary =
            run_compiled(
                observed_session, strategy,
                {{"veiled_chaos", 1.0}}, 10000, 246813579);
        PC_CHECK(
            observed_summary.success_count ==
            observed_summary.completed_runs);
        PC_CHECK(observed_summary.failure_count == 0);
        PC_CHECK(observed_summary.action_not_applied_count == 0);
        PC_CHECK(observed_summary.no_matching_edge_count == 0);
        std::printf(
            "solver quotient choice audit: offered=%zu "
            "projected_collision_classes=%u max_offered_per_class=%zu\n",
            compiled_offered_mods.size(), projected_collision_classes,
            max_offered_mods_per_projected_successor);
    }

    /* Pick one ordinary crafted prefix and suffix with no group conflict, then
     * force the deterministic Multimod option as the only candidate. */
    std::uint32_t finish_prefix = kNoId;
    std::uint32_t finish_suffix = kNoId;
    std::uint32_t finish_multimod = kNoId;
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
        if (action.params.type == ActionType::Bench &&
            (action.sets_flags & kFlagMultimod) != 0) {
            finish_multimod = index;
        }
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
            finish_suffix == kNoId &&
            !conflicts(registry.actions[finish_prefix].params.mod_id,
                       action.params.mod_id)) {
            finish_suffix = index;
        }
    }
    PC_CHECK(finish_prefix != kNoId);
    PC_CHECK(finish_suffix != kNoId);
    PC_CHECK(finish_multimod != kNoId);
    if (finish_prefix != kNoId && finish_suffix != kNoId &&
        finish_multimod != kNoId) {
        GoalSpec finish_goal;
        for (const std::uint32_t action_index :
             {finish_prefix, finish_suffix}) {
            GoalSlot finish_slot;
            finish_slot.family_id = session->family_id[
                registry.actions[action_index].params.mod_id];
            finish_goal.slots.push_back(finish_slot);
        }
        GoalSlot multimod_slot;
        multimod_slot.family_id = session->family_id[
            registry.actions[finish_multimod].params.mod_id];
        finish_goal.slots.push_back(multimod_slot);
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

/* S8.4R.3 focused fixture: the final goal is rare, and the solver discovers
 * the exact direct-Regal Imprint retry from a one-mod magic carrier. */
void run_imprint_gate(const char* artifact_dir) {
    if (artifact_dir == nullptr) {
        std::printf("solver Imprint fixture skipped (missing path)\n");
        return;
    }
    const std::string dir = artifact_dir;
    std::string manifest_text;
    std::string strings_text;
    std::string game_text;
    if (!read_text_file(dir + "/manifest.json", manifest_text) ||
        !read_text_file(dir + "/strings.json", strings_text) ||
        !read_text_file(dir + "/game-data.json", game_text)) {
        std::printf("solver Imprint fixture skipped (unreadable)\n");
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
        std::printf("solver Imprint fixture: %s\n", ex.what());
        PC_CHECK(false);
        return;
    }

    PC_CHECK(data->bestiary_action_by_id.contains("bestiary:imprint"));
    PC_CHECK(data->bestiary_action_by_id.contains(
        "bestiary:restore_imprint"));
    std::uint32_t carrier_mod = kNoId;
    std::uint32_t target_mod = kNoId;
    for (std::uint32_t mod = 0; mod < session->mod_count; ++mod) {
        if (!pc_bitset_test(session->normal_random_roll_mask.data(), mod) ||
            !pc_bitset_test(session->positive_base_weight_mask.data(), mod)) {
            continue;
        }
        if (session->gen_type[mod] == PC_SIDE_PREFIX &&
            carrier_mod == kNoId) {
            carrier_mod = mod;
        } else if (session->gen_type[mod] == PC_SIDE_SUFFIX &&
                   target_mod == kNoId) {
            target_mod = mod;
        }
        if (carrier_mod != kNoId && target_mod != kNoId) break;
    }
    PC_CHECK(carrier_mod != kNoId);
    PC_CHECK(target_mod != kNoId);
    if (carrier_mod == kNoId || target_mod == kNoId) return;

    const ActionRegistry complete = build_action_registry(*session);
    ActionRegistry registry;
    for (const char* id : {"augment", "regal"}) {
        const auto found = complete.index_by_id.find(id);
        PC_CHECK(found != complete.index_by_id.end());
        if (found == complete.index_by_id.end()) return;
        const std::uint32_t index =
            static_cast<std::uint32_t>(registry.actions.size());
        registry.index_by_id.emplace(id, index);
        registry.actions.push_back(complete.actions.at(found->second));
    }

    GoalSpec goal;
    goal.rarity = PC_RARITY_RARE;
    goal.automatic_candidates = true;
    GoalSlot carrier;
    carrier.group_id = session->primary_group[carrier_mod];
    GoalSlot target;
    target.group_id = session->primary_group[target_mod];
    goal.slots = {carrier, target};
    pc_item_state magic;
    pc_item_clear(&magic);
    magic.rarity = PC_RARITY_MAGIC;
    PC_CHECK(pc_item_add_mod(
                 &magic, PC_SIDE_PREFIX, carrier_mod,
                 static_cast<std::uint16_t>(
                     session->primary_group[carrier_mod]),
                 0, nullptr) == PC_RESULT_OK);

    const std::unordered_map<std::string, double> prices{
        {"augment", 0.1},
        {"regal", 100.0},
        {"beast:craicic-croaker", 0.01},
        {"beast:rare", 0.01},
    };

    /* Without a prior certified carrier upper, a depth-one fallback is an
     * honest open grammar rather than permission to publish the first useful
     * Imprint program. Keep that refusal separate from the mechanically closed
     * compiler fixture below. */
    CalcContext depth_limited_calc(session, goal, registry);
    SolveOptions depth_limited_options;
    depth_limited_options.consider_imprint_programs = true;
    depth_limited_options.max_imprint_program_depth = 1;
    depth_limited_options.max_imprint_program_work = 16;
    const SolveResult depth_limited = solve(
        depth_limited_calc, magic, prices, depth_limited_options);
    PC_CHECK(!depth_limited.converged);
    PC_CHECK(std::any_of(
        depth_limited.diagnostics.automatic_candidate_witnesses.begin(),
        depth_limited.diagnostics.automatic_candidate_witnesses.end(),
        [](const std::string& witness) {
            return witness.find("max_imprint_program_depth") !=
                   std::string::npos;
        }));

    CalcContext calc(session, goal, registry);
    SolveOptions solve_options;
    solve_options.consider_imprint_programs = true;
    solve_options.max_imprint_program_depth = 3;
    solve_options.max_imprint_program_work = 16;
    const SolveResult solved = solve(calc, magic, prices, solve_options);
    report_compile_solve_issue("automatic imprint retry", solved);
    PC_CHECK(solved.converged);
    PC_CHECK(solved.start_state < solved.policy.size());
    const std::uint32_t selected = solved.policy[solved.start_state].index;
    PC_CHECK(selected < calc.operators().size());
    if (selected >= calc.operators().size()) return;
    const PlannerOperator& planner = calc.operators().at(selected);
    PC_CHECK(planner.kind == PlannerOperatorKind::FixedOption);
    PC_CHECK(planner.option_kind == FixedOptionKind::ImprintRetry);
    PC_CHECK(planner.automatic_kind == AutomaticCandidateKind::Imprint);
    PC_CHECK(planner.primitive_program.size() == 1);
    PC_CHECK(planner.primitive_program.front() ==
             registry.index_by_id.at("regal"));

    const OptionKernel& kernel = calc.option_kernel(
        solved.start_state, selected);
    PC_CHECK(kernel.supported);
    PC_CHECK(kernel.legal);
    PC_CHECK(kernel.terminates_almost_surely);
    PC_CHECK(!kernel.retry_states.empty());
    bool has_exact_rare_exit = false;
    for (const OutcomeEntry& exit : kernel.exits) {
        if (exit.state == kNoId || exit.state == solved.start_state) continue;
        const AbstractState& successor = calc.state(exit.state);
        if (successor.rarity == PC_RARITY_RARE &&
            successor.slot_status[1] == static_cast<std::uint8_t>(
                GoalSlotStatus::Satisfied) &&
            calc.is_goal_state(successor)) {
            has_exact_rare_exit = true;
        }
    }
    PC_CHECK(has_exact_rare_exit);
    const auto quantity = [&](const char* key) {
        for (const auto& [resource, amount] : kernel.expected_resources) {
            if (resource == key) return amount;
        }
        return 0.0;
    };
    PC_CHECK(quantity("regal") == 1.0);
    PC_CHECK(quantity("beast:craicic-croaker") == 1.0);
    PC_CHECK(quantity("beast:rare") == 3.0);
    CalcContext work_limited_calc(session, goal, registry);
    SolveOptions work_limited_options;
    work_limited_options.consider_imprint_programs = true;
    work_limited_options.max_imprint_program_depth = 3;
    work_limited_options.max_imprint_program_work = 1;
    const SolveResult work_limited = solve(
        work_limited_calc, magic, prices, work_limited_options);
    PC_CHECK(std::any_of(
        work_limited.diagnostics.automatic_candidate_witnesses.begin(),
        work_limited.diagnostics.automatic_candidate_witnesses.end(),
        [](const std::string& witness) {
            return witness.find("max_imprint_program_work") !=
                   std::string::npos;
        }));

    const std::string strategy = compile_policy_strategy_json(
        calc, solved, "automatic-imprint-to-rare");
    PC_CHECK(strategy.find("\"type\":\"bestiary:imprint\"") !=
             std::string::npos);
    PC_CHECK(strategy.find("\"type\":\"augment\"") ==
             std::string::npos);
    PC_CHECK(strategy.find("\"type\":\"bestiary:restore_imprint\"") !=
             std::string::npos);
    PC_CHECK(strategy.find("\"type\":\"regal\"") !=
             std::string::npos);
    PC_CHECK(strategy.find("_imprint_route") != std::string::npos);

    auto compiled = compile_strategy_json(
        session, strategy.data(), strategy.size());
    const ActionRegistry accounting_registry =
        build_action_registry(*session);
    bool resolved_create = false;
    bool resolved_restore = false;
    for (const StrategyNode& node : compiled->nodes) {
        if (node.action_type != kStrategyBestiaryImprintOperation &&
            node.action_type !=
                kStrategyBestiaryRestoreImprintOperation) {
            continue;
        }
        const ResolvedStrategyOperation operation =
            resolve_strategy_operation(
                node, accounting_registry, *session);
        PC_CHECK(operation.kind ==
                 ResolvedStrategyOperationKind::Bestiary);
        PC_CHECK(operation.descriptor_index ==
                 node.bestiary_action_index);
        PC_CHECK(resolve_strategy_action(node, accounting_registry) ==
                 kNoId);
        if (node.action_type == kStrategyBestiaryImprintOperation) {
            resolved_create = true;
        } else {
            resolved_restore = true;
        }
    }
    PC_CHECK(resolved_create);
    PC_CHECK(resolved_restore);
    auto economy = std::make_shared<EconomyImpl>();
    economy->id = "s8.4r.3-focused";
    economy->prices = prices;
    StrategyEvalOptions eval_options;
    eval_options.economy = economy;
    const StrategyEvalResult exact =
        evaluate_strategy(*compiled, eval_options);
    PC_CHECK(exact.converged);
    PC_CHECK(exact.cost_complete);
    PC_CHECK(std::fabs(
                 exact.total_expected_cost -
                 solved.values[solved.start_state]) < 1e-8);
    PC_CHECK(exact.expected_consumption.at(
                 "beast:craicic-croaker") > 1.0);
    PC_CHECK(std::fabs(
                 exact.expected_consumption.at("beast:rare") -
                 3.0 * exact.expected_consumption.at(
                           "beast:craicic-croaker")) < 1e-8);
    PC_CHECK(std::any_of(
        exact.action_totals.begin(), exact.action_totals.end(),
        [](const StrategyEvalActionTotal& action) {
            return action.id == "bestiary:imprint";
        }));
    PC_CHECK(std::any_of(
        exact.action_totals.begin(), exact.action_totals.end(),
        [](const StrategyEvalActionTotal& action) {
            return action.id == "bestiary:restore_imprint";
        }));
    SimulatorImpl simulator;
    simulator.session = session;
    simulator.strategy = compiled;
    simulator.economy = economy;
    prepare_simulator_runtime(simulator);
    SimulationOptionsInternal simulation_options;
    simulation_options.target_runs = 64;
    simulation_options.seed = 20260718;
    simulation_options.max_actions_per_run = 100000;
    run_simulator_chunk(simulator, simulation_options, 64);
    PC_CHECK(simulator.summary.completed_runs == 64);
    PC_CHECK(simulator.summary.success_count == 64);
    PC_CHECK(simulator.summary.failure_count == 0);
    PC_CHECK(simulator.summary.action_limit_count == 0);
    PC_CHECK(simulator.summary.action_not_applied_count == 0);
    PC_CHECK(simulator.summary.no_matching_edge_count == 0);
    std::uint64_t imprint_count = 0;
    std::uint64_t restore_count = 0;
    for (std::size_t node = 0; node < compiled->nodes.size(); ++node) {
        if (compiled->nodes[node].action_type ==
            kStrategyBestiaryImprintOperation) {
            imprint_count += simulator.action_counts[node];
        } else if (compiled->nodes[node].action_type ==
                   kStrategyBestiaryRestoreImprintOperation) {
            restore_count += simulator.action_counts[node];
        }
    }
    PC_CHECK(imprint_count == restore_count + 64);
    PC_CHECK(simulator.action_descriptor_counts["augment"] == 0);
    PC_CHECK(simulator.action_descriptor_counts["regal"] == imprint_count);
    PC_CHECK(simulator.action_descriptor_counts["bestiary:imprint"] ==
             imprint_count);
    PC_CHECK(simulator.action_descriptor_counts[
                 "bestiary:restore_imprint"] == restore_count);
    PC_CHECK(simulator.material_counts["beast:craicic-croaker"] ==
             imprint_count);
    PC_CHECK(simulator.material_counts["beast:rare"] == 3 * imprint_count);
    std::printf(
        "solver Imprint R3 fixture: V=%.6f creates=%llu restores=%llu runs=64\n",
        solved.values[solved.start_state],
        static_cast<unsigned long long>(imprint_count),
        static_cast<unsigned long long>(restore_count));
}

} // namespace

void run_solver_compile_tests(const char* artifact_dir) {
    run_policy_description_test();
    run_finder_request_binding_tests();
    run_finder_default_success_regression();
    run_solver_return_bridge_tests();
    run_condition_expr_tests();
    run_policy_route_coalescing_tests();
    run_future_observed_choice_compile_test();
    run_structured_observation_route_tests();
    run_synthetic_gate();
    run_closed_coarse_certification_domain_test();
    run_artifact_gate(artifact_dir);
    run_imprint_gate(artifact_dir);
}

void run_solver_finder_binding_tests() {
    run_finder_request_binding_tests();
    run_finder_default_success_regression();
}

void run_solver_return_bridge_lifecycle_tests();

void run_nonempty_dirty_composition_tests() {
    auto session = make_compile_session();
    auto registry = build_action_registry(*session);
    const auto exalt = registry.index_by_id.at("exalt");
    const auto annul = registry.index_by_id.at("annul");
    GoalSpec goal;
    goal.rarity = PC_RARITY_RARE;
    for (const auto family : {100u, 104u}) {
        GoalSlot slot; slot.family_id = family; slot.min_tier = 1;
        goal.slots.push_back(slot);
    }
    CalcContext calc(session, goal, registry, {exalt, annul}, false, true, true,
        std::nullopt, {}, false, {}, true);
    pc_item_state frozen; pc_item_clear(&frozen); frozen.rarity = PC_RARITY_RARE;
    PC_CHECK(pc_item_add_mod(&frozen, PC_SIDE_PREFIX, 0, session->primary_group[0],
        PC_MOD_SLOT_FRACTURED, nullptr) == PC_RESULT_OK);
    SolveResult old;
    old.start_state = calc.intern_item(frozen);
    old.has_exact_start_item = old.policy_available = true;
    old.exact_start_item = frozen;
    old.policy_status = SolvePolicyStatus::BoundedFeasible;
    old.options.allow_economic_restart = false;
    old.options.goal_progress_gated_reforges = true;
    std::vector<std::uint32_t> walk{old.start_state};
    std::set<std::uint32_t> seen{old.start_state};
    for (std::size_t cursor = 0; cursor < walk.size(); ++cursor) {
        const auto state = walk[cursor], n = calc.state_count();
        old.values.resize(n, 0); old.policy.resize(n); old.policy_reachable.resize(n, 0);
        old.goal_states.resize(n, 0); old.expanded.resize(n, 0); old.policy_reachable[state] = 1;
        if (calc.is_goal_state(calc.state(state))) { old.goal_states[state] = 1; continue; }
        const auto action = state == old.start_state ? exalt : annul;
        old.policy[state] = PolicyOperatorRef{action}; old.expanded[state] = 1;
        const auto law = calc.outcomes(state, action, false);
        PC_CHECK(law.supported && law.applicable);
        for (const auto& exit : law.entries)
            if (exit.probability > 0 && seen.insert(exit.state).second) walk.push_back(exit.state);
    }
    auto dirty = frozen;
    PC_CHECK(pc_item_add_mod(&dirty, PC_SIDE_SUFFIX, 6, session->primary_group[6], 0, nullptr) == PC_RESULT_OK);
    const auto dirty_state = calc.intern_item(dirty);
    PC_CHECK(seen.contains(dirty_state));
    const auto removal = calc.outcomes(dirty_state, annul, false);
    PC_CHECK(removal.supported && removal.applicable && removal.entries.size() == 1);
    PC_CHECK(removal.entries.front().state == old.start_state);
    PC_CHECK(std::abs(removal.entries.front().probability - 1) < 1e-12);
    const auto scour = registry.index_by_id.at("scour");
    const auto recovery = calc.outcomes(dirty_state, scour, false);
    PC_CHECK(recovery.supported && recovery.applicable && recovery.entries.size() == 1);
    pc_item_state recovered;
    PC_CHECK(calc.materialize(recovery.entries.front().state, recovered));
    PC_CHECK(recovered.rarity == PC_RARITY_MAGIC && recovered.prefix_count == 1 && recovered.suffix_count == 0);
    PC_CHECK(recovered.prefixes[0].mod_id == frozen.prefixes[0].mod_id &&
        recovered.prefixes[0].flags == PC_MOD_SLOT_FRACTURED);
    PC_CHECK(std::abs(recovery.entries.front().probability - 1) < 1e-12);
    // A retained goal fracture prevents the native zero-progress retry
    // basin. Keep the gated caller scope; only terminal aggregation differs
    // from the complete ungated physical law, with no free reset or lost mass.
    const auto chaos=registry.index_by_id.at("chaos");
    const auto physical_redraw=calc.outcomes(old.start_state,chaos,false);
    const auto gated_redraw=calc.outcomes(old.start_state,chaos,true);
    PC_CHECK(physical_redraw.supported && gated_redraw.supported && gated_redraw.goal_progress_gated);
    PC_CHECK(gated_redraw.gated_retry_probability==0);
    std::map<std::uint32_t,double> physical_mass,gated_mass;
    const auto aggregate=[&](const auto& law,auto& mass) {
        for (const auto& exit:law.entries) {
            const auto& state=calc.state(exit.state);
            PC_CHECK(state.goal_progress_retry_basin==0 && state.fractured_goal_mask!=0);
            mass[calc.is_goal_state(state)?kNoId:exit.state]+=exit.probability;
        }
    };
    aggregate(physical_redraw,physical_mass); aggregate(gated_redraw,gated_mass);
    PC_CHECK(physical_mass.size()==gated_mass.size());
    for (const auto& [state,mass]:physical_mass) PC_CHECK(std::abs(mass-gated_mass[state])<1e-12);
    old.values.resize(calc.state_count(), 0); old.policy.resize(calc.state_count());
    old.policy_reachable.resize(calc.state_count(), 0); old.goal_states.resize(calc.state_count(), 0);
    old.expanded.resize(calc.state_count(), 0);
    PolicyCompilationTelemetry old_compilation;
    const auto old_graph = compile_policy_strategy_json(calc, old, "native frozen-goal reference",
        &old_compilation, old.options.max_strategy_json_bytes, nullptr, old.options.max_solver_owned_bytes,
        PolicyRouteDefaultMode::CertificationFailClosed);
    auto local = old;
    local.start_state = dirty_state; local.exact_start_item = dirty;
    local.policy_reachable.assign(calc.state_count(), 0);
    local.policy_reachable[dirty_state] = 1;
    const auto local_graph = compile_policy_strategy_json(calc, local, "native local paid removal",
        nullptr, old.options.max_strategy_json_bytes, nullptr, old.options.max_solver_owned_bytes,
        PolicyRouteDefaultMode::CertificationFailClosed);
    const auto combined = compile_dirty_continuation_strategy_json(calc, local_graph, old_graph,
        {dirty_state}, {old.start_state}, old.options);
    const std::unordered_map<std::string, double> prices{{"exalt", 2}, {"annul", 5}, {"base", 1}};
    const auto before = evaluate_compiled(session, old_graph, prices);
    const auto after = evaluate_compiled(session, combined, prices);
    PC_CHECK(before.converged && before.cost_complete && after.converged && after.cost_complete);
    PC_CHECK(after.failure_probability == 0 && after.no_matching_edge_probability == 0 &&
        after.action_not_applied_probability == 0 && after.stop_probability == 0);
    PC_CHECK(std::abs(after.success_probability - 1) < 1e-12);
    PC_CHECK(std::abs(after.total_expected_cost - before.total_expected_cost) < 1e-9);
    PC_CHECK(after.total_expected_cost > 2); // all failed additions still pay native removal
    // Genuine second-generation improvement: paid Scour/Regal/Annul renewal,
    // then the complete Exalt/Annul controller above, then paid Scour -> Magic
    // acquisition -> Regal. Both compositions enter at the original root.
    const auto compile_closed = [&](const auto& choose, const char* label,
            PolicyCompilationTelemetry& metadata, std::vector<std::uint32_t>& domain) {
        auto selected = old;
        selected.policy_reachable.assign(calc.state_count(), 0);
        selected.expanded.assign(calc.state_count(), 0);
        std::vector<std::uint32_t> pending{old.start_state};
        std::set<std::uint32_t> visited{old.start_state};
        for (std::size_t cursor = 0; cursor < pending.size(); ++cursor) {
            const auto state = pending[cursor], n = calc.state_count();
            selected.values.resize(n, 0); selected.policy.resize(n);
            selected.policy_reachable.resize(n, 0); selected.goal_states.resize(n, 0);
            selected.expanded.resize(n, 0); selected.policy_reachable[state] = 1;
            if (calc.is_goal_state(calc.state(state))) { selected.goal_states[state] = 1; continue; }
            pc_item_state item; PC_CHECK(calc.materialize(state, item));
            const auto action = choose(item);
            selected.policy[state] = PolicyOperatorRef{action}; selected.expanded[state] = 1;
            domain.push_back(state);
            const auto law = calc.outcomes(state, action, false);
            PC_CHECK(law.supported && law.applicable);
            for (const auto& exit : law.entries)
                if (exit.probability > 0 && visited.insert(exit.state).second) pending.push_back(exit.state);
        }
        return compile_policy_strategy_json(calc, selected, label, &metadata,
            old.options.max_strategy_json_bytes, nullptr, old.options.max_solver_owned_bytes,
            PolicyRouteDefaultMode::CertificationFailClosed);
    };
    PolicyCompilationTelemetry expensive_metadata;
    std::vector<std::uint32_t> expensive_domain;
    auto previous_graph = compile_closed([&](const pc_item_state& item) {
        return item.rarity == PC_RARITY_MAGIC ? registry.index_by_id.at("regal") :
            item.prefix_count + item.suffix_count == 1 ? scour : annul;
    }, "paid native renewal", expensive_metadata, expensive_domain);
    auto previous_provenance = expensive_metadata.graph_local_provenance;
    auto all_prices = prices; all_prices["exalt"] = 1;
    all_prices.insert({{"chaos",100}, {"scour",1}, {"alteration",1}, {"augment",1}, {"regal",1}});
    double previous_cost = evaluate_compiled(session, previous_graph, all_prices).total_expected_cost;
    for (unsigned generation : {1u, 2u}) {
        PolicyCompilationTelemetry metadata;
        std::vector<std::uint32_t> domain;
        const auto replacement = compile_closed([&](const pc_item_state& item) {
            if (generation == 1) return item.prefix_count + item.suffix_count == 1 ? exalt : annul;
            if (item.rarity == PC_RARITY_RARE)
                return item.prefix_count + item.suffix_count == 1 ? scour : annul;
            bool suffix_goal = false;
            for (unsigned i = 0; i < item.suffix_count; ++i) suffix_goal |= item.suffixes[i].mod_id == 5;
            return registry.index_by_id.at(suffix_goal ? "regal" :
                item.prefix_count + item.suffix_count == 1 ? "augment" : "alteration");
        }, "closed native improvement", metadata, domain);
        const auto composed = compile_dirty_continuation_strategy_json(calc, replacement, previous_graph,
            domain, {}, old.options, &metadata, true, &previous_provenance);
        PC_CHECK(metadata.policy_decision_bindings.empty());
        PC_CHECK(metadata.graph_local_provenance.matches(composed));
        auto parsed = compile_strategy_json(session, composed.data(), composed.size());
        auto economy = std::make_shared<EconomyImpl>(); economy->prices = all_prices;
        StrategyEvalOptions query; query.economy = economy;
        query.graph_local_provenance = metadata.graph_local_provenance;
        for (const auto& declaration : query.graph_local_provenance.decisions)
            query.policy_decision_entries.push_back({declaration.compiled_node_id, kNoId, kNoId, {},
                declaration.selected_operator_identity, declaration.fixed_observed_choice_policy, true});
        const auto checked = evaluate_strategy(*parsed, query);
        PC_CHECK(checked.converged && checked.cost_complete && std::abs(checked.success_probability - 1) < 1e-12);
        PC_CHECK(checked.total_expected_cost < previous_cost - 1e-9);
        bool reached_local = false;
        for (const auto& entry : checked.policy_entries.entries) {
            PC_CHECK(entry.coarse_state == kNoId && entry.selected_operator == kNoId);
            PC_CHECK(entry.coarse_state_identity.empty());
            reached_local |= entry.globally_routable() && entry.compiled_node_id.starts_with(generation == 1 ? "dirty_" : "dirty2_");
        }
        PC_CHECK(reached_local);
        auto foreign = query; foreign.policy_decision_entries.front().coarse_state = 0;
        PC_CHECK(evaluate_strategy(*parsed, foreign).policy_entries.decisions.front().status == StrategyPolicyEntryStatus::InvalidRequest);
        auto stale = query; stale.graph_local_provenance.strategy_json += " ";
        PC_CHECK(evaluate_strategy(*parsed, stale).policy_entries.certified_entries == 0);
        auto interior = query;
        interior.policy_decision_entries = {{"policy_route_root", kNoId, kNoId, {}, {1}, false, true}};
        PC_CHECK(evaluate_strategy(*parsed, interior).policy_entries.certified_entries == 0);
        std::printf("graph-local private generation %u: %.12g -> %.12g; entries=%u\n",
            generation, previous_cost, checked.total_expected_cost, checked.policy_entries.certified_entries);
        previous_graph = composed; previous_provenance = metadata.graph_local_provenance;
        previous_cost = checked.total_expected_cost;
    }
    // A new closed Magic acquisition domain enters only through the global
    // parent router. Paid Regal's complete native outcomes feed the existing
    // closed Rare controller; no old-root scalar or free rarity cast is used.
    auto magic_local=old;
    const auto magic_state=calc.intern_item(recovered);
    const auto regal=registry.index_by_id.at("regal");
    const auto promotion=calc.outcomes(magic_state,regal,false);
    PC_CHECK(promotion.supported && promotion.applicable);
    for (const auto& exit:promotion.entries) PC_CHECK(seen.contains(exit.state));
    magic_local.start_state=magic_state; magic_local.exact_start_item=recovered;
    magic_local.values.resize(calc.state_count(),0); magic_local.policy.resize(calc.state_count());
    magic_local.policy_reachable.resize(calc.state_count(),0); magic_local.expanded.resize(calc.state_count(),0);
    magic_local.goal_states.resize(calc.state_count(),0);
    magic_local.policy[magic_state]=PolicyOperatorRef{regal};
    magic_local.policy_reachable[magic_state]=magic_local.expanded[magic_state]=1;
    const auto magic_graph=compile_policy_strategy_json(calc,magic_local,"paid Magic entry",
        nullptr,old.options.max_strategy_json_bytes,nullptr,old.options.max_solver_owned_bytes,
        PolicyRouteDefaultMode::CertificationFailClosed);
    std::vector<std::uint32_t> closed{magic_state};
    for (const auto state:walk) if (!old.goal_states[state]) closed.push_back(state);
    const auto closed_graph=compile_dirty_continuation_strategy_json(calc,magic_graph,magic_graph,
        closed,{},old.options,nullptr,true);
    auto magic_prices=prices; magic_prices.emplace("regal",3.0);
    const auto magic_before=evaluate_compiled(session,magic_graph,magic_prices);
    const auto magic_after=evaluate_compiled(session,closed_graph,magic_prices);
    PC_CHECK(magic_after.converged && magic_after.cost_complete);
    PC_CHECK(magic_after.success_probability==1 && magic_after.action_not_applied_probability==0);
    PC_CHECK(std::abs(magic_before.total_expected_cost-magic_after.total_expected_cost)<1e-9);
    PC_CHECK(std::abs(magic_before.expected_actions-magic_after.expected_actions)<1e-9);
    PC_CHECK(closed_graph.find("\"id\":\"dirty_return\"")==std::string::npos);
    bool refused = false;
    auto wrong_goal = old_graph;
    const auto goal_at = wrong_goal.find("\"min_tier\":1");
    PC_CHECK(goal_at != std::string::npos);
    if (goal_at != std::string::npos) {
        wrong_goal.replace(goal_at, std::string("\"min_tier\":1").size(), "\"min_tier\":2");
        try { (void)compile_dirty_continuation_strategy_json(calc, local_graph, wrong_goal,
            {dirty_state}, {old.start_state}, old.options); }
        catch (const std::exception&) { refused = true; }
        PC_CHECK(refused);
    }
    refused = false;
    try { (void)compile_dirty_continuation_strategy_json(calc, local_graph, old_graph,
        {dirty_state}, {dirty_state}, old.options); }
    catch (const std::exception&) { refused = true; }
    PC_CHECK(refused);
    auto virtual_state = calc.state(dirty_state); virtual_state.goal_progress_retry_basin = 1;
    const auto virtual_id = calc.intern_state(virtual_state);
    refused = false;
    try { (void)compile_dirty_continuation_strategy_json(calc, local_graph, old_graph,
        {virtual_id}, {old.start_state}, old.options); }
    catch (const std::exception&) { refused = true; }
    PC_CHECK(refused);
    auto tiny = old.options; tiny.max_solver_owned_bytes = 128;
    refused = false;
    try { (void)compile_dirty_continuation_strategy_json(calc, local_graph, old_graph,
        {dirty_state}, {old.start_state}, tiny); }
    catch (const SolverResourceLimit&) { refused = true; }
    PC_CHECK(refused);
}

void run_solver_return_bridge_tests() {
    run_nonempty_dirty_composition_tests();
    auto session = make_compile_session();
    auto registry = build_action_registry(*session);
    const auto exalt = registry.index_by_id.at("exalt");
    const auto annul = registry.index_by_id.at("annul");
    GoalSpec goal;
    GoalSlot slot;
    slot.family_id = 100;
    slot.min_tier = 1;
    goal.slots.push_back(slot);
    goal.rarity = PC_RARITY_RARE;
    CalcContext calc(session, goal, registry, {exalt, annul});
    pc_item_state anchor;
    pc_item_clear(&anchor);
    anchor.rarity = PC_RARITY_RARE;
    SolveResult authored;
    authored.start_state = calc.intern_item(anchor);
    authored.has_exact_start_item = authored.policy_available = true;
    authored.exact_start_item = anchor;
    authored.policy_status = SolvePolicyStatus::BoundedFeasible;
    authored.options.allow_economic_restart = false;
    std::vector<std::uint32_t> pending{authored.start_state};
    std::set<std::uint32_t> seen{authored.start_state};
    for (std::size_t cursor = 0; cursor < pending.size(); ++cursor) {
        const auto state = pending[cursor];
        authored.values.resize(calc.state_count(), kInfinity);
        authored.policy.resize(calc.state_count());
        authored.policy_reachable.resize(calc.state_count());
        authored.goal_states.resize(calc.state_count());
        authored.expanded.resize(calc.state_count());
        authored.policy_reachable[state] = 1;
        if (calc.is_goal_state(calc.state(state))) {
            authored.values[state] = 0;
            authored.goal_states[state] = 1;
            continue;
        }
        const auto selected = state == authored.start_state ? exalt : annul;
        authored.policy[state] = PolicyOperatorRef{selected};
        authored.expanded[state] = 1;
        const auto& row = calc.outcomes(state, selected, false);
        PC_CHECK(row.supported && row.applicable);
        double mass = 0;
        for (const auto& outcome : row.entries) {
            mass += outcome.probability;
            if (selected == annul) {
                PC_CHECK(calc.state(outcome.state).rarity == PC_RARITY_RARE);
                PC_CHECK(outcome.state == authored.start_state);
            }
            if (seen.insert(outcome.state).second) pending.push_back(outcome.state);
        }
        PC_CHECK(std::abs(mass - 1) < 1e-12);
    }
    const std::unordered_map<std::string, double> prices{{"exalt", 2}, {"annul", 5}, {"base", 1}};
    PolicyCompilationTelemetry compiled;
    const auto repeat = compile_policy_strategy_json(calc, authored, "native return test", &compiled,
        authored.options.max_strategy_json_bytes, nullptr, authored.options.max_solver_owned_bytes,
        PolicyRouteDefaultMode::CertificationFailClosed);
    std::string entry;
    for (const auto& binding : compiled.policy_decision_bindings)
        if (binding.coarse_state == authored.start_state) entry = binding.compiled_node_id;
    PC_CHECK(!entry.empty());
    const auto once = compile_first_return_strategy_json(repeat, repeat, entry, anchor,
        FirstReturnCompilationMode::OneShot, authored.options);
    const auto excursion = compile_first_return_strategy_json(repeat, repeat, entry, anchor,
        FirstReturnCompilationMode::PrivateExcursion, authored.options);
    const auto old_eval = evaluate_compiled(session, repeat, prices);
    const auto once_eval = evaluate_compiled(session, once, prices);
    const auto first_eval = evaluate_compiled(session, excursion, prices);
    PC_CHECK(old_eval.converged && old_eval.cost_complete);
    PC_CHECK(once_eval.converged && once_eval.cost_complete);
    PC_CHECK(first_eval.converged && first_eval.cost_complete);
    PC_CHECK(std::abs(old_eval.success_probability - 1) < 1e-12);
    PC_CHECK(std::abs(once_eval.success_probability - 1) < 1e-12);
    PC_CHECK(first_eval.stop_probability > 0 && first_eval.stop_probability < 1);
    PC_CHECK(std::abs(first_eval.success_probability + first_eval.stop_probability - 1) < 1e-12);
    PC_CHECK(first_eval.failure_probability == 0 && first_eval.unresolved_probability == 0);
    PC_CHECK(std::abs(once_eval.total_expected_cost - first_eval.total_expected_cost -
        first_eval.stop_probability * old_eval.total_expected_cost) < 1e-9);
    PC_CHECK(std::abs(old_eval.total_expected_cost - first_eval.total_expected_cost /
        first_eval.success_probability) < 1e-9);
    PC_CHECK(first_eval.total_expected_cost > 2); // paid failed rolls; no time-zero return
    // The ordinary search state ceiling does not secretly cap an explicitly
    // configured candidate evaluator. The full native checker still owns
    // properness, prices, routing and the aggregate remaining-memory ceiling.
    auto bounded_authored = authored;
    bounded_authored.values[authored.start_state] = old_eval.total_expected_cost;
    bounded_authored.upper_bound = bounded_authored.evaluated_policy_cost = old_eval.total_expected_cost;
    auto checker_options = authored.options;
    checker_options.max_discovered_states = 1;
    checker_options.max_solver_owned_bytes = 8ull << 30;
    const auto capped = refinement::assert_compiled_policy_exact(calc, bounded_authored,
        prices, checker_options, "candidate checker inherited state cap");
    PC_CHECK(capped.status == refinement::CompiledPolicyAssertionStatus::ResourceCap);
    checker_options.candidate_evaluation_limits = {2000000,10000000,40000000,4ull<<30};
    const auto independent = refinement::assert_compiled_policy_exact(calc, bounded_authored,
        prices, checker_options, "candidate checker independent limits");
    PC_CHECK(independent.executable && independent.proper && independent.zero_off_policy);
    PC_CHECK(independent.cost_reconciled && independent.evaluation.cost_complete);
    PC_CHECK(independent.evaluator_memory_budget == (4ull<<30));
    PC_CHECK(checker_options.max_discovered_states == 1);
    PC_CHECK(resolved_candidate_evaluation_limits(checker_options,128ull<<20,1ull<<30)
        .max_owned_bytes == (128ull<<20));
    auto wrong = anchor;
    wrong.rarity = PC_RARITY_NORMAL;
    bool refused = false;
    try { (void)compile_first_return_strategy_json(repeat, repeat, entry, wrong,
        FirstReturnCompilationMode::OneShot, authored.options); }
    catch (const std::exception&) { refused = true; }
    PC_CHECK(refused);
    auto tiny = authored.options;
    tiny.max_solver_owned_bytes = 128;
    refused = false;
    try { (void)compile_first_return_strategy_json(repeat, repeat, entry, anchor,
        FirstReturnCompilationMode::OneShot, tiny); }
    catch (const SolverResourceLimit&) { refused = true; }
    PC_CHECK(refused);
    // Actual removal law retains both a clean goal and loss of the target.
    pc_item_state clean = anchor;
    PC_CHECK(pc_item_add_mod(&clean, PC_SIDE_PREFIX, 0, session->primary_group[0],
        0, nullptr) == PC_RESULT_OK);
    PC_CHECK(calc.is_goal_state(calc.state(calc.intern_item(clean))));
    auto dirty = clean;
    PC_CHECK(pc_item_add_mod(&dirty, PC_SIDE_SUFFIX, 5, session->primary_group[5],
        0, nullptr) == PC_RESULT_OK);
    const auto dirty_id = calc.intern_item(dirty);
    PC_CHECK(solve_detail::ordinary_return_bridge_item(*session, anchor));
    PC_CHECK(solve_detail::ordinary_return_bridge_item(*session, clean));
    PC_CHECK(solve_detail::ordinary_return_bridge_item(*session, dirty));
    PC_CHECK(!calc.is_goal_state(calc.state(dirty_id)));
    const auto removal = calc.outcomes(dirty_id, annul, false);
    double clean_mass = 0, lost_mass = 0;
    for (const auto& edge : removal.entries) {
        PC_CHECK(calc.state(edge.state).rarity == PC_RARITY_RARE);
        PC_CHECK(calc.state(edge.state).prefix_count + calc.state(edge.state).suffix_count == 1);
        if (calc.is_goal_state(calc.state(edge.state))) clean_mass += edge.probability;
        else {
            lost_mass += edge.probability;
            const auto next = calc.outcomes(edge.state, annul, false);
            PC_CHECK(next.entries.size() == 1);
            PC_CHECK(next.entries.front().state == authored.start_state);
            PC_CHECK(next.entries.front().probability == 1);
        }
    }
    PC_CHECK(clean_mass == 0.5 && lost_mass == 0.5);
    auto protected_item = dirty;
    protected_item.prefixes[0].flags = PC_MOD_SLOT_FRACTURED;
    const auto protected_id = calc.intern_item(protected_item);
    PC_CHECK(!solve_detail::ordinary_return_bridge_item(*session, protected_item));
    PC_CHECK((calc.state(protected_id).flags & kFlagFractured) != 0);
    const auto protected_row = calc.outcomes(protected_id, annul, false);
    PC_CHECK(protected_row.entries.size() == 1);
    PC_CHECK((calc.state(protected_row.entries.front().state).flags & kFlagFractured) != 0);
    PC_CHECK(protected_row.entries.front().state != authored.start_state);
    auto different_context = anchor;
    different_context.quality = 20;
    PC_CHECK(!solve_detail::ordinary_return_bridge_item(*session, different_context));
    different_context = anchor;
    different_context.generic_influence_bits = 1;
    PC_CHECK(!solve_detail::ordinary_return_bridge_item(*session, different_context));
    auto offer = dirty;
    offer.prefixes[0].veiled_option_count = 1;
    offer.prefixes[0].veiled_option_mod_ids[0] = 0;
    PC_CHECK(!solve_detail::ordinary_return_bridge_item(*session, offer));
    session->metamod_type[0] = 0;
    PC_CHECK(!solve_detail::ordinary_return_bridge_item(*session, dirty));
    session->metamod_type[0] = -1;

    // An omitted positive-probability tail remains an actual failed route.
    auto incomplete = authored;
    incomplete.policy_reachable.assign(authored.policy_reachable.size(), 0);
    incomplete.policy_reachable[authored.start_state] = 1;
    const auto open_graph = compile_policy_strategy_json(calc, incomplete, "uncovered native mass", nullptr,
        authored.options.max_strategy_json_bytes, nullptr, authored.options.max_solver_owned_bytes,
        PolicyRouteDefaultMode::CertificationFailClosed);
    const auto open_eval = evaluate_compiled(session, open_graph, prices);
    PC_CHECK(open_eval.failure_probability + open_eval.no_matching_edge_probability +
        open_eval.action_not_applied_probability + open_eval.unresolved_probability > 0);
    PC_CHECK(open_eval.success_probability < 1);

    auto stale = repeat;
    const std::string scope_key = "\"solver_policy_scope\":\"";
    const auto scope_at = stale.find(scope_key);
    PC_CHECK(scope_at != std::string::npos);
    const auto scope_end = stale.find('"', scope_at + scope_key.size());
    stale.replace(scope_at + scope_key.size(), scope_end - scope_at - scope_key.size(), "stale");
    refused = false;
    try { (void)compile_first_return_strategy_json(repeat, stale, entry, anchor,
        FirstReturnCompilationMode::OneShot, authored.options); }
    catch (const std::exception&) { refused = true; }
    PC_CHECK(refused);
    auto stale_goal = repeat;
    const auto goal_at = stale_goal.find("\"min_tier\":1");
    PC_CHECK(goal_at != std::string::npos);
    stale_goal.replace(goal_at, std::string("\"min_tier\":1").size(), "\"min_tier\":2");
    refused = false;
    try { (void)compile_first_return_strategy_json(repeat, stale_goal, entry, anchor,
        FirstReturnCompilationMode::OneShot, authored.options); }
    catch (const std::exception&) { refused = true; }
    PC_CHECK(refused);

    // Mandatory work removes even a freshly rolled target before control
    // returns: q=1 is a finite one-shot policy but an improper repetition.
    auto compulsory = repeat;
    const std::string original_edge = "\"from\":\"" + entry + "\",\"to\":\"policy_route_root\"";
    const auto edge_at = compulsory.find(original_edge);
    PC_CHECK(edge_at != std::string::npos);
    compulsory.replace(edge_at, original_edge.size(), "\"from\":\"" + entry + "\",\"to\":\"mandatory_remove\"");
    const auto nodes_end = compulsory.find("],\"edges\":[");
    PC_CHECK(nodes_end != std::string::npos);
    compulsory.insert(nodes_end, ",{\"id\":\"mandatory_remove\",\"kind\":\"operation\",\"operation\":{\"type\":\"annul\"}}");
    const auto edges_end = compulsory.rfind("]}");
    PC_CHECK(edges_end != std::string::npos);
    compulsory.insert(edges_end, ",{\"id\":\"mandatory_exit\",\"from\":\"mandatory_remove\",\"to\":\"policy_route_root\",\"priority\":0,\"is_default\":true}");
    const auto q1_once = compile_first_return_strategy_json(compulsory, repeat, entry, anchor,
        FirstReturnCompilationMode::OneShot, authored.options);
    const auto q1_private = compile_first_return_strategy_json(compulsory, repeat, entry, anchor,
        FirstReturnCompilationMode::PrivateExcursion, authored.options);
    const auto q1_once_eval = evaluate_compiled(session, q1_once, prices);
    const auto q1_private_eval = evaluate_compiled(session, q1_private, prices);
    const auto q1_repeat_eval = evaluate_compiled(session, compulsory, prices);
    PC_CHECK(q1_once_eval.converged && q1_once_eval.cost_complete);
    PC_CHECK(std::abs(q1_once_eval.total_expected_cost - old_eval.total_expected_cost - 7) < 1e-9);
    PC_CHECK(std::abs(q1_private_eval.stop_probability - 1) < 1e-12);
    PC_CHECK(q1_private_eval.success_probability == 0);
    PC_CHECK(std::abs(q1_private_eval.total_expected_cost - 7) < 1e-12);
    PC_CHECK(q1_repeat_eval.success_probability == 0);
    PC_CHECK(q1_repeat_eval.unresolved_probability > 0 || !q1_repeat_eval.cost_complete);

    const auto chaos = registry.index_by_id.at("chaos");
    CalcContext gated(session, goal, registry, {chaos, annul});
    const auto gated_source = gated.intern_item(dirty);
    const auto gated_row = gated.outcomes(gated_source, chaos, true);
    PC_CHECK(gated_row.goal_progress_gated && gated_row.gated_retry_probability > 0);
    PC_CHECK(gated.state(gated_row.gated_retry_state).goal_progress_retry_basin != 0);
    PC_CHECK(gated.state(gated_row.gated_retry_state).prefix_count +
        gated.state(gated_row.gated_retry_state).suffix_count == 0);
    const auto raw_row = gated.outcomes(gated_source, chaos, false);
    double raw_zero_mass = 0;
    for (const auto& exit : raw_row.entries) {
        const auto& state = gated.state(exit.state);
        if (state.slot_status[0] == static_cast<std::uint8_t>(GoalSlotStatus::Satisfied)) continue;
        raw_zero_mass += exit.probability;
        PC_CHECK(state.goal_progress_retry_basin == 0);
        PC_CHECK(state.prefix_count + state.suffix_count > 0);
        PC_CHECK(action_legal(*session, registry.actions[annul], state));
    }
    PC_CHECK(std::abs(raw_zero_mass - gated_row.gated_retry_probability) < 1e-12);
    // The actual positive-mass failed rolls can be annulled physically;
    // their compressed retry carrier is a different, mandatory control state.

    const auto unveil = registry.index_by_id.at("unveil");
    CalcContext observed(session, goal, registry, {unveil});
    pc_item_state veiled = anchor;
    PC_CHECK(pc_item_add_mod(&veiled, PC_SIDE_PREFIX, 8, session->primary_group[8],
        PC_MOD_SLOT_VEILED, nullptr) == PC_RESULT_OK);
    const auto offer_row = observed.outcomes(observed.intern_item(veiled), unveil);
    PC_CHECK(offer_row.supported && !offer_row.choice_groups.empty());
    double offered_mass = 0;
    for (const auto& group : offer_row.choice_groups) offered_mass += group.probability;
    PC_CHECK(std::abs(offered_mass - 1) < 1e-12);
    PC_CHECK(!solve_detail::ordinary_return_bridge_item(*session, veiled));
    auto observed_graph = repeat;
    const auto op_at = observed_graph.find("\"type\":\"exalt\"");
    PC_CHECK(op_at != std::string::npos);
    observed_graph.replace(op_at, std::string("\"type\":\"exalt\"").size(), "\"type\":\"unveil\"");
    refused = false;
    try { (void)compile_first_return_strategy_json(observed_graph, repeat, entry, anchor,
        FirstReturnCompilationMode::OneShot, authored.options); }
    catch (const std::exception&) { refused = true; }
    PC_CHECK(refused);
    std::printf("native first-return: old=%.12g once=%.12g r=%.12g q=%.12g\n",
        old_eval.total_expected_cost, once_eval.total_expected_cost,
        first_eval.total_expected_cost, first_eval.stop_probability);
    run_solver_return_bridge_lifecycle_tests();
}

void run_solver_native_blocker_entry_tests(const char* artifact_dir) {
    std::string manifest,strings,game;
    PC_CHECK(read_text_file(std::string(artifact_dir)+"/manifest.json",manifest));
    PC_CHECK(read_text_file(std::string(artifact_dir)+"/strings.json",strings));
    PC_CHECK(read_text_file(std::string(artifact_dir)+"/game-data.json",game));
    const auto data=load_data_impl(manifest,strings,game);
    for (const char* base:{"Metadata/Items/Amulets/Amulet7","Metadata/Items/Rings/Ring10"}) {
        auto session=std::make_shared<SessionImpl>(); session->data=data;
        session->base_index=data->base_by_path.at(base); session->item_level=86; build_session(*session);
        const auto mod=[&](const char* key) {
            const auto found=data->mod_pos_by_key.find(key);
            if (found==data->mod_pos_by_key.end()) throw std::runtime_error(std::string("missing native modifier: ")+key);
            const auto pos=found->second;
            const auto local=session->session_id_by_global_id.find(data->mod_global_ids.at(pos));
            if (local==session->session_id_by_global_id.end()) throw std::runtime_error(std::string("modifier outside native pool: ")+base+" / "+key);
            return local->second;
        };
        auto registry=build_action_registry(*session);
        const bool amulet=std::string_view(base).find("Amulets")!=std::string_view::npos;
        GoalSpec goal; goal.rarity=PC_RARITY_RARE; goal.automatic_candidates=true;
        for (const char* key:{"ChaosResist6",amulet ? "AllResistances6" : "FireResist8"}) {
            GoalSlot slot; slot.family_id=session->family_id.at(mod(key)); slot.min_tier=1; goal.slots.push_back(slot);
        }
        {
            GoalSlot slot; slot.family_id=session->family_id.at(mod(amulet ? "LightningDamagePercent5" : "AllAttributes4")); slot.min_tier=1;
            goal.slots.push_back(slot);
        }
        const auto add=[&](pc_item_state& item,const char* key) {
            const auto id=mod(key);
            PC_CHECK(pc_item_add_mod(&item,session->gen_type[id],id,session->primary_group[id],0,nullptr)==PC_RESULT_OK);
        };
        pc_item_state source; pc_item_clear(&source); source.rarity=PC_RARITY_RARE;
        add(source,"AddedColdDamage1"); add(source,"AddedFireDamage1"); add(source,"ChaosResist6");
        add(source,amulet ? "LightningDamagePercent5" : "AllAttributes4");
        auto conflict=source;
        PC_CHECK(pc_item_remove_at(&conflict,PC_SIDE_PREFIX,1)==PC_RESULT_OK);
        add(conflict,"AddedLightningDamage1");
        std::vector<std::uint32_t> candidates;
        for (const char* key:{"exalt","annul","scour","regal","chaos"}) {
            const auto found=registry.index_by_id.find(key);
            if (found==registry.index_by_id.end()) throw std::runtime_error(std::string("missing native action: ")+key);
            candidates.push_back(found->second);
        }
        const auto& bench=registry.actions.at(registry.index_by_id.at("bench:EinharMasterAddedLightningDamage1"));
        std::vector<std::uint64_t> universe(session->words,0);
        for (std::uint32_t m=0;m<session->mod_count;++m) pc_bitset_set(universe.data(),m);
        CalcContext coarse(session,goal,registry,candidates,false,false,false,std::nullopt,{},true,universe);
        const auto old_source=coarse.intern_item(source),old_conflict=coarse.intern_item(conflict);
        PC_CHECK(old_source==old_conflict);
        PC_CHECK(!temporary_bench_source_observation_complete(coarse,old_source,bench));
        const auto observation=temporary_bench_conflict_observation(*session,bench);
        CalcContext observed(session,goal,registry,candidates,false,false,false,std::nullopt,{observation},true,universe);
        const auto entry=observed.intern_item(source),blocked=observed.intern_item(conflict);
        PC_CHECK(entry!=blocked);
        PC_CHECK(temporary_bench_source_observation_complete(observed,entry,bench));
        PC_CHECK(temporary_bench_source_observation_complete(observed,blocked,bench));
        PC_CHECK(observed.candidates()==coarse.candidates());
        std::unordered_map<std::string,double> prices{{"exalt",1},{"annul",5},{"scour",1},{"regal",1},{"chaos",20},
            {"bench:EinharMasterAddedLightningDamage1",0.1},{"remove_crafted_modifiers",0.1}};
        for (const auto& priced:registry.actions) if (priced.params.type==ActionType::Bench)
            for (const auto& key:priced.cost_keys) if (!prices.contains(key)) prices[key]=1000;
        const auto admit=[&](std::uint32_t state) {
            AutomaticAdmissionLimits limits; limits.prices=&prices;
            StateLocalAutomaticBatch batch;
            while (!observed.advance_state_local_automatic_candidates(state,limits,batch,1)) {}
            return batch;
        };
        const auto admitted=admit(entry);
        bool offered=false;
        for (auto index:admitted.admitted_operators) {
            const auto& op=observed.operators().at(index);
            if (op.option_kind!=FixedOptionKind::TemporaryBenchRepeat || op.setup_action==kNoId ||
                registry.actions.at(op.setup_action).id!=bench.id) continue;
            offered=true;
            const auto& kernel=observed.option_kernel(entry,index);
            PC_CHECK(kernel.supported && kernel.legal && kernel.terminates_almost_surely);
            double mass=0;
            for (const auto& exit:kernel.exits) {
                mass+=exit.probability;
                if (exit.state==kNoId) continue;
                PC_CHECK((observed.state(exit.state).flags & kFlagCraftedMod)==0);
            }
            PC_CHECK(std::abs(mass-1)<1e-12);
        }
        if (!offered) for (const auto& decision:admitted.decisions)
            if (decision.id.find("EinharMasterAddedLightningDamage1")!=std::string::npos)
                std::printf("native blocker decision %s: %s / %s\n",base,decision.id.c_str(),decision.evidence.reason.c_str());
        if (amulet) PC_CHECK(offered);
        else {
            // This actual Ring pool preserves the entry distinction, but its
            // existing automatic owner refuses the complete option kernel.
            // Keep that unknown outcome; it is not a cross-pool upper fixture.
            PC_CHECK(offered || std::any_of(admitted.decisions.begin(),admitted.decisions.end(),[&](const auto& decision) {
                return decision.id.find("EinharMasterAddedLightningDamage1")!=std::string::npos &&
                    decision.evidence.reason=="exact_kernel_unsupported" && !decision.admitted;
            }));
        }
        const auto refused=admit(blocked);
        PC_CHECK(std::none_of(refused.admitted_operators.begin(),refused.admitted_operators.end(),[&](auto index) {
            const auto& op=observed.operators().at(index);
            return op.option_kind==FixedOptionKind::TemporaryBenchRepeat && op.setup_action!=kNoId &&
                registry.actions.at(op.setup_action).id==bench.id;
        }));
        std::printf("native blocker entry %s: coarse=%u observed=%u conflicting=%u\n",base,old_source,entry,blocked);
        if (!amulet) {
            // Real clean Ring entry: a complete native program must compose
            // without exporting its private junk partition into the old graph.
            GoalSpec clean_goal; clean_goal.rarity=PC_RARITY_RARE; clean_goal.automatic_candidates=true;
            for (const char* key : {"IncreasedEvasionRating7","FireResist8","AddedColdDamage9","AllAttributes4"}) {
                GoalSlot slot; slot.family_id=session->family_id.at(mod(key)); slot.min_tier=1;
                clean_goal.slots.push_back(slot);
            }
            pc_item_state clean; pc_item_clear(&clean); clean.rarity=PC_RARITY_RARE;
            for (const char* key : {"AddedColdDamage9","FireResist8","AllAttributes4"}) add(clean,key);
            CalcContext local(session,clean_goal,registry,candidates,false,false,false,std::nullopt,{},true,universe);
            const auto source=local.intern_item(clean);
            AutomaticAdmissionLimits admission; admission.prices=&prices; admission.consider_imprint_programs=false;
            StateLocalAutomaticBatch options;
            while (!local.advance_state_local_automatic_candidates(source,admission,options,1)) {}
            std::string old_graph;
            PC_CHECK(read_text_file("docs/active/2026-09-13-execution-aware-proposals/strategies/ring-four-count.strategy.json",old_graph));
            unsigned composed_options=0;
            for (const auto index : options.admitted_operators) {
                const auto op=local.operators().at(index);
                if (op.option_kind!=FixedOptionKind::TemporaryBenchRepeat || op.automatic_kind!=AutomaticCandidateKind::CannotRoll) continue;
                const auto kernel=local.option_kernel(source,index);
                PC_CHECK(kernel.legal && kernel.terminates_almost_surely && kernel.retry_states.empty());
                SolveResult selected; selected.start_state=source; selected.exact_start_item=clean; selected.has_exact_start_item=true;
                selected.policy_available=true; selected.policy_status=SolvePolicyStatus::BoundedFeasible;
                selected.options.allow_economic_restart=false; selected.options.consider_imprint_programs=false;
                selected.options.solve_profile=SolveProfile::CalculatorProductV1;
                selected.values.assign(local.state_count(),kInfinity); selected.values[source]=1;
                selected.upper_bound=selected.evaluated_policy_cost=1; // unverified compiler bookkeeping only
                selected.policy.resize(local.state_count()); selected.policy[source]=PolicyOperatorRef{op.kind,index};
                selected.policy_reachable.assign(local.state_count(),0); selected.expanded.assign(local.state_count(),0);
                selected.goal_states.assign(local.state_count(),0); selected.policy_reachable[source]=selected.expanded[source]=1;
                std::vector<std::uint32_t> returns;
                for (const auto& exit:kernel.exits) {
                    PC_CHECK(exit.state!=kNoId);
                    if (local.is_goal_state(local.state(exit.state))) { selected.goal_states[exit.state]=1; selected.values[exit.state]=0; }
                    else returns.push_back(exit.state);
                }
                PolicyCompilationTelemetry metadata;
                const auto graph=compile_policy_strategy_json(local,selected,"clean native option",&metadata,
                    selected.options.max_strategy_json_bytes,nullptr,selected.options.max_solver_owned_bytes,
                    PolicyRouteDefaultMode::CertificationFailClosed);
                const auto original_metadata=metadata;
                const auto composed=compile_dirty_continuation_strategy_json(local,graph,old_graph,{source},returns,
                    selected.options,&metadata,returns.empty(),nullptr,index);
                PC_CHECK(metadata.graph_local_provenance.matches(composed));
                PC_CHECK(metadata.policy_decision_bindings.empty());
                PC_CHECK(metadata.graph_local_provenance.decisions.size()==1);
                PC_CHECK(metadata.graph_local_provenance.decisions.front().compiled_node_id=="dirty_entry_option_0");
                PC_CHECK(metadata.graph_local_provenance.decision_routers==std::vector<std::string>{"policy_route_root"});
                PC_CHECK(composed.find("dirty_entry_option_1")!=std::string::npos);
                PC_CHECK(composed.find("dirty_return")!=std::string::npos);
                bool missing_refused=false;
                auto missing=returns; missing.pop_back(); metadata=original_metadata;
                try { (void)compile_dirty_continuation_strategy_json(local,graph,old_graph,{source},missing,
                    selected.options,&metadata,missing.empty(),nullptr,index); }
                catch (const std::exception&) { missing_refused=true; }
                PC_CHECK(missing_refused);
                ++composed_options;
            }
            PC_CHECK(composed_options>0);
            std::printf("native clean Ring compact options: %u; complete exits and private-ID-free provenance\n",composed_options);
        }
    }
}

void run_solver_compile_metadata_tests() {
    run_policy_description_test();
}

void run_solver_imprint_tests(const char* artifact_dir) {
    run_imprint_gate(artifact_dir);
}


namespace poecraft::solver {
struct SolveWorkTestAccess {
    using Impl = SolveWork::Impl;
    static Impl& get(SolveWork& work) { return *work.impl_; }
};
}

void run_solver_growth_tests(const bool blocker) {
    PC_CHECK(exact_checker_state_budget(200000, 800000, 200000) == 200000);
    PC_CHECK(exact_checker_state_budget(0, 800000, 200000) == 0);
    PC_CHECK(exact_checker_state_budget(std::nullopt, 800000, 200000) == 800000);
    PC_CHECK(exact_checker_state_budget(std::nullopt, 0, 200000) == 200000);
    // Finite witness for the Bow4 crash: a checked incremental upper seed
    // arrives before lower preparation has initialized the native goal bitmap.
    // Exercise both an absent bitmap and a stale bitmap after state growth.
    for (const bool stale_bitmap : {false, true}) {
        auto session = make_compile_session();
        const auto registry = build_action_registry(*session);
        GoalSpec goal; goal.rarity = PC_RARITY_RARE;
        GoalSlot slot; slot.family_id = session->family_id[0]; slot.min_tier = 1;
        goal.slots.push_back(slot);
        CalcContext calc(session, goal, registry, {registry.index_by_id.at("chaos")});
        pc_item_state root; pc_item_clear(&root); root.rarity = PC_RARITY_RARE;
        SolveOptions options; apply_solve_profile_defaults(options, SolveProfile::CalculatorProductV1);
        SolveWork work(calc, root, {{"chaos",100}}, options);
        auto& impl = SolveWorkTestAccess::get(work);
        const auto root_id = calc.intern_item(root);
        auto successful = root;
        PC_CHECK(pc_item_add_mod(&successful, PC_SIDE_PREFIX, 0,
            session->primary_group[0], 0, nullptr) == PC_RESULT_OK);
        const auto goal_id = calc.intern_item(successful);
        PC_CHECK(!calc.is_goal_state(calc.state(root_id)) && calc.is_goal_state(calc.state(goal_id)));
        impl.output_incumbent.emplace();
        impl.output_incumbent->values.assign(calc.state_count(), 10);
        impl.output_incumbent->values[goal_id] = 0;
        impl.output_incumbent->policy_rows.assign(calc.state_count(), std::numeric_limits<std::uint64_t>::max());
        impl.incremental_upper_policy_pass = true;
        impl.result.goal_states.clear();
        if (stale_bitmap) impl.result.goal_states.assign(1, 1);
        PC_CHECK(impl.begin_focused_upper_solve());
        PC_CHECK(impl.result.goal_states.size() == calc.state_count());
        PC_CHECK(impl.result.goal_states[root_id] == 0 && impl.result.goal_states[goal_id] == 1);
        PC_CHECK(impl.result.values[root_id] == 10 && impl.result.values[goal_id] == 0);
        impl.abort_incremental_upper_policy_pass_for_bounded_finish();
        impl.finalized_result.emplace();
        impl.phase = SolvePhase::Done;
        impl.incremental_action_generation = true;
        impl.incremental_envelope_closed = false;
        impl.incremental_upper_policy_dirty = true;
        const auto previous_attempts = impl.incremental_upper_policy_passes_requested;
        PC_CHECK(!impl.begin_incremental_upper_policy_pass());
        PC_CHECK(impl.phase == SolvePhase::Done && impl.finalized_result.has_value());
        PC_CHECK(impl.incremental_upper_policy_passes_requested == previous_attempts);
    }
    SolveOptions product;
    apply_solve_profile_defaults(product, SolveProfile::CalculatorProductV1);
    apply_solve_state_budget_overrides(product, 0, 0, 0);
    PC_CHECK(product.max_states == 800000 && product.max_discovered_states == 800000);
    PC_CHECK(product.max_expanded_states == 200000 && product.max_policy_refinement_states == 200000);
    PC_CHECK(product.max_solver_owned_bytes == (1ull << 30));
    PC_CHECK(product.candidate_evaluation_limits.max_states == 0);
    auto explicit_caps = product;
    explicit_caps.candidate_evaluation_limits.max_states = 12345;
    apply_solve_state_budget_overrides(explicit_caps, 200000, 0, 0);
    PC_CHECK(explicit_caps.max_states == 200000 && explicit_caps.max_discovered_states == 200000 &&
             explicit_caps.max_expanded_states == 200000);
    PC_CHECK(explicit_caps.candidate_evaluation_limits.max_states == 12345);
    PC_CHECK(explicit_caps.max_solver_owned_bytes == product.max_solver_owned_bytes);
    apply_solve_state_budget_overrides(explicit_caps, 150000, 170000, 190000);
    PC_CHECK(explicit_caps.max_states == 150000 && explicit_caps.max_discovered_states == 170000 &&
             explicit_caps.max_expanded_states == 190000);
    SolveOptions ordinary;
    apply_solve_profile_defaults(ordinary, SolveProfile::Default);
    apply_solve_state_budget_overrides(ordinary, 0, 0, 0);
    PC_CHECK(ordinary.max_states == 200000 && ordinary.max_discovered_states == 200000);
    for (unsigned fixture = 0; fixture < 4u; ++fixture) {
        auto session = make_compile_session();
        auto data = std::const_pointer_cast<DataImpl>(session->data);
        // Explicitly distinguish absent metamods from ordinary bench crafts
        // in this synthetic data, as the loaded production schema does.
        data->metamod_no_attack_code = 20;
        data->metamod_no_caster_code = 21;
        data->metamod_prefixes_locked_code = 22;
        data->metamod_suffixes_locked_code = 23;
        data->metamod_multimod_code = 24;
        session->flags[9] = 1 << 1;
        if (fixture == 1) {
            for (auto& side : session->gen_type) side = 1 - side;
            std::swap(session->prefix_mask, session->suffix_mask);
        }
        session->bench_mod_ids = {9};
        const auto registry = build_action_registry(*session);
        const auto chaos = registry.index_by_id.at("chaos");
        GoalSpec goal; goal.rarity = PC_RARITY_RARE; goal.automatic_candidates = true;
        goal.automatic_candidate_kind_mask = automatic_candidate_kind_bit(AutomaticCandidateKind::EldritchSide) |
            automatic_candidate_kind_bit(AutomaticCandidateKind::TemporaryBenchBlocker);
        for (auto mod : {0u, 3u, 4u, 5u, 6u}) {
            GoalSlot slot; slot.family_id = session->family_id[mod]; slot.min_tier = 1;
            goal.slots.push_back(slot);
        }
        SolveOptions caps; apply_solve_profile_defaults(caps, SolveProfile::CalculatorProductV1);
        caps.max_discovered_states = 10000; caps.max_expanded_states = 10000;
        caps.max_state_action_rows = 100000; caps.max_transitions = 1000000;
        caps.max_reforge_work = 1000000; caps.max_solver_owned_bytes = 256ull << 20;
        std::unordered_map<std::string,double> prices{{"chaos",100},{"eldritch_chaos",3},
            {"eldritch_annul",2},{"eldritch_exalt",1},{"exalt",.1},{"scour",.01},{"bench:mod9",.01}};
        for (unsigned tier=1; tier<=4; ++tier) {
            prices["eldritch_ember:"+std::to_string(tier)] = .01;
            prices["eldritch_ichor:"+std::to_string(tier)] = .01;
        }
        pc_item_state root; pc_item_clear(&root); root.rarity = PC_RARITY_RARE;
        if (fixture == 2) for (auto mod : {3u,5u,6u})
            PC_CHECK(pc_item_add_mod(&root, static_cast<pc_affix_side>(session->gen_type[mod]),
                mod, session->primary_group[mod], 0, nullptr) == PC_RESULT_OK);
        // Reproduce the refused c23 carrier as an independent original root:
        // two target goals present, one missing, and both held goals present.
        if (fixture == 3) for (auto mod : {3u,4u,5u,6u})
            PC_CHECK(pc_item_add_mod(&root, static_cast<pc_affix_side>(session->gen_type[mod]),
                mod, session->primary_group[mod], 0, nullptr) == PC_RESULT_OK);
        for (unsigned proposal=blocker ? 3u : 2u; proposal<(blocker ? 4u : 3u); ++proposal) {
            CalcContext calc(session, goal, registry, {chaos}, false, false, false,
                std::nullopt, {}, false, {}, true);
            calc.set_solve_resource_caps(caps.max_discovered_states, caps.max_reforge_work,
                false, caps.max_solver_owned_bytes);
            PC_CHECK(product_completion_proposal_count(calc) == 3);
            const auto held = product_completion_held_side(calc, proposal);
            const auto variant = product_completion_proposal_variant(calc, proposal);
            SelectiveCompletionProducer producer(calc, root, prices, caps, variant, kNoId, held);
            for (unsigned i=0; i<40000 && !producer.done(); ++i) producer.advance();
            if (!producer.candidate()) std::printf("growth fixture=%u proposal=%u construction=%s\n",fixture,proposal,producer.status().c_str());
            PC_CHECK(producer.done() && producer.candidate().has_value());
            if (!producer.candidate()) continue;
            const auto& control = producer.candidate()->control;
            const auto graph = compile_finder_control_json(calc, root, control, caps);
            const auto prepared = prepare_finder_candidate(calc, session, root, graph, &control);
            PC_CHECK(prepared.ready());
            PC_CHECK(!prepare_finder_candidate(calc, session, root, graph).ready());
            if (!prepared.ready()) continue;
            auto economy = std::make_shared<EconomyImpl>(); economy->prices = prices;
            StrategyEvalOptions eval; eval.economy = economy;
            eval.max_states = caps.max_discovered_states; eval.max_pairs = caps.max_state_action_rows;
            eval.max_transitions = caps.max_transitions; eval.max_owned_bytes = caps.max_solver_owned_bytes;
            eval.max_reforge_work = caps.max_reforge_work;
            eval.continuation_entries.push_back({calc.intern_item(root),0,1,root,false});
            eval.graph_local_provenance.strategy_json = graph;
            for (unsigned node=0; node<control.nodes.size(); ++node) {
                const auto& cn = control.nodes[node];
                if (cn.kind != FinderControlKind::RunNativeProgram) continue;
                const auto& binding = control.programs.at(cn.binding);
                const auto key = finder_program_occurrence_key(calc, binding);
                const auto id = "c"+std::to_string(node);
                eval.graph_local_provenance.decisions.push_back({id,key,false,false});
                StrategyPolicyDecisionRequest request; request.compiled_node_id = id;
                request.selected_operator_identity = key; request.graph_local = true;
                eval.policy_decision_entries.push_back(std::move(request));
            }
            const auto checked = evaluate_strategy(*prepared.strategy, eval);
            PC_CHECK(finder_evaluation_accepted(checked));
            if (proposal == 2 || fixture == 2)
                PC_CHECK(checked.expected_consumption.contains("eldritch_exalt") && checked.expected_consumption.at("eldritch_exalt") > 0);
            PC_CHECK(checked.expected_consumption.contains("eldritch_annul") && checked.expected_consumption.at("eldritch_annul") > 0);
            if (proposal == 3) for (const auto key : {"exalt","scour","bench:mod9"})
                PC_CHECK(checked.expected_consumption.contains(key) && checked.expected_consumption.at(key) > 0);
            unsigned entries = 0;
            try {
                SelectiveProgrammeEntryValidator validator(calc, session, control, checked.policy_entries, prices, caps);
                for (unsigned i=0; i<40000 && !validator.done(); ++i) validator.advance();
                PC_CHECK(validator.done() && validator.positive_entries() > 0);
                PC_CHECK(validator.validated_entries() == checked.policy_entries.entries.size());
                entries = validator.positive_entries();
            } catch (const std::exception& error) {
                std::printf("growth fixture=%u proposal=%u entry_refusal=%s\n",fixture,proposal,error.what());
                PC_CHECK(false);
                continue;
            }
            // A paid root EV cannot authorize a different native occurrence.
            auto tampered = checked.policy_entries;
            if (!tampered.entries.empty()) tampered.entries.front().selected_operator_identity.push_back(7);
            bool refused = false;
            try {
                SelectiveProgrammeEntryValidator invalid(calc, session, control, tampered, prices, caps);
                for (unsigned i=0; i<40000 && !invalid.done(); ++i) invalid.advance();
            } catch (const StrategyEvalUnsupported&) { refused = true; }
            PC_CHECK(refused);
            std::printf("growth fixture=%u proposal=%u cost=%.12g entries=%u\n",
                fixture,proposal,checked.total_expected_cost,entries);
        }
    }
}

void run_solver_protected_fill_tests() {
    // The native weighted fixture has three independent prefix goals, two
    // suffix goals and ordinary junk. Dedicated crafts occupy the opposite
    // side; neither their weights nor their flags enter ordinary rolls.
    for (unsigned fixture = 0; fixture < 6; ++fixture) {
        auto session = make_compile_session();
        auto data = std::const_pointer_cast<DataImpl>(session->data);
        session->eldritch_eligible = false;
        if (fixture == 2) {
            for (auto& side : session->gen_type) side = 1 - side;
            std::swap(session->prefix_mask, session->suffix_mask);
        }
        data->metamod_prefixes_locked_code = 3;
        data->metamod_suffixes_locked_code = 4;
        for (auto mod : {8u, 9u}) {
            session->metamod_type[mod] = session->gen_type[mod] == PC_SIDE_SUFFIX ? 3 : 4;
            session->special_kind[mod] = -1;
            session->flags[mod] = 1 << 1;
        }
        session->bench_mod_ids = {8, 9};
        auto registry = build_action_registry(*session);
        const auto chaos = registry.index_by_id.at("chaos");
        const auto exalt = registry.index_by_id.at("exalt");
        GoalSpec goal; goal.rarity = PC_RARITY_RARE; goal.automatic_candidates = true;
        goal.automatic_candidate_kind_mask = automatic_candidate_kind_bit(AutomaticCandidateKind::ProtectedMetamod);
        for (auto mod : {0u, 3u, 4u, 5u, 6u}) {
            if ((fixture == 3 || fixture == 5) && mod == 0) continue;
            if (fixture == 4 && mod == 6) continue;
            GoalSlot slot; slot.family_id = session->family_id[mod]; slot.min_tier = 1;
            goal.slots.push_back(slot);
        }
        SolveOptions caps; apply_solve_profile_defaults(caps, SolveProfile::CalculatorProductV1);
        caps.consider_imprint_programs = false;
        caps.max_discovered_states = caps.max_expanded_states = 10000;
        caps.max_state_action_rows = 100000; caps.max_transitions = 1000000;
        caps.max_reforge_work = 1000000; caps.max_solver_owned_bytes = 256ull << 20;
        std::unordered_map<std::string, double> prices{
            {"chaos", 100}, {"exalt", 2}, {"scour", .01}, {"bench:mod8", .01}, {"bench:mod9", .01}};
        pc_item_state root; pc_item_clear(&root); root.rarity = PC_RARITY_RARE;
        if (fixture == 1) for (auto mod : {0u, 3u, 4u})
            PC_CHECK(pc_item_add_mod(&root, PC_SIDE_PREFIX, mod, session->primary_group[mod], 0, nullptr) == PC_RESULT_OK);
        CalcContext calc(session, goal, registry, {chaos, exalt}, false, false, false,
            std::nullopt, {}, false, {}, true);
        calc.set_solve_resource_caps(caps.max_discovered_states, caps.max_reforge_work,
            false, caps.max_solver_owned_bytes);
        PC_CHECK(product_original_root_continuation_scope(calc, root, caps));
        PC_CHECK(product_completion_variant(calc) == SelectiveCompletionVariant::ProtectedScourFill);
        PC_CHECK(product_completion_has_two_orientations(calc) == (fixture == 3 || fixture == 5));
        auto explicit_gate = caps;
        explicit_gate.solve_profile_override_mask |= PC_SOLVE_PROFILE_OVERRIDE_GOAL_PROGRESS_GATED_REFORGES;
        PC_CHECK(!product_original_root_continuation_scope(calc, root, explicit_gate));
        auto gap = caps; gap.max_absolute_optimality_gap = 1;
        PC_CHECK(!product_original_root_continuation_scope(calc, root, gap));
        const auto held_side = fixture == 5 ? PC_SIDE_SUFFIX : product_completion_held_side(calc, 0);
        SelectiveCompletionProducer producer(calc, root, prices, caps,
            product_completion_variant(calc, held_side), kNoId, held_side);
        for (unsigned i = 0; i < 40000 && !producer.done(); ++i) producer.advance();
        if (!producer.candidate()) std::printf("protected fill fixture=%u construction=%s\n", fixture, producer.status().c_str());
        PC_CHECK(producer.done() && producer.candidate().has_value());
        if (!producer.candidate()) continue;
        const auto& control = producer.candidate()->control;
        const auto graph = compile_finder_control_json(calc, root, control, caps);
        const auto prepared = prepare_finder_candidate(calc, session, root, graph, &control);
        PC_CHECK(prepared.ready());
        PC_CHECK(!prepare_finder_candidate(calc, session, root, graph).ready());
        auto different_root = root; different_root.quality = 1;
        PC_CHECK(!prepare_finder_candidate(calc, session, different_root, graph, &control).ready());
        if (!prepared.ready()) continue;
        auto economy = std::make_shared<EconomyImpl>(); economy->id = "protected-fill"; economy->prices = prices;
        StrategyEvalOptions eval; eval.economy = economy;
        eval.max_states = caps.max_discovered_states; eval.max_pairs = caps.max_state_action_rows;
        eval.max_transitions = caps.max_transitions; eval.max_owned_bytes = caps.max_solver_owned_bytes;
        eval.max_reforge_work = caps.max_reforge_work;
        eval.continuation_entries.push_back({calc.intern_item(root), 0, 1, root, false});
        eval.graph_local_provenance.strategy_json = graph;
        for (unsigned node = 0; node < control.nodes.size(); ++node) {
            const auto& cn = control.nodes[node];
            if (cn.kind != FinderControlKind::RunNativeProgram) continue;
            const auto& binding = control.programs.at(cn.binding);
            const auto key = planner_operator_semantic_key(calc.operators().at(binding.operator_index));
            const auto id = "c" + std::to_string(node);
            eval.graph_local_provenance.decisions.push_back({id, key, false, false});
            StrategyPolicyDecisionRequest request; request.compiled_node_id = id;
            request.selected_operator_identity = key; request.graph_local = true;
            eval.policy_decision_entries.push_back(std::move(request));
        }
        const auto checked = evaluate_strategy(*prepared.strategy, eval);
        PC_CHECK(finder_evaluation_accepted(checked));
        const auto lock_code = held_side == PC_SIDE_PREFIX
            ? data->metamod_prefixes_locked_code : data->metamod_suffixes_locked_code;
        const auto lock_mod = std::find_if(session->bench_mod_ids.begin(), session->bench_mod_ids.end(),
            [&](const auto mod) { return session->metamod_type[mod] == lock_code; });
        PC_CHECK(lock_mod != session->bench_mod_ids.end());
        if (lock_mod == session->bench_mod_ids.end()) continue;
        const auto lock_key = "bench:" + data->strings[data->mod_key_sid[*lock_mod]];
        for (const auto& key : std::vector<std::string>{"exalt", "scour", lock_key})
            PC_CHECK(checked.expected_consumption.contains(key) && checked.expected_consumption.at(key) > 0);
        if (fixture == 1) PC_CHECK(!checked.expected_consumption.contains("chaos") || checked.expected_consumption.at("chaos") == 0);
        SelectiveProgrammeEntryValidator validator(calc, session, control, checked.policy_entries, prices, caps);
        for (unsigned i = 0; i < 40000 && !validator.done(); ++i) validator.advance();
        PC_CHECK(validator.done() && validator.positive_entries() > 0);
        PC_CHECK(validator.validated_entries() == checked.policy_entries.entries.size());
        // Root cost alone cannot bless a native programme at an entry which
        // lacks the held goal. Re-admission must reject that exact carrier.
        auto corrupted = checked.policy_entries;
        PC_CHECK(!corrupted.entries.empty());
        if (!corrupted.entries.empty()) {
            PC_CHECK(pc_item_remove_at(&corrupted.entries.front().item,
                static_cast<pc_affix_side>(held_side), 0) == PC_RESULT_OK);
            bool refused = false;
            try {
                SelectiveProgrammeEntryValidator invalid(calc, session, control, corrupted, prices, caps);
                for (unsigned i = 0; i < 40000 && !invalid.done(); ++i) invalid.advance();
            } catch (const StrategyEvalUnsupported&) { refused = true; }
            PC_CHECK(refused);
        }
        CalcContext baseline_calc(session, goal, registry, {chaos});
        const auto baseline = evaluate_compiled(session,
            compile_finder_candidate_json(baseline_calc, root, {chaos}, caps), prices);
        PC_CHECK(finder_evaluation_accepted(baseline));
        PC_CHECK(checked.total_expected_cost < baseline.total_expected_cost);
        std::printf("protected fill fixture=%u cost=%.12g chaos=%.12g positive_entries=%u\n",
            fixture, checked.total_expected_cost, baseline.total_expected_cost, validator.positive_entries());
        if (fixture != 0) continue;
        // End-to-end Current and product Finder must keep native provenance
        // through the original-root check, entry check and retained graph.
        CalcContext current_calc(session, goal, registry, {chaos, exalt});
        const auto current = solve(current_calc, root, prices, caps);
        PC_CHECK(current.policy_available && current.options.product_original_root_continuations);
        PC_CHECK(current.diagnostics.selective_completion_service_checks > 0);
        PC_CHECK(current.diagnostics.selective_completion_service_status == "retained");
        PC_CHECK(current.evaluated_policy_cost < baseline.total_expected_cost);
        PC_CHECK(current.lower_bound == 0 && current.closure_unavailable_by_profile);
        CalcContext finder_calc(session, goal, registry, {chaos, exalt});
        PolicyFinderWork finder(finder_calc, session, root, prices, caps);
        for (unsigned i = 0; i < 40000 && !finder.progress().done; ++i) finder.step(128);
        PC_CHECK(finder.progress().done && finder.progress().considered <= 8);
        PC_CHECK(finder.best().has_value());
        if (finder.best()) {
            PC_CHECK(finder.best()->native_control.has_value());
            PC_CHECK(finder.best()->expected_cost < baseline.total_expected_cost);
        }
        const auto expect_refusal = [&](GoalSpec request, std::vector<std::uint32_t> actions,
                                       std::unordered_map<std::string, double> costs, const char* reason) {
            CalcContext unavailable(session, request, registry, actions, false, false, false,
                std::nullopt, {}, false, {}, true);
            unavailable.set_solve_resource_caps(caps.max_discovered_states, caps.max_reforge_work,
                false, caps.max_solver_owned_bytes);
            SelectiveCompletionProducer denied(unavailable, root, costs, caps,
                SelectiveCompletionVariant::ProtectedScourFill);
            for (unsigned i = 0; i < 40000 && !denied.done(); ++i) denied.advance();
            PC_CHECK(denied.done() && !denied.candidate());
            PC_CHECK(denied.status().starts_with(reason));
        };
        expect_refusal(goal, {chaos}, prices, "no_priced_requested_exalt_fill");
        auto unpriced = prices; unpriced.erase("exalt");
        expect_refusal(goal, {chaos, exalt}, unpriced, "no_priced_requested_exalt_fill");
        auto no_bench = goal; no_bench.disabled_action_families |= solver_action_family_bit(SolverActionFamily::Bench);
        expect_refusal(no_bench, {chaos, exalt}, prices, "no_admitted_held_side_program");
        auto subset = goal; subset.min_satisfied_slots = 4;
        expect_refusal(subset, {chaos, exalt}, prices, "protected_scour_fill_requires_all_goals_and_target_craft_space");
        auto full_target = goal; GoalSlot extra; extra.family_id = session->family_id[7]; extra.min_tier = 1;
        full_target.slots.push_back(extra);
        CalcContext full_calc(session, full_target, registry, {chaos, exalt});
        PC_CHECK(!product_original_root_continuation_scope(full_calc, root, caps));
        expect_refusal(full_target, {chaos, exalt}, prices, "protected_scour_fill_requires_all_goals_and_target_craft_space");
    }
}

void run_solver_protected_finder_tests() {
    auto session = make_compile_session();
    auto data = std::const_pointer_cast<DataImpl>(session->data);
    data->metamod_prefixes_locked_code = 3;
    session->metamod_type[9] = 3;
    session->special_kind[9] = -1;
    session->bench_mod_ids = {9};
    session->flags[9] = 1 << 1;
    session->eldritch_eligible = false;
    auto registry = build_action_registry(*session);
    GoalSpec goal; goal.rarity = PC_RARITY_RARE; goal.automatic_candidates = true;
    goal.automatic_candidate_kind_mask = automatic_candidate_kind_bit(AutomaticCandidateKind::ProtectedMetamod);
    for (auto id : {3u,4u}) {
        GoalSlot slot; slot.family_id = session->family_id[id]; slot.min_tier = 1;
        goal.slots.push_back(slot);
    }
    const auto chaos = registry.index_by_id.at("chaos");
    CalcContext calc(session,goal,registry,{chaos},false,false,true,
        std::nullopt,std::vector<CountObservation>{},false,std::vector<std::uint64_t>{},true);
    pc_item_state root; pc_item_clear(&root); root.rarity = PC_RARITY_RARE;
    std::unordered_map<std::string,double> prices{{"chaos",1},{"scour",0.01},{"bench:mod9",0.01}};
    SolveOptions limits; limits.consider_imprint_programs = false;
    limits.max_solver_owned_bytes = 256ull << 20;
    limits.max_reforge_work = 1000000;
    limits.max_discovered_states = 10000;
    limits.max_state_action_rows = 100000;
    limits.max_transitions = 1000000;
    PolicyFinderWork finder(calc,session,root,prices,limits,FinderRankingMode::Heuristic,
        FinderGrammarMode::ConditionalProtectedScour);
    for (unsigned i=0; i<20000 && !finder.progress().done; ++i) finder.step(128);
    std::ofstream("out/metamod-finder-fixture.json") << finder.telemetry_json();
    PC_CHECK(finder.progress().done);
    PC_CHECK(finder.progress().considered <= 8);
    PC_CHECK(finder.best().has_value());
    if (finder.best()) {
        const auto& best = *finder.best();
        PC_CHECK(best.native_control.has_value());
        PC_CHECK(best.strategy_json.find("bench") != std::string::npos);
        PC_CHECK(best.strategy_json.find("scour") != std::string::npos);
        PC_CHECK(best.success_probability >= 1-1e-10);
        if (best.native_control) {
            PC_CHECK(prepare_finder_candidate(calc,session,root,best.strategy_json,&*best.native_control).ready());
            // Dependency descriptors in the registry are not standalone scope.
            PC_CHECK(!prepare_finder_candidate(calc,session,root,best.strategy_json).ready());
            const auto parsed = compile_strategy_json(session, best.strategy_json.data(), best.strategy_json.size());
            std::string refusal;
            PC_CHECK(!compiled_operations_match_request(calc, *parsed, {}, &refusal));
            PC_CHECK(refusal.find("outside the requested action scope") != std::string::npos);
            auto altered = best.strategy_json;
            const auto at = altered.find("scour");
            if (at != std::string::npos) altered.replace(at,5,"annul");
            PC_CHECK(!prepare_finder_candidate(calc,session,root,altered,&*best.native_control).ready());
        }
    }
    // Check the rederived cleanup relation at every occupancy predecessor,
    // including missing-goal carriers, rather than clamping the start value.
    {
        auto clean_goal = goal;
        clean_goal.automatic_candidate_kind_mask = automatic_candidate_kind_bit(AutomaticCandidateKind::CraftedCleanup);
        CalcContext lower_calc(session,clean_goal,registry,{chaos},false,false,true);
        pc_item_state dirty = root;
        for (auto id : {3u,4u}) PC_CHECK(pc_item_add_mod(&dirty,PC_SIDE_PREFIX,id,session->primary_group[id],0,nullptr)==PC_RESULT_OK);
        PC_CHECK(pc_item_add_mod(&dirty,PC_SIDE_PREFIX,8,session->primary_group[8],PC_MOD_SLOT_CRAFTED,nullptr)==PC_RESULT_OK);
        SolveOptions options = limits; options.high_impact_executable_uppers = false;
        options.native_retention_lower = false;
        SolveWorkTestAccess::Impl proof(lower_calc,dirty,prices,options);
        proof.prepare_goal_cover_cost();
        PC_CHECK(proof.goal_cover_clean_committed);
        PC_CHECK(proof.completion_proof_lower_value(proof.result.start_state) <= prices.at("scour") + 1e-9);
        const auto index = [](unsigned rarity,unsigned mask,unsigned p,unsigned s) {
            return (((rarity*4+mask)*4+p)*4+s);
        };
        for (unsigned mask=0;mask<4;++mask) {
            const auto count = std::popcount(mask);
            for (unsigned p=count;p<=3;++p) for (unsigned suffixes=0;suffixes<=3;++suffixes) {
                const auto current=index(PC_RARITY_RARE,mask,p,suffixes);
                const auto successor=index(PC_RARITY_RARE,mask,count,0);
                PC_CHECK(proof.clean_goal_cover_cost[current] <= prices.at("scour") + proof.clean_goal_cover_cost[successor] + 1e-9);
            }
        }
    }
    const auto telemetry = finder.telemetry_json();
    const auto report = json::Parser(telemetry.data(),telemetry.size()).parse();
    bool positive = false;
    for (const auto& candidate : report.at("candidates").as_array())
        if (candidate.at("native_program").boolean && candidate.at("status").string == "accepted")
            positive |= candidate.at("positive_programme_entries").number > 0 &&
                candidate.at("validated_programme_entries").number > 0;
    PC_CHECK(positive);
}


void run_solver_finder_foulborn_product_tests();
void run_solver_mixed_side_product_tests();

void run_solver_finder_essence_tests() {
    run_solver_finder_foulborn_product_tests();
    run_solver_mixed_side_product_tests();
    // Reconstructed finite fixtures. These exercise descriptor selection and
    // exact native checking; they are not reproductions of Oliver's phone run.
    const auto make_session = [] {
        auto session = make_compile_session();
        auto data = std::const_pointer_cast<DataImpl>(session->data);
        data->essence_count = 3;
        for (const auto& key : {"finder_low", "finder_high", "finder_held"}) {
            const auto sid = static_cast<std::uint32_t>(data->strings.size());
            data->strings.push_back(key);
            data->essence_key_sids.push_back(sid);
            data->essence_by_key.emplace(key, data->essence_by_key.size());
        }
        data->essence_item_level_restrictions.assign(3, -1);
        data->essence_is_corruption_only.assign(3, 0);
        session->essence_guaranteed_mod_ids = {1, 0, 3};
        return session;
    };
    auto session = make_session();
    auto registry = build_action_registry(*session);
    const auto low = registry.index_by_id.at("essence:finder_low");
    const auto high = registry.index_by_id.at("essence:finder_high");
    const auto chaos = registry.index_by_id.at("chaos");
    const auto exalt = registry.index_by_id.at("exalt");
    const auto annul = registry.index_by_id.at("annul");
    GoalSpec goal;
    goal.rarity = PC_RARITY_RARE;
    goal.terminal.extras = ExtraExplicitPolicy::Allow;
    GoalSlot slot; slot.family_id = session->family_id[0]; slot.min_tier = 1;
    goal.slots.push_back(slot);
    pc_item_state root; pc_item_clear(&root); root.rarity = PC_RARITY_RARE;
    SolveOptions limits;
    limits.consider_imprint_programs = false;
    limits.max_solver_owned_bytes = 256ull << 20;
    limits.max_discovered_states = 10000;
    limits.max_state_action_rows = 100000;
    limits.max_transitions = 1000000;
    limits.max_reforge_work = 1000000;
    const std::unordered_map<std::string,double> prices{
        {"essence:finder_low",0.01},{"essence:finder_high",0.2},
        {"chaos",1.0},{"exalt",0.02},{"annul",0.03}};
    const auto complete = [](PolicyFinderWork& finder) {
        for (unsigned i = 0; i < 20000 && !finder.progress().done; ++i)
            finder.step(256);
        PC_CHECK(finder.progress().done);
        PC_CHECK(finder.progress().considered <= 8);
    };
    {
        CalcContext calc(session,goal,registry,{low,exalt,annul,chaos,high});
        PolicyFinderWork finder(calc,session,root,prices,limits);
        const auto queued = json::Parser(finder.telemetry_json().data(),finder.telemetry_json().size()).parse();
        PC_CHECK(queued.at("attempt_limit").number == 8);
        PC_CHECK(queued.at("candidates").as_array().size() == 4);
        PC_CHECK(queued.at("candidates").as_array().front().at("actions").as_array().front().string == "essence:finder_high");
        complete(finder);
        PC_CHECK(finder.best().has_value());
        if (finder.best()) {
            PC_CHECK(std::abs(finder.best()->expected_cost - 0.2) < 1e-9);
            PC_CHECK(prepare_finder_candidate(calc,session,root,finder.best()->strategy_json).ready());
            const auto evaluated = evaluate_compiled(session,finder.best()->strategy_json,prices);
            PC_CHECK(finder_evaluation_accepted(evaluated));
            PC_CHECK(std::abs(evaluated.expected_actions - 1.0) < 1e-9);
            PC_CHECK(std::abs(evaluated.expected_consumption.at("essence:finder_high") - 1.0) < 1e-9);
        }
    }
    {
        // Guarantee reservation remains a proposal. A cheaper checked controller
        // must beat an expensive guaranteed action on full expected cost.
        auto expensive = prices; expensive["essence:finder_high"] = 1000.0;
        CalcContext calc(session,goal,registry,{chaos,high});
        PolicyFinderWork finder(calc,session,root,expensive,limits);
        complete(finder);
        PC_CHECK(finder.best().has_value());
        if (finder.best()) PC_CHECK(finder.best()->expected_cost < 1000.0);
        const auto report = json::Parser(finder.telemetry_json().data(),finder.telemetry_json().size()).parse();
        PC_CHECK(report.at("accepted").number == 2);
    }
    {
        auto diverse = goal;
        GoalSlot other; other.family_id = session->family_id[3]; other.min_tier = 1;
        diverse.slots.push_back(other);
        const auto held = registry.index_by_id.at("essence:finder_held");
        auto portfolio_prices = prices; portfolio_prices["essence:finder_held"] = 0.3;
        CalcContext calc(session,diverse,registry,{high,held,chaos});
        PolicyFinderWork finder(calc,session,root,portfolio_prices,limits);
        complete(finder);
        PC_CHECK(finder.best().has_value());
        const auto text = finder.telemetry_json();
        const auto report = json::Parser(text.data(),text.size()).parse();
        unsigned accepted_guarantees = 0;
        for (const auto& candidate : report.at("candidates").as_array()) {
            const auto& ids = candidate.at("actions").as_array();
            if (ids.size() == 1 && candidate.at("status").string == "accepted" &&
                (ids.front().string == "essence:finder_high" || ids.front().string == "essence:finder_held"))
                ++accepted_guarantees;
        }
        PC_CHECK(accepted_guarantees == 2);
    }
    {
        pc_item_state normal = root; normal.rarity = PC_RARITY_NORMAL;
        CalcContext calc(session,goal,registry,{high});
        PolicyFinderWork finder(calc,session,normal,prices,limits);
        complete(finder);
        PC_CHECK(finder.best().has_value());
        if (finder.best()) PC_CHECK(std::abs(finder.best()->expected_cost - 0.2) < 1e-9);
    }
    {
        auto unpriced = prices; unpriced.erase("essence:finder_high");
        CalcContext calc(session,goal,registry,{high});
        PolicyFinderWork finder(calc,session,root,unpriced,limits);
        complete(finder);
        PC_CHECK(!finder.best().has_value());
        PC_CHECK(finder.progress().generated == 0);
    }
    {
        // A fractured below-tier family blocker defeats the direct guarantee.
        // Generic descriptor legality does not certify the realized native law.
        pc_item_state conflict = root;
        PC_CHECK(pc_item_add_mod(&conflict,PC_SIDE_PREFIX,1,session->primary_group[1],PC_MOD_SLOT_FRACTURED,nullptr) == PC_RESULT_OK);
        CalcContext calc(session,goal,registry,{high});
        PolicyFinderWork finder(calc,session,conflict,prices,limits);
        complete(finder);
        PC_CHECK(!finder.best().has_value());
        PC_CHECK(finder.progress().refused == 1);
    }
    {
        // Four-plus-affix Essence renewal cannot manufacture clean success.
        auto clean = goal; clean.terminal.extras = ExtraExplicitPolicy::ForbidUnmatched;
        CalcContext calc(session,clean,registry,{high});
        PolicyFinderWork finder(calc,session,root,prices,limits);
        complete(finder);
        PC_CHECK(!finder.best().has_value());
        PC_CHECK(finder.progress().refused == 1);
    }
    {
        auto tiny = limits; tiny.max_discovered_states = 1;
        CalcContext calc(session,goal,registry,{high});
        PolicyFinderWork finder(calc,session,root,prices,tiny);
        complete(finder);
        PC_CHECK(!finder.best().has_value());
        PC_CHECK(finder.progress().censored == 1);
        PC_CHECK(finder.progress().refused == 0);
    }
    {
        // The ordinary two-stage vocabulary can retry a Normal-root Alchemy
        // by paying Scour on every miss. No Alchemy is executed on a Rare miss.
        const auto alchemy = registry.index_by_id.at("alchemy");
        const auto scour = registry.index_by_id.at("scour");
        pc_item_state normal = root; normal.rarity = PC_RARITY_NORMAL;
        const std::unordered_map<std::string,double> reset_prices{{"alchemy",0.05},{"scour",0.3741}};
        CalcContext calc(session,goal,registry,{alchemy,scour});
        const auto start = calc.intern_item(normal);
        const auto outcomes = calc.outcomes(start,alchemy);
        PC_CHECK(outcomes.supported && outcomes.applicable);
        double success = 0;
        for (const auto& exit : outcomes.entries) {
            if (!(exit.probability > 0)) continue;
            if (calc.is_goal_state(calc.state(exit.state))) success += exit.probability;
            else {
                const auto& reset = calc.outcomes(exit.state,scour);
                PC_CHECK(reset.supported && reset.applicable && reset.entries.size() == 1);
                if (reset.entries.size() == 1) {
                    pc_item_state reset_item;
                    PC_CHECK(calc.materialize(reset.entries[0].state,reset_item));
                    PC_CHECK(exact_item_state_key(reset_item) == exact_item_state_key(normal));
                    PC_CHECK(reset.entries[0].probability == 1.0);
                }
            }
        }
        PC_CHECK(success > 0 && success < 1);
        PolicyFinderWork finder(calc,session,normal,reset_prices,limits);
        complete(finder);
        PC_CHECK(finder.best().has_value());
        if (finder.best()) {
            const auto& best = *finder.best();
            PC_CHECK(best.strategy_json.find("scour") != std::string::npos);
            PC_CHECK(best.strategy_json.find("alchemy") != std::string::npos);
            PC_CHECK(prepare_finder_candidate(calc,session,normal,best.strategy_json).ready());
            const auto evaluated = evaluate_compiled(session,best.strategy_json,reset_prices);
            PC_CHECK(finder_evaluation_accepted(evaluated));
            const double expected = (0.05 + (1-success)*0.3741)/success;
            PC_CHECK(std::abs(best.expected_cost-expected) < 1e-9);
            PC_CHECK(std::abs(evaluated.expected_consumption.at("alchemy")-1/success) < 1e-9);
            PC_CHECK(std::abs(evaluated.expected_consumption.at("scour")-(1-success)/success) < 1e-9);
            std::printf("reconstructed Alchemy/Scour success=%.12g checked_cost=%.12g\n",success,best.expected_cost);
        }
        CalcContext excluded(session,goal,registry,{alchemy});
        PolicyFinderWork no_reset(excluded,session,normal,reset_prices,limits);
        complete(no_reset);
        PC_CHECK(!no_reset.best().has_value());
    }
    {
        CalcContext calc(session,goal,registry,{high,chaos});
        PolicyFinderWork finder(calc,session,root,prices,limits);
        finder.request_bounded_finish(); finder.step(1);
        const auto report = json::Parser(finder.telemetry_json().data(),finder.telemetry_json().size()).parse();
        PC_CHECK(finder.progress().done && finder.progress().considered == 0);
        for (const auto& candidate : report.at("candidates").as_array())
            PC_CHECK(candidate.at("status").string == "unserved_finish");
    }
    {
        // Native-only capture preserves every generated complete graph without
        // serving it. Finish and state censorship remain distinct outcomes.
        struct CapturedGraph { std::uint32_t ordinal; std::string hash, graph; bool served=false; };
        std::vector<CapturedGraph> captured;
        FinderCandidateGraphCapture capture;
        capture.context = &captured;
        capture.retained_owned_bytes = 1ull << 20;
        capture.max_write_scratch_bytes = 1ull << 20;
        capture.write = [](void* context, std::uint32_t ordinal,
                const std::string& hash, const std::string& graph) {
            static_cast<std::vector<CapturedGraph>*>(context)->push_back(
                {ordinal,hash,graph});
        };
        capture.verify = [](void* context,std::uint32_t ordinal,
                const std::string& hash,const std::string& graph) {
            auto& captured = static_cast<std::vector<CapturedGraph>*>(context)->at(ordinal-1);
            PC_CHECK(captured.hash == hash && captured.graph == graph);
            captured.served=true;
        };
        CalcContext calc(session,goal,registry,{high,chaos});
        const auto verify_capture = [&](const PolicyFinderWork& finder) {
            const std::string telemetry = finder.telemetry_json();
            const auto report = json::Parser(telemetry.data(),telemetry.size()).parse();
            PC_CHECK(report.at("diagnostic_candidate_graph_capture").at("graphs").number == captured.size());
            PC_CHECK(report.at("candidates").as_array().size() == captured.size());
            std::uint64_t bytes = 0;
            for (std::size_t i=0; i<captured.size(); ++i) {
                const auto& graph = captured[i];
                const auto& record = report.at("candidates").as_array()[i];
                PC_CHECK(graph.ordinal == i+1);
                PC_CHECK(record.at("diagnostic_graph_ordinal").number == graph.ordinal);
                PC_CHECK(record.at("graph_hash").string == graph.hash);
                PC_CHECK(graph.served == (record.at("started_ns").type != json::Type::Null));
                std::uint64_t hash = 14695981039346656037ull;
                for (const unsigned char c : graph.graph) { hash ^= c; hash *= 1099511628211ull; }
                PC_CHECK(std::to_string(hash) == graph.hash);
                PC_CHECK(prepare_finder_candidate(calc,session,root,graph.graph).ready());
                bytes += graph.graph.size();
            }
            PC_CHECK(report.at("diagnostic_candidate_graph_capture").at("serialized_bytes").number == bytes);
        };
        PolicyFinderWork unserved(calc,session,root,prices,limits,
            FinderRankingMode::Heuristic,FinderGrammarMode::Conditional,8,capture);
        unserved.request_bounded_finish(); unserved.step(1);
        const auto report = json::Parser(unserved.telemetry_json().data(),unserved.telemetry_json().size()).parse();
        PC_CHECK(unserved.progress().considered == 0);
        for (const auto& record : report.at("candidates").as_array())
            PC_CHECK(record.at("status").string == "unserved_finish");
        verify_capture(unserved);
        captured.clear();
        auto tiny = limits; tiny.max_discovered_states = 1;
        PolicyFinderWork censored(calc,session,root,prices,tiny,
            FinderRankingMode::Heuristic,FinderGrammarMode::Conditional,8,capture);
        complete(censored);
        PC_CHECK(censored.progress().censored > 0);
        verify_capture(censored);
        captured.clear();
        PolicyFinderWork checked(calc,session,root,prices,limits,
            FinderRankingMode::Heuristic,FinderGrammarMode::Conditional,8,capture);
        complete(checked); verify_capture(checked);
        PC_CHECK(checked.best().has_value());
        if (checked.best()) PC_CHECK(std::abs(checked.best()->expected_cost-0.2) < 1e-9);
        PolicyFinderWork ordinary(calc,session,root,prices,limits);
        PC_CHECK(ordinary.telemetry_json().find("diagnostic_candidate_graph_capture") == std::string::npos);
        auto oversized = capture; oversized.max_write_scratch_bytes = limits.max_solver_owned_bytes;
        bool bounded = false;
        try { PolicyFinderWork too_large(calc,session,root,prices,limits,
            FinderRankingMode::Heuristic,FinderGrammarMode::Conditional,8,oversized); }
        catch (const std::length_error&) { bounded = true; }
        PC_CHECK(bounded);
    }
    {
        // Any-two of three slots: successful selected affixes stop before the
        // count guard. Uniform Annul can lose the held goal; that exit redraws.
        GoalSpec any_two; any_two.rarity = PC_RARITY_RARE;
        any_two.min_satisfied_slots = 2;
        for (const auto mod : {3u,4u,0u}) {
            GoalSlot wanted; wanted.family_id = session->family_id[mod]; wanted.min_tier = 1;
            any_two.slots.push_back(wanted);
        }
        const auto held = registry.index_by_id.at("essence:finder_held");
        CalcContext calc(session,any_two,registry,{held,annul});
        PC_CHECK(any_two.required_satisfied_slots() == 2 && any_two.slots.size() == 3);
        FinderControlGraph control{{
            {FinderControlKind::TestGoal,kNoId,5,1},
            {FinderControlKind::TestSlot,0,2,4},
            {FinderControlKind::TestAffixCountAtLeast4,3,3,4},
            {FinderControlKind::RunPrimitive,annul,kNoId,kNoId,0},
            {FinderControlKind::RunPrimitive,held,kNoId,kNoId,0},
            {FinderControlKind::GoalTerminal}},0};
        const auto graph = compile_finder_control_json(calc,root,control,limits);
        const auto compiled = compile_strategy_json(session,graph.data(),graph.size());
        const auto make_item = [&](std::initializer_list<unsigned> mods) {
            auto item = root;
            for (const auto mod : mods)
                PC_CHECK(pc_item_add_mod(&item,static_cast<pc_affix_side>(session->gen_type[mod]),
                    mod,session->primary_group[mod],0,nullptr) == PC_RESULT_OK);
            return item;
        };
        const auto route = [&](const pc_item_state& item) {
            auto node = compiled->start_node;
            for (unsigned step=0; step<8; ++step) {
                const auto& current = compiled->nodes.at(node);
                if (current.kind == StrategyNodeKind::Operation ||
                    current.kind == StrategyNodeKind::Terminal) return current.id;
                bool matched = false;
                for (const auto& edge : current.edges) {
                    if (edge.is_default || evaluate_compiled_condition(edge.condition,*session,item)) {
                        node=edge.target; matched=true; break;
                    }
                }
                PC_CHECK(matched);
                if (!matched) return std::string("unmatched");
            }
            return std::string("open_route");
        };
        PC_CHECK(route(make_item({3,4})) == "c5");
        PC_CHECK(route(make_item({3,6})) == "c4");
        PC_CHECK(route(make_item({3,4,0})) == "c5");
        PC_CHECK(route(make_item({3,4,6})) == "c3");
        PC_CHECK(route(make_item({3,1,6})) == "c3");
        PC_CHECK(route(make_item({6,7})) == "c4");
        const auto three = make_item({3,4,6});
        const auto& exits = calc.outcomes(calc.intern_item(three),annul);
        PC_CHECK(exits.supported && exits.applicable);
        double stopped=0, held_lost=0, redraw=0;
        const auto& held_test = compiled->nodes.at(compiled->node_by_id.at("c1")).edges.front().condition;
        for (const auto& exit : exits.entries) {
            pc_item_state item; PC_CHECK(calc.materialize(exit.state,item));
            const auto next = route(item);
            if (next == "c5") stopped += exit.probability;
            if (next == "c4") redraw += exit.probability;
            if (!evaluate_compiled_condition(held_test,*session,item)) {
                held_lost += exit.probability; PC_CHECK(next == "c4");
            }
        }
        PC_CHECK(std::abs(stopped-1.0/3) < 1e-12);
        PC_CHECK(std::abs(held_lost-1.0/3) < 1e-12);
        PC_CHECK(std::abs(redraw-2.0/3) < 1e-12);
        bool emitted = false;
        FinderCandidateGraphCapture capture; capture.context = &emitted;
        capture.retained_owned_bytes = sizeof(emitted);
        capture.max_write_scratch_bytes = 65536;
        struct ExpectedGraph { const std::string* graph; bool* emitted; } expected{&graph,&emitted};
        capture.context = &expected; capture.retained_owned_bytes = sizeof(expected);
        capture.write = [](void* context,std::uint32_t,const std::string&,const std::string& bytes) {
            auto& expected = *static_cast<ExpectedGraph*>(context);
            if (bytes == *expected.graph) *expected.emitted=true;
        };
        PolicyFinderWork finder(calc,session,root,
            {{"essence:finder_held",0.2},{"annul",0.03}},limits,
            FinderRankingMode::Heuristic,FinderGrammarMode::Conditional,8,capture);
        complete(finder); PC_CHECK(emitted);
        std::printf("reconstructed any-two clean router: N2 goal=stop, miss=redraw; N3 goal=stop, miss=Annul; held loss=1/3 then redraw\n");
    }
    {
        // A full ordinary pool cannot reach clean two goals when Annul stops
        // at three affixes. The corrected controller retains the Essence's
        // semantic goal role and continues cleanup at three in either order.
        const auto held = registry.index_by_id.at("essence:finder_held");
        const std::unordered_map<std::string,double> clean_prices{
            {"essence:finder_held",0.2},{"chaos",1.0},{"annul",0.03}};
        double cost = -1;
        for (const bool reverse : {false,true}) {
            GoalSpec clean; clean.rarity = PC_RARITY_RARE;
            for (const auto mod : (reverse ? std::vector<unsigned>{4,3} : std::vector<unsigned>{3,4})) {
                GoalSlot wanted; wanted.family_id = session->family_id[mod]; wanted.min_tier = 1;
                clean.slots.push_back(wanted);
            }
            CalcContext calc(session,clean,registry,{held,chaos,annul});
            FinderControlGraph legacy{{
                {FinderControlKind::TestGoal,kNoId,5,1},
                {FinderControlKind::TestSlot,reverse ? 1u : 0u,2,4},
                {FinderControlKind::TestAffixCountAtLeast4,kNoId,3,4},
                {FinderControlKind::RunPrimitive,annul,kNoId,kNoId,0},
                {FinderControlKind::RunPrimitive,held,kNoId,kNoId,0},
                {FinderControlKind::GoalTerminal}},0};
            const auto old_graph = compile_finder_control_json(calc,root,legacy,limits);
            const auto old_result = evaluate_compiled(session,old_graph,clean_prices);
            PC_CHECK(!finder_evaluation_accepted(old_result));
            PC_CHECK(old_result.success_probability < 1e-12);
            auto invalid = legacy; invalid.nodes[2].binding = 0;
            bool refused = false;
            try { (void)compile_finder_control_json(calc,root,invalid,limits); }
            catch (const std::invalid_argument&) { refused = true; }
            PC_CHECK(refused);
            PolicyFinderWork finder(calc,session,root,clean_prices,limits);
            complete(finder);
            PC_CHECK(finder.best().has_value());
            if (finder.best()) {
                const auto& best = *finder.best();
                PC_CHECK(!best.native_control.has_value());
                const auto evaluated = evaluate_compiled(session,best.strategy_json,clean_prices);
                PC_CHECK(finder_evaluation_accepted(evaluated));
                PC_CHECK(evaluated.expected_consumption.at("annul") > 1);
                PC_CHECK(evaluated.expected_consumption.at("essence:finder_held") > 1);
                if (cost < 0) cost = best.expected_cost;
                else PC_CHECK(std::abs(best.expected_cost-cost) < 1e-9);
                std::printf("reconstructed ordinary clean Essence/Annul order=%d cost=%.12g essence=%.12g annul=%.12g\n",
                    reverse,best.expected_cost,evaluated.expected_consumption.at("essence:finder_held"),
                    evaluated.expected_consumption.at("annul"));
            }
        }
    }
    {
        // Same two-goal clean problem in both goal orders; complete paid
        // acquisition + lock + Scour, including all native retry outcomes.
        auto held_session = make_session();
        auto data = std::const_pointer_cast<DataImpl>(held_session->data);
        data->metamod_prefixes_locked_code = 3;
        held_session->metamod_type[9] = 3;
        held_session->bench_mod_ids = {9};
        held_session->flags[9] = 1 << 1;
        held_session->eldritch_eligible = false;
        auto held_registry = build_action_registry(*held_session);
        const auto held = held_registry.index_by_id.at("essence:finder_held");
        const auto held_chaos = held_registry.index_by_id.at("chaos");
        const std::unordered_map<std::string,double> held_prices{
            {"chaos",1.0},{"essence:finder_held",0.2},{"scour",0.01},{"bench:mod9",0.01}};
        double checked_cost = -1;
        for (const bool reverse : {false,true}) {
            GoalSpec clean; clean.rarity = PC_RARITY_RARE; clean.automatic_candidates = true;
            clean.automatic_candidate_kind_mask = automatic_candidate_kind_bit(AutomaticCandidateKind::ProtectedMetamod);
            for (const auto mod : (reverse ? std::vector<unsigned>{4,3} : std::vector<unsigned>{3,4})) {
                GoalSlot wanted; wanted.family_id = held_session->family_id[mod]; wanted.min_tier = 1;
                clean.slots.push_back(wanted);
            }
            CalcContext calc(held_session,clean,held_registry,{held_chaos,held},false,false,true,
                std::nullopt,std::vector<CountObservation>{},false,std::vector<std::uint64_t>{},true);
            SelectiveCompletionProducer current_default(calc,root,held_prices,limits,SelectiveCompletionVariant::ProtectedScour);
            for (unsigned i=0; i<20000 && !current_default.done(); ++i) current_default.advance();
            PC_CHECK(current_default.candidate().has_value());
            if (current_default.candidate()) PC_CHECK(current_default.candidate()->acquisition_action == held_chaos);
            auto product_limits = limits;
            apply_solve_profile_defaults(product_limits, SolveProfile::CalculatorProductV1);
            CalcContext current_calc(held_session,clean,held_registry,{held_chaos,held});
            const auto current=solve(current_calc,root,held_prices,product_limits);
            PC_CHECK(current.policy_available && current.options.product_original_root_continuations);
            PC_CHECK(current.lower_bound==0 && current.closure_unavailable_by_profile);
            if (current.policy_available) {
                const auto graph=!current.refined_policy_artifact.strategy_json.empty()
                    ? current.refined_policy_artifact.strategy_json
                    : compile_policy_strategy_json(current_calc,current,"product guaranteed protected Scour");
                const auto checked=evaluate_compiled(held_session,graph,held_prices);
                PC_CHECK(finder_evaluation_accepted(checked));
                PC_CHECK(checked.expected_consumption.contains("essence:finder_held") &&
                    checked.expected_consumption.at("essence:finder_held")>0);
                PC_CHECK(checked.expected_consumption.contains("scour") && checked.expected_consumption.at("scour")>0);
                PC_CHECK(checked.expected_consumption.contains("bench:mod9") && checked.expected_consumption.at("bench:mod9")>0);
                PC_CHECK(std::abs(current.evaluated_policy_cost-checked.total_expected_cost)<1e-8);
                std::printf("reconstructed product Current guaranteed Scour order=%d cost=%.12g status=%s\n",
                    reverse,current.evaluated_policy_cost,current.diagnostics.selective_completion_service_status.c_str());
            }
            PolicyFinderWork finder(calc,held_session,root,held_prices,product_limits);
            complete(finder);
            PC_CHECK(finder.best().has_value());
            if (finder.best()) {
                const auto& best = *finder.best();
                PC_CHECK(best.native_control.has_value());
                PC_CHECK(best.strategy_json.find("finder_held") != std::string::npos);
                PC_CHECK(best.strategy_json.find("scour") != std::string::npos);
                PC_CHECK(best.strategy_json.find("bench") != std::string::npos);
                const auto evaluated = evaluate_compiled(held_session,best.strategy_json,held_prices);
                PC_CHECK(finder_evaluation_accepted(evaluated));
                PC_CHECK(evaluated.expected_consumption.at("essence:finder_held") > 1);
                PC_CHECK(evaluated.expected_consumption.at("scour") > 0);
                PC_CHECK(evaluated.expected_consumption.at("bench:mod9") > 0);
                PC_CHECK(std::abs(evaluated.total_expected_cost-best.expected_cost) < 1e-8);
                if (checked_cost < 0) checked_cost = best.expected_cost;
                else PC_CHECK(std::abs(best.expected_cost-checked_cost) < 1e-8);
                const auto report = json::Parser(finder.telemetry_json().data(),finder.telemetry_json().size()).parse();
                bool checked_entries = false;
                for (const auto& c : report.at("candidates").as_array())
                    if (c.at("native_program").boolean && c.at("status").string == "accepted")
                        checked_entries |= c.at("positive_programme_entries").number > 0 &&
                            c.at("positive_programme_entries").number == c.at("validated_programme_entries").number;
                PC_CHECK(checked_entries);
                std::printf("reconstructed clean Essence order=%d cost=%.12g essence=%.12g lock=%.12g scour=%.12g\n",
                    reverse,best.expected_cost,evaluated.expected_consumption.at("essence:finder_held"),
                    evaluated.expected_consumption.at("bench:mod9"),evaluated.expected_consumption.at("scour"));
            }
        }
    }
}

void run_solver_finder_foulborn_product_tests() {
    auto session = make_compile_session();
    auto data = std::const_pointer_cast<DataImpl>(session->data);
    data->mod_type_key_sid = data->mod_key_sid;
    const auto registry = build_action_registry(*session);
    SolveOptions limits;
    apply_solve_profile_defaults(limits, SolveProfile::CalculatorProductV1);
    limits.max_solver_owned_bytes = 256ull << 20;
    limits.max_discovered_states = 10000;
    limits.max_state_action_rows = 100000;
    limits.max_transitions = 1000000;
    limits.max_reforge_work = 1000000;
    for (const auto type : {ActionType::FoulbornAugment, ActionType::FoulbornRegal,
                           ActionType::FoulbornExalt}) {
        const std::string add_id = type == ActionType::FoulbornAugment
            ? "foulborn_augment" : type == ActionType::FoulbornRegal
            ? "foulborn_regal" : "foulborn_exalt";
        const auto add = registry.index_by_id.at(add_id);
        const auto roll = registry.index_by_id.at(type == ActionType::FoulbornExalt ? "alchemy" : "transmute");
        const auto reset = registry.index_by_id.at("scour");
        GoalSpec goal;
        goal.rarity = type == ActionType::FoulbornAugment ? PC_RARITY_MAGIC : PC_RARITY_RARE;
        goal.terminal.extras = ExtraExplicitPolicy::Allow;
        GoalSlot slot; slot.family_id = session->family_id[0]; slot.min_tier = 1;
        goal.slots.push_back(slot);
        pc_item_state root; pc_item_clear(&root);
        const std::unordered_map<std::string,double> prices{
            {registry.actions[roll].id,2.0},{"scour",0.1},{add_id,0.01}};
        const auto run_product_api = [&](const pc_item_state& api_root,
                const std::vector<std::uint32_t>& scope,
                const std::unordered_map<std::string,double>& api_prices) {
            pc_session public_session; public_session.impl = session;
            std::string goal_json = "{\"version\":\"v1\",\"rarity\":\"" +
                std::string(goal.rarity == PC_RARITY_MAGIC ? "magic" : "rare") +
                "\",\"allow_extra_modifiers\":true,\"slots\":[{\"family_mod_key\":\"mod0\",\"min_tier\":1}],\"actions\":[";
            for (const auto index : scope) {
                if (goal_json.back() != '[') goal_json += ',';
                goal_json += '"' + registry.actions[index].id + '"';
            }
            goal_json += "]}";
            std::string economy_json = "{\"version\":\"v1\",\"prices\":{";
            for (const auto& [key,price] : api_prices) {
                if (economy_json.back() != '{') economy_json += ',';
                economy_json += '"'+key+"\":"+std::to_string(price);
            }
            economy_json += "}}";
            pc_error_info error{};
            pc_economy_handle economy = nullptr;
            PC_CHECK(pc_economy_load_json(economy_json.data(),economy_json.size(),&economy,&error)==PC_RESULT_OK);
            for (const auto mode : {PC_SOLVER_MODE_CURRENT,PC_SOLVER_MODE_STRATEGY_FINDER}) {
                pc_solver_handle solver = nullptr;
                PC_CHECK(pc_solver_create(&public_session,goal_json.data(),goal_json.size(),&solver,&error)==PC_RESULT_OK);
                if (!solver) continue;
                pc_solve_options options{};
                options.struct_size=sizeof(options); options.abi_version=PC_ABI_VERSION;
                options.solver_mode=mode; options.solve_profile=PC_SOLVE_PROFILE_CALCULATOR_PRODUCT_V1;
                options.max_states=10000; options.max_discovered_states=10000; options.max_expanded_states=10000;
                options.max_state_action_rows=100000; options.max_transitions=1000000;
                options.max_reforge_work=1000000; options.max_solver_owned_bytes=256ull<<20;
                pc_solve_summary summary{};
                const auto code = pc_solver_solve(solver,&api_root,economy,&options,&summary,&error);
                if (code!=PC_RESULT_OK) std::printf("product API %s mode=%u error=%s\n",add_id.c_str(),mode,error.message);
                PC_CHECK(code==PC_RESULT_OK && summary.policy_available);
                if (code==PC_RESULT_OK && summary.policy_available) {
                    size_t length=0;
                    PC_CHECK(pc_solver_compile_strategy(solver,nullptr,0,&length,&error)==PC_RESULT_OK);
                    std::string graph(length+1,'\0');
                    PC_CHECK(pc_solver_compile_strategy(solver,graph.data(),graph.size(),&length,&error)==PC_RESULT_OK);
                    graph.resize(length);
                    PC_CHECK(graph.find(add_id)!=std::string::npos);
                    const auto checked = evaluate_compiled(session,graph,api_prices);
                    PC_CHECK(finder_evaluation_accepted(checked));
                    PC_CHECK(checked.expected_consumption.contains(add_id) && checked.expected_consumption.at(add_id)>0);
                    PC_CHECK(std::abs(summary.evaluated_policy_cost-checked.total_expected_cost)<1e-8);
                    std::printf("product API %s mode=%u checked=%.12g\n",add_id.c_str(),mode,checked.total_expected_cost);
                }
                pc_solver_destroy(solver);
            }
            pc_economy_destroy(economy);
        };
        {
            auto incoming = root;
            incoming.rarity = type == ActionType::FoulbornExalt ? PC_RARITY_RARE : PC_RARITY_MAGIC;
            const auto held = type == ActionType::FoulbornRegal ? 0u : 7u;
            PC_CHECK(pc_item_add_mod(&incoming,static_cast<pc_affix_side>(session->gen_type[held]),
                held,session->primary_group[held],0,nullptr)==PC_RESULT_OK);
            const auto renewal = registry.index_by_id.at(type == ActionType::FoulbornExalt ? "chaos" : "alteration");
            const auto transmute = registry.index_by_id.at("transmute");
            std::vector<std::uint32_t> scope{add};
            std::unordered_map<std::string,double> incoming_prices{{add_id,0.01}};
            if (type != ActionType::FoulbornRegal) {
                scope.push_back(renewal); incoming_prices[registry.actions[renewal].id]=2.0;
            } else {
                scope.push_back(transmute); scope.push_back(reset);
                incoming_prices["transmute"]=2.0; incoming_prices["scour"]=0.1;
            }
            // Exercise the ordinary public request path, including product
            // options resolution and returned graph transport, in both lanes.
            run_product_api(incoming,scope,incoming_prices);
            CalcContext current_calc(session,goal,registry,scope);
            const auto current = solve(current_calc,incoming,incoming_prices,limits);
            PC_CHECK(current.policy_available);
            if (current.policy_available) {
                const auto graph = !current.refined_policy_artifact.strategy_json.empty()
                    ? current.refined_policy_artifact.strategy_json
                    : compile_policy_strategy_json(current_calc,current,"incoming product Foulborn fixture");
                const auto checked = evaluate_compiled(session,graph,incoming_prices);
                PC_CHECK(finder_evaluation_accepted(checked));
                PC_CHECK(graph.find(add_id)!=std::string::npos);
                PC_CHECK(checked.expected_consumption.contains(add_id) && checked.expected_consumption.at(add_id)>0);
                PC_CHECK(current.lower_bound == 0 && current.closure_unavailable_by_profile);
                PC_CHECK(std::abs(current.evaluated_policy_cost-checked.total_expected_cost)<1e-8);
                std::printf("incoming product Current %s cost=%.12g\n",add_id.c_str(),current.evaluated_policy_cost);
            }
            CalcContext finder_calc(session,goal,registry,scope);
            PolicyFinderWork finder(finder_calc,session,incoming,incoming_prices,limits);
            for (unsigned i=0;i<20000 && !finder.progress().done;++i) finder.step(256);
            PC_CHECK(finder.progress().done && finder.progress().considered<=8);
            PC_CHECK(finder.best().has_value());
            if (finder.best()) {
                const auto checked = evaluate_compiled(session,finder.best()->strategy_json,incoming_prices);
                PC_CHECK(finder_evaluation_accepted(checked));
                PC_CHECK(finder.best()->strategy_json.find(add_id)!=std::string::npos);
                PC_CHECK(checked.expected_consumption.contains(add_id) && checked.expected_consumption.at(add_id)>0);
                PC_CHECK(prepare_finder_candidate(finder_calc,session,incoming,finder.best()->strategy_json).ready());
                std::printf("incoming product Finder %s cost=%.12g\n",add_id.c_str(),finder.best()->expected_cost);
            }
        }
        for (unsigned control = 0; control < 5; ++control) {
            auto request = goal;
            auto actions = std::vector<std::uint32_t>{roll,reset};
            if (control != 1) actions.push_back(add);
            if (control == 2)
                request.disabled_action_families = solver_action_family_bit(solver_action_family_for_action(registry.actions[add]));
            auto cost = prices;
            if (control == 3) cost.erase(add_id);
            auto caps = limits;
            if (control == 4)
                caps.solve_profile_override_mask |= PC_SOLVE_PROFILE_OVERRIDE_GOAL_PROGRESS_GATED_REFORGES;
            CalcContext calc(session,request,registry,actions);
            PolicyFinderWork finder(calc,session,root,cost,caps);
            for (unsigned i=0;i<20000 && !finder.progress().done;++i) finder.step(256);
            PC_CHECK(finder.progress().done);
            PC_CHECK(finder.progress().considered <= 8);
            bool served = false;
            const auto telemetry = finder.telemetry_json();
            const auto report = json::Parser(telemetry.data(),telemetry.size()).parse();
            for (const auto& candidate : report.at("candidates").as_array()) {
                const auto& a = candidate.at("actions").as_array();
                const bool uses_add = std::any_of(a.begin(),a.end(),[&](const auto& id){return id.string==add_id;});
                if (uses_add && candidate.at("conditional").boolean)
                    served |= candidate.at("status").string == "accepted";
            }
            PC_CHECK(served == (control == 0));
            if (control == 0) {
                // Empty-Normal API replay catches Current capability leaking
                // into ordinary Finder primitive compilation.
                run_product_api(root,actions,cost);
                CalcContext current_calc(session,request,registry,actions);
                const auto current = solve(current_calc,root,cost,caps);
                std::printf("product Current %s available=%d cost=%.12g lower=%.12g scope=%s\n",add_id.c_str(),
                    current.policy_available,current.evaluated_policy_cost,current.lower_bound,current.diagnostics.solution_scope.c_str());
                PC_CHECK(current.policy_available);
                if (current.policy_available) {
                    const auto graph = !current.refined_policy_artifact.strategy_json.empty()
                        ? current.refined_policy_artifact.strategy_json
                        : compile_policy_strategy_json(current_calc,current,"product Foulborn fixture");
                    const auto checked = evaluate_compiled(session,graph,cost);
                    PC_CHECK(finder_evaluation_accepted(checked));
                    PC_CHECK(current.lower_bound == 0 && current.closure_unavailable_by_profile);
                    PC_CHECK(std::abs(current.evaluated_policy_cost-checked.total_expected_cost)<1e-8);
                    PC_CHECK(graph.find(add_id)!=std::string::npos);
                    PC_CHECK(checked.expected_consumption.contains(add_id) &&
                        checked.expected_consumption.at(add_id)>0);
                    PC_CHECK(checked.expected_consumption.contains("scour") &&
                        checked.expected_consumption.at("scour")>0);
                    std::printf("product Current %s graph_add=%d\n",add_id.c_str(),graph.find(add_id)!=std::string::npos);
                }
                PC_CHECK(finder.best().has_value());
                if (finder.best()) {
                    const auto& best = *finder.best();
                    PC_CHECK(best.strategy_json.find(add_id) != std::string::npos);
                    PC_CHECK(prepare_finder_candidate(calc,session,root,best.strategy_json).ready());
                    const auto evaluated = evaluate_compiled(session,best.strategy_json,cost);
                    PC_CHECK(finder_evaluation_accepted(evaluated));
                    PC_CHECK(evaluated.expected_consumption.at(add_id)>0);
                    PC_CHECK(evaluated.expected_consumption.at("scour")>0);
                    PC_CHECK(std::abs(best.expected_cost-evaluated.total_expected_cost)<1e-9);
                    std::printf("product Finder %s cost=%.12g add=%.12g reset=%.12g\n",add_id.c_str(),best.expected_cost,
                        evaluated.expected_consumption.at(add_id),evaluated.expected_consumption.at("scour"));
                }
            }
        }
    }
    {
        // Transmute plus one Regal can never supply three distinct prefixes.
        // The p0=0 extension must refuse q=0, including a free add.
        GoalSpec unreachable; unreachable.rarity = PC_RARITY_RARE;
        unreachable.terminal.extras = ExtraExplicitPolicy::Allow;
        for (const auto mod : {0u,3u,4u}) {
            GoalSlot wanted; wanted.family_id = session->family_id[mod]; wanted.min_tier = 1;
            unreachable.slots.push_back(wanted);
        }
        const auto roll = registry.index_by_id.at("transmute");
        const auto add = registry.index_by_id.at("foulborn_regal");
        const auto reset = registry.index_by_id.at("scour");
        CalcContext calc(session,unreachable,registry,{roll,add,reset});
        pc_item_state root; pc_item_clear(&root);
        const auto no_cycle = solve(calc,root,{{"transmute",2},{"foulborn_regal",0},{"scour",0.1}},limits);
        PC_CHECK(!no_cycle.policy_available);
        PC_CHECK(no_cycle.lower_bound == 0 && no_cycle.closure_unavailable_by_profile);
        auto legacy = limits; legacy.paid_root_foulborn_salvage = true;
        legacy.paid_root_foulborn_grammar_version = 1;
        bool refused = false;
        try { SolveWork work(calc,root,{},legacy); }
        catch (const std::invalid_argument&) { refused = true; }
        PC_CHECK(refused);
    }

}

void run_solver_mixed_side_product_tests() {
    auto session = make_compile_session();
    auto data=std::const_pointer_cast<DataImpl>(session->data);
    data->essence_count=2;
    for (const auto& key:{"mixed_prefix","mixed_suffix"}) {
        const auto sid=static_cast<std::uint32_t>(data->strings.size());
        data->strings.push_back(key);data->essence_key_sids.push_back(sid);
        data->essence_by_key.emplace(key,data->essence_by_key.size());
    }
    data->essence_item_level_restrictions.assign(2,-1);
    data->essence_is_corruption_only.assign(2,0);
    session->essence_guaranteed_mod_ids={3,5};
    auto registry = build_action_registry(*session);
    const auto chaos = registry.index_by_id.at("chaos");
    const std::array<std::uint32_t,2> essences{
        registry.index_by_id.at("essence:mixed_prefix"),registry.index_by_id.at("essence:mixed_suffix")};
    const std::vector<std::uint32_t> actions{chaos,essences[0],essences[1]};
    GoalSpec goal; goal.rarity = PC_RARITY_RARE; goal.automatic_candidates = true;
    goal.automatic_candidate_kind_mask = automatic_candidate_kind_bit(AutomaticCandidateKind::EldritchSide);
    for (auto mod : {3u,4u,5u,6u}) {
        GoalSlot wanted; wanted.family_id = session->family_id[mod]; wanted.min_tier = 1;
        goal.slots.push_back(wanted);
    }
    SolveOptions caps; apply_solve_profile_defaults(caps,SolveProfile::CalculatorProductV1);
    caps.max_discovered_states = 10000; caps.max_expanded_states = 10000;
    caps.max_state_action_rows = 100000; caps.max_transitions = 1000000;
    caps.max_reforge_work = 1000000; caps.max_solver_owned_bytes = 256ull << 20;
    std::unordered_map<std::string,double> prices{{"chaos",100},{"essence:mixed_prefix",2},{"essence:mixed_suffix",3},{"eldritch_chaos",0.01},{"eldritch_annul",0.01}};
    for (unsigned tier=1;tier<=4;++tier) {
        prices["eldritch_ember:"+std::to_string(tier)]=0.01;
        prices["eldritch_ichor:"+std::to_string(tier)]=0.01;
    }
    for (unsigned dirty=0;dirty<5;++dirty) {
        pc_item_state root; pc_item_clear(&root); root.rarity=PC_RARITY_RARE;
        const std::array<std::vector<unsigned>,5> roots{{{}, {0,3,4,5}, {3,5,6,7}, {3,4,5,6,7}, {0,3,4,5,6}}};
        for (auto mod:roots[dirty])
            PC_CHECK(pc_item_add_mod(&root,static_cast<pc_affix_side>(session->gen_type[mod]),
                mod,session->primary_group[mod],0,nullptr)==PC_RESULT_OK);
        CalcContext baseline_calc(session,goal,registry,{chaos});
        const auto baseline=evaluate_compiled(session,
            compile_finder_candidate_json(baseline_calc,root,{chaos},caps),prices);
        PC_CHECK(finder_evaluation_accepted(baseline));
        // A human-style guarantee acquisition followed by a correctly
        // dominated rewrite is the same generic producer in either direction.
        // Request and validate the exact reached-entry census, not just root EV.
        for (const auto side:{PC_SIDE_PREFIX,PC_SIDE_SUFFIX}) {
            CalcContext diagnostic(session,goal,registry,actions,false,false,false,
                std::nullopt,std::vector<CountObservation>{},false,std::vector<std::uint64_t>{},true);
            SelectiveCompletionProducer producer(diagnostic,root,prices,caps,
                SelectiveCompletionVariant::RetentionControl,essences[side],side);
            for (unsigned i=0;i<40000 && !producer.done();++i) producer.advance();
            PC_CHECK(producer.done() && producer.candidate().has_value());
            if (!producer.candidate()) continue;
            const auto& control=producer.candidate()->control;
            const auto graph=compile_finder_control_json(diagnostic,root,control,caps);
            const auto prepared=prepare_finder_candidate(diagnostic,session,root,graph,&control);
            PC_CHECK(prepared.ready());
            if (!prepared.ready()) continue;
            auto economy=std::make_shared<EconomyImpl>();economy->id="mixed-diagnostic";economy->prices=prices;
            StrategyEvalOptions eval;eval.economy=economy;
            eval.continuation_entries.push_back({diagnostic.intern_item(root),0,1,root,false});
            eval.graph_local_provenance.strategy_json=graph;
            for (unsigned node=0;node<control.nodes.size();++node) {
                const auto& cn=control.nodes[node];
                if (cn.kind!=FinderControlKind::RunNativeProgram) continue;
                const auto& binding=control.programs.at(cn.binding);
                const auto key=planner_operator_semantic_key(diagnostic.operators().at(binding.operator_index));
                const auto id="c"+std::to_string(node);
                eval.graph_local_provenance.decisions.push_back({id,key,false,false});
                StrategyPolicyDecisionRequest request;request.compiled_node_id=id;
                request.selected_operator_identity=key;request.graph_local=true;
                eval.policy_decision_entries.push_back(std::move(request));
            }
            const auto checked=evaluate_strategy(*prepared.strategy,eval);
            PC_CHECK(finder_evaluation_accepted(checked));
            PC_CHECK(checked.total_expected_cost<baseline.total_expected_cost);
            PC_CHECK(checked.expected_consumption.contains("eldritch_chaos") && checked.expected_consumption.at("eldritch_chaos")>0);
            const auto essence_key=registry.actions[essences[side]].id;
            const bool rewrite_existing_pair=(dirty==3 && side==PC_SIDE_PREFIX) || (dirty==4 && side==PC_SIDE_SUFFIX);
            const double acquisition_visits=checked.expected_consumption.contains(essence_key)
                ? checked.expected_consumption.at(essence_key) : 0;
            PC_CHECK(rewrite_existing_pair ? acquisition_visits==0 : acquisition_visits>0);
            // The empty original root requires paid native dominance setup.
            bool paid_setup=false;
            for (const auto& [id,count]:checked.expected_consumption)
                paid_setup|=(id.starts_with("eldritch_ember:") || id.starts_with("eldritch_ichor:")) && count>0;
            PC_CHECK(paid_setup);
            SelectiveProgrammeEntryValidator validator(diagnostic,session,control,checked.policy_entries,prices,caps);
            for (unsigned i=0;i<40000 && !validator.done();++i) validator.advance();
            PC_CHECK(validator.done() && validator.positive_entries()>0);
            std::printf("mixed-side diagnostic dirty=%u held=%u cost=%.12g acquire=%.12g positive_entries=%u\n",
                dirty,side,checked.total_expected_cost,acquisition_visits,validator.positive_entries());
        }
        CalcContext calc(session,goal,registry,actions);
        PolicyFinderWork finder(calc,session,root,prices,caps);
        for (unsigned i=0;i<40000 && !finder.progress().done;++i) finder.step(128);
        PC_CHECK(finder.progress().done && finder.progress().considered<=8);
        PC_CHECK(finder.best().has_value());
        if (finder.best()) {
            const auto& best=*finder.best();
            const auto checked=evaluate_compiled(session,best.strategy_json,prices);
            PC_CHECK(finder_evaluation_accepted(checked));
            PC_CHECK(best.native_control.has_value());
            PC_CHECK(best.expected_cost<baseline.total_expected_cost);
            PC_CHECK(best.strategy_json.find("eldritch_chaos")!=std::string::npos);
            PC_CHECK(checked.expected_consumption.contains("eldritch_chaos") &&
                checked.expected_consumption.at("eldritch_chaos")>0);
            PC_CHECK(std::abs(checked.total_expected_cost-best.expected_cost)<1e-8);
            std::printf("mixed-side product Finder dirty=%u cost=%.12g\n",dirty,best.expected_cost);
        }
        CalcContext current_calc(session,goal,registry,actions);
        const auto current=solve(current_calc,root,prices,caps);
        PC_CHECK(current.policy_available);
        PC_CHECK(current.options.product_original_root_continuations);
        PC_CHECK(current.lower_bound==0 && current.closure_unavailable_by_profile);
        if (current.policy_available) {
            const auto graph=!current.refined_policy_artifact.strategy_json.empty()
                ? current.refined_policy_artifact.strategy_json
                : compile_policy_strategy_json(current_calc,current,"mixed-side product fixture");
            const auto checked=evaluate_compiled(session,graph,prices);
            PC_CHECK(finder_evaluation_accepted(checked));
            PC_CHECK(graph.find("eldritch_chaos")!=std::string::npos);
            PC_CHECK(current.evaluated_policy_cost<baseline.total_expected_cost);
            PC_CHECK(std::abs(current.evaluated_policy_cost-checked.total_expected_cost)<1e-8);
            std::printf("mixed-side product Current dirty=%u cost=%.12g status=%s\n",dirty,
                current.evaluated_policy_cost,current.diagnostics.selective_completion_service_status.c_str());
        }
        auto explicit_gate=caps;
        explicit_gate.solve_profile_override_mask|=PC_SOLVE_PROFILE_OVERRIDE_GOAL_PROGRESS_GATED_REFORGES;
        PC_CHECK(!product_original_root_continuation_scope(current_calc,root,explicit_gate));
        auto gap=caps; gap.max_absolute_optimality_gap=1;
        PC_CHECK(!product_original_root_continuation_scope(current_calc,root,gap));
        // Exercise normal public parsing/options/graph transport with the same
        // caller action set and fully priced automatic native dependencies.
        pc_session public_session; public_session.impl=session;
        const std::string request=R"({"version":"v1","rarity":"rare","automatic_candidates":true,"slots":[{"family_mod_key":"mod3","min_tier":1},{"family_mod_key":"mod4","min_tier":1},{"family_mod_key":"mod5","min_tier":1},{"family_mod_key":"mod6","min_tier":1}],"actions":["chaos","essence:mixed_prefix","essence:mixed_suffix"]})";
        std::string economy_json="{\"version\":\"v1\",\"prices\":{";
        for (const auto& [id,price]:prices) {
            if (economy_json.back()!='{') economy_json+=',';
            economy_json+='"'+id+"\":"+std::to_string(price);
        }
        economy_json+="}}";
        pc_error_info error{}; pc_economy_handle economy=nullptr;
        PC_CHECK(pc_economy_load_json(economy_json.data(),economy_json.size(),&economy,&error)==PC_RESULT_OK);
        for (auto mode:{PC_SOLVER_MODE_CURRENT,PC_SOLVER_MODE_STRATEGY_FINDER}) {
            pc_solver_handle handle=nullptr;
            PC_CHECK(pc_solver_create(&public_session,request.data(),request.size(),&handle,&error)==PC_RESULT_OK);
            if (!handle) continue;
            pc_solve_options options{}; options.struct_size=sizeof(options);options.abi_version=PC_ABI_VERSION;
            options.solver_mode=mode; options.solve_profile=PC_SOLVE_PROFILE_CALCULATOR_PRODUCT_V1;
            options.max_states=10000; options.max_discovered_states=10000;options.max_expanded_states=10000;
            options.max_state_action_rows=100000;options.max_transitions=1000000;
            options.max_reforge_work=1000000;options.max_solver_owned_bytes=256ull<<20;
            pc_solve_summary summary{};
            const auto code=pc_solver_solve(handle,&root,economy,&options,&summary,&error);
            if(code!=PC_RESULT_OK) std::printf("mixed-side API mode=%u error=%s\n",mode,error.message);
            PC_CHECK(code==PC_RESULT_OK && summary.policy_available);
            if(code==PC_RESULT_OK && summary.policy_available) {
                size_t size=0;
                PC_CHECK(pc_solver_compile_strategy(handle,nullptr,0,&size,&error)==PC_RESULT_OK);
                std::string graph(size+1,'\0');
                PC_CHECK(pc_solver_compile_strategy(handle,graph.data(),graph.size(),&size,&error)==PC_RESULT_OK);
                graph.resize(size);
                const auto checked=evaluate_compiled(session,graph,prices);
                PC_CHECK(finder_evaluation_accepted(checked));
                PC_CHECK(graph.find("eldritch_chaos")!=std::string::npos);
                PC_CHECK(checked.total_expected_cost<baseline.total_expected_cost);
                PC_CHECK(std::abs(summary.evaluated_policy_cost-checked.total_expected_cost)<1e-8);
            }
            pc_solver_destroy(handle);
        }
        pc_economy_destroy(economy);
    }
}


void run_solver_uniform_removal_tests() {
    // Exhaustive finite native differential fixtures, never real-data timed
    // benchmarks. Unequal weights and overlapping exclusion groups are retained.
    auto session = make_compile_session();
    auto data = std::const_pointer_cast<DataImpl>(session->data);
    data->essence_count = 1;
    data->strings.push_back("removal_guarantee");
    data->essence_key_sids = {static_cast<std::uint32_t>(data->strings.size()-1)};
    data->essence_by_key = {{"removal_guarantee",0}};
    data->essence_item_level_restrictions = {-1};
    data->essence_is_corruption_only = {0};
    session->essence_guaranteed_mod_ids = {0};
    auto registry = build_action_registry(*session);
    const auto chaos = registry.index_by_id.at("chaos");
    const auto annul = registry.index_by_id.at("annul");
    const auto essence = registry.index_by_id.at("essence:removal_guarantee");
    GoalSpec goal; goal.rarity = PC_RARITY_RARE;
    for (const auto mod : {0u,5u}) {
        GoalSlot slot; slot.family_id = session->family_id[mod]; slot.min_tier = 1;
        goal.slots.push_back(slot);
    }
    const std::vector<std::uint32_t> actions{chaos,essence,annul};
    const auto make_calc = [&](const bool compact, const GoalSpec& request) {
        return std::make_unique<CalcContext>(session,request,registry,actions,
            false,false,!compact,std::nullopt,std::vector<CountObservation>{},
            compact,std::vector<std::uint64_t>{},!compact,false,false,false,
            compact,nullptr,true,compact);
    };
    auto full = make_calc(false,goal), compact = make_calc(true,goal);
    PC_CHECK(compact->uses_certified_uniform_removal());
    PC_CHECK(!full->uses_certified_uniform_removal());
    PC_CHECK(native_unprotected_affix_law(*session,registry.actions[essence]) == UnprotectedAffixLaw::FullRenewal);
    PC_CHECK(native_unprotected_affix_law(*session,registry.actions[annul]) == UnprotectedAffixLaw::UniformRemoval);
    for (const auto id : {"exalt","augment","regal","fracture","scour","restart","eldritch_annul"})
        PC_CHECK(native_unprotected_affix_law(*session,registry.actions.at(registry.index_by_id.at(id))) == UnprotectedAffixLaw::Unsupported);
    auto spoof = registry.actions[annul];
    spoof.refinement.affix_observations.push_back({refinement_feature(RefinementFeature::ModifierExclusionSignature),{}});
    PC_CHECK(native_unprotected_affix_law(*session,spoof) == UnprotectedAffixLaw::Unsupported);
    const auto compare_maps = [](const std::map<std::uint32_t,double>& a,const std::map<std::uint32_t,double>& b) {
        PC_CHECK(a.size() == b.size());
        double mass_a=0,mass_b=0;
        for (const auto& [state,p] : a) {
            mass_a+=p;
            const auto at=b.find(state);
            PC_CHECK(at!=b.end());
            if (at!=b.end()) PC_CHECK(std::abs(p-at->second)<1e-11);
        }
        for (const auto& [state,p] : b) { (void)state; mass_b+=p; }
        PC_CHECK(std::abs(mass_a-1)<1e-11);
        PC_CHECK(std::abs(mass_b-1)<1e-11);
    };
    const auto full_projected = [&](const OutcomeDistribution& row) {
        std::map<std::uint32_t,double> result;
        for (const auto& e : row.entries) {
            pc_item_state item; PC_CHECK(full->materialize(e.state,item));
            result[compact->intern_item(item)] += e.probability;
        }
        return result;
    };
    const auto compact_map = [](const OutcomeDistribution& row) {
        std::map<std::uint32_t,double> result;
        for (const auto& e : row.entries) result[e.state]+=e.probability;
        return result;
    };
    std::uint32_t carriers=0;
    for (std::uint32_t mask=0;mask<256;++mask) {
        pc_item_state item; pc_item_clear(&item); item.rarity=PC_RARITY_RARE;
        bool legal=true; std::set<std::uint32_t> groups;
        for (std::uint32_t mod=0;mod<8;++mod) if ((mask>>mod)&1u) {
            for (auto i=session->group_offsets[mod];i<session->group_offsets[mod+1];++i)
                if (!groups.insert(session->group_ids[i]).second) legal=false;
            if (pc_item_add_mod(&item,session->gen_type[mod],mod,session->primary_group[mod],0,nullptr)!=PC_RESULT_OK) legal=false;
        }
        if (!legal) continue;
        ++carriers;
        const auto f=full->intern_item(item), q=compact->intern_item(item);
        compare_maps(full_projected(full->outcomes(f,annul)),compact_map(compact->outcomes(q,annul)));
        // Independent native uniform-removal oracle: delete each occupied
        // physical slot directly. Neither calculator's Annul row contributes
        // the expected distribution, and generation weights are irrelevant.
        std::map<std::uint32_t,double> native_removed;
        const unsigned count = item.prefix_count + item.suffix_count;
        for (const int side : {PC_SIDE_PREFIX, PC_SIDE_SUFFIX}) {
            const unsigned side_count = side == PC_SIDE_PREFIX ? item.prefix_count : item.suffix_count;
            for (unsigned index = 0; index < side_count; ++index) {
                auto next = item;
                PC_CHECK(pc_item_remove_at(&next,side,index) == PC_RESULT_OK);
                native_removed[compact->intern_item(next)] += 1.0 / count;
            }
        }
        if (count == 0) native_removed[q] = 1;
        compare_maps(native_removed,compact_map(compact->outcomes(q,annul)));
        // All prior unprotected group identity is wiped by either native
        // renewal. Forced Essence mods, within-roll collisions and exhaustion
        // remain in the same exact native roll DP, independently rebuilt.
        for (const auto action : {chaos,essence})
            compare_maps(full_projected(full->outcomes(f,action)),compact_map(compact->outcomes(q,action)));
    }
    PC_CHECK(carriers>50);
    pc_item_state four; pc_item_clear(&four); four.rarity=PC_RARITY_RARE;
    for (const auto mod : {0u,5u,3u,6u})
        PC_CHECK(pc_item_add_mod(&four,session->gen_type[mod],mod,session->primary_group[mod],0,nullptr)==PC_RESULT_OK);
    const auto qfour=compact->intern_item(four);
    std::map<std::uint32_t,double> after_two;
    const auto first=compact->outcomes(qfour,annul);
    for (const auto& e:first.entries) {
        const auto second=compact->outcomes(e.state,annul);
        for (const auto& z:second.entries) after_two[z.state]+=e.probability*z.probability;
    }
    double retained_both=0;
    for (const auto& [state,p]:after_two)
        if (compact->state(state).slot_status[0]==2 && compact->state(state).slot_status[1]==2) retained_both+=p;
    PC_CHECK(std::abs(retained_both-1.0/6)<1e-12);
    std::map<std::uint32_t,double> physical_after_two;
    const auto physical_first=full->outcomes(full->intern_item(four),annul);
    for (const auto& e:physical_first.entries) {
        const auto physical_second=full->outcomes(e.state,annul);
        for (const auto& z:physical_second.entries) {
            pc_item_state item; PC_CHECK(full->materialize(z.state,item));
            physical_after_two[compact->intern_item(item)]+=e.probability*z.probability;
        }
    }
    compare_maps(physical_after_two,after_two);
    double physical_retained_both=0;
    for (const auto& [state,p]:physical_after_two)
        if (compact->state(state).slot_status[0]==2 && compact->state(state).slot_status[1]==2) physical_retained_both+=p;
    PC_CHECK(std::abs(physical_retained_both-1.0/6)<1e-12);

    pc_item_state root; pc_item_clear(&root); root.rarity=PC_RARITY_RARE;
    SolveOptions limits;
    FinderControlGraph controller{{
        {FinderControlKind::TestGoal,kNoId,5,1},
        {FinderControlKind::TestSlot,0,2,4},
        {FinderControlKind::TestAffixCountAtLeast4,3,3,4},
        {FinderControlKind::RunPrimitive,annul,kNoId,kNoId,0},
        {FinderControlKind::RunPrimitive,essence,kNoId,kNoId,0},
        {FinderControlKind::GoalTerminal}},0};
    const auto graph=compile_finder_control_json(*full,root,controller,limits);
    PC_CHECK(compiled_success_ingress_matches_request(*full,graph));
    const auto compiled=compile_strategy_json(session,graph.data(),graph.size());
    auto economy=std::make_shared<EconomyImpl>(); economy->id="finite-native-removal";
    economy->prices={{"chaos",1},{"annul",0.03},{"essence:removal_guarantee",0.2}};
    StrategyEvalOptions options; options.economy=economy;
    const auto compact_result=evaluate_strategy(*compiled,options);
    options.use_exact_exchangeable_family_compression=false;
    const auto physical_result=evaluate_strategy(*compiled,options);
    PC_CHECK(compact_result.observation_propagation.uniform_removal_carrier);
    PC_CHECK(!physical_result.observation_propagation.uniform_removal_carrier);
    PC_CHECK(finder_evaluation_accepted(compact_result));
    PC_CHECK(finder_evaluation_accepted(physical_result));
    PC_CHECK(std::abs(compact_result.success_probability-physical_result.success_probability)<1e-10);
    PC_CHECK(std::abs(compact_result.total_expected_cost-physical_result.total_expected_cost)<1e-9);
    PC_CHECK(std::abs(compact_result.expected_actions-physical_result.expected_actions)<1e-8);
    PC_CHECK(compact_result.expected_consumption.size()==physical_result.expected_consumption.size());
    for (const auto& [key,quantity]:physical_result.expected_consumption)
        PC_CHECK(std::abs(compact_result.expected_consumption.at(key)-quantity)<1e-8);
    PC_CHECK(compact_result.edges.size()==physical_result.edges.size());
    for (std::size_t i=0;i<compact_result.edges.size();++i) {
        PC_CHECK(compact_result.edges[i].id==physical_result.edges[i].id);
        PC_CHECK(std::abs(compact_result.edges[i].expected_traversals-physical_result.edges[i].expected_traversals)<1e-8);
    }
    std::printf("finite uniform-removal carriers=%u two-Annul retain-both=%.12g physical-retain=%.12g checked cost=%.12g physical=%.12g\n",
        carriers,retained_both,physical_retained_both,compact_result.total_expected_cost,physical_result.total_expected_cost);
    options.use_exact_exchangeable_family_compression=true;
    // Nonempty and protected roots do not enter the quotient. Other tests
    // retain source-engine fracture/lock semantics; these verify admission.
    for (const auto flag : {0u,static_cast<unsigned>(PC_MOD_SLOT_FRACTURED),static_cast<unsigned>(PC_MOD_SLOT_CRAFTED)}) {
        auto variant=std::make_shared<StrategyImpl>(*compiled);
        PC_CHECK(pc_item_add_mod(&variant->start_item,PC_SIDE_PREFIX,0,session->primary_group[0],flag,nullptr)==PC_RESULT_OK);
        const auto evaluated=evaluate_strategy(*variant,options);
        PC_CHECK(!evaluated.observation_propagation.uniform_removal_carrier);
    }
    // An extra discovery seed retains physical entry identities/certification.
    auto seeded=options; StrategyContinuationEntryRequest entry;
    entry.complete_member_count=1; entry.item=root; seeded.continuation_entries.push_back(entry);
    const auto seeded_result=evaluate_strategy(*compiled,seeded);
    PC_CHECK(!seeded_result.observation_propagation.uniform_removal_carrier);

    {
        auto legacy=controller; legacy.nodes[2].binding=kNoId;
        const auto bytes=compile_finder_control_json(*full,root,legacy,limits);
        const auto compiled_legacy=compile_strategy_json(session,bytes.data(),bytes.size());
        options.use_exact_exchangeable_family_compression=true;
        const auto q=evaluate_strategy(*compiled_legacy,options);
        options.use_exact_exchangeable_family_compression=false;
        const auto f=evaluate_strategy(*compiled_legacy,options);
        PC_CHECK(q.observation_propagation.uniform_removal_carrier);
        PC_CHECK(!finder_evaluation_accepted(q) && !finder_evaluation_accepted(f));
        PC_CHECK(q.success_probability<1e-12 && f.success_probability<1e-12);
    }
    options.use_exact_exchangeable_family_compression=true;
    {
        // Actual structured exclusion read, even on a dead graph edge, must
        // prevent admission. No unreachability theorem is supplied for it.
        auto variant=std::make_shared<StrategyImpl>(*compiled);
        auto program=std::make_shared<refinement::CompiledObservationProgram>();
        program->requirement=exclusion_requirement();
        StrategyNode dead; dead.id="dead_identity_read"; dead.kind=StrategyNodeKind::Router;
        StrategyEdge edge; edge.id="dead_identity_edge"; edge.target=variant->node_by_id.at("c5");
        edge.condition.kind=ConditionKind::ObservationSignature;
        edge.condition.observation_program=program;
        dead.edges.push_back(edge);
        variant->node_by_id[dead.id]=static_cast<std::uint32_t>(variant->nodes.size());
        variant->nodes.push_back(dead);
        const auto evaluated=evaluate_strategy(*variant,options);
        PC_CHECK(!evaluated.observation_propagation.uniform_removal_carrier);
        PC_CHECK(std::abs(evaluated.total_expected_cost-physical_result.total_expected_cost)<1e-9);
    }
    {
        auto variant=std::make_shared<StrategyImpl>(*compiled);
        auto& op=variant->nodes.at(variant->node_by_id.at("c4"));
        op.action={}; op.action.type=ActionType::Exalt;
        op.action_type=static_cast<int>(ActionType::Exalt); op.price_keys={"exalt"};
        const auto evaluated=evaluate_strategy(*variant,options);
        PC_CHECK(!evaluated.observation_propagation.uniform_removal_carrier);
    }
    {
        // An any-two goal still uses the original threshold; observing all
        // three slots is not permission to promote it to all-three success.
        auto any_goal=goal; GoalSlot third; third.family_id=session->family_id[3]; third.min_tier=1;
        any_goal.slots.push_back(third); any_goal.min_satisfied_slots=2;
        auto any_full=make_calc(false,any_goal);
        const auto bytes=compile_finder_control_json(*any_full,root,controller,limits);
        PC_CHECK(compiled_success_ingress_matches_request(*any_full,bytes));
        const auto any_strategy=compile_strategy_json(session,bytes.data(),bytes.size());
        auto qoptions=options; qoptions.use_exact_exchangeable_family_compression=true;
        auto foptions=qoptions; foptions.use_exact_exchangeable_family_compression=false;
        const auto q=evaluate_strategy(*any_strategy,qoptions), f=evaluate_strategy(*any_strategy,foptions);
        PC_CHECK(q.observation_propagation.uniform_removal_carrier);
        PC_CHECK(finder_evaluation_accepted(q) && finder_evaluation_accepted(f));
        PC_CHECK(std::abs(q.total_expected_cost-f.total_expected_cost)<1e-9);
        PC_CHECK(std::abs(q.success_probability-f.success_probability)<1e-10);
    }
    {
        auto tiny=options; tiny.max_owned_bytes=1024;
        bool refused=false;
        try { StrategyEvalWork work(compiled,tiny); }
        catch (const std::length_error& ex) { refused=std::string(ex.what()).find("max_owned_bytes")!=std::string::npos; }
        PC_CHECK(refused);
    }
    {
        // Two independent non-member exclusions block the same goal: their
        // multiplicities survive one removal and disappear only after both.
        auto overlap=std::make_shared<SessionImpl>(*session);
        std::vector<std::vector<std::uint32_t>> groups(overlap->mod_count);
        for (std::uint32_t mod=0;mod<overlap->mod_count;++mod)
            groups[mod].assign(overlap->group_ids.begin()+overlap->group_offsets[mod],
                overlap->group_ids.begin()+overlap->group_offsets[mod+1]);
        groups[0].push_back(20);
        const auto rebuild_groups=[&] {
            overlap->group_offsets={0}; overlap->group_ids.clear();
            overlap->group_masks.assign(32,std::vector<std::uint64_t>(overlap->words,0));
            for (std::uint32_t mod=0;mod<overlap->mod_count;++mod) {
                for (const auto group:groups[mod]) {
                    overlap->group_ids.push_back(group);
                    pc_bitset_set(overlap->group_masks[group].data(),mod);
                }
                overlap->group_offsets.push_back(static_cast<std::uint32_t>(overlap->group_ids.size()));
            }
        };
        rebuild_groups();
        GoalSpec single=goal; single.slots.resize(1);
        auto oregistry=build_action_registry(*overlap);
        const auto make_overlap=[&](const bool q,const GoalSpec& g) {
            return std::make_unique<CalcContext>(overlap,g,oregistry,actions,false,false,!q,
                std::nullopt,std::vector<CountObservation>{},q,std::vector<std::uint64_t>{},
                !q,false,false,false,q,nullptr,true,q);
        };
        auto oq=make_overlap(true,single), of=make_overlap(false,single);
        pc_item_state blockers=root;
        for (const auto mod:{2u,5u}) PC_CHECK(pc_item_add_mod(&blockers,overlap->gen_type[mod],mod,overlap->primary_group[mod],0,nullptr)==PC_RESULT_OK);
        const auto input=oq->intern_item(blockers); PC_CHECK(oq->state(input).blocked_mask==1);
        const auto first=oq->outcomes(input,annul); PC_CHECK(first.entries.size()==2);
        for (const auto& e:first.entries) {
            PC_CHECK(oq->state(e.state).blocked_mask==1);
            const auto second=oq->outcomes(e.state,annul);
            PC_CHECK(second.entries.size()==1);
            if (second.entries.size()==1) PC_CHECK(oq->state(second.entries[0].state).blocked_mask==0);
        }
        // Existing common group plus an additional cross-goal group: all
        // members must have uniform effects, independently of tier status.
        PC_CHECK(!prove_uniform_removal_goals(*overlap,goal).has_value());
        groups[1].push_back(20); rebuild_groups();
        PC_CHECK(!prove_uniform_removal_goals(*overlap,goal).has_value());
        bool refused=false;
        try { auto inadmissible=make_overlap(true,goal); }
        catch (const std::invalid_argument&) { refused=true; }
        PC_CHECK(refused);
        const auto rejected_graph=compile_strategy_json(overlap,graph.data(),graph.size());
        StrategyEvalWork rejected_work(rejected_graph,options);
        PC_CHECK(!rejected_work.diagnostic_result().observation_propagation.uniform_removal_carrier);
        // A conflict only between two below-tier goal members is still a
        // native exclusion. Neither satisfying mask mentions this group.
        auto below_overlap=std::make_shared<SessionImpl>(*session);
        below_overlap->family_id[6]=below_overlap->family_id[5];
        below_overlap->family_tier_index[6]=2;
        auto below_groups=groups;
        below_groups[0]={10}; below_groups[1]={10,21}; below_groups[6]={20,21};
        below_overlap->group_offsets={0}; below_overlap->group_ids.clear();
        below_overlap->group_masks.assign(32,std::vector<std::uint64_t>(below_overlap->words,0));
        for (std::uint32_t mod=0;mod<below_overlap->mod_count;++mod) {
            for (const auto group:below_groups[mod]) {
                below_overlap->group_ids.push_back(group);
                pc_bitset_set(below_overlap->group_masks[group].data(),mod);
            }
            below_overlap->group_offsets.push_back(static_cast<std::uint32_t>(below_overlap->group_ids.size()));
        }
        PC_CHECK(!prove_uniform_removal_goals(*below_overlap,goal).has_value());
        const auto below_graph=compile_strategy_json(below_overlap,graph.data(),graph.size());
        StrategyEvalWork below_work(below_graph,options);
        PC_CHECK(!below_work.diagnostic_result().observation_propagation.uniform_removal_carrier);
        // Same-side independent blockers share one quotient class. Native
        // removal is 2/2 into count one, then 1/1 into count zero; clearing
        // the blocker bit after the first removal would change future truth.
        auto same_side=std::make_shared<SessionImpl>(*overlap);
        same_side->gen_type[5]=PC_SIDE_PREFIX;
        auto side_registry=build_action_registry(*same_side);
        const auto make_side=[&](const bool q) {
            return std::make_unique<CalcContext>(same_side,single,side_registry,actions,false,false,!q,
                std::nullopt,std::vector<CountObservation>{},q,std::vector<std::uint64_t>{},
                !q,false,false,false,q,nullptr,true,q);
        };
        auto sq=make_side(true), sf=make_side(false);
        pc_item_state side_blockers=root;
        for (const auto mod:{2u,5u}) PC_CHECK(pc_item_add_mod(&side_blockers,PC_SIDE_PREFIX,mod,same_side->primary_group[mod],0,nullptr)==PC_RESULT_OK);
        const auto side_input=sq->intern_item(side_blockers);
        const auto blocker_class=sq->layout().junk_class_by_mod[2];
        PC_CHECK(blocker_class==sq->layout().junk_class_by_mod[5]);
        PC_CHECK(sq->state(side_input).junk_counts[blocker_class]==2);
        const auto side_first=sq->outcomes(side_input,annul);
        PC_CHECK(side_first.entries.size()==1);
        std::map<std::uint32_t,double> qfirst,ffirst;
        for (const auto& e:side_first.entries) {
            qfirst[e.state]+=e.probability;
            PC_CHECK(e.probability==1 && sq->state(e.state).blocked_mask==1);
            PC_CHECK(sq->state(e.state).junk_counts[blocker_class]==1);
            const auto side_second=sq->outcomes(e.state,annul);
            PC_CHECK(side_second.entries.size()==1);
            if (side_second.entries.size()==1) {
                PC_CHECK(side_second.entries[0].probability==1);
                PC_CHECK(sq->state(side_second.entries[0].state).blocked_mask==0);
                PC_CHECK(sq->state(side_second.entries[0].state).junk_counts[blocker_class]==0);
            }
        }
        const auto full_side_first=sf->outcomes(sf->intern_item(side_blockers),annul);
        for (const auto& e:full_side_first.entries) {
            pc_item_state item; PC_CHECK(sf->materialize(e.state,item));
            ffirst[sq->intern_item(item)]+=e.probability;
        }
        compare_maps(ffirst,qfirst);
        // Preserve a complete differential in the admitted one-goal domain,
        // including the two independent blocker groups. No mass is dropped.
        oq=make_overlap(true,single); of=make_overlap(false,single);
        for (const auto action:{chaos,essence}) {
            const auto frow=of->outcomes(of->intern_item(root),action);
            const auto qrow=oq->outcomes(oq->intern_item(root),action);
            std::map<std::uint32_t,double> fm,qm;
            for (const auto& e:frow.entries) {
                pc_item_state item; PC_CHECK(of->materialize(e.state,item));
                fm[oq->intern_item(item)]+=e.probability;
            }
            for (const auto& e:qrow.entries) qm[e.state]+=e.probability;
            compare_maps(fm,qm);
        }
    }
    {
        // Exhausted pool: three candidates share one group, so a forced
        // Essence consumes the only selectable family. Keep the one-affix
        // outcome with full mass; do not invent missing affixes or renormalize.
        auto exhausted=std::make_shared<SessionImpl>(*session);
        exhausted->normal_random_roll_mask.assign(exhausted->words,0);
        for (const auto mod:{0u,1u,2u}) pc_bitset_set(exhausted->normal_random_roll_mask.data(),mod);
        auto eregistry=build_action_registry(*exhausted);
        auto make_exhausted=[&](const bool q) {
            return std::make_unique<CalcContext>(exhausted,goal,eregistry,actions,false,false,!q,
                std::nullopt,std::vector<CountObservation>{},q,std::vector<std::uint64_t>{},
                !q,false,false,false,q,nullptr,true,q);
        };
        auto eq=make_exhausted(true), ef=make_exhausted(false);
        const auto qrow=eq->outcomes(eq->intern_item(root),essence);
        const auto frow=ef->outcomes(ef->intern_item(root),essence);
        PC_CHECK(qrow.entries.size()==1 && frow.entries.size()==1);
        if (qrow.entries.size()==1 && frow.entries.size()==1) {
            const auto& q=eq->state(qrow.entries[0].state); const auto& f=ef->state(frow.entries[0].state);
            PC_CHECK(q.prefix_count==1 && q.suffix_count==0 && q.slot_status[0]==2);
            PC_CHECK(f.prefix_count==1 && f.suffix_count==0 && f.slot_status[0]==2);
            PC_CHECK(qrow.entries[0].probability==1 && frow.entries[0].probability==1);
        }
    }
    // Native single-occupancy proof rejects a synthetic family spanning two
    // simultaneously legal groups, even with otherwise disjoint slot masks.
    auto nonexclusive=std::make_shared<SessionImpl>(*session);
    nonexclusive->family_id[3]=nonexclusive->family_id[0];
    PC_CHECK(!prove_uniform_removal_goals(*nonexclusive,goal).has_value());
}
