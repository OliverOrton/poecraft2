#include "solver_quotient_bellman.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <stdexcept>

namespace poecraft {
namespace solver {
namespace quotient {

namespace {

using solve_detail::SparsePolicyRowInput;
using solve_detail::SparsePolicyTransitionInput;
using solve_detail::WideFloat;

constexpr std::uint64_t kNoRow =
    std::numeric_limits<std::uint64_t>::max();

std::uint64_t saturated_add(
        const std::uint64_t left,
        const std::uint64_t right) {
    return right > std::numeric_limits<std::uint64_t>::max() - left
               ? std::numeric_limits<std::uint64_t>::max()
               : left + right;
}

std::uint64_t saturated_product(
        const std::size_t count,
        const std::size_t width) {
    return width != 0 &&
                   count >
                       std::numeric_limits<std::uint64_t>::max() / width
               ? std::numeric_limits<std::uint64_t>::max()
               : static_cast<std::uint64_t>(count) * width;
}

bool valid_probability(const double value) {
    return std::isfinite(value) && value >= 0.0;
}

std::vector<QuotientBellmanTransitionInput> canonical_transitions(
        std::vector<QuotientBellmanTransitionInput> transitions) {
    std::sort(
        transitions.begin(), transitions.end(),
        [](const auto& left, const auto& right) {
            return std::tie(left.label, left.target_cell_id) <
                   std::tie(right.label, right.target_cell_id);
        });
    std::vector<QuotientBellmanTransitionInput> out;
    for (QuotientBellmanTransitionInput& transition : transitions) {
        if (!valid_probability(transition.probability)) {
            throw std::invalid_argument(
                "quotient Bellman row has an invalid probability");
        }
        if (transition.probability == 0.0) continue;
        if (!out.empty() && out.back().label == transition.label &&
            out.back().target_cell_id == transition.target_cell_id) {
            const WideFloat combined =
                WideFloat{out.back().probability} +
                WideFloat{transition.probability};
            out.back().probability = combined.value();
        } else {
            out.push_back(std::move(transition));
        }
    }
    return out;
}

} // namespace

const char* quotient_bellman_status_name(
        const QuotientBellmanStatus status) {
    switch (status) {
    case QuotientBellmanStatus::Complete:
        return "complete";
    case QuotientBellmanStatus::EmptyGraph:
        return "empty_graph";
    case QuotientBellmanStatus::InvalidCell:
        return "invalid_cell";
    case QuotientBellmanStatus::InvalidRow:
        return "invalid_row";
    case QuotientBellmanStatus::MissingReachableCertificate:
        return "missing_reachable_certificate";
    case QuotientBellmanStatus::ImproperPolicy:
        return "improper_policy";
    case QuotientBellmanStatus::DidNotConverge:
        return "did_not_converge";
    case QuotientBellmanStatus::ResourceCap:
        return "resource_cap";
    }
    return "invalid_row";
}

QuotientBellmanGraph::QuotientBellmanGraph(
        const std::uint64_t max_owned_bytes) {
    transition_cache_.quotient_proofs =
        std::make_shared<ProofStore>(max_owned_bytes);
}

std::optional<std::uint32_t> QuotientBellmanGraph::state_for_cell(
        const std::uint32_t cell_id) const {
    const auto found = cells_.find(cell_id);
    if (found == cells_.end()) return std::nullopt;
    return found->second.state;
}

void QuotientBellmanGraph::install_cells(
        std::vector<QuotientBellmanCellInput> cells) {
    std::sort(
        cells.begin(), cells.end(),
        [](const auto& left, const auto& right) {
            return left.cell_id < right.cell_id;
        });
    for (QuotientBellmanCellInput& cell : cells) {
        if (cell.semantic_identity.empty() || cell.generation == 0) {
            throw std::invalid_argument(
                "quotient Bellman cell lacks stable identity or generation");
        }
        const auto found = cells_.find(cell.cell_id);
        if (found != cells_.end()) {
            if (found->second.cell != cell) {
                throw std::logic_error(
                    "stable quotient Bellman cell identity changed");
            }
            continue;
        }
        const std::uint32_t state =
            static_cast<std::uint32_t>(cell_by_state_.size());
        const std::uint64_t generation = cell.generation;
        cell_by_state_.push_back(cell.cell_id);
        reverse_predecessors_.emplace_back();
        cells_.emplace(
            cell.cell_id,
            CellRecord{
                std::move(cell), state,
                generation,
                generation});
        if (transition_cache_.state_rows.size() <= state) {
            transition_cache_.state_rows.resize(state + 1);
        }
    }
    transition_cache_.discovered_states =
        static_cast<std::uint32_t>(cell_by_state_.size());
    refresh_row_kernel_bytes();
}

void QuotientBellmanGraph::supersede_cell(
        QuotientBellmanCellInput cell) {
    const auto found = cells_.find(cell.cell_id);
    if (found == cells_.end() || cell.semantic_identity.empty() ||
        cell.generation <= found->second.cell.generation) {
        throw std::invalid_argument(
            "quotient Bellman replacement does not advance a known cell");
    }
    invalidate_source_split(cell.cell_id, cell.generation);
    invalidate_target_split(cell.cell_id, cell.generation);
    found->second.cell = std::move(cell);
    refresh_row_kernel_bytes();
}

std::uint64_t QuotientBellmanGraph::append_row(
        QuotientBellmanRowInput row) {
    const std::optional<std::uint32_t> source =
        state_for_cell(row.source_cell_id);
    if (!source.has_value() || !std::isfinite(row.cost) || row.cost < 0.0) {
        throw std::invalid_argument(
            "quotient Bellman row has an invalid source or cost");
    }
    row.transitions = canonical_transitions(std::move(row.transitions));
    if (row.transitions.empty()) {
        throw std::invalid_argument(
            "nonterminal quotient Bellman row has no transitions");
    }
    WideFloat probability_sum{0.0};
    SparsePolicyRowInput sparse;
    sparse.owner_state = *source;
    sparse.operator_index = row.operator_index;
    sparse.cost = row.cost;
    sparse.admitted = row.admitted;
    std::vector<TargetGenerationDependency> targets;
    std::set<std::uint32_t> distinct_targets;
    for (const QuotientBellmanTransitionInput& transition : row.transitions) {
        const auto target = cells_.find(transition.target_cell_id);
        if (target == cells_.end()) {
            throw std::invalid_argument(
                "quotient Bellman row names an unknown target cell");
        }
        probability_sum += WideFloat{transition.probability};
        sparse.transitions.push_back({
            target->second.state, transition.probability});
        if (distinct_targets.insert(transition.target_cell_id).second) {
            targets.push_back({
                transition.target_cell_id,
                target->second.target_generation});
            std::vector<std::uint32_t>& predecessors =
                reverse_predecessors_.at(target->second.state);
            if (std::find(
                    predecessors.begin(), predecessors.end(), *source) ==
                predecessors.end()) {
                predecessors.push_back(*source);
                std::sort(predecessors.begin(), predecessors.end());
            }
        }
    }
    if (std::fabs(probability_sum.value() - 1.0) > 1e-12) {
        throw std::invalid_argument(
            "quotient Bellman row probability does not sum exactly to one");
    }

    if (row.certified != row.proof_identity.has_value()) {
        throw std::invalid_argument(
            "quotient Bellman certification and proof payload disagree");
    }
    if (row.certified) {
        CertifiedRowIdentity canonical =
            canonical_certified_row_identity(*row.proof_identity);
        if (canonical != *row.proof_identity ||
            canonical.action_id != row.operator_index ||
            std::fabs(
                canonical.exact_total_probability -
                probability_sum.value()) > 1e-12 ||
            canonical.projected_arcs.size() != row.transitions.size()) {
            throw std::invalid_argument(
                "quotient Bellman row does not match its proof identity");
        }
        std::vector<ProofProjectedArc> expected_arcs;
        expected_arcs.reserve(row.transitions.size());
        for (const QuotientBellmanTransitionInput& transition :
             row.transitions) {
            const auto target = cells_.find(transition.target_cell_id);
            expected_arcs.push_back({
                transition.label,
                target->second.cell.semantic_identity,
                transition.probability});
        }
        std::sort(
            expected_arcs.begin(), expected_arcs.end(),
            [](const ProofProjectedArc& left,
               const ProofProjectedArc& right) {
                return std::tie(
                           left.label,
                           left.target_cell_identity,
                           left.probability) <
                       std::tie(
                           right.label,
                           right.target_cell_identity,
                           right.probability);
            });
        if (expected_arcs != canonical.projected_arcs) {
            throw std::invalid_argument(
                "quotient Bellman transition changed its proof projection");
        }
        row.proof_identity = std::move(canonical);
    }

    const std::uint64_t stable_row =
        solve_detail::append_sparse_policy_row(
            transition_cache_, priced_rows_, sparse);
    if (row.certified) {
        auto [payload, inserted] =
            transition_cache_.quotient_proofs->intern_payload(
                std::move(*row.proof_identity));
        if (!inserted) ++telemetry_.proof_payload_reuses;
        transition_cache_.quotient_proofs->attach_row(
            stable_row,
            payload,
            row.source_cell_id,
            cells_.at(row.source_cell_id).source_generation,
            std::move(targets),
            action_generation_,
            admission_generation_);
        ++telemetry_.row_reprojections;
    }
    refresh_row_kernel_bytes();
    return stable_row;
}

std::uint64_t QuotientBellmanGraph::invalidate_source_split(
        const std::uint32_t source_cell_id,
        const std::uint64_t replacement_generation) {
    auto found = cells_.find(source_cell_id);
    if (found == cells_.end() ||
        replacement_generation <= found->second.source_generation) {
        throw std::invalid_argument(
            "source split does not advance a known cell generation");
    }
    found->second.source_generation = replacement_generation;
    ++telemetry_.source_splits;
    const std::uint64_t invalidated =
        transition_cache_.quotient_proofs->invalidate_source(
            source_cell_id);
    telemetry_.reverse_invalidations += invalidated;
    return invalidated;
}

std::uint64_t QuotientBellmanGraph::invalidate_target_split(
        const std::uint32_t target_cell_id,
        const std::uint64_t replacement_generation) {
    auto found = cells_.find(target_cell_id);
    if (found == cells_.end() ||
        replacement_generation <= found->second.target_generation) {
        throw std::invalid_argument(
            "target split does not advance a known cell generation");
    }
    found->second.target_generation = replacement_generation;
    ++telemetry_.target_splits;
    const std::uint64_t invalidated =
        transition_cache_.quotient_proofs->invalidate_target(
            target_cell_id);
    telemetry_.reverse_invalidations += invalidated;
    return invalidated;
}

void QuotientBellmanGraph::note_price_change(
        const std::map<std::uint64_t, double>& row_costs) {
    for (const auto& [row, cost] : row_costs) {
        if (row >= priced_rows_.size() || !std::isfinite(cost) || cost < 0.0) {
            throw std::invalid_argument(
                "price update names an invalid sparse row or cost");
        }
        priced_rows_[row].cost = cost;
    }
    transition_cache_.quotient_proofs->note_price_change();
}

bool QuotientBellmanGraph::row_certificate_current(
        const std::uint64_t row) const {
    if (row >= transition_cache_.rows.size() ||
        !transition_cache_.quotient_proofs->has_use_site(row)) {
        return false;
    }
    const RowProofUseSite& use =
        transition_cache_.quotient_proofs->use_site(row);
    if (!use.present) return false;
    const CertifiedRowIdentity& expected =
        transition_cache_.quotient_proofs
            ->payload(use.payload_id).identity;
    const auto source = cells_.find(use.source_cell_id);
    if (source == cells_.end()) return false;
    ProofValidationContext context;
    context.source_generation = source->second.source_generation;
    context.target_generation = [&](const std::uint32_t cell_id)
        -> std::optional<std::uint64_t> {
        const auto found = cells_.find(cell_id);
        return found == cells_.end()
                   ? std::nullopt
                   : std::optional<std::uint64_t>{
                         found->second.target_generation};
    };
    context.action_generation = action_generation_;
    context.admission_generation = admission_generation_;
    return transition_cache_.quotient_proofs->validate_row(
               row, expected, context) == ProofValidationStatus::Current;
}

void QuotientBellmanGraph::refresh_row_kernel_bytes() {
    std::uint64_t bytes = external_row_kernel_bytes_;
        saturated_product(
            transition_cache_.rows.capacity(), sizeof(SparseRow));
    bytes = saturated_add(
        bytes,
        saturated_product(
            transition_cache_.state_rows.capacity(), sizeof(StateRowSpan)));
    bytes = saturated_add(
        bytes,
        saturated_product(
            transition_cache_.successors.capacity(),
            sizeof(std::uint32_t)));
    bytes = saturated_add(
        bytes,
        saturated_product(
            transition_cache_.probabilities.capacity(), sizeof(double)));
    bytes = saturated_add(
        bytes,
        saturated_product(
            priced_rows_.capacity(), sizeof(PricedSparseRow)));
    bytes = saturated_add(
        bytes,
        saturated_product(
            cell_by_state_.capacity(), sizeof(std::uint32_t)));
    bytes = saturated_add(
        bytes,
        saturated_product(
            reverse_predecessors_.capacity(),
            sizeof(std::vector<std::uint32_t>)));
    for (const std::vector<std::uint32_t>& predecessors :
         reverse_predecessors_) {
        bytes = saturated_add(
            bytes,
            saturated_product(
                predecessors.capacity(), sizeof(std::uint32_t)));
    }
    transition_cache_.quotient_proofs->ledger().set_owned_bytes(
        ProofMemoryCategory::RowKernel, bytes);
    telemetry_.memory =
        transition_cache_.quotient_proofs->ledger().snapshot();
}

QuotientBellmanResult QuotientBellmanGraph::solve(
        const std::vector<std::uint32_t>& entry_cell_ids,
        const std::uint32_t max_policy_iterations,
        const std::uint32_t max_component_iterations,
        const double improvement_epsilon) {
    QuotientBellmanResult out;
    if (entry_cell_ids.empty() || cells_.empty()) {
        out.failure_reason = "quotient Bellman graph has no entries";
        return out;
    }
    try {
        const std::size_t state_count = cell_by_state_.size();
        std::uint64_t scratch_bytes =
            saturated_product(state_count, 3 * sizeof(double));
        scratch_bytes = saturated_add(
            scratch_bytes,
            saturated_product(
                state_count, 2 * sizeof(std::uint64_t) +
                                 2 * sizeof(std::uint8_t)));
        ScopedProofMemoryCharge scratch(
            transition_cache_.quotient_proofs->ledger(),
            ProofMemoryCategory::Scratch,
            scratch_bytes);

        std::vector<double> optimistic(state_count, 0.0);
        out.lower_relaxation_by_state.assign(
            state_count, kInfinity);
        for (std::uint32_t state = 0; state < state_count; ++state) {
            const CellRecord& cell = cells_.at(cell_by_state_[state]);
            if (cell.cell.terminal) {
                out.lower_relaxation_by_state[state] = 0.0;
                continue;
            }
            const auto selected = solve_detail::select_sparse_policy_row(
                transition_cache_, state, improvement_epsilon,
                [&](const std::uint64_t row) {
                    return transition_cache_.rows[row].admitted;
                },
                [&](const std::uint64_t row, std::uint32_t& work) {
                    /* Start from the certified immediate-cost relaxation.
                     * It is a valid optimistic lower policy even when it is
                     * improper; exact SCC evaluation below either proves it
                     * executable or repairs it deterministically. */
                    work = 0;
                    return priced_rows_.at(row).cost;
                });
            telemetry_.bellman_rows_evaluated += selected.evaluated_rows;
            out.lower_relaxation_by_state[state] = selected.value;
        }

        /* Compute the greatest certified nonterminal closed set. When one
         * exists, seed it with a deterministic admitted-row witness so the
         * exact evaluator exercises (and must survive) the same improper
         * inherited-policy repair used after incremental invalidation. */
        std::vector<std::uint8_t> closed_seed(state_count, 1);
        for (std::uint32_t state = 0; state < state_count; ++state) {
            if (cells_.at(cell_by_state_[state]).cell.terminal) {
                closed_seed[state] = 0;
            }
        }
        bool closed_changed = true;
        while (closed_changed) {
            closed_changed = false;
            for (std::uint32_t state = 0; state < state_count; ++state) {
                if (!closed_seed[state]) continue;
                bool has_closed_row = false;
                for (const std::uint64_t row :
                     state_row_indices(transition_cache_, state)) {
                    if (!transition_cache_.rows[row].admitted ||
                        !row_certificate_current(row)) {
                        continue;
                    }
                    const SparseRow& sparse = transition_cache_.rows[row];
                    bool closed = true;
                    for (std::uint32_t i = 0;
                         i < sparse.transition_count; ++i) {
                        if (!closed_seed[transition_cache_.successors[
                                sparse.transition_offset + i]]) {
                            closed = false;
                            break;
                        }
                    }
                    if (closed) {
                        has_closed_row = true;
                        break;
                    }
                }
                if (!has_closed_row) {
                    closed_seed[state] = 0;
                    closed_changed = true;
                }
            }
        }

        std::vector<std::uint64_t> policy_rows(state_count, kNoRow);
        for (std::uint32_t state = 0; state < state_count; ++state) {
            const CellRecord& cell = cells_.at(cell_by_state_[state]);
            if (cell.cell.terminal) continue;
            if (closed_seed[state]) {
                for (const std::uint64_t row :
                     state_row_indices(transition_cache_, state)) {
                    if (!transition_cache_.rows[row].admitted ||
                        !row_certificate_current(row)) {
                        continue;
                    }
                    const SparseRow& sparse = transition_cache_.rows[row];
                    bool closed = true;
                    for (std::uint32_t i = 0;
                         i < sparse.transition_count; ++i) {
                        if (!closed_seed[transition_cache_.successors[
                                sparse.transition_offset + i]]) {
                            closed = false;
                            break;
                        }
                    }
                    if (closed) {
                        policy_rows[state] = row;
                        break;
                    }
                }
            }
            if (policy_rows[state] != kNoRow) continue;
            const auto selected = solve_detail::select_sparse_policy_row(
                transition_cache_, state, improvement_epsilon,
                [&](const std::uint64_t row) {
                    return transition_cache_.rows[row].admitted &&
                           row_certificate_current(row);
                },
                [&](const std::uint64_t row, std::uint32_t& work) {
                    work = 0;
                    return priced_rows_.at(row).cost;
                });
            telemetry_.bellman_rows_evaluated += selected.evaluated_rows;
            policy_rows[state] = selected.row;
        }

        std::vector<std::uint32_t> entries;
        entries.reserve(entry_cell_ids.size());
        for (const std::uint32_t cell_id : entry_cell_ids) {
            const std::optional<std::uint32_t> state = state_for_cell(cell_id);
            if (!state.has_value()) {
                out.status = QuotientBellmanStatus::InvalidCell;
                out.failure_reason =
                    "quotient Bellman entry names an unknown cell";
                return out;
            }
            entries.push_back(*state);
        }
        std::sort(entries.begin(), entries.end());
        entries.erase(std::unique(entries.begin(), entries.end()), entries.end());

        std::vector<double> values(state_count, kInfinity);
        for (std::uint32_t iteration = 0;
             iteration < max_policy_iterations; ++iteration) {
            std::vector<std::uint8_t> envelope(state_count, 0);
            std::set<std::uint32_t> pending(entries.begin(), entries.end());
            while (!pending.empty()) {
                const std::uint32_t state = *pending.begin();
                pending.erase(pending.begin());
                if (envelope[state]) continue;
                envelope[state] = 1;
                const CellRecord& cell = cells_.at(cell_by_state_[state]);
                if (cell.cell.terminal) continue;
                bool saw_certificate = false;
                for (const std::uint64_t row :
                     state_row_indices(transition_cache_, state)) {
                    if (!transition_cache_.rows[row].admitted ||
                        !row_certificate_current(row)) {
                        continue;
                    }
                    saw_certificate = true;
                    const SparseRow& sparse = transition_cache_.rows[row];
                    for (std::uint32_t i = 0;
                         i < sparse.transition_count; ++i) {
                        const std::uint32_t target =
                            transition_cache_.successors[
                                sparse.transition_offset + i];
                        if (!envelope[target]) pending.insert(target);
                    }
                }
                if (!saw_certificate || policy_rows[state] == kNoRow ||
                    !row_certificate_current(policy_rows[state])) {
                    out.status = QuotientBellmanStatus::
                        MissingReachableCertificate;
                    out.failure_reason =
                        "reachable quotient cell has no current certified row";
                    return out;
                }
            }

            std::vector<std::uint32_t> local_by_state(state_count, kNoId);
            std::vector<std::uint32_t> state_by_local;
            for (std::uint32_t state = 0; state < state_count; ++state) {
                if (!envelope[state]) continue;
                local_by_state[state] =
                    static_cast<std::uint32_t>(state_by_local.size());
                state_by_local.push_back(state);
            }

            refinement::RefinementResult fixed;
            fixed.status = refinement::RefinementStatus::Complete;
            fixed.executable = true;
            fixed.lumpable = true;
            fixed.classes.resize(state_by_local.size());
            for (std::uint32_t local = 0;
                 local < state_by_local.size(); ++local) {
                const std::uint32_t state = state_by_local[local];
                const CellRecord& cell = cells_.at(cell_by_state_[state]);
                refinement::RefinedPolicyClass& cls = fixed.classes[local];
                cls.class_id = local;
                cls.coarse_state = cell.cell.cell_id;
                cls.coarse_state_key = cell.cell.semantic_identity;
                cls.exact_members = {cell.cell.semantic_identity};
                cls.goal = cell.cell.terminal;
                cls.terminal = cell.cell.terminal;
                if (cell.cell.terminal) continue;
                const std::uint64_t row = policy_rows[state];
                const RowProofUseSite& use =
                    transition_cache_.quotient_proofs->use_site(row);
                const CertifiedRowIdentity& proof =
                    transition_cache_.quotient_proofs
                        ->payload(use.payload_id).identity;
                refinement::SelectedAction selected;
                selected.action_id = proof.action_id;
                selected.semantic_key = proof.semantic_action_identity;
                cls.selected_action = std::move(selected);
                cls.action_cost = priced_rows_[row].cost;
                std::map<std::uint32_t, WideFloat> mass_by_target;
                const SparseRow& sparse = transition_cache_.rows[row];
                for (std::uint32_t i = 0;
                     i < sparse.transition_count; ++i) {
                    const std::uint64_t offset = sparse.transition_offset + i;
                    const std::uint32_t target_state =
                        transition_cache_.successors[offset];
                    if (local_by_state[target_state] == kNoId) {
                        out.status = QuotientBellmanStatus::InvalidRow;
                        out.failure_reason =
                            "selected certified row escapes its closed envelope";
                        return out;
                    }
                    mass_by_target[local_by_state[target_state]] +=
                        WideFloat{transition_cache_.probabilities[offset]};
                }
                for (const auto& [target, probability] : mass_by_target) {
                    cls.transitions.push_back({target, probability.value()});
                }
            }

            refinement::PolicyEvaluationRequest evaluation_request;
            /* Bellman alternatives may leave the currently selected entry
             * closure. Evaluate every cell in the admitted certified
             * envelope so policy improvement never compares against an
             * inherited infinity or mixes values from incompatible rows. */
            for (std::uint32_t local = 0;
                 local < state_by_local.size(); ++local) {
                evaluation_request.start_classes.push_back(local);
            }
            evaluation_request.limits.max_reachable_classes =
                static_cast<std::uint32_t>(state_by_local.size());
            evaluation_request.limits.max_component_iterations =
                max_component_iterations;
            evaluation_request.limits.max_estimated_memory_bytes =
                transition_cache_.quotient_proofs->ledger().cap_bytes() -
                std::min(
                    transition_cache_.quotient_proofs->ledger().cap_bytes(),
                    transition_cache_.quotient_proofs->ledger()
                        .snapshot().total_bytes);
            refinement::PolicyEvaluationResult evaluated =
                refinement::evaluate_refined_policy_exact(
                    fixed, std::move(evaluation_request));
            telemetry_.scc_evaluations +=
                evaluated.strongly_connected_components;
            if (evaluated.status ==
                    refinement::PolicyEvaluationStatus::ImproperPolicy) {
                bool repaired = false;
                for (const std::uint32_t local :
                     evaluated.improper_component_classes) {
                    if (local >= state_by_local.size()) continue;
                    const std::uint32_t state = state_by_local[local];
                    bool passed_current = false;
                    std::uint64_t first_alternative = kNoRow;
                    for (const std::uint64_t row :
                         state_row_indices(transition_cache_, state)) {
                        if (!transition_cache_.rows[row].admitted ||
                            !row_certificate_current(row)) {
                            continue;
                        }
                        if (row != policy_rows[state] &&
                            first_alternative == kNoRow) {
                            first_alternative = row;
                        }
                        if (passed_current) {
                            policy_rows[state] = row;
                            repaired = true;
                            break;
                        }
                        if (row == policy_rows[state]) passed_current = true;
                    }
                    if (!repaired && first_alternative != kNoRow) {
                        policy_rows[state] = first_alternative;
                        repaired = true;
                    }
                    if (repaired) {
                        ++telemetry_.policy_improvements;
                        ++telemetry_.improper_policy_repairs;
                        break;
                    }
                }
                if (repaired) continue;
                out.status = QuotientBellmanStatus::ImproperPolicy;
                out.failure_reason = evaluated.failure_reason;
                return out;
            }
            if (evaluated.status !=
                    refinement::PolicyEvaluationStatus::Complete ||
                !evaluated.converged || !evaluated.proper) {
                out.status = evaluated.status ==
                                     refinement::PolicyEvaluationStatus::
                                         ResourceCap
                                 ? QuotientBellmanStatus::ResourceCap
                                 : QuotientBellmanStatus::DidNotConverge;
                out.failure_reason = evaluated.failure_reason;
                return out;
            }
            std::fill(values.begin(), values.end(), kInfinity);
            for (const refinement::RefinedClassValue& value :
                 evaluated.class_values) {
                values[state_by_local.at(value.class_id)] = value.value;
            }

            bool changed = false;
            for (const std::uint32_t state : state_by_local) {
                const CellRecord& cell = cells_.at(cell_by_state_[state]);
                if (cell.cell.terminal) continue;
                const auto selected = solve_detail::select_sparse_policy_row(
                    transition_cache_, state, improvement_epsilon,
                    [&](const std::uint64_t row) {
                        return transition_cache_.rows[row].admitted &&
                               row_certificate_current(row);
                    },
                    [&](const std::uint64_t row, std::uint32_t& work) {
                        return solve_detail::evaluate_sparse_policy_row(
                            transition_cache_, priced_rows_, values,
                            row, work);
                    });
                telemetry_.bellman_rows_evaluated += selected.evaluated_rows;
                if (selected.row != kNoRow &&
                    selected.row != policy_rows[state]) {
                    policy_rows[state] = selected.row;
                    ++telemetry_.policy_improvements;
                    changed = true;
                }
            }
            if (changed) continue;

            out.status = QuotientBellmanStatus::Complete;
            out.executable_upper = true;
            out.proper = true;
            out.values_by_state = std::move(values);
            out.selected_rows_by_state = std::move(policy_rows);
            std::set<std::uint32_t> selected_pending(entries.begin(), entries.end());
            std::vector<std::uint8_t> selected_seen(state_count, 0);
            while (!selected_pending.empty()) {
                const std::uint32_t state = *selected_pending.begin();
                selected_pending.erase(selected_pending.begin());
                if (selected_seen[state]) continue;
                selected_seen[state] = 1;
                out.reachable_cell_ids.push_back(cell_by_state_[state]);
                const std::uint64_t row = out.selected_rows_by_state[state];
                if (row == kNoRow) continue;
                const SparseRow& sparse = transition_cache_.rows[row];
                for (std::uint32_t i = 0;
                     i < sparse.transition_count; ++i) {
                    const std::uint32_t target =
                        transition_cache_.successors[
                            sparse.transition_offset + i];
                    if (!selected_seen[target]) selected_pending.insert(target);
                }
            }
            std::sort(
                out.reachable_cell_ids.begin(),
                out.reachable_cell_ids.end());
            refresh_row_kernel_bytes();
            telemetry_.memory =
                transition_cache_.quotient_proofs->ledger().snapshot();
            return out;
        }
        out.status = QuotientBellmanStatus::DidNotConverge;
        out.failure_reason =
            "quotient Bellman policy improvement reached its iteration cap";
        return out;
    } catch (const ProofMemoryLimit& error) {
        out.status = QuotientBellmanStatus::ResourceCap;
        out.failure_reason = error.what();
        return out;
    } catch (const std::exception& error) {
        out.status = QuotientBellmanStatus::InvalidRow;
        out.failure_reason = error.what();
        return out;
    }
}

} // namespace quotient
} // namespace solver
} // namespace poecraft
