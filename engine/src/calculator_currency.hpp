#pragma once
#include "solver_calc_types.hpp"

namespace poecraft::solver {
// Exact terminal, structural one-action odds. Resources are read only and
// successor IDs are local to this result, not strategy state IDs.
std::string calculate_currency_json(const CalcContext& source,
    const pc_item_state& receiver, const std::string& action,
    const SessionImpl* donor_session = nullptr,
    const pc_item_state* donor = nullptr);
}
