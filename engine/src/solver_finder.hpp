#pragma once

#include "solver_compile_contracts.hpp"
#include "solver_eval_types.hpp"

#include <memory>
#include <string>

namespace poecraft::solver {

struct FinderCandidatePreparation {
    std::shared_ptr<StrategyImpl> strategy;
    std::string refusal;

    bool ready() const { return strategy != nullptr && refusal.empty(); }
};

/* This accepts an untrusted complete ordinary graph only when its original
 * item, every success ingress and every operation are bound to the caller's
 * native request. It does not evaluate or certify the graph. */
FinderCandidatePreparation prepare_finder_candidate(
    const CalcContext& problem,
    std::shared_ptr<const SessionImpl> session,
    const pc_item_state& original_start,
    const std::string& strategy_json);

bool finder_evaluation_accepted(const StrategyEvalResult& result);

} // namespace poecraft::solver
