#include "tests.hpp"

#include "../src/json.hpp"
#include "../src/solver_compile_contracts.hpp"
#include "../src/solver_options_helpers.hpp"
#include "../src/solver_policy_refinement.hpp"
#include "../src/solver_sparse_policy.hpp"
#include "../src/solver_solve_types.hpp"
#include "poecraft/bitset.h"
#include "poecraft/item_state.h"

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <deque>
#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

using namespace poecraft;
using namespace poecraft::solver;

namespace poecraft::solver {

struct SolveWorkTestAccess {
    using Impl = SolveWork::Impl;
};

} // namespace poecraft::solver

namespace {

std::shared_ptr<SessionImpl> make_solve_session(
    const std::vector<std::string>& essence_keys = {});

std::size_t count_occurrences(
    const std::string& value,
    const std::string& needle) {
    std::size_t count = 0;
    std::size_t offset = 0;
    while ((offset = value.find(needle, offset)) !=
           std::string::npos) {
        ++count;
        offset += needle.size();
    }
    return count;
}

void run_bounded_policy_row_capture_tests() {
    auto session = make_solve_session();
    ActionRegistry registry = build_action_registry(*session);
    GoalSpec goal;
    goal.rarity = PC_RARITY_RARE;
    GoalSlot slot;
    slot.family_id = 100;
    slot.min_tier = 1;
    goal.slots.push_back(slot);
    CalcContext calc(
        session, goal, registry,
        {registry.index_by_id.at("alchemy"),
         registry.index_by_id.at("chaos"),
         registry.index_by_id.at("restart")});
    pc_item_state start;
    pc_item_clear(&start);
    start.rarity = PC_RARITY_RARE;
    const std::uint32_t state = calc.intern_item(start);
    PC_CHECK(!calc.operators().empty());

    SolveTransitionCache source;
    SparseRow row;
    row.owner_state = state;
    source.rows.push_back(row);
    source.choice_options.push_back(
        OutcomeChoiceOption{7, state, state, state});
    std::vector<PricedSparseRow> priced(1);
    priced[0].operator_index = 0;
    priced[0].cost = 17.0;
    priced[0].choice_option_count = 1;
    const solve_detail::CapturedBoundedPolicyRow captured =
        solve_detail::capture_bounded_policy_row(
            calc, source, priced, state, 0, kNoId);

    source.rows[0].owner_state = state + 1;
    priced[0].operator_index =
        calc.operators().size() > 1 ? 1 : 0;
    priced[0].cost = 3.0;
    source.choice_options[0].mod_id = 2;
    PC_CHECK(captured.policy.index == 0);
    PC_CHECK(captured.cost == 17.0);
    PC_CHECK(captured.choice_options.size() == 1);
    PC_CHECK(captured.choice_options[0].mod_id == 7);
    bool stale_row_rejected = false;
    try {
        (void)solve_detail::capture_bounded_policy_row(
            calc, source, priced, state, 0, kNoId);
    } catch (const std::logic_error&) {
        stale_row_rejected = true;
    }
    PC_CHECK(stale_row_rejected);
}

void run_certified_fallback_contract_tests() {
    using solve_detail::CertifiedFallbackContract;
    using solve_detail::CertifiedFallbackCurrentContext;

    CertifiedFallbackCurrentContext current;
    current.goal_identity = 11;
    current.economy_identity = 12;
    current.action_vocabulary_identity = 13;
    current.action_vocabulary_size = 27;
    current.artifact_identity = 14;
    current.source_generation = 101;
    current.target_generation = 102;
    current.graph_prefix_identity = 15;

    CertifiedFallbackContract fallback;
    fallback.certified_upper_bound = 4000.0;
    fallback.evaluated_policy_cost = 4000.0;
    fallback.goal_identity = current.goal_identity;
    fallback.economy_identity = current.economy_identity;
    fallback.action_vocabulary_identity =
        current.action_vocabulary_identity;
    fallback.action_vocabulary_size =
        current.action_vocabulary_size;
    fallback.artifact_identity = current.artifact_identity;
    fallback.source_generation = 100;
    fallback.target_generation = 100;
    fallback.graph_prefix_identity = current.graph_prefix_identity;
    fallback.witness_identity = 50;
    fallback.portfolio_identity = 60;
    fallback.root_operator = 7;
    fallback.kind = "renewal";
    fallback.complete_policy_or_witness = true;
    fallback.compiled_payload_present = true;
    fallback.compilation_provenance_present = true;
    fallback.independently_evaluated = true;
    fallback.proper = true;
    fallback.executable = true;
    PC_CHECK(
        solve_detail::certified_fallback_invalid_reason(
            fallback, current, 1e-9) == nullptr);

    const auto invalid_reason = [&](CertifiedFallbackContract candidate) {
        const char* reason =
            solve_detail::certified_fallback_invalid_reason(
                candidate, current, 1e-9);
        return reason == nullptr ? std::string{} : std::string{reason};
    };
    CertifiedFallbackContract changed = fallback;
    changed.economy_identity ^= 1;
    PC_CHECK(invalid_reason(changed) == "economy_identity_changed");
    changed = fallback;
    changed.goal_identity ^= 1;
    PC_CHECK(invalid_reason(changed) == "goal_identity_changed");
    changed = fallback;
    changed.graph_prefix_identity ^= 1;
    PC_CHECK(invalid_reason(changed) == "graph_prefix_changed");
    changed = fallback;
    ++changed.action_vocabulary_size;
    PC_CHECK(invalid_reason(changed) == "action_vocabulary_changed");
    changed = fallback;
    changed.action_vocabulary_identity ^= 1;
    PC_CHECK(invalid_reason(changed) == "action_vocabulary_changed");
    changed = fallback;
    changed.artifact_identity ^= 1;
    PC_CHECK(invalid_reason(changed) == "artifact_generation_changed");
    changed = fallback;
    changed.source_generation = current.source_generation + 1;
    PC_CHECK(invalid_reason(changed) == "graph_generation_rewound");
    changed = fallback;
    changed.target_generation = current.target_generation + 1;
    PC_CHECK(invalid_reason(changed) == "graph_generation_rewound");

    changed = fallback;
    changed.proper = false;
    PC_CHECK(invalid_reason(changed) == "improper_policy");
    changed = fallback;
    changed.executable = false;
    PC_CHECK(invalid_reason(changed) == "policy_not_executable");
    changed = fallback;
    changed.compiled_payload_present = false;
    PC_CHECK(invalid_reason(changed) ==
             "retained_artifact_provenance_missing");
    changed = fallback;
    changed.compilation_provenance_present = false;
    PC_CHECK(invalid_reason(changed) ==
             "retained_artifact_provenance_missing");
    changed = fallback;
    changed.complete_policy_or_witness = false;
    PC_CHECK(invalid_reason(changed) ==
             "retained_artifact_provenance_missing");
    changed = fallback;
    changed.independently_evaluated = false;
    PC_CHECK(invalid_reason(changed) ==
             "final_graph_not_independently_evaluated");
    PC_CHECK(
        solve_detail::retained_fallback_invalid_reason(
            changed, current) == nullptr);

    /* A cheaper candidate without executable provenance cannot displace the
     * more expensive certified fallback. A cheaper certified policy can. */
    CertifiedFallbackContract cheaper = fallback;
    cheaper.certified_upper_bound = 3000.0;
    cheaper.evaluated_policy_cost = 3000.0;
    cheaper.portfolio_identity = 61;
    cheaper.compiled_payload_present = false;
    std::vector<CertifiedFallbackContract> candidates{
        cheaper, fallback};
    candidates.erase(
        std::remove_if(
            candidates.begin(), candidates.end(),
            [&](const CertifiedFallbackContract& candidate) {
                return solve_detail::certified_fallback_invalid_reason(
                           candidate, current, 1e-9) != nullptr;
            }),
        candidates.end());
    PC_CHECK(candidates.size() == 1);
    PC_CHECK(candidates.front().certified_upper_bound == 4000.0);

    /* A later failed strict candidate contributes no certified artifact and
     * therefore cannot erase the already executable direct candidate. */
    CertifiedFallbackContract failed_strict_candidate = cheaper;
    failed_strict_candidate.compilation_provenance_present = false;
    if (solve_detail::certified_fallback_invalid_reason(
            failed_strict_candidate, current, 1e-9) == nullptr) {
        candidates.push_back(failed_strict_candidate);
    }
    PC_CHECK(candidates.size() == 1);
    PC_CHECK(candidates.front().portfolio_identity ==
             fallback.portfolio_identity);

    cheaper.compiled_payload_present = true;
    PC_CHECK(
        solve_detail::certified_fallback_precedes(
            cheaper, fallback));
    PC_CHECK(
        !solve_detail::certified_fallback_precedes(
            fallback, cheaper));

    CertifiedFallbackContract equal_left = fallback;
    CertifiedFallbackContract equal_right = fallback;
    equal_left.root_operator = 3;
    equal_right.root_operator = 4;
    PC_CHECK(solve_detail::certified_fallback_precedes(
        equal_left, equal_right));
    equal_right.root_operator = equal_left.root_operator;
    equal_left.witness_identity = 8;
    equal_right.witness_identity = 9;
    PC_CHECK(solve_detail::certified_fallback_precedes(
        equal_left, equal_right));
    std::vector<CertifiedFallbackContract> ordered{
        fallback, equal_right, cheaper, equal_left};
    std::sort(
        ordered.begin(), ordered.end(),
        [](const CertifiedFallbackContract& left,
           const CertifiedFallbackContract& right) {
            return solve_detail::certified_fallback_precedes(
                left, right);
        });
    PC_CHECK(ordered.front().certified_upper_bound == 3000.0);

    PC_CHECK(solve_detail::certified_fallback_fits_memory(
        700, 300, 1000));
    PC_CHECK(!solve_detail::certified_fallback_fits_memory(
        701, 300, 1000));
    PC_CHECK(!solve_detail::certified_fallback_fits_memory(
        1001, 0, 1000));

    SolveResult publication;
    publication.lower_bound = 10.0;
    publication.upper_bound = 10.0;
    publication.policy_available = true;
    publication.policy_status = SolvePolicyStatus::Exact;
    publication.converged = true;
    publication.evaluated_policy_cost = 10.0;
    PC_CHECK(std::string{
        solve_detail::publication_invariant_invalid_reason(publication)} ==
        "finite certified upper bound has no executable artifact");
    publication.refined_policy_artifact.strategy_json = "{}";
    PC_CHECK(
        solve_detail::publication_invariant_invalid_reason(publication) ==
        nullptr);
    publication.upper_bound = 11.0;
    publication.evaluated_policy_cost = 11.0;
    PC_CHECK(std::string{
        solve_detail::publication_invariant_invalid_reason(publication)} ==
        "exact policy status has no closed certified bounds");

    SolveResult tiny_inversion;
    tiny_inversion.policy_status = SolvePolicyStatus::Exact;
    tiny_inversion.lower_bound = 10.0 + 5e-8;
    tiny_inversion.upper_bound = 10.0;
    solve_detail::normalize_publication_result(tiny_inversion);
    PC_CHECK(tiny_inversion.lower_bound == 10.0);
    PC_CHECK(tiny_inversion.absolute_optimality_gap == 0.0);

    SolveResult material_inversion;
    material_inversion.policy_status =
        SolvePolicyStatus::BoundedFeasible;
    material_inversion.lower_bound = 11.0;
    material_inversion.upper_bound = 10.0;
    solve_detail::normalize_publication_result(material_inversion);
    PC_CHECK(material_inversion.lower_bound == 0.0);
    PC_CHECK(material_inversion.absolute_optimality_gap == 10.0);

    SolveResult bounded_equality;
    bounded_equality.policy_status =
        SolvePolicyStatus::BoundedFeasible;
    bounded_equality.lower_bound = 10.0;
    bounded_equality.upper_bound = 10.0;
    solve_detail::normalize_publication_result(bounded_equality);
    PC_CHECK(bounded_equality.lower_bound == 0.0);
    PC_CHECK(std::isinf(bounded_equality.relative_optimality_gap));
}

void run_direct_certification_contract_tests() {
    const auto evaluated = [](
            const double solver_cost,
            const double exact_cost,
            const double success,
            const double failure) {
        refinement::CompiledPolicyAssertion assertion;
        assertion.solver_cost = solver_cost;
        assertion.evaluation.converged = true;
        assertion.evaluation.cost_complete = true;
        assertion.evaluation.total_expected_cost = exact_cost;
        assertion.evaluation.success_probability = success;
        assertion.evaluation.failure_probability = failure;
        refinement::finalize_compiled_policy_assertion(assertion);
        return assertion;
    };

    const refinement::CompiledPolicyAssertion successful =
        evaluated(10.0, 10.0, 1.0, 0.0);
    PC_CHECK(
        successful.status ==
        refinement::CompiledPolicyAssertionStatus::Complete);
    PC_CHECK(successful.executable);
    PC_CHECK(successful.proper);
    PC_CHECK(successful.zero_off_policy);
    PC_CHECK(successful.cost_reconciled);

    const refinement::CompiledPolicyAssertion mismatch =
        evaluated(8.0, 10.0, 1.0, 0.0);
    PC_CHECK(
        mismatch.status ==
        refinement::CompiledPolicyAssertionStatus::CostMismatch);
    PC_CHECK(mismatch.executable);
    PC_CHECK(mismatch.proper);
    PC_CHECK(!mismatch.cost_reconciled);
    PC_CHECK(
        mismatch.failure_classification ==
        "solver_exact_cost_mismatch");

    const refinement::CompiledPolicyAssertion off_policy =
        evaluated(10.0, 10.0, 0.9, 0.1);
    PC_CHECK(
        off_policy.status ==
        refinement::CompiledPolicyAssertionStatus::ImproperPolicy);
    PC_CHECK(!off_policy.executable);
    PC_CHECK(!off_policy.proper);
    PC_CHECK(!off_policy.zero_off_policy);
    PC_CHECK(off_policy.off_policy_probability > 0.0);
    PC_CHECK(
        off_policy.failure_classification ==
        "route_coverage_failure");

    refinement::CompiledPolicyAssertion prepared;
    prepared.solver_cost = 10.0;
    prepared.strategy_json = "{\"graph\":\"same\"}";
    prepared.retained_solver_bytes = 17;
    prepared.evaluator_memory_budget = 23;
    prepared.compilation_ns = 29;
    std::optional<refinement::CompiledPolicyAssertion> cached =
        evaluated(11.0, 10.0, 1.0, 0.0);
    cached->strategy_json = "{\"graph\":\"product\"}";
    cached->certification_strategy_json =
        "{\"graph\":\"same\"}";
    cached->paired_default_only = true;
    PC_CHECK(refinement::reuse_compiled_policy_assertion_evaluation(
        prepared, cached));
    PC_CHECK(!cached.has_value());
    PC_CHECK(prepared.solver_cost == 10.0);
    PC_CHECK(prepared.exact_cost == 10.0);
    PC_CHECK(prepared.cost_reconciled);
    PC_CHECK(
        prepared.status ==
        refinement::CompiledPolicyAssertionStatus::Complete);
    PC_CHECK(prepared.strategy_json == "{\"graph\":\"product\"}");
    PC_CHECK(
        prepared.certification_strategy_json ==
        "{\"graph\":\"same\"}");
    PC_CHECK(prepared.retained_solver_bytes == 17);
    PC_CHECK(prepared.evaluator_memory_budget == 23);
    PC_CHECK(prepared.compilation_ns == 29);
    PC_CHECK(prepared.exact_evaluation_ns == 0);

    refinement::CompiledPolicyAssertion different;
    different.solver_cost = 10.0;
    different.strategy_json = "{\"graph\":\"different\"}";
    cached = prepared;
    PC_CHECK(!refinement::reuse_compiled_policy_assertion_evaluation(
        different, cached));
    PC_CHECK(cached.has_value());
    PC_CHECK(different.strategy_json ==
             "{\"graph\":\"different\"}");

    refinement::CompiledPolicyAssertion unpaired;
    unpaired.solver_cost = 10.0;
    unpaired.strategy_json = "{\"graph\":\"same\"}";
    cached = prepared;
    cached->paired_default_only = false;
    PC_CHECK(!refinement::reuse_compiled_policy_assertion_evaluation(
        unpaired, cached));
    PC_CHECK(cached.has_value());
    PC_CHECK(unpaired.strategy_json ==
             "{\"graph\":\"same\"}");

    refinement::CompiledPolicyAssertion observation_cap;
    observation_cap.status =
        refinement::CompiledPolicyAssertionStatus::ResourceCap;
    observation_cap.resource_cap = "max_solver_owned_bytes";
    observation_cap.strategy_json = "{}";
    observation_cap.failure_reason =
        "probe=observation_fixed_point_dense_nodes";
    PC_CHECK(
        refinement::compiled_policy_failure_classification(
            observation_cap) ==
        "exact_eval_observation_memory_cap");

    refinement::CompiledPolicyAssertion pair_cap = observation_cap;
    pair_cap.failure_reason =
        "pairs=8395474, path=pre_component, "
        "observation_requirements=594480";
    PC_CHECK(
        refinement::compiled_policy_failure_classification(pair_cap) ==
        "exact_eval_pair_discovery_memory_cap");

    refinement::CompiledPolicyAssertion refinement_memory_cap = pair_cap;
    refinement_memory_cap.evaluation.boundary_subphase =
        StrategyEvalSubphase::PairRefinement;
    PC_CHECK(
        refinement::compiled_policy_failure_classification(
            refinement_memory_cap) ==
        "exact_eval_pair_refinement_memory_cap");

    refinement::CompiledPolicyAssertion transition_cap = pair_cap;
    transition_cap.resource_cap = "max_transitions";
    transition_cap.failure_reason =
        "strategy evaluation exceeded max_transitions (10000000)";
    PC_CHECK(
        refinement::compiled_policy_failure_classification(
            transition_cap) ==
        "exact_eval_pair_discovery_transition_cap");

    refinement::CompiledPolicyAssertion class_cap = pair_cap;
    class_cap.resource_cap = "max_pairs";
    class_cap.failure_reason =
        "strategy evaluation exceeded max_pairs (1215000)";
    PC_CHECK(
        refinement::compiled_policy_failure_classification(class_cap) ==
        "exact_eval_pair_refinement_class_cap");
}

void run_shared_sparse_policy_kernel_tests() {
    SolveTransitionCache graph;
    std::vector<PricedSparseRow> priced;
    solve_detail::SparsePolicyRowInput first_row;
    first_row.owner_state = 0;
    first_row.cost = 10.0;
    first_row.transitions = {{0, 0.5}, {1, 0.5}};
    PC_CHECK(
        solve_detail::append_sparse_policy_row(
            graph, priced, first_row) == 0);
    solve_detail::SparsePolicyRowInput tied_row;
    tied_row.owner_state = 0;
    tied_row.cost = 24.0;
    PC_CHECK(
        solve_detail::append_sparse_policy_row(
            graph, priced, tied_row) == 1);
    PC_CHECK(graph.state_rows.size() == 1);
    PC_CHECK(graph.state_rows[0].count == 2);
    PC_CHECK(graph.rows[0].next_owner_row == 1);
    PC_CHECK(graph.rows[0].self_probability == 0.5);
    const std::vector<double> values{0.0, 4.0, 3.0005};

    std::uint32_t transition_work = 0;
    const double row_q = solve_detail::evaluate_sparse_policy_row(
        graph, priced, values, 0, transition_work);
    PC_CHECK(std::abs(row_q - 24.0) <= 1e-12);
    PC_CHECK(transition_work == 1);
    struct AccessorValues {
        const std::vector<double>* values = nullptr;
        std::uint32_t terminal = kNoId;
    };
    const auto accessor = [](const void* opaque, const std::uint32_t state) {
        const AccessorValues& source =
            *static_cast<const AccessorValues*>(opaque);
        if (state == source.terminal) return 0.0;
        return state < source.values->size()
            ? source.values->at(state)
            : kInfinity;
    };
    const AccessorValues direct_values{&values, kNoId};
    std::uint32_t accessor_work = 0;
    const double accessor_q =
        solve_detail::evaluate_sparse_policy_row_with_accessor(
            graph, priced, 0, accessor_work,
            accessor, &direct_values);
    PC_CHECK(accessor_q == row_q);
    PC_CHECK(accessor_work == transition_work);
    std::vector<double> non_finite_values = values;
    non_finite_values[1] = std::numeric_limits<double>::quiet_NaN();
    std::uint32_t non_finite_work = 0;
    PC_CHECK(
        solve_detail::evaluate_sparse_policy_row(
            graph, priced, non_finite_values, 0,
            non_finite_work) == kInfinity);

    SolveTransitionCache staged_graph;
    std::vector<PricedSparseRow> staged_priced;
    solve_detail::SparsePolicyRowInput staged_row;
    staged_row.owner_state = 0;
    staged_row.cost = 2.0;
    staged_row.transitions = {{0, 0.25}, {2, 0.75}};
    solve_detail::append_sparse_policy_row(
        staged_graph, staged_priced, staged_row);
    const std::vector<double> partial_values{0.0, kInfinity};
    const AccessorValues staged_values{&partial_values, 2};
    std::uint32_t staged_work = 0;
    PC_CHECK(
        solve_detail::evaluate_sparse_policy_row_with_accessor(
            staged_graph, staged_priced, 0, staged_work,
            accessor, &staged_values) ==
        2.0 / 0.75);
    PC_CHECK(staged_work == 1);

    const auto tied = solve_detail::select_sparse_policy_row(
        graph, 0,
        [](const std::uint64_t) { return true; },
        [](const std::uint64_t row, std::uint32_t& work) {
            work = 1;
            return row == 0 ? 24.0 : std::nextafter(24.0, 0.0);
        });
    PC_CHECK(tied.row == 1);
    PC_CHECK(tied.evaluated_rows == 2);
    PC_CHECK(tied.transition_work == 2);
    const auto exactly_equal = solve_detail::select_sparse_policy_row(
        graph, 0,
        [](const std::uint64_t) { return true; },
        [](const std::uint64_t, std::uint32_t& work) {
            work = 0;
            return 24.0;
        });
    PC_CHECK(exactly_equal.row == 0);
    const auto non_finite = solve_detail::select_sparse_policy_row(
        graph, 0,
        [](const std::uint64_t) { return true; },
        [](const std::uint64_t row, std::uint32_t& work) {
            work = 0;
            return row == 0
                ? std::numeric_limits<double>::quiet_NaN()
                : kInfinity;
        });
    PC_CHECK(
        non_finite.row ==
        std::numeric_limits<std::uint64_t>::max());
    const auto improving = solve_detail::select_sparse_policy_row(
        graph, 0,
        [](const std::uint64_t) { return true; },
        [](const std::uint64_t row, std::uint32_t& work) {
            work = 0;
            return row == 0 ? 24.0 : 23.9;
        });
    PC_CHECK(improving.row == 1);
    PC_CHECK(
        !solve_detail::sparse_policy_value_improvement_exceeds_tolerance(
            std::nextafter(24.0, 0.0), 24.0, 1e-3));
    PC_CHECK(
        solve_detail::sparse_policy_value_improvement_exceeds_tolerance(
            23.9, 24.0, 1e-3));
    PC_CHECK(
        solve_detail::sparse_policy_replacement_decision(
            std::nextafter(24.0, 0.0), 1,
            24.0, 0, 1e-3) ==
        solve_detail::SparsePolicyReplacementDecision::
            SuppressedStrictImprovement);
    PC_CHECK(
        solve_detail::sparse_policy_replacement_decision(
            24.0, 0, 24.0, 1, 1e-3) ==
        solve_detail::SparsePolicyReplacementDecision::Replace);
    PC_CHECK(
        solve_detail::sparse_policy_replacement_decision(
            23.9, 1, 24.0, 0, 1e-3) ==
        solve_detail::SparsePolicyReplacementDecision::Replace);

    graph.choice_successors = {2, 1};
    SparseChoiceGroup choice;
    choice.successor_offset = 0;
    choice.successor_count = 2;
    const std::vector<double> choice_values{0.0, 3.0, 3.0005};
    const std::uint32_t selected =
        solve_detail::select_sparse_policy_choice_successor(
            graph, choice, 0, choice_values);
    PC_CHECK(selected == 1);
    graph.choice_successors = {1, 2};
    const std::vector<double> near_choice_values{
        0.0, 3.0 + 5e-10, 3.0};
    PC_CHECK(
        solve_detail::select_sparse_policy_choice_successor(
            graph, choice, 0, near_choice_values) == 2);
    graph.choice_successors = {2, 1};
    const std::vector<double> infinite_choice_values{
        0.0, kInfinity, kInfinity};
    PC_CHECK(
        solve_detail::select_sparse_policy_choice_successor(
            graph, choice, 0, infinite_choice_values) ==
        1);
    SparseChoiceGroup empty_choice;
    PC_CHECK(
        solve_detail::select_sparse_policy_choice_successor(
            graph, empty_choice, 0, infinite_choice_values) ==
        kNoId);

    std::vector<PolicyRow> policy_rows(3);
    std::vector<PolicyEdge> policy_edges{
        {1, 1.0}, {0, 1.0}};
    policy_rows[0].edge_offset = 0;
    policy_rows[0].edge_count = 1;
    policy_rows[1].edge_offset = 1;
    policy_rows[1].edge_count = 1;
    const std::vector<std::uint32_t> active_states{0, 1};
    const std::vector<std::uint8_t> active{1, 1, 0};
    const std::vector<std::uint8_t> terminal{0, 0, 0};
    const std::vector<std::uint64_t> selected_rows{
        0, 1, std::numeric_limits<std::uint64_t>::max()};
    const std::vector<std::uint32_t> representatives{0, 1, 2};
    solve_detail::SparsePolicyComponentWorkspace components;
    const solve_detail::SparsePolicyTarjanView tarjan{
        active_states,
        active,
        terminal,
        selected_rows,
        representatives,
        policy_rows,
        policy_edges};
    for (std::uint32_t step = 0;
         step < 32 && !components.components_ready; ++step) {
        (void)solve_detail::advance_sparse_policy_components(
            tarjan, components, 1);
    }
    PC_CHECK(components.components_ready);
    PC_CHECK(components.components.size() == 1);
    PC_CHECK(components.components.front() ==
             std::vector<std::uint32_t>({0, 1}));

    policy_edges[0].probability = 0.5;
    policy_edges[1].probability = 0.25;
    components.local = {0, 1, -1};
    const std::vector<double> rhs{1.0, 2.0};
    const std::vector<double> previous{0.0, 0.0, 0.0};
    std::unique_ptr<solve_detail::SparsePolicyResume> resume;
    const solve_detail::SparsePolicyComponentView component{
        components.components.front(),
        0,
        components.component_by_state,
        components.local,
        policy_rows,
        policy_edges,
        rhs,
        previous,
        100000};
    const solve_detail::SparsePolicyComponentResult solved =
        solve_detail::advance_sparse_policy_component(
            component, resume);
    PC_CHECK(
        solved.status ==
        solve_detail::SparsePolicyComponentStatus::Complete);
    PC_CHECK(solved.values.size() == 2);
    PC_CHECK(std::abs(solved.values[0] - 16.0 / 7.0) <= 1e-12);
    PC_CHECK(std::abs(solved.values[1] - 18.0 / 7.0) <= 1e-12);

    /* The smallest representable exit below one is still a genuine finite
     * stored-model probability. It must neither be rounded into an infinite
     * row-local fixed point nor rejected by the dense component solve. A small
     * action cost keeps the finite value below the solver product ceiling. */
    const double rare_self_probability =
        std::nextafter(1.0, 0.0);
    const double rare_exit_probability =
        1.0 - rare_self_probability;
    constexpr double kRareStepCost = 1e-6;
    const double rare_expected_value =
        kRareStepCost / rare_exit_probability;
    PC_CHECK(std::isfinite(rare_expected_value));
    PC_CHECK(rare_expected_value < kValueCeiling);
    PC_CHECK(
        solve_detail::sparse_policy_exit_probability(
            solve_detail::WideFloat{rare_self_probability}) ==
        rare_exit_probability);

    SolveTransitionCache rare_graph;
    std::vector<PricedSparseRow> rare_priced;
    solve_detail::SparsePolicyRowInput rare_row;
    rare_row.owner_state = 0;
    rare_row.cost = kRareStepCost;
    rare_row.transitions = {
        {0, rare_self_probability},
        {1, rare_exit_probability}};
    solve_detail::append_sparse_policy_row(
        rare_graph, rare_priced, rare_row);
    std::uint32_t rare_transition_work = 0;
    const double rare_row_value =
        solve_detail::evaluate_sparse_policy_row(
            rare_graph, rare_priced, {0.0, 0.0}, 0,
            rare_transition_work);
    PC_CHECK(std::isfinite(rare_row_value));
    PC_CHECK(
        std::abs(rare_row_value - rare_expected_value) <=
        rare_expected_value * 1e-15);

    const std::vector<std::uint32_t> rare_members{0};
    const std::vector<std::uint32_t> rare_component_by_state{0};
    const std::vector<std::int32_t> rare_local_by_state{0};
    std::vector<PolicyRow> rare_policy_rows(1);
    rare_policy_rows[0].edge_offset = 0;
    rare_policy_rows[0].edge_count = 1;
    const std::vector<PolicyEdge> rare_policy_edges{
        {0, rare_self_probability}};
    const std::vector<double> rare_rhs{kRareStepCost};
    const std::vector<double> rare_previous{0.0};
    std::unique_ptr<solve_detail::SparsePolicyResume> rare_resume;
    const solve_detail::SparsePolicyComponentResult rare_solved =
        solve_detail::advance_sparse_policy_component(
            solve_detail::SparsePolicyComponentView{
                rare_members,
                0,
                rare_component_by_state,
                rare_local_by_state,
                rare_policy_rows,
                rare_policy_edges,
                rare_rhs,
                rare_previous,
                100000},
            rare_resume);
    PC_CHECK(
        rare_solved.status ==
        solve_detail::SparsePolicyComponentStatus::Complete);
    PC_CHECK(rare_solved.values.size() == 1);
    PC_CHECK(std::isfinite(rare_solved.values.front()));
    PC_CHECK(
        std::abs(rare_solved.values.front() - rare_expected_value) <=
        rare_expected_value * 1e-15);

    constexpr std::size_t kSparseRingSize =
        kDensePolicyComponentLimit + 1;
    constexpr double kAbsorptionProbability = 1.0 / 128.0;
    constexpr double kRingProbability =
        1.0 - kAbsorptionProbability;
    constexpr double kRingStepCost = 0.5;
    constexpr double kExpectedRingValue =
        kRingStepCost / kAbsorptionProbability;
    std::vector<std::uint32_t> ring_members(kSparseRingSize);
    std::vector<std::uint32_t> ring_component_by_state(
        kSparseRingSize, 0);
    std::vector<std::int32_t> ring_local_by_state(kSparseRingSize);
    std::vector<PolicyRow> ring_rows(kSparseRingSize);
    std::vector<PolicyEdge> ring_edges;
    ring_edges.reserve(kSparseRingSize);
    std::vector<double> ring_rhs(kSparseRingSize, kRingStepCost);
    std::vector<double> ring_previous(kSparseRingSize);
    for (std::size_t index = 0; index < kSparseRingSize; ++index) {
        ring_members[index] = static_cast<std::uint32_t>(index);
        ring_local_by_state[index] =
            static_cast<std::int32_t>(index);
        ring_rows[index].edge_offset =
            static_cast<std::uint32_t>(ring_edges.size());
        ring_rows[index].edge_count = 1;
        ring_edges.push_back(PolicyEdge{
            static_cast<std::uint32_t>(
                (index + 1) % kSparseRingSize),
            kRingProbability});
        const double perturbation =
            static_cast<double>((index * 37) % kSparseRingSize) -
            static_cast<double>(kSparseRingSize / 2);
        ring_previous[index] =
            kExpectedRingValue + perturbation * 0.5;
    }
    const solve_detail::SparsePolicyComponentView ring_component{
        ring_members,
        0,
        ring_component_by_state,
        ring_local_by_state,
        ring_rows,
        ring_edges,
        ring_rhs,
        ring_previous,
        100000};
    std::unique_ptr<solve_detail::SparsePolicyResume> ring_resume;
    solve_detail::SparsePolicyComponentResult ring_solved;
    std::uint32_t ring_work_units = 0;
    std::uint32_t ring_incomplete_work_units = 0;
    do {
        ring_solved = solve_detail::advance_sparse_policy_component(
            ring_component, ring_resume);
        ++ring_work_units;
        PC_CHECK(ring_solved.iterations <= 4);
        if (ring_solved.status ==
            solve_detail::SparsePolicyComponentStatus::Incomplete) {
            ++ring_incomplete_work_units;
            PC_CHECK(ring_resume != nullptr);
            PC_CHECK(ring_solved.values.empty());
        }
    } while (
        ring_solved.status ==
            solve_detail::SparsePolicyComponentStatus::Incomplete &&
        ring_work_units < 4096);
    PC_CHECK(ring_incomplete_work_units > 0);
    PC_CHECK(ring_work_units < 4096);
    PC_CHECK(
        ring_solved.status ==
        solve_detail::SparsePolicyComponentStatus::Complete);
    PC_CHECK(ring_resume == nullptr);
    PC_CHECK(ring_solved.values.size() == kSparseRingSize);
    PC_CHECK(ring_solved.total_iterations <= 20000);
    double max_ring_value_error = 0.0;
    double max_ring_residual = 0.0;
    for (std::size_t index = 0;
         index < ring_solved.values.size(); ++index) {
        max_ring_value_error = std::max(
            max_ring_value_error,
            std::abs(
                ring_solved.values[index] - kExpectedRingValue));
        const std::size_t successor =
            (index + 1) % ring_solved.values.size();
        max_ring_residual = std::max(
            max_ring_residual,
            std::abs(
                ring_solved.values[index] -
                kRingProbability * ring_solved.values[successor] -
                kRingStepCost));
    }
    PC_CHECK(max_ring_value_error <= 1e-9);
    PC_CHECK(max_ring_residual <= 1e-11);

    /* A closed cycle with positive cost is inconsistent: A*x=b has no
     * solution. Its constant first residual makes BiCGSTAB break down on the
     * zero denominator. The shared solver must switch to deterministic
     * Gauss-Seidel, remain resumable in four-sweep units, and exhaust the
     * caller's declared limit without claiming convergence. */
    std::vector<PolicyEdge> closed_ring_edges = ring_edges;
    for (PolicyEdge& edge : closed_ring_edges) {
        edge.probability = 1.0;
    }
    const std::vector<double> closed_ring_rhs(kSparseRingSize, 1.0);
    const std::vector<double> closed_ring_previous(kSparseRingSize, 0.0);
    const solve_detail::SparsePolicyComponentView closed_ring_component{
        ring_members,
        0,
        ring_component_by_state,
        ring_local_by_state,
        ring_rows,
        closed_ring_edges,
        closed_ring_rhs,
        closed_ring_previous,
        1000};
    std::unique_ptr<solve_detail::SparsePolicyResume>
        closed_ring_resume;
    solve_detail::SparsePolicyComponentResult closed_ring_solved;
    bool saw_gauss_seidel_fallback = false;
    std::uint32_t closed_ring_work_units = 0;
    do {
        closed_ring_solved =
            solve_detail::advance_sparse_policy_component(
                closed_ring_component, closed_ring_resume);
        ++closed_ring_work_units;
        PC_CHECK(closed_ring_solved.iterations <= 4);
        if (closed_ring_resume != nullptr) {
            saw_gauss_seidel_fallback =
                saw_gauss_seidel_fallback ||
                closed_ring_resume->mode ==
                    solve_detail::SparsePolicySolveMode::GaussSeidel;
        }
    } while (
        closed_ring_solved.status ==
            solve_detail::SparsePolicyComponentStatus::Incomplete &&
        closed_ring_work_units < 300);
    PC_CHECK(saw_gauss_seidel_fallback);
    PC_CHECK(closed_ring_work_units == 250);
    PC_CHECK(
        closed_ring_solved.status ==
        solve_detail::SparsePolicyComponentStatus::DidNotConverge);
    PC_CHECK(closed_ring_solved.total_iterations == 1000);
    PC_CHECK(closed_ring_solved.values.empty());
    PC_CHECK(closed_ring_resume == nullptr);

    PC_CHECK(
        solve_detail::sparse_policy_component_scratch_bytes(
            2, false) ==
        8 * sizeof(solve_detail::WideFloat) +
            2 * sizeof(double));
    constexpr std::size_t kSparseScratchOrder =
        kDensePolicyComponentLimit + 1;
    const std::uint64_t sparse_broad_scratch_expected =
        kSparseScratchOrder *
            (16 * sizeof(solve_detail::WideFloat) +
             sizeof(double) + sizeof(std::uint32_t)) +
        sizeof(solve_detail::SparsePolicyResume);
    PC_CHECK(
        solve_detail::sparse_policy_component_scratch_bytes(
            kSparseScratchOrder, false) ==
        sparse_broad_scratch_expected);
    const std::uint64_t sparse_retained_result_scratch_expected =
        sparse_broad_scratch_expected +
        kSparseScratchOrder * sizeof(double);
    PC_CHECK(
        solve_detail::sparse_policy_component_scratch_bytes(
            kSparseScratchOrder, true) ==
        sparse_retained_result_scratch_expected);
    if constexpr (sizeof(std::size_t) >= sizeof(std::uint64_t)) {
        PC_CHECK(
            solve_detail::sparse_policy_component_scratch_bytes(
                std::numeric_limits<std::size_t>::max(), false) ==
            std::numeric_limits<std::uint64_t>::max());
        PC_CHECK(
            solve_detail::sparse_policy_component_scratch_bytes(
                std::numeric_limits<std::size_t>::max(), true) ==
            std::numeric_limits<std::uint64_t>::max());
    }
}

/* Same eight-mod weighted universe as test_solver_calc.cpp. */
std::shared_ptr<SessionImpl> make_solve_session(
    const std::vector<std::string>& essence_keys) {
    auto data = std::make_shared<DataImpl>();
    data->mod_global_ids = {0, 1, 2, 3, 4, 5, 6, 7};
    data->spawn_offsets = {0, 1, 2, 3, 4, 5, 6, 7, 8};
    data->spawn_tag_ids.assign(8, 0);
    data->spawn_weights =
        {100, 100, 100, 100, 100, 100, 100, 400};
    data->mod_gen_type_code.assign(8, 0);
    data->gen_searing_implicit_code = 2;
    data->gen_eater_implicit_code = 3;
    data->mod_gen_type_code[0] =
        data->gen_searing_implicit_code;
    data->mod_gen_type_code[5] =
        data->gen_eater_implicit_code;
    data->strings = {
        "", "synthetic/base", "mod0", "mod1", "mod2",
        "mod3", "mod4", "mod5", "mod6", "mod7"};
    data->base_count = 1;
    data->base_metadata_path_sid = {1};
    data->mod_key_sid = {2, 3, 4, 5, 6, 7, 8, 9};
    data->essence_count =
        static_cast<std::uint32_t>(essence_keys.size());
    data->essence_item_level_restrictions.assign(
        essence_keys.size(), -1);
    for (std::uint32_t index = 0;
         index < essence_keys.size(); ++index) {
        const std::uint32_t sid =
            static_cast<std::uint32_t>(data->strings.size());
        data->strings.push_back(essence_keys[index]);
        data->essence_key_sids.push_back(sid);
        data->essence_by_key.emplace(essence_keys[index], index);
    }
    for (std::uint32_t mod = 0; mod < 8; ++mod) {
        data->mod_pos_by_key.emplace(
            "mod" + std::to_string(mod), mod);
    }

    auto session = std::make_shared<SessionImpl>();
    session->data = data;
    session->base_index = 0;
    session->item_level = 1;
    session->mod_count = 8;
    session->words = pc_bitset_words(8);
    session->global_index = {0, 1, 2, 3, 4, 5, 6, 7};
    for (std::uint32_t mod = 0; mod < 8; ++mod) {
        session->session_id_by_global_id.emplace(mod, mod);
    }
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
    session->effective_base_tag_ids = {0};

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

std::uint32_t satisfied_goal_count(
    const AbstractState& state,
    const GoalSpec& goal) {
    std::uint32_t count = 0;
    for (std::size_t slot = 0; slot < goal.slots.size(); ++slot) {
        count += state.slot_status[slot] ==
                 static_cast<std::uint8_t>(
                     GoalSlotStatus::Satisfied);
    }
    return count;
}

std::vector<std::pair<std::size_t, std::uint64_t>>
distribution_state_probability_bits(
    const CalcContext& calc,
    const OutcomeDistribution& distribution) {
    std::vector<std::pair<std::size_t, std::uint64_t>> result;
    result.reserve(distribution.entries.size());
    for (const OutcomeEntry& entry : distribution.entries) {
        result.push_back({
            abstract_state_hash(calc.state(entry.state)),
            std::bit_cast<std::uint64_t>(entry.probability)});
    }
    std::sort(result.begin(), result.end());
    return result;
}

SolveResult solve_stepped(
    CalcContext& calc,
    const pc_item_state& start,
    const std::unordered_map<std::string, double>& prices,
    const std::uint32_t budget) {
    SolveWork work(calc, start, prices);
    std::uint32_t progress_events = 0;
    const auto checked_progress = [&]() {
        const std::uint64_t ledger_requests_before =
            calc.telemetry().owned_byte_ledger_requests;
        const SolveProgress progress = work.progress();
        PC_CHECK(calc.telemetry().owned_byte_ledger_requests >
                 ledger_requests_before);
        PC_CHECK(progress.live_owned_bytes >= work.live_owned_bytes());
        return progress;
    };
    SolveProgress progress = checked_progress();
    while (!progress.done) {
        work.step(budget);
        ++progress_events;
        progress = checked_progress();
    }
    PC_CHECK(progress_events >= 2);
    return work.finish();
}

bool identical_solve(
    const SolveResult& left,
    const SolveResult& right) {
    return left.converged == right.converged &&
           left.policy_available == right.policy_available &&
           left.policy_status == right.policy_status &&
           left.termination == right.termination &&
           left.lower_bound == right.lower_bound &&
           left.upper_bound == right.upper_bound &&
           left.evaluated_policy_cost == right.evaluated_policy_cost &&
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
        PC_CHECK(result.policy_available);
        PC_CHECK(result.policy_status == SolvePolicyStatus::Exact);
        PC_CHECK(result.termination == SolveTermination::ExactClosed);
        PC_CHECK(result.lower_bound == result.upper_bound);
        PC_CHECK(result.upper_bound == result.evaluated_policy_cost);
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

        SolveResult refinement_sample = subset;
        PolicyRefinementTelemetry& refinement =
            refinement_sample.diagnostics.policy_refinement;
        refinement.triggers = 1;
        refinement.status = "complete";
        refinement.core_policy_candidate_present = true;
        refinement.core_policy_status = "exact";
        refinement.core_policy_lower_bound = 29.0;
        refinement.core_policy_upper_bound = 30.0;
        refinement.core_policy_evaluated_cost = 30.0;
        refinement.core_policy_transition_bits_hash = 31;
        refinement.core_policy_bits_hash = 32;
        refinement.core_policy_selected_states = 33;
        refinement.core_policy_distinct_actions = 2;
        refinement.core_policy_root_action = "regal";
        refinement.core_policy_goal_identity = 34;
        refinement.core_policy_economy_identity = 35;
        refinement.core_policy_action_vocabulary_identity = 36;
        refinement.core_policy_graph_identity = 37;
        refinement.core_policy_artifact_identity = 38;
        refinement.core_policy_owned_bytes = 39;
        refinement.direct_certification_status = "cost_mismatch";
        refinement.direct_certification_failure_reason = "cost mismatch";
        refinement.direct_certification_solver_cost = 30.0;
        refinement.direct_certification_exact_cost = 31.0;
        refinement.direct_certification_offpolicy_probability = 0.0;
        refinement.direct_certification_artifact_bytes = 40;
        refinement.direct_certification_peak_owned_bytes = 41;
        refinement.direct_certification_evaluation_stages.model_setup_ns = 42;
        refinement.direct_certification_evaluation_stages
            .observation_preparation_ns = 43;
        refinement.direct_certification_evaluation_stages.pair_discovery_ns =
            44;
        refinement.direct_certification_evaluation_stages.pair_interning_ns =
            45;
        refinement.direct_certification_evaluation_stages.exact_kernel_ns =
            46;
        refinement.direct_certification_evaluation_stages
            .pair_refinement_ns = 47;
        refinement.direct_certification_evaluation_stages
            .pair_partition_ns = 55;
        refinement.direct_certification_evaluation_stages
            .pair_quotient_conversion_ns = 56;
        refinement.direct_certification_evaluation_stages
            .component_construction_ns = 48;
        refinement.direct_certification_evaluation_stages.component_solve_ns =
            49;
        refinement.direct_certification_evaluation_stages.finalization_ns = 50;
        refinement.direct_certification_evaluation_max_work_item_ns = 57;
        refinement.direct_certification_evaluation_max_work_item_subphase =
            StrategyEvalSubphase::PairDiscovery;
        refinement.direct_certification_evaluation_boundary =
            StrategyEvalSubphase::PairRefinement;
        refinement.direct_certification_raw_pairs = 51;
        refinement.direct_certification_refined_pairs = 52;
        refinement.direct_certification_refined_pair_limit = 53;
        refinement
            .direct_certification_pair_discovery_index_peak_bytes = 54;
        refinement.direct_certification_executable = true;
        refinement.direct_certification_proper = true;
        refinement.direct_certification_cost_complete = true;
        refinement.direct_certification_zero_off_policy = true;
        refinement.direct_candidate_retained = true;
        refinement.strict_lift_status = "resource_cap";
        refinement.strict_lift_failure_reason = "strict cap";
        refinement.strict_lift_resource_cap = "max_exact_states";
        refinement.publication_status = "bounded_core_policy";
        refinement.published_candidate_kind =
            "direct_compiled_core_policy";
        refinement.policy_reachable_coarse_states = 2;
        refinement.exact_states = 3;
        refinement.retained_exact_states = 4;
        refinement.exact_classes = 5;
        refinement.initial_observation_classes = 6;
        refinement.behavior_splits = 7;
        refinement.merged_exact_states = 8;
        refinement.exact_transitions = 9;
        refinement.exact_kernels = 10;
        refinement.exact_kernel_cache_hits = 11;
        refinement.memory_bytes = 12;
        refinement.peak_memory_bytes = 13;
        refinement.memory_limit_bytes = 14;
        refinement.retained_artifact_bytes = 15;
        refinement.fallback_portfolio_candidates = 2;
        refinement.fallback_portfolio_invalidations = 3;
        refinement.fallback_portfolio_compilation_failures = 4;
        refinement.fallback_portfolio_memory_rejections = 5;
        refinement.fallback_portfolio_owned_bytes = 6;
        refinement.fallback_publication_attempts = 7;
        refinement.fallback_publication_successes = 1;
        refinement.cheapest_independently_evaluated_selected = true;
        refinement.preferred_candidate_upper = 30.0;
        refinement.published_fallback_upper = 40.0;
        refinement.published_fallback_evaluated_cost = 40.0;
        refinement.published_fallback_witness_hash = 31;
        refinement.published_fallback_kind = "renewal";
        refinement.preferred_publication_failure_reason = "lift cap";
        refinement.exact_state_reuses = 16;
        refinement.collapse_events = 17;
        refinement.collapse_destroyed_feature_mask = 1;
        refinement.collapse_preserved_feature_mask = 2;
        refinement.collapse_events_by_feature[0] = 2;
        refinement.preservation_events_by_feature[0] = 3;
        refinement.refinement_rounds = 18;
        refinement.backward_observation_rounds = 1;
        refinement.selected_action_routing_rounds = 2;
        refinement.observation_propagation_rounds = 3;
        refinement.partition_refinement_rounds = 4;
        refinement.local_reoptimization_rounds = 5;
        refinement.local_state_action_rows_scheduled = 24;
        refinement.local_state_action_rows_evaluated = 23;
        refinement.local_reoptimizations = 6;
        refinement.local_policy_changes = 22;
        refinement.local_value_changes = 21;
        refinement.lumpability_checks = 7;
        refinement.fixed_point_checked = true;
        refinement.fixed_point_complete = true;
        refinement.lumpability_checked = true;
        refinement.lumpable = true;
        refinement.class_policy_checked = true;
        refinement.class_policy_proper = true;
        refinement.compiled_assertion_checked = true;
        refinement.compiled_policy_proper = true;
        refinement.zero_off_policy = true;
        refinement.cost_reconciled = true;
        refinement.policy_changed = true;
        refinement.coarse_value_reconciled = true;
        refinement.counterexamples = 19;
        refinement.counterexample_samples = {
            "{\"kind\":\"observation\"}"};
        refinement.counterexample_samples_omitted = 20;
        refinement.refusal_causes = 21;
        refinement.refusal_cause_samples = {
            "max_exact_states"};
        refinement.refusal_cause_samples_omitted = 22;
        refinement.publication_candidate_samples = {
            "{\"identity\":1,\"disposition\":\"selected_for_publication\"}"};
        refinement.publication_candidate_samples_omitted = 1;
        refinement.publication_candidate_sample_bytes =
            refinement.publication_candidate_samples.front().size();
        refinement.structural_failure_samples = {
            "{\"status\":\"coarse_mapping_failure\"}"};
        refinement.structural_failure_samples_omitted = 2;
        refinement.structural_failure_sample_bytes =
            refinement.structural_failure_samples.front().size();
        refinement.evaluator_memory_samples = {
            "{\"stage\":\"strict_final_graph_evaluation\",\"nodes\":3}"};
        refinement.evaluator_memory_samples_omitted = 3;
        refinement.evaluator_memory_sample_bytes =
            refinement.evaluator_memory_samples.front().size();
        PolicyCompilationTelemetry compilation_sample;
        compilation_sample.working_states = 23;
        compilation_sample.behavioral_classes = 22;
        compilation_sample.policy_regions = 24;
        compilation_sample.nodes = 25;
        compilation_sample.edges = 26;
        compilation_sample.strategy_json_bytes = 27;
        compilation_sample.total_condition_bytes = 271;
        compilation_sample.max_condition_bytes = 272;
        compilation_sample.condition_edges = 274;
        compilation_sample.unique_condition_literals = 75;
        compilation_sample.repeated_condition_occurrences = 199;
        compilation_sample.repeated_condition_bytes = 278;
        compilation_sample.policy_route_nondefault_edges = 20;
        compilation_sample.policy_route_distinct_targets = 15;
        compilation_sample.same_target_branch_groups = 4;
        compilation_sample.same_target_branch_edges = 9;
        compilation_sample.projected_same_target_edge_savings = 5;
        compilation_sample.max_policy_route_out_degree = 6;
        compilation_sample.max_policy_route_distinct_targets = 5;
        compilation_sample.exact_state_fallbacks = 2;
        compilation_sample.junk_predicates = 273;
        compilation_sample.policy_route_default_edges = 5;
        compilation_sample.policy_route_restart_default_edges = 5;
        compilation_sample.policy_route_default_mode =
            "product_safe_restart";
        compilation_sample.certification_policy_route_default_edges = 5;
        compilation_sample
            .certification_policy_route_offpolicy_default_edges = 5;
        compilation_sample.certification_policy_route_default_mode =
            "certification_fail_closed";
        compilation_sample.paired_default_only = true;
        compilation_sample.peak_owned_bytes = 280;
        compilation_sample.previously_accounted_peak_owned_bytes = 28;
        compilation_sample.complete_peak_owned_bytes = 280;
        const std::string refinement_telemetry =
            serialize_solver_telemetry(
                calc, &refinement_sample, nullptr, std::nullopt,
                &compilation_sample);
        PC_CHECK(valid_json_object(refinement_telemetry));
        PC_CHECK(refinement_telemetry.find(
                     "\"policy_refinement\":{\"triggers\":1,"
                     "\"status\":\"complete\",\"resource_cap\":null,") !=
                 std::string::npos);
        PC_CHECK(refinement_telemetry.find(
                     "\"core_policy\":{\"candidate_present\":true,"
                     "\"status\":\"exact\",\"lower_bound\":29,"
                     "\"upper_bound\":30,\"evaluated_cost\":30,"
                     "\"transition_bits_hash\":\"000000000000001f\","
                     "\"policy_bits_hash\":\"0000000000000020\"") !=
                 std::string::npos);
        PC_CHECK(refinement_telemetry.find(
                     "\"direct_certification\":{"
                     "\"status\":\"cost_mismatch\","
                     "\"failure_reason\":\"cost mismatch\","
                     "\"failure_classification\":null,"
                     "\"resource_cap\":null,\"solver_cost\":30,"
                     "\"exact_cost\":31,\"offpolicy_probability\":0") !=
                 std::string::npos);
        PC_CHECK(refinement_telemetry.find(
                     "\"evaluation_stages_ns\":{"
                     "\"model_setup\":42,"
                     "\"observation_preparation\":43,"
                     "\"pair_discovery\":44,"
                     "\"pair_interning\":45,"
                     "\"exact_kernel\":46,"
                     "\"pair_refinement\":47,"
                     "\"pair_partition\":55,"
                     "\"pair_quotient_conversion\":56,"
                     "\"component_construction\":48,"
                     "\"component_solve\":49,"
                     "\"finalization\":50},"
                     "\"evaluation_max_work_item_ns\":57,"
                     "\"evaluation_max_work_item_subphase\":"
                     "\"pair_discovery\","
                     "\"evaluation_boundary\":{"
                     "\"subphase\":\"pair_refinement\","
                     "\"raw_pairs\":51,\"refined_pairs\":52,"
                     "\"refined_pair_limit\":53,"
                     "\"discovery_index_peak_bytes\":54}") !=
                 std::string::npos);
        PC_CHECK(refinement_telemetry.find(
                     "\"strict_lift\":{\"status\":\"resource_cap\","
                     "\"failure_reason\":\"strict cap\","
                     "\"resource_cap\":\"max_exact_states\"") !=
                 std::string::npos);
        PC_CHECK(refinement_telemetry.find(
                     "\"publication\":{"
                     "\"status\":\"bounded_core_policy\","
                     "\"candidate_kind\":"
                     "\"direct_compiled_core_policy\"}") !=
                 std::string::npos);
        PC_CHECK(refinement_telemetry.find(
                     "\"publication_candidates\":{"
                     "\"samples\":[{\"identity\":1,") !=
                 std::string::npos);
        PC_CHECK(refinement_telemetry.find(
                     "\"structural_failures\":{"
                     "\"samples\":[{\"status\":"
                     "\"coarse_mapping_failure\"}]") !=
                 std::string::npos);
        PC_CHECK(refinement_telemetry.find(
                     "\"evaluator_memory\":{"
                     "\"samples\":[{\"stage\":"
                     "\"strict_final_graph_evaluation\","
                     "\"nodes\":3}]") !=
                 std::string::npos);
        PC_CHECK(refinement_telemetry.find(
                     "\"policy_reachable_coarse_states\":2,"
                     "\"exact_states\":3,\"retained_exact_states\":4,"
                     "\"exact_classes\":5") !=
                 std::string::npos);
        PC_CHECK(refinement_telemetry.find(
                     "\"initial_observation_classes\":6,"
                     "\"behavior_splits\":7,\"merged_exact_states\":8,"
                     "\"exact_transitions\":9,\"exact_kernels\":10,"
                     "\"exact_kernel_cache_hits\":11") !=
                 std::string::npos);
        PC_CHECK(refinement_telemetry.find(
                     "\"memory_bytes\":12,\"peak_memory_bytes\":13,"
                     "\"memory_limit_bytes\":14,"
                     "\"retained_artifact_bytes\":15") !=
                 std::string::npos);
        PC_CHECK(refinement_telemetry.find(
                     "\"fallback_portfolio\":{\"candidates\":2,"
                     "\"invalidations\":3,\"compilation_failures\":4,"
                     "\"memory_rejections\":5,\"owned_bytes\":6,"
                     "\"publication_attempts\":7,"
                     "\"publication_successes\":1,"
                     "\"cheapest_independently_evaluated_selected\":true,"
                     "\"preferred_candidate_upper\":30,"
                     "\"published_upper\":40,"
                     "\"published_evaluated_cost\":40,"
                     "\"published_witness_hash\":\"000000000000001f\","
                     "\"published_kind\":\"renewal\","
                     "\"preferred_failure_reason\":\"lift cap\"}") !=
                 std::string::npos);
        PC_CHECK(refinement_telemetry.find(
                     "\"exact_state_reuses\":16,"
                     "\"collapse_events\":17,"
                     "\"collapse_destroyed_feature_mask\":1,"
                     "\"collapse_preserved_feature_mask\":2,"
                     "\"collapse_events_by_feature\":[2,0") !=
                 std::string::npos);
        PC_CHECK(refinement_telemetry.find(
                     "\"preservation_events_by_feature\":[3,0") !=
                 std::string::npos);
        PC_CHECK(refinement_telemetry.find(
                     "\"refinement_rounds\":18") !=
                 std::string::npos);
        PC_CHECK(refinement_telemetry.find(
                     "\"backward_observation_rounds\":1,"
                     "\"selected_action_routing_rounds\":2,"
                     "\"observation_propagation_rounds\":3,"
                     "\"partition_refinement_rounds\":4,"
                     "\"local_reoptimization_rounds\":5,"
                     "\"local_state_action_rows_scheduled\":24,"
                     "\"local_state_action_rows_evaluated\":23,"
                     "\"local_reoptimizations\":6,"
                     "\"local_policy_changes\":22,"
                     "\"local_value_changes\":21") !=
                 std::string::npos);
        PC_CHECK(refinement_telemetry.find(
                     "\"fixed_point\":{\"checked\":true,"
                     "\"complete\":true,\"lumpability_checked\":true,"
                     "\"lumpable\":true,\"lumpability_checks\":7},"
                     "\"class_policy\":{\"checked\":true,"
                     "\"proper\":true},"
                     "\"compiled_assertion\":{\"checked\":true,"
                     "\"proper\":true,\"zero_off_policy\":true,"
                     "\"cost_reconciled\":true},"
                     "\"policy_changed\":true,"
                     "\"coarse_value_reconciled\":true") !=
                 std::string::npos);
        const std::string sample_limit = std::to_string(
            refinement_sample.diagnostics.diagnostic_sample_limit);
        PC_CHECK(refinement_telemetry.find(
                     "\"counterexamples\":{\"count\":19,"
                     "\"samples\":[{\"kind\":\"observation\"}],"
                     "\"retained\":1,\"omitted\":20,\"limit\":" +
                     sample_limit + "}") !=
                 std::string::npos);
        PC_CHECK(refinement_telemetry.find(
                     "\"refusal_causes\":{\"count\":21,"
                     "\"samples\":[\"max_exact_states\"],"
                     "\"retained\":1,\"omitted\":22,\"limit\":" +
                     sample_limit + "}") !=
                 std::string::npos);
        PC_CHECK(refinement_telemetry.find(
                     "\"compilation\":{\"available\":true,"
                     "\"working_states\":23,"
                     "\"behavioral_classes\":22,"
                     "\"policy_regions\":24,"
                     "\"graph_census\":{"
                     "\"infrastructure_nodes\":0,"
                     "\"policy_route_nodes\":0,"
                     "\"local_gated_route_nodes\":0,"
                     "\"primitive_region_nodes\":0,"
                     "\"additional_recipe_nodes\":0},"
                     "\"nodes\":25,\"edges\":26,"
                     "\"strategy_json_bytes\":27,"
                     "\"total_condition_bytes\":271,"
                     "\"max_condition_bytes\":272,"
                     "\"condition_census\":{\"edges\":274,"
                     "\"unique_literals\":75,"
                     "\"repeated_occurrences\":199,"
                     "\"repeated_bytes\":278},"
                     "\"policy_route_density\":{\"nondefault_edges\":20,"
                     "\"distinct_targets\":15,"
                     "\"same_target_groups\":4,"
                     "\"same_target_edges\":9,"
                     "\"projected_edge_savings\":5,"
                     "\"max_out_degree\":6,"
                     "\"max_distinct_targets\":5},"
                     "\"exact_state_fallbacks\":2,"
                     "\"junk_predicates\":273,"
                     "\"policy_route_defaults\":{"
                     "\"mode\":\"product_safe_restart\","
                     "\"edges\":5,\"restart\":5,\"offpolicy\":0},"
                     "\"certification_policy_route_defaults\":{"
                     "\"mode\":\"certification_fail_closed\","
                     "\"edges\":5,\"offpolicy\":5},"
                     "\"paired_default_only\":true,"
                     "\"peak_owned_bytes\":280,"
                     "\"previously_accounted_peak_owned_bytes\":28,"
                     "\"complete_peak_owned_bytes\":280,"
                     "\"cap_hit\":null}") !=
                 std::string::npos);
        refinement.status = "resource_cap";
        refinement.resource_cap = "max_sweeps";
        const std::string refinement_cap_telemetry =
            serialize_solver_telemetry(
                calc, &refinement_sample, nullptr, std::nullopt,
                &compilation_sample);
        PC_CHECK(refinement_cap_telemetry.find(
                     "\"policy_refinement\":{\"triggers\":1,"
                     "\"status\":\"resource_cap\","
                     "\"resource_cap\":\"max_sweeps\"") !=
                 std::string::npos);

        SolveOptions capped_options;
        capped_options.max_states = 1;
        const SolveResult capped = solve(calc, start, prices, capped_options);
        PC_CHECK(!capped.converged);
        PC_CHECK(capped.diagnostics.state_cap_hit);
        PC_CHECK(!capped.policy_available);
        PC_CHECK(capped.policy_status == SolvePolicyStatus::None);
        PC_CHECK(capped.termination ==
                 SolveTermination::RefusedResourceCap);
        PC_CHECK(!std::isfinite(capped.upper_bound));
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

        /* Q1-Q3 white-box oracle: incremental byte accounting, exact kernel
         * reuse, and the completed outer quotient may change graph shape but
         * never the value or selected start action. */
        SolveOptions evidence_options;
        evidence_options.full_evidence = true;
        const SolveResult quotient =
            solve(calc, start, prices, evidence_options);
        SolveOptions strict_options = evidence_options;
        strict_options.strict_states = true;
        const SolveResult strict =
            solve(calc, start, prices, strict_options);
        SolveOptions no_reuse_options = evidence_options;
        no_reuse_options.kernel_reuse = false;
        const SolveResult no_reuse =
            solve(calc, start, prices, no_reuse_options);
        PC_CHECK(quotient.converged);
        PC_CHECK(strict.converged);
        PC_CHECK(no_reuse.converged);
        PC_CHECK(near(
            quotient.values[quotient.start_state],
            strict.values[strict.start_state], 1e-9));
        PC_CHECK(near(
            quotient.values[quotient.start_state],
            no_reuse.values[no_reuse.start_state], 1e-9));
        PC_CHECK(quotient.policy[quotient.start_state] ==
                 strict.policy[strict.start_state]);
        PC_CHECK(quotient.policy[quotient.start_state] ==
                 no_reuse.policy[no_reuse.start_state]);
        PC_CHECK(quotient.diagnostics.strict_discovered_states >
                 quotient.diagnostics.quotient_states);
        PC_CHECK(
            quotient.diagnostics.exact_behavioral_merges ==
            quotient.diagnostics.strict_discovered_states -
                quotient.diagnostics.quotient_states);
        PC_CHECK(!quotient.diagnostics.state_scaling_shadow_only);
        PC_CHECK(!quotient.behavioral_representative_by_state.empty());
        bool has_non_identity_representative = false;
        for (std::uint32_t state = 0;
             state < quotient.behavioral_representative_by_state.size();
             ++state) {
            has_non_identity_representative |=
                quotient.behavioral_representative_by_state[state] != state;
        }
        PC_CHECK(has_non_identity_representative);
        PC_CHECK(quotient.diagnostics.observation_signature_mismatches == 0);
        PC_CHECK(strict.behavioral_representative_by_state.empty());
        PC_CHECK(no_reuse.diagnostics.exact_kernel_payload_reuses == 0);
        PC_CHECK(quotient.diagnostics.solve_owned_byte_ledger_requests > 0);
        PC_CHECK(quotient.diagnostics.solve_owned_byte_reconciliations > 0);

        SolveOptions shadow_options = evidence_options;
        shadow_options.max_states = 1;
        const SolveResult shadow =
            solve(calc, start, prices, shadow_options);
        PC_CHECK(!shadow.converged);
        PC_CHECK(shadow.diagnostics.state_cap_hit);
        PC_CHECK(shadow.diagnostics.state_scaling_shadow_only);
        PC_CHECK(
            shadow.diagnostics.strict_discovered_states ==
            shadow.diagnostics.quotient_states);
        PC_CHECK(
            shadow.diagnostics.shadow_behavioral_classes <=
            shadow.diagnostics.strict_discovered_states);
        PC_CHECK(shadow.diagnostics.quotient_refinement_rounds == 1);
        PC_CHECK(shadow.diagnostics.exact_behavioral_merges == 0);
        PC_CHECK(shadow.behavioral_representative_by_state.empty());
        const std::string shadow_telemetry = serialize_solver_telemetry(
            calc, &shadow, nullptr, std::nullopt, nullptr);
        PC_CHECK(valid_json_object(shadow_telemetry));
        PC_CHECK(
            shadow_telemetry.find("\"shadow_only\":true") !=
            std::string::npos);
        std::printf(
            "solver quotient audit: completed=%u/%u merges=%llu "
            "shadow=%u/%u\n",
            quotient.diagnostics.strict_discovered_states,
            quotient.diagnostics.quotient_states,
            static_cast<unsigned long long>(
                quotient.diagnostics.exact_behavioral_merges),
            shadow.diagnostics.strict_discovered_states,
            shadow.diagnostics.shadow_behavioral_classes);

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
        /*
         * Runtime-contract admission precedes evaluator support filtering.
         * A future action with no semantic contract must fail while planner
         * operators are built, not after a policy selects it.
         */
        {
            ActionRegistry incomplete_registry = unsupported_registry;
            incomplete_registry.index_by_id.emplace(
                unsupported.id, unsupported_index);
            incomplete_registry.actions.push_back(unsupported);
            bool incomplete_rejected = false;
            try {
                CalcContext incomplete_calc(
                    session, goal, std::move(incomplete_registry),
                    {transmute, alteration, restart,
                     unsupported_index});
                (void)incomplete_calc;
            } catch (const std::logic_error& error) {
                incomplete_rejected =
                    std::string(error.what()).find(
                        "complete refinement contract") !=
                    std::string::npos;
            }
            PC_CHECK(incomplete_rejected);
        }
        {
            ActionRegistry malformed_registry = unsupported_registry;
            ActionDescriptor malformed = unsupported;
            malformed.refinement.schema_version =
                kActionRefinementContractVersion;
            malformed.refinement.preserved_item_features =
                kAllRefinementItemFeatures;
            /* No flow and no destruction covers occupied exact affixes. */
            malformed_registry.index_by_id.emplace(
                malformed.id, unsupported_index);
            malformed_registry.actions.push_back(
                std::move(malformed));
            bool malformed_rejected = false;
            try {
                CalcContext malformed_calc(
                    session, goal, std::move(malformed_registry),
                    {transmute, alteration, restart,
                     unsupported_index});
                (void)malformed_calc;
            } catch (const std::logic_error& error) {
                malformed_rejected =
                    std::string(error.what()).find(
                        "affix effect domain is incomplete") !=
                    std::string::npos;
            }
            PC_CHECK(malformed_rejected);
        }
        /*
         * Keep the older unsupported-evaluator coverage with an explicitly
         * complete inert semantic contract. The action remains unevaluable
         * and therefore cannot enter a selected executable policy.
         */
        unsupported.refinement.schema_version =
            kActionRefinementContractVersion;
        unsupported.refinement.preserved_item_features =
            kAllRefinementItemFeatures;
        unsupported.refinement.preserved_affixes.push_back({});
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

        SolveOptions current_carrier_only;
        current_carrier_only.allow_economic_restart = false;
        const SolveResult restricted = solve(
            calc, corrupted, prices, current_carrier_only);
        PC_CHECK(!restricted.converged);
        PC_CHECK(!restricted.policy_available);
        PC_CHECK(
            restricted.termination ==
            SolveTermination::NoExecutablePolicy);
        PC_CHECK(
            restricted.diagnostics.solution_scope ==
            "exact_within_no_economic_restart_restriction");
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

std::uint32_t reachable_nonterminal_refinement_classes(
        const refinement::PolicyExactLiftCertificate& lifted) {
    std::uint32_t result = 0;
    for (const refinement::RefinedClassValue& value :
         lifted.class_evaluation.class_values) {
        if (value.class_id < lifted.refinement.classes.size() &&
            !lifted.refinement.classes[value.class_id].terminal) {
            ++result;
        }
    }
    return result;
}

void report_lift_failure(
        const char* label,
        const refinement::PolicyExactLiftCertificate& lifted) {
    if (lifted.status ==
        refinement::PolicyExactLiftStatus::Complete) {
        return;
    }
    std::printf(
        "policy lift failure [%s]: status=%s reason=%s cap=%s "
        "refinement=%u evaluation=%u compiled=%u rows=%llu/%llu "
        "reopts=%llu policy_changes=%llu\n",
        label,
        refinement::policy_exact_lift_status_name(lifted.status),
        lifted.failure_reason.c_str(),
        lifted.resource_cap.c_str(),
        static_cast<unsigned>(lifted.refinement.status),
        static_cast<unsigned>(lifted.class_evaluation.status),
        static_cast<unsigned>(lifted.compiled.status),
        static_cast<unsigned long long>(
            lifted.adapter.local_state_action_rows_evaluated),
        static_cast<unsigned long long>(
            lifted.adapter.local_state_action_rows_scheduled),
        static_cast<unsigned long long>(
            lifted.adapter.local_reoptimizations),
        static_cast<unsigned long long>(
            lifted.adapter.local_policy_changes));
}

void report_solve_issue(
        const char* label,
        const SolveResult& solved,
        const bool require_converged = false) {
    if (solved.policy_available &&
        (!require_converged || solved.converged)) {
        return;
    }
    const std::string cap =
        solved.diagnostics.cap_hits.empty()
            ? std::string{}
            : solved.diagnostics.cap_hits.front();
    std::printf(
        "solver issue [%s]: available=%u converged=%u status=%u "
        "termination=%u publication=%s evaluation=%s "
        "compatibility=%s cap=%s states=%u/%u\n",
        label,
        solved.policy_available ? 1u : 0u,
        solved.converged ? 1u : 0u,
        static_cast<unsigned>(solved.policy_status),
        static_cast<unsigned>(solved.termination),
        solved.diagnostics.policy_publication_failure_reason.c_str(),
        solved.diagnostics.policy_evaluation_failure.c_str(),
        solved.diagnostics.policy_compatibility_reason.c_str(),
        cap.c_str(),
        solved.diagnostics.expanded_states,
        solved.diagnostics.discovered_states);
    const std::size_t detail_count = std::min<std::size_t>(
        16, solved.values.size());
    for (std::size_t state = 0; state < detail_count; ++state) {
        const PolicyOperatorRef policy =
            state < solved.policy.size()
                ? solved.policy[state]
                : PolicyOperatorRef{};
        std::printf(
            "  state=%zu value=%.17g policy=%u:%u expanded=%u "
            "goal=%u reachable=%u\n",
            state, solved.values[state],
            static_cast<unsigned>(policy.kind), policy.index,
            state < solved.expanded.size()
                ? static_cast<unsigned>(solved.expanded[state])
                : 0u,
            state < solved.goal_states.size()
                ? static_cast<unsigned>(solved.goal_states[state])
                : 0u,
            state < solved.policy_reachable.size()
                ? static_cast<unsigned>(
                      solved.policy_reachable[state])
                : 0u);
    }
}

void run_policy_guided_exact_lift_tests() {
    std::uint32_t unresolved_rounds = 0;
    PC_CHECK(!advance_unreconciled_stable_policy_latch(
        false, false, unresolved_rounds));
    PC_CHECK(unresolved_rounds == 1);
    PC_CHECK(advance_unreconciled_stable_policy_latch(
        false, false, unresolved_rounds));
    PC_CHECK(unresolved_rounds == 2);
    PC_CHECK(!advance_unreconciled_stable_policy_latch(
        true, false, unresolved_rounds));
    PC_CHECK(unresolved_rounds == 0);
    PC_CHECK(!advance_unreconciled_stable_policy_latch(
        false, true, unresolved_rounds));
    PC_CHECK(unresolved_rounds == 0);

    PC_CHECK(
        successful_refined_publication_termination(
            SolveTermination::ExactClosed, false) ==
        SolveTermination::NumericalStability);
    PC_CHECK(
        successful_refined_publication_termination(
            SolveTermination::ExactClosed, true) ==
        SolveTermination::RefusedResourceCap);
    PC_CHECK(
        successful_refined_publication_termination(
            SolveTermination::ExactClosed, false, true) ==
        SolveTermination::ExactClosed);
    PC_CHECK(
        successful_refined_publication_termination(
            SolveTermination::RefusedResourceCap, false) ==
        SolveTermination::RefusedResourceCap);
    PC_CHECK(
        successful_refined_publication_termination(
            SolveTermination::RefusedResourceCap, true, true) ==
        SolveTermination::ExactClosed);
    PC_CHECK(
        successful_refined_publication_termination(
            SolveTermination::None, true) ==
        SolveTermination::RefusedResourceCap);
    PC_CHECK(
        successful_refined_publication_termination(
            SolveTermination::TargetGap, false) ==
        SolveTermination::TargetGap);
    PC_CHECK(
        successful_refined_publication_termination(
            SolveTermination::NumericalStability, false) ==
        SolveTermination::NumericalStability);
    PC_CHECK(
        successful_refined_publication_termination(
            SolveTermination::NumericalStability, false, true) ==
        SolveTermination::ExactClosed);
    PC_CHECK(
        successful_refined_publication_termination(
            SolveTermination::NoExecutablePolicy, false, false, true) ==
        SolveTermination::ExactClosed);
    bool rejected_unclosed_synthesis = false;
    try {
        (void)successful_refined_publication_termination(
            SolveTermination::NoExecutablePolicy, false);
    } catch (const std::logic_error&) {
        rejected_unclosed_synthesis = true;
    }
    PC_CHECK(rejected_unclosed_synthesis);

    auto session = make_solve_session();
    ActionRegistry registry = build_action_registry(*session);

    GoalSpec goal;
    goal.rarity = PC_RARITY_RARE;
    GoalSlot slot;
    slot.family_id = 104; /* suffix mod 5 */
    slot.min_tier = 1;
    goal.slots.push_back(slot);

    const std::uint32_t alchemy =
        registry.index_by_id.at("alchemy");
    const std::uint32_t chaos =
        registry.index_by_id.at("chaos");
    const std::uint32_t regal =
        registry.index_by_id.at("regal");
    const std::uint32_t restart =
        registry.index_by_id.at("restart");
    const std::vector<std::uint32_t> candidates{
        alchemy, chaos, regal, restart};
    CalcContext calc(
        session, goal, registry, candidates,
        false, true, false, std::nullopt, {}, true);
    PC_CHECK(calc.product_solver_parent());

    /*
     * The coarse parent merges ordinary prefix mods 0..4 even though their
     * group/exclusion effects differ. Regal observes that identity. Starting
     * with the deterministic first member makes the old representative
     * kernel numerically sound for this root, while the policy still requires
     * a genuine strict lift before publication. A failed Regal attempt enters
     * the destructive Chaos renewal, which also exercises identity collapse.
     */
    pc_item_state start;
    pc_item_clear(&start);
    start.rarity = PC_RARITY_MAGIC;
    place(&start, PC_SIDE_PREFIX, 0, 10);
    const std::unordered_map<std::string, double> prices{
        {"alchemy", 1.0},
        {"chaos", 1.0},
        {"regal", 0.01},
        {"base", 10.0}};
    SolveOptions options;
    {
        CalcContext progress_calc(
            session, goal, registry, candidates,
            false, true, false, std::nullopt, {}, true);
        SolveWorkTestAccess::Impl progress_work(
            progress_calc, start, prices, options);
        progress_work.result.diagnostics.focused_expansion = true;
        progress_work.result.diagnostics.focused_upper_bound = 100.0;
        progress_work.finalization_verified_upper_bound = 12.5;
        SolveProgress live_progress = progress_work.progress();
        PC_CHECK(near(live_progress.upper_bound, 12.5, 1e-12));
        PC_CHECK(near(
            live_progress.absolute_optimality_gap,
            live_progress.upper_bound - live_progress.lower_bound,
            1e-12));
        progress_work.result.diagnostics.focused_upper_bound = 10.0;
        live_progress = progress_work.progress();
        PC_CHECK(near(live_progress.upper_bound, 10.0, 1e-12));
    }
    const SolveResult solved =
        solve(calc, start, prices, options);
    {
        auto fallback_session = make_solve_session({"fallback_goal"});
        fallback_session->essence_guaranteed_mod_ids = {0};
        ActionRegistry fallback_registry =
            build_action_registry(*fallback_session);
        GoalSpec fallback_goal;
        fallback_goal.rarity = PC_RARITY_RARE;
        for (const std::uint32_t family : {100u, 104u}) {
            GoalSlot fallback_slot;
            fallback_slot.family_id = family;
            fallback_slot.min_tier = 1;
            fallback_goal.slots.push_back(fallback_slot);
        }
        const std::uint32_t fallback_chaos =
            fallback_registry.index_by_id.at("chaos");
        const std::uint32_t fallback_essence =
            fallback_registry.index_by_id.at(
                "essence:fallback_goal");
        pc_item_state fallback_start;
        pc_item_clear(&fallback_start);
        fallback_start.rarity = PC_RARITY_RARE;
        const std::unordered_map<std::string, double> fallback_prices{
            {"chaos", 1.0}, {"essence:fallback_goal", 0.01}};
        const auto solve_fallback_case = [&]
            (const std::uint64_t work_cap,
             const std::uint64_t strategy_json_cap) {
            CalcContext fallback_calc(
                fallback_session, fallback_goal, fallback_registry,
                {fallback_chaos, fallback_essence},
                false, true, false, std::nullopt, {}, true);
            SolveOptions fallback_options;
            fallback_options.goal_progress_gated_reforges = true;
            fallback_options.focused_expansion_queue_threshold = 1000000;
            fallback_options.max_reforge_work = work_cap;
            fallback_options.max_strategy_json_bytes = strategy_json_cap;
            return solve(
                fallback_calc, fallback_start, fallback_prices,
                fallback_options);
        };
        const SolveResult capped_fallback =
            solve_fallback_case(
                1000, SolveOptions{}.max_strategy_json_bytes);
        const PolicyRefinementTelemetry& capped_telemetry =
            capped_fallback.diagnostics.policy_refinement;
        if (capped_telemetry.status == "direct_core_policy_exact") {
            PC_CHECK(capped_fallback.policy_available);
            PC_CHECK(capped_fallback.converged);
            PC_CHECK(
                capped_fallback.policy_status ==
                SolvePolicyStatus::Exact);
            PC_CHECK(capped_telemetry.core_policy_candidate_present);
            PC_CHECK(
                capped_telemetry.direct_certification_status ==
                "complete");
            PC_CHECK(capped_telemetry.direct_certification_executable);
            PC_CHECK(capped_telemetry.direct_certification_proper);
            PC_CHECK(capped_telemetry.direct_certification_cost_complete);
            PC_CHECK(capped_telemetry.direct_certification_zero_off_policy);
            PC_CHECK(capped_telemetry.direct_certification_cost_reconciled);
            PC_CHECK(capped_telemetry.fallback_publication_attempts == 0);
            PC_CHECK(capped_telemetry.fallback_publication_successes == 0);
            PC_CHECK(
                !capped_fallback.refined_policy_artifact
                     .strategy_json.empty());
            const std::shared_ptr<StrategyImpl> direct_strategy =
                compile_strategy_json(
                    fallback_session,
                    capped_fallback.refined_policy_artifact
                        .strategy_json.data(),
                    capped_fallback.refined_policy_artifact
                        .strategy_json.size());
            PC_CHECK(direct_strategy != nullptr);
        } else {
        PC_CHECK(capped_fallback.policy_available);
        PC_CHECK(!capped_fallback.converged);
        PC_CHECK(
            capped_fallback.policy_status ==
            SolvePolicyStatus::BoundedFeasible);
        PC_CHECK(capped_fallback.primitive_renewal_witness.valid);
        PC_CHECK(
            !capped_fallback.refined_policy_artifact
                 .strategy_json.empty());
        PC_CHECK(capped_telemetry.status ==
                 "certified_fallback_retained");
        PC_CHECK(capped_telemetry.fallback_publication_attempts == 1);
        PC_CHECK(capped_telemetry.fallback_publication_successes == 1);
        PC_CHECK(capped_telemetry.bounded_publication_retained);
        PC_CHECK(capped_telemetry.fallback_portfolio_candidates == 0);
        PC_CHECK(capped_telemetry.fallback_portfolio_owned_bytes == 0);
        PC_CHECK(
            capped_telemetry.preferred_candidate_upper <
            capped_telemetry.published_fallback_upper);
        PC_CHECK(near(
            capped_fallback.upper_bound,
            capped_telemetry.published_fallback_upper, 1e-12));
        PC_CHECK(near(
            capped_fallback.evaluated_policy_cost,
            capped_telemetry.published_fallback_evaluated_cost, 1e-12));
        PC_CHECK(
            capped_telemetry.published_fallback_witness_hash ==
            capped_fallback.primitive_renewal_witness.witness_hash);
        PC_CHECK(!capped_telemetry
                      .preferred_publication_failure_reason.empty());
        PC_CHECK(capped_fallback.diagnostics
                     .policy_publication_failure_reason.empty());
        PC_CHECK(
            capped_fallback.lower_bound <=
            capped_fallback.upper_bound + 1e-9);
        const std::shared_ptr<StrategyImpl> fallback_strategy =
            compile_strategy_json(
                fallback_session,
                capped_fallback.refined_policy_artifact
                    .strategy_json.data(),
                capped_fallback.refined_policy_artifact
                    .strategy_json.size());
        PC_CHECK(fallback_strategy != nullptr);

        const SolveResult preferred = solve_fallback_case(
            2000, SolveOptions{}.max_strategy_json_bytes);
        PC_CHECK(preferred.policy_available);
        PC_CHECK(preferred.policy_status == SolvePolicyStatus::Exact);
        PC_CHECK(
            preferred.diagnostics.policy_refinement
                .fallback_publication_attempts == 0);
        PC_CHECK(
            preferred.diagnostics.policy_refinement
                .fallback_publication_successes == 0);
        PC_CHECK(
            preferred.upper_bound < capped_fallback.upper_bound);

        const SolveResult compile_limited_fallback =
            solve_fallback_case(
                2000,
                capped_fallback.refined_policy_artifact
                    .strategy_json.size());
        PC_CHECK(compile_limited_fallback.policy_available);
        PC_CHECK(
            compile_limited_fallback.policy_status ==
            SolvePolicyStatus::Exact);
        PC_CHECK(
            compile_limited_fallback.diagnostics.policy_refinement
                .fallback_publication_attempts == 0);
        PC_CHECK(
            compile_limited_fallback.diagnostics.policy_refinement
                .fallback_publication_successes == 0);
        PC_CHECK(near(
            compile_limited_fallback.upper_bound,
            preferred.upper_bound, 1e-12));
        PC_CHECK(
            compile_limited_fallback.diagnostics.policy_refinement
                .preferred_publication_failure_reason.empty());
        }
    }
    PC_CHECK(solved.policy_available);
    const PolicyRefinementTelemetry& refinement_telemetry =
        solved.diagnostics.policy_refinement;
    if (refinement_telemetry.status == "direct_core_policy_exact") {
        PC_CHECK(solved.converged);
        PC_CHECK(solved.policy_status == SolvePolicyStatus::Exact);
        PC_CHECK(solved.termination == SolveTermination::ExactClosed);
        PC_CHECK(refinement_telemetry.core_policy_candidate_present);
        PC_CHECK(refinement_telemetry.core_policy_status == "exact");
        PC_CHECK(
            refinement_telemetry.direct_certification_status ==
            "complete");
        PC_CHECK(refinement_telemetry.direct_certification_executable);
        PC_CHECK(refinement_telemetry.direct_certification_proper);
        PC_CHECK(refinement_telemetry.direct_certification_cost_complete);
        PC_CHECK(refinement_telemetry.direct_certification_zero_off_policy);
        PC_CHECK(refinement_telemetry.direct_certification_cost_reconciled);
        PC_CHECK(refinement_telemetry.strict_lift_status == "not_run");
        PC_CHECK(
            refinement_telemetry.publication_status ==
            "exact_core_policy");
        PC_CHECK(
            !solved.refined_policy_artifact.strategy_json.empty());
        PC_CHECK(
            solved.policy[solved.start_state].index == regal);
        const std::string direct_json = serialize_solver_telemetry(
            calc, &solved, nullptr, std::nullopt, nullptr);
        PC_CHECK(valid_json_object(direct_json));
        PC_CHECK(direct_json.find(
                     "\"direct_certification\":{"
                     "\"status\":\"complete\"") !=
                 std::string::npos);
        PC_CHECK(direct_json.find(
                     "\"publication\":{"
                     "\"status\":\"exact_core_policy\"") !=
                 std::string::npos);
    } else {
    PC_CHECK(!solved.converged);
    PC_CHECK(
        solved.policy_status ==
        SolvePolicyStatus::BoundedFeasible);
    PC_CHECK(
        solved.termination ==
        SolveTermination::ExactClosed);
    PC_CHECK(
        solved.termination !=
        SolveTermination::NoExecutablePolicy);
    PC_CHECK(solved.diagnostics.policy_refinement.triggers > 0);
    PC_CHECK(refinement_telemetry.status == "complete");
    PC_CHECK(refinement_telemetry.resource_cap.empty());
    PC_CHECK(
        refinement_telemetry.policy_reachable_coarse_states > 0);
    PC_CHECK(refinement_telemetry.exact_states > 0);
    PC_CHECK(refinement_telemetry.retained_exact_states > 0);
    PC_CHECK(refinement_telemetry.exact_classes > 0);
    PC_CHECK(refinement_telemetry.exact_transitions > 0);
    PC_CHECK(refinement_telemetry.exact_kernels > 0);
    PC_CHECK(refinement_telemetry.selected_rows_begun > 0);
    PC_CHECK(refinement_telemetry.selected_rows_completed > 0);
    PC_CHECK(refinement_telemetry.selected_transitions > 0);
    PC_CHECK(refinement_telemetry.alternative_rows_begun > 0);
    PC_CHECK(refinement_telemetry.alternative_rows_completed > 0);
    PC_CHECK(refinement_telemetry.alternative_transitions > 0);
    PC_CHECK(refinement_telemetry.alternative_reforge_work > 0);
    PC_CHECK(refinement_telemetry.strict_reforge_active_work > 0);
    PC_CHECK(refinement_telemetry.strict_reforge_logical_work_v1 > 0);
    PC_CHECK(refinement_telemetry.strict_reforge_effort.rows_published > 0);
    PC_CHECK(!refinement_telemetry.strict_reforge_row_samples.empty());
    PC_CHECK(
        refinement_telemetry.strict_reforge_row_samples.size() <= 64);
    PC_CHECK(std::any_of(
        refinement_telemetry.strict_reforge_row_samples.begin(),
        refinement_telemetry.strict_reforge_row_samples.end(),
        [](const ReforgeRowTelemetry& row) {
            return row.owner == ReforgeRowOwner::StrictSelected &&
                   row.disposition ==
                       ReforgeRowDisposition::Published;
        }));
    PC_CHECK(std::any_of(
        refinement_telemetry.strict_reforge_row_samples.begin(),
        refinement_telemetry.strict_reforge_row_samples.end(),
        [](const ReforgeRowTelemetry& row) {
            return row.owner == ReforgeRowOwner::StrictAlternative;
        }));
    PC_CHECK(
        refinement_telemetry.selected_rows_completed +
            refinement_telemetry.alternative_rows_completed ==
        refinement_telemetry.exact_kernels);
    PC_CHECK(
        refinement_telemetry.selected_transitions +
            refinement_telemetry.alternative_transitions ==
        refinement_telemetry.exact_transitions);
    PC_CHECK(
        refinement_telemetry.alternative_obligations_created > 0);
    PC_CHECK(
        refinement_telemetry.unresolved_alternative_obligations <
        refinement_telemetry.alternative_obligations_created);
    PC_CHECK(
        refinement_telemetry.alternative_rows_avoided >=
        refinement_telemetry.alternative_obligations_created);
    PC_CHECK(refinement_telemetry.action_accounting_complete);
    PC_CHECK(refinement_telemetry.alternative_scheduling_rounds > 0);
    PC_CHECK(
        refinement_telemetry.alternative_obligations_scheduled > 0);
    PC_CHECK(
        refinement_telemetry.alternative_obligations_certified > 0);
    PC_CHECK(
        refinement_telemetry.competitive_alternatives_remaining ==
        refinement_telemetry.unresolved_alternative_obligations);
    PC_CHECK(refinement_telemetry.bounded_publication_retained);
    PC_CHECK(
        !refinement_telemetry.exact_alternative_envelope_closed);
    std::printf(
        "solver competitive alternatives: obligations=%llu "
        "scheduled=%llu certified=%llu partial=%llu "
        "noncompetitive=%llu interrupted=%llu remaining=%llu "
        "selected_work=%llu alternative_work=%llu improvements=%llu\n",
        static_cast<unsigned long long>(
            refinement_telemetry.alternative_obligations_created),
        static_cast<unsigned long long>(
            refinement_telemetry.alternative_obligations_scheduled),
        static_cast<unsigned long long>(
            refinement_telemetry.alternative_obligations_certified),
        static_cast<unsigned long long>(
            refinement_telemetry
                .alternative_obligations_partially_evaluated),
        static_cast<unsigned long long>(
            refinement_telemetry.alternative_obligations_noncompetitive),
        static_cast<unsigned long long>(
            refinement_telemetry
                .alternative_obligations_resource_interrupted),
        static_cast<unsigned long long>(
            refinement_telemetry.competitive_alternatives_remaining),
        static_cast<unsigned long long>(
            refinement_telemetry.selected_reforge_work),
        static_cast<unsigned long long>(
            refinement_telemetry.alternative_reforge_work),
        static_cast<unsigned long long>(
            refinement_telemetry.alternative_policy_improvements));
    PC_CHECK(refinement_telemetry.alternative_obligation_bytes > 0);
    PC_CHECK(
        refinement_telemetry.work_to_first_partition.has_value());
    PC_CHECK(
        refinement_telemetry.work_to_first_executable_upper.has_value());
    PC_CHECK(
        *refinement_telemetry.work_to_first_partition <=
        *refinement_telemetry.work_to_first_executable_upper);
    PC_CHECK(
        refinement_telemetry.wall_ns_to_first_partition.has_value());
    PC_CHECK(
        refinement_telemetry
            .wall_ns_to_first_executable_upper.has_value());
    PC_CHECK(
        *refinement_telemetry.wall_ns_to_first_partition <=
        *refinement_telemetry.wall_ns_to_first_executable_upper);
    PC_CHECK(
        *refinement_telemetry.work_to_first_partition ==
        refinement_telemetry.selected_reforge_work);
    PC_CHECK(
        refinement_telemetry
            .alternatives_materialized_before_first_upper ==
        0);
    PC_CHECK(refinement_telemetry.exact_state_reuses > 0);
    PC_CHECK(refinement_telemetry.collapse_events == 0);
    PC_CHECK(
        refinement_telemetry.memory_limit_bytes ==
        options.max_solver_owned_bytes);
    PC_CHECK(refinement_telemetry.retained_artifact_bytes > 0);
    PC_CHECK(refinement_telemetry.fixed_point_checked);
    PC_CHECK(refinement_telemetry.fixed_point_complete);
    PC_CHECK(refinement_telemetry.lumpability_checked);
    PC_CHECK(refinement_telemetry.lumpable);
    PC_CHECK(refinement_telemetry.lumpability_checks > 0);
    PC_CHECK(refinement_telemetry.class_policy_checked);
    PC_CHECK(refinement_telemetry.class_policy_proper);
    PC_CHECK(refinement_telemetry.compiled_assertion_checked);
    PC_CHECK(refinement_telemetry.compiled_policy_proper);
    PC_CHECK(refinement_telemetry.zero_off_policy);
    PC_CHECK(refinement_telemetry.cost_reconciled);
    PC_CHECK(refinement_telemetry.coarse_value_reconciled);
    PC_CHECK(
        solved.policy[solved.start_state].index == regal);
    PC_CHECK(
        solved.diagnostics.policy_compatibility_supported);
    PC_CHECK(
        !solved.refined_policy_artifact.strategy_json.empty());
    const std::string refinement_json = serialize_solver_telemetry(
        calc, &solved, nullptr, std::nullopt, nullptr);
    PC_CHECK(valid_json_object(refinement_json));
    PC_CHECK(refinement_json.find("\"certification_work\":{") !=
             std::string::npos);
    PC_CHECK(refinement_json.find(
                 "\"reforge_resource_accounting\":"
                 "{\"schema_version\":2") != std::string::npos);
    PC_CHECK(refinement_json.find("\"owner\":\"strict_selected\"") !=
             std::string::npos);
    PC_CHECK(refinement_json.find("\"owner\":\"strict_alternative\"") !=
             std::string::npos);
    PC_CHECK(refinement_json.find("\"evaluator\":\"v3_factored\"") !=
             std::string::npos);
    bool saw_production_selected_v3 = false;
    bool saw_production_alternative_v3 = false;
    for (const ReforgeRowTelemetry& row :
         refinement_telemetry.strict_reforge_row_samples) {
        if (row.owner == ReforgeRowOwner::StrictSelected) {
            saw_production_selected_v3 = true;
            PC_CHECK(
                row.evaluator ==
                ReforgeEvaluatorVersion::V3Factored);
        } else if (row.owner == ReforgeRowOwner::StrictAlternative) {
            saw_production_alternative_v3 = true;
            PC_CHECK(
                row.evaluator ==
                ReforgeEvaluatorVersion::V3Factored);
        }
    }
    PC_CHECK(saw_production_selected_v3);
    PC_CHECK(saw_production_alternative_v3);
    PC_CHECK(refinement_json.find("\"work_to_first_partition\":") !=
             std::string::npos);
    PC_CHECK(refinement_json.find("\"wall_ns_to_first_partition\":") !=
             std::string::npos);
    PC_CHECK(refinement_json.find(
                 "\"alternative_obligations_created\":") !=
             std::string::npos);
    PC_CHECK(refinement_json.find(
                 "\"action_accounting_complete\":true") !=
             std::string::npos);
    PC_CHECK(refinement_json.find(
                 "\"obligations_scheduled\":") !=
             std::string::npos);
    PC_CHECK(refinement_json.find(
                 "\"bounded_publication_retained\":true") !=
             std::string::npos);
    }

    const refinement::PolicyExactLiftCertificate lifted =
        refinement::lift_policy_quotient(
            calc, solved, start, prices, options,
            "focused policy-guided exact lift");
    report_lift_failure("Regal lift", lifted);
    PC_CHECK(lifted.adapter.strict_session_constructions == 1);
    PC_CHECK(lifted.adapter.strict_full_restarts == 0);
    PC_CHECK(lifted.adapter.strict_partition_updates == 1);
    PC_CHECK(
        lifted.status ==
        refinement::PolicyExactLiftStatus::Complete);
    PC_CHECK(lifted.executable);
    PC_CHECK(lifted.lumpable);
    PC_CHECK(!lifted.policy_changed);
    PC_CHECK(lifted.coarse_value_reconciled);
    PC_CHECK(
        lifted.refinement.status ==
        refinement::RefinementStatus::Complete);
    PC_CHECK(
        lifted.class_evaluation.status ==
        refinement::PolicyEvaluationStatus::Complete);
    PC_CHECK(lifted.class_evaluation.proper);
    PC_CHECK(lifted.class_evaluation.converged);
    PC_CHECK(lifted.compiled.executable);
    PC_CHECK(lifted.compiled.proper);
    PC_CHECK(lifted.compiled.zero_off_policy);
    PC_CHECK(lifted.compiled.cost_reconciled);
    PC_CHECK(
        lifted.compiled.compilation.working_states ==
        reachable_nonterminal_refinement_classes(lifted));
    PC_CHECK(
        lifted.compiled.off_policy_probability <= 1e-10);
    PC_CHECK(
        lifted.compiled.evaluation.success_probability >=
        1.0 - 1e-10);
    PC_CHECK(near(
        lifted.exact_start_cost,
        solved.evaluated_policy_cost, 1e-7));
    PC_CHECK(
        !lifted.compiled.strategy_json.empty());
    PC_CHECK(
        lifted.refinement.telemetry.final_refinement_classes > 0);
    PC_CHECK(
        lifted.refinement.telemetry.exact_states >=
        lifted.refinement.telemetry.final_refinement_classes);
    PC_CHECK(
        lifted.refinement.telemetry.lumpability_checks > 0);
    PC_CHECK(
        lifted.adapter.canonical_successor_collapses > 0);

    const auto cooperative_lift = [&](const std::uint32_t chunk) {
        refinement::PolicyExactLiftWork work(
            calc, solved, start, prices, options,
            "focused policy-guided exact lift");
        std::uint64_t steps = 0;
        double prior_verified_upper = kInfinity;
        bool saw_live_session = false;
        while (!work.progress().done) {
            work.step(chunk);
            ++steps;
            PC_CHECK(steps < 1000000);
            const refinement::PolicyLiftAdapterTelemetry& live =
                work.live_adapter_telemetry();
            PC_CHECK(live.strict_full_restarts == 0);
            saw_live_session = saw_live_session ||
                live.strict_session_constructions == 1;
            const refinement::PolicyExactLiftProgress progress =
                work.progress();
            if (std::isfinite(
                    progress.verified_executable_upper_bound)) {
                PC_CHECK(
                    progress.verified_executable_upper_bound <=
                    prior_verified_upper + 1e-12);
                prior_verified_upper =
                    progress.verified_executable_upper_bound;
            }
        }
        refinement::PolicyExactLiftCertificate certificate =
            work.take_result();
        PC_CHECK(saw_live_session);
        PC_CHECK(certificate.adapter.strict_session_constructions == 1);
        PC_CHECK(near(
            prior_verified_upper,
            certificate.compiled.exact_cost, 1e-12));
        return certificate;
    };
    for (const std::uint32_t chunk : {1u, 8u}) {
        const refinement::PolicyExactLiftCertificate chunked =
            cooperative_lift(chunk);
        PC_CHECK(chunked.status == lifted.status);
        PC_CHECK(chunked.executable == lifted.executable);
        PC_CHECK(chunked.lumpable == lifted.lumpable);
        PC_CHECK(chunked.policy_changed == lifted.policy_changed);
        PC_CHECK(near(
            chunked.exact_start_cost,
            lifted.exact_start_cost, 1e-12));
        PC_CHECK(
            chunked.compiled.strategy_json ==
            lifted.compiled.strategy_json);
        PC_CHECK(
            chunked.compiled.compilation.nodes ==
            lifted.compiled.compilation.nodes);
        PC_CHECK(
            chunked.compiled.compilation.edges ==
            lifted.compiled.compilation.edges);
        PC_CHECK(near(
            chunked.compiled.evaluation.total_expected_cost,
            lifted.compiled.evaluation.total_expected_cost, 1e-12));
        PC_CHECK(
            chunked.adapter.strict_kernels_built ==
            lifted.adapter.strict_kernels_built);
        PC_CHECK(
            chunked.adapter.strict_transitions_built ==
            lifted.adapter.strict_transitions_built);
    }

    const auto check_strict_evaluator = [](
            const refinement::PolicyExactLiftCertificate& certificate,
            const ReforgeEvaluatorVersion expected) {
        PC_CHECK(!certificate.adapter.strict_reforge_row_samples.empty());
        for (const ReforgeRowTelemetry& row :
             certificate.adapter.strict_reforge_row_samples) {
            PC_CHECK(row.evaluator == expected);
        }
    };
    check_strict_evaluator(
        lifted, ReforgeEvaluatorVersion::V3Factored);

    SolveOptions raw_oracle_options = options;
    raw_oracle_options.raw_strict_reforge_oracle_diagnostic = true;
    const refinement::PolicyExactLiftCertificate raw_oracle_lifted =
        refinement::lift_policy_exact(
            calc, solved, start, prices, raw_oracle_options,
            "focused policy-guided exact lift");
    report_lift_failure("Raw V1 rollback lift", raw_oracle_lifted);
    PC_CHECK(
        raw_oracle_lifted.status ==
        refinement::PolicyExactLiftStatus::Complete);
    PC_CHECK(raw_oracle_lifted.executable);
    PC_CHECK(raw_oracle_lifted.compiled.executable);
    PC_CHECK(raw_oracle_lifted.compiled.zero_off_policy);
    PC_CHECK(raw_oracle_lifted.compiled.cost_reconciled);
    PC_CHECK(near(
        raw_oracle_lifted.exact_start_cost,
        lifted.exact_start_cost, 1e-12));
    PC_CHECK(
        raw_oracle_lifted.compiled.strategy_json ==
        lifted.compiled.strategy_json);
    check_strict_evaluator(
        raw_oracle_lifted, ReforgeEvaluatorVersion::V1Raw);

    SolveOptions sparse_diagnostic_options = options;
    sparse_diagnostic_options.projected_reforge_frontier_diagnostic = true;
    const refinement::PolicyExactLiftCertificate sparse_diagnostic_lifted =
        refinement::lift_policy_exact(
            calc, solved, start, prices, sparse_diagnostic_options,
            "focused policy-guided exact lift");
    report_lift_failure(
        "Sparse V2 diagnostic lift", sparse_diagnostic_lifted);
    PC_CHECK(
        sparse_diagnostic_lifted.status ==
        refinement::PolicyExactLiftStatus::Complete);
    PC_CHECK(sparse_diagnostic_lifted.executable);
    PC_CHECK(sparse_diagnostic_lifted.compiled.executable);
    PC_CHECK(sparse_diagnostic_lifted.compiled.zero_off_policy);
    PC_CHECK(sparse_diagnostic_lifted.compiled.cost_reconciled);
    PC_CHECK(near(
        sparse_diagnostic_lifted.exact_start_cost,
        lifted.exact_start_cost, 1e-12));
    PC_CHECK(
        sparse_diagnostic_lifted.compiled.strategy_json ==
        lifted.compiled.strategy_json);
    check_strict_evaluator(
        sparse_diagnostic_lifted,
        ReforgeEvaluatorVersion::V2Sparse);

    refinement::RefinementLimits interrupted_limits;
    interrupted_limits.max_exact_kernels =
        static_cast<std::uint32_t>(
            lifted.adapter.selected_rows_completed);
    interrupted_limits.max_estimated_memory_bytes =
        options.max_solver_owned_bytes;
    const refinement::PolicyExactLiftCertificate interrupted =
        refinement::lift_policy_quotient(
            calc, solved, start, prices, options,
            "resource-interrupted competitive alternatives",
            &interrupted_limits);
    report_lift_failure("Interrupted alternative lift", interrupted);
    PC_CHECK(
        interrupted.status ==
        refinement::PolicyExactLiftStatus::Complete);
    PC_CHECK(interrupted.executable);
    PC_CHECK(interrupted.compiled.executable);
    if (interrupted.adapter
            .alternative_obligations_resource_interrupted != 0) {
        PC_CHECK(
            interrupted.adapter
                .alternative_obligations_resource_interrupted == 1);
        PC_CHECK(interrupted.adapter.bounded_publication_retained);
        PC_CHECK(
            interrupted.adapter.competitive_alternatives_remaining > 0);
        PC_CHECK(
            !interrupted.adapter.exact_alternative_envelope_closed);
    } else {
        /* A directly certified, globally closed core solve can discharge the
         * alternative envelope without spending beyond the selected-row cap.
         * That is stronger than the legacy resource-interrupted outcome. */
        PC_CHECK(interrupted.adapter.global_lower_bound_closed);
        PC_CHECK(interrupted.adapter.exact_alternative_envelope_closed);
        PC_CHECK(
            interrupted.adapter.competitive_alternatives_remaining == 0);
    }
    PC_CHECK(
        interrupted.adapter
            .alternatives_materialized_before_first_upper == 0);

    PolicyCompilationTelemetry retained_telemetry;
    const std::string retained =
        compile_policy_strategy_json(
            calc, solved, "retained exact lift",
            &retained_telemetry);
    PC_CHECK(
        retained ==
        solved.refined_policy_artifact.strategy_json);
    PC_CHECK(
        retained_telemetry.working_states ==
        solved.refined_policy_artifact.working_states);
    PC_CHECK(
        retained_telemetry.policy_regions ==
        solved.refined_policy_artifact.policy_regions);
    PC_CHECK(
        retained_telemetry.nodes ==
        solved.refined_policy_artifact.nodes);
    PC_CHECK(
        retained_telemetry.edges ==
        solved.refined_policy_artifact.edges);
    PC_CHECK(
        retained_telemetry.previously_accounted_peak_owned_bytes ==
        solved.refined_policy_artifact
            .previously_accounted_peak_owned_bytes);
    PC_CHECK(
        retained_telemetry.complete_peak_owned_bytes ==
        solved.refined_policy_artifact.complete_peak_owned_bytes);
    PC_CHECK(
        retained_telemetry.peak_owned_bytes ==
        retained_telemetry.complete_peak_owned_bytes);

    const std::shared_ptr<StrategyImpl> retained_strategy =
        compile_strategy_json(
            session, retained.data(), retained.size());
    auto economy = std::make_shared<EconomyImpl>();
    economy->id = "focused-policy-lift";
    economy->prices = prices;
    StrategyEvalOptions evaluation_options;
    evaluation_options.economy = economy;
    const StrategyEvalResult retained_evaluation =
        evaluate_strategy(
            *retained_strategy, evaluation_options);
    const double retained_off_policy =
        retained_evaluation.failure_probability +
        retained_evaluation.stop_probability +
        retained_evaluation.action_not_applied_probability +
        retained_evaluation.no_matching_edge_probability +
        retained_evaluation.unresolved_probability;
    PC_CHECK(retained_evaluation.converged);
    PC_CHECK(retained_evaluation.cost_complete);
    PC_CHECK(
        retained_evaluation.success_probability >=
        1.0 - 1e-10);
    PC_CHECK(retained_off_policy <= 1e-10);
    PC_CHECK(near(
        retained_evaluation.total_expected_cost,
        solved.evaluated_policy_cost, 1e-7));
}

void run_policy_guided_exalt_lift_tests() {
    auto session = make_solve_session();
    ActionRegistry registry = build_action_registry(*session);

    GoalSpec goal;
    goal.rarity = PC_RARITY_RARE;
    GoalSlot slot;
    slot.family_id = 104; /* suffix mod 5 */
    slot.min_tier = 1;
    goal.slots.push_back(slot);

    const std::uint32_t chaos =
        registry.index_by_id.at("chaos");
    const std::uint32_t exalt =
        registry.index_by_id.at("exalt");
    const std::uint32_t restart =
        registry.index_by_id.at("restart");
    CalcContext calc(
        session, goal, registry,
        {chaos, exalt, restart},
        false, true, false, std::nullopt, {}, true);

    pc_item_state start;
    pc_item_clear(&start);
    start.rarity = PC_RARITY_RARE;
    place(&start, PC_SIDE_PREFIX, 0, 10);
    const std::unordered_map<std::string, double> prices{
        {"chaos", 1.0},
        {"exalt", 0.01},
        {"base", 10.0}};
    SolveOptions options;
    const SolveResult solved =
        solve(calc, start, prices, options);

    PC_CHECK(solved.policy_available);
    PC_CHECK(solved.converged);
    PC_CHECK(
        solved.policy_status ==
        SolvePolicyStatus::Exact);
    /* The strict global envelope supersedes any earlier coarse stop cause. */
    PC_CHECK(
        solved.termination ==
        SolveTermination::ExactClosed);
    PC_CHECK(
        solved.termination !=
        SolveTermination::NoExecutablePolicy);
    PC_CHECK(
        solved.policy[solved.start_state].index == exalt);
    PC_CHECK(
        solved.diagnostics.policy_refinement.triggers > 0);
    PC_CHECK(
        solved.diagnostics.policy_refinement
            .global_lower_bound_closed);
    PC_CHECK(
        solved.diagnostics.policy_refinement
            .exact_alternative_envelope_closed);
    PC_CHECK(near(
        solved.lower_bound, solved.upper_bound, 1e-12));
    PC_CHECK(
        solved.diagnostics.policy_compatibility_supported);
    PC_CHECK(
        !solved.refined_policy_artifact.strategy_json.empty());

    const refinement::PolicyExactLiftCertificate lifted =
        refinement::lift_policy_exact(
            calc, solved, start, prices, options,
            "focused one-goal suffix Exalt lift");
    report_lift_failure("Exalt lift", lifted);
    PC_CHECK(
        lifted.status ==
        refinement::PolicyExactLiftStatus::Complete);
    PC_CHECK(lifted.executable);
    PC_CHECK(lifted.lumpable);
    PC_CHECK(
        lifted.refinement.status ==
        refinement::RefinementStatus::Complete);
    PC_CHECK(
        lifted.class_evaluation.status ==
        refinement::PolicyEvaluationStatus::Complete);
    PC_CHECK(lifted.class_evaluation.proper);
    PC_CHECK(lifted.compiled.executable);
    PC_CHECK(lifted.compiled.proper);
    PC_CHECK(lifted.compiled.zero_off_policy);
    PC_CHECK(lifted.compiled.cost_reconciled);
    PC_CHECK(
        lifted.compiled.compilation.working_states ==
        reachable_nonterminal_refinement_classes(lifted));
    PC_CHECK(
        lifted.compiled.evaluation.success_probability >=
        1.0 - 1e-10);
    PC_CHECK(near(
        lifted.compiled.exact_cost,
        lifted.exact_start_cost, 1e-7));

    const std::string& retained =
        solved.refined_policy_artifact.strategy_json;
    const std::shared_ptr<StrategyImpl> retained_strategy =
        compile_strategy_json(
            session, retained.data(), retained.size());
    auto economy = std::make_shared<EconomyImpl>();
    economy->id = "focused-exalt-lift";
    economy->prices = prices;
    StrategyEvalOptions evaluation_options;
    evaluation_options.economy = economy;
    const StrategyEvalResult retained_evaluation =
        evaluate_strategy(
            *retained_strategy, evaluation_options);
    PC_CHECK(retained_evaluation.converged);
    PC_CHECK(retained_evaluation.cost_complete);
    PC_CHECK(
        retained_evaluation.success_probability >=
        1.0 - 1e-10);
    PC_CHECK(near(
        retained_evaluation.total_expected_cost,
        solved.evaluated_policy_cost, 1e-7));
}

/*
 * Production Gate 3 control: one coarse magic-prefix junk carrier merges
 * exact modifiers with very different exclusion effects. The authored
 * coarse policy Regals every such carrier. Under strict kernels, blocking
 * the high-weight prefix group makes Regal attractive, while a different
 * member is cheaper to Alteration-reroll first. The local optimizer must
 * retain both decisions under the same coarse parent.
 */
void run_policy_guided_local_reoptimization_tests() {
    auto session = make_solve_session();
    const std::vector<std::uint32_t> weights{
        1000, 1000, 1000, 1, 1, 100, 1, 1};
    session->base_spawn_weight = weights;
    session->base_roll_weight = weights;
    ActionRegistry registry = build_action_registry(*session);

    GoalSpec goal;
    goal.rarity = PC_RARITY_RARE;
    GoalSlot slot;
    slot.family_id = 104; /* suffix mod 5 */
    slot.min_tier = 1;
    goal.slots.push_back(slot);

    const std::uint32_t transmute =
        registry.index_by_id.at("transmute");
    const std::uint32_t alteration =
        registry.index_by_id.at("alteration");
    const std::uint32_t regal =
        registry.index_by_id.at("regal");
    const std::uint32_t chaos =
        registry.index_by_id.at("chaos");
    CalcContext calc(
        session, goal, registry,
        {transmute, alteration, regal, chaos},
        false, true, false, std::nullopt, {}, true);

    pc_item_state start;
    pc_item_clear(&start);
    start.rarity = PC_RARITY_NORMAL;
    const std::uint32_t start_state =
        calc.intern_item(start);
    std::set<std::uint32_t> pending{start_state};
    std::map<std::uint32_t, std::uint32_t> selected_by_state;
    while (!pending.empty()) {
        const std::uint32_t state = *pending.begin();
        pending.erase(pending.begin());
        if (calc.is_goal_state(calc.state(state)) ||
            selected_by_state.contains(state)) {
            continue;
        }
        std::uint32_t action = kNoId;
        switch (calc.state(state).rarity) {
        case PC_RARITY_NORMAL:
            action = transmute;
            break;
        case PC_RARITY_MAGIC:
            action = regal;
            break;
        case PC_RARITY_RARE:
            action = chaos;
            break;
        default:
            break;
        }
        PC_CHECK(action != kNoId);
        if (action == kNoId) return;
        const OutcomeDistribution& distribution =
            calc.outcomes(state, action, false);
        PC_CHECK(distribution.supported);
        PC_CHECK(distribution.choice_groups.empty());
        if (!distribution.supported ||
            !distribution.choice_groups.empty()) {
            return;
        }
        selected_by_state.emplace(state, action);
        for (const OutcomeEntry& entry :
             distribution.entries) {
            if (entry.probability > 0.0 &&
                !calc.is_goal_state(
                    calc.state(entry.state))) {
                pending.insert(entry.state);
            }
        }
        PC_CHECK(calc.state_count() < 5000);
        if (calc.state_count() >= 5000) return;
    }

    SolveOptions options;
    SolveResult authored;
    authored.converged = true;
    authored.policy_available = true;
    authored.policy_status = SolvePolicyStatus::Exact;
    authored.termination = SolveTermination::ExactClosed;
    authored.start_state = start_state;
    authored.has_exact_start_item = true;
    authored.exact_start_item = start;
    authored.lower_bound = 0.0;
    authored.upper_bound = 0.0;
    authored.evaluated_policy_cost = 0.0;
    authored.absolute_optimality_gap = 0.0;
    authored.relative_optimality_gap = 0.0;
    authored.options = options;
    const std::uint32_t state_count = calc.state_count();
    authored.values.assign(state_count, 0.0);
    authored.policy.assign(state_count, PolicyOperatorRef{});
    authored.expanded.assign(state_count, 1);
    authored.goal_states.assign(state_count, 0);
    authored.policy_reachable.assign(state_count, 1);
    authored.unveil_preferences.resize(state_count);
    authored.option_unveil_preferences.resize(state_count);
    for (std::uint32_t state = 0;
         state < state_count; ++state) {
        authored.goal_states[state] =
            calc.is_goal_state(calc.state(state)) ? 1 : 0;
    }
    for (const auto& [state, action] :
         selected_by_state) {
        authored.policy[state] = PolicyOperatorRef{
            PlannerOperatorKind::Primitive, action};
    }

    const std::unordered_map<std::string, double> prices{
        {"transmute", 0.1},
        {"alteration", 0.3},
        {"regal", 4.7},
        {"chaos", 10.0}};
    const refinement::PolicyExactLiftCertificate lifted =
        refinement::lift_policy_exact(
            calc, authored, start, prices, options,
            "focused local exact policy improvement");
    report_lift_failure("local action reoptimization", lifted);
    PC_CHECK(
        lifted.status ==
        refinement::PolicyExactLiftStatus::Complete);
    PC_CHECK(lifted.executable);
    PC_CHECK(lifted.compiled.executable);
    PC_CHECK(lifted.compiled.cost_reconciled);
    PC_CHECK(
        lifted.compiled.strategy_json.find(
            "\"id\":\"refined_parent_") !=
        std::string::npos);
    PC_CHECK(
        lifted.compiled.compilation.working_states ==
        reachable_nonterminal_refinement_classes(lifted));
    PC_CHECK(lifted.adapter.local_reoptimizations > 0);
    PC_CHECK(
        lifted.adapter.local_state_action_rows_scheduled > 0);
    PC_CHECK(
        lifted.adapter.local_state_action_rows_evaluated > 0);
    PC_CHECK(lifted.adapter.local_policy_changes > 0);
    PC_CHECK(lifted.adapter.local_value_changes > 0);
    PC_CHECK(lifted.policy_changed);
    PC_CHECK(!lifted.coarse_value_reconciled);
    PC_CHECK(
        lifted.adapter.local_reoptimization_rounds > 0);
    PC_CHECK(lifted.exact_start_cost > 0.0);
    PC_CHECK(near(
        lifted.compiled.exact_cost,
        lifted.exact_start_cost, 1e-7));

    std::map<std::uint32_t, std::set<std::uint32_t>>
        actions_by_coarse_parent;
    for (const refinement::RefinedPolicyClass& policy_class :
         lifted.refinement.classes) {
        if (policy_class.selected_action.has_value()) {
            actions_by_coarse_parent[
                policy_class.coarse_state]
                    .insert(
                        policy_class.selected_action->action_id);
        }
    }
    bool divergent_subclasses = false;
    for (const auto& [unused, actions] :
         actions_by_coarse_parent) {
        (void)unused;
        if (actions.size() > 1) {
            divergent_subclasses = true;
            break;
        }
    }
    PC_CHECK(divergent_subclasses);

    refinement::RefinementLimits capped_limits;
    capped_limits.max_estimated_memory_bytes = 1;
    const refinement::PolicyExactLiftCertificate capped =
        refinement::lift_policy_exact(
            calc, authored, start, prices, options,
            "capped local exact policy improvement",
            &capped_limits);
    PC_CHECK(
        capped.status ==
        refinement::PolicyExactLiftStatus::ResourceCap);
    PC_CHECK(
        capped.resource_cap ==
        "max_estimated_memory_bytes");
}

/*
 * A destructive incumbent can legitimately merge concrete members that an
 * alternative action must split.  Chaos has the same row for every member of
 * one rare coarse parent.  Exalt preserves and observes the existing prefix:
 * prefix mod 0 leaves the dominant junk prefix in its pool (Exalt is worse
 * than Chaos), while prefix mod 3 excludes that junk group (Exalt is better).
 * The canonical first member is therefore not a sound proxy for the class.
 */
void run_candidate_induced_exact_subclass_tests() {
    auto session = make_solve_session();
    const std::vector<std::uint32_t> weights{
        1, 1, 1, 10000, 1, 100, 1, 1};
    session->base_spawn_weight = weights;
    session->base_roll_weight = weights;
    ActionRegistry registry = build_action_registry(*session);

    GoalSpec goal;
    goal.rarity = PC_RARITY_RARE;
    GoalSlot slot;
    slot.family_id = 104; /* suffix mod 5 */
    slot.min_tier = 1;
    goal.slots.push_back(slot);

    const std::uint32_t transmute =
        registry.index_by_id.at("transmute");
    const std::uint32_t regal =
        registry.index_by_id.at("regal");
    const std::uint32_t chaos =
        registry.index_by_id.at("chaos");
    const std::uint32_t exalt =
        registry.index_by_id.at("exalt");
    CalcContext calc(
        session, goal, registry,
        {transmute, regal, chaos, exalt},
        false, true, false, std::nullopt, {}, true);

    pc_item_state start;
    pc_item_clear(&start);
    start.rarity = PC_RARITY_NORMAL;
    const std::uint32_t start_state = calc.intern_item(start);
    std::set<std::uint32_t> pending{start_state};
    std::map<std::uint32_t, std::uint32_t> selected_by_state;
    std::uint32_t triggered_rare_parent = kNoId;
    while (!pending.empty()) {
        const std::uint32_t state = *pending.begin();
        pending.erase(pending.begin());
        if (calc.is_goal_state(calc.state(state)) ||
            selected_by_state.contains(state)) {
            continue;
        }
        std::uint32_t action = kNoId;
        switch (calc.state(state).rarity) {
        case PC_RARITY_NORMAL:
            action = transmute;
            break;
        case PC_RARITY_MAGIC:
            action = regal;
            break;
        case PC_RARITY_RARE:
            action = chaos;
            break;
        default:
            break;
        }
        PC_CHECK(action != kNoId);
        if (action == kNoId) return;
        const OutcomeDistribution& distribution =
            calc.outcomes(state, action, false);
        PC_CHECK(distribution.supported);
        PC_CHECK(distribution.choice_groups.empty());
        if (!distribution.supported ||
            !distribution.choice_groups.empty()) {
            return;
        }
        selected_by_state.emplace(state, action);
        for (const OutcomeEntry& entry : distribution.entries) {
            if (entry.probability <= 0.0 ||
                calc.is_goal_state(calc.state(entry.state))) {
                continue;
            }
            if (action == regal &&
                triggered_rare_parent == kNoId &&
                calc.state(entry.state).rarity == PC_RARITY_RARE) {
                triggered_rare_parent = entry.state;
            }
            pending.insert(entry.state);
        }
        PC_CHECK(calc.state_count() < 5000);
        if (calc.state_count() >= 5000) return;
    }
    PC_CHECK(triggered_rare_parent != kNoId);
    if (triggered_rare_parent == kNoId) return;

    SolveOptions options;
    SolveResult authored;
    authored.converged = true;
    authored.policy_available = true;
    authored.policy_status = SolvePolicyStatus::Exact;
    authored.termination = SolveTermination::ExactClosed;
    authored.start_state = start_state;
    authored.has_exact_start_item = true;
    authored.exact_start_item = start;
    authored.lower_bound = 0.0;
    authored.upper_bound = kInfinity;
    authored.evaluated_policy_cost = kInfinity;
    authored.absolute_optimality_gap = kInfinity;
    authored.relative_optimality_gap = kInfinity;
    authored.options = options;
    const std::uint32_t state_count = calc.state_count();
    authored.values.assign(state_count, kInfinity);
    authored.policy.assign(state_count, PolicyOperatorRef{});
    authored.expanded.assign(state_count, 1);
    authored.goal_states.assign(state_count, 0);
    authored.policy_reachable.assign(state_count, 1);
    authored.unveil_preferences.resize(state_count);
    authored.option_unveil_preferences.resize(state_count);
    for (std::uint32_t state = 0; state < state_count; ++state) {
        authored.goal_states[state] =
            calc.is_goal_state(calc.state(state)) ? 1 : 0;
    }
    for (const auto& [state, action] : selected_by_state) {
        authored.policy[state] = PolicyOperatorRef{
            PlannerOperatorKind::Primitive, action};
    }
    authored.diagnostics.policy_refinement.triggers = 1;
    authored.diagnostics.policy_refinement.status = "triggered";
    authored.diagnostics.policy_refinement
        .trigger_coarse_states.push_back(triggered_rare_parent);
    authored.diagnostics.policy_compatibility_state =
        triggered_rare_parent;

    const std::unordered_map<std::string, double> prices{
        {"transmute", 0.01},
        {"regal", 0.01},
        {"chaos", 1.0},
        {"exalt", 0.1}};
    const refinement::PolicyExactLiftCertificate lifted =
        refinement::lift_policy_exact(
            calc, authored, start, prices, options,
            "candidate-induced exact subclass");
    report_lift_failure("candidate-induced subclass", lifted);
    PC_CHECK(
        lifted.status ==
        refinement::PolicyExactLiftStatus::Complete);
    PC_CHECK(lifted.executable);
    PC_CHECK(lifted.compiled.executable);
    PC_CHECK(lifted.compiled.cost_reconciled);
    PC_CHECK(lifted.policy_changed);
    PC_CHECK(lifted.adapter.local_reoptimizations > 0);
    PC_CHECK(
        lifted.adapter.local_state_action_rows_scheduled > 0);
    PC_CHECK(
        lifted.adapter.local_state_action_rows_evaluated > 0);
    PC_CHECK(lifted.adapter.local_policy_changes > 0);
    PC_CHECK(lifted.adapter.local_value_changes > 0);

    std::set<std::uint32_t> target_actions;
    for (const refinement::RefinedPolicyClass& policy_class :
         lifted.refinement.classes) {
        if (policy_class.coarse_state == triggered_rare_parent &&
            policy_class.selected_action.has_value()) {
            target_actions.insert(
                policy_class.selected_action->action_id);
        }
    }
    /* Transmute and Regal are illegal on this rare parent, so the only
     * possible two admitted decisions are the incumbent Chaos and Exalt. */
    PC_CHECK(target_actions.size() > 1);
}

/*
 * The authored coarse row deliberately carries a stale Unveil preference:
 * its value assumes the goal option, while its sidecar selects a non-goal
 * option and then pays a bench finish. Exact refinement must treat the
 * preference as part of the selected decision, observe the value mismatch,
 * and install the single Bellman-greedy exact ordering for that carrier.
 */
void run_policy_guided_primitive_choice_reoptimization_tests() {
    auto session = make_solve_session();
    session->veiled_prefix_mod_id = 2;
    session->unveiled_generic_mask.assign(session->words, 0);
    pc_bitset_set(
        session->unveiled_generic_mask.data(), 0);
    pc_bitset_set(
        session->unveiled_generic_mask.data(), 3);
    pc_bitset_set(
        session->unveiled_generic_mask.data(), 4);
    pc_bitset_set(session->unveiled_mask.data(), 0);
    pc_bitset_set(session->unveiled_mask.data(), 3);
    pc_bitset_set(session->unveiled_mask.data(), 4);
    session->bench_mod_ids = {0};
    session->flags[0] |= 1u << 1;
    ActionRegistry registry = build_action_registry(*session);

    GoalSpec goal;
    goal.rarity = PC_RARITY_RARE;
    GoalSlot slot;
    slot.family_id = 100; /* unveiled prefix mod 0 */
    slot.min_tier = 1;
    goal.slots.push_back(slot);
    const std::uint32_t unveil =
        registry.index_by_id.at("unveil");
    const std::uint32_t bench =
        registry.index_by_id.at("bench:mod0");
    CalcContext calc(
        session, goal, registry, {unveil, bench},
        false, true, false, std::nullopt, {}, true);

    pc_item_state start;
    pc_item_clear(&start);
    start.rarity = PC_RARITY_RARE;
    PC_CHECK(
        pc_item_add_mod(
            &start, PC_SIDE_PREFIX, 2,
            static_cast<std::uint16_t>(
                session->primary_group[2]),
            PC_MOD_SLOT_VEILED, nullptr) ==
        PC_RESULT_OK);
    const std::uint32_t start_state =
        calc.intern_item(start);
    const OutcomeDistribution& offered =
        calc.outcomes(start_state, unveil, false);
    PC_CHECK(offered.supported);
    PC_CHECK(!offered.choice_groups.empty());
    PC_CHECK(offered.choice_options.size() == 3);

    std::map<std::uint32_t, std::uint32_t>
        successor_by_mod;
    for (const OutcomeChoiceOption& option :
         offered.choice_options) {
        successor_by_mod.emplace(
            option.mod_id, option.state);
    }
    PC_CHECK(successor_by_mod.contains(0));
    PC_CHECK(successor_by_mod.contains(3));
    PC_CHECK(successor_by_mod.contains(4));
    for (const auto& [mod, successor] :
         successor_by_mod) {
        (void)mod;
        if (!calc.is_goal_state(calc.state(successor))) {
            const OutcomeDistribution& finish =
                calc.outcomes(successor, bench, false);
            PC_CHECK(finish.supported);
            PC_CHECK(finish.entries.size() == 1);
            PC_CHECK(calc.is_goal_state(
                calc.state(finish.entries.front().state)));
        }
    }

    SolveOptions options;
    SolveResult authored;
    authored.converged = true;
    authored.policy_available = true;
    authored.policy_status = SolvePolicyStatus::Exact;
    authored.termination = SolveTermination::ExactClosed;
    authored.start_state = start_state;
    authored.has_exact_start_item = true;
    authored.exact_start_item = start;
    authored.lower_bound = 0.0;
    authored.upper_bound = 0.0;
    authored.evaluated_policy_cost = 0.0;
    authored.absolute_optimality_gap = 0.0;
    authored.relative_optimality_gap = 0.0;
    authored.options = options;
    const std::uint32_t state_count = calc.state_count();
    authored.values.assign(state_count, 0.0);
    authored.policy.assign(
        state_count, PolicyOperatorRef{});
    authored.expanded.assign(state_count, 1);
    authored.goal_states.assign(state_count, 0);
    authored.policy_reachable.assign(state_count, 1);
    authored.unveil_preferences.resize(state_count);
    authored.option_unveil_preferences.resize(state_count);
    authored.policy[start_state] = PolicyOperatorRef{
        PlannerOperatorKind::Primitive, unveil};
    authored.unveil_preferences[start_state] = {3, 0, 4};
    for (const auto& [unused_mod, successor] :
         successor_by_mod) {
        (void)unused_mod;
        if (calc.is_goal_state(calc.state(successor))) {
            authored.goal_states[successor] = 1;
            continue;
        }
        authored.values[successor] = 5.0;
        authored.policy[successor] = PolicyOperatorRef{
            PlannerOperatorKind::Primitive, bench};
        const OutcomeDistribution& finish =
            calc.outcomes(successor, bench, false);
        authored.goal_states[
            finish.entries.front().state] = 1;
    }

    const std::unordered_map<std::string, double> prices{
        {"bench:mod0", 5.0}};

    /*
     * A valid authored prefix need not enumerate strict-only offers absent
     * from the coarse sidecar.  Seed completion must retain mod0 first and
     * append every exact offer through the shared sparse-choice authority;
     * the resulting program remains executable without local repair.
     */
    SolveResult partial_sidecar = authored;
    partial_sidecar.unveil_preferences[start_state] = {0};
    const refinement::PolicyExactLiftCertificate completed_tail =
        refinement::lift_policy_exact(
            calc, partial_sidecar, start, prices, options,
            "focused primitive strict-only offer completion");
    report_lift_failure(
        "primitive strict-only offer completion", completed_tail);
    PC_CHECK(
        completed_tail.status ==
        refinement::PolicyExactLiftStatus::Complete);
    PC_CHECK(completed_tail.executable);
    PC_CHECK(completed_tail.lumpable);
    PC_CHECK(!completed_tail.policy_changed);
    PC_CHECK(completed_tail.coarse_value_reconciled);
    PC_CHECK(completed_tail.adapter.local_reoptimizations == 0);
    PC_CHECK(near(completed_tail.exact_start_cost, 0.0, 1e-12));
    PC_CHECK(completed_tail.compiled.executable);
    PC_CHECK(completed_tail.compiled.cost_reconciled);
    const std::string mod0_guard =
        "\"type\":\"has_unveil_option\",\"mod_key\":\"mod0\"";
    const std::string mod3_guard =
        "\"type\":\"has_unveil_option\",\"mod_key\":\"mod3\"";
    const std::string mod4_guard =
        "\"type\":\"has_unveil_option\",\"mod_key\":\"mod4\"";
    const std::size_t mod0_position =
        completed_tail.compiled.strategy_json.find(mod0_guard);
    const std::size_t mod3_position =
        completed_tail.compiled.strategy_json.find(mod3_guard);
    const std::size_t mod4_position =
        completed_tail.compiled.strategy_json.find(mod4_guard);
    PC_CHECK(mod0_position != std::string::npos);
    PC_CHECK(mod3_position != std::string::npos);
    PC_CHECK(mod4_position != std::string::npos);
    PC_CHECK(mod0_position < mod3_position);
    PC_CHECK(mod0_position < mod4_position);

    const refinement::PolicyExactLiftCertificate lifted =
        refinement::lift_policy_exact(
            calc, authored, start, prices, options,
            "focused primitive exact-choice reoptimization");
    report_lift_failure("primitive choice reoptimization", lifted);
    PC_CHECK(
        lifted.status ==
        refinement::PolicyExactLiftStatus::Complete);
    PC_CHECK(lifted.executable);
    PC_CHECK(lifted.lumpable);
    PC_CHECK(lifted.policy_changed);
    PC_CHECK(lifted.coarse_value_reconciled);
    PC_CHECK(
        lifted.adapter.local_reoptimizations > 0);
    PC_CHECK(near(lifted.exact_start_cost, 0.0, 1e-12));
    PC_CHECK(lifted.compiled.executable);
    PC_CHECK(lifted.compiled.cost_reconciled);

    /*
     * A primitive modifier id is one observed decision identity.  Poison the
     * coarse kernel so mod0 also names a distinct non-goal successor and
     * require seed admission to reject it before representative selection.
     */
    auto& poisoned = const_cast<OutcomeDistribution&>(
        calc.outcomes(start_state, unveil, false));
    const auto non_goal_offer = std::find_if(
        poisoned.choice_options.begin(),
        poisoned.choice_options.end(),
        [&](const OutcomeChoiceOption& option) {
            return option.mod_id != 0 &&
                   !calc.is_goal_state(calc.state(option.state));
        });
    PC_CHECK(non_goal_offer != poisoned.choice_options.end());
    PC_CHECK(
        non_goal_offer->state != successor_by_mod.at(0));
    OutcomeChoiceOption duplicate = *non_goal_offer;
    duplicate.mod_id = 0;
    poisoned.choice_options.push_back(duplicate);
    const refinement::PolicyExactLiftCertificate duplicate_rejected =
        refinement::lift_policy_exact(
            calc, partial_sidecar, start, prices, options,
            "focused primitive duplicate-mod rejection");
    PC_CHECK(
        duplicate_rejected.status ==
        refinement::PolicyExactLiftStatus::
            UnsupportedPrimitiveKernel);
    PC_CHECK(
        duplicate_rejected.failure_reason.find(
            "one primitive observed modifier has multiple semantic "
            "successors") != std::string::npos);
}

/*
 * Observed-choice primitives use their admitted semantic contract throughout
 * finalization and fixed-program runtime semantics.
 */
void run_future_observed_choice_finalization_tests() {
    auto session = make_solve_session();
    session->veiled_prefix_mod_id = 2;
    session->veiled_suffix_mod_id = 7;
    ActionRegistry registry = build_action_registry(*session);
    const std::uint32_t unveil =
        registry.index_by_id.at("unveil");
    ActionDescriptor future = registry.actions.at(unveil);
    future.id = "test:future_observed_modifier_offer";
    future.display_name = "future observed modifier offer";
    canonicalize_and_validate_action_refinement_contract(
        *session, future);
    PC_CHECK(action_observes_modifier_offer(future));

    std::vector<OutcomeChoiceOption> choices{
        {4, 0}, {3, 2}, {0, 1}};
    const std::vector<double> values{5.0, 1.0, 1.0};
    order_observed_modifier_choices(future, choices, values);
    PC_CHECK(choices.size() == 3);
    PC_CHECK(choices[0].mod_id == 0);
    PC_CHECK(choices[1].mod_id == 3);
    PC_CHECK(choices[2].mod_id == 4);

    std::vector<OutcomeChoiceOption> near_choices{
        {0, 2}, {3, 1}};
    const std::vector<double> near_values{
        0.0, 1.0, 1.0 + 5e-10};
    order_observed_modifier_choices(
        future, near_choices, near_values);
    PC_CHECK(near_choices.front().mod_id == 3);

    ActionDescriptor undeclared = future;
    undeclared.refinement.outcome_observation =
        RefinementOutcomeObservation::None;
    bool rejected_undeclared = false;
    try {
        order_observed_modifier_choices(
            undeclared, choices, values);
    } catch (const std::logic_error&) {
        rejected_undeclared = true;
    }
    PC_CHECK(rejected_undeclared);

    const std::uint32_t future_index =
        static_cast<std::uint32_t>(registry.actions.size());
    registry.index_by_id.emplace(future.id, future_index);
    registry.actions.push_back(std::move(future));
    PlannerOperator option;
    option.kind = PlannerOperatorKind::FixedOption;
    option.option_kind = FixedOptionKind::Renewal;
    option.primitive_program = {
        registry.index_by_id.at("veiled_chaos"), future_index};
    option.primitive_program_action_ids = {
        "veiled_chaos", "test:future_observed_modifier_offer"};
    const PlannerOperatorRuntimeSemantics runtime =
        planner_operator_runtime_semantics(option, registry);
    PC_CHECK(runtime.execution_paths.size() == 2);
    PC_CHECK(runtime.execution_paths[0].size() == 1);
    PC_CHECK(runtime.execution_paths[1].size() == 2);
    PC_CHECK(refinement_contract_observes_modifier_offer(
        runtime.compatibility_refinement));
}

/*
 * The fixed-option analogue starts from a valid solved renewal, then changes
 * only its observed-choice sidecar so every prefix offer rejects the goal and
 * retries. The resulting exact policy is improper. Local repair must evaluate
 * that retry continuation as infinite, retain finite goal continuations, and
 * restore one greedy recipe for the same admitted fixed operator.
 */
void run_policy_guided_fixed_choice_reoptimization_tests() {
    auto session = make_solve_session();
    session->veiled_prefix_mod_id = 2;
    session->veiled_suffix_mod_id = 7;
    pc_bitset_clear(
        session->normal_random_roll_mask.data(), 2);
    pc_bitset_clear(
        session->positive_spawn_weight_mask.data(), 2);
    pc_bitset_clear(
        session->positive_base_weight_mask.data(), 2);
    pc_bitset_clear(
        session->normal_random_roll_mask.data(), 7);
    pc_bitset_clear(
        session->positive_spawn_weight_mask.data(), 7);
    pc_bitset_clear(
        session->positive_base_weight_mask.data(), 7);
    /* Keep the synthetic renewal closed without adding a fallback operator.
     * Ordinary rolls may consume only group-10 prefix members and suffix
     * mod5; the remaining prefix Unveil offer contains goal mod3 and retry
     * mod4, while suffix offers remain valid retries. Clearing only the
     * normal-roll authority retains the actual Unveil offer vocabulary. */
    for (const std::uint32_t mod : {3u, 4u, 6u}) {
        pc_bitset_clear(
            session->normal_random_roll_mask.data(), mod);
    }
    session->unveiled_generic_mask.assign(session->words, 0);
    for (const std::uint32_t mod : {0u, 3u, 4u, 5u, 6u}) {
        pc_bitset_set(
            session->unveiled_generic_mask.data(), mod);
        pc_bitset_set(
            session->unveiled_mask.data(), mod);
    }
    ActionRegistry registry = build_action_registry(*session);

    GoalSpec goal;
    goal.rarity = PC_RARITY_RARE;
    GoalSlot slot;
    slot.family_id = 102; /* unveiled prefix mod 3 */
    slot.min_tier = 1;
    goal.slots.push_back(slot);
    FixedOptionSpec renewal;
    renewal.kind = FixedOptionKind::Renewal;
    renewal.program_action_ids = {
        "veiled_chaos", "unveil"};
    renewal.exit_goal_slots = {0};
    renewal.exit_min_satisfied = 1;
    goal.fixed_options.push_back(renewal);

    CalcContext calc(
        session, goal, registry, {}, false, false);
    pc_item_state start;
    pc_item_clear(&start);
    start.rarity = PC_RARITY_RARE;
    const std::uint32_t start_state = calc.intern_item(start);
    const std::uint32_t option_index =
        static_cast<std::uint32_t>(registry.actions.size());
    const OptionKernel& renewal_kernel =
        calc.option_kernel(start_state, option_index);
    PC_CHECK(renewal_kernel.legal);
    PC_CHECK(renewal_kernel.exits.empty());
    PC_CHECK(!renewal_kernel.observation_choice_groups.empty());
    bool mixed_goal_retry_offer = false;
    for (const OutcomeChoiceGroup& group :
         renewal_kernel.observation_choice_groups) {
        PC_CHECK(group.observation_state != kNoId);
        bool has_goal = false;
        bool has_retry = false;
        for (const std::uint32_t successor : group.states) {
            has_retry |= successor == kNoId;
            has_goal |= successor != kNoId &&
                        calc.is_goal_state(calc.state(successor));
            PC_CHECK(
                successor == kNoId ||
                calc.is_goal_state(calc.state(successor)));
        }
        mixed_goal_retry_offer |= has_goal && has_retry;
    }
    PC_CHECK(mixed_goal_retry_offer);
    const std::unordered_map<std::string, double> prices{
        {"veiled_chaos", 1.0}};
    SolveOptions options;
    const SolveResult solved =
        solve(calc, start, prices, options);
    report_solve_issue(
        "fixed choice reoptimization coarse solve",
        solved, true);
    PC_CHECK(solved.policy_available);
    PC_CHECK(solved.converged);
    const PolicyOperatorRef expected_option{
        PlannerOperatorKind::FixedOption,
        option_index};
    PC_CHECK(
        solved.policy[solved.start_state] ==
        expected_option);
    PC_CHECK(
        !solved.option_unveil_preferences[
             solved.start_state]
             .empty());

    SolveResult authored = solved;
    std::uint32_t goal_observations = 0;
    std::uint32_t rejected_goal_observations = 0;
    for (ObservedUnveilPreference& observation :
         authored.option_unveil_preferences[
             authored.start_state]) {
        const auto goal_choice = std::find_if(
            observation.choices.begin(),
            observation.choices.end(),
            [&](const ObservedUnveilChoice& choice) {
                return choice.successor_state <
                           calc.state_count() &&
                       calc.is_goal_state(
                           calc.state(
                               choice.successor_state));
            });
        if (goal_choice == observation.choices.end() ||
            observation.choices.size() < 2) {
            continue;
        }
        ++goal_observations;
        const auto retry_choice = std::find_if(
            observation.choices.begin(),
            observation.choices.end(),
            [&](const ObservedUnveilChoice& choice) {
                return choice.successor_state <
                           calc.state_count() &&
                       !calc.is_goal_state(
                           calc.state(
                               choice.successor_state));
            });
        if (retry_choice ==
            observation.choices.end()) {
            continue;
        }
        std::iter_swap(
            observation.choices.begin(),
            retry_choice);
        ++rejected_goal_observations;
    }
    PC_CHECK(goal_observations > 0);
    PC_CHECK(rejected_goal_observations == goal_observations);

    const refinement::PolicyExactLiftCertificate lifted =
        refinement::lift_policy_exact(
            calc, authored, start, prices, options,
            "focused fixed exact-choice reoptimization");
    report_lift_failure("fixed choice reoptimization", lifted);
    PC_CHECK(
        lifted.status ==
        refinement::PolicyExactLiftStatus::Complete);
    PC_CHECK(lifted.executable);
    PC_CHECK(lifted.lumpable);
    PC_CHECK(lifted.policy_changed);
    PC_CHECK(lifted.coarse_value_reconciled);
    PC_CHECK(
        lifted.adapter.local_reoptimizations > 0);
    for (const refinement::RefinedPolicyClass& policy_class :
         lifted.refinement.classes) {
        if (!policy_class.terminal &&
            policy_class.selected_action.has_value()) {
            PC_CHECK(
                policy_class.selected_action->action_id ==
                option_index);
        }
    }
    PC_CHECK(near(
        lifted.exact_start_cost,
        solved.evaluated_policy_cost, 1e-8));
    PC_CHECK(lifted.compiled.executable);
    PC_CHECK(lifted.compiled.cost_reconciled);
}

/*
 * Transmute and Scour form a legal two-carrier bottom SCC when only one
 * explicit can roll. The first canonical member's only alternative is an
 * equivalent Transmute, so its first transaction leaves the SCC closed.
 * Recursive repair must retain that mutation and replace Scour on the magic
 * member with a deterministic bench finish. This is a properness
 * counterexample, not an illegal-action counterexample.
 */
void run_policy_guided_improper_cycle_repair_tests() {
    auto session = make_solve_session();
    session->base_spawn_weight.assign(session->mod_count, 0);
    session->base_roll_weight.assign(session->mod_count, 0);
    session->base_spawn_weight[0] = 100;
    session->base_roll_weight[0] = 100;
    pc_bitset_zero(
        session->normal_random_roll_mask.data(),
        session->words);
    pc_bitset_zero(
        session->positive_spawn_weight_mask.data(),
        session->words);
    pc_bitset_zero(
        session->positive_base_weight_mask.data(),
        session->words);
    pc_bitset_set(session->normal_random_roll_mask.data(), 0);
    pc_bitset_set(session->positive_spawn_weight_mask.data(), 0);
    pc_bitset_set(session->positive_base_weight_mask.data(), 0);
    session->bench_mod_ids = {5};
    session->flags[5] |= 1u << 1;
    ActionRegistry registry = build_action_registry(*session);

    GoalSpec goal;
    goal.rarity = PC_RARITY_MAGIC;
    GoalSlot slot;
    slot.family_id = 104;
    slot.min_tier = 1;
    goal.slots.push_back(slot);

    const std::uint32_t transmute =
        registry.index_by_id.at("transmute");
    const std::uint32_t scour =
        registry.index_by_id.at("scour");
    const std::uint32_t bench =
        registry.index_by_id.at("bench:mod5");
    ActionDescriptor alternate_transmute =
        registry.actions.at(transmute);
    alternate_transmute.id = "transmute:transaction-control";
    alternate_transmute.display_name =
        "Transmute transaction control";
    alternate_transmute.cost_keys = {
        alternate_transmute.id};
    const std::uint32_t alternate_transmute_index =
        static_cast<std::uint32_t>(registry.actions.size());
    registry.index_by_id.emplace(
        alternate_transmute.id, alternate_transmute_index);
    registry.actions.push_back(
        std::move(alternate_transmute));
    CalcContext calc(
        session, goal, registry,
        {transmute, scour, bench, alternate_transmute_index},
        false, true, false, std::nullopt, {}, true);

    pc_item_state start;
    pc_item_clear(&start);
    start.rarity = PC_RARITY_NORMAL;
    const std::uint32_t start_state = calc.intern_item(start);
    const OutcomeDistribution& transmute_outcomes =
        calc.outcomes(start_state, transmute, false);
    PC_CHECK(transmute_outcomes.supported);
    PC_CHECK(transmute_outcomes.choice_groups.empty());
    PC_CHECK(transmute_outcomes.entries.size() == 1);
    PC_CHECK(
        near(
            transmute_outcomes.entries.front().probability,
            1.0, 1e-12));
    const std::uint32_t magic_state =
        transmute_outcomes.entries.front().state;
    PC_CHECK(magic_state != start_state);
    PC_CHECK(
        calc.state(magic_state).rarity == PC_RARITY_MAGIC);
    const OutcomeDistribution& scour_outcomes =
        calc.outcomes(magic_state, scour, false);
    PC_CHECK(scour_outcomes.supported);
    PC_CHECK(scour_outcomes.choice_groups.empty());
    PC_CHECK(scour_outcomes.entries.size() == 1);
    PC_CHECK(near(
        scour_outcomes.entries.front().probability, 1.0, 1e-12));
    PC_CHECK(
        scour_outcomes.entries.front().state == start_state);
    const OutcomeDistribution& bench_outcomes =
        calc.outcomes(magic_state, bench, false);
    PC_CHECK(bench_outcomes.supported);
    PC_CHECK(bench_outcomes.choice_groups.empty());
    PC_CHECK(bench_outcomes.entries.size() == 1);
    PC_CHECK(near(
        bench_outcomes.entries.front().probability, 1.0, 1e-12));
    PC_CHECK(calc.is_goal_state(
        calc.state(bench_outcomes.entries.front().state)));

    SolveOptions options;
    SolveResult authored;
    authored.converged = true;
    authored.policy_available = true;
    authored.policy_status = SolvePolicyStatus::Exact;
    authored.termination = SolveTermination::ExactClosed;
    authored.start_state = start_state;
    authored.has_exact_start_item = true;
    authored.exact_start_item = start;
    authored.lower_bound = 0.0;
    authored.upper_bound = 0.0;
    authored.evaluated_policy_cost = 0.0;
    authored.absolute_optimality_gap = 0.0;
    authored.relative_optimality_gap = 0.0;
    authored.options = options;
    authored.values.assign(calc.state_count(), 0.0);
    authored.policy.assign(
        calc.state_count(), PolicyOperatorRef{});
    authored.expanded.assign(calc.state_count(), 1);
    authored.goal_states.assign(calc.state_count(), 0);
    authored.policy_reachable.assign(calc.state_count(), 1);
    authored.unveil_preferences.resize(calc.state_count());
    authored.option_unveil_preferences.resize(calc.state_count());
    authored.policy[start_state] = PolicyOperatorRef{
        PlannerOperatorKind::Primitive, transmute};
    authored.policy[magic_state] = PolicyOperatorRef{
        PlannerOperatorKind::Primitive, scour};

    const std::unordered_map<std::string, double> prices{
        {"transmute", 0.5},
        {"scour", 0.25},
        {"bench:mod5", 2.0},
        {"transmute:transaction-control", 0.5}};
    const refinement::PolicyExactLiftCertificate lifted =
        refinement::lift_policy_exact(
            calc, authored, start, prices, options,
            "focused improper-cycle exact repair");
    report_lift_failure("improper-cycle repair", lifted);
    PC_CHECK(
        lifted.status ==
        refinement::PolicyExactLiftStatus::Complete);
    PC_CHECK(lifted.executable);
    PC_CHECK(lifted.lumpable);
    PC_CHECK(lifted.policy_changed);
    PC_CHECK(lifted.adapter.local_reoptimizations >= 2);
    PC_CHECK(
        lifted.adapter.local_reoptimization_rounds > 0);
    PC_CHECK(
        lifted.class_evaluation.status ==
        refinement::PolicyEvaluationStatus::Complete);
    PC_CHECK(lifted.class_evaluation.proper);
    PC_CHECK(lifted.compiled.executable);
    PC_CHECK(lifted.compiled.proper);
    PC_CHECK(lifted.compiled.cost_reconciled);
    PC_CHECK(near(lifted.exact_start_cost, 2.5, 1e-9));
}

/* A crafted-only goal has an exact direct finish. Chaos cannot produce the
 * target mod after it is removed from the normal-roll mask, so every Chaos
 * route must still pay the bench cost later. The constructive certificate
 * may therefore stop before materializing Chaos outcomes, while the
 * certificate-disabled oracle must retain the same value and policy. */
void run_constructive_state_certificate_tests() {
    auto session = make_solve_session();
    pc_bitset_clear(session->normal_random_roll_mask.data(), 0);
    session->bench_mod_ids = {0};
    session->flags[0] |= 1u << 1;
    ActionRegistry registry = build_action_registry(*session);
    const std::uint32_t bench_index =
        registry.index_by_id.at("bench:mod0");

    GoalSpec goal;
    GoalSlot slot;
    slot.family_id = 100;
    slot.min_tier = 1;
    goal.slots.push_back(slot);
    goal.rarity = PC_RARITY_RARE;
    const std::uint32_t chaos = registry.index_by_id.at("chaos");
    const std::uint32_t restart = registry.index_by_id.at("restart");
    CalcContext calc(
        session, goal, registry, {chaos, restart, bench_index});

    pc_item_state start;
    pc_item_clear(&start);
    start.rarity = PC_RARITY_RARE;
    const std::unordered_map<std::string, double> prices{
        {"chaos", 0.5}, {"base", 1.0}, {"bench:mod0", 3.0}};

    const SolveResult certified = solve(calc, start, prices);
    PC_CHECK(certified.converged);
    PC_CHECK(near(certified.values[certified.start_state], 3.0, 1e-9));
    PC_CHECK(certified.policy[certified.start_state] == bench_index);
    PC_CHECK(certified.diagnostics.constructive_state_certificates == 1);
    PC_CHECK(certified.diagnostics.constructive_state_operators_pruned == 2);
    PC_CHECK(near(
        certified.diagnostics.constructive_upper_bound, 3.0, 1e-9));
    PC_CHECK(certified.diagnostics.constructive_upper_first_expanded_state ==
             1);
    PC_CHECK(!certified.diagnostics.constructive_state_witnesses.empty());
    PC_CHECK(certified.diagnostics.discovered_states == 2);
    PC_CHECK(!certified.diagnostics.transition_cache_reused);
    const std::string telemetry = serialize_solver_telemetry(
        calc, &certified, nullptr, std::nullopt, nullptr);
    PC_CHECK(valid_json_object(telemetry));
    PC_CHECK(telemetry.find(
                 "\"constructive_state_certificates\":{\"accepted\":1") !=
             std::string::npos);
    PC_CHECK(telemetry.find(
                 "\"proof\":\"optimistic_goal_production_cover\"") !=
             std::string::npos);

    /* A price-bound partial graph is intentionally not a reprice cache. The
     * next solve rebuilds exact rows instead of inheriting a stale proof. */
    const SolveResult repeated = solve(calc, start, prices);
    PC_CHECK(repeated.converged);
    PC_CHECK(!repeated.diagnostics.transition_cache_reused);
    PC_CHECK(near(repeated.values[repeated.start_state], 3.0, 1e-9));

    SolveOptions oracle_options;
    oracle_options.state_certificate_control = false;
    const SolveResult oracle = solve(calc, start, prices, oracle_options);
    PC_CHECK(oracle.converged);
    PC_CHECK(near(
        oracle.values[oracle.start_state],
        certified.values[certified.start_state], 1e-9));
    PC_CHECK(oracle.policy[oracle.start_state] ==
             certified.policy[certified.start_state]);
    PC_CHECK(oracle.diagnostics.constructive_state_certificates == 0);
    PC_CHECK(oracle.diagnostics.discovered_states >
             certified.diagnostics.discovered_states);
}

/* An automatic renewal may establish a finite executable policy before its
 * successful exit states receive complete all-action expansions. The upper
 * evaluator is allowed to inspect an admitted deterministic finish kernel
 * directly; the lower proof still retains every admitted action. */
void run_constructive_renewal_upper_tests() {
    auto session = make_solve_session();
    session->metamod_type.assign(8, -10);
    pc_bitset_clear(session->normal_random_roll_mask.data(), 5);
    session->flags[5] |= 1u << 1;

    ActionRegistry registry = build_action_registry(*session);
    session->bench_mod_ids = {5};
    ActionDescriptor bench;
    bench.id = "bench:constructive_finish";
    bench.display_name = "Bench constructive finish";
    bench.params.type = ActionType::Bench;
    bench.params.mod_id = 5;
    bench.kind = TransitionKind::Deterministic;
    bench.cost_keys = {bench.id};
    bench.legality.rarity_mask = 1u << PC_RARITY_RARE;
    bench.legality.requires_open_affix = true;
    bench.sets_flags = kFlagCraftedMod;
    bench.refinement =
        derive_action_refinement_contract(*session, bench);
    validate_action_refinement_contract(bench);
    const std::uint32_t bench_index =
        static_cast<std::uint32_t>(registry.actions.size());
    registry.index_by_id.emplace(bench.id, bench_index);
    registry.actions.push_back(std::move(bench));

    GoalSpec goal;
    goal.rarity = PC_RARITY_RARE;
    goal.automatic_candidates = true;
    for (const std::uint32_t family : {100u, 102u, 104u}) {
        GoalSlot slot;
        slot.family_id = family;
        slot.min_tier = 1;
        goal.slots.push_back(slot);
    }
    const std::uint32_t chaos = registry.index_by_id.at("chaos");
    const std::uint32_t restart = registry.index_by_id.at("restart");
    CalcContext calc(
        session, goal, registry, {chaos, restart, bench_index});
    PC_CHECK(std::any_of(
        calc.operators().begin(), calc.operators().end(),
        [](const PlannerOperator& planner) {
            return planner.automatic_kind ==
                   AutomaticCandidateKind::ConstructiveRenewal;
        }));

    pc_item_state start;
    pc_item_clear(&start);
    start.rarity = PC_RARITY_RARE;
    SolveOptions options;
    options.max_expanded_states = 2;
    options.state_certificate_control = false;
    options.goal_progress_gated_reforges = true;
    options.focused_expansion_checkpoint = 1;
    options.focused_expansion_queue_threshold = 0;
    const std::unordered_map<std::string, double> prices{
        {"chaos", 1.0},
        {"base", 1.0},
        {"bench:constructive_finish", 2.0}};
    SolveWork work(calc, start, prices, options);
    SolveProgress progress = work.progress();
    std::uint32_t work_units = 0;
    while (!progress.done &&
           progress.expanded_states < options.max_expanded_states &&
           work_units < 10000) {
        work.step(1);
        progress = work.progress();
        ++work_units;
    }
    while (!progress.done && work_units < 10000) {
        work.step(1);
        progress = work.progress();
        ++work_units;
    }
    PC_CHECK(progress.expanded_states == options.max_expanded_states);
    PC_CHECK(progress.done);
    if (!progress.done) return;
    const SolveResult result = work.finish();
    PC_CHECK(result.diagnostics.focused_expansion);
    PC_CHECK(result.diagnostics.state_cap_hit);
    PC_CHECK(std::isfinite(result.diagnostics.focused_upper_bound));
    PC_CHECK(result.diagnostics.focused_upper_bound > 2.0);
    PC_CHECK(result.diagnostics.focused_lower_bound <=
             result.diagnostics.focused_upper_bound + 1e-9);
}

/* A genuine naturally rolled goal has no deterministic finish. A primitive
 * destructive roll is still an executable renewal when every miss is legal
 * and reproduces the exact engine-owned preserved-base signature. Restart's
 * fresh normal carrier must pay for a real rare setup before joining it. */
void run_primitive_destructive_renewal_upper_tests() {
    auto session = make_solve_session();
    ActionRegistry registry = build_action_registry(*session);
    GoalSpec goal;
    goal.rarity = PC_RARITY_RARE;
    goal.automatic_candidates = true;
    for (const std::uint32_t family : {100u, 102u, 104u}) {
        GoalSlot slot;
        slot.family_id = family;
        slot.min_tier = 1;
        goal.slots.push_back(slot);
    }
    const std::uint32_t alchemy = registry.index_by_id.at("alchemy");
    const std::uint32_t chaos = registry.index_by_id.at("chaos");
    const std::uint32_t restart = registry.index_by_id.at("restart");
    CalcContext calc(
        session, goal, registry, {alchemy, chaos, restart});
    pc_item_state start;
    pc_item_clear(&start);
    start.rarity = PC_RARITY_RARE;
    const std::uint32_t start_state = calc.intern_item(start);
    const OutcomeDistribution& kernel = calc.outcomes(start_state, chaos);
    PC_CHECK(kernel.supported);
    PC_CHECK(kernel.stable_shared_kernel);
    double success_probability = 0.0;
    std::uint32_t retry_state = kNoId;
    for (const OutcomeEntry& outcome : kernel.entries) {
        if (calc.is_goal_state(calc.state(outcome.state))) {
            success_probability += outcome.probability;
        } else if (retry_state == kNoId) {
            retry_state = outcome.state;
        }
    }
    PC_CHECK(success_probability > 0.0);
    PC_CHECK(retry_state != kNoId);
    std::vector<std::uint64_t> start_signature;
    std::vector<std::uint64_t> retry_signature;
    PC_CHECK(calc.exact_reforge_kernel_signature(
        start_state, chaos, start_signature));
    PC_CHECK(calc.exact_reforge_kernel_signature(
        retry_state, chaos, retry_signature));
    PC_CHECK(start_signature == retry_signature);

    /*
     * Ordinary zero-progress carriers remain strict states, but disposable
     * explicit junk is invisible to the exact same-carrier Chaos renewal
     * authority. Both rows therefore use the same renewal signature and the
     * same virtual retry representative without becoming Restart.
     */
    pc_item_state junk_left = start;
    pc_item_state junk_right = start;
    place(&junk_left, PC_SIDE_PREFIX, 2, 10);
    place(&junk_right, PC_SIDE_SUFFIX, 6, 21);
    const std::uint32_t junk_left_state =
        calc.intern_item(junk_left);
    const std::uint32_t junk_right_state =
        calc.intern_item(junk_right);
    PC_CHECK(junk_left_state != junk_right_state);
    std::vector<std::uint64_t> junk_left_signature;
    std::vector<std::uint64_t> junk_right_signature;
    PC_CHECK(calc.exact_reforge_kernel_signature(
        junk_left_state, chaos, junk_left_signature));
    PC_CHECK(calc.exact_reforge_kernel_signature(
        junk_right_state, chaos, junk_right_signature));
    PC_CHECK(junk_left_signature == start_signature);
    PC_CHECK(junk_right_signature == start_signature);
    const OutcomeDistribution& gated_junk_left =
        calc.outcomes(junk_left_state, chaos, true);
    const OutcomeDistribution& gated_junk_right =
        calc.outcomes(junk_right_state, chaos, true);
    PC_CHECK(gated_junk_left.gated_retry_state != kNoId);
    PC_CHECK(
        gated_junk_left.gated_retry_state ==
        gated_junk_right.gated_retry_state);
    PC_CHECK(
        calc.state(gated_junk_left.gated_retry_state)
                .goal_progress_retry_basin != 0);

    pc_item_state fractured = start;
    place(&fractured, PC_SIDE_PREFIX, 3, 12);
    fractured.prefixes[0].flags |= PC_MOD_SLOT_FRACTURED;
    std::vector<std::uint64_t> fractured_signature;
    PC_CHECK(calc.exact_reforge_kernel_signature(
        calc.intern_item(fractured), chaos, fractured_signature));
    PC_CHECK(fractured_signature != start_signature);

    /*
     * Persistent carrier features never enter the retry basin merely because
     * they currently satisfy no goal. A crafted affix also remains observable
     * by the retained non-renewal cleanup action even when Chaos would discard
     * it, while Fracture, protection, influence, corruption, and Eldritch
     * state retain distinct strict identities.
     */
    auto feature_session = make_solve_session();
    const std::shared_ptr<DataImpl> feature_data =
        std::const_pointer_cast<DataImpl>(feature_session->data);
    feature_data->metamod_prefixes_locked_code = 3;
    feature_session->metamod_type[4] =
        feature_data->metamod_prefixes_locked_code;
    ActionRegistry feature_registry =
        build_action_registry(*feature_session);
    const std::uint32_t feature_chaos =
        feature_registry.index_by_id.at("chaos");
    const std::uint32_t remove_crafted =
        feature_registry.index_by_id.at(
            "remove_crafted_modifiers");
    CalcContext feature_calc(
        feature_session, goal, feature_registry,
        {feature_chaos, remove_crafted});
    pc_item_state feature_clean = start;
    pc_item_state feature_crafted = start;
    place(&feature_crafted, PC_SIDE_PREFIX, 2, 10);
    feature_crafted.prefixes[0].flags |= PC_MOD_SLOT_CRAFTED;
    pc_item_state feature_protected = start;
    place(&feature_protected, PC_SIDE_PREFIX, 4, 13);
    feature_protected.prefixes[0].flags |= PC_MOD_SLOT_CRAFTED;
    pc_item_state feature_influenced = start;
    feature_influenced.generic_influence_bits = 1;
    pc_item_state feature_corrupted = start;
    feature_corrupted.item_flags = PC_ITEM_CORRUPTED;
    pc_item_state feature_eldritch = start;
    feature_eldritch.searing_exarch_tier = 2;
    feature_eldritch.eater_of_worlds_tier = 1;
    const std::array<pc_item_state, 6> feature_items{
        feature_clean, feature_crafted, feature_protected,
        feature_influenced, feature_corrupted, feature_eldritch};
    std::array<std::uint32_t, 6> feature_states{};
    for (std::size_t i = 0; i < feature_items.size(); ++i) {
        feature_states[i] =
            feature_calc.intern_item(feature_items[i]);
        PC_CHECK(
            feature_calc.state(feature_states[i])
                    .goal_progress_retry_basin == 0);
        if (i != 0) {
            PC_CHECK(feature_states[i] != feature_states[0]);
        }
    }
    PC_CHECK(
        (feature_calc.state(feature_states[1]).flags &
         kFlagCraftedMod) != 0);
    PC_CHECK(
        (feature_calc.state(feature_states[2]).flags &
         kProtectionFlags) != 0);
    PC_CHECK(
        (feature_calc.state(feature_states[3]).flags &
         kFlagInfluenced) != 0);
    PC_CHECK(
        (feature_calc.state(feature_states[4]).flags &
         kFlagCorrupted) != 0);
    PC_CHECK(
        (feature_calc.state(feature_states[5]).flags &
         kFlagEldritchImplicit) != 0);
    PC_CHECK(action_legal(
        *feature_session,
        feature_registry.actions.at(remove_crafted),
        feature_calc.state(feature_states[1])));
    PC_CHECK(!action_legal(
        *feature_session,
        feature_registry.actions.at(remove_crafted),
        feature_calc.state(feature_states[0])));

    /*
     * One fractured junk prefix makes a three-prefix goal infeasible under
     * every same-carrier renewal in this toy. Restart is the executable
     * dead-carrier fallback; it is not the live retry representative.
     */
    GoalSpec dead_goal;
    dead_goal.rarity = PC_RARITY_RARE;
    for (const std::uint32_t family : {100u, 102u, 103u}) {
        GoalSlot slot;
        slot.family_id = family;
        slot.min_tier = 1;
        dead_goal.slots.push_back(slot);
    }
    CalcContext dead_calc(
        session, dead_goal, registry, {alchemy, chaos, restart});
    pc_item_state dead_carrier = start;
    place(&dead_carrier, PC_SIDE_PREFIX, 2, 10);
    dead_carrier.prefixes[0].flags |= PC_MOD_SLOT_FRACTURED;
    const SolveResult dead_result = solve(
        dead_calc, dead_carrier,
        {{"alchemy", 0.5}, {"chaos", 1.0}, {"base", 1.0}});
    PC_CHECK(dead_result.converged);
    PC_CHECK(
        dead_result.policy[dead_result.start_state].index ==
        restart);

    SolveOptions options;
    options.max_expanded_states = 2;
    options.state_certificate_control = false;
    options.focused_expansion_checkpoint = 1;
    options.focused_expansion_queue_threshold = 0;
    SolveOptions bounded_options = options;
    bounded_options.goal_progress_gated_reforges = true;
    const std::unordered_map<std::string, double> prices{
        {"alchemy", 0.5}, {"chaos", 1.0}, {"base", 1.0}};
    const SolveResult result = solve(
        calc, start, prices, bounded_options);
    const double direct_renewal = 1.0 / success_probability;
    PC_CHECK(result.diagnostics.focused_expansion);
    PC_CHECK(result.diagnostics.state_cap_hit);
    PC_CHECK(result.policy_available);
    PC_CHECK(result.policy_status ==
             SolvePolicyStatus::BoundedFeasible);
    PC_CHECK(result.primitive_renewal_witness.valid);
    PC_CHECK(result.termination ==
             SolveTermination::RefusedResourceCap);
    PC_CHECK(result.lower_bound <= result.evaluated_policy_cost + 1e-9);
    PC_CHECK(result.evaluated_policy_cost <= result.upper_bound + 1e-9);
    PC_CHECK(near(
        result.evaluated_policy_cost,
        result.diagnostics.focused_upper_bound, 1e-8));
    for (std::uint32_t state = 0;
         state < result.policy_reachable.size(); ++state) {
        if (result.policy_reachable[state] && !result.goal_states[state]) {
            PC_CHECK(result.policy[state] != kNoId);
        }
    }
    PC_CHECK(std::isfinite(result.diagnostics.focused_upper_bound));
    PC_CHECK(near(
        result.diagnostics.focused_upper_bound, direct_renewal, 1e-8));
    PC_CHECK(result.diagnostics.focused_lower_bound <=
             result.diagnostics.focused_upper_bound + 1e-9);
    PC_CHECK(result.diagnostics.supported_priced_actions == 3);
    PC_CHECK(result.diagnostics.gated_root_renewal_candidates > 0);
    PC_CHECK(
        result.diagnostics.gated_root_renewal_validated_non_goal_states >
        0);
    PC_CHECK(
        result.diagnostics.destructive_renewal_action_id == "chaos");

    /* The same uncapped toy policy must close and lift to ordinary primitive
     * strategy behavior. This keeps the bounded fallback witness aligned with
     * the normal compiler path used once exact closure is proved. */
    CalcContext compile_calc(
        session, goal, registry, {alchemy, chaos, restart});
    const SolveResult compiled_policy = solve(
        compile_calc, start, prices);
    PC_CHECK(compiled_policy.converged);
    if (compiled_policy.converged) {
        PolicyCompilationTelemetry compilation;
        const std::string strategy_json = compile_policy_strategy_json(
            compile_calc, compiled_policy,
            "primitive destructive renewal", &compilation);
        PC_CHECK(strategy_json.find("\"type\":\"chaos\"") !=
                 std::string::npos);
        PC_CHECK(compilation.nodes > 0);
        PC_CHECK(compilation.edges > 0);
        PC_CHECK(compilation.working_states > 0);
    }

    {
        PolicyCompilationTelemetry compilation;
        const std::string strategy_json = compile_policy_strategy_json(
            calc, result, "bounded primitive destructive renewal",
            &compilation);
        const std::shared_ptr<StrategyImpl> strategy =
            compile_strategy_json(
                session, strategy_json.data(), strategy_json.size());
        auto economy = std::make_shared<EconomyImpl>();
        economy->id = "bounded-policy-test";
        economy->prices = prices;
        StrategyEvalOptions evaluation_options;
        evaluation_options.economy = economy;
        const StrategyEvalResult evaluation = evaluate_strategy(
            *strategy, evaluation_options);
        PC_CHECK(evaluation.converged);
        PC_CHECK(evaluation.cost_complete);
        PC_CHECK(near(
            evaluation.total_expected_cost,
            result.evaluated_policy_cost, 1e-7));
        PC_CHECK(!evaluation.occupancy_states.empty());
        PC_CHECK(!evaluation.occupancy.empty());
        PC_CHECK(evaluation.occupancy_reward_complete);
        double retained_reward = 0.0;
        for (const StrategyEvalOccupancyEntry& entry :
             evaluation.occupancy) {
            PC_CHECK(entry.state < evaluation.occupancy_states.size());
            PC_CHECK(entry.action != kNoId);
            PC_CHECK(entry.reward_complete);
            retained_reward +=
                entry.expected_applied * entry.immediate_reward;
        }
        PC_CHECK(near(
            retained_reward, evaluation.total_expected_cost, 1e-7));
        PC_CHECK(near(
            evaluation.occupancy_expected_reward,
            evaluation.total_expected_cost, 1e-7));
        PC_CHECK(near(
            evaluation.occupancy_reward_difference, 0.0, 1e-7));
    }

    {
        /* Once certified, the retained artifact is the compile result. A
         * later mutation of source vocabulary must not reconstruct a
         * different strategy or erase the already-proven witness. */
        const std::shared_ptr<DataImpl> mutable_data =
            std::const_pointer_cast<DataImpl>(session->data);
        const std::string saved_key = mutable_data->strings.at(2);
        mutable_data->strings.at(2).clear();
        const std::string retained_after_mutation =
            compile_policy_strategy_json(
                calc, result, "inexpressible bounded policy");
        mutable_data->strings.at(2) = saved_key;
        PC_CHECK(
            retained_after_mutation ==
            result.refined_policy_artifact.strategy_json);
        PC_CHECK(result.policy_status ==
                 SolvePolicyStatus::BoundedFeasible);
        PC_CHECK(result.policy_available);
        PC_CHECK(std::isfinite(result.upper_bound));
    }

    SolveOptions target_options = bounded_options;
    target_options.max_expanded_states = 1000;
    target_options.max_absolute_optimality_gap = 1e9;
    const SolveResult target = solve(
        calc, start, prices, target_options);
    PC_CHECK(target.policy_available);
    if (target.converged) {
        PC_CHECK(target.policy_status == SolvePolicyStatus::Exact);
        PC_CHECK(target.termination == SolveTermination::ExactClosed);
        PC_CHECK(!target.target_met);
        PC_CHECK(target.target_fired == SolveGapTarget::None);
        PC_CHECK(
            !target.refined_policy_artifact.strategy_json.empty());
    } else {
        /* A bounded equality without global lower closure is normalized to
         * lower zero, so it cannot retain a pre-normalization gap claim. */
        PC_CHECK(target.policy_status ==
                 SolvePolicyStatus::BoundedFeasible);
        /* Termination preserves the coarse event that stopped discovery;
         * target metadata describes the normalized final certificate. */
        PC_CHECK(target.termination == SolveTermination::TargetGap);
        PC_CHECK(!target.target_met);
        PC_CHECK(target.target_fired == SolveGapTarget::None);
        PC_CHECK(target.lower_bound == 0.0);
    }
    PC_CHECK(!target.diagnostics.state_cap_hit);
    /* Preparation-only lower refreshes can increment expansion rounds before
     * any complete upper pass exists. The target must fire on the first
     * completed lower/upper certificate, counted here. */
    PC_CHECK(target.diagnostics.focused_partial_policy_rounds == 1);
    PC_CHECK(target.diagnostics.supported_priced_actions ==
             result.diagnostics.supported_priced_actions);
    PC_CHECK(target.absolute_optimality_gap <=
             target_options.max_absolute_optimality_gap);

    SolveOptions unmet_options = bounded_options;
    unmet_options.max_absolute_optimality_gap = 1e-30;
    const SolveResult unmet = solve(
        calc, start, prices, unmet_options);
    PC_CHECK(unmet.policy_available);
    PC_CHECK(unmet.policy_status == SolvePolicyStatus::BoundedFeasible);
    PC_CHECK(unmet.termination == SolveTermination::RefusedResourceCap);
    PC_CHECK(!unmet.target_met);
    PC_CHECK(unmet.target_fired == SolveGapTarget::None);

    auto doubled = prices;
    for (auto& [unused, price] : doubled) {
        (void)unused;
        price *= 2.0;
    }
    const SolveResult repriced = solve(
        calc, start, doubled, bounded_options);
    PC_CHECK(std::isfinite(repriced.diagnostics.focused_upper_bound));
    PC_CHECK(near(
        repriced.diagnostics.focused_upper_bound,
        2.0 * result.diagnostics.focused_upper_bound, 1e-7));

    ActionRegistry illegal_registry = registry;
    illegal_registry.actions.at(chaos).legality.requires_open_affix = true;
    CalcContext illegal_calc(
        session, goal, std::move(illegal_registry),
        {alchemy, chaos, restart});
    const SolveResult illegal = solve(
        illegal_calc, start, prices, bounded_options);
    PC_CHECK(!illegal.policy_available);
    PC_CHECK(illegal.policy_status == SolvePolicyStatus::None);
    PC_CHECK(illegal.diagnostics.resource_cap_hit);
    PC_CHECK(illegal.termination == SolveTermination::RefusedResourceCap);
    PC_CHECK(!std::isfinite(illegal.upper_bound));
    PC_CHECK(std::none_of(
        illegal.diagnostics.action_inclusion_reasons.begin(),
        illegal.diagnostics.action_inclusion_reasons.end(),
        [](const std::string& reason) {
            return reason.find(
                       "included:primitive_destructive_renewal_policy:chaos") !=
                   std::string::npos;
        }));

    ActionRegistry illegal_gated_registry = registry;
    illegal_gated_registry.actions.at(chaos)
        .legality.requires_open_affix = true;
    CalcContext illegal_gated_calc(
        session, goal, std::move(illegal_gated_registry),
        {alchemy, chaos, restart});
    SolveOptions illegal_gated_options = options;
    illegal_gated_options.goal_progress_gated_reforges = true;
    illegal_gated_options.max_expanded_states = 1;
    const SolveResult illegal_gated = solve(
        illegal_gated_calc, start, prices, illegal_gated_options);
    PC_CHECK(!illegal_gated.primitive_renewal_witness.valid);
    PC_CHECK(
        illegal_gated.diagnostics.gated_root_renewal_candidates > 0);
    PC_CHECK(
        illegal_gated.diagnostics.gated_root_renewal_rejections > 0);

    auto fracture_session = make_solve_session();
    for (std::uint32_t mod = 0;
         mod < fracture_session->base_spawn_weight.size(); ++mod) {
        fracture_session->base_spawn_weight[mod] =
            (mod == 0 || mod == 3 || mod == 5) ? 1 : 1000;
    }
    fracture_session->base_roll_weight =
        fracture_session->base_spawn_weight;
    ActionRegistry fracture_registry =
        build_action_registry(*fracture_session);
    const std::uint32_t fracture_alchemy =
        fracture_registry.index_by_id.at("alchemy");
    const std::uint32_t fracture_chaos =
        fracture_registry.index_by_id.at("chaos");
    const std::uint32_t fracture =
        fracture_registry.index_by_id.at("fracture");
    const std::uint32_t fracture_restart =
        fracture_registry.index_by_id.at("restart");
    CalcContext fracture_calc(
        fracture_session, goal, fracture_registry,
        {fracture_alchemy, fracture_chaos, fracture, fracture_restart});
    const std::uint32_t fracture_start_state =
        fracture_calc.intern_item(start);
    const OutcomeDistribution fracture_direct_kernel =
        fracture_calc.outcomes(fracture_start_state, fracture_chaos);
    double fracture_direct_probability = 0.0;
    for (const OutcomeEntry& outcome : fracture_direct_kernel.entries) {
        if (fracture_calc.is_goal_state(
                fracture_calc.state(outcome.state))) {
            fracture_direct_probability += outcome.probability;
        }
    }
    PC_CHECK(fracture_direct_probability > 0.0);
    const double fracture_direct_value =
        1.0 / fracture_direct_probability;
    auto fracture_prices = std::unordered_map<std::string, double>{
        {"alchemy", 0.5}, {"chaos", 1.0},
        {"fracture", 0.1}, {"base", 1.0}};
    fracture_prices["fracture"] = 0.1;
    const SolveResult progressive = solve(
        fracture_calc, start, fracture_prices, options);
    std::printf(
        "solver progressive fracture oracle: action=%s status=%s "
        "class=%u/%u bootstrap=%.9g progressive=%.9g upper=%.9g "
        "post_modes=%u\n",
        progressive.diagnostics.destructive_renewal_action_id.c_str(),
        progressive.diagnostics.progressive_fracture_status.c_str(),
        progressive.diagnostics.progressive_fracture_class_mask,
        progressive.diagnostics.progressive_fracture_class_mod_count,
        progressive.diagnostics.destructive_renewal_start_value,
        progressive.diagnostics.progressive_fracture_start_value,
        progressive.diagnostics.focused_upper_bound,
        progressive.diagnostics.progressive_fracture_post_modes);
    PC_CHECK(std::isfinite(
        progressive.diagnostics.focused_upper_bound));
    PC_CHECK(progressive.diagnostics.focused_upper_bound <
             fracture_direct_value);
    PC_CHECK(
        progressive.diagnostics.progressive_fracture_roll_action_id ==
        "chaos");
    PC_CHECK(std::isfinite(
        progressive.diagnostics.progressive_fracture_start_value));
    PC_CHECK(progressive.diagnostics.progressive_fracture_post_modes > 0);

    /*
     * The product solver's coarse parent retains only goal-hit Fracture
     * successors plus one priced Restart miss. The ordinary exact context
     * above remains the strict primitive oracle.
     */
    CalcContext product_fracture_calc(
        fracture_session, goal, fracture_registry,
        {fracture_alchemy, fracture_chaos, fracture, fracture_restart},
        false, true, false, std::nullopt, {}, true);
    PC_CHECK(product_fracture_calc.product_solver_parent());
    PC_CHECK(
        product_fracture_calc.layout().junk_classes.size() <
        fracture_calc.layout().junk_classes.size());

    /*
     * The product-only reforge symmetry compression must be an exact quotient
     * of the strict physical-family frontier. Project every strict Chaos exit
     * through a concrete representative, then compare the entire probability
     * distribution with the online coarse evaluator.
     */
    const std::uint32_t product_fracture_start_state =
        product_fracture_calc.intern_item(start);
    std::unordered_map<std::uint32_t, double> projected_strict_chaos;
    for (const OutcomeEntry& outcome : fracture_direct_kernel.entries) {
        pc_item_state concrete;
        PC_CHECK(fracture_calc.materialize(outcome.state, concrete));
        projected_strict_chaos[
            product_fracture_calc.intern_item(concrete)] +=
            outcome.probability;
    }
    const OutcomeDistribution product_chaos_kernel =
        product_fracture_calc.outcomes(
            product_fracture_start_state, fracture_chaos);
    PC_CHECK(
        product_chaos_kernel.entries.size() ==
        projected_strict_chaos.size());
    PC_CHECK(
        product_chaos_kernel.entries.size() <
        fracture_direct_kernel.entries.size());
    for (const OutcomeEntry& outcome : product_chaos_kernel.entries) {
        const auto expected = projected_strict_chaos.find(outcome.state);
        PC_CHECK(expected != projected_strict_chaos.end());
        if (expected != projected_strict_chaos.end()) {
            PC_CHECK(near(
                outcome.probability, expected->second, 1e-12));
        }
    }

    SolveOptions product_fracture_options;
    product_fracture_options.max_expanded_states = 512;
    product_fracture_options.max_discovered_states = 4096;
    product_fracture_options.state_certificate_control = false;
    product_fracture_options.allow_economic_restart = false;
    const SolveResult product_fracture_result = solve(
        product_fracture_calc, start, fracture_prices,
        product_fracture_options);
    PC_CHECK(
        product_fracture_result.diagnostics.product_fracture_rows > 0);
    PC_CHECK(
        product_fracture_result.diagnostics
                .product_fracture_parent_miss_states_interned == 0);
    PC_CHECK(
        product_fracture_result.diagnostics
                .product_fracture_max_probability_error <= 1e-12);
    PC_CHECK(
        product_fracture_result.diagnostics
                .product_fracture_raw_outcomes >=
        product_fracture_result.diagnostics
                .product_fracture_hit_entries +
            product_fracture_result.diagnostics
                .product_fracture_miss_entries);
    PC_CHECK(
        product_fracture_result.diagnostics
                .product_fracture_selected_properness_checked ==
        product_fracture_result.diagnostics
                .product_fracture_selected_rows);
    PC_CHECK(
        product_fracture_result.diagnostics
                .product_fracture_selected_proper_rows ==
        product_fracture_result.diagnostics
                .product_fracture_selected_rows);
    PC_CHECK(
        product_fracture_result.diagnostics
                .product_fracture_selected_improper_rows == 0);
    PC_CHECK(
        product_fracture_result.diagnostics
                .product_fracture_selected_unproved_rows == 0);
    PC_CHECK(
        product_fracture_result.diagnostics
                .product_fracture_q_evaluated_rows +
            product_fracture_result.diagnostics
                .product_fracture_q_unresolved_rows ==
        product_fracture_result.diagnostics.product_fracture_rows);
    PC_CHECK(
        product_fracture_result.diagnostics
                .product_fracture_q_cheaper_rows +
            product_fracture_result.diagnostics
                .product_fracture_q_tied_rows +
            product_fracture_result.diagnostics
                .product_fracture_q_costlier_rows ==
        product_fracture_result.diagnostics
            .product_fracture_q_evaluated_rows);
    PC_CHECK(
        !product_fracture_result.diagnostics
             .product_fracture_witnesses.empty());
    PC_CHECK(
        product_fracture_result.diagnostics
                .product_fracture_witnesses.front().find(
                    "\"final_q\":{\"evaluated\":") !=
        std::string::npos);
    PC_CHECK(
        product_fracture_result.diagnostics.action_search_costs.at(
            "fracture").cache_requests == 0);
    PC_CHECK(
        product_fracture_result.diagnostics.action_search_costs.at(
            "chaos").root_retained_transitions ==
        product_chaos_kernel.entries.size());

    /* A strict policy lift must preserve the product Fracture quotient all
     * the way through executable compilation. Raw non-goal physical fracture
     * outcomes are the priced fresh-base branch, not off-policy strict junk
     * carriers. */
    const refinement::PolicyExactLiftCertificate product_fracture_lift =
        refinement::lift_policy_quotient(
            product_fracture_calc, product_fracture_result, start,
            fracture_prices, product_fracture_options,
            "product Fracture strict restart quotient");
    report_lift_failure(
        "Product Fracture lift", product_fracture_lift);
    PC_CHECK(
        product_fracture_lift.status ==
        refinement::PolicyExactLiftStatus::Complete);
    PC_CHECK(product_fracture_lift.executable);
    PC_CHECK(
        product_fracture_lift.compiled.status ==
        refinement::CompiledPolicyAssertionStatus::Complete);
    PC_CHECK(product_fracture_lift.compiled.paired_default_only);
    PC_CHECK(product_fracture_lift.compiled.executable);
    PC_CHECK(product_fracture_lift.compiled.proper);
    PC_CHECK(product_fracture_lift.compiled.zero_off_policy);
    PC_CHECK(product_fracture_lift.compiled.cost_reconciled);
    PC_CHECK(
        product_fracture_lift.compiled.strategy_json.find(
            "_fracture_route") != std::string::npos);
    PC_CHECK(
        product_fracture_lift.compiled.strategy_json.find(
            "\"type\":\"restart\"") != std::string::npos);
    PC_CHECK(
        product_fracture_lift.compiled.strategy_json.find(
            "\"solver_policy_scope\":\""
            "no_economic_restart_policy_restriction\"") !=
        std::string::npos);
    const std::shared_ptr<StrategyImpl> product_fracture_strategy =
        compile_strategy_json(
            fracture_session,
            product_fracture_lift.compiled.strategy_json.data(),
            product_fracture_lift.compiled.strategy_json.size());
    auto product_fracture_economy = std::make_shared<EconomyImpl>();
    product_fracture_economy->id =
        "product-fracture-replacement-recovery-test";
    product_fracture_economy->prices = fracture_prices;
    SimulatorImpl product_fracture_simulator;
    product_fracture_simulator.session = fracture_session;
    product_fracture_simulator.strategy = product_fracture_strategy;
    product_fracture_simulator.economy = product_fracture_economy;
    prepare_simulator_runtime(product_fracture_simulator);
    SimulationOptionsInternal product_fracture_simulation_options;
    product_fracture_simulation_options.target_runs = 10000;
    product_fracture_simulation_options.seed = 0x465241435245434fULL;
    product_fracture_simulation_options.max_actions_per_run = 100000;
    run_simulator_chunk(
        product_fracture_simulator,
        product_fracture_simulation_options, 10000);
    PC_CHECK(
        product_fracture_simulator.summary.completed_runs == 10000);
    PC_CHECK(product_fracture_simulator.summary.success_count == 10000);
    PC_CHECK(
        product_fracture_simulator.summary.action_not_applied_count == 0);
    PC_CHECK(
        product_fracture_simulator.summary.no_matching_edge_count == 0);
    std::set<std::pair<std::string, std::uint32_t>>
        product_fracture_behaviors;
    std::uint64_t product_fracture_selected_states = 0;
    for (std::uint32_t state = 0;
         state < product_fracture_result.policy.size(); ++state) {
        if (state >= product_fracture_result.policy_reachable.size() ||
            !product_fracture_result.policy_reachable[state] ||
            state >= product_fracture_result.goal_states.size() ||
            product_fracture_result.goal_states[state] ||
            product_fracture_result.policy[state] == kNoId) {
            continue;
        }
        const PlannerOperator& planner =
            product_fracture_calc.operators().at(
                product_fracture_result.policy[state]);
        if (planner.kind != PlannerOperatorKind::Primitive ||
            planner.automatic_kind !=
                AutomaticCandidateKind::Fracture) {
            continue;
        }
        ++product_fracture_selected_states;
        const AbstractState& source =
            product_fracture_calc.state(state);
        std::uint32_t acceptable_hit_mask = 0;
        for (std::uint32_t slot = 0;
             slot < product_fracture_calc.layout().slots.size(); ++slot) {
            const std::uint32_t bit = 1u << slot;
            if ((planner.relevant_goal_mask & bit) != 0 &&
                source.slot_status[slot] ==
                    static_cast<std::uint8_t>(
                        GoalSlotStatus::Satisfied) &&
                (source.fractured_goal_mask & bit) == 0) {
                acceptable_hit_mask |= bit;
            }
        }
        PC_CHECK(acceptable_hit_mask != 0);
        PC_CHECK(planner.primitive_program.size() == 1);
        if (planner.primitive_program.size() == 1) {
            product_fracture_behaviors.emplace(
                product_fracture_calc.registry()
                    .actions.at(planner.primitive_program.front()).id,
                acceptable_hit_mask);
        }
    }
    PolicyCompilationTelemetry shared_product_fracture_compilation;
    const std::string shared_product_fracture_json =
        compile_policy_strategy_json(
            product_fracture_calc, product_fracture_result,
            "behavior-shared product Fracture",
            &shared_product_fracture_compilation);
    const std::size_t emitted_fracture_routes = count_occurrences(
        shared_product_fracture_json,
        "_fracture_route\",\"kind\":\"router\"");
    const std::size_t emitted_fracture_operations = count_occurrences(
        shared_product_fracture_json,
        "\"operation\":{\"type\":\"fracture\"}");
    std::printf(
        "solver product Fracture compiler sharing: states=%llu "
        "behaviors=%llu routes=%llu operations=%llu regions=%llu/%llu\n",
        static_cast<unsigned long long>(
            product_fracture_selected_states),
        static_cast<unsigned long long>(
            product_fracture_behaviors.size()),
        static_cast<unsigned long long>(emitted_fracture_routes),
        static_cast<unsigned long long>(emitted_fracture_operations),
        static_cast<unsigned long long>(
            shared_product_fracture_compilation.policy_regions),
        static_cast<unsigned long long>(
            shared_product_fracture_compilation.working_states));
    PC_CHECK(product_fracture_behaviors.size() > 1);
    PC_CHECK(
        product_fracture_selected_states >
        product_fracture_behaviors.size());
    PC_CHECK(emitted_fracture_routes == 1);
    PC_CHECK(emitted_fracture_operations == 1);
    PC_CHECK(
        shared_product_fracture_compilation.policy_regions <
        shared_product_fracture_compilation.working_states);
    PC_CHECK(shared_product_fracture_compilation.infrastructure_nodes > 0);
    PC_CHECK(shared_product_fracture_compilation.policy_route_nodes > 0);
    PC_CHECK(
        shared_product_fracture_compilation.primitive_region_nodes >=
        emitted_fracture_operations);
    PC_CHECK(
        shared_product_fracture_compilation.infrastructure_nodes +
            shared_product_fracture_compilation.policy_route_nodes +
            shared_product_fracture_compilation.local_gated_route_nodes +
            shared_product_fracture_compilation.primitive_region_nodes +
            shared_product_fracture_compilation.additional_recipe_nodes ==
        shared_product_fracture_compilation.nodes);
    const std::shared_ptr<StrategyImpl> shared_product_fracture_strategy =
        compile_strategy_json(
            fracture_session, shared_product_fracture_json.data(),
            shared_product_fracture_json.size());
    auto shared_product_fracture_economy =
        std::make_shared<EconomyImpl>();
    shared_product_fracture_economy->id =
        "behavior-shared-product-fracture";
    shared_product_fracture_economy->prices = fracture_prices;
    StrategyEvalOptions shared_product_fracture_eval_options;
    shared_product_fracture_eval_options.economy =
        shared_product_fracture_economy;
    const StrategyEvalResult shared_product_fracture_evaluation =
        evaluate_strategy(
            *shared_product_fracture_strategy,
            shared_product_fracture_eval_options);
    PC_CHECK(shared_product_fracture_evaluation.converged);
    PC_CHECK(shared_product_fracture_evaluation.cost_complete);
    PC_CHECK(near(
        shared_product_fracture_evaluation.success_probability,
        1.0, 1e-12));
    PC_CHECK(near(
        shared_product_fracture_evaluation.failure_probability,
        0.0, 1e-12));
    PC_CHECK(near(
        shared_product_fracture_evaluation.action_not_applied_probability,
        0.0, 1e-12));
    PC_CHECK(near(
        shared_product_fracture_evaluation.no_matching_edge_probability,
        0.0, 1e-12));
    PC_CHECK(near(
        shared_product_fracture_evaluation.total_expected_cost,
        product_fracture_result.evaluated_policy_cost, 1e-10));
    PC_CHECK(near(
        shared_product_fracture_evaluation.total_expected_cost,
        product_fracture_lift.compiled.evaluation.total_expected_cost,
        1e-10));
    PC_CHECK(
        shared_product_fracture_evaluation.expected_consumption.size() ==
        product_fracture_lift.compiled.evaluation
            .expected_consumption.size());
    for (const auto& [action, expected] :
         product_fracture_lift.compiled.evaluation
             .expected_consumption) {
        const auto shared =
            shared_product_fracture_evaluation.expected_consumption.find(
                action);
        PC_CHECK(
            shared !=
            shared_product_fracture_evaluation.expected_consumption.end());
        if (shared !=
            shared_product_fracture_evaluation.expected_consumption.end()) {
            PC_CHECK(near(shared->second, expected, 1e-10));
        }
    }
    PC_CHECK(
        product_fracture_lift.compiled.evaluation
            .expected_consumption.contains("base"));
    if (product_fracture_lift.compiled.evaluation
            .expected_consumption.contains("base")) {
        PC_CHECK(
            product_fracture_lift.compiled.evaluation
                .expected_consumption.at("base") > 0.0);
    }

    std::uint64_t zero_hit_rows = 0;
    std::uint64_t single_hit_rows = 0;
    std::uint64_t multiple_hit_rows = 0;
    for (std::size_t n = 0;
         n < product_fracture_result.diagnostics
                 .product_fracture_shape_rows.size();
         ++n) {
        zero_hit_rows +=
            product_fracture_result.diagnostics
                .product_fracture_shape_rows[n][0];
        single_hit_rows +=
            product_fracture_result.diagnostics
                .product_fracture_shape_rows[n][1];
        for (std::size_t k = 2;
             k < product_fracture_result.diagnostics
                     .product_fracture_shape_rows[n].size();
             ++k) {
            multiple_hit_rows +=
                product_fracture_result.diagnostics
                    .product_fracture_shape_rows[n][k];
        }
    }
    PC_CHECK(zero_hit_rows == 0);
    PC_CHECK(single_hit_rows > 0);
    PC_CHECK(multiple_hit_rows > 0);
    for (std::size_t n = 4; n <= 6; ++n) {
        std::uint64_t rows = 0;
        for (const std::uint64_t count :
             product_fracture_result.diagnostics
                 .product_fracture_shape_rows[n]) {
            rows += count;
        }
        PC_CHECK(rows > 0);
    }

    /*
     * If two goal slots can name the same physical affix, the coarse carrier
     * cannot decide whether k counts one affix or two. The prototype must
     * refuse that observer collision instead of overcounting it.
     */
    pc_item_state overlap_carrier;
    bool found_overlap_carrier = false;
    for (const OutcomeEntry& outcome : fracture_direct_kernel.entries) {
        const AbstractState& candidate =
            fracture_calc.state(outcome.state);
        if (candidate.prefix_count + candidate.suffix_count < 4 ||
            candidate.slot_status[0] !=
                static_cast<std::uint8_t>(
                    GoalSlotStatus::Satisfied) ||
            candidate.slot_status[1] ==
                static_cast<std::uint8_t>(
                    GoalSlotStatus::Satisfied) ||
            candidate.slot_status[2] ==
                static_cast<std::uint8_t>(
                    GoalSlotStatus::Satisfied)) {
            continue;
        }
        found_overlap_carrier =
            fracture_calc.materialize(
                outcome.state, overlap_carrier);
        if (found_overlap_carrier) break;
    }
    PC_CHECK(found_overlap_carrier);
    if (found_overlap_carrier) {
        GoalSpec overlap_goal = goal;
        overlap_goal.slots[1] = overlap_goal.slots[0];
        bool overlap_refused = false;
        try {
            CalcContext overlap_calc(
                fracture_session, overlap_goal, fracture_registry,
                {fracture_alchemy, fracture_chaos, fracture,
                 fracture_restart},
                false, true, false, std::nullopt, {}, true);
            SolveOptions overlap_options;
            overlap_options.max_expanded_states = 32;
            overlap_options.max_discovered_states = 1024;
            overlap_options.state_certificate_control = false;
            (void)solve(
                overlap_calc, overlap_carrier, fracture_prices,
                overlap_options);
        } catch (const std::runtime_error& exception) {
            const std::string message = exception.what();
            overlap_refused =
                message.find("overlapping members") !=
                    std::string::npos ||
                message.find(
                    "physical hit identity across goal slots") !=
                    std::string::npos;
        }
        PC_CHECK(overlap_refused);
    }

    SolveOptions retained_progressive_options = options;
    retained_progressive_options.max_expanded_states = 8;
    retained_progressive_options.max_absolute_optimality_gap = 1e-30;
    const SolveResult retained_progressive = solve(
        fracture_calc, start, fracture_prices,
        retained_progressive_options);
    PC_CHECK(
        retained_progressive.diagnostics.focused_expansion_rounds > 1);
    PC_CHECK(
        retained_progressive.diagnostics.constructive_policy_syntheses >= 1);
    PC_CHECK(
        retained_progressive.diagnostics.constructive_policy_reuses >= 1);
    PC_CHECK(
        retained_progressive.diagnostics.constructive_policy_refreshes == 0);
    PC_CHECK(
        retained_progressive.diagnostics.constructive_policy_syntheses <
        retained_progressive.diagnostics.focused_expansion_rounds);
    PC_CHECK(
        !retained_progressive.diagnostics.focused_schedule_rounds.empty());
    PC_CHECK(std::any_of(
        retained_progressive.diagnostics.focused_schedule_rounds.begin(),
        retained_progressive.diagnostics.focused_schedule_rounds.end(),
        [](const FocusedScheduleRoundTelemetry& round) {
            return round.schedule_candidates > 0 &&
                   round.schedule_admissions > 0;
        }));
    PC_CHECK(
        retained_progressive.diagnostics.fallback_validation.calls > 0);
    PC_CHECK(
        retained_progressive.diagnostics.fallback_validation.goal_identity
            .checks > 0);
    PC_CHECK(
        retained_progressive.diagnostics.fallback_validation.structural
            .checks > 0);
    PC_CHECK(
        retained_progressive.diagnostics.fallback_validation
            .successful_proof_cache_checks ==
        retained_progressive.diagnostics.fallback_validation.calls);
    PC_CHECK(
        retained_progressive.diagnostics.fallback_validation
            .start_properness.checks +
            retained_progressive.diagnostics.fallback_validation
                .successful_proof_cache_hits ==
        retained_progressive.diagnostics.fallback_validation.calls);

    SolveOptions uncached_progressive_options =
        retained_progressive_options;
    uncached_progressive_options.fallback_properness_reuse_control = false;
    CalcContext cached_parity_fracture_calc(
        fracture_session, goal, fracture_registry,
        {fracture_alchemy, fracture_chaos, fracture, fracture_restart});
    const SolveResult cached_parity_progressive = solve(
        cached_parity_fracture_calc, start, fracture_prices,
        retained_progressive_options);
    CalcContext uncached_fracture_calc(
        fracture_session, goal, fracture_registry,
        {fracture_alchemy, fracture_chaos, fracture, fracture_restart});
    const SolveResult uncached_progressive = solve(
        uncached_fracture_calc, start, fracture_prices,
        uncached_progressive_options);
    PC_CHECK(
        uncached_progressive.diagnostics.fallback_validation
            .successful_proof_cache_hits == 0);
    PC_CHECK(
        uncached_progressive.diagnostics.fallback_validation
            .start_properness.checks ==
        uncached_progressive.diagnostics.fallback_validation.calls);
    PC_CHECK(
        uncached_progressive.converged ==
        cached_parity_progressive.converged);
    PC_CHECK(
        uncached_progressive.policy_available ==
        cached_parity_progressive.policy_available);
    PC_CHECK(
        uncached_progressive.policy_status ==
        cached_parity_progressive.policy_status);
    PC_CHECK(
        uncached_progressive.termination ==
        cached_parity_progressive.termination);
    PC_CHECK(
        uncached_progressive.lower_bound ==
        cached_parity_progressive.lower_bound);
    PC_CHECK(
        uncached_progressive.upper_bound ==
        cached_parity_progressive.upper_bound);
    PC_CHECK(
        uncached_progressive.values ==
        cached_parity_progressive.values);
    PC_CHECK(
        uncached_progressive.policy ==
        cached_parity_progressive.policy);
    PC_CHECK(
        uncached_progressive.diagnostics.transition_bits_hash ==
        cached_parity_progressive.diagnostics.transition_bits_hash);
    PC_CHECK(
        uncached_progressive.diagnostics.policy_bits_hash ==
        cached_parity_progressive.diagnostics.policy_bits_hash);
    const std::string focused_instrumentation =
        serialize_solver_telemetry(
            fracture_calc, &retained_progressive, nullptr,
            std::nullopt, nullptr);
    PC_CHECK(valid_json_object(focused_instrumentation));
    PC_CHECK(
        focused_instrumentation.find(
            "\"schedule\":{\"counting_contract\":") !=
        std::string::npos);
    PC_CHECK(
        focused_instrumentation.find(
            "\"fallback_validation\":{\"timing_contract\":") !=
        std::string::npos);
    PC_CHECK(
        focused_instrumentation.find(
            "\"successful_proof_cache\":{\"version\":1") !=
        std::string::npos);

    fracture_prices["fracture"] = 1000000.0;
    const SolveResult expensive_fracture = solve(
        fracture_calc, start, fracture_prices, options);
    PC_CHECK(near(
        expensive_fracture.diagnostics.focused_upper_bound,
        fracture_direct_value, 1e-5));
}

/* Goal-progress gating is an exact partition of the unrestricted reforge
 * row: terminal and zero-progress classes are each folded to one successor,
 * while every partial-progress state and its raw probability are retained. */
void run_goal_progress_gated_reforge_tests() {
    auto session = make_solve_session();
    ActionRegistry registry = build_action_registry(*session);
    GoalSpec goal;
    goal.rarity = PC_RARITY_RARE;
    for (const std::uint32_t family : {100u, 102u, 104u}) {
        GoalSlot slot;
        slot.family_id = family;
        slot.min_tier = 1;
        goal.slots.push_back(slot);
    }
    const std::uint32_t chaos = registry.index_by_id.at("chaos");
    CalcContext calc(session, goal, registry, {chaos});
    pc_item_state start;
    pc_item_clear(&start);
    start.rarity = PC_RARITY_RARE;
    const std::uint32_t start_state = calc.intern_item(start);

    const OutcomeDistribution& unrestricted =
        calc.outcomes(start_state, chaos);
    PC_CHECK(unrestricted.supported);
    PC_CHECK(!unrestricted.goal_progress_gated);
    const std::vector<OutcomeEntry> unrestricted_entries =
        unrestricted.entries;
    std::unordered_map<std::uint32_t, double> partial_probability;
    double terminal_probability = 0.0;
    double retry_probability = 0.0;
    double absent_retry_probability = 0.0;
    double below_tier_retry_probability = 0.0;
    for (const OutcomeEntry& entry : unrestricted_entries) {
        const AbstractState& state = calc.state(entry.state);
        if (calc.is_goal_state(state)) {
            terminal_probability += entry.probability;
        } else if (satisfied_goal_count(state, goal) == 0) {
            retry_probability += entry.probability;
            bool any_below_tier = false;
            for (std::size_t slot = 0; slot < goal.slots.size(); ++slot) {
                any_below_tier =
                    any_below_tier ||
                    state.slot_status[slot] ==
                        static_cast<std::uint8_t>(
                            GoalSlotStatus::PresentBelowTier);
            }
            if (any_below_tier) {
                below_tier_retry_probability += entry.probability;
            } else {
                absent_retry_probability += entry.probability;
            }
        } else {
            partial_probability[entry.state] += entry.probability;
        }
    }
    PC_CHECK(terminal_probability > 0.0);
    PC_CHECK(retry_probability > 0.0);
    PC_CHECK(absent_retry_probability > 0.0);
    PC_CHECK(below_tier_retry_probability > 0.0);
    PC_CHECK(!partial_probability.empty());

    const OutcomeDistribution& gated =
        calc.outcomes(start_state, chaos, true);
    PC_CHECK(gated.supported);
    PC_CHECK(gated.goal_progress_gated);
    PC_CHECK(&calc.outcomes(start_state, chaos, false) ==
             &unrestricted);
    PC_CHECK(unrestricted.entries == unrestricted_entries);
    PC_CHECK(gated.entries.size() < unrestricted.entries.size());
    PC_CHECK(gated.gated_terminal_state != kNoId);
    PC_CHECK(gated.gated_retry_state != kNoId);
    PC_CHECK(calc.is_goal_state(
        calc.state(gated.gated_terminal_state)));
    PC_CHECK(calc.state(gated.gated_retry_state)
                 .goal_progress_retry_basin != 0);
    PC_CHECK(near(
        gated.gated_terminal_probability,
        terminal_probability, 1e-12));
    PC_CHECK(near(
        gated.gated_retry_probability,
        retry_probability, 1e-12));
    PC_CHECK(near(
        gated.gated_retry_probability,
        absent_retry_probability + below_tier_retry_probability,
        1e-12));

    double total_probability = 0.0;
    double partial_total = 0.0;
    std::uint64_t partial_states = 0;
    for (const OutcomeEntry& entry : gated.entries) {
        total_probability += entry.probability;
        if (entry.state == gated.gated_terminal_state ||
            entry.state == gated.gated_retry_state) {
            continue;
        }
        const AbstractState& state = calc.state(entry.state);
        PC_CHECK(!calc.is_goal_state(state));
        PC_CHECK(state.goal_progress_retry_basin == 0);
        PC_CHECK(satisfied_goal_count(state, goal) > 0);
        const auto expected = partial_probability.find(entry.state);
        PC_CHECK(expected != partial_probability.end());
        if (expected != partial_probability.end()) {
            PC_CHECK(near(
                entry.probability, expected->second, 1e-12));
        }
        partial_total += entry.probability;
        ++partial_states;
    }
    PC_CHECK(near(total_probability, 1.0, 1e-12));
    PC_CHECK(near(
        gated.gated_partial_probability,
        partial_total, 1e-12));
    PC_CHECK(gated.gated_partial_states == partial_states);
    PC_CHECK(partial_states == partial_probability.size());
    PC_CHECK(gated.gated_terminal_short_circuits > 0);

    pc_item_state retry_item;
    PC_CHECK(calc.materialize(
        gated.gated_retry_state, retry_item));
    const std::uint32_t ordinary_retry_state =
        calc.intern_item(retry_item);
    PC_CHECK(ordinary_retry_state != gated.gated_retry_state);
    PC_CHECK(calc.state(ordinary_retry_state)
                 .goal_progress_retry_basin == 0);

    CalcContext deterministic_calc(
        session, goal, registry, {chaos});
    const std::uint32_t deterministic_start =
        deterministic_calc.intern_item(start);
    const OutcomeDistribution& deterministic =
        deterministic_calc.outcomes(
            deterministic_start, chaos, true);
    PC_CHECK(distribution_state_probability_bits(calc, gated) ==
             distribution_state_probability_bits(
                 deterministic_calc, deterministic));
    PC_CHECK(
        std::bit_cast<std::uint64_t>(
            gated.gated_terminal_probability) ==
        std::bit_cast<std::uint64_t>(
            deterministic.gated_terminal_probability));
    PC_CHECK(
        std::bit_cast<std::uint64_t>(
            gated.gated_retry_probability) ==
        std::bit_cast<std::uint64_t>(
            deterministic.gated_retry_probability));
    PC_CHECK(gated.gated_kernel_bits_hash ==
             deterministic.gated_kernel_bits_hash);

    const std::string telemetry = serialize_solver_telemetry(
        calc, nullptr, nullptr, std::nullopt, nullptr);
    PC_CHECK(valid_json_object(telemetry));
    PC_CHECK(telemetry.find(
                 "\"goal_progress_gated\":{\"rows\":1") !=
             std::string::npos);

    /* A full zero-progress start cannot Bench. After Chaos misses, the
     * physical preserved carrier would make a cheap goal Bench legal, but
     * the virtual basin must still select only destructive reforges. */
    auto restricted_session = make_solve_session();
    ActionRegistry restricted_registry =
        build_action_registry(*restricted_session);
    restricted_session->bench_mod_ids = {0};
    ActionDescriptor goal_bench;
    goal_bench.id = "bench:gated_goal";
    goal_bench.display_name = "Bench gated goal";
    goal_bench.params.type = ActionType::Bench;
    goal_bench.params.mod_id = 0;
    goal_bench.kind = TransitionKind::Deterministic;
    goal_bench.cost_keys = {goal_bench.id};
    goal_bench.legality.rarity_mask = 1u << PC_RARITY_RARE;
    goal_bench.legality.requires_open_affix = true;
    goal_bench.sets_flags = kFlagCraftedMod;
    goal_bench.refinement =
        derive_action_refinement_contract(
            *restricted_session, goal_bench);
    validate_action_refinement_contract(goal_bench);
    const std::uint32_t bench_index =
        static_cast<std::uint32_t>(
            restricted_registry.actions.size());
    restricted_registry.index_by_id.emplace(
        goal_bench.id, bench_index);
    restricted_registry.actions.push_back(std::move(goal_bench));
    const std::uint32_t restricted_chaos =
        restricted_registry.index_by_id.at("chaos");

    GoalSpec restricted_goal;
    restricted_goal.rarity = PC_RARITY_RARE;
    GoalSlot restricted_slot;
    restricted_slot.family_id = 100;
    restricted_slot.min_tier = 1;
    restricted_goal.slots.push_back(restricted_slot);
    CalcContext restricted_calc(
        restricted_session, restricted_goal, restricted_registry,
        {restricted_chaos, bench_index});
    pc_item_state full_start;
    pc_item_clear(&full_start);
    full_start.rarity = PC_RARITY_RARE;
    place(&full_start, PC_SIDE_PREFIX, 2, 10);
    place(&full_start, PC_SIDE_PREFIX, 3, 12);
    place(&full_start, PC_SIDE_PREFIX, 4, 13);
    place(&full_start, PC_SIDE_SUFFIX, 5, 20);
    place(&full_start, PC_SIDE_SUFFIX, 6, 21);
    place(&full_start, PC_SIDE_SUFFIX, 7, 22);
    const std::uint32_t restricted_start_state =
        restricted_calc.intern_item(full_start);
    PC_CHECK(
        restricted_calc.outcomes(
            restricted_start_state, restricted_chaos, true)
            .gated_retry_short_circuits > 0);
    SolveOptions restricted_options;
    restricted_options.goal_progress_gated_reforges = true;
    const std::unordered_map<std::string, double> restricted_prices{
        {"chaos", 1.0},
        {"bench:gated_goal", 0.001},
        {"base", 1000000000.0}};
    const SolveResult restricted = solve(
        restricted_calc, full_start, restricted_prices,
        restricted_options);
    PC_CHECK(restricted.converged);
    PC_CHECK(restricted.policy_available);
    PC_CHECK(restricted.diagnostics.solution_scope ==
             "exact_within_zero_progress_reroll_restriction");
    PC_CHECK(
        restricted.policy_status == SolvePolicyStatus::Exact);
    const double restricted_success_probability =
        restricted_calc.outcomes(
            restricted_start_state, restricted_chaos, true)
            .gated_terminal_probability;
    PC_CHECK(restricted_success_probability > 0.0);
    PC_CHECK(near(
        restricted.values[restricted.start_state],
        1.0 / restricted_success_probability, 1e-9));
    PolicyCompilationTelemetry restricted_compilation;
    const std::string restricted_compact_json =
        compile_policy_strategy_json(
            restricted_calc, restricted,
            "exact gated destructive renewal",
            &restricted_compilation);
    PC_CHECK(restricted_compilation.nodes == 3);
    PC_CHECK(restricted_compilation.edges == 3);
    PC_CHECK(restricted_compilation.policy_regions == 1);
    PC_CHECK(restricted_compilation.junk_predicates == 0);
    PC_CHECK(restricted_compact_json.size() < 8192);
    PC_CHECK(restricted_compact_json.find(
                 "\"from\":\"start\",\"to\":\"renewal\"") !=
             std::string::npos);
    PC_CHECK(restricted_compact_json.find(
                 "\"from\":\"renewal\",\"to\":\"goal\"") !=
             std::string::npos);
    PC_CHECK(restricted_compact_json.find(
                 "\"from\":\"renewal\",\"to\":\"renewal\"") !=
             std::string::npos);
    PC_CHECK(restricted_compact_json.find(
                 "\"id\":\"router\",\"kind\":\"router\"") ==
             std::string::npos);
    PC_CHECK(restricted_compact_json.find(
                 "\"description\":\"Exact executable fixed "
                 "destructive-renewal policy within the "
                 "zero-progress-reroll restriction\"") !=
             std::string::npos);
    PC_CHECK(restricted_compact_json.find(
                 "\"solver_policy_scope\":\""
                 "zero_progress_reroll_policy_restriction\"") !=
             std::string::npos);
    const std::shared_ptr<StrategyImpl> restricted_compact_strategy =
        compile_strategy_json(
            restricted_session, restricted_compact_json.data(),
            restricted_compact_json.size());
    PC_CHECK(restricted_compact_strategy != nullptr);

    /* A strict carrier established only by non-affix metadata cannot use the
     * reduced graph's observation domain.  The general route retains the
     * explicit count observers needed by exact evaluation. */
    SolveResult strict_metadata_renewal = restricted;
    pc_item_clear(&strict_metadata_renewal.exact_start_item);
    strict_metadata_renewal.exact_start_item.rarity = PC_RARITY_RARE;
    strict_metadata_renewal.exact_start_item.generic_influence_bits = 1;
    strict_metadata_renewal.has_exact_start_item = true;
    strict_metadata_renewal.refined_policy_artifact = {};
    PolicyCompilationTelemetry strict_metadata_compilation;
    const std::string strict_metadata_json =
        compile_policy_strategy_json(
            restricted_calc, strict_metadata_renewal,
            "strict metadata renewal", &strict_metadata_compilation);
    PC_CHECK(valid_json_object(strict_metadata_json));
    PC_CHECK(
        strict_metadata_compilation.nodes > 4 ||
        strict_metadata_compilation.edges > 4);
    PC_CHECK(strict_metadata_json.find(
                 "fixed destructive-renewal policy") ==
             std::string::npos);

    std::uint32_t basin_state = kNoId;
    for (std::uint32_t state = 0;
         state < restricted.policy_reachable.size(); ++state) {
        if (restricted.policy_reachable[state] &&
            restricted_calc.state(state)
                    .goal_progress_retry_basin != 0) {
            basin_state = state;
            break;
        }
    }
    PC_CHECK(basin_state != kNoId);
    if (basin_state != kNoId) {
        const PolicyOperatorRef selected =
            restricted.policy[basin_state];
        PC_CHECK(selected != kNoId);
        const PlannerOperator& planner =
            restricted_calc.operators().at(selected);
        PC_CHECK(planner.kind == PlannerOperatorKind::Primitive);
        PC_CHECK(
            restricted_calc.registry().actions.at(
                planner.primitive_action).params.type ==
            ActionType::Chaos);
        PC_CHECK(action_legal(
            *restricted_session,
            restricted_calc.registry().actions.at(bench_index),
            restricted_calc.state(basin_state)));
    }

    const refinement::PolicyExactLiftCertificate restricted_lifted =
        refinement::lift_policy_exact(
            restricted_calc, restricted, full_start,
            restricted_prices, restricted_options,
            "goal-progress-gated refined renewal");
    report_lift_failure(
        "goal-progress-gated refined renewal",
        restricted_lifted);
    PC_CHECK(
        restricted_lifted.status ==
        refinement::PolicyExactLiftStatus::Complete);
    PC_CHECK(restricted_lifted.executable);
    PC_CHECK(restricted_lifted.compiled.executable);
    PC_CHECK(restricted_lifted.compiled.proper);
    PC_CHECK(restricted_lifted.compiled.zero_off_policy);
    PC_CHECK(restricted_lifted.compiled.cost_reconciled);
    PC_CHECK(near(
        restricted_lifted.compiled.exact_cost,
        restricted.values[restricted.start_state], 1e-9));
    const std::string& restricted_json =
        restricted_lifted.compiled.strategy_json;
    PC_CHECK(restricted_json.find(
                 "\"description\":\"Exact executable fixed "
                 "destructive-renewal policy within the "
                 "zero-progress-reroll restriction\"") !=
             std::string::npos);
    PC_CHECK(restricted.options.goal_progress_gated_reforges);
    PC_CHECK(restricted_json.find(
                 "\"solver_policy_scope\":\""
                 "zero_progress_reroll_policy_restriction\"") !=
             std::string::npos);
    PC_CHECK(restricted_json.find("_gated_route") ==
             std::string::npos);
    const std::shared_ptr<StrategyImpl> restricted_strategy =
        compile_strategy_json(
            restricted_session, restricted_json.data(),
            restricted_json.size());
    auto restricted_economy = std::make_shared<EconomyImpl>();
    restricted_economy->id = "goal-progress-gated-test";
    restricted_economy->prices = restricted_prices;
    StrategyEvalOptions restricted_eval_options;
    restricted_eval_options.economy = restricted_economy;
    const StrategyEvalResult restricted_evaluation =
        evaluate_strategy(
            *restricted_strategy, restricted_eval_options);
    PC_CHECK(restricted_evaluation.converged);
    PC_CHECK(restricted_evaluation.cost_complete);
    PC_CHECK(near(
        restricted_evaluation.success_probability, 1.0, 1e-12));
    PC_CHECK(near(
        restricted_evaluation.failure_probability, 0.0, 1e-12));
    PC_CHECK(near(
        restricted_evaluation.action_not_applied_probability,
        0.0, 1e-12));
    PC_CHECK(near(
        restricted_evaluation.no_matching_edge_probability,
        0.0, 1e-12));
    PC_CHECK(near(
        restricted_evaluation.total_expected_cost,
        restricted.values[restricted.start_state], 1e-9));
    SimulatorImpl restricted_simulator;
    restricted_simulator.session = restricted_session;
    restricted_simulator.strategy = restricted_strategy;
    restricted_simulator.economy = restricted_economy;
    prepare_simulator_runtime(restricted_simulator);
    SimulationOptionsInternal restricted_simulation_options;
    restricted_simulation_options.target_runs = 10000;
    restricted_simulation_options.seed = 0x4750524752455353ULL;
    restricted_simulation_options.max_actions_per_run = 100000;
    run_simulator_chunk(
        restricted_simulator, restricted_simulation_options, 10000);
    if (restricted_simulator.summary.success_count != 10000) {
        std::printf(
            "goal-progress-gated refined simulation: completed=%llu "
            "success=%llu failure=%llu stop=%llu actions=%llu "
            "action_limit=%llu cost_limit=%llu step_limit=%llu "
            "no_edge=%llu not_applied=%llu\n",
            static_cast<unsigned long long>(
                restricted_simulator.summary.completed_runs),
            static_cast<unsigned long long>(
                restricted_simulator.summary.success_count),
            static_cast<unsigned long long>(
                restricted_simulator.summary.failure_count),
            static_cast<unsigned long long>(
                restricted_simulator.summary.stop_count),
            static_cast<unsigned long long>(
                restricted_simulator.summary.total_actions),
            static_cast<unsigned long long>(
                restricted_simulator.summary.action_limit_count),
            static_cast<unsigned long long>(
                restricted_simulator.summary.cost_limit_count),
            static_cast<unsigned long long>(
                restricted_simulator.summary.step_limit_count),
            static_cast<unsigned long long>(
                restricted_simulator.summary.no_matching_edge_count),
            static_cast<unsigned long long>(
                restricted_simulator.summary.action_not_applied_count));
        for (const FailureSummaryInternal& failure :
             restricted_simulator.failure_summaries) {
            std::printf(
                "  failure reason=%d node=%s count=%llu detail=%s\n",
                failure.failure_reason, failure.node_id.c_str(),
                static_cast<unsigned long long>(failure.count),
                failure.detail.c_str());
        }
    }
    PC_CHECK(restricted_simulator.summary.completed_runs == 10000);
    PC_CHECK(restricted_simulator.summary.success_count == 10000);
    PC_CHECK(
        restricted_simulator.summary.action_not_applied_count == 0);
    PC_CHECK(
        restricted_simulator.summary.no_matching_edge_count == 0);

    /*
     * Stop after the start carrier. A completed gated Chaos row must publish
     * the exact fixed policy "repeat Chaos until the goal" before later
     * frontier expansion. The compact compiler independently rechecks every
     * policy-reachable carrier's action-local kernel signature.
     */
    CalcContext early_renewal_calc(
        restricted_session, restricted_goal, restricted_registry,
        {restricted_chaos, bench_index});
    pc_item_state observer_rare;
    pc_item_clear(&observer_rare);
    observer_rare.rarity = PC_RARITY_RARE;
    const std::uint32_t observer_state =
        early_renewal_calc.intern_item(observer_rare);
    PC_CHECK(action_legal(
        *restricted_session,
        restricted_registry.actions.at(bench_index),
        early_renewal_calc.state(observer_state)));
    SolveOptions early_renewal_options;
    early_renewal_options.goal_progress_gated_reforges = true;
    early_renewal_options.high_impact_executable_uppers = true;
    early_renewal_options.max_expanded_states = 1;
    early_renewal_options.state_certificate_control = false;
    const SolveResult early_renewal = solve(
        early_renewal_calc, full_start, restricted_prices,
        early_renewal_options);
    PC_CHECK(!early_renewal.converged);
    PC_CHECK(early_renewal.policy_available);
    PC_CHECK(
        early_renewal.policy_status ==
        SolvePolicyStatus::BoundedFeasible);
    PC_CHECK(
        early_renewal.termination ==
        SolveTermination::RefusedResourceCap);
    PC_CHECK(early_renewal.diagnostics.state_cap_hit);
    PC_CHECK(early_renewal.lower_bound <=
             early_renewal.evaluated_policy_cost + 1e-9);
    PC_CHECK(near(
        early_renewal.evaluated_policy_cost,
        early_renewal.upper_bound, 1e-9));
    PC_CHECK(early_renewal.primitive_renewal_witness.valid);
    PC_CHECK(
        early_renewal.primitive_renewal_witness.primitive_action ==
        restricted_chaos);
    PC_CHECK(
        early_renewal.primitive_renewal_witness.witness_hash != 0);
    PC_CHECK(
        early_renewal.diagnostics
            .gated_root_renewal_validated_non_goal_states ==
        early_renewal.primitive_renewal_witness
            .validated_non_goal_states);
    PC_CHECK(observer_state < early_renewal.policy_reachable.size());
    PC_CHECK(!early_renewal.policy_reachable[observer_state]);
    std::uint64_t reachable_non_goal = 0;
    for (std::uint32_t state = 0;
         state < early_renewal.policy_reachable.size(); ++state) {
        if (early_renewal.policy_reachable[state] &&
            !early_renewal.goal_states[state]) {
            ++reachable_non_goal;
            PC_CHECK(
                early_renewal.policy[state].index ==
                early_renewal.primitive_renewal_witness
                    .operator_index);
        }
    }
    PC_CHECK(
        reachable_non_goal ==
        early_renewal.primitive_renewal_witness
            .validated_non_goal_states);
    const std::string early_telemetry =
        serialize_solver_telemetry(
            early_renewal_calc, &early_renewal, nullptr,
            std::nullopt, nullptr);
    PC_CHECK(valid_json_object(early_telemetry));
    PC_CHECK(
        early_telemetry.find(
            "\"upper_cap_zero_progress_audit\":{"
            "\"version\":1,\"observational\":true") !=
        std::string::npos);
    PC_CHECK(
        early_telemetry.find(
            "\"canonicalization\":{\"existing_retry_basin_states\":1") !=
        std::string::npos);
    PC_CHECK(
        early_telemetry.find(
            "\"merge_applied\":false") !=
        std::string::npos);
    PC_CHECK(
        early_telemetry.find(
            "\"nonrenewal_observer_actions\":[{"
            "\"action\":\"bench:gated_goal\"") !=
        std::string::npos);

    CalcContext interrupted_row_calc(
        restricted_session, restricted_goal, restricted_registry,
        {restricted_chaos});
    SolveOptions interrupted_row_options;
    interrupted_row_options.goal_progress_gated_reforges = true;
    interrupted_row_options.max_reforge_work = 1;
    interrupted_row_options.state_certificate_control = false;
    const SolveResult interrupted_row = solve(
        interrupted_row_calc, full_start, {{"chaos", 1.0}},
        interrupted_row_options);
    PC_CHECK(!interrupted_row.converged);
    PC_CHECK(interrupted_row.diagnostics.resource_cap_hit);
    const auto interrupted_cost =
        interrupted_row.diagnostics.action_search_costs.find("chaos");
    PC_CHECK(
        interrupted_cost !=
        interrupted_row.diagnostics.action_search_costs.end());
    if (interrupted_cost !=
        interrupted_row.diagnostics.action_search_costs.end()) {
        PC_CHECK(interrupted_cost->second.rows == 0);
        PC_CHECK(interrupted_cost->second.reforge_work == 1);
        PC_CHECK(interrupted_cost->second.interrupted_rows == 1);
        PC_CHECK(interrupted_cost->second.last_interrupted_state ==
                 interrupted_row.start_state);
        PC_CHECK(interrupted_cost->second.last_interrupted_root);
        PC_CHECK(interrupted_cost->second.last_interrupted_cursor > 0);
        PC_CHECK(interrupted_cost->second.last_interrupted_cap ==
                 "max_reforge_work");
    }
    const std::string interrupted_telemetry =
        serialize_solver_telemetry(
            interrupted_row_calc, &interrupted_row, nullptr,
            std::nullopt, nullptr);
    PC_CHECK(valid_json_object(interrupted_telemetry));
    PC_CHECK(
        interrupted_telemetry.find(
            "\"action_id\":\"chaos\",\"rows\":0") !=
        std::string::npos);
    PC_CHECK(
        interrupted_telemetry.find(
            "\"interrupted\":{\"rows\":1,\"state\":0,"
            "\"root\":true") != std::string::npos);
    PC_CHECK(
        interrupted_telemetry.find(
            "\"cap\":\"max_reforge_work\"") !=
        std::string::npos);
    PC_CHECK(
        interrupted_telemetry.find(
            "\"resource_accounting\":{\"schema_version\":2") !=
        std::string::npos);
    PC_CHECK(
        interrupted_telemetry.find(
            "\"rows_interrupted\":1") !=
        std::string::npos);
    PC_CHECK(
        interrupted_telemetry.find(
            "\"owner\":\"coarse\",\"family\":\"ordinary\","
            "\"evaluator\":\"v1_raw\",\"cache\":\"miss\"") !=
        std::string::npos);
    PC_CHECK(
        interrupted_telemetry.find(
            "\"disposition\":\"interrupted\"") !=
        std::string::npos);

    PolicyCompilationTelemetry early_compilation;
    const std::string early_json =
        compile_policy_strategy_json(
            early_renewal_calc, early_renewal,
            "early gated destructive renewal",
            &early_compilation);
    PC_CHECK(early_compilation.nodes == 3);
    PC_CHECK(early_compilation.edges == 3);
    PC_CHECK(early_compilation.policy_regions == 1);
    PC_CHECK(early_renewal.primitive_renewal_witness.gated_kernel_bits_hash ==
             early_renewal_calc.outcomes(
                 early_renewal.start_state,
                 early_renewal.primitive_renewal_witness.primitive_action,
                 true).gated_kernel_bits_hash);
    PC_CHECK(early_json.find(
                 "\"from\":\"renewal\",\"to\":\"renewal\"") !=
             std::string::npos);
    PC_CHECK(early_json.find(
                 "\"description\":\"Bounded executable fixed "
                 "destructive-renewal policy exact within the "
                 "zero-progress-reroll restriction") !=
             std::string::npos);
    PC_CHECK(early_json.find(
                 "\"solver_policy_scope\":\""
                 "zero_progress_reroll_policy_restriction\"") !=
             std::string::npos);
    PC_CHECK(early_json.find("_gated_route") == std::string::npos);
    const std::shared_ptr<StrategyImpl> early_strategy =
        compile_strategy_json(
            restricted_session, early_json.data(),
            early_json.size());
    StrategyEvalOptions early_eval_options;
    early_eval_options.economy = restricted_economy;
    const StrategyEvalResult early_evaluation =
        evaluate_strategy(*early_strategy, early_eval_options);
    PC_CHECK(early_evaluation.converged);
    PC_CHECK(early_evaluation.cost_complete);
    PC_CHECK(near(
        early_evaluation.total_expected_cost,
        early_renewal.evaluated_policy_cost, 1e-9));
    SimulatorImpl early_simulator;
    early_simulator.session = restricted_session;
    early_simulator.strategy = early_strategy;
    early_simulator.economy = restricted_economy;
    prepare_simulator_runtime(early_simulator);
    SimulationOptionsInternal early_simulation_options;
    early_simulation_options.target_runs = 10000;
    early_simulation_options.seed = 0x45524c5952454e45ULL;
    early_simulation_options.max_actions_per_run = 100000;
    run_simulator_chunk(
        early_simulator, early_simulation_options, 10000);
    PC_CHECK(early_simulator.summary.completed_runs == 10000);
    PC_CHECK(early_simulator.summary.success_count == 10000);
    PC_CHECK(early_simulator.summary.action_not_applied_count == 0);
    PC_CHECK(early_simulator.summary.no_matching_edge_count == 0);

    SolveResult stale_renewal = early_renewal;
    PC_CHECK(
        !stale_renewal.primitive_renewal_witness
             .kernel_signature.empty());
    stale_renewal.primitive_renewal_witness
        .kernel_signature.front() ^= 1;
    stale_renewal.refined_policy_artifact = {};
    PolicyCompilationTelemetry stale_compilation;
    const std::string stale_renewal_json =
        compile_policy_strategy_json(
            early_renewal_calc, stale_renewal,
            "stale gated destructive renewal",
            &stale_compilation);
    PC_CHECK(valid_json_object(stale_renewal_json));
    PC_CHECK(
        stale_compilation.nodes > 4 ||
        stale_compilation.edges > 4);
    PC_CHECK(stale_renewal_json.find(
                 "fixed destructive-renewal policy") ==
             std::string::npos);

    SolveResult conflicting_renewal = early_renewal;
    conflicting_renewal.refined_policy_artifact = {};
    std::uint32_t conflicting_carrier = kNoId;
    for (std::uint32_t state = 0;
         state < conflicting_renewal.policy_reachable.size(); ++state) {
        if (state != conflicting_renewal.start_state &&
            conflicting_renewal.policy_reachable[state] &&
            !conflicting_renewal.goal_states[state]) {
            conflicting_carrier = state;
            break;
        }
    }
    PC_CHECK(conflicting_carrier != kNoId);
    if (conflicting_carrier != kNoId) {
        conflicting_renewal.policy[conflicting_carrier] =
            PolicyOperatorRef{
                PlannerOperatorKind::Primitive, bench_index};
        PolicyCompilationTelemetry conflicting_compilation;
        const std::string conflicting_json =
            compile_policy_strategy_json(
                early_renewal_calc, conflicting_renewal,
                "conflicting gated destructive renewal",
                &conflicting_compilation);
        PC_CHECK(valid_json_object(conflicting_json));
        PC_CHECK(
            conflicting_compilation.nodes > 4 ||
            conflicting_compilation.edges > 4);
        PC_CHECK(conflicting_json.find(
                     "fixed destructive-renewal policy") ==
                 std::string::npos);
        PC_CHECK(conflicting_json.find(
                     "\"type\":\"bench\"") !=
                 std::string::npos);
    }

    /* Retained progress remains an ordinary exact state. A cheap Bench that
     * finishes a second goal is discoverable there even though it is
     * deliberately unavailable from the zero-progress basin. */
    GoalSpec partial_goal;
    partial_goal.rarity = PC_RARITY_RARE;
    for (const std::uint32_t family : {104u, 100u}) {
        GoalSlot partial_slot;
        partial_slot.family_id = family;
        partial_slot.min_tier = 1;
        partial_goal.slots.push_back(partial_slot);
    }
    CalcContext partial_source(
        restricted_session, partial_goal, restricted_registry,
        {restricted_chaos, bench_index});
    pc_item_state empty_rare;
    pc_item_clear(&empty_rare);
    empty_rare.rarity = PC_RARITY_RARE;
    const std::uint32_t empty_state =
        partial_source.intern_item(empty_rare);
    const OutcomeDistribution& partial_kernel =
        partial_source.outcomes(
            empty_state, restricted_chaos, true);
    std::uint32_t retained_partial = kNoId;
    for (const OutcomeEntry& entry : partial_kernel.entries) {
        const AbstractState& state =
            partial_source.state(entry.state);
        if (state.goal_progress_retry_basin == 0 &&
            !partial_source.is_goal_state(state) &&
            state.slot_status[0] ==
                static_cast<std::uint8_t>(
                    GoalSlotStatus::Satisfied) &&
            state.slot_status[1] ==
                static_cast<std::uint8_t>(
                    GoalSlotStatus::Absent) &&
            (state.blocked_mask & (1u << 1)) == 0 &&
            state.prefix_count < restricted_session->rare_affix_cap) {
            retained_partial = entry.state;
            break;
        }
    }
    PC_CHECK(retained_partial != kNoId);
    if (retained_partial != kNoId) {
        pc_item_state partial_item;
        PC_CHECK(partial_source.materialize(
            retained_partial, partial_item));
        PC_CHECK(action_legal(
            *restricted_session,
            restricted_registry.actions.at(bench_index),
            partial_source.state(retained_partial)));
        const OutcomeDistribution& bench_finish =
            partial_source.outcomes(
                retained_partial, bench_index);
        PC_CHECK(bench_finish.supported);
        PC_CHECK(!bench_finish.entries.empty());
        CalcContext partial_calc(
            restricted_session, partial_goal, restricted_registry,
            {restricted_chaos, bench_index});
        const SolveResult partial_result = solve(
            partial_calc, partial_item,
            {{"chaos", 100.0}, {"bench:gated_goal", 0.001}},
            restricted_options);
        report_solve_issue(
            "retained-progress bench solve",
            partial_result, true);
        PC_CHECK(partial_result.converged);
        const PlannerOperator& partial_selected =
            partial_calc.operators().at(
                partial_result.policy[partial_result.start_state]);
        PC_CHECK(partial_selected.kind ==
                 PlannerOperatorKind::Primitive);
        PC_CHECK(
            partial_calc.telemetry().primitive_families.at(
                static_cast<std::size_t>(
                    PrimitiveTelemetryFamily::Bench)).rows > 0);
    }
}

void run_incremental_action_generation_tests() {
    PC_CHECK(
        solve_detail::globally_certified_action_envelope_lower_bound(
            3759.5969190413853, true, false) == 0.0);
    PC_CHECK(near(
        solve_detail::globally_certified_action_envelope_lower_bound(
            3759.5969190413853, true, true),
        3759.5969190413853));
    PC_CHECK(near(
        solve_detail::globally_certified_action_envelope_lower_bound(
            3759.5969190413853, true, false, 21.772459401332767),
        21.772459401332767));
    PC_CHECK(near(
        solve_detail::globally_certified_action_envelope_lower_bound(
            3759.5969190413853, true, true, 21.772459401332767),
        3759.5969190413853));
    const solve_detail::SolveLowerBoundAuthority open_authority =
        solve_detail::classify_public_lower_bound_authority(
            0.0, SolvePolicyStatus::BoundedFeasible, true, false);
    PC_CHECK(!open_authority.globally_certified);
    PC_CHECK(
        open_authority.provenance ==
        SolveLowerBoundProvenance::
            OpenIncrementalEnvelopeUniversalZero);
    const solve_detail::SolveLowerBoundAuthority strict_open_authority =
        solve_detail::classify_public_lower_bound_authority(
            0.0,
            SolvePolicyStatus::BoundedFeasible,
            true,
            true,
            true);
    PC_CHECK(!strict_open_authority.globally_certified);
    PC_CHECK(
        strict_open_authority.provenance ==
        SolveLowerBoundProvenance::
            UnclosedStrictRefinementUniversalZero);
    const solve_detail::SolveLowerBoundAuthority independent_authority =
        solve_detail::classify_public_lower_bound_authority(
            21.772459401332767,
            SolvePolicyStatus::BoundedFeasible,
            true,
            false,
            true,
            21.772459401332767);
    PC_CHECK(independent_authority.globally_certified);
    PC_CHECK(
        independent_authority.provenance ==
        SolveLowerBoundProvenance::GlobalActionRelaxation);
    const solve_detail::SolveLowerBoundAuthority overclaimed_independent =
        solve_detail::classify_public_lower_bound_authority(
            22.0,
            SolvePolicyStatus::BoundedFeasible,
            true,
            false,
            true,
            21.772459401332767);
    PC_CHECK(!overclaimed_independent.globally_certified);
    PC_CHECK(
        overclaimed_independent.provenance ==
        SolveLowerBoundProvenance::
            UnclosedStrictRefinementUniversalZero);
    const solve_detail::SolveLowerBoundAuthority closed_authority =
        solve_detail::classify_public_lower_bound_authority(
            3759.5969190413853,
            SolvePolicyStatus::BoundedFeasible,
            true,
            true);
    PC_CHECK(closed_authority.globally_certified);
    PC_CHECK(
        closed_authority.provenance ==
        SolveLowerBoundProvenance::ClosedIncrementalActionEnvelope);
    const solve_detail::SolveLowerBoundAuthority global_authority =
        solve_detail::classify_public_lower_bound_authority(
            0.0, SolvePolicyStatus::None, false, false);
    PC_CHECK(global_authority.globally_certified);
    PC_CHECK(
        global_authority.provenance ==
        SolveLowerBoundProvenance::GlobalActionRelaxation);
    const solve_detail::SolveLowerBoundAuthority exact_authority =
        solve_detail::classify_public_lower_bound_authority(
            10.0, SolvePolicyStatus::Exact, true, false);
    PC_CHECK(exact_authority.globally_certified);
    PC_CHECK(
        exact_authority.provenance ==
        SolveLowerBoundProvenance::ExactPolicyClosure);
    PC_CHECK(near(
        q_directed_uncertainty_contribution(
            0.9, 10.0, 12.0),
        1.8));
    PC_CHECK(
        q_directed_uncertainty_contribution(
            0.9, 10.0, 12.0) >
        q_directed_uncertainty_contribution(
            0.1, 0.0, 10.0));
    auto session = make_solve_session(
        {"incremental_goal", "incremental_junk"});
    session->essence_guaranteed_mod_ids = {0, 2};
    ActionRegistry registry = build_action_registry(*session);
    const std::uint32_t good_essence =
        registry.index_by_id.at("essence:incremental_goal");
    const std::uint32_t bad_essence =
        registry.index_by_id.at("essence:incremental_junk");
    const std::uint32_t chaos = registry.index_by_id.at("chaos");

    GoalSpec goal;
    goal.rarity = PC_RARITY_RARE;
    GoalSlot slot;
    slot.family_id = 100;
    slot.min_tier = 1;
    goal.slots.push_back(slot);
    pc_item_state start;
    pc_item_clear(&start);
    start.rarity = PC_RARITY_RARE;
    const std::unordered_map<std::string, double> prices{
        {"chaos", 10.0},
        {"essence:incremental_goal", 0.01},
        {"essence:incremental_junk", 1000.0}};

    CalcContext calc(
        session, goal, registry,
        {chaos, good_essence, bad_essence});
    SolveOptions options;
    options.goal_progress_gated_reforges = true;
    options.focused_expansion_queue_threshold = 1000000;
    SolveWork incremental_work(calc, start, prices, options);
    SolveProgress incremental_progress = incremental_work.progress();
    double prior_published_lower = incremental_progress.lower_bound;
    bool saw_open_envelope = false;
    std::string open_snapshot_telemetry;
    std::uint32_t incremental_work_units = 0;
    while (!incremental_progress.done &&
           incremental_work_units < 100000) {
        incremental_work.step(1);
        incremental_progress = incremental_work.progress();
        const SolveTelemetrySnapshot snapshot =
            incremental_work.telemetry_snapshot();
        PC_CHECK(
            incremental_progress.lower_bound + 1e-9 >=
            prior_published_lower);
        prior_published_lower = incremental_progress.lower_bound;
        if (!saw_open_envelope &&
            snapshot.diagnostics.incremental_action_generation &&
            !snapshot.diagnostics.incremental_action_envelope_closed) {
            saw_open_envelope = true;
            PC_CHECK(incremental_progress.lower_bound == 0.0);
            if (std::isfinite(incremental_progress.upper_bound)) {
                PC_CHECK(near(
                    incremental_progress.absolute_optimality_gap,
                    incremental_progress.upper_bound, 1e-9));
            }
            open_snapshot_telemetry = serialize_solver_telemetry(
                calc, nullptr, &snapshot, std::nullopt, nullptr);
        }
        ++incremental_work_units;
    }
    PC_CHECK(incremental_progress.done);
    PC_CHECK(saw_open_envelope);
    PC_CHECK(valid_json_object(open_snapshot_telemetry));
    PC_CHECK(
        open_snapshot_telemetry.find(
            "\"lower_bound_scope\":\"independent_global_floor\"") !=
        std::string::npos);
    PC_CHECK(
        open_snapshot_telemetry.find(
            "\"restricted_action_envelope_lower_bound\":") !=
        std::string::npos);
    const SolveResult result = incremental_progress.done
        ? incremental_work.finish()
        : SolveResult{};
    std::printf(
        "solver incremental action oracle: converged=%d closed=%d "
        "admitted=%llu rejected=%llu unresolved=%llu unevaluated=%llu "
        "reopts=%llu expanded=%u failure=%s\n",
        result.converged ? 1 : 0,
        result.diagnostics.incremental_action_envelope_closed ? 1 : 0,
        static_cast<unsigned long long>(
            result.diagnostics.incremental_actions_admitted),
        static_cast<unsigned long long>(
            result.diagnostics.incremental_actions_non_improving),
        static_cast<unsigned long long>(
            result.diagnostics.incremental_actions_unresolved),
        static_cast<unsigned long long>(
            result.diagnostics.incremental_actions_unevaluated),
        static_cast<unsigned long long>(
            result.diagnostics.incremental_bellman_reoptimizations),
        result.diagnostics.expanded_states,
        result.diagnostics.policy_evaluation_failure.c_str());
    PC_CHECK(result.converged);
    PC_CHECK(result.policy_status == SolvePolicyStatus::Exact);
    PC_CHECK(result.global_lower_bound_certified);
    PC_CHECK(
        result.lower_bound_provenance ==
        SolveLowerBoundProvenance::ExactPolicyClosure);
    PC_CHECK(result.diagnostics.incremental_action_generation);
    PC_CHECK(
        result.diagnostics.incremental_action_envelope_closed);
    PC_CHECK(result.diagnostics.incremental_actions_admitted > 0);
    PC_CHECK(
        result.diagnostics.incremental_actions_non_improving > 0);
    PC_CHECK(
        result.diagnostics.incremental_bellman_reoptimizations > 0);
    PC_CHECK(
        result.diagnostics.incremental_upper_policy_updates > 0);
    PC_CHECK(
        result.diagnostics
            .incremental_first_alternative_expanded_states > 1);
    PC_CHECK(result.policy[result.start_state].index == good_essence);
    const std::string telemetry = serialize_solver_telemetry(
        calc, &result, nullptr, std::nullopt, nullptr);
    PC_CHECK(valid_json_object(telemetry));
    PC_CHECK(
        telemetry.find("\"incremental_action_envelope\":{"
                       "\"enabled\":true,\"closed\":true") !=
        std::string::npos);
    PC_CHECK(
        telemetry.find("\"global_lower_bound_certified\":true,") !=
        std::string::npos);
    PC_CHECK(
        telemetry.find(
            "\"lower_bound_provenance\":\"exact_policy_closure\"") !=
        std::string::npos);
    PC_CHECK(
        telemetry.find("\"completed_rows_recomputed\":0") !=
        std::string::npos);
    PC_CHECK(
        telemetry.find("\"upper_policy_provenance\":{"
                       "\"observational\":true") !=
        std::string::npos);
    PC_CHECK(
        result.diagnostics.upper_policy_provenance_samples.size() <=
        options.max_diagnostic_samples);
    PC_CHECK(
        result.diagnostics
            .upper_policy_provenance_candidate_count ==
        result.diagnostics.upper_policy_provenance_samples.size() +
            result.diagnostics
                .upper_policy_provenance_samples_omitted);
    for (const std::string& sample :
         result.diagnostics.upper_policy_provenance_samples) {
        PC_CHECK(valid_json_object(sample));
        PC_CHECK(
            sample.find("\"policy_witness\":{") !=
            std::string::npos);
        PC_CHECK(
            sample.find("\"proper\":true") !=
            std::string::npos);
    }

    /*
     * A cheap delayed Essence can improve only as a cooperative multi-row
     * renewal: it guarantees the prefix goal, then retries until the suffix
     * goal rolls. The Chaos-only restricted solve assigns finite values to
     * those partial carriers, but those values are not full-envelope lower
     * bounds. Using them to classify the delayed rows certifies the whole
     * improving Essence family NonImproving and closes on the wrong Chaos
     * policy. While the envelope is open, public progress must retain the
     * independent global floor even though the restricted scheduling value
     * is positive; after complete admission the exact policy must select the
     * Essence at the root.
     */
    auto cooperative_session = make_solve_session(
        {"incremental_cooperative"});
    cooperative_session->essence_guaranteed_mod_ids = {0};
    ActionRegistry cooperative_registry =
        build_action_registry(*cooperative_session);
    const std::uint32_t cooperative_chaos =
        cooperative_registry.index_by_id.at("chaos");
    const std::uint32_t cooperative_essence =
        cooperative_registry.index_by_id.at(
            "essence:incremental_cooperative");
    GoalSpec cooperative_goal;
    cooperative_goal.rarity = PC_RARITY_RARE;
    for (const std::uint32_t family : {100u, 104u}) {
        GoalSlot cooperative_slot;
        cooperative_slot.family_id = family;
        cooperative_slot.min_tier = 1;
        cooperative_goal.slots.push_back(cooperative_slot);
    }
    CalcContext cooperative_calc(
        cooperative_session, cooperative_goal,
        cooperative_registry,
        {cooperative_chaos, cooperative_essence});
    SolveOptions cooperative_options = options;
    cooperative_options.max_diagnostic_samples = 1024;
    SolveWork cooperative_work(
        cooperative_calc, start,
        {{"chaos", 100.0},
         {"essence:incremental_cooperative", 1.0}},
        cooperative_options);
    SolveProgress cooperative_progress = cooperative_work.progress();
    double cooperative_prior_lower = cooperative_progress.lower_bound;
    std::uint32_t cooperative_work_units = 0;
    while (!cooperative_progress.done &&
           cooperative_work_units < 100000) {
        cooperative_work.step(1);
        cooperative_progress = cooperative_work.progress();
        PC_CHECK(
            cooperative_progress.lower_bound + 1e-9 >=
            cooperative_prior_lower);
        cooperative_prior_lower = cooperative_progress.lower_bound;
        ++cooperative_work_units;
    }
    PC_CHECK(cooperative_progress.done);
    if (cooperative_progress.done) {
        const SolveResult cooperative = cooperative_work.finish();
        PC_CHECK(cooperative.converged);
        PC_CHECK(
            cooperative.diagnostics.incremental_action_envelope_closed);
        PC_CHECK(
            cooperative.policy[cooperative.start_state].index ==
            cooperative_essence);
        PC_CHECK(std::any_of(
            cooperative.diagnostics.incremental_action_witnesses.begin(),
            cooperative.diagnostics.incremental_action_witnesses.end(),
            [&](const std::string& witness) {
                return witness.find(
                           "\"state\":" +
                           std::to_string(cooperative.start_state) +
                           ",\"action\":\"essence:incremental_cooperative\"") !=
                           std::string::npos &&
                       witness.find("\"status\":\"admitted\"") !=
                           std::string::npos;
            }));
    }
    CalcContext repeat_calc(
        session, goal, registry,
        {chaos, good_essence, bad_essence});
    const SolveResult repeat =
        solve(repeat_calc, start, prices, options);
    PC_CHECK(identical_solve(result, repeat));
    PC_CHECK(result.diagnostics.transition_bits_hash != 0);
    PC_CHECK(result.diagnostics.policy_bits_hash != 0);
    PC_CHECK(
        result.diagnostics.transition_bits_hash ==
        repeat.diagnostics.transition_bits_hash);
    PC_CHECK(
        result.diagnostics.policy_bits_hash ==
        repeat.diagnostics.policy_bits_hash);
    PC_CHECK(
        result.diagnostics.upper_policy_provenance_samples ==
        repeat.diagnostics.upper_policy_provenance_samples);
    PC_CHECK(
        result.diagnostics
            .upper_policy_provenance_samples_omitted ==
        repeat.diagnostics
            .upper_policy_provenance_samples_omitted);

    CalcContext sampled_calc(
        session, goal, registry,
        {chaos, good_essence, bad_essence});
    SolveOptions sampled_options = options;
    sampled_options.max_diagnostic_samples = 1;
    const SolveResult sampled =
        solve(sampled_calc, start, prices, sampled_options);
    PC_CHECK(
        sampled.diagnostics.upper_policy_provenance_samples.size() <= 1);
    PC_CHECK(
        sampled.diagnostics
            .upper_policy_provenance_candidate_count ==
        sampled.diagnostics.upper_policy_provenance_samples.size() +
            sampled.diagnostics
                .upper_policy_provenance_samples_omitted);

    CalcContext capped_calc(
        session, goal, registry,
        {chaos, good_essence, bad_essence});
    SolveOptions capped_options = options;
    capped_options.max_expanded_states = 1;
    capped_options.max_state_action_rows = 1;
    const SolveResult capped =
        solve(capped_calc, start, prices, capped_options);
    /* The row cap also bounds final-graph evaluator pairs. Compilation is
     * retained diagnostically, but an unverified graph cannot publish. */
    PC_CHECK(!capped.converged);
    PC_CHECK(!capped.policy_available);
    PC_CHECK(capped.policy_status == SolvePolicyStatus::None);
    PC_CHECK(!capped.diagnostics.policy_compatibility_supported);
    PC_CHECK(capped.refined_policy_artifact.strategy_json.empty());
    PC_CHECK(
        capped.diagnostics.policy_refinement.publication_status ==
        "none");
    PC_CHECK(
        capped.diagnostics.policy_publication_failure_reason.find(
            "max_pairs (1)") != std::string::npos);
    PC_CHECK(
        std::any_of(
            capped.diagnostics.policy_refinement
                .publication_candidate_samples.begin(),
            capped.diagnostics.policy_refinement
                .publication_candidate_samples.end(),
            [](const std::string& sample) {
                return sample.find("compiled_unverified") !=
                       std::string::npos;
            }));
    PC_CHECK(
        !capped.diagnostics.incremental_action_envelope_closed);
    PC_CHECK(
        capped.diagnostics.incremental_actions_unresolved +
            capped.diagnostics.incremental_actions_unevaluated >
        0);
    PC_CHECK(capped.termination == SolveTermination::RefusedResourceCap);

    auto delta_session = make_solve_session(
        {"incremental_delta"});
    pc_bitset_clear(
        delta_session->normal_random_roll_mask.data(), 0);
    pc_bitset_clear(
        delta_session->positive_spawn_weight_mask.data(), 0);
    pc_bitset_clear(
        delta_session->positive_base_weight_mask.data(), 0);
    delta_session->base_spawn_weight[0] = 0;
    delta_session->base_roll_weight[0] = 0;
    delta_session->essence_guaranteed_mod_ids = {0};
    ActionRegistry delta_registry =
        build_action_registry(*delta_session);
    const std::uint32_t delta_essence_index =
        delta_registry.index_by_id.at(
            "essence:incremental_delta");
    const std::uint32_t delta_chaos =
        delta_registry.index_by_id.at("chaos");
    GoalSpec delta_goal;
    delta_goal.rarity = PC_RARITY_RARE;
    GoalSlot delta_prefix;
    delta_prefix.family_id = 100;
    delta_prefix.min_tier = 1;
    delta_goal.slots.push_back(delta_prefix);
    GoalSlot delta_suffix;
    delta_suffix.family_id = 104;
    delta_suffix.min_tier = 1;
    delta_goal.slots.push_back(delta_suffix);
    CalcContext delta_calc(
        delta_session, delta_goal, delta_registry,
        {delta_chaos, delta_essence_index});
    const SolveResult delta = solve(
        delta_calc, start,
        {{"chaos", 10.0}, {"essence:incremental_delta", 1.0}},
        options);
    std::printf(
        "solver incremental delta oracle: closed=%d admitted=%llu "
        "rejected=%llu outside=%llu states=%u expanded=%u witnesses=%zu "
        "failure=%s\n",
        delta.diagnostics.incremental_action_envelope_closed ? 1 : 0,
        static_cast<unsigned long long>(
            delta.diagnostics.incremental_actions_admitted),
        static_cast<unsigned long long>(
            delta.diagnostics.incremental_actions_non_improving),
        static_cast<unsigned long long>(
            delta.diagnostics.incremental_states_outside_chaos_support),
        delta.diagnostics.discovered_states,
        delta.diagnostics.expanded_states,
        delta.diagnostics.incremental_action_witnesses.size(),
        delta.diagnostics.policy_evaluation_failure.c_str());
    PC_CHECK(delta.converged);
    PC_CHECK(
        delta.diagnostics.incremental_action_envelope_closed);
    PC_CHECK(
        delta.diagnostics
            .incremental_states_outside_chaos_support > 0);
    for (std::uint32_t state = 0;
         state < delta.expanded.size(); ++state) {
        if (!delta.goal_states[state]) {
            PC_CHECK(delta.expanded[state]);
        }
    }

    auto varying_session = make_solve_session(
        {"incremental_inapplicable"});
    varying_session->essence_guaranteed_mod_ids = {0};
    ActionRegistry varying_registry =
        build_action_registry(*varying_session);
    const std::uint32_t inert_essence_index =
        varying_registry.index_by_id.at(
            "essence:incremental_inapplicable");
    ActionDescriptor& inert_essence =
        varying_registry.actions.at(inert_essence_index);
    inert_essence.legality.rarity_mask =
        1u << PC_RARITY_RARE;
    inert_essence.refinement =
        derive_action_refinement_contract(
            *varying_session, inert_essence);
    validate_action_refinement_contract(inert_essence);
    const std::uint32_t varying_transmute =
        varying_registry.index_by_id.at("transmute");
    const std::uint32_t varying_alteration =
        varying_registry.index_by_id.at("alteration");
    GoalSpec varying_goal;
    varying_goal.rarity = PC_RARITY_MAGIC;
    GoalSlot varying_slot;
    varying_slot.family_id = 100;
    varying_slot.min_tier = 1;
    varying_goal.slots.push_back(varying_slot);
    GoalSlot varying_second_slot;
    varying_second_slot.family_id = 104;
    varying_second_slot.min_tier = 1;
    varying_goal.slots.push_back(varying_second_slot);
    CalcContext varying_calc(
        varying_session, varying_goal, varying_registry,
        {varying_transmute, varying_alteration,
         inert_essence_index});
    pc_item_state varying_start;
    pc_item_clear(&varying_start);
    pc_item_state varying_magic;
    pc_item_clear(&varying_magic);
    varying_magic.rarity = PC_RARITY_MAGIC;
    place(&varying_magic, PC_SIDE_PREFIX, 0, 10);
    const std::uint32_t varying_magic_state =
        varying_calc.intern_item(varying_magic);
    const SolveResult varying = solve(
        varying_calc, varying_start,
        {{"transmute", 1.0},
         {"alteration", 1.0},
         {"essence:incremental_inapplicable", 1000.0}},
        options);
    PC_CHECK(varying.converged);
    PC_CHECK(varying.diagnostics.incremental_action_generation);
    PC_CHECK(
        varying.diagnostics.incremental_action_envelope_closed);
    PC_CHECK(
        varying.policy[varying.start_state].index ==
        varying_transmute);
    PC_CHECK(
        varying.policy[varying_magic_state].index ==
        varying_alteration);
}

void run_resource_stop_reachable_policy_tests() {
    /* A completed stochastic attempt has a goal branch and a retry branch.
     * Restart is an admitted self-loop on the fresh carrier, while the
     * delayed Essence row exhausts the work cap. The completed Transmute row
     * must still yield a bounded executable incumbent while the exact action
     * envelope remains open. */
    auto session = make_solve_session({"anytime_unresolved"});
    session->essence_guaranteed_mod_ids = {3};

    ActionRegistry registry = build_action_registry(*session);
    const std::uint32_t transmute =
        registry.index_by_id.at("transmute");
    const std::uint32_t alchemy =
        registry.index_by_id.at("alchemy");
    const std::uint32_t restart = registry.index_by_id.at("restart");
    const std::uint32_t delayed =
        registry.index_by_id.at("essence:anytime_unresolved");

    GoalSpec goal;
    goal.rarity = PC_RARITY_MAGIC;
    GoalSlot slot;
    slot.family_id = 100;
    slot.min_tier = 1;
    goal.slots.push_back(slot);
    pc_item_state start;
    pc_item_clear(&start);

    CalcContext calc(
        session, goal, registry,
        {transmute, alchemy, restart, delayed});
    SolveOptions options;
    options.goal_progress_gated_reforges = true;
    options.state_certificate_control = false;
    options.focused_expansion_queue_threshold = 1000000;
    options.max_expanded_states = 4;
    options.max_sweeps = 4096;
    const std::unordered_map<std::string, double> prices{
        {"transmute", 10.0},
        {"alchemy", 1000.0},
        {"base", 1.0},
        {"essence:anytime_unresolved", 1000.0}};
    const SolveResult result = solve(
        calc, start, prices, options);

    PC_CHECK(!result.converged);
    PC_CHECK(result.diagnostics.resource_cap_hit);
    PC_CHECK(std::find(
                 result.diagnostics.cap_hits.begin(),
                 result.diagnostics.cap_hits.end(),
                 "max_expanded_states") !=
             result.diagnostics.cap_hits.end());
    PC_CHECK(
        !result.diagnostics.incremental_action_envelope_closed);
    PC_CHECK(
        result.diagnostics.incremental_actions_unevaluated +
            result.diagnostics.incremental_actions_unresolved >
        0);
    PC_CHECK(result.policy_available);
    PC_CHECK(
        result.policy_status == SolvePolicyStatus::BoundedFeasible);
    PC_CHECK(
        result.termination == SolveTermination::RefusedResourceCap);
    PC_CHECK(result.lower_bound == 0.0);
    PC_CHECK(std::isfinite(result.upper_bound));
    PC_CHECK(near(result.upper_bound, result.evaluated_policy_cost, 1e-9));
    PC_CHECK(result.start_state < result.policy.size());
    PC_CHECK(result.policy[result.start_state].index == transmute);
    PC_CHECK(!result.refined_policy_artifact.strategy_json.empty());
    PC_CHECK(
        result.diagnostics.policy_refinement
            .direct_certification_proper);
    PC_CHECK(
        result.diagnostics.policy_refinement
            .direct_certification_zero_off_policy);
    PC_CHECK(
        result.diagnostics.policy_refinement
            .direct_certification_cost_reconciled);
    PC_CHECK(
        result.diagnostics.policy_refinement
            .bounded_publication_retained);
    PC_CHECK(
        result.diagnostics.policy_refinement
            .cheapest_independently_evaluated_selected);
    PC_CHECK(result.diagnostics.expanded_states == 4);
    PC_CHECK(result.diagnostics.policy_reachable_states == 3);
    PC_CHECK(
        result.diagnostics.policy_reachable_states <
        result.diagnostics.expanded_states);
    PC_CHECK(std::find(
                 result.diagnostics.action_inclusion_reasons.begin(),
                 result.diagnostics.action_inclusion_reasons.end(),
                 "included:resource_stop_start_reachable_proper_policy") !=
             result.diagnostics.action_inclusion_reasons.end());

    /* The outer resource-stop estimate is only an entry preflight; the shared
     * fixed-policy evaluator owns larger kernel/component scratch. A cap equal
     * to that entry projection must pass the first check, then stop the actual
     * proof without installing an over-budget incumbent. */
    CalcContext scratch_cap_calc(
        session, goal, registry,
        {transmute, alchemy, restart, delayed});
    SolveWorkTestAccess::Impl scratch_cap_work(
        scratch_cap_calc, start, prices, options);
    while (!scratch_cap_work.progress().done) {
        scratch_cap_work.step(4096);
    }
    PC_CHECK(std::find(
                 scratch_cap_work.result.diagnostics.cap_hits.begin(),
                 scratch_cap_work.result.diagnostics.cap_hits.end(),
                 "max_expanded_states") !=
             scratch_cap_work.result.diagnostics.cap_hits.end());
    PC_CHECK(!scratch_cap_work.output_incumbent.has_value());
    const std::size_t scratch_state_count =
        scratch_cap_calc.state_count();
    const std::uint64_t outer_preflight =
        static_cast<std::uint64_t>(scratch_state_count) *
            (2 * sizeof(double) + 2 * sizeof(std::uint64_t) +
             5 * sizeof(std::uint8_t) + 2 * sizeof(std::uint32_t)) +
        static_cast<std::uint64_t>(
            scratch_cap_work.transition_cache->rows.size()) *
            (sizeof(std::uint8_t) + sizeof(std::uint64_t));
    const std::uint64_t scratch_live =
        scratch_cap_work.fast_estimated_owned_bytes();
    PC_CHECK(
        outer_preflight <=
        std::numeric_limits<std::uint64_t>::max() - scratch_live);
    const std::uint64_t scratch_cap = scratch_live + outer_preflight;
    scratch_cap_work.options.max_solver_owned_bytes = scratch_cap;
    scratch_cap_work.result.options.max_solver_owned_bytes = scratch_cap;
    scratch_cap_work.transition_cache->max_solver_owned_bytes = scratch_cap;
    scratch_cap_calc.refresh_solve_owned_bytes_cap(scratch_cap);
    PC_CHECK(!scratch_cap_work.check_solver_byte_cap_fast(
        outer_preflight));
    PC_CHECK(
        !scratch_cap_work.try_install_resource_stop_reachable_incumbent());
    PC_CHECK(!scratch_cap_work.output_incumbent.has_value());
    PC_CHECK(std::find(
                 scratch_cap_work.result.diagnostics.cap_hits.begin(),
                 scratch_cap_work.result.diagnostics.cap_hits.end(),
                 "max_solver_owned_bytes") !=
             scratch_cap_work.result.diagnostics.cap_hits.end());
    PC_CHECK(std::find(
                 scratch_cap_work.result.diagnostics.cap_hits.begin(),
                 scratch_cap_work.result.diagnostics.cap_hits.end(),
                 "max_expanded_states") !=
             scratch_cap_work.result.diagnostics.cap_hits.end());
    PC_CHECK(std::find(
                 scratch_cap_work.result.diagnostics
                     .action_inclusion_reasons.begin(),
                 scratch_cap_work.result.diagnostics
                     .action_inclusion_reasons.end(),
                 "included:resource_stop_start_reachable_proper_policy") ==
             scratch_cap_work.result.diagnostics
                 .action_inclusion_reasons.end());
    const SolveResult scratch_capped = scratch_cap_work.finish();
    PC_CHECK(!scratch_capped.policy_available);
    PC_CHECK(scratch_capped.policy_status == SolvePolicyStatus::None);
    PC_CHECK(
        scratch_capped.termination ==
        SolveTermination::RefusedResourceCap);
    PC_CHECK(!std::isfinite(scratch_capped.upper_bound));
    PC_CHECK(!std::isfinite(scratch_capped.evaluated_policy_cost));
    PC_CHECK(std::find(
                 scratch_capped.diagnostics.cap_hits.begin(),
                 scratch_capped.diagnostics.cap_hits.end(),
                 "max_solver_owned_bytes") !=
             scratch_capped.diagnostics.cap_hits.end());

    /* A second adaptive cap sits one byte below the measured successful
     * candidate-construction peak. The identical evaluator must finish (same
     * retained scratch peak), while transactional incumbent construction is
     * refused before publication. */
    CalcContext candidate_probe_calc(
        session, goal, registry,
        {transmute, alchemy, restart, delayed});
    SolveWorkTestAccess::Impl candidate_probe_work(
        candidate_probe_calc, start, prices, options);
    while (!candidate_probe_work.progress().done) {
        candidate_probe_work.step(4096);
    }
    const std::uint64_t probe_peak_before =
        candidate_probe_work.peak_owned_bytes;
    PC_CHECK(
        candidate_probe_work
            .try_install_resource_stop_reachable_incumbent());
    PC_CHECK(candidate_probe_work.output_incumbent.has_value());
    const std::uint64_t candidate_construction_peak =
        candidate_probe_work.peak_owned_bytes;
    const std::uint64_t evaluator_scratch_peak =
        candidate_probe_work.peak_policy_scratch_bytes;
    PC_CHECK(candidate_construction_peak > probe_peak_before);
    PC_CHECK(evaluator_scratch_peak > 0);

    CalcContext candidate_cap_calc(
        session, goal, registry,
        {transmute, alchemy, restart, delayed});
    SolveWorkTestAccess::Impl candidate_cap_work(
        candidate_cap_calc, start, prices, options);
    while (!candidate_cap_work.progress().done) {
        candidate_cap_work.step(4096);
    }
    const std::uint64_t candidate_cap =
        candidate_construction_peak - 1;
    candidate_cap_work.options.max_solver_owned_bytes = candidate_cap;
    candidate_cap_work.result.options.max_solver_owned_bytes = candidate_cap;
    candidate_cap_work.transition_cache->max_solver_owned_bytes =
        candidate_cap;
    candidate_cap_calc.refresh_solve_owned_bytes_cap(candidate_cap);
    PC_CHECK(
        !candidate_cap_work
            .try_install_resource_stop_reachable_incumbent());
    PC_CHECK(!candidate_cap_work.output_incumbent.has_value());
    PC_CHECK(
        candidate_cap_work.peak_policy_scratch_bytes ==
        evaluator_scratch_peak);
    PC_CHECK(std::find(
                 candidate_cap_work.result.diagnostics.cap_hits.begin(),
                 candidate_cap_work.result.diagnostics.cap_hits.end(),
                 "max_solver_owned_bytes") !=
             candidate_cap_work.result.diagnostics.cap_hits.end());
}

void run_mixed_side_rare_cap_reporting_regression() {
    auto session = make_solve_session();
    ActionRegistry registry = build_action_registry(*session);
    GoalSpec goal;
    goal.rarity = PC_RARITY_RARE;
    GoalSlot prefix;
    prefix.family_id = 100;
    prefix.min_tier = 1;
    goal.slots.push_back(prefix);
    GoalSlot suffix;
    suffix.family_id = 104;
    suffix.min_tier = 1;
    goal.slots.push_back(suffix);
    const std::uint32_t chaos = registry.index_by_id.at("chaos");
    const std::uint32_t restart = registry.index_by_id.at("restart");
    CalcContext calc(session, goal, registry, {chaos, restart});
    pc_item_state start;
    pc_item_clear(&start);
    start.rarity = PC_RARITY_RARE;
    SolveOptions options;
    options.max_states = 200000;
    options.max_discovered_states = 200000;
    options.max_expanded_states = 1;
    options.goal_progress_gated_reforges = true;
    const SolveResult result = solve(
        calc, start, {{"chaos", 1.0}, {"base", 5.0}}, options);
    PC_CHECK(!result.converged);
    PC_CHECK(result.diagnostics.state_cap_hit);
    PC_CHECK(result.diagnostics.resource_cap_hit);
    PC_CHECK(result.termination == SolveTermination::RefusedResourceCap);
    PC_CHECK(std::find(
                 result.diagnostics.cap_hits.begin(),
                 result.diagnostics.cap_hits.end(),
                 "max_expanded_states") !=
             result.diagnostics.cap_hits.end());
    if (result.policy_available) {
        PC_CHECK(
            result.policy_status ==
                SolvePolicyStatus::BoundedFeasible ||
            result.policy_status ==
                SolvePolicyStatus::BoundedNearOptimal);
    } else {
        PC_CHECK(result.policy_status == SolvePolicyStatus::None);
    }
    const std::string telemetry = serialize_solver_telemetry(
        calc, &result, nullptr, std::nullopt, nullptr);
    PC_CHECK(valid_json_object(telemetry));
    PC_CHECK(
        telemetry.find("\"max_expanded_states\"") !=
        std::string::npos);

    GoalSpec product_goal = goal;
    product_goal.automatic_candidates = true;
    CalcContext product_calc(
        session, product_goal, registry, {chaos, restart});
    SolveOptions reforge_cap_options;
    reforge_cap_options.max_reforge_work = 1;
    reforge_cap_options.goal_progress_gated_reforges = true;
    reforge_cap_options.state_certificate_control = false;
    const SolveResult reforge_capped = solve(
        product_calc, start,
        {{"chaos", 1.0}, {"base", 5.0}},
        reforge_cap_options);
    PC_CHECK(!reforge_capped.converged);
    PC_CHECK(reforge_capped.diagnostics.resource_cap_hit);
    PC_CHECK(
        reforge_capped.termination ==
        SolveTermination::RefusedResourceCap);
    PC_CHECK(std::find(
                 reforge_capped.diagnostics.cap_hits.begin(),
                 reforge_capped.diagnostics.cap_hits.end(),
                 "max_reforge_work") !=
             reforge_capped.diagnostics.cap_hits.end());

    CalcContext prompt_calc(
        session, goal, registry, {chaos, restart});
    SolveOptions prompt_options;
    prompt_options.max_expanded_states = 9;
    prompt_options.goal_progress_gated_reforges = true;
    prompt_options.state_certificate_control = false;
    prompt_options.focused_expansion_checkpoint = 1;
    prompt_options.focused_expansion_queue_threshold = 0;
    SolveWork prompt_work(
        prompt_calc, start,
        {{"chaos", 1.0}, {"base", 5.0}},
        prompt_options);
    SolveProgress prompt_progress = prompt_work.progress();
    std::uint32_t prompt_work_units = 0;
    while (!prompt_progress.done && prompt_work_units < 100000) {
        prompt_work.step(1);
        prompt_progress = prompt_work.progress();
        ++prompt_work_units;
        if (prompt_progress.expanded_states >=
            prompt_options.max_expanded_states) {
            break;
        }
    }
    std::uint32_t prompt_finalization_units = 0;
    while (!prompt_progress.done &&
           prompt_progress.expanded_states ==
               prompt_options.max_expanded_states &&
           prompt_finalization_units < 4096) {
        prompt_work.step(1);
        prompt_progress = prompt_work.progress();
        ++prompt_work_units;
        ++prompt_finalization_units;
    }
    PC_CHECK(prompt_finalization_units < 4096);
    PC_CHECK(
        prompt_progress.expanded_states ==
        prompt_options.max_expanded_states);
    PC_CHECK(prompt_progress.done);
    if (prompt_progress.done) {
        const SolveResult prompt_result = prompt_work.finish();
        PC_CHECK(prompt_result.diagnostics.state_cap_hit);
        PC_CHECK(prompt_result.diagnostics.resource_cap_hit);
        PC_CHECK(std::find(
                     prompt_result.diagnostics.cap_hits.begin(),
                     prompt_result.diagnostics.cap_hits.end(),
                     "max_expanded_states") !=
                 prompt_result.diagnostics.cap_hits.end());
    }
}

void run_automatic_imprint_cooperative_tests() {
    const auto make_imprint_session = [](
        const std::vector<std::string>& essence_keys) {
        auto session = make_solve_session(essence_keys);
        auto data = std::make_shared<DataImpl>(*session->data);
        BestiaryActionDescriptor create;
        create.global_action_id = 20;
        create.global_recipe_id = 10;
        create.id = "bestiary:imprint";
        create.display_name = "Create Imprint";
        create.operation = BestiaryOperationKind::Create;
        create.rarity_mask = 1u << PC_RARITY_MAGIC;
        create.forbidden_item_flags =
            PC_ITEM_CORRUPTED | PC_ITEM_MIRRORED;
        create.checkpoint_requirement =
            BestiaryCheckpointRequirement::Absent;
        create.checkpoint_effect = BestiaryCheckpointEffect::Create;
        create.identity_requirement =
            BestiaryIdentityRequirement::CurrentItem;
        create.cost_keys = {
            "beast:craicic-croaker", "beast:rare", "beast:rare",
            "beast:rare"};
        BestiaryActionDescriptor restore;
        restore.global_action_id = 21;
        restore.global_recipe_id = 10;
        restore.id = "bestiary:restore_imprint";
        restore.display_name = "Restore Imprint";
        restore.operation = BestiaryOperationKind::Restore;
        restore.rarity_mask = 0x7;
        restore.forbidden_item_flags =
            PC_ITEM_CORRUPTED | PC_ITEM_MIRRORED;
        restore.checkpoint_requirement =
            BestiaryCheckpointRequirement::Present;
        restore.checkpoint_effect = BestiaryCheckpointEffect::Consume;
        restore.identity_requirement =
            BestiaryIdentityRequirement::SameItem;
        data->bestiary_actions = {create, restore};
        data->bestiary_action_by_id = {
            {"bestiary:imprint", 0},
            {"bestiary:restore_imprint", 1}};
        data->count_bestiary_actions = 2;
        session->data = std::move(data);
        return session;
    };

    auto session = make_imprint_session({});
    ActionRegistry registry = build_action_registry(*session);
    GoalSpec goal;
    goal.rarity = PC_RARITY_RARE;
    goal.automatic_candidates = true;
    GoalSlot carrier;
    carrier.group_id = session->primary_group.at(0);
    GoalSlot target;
    target.family_id = session->family_id.at(5);
    goal.slots = {carrier, target};
    const std::vector<std::uint32_t> candidates{
        registry.index_by_id.at("augment"),
        registry.index_by_id.at("regal")};
    const std::unordered_map<std::string, double> prices{
        {"augment", 0.1},
        {"alteration", 0.2},
        {"chaos", 1.0},
        {"regal", 1.0},
        {"scour", 0.05},
        {"beast:craicic-croaker", 10.0},
        {"beast:rare", 0.01}};
    AutomaticAdmissionLimits limits;
    limits.max_solver_owned_bytes = 256ull * 1024ull * 1024ull;
    limits.max_imprint_program_depth = 3;
    limits.max_imprint_program_work = 16;
    limits.prices = &prices;
    pc_item_state magic;
    pc_item_clear(&magic);
    magic.rarity = PC_RARITY_MAGIC;
    place(
        &magic, PC_SIDE_PREFIX, 0,
        static_cast<std::uint16_t>(session->primary_group.at(0)));

    const auto admitted_imprint_programs = [](
        const CalcContext& calc,
        const StateLocalAutomaticBatch& batch) {
        std::vector<std::vector<std::string>> programs;
        for (const StateLocalAutomaticCandidate& decision :
             batch.decisions) {
            if (!decision.admitted ||
                decision.kind != AutomaticCandidateKind::Imprint ||
                decision.operator_index == kNoId ||
                decision.operator_index >= calc.operators().size()) {
                continue;
            }
            programs.push_back(
                calc.operators()
                    .at(decision.operator_index)
                    .primitive_program_action_ids);
        }
        std::sort(programs.begin(), programs.end());
        return programs;
    };
    const auto find_imprint_operator = [](
        const CalcContext& calc,
        const StateLocalAutomaticBatch& batch,
        const std::vector<std::string>& program) {
        for (const StateLocalAutomaticCandidate& decision :
             batch.decisions) {
            if (!decision.admitted ||
                decision.kind != AutomaticCandidateKind::Imprint ||
                decision.operator_index == kNoId ||
                decision.operator_index >= calc.operators().size()) {
                continue;
            }
            if (calc.operators()
                    .at(decision.operator_index)
                    .primitive_program_action_ids == program) {
                return decision.operator_index;
            }
        }
        return kNoId;
    };
    const auto success_probability = [](const OptionKernel& kernel) {
        double success = 0.0;
        for (const OutcomeEntry& exit : kernel.exits) {
            if (exit.state != kNoId) success += exit.probability;
        }
        return success;
    };

    /* The caller scope switch excludes only generated Imprint programs and
     * does not turn their work cap into an unresolved obligation. */
    AutomaticAdmissionLimits no_imprint_limits = limits;
    no_imprint_limits.consider_imprint_programs = false;
    CalcContext no_imprint_calc(
        session, goal, registry, candidates,
        false, true, false, std::nullopt, {}, false);
    const std::uint32_t no_imprint_state =
        no_imprint_calc.intern_item(magic);
    const StateLocalAutomaticBatch no_imprint_batch =
        no_imprint_calc.admit_state_local_automatic_candidates(
            no_imprint_state, no_imprint_limits);
    PC_CHECK(
        no_imprint_batch.status ==
        StateLocalAutomaticBatchStatus::Complete);
    PC_CHECK(
        no_imprint_batch.phases.imprint_programs_evaluated == 0);
    PC_CHECK(
        no_imprint_batch.phases.imprint_action_state_evaluations == 0);
    PC_CHECK(
        no_imprint_batch.phases.imprint_outcomes_merged == 0);
    PC_CHECK(
        admitted_imprint_programs(
            no_imprint_calc, no_imprint_batch).empty());

    /* Use the strict parent as the deterministic complete-envelope reference.
     * The retained local admission context always distinguishes exclusion
     * effects independently of its parent's product setting. */
    CalcContext strict_calc(
        session, goal, registry, candidates,
        false, true, false, std::nullopt, {}, false);
    const std::uint32_t strict_state = strict_calc.intern_item(magic);
    const StateLocalAutomaticBatch strict_batch =
        strict_calc.admit_state_local_automatic_candidates(
            strict_state, limits);
    PC_CHECK(
        strict_batch.status == StateLocalAutomaticBatchStatus::Complete);
    PC_CHECK(!strict_batch.cached);
    PC_CHECK(strict_batch.phases.imprint_programs_evaluated > 0);
    PC_CHECK(strict_batch.phases.imprint_action_state_evaluations > 0);
    PC_CHECK(strict_batch.phases.imprint_outcomes_merged > 0);
    PC_CHECK(strict_batch.phases.imprint_max_atomic_outcomes_ns > 0);

    const std::vector<std::string> augment_program{"augment"};
    const std::vector<std::string> augment_regal_program{
        "augment", "regal"};
    const std::uint32_t strict_augment = find_imprint_operator(
        strict_calc, strict_batch, augment_program);
    const std::uint32_t strict_augment_regal = find_imprint_operator(
        strict_calc, strict_batch, augment_regal_program);
    PC_CHECK(strict_augment != kNoId);
    PC_CHECK(strict_augment_regal != kNoId);
    if (strict_augment != kNoId && strict_augment_regal != kNoId) {
        const OptionKernel& one_step = strict_calc.option_kernel(
            strict_state, strict_augment);
        const OptionKernel& two_step = strict_calc.option_kernel(
            strict_state, strict_augment_regal);
        PC_CHECK(success_probability(one_step) > 0.0);
        PC_CHECK(success_probability(two_step) >
                 success_probability(one_step));
    }
    for (const std::uint32_t action : candidates) {
        bool can_hit_missing_goal = false;
        for (const OutcomeEntry& outcome :
             strict_calc.outcomes(strict_state, action).entries) {
            can_hit_missing_goal |=
                strict_calc.state(outcome.state).slot_status[1] ==
                static_cast<std::uint8_t>(GoalSlotStatus::Satisfied);
        }
        PC_CHECK(can_hit_missing_goal);
    }

    /* Prefix composition must remain bit-identical to replay for a longer
     * mixed-rarity program, not only for repeated renewals. This seven-step
     * Alteration/Scour/Transmute control crosses magic and normal legality
     * boundaries three times and compares the complete exact support after
     * every one-step continuation with a single full-program execution. */
    const std::uint32_t mixed_alteration =
        registry.index_by_id.at("alteration");
    const std::uint32_t mixed_scour =
        registry.index_by_id.at("scour");
    const std::uint32_t mixed_transmute =
        registry.index_by_id.at("transmute");
    const std::vector<std::uint32_t> mixed_program{
        mixed_alteration, mixed_scour, mixed_transmute,
        mixed_alteration, mixed_scour, mixed_transmute,
        mixed_alteration};
    CalcContext mixed_calc(
        session, goal, registry,
        {mixed_alteration, mixed_scour, mixed_transmute},
        false, true, false, std::nullopt, {}, false);
    const std::uint32_t mixed_entry = mixed_calc.intern_item(magic);
    const AttemptKernel mixed_replay = execute_attempt(
        mixed_calc, mixed_program, mixed_entry);
    PC_CHECK(mixed_replay.supported);
    PC_CHECK(mixed_replay.fully_legal);
    std::vector<OutcomeEntry> mixed_support{{mixed_entry, 1.0}};
    for (const std::uint32_t action : mixed_program) {
        auto task = execute_attempt_cooperatively(
            mixed_calc, {action}, std::move(mixed_support));
        while (!task.resume()) {
        }
        AttemptKernel extension = task.take_result();
        task.reset();
        PC_CHECK(extension.supported);
        PC_CHECK(extension.fully_legal);
        PC_CHECK(extension.choice_groups.empty());
        PC_CHECK(extension.choice_options.empty());
        mixed_support = std::move(extension.entries);
    }
    PC_CHECK(mixed_replay.entries == mixed_support);

    /* A second state-independent Alteration exactly erases the first but
     * consumes a strict superset of its resources. Bit-identical per-carrier
     * and composed canonical entries are an exact dominance certificate, so
     * the repeated prefix and every extension disappear without a depth
     * claim. */
    const std::vector<std::uint32_t> renewal_candidates{
        registry.index_by_id.at("alteration")};
    CalcContext renewal_calc(
        session, goal, registry, renewal_candidates,
        false, true, false, std::nullopt, {}, false);
    const StateLocalAutomaticBatch renewal_batch =
        renewal_calc.admit_state_local_automatic_candidates(
            renewal_calc.intern_item(magic), limits);
    PC_CHECK(
        renewal_batch.status ==
        StateLocalAutomaticBatchStatus::Complete);
    PC_CHECK(
        renewal_batch.phases
            .imprint_distribution_dominated_programs > 0);
    const auto renewal_programs =
        admitted_imprint_programs(renewal_calc, renewal_batch);
    PC_CHECK(!renewal_programs.empty());
    PC_CHECK(std::all_of(
        renewal_programs.begin(), renewal_programs.end(),
        [](const std::vector<std::string>& program) {
            return program ==
                   std::vector<std::string>{"alteration"};
        }));

    /* With a one-slot rare goal, repeat Alteration until the exact target is
     * present and then Regal is a proper carrier-local policy. Its exact
     * renewal signature, downward-rounded success probability, fully
     * goal-terminal Regal support, and outward-rounded prices certify an
     * upper without a pre-existing sparse-row incumbent. The checkpoint
     * route is already dearer at its mandatory first step and closes before
     * an Imprint program leaf is evaluated. */
    GoalSpec renewal_finish_goal;
    renewal_finish_goal.rarity = PC_RARITY_RARE;
    renewal_finish_goal.automatic_candidates = true;
    renewal_finish_goal.slots = {target};
    const std::vector<std::uint32_t> renewal_finish_candidates{
        registry.index_by_id.at("alteration"),
        registry.index_by_id.at("regal")};
    AutomaticAdmissionLimits renewal_finish_limits = limits;
    renewal_finish_limits.max_imprint_program_depth = 1;
    renewal_finish_limits.max_imprint_program_work = 64;
    CalcContext renewal_finish_calc(
        session, renewal_finish_goal, registry,
        renewal_finish_candidates,
        false, true, false, std::nullopt, {}, false);
    const StateLocalAutomaticBatch renewal_finish_batch =
        renewal_finish_calc.admit_state_local_automatic_candidates(
            renewal_finish_calc.intern_item(magic),
            renewal_finish_limits);
    PC_CHECK(
        renewal_finish_batch.status ==
        StateLocalAutomaticBatchStatus::Complete);
    PC_CHECK(
        renewal_finish_batch.phases
            .imprint_price_bound_complete_carriers == 1);
    PC_CHECK(
        renewal_finish_batch.phases.imprint_price_pruned_programs > 0);
    PC_CHECK(
        renewal_finish_batch.phases.imprint_programs_evaluated == 0);

    /* A finite certified carrier upper plus strictly positive grammar prices
     * replaces an arbitrary depth cutoff. Depth one is intentionally too
     * small to see the repeated Alteration representative; exact full-kernel
     * Pareto dominance closes its subtree at depth two. */
    AutomaticAdmissionLimits price_closed_limits = limits;
    price_closed_limits.max_imprint_program_depth = 1;
    price_closed_limits.incumbent_upper_bound = 10.5;
    CalcContext price_closed_calc(
        session, goal, registry, renewal_candidates,
        false, true, false, std::nullopt, {}, false);
    const StateLocalAutomaticBatch price_closed_batch =
        price_closed_calc.admit_state_local_automatic_candidates(
            price_closed_calc.intern_item(magic), price_closed_limits);
    PC_CHECK(
        price_closed_batch.status ==
        StateLocalAutomaticBatchStatus::Complete);
    PC_CHECK(
        price_closed_batch.phases
            .imprint_price_bound_complete_carriers == 1);
    PC_CHECK(
        price_closed_batch.phases
            .imprint_price_bound_max_program_depth >= 2);
    PC_CHECK(
        price_closed_batch.phases.imprint_max_evaluated_depth >= 2);
    PC_CHECK(
        price_closed_batch.phases.imprint_max_evaluated_depth <=
        price_closed_batch.phases
            .imprint_price_bound_max_program_depth);
    PC_CHECK(
        price_closed_batch.phases
            .imprint_distribution_dominated_programs > 0);
    PC_CHECK(
        price_closed_batch.phases.imprint_max_frontier_size > 0);

    /* The checkpoint plus the first primitive already exceeds this certified
     * upper. Close without evaluating an outcome leaf and expose the exact
     * branch-and-bound decision separately from kernel Pareto pruning. */
    AutomaticAdmissionLimits price_pruned_limits = price_closed_limits;
    price_pruned_limits.incumbent_upper_bound = 10.15;
    CalcContext price_pruned_calc(
        session, goal, registry, renewal_candidates,
        false, true, false, std::nullopt, {}, false);
    const StateLocalAutomaticBatch price_pruned_batch =
        price_pruned_calc.admit_state_local_automatic_candidates(
            price_pruned_calc.intern_item(magic), price_pruned_limits);
    PC_CHECK(
        price_pruned_batch.status ==
        StateLocalAutomaticBatchStatus::Complete);
    PC_CHECK(
        price_pruned_batch.phases.imprint_price_pruned_programs > 0);
    PC_CHECK(
        price_pruned_batch.phases.imprint_programs_evaluated == 0);
    PC_CHECK(
        price_pruned_batch.phases
            .imprint_price_bound_complete_carriers == 1);

    /* A nonpositive reachable grammar step cannot use the finite-price proof.
     * The ordinary depth boundary remains honest and transactional: no
     * partial option is adopted or cached. */
    auto zero_prices = prices;
    zero_prices["alteration"] = 0.0;
    AutomaticAdmissionLimits zero_price_limits = price_closed_limits;
    zero_price_limits.prices = &zero_prices;
    CalcContext zero_price_calc(
        session, goal, registry, renewal_candidates,
        false, true, false, std::nullopt, {}, false);
    const StateLocalAutomaticBatch zero_price_batch =
        zero_price_calc.admit_state_local_automatic_candidates(
            zero_price_calc.intern_item(magic), zero_price_limits);
    PC_CHECK(
        zero_price_batch.status ==
        StateLocalAutomaticBatchStatus::ResourceDeferred);
    PC_CHECK(
        zero_price_batch.resource_cap ==
        "max_imprint_program_depth");
    PC_CHECK(zero_price_batch.admitted_operators.empty());
    PC_CHECK(
        zero_price_batch.phases
            .imprint_price_bound_complete_carriers == 0);

    /* Static direct-producer reachability is only a future suffix-cost lower
     * bound. It must not gate exact terminal exits: the deterministic cleanup
     * cannot create the target, but it preserves target hits made by Augment.
     * A depth-one safety setting must still discover the length-two program
     * when the positive-price proof owns closure. */
    const std::uint32_t remove_crafted =
        registry.index_by_id.at("remove_crafted_modifiers");
    pc_item_state crafted_magic = magic;
    crafted_magic.prefixes[0].flags |= PC_MOD_SLOT_CRAFTED;
    CalcContext preserver_calc(
        session, goal, registry,
        {registry.index_by_id.at("augment"), remove_crafted},
        false, true, false, std::nullopt, {}, false);
    AutomaticAdmissionLimits preserver_limits = limits;
    preserver_limits.max_imprint_program_depth = 1;
    preserver_limits.max_imprint_program_work = 64;
    preserver_limits.incumbent_upper_bound = 10.5;
    const StateLocalAutomaticBatch preserver_batch =
        preserver_calc.admit_state_local_automatic_candidates(
            preserver_calc.intern_item(crafted_magic),
            preserver_limits);
    PC_CHECK(
        preserver_batch.status ==
        StateLocalAutomaticBatchStatus::Complete);
    const auto preserver_programs =
        admitted_imprint_programs(preserver_calc, preserver_batch);
    PC_CHECK(std::find(
                 preserver_programs.begin(), preserver_programs.end(),
                 std::vector<std::string>{
                     "augment", "remove_crafted_modifiers"}) !=
             preserver_programs.end());
    PC_CHECK(
        preserver_batch.phases.imprint_max_evaluated_depth >= 2);

    /* The influence-only target has default weight zero but a positive active
     * selector row. The global base-positive reachability mask therefore
     * omits it even though an influenced carrier can produce it exactly. */
    auto influenced_session = make_imprint_session({});
    auto influenced_data =
        std::const_pointer_cast<DataImpl>(influenced_session->data);
    influenced_data->spawn_offsets = {0, 1, 2, 3, 4, 5, 7, 8, 9};
    influenced_data->spawn_tag_ids = {0, 0, 0, 0, 0, 42, 0, 0, 0};
    influenced_data->spawn_weights =
        {100, 100, 100, 100, 100, 100, 0, 100, 400};
    influenced_data->influence_exalt_name_by_code = {"", "warlord"};
    influenced_data->gen_offsets = {0, 1, 2, 3, 4, 5, 6, 7, 8};
    influenced_data->gen_tag_ids.assign(8, 0);
    influenced_data->gen_weights.assign(8, 100);
    influenced_session->influence_code[5] = 1;
    influenced_session->base_spawn_weight[5] = 0;
    influenced_session->base_roll_weight[5] = 0;
    pc_bitset_clear(
        influenced_session->positive_spawn_weight_mask.data(), 5);
    pc_bitset_clear(
        influenced_session->positive_base_weight_mask.data(), 5);
    influenced_session->selector_tag_by_influence = {-1, 42};
    influenced_session->influence_masks.resize(
        2, std::vector<std::uint64_t>(influenced_session->words, 0));
    pc_bitset_clear(
        influenced_session->influence_masks[0].data(), 5);
    pc_bitset_set(
        influenced_session->influence_masks[1].data(), 5);
    ActionRegistry influenced_registry =
        build_action_registry(*influenced_session);
    const std::uint32_t influenced_alteration =
        influenced_registry.index_by_id.at("alteration");
    const std::vector<std::uint64_t> base_reachability =
        action_explicit_affix_reachable_mask(
            *influenced_session,
            influenced_registry.actions.at(influenced_alteration));
    PC_CHECK(!pc_bitset_test(base_reachability.data(), 5));
    pc_item_state influenced_magic = magic;
    influenced_magic.generic_influence_bits = 1;
    CalcContext influenced_calc(
        influenced_session, goal, influenced_registry,
        {influenced_alteration},
        false, true, false, std::nullopt, {}, false);
    const std::uint32_t influenced_state =
        influenced_calc.intern_item(influenced_magic);
    bool exact_influenced_target = false;
    for (const OutcomeEntry& outcome :
         influenced_calc.outcomes(
             influenced_state, influenced_alteration).entries) {
        exact_influenced_target |=
            influenced_calc.state(outcome.state).slot_status[1] ==
            static_cast<std::uint8_t>(GoalSlotStatus::Satisfied);
    }
    PC_CHECK(exact_influenced_target);
    const StateLocalAutomaticBatch influenced_batch =
        influenced_calc.admit_state_local_automatic_candidates(
            influenced_state, limits);
    PC_CHECK(
        influenced_batch.status ==
        StateLocalAutomaticBatchStatus::Complete);
    const auto influenced_programs =
        admitted_imprint_programs(influenced_calc, influenced_batch);
    PC_CHECK(std::find(
                 influenced_programs.begin(),
                 influenced_programs.end(),
                 std::vector<std::string>{"alteration"}) !=
             influenced_programs.end());
    PC_CHECK(
        influenced_batch.phases
            .imprint_distribution_dominated_programs > 0);

    /* A certified upper may exceed the fallback safety depth on an already
     * influenced carrier: the active selector makes cheap Alteration a real
     * direct producer, so exact discovery must retain it and close by the
     * economic/Pareto proof rather than falsely requiring another Influence
     * Exalt. */
    AutomaticAdmissionLimits influenced_price_limits = limits;
    influenced_price_limits.max_imprint_program_depth = 1;
    influenced_price_limits.incumbent_upper_bound = 10.5;
    CalcContext influenced_price_calc(
        influenced_session, goal, influenced_registry,
        {influenced_alteration},
        false, true, false, std::nullopt, {}, false);
    const StateLocalAutomaticBatch influenced_price_batch =
        influenced_price_calc.admit_state_local_automatic_candidates(
            influenced_price_calc.intern_item(influenced_magic),
            influenced_price_limits);
    PC_CHECK(
        influenced_price_batch.status ==
        StateLocalAutomaticBatchStatus::Complete);
    PC_CHECK(
        influenced_price_batch.phases
            .imprint_price_bound_complete_carriers == 1);
    const auto influenced_price_programs = admitted_imprint_programs(
        influenced_price_calc, influenced_price_batch);
    PC_CHECK(std::find(
                 influenced_price_programs.begin(),
                 influenced_price_programs.end(),
                 std::vector<std::string>{"alteration"}) !=
             influenced_price_programs.end());

    /* Conversely, an uninfluenced carrier cannot use an ordinary pool roll
     * to reach an influence-only target until the corresponding Influence
     * Exalt has been paid. Keep mod5 in the base-positive reachability mask to
     * reproduce canonical rows that look globally positive while mask0 still
     * excludes them at runtime. The carrier-aware producer lower must price
     * Warlord Exalt, prune the root, and evaluate no program leaf. */
    /* The active-selector controls above are complete; reuse their synthetic
     * session for the complementary uninfluenced/base-positive control rather
     * than relying on SessionImpl copyability. */
    auto uninfluenced_bound_session = influenced_session;
    uninfluenced_bound_session->base_spawn_weight[5] = 100;
    uninfluenced_bound_session->base_roll_weight[5] = 100;
    pc_bitset_set(
        uninfluenced_bound_session->positive_spawn_weight_mask.data(), 5);
    pc_bitset_set(
        uninfluenced_bound_session->positive_base_weight_mask.data(), 5);
    ActionRegistry uninfluenced_bound_registry =
        build_action_registry(*uninfluenced_bound_session);
    const std::uint32_t uninfluenced_alteration =
        uninfluenced_bound_registry.index_by_id.at("alteration");
    const std::uint32_t uninfluenced_regal =
        uninfluenced_bound_registry.index_by_id.at("regal");
    const std::uint32_t warlord_exalt =
        uninfluenced_bound_registry.index_by_id.at(
            "influence_exalt:warlord");
    const std::vector<std::uint64_t> misleading_global_reach =
        action_explicit_affix_reachable_mask(
            *uninfluenced_bound_session,
            uninfluenced_bound_registry.actions.at(
                uninfluenced_alteration));
    PC_CHECK(pc_bitset_test(misleading_global_reach.data(), 5));
    CalcContext uninfluenced_exact_calc(
        uninfluenced_bound_session, goal,
        uninfluenced_bound_registry,
        {uninfluenced_alteration, uninfluenced_regal, warlord_exalt},
        false, true, false, std::nullopt, {}, false);
    const std::uint32_t uninfluenced_magic_state =
        uninfluenced_exact_calc.intern_item(magic);
    const OutcomeDistribution& uninfluenced_alteration_outcomes =
        uninfluenced_exact_calc.outcomes(
            uninfluenced_magic_state, uninfluenced_alteration);
    PC_CHECK(std::none_of(
        uninfluenced_alteration_outcomes.entries.begin(),
        uninfluenced_alteration_outcomes.entries.end(),
        [&](const OutcomeEntry& outcome) {
            return uninfluenced_exact_calc.state(outcome.state)
                       .slot_status[1] ==
                   static_cast<std::uint8_t>(
                       GoalSlotStatus::Satisfied);
        }));
    auto uninfluenced_prices = prices;
    uninfluenced_prices["influence_exalt:warlord"] = 5.0;
    AutomaticAdmissionLimits uninfluenced_bound_limits = limits;
    uninfluenced_bound_limits.prices = &uninfluenced_prices;
    uninfluenced_bound_limits.max_imprint_program_depth = 1;
    uninfluenced_bound_limits.max_imprint_program_work = 64;
    uninfluenced_bound_limits.incumbent_upper_bound = 15.0;
    CalcContext uninfluenced_bound_calc(
        uninfluenced_bound_session, goal,
        uninfluenced_bound_registry,
        {uninfluenced_alteration, uninfluenced_regal, warlord_exalt},
        false, true, false, std::nullopt, {}, false);
    const StateLocalAutomaticBatch uninfluenced_bound_batch =
        uninfluenced_bound_calc.admit_state_local_automatic_candidates(
            uninfluenced_bound_calc.intern_item(magic),
            uninfluenced_bound_limits);
    PC_CHECK(
        uninfluenced_bound_batch.status ==
        StateLocalAutomaticBatchStatus::Complete);
    PC_CHECK(
        uninfluenced_bound_batch.phases.imprint_programs_evaluated == 0);
    PC_CHECK(
        uninfluenced_bound_batch.phases.imprint_price_pruned_programs > 0);
    PC_CHECK(
        uninfluenced_bound_batch.phases
            .imprint_price_bound_complete_carriers == 1);
    AutomaticAdmissionLimits uninfluenced_open_limits =
        uninfluenced_bound_limits;
    uninfluenced_open_limits.incumbent_upper_bound = 20.0;
    CalcContext uninfluenced_open_calc(
        uninfluenced_bound_session, goal,
        uninfluenced_bound_registry,
        {uninfluenced_alteration, uninfluenced_regal, warlord_exalt},
        false, true, false, std::nullopt, {}, false);
    const StateLocalAutomaticBatch uninfluenced_open_batch =
        uninfluenced_open_calc.admit_state_local_automatic_candidates(
            uninfluenced_open_calc.intern_item(magic),
            uninfluenced_open_limits);
    PC_CHECK(
        uninfluenced_open_batch.status ==
        StateLocalAutomaticBatchStatus::Complete);
    const auto uninfluenced_open_programs = admitted_imprint_programs(
        uninfluenced_open_calc, uninfluenced_open_batch);
    PC_CHECK(std::find(
                 uninfluenced_open_programs.begin(),
                 uninfluenced_open_programs.end(),
                 std::vector<std::string>{
                     "regal", "influence_exalt:warlord"}) !=
             uninfluenced_open_programs.end());

    const auto drive_state_local_preparation = [](
        SolveWorkTestAccess::Impl& work,
        const std::uint32_t state) {
        std::pair<bool, std::string> terminal;
        for (std::uint32_t resume = 0; resume < 4096; ++resume) {
            try {
                if (work.prepare_state_expansion(state, true)) {
                    terminal.first = true;
                    return terminal;
                }
            } catch (const SolverResourceLimit& limit) {
                terminal.second = limit.cap_name();
                return terminal;
            }
        }
        throw std::runtime_error(
            "state-local automatic preparation did not terminate");
    };

    /* A focused lower vector can contain finite optimistic values for
     * carriers outside its selected policy. It is never a feasible carrier
     * upper: with no other certificate, the fallback depth must remain an
     * honest refusal rather than using the injected zero to price-prune the
     * Imprint grammar closed. */
    SolveOptions focused_lower_options;
    focused_lower_options.max_imprint_program_depth = 1;
    focused_lower_options.max_imprint_program_work = 64;
    focused_lower_options.max_solver_owned_bytes =
        256ull * 1024ull * 1024ull;
    CalcContext focused_lower_calc(
        session, goal, registry, candidates,
        false, true, false, std::nullopt, {}, false);
    SolveWorkTestAccess::Impl focused_lower_work(
        focused_lower_calc, magic, prices, focused_lower_options);
    focused_lower_work.focused_previous_upper_values.assign(
        focused_lower_calc.state_count(), 0.0);
    const auto focused_lower_terminal =
        drive_state_local_preparation(
            focused_lower_work,
            focused_lower_work.result.start_state);
    PC_CHECK(!focused_lower_terminal.first);
    PC_CHECK(
        focused_lower_terminal.second ==
        "max_imprint_program_depth");
    PC_CHECK(
        focused_lower_work.transition_cache
            ->automatic_admission_phases
            .imprint_price_bound_complete_carriers == 0);

    /* Restart reaches restart_state, not the original solve start. A partial
     * start can be much cheaper than the fresh restart state, so only the
     * independently certified value at restart_state owns the generic
     * carrier route. The deliberately tiny V(start) below must not root-prune
     * the Imprint grammar: at least one exact program is evaluated under
     * Restart + V(restart). */
    auto restart_upper_prices = prices;
    restart_upper_prices["base"] = 1.0;
    const std::vector<std::uint32_t> restart_upper_candidates{
        registry.index_by_id.at("augment"),
        registry.index_by_id.at("regal"),
        registry.index_by_id.at("restart")};
    CalcContext restart_upper_calc(
        session, goal, registry, restart_upper_candidates,
        false, true, false, std::nullopt, {}, false);
    pc_item_state partial_start = magic;
    partial_start.rarity = PC_RARITY_RARE;
    SolveWorkTestAccess::Impl restart_upper_work(
        restart_upper_calc, partial_start,
        restart_upper_prices, focused_lower_options);
    pc_item_state fresh_restart;
    pc_item_clear(&fresh_restart);
    const std::uint32_t fresh_restart_state =
        restart_upper_calc.intern_item(fresh_restart);
    const std::uint32_t restart_carrier_state =
        restart_upper_calc.intern_item(magic);
    restart_upper_work.restart_state = fresh_restart_state;
    SolveWorkTestAccess::Impl::BoundedPolicyIncumbent restart_incumbent;
    restart_incumbent.certified_upper_bound = 0.0;
    restart_incumbent.evaluated_policy_cost = 0.0;
    restart_incumbent.values.assign(
        restart_upper_calc.state_count(), kInfinity);
    restart_incumbent.policy_reachable.assign(
        restart_upper_calc.state_count(), 0);
    restart_incumbent.values[restart_upper_work.result.start_state] = 0.0;
    restart_incumbent.values[fresh_restart_state] = 9.5;
    restart_incumbent.policy_reachable[
        restart_upper_work.result.start_state] = 1;
    restart_incumbent.policy_reachable[fresh_restart_state] = 1;
    restart_incumbent.independently_certified = true;
    restart_incumbent.independently_evaluated = true;
    restart_incumbent.proper = true;
    restart_incumbent.executable = true;
    restart_upper_work.output_incumbent =
        std::move(restart_incumbent);
    (void)drive_state_local_preparation(
        restart_upper_work, restart_carrier_state);
    PC_CHECK(
        restart_upper_work.transition_cache
            ->automatic_admission_phases
            .imprint_programs_evaluated > 0);

    /* Solve scheduling must provide the certified restricted-policy upper to
     * automatic admission before any resource stop. The per-carrier authority
     * is the stable exact restricted-policy snapshot. No fallback or
     * pre-finish incumbent exists yet; a deliberately tiny fallback depth
     * would defer without the snapshot. */
    auto scheduled_session =
        make_imprint_session({"imprint_delayed"});
    scheduled_session->essence_guaranteed_mod_ids = {3};
    ActionRegistry scheduled_registry =
        build_action_registry(*scheduled_session);
    const std::uint32_t scheduled_transmute =
        scheduled_registry.index_by_id.at("transmute");
    const std::uint32_t scheduled_alteration =
        scheduled_registry.index_by_id.at("alteration");
    const std::uint32_t scheduled_augment =
        scheduled_registry.index_by_id.at("augment");
    const std::uint32_t scheduled_regal =
        scheduled_registry.index_by_id.at("regal");
    const std::uint32_t scheduled_restart =
        scheduled_registry.index_by_id.at("restart");
    const std::uint32_t scheduled_delayed =
        scheduled_registry.index_by_id.at("essence:imprint_delayed");
    GoalSpec scheduled_goal = goal;
    scheduled_goal.slots[0].group_id = kNoId;
    scheduled_goal.slots[0].family_id =
        scheduled_session->family_id.at(0);
    CalcContext scheduled_calc(
        scheduled_session, scheduled_goal, scheduled_registry,
        {scheduled_transmute, scheduled_alteration,
         scheduled_augment, scheduled_regal,
         scheduled_restart, scheduled_delayed});
    SolveOptions scheduled_options;
    scheduled_options.goal_progress_gated_reforges = true;
    scheduled_options.state_certificate_control = false;
    scheduled_options.focused_expansion_queue_threshold = 1000000;
    scheduled_options.max_imprint_program_depth = 1;
    scheduled_options.max_imprint_program_work = 64;
    scheduled_options.max_expanded_states = 128;
    scheduled_options.max_sweeps = 4096;
    auto scheduled_prices = prices;
    scheduled_prices["transmute"] = 0.5;
    scheduled_prices["base"] = 1.0;
    scheduled_prices["beast:craicic-croaker"] = 100.0;
    scheduled_prices["essence:imprint_delayed"] = 1000.0;
    pc_item_state scheduled_start;
    pc_item_clear(&scheduled_start);
    SolveWorkTestAccess::Impl scheduled_work(
        scheduled_calc, scheduled_start,
        scheduled_prices, scheduled_options);
    while (!scheduled_work.progress().done) {
        scheduled_work.step(4096);
    }
    PC_CHECK(std::any_of(
        scheduled_work.incremental_certified_upper_values.begin(),
        scheduled_work.incremental_certified_upper_values.end(),
        [](const double value) {
            return std::isfinite(value) && value >= 0.0 &&
                value < kValueCeiling;
    }));
    PC_CHECK(!scheduled_work.focused_fallback_policy);
    PC_CHECK(!scheduled_work.output_incumbent.has_value());
    PC_CHECK(
        scheduled_work.transition_cache
            ->automatic_admission_phases
            .imprint_price_bound_complete_carriers > 0);
    const SolveResult scheduled = scheduled_work.finish();
    PC_CHECK(std::find(
                 scheduled.diagnostics.cap_hits.begin(),
                 scheduled.diagnostics.cap_hits.end(),
                 "max_imprint_program_depth") ==
             scheduled.diagnostics.cap_hits.end());
    PC_CHECK(
        scheduled.diagnostics.automatic_admission_phases
            .imprint_price_bound_complete_carriers > 0);

    /* A numerical latch inside a temporary high-impact upper pass rejects
     * that pass only. It must restore the surrounding lower snapshot and
     * release the pass flags so the finite action scheduler can continue.
     * The former direct Done path stranded the active pass and silently left
     * later automatic carriers unevaluated. */
    CalcContext numerical_pass_calc(
        scheduled_session, scheduled_goal, scheduled_registry,
        {scheduled_transmute, scheduled_alteration,
         scheduled_augment, scheduled_regal,
         scheduled_restart, scheduled_delayed});
    SolveWorkTestAccess::Impl numerical_pass_work(
        numerical_pass_calc, scheduled_start,
        scheduled_prices, scheduled_options);
    numerical_pass_work.focused_round_lower_values =
        numerical_pass_work.result.values;
    numerical_pass_work.focused_round_lower_policy_rows =
        numerical_pass_work.policy_rows;
    numerical_pass_work.focused_upper_mode = true;
    numerical_pass_work.focused_lower_mode = true;
    numerical_pass_work.focus_optimizing = true;
    numerical_pass_work.incremental_upper_policy_pass = true;
    numerical_pass_work.incremental_action_generation = true;
    numerical_pass_work.incremental_envelope_closed = false;
    numerical_pass_work.incremental_unevaluated_actions = 1;
    numerical_pass_work.numerical_stability_stop = true;
    numerical_pass_work.result.diagnostics.policy_evaluation_failure =
        "strict_policy_order_unreconciled_at_numerical_stability";
    numerical_pass_work.backup_active = false;
    numerical_pass_work.policy_iteration_failed = true;
    const std::uint64_t rejected_before =
        numerical_pass_work.incremental_upper_policy_passes_rejected;
    numerical_pass_work.run_focused_lower_unit();
    PC_CHECK(!numerical_pass_work.focused_upper_mode);
    PC_CHECK(!numerical_pass_work.incremental_upper_policy_pass);
    PC_CHECK(
        numerical_pass_work.incremental_upper_policy_passes_rejected ==
        rejected_before + 1);
    PC_CHECK(
        numerical_pass_work.incremental_upper_policy_last_failure ==
        "strict_policy_order_unreconciled_at_numerical_stability");

    /* Resuming one cooperative checkpoint at a time must produce the same
     * complete envelope without replaying a discovery unit. Every returned
     * worker slice and every still-atomic Calc outcomes leaf stays below the
     * 20-second worker contract. */
    CalcContext resumed_calc(
        session, goal, registry, candidates,
        false, true, false, std::nullopt, {}, false);
    const std::uint32_t resumed_state = resumed_calc.intern_item(magic);
    StateLocalAutomaticBatch resumed_batch;
    bool resumed_complete = false;
    std::uint64_t resume_calls = 0;
    std::uint64_t measured_max_slice_ns = 0;
    while (!resumed_complete && resume_calls < 100000) {
        const auto started = std::chrono::steady_clock::now();
        resumed_complete =
            resumed_calc.advance_state_local_automatic_candidates(
                resumed_state, limits, resumed_batch, 1);
        measured_max_slice_ns = std::max(
            measured_max_slice_ns,
            static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() - started)
                    .count()));
        ++resume_calls;
        if (!resumed_complete) {
            PC_CHECK(
                resumed_calc.automatic_admission_cursor_bytes() > 0);
            PC_CHECK(
                resumed_calc.fast_estimated_owned_bytes() >=
                resumed_calc.audited_estimated_owned_bytes());
        }
    }
    PC_CHECK(resumed_complete);
    PC_CHECK(resume_calls > 2);
    PC_CHECK(
        admitted_imprint_programs(resumed_calc, resumed_batch) ==
        admitted_imprint_programs(strict_calc, strict_batch));
    PC_CHECK(
        resumed_batch.phases.imprint_programs_evaluated ==
        strict_batch.phases.imprint_programs_evaluated);
    PC_CHECK(
        resumed_batch.phases.imprint_action_state_evaluations ==
        strict_batch.phases.imprint_action_state_evaluations);
    PC_CHECK(
        resumed_batch.phases.imprint_outcomes_merged ==
        strict_batch.phases.imprint_outcomes_merged);
    constexpr std::uint64_t kWorkerSliceNs = 20'000'000'000ull;
    PC_CHECK(measured_max_slice_ns < kWorkerSliceNs);
    PC_CHECK(resumed_batch.max_continuation_slice_ns < kWorkerSliceNs);
    PC_CHECK(
        resumed_batch.phases.imprint_max_atomic_outcomes_ns <
        kWorkerSliceNs);

    /* Cancellation after one completed atomic outcome exposes no complete
     * batch or adopted parent operator. Destruction rolls back the suspended
     * transaction; retry then deterministically publishes the complete
     * envelope once. */
    CalcContext cancelled_calc(
        session, goal, registry, candidates,
        false, true, false, std::nullopt, {}, false);
    const std::uint32_t cancelled_state =
        cancelled_calc.intern_item(magic);
    const std::size_t cancelled_operator_count =
        cancelled_calc.operators().size();
    const std::size_t cancelled_candidate_count =
        cancelled_calc.candidate_operators().size();
    const std::size_t cancelled_distribution_count =
        cancelled_calc.cached_distribution_count();
    const std::size_t cancelled_reforge_count =
        cancelled_calc.cached_reforge_count();
    StateLocalAutomaticBatch unpublished;
    PC_CHECK(!cancelled_calc.advance_state_local_automatic_candidates(
        cancelled_state, limits, unpublished, 1));
    PC_CHECK(!cancelled_calc.advance_state_local_automatic_candidates(
        cancelled_state, limits, unpublished, 1));
    PC_CHECK(unpublished.admitted_operators.empty());
    PC_CHECK(unpublished.decisions.empty());
    PC_CHECK(cancelled_calc.operators().size() == cancelled_operator_count);
    PC_CHECK(
        cancelled_calc.candidate_operators().size() ==
        cancelled_candidate_count);
    PC_CHECK(cancelled_calc.automatic_admission_cursor_bytes() > 0);
    cancelled_calc.cancel_state_local_automatic_candidates(cancelled_state);
    PC_CHECK(cancelled_calc.automatic_admission_cursor_bytes() == 0);
    PC_CHECK(cancelled_calc.operators().size() == cancelled_operator_count);
    PC_CHECK(
        cancelled_calc.candidate_operators().size() ==
        cancelled_candidate_count);
    PC_CHECK(
        cancelled_calc.cached_distribution_count() ==
        cancelled_distribution_count);
    PC_CHECK(
        cancelled_calc.cached_reforge_count() ==
        cancelled_reforge_count);
    const StateLocalAutomaticBatch cancelled_retry =
        cancelled_calc.admit_state_local_automatic_candidates(
            cancelled_state, limits);
    PC_CHECK(!cancelled_retry.cached);
    PC_CHECK(
        cancelled_retry.status ==
        StateLocalAutomaticBatchStatus::Complete);
    PC_CHECK(
        admitted_imprint_programs(cancelled_calc, cancelled_retry) ==
        admitted_imprint_programs(strict_calc, strict_batch));

    /* A depth boundary retains a bounded concrete frontier witness so a real
     * product report identifies the exact action grammar that remains open. */
    CalcContext depth_deferred_calc(
        session, goal, registry, candidates,
        false, true, false, std::nullopt, {}, false);
    const std::uint32_t depth_deferred_state =
        depth_deferred_calc.intern_item(magic);
    AutomaticAdmissionLimits depth_deferred_limits = limits;
    depth_deferred_limits.max_imprint_program_depth = 1;
    const StateLocalAutomaticBatch depth_deferred_batch =
        depth_deferred_calc.admit_state_local_automatic_candidates(
            depth_deferred_state, depth_deferred_limits);
    PC_CHECK(
        depth_deferred_batch.status ==
        StateLocalAutomaticBatchStatus::ResourceDeferred);
    PC_CHECK(
        depth_deferred_batch.resource_cap ==
        "max_imprint_program_depth");
    const auto depth_boundary = std::find_if(
        depth_deferred_batch.decisions.begin(),
        depth_deferred_batch.decisions.end(),
        [](const StateLocalAutomaticCandidate& decision) {
            return decision.deferred &&
                   decision.kind == AutomaticCandidateKind::Imprint;
        });
    PC_CHECK(depth_boundary != depth_deferred_batch.decisions.end());
    if (depth_boundary != depth_deferred_batch.decisions.end()) {
        PC_CHECK(
            depth_boundary->evidence.reason.find(
                "frontier_samples_") != std::string::npos);
        PC_CHECK(
            depth_boundary->evidence.reason.find(
                "augment:evaluated_changed") != std::string::npos);
    }
    CalcContext sampled_depth_calc(
        session, goal, registry, candidates,
        false, true, false, std::nullopt, {}, false);
    SolveOptions sampled_depth_options;
    sampled_depth_options.max_imprint_program_depth = 1;
    sampled_depth_options.max_imprint_program_work = 16;
    sampled_depth_options.max_diagnostic_samples = 1;
    sampled_depth_options.max_solver_owned_bytes =
        256ull * 1024ull * 1024ull;
    sampled_depth_options.goal_progress_gated_reforges = true;
    sampled_depth_options.state_certificate_control = false;
    const SolveResult sampled_depth = solve(
        sampled_depth_calc, magic, prices, sampled_depth_options);
    PC_CHECK(!sampled_depth.converged);
    PC_CHECK(std::find(
                 sampled_depth.diagnostics.cap_hits.begin(),
                 sampled_depth.diagnostics.cap_hits.end(),
                 "max_imprint_program_depth") !=
             sampled_depth.diagnostics.cap_hits.end());
    PC_CHECK(
        sampled_depth.diagnostics.automatic_candidate_witnesses.size() ==
        1);
    if (!sampled_depth.diagnostics
             .automatic_candidate_witnesses.empty()) {
        const std::string& witness =
            sampled_depth.diagnostics
                .automatic_candidate_witnesses.front();
        PC_CHECK(
            witness.find("\"disposition\":\"deferred\"") !=
            std::string::npos);
        PC_CHECK(
            witness.find("frontier_samples_") !=
            std::string::npos);
        PC_CHECK(
            witness.find("\"influence_bits\":0") !=
            std::string::npos);
        PC_CHECK(
            witness.find("augment:evaluated_changed") !=
            std::string::npos);
    }

    /* A truncated grammar is an explicit unresolved resource boundary, never
     * a cacheable complete envelope. Retrying with room regenerates and then
     * caches only the deterministic complete result. */
    CalcContext deferred_calc(
        session, goal, registry, candidates,
        false, true, false, std::nullopt, {}, false);
    const std::uint32_t deferred_state = deferred_calc.intern_item(magic);
    const std::size_t deferred_operator_count = deferred_calc.operators().size();
    const std::size_t deferred_candidate_count =
        deferred_calc.candidate_operators().size();
    AutomaticAdmissionLimits deferred_limits = limits;
    deferred_limits.max_imprint_program_work = 1;
    const StateLocalAutomaticBatch deferred_batch =
        deferred_calc.admit_state_local_automatic_candidates(
            deferred_state, deferred_limits);
    PC_CHECK(
        deferred_batch.status ==
        StateLocalAutomaticBatchStatus::ResourceDeferred);
    PC_CHECK(!deferred_batch.cached);
    PC_CHECK(deferred_batch.resource_cap == "max_imprint_program_work");
    PC_CHECK(deferred_batch.resource_limit == 1);
    PC_CHECK(deferred_batch.phases.imprint_programs_evaluated == 1);
    PC_CHECK(
        deferred_batch.phases.imprint_action_state_evaluations > 0);
    PC_CHECK(deferred_batch.admitted_operators.empty());
    PC_CHECK(deferred_calc.operators().size() == deferred_operator_count);
    PC_CHECK(
        deferred_calc.candidate_operators().size() ==
        deferred_candidate_count);
    const StateLocalAutomaticBatch deferred_retry =
        deferred_calc.admit_state_local_automatic_candidates(
            deferred_state, limits);
    PC_CHECK(!deferred_retry.cached);
    PC_CHECK(
        deferred_retry.status ==
        StateLocalAutomaticBatchStatus::Complete);
    PC_CHECK(
        admitted_imprint_programs(deferred_calc, deferred_retry) ==
        admitted_imprint_programs(strict_calc, strict_batch));
    const StateLocalAutomaticBatch deferred_cached =
        deferred_calc.admit_state_local_automatic_candidates(
            deferred_state, limits);
    PC_CHECK(deferred_cached.cached);
    PC_CHECK(
        deferred_cached.status ==
        StateLocalAutomaticBatchStatus::Complete);

    std::printf(
        "automatic Imprint cooperative: resumes=%llu max_slice_ms=%.3f "
        "max_atomic_ms=%.3f programs=%llu action_states=%llu "
        "outcomes=%llu\n",
        static_cast<unsigned long long>(resume_calls),
        static_cast<double>(measured_max_slice_ns) / 1000000.0,
        static_cast<double>(
            resumed_batch.phases.imprint_max_atomic_outcomes_ns) /
            1000000.0,
        static_cast<unsigned long long>(
            resumed_batch.phases.imprint_programs_evaluated),
        static_cast<unsigned long long>(
            resumed_batch.phases.imprint_action_state_evaluations),
        static_cast<unsigned long long>(
            resumed_batch.phases.imprint_outcomes_merged));
}

void run_automatic_eldritch_side_tests() {
    auto session = make_solve_session();
    session->eldritch_eligible = true;
    session->eldritch_searing_tier_mod_ids.resize(5);
    session->eldritch_eater_tier_mod_ids.resize(5);
    for (std::uint32_t tier = 1; tier <= 4; ++tier) {
        session->eldritch_searing_tier_mod_ids[tier] = {0};
        session->eldritch_eater_tier_mod_ids[tier] = {5};
    }
    ActionRegistry registry = build_action_registry(*session);
    GoalSpec goal;
    goal.rarity = PC_RARITY_RARE;
    goal.automatic_candidates = true;
    GoalSlot prefix;
    prefix.family_id = 100;
    prefix.min_tier = 1;
    goal.slots.push_back(prefix);
    GoalSlot suffix;
    suffix.family_id = 104;
    suffix.min_tier = 1;
    goal.slots.push_back(suffix);
    const std::vector<std::uint32_t> candidates{
        registry.index_by_id.at("chaos"),
        registry.index_by_id.at("annul")};
    std::unordered_map<std::string, double> prices{
        {"chaos", 10.0},
        {"annul", 10.0},
        {"eldritch_exalt", 0.001},
        {"eldritch_chaos", 3.0},
        {"eldritch_annul", 2.0},
        {"eldritch_ember:1", 9.0},
        {"eldritch_ember:2", 1.0},
        {"eldritch_ember:3", 4.0},
        {"eldritch_ember:4", 5.0},
        {"eldritch_ichor:1", 8.0},
        {"eldritch_ichor:2", 1.5},
        {"eldritch_ichor:3", 4.5},
        {"eldritch_ichor:4", 5.5}};
    constexpr std::uint64_t kFocusedRetainedReforgeWork = 1000000;
    AutomaticAdmissionLimits limits;
    limits.max_state_action_rows = 10000;
    limits.max_transitions = 100000;
    limits.max_solver_owned_bytes = 256ull * 1024ull * 1024ull;
    limits.max_imprint_program_depth = 3;
    limits.max_imprint_program_work = 256;
    limits.prices = &prices;
    const auto decision_is_eldritch_exalt = [](
        const CalcContext& context,
        const StateLocalAutomaticCandidate& decision) {
        if (decision.kind != AutomaticCandidateKind::EldritchSide ||
            decision.operator_index == kNoId ||
            decision.operator_index >= context.operators().size()) {
            return false;
        }
        const PlannerOperator& planner =
            context.operators().at(decision.operator_index);
        return !planner.primitive_program.empty() &&
               context.registry().actions.at(
                   planner.primitive_program.back()).params.type ==
                   ActionType::EldritchExalt;
    };
    const auto admitted_ids = [](
        const CalcContext& context,
        const StateLocalAutomaticBatch& batch,
        const AutomaticCandidateKind kind) {
        std::vector<std::string> ids;
        for (const StateLocalAutomaticCandidate& decision :
             batch.decisions) {
            if (!decision.admitted || decision.kind != kind ||
                decision.operator_index == kNoId ||
                decision.operator_index >= context.operators().size()) {
                continue;
            }
            ids.push_back(
                context.operators().at(decision.operator_index).id);
        }
        std::sort(ids.begin(), ids.end());
        return ids;
    };

    pc_item_state repair_prefix;
    pc_item_clear(&repair_prefix);
    repair_prefix.rarity = PC_RARITY_RARE;
    place(
        &repair_prefix, PC_SIDE_PREFIX, 3,
        session->primary_group[3]);
    place(
        &repair_prefix, PC_SIDE_SUFFIX, 5,
        session->primary_group[5]);
    repair_prefix.eater_of_worlds_tier = 1;
    CalcContext prefix_calc(
        session, goal, registry, candidates);
    const std::uint32_t prefix_state =
        prefix_calc.intern_item(repair_prefix);
    const StateLocalAutomaticBatch prefix_batch =
        prefix_calc.admit_state_local_automatic_candidates(
            prefix_state, limits);
    std::vector<std::uint32_t> prefix_eldritch;
    for (const StateLocalAutomaticCandidate& decision :
         prefix_batch.decisions) {
        if (decision.kind ==
                AutomaticCandidateKind::EldritchSide &&
            decision.admitted) {
            prefix_eldritch.push_back(decision.operator_index);
        }
    }
    PC_CHECK(prefix_eldritch.size() == 3);
    bool saw_prefix_annul = false;
    bool saw_prefix_chaos = false;
    bool saw_prefix_exalt = false;
    for (const std::uint32_t op : prefix_eldritch) {
        const PlannerOperator& planner =
            prefix_calc.operators().at(op);
        PC_CHECK(planner.intended_side == PC_SIDE_PREFIX);
        PC_CHECK(planner.automatic_kind ==
                 AutomaticCandidateKind::EldritchSide);
        PC_CHECK(planner.primitive_program.size() == 2);
        PC_CHECK(
            registry.actions.at(planner.primitive_program.front()).id ==
            "eldritch_ember:2");
        const ActionType final = registry.actions.at(
            planner.primitive_program.back()).params.type;
        saw_prefix_annul |= final == ActionType::EldritchAnnul;
        saw_prefix_chaos |= final == ActionType::EldritchChaos;
        saw_prefix_exalt |= final == ActionType::EldritchExalt;
        if (final == ActionType::EldritchExalt) {
            PC_CHECK(planner.display_name == "Eldritch Exalt Prefix");
        }
        const OptionKernel& kernel =
            prefix_calc.option_kernel(prefix_state, op);
        PC_CHECK(kernel.automatic.eligible);
        PC_CHECK(kernel.expected_primitive_actions == 2.0);
        PC_CHECK(
            kernel.automatic.kernel_change_mechanisms &
            kAutomaticEldritchDominance);
        for (const OutcomeEntry& exit : kernel.exits) {
            pc_item_state item;
            PC_CHECK(prefix_calc.materialize(exit.state, item));
            bool suffix_preserved = false;
            for (std::uint8_t i = 0; i < item.suffix_count; ++i) {
                suffix_preserved |= item.suffixes[i].mod_id == 5;
            }
            PC_CHECK(suffix_preserved);
            PC_CHECK(item.searing_exarch_tier >
                     item.eater_of_worlds_tier);
            if (final == ActionType::EldritchAnnul) {
                PC_CHECK(item.prefix_count == 0);
            }
        }
    }
    PC_CHECK(saw_prefix_annul);
    PC_CHECK(saw_prefix_chaos);
    PC_CHECK(saw_prefix_exalt);

    /* Automatic fixed options carry the exact carrier-local expected
     * resource vector installed from their OptionKernel. The optimistic
     * state bound must therefore price the complete Eldritch program, not
     * only its setup primitive as required for authored conditional fixed
     * options. This lets the existing state-incumbent filter reject a whole
     * exact option before scheduling its sparse row. */
    SolveOptions automatic_lower_options;
    automatic_lower_options.max_states = 10000;
    automatic_lower_options.max_discovered_states = 10000;
    automatic_lower_options.max_expanded_states = 10000;
    automatic_lower_options.max_state_action_rows = 10000;
    automatic_lower_options.max_transitions = 100000;
    automatic_lower_options.max_solver_owned_bytes =
        256ull * 1024ull * 1024ull;
    const auto check_eldritch_completion_bellman = [&] (
        CalcContext& context,
        const pc_item_state& start,
        const std::uint32_t state,
        const StateLocalAutomaticBatch& batch) {
        SolveWorkTestAccess::Impl work(
            context, start, prices, automatic_lower_options);
        const double state_lower =
            work.optimistic_completion_cost_for_state(state);
        PC_CHECK(std::isfinite(state_lower));
        PC_CHECK(state_lower > 0.0);
        std::uint32_t rows = 0;
        for (const StateLocalAutomaticCandidate& decision :
             batch.decisions) {
            if (!decision.admitted ||
                decision.kind != AutomaticCandidateKind::EldritchSide ||
                decision.operator_index == kNoId) {
                continue;
            }
            const std::uint32_t op = decision.operator_index;
            const std::int32_t position =
                work.priced_operator_position.at(op);
            PC_CHECK(position >= 0);
            if (position < 0) continue;
            const OptionKernel& exact = context.option_kernel(state, op);
            PC_CHECK(exact.observation_choice_groups.empty());
            PC_CHECK(exact.observation_choice_options.empty());
            double backup = work.operators.at(
                static_cast<std::size_t>(position)).cost;
            double probability_sum = 0.0;
            for (const OutcomeEntry& exit : exact.exits) {
                probability_sum += exit.probability;
                backup += exit.probability *
                    work.optimistic_completion_cost_for_state(exit.state);
            }
            PC_CHECK(std::fabs(probability_sum - 1.0) < 1e-10);
            if (!(state_lower <= backup +
                  1e-9 * std::max(1.0, std::fabs(backup)))) {
                std::printf(
                    "Eldritch lower Bellman witness: option=%s "
                    "lower=%.17g backup=%.17g\n",
                    context.operators().at(op).id.c_str(),
                    state_lower, backup);
            }
            PC_CHECK(
                state_lower <= backup +
                    1e-9 * std::max(1.0, std::fabs(backup)));
            ++rows;
        }
        return std::pair{state_lower, rows};
    };
    CalcContext automatic_lower_calc(
        session, goal, registry, candidates);
    const std::uint32_t automatic_lower_state =
        automatic_lower_calc.intern_item(repair_prefix);
    const StateLocalAutomaticBatch automatic_lower_batch =
        automatic_lower_calc.admit_state_local_automatic_candidates(
            automatic_lower_state, limits);
    SolveWorkTestAccess::Impl automatic_lower_work(
        automatic_lower_calc, repair_prefix, prices,
        automatic_lower_options);
    const double eligible_completion_lower =
        automatic_lower_work.optimistic_completion_cost_for_state(
            automatic_lower_state);
    PC_CHECK(std::isfinite(eligible_completion_lower));
    PC_CHECK(eligible_completion_lower > 0.0);
    PC_CHECK(automatic_lower_work.strict_clean_goal_cover_cost.empty());

    /* Price availability is part of the proof envelope. Removing every
     * Eldritch final primitive must reduce this to the unchanged ordinary
     * clean-carrier relaxation; making the additional side macros available
     * may only lower that minimization. */
    std::unordered_map<std::string, double> missing_eldritch_prices = prices;
    missing_eldritch_prices.erase("eldritch_annul");
    missing_eldritch_prices.erase("eldritch_chaos");
    missing_eldritch_prices.erase("eldritch_exalt");
    CalcContext missing_price_calc(
        session, goal, registry, candidates);
    const std::uint32_t missing_price_state =
        missing_price_calc.intern_item(repair_prefix);
    SolveWorkTestAccess::Impl missing_price_work(
        missing_price_calc, repair_prefix, missing_eldritch_prices,
        automatic_lower_options);
    const double missing_price_lower =
        missing_price_work.optimistic_completion_cost_for_state(
            missing_price_state);
    PC_CHECK(std::isfinite(missing_price_lower));

    auto ineligible_session = make_solve_session();
    ActionRegistry ineligible_control_registry =
        build_action_registry(*ineligible_session);
    const std::vector<std::uint32_t> ineligible_candidates{
        ineligible_control_registry.index_by_id.at("chaos"),
        ineligible_control_registry.index_by_id.at("annul")};
    CalcContext ineligible_calc(
        ineligible_session, goal, ineligible_control_registry,
        ineligible_candidates);
    const std::uint32_t ineligible_state =
        ineligible_calc.intern_item(repair_prefix);
    SolveWorkTestAccess::Impl ineligible_work(
        ineligible_calc, repair_prefix, missing_eldritch_prices,
        automatic_lower_options);
    const double ineligible_lower =
        ineligible_work.optimistic_completion_cost_for_state(
            ineligible_state);
    PC_CHECK(near(missing_price_lower, ineligible_lower));
    PC_CHECK(eligible_completion_lower <= missing_price_lower + 1e-12);

    CalcContext repeated_lower_calc(
        session, goal, registry, candidates);
    const std::uint32_t repeated_lower_state =
        repeated_lower_calc.intern_item(repair_prefix);
    SolveWorkTestAccess::Impl repeated_lower_work(
        repeated_lower_calc, repair_prefix, prices,
        automatic_lower_options);
    PC_CHECK(near(
        eligible_completion_lower,
        repeated_lower_work.optimistic_completion_cost_for_state(
            repeated_lower_state)));
    bool checked_eldritch_chaos_lower = false;
    std::uint32_t checked_eldritch_bellman_rows = 0;
    for (const StateLocalAutomaticCandidate& decision :
         automatic_lower_batch.decisions) {
        if (!decision.admitted ||
            decision.kind != AutomaticCandidateKind::EldritchSide ||
            decision.operator_index == kNoId) {
            continue;
        }
        const std::uint32_t op = decision.operator_index;
        const PlannerOperator& planner =
            automatic_lower_calc.operators().at(op);
        const std::int32_t priced_position =
            automatic_lower_work.priced_operator_position.at(op);
        PC_CHECK(priced_position >= 0);
        if (priced_position >= 0) {
            const OptionKernel& exact =
                automatic_lower_calc.option_kernel(
                    automatic_lower_state, op);
            PC_CHECK(exact.observation_choice_groups.empty());
            PC_CHECK(exact.observation_choice_options.empty());
            double exact_lower_backup =
                automatic_lower_work.operators.at(
                    static_cast<std::size_t>(priced_position)).cost;
            double probability_sum = 0.0;
            for (const OutcomeEntry& exit : exact.exits) {
                probability_sum += exit.probability;
                exact_lower_backup += exit.probability *
                    automatic_lower_work
                        .optimistic_completion_cost_for_state(exit.state);
            }
            PC_CHECK(std::fabs(probability_sum - 1.0) < 1e-10);
            PC_CHECK(
                eligible_completion_lower <=
                exact_lower_backup + 1e-9 *
                    std::max(1.0, std::fabs(exact_lower_backup)));
            ++checked_eldritch_bellman_rows;
        }
        if (automatic_lower_calc.registry().actions.at(
                planner.primitive_program.back()).params.type !=
            ActionType::EldritchChaos) {
            continue;
        }
        const std::int32_t position =
            automatic_lower_work.priced_operator_position.at(op);
        PC_CHECK(position >= 0);
        if (position < 0) continue;
        const double full_immediate =
            automatic_lower_work.operators.at(
                static_cast<std::size_t>(position)).cost;
        const double lower =
            automatic_lower_work.optimistic_operator_lower(
                automatic_lower_state, op);
        PC_CHECK(std::fabs(full_immediate - 4.0) < 1e-12);
        PC_CHECK(lower >= full_immediate);
        checked_eldritch_chaos_lower = true;
    }
    PC_CHECK(checked_eldritch_chaos_lower);
    PC_CHECK(checked_eldritch_bellman_rows == 3);

    /* Restart is the one destructive operator whose complete successor set
     * is known without constructing a stochastic kernel: the synthetic
     * evaluator deterministically returns a fresh Normal carrier. Its
     * operator lower must therefore price completion from that fresh state,
     * not carry goal progress from the partial source. The legacy expression
     * below is retained only as a regression witness for the lost pruning. */
    auto restart_lower_session = make_solve_session();
    restart_lower_session->bench_mod_ids = {0, 3, 5, 6};
    ActionRegistry restart_lower_registry =
        build_action_registry(*restart_lower_session);
    std::unordered_map<std::string, double> restart_lower_prices{
        {"bench:mod0", 1.0}, {"bench:mod3", 2.0},
        {"bench:mod5", 3.0}, {"bench:mod6", 4.0},
        {"chaos", 100.0}, {"annul", 1.0}, {"scour", 1.0},
        {"base", 1.0}};
    GoalSpec restart_lower_goal;
    restart_lower_goal.rarity = PC_RARITY_RARE;
    for (const std::uint32_t family : {100u, 102u, 104u, 105u}) {
        GoalSlot restart_slot;
        restart_slot.family_id = family;
        restart_slot.min_tier = 1;
        restart_lower_goal.slots.push_back(restart_slot);
    }
    const std::vector<std::uint32_t> restart_lower_candidates{
        restart_lower_registry.index_by_id.at("bench:mod0"),
        restart_lower_registry.index_by_id.at("bench:mod3"),
        restart_lower_registry.index_by_id.at("bench:mod5"),
        restart_lower_registry.index_by_id.at("bench:mod6"),
        restart_lower_registry.index_by_id.at("chaos"),
        restart_lower_registry.index_by_id.at("annul"),
        restart_lower_registry.index_by_id.at("scour"),
        restart_lower_registry.index_by_id.at("restart")};
    pc_item_state partial_restart_source;
    pc_item_clear(&partial_restart_source);
    partial_restart_source.rarity = PC_RARITY_RARE;
    place(
        &partial_restart_source, PC_SIDE_PREFIX, 0,
        restart_lower_session->primary_group[0]);
    place(
        &partial_restart_source, PC_SIDE_PREFIX, 3,
        restart_lower_session->primary_group[3]);
    place(
        &partial_restart_source, PC_SIDE_SUFFIX, 5,
        restart_lower_session->primary_group[5]);
    CalcContext restart_lower_calc(
        restart_lower_session, restart_lower_goal, restart_lower_registry,
        restart_lower_candidates);
    SolveWorkTestAccess::Impl restart_lower_work(
        restart_lower_calc, partial_restart_source,
        restart_lower_prices, automatic_lower_options);
    const std::uint32_t restart_source_state =
        restart_lower_work.result.start_state;
    const std::uint32_t restart_operator =
        restart_lower_work.restart_operator_index;
    PC_CHECK(restart_operator != kNoId);
    const std::int32_t restart_position =
        restart_lower_work.priced_operator_position.at(restart_operator);
    PC_CHECK(restart_position >= 0);
    if (restart_position >= 0) {
        const double restart_immediate =
            restart_lower_work.operators.at(
                static_cast<std::size_t>(restart_position)).cost;
        const std::uint32_t carried_mask =
            restart_lower_work.satisfied_goal_mask_for_state(
                restart_source_state) |
            restart_lower_work.planner_goal_reach_mask(restart_operator);
        PC_CHECK(std::popcount(carried_mask) == 3);
        const double legacy_carried_progress_lower =
            restart_immediate +
            restart_lower_work.optimistic_completion_cost(carried_mask);
        const double restart_lower =
            restart_lower_work.optimistic_operator_lower(
                restart_source_state, restart_operator);
        const OutcomeDistribution& restart_distribution =
            restart_lower_calc.outcomes(
                restart_source_state,
                restart_lower_calc.operators().at(restart_operator)
                    .primitive_action);
        PC_CHECK(restart_distribution.supported);
        PC_CHECK(restart_distribution.entries.size() == 1);
        if (restart_distribution.entries.size() == 1) {
            const double fresh_shaped_relaxation =
                restart_lower_work.optimistic_completion_cost_for_state(
                    restart_distribution.entries.front().state);
            const double exact_successor_relaxation = restart_immediate +
                std::max(
                    restart_lower_work.optimistic_completion_cost(0),
                    fresh_shaped_relaxation);
            PC_CHECK(near(restart_lower, exact_successor_relaxation));
        }
        PC_CHECK(restart_lower > legacy_carried_progress_lower);
    }
    const std::uint32_t source_goal_mask =
        restart_lower_work.satisfied_goal_mask_for_state(
            restart_source_state);
    PC_CHECK(std::popcount(source_goal_mask) == 3);
    const auto verify_successor_lower =
        [&](const char* action_id,
            const std::uint32_t expected_survivors) {
            const std::uint32_t action =
                restart_lower_registry.index_by_id.at(action_id);
            const std::uint32_t survivor_mask =
                restart_lower_work.planner_goal_may_survive_mask(
                    restart_source_state, action);
            PC_CHECK(survivor_mask == expected_survivors);
            const std::int32_t position =
                restart_lower_work.priced_operator_position.at(action);
            PC_CHECK(position >= 0);
            if (position < 0) return;
            const double immediate = restart_lower_work.operators.at(
                static_cast<std::size_t>(position)).cost;
            const double lower =
                restart_lower_work.optimistic_operator_lower(
                    restart_source_state, action);
            const OutcomeDistribution& exact =
                restart_lower_calc.outcomes(
                    restart_source_state, action);
            PC_CHECK(exact.supported);
            double probability_sum = 0.0;
            double exact_relaxed_backup = immediate;
            for (const OutcomeEntry& outcome : exact.entries) {
                probability_sum += outcome.probability;
                exact_relaxed_backup += outcome.probability *
                    restart_lower_work
                        .optimistic_completion_cost_for_state(outcome.state);
            }
            PC_CHECK(std::fabs(probability_sum - 1.0) < 1e-10);
            std::printf(
                "successor lower %s: survive=%u lower=%.9g exact_h=%.9g\n",
                action_id, survivor_mask, lower, exact_relaxed_backup);
            PC_CHECK(
                lower <= exact_relaxed_backup +
                    1e-9 * std::max(1.0, std::fabs(exact_relaxed_backup)));
        };
    verify_successor_lower("chaos", 0);
    verify_successor_lower("scour", 0);
    verify_successor_lower("annul", source_goal_mask);

    pc_item_state fractured_source = partial_restart_source;
    fractured_source.prefixes[0].flags |= PC_MOD_SLOT_FRACTURED;
    const std::uint32_t fractured_source_state =
        restart_lower_calc.intern_item(fractured_source);
    const std::uint32_t fractured_slot_bit = 1u;
    PC_CHECK(
        restart_lower_work.planner_goal_may_survive_mask(
            fractured_source_state,
            restart_lower_registry.index_by_id.at("chaos")) ==
        fractured_slot_bit);
    PC_CHECK(
        restart_lower_work.planner_goal_may_survive_mask(
            fractured_source_state,
            restart_lower_registry.index_by_id.at("scour")) ==
        fractured_slot_bit);
    std::printf(
        "solver automatic Eldritch lower: seed=%.9f bellman_rows=%u\n",
        eligible_completion_lower, checked_eldritch_bellman_rows);

    /* Parent-context Eldritch options are fixed two-step programs. If their
     * complete immediate resource cost already exceeds a certified feasible
     * value for this carrier, nonnegative continuation cost makes them
     * strictly non-improving. Reject them before constructing the expensive
     * parent reforge kernels, just as local automatic options are rejected
     * against the same incumbent authority. */
    CalcContext price_dominated_calc(
        session, goal, registry, candidates);
    const std::uint32_t price_dominated_state =
        price_dominated_calc.intern_item(repair_prefix);
    AutomaticAdmissionLimits price_dominated_limits = limits;
    price_dominated_limits.incumbent_upper_bound = 0.5;
    const StateLocalAutomaticBatch price_dominated_batch =
        price_dominated_calc.admit_state_local_automatic_candidates(
            price_dominated_state, price_dominated_limits);
    std::uint64_t price_dominated_eldritch = 0;
    for (const StateLocalAutomaticCandidate& decision :
         price_dominated_batch.decisions) {
        if (decision.kind !=
            AutomaticCandidateKind::EldritchSide) {
            continue;
        }
        ++price_dominated_eldritch;
        PC_CHECK(!decision.admitted);
        PC_CHECK(!decision.collapsed);
        PC_CHECK(!decision.missing_price);
        PC_CHECK(decision.operator_index == kNoId);
        PC_CHECK(decision.raw_outcomes == 0);
        PC_CHECK(
            decision.evidence.legality_result ==
            "dominated_by_incumbent");
        PC_CHECK(
            decision.evidence.reason ==
            "exact_expected_cost_exceeds_feasible_state_upper");
    }
    PC_CHECK(price_dominated_eldritch == 3);
    PC_CHECK(
        admitted_ids(
            price_dominated_calc, price_dominated_batch,
            AutomaticCandidateKind::EldritchSide).empty());

    /* Carrier-local automatic preparation is one resumable transaction. A
     * one-checkpoint advance must never replay a completed candidate, every
     * suspension must reconcile the selected-owned-byte ledger, and no leaf
     * may monopolize the worker beyond the public 20 second slice contract. */
    constexpr std::uint64_t kWorkerSliceLimitNs =
        20ull * 1000ull * 1000ull * 1000ull;
    CalcContext resumed_calc(
        session, goal, registry, candidates);
    const std::uint32_t resumed_state =
        resumed_calc.intern_item(repair_prefix);
    StateLocalAutomaticBatch resumed_batch;
    std::vector<std::uint64_t> suspended_owned_bytes;
    std::uint64_t measured_max_slice_ns = 0;
    std::uint64_t resume_calls = 0;
    bool resumed_complete = false;
    bool single_flight_rejected = false;
    while (!resumed_complete && resume_calls < 4096) {
        const auto resume_started = std::chrono::steady_clock::now();
        resumed_complete =
            resumed_calc.advance_state_local_automatic_candidates(
                resumed_state, limits, resumed_batch, 1);
        measured_max_slice_ns = std::max(
            measured_max_slice_ns,
            static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() - resume_started)
                    .count()));
        ++resume_calls;
        if (resumed_complete) continue;
        PC_CHECK(resumed_calc.automatic_admission_cursor_bytes() > 0);
        const std::uint64_t audited =
            resumed_calc.audited_estimated_owned_bytes();
        const std::uint64_t fast =
            resumed_calc.fast_estimated_owned_bytes();
        PC_CHECK(fast >= audited);
        PC_CHECK(
            fast >= resumed_calc.automatic_admission_cursor_bytes());
        suspended_owned_bytes.push_back(fast);
        if (!single_flight_rejected) {
            StateLocalAutomaticBatch conflicting;
            try {
                (void)resumed_calc
                    .advance_state_local_automatic_candidates(
                        resumed_state + 1, limits, conflicting, 1);
            } catch (const std::logic_error&) {
                single_flight_rejected = true;
            }
        }
    }
    PC_CHECK(resumed_complete);
    PC_CHECK(single_flight_rejected);
    PC_CHECK(!suspended_owned_bytes.empty());
    PC_CHECK(resumed_calc.automatic_admission_cursor_bytes() == 0);
    PC_CHECK(resumed_batch.continuation_resumes == resume_calls);
    PC_CHECK(
        resumed_batch.continuation_suspensions + 1 ==
        resumed_batch.continuation_resumes);
    PC_CHECK(
        resumed_batch.continuation_suspensions ==
        suspended_owned_bytes.size());
    PC_CHECK(
        resumed_batch.max_continuation_slice_ns <=
        measured_max_slice_ns);
    PC_CHECK(
        resumed_batch.max_continuation_slice_ns <
        kWorkerSliceLimitNs);
    PC_CHECK(measured_max_slice_ns < kWorkerSliceLimitNs);
    PC_CHECK(
        resumed_calc.audited_estimated_owned_bytes() <=
        resumed_calc.fast_estimated_owned_bytes());
    PC_CHECK(
        resumed_batch.decisions.size() ==
        prefix_batch.decisions.size());
    if (resumed_batch.decisions.size() ==
        prefix_batch.decisions.size()) {
        for (std::size_t i = 0;
             i < resumed_batch.decisions.size(); ++i) {
            const StateLocalAutomaticCandidate& resumed =
                resumed_batch.decisions[i];
            const StateLocalAutomaticCandidate& synchronous =
                prefix_batch.decisions[i];
            PC_CHECK(resumed.id == synchronous.id);
            PC_CHECK(resumed.kind == synchronous.kind);
            PC_CHECK(resumed.admitted == synchronous.admitted);
            PC_CHECK(resumed.deferred == synchronous.deferred);
            PC_CHECK(resumed.collapsed == synchronous.collapsed);
            PC_CHECK(resumed.missing_price == synchronous.missing_price);
            PC_CHECK(resumed.raw_outcomes == synchronous.raw_outcomes);
            PC_CHECK(
                resumed.evidence.legality_result ==
                synchronous.evidence.legality_result);
            PC_CHECK(
                resumed.evidence.reason ==
                synchronous.evidence.reason);
            if (resumed.operator_index != kNoId &&
                synchronous.operator_index != kNoId) {
                PC_CHECK(
                    resumed_calc.operators().at(
                        resumed.operator_index).id ==
                    prefix_calc.operators().at(
                        synchronous.operator_index).id);
            }
        }
    }
    PC_CHECK(
        admitted_ids(
            resumed_calc, resumed_batch,
            AutomaticCandidateKind::EldritchSide) ==
        admitted_ids(
            prefix_calc, prefix_batch,
            AutomaticCandidateKind::EldritchSide));
    PC_CHECK(
        resumed_calc.telemetry().reforge_logical_work_v1 ==
        prefix_calc.telemetry().reforge_logical_work_v1);

    /* Cancellation after one parent-Eldritch candidate destroys the frame
     * before restoring staged operators and every cache/template kernel. A
     * clean retry then publishes the same deterministic envelope once. */
    CalcContext cancelled_calc(
        session, goal, registry, candidates);
    const std::uint32_t cancelled_state =
        cancelled_calc.intern_item(repair_prefix);
    const std::size_t cancelled_operator_count_before =
        cancelled_calc.operators().size();
    const std::size_t cancelled_candidate_count_before =
        cancelled_calc.candidate_operators().size();
    const std::uint64_t cancelled_distribution_count_before =
        cancelled_calc.cached_distribution_count();
    const std::uint64_t cancelled_reforge_count_before =
        cancelled_calc.cached_reforge_count();
    StateLocalAutomaticBatch abandoned_batch;
    PC_CHECK(
        !cancelled_calc.advance_state_local_automatic_candidates(
            cancelled_state, limits, abandoned_batch, 1));
    PC_CHECK(
        !cancelled_calc.advance_state_local_automatic_candidates(
            cancelled_state, limits, abandoned_batch, 1));
    PC_CHECK(
        cancelled_calc.operators().size() >
        cancelled_operator_count_before);
    PC_CHECK(cancelled_calc.automatic_admission_cursor_bytes() > 0);
    PC_CHECK(
        cancelled_calc.audited_estimated_owned_bytes() <=
        cancelled_calc.fast_estimated_owned_bytes());
    cancelled_calc.cancel_state_local_automatic_candidates(
        cancelled_state);
    PC_CHECK(cancelled_calc.automatic_admission_cursor_bytes() == 0);
    PC_CHECK(
        cancelled_calc.operators().size() ==
        cancelled_operator_count_before);
    PC_CHECK(
        cancelled_calc.candidate_operators().size() ==
        cancelled_candidate_count_before);
    PC_CHECK(
        cancelled_calc.cached_distribution_count() ==
        cancelled_distribution_count_before);
    PC_CHECK(
        cancelled_calc.cached_reforge_count() ==
        cancelled_reforge_count_before);
    PC_CHECK(
        cancelled_calc.audited_estimated_owned_bytes() <=
        cancelled_calc.fast_estimated_owned_bytes());
    const StateLocalAutomaticBatch cancelled_retry =
        cancelled_calc.admit_state_local_automatic_candidates(
            cancelled_state, limits);
    PC_CHECK(!cancelled_retry.cached);
    PC_CHECK(
        cancelled_retry.status ==
        StateLocalAutomaticBatchStatus::Complete);
    PC_CHECK(
        admitted_ids(
            cancelled_calc, cancelled_retry,
            AutomaticCandidateKind::EldritchSide) ==
        admitted_ids(
            prefix_calc, prefix_batch,
            AutomaticCandidateKind::EldritchSide));
    PC_CHECK(
        cancelled_calc.operators().size() ==
        prefix_calc.operators().size());

    /* The final checkpoint owns the last grown decision/staging vectors.
     * Probe its exact selected bytes, then place the cap one byte below it.
     * With no completed admission contexts retained, the transient child may
     * encounter that cap first and return ResourceDeferred instead of the
     * parent checkpoint throwing. Both paths must roll back, and a retry with
     * room must still be complete and uncached. */
    auto final_checkpoint_session = make_solve_session();
    ActionRegistry final_checkpoint_registry =
        build_action_registry(*final_checkpoint_session);
    const std::vector<std::uint32_t> final_checkpoint_candidates{
        final_checkpoint_registry.index_by_id.at("chaos"),
        final_checkpoint_registry.index_by_id.at("annul")};
    const std::unordered_map<std::string, double>
        final_checkpoint_prices{
            {"chaos", 1.0}, {"annul", 1.0}};
    AutomaticAdmissionLimits final_checkpoint_limits = limits;
    final_checkpoint_limits.max_solver_owned_bytes = 0;
    final_checkpoint_limits.prices = &final_checkpoint_prices;
    pc_item_state final_checkpoint_start;
    pc_item_clear(&final_checkpoint_start);
    final_checkpoint_start.rarity = PC_RARITY_RARE;
    CalcContext final_checkpoint_probe(
        final_checkpoint_session, goal, final_checkpoint_registry,
        final_checkpoint_candidates);
    const std::uint32_t final_checkpoint_probe_state =
        final_checkpoint_probe.intern_item(final_checkpoint_start);
    StateLocalAutomaticBatch final_checkpoint_probe_batch;
    std::vector<std::uint64_t> final_checkpoint_probe_bytes;
    bool final_checkpoint_probe_complete = false;
    while (!final_checkpoint_probe_complete &&
           final_checkpoint_probe_bytes.size() < 4096) {
        final_checkpoint_probe_complete =
            final_checkpoint_probe
                .advance_state_local_automatic_candidates(
                    final_checkpoint_probe_state,
                    final_checkpoint_limits,
                    final_checkpoint_probe_batch, 1);
        if (!final_checkpoint_probe_complete) {
            const std::uint64_t audited = final_checkpoint_probe
                .audited_estimated_owned_bytes();
            const std::uint64_t fast = final_checkpoint_probe
                .fast_estimated_owned_bytes();
            PC_CHECK(fast >= audited);
            final_checkpoint_probe_bytes.push_back(fast);
        }
    }
    PC_CHECK(final_checkpoint_probe_complete);
    PC_CHECK(!final_checkpoint_probe_bytes.empty());
    PC_CHECK(final_checkpoint_probe_bytes.size() >= 2);
    const std::uint64_t exact_publication_checkpoint_owned =
        final_checkpoint_probe_bytes.back();
    const std::uint64_t final_checkpoint_owned =
        final_checkpoint_probe_bytes[
            final_checkpoint_probe_bytes.size() - 2];
    const std::uint64_t prior_checkpoint_max =
        final_checkpoint_probe_bytes.size() <= 2
            ? 0
            : *std::max_element(
                  final_checkpoint_probe_bytes.begin(),
                  final_checkpoint_probe_bytes.end() - 2);
    PC_CHECK(final_checkpoint_owned > prior_checkpoint_max);
    PC_CHECK(
        exact_publication_checkpoint_owned >=
        final_checkpoint_probe.fast_estimated_owned_bytes());
    PC_CHECK(
        final_checkpoint_owned >
        final_checkpoint_probe.fast_estimated_owned_bytes());
    CalcContext final_cap_calc(
        final_checkpoint_session, goal, final_checkpoint_registry,
        final_checkpoint_candidates);
    const std::uint32_t final_cap_state =
        final_cap_calc.intern_item(final_checkpoint_start);
    const std::size_t final_cap_operator_count_before =
        final_cap_calc.operators().size();
    const std::size_t final_cap_candidate_count_before =
        final_cap_calc.candidate_operators().size();
    AutomaticAdmissionLimits final_cap_limits =
        final_checkpoint_limits;
    final_cap_limits.max_solver_owned_bytes =
        final_checkpoint_owned - 1;
    bool final_cap_hit = false;
    bool final_cap_completed = false;
    std::uint64_t final_cap_completed_suspensions = 0;
    StateLocalAutomaticBatch capped_batch;
    try {
        for (std::uint64_t i = 0; i < 4096; ++i) {
            if (final_cap_calc.advance_state_local_automatic_candidates(
                    final_cap_state, final_cap_limits,
                    capped_batch, 1)) {
                final_cap_completed = true;
                break;
            }
            ++final_cap_completed_suspensions;
        }
    } catch (const SolverResourceLimit& limit) {
        final_cap_hit =
            limit.cap_name() == "max_solver_owned_bytes" &&
            limit.limit() == final_cap_limits.max_solver_owned_bytes;
    }
    const bool final_cap_deferred =
        final_cap_completed &&
        capped_batch.status ==
            StateLocalAutomaticBatchStatus::ResourceDeferred &&
        capped_batch.resource_cap == "max_solver_owned_bytes";
    PC_CHECK(final_cap_hit || final_cap_deferred);
    PC_CHECK(
        final_cap_completed_suspensions +
                (final_cap_hit ? 2 : 1) ==
        final_checkpoint_probe_bytes.size());
    PC_CHECK(final_cap_calc.automatic_admission_cursor_bytes() == 0);
    PC_CHECK(
        final_cap_calc.operators().size() ==
        final_cap_operator_count_before);
    PC_CHECK(
        final_cap_calc.candidate_operators().size() ==
        final_cap_candidate_count_before);
    PC_CHECK(
        final_cap_calc.audited_estimated_owned_bytes() <=
        final_cap_calc.fast_estimated_owned_bytes());
    const StateLocalAutomaticBatch final_cap_retry =
        final_cap_calc.admit_state_local_automatic_candidates(
            final_cap_state, final_checkpoint_limits);
    PC_CHECK(!final_cap_retry.cached);
    PC_CHECK(
        final_cap_retry.status ==
        StateLocalAutomaticBatchStatus::Complete);
    PC_CHECK(
        final_cap_retry.admitted_operators ==
        final_checkpoint_probe_batch.admitted_operators);
    PC_CHECK(
        final_cap_retry.decisions.size() ==
        final_checkpoint_probe_batch.decisions.size());

    /* Cached completion copies the retained carrier envelope into the return
     * batch. Its allocation is preflighted before the copy, and cancellation
     * or a one-byte refusal cannot disturb the already-published cache. */
    const std::uint64_t cached_base_owned =
        final_cap_calc.fast_estimated_owned_bytes();
    StateLocalAutomaticBatch cached_projection_batch;
    PC_CHECK(!final_cap_calc.advance_state_local_automatic_candidates(
        final_cap_state, final_checkpoint_limits,
        cached_projection_batch, 1));
    const std::uint64_t cached_projection_owned =
        final_cap_calc.fast_estimated_owned_bytes();
    PC_CHECK(cached_projection_owned > cached_base_owned);
    final_cap_calc.cancel_state_local_automatic_candidates(
        final_cap_state);
    AutomaticAdmissionLimits cached_cap_limits =
        final_checkpoint_limits;
    cached_cap_limits.max_solver_owned_bytes =
        cached_projection_owned - 1;
    bool cached_copy_cap_hit = false;
    try {
        StateLocalAutomaticBatch cached_capped;
        (void)final_cap_calc.advance_state_local_automatic_candidates(
            final_cap_state, cached_cap_limits, cached_capped, 1);
    } catch (const SolverResourceLimit& limit) {
        cached_copy_cap_hit =
            limit.cap_name() == "max_solver_owned_bytes";
    }
    PC_CHECK(cached_copy_cap_hit);
    PC_CHECK(final_cap_calc.automatic_admission_cursor_bytes() == 0);
    const StateLocalAutomaticBatch cached_after_cap =
        final_cap_calc.admit_state_local_automatic_candidates(
            final_cap_state, final_checkpoint_limits);
    PC_CHECK(cached_after_cap.cached);
    PC_CHECK(
        cached_after_cap.admitted_operators ==
        final_cap_retry.admitted_operators);

    /* Even an empty automatic envelope publishes a carrier cache node. Its
     * staging buckets/nodes are guarded before allocation, and a refusal
     * leaves the retry observably uncached. */
    GoalSpec empty_publication_goal = goal;
    empty_publication_goal.automatic_candidates = false;
    CalcContext empty_publication_calc(
        final_checkpoint_session, empty_publication_goal,
        final_checkpoint_registry, final_checkpoint_candidates);
    const std::uint32_t empty_publication_state =
        empty_publication_calc.intern_item(final_checkpoint_start);
    const std::uint64_t empty_publication_base =
        empty_publication_calc.fast_estimated_owned_bytes();
    StateLocalAutomaticBatch empty_projection_batch;
    PC_CHECK(
        !empty_publication_calc.advance_state_local_automatic_candidates(
            empty_publication_state, final_checkpoint_limits,
            empty_projection_batch, 1));
    const std::uint64_t empty_projection_owned =
        empty_publication_calc.fast_estimated_owned_bytes();
    PC_CHECK(empty_projection_owned > empty_publication_base);
    empty_publication_calc.cancel_state_local_automatic_candidates(
        empty_publication_state);
    AutomaticAdmissionLimits empty_cap_limits =
        final_checkpoint_limits;
    empty_cap_limits.max_solver_owned_bytes =
        empty_projection_owned - 1;
    bool empty_publication_cap_hit = false;
    try {
        StateLocalAutomaticBatch empty_capped;
        (void)empty_publication_calc
            .advance_state_local_automatic_candidates(
                empty_publication_state, empty_cap_limits,
                empty_capped, 1);
    } catch (const SolverResourceLimit& limit) {
        empty_publication_cap_hit =
            limit.cap_name() == "max_solver_owned_bytes";
    }
    PC_CHECK(empty_publication_cap_hit);
    PC_CHECK(
        empty_publication_calc.automatic_admission_cursor_bytes() == 0);
    const StateLocalAutomaticBatch empty_retry =
        empty_publication_calc.admit_state_local_automatic_candidates(
            empty_publication_state, final_checkpoint_limits);
    PC_CHECK(!empty_retry.cached);
    PC_CHECK(empty_retry.admitted_operators.empty());
    const StateLocalAutomaticBatch empty_cached =
        empty_publication_calc.admit_state_local_automatic_candidates(
            empty_publication_state, final_checkpoint_limits);
    PC_CHECK(empty_cached.cached);
    PC_CHECK(empty_cached.admitted_operators.empty());
    std::printf(
        "automatic admission continuation: resumes=%llu "
        "suspensions=%zu max_leaf_ms=%.3f final_owned=%llu\n",
        static_cast<unsigned long long>(resume_calls),
        suspended_owned_bytes.size(),
        static_cast<double>(measured_max_slice_ns) / 1000000.0,
        static_cast<unsigned long long>(final_checkpoint_owned));

    /* Parent-context Eldritch evaluation is transactional, but its transient
     * reforge work does not own the retained graph's max_reforge_work cap. A
     * complete automatic envelope may exceed a tiny parent cap; the next
     * ordinary retained Chaos row must still encounter that untouched cap. */
    CalcContext parent_deferred_calc(
        session, goal, registry, candidates);
    const std::uint32_t parent_deferred_state =
        parent_deferred_calc.intern_item(repair_prefix);
    const std::size_t parent_operator_count_before =
        parent_deferred_calc.operators().size();
    const std::size_t parent_candidate_count_before =
        parent_deferred_calc.candidate_operators().size();
    constexpr std::uint64_t kRetainedParentReforgeCap = 1;
    parent_deferred_calc.set_solve_resource_caps(
        10000, kRetainedParentReforgeCap, false,
        256ull * 1024ull * 1024ull);
    AutomaticAdmissionLimits parent_deferred_limits = limits;
    const StateLocalAutomaticBatch parent_deferred =
        parent_deferred_calc.admit_state_local_automatic_candidates(
            parent_deferred_state, parent_deferred_limits);
    PC_CHECK(
        parent_deferred.status ==
        StateLocalAutomaticBatchStatus::Complete);
    PC_CHECK(!parent_deferred.cached);
    PC_CHECK(
        parent_deferred.phases.reforge_logical_work_v1 >
        kRetainedParentReforgeCap);
    PC_CHECK(
        parent_deferred_calc.telemetry()
            .automatic_admission_reforge_logical_work_v1 ==
        parent_deferred.phases.reforge_logical_work_v1);
    PC_CHECK(
        parent_deferred_calc.telemetry().reforge_logical_work_v1 <
        kRetainedParentReforgeCap);
    PC_CHECK(
        parent_deferred_calc.operators().size() >
        parent_operator_count_before);
    PC_CHECK(
        parent_deferred_calc.candidate_operators().size() >
        parent_candidate_count_before);
    PC_CHECK(
        admitted_ids(
            parent_deferred_calc, parent_deferred,
            AutomaticCandidateKind::EldritchSide) ==
        admitted_ids(
            prefix_calc, prefix_batch,
            AutomaticCandidateKind::EldritchSide));
    PC_CHECK(
        parent_deferred_calc.operators().size() ==
        prefix_calc.operators().size());
    bool retained_reforge_cap_hit = false;
    try {
        (void)parent_deferred_calc.outcomes(
            parent_deferred_state,
            registry.index_by_id.at("chaos"));
    } catch (const SolverResourceLimit& limit) {
        retained_reforge_cap_hit =
            limit.cap_name() == "max_reforge_work" &&
            limit.limit() == kRetainedParentReforgeCap;
    }
    PC_CHECK(retained_reforge_cap_hit);
    const StateLocalAutomaticBatch parent_cached =
        parent_deferred_calc.admit_state_local_automatic_candidates(
            parent_deferred_state, limits);
    PC_CHECK(
        parent_cached.status ==
        StateLocalAutomaticBatchStatus::Complete);
    PC_CHECK(parent_cached.cached);
    PC_CHECK(
        parent_cached.admitted_operators ==
        parent_deferred.admitted_operators);

    /* The retained admission context is also retry-safe when selected-owned
     * memory stops a deterministic Multimod finish. Local state discovery is
     * intentionally uncapped; raising the actual memory boundary on the same
     * parent regenerates the complete deterministic envelope once. */
    auto local_session = make_solve_session();
    constexpr int kLocalMultimodCode = 17;
    auto local_data =
        std::make_shared<DataImpl>(*local_session->data);
    local_data->metamod_multimod_code = kLocalMultimodCode;
    local_session->data = std::move(local_data);
    local_session->metamod_type[5] = kLocalMultimodCode;
    local_session->bench_mod_ids = {3, 5, 6};
    for (const std::uint32_t mod : local_session->bench_mod_ids) {
        local_session->flags[mod] = 1u << 1;
        pc_bitset_clear(
            local_session->normal_random_roll_mask.data(), mod);
    }
    ActionRegistry local_registry =
        build_action_registry(*local_session);
    GoalSpec local_goal;
    local_goal.rarity = PC_RARITY_RARE;
    local_goal.automatic_candidates = true;
    for (const std::uint32_t family : {102u, 105u}) {
        GoalSlot slot;
        slot.family_id = family;
        slot.min_tier = 1;
        local_goal.slots.push_back(slot);
    }
    const std::vector<std::uint32_t> local_candidates{
        local_registry.index_by_id.at("bench:mod3"),
        local_registry.index_by_id.at("bench:mod5"),
        local_registry.index_by_id.at("bench:mod6")};
    const std::unordered_map<std::string, double> local_prices{
        {"bench:mod3", 1.0},
        {"bench:mod5", 2.0},
        {"bench:mod6", 1.0}};
    AutomaticAdmissionLimits local_limits;
    local_limits.max_solver_owned_bytes =
        256ull * 1024ull * 1024ull;
    local_limits.max_imprint_program_depth = 3;
    local_limits.max_imprint_program_work = 256;
    local_limits.prices = &local_prices;
    pc_item_state local_start;
    pc_item_clear(&local_start);
    local_start.rarity = PC_RARITY_RARE;

    CalcContext local_probe_calc(
        local_session, local_goal, local_registry, local_candidates);
    const StateLocalAutomaticBatch local_probe =
        local_probe_calc.admit_state_local_automatic_candidates(
            local_probe_calc.intern_item(local_start), local_limits);
    PC_CHECK(
        local_probe.status ==
        StateLocalAutomaticBatchStatus::Complete);
    const std::vector<std::string> local_probe_ids = admitted_ids(
        local_probe_calc, local_probe,
        AutomaticCandidateKind::MultimodFinish);
    PC_CHECK(!local_probe_ids.empty());
    const std::uint32_t retained_local_parent_states =
        local_probe_calc.state_count();
    PC_CHECK(retained_local_parent_states > 0);
    PC_CHECK(
        local_probe.phases.discovered_states >
        retained_local_parent_states);

    CalcContext local_state_cap_calc(
        local_session, local_goal, local_registry, local_candidates);
    const std::uint32_t local_state_cap_state =
        local_state_cap_calc.intern_item(local_start);
    local_state_cap_calc.set_solve_resource_caps(
        retained_local_parent_states, kFocusedRetainedReforgeWork,
        false, local_limits.max_solver_owned_bytes);
    const StateLocalAutomaticBatch local_state_cap_batch =
        local_state_cap_calc.admit_state_local_automatic_candidates(
            local_state_cap_state, local_limits);
    PC_CHECK(
        local_state_cap_batch.status ==
        StateLocalAutomaticBatchStatus::Complete);
    PC_CHECK(
        local_state_cap_calc.state_count() <=
        retained_local_parent_states);
    PC_CHECK(
        local_state_cap_batch.phases.discovered_states >
        retained_local_parent_states);
    PC_CHECK(
        admitted_ids(
            local_state_cap_calc, local_state_cap_batch,
            AutomaticCandidateKind::MultimodFinish) ==
        local_probe_ids);

    CalcContext local_deferred_calc(
        local_session, local_goal, local_registry, local_candidates);
    const std::uint32_t local_deferred_state =
        local_deferred_calc.intern_item(local_start);
    const std::size_t local_candidate_count_before =
        local_deferred_calc.candidate_operators().size();
    AutomaticAdmissionLimits local_deferred_limits = local_limits;
    const std::uint64_t local_owned_before =
        local_deferred_calc.fast_estimated_owned_bytes();
    const std::uint64_t local_probe_owned =
        local_probe_calc.fast_estimated_owned_bytes();
    PC_CHECK(local_probe_owned > local_owned_before);
    local_deferred_limits.max_solver_owned_bytes =
        local_owned_before +
        (local_probe_owned - local_owned_before) / 2;
    bool local_memory_deferred = false;
    try {
        (void)local_deferred_calc
            .admit_state_local_automatic_candidates(
                local_deferred_state, local_deferred_limits);
    } catch (const SolverResourceLimit& limit) {
        local_memory_deferred =
            (limit.cap_name() == "max_solver_owned_bytes" ||
             limit.cap_name() == "max_owned_bytes") &&
            limit.limit() ==
                local_deferred_limits.max_solver_owned_bytes;
    }
    PC_CHECK(local_memory_deferred);
    PC_CHECK(
        local_deferred_calc.automatic_admission_cursor_bytes() == 0);
    PC_CHECK(
        local_deferred_calc.candidate_operators().size() ==
        local_candidate_count_before);
    const StateLocalAutomaticBatch local_retry =
        local_deferred_calc.admit_state_local_automatic_candidates(
            local_deferred_state, local_limits);
    PC_CHECK(
        local_retry.status ==
        StateLocalAutomaticBatchStatus::Complete);
    PC_CHECK(!local_retry.cached);
    PC_CHECK(
        admitted_ids(
            local_deferred_calc, local_retry,
            AutomaticCandidateKind::MultimodFinish) ==
        local_probe_ids);
    PC_CHECK(
        local_deferred_calc.operators().size() ==
        local_probe_calc.operators().size());
    PC_CHECK(
        local_deferred_calc.candidate_operators().size() ==
        local_probe_calc.candidate_operators().size());
    const StateLocalAutomaticBatch local_cached =
        local_deferred_calc.admit_state_local_automatic_candidates(
            local_deferred_state, local_limits);
    PC_CHECK(
        local_cached.status ==
        StateLocalAutomaticBatchStatus::Complete);
    PC_CHECK(local_cached.cached);
    PC_CHECK(
        local_cached.admitted_operators ==
        local_retry.admitted_operators);

    /*
     * The side intent is an ordinary Bellman choice, not a prescribed route.
     * With the completed suffix worth preserving and Eldritch setup priced
     * below full Chaos, the exact restricted solver must discover a prefix
     * side action and compilation must lower it to the real setup currency
     * followed by the real Eldritch currency.
     */
    CalcContext prefix_solve_calc(
        session, goal, registry, candidates);
    SolveOptions prefix_solve_options;
    prefix_solve_options.goal_progress_gated_reforges = true;
    prefix_solve_options.focused_expansion_queue_threshold = 1000000;
    /* Keep this synthetic regression on the candidate-admission envelope it
     * was designed to exercise. Product-default cap changes are verified
     * separately and must not multiply the focused suite's runtime. */
    prefix_solve_options.max_reforge_work =
        kFocusedRetainedReforgeWork;
    prefix_solve_options.max_sweeps = 512;
    auto deferred_closure_session =
        make_solve_session({"automatic_closure_delayed"});
    deferred_closure_session->essence_guaranteed_mod_ids = {0};
    deferred_closure_session->eldritch_eligible = true;
    deferred_closure_session->eldritch_searing_tier_mod_ids.resize(5);
    deferred_closure_session->eldritch_eater_tier_mod_ids.resize(5);
    for (std::uint32_t tier = 1; tier <= 4; ++tier) {
        deferred_closure_session->eldritch_searing_tier_mod_ids[tier] =
            {0};
        deferred_closure_session->eldritch_eater_tier_mod_ids[tier] =
            {5};
    }
    ActionRegistry deferred_closure_registry =
        build_action_registry(*deferred_closure_session);
    CalcContext deferred_closure_calc(
        deferred_closure_session, goal, deferred_closure_registry,
        {deferred_closure_registry.index_by_id.at(
             "essence:automatic_closure_delayed"),
         deferred_closure_registry.index_by_id.at("restart")});
    auto deferred_closure_prices = prices;
    deferred_closure_prices["base"] = 1.0;
    deferred_closure_prices["essence:automatic_closure_delayed"] = 1.0;
    SolveOptions deferred_closure_options = prefix_solve_options;
    deferred_closure_options.max_reforge_work = 1;
    const SolveResult deferred_closure = solve(
        deferred_closure_calc, repair_prefix, deferred_closure_prices,
        deferred_closure_options);
    PC_CHECK(!deferred_closure.converged);
    PC_CHECK(
        !deferred_closure.diagnostics
            .incremental_action_envelope_closed);
    PC_CHECK(
        deferred_closure.diagnostics.incremental_actions_unresolved > 0);
    PC_CHECK(
        deferred_closure.termination ==
        SolveTermination::RefusedResourceCap);
    PC_CHECK(
        deferred_closure.diagnostics.independent_goal_cover_lower_bound >
        0.0);
    PC_CHECK(deferred_closure.lower_bound > 0.0);
    PC_CHECK(deferred_closure.global_lower_bound_certified);
    PC_CHECK(
        deferred_closure.lower_bound_provenance ==
        SolveLowerBoundProvenance::GlobalActionRelaxation);
    PC_CHECK(std::find(
                 deferred_closure.diagnostics.cap_hits.begin(),
                 deferred_closure.diagnostics.cap_hits.end(),
                 "max_reforge_work") !=
             deferred_closure.diagnostics.cap_hits.end());
    PC_CHECK(
        deferred_closure.diagnostics.automatic_admission_phases
            .reforge_logical_work_v1 > 1);
    PC_CHECK(
        deferred_closure.diagnostics.reforge_logical_work_v1 <= 1);
    const std::string deferred_closure_telemetry =
        serialize_solver_telemetry(
            deferred_closure_calc, &deferred_closure,
            nullptr, std::nullopt, nullptr);
    PC_CHECK(
        deferred_closure_telemetry.find(
            "parent_eldritch_kernel_generation_max_reforge_work") ==
        std::string::npos);
    const SolveResult prefix_solved = solve(
        prefix_solve_calc, repair_prefix, prices,
        prefix_solve_options);
    PC_CHECK(prefix_solved.policy_available);
    PC_CHECK(
        eligible_completion_lower <=
        prefix_solved.evaluated_policy_cost + 1e-9 *
            std::max(
                1.0,
                std::fabs(prefix_solved.evaluated_policy_cost)));
    /* Automatic admission may inspect a much larger transient exact kernel
     * than the sparse row ultimately retained by Solve. max_transitions owns
     * only that retained graph; replay the deterministic solve with a cap
     * strictly between retained storage and transient admission work and
     * require the automatic work ledger to pass through it without consuming
     * it. Leave bounded per-row insertion headroom because entry-relative
     * self transitions are checked before sparse normalization. */
    const std::uint64_t retained_transitions =
        prefix_solved.diagnostics.sparse_transitions;
    const std::uint64_t automatic_transition_work =
        prefix_solved.diagnostics.automatic_admission_phases
            .transition_entries;
    PC_CHECK(retained_transitions > 0);
    PC_CHECK(
        automatic_transition_work > retained_transitions);
    const std::uint64_t retained_transition_cap =
        retained_transitions +
        (automatic_transition_work - retained_transitions) / 2;
    PC_CHECK(retained_transition_cap > retained_transitions);
    PC_CHECK(retained_transition_cap < automatic_transition_work);
    CalcContext retained_transition_calc(
        session, goal, registry, candidates);
    SolveOptions retained_transition_options = prefix_solve_options;
    retained_transition_options.max_transitions = retained_transition_cap;
    const SolveResult retained_transition_solved = solve(
        retained_transition_calc, repair_prefix, prices,
        retained_transition_options);
    PC_CHECK(
        retained_transition_solved.diagnostics.sparse_transitions <=
        retained_transition_cap);
    PC_CHECK(
        retained_transition_solved.diagnostics.automatic_admission_phases
            .transition_entries > retained_transition_cap);
    PC_CHECK(std::find(
                 retained_transition_solved.diagnostics.cap_hits.begin(),
                 retained_transition_solved.diagnostics.cap_hits.end(),
                 "max_transitions") ==
             retained_transition_solved.diagnostics.cap_hits.end());
    PC_CHECK(retained_transition_solved.policy_available);
    PC_CHECK(
        retained_transition_solved.diagnostics
            .automatic_admission_phases.discovered_states > 0);
    const std::string retained_transition_telemetry =
        serialize_solver_telemetry(
            retained_transition_calc, &retained_transition_solved,
            nullptr, std::nullopt, nullptr);
    const std::string automatic_work_json =
        "\"work\":{\"discovered_states\":" +
        std::to_string(
            retained_transition_solved.diagnostics
                .automatic_admission_phases.discovered_states) +
        ",\"state_action_rows\":" +
        std::to_string(
            retained_transition_solved.diagnostics
                .automatic_admission_phases.state_action_rows) +
        ",\"transition_entries\":" +
        std::to_string(
            retained_transition_solved.diagnostics
                .automatic_admission_phases.transition_entries) +
        ",\"reforge_active_work\":" +
        std::to_string(
            retained_transition_solved.diagnostics
                .automatic_admission_phases.reforge_active_work) +
        ",\"reforge_logical_work_v1\":" +
        std::to_string(
            retained_transition_solved.diagnostics
                .automatic_admission_phases.reforge_logical_work_v1) +
        "}";
    PC_CHECK(
        retained_transition_telemetry.find(automatic_work_json) !=
        std::string::npos);
    const PolicyOperatorRef prefix_selected =
        prefix_solved.policy_available
            ? prefix_solved.policy[prefix_solved.start_state]
            : PolicyOperatorRef{};
    PC_CHECK(prefix_selected != kNoId);
    if (prefix_selected != kNoId) {
        const PlannerOperator& planner =
            prefix_solve_calc.operators().at(prefix_selected.index);
        PC_CHECK(
            planner.automatic_kind ==
            AutomaticCandidateKind::EldritchSide);
        PC_CHECK(planner.intended_side == PC_SIDE_PREFIX);
    }
    if (prefix_solved.policy_available) {
        const std::string strategy_json = compile_policy_strategy_json(
            prefix_solve_calc, prefix_solved,
            "automatic Eldritch prefix repair");
        PC_CHECK(
            strategy_json.find(
                "\"type\":\"eldritch_ember\",\"tier\":2") !=
                 std::string::npos);
        PC_CHECK(
            strategy_json.find("\"type\":\"eldritch_exalt\"") !=
                std::string::npos);
        const std::shared_ptr<StrategyImpl> strategy =
            compile_strategy_json(
                session, strategy_json.data(), strategy_json.size());
        auto economy = std::make_shared<EconomyImpl>();
        economy->id = "automatic-eldritch-side-test";
        economy->prices = prices;
        StrategyEvalOptions evaluation_options;
        evaluation_options.economy = economy;
        const StrategyEvalResult evaluation =
            evaluate_strategy(*strategy, evaluation_options);
        PC_CHECK(evaluation.converged);
        PC_CHECK(evaluation.cost_complete);
        PC_CHECK(near(
            evaluation.total_expected_cost,
            prefix_solved.evaluated_policy_cost, 1e-8));

        SimulatorImpl simulator;
        simulator.session = session;
        simulator.strategy = strategy;
        simulator.economy = economy;
        prepare_simulator_runtime(simulator);
        SimulationOptionsInternal simulation_options;
        simulation_options.target_runs = 10000;
        simulation_options.seed = 0x454c445249544348ULL;
        simulation_options.max_actions_per_run = 100000;
        run_simulator_chunk(simulator, simulation_options, 10000);
        PC_CHECK(simulator.summary.completed_runs == 10000);
        PC_CHECK(simulator.summary.success_count == 10000);
        PC_CHECK(simulator.summary.action_not_applied_count == 0);
        PC_CHECK(simulator.summary.no_matching_edge_count == 0);
        const double empirical_cost =
            simulator.summary.known_total_cost / 10000.0;
        PC_CHECK(
            std::abs(
                empirical_cost -
                evaluation.total_expected_cost) <=
            std::max(
                0.5,
                evaluation.total_expected_cost * 0.10));
        std::printf(
            "solver automatic Eldritch compiled policy: "
            "exact=%.6f empirical=%.6f runs=10000\n",
            evaluation.total_expected_cost, empirical_cost);
    }
    CalcContext prefix_repeat_calc(
        session, goal, registry, candidates);
    const SolveResult prefix_repeated = solve(
        prefix_repeat_calc, repair_prefix, prices,
        prefix_solve_options);
    PC_CHECK(identical_solve(prefix_solved, prefix_repeated));
    PC_CHECK(
        prefix_solved.diagnostics.transition_bits_hash ==
        prefix_repeated.diagnostics.transition_bits_hash);
    PC_CHECK(
        prefix_solved.diagnostics.policy_bits_hash ==
        prefix_repeated.diagnostics.policy_bits_hash);

    pc_item_state dominant = repair_prefix;
    dominant.searing_exarch_tier = 2;
    dominant.eater_of_worlds_tier = 1;
    CalcContext dominant_calc(
        session, goal, registry, candidates);
    const std::uint32_t dominant_state =
        dominant_calc.intern_item(dominant);
    const StateLocalAutomaticBatch dominant_batch =
        dominant_calc.admit_state_local_automatic_candidates(
            dominant_state, limits);
    const auto [dominant_lower, dominant_bellman_rows] =
        check_eldritch_completion_bellman(
            dominant_calc, dominant, dominant_state, dominant_batch);
    (void)dominant_lower;
    std::uint32_t direct_count = 0;
    for (const StateLocalAutomaticCandidate& decision :
         dominant_batch.decisions) {
        if (decision.kind !=
                AutomaticCandidateKind::EldritchSide ||
            !decision.admitted) {
            continue;
        }
        const PlannerOperator& planner =
            dominant_calc.operators().at(decision.operator_index);
        PC_CHECK(planner.primitive_program.size() == 1);
        PC_CHECK(planner.id.find(":direct") != std::string::npos);
        const OptionKernel& kernel = dominant_calc.option_kernel(
            dominant_state, decision.operator_index);
        PC_CHECK(kernel.expected_primitive_actions == 1.0);
        ++direct_count;
    }
    PC_CHECK(direct_count == 3);
    PC_CHECK(dominant_bellman_rows == direct_count);

    pc_item_state repair_suffix;
    pc_item_clear(&repair_suffix);
    repair_suffix.rarity = PC_RARITY_RARE;
    place(
        &repair_suffix, PC_SIDE_PREFIX, 0,
        session->primary_group[0]);
    place(
        &repair_suffix, PC_SIDE_SUFFIX, 6,
        session->primary_group[6]);
    repair_suffix.searing_exarch_tier = 1;
    CalcContext suffix_calc(
        session, goal, registry, candidates);
    const std::uint32_t suffix_state =
        suffix_calc.intern_item(repair_suffix);
    const StateLocalAutomaticBatch suffix_batch =
        suffix_calc.admit_state_local_automatic_candidates(
            suffix_state, limits);
    const auto [suffix_lower, suffix_bellman_rows] =
        check_eldritch_completion_bellman(
            suffix_calc, repair_suffix, suffix_state, suffix_batch);
    (void)suffix_lower;
    std::uint32_t suffix_count = 0;
    bool saw_suffix_exalt = false;
    for (const StateLocalAutomaticCandidate& decision :
         suffix_batch.decisions) {
        if (decision.kind !=
                AutomaticCandidateKind::EldritchSide ||
            !decision.admitted) {
            continue;
        }
        const PlannerOperator& planner =
            suffix_calc.operators().at(decision.operator_index);
        PC_CHECK(planner.intended_side == PC_SIDE_SUFFIX);
        PC_CHECK(
            registry.actions.at(planner.primitive_program.front()).id ==
            "eldritch_ichor:2");
        const ActionType final = registry.actions.at(
            planner.primitive_program.back()).params.type;
        if (final == ActionType::EldritchExalt) {
            saw_suffix_exalt = true;
            PC_CHECK(planner.display_name == "Eldritch Exalt Suffix");
            const OptionKernel& kernel = suffix_calc.option_kernel(
                suffix_state, decision.operator_index);
            PC_CHECK(kernel.automatic.eligible);
            PC_CHECK(kernel.expected_primitive_actions == 2.0);
            bool saw_goal_exit = false;
            for (const OutcomeEntry& exit : kernel.exits) {
                const AbstractState& successor =
                    suffix_calc.state(exit.state);
                PC_CHECK(
                    successor.slot_status[0] ==
                    static_cast<std::uint8_t>(
                        GoalSlotStatus::Satisfied));
                saw_goal_exit |=
                    successor.slot_status[1] ==
                    static_cast<std::uint8_t>(
                        GoalSlotStatus::Satisfied);
            }
            PC_CHECK(saw_goal_exit);
        }
        ++suffix_count;
    }
    PC_CHECK(suffix_count == 3);
    PC_CHECK(suffix_bellman_rows == suffix_count);
    PC_CHECK(saw_suffix_exalt);

    /* A target family blocked by an existing explicit group is not rollable,
     * even with target-side capacity and useful opposite-side progress. */
    pc_item_state blocked_prefix;
    pc_item_clear(&blocked_prefix);
    blocked_prefix.rarity = PC_RARITY_RARE;
    place(
        &blocked_prefix, PC_SIDE_PREFIX, 2,
        session->primary_group[2]);
    place(
        &blocked_prefix, PC_SIDE_SUFFIX, 5,
        session->primary_group[5]);
    blocked_prefix.eater_of_worlds_tier = 1;
    CalcContext blocked_calc(
        session, goal, registry, candidates);
    const StateLocalAutomaticBatch blocked_batch =
        blocked_calc.admit_state_local_automatic_candidates(
            blocked_calc.intern_item(blocked_prefix), limits);
    PC_CHECK(std::none_of(
        blocked_batch.decisions.begin(), blocked_batch.decisions.end(),
        [&](const StateLocalAutomaticCandidate& decision) {
            return decision_is_eldritch_exalt(blocked_calc, decision);
        }));

    /* Capacity and a rollable target are insufficient without already useful
     * goal progress on the side that Eldritch Exalt preserves. */
    pc_item_state no_opposite_progress;
    pc_item_clear(&no_opposite_progress);
    no_opposite_progress.rarity = PC_RARITY_RARE;
    place(
        &no_opposite_progress, PC_SIDE_PREFIX, 3,
        session->primary_group[3]);
    place(
        &no_opposite_progress, PC_SIDE_SUFFIX, 6,
        session->primary_group[6]);
    no_opposite_progress.eater_of_worlds_tier = 1;
    CalcContext no_opposite_calc(
        session, goal, registry, candidates);
    const StateLocalAutomaticBatch no_opposite_batch =
        no_opposite_calc.admit_state_local_automatic_candidates(
            no_opposite_calc.intern_item(no_opposite_progress), limits);
    PC_CHECK(std::none_of(
        no_opposite_batch.decisions.begin(),
        no_opposite_batch.decisions.end(),
        [&](const StateLocalAutomaticCandidate& decision) {
            return decision_is_eldritch_exalt(
                no_opposite_calc, decision);
        }));

    /* Missing action and setup prices retain auditable rejected decisions but
     * never admit the compound option. */
    auto missing_action_prices = prices;
    missing_action_prices.erase("eldritch_exalt");
    AutomaticAdmissionLimits missing_action_limits = limits;
    missing_action_limits.prices = &missing_action_prices;
    CalcContext missing_action_calc(
        session, goal, registry, candidates);
    const StateLocalAutomaticBatch missing_action_batch =
        missing_action_calc.admit_state_local_automatic_candidates(
            missing_action_calc.intern_item(repair_prefix),
            missing_action_limits);
    bool saw_missing_action_price = false;
    for (const StateLocalAutomaticCandidate& decision :
         missing_action_batch.decisions) {
        if (!decision_is_eldritch_exalt(
                missing_action_calc, decision)) {
            continue;
        }
        saw_missing_action_price = true;
        PC_CHECK(!decision.admitted);
        PC_CHECK(decision.missing_price);
        PC_CHECK(
            decision.evidence.reason ==
            "automatic_candidate_missing_price");
    }
    PC_CHECK(saw_missing_action_price);

    auto missing_setup_prices = prices;
    missing_setup_prices.erase("eldritch_ember:2");
    missing_setup_prices.erase("eldritch_ember:3");
    missing_setup_prices.erase("eldritch_ember:4");
    AutomaticAdmissionLimits missing_setup_limits = limits;
    missing_setup_limits.prices = &missing_setup_prices;
    CalcContext missing_setup_calc(
        session, goal, registry, candidates);
    const StateLocalAutomaticBatch missing_setup_batch =
        missing_setup_calc.admit_state_local_automatic_candidates(
            missing_setup_calc.intern_item(repair_prefix),
            missing_setup_limits);
    bool saw_missing_setup_price = false;
    for (const StateLocalAutomaticCandidate& decision :
         missing_setup_batch.decisions) {
        if (!decision_is_eldritch_exalt(
                missing_setup_calc, decision)) {
            continue;
        }
        saw_missing_setup_price = true;
        PC_CHECK(!decision.admitted);
        PC_CHECK(decision.missing_price);
    }
    PC_CHECK(saw_missing_setup_price);

    pc_item_state influenced_exalt = repair_prefix;
    influenced_exalt.generic_influence_bits = 1;
    CalcContext influenced_exalt_calc(
        session, goal, registry, candidates);
    const StateLocalAutomaticBatch influenced_exalt_batch =
        influenced_exalt_calc.admit_state_local_automatic_candidates(
            influenced_exalt_calc.intern_item(influenced_exalt), limits);
    bool saw_influenced_exalt = false;
    for (const StateLocalAutomaticCandidate& decision :
         influenced_exalt_batch.decisions) {
        if (!decision_is_eldritch_exalt(
                influenced_exalt_calc, decision)) {
            continue;
        }
        saw_influenced_exalt = true;
        PC_CHECK(!decision.admitted);
        PC_CHECK(!decision.evidence.eligible);
    }
    PC_CHECK(saw_influenced_exalt);

    auto full_session = make_solve_session();
    full_session->eldritch_eligible = true;
    full_session->rare_affix_cap = 2;
    full_session->eldritch_searing_tier_mod_ids.resize(5);
    full_session->eldritch_eater_tier_mod_ids.resize(5);
    for (std::uint32_t tier = 1; tier <= 4; ++tier) {
        full_session->eldritch_searing_tier_mod_ids[tier] = {0};
        full_session->eldritch_eater_tier_mod_ids[tier] = {5};
    }
    ActionRegistry full_registry =
        build_action_registry(*full_session);
    const std::vector<std::uint32_t> full_candidates{
        full_registry.index_by_id.at("chaos"),
        full_registry.index_by_id.at("annul")};
    pc_item_state full_prefix;
    pc_item_clear(&full_prefix);
    full_prefix.rarity = PC_RARITY_RARE;
    place(
        &full_prefix, PC_SIDE_PREFIX, 3,
        full_session->primary_group[3]);
    place(
        &full_prefix, PC_SIDE_PREFIX, 4,
        full_session->primary_group[4]);
    place(
        &full_prefix, PC_SIDE_SUFFIX, 5,
        full_session->primary_group[5]);
    full_prefix.eater_of_worlds_tier = 1;
    CalcContext full_calc(
        full_session, goal, full_registry, full_candidates);
    const StateLocalAutomaticBatch full_batch =
        full_calc.admit_state_local_automatic_candidates(
            full_calc.intern_item(full_prefix), limits);
    PC_CHECK(std::none_of(
        full_batch.decisions.begin(), full_batch.decisions.end(),
        [&](const StateLocalAutomaticCandidate& decision) {
            return decision_is_eldritch_exalt(full_calc, decision);
        }));

    /*
     * Remove the completed prefix from ordinary random support. Chaos can no
     * longer recreate it, while an Eldritch suffix action deliberately
     * preserves it. The resulting states are therefore true support deltas
     * and must be interned and expanded before the side action is classified.
     */
    auto delta_session = make_solve_session(
        {"incremental_goal"});
    delta_session->eldritch_eligible = true;
    delta_session->eldritch_searing_tier_mod_ids.resize(5);
    delta_session->eldritch_eater_tier_mod_ids.resize(5);
    for (std::uint32_t tier = 1; tier <= 4; ++tier) {
        delta_session->eldritch_searing_tier_mod_ids[tier] = {0};
        delta_session->eldritch_eater_tier_mod_ids[tier] = {5};
    }
    pc_bitset_clear(
        delta_session->normal_random_roll_mask.data(), 0);
    pc_bitset_clear(
        delta_session->positive_spawn_weight_mask.data(), 0);
    pc_bitset_clear(
        delta_session->positive_base_weight_mask.data(), 0);
    delta_session->base_spawn_weight[0] = 0;
    delta_session->base_roll_weight[0] = 0;
    delta_session->essence_guaranteed_mod_ids = {5};
    ActionRegistry delta_registry =
        build_action_registry(*delta_session);
    const std::uint32_t delta_essence_index =
        delta_registry.index_by_id.at(
            "essence:incremental_goal");
    const std::vector<std::uint32_t> delta_candidates{
        delta_registry.index_by_id.at("chaos"),
        delta_registry.index_by_id.at("annul"),
        delta_essence_index};
    CalcContext delta_calc(
        delta_session, goal, delta_registry, delta_candidates);
    SolveOptions delta_options = prefix_solve_options;
    auto delta_prices = prices;
    delta_prices["essence:incremental_goal"] =
        1000000.0;
    const SolveResult delta_solved = solve(
        delta_calc, repair_suffix, delta_prices, delta_options);
    std::printf(
        "solver automatic Eldritch delta: closed=%d outside=%llu "
        "states=%u expanded=%u admitted=%llu rejected=%llu "
        "unresolved=%llu\n",
        delta_solved.diagnostics
                .incremental_action_envelope_closed
            ? 1
            : 0,
        static_cast<unsigned long long>(
            delta_solved.diagnostics
                .incremental_states_outside_chaos_support),
        delta_solved.diagnostics.discovered_states,
        delta_solved.diagnostics.expanded_states,
        static_cast<unsigned long long>(
            delta_solved.diagnostics.incremental_actions_admitted),
        static_cast<unsigned long long>(
            delta_solved.diagnostics
                .incremental_actions_non_improving),
        static_cast<unsigned long long>(
            delta_solved.diagnostics.incremental_actions_unresolved));
    PC_CHECK(delta_solved.policy_available);
    PC_CHECK(
        delta_solved.diagnostics.sweeps <
        delta_options.max_sweeps);
    PC_CHECK(
        delta_solved.diagnostics
            .incremental_states_outside_chaos_support > 0);
    PC_CHECK(
        delta_solved.diagnostics
            .incremental_action_envelope_closed);
    const StateLocalAutomaticBatch delta_batch =
        delta_calc.admit_state_local_automatic_candidates(
            delta_solved.start_state, limits);
    bool checked_delta_eldritch_exit = false;
    for (const std::uint32_t operator_index :
         delta_batch.admitted_operators) {
        const PlannerOperator& planner =
            delta_calc.operators().at(operator_index);
        if (planner.automatic_kind !=
                AutomaticCandidateKind::EldritchSide ||
            planner.intended_side != PC_SIDE_SUFFIX) {
            continue;
        }
        const OptionKernel& kernel =
            delta_calc.option_kernel(
                delta_solved.start_state, operator_index);
        for (const OutcomeEntry& exit : kernel.exits) {
            if (delta_calc.is_goal_state(
                    delta_calc.state(exit.state))) {
                continue;
            }
            checked_delta_eldritch_exit = true;
            PC_CHECK(exit.state < delta_solved.expanded.size());
            if (exit.state < delta_solved.expanded.size()) {
                PC_CHECK(delta_solved.expanded[exit.state]);
            }
        }
    }
    PC_CHECK(checked_delta_eldritch_exit);
    if (delta_solved.policy_available) {
        const PolicyOperatorRef selected =
            delta_solved.policy[delta_solved.start_state];
        PC_CHECK(selected != kNoId);
        if (selected != kNoId) {
            const PlannerOperator& planner =
                delta_calc.operators().at(selected.index);
            PC_CHECK(
                planner.automatic_kind ==
                AutomaticCandidateKind::EldritchSide);
            PC_CHECK(planner.intended_side == PC_SIDE_SUFFIX);
        }
    }

    auto ineligible = make_solve_session();
    ineligible->eldritch_eligible = false;
    ActionRegistry ineligible_registry =
        build_action_registry(*ineligible);
    PC_CHECK(
        !ineligible_registry.index_by_id.contains(
            "eldritch_chaos"));
    PC_CHECK(
        !ineligible_registry.index_by_id.contains(
            "eldritch_annul"));
    PC_CHECK(
        !ineligible_registry.index_by_id.contains(
            "eldritch_exalt"));
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

    /*
     * Artifact-backed split-side control. Keep one real prefix goal, put
     * unrelated junk on the suffix side, and require a real suffix goal.
     * Automatic Eldritch planning must be available on this Body Armour and
     * Bellman remains free to choose the exact suffix-side repair.
     */
    PC_CHECK(session->eldritch_eligible);
    std::uint32_t artifact_prefix_goal = kNoId;
    std::uint32_t artifact_suffix_goal = kNoId;
    std::uint32_t artifact_suffix_junk = kNoId;
    const auto pure_family_side =
        [&](const std::uint32_t family) {
            std::int8_t side = -1;
            for (std::uint32_t candidate = 0;
                 candidate < session->mod_count; ++candidate) {
                if (session->family_id[candidate] != family) continue;
                const std::int8_t candidate_side =
                    session->gen_type[candidate];
                if (candidate_side != PC_SIDE_PREFIX &&
                    candidate_side != PC_SIDE_SUFFIX) {
                    continue;
                }
                if (side == -1) {
                    side = candidate_side;
                } else if (side != candidate_side) {
                    return static_cast<std::int8_t>(-1);
                }
            }
            return side;
        };
    for (std::uint32_t mod = 0; mod < session->mod_count; ++mod) {
        if (!pc_bitset_test(
                session->normal_random_roll_mask.data(), mod) ||
            !pc_bitset_test(
                session->positive_base_weight_mask.data(), mod) ||
            session->primary_group[mod] == kNoId ||
            session->family_id[mod] == kNoId) {
            continue;
        }
        const std::int8_t family_side =
            pure_family_side(session->family_id[mod]);
        if (family_side == PC_SIDE_PREFIX &&
            artifact_prefix_goal == kNoId) {
            artifact_prefix_goal = mod;
        } else if (family_side == PC_SIDE_SUFFIX) {
            if (artifact_suffix_goal == kNoId) {
                artifact_suffix_goal = mod;
            } else if (
                session->family_id[mod] !=
                    session->family_id[artifact_suffix_goal]) {
                artifact_suffix_junk = mod;
                break;
            }
        }
    }
    PC_CHECK(artifact_prefix_goal != kNoId);
    PC_CHECK(artifact_suffix_goal != kNoId);
    PC_CHECK(artifact_suffix_junk != kNoId);
    if (artifact_prefix_goal != kNoId &&
        artifact_suffix_goal != kNoId &&
        artifact_suffix_junk != kNoId) {
        GoalSpec split_goal;
        split_goal.rarity = PC_RARITY_RARE;
        split_goal.automatic_candidates = true;
        GoalSlot split_prefix;
        split_prefix.family_id =
            session->family_id[artifact_prefix_goal];
        split_goal.slots.push_back(split_prefix);
        GoalSlot split_suffix;
        split_suffix.family_id =
            session->family_id[artifact_suffix_goal];
        split_goal.slots.push_back(split_suffix);
        const std::vector<std::uint32_t> split_candidates{
            registry.index_by_id.at("chaos"),
            registry.index_by_id.at("annul"),
            registry.index_by_id.at("restart")};
        std::unordered_map<std::string, double> split_prices{
            {"chaos", 1000.0},
            {"annul", 1000.0},
            {"base", 1000.0},
            {"eldritch_chaos", 1.0},
            {"eldritch_annul", 1.0},
            {"eldritch_ember:1", 0.4},
            {"eldritch_ember:2", 0.3},
            {"eldritch_ember:3", 0.2},
            {"eldritch_ember:4", 0.1},
            {"eldritch_ichor:1", 0.4},
            {"eldritch_ichor:2", 0.3},
            {"eldritch_ichor:3", 0.2},
            {"eldritch_ichor:4", 0.1}};
        pc_item_state split_start;
        pc_item_clear(&split_start);
        split_start.rarity = PC_RARITY_RARE;
        place(
            &split_start, PC_SIDE_PREFIX, artifact_prefix_goal,
            session->primary_group[artifact_prefix_goal]);
        place(
            &split_start, PC_SIDE_SUFFIX, artifact_suffix_junk,
            session->primary_group[artifact_suffix_junk]);
        split_start.searing_exarch_tier = 1;
        split_start.eater_of_worlds_tier = 2;

        CalcContext split_calc(
            session, split_goal, registry, split_candidates);
        const std::uint32_t split_start_state =
            split_calc.intern_item(split_start);
        const AbstractState& split_abstract =
            split_calc.state(split_start_state);
        PC_CHECK(!split_calc.is_goal_state(split_abstract));
        PC_CHECK(
            split_abstract.slot_status[0] ==
            static_cast<std::uint8_t>(
                GoalSlotStatus::Satisfied));
        PC_CHECK(
            split_abstract.slot_status[1] ==
            static_cast<std::uint8_t>(
                GoalSlotStatus::Absent));
        PC_CHECK(
            registry.index_by_id.contains(
                "eldritch_ember:1"));
        PC_CHECK(
            registry.index_by_id.contains(
                "eldritch_ichor:1"));
        pc_item_state split_materialized;
        const bool split_materialized_ok =
            split_calc.materialize(
                split_start_state, split_materialized);
        const OutcomeDistribution& raw_split_annul =
            split_calc.outcomes(
                split_start_state,
                registry.index_by_id.at("eldritch_annul"));
        const OutcomeDistribution& raw_split_chaos =
            split_calc.outcomes(
                split_start_state,
                registry.index_by_id.at("eldritch_chaos"));
        PC_CHECK(split_materialized_ok);
        PC_CHECK(
            split_materialized.searing_exarch_tier == 1);
        PC_CHECK(
            split_materialized.eater_of_worlds_tier == 2);
        PC_CHECK(raw_split_annul.supported);
        PC_CHECK(!raw_split_annul.entries.empty());
        PC_CHECK(raw_split_chaos.supported);
        PC_CHECK(!raw_split_chaos.entries.empty());
        AutomaticAdmissionLimits split_limits;
        split_limits.max_state_action_rows = 300000;
        split_limits.max_transitions = 10000000;
        split_limits.max_solver_owned_bytes =
            512ull * 1024ull * 1024ull;
        split_limits.prices = &split_prices;
        const StateLocalAutomaticBatch split_batch =
            split_calc.admit_state_local_automatic_candidates(
                split_start_state, split_limits);
        bool saw_artifact_suffix_side = false;
        for (const StateLocalAutomaticCandidate& decision :
             split_batch.decisions) {
            if (!decision.admitted ||
                decision.kind !=
                    AutomaticCandidateKind::EldritchSide) {
                continue;
            }
            const PlannerOperator& planner =
                split_calc.operators().at(decision.operator_index);
            saw_artifact_suffix_side |=
                planner.intended_side == PC_SIDE_SUFFIX;
        }
        PC_CHECK(saw_artifact_suffix_side);

        CalcContext split_solve_calc(
            session, split_goal, registry, split_candidates);
        SolveOptions split_options;
        split_options.goal_progress_gated_reforges = true;
        split_options.max_discovered_states = 200000;
        split_options.max_expanded_states = 25000;
        split_options.max_reforge_work = 100000000;
        split_options.max_solver_owned_bytes =
            512ull * 1024ull * 1024ull;
        const SolveResult split_solved = solve(
            split_solve_calc, split_start, split_prices,
            split_options);
        PC_CHECK(split_solved.policy_available);
        if (split_solved.policy_available) {
            const PolicyOperatorRef selected =
                split_solved.policy[split_solved.start_state];
            PC_CHECK(selected != kNoId);
            if (selected != kNoId) {
                const PlannerOperator& planner =
                    split_solve_calc.operators().at(selected.index);
                PC_CHECK(
                    planner.automatic_kind ==
                    AutomaticCandidateKind::EldritchSide);
                PC_CHECK(planner.intended_side == PC_SIDE_SUFFIX);
            }
        }
        std::printf(
            "solver artifact Eldritch split-side: policy=%d "
            "status=%u states=%u expanded=%u\n",
            split_solved.policy_available ? 1 : 0,
            static_cast<unsigned>(split_solved.policy_status),
            split_solved.diagnostics.discovered_states,
            split_solved.diagnostics.expanded_states);
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

void run_solver_automatic_eldritch_tests() {
    run_resource_stop_reachable_policy_tests();
    run_automatic_imprint_cooperative_tests();
    run_automatic_eldritch_side_tests();
}

void run_solver_policy_refinement_tests() {
    run_shared_sparse_policy_kernel_tests();
    run_policy_guided_exact_lift_tests();
    run_policy_guided_exalt_lift_tests();
    run_policy_guided_local_reoptimization_tests();
    run_candidate_induced_exact_subclass_tests();
    run_policy_guided_primitive_choice_reoptimization_tests();
    run_future_observed_choice_finalization_tests();
    run_policy_guided_fixed_choice_reoptimization_tests();
    run_policy_guided_improper_cycle_repair_tests();
    run_mixed_side_rare_cap_reporting_regression();
}

void run_solver_solve_tests(const char* artifact_dir) {
    const SolveOptions default_options;
    PC_CHECK(default_options.max_reforge_work == 50000000);
    PC_CHECK(
        default_options.max_solver_owned_bytes ==
        1024ull * 1024ull * 1024ull);
    run_bounded_policy_row_capture_tests();
    run_certified_fallback_contract_tests();
    run_direct_certification_contract_tests();
    run_alt_spam_tests();
    run_solver_policy_refinement_tests();
    run_constructive_state_certificate_tests();
    run_constructive_renewal_upper_tests();
    run_primitive_destructive_renewal_upper_tests();
    run_goal_progress_gated_reforge_tests();
    run_incremental_action_generation_tests();
    run_automatic_eldritch_side_tests();
    run_artifact_solve_tests(artifact_dir);
}
