#include "solver_solve_types.hpp"

namespace poecraft {
namespace solver {

using namespace solve_detail;

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
            if (best_operator != kNoId &&
                best_row_index != std::numeric_limits<std::uint64_t>::max() &&
                calc.operators()[best_operator].kind ==
                    PlannerOperatorKind::Primitive &&
                calc.registry()
                        .actions[calc.operators()[best_operator]
                                     .primitive_action]
                        .params.type == ActionType::Unveil) {
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
                std::sort(
                    choice_options.begin(), choice_options.end(),
                    [&](const OutcomeChoiceOption& a,
                        const OutcomeChoiceOption& b) {
                        const double left = result.values[a.state];
                        const double right = result.values[b.state];
                        return left != right ? left < right
                                             : a.mod_id < b.mod_id;
                    });
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
                    std::sort(
                        choices.begin(), choices.end(),
                        [&](const OutcomeChoiceOption& a,
                            const OutcomeChoiceOption& b) {
                            const double left = result.values[a.state];
                            const double right = result.values[b.state];
                            return left != right ? left < right
                                                 : a.mod_id < b.mod_id;
                        });
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
                    std::uint32_t chosen = group.has_self ? state : kNoId;
                    double chosen_value =
                        group.has_self ? result.values[state] : kInfinity;
                    for (std::uint32_t s = 0; s < group.successor_count; ++s) {
                        const std::uint32_t successor =
                            transition_cache->choice_successors.at(
                                group.successor_offset + s);
                        const double successor_value = result.values[successor];
                        if (successor_value < chosen_value - options.epsilon ||
                            (std::abs(successor_value - chosen_value) <=
                                 options.epsilon &&
                             successor < chosen)) {
                            chosen = successor;
                            chosen_value = successor_value;
                        }
                    }
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
         * exclusion-group identity. Renewal rows that wipe the ambiguous
         * carrier remain exact, but a selected pool-add row (or a renewal
         * preserving that carrier) would compile to the concrete engine and
         * can have a different value. Do not publish such a policy as
         * executable. This is a compatibility refusal, not a new planner
         * action filter or a change to the qualified parent transitions.
         */
        bool executable_policy_abstraction_supported = true;
        if (calc.product_solver_parent()) {
            const auto exclusion_signature =
                [&](const std::uint32_t mod) {
                    std::vector<std::uint64_t> signature(
                        session.words, 0);
                    if (mod + 1 >= session.group_offsets.size()) {
                        return signature;
                    }
                    for (std::uint32_t offset =
                             session.group_offsets[mod];
                         offset < session.group_offsets[mod + 1];
                         ++offset) {
                        const std::uint32_t group =
                            session.group_ids[offset];
                        if (group >= session.group_masks.size() ||
                            session.group_masks[group].empty()) {
                            continue;
                        }
                        pc_bitset_or(
                            signature.data(), signature.data(),
                            session.group_masks[group].data(),
                            session.words);
                    }
                    return signature;
                };
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
                                exclusion_signature(
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
            const auto state_has_ambiguous_identity =
                [&](const AbstractState& state,
                    const bool preserved_only) {
                    for (std::size_t junk = 0;
                         junk < ambiguous_junk.size(); ++junk) {
                        if (!ambiguous_junk[junk]) continue;
                        const JunkClass& klass =
                            calc.layout().junk_classes[junk];
                        const bool locked =
                            (klass.gen_type == PC_SIDE_PREFIX &&
                             (state.flags & kFlagPrefixesLocked)) ||
                            (klass.gen_type == PC_SIDE_SUFFIX &&
                             (state.flags & kFlagSuffixesLocked));
                        const std::uint8_t count =
                            preserved_only
                                ? state.fractured_junk_counts[junk] +
                                      (locked
                                           ? state.junk_counts[junk] -
                                                 state
                                                     .fractured_junk_counts[
                                                         junk]
                                           : 0)
                                : state.junk_counts[junk];
                        if (count != 0) return true;
                    }
                    for (std::size_t slot = 0;
                         slot < ambiguous_goal.size(); ++slot) {
                        const auto status = static_cast<GoalSlotStatus>(
                            state.slot_status[slot]);
                        if (!ambiguous_goal[slot][
                                static_cast<std::size_t>(status)]) {
                            continue;
                        }
                        if (!preserved_only ||
                            (state.fractured_goal_mask & (1u << slot)) != 0) {
                            return true;
                        }
                    }
                    return false;
                };
            const auto action_needs_identity =
                [&](const ActionDescriptor& action,
                    const AbstractState& state) {
                    switch (action.params.type) {
                    case ActionType::Augment:
                    case ActionType::Regal:
                    case ActionType::Exalt:
                        return state_has_ambiguous_identity(state, false);
                    default:
                        break;
                    }
                    if (action_transition_facts(action.params.type).renewal) {
                        return state_has_ambiguous_identity(state, true);
                    }
                    return false;
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
                            executable_policy_abstraction_supported =
                                false;
                            const std::string reason =
                                "coarse_parent_capped_renewal_without_"
                                "exact_witness";
                            result.diagnostics
                                .policy_compatibility_supported = false;
                            result.diagnostics
                                .policy_compatibility_state =
                                result.start_state;
                            result.diagnostics
                                .policy_compatibility_action =
                                action.id;
                            result.diagnostics
                                .policy_compatibility_reason =
                                reason;
                            record_skipped_unsupported(action.id);
                            add_action_reason(
                                "unsupported", action.id,
                                reason + "_at_state_" +
                                    std::to_string(
                                        result.start_state));
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
                std::vector<std::uint32_t> actions =
                    planner.kind == PlannerOperatorKind::Primitive
                        ? std::vector<std::uint32_t>{
                              planner.primitive_action}
                        : planner.primitive_program;
                if (planner.conditional_action != kNoId) {
                    actions.push_back(planner.conditional_action);
                }
                for (const std::uint32_t action_index : actions) {
                    if (action_index >=
                            calc.registry().actions.size() ||
                        !action_needs_identity(
                            calc.registry().actions[action_index],
                            calc.state(state_id))) {
                        continue;
                    }
                    executable_policy_abstraction_supported = false;
                    const std::string& action_id =
                        calc.registry().actions[action_index].id;
                    const std::string reason =
                        "coarse_parent_requires_exact_exclusion_identity";
                    result.diagnostics.policy_compatibility_supported =
                        false;
                    result.diagnostics.policy_compatibility_state =
                        state_id;
                    result.diagnostics.policy_compatibility_action =
                        action_id;
                    result.diagnostics.policy_compatibility_reason =
                        reason;
                    record_skipped_unsupported(action_id);
                    add_action_reason(
                        "unsupported", action_id,
                        reason + "_at_state_" +
                            std::to_string(state_id));
                    break;
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
        const std::uint64_t final_live_bytes = estimated_owned_bytes();
        peak_owned_bytes = std::max(peak_owned_bytes, final_live_bytes);
        if (final_live_bytes > options.max_solver_owned_bytes) {
            record_cap("max_solver_owned_bytes");
            result.converged = false;
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
