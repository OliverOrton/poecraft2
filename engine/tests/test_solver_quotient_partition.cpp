#include "tests.hpp"

#include "../src/solver_quotient_partition.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <vector>

using namespace poecraft;
using namespace poecraft::solver;
using namespace poecraft::solver::quotient;

namespace {

struct CoverageFixture {
    CoverageDescriptor descriptor;
    std::vector<CoverageCarrier> carriers;
};

CoverageFixture coverage_for(
        const std::vector<CertifiedCarrierNode>& nodes,
        const std::uint64_t identity = 7000) {
    CoverageFixture out;
    out.descriptor.strict_kernel_identity = {identity};
    out.descriptor.replay_authority_identity = {identity + 1};
    out.descriptor.normalized_enumeration_identity = {identity + 2};
    const double mass = static_cast<double>(nodes.size()) * 0.125;
    out.descriptor.ranges = {{
        {identity + 3}, 0, nodes.size(), mass}};
    out.descriptor.exact_source_count = nodes.size();
    out.descriptor.exact_total_probability = mass;
    for (std::uint64_t index = 0; index < nodes.size(); ++index) {
        out.carriers.push_back({
            nodes[index].stable_key,
            {identity + 3},
            index,
            0.125});
    }
    return out;
}

CertifiedCarrierNode node(
        const std::uint64_t key,
        const std::uint32_t coarse,
        const std::uint64_t parent,
        const std::uint64_t immediate,
        const bool terminal = false) {
    CertifiedCarrierNode out;
    out.stable_key = {key};
    out.coarse_state = coarse;
    out.coarse_parent = {parent};
    out.immediate_identity = terminal ? StableKey{} : StableKey{immediate};
    out.terminal = terminal;
    return out;
}

void internal_arc(
        CertifiedCarrierNode& source,
        const std::uint64_t target,
        const double probability,
        StableKey label = {}) {
    source.arcs.push_back({
        std::move(label), StableKey{target}, std::nullopt, probability});
}

void external_arc(
        CertifiedCarrierNode& source,
        StableKey frontier,
        const double probability,
        StableKey label = {}) {
    source.arcs.push_back({
        std::move(label), std::nullopt, std::move(frontier), probability});
}

QuotientPartitionResult partition(
        const CoverageFixture& coverage,
        std::vector<CertifiedCarrierNode> nodes,
        std::vector<CertifiedEntry> entries,
        const QuotientPartitionState* previous = nullptr,
        ProofStore* proofs = nullptr) {
    return refine_certified_quotient_partition(
        coverage.descriptor,
        coverage.carriers,
        std::move(nodes),
        std::move(entries),
        previous,
        proofs);
}

const QuotientCell* cell_for_coarse(
        const QuotientPartitionState& state,
        const std::uint32_t coarse) {
    const auto found = std::find_if(
        state.cells.begin(), state.cells.end(),
        [&](const QuotientCell& cell) {
            return cell.coarse_state == coarse;
        });
    return found == state.cells.end() ? nullptr : &*found;
}

double cell_arc_mass(const QuotientCell& cell) {
    double result = 0.0;
    for (const QuotientCellArc& arc : cell.arcs) {
        result += arc.probability;
    }
    return result;
}

std::vector<CertifiedCarrierNode> minimal_open_cycle() {
    std::vector<CertifiedCarrierNode> nodes{
        node(10, 0, 1, 100),
        node(11, 0, 1, 100),
        node(30, 1, 3, 0, true),
        node(31, 2, 4, 0, true)};
    internal_arc(nodes[0], 11, 0.5);
    internal_arc(nodes[0], 30, 0.5);
    internal_arc(nodes[1], 10, 0.5);
    internal_arc(nodes[1], 31, 0.5);
    return nodes;
}

void run_four_node_and_equal_cycle_tests() {
    const std::vector<CertifiedCarrierNode> four = minimal_open_cycle();
    const CoverageFixture coverage = coverage_for(four);
    ProofMemoryLedger replay_ledger;
    CoverageReplaySlice live_slice = replay_coverage(
        coverage.descriptor,
        coverage.descriptor.replay_authority_identity,
        [&](const CoverageRange&) {
            std::vector<CoverageCarrier> replayed = coverage.carriers;
            std::reverse(replayed.begin(), replayed.end());
            return replayed;
        },
        replay_ledger);
    const QuotientPartitionResult split =
        refine_certified_quotient_partition(
            coverage.descriptor,
            live_slice.carriers(),
            four,
            {{{10}, 0.5}, {{11}, 0.5}},
            nullptr,
            nullptr);
    PC_CHECK(split.status == QuotientPartitionStatus::Complete);
    PC_CHECK(split.closed);
    PC_CHECK(split.lumpable);
    PC_CHECK(!split.properness_checked);
    PC_CHECK(split.telemetry.initial_classes == 3);
    PC_CHECK(split.telemetry.final_classes == 4);
    PC_CHECK(split.state.cells.size() == 4);
    PC_CHECK(split.telemetry.exact_carriers_replayed == 4);
    PC_CHECK(split.telemetry.exact_coverage_mass == 0.5);
    PC_CHECK(split.telemetry.exact_entry_mass == 1.0);
    PC_CHECK(replay_ledger.snapshot().live_slice_count == 1);
    PC_CHECK(!split.counterexamples.empty());
    PC_CHECK(
        split.counterexamples.front().kind ==
        refinement::CounterexampleKind::SuccessorProjection);
    std::uint64_t retained = 0;
    for (const QuotientCell& cell : split.state.cells) {
        retained += cell.coverage.exact_source_count;
        PC_CHECK(cell.semantic_identity.size() > 1);
        if (!cell.terminal) {
            PC_CHECK(std::abs(cell_arc_mass(cell) - 1.0) < 1e-12);
        }
    }
    PC_CHECK(retained == 4);
    live_slice.reset();
    PC_CHECK(replay_ledger.snapshot().live_slice_count == 0);

    std::vector<CertifiedCarrierNode> equal{
        node(10, 0, 1, 100),
        node(11, 0, 1, 100),
        node(20, 1, 2, 200),
        node(21, 1, 2, 200)};
    internal_arc(equal[0], 20, 1.0);
    internal_arc(equal[1], 21, 1.0);
    internal_arc(equal[2], 10, 1.0);
    internal_arc(equal[3], 11, 1.0);
    const QuotientPartitionResult merged = partition(
        coverage_for(equal, 7100), equal,
        {{{10}, 0.5}, {{11}, 0.5}});
    PC_CHECK(merged.status == QuotientPartitionStatus::Complete);
    PC_CHECK(merged.state.cells.size() == 2);
    PC_CHECK(merged.telemetry.initial_classes == 2);
    PC_CHECK(merged.telemetry.final_classes == 2);
    PC_CHECK(merged.counterexamples.empty());
    for (const QuotientCell& cell : merged.state.cells) {
        PC_CHECK(cell.coverage.exact_source_count == 2);
        PC_CHECK(cell.arcs.size() == 1);
        PC_CHECK(cell.arcs.front().probability == 1.0);
    }

    std::vector<CertifiedCarrierNode> improper{
        node(50, 0, 5, 500), node(51, 0, 5, 500)};
    internal_arc(improper[0], 51, 1.0);
    internal_arc(improper[1], 50, 1.0);
    const QuotientPartitionResult improper_result = partition(
        coverage_for(improper, 7200), improper,
        {{{50}, 1.0}});
    PC_CHECK(
        improper_result.status == QuotientPartitionStatus::Complete);
    PC_CHECK(improper_result.state.cells.size() == 1);
    PC_CHECK(!improper_result.properness_checked);
    PC_CHECK(
        improper_result.state.cells.front().arcs.front().target_cell_id ==
        improper_result.state.cells.front().cell_id);

    refinement::RefinementResult exact_policy;
    exact_policy.status = refinement::RefinementStatus::Complete;
    exact_policy.executable = true;
    exact_policy.lumpable = true;
    for (const QuotientCell& cell : improper_result.state.cells) {
        refinement::RefinedPolicyClass refined;
        refined.class_id = cell.cell_id;
        refined.coarse_state = cell.coarse_state;
        refined.coarse_state_key = cell.coarse_parent;
        refined.terminal = cell.terminal;
        refined.action_cost = cell.terminal ? 0.0 : 1.0;
        if (!cell.terminal) {
            refinement::SelectedAction selected;
            selected.action_id = 1;
            selected.semantic_key = cell.immediate_identity;
            selected.contract.schema_version =
                kActionRefinementContractVersion;
            refined.selected_action = std::move(selected);
        }
        for (const QuotientCellArc& arc : cell.arcs) {
            if (arc.target_cell_id.has_value()) {
                refined.transitions.push_back(
                    {*arc.target_cell_id, arc.probability});
            }
        }
        exact_policy.classes.push_back(std::move(refined));
    }
    refinement::PolicyEvaluationRequest evaluation_request;
    evaluation_request.start_classes = {
        improper_result.state.cells.front().cell_id};
    const refinement::PolicyEvaluationResult evaluation =
        refinement::evaluate_refined_policy_exact(
            exact_policy, evaluation_request);
    PC_CHECK(
        evaluation.status ==
        refinement::PolicyEvaluationStatus::ImproperPolicy);
    PC_CHECK(!evaluation.proper);
    PC_CHECK(evaluation.improper_component_count == 1);
}

std::vector<CertifiedCarrierNode> delayed_split_cycle() {
    std::vector<CertifiedCarrierNode> nodes{
        node(10, 0, 1, 100),
        node(11, 0, 1, 100),
        node(20, 1, 2, 200),
        node(21, 1, 2, 200),
        node(30, 2, 3, 0, true),
        node(31, 3, 4, 0, true)};
    internal_arc(nodes[0], 20, 1.0);
    internal_arc(nodes[1], 21, 1.0);
    internal_arc(nodes[2], 10, 0.5);
    internal_arc(nodes[2], 30, 0.5);
    internal_arc(nodes[3], 11, 0.5);
    internal_arc(nodes[3], 31, 0.5);
    return nodes;
}

void run_multiple_entry_and_determinism_tests() {
    const std::vector<CertifiedCarrierNode> nodes = delayed_split_cycle();
    const CoverageFixture coverage = coverage_for(nodes, 7300);
    const std::vector<CertifiedEntry> entries{
        {{10}, 0.25}, {{11}, 0.75}};
    const QuotientPartitionResult forward =
        partition(coverage, nodes, entries);
    PC_CHECK(forward.status == QuotientPartitionStatus::Complete);
    PC_CHECK(forward.telemetry.initial_classes == 4);
    PC_CHECK(forward.telemetry.final_classes == 6);
    PC_CHECK(forward.telemetry.refinement_rounds >= 3);
    PC_CHECK(forward.state.entries.size() == 2);
    double entry_mass = 0.0;
    for (const QuotientEntry& entry : forward.state.entries) {
        entry_mass += entry.probability;
    }
    PC_CHECK(entry_mass == 1.0);

    std::vector<CertifiedCarrierNode> reversed_nodes = nodes;
    std::reverse(reversed_nodes.begin(), reversed_nodes.end());
    for (CertifiedCarrierNode& candidate : reversed_nodes) {
        std::reverse(candidate.arcs.begin(), candidate.arcs.end());
    }
    CoverageFixture reversed_coverage = coverage;
    std::reverse(
        reversed_coverage.carriers.begin(),
        reversed_coverage.carriers.end());
    std::vector<CertifiedEntry> reversed_entries = entries;
    std::reverse(reversed_entries.begin(), reversed_entries.end());
    const QuotientPartitionResult backward = partition(
        reversed_coverage,
        std::move(reversed_nodes),
        std::move(reversed_entries));
    PC_CHECK(backward.status == QuotientPartitionStatus::Complete);
    PC_CHECK(backward.state == forward.state);
    PC_CHECK(backward.counterexamples == forward.counterexamples);
    PC_CHECK(
        backward.telemetry.retained_coverage_ranges ==
        forward.telemetry.retained_coverage_ranges);
}

CertifiedRowIdentity proof_identity() {
    CertifiedRowIdentity identity;
    identity.source_coarse_parent = {1};
    identity.action_id = 1;
    identity.semantic_action_identity = {2};
    identity.runtime_contract_program_identity = {3};
    identity.exact_choice_recipe_identity = {4};
    identity.session_identity = {5};
    identity.layout_identity = {6};
    identity.goal_identity = {7};
    identity.artifact_identity = {8};
    identity.start_identity = {9};
    identity.solver_options_identity = {10};
    identity.coverage.strict_kernel_identity = {11};
    identity.coverage.replay_authority_identity = {12};
    identity.coverage.normalized_enumeration_identity = {13};
    identity.coverage.ranges = {{{14}, 0, 1, 1.0}};
    identity.coverage.exact_source_count = 1;
    identity.coverage.exact_total_probability = 1.0;
    identity.exact_total_probability = 1.0;
    identity.projected_arcs = {{{}, {15}, 1.0}};
    return identity;
}

void apply_rarity_requirement(
        CertifiedCarrierNode& target,
        const std::uint64_t rarity) {
    target.observation_requirement.item_features =
        refinement_feature(RefinementFeature::Rarity);
    refinement::FeatureAtom atom;
    atom.feature = RefinementFeature::Rarity;
    atom.value = {rarity};
    target.exact_features = {atom};
}

void run_split_invalidation_and_requirement_tests() {
    std::vector<CertifiedCarrierNode> coarse{
        node(10, 0, 1, 100),
        node(11, 0, 1, 100),
        node(20, 1, 2, 200),
        node(30, 2, 3, 0, true)};
    internal_arc(coarse[0], 30, 1.0);
    internal_arc(coarse[1], 30, 1.0);
    internal_arc(coarse[2], 10, 0.5);
    internal_arc(coarse[2], 11, 0.5);
    const CoverageFixture coverage = coverage_for(coarse, 7400);
    const std::vector<CertifiedEntry> entries{{{20}, 1.0}};
    const QuotientPartitionResult initial =
        partition(coverage, coarse, entries);
    PC_CHECK(initial.status == QuotientPartitionStatus::Complete);
    PC_CHECK(initial.state.cells.size() == 3);
    const QuotientCell* merged = cell_for_coarse(initial.state, 0);
    const QuotientCell* predecessor = cell_for_coarse(initial.state, 1);
    const QuotientCell* terminal = cell_for_coarse(initial.state, 2);
    PC_CHECK(merged != nullptr);
    PC_CHECK(predecessor != nullptr);
    PC_CHECK(terminal != nullptr);
    PC_CHECK(merged->coverage.exact_source_count == 2);

    ProofStore proofs;
    const std::uint32_t payload =
        proofs.intern_payload(proof_identity()).first;
    proofs.attach_row(
        0, payload, merged->cell_id, merged->generation,
        {{terminal->cell_id, terminal->generation}}, 1, 1);
    proofs.attach_row(
        1, payload, predecessor->cell_id, predecessor->generation,
        {{merged->cell_id, merged->generation}}, 1, 1);

    std::vector<CertifiedCarrierNode> refined = coarse;
    apply_rarity_requirement(refined[0], PC_RARITY_RARE);
    apply_rarity_requirement(refined[1], PC_RARITY_MAGIC);
    const QuotientPartitionResult split = partition(
        coverage, refined, entries, &initial.state, &proofs);
    PC_CHECK(split.status == QuotientPartitionStatus::Complete);
    PC_CHECK(split.state.cells.size() == 4);
    PC_CHECK(split.telemetry.source_splits == 1);
    PC_CHECK(split.telemetry.target_splits == 1);
    PC_CHECK(split.telemetry.source_invalidations == 1);
    PC_CHECK(split.telemetry.reverse_invalidations == 1);
    PC_CHECK(!proofs.use_site(0).valid);
    PC_CHECK(!proofs.use_site(1).valid);
    PC_CHECK(!split.counterexamples.empty());
    PC_CHECK(
        split.counterexamples.front().kind ==
        refinement::CounterexampleKind::Observation);
    const QuotientCell* retained_predecessor =
        cell_for_coarse(split.state, 1);
    PC_CHECK(retained_predecessor != nullptr);
    PC_CHECK(retained_predecessor->cell_id == predecessor->cell_id);
    PC_CHECK(
        retained_predecessor->generation == predecessor->generation);
}

void run_population_growth_and_reference_tests() {
    std::vector<CertifiedCarrierNode> prefix{
        node(10, 0, 1, 100),
        node(11, 0, 1, 100),
        node(20, 1, 2, 200),
        node(30, 2, 3, 0, true)};
    internal_arc(prefix[0], 30, 1.0);
    internal_arc(prefix[1], 30, 1.0);
    internal_arc(prefix[2], 10, 0.5);
    internal_arc(prefix[2], 11, 0.5);
    const std::vector<CertifiedEntry> entries{{{20}, 1.0}};

    const QuotientPartitionResult initial = partition(
        coverage_for(prefix, 7600), prefix, entries);
    PC_CHECK(initial.status == QuotientPartitionStatus::Complete);
    PC_CHECK(initial.state.cells.size() == 3);
    PC_CHECK(initial.telemetry.new_cells == 3);
    PC_CHECK(initial.telemetry.retained_cells == 0);
    PC_CHECK(initial.telemetry.superseded_cells == 0);

    std::vector<CertifiedCarrierNode> first_growth = prefix;
    first_growth.push_back(node(40, 3, 4, 0, true));
    ProofStore proofs;
    const QuotientPartitionResult first = partition(
        coverage_for(first_growth, 7600), first_growth, entries,
        &initial.state, &proofs);
    PC_CHECK(first.status == QuotientPartitionStatus::Complete);
    PC_CHECK(first.state.cells.size() == 4);
    PC_CHECK(first.telemetry.retained_cells == 3);
    PC_CHECK(first.telemetry.new_cells == 1);
    PC_CHECK(first.telemetry.superseded_cells == 0);
    PC_CHECK(first.telemetry.source_splits == 0);
    PC_CHECK(first.telemetry.target_splits == 0);

    const QuotientCell* merged = cell_for_coarse(first.state, 0);
    const QuotientCell* predecessor = cell_for_coarse(first.state, 1);
    const QuotientCell* stable_terminal = cell_for_coarse(first.state, 2);
    const QuotientCell* growing_terminal = cell_for_coarse(first.state, 3);
    PC_CHECK(merged != nullptr);
    PC_CHECK(predecessor != nullptr);
    PC_CHECK(stable_terminal != nullptr);
    PC_CHECK(growing_terminal != nullptr);
    const std::uint32_t payload =
        proofs.intern_payload(proof_identity()).first;
    proofs.attach_row(
        0, payload, merged->cell_id, merged->generation,
        {{stable_terminal->cell_id, stable_terminal->generation}}, 1, 1);
    proofs.attach_row(
        1, payload, predecessor->cell_id, predecessor->generation,
        {{merged->cell_id, merged->generation}}, 1, 1);
    proofs.attach_row(
        2, payload, stable_terminal->cell_id, stable_terminal->generation,
        {{stable_terminal->cell_id, stable_terminal->generation}}, 1, 1);
    proofs.attach_row(
        3, payload, growing_terminal->cell_id,
        growing_terminal->generation,
        {{growing_terminal->cell_id, growing_terminal->generation}}, 1, 1);

    std::vector<CertifiedCarrierNode> second_growth = first_growth;
    second_growth.push_back(node(41, 3, 4, 0, true));
    apply_rarity_requirement(second_growth[0], PC_RARITY_RARE);
    apply_rarity_requirement(second_growth[1], PC_RARITY_MAGIC);
    const CoverageFixture second_coverage =
        coverage_for(second_growth, 7600);
    const QuotientPartitionResult grown = partition(
        second_coverage, second_growth, entries, &first.state, &proofs);
    PC_CHECK(grown.status == QuotientPartitionStatus::Complete);
    PC_CHECK(grown.state.partition_generation ==
             first.state.partition_generation + 1);
    PC_CHECK(grown.state.cells.size() == 5);
    PC_CHECK(grown.telemetry.source_splits == 1);
    PC_CHECK(grown.telemetry.target_splits == 1);
    PC_CHECK(grown.telemetry.new_cells == 2);
    PC_CHECK(grown.telemetry.superseded_cells == 1);
    PC_CHECK(grown.telemetry.retained_cells == 2);
    PC_CHECK(!proofs.use_site(0).valid);
    PC_CHECK(!proofs.use_site(1).valid);
    PC_CHECK(proofs.use_site(2).valid);
    PC_CHECK(!proofs.use_site(3).valid);

    const QuotientCell* retained_terminal =
        cell_for_coarse(grown.state, 2);
    const QuotientCell* superseded_terminal =
        cell_for_coarse(grown.state, 3);
    PC_CHECK(retained_terminal != nullptr);
    PC_CHECK(superseded_terminal != nullptr);
    PC_CHECK(retained_terminal->cell_id == stable_terminal->cell_id);
    PC_CHECK(retained_terminal->generation == stable_terminal->generation);
    PC_CHECK(superseded_terminal->cell_id == growing_terminal->cell_id);
    PC_CHECK(superseded_terminal->generation ==
             growing_terminal->generation + 1);
    PC_CHECK(superseded_terminal->coverage.exact_source_count == 2);

    const QuotientPartitionResult reference = partition(
        second_coverage, second_growth, entries);
    PC_CHECK(reference.status == QuotientPartitionStatus::Complete);
    PC_CHECK(reference.state.cells.size() == grown.state.cells.size());
    for (const QuotientCell& persistent_cell : grown.state.cells) {
        const auto matching = std::find_if(
            reference.state.cells.begin(), reference.state.cells.end(),
            [&](const QuotientCell& candidate) {
                return candidate.semantic_identity ==
                       persistent_cell.semantic_identity;
            });
        PC_CHECK(matching != reference.state.cells.end());
        PC_CHECK(matching->coarse_state == persistent_cell.coarse_state);
        PC_CHECK(matching->terminal == persistent_cell.terminal);
        PC_CHECK(matching->coverage == persistent_cell.coverage);
    }
}

void run_external_frontier_and_open_rejection_tests() {
    std::vector<CertifiedCarrierNode> external{
        node(80, 0, 8, 800), node(81, 0, 8, 800)};
    external_arc(external[0], {900}, 1.0, {1});
    external_arc(external[1], {901}, 1.0, {1});
    const CoverageFixture coverage = coverage_for(external, 7500);
    const QuotientPartitionResult distinct = partition(
        coverage, external, {{{80}, 0.5}, {{81}, 0.5}});
    PC_CHECK(distinct.status == QuotientPartitionStatus::Complete);
    PC_CHECK(distinct.state.cells.size() == 2);
    std::vector<StableKey> frontiers;
    for (const QuotientCell& cell : distinct.state.cells) {
        PC_CHECK(cell.arcs.size() == 1);
        PC_CHECK(!cell.arcs.front().target_cell_id.has_value());
        PC_CHECK(cell.arcs.front().external_frontier.has_value());
        frontiers.push_back(*cell.arcs.front().external_frontier);
        PC_CHECK(cell.arcs.front().label == StableKey{1});
        PC_CHECK(cell.arcs.front().probability == 1.0);
    }
    std::sort(frontiers.begin(), frontiers.end());
    const std::vector<StableKey> expected_frontiers{{900}, {901}};
    PC_CHECK(frontiers == expected_frontiers);

    std::vector<CertifiedCarrierNode> open = external;
    open[1].arcs.front().external_frontier.reset();
    const QuotientPartitionResult rejected = partition(
        coverage, std::move(open), {{{80}, 0.5}, {{81}, 0.5}});
    PC_CHECK(rejected.status == QuotientPartitionStatus::OpenFrontier);
    PC_CHECK(!rejected.closed);
    PC_CHECK(
        rejected.shared_partition_status ==
        refinement::ClosedPartitionStatus::EmptyGraph);
}

void run_replay_backed_shared_partition_tests() {
    std::vector<refinement::ClosedPartitionNode> nodes(4);
    nodes[0] = {{10}, {100}, {1000}, false,
                {{{1}, 1, 0.5}, {{2}, 2, 0.5}}};
    nodes[1] = {{11}, {100}, {1000}, false,
                {{{1}, 0, 0.5}, {{2}, 3, 0.5}}};
    nodes[2] = {{12}, {200}, {2000}, true, {}};
    nodes[3] = {{13}, {300}, {3000}, true, {}};
    const refinement::ClosedPartitionResult materialized =
        refinement::refine_closed_probabilistic_partition(nodes);
    std::uint32_t live = 0;
    std::uint32_t peak = 0;
    const refinement::ClosedPartitionResult replayed =
        refinement::refine_closed_probabilistic_partition_replay(
            static_cast<std::uint32_t>(nodes.size()),
            [&](const std::uint32_t index) {
                ++live;
                peak = std::max(peak, live);
                refinement::ClosedPartitionNode out = nodes.at(index);
                --live;
                return out;
            },
            {}, true);
    PC_CHECK(materialized.status ==
             refinement::ClosedPartitionStatus::Complete);
    PC_CHECK(replayed.status ==
             refinement::ClosedPartitionStatus::Complete);
    PC_CHECK(replayed.lumpable);
    PC_CHECK(replayed.initial_class_by_node ==
             materialized.initial_class_by_node);
    PC_CHECK(replayed.class_by_node == materialized.class_by_node);
    PC_CHECK(replayed.classes == materialized.classes);
    PC_CHECK(peak == 1);

    std::vector<refinement::ClosedPartitionNode> shared_materialized_nodes =
        nodes;
    shared_materialized_nodes[1].arcs =
        shared_materialized_nodes[0].arcs;
    const refinement::ClosedPartitionResult shared_materialized =
        refinement::refine_closed_probabilistic_partition(
            shared_materialized_nodes);
    std::vector<refinement::ClosedPartitionNode> shared_nodes =
        shared_materialized_nodes;
    shared_nodes[1].arcs.clear();
    shared_nodes[1].arc_source = 0;
    std::vector<std::optional<std::uint32_t>> known_sources(
        shared_nodes.size());
    known_sources[1] = 0;
    std::uint32_t authority_arc_replays = 0;
    const refinement::ClosedPartitionResult known_authority =
        refinement::refine_closed_probabilistic_partition_replay(
            static_cast<std::uint32_t>(shared_nodes.size()),
            [&](const std::uint32_t index) {
                if (!shared_nodes.at(index).arcs.empty()) {
                    ++authority_arc_replays;
                }
                return shared_nodes.at(index);
            },
            {}, true, {}, false, &known_sources);
    PC_CHECK(known_authority.status ==
             refinement::ClosedPartitionStatus::Complete);
    PC_CHECK(known_authority.lumpable);
    PC_CHECK(known_authority.class_by_node ==
             shared_materialized.class_by_node);
    PC_CHECK(authority_arc_replays > 0);
    const refinement::ClosedPartitionResult streamed_authority =
        refinement::refine_closed_probabilistic_partition_replay(
            static_cast<std::uint32_t>(shared_nodes.size()),
            [&](const std::uint32_t index) {
                return shared_nodes.at(index);
            },
            {}, true, {}, false, &known_sources, false);
    PC_CHECK(streamed_authority.status ==
             refinement::ClosedPartitionStatus::Complete);
    PC_CHECK(streamed_authority.lumpable);
    PC_CHECK(streamed_authority.initial_class_by_node ==
             shared_materialized.initial_class_by_node);
    PC_CHECK(streamed_authority.class_by_node ==
             shared_materialized.class_by_node);
    PC_CHECK(streamed_authority.classes ==
             shared_materialized.classes);
    const refinement::ClosedPartitionResult compact_authority =
        refinement::refine_closed_probabilistic_partition_replay(
            static_cast<std::uint32_t>(shared_nodes.size()),
            [&](const std::uint32_t index) {
                return shared_nodes.at(index);
            },
            {}, false, {}, false, &known_sources,
            false, false, true);
    PC_CHECK(compact_authority.status ==
             refinement::ClosedPartitionStatus::Complete);
    PC_CHECK(compact_authority.lumpable);
    PC_CHECK(compact_authority.final_class_count ==
             shared_materialized.final_class_count);
    for (std::size_t left = 0; left < shared_nodes.size(); ++left) {
        for (std::size_t right = 0; right < shared_nodes.size(); ++right) {
            PC_CHECK(
                (compact_authority.class_by_node[left] ==
                 compact_authority.class_by_node[right]) ==
                (shared_materialized.class_by_node[left] ==
                 shared_materialized.class_by_node[right]));
        }
    }

    std::vector<refinement::ClosedPartitionNode> observed = nodes;
    observed[1].observation_key = {101};
    const refinement::ClosedPartitionResult split =
        refinement::refine_closed_probabilistic_partition_replay(
            static_cast<std::uint32_t>(observed.size()),
            [&](const std::uint32_t index) {
                return observed.at(index);
            },
            replayed.class_by_node,
            false);
    PC_CHECK(split.status ==
             refinement::ClosedPartitionStatus::Complete);
    PC_CHECK(split.final_class_count >= replayed.final_class_count);
    PC_CHECK(split.classes.front().member_keys.empty());
}

} // namespace

void run_solver_quotient_partition_tests() {
    run_four_node_and_equal_cycle_tests();
    run_multiple_entry_and_determinism_tests();
    run_split_invalidation_and_requirement_tests();
    run_population_growth_and_reference_tests();
    run_external_frontier_and_open_rejection_tests();
    run_replay_backed_shared_partition_tests();
}
