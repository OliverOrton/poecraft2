#pragma once
#include "solver_calc_types.hpp"

namespace poecraft::solver {
// Terminal Calculator requirements; never admitted as a strategy-search goal.
struct CalculatorItemGoal {
    std::vector<std::uint32_t> implicit_mods;
    std::optional<std::uint8_t> influence_bits;
    std::optional<bool> corrupted;
};
CalculatorItemGoal parse_calculator_item_goal(const SessionImpl&, const char*, std::size_t);
// Exact terminal, structural one-action odds. Resources are read only and
// successor IDs are local to this result, not strategy state IDs.
std::string calculate_currency_json(CalcContext& source,
    const pc_item_state& receiver, const std::string& action,
    const SessionImpl* donor_session = nullptr,
    const pc_item_state* donor = nullptr,
    const CalculatorItemGoal& item_goal = {});
}
