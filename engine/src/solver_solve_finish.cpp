#include "solver_solve_types.hpp"
#include "solver_policy_refinement.hpp"
#include "solver_sparse_policy.hpp"

#include <cstring>

namespace poecraft {
namespace solver {

using namespace solve_detail;

/* Policy-lift coroutines already suspend at carrier, partition, proof, and
 * compiled-evaluation boundaries. Advancing one suspension per public solve
 * step made progress/memory telemetry dominate tiny proof items. Keep
 * bounded batches so cancellation remains responsive while telemetry is
 * audited once per useful slice. Alternative proof uses the request's
 * measured 128-item slice; heavier construction phases keep a smaller
 * batch. */
constexpr std::uint32_t kCooperativePolicyLiftBatch = 32;
constexpr std::uint32_t kCooperativeAlternativeProofBatch = 128;

void order_observed_modifier_choices(
        const ActionDescriptor& action,
        std::vector<OutcomeChoiceOption>& choices,
        const std::vector<double>& values) {
    if (!action_observes_modifier_offer(action)) {
        throw std::logic_error(
            "observed-choice finalization requires an admitted "
            "outcome-observation contract");
    }
    std::sort(
        choices.begin(), choices.end(),
        [&](const OutcomeChoiceOption& left,
            const OutcomeChoiceOption& right) {
            if (left.state >= values.size() ||
                right.state >= values.size()) {
                throw std::logic_error(
                    "observed-choice finalization references a state "
                    "outside the value table");
            }
            const double left_value = values[left.state];
            const double right_value = values[right.state];
            if (left.state == right.state) {
                return left.mod_id < right.mod_id;
            }
            return sparse_policy_choice_precedes(
                left_value, left.state,
                right_value, right.state);
        });
}

SolveTermination successful_refined_publication_termination(
        const SolveTermination coarse_termination,
        const bool resource_cap_hit,
        const bool globally_exact,
        const bool coarse_discovery_closed) {
    if (globally_exact) {
        return SolveTermination::ExactClosed;
    }
    if (coarse_termination ==
            SolveTermination::RefusedResourceCap ||
        resource_cap_hit) {
        return SolveTermination::RefusedResourceCap;
    }
    if (coarse_termination == SolveTermination::TargetGap) {
        return SolveTermination::TargetGap;
    }
    if (coarse_termination == SolveTermination::NumericalStability) {
        return SolveTermination::NumericalStability;
    }
    if (coarse_termination ==
        SolveTermination::RequestedBoundedFinish) {
        return SolveTermination::RequestedBoundedFinish;
    }
    if (coarse_termination == SolveTermination::ExactClosed) {
        /* Exact closure of a coarse quotient cannot survive a failed or
         * incomplete strict publication proof. The executable graph still
         * supplies a bounded upper, but global closure remains unresolved. */
        return SolveTermination::NumericalStability;
    }
    /*
     * The caller has retained an exact executable strategy, so None and
     * NoExecutablePolicy cannot remain the published stopping cause. They
     * become ExactClosed only when the caller separately proves that broad
     * discovery itself closed; executable recovery alone is insufficient.
     */
    if (!coarse_discovery_closed) {
        throw std::logic_error(
            "bounded refined publication has no coarse discovery closure "
            "or orthogonal stop cause");
    }
    return SolveTermination::ExactClosed;
}

void record_live_policy_lift_telemetry(
        PolicyRefinementTelemetry& telemetry,
        const refinement::PolicyLiftAdapterTelemetry& adapter) {
    telemetry.strict_lift_total_ns = adapter.total_ns;
    telemetry.strict_carrier_discovery_ns = adapter.carrier_discovery_ns;
    telemetry.strict_partition_refinement_ns =
        adapter.partition_refinement_ns;
    telemetry.strict_policy_evaluation_ns = adapter.policy_evaluation_ns;
    telemetry.strict_local_reoptimization_ns =
        adapter.local_reoptimization_ns;
    telemetry.strict_session_constructions =
        adapter.strict_session_constructions;
    telemetry.strict_full_restarts = adapter.strict_full_restarts;
    telemetry.strict_partition_updates = adapter.strict_partition_updates;
    telemetry.strict_frontier_insertions =
        adapter.strict_frontier_insertions;
    telemetry.strict_frontier_states_inserted =
        adapter.strict_frontier_states_inserted;
    telemetry.strict_frontier_update_max_ns =
        adapter.strict_frontier_update_max_ns;
    telemetry.strict_cells_retained = adapter.strict_cells_retained;
    telemetry.strict_cells_created = adapter.strict_cells_created;
    telemetry.strict_cells_superseded = adapter.strict_cells_superseded;
    telemetry.exact_states = adapter.strict_carriers_materialized;
    telemetry.exact_transitions = adapter.strict_transitions_built;
    telemetry.exact_kernels = adapter.strict_kernels_built;
    telemetry.exact_kernel_cache_hits = adapter.strict_kernel_cache_hits;
    telemetry.selected_rows_begun = adapter.selected_rows_begun;
    telemetry.selected_rows_completed = adapter.selected_rows_completed;
    telemetry.selected_reforge_work = adapter.selected_reforge_work;
    telemetry.selected_transitions = adapter.selected_transitions;
    telemetry.alternative_rows_begun = adapter.alternative_rows_begun;
    telemetry.alternative_rows_completed =
        adapter.alternative_rows_completed;
    telemetry.alternative_reforge_work = adapter.alternative_reforge_work;
    telemetry.alternative_transitions = adapter.alternative_transitions;
    telemetry.strict_reforge_active_work = adapter.strict_reforge_work;
    telemetry.strict_reforge_logical_work_v1 =
        adapter.strict_reforge_logical_work_v1;
    telemetry.strict_reforge_evaluator_work_v1 =
        adapter.strict_reforge_evaluator_work_v1;
    telemetry.strict_reforge_evaluator_work_v2 =
        adapter.strict_reforge_evaluator_work_v2;
    telemetry.strict_reforge_evaluator_work_v3 =
        adapter.strict_reforge_evaluator_work_v3;
    telemetry.work_to_first_partition = adapter.work_to_first_partition;
    telemetry.work_to_first_executable_upper =
        adapter.work_to_first_executable_upper;
    telemetry.wall_ns_to_first_partition =
        adapter.wall_ns_to_first_partition;
    telemetry.wall_ns_to_first_executable_upper =
        adapter.wall_ns_to_first_executable_upper;
    telemetry.alternatives_materialized_before_first_upper =
        adapter.alternatives_materialized_before_first_upper;
    telemetry.alternative_obligations_created =
        adapter.alternative_obligations_created;
    telemetry.unresolved_alternative_obligations =
        adapter.unresolved_alternative_obligations;
    telemetry.alternative_rows_avoided = adapter.alternative_rows_avoided;
    telemetry.action_accounting_complete =
        adapter.action_accounting_complete;
    telemetry.alternative_scheduling_rounds =
        adapter.alternative_scheduling_rounds;
    telemetry.alternative_obligations_scheduled =
        adapter.alternative_obligations_scheduled;
    telemetry.alternative_obligations_certified =
        adapter.alternative_obligations_certified;
    telemetry.alternative_obligations_partially_evaluated =
        adapter.alternative_obligations_partially_evaluated;
    telemetry.alternative_obligations_noncompetitive =
        adapter.alternative_obligations_noncompetitive;
    telemetry.alternative_obligations_stale =
        adapter.alternative_obligations_stale;
    telemetry.alternative_verdict_revocations =
        adapter.alternative_verdict_revocations;
    telemetry.alternative_obligations_resource_interrupted =
        adapter.alternative_obligations_resource_interrupted;
    telemetry.competitive_alternatives_remaining =
        adapter.competitive_alternatives_remaining;
    telemetry.alternative_policy_improvements =
        adapter.alternative_policy_improvements;
    telemetry.bounded_publication_retained =
        adapter.bounded_publication_retained;
    telemetry.exact_alternative_envelope_closed =
        adapter.exact_alternative_envelope_closed;
    telemetry.local_reoptimization_rounds =
        adapter.local_reoptimization_rounds;
    telemetry.local_state_action_rows_scheduled =
        adapter.local_state_action_rows_scheduled;
    telemetry.local_state_action_rows_evaluated =
        adapter.local_state_action_rows_evaluated;
    telemetry.local_reoptimizations = adapter.local_reoptimizations;
    telemetry.local_policy_changes = adapter.local_policy_changes;
    telemetry.local_value_changes = adapter.local_value_changes;
    telemetry.proof_payload_reuses = adapter.proof_payload_reuses;
    telemetry.row_reprojections = adapter.row_reprojections;
    telemetry.quotient_source_splits = adapter.quotient_source_splits;
    telemetry.quotient_target_splits = adapter.quotient_target_splits;
    telemetry.reverse_invalidations = adapter.reverse_invalidations;
    telemetry.improper_policy_repairs = adapter.improper_policy_repairs;
    telemetry.exact_carriers_replayed = adapter.exact_carriers_replayed;
    telemetry.current_live_slices = adapter.current_live_slices;
    telemetry.peak_live_slices = adapter.peak_live_slices;
    telemetry.current_live_slice_bytes = adapter.current_live_slice_bytes;
    telemetry.peak_live_slice_bytes = adapter.peak_live_slice_bytes;
    telemetry.coverage_descriptor_bytes =
        adapter.coverage_descriptor_bytes;
    telemetry.certificate_bytes = adapter.certificate_bytes;
    telemetry.dependency_sidecar_bytes = adapter.dependency_sidecar_bytes;
    telemetry.alternative_obligation_bytes =
        adapter.alternative_obligation_bytes;
    telemetry.partition_bytes = adapter.partition_bytes;
    telemetry.carrier_bytes = adapter.carrier_bytes;
    telemetry.row_kernel_bytes = adapter.row_kernel_bytes;
    telemetry.scratch_bytes = adapter.scratch_bytes;
    telemetry.total_solver_owned_bytes = adapter.total_solver_owned_bytes;
    telemetry.quotient_attractor_ns = adapter.quotient_attractor_ns;
    telemetry.quotient_lower_relaxation_ns =
        adapter.quotient_lower_relaxation_ns;
    telemetry.quotient_policy_seed_ns = adapter.quotient_policy_seed_ns;
    telemetry.quotient_envelope_construction_ns =
        adapter.quotient_envelope_construction_ns;
    telemetry.quotient_exact_policy_evaluation_ns =
        adapter.quotient_exact_policy_evaluation_ns;
    telemetry.quotient_policy_improvement_ns =
        adapter.quotient_policy_improvement_ns;
    telemetry.quotient_publication_audit_ns =
        adapter.quotient_publication_audit_ns;
    telemetry.reference_adapter_invocations =
        adapter.reference_adapter_invocations;
}

const char* solve_detail::publication_invariant_invalid_reason(
        const SolveResult& result) {
    const bool finite_upper = std::isfinite(result.upper_bound);
    const bool has_artifact =
        !result.refined_policy_artifact.strategy_json.empty();
    const bool equal_finite_bounds =
        finite_upper && std::isfinite(result.lower_bound) &&
        std::abs(result.upper_bound - result.lower_bound) <=
            1e-9 * std::max(1.0, std::abs(result.upper_bound));
    if (finite_upper && (!result.policy_available || !has_artifact)) {
        return "finite certified upper bound has no executable artifact";
    }
    if (result.policy_available && !has_artifact) {
        return "published executable policy has no retained exact artifact";
    }
    if (finite_upper &&
        (!std::isfinite(result.evaluated_policy_cost) ||
         std::abs(
             result.upper_bound - result.evaluated_policy_cost) >
             result.options.epsilon *
                 std::max(1.0, std::abs(result.upper_bound)) * 10.0)) {
        return "finite upper bound differs from final graph evaluated cost";
    }
    if (finite_upper &&
        (!std::isfinite(result.lower_bound) ||
         result.lower_bound < 0.0 ||
         result.lower_bound > result.upper_bound)) {
        return "published bounds are not normalized";
    }
    if (result.policy_status == SolvePolicyStatus::Exact &&
        !equal_finite_bounds) {
        return "exact policy status has no closed certified bounds";
    }
    if (result.policy_status == SolvePolicyStatus::Exact &&
        !result.converged) {
        return "exact policy status has no global lower-bound closure";
    }
    return nullptr;
}

void solve_detail::normalize_publication_result(SolveResult& result) {
    if (!std::isfinite(result.upper_bound)) {
        result.absolute_optimality_gap = kInfinity;
        result.relative_optimality_gap = kInfinity;
        return;
    }
    const double tolerance =
        result.options.epsilon *
        std::max(1.0, std::abs(result.upper_bound)) * 10.0;
    if (!std::isfinite(result.lower_bound) ||
        result.lower_bound < 0.0) {
        result.lower_bound = 0.0;
    } else if (result.lower_bound > result.upper_bound) {
        result.lower_bound =
            result.lower_bound <= result.upper_bound + tolerance
                ? result.upper_bound
                : 0.0;
    }
    if (result.policy_status != SolvePolicyStatus::Exact &&
        std::abs(result.upper_bound - result.lower_bound) <= tolerance) {
        /* A bounded label means global lower closure was not established.
         * Do not render an equality-grade 1.00x claim from that scalar. */
        result.lower_bound = 0.0;
    }
    result.absolute_optimality_gap =
        result.upper_bound - result.lower_bound;
    result.relative_optimality_gap =
        result.lower_bound > 0.0
            ? result.upper_bound / result.lower_bound - 1.0
            : kInfinity;
}

void SolveWork::Impl::count_policy_actions(
        const std::vector<std::uint64_t>& rows,
        const std::vector<std::uint32_t>* frontier,
        std::map<std::string, std::uint64_t>& counts) const {
        counts.clear();
        const std::uint64_t no_row =
            std::numeric_limits<std::uint64_t>::max();
        const std::size_t state_count = std::max(
            rows.size(), frontier == nullptr ? 0 : frontier->size());
        for (std::uint32_t state = 0; state < state_count; ++state) {
            if (state < calc.state_count() &&
                calc.is_goal_state(calc.state(state))) {
                continue;
            }
            std::uint32_t operator_index = kNoId;
            const std::uint64_t row =
                state < rows.size() ? rows[state] : no_row;
            if (row != no_row && row < priced_rows.size()) {
                operator_index = priced_rows[row].operator_index;
            } else if (frontier != nullptr && state < frontier->size()) {
                operator_index = (*frontier)[state];
            }
            if (operator_index == kNoId ||
                operator_index >= calc.operators().size()) {
                continue;
            }
            ++counts[calc.operators()[operator_index].id];
        }
    }

void SolveWork::Impl::count_policy_actions(
        const std::vector<PolicyOperatorRef>& policy,
        std::map<std::string, std::uint64_t>& counts) const {
        counts.clear();
        for (std::uint32_t state = 0; state < policy.size(); ++state) {
            if (state < calc.state_count() &&
                calc.is_goal_state(calc.state(state))) {
                continue;
            }
            const std::uint32_t operator_index = policy[state].index;
            if (operator_index == kNoId ||
                operator_index >= calc.operators().size()) {
                continue;
            }
            ++counts[calc.operators()[operator_index].id];
        }
    }

solve_detail::CooperativeTask<SolveResult>
SolveWork::Impl::run_finalization() {
        if (phase != SolvePhase::Refining &&
            phase != SolvePhase::Compiling &&
            phase != SolvePhase::Certifying) {
            throw std::logic_error("solver finalization was not started");
        }
        if (consumed) {
            throw std::logic_error("solver work was already finished");
        }
        co_await solve_detail::CooperativeCheckpoint{};
        const auto extraction_started = std::chrono::steady_clock::now();
        if (result.diagnostics.resource_cap_hit ||
            (requested_bounded_finish && output_incumbent.has_value())) {
            (void)try_install_reachable_incumbent(
                !requested_bounded_finish);
        }
        const auto finalize_diagnostic =
            [&](auto&& finalizer) {
                try {
                    finalizer();
                } catch (const SolverResourceLimit& limit) {
                    /*
                     * Finalization diagnostics are observational. A solve
                     * that already exhausted an exact-kernel budget can
                     * revisit automatic-candidate/provenance helpers here;
                     * keep the cap as the public stop instead of escaping
                     * through solve_finish as an internal error.
                     */
                    record_cap(
                        limit.cap_name(),
                        limit.cap_name() == "max_discovered_states");
                }
            };
        finalize_diagnostic(
            [&] { finalize_incremental_diagnostics(); });
        /*
         * Capture provenance while the incumbent still owns its complete
         * selected-row/value witness. Keep the optional strings outside
         * SolveDiagnostics until final accounting is frozen.
         */
        finalize_diagnostic(
            [&] { finalize_upper_policy_provenance(); });
        finalize_diagnostic(
            [&] { finalize_upper_cap_zero_progress_audit(); });
        std::vector<std::string> upper_policy_provenance_samples =
            std::move(
                result.diagnostics.upper_policy_provenance_samples);
        std::string upper_cap_zero_progress_audit_json =
            std::move(
                result.diagnostics
                    .upper_cap_zero_progress_audit_json);
        const std::uint64_t
            upper_policy_provenance_samples_omitted =
                result.diagnostics
                    .upper_policy_provenance_samples_omitted;
        const std::uint64_t
            upper_policy_provenance_candidate_count =
                result.diagnostics
                    .upper_policy_provenance_candidate_count;
        const std::uint64_t
            upper_policy_provenance_retained_bytes =
                result.diagnostics
                    .upper_policy_provenance_retained_bytes;
        result.diagnostics.upper_policy_provenance_samples_omitted = 0;
        result.diagnostics.upper_policy_provenance_candidate_count = 0;
        result.diagnostics.upper_policy_provenance_retained_bytes = 0;
        result.diagnostics.upper_cap_zero_progress_audit_json.clear();

        const bool sweep_cap_hit =
            sweeps >= options.max_sweeps && !optimization_converged();
        if (sweep_cap_hit) record_cap("max_sweeps");
        /* Ordinary non-convergence may leave the executable incumbent only
         * in the retained portfolio. Promote the best structurally valid
         * artifact only when the normal output slot is empty. If a current
         * incumbent exists, preserve both for final evaluated-cost selection
         * instead of displacing either by an unverified estimate. */
        if (!certified_fallback_portfolio.empty()) {
            auto best_retained = std::min_element(
                certified_fallback_portfolio.begin(),
                certified_fallback_portfolio.end(),
                [&](const BoundedPolicyIncumbent& left,
                    const BoundedPolicyIncumbent& right) {
                    return incumbent_precedes(left, right);
                });
            if (!output_incumbent.has_value()) {
                output_incumbent = std::move(*best_retained);
                certified_fallback_portfolio.erase(best_retained);
            }
            PolicyRefinementTelemetry& portfolio =
                result.diagnostics.policy_refinement;
            portfolio.fallback_portfolio_candidates =
                certified_fallback_portfolio.size();
            portfolio.fallback_portfolio_owned_bytes = 0;
            for (const BoundedPolicyIncumbent& retained :
                 certified_fallback_portfolio) {
                portfolio.fallback_portfolio_owned_bytes +=
                    incumbent_owned_bytes(retained);
            }
        }
        bool pre_extraction_full_non_goal_closure = true;
        for (std::uint32_t state = 0; state < result.values.size(); ++state) {
            if (!result.behavioral_representative_by_state.empty() &&
                result.behavioral_representative_by_state[state] != state) {
                continue;
            }
            if (!result.expanded[state] && !result.goal_states[state]) {
                pre_extraction_full_non_goal_closure = false;
                break;
            }
        }
        const bool current_policy_can_still_be_exact =
            !requested_bounded_finish &&
            optimization_converged() &&
            (!incremental_action_generation ||
             incremental_envelope_closed) &&
            !result.diagnostics.state_cap_hit &&
            !result.diagnostics.resource_cap_hit &&
            (!result.diagnostics.focused_expansion ||
             focused_bound_proved ||
             full_closure_after_focused_fallback ||
             pre_extraction_full_non_goal_closure ||
             (focused_closure_proved &&
              result.diagnostics.focused_optimality_gap <=
                  exact_gap_proof_tolerance()));
        const bool coarse_discovery_closed =
            !requested_bounded_finish &&
            (!incremental_action_generation ||
             incremental_envelope_closed) &&
            !result.diagnostics.state_cap_hit &&
            !result.diagnostics.resource_cap_hit &&
            (!result.diagnostics.focused_expansion ||
             focused_bound_proved ||
             full_closure_after_focused_fallback ||
             pre_extraction_full_non_goal_closure ||
             (focused_closure_proved &&
              result.diagnostics.focused_optimality_gap <=
                  exact_gap_proof_tolerance()));
        const bool restore_output_incumbent =
            output_incumbent.has_value() &&
            !current_policy_can_still_be_exact;
        {
            const auto snapshot_started =
                std::chrono::steady_clock::now();
            PolicyRefinementTelemetry& telemetry =
                result.diagnostics.policy_refinement;
            const std::uint64_t no_row =
                std::numeric_limits<std::uint64_t>::max();
            telemetry.pre_restore_policy_present =
                restore_output_incumbent &&
                result.start_state < result.values.size() &&
                result.start_state < policy_rows.size() &&
                policy_rows[result.start_state] != no_row;
            telemetry.pre_restore_policy_numerical_stop =
                telemetry.pre_restore_policy_present &&
                numerical_stability_stop;
            if (telemetry.pre_restore_policy_present) {
                telemetry.pre_restore_policy_start_value =
                    result.values[result.start_state];
                telemetry.pre_restore_policy_residual = residual;
                telemetry.pre_restore_policy_goal_identity =
                    goal_identity();
                telemetry.pre_restore_policy_economy_identity =
                    economy_identity();
                telemetry
                    .pre_restore_policy_action_vocabulary_identity =
                    action_vocabulary_identity();
                telemetry.pre_restore_policy_graph_identity =
                    graph_identity();
                telemetry.pre_restore_policy_artifact_identity =
                    artifact_identity();
                telemetry.pre_restore_policy_source_generation =
                    transition_cache->rows.size();
                telemetry.pre_restore_policy_target_generation =
                    calc.state_count();

                std::uint64_t policy_hash = 1469598103934665603ULL;
                const auto mix = [&policy_hash](const std::uint64_t value) {
                    policy_hash ^= value;
                    policy_hash *= 1099511628211ULL;
                };
                for (std::size_t state = 0;
                     state < policy_rows.size(); ++state) {
                    const std::uint64_t row = policy_rows[state];
                    mix(state);
                    mix(row);
                    if (row == no_row) continue;
                    ++telemetry.pre_restore_policy_selected_rows;
                    if (row < priced_rows.size()) {
                        mix(priced_rows[row].operator_index);
                        mix(std::bit_cast<std::uint64_t>(
                            priced_rows[row].cost));
                    }
                }
                telemetry.pre_restore_policy_bits_hash = policy_hash;

                const std::size_t state_count = result.values.size();
                std::vector<std::uint8_t> reachable(state_count, 0);
                std::vector<std::uint32_t> pending;
                pending.reserve(state_count);
                std::vector<std::uint8_t> action_seen(
                    calc.operators().size(), 0);
                telemetry.pre_restore_policy_snapshot_peak_bytes =
                    reachable.capacity() * sizeof(std::uint8_t) +
                    pending.capacity() * sizeof(std::uint32_t) +
                    action_seen.capacity() * sizeof(std::uint8_t);
                pending.push_back(result.start_state);
                reachable[result.start_state] = 1;
                bool materializable = true;
                for (std::size_t cursor = 0;
                     cursor < pending.size(); ++cursor) {
                    const std::uint32_t state = pending[cursor];
                    if (state >= state_count) {
                        materializable = false;
                        continue;
                    }
                    ++telemetry.pre_restore_policy_reachable_states;
                    if (calc.is_goal_state(calc.state(state))) continue;
                    if (state >= policy_rows.size()) {
                        materializable = false;
                        continue;
                    }
                    const std::uint64_t row_index = policy_rows[state];
                    if (row_index == no_row ||
                        row_index >= transition_cache->rows.size() ||
                        row_index >= priced_rows.size()) {
                        materializable = false;
                        continue;
                    }
                    const SparseRow& row =
                        transition_cache->rows[row_index];
                    const PricedSparseRow& priced =
                        priced_rows[row_index];
                    if (!row.admitted ||
                        priced.operator_index >= calc.operators().size()) {
                        materializable = false;
                        continue;
                    }
                    ++telemetry.pre_restore_policy_reachable_rows;
                    if (!action_seen[priced.operator_index]) {
                        action_seen[priced.operator_index] = 1;
                        ++telemetry.pre_restore_policy_distinct_actions;
                    }
                    telemetry.pre_restore_policy_choice_groups +=
                        row.choice_count;
                    telemetry.pre_restore_policy_choice_options +=
                        priced.choice_option_count;
                    for (std::uint32_t i = 0;
                         i < row.transition_count; ++i) {
                        const std::uint64_t offset =
                            row.transition_offset + i;
                        if (transition_cache->probabilities.at(offset) <=
                            0.0) {
                            continue;
                        }
                        const std::uint32_t successor =
                            transition_cache->successors.at(offset);
                        if (successor >= state_count) {
                            materializable = false;
                        } else if (!reachable[successor]) {
                            reachable[successor] = 1;
                            pending.push_back(successor);
                        }
                    }
                    for (std::uint32_t i = 0;
                         i < row.choice_count; ++i) {
                        const SparseChoiceGroup& choice =
                            transition_cache->choices.at(
                                row.choice_offset + i);
                        const std::uint32_t successor =
                            select_sparse_policy_choice_successor(
                                *transition_cache, choice, state,
                                result.values);
                        if (successor == kNoId ||
                            successor >= state_count) {
                            materializable = false;
                        } else if (!reachable[successor]) {
                            reachable[successor] = 1;
                            pending.push_back(successor);
                        }
                    }
                }
                telemetry.pre_restore_policy_materializable =
                    materializable;
                telemetry.selected_candidate_capture_attempted = true;
                telemetry.selected_candidate_estimated_cost =
                    telemetry.pre_restore_policy_start_value;
                if (materializable) {
                    const auto capture_started =
                        std::chrono::steady_clock::now();
                    UnverifiedSelectedPolicyCandidate selected;
                    BoundedPolicyIncumbent& snapshot = selected.snapshot;
                    selected.has_exact_start_item =
                        result.has_exact_start_item;
                    selected.exact_start_item = result.exact_start_item;
                    selected.selected_estimate =
                        telemetry.pre_restore_policy_start_value;
                    selected.selected_policy_hash = policy_hash;
                    selected.numerical_stability_stop =
                        numerical_stability_stop;
                    snapshot.certified_upper_bound =
                        selected.selected_estimate;
                    snapshot.evaluated_policy_cost = kInfinity;
                    snapshot.values = result.values;
                    snapshot.policy_rows = policy_rows;
                    snapshot.policy_rows.resize(
                        snapshot.values.size(), no_row);
                    snapshot.policy_reachable = std::move(reachable);
                    snapshot.behavioral_representative_by_state =
                        result.behavioral_representative_by_state;
                    snapshot.restart_operator = restart_operator_index;
                    snapshot.restart_state = restart_state;
                    snapshot.round =
                        result.diagnostics.focused_expansion_rounds;
                    snapshot.kind = "selected_coarse_policy";
                    snapshot.goal_identity = goal_identity();
                    snapshot.economy_identity = economy_identity();
                    snapshot.action_vocabulary_identity =
                        action_vocabulary_identity();
                    snapshot.action_vocabulary_size = operators.size();
                    snapshot.graph_identity = graph_identity();
                    snapshot.artifact_identity = artifact_identity();
                    snapshot.source_generation =
                        transition_cache->rows.size();
                    snapshot.target_generation = calc.state_count();
                    snapshot.graph_row_count =
                        transition_cache->rows.size();
                    snapshot.graph_priced_row_count = priced_rows.size();
                    snapshot.graph_successor_count =
                        transition_cache->successors.size();
                    snapshot.graph_probability_count =
                        transition_cache->probabilities.size();
                    snapshot.graph_choice_count =
                        transition_cache->choices.size();
                    snapshot.graph_choice_successor_count =
                        transition_cache->choice_successors.size();
                    snapshot.graph_choice_option_count =
                        transition_cache->choice_options.size();
                    snapshot.graph_prefix_identity =
                        incumbent_graph_prefix_identity(
                            snapshot.graph_row_count,
                            snapshot.graph_priced_row_count,
                            snapshot.graph_successor_count,
                            snapshot.graph_probability_count,
                            snapshot.graph_choice_count,
                            snapshot.graph_choice_successor_count,
                            snapshot.graph_choice_option_count);
                    snapshot.strict_state_provenance =
                        result.behavioral_representative_by_state.empty();
                    snapshot.policy_materialized = false;
                    snapshot.independently_certified = false;
                    snapshot.independently_evaluated = false;
                    snapshot.proper = false;
                    snapshot.executable = false;
                    try {
                        capture_incumbent_policy(snapshot);
                        std::uint64_t identity =
                            1469598103934665603ULL;
                        identity_mix(identity, selected.selected_policy_hash);
                        identity_mix(identity, snapshot.goal_identity);
                        identity_mix(identity, snapshot.economy_identity);
                        identity_mix(
                            identity,
                            snapshot.action_vocabulary_identity);
                        identity_mix(identity, snapshot.artifact_identity);
                        identity_mix(
                            identity, snapshot.graph_prefix_identity);
                        identity_mix(identity, snapshot.source_generation);
                        identity_mix(identity, snapshot.target_generation);
                        identity_mix_string(identity, snapshot.kind);
                        selected.capture_identity = identity;
                        snapshot.portfolio_identity = identity;
                        const std::uint64_t candidate_bytes =
                            incumbent_owned_bytes(snapshot) -
                            sizeof(BoundedPolicyIncumbent);
                        const std::uint64_t live_bytes =
                            fast_estimated_owned_bytes();
                        const std::uint64_t transient_bytes =
                            telemetry.pre_restore_policy_snapshot_peak_bytes;
                        const std::uint64_t current_with_transient =
                            transient_bytes >
                                    std::numeric_limits<std::uint64_t>::max() -
                                        live_bytes
                                ? std::numeric_limits<std::uint64_t>::max()
                                : live_bytes + transient_bytes;
                        if (!certified_fallback_fits_memory(
                                current_with_transient, candidate_bytes,
                                options.max_solver_owned_bytes)) {
                            telemetry.selected_candidate_memory_rejected =
                                true;
                            telemetry.selected_candidate_status =
                                "capture_memory_rejected";
                            telemetry.selected_candidate_failure_reason =
                                "selected policy snapshot does not fit "
                                "max_solver_owned_bytes";
                        } else {
                            snapshot.retained_owned_bytes =
                                incumbent_owned_bytes(snapshot);
                            telemetry.selected_candidate_owned_bytes =
                                snapshot.retained_owned_bytes;
                            telemetry.selected_candidate_identity =
                                selected.capture_identity;
                            unverified_selected_policy_candidate =
                                std::move(selected);
                            telemetry.selected_candidate_captured = true;
                            telemetry.selected_candidate_identity_valid =
                                true;
                            telemetry.selected_candidate_status =
                                "captured_unverified";
                            peak_owned_bytes = std::max(
                                peak_owned_bytes,
                                fast_estimated_owned_bytes());
                        }
                    } catch (const std::exception& error) {
                        telemetry.selected_candidate_status =
                            "capture_failed";
                        telemetry.selected_candidate_failure_reason =
                            error.what();
                    }
                    telemetry.selected_candidate_capture_ns =
                        static_cast<std::uint64_t>(
                            std::chrono::duration_cast<
                                std::chrono::nanoseconds>(
                                std::chrono::steady_clock::now() -
                                capture_started)
                                .count());
                } else {
                    telemetry.selected_candidate_status =
                        "not_materializable";
                    telemetry.selected_candidate_failure_reason =
                        "selected row policy is not structurally "
                        "materializable";
                }
            }
            telemetry.pre_restore_policy_snapshot_ns =
                static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now() -
                        snapshot_started)
                        .count());
        }
        const auto extraction_materialization_started =
            std::chrono::steady_clock::now();
        std::vector<double> restored_policy_row_costs;
        std::vector<std::uint8_t> restored_policy_reachable;
        bool explicit_restored_policy_reachable = false;
        if (restore_output_incumbent) {
            const auto restore_started =
                std::chrono::steady_clock::now();
            BoundedPolicyIncumbent& incumbent = *output_incumbent;
            count_policy_actions(
                policy_rows, nullptr,
                result.diagnostics.lower_policy_action_states);
            count_policy_actions(
                incumbent.policy,
                result.diagnostics.upper_policy_action_states);
            populate_incumbent_policy(incumbent);
            result.values = std::move(incumbent.values);
            result.policy = std::move(incumbent.policy);
            result.unveil_preferences =
                std::move(incumbent.unveil_preferences);
            result.option_unveil_preferences =
                std::move(incumbent.option_unveil_preferences);
            owned_result_nested_bytes = 0;
            for (const auto& preferences :
                 result.unveil_preferences) {
                owned_result_nested_bytes +=
                    preferences.capacity() * sizeof(std::uint32_t);
            }
            for (const auto& preferences :
                 result.option_unveil_preferences) {
                owned_result_nested_bytes += preferences.capacity() *
                    sizeof(ObservedUnveilPreference);
                for (const auto& preference : preferences) {
                    owned_result_nested_bytes +=
                        preference.choices.capacity() *
                        sizeof(ObservedUnveilChoice);
                }
            }
            result.behavioral_representative_by_state =
                std::move(incumbent.behavioral_representative_by_state);
            explicit_restored_policy_reachable =
                !incumbent.policy_reachable.empty();
            restored_policy_reachable =
                std::move(incumbent.policy_reachable);
            result.primitive_renewal_witness =
                std::move(incumbent.primitive_renewal_witness);
            if (result.primitive_renewal_witness.valid) {
                const PrimitiveRenewalWitness& witness =
                    result.primitive_renewal_witness;
                const bool witness_matches =
                    result.start_state < result.policy.size() &&
                    witness.operator_index < calc.operators().size() &&
                    result.policy[result.start_state].index ==
                        witness.operator_index &&
                    result.policy[result.start_state].kind ==
                        calc.operators()[witness.operator_index].kind &&
                    calc.operators()[witness.operator_index]
                            .primitive_action ==
                        witness.primitive_action;
                if (!witness_matches) {
                    result.primitive_renewal_witness =
                        PrimitiveRenewalWitness{};
                }
            }
            policy_rows = std::move(incumbent.policy_rows);
            restored_policy_row_costs =
                std::move(incumbent.policy_row_costs);
            const std::size_t state_count = result.values.size();
            result.goal_states.assign(state_count, 0);
            for (std::uint32_t state = 0; state < state_count; ++state) {
                if (calc.is_goal_state(calc.state(state))) {
                    result.goal_states[state] = 1;
                }
            }
            if (!result.behavioral_representative_by_state.empty()) {
                if (result.behavioral_representative_by_state.size() !=
                    state_count) {
                    throw std::logic_error(
                        "bounded incumbent quotient provenance size changed");
                }
                for (std::uint32_t state = 0; state < state_count; ++state) {
                    const std::uint32_t representative =
                        result.behavioral_representative_by_state[state];
                    if (representative >= state_count ||
                        result.policy[state] != result.policy[representative] ||
                        result.unveil_preferences[state] !=
                            result.unveil_preferences[representative] ||
                        result.option_unveil_preferences[state] !=
                            result.option_unveil_preferences[representative]) {
                        throw std::logic_error(
                            "bounded incumbent quotient lift changed policy "
                            "or preferences");
                    }
                }
            }
            result.diagnostics.policy_refinement.incumbent_restore_ns =
                static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now() -
                        restore_started)
                        .count());
        }

        const std::uint32_t state_count =
            static_cast<std::uint32_t>(result.values.size());
        /*
         * A resource cap can now be raised by focused/automatic kernel work
         * after it has interned successors but before prepare_iteration()
         * synchronizes every result vector. Finalization must still produce
         * an analyzable no-policy result instead of indexing the shorter
         * pre-cap arrays.
         */
        if (result.expanded.size() < state_count) {
            result.expanded.resize(state_count, 0);
        }
        const std::size_t prior_goal_count = result.goal_states.size();
        if (prior_goal_count < state_count) {
            result.goal_states.resize(state_count, 0);
            for (std::uint32_t state =
                     static_cast<std::uint32_t>(prior_goal_count);
                 state < state_count; ++state) {
                if (calc.is_goal_state(calc.state(state))) {
                    result.goal_states[state] = 1;
                    result.values[state] = 0.0;
                    ++result.diagnostics.goal_states;
                }
            }
        }
        if (result.policy.size() < state_count) {
            result.policy.resize(state_count, PolicyOperatorRef{});
        }
        if (result.unveil_preferences.size() < state_count) {
            result.unveil_preferences.resize(state_count);
        }
        if (result.option_unveil_preferences.size() < state_count) {
            result.option_unveil_preferences.resize(state_count);
        }
        finalize_preservation_diagnostics();
        const std::uint64_t extraction_base_bytes = estimated_owned_bytes();
        bool finalization_capped =
            check_solver_byte_cap_from(extraction_base_bytes);
        /* Final exact extraction uses the same strict row argmin as Howard
         * selection. Expected cost is the objective; stable row identity is
         * the only exact-tie breaker. */
        const bool authoritative_policy_available =
            !result.diagnostics.state_cap_hit &&
            !result.diagnostics.resource_cap_hit &&
            !focused_bound_proved && !restore_output_incumbent;
        for (std::uint32_t state = 0;
             authoritative_policy_available && state < state_count; ++state) {
            if (finalization_capped) break;
            if (!result.expanded[state] || result.goal_states[state]) continue;
            const SparsePolicyRowSelection selected =
                select_sparse_policy_row(
                    *transition_cache, state,
                    [&](const std::uint64_t row) {
                        return transition_cache->rows.at(row).admitted &&
                               !preservation_prunes(row);
                    },
                    [&](const std::uint64_t row,
                        std::uint32_t& work) {
                        return sparse_row_q(row, work);
                    });
            result.diagnostics.extraction_action_evaluations +=
                selected.evaluated_rows;
            const std::uint64_t best_row_index = selected.row;
            const std::uint32_t best_operator =
                best_row_index ==
                        std::numeric_limits<std::uint64_t>::max()
                    ? kNoId
                    : priced_rows.at(best_row_index).operator_index;
            result.policy[state] =
                best_operator == kNoId
                    ? PolicyOperatorRef{}
                    : PolicyOperatorRef{
                          calc.operators()[best_operator].kind,
                          best_operator};
            const bool selected_primitive =
                best_operator != kNoId &&
                best_row_index !=
                    std::numeric_limits<std::uint64_t>::max() &&
                calc.operators()[best_operator].kind ==
                    PlannerOperatorKind::Primitive;
            const bool primitive_observes_modifier_offer =
                selected_primitive &&
                action_observes_modifier_offer(
                    calc.registry().actions.at(
                        calc.operators()[best_operator]
                            .primitive_action));
            if (selected_primitive &&
                (transition_cache->rows[best_row_index].choice_count != 0 ||
                 priced_rows[best_row_index].choice_option_count != 0) &&
                !primitive_observes_modifier_offer) {
                throw std::logic_error(
                    "primitive exact row exposes observed choices without "
                    "an admitted outcome-observation contract");
            }
            if (primitive_observes_modifier_offer) {
                std::vector<OutcomeChoiceOption> choice_options;
                const PricedSparseRow& best_row = priced_rows.at(
                    best_row_index);
                if (check_solver_byte_cap(
                        static_cast<std::uint64_t>(
                            best_row.choice_option_count) *
                        sizeof(OutcomeChoiceOption))) {
                    finalization_capped = true;
                    break;
                }
                for (std::uint32_t i = 0;
                     i < best_row.choice_option_count; ++i) {
                    choice_options.push_back(
                        transition_cache->choice_options.at(
                            best_row.choice_option_offset + i));
                }
                order_observed_modifier_choices(
                    calc.registry().actions.at(
                        calc.operators()[best_operator]
                            .primitive_action),
                    choice_options, result.values);
                for (const OutcomeChoiceOption& option : choice_options) {
                    auto& preferences = result.unveil_preferences[state];
                    const std::size_t old_capacity = preferences.capacity();
                    preferences.push_back(option.mod_id);
                    owned_result_nested_bytes +=
                        (preferences.capacity() - old_capacity) *
                        sizeof(std::uint32_t);
                }
            } else if (best_operator != kNoId &&
                       best_row_index !=
                           std::numeric_limits<std::uint64_t>::max() &&
                       calc.operators()[best_operator].kind ==
                           PlannerOperatorKind::FixedOption &&
                       priced_rows[best_row_index].choice_option_count != 0) {
                const PlannerOperator& selected =
                    calc.operators()[best_operator];
                if (selected.primitive_program.empty() ||
                    selected.primitive_program.back() >=
                        calc.registry().actions.size() ||
                    !action_observes_modifier_offer(
                        calc.registry().actions[
                            selected.primitive_program.back()])) {
                    throw std::logic_error(
                        "fixed exact row exposes observed choices without "
                        "an admitted outcome-observation contract");
                }
                std::map<std::uint32_t, std::vector<OutcomeChoiceOption>>
                    by_observation;
                const PricedSparseRow& best_row = priced_rows.at(
                    best_row_index);
                if (check_solver_byte_cap(
                        static_cast<std::uint64_t>(
                            best_row.choice_option_count) * 128ull)) {
                    finalization_capped = true;
                    break;
                }
                for (std::uint32_t i = 0;
                     i < best_row.choice_option_count; ++i) {
                    const OutcomeChoiceOption& choice =
                        transition_cache->choice_options.at(
                            best_row.choice_option_offset + i);
                    by_observation[choice.observation_state].push_back(
                        choice);
                }
                for (auto& [observation_state, choices] : by_observation) {
                    order_observed_modifier_choices(
                        calc.registry().actions.at(
                            selected.primitive_program.back()),
                        choices, result.values);
                    ObservedUnveilPreference preference;
                    preference.observation_state = observation_state;
                    for (const OutcomeChoiceOption& choice : choices) {
                        preference.choices.push_back(
                            {choice.mod_id, choice.state,
                             choice.actual_state});
                    }
                    auto& preferences =
                        result.option_unveil_preferences[state];
                    const std::size_t old_capacity = preferences.capacity();
                    owned_result_nested_bytes +=
                        preference.choices.capacity() *
                        sizeof(ObservedUnveilChoice);
                    preferences.push_back(std::move(preference));
                    owned_result_nested_bytes +=
                        (preferences.capacity() - old_capacity) *
                        sizeof(ObservedUnveilPreference);
                }
            }
        }
        finalize_automatic_candidate_diagnostics();
        finalization_capped =
            check_solver_byte_cap() || finalization_capped;

        if (!finalization_capped &&
            check_solver_byte_cap(
                static_cast<std::uint64_t>(state_count) *
                (sizeof(std::uint8_t) + sizeof(std::uint32_t)))) {
            finalization_capped = true;
        }
        if (!finalization_capped) {
            if (restore_output_incumbent &&
                !restored_policy_reachable.empty()) {
                if (restored_policy_reachable.size() != state_count ||
                    !restored_policy_reachable[result.start_state]) {
                    throw std::logic_error(
                        "bounded incumbent reachability changed before "
                        "finalization");
                }
                result.policy_reachable =
                    std::move(restored_policy_reachable);
            } else {
                result.policy_reachable.assign(state_count, 0);
            }
        } else {
            result.policy_reachable.clear();
        }
        bool reachable_policy_complete = true;
        std::string reachable_policy_failure;
        if (!finalization_capped && restore_output_incumbent) {
            result.diagnostics.policy_reachable_states = 0;
            for (std::uint32_t state = 0; state < state_count; ++state) {
                if (result.policy_reachable.empty()) {
                    throw std::logic_error(
                        "bounded incumbent reachability is unavailable");
                }
                if (explicit_restored_policy_reachable &&
                    !result.policy_reachable[state]) {
                    continue;
                }
                if (!explicit_restored_policy_reachable) {
                    result.policy_reachable[state] = 1;
                }
                ++result.diagnostics.policy_reachable_states;
                if (!result.goal_states[state] &&
                    result.policy[state] == kNoId) {
                    reachable_policy_complete = false;
                    if (reachable_policy_failure.empty()) {
                        reachable_policy_failure =
                            "restored_missing_operator:state=" +
                            std::to_string(state);
                    }
                }
            }
        } else if (!finalization_capped &&
                   result.start_state < state_count) {
            std::deque<std::uint32_t> walk{result.start_state};
            while (!walk.empty()) {
                const std::uint32_t state = walk.front();
                walk.pop_front();
                if (result.policy_reachable[state]) continue;
                result.policy_reachable[state] = 1;
                ++result.diagnostics.policy_reachable_states;
                if (result.goal_states[state]) continue;
                const std::uint32_t operator_index = result.policy[state];
                if (operator_index == kNoId) {
                    reachable_policy_complete = false;
                    if (reachable_policy_failure.empty()) {
                        const AbstractState& missing = calc.state(state);
                        std::uint32_t admitted_rows = 0;
                        std::string admitted_actions;
                        double admitted_q = kInfinity;
                        for (const std::uint64_t row_index :
                             state_row_indices(*transition_cache, state)) {
                            if (transition_cache->rows.at(row_index).admitted) {
                                ++admitted_rows;
                                admitted_q = std::min(
                                    admitted_q,
                                    sparse_row_q_for_values(
                                        row_index, result.values));
                                if (row_index < priced_rows.size()) {
                                    const std::uint32_t row_operator =
                                        priced_rows[row_index].operator_index;
                                    if (!admitted_actions.empty()) {
                                        admitted_actions += ',';
                                    }
                                    admitted_actions +=
                                        row_operator < calc.operators().size()
                                            ? calc.operators()
                                                  .at(row_operator).id
                                            : std::to_string(row_operator);
                                }
                            }
                        }
                        reachable_policy_failure =
                            "missing_operator:state=" +
                            std::to_string(state) + ":influences=" +
                            std::to_string(missing.influence_bits) +
                            ":rarity=" +
                            std::to_string(missing.rarity) +
                            ":prefixes=" +
                            std::to_string(missing.prefix_count) +
                            ":suffixes=" +
                            std::to_string(missing.suffix_count) +
                            ":satisfied=" + std::to_string(
                                satisfied_goal_mask_for_state(state)) +
                            ":value=" + finite_json(result.values[state]) +
                            ":q=" + finite_json(admitted_q) +
                            ":restart_allowed=" +
                            std::to_string(restart_row_allowed(state)) +
                            ":admitted_rows=" +
                            std::to_string(admitted_rows) +
                            ":actions=" + admitted_actions +
                            ":representative=" +
                            std::to_string(
                                result.behavioral_representative_by_state.empty()
                                    ? state
                                    : result
                                          .behavioral_representative_by_state
                                          .at(state));
                    }
                    continue;
                }
                if (operator_index == restart_operator_index &&
                    restart_state != kNoId) {
                    if (!result.policy_reachable[restart_state]) {
                        walk.push_back(restart_state);
                    }
                    continue;
                }
                const SparseRow* selected = nullptr;
                for (const std::uint64_t row_index :
                     state_row_indices(*transition_cache, state)) {
                    const SparseRow& row =
                        transition_cache->rows.at(row_index);
                    if (!row.admitted) continue;
                    if (priced_rows[row_index].operator_index ==
                        operator_index) {
                        selected = &row;
                        break;
                    }
                }
                if (selected == nullptr) {
                    reachable_policy_complete = false;
                    if (reachable_policy_failure.empty()) {
                        reachable_policy_failure =
                            "missing_row:state=" + std::to_string(state) +
                            ":operator=" +
                            std::to_string(operator_index);
                    }
                    continue;
                }
                for (std::uint32_t i = 0;
                     i < selected->transition_count; ++i) {
                    const std::uint32_t successor =
                        transition_cache->successors.at(
                            selected->transition_offset + i);
                    if (successor == state) continue;
                    if (!result.policy_reachable[successor]) {
                        walk.push_back(successor);
                    }
                }
                for (std::uint32_t i = 0; i < selected->choice_count; ++i) {
                    const SparseChoiceGroup& group =
                        transition_cache->choices.at(
                            selected->choice_offset + i);
                    const std::uint32_t chosen =
                        select_sparse_policy_choice_successor(
                            *transition_cache, group, state,
                            result.values);
                    /* An observation choice follows only the policy-selected
                     * successor. Unselected unveil alternatives are not
                     * policy-reachable and may intentionally have no action. */
                    if (chosen != kNoId && chosen != state &&
                        !result.policy_reachable[chosen]) {
                        walk.push_back(chosen);
                    }
                }
            }
        }
        if (finalization_capped) reachable_policy_complete = false;
        result.diagnostics.policy_refinement.extraction_materialization_ns =
            static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() -
                    extraction_materialization_started)
                    .count());

        bool full_non_goal_closure = true;
        for (std::uint32_t state = 0; state < result.values.size(); ++state) {
            if (!result.behavioral_representative_by_state.empty() &&
                result.behavioral_representative_by_state[state] != state) {
                continue;
            }
            if (!result.expanded[state] && !result.goal_states[state]) {
                full_non_goal_closure = false;
                break;
            }
        }
        if (result.diagnostics.focused_expansion &&
            result.start_state < result.values.size()) {
            if (focused_bound_proved ||
                full_closure_after_focused_fallback ||
                full_non_goal_closure ||
                !std::isfinite(result.diagnostics.focused_upper_bound)) {
                result.diagnostics.focused_upper_bound =
                    result.values[result.start_state];
            }
            if (full_closure_after_focused_fallback ||
                full_non_goal_closure) {
                result.diagnostics.focused_lower_bound =
                    result.diagnostics.focused_upper_bound;
                result.diagnostics.focused_optimality_gap = 0.0;
            } else {
                result.diagnostics.focused_optimality_gap =
                    std::max(
                        0.0,
                        result.diagnostics.focused_upper_bound -
                            result.diagnostics.focused_lower_bound);
            }
        }
        const bool focused_exact =
            !result.diagnostics.focused_expansion ||
            full_closure_after_focused_fallback ||
            full_non_goal_closure ||
            (focused_closure_proved &&
             result.diagnostics.focused_optimality_gap <=
                 exact_gap_proof_tolerance());
        result.diagnostics.focused_exact_gap_proof_tolerance =
            exact_gap_proof_tolerance();
        const bool final_optimization_converged = optimization_converged();
        result.converged = focused_exact &&
                           (!incremental_action_generation ||
                            incremental_envelope_closed) &&
                           !result.diagnostics.state_cap_hit &&
                           !result.diagnostics.resource_cap_hit &&
                           final_optimization_converged &&
                           reachable_policy_complete &&
                           result.start_state < state_count &&
                           result.values[result.start_state] < kValueCeiling;
        if (!result.converged &&
            result.diagnostics.policy_evaluation_failure.empty()) {
            result.diagnostics.policy_evaluation_failure =
                "final_convergence_gate:focused_exact=" +
                std::to_string(focused_exact) +
                ",optimization=" +
                std::to_string(final_optimization_converged) +
                ",reachable_policy=" +
                std::to_string(reachable_policy_complete) +
                ",reachable_failure=" +
                (reachable_policy_failure.empty()
                     ? std::string("none")
                     : reachable_policy_failure) +
                ",state_in_range=" +
                std::to_string(result.start_state < state_count) +
                ",finite_start=" + std::to_string(
                    result.start_state < state_count &&
                    result.values[result.start_state] < kValueCeiling);
        }
        if (!result.behavioral_representative_by_state.empty()) {
            for (std::uint32_t state = 0;
                 state < result.behavioral_representative_by_state.size();
                 ++state) {
                const std::uint32_t representative =
                    result.behavioral_representative_by_state[state];
                if (representative == state) continue;
                result.values[state] = result.values[representative];
                result.policy[state] = result.policy[representative];
                result.unveil_preferences[state] =
                    result.unveil_preferences[representative];
                result.option_unveil_preferences[state] =
                    result.option_unveil_preferences[representative];
            }
        }
        /*
         * The deliberately coarse product parent does not retain complete
         * exclusion-group identity. Treat an exact feature observed or
         * preserved by the selected policy as a refinement counterexample.
         * Publication is rejected only for an actual named resource/product
         * limit below; ordinary identity witnesses continue to the shared
         * policy-guided exact evaluator.
         */
        bool executable_policy_abstraction_supported = true;
        if (calc.product_solver_parent()) {
            const auto ambiguous_members =
                [&](const std::vector<std::uint64_t>& members,
                    const std::vector<std::uint64_t>* excluded) {
                    std::optional<std::vector<std::uint64_t>> first;
                    bool ambiguous = false;
                    pc_bitset_for_each(
                        members.data(), session.words,
                        [&](const std::size_t bit) {
                            if (ambiguous ||
                                (excluded != nullptr &&
                                 pc_bitset_test(
                                     excluded->data(), bit))) {
                                return;
                            }
                            std::vector<std::uint64_t> signature =
                                modifier_exclusion_effect_signature(
                                    session,
                                    static_cast<std::uint32_t>(bit));
                            if (!first.has_value()) {
                                first = std::move(signature);
                            } else if (*first != signature) {
                                ambiguous = true;
                            }
                        });
                    return ambiguous;
                };
            std::vector<std::uint8_t> ambiguous_junk(
                calc.layout().junk_classes.size(), 0);
            for (std::size_t junk = 0;
                 junk < calc.layout().junk_classes.size(); ++junk) {
                ambiguous_junk[junk] = ambiguous_members(
                    calc.layout().junk_classes[junk].member_mask,
                    nullptr)
                                           ? 1
                                           : 0;
            }
            std::vector<std::array<std::uint8_t, 3>> ambiguous_goal(
                calc.layout().slots.size());
            for (std::size_t slot = 0;
                 slot < calc.layout().slots.size(); ++slot) {
                const ResolvedGoalSlot& resolved =
                    calc.layout().slots[slot];
                ambiguous_goal[slot][static_cast<std::size_t>(
                    GoalSlotStatus::Absent)] = 0;
                ambiguous_goal[slot][static_cast<std::size_t>(
                    GoalSlotStatus::PresentBelowTier)] =
                    ambiguous_members(
                        resolved.member_mask,
                        &resolved.satisfying_mask)
                        ? 1
                        : 0;
                ambiguous_goal[slot][static_cast<std::size_t>(
                    GoalSlotStatus::Satisfied)] =
                    ambiguous_members(
                        resolved.satisfying_mask, nullptr)
                        ? 1
                        : 0;
            }
            const auto affix_needs_exclusion_identity =
                [&](const refinement::ObservationRequirement& requirement,
                    const std::uint16_t affix_traits,
                    const std::uint8_t item_traits,
                    const std::vector<std::uint32_t>& tag_ids) {
                    const RefinementFeatureMask exclusion =
                        refinement_feature(
                            RefinementFeature::
                                ModifierExclusionSignature);
                    return std::any_of(
                        requirement.affix_observations.begin(),
                        requirement.affix_observations.end(),
                        [&](const RefinementAffixObservation&
                                observation) {
                            return (observation.features &
                                    exclusion) != 0 &&
                                   refinement_selector_matches(
                                       observation.selector,
                                       affix_traits,
                                       item_traits,
                                       tag_ids);
                        });
                };
            const auto state_has_observable_ambiguous_identity =
                [&](const refinement::ObservationRequirement& requirement,
                    const AbstractState& state) {
                    const bool has_eldritch_dominance =
                        state.searing_exarch_tier !=
                        state.eater_of_worlds_tier;
                    std::uint8_t item_traits =
                        has_eldritch_dominance
                            ? kRefinementItemHasEldritchDominance
                            : 0;
                    const bool prefix_locked =
                        (state.flags & kFlagPrefixesLocked) != 0;
                    const bool suffix_locked =
                        (state.flags & kFlagSuffixesLocked) != 0;
                    if (prefix_locked != suffix_locked) {
                        item_traits |=
                            kRefinementItemExactlyOneSideLocked;
                    }
                    const std::int8_t dominant_side =
                        state.searing_exarch_tier >
                                state.eater_of_worlds_tier
                            ? PC_SIDE_PREFIX
                            : (state.eater_of_worlds_tier >
                                       state.searing_exarch_tier
                                   ? PC_SIDE_SUFFIX
                                   : -1);
                    const auto side_traits =
                        [&](const std::int8_t side) {
                            std::uint16_t traits =
                                side == PC_SIDE_PREFIX
                                    ? kRefinementAffixPrefix
                                    : kRefinementAffixSuffix;
                            const bool locked =
                                (side == PC_SIDE_PREFIX &&
                                 (state.flags &
                                  kFlagPrefixesLocked) != 0) ||
                                (side == PC_SIDE_SUFFIX &&
                                 (state.flags &
                                  kFlagSuffixesLocked) != 0);
                            if (locked) {
                                traits |=
                                    kRefinementAffixOnLockedSide;
                            }
                            if (dominant_side >= 0) {
                                traits |=
                                    side == dominant_side
                                        ? kRefinementAffixOnEldritchDominantSide
                                        : kRefinementAffixOnEldritchNonDominantSide;
                            }
                            return traits;
                        };
                    for (std::size_t junk = 0;
                         junk < ambiguous_junk.size(); ++junk) {
                        if (!ambiguous_junk[junk]) continue;
                        const JunkClass& klass =
                            calc.layout().junk_classes[junk];
                        std::vector<std::uint32_t> tag_ids;
                        for (std::size_t tag = 0;
                             tag <
                             calc.layout()
                                 .discriminating_tag_ids.size();
                             ++tag) {
                            if ((klass.tag_bits &
                                 (std::uint64_t{1} << tag)) != 0) {
                                tag_ids.push_back(
                                    calc.layout()
                                        .discriminating_tag_ids[tag]);
                            }
                        }
                        const std::uint8_t total =
                            state.junk_counts[junk];
                        const std::uint8_t fractured =
                            state.fractured_junk_counts[junk];
                        const std::uint8_t crafted =
                            state.crafted_junk_counts[junk];
                        const std::uint8_t both =
                            state.fractured_crafted_junk_counts[junk];
                        const std::array<std::pair<
                            std::uint8_t, std::uint16_t>, 4>
                            carriers{{
                                {static_cast<std::uint8_t>(
                                     total - fractured - crafted + both),
                                 0},
                                {static_cast<std::uint8_t>(
                                     fractured - both),
                                 kRefinementAffixFractured},
                                {static_cast<std::uint8_t>(
                                     crafted - both),
                                 kRefinementAffixCrafted},
                                {both,
                                 static_cast<std::uint16_t>(
                                     kRefinementAffixFractured |
                                     kRefinementAffixCrafted)},
                            }};
                        for (const auto& [count, flags] : carriers) {
                            if (count == 0) continue;
                            std::uint16_t traits =
                                side_traits(klass.gen_type) | flags;
                            bool veiled = false;
                            pc_bitset_for_each(
                                klass.member_mask.data(), session.words,
                                [&](const std::size_t bit) {
                                    veiled =
                                        veiled ||
                                        modifier_is_veiled_template(
                                            session,
                                            static_cast<std::uint32_t>(
                                                bit));
                                });
                            if (veiled) {
                                traits |= kRefinementAffixVeiled;
                            }
                            if (affix_needs_exclusion_identity(
                                    requirement, traits, item_traits,
                                    tag_ids)) {
                                return true;
                            }
                        }
                    }
                    for (std::size_t slot = 0;
                         slot < ambiguous_goal.size(); ++slot) {
                        const auto status = static_cast<GoalSlotStatus>(
                            state.slot_status[slot]);
                        if (!ambiguous_goal[slot][
                                static_cast<std::size_t>(status)]) {
                            continue;
                        }
                        std::uint16_t traits = 0;
                        const ResolvedGoalSlot& resolved =
                            calc.layout().slots[slot];
                        pc_bitset_for_each(
                            resolved.member_mask.data(), session.words,
                            [&](const std::size_t bit) {
                                const std::int8_t side =
                                    session.gen_type[
                                        static_cast<std::uint32_t>(bit)];
                                if (side == PC_SIDE_PREFIX ||
                                    side == PC_SIDE_SUFFIX) {
                                    traits |= side_traits(side);
                                }
                            });
                        if ((state.fractured_goal_mask &
                             (1u << slot)) != 0) {
                            traits |= kRefinementAffixFractured;
                        }
                        if ((state.crafted_goal_mask &
                             (1u << slot)) != 0) {
                            traits |= kRefinementAffixCrafted;
                        }
                        if (affix_needs_exclusion_identity(
                                requirement, traits, item_traits, {})) {
                            return true;
                        }
                    }
                    return false;
                };
            const auto record_refinement_trigger_parent =
                [&](const std::uint32_t coarse_state) {
                    if (coarse_state == kNoId) return;
                    PolicyRefinementTelemetry& refinement =
                        result.diagnostics.policy_refinement;
                    if (std::find(
                            refinement.trigger_coarse_states.begin(),
                            refinement.trigger_coarse_states.end(),
                            coarse_state) !=
                        refinement.trigger_coarse_states.end()) {
                        return;
                    }
                    if (refinement.trigger_coarse_states.size() <
                        result.policy.size()) {
                        refinement.trigger_coarse_states.push_back(
                            coarse_state);
                    } else {
                        ++refinement.trigger_coarse_states_omitted;
                    }
                    /* Preserve the first witness for existing diagnostic
                     * consumers while exact refinement consumes the bounded
                     * structured collection above. */
                    if (result.diagnostics.policy_compatibility_state ==
                        kNoId) {
                        result.diagnostics.policy_compatibility_state =
                            coarse_state;
                    }
                };
            constexpr double kProductSimulatorActionLimit = 100000.0;
            if (restore_output_incumbent &&
                result.primitive_renewal_witness.valid &&
                result.primitive_renewal_witness.success_probability >
                    0.0 &&
                1.0 /
                        result.primitive_renewal_witness
                            .success_probability >
                    kProductSimulatorActionLimit) {
                executable_policy_abstraction_supported = false;
                const PrimitiveRenewalWitness& witness =
                    result.primitive_renewal_witness;
                const std::string action_id =
                    witness.primitive_action <
                            calc.registry().actions.size()
                        ? calc.registry()
                              .actions[witness.primitive_action]
                              .id
                        : std::string{};
                const std::string reason =
                    "primitive_renewal_expected_actions_exceed_"
                    "simulator_cap";
                result.diagnostics.policy_compatibility_supported =
                    false;
                result.diagnostics.policy_compatibility_state =
                    result.start_state;
                result.diagnostics.policy_compatibility_action =
                    action_id;
                result.diagnostics.policy_compatibility_reason =
                    reason;
                if (!action_id.empty()) {
                    record_skipped_unsupported(action_id);
                    add_action_reason(
                        "unsupported", action_id,
                        reason + "_at_state_" +
                            std::to_string(result.start_state));
                }
            }
            if (restore_output_incumbent &&
                result.diagnostics.resource_cap_hit &&
                !result.primitive_renewal_witness.valid &&
                result.start_state < result.policy.size()) {
                const PolicyOperatorRef selected =
                    result.policy[result.start_state];
                if (selected.index < calc.operators().size()) {
                    const PlannerOperator& planner =
                        calc.operators()[selected.index];
                    if (planner.kind ==
                            PlannerOperatorKind::Primitive &&
                        planner.primitive_action <
                            calc.registry().actions.size()) {
                        const ActionDescriptor& action =
                            calc.registry().actions[
                                planner.primitive_action];
                        if (action_transition_facts(
                                action.params.type)
                                .renewal) {
                            PolicyRefinementTelemetry& refinement =
                                result.diagnostics.policy_refinement;
                            ++refinement.triggers;
                            record_refinement_trigger_parent(
                                result.start_state);
                            refinement.status = "triggered";
                            ++refinement.counterexamples;
                            const std::string witness =
                                "{\"source\":\"compatibility\","
                                "\"kind\":\"capped_policy_lift\","
                                "\"coarse_state\":" +
                                std::to_string(result.start_state) +
                                ",\"action\":\"" + action.id + "\"}";
                            if (refinement.counterexample_samples.size() <
                                result.diagnostics
                                    .diagnostic_sample_limit) {
                                refinement.counterexample_samples.push_back(
                                    witness);
                            } else {
                                ++refinement
                                      .counterexample_samples_omitted;
                            }
                        }
                    }
                }
            }
            const auto compatibility_reachable =
                [&](const std::uint32_t state_id) {
                    if (state_id < result.policy_reachable.size()) {
                        return result.policy_reachable[state_id] != 0;
                    }
                    if (state_id < restored_policy_reachable.size()) {
                        return restored_policy_reachable[state_id] != 0;
                    }
                    return false;
                };
            if (restore_output_incumbent &&
                result.policy_reachable.empty() &&
                restored_policy_reachable.empty()) {
                executable_policy_abstraction_supported = false;
                result.diagnostics.policy_compatibility_supported =
                    false;
                result.diagnostics.policy_compatibility_reason =
                    "coarse_parent_policy_reachability_unavailable_after_cap";
            }
            for (std::uint32_t state_id = 0;
                 state_id < result.policy.size() &&
                 executable_policy_abstraction_supported;
                 ++state_id) {
                if (!compatibility_reachable(state_id) ||
                    state_id >= result.goal_states.size() ||
                    result.goal_states[state_id]) {
                    continue;
                }
                const PolicyOperatorRef selected = result.policy[state_id];
                if (selected.index == kNoId ||
                    selected.index >= calc.operators().size()) {
                    continue;
                }
                const PlannerOperator& planner =
                    calc.operators()[selected.index];
                /*
                 * The engine-owned ordered runtime program composes every
                 * internal observation and the full-program exclusion
                 * survivor preimage at the operator entry. This admits both
                 * primitive and fixed policies to the same witness-driven
                 * lift without action-name switches or treating any internal
                 * primitive as the representative action.
                 */
                const PlannerOperatorRuntimeSemantics runtime =
                    planner_operator_runtime_semantics(
                        planner, calc.registry());
                refinement::SelectedAction runtime_selection;
                runtime_selection.contract =
                    runtime.compatibility_refinement;
                runtime_selection.ordered_program.reserve(
                    runtime.ordered_program.size());
                for (const PlannerOperatorRuntimeStep& step :
                     runtime.ordered_program) {
                    runtime_selection.ordered_program.push_back(
                        step.refinement);
                }
                runtime_selection.execution_paths.reserve(
                    runtime.execution_paths.size());
                for (const std::vector<
                         PlannerOperatorRuntimeStep>& path :
                     runtime.execution_paths) {
                    std::vector<ActionRefinementContract> contracts;
                    contracts.reserve(path.size());
                    for (const PlannerOperatorRuntimeStep& step :
                         path) {
                        contracts.push_back(step.refinement);
                    }
                    runtime_selection.execution_paths.push_back(
                        std::move(contracts));
                }
                const refinement::ObservationRequirement
                    direct_requirement =
                        refinement::
                            observation_requirement_from_selected_action(
                                runtime_selection);
                refinement::ObservationRequirement
                    downstream_exclusion;
                downstream_exclusion.affix_observations.push_back(
                    {
                        refinement_feature(
                            RefinementFeature::
                                ModifierExclusionSignature),
                        {}});
                const refinement::ObservationRequirement
                    preserved_exclusion =
                        refinement::
                            preserved_observation_requirement(
                                downstream_exclusion,
                                runtime_selection);
                const refinement::ObservationRequirement
                    compatibility_requirement =
                        refinement::
                            merge_observation_requirements(
                                direct_requirement,
                                preserved_exclusion);
                const refinement::AbstractFeatureExtraction
                    compatibility_extraction =
                        refinement::
                            extract_strict_abstract_features(
                                session,
                                calc.layout(),
                                calc.state(state_id),
                                compatibility_requirement);
                const bool unavailable_compatibility_observation =
                    !compatibility_extraction.complete();
                if (!unavailable_compatibility_observation &&
                    !state_has_observable_ambiguous_identity(
                        compatibility_requirement,
                        calc.state(state_id))) {
                    continue;
                }
                PolicyRefinementTelemetry& refinement =
                    result.diagnostics.policy_refinement;
                ++refinement.triggers;
                record_refinement_trigger_parent(state_id);
                refinement.status = "triggered";
                ++refinement.counterexamples;
                RefinementFeatureMask required_feature_mask =
                    compatibility_requirement.item_features;
                for (const RefinementAffixObservation& observation :
                     compatibility_requirement.affix_observations) {
                    required_feature_mask |= observation.features;
                }
                const bool preserved_exclusion_observed =
                    preserved_exclusion.item_features != 0 ||
                    !preserved_exclusion.modifier_tag_ids.empty() ||
                    !preserved_exclusion.affix_observations.empty();
                const std::string witness =
                    "{\"source\":\"compatibility\","
                    "\"kind\":\"observation\","
                    "\"coarse_state\":" +
                    std::to_string(state_id) +
                    ",\"action\":\"" + planner.id +
                    "\",\"operator_kind\":\"" +
                    (planner.kind ==
                             PlannerOperatorKind::Primitive
                         ? "primitive"
                         : "fixed_option") +
                    "\",\"runtime_dependency_count\":" +
                    std::to_string(
                        runtime.action_dependencies.size()) +
                    ",\"unavailable_feature_mask\":" +
                    std::to_string(
                        compatibility_extraction
                            .unavailable_features) +
                    ",\"required_feature_mask\":" +
                    std::to_string(required_feature_mask) +
                    ",\"preserved_exclusion_witness\":" +
                    (preserved_exclusion_observed ? "true" : "false") +
                    "}";
                if (refinement.counterexample_samples.size() <
                    result.diagnostics.diagnostic_sample_limit) {
                    refinement.counterexample_samples.push_back(
                        witness);
                } else {
                    ++refinement.counterexample_samples_omitted;
                }
            }
        }
        if (!executable_policy_abstraction_supported) {
            result.converged = false;
        }
        if (result.converged) {
            const double exact_value = result.values[result.start_state];
            result.policy_available = true;
            result.policy_status = SolvePolicyStatus::Exact;
            result.termination = SolveTermination::ExactClosed;
            result.lower_bound = exact_value;
            result.upper_bound = exact_value;
            result.evaluated_policy_cost = exact_value;
            result.absolute_optimality_gap = 0.0;
            result.relative_optimality_gap = 0.0;
        } else if (restore_output_incumbent &&
                   reachable_policy_complete && !finalization_capped) {
            const BoundedPolicyIncumbent& incumbent = *output_incumbent;
            result.policy_available = true;
            result.target_met = target_gap_stop;
            result.target_fired =
                target_gap_stop ? target_gap_fired : SolveGapTarget::None;
            result.policy_status =
                target_gap_stop
                    ? SolvePolicyStatus::BoundedNearOptimal
                    : SolvePolicyStatus::BoundedFeasible;
            result.termination =
                target_gap_stop
                    ? SolveTermination::TargetGap
                    : requested_bounded_finish
                          ? SolveTermination::RequestedBoundedFinish
                          : numerical_stability_stop
                                ? SolveTermination::NumericalStability
                                : SolveTermination::RefusedResourceCap;
            result.lower_bound = certified_global_lower_bound();
            result.upper_bound = incumbent.certified_upper_bound;
            result.evaluated_policy_cost =
                incumbent.independently_evaluated
                    ? incumbent.evaluated_policy_cost
                    : incumbent.certified_upper_bound;
            const double bracket_tolerance =
                value_comparison_tolerance(result.upper_bound);
            if (result.lower_bound > result.evaluated_policy_cost +
                                         bracket_tolerance ||
                result.evaluated_policy_cost > result.upper_bound +
                                                   bracket_tolerance) {
                throw std::logic_error(
                    "bounded incumbent evaluation violates L <= J_pi <= U");
            }
            result.absolute_optimality_gap = std::max(
                0.0, result.upper_bound - result.lower_bound);
            result.relative_optimality_gap =
                result.lower_bound > 0.0
                    ? std::max(
                          0.0,
                          result.upper_bound / result.lower_bound - 1.0)
                    : kInfinity;
        } else {
            result.policy_available = false;
            result.policy_status = SolvePolicyStatus::None;
            /* Stopping cause and policy availability are separate. A cap
             * without an incumbent is still a resource-cap result; callers
             * must not have to infer it from optional telemetry. */
            result.termination =
                requested_bounded_finish
                    ? SolveTermination::RequestedBoundedFinish
                    : numerical_stability_stop
                          ? SolveTermination::NumericalStability
                          : result.diagnostics.resource_cap_hit
                                ? SolveTermination::RefusedResourceCap
                                : SolveTermination::NoExecutablePolicy;
            result.lower_bound = certified_global_lower_bound();
            result.upper_bound = kInfinity;
            result.evaluated_policy_cost = kInfinity;
            result.absolute_optimality_gap = kInfinity;
            result.relative_optimality_gap = kInfinity;
        }

        const auto canonical_publication_cap =
            [](std::string cap) {
                if (cap == "max_pairs" ||
                    cap == "max_exact_kernels") {
                    return std::string{"max_state_action_rows"};
                }
                if (cap == "max_classes" ||
                    cap == "max_refinement_classes" ||
                    cap == "max_reachable_classes" ||
                    cap == "max_exact_states" ||
                    cap == "max_coarse_states") {
                    return std::string{"max_discovered_states"};
                }
                if (cap == "max_owned_bytes" ||
                    cap == "max_estimated_memory_bytes") {
                    return std::string{"max_solver_owned_bytes"};
                }
                if (cap == "max_refinement_rounds" ||
                    cap == "max_component_iterations") {
                    return std::string{"max_sweeps"};
                }
                if (cap == "max_output_json_bytes") {
                    return std::string{"max_strategy_json_bytes"};
                }
                return cap;
            };
        const auto record_refinement_refusal =
            [&](const std::string& cause) {
                PolicyRefinementTelemetry& telemetry =
                    result.diagnostics.policy_refinement;
                ++telemetry.refusal_causes;
                if (telemetry.refusal_cause_samples.size() <
                    result.diagnostics.diagnostic_sample_limit) {
                    telemetry.refusal_cause_samples.push_back(cause);
                } else {
                    ++telemetry.refusal_cause_samples_omitted;
                }
            };
        const auto revoke_publication =
            [&](const std::string& reason,
                const std::string& resource_cap = std::string{}) {
                if (!resource_cap.empty()) {
                    const std::string solve_cap =
                        canonical_publication_cap(resource_cap);
                    result.diagnostics.policy_refinement.resource_cap =
                        solve_cap;
                    record_cap(solve_cap);
                }
                result.refined_policy_artifact = {};
                result.diagnostics.policy_refinement
                    .retained_artifact_bytes = 0;
                result.diagnostics.policy_refinement.publication_status =
                    "none";
                result.diagnostics.policy_refinement
                    .published_candidate_kind.clear();
                result.diagnostics.policy_compatibility_supported =
                    false;
                if (result.diagnostics.policy_compatibility_state ==
                    kNoId) {
                    result.diagnostics.policy_compatibility_state =
                        result.start_state;
                }
                if (result.diagnostics
                        .policy_compatibility_action.empty() &&
                    result.start_state < result.policy.size()) {
                    const PolicyOperatorRef selected =
                        result.policy[result.start_state];
                    if (selected.index < calc.operators().size()) {
                        result.diagnostics
                            .policy_compatibility_action =
                            calc.operators()[selected.index].id;
                    }
                }
                result.diagnostics
                    .policy_publication_failure_reason =
                    reason;
                if (result.diagnostics
                        .policy_compatibility_reason.empty()) {
                    result.diagnostics.policy_compatibility_reason =
                        reason;
                }
                executable_policy_abstraction_supported = false;
                result.converged = false;
                result.policy_available = false;
                result.policy_status = SolvePolicyStatus::None;
                result.target_met = false;
                result.target_fired = SolveGapTarget::None;
                result.termination =
                    result.diagnostics.resource_cap_hit
                        ? SolveTermination::RefusedResourceCap
                        : requested_bounded_finish
                              ? SolveTermination::RequestedBoundedFinish
                              : SolveTermination::NoExecutablePolicy;
                result.lower_bound = certified_global_lower_bound();
                result.upper_bound = kInfinity;
                result.evaluated_policy_cost = kInfinity;
                result.absolute_optimality_gap = kInfinity;
                result.relative_optimality_gap = kInfinity;
            };
        const auto diagnostic_json_escape =
            [](const std::string& value) {
                std::string escaped;
                escaped.reserve(value.size());
                for (const unsigned char ch : value) {
                    switch (ch) {
                    case '\\': escaped += "\\\\"; break;
                    case '"': escaped += "\\\""; break;
                    case '\n': escaped += "\\n"; break;
                    case '\r': escaped += "\\r"; break;
                    case '\t': escaped += "\\t"; break;
                    default:
                        if (ch >= 0x20) {
                            escaped.push_back(static_cast<char>(ch));
                        }
                        break;
                    }
                }
                return escaped;
            };
        const auto diagnostic_finite_double =
            [](const double value) {
                if (!std::isfinite(value)) return std::string{"null"};
                char buffer[64];
                const auto [end, error] = std::to_chars(
                    buffer, buffer + sizeof(buffer), value,
                    std::chars_format::general,
                    std::numeric_limits<double>::max_digits10);
                if (error != std::errc{}) return std::string{"null"};
                return std::string(buffer, end);
            };
        const auto retain_bounded_json_sample =
            [&](std::vector<std::string>& samples,
                std::uint64_t& omitted,
                std::uint64_t& retained_bytes,
                std::string sample) {
                PolicyRefinementTelemetry& telemetry =
                    result.diagnostics.policy_refinement;
                const std::uint64_t shared_bytes =
                    telemetry.publication_candidate_sample_bytes +
                    telemetry.structural_failure_sample_bytes +
                    telemetry.evaluator_memory_sample_bytes;
                const std::uint64_t byte_limit =
                    options.max_telemetry_json_bytes / 4;
                if (samples.size() >=
                        result.diagnostics.diagnostic_sample_limit ||
                    sample.size() > byte_limit ||
                    shared_bytes > byte_limit - sample.size()) {
                    ++omitted;
                    return;
                }
                retained_bytes += sample.size();
                samples.push_back(std::move(sample));
            };
        const auto record_candidate_sample =
            [&](const BoundedPolicyIncumbent& candidate,
                const std::string& stage,
                const std::string& disposition,
                const std::string& reason) {
                PolicyRefinementTelemetry& telemetry =
                    result.diagnostics.policy_refinement;
                retain_bounded_json_sample(
                    telemetry.publication_candidate_samples,
                    telemetry.publication_candidate_samples_omitted,
                    telemetry.publication_candidate_sample_bytes,
                    "{\"identity\":\"" +
                        std::to_string(candidate.portfolio_identity) +
                        "\",\"kind\":\"" +
                        diagnostic_json_escape(candidate.kind) +
                        "\",\"stage\":\"" +
                        diagnostic_json_escape(stage) +
                        "\",\"verification\":\"" +
                        (candidate.independently_evaluated
                             ? std::string{"independently_evaluated"}
                             : std::string{"compiled_unverified"}) +
                        "\",\"evaluated_cost\":" +
                        diagnostic_finite_double(
                            candidate.evaluated_policy_cost) +
                        ",\"estimate_cost\":" +
                        diagnostic_finite_double(
                            candidate.certified_upper_bound) +
                        ",\"reconciliation_absolute_delta\":" +
                        diagnostic_finite_double(
                            candidate.reconciliation_absolute_delta) +
                        ",\"reconciliation_relative_delta\":" +
                        diagnostic_finite_double(
                            candidate.reconciliation_relative_delta) +
                        ",\"disposition\":\"" +
                        diagnostic_json_escape(disposition) +
                        "\",\"selection_reason\":\"" +
                        diagnostic_json_escape(reason) + "\"}");
            };
        const auto publication_options_for_live =
            [&](const std::uint64_t live_bytes)
                -> std::optional<SolveOptions> {
                if (live_bytes >=
                    options.max_solver_owned_bytes) {
                    return std::nullopt;
                }
                const std::uint64_t retained =
                    estimated_retained_solver_bytes(calc, &result);
                const std::uint64_t external_live =
                    live_bytes > retained
                        ? live_bytes - retained
                        : 0;
                if (external_live >=
                    options.max_solver_owned_bytes) {
                    return std::nullopt;
                }
                SolveOptions scoped = options;
                scoped.max_solver_owned_bytes =
                    options.max_solver_owned_bytes - external_live;
                /* max_reforge_work owns one solve/evaluation stage, not a
                 * fungible lifetime pool shared by graph discovery, direct
                 * final-graph assertion, and an optional strict lift. Each
                 * stage retains its own V1-equivalent ledger in telemetry;
                 * starving independent verification with coarse-search work
                 * can otherwise turn a found executable policy into a false
                 * publication failure. Memory remains shared because those
                 * allocations are simultaneously live. */
                scoped.max_reforge_work = options.max_reforge_work;
                return scoped;
            };
        const auto saturated_publication_add =
            [](const std::uint64_t left, const std::uint64_t right) {
                return right >
                               std::numeric_limits<std::uint64_t>::max() -
                                   left
                    ? std::numeric_limits<std::uint64_t>::max()
                    : left + right;
            };
        const auto verify_retained_final_graph =
            [&](BoundedPolicyIncumbent& candidate) {
                if (certified_incumbent_invalid_reason(candidate) ==
                    nullptr) {
                    return true;
                }
                if (!candidate.final_graph_verification_failure.empty()) {
                    return false;
                }
                if (retained_incumbent_invalid_reason(candidate) !=
                    nullptr) {
                    return false;
                }
                const std::optional<SolveOptions> scoped =
                    publication_options_for_live(
                        estimated_owned_bytes());
                if (!scoped.has_value()) {
                    candidate.final_graph_verification_failure =
                        "compiled_unverified: no remaining solver memory "
                        "for final graph evaluation";
                    candidate.independently_certified = false;
                    candidate.independently_evaluated = false;
                    candidate.proper = false;
                    candidate.executable = false;
                    candidate.evaluated_policy_cost = kInfinity;
                    record_candidate_sample(
                        candidate, "final_graph_evaluation",
                        "retained_diagnostic_only",
                        candidate.final_graph_verification_failure);
                    return false;
                }
                SolveResult proof;
                proof.policy_available = true;
                proof.policy_status = SolvePolicyStatus::BoundedFeasible;
                proof.evaluated_policy_cost =
                    candidate.certified_upper_bound;
                proof.start_state = result.start_state;
                proof.has_exact_start_item = result.has_exact_start_item;
                proof.exact_start_item = result.exact_start_item;
                proof.values = candidate.values;
                proof.policy = candidate.policy;
                proof.policy_reachable = candidate.policy_reachable;
                proof.unveil_preferences = candidate.unveil_preferences;
                proof.option_unveil_preferences =
                    candidate.option_unveil_preferences;
                proof.behavioral_representative_by_state =
                    candidate.behavioral_representative_by_state;
                proof.primitive_renewal_witness =
                    candidate.primitive_renewal_witness;
                proof.options = options;
                proof.goal_states.assign(proof.values.size(), 0);
                proof.expanded.assign(proof.values.size(), 0);
                for (std::uint32_t state = 0;
                     state < proof.values.size(); ++state) {
                    proof.goal_states[state] =
                        calc.is_goal_state(calc.state(state)) ? 1 : 0;
                }
                refinement::CompiledPolicyAssertion assertion =
                    refinement::assert_compiled_policy_exact(
                        calc, proof, prices, *scoped,
                        "retained publication candidate");
                result.diagnostics.reforge_frontier_work =
                    saturated_publication_add(
                        result.diagnostics.reforge_frontier_work,
                        assertion.evaluation.reforge_work);
                result.diagnostics.reforge_logical_work_v1 =
                    std::min(
                        options.max_reforge_work,
                        saturated_publication_add(
                            result.diagnostics.reforge_logical_work_v1,
                            assertion.evaluation
                                .reforge_logical_work_v1));
                candidate.reconciliation_absolute_delta =
                    assertion.absolute_cost_delta;
                candidate.reconciliation_relative_delta =
                    assertion.relative_cost_delta;
                if (assertion.executable && assertion.proper &&
                    assertion.evaluation.cost_complete &&
                    assertion.zero_off_policy &&
                    std::isfinite(assertion.exact_cost) &&
                    assertion.exact_cost >= 0.0) {
                    candidate.certified_upper_bound =
                        assertion.exact_cost;
                    candidate.evaluated_policy_cost =
                        assertion.exact_cost;
                    candidate.independently_certified = true;
                    candidate.independently_evaluated = true;
                    candidate.proper = true;
                    candidate.executable = true;
                    candidate.final_graph_verification_failure.clear();
                    candidate.compilation_provenance +=
                        "+independent_final_graph_evaluation_v1";
                    candidate.retained_owned_bytes =
                        incumbent_owned_bytes(candidate);
                    record_candidate_sample(
                        candidate, "final_graph_evaluation",
                        "eligible_verified_candidate",
                        assertion.cost_reconciled
                            ? "final graph cost reconciled"
                            : "executable cost mismatch blocks exactness");
                    return true;
                }
                candidate.final_graph_verification_failure =
                    "compiled_unverified: " +
                    (assertion.failure_reason.empty()
                         ? std::string{"final graph evaluation failed"}
                         : assertion.failure_reason);
                candidate.independently_certified = false;
                candidate.independently_evaluated = false;
                candidate.proper = false;
                candidate.executable = false;
                candidate.evaluated_policy_cost = kInfinity;
                record_candidate_sample(
                    candidate, "final_graph_evaluation",
                    "retained_diagnostic_only",
                    candidate.final_graph_verification_failure);
                if (assertion.resource_cap ==
                        "max_solver_owned_bytes" ||
                    assertion.failure_reason.find(
                        "max_owned_bytes") != std::string::npos) {
                    PolicyRefinementTelemetry& telemetry =
                        result.diagnostics.policy_refinement;
                    retain_bounded_json_sample(
                        telemetry.evaluator_memory_samples,
                        telemetry.evaluator_memory_samples_omitted,
                        telemetry.evaluator_memory_sample_bytes,
                        "{\"candidate_identity\":\"" +
                            std::to_string(
                                candidate.portfolio_identity) +
                            "\",\"stage\":\"final_graph_evaluation\","
                            "\"nodes\":" +
                            std::to_string(
                                candidate.compiled_artifact.nodes) +
                            ",\"edges\":" +
                            std::to_string(
                                candidate.compiled_artifact.edges) +
                            ",\"policy_regions\":" +
                            std::to_string(
                                candidate.compiled_artifact
                                    .policy_regions) +
                            ",\"calculation\":\"" +
                            diagnostic_json_escape(
                                assertion.failure_reason) + "\"}");
                }
                return false;
            };
        const auto verify_retained_portfolio = [&] {
            for (BoundedPolicyIncumbent& candidate :
                 certified_fallback_portfolio) {
                (void)verify_retained_final_graph(candidate);
            }
            std::sort(
                certified_fallback_portfolio.begin(),
                certified_fallback_portfolio.end(),
                [&](const BoundedPolicyIncumbent& left,
                    const BoundedPolicyIncumbent& right) {
                    const bool left_verified =
                        certified_incumbent_invalid_reason(left) == nullptr;
                    const bool right_verified =
                        certified_incumbent_invalid_reason(right) == nullptr;
                    if (left_verified != right_verified) {
                        return left_verified;
                    }
                    return incumbent_precedes(left, right);
                });
        };
        const auto publish_certified_fallback =
            [&](const SolveTermination coarse_solve_termination) {
                PolicyRefinementTelemetry& telemetry =
                    result.diagnostics.policy_refinement;
                verify_retained_portfolio();
                BoundedPolicyIncumbent* retained =
                    best_current_certified_fallback();
                if (retained == nullptr) return false;
                ++telemetry.fallback_publication_attempts;
                BoundedPolicyIncumbent fallback = std::move(*retained);
                const bool direct_core_policy =
                    fallback.compilation_provenance ==
                        "direct_compiled_policy_assertion_v1" ||
                    fallback.compilation_provenance ==
                        "selected_compiled_policy_assertion_v1" ||
                    fallback.compilation_provenance ==
                        "selected_policy_guided_exact_refinement_v1";
                record_candidate_sample(
                    fallback, "publication",
                    "selected_for_publication",
                    "strict cheapest independently evaluated candidate");
                for (const BoundedPolicyIncumbent& candidate :
                     certified_fallback_portfolio) {
                    if (candidate.portfolio_identity ==
                        fallback.portfolio_identity) {
                        continue;
                    }
                    record_candidate_sample(
                        candidate, "publication", "not_selected",
                        candidate.independently_evaluated
                            ? "a cheaper independently evaluated candidate "
                              "won publication"
                            : "final graph evaluation did not establish an "
                              "executable cost");
                }
                certified_fallback_portfolio.clear();
                certified_fallback_portfolio.shrink_to_fit();
                telemetry.fallback_portfolio_candidates = 0;
                telemetry.fallback_portfolio_owned_bytes = 0;
                populate_incumbent_policy(fallback);
                result.values = std::move(fallback.values);
                result.policy = std::move(fallback.policy);
                result.unveil_preferences =
                    std::move(fallback.unveil_preferences);
                result.option_unveil_preferences =
                    std::move(fallback.option_unveil_preferences);
                result.behavioral_representative_by_state =
                    std::move(
                        fallback.behavioral_representative_by_state);
                result.policy_reachable =
                    std::move(fallback.policy_reachable);
                result.primitive_renewal_witness =
                    std::move(fallback.primitive_renewal_witness);
                result.refined_policy_artifact =
                    std::move(fallback.compiled_artifact);
                policy_rows = std::move(fallback.policy_rows);
                restored_policy_row_costs =
                    std::move(fallback.policy_row_costs);
                result.goal_states.assign(result.values.size(), 0);
                for (std::uint32_t state = 0;
                     state < result.values.size(); ++state) {
                    result.goal_states[state] =
                        calc.is_goal_state(calc.state(state)) ? 1 : 0;
                }
                owned_result_nested_bytes = 0;
                for (const auto& preferences :
                     result.unveil_preferences) {
                    owned_result_nested_bytes +=
                        preferences.capacity() * sizeof(std::uint32_t);
                }
                for (const auto& preferences :
                     result.option_unveil_preferences) {
                    owned_result_nested_bytes +=
                        preferences.capacity() *
                        sizeof(ObservedUnveilPreference);
                    for (const auto& preference : preferences) {
                        owned_result_nested_bytes +=
                            preference.choices.capacity() *
                            sizeof(ObservedUnveilChoice);
                    }
                }
                result.converged = false;
                result.policy_available = true;
                result.policy_status = SolvePolicyStatus::BoundedFeasible;
                result.target_met = false;
                result.target_fired = SolveGapTarget::None;
                result.lower_bound = certified_global_lower_bound();
                result.upper_bound = fallback.certified_upper_bound;
                result.evaluated_policy_cost =
                    fallback.evaluated_policy_cost;
                result.absolute_optimality_gap = std::max(
                    0.0, result.upper_bound - result.lower_bound);
                result.relative_optimality_gap =
                    result.lower_bound > 0.0
                        ? std::max(
                              0.0,
                              result.upper_bound / result.lower_bound -
                                  1.0)
                        : kInfinity;
                result.termination =
                    successful_refined_publication_termination(
                        coarse_solve_termination,
                        result.diagnostics.resource_cap_hit,
                        false,
                        coarse_discovery_closed);
                result.diagnostics.focused_upper_bound =
                    result.upper_bound;
                result.diagnostics
                    .focused_partial_policy_upper_bound =
                    result.upper_bound;
                result.diagnostics.focused_optimality_gap =
                    result.absolute_optimality_gap;
                result.diagnostics.solution_scope =
                    direct_core_policy
                        ? "direct_certified_core_policy_bounded"
                        : "certified_executable_fallback_bounded";
                result.diagnostics.incumbent_kind = fallback.kind;
                result.diagnostics.incumbent_round = fallback.round;
                result.diagnostics.incumbent_restart_state =
                    fallback.restart_state;
                result.diagnostics.incumbent_anchor_state =
                    fallback.fallback_anchor_state;
                result.diagnostics.incumbent_goal_identity =
                    fallback.goal_identity;
                result.diagnostics.incumbent_economy_identity =
                    fallback.economy_identity;
                result.diagnostics
                    .incumbent_action_vocabulary_identity =
                    fallback.action_vocabulary_identity;
                result.diagnostics.incumbent_graph_identity =
                    fallback.graph_identity;
                result.diagnostics.incumbent_strict_state_provenance =
                    fallback.strict_state_provenance;
                result.diagnostics
                    .policy_publication_failure_reason.clear();
                telemetry.status =
                    direct_core_policy
                        ? "direct_core_policy_retained_after_strict_lift"
                        : "certified_fallback_retained";
                telemetry.bounded_publication_retained = true;
                telemetry.direct_candidate_retained =
                    direct_core_policy;
                telemetry.publication_status =
                    direct_core_policy
                        ? "bounded_core_policy"
                        : "bounded_fallback_policy";
                telemetry.published_candidate_kind = fallback.kind;
                telemetry.retained_artifact_bytes =
                    result.refined_policy_artifact
                            .strategy_json.capacity() +
                    result.refined_policy_artifact
                            .certification_strategy_json.capacity() +
                    2;
                telemetry.published_fallback_upper = result.upper_bound;
                telemetry.published_fallback_evaluated_cost =
                    result.evaluated_policy_cost;
                telemetry.published_fallback_witness_hash =
                    result.primitive_renewal_witness.witness_hash;
                telemetry.published_fallback_kind = fallback.kind;
                ++telemetry.fallback_publication_successes;
                telemetry.cheapest_independently_evaluated_selected = true;
                count_policy_actions(
                    result.policy,
                    result.diagnostics.upper_policy_action_states);
                executable_policy_abstraction_supported = true;
                return true;
            };
        if (result.policy_available &&
            std::isfinite(result.evaluated_policy_cost)) {
            result.diagnostics.policy_refinement
                .preferred_candidate_upper =
                result.evaluated_policy_cost;
        }
        const auto assertion_status_name =
            [](const refinement::CompiledPolicyAssertionStatus status) {
                switch (status) {
                case refinement::CompiledPolicyAssertionStatus::NotRun:
                    return "not_run";
                case refinement::CompiledPolicyAssertionStatus::Complete:
                    return "complete";
                case refinement::CompiledPolicyAssertionStatus::NoPolicy:
                    return "no_policy";
                case refinement::CompiledPolicyAssertionStatus::ResourceCap:
                    return "resource_cap";
                case refinement::CompiledPolicyAssertionStatus::
                        CompilationFailure:
                    return "compilation_failure";
                case refinement::CompiledPolicyAssertionStatus::
                        ExactEvaluationFailure:
                    return "exact_evaluation_failure";
                case refinement::CompiledPolicyAssertionStatus::
                        ImproperPolicy:
                    return "improper_policy";
                case refinement::CompiledPolicyAssertionStatus::
                        IncompleteCost:
                    return "incomplete_cost";
                case refinement::CompiledPolicyAssertionStatus::CostMismatch:
                    return "cost_mismatch";
                }
                return "not_run";
            };
        const auto retained_artifact_from_assertion =
            [](refinement::CompiledPolicyAssertion& assertion) {
                RetainedCompiledPolicyArtifact artifact;
                artifact.strategy_json = std::move(assertion.strategy_json);
                artifact.certification_strategy_json =
                    std::move(assertion.certification_strategy_json);
                artifact.working_states =
                    assertion.compilation.working_states;
                artifact.behavioral_classes =
                    assertion.compilation.behavioral_classes;
                artifact.policy_regions =
                    assertion.compilation.policy_regions;
                artifact.infrastructure_nodes =
                    assertion.compilation.infrastructure_nodes;
                artifact.policy_route_nodes =
                    assertion.compilation.policy_route_nodes;
                artifact.local_gated_route_nodes =
                    assertion.compilation.local_gated_route_nodes;
                artifact.primitive_region_nodes =
                    assertion.compilation.primitive_region_nodes;
                artifact.additional_recipe_nodes =
                    assertion.compilation.additional_recipe_nodes;
                artifact.nodes = assertion.compilation.nodes;
                artifact.edges = assertion.compilation.edges;
                artifact.total_condition_bytes =
                    assertion.compilation.total_condition_bytes;
                artifact.max_condition_bytes =
                    assertion.compilation.max_condition_bytes;
                artifact.condition_edges =
                    assertion.compilation.condition_edges;
                artifact.unique_condition_literals =
                    assertion.compilation.unique_condition_literals;
                artifact.repeated_condition_occurrences =
                    assertion.compilation.repeated_condition_occurrences;
                artifact.repeated_condition_bytes =
                    assertion.compilation.repeated_condition_bytes;
                artifact.policy_route_nondefault_edges =
                    assertion.compilation.policy_route_nondefault_edges;
                artifact.policy_route_distinct_targets =
                    assertion.compilation.policy_route_distinct_targets;
                artifact.same_target_branch_groups =
                    assertion.compilation.same_target_branch_groups;
                artifact.same_target_branch_edges =
                    assertion.compilation.same_target_branch_edges;
                artifact.projected_same_target_edge_savings =
                    assertion.compilation
                        .projected_same_target_edge_savings;
                artifact.max_policy_route_out_degree =
                    assertion.compilation.max_policy_route_out_degree;
                artifact.max_policy_route_distinct_targets =
                    assertion.compilation
                        .max_policy_route_distinct_targets;
                artifact.exact_state_fallbacks =
                    assertion.compilation.exact_state_fallbacks;
                artifact.junk_predicates =
                    assertion.compilation.junk_predicates;
                artifact.policy_route_default_edges =
                    assertion.compilation.policy_route_default_edges;
                artifact.policy_route_restart_default_edges =
                    assertion.compilation
                        .policy_route_restart_default_edges;
                artifact.policy_route_offpolicy_default_edges =
                    assertion.compilation
                        .policy_route_offpolicy_default_edges;
                artifact.policy_route_default_mode =
                    assertion.compilation.policy_route_default_mode;
                artifact.certification_policy_route_default_edges =
                    assertion.certification_compilation
                        .policy_route_default_edges;
                artifact.certification_policy_route_offpolicy_default_edges =
                    assertion.certification_compilation
                        .policy_route_offpolicy_default_edges;
                artifact.certification_policy_route_default_mode =
                    assertion.certification_compilation
                        .policy_route_default_mode;
                artifact.paired_default_only =
                    assertion.paired_default_only;
                artifact.peak_owned_bytes =
                    assertion.compilation.peak_owned_bytes;
                artifact.previously_accounted_peak_owned_bytes =
                    assertion.compilation
                        .previously_accounted_peak_owned_bytes;
                artifact.complete_peak_owned_bytes =
                    assertion.compilation.complete_peak_owned_bytes;
                return artifact;
            };
        const auto retain_compiled_unverified =
            [&](refinement::CompiledPolicyAssertion& assertion,
                const std::string& kind,
                const std::string& stage,
                const std::string& failure_reason) {
                if (assertion.strategy_json.empty()) return;
                /* A graph that failed independent exact evaluation is useful
                 * diagnostic evidence, but it is not a fallback candidate.
                 * Retaining its complete policy vectors and JSON in the
                 * certified portfolio used to consume the same memory the
                 * strict lift needed to replace it. Record stable metadata and
                 * release the unverified payload before refinement instead. */
                BoundedPolicyIncumbent candidate;
                candidate.certified_upper_bound =
                    std::isfinite(assertion.solver_cost) &&
                            assertion.solver_cost >= 0.0
                        ? assertion.solver_cost
                        : kInfinity;
                candidate.evaluated_policy_cost = kInfinity;
                candidate.kind = kind;
                candidate.compilation_provenance =
                    "compiled_unverified_" + stage + "_v1";
                candidate.final_graph_verification_failure =
                    "compiled_unverified: " + failure_reason;
                candidate.independently_certified = false;
                candidate.independently_evaluated = false;
                candidate.proper = false;
                candidate.executable = false;
                candidate.goal_identity = goal_identity();
                candidate.economy_identity = economy_identity();
                candidate.artifact_identity = artifact_identity();
                std::uint64_t identity = 1469598103934665603ULL;
                identity_mix(identity, candidate.goal_identity);
                identity_mix(identity, candidate.economy_identity);
                identity_mix(identity, candidate.artifact_identity);
                identity_mix_string(identity, candidate.kind);
                identity_mix_string(
                    identity, assertion.strategy_json);
                candidate.portfolio_identity = identity;
                record_candidate_sample(
                    candidate, stage,
                    "diagnostic_metadata_only",
                    candidate.final_graph_verification_failure);
                std::string{}.swap(assertion.strategy_json);
            };
        const auto saturated_add =
            [](const std::uint64_t lhs, const std::uint64_t rhs) {
                return rhs >
                               std::numeric_limits<std::uint64_t>::max() -
                                   lhs
                           ? std::numeric_limits<std::uint64_t>::max()
                           : lhs + rhs;
            };
        const auto classify_bounded_publication = [&] {
            SolveGapTarget fired = SolveGapTarget::None;
            const bool eligible =
                !requested_bounded_finish &&
                !result.diagnostics.resource_cap_hit &&
                !result.diagnostics.state_cap_hit &&
                std::isfinite(result.lower_bound) &&
                std::isfinite(result.upper_bound) &&
                result.lower_bound <=
                    result.upper_bound +
                        value_comparison_tolerance(result.upper_bound);
            if (eligible) {
                const bool absolute =
                    options.max_absolute_optimality_gap > 0.0 &&
                    result.upper_bound - result.lower_bound <=
                        options.max_absolute_optimality_gap;
                const bool relative =
                    options.max_relative_optimality_gap > 0.0 &&
                    result.lower_bound > 0.0 &&
                    result.upper_bound <=
                        (1.0 + options.max_relative_optimality_gap) *
                            result.lower_bound;
                fired = absolute && relative
                    ? SolveGapTarget::Both
                    : absolute
                          ? SolveGapTarget::Absolute
                          : relative ? SolveGapTarget::Relative
                                     : SolveGapTarget::None;
            }
            result.target_met = fired != SolveGapTarget::None;
            result.target_fired = fired;
            result.policy_status = result.target_met
                ? SolvePolicyStatus::BoundedNearOptimal
                : SolvePolicyStatus::BoundedFeasible;
            if (result.target_met) {
                result.termination = SolveTermination::TargetGap;
            }
        };
        bool skip_strict_lift = false;
        bool direct_certification_requires_strict_lift = false;
        if (unverified_selected_policy_candidate.has_value()) {
            PolicyRefinementTelemetry& telemetry =
                result.diagnostics.policy_refinement;
            UnverifiedSelectedPolicyCandidate& selected =
                *unverified_selected_policy_candidate;
            const std::uint64_t selected_capture_identity =
                selected.capture_identity;
            BoundedPolicyIncumbent& snapshot = selected.snapshot;
            telemetry.selected_candidate_certification_attempted = true;
            const auto certification_started =
                std::chrono::steady_clock::now();
            const auto finish_candidate_timing = [&] {
                telemetry.selected_candidate_certification_ns =
                    static_cast<std::uint64_t>(
                        std::chrono::duration_cast<
                            std::chrono::nanoseconds>(
                            std::chrono::steady_clock::now() -
                            certification_started)
                            .count());
            };
            const auto current_prefix_identity =
                incumbent_graph_prefix_identity(
                    snapshot.graph_row_count,
                    snapshot.graph_priced_row_count,
                    snapshot.graph_successor_count,
                    snapshot.graph_probability_count,
                    snapshot.graph_choice_count,
                    snapshot.graph_choice_successor_count,
                    snapshot.graph_choice_option_count);
            const char* invalid_reason = nullptr;
            if (snapshot.goal_identity != goal_identity()) {
                invalid_reason = "goal_identity_changed";
            } else if (snapshot.economy_identity != economy_identity()) {
                invalid_reason = "economy_identity_changed";
            } else if (operators.size() <
                           snapshot.action_vocabulary_size ||
                       snapshot.action_vocabulary_identity !=
                           action_vocabulary_prefix_identity(
                               snapshot.action_vocabulary_size)) {
                /* State-local automatic operators append to the vocabulary
                 * while the saved policy waits for final certification. The
                 * captured row/operator indices remain valid when their
                 * original prefix is unchanged; later operators are merely
                 * additional alternatives and cannot invalidate execution
                 * of this immutable candidate. */
                invalid_reason = "action_vocabulary_prefix_changed";
            } else if (snapshot.artifact_identity != artifact_identity()) {
                invalid_reason = "artifact_generation_changed";
            } else if (snapshot.source_generation >
                           transition_cache->rows.size() ||
                       snapshot.target_generation > calc.state_count()) {
                invalid_reason = "graph_generation_rewound";
            } else if (snapshot.graph_prefix_identity !=
                       current_prefix_identity) {
                invalid_reason = "graph_prefix_changed";
            } else if (selected.has_exact_start_item !=
                           result.has_exact_start_item ||
                       (selected.has_exact_start_item &&
                        std::memcmp(
                            &selected.exact_start_item,
                            &result.exact_start_item,
                            sizeof(pc_item_state)) != 0)) {
                invalid_reason = "exact_start_item_changed";
            }
            std::uint64_t captured_policy_hash =
                1469598103934665603ULL;
            const auto mix_captured =
                [&captured_policy_hash](const std::uint64_t value) {
                    captured_policy_hash ^= value;
                    captured_policy_hash *= 1099511628211ULL;
                };
            for (std::size_t state = 0;
                 state < snapshot.policy_rows.size(); ++state) {
                const std::uint64_t row = snapshot.policy_rows[state];
                mix_captured(state);
                mix_captured(row);
                if (row == std::numeric_limits<std::uint64_t>::max()) {
                    continue;
                }
                if (row >= priced_rows.size()) {
                    invalid_reason = "captured_policy_row_changed";
                    break;
                }
                mix_captured(priced_rows[row].operator_index);
                mix_captured(std::bit_cast<std::uint64_t>(
                    priced_rows[row].cost));
            }
            if (invalid_reason == nullptr &&
                captured_policy_hash != selected.selected_policy_hash) {
                invalid_reason = "selected_policy_changed_after_capture";
            }
            telemetry.selected_candidate_identity_valid =
                invalid_reason == nullptr;
            if (invalid_reason != nullptr) {
                telemetry.selected_candidate_status =
                    "identity_invalidated";
                telemetry.selected_candidate_failure_reason =
                    invalid_reason;
                unverified_selected_policy_candidate.reset();
                finish_candidate_timing();
            } else {
                std::uint64_t preference_projection =
                    static_cast<std::uint64_t>(snapshot.values.size()) *
                    (sizeof(std::vector<std::uint32_t>) +
                     sizeof(std::vector<ObservedUnveilPreference>));
                for (const BoundedPolicyIncumbent::ChoiceSource& source :
                     snapshot.choice_sources) {
                    const std::uint64_t choices = source.choices.size();
                    preference_projection = saturated_add(
                        preference_projection,
                        choices >
                                std::numeric_limits<std::uint64_t>::max() /
                                    128
                            ? std::numeric_limits<std::uint64_t>::max()
                            : choices * 128);
                }
                const std::uint64_t live_before_materialization =
                    estimated_owned_bytes();
                if (!certified_fallback_fits_memory(
                        live_before_materialization,
                        preference_projection,
                        options.max_solver_owned_bytes)) {
                    telemetry.selected_candidate_memory_rejected = true;
                    telemetry.selected_candidate_status =
                        "materialization_memory_rejected";
                    telemetry.selected_candidate_failure_reason =
                        "selected policy preferences do not fit "
                        "max_solver_owned_bytes";
                    unverified_selected_policy_candidate.reset();
                    finish_candidate_timing();
                } else {
                    try {
                        populate_incumbent_policy(snapshot);
                    } catch (const std::exception& error) {
                        telemetry.selected_candidate_status =
                            "materialization_failed";
                        telemetry.selected_candidate_failure_reason =
                            error.what();
                    }
                    if (!snapshot.policy_materialized) {
                        unverified_selected_policy_candidate.reset();
                        finish_candidate_timing();
                    } else if (estimated_owned_bytes() >
                               options.max_solver_owned_bytes) {
                        telemetry.selected_candidate_memory_rejected = true;
                        telemetry.selected_candidate_status =
                            "materialization_memory_rejected";
                        telemetry.selected_candidate_failure_reason =
                            "selected policy materialization exceeded "
                            "max_solver_owned_bytes";
                        unverified_selected_policy_candidate.reset();
                        finish_candidate_timing();
                    } else {
                        telemetry.selected_candidate_owned_bytes =
                            incumbent_owned_bytes(snapshot);
                        const std::optional<SolveOptions> scoped =
                            publication_options_for_live(
                                estimated_owned_bytes());
                        if (!scoped.has_value()) {
                            telemetry.selected_candidate_memory_rejected =
                                true;
                            telemetry.selected_candidate_status =
                                "certification_memory_rejected";
                            telemetry.selected_candidate_failure_reason =
                                "no remaining solver memory for selected "
                                "policy certification";
                            unverified_selected_policy_candidate.reset();
                            finish_candidate_timing();
                        } else {
                            SolveResult proof;
                            proof.policy_available = true;
                            proof.policy_status =
                                SolvePolicyStatus::BoundedFeasible;
                            proof.termination =
                                SolveTermination::NumericalStability;
                            proof.lower_bound = 0.0;
                            proof.upper_bound =
                                selected.selected_estimate;
                            proof.evaluated_policy_cost =
                                selected.selected_estimate;
                            proof.start_state = result.start_state;
                            proof.has_exact_start_item =
                                selected.has_exact_start_item;
                            proof.exact_start_item =
                                selected.exact_start_item;
                            proof.values = snapshot.values;
                            proof.policy = snapshot.policy;
                            proof.policy_reachable =
                                snapshot.policy_reachable;
                            proof.unveil_preferences =
                                snapshot.unveil_preferences;
                            proof.option_unveil_preferences =
                                snapshot.option_unveil_preferences;
                            proof.behavioral_representative_by_state =
                                snapshot.behavioral_representative_by_state;
                            proof.options = *scoped;
                            proof.goal_states.assign(
                                proof.values.size(), 0);
                            proof.expanded.assign(
                                proof.values.size(), 0);
                            for (std::uint32_t state = 0;
                                 state < proof.values.size(); ++state) {
                                proof.goal_states[state] =
                                    calc.is_goal_state(calc.state(state))
                                        ? 1
                                        : 0;
                            }
                            refinement::CompiledPolicyAssertionWork
                                assertion_work(
                                    calc, proof, prices, *scoped,
                                    "selected coarse policy");
                            while (!assertion_work.progress().done) {
                                const auto assertion_progress =
                                    assertion_work.progress();
                                phase = assertion_progress.phase ==
                                        refinement::
                                            CompiledPolicyAssertionPhase::
                                                Compiling
                                    ? SolvePhase::Compiling
                                    : SolvePhase::Certifying;
                                finalization_evaluation_progress =
                                    assertion_progress.evaluation;
                                assertion_work.step(1);
                                co_await solve_detail::
                                    CooperativeCheckpoint{
                                        assertion_work.retained_bytes()};
                            }
                            refinement::CompiledPolicyAssertion assertion =
                                assertion_work.take_result();
                            result.diagnostics.reforge_frontier_work =
                                saturated_add(
                                    result.diagnostics.reforge_frontier_work,
                                    assertion.evaluation.reforge_work);
                            result.diagnostics.reforge_logical_work_v1 =
                                std::min(
                                    options.max_reforge_work,
                                    saturated_add(
                                        result.diagnostics
                                            .reforge_logical_work_v1,
                                        assertion.evaluation
                                            .reforge_logical_work_v1));
                            telemetry.strategy_compilation_ns =
                                saturated_add(
                                    telemetry.strategy_compilation_ns,
                                    assertion.compilation_ns);
                            telemetry.exact_graph_evaluation_ns =
                                saturated_add(
                                    telemetry.exact_graph_evaluation_ns,
                                    assertion.exact_evaluation_ns);
                            telemetry.selected_candidate_exact_cost =
                                assertion.exact_cost;
                            telemetry.selected_candidate_status =
                                assertion_status_name(assertion.status);
                            telemetry.selected_candidate_failure_reason =
                                assertion.failure_reason;
                            const bool verified =
                                assertion.executable && assertion.proper &&
                                assertion.evaluation.cost_complete &&
                                assertion.zero_off_policy &&
                                std::isfinite(assertion.exact_cost) &&
                                assertion.exact_cost >= 0.0;
                            if (verified) {
                                BoundedPolicyIncumbent candidate =
                                    std::move(snapshot);
                                unverified_selected_policy_candidate.reset();
                                candidate.certified_upper_bound =
                                    assertion.exact_cost;
                                candidate.evaluated_policy_cost =
                                    assertion.exact_cost;
                                candidate.compiled_artifact =
                                    retained_artifact_from_assertion(
                                        assertion);
                                candidate.compilation_provenance =
                                    "selected_compiled_policy_assertion_v1";
                                candidate.independently_certified = true;
                                candidate.independently_evaluated = true;
                                candidate.proper = true;
                                candidate.executable = true;
                                candidate.reconciliation_absolute_delta =
                                    assertion.absolute_cost_delta;
                                candidate.reconciliation_relative_delta =
                                    assertion.relative_cost_delta;
                                std::uint64_t identity =
                                    selected_capture_identity;
                                identity_mix(
                                    identity,
                                    std::bit_cast<std::uint64_t>(
                                        candidate.evaluated_policy_cost));
                                identity_mix_string(
                                    identity,
                                    candidate.compilation_provenance);
                                candidate.portfolio_identity = identity;
                                candidate.retained_owned_bytes =
                                    incumbent_owned_bytes(candidate);
                                telemetry.selected_candidate_identity =
                                    identity;
                                telemetry
                                    .selected_candidate_independently_evaluated =
                                    true;
                                assertion.evaluation = {};
                                const std::uint64_t candidate_dynamic =
                                    candidate.retained_owned_bytes -
                                    sizeof(BoundedPolicyIncumbent);
                                if (retain_certified_incumbent(
                                        candidate, candidate_dynamic)) {
                                    telemetry.selected_candidate_retained =
                                        true;
                                    telemetry.selected_candidate_status =
                                        "verified_retained";
                                    telemetry
                                        .selected_candidate_failure_reason
                                        .clear();
                                    record_candidate_sample(
                                        candidate,
                                        "selected_policy_certification",
                                        "eligible_verified_candidate",
                                        assertion.cost_reconciled
                                            ? "selected compiled graph cost "
                                              "reconciled"
                                            : "selected compiled graph owns "
                                              "the bounded evaluated cost");
                                } else {
                                    telemetry.selected_candidate_status =
                                        "verified_retention_rejected";
                                    telemetry
                                        .selected_candidate_failure_reason =
                                        "verified selected candidate did not "
                                        "fit the retained portfolio";
                                }
                            } else {
                                /* Direct graph evaluation can be too broad
                                 * even when the selected row policy is useful.
                                 * Give that same immutable candidate—not the
                                 * restored fallback—to strict quotient lift. */
                                std::string{}.swap(assertion.strategy_json);
                                assertion.evaluation = {};
                                const std::optional<SolveOptions>
                                    strict_options =
                                        publication_options_for_live(
                                            estimated_owned_bytes());
                                const auto strict_started =
                                    std::chrono::steady_clock::now();
                                refinement::PolicyExactLiftCertificate
                                    certificate;
                                if (strict_options.has_value()) {
                                    SolveResult proof;
                                    proof.policy_available = true;
                                    proof.policy_status =
                                        SolvePolicyStatus::BoundedFeasible;
                                    proof.termination =
                                        SolveTermination::NumericalStability;
                                    proof.lower_bound = 0.0;
                                    proof.upper_bound =
                                        selected.selected_estimate;
                                    proof.evaluated_policy_cost =
                                        selected.selected_estimate;
                                    proof.start_state = result.start_state;
                                    proof.has_exact_start_item =
                                        selected.has_exact_start_item;
                                    proof.exact_start_item =
                                        selected.exact_start_item;
                                    proof.values = snapshot.values;
                                    proof.policy = snapshot.policy;
                                    proof.policy_reachable =
                                        snapshot.policy_reachable;
                                    proof.unveil_preferences =
                                        snapshot.unveil_preferences;
                                    proof.option_unveil_preferences =
                                        snapshot
                                            .option_unveil_preferences;
                                    proof.behavioral_representative_by_state =
                                        snapshot
                                            .behavioral_representative_by_state;
                                    proof.options = *strict_options;
                                    proof.goal_states.assign(
                                        proof.values.size(), 0);
                                    proof.expanded.assign(
                                        proof.values.size(), 0);
                                    for (std::uint32_t state = 0;
                                         state < proof.values.size();
                                         ++state) {
                                        proof.goal_states[state] =
                                            calc.is_goal_state(
                                                calc.state(state))
                                                ? 1
                                                : 0;
                                    }
                                    refinement::PolicyExactLiftWork
                                        selected_lift_work(
                                            calc, proof,
                                            selected.exact_start_item,
                                            prices, *strict_options,
                                            "selected policy strict lift");
                                    co_await solve_detail::
                                        CooperativeCheckpoint{
                                            selected_lift_work
                                                .retained_bytes()};
                                    while (!selected_lift_work
                                                .progress().done) {
                                        const refinement::
                                            PolicyExactLiftProgress
                                                lift_progress =
                                                    selected_lift_work
                                                        .progress();
                                        switch (lift_progress.phase) {
                                        case refinement::
                                                PolicyExactLiftPhase::
                                                    Compiling:
                                            phase = SolvePhase::Compiling;
                                            break;
                                        case refinement::
                                                PolicyExactLiftPhase::
                                                    Certifying:
                                            phase = SolvePhase::Certifying;
                                            break;
                                        default:
                                            phase = SolvePhase::Refining;
                                            break;
                                        }
                                        finalization_refinement_states =
                                            lift_progress.strict_states;
                                        finalization_refinement_kernels =
                                            lift_progress.strict_kernels;
                                        finalization_refinement_transitions =
                                            lift_progress
                                                .strict_transitions;
                                        finalization_refinement_rounds =
                                            lift_progress.partition_rounds;
                                        finalization_refinement_classes =
                                            lift_progress.partition_classes;
                                        finalization_verified_upper_bound =
                                            std::min(
                                                finalization_verified_upper_bound,
                                                lift_progress
                                                    .verified_executable_upper_bound);
                                        finalization_evaluation_progress =
                                            lift_progress.evaluation;
                                        selected_lift_work.step(
                                            lift_progress.phase ==
                                                    refinement::
                                                        PolicyExactLiftPhase::
                                                            LocalReoptimization
                                                ? kCooperativeAlternativeProofBatch
                                                : kCooperativePolicyLiftBatch);
                                        record_live_policy_lift_telemetry(
                                            telemetry,
                                            selected_lift_work
                                                .live_adapter_telemetry());
                                        co_await solve_detail::
                                            CooperativeCheckpoint{
                                                selected_lift_work
                                                    .retained_bytes()};
                                    }
                                    certificate =
                                        selected_lift_work.take_result();
                                } else {
                                    certificate.status = refinement::
                                        PolicyExactLiftStatus::ResourceCap;
                                    certificate.resource_cap =
                                        "max_solver_owned_bytes";
                                    certificate.failure_reason =
                                        "no remaining solver memory for "
                                        "selected strict policy lift";
                                }
                                const std::uint64_t strict_elapsed_ns =
                                    static_cast<std::uint64_t>(
                                        std::chrono::duration_cast<
                                            std::chrono::nanoseconds>(
                                            std::chrono::steady_clock::now() -
                                            strict_started)
                                            .count());
                                telemetry.strict_lift_total_ns =
                                    saturated_add(
                                        telemetry.strict_lift_total_ns,
                                        strict_elapsed_ns);
                                telemetry.strict_carrier_discovery_ns =
                                    saturated_add(
                                        telemetry
                                            .strict_carrier_discovery_ns,
                                        certificate.adapter
                                            .carrier_discovery_ns);
                                telemetry.strict_partition_refinement_ns =
                                    saturated_add(
                                        telemetry
                                            .strict_partition_refinement_ns,
                                        certificate.adapter
                                            .partition_refinement_ns);
                                telemetry.strict_policy_evaluation_ns =
                                    saturated_add(
                                        telemetry
                                            .strict_policy_evaluation_ns,
                                        certificate.adapter
                                            .policy_evaluation_ns);
                                telemetry.strict_local_reoptimization_ns =
                                    saturated_add(
                                        telemetry
                                            .strict_local_reoptimization_ns,
                                        certificate.adapter
                                            .local_reoptimization_ns);
                                telemetry.strategy_compilation_ns =
                                    saturated_add(
                                        telemetry.strategy_compilation_ns,
                                        certificate.adapter.compilation_ns);
                                telemetry.exact_graph_evaluation_ns =
                                    saturated_add(
                                        telemetry.exact_graph_evaluation_ns,
                                        certificate.adapter
                                            .exact_evaluation_ns);
                                telemetry.selected_candidate_status =
                                    std::string{"strict_"} +
                                    refinement::
                                        policy_exact_lift_status_name(
                                            certificate.status);
                                telemetry
                                    .selected_candidate_failure_reason =
                                    certificate.failure_reason;
                                telemetry.selected_candidate_exact_cost =
                                    certificate.compiled.exact_cost;
                                telemetry.strict_lift_status =
                                    refinement::
                                        policy_exact_lift_status_name(
                                            certificate.status);
                                telemetry.strict_lift_failure_reason =
                                    certificate.failure_reason;
                                telemetry.strict_lift_resource_cap =
                                    certificate.resource_cap;
                                telemetry.exact_states =
                                    certificate.adapter
                                        .strict_carriers_materialized;
                                telemetry.retained_exact_states =
                                    certificate.refinement.telemetry
                                        .exact_states;
                                telemetry.exact_classes =
                                    certificate.refinement.telemetry
                                        .final_refinement_classes;
                                telemetry.exact_kernels =
                                    certificate.adapter
                                        .strict_kernels_built;
                                telemetry.exact_kernel_cache_hits =
                                    certificate.adapter
                                        .strict_kernel_cache_hits;
                                result.diagnostics.reforge_frontier_work =
                                    saturated_add(
                                        result.diagnostics
                                            .reforge_frontier_work,
                                        saturated_add(
                                            certificate.adapter
                                                .strict_reforge_work,
                                            certificate.compiled.evaluation
                                                .reforge_work));
                                result.diagnostics.reforge_logical_work_v1 =
                                    std::min(
                                        options.max_reforge_work,
                                        saturated_add(
                                            result.diagnostics
                                                .reforge_logical_work_v1,
                                            saturated_add(
                                                certificate.adapter
                                                    .strict_reforge_logical_work_v1,
                                                certificate.compiled
                                                    .evaluation
                                                    .reforge_logical_work_v1)));
                                const bool strict_verified =
                                    certificate.status == refinement::
                                        PolicyExactLiftStatus::Complete &&
                                    certificate.executable &&
                                    certificate.lumpable &&
                                    certificate.compiled.executable &&
                                    certificate.compiled.proper &&
                                    certificate.compiled.evaluation
                                        .cost_complete &&
                                    certificate.compiled.zero_off_policy &&
                                    std::isfinite(
                                        certificate.compiled.exact_cost) &&
                                    certificate.compiled.exact_cost >= 0.0;
                                if (strict_verified) {
                                    BoundedPolicyIncumbent candidate =
                                        std::move(snapshot);
                                    unverified_selected_policy_candidate
                                        .reset();
                                    candidate.certified_upper_bound =
                                        certificate.compiled.exact_cost;
                                    candidate.evaluated_policy_cost =
                                        certificate.compiled.exact_cost;
                                    candidate.compiled_artifact =
                                        retained_artifact_from_assertion(
                                            certificate.compiled);
                                    candidate.compilation_provenance =
                                        "selected_policy_guided_exact_"
                                        "refinement_v1";
                                    candidate.kind =
                                        "selected_policy_guided_exact_"
                                        "refinement";
                                    candidate.strict_state_provenance = true;
                                    candidate.independently_certified = true;
                                    candidate.independently_evaluated = true;
                                    candidate.proper = true;
                                    candidate.executable = true;
                                    candidate
                                        .reconciliation_absolute_delta =
                                        certificate.compiled
                                            .absolute_cost_delta;
                                    candidate
                                        .reconciliation_relative_delta =
                                        certificate.compiled
                                            .relative_cost_delta;
                                    std::uint64_t identity =
                                        selected_capture_identity;
                                    identity_mix(
                                        identity,
                                        std::bit_cast<std::uint64_t>(
                                            candidate
                                                .evaluated_policy_cost));
                                    identity_mix_string(
                                        identity,
                                        candidate
                                            .compilation_provenance);
                                    candidate.portfolio_identity = identity;
                                    candidate.retained_owned_bytes =
                                        incumbent_owned_bytes(candidate);
                                    telemetry.selected_candidate_identity =
                                        identity;
                                    telemetry
                                        .selected_candidate_independently_evaluated =
                                        true;
                                    certificate.compiled.evaluation = {};
                                    const std::uint64_t candidate_dynamic =
                                        candidate.retained_owned_bytes -
                                        sizeof(BoundedPolicyIncumbent);
                                    if (retain_certified_incumbent(
                                            candidate,
                                            candidate_dynamic)) {
                                        telemetry
                                            .selected_candidate_retained =
                                            true;
                                        telemetry
                                            .selected_candidate_status =
                                            "strict_verified_retained";
                                        telemetry
                                            .selected_candidate_failure_reason
                                            .clear();
                                        record_candidate_sample(
                                            candidate,
                                            "selected_policy_strict_lift",
                                            "eligible_verified_candidate",
                                            "strict compiled graph owns the "
                                            "selected policy bounded cost");
                                    } else {
                                        telemetry
                                            .selected_candidate_status =
                                            "strict_verified_retention_"
                                            "rejected";
                                        telemetry
                                            .selected_candidate_failure_reason =
                                            "verified strict selected "
                                            "candidate did not fit the "
                                            "retained portfolio";
                                    }
                                }
                            }
                            finish_candidate_timing();
                        }
                    }
                }
            }
        }
        {
            PolicyRefinementTelemetry& telemetry =
                result.diagnostics.policy_refinement;
            telemetry.core_policy_candidate_present =
                result.policy_available || output_incumbent.has_value() ||
                !certified_fallback_portfolio.empty();
            if (!result.policy_available &&
                telemetry.core_policy_candidate_present) {
                telemetry.core_policy_status =
                    "retained_candidate_not_yet_published";
            }
        }
        if (result.policy_available) {
            PolicyRefinementTelemetry& telemetry =
                result.diagnostics.policy_refinement;
            telemetry.core_policy_candidate_present = true;
            telemetry.core_policy_status =
                result.policy_status == SolvePolicyStatus::Exact
                    ? "exact"
                    : result.policy_status ==
                              SolvePolicyStatus::BoundedNearOptimal
                          ? "bounded_near_optimal"
                          : "bounded_feasible";
            telemetry.core_policy_lower_bound = result.lower_bound;
            telemetry.core_policy_upper_bound = result.upper_bound;
            telemetry.core_policy_evaluated_cost =
                result.evaluated_policy_cost;
            telemetry.core_policy_goal_identity = goal_identity();
            telemetry.core_policy_economy_identity = economy_identity();
            telemetry.core_policy_action_vocabulary_identity =
                action_vocabulary_identity();
            telemetry.core_policy_graph_identity = graph_identity();
            telemetry.core_policy_artifact_identity = artifact_identity();
            telemetry.core_policy_owned_bytes = estimated_owned_bytes();
            std::map<std::string, std::uint64_t> core_action_counts;
            count_policy_actions(result.policy, core_action_counts);
            telemetry.core_policy_distinct_actions =
                core_action_counts.size();
            for (const auto& [unused_action, count] :
                 core_action_counts) {
                (void)unused_action;
                telemetry.core_policy_selected_states = saturated_add(
                    telemetry.core_policy_selected_states, count);
            }
            if (result.start_state < result.policy.size()) {
                const PolicyOperatorRef root =
                    result.policy[result.start_state];
                if (root.index < calc.operators().size()) {
                    telemetry.core_policy_root_action =
                        calc.operators()[root.index].id;
                }
            }
            {
                std::uint64_t hash = 1469598103934665603ULL;
                const auto mix = [&hash](const std::uint64_t value) {
                    hash ^= value;
                    hash *= 1099511628211ULL;
                };
                for (std::size_t i = 0;
                     i < transition_cache->successors.size(); ++i) {
                    mix(transition_cache->successors[i]);
                    mix(std::bit_cast<std::uint64_t>(
                        transition_cache->probabilities[i]));
                }
                for (const SparseRow& row : transition_cache->rows) {
                    mix(row.owner_state);
                    mix(row.transition_offset);
                    mix(row.transition_count);
                    mix(std::bit_cast<std::uint64_t>(
                        row.self_probability));
                    mix(std::bit_cast<std::uint64_t>(
                        row.embedded_self_probability));
                }
                telemetry.core_policy_transition_bits_hash = hash;
                hash = 1469598103934665603ULL;
                for (std::size_t state = 0;
                     state < result.policy.size(); ++state) {
                    mix(state);
                    mix(result.policy[state].index);
                    mix(result.policy[state].kind ==
                                PlannerOperatorKind::Primitive
                            ? 0u
                            : 1u);
                    mix(state < policy_rows.size()
                            ? policy_rows[state]
                            : std::uint64_t{0});
                    if (state < policy_rows.size() &&
                        policy_rows[state] !=
                            std::numeric_limits<std::uint64_t>::max() &&
                        state < restored_policy_row_costs.size()) {
                        mix(std::bit_cast<std::uint64_t>(
                            restored_policy_row_costs[state]));
                    } else if (state < policy_rows.size() &&
                               policy_rows[state] < priced_rows.size()) {
                        mix(std::bit_cast<std::uint64_t>(
                            priced_rows[policy_rows[state]].cost));
                    }
                }
                telemetry.core_policy_bits_hash = hash;
            }

            const SolveTermination core_solve_termination =
                result.termination;
            const std::uint64_t publication_live_bytes =
                estimated_owned_bytes();
            const std::uint64_t publication_retained_solver_bytes =
                estimated_retained_solver_bytes(calc, &result);
            const std::uint64_t publication_external_live_bytes =
                publication_live_bytes > publication_retained_solver_bytes
                    ? publication_live_bytes -
                          publication_retained_solver_bytes
                    : 0;
            const std::optional<SolveOptions> assertion_options =
                publication_options_for_live(publication_live_bytes);
            const auto direct_certification_started =
                std::chrono::steady_clock::now();
            refinement::CompiledPolicyAssertion assertion;
            if (assertion_options.has_value()) {
                SolveOptions scoped_assertion_options =
                    *assertion_options;
                if (options.max_policy_refinement_states != 0) {
                    scoped_assertion_options.max_discovered_states =
                        std::min(
                            scoped_assertion_options.max_discovered_states,
                            options.max_policy_refinement_states);
                }
                refinement::CompiledPolicyAssertionWork assertion_work(
                    calc, result, prices, scoped_assertion_options,
                    "selected core policy");
                while (!assertion_work.progress().done) {
                    const auto assertion_progress =
                        assertion_work.progress();
                    phase = assertion_progress.phase ==
                            refinement::CompiledPolicyAssertionPhase::
                                Compiling
                        ? SolvePhase::Compiling
                        : SolvePhase::Certifying;
                    finalization_evaluation_progress =
                        assertion_progress.evaluation;
                    assertion_work.step(1);
                    co_await solve_detail::CooperativeCheckpoint{
                        assertion_work.retained_bytes()};
                }
                assertion = assertion_work.take_result();
            } else {
                assertion.status =
                    refinement::CompiledPolicyAssertionStatus::ResourceCap;
                assertion.resource_cap = "max_solver_owned_bytes";
                assertion.failure_reason =
                    "coarse live solve leaves no memory for direct exact "
                    "policy certification";
            }
            telemetry.direct_certification_ns =
                static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now() -
                        direct_certification_started)
                        .count());
            telemetry.strategy_compilation_ns +=
                assertion.compilation_ns;
            telemetry.exact_graph_evaluation_ns +=
                assertion.exact_evaluation_ns;
            result.diagnostics.reforge_frontier_work = saturated_add(
                result.diagnostics.reforge_frontier_work,
                assertion.evaluation.reforge_work);
            result.diagnostics.reforge_logical_work_v1 =
                std::min(
                    options.max_reforge_work,
                    saturated_add(
                        result.diagnostics.reforge_logical_work_v1,
                        assertion.evaluation
                            .reforge_logical_work_v1));
            telemetry.direct_certification_status =
                assertion_status_name(assertion.status);
            telemetry.direct_certification_failure_reason =
                assertion.failure_reason;
            telemetry.direct_certification_failure_classification =
                assertion.failure_classification;
            telemetry.direct_certification_resource_cap =
                options.max_policy_refinement_states != 0 &&
                        assertion.resource_cap == "max_discovered_states"
                    ? "max_policy_refinement_states"
                    : assertion.resource_cap;
            telemetry.direct_certification_solver_cost =
                assertion.solver_cost;
            telemetry.direct_certification_exact_cost =
                assertion.exact_cost;
            telemetry.direct_certification_offpolicy_probability =
                assertion.off_policy_probability;
            telemetry.direct_certification_reforge_work =
                assertion.evaluation.reforge_logical_work_v1;
            telemetry.direct_certification_artifact_bytes =
                assertion.strategy_json.capacity() + 1;
            telemetry.direct_certification_peak_owned_bytes =
                saturated_add(
                    publication_external_live_bytes,
                    assertion.publication_peak_owned_bytes);
            telemetry.direct_certification_evaluation_stages =
                assertion.evaluation.stage_timings;
            telemetry.direct_certification_evaluation_max_work_item_ns =
                assertion.evaluation.max_work_item_ns;
            telemetry
                .direct_certification_evaluation_max_work_item_subphase =
                    assertion.evaluation.max_work_item_subphase;
            telemetry.direct_certification_evaluation_boundary =
                assertion.evaluation.boundary_subphase;
            telemetry
                .direct_certification_pair_discovery_index_peak_bytes =
                assertion.evaluation
                    .pair_discovery_index_peak_bytes;
            telemetry.direct_certification_raw_pairs =
                assertion.evaluation.raw_pairs_discovered;
            telemetry.direct_certification_refined_pairs =
                assertion.evaluation.refined_pairs;
            telemetry.direct_certification_refined_pair_limit =
                assertion.evaluation.refined_pair_limit;
            const PolicyCompilationTelemetry& certification_compilation =
                assertion.paired_default_only
                    ? assertion.certification_compilation
                    : assertion.compilation;
            telemetry.direct_certification_route_default_edges =
                certification_compilation
                    .policy_route_default_edges;
            telemetry.direct_product_route_default_edges =
                assertion.paired_default_only
                    ? assertion.compilation.policy_route_default_edges
                    : 0;
            telemetry.direct_certification_route_default_mode =
                certification_compilation
                    .policy_route_default_mode;
            telemetry.direct_product_route_default_mode =
                assertion.paired_default_only
                    ? assertion.compilation.policy_route_default_mode
                    : std::string{};
            telemetry.direct_paired_default_only =
                assertion.paired_default_only;
            telemetry.direct_certification_executable =
                assertion.executable;
            telemetry.direct_certification_proper = assertion.proper;
            telemetry.direct_certification_cost_complete =
                assertion.evaluation.cost_complete;
            telemetry.direct_certification_zero_off_policy =
                assertion.zero_off_policy;
            telemetry.direct_certification_cost_reconciled =
                assertion.cost_reconciled;
            if ((assertion.resource_cap ==
                     "max_solver_owned_bytes" ||
                 assertion.failure_reason.find(
                     "max_owned_bytes") != std::string::npos) &&
                !assertion.failure_reason.empty()) {
                const auto& observation =
                    assertion.evaluation.observation_propagation;
                retain_bounded_json_sample(
                    telemetry.evaluator_memory_samples,
                    telemetry.evaluator_memory_samples_omitted,
                    telemetry.evaluator_memory_sample_bytes,
                    "{\"stage\":\"direct_final_graph_evaluation\","
                        "\"nodes\":" +
                        std::to_string(assertion.compilation.nodes) +
                        ",\"edges\":" +
                        std::to_string(assertion.compilation.edges) +
                        ",\"policy_regions\":" +
                        std::to_string(
                            assertion.compilation.policy_regions) +
                        ",\"reachable_states\":" +
                        std::to_string(
                            assertion.evaluation.raw_pairs_discovered) +
                        ",\"state_action_pairs\":" +
                        std::to_string(
                            assertion.evaluation.refined_pairs) +
                        ",\"observation\":{\"nodes\":" +
                        std::to_string(observation.nodes) +
                        ",\"groups\":" +
                        std::to_string(observation.groups) +
                        ",\"rounds\":" +
                        std::to_string(observation.rounds) +
                        ",\"unique_canonical_requirements\":" +
                        std::to_string(
                            observation.unique_canonical_requirements) +
                        ",\"direct_payload_p50_bytes\":" +
                        std::to_string(
                            observation.direct_payload_p50_bytes) +
                        ",\"direct_payload_p95_bytes\":" +
                        std::to_string(
                            observation.direct_payload_p95_bytes) +
                        ",\"direct_payload_max_bytes\":" +
                        std::to_string(
                            observation.direct_payload_max_bytes) +
                        ",\"propagated_payload_p50_bytes\":" +
                        std::to_string(
                            observation.propagated_payload_p50_bytes) +
                        ",\"propagated_payload_p95_bytes\":" +
                        std::to_string(
                            observation.propagated_payload_p95_bytes) +
                        ",\"propagated_payload_max_bytes\":" +
                        std::to_string(
                            observation.propagated_payload_max_bytes) +
                        ",\"projected_peak_bytes\":" +
                        std::to_string(
                            observation.projected_peak_bytes) +
                        ",\"actual_peak_bytes\":" +
                        std::to_string(
                            observation.actual_peak_bytes) +
                        ",\"retained_bytes\":" +
                        std::to_string(observation.retained_bytes) +
                        ",\"duration_ns\":" +
                        std::to_string(observation.duration_ns) + "}" +
                        ",\"calculation\":\"" +
                        diagnostic_json_escape(
                            assertion.failure_reason) + "\"}");
            }
            telemetry.compiled_assertion_checked =
                assertion.status !=
                refinement::CompiledPolicyAssertionStatus::NotRun;
            telemetry.compiled_policy_proper = assertion.proper;
            telemetry.zero_off_policy = assertion.zero_off_policy;
            telemetry.cost_reconciled = assertion.cost_reconciled;
            telemetry.memory_limit_bytes =
                options.max_solver_owned_bytes;
            telemetry.peak_memory_bytes = std::max(
                telemetry.peak_memory_bytes,
                telemetry.direct_certification_peak_owned_bytes);
            peak_owned_bytes = std::max(
                peak_owned_bytes, telemetry.peak_memory_bytes);
            telemetry.status =
                "direct_assertion_" +
                telemetry.direct_certification_status;

            if (assertion.executable && assertion.proper &&
                assertion.evaluation.cost_complete &&
                assertion.zero_off_policy &&
                std::isfinite(assertion.exact_cost) &&
                assertion.exact_cost >= 0.0) {
                BoundedPolicyIncumbent candidate;
                candidate.certified_upper_bound = assertion.exact_cost;
                candidate.evaluated_policy_cost = assertion.exact_cost;
                candidate.values = result.values;
                if (result.start_state < candidate.values.size()) {
                    candidate.values[result.start_state] =
                        assertion.exact_cost;
                }
                candidate.policy_rows = policy_rows;
                candidate.policy_rows.resize(
                    candidate.values.size(),
                    std::numeric_limits<std::uint64_t>::max());
                candidate.policy_row_costs.assign(
                    candidate.values.size(), kInfinity);
                for (std::size_t state = 0;
                     state < candidate.policy_rows.size(); ++state) {
                    const std::uint64_t row =
                        candidate.policy_rows[state];
                    if (row ==
                            std::numeric_limits<std::uint64_t>::max()) {
                        continue;
                    }
                    if (state < restored_policy_row_costs.size()) {
                        candidate.policy_row_costs[state] =
                            restored_policy_row_costs[state];
                    } else if (row < priced_rows.size()) {
                        candidate.policy_row_costs[state] =
                            priced_rows[row].cost;
                    }
                }
                candidate.policy = result.policy;
                candidate.unveil_preferences =
                    result.unveil_preferences;
                candidate.option_unveil_preferences =
                    result.option_unveil_preferences;
                candidate.behavioral_representative_by_state =
                    result.behavioral_representative_by_state;
                candidate.policy_reachable = result.policy_reachable;
                candidate.primitive_renewal_witness =
                    result.primitive_renewal_witness;
                candidate.compiled_artifact =
                    retained_artifact_from_assertion(assertion);
                candidate.restart_operator = restart_operator_index;
                candidate.restart_state = restart_state;
                candidate.round =
                    result.diagnostics.focused_expansion_rounds;
                candidate.kind = "direct_compiled_core_policy";
                candidate.goal_identity = goal_identity();
                candidate.economy_identity = economy_identity();
                candidate.action_vocabulary_identity =
                    action_vocabulary_identity();
                candidate.action_vocabulary_size = operators.size();
                candidate.graph_identity = graph_identity();
                candidate.artifact_identity = artifact_identity();
                candidate.source_generation =
                    transition_cache->rows.size();
                candidate.target_generation = calc.state_count();
                candidate.graph_row_count =
                    transition_cache->rows.size();
                candidate.graph_priced_row_count = priced_rows.size();
                candidate.graph_successor_count =
                    transition_cache->successors.size();
                candidate.graph_probability_count =
                    transition_cache->probabilities.size();
                candidate.graph_choice_count =
                    transition_cache->choices.size();
                candidate.graph_choice_successor_count =
                    transition_cache->choice_successors.size();
                candidate.graph_choice_option_count =
                    transition_cache->choice_options.size();
                candidate.graph_prefix_identity =
                    incumbent_graph_prefix_identity(
                        candidate.graph_row_count,
                        candidate.graph_priced_row_count,
                        candidate.graph_successor_count,
                        candidate.graph_probability_count,
                        candidate.graph_choice_count,
                        candidate.graph_choice_successor_count,
                        candidate.graph_choice_option_count);
                candidate.compilation_provenance =
                    "direct_compiled_policy_assertion_v1";
                candidate.strict_state_provenance = false;
                candidate.policy_materialized = true;
                candidate.independently_certified = true;
                candidate.independently_evaluated = true;
                candidate.proper = true;
                candidate.executable = true;
                candidate.reconciliation_absolute_delta =
                    assertion.absolute_cost_delta;
                candidate.reconciliation_relative_delta =
                    assertion.relative_cost_delta;
                std::uint64_t identity = 1469598103934665603ULL;
                identity_mix(
                    identity,
                    std::bit_cast<std::uint64_t>(
                        candidate.certified_upper_bound));
                identity_mix(identity, candidate.goal_identity);
                identity_mix(identity, candidate.economy_identity);
                identity_mix(
                    identity, candidate.action_vocabulary_identity);
                identity_mix(identity, candidate.artifact_identity);
                identity_mix(identity, candidate.graph_prefix_identity);
                identity_mix(
                    identity, telemetry.core_policy_bits_hash);
                identity_mix_string(identity, candidate.kind);
                candidate.portfolio_identity = identity;
                candidate.retained_owned_bytes =
                    incumbent_owned_bytes(candidate);
                record_candidate_sample(
                    candidate, "direct_certification",
                    "eligible_verified_candidate",
                    assertion.cost_reconciled
                        ? "final graph cost reconciled"
                        : "executable cost mismatch blocks exactness");

                const double direct_lower_delta = std::abs(
                    candidate.evaluated_policy_cost -
                    result.lower_bound);
                const double direct_lower_relative =
                    std::abs(result.lower_bound) > 1e-12
                        ? direct_lower_delta /
                              std::abs(result.lower_bound)
                        : direct_lower_delta;
                verify_retained_portfolio();
                BoundedPolicyIncumbent* cheaper_verified =
                    best_current_certified_fallback();
                const bool direct_superseded =
                    cheaper_verified != nullptr &&
                    cheaper_verified->evaluated_policy_cost <
                        candidate.evaluated_policy_cost;
                const bool direct_precedes_verified =
                    cheaper_verified == nullptr ||
                    incumbent_precedes(candidate, *cheaper_verified);
                const bool exact_without_refinement =
                    assertion.status ==
                        refinement::CompiledPolicyAssertionStatus::Complete &&
                    assertion.cost_reconciled &&
                    result.converged &&
                    result.policy_status == SolvePolicyStatus::Exact &&
                    std::isfinite(result.lower_bound) &&
                    result.lower_bound >= 0.0 &&
                    !direct_superseded &&
                    (direct_lower_delta <= 1e-7 ||
                     direct_lower_relative <= 1e-9);
                if (exact_without_refinement) {
                    record_candidate_sample(
                        candidate, "publication",
                        "selected_for_publication",
                        "independent graph cost matches the valid global "
                        "lower certificate");
                    result.lower_bound =
                        candidate.evaluated_policy_cost;
                    result.upper_bound =
                        candidate.evaluated_policy_cost;
                    result.evaluated_policy_cost =
                        candidate.evaluated_policy_cost;
                    if (result.start_state < result.values.size()) {
                        result.values[result.start_state] =
                            candidate.evaluated_policy_cost;
                    }
                    result.refined_policy_artifact =
                        std::move(candidate.compiled_artifact);
                    telemetry.retained_artifact_bytes =
                        result.refined_policy_artifact
                                .strategy_json.capacity() +
                        result.refined_policy_artifact
                                .certification_strategy_json.capacity() +
                        2;
                    telemetry.publication_status =
                        "exact_core_policy";
                    telemetry.published_candidate_kind = candidate.kind;
                    telemetry.status =
                        "direct_core_policy_exact";
                    executable_policy_abstraction_supported = true;
                    for (const BoundedPolicyIncumbent& retained :
                         certified_fallback_portfolio) {
                        record_candidate_sample(
                            retained, "publication", "not_selected",
                            retained.independently_evaluated
                                ? "the globally exact direct graph was "
                                  "cheaper or exactly tied with stronger "
                                  "proof authority"
                                : "final graph evaluation did not establish "
                                  "an executable cost");
                    }
                    certified_fallback_portfolio.clear();
                    certified_fallback_portfolio.shrink_to_fit();
                    telemetry.fallback_portfolio_candidates = 0;
                    telemetry.fallback_portfolio_owned_bytes = 0;
                    skip_strict_lift = true;
                } else if (telemetry.triggers == 0) {
                    const std::uint64_t candidate_dynamic_bytes =
                        incumbent_owned_bytes(candidate) -
                        sizeof(BoundedPolicyIncumbent);
                    if (retain_certified_incumbent(
                            candidate, candidate_dynamic_bytes)) {
                        telemetry.direct_candidate_retained = true;
                        skip_strict_lift =
                            publish_certified_fallback(
                                core_solve_termination);
                    } else {
                        record_candidate_sample(
                            candidate, "publication",
                            "selected_for_publication",
                            "only verified direct candidate fit the live "
                            "publication memory budget");
                        result.refined_policy_artifact =
                            std::move(candidate.compiled_artifact);
                        telemetry.retained_artifact_bytes =
                            result.refined_policy_artifact
                                    .strategy_json.capacity() +
                            result.refined_policy_artifact
                                    .certification_strategy_json.capacity() +
                            2;
                        result.converged = false;
                        result.lower_bound =
                            std::isfinite(result.lower_bound) &&
                                    result.lower_bound <=
                                        candidate.evaluated_policy_cost +
                                            value_comparison_tolerance(
                                                candidate
                                                    .evaluated_policy_cost)
                                ? std::max(0.0, result.lower_bound)
                                : 0.0;
                        result.upper_bound =
                            candidate.evaluated_policy_cost;
                        result.evaluated_policy_cost =
                            candidate.evaluated_policy_cost;
                        result.termination =
                            successful_refined_publication_termination(
                                core_solve_termination,
                                result.diagnostics.resource_cap_hit,
                                false,
                                coarse_discovery_closed);
                        classify_bounded_publication();
                        result.diagnostics.solution_scope =
                            "direct_certified_core_policy_bounded";
                        result.diagnostics.incumbent_kind =
                            candidate.kind;
                        result.diagnostics
                            .policy_publication_failure_reason.clear();
                        result.diagnostics
                            .policy_compatibility_supported = true;
                        telemetry.bounded_publication_retained = true;
                        telemetry.publication_status =
                            "bounded_core_policy";
                        telemetry.published_candidate_kind =
                            candidate.kind;
                        telemetry.status =
                            "direct_core_policy_bounded";
                        executable_policy_abstraction_supported = true;
                        skip_strict_lift = true;
                    }
                } else if (retain_certified_incumbent(
                               candidate,
                               incumbent_owned_bytes(candidate) -
                                   sizeof(BoundedPolicyIncumbent))) {
                    telemetry.direct_candidate_retained = true;
                    /* A nonzero refinement allowance is the product's
                     * bounded optional-proof budget. Once direct compilation
                     * has independently established a proper, zero-offpolicy,
                     * completely priced executable that improves the retained
                     * portfolio, publish that exact evaluated cost as the
                     * bounded upper. A solver/exact cost mismatch still
                     * blocks exactness, but it must not trigger a second
                     * strict-lift traversal that delays the better strategy
                     * past the product boundary. Native callers that leave
                     * the allowance at zero retain the historical exhaustive
                     * strict-lift behavior. */
                    if (options.max_policy_refinement_states != 0 &&
                        direct_precedes_verified) {
                        skip_strict_lift =
                            publish_certified_fallback(
                                core_solve_termination);
                    }
                } else {
                    verify_retained_portfolio();
                    BoundedPolicyIncumbent* retained =
                        best_current_certified_fallback();
                    if (retained != nullptr &&
                        incumbent_precedes(*retained, candidate)) {
                        skip_strict_lift =
                            publish_certified_fallback(
                                core_solve_termination);
                    } else {
                    record_candidate_sample(
                        candidate, "publication",
                        "selected_for_publication",
                        "verified direct candidate is cheaper than every "
                        "retained verified fallback");
                    /* The asserted artifact itself already fit the live solve
                     * cap. If duplicating the complete incumbent for a later
                     * strict-lift fallback does not fit, publish it now rather
                     * than spending more memory and losing it. */
                    result.refined_policy_artifact =
                        std::move(candidate.compiled_artifact);
                    telemetry.retained_artifact_bytes =
                        result.refined_policy_artifact
                                .strategy_json.capacity() +
                        result.refined_policy_artifact
                                .certification_strategy_json.capacity() +
                        2;
                    const double exact_cost =
                        candidate.evaluated_policy_cost;
                    result.converged = false;
                    result.lower_bound =
                        std::isfinite(result.lower_bound) &&
                                result.lower_bound <=
                                    exact_cost +
                                        value_comparison_tolerance(
                                            exact_cost)
                            ? std::max(0.0, result.lower_bound)
                            : 0.0;
                    result.upper_bound = exact_cost;
                    result.evaluated_policy_cost = exact_cost;
                    result.absolute_optimality_gap = std::max(
                        0.0, exact_cost - result.lower_bound);
                    result.relative_optimality_gap =
                        result.lower_bound > 0.0
                            ? std::max(
                                  0.0,
                                  exact_cost / result.lower_bound - 1.0)
                            : kInfinity;
                    result.termination =
                        successful_refined_publication_termination(
                            core_solve_termination,
                            result.diagnostics.resource_cap_hit,
                            false,
                            coarse_discovery_closed);
                    classify_bounded_publication();
                    result.diagnostics.solution_scope =
                        "direct_certified_core_policy_bounded";
                    result.diagnostics.incumbent_kind = candidate.kind;
                    result.diagnostics
                        .policy_publication_failure_reason.clear();
                    result.diagnostics.policy_compatibility_supported =
                        true;
                    telemetry.bounded_publication_retained = true;
                    telemetry.publication_status =
                        "bounded_core_policy";
                    telemetry.published_candidate_kind = candidate.kind;
                    telemetry.status =
                        "direct_core_policy_bounded";
                    executable_policy_abstraction_supported = true;
                    skip_strict_lift = true;
                    }
                }
            } else {
                const std::string reason =
                    "policy_direct_certification_" +
                    telemetry.direct_certification_status;
                telemetry.preferred_publication_failure_reason =
                    assertion.failure_reason.empty()
                        ? reason
                        : reason + ": " + assertion.failure_reason;
                retain_compiled_unverified(
                    assertion, "direct_compiled_core_policy",
                    "direct_final_graph_evaluation",
                    telemetry.preferred_publication_failure_reason);
                const bool refinement_state_budget_exhausted =
                    options.max_policy_refinement_states != 0 &&
                    assertion.resource_cap == "max_discovered_states";
                if (refinement_state_budget_exhausted) {
                    /* This cap owns optional proof improvement, not the
                     * already verified executable fallback. Do not spend the
                     * same exhausted allowance again in strict lift before
                     * returning that bounded policy. */
                    verify_retained_portfolio();
                    if (best_current_certified_fallback() != nullptr) {
                        skip_strict_lift = publish_certified_fallback(
                            core_solve_termination);
                    }
                }
                direct_certification_requires_strict_lift =
                    telemetry.triggers == 0 && !skip_strict_lift;
            }
        }
        if (result.policy_available &&
            (result.diagnostics.policy_refinement.triggers != 0 ||
             direct_certification_requires_strict_lift) &&
            !skip_strict_lift) {
            /* Direct assertion has finished and its candidate has been
             * recorded. Yield before allocating/starting strict lift so a
             * WASM step never owns both proof stages' completion work. */
            phase = SolvePhase::Refining;
            co_await solve_detail::CooperativeCheckpoint{};
            /* Final-graph verification of already retained fallbacks happens
             * before optional strict work so a later lift cannot consume the
             * remaining allowance and erase an executable candidate. */
            verify_retained_portfolio();
            /*
             * Exact lifting changes the publication proof, not the stopping
             * cause of the coarse solve that supplied the incumbent.
             */
            const SolveTermination coarse_solve_termination =
                result.termination;
            /*
             * A coarse compatibility witness is now a request for bounded
             * exact lifting. The adapter uses the authored start carrier,
             * strict mechanics kernels, the shared observation partition,
             * and the shared proper-policy evaluator. Publication retains
             * only the strict strategy artifact that independently compiles,
             * exact-evaluates, and reconciles.
             */
            const std::uint64_t publication_live_bytes =
                estimated_owned_bytes();
            const std::uint64_t publication_retained_solver_bytes =
                estimated_retained_solver_bytes(calc, &result);
            const std::uint64_t publication_external_live_bytes =
                publication_live_bytes >
                        publication_retained_solver_bytes
                    ? publication_live_bytes -
                          publication_retained_solver_bytes
                    : 0;
            const std::optional<SolveOptions> lift_options =
                publication_options_for_live(
                    publication_live_bytes);
            const auto strict_lift_started =
                std::chrono::steady_clock::now();
            refinement::PolicyExactLiftCertificate certificate;
            if (lift_options.has_value()) {
                SolveOptions scoped_lift_options = *lift_options;
                if (options.max_policy_refinement_states != 0) {
                    scoped_lift_options.max_discovered_states = std::min(
                        scoped_lift_options.max_discovered_states,
                        options.max_policy_refinement_states);
                }
                co_await solve_detail::CooperativeCheckpoint{};
                refinement::PolicyExactLiftWork lift_work(
                    calc, result, exact_start_item, prices,
                    scoped_lift_options, "solved policy");
                co_await solve_detail::CooperativeCheckpoint{
                    lift_work.retained_bytes()};
                while (!lift_work.progress().done) {
                    const refinement::PolicyExactLiftProgress
                        lift_progress = lift_work.progress();
                    switch (lift_progress.phase) {
                    case refinement::PolicyExactLiftPhase::Compiling:
                        phase = SolvePhase::Compiling;
                        break;
                    case refinement::PolicyExactLiftPhase::Certifying:
                        phase = SolvePhase::Certifying;
                        break;
                    default:
                        phase = SolvePhase::Refining;
                        break;
                    }
                    finalization_refinement_states =
                        lift_progress.strict_states;
                    finalization_refinement_kernels =
                        lift_progress.strict_kernels;
                    finalization_refinement_transitions =
                        lift_progress.strict_transitions;
                    finalization_refinement_rounds =
                        lift_progress.partition_rounds;
                    finalization_refinement_classes =
                        lift_progress.partition_classes;
                    finalization_verified_upper_bound = std::min(
                        finalization_verified_upper_bound,
                        lift_progress
                            .verified_executable_upper_bound);
                    finalization_evaluation_progress =
                        lift_progress.evaluation;
                    lift_work.step(
                        lift_progress.phase == refinement::
                                PolicyExactLiftPhase::LocalReoptimization
                            ? kCooperativeAlternativeProofBatch
                            : kCooperativePolicyLiftBatch);
                    {
                        PolicyRefinementTelemetry& live_telemetry =
                            result.diagnostics.policy_refinement;
                        live_telemetry.status = "strict_lift_running";
                        live_telemetry.strict_lift_status =
                            "strict_lift_running";
                        record_live_policy_lift_telemetry(
                            live_telemetry,
                            lift_work.live_adapter_telemetry());
                    }
                    co_await solve_detail::CooperativeCheckpoint{
                        lift_work.retained_bytes()};
                }
                certificate = lift_work.take_result();
            } else {
                certificate.status =
                    refinement::PolicyExactLiftStatus::ResourceCap;
                certificate.resource_cap =
                    "max_solver_owned_bytes";
                certificate.failure_reason =
                    "coarse live solve leaves no memory for exact "
                    "publication refinement";
            }
            PolicyRefinementTelemetry& telemetry =
                result.diagnostics.policy_refinement;
            telemetry.strict_lift_total_ns =
                static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now() -
                        strict_lift_started)
                        .count());
            telemetry.strict_carrier_discovery_ns =
                certificate.adapter.carrier_discovery_ns;
            telemetry.strict_partition_refinement_ns =
                certificate.adapter.partition_refinement_ns;
            telemetry.strict_policy_evaluation_ns =
                certificate.adapter.policy_evaluation_ns;
            telemetry.strict_local_reoptimization_ns =
                certificate.adapter.local_reoptimization_ns;
            telemetry.strategy_compilation_ns +=
                certificate.adapter.compilation_ns;
            telemetry.exact_graph_evaluation_ns +=
                certificate.adapter.exact_evaluation_ns;
            telemetry.status =
                refinement::policy_exact_lift_status_name(
                    certificate.status);
            telemetry.strict_lift_status = telemetry.status;
            telemetry.strict_lift_failure_reason =
                certificate.failure_reason;
            telemetry.strict_lift_resource_cap =
                options.max_policy_refinement_states != 0 &&
                        certificate.resource_cap == "max_discovered_states"
                    ? "max_policy_refinement_states"
                    : certificate.resource_cap;
            telemetry.strict_global_lower_bound_closed =
                certificate.global_lower_bound_closed;
            telemetry.global_lower_bound_closed =
                certificate.adapter.global_lower_bound_closed;
            if (certificate.status ==
                    refinement::PolicyExactLiftStatus::InvalidSolveState ||
                certificate.status ==
                    refinement::PolicyExactLiftStatus::CoarseMappingFailure ||
                certificate.status ==
                    refinement::PolicyExactLiftStatus::ObservationUnavailable) {
                retain_bounded_json_sample(
                    telemetry.structural_failure_samples,
                    telemetry.structural_failure_samples_omitted,
                    telemetry.structural_failure_sample_bytes,
                    "{\"status\":\"" +
                        std::string{
                            refinement::policy_exact_lift_status_name(
                                certificate.status)} +
                        "\",\"coarse_states\":" +
                        std::to_string(
                            certificate.adapter.coarse_policy_states) +
                        ",\"strict_states\":" +
                        std::to_string(
                            certificate.adapter.strict_states_discovered) +
                        ",\"strict_carriers\":" +
                        std::to_string(
                            certificate.adapter
                                .strict_carriers_materialized) +
                        ",\"solved_policy_size\":" +
                        std::to_string(result.policy.size()) +
                        ",\"partition_identity\":" +
                        std::to_string(
                            telemetry.core_policy_bits_hash) +
                        ",\"mapping_identity\":" +
                        std::to_string(
                            telemetry
                                .core_policy_transition_bits_hash) +
                        ",\"selected_operator\":\"" +
                        diagnostic_json_escape(
                            telemetry.core_policy_root_action) +
                        "\",\"first_violated_invariant\":\"" +
                        diagnostic_json_escape(
                            certificate.failure_reason) + "\"}");
            }
            if ((certificate.compiled.resource_cap ==
                     "max_solver_owned_bytes" ||
                 certificate.compiled.failure_reason.find(
                     "max_owned_bytes") != std::string::npos) &&
                !certificate.compiled.failure_reason.empty()) {
                retain_bounded_json_sample(
                    telemetry.evaluator_memory_samples,
                    telemetry.evaluator_memory_samples_omitted,
                    telemetry.evaluator_memory_sample_bytes,
                    "{\"stage\":\"strict_final_graph_evaluation\","
                        "\"nodes\":" +
                        std::to_string(
                            certificate.compiled.compilation.nodes) +
                        ",\"edges\":" +
                        std::to_string(
                            certificate.compiled.compilation.edges) +
                        ",\"policy_regions\":" +
                        std::to_string(
                            certificate.compiled.compilation
                                .policy_regions) +
                        ",\"reachable_states\":" +
                        std::to_string(
                            certificate.compiled.evaluation
                                .raw_pairs_discovered) +
                        ",\"state_action_pairs\":" +
                        std::to_string(
                            certificate.compiled.evaluation
                                .refined_pairs) +
                        ",\"calculation\":\"" +
                        diagnostic_json_escape(
                            certificate.compiled.failure_reason) +
                        "\"}");
            }
            telemetry.memory_limit_bytes =
                options.max_solver_owned_bytes;
            telemetry.strict_session_constructions =
                certificate.adapter.strict_session_constructions;
            telemetry.strict_full_restarts =
                certificate.adapter.strict_full_restarts;
            telemetry.strict_partition_updates =
                certificate.adapter.strict_partition_updates;
            telemetry.strict_frontier_insertions =
                certificate.adapter.strict_frontier_insertions;
            telemetry.strict_frontier_states_inserted =
                certificate.adapter.strict_frontier_states_inserted;
            telemetry.strict_frontier_update_max_ns =
                certificate.adapter.strict_frontier_update_max_ns;
            telemetry.strict_cells_retained =
                certificate.adapter.strict_cells_retained;
            telemetry.strict_cells_created =
                certificate.adapter.strict_cells_created;
            telemetry.strict_cells_superseded =
                certificate.adapter.strict_cells_superseded;
            telemetry.policy_reachable_coarse_states =
                certificate.refinement.telemetry
                    .policy_reachable_coarse_states;
            telemetry.exact_states =
                certificate.adapter.strict_carriers_materialized;
            telemetry.retained_exact_states =
                certificate.refinement.telemetry.exact_states;
            telemetry.exact_classes =
                certificate.refinement.telemetry
                    .final_refinement_classes;
            telemetry.initial_observation_classes =
                certificate.refinement.telemetry
                    .initial_observation_classes;
            telemetry.behavior_splits =
                certificate.refinement.telemetry.behavior_splits;
            telemetry.merged_exact_states =
                certificate.refinement.telemetry.merged_exact_states;
            telemetry.exact_transitions =
                certificate.adapter.strict_transitions_built;
            telemetry.exact_kernels =
                certificate.adapter.strict_kernels_built;
            telemetry.exact_kernel_cache_hits =
                certificate.adapter.strict_kernel_cache_hits;
            telemetry.selected_rows_begun =
                certificate.adapter.selected_rows_begun;
            telemetry.selected_rows_completed =
                certificate.adapter.selected_rows_completed;
            telemetry.selected_reforge_work =
                certificate.adapter.selected_reforge_work;
            telemetry.selected_transitions =
                certificate.adapter.selected_transitions;
            telemetry.alternative_rows_begun =
                certificate.adapter.alternative_rows_begun;
            telemetry.alternative_rows_completed =
                certificate.adapter.alternative_rows_completed;
            telemetry.alternative_reforge_work =
                certificate.adapter.alternative_reforge_work;
            telemetry.alternative_transitions =
                certificate.adapter.alternative_transitions;
            telemetry.strict_reforge_active_work =
                certificate.adapter.strict_reforge_work;
            telemetry.strict_reforge_logical_work_v1 =
                certificate.adapter.strict_reforge_logical_work_v1;
            telemetry.strict_reforge_evaluator_work_v1 =
                certificate.adapter.strict_reforge_evaluator_work_v1;
            telemetry.strict_reforge_evaluator_work_v2 =
                certificate.adapter.strict_reforge_evaluator_work_v2;
            telemetry.strict_reforge_evaluator_work_v3 =
                certificate.adapter.strict_reforge_evaluator_work_v3;
            telemetry.strict_reforge_effort =
                certificate.adapter.strict_reforge_effort;
            telemetry.strict_reforge_row_samples =
                std::move(
                    certificate.adapter.strict_reforge_row_samples);
            telemetry.strict_reforge_row_samples_omitted =
                certificate.adapter
                    .strict_reforge_row_samples_omitted;
            telemetry.broad_row_attribution =
                std::move(certificate.adapter.broad_row_attribution);
            telemetry.broad_row_attribution_omitted =
                certificate.adapter.broad_row_attribution_omitted;
            telemetry.work_to_first_partition =
                certificate.adapter.work_to_first_partition;
            telemetry.work_to_first_executable_upper =
                certificate.adapter.work_to_first_executable_upper;
            telemetry.wall_ns_to_first_partition =
                certificate.adapter.wall_ns_to_first_partition;
            telemetry.wall_ns_to_first_executable_upper =
                certificate.adapter.wall_ns_to_first_executable_upper;
            telemetry.alternatives_materialized_before_first_upper =
                certificate.adapter
                    .alternatives_materialized_before_first_upper;
            telemetry.alternative_obligations_created =
                certificate.adapter.alternative_obligations_created;
            telemetry.unresolved_alternative_obligations =
                certificate.adapter.unresolved_alternative_obligations;
            telemetry.alternative_rows_avoided =
                certificate.adapter.alternative_rows_avoided;
            telemetry.action_accounting_complete =
                certificate.adapter.action_accounting_complete;
            telemetry.alternative_scheduling_rounds =
                certificate.adapter.alternative_scheduling_rounds;
            telemetry.alternative_obligations_scheduled =
                certificate.adapter.alternative_obligations_scheduled;
            telemetry.alternative_obligations_certified =
                certificate.adapter.alternative_obligations_certified;
            telemetry.alternative_obligations_partially_evaluated =
                certificate.adapter
                    .alternative_obligations_partially_evaluated;
            telemetry.alternative_obligations_noncompetitive =
                certificate.adapter
                    .alternative_obligations_noncompetitive;
            telemetry.alternative_obligations_stale =
                certificate.adapter.alternative_obligations_stale;
            telemetry.alternative_verdict_revocations =
                certificate.adapter.alternative_verdict_revocations;
            telemetry.alternative_obligations_resource_interrupted =
                certificate.adapter
                    .alternative_obligations_resource_interrupted;
            telemetry.competitive_alternatives_remaining =
                certificate.adapter.competitive_alternatives_remaining;
            telemetry.alternative_policy_improvements =
                certificate.adapter.alternative_policy_improvements;
            telemetry.bounded_publication_retained =
                certificate.adapter.bounded_publication_retained;
            telemetry.exact_alternative_envelope_closed =
                certificate.adapter.exact_alternative_envelope_closed;
            telemetry.exact_state_reuses =
                certificate.adapter.canonical_successor_collapses;
            telemetry.collapse_events =
                certificate.refinement.telemetry.collapse_events;
            telemetry.collapse_destroyed_feature_mask =
                certificate.refinement.telemetry
                    .collapse_destroyed_feature_mask;
            telemetry.collapse_preserved_feature_mask =
                certificate.refinement.telemetry
                    .collapse_preserved_feature_mask;
            telemetry.collapse_events_by_feature =
                certificate.refinement.telemetry
                    .collapse_events_by_feature;
            telemetry.preservation_events_by_feature =
                certificate.refinement.telemetry
                    .preservation_events_by_feature;
            telemetry.backward_observation_rounds =
                certificate.adapter.backward_observation_rounds;
            telemetry.selected_action_routing_rounds =
                certificate.refinement.telemetry
                    .selected_action_routing_rounds;
            telemetry.observation_propagation_rounds =
                certificate.refinement.telemetry
                    .observation_propagation_rounds;
            telemetry.partition_refinement_rounds =
                certificate.refinement.telemetry
                    .partition_refinement_rounds;
            telemetry.local_reoptimization_rounds =
                certificate.adapter.local_reoptimization_rounds;
            telemetry.local_state_action_rows_scheduled =
                certificate.adapter
                    .local_state_action_rows_scheduled;
            telemetry.local_state_action_rows_evaluated =
                certificate.adapter
                    .local_state_action_rows_evaluated;
            telemetry.refinement_rounds =
                static_cast<std::uint64_t>(
                    certificate.adapter.backward_observation_rounds) +
                certificate.adapter.exact_fixed_point_rounds +
                certificate.adapter.local_reoptimization_rounds;
            telemetry.local_reoptimizations =
                certificate.adapter.local_reoptimizations;
            telemetry.local_policy_changes =
                certificate.adapter.local_policy_changes;
            telemetry.local_value_changes =
                certificate.adapter.local_value_changes;
            telemetry.proof_payload_reuses =
                certificate.adapter.proof_payload_reuses;
            telemetry.row_reprojections =
                certificate.adapter.row_reprojections;
            telemetry.quotient_source_splits =
                certificate.adapter.quotient_source_splits;
            telemetry.quotient_target_splits =
                certificate.adapter.quotient_target_splits;
            telemetry.reverse_invalidations =
                certificate.adapter.reverse_invalidations;
            telemetry.improper_policy_repairs =
                certificate.adapter.improper_policy_repairs;
            telemetry.exact_carriers_replayed =
                certificate.adapter.exact_carriers_replayed;
            telemetry.current_live_slices =
                certificate.adapter.current_live_slices;
            telemetry.peak_live_slices =
                certificate.adapter.peak_live_slices;
            telemetry.current_live_slice_bytes =
                certificate.adapter.current_live_slice_bytes;
            telemetry.peak_live_slice_bytes =
                certificate.adapter.peak_live_slice_bytes;
            telemetry.coverage_descriptor_bytes =
                certificate.adapter.coverage_descriptor_bytes;
            telemetry.certificate_bytes =
                certificate.adapter.certificate_bytes;
            telemetry.dependency_sidecar_bytes =
                certificate.adapter.dependency_sidecar_bytes;
            telemetry.alternative_obligation_bytes =
                certificate.adapter.alternative_obligation_bytes;
            telemetry.partition_bytes =
                certificate.adapter.partition_bytes;
            telemetry.carrier_bytes =
                certificate.adapter.carrier_bytes;
            telemetry.row_kernel_bytes =
                certificate.adapter.row_kernel_bytes;
            telemetry.scratch_bytes =
                certificate.adapter.scratch_bytes;
            telemetry.total_solver_owned_bytes =
                certificate.adapter.total_solver_owned_bytes;
            telemetry.quotient_attractor_ns =
                certificate.adapter.quotient_attractor_ns;
            telemetry.quotient_lower_relaxation_ns =
                certificate.adapter.quotient_lower_relaxation_ns;
            telemetry.quotient_policy_seed_ns =
                certificate.adapter.quotient_policy_seed_ns;
            telemetry.quotient_envelope_construction_ns =
                certificate.adapter.quotient_envelope_construction_ns;
            telemetry.quotient_exact_policy_evaluation_ns =
                certificate.adapter.quotient_exact_policy_evaluation_ns;
            telemetry.quotient_policy_improvement_ns =
                certificate.adapter.quotient_policy_improvement_ns;
            telemetry.quotient_publication_audit_ns =
                certificate.adapter.quotient_publication_audit_ns;
            telemetry.reference_adapter_invocations =
                certificate.adapter.reference_adapter_invocations;
            telemetry.lumpability_checks =
                certificate.refinement.telemetry.lumpability_checks;
            telemetry.fixed_point_checked =
                certificate.refinement.status !=
                refinement::RefinementStatus::EmptyRequest;
            telemetry.fixed_point_complete =
                certificate.refinement.status ==
                refinement::RefinementStatus::Complete;
            telemetry.lumpability_checked =
                telemetry.fixed_point_complete ||
                telemetry.lumpability_checks != 0;
            telemetry.lumpable = certificate.refinement.lumpable;
            telemetry.class_policy_checked =
                telemetry.fixed_point_complete;
            telemetry.class_policy_proper =
                certificate.class_evaluation.proper;
            telemetry.compiled_assertion_checked =
                certificate.compiled.status !=
                refinement::CompiledPolicyAssertionStatus::NotRun;
            telemetry.compiled_policy_proper =
                certificate.compiled.proper;
            telemetry.zero_off_policy =
                certificate.compiled.zero_off_policy;
            telemetry.cost_reconciled =
                certificate.compiled.cost_reconciled;
            telemetry.policy_changed = certificate.policy_changed;
            telemetry.coarse_value_reconciled =
                certificate.coarse_value_reconciled;
            const auto saturated_add =
                [](const std::uint64_t lhs,
                   const std::uint64_t rhs) {
                    return rhs >
                                   std::numeric_limits<
                                       std::uint64_t>::max() -
                                       lhs
                               ? std::numeric_limits<
                                     std::uint64_t>::max()
                               : lhs + rhs;
                };
            const auto counterexample_kind_name =
                [](const refinement::CounterexampleKind kind) {
                    switch (kind) {
                    case refinement::CounterexampleKind::Observation:
                        return "observation";
                    case refinement::CounterexampleKind::SelectedAction:
                        return "selected_action";
                    case refinement::CounterexampleKind::ActionCost:
                        return "action_cost";
                    case refinement::CounterexampleKind::
                            SuccessorProjection:
                        return "successor_projection";
                    }
                    return "observation";
                };
            telemetry.counterexamples = saturated_add(
                telemetry.counterexamples,
                saturated_add(
                    certificate.refinement.counterexamples.size(),
                    certificate.refinement.telemetry
                        .witnesses_omitted));
            for (const refinement::RefinementCounterexample& witness :
                 certificate.refinement.counterexamples) {
                RefinementFeatureMask differing_feature_mask = 0;
                for (const refinement::FeatureAtom& atom :
                     witness.differing_features) {
                    differing_feature_mask |=
                        refinement_feature(atom.feature);
                }
                const std::string sample =
                    "{\"source\":\"refinement\",\"kind\":\"" +
                    std::string{counterexample_kind_name(witness.kind)} +
                    "\",\"coarse_state\":" +
                    std::to_string(witness.coarse_state) +
                    ",\"differing_feature_mask\":" +
                    std::to_string(differing_feature_mask) + "}";
                if (telemetry.counterexample_samples.size() <
                    result.diagnostics.diagnostic_sample_limit) {
                    telemetry.counterexample_samples.push_back(sample);
                } else {
                    ++telemetry.counterexample_samples_omitted;
                }
            }
            telemetry.counterexample_samples_omitted = saturated_add(
                telemetry.counterexample_samples_omitted,
                certificate.refinement.telemetry.witnesses_omitted);
            result.diagnostics.reforge_frontier_work =
                saturated_add(
                    result.diagnostics.reforge_frontier_work,
                    saturated_add(
                        certificate.adapter.strict_reforge_work,
                        certificate.compiled.evaluation.reforge_work));
            result.diagnostics.reforge_logical_work_v1 =
                std::min(
                    options.max_reforge_work,
                    saturated_add(
                        result.diagnostics.reforge_logical_work_v1,
                        saturated_add(
                            certificate.adapter
                                .strict_reforge_logical_work_v1,
                            certificate.compiled.evaluation
                                .reforge_logical_work_v1)));
            const std::uint64_t adapter_memory = std::max(
                certificate.adapter.adapter_owned_bytes,
                certificate.adapter.strict_calc_owned_bytes);
            const std::uint64_t adapter_peak = std::max(
                certificate.adapter.peak_adapter_owned_bytes,
                adapter_memory);
            const std::uint64_t refinement_memory = std::max(
                certificate.refinement.telemetry
                    .estimated_memory_bytes,
                adapter_memory);
            const std::uint64_t refinement_peak = std::max(
                certificate.refinement.telemetry
                    .peak_estimated_memory_bytes,
                adapter_peak);
            const std::uint64_t refinement_phase_memory =
                saturated_add(
                    publication_live_bytes,
                    refinement_memory);
            const std::uint64_t refinement_phase_peak =
                saturated_add(
                    publication_live_bytes,
                    refinement_peak);
            const std::uint64_t compiled_phase_memory =
                saturated_add(
                    publication_live_bytes,
                    saturated_add(
                        certificate.compiled.retained_solver_bytes,
                        saturated_add(
                            certificate.compiled.parsed_strategy_bytes,
                            saturated_add(
                                certificate.compiled.economy_bytes,
                                saturated_add(
                                    certificate.compiled.evaluation
                                        .owned_bytes_estimate,
                                    certificate.compiled.strategy_json
                                            .capacity() +
                                        1)))));
            const std::uint64_t compiled_phase_peak =
                saturated_add(
                    publication_live_bytes,
                    saturated_add(
                        certificate.compiled.retained_solver_bytes,
                        saturated_add(
                            certificate.compiled.parsed_strategy_bytes,
                            saturated_add(
                                certificate.compiled.economy_bytes,
                                saturated_add(
                                    certificate.compiled.evaluation
                                        .peak_owned_bytes_estimate,
                                     certificate.compiled.strategy_json
                                             .capacity() +
                                         1)))));
            const std::uint64_t reported_compiled_phase_peak =
                saturated_add(
                    publication_external_live_bytes,
                    certificate.compiled
                        .publication_peak_owned_bytes);
            telemetry.memory_bytes = std::max(
                refinement_phase_memory, compiled_phase_memory);
            telemetry.peak_memory_bytes = std::max(
                refinement_phase_peak,
                std::max(
                    compiled_phase_peak,
                    reported_compiled_phase_peak));
            peak_owned_bytes = std::max(
                peak_owned_bytes, telemetry.peak_memory_bytes);

            bool lift_complete =
                certificate.status ==
                    refinement::PolicyExactLiftStatus::Complete &&
                certificate.executable &&
                certificate.lumpable &&
                certificate.compiled.executable;
            if (lift_complete) {
                RetainedCompiledPolicyArtifact artifact;
                artifact.strategy_json =
                    std::move(certificate.compiled.strategy_json);
                artifact.certification_strategy_json =
                    std::move(
                        certificate.compiled
                            .certification_strategy_json);
                artifact.working_states =
                    certificate.compiled.compilation.working_states;
                artifact.behavioral_classes =
                    certificate.compiled.compilation.behavioral_classes;
                artifact.policy_regions =
                    certificate.compiled.compilation.policy_regions;
                artifact.infrastructure_nodes =
                    certificate.compiled.compilation.infrastructure_nodes;
                artifact.policy_route_nodes =
                    certificate.compiled.compilation.policy_route_nodes;
                artifact.local_gated_route_nodes =
                    certificate.compiled.compilation
                        .local_gated_route_nodes;
                artifact.primitive_region_nodes =
                    certificate.compiled.compilation
                        .primitive_region_nodes;
                artifact.additional_recipe_nodes =
                    certificate.compiled.compilation
                        .additional_recipe_nodes;
                artifact.nodes =
                    certificate.compiled.compilation.nodes;
                artifact.edges =
                    certificate.compiled.compilation.edges;
                artifact.total_condition_bytes =
                    certificate.compiled.compilation.total_condition_bytes;
                artifact.max_condition_bytes =
                    certificate.compiled.compilation.max_condition_bytes;
                artifact.condition_edges =
                    certificate.compiled.compilation.condition_edges;
                artifact.unique_condition_literals =
                    certificate.compiled.compilation
                        .unique_condition_literals;
                artifact.repeated_condition_occurrences =
                    certificate.compiled.compilation
                        .repeated_condition_occurrences;
                artifact.repeated_condition_bytes =
                    certificate.compiled.compilation
                        .repeated_condition_bytes;
                artifact.policy_route_nondefault_edges =
                    certificate.compiled.compilation
                        .policy_route_nondefault_edges;
                artifact.policy_route_distinct_targets =
                    certificate.compiled.compilation
                        .policy_route_distinct_targets;
                artifact.same_target_branch_groups =
                    certificate.compiled.compilation
                        .same_target_branch_groups;
                artifact.same_target_branch_edges =
                    certificate.compiled.compilation
                        .same_target_branch_edges;
                artifact.projected_same_target_edge_savings =
                    certificate.compiled.compilation
                        .projected_same_target_edge_savings;
                artifact.max_policy_route_out_degree =
                    certificate.compiled.compilation
                        .max_policy_route_out_degree;
                artifact.max_policy_route_distinct_targets =
                    certificate.compiled.compilation
                        .max_policy_route_distinct_targets;
                artifact.exact_state_fallbacks =
                    certificate.compiled.compilation.exact_state_fallbacks;
                artifact.junk_predicates =
                    certificate.compiled.compilation.junk_predicates;
                artifact.policy_route_default_edges =
                    certificate.compiled.compilation
                        .policy_route_default_edges;
                artifact.policy_route_restart_default_edges =
                    certificate.compiled.compilation
                        .policy_route_restart_default_edges;
                artifact.policy_route_offpolicy_default_edges =
                    certificate.compiled.compilation
                        .policy_route_offpolicy_default_edges;
                artifact.policy_route_default_mode =
                    certificate.compiled.compilation
                        .policy_route_default_mode;
                artifact.certification_policy_route_default_edges =
                    certificate.compiled.certification_compilation
                        .policy_route_default_edges;
                artifact.certification_policy_route_offpolicy_default_edges =
                    certificate.compiled.certification_compilation
                        .policy_route_offpolicy_default_edges;
                artifact.certification_policy_route_default_mode =
                    certificate.compiled.certification_compilation
                        .policy_route_default_mode;
                artifact.paired_default_only =
                    certificate.compiled.paired_default_only;
                artifact.peak_owned_bytes =
                    certificate.compiled.compilation.peak_owned_bytes;
                artifact.previously_accounted_peak_owned_bytes =
                    certificate.compiled.compilation
                        .previously_accounted_peak_owned_bytes;
                artifact.complete_peak_owned_bytes =
                    certificate.compiled.compilation
                        .complete_peak_owned_bytes;
                result.refined_policy_artifact =
                    std::move(artifact);
                telemetry.retained_artifact_bytes =
                    result.refined_policy_artifact
                            .strategy_json.capacity() +
                    result.refined_policy_artifact
                            .certification_strategy_json.capacity() +
                    2;
                if (estimated_owned_bytes() >
                    options.max_solver_owned_bytes) {
                    result.refined_policy_artifact = {};
                    telemetry.retained_artifact_bytes = 0;
                    certificate.status =
                        refinement::PolicyExactLiftStatus::ResourceCap;
                    certificate.resource_cap =
                        "max_solver_owned_bytes";
                    certificate.failure_reason =
                        "retaining the exact refined strategy reached "
                        "max_solver_owned_bytes";
                    lift_complete = false;
                }
                if (lift_complete) {
                    /* The independently evaluated emitted graph owns the
                     * executable cost. The strict class-policy value remains
                     * internal reconciliation evidence only. */
                    if (!std::isfinite(
                            certificate.compiled.exact_cost) ||
                        certificate.compiled.exact_cost < 0.0) {
                        throw std::logic_error(
                            "completed exact policy refinement returned "
                            "an invalid final graph cost");
                    }
                    const double exact_policy_cost =
                        certificate.compiled.exact_cost;
                    BoundedPolicyIncumbent strict_candidate_record;
                    strict_candidate_record.certified_upper_bound =
                        exact_policy_cost;
                    strict_candidate_record.evaluated_policy_cost =
                        exact_policy_cost;
                    strict_candidate_record.kind =
                        "policy_guided_exact_refinement";
                    strict_candidate_record.independently_certified = true;
                    strict_candidate_record.independently_evaluated = true;
                    strict_candidate_record.proper = true;
                    strict_candidate_record.executable = true;
                    strict_candidate_record
                        .reconciliation_absolute_delta =
                        certificate.compiled.absolute_cost_delta;
                    strict_candidate_record
                        .reconciliation_relative_delta =
                        certificate.compiled.relative_cost_delta;
                    strict_candidate_record.portfolio_identity =
                        telemetry.core_policy_bits_hash;
                    identity_mix(
                        strict_candidate_record.portfolio_identity,
                        std::bit_cast<std::uint64_t>(
                            exact_policy_cost));
                    identity_mix_string(
                        strict_candidate_record.portfolio_identity,
                        strict_candidate_record.kind);
                    record_candidate_sample(
                        strict_candidate_record,
                        "strict_final_graph_evaluation",
                        "eligible_verified_candidate",
                        certificate.compiled.cost_reconciled
                            ? "final graph cost reconciled"
                            : "executable cost mismatch blocks exactness");
                    const bool globally_exact =
                        certificate.global_lower_bound_closed &&
                        certificate.compiled.cost_reconciled &&
                        std::isfinite(exact_policy_cost);
                    verify_retained_portfolio();
                    BoundedPolicyIncumbent* cheaper_verified =
                        best_current_certified_fallback();
                    const bool strict_superseded =
                        cheaper_verified != nullptr &&
                        cheaper_verified->evaluated_policy_cost <
                            exact_policy_cost &&
                        publish_certified_fallback(
                            coarse_solve_termination);
                    if (strict_superseded) {
                        record_candidate_sample(
                            strict_candidate_record, "publication",
                            "not_selected",
                            "a cheaper independently evaluated retained "
                            "candidate won publication");
                        telemetry.status =
                            "strict_lift_complete_cheaper_verified_"
                            "candidate_selected";
                    } else {
                    record_candidate_sample(
                        strict_candidate_record, "publication",
                        "selected_for_publication",
                        "no retained independently evaluated candidate was "
                        "strictly cheaper; exact cost ties retain the "
                        "stronger strict proof authority");
                    const double retained_lower =
                        std::isfinite(result.lower_bound) &&
                                result.lower_bound <=
                                    exact_policy_cost +
                                        value_comparison_tolerance(
                                            exact_policy_cost)
                            ? std::max(0.0, result.lower_bound)
                            : 0.0;
                    result.converged = globally_exact;
                    result.policy_status = globally_exact
                        ? SolvePolicyStatus::Exact
                        : SolvePolicyStatus::BoundedFeasible;
                    result.target_met = false;
                    result.target_fired = SolveGapTarget::None;
                    result.lower_bound = globally_exact
                        ? exact_policy_cost
                        : retained_lower;
                    result.upper_bound = exact_policy_cost;
                    result.evaluated_policy_cost =
                        exact_policy_cost;
                    result.absolute_optimality_gap = std::max(
                        0.0, exact_policy_cost - result.lower_bound);
                    result.relative_optimality_gap =
                        result.lower_bound > 0.0
                            ? std::max(
                                  0.0,
                                  exact_policy_cost /
                                          result.lower_bound -
                                      1.0)
                            : kInfinity;
                    if (result.start_state <
                        result.values.size()) {
                        result.values[result.start_state] =
                            exact_policy_cost;
                    }
                    result.diagnostics.focused_lower_bound =
                        result.lower_bound;
                    result.diagnostics.focused_upper_bound =
                        exact_policy_cost;
                    result.diagnostics
                        .focused_partial_policy_upper_bound =
                        exact_policy_cost;
                    result.diagnostics.focused_optimality_gap =
                        result.absolute_optimality_gap;
                    result.diagnostics.solution_scope = globally_exact
                        ? "policy_guided_exact_refinement_global"
                        : "policy_guided_exact_refinement_bounded";
                    result.diagnostics.incumbent_kind =
                        "policy_guided_exact_refinement";
                    telemetry.publication_status = globally_exact
                        ? "exact_strict_policy"
                        : "bounded_strict_policy";
                    telemetry.published_candidate_kind =
                        "policy_guided_exact_refinement";
                    /* A complete strict Bellman envelope is a fresh global
                     * optimality proof. Once it closes, an earlier coarse
                     * resource stop no longer describes the published
                     * result; only bounded strict publication inherits that
                     * stop cause. */
                    result.termination =
                        successful_refined_publication_termination(
                            coarse_solve_termination,
                            result.diagnostics.resource_cap_hit,
                            globally_exact,
                            coarse_discovery_closed);
                    if (!globally_exact) {
                        classify_bounded_publication();
                    }
                    for (const BoundedPolicyIncumbent& candidate :
                         certified_fallback_portfolio) {
                        record_candidate_sample(
                            candidate, "publication", "not_selected",
                            candidate.independently_evaluated
                                ? "strict final graph was cheaper or exactly "
                                  "tied with stronger proof authority"
                                : "final graph evaluation did not establish "
                                  "an executable cost");
                    }
                    certified_fallback_portfolio.clear();
                    certified_fallback_portfolio.shrink_to_fit();
                    telemetry.fallback_portfolio_candidates = 0;
                    telemetry.fallback_portfolio_owned_bytes = 0;
                    }
                }
            }
            if (!lift_complete) {
                const std::string status =
                    refinement::policy_exact_lift_status_name(
                        certificate.status);
                telemetry.status = status;
                const std::string reason =
                    "policy_exact_refinement_" + status;
                if (!certificate.compiled.strategy_json.empty()) {
                    retain_compiled_unverified(
                        certificate.compiled,
                        "policy_guided_exact_refinement",
                        "strict_final_graph_evaluation",
                        certificate.failure_reason.empty()
                            ? reason
                            : reason + ": " +
                                  certificate.failure_reason);
                }
                record_refinement_refusal(reason);
                revoke_publication(
                    certificate.failure_reason.empty()
                        ? reason
                        : reason + ": " +
                              certificate.failure_reason,
                    certificate.resource_cap);
                telemetry.preferred_publication_failure_reason =
                    result.diagnostics
                        .policy_publication_failure_reason;
                telemetry.publication_status = "none";
                (void)publish_certified_fallback(
                    coarse_solve_termination);
            }
        }
        const bool unclosed_strict_refinement =
            result.diagnostics.policy_refinement.strict_lift_status !=
                "not_run" &&
            !result.diagnostics.policy_refinement
                 .strict_global_lower_bound_closed &&
            result.policy_status != SolvePolicyStatus::Exact;
        if (unclosed_strict_refinement) {
            /* Once exact mechanics expose a compatibility witness, the
             * coarse objective is not a lower relaxation of the strict
             * decision problem. A failed or bounded strict lift can retain
             * its independently evaluated upper and the separately proved
             * action-envelope-independent goal-cover floor, but not the
             * stronger coarse/restricted policy value. */
            result.lower_bound =
                std::isfinite(
                    result.diagnostics
                        .independent_goal_cover_lower_bound) &&
                        result.diagnostics
                                .independent_goal_cover_lower_bound >=
                            0.0
                    ? result.diagnostics
                          .independent_goal_cover_lower_bound
                    : 0.0;
        }
        solve_detail::normalize_publication_result(result);
        if (result.policy_available) {
            result.diagnostics.focused_lower_bound = result.lower_bound;
            result.diagnostics.focused_upper_bound = result.upper_bound;
            result.diagnostics.focused_partial_policy_upper_bound =
                result.upper_bound;
            result.diagnostics.focused_optimality_gap =
                result.absolute_optimality_gap;
        }
        /*
         * The first pass retains and accounts diagnostic payloads before the
         * final byte-cap checks. Refresh policy dispositions now that the
         * executable-policy gate has decided whether this policy is actually
         * published.
         */
        finalize_automatic_candidate_diagnostics();
        {
            std::uint64_t hash = 1469598103934665603ULL;
            const auto mix = [&hash](const std::uint64_t value) {
                hash ^= value;
                hash *= 1099511628211ULL;
            };
            for (std::size_t i = 0;
                 i < transition_cache->successors.size(); ++i) {
                mix(transition_cache->successors[i]);
                mix(std::bit_cast<std::uint64_t>(
                    transition_cache->probabilities[i]));
            }
            for (const SparseRow& row : transition_cache->rows) {
                mix(row.owner_state);
                mix(row.transition_offset);
                mix(row.transition_count);
                mix(std::bit_cast<std::uint64_t>(row.self_probability));
                mix(std::bit_cast<std::uint64_t>(
                    row.embedded_self_probability));
            }
            result.diagnostics.transition_bits_hash = hash;
            hash = 1469598103934665603ULL;
            for (std::size_t state = 0; state < result.policy.size(); ++state) {
                mix(state);
                mix(result.policy[state].index);
                mix(result.policy[state].kind == PlannerOperatorKind::Primitive
                        ? 0u
                        : 1u);
                mix(state < policy_rows.size() ? policy_rows[state]
                                               : std::uint64_t{0});
                if (state < policy_rows.size() &&
                    policy_rows[state] !=
                        std::numeric_limits<std::uint64_t>::max() &&
                    state < restored_policy_row_costs.size()) {
                    mix(std::bit_cast<std::uint64_t>(
                        restored_policy_row_costs[state]));
                } else if (state < policy_rows.size() &&
                           policy_rows[state] < priced_rows.size()) {
                    mix(std::bit_cast<std::uint64_t>(
                        priced_rows[policy_rows[state]].cost));
                }
            }
            result.diagnostics.policy_bits_hash = hash;
        }
        result.diagnostics.extraction_ns = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - extraction_started)
                .count());
        result.diagnostics.solve_owned_byte_ledger_requests =
            owned_byte_ledger_requests;
        result.diagnostics.solve_owned_byte_reconciliations =
            owned_byte_reconciliations;
        result.diagnostics.solve_owned_byte_ledger_max_overestimate =
            owned_byte_ledger_max_overestimate;
        std::uint64_t final_live_bytes = estimated_owned_bytes();
        peak_owned_bytes = std::max(peak_owned_bytes, final_live_bytes);
        bool publication_revoked_at_final_cap = false;
        if (final_live_bytes > options.max_solver_owned_bytes) {
            if (result.policy_available) {
                result.diagnostics.policy_refinement.status =
                    "retained_memory_resource_cap";
                record_refinement_refusal(
                    "policy_publication_retained_memory_resource_cap");
                revoke_publication(
                    "retained solve result reached "
                    "max_solver_owned_bytes",
                    "max_solver_owned_bytes");
                publication_revoked_at_final_cap = true;
            } else {
                record_cap("max_solver_owned_bytes");
                result.converged = false;
            }
        }
        if (publication_revoked_at_final_cap) {
            finalize_automatic_candidate_diagnostics();
            final_live_bytes = estimated_owned_bytes();
            peak_owned_bytes =
                std::max(peak_owned_bytes, final_live_bytes);
            if (final_live_bytes > options.max_solver_owned_bytes) {
                record_cap("max_solver_owned_bytes");
                result.converged = false;
            }
        }
        result.diagnostics.solver_owned_bytes_estimate =
            std::max(peak_owned_bytes, estimated_owned_bytes());
        result.diagnostics.solver_live_owned_bytes_estimate =
            estimated_retained_solver_bytes(calc, &result);
        result.diagnostics.diagnostics_retained_bytes_estimate =
            diagnostics_owned_bytes(result.diagnostics);
        result.diagnostics.upper_policy_provenance_samples =
            std::move(upper_policy_provenance_samples);
        result.diagnostics.upper_policy_provenance_samples_omitted =
            upper_policy_provenance_samples_omitted;
        result.diagnostics.upper_policy_provenance_candidate_count =
            upper_policy_provenance_candidate_count;
        result.diagnostics.upper_policy_provenance_retained_bytes =
            upper_policy_provenance_retained_bytes;
        result.diagnostics.upper_cap_zero_progress_audit_json =
            std::move(upper_cap_zero_progress_audit_json);
        const solve_detail::SolveLowerBoundAuthority lower_authority =
            solve_detail::classify_public_lower_bound_authority(
                result.lower_bound,
                result.policy_status,
                incremental_action_generation,
                incremental_envelope_closed,
                unclosed_strict_refinement,
                result.diagnostics.independent_goal_cover_lower_bound);
        result.global_lower_bound_certified =
            lower_authority.globally_certified;
        result.lower_bound_provenance = lower_authority.provenance;
        if (const char* invariant =
                solve_detail::publication_invariant_invalid_reason(result)) {
            throw std::logic_error(invariant);
        }
        consumed = true;
        co_return std::move(result);
    }

void SolveWork::Impl::begin_finalization() {
        if (finalization_task.has_value() ||
            finalized_result.has_value()) {
            return;
        }
        phase = SolvePhase::Refining;
        finalization_task.emplace(run_finalization());
    }

void SolveWork::Impl::advance_finalization() {
        if (!finalization_task.has_value()) {
            throw std::logic_error(
                "solver finalization continuation is missing");
        }
        ++finalization_work_items;
        if (!finalization_task->resume()) return;
        finalized_result.emplace(finalization_task->take_result());
        finalization_task.reset();
        phase = SolvePhase::Done;
    }

SolveResult SolveWork::Impl::finish() {
        if (phase != SolvePhase::Done ||
            !finalized_result.has_value()) {
            throw std::logic_error("solver work is not finished");
        }
        SolveResult finished = std::move(*finalized_result);
        finalized_result.reset();
        return finished;
    }

}
}
