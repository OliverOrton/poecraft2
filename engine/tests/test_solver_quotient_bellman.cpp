#include "tests.hpp"

#include "../src/solver_quotient_bellman.hpp"

#include <cmath>
#include <cstdint>
#include <map>
#include <utility>
#include <vector>

using namespace poecraft;
using namespace poecraft::solver;
using namespace poecraft::solver::quotient;

namespace {

QuotientBellmanCellInput cell(
        const std::uint32_t id,
        const bool terminal = false) {
    return {id, 1, StableKey{9000 + id}, terminal};
}

CertifiedRowIdentity proof_for(
        const QuotientBellmanRowInput& row,
        const std::map<std::uint32_t, StableKey>& identities,
        const std::uint64_t seed) {
    refinement::SelectedAction selected;
    selected.action_id = row.operator_index;
    selected.semantic_key = {10000 + seed};
    selected.contract.schema_version = kActionRefinementContractVersion;

    CertifiedRowIdentity proof;
    proof.source_coarse_parent = {11000 + row.source_cell_id};
    proof.action_id = row.operator_index;
    proof.semantic_action_identity = selected.semantic_key;
    proof.runtime_contract_program_identity =
        refinement::canonical_selected_runtime_contract_identity(selected);
    proof.exact_choice_recipe_identity = {12000 + seed};
    proof.session_identity = {13000};
    proof.layout_identity = {13001};
    proof.goal_identity = {13002};
    proof.artifact_identity = {13003};
    proof.start_identity = {13004};
    proof.solver_options_identity = {13005};
    proof.coverage.strict_kernel_identity = {14000 + seed};
    proof.coverage.replay_authority_identity = {14001};
    proof.coverage.normalized_enumeration_identity = {14002};
    proof.coverage.ranges = {{
        {14003}, seed, 1, 1.0}};
    proof.coverage.exact_source_count = 1;
    proof.coverage.exact_total_probability = 1.0;
    proof.exact_total_probability = 1.0;
    for (const QuotientBellmanTransitionInput& transition :
         row.transitions) {
        proof.projected_arcs.push_back({
            transition.label,
            identities.at(transition.target_cell_id),
            transition.probability});
    }
    return canonical_certified_row_identity(std::move(proof));
}

QuotientBellmanRowInput certified_row(
        const std::uint32_t source,
        const std::uint32_t action,
        const double cost,
        std::vector<QuotientBellmanTransitionInput> transitions,
        const std::map<std::uint32_t, StableKey>& identities,
        const std::uint64_t seed) {
    QuotientBellmanRowInput row;
    row.source_cell_id = source;
    row.operator_index = action;
    row.cost = cost;
    row.certified = true;
    row.transitions = std::move(transitions);
    row.proof_identity = proof_for(row, identities, seed);
    return row;
}

void run_bellman_projection_and_invalidation_tests() {
    QuotientBellmanGraph graph(64ull * 1024 * 1024);
    graph.install_cells({cell(1), cell(2), cell(3, true)});
    const std::map<std::uint32_t, StableKey> identities{
        {1, {9001}}, {2, {9002}}, {3, {9003}}};

    const std::uint64_t direct = graph.append_row(certified_row(
        1, 10, 10.0, {{{1}, 3, 1.0}}, identities, 1));
    const std::uint64_t cyclic = graph.append_row(certified_row(
        1, 11, 1.0, {{{2}, 2, 1.0}}, identities, 2));
    const std::uint64_t exit = graph.append_row(certified_row(
        2, 12, 1.0,
        {{{3}, 1, 0.5}, {{4}, 3, 0.5}}, identities, 3));
    (void)exit;
    QuotientBellmanRowInput optimistic;
    optimistic.source_cell_id = 1;
    optimistic.operator_index = 13;
    optimistic.cost = 2.0;
    optimistic.admitted = true;
    optimistic.transitions = {{{5}, 3, 1.0}};
    const std::uint64_t uncertified = graph.append_row(std::move(optimistic));

    CoverageDescriptor alternative_coverage;
    alternative_coverage.strict_kernel_identity = {15000};
    alternative_coverage.replay_authority_identity = {15001};
    alternative_coverage.normalized_enumeration_identity = {15002};
    alternative_coverage.ranges = {{{15003}, 0, 1, 1.0}};
    alternative_coverage.exact_source_count = 1;
    alternative_coverage.exact_total_probability = 1.0;
    alternative_coverage = canonical_coverage_descriptor(
        std::move(alternative_coverage));
    UnresolvedAlternativeObligationIdentity alternative;
    alternative.source_cell_id = 1;
    alternative.source_cell_identity = identities.at(1);
    alternative.action = canonical_alternative_action_identity({
        14, {16000}, {16001}, {16002}});
    alternative.price_identity = {16003};
    alternative.vocabulary_identity = {16004};
    alternative.requirement_generation = 1;
    alternative.source_generation = 1;
    alternative.target_generation = 1;
    alternative.partition_generation = 1;
    alternative.action_generation = 1;
    alternative.admission_generation = 1;
    alternative.price_generation = 1;
    alternative.vocabulary_generation = 1;
    alternative.optimistic_lower = certify_carrier_wide_lower_q(
        alternative.source_cell_identity,
        alternative_coverage,
        {16005},
        {{{15003}, 0, 0.5}});
    alternative.resumable_work_identity = {16006};
    const std::uint32_t alternative_id = graph.proof_store()
        ->intern_alternative_obligation(std::move(alternative)).first;
    graph.proof_store()->transition_alternative_obligation(
        alternative_id, AlternativeObligationStatus::LowerOnly);

    const SolveTransitionCache& sparse = graph.transition_cache();
    PC_CHECK(sparse.rows.size() == 4);
    PC_CHECK(sparse.state_rows.at(0).count == 3);
    PC_CHECK(sparse.rows.at(direct).owner_state == 0);
    PC_CHECK(sparse.rows.at(cyclic).owner_state == 0);
    PC_CHECK(sparse.rows.at(uncertified).owner_state == 0);

    QuotientBellmanResult solved = graph.solve({1, 2});
    PC_CHECK(solved.status == QuotientBellmanStatus::Complete);
    PC_CHECK(solved.executable_upper);
    PC_CHECK(solved.proper);
    PC_CHECK(solved.publication_audit.complete());
    PC_CHECK(
        solved.publication_audit.reachable_selected_rows_current);
    PC_CHECK(solved.publication_audit.closed_target_dependencies);
    PC_CHECK(
        solved.publication_audit.terminal_reachable_bottom_sccs);
    PC_CHECK(solved.publication_audit.proper_every_entry);
    if (solved.status == QuotientBellmanStatus::Complete) {
        PC_CHECK(solved.selected_rows_by_state.at(0) == cyclic);
        PC_CHECK(std::fabs(solved.values_by_state.at(0) - 4.0) < 1e-10);
        PC_CHECK(std::fabs(solved.values_by_state.at(1) - 3.0) < 1e-10);
        PC_CHECK(solved.lower_relaxation_by_state.at(0) == 0.5);
    }
    PC_CHECK(graph.telemetry().bellman_rows_evaluated != 0);
    PC_CHECK(graph.telemetry().scc_evaluations != 0);
    PC_CHECK(graph.telemetry().reference_adapter_invocations == 0);

    const std::uint64_t price_generation =
        graph.proof_store()->price_generation();
    graph.note_price_change({{cyclic, 5.0}});
    PC_CHECK(graph.transition_cache().rows.size() == 4);
    PC_CHECK(graph.proof_store()->price_generation() == price_generation + 1);
    PC_CHECK(
        graph.proof_store()->alternative_obligation(alternative_id).status ==
        AlternativeObligationStatus::Stale);
    PC_CHECK(
        !graph.proof_store()
             ->optimistic_alternative_lower_for_source(1)
             .has_value());
    solved = graph.solve({1});
    PC_CHECK(solved.status == QuotientBellmanStatus::Complete);
    if (solved.status == QuotientBellmanStatus::Complete) {
        PC_CHECK(solved.selected_rows_by_state.at(0) == direct);
        PC_CHECK(std::fabs(solved.values_by_state.at(0) - 10.0) < 1e-10);
    }

    PC_CHECK(graph.invalidate_target_split(2, 2) == 1);
    solved = graph.solve({1});
    PC_CHECK(solved.status == QuotientBellmanStatus::Complete);
    if (solved.status == QuotientBellmanStatus::Complete) {
        PC_CHECK(solved.selected_rows_by_state.at(0) == direct);
    }
    PC_CHECK(graph.telemetry().target_splits == 1);
    PC_CHECK(graph.telemetry().reverse_invalidations == 1);

    const std::uint64_t reprojected_cycle = graph.append_row(certified_row(
        1, 11, 1.0, {{{2}, 2, 1.0}}, identities, 2));
    solved = graph.solve({1});
    PC_CHECK(solved.status == QuotientBellmanStatus::Complete);
    if (solved.status == QuotientBellmanStatus::Complete) {
        PC_CHECK(solved.selected_rows_by_state.at(0) == reprojected_cycle);
    }
    PC_CHECK(graph.telemetry().proof_payload_reuses != 0);
    PC_CHECK(graph.telemetry().row_reprojections == 4);

    PC_CHECK(graph.invalidate_source_split(2, 3) == 1);
    solved = graph.solve({1});
    PC_CHECK(solved.status == QuotientBellmanStatus::Complete);
    PC_CHECK(solved.publication_audit.complete());
    if (solved.status == QuotientBellmanStatus::Complete) {
        PC_CHECK(solved.selected_rows_by_state.at(0) == direct);
    }
    PC_CHECK(graph.telemetry().source_splits == 1);
    PC_CHECK(graph.telemetry().reverse_invalidations == 2);

    graph.append_row(certified_row(
        2, 12, 1.0,
        {{{3}, 1, 0.5}, {{4}, 3, 0.5}}, identities, 3));
    solved = graph.solve({1});
    PC_CHECK(solved.status == QuotientBellmanStatus::Complete);
    PC_CHECK(solved.executable_upper);
    PC_CHECK(graph.telemetry().memory.bytes[
                 static_cast<std::size_t>(
                     ProofMemoryCategory::RowKernel)] != 0);
}

void run_improper_and_determinism_tests() {
    QuotientBellmanGraph repairable;
    repairable.install_cells({cell(7), cell(8), cell(9, true)});
    const std::map<std::uint32_t, StableKey> repair_identities{
        {7, {9007}}, {8, {9008}}, {9, {9009}}};
    repairable.append_row(certified_row(
        7, 18, 1.0, {{{1}, 8, 1.0}}, repair_identities, 7));
    const std::uint64_t repaired_exit = repairable.append_row(certified_row(
        7, 19, 5.0, {{{1}, 9, 1.0}}, repair_identities, 8));
    repairable.append_row(certified_row(
        8, 20, 1.0, {{{1}, 7, 1.0}}, repair_identities, 9));
    repairable.append_row(certified_row(
        8, 21, 5.0, {{{1}, 9, 1.0}}, repair_identities, 10));
    const QuotientBellmanResult repaired = repairable.solve({7});
    PC_CHECK(repaired.status == QuotientBellmanStatus::Complete);
    PC_CHECK(repaired.proper);
    PC_CHECK(repaired.publication_audit.complete());
    PC_CHECK(repaired.selected_rows_by_state.at(0) == repaired_exit);
    PC_CHECK(repairable.telemetry().policy_improvements != 0);
    PC_CHECK(repairable.telemetry().improper_policy_repairs != 0);

    QuotientBellmanGraph graph;
    graph.install_cells({cell(10), cell(11)});
    const std::map<std::uint32_t, StableKey> identities{
        {10, {9010}}, {11, {9011}}};
    graph.append_row(certified_row(
        10, 20, 1.0, {{{1}, 11, 1.0}}, identities, 10));
    graph.append_row(certified_row(
        11, 21, 1.0, {{{1}, 10, 1.0}}, identities, 11));
    const QuotientBellmanResult improper = graph.solve({10});
    PC_CHECK(improper.status == QuotientBellmanStatus::ImproperPolicy);
    PC_CHECK(!improper.executable_upper);
    PC_CHECK(!improper.publication_audit.complete());

    QuotientBellmanGraph reversed;
    reversed.install_cells({cell(11), cell(10)});
    reversed.append_row(certified_row(
        11, 21, 1.0, {{{1}, 10, 1.0}}, identities, 11));
    reversed.append_row(certified_row(
        10, 20, 1.0, {{{1}, 11, 1.0}}, identities, 10));
    const QuotientBellmanResult reversed_improper = reversed.solve({10});
    PC_CHECK(reversed_improper.status == improper.status);
    PC_CHECK(
        reversed.transition_cache().discovered_states ==
        graph.transition_cache().discovered_states);
}

} // namespace

void run_solver_quotient_bellman_tests() {
    run_bellman_projection_and_invalidation_tests();
    run_improper_and_determinism_tests();
}
