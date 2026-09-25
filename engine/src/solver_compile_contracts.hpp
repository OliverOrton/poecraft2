#pragma once

#include "solver_solve_contracts.hpp"

namespace poecraft {
namespace solver {

// --- policy -> strategy graph compiler (S5) --------------------------------------

enum class PolicyRouteDefaultMode : std::uint8_t {
    ProductSafeRestart = 0,
    CertificationFailClosed,
};

/* Finder-only proposal emission. The sequence is a finite native primitive
 * controller stage: after each paid operation the exact request goal is tested;
 * the final miss either repeats the final stage or returns to the first.
 * This emits no solve or
 * proof fields. Acceptance must still bind the original item and scope and
 * independently evaluate the complete graph. */
std::string compile_finder_candidate_json(
    const CalcContext& calc,
    const pc_item_state& start_item,
    const std::vector<std::uint32_t>& primitive_sequence,
    const SolveOptions& limits,
    bool return_to_first = false);

std::string compile_finder_goal_condition(const CalcContext& calc);

/* A finite, native-bound finder proposal. Indices name nodes in this vector;
 * Hole is search-only and cannot be compiled. Goal ingress is assembled from
 * the original request, never supplied as an arbitrary proposer predicate. */
enum class FinderControlKind : std::uint8_t {
    TestGoal, TestSlot, TestAffixCountAtLeast4,
    RunPrimitive, RunScourAlchemy, GoalTerminal, FailureTerminal, Hole,
    TestEldritchTiers, TestSideCountAtLeast, RunNativeProgram
};
struct FinderProgramBinding {
    // Both handles are local to the original CalcContext. The finder creates
    // them through complete state-local automatic admission, never JSON.
    std::uint32_t operator_index = kNoId;
    std::uint32_t admitted_state = kNoId;
    std::uint32_t held_goal_mask = 0;
};
struct FinderControlNode {
    FinderControlKind kind = FinderControlKind::Hole;
    std::uint32_t binding = kNoId;
    std::uint32_t on_true = kNoId;
    std::uint32_t on_false = kNoId;
    std::uint32_t next = kNoId;
};
struct FinderControlGraph {
    std::vector<FinderControlNode> nodes;
    std::uint32_t entry = kNoId;
    std::vector<FinderProgramBinding> programs;
};
std::string compile_finder_control_json(
    const CalcContext& calc,
    const pc_item_state& start_item,
    const FinderControlGraph& control,
    const SolveOptions& limits);

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
    PolicyCompilationTelemetry* telemetry = nullptr,
    bool closed_local_domain = false,
    const GraphLocalPolicyProvenance* old_provenance = nullptr,
    std::uint32_t single_pass_option = kNoId);


} // namespace solver
} // namespace poecraft
