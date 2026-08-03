#include "solver_solve_types.hpp"
#include "solver_policy_refinement.hpp"
#include "solver_sparse_policy.hpp"

namespace poecraft {
namespace solver {

using namespace solve_detail;

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
        const bool resource_cap_hit) {
    if (coarse_termination == SolveTermination::ExactClosed) {
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
    /*
     * The caller has retained an exact executable strategy, so None and
     * NoExecutablePolicy cannot remain the published stopping cause.
     */
    return SolveTermination::ExactClosed;
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

SolveResult SolveWork::Impl::finish() {
        if (phase != SolvePhase::Done) {
            throw std::logic_error("solver work is not finished");
        }
        if (consumed) {
            throw std::logic_error("solver work was already finished");
        }
        const auto extraction_started = std::chrono::steady_clock::now();
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
        const bool restore_output_incumbent =
            output_incumbent.has_value() &&
            (target_gap_stop || result.diagnostics.state_cap_hit ||
             result.diagnostics.resource_cap_hit || sweep_cap_hit);
        std::vector<double> restored_policy_row_costs;
        std::vector<std::uint8_t> restored_policy_reachable;
        bool explicit_restored_policy_reachable = false;
        if (restore_output_incumbent) {
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
        /* Deterministic argmin: cost ties break toward lower cost-to-go
         * variance, then lower action index by stable registry traversal. */
        const bool authoritative_policy_available =
            !result.diagnostics.state_cap_hit &&
            !result.diagnostics.resource_cap_hit &&
            !focused_bound_proved && !restore_output_incumbent;
        for (std::uint32_t state = 0;
             authoritative_policy_available && state < state_count; ++state) {
            if (finalization_capped) break;
            if (!result.expanded[state] || result.goal_states[state]) continue;
            double best_q = kInfinity;
            double best_variance = kInfinity;
            std::uint32_t best_operator = kNoId;
            std::uint64_t best_row_index = std::numeric_limits<std::uint64_t>::max();
            for (const std::uint64_t absolute_row :
                 state_row_indices(*transition_cache, state)) {
                if (preservation_prunes(absolute_row)) continue;
                const SparseRow& row = transition_cache->rows.at(absolute_row);
                if (!row.admitted) continue;
                const std::uint64_t variance_scratch_bytes =
                    (static_cast<std::uint64_t>(row.transition_count) +
                     row.choice_count + 1) *
                    sizeof(std::pair<double, double>);
                if (check_solver_byte_cap_from(
                        extraction_base_bytes,
                        variance_scratch_bytes)) {
                    finalization_capped = true;
                    break;
                }
                ++result.diagnostics.extraction_action_evaluations;
                std::uint32_t transition_work = 0;
                const double q = sparse_row_q(absolute_row, transition_work);
                if (q == kInfinity) continue;
                double mean = 0.0;
                std::vector<std::pair<double, double>> random_values;
                if (row.self_probability > 0.0) {
                    random_values.push_back(
                        {row.self_probability, result.values[state]});
                    mean += row.self_probability * result.values[state];
                }
                for (std::uint32_t i = 0;
                     i < row.transition_count; ++i) {
                    const std::uint64_t offset =
                        row.transition_offset + i;
                    if (transition_cache->successors.at(offset) == state) {
                        continue;
                    }
                    random_values.push_back(
                        {transition_cache->probabilities.at(offset),
                         result.values[
                             transition_cache->successors.at(offset)]});
                    mean += transition_cache->probabilities.at(offset) *
                            result.values[
                                transition_cache->successors.at(offset)];
                }
                for (std::uint32_t i = 0; i < row.choice_count; ++i) {
                    const SparseChoiceGroup& group =
                        transition_cache->choices.at(row.choice_offset + i);
                    double chosen = group.has_self
                                        ? result.values[state]
                                        : kInfinity;
                    for (std::uint32_t s = 0;
                         s < group.successor_count; ++s) {
                        chosen = std::min(
                            chosen,
                            result.values[transition_cache->choice_successors.at(
                                group.successor_offset + s)]);
                    }
                    random_values.push_back(
                        {group.probability, chosen});
                    mean += group.probability * chosen;
                }
                double variance = 0.0;
                for (const auto& [probability, value] : random_values) {
                    const double delta = value - mean;
                    variance += probability * delta * delta;
                }
                const bool better =
                    q < best_q - options.epsilon ||
                    (q < best_q + options.epsilon &&
                     variance < best_variance - options.epsilon);
                if (better) {
                    best_q = q;
                    best_variance = variance;
                    best_operator = priced_rows[absolute_row].operator_index;
                    best_row_index = absolute_row;
                }
            }
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
        } else if (executable_policy_abstraction_supported &&
                   restore_output_incumbent &&
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
                    : SolveTermination::RefusedResourceCap;
            result.lower_bound =
                result.diagnostics.focused_lower_bound;
            result.upper_bound = incumbent.certified_upper_bound;
            result.evaluated_policy_cost =
                incumbent.evaluated_policy_cost;
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
                result.diagnostics.resource_cap_hit
                    ? SolveTermination::RefusedResourceCap
                    : SolveTermination::NoExecutablePolicy;
            result.lower_bound =
                result.diagnostics.focused_expansion
                    ? result.diagnostics.focused_lower_bound
                    : 0.0;
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
                        : SolveTermination::NoExecutablePolicy;
                result.lower_bound =
                    result.diagnostics.focused_expansion
                        ? result.diagnostics.focused_lower_bound
                        : 0.0;
                result.upper_bound = kInfinity;
                result.evaluated_policy_cost = kInfinity;
                result.absolute_optimality_gap = kInfinity;
                result.relative_optimality_gap = kInfinity;
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
                const std::uint64_t coarse_reforge_work =
                    std::min(
                        calc.telemetry().reforge_frontier_work,
                        options.max_reforge_work);
                scoped.max_reforge_work =
                    options.max_reforge_work -
                    coarse_reforge_work;
                return scoped;
            };
        if (result.policy_available &&
            result.diagnostics.policy_refinement.triggers != 0) {
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
            refinement::PolicyExactLiftCertificate certificate;
            if (lift_options.has_value()) {
                certificate = refinement::lift_policy_quotient(
                    calc, result, exact_start_item, prices,
                    *lift_options, "solved policy");
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
            telemetry.status =
                refinement::policy_exact_lift_status_name(
                    certificate.status);
            telemetry.memory_limit_bytes =
                options.max_solver_owned_bytes;
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
            telemetry.work_to_first_partition =
                certificate.adapter.work_to_first_partition;
            telemetry.work_to_first_executable_upper =
                certificate.adapter.work_to_first_executable_upper;
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
                certificate.resource_cap == "max_reforge_work"
                    ? options.max_reforge_work
                    : std::min(
                          options.max_reforge_work,
                          saturated_add(
                              result.diagnostics.reforge_frontier_work,
                              saturated_add(
                                  certificate.adapter
                                      .strict_reforge_work,
                                  certificate.compiled.evaluation
                                      .reforge_work)));
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
                artifact.working_states =
                    certificate.compiled.compilation.working_states;
                artifact.policy_regions =
                    certificate.compiled.compilation.policy_regions;
                artifact.nodes =
                    certificate.compiled.compilation.nodes;
                artifact.edges =
                    certificate.compiled.compilation.edges;
                result.refined_policy_artifact =
                    std::move(artifact);
                telemetry.retained_artifact_bytes =
                    result.refined_policy_artifact
                            .strategy_json.capacity() +
                    1;
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
                    /*
                     * The retained artifact is an exact, proper executable
                     * policy. Compatibility-triggered lifting proves that
                     * fixed policy, not global optimality over every exact
                     * subclass and admitted alternative. Publish its exact
                     * cost as a feasible upper bound and discard the coarse
                     * exactness claim even when the lifted value happens to
                     * reconcile and no local action changed.
                     */
                    if (!std::isfinite(
                            certificate.exact_start_cost) ||
                        certificate.exact_start_cost < 0.0) {
                        throw std::logic_error(
                            "completed exact policy refinement returned "
                            "an invalid start cost");
                    }
                    const double exact_policy_cost =
                        certificate.exact_start_cost;
                    result.converged = false;
                    result.policy_status =
                        SolvePolicyStatus::BoundedFeasible;
                    result.target_met = false;
                    result.target_fired = SolveGapTarget::None;
                    result.lower_bound = 0.0;
                    result.upper_bound = exact_policy_cost;
                    result.evaluated_policy_cost =
                        exact_policy_cost;
                    result.absolute_optimality_gap =
                        exact_policy_cost;
                    result.relative_optimality_gap = kInfinity;
                    if (result.start_state <
                        result.values.size()) {
                        result.values[result.start_state] =
                            exact_policy_cost;
                    }
                    result.diagnostics.focused_lower_bound = 0.0;
                    result.diagnostics.focused_upper_bound =
                        exact_policy_cost;
                    result.diagnostics
                        .focused_partial_policy_upper_bound =
                        exact_policy_cost;
                    result.diagnostics.focused_optimality_gap =
                        exact_policy_cost;
                    result.diagnostics.solution_scope =
                        "policy_guided_exact_refinement_bounded";
                    result.diagnostics.incumbent_kind =
                        "policy_guided_exact_refinement";
                    result.termination =
                        successful_refined_publication_termination(
                            coarse_solve_termination,
                            result.diagnostics.resource_cap_hit);
                }
            }
            if (!lift_complete) {
                const std::string status =
                    refinement::policy_exact_lift_status_name(
                        certificate.status);
                telemetry.status = status;
                const std::string reason =
                    "policy_exact_refinement_" + status;
                record_refinement_refusal(reason);
                revoke_publication(
                    certificate.failure_reason.empty()
                        ? reason
                        : reason + ": " +
                              certificate.failure_reason,
                    certificate.resource_cap);
            }
        }
        if (result.policy_available &&
            result.diagnostics.policy_refinement.triggers == 0) {
            /*
             * Compatibility-triggered lifting is lazy, but publication proof
             * is unconditional. A zero-trigger policy still has to compile,
             * parse, exact-evaluate as a proper absorbing strategy, and
             * reconcile with the solver value.
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
            const std::optional<SolveOptions> assertion_options =
                publication_options_for_live(
                    publication_live_bytes);
            refinement::CompiledPolicyAssertion assertion;
            if (assertion_options.has_value()) {
                assertion =
                    refinement::assert_compiled_policy_exact(
                        calc, result, prices, *assertion_options,
                        "solved policy");
            } else {
                assertion.status =
                    refinement::CompiledPolicyAssertionStatus::
                        ResourceCap;
                assertion.resource_cap =
                    "max_solver_owned_bytes";
                assertion.failure_reason =
                    "coarse live solve leaves no memory for exact "
                    "publication assertion";
            }
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
            result.diagnostics.reforge_frontier_work =
                assertion.resource_cap == "max_reforge_work"
                    ? options.max_reforge_work
                    : std::min(
                          options.max_reforge_work,
                          saturated_add(
                              result.diagnostics.reforge_frontier_work,
                              assertion.evaluation.reforge_work));
            PolicyRefinementTelemetry& telemetry =
                result.diagnostics.policy_refinement;
            telemetry.memory_limit_bytes =
                options.max_solver_owned_bytes;
            telemetry.compiled_assertion_checked =
                assertion.status !=
                refinement::CompiledPolicyAssertionStatus::NotRun;
            telemetry.compiled_policy_proper = assertion.proper;
            telemetry.zero_off_policy = assertion.zero_off_policy;
            telemetry.cost_reconciled = assertion.cost_reconciled;
            const auto assertion_status_name =
                [](const refinement::CompiledPolicyAssertionStatus status) {
                    switch (status) {
                    case refinement::CompiledPolicyAssertionStatus::NotRun:
                        return "not_run";
                    case refinement::CompiledPolicyAssertionStatus::Complete:
                        return "complete";
                    case refinement::CompiledPolicyAssertionStatus::NoPolicy:
                        return "no_policy";
                    case refinement::CompiledPolicyAssertionStatus::
                            ResourceCap:
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
                    case refinement::CompiledPolicyAssertionStatus::
                            CostMismatch:
                        return "cost_mismatch";
                    }
                    return "not_run";
                };
            const std::string assertion_status =
                assertion_status_name(assertion.status);
            telemetry.status =
                "exact_assertion_" + assertion_status;
            const std::uint64_t assertion_memory =
                saturated_add(
                    publication_live_bytes,
                    saturated_add(
                        assertion.strategy_json.capacity() + 1,
                        saturated_add(
                            assertion.parsed_strategy_bytes,
                            saturated_add(
                                assertion.economy_bytes,
                                assertion.evaluation
                                    .owned_bytes_estimate))));
            const std::uint64_t assertion_peak =
                saturated_add(
                    publication_live_bytes,
                    saturated_add(
                        assertion.strategy_json.capacity() + 1,
                        saturated_add(
                            assertion.parsed_strategy_bytes,
                            saturated_add(
                                assertion.economy_bytes,
                                assertion.evaluation
                                    .peak_owned_bytes_estimate))));
            const std::uint64_t reported_assertion_peak =
                saturated_add(
                    publication_external_live_bytes,
                    assertion.publication_peak_owned_bytes);
            telemetry.memory_bytes =
                std::max(telemetry.memory_bytes, assertion_memory);
            telemetry.peak_memory_bytes =
                std::max(
                    telemetry.peak_memory_bytes,
                    std::max(
                        assertion_peak,
                        reported_assertion_peak));
            peak_owned_bytes =
                std::max(peak_owned_bytes, telemetry.peak_memory_bytes);
            if (assertion.status !=
                    refinement::CompiledPolicyAssertionStatus::
                        Complete ||
                !assertion.executable) {
                const std::string reason =
                    "policy_exact_publication_assertion: " +
                    (assertion.failure_reason.empty()
                         ? std::string{"incomplete"}
                         : assertion.failure_reason);
                record_refinement_refusal(
                    "policy_exact_publication_assertion_" +
                    assertion_status);
                revoke_publication(
                    reason, assertion.resource_cap);
            }
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
        consumed = true;
        return std::move(result);
    }

}
}
