#pragma once

#include "solver_refinement.hpp"
#include "solver_solve_contracts.hpp"

#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <unordered_map>

namespace poecraft {
namespace solver {
namespace refinement {

/*
 * Production adapter result. The shared refinement engine remains the sole
 * owner of partitioning and lumpability; this type only records the bounded
 * strict-carrier discovery needed to feed that engine.
 */
struct PolicyLiftAdapterTelemetry {
    std::uint32_t coarse_policy_states = 0;
    std::uint64_t coarse_policy_edges = 0;
    std::uint32_t strict_states_discovered = 0;
    std::uint32_t strict_carriers_materialized = 0;
    std::uint32_t strict_kernels_built = 0;
    std::uint64_t strict_transitions_built = 0;
    std::uint64_t canonical_successor_collapses = 0;
    std::uint64_t strict_kernel_cache_hits = 0;
    std::uint32_t backward_observation_rounds = 0;
    std::uint64_t exact_fixed_point_rounds = 0;
    std::uint64_t strict_calc_owned_bytes = 0;
    std::uint64_t adapter_owned_bytes = 0;
    std::uint64_t peak_adapter_owned_bytes = 0;
    std::uint64_t strict_reforge_work = 0;
    std::uint64_t strict_reforge_logical_work_v1 = 0;
    std::uint64_t strict_reforge_evaluator_work_v1 = 0;
    std::uint64_t strict_reforge_evaluator_work_v2 = 0;
    std::uint64_t strict_reforge_evaluator_work_v3 = 0;
    ReforgeEffortBreakdown strict_reforge_effort;
    std::vector<ReforgeRowTelemetry> strict_reforge_row_samples;
    std::uint64_t strict_reforge_row_samples_omitted = 0;
    /* Aggregate, deterministic attribution for the production streamed
     * quotient. "Selected" is the inherited/current exact policy decision;
     * "alternative" is any other admitted semantic decision considered for
     * that carrier. Begun rows include the resource-interrupted row, while
     * completed rows and transitions include only fully materialized rows. */
    std::uint64_t selected_rows_begun = 0;
    std::uint64_t selected_rows_completed = 0;
    std::uint64_t selected_reforge_work = 0;
    std::uint64_t selected_transitions = 0;
    std::uint64_t alternative_rows_begun = 0;
    std::uint64_t alternative_rows_completed = 0;
    std::uint64_t alternative_reforge_work = 0;
    std::uint64_t alternative_transitions = 0;
    std::vector<PolicyBroadRowAttribution> broad_row_attribution;
    std::uint64_t broad_row_attribution_omitted = 0;
    std::optional<std::uint64_t> work_to_first_partition;
    std::optional<std::uint64_t> work_to_first_executable_upper;
    std::optional<std::uint64_t> wall_ns_to_first_partition;
    std::optional<std::uint64_t> wall_ns_to_first_executable_upper;
    std::uint64_t alternatives_materialized_before_first_upper = 0;
    std::uint64_t alternative_obligations_created = 0;
    std::uint64_t unresolved_alternative_obligations = 0;
    std::uint64_t alternative_rows_avoided = 0;
    bool action_accounting_complete = false;
    std::uint64_t alternative_scheduling_rounds = 0;
    std::uint64_t alternative_obligations_scheduled = 0;
    std::uint64_t alternative_obligations_certified = 0;
    std::uint64_t alternative_obligations_partially_evaluated = 0;
    std::uint64_t alternative_obligations_noncompetitive = 0;
    std::uint64_t alternative_obligations_stale = 0;
    std::uint64_t alternative_verdict_revocations = 0;
    std::uint64_t alternative_obligations_resource_interrupted = 0;
    std::uint64_t competitive_alternatives_remaining = 0;
    std::uint64_t alternative_policy_improvements = 0;
    bool bounded_publication_retained = false;
    bool global_lower_bound_closed = false;
    bool exact_alternative_envelope_closed = false;
    std::uint32_t local_reoptimization_rounds = 0;
    std::uint64_t local_state_action_rows_scheduled = 0;
    std::uint64_t local_state_action_rows_evaluated = 0;
    std::uint64_t local_reoptimizations = 0;
    std::uint64_t local_policy_changes = 0;
    std::uint64_t local_value_changes = 0;
    std::uint64_t proof_payload_reuses = 0;
    std::uint64_t row_reprojections = 0;
    std::uint64_t quotient_source_splits = 0;
    std::uint64_t quotient_target_splits = 0;
    std::uint64_t reverse_invalidations = 0;
    std::uint64_t improper_policy_repairs = 0;
    std::uint64_t exact_carriers_replayed = 0;
    std::uint64_t current_live_slices = 0;
    std::uint64_t peak_live_slices = 0;
    std::uint64_t current_live_slice_bytes = 0;
    std::uint64_t peak_live_slice_bytes = 0;
    std::uint64_t coverage_descriptor_bytes = 0;
    std::uint64_t certificate_bytes = 0;
    std::uint64_t dependency_sidecar_bytes = 0;
    std::uint64_t alternative_obligation_bytes = 0;
    std::uint64_t partition_bytes = 0;
    std::uint64_t carrier_bytes = 0;
    std::uint64_t row_kernel_bytes = 0;
    std::uint64_t scratch_bytes = 0;
    std::uint64_t total_solver_owned_bytes = 0;
    std::uint64_t reference_adapter_invocations = 0;
    std::uint64_t total_ns = 0;
    std::uint64_t carrier_discovery_ns = 0;
    std::uint64_t partition_refinement_ns = 0;
    std::uint64_t policy_evaluation_ns = 0;
    std::uint64_t local_reoptimization_ns = 0;
    std::uint64_t compilation_ns = 0;
    std::uint64_t exact_evaluation_ns = 0;
};

/*
 * Structured authority for compiling an exactly refined policy. The strict
 * state ids are carrier handles only; policy routing is defined exclusively
 * by the canonical observation program and its resulting signature.
 */
struct RefinedPolicyCompileClass {
    std::uint32_t class_id = 0;
    std::uint32_t coarse_state = kNoId;
    StableKey coarse_state_key;
    std::uint32_t representative_state = kNoId;
    std::vector<std::uint32_t> strict_members;
    bool terminal = false;
    ObservationRequirement required_observations;
    FeatureSignature observation_signature;
    std::optional<SelectedAction> selected_action;
    double action_cost = 0.0;
    SharedProjectedTransitions transitions;
};

struct RefinedPolicyCompileParent {
    std::uint32_t coarse_state = kNoId;
    StableKey coarse_state_key;
    AbstractState state;
};

struct RefinedPolicyCompileRouting {
    std::vector<RefinedPolicyCompileClass> classes;
    std::vector<RefinedPolicyCompileParent> parents;
    /* Call-scoped authority owned by the outer coarse CalcContext. */
    const AbstractLayout* parent_layout = nullptr;
    /*
     * The lifted CalcContext is physically strict, but selected Fracture
     * rows may still execute the outer product solver's proven
     * goal-hit/Restart quotient. Preserve that parent semantic explicitly so
     * compilation emits the same conditional restart route certified by the
     * refinement kernel.
     */
    bool product_solver_parent = false;
};

/* Streamed quotient coverage names the strict locators visited by the proof,
 * while materializing one locator can discover additional semantically
 * equivalent strict carriers. Compilation must represent the canonical union
 * rather than letting either source replace the other. */
void merge_refined_compile_strict_members(
    std::vector<std::uint32_t>& represented_members,
    const std::vector<std::uint32_t>& streamed_members);

enum class CompiledPolicyAssertionStatus : std::uint8_t {
    NotRun = 0,
    Complete,
    NoPolicy,
    ResourceCap,
    CompilationFailure,
    ExactEvaluationFailure,
    ImproperPolicy,
    IncompleteCost,
    CostMismatch,
};

struct CompiledPolicyAssertion {
    CompiledPolicyAssertionStatus status =
        CompiledPolicyAssertionStatus::NotRun;
    bool executable = false;
    bool proper = false;
    bool zero_off_policy = false;
    bool cost_reconciled = false;
    std::string failure_reason;
    std::string failure_classification;
    std::string resource_cap;
    std::string strategy_json;
    PolicyCompilationTelemetry compilation;
    std::string certification_strategy_json;
    PolicyCompilationTelemetry certification_compilation;
    bool paired_default_only = false;
    StrategyEvalResult evaluation;
    double solver_cost = std::numeric_limits<double>::infinity();
    double exact_cost = std::numeric_limits<double>::infinity();
    double absolute_cost_delta = std::numeric_limits<double>::infinity();
    double relative_cost_delta = std::numeric_limits<double>::infinity();
    double off_policy_probability = std::numeric_limits<double>::infinity();
    std::uint64_t retained_solver_bytes = 0;
    std::uint64_t parsed_strategy_bytes = 0;
    std::uint64_t economy_bytes = 0;
    std::uint64_t evaluator_memory_budget = 0;
    std::uint64_t publication_peak_owned_bytes = 0;
    std::uint64_t compilation_ns = 0;
    std::uint64_t exact_evaluation_ns = 0;
};

/* Pure final classification shared by the production assertion and focused
 * contract tests. The supplied evaluation must already be the parsed
 * strategy's exact result. */
void finalize_compiled_policy_assertion(
    CompiledPolicyAssertion& assertion);

/* Reuse an independently completed exact graph evaluation only when the
 * newly compiled fail-closed graph is byte-identical. Reconciliation remains
 * owned by the current class-policy value. The cache is consumed on a hit. */
bool reuse_compiled_policy_assertion_evaluation(
    CompiledPolicyAssertion& current,
    std::optional<CompiledPolicyAssertion>& cached);

std::uint64_t compiled_policy_assertion_retained_bytes(
    const CompiledPolicyAssertion& assertion);

std::string compiled_policy_failure_classification(
    const CompiledPolicyAssertion& assertion);

/*
 * Compile the supplied policy/layout, parse the emitted strategy through
 * simulator authority, and exact-evaluate it under the remaining solve
 * resource budget. The production lift calls this with its strict context and
 * a policy populated for every reached strict carrier. Every call parses and
 * independently evaluates the exact emitted JSON graph; a strict class-policy
 * witness cannot substitute for this final assertion. A caller may supply an
 * already emitted document and its compiler telemetry for final verification.
 * Reconciliation uses the portfolio contract: absolute error <= 1e-7 OR
 * relative error <= 1e-9.
 */
CompiledPolicyAssertion assert_compiled_policy_exact(
    CalcContext& coarse,
    const SolveResult& solved,
    const std::unordered_map<std::string, double>& prices,
    const SolveOptions& options,
    const std::string& strategy_name,
    const RefinedPolicyCompileRouting* refined_routing = nullptr,
    const std::string* emitted_strategy_json = nullptr,
    const PolicyCompilationTelemetry* emitted_compilation = nullptr);

enum class CompiledPolicyAssertionPhase : std::uint8_t {
    Compiling = 0,
    Certifying,
    Done,
};

struct CompiledPolicyAssertionProgress {
    CompiledPolicyAssertionPhase phase =
        CompiledPolicyAssertionPhase::Compiling;
    bool done = false;
    StrategyEvalProgress evaluation;
};

/* Retained counterpart of assert_compiled_policy_exact(). Compilation and
 * parsing are bounded stages; exact graph evaluation advances through the
 * evaluator's existing one-work-item continuation. Borrowed inputs must
 * outlive the work object. */
class CompiledPolicyAssertionWork {
  public:
    CompiledPolicyAssertionWork(
        CalcContext& coarse,
        const SolveResult& solved,
        const std::unordered_map<std::string, double>& prices,
        const SolveOptions& options,
        std::string strategy_name,
        const RefinedPolicyCompileRouting* refined_routing = nullptr,
        const std::string* emitted_strategy_json = nullptr,
        const PolicyCompilationTelemetry* emitted_compilation = nullptr);
    ~CompiledPolicyAssertionWork();
    CompiledPolicyAssertionWork(CompiledPolicyAssertionWork&&) noexcept;
    CompiledPolicyAssertionWork& operator=(
        CompiledPolicyAssertionWork&&) noexcept;
    CompiledPolicyAssertionWork(const CompiledPolicyAssertionWork&) = delete;
    CompiledPolicyAssertionWork& operator=(
        const CompiledPolicyAssertionWork&) = delete;

    void step(std::uint32_t max_work_items);
    CompiledPolicyAssertionProgress progress() const;
    CompiledPolicyAssertion take_result();
    std::uint64_t retained_bytes() const;
    bool try_reuse_completed_evaluation(
        std::optional<CompiledPolicyAssertion>& cached);

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

enum class PolicyExactLiftStatus : std::uint8_t {
    Complete = 0,
    NoPolicy,
    InvalidSolveState,
    MissingPrice,
    UnsupportedPrimitiveKernel,
    CoarseMappingFailure,
    ObservationUnavailable,
    ResourceCap,
    RefinementFailure,
    LocalReoptimizationRequired,
    CompiledAssertionFailure,
};

const char* policy_exact_lift_status_name(PolicyExactLiftStatus status);

struct PolicyExactLiftCertificate {
    PolicyExactLiftStatus status = PolicyExactLiftStatus::InvalidSolveState;
    bool executable = false;
    bool lumpable = false;
    bool policy_changed = false;
    bool coarse_value_reconciled = false;
    bool global_lower_bound_closed = false;
    std::string failure_reason;
    std::string resource_cap;
    double exact_start_cost = std::numeric_limits<double>::infinity();
    double solver_cost = std::numeric_limits<double>::infinity();
    double absolute_cost_delta = std::numeric_limits<double>::infinity();
    double relative_cost_delta = std::numeric_limits<double>::infinity();
    std::uint32_t root_refinement_class = kNoId;
    StableKey exact_root_key;
    PolicyLiftAdapterTelemetry adapter;
    RefinementResult refinement;
    PolicyEvaluationResult class_evaluation;
    CompiledPolicyAssertion compiled;
};

enum class PolicyExactLiftPhase : std::uint8_t {
    CarrierDiscovery = 0,
    PartitionRefinement,
    PolicyEvaluation,
    LocalReoptimization,
    Compiling,
    Certifying,
    Done,
};

struct PolicyExactLiftProgress {
    PolicyExactLiftPhase phase =
        PolicyExactLiftPhase::CarrierDiscovery;
    bool done = false;
    std::uint64_t work_items = 0;
    std::uint32_t strict_states = 0;
    std::uint32_t strict_kernels = 0;
    std::uint64_t strict_transitions = 0;
    std::uint32_t partition_rounds = 0;
    std::uint32_t partition_classes = 0;
    StrategyEvalProgress evaluation;
};

class PolicyExactLiftWork {
  public:
    PolicyExactLiftWork(
        CalcContext& coarse,
        const SolveResult& solved,
        const pc_item_state& exact_start,
        const std::unordered_map<std::string, double>& prices,
        const SolveOptions& options,
        std::string strategy_name,
        const RefinementLimits* limits_override = nullptr);
    ~PolicyExactLiftWork();
    PolicyExactLiftWork(PolicyExactLiftWork&&) noexcept;
    PolicyExactLiftWork& operator=(PolicyExactLiftWork&&) noexcept;
    PolicyExactLiftWork(const PolicyExactLiftWork&) = delete;
    PolicyExactLiftWork& operator=(const PolicyExactLiftWork&) = delete;

    void step(std::uint32_t max_work_items);
    PolicyExactLiftProgress progress() const;
    PolicyExactLiftCertificate take_result();
    std::uint64_t retained_bytes() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/*
 * Lift only the policy-reachable strict closure of `exact_start`. Primitive
 * and fixed PlannerOperators are imported by context-independent semantic
 * identity. Fixed operators derive one conservative contract from their
 * declared primitive dependencies and execute through exact OptionKernel
 * authority; the shared refinement engine remains independent of option
 * names and kinds.
 *
 * The shared class-policy evaluator proves properness and supplies
 * exact_start_cost before compilation. The parsed compiled-strategy evaluator
 * is then the independent final executable-artifact assertion.
 */
PolicyExactLiftCertificate lift_policy_exact(
    CalcContext& coarse,
    const SolveResult& solved,
    const pc_item_state& exact_start,
    const std::unordered_map<std::string, double>& prices,
    const SolveOptions& options,
    const std::string& strategy_name,
    const RefinementLimits* limits_override = nullptr);

/* Production proof-carrying quotient publication. Exact carriers are
 * discovered through native kernel authority, partitioned through the shared
 * closed probabilistic engine, projected into stable certified sparse rows,
 * and evaluated by the common proper-policy machinery. The reconstruct-then-
 * merge adapter above remains a bounded reference oracle only. */
PolicyExactLiftCertificate lift_policy_quotient(
    CalcContext& coarse,
    const SolveResult& solved,
    const pc_item_state& exact_start,
    const std::unordered_map<std::string, double>& prices,
    const SolveOptions& options,
    const std::string& strategy_name,
    const RefinementLimits* limits_override = nullptr);

} // namespace refinement
} // namespace solver
} // namespace poecraft
