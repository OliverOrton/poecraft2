#include "solver_solve_types.hpp"

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

bool SolveWork::Impl::optimization_converged() const {
        if (policy_iteration_failed) {
            return residual <= options.epsilon;
        }
        return policy_initialized && policy_stable &&
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
        if (price_bound_state_pruning) {
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
        prepare_priced_rows();
        if (focused_bound_proved) {
            const std::uint64_t no_row =
                std::numeric_limits<std::uint64_t>::max();
            for (std::uint32_t state = 0; state < state_count; ++state) {
                if (result.goal_states[state]) continue;
                std::uint32_t operator_index = restart_operator_index;
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
        if (focused_bound_proved) phase = SolvePhase::Done;
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
            calc.outcomes(state, action_index);
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
        const SparseRow& row = transition_cache->rows.at(row_index);
        double constant = priced_rows.at(row_index).cost;
        transition_work = 0;
        if (kernel_value_cache_active && row.choice_count == 0 &&
            row.transition_count >= 1024) {
            KernelValueCache& cache = value_cache_for(row);
            const double self_value = result.values.at(row.owner_state);
            const bool infinite_self =
                row.embedded_self_probability > 0.0 &&
                self_value == kInfinity;
            const std::uint32_t non_self_infinite =
                cache.infinite_count - (infinite_self ? 1u : 0u);
            transition_work += row.transition_count;
            if (non_self_infinite != 0) return kInfinity;
            constant += cache.finite_sum;
            if (!infinite_self) {
                constant -=
                    row.embedded_self_probability * self_value;
            }
        } else {
            for (std::uint32_t i = 0; i < row.transition_count; ++i) {
                const std::uint64_t offset = row.transition_offset + i;
                if (transition_cache->successors.at(offset) ==
                    row.owner_state) {
                    continue;
                }
                const double value = result.values[
                    transition_cache->successors.at(offset)];
                ++transition_work;
                if (value == kInfinity) return kInfinity;
                constant += transition_cache->probabilities.at(offset) *
                            value;
            }
        }
        std::vector<std::pair<double, double>> self_choices;
        for (std::uint32_t i = 0; i < row.choice_count; ++i) {
            const SparseChoiceGroup& group = transition_cache->choices.at(
                row.choice_offset + i);
            double best = kInfinity;
            for (std::uint32_t s = 0; s < group.successor_count; ++s) {
                best = std::min(
                    best,
                    result.values[transition_cache->choice_successors.at(
                        group.successor_offset + s)]);
            }
            transition_work += group.successor_count;
            if (group.has_self) {
                self_choices.push_back({best, group.probability});
            } else {
                if (best == kInfinity) return kInfinity;
                constant += group.probability * best;
            }
        }
        /* Solve the row-local fixed point exactly:
         *
         * x = constant + p_self*x + sum g*min(x, alternate_g)
         *
         * Sorting the observed-choice thresholds identifies which offers
         * return to the outer policy and which take a concrete successor. */
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
            const double value = denominator > 1e-15
                                     ? constant / denominator
                                     : kInfinity;
            if (fixed_choices >= self_choices.size() ||
                value <= self_choices[fixed_choices].first + 1e-12) {
                return value;
            }
            const auto [alternate, probability] =
                self_choices[fixed_choices++];
            if (!std::isfinite(alternate)) return kInfinity;
            constant += probability * alternate;
            loop_probability -= probability;
        }
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
        policy_selection_residual = 0.0;
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
                const StateRowSpan& span =
                    transition_cache->state_rows.at(state);
                std::uint64_t best_row =
                    std::numeric_limits<std::uint64_t>::max();
                for (std::uint32_t relative = 0;
                     relative < span.count; ++relative) {
                    const std::uint64_t absolute = span.offset + relative;
                    if (preservation_prunes(absolute)) continue;
                    const SparseRow& row =
                        transition_cache->rows.at(absolute);
                    bool valid = true;
                    bool exits_rank = false;
                    const auto route = [&](const std::uint32_t successor) {
                        if (!valid || successor == state) return;
                        if (successor >= result.values.size()) {
                            valid = false;
                        } else if (result.goal_states[successor] ||
                                   !result.expanded[successor] ||
                                   assigned[successor]) {
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
                        std::uint32_t selected =
                            choice.has_self ? state : kNoId;
                        double selected_value =
                            choice.has_self ? result.values[state] : kInfinity;
                        for (std::uint32_t s = 0;
                             s < choice.successor_count; ++s) {
                            const std::uint32_t successor =
                                transition_cache->choice_successors.at(
                                    choice.successor_offset + s);
                            const double value = result.values[successor];
                            if (value < selected_value - options.epsilon ||
                                (std::abs(value - selected_value) <=
                                     options.epsilon &&
                                 successor < selected)) {
                                selected = successor;
                                selected_value = value;
                            }
                        }
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
        if (remaining != 0) return false;
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
            const StateRowSpan& span = transition_cache->state_rows.at(state);
            double best = kInfinity;
            std::uint64_t best_row = no_row;
            for (std::uint32_t row = 0; row < span.count; ++row) {
                std::uint32_t work = 0;
                const std::uint64_t absolute = span.offset + row;
                if (preservation_prunes(absolute)) continue;
                const double q = sparse_row_q(absolute, work);
                ++result.diagnostics.bellman_action_evaluations;
                if (q < best - options.epsilon) {
                    best = q;
                    best_row = absolute;
                }
            }
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
                    improving = best < current - options.epsilon;
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

        /* Exact fixed-policy quotient. A policy state's value equation is
         * determined entirely by its immediate cost and full transition row.
         * States with byte-identical equations therefore have identical
         * values and can share one variable. Reforge-heavy policies contain
         * thousands of such states; quotienting them turns the giant retry
         * SCC into the small exact system it represents without changing a
         * probability, carrier distinction, action, or policy choice. */
        if (policy_kernel_preparation == nullptr ||
            policy_kernel_preparation->state_count != state_count) {
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
                std::uint32_t selected = choice.has_self ? state : kNoId;
                double selected_value =
                    choice.has_self ? result.values[state] : kInfinity;
                for (std::uint32_t successor = 0;
                     successor < choice.successor_count; ++successor) {
                    const std::uint32_t candidate =
                        transition_cache->choice_successors.at(
                            choice.successor_offset + successor);
                    const double candidate_value = result.values[candidate];
                    if (candidate_value < selected_value - options.epsilon ||
                        (std::abs(candidate_value - selected_value) <=
                             options.epsilon &&
                         candidate < selected)) {
                        selected = candidate;
                        selected_value = candidate_value;
                    }
                }
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
        if (preparation.cursor < preparation.active_states.size()) {
            policy_evaluation_incomplete = true;
            return false;
        }

        if (preparation.representative.empty()) {
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
        if (preparation.grouping_cursor < preparation.active_states.size()) {
            policy_evaluation_incomplete = true;
            return false;
        }
        constexpr std::uint32_t kQuotientStatesPerWorkUnit = 64;
        const std::uint32_t quotient_end = std::min<std::uint32_t>(
            static_cast<std::uint32_t>(preparation.active_states.size()),
            preparation.quotient_cursor + kQuotientStatesPerWorkUnit);
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
            const WideFloat denominator =
                WideFloat{1.0} - self_probability;
            if (denominator.value() > 1e-15) {
                const double divisor = denominator.value();
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
            if (preparation.tarjan_index.empty()) {
                preparation.tarjan_index.assign(state_count, kNoId);
                preparation.tarjan_lowlink.assign(state_count, kNoId);
                preparation.tarjan_on_stack.assign(state_count, 0);
            }
            const auto push = [&](const std::uint32_t state) {
                preparation.tarjan_index[state] =
                    preparation.tarjan_lowlink[state] =
                        preparation.tarjan_next_index++;
                preparation.tarjan_stack.push_back(state);
                preparation.tarjan_on_stack[state] = 1;
                preparation.tarjan_dfs.push_back({state, 0});
            };
            constexpr std::uint32_t kTarjanWorkPerUnit = 4096;
            std::uint32_t tarjan_work = 0;
            while (tarjan_work < kTarjanWorkPerUnit) {
                if (preparation.tarjan_dfs.empty()) {
                    while (preparation.tarjan_root_cursor <
                               preparation.active_states.size() &&
                           tarjan_work < kTarjanWorkPerUnit) {
                        const std::uint32_t root =
                            preparation.active_states[
                                preparation.tarjan_root_cursor++];
                        ++tarjan_work;
                        if (!result.expanded[root] ||
                            result.goal_states[root] ||
                            policy_rows[root] == no_row ||
                            representative[root] != root ||
                            preparation.tarjan_index[root] != kNoId) {
                            continue;
                        }
                        push(root);
                        break;
                    }
                    if (preparation.tarjan_dfs.empty()) break;
                }
                PolicyTarjanFrame& frame = preparation.tarjan_dfs.back();
                const PolicyRow& row = rows[frame.state];
                if (frame.next_edge < row.edge_count) {
                    const std::uint32_t target = edges.at(
                        row.edge_offset + frame.next_edge++).target;
                    ++tarjan_work;
                    if (target >= state_count || !result.expanded[target] ||
                        result.goal_states[target] ||
                        policy_rows[target] == no_row) {
                        continue;
                    }
                    if (preparation.tarjan_index[target] == kNoId) {
                        push(target);
                    } else if (preparation.tarjan_on_stack[target]) {
                        preparation.tarjan_lowlink[frame.state] = std::min(
                            preparation.tarjan_lowlink[frame.state],
                            preparation.tarjan_index[target]);
                    }
                    continue;
                }
                const std::uint32_t completed = frame.state;
                preparation.tarjan_dfs.pop_back();
                ++tarjan_work;
                if (!preparation.tarjan_dfs.empty()) {
                    const std::uint32_t parent =
                        preparation.tarjan_dfs.back().state;
                    preparation.tarjan_lowlink[parent] = std::min(
                        preparation.tarjan_lowlink[parent],
                        preparation.tarjan_lowlink[completed]);
                }
                if (preparation.tarjan_lowlink[completed] ==
                    preparation.tarjan_index[completed]) {
                    components.emplace_back();
                    while (true) {
                        const std::uint32_t member =
                            preparation.tarjan_stack.back();
                        preparation.tarjan_stack.pop_back();
                        preparation.tarjan_on_stack[member] = 0;
                        components.back().push_back(member);
                        if (member == completed) break;
                    }
                    std::sort(
                        components.back().begin(), components.back().end());
                }
            }

            if (preparation.tarjan_root_cursor >=
                    preparation.active_states.size() &&
                preparation.tarjan_dfs.empty()) {
                preparation.component_by_state.assign(state_count, kNoId);
                for (std::uint32_t component = 0;
                     component < components.size(); ++component) {
                    for (const std::uint32_t state : components[component]) {
                        preparation.component_by_state[state] = component;
                    }
                }
                preparation.local.assign(state_count, -1);
                preparation.components_ready = true;
            }
            if (!preparation.components_ready) {
                policy_evaluation_incomplete = true;
                return false;
            }
        }
        std::vector<std::uint32_t>& component_by_state =
            preparation.component_by_state;
        std::vector<std::int32_t>& local = preparation.local;
        std::uint64_t policy_scratch =
            preparation.active_states.capacity() * sizeof(std::uint32_t) +
            preparation.kernel_owner.capacity() * sizeof(std::uint32_t) +
            preparation.full_kernel.capacity() *
                sizeof(std::vector<PolicyEdge>) +
            preparation.representative.capacity() * sizeof(std::uint32_t) +
            preparation.group_members.capacity() *
                sizeof(std::vector<std::uint32_t>) +
            rows.capacity() * sizeof(PolicyRow) +
            edges.capacity() * sizeof(PolicyEdge) +
            component_by_state.capacity() * sizeof(std::uint32_t) +
            local.capacity() * sizeof(std::int32_t) +
            components.capacity() * sizeof(std::vector<std::uint32_t>) +
            preparation.tarjan_index.capacity() * sizeof(std::uint32_t) +
            preparation.tarjan_lowlink.capacity() * sizeof(std::uint32_t) +
            preparation.tarjan_on_stack.capacity() * sizeof(std::uint8_t) +
            preparation.tarjan_stack.capacity() * sizeof(std::uint32_t) +
            preparation.tarjan_dfs.capacity() * sizeof(PolicyTarjanFrame);
        for (const auto& kernel : preparation.full_kernel) {
            policy_scratch += kernel.capacity() * sizeof(PolicyEdge);
        }
        for (const auto& members : preparation.group_members) {
            policy_scratch += members.capacity() * sizeof(std::uint32_t);
        }
        for (const auto& component : components) {
            policy_scratch += component.capacity() * sizeof(std::uint32_t);
        }
        const auto map_vectors_bytes = [](const auto& values) {
            std::uint64_t bytes = values.bucket_count() * sizeof(void*);
            bytes += values.size() *
                     (sizeof(typename std::decay_t<decltype(values)>::value_type) +
                      2 * sizeof(void*));
            for (const auto& [unused, entries] : values) {
                (void)unused;
                bytes += entries.capacity() * sizeof(std::uint32_t);
            }
            return bytes;
        };
        policy_scratch += map_vectors_bytes(
            preparation.representatives_by_hash);
        policy_scratch +=
            preparation.shared_transition_representatives.bucket_count() *
            sizeof(void*);
        policy_scratch +=
            preparation.shared_transition_representatives.size() *
            (sizeof(decltype(preparation.shared_transition_representatives)::
                        value_type) +
             2 * sizeof(void*));
        for (const auto& [unused, entries] :
             preparation.shared_transition_representatives) {
            (void)unused;
            policy_scratch += entries.capacity() *
                              sizeof(SharedPolicyKernelRepresentative);
            for (const SharedPolicyKernelRepresentative& entry : entries) {
                policy_scratch += entry.exact_signature.capacity() *
                                  sizeof(std::uint64_t);
            }
        }
        current_policy_scratch_bytes = policy_scratch;
        peak_policy_scratch_bytes = std::max(
            peak_policy_scratch_bytes, policy_scratch);
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

            std::vector<double> solved(n, 0.0);
            if (n <= kDensePolicyComponentLimit) {
                const std::uint64_t matrix_bytes =
                    n * (n + 1) * sizeof(WideFloat);
                check_solver_byte_cap_fast(matrix_bytes);
                peak_policy_scratch_bytes = std::max(
                    peak_policy_scratch_bytes,
                    policy_scratch + matrix_bytes);
                const std::size_t stride = n + 1;
                std::vector<WideFloat> matrix(n * stride, 0.0);
                const auto cell = [&](const std::size_t row,
                                      const std::size_t column) -> WideFloat& {
                    return matrix[row * stride + column];
                };
                for (std::size_t i = 0; i < n; ++i) {
                    cell(i, i) = 1.0;
                    cell(i, n) = rhs[i];
                    const PolicyRow& row = rows[members[i]];
                    for (std::uint32_t e = 0; e < row.edge_count; ++e) {
                        const PolicyEdge& edge = edges.at(row.edge_offset + e);
                        if (component_by_state[edge.target] == component) {
                            cell(i, static_cast<std::size_t>(
                                local[edge.target])) -=
                                WideFloat{edge.probability};
                        }
                    }
                }
                for (std::size_t column = 0; column < n; ++column) {
                    std::size_t pivot = column;
                    for (std::size_t row = column + 1; row < n; ++row) {
                        if (std::fabs(cell(row, column).value()) >
                            std::fabs(cell(pivot, column).value())) {
                            pivot = row;
                        }
                    }
                    if (std::fabs(cell(pivot, column).value()) <= 1e-15) {
                        return fail("dense_policy_component_is_singular");
                    }
                    if (pivot != column) {
                        for (std::size_t k = column; k <= n; ++k) {
                            std::swap(cell(pivot, k), cell(column, k));
                        }
                    }
                    for (std::size_t row = column + 1; row < n; ++row) {
                        const WideFloat factor =
                            cell(row, column) / cell(column, column);
                        if (factor.value() == 0.0) continue;
                        for (std::size_t k = column; k <= n; ++k) {
                            cell(row, k) -= factor * cell(column, k);
                        }
                    }
                }
                for (std::size_t back = n; back-- > 0;) {
                    WideFloat value = cell(back, n);
                    for (std::size_t column = back + 1; column < n; ++column) {
                        value -= cell(back, column) * solved[column];
                    }
                    solved[back] =
                        (value / cell(back, back)).value();
                }
            } else {
                const std::uint64_t iterative_bytes =
                    n * 8 * sizeof(WideFloat);
                check_solver_byte_cap_fast(iterative_bytes);
                peak_policy_scratch_bytes = std::max(
                    peak_policy_scratch_bytes,
                    policy_scratch + iterative_bytes);
                /* Large fixed-policy components use BiCGSTAB on the sparse
                 * M-matrix (I-P). It avoids quadratic storage while retaining
                 * an explicit residual check before values are accepted. */
                std::vector<WideFloat> b(n);
                for (std::size_t i = 0; i < n; ++i) {
                    b[i] = rhs[i];
                }
                const bool can_resume =
                    sparse_policy_resume != nullptr &&
                    sparse_policy_resume->members == members &&
                    sparse_policy_resume->b == b;
                std::vector<WideFloat> x(n), r(n), r0(n), p(n, 0.0),
                    v(n, 0.0), s(n), t(n);
                WideFloat rho_previous = 1.0;
                WideFloat alpha = 1.0;
                WideFloat omega = 1.0;
                std::uint32_t resumed_iterations = 0;
                std::uint32_t refinement_count = 0;
                if (can_resume) {
                    x = std::move(sparse_policy_resume->x);
                    r = std::move(sparse_policy_resume->r);
                    r0 = std::move(sparse_policy_resume->r0);
                    p = std::move(sparse_policy_resume->p);
                    v = std::move(sparse_policy_resume->v);
                    s = std::move(sparse_policy_resume->s);
                    t = std::move(sparse_policy_resume->t);
                    rho_previous = sparse_policy_resume->rho_previous;
                    alpha = sparse_policy_resume->alpha;
                    omega = sparse_policy_resume->omega;
                    resumed_iterations = sparse_policy_resume->iterations;
                    refinement_count =
                        sparse_policy_resume->refinement_count;
                    sparse_policy_resume.reset();
                } else {
                    for (std::size_t i = 0; i < n; ++i) {
                        const double previous = result.values[members[i]];
                        x[i] = std::isfinite(previous) && previous >= 0.0 &&
                                       previous < kValueCeiling
                                   ? previous
                                   : 0.0;
                    }
                }
                const auto dot = [](const auto& left, const auto& right) {
                    WideFloat value = 0.0;
                    for (std::size_t i = 0; i < left.size(); ++i) {
                        value = value + left[i] * right[i];
                    }
                    return value;
                };
                const auto multiply = [&](
                    const std::vector<WideFloat>& input,
                    std::vector<WideFloat>& output) {
                    for (std::size_t i = 0; i < n; ++i) {
                        WideFloat internal_sum = 0.0;
                        const PolicyRow& row = rows[members[i]];
                        for (std::uint32_t e = 0; e < row.edge_count; ++e) {
                            const PolicyEdge& edge = edges.at(
                                row.edge_offset + e);
                            if (component_by_state[edge.target] == component) {
                                const WideFloat product =
                                    edge.probability * input[
                                    static_cast<std::size_t>(
                                        local[edge.target])];
                                internal_sum = internal_sum + product;
                            }
                        }
                        output[i] = input[i] - internal_sum;
                    }
                };
                const auto norm = [&](const auto& values) {
                    return std::sqrt(std::max(
                        0.0, dot(values, values).value()));
                };
                /* Fixed-policy evaluation is the numerical ground truth used
                 * by policy improvement. Solve it substantially tighter than
                 * the outer Bellman stopping tolerance so independently
                 * rounded native/WASM iterates agree at endgame value scales.
                 * Warm-starting from the preceding exact policy values makes
                 * the tighter solve cheaper than restarting BiCGSTAB at zero. */
                if (!can_resume) {
                    multiply(x, v);
                    for (std::size_t i = 0; i < n; ++i) {
                        r[i] = r0[i] = b[i] - v[i];
                    }
                }
                const double fixed_policy_relative_tolerance = 1e-18;
                const double tolerance =
                    fixed_policy_relative_tolerance * std::max(
                        1.0, norm(b));
                bool converged = norm(r) <= tolerance;
                const std::size_t max_iterations = std::min<std::size_t>(
                    100000, std::max<std::size_t>(1000, n * 10));
                constexpr std::size_t kIterationsPerWorkUnit = 4;
                const std::size_t work_unit_iterations =
                    std::min(max_iterations, kIterationsPerWorkUnit);
                std::size_t iterations = 0;
                for (; !converged && iterations < work_unit_iterations;
                     ++iterations) {
                    const WideFloat rho = dot(r0, r);
                    if (rho.value() == 0.0 || omega.value() == 0.0 ||
                        !std::isfinite(rho.value()) ||
                        !std::isfinite(omega.value())) break;
                    const WideFloat beta =
                        (rho / rho_previous) * (alpha / omega);
                    for (std::size_t i = 0; i < n; ++i) {
                        p[i] = r[i] + beta * (p[i] - omega * v[i]);
                    }
                    multiply(p, v);
                    const WideFloat denominator = dot(r0, v);
                    if (denominator.value() == 0.0 ||
                        !std::isfinite(denominator.value())) break;
                    alpha = rho / denominator;
                    for (std::size_t i = 0; i < n; ++i) {
                        s[i] = r[i] - alpha * v[i];
                    }
                    if (norm(s) <= tolerance) {
                        for (std::size_t i = 0; i < n; ++i) {
                            x[i] += alpha * p[i];
                        }
                        converged = true;
                        break;
                    }
                    multiply(s, t);
                    const WideFloat tt = dot(t, t);
                    if (tt.value() == 0.0 ||
                        !std::isfinite(tt.value())) break;
                    omega = dot(t, s) / tt;
                    for (std::size_t i = 0; i < n; ++i) {
                        x[i] += alpha * p[i] + omega * s[i];
                        r[i] = s[i] - omega * t[i];
                    }
                    converged = norm(r) <= tolerance;
                    rho_previous = rho;
                }
                bool refinement_restart = false;
                if (converged && refinement_count < 10) {
                    /* BiCGSTAB's recursively updated residual can look
                     * converged before b-Ax is equally small on a highly
                     * recurrent policy. Recompute the true residual and use
                     * it as an exact iterative-refinement restart. This keeps
                     * forward error, not only recurrence error, stable across
                     * native and WebAssembly floating-point implementations. */
                    multiply(x, t);
                    for (std::size_t i = 0; i < n; ++i) {
                        r[i] = b[i] - t[i];
                    }
                    ++refinement_count;
                    converged = norm(r) <= tolerance;
                    if (!converged) {
                        r0 = r;
                        std::fill(p.begin(), p.end(), 0.0);
                        std::fill(v.begin(), v.end(), 0.0);
                        rho_previous = 1.0;
                        alpha = 1.0;
                        omega = 1.0;
                        refinement_restart = true;
                    }
                }
                result.diagnostics.sparse_policy_iterations += iterations;
                const std::size_t total_iterations =
                    resumed_iterations + iterations;
                if (!converged &&
                    (iterations >= work_unit_iterations ||
                     refinement_restart) &&
                    total_iterations < max_iterations) {
                    sparse_policy_resume =
                        std::make_unique<SparsePolicyResume>();
                    sparse_policy_resume->members = members;
                    sparse_policy_resume->b = std::move(b);
                    sparse_policy_resume->x = std::move(x);
                    sparse_policy_resume->r = std::move(r);
                    sparse_policy_resume->r0 = std::move(r0);
                    sparse_policy_resume->p = std::move(p);
                    sparse_policy_resume->v = std::move(v);
                    sparse_policy_resume->s = std::move(s);
                    sparse_policy_resume->t = std::move(t);
                    sparse_policy_resume->rho_previous = rho_previous;
                    sparse_policy_resume->alpha = alpha;
                    sparse_policy_resume->omega = omega;
                    sparse_policy_resume->iterations =
                        static_cast<std::uint32_t>(total_iterations);
                    sparse_policy_resume->refinement_count =
                        refinement_count;
                    policy_evaluation_incomplete = true;
                    return false;
                }
                if (!converged) {
                    sparse_policy_resume.reset();
                    return fail("sparse_policy_component_did_not_converge");
                }
                result.diagnostics.max_sparse_policy_iterations = std::max(
                    result.diagnostics.max_sparse_policy_iterations,
                    resumed_iterations +
                        static_cast<std::uint32_t>(iterations));
                for (std::size_t i = 0; i < n; ++i) {
                    solved[i] = x[i].value();
                }
            }
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
        std::vector<std::uint8_t> in_component(result.values.size(), 0);
        for (const std::uint32_t state : improper_policy_states) {
            in_component[state] = 1;
        }
        bool repaired = false;
        for (const std::uint32_t state : improper_policy_states) {
            double best_q = kInfinity;
            std::uint64_t best_row =
                std::numeric_limits<std::uint64_t>::max();
            const StateRowSpan& span = transition_cache->state_rows.at(state);
            for (std::uint32_t i = 0; i < span.count; ++i) {
                const std::uint64_t row_index = span.offset + i;
                if (policy_rows[state] == row_index) continue;
                if (preservation_prunes(row_index)) continue;
                const SparseRow& row = transition_cache->rows.at(row_index);
                bool exits = false;
                for (std::uint32_t transition = 0;
                     transition < row.transition_count; ++transition) {
                    const std::uint32_t successor =
                        transition_cache->successors.at(
                            row.transition_offset + transition);
                    if (successor == state) continue;
                    if (result.goal_states[successor] ||
                        !in_component[successor]) {
                        exits = true;
                        break;
                    }
                }
                for (std::uint32_t choice_index = 0;
                     !exits && choice_index < row.choice_count;
                     ++choice_index) {
                    const SparseChoiceGroup& choice =
                        transition_cache->choices.at(
                            row.choice_offset + choice_index);
                    std::uint32_t selected =
                        choice.has_self ? state : kNoId;
                    double selected_value =
                        choice.has_self ? result.values[state] : kInfinity;
                    for (std::uint32_t successor_index = 0;
                         successor_index < choice.successor_count;
                         ++successor_index) {
                        const std::uint32_t successor =
                            transition_cache->choice_successors.at(
                                choice.successor_offset + successor_index);
                        const double value = result.values[successor];
                        if (value < selected_value - options.epsilon ||
                            (std::abs(value - selected_value) <=
                                 options.epsilon &&
                             successor < selected)) {
                            selected = successor;
                            selected_value = value;
                        }
                    }
                    exits = selected != kNoId &&
                            (result.goal_states[selected] ||
                             !in_component[selected]);
                }
                if (!exits) continue;
                std::uint32_t work = 0;
                const double q = sparse_row_q(row_index, work);
                if (q < best_q - options.epsilon) {
                    best_q = q;
                    best_row = row_index;
                }
            }
            if (best_row != std::numeric_limits<std::uint64_t>::max()) {
                /* Every selected row has a certified exit from the closed
                 * component. Repair them together; the next fixed-policy
                 * evaluation proves properness and ordinary Howard
                 * improvement is still responsible for optimality. */
                policy_rows[state] = best_row;
                repaired = true;
            }
        }
        if (!repaired) return false;
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
                backup_active = true;
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
        if ((policy_stable && residual <= acceptable_residual()) ||
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
        double best = kInfinity;
        transition_work = 0;
        const StateRowSpan& span = transition_cache->state_rows.at(state);
        ++result.diagnostics.bellman_backups;
        for (std::uint32_t row = 0; row < span.count; ++row) {
            std::uint32_t row_work = 0;
            if (preservation_prunes(span.offset + row)) continue;
            best = std::min(
                best, sparse_row_q(span.offset + row, row_work));
            transition_work += row_work;
            ++result.diagnostics.bellman_action_evaluations;
        }
        return best;
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
        std::uint32_t remaining = std::max<std::uint32_t>(1, max_work_items);
        while (remaining > 0 && phase != SolvePhase::Done) {
            if (phase == SolvePhase::Expanding) {
                if (target_gap_stop) {
                    prepare_iteration();
                    phase = SolvePhase::Done;
                    break;
                }
                if (focus_optimizing) {
                    run_focused_lower_unit();
                    --remaining;
                    continue;
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
                    if (result.diagnostics.resource_cap_hit) {
                        phase = SolvePhase::Done;
                    }
                    break; /* expose the phase boundary to callers */
                }
                const bool completed_state = expand_one_unit();
                --remaining;
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
                    if (result.diagnostics.resource_cap_hit) {
                        phase = SolvePhase::Done;
                    }
                    break;
                }
                if (completed_state &&
                    expanded_count >= options.max_expanded_states) {
                    /* Measure the final exact partial graph before reporting
                     * its cap. Otherwise a focused batch that lands exactly
                     * on the limit exposes bounds from the preceding round and
                     * discards every state in the last batch. */
                    if (focused_mode && !focused_closure_proved) {
                        begin_focused_lower_solve();
                        continue;
                    }
                    prepare_iteration();
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

            if (!backup_active &&
                (optimization_converged() ||
                 sweeps >= options.max_sweeps)) {
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
            if (!backup_active &&
                (optimization_converged() ||
                 sweeps >= options.max_sweeps)) {
                phase = SolvePhase::Done;
            }
        }
    }

}
}
