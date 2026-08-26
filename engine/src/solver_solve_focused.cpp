#include "solver_solve_types.hpp"
#include "solver_sparse_policy.hpp"

namespace poecraft {
namespace solver {

using namespace solve_detail;

void SolveWork::Impl::begin_focused_lower_solve() {
        if (focused_lower_preparation_stage !=
            FocusedLowerPreparationStage::Idle) {
            throw std::logic_error(
                "focused lower preparation already active");
        }
        focused_mode = true;
        focus_optimizing = true;
        focused_lower_mode = true;
        result.diagnostics.focused_expansion = true;
        const std::uint32_t state_count = calc.state_count();
        transition_cache->state_rows.resize(state_count);
        focused_lower_previous_values = std::move(result.values);
        focused_lower_retained_minimum.clear();
        if (focused_lower_completion_proof_snapshot_initialized) {
            focused_lower_completion_proof_values.resize(
                state_count, std::numeric_limits<double>::quiet_NaN());
        } else {
            focused_lower_completion_proof_values.assign(
                state_count, std::numeric_limits<double>::quiet_NaN());
        }
        result.values.assign(state_count, 0.0);
        focused_lower_preparation_cursor = 0;
        focused_lower_preparation_stage =
            FocusedLowerPreparationStage::ProofValues;
    }

bool SolveWork::Impl::advance_focused_lower_preparation() {
        const std::uint32_t state_count = calc.state_count();
        switch (focused_lower_preparation_stage) {
        case FocusedLowerPreparationStage::Idle:
            return true;
        case FocusedLowerPreparationStage::ProofValues:
            if (focused_lower_preparation_cursor < state_count) {
                const std::uint32_t state =
                    focused_lower_preparation_cursor++;
                if (!calc.is_goal_state(calc.state(state))) {
                    /* Retain completed values and produce proof for newly
                     * interned support one state per host continuation. The
                     * shared goal-cover model was prepared during measured
                     * solve setup, so this lookup cannot hide that one-time
                     * construction inside a state-local slice. */
                    double proof =
                        focused_lower_completion_proof_values[state];
                    if (!std::isfinite(proof)) {
                        proof = completion_proof_lower(state).value;
                    }
                    result.values[state] = proof;
                    focused_lower_completion_proof_values[state] = proof;
                }
                return false;
            }
            focused_lower_completion_proof_snapshot_initialized = true;
        /* This independent carrier/pattern relaxation covers the complete
         * requested action envelope. Publish it for every session; the
         * restricted focused optimum remains separate and cannot replace
         * this global floor while incremental action coverage is open. */
        if (result.start_state < result.values.size()) {
            const double independent_lower =
                result.values[result.start_state];
            if (std::isfinite(independent_lower) &&
                independent_lower >= 0.0 &&
                independent_lower < kValueCeiling) {
                result.diagnostics.independent_goal_cover_lower_bound =
                    std::max(
                        result.diagnostics
                            .independent_goal_cover_lower_bound,
                        independent_lower);
                result.diagnostics.focused_lower_bound = std::max(
                    result.diagnostics.focused_lower_bound,
                        independent_lower);
            }
        }
            focused_lower_preparation_cursor = 0;
            if (focused_behavioral_representative.empty()) {
                focused_lower_preparation_stage =
                    FocusedLowerPreparationStage::RetainDirect;
            } else {
                focused_lower_retained_minimum.assign(
                    state_count, kInfinity);
                focused_lower_preparation_stage =
                    FocusedLowerPreparationStage::RetainClassMinimum;
            }
            return false;
        case FocusedLowerPreparationStage::RetainDirect: {
        /* Expanding a previously zero-valued frontier can only raise the
         * focused lower bound. Preserve the last round's admissible values
         * for already-known states instead of restarting every exact policy
         * evaluation from zero. */
        const std::size_t retained = std::min<std::size_t>(
            focused_lower_previous_values.size(), result.values.size());
            if (focused_lower_preparation_cursor < retained) {
                const std::size_t state =
                    focused_lower_preparation_cursor++;
                const double previous =
                    focused_lower_previous_values[state];
                if (std::isfinite(previous) && previous >= 0.0 &&
                    previous < kValueCeiling) {
                    result.values[state] =
                        std::max(result.values[state], previous);
                }
                return false;
            }
            focused_lower_previous_values.clear();
            focused_lower_preparation_cursor = 0;
            focused_lower_preparation_stage =
                FocusedLowerPreparationStage::GoalStates;
            return false;
        }
        case FocusedLowerPreparationStage::RetainClassMinimum: {
            /* Exact class members have the same value in the current lower
             * problem. Retaining their minimum prior lower bound preserves
             * admissibility even if an earlier solve stopped at tolerance. */
            const std::size_t retained = std::min<std::size_t>(
                focused_lower_previous_values.size(), result.values.size());
            if (focused_lower_preparation_cursor < retained) {
                const std::size_t state =
                    focused_lower_preparation_cursor++;
                const double value = focused_lower_previous_values[state];
                if (!std::isfinite(value) || value < 0.0 ||
                    value >= kValueCeiling) {
                    return false;
                }
                const std::uint32_t representative =
                    focused_behavioral_representative.at(state);
                focused_lower_retained_minimum[representative] = std::min(
                    focused_lower_retained_minimum[representative], value);
                return false;
            }
            focused_lower_previous_values.clear();
            focused_lower_preparation_cursor = 0;
            focused_lower_preparation_stage =
                FocusedLowerPreparationStage::ApplyClassMinimum;
            return false;
        }
        case FocusedLowerPreparationStage::ApplyClassMinimum:
            if (focused_lower_preparation_cursor < state_count) {
                const std::uint32_t representative =
                    focused_lower_preparation_cursor++;
                if (std::isfinite(
                        focused_lower_retained_minimum[representative])) {
                    result.values[representative] = std::max(
                        result.values[representative],
                        focused_lower_retained_minimum[representative]);
                }
                return false;
            }
            focused_lower_retained_minimum.clear();
            focused_lower_preparation_cursor = 0;
            focused_lower_preparation_stage =
                FocusedLowerPreparationStage::GoalStates;
            return false;
        case FocusedLowerPreparationStage::GoalStates:
            if (focused_lower_preparation_cursor == 0) {
        result.expanded = expanded;
        result.expanded.resize(state_count, 0);
        result.goal_states.assign(state_count, 0);
            }
            if (focused_lower_preparation_cursor < state_count) {
                const std::uint32_t state =
                    focused_lower_preparation_cursor++;
                if (calc.is_goal_state(calc.state(state))) {
                    result.goal_states[state] = 1;
                }
                return false;
            }
            focused_lower_preparation_cursor = 0;
            focused_lower_preparation_stage =
                FocusedLowerPreparationStage::Finalize;
            return false;
        case FocusedLowerPreparationStage::Finalize:
        prepare_focused_exact_quotient();
        prepare_priced_rows();
        policy_rows.clear();
        improper_policy_states.clear();
        policy_initialized = false;
        policy_stable = false;
        policy_iteration_failed = false;
        reset_policy_iteration_units();
        /* Focused solves construct an explicitly proper ranked policy before
         * their first evaluation. The legacy four all-action seed passes were
         * only a heuristic defense against improper initialization and become
         * pure duplicate transition work once that exact initializer exists. */
        policy_unit_stage = PolicyUnitStage::InitialSelect;
        backup_active = false;
        sweeps = 0;
        residual = kValueCeiling;
        if (!result.diagnostics.policy_evaluation_failure.starts_with(
                "selected_byte_cap_breakdown:")) {
            result.diagnostics.policy_evaluation_failure.clear();
        }
        if (expanded_count == 1 &&
            result.start_state < result.expanded.size() &&
            result.expanded[result.start_state] &&
            !result.goal_states[result.start_state]) {
            const std::uint64_t no_row =
                std::numeric_limits<std::uint64_t>::max();
            policy_rows.assign(state_count, no_row);
            double best = kInfinity;
            std::uint64_t best_row = no_row;
            for (const std::uint64_t absolute :
                 state_row_indices(*transition_cache, result.start_state)) {
                if (!transition_cache->rows.at(absolute).admitted) continue;
                if (preservation_prunes(absolute)) continue;
                std::uint32_t work = 0;
                const double candidate = sparse_row_q(absolute, work);
                ++result.diagnostics.bellman_action_evaluations;
                if (sparse_policy_row_precedes(
                        candidate, absolute, best, best_row)) {
                    best = candidate;
                    best_row = absolute;
                }
            }
            ++result.diagnostics.bellman_backups;
            if (best_row != no_row && std::isfinite(best)) {
                result.values[result.start_state] = best;
                policy_rows[result.start_state] = best_row;
                policy_initialized = true;
                policy_stable = true;
                residual = 0.0;
                result.diagnostics.residual = 0.0;
                finish_focused_lower_solve();
            }
        }
            focused_lower_preparation_stage =
                FocusedLowerPreparationStage::Idle;
            focused_lower_preparation_cursor = 0;
            return true;
        }
        throw std::logic_error("invalid focused lower preparation stage");
    }

bool SolveWork::Impl::collect_focused_fringe(
        std::vector<std::uint32_t>& fringe,
        std::vector<double>& priority,
        const std::vector<double>* gap_lower_values,
        std::vector<std::uint8_t>* policy_reachable) {
        const std::uint64_t no_row =
            std::numeric_limits<std::uint64_t>::max();
        if (result.start_state >= result.values.size()) return false;
        std::vector<std::uint8_t> visited(result.values.size(), 0);
        std::vector<std::uint8_t> reachable(result.values.size(), 0);
        std::vector<std::uint8_t> queued_fringe(result.values.size(), 0);
        priority.assign(result.values.size(), 0.0);
        std::vector<double> path_mass(result.values.size(), 0.0);
        path_mass[result.start_state] = 1.0;
        std::unordered_set<std::uint64_t> routed_transition_kernels;
        std::deque<std::uint32_t> walk{result.start_state};
        const auto route = [&](const std::uint32_t successor,
                               const double mass) {
            reachable[successor] = 1;
            if (result.goal_states[successor]) return;
            if (!result.expanded[successor]) {
                double contribution = mass;
                if (gap_lower_values != nullptr &&
                    successor < gap_lower_values->size()) {
                    const double state_upper = result.values[successor];
                    const double state_lower = (*gap_lower_values)[successor];
                    if (std::isfinite(state_upper) &&
                        std::isfinite(state_lower)) {
                        contribution *= std::max(
                            0.0, state_upper - state_lower);
                    }
                } else if (successor <
                               focused_previous_upper_values.size()) {
                    const double state_upper =
                        focused_previous_upper_values[successor];
                    const double state_lower = result.values[successor];
                    if (std::isfinite(state_upper) &&
                        std::isfinite(state_lower)) {
                        contribution *= std::max(
                            0.0, state_upper - state_lower);
                    }
                } else if (focused_fallback_policy) {
                    const FocusedFallbackPolicy& fallback =
                        *focused_fallback_policy;
                    const double state_upper = std::min(
                        fallback_terminal_upper(successor, fallback),
                        restart_cost + fallback.anchor_state_value);
                    const double state_lower = result.values[successor];
                    if (std::isfinite(state_upper) &&
                        std::isfinite(state_lower)) {
                        contribution *= std::max(
                            0.0, state_upper - state_lower);
                    }
                }
                if (gap_lower_values != nullptr) {
                    const std::uint32_t progress = std::popcount(
                        satisfied_goal_mask_for_state(successor));
                    contribution *= 1.0 +
                        options.focused_goal_progress_priority_multiplier *
                            static_cast<double>(progress);
                }
                priority[successor] += contribution;
                if (!queued_fringe[successor]) {
                    queued_fringe[successor] = 1;
                    fringe.push_back(successor);
                }
            } else if (!visited[successor]) {
                path_mass[successor] += mass;
                walk.push_back(successor);
            }
        };
        while (!walk.empty()) {
            const std::uint32_t state = walk.front();
            walk.pop_front();
            reachable[state] = 1;
            if (visited[state] || result.goal_states[state]) continue;
            visited[state] = 1;
            if (!result.expanded[state] || state >= policy_rows.size() ||
                policy_rows[state] == no_row) {
                return false;
            }
            const SparseRow& row = transition_cache->rows.at(
                policy_rows[state]);
            const double source_mass = path_mass[state];
            double loop_probability = row.self_probability;
            for (std::uint32_t i = 0; i < row.choice_count; ++i) {
                const SparseChoiceGroup& choice =
                    transition_cache->choices.at(row.choice_offset + i);
                if (choice.has_self) loop_probability += choice.probability;
            }
            const double exit_probability = 1.0 - loop_probability;
            const double normalization = exit_probability > 1e-15
                                             ? source_mass / exit_probability
                                             : source_mass;
            const bool route_transitions =
                row.choice_count != 0 || row.transition_count == 0 ||
                routed_transition_kernels.insert(row.transition_offset)
                    .second;
            if (route_transitions) {
                for (std::uint32_t i = 0; i < row.transition_count; ++i) {
                    const std::uint32_t successor =
                        transition_cache->successors.at(
                            row.transition_offset + i);
                    route(
                        successor,
                        normalization * transition_cache->probabilities.at(
                            row.transition_offset + i));
                }
            }
            for (std::uint32_t i = 0; i < row.choice_count; ++i) {
                const SparseChoiceGroup& choice =
                    transition_cache->choices.at(row.choice_offset + i);
                const std::uint32_t selected =
                    select_sparse_policy_choice_successor(
                        *transition_cache, choice, state,
                        result.values);
                if (selected != state && selected != kNoId) {
                    route(selected, normalization * choice.probability);
                }
            }
        }
        if (policy_reachable != nullptr) {
            *policy_reachable = std::move(reachable);
        }
        return true;
    }

double SolveWork::Impl::focused_start_upper_bound(
        const FocusedFallbackPolicy& fallback) const {
        if (result.start_state >= transition_cache->state_rows.size()) {
            return kInfinity;
        }
        const double failure_value =
            restart_cost + fallback.anchor_state_value;
        const auto continuation_upper = [&](const std::uint32_t state) {
            if (state == fallback.anchor_state) {
                return fallback.anchor_state_value;
            }
            return std::min(
                failure_value, fallback_terminal_upper(state, fallback));
        };
        struct FocusedUpperValueContext {
            const SolveWork::Impl* work = nullptr;
            const FocusedFallbackPolicy* fallback = nullptr;
            double failure_value = kInfinity;
        };
        const FocusedUpperValueContext value_context{
            this, &fallback, failure_value};
        double best = continuation_upper(result.start_state);
        for (const std::uint64_t absolute :
             state_row_indices(*transition_cache, result.start_state)) {
            const SparseRow& row = transition_cache->rows.at(absolute);
            if (!row.admitted) continue;
            const PricedSparseRow& priced = priced_rows.at(absolute);
            if (priced.operator_index == kNoId ||
                !std::isfinite(priced.cost) || priced.cost < 0.0) {
                continue;
            }
            std::uint32_t transition_work = 0;
            const double value =
                evaluate_sparse_policy_row_with_accessor(
                    *transition_cache, priced_rows, absolute,
                    transition_work,
                    [](const void* const opaque,
                       const std::uint32_t state) {
                        const FocusedUpperValueContext& source =
                            *static_cast<
                                const FocusedUpperValueContext*>(opaque);
                        if (source.work->calc.is_goal_state(
                                source.work->calc.state(state))) {
                            return 0.0;
                        }
                        if (state == source.fallback->anchor_state) {
                            return source.fallback->anchor_state_value;
                        }
                        return std::min(
                            source.failure_value,
                            source.work->fallback_terminal_upper(
                                state, *source.fallback));
                    },
                    &value_context);
            best = std::min(best, value);
        }
        return best;
    }

std::pair<double, std::uint64_t> SolveWork::Impl::focused_direct_state_upper(
        const std::uint32_t state) const {
        const std::uint64_t no_row =
            std::numeric_limits<std::uint64_t>::max();
        if (state >= transition_cache->state_rows.size()) {
            return {kInfinity, no_row};
        }
        double best = kInfinity;
        std::uint64_t best_row = no_row;
        for (const std::uint64_t absolute :
             state_row_indices(*transition_cache, state)) {
            const SparseRow& row = transition_cache->rows.at(absolute);
            if (!row.admitted) continue;
            const PricedSparseRow& priced = priced_rows.at(absolute);
            if (priced.operator_index == kNoId ||
                !std::isfinite(priced.cost) || priced.cost < 0.0) {
                continue;
            }
            double goal_probability = 0.0;
            double self_probability = row.self_probability;
            bool feasible = true;
            for (std::uint32_t i = 0; i < row.transition_count; ++i) {
                const std::uint64_t offset = row.transition_offset + i;
                const std::uint32_t successor =
                    transition_cache->successors.at(offset);
                if (successor == state) continue;
                if (!calc.is_goal_state(calc.state(successor))) {
                    feasible = false;
                    break;
                }
                goal_probability +=
                    transition_cache->probabilities.at(offset);
            }
            if (!feasible) continue;
            for (std::uint32_t i = 0; i < row.choice_count; ++i) {
                const SparseChoiceGroup& group =
                    transition_cache->choices.at(row.choice_offset + i);
                bool has_goal = false;
                for (std::uint32_t s = 0; s < group.successor_count; ++s) {
                    has_goal |= calc.is_goal_state(calc.state(
                        transition_cache->choice_successors.at(
                            group.successor_offset + s)));
                }
                if (has_goal) {
                    goal_probability += group.probability;
                } else if (group.has_self) {
                    self_probability += group.probability;
                } else {
                    feasible = false;
                    break;
                }
            }
            const double total = goal_probability + self_probability;
            if (!feasible || goal_probability <= 0.0 ||
                std::abs(total - 1.0) > 1e-9) {
                continue;
            }
            const double value = priced.cost / goal_probability;
            if (sparse_policy_row_precedes(
                    value, absolute, best, best_row)) {
                best = value;
                best_row = absolute;
            }
        }
        return {best, best_row};
    }

std::pair<double, std::uint64_t> SolveWork::Impl::focused_direct_start_upper() const {
        return focused_direct_state_upper(result.start_state);
    }

void SolveWork::Impl::reset_focused_optimization_state() {
        policy_rows.clear();
        improper_policy_states.clear();
        policy_initialized = false;
        policy_stable = false;
        policy_iteration_failed = false;
        reset_policy_iteration_units();
        backup_active = false;
        sweeps = 0;
        residual = kValueCeiling;
        result.diagnostics.sweeps = 0;
        result.diagnostics.policy_improvement_rounds = 0;
        result.diagnostics.residual = kValueCeiling;
        result.diagnostics.bellman_backups = 0;
        result.diagnostics.bellman_action_evaluations = 0;
        result.diagnostics.bellman_work_units = 0;
        result.diagnostics.optimization_ns = 0;
        if (!result.diagnostics.policy_evaluation_failure.starts_with(
                "selected_byte_cap_breakdown:")) {
            result.diagnostics.policy_evaluation_failure.clear();
        }
    }

void SolveWork::Impl::schedule_next_focused_expansion(
        std::vector<std::uint32_t> fringe,
        const bool complete,
        const std::vector<double>& priority,
        FocusedScheduleRoundTelemetry telemetry) {
        queue.clear();
        queued.assign(calc.state_count(), 0);
        for (std::uint32_t state = 0; state < expanded.size(); ++state) {
            if (expanded[state]) queued[state] = 1;
        }
        if (!complete) {
            /* An incomplete policy walk cannot certify that it found the
             * whole relevant frontier. Retain exactness by admitting every
             * state already discovered by strict transition generation. */
            for (std::uint32_t state = 0; state < calc.state_count(); ++state) {
                if (!queued[state]) fringe.push_back(state);
            }
        }
        std::sort(fringe.begin(), fringe.end());
        fringe.erase(std::unique(fringe.begin(), fringe.end()), fringe.end());
        telemetry.schedule_candidates = fringe.size();
        const auto state_priority = [&](const std::uint32_t state) {
            return state < priority.size() ? priority[state] : 0.0;
        };
        std::stable_sort(
            fringe.begin(), fringe.end(),
            [&](const std::uint32_t left, const std::uint32_t right) {
                const double left_priority = state_priority(left);
                const double right_priority = state_priority(right);
                return left_priority != right_priority
                           ? left_priority > right_priority
                           : left < right;
            });
        /* A focused round is a work schedule, not a quotient. Bound a broad
         * reforge outcome fringe while retaining strict IDs; later rounds
         * select the next unexpanded members of the same coarse class. */
        const std::uint32_t members_per_class = std::max<std::uint32_t>(
            1, options.focused_members_per_fringe_class);
        auto [coarse, coarse_count] = exact_partition(
            calc.state_count(), [&](const std::uint32_t state) {
                return focused_schedule_signature(state);
            });
        std::vector<std::uint32_t> selected_per_class(coarse_count, 0);
        std::vector<std::uint8_t> selected(calc.state_count(), 0);
        std::vector<std::uint32_t> selected_fringe;
        selected_fringe.reserve(
            std::min<std::size_t>(
                fringe.size(),
                static_cast<std::uint64_t>(coarse_count) *
                    members_per_class));
        if (!focused_fallback_policy &&
            restart_state != kNoId && restart_state < queued.size() &&
            !queued[restart_state]) {
            selected_fringe.push_back(restart_state);
            selected[restart_state] = 1;
            ++selected_per_class[coarse.at(restart_state)];
        }

        /* Reserve half the round for a carrier ladder stratified by the
         * complete satisfied-goal subset. This is work ordering only: no
         * state is merged, pruned, or granted terminal authority. Cycling
         * across masks lets rare 2/5, 3/5, and 4/5 carriers receive rows even
         * when a common zero-progress branch owns most of the path mass, and
         * naturally permits a direct 3 -> 5 transition because terminal
         * successors never need another expansion. Dirty carriers remain in
         * the same competition as clean ones; the structural schedule class
         * above distinguishes the engine-observed junk/protection shape. */
        std::vector<solve_detail::CarrierOrderingScore>
            carrier_candidates;
        carrier_candidates.reserve(fringe.size());
        for (const std::uint32_t state : fringe) {
            if (state >= selected.size() || selected[state] ||
                queued.at(state) ||
                (state == restart_state && !focused_fallback_policy)) {
                continue;
            }
            carrier_candidates.push_back(
                carrier_ordering_score(
                    state, state_priority(state)));
            ++telemetry.carrier_ladder_candidates;
            record_carrier_schedule_attribution(
                CarrierBoundAttributionWork::ScheduleStage::
                    FocusedCandidate,
                state);
        }
        const std::size_t batch = std::max<std::uint32_t>(
            1, options.focused_expansion_batch_states);
        const std::size_t ladder_limit = std::min<std::size_t>(
            batch / 2, batch - selected_fringe.size());
        solve_detail::CarrierPriorityBuckets fairness_buckets =
            solve_detail::build_carrier_priority_buckets(
                carrier_candidates,
                solve_detail::CarrierOrderingMode::FocusedLegacy);
        telemetry.carrier_ladder_goal_subsets =
            fairness_buckets.by_goal_subset.size();
        if (!options.high_impact_executable_uppers ||
            !cooperative_high_progress_ordering_enabled()) {
            std::map<std::uint32_t, std::size_t> subset_cursor;
            bool ladder_advanced = true;
            while (telemetry.carrier_ladder_admissions < ladder_limit &&
                   ladder_advanced) {
                ladder_advanced = false;
                for (const std::uint32_t mask :
                     fairness_buckets.subset_order) {
                    if (telemetry.carrier_ladder_admissions >= ladder_limit) {
                        break;
                    }
                    auto& bucket = fairness_buckets.by_goal_subset.at(mask);
                    std::size_t& cursor = subset_cursor[mask];
                    while (cursor < bucket.size()) {
                        const std::uint32_t state = bucket[cursor++];
                        const std::uint32_t candidate = coarse.at(state);
                        if (selected[state] || queued.at(state) ||
                            selected_per_class[candidate] >=
                                members_per_class) {
                            if (selected_per_class[candidate] >=
                                members_per_class) {
                                ++telemetry.per_class_cap_hits;
                            }
                            continue;
                        }
                        selected[state] = 1;
                        ++selected_per_class[candidate];
                        selected_fringe.push_back(state);
                        record_carrier_schedule_attribution(
                            CarrierBoundAttributionWork::ScheduleStage::
                                FocusedLadderAdmission,
                            state);
                        ++telemetry.carrier_ladder_admissions;
                        ladder_advanced = true;
                        break;
                    }
                }
            }
        } else {
            solve_detail::CarrierPriorityBuckets high_progress_buckets =
                solve_detail::build_carrier_priority_buckets(
                    carrier_candidates,
                    cooperative_high_progress_ordering_enabled()
                        ? solve_detail::CarrierOrderingMode::
                              CooperativeHighProgress
                        : solve_detail::CarrierOrderingMode::FocusedLegacy);
            std::map<std::uint32_t, std::size_t> fairness_cursor;
            std::map<std::uint32_t, std::size_t> high_progress_cursor;
            std::size_t fairness_subset_cursor = 0;
            std::size_t high_progress_subset_cursor = 0;
            const auto admit_one = [&](
                    solve_detail::CarrierPriorityBuckets& buckets,
                    std::map<std::uint32_t, std::size_t>& cursors,
                    std::size_t& subset_cursor) {
                if (buckets.subset_order.empty()) return false;
                for (std::size_t attempt = 0;
                     attempt < buckets.subset_order.size(); ++attempt) {
                    const std::uint32_t mask = buckets.subset_order[
                        subset_cursor % buckets.subset_order.size()];
                    subset_cursor =
                        (subset_cursor + 1) % buckets.subset_order.size();
                    auto& bucket = buckets.by_goal_subset.at(mask);
                    std::size_t& cursor = cursors[mask];
                    while (cursor < bucket.size()) {
                        const std::uint32_t state = bucket[cursor++];
                        const std::uint32_t candidate = coarse.at(state);
                        if (selected[state] || queued.at(state) ||
                            selected_per_class[candidate] >=
                                members_per_class) {
                            if (selected_per_class[candidate] >=
                                members_per_class) {
                                ++telemetry.per_class_cap_hits;
                            }
                            continue;
                        }
                        selected[state] = 1;
                        ++selected_per_class[candidate];
                        selected_fringe.push_back(state);
                        record_carrier_schedule_attribution(
                            CarrierBoundAttributionWork::ScheduleStage::
                                FocusedLadderAdmission,
                            state);
                        ++telemetry.carrier_ladder_admissions;
                        return true;
                    }
                }
                return false;
            };
            bool fairness_available = true;
            bool high_progress_available = true;
            while (telemetry.carrier_ladder_admissions < ladder_limit &&
                   (fairness_available || high_progress_available)) {
                solve_detail::SolveScheduler::Availability available{};
                available[static_cast<std::size_t>(
                    solve_detail::AnytimeSchedulerLane::LegacyFairness)] =
                    fairness_available;
                available[static_cast<std::size_t>(
                    solve_detail::AnytimeSchedulerLane::HighProgress)] =
                    high_progress_available;
                const solve_detail::AnytimeSchedulerLane lane =
                    focused_anytime_scheduler.select(available);
                bool admitted = false;
                if (lane == solve_detail::AnytimeSchedulerLane::
                                LegacyFairness) {
                    admitted = admit_one(
                        fairness_buckets, fairness_cursor,
                        fairness_subset_cursor);
                    fairness_available = admitted;
                } else if (lane == solve_detail::AnytimeSchedulerLane::
                                       HighProgress) {
                    admitted = admit_one(
                        high_progress_buckets, high_progress_cursor,
                        high_progress_subset_cursor);
                    high_progress_available = admitted;
                } else {
                    break;
                }
                if (!admitted) focused_anytime_scheduler.record_yield(lane);
            }
        }
        for (const std::uint32_t state : fringe) {
            if (selected_fringe.size() >=
                options.focused_expansion_batch_states) {
                ++telemetry.global_batch_cap_hits;
                break;
            }
            if (queued.at(state) ||
                selected.at(state) ||
                (state == restart_state &&
                 !focused_fallback_policy)) {
                continue;
            }
            const std::uint32_t candidate = coarse.at(state);
            if (selected_per_class[candidate] >= members_per_class) {
                ++telemetry.per_class_cap_hits;
                continue;
            }
            ++selected_per_class[candidate];
            selected[state] = 1;
            selected_fringe.push_back(state);
        }
        fringe = std::move(selected_fringe);
        for (const std::uint32_t state : fringe) {
            record_carrier_schedule_attribution(
                CarrierBoundAttributionWork::ScheduleStage::
                    FocusedAdmission,
                state);
        }
        telemetry.schedule_admissions = fringe.size();
        result.diagnostics.focused_schedule_rounds.push_back(telemetry);
        for (const std::uint32_t state : fringe) enqueue(state);
        if (queue.empty()) {
            /* No scheduling filter is a closure proof. If the lower-selected
             * fringe produced no work, resume strict closure from every
             * discovered unexpanded state. Only an actually exhausted strict
             * graph may set the full-closure flag. */
            for (std::uint32_t state = 0;
                 state < calc.state_count(); ++state) {
                if (state >= expanded.size() || !expanded[state]) {
                    enqueue(state);
                }
            }
            if (queue.empty()) {
                focused_mode = false;
                full_closure_after_focused_fallback = true;
            } else {
                full_closure_after_focused_fallback = false;
            }
        }
        peak_queue_size = std::max<std::uint32_t>(
            peak_queue_size, static_cast<std::uint32_t>(queue.size()));
        focus_optimizing = false;
        focused_lower_mode = false;
        reset_focused_optimization_state();
    }

bool SolveWork::Impl::begin_focused_upper_solve() {
        const bool from_incremental_incumbent =
            incremental_upper_policy_pass &&
            output_incumbent.has_value();
        if ((!focused_fallback_policy && !from_incremental_incumbent) ||
            focused_strict_transition_cache != nullptr ||
            (!from_incremental_incumbent &&
             !options.allow_economic_restart) ||
            (!from_incremental_incumbent &&
             (!std::isfinite(restart_cost) || restart_cost < 0.0))) {
            return false;
        }
        const FocusedFallbackPolicy* fallback =
            focused_fallback_policy.get();
        focused_round_lower_values = std::move(result.values);
        focused_round_lower_policy_rows = std::move(policy_rows);
        reset_focused_optimization_state();
        if (from_incremental_incumbent) {
            result.values = output_incumbent->values;
            result.values.resize(calc.state_count(), kInfinity);
            /* The upper pass owns one exact fixed-policy proof, so its
             * InitialSelect step must start from the incumbent's executable
             * row choices. Re-running the generic ranked initializer would
             * choose the first merely proper row at every affected carrier;
             * without later Howard sweeps that proves an arbitrary policy
             * and can discard a much cheaper incumbent. Preserve the known
             * policy, then let ordinary selection propose the best newly
             * admitted row against the incumbent value vector before the
             * single proof. */
            policy_rows = output_incumbent->policy_rows;
            policy_rows.resize(
                calc.state_count(),
                std::numeric_limits<std::uint64_t>::max());
            policy_initialized = true;
            focused_frontier_upper_operator =
                output_incumbent->frontier_operators;
            focused_frontier_upper_operator.resize(
                calc.state_count(), kNoId);
            result.expanded.resize(calc.state_count(), 0);
            for (const std::uint64_t row :
                 incremental_upper_temporary_rows) {
                if (row < transition_cache->rows.size()) {
                    const std::uint32_t owner =
                        transition_cache->rows[row].owner_state;
                    if (owner < result.expanded.size()) {
                        result.expanded[owner] = 1;
                    }
                }
            }
        } else {
            const double frontier_upper =
                restart_cost + fallback->anchor_state_value;
            if (!std::isfinite(frontier_upper) ||
                frontier_upper >= kValueCeiling) {
                result.values =
                    std::move(focused_round_lower_values);
                policy_rows =
                    std::move(focused_round_lower_policy_rows);
                return false;
            }
            result.values.assign(calc.state_count(), frontier_upper);
            focused_frontier_upper_operator.assign(
                calc.state_count(), restart_operator_index);
        }
        for (std::uint32_t state = 0; state < result.values.size(); ++state) {
            if (result.goal_states[state]) {
                result.values[state] = 0.0;
                focused_frontier_upper_operator[state] = kNoId;
                continue;
            }
            if (!from_incremental_incumbent && fallback != nullptr) {
                std::uint32_t terminal_operator = kNoId;
                const double terminal = fallback_terminal_upper(
                    state, *fallback, &terminal_operator);
                if (terminal < result.values[state]) {
                    result.values[state] = terminal;
                    focused_frontier_upper_operator[state] =
                        terminal_operator;
                }
            }
        }
        if (!from_incremental_incumbent && fallback != nullptr &&
            fallback->anchor_state < result.values.size()) {
            result.values[fallback->anchor_state] =
                fallback->anchor_state_value;
        }
        /* Every finite frontier terminal means the executable policy
         * `Restart; fallback-at-anchor`. Howard iteration is therefore
         * optimizing a real feasible policy on the expanded exact graph, not
         * a heuristic terminal estimate. The ranked initializer supplies a
         * proper first row at every expanded state. */
        focused_upper_mode = true;
        focus_optimizing = true;
        focused_lower_mode = true;
        policy_unit_stage = PolicyUnitStage::InitialSelect;
        return true;
    }

void SolveWork::Impl::abort_incremental_upper_policy_pass_for_bounded_finish() {
        if (!incremental_upper_policy_pass) return;
        /* The request arrives only between public work units, so no sparse
         * row append is live. Restore the complete lower snapshot and every
         * temporary admission exactly as a rejected upper pass would, but do
         * not finish the potentially large fixed-policy proof first. The
         * last completed output_incumbent remains immutable publication
         * evidence. */
        for (const std::uint64_t row :
             incremental_upper_temporary_rows) {
            if (row < transition_cache->rows.size()) {
                transition_cache->rows[row].admitted = false;
            }
        }
        incremental_upper_temporary_rows.clear();
        result.values = std::move(focused_round_lower_values);
        policy_rows = std::move(focused_round_lower_policy_rows);
        result.expanded = expanded;
        result.expanded.resize(calc.state_count(), 0);
        focused_frontier_upper_operator.clear();
        focused_pending_upper_fringe.clear();
        focused_pending_upper_priority.clear();
        focused_pending_upper_complete = false;
        focused_upper_mode = false;
        focused_lower_mode = false;
        focus_optimizing = false;
        incremental_upper_policy_pass = false;
        incremental_upper_fixed_policy_proved = false;
        incremental_restricted_values_ready = true;
        policy_iteration_failed = false;
        policy_initialized = true;
        policy_stable = true;
        reset_policy_iteration_units();
        ++incremental_upper_policy_passes_rejected;
        incremental_upper_policy_last_failure =
            "requested_bounded_finish";
        retain_action_reason(
            "rejected:high_impact_executable_uppers:requested_bounded_finish");
    }

void SolveWork::Impl::finish_focused_lower_solve(
        const bool allow_upper_pass ) {
        if (allow_upper_pass) {
            if (!constructive_policy_active) {
                ++result.diagnostics.focused_expansion_rounds;
                result.diagnostics.focused_lower_bound =
                    result.values.at(result.start_state);
                result.diagnostics.focused_expansion_ns +=
                    result.diagnostics.optimization_ns;
                constructive_policy_active = true;
            }
            const auto constructive_policy_start =
                std::chrono::steady_clock::now();
            bool fallback_complete = false;
            FocusedFallbackWitness fallback =
                acquire_focused_fallback(fallback_complete);
            result.diagnostics.constructive_policy_ns +=
                static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now() -
                        constructive_policy_start)
                        .count());
            if (!fallback_complete) return;
            constructive_policy_active = false;
            focused_fallback_policy = std::move(fallback);
            const auto strict_clean_goal_cover_start =
                std::chrono::steady_clock::now();
            prepare_strict_clean_goal_cover();
            result.diagnostics.strict_clean_goal_cover_ns +=
                static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now() -
                        strict_clean_goal_cover_start)
                        .count());
            sync_constructive_discovered_states();
            if (strict_clean_goal_cover_refresh_needed) {
                strict_clean_goal_cover_refresh_needed = false;
                begin_focused_lower_solve();
                return;
            }
            if (focused_fallback_policy) {
                install_fallback_output_incumbent(
                    focused_fallback_policy);
            }
            if (begin_focused_upper_solve()) return;
        }
        std::vector<std::uint32_t> fringe;
        std::vector<double> fringe_priority;
        const bool complete = collect_focused_fringe(
            fringe, fringe_priority);
        std::stable_sort(
            fringe.begin(), fringe.end(),
            [&](const std::uint32_t left, const std::uint32_t right) {
                const double left_priority = fringe_priority[left];
                const double right_priority = fringe_priority[right];
                return left_priority != right_priority
                           ? left_priority > right_priority
                           : left < right;
            });
        FocusedScheduleRoundTelemetry schedule_telemetry;
        schedule_telemetry.round =
            result.diagnostics.focused_expansion_rounds;
        schedule_telemetry.lower_candidates = fringe.size();
        schedule_telemetry.upper_candidates =
            focused_pending_upper_fringe.size();
        schedule_telemetry.batch_states = std::max<std::uint32_t>(
            1, options.focused_expansion_batch_states);
        schedule_telemetry.lower_quota = std::min<std::uint64_t>(
            schedule_telemetry.batch_states,
            options.focused_lower_batch_states);
        schedule_telemetry.upper_quota =
            schedule_telemetry.batch_states -
            schedule_telemetry.lower_quota;
        focused_closure_proved = complete && fringe.empty();
        if (!focused_closure_proved && !focused_pending_upper_fringe.empty()) {
            std::stable_sort(
                focused_pending_upper_fringe.begin(),
                focused_pending_upper_fringe.end(),
                [&](const std::uint32_t left, const std::uint32_t right) {
                    const double left_priority =
                        left < focused_pending_upper_priority.size()
                            ? focused_pending_upper_priority[left]
                            : 0.0;
                    const double right_priority =
                        right < focused_pending_upper_priority.size()
                            ? focused_pending_upper_priority[right]
                            : 0.0;
                    return left_priority != right_priority
                               ? left_priority > right_priority
                               : left < right;
                });
            /* Reserve half of every exact expansion batch for each bound.
             * Combining unlike lower-gap and incumbent-improvement scores in
             * one scalar allowed either policy to starve the other. The
             * balanced union changes only work order and remains complete. */
            const std::size_t batch = std::max<std::uint32_t>(
                1, options.focused_expansion_batch_states);
            const std::size_t lower_quota = std::min<std::size_t>(
                batch, options.focused_lower_batch_states);
            const std::size_t upper_quota = batch - lower_quota;
            std::vector<std::uint8_t> selected(result.values.size(), 0);
            std::vector<std::uint32_t> balanced;
            balanced.reserve(batch);
            const auto take = [&](const std::vector<std::uint32_t>& source,
                                  const std::size_t quota) {
                std::size_t admitted = 0;
                for (const std::uint32_t state : source) {
                    if (balanced.size() >= batch || admitted >= quota) break;
                    if (state >= selected.size() || selected[state]) continue;
                    selected[state] = 1;
                    balanced.push_back(state);
                    ++admitted;
                    if (state < focused_pending_upper_priority.size()) {
                        fringe_priority[state] = std::max(
                        fringe_priority[state],
                        focused_pending_upper_priority[state]);
                    }
                }
                return admitted;
            };
            schedule_telemetry.lower_quota_admissions =
                take(fringe, lower_quota);
            schedule_telemetry.upper_quota_admissions =
                take(focused_pending_upper_fringe, upper_quota);
            if (balanced.size() < batch) {
                schedule_telemetry.lower_fill_admissions =
                    take(fringe, batch);
            }
            if (balanced.size() < batch) {
                schedule_telemetry.upper_fill_admissions =
                    take(focused_pending_upper_fringe, batch);
            }
            if (balanced.size() >= batch) {
                const auto has_unselected =
                    [&](const std::vector<std::uint32_t>& source) {
                        return std::any_of(
                            source.begin(), source.end(),
                            [&](const std::uint32_t state) {
                                return state < selected.size() &&
                                       !selected[state];
                            });
                    };
                if (has_unselected(fringe) ||
                    has_unselected(focused_pending_upper_fringe)) {
                    ++schedule_telemetry.global_batch_cap_hits;
                }
            }
            fringe = std::move(balanced);
        }
        focused_pending_upper_fringe.clear();
        focused_pending_upper_priority.clear();
        focused_pending_upper_complete = false;

        if (focused_strict_transition_cache != nullptr) {
            std::vector<std::uint32_t> strict_fringe;
            /* Amortize refinement of the intentionally broad exact
             * zero-terminal frontier class. These remain strict IDs selected
             * only for work scheduling; the next lower solve may distinguish
             * every member through its complete all-action rows. */
            const std::uint32_t members_per_class =
                std::max<std::uint32_t>(
                    1, options.focused_members_per_fringe_class);
            std::vector<std::uint8_t> fringe_class(
                result.values.size(), 0);
            std::vector<std::uint32_t> selected_per_class(
                result.values.size(), 0);
            for (const std::uint32_t representative : fringe) {
                fringe_class.at(representative) = 1;
            }
            strict_fringe.reserve(
                focused_behavioral_representative.size());
            for (std::uint32_t state = 0;
                 state < focused_behavioral_representative.size(); ++state) {
                const std::uint32_t representative =
                    focused_behavioral_representative[state];
                if (!fringe_class.at(representative) ||
                    (state < focused_strict_expanded.size() &&
                     focused_strict_expanded[state])) {
                    continue;
                }
                if (selected_per_class.at(representative) >=
                    members_per_class) {
                    ++schedule_telemetry.per_class_cap_hits;
                    continue;
                }
                ++selected_per_class[representative];
                strict_fringe.push_back(state);
            }
            const std::vector<double> quotient_values = result.values;
            for (std::uint32_t state = 0;
                 state < focused_behavioral_representative.size(); ++state) {
                const std::uint32_t representative =
                    focused_behavioral_representative[state];
                result.values[state] = quotient_values.at(representative);
            }
            fringe = std::move(strict_fringe);
            transition_cache = std::move(focused_strict_transition_cache);
            expanded = std::move(focused_strict_expanded);
            expanded_count = focused_strict_expanded_count;
            focused_strict_expanded_count = 0;
            focused_behavioral_representative.clear();
            result.behavioral_representative_by_state.clear();
            result.expanded = expanded;
            priced_rows.clear();
            pricing_diagnostics_cursor = 0;
            policy_rows.clear();
            prepare_priced_rows();
        } else if (!fringe.empty()) {
            /* Bound each focused round without treating the scheduling
             * classes as solver equivalence. All selected IDs remain strict
             * states, and subsequent exact rows may distinguish every one. */
            const std::uint32_t members_per_class =
                std::max<std::uint32_t>(
                    1, options.focused_members_per_fringe_class);
            auto [coarse, coarse_count] = exact_partition(
                calc.state_count(), [&](const std::uint32_t state) {
                    return focused_schedule_signature(state);
                });
            std::vector<std::uint32_t> selected_per_class(coarse_count, 0);
            std::vector<std::uint32_t> selected_fringe;
            selected_fringe.reserve(fringe.size());
            for (const std::uint32_t state : fringe) {
                const std::uint32_t candidate = coarse.at(state);
                if (selected_per_class[candidate] >= members_per_class) {
                    ++schedule_telemetry.per_class_cap_hits;
                    continue;
                }
                ++selected_per_class[candidate];
                selected_fringe.push_back(state);
            }
            fringe = std::move(selected_fringe);
        }

        focused_pending_lower_fringe = std::move(fringe);
        const auto [direct_upper, direct_row] =
            focused_direct_start_upper();
        if (std::isfinite(direct_upper)) {
            install_direct_output_incumbent(direct_upper, direct_row);
            if (direct_upper < result.diagnostics.focused_upper_bound) {
                result.diagnostics.focused_upper_bound = direct_upper;
                focused_direct_upper_row = direct_row;
            }
            result.diagnostics.focused_optimality_gap = std::max(
                0.0,
                result.diagnostics.focused_upper_bound -
                    result.diagnostics.focused_lower_bound);
            const double proof_tolerance = value_comparison_tolerance(
                result.diagnostics.focused_upper_bound);
            if (focused_direct_upper_row !=
                    std::numeric_limits<std::uint64_t>::max() &&
                std::abs(
                    result.diagnostics.focused_upper_bound -
                    result.diagnostics.focused_lower_bound) <=
                    proof_tolerance) {
                const std::uint64_t no_row =
                    std::numeric_limits<std::uint64_t>::max();
                policy_rows.assign(result.values.size(), no_row);
                policy_rows[result.start_state] =
                    focused_direct_upper_row;
                result.values[result.start_state] =
                    result.diagnostics.focused_upper_bound;
                result.diagnostics.focused_lower_bound =
                    result.diagnostics.focused_upper_bound;
                result.diagnostics.focused_optimality_gap = 0.0;
                focused_bound_proved = true;
                focused_closure_proved = true;
                focused_previous_upper_values = std::move(result.values);
                queue.clear();
                focus_optimizing = false;
                focused_lower_mode = false;
                return;
            }
        }
        if (output_incumbent.has_value()) {
            result.diagnostics.focused_upper_bound =
                output_incumbent->certified_upper_bound;
            result.diagnostics.focused_optimality_gap = std::max(
                0.0,
                result.diagnostics.focused_upper_bound -
                    result.diagnostics.focused_lower_bound);
        }
        if (std::isfinite(focused_partial_upper_bound) &&
            focused_previous_upper_values.size() == result.values.size() &&
            focused_previous_upper_policy_rows.size() == result.values.size()) {
            const double proof_tolerance = value_comparison_tolerance(
                focused_partial_upper_bound);
            if (std::abs(
                    focused_partial_upper_bound -
                    result.diagnostics.focused_lower_bound) <=
                proof_tolerance) {
                result.values = focused_previous_upper_values;
                policy_rows = focused_previous_upper_policy_rows;
                result.diagnostics.focused_upper_bound =
                    focused_partial_upper_bound;
                result.diagnostics.focused_lower_bound =
                    focused_partial_upper_bound;
                result.diagnostics.focused_optimality_gap = 0.0;
                focused_bound_proved = true;
                focused_closure_proved = true;
                queue.clear();
                focus_optimizing = false;
                focused_lower_mode = false;
                return;
            }
        }
        if (!std::isfinite(direct_upper) &&
            !focused_fallback_policy &&
            (restart_state == kNoId ||
             restart_state >= expanded.size() ||
             !expanded[restart_state])) {
            if (restart_state != kNoId) {
                focused_pending_lower_fringe.push_back(restart_state);
            }
            focused_closure_proved = false;
            schedule_next_focused_expansion(
                std::move(focused_pending_lower_fringe), complete,
                fringe_priority, schedule_telemetry);
            return;
        }
        if (focused_closure_proved) {
            /* The lower-selected policy reaches no optimistic frontier, so
             * it is itself a feasible policy with the same value as the
             * global lower bound. This is an exact bracket even when the
             * generic clean-base renewal route is unavailable. */
            focused_bound_proved = true;
            result.diagnostics.focused_upper_bound =
                result.diagnostics.focused_lower_bound;
            result.diagnostics.focused_optimality_gap = 0.0;
            focused_previous_upper_values = std::move(result.values);
            queue.clear();
            focus_optimizing = false;
            focused_lower_mode = false;
            return;
        }
        schedule_next_focused_expansion(
            std::move(focused_pending_lower_fringe), complete,
            fringe_priority, schedule_telemetry);
    }

void SolveWork::Impl::finish_focused_upper_solve(const bool succeeded) {
        const bool incremental_pass = incremental_upper_policy_pass;
        if (incremental_pass) {
            if (succeeded) {
                ++incremental_upper_policy_passes_proper;
            } else {
                ++incremental_upper_policy_passes_rejected;
                incremental_upper_policy_last_failure =
                    result.diagnostics.policy_evaluation_failure;
            }
        }
        result.diagnostics.focused_expansion_ns +=
            result.diagnostics.optimization_ns;
        focused_pending_upper_fringe.clear();
        focused_pending_upper_priority.clear();
        focused_pending_upper_complete = false;
        if (succeeded && result.start_state < result.values.size() &&
            std::isfinite(result.values[result.start_state])) {
            std::vector<std::uint8_t> upper_policy_reachable;
            focused_pending_upper_complete = collect_focused_fringe(
                focused_pending_upper_fringe,
                focused_pending_upper_priority,
                &focused_round_lower_values,
                &upper_policy_reachable);
            if (focused_pending_upper_complete) {
                focused_partial_upper_bound =
                    result.values[result.start_state];
                result.diagnostics.focused_partial_policy_upper_bound =
                    focused_partial_upper_bound;
                ++result.diagnostics.focused_partial_policy_rounds;
                focused_previous_upper_values = result.values;
                focused_previous_upper_policy_rows = policy_rows;
                focused_previous_frontier_upper_operator =
                    focused_frontier_upper_operator;
                std::string incumbent_kind =
                    "partial_upper_plus_fallback";
                if (result.diagnostics.progressive_fracture_status ==
                    "complete") {
                    incumbent_kind =
                        "partial_upper_plus_progressive_fracture";
                } else if (!result.diagnostics
                                .destructive_renewal_action_id.empty()) {
                    incumbent_kind =
                        "partial_upper_plus_destructive_renewal";
                }
                install_output_incumbent(
                    focused_partial_upper_bound, result.values, policy_rows,
                    focused_frontier_upper_operator,
                    focused_fallback_policy, std::move(incumbent_kind),
                    &upper_policy_reachable);
                result.diagnostics.focused_upper_bound =
                    output_incumbent.has_value()
                        ? output_incumbent->certified_upper_bound
                        : result.diagnostics.focused_upper_bound;
                result.diagnostics.focused_optimality_gap = std::max(
                    0.0,
                    result.diagnostics.focused_upper_bound -
                        result.diagnostics.focused_lower_bound);
            }
        }
        const bool target_reached =
            succeeded && focused_pending_upper_complete &&
            stop_for_satisfied_gap_target();
        if (incremental_pass) {
            const bool improved =
                succeeded && focused_pending_upper_complete &&
                output_incumbent.has_value() &&
                output_incumbent->certified_upper_bound <
                    incremental_upper_policy_prior_bound -
                        value_comparison_tolerance(
                            incremental_upper_policy_prior_bound);
            std::unordered_set<std::uint64_t> selected_temporary;
            if (improved) {
                selected_temporary.reserve(policy_rows.size());
                for (const std::uint64_t row : policy_rows) {
                    if (row !=
                        std::numeric_limits<std::uint64_t>::max()) {
                        selected_temporary.insert(row);
                    }
                }
            }
            std::unordered_set<std::uint64_t> promoted_temporary;
            if (improved) {
                promoted_temporary.reserve(
                    incremental_upper_temporary_rows.size());
            }
            for (const std::uint64_t row :
                 incremental_upper_temporary_rows) {
                const bool promote =
                    improved && selected_temporary.contains(row);
                transition_cache->rows.at(row).admitted = promote;
                if (promote) promoted_temporary.insert(row);
            }
            /* Row identity is unique in the action envelope. Restore every
             * temporary row first, then match promotions in one stable pass;
             * the former per-row linear search was quadratic at upper-pass
             * publication boundaries. Erasing preserves the old first-match
             * behavior if malformed duplicate records ever appear. */
            for (IncrementalAlternativeRow& candidate :
                 incremental_alternative_rows) {
                if (promoted_temporary.erase(candidate.row_index) == 0) {
                    continue;
                }
                candidate.status =
                    IncrementalAlternativeRow::Status::Admitted;
                candidate.improvement_margin =
                    incremental_upper_policy_prior_bound -
                    output_incumbent->certified_upper_bound;
                ++incremental_upper_policy_updates;
            }
            incremental_upper_temporary_rows.clear();
            incremental_reclassify_all = true;
            /* The temporary fixed-policy pass tests rows against the current
             * incumbent one at a time. Before restoring the lower snapshot,
             * also give the completed rows an amortized chance to form a
             * jointly proper executable policy. This is upper-only: every
             * row is restored to its prior admission status and the open
             * action envelope remains open. */
            (void)maybe_install_incremental_anytime_incumbent();
        }
        result.values = std::move(focused_round_lower_values);
        policy_rows = std::move(focused_round_lower_policy_rows);
        if (incremental_pass) {
            result.expanded = expanded;
            result.expanded.resize(calc.state_count(), 0);
        }
        focused_frontier_upper_operator.clear();
        focused_upper_mode = false;
        incremental_upper_policy_pass = false;
        incremental_upper_fixed_policy_proved = false;
        policy_iteration_failed = false;
        policy_initialized = true;
        policy_stable = true;
        reset_policy_iteration_units();
        if (target_reached) return;
        if (incremental_pass) {
            focus_optimizing = false;
            focused_lower_mode = false;
            incremental_restricted_values_ready = true;
            /* A host-requested bounded publication is latched while this
             * upper pass owns temporary admissions. Stop only after the pass
             * has restored/promoted them and released policy scratch; do not
             * schedule the next open-envelope alternative. */
            if (requested_bounded_finish) {
                phase = SolvePhase::Done;
                return;
            }
            begin_incremental_post_upper_scheduling();
            return;
        }
        finish_focused_lower_solve(false);
    }

void SolveWork::Impl::run_focused_lower_unit() {
        if (!policy_iteration_failed) {
            if (!run_policy_iteration_unit()) {
                if (focused_upper_mode) {
                    finish_focused_upper_solve(false);
                    return;
                }
                /*
                 * An incomplete lower policy is still a valid lower-bound
                 * snapshot. The high-impact experiment can independently
                 * evaluate its complete executable upper witness before the
                 * legacy focused fallback releases the broad fringe.
                 */
                if (incremental_action_generation &&
                    !incremental_envelope_closed &&
                    begin_incremental_upper_policy_pass()) {
                    return;
                }
                /* Focused lower bounds cannot use the descending prioritized
                 * fallback. Preserve the bound and resume full expansion.
                 * The absence of a focused queue is not a closure proof: make
                 * every discovered strict frontier state available before
                 * leaving focused mode. */
                queue.clear();
                queued.assign(calc.state_count(), 0);
                for (std::uint32_t state = 0;
                     state < calc.state_count(); ++state) {
                    if (state < expanded.size() && expanded[state]) {
                        queued[state] = 1;
                    } else {
                        enqueue(state);
                    }
                }
                full_closure_after_focused_fallback = false;
                focus_optimizing = false;
                focused_lower_mode = false;
                focused_mode = false;
                policy_iteration_failed = false;
                policy_initialized = false;
                policy_stable = false;
                reset_policy_iteration_units();
                return;
            }
        }
        if (!backup_active && numerical_stability_stop) {
            if (focused_upper_mode) {
                /* A numerical stop rejects this temporary upper-policy
                 * experiment; it does not terminate the surrounding finite
                 * action envelope. Finalizing the pass rolls back its
                 * temporary rows and lets the incremental scheduler continue
                 * with the next candidate. */
                finish_focused_upper_solve(false);
                return;
            }
            if (continue_open_incremental_envelope()) {
                return;
            }
            phase = SolvePhase::Done;
            return;
        }
        if (!backup_active && focused_upper_mode &&
            incremental_upper_policy_pass &&
            incremental_upper_fixed_policy_proved) {
            /* A completed exact proper fixed-policy evaluation is the whole
             * executable-upper proof. Its inherited Bellman residual is an
             * optimality measure for the open partial graph and must neither
             * trigger Howard improvement nor prevent witness publication. */
            finish_focused_upper_solve(true);
            return;
        }
        if (!backup_active && optimization_converged()) {
            if (focused_upper_mode) {
                finish_focused_upper_solve(true);
            } else if (incremental_action_generation &&
                       !incremental_envelope_closed) {
                ++result.diagnostics.focused_expansion_rounds;
                result.diagnostics.focused_lower_bound =
                    result.values.at(result.start_state);
                if (output_incumbent.has_value()) {
                    result.diagnostics.focused_upper_bound =
                        output_incumbent->certified_upper_bound;
                    result.diagnostics.focused_optimality_gap = std::max(
                        0.0,
                        result.diagnostics.focused_upper_bound -
                            result.diagnostics.focused_lower_bound);
                }
                if (continue_open_incremental_envelope()) return;
                /* classify_incremental_alternatives() may close the last
                 * delayed-action envelope in this optimized focused round.
                 * Give the now-complete graph its normal focused closure or
                 * direct-upper proof. */
                if (incremental_envelope_closed) {
                    finish_focused_lower_solve();
                } else {
                    phase = SolvePhase::Done;
                }
            } else {
                finish_focused_lower_solve();
            }
        }
    }

}
}
