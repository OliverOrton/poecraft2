#include "solver_solve_types.hpp"

#include "solver_quotient_proof.hpp"

namespace poecraft {
namespace solver {

using namespace solve_detail;

namespace solve_detail {

constexpr std::uint64_t kUpperPolicyProvenanceStructuralBytes =
    sizeof(std::vector<std::string>) +
    3 * sizeof(std::uint64_t) +
    sizeof(std::string);
/*
 * Solve-owned accounting observes the retained result through two structural
 * shells. The finalization-only provenance fields belong to neither resource
 * boundary, so remove both static footprints as well as excluding their
 * dynamic strings below.
 */
constexpr std::uint64_t kUpperPolicyProvenanceAccountingOffset =
    2 * kUpperPolicyProvenanceStructuralBytes;

std::uint64_t SparseVariantArena::selected_bytes() const {
        return sizeof(*this) +
               variants.capacity() * sizeof(SparseVariant) +
               row_variant_indices.capacity() * sizeof(std::uint32_t) +
               variant_quantities.capacity() * sizeof(double);
    }

const char* automatic_telemetry_kind_name(
    const AutomaticTelemetryKind kind) {
    switch (kind) {
    case AutomaticTelemetryKind::Imprint: return "imprint";
    case AutomaticTelemetryKind::Renewal: return "renewal";
    case AutomaticTelemetryKind::ProtectedSide: return "protected_side";
    case AutomaticTelemetryKind::TemporaryBench: return "temporary_bench";
    case AutomaticTelemetryKind::FracturePrepare:
        return "fracture_prepare";
    case AutomaticTelemetryKind::PermanentBench: return "permanent_bench";
    case AutomaticTelemetryKind::MultimodFinish: return "multimod_finish";
    case AutomaticTelemetryKind::PrimitiveFracture:
        return "primitive_fracture";
    case AutomaticTelemetryKind::EldritchSide:
        return "eldritch_side";
    case AutomaticTelemetryKind::CannotRoll:
        return "cannot_roll";
    case AutomaticTelemetryKind::Veiled:
        return "veiled";
    case AutomaticTelemetryKind::Count:
    case AutomaticTelemetryKind::None:
        return "none";
    }
    return "none";
}

const char* primitive_telemetry_family_name(
    const PrimitiveTelemetryFamily family) {
    switch (family) {
    case PrimitiveTelemetryFamily::Currency: return "currency";
    case PrimitiveTelemetryFamily::Essence: return "essence";
    case PrimitiveTelemetryFamily::Fossil: return "fossil";
    case PrimitiveTelemetryFamily::Harvest: return "harvest";
    case PrimitiveTelemetryFamily::Bench: return "bench";
    case PrimitiveTelemetryFamily::Bestiary: return "bestiary";
    case PrimitiveTelemetryFamily::Fracture: return "fracture";
    case PrimitiveTelemetryFamily::Other: return "other";
    case PrimitiveTelemetryFamily::EldritchSetup:
        return "eldritch_setup";
    case PrimitiveTelemetryFamily::EldritchChaos:
        return "eldritch_chaos";
    case PrimitiveTelemetryFamily::EldritchAnnul:
        return "eldritch_annul";
    case PrimitiveTelemetryFamily::EldritchExalt:
        return "eldritch_exalt";
    case PrimitiveTelemetryFamily::InfluenceExalt:
        return "influence_exalt";
    case PrimitiveTelemetryFamily::VeiledChaos: return "veiled_chaos";
    case PrimitiveTelemetryFamily::VeiledExalt: return "veiled_exalt";
    case PrimitiveTelemetryFamily::Unveil: return "unveil";
    case PrimitiveTelemetryFamily::Count: return "none";
    }
    return "none";
}

std::uint64_t string_vector_owned_bytes(
    const std::vector<std::string>& values) {
    std::uint64_t total = values.capacity() * sizeof(std::string);
    for (const std::string& value : values) total += value.capacity() + 1;
    return total;
}

std::uint64_t broad_row_attribution_owned_bytes(
        const std::vector<PolicyBroadRowAttribution>& samples) {
    std::uint64_t total =
        samples.capacity() * sizeof(PolicyBroadRowAttribution);
    for (const PolicyBroadRowAttribution& sample : samples) {
        total += sample.planner_id.capacity() + 1;
        total += string_vector_owned_bytes(
            sample.primitive_program_action_ids);
        total += sample.observation_modifier_tag_ids.capacity() *
                 sizeof(std::uint32_t);
        total += sample.reforge.action_id.capacity() + 1;
        total += sample.reforge.terminal_contribution_samples.capacity() *
                 sizeof(ReforgeBuildAttribution::
                            TerminalContributionSample);
    }
    return total;
}

std::uint64_t diagnostics_owned_bytes(const SolveDiagnostics& diagnostics) {
    std::uint64_t bytes =
           string_vector_owned_bytes(diagnostics.skipped_missing_price) +
           string_vector_owned_bytes(diagnostics.skipped_unsupported) +
           string_vector_owned_bytes(diagnostics.cap_hits) +
           string_vector_owned_bytes(
               diagnostics.action_inclusion_reasons) +
           string_vector_owned_bytes(diagnostics.preservation_witnesses) +
           string_vector_owned_bytes(
               diagnostics.constructive_state_witnesses) +
           string_vector_owned_bytes(
               diagnostics.automatic_candidate_witnesses) +
           string_vector_owned_bytes(
               diagnostics.product_fracture_witnesses) +
           string_vector_owned_bytes(
               diagnostics.incremental_action_witnesses) +
           string_vector_owned_bytes(
               diagnostics.policy_refinement.counterexample_samples) +
           string_vector_owned_bytes(
               diagnostics.policy_refinement.refusal_cause_samples) +
           string_vector_owned_bytes(
               diagnostics.policy_refinement
                   .publication_candidate_samples) +
           string_vector_owned_bytes(
               diagnostics.policy_refinement
                   .structural_failure_samples) +
           string_vector_owned_bytes(
               diagnostics.policy_refinement
                   .evaluator_memory_samples) +
           diagnostics.policy_refinement.trigger_coarse_states.capacity() *
               sizeof(std::uint32_t) +
           diagnostics.policy_refinement.status.capacity() + 1 +
           diagnostics.policy_refinement.resource_cap.capacity() + 1 +
           diagnostics.policy_refinement.core_policy_status.capacity() + 1 +
           diagnostics.policy_refinement.core_policy_root_action.capacity() +
               1 +
           diagnostics.policy_refinement
                   .direct_certification_status.capacity() +
               1 +
           diagnostics.policy_refinement
                   .direct_certification_failure_reason.capacity() +
               1 +
           diagnostics.policy_refinement
                   .direct_certification_failure_classification.capacity() +
               1 +
           diagnostics.policy_refinement
                   .direct_certification_resource_cap.capacity() +
               1 +
           diagnostics.policy_refinement
                   .direct_certification_route_default_mode.capacity() +
               1 +
           diagnostics.policy_refinement
                   .direct_product_route_default_mode.capacity() +
               1 +
           diagnostics.policy_refinement.strict_lift_status.capacity() + 1 +
           diagnostics.policy_refinement
                   .strict_lift_failure_reason.capacity() +
               1 +
           diagnostics.policy_refinement.strict_lift_resource_cap.capacity() +
               1 +
           diagnostics.policy_refinement.publication_status.capacity() + 1 +
           diagnostics.policy_refinement.published_candidate_kind.capacity() +
               1 +
           diagnostics.policy_refinement
                   .published_fallback_kind.capacity() +
               1 +
           diagnostics.policy_refinement
                   .preferred_publication_failure_reason.capacity() +
               1 +
           diagnostics.policy_refinement
                   .selected_candidate_status.capacity() +
               1 +
           diagnostics.policy_refinement
                   .selected_candidate_failure_reason.capacity() +
               1 +
           broad_row_attribution_owned_bytes(
               diagnostics.policy_refinement.broad_row_attribution) +
           diagnostics.policy_refinement
                   .strict_reforge_row_samples.capacity() *
               sizeof(ReforgeRowTelemetry) +
           string_vector_owned_bytes(diagnostics.equivalence_witnesses) +
           diagnostics.focused_schedule_rounds.capacity() *
               sizeof(FocusedScheduleRoundTelemetry) +
           diagnostics.solution_scope.capacity() + 1 +
           diagnostics.policy_evaluation_failure.capacity() + 1 +
           diagnostics.policy_compatibility_action.capacity() + 1 +
           diagnostics.policy_compatibility_reason.capacity() + 1 +
           diagnostics.policy_publication_failure_reason.capacity() + 1 +
           diagnostics.incumbent_kind.capacity() + 1 +
           diagnostics.destructive_renewal_action_id.capacity() + 1 +
           diagnostics.progressive_fracture_roll_action_id.capacity() + 1 +
           diagnostics.progressive_fracture_status.capacity() + 1;
    bytes += diagnostics.action_search_costs_owned_bytes;
    for (const auto& [id, unused] :
         diagnostics.lower_policy_action_states) {
        (void)unused;
        bytes += sizeof(std::pair<const std::string, std::uint64_t>) +
                 id.capacity() + 1;
    }
    for (const auto& [id, unused] :
         diagnostics.upper_policy_action_states) {
        (void)unused;
        bytes += sizeof(std::pair<const std::string, std::uint64_t>) +
                 id.capacity() + 1;
    }
    return bytes;
}

std::uint64_t solve_result_owned_bytes(const SolveResult& result) {
    std::uint64_t bytes =
        sizeof(result) -
        kUpperPolicyProvenanceAccountingOffset;
    bytes += result.values.capacity() * sizeof(double);
    bytes += result.policy.capacity() * sizeof(PolicyOperatorRef);
    bytes += result.expanded.capacity() * sizeof(std::uint8_t);
    bytes += result.goal_states.capacity() * sizeof(std::uint8_t);
    bytes += result.policy_reachable.capacity() * sizeof(std::uint8_t);
    bytes += result.unveil_preferences.capacity() *
             sizeof(std::vector<std::uint32_t>);
    for (const auto& preferences : result.unveil_preferences) {
        bytes += preferences.capacity() * sizeof(std::uint32_t);
    }
    bytes += result.option_unveil_preferences.capacity() *
             sizeof(std::vector<ObservedUnveilPreference>);
    for (const auto& preferences : result.option_unveil_preferences) {
        bytes += preferences.capacity() * sizeof(ObservedUnveilPreference);
        for (const ObservedUnveilPreference& preference : preferences) {
            bytes += preference.choices.capacity() *
                     sizeof(ObservedUnveilChoice);
        }
    }
    bytes += result.behavioral_representative_by_state.capacity() *
             sizeof(std::uint32_t);
    bytes += result.primitive_renewal_witness.kernel_signature.capacity() *
             sizeof(std::uint64_t);
    bytes += result.refined_policy_artifact.strategy_json.capacity() + 1;
    bytes += result.refined_policy_artifact
                 .certification_strategy_json.capacity() + 1;
    bytes += result.refined_policy_artifact
                 .policy_route_default_mode.capacity() + 1;
    bytes += result.refined_policy_artifact
                 .certification_policy_route_default_mode.capacity() + 1;
    bytes += diagnostics_owned_bytes(result.diagnostics);
    return bytes;
}

}

 std::uint64_t SolveTransitionCache::automatic_sample_nested_bytes(
        const AutomaticCandidateRecord& record) {
        return record.evidence.legality_result.capacity() +
               record.evidence.reason.capacity() +
               record.candidate_id.capacity() +
               record.setup_action_id.capacity() +
               record.followup_action_id.capacity() +
               record.cleanup_action_id.capacity();
    }

std::uint64_t SolveTransitionCache::shallow_estimated_owned_bytes() const {
        std::uint64_t bytes = sizeof(*this);
        bytes += operator_indices.capacity() * sizeof(std::uint32_t);
        bytes += expanded.capacity() * sizeof(std::uint8_t);
        bytes += behavioral_representative_by_state.capacity() *
                 sizeof(std::uint32_t);
        bytes += state_rows.capacity() * sizeof(StateRowSpan);
        bytes += rows.capacity() * sizeof(SparseRow);
        if (quotient_proofs != nullptr) {
            bytes += quotient_proofs->ledger().snapshot().total_bytes;
        }
        if (accounts_variant_arena && variant_arena != nullptr) {
            bytes += variant_arena->selected_bytes();
        }
        bytes += successors.capacity() * sizeof(std::uint32_t);
        bytes += probabilities.capacity() * sizeof(double);
        bytes += choices.capacity() * sizeof(SparseChoiceGroup);
        bytes += choice_successors.capacity() * sizeof(std::uint32_t);
        bytes += choice_options.capacity() * sizeof(OutcomeChoiceOption);
        bytes += automatic_candidate_samples.capacity() *
                 sizeof(AutomaticCandidateRecord);
        bytes += product_fracture_rows.capacity() *
                 sizeof(ProductFractureRowWitness);
        return bytes;
    }

std::uint64_t SolveTransitionCache::fast_estimated_owned_bytes() const {
        return shallow_estimated_owned_bytes() +
               owned_automatic_sample_nested_bytes;
    }

std::uint64_t SolveTransitionCache::audited_estimated_owned_bytes() const {
        std::uint64_t bytes = shallow_estimated_owned_bytes();
        for (const AutomaticCandidateRecord& record :
             automatic_candidate_samples) {
            bytes += automatic_sample_nested_bytes(record);
        }
        return bytes;
    }

std::uint64_t SolveTransitionCache::estimated_owned_bytes() const {
        return audited_estimated_owned_bytes();
    }

SolveProgress SolveWork::Impl::progress() const {
        SolveProgress value;
        value.phase = phase;
        value.done = phase == SolvePhase::Done;
        value.expanded_states = expanded_count;
        value.sweeps = sweeps;
        value.residual = residual;
        value.finalization_work_items = finalization_work_items;
        value.refinement_states = finalization_refinement_states;
        value.refinement_kernels = finalization_refinement_kernels;
        value.refinement_transitions =
            finalization_refinement_transitions;
        value.refinement_rounds = finalization_refinement_rounds;
        value.refinement_classes = finalization_refinement_classes;
        value.certification_discovered_pairs =
            finalization_evaluation_progress.discovered_pairs;
        value.certification_pending_pairs =
            finalization_evaluation_progress.pending_pairs;
        value.certification_solved_sccs =
            finalization_evaluation_progress.solved_sccs;
        value.certification_total_sccs =
            finalization_evaluation_progress.total_sccs;
        if (finalized_result.has_value()) {
            const SolveResult& finalized = *finalized_result;
            value.start_value_bound =
                finalized.start_state < finalized.values.size()
                    ? finalized.values[finalized.start_state]
                    : finalized.upper_bound;
            value.lower_bound = finalized.lower_bound;
            value.upper_bound = finalized.upper_bound;
            value.absolute_optimality_gap =
                finalized.absolute_optimality_gap;
            value.relative_optimality_gap =
                finalized.relative_optimality_gap;
            value.focused_round =
                finalized.diagnostics.focused_expansion_rounds;
            value.incumbent_kind =
                finalized.diagnostics.incumbent_kind;
            value.discovered_states =
                finalized.diagnostics.discovered_states;
            value.frontier_states =
                finalized.diagnostics.frontier_states;
            value.state_action_rows =
                finalized.diagnostics.sparse_rows;
            value.transition_entries =
                finalized.diagnostics.sparse_transitions;
            value.reforge_work =
                finalized.diagnostics.reforge_logical_work_v1;
            value.live_owned_bytes = fast_estimated_owned_bytes();
            value.peak_owned_bytes = std::max(
                peak_owned_bytes, value.live_owned_bytes);
            return value;
        }
        value.start_value_bound = kValueCeiling;
        if (!result.values.empty() &&
            result.start_state < result.values.size()) {
            value.start_value_bound = result.values[result.start_state];
        }
        if (result.diagnostics.focused_expansion) {
            value.lower_bound = certified_global_lower_bound();
            value.upper_bound = output_incumbent.has_value()
                                    ? output_incumbent->certified_upper_bound
                                    : result.diagnostics.focused_upper_bound;
        } else {
            value.lower_bound = 0.0;
            value.upper_bound = value.start_value_bound;
            bool full_non_goal_closure = value.done;
            if (full_non_goal_closure) {
                for (std::uint32_t state = 0;
                     state < result.values.size(); ++state) {
                    if (!result.behavioral_representative_by_state.empty() &&
                        result.behavioral_representative_by_state[state] !=
                            state) {
                        continue;
                    }
                    if (!result.expanded[state] &&
                        !calc.is_goal_state(calc.state(state))) {
                        full_non_goal_closure = false;
                        break;
                    }
                }
            }
            const bool exact_closed =
                full_non_goal_closure && !target_gap_stop &&
                !result.diagnostics.state_cap_hit &&
                !result.diagnostics.resource_cap_hit &&
                (!incremental_action_generation ||
                 incremental_envelope_closed) &&
                optimization_converged() &&
                value.start_value_bound < kValueCeiling;
            if (exact_closed) {
                value.lower_bound = value.start_value_bound;
            }
        }
        if (std::isfinite(value.lower_bound) &&
            std::isfinite(value.upper_bound)) {
            value.absolute_optimality_gap = std::max(
                0.0, value.upper_bound - value.lower_bound);
            if (value.lower_bound > 0.0) {
                value.relative_optimality_gap = std::max(
                    0.0,
                    value.upper_bound / value.lower_bound - 1.0);
            }
        }
        value.focused_round = result.diagnostics.focused_expansion_rounds;
        value.incumbent_kind = result.diagnostics.incumbent_kind;
        value.discovered_states = calc.state_count();
        value.frontier_states = value.discovered_states >= expanded_count
                                    ? value.discovered_states - expanded_count
                                    : 0;
        value.state_action_rows = transition_cache == nullptr
                                      ? 0
                                      : transition_cache->rows.size();
        value.transition_entries = transition_cache == nullptr
                                       ? 0
                                       : transition_cache->successors.size() +
                                             transition_cache
                                                 ->choice_successors.size();
        value.reforge_work = value.done
                                 ? result.diagnostics
                                       .reforge_logical_work_v1
                                 : calc.telemetry()
                                       .reforge_logical_work_v1;
        /* Progress is queried after every bounded C/WASM step. Before
         * finalization only the coarse child has run; the completed snapshot
         * uses the aggregate coarse/strict/evaluation logical audit. */
        value.live_owned_bytes = fast_estimated_owned_bytes();
        value.peak_owned_bytes = std::max(
            peak_owned_bytes, value.live_owned_bytes);
        return value;
    }

SolveTelemetrySnapshot SolveWork::Impl::telemetry_snapshot(bool abandoned) const {
        SolveTelemetrySnapshot snapshot;
        snapshot.phase = phase;
        snapshot.abandoned = abandoned;
        snapshot.diagnostics = result.diagnostics;
        snapshot.diagnostics.expanded_states = expanded_count;
        snapshot.diagnostics.discovered_states = calc.state_count();
        snapshot.diagnostics.frontier_states =
            snapshot.diagnostics.discovered_states - expanded_count;
        snapshot.diagnostics.goal_states = 0;
        for (std::uint32_t state = 0; state < calc.state_count(); ++state) {
            if (!result.behavioral_representative_by_state.empty() &&
                result.behavioral_representative_by_state[state] != state) {
                continue;
            }
            if (calc.is_goal_state(calc.state(state))) {
                ++snapshot.diagnostics.goal_states;
            }
        }
        snapshot.diagnostics.sweeps = sweeps;
        snapshot.diagnostics.residual = residual;
        snapshot.diagnostics.automatic_rows_considered =
            transition_cache->automatic_rows_considered;
        snapshot.diagnostics.automatic_rows_eligible =
            transition_cache->automatic_rows_eligible;
        snapshot.diagnostics.automatic_rows_rejected =
            transition_cache->automatic_rows_rejected;
        snapshot.diagnostics.automatic_rows_collapsed =
            transition_cache->automatic_rows_collapsed;
        snapshot.diagnostics.automatic_rows_deferred =
            transition_cache->automatic_rows_deferred;
        snapshot.diagnostics.automatic_kind_telemetry =
            transition_cache->automatic_kind_telemetry;
        snapshot.diagnostics.automatic_admission_phases =
            transition_cache->automatic_admission_phases;
        snapshot.diagnostics.automatic_candidate_witnesses_omitted =
            snapshot.diagnostics.automatic_rows_considered;
        snapshot.diagnostics.product_fracture_rows =
            transition_cache->product_fracture_rows.size();
        snapshot.diagnostics.product_fracture_raw_outcomes = 0;
        snapshot.diagnostics.product_fracture_hit_entries = 0;
        snapshot.diagnostics.product_fracture_miss_entries = 0;
        snapshot.diagnostics
            .product_fracture_parent_miss_states_interned = 0;
        snapshot.diagnostics.product_fracture_max_probability_error = 0.0;
        snapshot.diagnostics.product_fracture_witnesses.clear();
        for (const auto& witness :
             transition_cache->product_fracture_rows) {
            snapshot.diagnostics.product_fracture_raw_outcomes +=
                witness.raw_affix_count;
            snapshot.diagnostics.product_fracture_hit_entries +=
                witness.hit_state_count;
            snapshot.diagnostics.product_fracture_miss_entries +=
                witness.miss_probability > 0.0 ? 1 : 0;
            snapshot.diagnostics
                .product_fracture_parent_miss_states_interned +=
                witness.parent_miss_state_count;
            snapshot.diagnostics
                .product_fracture_max_probability_error =
                std::max(
                    snapshot.diagnostics
                        .product_fracture_max_probability_error,
                    std::abs(witness.probability_sum - 1.0));
        }
        snapshot.diagnostics.product_fracture_witnesses_omitted =
            snapshot.diagnostics.product_fracture_rows;
        snapshot.diagnostics.incremental_action_generation =
            incremental_action_generation;
        snapshot.diagnostics.incremental_action_envelope_closed =
            !incremental_action_generation || incremental_envelope_closed;
        snapshot.diagnostics.incremental_actions_unevaluated =
            incremental_unevaluated_actions;
        snapshot.diagnostics.incremental_actions_evaluating =
            expansion_active && expansion_is_incremental_alternative ? 1 : 0;
        snapshot.diagnostics.incremental_actions_unresolved =
            incremental_resource_unresolved_actions;
        snapshot.diagnostics.incremental_actions_inapplicable =
            incremental_inapplicable_actions;
        snapshot.diagnostics.incremental_actions_admitted = 0;
        snapshot.diagnostics.incremental_actions_non_improving = 0;
        snapshot.diagnostics
            .incremental_states_outside_chaos_support = 0;
        for (const IncrementalAlternativeRow& candidate :
             incremental_alternative_rows) {
            if (candidate.status ==
                IncrementalAlternativeRow::Status::Admitted) {
                ++snapshot.diagnostics.incremental_actions_admitted;
            } else if (candidate.status ==
                       IncrementalAlternativeRow::Status::NonImproving) {
                ++snapshot.diagnostics
                    .incremental_actions_non_improving;
            } else {
                ++snapshot.diagnostics.incremental_actions_unresolved;
            }
            snapshot.diagnostics
                .incremental_states_outside_chaos_support +=
                candidate.states_added;
        }
        snapshot.diagnostics.incremental_unique_kernel_evaluations =
            incremental_unique_kernel_evaluations;
        snapshot.diagnostics.incremental_carrier_kernel_reuses =
            incremental_carrier_kernel_reuses;
        snapshot.diagnostics.incremental_bellman_reoptimizations =
            incremental_reoptimizations;
        snapshot.diagnostics
            .incremental_first_alternative_expanded_states =
            incremental_first_alternative_expanded_states;
        snapshot.diagnostics.incremental_refinement_rounds =
            incremental_refinement_rounds;
        snapshot.diagnostics.incremental_refinement_states_selected =
            incremental_refinement_states_selected;
        snapshot.diagnostics.incremental_rows_reconsidered =
            incremental_rows_reconsidered;
        snapshot.diagnostics.incremental_upper_policy_updates =
            incremental_upper_policy_updates;
        snapshot.diagnostics
            .incremental_upper_policy_passes_requested =
            incremental_upper_policy_passes_requested;
        snapshot.diagnostics
            .incremental_upper_policy_passes_started =
            incremental_upper_policy_passes_started;
        snapshot.diagnostics
            .incremental_upper_policy_passes_proper =
            incremental_upper_policy_passes_proper;
        snapshot.diagnostics
            .incremental_upper_policy_passes_rejected =
            incremental_upper_policy_passes_rejected;
        snapshot.diagnostics.incremental_upper_policy_last_failure =
            incremental_upper_policy_last_failure;
        snapshot.diagnostics.incremental_refinement_uncertainty =
            incremental_refinement_uncertainty;
        snapshot.diagnostics.solve_owned_byte_ledger_requests =
            owned_byte_ledger_requests;
        snapshot.diagnostics.solve_owned_byte_reconciliations =
            owned_byte_reconciliations;
        snapshot.diagnostics.solve_owned_byte_ledger_max_overestimate =
            owned_byte_ledger_max_overestimate;
        const std::uint64_t live_bytes = estimated_owned_bytes();
        snapshot.diagnostics.solver_owned_bytes_estimate =
            std::max(peak_owned_bytes, live_bytes);
        snapshot.diagnostics.solver_live_owned_bytes_estimate = live_bytes;
        snapshot.diagnostics.diagnostics_retained_bytes_estimate =
            diagnostics_owned_bytes(snapshot.diagnostics);
        snapshot.raw_start_bound = progress().start_value_bound;
        return snapshot;
    }

std::uint64_t SolveWork::Impl::estimated_owned_bytes() const {
        return estimated_owned_bytes_with_calc(
            calc.estimated_owned_bytes());
    }

std::uint64_t SolveWork::Impl::audited_estimated_owned_bytes() const {
        const std::uint64_t calc_bytes =
            calc.audited_estimated_owned_bytes();
        const std::uint64_t audited =
            estimated_owned_bytes_with_calc(calc_bytes);
        const std::uint64_t fast =
            fast_estimated_owned_bytes_with_calc(calc.fast_estimated_owned_bytes());
        ++owned_byte_reconciliations;
        if (fast < audited) {
            throw std::logic_error(
                "incremental SolveWork selected-owned-byte ledger "
                "undercounted by " + std::to_string(audited - fast) +
                " bytes (audited=" + std::to_string(audited) +
                ", ledger=" + std::to_string(fast) +
                ", calc=" + std::to_string(calc_bytes) +
                ", transition_cache_audited=" +
                std::to_string(
                    transition_cache == nullptr
                        ? 0
                        : transition_cache->audited_estimated_owned_bytes()) +
                ", transition_cache_ledger=" +
                std::to_string(
                    transition_cache == nullptr
                        ? 0
                        : transition_cache->fast_estimated_owned_bytes()) +
                ", operator_nested=" +
                std::to_string(owned_operators_nested_bytes) +
                ", kernel_bucket_nested=" +
                std::to_string(owned_kernel_row_bucket_bytes) +
                ", result_nested=" +
                std::to_string(owned_result_nested_bytes) + ")");
        }
        owned_byte_ledger_max_overestimate = std::max(
            owned_byte_ledger_max_overestimate, fast - audited);
        return audited;
    }

std::uint64_t SolveWork::Impl::output_incumbent_owned_bytes() const {
        std::uint64_t bytes = certified_fallback_portfolio.capacity() *
            sizeof(BoundedPolicyIncumbent);
        if (output_incumbent.has_value()) {
            /* std::optional owns its inline object inside Impl; only the
             * selected dynamic allocations are additional live storage. */
            bytes += incumbent_owned_bytes(*output_incumbent) -
                sizeof(BoundedPolicyIncumbent);
        }
        if (unverified_selected_policy_candidate.has_value()) {
            /* The wrapper and nested incumbent are inline in Impl. */
            bytes += incumbent_owned_bytes(
                         unverified_selected_policy_candidate->snapshot) -
                sizeof(BoundedPolicyIncumbent);
        }
        for (const BoundedPolicyIncumbent& incumbent :
             certified_fallback_portfolio) {
            bytes += incumbent_owned_bytes(incumbent) -
                sizeof(BoundedPolicyIncumbent);
        }
        return bytes;
    }

std::uint64_t SolveWork::Impl::fallback_policy_dynamic_owned_bytes(
        const FocusedFallbackPolicy& fallback) const {
        std::uint64_t bytes = fallback.renewal_kernel_signature.capacity() *
            sizeof(std::uint64_t);
        bytes += fallback.primitive_renewal_modes.capacity() *
            sizeof(FocusedFallbackPolicy::PrimitiveRenewalMode);
        for (const auto& mode : fallback.primitive_renewal_modes) {
            bytes += mode.kernel_signature.capacity() *
                sizeof(std::uint64_t);
        }
        bytes += fallback.progress_state_value.bucket_count() *
            sizeof(void*);
        bytes += fallback.progress_state_value.size() *
            (sizeof(std::pair<const std::uint32_t, double>) +
             2 * sizeof(void*));
        bytes += fallback.progress_state_operator.bucket_count() *
            sizeof(void*);
        bytes += fallback.progress_state_operator.size() *
            (sizeof(std::pair<const std::uint32_t, std::uint32_t>) +
             2 * sizeof(void*));
        return bytes;
    }

std::uint64_t SolveWork::Impl::fast_estimated_owned_bytes() const {
        ++owned_byte_ledger_requests;
        return fast_estimated_owned_bytes_with_calc(
            calc.fast_estimated_owned_bytes());
    }

std::uint64_t SolveWork::Impl::fast_estimated_owned_bytes_with_calc(
        const std::uint64_t calc_bytes) const {
        std::uint64_t bytes =
            sizeof(*this) -
            kUpperPolicyProvenanceAccountingOffset +
            calc_bytes;
        bytes += prices.bucket_count() * sizeof(void*);
        bytes += prices.size() *
                 (sizeof(std::pair<const std::string, double>) +
                  2 * sizeof(void*));
        bytes += owned_prices_nested_bytes;
        bytes += operators.capacity() * sizeof(PricedOperator);
        bytes += owned_operators_nested_bytes;
        bytes += (reported_unsupported.capacity() + 7) / 8;
        bytes += static_operator_indices.capacity() * sizeof(std::uint32_t);
        bytes += delayed_operator_indices.capacity() *
                 sizeof(std::uint32_t);
        bytes += expansion_operator_indices.capacity() *
                 sizeof(std::uint32_t);
        bytes += incremental_carriers.capacity() * sizeof(std::uint32_t);
        bytes += incremental_certified_upper_values.capacity() *
                 sizeof(double);
        bytes += incremental_dynamic_operator_indices.capacity() *
                 sizeof(std::uint32_t);
        bytes += incremental_alternative_rows.capacity() *
                 sizeof(IncrementalAlternativeRow);
        bytes += incremental_completed_pairs.bucket_count() * sizeof(void*);
        bytes += incremental_completed_pairs.size() *
                 (sizeof(std::uint64_t) + 2 * sizeof(void*));
        bytes += incremental_chaos_support.capacity() *
                 sizeof(std::uint8_t);
        bytes += incremental_nonchaos_states_seen.capacity() *
                 sizeof(std::uint8_t);
        bytes += expanded.capacity() * sizeof(std::uint8_t);
        bytes += queued.capacity() * sizeof(std::uint8_t);
        bytes += static_cast<std::uint64_t>(peak_queue_size) *
                 sizeof(std::uint32_t);
        if (transition_cache != nullptr) {
            bytes += transition_cache->fast_estimated_owned_bytes();
        }
        if (focused_strict_transition_cache != nullptr &&
            focused_strict_transition_cache != transition_cache) {
            bytes +=
                focused_strict_transition_cache->fast_estimated_owned_bytes();
        }
        bytes += focused_strict_expanded.capacity() * sizeof(std::uint8_t);
        bytes += focused_behavioral_representative.capacity() *
                 sizeof(std::uint32_t);
        bytes += focused_previous_upper_values.capacity() * sizeof(double);
        bytes += focused_previous_upper_policy_rows.capacity() *
                 sizeof(std::uint64_t);
        bytes += focused_frontier_upper_operator.capacity() *
                 sizeof(std::uint32_t);
        bytes += focused_previous_frontier_upper_operator.capacity() *
                 sizeof(std::uint32_t);
        bytes += focused_round_lower_values.capacity() * sizeof(double);
        bytes += focused_round_lower_policy_rows.capacity() *
                 sizeof(std::uint64_t);
        bytes += focused_pending_lower_fringe.capacity() *
                 sizeof(std::uint32_t);
        bytes += focused_pending_upper_fringe.capacity() *
                 sizeof(std::uint32_t);
        bytes += focused_pending_upper_priority.capacity() * sizeof(double);
        bytes += operator_goal_reach_mask.capacity() * sizeof(std::uint32_t);
        bytes += operator_goal_reach_computed.capacity() *
                 sizeof(std::uint8_t);
        bytes += goal_cover_cost.capacity() * sizeof(double);
        bytes += clean_goal_cover_cost.capacity() * sizeof(double);
        bytes += clean_goal_escape_cost.capacity() * sizeof(double);
        bytes += clean_goal_escape_action.capacity() *
                 sizeof(std::uint32_t);
        bytes += clean_goal_no_exalt_escape_cost.capacity() *
                 sizeof(double);
        bytes += clean_goal_no_exalt_escape_action.capacity() *
                 sizeof(std::uint32_t);
        bytes += strict_clean_goal_cover_cost.capacity() * sizeof(double);
        if (focused_fallback_policy) {
            const FocusedFallbackPolicy& fallback =
                *focused_fallback_policy;
            bytes += sizeof(FocusedFallbackPolicy);
            bytes += fallback.renewal_kernel_signature.capacity() *
                sizeof(std::uint64_t);
            bytes += fallback.primitive_renewal_modes.capacity() *
                sizeof(FocusedFallbackPolicy::PrimitiveRenewalMode);
            for (const auto& mode : fallback.primitive_renewal_modes) {
                bytes += mode.kernel_signature.capacity() *
                    sizeof(std::uint64_t);
            }
            bytes += fallback.progress_state_value.bucket_count() *
                sizeof(void*);
            bytes += fallback.progress_state_value.size() *
                (sizeof(std::pair<const std::uint32_t, double>) +
                 2 * sizeof(void*));
            bytes += fallback.progress_state_operator.bucket_count() *
                sizeof(void*);
            bytes += fallback.progress_state_operator.size() *
                (sizeof(std::pair<const std::uint32_t, std::uint32_t>) +
                 2 * sizeof(void*));
        }
        if (constructive_progress_fallback.has_value()) {
            bytes += fallback_policy_dynamic_owned_bytes(
                *constructive_progress_fallback);
        }
        bytes += primitive_destructive_renewal_work
                     .materialized_alternatives.capacity() *
                 sizeof(std::uint64_t);
        bytes += primitive_destructive_renewal_work
                     .renewal_sources.capacity() *
                 sizeof(std::uint32_t);
        bytes += fallback_policy_dynamic_owned_bytes(
            primitive_destructive_renewal_work.best);
        bytes += certified_state_upper.capacity() * sizeof(double);
        bytes += certified_state_row.capacity() * sizeof(std::uint64_t);
        bytes += priced_rows.capacity() * sizeof(PricedSparseRow);
        bytes += priced_operator_position.capacity() * sizeof(std::int32_t);
        bytes += prioritized_states.capacity() *
                 sizeof(std::pair<double, std::uint32_t>);
        bytes += policy_rows.capacity() * sizeof(std::uint64_t);
        bytes += policy_seed_states.capacity() * sizeof(std::uint32_t);
        bytes += policy_selection_states.capacity() * sizeof(std::uint32_t);
        bytes += current_policy_scratch_bytes;
        bytes += anytime_policy_scratch_bytes;
        bytes += kernel_value_caches.capacity() * sizeof(KernelValueCache);
        bytes += kernel_value_cache_by_offset.bucket_count() * sizeof(void*);
        bytes += kernel_value_cache_by_offset.size() *
                 (sizeof(std::pair<const std::uint64_t, std::size_t>) +
                  2 * sizeof(void*));
        bytes += owned_kernel_value_cache_nested_bytes;
        bytes += kernel_rows_by_hash.bucket_count() * sizeof(void*);
        bytes += kernel_rows_by_hash.size() *
                 (sizeof(std::pair<const std::size_t,
                                   std::vector<KernelRowMemo>>) +
                  2 * sizeof(void*));
        bytes += owned_kernel_row_bucket_bytes;
        bytes += automatic_admission_records.bucket_count() * sizeof(void*);
        bytes += automatic_admission_records.size() *
                 (sizeof(std::uint64_t) + 2 * sizeof(void*));
        bytes += automatic_carrier_work.bucket_count() * sizeof(void*);
        bytes += automatic_carrier_work.size() *
                 (sizeof(std::uint64_t) + sizeof(AutomaticCarrierWork) +
                  2 * sizeof(void*));
        bytes += result.values.capacity() * sizeof(double);
        bytes += result.policy.capacity() * sizeof(PolicyOperatorRef);
        bytes += result.expanded.capacity() * sizeof(std::uint8_t);
        bytes += result.goal_states.capacity() * sizeof(std::uint8_t);
        bytes += result.policy_reachable.capacity() * sizeof(std::uint8_t);
        bytes += result.unveil_preferences.capacity() *
                 sizeof(std::vector<std::uint32_t>);
        bytes += result.option_unveil_preferences.capacity() *
                 sizeof(std::vector<ObservedUnveilPreference>);
        bytes += result.behavioral_representative_by_state.capacity() *
                 sizeof(std::uint32_t);
        bytes += result.primitive_renewal_witness
                     .kernel_signature.capacity() *
                 sizeof(std::uint64_t);
        bytes +=
            result.refined_policy_artifact.strategy_json.capacity() + 1;
        bytes += result.refined_policy_artifact
                     .certification_strategy_json.capacity() + 1;
        bytes += result.refined_policy_artifact
                     .policy_route_default_mode.capacity() + 1;
        bytes += result.refined_policy_artifact
                     .certification_policy_route_default_mode.capacity() + 1;
        bytes += owned_result_nested_bytes;
        bytes += output_incumbent_owned_bytes();
        if (finalization_task.has_value()) {
            bytes += finalization_task->retained_bytes();
        }
        if (finalized_result.has_value()) {
            bytes += solve_result_owned_bytes(*finalized_result) -
                (sizeof(SolveResult) -
                 kUpperPolicyProvenanceAccountingOffset);
        }
        /* Diagnostic samples are strictly bounded and are not graph-sized.
         * Keep their exact current allocation in both ledger paths. */
        bytes += diagnostics_owned_bytes(result.diagnostics);
        return bytes;
    }

std::uint64_t SolveWork::Impl::estimated_owned_bytes_with_calc(
        const std::uint64_t calc_bytes) const {
        std::uint64_t bytes =
            sizeof(*this) -
            kUpperPolicyProvenanceAccountingOffset +
            calc_bytes;
        bytes += prices.bucket_count() * sizeof(void*);
        bytes += prices.size() *
                 (sizeof(std::pair<const std::string, double>) +
                  2 * sizeof(void*));
        for (const auto& [key, price] : prices) {
            (void)price;
            bytes += key.capacity() + 1;
        }
        bytes += operators.capacity() * sizeof(PricedOperator);
        for (const PricedOperator& priced : operators) {
            bytes += priced.resource_prices.capacity() *
                     sizeof(std::pair<std::string, double>);
            for (const auto& [key, price] : priced.resource_prices) {
                (void)price;
                bytes += key.capacity() + 1;
            }
        }
        bytes += (reported_unsupported.capacity() + 7) / 8;
        bytes += static_operator_indices.capacity() * sizeof(std::uint32_t);
        bytes += delayed_operator_indices.capacity() *
                 sizeof(std::uint32_t);
        bytes += expansion_operator_indices.capacity() *
                 sizeof(std::uint32_t);
        bytes += incremental_carriers.capacity() * sizeof(std::uint32_t);
        bytes += incremental_certified_upper_values.capacity() *
                 sizeof(double);
        bytes += incremental_dynamic_operator_indices.capacity() *
                 sizeof(std::uint32_t);
        bytes += incremental_alternative_rows.capacity() *
                 sizeof(IncrementalAlternativeRow);
        bytes += incremental_completed_pairs.bucket_count() * sizeof(void*);
        bytes += incremental_completed_pairs.size() *
                 (sizeof(std::uint64_t) + 2 * sizeof(void*));
        bytes += incremental_chaos_support.capacity() *
                 sizeof(std::uint8_t);
        bytes += incremental_nonchaos_states_seen.capacity() *
                 sizeof(std::uint8_t);
        bytes += expanded.capacity() * sizeof(std::uint8_t);
        bytes += queued.capacity() * sizeof(std::uint8_t);
        bytes += static_cast<std::uint64_t>(peak_queue_size) *
                 sizeof(std::uint32_t);
        if (transition_cache != nullptr) {
            bytes += transition_cache->estimated_owned_bytes();
        }
        if (focused_strict_transition_cache != nullptr &&
            focused_strict_transition_cache != transition_cache) {
            bytes += focused_strict_transition_cache->estimated_owned_bytes();
        }
        bytes += focused_strict_expanded.capacity() * sizeof(std::uint8_t);
        bytes += focused_behavioral_representative.capacity() *
                 sizeof(std::uint32_t);
        bytes += focused_previous_upper_values.capacity() * sizeof(double);
        bytes += focused_previous_upper_policy_rows.capacity() *
                 sizeof(std::uint64_t);
        bytes += focused_frontier_upper_operator.capacity() *
                 sizeof(std::uint32_t);
        bytes += focused_previous_frontier_upper_operator.capacity() *
                 sizeof(std::uint32_t);
        bytes += focused_round_lower_values.capacity() * sizeof(double);
        bytes += focused_round_lower_policy_rows.capacity() *
                 sizeof(std::uint64_t);
        bytes += focused_pending_lower_fringe.capacity() *
                 sizeof(std::uint32_t);
        bytes += focused_pending_upper_fringe.capacity() *
                 sizeof(std::uint32_t);
        bytes += focused_pending_upper_priority.capacity() * sizeof(double);
        bytes += operator_goal_reach_mask.capacity() * sizeof(std::uint32_t);
        bytes += operator_goal_reach_computed.capacity() *
                 sizeof(std::uint8_t);
        bytes += goal_cover_cost.capacity() * sizeof(double);
        bytes += clean_goal_cover_cost.capacity() * sizeof(double);
        bytes += clean_goal_escape_cost.capacity() * sizeof(double);
        bytes += clean_goal_escape_action.capacity() *
                 sizeof(std::uint32_t);
        bytes += clean_goal_no_exalt_escape_cost.capacity() *
                 sizeof(double);
        bytes += clean_goal_no_exalt_escape_action.capacity() *
                 sizeof(std::uint32_t);
        bytes += strict_clean_goal_cover_cost.capacity() * sizeof(double);
        if (focused_fallback_policy) {
            const FocusedFallbackPolicy& fallback =
                *focused_fallback_policy;
            bytes += sizeof(FocusedFallbackPolicy);
            bytes += fallback.renewal_kernel_signature.capacity() *
                sizeof(std::uint64_t);
            bytes += fallback.primitive_renewal_modes.capacity() *
                sizeof(FocusedFallbackPolicy::PrimitiveRenewalMode);
            for (const auto& mode : fallback.primitive_renewal_modes) {
                bytes += mode.kernel_signature.capacity() *
                    sizeof(std::uint64_t);
            }
            bytes += fallback.progress_state_value.bucket_count() *
                sizeof(void*);
            bytes += fallback.progress_state_value.size() *
                (sizeof(std::pair<const std::uint32_t, double>) +
                 2 * sizeof(void*));
            bytes += fallback.progress_state_operator.bucket_count() *
                sizeof(void*);
            bytes += fallback.progress_state_operator.size() *
                (sizeof(std::pair<const std::uint32_t, std::uint32_t>) +
                 2 * sizeof(void*));
        }
        if (constructive_progress_fallback.has_value()) {
            bytes += fallback_policy_dynamic_owned_bytes(
                *constructive_progress_fallback);
        }
        bytes += primitive_destructive_renewal_work
                     .materialized_alternatives.capacity() *
                 sizeof(std::uint64_t);
        bytes += primitive_destructive_renewal_work
                     .renewal_sources.capacity() *
                 sizeof(std::uint32_t);
        bytes += fallback_policy_dynamic_owned_bytes(
            primitive_destructive_renewal_work.best);
        bytes += certified_state_upper.capacity() * sizeof(double);
        bytes += certified_state_row.capacity() * sizeof(std::uint64_t);
        bytes += priced_rows.capacity() * sizeof(PricedSparseRow);
        bytes += priced_operator_position.capacity() * sizeof(std::int32_t);
        bytes += prioritized_states.capacity() *
                 sizeof(std::pair<double, std::uint32_t>);
        bytes += policy_rows.capacity() * sizeof(std::uint64_t);
        bytes += policy_seed_states.capacity() * sizeof(std::uint32_t);
        bytes += policy_selection_states.capacity() * sizeof(std::uint32_t);
        bytes += current_policy_scratch_bytes;
        bytes += anytime_policy_scratch_bytes;
        bytes += kernel_value_caches.capacity() * sizeof(KernelValueCache);
        bytes += kernel_value_cache_by_offset.bucket_count() * sizeof(void*);
        bytes += kernel_value_cache_by_offset.size() *
                 (sizeof(std::pair<const std::uint64_t, std::size_t>) +
                  2 * sizeof(void*));
        bytes += kernel_rows_by_hash.bucket_count() * sizeof(void*);
        bytes += kernel_rows_by_hash.size() *
                 (sizeof(std::pair<const std::size_t,
                                   std::vector<KernelRowMemo>>) +
                  2 * sizeof(void*));
        for (const auto& [unused, rows] : kernel_rows_by_hash) {
            (void)unused;
            bytes += rows.capacity() * sizeof(KernelRowMemo);
        }
        bytes += automatic_admission_records.bucket_count() * sizeof(void*);
        bytes += automatic_admission_records.size() *
                 (sizeof(std::uint64_t) + 2 * sizeof(void*));
        bytes += automatic_carrier_work.bucket_count() * sizeof(void*);
        bytes += automatic_carrier_work.size() *
                 (sizeof(std::uint64_t) + sizeof(AutomaticCarrierWork) +
                  2 * sizeof(void*));
        bytes += result.values.capacity() * sizeof(double);
        bytes += result.policy.capacity() * sizeof(PolicyOperatorRef);
        bytes += result.expanded.capacity() * sizeof(std::uint8_t);
        bytes += result.goal_states.capacity() * sizeof(std::uint8_t);
        bytes += result.policy_reachable.capacity() * sizeof(std::uint8_t);
        bytes += result.unveil_preferences.capacity() *
                 sizeof(std::vector<std::uint32_t>);
        for (const auto& preferences : result.unveil_preferences) {
            bytes += preferences.capacity() * sizeof(std::uint32_t);
        }
        bytes += result.option_unveil_preferences.capacity() *
                 sizeof(std::vector<ObservedUnveilPreference>);
        for (const auto& preferences :
             result.option_unveil_preferences) {
            bytes += preferences.capacity() *
                     sizeof(ObservedUnveilPreference);
            for (const ObservedUnveilPreference& preference : preferences) {
                bytes += preference.choices.capacity() *
                         sizeof(ObservedUnveilChoice);
            }
        }
        bytes += result.behavioral_representative_by_state.capacity() *
                 sizeof(std::uint32_t);
        bytes += result.primitive_renewal_witness
                     .kernel_signature.capacity() *
                 sizeof(std::uint64_t);
        bytes +=
            result.refined_policy_artifact.strategy_json.capacity() + 1;
        bytes += result.refined_policy_artifact
                     .certification_strategy_json.capacity() + 1;
        bytes += result.refined_policy_artifact
                     .policy_route_default_mode.capacity() + 1;
        bytes += result.refined_policy_artifact
                     .certification_policy_route_default_mode.capacity() + 1;
        bytes += output_incumbent_owned_bytes();
        if (finalization_task.has_value()) {
            bytes += finalization_task->retained_bytes();
        }
        if (finalized_result.has_value()) {
            bytes += solve_result_owned_bytes(*finalized_result) -
                (sizeof(SolveResult) -
                 kUpperPolicyProvenanceAccountingOffset);
        }
        bytes += diagnostics_owned_bytes(result.diagnostics);
        return bytes;
    }

std::uint64_t estimated_retained_solver_bytes(
    const CalcContext& calc,
    const SolveResult* result) {
    std::uint64_t bytes = calc.estimated_owned_bytes();
    if (calc.solve_transition_cache() != nullptr) {
        bytes += calc.solve_transition_cache()->estimated_owned_bytes();
    }
    if (result != nullptr) bytes += solve_result_owned_bytes(*result);
    return bytes;
}

std::string serialize_solve_log(
    const CalcContext& calc,
    const SolveResult& result) {
    std::string log;
    char buffer[256];
    for (std::uint32_t state = 0; state < result.values.size(); ++state) {
        if (!result.expanded[state]) continue;
        const AbstractState& features = calc.state(state);
        std::snprintf(buffer, sizeof(buffer),
                      "{\"state\":%u,\"value\":%.9g,\"action\":", state,
                      result.values[state]);
        log += buffer;
        const std::uint32_t action = result.policy[state];
        if (action == kNoId) {
            log += "null";
        } else {
            log += '"';
            log += calc.operators().at(action).id;
            log += '"';
        }
        std::snprintf(
            buffer, sizeof(buffer),
            ",\"goal\":%d,\"reachable\":%d,\"rarity\":%u,\"prefixes\":%u,"
            "\"suffixes\":%u,\"blocked\":%u,\"flags\":%u,"
            "\"veiled_side\":%d,\"searing_tier\":%u,\"eater_tier\":%u,"
            "\"slots\":[",
            result.goal_states[state] ? 1 : 0,
            result.policy_reachable[state] ? 1 : 0, features.rarity,
            features.prefix_count, features.suffix_count,
            features.blocked_mask, features.flags, features.veiled_side,
            features.searing_exarch_tier,
            features.eater_of_worlds_tier);
        log += buffer;
        for (std::size_t i = 0; i < calc.layout().slots.size(); ++i) {
            if (i > 0) log += ',';
            log += std::to_string(features.slot_status[i]);
        }
        log += "],\"junk\":[";
        for (std::size_t i = 0; i < features.junk_counts.size(); ++i) {
            if (i > 0) log += ',';
            log += std::to_string(features.junk_counts[i]);
        }
        log += "]}\n";
    }
    return log;
}

class BoundedTelemetryJson {
  public:
    explicit BoundedTelemetryJson(std::uint64_t limit) : limit_(limit) {}

    BoundedTelemetryJson& operator+=(const std::string& text) {
        append(text);
        return *this;
    }
    BoundedTelemetryJson& operator+=(const char* text) {
        append(text == nullptr ? std::string_view{} : std::string_view(text));
        return *this;
    }
    BoundedTelemetryJson& operator+=(char value) {
        push_back(value);
        return *this;
    }
    void push_back(char value) {
        ensure(1);
        value_.push_back(value);
    }
    std::size_t size() const { return value_.size(); }
    std::string take() && { return std::move(value_); }

  private:
    void append(std::string_view text) {
        ensure(text.size());
        value_.append(text.data(), text.size());
    }
    void ensure(std::size_t additional) const {
        if (value_.size() > limit_ || additional > limit_ - value_.size()) {
            throw std::length_error(
                "solver telemetry exceeded max_telemetry_json_bytes (" +
                std::to_string(limit_) + ")");
        }
    }
    std::string value_;
    std::uint64_t limit_;
};

void append_telemetry_json_string(
    BoundedTelemetryJson& out, const std::string_view value) {
    static constexpr char kHex[] = "0123456789abcdef";
    out += '"';
    for (const unsigned char c : value) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    out += "\\u00";
                    out += kHex[c >> 4];
                    out += kHex[c & 0x0f];
                } else {
                    out += static_cast<char>(c);
                }
                break;
        }
    }
    out += '"';
}

void append_reforge_effort_json(
    BoundedTelemetryJson& out,
    const ReforgeEffortBreakdown& effort) {
    out += "{\"rows_begun\":" + std::to_string(effort.rows_begun);
    out += ",\"rows_completed\":" +
           std::to_string(effort.rows_completed);
    out += ",\"rows_interrupted\":" +
           std::to_string(effort.rows_interrupted);
    out += ",\"rows_cache_reused\":" +
           std::to_string(effort.rows_cache_reused);
    out += ",\"rows_discarded\":" +
           std::to_string(effort.rows_discarded);
    out += ",\"rows_published\":" +
           std::to_string(effort.rows_published);
    out += ",\"pool_entries_scanned\":" +
           std::to_string(effort.pool_entries_scanned);
    out += ",\"physical_families_built\":" +
           std::to_string(effort.physical_families_built);
    out += ",\"roll_buckets_built\":" +
           std::to_string(effort.roll_buckets_built);
    out += ",\"exclusion_group_checks\":" +
           std::to_string(effort.exclusion_group_checks);
    out += ",\"availability_classes_built\":" +
           std::to_string(effort.availability_classes_built);
    out += ",\"availability_words_built\":" +
           std::to_string(effort.availability_words_built);
    out += ",\"frontier_nodes\":" +
           std::to_string(effort.frontier_nodes);
    out += ",\"dense_bucket_probes\":" +
           std::to_string(effort.dense_bucket_probes);
    out += ",\"availability_words_scanned\":" +
           std::to_string(effort.availability_words_scanned);
    out += ",\"eligible_nonterminal_edges\":" +
           std::to_string(effort.eligible_nonterminal_edges);
    out += ",\"terminal_contributions\":" +
           std::to_string(effort.terminal_contributions);
    out += ",\"canonical_terminal_successors\":" +
           std::to_string(effort.canonical_terminal_successors);
    out += ",\"duplicate_terminal_contributions\":" +
           std::to_string(effort.duplicate_terminal_contributions);
    out += ",\"raw_choice_entries\":" +
           std::to_string(effort.raw_choice_entries);
    out += ",\"identity_tree_nodes\":" +
           std::to_string(effort.identity_tree_nodes);
    out += ",\"successor_publication_attempts\":" +
           std::to_string(effort.successor_publication_attempts);
    out += ",\"successor_unique_insertions\":" +
           std::to_string(effort.successor_unique_insertions);
    out += ",\"successor_duplicate_merges\":" +
           std::to_string(effort.successor_duplicate_merges);
    out += ",\"state_interning_attempts\":" +
           std::to_string(effort.state_interning_attempts);
    out += ",\"v3_predecessor_index_entries\":" +
           std::to_string(effort.v3_predecessor_index_entries);
    out += ",\"v3_denominator_edges\":" +
           std::to_string(effort.v3_denominator_edges);
    out += ",\"v3_subset_checks\":" +
           std::to_string(effort.v3_subset_checks);
    out += ",\"v3_candidate_sets\":" +
           std::to_string(effort.v3_candidate_sets);
    out += ",\"v3_recurrence_terms\":" +
           std::to_string(effort.v3_recurrence_terms);
    out += ",\"v3_commits\":" +
           std::to_string(effort.v3_commits);
    out += ",\"nested_automatic_child_logical_work\":" +
           std::to_string(
               effort.nested_automatic_child_logical_work);
    out += ",\"nested_automatic_child_active_work\":" +
           std::to_string(
               effort.nested_automatic_child_active_work) + "}";
}

void append_reforge_resource_accounting_json(
    BoundedTelemetryJson& out,
    const std::uint64_t legacy_active_work,
    const std::uint64_t logical_work_v1,
    const std::uint64_t evaluator_work_v1,
    const std::uint64_t evaluator_work_v2,
    const std::uint64_t evaluator_work_v3,
    const ReforgeEffortBreakdown& effort,
    const std::vector<ReforgeRowTelemetry>& row_samples,
    const std::uint64_t row_samples_omitted) {
    out += "{\"schema_version\":2";
    out += ",\"cap_contract\":{\"version\":1,";
    out += "\"cap\":\"max_reforge_work\",";
    out += "\"basis\":\"logical_work_v1\"}";
    out += ",\"legacy_active_work\":" +
           std::to_string(legacy_active_work);
    out += ",\"logical_work_v1\":" +
           std::to_string(logical_work_v1);
    out += ",\"evaluator_work\":{\"v1\":" +
           std::to_string(evaluator_work_v1);
    out += ",\"v2\":" + std::to_string(evaluator_work_v2);
    out += ",\"v3\":" + std::to_string(evaluator_work_v3) + "}";
    out += ",\"components\":";
    append_reforge_effort_json(out, effort);
    out += ",\"row_samples\":[";
    for (std::size_t i = 0; i < row_samples.size(); ++i) {
        if (i != 0) out.push_back(',');
        const ReforgeRowTelemetry& row = row_samples[i];
        out += "{\"sequence\":" + std::to_string(row.sequence);
        out += ",\"action_index\":" +
               std::to_string(row.action_index);
        out += ",\"owner\":\"";
        out += reforge_row_owner_name(row.owner);
        out += "\",\"family\":\"";
        out += reforge_row_family_name(row.family);
        out += "\",\"evaluator\":\"";
        out += reforge_evaluator_version_name(row.evaluator);
        out += "\",\"cache\":\"";
        out += (row.cache_reused ? "hit" : "miss");
        out += "\",\"cache_reused\":";
        out += (row.cache_reused ? "true" : "false");
        out += ",\"disposition\":\"";
        out += reforge_row_disposition_name(row.disposition);
        out += "\",\"legacy_active_work\":" +
               std::to_string(row.legacy_active_work);
        out += ",\"logical_work_v1\":" +
               std::to_string(row.logical_work_v1);
        out += ",\"evaluator_work\":{\"v1\":" +
               std::to_string(row.evaluator_work_v1);
        out += ",\"v2\":" +
               std::to_string(row.evaluator_work_v2);
        out += ",\"v3\":" +
               std::to_string(row.evaluator_work_v3) + "}";
        out += ",\"components\":";
        append_reforge_effort_json(out, row.components);
        out += "}";
    }
    out += "],\"row_samples_omitted\":" +
           std::to_string(row_samples_omitted) + "}";
}

std::string telemetry_finite_json(const double value) {
    if (!std::isfinite(value)) return "null";
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.17g", value);
    return buffer;
}

std::string telemetry_hex_u64(const std::uint64_t value) {
    char buffer[17];
    std::snprintf(
        buffer, sizeof(buffer), "%016llx",
        static_cast<unsigned long long>(value));
    return buffer;
}

std::string serialize_solver_telemetry(
    const CalcContext& calc,
    const SolveResult* result,
    const SolveTelemetrySnapshot* snapshot,
    const std::optional<std::uint64_t>& registry_generation_ns,
    const PolicyCompilationTelemetry* compilation) {
    const CalcTelemetry& cache = calc.telemetry();
    const SolveDiagnostics* diagnostics =
        result != nullptr
            ? &result->diagnostics
            : (snapshot == nullptr ? nullptr : &snapshot->diagnostics);
    const bool qualified_action_subset =
        diagnostics != nullptr &&
        (diagnostics->skipped_missing_price_count != 0 ||
         diagnostics->evaluator_supported_actions <
             diagnostics->candidate_actions ||
         diagnostics->skipped_unsupported_count != 0);
    const auto count_or_null = [](const std::uint64_t* value) {
        return value == nullptr ? std::string("null")
                                : std::to_string(*value);
    };
    const auto optional_count = [](const auto& value) {
        return value.has_value() ? std::to_string(*value)
                                 : std::string("null");
    };
    const auto bool_json = [](bool value) {
        return value ? "true" : "false";
    };
    const auto policy_status_name = [](const SolvePolicyStatus status) {
        switch (status) {
        case SolvePolicyStatus::None: return "none";
        case SolvePolicyStatus::BoundedFeasible:
            return "bounded_feasible";
        case SolvePolicyStatus::BoundedNearOptimal:
            return "bounded_near_optimal";
        case SolvePolicyStatus::Exact: return "exact";
        }
        return "none";
    };
    const auto termination_name = [](const SolveTermination termination) {
        switch (termination) {
        case SolveTermination::None: return "none";
        case SolveTermination::RefusedResourceCap:
            return "refused_resource_cap";
        case SolveTermination::TargetGap: return "target_gap";
        case SolveTermination::ExactClosed: return "exact_closed";
        case SolveTermination::NoExecutablePolicy:
            return "no_executable_policy";
        case SolveTermination::NumericalStability:
            return "numerical_stability";
        }
        return "none";
    };
    const auto gap_target_name = [](const SolveGapTarget target) {
        switch (target) {
        case SolveGapTarget::None: return "none";
        case SolveGapTarget::Absolute: return "absolute";
        case SolveGapTarget::Relative: return "relative";
        case SolveGapTarget::Both: return "both";
        }
        return "none";
    };

    const std::uint64_t output_limit =
        diagnostics == nullptr
            ? SolveOptions{}.max_telemetry_json_bytes
            : diagnostics->telemetry_json_byte_limit;
    BoundedTelemetryJson json(output_limit);
    const auto append_refinement_feature_counts =
        [&](const auto& counts) {
            json += '[';
            for (std::size_t feature = 0;
                 feature < counts.size(); ++feature) {
                if (feature != 0) json += ',';
                json += std::to_string(counts[feature]);
            }
            json += ']';
        };
    json += "{\"version\":\"solver_telemetry_v1\"";
    json += ",\"availability\":{";
    json += "\"evaluator_support\":\"applied_before_expansion\"";
    json += ",\"relevance_filter\":\"explicit_envelope_or_conservative_include\"";
    json += ",\"dominance_filter\":\"certified_kernel_equivalence_preservation_or_constructive_goal_bound\"";
    json += ",\"policy_improvement_rounds\":\"available\"";
    json += ",\"optimality_gap\":\"available_for_focused_expansion\"";
    json += ",\"verification\":\"external_harness\"}";

    json += ",\"execution\":{";
    if (result != nullptr) {
        json += "\"status\":\"complete\",\"phase\":\"done\"";
    } else if (snapshot != nullptr) {
        const char* phase = snapshot->phase == SolvePhase::Expanding
                                ? "expanding"
                                : (snapshot->phase == SolvePhase::Iterating
                                       ? "iterating"
                                       : "ready_to_finish");
        json += "\"status\":\"";
        json += snapshot->abandoned ? "abandoned" : "in_progress";
        json += "\",\"phase\":\"" + std::string(phase) + "\"";
    } else {
        json += "\"status\":\"not_started\",\"phase\":null";
    }
    json += ",\"solution_scope\":";
    if (diagnostics == nullptr) {
        json += "null";
    } else {
        append_telemetry_json_string(
            json, diagnostics->solution_scope);
    }
    json += "}";

    const std::uint64_t registry_actions =
        diagnostics == nullptr
            ? calc.registry().actions.size()
            : diagnostics->registry_actions;
    const std::uint64_t candidate_actions =
        diagnostics == nullptr ? calc.candidates().size()
                               : diagnostics->candidate_actions;
    json += ",\"actions\":{\"registry\":" +
            std::to_string(registry_actions);
    json += ",\"registry_before_lazy\":" + std::to_string(
        registry_actions + calc.registry().fossil_loadouts_deferred);
    json += ",\"candidate\":" + std::to_string(candidate_actions);
    if (diagnostics == nullptr) {
        json += ",\"evaluator_supported\":null";
        json += ",\"priced_scanned\":null,\"supported_priced\":null";
        json += ",\"unsupported_requested\":null";
        json += ",\"relevance_reduced\":null,\"dominance_reduced\":null";
        json += ",\"deferred\":null,\"equivalent_price_ties\":null";
        json += ",\"missing_price\":null,\"unsupported_observed\":null";
    } else {
        json += ",\"evaluator_supported\":" +
                std::to_string(diagnostics->evaluator_supported_actions);
        json += ",\"priced_scanned\":" +
                std::to_string(diagnostics->priced_scanned_actions);
        json += ",\"supported_priced\":" +
                std::to_string(diagnostics->supported_priced_actions);
        json += ",\"unsupported_requested\":" +
                std::to_string(diagnostics->candidate_actions -
                               diagnostics->evaluator_supported_actions);
        json += ",\"relevance_reduced\":" + std::to_string(
                    diagnostics->relevance_reduced_actions);
        json += ",\"dominance_reduced\":" + std::to_string(
                    diagnostics->equivalent_actions_collapsed +
                    diagnostics->preservation_rows_pruned +
                    diagnostics->constructive_state_operators_pruned);
        json += ",\"deferred\":" + std::to_string(
                    diagnostics->deferred_actions);
        json += ",\"equivalent_price_ties\":" + std::to_string(
                    diagnostics->equivalent_price_ties);
        json += ",\"missing_price\":" + std::to_string(
                    diagnostics->skipped_missing_price_count);
        json += ",\"unsupported_observed\":" + std::to_string(
                    diagnostics->skipped_unsupported_count);
        json += ",\"missing_price_samples\":[";
        for (std::size_t i = 0;
             i < diagnostics->skipped_missing_price.size(); ++i) {
            if (i != 0) json += ',';
            append_telemetry_json_string(
                json, diagnostics->skipped_missing_price[i]);
        }
        json += "]";
        json += ",\"unsupported_samples\":[";
        for (std::size_t i = 0;
             i < diagnostics->skipped_unsupported.size(); ++i) {
            if (i != 0) json += ',';
            append_telemetry_json_string(
                json, diagnostics->skipped_unsupported[i]);
        }
        json += "]";
    }
    json += "}";

    json += ",\"policy_compatibility\":{";
    if (diagnostics == nullptr) {
        json += "\"supported\":null,\"state_id\":null,"
                "\"action\":null,\"reason\":null,"
                "\"publication_failure_reason\":null";
    } else {
        json += "\"supported\":" + std::string(bool_json(
            diagnostics->policy_compatibility_supported));
        json += ",\"state_id\":";
        if (diagnostics->policy_compatibility_state == kNoId) {
            json += "null";
        } else {
            json += std::to_string(
                diagnostics->policy_compatibility_state);
        }
        json += ",\"action\":";
        if (diagnostics->policy_compatibility_action.empty()) {
            json += "null";
        } else {
            append_telemetry_json_string(
                json, diagnostics->policy_compatibility_action);
        }
        json += ",\"reason\":";
        if (diagnostics->policy_compatibility_reason.empty()) {
            json += "null";
        } else {
            append_telemetry_json_string(
                json, diagnostics->policy_compatibility_reason);
        }
        json += ",\"publication_failure_reason\":";
        if (diagnostics->policy_publication_failure_reason.empty()) {
            json += "null";
        } else {
            append_telemetry_json_string(
                json,
                diagnostics->policy_publication_failure_reason);
        }
    }
    json += "}";

    json += ",\"policy_refinement\":{";
    if (diagnostics == nullptr) {
        json += "\"triggers\":null,\"status\":null,\"resource_cap\":null";
        json += ",\"core_policy\":{"
                "\"candidate_present\":null,\"status\":null,"
                "\"lower_bound\":null,\"upper_bound\":null,"
                "\"evaluated_cost\":null,\"transition_bits_hash\":null,"
                "\"policy_bits_hash\":null,\"selected_states\":null,"
                "\"distinct_actions\":null,\"root_action\":null,"
                "\"goal_identity\":null,\"economy_identity\":null,"
                "\"action_vocabulary_identity\":null,"
                "\"graph_identity\":null,\"artifact_identity\":null,"
                "\"owned_bytes\":null}";
        json += ",\"direct_certification\":{"
                "\"status\":null,\"failure_reason\":null,"
                "\"resource_cap\":null,\"solver_cost\":null,"
                "\"exact_cost\":null,\"offpolicy_probability\":null,"
                "\"reforge_work\":null,\"artifact_bytes\":null,"
                "\"peak_owned_bytes\":null,\"executable\":null,"
                "\"proper\":null,\"cost_complete\":null,"
                "\"zero_off_policy\":null,\"cost_reconciled\":null,"
                "\"candidate_retained\":null}";
        json += ",\"strict_lift\":{"
                "\"status\":null,\"failure_reason\":null,"
                "\"resource_cap\":null,"
                "\"global_lower_bound_closed\":null}";
        json += ",\"publication\":{"
                "\"status\":null,\"candidate_kind\":null}";
        json += ",\"pre_restore_selected_policy\":{"
                "\"present\":null,\"materializable\":null,"
                "\"numerical_stop\":null,\"start_value\":null,"
                "\"residual\":null,\"policy_bits_hash\":null,"
                "\"selected_rows\":null,\"reachable_states\":null,"
                "\"reachable_rows\":null,\"distinct_actions\":null,"
                "\"choice_groups\":null,\"choice_options\":null,"
                "\"goal_identity\":null,\"economy_identity\":null,"
                "\"action_vocabulary_identity\":null,"
                "\"graph_identity\":null,\"artifact_identity\":null,"
                "\"source_generation\":null,"
                "\"target_generation\":null,\"snapshot_ns\":null,"
                "\"snapshot_peak_bytes\":null,"
                "\"strict_order_suppressed_comparisons\":null,"
                "\"suppressed_samples\":[],"
                "\"suppressed_samples_retained\":null,"
                "\"suppressed_samples_omitted\":null}";
        json += ",\"selected_policy_candidate\":{"
                "\"capture_attempted\":null,\"captured\":null,"
                "\"memory_rejected\":null,\"identity_valid\":null,"
                "\"certification_attempted\":null,"
                "\"independently_evaluated\":null,"
                "\"retained\":null,\"estimated_cost\":null,"
                "\"exact_cost\":null,\"owned_bytes\":null,"
                "\"identity\":null,\"capture_ns\":null,"
                "\"certification_ns\":null,\"status\":null,"
                "\"failure_reason\":null}";
        json += ",\"finalization_stages_ns\":{"
                "\"incumbent_restore\":null,"
                "\"extraction_materialization\":null,"
                "\"direct_certification\":null,"
                "\"strict_lift_total\":null,"
                "\"strict_carrier_discovery\":null,"
                "\"strict_partition_refinement\":null,"
                "\"strict_policy_evaluation\":null,"
                "\"strict_local_reoptimization\":null,"
                "\"strategy_compilation\":null,"
                "\"exact_graph_evaluation\":null}";
        json += ",\"policy_reachable_coarse_states\":null";
        json += ",\"exact_states\":null,\"retained_exact_states\":null";
        json += ",\"exact_classes\":null";
        json += ",\"initial_observation_classes\":null";
        json += ",\"behavior_splits\":null,\"merged_exact_states\":null";
        json += ",\"exact_transitions\":null,\"exact_kernels\":null";
        json += ",\"exact_kernel_cache_hits\":null";
        json += ",\"certification_work\":{"
                "\"selected\":{"
                "\"rows_begun\":null,\"rows_completed\":null,"
                "\"reforge_work\":null,\"transitions\":null},"
                "\"alternatives\":{"
                "\"rows_begun\":null,\"rows_completed\":null,"
                "\"reforge_work\":null,\"transitions\":null},"
                "\"work_to_first_partition\":null,"
                "\"work_to_first_executable_upper\":null,"
                "\"wall_ns_to_first_partition\":null,"
                "\"wall_ns_to_first_executable_upper\":null,"
                "\"exact_alternatives_materialized_before_first_upper\":"
                "null,\"alternative_obligations_created\":null,"
                "\"unresolved_alternative_obligations\":null,"
                "\"alternative_exact_rows_avoided\":null,"
                "\"action_accounting_complete\":null,"
                "\"scheduling_rounds\":null,"
                "\"obligations_scheduled\":null,"
                "\"obligations_certified\":null,"
                "\"obligations_partially_evaluated\":null,"
                "\"obligations_noncompetitive\":null,"
                "\"obligations_stale\":null,"
                "\"verdict_revocations\":null,"
                "\"obligations_resource_interrupted\":null,"
                "\"competitive_alternatives_remaining\":null,"
                "\"policy_improvements\":null,"
                "\"bounded_publication_retained\":null,"
                "\"global_lower_bound_closed\":null,"
                "\"exact_alternative_envelope_closed\":null}";
        json += ",\"memory_bytes\":null,\"peak_memory_bytes\":null";
        json += ",\"memory_limit_bytes\":null";
        json += ",\"retained_artifact_bytes\":null";
        json += ",\"fallback_portfolio\":{"
                "\"candidates\":null,\"invalidations\":null,"
                "\"compilation_failures\":null,"
                "\"memory_rejections\":null,\"owned_bytes\":null,"
                "\"publication_attempts\":null,"
                "\"publication_successes\":null,"
                "\"preferred_candidate_upper\":null,"
                "\"published_upper\":null,"
                "\"published_evaluated_cost\":null,"
                "\"published_witness_hash\":null,"
                "\"published_kind\":null,"
                "\"preferred_failure_reason\":null}";
        json += ",\"exact_state_reuses\":null,\"collapse_events\":null";
        json += ",\"collapse_destroyed_feature_mask\":null";
        json += ",\"collapse_preserved_feature_mask\":null";
        json += ",\"collapse_events_by_feature\":null";
        json += ",\"preservation_events_by_feature\":null";
        json += ",\"refinement_rounds\":null";
        json += ",\"backward_observation_rounds\":null";
        json += ",\"selected_action_routing_rounds\":null";
        json += ",\"observation_propagation_rounds\":null";
        json += ",\"partition_refinement_rounds\":null";
        json += ",\"local_reoptimization_rounds\":null";
        json += ",\"local_state_action_rows_scheduled\":null";
        json += ",\"local_state_action_rows_evaluated\":null";
        json += ",\"local_reoptimizations\":null";
        json += ",\"local_policy_changes\":null";
        json += ",\"local_value_changes\":null";
        json += ",\"quotient_proof\":{"
                "\"payload_reuses\":null,\"row_reprojections\":null,"
                "\"source_splits\":null,\"target_splits\":null,"
                "\"reverse_invalidations\":null,"
                "\"improper_policy_repairs\":null,"
                "\"exact_carriers_replayed\":null,"
                "\"current_live_slices\":null,\"peak_live_slices\":null,"
                "\"current_live_slice_bytes\":null,"
                "\"peak_live_slice_bytes\":null,"
                "\"coverage_descriptor_bytes\":null,"
                "\"certificate_bytes\":null,"
                "\"dependency_sidecar_bytes\":null,"
                "\"alternative_obligation_bytes\":null,"
                "\"partition_bytes\":null,\"carrier_bytes\":null,"
                "\"row_kernel_bytes\":null,\"scratch_bytes\":null,"
                "\"total_solver_owned_bytes\":null,"
                "\"reference_adapter_invocations\":null}";
        json += ",\"fixed_point\":{"
                "\"checked\":null,\"complete\":null,"
                "\"lumpability_checked\":null,\"lumpable\":null,"
                "\"lumpability_checks\":null}";
        json += ",\"class_policy\":{"
                "\"checked\":null,\"proper\":null}";
        json += ",\"compiled_assertion\":{"
                "\"checked\":null,\"proper\":null,"
                "\"zero_off_policy\":null,\"cost_reconciled\":null}";
        json += ",\"policy_changed\":null";
        json += ",\"coarse_value_reconciled\":null";
        json += ",\"counterexamples\":{"
                "\"count\":null,\"samples\":[],\"retained\":null,"
                "\"omitted\":null,\"limit\":null}";
        json += ",\"refusal_causes\":{"
                "\"count\":null,\"samples\":[],\"retained\":null,"
                "\"omitted\":null,\"limit\":null}";
        json += ",\"publication_candidates\":{"
                "\"samples\":[],\"retained\":null,\"omitted\":null,"
                "\"retained_bytes\":null,\"limit\":null}";
        json += ",\"structural_failures\":{"
                "\"samples\":[],\"retained\":null,\"omitted\":null,"
                "\"retained_bytes\":null,\"limit\":null}";
        json += ",\"evaluator_memory\":{"
                "\"samples\":[],\"retained\":null,\"omitted\":null,"
                "\"retained_bytes\":null,\"limit\":null}";
    } else {
        const PolicyRefinementTelemetry& refinement =
            diagnostics->policy_refinement;
        json += "\"triggers\":" +
                std::to_string(refinement.triggers);
        json += ",\"status\":";
        append_telemetry_json_string(json, refinement.status);
        json += ",\"resource_cap\":";
        if (refinement.resource_cap.empty()) {
            json += "null";
        } else {
            append_telemetry_json_string(json, refinement.resource_cap);
        }
        json += ",\"core_policy\":{\"candidate_present\":" +
                std::string(bool_json(
                    refinement.core_policy_candidate_present));
        json += ",\"status\":";
        append_telemetry_json_string(
            json, refinement.core_policy_status);
        json += ",\"lower_bound\":" +
                telemetry_finite_json(
                    refinement.core_policy_lower_bound);
        json += ",\"upper_bound\":" +
                telemetry_finite_json(
                    refinement.core_policy_upper_bound);
        json += ",\"evaluated_cost\":" +
                telemetry_finite_json(
                    refinement.core_policy_evaluated_cost);
        json += ",\"transition_bits_hash\":";
        append_telemetry_json_string(
            json,
            telemetry_hex_u64(
                refinement.core_policy_transition_bits_hash));
        json += ",\"policy_bits_hash\":";
        append_telemetry_json_string(
            json,
            telemetry_hex_u64(refinement.core_policy_bits_hash));
        json += ",\"selected_states\":" +
                std::to_string(
                    refinement.core_policy_selected_states);
        json += ",\"distinct_actions\":" +
                std::to_string(
                    refinement.core_policy_distinct_actions);
        json += ",\"root_action\":";
        if (refinement.core_policy_root_action.empty()) {
            json += "null";
        } else {
            append_telemetry_json_string(
                json, refinement.core_policy_root_action);
        }
        json += ",\"goal_identity\":";
        append_telemetry_json_string(
            json,
            telemetry_hex_u64(refinement.core_policy_goal_identity));
        json += ",\"economy_identity\":";
        append_telemetry_json_string(
            json,
            telemetry_hex_u64(refinement.core_policy_economy_identity));
        json += ",\"action_vocabulary_identity\":";
        append_telemetry_json_string(
            json,
            telemetry_hex_u64(
                refinement.core_policy_action_vocabulary_identity));
        json += ",\"graph_identity\":";
        append_telemetry_json_string(
            json,
            telemetry_hex_u64(refinement.core_policy_graph_identity));
        json += ",\"artifact_identity\":";
        append_telemetry_json_string(
            json,
            telemetry_hex_u64(refinement.core_policy_artifact_identity));
        json += ",\"owned_bytes\":" +
                std::to_string(refinement.core_policy_owned_bytes) + "}";
        json += ",\"direct_certification\":{\"status\":";
        append_telemetry_json_string(
            json, refinement.direct_certification_status);
        json += ",\"failure_reason\":";
        if (refinement.direct_certification_failure_reason.empty()) {
            json += "null";
        } else {
            append_telemetry_json_string(
                json,
                refinement.direct_certification_failure_reason);
        }
        json += ",\"failure_classification\":";
        if (refinement.direct_certification_failure_classification.empty()) {
            json += "null";
        } else {
            append_telemetry_json_string(
                json,
                refinement.direct_certification_failure_classification);
        }
        json += ",\"resource_cap\":";
        if (refinement.direct_certification_resource_cap.empty()) {
            json += "null";
        } else {
            append_telemetry_json_string(
                json, refinement.direct_certification_resource_cap);
        }
        json += ",\"solver_cost\":" +
                telemetry_finite_json(
                    refinement.direct_certification_solver_cost);
        json += ",\"exact_cost\":" +
                telemetry_finite_json(
                    refinement.direct_certification_exact_cost);
        json += ",\"offpolicy_probability\":" +
                telemetry_finite_json(
                    refinement
                        .direct_certification_offpolicy_probability);
        json += ",\"reforge_work\":" +
                std::to_string(
                    refinement.direct_certification_reforge_work);
        json += ",\"artifact_bytes\":" +
                std::to_string(
                    refinement.direct_certification_artifact_bytes);
        json += ",\"peak_owned_bytes\":" +
                std::to_string(
                    refinement.direct_certification_peak_owned_bytes);
        json += ",\"route_defaults\":{\"paired_default_only\":" +
                std::string(bool_json(
                    refinement.direct_paired_default_only));
        json += ",\"certification\":{\"mode\":";
        if (refinement.direct_certification_route_default_mode.empty()) {
            json += "null";
        } else {
            append_telemetry_json_string(
                json,
                refinement.direct_certification_route_default_mode);
        }
        json += ",\"edges\":" +
                std::to_string(
                    refinement
                        .direct_certification_route_default_edges) +
                "},\"product\":{\"mode\":";
        if (refinement.direct_product_route_default_mode.empty()) {
            json += "null";
        } else {
            append_telemetry_json_string(
                json, refinement.direct_product_route_default_mode);
        }
        json += ",\"edges\":" +
                std::to_string(
                    refinement.direct_product_route_default_edges) +
                "}}";
        json += ",\"executable\":" +
                std::string(bool_json(
                    refinement.direct_certification_executable));
        json += ",\"proper\":" +
                std::string(bool_json(
                    refinement.direct_certification_proper));
        json += ",\"cost_complete\":" +
                std::string(bool_json(
                    refinement.direct_certification_cost_complete));
        json += ",\"zero_off_policy\":" +
                std::string(bool_json(
                    refinement.direct_certification_zero_off_policy));
        json += ",\"cost_reconciled\":" +
                std::string(bool_json(
                    refinement.direct_certification_cost_reconciled));
        json += ",\"candidate_retained\":" +
                std::string(bool_json(
                    refinement.direct_candidate_retained)) + "}";
        json += ",\"strict_lift\":{\"status\":";
        append_telemetry_json_string(
            json, refinement.strict_lift_status);
        json += ",\"failure_reason\":";
        if (refinement.strict_lift_failure_reason.empty()) {
            json += "null";
        } else {
            append_telemetry_json_string(
                json, refinement.strict_lift_failure_reason);
        }
        json += ",\"resource_cap\":";
        if (refinement.strict_lift_resource_cap.empty()) {
            json += "null";
        } else {
            append_telemetry_json_string(
                json, refinement.strict_lift_resource_cap);
        }
        json += ",\"global_lower_bound_closed\":" +
                std::string(bool_json(
                    refinement.strict_global_lower_bound_closed)) +
                "}";
        json += ",\"publication\":{\"status\":";
        append_telemetry_json_string(
            json, refinement.publication_status);
        json += ",\"candidate_kind\":";
        if (refinement.published_candidate_kind.empty()) {
            json += "null";
        } else {
            append_telemetry_json_string(
                json, refinement.published_candidate_kind);
        }
        json += "}";
        json += ",\"pre_restore_selected_policy\":{";
        json += "\"present\":" +
                std::string(bool_json(
                    refinement.pre_restore_policy_present));
        json += ",\"materializable\":" +
                std::string(bool_json(
                    refinement.pre_restore_policy_materializable));
        json += ",\"numerical_stop\":" +
                std::string(bool_json(
                    refinement.pre_restore_policy_numerical_stop));
        json += ",\"start_value\":" +
                telemetry_finite_json(
                    refinement.pre_restore_policy_start_value);
        json += ",\"residual\":" +
                telemetry_finite_json(
                    refinement.pre_restore_policy_residual);
        json += ",\"policy_bits_hash\":";
        append_telemetry_json_string(
            json,
            telemetry_hex_u64(
                refinement.pre_restore_policy_bits_hash));
        json += ",\"selected_rows\":" +
                std::to_string(
                    refinement.pre_restore_policy_selected_rows);
        json += ",\"reachable_states\":" +
                std::to_string(
                    refinement.pre_restore_policy_reachable_states);
        json += ",\"reachable_rows\":" +
                std::to_string(
                    refinement.pre_restore_policy_reachable_rows);
        json += ",\"distinct_actions\":" +
                std::to_string(
                    refinement.pre_restore_policy_distinct_actions);
        json += ",\"choice_groups\":" +
                std::to_string(
                    refinement.pre_restore_policy_choice_groups);
        json += ",\"choice_options\":" +
                std::to_string(
                    refinement.pre_restore_policy_choice_options);
        json += ",\"goal_identity\":";
        append_telemetry_json_string(
            json,
            telemetry_hex_u64(
                refinement.pre_restore_policy_goal_identity));
        json += ",\"economy_identity\":";
        append_telemetry_json_string(
            json,
            telemetry_hex_u64(
                refinement.pre_restore_policy_economy_identity));
        json += ",\"action_vocabulary_identity\":";
        append_telemetry_json_string(
            json,
            telemetry_hex_u64(
                refinement
                    .pre_restore_policy_action_vocabulary_identity));
        json += ",\"graph_identity\":";
        append_telemetry_json_string(
            json,
            telemetry_hex_u64(
                refinement.pre_restore_policy_graph_identity));
        json += ",\"artifact_identity\":";
        append_telemetry_json_string(
            json,
            telemetry_hex_u64(
                refinement.pre_restore_policy_artifact_identity));
        json += ",\"source_generation\":" +
                std::to_string(
                    refinement.pre_restore_policy_source_generation);
        json += ",\"target_generation\":" +
                std::to_string(
                    refinement.pre_restore_policy_target_generation);
        json += ",\"snapshot_ns\":" +
                std::to_string(
                    refinement.pre_restore_policy_snapshot_ns);
        json += ",\"snapshot_peak_bytes\":" +
                std::to_string(
                    refinement.pre_restore_policy_snapshot_peak_bytes);
        json += ",\"strict_order_suppressed_comparisons\":" +
                std::to_string(
                    refinement.strict_order_suppressed_comparisons);
        json += ",\"suppressed_samples\":[";
        for (std::uint32_t sample_index = 0;
             sample_index <
                 refinement.strict_order_suppressed_samples_retained;
             ++sample_index) {
            if (sample_index != 0) json.push_back(',');
            const auto& sample =
                refinement.strict_order_suppressed_samples[sample_index];
            json += "{\"state\":" + std::to_string(sample.state);
            json += ",\"retained_row\":" +
                    std::to_string(sample.retained_row);
            json += ",\"preferred_row\":" +
                    std::to_string(sample.preferred_row);
            json += ",\"retained_value\":" +
                    telemetry_finite_json(sample.retained_value);
            json += ",\"preferred_value\":" +
                    telemetry_finite_json(sample.preferred_value) + "}";
        }
        json += "],\"suppressed_samples_retained\":" +
                std::to_string(
                    refinement.strict_order_suppressed_samples_retained);
        json += ",\"suppressed_samples_omitted\":" +
                std::to_string(
                    refinement.strict_order_suppressed_samples_omitted) +
                "}";
        json += ",\"selected_policy_candidate\":{";
        json += "\"capture_attempted\":" +
                std::string(bool_json(
                    refinement.selected_candidate_capture_attempted));
        json += ",\"captured\":" +
                std::string(bool_json(
                    refinement.selected_candidate_captured));
        json += ",\"memory_rejected\":" +
                std::string(bool_json(
                    refinement.selected_candidate_memory_rejected));
        json += ",\"identity_valid\":" +
                std::string(bool_json(
                    refinement.selected_candidate_identity_valid));
        json += ",\"certification_attempted\":" +
                std::string(bool_json(
                    refinement
                        .selected_candidate_certification_attempted));
        json += ",\"independently_evaluated\":" +
                std::string(bool_json(
                    refinement
                        .selected_candidate_independently_evaluated));
        json += ",\"retained\":" +
                std::string(bool_json(
                    refinement.selected_candidate_retained));
        json += ",\"estimated_cost\":" +
                telemetry_finite_json(
                    refinement.selected_candidate_estimated_cost);
        json += ",\"exact_cost\":" +
                telemetry_finite_json(
                    refinement.selected_candidate_exact_cost);
        json += ",\"owned_bytes\":" +
                std::to_string(
                    refinement.selected_candidate_owned_bytes);
        json += ",\"identity\":";
        append_telemetry_json_string(
            json,
            telemetry_hex_u64(
                refinement.selected_candidate_identity));
        json += ",\"capture_ns\":" +
                std::to_string(
                    refinement.selected_candidate_capture_ns);
        json += ",\"certification_ns\":" +
                std::to_string(
                    refinement.selected_candidate_certification_ns);
        json += ",\"status\":";
        append_telemetry_json_string(
            json, refinement.selected_candidate_status);
        json += ",\"failure_reason\":";
        if (refinement.selected_candidate_failure_reason.empty()) {
            json += "null";
        } else {
            append_telemetry_json_string(
                json,
                refinement.selected_candidate_failure_reason);
        }
        json += "}";
        json += ",\"finalization_stages_ns\":{";
        json += "\"incumbent_restore\":" +
                std::to_string(refinement.incumbent_restore_ns);
        json += ",\"extraction_materialization\":" +
                std::to_string(
                    refinement.extraction_materialization_ns);
        json += ",\"direct_certification\":" +
                std::to_string(refinement.direct_certification_ns);
        json += ",\"strict_lift_total\":" +
                std::to_string(refinement.strict_lift_total_ns);
        json += ",\"strict_carrier_discovery\":" +
                std::to_string(
                    refinement.strict_carrier_discovery_ns);
        json += ",\"strict_partition_refinement\":" +
                std::to_string(
                    refinement.strict_partition_refinement_ns);
        json += ",\"strict_policy_evaluation\":" +
                std::to_string(
                    refinement.strict_policy_evaluation_ns);
        json += ",\"strict_local_reoptimization\":" +
                std::to_string(
                    refinement.strict_local_reoptimization_ns);
        json += ",\"strategy_compilation\":" +
                std::to_string(refinement.strategy_compilation_ns);
        json += ",\"exact_graph_evaluation\":" +
                std::to_string(refinement.exact_graph_evaluation_ns) +
                "}";
        json += ",\"policy_reachable_coarse_states\":" +
                std::to_string(
                    refinement.policy_reachable_coarse_states);
        json += ",\"exact_states\":" +
                std::to_string(refinement.exact_states);
        json += ",\"retained_exact_states\":" +
                std::to_string(refinement.retained_exact_states);
        json += ",\"exact_classes\":" +
                std::to_string(refinement.exact_classes);
        json += ",\"initial_observation_classes\":" +
                std::to_string(
                    refinement.initial_observation_classes);
        json += ",\"behavior_splits\":" +
                std::to_string(refinement.behavior_splits);
        json += ",\"merged_exact_states\":" +
                std::to_string(refinement.merged_exact_states);
        json += ",\"exact_transitions\":" +
                std::to_string(refinement.exact_transitions);
        json += ",\"exact_kernels\":" +
                std::to_string(refinement.exact_kernels);
        json += ",\"exact_kernel_cache_hits\":" +
                std::to_string(refinement.exact_kernel_cache_hits);
        json += ",\"certification_work\":{\"selected\":{";
        json += "\"rows_begun\":" +
                std::to_string(refinement.selected_rows_begun);
        json += ",\"rows_completed\":" +
                std::to_string(refinement.selected_rows_completed);
        json += ",\"reforge_work\":" +
                std::to_string(refinement.selected_reforge_work);
        json += ",\"transitions\":" +
                std::to_string(refinement.selected_transitions);
        json += "},\"alternatives\":{\"rows_begun\":" +
                std::to_string(refinement.alternative_rows_begun);
        json += ",\"rows_completed\":" +
                std::to_string(refinement.alternative_rows_completed);
        json += ",\"reforge_work\":" +
                std::to_string(refinement.alternative_reforge_work);
        json += ",\"transitions\":" +
                std::to_string(refinement.alternative_transitions);
        json += "},\"reforge_resource_accounting\":";
        append_reforge_resource_accounting_json(
            json,
            refinement.strict_reforge_active_work,
            refinement.strict_reforge_logical_work_v1,
            refinement.strict_reforge_evaluator_work_v1,
            refinement.strict_reforge_evaluator_work_v2,
            refinement.strict_reforge_evaluator_work_v3,
            refinement.strict_reforge_effort,
            refinement.strict_reforge_row_samples,
            refinement.strict_reforge_row_samples_omitted);
        json += ",\"work_to_first_partition\":";
        if (refinement.work_to_first_partition.has_value()) {
            json += std::to_string(*refinement.work_to_first_partition);
        } else {
            json += "null";
        }
        json += ",\"work_to_first_executable_upper\":";
        if (refinement.work_to_first_executable_upper.has_value()) {
            json += std::to_string(
                *refinement.work_to_first_executable_upper);
        } else {
            json += "null";
        }
        json += ",\"wall_ns_to_first_partition\":";
        if (refinement.wall_ns_to_first_partition.has_value()) {
            json += std::to_string(
                *refinement.wall_ns_to_first_partition);
        } else {
            json += "null";
        }
        json += ",\"wall_ns_to_first_executable_upper\":";
        if (refinement.wall_ns_to_first_executable_upper.has_value()) {
            json += std::to_string(
                *refinement.wall_ns_to_first_executable_upper);
        } else {
            json += "null";
        }
        json +=
            ",\"exact_alternatives_materialized_before_first_upper\":" +
            std::to_string(
                refinement.alternatives_materialized_before_first_upper);
        json += ",\"alternative_obligations_created\":" +
                std::to_string(
                    refinement.alternative_obligations_created);
        json += ",\"unresolved_alternative_obligations\":" +
                std::to_string(
                    refinement.unresolved_alternative_obligations);
        json += ",\"alternative_exact_rows_avoided\":" +
                std::to_string(refinement.alternative_rows_avoided);
        json += ",\"action_accounting_complete\":" +
                std::string(bool_json(
                    refinement.action_accounting_complete));
        json += ",\"scheduling_rounds\":" +
                std::to_string(
                    refinement.alternative_scheduling_rounds);
        json += ",\"obligations_scheduled\":" +
                std::to_string(
                    refinement.alternative_obligations_scheduled);
        json += ",\"obligations_certified\":" +
                std::to_string(
                    refinement.alternative_obligations_certified);
        json += ",\"obligations_partially_evaluated\":" +
                std::to_string(
                    refinement
                        .alternative_obligations_partially_evaluated);
        json += ",\"obligations_noncompetitive\":" +
                std::to_string(
                    refinement.alternative_obligations_noncompetitive);
        json += ",\"obligations_stale\":" +
                std::to_string(
                    refinement.alternative_obligations_stale);
        json += ",\"verdict_revocations\":" +
                std::to_string(
                    refinement.alternative_verdict_revocations);
        json += ",\"obligations_resource_interrupted\":" +
                std::to_string(
                    refinement
                        .alternative_obligations_resource_interrupted);
        json += ",\"competitive_alternatives_remaining\":" +
                std::to_string(
                    refinement.competitive_alternatives_remaining);
        json += ",\"policy_improvements\":" +
                std::to_string(
                    refinement.alternative_policy_improvements);
        json += ",\"bounded_publication_retained\":" +
                std::string(bool_json(
                    refinement.bounded_publication_retained));
        json += ",\"global_lower_bound_closed\":" +
                std::string(bool_json(
                    refinement.global_lower_bound_closed));
        json += ",\"exact_alternative_envelope_closed\":" +
                std::string(bool_json(
                    refinement.exact_alternative_envelope_closed)) +
                "}";
        json += ",\"broad_row_attribution\":{\"samples\":[";
        bool first_broad_sample = true;
        for (const PolicyBroadRowAttribution& sample :
             refinement.broad_row_attribution) {
            if (!first_broad_sample) json += ',';
            first_broad_sample = false;
            const ReforgeBuildAttribution& row = sample.reforge;
            json += "{\"alternative\":" +
                    std::string(bool_json(sample.alternative));
            json += ",\"row_sequence\":" +
                    std::to_string(sample.row_sequence);
            json += ",\"source_strict_state\":" +
                    std::to_string(sample.source_strict_state);
            json += ",\"source_coarse_state\":" +
                    std::to_string(sample.source_coarse_state);
            json += ",\"source_stable_key_hash\":" +
                    std::to_string(sample.source_stable_key_hash);
            json += ",\"operator_index\":" +
                    std::to_string(sample.operator_index);
            json += ",\"planner_id\":";
            append_telemetry_json_string(json, sample.planner_id);
            json += ",\"primitive_program_action_ids\":[";
            for (std::size_t index = 0;
                 index < sample.primitive_program_action_ids.size();
                 ++index) {
                if (index != 0) json += ',';
                append_telemetry_json_string(
                    json, sample.primitive_program_action_ids[index]);
            }
            json += "],\"observation\":{\"item_feature_mask\":" +
                    std::to_string(sample.observation_item_features);
            json += ",\"modifier_tag_ids\":[";
            for (std::size_t index = 0;
                 index < sample.observation_modifier_tag_ids.size();
                 ++index) {
                if (index != 0) json += ',';
                json += std::to_string(
                    sample.observation_modifier_tag_ids[index]);
            }
            json += "],\"affix_selectors\":" +
                    std::to_string(sample.observation_affix_selectors) +
                    "}";
            json += ",\"raw_transitions\":" +
                    std::to_string(sample.raw_transitions);
            json += ",\"reforge\":{\"action_id\":";
            append_telemetry_json_string(json, row.action_id);
            json += ",\"action_index\":" +
                    std::to_string(row.action_index);
            json += ",\"preserved_base_hash\":" +
                    std::to_string(row.preserved_base_hash);
            json += ",\"goal_progress_gated\":" +
                    std::string(bool_json(row.goal_progress_gated));
            json += ",\"projected_sparse_frontier\":" +
                    std::string(bool_json(
                        row.projected_sparse_frontier));
            json += ",\"factored_terminal_accumulator\":" +
                    std::string(bool_json(
                        row.factored_terminal_accumulator));
            json += ",\"completed\":" +
                    std::string(bool_json(row.completed));
            json += ",\"forced_modifier_count\":" +
                    std::to_string(row.forced_modifier_count);
            json += ",\"pool\":{\"natural_entries\":" +
                    std::to_string(row.natural_pool_entries);
            json += ",\"natural_weight\":" +
                    std::to_string(row.natural_pool_weight);
            json += ",\"prefix_entries\":" +
                    std::to_string(row.natural_prefix_entries);
            json += ",\"prefix_weight\":" +
                    std::to_string(row.natural_prefix_weight);
            json += ",\"suffix_entries\":" +
                    std::to_string(row.natural_suffix_entries);
            json += ",\"suffix_weight\":" +
                    std::to_string(row.natural_suffix_weight);
            json += ",\"guaranteed_entries\":" +
                    std::to_string(row.guaranteed_pool_entries);
            json += ",\"guaranteed_weight\":" +
                    std::to_string(row.guaranteed_pool_weight) + "}";
            json += ",\"structure\":{\"physical_families\":" +
                    std::to_string(row.physical_families);
            json += ",\"roll_buckets\":" +
                    std::to_string(row.roll_buckets);
            json += ",\"prefix_buckets\":" +
                    std::to_string(row.prefix_buckets);
            json += ",\"suffix_buckets\":" +
                    std::to_string(row.suffix_buckets);
            json += ",\"goal_satisfied_buckets\":" +
                    std::to_string(row.goal_satisfied_buckets);
            json += ",\"goal_below_buckets\":" +
                    std::to_string(row.goal_below_buckets);
            json += ",\"junk_buckets\":" +
                    std::to_string(row.junk_buckets);
            json += ",\"raw_choice_entries\":" +
                    std::to_string(row.raw_choice_entries);
            json += ",\"exclusion_group_entries\":" +
                    std::to_string(row.exclusion_group_entries);
            json += ",\"exclusion_pair_checks\":" +
                    std::to_string(row.exclusion_pair_checks);
            json += ",\"exclusion_conflicts\":" +
                    std::to_string(row.exclusion_conflicts);
            json += ",\"projectable_physical_families\":" +
                    std::to_string(row.projectable_physical_families);
            json += ",\"projectable_family_classes\":" +
                    std::to_string(row.projectable_family_classes);
            json += ",\"projected_families_removed\":" +
                    std::to_string(row.projected_families_removed);
            json += ",\"structural_bits_hash\":" +
                    std::to_string(row.structural_bits_hash) + "}";
            json += ",\"enumeration\":{\"frontier_state_visits\":" +
                    std::to_string(row.frontier_state_visits);
            json += ",\"frontier_edges\":" +
                    std::to_string(row.frontier_edges);
            json += ",\"maximum_frontier_states\":" +
                    std::to_string(row.maximum_frontier_states);
            json += ",\"terminal_roll_states\":" +
                    std::to_string(row.terminal_roll_states);
            json += ",\"raw_identity_tree_nodes\":" +
                    std::to_string(row.raw_identity_tree_nodes);
            json += ",\"raw_identity_tree_leaves\":" +
                    std::to_string(row.raw_identity_tree_leaves);
            json += ",\"successor_commits\":" +
                    std::to_string(row.successor_commits);
            json += ",\"unique_projected_outcomes\":" +
                    std::to_string(row.unique_projected_outcomes);
            json += ",\"duplicate_projected_outcomes\":" +
                    std::to_string(row.duplicate_projected_outcomes);
            json += ",\"duplicate_projected_probability_mass\":" +
                    telemetry_finite_json(
                        row.duplicate_projected_probability_mass);
            const auto append_count_array =
                    [&](const auto& counts) {
                json += '[';
                for (std::size_t index = 0;
                     index < counts.size(); ++index) {
                    if (index != 0) json += ',';
                    json += std::to_string(counts[index]);
                }
                json += ']';
            };
            json += ",\"terminal_prefix_counts\":";
            append_count_array(row.terminal_prefix_counts);
            json += ",\"terminal_suffix_counts\":";
            append_count_array(row.terminal_suffix_counts);
            json += "}";
            const auto append_terminal_aggregate =
                [&](const ReforgeBuildAttribution::TerminalAggregate&
                        aggregate) {
                    json += "{\"branches\":" +
                            std::to_string(aggregate.branches);
                    json += ",\"canonical_commits\":" +
                            std::to_string(
                                aggregate.canonical_commits);
                    json += ",\"duplicate_canonical_commits\":" +
                            std::to_string(
                                aggregate.duplicate_canonical_commits);
                    json += ",\"duplicates_within_predecessor\":" +
                            std::to_string(
                                aggregate
                                    .duplicates_within_predecessor);
                    json += ",\"duplicates_across_predecessors\":" +
                            std::to_string(
                                aggregate
                                    .duplicates_across_predecessors);
                    json += ",\"probability_mass\":" +
                            telemetry_finite_json(
                                aggregate.probability_mass);
                    json += ",\"duplicate_probability_mass\":" +
                            telemetry_finite_json(
                                aggregate.duplicate_probability_mass) +
                            "}";
                };
            json += ",\"terminal_factorization\":{\"target_counts\":[";
            for (std::size_t target = 0;
                 target < row.terminal_target_counts.size(); ++target) {
                if (target != 0) json += ',';
                const auto& target_row =
                    row.terminal_target_counts[target];
                json += "{\"target\":" + std::to_string(target);
                json += ",\"state_contributions\":" +
                        std::to_string(target_row.state_contributions);
                json += ",\"final_modifier_branches\":" +
                        std::to_string(
                            target_row.final_modifier_branches);
                json += ",\"probability_mass\":" +
                        telemetry_finite_json(
                            target_row.probability_mass);
                json += ",\"final_modifier_probability_mass\":" +
                        telemetry_finite_json(
                            target_row
                                .final_modifier_probability_mass) +
                        "}";
            }
            json += "],\"side\":{\"prefix\":";
            append_terminal_aggregate(row.terminal_side_counts[0]);
            json += ",\"suffix\":";
            append_terminal_aggregate(row.terminal_side_counts[1]);
            json += "},\"terminal_bucket\":{\"goal_satisfied\":";
            append_terminal_aggregate(
                row.terminal_bucket_kind_counts[0]);
            json += ",\"goal_below_tier\":";
            append_terminal_aggregate(
                row.terminal_bucket_kind_counts[1]);
            json += ",\"junk\":";
            append_terminal_aggregate(
                row.terminal_bucket_kind_counts[2]);
            json += "},\"predecessor_observation\":{\"goal_satisfied\":";
            append_terminal_aggregate(
                row.terminal_predecessor_observation_counts[0]);
            json += ",\"goal_below_tier\":";
            append_terminal_aggregate(
                row.terminal_predecessor_observation_counts[1]);
            json += ",\"junk\":";
            append_terminal_aggregate(
                row.terminal_predecessor_observation_counts[2]);
            json += ",\"exclusion\":";
            append_terminal_aggregate(
                row.terminal_predecessor_observation_counts[3]);
            json += "},\"final_depth\":{\"predecessors\":" +
                    std::to_string(row.final_depth_predecessors);
            json += ",\"branches\":" +
                    std::to_string(row.final_depth_branches);
            json += ",\"canonical_commits\":" +
                    std::to_string(
                        row.final_depth_canonical_commits);
            json += ",\"unique_canonical_successors\":" +
                    std::to_string(
                        row.final_depth_unique_canonical_successors);
            json += ",\"duplicate_canonical_commits\":" +
                    std::to_string(
                        row.final_depth_duplicate_canonical_commits);
            json += ",\"duplicates_within_predecessor\":" +
                    std::to_string(
                        row.final_depth_duplicates_within_predecessor);
            json += ",\"duplicates_across_predecessors\":" +
                    std::to_string(
                        row.final_depth_duplicates_across_predecessors);
            json += ",\"duplicates_same_terminal_bucket\":" +
                    std::to_string(
                        row.final_depth_duplicates_same_terminal_bucket);
            json += ",\"duplicates_different_terminal_bucket\":" +
                    std::to_string(
                        row.final_depth_duplicates_different_terminal_bucket);
            json += ",\"duplicates_different_terminal_side\":" +
                    std::to_string(
                        row.final_depth_duplicates_different_terminal_side);
            json += ",\"duplicates_different_terminal_kind\":" +
                    std::to_string(
                        row.final_depth_duplicates_different_terminal_kind);
            json += ",\"duplicates_different_terminal_exclusion_signature\":" +
                    std::to_string(
                        row
                            .final_depth_duplicates_different_terminal_exclusion_signature);
            json += ",\"duplicates_different_predecessor_observation\":" +
                    std::to_string(
                        row
                            .final_depth_duplicates_different_predecessor_observation);
            json += ",\"duplicates_different_predecessor_availability\":" +
                    std::to_string(
                        row
                            .final_depth_duplicates_different_predecessor_availability);
            json += ",\"successors_with_multiple_predecessors\":" +
                    std::to_string(
                        row
                            .final_depth_successors_with_multiple_predecessors);
            json += ",\"predecessor_convergence_excess\":" +
                    std::to_string(
                        row.final_depth_predecessor_convergence_excess);
            json += ",\"max_predecessors_per_successor\":" +
                    std::to_string(
                        row.final_depth_max_predecessors_per_successor);
            json += ",\"predecessors_with_multiple_orders\":" +
                    std::to_string(
                        row
                            .final_depth_predecessors_with_multiple_orders);
            json += ",\"max_predecessor_order_multiplicity\":" +
                    std::to_string(
                        row
                            .final_depth_max_predecessor_order_multiplicity);
            json += ",\"predecessor_order_excess\":" +
                    std::to_string(
                        row.final_depth_predecessor_order_excess);
            json += ",\"represented_order_paths\":" +
                    std::to_string(
                        row.final_depth_represented_order_paths);
            json += ",\"terminal_order_excess\":" +
                    std::to_string(
                        row.final_depth_terminal_order_excess);
            json += ",\"physical_modifier_choices\":" +
                    std::to_string(
                        row.final_depth_physical_modifier_choices);
            json += ",\"branches_with_exclusion_observation\":" +
                    std::to_string(
                        row
                            .final_depth_branches_with_exclusion_observation);
            json += ",\"probability_mass\":" +
                    telemetry_finite_json(
                        row.final_depth_probability_mass);
            json += ",\"duplicate_probability_mass\":" +
                    telemetry_finite_json(
                        row.final_depth_duplicate_probability_mass);
            json += ",\"within_predecessor_duplicate_probability_mass\":" +
                    telemetry_finite_json(
                        row
                            .final_depth_within_predecessor_duplicate_probability_mass);
            json += ",\"across_predecessor_duplicate_probability_mass\":" +
                    telemetry_finite_json(
                        row
                            .final_depth_across_predecessor_duplicate_probability_mass);
            json += ",\"same_terminal_bucket_duplicate_probability_mass\":" +
                    telemetry_finite_json(
                        row
                            .final_depth_same_terminal_bucket_duplicate_probability_mass);
            json += ",\"different_terminal_bucket_duplicate_probability_mass\":" +
                    telemetry_finite_json(
                        row
                            .final_depth_different_terminal_bucket_duplicate_probability_mass);
            json += ",\"different_predecessor_observation_duplicate_probability_mass\":" +
                    telemetry_finite_json(
                        row
                            .final_depth_different_predecessor_observation_duplicate_probability_mass);
            json += ",\"lower_bounds\":{\"unavoidable_successor_representations\":" +
                    std::to_string(
                        row.final_depth_unique_canonical_successors);
            json += ",\"algebraically_accumulable_contributions\":" +
                    std::to_string(
                        row.final_depth_duplicate_canonical_commits);
            json += ",\"temporary_roll_order_distinctions\":" +
                    std::to_string(
                        row.final_depth_terminal_order_excess) +
                    "}}";
            json += ",\"samples\":[";
            for (std::size_t index = 0;
                 index < row.terminal_contribution_samples.size();
                 ++index) {
                if (index != 0) json += ',';
                const auto& contribution =
                    row.terminal_contribution_samples[index];
                json += "{\"predecessor_sequence\":" +
                        std::to_string(
                            contribution.predecessor_sequence);
                json += ",\"predecessor_signature_hash\":" +
                        std::to_string(
                            contribution.predecessor_signature_hash);
                json += ",\"predecessor_order_multiplicity\":" +
                        std::to_string(
                            contribution.predecessor_order_multiplicity);
                json += ",\"predecessor\":{\"sat_mask\":" +
                        std::to_string(
                            contribution.predecessor_sat_mask);
                json += ",\"below_mask\":" +
                        std::to_string(
                            contribution.predecessor_below_mask);
                json += ",\"blocked_mask\":" +
                        std::to_string(
                            contribution.predecessor_blocked_mask);
                json += ",\"prefix_picks\":" +
                        std::to_string(
                            contribution.predecessor_prefix_picks);
                json += ",\"suffix_picks\":" +
                        std::to_string(
                            contribution.predecessor_suffix_picks);
                json += ",\"availability_class\":" +
                        std::to_string(
                            contribution
                                .predecessor_availability_class) +
                        "}";
                json += ",\"target_count\":" +
                        std::to_string(contribution.target_count);
                json += ",\"terminal_bucket\":{\"index\":" +
                        std::to_string(contribution.terminal_bucket);
                json += ",\"side\":" +
                        std::to_string(contribution.terminal_side);
                json += ",\"kind\":" +
                        std::to_string(
                            contribution.terminal_bucket_kind);
                json += ",\"goal_slot\":" +
                        std::to_string(
                            contribution.terminal_goal_slot);
                json += ",\"junk_class\":" +
                        std::to_string(
                            contribution.terminal_junk_class);
                json += ",\"block_mask\":" +
                        std::to_string(
                            contribution.terminal_block_mask);
                json += ",\"multiplicity\":" +
                        std::to_string(
                            contribution
                                .terminal_bucket_multiplicity);
                json += ",\"available_family_choices\":" +
                        std::to_string(
                            contribution
                                .terminal_available_family_choices);
                json += ",\"exclusion_group_count\":" +
                        std::to_string(
                            contribution
                                .terminal_exclusion_group_count);
                json += ",\"exclusion_signature_hash\":" +
                        std::to_string(
                            contribution
                                .terminal_exclusion_signature_hash) +
                        "}";
                json += ",\"canonical_successor\":{\"state\":" +
                        std::to_string(
                            contribution.canonical_successor_state);
                json += ",\"hash\":" +
                        std::to_string(
                            contribution.canonical_successor_hash);
                json += ",\"duplicate\":" +
                        std::string(bool_json(
                            contribution
                                .duplicate_canonical_successor));
                json += ",\"duplicate_within_predecessor\":" +
                        std::string(bool_json(
                            contribution
                                .duplicate_within_predecessor));
                json += ",\"duplicate_across_predecessors\":" +
                        std::string(bool_json(
                            contribution
                                .duplicate_across_predecessors));
                json += ",\"different_terminal_bucket_from_first\":" +
                        std::string(bool_json(
                            contribution
                                .different_terminal_bucket_from_first));
                json += ",\"different_predecessor_observation_from_first\":" +
                        std::string(bool_json(
                            contribution
                                .different_predecessor_observation_from_first)) +
                        "}";
                json += ",\"probability_mass\":" +
                        telemetry_finite_json(
                            contribution.probability_mass) +
                        "}";
            }
            json += "],\"samples_omitted\":" +
                    std::to_string(
                        row.terminal_contribution_samples_omitted) +
                    "}";
            json += ",\"work\":{\"raw_choice_table\":" +
                    std::to_string(row.raw_choice_table_work);
            json += ",\"guaranteed_scan\":" +
                    std::to_string(row.guaranteed_scan_work);
            json += ",\"frontier\":" +
                    std::to_string(row.frontier_work);
            json += ",\"raw_identity_tree\":" +
                    std::to_string(row.raw_identity_tree_work);
            json += ",\"total\":" +
                    std::to_string(row.total_reforge_work);
            json += ",\"raw_equivalent_v1\":" +
                    std::to_string(
                        row.raw_equivalent_reforge_work);
            json += ",\"projected_v2\":" +
                    std::to_string(row.projected_reforge_work);
            json += ",\"factored_v3\":" +
                    std::to_string(row.factored_reforge_work);
            json += ",\"factored_terminal\":{\"predecessors\":" +
                    std::to_string(row.factored_terminal_predecessors);
            json += ",\"candidates\":" +
                    std::to_string(row.factored_terminal_candidates);
            json += ",\"canonical_subset_checks\":" +
                    std::to_string(
                        row.factored_canonical_subset_checks);
            json += ",\"last_pick_terms\":" +
                    std::to_string(row.factored_last_pick_terms);
            json += ",\"commits\":" +
                    std::to_string(row.factored_terminal_commits);
            json += ",\"retry_aggregates\":" +
                    std::to_string(row.factored_retry_aggregates);
            json += ",\"subset_cache_hits\":" +
                    std::to_string(row.factored_subset_cache_hits);
            json += ",\"subset_cache_misses\":" +
                    std::to_string(row.factored_subset_cache_misses);
            json += ",\"subset_identity_mismatches\":" +
                    std::to_string(
                        row.factored_subset_identity_mismatches) + "}}";
            json += ",\"time_ns\":{\"pool\":" +
                    std::to_string(row.pool_build_ns);
            json += ",\"bucket\":" +
                    std::to_string(row.bucket_build_ns);
            json += ",\"exclusion\":" +
                    std::to_string(row.exclusion_build_ns);
            json += ",\"frontier\":" +
                    std::to_string(row.frontier_build_ns);
            json += ",\"finalize\":" +
                    std::to_string(row.finalize_ns);
            json += ",\"total\":" +
                    std::to_string(row.total_build_ns) + "}}}";
        }
        json += "],\"omitted\":" +
                std::to_string(
                    refinement.broad_row_attribution_omitted) +
                "}";
        json += ",\"memory_bytes\":" +
                std::to_string(refinement.memory_bytes);
        json += ",\"peak_memory_bytes\":" +
                std::to_string(refinement.peak_memory_bytes);
        json += ",\"memory_limit_bytes\":" +
                std::to_string(refinement.memory_limit_bytes);
        json += ",\"retained_artifact_bytes\":" +
                std::to_string(refinement.retained_artifact_bytes);
        json += ",\"fallback_portfolio\":{";
        json += "\"candidates\":" +
                std::to_string(
                    refinement.fallback_portfolio_candidates);
        json += ",\"invalidations\":" +
                std::to_string(
                    refinement.fallback_portfolio_invalidations);
        json += ",\"compilation_failures\":" +
                std::to_string(
                    refinement.fallback_portfolio_compilation_failures);
        json += ",\"memory_rejections\":" +
                std::to_string(
                    refinement.fallback_portfolio_memory_rejections);
        json += ",\"owned_bytes\":" +
                std::to_string(
                    refinement.fallback_portfolio_owned_bytes);
        json += ",\"publication_attempts\":" +
                std::to_string(
                    refinement.fallback_publication_attempts);
        json += ",\"publication_successes\":" +
                std::to_string(
                    refinement.fallback_publication_successes);
        json += ",\"cheapest_independently_evaluated_selected\":";
        json += refinement.cheapest_independently_evaluated_selected
                    ? "true"
                    : "false";
        json += ",\"preferred_candidate_upper\":" +
                telemetry_finite_json(
                    refinement.preferred_candidate_upper);
        json += ",\"published_upper\":" +
                telemetry_finite_json(
                    refinement.published_fallback_upper);
        json += ",\"published_evaluated_cost\":" +
                telemetry_finite_json(
                    refinement.published_fallback_evaluated_cost);
        char fallback_witness_hash[17];
        std::snprintf(
            fallback_witness_hash, sizeof(fallback_witness_hash),
            "%016llx",
            static_cast<unsigned long long>(
                refinement.published_fallback_witness_hash));
        json += ",\"published_witness_hash\":\"";
        json += fallback_witness_hash;
        json += "\",\"published_kind\":";
        if (refinement.published_fallback_kind.empty()) {
            json += "null";
        } else {
            append_telemetry_json_string(
                json, refinement.published_fallback_kind);
        }
        json += ",\"preferred_failure_reason\":";
        if (refinement.preferred_publication_failure_reason.empty()) {
            json += "null";
        } else {
            append_telemetry_json_string(
                json,
                refinement.preferred_publication_failure_reason);
        }
        json += '}';
        json += ",\"exact_state_reuses\":" +
                std::to_string(refinement.exact_state_reuses);
        json += ",\"collapse_events\":" +
                std::to_string(refinement.collapse_events);
        json += ",\"collapse_destroyed_feature_mask\":" +
                std::to_string(
                    refinement.collapse_destroyed_feature_mask);
        json += ",\"collapse_preserved_feature_mask\":" +
                std::to_string(
                    refinement.collapse_preserved_feature_mask);
        json += ",\"collapse_events_by_feature\":";
        append_refinement_feature_counts(
            refinement.collapse_events_by_feature);
        json += ",\"preservation_events_by_feature\":";
        append_refinement_feature_counts(
            refinement.preservation_events_by_feature);
        json += ",\"refinement_rounds\":" +
                std::to_string(refinement.refinement_rounds);
        json += ",\"backward_observation_rounds\":" +
                std::to_string(
                    refinement.backward_observation_rounds);
        json += ",\"selected_action_routing_rounds\":" +
                std::to_string(
                    refinement.selected_action_routing_rounds);
        json += ",\"observation_propagation_rounds\":" +
                std::to_string(
                    refinement.observation_propagation_rounds);
        json += ",\"partition_refinement_rounds\":" +
                std::to_string(
                    refinement.partition_refinement_rounds);
        json += ",\"local_reoptimization_rounds\":" +
                std::to_string(
                    refinement.local_reoptimization_rounds);
        json += ",\"local_state_action_rows_scheduled\":" +
                std::to_string(
                    refinement.local_state_action_rows_scheduled);
        json += ",\"local_state_action_rows_evaluated\":" +
                std::to_string(
                    refinement.local_state_action_rows_evaluated);
        json += ",\"local_reoptimizations\":" +
                std::to_string(refinement.local_reoptimizations);
        json += ",\"local_policy_changes\":" +
                std::to_string(refinement.local_policy_changes);
        json += ",\"local_value_changes\":" +
                std::to_string(refinement.local_value_changes);
        json += ",\"quotient_proof\":{\"payload_reuses\":" +
                std::to_string(refinement.proof_payload_reuses);
        json += ",\"row_reprojections\":" +
                std::to_string(refinement.row_reprojections);
        json += ",\"source_splits\":" +
                std::to_string(refinement.quotient_source_splits);
        json += ",\"target_splits\":" +
                std::to_string(refinement.quotient_target_splits);
        json += ",\"reverse_invalidations\":" +
                std::to_string(refinement.reverse_invalidations);
        json += ",\"improper_policy_repairs\":" +
                std::to_string(refinement.improper_policy_repairs);
        json += ",\"exact_carriers_replayed\":" +
                std::to_string(refinement.exact_carriers_replayed);
        json += ",\"current_live_slices\":" +
                std::to_string(refinement.current_live_slices);
        json += ",\"peak_live_slices\":" +
                std::to_string(refinement.peak_live_slices);
        json += ",\"current_live_slice_bytes\":" +
                std::to_string(refinement.current_live_slice_bytes);
        json += ",\"peak_live_slice_bytes\":" +
                std::to_string(refinement.peak_live_slice_bytes);
        json += ",\"coverage_descriptor_bytes\":" +
                std::to_string(refinement.coverage_descriptor_bytes);
        json += ",\"certificate_bytes\":" +
                std::to_string(refinement.certificate_bytes);
        json += ",\"dependency_sidecar_bytes\":" +
                std::to_string(refinement.dependency_sidecar_bytes);
        json += ",\"alternative_obligation_bytes\":" +
                std::to_string(refinement.alternative_obligation_bytes);
        json += ",\"partition_bytes\":" +
                std::to_string(refinement.partition_bytes);
        json += ",\"carrier_bytes\":" +
                std::to_string(refinement.carrier_bytes);
        json += ",\"row_kernel_bytes\":" +
                std::to_string(refinement.row_kernel_bytes);
        json += ",\"scratch_bytes\":" +
                std::to_string(refinement.scratch_bytes);
        json += ",\"total_solver_owned_bytes\":" +
                std::to_string(refinement.total_solver_owned_bytes);
        json += ",\"reference_adapter_invocations\":" +
                std::to_string(
                    refinement.reference_adapter_invocations) + "}";
        json += ",\"fixed_point\":{\"checked\":" +
                std::string(bool_json(refinement.fixed_point_checked));
        json += ",\"complete\":" +
                std::string(bool_json(refinement.fixed_point_complete));
        json += ",\"lumpability_checked\":" +
                std::string(bool_json(refinement.lumpability_checked));
        json += ",\"lumpable\":" +
                std::string(bool_json(refinement.lumpable));
        json += ",\"lumpability_checks\":" +
                std::to_string(refinement.lumpability_checks) + "}";
        json += ",\"class_policy\":{\"checked\":" +
                std::string(bool_json(refinement.class_policy_checked));
        json += ",\"proper\":" +
                std::string(bool_json(refinement.class_policy_proper)) +
                "}";
        json += ",\"compiled_assertion\":{\"checked\":" +
                std::string(
                    bool_json(refinement.compiled_assertion_checked));
        json += ",\"proper\":" +
                std::string(bool_json(refinement.compiled_policy_proper));
        json += ",\"zero_off_policy\":" +
                std::string(bool_json(refinement.zero_off_policy));
        json += ",\"cost_reconciled\":" +
                std::string(bool_json(refinement.cost_reconciled)) + "}";
        json += ",\"policy_changed\":" +
                std::string(bool_json(refinement.policy_changed));
        json += ",\"coarse_value_reconciled\":" +
                std::string(
                    bool_json(refinement.coarse_value_reconciled));
        json += ",\"counterexamples\":{\"count\":" +
                std::to_string(refinement.counterexamples);
        json += ",\"samples\":[";
        for (std::size_t i = 0;
             i < refinement.counterexample_samples.size(); ++i) {
            if (i != 0) json += ',';
            json += refinement.counterexample_samples[i];
        }
        json += "],\"retained\":" +
                std::to_string(
                    refinement.counterexample_samples.size());
        json += ",\"omitted\":" +
                std::to_string(
                    refinement.counterexample_samples_omitted);
        json += ",\"limit\":" +
                std::to_string(diagnostics->diagnostic_sample_limit) +
                "}";
        json += ",\"refusal_causes\":{\"count\":" +
                std::to_string(refinement.refusal_causes);
        json += ",\"samples\":[";
        for (std::size_t i = 0;
             i < refinement.refusal_cause_samples.size(); ++i) {
            if (i != 0) json += ',';
            append_telemetry_json_string(
                json, refinement.refusal_cause_samples[i]);
        }
        json += "],\"retained\":" +
                std::to_string(
                    refinement.refusal_cause_samples.size());
        json += ",\"omitted\":" +
                std::to_string(
                    refinement.refusal_cause_samples_omitted);
        json += ",\"limit\":" +
                std::to_string(diagnostics->diagnostic_sample_limit) +
                "}";
        const auto append_json_samples =
            [&](const char* name,
                const std::vector<std::string>& samples,
                const std::uint64_t omitted,
                const std::uint64_t retained_bytes) {
                json += ",\"" + std::string{name} +
                    "\":{\"samples\":[";
                for (std::size_t i = 0; i < samples.size(); ++i) {
                    if (i != 0) json.push_back(',');
                    json += samples[i];
                }
                json += "],\"retained\":" +
                    std::to_string(samples.size()) +
                    ",\"omitted\":" + std::to_string(omitted) +
                    ",\"retained_bytes\":" +
                    std::to_string(retained_bytes) +
                    ",\"limit\":" +
                    std::to_string(
                        diagnostics->diagnostic_sample_limit) + "}";
            };
        append_json_samples(
            "publication_candidates",
            refinement.publication_candidate_samples,
            refinement.publication_candidate_samples_omitted,
            refinement.publication_candidate_sample_bytes);
        append_json_samples(
            "structural_failures",
            refinement.structural_failure_samples,
            refinement.structural_failure_samples_omitted,
            refinement.structural_failure_sample_bytes);
        append_json_samples(
            "evaluator_memory",
            refinement.evaluator_memory_samples,
            refinement.evaluator_memory_samples_omitted,
            refinement.evaluator_memory_sample_bytes);
    }
    json += "}";

    json += ",\"action_control\":{";
    json += "\"explicit_envelope\":" + std::string(bool_json(
        calc.action_control().explicit_envelope));
    json += ",\"dependency_primitives\":" + std::to_string(
        calc.action_control().dependency_primitives);
    json += ",\"layout_primitives\":" + std::to_string(
        calc.action_control().layout_primitives);
    json += ",\"goal_relevant_pruned\":" + std::to_string(
        calc.action_control().pruned_outside_goal_relevance);
    json += ",\"product_admission\":{\"goal_filtering\":" +
            std::string(bool_json(calc.registry().product_goal_filtering));
    json += ",\"roles\":{";
    constexpr const char* product_role_names[] = {
        "candidate", "automatic_dependency", "filtered"};
    for (std::size_t role = 0; role < kProductActionRoleCount; ++role) {
        if (role != 0) json.push_back(',');
        json += '\"';
        json += product_role_names[role];
        json += "\":{\"total\":" + std::to_string(
            calc.registry().product_role_counts[role]);
        json += ",\"by_family\":{";
        for (std::size_t family = 0;
             family < kPrimitiveTelemetryFamilyCount; ++family) {
            if (family != 0) json.push_back(',');
            json += '\"';
            json += primitive_telemetry_family_name(
                static_cast<PrimitiveTelemetryFamily>(family));
            json += "\":" + std::to_string(
                calc.registry().product_role_family_counts[role][family]);
        }
        json += "}}";
    }
    json += "},\"reasons\":{";
    bool first_product_reason = true;
    for (const auto& [reason, count] :
         calc.registry().product_reason_counts) {
        if (!first_product_reason) json.push_back(',');
        first_product_reason = false;
        json += '\"';
        json += reason;
        json += "\":" + std::to_string(count);
    }
    json += "}}";
    if (diagnostics == nullptr) {
        json += ",\"preservation_rows\":null";
        json += ",\"constructive_state_certificates\":null";
    } else {
        json += ",\"preservation_rows\":{";
        json += "\"considered\":" + std::to_string(
            diagnostics->preservation_rows_considered);
        json += ",\"included\":" + std::to_string(
            diagnostics->preservation_rows_retained);
        json += ",\"pruned\":" + std::to_string(
            diagnostics->preservation_rows_pruned);
        json += ",\"certified_disposable\":" + std::to_string(
            diagnostics->certified_disposable_rows) + "}";
        json += ",\"constructive_state_certificates\":{";
        json += "\"accepted\":" + std::to_string(
            diagnostics->constructive_state_certificates);
        json += ",\"operators_pruned\":" + std::to_string(
            diagnostics->constructive_state_operators_pruned);
        json += ",\"start_upper_bound\":";
        if (std::isfinite(diagnostics->constructive_upper_bound)) {
            json += std::to_string(
                diagnostics->constructive_upper_bound);
        } else {
            json += "null";
        }
        json += ",\"first_expanded_state\":";
        json += diagnostics->constructive_upper_first_expanded_state == 0
                    ? "null"
                    : std::to_string(
                          diagnostics
                              ->constructive_upper_first_expanded_state);
        json += "}";
    }
    json += ",\"fossil_loadouts\":{";
    json += "\"possible\":" + std::to_string(
        calc.registry().fossil_loadouts_possible);
    json += ",\"generated\":" + std::to_string(
        calc.registry().fossil_loadouts_generated);
    json += ",\"deferred\":" + std::to_string(
        calc.registry().fossil_loadouts_deferred);
    json += ",\"lazy\":" + std::string(bool_json(
        calc.registry().fossil_generation_lazy));
    json += ",\"mode\":\"";
    json += calc.registry().fossil_generation_goal_relevant
                ? "goal_relevant"
                : calc.registry().fossil_generation_lazy ? "requested"
                                                         : "exhaustive";
    json += "\"}";
    json += ",\"reasons\":[";
    if (diagnostics != nullptr) {
        for (std::size_t i = 0;
             i < diagnostics->action_inclusion_reasons.size(); ++i) {
            if (i != 0) json += ',';
            json += '"';
            for (const char c : diagnostics->action_inclusion_reasons[i]) {
                if (c == '"' || c == '\\') json += '\\';
                json += c;
            }
            json += '"';
        }
    }
    json += "],\"reason_samples\":{\"limit\":";
    json += diagnostics == nullptr
                ? "null"
                : std::to_string(diagnostics->diagnostic_sample_limit);
    json += ",\"omitted\":";
    json += diagnostics == nullptr
                ? "null"
                : std::to_string(
                      diagnostics->action_inclusion_reasons_omitted);
    json += "},\"preservation_witnesses\":[";
    if (diagnostics != nullptr) {
        for (std::size_t i = 0;
             i < diagnostics->preservation_witnesses.size(); ++i) {
            if (i != 0) json.push_back(',');
            json += diagnostics->preservation_witnesses[i];
        }
    }
    json += "],\"preservation_witness_samples\":{\"retained\":";
    json += diagnostics == nullptr
                ? "null"
                : std::to_string(
                      diagnostics->preservation_witnesses.size());
    json += ",\"omitted\":";
    json += diagnostics == nullptr
                ? "null"
                : std::to_string(
                      diagnostics->preservation_witnesses_omitted);
    json += "},\"constructive_state_witnesses\":[";
    if (diagnostics != nullptr) {
        for (std::size_t i = 0;
             i < diagnostics->constructive_state_witnesses.size(); ++i) {
            if (i != 0) json.push_back(',');
            json += diagnostics->constructive_state_witnesses[i];
        }
    }
    json += "],\"constructive_state_witness_samples\":{";
    json += "\"retained\":";
    json += diagnostics == nullptr
                ? "null"
                : std::to_string(
                      diagnostics->constructive_state_witnesses.size());
    json += ",\"omitted\":";
    json += diagnostics == nullptr
                ? "null"
                : std::to_string(
                      diagnostics->constructive_state_witnesses_omitted);
    json += "},\"automatic_candidates\":{";
    json += "\"enabled\":" + std::string(bool_json(
        calc.goal().automatic_candidates));
    json += ",\"operators\":" + std::to_string(
        calc.action_control().automatic_options);
    json += ",\"dependency_primitives\":" + std::to_string(
        calc.action_control().automatic_dependency_primitives);
    if (diagnostics == nullptr) {
        json += ",\"rows\":null,\"admission_phases\":null";
        json +=
            ",\"by_kind\":null,\"product_fracture_local\":null,"
            "\"witnesses\":[]";
    } else {
        json += ",\"rows\":{\"considered\":" + std::to_string(
            diagnostics->automatic_rows_considered);
        json += ",\"eligible\":" + std::to_string(
            diagnostics->automatic_rows_eligible);
        json += ",\"rejected\":" + std::to_string(
            diagnostics->automatic_rows_rejected);
        json += ",\"collapsed\":" + std::to_string(
            diagnostics->automatic_rows_collapsed);
        json += ",\"selected\":" + std::to_string(
            diagnostics->automatic_rows_selected);
        json += ",\"deferred\":" + std::to_string(
            diagnostics->automatic_rows_deferred) + "}";
        const AutomaticAdmissionPhaseTelemetry& phases =
            diagnostics->automatic_admission_phases;
        json += ",\"admission_phases\":{\"carriers\":" +
                std::to_string(phases.carriers);
        json += ",\"work\":{\"discovered_states\":" +
                std::to_string(phases.discovered_states);
        json += ",\"state_action_rows\":" +
                std::to_string(phases.state_action_rows);
        json += ",\"transition_entries\":" +
                std::to_string(phases.transition_entries);
        json += ",\"reforge_active_work\":" +
                std::to_string(phases.reforge_active_work);
        json += ",\"reforge_logical_work_v1\":" +
                std::to_string(phases.reforge_logical_work_v1) + "}";
        json += ",\"imprint_discovery\":{\"programs_evaluated\":" +
                std::to_string(phases.imprint_programs_evaluated);
        json += ",\"programs_pruned\":" +
                std::to_string(phases.imprint_programs_pruned);
        json += ",\"distribution_dominated_programs\":" +
                std::to_string(
                    phases.imprint_distribution_dominated_programs);
        json += ",\"price_pruned_programs\":" +
                std::to_string(
                    phases.imprint_price_pruned_programs);
        json += ",\"price_bound_max_program_depth\":" +
                std::to_string(
                    phases.imprint_price_bound_max_program_depth);
        json += ",\"max_evaluated_depth\":" +
                std::to_string(
                    phases.imprint_max_evaluated_depth);
        json += ",\"max_frontier_size\":" +
                std::to_string(
                    phases.imprint_max_frontier_size);
        json += ",\"price_bound_complete_carriers\":" +
                std::to_string(
                    phases.imprint_price_bound_complete_carriers);
        json += ",\"action_state_evaluations\":" +
                std::to_string(
                    phases.imprint_action_state_evaluations);
        json += ",\"outcomes_merged\":" +
                std::to_string(phases.imprint_outcomes_merged);
        json += ",\"max_atomic_outcomes_ns\":" +
                std::to_string(
                    phases.imprint_max_atomic_outcomes_ns) + "}";
        json += ",\"synthesis_ns\":" +
                std::to_string(phases.synthesis_ns);
        json += ",\"local_context_ns\":" +
                std::to_string(phases.local_context_ns);
        json += ",\"local_planner_build_ns\":" +
                std::to_string(phases.local_planner_build_ns);
        json += ",\"local_layout_build_ns\":" +
                std::to_string(phases.local_layout_build_ns);
        json += ",\"local_ledger_init_ns\":" +
                std::to_string(phases.local_ledger_init_ns);
        json += ",\"local_context_other_ns\":" +
                std::to_string(phases.local_context_other_ns) + "}";
        json += ",\"by_kind\":{";
        for (std::size_t i = 0; i < kAutomaticTelemetryKindCount; ++i) {
            if (i != 0) json.push_back(',');
            const AutomaticTelemetryKind kind =
                static_cast<AutomaticTelemetryKind>(i);
            const AutomaticKindTelemetry& values =
                diagnostics->automatic_kind_telemetry[i];
            json += '"';
            json += automatic_telemetry_kind_name(kind);
            json += "\":{\"candidates\":" +
                    std::to_string(values.candidates);
            json += ",\"eligible\":" +
                    std::to_string(values.eligible_candidates);
            json += ",\"rejected\":" +
                    std::to_string(values.rejected_candidates);
            json += ",\"collapsed\":" +
                    std::to_string(values.collapsed_candidates);
            json += ",\"deferred\":" +
                    std::to_string(values.deferred_candidates);
            json += ",\"missing_price\":" +
                    std::to_string(values.missing_price_candidates);
            json += ",\"rejection_reasons\":{\"setup\":" +
                    std::to_string(values.setup_rejections);
            json += ",\"neutral_kernel\":" +
                    std::to_string(values.neutral_kernel_rejections);
            json += ",\"relevance\":" +
                    std::to_string(values.relevance_rejections);
            json += ",\"cleanup\":" +
                    std::to_string(values.cleanup_rejections);
            json += ",\"other\":" +
                    std::to_string(values.other_rejections) + "}";
            json += ",\"carriers\":" +
                    std::to_string(values.carriers);
            json += ",\"max_candidates_per_carrier\":" +
                    std::to_string(values.max_candidates_per_carrier);
            json += ",\"precompiled_classes\":" +
                    std::to_string(values.precompiled_classes);
            json += ",\"precompile_time_ns\":" +
                    std::to_string(values.precompile_ns);
            json += ",\"precompiled_bytes\":" +
                    std::to_string(values.precompiled_bytes);
            json += ",\"candidate_variants\":" +
                    std::to_string(values.candidate_variants);
            json += ",\"max_candidate_variants_per_carrier\":" +
                    std::to_string(
                        values.max_candidate_variants_per_carrier);
            json += ",\"effect_classes\":" +
                    std::to_string(values.effect_classes);
            json += ",\"max_effect_classes_per_carrier\":" +
                    std::to_string(values.max_effect_classes_per_carrier);
            json += ",\"collapsed_variants\":" +
                    std::to_string(values.collapsed_variants);
            json += ",\"unique_templates\":" +
                    std::to_string(values.unique_templates);
            json += ",\"template_hits\":" +
                    std::to_string(values.template_hits);
            json += ",\"max_templates_per_carrier\":" +
                    std::to_string(values.max_templates_per_carrier);
            json += ",\"rows\":" + std::to_string(values.rows);
            json += ",\"max_rows_per_carrier\":" +
                    std::to_string(values.max_rows_per_carrier);
            json += ",\"raw_outcomes\":" +
                    std::to_string(values.raw_outcomes);
            json += ",\"retained_transitions\":" +
                    std::to_string(values.retained_transitions);
            json += ",\"time_ns\":" +
                    std::to_string(values.admission_ns);
            json += ",\"kernel_evaluation_ns\":" +
                    std::to_string(values.kernel_evaluation_ns);
            json += ",\"outcome_mapping_ns\":" +
                    std::to_string(values.outcome_mapping_ns);
            json += ",\"template_matching_ns\":" +
                    std::to_string(values.template_matching_ns);
            json += ",\"protected_detail\":{\"side_evaluations\":" +
                    std::to_string(values.protected_side_evaluations);
            json += ",\"repeat_evaluations\":" +
                    std::to_string(values.protected_repeat_evaluations);
            json += ",\"retry_checks\":" +
                    std::to_string(values.protected_retry_checks);
            json += ",\"retry_certificates\":" +
                    std::to_string(values.protected_retry_certificates);
            json += ",\"retry_fallbacks\":" +
                    std::to_string(values.protected_retry_fallbacks);
            json += ",\"attempt_ns\":" +
                    std::to_string(values.protected_attempt_ns);
            json += ",\"baseline_ns\":" +
                    std::to_string(values.protected_baseline_ns);
            json += ",\"normalization_ns\":" +
                    std::to_string(values.protected_normalization_ns);
            json += ",\"finish_ns\":" +
                    std::to_string(values.protected_finish_ns) + "}";
            json += ",\"enumeration_time_ns\":" +
                    std::to_string(values.enumeration_ns);
            json += ",\"row_time_ns\":" +
                    std::to_string(values.row_ns);
            json += ",\"selected_bytes\":" +
                    std::to_string(values.selected_bytes) + "}";
        }
        json += "}";
        json += ",\"product_fracture_local\":{\"rows\":" +
                std::to_string(diagnostics->product_fracture_rows);
        json += ",\"raw_physical_outcomes\":" +
                std::to_string(
                    diagnostics->product_fracture_raw_outcomes);
        json += ",\"retained_hit_entries\":" +
                std::to_string(
                    diagnostics->product_fracture_hit_entries);
        json += ",\"retained_miss_entries\":" +
                std::to_string(
                    diagnostics->product_fracture_miss_entries);
        json += ",\"parent_miss_states_interned\":" +
                std::to_string(
                    diagnostics
                        ->product_fracture_parent_miss_states_interned);
        json += ",\"selected_policy\":{\"rows\":" +
                std::to_string(
                    diagnostics->product_fracture_selected_rows);
        json += ",\"properness_checked\":" +
                std::to_string(
                    diagnostics
                        ->product_fracture_selected_properness_checked);
        json += ",\"proper\":" +
                std::to_string(
                    diagnostics
                        ->product_fracture_selected_proper_rows);
        json += ",\"improper\":" +
                std::to_string(
                    diagnostics
                        ->product_fracture_selected_improper_rows);
        json += ",\"unproved\":" +
                std::to_string(
                    diagnostics
                        ->product_fracture_selected_unproved_rows) +
                "}";
        json += ",\"max_probability_error\":" +
                std::to_string(
                    diagnostics
                        ->product_fracture_max_probability_error);
        json += ",\"shape_rows\":[";
        bool first_shape = true;
        for (std::size_t n = 0;
             n < diagnostics->product_fracture_shape_rows.size(); ++n) {
            for (std::size_t k = 0;
                 k < diagnostics->product_fracture_shape_rows[n].size();
                 ++k) {
                const std::uint64_t rows =
                    diagnostics->product_fracture_shape_rows[n][k];
                if (rows == 0) continue;
                if (!first_shape) json.push_back(',');
                first_shape = false;
                json += "{\"n\":" + std::to_string(n) +
                        ",\"k\":" + std::to_string(k) +
                        ",\"rows\":" + std::to_string(rows) + "}";
            }
        }
        json += "]";
        json +=
            ",\"miss_route\":\"priced_restart\","
            "\"primitive_evaluator\":\"exact_unchanged\","
            "\"witnesses\":[";
        for (std::size_t i = 0;
             i < diagnostics->product_fracture_witnesses.size(); ++i) {
            if (i != 0) json.push_back(',');
            json += diagnostics->product_fracture_witnesses[i];
        }
        json += "],\"witness_samples\":{\"retained\":" +
                std::to_string(
                    diagnostics->product_fracture_witnesses.size());
        json += ",\"omitted\":" +
                std::to_string(
                    diagnostics
                        ->product_fracture_witnesses_omitted);
        json += ",\"limit\":" +
                std::to_string(diagnostics->diagnostic_sample_limit) +
                "}}";
        json += ",\"witnesses\":[";
        for (std::size_t i = 0;
             i < diagnostics->automatic_candidate_witnesses.size(); ++i) {
            if (i != 0) json.push_back(',');
            json += diagnostics->automatic_candidate_witnesses[i];
        }
        json += "],\"witness_samples\":{\"retained\":" +
                std::to_string(
                    diagnostics->automatic_candidate_witnesses.size()) +
                ",\"omitted\":" +
                std::to_string(
                    diagnostics->automatic_candidate_witnesses_omitted) +
                ",\"limit\":" +
                std::to_string(diagnostics->diagnostic_sample_limit) + "}";
    }
    json += "}}";

    json += ",\"planner\":{\"registry\":" +
            std::to_string(calc.operators().size());
    json += ",\"candidate\":" +
            std::to_string(calc.candidate_operators().size());
    json += ",\"fixed_options\":" +
            std::to_string(calc.operators().size() -
                           calc.registry().actions.size()) +
            "}";

    json += ",\"abstraction\":{\"discriminating_tags\":" +
            std::to_string(calc.layout().discriminating_tag_ids.size());
    json += ",\"junk_classes\":" +
            std::to_string(calc.layout().junk_classes.size()) + "}";

    json += ",\"exact_state_scaling\":{";
    if (diagnostics == nullptr) {
        json += "\"strict_states\":null,\"quotient_states\":null";
        json += ",\"refinement_rounds\":null";
        json += ",\"coarse_candidate_classes\":null";
        json += ",\"max_strict_states_per_coarse_class\":null";
        json += ",\"shadow_only\":null";
        json += ",\"shadow_behavioral_classes\":null";
        json += ",\"shadow_expanded_states_observed\":null";
        json += ",\"literal_duplicates\":null";
        json += ",\"exact_behavioral_merges\":null";
        json += ",\"witnessed_non_equivalences\":null";
        json += ",\"projected_successor_class_mismatches\":null";
        json += ",\"kernel_payload_reuses\":null";
        json += ",\"kernel_payload_bytes_saved\":null";
        json += ",\"observation_signature_mismatches\":null";
        json += ",\"action_observation_cardinality\":[]";
        json += ",\"witnesses\":[]";
    } else {
        json += "\"strict_states\":" +
                std::to_string(diagnostics->strict_discovered_states);
        json += ",\"quotient_states\":" +
                std::to_string(diagnostics->quotient_states);
        json += ",\"refinement_rounds\":" +
                std::to_string(diagnostics->quotient_refinement_rounds);
        json += ",\"coarse_candidate_classes\":" +
                std::to_string(diagnostics->coarse_candidate_classes);
        json += ",\"max_strict_states_per_coarse_class\":" +
                std::to_string(
                    diagnostics->max_strict_states_per_coarse_class);
        json += ",\"shadow_only\":" + std::string(
                    bool_json(diagnostics->state_scaling_shadow_only));
        json += ",\"shadow_behavioral_classes\":" +
                std::to_string(diagnostics->shadow_behavioral_classes);
        json += ",\"shadow_expanded_states_observed\":" +
                std::to_string(
                    diagnostics->shadow_expanded_states_observed);
        json += ",\"literal_duplicates\":" +
                std::to_string(diagnostics->literal_duplicate_states);
        json += ",\"exact_behavioral_merges\":" +
                std::to_string(diagnostics->exact_behavioral_merges);
        json += ",\"witnessed_non_equivalences\":" +
                std::to_string(diagnostics->witnessed_non_equivalences);
        json += ",\"projected_successor_class_mismatches\":" +
                std::to_string(
                    diagnostics->projected_successor_class_mismatches);
        json += ",\"kernel_payload_reuses\":" +
                std::to_string(diagnostics->exact_kernel_payload_reuses);
        json += ",\"kernel_payload_bytes_saved\":" +
                std::to_string(
                    diagnostics->exact_kernel_payload_bytes_saved);
        json += ",\"observation_signature_mismatches\":" +
                std::to_string(
                    diagnostics->observation_signature_mismatches);
        json += ",\"action_observation_cardinality\":[";
        for (std::size_t i = 0;
             i < diagnostics->action_observation_cardinalities.size(); ++i) {
            if (i != 0) json.push_back(',');
            json += diagnostics->action_observation_cardinalities[i];
        }
        json += "]";
        json += ",\"witnesses\":[";
        for (std::size_t i = 0;
             i < diagnostics->equivalence_witnesses.size(); ++i) {
            if (i != 0) json.push_back(',');
            json += diagnostics->equivalence_witnesses[i];
        }
        json += "],\"witness_samples\":{\"retained\":" +
                std::to_string(diagnostics->equivalence_witnesses.size()) +
                ",\"omitted\":" +
                std::to_string(
                    diagnostics->equivalence_witnesses_omitted) +
                ",\"limit\":" +
                std::to_string(diagnostics->diagnostic_sample_limit) + "}";
    }
    json += "}";

    json += ",\"states\":{";
    if (diagnostics == nullptr) {
        json += "\"discovered\":null,\"expanded\":null,\"frontier\":null";
        json += ",\"goal\":null,\"policy_reachable\":null";
    } else {
        json += "\"discovered\":" +
                std::to_string(diagnostics->discovered_states);
        json += ",\"strict_discovered\":" +
                std::to_string(diagnostics->strict_discovered_states);
        json += ",\"quotient\":" +
                std::to_string(diagnostics->quotient_states);
        json += ",\"expanded\":" +
                std::to_string(diagnostics->expanded_states);
        json += ",\"frontier\":" +
                std::to_string(diagnostics->frontier_states);
        json += ",\"goal\":" +
                std::to_string(diagnostics->goal_states);
        if (result == nullptr) {
            json += ",\"policy_reachable\":null";
        } else {
            json += ",\"policy_reachable\":" +
                    std::to_string(diagnostics->policy_reachable_states);
        }
    }
    json += "}";

    const bool focused_restricted_envelope_open =
        diagnostics != nullptr &&
        diagnostics->incremental_action_generation &&
        !diagnostics->incremental_action_envelope_closed;
    const double focused_published_lower =
        diagnostics == nullptr
            ? 0.0
            : globally_certified_action_envelope_lower_bound(
                  diagnostics->focused_lower_bound,
                  diagnostics->incremental_action_generation,
                  diagnostics->incremental_action_envelope_closed);
    const double focused_published_gap =
        diagnostics != nullptr &&
                std::isfinite(diagnostics->focused_upper_bound) &&
                std::isfinite(focused_published_lower)
            ? std::max(
                  0.0,
                  diagnostics->focused_upper_bound -
                      focused_published_lower)
            : kInfinity;
    json += ",\"focused_expansion\":{";
    if (diagnostics == nullptr) {
        json += "\"used\":null,\"rounds\":null,\"lower_bound\":null";
        json += ",\"restricted_action_envelope_lower_bound\":null";
        json += ",\"lower_bound_scope\":null";
        json += ",\"upper_bound\":null,\"partial_policy_upper_bound\":null";
        json += ",\"partial_policy_rounds\":null,\"optimality_gap\":null";
        json += ",\"restricted_action_envelope_optimality_gap\":null";
        json += ",\"duration_ns\":null,\"schedule\":null";
        json += ",\"fallback_validation\":null,\"constructive_policy\":null";
    } else {
        json += "\"used\":" + std::string(bool_json(
            diagnostics->focused_expansion));
        json += ",\"rounds\":" + std::to_string(
            diagnostics->focused_expansion_rounds);
        const auto append_bound = [&](const double value) {
            if (std::isfinite(value)) {
                char buffer[40];
                std::snprintf(buffer, sizeof(buffer), "%.17g", value);
                json += buffer;
            } else {
                json += "null";
            }
        };
        json += ",\"lower_bound\":";
        append_bound(focused_published_lower);
        json += ",\"restricted_action_envelope_lower_bound\":";
        append_bound(diagnostics->focused_lower_bound);
        json += ",\"lower_bound_scope\":\"";
        json += focused_restricted_envelope_open
                    ? "independent_global_floor"
                    : "closed_action_envelope";
        json += "\"";
        json += ",\"upper_bound\":";
        append_bound(diagnostics->focused_upper_bound);
        json += ",\"partial_policy_upper_bound\":";
        append_bound(
            diagnostics->focused_partial_policy_upper_bound);
        json += ",\"partial_policy_rounds\":" + std::to_string(
            diagnostics->focused_partial_policy_rounds);
        json += ",\"optimality_gap\":";
        append_bound(focused_published_gap);
        json += ",\"restricted_action_envelope_optimality_gap\":";
        append_bound(diagnostics->focused_optimality_gap);
        json += ",\"exact_gap_proof_tolerance\":";
        append_bound(diagnostics->focused_exact_gap_proof_tolerance);
        json += ",\"incumbent\":{";
        json += "\"kind\":";
        if (diagnostics->incumbent_kind.empty()) {
            json += "null";
        } else {
            append_telemetry_json_string(
                json, diagnostics->incumbent_kind);
        }
        json += ",\"round\":" +
                std::to_string(diagnostics->incumbent_round);
        json += ",\"restart_state\":";
        json += diagnostics->incumbent_restart_state == kNoId
                    ? "null"
                    : std::to_string(
                          diagnostics->incumbent_restart_state);
        json += ",\"anchor_state\":";
        json += diagnostics->incumbent_anchor_state == kNoId
                    ? "null"
                    : std::to_string(
                          diagnostics->incumbent_anchor_state);
        const auto append_identity = [&](const char* name,
                                         const std::uint64_t value) {
            char buffer[17];
            std::snprintf(
                buffer, sizeof(buffer), "%016llx",
                static_cast<unsigned long long>(value));
            json += ",\"";
            json += name;
            json += "\":\"";
            json += buffer;
            json += '"';
        };
        append_identity(
            "goal_identity", diagnostics->incumbent_goal_identity);
        append_identity(
            "economy_identity", diagnostics->incumbent_economy_identity);
        append_identity(
            "action_vocabulary_identity",
            diagnostics->incumbent_action_vocabulary_identity);
        append_identity(
            "graph_identity", diagnostics->incumbent_graph_identity);
        json += ",\"strict_state_provenance\":" + std::string(bool_json(
            diagnostics->incumbent_strict_state_provenance));
        json += "}";
        json += ",\"duration_ns\":" + std::to_string(
            diagnostics->focused_expansion_ns);
        FocusedScheduleRoundTelemetry schedule_totals;
        for (const FocusedScheduleRoundTelemetry& round :
             diagnostics->focused_schedule_rounds) {
            schedule_totals.lower_candidates += round.lower_candidates;
            schedule_totals.upper_candidates += round.upper_candidates;
            schedule_totals.lower_quota_admissions +=
                round.lower_quota_admissions;
            schedule_totals.upper_quota_admissions +=
                round.upper_quota_admissions;
            schedule_totals.lower_fill_admissions +=
                round.lower_fill_admissions;
            schedule_totals.upper_fill_admissions +=
                round.upper_fill_admissions;
            schedule_totals.schedule_candidates +=
                round.schedule_candidates;
            schedule_totals.schedule_admissions +=
                round.schedule_admissions;
            schedule_totals.global_batch_cap_hits +=
                round.global_batch_cap_hits;
            schedule_totals.per_class_cap_hits +=
                round.per_class_cap_hits;
        }
        json += ",\"schedule\":{\"counting_contract\":{";
        json += "\"lower_upper_candidates\":\"bound_walk_fringe_before_union\"";
        json += ",\"quota_admissions\":\"unique_bound_members_admitted_by_reserved_quota\"";
        json += ",\"fill_admissions\":\"unique_bound_members_admitted_from_unused_quota\"";
        json += ",\"schedule_candidates\":\"unique_candidates_before_schedule_caps\"";
        json += ",\"schedule_admissions\":\"states_selected_after_schedule_caps\"";
        json += ",\"cap_hits\":\"encountered_global_stops_or_per_class_rejections\"}";
        json += ",\"totals\":{\"lower_candidates\":" +
                std::to_string(schedule_totals.lower_candidates);
        json += ",\"upper_candidates\":" +
                std::to_string(schedule_totals.upper_candidates);
        json += ",\"lower_quota_admissions\":" +
                std::to_string(schedule_totals.lower_quota_admissions);
        json += ",\"upper_quota_admissions\":" +
                std::to_string(schedule_totals.upper_quota_admissions);
        json += ",\"lower_fill_admissions\":" +
                std::to_string(schedule_totals.lower_fill_admissions);
        json += ",\"upper_fill_admissions\":" +
                std::to_string(schedule_totals.upper_fill_admissions);
        json += ",\"schedule_candidates\":" +
                std::to_string(schedule_totals.schedule_candidates);
        json += ",\"schedule_admissions\":" +
                std::to_string(schedule_totals.schedule_admissions);
        json += ",\"global_batch_cap_hits\":" +
                std::to_string(schedule_totals.global_batch_cap_hits);
        json += ",\"per_class_cap_hits\":" +
                std::to_string(schedule_totals.per_class_cap_hits);
        json += "},\"by_round\":[";
        for (std::size_t i = 0;
             i < diagnostics->focused_schedule_rounds.size(); ++i) {
            if (i != 0) json.push_back(',');
            const FocusedScheduleRoundTelemetry& round =
                diagnostics->focused_schedule_rounds[i];
            json += "{\"round\":" + std::to_string(round.round);
            json += ",\"lower_candidates\":" +
                    std::to_string(round.lower_candidates);
            json += ",\"upper_candidates\":" +
                    std::to_string(round.upper_candidates);
            json += ",\"batch_states\":" +
                    std::to_string(round.batch_states);
            json += ",\"lower_quota\":" +
                    std::to_string(round.lower_quota);
            json += ",\"upper_quota\":" +
                    std::to_string(round.upper_quota);
            json += ",\"lower_quota_admissions\":" +
                    std::to_string(round.lower_quota_admissions);
            json += ",\"upper_quota_admissions\":" +
                    std::to_string(round.upper_quota_admissions);
            json += ",\"lower_fill_admissions\":" +
                    std::to_string(round.lower_fill_admissions);
            json += ",\"upper_fill_admissions\":" +
                    std::to_string(round.upper_fill_admissions);
            json += ",\"schedule_candidates\":" +
                    std::to_string(round.schedule_candidates);
            json += ",\"schedule_admissions\":" +
                    std::to_string(round.schedule_admissions);
            json += ",\"global_batch_cap_hits\":" +
                    std::to_string(round.global_batch_cap_hits);
            json += ",\"per_class_cap_hits\":" +
                    std::to_string(round.per_class_cap_hits) + "}";
        }
        json += "]}";
        const FallbackValidationTelemetry& fallback_validation =
            diagnostics->fallback_validation;
        const std::uint64_t fallback_component_ns =
            fallback_validation.goal_identity.duration_ns +
            fallback_validation.economy_identity.duration_ns +
            fallback_validation.action_vocabulary_identity.duration_ns +
            fallback_validation.successful_proof_identity.duration_ns +
            fallback_validation.structural.duration_ns +
            fallback_validation.anchor_properness.duration_ns +
            fallback_validation.start_properness.duration_ns;
        const std::uint64_t fallback_unattributed_ns =
            fallback_validation.total_ns > fallback_component_ns
                ? fallback_validation.total_ns - fallback_component_ns
                : 0;
        json += ",\"fallback_validation\":{\"timing_contract\":{";
        json += "\"total\":\"inclusive_parent\"";
        json += ",\"components\":\"mutually_exclusive_leaves\"";
        json += ",\"unattributed\":\"total_minus_component_sum\"}";
        json += ",\"calls\":" +
                std::to_string(fallback_validation.calls);
        json += ",\"duration_ns\":" +
                std::to_string(fallback_validation.total_ns);
        json += ",\"unattributed_ns\":" +
                std::to_string(fallback_unattributed_ns);
        json += ",\"successful_proof_cache\":{\"version\":" +
                std::to_string(fallback_validation.proof_version);
        json += ",\"checks\":" +
                std::to_string(
                    fallback_validation.successful_proof_cache_checks);
        json += ",\"hits\":" +
                std::to_string(
                    fallback_validation.successful_proof_cache_hits);
        json += ",\"misses\":" +
                std::to_string(
                    fallback_validation.successful_proof_cache_misses);
        json += ",\"last_miss_reason\":";
        if (fallback_validation.successful_proof_last_miss_reason.empty()) {
            json += "null";
        } else {
            append_telemetry_json_string(
                json,
                fallback_validation.successful_proof_last_miss_reason);
        }
        json += "}";
        json += ",\"components\":{";
        const auto append_fallback_component =
            [&](const char* name,
                const FallbackValidationTelemetry::Component& component,
                const bool first) {
                if (!first) json.push_back(',');
                json += '"';
                json += name;
                json += "\":{\"checks\":" +
                        std::to_string(component.checks);
                json += ",\"duration_ns\":" +
                        std::to_string(component.duration_ns) + "}";
            };
        append_fallback_component(
            "goal_identity", fallback_validation.goal_identity, true);
        append_fallback_component(
            "economy_identity", fallback_validation.economy_identity,
            false);
        append_fallback_component(
            "action_vocabulary_identity",
            fallback_validation.action_vocabulary_identity, false);
        append_fallback_component(
            "successful_proof_identity",
            fallback_validation.successful_proof_identity, false);
        append_fallback_component(
            "structural", fallback_validation.structural, false);
        append_fallback_component(
            "anchor_properness",
            fallback_validation.anchor_properness, false);
        append_fallback_component(
            "start_properness",
            fallback_validation.start_properness, false);
        json += "}}";
        json += ",\"constructive_policy\":{\"syntheses\":" +
                std::to_string(
                    diagnostics->constructive_policy_syntheses);
        json += ",\"reuses\":" + std::to_string(
            diagnostics->constructive_policy_reuses);
        json += ",\"refreshes\":" + std::to_string(
            diagnostics->constructive_policy_refreshes);
        json += ",\"last_refresh_reason\":";
        if (diagnostics->constructive_policy_last_refresh_reason.empty()) {
            json += "null";
        } else {
            append_telemetry_json_string(
                json,
                diagnostics->constructive_policy_last_refresh_reason);
        }
        json += ",\"anchor_checks\":" +
                std::to_string(
                    diagnostics->constructive_policy_anchor_checks);
        json += ",\"anchor_eligible\":" + std::to_string(
            diagnostics->constructive_policy_anchor_eligible);
        json += ",\"renewal_variants\":" + std::to_string(
            diagnostics->constructive_policy_renewal_variants);
        json += ",\"exit_checks\":" + std::to_string(
            diagnostics->constructive_policy_exit_checks);
        json += ",\"finishable_exits\":" + std::to_string(
            diagnostics->constructive_policy_finishable_exits);
        json += ",\"feasible_policies\":" + std::to_string(
            diagnostics->constructive_policy_feasible_policies);
        json += ",\"bootstrap\":{\"action\":";
        if (diagnostics->destructive_renewal_action_id.empty()) {
            json += "null";
        } else {
            append_telemetry_json_string(
                json, diagnostics->destructive_renewal_action_id);
        }
        json += ",\"renewal_value\":";
        append_bound(diagnostics->destructive_renewal_value);
        json += ",\"anchor_value\":";
        append_bound(diagnostics->destructive_renewal_anchor_value);
        json += ",\"start_value\":";
        append_bound(diagnostics->destructive_renewal_start_value);
        json += "}";
        json += ",\"gated_root_renewal\":{\"candidates\":" +
                std::to_string(
                    diagnostics->gated_root_renewal_candidates);
        json += ",\"rejections\":" + std::to_string(
            diagnostics->gated_root_renewal_rejections);
        json += ",\"validated_non_goal_states\":" +
                std::to_string(
                    diagnostics
                        ->gated_root_renewal_validated_non_goal_states);
        json += ",\"success_probability\":";
        append_bound(
            diagnostics->gated_root_renewal_success_probability);
        char renewal_witness_hash[17];
        std::snprintf(
            renewal_witness_hash, sizeof(renewal_witness_hash),
            "%016llx",
            static_cast<unsigned long long>(
                diagnostics->gated_root_renewal_witness_hash));
        json += ",\"witness_hash\":\"";
        json += renewal_witness_hash;
        json += "\"}";
        json += ",\"progressive_fracture\":{\"roll_action\":";
        if (diagnostics->progressive_fracture_roll_action_id.empty()) {
            json += "null";
        } else {
            append_telemetry_json_string(
                json,
                diagnostics->progressive_fracture_roll_action_id);
        }
        json += ",\"status\":";
        if (diagnostics->progressive_fracture_status.empty()) {
            json += "null";
        } else {
            append_telemetry_json_string(
                json, diagnostics->progressive_fracture_status);
        }
        json += ",\"renewal_value\":";
        append_bound(diagnostics->progressive_fracture_value);
        json += ",\"anchor_value\":";
        append_bound(diagnostics->progressive_fracture_anchor_value);
        json += ",\"start_value\":";
        append_bound(diagnostics->progressive_fracture_start_value);
        json += ",\"class_mask\":" + std::to_string(
            diagnostics->progressive_fracture_class_mask);
        json += ",\"class_mod_count\":" + std::to_string(
            diagnostics->progressive_fracture_class_mod_count);
        json += ",\"class_probability\":";
        append_bound(
            diagnostics->progressive_fracture_class_probability);
        json += ",\"post_modes\":" + std::to_string(
            diagnostics->progressive_fracture_post_modes) + "}}";
    }
    json += "}";

    json += ",\"work\":{\"state_action_rows\":" +
            std::to_string(diagnostics == nullptr
                               ? cache.state_action_rows
                               : diagnostics->sparse_rows);
    json += ",\"transition_entries\":" +
            std::to_string(diagnostics == nullptr
                               ? cache.transition_entries
                               : diagnostics->sparse_transitions);
    json += ",\"outcome_entries\":" +
            std::to_string(cache.outcome_entries);
    json += ",\"choice_groups\":" +
            std::to_string(cache.choice_groups);
    json += ",\"choice_successor_entries\":" +
            std::to_string(cache.choice_successor_entries);
    json += ",\"reforge_work\":" +
            std::to_string(
                diagnostics == nullptr
                    ? cache.reforge_logical_work_v1
                    : diagnostics->reforge_logical_work_v1);
    json += ",\"legacy_reforge_active_work\":" +
            std::to_string(
                diagnostics == nullptr
                    ? cache.reforge_frontier_work
                    : diagnostics->reforge_frontier_work);
    if (diagnostics == nullptr) {
        json += ",\"bellman_backups\":null";
        json += ",\"bellman_action_evaluations\":null";
        json += ",\"extraction_action_evaluations\":null";
        json += ",\"bellman_work_units\":null";
        json += ",\"max_bellman_unit_transitions\":null";
        json += ",\"algebraic_self_loops\":null";
    } else {
        json += ",\"bellman_backups\":" +
                std::to_string(diagnostics->bellman_backups);
        json += ",\"bellman_action_evaluations\":" +
                std::to_string(diagnostics->bellman_action_evaluations);
        json += ",\"extraction_action_evaluations\":" +
                std::to_string(diagnostics->extraction_action_evaluations);
        json += ",\"bellman_work_units\":" +
                std::to_string(diagnostics->bellman_work_units);
        json += ",\"max_bellman_unit_transitions\":" +
                std::to_string(
                    diagnostics->max_bellman_unit_transitions);
        json += ",\"algebraic_self_loops\":" +
                std::to_string(diagnostics->algebraic_self_loops);
        char hash_buffer[17];
        std::snprintf(
            hash_buffer, sizeof(hash_buffer), "%016llx",
            static_cast<unsigned long long>(
                diagnostics->transition_bits_hash));
        json += ",\"transition_bits_hash\":\"";
        json += hash_buffer;
        json += '"';
        std::snprintf(
            hash_buffer, sizeof(hash_buffer), "%016llx",
            static_cast<unsigned long long>(
                diagnostics->policy_bits_hash));
        json += ",\"policy_bits_hash\":\"";
        json += hash_buffer;
        json += '"';
    }
    json += "}";

    json += ",\"cache\":{\"distribution\":{\"requests\":" +
            std::to_string(cache.distribution_requests);
    json += ",\"hits\":" + std::to_string(cache.distribution_hits);
    json += ",\"misses\":" + std::to_string(cache.distribution_misses);
    json += ",\"entries\":" +
            std::to_string(calc.cached_distribution_count());
    json += ",\"build_ns\":" +
            std::to_string(cache.distribution_build_ns);
    json += ",\"released_after_sparse_copy\":" +
            std::string(bool_json(diagnostics != nullptr)) + "}";
    json += ",\"transition_graph\":{\"reused\":";
    json += diagnostics == nullptr
                ? "null"
                : bool_json(diagnostics->transition_cache_reused);
    json += "}";
    json += ",\"reforge\":{\"requests\":" +
            std::to_string(cache.reforge_requests);
    json += ",\"hits\":" + std::to_string(cache.reforge_hits);
    json += ",\"misses\":" + std::to_string(cache.reforge_misses);
    json += ",\"entries\":" +
            std::to_string(calc.cached_reforge_count());
    json += ",\"build_ns\":" +
            std::to_string(cache.reforge_build_ns);
    json += ",\"frontier_work\":" +
            std::to_string(cache.reforge_frontier_work);
    json += ",\"logical_work_v1\":" +
            std::to_string(cache.reforge_logical_work_v1);
    json += ",\"raw_equivalent_work_v1\":" +
            std::to_string(cache.reforge_raw_equivalent_work);
    json += ",\"projected_work_v2\":" +
            std::to_string(cache.reforge_projected_work);
    json += ",\"factored_work_v3\":" +
            std::to_string(cache.reforge_factored_work);
    json += ",\"resource_accounting\":";
    append_reforge_resource_accounting_json(
        json,
        cache.reforge_frontier_work,
        cache.reforge_logical_work_v1,
        cache.reforge_raw_equivalent_work,
        cache.reforge_projected_work,
        cache.reforge_factored_work,
        cache.reforge_effort,
        cache.reforge_row_samples,
        cache.reforge_row_samples_omitted);
    json += ",\"goal_progress_gated\":{\"rows\":" +
            std::to_string(cache.gated_reforge_rows);
    char gated_number[40];
    std::snprintf(
        gated_number, sizeof(gated_number), "%.17g",
        cache.gated_terminal_probability);
    json += ",\"terminal_probability\":" +
            std::string(gated_number);
    std::snprintf(
        gated_number, sizeof(gated_number), "%.17g",
        cache.gated_retry_probability);
    json += ",\"retry_probability\":" +
            std::string(gated_number);
    std::snprintf(
        gated_number, sizeof(gated_number), "%.17g",
        cache.gated_partial_probability);
    json += ",\"partial_probability\":" +
            std::string(gated_number);
    std::snprintf(
        gated_number, sizeof(gated_number), "%.17g",
        cache.gated_terminal_probability +
            cache.gated_retry_probability +
            cache.gated_partial_probability);
    json += ",\"mass_sum\":" + std::string(gated_number);
    json += ",\"terminal_short_circuits\":" +
            std::to_string(cache.gated_terminal_short_circuits);
    json += ",\"retry_short_circuits\":" +
            std::to_string(cache.gated_retry_short_circuits);
    json += ",\"partial_states\":" +
            std::to_string(cache.gated_partial_states);
    char gated_hash[17];
    std::snprintf(
        gated_hash, sizeof(gated_hash), "%016llx",
        static_cast<unsigned long long>(
            cache.gated_first_kernel_bits_hash));
    json += ",\"first_kernel_bits_hash\":\"";
    json += gated_hash;
    json += "\"}}";
    json += ",\"primitive_families\":{";
    for (std::size_t i = 0; i < kPrimitiveTelemetryFamilyCount; ++i) {
        if (i != 0) json.push_back(',');
        const PrimitiveTelemetryFamily family =
            static_cast<PrimitiveTelemetryFamily>(i);
        const PrimitiveFamilyTelemetry& values =
            cache.primitive_families[i];
        json += '"';
        json += primitive_telemetry_family_name(family);
        json += "\":{\"requests\":" + std::to_string(values.requests);
        json += ",\"cache_hits\":" +
                std::to_string(values.cache_hits);
        json += ",\"rows\":" + std::to_string(values.rows);
        json += ",\"raw_outcomes\":" +
                std::to_string(values.raw_outcomes);
        json += ",\"transitions\":" +
                std::to_string(values.transitions);
        json += ",\"time_ns\":" + std::to_string(values.build_ns);
        json += ",\"row_time_ns\":" +
                std::to_string(values.row_ns);
        json += ",\"selected_bytes\":" +
                std::to_string(values.selected_bytes) + "}";
    }
    json += "}}";

    json += ",\"incremental_action_envelope\":";
    if (diagnostics == nullptr) {
        json += "null";
    } else {
        json += "{\"enabled\":" +
                std::string(bool_json(
                    diagnostics->incremental_action_generation));
        json += ",\"closed\":" +
                std::string(bool_json(
                    diagnostics->incremental_action_envelope_closed));
        json += ",\"actions\":{\"admitted\":" +
                std::to_string(
                    diagnostics->incremental_actions_admitted);
        json += ",\"evaluated_non_improving\":" +
                std::to_string(
                    diagnostics->incremental_actions_non_improving);
        json += ",\"unevaluated\":" +
                std::to_string(
                    diagnostics->incremental_actions_unevaluated);
        json += ",\"evaluating\":" +
                std::to_string(
                    diagnostics->incremental_actions_evaluating);
        json += ",\"unresolved\":" +
                std::to_string(
                    diagnostics->incremental_actions_unresolved);
        json += ",\"inapplicable\":" +
                std::to_string(
                    diagnostics->incremental_actions_inapplicable) + "}";
        json += ",\"kernels\":{\"unique_evaluations\":" +
                std::to_string(
                    diagnostics->incremental_unique_kernel_evaluations);
        json += ",\"carrier_reuses\":" +
                std::to_string(
                    diagnostics->incremental_carrier_kernel_reuses) + "}";
        json += ",\"states_outside_chaos_support\":" +
                std::to_string(
                    diagnostics
                        ->incremental_states_outside_chaos_support);
        json += ",\"bellman_reoptimizations\":" +
                std::to_string(
                    diagnostics->incremental_bellman_reoptimizations);
        json += ",\"first_alternative_expanded_states\":" +
                std::to_string(
                    diagnostics
                        ->incremental_first_alternative_expanded_states);
        json += ",\"q_refinement\":{\"rounds\":" +
                std::to_string(
                    diagnostics->incremental_refinement_rounds);
        json += ",\"states_selected\":" +
                std::to_string(
                    diagnostics
                        ->incremental_refinement_states_selected);
        json += ",\"rows_reconsidered\":" +
                std::to_string(
                    diagnostics->incremental_rows_reconsidered);
        json += ",\"upper_policy_updates\":" +
                std::to_string(
                    diagnostics->incremental_upper_policy_updates);
        json += ",\"selected_uncertainty\":" +
                (std::isfinite(
                     diagnostics
                         ->incremental_refinement_uncertainty)
                     ? std::to_string(
                           diagnostics
                               ->incremental_refinement_uncertainty)
                     : std::string("null"));
        json += ",\"completed_rows_recomputed\":0}";
        json += ",\"upper_policy_passes\":{\"requested\":" +
                std::to_string(
                    diagnostics
                        ->incremental_upper_policy_passes_requested);
        json += ",\"started\":" +
                std::to_string(
                    diagnostics
                        ->incremental_upper_policy_passes_started);
        json += ",\"proper\":" +
                std::to_string(
                    diagnostics
                        ->incremental_upper_policy_passes_proper);
        json += ",\"rejected\":" +
                std::to_string(
                    diagnostics
                        ->incremental_upper_policy_passes_rejected);
        json += ",\"last_failure\":";
        append_telemetry_json_string(
            json,
            diagnostics->incremental_upper_policy_last_failure);
        json += "}";
        json += ",\"remaining_action_envelope\":" +
                std::to_string(
                    diagnostics->incremental_actions_unevaluated +
                    diagnostics->incremental_actions_evaluating +
                    diagnostics->incremental_actions_unresolved);
        json += ",\"witnesses\":[";
        for (std::size_t i = 0;
             i < diagnostics->incremental_action_witnesses.size(); ++i) {
            if (i != 0) json.push_back(',');
            json += diagnostics->incremental_action_witnesses[i];
        }
        json += "],\"witness_samples\":{\"retained\":" +
                std::to_string(
                    diagnostics->incremental_action_witnesses.size());
        json += ",\"omitted\":" +
                std::to_string(
                    diagnostics
                        ->incremental_action_witnesses_omitted);
        json += ",\"limit\":" +
                std::to_string(diagnostics->diagnostic_sample_limit) +
                "}";
        json += ",\"upper_policy_provenance\":{\"observational\":true";
        json += ",\"samples\":[";
        for (std::size_t i = 0;
             i < diagnostics->upper_policy_provenance_samples.size(); ++i) {
            if (i != 0) json.push_back(',');
            json += diagnostics->upper_policy_provenance_samples[i];
        }
        json += "],\"sample_counts\":{\"candidates\":" +
                std::to_string(
                    diagnostics
                        ->upper_policy_provenance_candidate_count);
        json += ",\"retained\":" +
                std::to_string(
                    diagnostics
                        ->upper_policy_provenance_samples.size());
        json += ",\"omitted\":" +
                std::to_string(
                    diagnostics
                        ->upper_policy_provenance_samples_omitted);
        json += ",\"limit\":" +
                std::to_string(diagnostics->diagnostic_sample_limit) +
                "}";
        json += ",\"retained_bytes\":" +
                std::to_string(
                    diagnostics
                        ->upper_policy_provenance_retained_bytes);
        json += ",\"telemetry_json_byte_limit\":" +
                std::to_string(
                    diagnostics->telemetry_json_byte_limit);
        json += "}";
        if (!diagnostics
                 ->upper_cap_zero_progress_audit_json.empty()) {
            json += ",\"upper_cap_zero_progress_audit\":";
            json += diagnostics
                        ->upper_cap_zero_progress_audit_json;
        }
        json += "}";
    }

    json += ",\"action_analysis\":{\"semantics\":{"
            "\"search_cost_is_observational\":true,"
            "\"non_use_is_pruning_certificate\":false},"
            "\"search_cost\":[";
    if (diagnostics != nullptr) {
        std::size_t action_index = 0;
        for (const auto& [action_id, cost] :
             diagnostics->action_search_costs) {
            if (action_index++ != 0) json.push_back(',');
            json += "{\"action_id\":";
            append_telemetry_json_string(json, action_id);
            json += ",\"rows\":" + std::to_string(cost.rows);
            json += ",\"raw_outcomes\":" +
                    std::to_string(cost.raw_outcomes);
            json += ",\"retained_transitions\":" +
                    std::to_string(cost.retained_transitions);
            json += ",\"reforge_work\":" +
                    std::to_string(cost.reforge_work);
            json += ",\"cache_requests\":" +
                    std::to_string(cost.cache_requests);
            json += ",\"cache_hits\":" +
                    std::to_string(cost.cache_hits);
            json += ",\"wall_ns\":" + std::to_string(cost.wall_ns);
            json += ",\"retained_bytes\":" +
                    std::to_string(cost.retained_bytes);
            json += ",\"root\":{\"rows\":" +
                    std::to_string(cost.root_rows);
            json += ",\"raw_outcomes\":" +
                    std::to_string(cost.root_raw_outcomes);
            json += ",\"retained_transitions\":" +
                    std::to_string(cost.root_retained_transitions) + "}";
            json += ",\"interrupted\":";
            if (cost.interrupted_rows == 0) {
                json += "null";
            } else {
                json += "{\"rows\":" +
                        std::to_string(cost.interrupted_rows);
                json += ",\"state\":" +
                        std::to_string(cost.last_interrupted_state);
                json += ",\"root\":" +
                        std::string(
                            cost.last_interrupted_root ? "true" : "false");
                json += ",\"operator\":" +
                        std::to_string(cost.last_interrupted_operator);
                json += ",\"cursor\":" +
                        std::to_string(cost.last_interrupted_cursor);
                json += ",\"cap\":";
                append_telemetry_json_string(
                    json, cost.last_interrupted_cap);
                json += "}";
            }
            json += "}";
        }
    }
    json += "],\"lower_upper_policy\":{\"basis\":"
            "\"selected_abstract_states_not_occupancy\",\"actions\":[";
    if (diagnostics != nullptr) {
        std::set<std::string> policy_actions;
        for (const auto& [id, unused] :
             diagnostics->lower_policy_action_states) {
            (void)unused;
            policy_actions.insert(id);
        }
        for (const auto& [id, unused] :
             diagnostics->upper_policy_action_states) {
            (void)unused;
            policy_actions.insert(id);
        }
        std::size_t policy_index = 0;
        for (const std::string& id : policy_actions) {
            if (policy_index++ != 0) json.push_back(',');
            const auto lower =
                diagnostics->lower_policy_action_states.find(id);
            const auto upper =
                diagnostics->upper_policy_action_states.find(id);
            const std::uint64_t lower_states =
                lower == diagnostics->lower_policy_action_states.end()
                    ? 0
                    : lower->second;
            const std::uint64_t upper_states =
                upper == diagnostics->upper_policy_action_states.end()
                    ? 0
                    : upper->second;
            json += "{\"action_id\":";
            append_telemetry_json_string(json, id);
            json += ",\"lower_selected_states\":" +
                    std::to_string(lower_states);
            json += ",\"upper_selected_states\":" +
                    std::to_string(upper_states);
            json += ",\"delta_upper_minus_lower\":" +
                    std::to_string(
                        static_cast<std::int64_t>(upper_states) -
                        static_cast<std::int64_t>(lower_states)) + "}";
        }
    }
    json += "]}}";

    json += ",\"optimization\":{";
    json += "\"method\":\"";
    json += diagnostics != nullptr && diagnostics->policy_iteration_fallback
                ? "policy_iteration_scc_with_prioritized_fallback"
                : "policy_iteration_scc";
    json += "\"";
    json += ",\"policy_evaluation_calls\":" + std::to_string(
        diagnostics == nullptr ? 0 : diagnostics->policy_evaluation_calls);
    json += ",\"largest_policy_component\":" + std::to_string(
        diagnostics == nullptr ? 0 : diagnostics->largest_policy_component);
    json += ",\"sparse_policy_iterations\":" + std::to_string(
        diagnostics == nullptr ? 0 : diagnostics->sparse_policy_iterations);
    json += ",\"max_sparse_policy_iterations\":" + std::to_string(
        diagnostics == nullptr ? 0
                               : diagnostics->max_sparse_policy_iterations);
    json += ",\"policy_kernel_groups\":" + std::to_string(
        diagnostics == nullptr ? 0 : diagnostics->policy_kernel_groups);
    json += ",\"policy_states_collapsed\":" + std::to_string(
        diagnostics == nullptr ? 0 : diagnostics->policy_states_collapsed);
    json += ",\"policy_evaluation_failure\":";
    if (diagnostics == nullptr ||
        diagnostics->policy_evaluation_failure.empty()) {
        json += "null";
    } else {
        json += "\"" + diagnostics->policy_evaluation_failure + "\"";
    }
    if (result == nullptr && snapshot == nullptr) {
        json += ",\"status\":\"not_run\",\"converged\":null";
        json += ",\"sweeps\":null,\"policy_improvement_rounds\":null";
        json += ",\"residual\":null,\"optimality_gap\":null";
        json += ",\"state_cap_hit\":null";
        json += ",\"resource_cap_hit\":null,\"cap_hits\":[]";
        json += ",\"full_request_status\":\"not_run\"";
    } else if (result == nullptr) {
        json += ",\"status\":\"";
        json += snapshot->abandoned ? "abandoned" : "in_progress";
        json += "\",\"converged\":null";
        json += ",\"sweeps\":" + std::to_string(diagnostics->sweeps);
        json += ",\"policy_improvement_rounds\":" + std::to_string(
                    diagnostics->policy_improvement_rounds);
        json += ",\"residual\":" +
                telemetry_finite_json(diagnostics->residual);
        json += ",\"optimality_gap\":";
        if (diagnostics->focused_expansion &&
            std::isfinite(focused_published_gap)) {
            char buffer[40];
            std::snprintf(
                buffer, sizeof(buffer), "%.17g",
                focused_published_gap);
            json += buffer;
        } else {
            json += "null";
        }
        json += ",\"state_cap_hit\":" +
                std::string(bool_json(diagnostics->state_cap_hit));
        json += ",\"resource_cap_hit\":" +
                std::string(bool_json(diagnostics->resource_cap_hit));
        json += ",\"cap_hits\":[";
        for (std::size_t i = 0; i < diagnostics->cap_hits.size(); ++i) {
            if (i != 0) json += ',';
            json += "\"" + diagnostics->cap_hits[i] + "\"";
        }
        json += "]";
        json += ",\"full_request_status\":\"incomplete_not_finished\"";
    } else {
        const char* status =
            result->policy_status == SolvePolicyStatus::BoundedNearOptimal
                ? "bounded_near_optimal"
                : result->policy_status == SolvePolicyStatus::BoundedFeasible
                      ? "bounded_feasible"
                      : result->converged
                            ? (qualified_action_subset
                                   ? "exact_supported_priced_subset"
                                   : "exact_abstract")
                            : (diagnostics->state_cap_hit
                                   ? "incomplete_state_cap"
                                   : (diagnostics->resource_cap_hit
                                          ? "incomplete_resource_cap"
                                          : "not_converged"));
        json += ",\"status\":\"" + std::string(status) + "\"";
        json += ",\"converged\":" +
                std::string(bool_json(result->converged));
        json += ",\"sweeps\":" + std::to_string(diagnostics->sweeps);
        json += ",\"policy_improvement_rounds\":" + std::to_string(
                    diagnostics->policy_improvement_rounds);
        json += ",\"residual\":" +
                telemetry_finite_json(diagnostics->residual);
        json += ",\"optimality_gap\":";
        if (diagnostics->focused_expansion &&
            std::isfinite(focused_published_gap)) {
            char buffer[40];
            std::snprintf(
                buffer, sizeof(buffer), "%.17g",
                focused_published_gap);
            json += buffer;
        } else {
            json += "null";
        }
        json += ",\"state_cap_hit\":" +
                std::string(bool_json(diagnostics->state_cap_hit));
        json += ",\"resource_cap_hit\":" +
                std::string(bool_json(diagnostics->resource_cap_hit));
        json += ",\"cap_hits\":[";
        for (std::size_t i = 0; i < diagnostics->cap_hits.size(); ++i) {
            if (i != 0) json += ',';
            json += "\"" + diagnostics->cap_hits[i] + "\"";
        }
        json += "]";
        json += ",\"full_request_status\":\"";
        if (result->termination == SolveTermination::TargetGap) {
            json += "target_gap";
        } else if (diagnostics->state_cap_hit) {
            json += "incomplete_state_cap";
        } else if (diagnostics->resource_cap_hit) {
            json += "incomplete_resource_cap";
        } else if (!result->converged) {
            json += "incomplete_solve";
        } else if (qualified_action_subset) {
            json += "incomplete_action_subset";
        } else {
            json += "exact_abstract_within_tolerance";
        }
        json += "\"";
    }
    json += "}";

    json += ",\"policy_result\":";
    if (result == nullptr) {
        json += "null";
    } else {
        const auto append_result_number = [&](const double value) {
            if (!std::isfinite(value)) {
                json += "null";
                return;
            }
            char buffer[40];
            std::snprintf(buffer, sizeof(buffer), "%.17g", value);
            json += buffer;
        };
        json += "{\"available\":" +
                std::string(bool_json(result->policy_available));
        json += ",\"status\":\"";
        json += policy_status_name(result->policy_status);
        json += "\",\"termination\":\"";
        json += termination_name(result->termination);
        json += "\",\"lower_bound\":";
        append_result_number(result->lower_bound);
        json += ",\"global_lower_bound_certified\":" +
                std::string(bool_json(
                    result->global_lower_bound_certified));
        json += ",\"lower_bound_provenance\":\"";
        switch (result->lower_bound_provenance) {
        case SolveLowerBoundProvenance::None:
            json += "none";
            break;
        case SolveLowerBoundProvenance::
                OpenIncrementalEnvelopeUniversalZero:
            json += "open_incremental_envelope_universal_zero";
            break;
        case SolveLowerBoundProvenance::
                UnclosedStrictRefinementUniversalZero:
            json += "unclosed_strict_refinement_universal_zero";
            break;
        case SolveLowerBoundProvenance::
                ClosedIncrementalActionEnvelope:
            json += "closed_incremental_action_envelope";
            break;
        case SolveLowerBoundProvenance::GlobalActionRelaxation:
            json += "global_action_relaxation";
            break;
        case SolveLowerBoundProvenance::ExactPolicyClosure:
            json += "exact_policy_closure";
            break;
        }
        json += "\"";
        json += ",\"upper_bound\":";
        append_result_number(result->upper_bound);
        json += ",\"evaluated_policy_cost\":";
        append_result_number(result->evaluated_policy_cost);
        json += ",\"absolute_gap\":";
        append_result_number(result->absolute_optimality_gap);
        json += ",\"relative_gap\":";
        append_result_number(result->relative_optimality_gap);
        json += ",\"requested_absolute_gap\":";
        append_result_number(result->requested_absolute_optimality_gap);
        json += ",\"requested_relative_gap\":";
        append_result_number(result->requested_relative_optimality_gap);
        json += ",\"target_met\":" +
                std::string(bool_json(result->target_met));
        json += ",\"target_fired\":\"";
        json += gap_target_name(result->target_fired);
        json += "\"}";
    }

    json += ",\"timings_ns\":{\"registry_generation\":" +
            optional_count(registry_generation_ns);
    json += ",\"abstract_layout\":" +
            std::to_string(calc.layout_build_ns());
    if (diagnostics == nullptr) {
        json += ",\"solve_setup\":null,\"expansion\":null";
        json += ",\"expansion_phases\":null";
        json += ",\"transition_calculation\":null,\"optimization\":null";
        json += ",\"extraction\":null";
    } else {
        json += ",\"solve_setup\":" +
                std::to_string(diagnostics->solve_setup_ns);
        json += ",\"expansion\":" +
                std::to_string(diagnostics->expansion_ns);
        const std::uint64_t expansion_attributed_ns =
            diagnostics->expansion_prepare_ns +
            diagnostics->expansion_kernel_ns +
            diagnostics->expansion_sparse_row_ns +
            diagnostics->expansion_row_byte_audit_ns +
            diagnostics->expansion_diagnostics_ns +
            diagnostics->expansion_release_ns +
            diagnostics->expansion_cap_byte_audit_ns +
            diagnostics->expansion_finalize_ns;
        const std::uint64_t expansion_unattributed_ns =
            diagnostics->expansion_ns > expansion_attributed_ns
                ? diagnostics->expansion_ns - expansion_attributed_ns
                : 0;
        json += ",\"expansion_phases\":{";
        json += "\"prepare\":" +
                std::to_string(diagnostics->expansion_prepare_ns);
        json += ",\"prepare_detail\":{";
        json += "\"byte_audit\":" + std::to_string(
            diagnostics->expansion_prepare_byte_audit_ns);
        json += ",\"automatic_admission\":" + std::to_string(
            diagnostics->expansion_prepare_admission_ns);
        json += ",\"diagnostics\":" + std::to_string(
            diagnostics->expansion_prepare_diagnostics_ns);
        json += ",\"operator_pricing\":" + std::to_string(
            diagnostics->expansion_prepare_pricing_ns) + "}";
        json += ",\"kernel\":" +
                std::to_string(diagnostics->expansion_kernel_ns);
        json += ",\"sparse_row\":" +
                std::to_string(diagnostics->expansion_sparse_row_ns);
        json += ",\"row_byte_audit\":" + std::to_string(
            diagnostics->expansion_row_byte_audit_ns);
        json += ",\"diagnostics\":" +
                std::to_string(diagnostics->expansion_diagnostics_ns);
        json += ",\"cache_release\":" +
                std::to_string(diagnostics->expansion_release_ns);
        json += ",\"periodic_cap_byte_audit\":" + std::to_string(
            diagnostics->expansion_cap_byte_audit_ns);
        json += ",\"finalize\":" +
                std::to_string(diagnostics->expansion_finalize_ns);
        json += ",\"finalize_byte_audit\":" + std::to_string(
            diagnostics->expansion_finalize_byte_audit_ns);
        json += ",\"attributed\":" +
                std::to_string(expansion_attributed_ns);
        json += ",\"unattributed\":" +
                std::to_string(expansion_unattributed_ns) + "}";
        json += ",\"transition_calculation\":" +
                std::to_string(cache.distribution_build_ns);
        json += ",\"optimization\":" +
                std::to_string(diagnostics->optimization_ns);
        json += ",\"constructive_policy\":" +
                std::to_string(diagnostics->constructive_policy_ns);
        json += ",\"strict_clean_goal_cover\":" +
                std::to_string(diagnostics->strict_clean_goal_cover_ns);
        if (result == nullptr) {
            json += ",\"extraction\":null";
        } else {
            json += ",\"extraction\":" +
                    std::to_string(diagnostics->extraction_ns);
        }
    }
    json += ",\"compilation\":" +
            count_or_null(compilation == nullptr ? nullptr
                                                 : &compilation->duration_ns);
    json += ",\"verification\":null}";

    json += ",\"diagnostic_cost\":{";
    json += "\"calc_owned_byte_audit_requests\":" +
            std::to_string(cache.owned_byte_audit_requests);
    json += ",\"calc_owned_byte_audit_ns\":" +
            std::to_string(cache.owned_byte_audit_ns);
    json += ",\"calc_owned_byte_ledger_requests\":" +
            std::to_string(cache.owned_byte_ledger_requests);
    json += ",\"calc_owned_byte_ledger_ns\":" +
            std::to_string(cache.owned_byte_ledger_ns);
    json += ",\"calc_owned_byte_ledger_child_context_visits\":" +
            std::to_string(
                cache.owned_byte_ledger_child_context_visits);
    json += ",\"calc_owned_byte_ledger_max_recursion_depth\":" +
            std::to_string(
                cache.owned_byte_ledger_max_recursion_depth);
    json += ",\"calc_owned_byte_reconciliations\":" +
            std::to_string(cache.owned_byte_reconciliations);
    json += ",\"calc_owned_byte_ledger_max_overestimate\":" +
            std::to_string(
                cache.owned_byte_ledger_max_overestimate);
    json += ",\"solve_owned_byte_ledger_requests\":" +
            std::to_string(
                diagnostics == nullptr
                    ? 0
                    : diagnostics->solve_owned_byte_ledger_requests);
    json += ",\"solve_owned_byte_reconciliations\":" +
            std::to_string(
                diagnostics == nullptr
                    ? 0
                    : diagnostics->solve_owned_byte_reconciliations);
    json += ",\"solve_owned_byte_ledger_max_overestimate\":" +
            std::to_string(
                diagnostics == nullptr
                    ? 0
                    : diagnostics
                          ->solve_owned_byte_ledger_max_overestimate) + "}";

    const std::uint64_t current_bytes = calc.estimated_owned_bytes();
    json += ",\"memory\":{\"solver_owned_bytes_estimate\":" +
            std::to_string(diagnostics == nullptr
                               ? current_bytes
                               : diagnostics->solver_owned_bytes_estimate);
    json += ",\"solver_live_owned_bytes_estimate\":" +
            std::to_string(diagnostics == nullptr
                               ? current_bytes
                               : diagnostics->solver_live_owned_bytes_estimate);
    json += ",\"diagnostics_retained_bytes_estimate\":" +
            std::to_string(
                diagnostics == nullptr
                    ? 0
                    : diagnostics->diagnostics_retained_bytes_estimate);
    json += ",\"telemetry_json_byte_limit\":" +
            std::to_string(
                diagnostics == nullptr
                    ? SolveOptions{}.max_telemetry_json_bytes
                    : diagnostics->telemetry_json_byte_limit);
    json += ",\"estimate_kind\":\"selected_allocations_not_process_heap\"";
    json += ",\"abstract_state_bytes\":" +
            std::to_string(sizeof(AbstractState));
    json += ",\"state_payload\":\"inline_sparse_junk_counts\"}";

    json += ",\"compilation\":{";
    if (compilation == nullptr) {
        json += "\"available\":false,\"working_states\":null";
        json += ",\"behavioral_classes\":null";
        json += ",\"policy_regions\":null";
        json += ",\"nodes\":null,\"edges\":null";
        json += ",\"strategy_json_bytes\":null";
        json += ",\"total_condition_bytes\":null";
        json += ",\"max_condition_bytes\":null";
        json += ",\"exact_state_fallbacks\":null";
        json += ",\"junk_predicates\":null";
        json += ",\"policy_route_defaults\":null";
        json += ",\"certification_policy_route_defaults\":null";
        json += ",\"paired_default_only\":null";
        json += ",\"peak_owned_bytes\":null";
        json += ",\"previously_accounted_peak_owned_bytes\":null";
        json += ",\"complete_peak_owned_bytes\":null";
        json += ",\"cap_hit\":null";
    } else {
        json += "\"available\":true,\"working_states\":" +
                std::to_string(compilation->working_states);
        json += ",\"behavioral_classes\":" +
                std::to_string(compilation->behavioral_classes);
        json += ",\"policy_regions\":" +
                std::to_string(compilation->policy_regions);
        json += ",\"nodes\":" + std::to_string(compilation->nodes);
        json += ",\"edges\":" + std::to_string(compilation->edges);
        json += ",\"strategy_json_bytes\":" +
                std::to_string(compilation->strategy_json_bytes);
        json += ",\"total_condition_bytes\":" +
                std::to_string(compilation->total_condition_bytes);
        json += ",\"max_condition_bytes\":" +
                std::to_string(compilation->max_condition_bytes);
        json += ",\"exact_state_fallbacks\":" +
                std::to_string(compilation->exact_state_fallbacks);
        json += ",\"junk_predicates\":" +
                std::to_string(compilation->junk_predicates);
        json += ",\"policy_route_defaults\":{\"mode\":";
        if (compilation->policy_route_default_mode.empty()) {
            json += "null";
        } else {
            append_telemetry_json_string(
                json, compilation->policy_route_default_mode);
        }
        json += ",\"edges\":" +
                std::to_string(
                    compilation->policy_route_default_edges);
        json += ",\"restart\":" +
                std::to_string(
                    compilation->policy_route_restart_default_edges);
        json += ",\"offpolicy\":" +
                std::to_string(
                    compilation->policy_route_offpolicy_default_edges) +
                "}";
        json += ",\"certification_policy_route_defaults\":{";
        json += "\"mode\":";
        if (compilation->certification_policy_route_default_mode.empty()) {
            json += "null";
        } else {
            append_telemetry_json_string(
                json,
                compilation->certification_policy_route_default_mode);
        }
        json += ",\"edges\":" +
                std::to_string(
                    compilation
                        ->certification_policy_route_default_edges);
        json += ",\"offpolicy\":" +
                std::to_string(
                    compilation
                        ->certification_policy_route_offpolicy_default_edges) +
                "}";
        json += ",\"paired_default_only\":";
        json += compilation->paired_default_only ? "true" : "false";
        json += ",\"peak_owned_bytes\":" +
                std::to_string(compilation->peak_owned_bytes);
        json += ",\"previously_accounted_peak_owned_bytes\":" +
                std::to_string(
                    compilation->previously_accounted_peak_owned_bytes);
        json += ",\"complete_peak_owned_bytes\":" +
                std::to_string(compilation->complete_peak_owned_bytes);
        json += ",\"cap_hit\":";
        if (compilation->cap_hit.empty()) {
            json += "null";
        } else {
            json += "\"" + compilation->cap_hit + "\"";
        }
    }
    json += "}";

    const bool has_result_bound =
        result != nullptr && result->start_state < result->values.size() &&
        std::isfinite(result->values[result->start_state]);
    const bool has_snapshot_bound =
        snapshot != nullptr && std::isfinite(snapshot->raw_start_bound);
    json += ",\"value\":{\"start\":";
    if (!has_result_bound || !result->policy_available) {
        json += "null";
    } else {
        char buffer[40];
        std::snprintf(buffer, sizeof(buffer), "%.17g",
                      result->values[result->start_state]);
        json += buffer;
    }
    json += ",\"start_status\":";
    if (result == nullptr && snapshot == nullptr) {
        json += "\"not_run\"";
    } else if (result == nullptr) {
        json += snapshot->abandoned ? "\"abandoned_before_completion\""
                                    : "\"solve_in_progress\"";
    } else if (result->converged) {
        if (result->options.goal_progress_gated_reforges) {
            json +=
                "\"exact_within_zero_progress_reroll_restriction\"";
        } else {
            json += qualified_action_subset
                        ? "\"exact_supported_priced_subset_within_tolerance\""
                        : "\"exact_abstract_within_tolerance\"";
        }
    } else if (result->policy_status ==
               SolvePolicyStatus::BoundedNearOptimal) {
        json += "\"bounded_near_optimal_certificate\"";
    } else if (result->policy_status ==
               SolvePolicyStatus::BoundedFeasible) {
        json += "\"bounded_feasible_certificate\"";
    } else {
        json += "\"unavailable_incomplete_solve\"";
    }
    json += ",\"start_scope\":";
    if (result == nullptr || !result->policy_available) {
        json += "null";
    } else if (result->options.goal_progress_gated_reforges) {
        json += "\"zero_progress_reroll_policy_restriction\"";
    } else if (!result->converged) {
        json += "\"executable_returned_policy\"";
    } else {
        json += qualified_action_subset ? "\"supported_priced_subset\""
                                        : "\"full_requested_action_set\"";
    }
    json += ",\"raw_start_bound\":";
    if (has_result_bound) {
        char buffer[40];
        std::snprintf(buffer, sizeof(buffer), "%.17g",
                      result->values[result->start_state]);
        json += buffer;
    } else if (has_snapshot_bound) {
        char buffer[40];
        std::snprintf(buffer, sizeof(buffer), "%.17g",
                      snapshot->raw_start_bound);
        json += buffer;
    } else {
        json += "null";
    }
    json += "},\"verification\":null}";
    return std::move(json).take();
}

} // namespace solver
} // namespace poecraft
