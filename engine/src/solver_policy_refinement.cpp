#include "solver_policy_refinement_helpers.hpp"
#include "solver_quotient_bellman.hpp"
#include "solver_cooperative_task.hpp"

#include <chrono>

namespace poecraft {
namespace solver {
namespace refinement {

void merge_refined_compile_strict_members(
        std::vector<std::uint32_t>& represented_members,
        const std::vector<std::uint32_t>& streamed_members) {
    std::vector<std::uint32_t> merged;
    merged.reserve(
        represented_members.size() + streamed_members.size());
    std::set_union(
        represented_members.begin(), represented_members.end(),
        streamed_members.begin(), streamed_members.end(),
        std::back_inserter(merged));
    represented_members = std::move(merged);
}

namespace {

struct QuotientOracleRow {
    SelectedAction selected;
    ExactActionKernel kernel;
};

struct QuotientOracleCompactTransition {
    std::uint32_t strict_state = kNoId;
    double probability = 0.0;
};

struct QuotientOracleCompactRow {
    SelectedAction selected;
    ExactChoiceRecipe choice_recipe;
    double action_cost = 0.0;
    std::vector<QuotientOracleCompactTransition> transitions;
};

std::uint64_t quotient_compact_row_bytes(
        const QuotientOracleCompactRow& row) {
    std::uint64_t bytes = sizeof(QuotientOracleCompactRow);
    saturating_add(bytes, selected_action_bytes(row.selected));
    saturating_add(bytes, exact_choice_recipe_bytes(row.choice_recipe));
    saturating_add(
        bytes,
        row.transitions.capacity() *
            sizeof(QuotientOracleCompactTransition));
    return bytes;
}

struct QuotientAlternativeDescriptor {
    std::uint32_t operator_index = kNoId;
    quotient::AlternativeActionIdentity action;
    ObservationRequirement routing_observes;
    quotient::SharedStableKey resumable_work_identity;

    bool operator==(const QuotientAlternativeDescriptor&) const = default;
};

#include "solver_policy_oracle.hpp"
#include "solver_policy_oracle_evaluate.inc"
#include "solver_policy_oracle_resources.inc"
#include "solver_policy_oracle_improve.inc"

} // namespace

// Bounded test/debug oracle retained for structural comparisons only. The
// production lift below never calls this materialized scaffold.
PolicyExactLiftCertificate lift_policy_quotient_materialized_scaffold(
        CalcContext& coarse,
        const SolveResult& solved,
        const pc_item_state& exact_start,
        const std::unordered_map<std::string, double>& prices,
        const SolveOptions& options,
        const std::string& strategy_name,
        const RefinementLimits* limits_override) {
    PolicyExactLiftCertificate certificate;
    certificate.solver_cost = solved.evaluated_policy_cost;
    const RefinementLimits limits =
        limits_override == nullptr
            ? default_limits(coarse, solved, options)
            : *limits_override;
    PolicyLiftAdapterTelemetry telemetry;
    try {
        ReoptimizationSeed seed;
        seed.coarse_parents.insert(
            solved.diagnostics.policy_refinement
                .trigger_coarse_states.begin(),
            solved.diagnostics.policy_refinement
                .trigger_coarse_states.end());
        if (seed.coarse_parents.empty() &&
            solved.diagnostics.policy_compatibility_state != kNoId) {
            seed.coarse_parents.insert(
                solved.diagnostics.policy_compatibility_state);
        }
        ProductionPolicyOracle oracle(
            coarse, solved, exact_start, prices, options, limits,
            telemetry, seed.empty() ? nullptr : &seed);
        certificate.exact_root_key = oracle.root_key();

        struct Carrier {
            ExactState state;
            std::vector<QuotientOracleRow> rows;
        };
        std::vector<Carrier> carriers;
        std::map<StableKey, std::uint32_t> carrier_by_key;
        const auto intern = [&](ExactState state) {
            const auto found = carrier_by_key.find(state.stable_key);
            if (found != carrier_by_key.end()) {
                if (carriers[found->second].state != state) {
                    throw AdapterFailure(
                        PolicyExactLiftStatus::RefinementFailure,
                        "one quotient carrier identity changed its exact payload");
                }
                return found->second;
            }
            if (carriers.size() >= limits.max_exact_states) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::ResourceCap,
                    "proof-carrying quotient reached max_exact_states",
                    "max_exact_states");
            }
            const std::uint32_t index =
                static_cast<std::uint32_t>(carriers.size());
            carrier_by_key.emplace(state.stable_key, index);
            carriers.push_back({std::move(state), {}});
            return index;
        };
        intern(oracle.quotient_root_state());
        std::uint64_t transition_count = 0;
        for (std::size_t cursor = 0; cursor < carriers.size(); ++cursor) {
            Carrier& carrier = carriers[cursor];
            if (carrier.state.terminal) continue;
            carrier.rows = oracle.quotient_action_rows(carrier.state);
            for (const QuotientOracleRow& row : carrier.rows) {
                if (!std::isfinite(row.kernel.action_cost) ||
                    row.kernel.action_cost < 0.0 ||
                    row.kernel.transitions.empty()) {
                    throw AdapterFailure(
                        PolicyExactLiftStatus::RefinementFailure,
                        "quotient oracle returned an invalid admitted row");
                }
                solve_detail::WideFloat mass{0.0};
                for (const ExactTransition& transition :
                     row.kernel.transitions) {
                    if (!std::isfinite(transition.probability) ||
                        transition.probability < 0.0) {
                        throw AdapterFailure(
                            PolicyExactLiftStatus::RefinementFailure,
                            "quotient oracle returned an invalid transition");
                    }
                    if (transition.probability == 0.0) continue;
                    mass += solve_detail::WideFloat{transition.probability};
                    intern(transition.successor);
                    ++transition_count;
                    if (transition_count > limits.max_transitions) {
                        throw AdapterFailure(
                            PolicyExactLiftStatus::ResourceCap,
                            "proof-carrying quotient reached max_transitions",
                            "max_transitions");
                    }
                }
                if (std::fabs(mass.value() - 1.0) >
                    limits.probability_sum_tolerance) {
                    throw AdapterFailure(
                        PolicyExactLiftStatus::RefinementFailure,
                        "quotient admitted row probability does not sum to one");
                }
            }
        }

        telemetry.current_live_slices = 1;
        telemetry.peak_live_slices = 1;
        std::uint64_t carrier_bytes =
            carriers.capacity() * sizeof(Carrier) +
            carrier_by_key.size() *
                (sizeof(decltype(carrier_by_key)::value_type) +
                 3 * sizeof(void*));
        for (const Carrier& carrier : carriers) {
            carrier_bytes += exact_state_bytes(carrier.state);
            carrier_bytes +=
                carrier.rows.capacity() * sizeof(QuotientOracleRow);
            for (const QuotientOracleRow& row : carrier.rows) {
                carrier_bytes += selected_action_bytes(row.selected);
                carrier_bytes += row.kernel.transitions.capacity() *
                    sizeof(ExactTransition);
                for (const ExactTransition& transition :
                     row.kernel.transitions) {
                    carrier_bytes += exact_state_bytes(transition.successor);
                }
            }
        }
        telemetry.carrier_bytes = carrier_bytes;
        telemetry.current_live_slice_bytes = carrier_bytes;
        telemetry.peak_live_slice_bytes = carrier_bytes;

        const std::uint64_t oracle_bytes = oracle.estimated_owned_bytes();
        if (oracle_bytes >= limits.max_estimated_memory_bytes) {
            throw AdapterFailure(
                PolicyExactLiftStatus::ResourceCap,
                "strict quotient oracle leaves no proof-store memory",
                "max_estimated_memory_bytes");
        }
        quotient::QuotientBellmanGraph bellman(
            limits.max_estimated_memory_bytes - oracle_bytes);
        quotient::ProofMemoryLedger& ledger =
            bellman.proof_store()->ledger();
        quotient::ScopedProofMemoryCharge retained_carriers(
            ledger, quotient::ProofMemoryCategory::Carrier,
            carrier_bytes);
        ledger.charge_live_slice(carrier_bytes);

        quotient::CoverageDescriptor coverage;
        coverage.strict_kernel_identity = {
            0x7063717374726b31ull};
        coverage.strict_kernel_identity.insert(
            coverage.strict_kernel_identity.end(),
            certificate.exact_root_key.begin(),
            certificate.exact_root_key.end());
        coverage.replay_authority_identity = {
            0x7063717265706c31ull};
        coverage.replay_authority_identity.insert(
            coverage.replay_authority_identity.end(),
            certificate.exact_root_key.begin(),
            certificate.exact_root_key.end());
        coverage.normalized_enumeration_identity = {
            0x706371656e756d31ull};
        coverage.normalized_enumeration_identity.insert(
            coverage.normalized_enumeration_identity.end(),
            certificate.exact_root_key.begin(),
            certificate.exact_root_key.end());
        const StableKey range_identity{0x70637172616e6731ull};
        coverage.ranges = {{
            range_identity, 0, carriers.size(),
            static_cast<double>(carriers.size())}};
        coverage.exact_source_count = carriers.size();
        coverage.exact_total_probability =
            static_cast<double>(carriers.size());
        coverage = quotient::canonical_coverage_descriptor(
            std::move(coverage));

        std::vector<quotient::CoverageCarrier> exact_coverage;
        exact_coverage.reserve(carriers.size());
        for (std::uint32_t index = 0;
             index < carriers.size(); ++index) {
            const Carrier& carrier = carriers[index];
            exact_coverage.push_back({
                carrier.state.stable_key,
                range_identity,
                index,
                1.0});
        }

        const auto make_nodes = [&](const bool include_observations) {
            std::vector<quotient::CertifiedCarrierNode> nodes;
            nodes.reserve(carriers.size());
            for (const Carrier& carrier : carriers) {
                quotient::CertifiedCarrierNode node;
                node.stable_key = carrier.state.stable_key;
                node.coarse_state = carrier.state.coarse_state;
                node.coarse_parent = carrier.state.coarse_state_key;
                node.exact_features = carrier.state.features;
                node.terminal = carrier.state.terminal;
                if (!node.terminal) {
                    StableKey immediate{0x706371726f777331ull};
                    immediate.push_back(carrier.rows.size());
                    const double row_weight =
                        1.0 / static_cast<double>(carrier.rows.size());
                    for (const QuotientOracleRow& row : carrier.rows) {
                        if (include_observations) {
                            node.observation_requirement =
                                merge_observation_requirements(
                                    std::move(node.observation_requirement),
                                    row.selected.routing_observes);
                        }
                        immediate.push_back(row.selected.semantic_key.size());
                        immediate.insert(
                            immediate.end(),
                            row.selected.semantic_key.begin(),
                            row.selected.semantic_key.end());
                        immediate.push_back(
                            std::bit_cast<std::uint64_t>(
                                row.kernel.action_cost));
                        StableKey label{0x7063716163746e31ull};
                        label.push_back(row.selected.semantic_key.size());
                        label.insert(
                            label.end(),
                            row.selected.semantic_key.begin(),
                            row.selected.semantic_key.end());
                        for (const ExactTransition& transition :
                             row.kernel.transitions) {
                            if (transition.probability == 0.0) continue;
                            node.arcs.push_back({
                                label,
                                transition.successor.stable_key,
                                std::nullopt,
                                transition.probability * row_weight});
                        }
                    }
                    node.immediate_identity = std::move(immediate);
                }
                nodes.push_back(std::move(node));
            }
            return nodes;
        };

        quotient::QuotientPartitionResult observation_coarse =
            quotient::refine_certified_quotient_partition(
                coverage,
                exact_coverage,
                make_nodes(false),
                {{{certificate.exact_root_key}, 1.0}},
                nullptr,
                nullptr,
                {
                    limits.max_refinement_classes,
                    limits.max_refinement_rounds,
                    oracle_bytes + carrier_bytes,
                    limits.max_estimated_memory_bytes,
                    limits.probability_sum_tolerance});
        if (observation_coarse.status !=
                quotient::QuotientPartitionStatus::Complete ||
            !observation_coarse.closed || !observation_coarse.lumpable) {
            throw AdapterFailure(
                observation_coarse.status ==
                        quotient::QuotientPartitionStatus::ResourceCap
                    ? PolicyExactLiftStatus::ResourceCap
                    : PolicyExactLiftStatus::RefinementFailure,
                observation_coarse.failure_reason,
                observation_coarse.status ==
                        quotient::QuotientPartitionStatus::ResourceCap
                    ? "max_estimated_memory_bytes"
                    : std::string{});
        }
        quotient::ScopedProofMemoryCharge retained_coarse_partition(
            ledger, quotient::ProofMemoryCategory::Partition,
            observation_coarse.telemetry.partition_estimated_bytes);

        quotient::QuotientPartitionResult partition =
            quotient::refine_certified_quotient_partition(
                coverage,
                exact_coverage,
                make_nodes(true),
                {{{certificate.exact_root_key}, 1.0}},
                &observation_coarse.state,
                nullptr,
                {
                    limits.max_refinement_classes,
                    limits.max_refinement_rounds,
                    oracle_bytes + carrier_bytes,
                    limits.max_estimated_memory_bytes,
                    limits.probability_sum_tolerance});
        ledger.release_live_slice(carrier_bytes);
        telemetry.current_live_slices = 0;
        telemetry.current_live_slice_bytes = 0;
        telemetry.exact_carriers_replayed =
            observation_coarse.telemetry.exact_carriers_replayed +
            partition.telemetry.exact_carriers_replayed;
        telemetry.partition_bytes =
            partition.telemetry.partition_estimated_bytes;
        if (partition.status !=
                quotient::QuotientPartitionStatus::Complete ||
            !partition.closed || !partition.lumpable) {
            throw AdapterFailure(
                partition.status ==
                        quotient::QuotientPartitionStatus::ResourceCap
                    ? PolicyExactLiftStatus::ResourceCap
                    : PolicyExactLiftStatus::RefinementFailure,
                partition.failure_reason,
                partition.status ==
                        quotient::QuotientPartitionStatus::ResourceCap
                    ? "max_estimated_memory_bytes"
                    : std::string{});
        }
        quotient::ScopedProofMemoryCharge retained_partition(
            ledger, quotient::ProofMemoryCategory::Partition,
            partition.telemetry.partition_estimated_bytes);

        struct PublishedRow {
            std::uint64_t sparse_row = 0;
            std::uint32_t source_cell = 0;
            SelectedAction selected;
        };
        const auto map_partition = [&](const quotient::QuotientPartitionState& state) {
            std::vector<std::uint32_t> mapped(carriers.size(), kNoId);
            for (const quotient::QuotientCell& cell : state.cells) {
                for (const quotient::CoverageRange& range : cell.coverage.ranges) {
                    for (std::uint64_t index = range.begin;
                         index < range.begin + range.count; ++index) {
                        if (index >= mapped.size() || mapped[index] != kNoId) {
                            throw AdapterFailure(
                                PolicyExactLiftStatus::RefinementFailure,
                                "quotient partition coverage does not map uniquely");
                        }
                        mapped[index] = cell.cell_id;
                    }
                }
            }
            if (std::find(mapped.begin(), mapped.end(), kNoId) != mapped.end()) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::RefinementFailure,
                    "quotient partition coverage is incomplete");
            }
            return mapped;
        };
        const auto install_cells = [&](const quotient::QuotientPartitionState& state) {
            std::vector<quotient::QuotientBellmanCellInput> cells;
            cells.reserve(state.cells.size());
            for (const quotient::QuotientCell& cell : state.cells) {
                cells.push_back({
                    cell.cell_id, cell.generation,
                    cell.semantic_identity, cell.terminal});
            }
            bellman.install_cells(std::move(cells));
        };
        const auto publish_rows =
                [&](const quotient::QuotientPartitionState& state,
                   const std::vector<std::uint32_t>& mapped) {
            std::vector<PublishedRow> published;
            for (const quotient::QuotientCell& cell : state.cells) {
                if (cell.terminal || cell.coverage.ranges.empty()) continue;
                const std::uint64_t representative =
                    cell.coverage.ranges.front().begin;
                if (representative >= carriers.size()) {
                    throw AdapterFailure(
                        PolicyExactLiftStatus::RefinementFailure,
                        "quotient cell has no replayable representative");
                }
                const Carrier& carrier = carriers[representative];
                for (const QuotientOracleRow& row : carrier.rows) {
                    std::map<std::uint32_t, solve_detail::WideFloat> projected;
                    for (const ExactTransition& transition : row.kernel.transitions) {
                        if (transition.probability == 0.0) continue;
                        const auto successor = carrier_by_key.find(
                            transition.successor.stable_key);
                        if (successor == carrier_by_key.end()) {
                            throw AdapterFailure(
                                PolicyExactLiftStatus::RefinementFailure,
                                "quotient row successor is outside coverage");
                        }
                        projected[mapped[successor->second]] +=
                            solve_detail::WideFloat{transition.probability};
                    }
                    quotient::QuotientBellmanRowInput input;
                    input.source_cell_id = cell.cell_id;
                    input.operator_index = row.selected.action_id;
                    input.cost = row.kernel.action_cost;
                    input.certified = true;
                    std::vector<quotient::ProofProjectedArc> proof_arcs;
                    solve_detail::WideFloat projected_total{0.0};
                    for (const auto& [target, probability] : projected) {
                        input.transitions.push_back({{}, target, probability.value()});
                        proof_arcs.push_back({
                            {}, bellman.shared_semantic_identity_for_cell(target),
                            probability.value()});
                    }
                    projected_total = solve_detail::WideFloat{0.0};
                    for (const quotient::ProofProjectedArc& arc : proof_arcs) {
                        projected_total +=
                            solve_detail::WideFloat{arc.probability};
                    }
                    input.proof_identity = oracle.quotient_proof_identity(
                        carrier.state,
                        row.selected,
                        [&] {
                            quotient::CoverageDescriptor source = cell.coverage;
                            for (quotient::CoverageRange& range : source.ranges) {
                                range.total_probability =
                                    static_cast<double>(range.count) /
                                    static_cast<double>(source.exact_source_count) *
                                    projected_total.value();
                            }
                            source.exact_total_probability =
                                projected_total.value();
                            return quotient::canonical_coverage_descriptor(
                                std::move(source));
                        }(),
                        std::move(proof_arcs),
                        projected_total.value());
                    const std::uint64_t sparse_row =
                        bellman.append_row(std::move(input));
                    published.push_back({
                        sparse_row, cell.cell_id, row.selected});
                }
            }
            return published;
        };

        const std::vector<std::uint32_t> coarse_cell_by_carrier =
            map_partition(observation_coarse.state);
        install_cells(observation_coarse.state);
        const std::vector<PublishedRow> coarse_rows =
            publish_rows(observation_coarse.state, coarse_cell_by_carrier);
        (void)coarse_rows;

        const std::vector<std::uint32_t> cell_by_carrier =
            map_partition(partition.state);
        std::map<std::uint32_t, std::set<std::uint32_t>> children_by_old;
        for (std::size_t index = 0; index < carriers.size(); ++index) {
            children_by_old[coarse_cell_by_carrier[index]].insert(
                cell_by_carrier[index]);
        }
        for (const quotient::QuotientCell& old :
             observation_coarse.state.cells) {
            const std::set<std::uint32_t>& children =
                children_by_old.at(old.cell_id);
            if (children.size() > 1) {
                bellman.invalidate_source_split(
                    old.cell_id, old.generation + 1);
                bellman.invalidate_target_split(
                    old.cell_id, old.generation + 1);
                continue;
            }
            const quotient::QuotientCell* replacement =
                partition.state.find_cell(*children.begin());
            if (replacement == nullptr) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::RefinementFailure,
                    "quotient refinement lost a replacement cell");
            }
            if (replacement->generation > old.generation) {
                bellman.supersede_cell({
                    replacement->cell_id,
                    replacement->generation,
                    replacement->semantic_identity,
                    replacement->terminal});
            }
        }
        install_cells(partition.state);
        retained_coarse_partition.reset();
        const std::vector<PublishedRow> published_rows =
            publish_rows(partition.state, cell_by_carrier);
        std::vector<std::uint32_t> entry_cells;
        for (const quotient::QuotientEntry& entry :
             partition.state.entries) {
            entry_cells.push_back(entry.cell_id);
        }
        const quotient::QuotientBellmanResult solved_quotient =
            bellman.solve(
                entry_cells,
                limits.max_refinement_rounds,
                std::max<std::uint32_t>(1, options.max_sweeps));
        const quotient::QuotientBellmanTelemetry& bellman_telemetry =
            bellman.telemetry();
        telemetry.proof_payload_reuses =
            bellman_telemetry.proof_payload_reuses;
        telemetry.row_reprojections =
            bellman_telemetry.row_reprojections;
        telemetry.quotient_source_splits =
            bellman_telemetry.source_splits;
        telemetry.quotient_target_splits =
            bellman_telemetry.target_splits;
        telemetry.reverse_invalidations =
            bellman_telemetry.reverse_invalidations;
        telemetry.improper_policy_repairs =
            bellman_telemetry.improper_policy_repairs;
        telemetry.local_reoptimization_rounds =
            bellman_telemetry.scc_evaluations;
        telemetry.local_state_action_rows_evaluated =
            bellman_telemetry.bellman_rows_evaluated;
        telemetry.local_reoptimizations =
            bellman_telemetry.policy_improvements;
        telemetry.local_policy_changes =
            bellman_telemetry.policy_improvements;
        telemetry.quotient_attractor_ns = bellman_telemetry.attractor_ns;
        telemetry.quotient_lower_relaxation_ns =
            bellman_telemetry.lower_relaxation_ns;
        telemetry.quotient_policy_seed_ns =
            bellman_telemetry.policy_seed_ns;
        telemetry.quotient_envelope_construction_ns =
            bellman_telemetry.envelope_construction_ns;
        telemetry.quotient_exact_policy_evaluation_ns =
            bellman_telemetry.exact_policy_evaluation_ns;
        telemetry.quotient_policy_improvement_ns =
            bellman_telemetry.policy_improvement_ns;
        telemetry.quotient_publication_audit_ns =
            bellman_telemetry.publication_audit_ns;
        const quotient::ProofMemorySnapshot memory =
            bellman.proof_store()->ledger().snapshot();
        telemetry.coverage_descriptor_bytes = memory.bytes[
            static_cast<std::size_t>(
                quotient::ProofMemoryCategory::CoverageDescriptor)];
        telemetry.certificate_bytes = memory.bytes[
            static_cast<std::size_t>(
                quotient::ProofMemoryCategory::Certificate)];
        telemetry.dependency_sidecar_bytes = memory.bytes[
            static_cast<std::size_t>(
                quotient::ProofMemoryCategory::DependencySidecar)];
        telemetry.alternative_obligation_bytes = memory.bytes[
            static_cast<std::size_t>(
                quotient::ProofMemoryCategory::AlternativeObligation)];
        telemetry.row_kernel_bytes = memory.bytes[
            static_cast<std::size_t>(
                quotient::ProofMemoryCategory::RowKernel)];
        telemetry.scratch_bytes = memory.bytes[
            static_cast<std::size_t>(
                quotient::ProofMemoryCategory::Scratch)];
        telemetry.total_solver_owned_bytes =
            oracle_bytes + memory.peak_total_bytes;
        if (solved_quotient.status !=
                quotient::QuotientBellmanStatus::Complete ||
            !solved_quotient.executable_upper ||
            !solved_quotient.proper ||
            !solved_quotient.publication_audit.complete()) {
            throw AdapterFailure(
                solved_quotient.status ==
                        quotient::QuotientBellmanStatus::ResourceCap
                    ? PolicyExactLiftStatus::ResourceCap
                    : PolicyExactLiftStatus::RefinementFailure,
                solved_quotient.failure_reason,
                solved_quotient.status ==
                        quotient::QuotientBellmanStatus::ResourceCap
                    ? "max_estimated_memory_bytes"
                    : std::string{});
        }

        std::map<std::uint64_t, const PublishedRow*> publication_by_row;
        for (const PublishedRow& row : published_rows) {
            publication_by_row.emplace(row.sparse_row, &row);
        }
        std::map<std::uint32_t, std::uint32_t> local_by_cell;
        std::vector<const quotient::QuotientCell*> published_cells;
        std::set<std::uint32_t> reachable_cells(
            solved_quotient.reachable_cell_ids.begin(),
            solved_quotient.reachable_cell_ids.end());
        for (const quotient::QuotientCell& cell :
             partition.state.cells) {
            if (!reachable_cells.contains(cell.cell_id)) continue;
            local_by_cell.emplace(
                cell.cell_id,
                static_cast<std::uint32_t>(published_cells.size()));
            published_cells.push_back(&cell);
        }
        certificate.refinement.status = RefinementStatus::Complete;
        certificate.refinement.executable = true;
        certificate.refinement.lumpable = true;
        certificate.refinement.classes.resize(
            published_cells.size());
        const SolveTransitionCache& graph =
            bellman.transition_cache();
        for (std::uint32_t local = 0;
             local < published_cells.size(); ++local) {
            const quotient::QuotientCell& cell =
                *published_cells[local];
            RefinedPolicyClass& policy_class =
                certificate.refinement.classes[local];
            policy_class.class_id = local;
            policy_class.coarse_state = cell.coarse_state;
            policy_class.coarse_state_key = cell.coarse_parent;
            policy_class.terminal = cell.terminal;
            policy_class.goal = cell.terminal;
            policy_class.required_observations =
                cell.observation_requirement;
            policy_class.observation_signature =
                cell.observed_features;
            for (const quotient::CoverageRange& range :
                 cell.coverage.ranges) {
                for (std::uint64_t index = range.begin;
                     index < range.begin + range.count; ++index) {
                    policy_class.exact_members.push_back(
                        carriers.at(index).state.stable_key);
                }
            }
            if (cell.terminal) continue;
            const std::optional<std::uint32_t> graph_state =
                bellman.state_index_for_cell(cell.cell_id);
            if (!graph_state.has_value()) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::RefinementFailure,
                    "published quotient cell has no sparse state");
            }
            const std::uint32_t state = *graph_state;
            const std::uint64_t selected_row =
                solved_quotient.selected_rows_by_state.at(state);
            const PublishedRow& published =
                *publication_by_row.at(selected_row);
            policy_class.selected_action = published.selected;
            policy_class.action_cost =
                bellman.priced_rows().at(selected_row).cost;
            std::map<std::uint32_t, solve_detail::WideFloat> mass;
            const SparseRow& sparse = graph.rows.at(selected_row);
            for (std::uint32_t i = 0;
                 i < sparse.transition_count; ++i) {
                const std::uint64_t offset = sparse.transition_offset + i;
                const std::uint32_t target_state =
                    graph.successors.at(offset);
                const std::optional<std::uint32_t> target_cell =
                    bellman.cell_id_for_state(target_state);
                if (!target_cell.has_value()) {
                    throw AdapterFailure(
                        PolicyExactLiftStatus::RefinementFailure,
                        "sparse quotient row names an unknown target state");
                }
                mass[local_by_cell.at(*target_cell)] +=
                    solve_detail::WideFloat{
                        graph.probabilities.at(offset)};
            }
            for (const auto& [target, probability] : mass) {
                policy_class.transitions.push_back({
                    target, probability.value()});
            }
            for (const StableKey& member : policy_class.exact_members) {
                oracle.quotient_install_policy(
                    member, *policy_class.selected_action);
            }
        }
        for (const RefinedPolicyClass& policy_class :
             certificate.refinement.classes) {
            for (std::uint32_t member = 0;
                 member < policy_class.exact_members.size(); ++member) {
                certificate.refinement.assignments.push_back({
                    policy_class.coarse_state,
                    policy_class.class_id,
                    member});
            }
        }
        certificate.refinement.telemetry.exact_states =
            carriers.size();
        certificate.refinement.telemetry.final_refinement_classes =
            published_cells.size();
        certificate.refinement.telemetry.initial_observation_classes =
            partition.telemetry.initial_classes;
        certificate.refinement.telemetry.partition_refinement_rounds =
            partition.telemetry.refinement_rounds;
        certificate.refinement.telemetry.merged_exact_states =
            certificate.refinement.assignments.size() -
            published_cells.size();
        certificate.refinement.telemetry.lumpability_checks =
            carriers.size();
        certificate.refinement.telemetry.estimated_memory_bytes =
            memory.total_bytes;
        certificate.refinement.telemetry.peak_estimated_memory_bytes =
            memory.peak_total_bytes;

        const std::uint32_t root_cell = cell_by_carrier.at(0);
        certificate.root_refinement_class =
            local_by_cell.at(root_cell);
        PolicyEvaluationRequest evaluation_request;
        evaluation_request.limits.max_reachable_classes =
            limits.max_refinement_classes;
        evaluation_request.limits.max_component_iterations =
            std::max<std::uint32_t>(1, options.max_sweeps);
        evaluation_request.limits.max_estimated_memory_bytes =
            limits.max_estimated_memory_bytes;
        certificate.class_evaluation =
            evaluate_refined_policy_exact(
                certificate.refinement,
                std::move(evaluation_request));
        if (certificate.class_evaluation.status !=
                PolicyEvaluationStatus::Complete ||
            !certificate.class_evaluation.converged ||
            !certificate.class_evaluation.proper ||
            certificate.class_evaluation.start_values.size() != 1) {
            throw AdapterFailure(
                certificate.class_evaluation.status ==
                        PolicyEvaluationStatus::ResourceCap
                    ? PolicyExactLiftStatus::ResourceCap
                    : PolicyExactLiftStatus::RefinementFailure,
                certificate.class_evaluation.failure_reason,
                certificate.class_evaluation.resource_cap);
        }
        certificate.exact_start_cost =
            certificate.class_evaluation.start_values.front();
        certificate.coarse_value_reconciled = reconciled(
            certificate.exact_start_cost,
            certificate.solver_cost,
            certificate.absolute_cost_delta,
            certificate.relative_cost_delta);
        certificate.policy_changed =
            oracle.final_policy_changed(certificate.refinement);
        certificate.compiled = oracle.assert_lifted_policy(
            strategy_name,
            certificate.refinement,
            certificate.class_evaluation);
        certificate.lumpable = true;
        if (telemetry.reference_adapter_invocations != 0) {
            throw AdapterFailure(
                PolicyExactLiftStatus::RefinementFailure,
                "production quotient path invoked reconstruct-then-merge reference adapter");
        }
        if (certificate.compiled.status !=
                CompiledPolicyAssertionStatus::Complete ||
            !certificate.compiled.paired_default_only ||
            !certificate.compiled.executable ||
            !certificate.compiled.proper ||
            !certificate.compiled.zero_off_policy ||
            !certificate.compiled.evaluation.cost_complete ||
            !std::isfinite(certificate.compiled.exact_cost) ||
            certificate.compiled.exact_cost < 0.0) {
            throw AdapterFailure(
                certificate.compiled.status ==
                        CompiledPolicyAssertionStatus::ResourceCap
                    ? PolicyExactLiftStatus::ResourceCap
                    : PolicyExactLiftStatus::CompiledAssertionFailure,
                certificate.compiled.failure_reason,
                certificate.compiled.resource_cap);
        }
        certificate.status = PolicyExactLiftStatus::Complete;
        certificate.executable = true;
    } catch (const AdapterFailure& error) {
        certificate.status = error.status;
        certificate.failure_reason = error.what();
        certificate.resource_cap = error.cap;
    } catch (const quotient::ProofMemoryLimit& error) {
        certificate.status = PolicyExactLiftStatus::ResourceCap;
        certificate.failure_reason = error.what();
        certificate.resource_cap = "max_estimated_memory_bytes";
    } catch (const SolverResourceLimit& error) {
        certificate.status = PolicyExactLiftStatus::ResourceCap;
        certificate.failure_reason = error.what();
        certificate.resource_cap = adapter_resource_cap_name(error.cap_name());
    } catch (const std::exception& error) {
        certificate.status = PolicyExactLiftStatus::RefinementFailure;
        certificate.failure_reason = error.what();
    }
    certificate.adapter = std::move(telemetry);
    return certificate;
}

namespace {

struct QuotientPassResult {
    std::optional<PolicyExactLiftCertificate> certificate;
    std::vector<AbstractState> frontier_states;
};

ReoptimizationSeed quotient_reoptimization_seed(
        const SolveResult& solved) {
    ReoptimizationSeed seed;
    seed.coarse_parents.insert(
        solved.diagnostics.policy_refinement
            .trigger_coarse_states.begin(),
        solved.diagnostics.policy_refinement
            .trigger_coarse_states.end());
    if (seed.coarse_parents.empty() &&
        solved.diagnostics.policy_compatibility_state != kNoId) {
        seed.coarse_parents.insert(
            solved.diagnostics.policy_compatibility_state);
    }
    return seed;
}

struct PersistentRawRowPayload {
    SelectedAction selected;
    ExactChoiceRecipe choice_recipe;
    double action_cost = 0.0;
    std::vector<QuotientOracleCompactTransition> transitions;
};

struct PersistentPublishedRow {
    std::uint64_t sparse_row = 0;
    std::uint32_t source_cell_id = 0;
    std::uint64_t source_generation = 0;
    std::optional<std::uint32_t> selected_payload_id;
    SelectedAction selected;
};

using PersistentAlternativeOperatorIds =
    std::vector<std::uint32_t>;

struct PersistentSelectedClosure {
    std::vector<PersistentRawRowPayload> raw_rows;
    std::map<StableKey, std::uint32_t> raw_row_by_identity;
    std::map<StableKey, std::uint32_t> raw_row_by_reuse_identity;
    std::vector<std::uint32_t> locators;
    std::vector<std::uint32_t> ordinal_by_strict_state;
    std::vector<std::vector<std::uint32_t>> rows_by_ordinal;
    std::vector<
        std::shared_ptr<const PersistentAlternativeOperatorIds>>
        alternatives_by_ordinal;
    std::vector<std::uint32_t> alternative_payload_by_ordinal;
    std::vector<
        std::shared_ptr<const PersistentAlternativeOperatorIds>>
        alternative_descriptor_payloads;
    std::size_t discovery_cursor = 0;
    std::size_t installed_recipe_count = 0;
};

std::uint64_t persistent_selected_closure_bytes(
        const PersistentSelectedClosure& closure) {
    std::uint64_t bytes =
        closure.raw_rows.capacity() * sizeof(PersistentRawRowPayload) +
        closure.locators.capacity() * sizeof(std::uint32_t) +
        closure.ordinal_by_strict_state.capacity() *
            sizeof(std::uint32_t) +
        closure.rows_by_ordinal.capacity() *
            sizeof(std::vector<std::uint32_t>) +
        closure.alternatives_by_ordinal.capacity() *
            sizeof(std::shared_ptr<const PersistentAlternativeOperatorIds>) +
        closure.alternative_payload_by_ordinal.capacity() *
            sizeof(std::uint32_t) +
        closure.alternative_descriptor_payloads.capacity() *
            sizeof(std::shared_ptr<const PersistentAlternativeOperatorIds>);
    const auto add_identity_map = [&](const auto& identities) {
        saturating_add(
            bytes,
            identities.size() *
                (sizeof(typename std::decay_t<decltype(identities)>::
                            value_type) +
                 3 * sizeof(void*)));
        for (const auto& [identity, unused] : identities) {
            (void)unused;
            saturating_add(bytes, stable_key_bytes(identity));
        }
    };
    add_identity_map(closure.raw_row_by_identity);
    add_identity_map(closure.raw_row_by_reuse_identity);
    for (const PersistentRawRowPayload& row : closure.raw_rows) {
        saturating_add(bytes, selected_action_bytes(row.selected));
        saturating_add(bytes, exact_choice_recipe_bytes(row.choice_recipe));
        saturating_add(
            bytes,
            row.transitions.capacity() *
                sizeof(QuotientOracleCompactTransition));
    }
    for (const auto& rows : closure.rows_by_ordinal) {
        saturating_add(
            bytes, rows.capacity() * sizeof(std::uint32_t));
    }
    for (const auto& descriptors :
         closure.alternative_descriptor_payloads) {
        saturating_add(
            bytes,
            descriptors->capacity() * sizeof(std::uint32_t));
    }
    return bytes;
}

std::uint64_t persistent_published_rows_bytes(
        const std::vector<PersistentPublishedRow>& rows) {
    std::uint64_t bytes =
        rows.capacity() * sizeof(PersistentPublishedRow);
    for (const PersistentPublishedRow& row : rows) {
        saturating_add(bytes, selected_action_bytes(row.selected));
    }
    return bytes;
}

struct PersistentQuotientSession {
    RefinementLimits limits;
    PolicyLiftAdapterTelemetry telemetry;
    ReoptimizationSeed seed;
    ProductionPolicyOracle oracle;
    PersistentSelectedClosure selected_closure;
    quotient::QuotientBellmanGraph bellman;
    std::vector<PersistentPublishedRow> published_rows;
    quotient::QuotientPartitionState partition_state;
    bool partition_initialized = false;
    bool initialized = false;
    std::chrono::steady_clock::time_point started_at =
        std::chrono::steady_clock::now();

    PersistentQuotientSession(
            CalcContext& coarse,
            const SolveResult& solved,
            const pc_item_state& exact_start,
            const std::unordered_map<std::string, double>& prices,
            const SolveOptions& options,
            RefinementLimits limits_value)
        : limits(std::move(limits_value)),
          seed(quotient_reoptimization_seed(solved)),
          oracle(
              coarse, solved, exact_start, prices, options, limits,
              telemetry, nullptr, false, true, true),
          bellman(limits.max_estimated_memory_bytes) {}
};

solve_detail::CooperativeTask<QuotientPassResult>
lift_policy_quotient_pass_task(
        CalcContext& coarse,
        const SolveResult& solved,
        const pc_item_state& exact_start,
        const std::unordered_map<std::string, double>& prices,
        const SolveOptions& options,
        const std::string& strategy_name,
        PersistentQuotientSession& session,
        std::vector<AbstractState> frontier_seeds,
        PolicyExactLiftProgress& progress,
        std::optional<CompiledPolicyAssertion>* reusable_assertion,
        const double verified_rollback_upper_bound) {
    PolicyExactLiftCertificate certificate;
    certificate.solver_cost = solved.evaluated_policy_cost;
    const RefinementLimits& limits = session.limits;
    PolicyLiftAdapterTelemetry& telemetry = session.telemetry;
    const auto elapsed_certification_ns = [&] {
        return static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() -
                session.started_at)
                .count());
    };
    try {
        progress = {};
        progress.verified_executable_upper_bound =
            verified_rollback_upper_bound;
        if (std::isfinite(verified_rollback_upper_bound) &&
            verified_rollback_upper_bound >= 0.0) {
            telemetry.external_verified_upper_seeded = true;
            if (!telemetry.work_to_first_executable_upper.has_value()) {
                telemetry.work_to_first_executable_upper = 0;
                telemetry.wall_ns_to_first_executable_upper = 0;
                telemetry.alternatives_materialized_before_first_upper = 0;
            }
        }
        progress.phase = PolicyExactLiftPhase::CarrierDiscovery;
        co_await solve_detail::CooperativeCheckpoint{};
        ProductionPolicyOracle& oracle = session.oracle;
        if (!session.initialized) {
            auto oracle_initialization = oracle.initialize_cooperatively();
            while (!oracle_initialization.resume()) {
                ++progress.work_items;
                co_await solve_detail::CooperativeCheckpoint{
                    oracle.estimated_owned_bytes()};
            }
            (void)oracle_initialization.take_result();
            session.initialized = true;
            saturating_add(telemetry.strict_session_constructions, 1);
        }
        progress.strict_states = 1;
        co_await solve_detail::CooperativeCheckpoint{
            oracle.estimated_owned_bytes()};
        certificate.exact_root_key = oracle.root_key();
        const std::uint32_t root_locator =
            oracle.quotient_locator(certificate.exact_root_key);

        quotient::QuotientBellmanGraph& bellman = session.bellman;
        quotient::ProofMemoryLedger& ledger =
            bellman.proof_store()->ledger();

        using RawRowPayload = PersistentRawRowPayload;
        using AlternativeOperatorIds = PersistentAlternativeOperatorIds;
        PersistentSelectedClosure& closure = session.selected_closure;
        std::vector<RawRowPayload>& raw_rows = closure.raw_rows;
        std::map<StableKey, std::uint32_t>& raw_row_by_identity =
            closure.raw_row_by_identity;
        std::map<StableKey, std::uint32_t>& raw_row_by_reuse_identity =
            closure.raw_row_by_reuse_identity;
        std::vector<std::uint32_t>& locators = closure.locators;
        std::vector<std::uint32_t>& ordinal_by_strict_state =
            closure.ordinal_by_strict_state;
        std::vector<std::vector<std::uint32_t>>& rows_by_ordinal =
            closure.rows_by_ordinal;
        std::vector<std::shared_ptr<const AlternativeOperatorIds>>&
            alternatives_by_ordinal = closure.alternatives_by_ordinal;
        std::vector<std::uint32_t>& alternative_payload_by_ordinal =
            closure.alternative_payload_by_ordinal;
        std::vector<std::shared_ptr<const AlternativeOperatorIds>>&
            alternative_descriptor_payloads =
                closure.alternative_descriptor_payloads;

        const auto ensure_locator_capacity = [&](const std::uint32_t locator) {
            if (ordinal_by_strict_state.size() <= locator) {
                ordinal_by_strict_state.resize(
                    static_cast<std::size_t>(locator) + 1, kNoId);
            }
        };
        const auto intern_locator = [&](const std::uint32_t locator) {
            ensure_locator_capacity(locator);
            if (ordinal_by_strict_state[locator] != kNoId) {
                return ordinal_by_strict_state[locator];
            }
            if (locators.size() >= limits.max_exact_states) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::ResourceCap,
                    "streamed quotient reached max_exact_states",
                    "max_exact_states");
            }
            const std::uint32_t ordinal =
                static_cast<std::uint32_t>(locators.size());
            ordinal_by_strict_state[locator] = ordinal;
            locators.push_back(locator);
            rows_by_ordinal.emplace_back();
            alternatives_by_ordinal.push_back(nullptr);
            alternative_payload_by_ordinal.push_back(kNoId);
            return ordinal;
        };
        intern_locator(root_locator);
        for (const AbstractState& frontier : frontier_seeds) {
            intern_locator(oracle.quotient_seed_locator(frontier));
        }

        const auto canonical_raw_row = [&](QuotientOracleCompactRow row) {
            std::map<std::uint32_t, solve_detail::WideFloat> mass;
            for (const QuotientOracleCompactTransition& transition :
                 row.transitions) {
                if (!std::isfinite(transition.probability) ||
                    transition.probability < 0.0) {
                    throw AdapterFailure(
                        PolicyExactLiftStatus::RefinementFailure,
                        "streamed quotient row has an invalid probability");
                }
                if (transition.probability == 0.0) continue;
                mass[transition.strict_state] +=
                    solve_detail::WideFloat{transition.probability};
            }
            row.transitions.clear();
            solve_detail::WideFloat total{0.0};
            for (const auto& [locator, probability] : mass) {
                row.transitions.push_back({locator, probability.value()});
                total += solve_detail::WideFloat{probability.value()};
            }
            if (row.transitions.empty() ||
                std::fabs(total.value() - 1.0) >
                    limits.probability_sum_tolerance) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::RefinementFailure,
                    "streamed quotient row is not stochastic");
            }
            return row;
        };
        const auto raw_identity = [&](const QuotientOracleCompactRow& row) {
            StableKey identity{
                0x7063717261777231ull,
                row.selected.action_id,
                std::bit_cast<std::uint64_t>(row.action_cost)};
            const StableKey runtime =
                canonical_selected_runtime_contract_identity(row.selected);
            identity.push_back(row.selected.semantic_key.size());
            identity.insert(
                identity.end(), row.selected.semantic_key.begin(),
                row.selected.semantic_key.end());
            identity.push_back(runtime.size());
            identity.insert(identity.end(), runtime.begin(), runtime.end());
            identity.push_back(row.transitions.size());
            for (const QuotientOracleCompactTransition& transition :
                 row.transitions) {
                identity.push_back(transition.strict_state);
                identity.push_back(std::bit_cast<std::uint64_t>(
                    transition.probability));
            }
            return identity;
        };

        for (std::size_t& cursor = closure.discovery_cursor;
             cursor < locators.size(); ++cursor) {
            ExactState state =
                oracle.quotient_materialize_locator(locators[cursor]);
            const std::vector<std::uint32_t> equivalents =
                oracle.quotient_equivalent_locators(state.stable_key);
            if (std::find(
                    equivalents.begin(), equivalents.end(),
                    locators[cursor]) == equivalents.end()) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::RefinementFailure,
                    "streamed quotient lost its canonical locator authority");
            }
            if (!state.terminal) {
                auto reuse_identity_task =
                    oracle
                        .quotient_selected_compact_row_identity_cooperatively(
                            state);
                while (!reuse_identity_task.resume()) {
                    ++progress.work_items;
                    co_await solve_detail::CooperativeCheckpoint{
                        reuse_identity_task.retained_bytes()};
                }
                const std::optional<StableKey> reuse_identity =
                    reuse_identity_task.take_result();
                AlternativeOperatorIds descriptors =
                    oracle.quotient_alternative_operator_ids(state);
                const auto shared_descriptors = std::find_if(
                    alternative_descriptor_payloads.begin(),
                    alternative_descriptor_payloads.end(),
                    [&](const auto& payload) {
                        return *payload == descriptors;
                    });
                if (shared_descriptors !=
                    alternative_descriptor_payloads.end()) {
                    alternatives_by_ordinal[cursor] =
                        *shared_descriptors;
                    alternative_payload_by_ordinal[cursor] =
                        static_cast<std::uint32_t>(
                            shared_descriptors -
                            alternative_descriptor_payloads.begin());
                } else {
                    auto payload =
                        std::make_shared<const AlternativeOperatorIds>(
                            std::move(descriptors));
                    alternative_descriptor_payloads.push_back(payload);
                    alternatives_by_ordinal[cursor] =
                        std::move(payload);
                    alternative_payload_by_ordinal[cursor] =
                        static_cast<std::uint32_t>(
                            alternative_descriptor_payloads.size() - 1);
                }
                const AlternativeOperatorIds& state_alternatives =
                    *alternatives_by_ordinal[cursor];
                const auto reused =
                    reuse_identity.has_value()
                        ? raw_row_by_reuse_identity.find(*reuse_identity)
                        : raw_row_by_reuse_identity.end();
                if (reused != raw_row_by_reuse_identity.end()) {
                    rows_by_ordinal[cursor].push_back(reused->second);
                    oracle.note_exact_kernel_reuse();
                } else {
                    auto selected_task =
                        oracle.quotient_selected_compact_row_cooperatively(
                            state);
                    while (!selected_task.resume()) {
                        ++progress.work_items;
                        co_await solve_detail::CooperativeCheckpoint{
                            selected_task.retained_bytes()};
                    }
                    QuotientOracleCompactRow selected =
                        selected_task.take_result();
                    std::uint64_t discovery_live =
                        exact_state_bytes(state);
                    saturating_add(
                        discovery_live,
                        sizeof(QuotientOracleCompactRow));
                    saturating_add(
                        discovery_live,
                        state_alternatives.capacity() *
                            sizeof(std::uint32_t));
                    saturating_add(
                        discovery_live,
                        selected_action_bytes(selected.selected));
                    saturating_add(
                        discovery_live,
                        exact_choice_recipe_bytes(
                            selected.choice_recipe));
                    saturating_add(
                        discovery_live,
                        selected.transitions.capacity() *
                            sizeof(QuotientOracleCompactTransition));
                    telemetry.current_live_slices = 1;
                    telemetry.peak_live_slices = 1;
                    telemetry.current_live_slice_bytes = discovery_live;
                    telemetry.peak_live_slice_bytes = std::max(
                        telemetry.peak_live_slice_bytes, discovery_live);
                    for (QuotientOracleCompactTransition& transition :
                         selected.transitions) {
                        transition.strict_state =
                            oracle.quotient_canonical_locator(
                                transition.strict_state);
                    }
                    selected = canonical_raw_row(std::move(selected));
                    for (const QuotientOracleCompactTransition& transition :
                         selected.transitions) {
                        intern_locator(transition.strict_state);
                    }
                    StableKey identity = raw_identity(selected);
                    auto [found, inserted] = raw_row_by_identity.emplace(
                        std::move(identity),
                        static_cast<std::uint32_t>(raw_rows.size()));
                    if (inserted) {
                        raw_rows.push_back({
                            std::move(selected.selected),
                            std::move(selected.choice_recipe),
                            selected.action_cost,
                            std::move(selected.transitions)});
                    }
                    rows_by_ordinal[cursor].push_back(found->second);
                    if (reuse_identity.has_value()) {
                        const auto [unused, reuse_inserted] =
                            raw_row_by_reuse_identity.emplace(
                                *reuse_identity, found->second);
                        (void)unused;
                        if (!reuse_inserted) {
                            throw AdapterFailure(
                                PolicyExactLiftStatus::RefinementFailure,
                                "streamed quotient reuse identity changed "
                                "during discovery");
                        }
                    }
                }
                saturating_add(
                    telemetry.alternative_rows_avoided,
                    state_alternatives.size());
            }
            const std::uint64_t live_bytes = exact_state_bytes(state);
            telemetry.current_live_slices = 1;
            telemetry.peak_live_slices = 1;
            telemetry.current_live_slice_bytes = live_bytes;
            telemetry.peak_live_slice_bytes = std::max(
                telemetry.peak_live_slice_bytes, live_bytes);
            oracle.quotient_release_carrier(state.stable_key);
            telemetry.current_live_slice_bytes = 0;
            telemetry.current_live_slices = 0;

            std::uint64_t discovery_external_bytes =
                persistent_selected_closure_bytes(closure);
            saturating_add(
                discovery_external_bytes,
                persistent_published_rows_bytes(
                    session.published_rows));
            /* During growth, charge the complete live closure through the
             * Bellman ledger. The post-discovery split below moves locator
             * storage to Carrier without ever counting the closure twice. */
            bellman.set_external_row_kernel_bytes(
                discovery_external_bytes);
            std::uint64_t combined = oracle.estimated_owned_bytes();
            saturating_add(combined, ledger.snapshot().total_bytes);
            if (combined > limits.max_estimated_memory_bytes) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::ResourceCap,
                    "streamed quotient discovery reached memory cap",
                    "max_estimated_memory_bytes");
            }
            ++progress.work_items;
            progress.strict_states = static_cast<std::uint32_t>(
                locators.size());
            progress.strict_kernels = telemetry.strict_kernels_built;
            progress.strict_transitions =
                telemetry.strict_transitions_built;
            co_await solve_detail::CooperativeCheckpoint{combined};
        }
        oracle.quotient_release_transient_kernel_caches();

        const std::uint64_t locator_bytes =
            locators.capacity() * sizeof(std::uint32_t) +
            ordinal_by_strict_state.capacity() * sizeof(std::uint32_t);
        const std::uint64_t selected_closure_bytes =
            persistent_selected_closure_bytes(closure);
        const std::uint64_t raw_row_bytes =
            selected_closure_bytes >= locator_bytes
                ? selected_closure_bytes - locator_bytes
                : 0;
        for (; closure.installed_recipe_count < raw_rows.size();
             ++closure.installed_recipe_count) {
            const RawRowPayload& row =
                raw_rows[closure.installed_recipe_count];
            oracle.quotient_install_streamed_recipe(
                row.selected, row.choice_recipe);
        }
        telemetry.carrier_bytes = locator_bytes;
        std::uint64_t external_row_bytes = raw_row_bytes;
        saturating_add(
            external_row_bytes,
            persistent_published_rows_bytes(
                session.published_rows));
        bellman.set_external_row_kernel_bytes(external_row_bytes);
        quotient::ScopedProofMemoryCharge retained_locators(
            ledger, quotient::ProofMemoryCategory::Carrier,
            locator_bytes);

        /* These keys name the live session authorities, not a population
         * snapshot. Canonical coverage ranges/counts below commit each exact
         * prefix; keeping the authority stable lets an untouched cell retain
         * its semantic identity when another range grows. */
        const StableKey strict_identity{
            0x7063717374726b32ull,
            root_locator};
        const StableKey replay_identity{
            0x7063717265706c32ull,
            root_locator};
        const StableKey enumeration_identity{
            0x706371656e756d32ull,
            root_locator};
        const StableKey range_identity{0x70637172616e6732ull};

        std::vector<std::optional<std::uint32_t>>
            arc_source_by_ordinal(locators.size());
        std::map<std::vector<std::uint32_t>, std::uint32_t>
            first_ordinal_by_absolute_row;
        for (std::uint32_t ordinal = 0;
             ordinal < rows_by_ordinal.size(); ++ordinal) {
            const std::vector<std::uint32_t>& row =
                rows_by_ordinal[ordinal];
            if (row.empty()) continue;
            auto [authority, inserted] =
                first_ordinal_by_absolute_row.emplace(row, ordinal);
            if (!inserted) {
                arc_source_by_ordinal[ordinal] = authority->second;
            }
        }
        std::map<std::vector<std::uint32_t>, std::uint32_t>{}.swap(
            first_ordinal_by_absolute_row);

        const auto replay_node =
                [&](const bool include_observations,
                    const std::uint32_t ordinal,
                    const ExactState& state) {
            refinement::ClosedPartitionNode node;
            node.stable_key = {
                0x7063716e6f646532ull,
                locators.at(ordinal)};
            node.terminal = state.terminal;
            node.arc_source = arc_source_by_ordinal.at(ordinal);
            ObservationRequirement requirement;
            StableKey immediate{
                state.terminal ? 0x7063717465726d31ull
                               : 0x706371726f777332ull};
            if (!state.terminal) {
                immediate.push_back(rows_by_ordinal.at(ordinal).size());
                const double row_weight = 1.0 / static_cast<double>(
                    rows_by_ordinal.at(ordinal).size());
                for (const std::uint32_t payload_id :
                     rows_by_ordinal.at(ordinal)) {
                    const RawRowPayload& row = raw_rows.at(payload_id);
                    if (include_observations) {
                        requirement = merge_observation_requirements(
                            std::move(requirement),
                            row.selected.routing_observes);
                    }
                    immediate.push_back(row.selected.semantic_key.size());
                    immediate.insert(
                        immediate.end(),
                        row.selected.semantic_key.begin(),
                        row.selected.semantic_key.end());
                    immediate.push_back(std::bit_cast<std::uint64_t>(
                        row.action_cost));
                    if (!node.arc_source.has_value()) {
                        for (const QuotientOracleCompactTransition& transition :
                             row.transitions) {
                            const std::uint32_t successor =
                                ordinal_by_strict_state.at(
                                    transition.strict_state);
                            if (successor == kNoId) {
                                throw AdapterFailure(
                                    PolicyExactLiftStatus::RefinementFailure,
                                    "streamed quotient row lost a successor locator");
                            }
                            node.arcs.push_back({
                                {}, successor,
                                transition.probability * row_weight});
                        }
                    }
                }
                if (include_observations) {
                    for (const std::uint32_t operator_index :
                         *alternatives_by_ordinal.at(ordinal)) {
                        requirement = merge_observation_requirements(
                            std::move(requirement),
                            oracle.quotient_alternative_descriptor(
                                operator_index).routing_observes);
                    }
                }
                immediate.push_back(
                    alternatives_by_ordinal.at(ordinal)->size());
                /* Collision-free interned authority: payload ids are assigned
                 * only after full descriptor equality succeeds. The full
                 * admitted operator ids remain retained once in
                 * alternative_descriptor_payloads; full action identities
                 * are deterministic operator metadata and are materialized
                 * only for proof obligations. Per-cell partition keys need
                 * only the deterministic payload id. */
                immediate.push_back(
                    alternative_payload_by_ordinal.at(ordinal));
            }
            node.immediate_key = std::move(immediate);
            node.observation_key = canonical_observation_identity(
                state.coarse_state_key, requirement, state.features);
            telemetry.peak_live_slice_bytes = std::max(
                telemetry.peak_live_slice_bytes,
                exact_state_bytes(state));
            return node;
        };

        /* Replaying an exact carrier through the strict calculator dominated
         * the old partition pass because every refinement round rebuilt the
         * same immutable node. Retain both observation projections once,
         * under the existing proof-memory ceiling, then let the unchanged
         * replay partition authority consume deterministic copies. */
        progress.phase = PolicyExactLiftPhase::PartitionRefinement;
        std::vector<refinement::ClosedPartitionNode>
            cached_observation_nodes;
        std::vector<refinement::ClosedPartitionNode> cached_exact_nodes;
        std::vector<ExactState> cached_partition_states;
        cached_observation_nodes.reserve(locators.size());
        cached_exact_nodes.reserve(locators.size());
        cached_partition_states.reserve(locators.size());
        std::uint64_t cached_partition_node_bytes =
            cached_observation_nodes.capacity() *
                sizeof(refinement::ClosedPartitionNode) +
            cached_exact_nodes.capacity() *
                sizeof(refinement::ClosedPartitionNode);
        std::uint64_t cached_partition_state_bytes =
            cached_partition_states.capacity() * sizeof(ExactState);
        const auto account_cached_node =
                [&](const refinement::ClosedPartitionNode& node) {
            saturating_add(
                cached_partition_node_bytes,
                node.stable_key.capacity() * sizeof(std::uint64_t));
            saturating_add(
                cached_partition_node_bytes,
                node.observation_key.capacity() * sizeof(std::uint64_t));
            saturating_add(
                cached_partition_node_bytes,
                node.immediate_key.capacity() * sizeof(std::uint64_t));
            saturating_add(
                cached_partition_node_bytes,
                node.arcs.capacity() *
                    sizeof(refinement::ClosedPartitionArc));
            for (const refinement::ClosedPartitionArc& arc : node.arcs) {
                saturating_add(
                    cached_partition_node_bytes,
                    arc.label.capacity() * sizeof(std::uint64_t));
            }
        };
        for (std::uint32_t ordinal = 0;
             ordinal < locators.size(); ++ordinal) {
            cached_partition_states.push_back(
                oracle.quotient_materialize_locator(locators.at(ordinal)));
            saturating_add(
                cached_partition_state_bytes,
                exact_state_bytes(cached_partition_states.back()));
            cached_observation_nodes.push_back(
                replay_node(
                    false, ordinal,
                    cached_partition_states.back()));
            account_cached_node(cached_observation_nodes.back());
            cached_exact_nodes.push_back(replay_node(
                true, ordinal, cached_partition_states.back()));
            account_cached_node(cached_exact_nodes.back());
            oracle.quotient_release_carrier(
                cached_partition_states.back().stable_key);
            std::uint64_t live = oracle.estimated_owned_bytes();
            saturating_add(live, ledger.snapshot().total_bytes);
            saturating_add(live, cached_partition_node_bytes);
            saturating_add(live, cached_partition_state_bytes);
            if (live > limits.max_estimated_memory_bytes) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::ResourceCap,
                    "cached streamed quotient partition reached memory cap",
                    "max_estimated_memory_bytes");
            }
            ++progress.work_items;
            co_await solve_detail::CooperativeCheckpoint{live};
        }

        telemetry.carrier_discovery_ns = elapsed_certification_ns();
        std::uint64_t retained_for_partition = oracle.estimated_owned_bytes();
        saturating_add(
            retained_for_partition, ledger.snapshot().total_bytes);
        saturating_add(
            retained_for_partition, cached_partition_node_bytes);
        saturating_add(
            retained_for_partition, cached_partition_state_bytes);
        refinement::ClosedPartitionLimits partition_limits{
            limits.max_refinement_classes,
            limits.max_refinement_rounds,
            retained_for_partition,
            limits.max_estimated_memory_bytes,
            limits.probability_sum_tolerance};
        const auto observation_partition_started =
            std::chrono::steady_clock::now();
        refinement::ClosedPartitionResult observation_coarse =
            refinement::refine_closed_probabilistic_partition_replay(
                static_cast<std::uint32_t>(locators.size()),
                [&](const std::uint32_t ordinal) {
                    return cached_observation_nodes.at(ordinal);
                },
                {}, false, partition_limits);
        telemetry.partition_refinement_ns +=
            static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() -
                    observation_partition_started)
                    .count());
        progress.partition_rounds += observation_coarse.rounds;
        progress.partition_classes =
            observation_coarse.final_class_count;
        ++progress.work_items;
        co_await solve_detail::CooperativeCheckpoint{
            retained_for_partition};
        if (observation_coarse.status !=
                refinement::ClosedPartitionStatus::Complete ||
            !observation_coarse.lumpable) {
            throw AdapterFailure(
                observation_coarse.status ==
                        refinement::ClosedPartitionStatus::ResourceCap
                    ? PolicyExactLiftStatus::ResourceCap
                    : PolicyExactLiftStatus::RefinementFailure,
                observation_coarse.failure_reason,
                observation_coarse.resource_cap);
        }
        if (!telemetry.work_to_first_partition.has_value()) {
            telemetry.work_to_first_partition =
                telemetry.strict_reforge_work;
            telemetry.wall_ns_to_first_partition =
                elapsed_certification_ns();
        }
        /* The first pass supplies only the monotone split seed. Its complete
         * projected class payload is not a publication dependency and would
         * otherwise overlap the final exact partition at peak memory. */
        observation_coarse.initial_class_by_node.clear();
        observation_coarse.initial_class_by_node.shrink_to_fit();
        observation_coarse.classes.clear();
        observation_coarse.classes.shrink_to_fit();
        const std::uint64_t coarse_partition_owned =
            observation_coarse.class_by_node.capacity() *
            sizeof(std::uint32_t);
        quotient::ScopedProofMemoryCharge retained_coarse_partition(
            ledger, quotient::ProofMemoryCategory::Partition,
            coarse_partition_owned);

        partition_limits.retained_estimated_memory_bytes =
            oracle.estimated_owned_bytes();
        saturating_add(
            partition_limits.retained_estimated_memory_bytes,
            ledger.snapshot().total_bytes);
        saturating_add(
            partition_limits.retained_estimated_memory_bytes,
            cached_partition_node_bytes);
        saturating_add(
            partition_limits.retained_estimated_memory_bytes,
            cached_partition_state_bytes);
        const std::uint64_t final_partition_retained =
            partition_limits.retained_estimated_memory_bytes;
        const auto exact_partition_started =
            std::chrono::steady_clock::now();
        refinement::ClosedPartitionResult partition =
            refinement::refine_closed_probabilistic_partition_replay(
                static_cast<std::uint32_t>(locators.size()),
                [&](const std::uint32_t ordinal) {
                    return cached_exact_nodes.at(ordinal);
                },
                observation_coarse.class_by_node,
                false, partition_limits);
        telemetry.partition_refinement_ns +=
            static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() -
                    exact_partition_started)
                    .count());
        progress.partition_rounds += partition.rounds;
        progress.partition_classes = partition.final_class_count;
        ++progress.work_items;
        co_await solve_detail::CooperativeCheckpoint{
            partition_limits.retained_estimated_memory_bytes};
        if (partition.status != refinement::ClosedPartitionStatus::Complete ||
            !partition.lumpable) {
            throw AdapterFailure(
                partition.status == refinement::ClosedPartitionStatus::ResourceCap
                    ? PolicyExactLiftStatus::ResourceCap
                    : PolicyExactLiftStatus::RefinementFailure,
                partition.failure_reason,
                partition.resource_cap);
        }
        const std::uint64_t final_partition_owned =
            partition.estimated_memory_bytes > final_partition_retained
                ? partition.estimated_memory_bytes -
                      final_partition_retained
                : 0;
        quotient::ScopedProofMemoryCharge retained_partition(
            ledger, quotient::ProofMemoryCategory::Partition,
            final_partition_owned);
        std::vector<refinement::ClosedPartitionNode>{}.swap(
            cached_observation_nodes);
        std::vector<refinement::ClosedPartitionNode>{}.swap(
            cached_exact_nodes);
        cached_partition_node_bytes = 0;

        const auto class_coverage =
                [&](const std::vector<std::uint32_t>& class_by_node,
                    const std::uint32_t class_id) {
            quotient::CoverageDescriptor coverage;
            coverage.strict_kernel_identity = strict_identity;
            coverage.replay_authority_identity = replay_identity;
            coverage.normalized_enumeration_identity = enumeration_identity;
            for (std::uint64_t begin = 0; begin < locators.size();) {
                while (begin < locators.size() &&
                       class_by_node[begin] != class_id) {
                    ++begin;
                }
                if (begin == locators.size()) break;
                std::uint64_t end = begin + 1;
                while (end < locators.size() &&
                       class_by_node[end] == class_id) {
                    ++end;
                }
                coverage.ranges.push_back({
                    range_identity, begin, end - begin,
                    static_cast<double>(end - begin)});
                coverage.exact_source_count += end - begin;
                coverage.exact_total_probability +=
                    static_cast<double>(end - begin);
                begin = end;
            }
            return quotient::canonical_coverage_descriptor(
                std::move(coverage));
        };
        const auto representative_for =
                [&](const std::vector<std::uint32_t>& class_by_node,
                    const std::uint32_t class_id) {
            const auto found = std::find(
                class_by_node.begin(), class_by_node.end(), class_id);
            if (found == class_by_node.end()) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::RefinementFailure,
                    "streamed quotient class has no representative");
            }
            return static_cast<std::uint32_t>(
                found - class_by_node.begin());
        };

        const auto build_cells =
                [&](const refinement::ClosedPartitionResult& shared,
                    const bool include_observations,
                    const std::vector<std::uint32_t>& cell_id_by_class,
                    const std::vector<std::uint64_t>& generation_by_class)
                    -> solve_detail::CooperativeTask<
                        std::vector<quotient::QuotientCell>> {
            std::vector<quotient::QuotientCell> cells(shared.classes.size());
            std::uint32_t built = 0;
            for (const refinement::ClosedPartitionClass& cls :
                 shared.classes) {
                const std::uint32_t representative = representative_for(
                    shared.class_by_node, cls.class_id);
                const ExactState& state =
                    cached_partition_states.at(representative);
                quotient::QuotientCell cell;
                cell.cell_id = cell_id_by_class.at(cls.class_id);
                cell.generation = generation_by_class.at(cls.class_id);
                cell.coarse_state = state.coarse_state;
                cell.coarse_parent = state.coarse_state_key;
                cell.terminal = state.terminal;
                if (include_observations && !state.terminal) {
                    for (const std::uint32_t payload_id :
                         rows_by_ordinal[representative]) {
                        cell.observation_requirement =
                            merge_observation_requirements(
                                std::move(cell.observation_requirement),
                                raw_rows[payload_id]
                                    .selected.routing_observes);
                    }
                    for (const std::uint32_t operator_index :
                         *alternatives_by_ordinal[representative]) {
                        cell.observation_requirement =
                            merge_observation_requirements(
                                std::move(cell.observation_requirement),
                                oracle.quotient_alternative_descriptor(
                                    operator_index).routing_observes);
                    }
                }
                cell.observed_features = observe_features(
                    state.features, cell.observation_requirement);
                cell.immediate_identity = cls.immediate_key;
                cell.coverage = class_coverage(
                    shared.class_by_node, cls.class_id);
                cell.semantic_identity =
                    quotient::canonical_quotient_cell_identity(cell);
                cells[cls.class_id] = std::move(cell);
                ++built;
                if ((built & 63u) == 0u) {
                    co_await solve_detail::CooperativeCheckpoint{
                        cells.capacity() *
                        sizeof(quotient::QuotientCell)};
                }
            }
            std::uint32_t projected = 0;
            for (const refinement::ClosedPartitionClass& cls :
                 shared.classes) {
                quotient::QuotientCell& cell = cells[cls.class_id];
                for (const refinement::ClosedPartitionProjectedArc& arc :
                     cls.arcs) {
                    cell.arcs.push_back({
                        arc.label,
                        arc.successor_class.has_value()
                            ? std::optional<std::uint32_t>{
                                  cell_id_by_class.at(*arc.successor_class)}
                            : std::nullopt,
                        std::nullopt,
                        arc.probability});
                }
                ++projected;
                if ((projected & 63u) == 0u) {
                    co_await solve_detail::CooperativeCheckpoint{
                        cells.capacity() *
                        sizeof(quotient::QuotientCell)};
                }
            }
            co_return cells;
        };

        std::vector<std::uint32_t> batch_cell_id(
            partition.final_class_count);
        std::iota(
            batch_cell_id.begin(), batch_cell_id.end(), 0u);
        std::vector<std::uint64_t> batch_generation(
            partition.final_class_count, 1);
        auto build_cells_work = build_cells(
            partition, true, batch_cell_id, batch_generation);
        while (!build_cells_work.resume()) {
            ++progress.work_items;
            co_await solve_detail::CooperativeCheckpoint{
                build_cells_work.retained_bytes()};
        }
        std::vector<quotient::QuotientCell> batch_cells =
            build_cells_work.take_result();
        std::optional<quotient::QuotientPartitionState>
            previous_partition;
        if (session.partition_initialized) {
            previous_partition.emplace(
                std::move(session.partition_state));
        }
        quotient::QuotientPartitionUpdate partition_update =
            quotient::stabilize_quotient_partition_state(
                std::move(batch_cells),
                {{partition.class_by_node.at(0), 1.0}},
                previous_partition.has_value()
                    ? &*previous_partition
                    : nullptr);
        saturating_add(telemetry.strict_partition_updates, 1);
        saturating_add(
            telemetry.strict_cells_retained,
            partition_update.telemetry.retained_cells);
        saturating_add(
            telemetry.strict_cells_created,
            partition_update.telemetry.new_cells);
        saturating_add(
            telemetry.strict_cells_superseded,
            partition_update.telemetry.superseded_cells);
        if (partition_update.status !=
            quotient::QuotientPartitionStatus::Complete) {
            throw AdapterFailure(
                PolicyExactLiftStatus::RefinementFailure,
                partition_update.failure_reason);
        }
        if (previous_partition.has_value()) {
            for (const std::uint32_t old_id :
                 partition_update.split_previous_cells) {
                const quotient::QuotientCell* old =
                    previous_partition->find_cell(old_id);
                if (old == nullptr) {
                    throw AdapterFailure(
                        PolicyExactLiftStatus::RefinementFailure,
                        "split quotient cell lost its prior generation");
                }
                bellman.invalidate_source_split(
                    old_id, old->generation + 1);
                bellman.invalidate_target_split(
                    old_id, old->generation + 1);
            }
            for (const std::uint32_t old_id :
                 partition_update.superseded_previous_cells) {
                const quotient::QuotientCell* replacement =
                    partition_update.state.find_cell(old_id);
                if (replacement == nullptr) {
                    throw AdapterFailure(
                        PolicyExactLiftStatus::RefinementFailure,
                        "superseded quotient cell lost its replacement");
                }
                bellman.supersede_cell({
                    replacement->cell_id,
                    replacement->generation,
                    quotient::SharedStableKey{
                        replacement->semantic_identity},
                    replacement->terminal});
            }
        }
        std::vector<std::uint32_t> final_cell_id =
            std::move(partition_update.cell_id_by_class);
        session.partition_state = std::move(partition_update.state);
        session.partition_initialized = true;
        std::vector<quotient::QuotientCell>& final_cells =
            session.partition_state.cells;
        std::map<std::uint32_t, std::uint32_t> final_class_by_cell;
        for (std::uint32_t cls = 0; cls < final_cell_id.size(); ++cls) {
            final_class_by_cell.emplace(final_cell_id[cls], cls);
        }
        const auto cell_for_class = [&](const std::uint32_t cls)
                -> const quotient::QuotientCell& {
            const quotient::QuotientCell* cell =
                session.partition_state.find_cell(
                    final_cell_id.at(cls));
            if (cell == nullptr) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::RefinementFailure,
                    "stable quotient class lost its cell");
            }
            return *cell;
        };
        for (quotient::QuotientCell& cell : final_cells) {
            std::vector<quotient::QuotientCellArc>{}.swap(cell.arcs);
        }
        retained_partition.reset();
        partition.initial_class_by_node.clear();
        partition.initial_class_by_node.shrink_to_fit();
        partition.classes.clear();
        partition.classes.shrink_to_fit();
        const std::uint64_t final_partition_mapping_bytes =
            partition.class_by_node.capacity() * sizeof(std::uint32_t);
        quotient::ScopedProofMemoryCharge retained_partition_mapping(
            ledger, quotient::ProofMemoryCategory::Partition,
            final_partition_mapping_bytes);

        const auto install_cells =
                [&](std::vector<quotient::QuotientCell>& cells) {
            std::vector<quotient::QuotientBellmanCellInput> inputs;
            inputs.reserve(cells.size());
            for (quotient::QuotientCell& cell : cells) {
                inputs.push_back({
                    cell.cell_id, cell.generation,
                    quotient::SharedStableKey{cell.semantic_identity},
                    cell.terminal});
            }
            bellman.install_cells(std::move(inputs));
        };
        using PublishedRow = PersistentPublishedRow;
        const auto publish_rows =
                [&](const refinement::ClosedPartitionResult& shared,
                    const std::vector<quotient::QuotientCell>& cells,
                    const std::vector<std::uint32_t>& cell_id_by_class)
                    -> solve_detail::CooperativeTask<
                        std::vector<PublishedRow>> {
            using ProjectedPayload =
                std::vector<std::pair<std::uint32_t, double>>;
            std::vector<ProjectedPayload> projected_payloads(
                raw_rows.size());
            std::vector<bool> projected_payload_ready(
                raw_rows.size(), false);
            quotient::ScopedProofMemoryCharge projected_cache_index_charge(
                ledger, quotient::ProofMemoryCategory::Scratch,
                projected_payloads.capacity() * sizeof(ProjectedPayload) +
                    projected_payload_ready.capacity() * sizeof(bool));
            std::vector<quotient::ScopedProofMemoryCharge>
                projected_payload_charges;
            projected_payload_charges.reserve(raw_rows.size());
            std::vector<PublishedRow> published;
            for (const quotient::QuotientCell& cell : cells) {
                if (cell.terminal) continue;
                const std::uint32_t class_id =
                    final_class_by_cell.at(cell.cell_id);
                const std::uint32_t representative = representative_for(
                    shared.class_by_node, class_id);
                const ExactState& source =
                    cached_partition_states.at(representative);
                for (const std::uint32_t payload_id :
                     rows_by_ordinal[representative]) {
                    const auto reusable = std::find_if(
                        session.published_rows.begin(),
                        session.published_rows.end(),
                        [&](const PublishedRow& candidate) {
                            if (candidate.source_cell_id != cell.cell_id ||
                                candidate.source_generation !=
                                    cell.generation ||
                                candidate.selected_payload_id != payload_id ||
                                !bellman.proof_store()->has_use_site(
                                    candidate.sparse_row)) {
                                return false;
                            }
                            const quotient::RowProofUseSite& use =
                                bellman.proof_store()->use_site(
                                    candidate.sparse_row);
                            return use.valid &&
                                   use.source_generation == cell.generation;
                        });
                    if (reusable != session.published_rows.end()) continue;
                    const RawRowPayload& row = raw_rows[payload_id];
                    if (!projected_payload_ready[payload_id]) {
                        std::map<std::uint32_t, solve_detail::WideFloat>
                            accumulated;
                        for (const QuotientOracleCompactTransition& transition :
                             row.transitions) {
                            const std::uint32_t ordinal =
                                ordinal_by_strict_state[
                                    transition.strict_state];
                            accumulated[cell_id_by_class[
                                shared.class_by_node[ordinal]]] +=
                                    solve_detail::WideFloat{
                                        transition.probability};
                        }
                        ProjectedPayload& cached =
                            projected_payloads[payload_id];
                        cached.reserve(accumulated.size());
                        for (const auto& [target, probability] :
                             accumulated) {
                            cached.emplace_back(
                                target, probability.value());
                        }
                        projected_payload_charges.emplace_back(
                            ledger,
                            quotient::ProofMemoryCategory::Scratch,
                            cached.capacity() *
                                sizeof(ProjectedPayload::value_type));
                        projected_payload_ready[payload_id] = true;
                    }
                    const ProjectedPayload& projected =
                        projected_payloads[payload_id];
                    quotient::QuotientBellmanRowInput input;
                    input.source_cell_id = cell.cell_id;
                    input.operator_index = row.selected.action_id;
                    input.cost = row.action_cost;
                    input.certified = true;
                    std::vector<quotient::ProofProjectedArc> arcs;
                    solve_detail::WideFloat total{0.0};
                    for (const auto& [target, probability] : projected) {
                        input.transitions.push_back({
                            {}, target, probability});
                        arcs.push_back({
                            {}, bellman.shared_semantic_identity_for_cell(target),
                            probability});
                        total += solve_detail::WideFloat{probability};
                    }
                    quotient::CoverageDescriptor row_coverage =
                        cell.coverage;
                    for (quotient::CoverageRange& range :
                         row_coverage.ranges) {
                        range.total_probability =
                            static_cast<double>(range.count) /
                            static_cast<double>(
                                row_coverage.exact_source_count) *
                            total.value();
                    }
                    row_coverage.exact_total_probability = total.value();
                    input.proof_identity = oracle.quotient_proof_identity(
                        source, row.selected,
                        quotient::canonical_coverage_descriptor(
                            std::move(row_coverage)),
                        std::move(arcs), total.value());
                    const std::uint64_t sparse_row =
                        bellman.append_row(std::move(input));
                    published.push_back({
                        sparse_row, cell.cell_id, cell.generation,
                        payload_id, row.selected});
                    if ((published.size() & 255u) == 0u) {
                        co_await solve_detail::CooperativeCheckpoint{
                            published.capacity() * sizeof(PublishedRow)};
                    }
                }
                if ((class_id & 31u) == 31u) {
                    co_await solve_detail::CooperativeCheckpoint{
                        published.capacity() * sizeof(PublishedRow)};
                }
            }
            co_return published;
        };

        install_cells(final_cells);
        retained_coarse_partition.reset();
        auto publish_rows_work = publish_rows(
            partition, final_cells, final_cell_id);
        while (!publish_rows_work.resume()) {
            ++progress.work_items;
            co_await solve_detail::CooperativeCheckpoint{
                publish_rows_work.retained_bytes()};
        }
        std::vector<PublishedRow> newly_published_rows =
            publish_rows_work.take_result();
        session.published_rows.insert(
            session.published_rows.end(),
            std::make_move_iterator(newly_published_rows.begin()),
            std::make_move_iterator(newly_published_rows.end()));
        std::vector<PublishedRow>& published_rows =
            session.published_rows;
        std::vector<ExactState>{}.swap(cached_partition_states);
        cached_partition_state_bytes = 0;
        const auto refresh_external_row_kernel_bytes = [&] {
            std::uint64_t retained = raw_row_bytes;
            saturating_add(
                retained,
                persistent_published_rows_bytes(published_rows));
            bellman.set_external_row_kernel_bytes(retained);
        };
        refresh_external_row_kernel_bytes();

        const quotient::SharedStableKey price_identity{
            oracle.quotient_price_identity()};
        const quotient::SharedStableKey vocabulary_identity{
            oracle.quotient_vocabulary_identity()};
        double retained_lower_delta = kInfinity;
        double retained_lower_relative = kInfinity;
        const bool retained_global_lower_authority =
            solved.converged &&
            solved.policy_status == SolvePolicyStatus::Exact &&
            std::isfinite(solved.lower_bound) &&
            solved.lower_bound >= 0.0 &&
            reconciled(
                certificate.solver_cost, solved.lower_bound,
                retained_lower_delta, retained_lower_relative);
        std::vector<quotient::AccountedAlternativeAction>
            admitted_action_accounting;
        std::vector<quotient::AccountedAlternativeAction>
            completed_action_accounting;
        std::uint64_t current_alternative_admissions = 0;
        std::map<std::uint32_t, std::uint32_t> scheduler_class_by_cell;
        for (std::uint32_t cls = 0; cls < final_cell_id.size(); ++cls) {
            scheduler_class_by_cell.emplace(final_cell_id[cls], cls);
        }
        std::set<std::uint32_t> accounted_source_cells;
        std::set<std::uint32_t> selected_closure_scanned_cells;
        const auto account_source_cell =
                [&](const std::uint32_t source_cell_id) {
            const auto source_class = scheduler_class_by_cell.find(
                source_cell_id);
            if (source_class == scheduler_class_by_cell.end()) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::RefinementFailure,
                    "proof envelope reached an unknown quotient cell");
            }
            if (!accounted_source_cells.insert(source_cell_id).second) {
                return false;
            }
            const std::uint32_t cls = source_class->second;
            const quotient::QuotientCell& cell = cell_for_class(cls);
            if (cell.terminal) return true;
            const std::uint32_t representative = representative_for(
                partition.class_by_node, cls);
            const auto& alternative_operator_ids =
                *alternatives_by_ordinal.at(representative);
            /* The closed partition's collision-checked immediate key contains
             * the interned full alternative-descriptor payload id. Therefore
             * every carrier in this cell has this same admitted vocabulary;
             * rescanning the complete carrier range here merely repeated the
             * partition proof. */
            const auto selected = std::find_if(
                published_rows.begin(), published_rows.end(),
                [&](const PublishedRow& row) {
                    if (row.source_cell_id != cell.cell_id ||
                        row.source_generation != cell.generation ||
                        !row.selected_payload_id.has_value() ||
                        !bellman.proof_store()->has_use_site(
                            row.sparse_row)) {
                        return false;
                    }
                    const quotient::RowProofUseSite& use =
                        bellman.proof_store()->use_site(row.sparse_row);
                    return use.valid &&
                           use.source_generation == cell.generation;
                });
            if (selected == published_rows.end()) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::RefinementFailure,
                    "nonterminal quotient cell has no selected certified row");
            }
            const quotient::AlternativeActionIdentity selected_identity =
                quotient::canonical_alternative_action_identity({
                    selected->selected.action_id,
                    selected->selected.semantic_key,
                    canonical_selected_runtime_contract_identity(
                        selected->selected),
                    selected->selected.semantic_key});
            admitted_action_accounting.push_back({
                cell.cell_id,
                selected_identity,
                quotient::AlternativeActionAccountingKind::
                    CurrentSelectedCertified,
                std::nullopt,
                std::nullopt});
            completed_action_accounting.push_back({
                cell.cell_id,
                selected_identity,
                quotient::AlternativeActionAccountingKind::
                    CurrentSelectedCertified,
                selected->sparse_row,
                std::nullopt});
            if (retained_global_lower_authority) return true;
            const quotient::SharedObservationRequirement
                shared_observation_requirement{
                    cell.observation_requirement};
            for (const std::uint32_t operator_index :
                 alternative_operator_ids) {
                const QuotientAlternativeDescriptor& descriptor =
                    oracle.quotient_alternative_descriptor(operator_index);
                const quotient::SharedStableKey& shared_cell_identity =
                    bellman.shared_semantic_identity_for_cell(cell.cell_id);
                quotient::UnresolvedAlternativeObligationIdentity identity;
                identity.source_cell_id = cell.cell_id;
                identity.source_cell_identity = shared_cell_identity;
                identity.observation_requirement =
                    shared_observation_requirement;
                identity.action = descriptor.action;
                identity.price_identity = price_identity;
                identity.vocabulary_identity = vocabulary_identity;
                identity.requirement_generation = cell.generation;
                identity.source_generation = cell.generation;
                identity.target_generation = cell.generation;
                identity.partition_generation = cell.generation;
                identity.action_generation = 1;
                identity.admission_generation = 1;
                identity.price_generation = 1;
                identity.vocabulary_generation = 1;
                const double immediate_cost_lower =
                    oracle.quotient_operator_immediate_cost_lower(
                        operator_index);
                identity.optimistic_lower =
                    quotient::certify_uniform_carrier_wide_lower_q(
                        shared_cell_identity, cell.coverage,
                        {0x706371707269636cull,
                         operator_index,
                         std::bit_cast<std::uint64_t>(
                             immediate_cost_lower),
                         1},
                        immediate_cost_lower);
                identity.scheduling_priority = 0.0;
                identity.resumable_work_identity =
                    descriptor.resumable_work_identity;
                const auto [obligation_id, obligation_reused] =
                    bellman.proof_store()
                        ->intern_alternative_obligation(
                            std::move(identity));
                if (!obligation_reused) {
                    bellman.proof_store()
                        ->transition_alternative_obligation(
                            obligation_id,
                            quotient::AlternativeObligationStatus::
                                LowerOnly);
                    saturating_add(
                        telemetry.alternative_obligations_created, 1);
                }
                admitted_action_accounting.push_back({
                    cell.cell_id,
                    descriptor.action,
                    quotient::AlternativeActionAccountingKind::
                        UnresolvedObligation,
                    std::nullopt,
                    std::nullopt});
                const quotient::UnresolvedAlternativeObligation&
                    obligation = bellman.proof_store()
                        ->alternative_obligation(obligation_id);
                if (obligation.status ==
                        quotient::AlternativeObligationStatus::Certified &&
                    obligation.certified_row_id.has_value()) {
                    completed_action_accounting.push_back({
                        cell.cell_id,
                        descriptor.action,
                        quotient::AlternativeActionAccountingKind::
                            OtherCertified,
                        obligation.certified_row_id,
                        std::nullopt});
                } else {
                    completed_action_accounting.push_back({
                        cell.cell_id,
                        descriptor.action,
                        quotient::AlternativeActionAccountingKind::
                            UnresolvedObligation,
                        std::nullopt,
                        obligation_id});
                }
                ++current_alternative_admissions;
            }
            return true;
        };
        ++progress.work_items;
        co_await solve_detail::CooperativeCheckpoint{
            ledger.snapshot().total_bytes};

        const std::uint32_t root_class = partition.class_by_node.at(0);
        const std::uint32_t root_cell = final_cell_id.at(root_class);
        quotient::QuotientBellmanResult solved_quotient =
            retained_global_lower_authority
                ? bellman.project_unique_certified_policy({root_cell})
                : bellman.solve(
                      {root_cell}, limits.max_refinement_rounds,
                      std::max<std::uint32_t>(1, options.max_sweeps));
        ++progress.work_items;
        co_await solve_detail::CooperativeCheckpoint{
            ledger.snapshot().total_bytes};
        const auto throw_failed_quotient = [&] (
                const quotient::QuotientBellmanResult& failed) {
            std::string detail =
                std::string{"selected-first quotient solve failed ("} +
                quotient::quotient_bellman_status_name(
                    failed.status) +
                "): " + failed.failure_reason;
            if (!failed.failure_path_cell_ids.empty()) {
                detail += "; named_dead_path=";
                for (std::size_t i = 0;
                     i < failed.failure_path_cell_ids.size(); ++i) {
                    if (i != 0) detail += ">";
                    detail += std::to_string(
                        failed.failure_path_cell_ids[i]);
                    detail += "[n=";
                    const auto path_class = final_class_by_cell.find(
                        failed.failure_path_cell_ids[i]);
                    detail += path_class == final_class_by_cell.end()
                        ? std::string{"unknown"}
                        : std::to_string(
                              cell_for_class(path_class->second)
                                  .coverage.exact_source_count);
                    detail += ",";
                    const std::uint32_t operator_index =
                        failed.failure_path_operator_indices.at(i);
                    detail += operator_index == kNoId
                        ? std::string{"no-certified-row"}
                        : oracle.quotient_operator_id(operator_index);
                    detail += "]";
                }
            }
            throw AdapterFailure(
                failed.status ==
                        quotient::QuotientBellmanStatus::ResourceCap
                    ? PolicyExactLiftStatus::ResourceCap
                    : PolicyExactLiftStatus::RefinementFailure,
                detail,
                failed.status ==
                        quotient::QuotientBellmanStatus::ResourceCap
                    ? "max_estimated_memory_bytes"
                    : std::string{});
        };
        const auto account_selected_closure =
                [&](std::vector<std::uint32_t> seeds,
                    const quotient::QuotientBellmanResult& upper)
                    -> solve_detail::CooperativeTask<bool> {
            bool grew = false;
            std::set<std::uint32_t> pending_sources(
                seeds.begin(), seeds.end());
            std::set<std::uint32_t> visited;
            const SolveTransitionCache& graph = bellman.transition_cache();
            while (!pending_sources.empty()) {
                const std::uint32_t cell_id = *pending_sources.begin();
                pending_sources.erase(pending_sources.begin());
                if (!visited.insert(cell_id).second) continue;
                const bool source_grew = account_source_cell(cell_id);
                grew = source_grew || grew;
                const std::optional<std::uint32_t> state =
                    bellman.state_index_for_cell(cell_id);
                if (!state.has_value() ||
                    *state >= upper.selected_rows_by_state.size()) {
                    throw AdapterFailure(
                        PolicyExactLiftStatus::RefinementFailure,
                        "proof envelope lost its selected quotient state");
                }
                const std::uint64_t row =
                    upper.selected_rows_by_state[*state];
                if (!selected_closure_scanned_cells.insert(cell_id).second) {
                    continue;
                }
                if (row == std::numeric_limits<std::uint64_t>::max()) {
                    if (source_grew) {
                        co_await solve_detail::CooperativeCheckpoint{
                            oracle.estimated_owned_bytes()};
                    }
                    continue;
                }
                /* Later selected-row changes can only select a row that was
                 * already certified from an accounted source. Its targets
                 * entered this proof envelope when that row was certified, so
                 * the original selected closure is traversed exactly once. */
                const SparseRow& sparse = graph.rows.at(row);
                for (std::uint32_t i = 0;
                     i < sparse.transition_count; ++i) {
                    const std::uint32_t target_state =
                        graph.successors.at(sparse.transition_offset + i);
                    const std::optional<std::uint32_t> target_cell =
                        bellman.cell_id_for_state(target_state);
                    if (!target_cell.has_value()) {
                        throw AdapterFailure(
                            PolicyExactLiftStatus::RefinementFailure,
                            "selected quotient row lost its target cell");
                    }
                    if (!visited.contains(*target_cell)) {
                        pending_sources.insert(*target_cell);
                    }
                }
                std::uint64_t retained = oracle.estimated_owned_bytes();
                saturating_add(retained, ledger.snapshot().total_bytes);
                saturating_add(
                    retained,
                    admitted_action_accounting.capacity() *
                        sizeof(quotient::AccountedAlternativeAction));
                saturating_add(
                    retained,
                    completed_action_accounting.capacity() *
                        sizeof(quotient::AccountedAlternativeAction));
                co_await solve_detail::CooperativeCheckpoint{retained};
            }
            co_return grew;
        };
        quotient::QuotientBellmanResult selected_projection;
        if (solved_quotient.status ==
                quotient::QuotientBellmanStatus::Complete &&
            solved_quotient.executable_upper && solved_quotient.proper) {
            selected_projection = solved_quotient;
        } else {
            /* Once bootstrap alternatives exist, the proof graph correctly
             * contains more than one certified row. The inherited-selected
             * closure remains identified by selected_payload_id; reconstruct
             * that structural view instead of asking the whole proof graph to
             * be unique. */
            selected_projection.status =
                quotient::QuotientBellmanStatus::Complete;
            std::uint32_t state_count = 0;
            for (const quotient::QuotientCell& cell : final_cells) {
                const std::optional<std::uint32_t> state =
                    bellman.state_index_for_cell(cell.cell_id);
                if (state.has_value()) {
                    state_count = std::max(state_count, *state + 1);
                }
            }
            selected_projection.selected_rows_by_state.assign(
                state_count, std::numeric_limits<std::uint64_t>::max());
            std::vector<std::uint32_t> selected_payload_by_state(
                state_count, kNoId);
            for (const PublishedRow& row : published_rows) {
                if (!row.selected_payload_id.has_value() ||
                    !bellman.proof_store()->has_use_site(row.sparse_row)) {
                    continue;
                }
                const quotient::RowProofUseSite& use =
                    bellman.proof_store()->use_site(row.sparse_row);
                if (!use.valid ||
                    use.source_generation != row.source_generation) {
                    continue;
                }
                const std::optional<std::uint32_t> state =
                    bellman.state_index_for_cell(row.source_cell_id);
                if (!state.has_value()) continue;
                std::uint64_t& selected =
                    selected_projection.selected_rows_by_state.at(*state);
                if (selected !=
                        std::numeric_limits<std::uint64_t>::max() &&
                    selected != row.sparse_row) {
                    const SparseRow& prior_sparse =
                        bellman.transition_cache().rows.at(selected);
                    const SparseRow& next_sparse =
                        bellman.transition_cache().rows.at(row.sparse_row);
                    const solve_detail::PricedSparseRow& prior_priced =
                        bellman.priced_rows().at(selected);
                    const solve_detail::PricedSparseRow& next_priced =
                        bellman.priced_rows().at(row.sparse_row);
                    selected_projection.status =
                        quotient::QuotientBellmanStatus::InvalidRow;
                    selected_projection.failure_reason =
                        "inherited selected closure has multiple rows at "
                        "cell " + std::to_string(row.source_cell_id) +
                        " (payloads " + std::to_string(
                            selected_payload_by_state.at(*state)) +
                        "/" + std::to_string(*row.selected_payload_id) +
                        ", operators " + std::to_string(
                            prior_priced.operator_index) +
                        "/" + std::to_string(next_priced.operator_index) +
                        ", costs " + std::to_string(prior_priced.cost) +
                        "/" + std::to_string(next_priced.cost) +
                        ", transitions " + std::to_string(
                            prior_sparse.transition_count) +
                        "/" + std::to_string(
                            next_sparse.transition_count) + ")";
                    break;
                }
                selected = row.sparse_row;
                selected_payload_by_state.at(*state) =
                    *row.selected_payload_id;
            }
        }
        if (selected_projection.status !=
                quotient::QuotientBellmanStatus::Complete) {
            throw_failed_quotient(selected_projection);
        }
        auto initial_accounting =
            account_selected_closure({root_cell}, selected_projection);
        while (!initial_accounting.resume()) {
            ++progress.work_items;
            co_await solve_detail::CooperativeCheckpoint{
                initial_accounting.retained_bytes()};
        }
        (void)initial_accounting.take_result();
        const quotient::AlternativeActionAccountingAudit action_audit =
            bellman.proof_store()->audit_alternative_actions(
                admitted_action_accounting,
                completed_action_accounting,
                {},
                bellman.proof_store()->q_generation());
        telemetry.action_accounting_complete = action_audit.complete;
        telemetry.unresolved_alternative_obligations =
            action_audit.unresolved_actions;
        if (!action_audit.complete ||
            action_audit.unresolved_actions +
                    action_audit.other_certified_actions !=
                current_alternative_admissions) {
            throw AdapterFailure(
                PolicyExactLiftStatus::RefinementFailure,
                "selected-first quotient lost admitted-action accounting");
        }

        /* An inherited broad policy can become improper only after strict
         * carrier semantics are restored. The normal alternative scheduler
         * used to be unreachable in that case because it required a proper
         * selected-only upper before certifying any alternative. Repair the
         * bootstrap ordering on the smallest proved envelope: singleton
         * cells on the current dead path. A multi-carrier cell still requires
         * the carrier-wide scheduler below and is never guessed from its
         * representative. */
        struct BootstrapAlternativeResult {
            bool certified = false;
            std::vector<AbstractState> frontier;
        };
        std::set<std::pair<std::uint32_t, std::uint32_t>>
            bootstrap_attempted;
        const auto certify_singleton_bootstrap_alternative =
                [&](const std::uint32_t source_cell_id)
                    -> solve_detail::CooperativeTask<
                        BootstrapAlternativeResult> {
            BootstrapAlternativeResult result;
            const auto source_class = scheduler_class_by_cell.find(
                source_cell_id);
            if (source_class == scheduler_class_by_cell.end()) {
                co_return result;
            }
            const quotient::QuotientCell& cell =
                cell_for_class(source_class->second);
            if (cell.terminal || cell.coverage.exact_source_count != 1 ||
                cell.coverage.ranges.size() != 1 ||
                cell.coverage.ranges.front().count != 1) {
                co_return result;
            }
            const std::uint32_t ordinal = static_cast<std::uint32_t>(
                cell.coverage.ranges.front().begin);
            if (ordinal >= locators.size() ||
                rows_by_ordinal.at(ordinal).empty()) {
                co_return result;
            }
            const std::uint32_t inherited_action =
                raw_rows.at(rows_by_ordinal.at(ordinal).front())
                    .selected.action_id;
            const auto& alternatives =
                *alternatives_by_ordinal.at(ordinal);
            for (const std::uint32_t operator_index : alternatives) {
                if (operator_index == inherited_action ||
                    !bootstrap_attempted.insert(
                        {source_cell_id, operator_index}).second) {
                    continue;
                }
                const QuotientAlternativeDescriptor& descriptor =
                    oracle.quotient_alternative_descriptor(operator_index);
                std::optional<std::uint32_t> proof_obligation_id;
                for (std::uint32_t obligation_id = 0;
                     obligation_id < bellman.proof_store()
                         ->alternative_obligation_count();
                     ++obligation_id) {
                    const quotient::UnresolvedAlternativeObligation&
                        obligation = bellman.proof_store()
                            ->alternative_obligation(obligation_id);
                    if (obligation.identity.source_cell_id !=
                            cell.cell_id ||
                        obligation.identity.action != descriptor.action ||
                        (obligation.status !=
                             quotient::AlternativeObligationStatus::
                                 LowerOnly &&
                         obligation.status !=
                             quotient::AlternativeObligationStatus::
                                 Scheduled)) {
                        continue;
                    }
                    proof_obligation_id = obligation_id;
                    break;
                }
                if (!proof_obligation_id.has_value()) continue;
                ++progress.work_items;
                co_await solve_detail::CooperativeCheckpoint{
                    oracle.estimated_owned_bytes()};
                ExactState source =
                    oracle.quotient_materialize_locator(
                        locators.at(ordinal));
                std::optional<QuotientOracleCompactRow> candidate;
                try {
                    auto candidate_task = oracle
                        .quotient_certify_alternative_descriptor_cooperatively(
                            source, descriptor);
                    while (!candidate_task.resume()) {
                        ++progress.work_items;
                        co_await solve_detail::CooperativeCheckpoint{
                            candidate_task.retained_bytes()};
                    }
                    candidate = candidate_task.take_result();
                } catch (...) {
                    oracle.quotient_release_carrier(source.stable_key);
                    throw;
                }
                oracle.quotient_release_carrier(source.stable_key);
                if (!candidate.has_value()) continue;
                for (QuotientOracleCompactTransition& transition :
                     candidate->transitions) {
                    transition.strict_state =
                        oracle.quotient_canonical_locator(
                            transition.strict_state);
                }
                QuotientOracleCompactRow row =
                    canonical_raw_row(std::move(*candidate));
                const ObservationRequirement merged_requirement =
                    canonical_observation_requirement(
                        merge_observation_requirements(
                            cell.observation_requirement,
                            descriptor.routing_observes));
                if (merged_requirement !=
                    canonical_observation_requirement(
                        cell.observation_requirement)) {
                    continue;
                }

                std::map<std::uint32_t, solve_detail::WideFloat>
                    projected;
                for (const QuotientOracleCompactTransition& transition :
                     row.transitions) {
                    if (transition.strict_state >=
                            ordinal_by_strict_state.size() ||
                        ordinal_by_strict_state[
                            transition.strict_state] == kNoId) {
                        result.frontier.push_back(
                            oracle.quotient_export_locator(
                                transition.strict_state));
                        continue;
                    }
                    const std::uint32_t target_ordinal =
                        ordinal_by_strict_state[transition.strict_state];
                    projected[final_cell_id[
                        partition.class_by_node[target_ordinal]]] +=
                        solve_detail::WideFloat{
                            transition.probability};
                }
                if (!result.frontier.empty()) {
                    oracle.quotient_release_transient_kernel_caches();
                    co_return result;
                }

                quotient::QuotientBellmanRowInput input;
                input.source_cell_id = cell.cell_id;
                input.operator_index = row.selected.action_id;
                input.cost = row.action_cost;
                input.certified = true;
                std::vector<quotient::ProofProjectedArc> arcs;
                solve_detail::WideFloat total{0.0};
                for (const auto& [target, probability] : projected) {
                    input.transitions.push_back(
                        {{}, target, probability.value()});
                    arcs.push_back({
                        {},
                        bellman.shared_semantic_identity_for_cell(target),
                        probability.value()});
                    total += probability;
                }
                quotient::CoverageDescriptor row_coverage =
                    cell.coverage;
                for (quotient::CoverageRange& range :
                     row_coverage.ranges) {
                    range.total_probability =
                        static_cast<double>(range.count) /
                        static_cast<double>(
                            row_coverage.exact_source_count) *
                        total.value();
                }
                row_coverage.exact_total_probability = total.value();
                input.proof_identity = oracle.quotient_proof_identity(
                    source, row.selected,
                    quotient::canonical_coverage_descriptor(
                        std::move(row_coverage)),
                    std::move(arcs), total.value());
                const std::uint64_t sparse_row =
                    bellman.append_row(std::move(input));
                oracle.quotient_install_streamed_recipe(
                    row.selected, row.choice_recipe);
                published_rows.push_back({
                    sparse_row, cell.cell_id, cell.generation,
                    std::nullopt, row.selected});
                refresh_external_row_kernel_bytes();

                const quotient::UnresolvedAlternativeObligation& obligation =
                    bellman.proof_store()->alternative_obligation(
                        *proof_obligation_id);
                if (obligation.status ==
                        quotient::AlternativeObligationStatus::LowerOnly) {
                    bellman.proof_store()
                        ->transition_alternative_obligation(
                            *proof_obligation_id,
                            quotient::AlternativeObligationStatus::Scheduled);
                    saturating_add(
                        telemetry.alternative_obligations_scheduled, 1);
                }
                bellman.proof_store()
                    ->transition_alternative_obligation(
                        *proof_obligation_id,
                        quotient::AlternativeObligationStatus::Certified,
                        0, sparse_row);
                for (quotient::AccountedAlternativeAction& entry :
                     completed_action_accounting) {
                    if (entry.obligation_id == *proof_obligation_id) {
                        entry.kind = quotient::
                            AlternativeActionAccountingKind::OtherCertified;
                        entry.certified_row_id = sparse_row;
                        entry.obligation_id.reset();
                        break;
                    }
                }
                saturating_add(
                    telemetry.alternative_obligations_certified, 1);
                oracle.quotient_release_transient_kernel_caches();
                result.certified = true;
                co_return result;
            }
            co_return result;
        };

        if (!retained_global_lower_authority &&
            solved_quotient.status ==
                quotient::QuotientBellmanStatus::ImproperPolicy) {
            bool bootstrap_progress = true;
            while (solved_quotient.status ==
                       quotient::QuotientBellmanStatus::ImproperPolicy &&
                   bootstrap_progress) {
                bootstrap_progress = false;
                const std::vector<std::uint32_t> dead_cells =
                    solved_quotient.failure_path_cell_ids;
                /* The first state after the reachable predecessor is the
                 * earliest selected-policy divergence. Repairing the deepest
                 * member of a dead cycle first can materialize a broad
                 * renewal kernel that the repaired predecessor would never
                 * reach. */
                auto cell_it = dead_cells.begin();
                if (cell_it != dead_cells.end()) ++cell_it;
                for (; cell_it != dead_cells.end(); ++cell_it) {
                    auto bootstrap =
                        certify_singleton_bootstrap_alternative(*cell_it);
                    while (!bootstrap.resume()) {
                        ++progress.work_items;
                        co_await solve_detail::CooperativeCheckpoint{
                            bootstrap.retained_bytes()};
                    }
                    BootstrapAlternativeResult result =
                        bootstrap.take_result();
                    if (!result.frontier.empty()) {
                        oracle.quotient_sync_resource_telemetry();
                        telemetry.strict_states_discovered =
                            static_cast<std::uint32_t>(locators.size());
                        telemetry.strict_carriers_materialized =
                            static_cast<std::uint32_t>(locators.size());
                        co_return QuotientPassResult{
                            std::nullopt, std::move(result.frontier)};
                    }
                    if (!result.certified) continue;
                    bootstrap_progress = true;
                    solved_quotient = bellman.solve(
                        {root_cell}, limits.max_refinement_rounds,
                        std::max<std::uint32_t>(
                            1, options.max_sweeps));
                    if (solved_quotient.status !=
                            quotient::QuotientBellmanStatus::
                                ImproperPolicy) {
                        break;
                    }
                }
            }
        }
        const quotient::QuotientBellmanTelemetry& bellman_telemetry =
            bellman.telemetry();
        telemetry.proof_payload_reuses =
            bellman_telemetry.proof_payload_reuses;
        telemetry.row_reprojections =
            bellman_telemetry.row_reprojections;
        telemetry.quotient_source_splits =
            bellman_telemetry.source_splits;
        telemetry.quotient_target_splits =
            bellman_telemetry.target_splits;
        telemetry.reverse_invalidations =
            bellman_telemetry.reverse_invalidations;
        telemetry.improper_policy_repairs =
            bellman_telemetry.improper_policy_repairs;
        telemetry.local_reoptimization_rounds =
            bellman_telemetry.scc_evaluations;
        telemetry.local_state_action_rows_evaluated =
            bellman_telemetry.bellman_rows_evaluated;
        telemetry.local_reoptimizations =
            bellman_telemetry.policy_improvements;
        telemetry.local_policy_changes =
            bellman_telemetry.policy_improvements;
        telemetry.quotient_attractor_ns = bellman_telemetry.attractor_ns;
        telemetry.quotient_lower_relaxation_ns =
            bellman_telemetry.lower_relaxation_ns;
        telemetry.quotient_policy_seed_ns =
            bellman_telemetry.policy_seed_ns;
        telemetry.quotient_envelope_construction_ns =
            bellman_telemetry.envelope_construction_ns;
        telemetry.quotient_exact_policy_evaluation_ns =
            bellman_telemetry.exact_policy_evaluation_ns;
        telemetry.quotient_policy_improvement_ns =
            bellman_telemetry.policy_improvement_ns;
        telemetry.quotient_publication_audit_ns =
            bellman_telemetry.publication_audit_ns;
        telemetry.exact_carriers_replayed =
            static_cast<std::uint64_t>(locators.size()) *
            (observation_coarse.rounds + partition.rounds + 4);
        if (solved_quotient.status !=
                quotient::QuotientBellmanStatus::Complete ||
            (!retained_global_lower_authority &&
             (!solved_quotient.executable_upper ||
              !solved_quotient.proper))) {
            throw_failed_quotient(solved_quotient);
        }
        const auto publish_current_upper =
                [&](const quotient::QuotientBellmanResult& solved_upper,
                    const bool allow_reusable_assertion = true,
                    const bool materialize_compiled_assertion = true)
                    -> solve_detail::CooperativeTask<bool> {
        std::optional<CompiledPolicyAssertion> previous_assertion;
        bool previous_assertion_already_debited = false;
        if (!certificate.compiled.strategy_json.empty() ||
            !certificate.compiled.certification_strategy_json.empty()) {
            previous_assertion.emplace(
                std::move(certificate.compiled));
        } else if (allow_reusable_assertion &&
                   reusable_assertion != nullptr &&
                   reusable_assertion->has_value()) {
            previous_assertion.emplace(
                std::move(**reusable_assertion));
            reusable_assertion->reset();
            previous_assertion_already_debited = true;
        }
        certificate.refinement = {};
        certificate.class_evaluation = {};
        certificate.compiled = {};
        certificate.root_refinement_class = kNoId;
        certificate.exact_start_cost = kInfinity;
        certificate.absolute_cost_delta = kInfinity;
        certificate.relative_cost_delta = kInfinity;
        certificate.coarse_value_reconciled = false;
        certificate.lumpable = false;
        certificate.executable = false;
        std::map<std::uint64_t, const PublishedRow*> publication_by_row;
        for (const PublishedRow& row : published_rows) {
            publication_by_row.emplace(row.sparse_row, &row);
        }
        std::set<std::uint32_t> reachable(
            solved_upper.reachable_cell_ids.begin(),
            solved_upper.reachable_cell_ids.end());
        std::vector<std::uint32_t> reachable_classes;
        for (std::uint32_t cls = 0; cls < final_cell_id.size(); ++cls) {
            if (reachable.contains(final_cell_id[cls])) {
                reachable_classes.push_back(cls);
            }
        }
        std::map<std::uint32_t, std::uint32_t> local_by_cell;
        certificate.refinement.classes.resize(reachable_classes.size());
        for (std::uint32_t local = 0;
             local < reachable_classes.size(); ++local) {
            local_by_cell.emplace(
                final_cell_id[reachable_classes[local]], local);
        }
        certificate.refinement.status = RefinementStatus::Complete;
        certificate.refinement.executable = true;
        certificate.refinement.lumpable = true;
        const SolveTransitionCache& graph = bellman.transition_cache();
        std::map<std::pair<std::uint64_t, std::uint32_t>, std::uint32_t>
            transition_owner_by_span;
        for (std::uint32_t local = 0;
             local < reachable_classes.size(); ++local) {
            if (local != 0 && (local & 31u) == 0u) {
                co_await solve_detail::CooperativeCheckpoint{
                    oracle.estimated_owned_bytes()};
            }
            const std::uint32_t cls = reachable_classes[local];
            const quotient::QuotientCell& cell = cell_for_class(cls);
            RefinedPolicyClass& policy_class =
                certificate.refinement.classes[local];
            policy_class.class_id = local;
            policy_class.coarse_state = cell.coarse_state;
            policy_class.coarse_state_key = cell.coarse_parent;
            policy_class.terminal = cell.terminal;
            policy_class.goal = cell.terminal;
            for (const quotient::CoverageRange& range :
                 cell.coverage.ranges) {
                for (std::uint64_t ordinal = range.begin;
                     ordinal < range.begin + range.count; ++ordinal) {
                    policy_class.strict_members.push_back(
                        locators.at(ordinal));
                }
            }
            std::sort(
                policy_class.strict_members.begin(),
                policy_class.strict_members.end());
            if (std::adjacent_find(
                    policy_class.strict_members.begin(),
                    policy_class.strict_members.end()) !=
                policy_class.strict_members.end()) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::RefinementFailure,
                    "published quotient has overlapping strict locator coverage");
            }
            if (cell.terminal) continue;
            const std::uint32_t state =
                *bellman.state_index_for_cell(cell.cell_id);
            const std::uint64_t selected_row =
                solved_upper.selected_rows_by_state.at(state);
            policy_class.selected_action =
                publication_by_row.at(selected_row)->selected;
            /* The proof partition observes every admitted alternative so its
             * carrier-wide rows cannot merge behavior they would route
             * differently. The published policy needs only the observations
             * consumed by its selected action; exposing proof-only
             * observations here would make the compiler demand predicates
             * that cannot affect the executable continuation. */
            policy_class.required_observations =
                canonical_observation_requirement(
                    policy_class.selected_action->routing_observes);
            policy_class.observation_signature = observe_features(
                cell.observed_features,
                policy_class.required_observations);
            policy_class.action_cost =
                bellman.priced_rows().at(selected_row).cost;
            const SparseRow& sparse = graph.rows.at(selected_row);
            const auto [owner, inserted] =
                transition_owner_by_span.emplace(
                    std::pair{
                        sparse.transition_offset,
                        sparse.transition_count},
                    local);
            if (!inserted) {
                policy_class.transitions =
                    certificate.refinement.classes[
                        owner->second].transitions;
                continue;
            }
            policy_class.transitions.reserve(sparse.transition_count);
            for (std::uint32_t i = 0;
                 i < sparse.transition_count; ++i) {
                const std::uint64_t offset = sparse.transition_offset + i;
                const std::uint32_t target_state =
                    graph.successors.at(offset);
                const std::uint32_t target_cell =
                    *bellman.cell_id_for_state(target_state);
                policy_class.transitions.push_back({
                    local_by_cell.at(target_cell),
                    graph.probabilities.at(offset)});
            }
        }
        certificate.refinement.telemetry.exact_states = locators.size();
        certificate.refinement.telemetry.policy_reachable_coarse_states =
            telemetry.coarse_policy_states;
        certificate.refinement.telemetry.final_refinement_classes =
            reachable_classes.size();
        certificate.refinement.telemetry.initial_observation_classes =
            observation_coarse.initial_class_count;
        certificate.refinement.telemetry.partition_refinement_rounds =
            observation_coarse.rounds + partition.rounds;
        certificate.refinement.telemetry.merged_exact_states =
            locators.size() - reachable_classes.size();
        certificate.refinement.telemetry.lumpability_checks =
            observation_coarse.lumpability_checks +
            partition.lumpability_checks;

        certificate.root_refinement_class =
            local_by_cell.at(root_cell);
        PolicyEvaluationRequest evaluation_request;
        evaluation_request.start_classes = {
            certificate.root_refinement_class};
        evaluation_request.limits.max_reachable_classes =
            limits.max_refinement_classes;
        evaluation_request.limits.max_component_iterations =
            std::max<std::uint32_t>(1, options.max_sweeps);
        evaluation_request.limits.max_estimated_memory_bytes =
            limits.max_estimated_memory_bytes;
        const auto policy_evaluation_started =
            std::chrono::steady_clock::now();
        if (retained_global_lower_authority) {
            std::vector<std::uint32_t> group_by_local(
                certificate.refinement.classes.size(), kNoId);
            std::vector<const SparseRow*> sparse_by_local(
                certificate.refinement.classes.size(), nullptr);
            std::map<std::pair<bool, std::uint64_t>, std::uint32_t>
                initial_group;
            for (std::uint32_t local = 0;
                 local < certificate.refinement.classes.size(); ++local) {
                if (local != 0 && (local & 63u) == 0u) {
                    co_await solve_detail::CooperativeCheckpoint{
                        oracle.estimated_owned_bytes()};
                }
                const RefinedPolicyClass& cls =
                    certificate.refinement.classes[local];
                const std::pair initial_key{
                    cls.terminal,
                    cls.terminal
                        ? std::uint64_t{0}
                        : std::bit_cast<std::uint64_t>(cls.action_cost)};
                auto [found, inserted] = initial_group.emplace(
                    initial_key,
                    static_cast<std::uint32_t>(initial_group.size()));
                (void)inserted;
                group_by_local[local] = found->second;
                if (!cls.terminal) {
                    const quotient::QuotientCell& cell =
                        cell_for_class(reachable_classes[local]);
                    const std::uint32_t state =
                        *bellman.state_index_for_cell(cell.cell_id);
                    const std::uint64_t selected_row =
                        solved_upper.selected_rows_by_state.at(state);
                    sparse_by_local[local] =
                        &graph.rows.at(selected_row);
                }
            }
            using EvaluationProjection =
                std::vector<std::pair<std::uint32_t, double>>;
            std::uint32_t evaluation_partition_rounds = 0;
            for (; evaluation_partition_rounds <
                       limits.max_refinement_rounds;
                 ++evaluation_partition_rounds) {
                std::map<
                    std::pair<std::uint64_t, std::uint32_t>,
                    std::uint32_t> projection_id_by_span;
                std::map<EvaluationProjection, std::uint32_t>
                    projection_id_by_value;
                std::map<std::pair<std::uint32_t, std::uint32_t>,
                         std::uint32_t> next_group_by_signature;
                std::vector<std::uint32_t> next_group(
                    group_by_local.size(), kNoId);
                for (std::uint32_t local = 0;
                     local < group_by_local.size(); ++local) {
                    if (local != 0 && (local & 63u) == 0u) {
                        co_await solve_detail::CooperativeCheckpoint{
                            oracle.estimated_owned_bytes()};
                    }
                    std::uint32_t projection_value_id = 0;
                    if (sparse_by_local[local] != nullptr) {
                        const SparseRow& sparse =
                            *sparse_by_local[local];
                        const std::pair span{
                            sparse.transition_offset,
                            sparse.transition_count};
                        auto cached = projection_id_by_span.find(span);
                        if (cached == projection_id_by_span.end()) {
                            EvaluationProjection projection;
                            std::map<
                                std::uint32_t,
                                solve_detail::WideFloat> mass;
                            for (const ProjectedTransition& transition :
                                 certificate.refinement.classes[local]
                                     .transitions) {
                                mass[group_by_local.at(
                                    transition.successor_class)] +=
                                    solve_detail::WideFloat{
                                        transition.probability};
                            }
                            for (const auto& [target, probability] : mass) {
                                projection.push_back({
                                    target, probability.value()});
                            }
                            auto [projection_id, projection_inserted] =
                                projection_id_by_value.emplace(
                                    std::move(projection),
                                    static_cast<std::uint32_t>(
                                        projection_id_by_value.size()));
                            (void)projection_inserted;
                            cached = projection_id_by_span.emplace(
                                span, projection_id->second).first;
                        }
                        projection_value_id = cached->second;
                    } else {
                        auto [projection_id, projection_inserted] =
                            projection_id_by_value.emplace(
                                EvaluationProjection{},
                                static_cast<std::uint32_t>(
                                    projection_id_by_value.size()));
                        (void)projection_inserted;
                        projection_value_id = projection_id->second;
                    }
                    const std::pair signature{
                        group_by_local[local], projection_value_id};
                    auto [next, inserted] =
                        next_group_by_signature.emplace(
                            signature,
                            static_cast<std::uint32_t>(
                                next_group_by_signature.size()));
                    (void)inserted;
                    next_group[local] = next->second;
                }
                if (next_group == group_by_local) break;
                group_by_local = std::move(next_group);
            }
            if (evaluation_partition_rounds ==
                limits.max_refinement_rounds) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::ResourceCap,
                    "exact policy-evaluation partition reached its round cap",
                    "max_refinement_rounds");
            }
            const std::uint32_t evaluation_group_count =
                group_by_local.empty()
                    ? 0
                    : *std::max_element(
                          group_by_local.begin(),
                          group_by_local.end()) + 1;
            std::vector<std::uint32_t> representative_by_group(
                evaluation_group_count, kNoId);
            for (std::uint32_t local = 0;
                 local < group_by_local.size(); ++local) {
                if (local != 0 && (local & 255u) == 0u) {
                    co_await solve_detail::CooperativeCheckpoint{
                        oracle.estimated_owned_bytes()};
                }
                if (representative_by_group[group_by_local[local]] == kNoId) {
                    representative_by_group[group_by_local[local]] = local;
                }
            }
            RefinementResult evaluation_refinement;
            evaluation_refinement.status = RefinementStatus::Complete;
            evaluation_refinement.executable = true;
            evaluation_refinement.lumpable = true;
            evaluation_refinement.classes.resize(
                representative_by_group.size());
            for (std::uint32_t group = 0;
                 group < representative_by_group.size(); ++group) {
                if (group != 0 && (group & 63u) == 0u) {
                    co_await solve_detail::CooperativeCheckpoint{
                        oracle.estimated_owned_bytes()};
                }
                const RefinedPolicyClass& source =
                    certificate.refinement.classes[
                        representative_by_group[group]];
                RefinedPolicyClass& target =
                    evaluation_refinement.classes[group];
                target.class_id = group;
                target.coarse_state = source.coarse_state;
                target.coarse_state_key = source.coarse_state_key;
                target.terminal = source.terminal;
                target.goal = source.goal;
                target.selected_action = source.selected_action;
                target.action_cost = source.action_cost;
                std::map<std::uint32_t, solve_detail::WideFloat> mass;
                for (const ProjectedTransition& transition :
                     source.transitions) {
                    mass[group_by_local.at(transition.successor_class)] +=
                        solve_detail::WideFloat{transition.probability};
                }
                for (const auto& [target_group, probability] : mass) {
                    target.transitions.push_back({
                        target_group, probability.value()});
                }
            }
            evaluation_request.start_classes = {
                group_by_local.at(certificate.root_refinement_class)};
            PolicyEvaluationResult grouped =
                evaluate_refined_policy_exact(
                    evaluation_refinement,
                    std::move(evaluation_request));
            if (grouped.status == PolicyEvaluationStatus::Complete &&
                grouped.converged && grouped.proper) {
                std::vector<double> value_by_group(
                    evaluation_refinement.classes.size(), kInfinity);
                for (const RefinedClassValue& value :
                     grouped.class_values) {
                    value_by_group.at(value.class_id) = value.value;
                }
                grouped.class_values.clear();
                grouped.class_values.reserve(group_by_local.size());
                for (std::uint32_t local = 0;
                     local < group_by_local.size(); ++local) {
                    if (local != 0 && (local & 255u) == 0u) {
                        co_await solve_detail::CooperativeCheckpoint{
                            oracle.estimated_owned_bytes()};
                    }
                    grouped.class_values.push_back({
                        local,
                        value_by_group.at(group_by_local[local])});
                }
                grouped.reachable_classes = group_by_local.size();
                const std::uint64_t expanded_bytes =
                    grouped.class_values.capacity() *
                    sizeof(RefinedClassValue);
                saturating_add(
                    grouped.estimated_memory_bytes, expanded_bytes);
                grouped.peak_estimated_memory_bytes = std::max(
                    grouped.peak_estimated_memory_bytes,
                    grouped.estimated_memory_bytes);
            } else if (
                grouped.status ==
                PolicyEvaluationStatus::ImproperPolicy) {
                std::vector<std::uint32_t> expanded;
                for (const std::uint32_t group :
                     grouped.improper_component_classes) {
                    for (std::uint32_t local = 0;
                         local < group_by_local.size(); ++local) {
                        if (group_by_local[local] == group) {
                            expanded.push_back(local);
                        }
                    }
                }
                grouped.improper_component_classes = std::move(expanded);
            }
            certificate.class_evaluation = std::move(grouped);
        } else {
            evaluation_request.start_classes = {
                certificate.root_refinement_class};
            certificate.class_evaluation = evaluate_refined_policy_exact(
                certificate.refinement, std::move(evaluation_request));
        }
        telemetry.policy_evaluation_ns +=
            static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() -
                    policy_evaluation_started)
                    .count());
        if (certificate.class_evaluation.status !=
                PolicyEvaluationStatus::Complete ||
            !certificate.class_evaluation.converged ||
            !certificate.class_evaluation.proper ||
            certificate.class_evaluation.start_values.size() != 1) {
            throw AdapterFailure(
                certificate.class_evaluation.status ==
                        PolicyEvaluationStatus::ResourceCap
                    ? PolicyExactLiftStatus::ResourceCap
                    : PolicyExactLiftStatus::RefinementFailure,
                certificate.class_evaluation.failure_reason,
                certificate.class_evaluation.resource_cap);
        }
        certificate.exact_start_cost =
            certificate.class_evaluation.start_values.front();
        certificate.coarse_value_reconciled = reconciled(
            certificate.exact_start_cost, certificate.solver_cost,
            certificate.absolute_cost_delta,
            certificate.relative_cost_delta);
        certificate.policy_changed =
            bellman_telemetry.policy_improvements != 0;

        certificate.lumpable = true;
        if (telemetry.reference_adapter_invocations != 0) {
            throw AdapterFailure(
                PolicyExactLiftStatus::RefinementFailure,
                "production quotient invoked reference adapter");
        }
        if (!materialize_compiled_assertion) {
            /* A caller-owned, independently verified executable policy keeps
             * rollback publication safe while this quotient accounts for
             * alternatives. The class evaluation above is proof state only;
             * exact closure still compiles and evaluates the final selected
             * quotient policy below. */
            certificate.status = PolicyExactLiftStatus::Complete;
            certificate.executable = false;
            co_return true;
        }

        progress.phase = PolicyExactLiftPhase::Compiling;
        ProductionPolicyOracle::PreparedLiftedPolicyAssertion prepared =
            oracle.prepare_lifted_policy_assertion(
                certificate.refinement,
                certificate.class_evaluation);
        if (!prepared.ready) {
            certificate.compiled = std::move(prepared.immediate);
        } else {
            if (previous_assertion.has_value() &&
                !previous_assertion_already_debited) {
                const std::uint64_t cache_bytes =
                    compiled_policy_assertion_retained_bytes(
                        *previous_assertion);
                if (cache_bytes >=
                        prepared.options.max_solver_owned_bytes) {
                    previous_assertion.reset();
                } else {
                    prepared.options.max_solver_owned_bytes -=
                        cache_bytes;
                }
            }
            CompiledPolicyAssertionWork assertion_work =
                oracle.begin_lifted_policy_assertion(
                    prepared, strategy_name);
            bool previous_assertion_checked = false;
            while (!assertion_work.progress().done) {
                const CompiledPolicyAssertionProgress assertion_progress =
                    assertion_work.progress();
                progress.phase =
                    assertion_progress.phase ==
                            CompiledPolicyAssertionPhase::Compiling
                        ? PolicyExactLiftPhase::Compiling
                        : PolicyExactLiftPhase::Certifying;
                progress.evaluation = assertion_progress.evaluation;
                assertion_work.step(1);
                if (!previous_assertion_checked &&
                    assertion_progress.phase ==
                        CompiledPolicyAssertionPhase::Compiling) {
                    previous_assertion_checked = true;
                    (void)assertion_work
                        .try_reuse_completed_evaluation(
                            previous_assertion);
                    previous_assertion.reset();
                }
                ++progress.work_items;
                std::uint64_t retained =
                    oracle.estimated_owned_bytes();
                saturating_add(
                    retained, assertion_work.retained_bytes());
                co_await solve_detail::CooperativeCheckpoint{
                    retained};
            }
            certificate.compiled =
                oracle.finish_lifted_policy_assertion(
                    prepared,
                    assertion_work.take_result(),
                    certificate.refinement,
                    certificate.class_evaluation);
        }
        if (certificate.compiled.status !=
                CompiledPolicyAssertionStatus::Complete ||
            !certificate.compiled.paired_default_only ||
            !certificate.compiled.executable ||
            !certificate.compiled.proper ||
            !certificate.compiled.zero_off_policy ||
            !certificate.compiled.evaluation.cost_complete ||
            !std::isfinite(certificate.compiled.exact_cost) ||
            certificate.compiled.exact_cost < 0.0) {
            throw AdapterFailure(
                certificate.compiled.status ==
                        CompiledPolicyAssertionStatus::ResourceCap
                    ? PolicyExactLiftStatus::ResourceCap
                    : PolicyExactLiftStatus::CompiledAssertionFailure,
                certificate.compiled.failure_reason,
                certificate.compiled.resource_cap);
        }
        certificate.status = PolicyExactLiftStatus::Complete;
        certificate.executable = true;
        progress.verified_executable_upper_bound = std::min(
            progress.verified_executable_upper_bound,
            certificate.compiled.exact_cost);
        co_return true;
        };

        const auto retain_compiled_incumbent = [&] {
            if (reusable_assertion == nullptr ||
                (certificate.compiled.strategy_json.empty() &&
                 certificate.compiled
                     .certification_strategy_json.empty())) {
                return;
            }
            *reusable_assertion = std::move(certificate.compiled);
        };
        const auto reuse_compiled_incumbent =
                [&](const quotient::QuotientBellmanResult& solved_upper) {
            if (reusable_assertion == nullptr ||
                !reusable_assertion->has_value()) {
                return false;
            }
            CompiledPolicyAssertion& incumbent = **reusable_assertion;
            if (incumbent.status !=
                    CompiledPolicyAssertionStatus::Complete ||
                !incumbent.paired_default_only ||
                !incumbent.executable || !incumbent.proper ||
                !incumbent.zero_off_policy ||
                !incumbent.evaluation.cost_complete ||
                !std::isfinite(incumbent.exact_cost) ||
                incumbent.exact_cost < 0.0) {
                return false;
            }
            const std::optional<std::uint32_t> root_state =
                bellman.state_index_for_cell(root_cell);
            if (!root_state.has_value() ||
                *root_state >= solved_upper.values_by_state.size()) {
                return false;
            }
            const double current_root_cost =
                solved_upper.values_by_state[*root_state];
            double absolute_delta = kInfinity;
            double relative_delta = kInfinity;
            const bool current_q_reconciled = reconciled(
                incumbent.exact_cost, current_root_cost,
                absolute_delta, relative_delta);
            /* The compiled graph was parsed and independently evaluated in
             * the preceding frontier prefix. Growing the quotient cannot
             * invalidate that executable upper. Keep it while the expanded
             * proof graph schedules alternatives; only an actual public
             * policy improvement rebuilds below. A changed current Q remains
             * proof state, not publication authority, and cannot establish
             * exact closure without reconciliation below. */
            if (current_q_reconciled) {
                incumbent.solver_cost = current_root_cost;
                incumbent.absolute_cost_delta = absolute_delta;
                incumbent.relative_cost_delta = relative_delta;
                incumbent.cost_reconciled = true;
            }
            certificate.refinement.status = RefinementStatus::Complete;
            certificate.refinement.executable = true;
            certificate.refinement.lumpable = true;
            certificate.refinement.telemetry.exact_states = locators.size();
            certificate.refinement.telemetry.final_refinement_classes =
                solved_upper.reachable_cell_ids.size();
            certificate.class_evaluation.status =
                PolicyEvaluationStatus::Complete;
            certificate.class_evaluation.converged = true;
            certificate.class_evaluation.proper = true;
            certificate.class_evaluation.reachable_classes =
                static_cast<std::uint32_t>(
                    solved_upper.reachable_cell_ids.size());
            certificate.class_evaluation.start_values = {
                current_root_cost};
            certificate.root_refinement_class = 0;
            certificate.exact_start_cost = current_root_cost;
            certificate.absolute_cost_delta = absolute_delta;
            certificate.relative_cost_delta = relative_delta;
            certificate.coarse_value_reconciled = reconciled(
                current_root_cost, certificate.solver_cost,
                certificate.absolute_cost_delta,
                certificate.relative_cost_delta);
            certificate.policy_changed =
                bellman_telemetry.policy_improvements != 0;
            certificate.lumpable = true;
            certificate.executable = true;
            certificate.status = PolicyExactLiftStatus::Complete;
            progress.verified_executable_upper_bound = std::min(
                progress.verified_executable_upper_bound,
                incumbent.exact_cost);
            return true;
        };
        bool compiled_publication_built_this_pass = false;
        {
            if (!reuse_compiled_incumbent(solved_quotient)) {
                const bool verified_external_rollback_available =
                    std::isfinite(verified_rollback_upper_bound) &&
                    verified_rollback_upper_bound >= 0.0;
                auto publication_task = publish_current_upper(
                    solved_quotient,
                    !verified_external_rollback_available,
                    !verified_external_rollback_available);
                while (!publication_task.resume()) {
                    ++progress.work_items;
                    co_await solve_detail::CooperativeCheckpoint{
                        oracle.estimated_owned_bytes()};
                }
                (void)publication_task.take_result();
                if (verified_external_rollback_available) {
                    telemetry.interim_compiled_assertion_deferred = true;
                } else {
                    compiled_publication_built_this_pass = true;
                    retain_compiled_incumbent();
                }
            }
        }
        progress.phase = PolicyExactLiftPhase::LocalReoptimization;
        progress.evaluation = {};
        const auto local_reoptimization_started =
            std::chrono::steady_clock::now();
        if (!telemetry.work_to_first_executable_upper.has_value()) {
            telemetry.work_to_first_executable_upper =
                telemetry.strict_reforge_work;
            telemetry.wall_ns_to_first_executable_upper =
                elapsed_certification_ns();
            telemetry.alternatives_materialized_before_first_upper =
                telemetry.alternative_rows_completed;
        }
        double global_lower_delta = kInfinity;
        double global_lower_relative = kInfinity;
        certificate.global_lower_bound_closed =
            solved.converged &&
            solved.policy_status == SolvePolicyStatus::Exact &&
            std::isfinite(solved.lower_bound) &&
            solved.lower_bound >= 0.0 &&
            reconciled(
                certificate.exact_start_cost, solved.lower_bound,
                global_lower_delta, global_lower_relative);
        telemetry.global_lower_bound_closed =
            certificate.global_lower_bound_closed;

        /*
         * The selected-only publication above is the rollback authority for
         * lazy alternative work.  An alternative may consume the remaining
         * exact-work budget, but it must never erase an already compiled,
         * proper executable upper.  Completed rows are installed only after
         * every carrier in their source cell agrees on the exact projected
         * quotient row.  A row exposing a new frontier or a required source
         * split remains explicitly partial; no synthetic arc is installed.
         */
        quotient::QuotientBellmanResult current_solved = solved_quotient;
        PolicyExactLiftCertificate retained_publication = certificate;
        bool publication_blocked_after_improvement = false;
        bool stop_alternative_scheduling =
            certificate.global_lower_bound_closed;
        std::set<std::uint32_t> attempted_obligations;
        std::map<StableKey, AbstractState> frontier_states;
        const auto upper_by_source =
                [&](const quotient::QuotientBellmanResult& upper) {
            std::map<std::uint32_t, double> values;
            for (const quotient::QuotientCell& cell : final_cells) {
                const std::optional<std::uint32_t> state =
                    bellman.state_index_for_cell(cell.cell_id);
                if (state.has_value() &&
                    *state < upper.values_by_state.size()) {
                    values.emplace(
                        cell.cell_id,
                        upper.values_by_state[*state]);
                }
            }
            return values;
        };
        const auto mark_resource_interrupted =
                [&](const std::uint32_t obligation_id,
                    const std::uint64_t work_completed) {
            bellman.proof_store()->transition_alternative_obligation(
                obligation_id,
                quotient::AlternativeObligationStatus::
                    ResourceInterrupted,
                work_completed);
            saturating_add(
                telemetry
                    .alternative_obligations_resource_interrupted,
                1);
            telemetry.bounded_publication_retained = true;
            stop_alternative_scheduling = true;
        };

        while (!stop_alternative_scheduling) {
            const std::map<std::uint32_t, double> current_upper =
                upper_by_source(current_solved);
            for (std::uint32_t obligation_id = 0;
                 obligation_id <
                     bellman.proof_store()
                         ->alternative_obligation_count();
                 ++obligation_id) {
                const quotient::UnresolvedAlternativeObligation&
                    obligation = bellman.proof_store()
                        ->alternative_obligation(obligation_id);
                if (obligation.status !=
                        quotient::AlternativeObligationStatus::
                            ConditionallyNoncompetitive) {
                    continue;
                }
                const auto upper = current_upper.find(
                    obligation.identity.source_cell_id);
                if (upper != current_upper.end() &&
                    std::isfinite(upper->second) &&
                    obligation.identity.optimistic_lower.lower_q() >=
                        upper->second) {
                    bellman.proof_store()
                        ->transition_alternative_obligation(
                            obligation_id,
                            quotient::AlternativeObligationStatus::
                                ConditionallyNoncompetitive,
                            obligation.work_completed,
                            std::nullopt,
                            upper->second,
                            bellman.proof_store()->q_generation());
                } else {
                    bellman.proof_store()
                        ->transition_alternative_obligation(
                            obligation_id,
                            quotient::AlternativeObligationStatus::
                                Scheduled);
                    saturating_add(
                        telemetry.alternative_verdict_revocations, 1);
                }
            }

            std::vector<std::uint32_t> pending =
                bellman.proof_store()
                    ->ordered_pending_alternative_obligations();
            pending.erase(
                std::remove_if(
                    pending.begin(), pending.end(),
                    [&](const std::uint32_t obligation_id) {
                        return attempted_obligations.contains(
                            obligation_id);
                    }),
                pending.end());
            if (pending.empty()) break;
            saturating_add(telemetry.alternative_scheduling_rounds, 1);
            bool scheduler_progress_this_round = false;

            for (std::size_t pending_index = 0;
                 pending_index < pending.size(); ++pending_index) {
                /* One obligation can replay a large quotient carrier. Keep
                 * the scheduler boundary visible even when the preceding
                 * obligation happened to finish after only one carrier. */
                if (pending_index != 0) {
                    ++progress.work_items;
                    co_await solve_detail::CooperativeCheckpoint{
                        oracle.estimated_owned_bytes()};
                }
                const std::uint32_t obligation_id =
                    pending[pending_index];
                const quotient::UnresolvedAlternativeObligation before =
                    bellman.proof_store()->alternative_obligation(
                        obligation_id);
                const auto source_class = scheduler_class_by_cell.find(
                    before.identity.source_cell_id);
                if (source_class == scheduler_class_by_cell.end()) {
                    bellman.proof_store()
                        ->transition_alternative_obligation(
                            obligation_id,
                            quotient::AlternativeObligationStatus::Stale);
                    saturating_add(
                        telemetry.alternative_obligations_stale, 1);
                    attempted_obligations.insert(obligation_id);
                    continue;
                }
                const quotient::QuotientCell& cell =
                    cell_for_class(source_class->second);
                quotient::AlternativeObligationValidationContext context;
                context.source_cell_identity =
                    bellman.semantic_identity_for_cell(cell.cell_id);
                context.price_identity = price_identity.value();
                context.vocabulary_identity = vocabulary_identity.value();
                context.requirement_generation = cell.generation;
                context.source_generation = cell.generation;
                context.target_generation = cell.generation;
                context.partition_generation = cell.generation;
                context.action_generation = 1;
                context.admission_generation = 1;
                context.price_generation = 1;
                context.vocabulary_generation = 1;
                context.q_generation =
                    bellman.proof_store()->q_generation();
                if (bellman.proof_store()
                        ->validate_alternative_obligation(
                            obligation_id, before.identity, context) !=
                    quotient::AlternativeObligationValidationStatus::
                        Current) {
                    bellman.proof_store()
                        ->transition_alternative_obligation(
                            obligation_id,
                            quotient::AlternativeObligationStatus::Stale);
                    saturating_add(
                        telemetry.alternative_obligations_stale, 1);
                    attempted_obligations.insert(obligation_id);
                    continue;
                }
                const auto source_upper = current_upper.find(
                    cell.cell_id);
                if (source_upper != current_upper.end() &&
                    std::isfinite(source_upper->second) &&
                    before.identity.optimistic_lower.lower_q() >=
                        source_upper->second) {
                    bellman.proof_store()
                        ->transition_alternative_obligation(
                            obligation_id,
                            quotient::AlternativeObligationStatus::
                                ConditionallyNoncompetitive,
                            before.work_completed, std::nullopt,
                            source_upper->second,
                            bellman.proof_store()->q_generation());
                    saturating_add(
                        telemetry.alternative_obligations_noncompetitive,
                        1);
                    continue;
                }

                const std::uint32_t representative = representative_for(
                    partition.class_by_node, source_class->second);
                const auto& representative_operator_ids =
                    *alternatives_by_ordinal.at(representative);
                const auto operator_it = std::find(
                    representative_operator_ids.begin(),
                    representative_operator_ids.end(),
                    before.identity.action.action_id);
                if (operator_it == representative_operator_ids.end()) {
                    bellman.proof_store()
                        ->transition_alternative_obligation(
                            obligation_id,
                            quotient::AlternativeObligationStatus::Stale);
                    saturating_add(
                        telemetry.alternative_obligations_stale, 1);
                    attempted_obligations.insert(obligation_id);
                    continue;
                }
                const QuotientAlternativeDescriptor& descriptor =
                    oracle.quotient_alternative_descriptor(*operator_it);
                if (descriptor.action != before.identity.action) {
                    bellman.proof_store()
                        ->transition_alternative_obligation(
                            obligation_id,
                            quotient::AlternativeObligationStatus::Stale);
                    saturating_add(
                        telemetry.alternative_obligations_stale, 1);
                    attempted_obligations.insert(obligation_id);
                    continue;
                }
                bellman.proof_store()->transition_alternative_obligation(
                    obligation_id,
                    quotient::AlternativeObligationStatus::Scheduled);
                saturating_add(
                    telemetry.alternative_obligations_scheduled, 1);
                const std::uint64_t work_before =
                    telemetry.alternative_reforge_work;

                bool complete_cell_row = true;
                bool have_row = false;
                SelectedAction certified_selected;
                ExactChoiceRecipe certified_recipe;
                ExactState proof_source;
                double certified_cost = kInfinity;
                std::vector<std::pair<std::uint32_t, double>>
                    certified_projection;
                try {
                    bool replayed_carrier = false;
                    for (const quotient::CoverageRange& range :
                         cell.coverage.ranges) {
                        for (std::uint64_t ordinal = range.begin;
                             ordinal < range.begin + range.count;
                             ++ordinal) {
                            /* The previous implementation made the complete
                             * carrier-wide proof one atomic public solve step.
                             * Yield before every carrier after the first so
                             * large cells remain cancellable without retaining
                             * a materialized carrier or compact row across the
                             * suspension point. */
                            if (replayed_carrier) {
                                ++progress.work_items;
                                std::uint64_t retained =
                                    oracle.estimated_owned_bytes();
                                saturating_add(
                                    retained,
                                    ledger.snapshot().total_bytes);
                                co_await solve_detail::CooperativeCheckpoint{
                                    retained};
                            }
                            replayed_carrier = true;
                            ExactState source =
                                oracle.quotient_materialize_locator(
                                    locators.at(ordinal));
                            std::optional<QuotientOracleCompactRow>
                                candidate;
                            try {
                                auto candidate_task = oracle
                                    .quotient_certify_alternative_descriptor_cooperatively(
                                        source, descriptor);
                                while (!candidate_task.resume()) {
                                    ++progress.work_items;
                                    co_await solve_detail::CooperativeCheckpoint{
                                        candidate_task.retained_bytes()};
                                }
                                candidate = candidate_task.take_result();
                            } catch (...) {
                                oracle.quotient_release_carrier(
                                    source.stable_key);
                                throw;
                            }
                            oracle.quotient_release_carrier(
                                source.stable_key);
                            if (!candidate.has_value()) {
                                complete_cell_row = false;
                                break;
                            }
                            for (QuotientOracleCompactTransition& transition :
                                 candidate->transitions) {
                                transition.strict_state =
                                    oracle.quotient_canonical_locator(
                                        transition.strict_state);
                            }
                            QuotientOracleCompactRow row =
                                canonical_raw_row(
                                    std::move(*candidate));
                            if (row.selected.action_id !=
                                    before.identity.action.action_id ||
                                canonical_selected_runtime_contract_identity(
                                    row.selected) !=
                                    before.identity.action
                                        .runtime_contract_program_identity) {
                                complete_cell_row = false;
                                break;
                            }
                            std::map<
                                std::uint32_t,
                                solve_detail::WideFloat> projected;
                            bool reaches_new_frontier = false;
                            for (const auto& transition :
                                 row.transitions) {
                                if (transition.strict_state >=
                                        ordinal_by_strict_state.size() ||
                                    ordinal_by_strict_state[
                                        transition.strict_state] == kNoId) {
                                    AbstractState frontier =
                                        oracle.quotient_export_locator(
                                            transition.strict_state);
                                    frontier_states.emplace(
                                        exact_abstract_state_key(frontier, 0),
                                        std::move(frontier));
                                    reaches_new_frontier = true;
                                    continue;
                                }
                                const std::uint32_t target_ordinal =
                                    ordinal_by_strict_state[
                                        transition.strict_state];
                                projected[final_cell_id[
                                    partition.class_by_node[
                                        target_ordinal]]] +=
                                    solve_detail::WideFloat{
                                        transition.probability};
                            }
                            if (reaches_new_frontier) {
                                complete_cell_row = false;
                            }
                            if (!complete_cell_row) break;
                            std::vector<std::pair<std::uint32_t, double>>
                                projection;
                            for (const auto& [target, probability] :
                                 projected) {
                                projection.emplace_back(
                                    target, probability.value());
                            }
                            if (!have_row) {
                                have_row = true;
                                certified_selected = row.selected;
                                certified_recipe = row.choice_recipe;
                                proof_source = std::move(source);
                                certified_cost = row.action_cost;
                                certified_projection =
                                    std::move(projection);
                            } else {
                                if (row.selected.semantic_key !=
                                        certified_selected.semantic_key ||
                                    canonical_selected_runtime_contract_identity(
                                        row.selected) !=
                                        canonical_selected_runtime_contract_identity(
                                            certified_selected) ||
                                    std::fabs(
                                        certified_cost -
                                        row.action_cost) >
                                        limits
                                            .probability_sum_tolerance ||
                                    certified_projection.size() !=
                                        projection.size()) {
                                    complete_cell_row = false;
                                    break;
                                }
                                for (std::size_t i = 0;
                                     i < projection.size(); ++i) {
                                    if (certified_projection[i].first !=
                                            projection[i].first ||
                                        std::fabs(
                                            certified_projection[i]
                                                .second -
                                            projection[i].second) >
                                            limits
                                                .probability_sum_tolerance) {
                                        complete_cell_row = false;
                                        break;
                                    }
                                }
                                if (!complete_cell_row) break;
                            }
                        }
                        if (!complete_cell_row) break;
                    }
                    const ObservationRequirement merged_requirement =
                        canonical_observation_requirement(
                            merge_observation_requirements(
                                cell.observation_requirement,
                                descriptor.routing_observes));
                    if (merged_requirement !=
                        canonical_observation_requirement(
                            cell.observation_requirement)) {
                        complete_cell_row = false;
                    }
                    if (!complete_cell_row || !have_row) {
                        const std::uint64_t completed_work =
                            before.work_completed +
                            (telemetry.alternative_reforge_work >=
                                     work_before
                                 ? telemetry.alternative_reforge_work -
                                       work_before
                                 : 0);
                        bellman.proof_store()
                            ->transition_alternative_obligation(
                                obligation_id,
                                quotient::AlternativeObligationStatus::
                                    PartiallyEvaluated,
                                completed_work);
                        saturating_add(
                            telemetry
                                .alternative_obligations_partially_evaluated,
                            1);
                        attempted_obligations.insert(obligation_id);
                        oracle.quotient_release_transient_kernel_caches();
                        if (!frontier_states.empty()) {
                            /* Frontier discovery invalidates the premise of
                             * the remaining cell-local obligations. Return it
                             * immediately so the persistent session can grow,
                             * repartition, and invalidate through its existing
                             * generation ledger. Before broad rows became
                             * resumable this loop could replay thousands of
                             * already-doomed obligations and never reach the
                             * grow-in-place boundary before the watchdog. */
                            saturating_add(
                                telemetry.alternative_frontier_growth_yields,
                                1);
                            break;
                        }
                        continue;
                    }

                    quotient::QuotientBellmanRowInput input;
                    input.source_cell_id = cell.cell_id;
                    input.operator_index =
                        certified_selected.action_id;
                    input.cost = certified_cost;
                    input.certified = true;
                    std::vector<quotient::ProofProjectedArc> arcs;
                    solve_detail::WideFloat total{0.0};
                    for (const auto& [target, probability] :
                         certified_projection) {
                        input.transitions.push_back(
                            {{}, target, probability});
                        arcs.push_back({
                            {},
                            bellman.shared_semantic_identity_for_cell(target),
                            probability});
                        total += solve_detail::WideFloat{probability};
                    }
                    quotient::CoverageDescriptor row_coverage =
                        cell.coverage;
                    for (quotient::CoverageRange& range :
                         row_coverage.ranges) {
                        range.total_probability =
                            static_cast<double>(range.count) /
                            static_cast<double>(
                                row_coverage.exact_source_count) *
                            total.value();
                    }
                    row_coverage.exact_total_probability = total.value();
                    input.proof_identity =
                        oracle.quotient_proof_identity(
                            proof_source, certified_selected,
                            quotient::canonical_coverage_descriptor(
                                std::move(row_coverage)),
                            std::move(arcs), total.value());
                    const std::uint64_t sparse_row =
                        bellman.append_row(std::move(input));
                    oracle.quotient_install_streamed_recipe(
                        certified_selected, certified_recipe);
                    published_rows.push_back({
                        sparse_row, cell.cell_id, cell.generation,
                        std::nullopt,
                        certified_selected});
                    refresh_external_row_kernel_bytes();
                    const std::uint64_t completed_work =
                        before.work_completed +
                        (telemetry.alternative_reforge_work >=
                                 work_before
                             ? telemetry.alternative_reforge_work -
                                   work_before
                             : 0);
                    bellman.proof_store()
                        ->transition_alternative_obligation(
                            obligation_id,
                            quotient::AlternativeObligationStatus::
                                Certified,
                            completed_work, sparse_row);
                    for (quotient::AccountedAlternativeAction& entry :
                         completed_action_accounting) {
                        if (entry.obligation_id == obligation_id) {
                            entry.kind = quotient::
                                AlternativeActionAccountingKind::
                                    OtherCertified;
                            entry.certified_row_id = sparse_row;
                            entry.obligation_id.reset();
                            break;
                        }
                    }
                    saturating_add(
                        telemetry.alternative_obligations_certified, 1);
                    attempted_obligations.insert(obligation_id);
                    oracle.quotient_release_transient_kernel_caches();

                    std::vector<std::uint32_t> target_cells;
                    target_cells.reserve(certified_projection.size());
                    double candidate_q = certified_cost;
                    for (const auto& [target, probability] :
                         certified_projection) {
                        target_cells.push_back(target);
                        const std::uint32_t target_state =
                            *bellman.state_index_for_cell(target);
                        candidate_q += probability *
                            current_solved.values_by_state.at(
                                target_state);
                    }
                    const std::uint32_t source_state =
                        *bellman.state_index_for_cell(cell.cell_id);
                    auto account_targets = account_selected_closure(
                        target_cells, current_solved);
                    while (!account_targets.resume()) {
                        ++progress.work_items;
                        co_await solve_detail::CooperativeCheckpoint{
                            account_targets.retained_bytes()};
                    }
                    const bool proof_envelope_grew =
                        account_targets.take_result();
                    const bool candidate_improves = solve_detail::
                        sparse_policy_value_improvement_exceeds_tolerance(
                            candidate_q,
                            current_solved.values_by_state.at(source_state),
                            1e-12);
                    if (!proof_envelope_grew && !candidate_improves) {
                        continue;
                    }

                    quotient::QuotientBellmanResult improved =
                        bellman.solve(
                            {root_cell}, limits.max_refinement_rounds,
                            std::max<std::uint32_t>(
                                1, options.max_sweeps));
                    if (improved.status !=
                            quotient::QuotientBellmanStatus::Complete ||
                        !improved.executable_upper ||
                        !improved.proper ||
                        !improved.publication_audit.complete()) {
                        publication_blocked_after_improvement = true;
                        telemetry.bounded_publication_retained = true;
                        stop_alternative_scheduling = true;
                        break;
                    }
                    const std::uint32_t root_state =
                        *bellman.state_index_for_cell(root_cell);
                    bool public_policy_changed =
                        improved.reachable_cell_ids !=
                        current_solved.reachable_cell_ids;
                    if (!public_policy_changed) {
                        for (const std::uint32_t reachable_cell :
                             current_solved.reachable_cell_ids) {
                            const std::uint32_t reachable_state =
                                *bellman.state_index_for_cell(
                                    reachable_cell);
                            if (improved.selected_rows_by_state.at(
                                    reachable_state) !=
                                current_solved.selected_rows_by_state.at(
                                    reachable_state)) {
                                public_policy_changed = true;
                                break;
                            }
                        }
                    }
                    public_policy_changed =
                        public_policy_changed ||
                        solve_detail::
                            sparse_policy_value_improvement_exceeds_tolerance(
                                improved.values_by_state.at(root_state),
                                current_solved.values_by_state.at(root_state),
                                1e-12);
                    current_solved = std::move(improved);
                    if (public_policy_changed) {
                        try {
                            auto publication_task =
                                publish_current_upper(
                                    current_solved, false);
                            while (!publication_task.resume()) {
                                ++progress.work_items;
                                co_await solve_detail::CooperativeCheckpoint{
                                    oracle.estimated_owned_bytes()};
                            }
                            (void)publication_task.take_result();
                            compiled_publication_built_this_pass = true;
                            retain_compiled_incumbent();
                            progress.phase =
                                PolicyExactLiftPhase::LocalReoptimization;
                            progress.evaluation = {};
                        } catch (const AdapterFailure& error) {
                            if (error.status !=
                                PolicyExactLiftStatus::ResourceCap) {
                                throw;
                            }
                            certificate = retained_publication;
                            publication_blocked_after_improvement = true;
                            telemetry.bounded_publication_retained = true;
                            stop_alternative_scheduling = true;
                            break;
                        } catch (const quotient::ProofMemoryLimit&) {
                            certificate = retained_publication;
                            publication_blocked_after_improvement = true;
                            telemetry.bounded_publication_retained = true;
                            stop_alternative_scheduling = true;
                            break;
                        } catch (const SolverResourceLimit&) {
                            certificate = retained_publication;
                            publication_blocked_after_improvement = true;
                            telemetry.bounded_publication_retained = true;
                            stop_alternative_scheduling = true;
                            break;
                        }
                        retained_publication = certificate;
                        saturating_add(
                            telemetry.alternative_policy_improvements, 1);
                    }
                    scheduler_progress_this_round =
                        candidate_improves || proof_envelope_grew;
                    break;
                } catch (const AdapterFailure& error) {
                    oracle.quotient_release_transient_kernel_caches();
                    const std::uint64_t completed_work =
                        before.work_completed +
                        (telemetry.alternative_reforge_work >= work_before
                             ? telemetry.alternative_reforge_work -
                                   work_before
                             : 0);
                    if (error.status ==
                        PolicyExactLiftStatus::ResourceCap) {
                        mark_resource_interrupted(
                            obligation_id, completed_work);
                        break;
                    }
                    if (error.status ==
                        PolicyExactLiftStatus::InvalidSolveState) {
                        bellman.proof_store()
                            ->transition_alternative_obligation(
                                obligation_id,
                                quotient::AlternativeObligationStatus::
                                    Stale,
                                completed_work);
                        saturating_add(
                            telemetry.alternative_obligations_stale, 1);
                        attempted_obligations.insert(obligation_id);
                        continue;
                    }
                    throw;
                } catch (const quotient::ProofMemoryLimit&) {
                    oracle.quotient_release_transient_kernel_caches();
                    const std::uint64_t completed_work =
                        before.work_completed +
                        (telemetry.alternative_reforge_work >= work_before
                             ? telemetry.alternative_reforge_work -
                                   work_before
                             : 0);
                    mark_resource_interrupted(
                        obligation_id, completed_work);
                    break;
                } catch (const SolverResourceLimit&) {
                    oracle.quotient_release_transient_kernel_caches();
                    const std::uint64_t completed_work =
                        before.work_completed +
                        (telemetry.alternative_reforge_work >= work_before
                             ? telemetry.alternative_reforge_work -
                                   work_before
                             : 0);
                    mark_resource_interrupted(
                        obligation_id, completed_work);
                    break;
                }
            }
            if (!scheduler_progress_this_round) break;
        }

        if (!frontier_states.empty()) {
            if (reusable_assertion != nullptr &&
                (!certificate.compiled.strategy_json.empty() ||
                 !certificate.compiled
                      .certification_strategy_json.empty())) {
                *reusable_assertion =
                    std::move(certificate.compiled);
            }
            oracle.quotient_sync_resource_telemetry();
            telemetry.strict_states_discovered =
                static_cast<std::uint32_t>(locators.size());
            telemetry.strict_carriers_materialized =
                static_cast<std::uint32_t>(locators.size());
            std::vector<AbstractState> states;
            states.reserve(frontier_states.size());
            for (auto& [identity, state] : frontier_states) {
                (void)identity;
                states.push_back(std::move(state));
            }
            co_return QuotientPassResult{
                std::nullopt, std::move(states)};
        }

        const std::map<std::uint32_t, double> current_upper =
            upper_by_source(current_solved);
        const quotient::AlternativeActionAccountingAudit final_action_audit =
            bellman.proof_store()->audit_alternative_actions(
                admitted_action_accounting,
                completed_action_accounting,
                current_upper,
                bellman.proof_store()->q_generation());
        if (!final_action_audit.complete) {
            throw AdapterFailure(
                PolicyExactLiftStatus::RefinementFailure,
                "competitive alternative scheduler lost admitted-action accounting");
        }
        telemetry.action_accounting_complete = true;
        const std::optional<std::uint32_t> final_root_state =
            bellman.state_index_for_cell(root_cell);
        const auto assertion_reconciles_current_q =
                [&](const CompiledPolicyAssertion& assertion) {
            double absolute_delta = kInfinity;
            double relative_delta = kInfinity;
            return final_root_state.has_value() &&
                   *final_root_state <
                       current_solved.values_by_state.size() &&
                   std::isfinite(assertion.exact_cost) &&
                   reconciled(
                       assertion.exact_cost,
                       current_solved.values_by_state[*final_root_state],
                       absolute_delta, relative_delta);
        };
        const bool envelope_ready_for_exact_publication =
            final_action_audit.exact_alternative_envelope_closed &&
            !publication_blocked_after_improvement;
        const bool retained_assertion_reconciles_current_q =
            reusable_assertion != nullptr &&
            reusable_assertion->has_value() &&
            assertion_reconciles_current_q(**reusable_assertion);
        const bool current_assertion_reconciles_current_q =
            assertion_reconciles_current_q(certificate.compiled);
        const bool final_assertion_required =
            (certificate.global_lower_bound_closed ||
             envelope_ready_for_exact_publication) &&
            !retained_assertion_reconciles_current_q &&
            !current_assertion_reconciles_current_q;
        if (final_assertion_required) {
            /* A retained incumbent is sufficient during lazy proof work.
             * Exact closure additionally needs the final selected quotient
             * policy to compile and independently evaluate once. */
            const PolicyExactLiftCertificate retained = certificate;
            try {
                auto publication_task =
                    publish_current_upper(current_solved, false);
                while (!publication_task.resume()) {
                    ++progress.work_items;
                    co_await solve_detail::CooperativeCheckpoint{
                        oracle.estimated_owned_bytes()};
                }
                (void)publication_task.take_result();
                compiled_publication_built_this_pass = true;
                reusable_assertion->reset();
            } catch (const AdapterFailure& error) {
                if (error.status != PolicyExactLiftStatus::ResourceCap) {
                    throw;
                }
                certificate = retained;
                publication_blocked_after_improvement = true;
            } catch (const quotient::ProofMemoryLimit&) {
                certificate = retained;
                publication_blocked_after_improvement = true;
            } catch (const SolverResourceLimit&) {
                certificate = retained;
                publication_blocked_after_improvement = true;
            }
        }
        if (reusable_assertion != nullptr &&
            reusable_assertion->has_value() &&
            certificate.compiled.strategy_json.empty() &&
            certificate.compiled.certification_strategy_json.empty()) {
            certificate.compiled =
                std::move(**reusable_assertion);
            reusable_assertion->reset();
        }
        double publication_q_absolute_delta = kInfinity;
        double publication_q_relative_delta = kInfinity;
        const bool publication_reconciles_current_q =
            final_root_state.has_value() &&
            *final_root_state < current_solved.values_by_state.size() &&
            std::isfinite(certificate.compiled.exact_cost) &&
            reconciled(
                certificate.compiled.exact_cost,
                current_solved.values_by_state[*final_root_state],
                publication_q_absolute_delta,
                publication_q_relative_delta);
        const bool exact_alternative_envelope_closed =
            final_action_audit.exact_alternative_envelope_closed &&
            !publication_blocked_after_improvement &&
            (publication_reconciles_current_q ||
             compiled_publication_built_this_pass);
        /*
         * Reconciliation with an already-exact coarse lower is the cheap
         * closure path above. When a strict quotient changes that value, the
         * complete action-accounting audit is the independent closure proof:
         * the root-selected closure is expanded through every certified
         * alternative row, and every admitted action at every cell in that
         * proof envelope is either carrier-wide certified or dominated by a
         * carrier-wide lower-Q certificate at the current Q generation. With
         * no blocked improved publication, the retained proper root policy is
         * therefore the global Bellman fixed point even when unrelated live
         * quotient cells were never materialized into obligations.
         */
        certificate.global_lower_bound_closed =
            certificate.global_lower_bound_closed ||
            exact_alternative_envelope_closed;
        telemetry.global_lower_bound_closed =
            certificate.global_lower_bound_closed;
        telemetry.unresolved_alternative_obligations =
            final_action_audit.unresolved_actions;
        telemetry.competitive_alternatives_remaining =
            certificate.global_lower_bound_closed
                ? 0
                : final_action_audit.unresolved_actions;
        if (publication_blocked_after_improvement) {
            saturating_add(
                telemetry.competitive_alternatives_remaining, 1);
        }
        telemetry.exact_alternative_envelope_closed =
            certificate.global_lower_bound_closed ||
            exact_alternative_envelope_closed;
        telemetry.bounded_publication_retained =
            telemetry.bounded_publication_retained ||
            (certificate.executable &&
             !certificate.global_lower_bound_closed &&
             !telemetry.exact_alternative_envelope_closed);

        telemetry.local_reoptimization_ns +=
            static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() -
                    local_reoptimization_started)
                    .count());
        const quotient::QuotientBellmanTelemetry& final_bellman_telemetry =
            bellman.telemetry();
        telemetry.proof_payload_reuses =
            final_bellman_telemetry.proof_payload_reuses;
        telemetry.row_reprojections =
            final_bellman_telemetry.row_reprojections;
        telemetry.reverse_invalidations =
            final_bellman_telemetry.reverse_invalidations;
        telemetry.improper_policy_repairs =
            final_bellman_telemetry.improper_policy_repairs;
        telemetry.local_reoptimization_rounds =
            final_bellman_telemetry.scc_evaluations;
        telemetry.local_reoptimizations =
            final_bellman_telemetry.policy_improvements;
        telemetry.local_policy_changes =
            final_bellman_telemetry.policy_improvements;
        telemetry.quotient_attractor_ns =
            final_bellman_telemetry.attractor_ns;
        telemetry.quotient_lower_relaxation_ns =
            final_bellman_telemetry.lower_relaxation_ns;
        telemetry.quotient_policy_seed_ns =
            final_bellman_telemetry.policy_seed_ns;
        telemetry.quotient_envelope_construction_ns =
            final_bellman_telemetry.envelope_construction_ns;
        telemetry.quotient_exact_policy_evaluation_ns =
            final_bellman_telemetry.exact_policy_evaluation_ns;
        telemetry.quotient_policy_improvement_ns =
            final_bellman_telemetry.policy_improvement_ns;
        telemetry.quotient_publication_audit_ns =
            final_bellman_telemetry.publication_audit_ns;
        const quotient::ProofMemorySnapshot memory =
            ledger.snapshot();
        telemetry.coverage_descriptor_bytes = memory.bytes[
            static_cast<std::size_t>(
                quotient::ProofMemoryCategory::CoverageDescriptor)];
        telemetry.certificate_bytes = memory.bytes[
            static_cast<std::size_t>(
                quotient::ProofMemoryCategory::Certificate)];
        telemetry.dependency_sidecar_bytes = memory.bytes[
            static_cast<std::size_t>(
                quotient::ProofMemoryCategory::DependencySidecar)];
        telemetry.alternative_obligation_bytes = memory.bytes[
            static_cast<std::size_t>(
                quotient::ProofMemoryCategory::AlternativeObligation)];
        telemetry.partition_bytes = memory.bytes[
            static_cast<std::size_t>(
                quotient::ProofMemoryCategory::Partition)];
        telemetry.row_kernel_bytes = memory.bytes[
            static_cast<std::size_t>(
                quotient::ProofMemoryCategory::RowKernel)];
        telemetry.scratch_bytes = memory.bytes[
            static_cast<std::size_t>(
                quotient::ProofMemoryCategory::Scratch)];
        telemetry.total_solver_owned_bytes = oracle.estimated_owned_bytes();
        saturating_add(
            telemetry.total_solver_owned_bytes, memory.peak_total_bytes);
        certificate.refinement.telemetry.estimated_memory_bytes =
            memory.total_bytes;
        certificate.refinement.telemetry.peak_estimated_memory_bytes =
            memory.peak_total_bytes;
        telemetry.strict_states_discovered =
            static_cast<std::uint32_t>(locators.size());
        telemetry.strict_carriers_materialized =
            static_cast<std::uint32_t>(locators.size());
        certificate.status = PolicyExactLiftStatus::Complete;
        certificate.executable = certificate.compiled.executable;
    } catch (const AdapterFailure& error) {
        certificate.status = error.status;
        certificate.failure_reason = error.what();
        certificate.resource_cap = error.cap;
    } catch (const quotient::ProofMemoryLimit& error) {
        certificate.status = PolicyExactLiftStatus::ResourceCap;
        certificate.failure_reason = error.what();
        certificate.resource_cap = "max_estimated_memory_bytes";
    } catch (const SolverResourceLimit& error) {
        certificate.status = PolicyExactLiftStatus::ResourceCap;
        certificate.failure_reason = error.what();
        certificate.resource_cap = adapter_resource_cap_name(error.cap_name());
    } catch (const std::exception& error) {
        certificate.status = PolicyExactLiftStatus::RefinementFailure;
        certificate.failure_reason = error.what();
    }
    telemetry.compilation_ns += certificate.compiled.compilation_ns;
    telemetry.exact_evaluation_ns +=
        certificate.compiled.exact_evaluation_ns;
    telemetry.total_ns = elapsed_certification_ns();
    certificate.adapter = telemetry;
    progress.phase = PolicyExactLiftPhase::Done;
    progress.done = true;
    co_return QuotientPassResult{
        std::move(certificate), {}};
}

} // namespace

struct PolicyExactLiftWork::Impl {
    CalcContext& coarse;
    const SolveResult& solved;
    pc_item_state exact_start{};
    const std::unordered_map<std::string, double>& prices;
    SolveOptions options;
    std::string strategy_name;
    RefinementLimits limits;
    std::unique_ptr<PersistentQuotientSession> session;
    std::map<StableKey, AbstractState> frontier_by_identity;
    SolveOptions active_pass_options;
    PolicyExactLiftProgress progress;
    double best_verified_executable_upper_bound = kInfinity;
    solve_detail::CooperativeTask<QuotientPassResult> pass;
    std::optional<CompiledPolicyAssertion> reusable_assertion;
    std::optional<PolicyExactLiftCertificate> completed;

    Impl(
            CalcContext& coarse_value,
            const SolveResult& solved_value,
            const pc_item_state& exact_start_value,
            const std::unordered_map<std::string, double>& prices_value,
            const SolveOptions& options_value,
            std::string strategy_name_value,
            const RefinementLimits* limits_override,
            const PolicyExactLiftRollbackUpper* rollback_upper)
        : coarse(coarse_value),
          solved(solved_value),
          exact_start(exact_start_value),
          prices(prices_value),
          options(options_value),
          strategy_name(std::move(strategy_name_value)),
          limits(
              limits_override == nullptr
                  ? default_limits(coarse, solved, options)
                  : *limits_override),
          session(std::make_unique<PersistentQuotientSession>(
              coarse, solved, exact_start, prices, options, limits)) {
        if (rollback_upper != nullptr &&
            std::isfinite(rollback_upper->exact_cost) &&
            rollback_upper->exact_cost >= 0.0) {
            best_verified_executable_upper_bound =
                rollback_upper->exact_cost;
        }
        begin_pass();
    }

    PolicyExactLiftCertificate resource_cap(
            const char* reason, const char* cap) const {
        PolicyExactLiftCertificate certificate;
        certificate.solver_cost = solved.evaluated_policy_cost;
        certificate.status = PolicyExactLiftStatus::ResourceCap;
        certificate.failure_reason = reason;
        certificate.resource_cap = cap;
        certificate.adapter = session->telemetry;
        return certificate;
    }

    void begin_pass() {
        std::vector<AbstractState> frontier_seeds;
        frontier_seeds.reserve(frontier_by_identity.size());
        for (const auto& [identity, state] : frontier_by_identity) {
            (void)identity;
            frontier_seeds.push_back(state);
        }
        progress = {};
        active_pass_options = options;
        if (reusable_assertion.has_value()) {
            const std::uint64_t cache_bytes =
                compiled_policy_assertion_retained_bytes(
                    *reusable_assertion);
            if (cache_bytes >=
                    active_pass_options.max_solver_owned_bytes) {
                reusable_assertion.reset();
            } else {
                active_pass_options.max_solver_owned_bytes -=
                    cache_bytes;
            }
        }
        pass = lift_policy_quotient_pass_task(
            coarse, solved, exact_start, prices,
            active_pass_options,
            strategy_name, *session, std::move(frontier_seeds),
            progress, &reusable_assertion,
            best_verified_executable_upper_bound);
        progress.verified_executable_upper_bound =
            best_verified_executable_upper_bound;
    }

    void retain_verified_progress_upper() {
        if (std::isfinite(
                progress.verified_executable_upper_bound)) {
            best_verified_executable_upper_bound = std::min(
                best_verified_executable_upper_bound,
                progress.verified_executable_upper_bound);
        }
        progress.verified_executable_upper_bound =
            best_verified_executable_upper_bound;
    }

    void accept_frontier(std::vector<AbstractState> states) {
        const auto update_started = std::chrono::steady_clock::now();
        bool grew = false;
        std::uint64_t inserted_states = 0;
        for (AbstractState& state : states) {
            const StableKey identity =
                exact_abstract_state_key(state, 0);
            auto [found, inserted] = frontier_by_identity.emplace(
                identity, std::move(state));
            (void)found;
            grew = grew || inserted;
            if (inserted) ++inserted_states;
        }
        if (!grew) {
            PolicyExactLiftCertificate certificate;
            certificate.solver_cost = solved.evaluated_policy_cost;
            certificate.status =
                PolicyExactLiftStatus::RefinementFailure;
            certificate.failure_reason =
                "competitive quotient frontier did not grow";
            completed = std::move(certificate);
            return;
        }
        saturating_add(
            session->telemetry.strict_frontier_insertions, 1);
        saturating_add(
            session->telemetry.strict_frontier_states_inserted,
            inserted_states);
        if (frontier_by_identity.size() >= limits.max_exact_states) {
            completed = resource_cap(
                "competitive quotient frontier reached max_exact_states",
                "max_exact_states");
            return;
        }
        if (session->telemetry.strict_kernels_built >=
            limits.max_exact_kernels) {
            completed = resource_cap(
                "competitive quotient frontier reached max_exact_kernels",
                "max_exact_kernels");
            return;
        }
        if (session->telemetry.strict_transitions_built >=
            limits.max_transitions) {
            completed = resource_cap(
                "competitive quotient frontier reached max_transitions",
                "max_transitions");
            return;
        }
        std::uint64_t spent_rounds =
            session->telemetry.backward_observation_rounds;
        saturating_add(
            spent_rounds,
            session->telemetry.exact_fixed_point_rounds);
        saturating_add(
            spent_rounds,
            session->telemetry.local_reoptimization_rounds);
        if (spent_rounds >= limits.max_refinement_rounds) {
            completed = resource_cap(
                "competitive quotient frontier reached "
                "max_refinement_rounds",
                "max_refinement_rounds");
            return;
        }
        if (session->telemetry.strict_reforge_logical_work_v1 >=
            options.max_reforge_work) {
            completed = resource_cap(
                "competitive quotient frontier reached max_reforge_work",
                "max_reforge_work");
            return;
        }
        session->telemetry.strict_frontier_update_max_ns = std::max(
            session->telemetry.strict_frontier_update_max_ns,
            static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() - update_started)
                    .count()));
        begin_pass();
    }

    void step(std::uint32_t max_work_items) {
        std::uint32_t remaining =
            std::max<std::uint32_t>(1, max_work_items);
        while (remaining-- != 0 && !completed.has_value()) {
            if (!pass.resume()) {
                retain_verified_progress_upper();
                continue;
            }
            retain_verified_progress_upper();
            QuotientPassResult result = pass.take_result();
            if (!result.frontier_states.empty()) {
                accept_frontier(std::move(result.frontier_states));
                continue;
            }
            if (!result.certificate.has_value()) {
                throw std::logic_error(
                    "quotient update produced no certificate or frontier");
            }
            progress.phase = PolicyExactLiftPhase::Done;
            progress.done = true;
            reusable_assertion.reset();
            completed = std::move(*result.certificate);
        }
    }

    std::uint64_t retained_bytes() const {
        std::uint64_t session_bytes =
            session->oracle.estimated_owned_bytes();
        const std::uint64_t proof_bytes =
            session->bellman.proof_store()->ledger()
                .snapshot().total_bytes;
        if (proof_bytes != 0) {
            saturating_add(session_bytes, proof_bytes);
        } else {
            saturating_add(
                session_bytes,
                persistent_selected_closure_bytes(
                    session->selected_closure));
            saturating_add(
                session_bytes,
                persistent_published_rows_bytes(
                    session->published_rows));
        }
        std::uint64_t bytes = std::max<std::uint64_t>(
            static_cast<std::uint64_t>(pass.retained_bytes()),
            session_bytes);
        if (reusable_assertion.has_value()) {
            saturating_add(
                bytes,
                compiled_policy_assertion_retained_bytes(
                    *reusable_assertion));
        }
        for (const auto& [identity, state] : frontier_by_identity) {
            saturating_add(
                bytes,
                identity.capacity() * sizeof(std::uint64_t));
            (void)state;
            saturating_add(bytes, sizeof(AbstractState));
        }
        return bytes;
    }

    void refresh_live_telemetry() {
        session->oracle.quotient_sync_resource_telemetry();
        PolicyLiftAdapterTelemetry& telemetry = session->telemetry;
        const quotient::ProofMemorySnapshot memory =
            session->bellman.proof_store()->ledger().snapshot();
        telemetry.coverage_descriptor_bytes = memory.bytes[
            static_cast<std::size_t>(
                quotient::ProofMemoryCategory::CoverageDescriptor)];
        telemetry.certificate_bytes = memory.bytes[
            static_cast<std::size_t>(
                quotient::ProofMemoryCategory::Certificate)];
        telemetry.dependency_sidecar_bytes = memory.bytes[
            static_cast<std::size_t>(
                quotient::ProofMemoryCategory::DependencySidecar)];
        telemetry.alternative_obligation_bytes = memory.bytes[
            static_cast<std::size_t>(
                quotient::ProofMemoryCategory::AlternativeObligation)];
        telemetry.partition_bytes = memory.bytes[
            static_cast<std::size_t>(
                quotient::ProofMemoryCategory::Partition)];
        telemetry.carrier_bytes = memory.bytes[
            static_cast<std::size_t>(
                quotient::ProofMemoryCategory::Carrier)];
        telemetry.row_kernel_bytes = memory.bytes[
            static_cast<std::size_t>(
                quotient::ProofMemoryCategory::RowKernel)];
        telemetry.scratch_bytes = memory.bytes[
            static_cast<std::size_t>(
                quotient::ProofMemoryCategory::Scratch)];
        telemetry.total_solver_owned_bytes =
            session->oracle.estimated_owned_bytes();
        saturating_add(
            telemetry.total_solver_owned_bytes,
            memory.total_bytes);
        telemetry.adapter_owned_bytes = retained_bytes();
        telemetry.peak_adapter_owned_bytes = std::max(
            telemetry.peak_adapter_owned_bytes,
            telemetry.adapter_owned_bytes);
        telemetry.total_ns = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - session->started_at)
                .count());
    }
};

PolicyExactLiftWork::PolicyExactLiftWork(
        CalcContext& coarse,
        const SolveResult& solved,
        const pc_item_state& exact_start,
        const std::unordered_map<std::string, double>& prices,
        const SolveOptions& options,
        std::string strategy_name,
        const RefinementLimits* limits_override,
        const PolicyExactLiftRollbackUpper* rollback_upper)
    : impl_(std::make_unique<Impl>(
          coarse, solved, exact_start, prices, options,
          std::move(strategy_name), limits_override, rollback_upper)) {}

PolicyExactLiftWork::~PolicyExactLiftWork() = default;
PolicyExactLiftWork::PolicyExactLiftWork(
    PolicyExactLiftWork&&) noexcept = default;
PolicyExactLiftWork& PolicyExactLiftWork::operator=(
    PolicyExactLiftWork&&) noexcept = default;

void PolicyExactLiftWork::step(const std::uint32_t max_work_items) {
    impl_->step(max_work_items);
}

PolicyExactLiftProgress PolicyExactLiftWork::progress() const {
    return impl_->progress;
}

const PolicyLiftAdapterTelemetry&
PolicyExactLiftWork::live_adapter_telemetry() {
    impl_->refresh_live_telemetry();
    return impl_->session->telemetry;
}

PolicyExactLiftCertificate PolicyExactLiftWork::take_result() {
    if (!impl_->completed.has_value()) {
        throw std::logic_error("policy exact lift work is not finished");
    }
    return std::move(*impl_->completed);
}

std::uint64_t PolicyExactLiftWork::retained_bytes() const {
    return impl_->retained_bytes();
}

PolicyExactLiftCertificate lift_policy_quotient(
        CalcContext& coarse,
        const SolveResult& solved,
        const pc_item_state& exact_start,
        const std::unordered_map<std::string, double>& prices,
        const SolveOptions& options,
        const std::string& strategy_name,
        const RefinementLimits* limits_override) {
    PolicyExactLiftWork work(
        coarse, solved, exact_start, prices, options,
        strategy_name, limits_override);
    while (!work.progress().done) work.step(1024);
    return work.take_result();
}

PolicyExactLiftCertificate lift_policy_exact(
        CalcContext& coarse,
        const SolveResult& solved,
        const pc_item_state& exact_start,
        const std::unordered_map<std::string, double>& prices,
        const SolveOptions& options,
        const std::string& strategy_name,
        const RefinementLimits* limits_override) {
    PolicyExactLiftCertificate certificate;
    certificate.solver_cost = solved.evaluated_policy_cost;
    RefinementLimits limits =
        limits_override == nullptr
            ? default_limits(coarse, solved, options)
            : *limits_override;
    PolicyLiftAdapterTelemetry lift_telemetry;
    PolicyLiftAdapterTelemetry reoptimization_telemetry;
    const auto capture_adapter_telemetry = [&] {
        PolicyLiftAdapterTelemetry total;
        merge_adapter_telemetry(total, lift_telemetry);
        merge_adapter_telemetry(
            total, reoptimization_telemetry);
        certificate.adapter = std::move(total);
    };
    try {
        const auto check_initial_memory =
            [&](ProductionPolicyOracle& oracle,
                const RefinementLimits& pass_limits) {
                if (oracle.estimated_owned_bytes() >=
                    pass_limits.max_estimated_memory_bytes) {
                    throw AdapterFailure(
                        PolicyExactLiftStatus::ResourceCap,
                        "strict policy discovery reached "
                        "max_estimated_memory_bytes",
                        "max_estimated_memory_bytes");
                }
            };
        const auto finalize =
            [&](ProductionPolicyOracle& oracle,
                ExactPolicyRun policy) {
                const bool policy_complete =
                    policy.complete;
                certificate.exact_root_key =
                    oracle.root_key();
                certificate.refinement =
                    std::move(policy.refinement);
                certificate.class_evaluation =
                    std::move(policy.evaluation);
                std::vector<StableKey>{}.swap(policy.roots);
                policy.value_by_exact.clear();
                certificate.policy_changed =
                    oracle.final_policy_changed(
                        certificate.refinement);
                if (!policy_complete ||
                    certificate.class_evaluation
                            .start_values.size() != 1) {
                    return;
                }
                for (const StateClassAssignment& assignment :
                     certificate.refinement.assignments) {
                    if (assignment_exact_state(
                            certificate.refinement, assignment) ==
                        certificate.exact_root_key) {
                        certificate.root_refinement_class =
                            assignment.class_id;
                        break;
                    }
                }
                if (certificate.root_refinement_class ==
                    kNoId) {
                    throw AdapterFailure(
                        PolicyExactLiftStatus::
                            RefinementFailure,
                        "refinement result omitted the exact root "
                        "assignment");
                }
                certificate.exact_start_cost =
                    certificate.class_evaluation
                        .start_values.front();
                certificate.coarse_value_reconciled =
                    reconciled(
                        certificate.exact_start_cost,
                        certificate.solver_cost,
                        certificate.absolute_cost_delta,
                        certificate.relative_cost_delta);
                certificate.compiled =
                    oracle.assert_lifted_policy(
                        strategy_name,
                        certificate.refinement,
                        certificate.class_evaluation);
            };

        ReoptimizationSeed reoptimization_seed;
        if (solved.diagnostics.policy_refinement.triggers != 0) {
            reoptimization_seed.coarse_parents.insert(
                solved.diagnostics.policy_refinement
                    .trigger_coarse_states.begin(),
                solved.diagnostics.policy_refinement
                    .trigger_coarse_states.end());
            if (reoptimization_seed.coarse_parents.empty() &&
                solved.diagnostics.policy_compatibility_state !=
                    kNoId) {
                reoptimization_seed.coarse_parents.insert(
                    solved.diagnostics.policy_compatibility_state);
            }
        }
        ExactPolicyRun lift_policy;
        if (reoptimization_seed.empty()) {
            /*
             * Pass one is the minimum selected-policy carrier. It cannot
             * grow support for an unused alternative merely because that
             * alternative was admitted by the broad solver.
             */
            ProductionPolicyOracle oracle(
                coarse, solved, exact_start, prices,
                options, limits, lift_telemetry, nullptr);
            check_initial_memory(oracle, limits);
            lift_policy = oracle.evaluate_fixed_policy();
            reoptimization_seed =
                oracle.reoptimization_seed(lift_policy);
            const std::uint64_t spent_reforge = std::min(
                lift_telemetry.strict_reforge_logical_work_v1,
                options.max_reforge_work);
            const std::uint64_t remaining_reforge =
                options.max_reforge_work - spent_reforge;
            const bool optional_rebuild_unbudgeted =
                lift_policy.complete &&
                !reoptimization_seed.empty() &&
                spent_reforge > remaining_reforge;
            if (reoptimization_seed.empty() ||
                optional_rebuild_unbudgeted) {
                /*
                 * A complete fixed policy is already a valid bounded
                 * incumbent. Do not spend a smaller residual budget on a
                 * second adapter whose only purpose is optional improvement;
                 * retain the two-pass path whenever at least half of the
                 * declared reforge work remains.
                 */
                finalize(
                    oracle, std::move(lift_policy));
                reoptimization_seed = {};
            }
        }
        while (!reoptimization_seed.empty() &&
               certificate.compiled.status ==
                   CompiledPolicyAssertionStatus::NotRun) {
            /*
             * The first adapter is gone before widening the strict child.
             * Charge completed kernel/transition/reforge/round work to every
             * witness-driven rebuild so no declared cap can reset.
             */
            RefinementLimits reoptimization_limits =
                limits;
            const std::uint64_t spent_kernels =
                static_cast<std::uint64_t>(
                    lift_telemetry.strict_kernels_built) +
                reoptimization_telemetry.strict_kernels_built;
            if (spent_kernels >=
                reoptimization_limits.max_exact_kernels) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::ResourceCap,
                    "witness-driven local exact refinement reached "
                    "max_exact_kernels",
                    "max_exact_kernels");
            }
            reoptimization_limits.max_exact_kernels -=
                static_cast<std::uint32_t>(spent_kernels);
            std::uint64_t spent_transitions =
                lift_telemetry.strict_transitions_built;
            saturating_add(
                spent_transitions,
                reoptimization_telemetry.strict_transitions_built);
            if (spent_transitions >=
                reoptimization_limits.max_transitions) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::ResourceCap,
                    "witness-driven local exact refinement reached "
                    "max_transitions",
                    "max_transitions");
            }
            reoptimization_limits.max_transitions -=
                spent_transitions;
            std::uint64_t spent_rounds =
                lift_telemetry.backward_observation_rounds;
            saturating_add(
                spent_rounds,
                lift_telemetry.exact_fixed_point_rounds);
            saturating_add(
                spent_rounds,
                lift_telemetry.local_reoptimization_rounds);
            saturating_add(
                spent_rounds,
                reoptimization_telemetry
                    .backward_observation_rounds);
            saturating_add(
                spent_rounds,
                reoptimization_telemetry
                    .exact_fixed_point_rounds);
            saturating_add(
                spent_rounds,
                reoptimization_telemetry
                    .local_reoptimization_rounds);
            if (spent_rounds >=
                reoptimization_limits
                    .max_refinement_rounds) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::ResourceCap,
                    "witness-driven local exact refinement reached "
                    "max_refinement_rounds",
                    "max_refinement_rounds");
            }
            reoptimization_limits.max_refinement_rounds -=
                static_cast<std::uint32_t>(spent_rounds);
            SolveOptions reoptimization_options = options;
            std::uint64_t spent_reforge =
                lift_telemetry.strict_reforge_logical_work_v1;
            saturating_add(
                spent_reforge,
                reoptimization_telemetry.strict_reforge_logical_work_v1);
            if (spent_reforge >=
                reoptimization_options.max_reforge_work) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::ResourceCap,
                    "witness-driven local exact refinement reached "
                    "max_reforge_work",
                    "max_reforge_work");
            }
            reoptimization_options.max_reforge_work -=
                spent_reforge;
            lift_policy = ExactPolicyRun{};

            PolicyLiftAdapterTelemetry pass_telemetry;
            ReoptimizationSeed discovered_seed;
            bool rebuild = false;
            try {
                ProductionPolicyOracle oracle(
                    coarse, solved, exact_start, prices,
                    reoptimization_options,
                    reoptimization_limits,
                    pass_telemetry,
                    &reoptimization_seed);
                check_initial_memory(
                    oracle, reoptimization_limits);
                ExactPolicyRun policy =
                    oracle.repair_invalid_policy();
                if (policy.complete) {
                    policy = oracle.improve_policy(
                        std::move(policy));
                }
                discovered_seed =
                    oracle.reoptimization_seed(policy);
                rebuild =
                    oracle.requires_operator_vocabulary_widening(
                        policy);
                if (!rebuild) {
                    finalize(oracle, std::move(policy));
                    reoptimization_seed = {};
                }
            } catch (...) {
                merge_adapter_telemetry(
                    reoptimization_telemetry,
                    pass_telemetry);
                throw;
            }
            merge_adapter_telemetry(
                reoptimization_telemetry,
                pass_telemetry);
            if (rebuild) {
                const std::size_t before =
                    reoptimization_seed.coarse_parents.size();
                reoptimization_seed.merge(discovered_seed);
                if (reoptimization_seed.coarse_parents.size() ==
                    before) {
                    throw AdapterFailure(
                        PolicyExactLiftStatus::
                            LocalReoptimizationRequired,
                        "local exact refinement requested the same "
                        "operator vocabulary twice without progress");
                }
            }
        }
        capture_adapter_telemetry();
    } catch (const AdapterFailure& error) {
        capture_adapter_telemetry();
        certificate.status = error.status;
        certificate.failure_reason = error.what();
        certificate.resource_cap = error.cap;
        return certificate;
    } catch (const SolverResourceLimit& error) {
        capture_adapter_telemetry();
        certificate.status = PolicyExactLiftStatus::ResourceCap;
        certificate.failure_reason = error.what();
        certificate.resource_cap =
            adapter_resource_cap_name(error.cap_name());
        return certificate;
    } catch (const std::length_error& error) {
        capture_adapter_telemetry();
        certificate.status = PolicyExactLiftStatus::ResourceCap;
        certificate.failure_reason = error.what();
        certificate.resource_cap =
            resource_cap_from_message(error.what());
        return certificate;
    } catch (const std::exception& error) {
        capture_adapter_telemetry();
        certificate.status =
            PolicyExactLiftStatus::RefinementFailure;
        certificate.failure_reason = error.what();
        return certificate;
    }

    if (certificate.refinement.status !=
            RefinementStatus::Complete ||
        !certificate.refinement.executable ||
        !certificate.refinement.lumpable) {
        certificate.status =
            certificate.refinement.status ==
                    RefinementStatus::ResourceCap ||
                    certificate.refinement.status ==
                        RefinementStatus::RefinementRoundCap
                ? PolicyExactLiftStatus::ResourceCap
                : PolicyExactLiftStatus::RefinementFailure;
        certificate.failure_reason =
            certificate.refinement.failure_reason;
        certificate.resource_cap =
            certificate.refinement.resource_cap;
        return certificate;
    }
    certificate.lumpable = true;
    if (certificate.class_evaluation.status !=
            PolicyEvaluationStatus::Complete ||
        !certificate.class_evaluation.converged ||
        !certificate.class_evaluation.proper ||
        certificate.class_evaluation.start_values.size() != 1) {
        certificate.status =
            certificate.class_evaluation.status ==
                    PolicyEvaluationStatus::ResourceCap
                ? PolicyExactLiftStatus::ResourceCap
                : PolicyExactLiftStatus::RefinementFailure;
        certificate.failure_reason =
            certificate.class_evaluation.failure_reason.empty()
                ? "refined class policy did not produce one proper "
                  "root value"
                : certificate.class_evaluation.failure_reason;
        certificate.resource_cap =
            certificate.class_evaluation.resource_cap;
        return certificate;
    }

    if (certificate.compiled.status ==
            CompiledPolicyAssertionStatus::Complete &&
        certificate.compiled.paired_default_only &&
        certificate.compiled.executable &&
        certificate.compiled.proper &&
        certificate.compiled.zero_off_policy &&
        certificate.compiled.evaluation.cost_complete &&
        std::isfinite(certificate.compiled.exact_cost) &&
        certificate.compiled.exact_cost >= 0.0) {
        certificate.status = PolicyExactLiftStatus::Complete;
        certificate.executable = true;
        return certificate;
    }

    certificate.failure_reason =
        certificate.compiled.failure_reason;
    certificate.resource_cap =
        certificate.compiled.resource_cap;
    if (certificate.compiled.status ==
               CompiledPolicyAssertionStatus::ResourceCap) {
        certificate.status = PolicyExactLiftStatus::ResourceCap;
    } else {
        certificate.status =
            PolicyExactLiftStatus::CompiledAssertionFailure;
    }
    return certificate;
}

const char* exact_boundary_recovery_status_name(
        const ExactBoundaryRecoveryStatus status) {
    switch (status) {
    case ExactBoundaryRecoveryStatus::NotRun: return "not_run";
    case ExactBoundaryRecoveryStatus::Complete: return "complete";
    case ExactBoundaryRecoveryStatus::InvalidPrefix:
        return "invalid_prefix";
    case ExactBoundaryRecoveryStatus::UnsupportedKernel:
        return "unsupported_kernel";
    case ExactBoundaryRecoveryStatus::ResourceCap:
        return "resource_cap";
    case ExactBoundaryRecoveryStatus::WallTimeCap:
        return "wall_time_cap";
    case ExactBoundaryRecoveryStatus::ImproperPrefix:
        return "improper_prefix";
    case ExactBoundaryRecoveryStatus::NoRequestedEntry:
        return "no_requested_entry";
    }
    return "unknown";
}

ExactBoundaryClosureResult analyze_exact_boundary_closure(
        const std::vector<ExactBoundaryClosureNode>& nodes,
        const std::uint32_t requested_entry) {
    ExactBoundaryClosureResult result;
    result.complete_support = true;
    std::vector<std::vector<std::uint32_t>> predecessors(nodes.size());
    std::vector<std::uint8_t> reaches_stop(nodes.size(), 0);
    std::vector<std::uint32_t> pending;
    for (std::uint32_t source = 0; source < nodes.size(); ++source) {
        const ExactBoundaryClosureNode& node = nodes[source];
        if (node.state.terminal) {
            reaches_stop[source] = 1;
            pending.push_back(source);
            if (!node.state.goal &&
                node.state.coarse_state == requested_entry) {
                result.requested_nodes.push_back(source);
            }
        }
        for (const std::uint32_t successor : node.successors) {
            if (successor >= nodes.size()) {
                result.status = ExactBoundaryRecoveryStatus::InvalidPrefix;
                result.refusal =
                    "selected exact prefix names an unknown successor";
                return result;
            }
            predecessors[successor].push_back(source);
        }
    }
    for (std::size_t position = 0; position < pending.size(); ++position) {
        for (const std::uint32_t predecessor :
             predecessors[pending[position]]) {
            if (!reaches_stop[predecessor]) {
                reaches_stop[predecessor] = 1;
                pending.push_back(predecessor);
            }
        }
    }
    if (std::find(reaches_stop.begin(), reaches_stop.end(), 0) !=
        reaches_stop.end()) {
        result.status = ExactBoundaryRecoveryStatus::ImproperPrefix;
        result.refusal =
            "selected exact prefix contains a closed nonterminal class";
        return result;
    }
    result.absorption_proved = true;
    if (result.requested_nodes.empty()) {
        result.status = ExactBoundaryRecoveryStatus::NoRequestedEntry;
        result.refusal =
            "selected exact prefix does not reach the requested entry";
        return result;
    }
    result.status = ExactBoundaryRecoveryStatus::Complete;
    return result;
}

struct ExactBoundaryRecoveryWork::Impl {
    RefinementLimits limits;
    std::uint64_t max_work = 0;
    std::uint64_t max_wall_time_ms = 0;
    std::uint32_t requested_entry = kNoId;
    ExactBoundaryRecoveryResult result;
    std::unique_ptr<ProductionPolicyOracle> oracle;
    std::optional<solve_detail::CooperativeTask<bool>> initialization;
    std::optional<solve_detail::CooperativeTask<QuotientOracleCompactRow>>
        row_work;
    std::vector<ExactBoundaryClosureNode> nodes;
    std::map<StableKey, std::uint32_t> node_by_key;
    std::size_t cursor = 0;
    std::uint32_t active_node = kNoId;
    bool finished = false;
    std::chrono::steady_clock::time_point started_at =
        std::chrono::steady_clock::now();

    Impl(
            CalcContext& coarse,
            const SolveResult& selected_prefix,
            const pc_item_state& exact_start,
            const std::unordered_map<std::string, double>& prices,
            const SolveOptions& options,
            std::set<std::uint32_t> observation_stops,
            const std::uint32_t requested_entry_value,
            RefinementLimits limits_value,
            const std::uint64_t max_work_value,
            const std::uint64_t max_wall_time_ms_value)
        : limits(std::move(limits_value)),
          max_work(max_work_value),
          max_wall_time_ms(max_wall_time_ms_value),
          requested_entry(requested_entry_value) {
        oracle = std::make_unique<ProductionPolicyOracle>(
            coarse, selected_prefix, exact_start, prices, options, limits,
            result.adapter, nullptr, true, true, false,
            std::move(observation_stops));
        initialization.emplace(oracle->initialize_cooperatively());
    }

    std::uint64_t elapsed_ms() const {
        return static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - started_at).count());
    }

    std::uint64_t owned_bytes() const {
        std::uint64_t bytes = sizeof(*this);
        bytes += nodes.capacity() * sizeof(ExactBoundaryClosureNode);
        for (const ExactBoundaryClosureNode& node : nodes) {
            bytes += exact_state_bytes(node.state);
            bytes += node.successors.capacity() * sizeof(std::uint32_t);
        }
        bytes += node_by_key.size() *
            (sizeof(std::pair<const StableKey, std::uint32_t>) +
             3 * sizeof(void*));
        for (const auto& [key, unused] : node_by_key) {
            (void)unused;
            bytes += key.capacity() * sizeof(std::uint64_t);
        }
        bytes += result.requested_entries.capacity() *
            sizeof(ExactBoundaryRecoveredMember);
        for (const ExactBoundaryRecoveredMember& member :
             result.requested_entries) {
            bytes += member.stable_key.capacity() * sizeof(std::uint64_t);
        }
        if (oracle != nullptr) {
            saturating_add(
                bytes, oracle->estimated_owned_bytes());
        }
        if (initialization.has_value()) {
            saturating_add(
                bytes, initialization->retained_bytes());
        }
        if (row_work.has_value()) {
            saturating_add(
                bytes, row_work->retained_bytes());
        }
        return bytes;
    }

    void refuse(
            const ExactBoundaryRecoveryStatus status,
            std::string reason) {
        result.status = status;
        result.refusal = std::move(reason);
        result.wall_time_ms = elapsed_ms();
        result.retained_owned_bytes = owned_bytes();
        result.peak_owned_bytes = std::max(
            result.peak_owned_bytes, result.retained_owned_bytes);
        row_work.reset();
        initialization.reset();
        oracle.reset();
        finished = true;
    }

    std::uint32_t intern(const std::uint32_t locator) {
        ExactState state = oracle->quotient_materialize_locator(locator);
        const auto found = node_by_key.find(state.stable_key);
        if (found != node_by_key.end()) {
            if (nodes.at(found->second).state != state) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::RefinementFailure,
                    "one exact boundary identity changed its hard payload");
            }
            return found->second;
        }
        if (nodes.size() >= limits.max_exact_states) {
            throw AdapterFailure(
                PolicyExactLiftStatus::ResourceCap,
                "exact boundary replay reached max_exact_states",
                "max_exact_states");
        }
        const std::uint32_t ordinal =
            static_cast<std::uint32_t>(nodes.size());
        node_by_key.emplace(state.stable_key, ordinal);
        nodes.push_back({std::move(state), locator, {}});
        result.exact_states = static_cast<std::uint32_t>(nodes.size());
        return ordinal;
    }

    void finish_graph() {
        result.exact_states = static_cast<std::uint32_t>(nodes.size());
        const ExactBoundaryClosureResult closure =
            analyze_exact_boundary_closure(nodes, requested_entry);
        result.complete_support = closure.complete_support;
        result.absorption_proved = closure.absorption_proved;
        if (closure.status != ExactBoundaryRecoveryStatus::Complete) {
            refuse(closure.status, closure.refusal);
            return;
        }
        for (const std::uint32_t ordinal : closure.requested_nodes) {
            const ExactBoundaryClosureNode& node = nodes.at(ordinal);
            pc_item_state item{};
            if (!oracle->quotient_export_item(node.locator, item)) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::ObservationUnavailable,
                    "exact boundary member cannot export its hard item");
            }
            result.requested_entries.push_back({
                node.state.stable_key,
                node.state.coarse_state,
                item});
        }
        std::sort(
            result.requested_entries.begin(),
            result.requested_entries.end(),
            [](const ExactBoundaryRecoveredMember& left,
               const ExactBoundaryRecoveredMember& right) {
                return left.stable_key < right.stable_key;
            });
        result.requested_entries.erase(
            std::unique(
                result.requested_entries.begin(),
                result.requested_entries.end(),
                [](const ExactBoundaryRecoveredMember& left,
                   const ExactBoundaryRecoveredMember& right) {
                    return left.stable_key == right.stable_key;
                }),
            result.requested_entries.end());
        std::uint64_t identity = 1469598103934665603ULL;
        const auto mix = [&](const std::uint64_t word) {
            identity ^= word;
            identity *= 1099511628211ULL;
        };
        mix(1); /* exact-member identity schema */
        mix(requested_entry);
        mix(result.requested_entries.size());
        for (const ExactBoundaryRecoveredMember& member :
             result.requested_entries) {
            mix(member.coarse_state);
            mix(member.stable_key.size());
            for (const std::uint64_t word : member.stable_key) mix(word);
        }
        result.member_identity = identity;
        result.status = ExactBoundaryRecoveryStatus::Complete;
        result.wall_time_ms = elapsed_ms();
        result.retained_owned_bytes = owned_bytes();
        result.peak_owned_bytes = std::max(
            result.peak_owned_bytes, result.retained_owned_bytes);
        row_work.reset();
        initialization.reset();
        oracle.reset();
        finished = true;
    }

    void advance_one() {
        if (finished) return;
        if (elapsed_ms() > max_wall_time_ms) {
            refuse(
                ExactBoundaryRecoveryStatus::WallTimeCap,
                "max_wall_time_ms");
            return;
        }
        if (++result.work_items > max_work) {
            refuse(
                ExactBoundaryRecoveryStatus::ResourceCap,
                "max_exact_work");
            return;
        }
        try {
            if (initialization.has_value()) {
                if (!initialization->resume()) return;
                (void)initialization->take_result();
                initialization.reset();
                const std::uint32_t root =
                    oracle->quotient_locator(oracle->root_key());
                intern(root);
                return;
            }
            if (row_work.has_value()) {
                if (!row_work->resume()) return;
                QuotientOracleCompactRow row = row_work->take_result();
                row_work.reset();
                ++result.exact_rows;
                if (result.exact_rows > limits.max_exact_kernels) {
                    throw AdapterFailure(
                        PolicyExactLiftStatus::ResourceCap,
                        "exact boundary replay reached max_exact_kernels",
                        "max_exact_kernels");
                }
                std::map<std::uint32_t, solve_detail::WideFloat> mass;
                for (const QuotientOracleCompactTransition& transition :
                     row.transitions) {
                    if (!std::isfinite(transition.probability) ||
                        transition.probability < 0.0) {
                        throw AdapterFailure(
                            PolicyExactLiftStatus::RefinementFailure,
                            "exact boundary row has invalid probability");
                    }
                    if (transition.probability == 0.0) continue;
                    const std::uint32_t canonical =
                        oracle->quotient_canonical_locator(
                            transition.strict_state);
                    const std::uint32_t successor = intern(canonical);
                    mass[successor] += solve_detail::WideFloat{
                        transition.probability};
                }
                solve_detail::WideFloat total{0.0};
                ExactBoundaryClosureNode& node = nodes.at(active_node);
                for (const auto& [successor, probability] : mass) {
                    node.successors.push_back(successor);
                    total += probability;
                }
                result.exact_transitions += mass.size();
                if (result.exact_transitions > limits.max_transitions) {
                    throw AdapterFailure(
                        PolicyExactLiftStatus::ResourceCap,
                        "exact boundary replay reached max_transitions",
                        "max_transitions");
                }
                if (mass.empty() ||
                    std::fabs(total.value() - 1.0) >
                        limits.probability_sum_tolerance) {
                    throw AdapterFailure(
                        PolicyExactLiftStatus::RefinementFailure,
                        "exact boundary selected row is not stochastic");
                }
                ++cursor;
                active_node = kNoId;
                return;
            }
            if (cursor >= nodes.size()) {
                finish_graph();
                return;
            }
            ExactBoundaryClosureNode& node = nodes[cursor];
            if (node.state.terminal) {
                ++cursor;
                return;
            }
            active_node = static_cast<std::uint32_t>(cursor);
            row_work.emplace(
                oracle->boundary_selected_compact_row_cooperatively(
                    node.state));
        } catch (const AdapterFailure& error) {
            const ExactBoundaryRecoveryStatus status =
                error.status == PolicyExactLiftStatus::ResourceCap
                    ? ExactBoundaryRecoveryStatus::ResourceCap
                    : error.status ==
                              PolicyExactLiftStatus::UnsupportedPrimitiveKernel
                        ? ExactBoundaryRecoveryStatus::UnsupportedKernel
                        : ExactBoundaryRecoveryStatus::InvalidPrefix;
            refuse(status, error.cap.empty() ? error.what() : error.cap);
        } catch (const SolverResourceLimit& error) {
            refuse(
                ExactBoundaryRecoveryStatus::ResourceCap,
                adapter_resource_cap_name(error.cap_name()));
        } catch (const std::length_error& error) {
            refuse(
                ExactBoundaryRecoveryStatus::ResourceCap,
                resource_cap_from_message(error.what()));
        } catch (const std::exception& error) {
            refuse(
                ExactBoundaryRecoveryStatus::InvalidPrefix,
                error.what());
        }
        if (!finished) {
            const std::uint64_t live = owned_bytes();
            result.peak_owned_bytes = std::max(
                result.peak_owned_bytes, live);
            if (live > limits.max_estimated_memory_bytes) {
                refuse(
                    ExactBoundaryRecoveryStatus::ResourceCap,
                    "max_owned_bytes");
            }
        }
    }
};

ExactBoundaryRecoveryWork::ExactBoundaryRecoveryWork(
        CalcContext& coarse,
        const SolveResult& selected_prefix,
        const pc_item_state& exact_start,
        const std::unordered_map<std::string, double>& prices,
        const SolveOptions& options,
        std::set<std::uint32_t> observation_stops,
        const std::uint32_t requested_entry,
        RefinementLimits limits,
        const std::uint64_t max_work,
        const std::uint64_t max_wall_time_ms)
    : impl_(std::make_unique<Impl>(
          coarse, selected_prefix, exact_start, prices, options,
          std::move(observation_stops), requested_entry,
          std::move(limits), max_work, max_wall_time_ms)) {}

ExactBoundaryRecoveryWork::~ExactBoundaryRecoveryWork() = default;
ExactBoundaryRecoveryWork::ExactBoundaryRecoveryWork(
    ExactBoundaryRecoveryWork&&) noexcept = default;
ExactBoundaryRecoveryWork& ExactBoundaryRecoveryWork::operator=(
    ExactBoundaryRecoveryWork&&) noexcept = default;

void ExactBoundaryRecoveryWork::step(const std::uint32_t max_work_items) {
    if (max_work_items == 0) return;
    for (std::uint32_t item = 0;
         item < max_work_items && !impl_->finished; ++item) {
        impl_->advance_one();
        if (!impl_->finished) {
            const std::uint64_t live = impl_->owned_bytes();
            impl_->result.peak_owned_bytes = std::max(
                impl_->result.peak_owned_bytes, live);
            if (live > impl_->limits.max_estimated_memory_bytes) {
                impl_->refuse(
                    ExactBoundaryRecoveryStatus::ResourceCap,
                    "max_owned_bytes");
            }
        }
    }
}

bool ExactBoundaryRecoveryWork::done() const {
    return impl_->finished;
}

std::uint64_t ExactBoundaryRecoveryWork::retained_bytes() const {
    return impl_->owned_bytes();
}

ExactBoundaryRecoveryResult ExactBoundaryRecoveryWork::take_result() {
    if (!impl_->finished) {
        throw std::logic_error(
            "exact boundary recovery result requested before completion");
    }
    ExactBoundaryRecoveryResult result = std::move(impl_->result);
    impl_.reset();
    return result;
}

} // namespace refinement
} // namespace solver
} // namespace poecraft
