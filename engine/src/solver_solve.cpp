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
        calc.reset_solve_telemetry();
        result.diagnostics.registry_actions = static_cast<std::uint32_t>(
            calc.registry().actions.size());
        result.diagnostics.candidate_actions = static_cast<std::uint32_t>(
            calc.candidates().size());
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
                continue;
            }
            operators.push_back({index, cost});
            ++result.diagnostics.priced_scanned_actions;
            const bool supported =
                planner.kind == PlannerOperatorKind::FixedOption ||
                calc_supports(calc.registry().actions.at(
                    planner.primitive_action));
            if (supported) ++result.diagnostics.supported_priced_actions;
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

        for (const PricedOperator& priced : operators) {
            const PlannerOperator& planner =
                calc.operators().at(priced.index);
            if (planner.kind == PlannerOperatorKind::FixedOption) {
                const OptionKernel& kernel =
                    calc.option_kernel(state, priced.index);
                if (!kernel.supported) {
                    if (!reported_unsupported[priced.index]) {
                        reported_unsupported[priced.index] = true;
                        result.diagnostics.skipped_unsupported.push_back(
                            planner.id);
                    }
                    continue;
                }
                if (!kernel.legal) continue;
                for (const OutcomeEntry& exit : kernel.exits) {
                    enqueue(exit.state);
                }
                continue;
            }

            const std::uint32_t action_index = planner.primitive_action;
            if (!action_legal(session,
                              calc.registry().actions[action_index],
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
                }
                continue;
            }
            for (const OutcomeEntry& entry : distribution.entries) {
                enqueue(entry.state);
            }
            for (const OutcomeChoiceGroup& group :
                 distribution.choice_groups) {
                for (std::uint32_t successor : group.states) {
                    enqueue(successor);
                }
            }
        }
        result.diagnostics.expansion_ns += static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - started)
                .count());
    }

    void prepare_iteration() {
        const auto started = std::chrono::steady_clock::now();
        if (!queue.empty() && expanded_count >= options.max_states) {
            result.diagnostics.state_cap_hit = true;
        }
        result.diagnostics.expanded_states = expanded_count;

        const std::uint32_t state_count = calc.state_count();
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

    void run_sweep() {
        const auto started = std::chrono::steady_clock::now();
        residual = 0.0;
        ++sweeps;
        const std::uint32_t state_count =
            static_cast<std::uint32_t>(result.values.size());
        for (std::uint32_t state = 0; state < state_count; ++state) {
            if (!result.expanded[state] || result.goal_states[state]) {
                continue;
            }
            ++result.diagnostics.bellman_backups;
            double best = kInfinity;
            for (const PricedOperator& planner : operators) {
                ++result.diagnostics.bellman_action_evaluations;
                best = std::min(best, operator_q(state, planner));
            }
            /* Monotone descent from the ceiling: updates never raise a
             * value, so unreachable-to-goal states settle at the ceiling. */
            const double before = result.values[state];
            if (best >= before) continue;
            residual = std::max(residual, before - best);
            result.values[state] = best;
        }
        result.diagnostics.sweeps = sweeps;
        result.diagnostics.residual = residual;
        result.diagnostics.optimization_ns += static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - started)
                .count());
    }

    void step(std::uint32_t max_work_items) {
        std::uint32_t remaining = std::max<std::uint32_t>(1, max_work_items);
        while (remaining > 0 && phase != SolvePhase::Done) {
            if (phase == SolvePhase::Expanding) {
                if (queue.empty() || expanded_count >= options.max_states) {
                    prepare_iteration();
                    break; /* expose the phase boundary to callers */
                }
                expand_one();
                --remaining;
                if (queue.empty() || expanded_count >= options.max_states) {
                    prepare_iteration();
                    break;
                }
                continue;
            }

            if (residual <= options.epsilon || sweeps >= options.max_sweeps) {
                phase = SolvePhase::Done;
                break;
            }
            run_sweep();
            --remaining;
            if (residual <= options.epsilon || sweeps >= options.max_sweeps) {
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
            estimated_owned_bytes();
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
            for (const PricedOperator& priced : operators) {
                ++result.diagnostics.extraction_action_evaluations;
                const double q = operator_q(state, priced);
                if (q == kInfinity) continue;
                double mean = 0.0;
                std::vector<std::pair<double, double>> random_values;
                const PlannerOperator& planner =
                    calc.operators().at(priced.index);
                if (planner.kind == PlannerOperatorKind::FixedOption) {
                    const OptionKernel& kernel =
                        calc.option_kernel(state, priced.index);
                    for (const OutcomeEntry& exit : kernel.exits) {
                        random_values.push_back(
                            {exit.probability, result.values[exit.state]});
                        mean += exit.probability * result.values[exit.state];
                    }
                } else {
                    const OutcomeDistribution& distribution = calc.outcomes(
                        state, planner.primitive_action);
                    if (!distribution.choice_groups.empty()) {
                        for (const OutcomeChoiceGroup& group :
                             distribution.choice_groups) {
                            double chosen = kInfinity;
                            for (std::uint32_t successor : group.states) {
                                chosen = std::min(
                                    chosen, result.values[successor]);
                            }
                            random_values.push_back(
                                {group.probability, chosen});
                            mean += group.probability * chosen;
                        }
                    } else {
                        for (const OutcomeEntry& entry :
                             distribution.entries) {
                            random_values.push_back(
                                {entry.probability,
                                 result.values[entry.state]});
                            mean += entry.probability *
                                    result.values[entry.state];
                        }
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
                    best_operator = priced.index;
                }
            }
            result.policy[state] =
                best_operator == kNoId
                    ? PolicyOperatorRef{}
                    : PolicyOperatorRef{
                          calc.operators()[best_operator].kind,
                          best_operator};
            if (best_operator != kNoId &&
                calc.operators()[best_operator].kind ==
                    PlannerOperatorKind::Primitive &&
                calc.registry()
                        .actions[calc.operators()[best_operator]
                                     .primitive_action]
                        .params.type == ActionType::Unveil) {
                const std::uint32_t unveil_action =
                    calc.operators()[best_operator].primitive_action;
                const OutcomeDistribution& distribution =
                    calc.outcomes(state, unveil_action);
                std::vector<OutcomeChoiceOption> options =
                    distribution.choice_options;
                std::sort(
                    options.begin(), options.end(),
                    [&](const OutcomeChoiceOption& a,
                        const OutcomeChoiceOption& b) {
                        const double left = result.values[a.state];
                        const double right = result.values[b.state];
                        return left != right ? left < right
                                             : a.mod_id < b.mod_id;
                    });
                for (const OutcomeChoiceOption& option : options) {
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
                const PlannerOperator& planner =
                    calc.operators().at(operator_index);
                if (planner.kind == PlannerOperatorKind::FixedOption) {
                    const OptionKernel& kernel =
                        calc.option_kernel(state, operator_index);
                    for (const OutcomeEntry& exit : kernel.exits) {
                        if (!result.policy_reachable[exit.state]) {
                            walk.push_back(exit.state);
                        }
                    }
                    continue;
                }
                const OutcomeDistribution& distribution = calc.outcomes(
                    state, planner.primitive_action);
                for (const OutcomeEntry& entry : distribution.entries) {
                    if (!result.policy_reachable[entry.state]) {
                        walk.push_back(entry.state);
                    }
                }
                for (const OutcomeChoiceGroup& group :
                     distribution.choice_groups) {
                    for (std::uint32_t successor : group.states) {
                        if (!result.policy_reachable[successor]) {
                            walk.push_back(successor);
                        }
                    }
                }
            }
        }

        result.converged = !result.diagnostics.state_cap_hit &&
                           residual <= options.epsilon &&
                           result.start_state < state_count &&
                           result.values[result.start_state] < kValueCeiling;
        result.diagnostics.extraction_ns = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - extraction_started)
                .count());
        result.diagnostics.solver_owned_bytes_estimate =
            estimated_owned_bytes();
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
    json += "\"evaluator_support\":\"diagnostic_not_applied_filter\"";
    json += ",\"relevance_filter\":\"not_implemented\"";
    json += ",\"dominance_filter\":\"not_implemented\"";
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
    json += ",\"candidate\":" + std::to_string(candidate_actions);
    if (diagnostics == nullptr) {
        json += ",\"evaluator_supported\":null";
        json += ",\"priced_scanned\":null,\"supported_priced\":null";
        json += ",\"unsupported_requested\":null";
        json += ",\"relevance_reduced\":null,\"dominance_reduced\":null";
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
        json += ",\"relevance_reduced\":null,\"dominance_reduced\":null";
        json += ",\"missing_price\":" + std::to_string(
                    diagnostics->skipped_missing_price.size());
        json += ",\"unsupported_observed\":" + std::to_string(
                    diagnostics->skipped_unsupported.size());
    }
    json += "}";

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
            std::to_string(cache.state_action_rows);
    json += ",\"transition_entries\":" +
            std::to_string(cache.transition_entries);
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
    } else {
        json += ",\"bellman_backups\":" +
                std::to_string(diagnostics->bellman_backups);
        json += ",\"bellman_action_evaluations\":" +
                std::to_string(diagnostics->bellman_action_evaluations);
        json += ",\"extraction_action_evaluations\":" +
                std::to_string(diagnostics->extraction_action_evaluations);
    }
    json += "}";

    json += ",\"cache\":{\"distribution\":{\"requests\":" +
            std::to_string(cache.distribution_requests);
    json += ",\"hits\":" + std::to_string(cache.distribution_hits);
    json += ",\"misses\":" + std::to_string(cache.distribution_misses);
    json += ",\"entries\":" +
            std::to_string(calc.cached_distribution_count());
    json += ",\"build_ns\":" +
            std::to_string(cache.distribution_build_ns) + "}";
    json += ",\"reforge\":{\"requests\":" +
            std::to_string(cache.reforge_requests);
    json += ",\"hits\":" + std::to_string(cache.reforge_hits);
    json += ",\"misses\":" + std::to_string(cache.reforge_misses);
    json += ",\"entries\":" +
            std::to_string(calc.cached_reforge_count());
    json += ",\"build_ns\":" +
            std::to_string(cache.reforge_build_ns) + "}}";

    json += ",\"optimization\":{";
    json += "\"method\":\"value_iteration\"";
    if (result == nullptr && snapshot == nullptr) {
        json += ",\"status\":\"not_run\",\"converged\":null";
        json += ",\"sweeps\":null,\"policy_improvement_rounds\":null";
        json += ",\"residual\":null,\"optimality_gap\":null";
        json += ",\"state_cap_hit\":null";
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
        json += ",\"full_request_status\":\"incomplete_not_finished\"";
    } else {
        const char* status = result->converged
                                 ? (qualified_action_subset
                                        ? "exact_supported_priced_subset"
                                        : "exact_abstract")
                                 : (diagnostics->state_cap_hit
                                        ? "incomplete_state_cap"
                                        : "not_converged");
        json += ",\"status\":\"" + std::string(status) + "\"";
        json += ",\"converged\":" +
                std::string(bool_json(result->converged));
        json += ",\"sweeps\":" + std::to_string(diagnostics->sweeps);
        json += ",\"policy_improvement_rounds\":null";
        json += ",\"residual\":" + std::to_string(diagnostics->residual);
        json += ",\"optimality_gap\":null";
        json += ",\"state_cap_hit\":" +
                std::string(bool_json(diagnostics->state_cap_hit));
        json += ",\"full_request_status\":\"";
        if (!result->converged) {
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
    json += ",\"estimate_kind\":\"selected_allocations_not_process_heap\"}";

    json += ",\"compilation\":{";
    if (compilation == nullptr) {
        json += "\"available\":false,\"working_states\":null";
        json += ",\"nodes\":null,\"edges\":null";
        json += ",\"strategy_json_bytes\":null";
    } else {
        json += "\"available\":true,\"working_states\":" +
                std::to_string(compilation->working_states);
        json += ",\"nodes\":" + std::to_string(compilation->nodes);
        json += ",\"edges\":" + std::to_string(compilation->edges);
        json += ",\"strategy_json_bytes\":" +
                std::to_string(compilation->strategy_json_bytes);
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
