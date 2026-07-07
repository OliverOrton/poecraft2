#include "solver_internal.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <deque>
#include <limits>
#include <string>
#include <unordered_map>
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

struct PricedAction {
    std::uint32_t index = kNoId;
    double cost = 0.0;
};

} // namespace

SolveResult solve(
    CalcContext& calc,
    const pc_item_state& start_item,
    const std::unordered_map<std::string, double>& prices,
    const SolveOptions& options) {
    const SessionImpl& session = calc.session();
    SolveResult result;

    /* Price the candidate actions; unpriced or unsupported actions are
     * excluded from planning and reported, never silently costed. */
    std::vector<PricedAction> actions;
    std::vector<bool> reported_unsupported(calc.registry().actions.size(),
                                           false);
    for (std::uint32_t index : calc.candidates()) {
        const ActionDescriptor& descriptor =
            calc.registry().actions.at(index);
        double cost = 0.0;
        bool priced = true;
        for (const std::string& key : descriptor.cost_keys) {
            const auto it = prices.find(key);
            if (it == prices.end()) {
                priced = false;
                break;
            }
            cost += it->second;
        }
        if (!priced) {
            result.diagnostics.skipped_missing_price.push_back(
                descriptor.id);
            continue;
        }
        actions.push_back({index, cost});
    }

    /* --- reachable closure from the start item ----------------------------- */
    result.start_state = calc.intern_item(start_item);
    std::vector<std::uint8_t> expanded;
    std::deque<std::uint32_t> queue;
    std::vector<std::uint8_t> queued;
    const auto enqueue = [&](std::uint32_t state) {
        if (state >= queued.size()) queued.resize(state + 1, 0);
        if (queued[state]) return;
        queued[state] = 1;
        queue.push_back(state);
    };
    enqueue(result.start_state);
    std::uint32_t expanded_count = 0;
    while (!queue.empty()) {
        if (expanded_count >= options.max_states) {
            result.diagnostics.state_cap_hit = true;
            break;
        }
        const std::uint32_t state = queue.front();
        queue.pop_front();
        if (state >= expanded.size()) expanded.resize(state + 1, 0);
        expanded[state] = 1;
        ++expanded_count;
        if (calc.is_goal_state(calc.state(state))) continue;
        for (const PricedAction& action : actions) {
            if (!action_legal(session,
                              calc.registry().actions[action.index],
                              calc.state(state))) {
                continue;
            }
            const OutcomeDistribution& distribution =
                calc.outcomes(state, action.index);
            if (!distribution.supported) {
                if (!reported_unsupported[action.index]) {
                    reported_unsupported[action.index] = true;
                    result.diagnostics.skipped_unsupported.push_back(
                        calc.registry().actions[action.index].id);
                }
                continue;
            }
            for (const OutcomeEntry& entry : distribution.entries) {
                enqueue(entry.state);
            }
        }
    }
    result.diagnostics.expanded_states = expanded_count;

    const std::uint32_t state_count = calc.state_count();
    expanded.resize(state_count, 0);
    result.expanded = expanded;
    result.values.assign(state_count, kValueCeiling);
    result.policy.assign(state_count, kNoId);
    result.goal_states.assign(state_count, 0);
    for (std::uint32_t state = 0; state < state_count; ++state) {
        if (calc.is_goal_state(calc.state(state))) {
            result.goal_states[state] = 1;
            result.values[state] = 0.0;
        } else if (!result.expanded[state]) {
            /* Past-the-cap frontier: no Bellman backing, keep infinite so
             * plans through it are never preferred. */
            result.values[state] = kInfinity;
        }
    }

    /* --- in-place value iteration ------------------------------------------ */
    const auto action_q = [&](std::uint32_t state,
                              const PricedAction& action) -> double {
        if (!action_legal(session, calc.registry().actions[action.index],
                          calc.state(state))) {
            return kInfinity;
        }
        const OutcomeDistribution& distribution =
            calc.outcomes(state, action.index);
        if (!distribution.supported) return kInfinity;
        double expected = action.cost;
        for (const OutcomeEntry& entry : distribution.entries) {
            const double value = result.values[entry.state];
            if (value == kInfinity) return kInfinity;
            expected += entry.probability * value;
        }
        return expected;
    };

    double residual = kInfinity;
    std::uint32_t sweeps = 0;
    while (residual > options.epsilon && sweeps < options.max_sweeps) {
        residual = 0.0;
        ++sweeps;
        for (std::uint32_t state = 0; state < state_count; ++state) {
            if (!result.expanded[state] || result.goal_states[state]) {
                continue;
            }
            double best = kInfinity;
            for (const PricedAction& action : actions) {
                best = std::min(best, action_q(state, action));
            }
            /* Monotone descent from the ceiling: updates never raise a
             * value, so unreachable-to-goal states settle at the ceiling
             * instead of chasing cost(a) upward forever. */
            const double before = result.values[state];
            if (best >= before) continue;
            residual = std::max(residual, before - best);
            result.values[state] = best;
        }
    }
    result.diagnostics.sweeps = sweeps;
    result.diagnostics.residual = residual;

    /* --- policy extraction --------------------------------------------------
     * Deterministic argmin: cost ties break toward lower cost-to-go
     * variance, then lower action index, so identical inputs always yield
     * identical strategies. */
    for (std::uint32_t state = 0; state < state_count; ++state) {
        if (!result.expanded[state] || result.goal_states[state]) continue;
        double best_q = kInfinity;
        double best_variance = kInfinity;
        std::uint32_t best_action = kNoId;
        for (const PricedAction& action : actions) {
            const double q = action_q(state, action);
            if (q == kInfinity) continue;
            double variance = 0.0;
            {
                const OutcomeDistribution& distribution =
                    calc.outcomes(state, action.index);
                double mean = 0.0;
                for (const OutcomeEntry& entry : distribution.entries) {
                    mean += entry.probability * result.values[entry.state];
                }
                for (const OutcomeEntry& entry : distribution.entries) {
                    const double delta =
                        result.values[entry.state] - mean;
                    variance += entry.probability * delta * delta;
                }
            }
            const bool better =
                q < best_q - 1e-12 ||
                (q < best_q + 1e-12 && variance < best_variance - 1e-12);
            if (better) {
                best_q = q;
                best_variance = variance;
                best_action = action.index;
            }
        }
        result.policy[state] = best_action;
    }

    /* --- policy-reachable set (the states the policy can actually visit) --- */
    result.policy_reachable.assign(state_count, 0);
    if (result.start_state < state_count) {
        std::deque<std::uint32_t> walk{result.start_state};
        while (!walk.empty()) {
            const std::uint32_t state = walk.front();
            walk.pop_front();
            if (result.policy_reachable[state]) continue;
            result.policy_reachable[state] = 1;
            const std::uint32_t action = result.policy[state];
            if (action == kNoId) continue;
            const OutcomeDistribution& distribution =
                calc.outcomes(state, action);
            for (const OutcomeEntry& entry : distribution.entries) {
                if (!result.policy_reachable[entry.state]) {
                    walk.push_back(entry.state);
                }
            }
        }
    }

    result.converged = !result.diagnostics.state_cap_hit &&
                       residual <= options.epsilon &&
                       result.start_state < state_count &&
                       result.values[result.start_state] < kValueCeiling;
    return result;
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
            log += calc.registry().actions[action].id;
            log += '"';
        }
        std::snprintf(
            buffer, sizeof(buffer),
            ",\"goal\":%d,\"reachable\":%d,\"rarity\":%u,\"prefixes\":%u,"
            "\"suffixes\":%u,\"blocked\":%u,\"flags\":%u,\"slots\":[",
            result.goal_states[state] ? 1 : 0,
            result.policy_reachable[state] ? 1 : 0, features.rarity,
            features.prefix_count, features.suffix_count,
            features.blocked_mask, features.flags);
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

} // namespace solver
} // namespace poecraft
