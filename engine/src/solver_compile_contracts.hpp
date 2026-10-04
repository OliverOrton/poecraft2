#pragma once

#include "solver_solve_contracts.hpp"

namespace poecraft {
namespace solver {

// --- policy -> strategy graph compiler (S5) --------------------------------------

enum class PolicyRouteDefaultMode : std::uint8_t {
    ProductSafeRestart = 0,
    CertificationFailClosed,
};

/* Native primitive-sequence proposal emission for Finder and Current. The sequence is a finite native primitive
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

/* Supplementary finite original-root grammar. The supplied private context
 * includes all three native actions; selected misses use its exact observable
 * state predicates. All add failures unconditionally pay reset, so add never
 * repeats within one renewal. The caller must prove every reset obligation. */
std::string compile_paid_root_foulborn_candidate_json(
    const CalcContext& private_calc, const pc_item_state& start_item,
    std::uint32_t roll, std::uint32_t add, std::uint32_t reset,
    const std::vector<std::uint32_t>& selected_misses, const SolveOptions& limits);

std::string compile_finder_goal_condition(const CalcContext& calc);

/* Conservative compiler-owned entailment for solver-produced graphs. Every
 * effective edge into success must test the exact original native request;
 * graph conditions used only for routing remain evaluator observations. */
bool compiled_success_ingress_matches_request(
    const CalcContext& calc, const std::string& strategy_json,
    std::string* refusal = nullptr, bool allow_paid_root_foulborn = false);

/* Parsed operations must be caller primitives or individual dependency steps
 * bound by an existing trusted native programme owner. Registry membership or
 * graph-authored metadata alone grants no permission. Empty bindings are the
 * conservative supplied-graph boundary used by root-only assertions. */
bool compiled_operations_match_request(
    const CalcContext& calc, const StrategyImpl& strategy,
    const std::unordered_map<std::string, std::uint32_t>& trusted_steps,
    std::string* refusal = nullptr);

/* A finite, native-bound finder proposal. Indices name nodes in this vector;
 * Hole is search-only and cannot be compiled. Goal ingress is assembled from
 * the original request, never supplied as an arbitrary proposer predicate. */
enum class FinderControlKind : std::uint8_t {
    TestGoal, TestSlot, TestAffixCountAtLeast4,
    RunPrimitive, RunScourAlchemy, GoalTerminal, FailureTerminal, Hole,
    TestEldritchTiers, TestSideCountAtLeast, RunNativeProgram,
    // Compiler projection into existing predicates, not native admission authority.
    // Binding selects the target side; false retains the paid recovery branch.
    TestMissingGoalRollable
};
enum class FinderProgramIntent : std::uint8_t {
    ExactOperator,
    NativeMissingEldritchGoal,
    NativeTemporaryGoalAttempt,
};
struct FinderProgramBinding {
    // Both handles are local to the original CalcContext. The finder creates
    // them through complete state-local native admission of the requested
    // intent (possibly a constructive query), never JSON. Query completion
    // grants no full automatic action-envelope closure.
    std::uint32_t operator_index = kNoId;
    std::uint32_t admitted_state = kNoId;
    std::uint32_t held_goal_mask = 0;
    // Compiler-owned upper proposal rule. Dynamic intent is rederived through
    // complete requested native admission at each positive exact entry;
    // action/resource identity remains fixed and every selected kernel keeps
    // its full outcome obligations. No supplied graph can grant authority.
    FinderProgramIntent intent = FinderProgramIntent::ExactOperator;
};
struct FinderControlNode {
    // TestAffixCountAtLeast4 retains its legacy kind identity: an unset
    // binding means four; an explicit binding supplies a threshold in [1,7].
    // Seven is a never guard for ordinary equipment's six explicit affixes.
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
std::vector<std::uint64_t> finder_program_occurrence_key(
    const CalcContext& calc, const FinderProgramBinding& binding);
bool finder_program_is_single_temporary_attempt(
    const CalcContext& calc, std::uint32_t state, std::uint32_t option);
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
