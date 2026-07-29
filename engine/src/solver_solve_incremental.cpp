#include "solver_solve_types.hpp"

namespace poecraft {
namespace solver {

using namespace solve_detail;

double q_directed_uncertainty_contribution(
        const double probability,
        const double lower,
        const double upper) {
    if (!std::isfinite(probability) || probability < 0.0 ||
        !std::isfinite(lower) || !std::isfinite(upper) ||
        upper < lower) {
        return kInfinity;
    }
    return probability * (upper - lower);
}

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
        if (successor >= values.size()) {
            if (calc.is_goal_state(calc.state(successor))) continue;
            return kInfinity;
        }
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
            if (successor >= values.size()) {
                if (calc.is_goal_state(calc.state(successor))) {
                    best = 0.0;
                    continue;
                }
                return kInfinity;
            }
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

std::vector<double>
SolveWork::Impl::certified_incremental_lower_values() {
    std::vector<double> lower = result.values;
    lower.resize(calc.state_count(), 0.0);
    for (std::uint32_t state = 0; state < lower.size(); ++state) {
        if (calc.is_goal_state(calc.state(state))) {
            lower[state] = 0.0;
            continue;
        }
        /*
         * An exact partial graph can be improper under its currently
         * admitted actions even though a delayed action or Restart gives the
         * concrete state a finite continuation. Infinity in that restricted
         * graph is therefore not a global lower bound while the action
         * envelope remains open. Replace only unavailable values with the
         * independently admissible state heuristic (or its zero fallback);
         * retain every finite Bellman lift.
         */
        if (!std::isfinite(lower[state]) ||
            lower[state] >= kValueCeiling) {
            lower[state] =
                optimistic_completion_cost_for_state(state);
        }
    }
    return lower;
}

void SolveWork::Impl::refresh_incremental_upper_incumbent() {
    if (!output_incumbent.has_value()) return;
    BoundedPolicyIncumbent& incumbent = *output_incumbent;
    const std::uint64_t no_row =
        std::numeric_limits<std::uint64_t>::max();
    const std::size_t old_size = incumbent.values.size();
    const std::size_t state_count = calc.state_count();
    if (old_size < state_count) {
        incumbent.values.resize(state_count, kInfinity);
        incumbent.policy_rows.resize(state_count, no_row);
        incumbent.frontier_operators.resize(state_count, kNoId);
        incumbent.policy.resize(state_count);
        incumbent.policy_row_costs.resize(state_count, kInfinity);
        if (!incumbent.policy_reachable.empty()) {
            incumbent.policy_reachable.resize(state_count, 0);
        }
        for (std::uint32_t state =
                 static_cast<std::uint32_t>(old_size);
             state < state_count; ++state) {
            if (calc.is_goal_state(calc.state(state))) {
                incumbent.values[state] = 0.0;
                if (!incumbent.policy_reachable.empty()) {
                    incumbent.policy_reachable[state] = 1;
                }
                continue;
            }
            if (restart_operator_index == kNoId ||
                restart_state >= incumbent.values.size() ||
                !std::isfinite(restart_cost) ||
                !std::isfinite(incumbent.values[restart_state])) {
                continue;
            }
            incumbent.values[state] =
                restart_cost + incumbent.values[restart_state];
            incumbent.frontier_operators[state] =
                restart_operator_index;
            if (!incumbent.policy_reachable.empty()) {
                incumbent.policy_reachable[state] = 1;
            }
            capture_incumbent_state(incumbent, state, no_row);
        }
    }

    std::vector<std::uint8_t> changed(state_count, 0);
    /*
     * The incumbent starts as a complete executable policy. Replacing a
     * state action only when its exact row maps the incumbent value vector
     * below the current state value preserves a Bellman super-solution.
     * Later decreases of successor values can only strengthen that witness.
     * A bounded number of deterministic Gauss-Seidel passes is therefore a
     * certified policy improvement, even when it has not reached the exact
     * value of the improved policy.
     */
    for (std::uint32_t sweep = 0; sweep < 16; ++sweep) {
        bool improved = false;
        for (const IncrementalAlternativeRow& candidate :
             incremental_alternative_rows) {
            if (candidate.status !=
                    IncrementalAlternativeRow::Status::Admitted ||
                candidate.state >= incumbent.values.size()) {
                continue;
            }
            const double q = sparse_row_q_for_values(
                candidate.row_index, incumbent.values);
            const double current = incumbent.values[candidate.state];
            const double tolerance = value_comparison_tolerance(
                std::isfinite(current) ? current : 1.0);
            if (!std::isfinite(q) ||
                q >= current - tolerance) {
                continue;
            }
            incumbent.values[candidate.state] = q;
            incumbent.policy_rows[candidate.state] =
                candidate.row_index;
            incumbent.frontier_operators[candidate.state] = kNoId;
            changed[candidate.state] = 1;
            improved = true;
            ++incremental_upper_policy_updates;
        }
        if (!improved) break;
    }
    for (std::uint32_t state = 0; state < changed.size(); ++state) {
        if (changed[state]) {
            capture_incumbent_state(
                incumbent, state, incumbent.policy_rows[state]);
        }
    }
    incumbent.certified_upper_bound =
        incumbent.values.at(result.start_state);
    incumbent.evaluated_policy_cost =
        incumbent.certified_upper_bound;
    incumbent.kind = "q_directed_incremental_policy";
    incumbent.graph_identity = graph_identity();
    result.diagnostics.incumbent_kind = incumbent.kind;
    result.diagnostics.incumbent_graph_identity =
        incumbent.graph_identity;
    result.diagnostics.focused_upper_bound =
        incumbent.certified_upper_bound;
    if (result.start_state < result.values.size()) {
        result.diagnostics.focused_optimality_gap = std::max(
            0.0,
            incumbent.certified_upper_bound -
                result.values[result.start_state]);
    }
}

bool SolveWork::Impl::schedule_incremental_refinement(
        const bool force) {
    if (!incremental_action_generation ||
        incremental_envelope_closed ||
        incremental_refinement_active) {
        return false;
    }
    const std::size_t completed =
        incremental_alternative_rows.size();
    if (!force && completed < 3) {
        return false;
    }
    const std::vector<double>& lower = result.values;
    const std::vector<double> no_upper;
    const std::vector<double>& upper =
        output_incumbent.has_value()
            ? output_incumbent->values
            : no_upper;
    const std::size_t state_count = calc.state_count();
    std::vector<double> priority(state_count, 0.0);
    const double unbounded_priority =
        std::numeric_limits<double>::max() / 16.0;
    bool has_unresolved = false;

    const auto state_width =
        [&](const std::uint32_t state) {
            if (state >= lower.size() || state >= upper.size() ||
                !std::isfinite(lower[state]) ||
                !std::isfinite(upper[state])) {
                return kInfinity;
            }
            return std::max(0.0, upper[state] - lower[state]);
        };
    for (const IncrementalAlternativeRow& candidate :
         incremental_alternative_rows) {
        if (candidate.status !=
            IncrementalAlternativeRow::Status::Unresolved) {
            continue;
        }
        has_unresolved = true;
        ++incremental_rows_reconsidered;
        const SparseRow& row =
            transition_cache->rows.at(candidate.row_index);
        const double q_width =
            std::isfinite(candidate.lower_q) &&
                    std::isfinite(candidate.upper_q)
                ? std::max(
                      0.0, candidate.upper_q - candidate.lower_q)
                : kInfinity;
        double raw_total = 0.0;
        bool raw_unbounded = false;
        for (std::uint32_t i = 0; i < row.transition_count; ++i) {
            const std::uint64_t offset = row.transition_offset + i;
            const std::uint32_t successor =
                transition_cache->successors.at(offset);
            if (successor == row.owner_state) continue;
            const double width = state_width(successor);
            if (!std::isfinite(width)) {
                raw_unbounded = true;
            } else {
                raw_total +=
                    q_directed_uncertainty_contribution(
                        transition_cache->probabilities.at(offset),
                        0.0, width);
            }
        }
        for (std::uint32_t i = 0; i < row.choice_count; ++i) {
            const SparseChoiceGroup& group =
                transition_cache->choices.at(row.choice_offset + i);
            double best_upper = kInfinity;
            for (std::uint32_t s = 0; s < group.successor_count; ++s) {
                const std::uint32_t successor =
                    transition_cache->choice_successors.at(
                        group.successor_offset + s);
                if (successor < upper.size()) {
                    best_upper = std::min(
                        best_upper, upper[successor]);
                }
            }
            for (std::uint32_t s = 0; s < group.successor_count; ++s) {
                const std::uint32_t successor =
                    transition_cache->choice_successors.at(
                        group.successor_offset + s);
                if (successor >= lower.size() ||
                    lower[successor] >
                        best_upper +
                            value_comparison_tolerance(best_upper)) {
                    continue;
                }
                const double width = state_width(successor);
                if (!std::isfinite(width)) {
                    raw_unbounded = true;
                } else {
                    raw_total +=
                        q_directed_uncertainty_contribution(
                            group.probability, 0.0, width);
                }
            }
        }
        const double scale =
            std::isfinite(q_width) && raw_total > 0.0
                ? q_width / raw_total
                : 1.0;
        const auto add =
            [&](const std::uint32_t successor,
                const double probability) {
                if (successor == row.owner_state ||
                    successor >= state_count ||
                    (successor < expanded.size() &&
                     expanded[successor])) {
                    return;
                }
                const double width = state_width(successor);
                if (!std::isfinite(width) ||
                    !std::isfinite(q_width) || raw_unbounded) {
                    priority[successor] = unbounded_priority;
                    return;
                }
                /*
                 * Exceptional-support actions must not be classified from
                 * an analytic fringe value. Their exact new states enter the
                 * ordinary lifecycle and are expanded before the action can
                 * be admitted or rejected.
                 */
                if (successor >= incremental_chaos_support.size() ||
                    !incremental_chaos_support[successor]) {
                    priority[successor] = unbounded_priority;
                    return;
                }
                priority[successor] +=
                    q_directed_uncertainty_contribution(
                        probability, 0.0, width) *
                    scale;
            };
        for (std::uint32_t i = 0; i < row.transition_count; ++i) {
            const std::uint64_t offset = row.transition_offset + i;
            add(
                transition_cache->successors.at(offset),
                transition_cache->probabilities.at(offset));
        }
        for (std::uint32_t i = 0; i < row.choice_count; ++i) {
            const SparseChoiceGroup& group =
                transition_cache->choices.at(row.choice_offset + i);
            double best_upper = kInfinity;
            for (std::uint32_t s = 0; s < group.successor_count; ++s) {
                const std::uint32_t successor =
                    transition_cache->choice_successors.at(
                        group.successor_offset + s);
                if (successor < upper.size()) {
                    best_upper = std::min(
                        best_upper, upper[successor]);
                }
            }
            for (std::uint32_t s = 0; s < group.successor_count; ++s) {
                const std::uint32_t successor =
                    transition_cache->choice_successors.at(
                        group.successor_offset + s);
                if (successor < lower.size() &&
                    lower[successor] <=
                        best_upper +
                            value_comparison_tolerance(best_upper)) {
                    add(successor, group.probability);
                }
            }
        }
    }
    if (!has_unresolved) return false;

    std::vector<std::uint32_t> ranked;
    ranked.reserve(state_count - std::min<std::size_t>(
        state_count, expanded.size()));
    for (std::uint32_t state = 0; state < state_count; ++state) {
        if (priority[state] > 0.0 &&
            (state >= expanded.size() || !expanded[state])) {
            ranked.push_back(state);
        }
    }
    /*
     * A nonlinear choice or an unavailable executable incumbent can leave
     * no scalar attribution even though the exact envelope overlaps. In
     * that case continue toward strict closure in bounded batches. This is
     * the completeness fallback after Q attribution has no candidate, not
     * the normal broad-fringe policy.
     */
    if (ranked.empty()) {
        for (std::uint32_t state = 0; state < state_count; ++state) {
            if (!calc.is_goal_state(calc.state(state)) &&
                (state >= expanded.size() || !expanded[state])) {
                priority[state] = 1.0;
                ranked.push_back(state);
            }
        }
    }
    std::stable_sort(
        ranked.begin(), ranked.end(),
        [&](const std::uint32_t left, const std::uint32_t right) {
            return priority[left] != priority[right]
                       ? priority[left] > priority[right]
                       : left < right;
        });
    const std::uint32_t remaining_capacity =
        options.max_expanded_states > expanded_count
            ? options.max_expanded_states - expanded_count
            : 0;
    const std::size_t batch = std::min<std::size_t>(
        ranked.size(),
        std::min<std::uint32_t>(
            remaining_capacity,
            std::max<std::uint32_t>(
                1024, options.focused_expansion_batch_states)));
    if (batch == 0) return false;
    ranked.resize(batch);

    std::vector<std::uint8_t> selected(state_count, 0);
    double selected_uncertainty = 0.0;
    for (const std::uint32_t state : ranked) {
        selected[state] = 1;
        selected_uncertainty = std::min(
            unbounded_priority,
            selected_uncertainty + priority[state]);
    }
    std::deque<std::uint32_t> remainder;
    for (const std::uint32_t state : queue) {
        if (state >= selected.size() || !selected[state]) {
            remainder.push_back(state);
        }
    }
    queue = std::move(remainder);
    for (auto it = ranked.rbegin(); it != ranked.rend(); ++it) {
        queue.push_front(*it);
        if (*it >= queued.size()) queued.resize(*it + 1, 0);
        queued[*it] = 1;
    }
    peak_queue_size = std::max<std::uint32_t>(
        peak_queue_size, static_cast<std::uint32_t>(queue.size()));
    incremental_refinement_active = true;
    incremental_refinement_target_expanded =
        expanded_count + static_cast<std::uint32_t>(batch);
    ++incremental_refinement_rounds;
    incremental_refinement_states_selected += batch;
    incremental_refinement_uncertainty = std::min(
        unbounded_priority,
        incremental_refinement_uncertainty +
            selected_uncertainty);
    phase = SolvePhase::Expanding;
    return true;
}

bool SolveWork::Impl::classify_incremental_alternatives() {
    const std::vector<double>* upper_values = nullptr;
    const std::vector<double> certified_lower =
        certified_incremental_lower_values();
    bool restricted_graph_closed = true;
    for (std::uint32_t state = 0; state < calc.state_count(); ++state) {
        if (!calc.is_goal_state(calc.state(state)) &&
            (state >= expanded.size() || !expanded[state])) {
            restricted_graph_closed = false;
            break;
        }
    }
    const bool exact_restricted_values =
        (restricted_graph_closed &&
         incremental_restricted_values_ready &&
         optimization_converged()) ||
        (optimization_converged() &&
         (focused_bound_proved || !focused_mode));
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
        ++incremental_rows_reconsidered;
        candidate.lower_q = sparse_row_q_for_values(
            candidate.row_index, certified_lower);
        candidate.upper_q =
            upper_values == nullptr
                ? kInfinity
                : sparse_row_q_for_values(
                      candidate.row_index, *upper_values);
        const SparseRow& row =
            transition_cache->rows.at(candidate.row_index);
        const auto unexpanded_delta =
            [&](const std::uint32_t successor) {
                return !calc.is_goal_state(calc.state(successor)) &&
                       (successor >= incremental_chaos_support.size() ||
                        !incremental_chaos_support[successor]) &&
                       (successor >= expanded.size() ||
                        !expanded[successor]);
            };
        bool pending_delta = false;
        for (std::uint32_t i = 0;
             !pending_delta && i < row.transition_count; ++i) {
            pending_delta = unexpanded_delta(
                transition_cache->successors.at(
                    row.transition_offset + i));
        }
        for (std::uint32_t i = 0;
             !pending_delta && i < row.choice_count; ++i) {
            const SparseChoiceGroup& group =
                transition_cache->choices.at(row.choice_offset + i);
            for (std::uint32_t s = 0;
                 !pending_delta && s < group.successor_count; ++s) {
                pending_delta = unexpanded_delta(
                    transition_cache->choice_successors.at(
                        group.successor_offset + s));
            }
        }
        if (pending_delta) {
            candidate.status =
                IncrementalAlternativeRow::Status::Unresolved;
            continue;
        }
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
        if (std::isfinite(current_upper) &&
            current_upper < kValueCeiling &&
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
         * Once every non-goal state and every filtered action are complete,
         * there is no fringe estimate left to refine. Overlap can then mean
         * that several rows form a proper/improving policy only together.
         * Admit one such row to the exact closed Bellman problem and let the
         * following solve decide it. This closure rule cannot run on the
         * large incomplete Chaos fringe and is not the removed broad-graph
         * overlap fallback.
         */
        if (unresolved != incremental_alternative_rows.end() &&
            restricted_graph_closed &&
            incremental_restricted_values_ready &&
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
    refresh_incremental_upper_incumbent();
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
    diagnostics.incremental_refinement_rounds =
        incremental_refinement_rounds;
    diagnostics.incremental_refinement_states_selected =
        incremental_refinement_states_selected;
    diagnostics.incremental_rows_reconsidered =
        incremental_rows_reconsidered;
    diagnostics.incremental_upper_policy_updates =
        incremental_upper_policy_updates;
    diagnostics.incremental_refinement_uncertainty =
        incremental_refinement_uncertainty;
    diagnostics.incremental_action_witnesses.clear();
    diagnostics.incremental_action_witnesses_omitted = 0;
    const bool exact_final_values =
        incremental_envelope_closed && optimization_converged() &&
        (focused_bound_proved || !focused_mode);
    const std::vector<double> certified_lower =
        certified_incremental_lower_values();
    for (IncrementalAlternativeRow& candidate :
         incremental_alternative_rows) {
        if (exact_final_values) {
            candidate.lower_q = sparse_row_q_for_values(
                candidate.row_index, result.values);
            candidate.upper_q = candidate.lower_q;
        } else {
            candidate.lower_q = sparse_row_q_for_values(
                candidate.row_index, certified_lower);
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
