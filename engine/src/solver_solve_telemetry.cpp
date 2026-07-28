#include "solver_solve_types.hpp"

namespace poecraft {
namespace solver {

using namespace solve_detail;

namespace solve_detail {

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
           string_vector_owned_bytes(diagnostics.equivalence_witnesses) +
           diagnostics.focused_schedule_rounds.capacity() *
               sizeof(FocusedScheduleRoundTelemetry) +
           diagnostics.solution_scope.capacity() + 1 +
           diagnostics.policy_evaluation_failure.capacity() + 1 +
           diagnostics.incumbent_kind.capacity() + 1 +
           diagnostics.destructive_renewal_action_id.capacity() + 1 +
           diagnostics.progressive_fracture_roll_action_id.capacity() + 1 +
           diagnostics.progressive_fracture_status.capacity() + 1;
    for (const auto& [id, unused] : diagnostics.action_search_costs) {
        (void)unused;
        bytes += sizeof(std::pair<const std::string,
                                  SolveDiagnostics::ActionSearchCost>) +
                 id.capacity() + 1;
    }
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
    std::uint64_t bytes = sizeof(result);
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
        value.start_value_bound = kValueCeiling;
        if (!result.values.empty() &&
            result.start_state < result.values.size()) {
            value.start_value_bound = result.values[result.start_state];
        }
        if (result.diagnostics.focused_expansion) {
            value.lower_bound =
                result.diagnostics.focused_lower_bound;
            value.upper_bound = output_incumbent.has_value()
                                    ? output_incumbent->certified_upper_bound
                                    : result.diagnostics.focused_upper_bound;
        } else {
            value.lower_bound = 0.0;
            value.upper_bound = value.start_value_bound;
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
        value.reforge_work = calc.telemetry().reforge_frontier_work;
        /* Progress is queried after every bounded C/WASM step. Use the
         * conservative ledger here; cap/finalization checkpoints retain the
         * full selected-allocation audits. */
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
        if (!output_incumbent.has_value()) return 0;
        const BoundedPolicyIncumbent& incumbent = *output_incumbent;
        std::uint64_t bytes = sizeof(BoundedPolicyIncumbent) +
            incumbent.kind.capacity() + 1;
        bytes += incumbent.values.capacity() * sizeof(double);
        bytes += incumbent.policy_rows.capacity() * sizeof(std::uint64_t);
        bytes += incumbent.policy_row_costs.capacity() * sizeof(double);
        bytes += incumbent.policy.capacity() * sizeof(PolicyOperatorRef);
        bytes += incumbent.choice_sources.capacity() *
                 sizeof(BoundedPolicyIncumbent::ChoiceSource);
        for (const auto& source : incumbent.choice_sources) {
            bytes += source.choices.capacity() *
                     sizeof(OutcomeChoiceOption);
        }
        bytes += incumbent.frontier_operators.capacity() *
                 sizeof(std::uint32_t);
        bytes += incumbent.behavioral_representative_by_state.capacity() *
                 sizeof(std::uint32_t);
        bytes += incumbent.policy_reachable.capacity() *
                 sizeof(std::uint8_t);
        bytes += incumbent.primitive_renewal_witness
                     .kernel_signature.capacity() *
                 sizeof(std::uint64_t);
        bytes += incumbent.unveil_preferences.capacity() *
                 sizeof(std::vector<std::uint32_t>);
        for (const auto& preferences : incumbent.unveil_preferences) {
            bytes += preferences.capacity() * sizeof(std::uint32_t);
        }
        bytes += incumbent.option_unveil_preferences.capacity() *
                 sizeof(std::vector<ObservedUnveilPreference>);
        for (const auto& preferences :
             incumbent.option_unveil_preferences) {
            bytes += preferences.capacity() *
                     sizeof(ObservedUnveilPreference);
            for (const auto& preference : preferences) {
                bytes += preference.choices.capacity() *
                         sizeof(ObservedUnveilChoice);
            }
        }
        if (incumbent.fallback &&
            incumbent.fallback != focused_fallback_policy) {
            const FocusedFallbackPolicy& fallback = *incumbent.fallback;
            bytes += sizeof(FocusedFallbackPolicy);
            bytes += fallback.renewal_kernel_signature.capacity() *
                     sizeof(std::uint64_t);
            bytes += fallback.primitive_renewal_modes.capacity() *
                     sizeof(FocusedFallbackPolicy::PrimitiveRenewalMode);
            for (const auto& mode : fallback.primitive_renewal_modes) {
                bytes += mode.kernel_signature.capacity() *
                         sizeof(std::uint64_t);
            }
            bytes += fallback.progress_state_value.size() *
                     (sizeof(std::pair<const std::uint32_t, double>) +
                      2 * sizeof(void*));
            bytes += fallback.progress_state_operator.size() *
                     (sizeof(std::pair<const std::uint32_t, std::uint32_t>) +
                      2 * sizeof(void*));
        }
        return bytes;
    }

std::uint64_t SolveWork::Impl::fast_estimated_owned_bytes() const {
        ++owned_byte_ledger_requests;
        return fast_estimated_owned_bytes_with_calc(
            calc.fast_estimated_owned_bytes());
    }

std::uint64_t SolveWork::Impl::fast_estimated_owned_bytes_with_calc(
        const std::uint64_t calc_bytes) const {
        std::uint64_t bytes = sizeof(*this) + calc_bytes;
        bytes += prices.bucket_count() * sizeof(void*);
        bytes += prices.size() *
                 (sizeof(std::pair<const std::string, double>) +
                  2 * sizeof(void*));
        bytes += owned_prices_nested_bytes;
        bytes += operators.capacity() * sizeof(PricedOperator);
        bytes += owned_operators_nested_bytes;
        bytes += (reported_unsupported.capacity() + 7) / 8;
        bytes += static_operator_indices.capacity() * sizeof(std::uint32_t);
        bytes += expansion_operator_indices.capacity() *
                 sizeof(std::uint32_t);
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
        bytes += owned_result_nested_bytes;
        bytes += output_incumbent_owned_bytes();
        /* Diagnostic samples are strictly bounded and are not graph-sized.
         * Keep their exact current allocation in both ledger paths. */
        bytes += diagnostics_owned_bytes(result.diagnostics);
        return bytes;
    }

std::uint64_t SolveWork::Impl::estimated_owned_bytes_with_calc(
        const std::uint64_t calc_bytes) const {
        std::uint64_t bytes = sizeof(*this) + calc_bytes;
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
        bytes += expansion_operator_indices.capacity() *
                 sizeof(std::uint32_t);
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
        bytes += output_incumbent_owned_bytes();
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

    json += ",\"action_control\":{";
    json += "\"explicit_envelope\":" + std::string(bool_json(
        calc.action_control().explicit_envelope));
    json += ",\"dependency_primitives\":" + std::to_string(
        calc.action_control().dependency_primitives);
    json += ",\"goal_relevant_pruned\":" + std::to_string(
        calc.action_control().pruned_outside_goal_relevance);
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
        json += ",\"by_kind\":null,\"witnesses\":[]";
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

    json += ",\"focused_expansion\":{";
    if (diagnostics == nullptr) {
        json += "\"used\":null,\"rounds\":null,\"lower_bound\":null";
        json += ",\"upper_bound\":null,\"partial_policy_upper_bound\":null";
        json += ",\"partial_policy_rounds\":null,\"optimality_gap\":null";
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
        append_bound(diagnostics->focused_lower_bound);
        json += ",\"upper_bound\":";
        append_bound(diagnostics->focused_upper_bound);
        json += ",\"partial_policy_upper_bound\":";
        append_bound(
            diagnostics->focused_partial_policy_upper_bound);
        json += ",\"partial_policy_rounds\":" + std::to_string(
            diagnostics->focused_partial_policy_rounds);
        json += ",\"optimality_gap\":";
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
                    std::to_string(cost.retained_bytes) + "}";
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
        json += ",\"residual\":" + std::to_string(diagnostics->residual);
        json += ",\"optimality_gap\":";
        if (diagnostics->focused_expansion &&
            std::isfinite(diagnostics->focused_optimality_gap)) {
            char buffer[40];
            std::snprintf(
                buffer, sizeof(buffer), "%.17g",
                diagnostics->focused_optimality_gap);
            json += buffer;
        } else {
            json += "null";
        }
        json += ",\"state_cap_hit\":" +
                std::string(bool_json(diagnostics->state_cap_hit));
        json += ",\"resource_cap_hit\":" +
                std::string(bool_json(diagnostics->resource_cap_hit));
        json += ",\"cap_hits\":[]";
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
        json += ",\"residual\":" + std::to_string(diagnostics->residual);
        json += ",\"optimality_gap\":";
        if (diagnostics->focused_expansion &&
            std::isfinite(diagnostics->focused_optimality_gap)) {
            char buffer[40];
            std::snprintf(
                buffer, sizeof(buffer), "%.17g",
                diagnostics->focused_optimality_gap);
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
        json += ",\"policy_regions\":null";
        json += ",\"nodes\":null,\"edges\":null";
        json += ",\"strategy_json_bytes\":null";
        json += ",\"cap_hit\":null";
    } else {
        json += "\"available\":true,\"working_states\":" +
                std::to_string(compilation->working_states);
        json += ",\"policy_regions\":" +
                std::to_string(compilation->policy_regions);
        json += ",\"nodes\":" + std::to_string(compilation->nodes);
        json += ",\"edges\":" + std::to_string(compilation->edges);
        json += ",\"strategy_json_bytes\":" +
                std::to_string(compilation->strategy_json_bytes);
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
