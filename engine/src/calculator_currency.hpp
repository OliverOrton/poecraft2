#pragma once
#include "solver_calc_types.hpp"

namespace poecraft::json { struct Value; }
namespace poecraft::solver {
// Terminal Calculator requirements; never admitted as a strategy-search goal.
struct CalculatorItemGoal {
    std::vector<std::uint32_t> implicit_mods;
    std::optional<std::uint8_t> influence_bits;
    std::optional<bool> corrupted;
};
// Stable IDs label terminal membership only; no continuation/proof authority.
inline constexpr std::size_t kMaxCalculatorGoals = 8;
struct CalculatorGoal {
    std::string id;
    GoalSpec explicit_goal;
    CalculatorItemGoal item_goal;
};
// Pair-law owners supply one weighted terminal stream in the explicitly bound
// output session/model. This observer never invents or chooses a pair law.
using CalculatorTerminalSink = std::function<void(const pc_item_state&, long double)>;
using CalculatorTerminalLaw = std::function<void(const CalculatorTerminalSink&)>;
std::string observe_calculator_terminal_law_json(CalcContext& output,
    const std::vector<CalculatorGoal>& goals, const CalculatorTerminalLaw& law);
// Reuses the public bounded goal-set parser; IDs/tier ranks remain bound to
// this reference session until a pair owner explicitly maps each carrier.
std::vector<CalculatorGoal> bind_calculator_goal_set(
    std::shared_ptr<const SessionImpl>, const char*, std::size_t);
CalculatorItemGoal parse_calculator_item_goal(const SessionImpl&, const json::Value&);
CalculatorItemGoal parse_calculator_item_goal(const SessionImpl&, const char*, std::size_t);
// Exact terminal, structural one-action odds. Resources are read only and
// successor IDs are local to this result, not strategy state IDs.
std::string calculate_currency_json(CalcContext& source,
    const pc_item_state& receiver, const std::string& action,
    const SessionImpl* donor_session = nullptr,
    const pc_item_state* donor = nullptr,
    const CalculatorItemGoal& item_goal = {},
    const std::vector<CalculatorGoal>& goals = {},
    const CalculatorTerminalLaw* terminal_law = nullptr);
}
