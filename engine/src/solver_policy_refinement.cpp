#include "solver_policy_refinement_helpers.hpp"
#include "solver_quotient_bellman.hpp"

namespace poecraft {
namespace solver {
namespace refinement {

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

struct QuotientAlternativeDescriptor {
    std::uint32_t operator_index = kNoId;
    quotient::AlternativeActionIdentity action;
    ObservationRequirement routing_observes;
    StableKey resumable_work_identity;

    bool operator==(const QuotientAlternativeDescriptor&) const = default;
};

std::uint64_t quotient_alternative_descriptor_bytes(
        const QuotientAlternativeDescriptor& descriptor) {
    std::uint64_t bytes = 0;
    saturating_add(
        bytes,
        stable_key_bytes(descriptor.action.semantic_action_identity));
    saturating_add(
        bytes,
        stable_key_bytes(
            descriptor.action.runtime_contract_program_identity));
    saturating_add(
        bytes,
        stable_key_bytes(descriptor.action.exact_choice_recipe_identity));
    saturating_add(
        bytes, requirement_bytes(descriptor.routing_observes));
    saturating_add(
        bytes, stable_key_bytes(descriptor.resumable_work_identity));
    return bytes;
}

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
            std::map<std::uint32_t, const quotient::QuotientCell*> cell_by_id;
            for (const quotient::QuotientCell& cell : state.cells) {
                cell_by_id.emplace(cell.cell_id, &cell);
            }
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
                        const quotient::QuotientCell* target_cell =
                            cell_by_id.at(target);
                        input.transitions.push_back({{}, target, probability.value()});
                        proof_arcs.push_back({
                            {}, target_cell->semantic_identity,
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
        for (const PublishedRow& row : coarse_rows) {
            publication_by_row.emplace(row.sparse_row, &row);
        }
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
        evaluation_request.start_classes = {
            certificate.root_refinement_class};
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
            !certificate.compiled.executable) {
            throw AdapterFailure(
                certificate.compiled.status ==
                        CompiledPolicyAssertionStatus::ResourceCap
                    ? PolicyExactLiftStatus::ResourceCap
                    : PolicyExactLiftStatus::CompiledAssertionFailure,
                certificate.compiled.failure_reason,
                certificate.compiled.resource_cap);
        }
        double artifact_delta = 0.0;
        double artifact_relative = 0.0;
        if (!reconciled(
                certificate.compiled.exact_cost,
                certificate.exact_start_cost,
                artifact_delta,
                artifact_relative)) {
            throw AdapterFailure(
                PolicyExactLiftStatus::CompiledAssertionFailure,
                "compiled quotient artifact does not reconcile with Bellman value");
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

PolicyExactLiftCertificate lift_policy_quotient(
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
            telemetry, seed.empty() ? nullptr : &seed, true);
        certificate.exact_root_key = oracle.root_key();
        const std::uint32_t root_locator =
            oracle.quotient_locator(certificate.exact_root_key);

        quotient::QuotientBellmanGraph bellman(
            limits.max_estimated_memory_bytes);
        quotient::ProofMemoryLedger& ledger =
            bellman.proof_store()->ledger();

        struct RawRowPayload {
            SelectedAction selected;
            ExactChoiceRecipe choice_recipe;
            double action_cost = 0.0;
            std::vector<QuotientOracleCompactTransition> transitions;
        };
        std::vector<RawRowPayload> raw_rows;
        std::map<StableKey, std::uint32_t> raw_row_by_identity;
        std::vector<std::uint32_t> locators;
        std::vector<std::uint32_t> ordinal_by_strict_state;
        std::vector<std::vector<std::uint32_t>> rows_by_ordinal;
        std::vector<std::vector<QuotientAlternativeDescriptor>>
            alternatives_by_ordinal;

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
            alternatives_by_ordinal.emplace_back();
            return ordinal;
        };
        intern_locator(root_locator);

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

        for (std::size_t cursor = 0; cursor < locators.size(); ++cursor) {
            ExactState state =
                oracle.quotient_materialize_locator(locators[cursor]);
            const std::vector<std::uint32_t> equivalents =
                oracle.quotient_equivalent_locators(state.stable_key);
            if (equivalents.size() != 1 ||
                equivalents.front() != locators[cursor]) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::RefinementFailure,
                    "streamed quotient strict identity unexpectedly collapsed");
            }
            if (!state.terminal) {
                QuotientOracleCompactRow selected =
                    oracle.quotient_selected_compact_row(state);
                alternatives_by_ordinal[cursor] =
                    oracle.quotient_alternative_descriptors(state);
                std::uint64_t discovery_live = exact_state_bytes(state);
                saturating_add(
                    discovery_live,
                    sizeof(QuotientOracleCompactRow));
                saturating_add(
                    discovery_live,
                    alternatives_by_ordinal[cursor].capacity() *
                        sizeof(QuotientAlternativeDescriptor));
                for (const QuotientAlternativeDescriptor& descriptor :
                     alternatives_by_ordinal[cursor]) {
                    saturating_add(
                        discovery_live,
                        quotient_alternative_descriptor_bytes(descriptor));
                }
                saturating_add(
                    discovery_live,
                    selected_action_bytes(selected.selected));
                saturating_add(
                    discovery_live,
                    exact_choice_recipe_bytes(selected.choice_recipe));
                saturating_add(
                    discovery_live,
                    selected.transitions.capacity() *
                        sizeof(QuotientOracleCompactTransition));
                telemetry.current_live_slices = 1;
                telemetry.peak_live_slices = 1;
                telemetry.current_live_slice_bytes = discovery_live;
                telemetry.peak_live_slice_bytes = std::max(
                    telemetry.peak_live_slice_bytes, discovery_live);
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
                saturating_add(
                    telemetry.alternative_rows_avoided,
                    alternatives_by_ordinal[cursor].size());
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

            std::uint64_t combined = oracle.estimated_owned_bytes();
            saturating_add(combined, ledger.snapshot().total_bytes);
            if (combined > limits.max_estimated_memory_bytes) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::ResourceCap,
                    "streamed quotient discovery reached memory cap",
                    "max_estimated_memory_bytes");
            }
        }

        std::map<StableKey, std::uint32_t>{}.swap(
            raw_row_by_identity);
        std::uint64_t raw_row_bytes =
            raw_rows.capacity() * sizeof(RawRowPayload) +
            rows_by_ordinal.capacity() *
                sizeof(std::vector<std::uint32_t>) +
            alternatives_by_ordinal.capacity() *
                sizeof(std::vector<QuotientAlternativeDescriptor>);
        for (const RawRowPayload& row : raw_rows) {
            saturating_add(
                raw_row_bytes, selected_action_bytes(row.selected));
            saturating_add(
                raw_row_bytes,
                exact_choice_recipe_bytes(row.choice_recipe));
            saturating_add(
                raw_row_bytes,
                row.transitions.capacity() *
                    sizeof(QuotientOracleCompactTransition));
        }
        for (const RawRowPayload& row : raw_rows) {
            oracle.quotient_install_streamed_recipe(
                row.selected, row.choice_recipe);
        }
        for (const auto& rows : rows_by_ordinal) {
            saturating_add(
                raw_row_bytes,
                rows.capacity() * sizeof(std::uint32_t));
        }
        for (const auto& descriptors : alternatives_by_ordinal) {
            saturating_add(
                raw_row_bytes,
                descriptors.capacity() *
                    sizeof(QuotientAlternativeDescriptor));
            for (const QuotientAlternativeDescriptor& descriptor :
                 descriptors) {
                saturating_add(
                    raw_row_bytes,
                    quotient_alternative_descriptor_bytes(descriptor));
            }
        }
        std::uint64_t locator_bytes =
            locators.capacity() * sizeof(std::uint32_t) +
            ordinal_by_strict_state.capacity() * sizeof(std::uint32_t);
        telemetry.carrier_bytes = locator_bytes;
        bellman.set_external_row_kernel_bytes(raw_row_bytes);
        quotient::ScopedProofMemoryCharge retained_locators(
            ledger, quotient::ProofMemoryCategory::Carrier,
            locator_bytes);

        const StableKey strict_identity{
            0x7063717374726b32ull,
            root_locator,
            locators.size()};
        const StableKey replay_identity{
            0x7063717265706c32ull,
            root_locator,
            locators.size()};
        const StableKey enumeration_identity{
            0x706371656e756d32ull,
            root_locator,
            locators.size()};
        const StableKey range_identity{0x70637172616e6732ull};

        const auto replay_node =
                [&](const bool include_observations,
                    const std::uint32_t ordinal) {
            ExactState state =
                oracle.quotient_materialize_locator(locators.at(ordinal));
            refinement::ClosedPartitionNode node;
            node.stable_key = {
                0x7063716e6f646532ull,
                locators.at(ordinal)};
            node.terminal = state.terminal;
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
                    StableKey label{0x7063716163746e32ull};
                    label.push_back(row.selected.semantic_key.size());
                    label.insert(
                        label.end(), row.selected.semantic_key.begin(),
                        row.selected.semantic_key.end());
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
                            label, successor,
                            transition.probability * row_weight});
                    }
                }
                immediate.push_back(
                    alternatives_by_ordinal.at(ordinal).size());
                const auto append_identity =
                        [&](const StableKey& identity) {
                    immediate.push_back(identity.size());
                    immediate.insert(
                        immediate.end(),
                        identity.begin(), identity.end());
                };
                for (const QuotientAlternativeDescriptor& descriptor :
                     alternatives_by_ordinal.at(ordinal)) {
                    immediate.push_back(descriptor.operator_index);
                    append_identity(
                        descriptor.action.semantic_action_identity);
                    append_identity(
                        descriptor.action
                            .runtime_contract_program_identity);
                    append_identity(
                        descriptor.action.exact_choice_recipe_identity);
                }
            }
            node.immediate_key = std::move(immediate);
            node.observation_key = canonical_observation_identity(
                state.coarse_state_key, requirement, state.features);
            telemetry.peak_live_slice_bytes = std::max(
                telemetry.peak_live_slice_bytes,
                exact_state_bytes(state));
            oracle.quotient_release_carrier(state.stable_key);
            return node;
        };

        std::uint64_t retained_for_partition = oracle.estimated_owned_bytes();
        saturating_add(
            retained_for_partition, ledger.snapshot().total_bytes);
        refinement::ClosedPartitionLimits partition_limits{
            limits.max_refinement_classes,
            limits.max_refinement_rounds,
            retained_for_partition,
            limits.max_estimated_memory_bytes,
            limits.probability_sum_tolerance};
        refinement::ClosedPartitionResult observation_coarse =
            refinement::refine_closed_probabilistic_partition_replay(
                static_cast<std::uint32_t>(locators.size()),
                [&](const std::uint32_t ordinal) {
                    return replay_node(false, ordinal);
                },
                {}, false, partition_limits);
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
        }
        const std::uint64_t coarse_partition_owned =
            observation_coarse.estimated_memory_bytes >
                    retained_for_partition
                ? observation_coarse.estimated_memory_bytes -
                      retained_for_partition
                : 0;
        quotient::ScopedProofMemoryCharge retained_coarse_partition(
            ledger, quotient::ProofMemoryCategory::Partition,
            coarse_partition_owned);

        partition_limits.retained_estimated_memory_bytes =
            oracle.estimated_owned_bytes();
        saturating_add(
            partition_limits.retained_estimated_memory_bytes,
            ledger.snapshot().total_bytes);
        const std::uint64_t final_partition_retained =
            partition_limits.retained_estimated_memory_bytes;
        refinement::ClosedPartitionResult partition =
            refinement::refine_closed_probabilistic_partition_replay(
                static_cast<std::uint32_t>(locators.size()),
                [&](const std::uint32_t ordinal) {
                    return replay_node(true, ordinal);
                },
                observation_coarse.class_by_node,
                false, partition_limits);
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
                    const std::vector<std::uint64_t>& generation_by_class) {
            std::vector<quotient::QuotientCell> cells(shared.classes.size());
            for (const refinement::ClosedPartitionClass& cls :
                 shared.classes) {
                const std::uint32_t representative = representative_for(
                    shared.class_by_node, cls.class_id);
                ExactState state = oracle.quotient_materialize_locator(
                    locators[representative]);
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
                }
                cell.observed_features = observe_features(
                    state.features, cell.observation_requirement);
                cell.immediate_identity = cls.immediate_key;
                cell.coverage = class_coverage(
                    shared.class_by_node, cls.class_id);
                cell.semantic_identity =
                    quotient::canonical_quotient_cell_identity(cell);
                oracle.quotient_release_carrier(state.stable_key);
                cells[cls.class_id] = std::move(cell);
            }
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
            }
            return cells;
        };

        std::vector<std::uint32_t> coarse_cell_id(
            observation_coarse.final_class_count);
        std::iota(coarse_cell_id.begin(), coarse_cell_id.end(), 0u);
        std::vector<std::uint64_t> coarse_generation(
            observation_coarse.final_class_count, 1);
        std::vector<quotient::QuotientCell> coarse_cells = build_cells(
            observation_coarse, false,
            coarse_cell_id, coarse_generation);

        std::vector<std::set<std::uint32_t>> children(
            observation_coarse.final_class_count);
        for (std::uint32_t ordinal = 0;
             ordinal < locators.size(); ++ordinal) {
            children[observation_coarse.class_by_node[ordinal]].insert(
                partition.class_by_node[ordinal]);
        }
        std::uint32_t next_cell_id =
            observation_coarse.final_class_count;
        std::vector<std::uint32_t> final_cell_id(
            partition.final_class_count, kNoId);
        std::vector<std::uint64_t> final_generation(
            partition.final_class_count, 2);
        for (std::uint32_t old = 0; old < children.size(); ++old) {
            if (children[old].size() == 1) {
                final_cell_id[*children[old].begin()] = old;
            } else {
                for (const std::uint32_t child : children[old]) {
                    final_cell_id[child] = next_cell_id++;
                }
            }
        }
        std::vector<quotient::QuotientCell> final_cells = build_cells(
            partition, true, final_cell_id, final_generation);
        for (std::uint32_t cls = 0; cls < final_cells.size(); ++cls) {
            const std::uint32_t old =
                observation_coarse.class_by_node[
                    representative_for(partition.class_by_node, cls)];
            if (children[old].size() == 1 &&
                final_cells[cls].semantic_identity ==
                    coarse_cells[old].semantic_identity) {
                final_cells[cls].generation =
                    coarse_cells[old].generation;
            }
        }

        const auto install_cells =
                [&](const std::vector<quotient::QuotientCell>& cells) {
            std::vector<quotient::QuotientBellmanCellInput> inputs;
            inputs.reserve(cells.size());
            for (const quotient::QuotientCell& cell : cells) {
                inputs.push_back({
                    cell.cell_id, cell.generation,
                    cell.semantic_identity, cell.terminal});
            }
            bellman.install_cells(std::move(inputs));
        };
        struct PublishedRow {
            std::uint64_t sparse_row = 0;
            std::uint32_t source_cell_id = 0;
            SelectedAction selected;
        };
        const auto publish_rows =
                [&](const refinement::ClosedPartitionResult& shared,
                    const std::vector<quotient::QuotientCell>& cells,
                    const std::vector<std::uint32_t>& cell_id_by_class) {
            std::map<std::uint32_t, const quotient::QuotientCell*> by_id;
            for (const quotient::QuotientCell& cell : cells) {
                by_id.emplace(cell.cell_id, &cell);
            }
            std::vector<PublishedRow> published;
            for (const quotient::QuotientCell& cell : cells) {
                if (cell.terminal) continue;
                const std::uint32_t class_id = static_cast<std::uint32_t>(
                    &cell - cells.data());
                const std::uint32_t representative = representative_for(
                    shared.class_by_node, class_id);
                ExactState source = oracle.quotient_materialize_locator(
                    locators[representative]);
                for (const std::uint32_t payload_id :
                     rows_by_ordinal[representative]) {
                    const RawRowPayload& row = raw_rows[payload_id];
                    std::map<std::uint32_t, solve_detail::WideFloat> projected;
                    for (const QuotientOracleCompactTransition& transition :
                         row.transitions) {
                        const std::uint32_t ordinal =
                            ordinal_by_strict_state[transition.strict_state];
                        projected[cell_id_by_class[
                            shared.class_by_node[ordinal]]] +=
                                solve_detail::WideFloat{
                                    transition.probability};
                    }
                    quotient::QuotientBellmanRowInput input;
                    input.source_cell_id = cell.cell_id;
                    input.operator_index = row.selected.action_id;
                    input.cost = row.action_cost;
                    input.certified = true;
                    std::vector<quotient::ProofProjectedArc> arcs;
                    solve_detail::WideFloat total{0.0};
                    for (const auto& [target, probability] : projected) {
                        input.transitions.push_back({
                            {}, target, probability.value()});
                        arcs.push_back({
                            {}, by_id.at(target)->semantic_identity,
                            probability.value()});
                        total += solve_detail::WideFloat{
                            probability.value()};
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
                        sparse_row, cell.cell_id, row.selected});
                }
                oracle.quotient_release_carrier(source.stable_key);
            }
            return published;
        };

        install_cells(coarse_cells);
        const std::vector<PublishedRow> coarse_rows = publish_rows(
            observation_coarse, coarse_cells, coarse_cell_id);
        for (std::uint32_t old = 0; old < children.size(); ++old) {
            if (children[old].size() > 1) {
                bellman.invalidate_source_split(old, 2);
                bellman.invalidate_target_split(old, 2);
            } else {
                const std::uint32_t child = *children[old].begin();
                if (final_cells[child].generation >
                    coarse_cells[old].generation) {
                    bellman.supersede_cell({
                        final_cells[child].cell_id,
                        final_cells[child].generation,
                        final_cells[child].semantic_identity,
                        final_cells[child].terminal});
                }
            }
        }
        install_cells(final_cells);
        retained_coarse_partition.reset();
        std::vector<PublishedRow> published_rows = publish_rows(
            partition, final_cells, final_cell_id);
        const auto refresh_external_row_kernel_bytes = [&] {
            std::uint64_t retained = raw_row_bytes;
            saturating_add(
                retained,
                coarse_rows.capacity() * sizeof(PublishedRow));
            saturating_add(
                retained,
                published_rows.capacity() * sizeof(PublishedRow));
            for (const PublishedRow& row : coarse_rows) {
                saturating_add(
                    retained, selected_action_bytes(row.selected));
            }
            for (const PublishedRow& row : published_rows) {
                saturating_add(
                    retained, selected_action_bytes(row.selected));
            }
            bellman.set_external_row_kernel_bytes(retained);
        };
        refresh_external_row_kernel_bytes();

        const StableKey price_identity =
            oracle.quotient_price_identity();
        const StableKey vocabulary_identity =
            oracle.quotient_vocabulary_identity();
        std::vector<quotient::AccountedAlternativeAction>
            admitted_action_accounting;
        std::vector<quotient::AccountedAlternativeAction>
            completed_action_accounting;
        for (std::uint32_t cls = 0; cls < final_cells.size(); ++cls) {
            const quotient::QuotientCell& cell = final_cells[cls];
            if (cell.terminal) continue;
            const std::uint32_t representative = representative_for(
                partition.class_by_node, cls);
            const auto& descriptors =
                alternatives_by_ordinal.at(representative);
            for (const quotient::CoverageRange& range :
                 cell.coverage.ranges) {
                for (std::uint64_t ordinal = range.begin;
                     ordinal < range.begin + range.count; ++ordinal) {
                    if (alternatives_by_ordinal.at(ordinal) != descriptors) {
                        throw AdapterFailure(
                            PolicyExactLiftStatus::RefinementFailure,
                            "one quotient cell merged incompatible admitted alternatives");
                    }
                }
            }
            const auto selected = std::find_if(
                published_rows.begin(), published_rows.end(),
                [&](const PublishedRow& row) {
                    return row.source_cell_id == cell.cell_id;
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
            for (const QuotientAlternativeDescriptor& descriptor :
                 descriptors) {
                StableKey resumable{
                    0x70637163656c6c72ull,
                    cell.semantic_identity.size()};
                resumable.insert(
                    resumable.end(),
                    cell.semantic_identity.begin(),
                    cell.semantic_identity.end());
                resumable.push_back(
                    descriptor.resumable_work_identity.size());
                resumable.insert(
                    resumable.end(),
                    descriptor.resumable_work_identity.begin(),
                    descriptor.resumable_work_identity.end());
                quotient::UnresolvedAlternativeObligationIdentity identity;
                identity.source_cell_id = cell.cell_id;
                identity.source_cell_identity = cell.semantic_identity;
                identity.observation_requirement =
                    cell.observation_requirement;
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
                identity.optimistic_lower =
                    quotient::trivial_carrier_wide_lower_q(
                        cell.semantic_identity, cell.coverage);
                identity.scheduling_priority = 0.0;
                identity.resumable_work_identity =
                    std::move(resumable);
                const std::uint32_t obligation_id =
                    bellman.proof_store()
                        ->intern_alternative_obligation(
                            std::move(identity)).first;
                bellman.proof_store()
                    ->transition_alternative_obligation(
                        obligation_id,
                        quotient::AlternativeObligationStatus::LowerOnly);
                admitted_action_accounting.push_back({
                    cell.cell_id,
                    descriptor.action,
                    quotient::AlternativeActionAccountingKind::
                        UnresolvedObligation,
                    std::nullopt,
                    std::nullopt});
                completed_action_accounting.push_back({
                    cell.cell_id,
                    descriptor.action,
                    quotient::AlternativeActionAccountingKind::
                        UnresolvedObligation,
                    std::nullopt,
                    obligation_id});
                saturating_add(
                    telemetry.alternative_obligations_created, 1);
            }
        }
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
            action_audit.unresolved_actions !=
                telemetry.alternative_obligations_created) {
            throw AdapterFailure(
                PolicyExactLiftStatus::RefinementFailure,
                "selected-first quotient lost admitted-action accounting");
        }

        const std::uint32_t root_class = partition.class_by_node.at(0);
        const std::uint32_t root_cell = final_cell_id.at(root_class);
        const quotient::QuotientBellmanResult solved_quotient =
            bellman.solve(
                {root_cell}, limits.max_refinement_rounds,
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
        telemetry.exact_carriers_replayed =
            static_cast<std::uint64_t>(locators.size()) *
            (observation_coarse.rounds + partition.rounds + 4);
        if (solved_quotient.status !=
                quotient::QuotientBellmanStatus::Complete ||
            !solved_quotient.executable_upper ||
            !solved_quotient.proper) {
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
        if (!telemetry.work_to_first_executable_upper.has_value()) {
            telemetry.work_to_first_executable_upper =
                telemetry.strict_reforge_work;
            telemetry.alternatives_materialized_before_first_upper =
                telemetry.alternative_rows_completed;
        }

        const auto publish_current_upper =
                [&](const quotient::QuotientBellmanResult& solved_upper) {
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
        for (const PublishedRow& row : coarse_rows) {
            publication_by_row.emplace(row.sparse_row, &row);
        }
        for (const PublishedRow& row : published_rows) {
            publication_by_row.emplace(row.sparse_row, &row);
        }
        std::map<std::uint32_t, std::uint32_t> final_class_by_cell;
        for (std::uint32_t cls = 0; cls < final_cell_id.size(); ++cls) {
            final_class_by_cell.emplace(final_cell_id[cls], cls);
        }
        std::set<std::uint32_t> reachable(
            solved_upper.reachable_cell_ids.begin(),
            solved_upper.reachable_cell_ids.end());
        std::vector<std::uint32_t> reachable_classes;
        for (std::uint32_t cls = 0; cls < final_cells.size(); ++cls) {
            if (reachable.contains(final_cells[cls].cell_id)) {
                reachable_classes.push_back(cls);
            }
        }
        std::map<std::uint32_t, std::uint32_t> local_by_cell;
        certificate.refinement.classes.resize(reachable_classes.size());
        for (std::uint32_t local = 0;
             local < reachable_classes.size(); ++local) {
            local_by_cell.emplace(
                final_cells[reachable_classes[local]].cell_id, local);
        }
        certificate.refinement.status = RefinementStatus::Complete;
        certificate.refinement.executable = true;
        certificate.refinement.lumpable = true;
        const SolveTransitionCache& graph = bellman.transition_cache();
        for (std::uint32_t local = 0;
             local < reachable_classes.size(); ++local) {
            const std::uint32_t cls = reachable_classes[local];
            const quotient::QuotientCell& cell = final_cells[cls];
            RefinedPolicyClass& policy_class =
                certificate.refinement.classes[local];
            policy_class.class_id = local;
            policy_class.coarse_state = cell.coarse_state;
            policy_class.coarse_state_key = cell.coarse_parent;
            policy_class.terminal = cell.terminal;
            policy_class.goal = cell.terminal;
            policy_class.required_observations =
                cell.observation_requirement;
            policy_class.observation_signature = cell.observed_features;
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
            policy_class.action_cost =
                bellman.priced_rows().at(selected_row).cost;
            const SparseRow& sparse = graph.rows.at(selected_row);
            std::map<std::uint32_t, solve_detail::WideFloat> mass;
            for (std::uint32_t i = 0;
                 i < sparse.transition_count; ++i) {
                const std::uint64_t offset = sparse.transition_offset + i;
                const std::uint32_t target_state =
                    graph.successors.at(offset);
                const std::uint32_t target_cell =
                    *bellman.cell_id_for_state(target_state);
                mass[local_by_cell.at(target_cell)] +=
                    solve_detail::WideFloat{
                        graph.probabilities.at(offset)};
            }
            for (const auto& [target, probability] : mass) {
                policy_class.transitions.push_back({
                    target, probability.value()});
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
        certificate.class_evaluation = evaluate_refined_policy_exact(
            certificate.refinement, std::move(evaluation_request));
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

        (void)oracle.quotient_materialize_locator(root_locator);
        certificate.compiled = oracle.assert_lifted_policy(
            strategy_name, certificate.refinement,
            certificate.class_evaluation);
        certificate.lumpable = true;
        if (telemetry.reference_adapter_invocations != 0) {
            throw AdapterFailure(
                PolicyExactLiftStatus::RefinementFailure,
                "production quotient invoked reference adapter");
        }
        if (certificate.compiled.status !=
                CompiledPolicyAssertionStatus::Complete ||
            !certificate.compiled.executable ||
            !certificate.compiled.zero_off_policy) {
            throw AdapterFailure(
                certificate.compiled.status ==
                        CompiledPolicyAssertionStatus::ResourceCap
                    ? PolicyExactLiftStatus::ResourceCap
                    : PolicyExactLiftStatus::CompiledAssertionFailure,
                certificate.compiled.failure_reason,
                certificate.compiled.resource_cap);
        }
        double artifact_delta = 0.0;
        double artifact_relative = 0.0;
        if (!reconciled(
                certificate.compiled.exact_cost,
                certificate.exact_start_cost,
                artifact_delta, artifact_relative)) {
            throw AdapterFailure(
                PolicyExactLiftStatus::CompiledAssertionFailure,
                "compiled quotient artifact does not reconcile with Bellman value");
        }
        certificate.status = PolicyExactLiftStatus::Complete;
        certificate.executable = true;
        };
        publish_current_upper(solved_quotient);

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
        quotient::QuotientBellmanResult published_solved = solved_quotient;
        PolicyExactLiftCertificate retained_publication = certificate;
        std::uint64_t published_q_generation =
            bellman.proof_store()->q_generation();
        bool publication_blocked_after_improvement = false;
        bool stop_alternative_scheduling = false;
        std::set<std::uint32_t> attempted_obligations;
        std::map<std::uint32_t, std::uint32_t>
            scheduler_class_by_cell;
        for (std::uint32_t cls = 0; cls < final_cell_id.size(); ++cls) {
            scheduler_class_by_cell.emplace(final_cell_id[cls], cls);
        }
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
                if (upper == current_upper.end() ||
                    bellman.proof_store()
                        ->alternative_obligation_blocks_exactness(
                            obligation_id, upper->second,
                            bellman.proof_store()->q_generation())) {
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
            bool policy_improved_this_round = false;

            for (const std::uint32_t obligation_id : pending) {
                const quotient::UnresolvedAlternativeObligation& before =
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
                    final_cells[source_class->second];
                quotient::AlternativeObligationValidationContext context;
                context.source_cell_identity = cell.semantic_identity;
                context.price_identity = price_identity;
                context.vocabulary_identity = vocabulary_identity;
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
                const auto& representative_descriptors =
                    alternatives_by_ordinal.at(representative);
                const auto descriptor_it = std::find_if(
                    representative_descriptors.begin(),
                    representative_descriptors.end(),
                    [&](const QuotientAlternativeDescriptor& descriptor) {
                        return descriptor.action == before.identity.action;
                    });
                if (descriptor_it == representative_descriptors.end()) {
                    bellman.proof_store()
                        ->transition_alternative_obligation(
                            obligation_id,
                            quotient::AlternativeObligationStatus::Stale);
                    saturating_add(
                        telemetry.alternative_obligations_stale, 1);
                    attempted_obligations.insert(obligation_id);
                    continue;
                }
                const QuotientAlternativeDescriptor descriptor =
                    *descriptor_it;
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
                    for (const quotient::CoverageRange& range :
                         cell.coverage.ranges) {
                        for (std::uint64_t ordinal = range.begin;
                             ordinal < range.begin + range.count;
                             ++ordinal) {
                            ExactState source =
                                oracle.quotient_materialize_locator(
                                    locators.at(ordinal));
                            std::optional<QuotientOracleCompactRow>
                                candidate;
                            try {
                                candidate = oracle
                                    .quotient_certify_alternative_descriptor(
                                        source, descriptor);
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
                            for (const auto& transition :
                                 row.transitions) {
                                if (transition.strict_state >=
                                        ordinal_by_strict_state.size() ||
                                    ordinal_by_strict_state[
                                        transition.strict_state] == kNoId) {
                                    complete_cell_row = false;
                                    break;
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
                            final_cells[
                                scheduler_class_by_cell.at(target)]
                                .semantic_identity,
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
                        sparse_row, cell.cell_id,
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

                    double candidate_q = certified_cost;
                    for (const auto& [target, probability] :
                         certified_projection) {
                        const std::uint32_t target_state =
                            *bellman.state_index_for_cell(target);
                        candidate_q += probability *
                            current_solved.values_by_state.at(
                                target_state);
                    }
                    const std::uint32_t source_state =
                        *bellman.state_index_for_cell(cell.cell_id);
                    if (candidate_q + 1e-12 >=
                        current_solved.values_by_state.at(
                            source_state)) {
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
                    current_solved = std::move(improved);
                    try {
                        publish_current_upper(current_solved);
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
                    published_solved = current_solved;
                    published_q_generation =
                        bellman.proof_store()->q_generation();
                    saturating_add(
                        telemetry.alternative_policy_improvements, 1);
                    policy_improved_this_round = true;
                    break;
                } catch (const AdapterFailure& error) {
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
            if (!policy_improved_this_round) break;
        }

        const std::map<std::uint32_t, double> published_upper =
            upper_by_source(published_solved);
        const quotient::AlternativeActionAccountingAudit final_action_audit =
            bellman.proof_store()->audit_alternative_actions(
                admitted_action_accounting,
                completed_action_accounting,
                published_upper,
                published_q_generation);
        if (!final_action_audit.complete) {
            throw AdapterFailure(
                PolicyExactLiftStatus::RefinementFailure,
                "competitive alternative scheduler lost admitted-action accounting");
        }
        telemetry.action_accounting_complete = true;
        telemetry.unresolved_alternative_obligations =
            final_action_audit.unresolved_actions;
        telemetry.competitive_alternatives_remaining =
            final_action_audit.unresolved_actions;
        if (publication_blocked_after_improvement) {
            saturating_add(
                telemetry.competitive_alternatives_remaining, 1);
        }
        telemetry.exact_alternative_envelope_closed =
            final_action_audit.exact_alternative_envelope_closed &&
            !publication_blocked_after_improvement;
        telemetry.bounded_publication_retained =
            telemetry.bounded_publication_retained ||
            (certificate.executable &&
             !telemetry.exact_alternative_envelope_closed);

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
                lift_telemetry.strict_reforge_work,
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
                lift_telemetry.strict_reforge_work;
            saturating_add(
                spent_reforge,
                reoptimization_telemetry.strict_reforge_work);
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
        CompiledPolicyAssertionStatus::Complete) {
        double artifact_delta = 0.0;
        double artifact_relative = 0.0;
        if (!reconciled(
                certificate.compiled.exact_cost,
                certificate.exact_start_cost,
                artifact_delta,
                artifact_relative)) {
            certificate.status =
                PolicyExactLiftStatus::CompiledAssertionFailure;
            certificate.failure_reason =
                "compiled lifted artifact does not reconcile with its "
                "refined class-policy value";
            return certificate;
        }
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

} // namespace refinement
} // namespace solver
} // namespace poecraft
