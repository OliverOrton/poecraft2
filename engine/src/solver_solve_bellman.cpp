#include "solver_solve_types.hpp"
#include "solver_sparse_policy.hpp"

namespace poecraft {
namespace solver {

using namespace solve_detail;

namespace solve_detail {

WideFloat::WideFloat(const double value) : high(value) {}

WideFloat::WideFloat(const double high_value, const double low_value)
        : high(high_value), low(low_value) {}

double WideFloat::value() const { return high + low; }

std::pair<double, double> exact_sum(const double a, const double b) {
    const double sum = a + b;
    const double b_virtual = sum - a;
    const double error =
        (a - (sum - b_virtual)) + (b - b_virtual);
    return {sum, error};
}

WideFloat wide_normalize(const double high, const double low) {
    const auto [sum, error] = exact_sum(high, low);
    return {sum, error};
}

WideFloat operator+(const WideFloat a, const WideFloat b) {
    const auto [sum, error] = exact_sum(a.high, b.high);
    return wide_normalize(sum, error + a.low + b.low);
}

WideFloat operator-(const WideFloat value) {
    return {-value.high, -value.low};
}

WideFloat operator-(const WideFloat a, const WideFloat b) {
    return a + (-b);
}

WideFloat& operator+=(WideFloat& target, const WideFloat value) {
    target = target + value;
    return target;
}

WideFloat& operator-=(WideFloat& target, const WideFloat value) {
    target = target - value;
    return target;
}

WideFloat operator*(const WideFloat a, const WideFloat b) {
    const double product = a.high * b.high;
    constexpr double split = 134217729.0; /* 2^27 + 1 */
    const double a_split = split * a.high;
    const double a_high = a_split - (a_split - a.high);
    const double a_low = a.high - a_high;
    const double b_split = split * b.high;
    const double b_high = b_split - (b_split - b.high);
    const double b_low = b.high - b_high;
    const double product_error =
        ((a_high * b_high - product) + a_high * b_low +
         a_low * b_high) +
        a_low * b_low;
    const double error = product_error + a.high * b.low +
                         a.low * b.high + a.low * b.low;
    return wide_normalize(product, error);
}

WideFloat operator*(const double a, const WideFloat b) {
    return WideFloat{a} * b;
}

WideFloat operator*(const WideFloat a, const double b) {
    return a * WideFloat{b};
}

WideFloat operator/(const WideFloat numerator, const WideFloat denominator) {
    const double first = numerator.high / denominator.high;
    const WideFloat remainder = numerator - denominator * first;
    const double second = remainder.value() / denominator.high;
    const WideFloat partial = WideFloat{first} + WideFloat{second};
    const WideFloat final_remainder = numerator - denominator * partial;
    return partial + WideFloat{final_remainder.value() / denominator.high};
}

}

namespace {

/*
 * Tiny diagnostic solves retain their historical final focused measurement:
 * its work is strictly bounded by this fixed graph size and several unit
 * tests inspect the resulting fallback diagnostics. Portfolio-scale caps
 * stop without reoptimizing the entire final partial graph.
 */
constexpr std::uint32_t kSynchronousCapFinalFocusStates = 8;

}

bool SolveWork::Impl::optimization_converged() const {
        if (policy_iteration_failed) {
            return residual <= options.epsilon;
        }
        return policy_initialized && policy_stable &&
               policy_strict_order_reconciled &&
               residual <= acceptable_residual();
    }

void SolveWork::Impl::prepare_iteration() {
        const auto started = std::chrono::steady_clock::now();
        if (!cache_pending &&
            expanded_count >= options.max_expanded_states &&
            calc.state_count() > expanded_count) {
            /* A focused batch can consume its last scheduled state exactly as
             * it reaches the expansion cap. Queue emptiness is not a closure
             * certificate: generated strict successors still witness the
             * unfinished graph. Record the cap before outer quotienting can
             * reduce the visible expanded-class count. */
            result.diagnostics.state_cap_hit = true;
            record_cap("max_expanded_states", true);
        }
        result.diagnostics.expanded_states = expanded_count;

        if (cache_pending) {
            expanded = transition_cache->expanded;
        } else {
            transition_cache->discovered_states = calc.state_count();
            transition_cache->expanded_states = expanded_count;
            transition_cache->expanded = expanded;
            transition_cache->expanded.resize(
                transition_cache->discovered_states, 0);
            transition_cache->state_rows.resize(
                transition_cache->discovered_states);
        }
        if (incremental_action_generation &&
            !incremental_envelope_closed) {
            const std::uint32_t state_count =
                transition_cache->discovered_states;
            result.diagnostics.strict_discovered_states = state_count;
            result.diagnostics.quotient_states = state_count;
            transition_cache->strict_discovered_states = state_count;
            transition_cache->quotient_states = state_count;
            transition_cache->exact_quotient = false;
            transition_cache->behavioral_representative_by_state.clear();
            result.behavioral_representative_by_state.clear();
        } else if (price_bound_state_pruning) {
            /* Certified graphs contain every row needed by the exact current
             * optimum, but deliberately omit price-dominated action rows.
             * Keep their concrete state identities and never present the
             * retained subset as an all-action behavioral quotient. */
            const std::uint32_t state_count =
                transition_cache->discovered_states;
            result.diagnostics.strict_discovered_states = state_count;
            result.diagnostics.quotient_states = state_count;
            transition_cache->strict_discovered_states = state_count;
            transition_cache->quotient_states = state_count;
            transition_cache->exact_quotient = false;
            transition_cache->behavioral_representative_by_state.clear();
            result.behavioral_representative_by_state.clear();
        } else if (cache_pending && transition_cache->exact_quotient &&
            !transition_cache->behavioral_representative_by_state.empty()) {
            /* A retained price-only graph already owns its exact quotient.
             * Re-refining representative-only rows would treat lifted strict
             * members as unexpanded frontier and erase their policy/value
             * lift on the second solve. */
            result.diagnostics.strict_discovered_states =
                transition_cache->strict_discovered_states;
            result.diagnostics.quotient_states =
                transition_cache->quotient_states;
        } else {
            prepare_exact_outer_quotient();
        }
        result.diagnostics.expanded_states = expanded_count;
        const std::uint32_t state_count = transition_cache->discovered_states;
        result.diagnostics.discovered_states =
            transition_cache->quotient_states == 0
                ? state_count
                : transition_cache->quotient_states;
        result.diagnostics.frontier_states =
            result.diagnostics.discovered_states - expanded_count;
        expanded.resize(state_count, 0);
        result.expanded = expanded;
        if (focused_bound_proved &&
            focused_previous_upper_values.size() == state_count) {
            result.values = std::move(focused_previous_upper_values);
        } else {
            result.values.assign(state_count, kValueCeiling);
        }
        result.values.resize(state_count, kValueCeiling);
        result.policy.assign(state_count, PolicyOperatorRef{});
        result.unveil_preferences.assign(state_count, {});
        result.option_unveil_preferences.assign(state_count, {});
        result.goal_states.assign(state_count, 0);
        for (std::uint32_t state = 0; state < state_count; ++state) {
            if (!result.behavioral_representative_by_state.empty() &&
                result.behavioral_representative_by_state[state] != state) {
                result.values[state] = kInfinity;
                continue;
            }
            if (calc.is_goal_state(calc.state(state))) {
                result.goal_states[state] = 1;
                ++result.diagnostics.goal_states;
                result.values[state] = 0.0;
            } else if (!result.expanded[state] && !focused_bound_proved) {
                /* Past-the-cap frontier: no Bellman backing, keep infinite so
                 * plans through it are never preferred. */
                result.values[state] = kInfinity;
            }
        }
        residual = kValueCeiling;
        phase = SolvePhase::Iterating;
        result.diagnostics.sparse_rows = transition_cache->rows.size();
        result.diagnostics.sparse_transitions =
            transition_cache->successors.size() +
            transition_cache->choice_successors.size();
        result.diagnostics.algebraic_self_loops =
            transition_cache->algebraic_self_loops;
        result.diagnostics.reforge_frontier_work =
            calc.telemetry().reforge_frontier_work;
        result.diagnostics.reforge_logical_work_v1 =
            calc.telemetry().reforge_logical_work_v1;
        prepare_priced_rows();
        if (focused_bound_proved) {
            const std::uint64_t no_row =
                std::numeric_limits<std::uint64_t>::max();
            for (std::uint32_t state = 0; state < state_count; ++state) {
                if (result.goal_states[state]) continue;
                std::uint32_t operator_index =
                    restart_row_allowed(state)
                        ? restart_operator_index
                        : kNoId;
                if (state <
                        focused_previous_frontier_upper_operator.size() &&
                    focused_previous_frontier_upper_operator[state] !=
                        kNoId) {
                    operator_index =
                        focused_previous_frontier_upper_operator[state];
                }
                if (state < policy_rows.size() &&
                    policy_rows[state] != no_row &&
                    policy_rows[state] < priced_rows.size()) {
                    operator_index =
                        priced_rows[policy_rows[state]].operator_index;
                }
                if (operator_index != kNoId) {
                    result.policy[state] = PolicyOperatorRef{
                        calc.operators()[operator_index].kind,
                        operator_index};
                }
            }
            /* The equal constructive upper and global lower bounds are the
             * convergence proof. prepare_iteration() normally resets these
             * fields for a new Bellman phase; preserve the proved terminal
             * state instead of reporting an artificial ceiling residual. */
            residual = 0.0;
            result.diagnostics.residual = 0.0;
            policy_initialized = true;
            policy_stable = true;
            policy_iteration_failed = false;
            backup_active = false;
        }
        const auto final_cap_audit_started =
            std::chrono::steady_clock::now();
        check_solver_byte_cap();
        result.diagnostics.expansion_finalize_byte_audit_ns +=
            static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() -
                    final_cap_audit_started)
                    .count());
        /* The solve now owns the compact CSR rows used by every later phase;
         * release evaluator distributions so transitions are stored once. A
         * reusable price-only context is the separate S7.5 cache-reuse pass. */
        if (!incremental_action_generation ||
            incremental_envelope_closed) {
            calc.release_solve_transition_caches();
            if (!cache_pending && !result.diagnostics.state_cap_hit &&
                !result.diagnostics.resource_cap_hit &&
                queue.empty() && !focused_bound_proved &&
                !price_bound_state_pruning) {
                transition_cache->focused_partial = focused_mode;
                calc.retain_solve_transition_cache(transition_cache);
            }
            kernel_rows_by_hash.clear();
            kernel_rows_by_hash.rehash(0);
            owned_kernel_row_bucket_bytes = 0;
            shared_kernel_rows.clear();
            shared_kernel_rows.rehash(0);
        }
        cache_pending = false;
        const auto peak_byte_audit_started =
            std::chrono::steady_clock::now();
        peak_owned_bytes = std::max(
            peak_owned_bytes, estimated_owned_bytes());
        result.diagnostics.expansion_finalize_byte_audit_ns +=
            static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() -
                    peak_byte_audit_started)
                    .count());
        result.diagnostics.expansion_finalize_ns +=
            static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() - started)
                    .count());
        result.diagnostics.expansion_ns += static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - started)
                .count());
        if (focused_bound_proved &&
            (!incremental_action_generation ||
             incremental_envelope_closed)) {
            phase = SolvePhase::Done;
        }
    }

double SolveWork::Impl::operator_q(
        const std::uint32_t state,
        const PricedOperator& priced) {
        const PlannerOperator& planner = calc.operators().at(priced.index);
        if (planner.kind == PlannerOperatorKind::FixedOption) {
            const OptionKernel& kernel =
                calc.option_kernel(state, priced.index);
            if (!kernel.supported || !kernel.legal) return kInfinity;
            double expected = 0.0;
            const auto& resources =
                calc.is_state_local_automatic_operator(priced.index)
                    ? planner.resource_quantities
                    : kernel.expected_resources;
            for (const auto& [key, quantity] : resources) {
                const auto found = std::find_if(
                    priced.resource_prices.begin(),
                    priced.resource_prices.end(),
                    [&](const auto& price) { return price.first == key; });
                if (found == priced.resource_prices.end()) return kInfinity;
                expected += quantity * found->second;
            }
            for (const OutcomeEntry& exit : kernel.exits) {
                const std::uint32_t successor =
                    exit.state == kNoId ? state : exit.state;
                const double value = result.values[successor];
                if (value == kInfinity) return kInfinity;
                expected += exit.probability * value;
            }
            for (const OutcomeChoiceGroup& group :
                 kernel.observation_choice_groups) {
                double best = kInfinity;
                for (const std::uint32_t successor : group.states) {
                    best = std::min(
                        best,
                        result.values[successor == kNoId ? state
                                                         : successor]);
                }
                if (best == kInfinity) return kInfinity;
                expected += group.probability * best;
            }
            return expected;
        }
        const std::uint32_t action_index = planner.primitive_action;
        if (!action_legal(session, calc.registry().actions[action_index],
                          calc.state(state))) {
            return kInfinity;
        }
        const OutcomeDistribution& distribution =
            calc.outcomes(
                state, action_index,
                options.goal_progress_gated_reforges);
        if (!distribution.supported) return kInfinity;
        double expected = priced.cost;
        if (!distribution.choice_groups.empty()) {
            for (const OutcomeChoiceGroup& group :
                 distribution.choice_groups) {
                double best = kInfinity;
                for (std::uint32_t successor : group.states) {
                    best = std::min(best, result.values[successor]);
                }
                if (best == kInfinity) return kInfinity;
                expected += group.probability * best;
            }
            return expected;
        }
        for (const OutcomeEntry& entry : distribution.entries) {
            const double value = result.values[entry.state];
            if (value == kInfinity) return kInfinity;
            expected += entry.probability * value;
        }
        return expected;
    }

void SolveWork::Impl::reset_kernel_value_cache(bool active ) {
        kernel_value_cache_active = active;
        kernel_value_caches.clear();
        kernel_value_cache_by_offset.clear();
        owned_kernel_value_cache_nested_bytes = 0;
    }

auto SolveWork::Impl::value_cache_for(const SparseRow& row) -> KernelValueCache& {
        const auto found =
            kernel_value_cache_by_offset.find(row.transition_offset);
        if (found != kernel_value_cache_by_offset.end()) {
            return kernel_value_caches.at(found->second);
        }
        KernelValueCache cache;
        cache.transition_offset = row.transition_offset;
        cache.transition_count = row.transition_count;
        std::uint32_t previous = 0;
        bool first = true;
        for (std::uint32_t i = 0; i < row.transition_count; ++i) {
            const std::uint64_t offset = row.transition_offset + i;
            const std::uint32_t successor =
                transition_cache->successors.at(offset);
            const double probability =
                transition_cache->probabilities.at(offset);
            if (!first && successor < previous) {
                cache.sorted_successors = false;
            }
            first = false;
            previous = successor;
            const double value = result.values.at(successor);
            if (value == kInfinity) ++cache.infinite_count;
            else cache.finite_sum += probability * value;
        }
        const std::size_t index = kernel_value_caches.size();
        kernel_value_caches.push_back(std::move(cache));
        kernel_value_cache_by_offset.emplace(row.transition_offset, index);
        return kernel_value_caches.back();
    }

void SolveWork::Impl::update_kernel_value_cache(
        const std::uint32_t state,
        const double before,
        const double after) {
        if (!kernel_value_cache_active || before == after) return;
        for (KernelValueCache& cache : kernel_value_caches) {
            const auto begin = transition_cache->successors.begin() +
                               cache.transition_offset;
            const auto end = begin + cache.transition_count;
            double probability = 0.0;
            if (cache.sorted_successors) {
                auto found = std::lower_bound(begin, end, state);
                while (found != end && *found == state) {
                    const std::uint64_t offset =
                        cache.transition_offset + (found - begin);
                    probability += transition_cache->probabilities.at(offset);
                    ++found;
                }
            } else {
                for (auto found = begin; found != end; ++found) {
                    if (*found != state) continue;
                    const std::uint64_t offset =
                        cache.transition_offset + (found - begin);
                    probability += transition_cache->probabilities.at(offset);
                }
            }
            if (probability == 0.0) continue;
            if (before == kInfinity) {
                --cache.infinite_count;
            } else {
                cache.finite_sum -= probability * before;
            }
            if (after == kInfinity) {
                ++cache.infinite_count;
            } else {
                cache.finite_sum += probability * after;
            }
        }
    }

double SolveWork::Impl::sparse_row_q(
        const std::size_t row_index,
        std::uint32_t& transition_work) {
        const SparseRow& row =
            transition_cache->rows.at(row_index);
        std::optional<SparsePolicyCachedTransitionValue>
            cached_transitions;
        if (kernel_value_cache_active && row.choice_count == 0 &&
            row.transition_count >= 1024) {
            KernelValueCache& cache = value_cache_for(row);
            cached_transitions =
                SparsePolicyCachedTransitionValue{
                    cache.infinite_count, cache.finite_sum};
        }
        return evaluate_sparse_policy_row(
            *transition_cache, priced_rows, result.values,
            row_index, transition_work, cached_transitions);
    }

void SolveWork::Impl::begin_policy_selection() {
        reset_kernel_value_cache(true);
        const std::uint64_t no_row =
            std::numeric_limits<std::uint64_t>::max();
        if (policy_rows.size() != result.values.size()) {
            policy_rows.assign(result.values.size(), no_row);
        }
        policy_selection_active = true;
        policy_selection_cursor = 0;
        policy_selection_states.clear();
        policy_selection_states.reserve(expanded_count);
        for (std::uint32_t state = 0;
             state < result.values.size(); ++state) {
            if (result.expanded[state] && !result.goal_states[state]) {
                policy_selection_states.push_back(state);
            }
        }
        policy_selection_improved = false;
        policy_strict_order_reconciled = true;
        policy_selection_residual = 0.0;
        PolicyRefinementTelemetry& refinement =
            result.diagnostics.policy_refinement;
        refinement.strict_order_suppressed_comparisons = 0;
        refinement.strict_order_suppressed_samples_retained = 0;
        refinement.strict_order_suppressed_samples_omitted = 0;
    }

bool SolveWork::Impl::initialize_focused_proper_policy() {
        if (!policy_selection_active) begin_policy_selection();
        const std::uint64_t transient_bytes = result.values.size() +
            policy_selection_states.size() * sizeof(std::uint32_t);
        if (check_solver_byte_cap_fast(transient_bytes)) return false;
        std::vector<std::uint8_t> assigned(result.values.size(), 0);
        std::vector<std::uint32_t> order = policy_selection_states;
        const auto start = std::find(
            order.begin(), order.end(), result.start_state);
        if (start != order.end()) {
            std::rotate(order.begin(), start, start + 1);
        }
        std::size_t remaining = order.size();
        bool progress = true;
        while (remaining != 0 && progress) {
            progress = false;
            for (const std::uint32_t state : order) {
                if (assigned[state]) continue;
                std::uint64_t best_row =
                    std::numeric_limits<std::uint64_t>::max();
                for (const std::uint64_t absolute :
                     state_row_indices(*transition_cache, state)) {
                    if (preservation_prunes(absolute)) continue;
                    const SparseRow& row =
                        transition_cache->rows.at(absolute);
                    if (!row.admitted) continue;
                    bool valid = true;
                    bool exits_rank = false;
                    const auto route = [&](const std::uint32_t successor) {
                        if (!valid || successor == state) return;
                        if (successor >= result.values.size()) {
                            valid = false;
                        } else if (result.goal_states[successor]) {
                            exits_rank = true;
                        } else if (!result.expanded[successor]) {
                            if (incremental_upper_policy_pass &&
                                (!std::isfinite(
                                     result.values[successor]) ||
                                 successor >=
                                     focused_frontier_upper_operator
                                         .size() ||
                                 focused_frontier_upper_operator[
                                     successor] == kNoId)) {
                                valid = false;
                            } else {
                                exits_rank = true;
                            }
                        } else if (assigned[successor]) {
                            exits_rank = true;
                        } else {
                            valid = false;
                        }
                    };
                    for (std::uint32_t i = 0;
                         valid && i < row.transition_count; ++i) {
                        const std::uint64_t offset = row.transition_offset + i;
                        if (transition_cache->probabilities.at(offset) > 0.0) {
                            route(transition_cache->successors.at(offset));
                        }
                    }
                    for (std::uint32_t i = 0;
                         valid && i < row.choice_count; ++i) {
                        const SparseChoiceGroup& choice =
                            transition_cache->choices.at(row.choice_offset + i);
                        if (choice.probability <= 0.0) continue;
                        const std::uint32_t selected =
                            select_sparse_policy_choice_successor(
                                *transition_cache, choice, state,
                                result.values);
                        if (selected == kNoId) valid = false;
                        else route(selected);
                    }
                    if (!valid || !exits_rank) continue;
                    /* Initialization needs any ranked proper row. Operator
                     * scheduling places deterministic finishes before broad
                     * stochastic kernels, so take the first certificate and
                     * leave cost optimality to ordinary Howard improvement. */
                    best_row = absolute;
                    break;
                }
                if (best_row == std::numeric_limits<std::uint64_t>::max()) {
                    continue;
                }
                policy_rows[state] = best_row;
                assigned[state] = 1;
                --remaining;
                progress = true;
            }
        }
        if (remaining != 0) {
            if (options.allow_economic_restart ||
                output_incumbent.has_value()) {
                return false;
            }
            /* A proper stochastic policy can contain a mutually reachable
             * retry basin with positive escape probability. The strict rank
             * construction above deliberately cannot orient such a basin.
             * Complete the seed with deterministic first-row choices so the
             * exact SCC evaluator can either prove it proper or identify a
             * closed component for repair. Do this only when restricted mode
             * has no independently certified incumbent; otherwise the normal
             * bounded publication path already owns an executable strategy
             * and this broad repair would add work without improving safety.
             * The seed is never publication authority: only the subsequent
             * fixed-policy proof may make it executable. */
            for (const std::uint32_t state : order) {
                if (assigned[state]) continue;
                std::uint64_t seed_row =
                    std::numeric_limits<std::uint64_t>::max();
                for (const std::uint64_t absolute :
                     state_row_indices(*transition_cache, state)) {
                    if (preservation_prunes(absolute) ||
                        !transition_cache->rows.at(absolute).admitted) {
                        continue;
                    }
                    seed_row = absolute;
                    break;
                }
                if (seed_row ==
                    std::numeric_limits<std::uint64_t>::max()) {
                    return false;
                }
                policy_rows[state] = seed_row;
            }
        }
        policy_selection_active = false;
        policy_selection_cursor = 0;
        policy_selection_states.clear();
        policy_selection_improved = false;
        policy_selection_residual = 0.0;
        return true;
    }

bool SolveWork::Impl::advance_policy_selection(bool& improved) {
        if (!policy_selection_active) begin_policy_selection();
        const std::uint64_t no_row =
            std::numeric_limits<std::uint64_t>::max();
        constexpr std::uint32_t kStatesPerSelectionUnit = 128;
        const std::uint32_t end = std::min<std::uint32_t>(
            static_cast<std::uint32_t>(policy_selection_states.size()),
            policy_selection_cursor + kStatesPerSelectionUnit);
        for (std::uint32_t active = policy_selection_cursor;
             active < end; ++active) {
            const std::uint32_t state = policy_selection_states[active];
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
            const double best = selected.value;
            const std::uint64_t best_row = selected.row;
            result.diagnostics.bellman_action_evaluations +=
                selected.evaluated_rows;
            ++result.diagnostics.bellman_backups;
            if (std::isfinite(best)) {
                policy_selection_residual = std::max(
                    policy_selection_residual,
                    std::abs(result.values[state] - best));
            }
            if (best_row != no_row && policy_rows[state] != best_row) {
                bool improving = policy_rows[state] == no_row;
                if (!improving) {
                    std::uint32_t current_work = 0;
                    const double current = sparse_row_q(
                        policy_rows[state], current_work);
                    const double tolerance =
                        incremental_upper_policy_pass
                            ? value_comparison_tolerance(current)
                            : options.epsilon;
                    const SparsePolicyReplacementDecision decision =
                        sparse_policy_replacement_decision(
                            best, best_row, current,
                            policy_rows[state], tolerance);
                    improving =
                        decision ==
                        SparsePolicyReplacementDecision::Replace;
                    if (decision == SparsePolicyReplacementDecision::
                            SuppressedStrictImprovement) {
                        policy_strict_order_reconciled = false;
                        PolicyRefinementTelemetry& refinement =
                            result.diagnostics.policy_refinement;
                        ++refinement.strict_order_suppressed_comparisons;
                        if (refinement
                                .strict_order_suppressed_samples_retained <
                            PolicyRefinementTelemetry::
                                kSuppressedStrictImprovementSampleLimit) {
                            auto& sample =
                                refinement.strict_order_suppressed_samples[
                                    refinement
                                        .strict_order_suppressed_samples_retained++];
                            sample.state = state;
                            sample.retained_row = policy_rows[state];
                            sample.preferred_row = best_row;
                            sample.retained_value = current;
                            sample.preferred_value = best;
                        } else {
                            ++refinement
                                  .strict_order_suppressed_samples_omitted;
                        }
                    }
                }
                if (improving) {
                    policy_rows[state] = best_row;
                    policy_selection_improved = true;
                }
            }
        }
        if (policy_selection_cursor < policy_selection_states.size()) {
            policy_selection_cursor = end;
        }
        if (policy_selection_cursor < policy_selection_states.size()) {
            return false;
        }
        residual = policy_selection_residual;
        result.diagnostics.residual = residual;
        improved = policy_selection_improved;
        if (advance_unreconciled_stable_policy_latch(
                policy_selection_improved,
                policy_strict_order_reconciled,
                unreconciled_stable_policy_rounds)) {
            /* The selected row policy did not change across two complete
             * exact fixed-policy evaluations, but a strictly cheaper
             * comparison remains inside the numerical stability latch.
             * Repeating the identical policy cannot reconcile that proof
             * obligation. Stop with the independently evaluated fallback
             * as a bounded upper instead of claiming exactness or running
             * to the unrelated public sweep cap. */
            numerical_stability_stop = true;
            if (result.diagnostics.policy_evaluation_failure.empty()) {
                result.diagnostics.policy_evaluation_failure =
                    "strict_policy_order_unreconciled_at_"
                    "numerical_stability";
            }
        }
        policy_selection_active = false;
        policy_selection_states.clear();
        return true;
    }

void SolveWork::Impl::reset_policy_iteration_units() {
        policy_unit_stage = PolicyUnitStage::Seed;
        policy_seed_pass = 0;
        policy_seed_cursor = 0;
        policy_seed_states.clear();
        policy_selection_active = false;
        policy_selection_cursor = 0;
        policy_selection_states.clear();
        policy_selection_improved = false;
        policy_strict_order_reconciled = true;
        unreconciled_stable_policy_rounds = 0;
        numerical_stability_stop = false;
        policy_selection_residual = 0.0;
        sparse_policy_resume.reset();
        policy_kernel_preparation.reset();
        current_policy_scratch_bytes = 0;
        reset_kernel_value_cache();
    }

bool SolveWork::Impl::evaluate_fixed_policy() {
        reset_kernel_value_cache();
        policy_evaluation_incomplete = false;
        improper_policy_states.clear();
        if (!result.diagnostics.policy_evaluation_failure.starts_with(
                "selected_byte_cap_breakdown:")) {
            result.diagnostics.policy_evaluation_failure.clear();
        }
        const auto fail = [&](const char* reason) {
            result.diagnostics.policy_evaluation_failure = reason;
            policy_kernel_preparation.reset();
            current_policy_scratch_bytes = 0;
            sparse_policy_resume.reset();
            return false;
        };
        const std::size_t state_count = result.values.size();
        const std::uint64_t no_row =
            std::numeric_limits<std::uint64_t>::max();
        const auto byte_product = [](
            const std::size_t count, const std::size_t width) {
            return count >
                    std::numeric_limits<std::uint64_t>::max() / width
                ? std::numeric_limits<std::uint64_t>::max()
                : static_cast<std::uint64_t>(count) * width;
        };
        const auto byte_add = [](
            const std::uint64_t left, const std::uint64_t right) {
            return right >
                    std::numeric_limits<std::uint64_t>::max() - left
                ? std::numeric_limits<std::uint64_t>::max()
                : left + right;
        };

        /* Exact fixed-policy quotient. A policy state's value equation is
         * determined entirely by its immediate cost and full transition row.
         * States with byte-identical equations therefore have identical
         * values and can share one variable. Reforge-heavy policies contain
         * thousands of such states; quotienting them turns the giant retry
         * SCC into the small exact system it represents without changing a
         * probability, carrier distinction, action, or policy choice. */
        if (policy_kernel_preparation == nullptr ||
            policy_kernel_preparation->state_count != state_count) {
            policy_kernel_preparation.reset();
            sparse_policy_resume.reset();
            current_policy_scratch_bytes = 0;
            std::uint64_t initial_projection =
                sizeof(PolicyKernelPreparation);
            initial_projection = byte_add(
                initial_projection,
                byte_product(expanded_count, sizeof(std::uint32_t)));
            initial_projection = byte_add(
                initial_projection,
                byte_product(state_count, sizeof(std::uint32_t)));
            initial_projection = byte_add(
                initial_projection,
                byte_product(
                    state_count, sizeof(std::vector<PolicyEdge>)));
            if (check_solver_byte_cap_fast(initial_projection)) {
                return fail("fixed_policy_preparation_exceeds_byte_cap");
            }
            policy_kernel_preparation =
                std::make_unique<PolicyKernelPreparation>();
            policy_kernel_preparation->state_count = state_count;
            policy_kernel_preparation->active_states.reserve(
                expanded_count);
            for (std::uint32_t state = 0; state < state_count; ++state) {
                if (result.expanded[state] && !result.goal_states[state]) {
                    policy_kernel_preparation->active_states.push_back(
                        state);
                }
            }
            policy_kernel_preparation->kernel_owner.assign(
                state_count, kNoId);
            policy_kernel_preparation->full_kernel.resize(state_count);
            ++result.diagnostics.policy_evaluation_calls;
        }
        PolicyKernelPreparation& preparation =
            *policy_kernel_preparation;
        const auto refresh_policy_scratch = [&]() {
            std::uint64_t bytes = sizeof(PolicyKernelPreparation);
            const auto add = [&](const std::uint64_t amount) {
                bytes = amount >
                                std::numeric_limits<std::uint64_t>::max() -
                                    bytes
                            ? std::numeric_limits<std::uint64_t>::max()
                            : bytes + amount;
            };
            const auto add_vector = [&]
                (const std::size_t capacity,
                 const std::size_t element_size) {
                add(capacity >
                            std::numeric_limits<std::uint64_t>::max() /
                                element_size
                        ? std::numeric_limits<std::uint64_t>::max()
                        : static_cast<std::uint64_t>(capacity) *
                              element_size);
            };
            add_vector(
                preparation.active_states.capacity(),
                sizeof(std::uint32_t));
            add_vector(
                preparation.kernel_owner.capacity(),
                sizeof(std::uint32_t));
            add_vector(
                preparation.full_kernel.capacity(),
                sizeof(std::vector<PolicyEdge>));
            for (const auto& kernel : preparation.full_kernel) {
                add_vector(kernel.capacity(), sizeof(PolicyEdge));
            }
            add_vector(
                preparation.representative.capacity(),
                sizeof(std::uint32_t));
            add_vector(
                preparation.group_members.capacity(),
                sizeof(std::vector<std::uint32_t>));
            for (const auto& members : preparation.group_members) {
                add_vector(members.capacity(), sizeof(std::uint32_t));
            }
            add_vector(preparation.rows.capacity(), sizeof(PolicyRow));
            add_vector(preparation.edges.capacity(), sizeof(PolicyEdge));
            add_vector(
                preparation.component_by_state.capacity(),
                sizeof(std::uint32_t));
            add_vector(preparation.local.capacity(), sizeof(std::int32_t));
            add_vector(
                preparation.components.capacity(),
                sizeof(std::vector<std::uint32_t>));
            for (const auto& component : preparation.components) {
                add_vector(component.capacity(), sizeof(std::uint32_t));
            }
            add_vector(
                preparation.tarjan_index.capacity(),
                sizeof(std::uint32_t));
            add_vector(
                preparation.tarjan_lowlink.capacity(),
                sizeof(std::uint32_t));
            add_vector(
                preparation.tarjan_on_stack.capacity(),
                sizeof(std::uint8_t));
            add_vector(
                preparation.tarjan_stack.capacity(),
                sizeof(std::uint32_t));
            add_vector(
                preparation.tarjan_dfs.capacity(),
                sizeof(PolicyTarjanFrame));
            add(
                preparation.representatives_by_hash.bucket_count() *
                sizeof(void*));
            add(
                preparation.representatives_by_hash.size() *
                (sizeof(decltype(
                     preparation.representatives_by_hash)::value_type) +
                 2 * sizeof(void*)));
            for (const auto& [unused, entries] :
                 preparation.representatives_by_hash) {
                (void)unused;
                add_vector(entries.capacity(), sizeof(std::uint32_t));
            }
            add(
                preparation.shared_transition_representatives
                    .bucket_count() * sizeof(void*));
            add(
                preparation.shared_transition_representatives.size() *
                (sizeof(decltype(
                     preparation.shared_transition_representatives)::
                            value_type) +
                 2 * sizeof(void*)));
            for (const auto& [unused, entries] :
                 preparation.shared_transition_representatives) {
                (void)unused;
                add_vector(
                    entries.capacity(),
                    sizeof(SharedPolicyKernelRepresentative));
                for (const SharedPolicyKernelRepresentative& entry :
                     entries) {
                    add_vector(
                        entry.exact_signature.capacity(),
                        sizeof(std::uint64_t));
                }
            }
            if (sparse_policy_resume != nullptr) {
                add(sizeof(SparsePolicyResume));
                add_vector(
                    sparse_policy_resume->members.capacity(),
                    sizeof(std::uint32_t));
                const auto add_wide = [&](const auto& values) {
                    add_vector(values.capacity(), sizeof(WideFloat));
                };
                add_wide(sparse_policy_resume->b);
                add_wide(sparse_policy_resume->x);
                add_wide(sparse_policy_resume->r);
                add_wide(sparse_policy_resume->r0);
                add_wide(sparse_policy_resume->p);
                add_wide(sparse_policy_resume->v);
                add_wide(sparse_policy_resume->s);
                add_wide(sparse_policy_resume->t);
            }
            current_policy_scratch_bytes = bytes;
            peak_policy_scratch_bytes = std::max(
                peak_policy_scratch_bytes, bytes);
            return bytes;
        };
        const auto policy_scratch_within_cap = [&]
            (const std::uint64_t transient_bytes = 0) {
                const std::uint64_t retained = refresh_policy_scratch();
                peak_policy_scratch_bytes = std::max(
                    peak_policy_scratch_bytes,
                    byte_add(retained, transient_bytes));
                if (!check_solver_byte_cap_fast(transient_bytes)) {
                    return true;
                }
                /* A byte refusal is terminal for this fixed-policy attempt.
                 * Never continue to install an incumbent after a preparation
                 * or component scratch check has failed. */
                policy_evaluation_incomplete = false;
                policy_kernel_preparation.reset();
                sparse_policy_resume.reset();
                current_policy_scratch_bytes = 0;
                return false;
            };
        if (!policy_scratch_within_cap()) return false;
        std::vector<std::uint32_t>& kernel_owner =
            preparation.kernel_owner;
        std::vector<std::vector<PolicyEdge>>& full_kernel =
            preparation.full_kernel;
        auto& representatives_by_hash =
            preparation.representatives_by_hash;
        auto& shared_transition_representatives =
            preparation.shared_transition_representatives;
        const auto hash_combine = [](std::size_t& hash, std::uint64_t value) {
            hash ^= static_cast<std::size_t>(value) +
                    static_cast<std::size_t>(0x9e3779b97f4a7c15ULL) +
                    (hash << 6) + (hash >> 2);
        };
        const auto kernels_equal = [&](const std::uint32_t left,
                                       const std::uint32_t right,
                                       const std::vector<PolicyEdge>& candidate) {
            if (priced_rows.at(policy_rows[left]).cost !=
                priced_rows.at(policy_rows[right]).cost) {
                return false;
            }
            const std::vector<PolicyEdge>& stored = full_kernel[left];
            if (stored.size() != candidate.size()) return false;
            for (std::size_t i = 0; i < stored.size(); ++i) {
                if (stored[i].target != candidate[i].target ||
                    stored[i].probability != candidate[i].probability) {
                    return false;
                }
            }
            return true;
        };
        constexpr std::uint32_t kKernelStatesPerWorkUnit = 64;
        const std::uint32_t kernel_end = std::min<std::uint32_t>(
            static_cast<std::uint32_t>(preparation.active_states.size()),
            preparation.cursor + kKernelStatesPerWorkUnit);
        std::uint64_t kernel_transient = 0;
        for (std::uint32_t active = preparation.cursor;
             active < kernel_end; ++active) {
            const std::uint32_t state = preparation.active_states[active];
            if (!result.expanded[state] || result.goal_states[state] ||
                policy_rows[state] == no_row) {
                continue;
            }
            const SparseRow& sparse = transition_cache->rows.at(
                policy_rows[state]);
            const std::uint64_t selected =
                static_cast<std::uint64_t>(sparse.choice_count) *
                    sizeof(std::uint32_t) +
                static_cast<std::uint64_t>(
                    6 + 2 * sparse.choice_count) *
                    sizeof(std::uint64_t) +
                static_cast<std::uint64_t>(
                    sparse.transition_count + sparse.choice_count + 1) *
                    sizeof(PolicyEdge);
            kernel_transient = std::max(kernel_transient, selected);
        }
        if (!policy_scratch_within_cap(kernel_transient)) return false;
        for (std::uint32_t active = preparation.cursor;
             active < kernel_end; ++active) {
            const std::uint32_t state = preparation.active_states[active];
            if (!result.expanded[state] || result.goal_states[state]) continue;
            if (policy_rows[state] == no_row) continue;
            const std::uint64_t row_index = policy_rows[state];
            const SparseRow& sparse = transition_cache->rows.at(row_index);
            std::vector<std::uint32_t> selected_choices;
            selected_choices.reserve(sparse.choice_count);
            const double detached_self_probability =
                sparse.self_probability -
                sparse.embedded_self_probability;
            std::vector<std::uint64_t> shared_signature{
                std::bit_cast<std::uint64_t>(
                    priced_rows.at(row_index).cost),
                sparse.transition_offset,
                sparse.transition_count,
                sparse.choice_count,
                detached_self_probability > 0.0 ? state : kNoId,
                std::bit_cast<std::uint64_t>(
                    detached_self_probability)};
            for (std::uint32_t i = 0; i < sparse.choice_count; ++i) {
                const SparseChoiceGroup& choice =
                    transition_cache->choices.at(sparse.choice_offset + i);
                const std::uint32_t selected =
                    select_sparse_policy_choice_successor(
                        *transition_cache, choice, state,
                        result.values);
                if (selected == kNoId) return fail("empty_observation_choice");
                selected_choices.push_back(selected);
                shared_signature.push_back(selected);
                shared_signature.push_back(
                    std::bit_cast<std::uint64_t>(choice.probability));
            }
            std::size_t shared_hash = static_cast<std::size_t>(
                1469598103934665603ULL);
            for (const std::uint64_t token : shared_signature) {
                hash_combine(shared_hash, token);
            }
            std::uint32_t shared_owner = kNoId;
            for (const SharedPolicyKernelRepresentative& possible :
                 shared_transition_representatives[shared_hash]) {
                if (possible.exact_signature == shared_signature) {
                    shared_owner = possible.state;
                    break;
                }
            }
            if (shared_owner != kNoId) {
                kernel_owner[state] = shared_owner;
                continue;
            }
            std::vector<PolicyEdge> candidate;
            candidate.reserve(
                sparse.transition_count + sparse.choice_count + 1);
            for (std::uint32_t i = 0; i < sparse.transition_count; ++i) {
                const std::uint64_t offset = sparse.transition_offset + i;
                const double probability =
                    transition_cache->probabilities.at(offset);
                if (probability == 0.0) continue;
                candidate.push_back({
                    transition_cache->successors.at(offset),
                    probability});
            }
            if (detached_self_probability > 0.0) {
                const auto self_position = std::lower_bound(
                    candidate.begin(), candidate.end(), state,
                    [](const PolicyEdge& edge, const std::uint32_t target) {
                        return edge.target < target;
                    });
                if (self_position != candidate.end() &&
                    self_position->target == state) {
                    self_position->probability += detached_self_probability;
                } else {
                    candidate.insert(
                        self_position, {state, detached_self_probability});
                }
            }
            for (std::uint32_t i = 0; i < sparse.choice_count; ++i) {
                const SparseChoiceGroup& choice =
                    transition_cache->choices.at(sparse.choice_offset + i);
                candidate.push_back(
                    {selected_choices.at(i), choice.probability});
            }
            if (sparse.choice_count != 0) {
                std::sort(
                    candidate.begin(), candidate.end(),
                    [](const PolicyEdge& left, const PolicyEdge& right) {
                        return left.target < right.target;
                    });
                std::size_t merged = 0;
                for (const PolicyEdge& entry : candidate) {
                    if (entry.probability == 0.0) continue;
                    if (merged != 0 &&
                        candidate[merged - 1].target == entry.target) {
                        candidate[merged - 1].probability += entry.probability;
                    } else {
                        candidate[merged++] = entry;
                    }
                }
                candidate.resize(merged);
            }

            std::size_t hash =
                static_cast<std::size_t>(1469598103934665603ULL);
            hash_combine(
                hash, std::bit_cast<std::uint64_t>(
                          priced_rows.at(row_index).cost));
            for (const PolicyEdge& entry : candidate) {
                hash_combine(hash, entry.target);
                hash_combine(
                    hash, std::bit_cast<std::uint64_t>(entry.probability));
            }
            std::uint32_t owner = kNoId;
            for (const std::uint32_t possible :
                 representatives_by_hash[hash]) {
                if (kernels_equal(possible, state, candidate)) {
                    owner = possible;
                    break;
                }
            }
            if (owner == kNoId) {
                owner = state;
                representatives_by_hash[hash].push_back(state);
                full_kernel[state] = std::move(candidate);
            }
            shared_transition_representatives[shared_hash].push_back(
                {owner, std::move(shared_signature)});
            kernel_owner[state] = owner;
        }
        preparation.cursor = kernel_end;
        if (!policy_scratch_within_cap()) return false;
        if (preparation.cursor < preparation.active_states.size()) {
            policy_evaluation_incomplete = true;
            return false;
        }

        if (preparation.representative.empty()) {
            const std::uint64_t projected =
                static_cast<std::uint64_t>(state_count) *
                (2 * sizeof(std::uint32_t) + sizeof(PolicyRow) +
                 sizeof(std::vector<std::uint32_t>) +
                 sizeof(PolicyEdge));
            if (!policy_scratch_within_cap(projected)) return false;
            preparation.representative = kernel_owner;
            preparation.group_members.resize(state_count);
            preparation.rows.resize(state_count);
            /* The fixed policy retains one chosen row per expanded state,
             * not the entire action graph. Reserving every graph successor
             * duplicated tens of MiB at focused-quotient peak even when
             * exact kernel sharing reduced the policy to a small edge set.
             * Start with one edge per state and let the vector grow exactly
             * if a genuinely wider selected policy requires it. */
            preparation.edges.reserve(std::min<std::size_t>(
                transition_cache->successors.size(), state_count));
        }
        std::vector<std::uint32_t>& representative =
            preparation.representative;
        std::vector<std::vector<std::uint32_t>>& group_members =
            preparation.group_members;
        std::vector<PolicyRow>& rows = preparation.rows;
        std::vector<PolicyEdge>& edges = preparation.edges;
        constexpr std::uint32_t kGroupingStatesPerWorkUnit = 256;
        const std::uint32_t grouping_end = std::min<std::uint32_t>(
            static_cast<std::uint32_t>(preparation.active_states.size()),
            preparation.grouping_cursor + kGroupingStatesPerWorkUnit);
        for (std::uint32_t active = preparation.grouping_cursor;
             active < grouping_end; ++active) {
            const std::uint32_t state = preparation.active_states[active];
            if (representative[state] != kNoId) {
                group_members[representative[state]].push_back(state);
            }
            if (representative[state] == state) {
                ++result.diagnostics.policy_kernel_groups;
            } else if (representative[state] != kNoId) {
                ++result.diagnostics.policy_states_collapsed;
            }
        }
        preparation.grouping_cursor = grouping_end;
        if (!policy_scratch_within_cap()) return false;
        if (preparation.grouping_cursor < preparation.active_states.size()) {
            policy_evaluation_incomplete = true;
            return false;
        }
        constexpr std::uint32_t kQuotientStatesPerWorkUnit = 64;
        const std::uint32_t quotient_end = std::min<std::uint32_t>(
            static_cast<std::uint32_t>(preparation.active_states.size()),
            preparation.quotient_cursor + kQuotientStatesPerWorkUnit);
        std::uint64_t quotient_transient = 0;
        for (std::uint32_t active = preparation.quotient_cursor;
             active < quotient_end; ++active) {
            const std::uint32_t state = preparation.active_states[active];
            if (representative[state] != state) continue;
            quotient_transient = std::max(
                quotient_transient,
                static_cast<std::uint64_t>(
                    full_kernel[kernel_owner[state]].size()) *
                    sizeof(PolicyEdge));
        }
        if (!policy_scratch_within_cap(quotient_transient)) return false;
        for (std::uint32_t active = preparation.quotient_cursor;
             active < quotient_end; ++active) {
            const std::uint32_t state = preparation.active_states[active];
            if (representative[state] != state) continue;
            PolicyRow& row = rows[state];
            row.edge_offset = edges.size();
            row.cost = priced_rows.at(policy_rows[state]).cost;
            std::vector<PolicyEdge> quotient;
            quotient.reserve(full_kernel[kernel_owner[state]].size());
            for (PolicyEdge entry : full_kernel[kernel_owner[state]]) {
                if (entry.target < representative.size() &&
                    representative[entry.target] != kNoId) {
                    entry.target = representative[entry.target];
                }
                quotient.push_back(entry);
            }
            std::sort(
                quotient.begin(), quotient.end(),
                [](const PolicyEdge& left, const PolicyEdge& right) {
                    return left.target < right.target;
                });
            WideFloat self_probability = 0.0;
            for (std::size_t begin = 0; begin < quotient.size();) {
                const std::uint32_t target = quotient[begin].target;
                WideFloat probability = 0.0;
                std::size_t end = begin;
                while (end < quotient.size() &&
                       quotient[end].target == target) {
                    probability = probability +
                                  WideFloat{quotient[end].probability};
                    ++end;
                }
                if (target == state) {
                    self_probability = self_probability + probability;
                } else {
                    edges.push_back({target, probability.value()});
                }
                begin = end;
            }
            const double divisor =
                sparse_policy_exit_probability(self_probability);
            if (divisor > 0.0) {
                row.cost /= divisor;
                for (std::size_t edge = row.edge_offset;
                     edge < edges.size(); ++edge) {
                    edges[edge].probability /= divisor;
                }
            }
            row.edge_count = static_cast<std::uint32_t>(
                edges.size() - row.edge_offset);
        }
        preparation.quotient_cursor = quotient_end;
        if (!policy_scratch_within_cap()) return false;
        if (preparation.quotient_cursor < preparation.active_states.size()) {
            policy_evaluation_incomplete = true;
            return false;
        }
        if (!preparation.source_kernels_released) {
            preparation.kernel_owner.clear();
            preparation.kernel_owner.shrink_to_fit();
            preparation.full_kernel.clear();
            preparation.full_kernel.shrink_to_fit();
            preparation.representatives_by_hash.clear();
            preparation.representatives_by_hash.rehash(0);
            preparation.shared_transition_representatives.clear();
            preparation.shared_transition_representatives.rehash(0);
            preparation.source_kernels_released = true;
        }

        std::vector<std::vector<std::uint32_t>>& components =
            preparation.components;
        if (!preparation.components_ready) {
            constexpr std::uint32_t kTarjanWorkPerUnit = 4096;
            const SparsePolicyTarjanView tarjan_view{
                preparation.active_states,
                result.expanded,
                result.goal_states,
                policy_rows,
                representative,
                rows,
                edges};
            if (!advance_sparse_policy_components(
                    tarjan_view, preparation,
                    kTarjanWorkPerUnit)) {
                if (!policy_scratch_within_cap()) return false;
                policy_evaluation_incomplete = true;
                return false;
            }
        }
        if (!policy_scratch_within_cap()) return false;
        std::vector<std::uint32_t>& component_by_state =
            preparation.component_by_state;
        std::vector<std::int32_t>& local = preparation.local;
        refresh_policy_scratch();
        constexpr std::uint32_t kComponentsPerWorkUnit = 64;
        const std::uint32_t component_end = std::min<std::uint32_t>(
            static_cast<std::uint32_t>(components.size()),
            preparation.component_cursor + kComponentsPerWorkUnit);
        for (std::uint32_t component = preparation.component_cursor;
             component < component_end; ++component) {
            const std::vector<std::uint32_t>& members = components[component];
            const std::size_t n = members.size();
            result.diagnostics.largest_policy_component = std::max(
                result.diagnostics.largest_policy_component,
                static_cast<std::uint32_t>(n));
            bool has_exit = false;
            for (const std::uint32_t state : members) {
                const PolicyRow& row = rows[state];
                for (std::uint32_t e = 0; e < row.edge_count; ++e) {
                    const PolicyEdge& edge = edges.at(row.edge_offset + e);
                    if (result.goal_states[edge.target] ||
                        component_by_state[edge.target] != component) {
                        has_exit = true;
                        break;
                    }
                }
                if (has_exit) break;
            }
            if (!has_exit) {
                improper_policy_states.clear();
                for (const std::uint32_t state : members) {
                    improper_policy_states.insert(
                        improper_policy_states.end(),
                        group_members[state].begin(),
                        group_members[state].end());
                }
                return fail("improper_closed_component");
            }
            for (std::size_t i = 0; i < n; ++i) local[members[i]] =
                static_cast<std::int32_t>(i);
            const std::uint64_t rhs_bytes =
                byte_product(n, sizeof(double));
            std::uint64_t solve_scratch =
                sparse_policy_component_scratch_bytes(n, false);
            if (n > kDensePolicyComponentLimit &&
                sparse_policy_resume != nullptr) {
                /* The shared helper includes a worst-case retained resume.
                 * That resume is already in current_policy_scratch_bytes, so
                 * remove only its n-sized projection before adding the helper
                 * as transient call storage. Any excess retained capacities
                 * remain counted by refresh_policy_scratch(). */
                std::uint64_t retained_resume_projection =
                    sizeof(SparsePolicyResume);
                retained_resume_projection = byte_add(
                    retained_resume_projection,
                    byte_product(n, sizeof(std::uint32_t)));
                retained_resume_projection = byte_add(
                    retained_resume_projection,
                    byte_product(n, 8 * sizeof(WideFloat)));
                solve_scratch = solve_scratch > retained_resume_projection
                    ? solve_scratch - retained_resume_projection
                    : 0;
            }
            if (!policy_scratch_within_cap(
                    byte_add(rhs_bytes, solve_scratch))) {
                return false;
            }
            /* Keep policy numerics identical on native and wasm32. On x86,
             * long double uses an 80-bit accumulator while WebAssembly maps
             * it to 64-bit double, which made otherwise identical fixed
             * policies diverge beyond the approved start-value tolerance. */
            std::vector<double> rhs(n, 0.0);
            for (std::size_t i = 0; i < n; ++i) {
                const std::uint32_t state = members[i];
                double external_sum = 0.0;
                double external_correction = 0.0;
                const PolicyRow& row = rows[state];
                for (std::uint32_t e = 0; e < row.edge_count; ++e) {
                    const PolicyEdge& edge = edges.at(row.edge_offset + e);
                    if (edge.target >= state_count) {
                        return fail("successor_outside_cached_closure");
                    }
                    if (result.goal_states[edge.target]) continue;
                    /* Focused lower solves use the exact state's admissible
                     * frontier value as their terminal cost. The former zero
                     * shortcut was valid only while every frontier value was
                     * initialized to zero; retaining it after goal-cover
                     * initialization made fixed-policy evaluation disagree
                     * with the Bellman backup and re-evaluate a stable policy
                     * forever. The frontier value is included in external_sum
                     * below. */
                    if (!result.expanded[edge.target] &&
                        !focused_lower_mode) {
                        return fail("policy_reaches_unexpanded_frontier");
                    }
                    if (!std::isfinite(result.values[edge.target])) {
                        return fail("policy_reaches_unexpanded_frontier");
                    }
                    if (component_by_state[edge.target] != component) {
                        const double product =
                            edge.probability * result.values[edge.target];
                        const double adjusted =
                            product - external_correction;
                        const double updated = external_sum + adjusted;
                        external_correction =
                            (updated - external_sum) - adjusted;
                        external_sum = updated;
                    }
                }
                rhs[i] = rows[state].cost + external_sum;
            }

            const SparsePolicyComponentView component_view{
                members,
                component,
                component_by_state,
                local,
                rows,
                edges,
                rhs,
                result.values,
                options.max_sweeps};
            SparsePolicyComponentResult component_result =
                advance_sparse_policy_component(
                    component_view, sparse_policy_resume);
            if (!policy_scratch_within_cap(byte_product(
                    component_result.values.capacity(),
                    sizeof(double)))) {
                return false;
            }
            if (n > kDensePolicyComponentLimit) {
                result.diagnostics.sparse_policy_iterations +=
                    component_result.iterations;
            }
            if (component_result.status ==
                SparsePolicyComponentStatus::Incomplete) {
                policy_evaluation_incomplete = true;
                return false;
            }
            if (component_result.status ==
                SparsePolicyComponentStatus::Singular) {
                return fail("dense_policy_component_is_singular");
            }
            if (component_result.status ==
                SparsePolicyComponentStatus::DidNotConverge) {
                record_cap("max_sweeps");
                return fail(
                    "sparse_policy_component_did_not_converge");
            }
            if (component_result.status ==
                SparsePolicyComponentStatus::NumericFailure) {
                return fail(
                    "fixed_policy_component_numeric_failure");
            }
            if (n > kDensePolicyComponentLimit) {
                result.diagnostics.max_sparse_policy_iterations =
                    std::max(
                        result.diagnostics
                            .max_sparse_policy_iterations,
                        component_result.total_iterations);
            }
            const std::vector<double>& solved =
                component_result.values;
            for (std::size_t i = 0; i < n; ++i) {
                if (!std::isfinite(solved[i]) || solved[i] < -1e-8) {
                    return fail("fixed_policy_value_is_invalid");
                }
                result.values[members[i]] = std::max(0.0, solved[i]);
                for (const std::uint32_t member :
                     group_members[members[i]]) {
                    result.values[member] = result.values[members[i]];
                }
                local[members[i]] = -1;
            }
            preparation.component_cursor = component + 1;
        }
        if (preparation.component_cursor < components.size()) {
            policy_evaluation_incomplete = true;
            return false;
        }
        policy_kernel_preparation.reset();
        current_policy_scratch_bytes = 0;
        sparse_policy_resume.reset();
        return true;
    }

bool SolveWork::Impl::repair_improper_policy() {
        if (improper_policy_states.empty()) return false;
        const std::uint64_t projected_repair_bytes =
            result.values.size() >
                    std::numeric_limits<std::uint64_t>::max() /
                        (sizeof(std::uint8_t) + sizeof(double))
                ? std::numeric_limits<std::uint64_t>::max()
                : static_cast<std::uint64_t>(result.values.size()) *
                      (sizeof(std::uint8_t) + sizeof(double));
        if (check_solver_byte_cap_fast(projected_repair_bytes)) {
            return false;
        }
        std::vector<std::uint8_t> in_component(result.values.size(), 0);
        for (const std::uint32_t state : improper_policy_states) {
            in_component[state] = 1;
        }
        /*
         * The incumbent values are stale after a closed policy SCC is
         * rejected. Resolve candidate observer choices against the candidate
         * boundary condition instead: every continuation that remains in the
         * closed component is infinite, while a finite outside continuation
         * is a genuine exit. This is the row-local fixed-point decision used
         * by the subsequent policy proof, not the rejected incumbent choice.
         */
        std::vector<double> repair_values = result.values;
        for (const std::uint32_t state : improper_policy_states) {
            repair_values[state] = kInfinity;
        }
        const std::uint64_t actual_repair_bytes =
            static_cast<std::uint64_t>(in_component.capacity()) *
                sizeof(std::uint8_t) +
            static_cast<std::uint64_t>(repair_values.capacity()) *
                sizeof(double);
        if (check_solver_byte_cap_fast(actual_repair_bytes)) {
            return false;
        }
        bool repaired = false;
        for (const std::uint32_t state : improper_policy_states) {
            const SparsePolicyRowSelection selected =
                select_sparse_policy_row(
                    *transition_cache, state,
                    [&](const std::uint64_t row_index) {
                        if (preservation_prunes(row_index)) {
                            return false;
                        }
                        const SparseRow& row =
                            transition_cache->rows.at(row_index);
                        if (!row.admitted) return false;
                        bool exits = false;
                        for (std::uint32_t transition = 0;
                             transition < row.transition_count;
                             ++transition) {
                            const std::uint32_t successor =
                                transition_cache->successors.at(
                                    row.transition_offset +
                                    transition);
                            if (successor == state) continue;
                            if (result.goal_states[successor] ||
                                !in_component[successor]) {
                                exits = true;
                                break;
                            }
                        }
                        for (std::uint32_t choice_index = 0;
                             !exits &&
                             choice_index < row.choice_count;
                             ++choice_index) {
                            const SparseChoiceGroup& choice =
                                transition_cache->choices.at(
                                    row.choice_offset +
                                    choice_index);
                            const std::uint32_t successor =
                                select_sparse_policy_choice_successor(
                                    *transition_cache, choice, state,
                                    repair_values);
                            exits = successor != kNoId &&
                                    (result.goal_states[successor] ||
                                     !in_component[successor]);
                        }
                        return exits;
                    },
                    [&](const std::uint64_t row,
                        std::uint32_t& work) {
                        return evaluate_sparse_policy_row(
                            *transition_cache, priced_rows,
                            repair_values,
                            static_cast<std::size_t>(row),
                            work);
                    });
            const std::uint64_t best_row = selected.row;
            if (best_row != std::numeric_limits<std::uint64_t>::max()) {
                policy_rows[state] = best_row;
                repaired = true;
            }
        }
        if (!repaired) return false;
        /* Fixed rows may contain observed choices. Seed the rejected closed
         * component at infinity so the next exact kernel preparation keeps
         * the certified outside choices used by repair_values instead of
         * recreating the same closed observer cycle from stale finite values. */
        for (const std::uint32_t state : improper_policy_states) {
            result.values[state] = kInfinity;
        }
        improper_policy_states.clear();
        return true;
    }

bool SolveWork::Impl::run_policy_iteration_unit() {
        const auto started = std::chrono::steady_clock::now();
        const auto finish_unit = [&]() {
            result.diagnostics.optimization_ns +=
                static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now() - started)
                        .count());
        };
        if (policy_unit_stage == PolicyUnitStage::Seed) {
            /* A few alternating algebraic Gauss-Seidel passes propagate the
             * proper restart/goal bound before Howard initialization. Starting
             * directly from the finite ceiling can otherwise choose a closed
             * cross-state cycle whose one-step Q is finite but whose fixed
             * policy is improper. This is a bounded seed, not the convergence
             * algorithm. */
            if (policy_seed_states.empty()) {
                policy_seed_states.reserve(expanded_count);
                for (std::uint32_t state = 0;
                     state < result.values.size(); ++state) {
                    if (result.expanded[state] && !result.goal_states[state]) {
                        policy_seed_states.push_back(state);
                    }
                }
            }
            if (policy_seed_cursor == 0) {
                reset_kernel_value_cache(true);
            }
            constexpr std::uint32_t kStatesPerSeedUnit = 128;
            const std::uint32_t end = std::min<std::uint32_t>(
                static_cast<std::uint32_t>(policy_seed_states.size()),
                policy_seed_cursor + kStatesPerSeedUnit);
            for (std::uint32_t offset = policy_seed_cursor;
                 offset < end; ++offset) {
                const std::uint32_t state =
                        policy_seed_pass % 2 == 0
                            ? policy_seed_states[
                                  policy_seed_states.size() - 1 - offset]
                            : policy_seed_states[offset];
                std::uint32_t work = 0;
                const double best = backup_state(state, work);
                if (best < result.values[state]) {
                    update_kernel_value_cache(
                        state, result.values[state], best);
                    result.values[state] = best;
                }
            }
            policy_seed_cursor = end;
            if (policy_seed_cursor >= policy_seed_states.size()) {
                policy_seed_cursor = 0;
                if (++policy_seed_pass >= 4) {
                    policy_seed_states.clear();
                    policy_unit_stage = PolicyUnitStage::InitialSelect;
                }
            }
            backup_active = true;
            finish_unit();
            return true;
        }
        if (policy_unit_stage == PolicyUnitStage::InitialSelect) {
            if (focused_lower_mode && !policy_initialized &&
                initialize_focused_proper_policy()) {
                policy_initialized = true;
                policy_stable = false;
                policy_unit_stage = PolicyUnitStage::Evaluate;
                backup_active = true;
                finish_unit();
                return true;
            }
            bool unused_improved = false;
            if (!advance_policy_selection(unused_improved)) {
                backup_active = true;
                finish_unit();
                return true;
            }
            policy_initialized = true;
            policy_stable = false;
            policy_unit_stage = PolicyUnitStage::Evaluate;
            backup_active = true;
            finish_unit();
            return true;
        }
        if (policy_unit_stage == PolicyUnitStage::Evaluate &&
            !evaluate_fixed_policy()) {
            if (policy_evaluation_incomplete) {
                backup_active = true;
                finish_unit();
                return true;
            }
            if (repair_improper_policy()) {
                policy_stable = false;
                ++sweeps;
                result.diagnostics.sweeps = sweeps;
                result.diagnostics.policy_improvement_rounds = sweeps;
                policy_unit_stage = PolicyUnitStage::Evaluate;
                finish_unit();
                backup_active = sweeps < options.max_sweeps;
                return true;
            }
            policy_iteration_failed = true;
            policy_stable = false;
            result.diagnostics.policy_iteration_fallback = true;
            backup_active = false;
            finish_unit();
            return false;
        }
        if (policy_unit_stage == PolicyUnitStage::Evaluate) {
            if (incremental_upper_policy_pass) {
                /*
                 * The current policy has just been evaluated exactly and
                 * proved proper. That is the complete proof required for an
                 * executable upper witness: Bellman improvement would only
                 * optimize this open partial graph and is neither required
                 * for feasibility nor authority to close the action envelope.
                 * In particular, `residual` still describes the pre-evaluation
                 * seed/selection comparison and may be large even though the
                 * selected fixed policy now has exact finite values.
                 */
                ++incremental_upper_policy_fixed_policy_proofs;
                incremental_upper_fixed_policy_proved = true;
                policy_stable = true;
                backup_active = false;
                finish_unit();
                return true;
            }
            policy_unit_stage = PolicyUnitStage::ImproveSelect;
            backup_active = true;
            finish_unit();
            return true;
        }
        bool improved = false;
        if (!advance_policy_selection(improved)) {
            backup_active = true;
            finish_unit();
            return true;
        }
        policy_stable = !improved;
        ++sweeps;
        result.diagnostics.sweeps = sweeps;
        result.diagnostics.policy_improvement_rounds = sweeps;
        ++result.diagnostics.bellman_work_units;
        policy_unit_stage = PolicyUnitStage::Evaluate;
        finish_unit();
        if (numerical_stability_stop ||
            (policy_stable && residual <= acceptable_residual()) ||
            sweeps >= options.max_sweeps) {
            backup_active = false;
        } else {
            backup_active = true;
        }
        return true;
    }

double SolveWork::Impl::backup_state(
        const std::uint32_t state,
        std::uint32_t& transition_work) {
        ++result.diagnostics.bellman_backups;
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
        transition_work = selected.transition_work;
        result.diagnostics.bellman_action_evaluations +=
            selected.evaluated_rows;
        return selected.value;
    }

void SolveWork::Impl::begin_priority_measurement() {
        reset_kernel_value_cache();
        backup_active = true;
        backup_stage = BackupStage::Measure;
        backup_cursor = 0;
        measured_residual = 0.0;
        prioritized_states.clear();
    }

void SolveWork::Impl::run_bellman_unit() {
        const auto started = std::chrono::steady_clock::now();
        if (!backup_active) begin_priority_measurement();
        const std::uint32_t state_count =
            static_cast<std::uint32_t>(result.values.size());
        constexpr std::uint32_t kStatesPerUnit = 256;
        constexpr std::uint32_t kTransitionsPerUnit = 4096;
        std::uint32_t states_done = 0;
        std::uint32_t transitions_done = 0;
        const std::uint32_t work_count =
            backup_stage == BackupStage::Measure
                ? state_count
                : static_cast<std::uint32_t>(prioritized_states.size());
        while (backup_cursor < work_count &&
               states_done < kStatesPerUnit &&
               (transitions_done < kTransitionsPerUnit || states_done == 0)) {
            const std::uint32_t state =
                backup_stage == BackupStage::Measure
                    ? backup_cursor
                    : prioritized_states[backup_cursor].second;
            ++backup_cursor;
            if (!result.expanded[state] || result.goal_states[state]) {
                continue;
            }
            std::uint32_t state_work = 0;
            const double best = backup_state(state, state_work);
            transitions_done += state_work;
            ++states_done;
            const double before = result.values[state];
            const double improvement =
                best < before ? before - best : 0.0;
            if (backup_stage == BackupStage::Measure) {
                measured_residual = std::max(measured_residual, improvement);
                if (improvement > options.epsilon) {
                    prioritized_states.push_back({improvement, state});
                }
            } else if (best < before) {
                result.values[state] = best;
            }
        }
        if (backup_cursor >= work_count) {
            if (backup_stage == BackupStage::Measure) {
                residual = measured_residual;
                result.diagnostics.residual = residual;
                if (residual <= options.epsilon ||
                    sweeps >= options.max_sweeps) {
                    backup_active = false;
                } else {
                    std::sort(
                        prioritized_states.begin(),
                        prioritized_states.end(),
                        [](const auto& left, const auto& right) {
                            return left.first != right.first
                                       ? left.first > right.first
                                       : left.second < right.second;
                        });
                    backup_stage = BackupStage::Apply;
                    backup_cursor = 0;
                }
            } else {
                ++sweeps;
                result.diagnostics.sweeps = sweeps;
                begin_priority_measurement();
            }
        }
        ++result.diagnostics.bellman_work_units;
        result.diagnostics.max_bellman_unit_transitions = std::max(
            result.diagnostics.max_bellman_unit_transitions,
            transitions_done);
        result.diagnostics.optimization_ns += static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - started)
                .count());
    }

void SolveWork::Impl::step(std::uint32_t max_work_items) {
    try {
        if (setup_resource_limit.has_value()) {
            const auto [cap_name, limit] =
                std::move(*setup_resource_limit);
            setup_resource_limit.reset();
            throw SolverResourceLimit(cap_name, limit);
        }
        /* max_work_items is a caller ceiling, not permission to monopolize
         * the host for an arbitrarily large native/WASM batch. Individual
         * rows and retained continuations remain the logical work units; this
         * engine-owned ceiling only exposes cancellation and telemetry often
         * enough for product callers. */
        constexpr std::uint32_t kMaxCooperativeUnitsPerStep = 32;
        std::uint32_t remaining = std::max<std::uint32_t>(
            1, std::min(max_work_items, kMaxCooperativeUnitsPerStep));
        while (remaining > 0 && phase != SolvePhase::Done) {
            if (requested_bounded_finish &&
                incremental_upper_policy_pass) {
                abort_incremental_upper_policy_pass_for_bounded_finish();
            }
            if (requested_bounded_finish &&
                (phase == SolvePhase::Expanding ||
                 phase == SolvePhase::Iterating) &&
                can_prepare_requested_bounded_finish()) {
                prepare_requested_bounded_finish();
                begin_publication_pipeline();
                continue;
            }
            if (phase == SolvePhase::Refining ||
                phase == SolvePhase::Compiling ||
                phase == SolvePhase::Certifying) {
                advance_publication_pipeline();
                /* Finalization continuations define the UI/cancellation
                 * boundary. Never fold multiple proof resumes into one
                 * public solve step, even for a 1,024-item caller batch. */
                break;
            }
            if (phase == SolvePhase::Expanding) {
                if (incremental_post_upper_scheduling_active) {
                    advance_incremental_post_upper_scheduling();
                    --remaining;
                    /* Missing proof-only state values and the subsequent
                     * delayed-row classification are an explicit phase
                     * transition. Expose every retained proof resume to the
                     * host before scheduling another exact pair. */
                    break;
                }
                if (focused_lower_preparation_stage !=
                    FocusedLowerPreparationStage::Idle) {
                    advance_focused_lower_preparation();
                    --remaining;
                    /* Proof-pattern lower values are retained one state at a
                     * time. Their cursor is the cancellation boundary; do
                     * not fold the whole focused initialization into one
                     * public step. */
                    break;
                }
                if (incremental_dynamic_prepare_active) {
                    const bool preparation_complete =
                        advance_incremental_dynamic_preparation();
                    --remaining;
                    if (!preparation_complete) {
                        /* Dynamic automatic preparation owns a retained
                         * checkpoint. Return it to the host immediately so a
                         * large public batch cannot fold every coroutine
                         * resume into one uncancellable solve step. */
                        break;
                    }
                    if (requested_bounded_finish) {
                        phase = SolvePhase::Done;
                        break;
                    }
                    if (schedule_next_incremental_alternative()) continue;
                    if (schedule_incremental_refinement()) {
                        incremental_restricted_values_ready = false;
                        continue;
                    }
                    if (schedule_incremental_refinement(true)) {
                        incremental_restricted_values_ready = false;
                        continue;
                    }
                    phase = SolvePhase::Done;
                    break;
                }
                if (constructive_policy_active) {
                    finish_focused_lower_solve();
                    --remaining;
                    /* The retained cursor is an explicit cooperative
                     * boundary. Return even when the caller requested a
                     * large batch so UI qualification cannot fold every
                     * carrier proof back into one synchronous step. */
                    break;
                }
                if (target_gap_stop) {
                    prepare_iteration();
                    phase = SolvePhase::Done;
                    break;
                }
                if (focus_optimizing &&
                    expanded_count >= options.max_expanded_states &&
                    expanded_count >
                        kSynchronousCapFinalFocusStates &&
                    calc.state_count() > expanded_count) {
                    /*
                     * The expansion limit is a prompt resource boundary.
                     * Keep the most recent completed focused bound and any
                     * executable incumbent, but do not begin or continue a
                     * potentially unbounded optimization pass over the final
                     * cap-sized partial graph.
                     */
                    focus_optimizing = false;
                    focused_lower_mode = false;
                    prepare_iteration();
                    phase = SolvePhase::Done;
                    break;
                }
                if (focus_optimizing) {
                    run_focused_lower_unit();
                    --remaining;
                    /* Focused policy evaluation owns retained SCC and
                     * selection cursors. One resume is the cooperative host
                     * boundary; batching dozens of those logical units made
                     * a bounded continuation externally uncancellable for
                     * several seconds on owner-scale graphs. */
                    break;
                }
                if (!expansion_active && queue.empty() && focused_mode &&
                    !focused_closure_proved &&
                    expanded_count < options.max_expanded_states) {
                    begin_focused_lower_solve();
                    --remaining;
                    continue;
                }
                if ((!expansion_active && queue.empty()) ||
                    (!expansion_active &&
                     expanded_count >= options.max_expanded_states) ||
                    focused_closure_proved) {
                    prepare_iteration();
                    if (result.diagnostics.resource_cap_hit &&
                        (!incremental_action_generation ||
                         incremental_envelope_closed)) {
                        phase = SolvePhase::Done;
                    }
                    break; /* expose the phase boundary to callers */
                }
                const bool alternative_unit =
                    expansion_is_incremental_alternative;
                const std::size_t cap_count_before_unit =
                    result.diagnostics.cap_hits.size();
                const bool completed_state = expand_one_unit();
                --remaining;
                if (!completed_state && expansion_active &&
                    !expansion_prepared) {
                    /* State-local automatic admission suspended before row
                     * materialization. Its continuation is the cooperative
                     * unit; expose that boundary even when the caller asked
                     * for hundreds of ordinary expansion rows. */
                    break;
                }
                if (completed_state && alternative_unit) {
                    if (expansion_incremental_resource_limited ||
                        result.diagnostics.cap_hits.size() >
                            cap_count_before_unit) {
                        phase = SolvePhase::Done;
                        break;
                    }
                    if (requested_bounded_finish) {
                        phase = SolvePhase::Done;
                        break;
                    }
                    const bool action_added_states =
                        !incremental_alternative_rows.empty() &&
                        incremental_alternative_rows.back().states_added != 0;
                    incremental_epoch_added_states =
                        incremental_epoch_added_states ||
                        action_added_states;
                    /* Evaluate every automatic option on the frozen reachable
                     * carrier frontier, or every carrier for one delayed
                     * primitive, against the same incumbent/frontier epoch
                     * before restarting lower optimization and proving
                     * another executable upper.
                     * Row materialization is independent of Bellman values;
                     * newly interned support remains an open obligation until
                     * this natural scheduler boundary is reached. */
                    if (options.high_impact_executable_uppers &&
                        schedule_next_incremental_alternative(true)) {
                        continue;
                    }
                    if (incremental_epoch_added_states) {
                        /*
                         * Publish the epoch's late support into the value
                         * vectors before classification. Goal terminals need
                         * no expansion; non-goal deltas remain
                         * zero/feasible-bound fringe and are selected by the
                         * following Q refinement.
                         */
                        incremental_epoch_added_states = false;
                        incremental_restricted_values_ready = false;
                        incremental_upper_policy_dirty = true;
                        /* Newly interned support from a frozen automatic
                         * epoch is exactly the continuation frontier whose
                         * cleanup/recovery rows can complete a multi-action
                         * carrier ladder. Schedule that exceptional support
                         * before re-solving the unchanged Chaos restriction;
                         * otherwise the focused pass can converge without
                         * ever making a second automatic carrier epoch and
                         * finalize with thousands of known actions still
                         * unevaluated. */
                        if (schedule_incremental_refinement(true)) {
                            continue;
                        }
                        begin_focused_lower_solve();
                        continue;
                    }
                    if (classify_incremental_alternatives()) {
                        restart_incremental_optimization();
                        continue;
                    }
                    if (options.high_impact_executable_uppers &&
                        schedule_next_incremental_alternative()) {
                        continue;
                    }
                    if (schedule_incremental_refinement()) {
                        incremental_restricted_values_ready = false;
                        continue;
                    }
                    if (schedule_next_incremental_alternative()) {
                        continue;
                    }
                    if (schedule_incremental_refinement(true)) {
                        incremental_restricted_values_ready = false;
                        continue;
                    }
                    phase = SolvePhase::Done;
                    break;
                }
                if (completed_state && requested_bounded_finish) {
                    phase = SolvePhase::Done;
                    break;
                }
                if (completed_state &&
                    incremental_refinement_active &&
                    expanded_count >=
                        incremental_refinement_target_expanded) {
                    incremental_refinement_active = false;
                    incremental_restricted_values_ready = false;
                    incremental_upper_policy_dirty = true;
                    begin_focused_lower_solve();
                    continue;
                }
                if (completed_state && !focused_mode &&
                    expanded_count >= next_focus_checkpoint &&
                    queue.size() >
                        options.focused_expansion_queue_threshold &&
                    expanded_count < options.max_expanded_states) {
                    begin_focused_lower_solve();
                    continue;
                }
                if (result.diagnostics.resource_cap_hit) {
                    prepare_iteration();
                    if (result.diagnostics.resource_cap_hit &&
                        (!incremental_action_generation ||
                         incremental_envelope_closed)) {
                        phase = SolvePhase::Done;
                    }
                    break;
                }
                if (completed_state &&
                    expanded_count >= options.max_expanded_states) {
                    /*
                     * The last completed focused bound and executable
                     * incumbent remain available for cap finalization. Do
                     * not spend unbounded time reoptimizing the entire final
                     * cap-sized partial graph.
                     */
                    if (focused_mode && !focused_closure_proved &&
                        expanded_count <=
                            kSynchronousCapFinalFocusStates) {
                        begin_focused_lower_solve();
                        continue;
                    }
                    prepare_iteration();
                    phase = SolvePhase::Done;
                    break;
                }
                if (completed_state && !expansion_active && queue.empty()) {
                    if (focused_mode && !focused_closure_proved) {
                        begin_focused_lower_solve();
                    } else {
                        prepare_iteration();
                        if (result.diagnostics.resource_cap_hit) {
                            phase = SolvePhase::Done;
                        }
                        break;
                    }
                }
                continue;
            }

            if (!backup_active && numerical_stability_stop) {
                if (continue_open_incremental_envelope()) {
                    continue;
                }
                phase = SolvePhase::Done;
                break;
            }
            if (!backup_active &&
                (optimization_converged() ||
                 sweeps >= options.max_sweeps)) {
                if (incremental_action_generation &&
                    !incremental_envelope_closed) {
                    if (continue_open_incremental_envelope()) continue;
                }
                phase = SolvePhase::Done;
                break;
            }
            if (!policy_iteration_failed) {
                if (!run_policy_iteration_unit()) {
                    begin_priority_measurement();
                }
            } else {
                run_bellman_unit();
            }
            --remaining;
            if (!backup_active && numerical_stability_stop) {
                if (continue_open_incremental_envelope()) {
                    continue;
                }
                phase = SolvePhase::Done;
                break;
            }
            if (!backup_active &&
                (optimization_converged() ||
                 sweeps >= options.max_sweeps)) {
                if (incremental_action_generation &&
                    !incremental_envelope_closed) {
                    if (continue_open_incremental_envelope()) continue;
                }
                phase = SolvePhase::Done;
            }
        }
    } catch (const SolverResourceLimit& limit) {
        if (!options.goal_progress_gated_reforges) {
            throw;
        }
        /*
         * Exact kernel construction can also be requested by focused
         * heuristics and automatic admission outside expand_one_unit's
         * state-row catch. A configured solver cap is still a normal,
         * analyzable termination there; do not let it escape as a harness
         * error merely because the request came from a different phase.
         */
        record_cap(
            limit.cap_name(),
            limit.cap_name() == "max_discovered_states");
        const std::uint32_t discovered = calc.state_count();
        transition_cache->discovered_states = discovered;
        transition_cache->expanded_states = expanded_count;
        transition_cache->state_rows.resize(discovered);
        result.diagnostics.discovered_states = discovered;
        result.diagnostics.strict_discovered_states = discovered;
        result.diagnostics.quotient_states = discovered;
        result.diagnostics.expanded_states = expanded_count;
        result.diagnostics.frontier_states =
            discovered >= expanded_count ? discovered - expanded_count : 0;
        result.diagnostics.sparse_rows = transition_cache->rows.size();
        result.diagnostics.sparse_transitions =
            transition_cache->successors.size() +
            transition_cache->choice_successors.size();
        result.diagnostics.reforge_frontier_work =
            calc.telemetry().reforge_frontier_work;
        result.diagnostics.reforge_logical_work_v1 =
            calc.telemetry().reforge_logical_work_v1;
        expansion_active = false;
        backup_active = false;
        phase = SolvePhase::Done;
    }
        /*
         * Resource refusal ends graph growth, not executable-policy
         * evaluation on the graph already retained. Keep the experimental
         * pass resumable by reopening the ordinary focused work phase before
         * progress() can publish Done.
         */
        if (phase == SolvePhase::Done &&
            !requested_bounded_finish &&
            begin_incremental_upper_policy_pass()) {
            phase = SolvePhase::Expanding;
        }
        if (phase == SolvePhase::Done &&
            !finalized_result.has_value()) {
            if (requested_bounded_finish) {
                prepare_requested_bounded_finish();
            }
            begin_publication_pipeline();
        }
    }

bool SolveWork::Impl::can_prepare_requested_bounded_finish() const {
        /* Public SolveWork::step boundaries never suspend inside a sparse-row
         * append. Ordinary expansion/focused scratch can therefore be
         * discarded transactionally here. The incremental upper pass is the
         * sole owner of a moved lower snapshot and must first use its
         * dedicated restore path above. */
        return !incremental_upper_policy_pass;
    }

void SolveWork::Impl::prepare_requested_bounded_finish() {
        if (!can_prepare_requested_bounded_finish()) {
            throw std::logic_error(
                "requested bounded finish crossed live solver scratch");
        }
        /* A public step boundary never suspends halfway through appending a
         * sparse row. It can, however, retain a state-local automatic
         * admission coroutine or focused-policy scratch. Roll back only that
         * staged transaction and leave every completed row immutable for the
         * reachable anytime-policy builder. */
        calc.cancel_state_local_automatic_candidates();
        incremental_dynamic_prepare_active = false;
        incremental_dynamic_prepared = false;
        incremental_dynamic_operator_cursor = 0;
        incremental_dynamic_operator_indices.clear();
        incremental_post_upper_scheduling_active = false;
        incremental_post_upper_proof_cursor = 0;
        incremental_classification_active = false;
        incremental_classification_cursor = 0;
        incremental_classification_certified_lower.clear();

        /* A high-impact upper pass temporarily admits completed alternatives
         * to its private minimization problem. The independently retained
         * incumbent survives, but open rows must not leak into lower-bound or
         * exact-envelope authority during requested finalization. */
        for (const std::uint64_t row :
             incremental_upper_temporary_rows) {
            if (row < transition_cache->rows.size()) {
                transition_cache->rows[row].admitted = false;
            }
        }
        incremental_upper_temporary_rows.clear();
        incremental_upper_policy_pass = false;
        incremental_upper_fixed_policy_proved = false;

        expansion_active = false;
        expansion_prepared = false;
        expansion_is_incremental_alternative = false;
        expansion_operator_indices.clear();
        expansion_operator_cursor = 0;
        focus_optimizing = false;
        focused_lower_mode = false;
        focused_upper_mode = false;
        constructive_policy_active = false;
        constructive_fallback_pending = false;
        constructive_progress_fallback.reset();
        focused_lower_preparation_stage =
            FocusedLowerPreparationStage::Idle;
        focused_lower_preparation_cursor = 0;
        focused_lower_previous_values.clear();
        focused_lower_retained_minimum.clear();
        primitive_destructive_renewal_work = {};
        backup_active = false;
        reset_policy_iteration_units();

        const std::uint32_t discovered = calc.state_count();
        transition_cache->discovered_states = discovered;
        transition_cache->expanded_states = expanded_count;
        transition_cache->state_rows.resize(discovered);
        result.diagnostics.discovered_states = discovered;
        result.diagnostics.strict_discovered_states = discovered;
        result.diagnostics.quotient_states = discovered;
        result.diagnostics.expanded_states = expanded_count;
        result.diagnostics.frontier_states =
            discovered >= expanded_count ? discovered - expanded_count : 0;
        result.diagnostics.sparse_rows = transition_cache->rows.size();
        result.diagnostics.sparse_transitions =
            transition_cache->successors.size() +
            transition_cache->choice_successors.size();
        result.converged = false;
    }

}
}
