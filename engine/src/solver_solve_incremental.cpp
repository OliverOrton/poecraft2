#include "solver_solve_types.hpp"

namespace poecraft {
namespace solver {

using namespace solve_detail;

void SolveWork::Impl::retain_incremental_carrier(
        const std::uint32_t state) {
    if (!incremental_action_generation ||
        calc.is_goal_state(calc.state(state))) {
        return;
    }
    incremental_carriers.push_back(state);
    incremental_unevaluated_actions += delayed_operator_indices.size();
}

bool SolveWork::Impl::schedule_next_incremental_alternative() {
    if (!incremental_action_generation || incremental_envelope_closed) {
        return false;
    }
    while (incremental_carrier_cursor < incremental_carriers.size()) {
        const std::uint32_t state =
            incremental_carriers[incremental_carrier_cursor];
        std::uint32_t operator_index = kNoId;
        if (incremental_operator_cursor < delayed_operator_indices.size()) {
            operator_index =
                delayed_operator_indices[incremental_operator_cursor++];
        } else if (!incremental_dynamic_prepared) {
            /*
             * State-local compound candidates are deliberately synthesized
             * only after the Chaos-anchored restricted graph has usable
             * values. prepare_state_expansion() returns anchors plus the
             * newly admitted local operators; keep only the latter.
             */
            try {
                prepare_state_expansion(state, true);
            } catch (const SolverResourceLimit&) {
                ++incremental_resource_unresolved_actions;
                throw;
            }
            incremental_dynamic_operator_indices.clear();
            for (const std::uint32_t candidate :
                 expansion_operator_indices) {
                if (std::find(
                        static_operator_indices.begin(),
                        static_operator_indices.end(),
                        candidate) == static_operator_indices.end()) {
                    incremental_dynamic_operator_indices.push_back(
                        candidate);
                }
            }
            incremental_dynamic_prepared = true;
            incremental_dynamic_operator_cursor = 0;
            incremental_unevaluated_actions +=
                incremental_dynamic_operator_indices.size();
            expansion_operator_indices.clear();
            continue;
        } else if (incremental_dynamic_operator_cursor <
                   incremental_dynamic_operator_indices.size()) {
            operator_index = incremental_dynamic_operator_indices[
                incremental_dynamic_operator_cursor++];
        } else {
            ++incremental_carrier_cursor;
            incremental_operator_cursor = 0;
            incremental_dynamic_prepared = false;
            incremental_dynamic_operator_cursor = 0;
            incremental_dynamic_operator_indices.clear();
            continue;
        }
        if (incremental_unevaluated_actions != 0) {
            --incremental_unevaluated_actions;
        }
        expansion_state = state;
        expansion_operator_indices.assign(1, operator_index);
        expansion_operator_cursor = 0;
        expansion_active = true;
        expansion_prepared = true;
        expansion_is_incremental_alternative = true;
        expansion_incremental_resource_limited = false;
        expansion_appended_row =
            std::numeric_limits<std::uint64_t>::max();
        expansion_states_outside_chaos_support = 0;
        if (incremental_first_alternative_expanded_states == 0) {
            incremental_first_alternative_expanded_states =
                expanded_count;
        }
        phase = SolvePhase::Expanding;
        return true;
    }
    return false;
}

double SolveWork::Impl::sparse_row_q_for_values(
        const std::size_t row_index,
        const std::vector<double>& values) const {
    const SparseRow& row = transition_cache->rows.at(row_index);
    if (row_index >= priced_rows.size()) return kInfinity;
    double constant = priced_rows.at(row_index).cost;
    if (!std::isfinite(constant)) return kInfinity;
    for (std::uint32_t i = 0; i < row.transition_count; ++i) {
        const std::uint64_t offset = row.transition_offset + i;
        const std::uint32_t successor =
            transition_cache->successors.at(offset);
        if (successor == row.owner_state) continue;
        if (successor >= values.size()) return kInfinity;
        const double value = values[successor];
        if (!std::isfinite(value) || value >= kValueCeiling) {
            return kInfinity;
        }
        constant += transition_cache->probabilities.at(offset) * value;
    }
    std::vector<std::pair<double, double>> self_choices;
    for (std::uint32_t i = 0; i < row.choice_count; ++i) {
        const SparseChoiceGroup& group =
            transition_cache->choices.at(row.choice_offset + i);
        double best = kInfinity;
        for (std::uint32_t s = 0; s < group.successor_count; ++s) {
            const std::uint32_t successor =
                transition_cache->choice_successors.at(
                    group.successor_offset + s);
            if (successor >= values.size()) return kInfinity;
            best = std::min(best, values[successor]);
        }
        if (group.has_self) {
            self_choices.push_back({best, group.probability});
        } else {
            if (!std::isfinite(best) || best >= kValueCeiling) {
                return kInfinity;
            }
            constant += group.probability * best;
        }
    }
    std::sort(
        self_choices.begin(), self_choices.end(),
        [](const auto& left, const auto& right) {
            return left.first < right.first;
        });
    double loop_probability = row.self_probability;
    for (const auto& [unused, probability] : self_choices) {
        (void)unused;
        loop_probability += probability;
    }
    std::size_t fixed_choices = 0;
    while (true) {
        const double denominator = 1.0 - loop_probability;
        const double value =
            denominator > 1e-15 ? constant / denominator : kInfinity;
        if (fixed_choices >= self_choices.size() ||
            value <= self_choices[fixed_choices].first + 1e-12) {
            return value;
        }
        const auto [alternate, probability] =
            self_choices[fixed_choices++];
        if (!std::isfinite(alternate) || alternate >= kValueCeiling) {
            return kInfinity;
        }
        constant += probability * alternate;
        loop_probability -= probability;
    }
}

bool SolveWork::Impl::classify_incremental_alternatives() {
    const std::vector<double>* upper_values = nullptr;
    const bool exact_restricted_values =
        optimization_converged() &&
        (focused_bound_proved || !focused_mode);
    if (exact_restricted_values) {
        upper_values = &result.values;
    } else if (output_incumbent.has_value()) {
        upper_values = &output_incumbent->values;
    }

    for (IncrementalAlternativeRow& candidate :
         incremental_alternative_rows) {
        if (candidate.status ==
            IncrementalAlternativeRow::Status::Admitted) {
            continue;
        }
        candidate.status =
            IncrementalAlternativeRow::Status::PendingValues;
        candidate.lower_q = sparse_row_q_for_values(
            candidate.row_index, result.values);
        candidate.upper_q =
            upper_values == nullptr
                ? kInfinity
                : sparse_row_q_for_values(
                      candidate.row_index, *upper_values);
        const double current_upper =
            upper_values != nullptr &&
                    candidate.state < upper_values->size()
                ? upper_values->at(candidate.state)
                : kInfinity;
        const double tolerance = value_comparison_tolerance(
            std::isfinite(current_upper) ? current_upper : 1.0);
        if (std::isfinite(candidate.upper_q) &&
            candidate.upper_q < current_upper - tolerance) {
            transition_cache->rows.at(candidate.row_index).admitted = true;
            candidate.status =
                IncrementalAlternativeRow::Status::Admitted;
            candidate.improvement_margin =
                current_upper - candidate.upper_q;
            return true;
        }
        if (std::isfinite(candidate.lower_q) &&
            std::isfinite(current_upper) &&
            candidate.lower_q >= current_upper - tolerance) {
            candidate.status =
                IncrementalAlternativeRow::Status::NonImproving;
            candidate.improvement_margin =
                current_upper - candidate.lower_q;
        } else {
            candidate.status =
                IncrementalAlternativeRow::Status::Unresolved;
        }
    }

    if (incremental_unevaluated_actions == 0) {
        auto unresolved = std::find_if(
            incremental_alternative_rows.begin(),
            incremental_alternative_rows.end(),
            [](const IncrementalAlternativeRow& candidate) {
                return candidate.status ==
                       IncrementalAlternativeRow::Status::Unresolved;
            });
        /*
         * With no resource boundary, an exact completed row that cannot yet
         * be separated by the current bracket must enter the admitted set.
         * This is conservative exact closure, not a claim that the row is
         * already improving.  The following Bellman pass decides it.
         */
        if (unresolved != incremental_alternative_rows.end() &&
            !result.diagnostics.resource_cap_hit) {
            transition_cache->rows.at(unresolved->row_index).admitted = true;
            unresolved->status =
                IncrementalAlternativeRow::Status::Admitted;
            unresolved->improvement_margin = 0.0;
            return true;
        }
        if (unresolved == incremental_alternative_rows.end() &&
            incremental_unevaluated_actions == 0) {
            incremental_envelope_closed = true;
        }
    }
    return false;
}

void SolveWork::Impl::restart_incremental_optimization() {
    ++incremental_reoptimizations;
    focused_bound_proved = false;
    focused_closure_proved = false;
    policy_initialized = false;
    policy_stable = false;
    policy_iteration_failed = false;
    backup_active = false;
    reset_policy_iteration_units();
    residual = kValueCeiling;
    if (focused_mode) {
        phase = SolvePhase::Expanding;
        begin_focused_lower_solve();
    } else {
        phase = SolvePhase::Iterating;
    }
}

void SolveWork::Impl::finalize_incremental_diagnostics() {
    SolveDiagnostics& diagnostics = result.diagnostics;
    diagnostics.incremental_action_generation =
        incremental_action_generation;
    diagnostics.incremental_action_envelope_closed =
        !incremental_action_generation || incremental_envelope_closed;
    diagnostics.incremental_actions_unevaluated =
        incremental_unevaluated_actions;
    diagnostics.incremental_actions_evaluating =
        expansion_active && expansion_is_incremental_alternative ? 1 : 0;
    diagnostics.incremental_actions_unresolved =
        incremental_resource_unresolved_actions;
    diagnostics.incremental_actions_inapplicable =
        incremental_inapplicable_actions;
    diagnostics.incremental_unique_kernel_evaluations =
        incremental_unique_kernel_evaluations;
    diagnostics.incremental_carrier_kernel_reuses =
        incremental_carrier_kernel_reuses;
    diagnostics.incremental_bellman_reoptimizations =
        incremental_reoptimizations;
    diagnostics.incremental_first_alternative_expanded_states =
        incremental_first_alternative_expanded_states;
    diagnostics.incremental_action_witnesses.clear();
    diagnostics.incremental_action_witnesses_omitted = 0;
    const bool exact_final_values =
        incremental_envelope_closed && optimization_converged() &&
        (focused_bound_proved || !focused_mode);
    for (IncrementalAlternativeRow& candidate :
         incremental_alternative_rows) {
        if (exact_final_values) {
            candidate.lower_q = sparse_row_q_for_values(
                candidate.row_index, result.values);
            candidate.upper_q = candidate.lower_q;
        } else {
            candidate.lower_q = sparse_row_q_for_values(
                candidate.row_index, result.values);
            if (output_incumbent.has_value()) {
                candidate.upper_q = sparse_row_q_for_values(
                    candidate.row_index, output_incumbent->values);
            }
        }
        const char* status = "unresolved";
        switch (candidate.status) {
        case IncrementalAlternativeRow::Status::PendingValues:
        case IncrementalAlternativeRow::Status::Unresolved:
            ++diagnostics.incremental_actions_unresolved;
            break;
        case IncrementalAlternativeRow::Status::Admitted:
            status = "admitted";
            ++diagnostics.incremental_actions_admitted;
            break;
        case IncrementalAlternativeRow::Status::NonImproving:
            status = "evaluated_non_improving";
            ++diagnostics.incremental_actions_non_improving;
            break;
        }
        diagnostics.incremental_states_outside_chaos_support +=
            candidate.states_added;
        if (diagnostics.incremental_action_witnesses.size() >=
            options.max_diagnostic_samples) {
            ++diagnostics.incremental_action_witnesses_omitted;
            continue;
        }
        std::string witness = "{\"state\":";
        witness += std::to_string(candidate.state);
        witness += ",\"action\":";
        append_json_string(
            witness,
            calc.operators().at(candidate.operator_index).id);
        witness += ",\"status\":";
        append_json_string(witness, status);
        witness += ",\"lower_q\":" + finite_json(candidate.lower_q);
        witness += ",\"upper_q\":" + finite_json(candidate.upper_q);
        witness += ",\"improvement_margin\":" +
                   finite_json(candidate.improvement_margin);
        witness += ",\"states_outside_chaos_support\":" +
                   std::to_string(candidate.states_added);
        witness += "}";
        diagnostics.incremental_action_witnesses.push_back(
            std::move(witness));
    }
}

} // namespace solver
} // namespace poecraft
