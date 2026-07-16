#include "solver_internal.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <deque>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

/*
 * Solver S4: value iteration over the reachable abstract state set
 * (docs/crafting-solver-plan.md, DP Solver).
 *
 *   V(goal) = 0
 *   V(s)    = min over legal actions a of cost(a) + sum P(s'|s,a) V(s')
 *
 * Restarting is an ordinary action whose successor is the clean base, so
 * it upper-bounds every value and salvage-versus-restart falls out of the
 * minimization. Reforge-class cycles make this value iteration rather
 * than settling; iteration starts from +infinity and converges because
 * the restart bound pulls every state that can reach the goal to a finite
 * value.
 */
namespace poecraft {
namespace solver {

namespace {

constexpr double kInfinity = std::numeric_limits<double>::infinity();
/* Finite upper-bound initialization (the restart bound makes every
 * goal-connected value finite; genuinely unreachable states stay here).
 * Value iteration descends monotonically from above, so a plain +infinity
 * start would never lift off: a backup is only finite once every
 * successor is, and no such state exists initially. */
constexpr double kValueCeiling = 1e12;

struct PricedOperator {
    std::uint32_t index = kNoId;
    double cost = 0.0;
};

struct SparseChoiceGroup {
    std::uint64_t successor_offset = 0;
    std::uint32_t successor_count = 0;
    double probability = 0.0;
};

struct SparseRow {
    std::uint32_t operator_index = kNoId;
    double cost = 0.0;
    std::uint64_t transition_offset = 0;
    std::uint32_t transition_count = 0;
    std::uint64_t choice_offset = 0;
    std::uint32_t choice_count = 0;
    std::uint64_t choice_option_offset = 0;
    std::uint32_t choice_option_count = 0;
};

struct StateRowSpan {
    std::uint64_t offset = 0;
    std::uint32_t count = 0;
};

struct PendingSparseRow {
    std::uint32_t operator_index = kNoId;
    double cost = 0.0;
    const std::vector<OutcomeEntry>* transitions = nullptr;
    const std::vector<OutcomeChoiceGroup>* choices = nullptr;
    const std::vector<OutcomeChoiceOption>* choice_options = nullptr;
};

} // namespace

struct SolveWork::Impl {
    CalcContext& calc;
    const SessionImpl& session;
    SolveOptions options;
    SolveResult result;
    std::vector<PricedOperator> operators;
    std::vector<bool> reported_unsupported;
    std::vector<std::uint8_t> expanded;
    std::vector<std::uint8_t> queued;
    std::deque<std::uint32_t> queue;
    std::uint32_t expanded_count = 0;
    std::uint32_t peak_queue_size = 0;
    std::uint32_t sweeps = 0;
    double residual = kValueCeiling;
    std::vector<StateRowSpan> state_rows;
    std::vector<SparseRow> sparse_rows;
    std::vector<std::uint32_t> sparse_successors;
    std::vector<double> sparse_probabilities;
    std::vector<SparseChoiceGroup> sparse_choices;
    std::vector<std::uint32_t> sparse_choice_successors;
    std::vector<OutcomeChoiceOption> sparse_choice_options;
    std::unordered_map<std::size_t, std::vector<std::uint64_t>>
        kernel_rows_by_hash;
    std::uint32_t sweep_state = 0;
    std::uint32_t sweep_row = 0;
    double sweep_best = kInfinity;
    double sweep_residual = 0.0;
    bool sweep_active = false;
    std::uint64_t peak_owned_bytes = 0;
    SolvePhase phase = SolvePhase::Expanding;
    bool consumed = false;

    Impl(
        CalcContext& context,
        const pc_item_state& start_item,
        const std::unordered_map<std::string, double>& prices,
        const SolveOptions& solve_options)
        : calc(context), session(context.session()), options(solve_options),
          reported_unsupported(context.operators().size(), false) {
        const auto setup_started = std::chrono::steady_clock::now();
        options.max_expanded_states = std::min(
            options.max_expanded_states, options.max_states);
        calc.reset_solve_telemetry();
        calc.set_solve_resource_caps(
            options.max_discovered_states, options.max_reforge_work);
        result.options = options;
        result.diagnostics.registry_actions = static_cast<std::uint32_t>(
            calc.registry().actions.size());
        result.diagnostics.candidate_actions = static_cast<std::uint32_t>(
            calc.candidates().size());
        const ActionControlSummary& control = calc.action_control();
        result.diagnostics.relevance_reduced_actions =
            control.pruned_outside_envelope;
        result.diagnostics.dependency_actions =
            control.dependency_primitives;
        result.diagnostics.deferred_actions =
            control.deferred_fossil_loadouts;
        result.diagnostics.action_inclusion_reasons.push_back(
            std::string(control.explicit_envelope
                            ? "included:explicit_goal_envelope:"
                            : "included:conservative_exhaustive_envelope:") +
            std::to_string(control.included_primitives));
        if (control.pruned_outside_envelope != 0) {
            result.diagnostics.action_inclusion_reasons.push_back(
                "pruned:not_permitted_by_explicit_goal_envelope:" +
                std::to_string(control.pruned_outside_envelope));
        }
        if (control.dependency_primitives != 0) {
            result.diagnostics.action_inclusion_reasons.push_back(
                "included:fixed_option_structural_dependency:" +
                std::to_string(control.dependency_primitives));
        }
        if (control.deferred_fossil_loadouts != 0) {
            result.diagnostics.action_inclusion_reasons.push_back(
                "deferred:lazy_fossil_signature_not_requested:" +
                std::to_string(control.deferred_fossil_loadouts));
        }
        /* Primitive support remains action-registry telemetry. Fixed options
         * are priced and evaluated alongside those primitive wrappers. */
        for (const std::uint32_t index : calc.candidates()) {
            const ActionDescriptor& descriptor =
                calc.registry().actions.at(index);
            const bool supported = calc_supports(descriptor);
            if (supported) {
                ++result.diagnostics.evaluator_supported_actions;
            }
        }
        for (const std::uint32_t index : calc.candidate_operators()) {
            const PlannerOperator& planner = calc.operators().at(index);
            double cost = 0.0;
            bool priced = true;
            for (const auto& [key, quantity] :
                 planner.resource_quantities) {
                const auto found = prices.find(key);
                if (found == prices.end()) {
                    priced = false;
                    break;
                }
                cost += quantity * found->second;
            }
            if (!priced) {
                result.diagnostics.skipped_missing_price.push_back(
                    planner.id);
                add_action_reason(
                    "unpriced", planner.id,
                    "missing_one_or_more_resource_prices");
                continue;
            }
            ++result.diagnostics.priced_scanned_actions;
            const bool supported =
                planner.kind == PlannerOperatorKind::FixedOption ||
                calc_supports(calc.registry().actions.at(
                    planner.primitive_action));
            if (!supported) {
                reported_unsupported[index] = true;
                result.diagnostics.skipped_unsupported.push_back(planner.id);
                add_action_reason(
                    "unsupported", planner.id,
                    "no_exact_evaluator_for_requested_primitive");
                continue;
            }
            operators.push_back({index, cost});
            ++result.diagnostics.supported_priced_actions;
        }

        result.start_state = calc.intern_item(start_item);
        enqueue(result.start_state);
        result.diagnostics.solve_setup_ns = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - setup_started)
                .count());
    }

    void enqueue(const std::uint32_t state) {
        if (state >= queued.size()) queued.resize(state + 1, 0);
        if (queued[state]) return;
        queued[state] = 1;
        queue.push_back(state);
        peak_queue_size = std::max<std::uint32_t>(
            peak_queue_size, static_cast<std::uint32_t>(queue.size()));
    }

    bool same_kernel(
        const SparseRow& stored,
        const PendingSparseRow& pending) const {
        const std::size_t transition_count =
            pending.transitions == nullptr ? 0 : pending.transitions->size();
        const std::size_t choice_count =
            pending.choices == nullptr ? 0 : pending.choices->size();
        if (stored.transition_count != transition_count ||
            stored.choice_count != choice_count) {
            return false;
        }
        for (std::size_t i = 0; i < transition_count; ++i) {
            const std::uint64_t offset = stored.transition_offset + i;
            const OutcomeEntry& right = pending.transitions->at(i);
            if (sparse_successors.at(offset) != right.state ||
                sparse_probabilities.at(offset) != right.probability) {
                return false;
            }
        }
        for (std::size_t i = 0; i < choice_count; ++i) {
            const SparseChoiceGroup& left = sparse_choices.at(
                stored.choice_offset + i);
            const OutcomeChoiceGroup& right = pending.choices->at(i);
            if (left.probability != right.probability ||
                left.successor_count != right.states.size()) {
                return false;
            }
            for (std::size_t s = 0; s < right.states.size(); ++s) {
                if (sparse_choice_successors.at(
                        left.successor_offset + s) != right.states[s]) {
                    return false;
                }
            }
        }
        return true;
    }

    std::size_t kernel_hash(const PendingSparseRow& pending) const {
        const auto mix = [](std::size_t& hash, std::size_t value) {
            hash ^= value + 0x9e3779b97f4a7c15ULL + (hash << 6) +
                    (hash >> 2);
        };
        std::size_t hash = 2166136261u;
        if (pending.transitions != nullptr) {
            mix(hash, pending.transitions->size());
            for (const OutcomeEntry& entry : *pending.transitions) {
                mix(hash, entry.state);
                mix(hash, std::hash<double>{}(entry.probability));
            }
        }
        if (pending.choices != nullptr) {
            mix(hash, pending.choices->size());
            for (const OutcomeChoiceGroup& group : *pending.choices) {
                mix(hash, std::hash<double>{}(group.probability));
                mix(hash, group.states.size());
                for (const std::uint32_t state : group.states) {
                    mix(hash, state);
                }
            }
        }
        return hash;
    }

    void add_action_reason(
        const char* disposition,
        const std::string& action,
        const std::string& reason) {
        result.diagnostics.action_inclusion_reasons.push_back(
            std::string(disposition) + ":" + reason + ":" + action);
    }

    void record_cap(const std::string& name, bool state_cap = false) {
        if (std::find(result.diagnostics.cap_hits.begin(),
                      result.diagnostics.cap_hits.end(), name) ==
            result.diagnostics.cap_hits.end()) {
            result.diagnostics.cap_hits.push_back(name);
        }
        result.diagnostics.resource_cap_hit = true;
        if (state_cap) result.diagnostics.state_cap_hit = true;
    }

    void check_solver_byte_cap() {
        const std::uint64_t current = estimated_owned_bytes();
        peak_owned_bytes = std::max(peak_owned_bytes, current);
        if (current > options.max_solver_owned_bytes) {
            record_cap("max_solver_owned_bytes");
        }
    }

    void append_sparse_row(
        const std::uint32_t state,
        PendingSparseRow pending) {
        static const std::vector<OutcomeEntry> empty_transitions;
        static const std::vector<OutcomeChoiceGroup> empty_choices;
        static const std::vector<OutcomeChoiceOption> empty_choice_options;
        const auto& transitions = pending.transitions == nullptr
                                      ? empty_transitions
                                      : *pending.transitions;
        const auto& choices = pending.choices == nullptr
                                  ? empty_choices
                                  : *pending.choices;
        const auto& choice_options = pending.choice_options == nullptr
                                         ? empty_choice_options
                                         : *pending.choice_options;
        if (state >= state_rows.size()) state_rows.resize(state + 1);
        StateRowSpan& span = state_rows[state];
        for (std::uint32_t i = 0; i < span.count; ++i) {
            SparseRow& stored = sparse_rows.at(span.offset + i);
            if (!same_kernel(stored, pending)) continue;
            const std::string& candidate_id =
                calc.operators().at(pending.operator_index).id;
            const std::string& representative_id =
                calc.operators().at(stored.operator_index).id;
            if (pending.cost < stored.cost - 1e-12) {
                add_action_reason(
                    "pruned", representative_id,
                    "certified_equivalent_kernel_more_expensive_than_" +
                        candidate_id);
                stored.operator_index = pending.operator_index;
                stored.cost = pending.cost;
                stored.choice_option_offset = sparse_choice_options.size();
                stored.choice_option_count = static_cast<std::uint32_t>(
                    choice_options.size());
                sparse_choice_options.insert(
                    sparse_choice_options.end(),
                    choice_options.begin(), choice_options.end());
            } else if (std::abs(pending.cost - stored.cost) <= 1e-12) {
                ++result.diagnostics.equivalent_price_ties;
                add_action_reason(
                    "included", candidate_id,
                    "certified_equivalent_kernel_price_tie_with_" +
                        representative_id);
            } else {
                add_action_reason(
                    "pruned", candidate_id,
                    "certified_equivalent_kernel_more_expensive_than_" +
                        representative_id);
            }
            ++result.diagnostics.equivalent_actions_collapsed;
            return;
        }

        std::uint64_t transition_count = transitions.size();
        for (const OutcomeChoiceGroup& group : choices) {
            transition_count += group.states.size();
        }
        if (sparse_rows.size() >= options.max_state_action_rows) {
            throw SolverResourceLimit(
                "max_state_action_rows", options.max_state_action_rows);
        }

        SparseRow row;
        row.operator_index = pending.operator_index;
        row.cost = pending.cost;
        const std::size_t hash = kernel_hash(pending);
        const SparseRow* shared_kernel = nullptr;
        const auto found = kernel_rows_by_hash.find(hash);
        if (found != kernel_rows_by_hash.end()) {
            for (const std::uint64_t row_index : found->second) {
                const SparseRow& candidate = sparse_rows.at(row_index);
                if (same_kernel(candidate, pending)) {
                    shared_kernel = &candidate;
                    break;
                }
            }
        }
        if (shared_kernel != nullptr) {
            row.transition_offset = shared_kernel->transition_offset;
            row.transition_count = shared_kernel->transition_count;
            row.choice_offset = shared_kernel->choice_offset;
            row.choice_count = shared_kernel->choice_count;
        } else {
            if (sparse_successors.size() + sparse_choice_successors.size() +
                    transition_count > options.max_transitions) {
                throw SolverResourceLimit(
                    "max_transitions", options.max_transitions);
            }
            row.transition_offset = sparse_successors.size();
            row.transition_count = static_cast<std::uint32_t>(
                transitions.size());
            for (const OutcomeEntry& entry : transitions) {
                sparse_successors.push_back(entry.state);
                sparse_probabilities.push_back(entry.probability);
            }
            row.choice_offset = sparse_choices.size();
            row.choice_count = static_cast<std::uint32_t>(choices.size());
            for (const OutcomeChoiceGroup& group : choices) {
                SparseChoiceGroup stored;
                stored.successor_offset = sparse_choice_successors.size();
                stored.successor_count = static_cast<std::uint32_t>(
                    group.states.size());
                stored.probability = group.probability;
                sparse_choice_successors.insert(
                    sparse_choice_successors.end(), group.states.begin(),
                    group.states.end());
                sparse_choices.push_back(stored);
            }
        }
        row.choice_option_offset = sparse_choice_options.size();
        row.choice_option_count = static_cast<std::uint32_t>(
            choice_options.size());
        sparse_choice_options.insert(
            sparse_choice_options.end(), choice_options.begin(),
            choice_options.end());
        if (span.count == 0) span.offset = sparse_rows.size();
        sparse_rows.push_back(row);
        if (shared_kernel == nullptr) {
            kernel_rows_by_hash[hash].push_back(sparse_rows.size() - 1);
        }
        ++span.count;
        result.diagnostics.sparse_rows = sparse_rows.size();
        result.diagnostics.sparse_transitions =
            sparse_successors.size() + sparse_choice_successors.size();

        for (const OutcomeEntry& entry : transitions) {
            enqueue(entry.state);
        }
        for (const OutcomeChoiceGroup& group : choices) {
            for (const std::uint32_t successor : group.states) {
                enqueue(successor);
            }
        }
    }

    void expand_one() {
        const auto started = std::chrono::steady_clock::now();
        const std::uint32_t state = queue.front();
        queue.pop_front();
        if (state >= expanded.size()) expanded.resize(state + 1, 0);
        expanded[state] = 1;
        ++expanded_count;
        if (calc.is_goal_state(calc.state(state))) {
            result.diagnostics.expansion_ns += static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() - started)
                    .count());
            return;
        }

        try {
            for (const PricedOperator& priced : operators) {
                const PlannerOperator& planner =
                    calc.operators().at(priced.index);
                PendingSparseRow pending;
                pending.operator_index = priced.index;
                pending.cost = priced.cost;
                if (planner.kind == PlannerOperatorKind::FixedOption) {
                    const OptionKernel& kernel =
                        calc.option_kernel(state, priced.index);
                    if (!kernel.supported) {
                        if (!reported_unsupported[priced.index]) {
                            reported_unsupported[priced.index] = true;
                            result.diagnostics.skipped_unsupported.push_back(
                                planner.id);
                            add_action_reason(
                                "unsupported", planner.id,
                                "fixed_option_kernel_unavailable");
                        }
                        continue;
                    }
                    if (!kernel.legal) continue;
                    pending.transitions = &kernel.exits;
                } else {
                    const std::uint32_t action_index =
                        planner.primitive_action;
                    if (!action_legal(
                            session, calc.registry().actions[action_index],
                            calc.state(state))) {
                        continue;
                    }
                    const OutcomeDistribution& distribution =
                        calc.outcomes(state, action_index);
                    if (!distribution.supported) {
                        if (!reported_unsupported[priced.index]) {
                            reported_unsupported[priced.index] = true;
                            result.diagnostics.skipped_unsupported.push_back(
                                planner.id);
                            add_action_reason(
                                "unsupported", planner.id,
                                "exact_evaluator_unavailable");
                        }
                        continue;
                    }
                    if (distribution.choice_groups.empty()) {
                        pending.transitions = &distribution.entries;
                    } else {
                        pending.choices = &distribution.choice_groups;
                        pending.choice_options =
                            &distribution.choice_options;
                    }
                }
                try {
                    append_sparse_row(state, std::move(pending));
                } catch (...) {
                    if (planner.kind == PlannerOperatorKind::FixedOption) {
                        calc.release_option_kernel(state, priced.index);
                    } else {
                        calc.release_outcome(
                            state, planner.primitive_action);
                    }
                    throw;
                }
                if (planner.kind == PlannerOperatorKind::FixedOption) {
                    calc.release_option_kernel(state, priced.index);
                } else {
                    calc.release_outcome(state, planner.primitive_action);
                }
            }
        } catch (const SolverResourceLimit& limit) {
            record_cap(
                limit.cap_name(),
                limit.cap_name() == "max_discovered_states");
        }
        result.diagnostics.expansion_ns += static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - started)
                .count());
        if (!result.diagnostics.resource_cap_hit &&
            expanded_count % 64 == 0) {
            check_solver_byte_cap();
        }
    }

    void prepare_iteration() {
        const auto started = std::chrono::steady_clock::now();
        if (!queue.empty() &&
            expanded_count >= options.max_expanded_states) {
            result.diagnostics.state_cap_hit = true;
            record_cap("max_expanded_states", true);
        }
        result.diagnostics.expanded_states = expanded_count;

        const std::uint32_t state_count = calc.state_count();
        state_rows.resize(state_count);
        result.diagnostics.discovered_states = state_count;
        result.diagnostics.frontier_states = state_count - expanded_count;
        expanded.resize(state_count, 0);
        result.expanded = expanded;
        result.values.assign(state_count, kValueCeiling);
        result.policy.assign(state_count, PolicyOperatorRef{});
        result.unveil_preferences.assign(state_count, {});
        result.goal_states.assign(state_count, 0);
        for (std::uint32_t state = 0; state < state_count; ++state) {
            if (calc.is_goal_state(calc.state(state))) {
                result.goal_states[state] = 1;
                ++result.diagnostics.goal_states;
                result.values[state] = 0.0;
            } else if (!result.expanded[state]) {
                /* Past-the-cap frontier: no Bellman backing, keep infinite so
                 * plans through it are never preferred. */
                result.values[state] = kInfinity;
            }
        }
        residual = kValueCeiling;
        phase = SolvePhase::Iterating;
        result.diagnostics.reforge_frontier_work =
            calc.telemetry().reforge_frontier_work;
        check_solver_byte_cap();
        /* The solve now owns the compact CSR rows used by every later phase;
         * release evaluator distributions so transitions are stored once. A
         * reusable price-only context is the separate S7.5 cache-reuse pass. */
        calc.release_solve_transition_caches();
        peak_owned_bytes = std::max(
            peak_owned_bytes, estimated_owned_bytes());
        result.diagnostics.expansion_ns += static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - started)
                .count());
    }

    double operator_q(
        const std::uint32_t state,
        const PricedOperator& priced) {
        const PlannerOperator& planner = calc.operators().at(priced.index);
        if (planner.kind == PlannerOperatorKind::FixedOption) {
            const OptionKernel& kernel =
                calc.option_kernel(state, priced.index);
            if (!kernel.supported || !kernel.legal) return kInfinity;
            double expected = priced.cost;
            for (const OutcomeEntry& exit : kernel.exits) {
                const double value = result.values[exit.state];
                if (value == kInfinity) return kInfinity;
                expected += exit.probability * value;
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

    double sparse_row_q(
        const SparseRow& row,
        std::uint32_t& transition_work) const {
        double expected = row.cost;
        transition_work = 0;
        if (row.choice_count != 0) {
            for (std::uint32_t i = 0; i < row.choice_count; ++i) {
                const SparseChoiceGroup& group = sparse_choices.at(
                    row.choice_offset + i);
                double best = kInfinity;
                for (std::uint32_t s = 0; s < group.successor_count; ++s) {
                    best = std::min(
                        best,
                        result.values[sparse_choice_successors.at(
                            group.successor_offset + s)]);
                }
                transition_work += group.successor_count;
                if (best == kInfinity) return kInfinity;
                expected += group.probability * best;
            }
            return expected;
        }
        for (std::uint32_t i = 0; i < row.transition_count; ++i) {
            const std::uint64_t offset = row.transition_offset + i;
            const double value = result.values[sparse_successors.at(offset)];
            ++transition_work;
            if (value == kInfinity) return kInfinity;
            expected += sparse_probabilities.at(offset) * value;
        }
        return expected;
    }

    void begin_sweep() {
        sweep_active = true;
        sweep_state = 0;
        sweep_row = 0;
        sweep_best = kInfinity;
        sweep_residual = 0.0;
        ++sweeps;
    }

    void finish_sweep() {
        residual = sweep_residual;
        sweep_active = false;
        result.diagnostics.sweeps = sweeps;
        result.diagnostics.residual = residual;
    }

    void finish_bellman_state() {
        const double before = result.values[sweep_state];
        if (sweep_best < before) {
            sweep_residual = std::max(
                sweep_residual, before - sweep_best);
            result.values[sweep_state] = sweep_best;
        }
        ++sweep_state;
        sweep_row = 0;
        sweep_best = kInfinity;
    }

    void run_bellman_unit() {
        const auto started = std::chrono::steady_clock::now();
        if (!sweep_active) begin_sweep();
        const std::uint32_t state_count =
            static_cast<std::uint32_t>(result.values.size());
        constexpr std::uint32_t kRowsPerUnit = 256;
        constexpr std::uint32_t kTransitionsPerUnit = 4096;
        std::uint32_t rows_done = 0;
        std::uint32_t transitions_done = 0;
        while (sweep_state < state_count && rows_done < kRowsPerUnit &&
               (transitions_done < kTransitionsPerUnit || rows_done == 0)) {
            if (!result.expanded[sweep_state] ||
                result.goal_states[sweep_state]) {
                ++sweep_state;
                sweep_row = 0;
                sweep_best = kInfinity;
                continue;
            }
            const StateRowSpan& span = state_rows.at(sweep_state);
            if (sweep_row == 0) {
                ++result.diagnostics.bellman_backups;
            }
            if (sweep_row >= span.count) {
                finish_bellman_state();
                ++rows_done;
                continue;
            }
            const SparseRow& row = sparse_rows.at(
                span.offset + sweep_row);
            std::uint32_t row_work = 0;
            const double q = sparse_row_q(row, row_work);
            sweep_best = std::min(sweep_best, q);
            ++sweep_row;
            ++rows_done;
            transitions_done += row_work;
            ++result.diagnostics.bellman_action_evaluations;
            if (sweep_row >= span.count) finish_bellman_state();
        }
        if (sweep_state >= state_count) finish_sweep();
        ++result.diagnostics.bellman_work_units;
        result.diagnostics.max_bellman_unit_transitions = std::max(
            result.diagnostics.max_bellman_unit_transitions,
            transitions_done);
        result.diagnostics.optimization_ns += static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - started)
                .count());
    }

    void step(std::uint32_t max_work_items) {
        std::uint32_t remaining = std::max<std::uint32_t>(1, max_work_items);
        while (remaining > 0 && phase != SolvePhase::Done) {
            if (phase == SolvePhase::Expanding) {
                if (queue.empty() ||
                    expanded_count >= options.max_expanded_states) {
                    prepare_iteration();
                    if (result.diagnostics.resource_cap_hit) {
                        phase = SolvePhase::Done;
                    }
                    break; /* expose the phase boundary to callers */
                }
                expand_one();
                --remaining;
                if (result.diagnostics.resource_cap_hit || queue.empty() ||
                    expanded_count >= options.max_expanded_states) {
                    prepare_iteration();
                    if (result.diagnostics.resource_cap_hit) {
                        phase = SolvePhase::Done;
                    }
                    break;
                }
                continue;
            }

            if (!sweep_active &&
                (residual <= options.epsilon ||
                 sweeps >= options.max_sweeps)) {
                phase = SolvePhase::Done;
                break;
            }
            run_bellman_unit();
            --remaining;
            if (!sweep_active &&
                (residual <= options.epsilon ||
                 sweeps >= options.max_sweeps)) {
                phase = SolvePhase::Done;
            }
        }
    }

    SolveProgress progress() const {
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
        return value;
    }

    SolveTelemetrySnapshot telemetry_snapshot(bool abandoned) const {
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
            if (calc.is_goal_state(calc.state(state))) {
                ++snapshot.diagnostics.goal_states;
            }
        }
        snapshot.diagnostics.sweeps = sweeps;
        snapshot.diagnostics.residual = residual;
        snapshot.diagnostics.solver_owned_bytes_estimate =
            std::max(peak_owned_bytes, estimated_owned_bytes());
        snapshot.raw_start_bound = progress().start_value_bound;
        return snapshot;
    }

    SolveResult finish() {
        if (phase != SolvePhase::Done) {
            throw std::logic_error("solver work is not finished");
        }
        if (consumed) {
            throw std::logic_error("solver work was already finished");
        }
        const auto extraction_started = std::chrono::steady_clock::now();

        const std::uint32_t state_count =
            static_cast<std::uint32_t>(result.values.size());
        /* Deterministic argmin: cost ties break toward lower cost-to-go
         * variance, then lower action index by stable registry traversal. */
        for (std::uint32_t state = 0; state < state_count; ++state) {
            if (!result.expanded[state] || result.goal_states[state]) continue;
            double best_q = kInfinity;
            double best_variance = kInfinity;
            std::uint32_t best_operator = kNoId;
            const SparseRow* best_row = nullptr;
            const StateRowSpan& span = state_rows.at(state);
            for (std::uint32_t row_index = 0; row_index < span.count;
                 ++row_index) {
                const SparseRow& row = sparse_rows.at(
                    span.offset + row_index);
                ++result.diagnostics.extraction_action_evaluations;
                std::uint32_t transition_work = 0;
                const double q = sparse_row_q(row, transition_work);
                if (q == kInfinity) continue;
                double mean = 0.0;
                std::vector<std::pair<double, double>> random_values;
                if (row.choice_count == 0) {
                    for (std::uint32_t i = 0;
                         i < row.transition_count; ++i) {
                        const std::uint64_t offset =
                            row.transition_offset + i;
                        random_values.push_back(
                            {sparse_probabilities.at(offset),
                             result.values[sparse_successors.at(offset)]});
                        mean += sparse_probabilities.at(offset) *
                                result.values[sparse_successors.at(offset)];
                    }
                } else {
                    for (std::uint32_t i = 0; i < row.choice_count; ++i) {
                        const SparseChoiceGroup& group = sparse_choices.at(
                            row.choice_offset + i);
                        double chosen = kInfinity;
                        for (std::uint32_t s = 0;
                             s < group.successor_count; ++s) {
                            chosen = std::min(
                                chosen,
                                result.values[sparse_choice_successors.at(
                                    group.successor_offset + s)]);
                        }
                        random_values.push_back(
                            {group.probability, chosen});
                        mean += group.probability * chosen;
                    }
                }
                double variance = 0.0;
                for (const auto& [probability, value] : random_values) {
                    const double delta = value - mean;
                    variance += probability * delta * delta;
                }
                const bool better =
                    q < best_q - 1e-12 ||
                    (q < best_q + 1e-12 &&
                     variance < best_variance - 1e-12);
                if (better) {
                    best_q = q;
                    best_variance = variance;
                    best_operator = row.operator_index;
                    best_row = &row;
                }
            }
            result.policy[state] =
                best_operator == kNoId
                    ? PolicyOperatorRef{}
                    : PolicyOperatorRef{
                          calc.operators()[best_operator].kind,
                          best_operator};
            if (best_operator != kNoId && best_row != nullptr &&
                calc.operators()[best_operator].kind ==
                    PlannerOperatorKind::Primitive &&
                calc.registry()
                        .actions[calc.operators()[best_operator]
                                     .primitive_action]
                        .params.type == ActionType::Unveil) {
                std::vector<OutcomeChoiceOption> choice_options;
                for (std::uint32_t i = 0;
                     i < best_row->choice_option_count; ++i) {
                    choice_options.push_back(sparse_choice_options.at(
                        best_row->choice_option_offset + i));
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
                    result.unveil_preferences[state].push_back(
                        option.mod_id);
                }
            }
        }

        result.policy_reachable.assign(state_count, 0);
        if (result.start_state < state_count) {
            std::deque<std::uint32_t> walk{result.start_state};
            while (!walk.empty()) {
                const std::uint32_t state = walk.front();
                walk.pop_front();
                if (result.policy_reachable[state]) continue;
                result.policy_reachable[state] = 1;
                ++result.diagnostics.policy_reachable_states;
                const std::uint32_t operator_index = result.policy[state];
                if (operator_index == kNoId) continue;
                const StateRowSpan& span = state_rows.at(state);
                const SparseRow* selected = nullptr;
                for (std::uint32_t i = 0; i < span.count; ++i) {
                    const SparseRow& row = sparse_rows.at(span.offset + i);
                    if (row.operator_index == operator_index) {
                        selected = &row;
                        break;
                    }
                }
                if (selected == nullptr) continue;
                for (std::uint32_t i = 0;
                     i < selected->transition_count; ++i) {
                    const std::uint32_t successor = sparse_successors.at(
                        selected->transition_offset + i);
                    if (!result.policy_reachable[successor]) {
                        walk.push_back(successor);
                    }
                }
                for (std::uint32_t i = 0; i < selected->choice_count; ++i) {
                    const SparseChoiceGroup& group = sparse_choices.at(
                        selected->choice_offset + i);
                    for (std::uint32_t s = 0; s < group.successor_count; ++s) {
                        const std::uint32_t successor =
                            sparse_choice_successors.at(
                                group.successor_offset + s);
                        if (!result.policy_reachable[successor]) {
                            walk.push_back(successor);
                        }
                    }
                }
            }
        }

        result.converged = !result.diagnostics.state_cap_hit &&
                           !result.diagnostics.resource_cap_hit &&
                           residual <= options.epsilon &&
                           result.start_state < state_count &&
                           result.values[result.start_state] < kValueCeiling;
        result.diagnostics.extraction_ns = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - extraction_started)
                .count());
        result.diagnostics.solver_owned_bytes_estimate =
            std::max(peak_owned_bytes, estimated_owned_bytes());
        consumed = true;
        return std::move(result);
    }

    std::uint64_t estimated_owned_bytes() const {
        std::uint64_t bytes = sizeof(*this) + calc.estimated_owned_bytes();
        bytes += operators.capacity() * sizeof(PricedOperator);
        bytes += (reported_unsupported.capacity() + 7) / 8;
        bytes += expanded.capacity() * sizeof(std::uint8_t);
        bytes += queued.capacity() * sizeof(std::uint8_t);
        bytes += static_cast<std::uint64_t>(peak_queue_size) *
                 sizeof(std::uint32_t);
        bytes += state_rows.capacity() * sizeof(StateRowSpan);
        bytes += sparse_rows.capacity() * sizeof(SparseRow);
        bytes += sparse_successors.capacity() * sizeof(std::uint32_t);
        bytes += sparse_probabilities.capacity() * sizeof(double);
        bytes += sparse_choices.capacity() * sizeof(SparseChoiceGroup);
        bytes += sparse_choice_successors.capacity() * sizeof(std::uint32_t);
        bytes += sparse_choice_options.capacity() *
                 sizeof(OutcomeChoiceOption);
        bytes += kernel_rows_by_hash.bucket_count() * sizeof(void*);
        bytes += kernel_rows_by_hash.size() *
                 (sizeof(std::pair<const std::size_t,
                                   std::vector<std::uint64_t>>) +
                  2 * sizeof(void*));
        for (const auto& [unused, rows] : kernel_rows_by_hash) {
            (void)unused;
            bytes += rows.capacity() * sizeof(std::uint64_t);
        }
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
        const auto string_vector_bytes = [](const auto& values) {
            std::uint64_t total =
                values.capacity() * sizeof(std::string);
            for (const std::string& value : values) {
                total += value.capacity() + 1;
            }
            return total;
        };
        bytes += string_vector_bytes(
            result.diagnostics.skipped_missing_price);
        bytes += string_vector_bytes(result.diagnostics.skipped_unsupported);
        bytes += string_vector_bytes(
            result.diagnostics.action_inclusion_reasons);
        bytes += string_vector_bytes(result.diagnostics.cap_hits);
        return bytes;
    }
};

SolveWork::SolveWork(
    CalcContext& calc,
    const pc_item_state& start_item,
    const std::unordered_map<std::string, double>& prices,
    const SolveOptions& options)
    : impl_(std::make_unique<Impl>(calc, start_item, prices, options)) {}

SolveWork::~SolveWork() = default;
SolveWork::SolveWork(SolveWork&&) noexcept = default;
SolveWork& SolveWork::operator=(SolveWork&&) noexcept = default;

void SolveWork::step(const std::uint32_t max_work_items) {
    impl_->step(max_work_items);
}

SolveProgress SolveWork::progress() const {
    return impl_->progress();
}

SolveTelemetrySnapshot SolveWork::telemetry_snapshot(bool abandoned) const {
    return impl_->telemetry_snapshot(abandoned);
}

SolveResult SolveWork::finish() {
    return impl_->finish();
}

SolveResult solve(
    CalcContext& calc,
    const pc_item_state& start_item,
    const std::unordered_map<std::string, double>& prices,
    const SolveOptions& options) {
    SolveWork work(calc, start_item, prices, options);
    while (!work.progress().done) work.step(4096);
    return work.finish();
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
        (!diagnostics->skipped_missing_price.empty() ||
         diagnostics->evaluator_supported_actions <
             diagnostics->candidate_actions ||
         !diagnostics->skipped_unsupported.empty());
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

    std::string json = "{\"version\":\"solver_telemetry_v1\"";
    json += ",\"availability\":{";
    json += "\"evaluator_support\":\"applied_before_expansion\"";
    json += ",\"relevance_filter\":\"explicit_envelope_or_conservative_include\"";
    json += ",\"dominance_filter\":\"certified_abstract_kernel_equivalence\"";
    json += ",\"policy_improvement_rounds\":\"not_applicable\"";
    json += ",\"optimality_gap\":\"not_available\"";
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
                    diagnostics->equivalent_actions_collapsed);
        json += ",\"deferred\":" + std::to_string(
                    diagnostics->deferred_actions);
        json += ",\"equivalent_price_ties\":" + std::to_string(
                    diagnostics->equivalent_price_ties);
        json += ",\"missing_price\":" + std::to_string(
                    diagnostics->skipped_missing_price.size());
        json += ",\"unsupported_observed\":" + std::to_string(
                    diagnostics->skipped_unsupported.size());
    }
    json += "}";

    json += ",\"action_control\":{";
    json += "\"explicit_envelope\":" + std::string(bool_json(
        calc.action_control().explicit_envelope));
    json += ",\"dependency_primitives\":" + std::to_string(
        calc.action_control().dependency_primitives);
    json += ",\"fossil_loadouts\":{";
    json += "\"possible\":" + std::to_string(
        calc.registry().fossil_loadouts_possible);
    json += ",\"generated\":" + std::to_string(
        calc.registry().fossil_loadouts_generated);
    json += ",\"deferred\":" + std::to_string(
        calc.registry().fossil_loadouts_deferred);
    json += ",\"lazy\":" + std::string(bool_json(
        calc.registry().fossil_generation_lazy)) + "}";
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
    json += "]}";

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

    json += ",\"states\":{";
    if (diagnostics == nullptr) {
        json += "\"discovered\":null,\"expanded\":null,\"frontier\":null";
        json += ",\"goal\":null,\"policy_reachable\":null";
    } else {
        json += "\"discovered\":" +
                std::to_string(diagnostics->discovered_states);
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
    json += ",\"reforge\":{\"requests\":" +
            std::to_string(cache.reforge_requests);
    json += ",\"hits\":" + std::to_string(cache.reforge_hits);
    json += ",\"misses\":" + std::to_string(cache.reforge_misses);
    json += ",\"entries\":" +
            std::to_string(calc.cached_reforge_count());
    json += ",\"build_ns\":" +
            std::to_string(cache.reforge_build_ns);
    json += ",\"frontier_work\":" +
            std::to_string(cache.reforge_frontier_work) + "}}";

    json += ",\"optimization\":{";
    json += "\"method\":\"value_iteration\"";
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
        json += ",\"policy_improvement_rounds\":null";
        json += ",\"residual\":" + std::to_string(diagnostics->residual);
        json += ",\"optimality_gap\":null";
        json += ",\"state_cap_hit\":" +
                std::string(bool_json(diagnostics->state_cap_hit));
        json += ",\"resource_cap_hit\":" +
                std::string(bool_json(diagnostics->resource_cap_hit));
        json += ",\"cap_hits\":[]";
        json += ",\"full_request_status\":\"incomplete_not_finished\"";
    } else {
        const char* status = result->converged
                                 ? (qualified_action_subset
                                        ? "exact_supported_priced_subset"
                                        : "exact_abstract")
                                 : (diagnostics->resource_cap_hit
                                        ? "incomplete_resource_cap"
                                        : (diagnostics->state_cap_hit
                                               ? "incomplete_state_cap"
                                               : "not_converged"));
        json += ",\"status\":\"" + std::string(status) + "\"";
        json += ",\"converged\":" +
                std::string(bool_json(result->converged));
        json += ",\"sweeps\":" + std::to_string(diagnostics->sweeps);
        json += ",\"policy_improvement_rounds\":null";
        json += ",\"residual\":" + std::to_string(diagnostics->residual);
        json += ",\"optimality_gap\":null";
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
        if (diagnostics->resource_cap_hit) {
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

    json += ",\"timings_ns\":{\"registry_generation\":" +
            optional_count(registry_generation_ns);
    json += ",\"abstract_layout\":" +
            std::to_string(calc.layout_build_ns());
    if (diagnostics == nullptr) {
        json += ",\"solve_setup\":null,\"expansion\":null";
        json += ",\"transition_calculation\":null,\"optimization\":null";
        json += ",\"extraction\":null";
    } else {
        json += ",\"solve_setup\":" +
                std::to_string(diagnostics->solve_setup_ns);
        json += ",\"expansion\":" +
                std::to_string(diagnostics->expansion_ns);
        json += ",\"transition_calculation\":" +
                std::to_string(cache.distribution_build_ns);
        json += ",\"optimization\":" +
                std::to_string(diagnostics->optimization_ns);
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

    const std::uint64_t current_bytes = calc.estimated_owned_bytes();
    json += ",\"memory\":{\"solver_owned_bytes_estimate\":" +
            std::to_string(diagnostics == nullptr
                               ? current_bytes
                               : diagnostics->solver_owned_bytes_estimate);
    json += ",\"estimate_kind\":\"selected_allocations_not_process_heap\"";
    json += ",\"abstract_state_bytes\":" +
            std::to_string(sizeof(AbstractState));
    json += ",\"state_payload\":\"inline_sparse_junk_counts\"}";

    json += ",\"compilation\":{";
    if (compilation == nullptr) {
        json += "\"available\":false,\"working_states\":null";
        json += ",\"nodes\":null,\"edges\":null";
        json += ",\"strategy_json_bytes\":null";
        json += ",\"cap_hit\":null";
    } else {
        json += "\"available\":true,\"working_states\":" +
                std::to_string(compilation->working_states);
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
    if (!has_result_bound || !result->converged) {
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
        json += qualified_action_subset
                    ? "\"exact_supported_priced_subset_within_tolerance\""
                    : "\"exact_abstract_within_tolerance\"";
    } else {
        json += "\"unavailable_incomplete_solve\"";
    }
    json += ",\"start_scope\":";
    if (result == nullptr || !result->converged) {
        json += "null";
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
    return json;
}

} // namespace solver
} // namespace poecraft
