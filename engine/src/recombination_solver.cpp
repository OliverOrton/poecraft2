#include "recombination_solver.hpp"
#include "calculator_currency.hpp"
#include "recombination_calculator.hpp"
#include "json.hpp"
#include "poecraft/bitset.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <stdexcept>

namespace poecraft {
namespace {
void require(bool ok, const char* why) { if (!ok) throw std::invalid_argument(why); }
bool finite_cost(double cost) { return std::isfinite(cost) && cost >= 0; }
struct Action {
    RecombDecisionKind kind;
    unsigned argument = 0;
    double cost = 0;
    std::vector<std::pair<unsigned, double>> exits;
};
struct State { std::vector<unsigned> items; std::vector<Action> actions; bool terminal = false; };
struct WorkGuard {
    const RecombSolverRequest& request;
    std::uint64_t spent = 0;
    void tick(std::uint64_t amount = 1) {
        if (request.cancelled && request.cancelled()) throw std::length_error("Inventory solve cancelled");
        if (amount > request.max_work - spent) throw std::length_error("Inventory numerical work cap reached");
        spent += amount;
    }
};

// Canonicalize represented fields, never padding or inactive slot bytes. Slot
// order remains represented: no unproved quotient by a desired-mod mask.
std::string item_key(const pc_item_state& item) {
    std::ostringstream out;
    for (auto v : {item.rarity, item.quality, item.memory_strands, item.lifecycle,
            item.item_flags, item.generic_influence_bits, item.searing_exarch_tier,
            item.eater_of_worlds_tier, item.socket_count, item.link_mask})
        out << unsigned(v) << ':';
    for (unsigned i = 0; i < item.socket_count; ++i) out << unsigned(item.socket_colors[i]) << ':';
    const auto slots = [&](const pc_mod_slot* values, unsigned count) {
        out << '/' << count << ':';
        for (unsigned i = 0; i < count; ++i) {
            const auto& slot = values[i];
            out << slot.mod_id << ':' << slot.group_id << ':' << unsigned(slot.flags)
                << ':' << unsigned(slot.roll_count) << ':';
            for (unsigned j = 0; j < slot.roll_count; ++j) out << slot.rolls[j] << ':';
            out << unsigned(slot.veiled_option_count) << ':';
            for (unsigned j = 0; j < slot.veiled_option_count; ++j) out << slot.veiled_option_mod_ids[j] << ':';
            out << slot.veiled_chosen_mod_id << ';';
        }
    };
    slots(item.prefixes, item.prefix_count); slots(item.suffixes, item.suffix_count);
    slots(item.implicits, item.implicit_count); slots(item.enchantments, item.enchantment_count);
    return out.str();
}
pc_item_state root_item(const pc_item_state& item, const SessionImpl& from, const SessionImpl& root) {
    require(from.data == root.data && from.base_index == root.base_index && from.item_level == root.item_level,
            "Recombination solver outcome leaves the selected base/level scope");
    auto result = item;
    const auto map = [&](pc_mod_slot* slots, unsigned count) {
        for (unsigned i = 0; i < count; ++i) {
            auto& slot = slots[i];
            const auto global = from.data->mod_global_ids.at(from.global_index.at(slot.mod_id));
            slot.mod_id = root.session_id_by_global_id.at(global);
            slot.group_id = static_cast<std::uint16_t>(root.primary_group.at(slot.mod_id));
            for (unsigned j = 0; j < slot.veiled_option_count; ++j) {
                const auto g = from.data->mod_global_ids.at(from.global_index.at(slot.veiled_option_mod_ids[j]));
                slot.veiled_option_mod_ids[j] = root.session_id_by_global_id.at(g);
            }
            if (slot.veiled_chosen_mod_id != PC_MOD_NONE) {
                const auto g = from.data->mod_global_ids.at(from.global_index.at(slot.veiled_chosen_mod_id));
                slot.veiled_chosen_mod_id = root.session_id_by_global_id.at(g);
            }
        }
    };
    map(result.prefixes, result.prefix_count); map(result.suffixes, result.suffix_count);
    map(result.implicits, result.implicit_count); map(result.enchantments, result.enchantment_count);
    return result;
}
bool admissible(const Action& action, const std::vector<bool>& winning) {
    for (const auto& [next, probability] : action.exits)
        if (probability > 0 && !winning.at(next)) return false;
    return !action.exits.empty();
}
// Greatest almost-sure reachability set. A positive branch into a losing state
// cannot be omitted merely because another branch hits the goal.
std::vector<bool> winning_states(const std::vector<State>& states) {
    std::vector<bool> winning(states.size(), true);
    for (;;) {
        std::vector<std::vector<unsigned>> reverse(states.size());
        std::queue<unsigned> pending; std::vector<bool> reaches(states.size(), false);
        for (unsigned s = 0; s < states.size(); ++s) {
            if (states[s].terminal) { reaches[s] = true; pending.push(s); }
            for (const auto& action : states[s].actions) if (admissible(action, winning))
                for (const auto& [t, p] : action.exits) if (p > 0) reverse[t].push_back(s);
        }
        while (!pending.empty()) {
            const auto t = pending.front(); pending.pop();
            for (auto s : reverse[t]) if (!reaches[s]) { reaches[s] = true; pending.push(s); }
        }
        bool changed = false;
        for (unsigned s = 0; s < states.size(); ++s)
            if (winning[s] && !reaches[s]) { winning[s] = false; changed = true; }
        if (!changed) return winning;
    }
}
std::vector<unsigned> bootstrap_policy(const std::vector<State>& states, const std::vector<bool>& winning) {
    const auto none = std::numeric_limits<unsigned>::max();
    std::vector<unsigned> rank(states.size(), none), policy(states.size(), none);
    for (unsigned s = 0; s < states.size(); ++s) if (states[s].terminal) rank[s] = 0;
    for (unsigned depth = 1; depth <= states.size(); ++depth) {
        bool changed = false;
        for (unsigned s = 0; s < states.size(); ++s) if (winning[s] && rank[s] == none) {
            for (unsigned a = 0; a < states[s].actions.size(); ++a) {
                const auto& action = states[s].actions[a]; if (!admissible(action, winning)) continue;
                if (std::any_of(action.exits.begin(), action.exits.end(),
                        [&](auto exit) { return exit.second > 0 && rank[exit.first] < depth; })) {
                    rank[s] = depth; policy[s] = a; changed = true; break;
                }
            }
        }
        if (!changed) break;
    }
    for (unsigned s = 0; s < states.size(); ++s)
        require(!winning[s] || states[s].terminal || policy[s] != none, "Proper policy construction failed");
    return policy;
}
bool policy_is_proper(const std::vector<State>& states, const std::vector<bool>& winning,
        const std::vector<unsigned>& policy) {
    std::vector<std::vector<unsigned>> reverse(states.size());
    std::queue<unsigned> pending; std::vector<bool> reaches(states.size(), false);
    for (unsigned s = 0; s < states.size(); ++s) if (winning[s]) {
        if (states[s].terminal) { reaches[s] = true; pending.push(s); continue; }
        if (policy[s] >= states[s].actions.size()) return false;
        const auto& action = states[s].actions[policy[s]];
        if (!admissible(action, winning)) return false;
        for (const auto& [t, p] : action.exits) if (p > 0) reverse[t].push_back(s);
    }
    while (!pending.empty()) {
        const auto t = pending.front(); pending.pop();
        for (auto s : reverse[t]) if (!reaches[s]) { reaches[s] = true; pending.push(s); }
    }
    for (unsigned s = 0; s < states.size(); ++s) if (winning[s] && !reaches[s]) return false;
    return true;
}
using Values = std::vector<std::vector<double>>;
Values evaluate_policy(const std::vector<State>& states, const std::vector<bool>& winning,
        const std::vector<unsigned>& policy, unsigned acquisitions, WorkGuard& work) {
    require(policy_is_proper(states, winning, policy), "Nonterminating inventory policy refused");
    std::vector<unsigned> active; std::vector<int> index(states.size(), -1);
    for (unsigned s = 0; s < states.size(); ++s) if (winning[s] && !states[s].terminal) {
        index[s] = int(active.size()); active.push_back(s);
    }
    const unsigned n = static_cast<unsigned>(active.size()), rhs = acquisitions + 3;
    Values matrix(n, std::vector<double>(n + rhs, 0));
    for (unsigned i = 0; i < n; ++i) {
        const auto& action = states[active[i]].actions.at(policy[active[i]]);
        matrix[i][i] = 1; matrix[i][n] = action.cost;
        if (action.kind == RecombDecisionKind::Recombine) matrix[i][n + 1] = 1;
        if (action.kind == RecombDecisionKind::Discard) matrix[i][n + 2] = 1;
        if (action.kind == RecombDecisionKind::Acquire) matrix[i][n + 3 + action.argument] = 1;
        for (const auto& [t, p] : action.exits) if (index[t] >= 0) matrix[i][unsigned(index[t])] -= p;
    }
    // Solve (I-P) for costs and all resource counters using the same factorization.
    for (unsigned column = 0; column < n; ++column) {
        work.tick(n + rhs);
        unsigned pivot = column;
        for (unsigned row = column + 1; row < n; ++row)
            if (std::abs(matrix[row][column]) > std::abs(matrix[pivot][column])) pivot = row;
        require(std::abs(matrix[pivot][column]) > 1e-13, "Inventory policy evaluation is numerically unresolved");
        std::swap(matrix[pivot], matrix[column]);
        const double divisor = matrix[column][column];
        for (unsigned j = column; j < n + rhs; ++j) matrix[column][j] /= divisor;
        for (unsigned row = 0; row < n; ++row) if (row != column) {
            const double factor = matrix[row][column]; if (!factor) continue;
            work.tick(n + rhs - column);
            for (unsigned j = column; j < n + rhs; ++j) matrix[row][j] -= factor * matrix[column][j];
        }
    }
    Values values(states.size(), std::vector<double>(rhs, 0));
    for (unsigned i = 0; i < n; ++i) for (unsigned j = 0; j < rhs; ++j) {
        const double v = matrix[i][n + j];
        require(std::isfinite(v) && v >= -1e-8, "Invalid inventory policy expectation");
        values[active[i]][j] = std::max(0.0, v);
    }
    for (unsigned s : active) {
        const auto& action = states[s].actions[policy[s]];
        for (unsigned j = 0; j < rhs; ++j) {
            double expected = j == 0 ? action.cost :
                (j == 1 ? action.kind == RecombDecisionKind::Recombine :
                 j == 2 ? action.kind == RecombDecisionKind::Discard :
                 action.kind == RecombDecisionKind::Acquire && action.argument == j - 3);
            for (const auto& [t, p] : action.exits) expected += p * values[t][j];
            require(std::abs(expected - values[s][j]) <= 1e-8 * std::max(1.0, values[s][j]),
                    "Inventory fixed-policy residual exceeds numerical acceptance");
        }
    }
    return values;
}
}
RecombSolverResult solve_random_recomb_inventory(const RecombSolverRequest& request) {
    require(request.session != nullptr && !request.price_identity.empty(), "Inventory solver session/prices are missing");
    require(request.recombination_cost_complete && request.recombination_cost_chaos &&
            finite_cost(*request.recombination_cost_chaos), "Recombination gold/dust/attempt cost is incomplete");
    require(!request.acquisitions.empty() && request.acquisitions.size() <= 32, "Inventory solver requires a bounded acquisition catalogue");
    require(request.initial_items.size() <= 2 && request.initial_item_costs.size() == request.initial_items.size(),
            "Inventory solver initial item/cost arity differs");
    require(request.max_items >= 1 && request.max_items <= 128 && request.max_states >= 1 &&
            request.max_states <= 256 && request.max_policy_iterations >= 1 &&
            request.max_policy_iterations <= 64, "Inventory solver resource caps are invalid");
    require(request.max_work >= 1 && request.max_work <= 500000000, "Inventory numerical work cap is invalid");
    WorkGuard work{request}; work.tick();
    validate_random_recomb_goal_projection(request.goal_set_json.data(), request.goal_set_json.size());
    RecombSolverResult result; result.price_identity = request.price_identity; result.cost_complete = true;
    const auto goals = solver::bind_calculator_goal_set(request.session, request.goal_set_json.data(), request.goal_set_json.size());
    std::vector<std::uint64_t> reachable(request.session->words, 0);
    for (unsigned mod = 0; mod < request.session->mod_count; ++mod)
        if (request.session->gen_type[mod] <= 1) pc_bitset_set(reachable.data(), mod);
    solver::ActionRegistry registry;
    solver::CalcContext observer(request.session, goals.front().explicit_goal, std::move(registry), {}, true, false,
        false, std::nullopt, {}, false, reachable, false, false, false, false, false, nullptr, true, false, true);
    std::map<std::string, unsigned> item_ids;
    std::vector<bool> item_goals;
    const auto intern_item = [&](const pc_item_state& item) {
        work.tick();
        validate_craft_resource({"validation", "item", request.session, item});
        const auto key = item_key(item);
        if (const auto found = item_ids.find(key); found != item_ids.end()) return found->second;
        if (result.items.size() >= request.max_items) throw std::length_error("Inventory item discovery cap reached; no outcomes dropped");
        auto projected = item; projected.memory_strands = 0; projected.enchantment_count = 0;
        const auto observed = solver::observe_calculator_terminal_law_json(observer, goals,
            [&](const solver::CalculatorTerminalSink& sink) { sink(projected, 1); });
        const auto parsed = json::Parser(observed.data(), observed.size()).parse();
        const auto p = parsed.at("any_goal_probability").as_number();
        require(p == 0 || p == 1, "Inventory terminal observation is not deterministic");
        const auto id = static_cast<unsigned>(result.items.size());
        item_ids.emplace(key, id); result.items.push_back(item); item_goals.push_back(p == 1);
        return id;
    };
    std::set<std::string> acquisition_ids;
    std::vector<unsigned> acquisition_items;
    for (const auto& offer : request.acquisitions) {
        require(!offer.id.empty() && acquisition_ids.insert(offer.id).second && !offer.quote_identity.empty() &&
            (offer.source_kind == "purchase" || offer.source_kind == "completed_feeder") &&
            offer.cost_complete && finite_cost(offer.total_cost_chaos), "Incomplete/invalid exact-item acquisition quote");
        acquisition_items.push_back(intern_item(offer.item));
    }
    std::vector<unsigned> initial;
    for (unsigned i = 0; i < request.initial_items.size(); ++i) {
        require(finite_cost(request.initial_item_costs[i]), "Initial input cost is incomplete");
        initial.push_back(intern_item(request.initial_items[i])); result.entry_cost_chaos += request.initial_item_costs[i];
    }
    require(std::isfinite(result.entry_cost_chaos), "Initial input cost overflow");
    std::map<std::vector<unsigned>, unsigned> state_ids; std::vector<State> states;
    const auto intern_state = [&](std::vector<unsigned> inventory) {
        work.tick();
        require(inventory.size() <= 2, "Inventory capacity exceeded"); std::sort(inventory.begin(), inventory.end());
        if (const auto found = state_ids.find(inventory); found != state_ids.end()) return found->second;
        if (states.size() >= request.max_states) throw std::length_error("Inventory state discovery cap reached; no outcomes dropped");
        const bool terminal = std::any_of(inventory.begin(), inventory.end(), [&](unsigned id) { return item_goals.at(id); });
        const auto id = static_cast<unsigned>(states.size()); state_ids.emplace(inventory, id);
        states.push_back({std::move(inventory), {}, terminal}); return id;
    };
    result.initial_state = intern_state(std::move(initial));
    std::set<std::string> exclusions;
    for (unsigned s = 0; s < states.size(); ++s) {
        work.tick();
        if (states[s].terminal) continue;
        // Interning successors can reallocate states; hold only value copies.
        const auto inventory = states[s].items; std::vector<Action> actions;
        if (inventory.size() < 2) for (unsigned a = 0; a < request.acquisitions.size(); ++a) {
            auto next = inventory; next.push_back(acquisition_items[a]);
            actions.push_back({RecombDecisionKind::Acquire, a, request.acquisitions[a].total_cost_chaos,
                {{intern_state(std::move(next)), 1}}});
        }
        for (unsigned i = 0; i < inventory.size(); ++i) {
            auto next = inventory; next.erase(next.begin() + i);
            actions.push_back({RecombDecisionKind::Discard, i, 0, {{intern_state(std::move(next)), 1}}});
        }
        if (inventory.size() == 2) {
            std::optional<RandomRecombPair> pair;
            try {
                pair = prepare_random_recomb_pair(
                    {"solver-input-a", "a", request.session, result.items[inventory[0]]},
                    {"solver-input-b", "b", request.session, result.items[inventory[1]]});
            } catch (const std::invalid_argument& error) { exclusions.insert(error.what()); }
            if (pair) {
                std::map<unsigned, double> mass; double total = 0;
                for (const auto& outcome : enumerate_random_recomb_pair(*pair)) {
                    require(std::isfinite(outcome.probability) && outcome.probability > 0, "Invalid inventory transition probability");
                    const auto& from = *pair->carriers[outcome.carrier].output_session;
                    const auto item = root_item(materialize_random_recomb_outcome(*pair, outcome), from, *request.session);
                    const auto t = intern_state({intern_item(item)});
                    mass[t] += outcome.probability; total += outcome.probability;
                }
                require(std::abs(total - 1) <= 1e-10, "Inventory pair transition loses probability mass");
                actions.push_back({RecombDecisionKind::Recombine, 0, *request.recombination_cost_chaos,
                    {mass.begin(), mass.end()}});
            }
        }
        states[s].actions = std::move(actions);
    }
    const auto winning = winning_states(states);
    require(winning.at(result.initial_state), "No proper policy in the declared bounded inventory scope");
    auto policy = bootstrap_policy(states, winning);
    auto values = evaluate_policy(states, winning, policy, static_cast<unsigned>(request.acquisitions.size()), work);
    for (unsigned iteration = 0; iteration < request.max_policy_iterations; ++iteration) {
        result.policy_iterations = iteration + 1; auto next = policy; bool changed = false;
        for (unsigned s = 0; s < states.size(); ++s) if (winning[s] && !states[s].terminal) {
            double best = values[s][0];
            for (unsigned a = 0; a < states[s].actions.size(); ++a) {
                const auto& action = states[s].actions[a]; if (!admissible(action, winning)) continue;
                double cost = action.cost;
                for (const auto& [t, p] : action.exits) cost += p * values[t][0];
                if (cost < best - 1e-10 * std::max(1.0, best)) { best = cost; next[s] = a; changed = true; }
            }
        }
        if (!changed) { result.search_converged = true; break; }
        if (!policy_is_proper(states, winning, next)) { exclusions.insert("Policy improvement proposed a nonterminating policy"); break; }
        policy = std::move(next);
        values = evaluate_policy(states, winning, policy, static_cast<unsigned>(request.acquisitions.size()), work);
    }
    const auto& entry = values[result.initial_state];
    result.expected_cost_chaos = entry[0] + result.entry_cost_chaos;
    result.expected_recombinations = entry[1]; result.expected_discards = entry[2];
    result.expected_acquisitions.assign(entry.begin() + 3, entry.end());
    require(std::isfinite(result.expected_cost_chaos), "Inventory expected cost overflow");
    for (unsigned s = 0; s < states.size(); ++s) {
        result.inventories.push_back(states[s].items); result.terminal.push_back(states[s].terminal);
        if (!winning[s] || states[s].terminal) continue;
        const auto& action = states[s].actions[policy[s]];
        RecombPolicyDecision decision; decision.state = s; decision.kind = action.kind;
        if (action.kind == RecombDecisionKind::Acquire) decision.acquisition = action.argument;
        if (action.kind == RecombDecisionKind::Recombine) {
            decision.input_a = states[s].items[0]; decision.input_b = states[s].items[1];
        }
        if (action.kind == RecombDecisionKind::Discard) decision.input_a = action.argument;
        decision.outcomes = action.exits; result.policy.push_back(std::move(decision));
    }
    result.work_spent = work.spent;
    result.items.shrink_to_fit();
    result.exclusions.assign(exclusions.begin(), exclusions.end()); return result;
}
}
