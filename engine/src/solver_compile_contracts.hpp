#pragma once

#include "solver_solve_contracts.hpp"

namespace poecraft {
namespace solver {

// --- policy -> strategy graph compiler (S5) --------------------------------------

enum class PolicyRouteDefaultMode : std::uint8_t {
    ProductSafeRestart = 0,
    CertificationFailClosed,
};

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
        std::numeric_limits<std::uint64_t>::max(),
    PolicyRouteDefaultMode route_default_mode =
        PolicyRouteDefaultMode::ProductSafeRestart,
    bool compile_closed_coarse_certification_domain = false);

enum class FirstReturnCompilationMode : std::uint8_t {
    OneShot,
    PrivateExcursion,
};

/* Candidate construction only. The ordinary repeated controller has already
 * been compiled from complete native decisions. Preserve its exact initial
 * operation, intercept subsequent empty-Rare decision returns, and either
 * enter a separately namespaced immutable old controller or a private STOP
 * boundary. The latter is never a publishable crafting controller. */
std::string compile_first_return_strategy_json(
    const std::string& repeated_strategy_json,
    const std::string& old_strategy_json,
    const std::string& initial_operation_node,
    const pc_item_state& exact_anchor,
    FirstReturnCompilationMode mode,
    const SolveOptions& limits);

/* Compose a complete native local decision domain with an immutable current
 * controller. Entry/return predicates belong to the private calculator's
 * namespace and are tested only at global decision routers, after mandatory
 * programs finish. The result remains an unevaluated upper proposal. */
std::string compile_dirty_continuation_strategy_json(
    CalcContext& private_calc,
    const std::string& local_strategy_json,
    const std::string& old_strategy_json,
    const std::vector<std::uint32_t>& local_states,
    const std::vector<std::uint32_t>& return_states,
    const SolveOptions& limits,
    PolicyCompilationTelemetry* telemetry = nullptr);


} // namespace solver
} // namespace poecraft
