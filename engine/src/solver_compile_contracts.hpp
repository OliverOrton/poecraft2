#pragma once

#include "solver_solve_contracts.hpp"

namespace poecraft {
namespace solver {

// --- policy -> strategy graph compiler (S5) --------------------------------------

/*
 * Compile a solved policy into ordinary strategy JSON (the same format the
 * editor and simulator consume): a master router whose prioritized edges
 * test policy-reachable state membership with existing condition types,
 * one primitive operation or fixed-option primitive chain per state with the
 * first node annotated by its expected remaining cost,
 * a success terminal for the goal, and a failure terminal for off-policy
 * leaks so abstraction drift fails loudly in the verification gate.
 *
 * Throws std::runtime_error on condition-vocabulary gaps the current
 * condition set cannot express: tag-discriminating layouts, states with
 * metamod/influence flags, group slots with tier thresholds, blocked
 * flags alongside present goal mods, or two reachable states sharing one
 * expressible signature.
 */
std::string compile_policy_strategy_json(
    CalcContext& calc,
    const SolveResult& result,
    const std::string& name,
    PolicyCompilationTelemetry* telemetry = nullptr,
    std::uint64_t max_strategy_json_bytes =
        std::numeric_limits<std::uint64_t>::max(),
    const refinement::RefinedPolicyCompileRouting* refined_routing =
        nullptr,
    std::uint64_t max_compiler_owned_bytes =
        std::numeric_limits<std::uint64_t>::max());


} // namespace solver
} // namespace poecraft
