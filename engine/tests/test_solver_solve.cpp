#include "tests.hpp"

#include "../src/json.hpp"
#include "../src/solver_compile_contracts.hpp"
#include "../src/solver_policy_refinement.hpp"
#include "../src/solver_sparse_policy.hpp"
#include "../src/solver_solve_types.hpp"
#include "poecraft/bitset.h"
#include "poecraft/item_state.h"

#include <algorithm>
#include <array>
#include <bit>
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

namespace {

std::shared_ptr<SessionImpl> make_solve_session(
    const std::vector<std::string>& essence_keys = {});

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
             "executable_provenance_missing");
    changed = fallback;
    changed.compilation_provenance_present = false;
    PC_CHECK(invalid_reason(changed) ==
             "executable_provenance_missing");
    changed = fallback;
    changed.complete_policy_or_witness = false;
    PC_CHECK(invalid_reason(changed) ==
             "executable_provenance_missing");
    changed = fallback;
    changed.independently_evaluated = false;
    PC_CHECK(invalid_reason(changed) ==
             "executable_provenance_missing");

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

    const auto tied = solve_detail::select_sparse_policy_row(
        graph, 0, 1e-3,
        [](const std::uint64_t) { return true; },
        [](const std::uint64_t row, std::uint32_t& work) {
            work = 1;
            return row == 0 ? 24.0 : 23.9995;
        });
    PC_CHECK(tied.row == 0);
    PC_CHECK(tied.evaluated_rows == 2);
    PC_CHECK(tied.transition_work == 2);
    const auto improving = solve_detail::select_sparse_policy_row(
        graph, 0, 1e-3,
        [](const std::uint64_t) { return true; },
        [](const std::uint64_t row, std::uint32_t& work) {
            work = 0;
            return row == 0 ? 24.0 : 23.9;
        });
    PC_CHECK(improving.row == 1);

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
        PolicyCompilationTelemetry compilation_sample;
        compilation_sample.working_states = 23;
        compilation_sample.policy_regions = 24;
        compilation_sample.nodes = 25;
        compilation_sample.edges = 26;
        compilation_sample.strategy_json_bytes = 27;
        compilation_sample.peak_owned_bytes = 28;
        const std::string refinement_telemetry =
            serialize_solver_telemetry(
                calc, &refinement_sample, nullptr, std::nullopt,
                &compilation_sample);
        PC_CHECK(valid_json_object(refinement_telemetry));
        PC_CHECK(refinement_telemetry.find(
                     "\"policy_refinement\":{\"triggers\":1,"
                     "\"status\":\"complete\",\"resource_cap\":null,"
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
                     "\"working_states\":23,\"policy_regions\":24,"
                     "\"nodes\":25,\"edges\":26,"
                     "\"strategy_json_bytes\":27,"
                     "\"peak_owned_bytes\":28,\"cap_hit\":null}") !=
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
    PC_CHECK(
        successful_refined_publication_termination(
            SolveTermination::ExactClosed, false) ==
        SolveTermination::ExactClosed);
    PC_CHECK(
        successful_refined_publication_termination(
            SolveTermination::RefusedResourceCap, false) ==
        SolveTermination::RefusedResourceCap);
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
            SolveTermination::NoExecutablePolicy, false) ==
        SolveTermination::ExactClosed);

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
            SolvePolicyStatus::BoundedFeasible);
        PC_CHECK(
            compile_limited_fallback.diagnostics.policy_refinement
                .fallback_publication_attempts == 1);
        PC_CHECK(
            compile_limited_fallback.diagnostics.policy_refinement
                .fallback_publication_successes == 1);
        PC_CHECK(near(
            compile_limited_fallback.upper_bound,
            capped_fallback.upper_bound, 1e-12));
        PC_CHECK(
            compile_limited_fallback.diagnostics.policy_refinement
                .preferred_publication_failure_reason.find(
                    "max_strategy_json_bytes") != std::string::npos);
    }
    PC_CHECK(solved.policy_available);
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
    const PolicyRefinementTelemetry& refinement_telemetry =
        solved.diagnostics.policy_refinement;
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

    const refinement::PolicyExactLiftCertificate lifted =
        refinement::lift_policy_exact(
            calc, solved, start, prices, options,
            "focused policy-guided exact lift");
    report_lift_failure("Regal lift", lifted);
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
        lifted.refinement.telemetry.merged_exact_states > 0);
    PC_CHECK(
        lifted.refinement.telemetry.lumpability_checks > 0);
    PC_CHECK(
        lifted.adapter.canonical_successor_collapses > 0);

    refinement::RefinementLimits interrupted_limits;
    interrupted_limits.max_exact_kernels =
        static_cast<std::uint32_t>(
            refinement_telemetry.selected_rows_completed);
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
    PC_CHECK(
        interrupted.adapter
            .alternative_obligations_resource_interrupted == 1);
    PC_CHECK(interrupted.adapter.bounded_publication_retained);
    PC_CHECK(
        interrupted.adapter.competitive_alternatives_remaining > 0);
    PC_CHECK(
        !interrupted.adapter.exact_alternative_envelope_closed);
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
    PC_CHECK(
        solved.policy[solved.start_state].index == exalt);
    PC_CHECK(
        solved.diagnostics.policy_refinement.triggers > 0);
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
        /* A native bounded policy and a compilable strategy are separate
         * contracts. Remove one stable vocabulary key after the native
         * policy has been certified: compilation must refuse without
         * relabelling the policy as exact or erasing its finite bound. */
        const std::shared_ptr<DataImpl> mutable_data =
            std::const_pointer_cast<DataImpl>(session->data);
        const std::string saved_key = mutable_data->strings.at(2);
        mutable_data->strings.at(2).clear();
        bool compile_refused = false;
        try {
            (void)compile_policy_strategy_json(
                calc, result, "inexpressible bounded policy");
        } catch (const std::runtime_error&) {
            compile_refused = true;
        }
        mutable_data->strings.at(2) = saved_key;
        PC_CHECK(compile_refused);
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
    PC_CHECK(!target.converged);
    PC_CHECK(target.policy_available);
    PC_CHECK(target.policy_status ==
             SolvePolicyStatus::BoundedNearOptimal);
    PC_CHECK(target.termination == SolveTermination::TargetGap);
    PC_CHECK(target.target_met);
    PC_CHECK(target.target_fired == SolveGapTarget::Absolute);
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
        !product_fracture_result.diagnostics
             .product_fracture_witnesses.empty());
    PC_CHECK(
        product_fracture_result.diagnostics.action_search_costs.at(
            "fracture").cache_requests == 0);
    PC_CHECK(
        product_fracture_result.diagnostics.action_search_costs.at(
            "chaos").root_retained_transitions ==
        product_chaos_kernel.entries.size());
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
    for (const OutcomeEntry& entry : unrestricted_entries) {
        const AbstractState& state = calc.state(entry.state);
        if (calc.is_goal_state(state)) {
            terminal_probability += entry.probability;
        } else if (satisfied_goal_count(state, goal) == 0) {
            retry_probability += entry.probability;
        } else {
            partial_probability[entry.state] += entry.probability;
        }
    }
    PC_CHECK(terminal_probability > 0.0);
    PC_CHECK(retry_probability > 0.0);
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
    const double restricted_success_probability =
        restricted_calc.outcomes(
            restricted_start_state, restricted_chaos, true)
            .gated_terminal_probability;
    PC_CHECK(restricted_success_probability > 0.0);
    PC_CHECK(near(
        restricted.values[restricted.start_state],
        1.0 / restricted_success_probability, 1e-9));
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
                 "\"description\":\"Exact within the "
                 "zero-progress-reroll policy restriction") !=
             std::string::npos);
    PC_CHECK(restricted_json.find("_gated_route") !=
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
    PC_CHECK(early_compilation.nodes == 4);
    PC_CHECK(early_compilation.edges == 4);
    PC_CHECK(early_compilation.policy_regions == 1);
    PC_CHECK(early_json.find(
                 "\"description\":\"Bounded executable fixed "
                 "destructive-renewal policy exact within the "
                 "zero-progress-reroll restriction") !=
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
    bool stale_renewal_refused = false;
    try {
        (void)compile_policy_strategy_json(
            early_renewal_calc, stale_renewal,
            "stale gated destructive renewal");
    } catch (const std::runtime_error&) {
        stale_renewal_refused = true;
    }
    PC_CHECK(stale_renewal_refused);

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
    const SolveResult result = solve(calc, start, prices, options);
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
    /* A bounded incumbent is not returned when the same declared row cap
     * cannot certify its compiled exact policy. */
    PC_CHECK(!capped.converged);
    PC_CHECK(!capped.policy_available);
    PC_CHECK(
        capped.policy_status == SolvePolicyStatus::None);
    PC_CHECK(!capped.diagnostics.policy_compatibility_supported);
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
    if (!prompt_progress.done &&
        prompt_progress.expanded_states ==
            prompt_options.max_expanded_states) {
        prompt_work.step(1);
        prompt_progress = prompt_work.progress();
        ++prompt_work_units;
    }
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
    AutomaticAdmissionLimits limits;
    limits.max_discovered_states = 10000;
    limits.max_state_action_rows = 10000;
    limits.max_transitions = 100000;
    limits.max_reforge_work = 1000000;
    limits.max_solver_owned_bytes = 256ull * 1024ull * 1024ull;
    limits.max_imprint_program_depth = 3;
    limits.max_imprint_program_work = 256;
    limits.prices = &prices;

    pc_item_state repair_prefix;
    pc_item_clear(&repair_prefix);
    repair_prefix.rarity = PC_RARITY_RARE;
    place(
        &repair_prefix, PC_SIDE_PREFIX, 2,
        session->primary_group[2]);
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
    PC_CHECK(prefix_eldritch.size() == 2);
    bool saw_prefix_annul = false;
    bool saw_prefix_chaos = false;
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
    prefix_solve_options.max_reforge_work = limits.max_reforge_work;
    prefix_solve_options.max_sweeps = 512;
    const SolveResult prefix_solved = solve(
        prefix_solve_calc, repair_prefix, prices,
        prefix_solve_options);
    PC_CHECK(prefix_solved.policy_available);
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
            strategy_json.find("\"type\":\"eldritch_chaos\"") !=
                std::string::npos ||
            strategy_json.find("\"type\":\"eldritch_annul\"") !=
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
    PC_CHECK(direct_count == 2);

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
    std::uint32_t suffix_count = 0;
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
        ++suffix_count;
    }
    PC_CHECK(suffix_count == 2);

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
        split_limits.max_discovered_states = 200000;
        split_limits.max_state_action_rows = 300000;
        split_limits.max_transitions = 10000000;
        split_limits.max_reforge_work = 100000000;
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
