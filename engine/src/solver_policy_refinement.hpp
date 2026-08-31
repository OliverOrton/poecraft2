#pragma once

#include "solver_refinement.hpp"
#include "solver_solve_contracts.hpp"
#include "solver_cooperative_task.hpp"

#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <set>
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
    std::uint64_t strict_session_constructions = 0;
    std::uint64_t strict_full_restarts = 0;
    std::uint64_t strict_partition_updates = 0;
    std::uint64_t strict_frontier_insertions = 0;
    std::uint64_t strict_frontier_states_inserted = 0;
    std::uint64_t strict_frontier_update_max_ns = 0;
    std::uint64_t strict_cells_retained = 0;
    std::uint64_t strict_cells_created = 0;
    std::uint64_t strict_cells_superseded = 0;
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
    std::uint64_t strict_reforge_continuation_resumes = 0;
    std::uint64_t strict_reforge_continuation_suspensions = 0;
    std::uint64_t strict_reforge_continuation_completions = 0;
    std::uint64_t strict_reforge_continuation_cancellations = 0;
    std::uint64_t strict_reforge_continuation_max_slice_ns = 0;
    std::uint64_t strict_reforge_continuation_max_retained_bytes = 0;
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
    bool external_verified_upper_seeded = false;
    bool interim_compiled_assertion_deferred = false;
    std::uint64_t alternative_obligations_created = 0;
    std::uint64_t unresolved_alternative_obligations = 0;
    std::uint64_t alternative_rows_avoided = 0;
    bool action_accounting_complete = false;
    std::uint64_t alternative_scheduling_rounds = 0;
    std::uint64_t alternative_obligations_scheduled = 0;
    std::uint64_t alternative_obligations_certified = 0;
    std::uint64_t alternative_obligations_partially_evaluated = 0;
    std::uint64_t alternative_frontier_growth_yields = 0;
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
    std::uint64_t quotient_attractor_ns = 0;
    std::uint64_t quotient_lower_relaxation_ns = 0;
    std::uint64_t quotient_policy_seed_ns = 0;
    std::uint64_t quotient_envelope_construction_ns = 0;
    std::uint64_t quotient_exact_policy_evaluation_ns = 0;
    std::uint64_t quotient_policy_improvement_ns = 0;
    std::uint64_t quotient_publication_audit_ns = 0;
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

/* A fail-closed certification graph may be retried as the priced-restart
 * product graph only when its exact loss is exclusively the compiler's
 * off-policy failure terminal. */
bool certification_default_failure_can_use_product_restart(
    const StrategyEvalResult& evaluation,
    bool allow_economic_restart);

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

enum class VerifiedPolicyAlternativeShadowStatus : std::uint8_t {
    Complete = 0,
    IncompleteCertificate,
    IdentityMismatch,
    InvalidEntry,
    ResourceCap,
    AdapterFailure,
};

struct VerifiedPolicyStrictEntry {
    std::string compiled_node_id;
    StableKey exact_entry_identity;
    StableKey exact_item_identity;
    StableKey strict_state_identity;
    /* The exact physical item can project to a different coarse parent than
     * the behavioral representative whose compiled node selected the fixed
     * strategy action.  Keep both identities: the physical parent owns
     * action admission/lower lookup, while the representative remains
     * compiler provenance only. */
    StableKey coarse_state_identity;
    StableKey represented_coarse_state_identity;
    StableKey selected_operator_identity;
    StableKey selected_exact_decision_identity;
    std::uint32_t coarse_state = kNoId;
    std::uint32_t represented_coarse_state = kNoId;
    std::uint32_t selected_operator = kNoId;
    double exact_continuation_upper =
        std::numeric_limits<double>::infinity();
    double policy_bellman_residual =
        std::numeric_limits<double>::infinity();
    double root_expected_visits = 0.0;
    bool global_policy_entry = false;
    bool fixed_observed_choice_policy = false;
};

enum class RetentionCapacityFractureShadowStatus : std::uint8_t {
    NotApplicable = 0,
    Complete,
    InvalidRequest,
    UnsupportedMechanic,
    Inapplicable,
    IncompleteMass,
    IdentityFailure,
};

struct RetentionCapacityFractureTransition {
    StableKey exact_successor_identity;
    std::uint32_t projected_coarse_state = kNoId;
    std::uint32_t satisfied_goal_mask = 0;
    std::uint32_t fractured_goal_mask = 0;
    std::uint32_t fractured_junk_count = 0;
    std::uint32_t fractured_crafted_junk_count = 0;
    std::uint32_t fractured_metamod_flags = 0;
    std::uint8_t prefix_count = 0;
    std::uint8_t suffix_count = 0;
    bool terminal = false;
    double probability = 0.0;

    bool operator==(const RetentionCapacityFractureTransition&) const =
        default;
};

/* One exact, action-local pushed-forward Fracture row. It is proof-only
 * input: the row owns neither a lower nor retirement authority. A consumer
 * may combine its complete probabilities with independently admissible
 * successor lowers. Missing coarse projections deliberately remain kNoId so
 * that consumer can use the existing zero fallback without inventing a
 * representative. */
struct RetentionCapacityFractureShadowRow {
    RetentionCapacityFractureShadowStatus status =
        RetentionCapacityFractureShadowStatus::NotApplicable;
    StableKey exact_entry_identity;
    StableKey action_identity;
    StableKey semantic_identity;
    std::uint32_t source_satisfied_goal_mask = 0;
    std::uint32_t source_blocked_mask = 0;
    std::uint8_t source_prefix_count = 0;
    std::uint8_t source_suffix_count = 0;
    double immediate_cost = 0.0;
    double probability_mass = 0.0;
    double fractured_goal_probability = 0.0;
    double fractured_junk_probability = 0.0;
    std::uint64_t strict_states_created = 0;
    std::uint64_t retained_bytes = 0;
    std::uint64_t transient_bytes = 0;
    std::uint64_t build_ns = 0;
    std::vector<RetentionCapacityFractureTransition> transitions;

    bool available() const {
        return status ==
               RetentionCapacityFractureShadowStatus::Complete;
    }
};

enum class VerifiedPolicyExactRowStatus : std::uint8_t {
    Complete = 0,
    InvalidRequest,
    UnsupportedObservedChoice,
    Inapplicable,
    IncompleteMass,
    IdentityFailure,
    AdapterFailure,
};

struct VerifiedPolicyExactRowTransition {
    StableKey strict_state_identity;
    StableKey exact_item_identity;
    std::uint32_t projected_coarse_state = kNoId;
    bool terminal = false;
    double probability = 0.0;
};

/* Complete strict-kernel projection for one already identity-bound planner
 * decision. It is diagnostic input only and deliberately carries no lower,
 * pruning, retirement, or publication authority. */
struct VerifiedPolicyExactActionRow {
    VerifiedPolicyExactRowStatus status =
        VerifiedPolicyExactRowStatus::InvalidRequest;
    StableKey exact_entry_identity;
    StableKey operator_identity;
    StableKey exact_decision_identity;
    std::string refusal_reason;
    double action_cost = 0.0;
    double probability_mass = 0.0;
    std::uint64_t strict_states_created = 0;
    std::uint64_t retained_bytes = 0;
    std::uint64_t transient_bytes = 0;
    std::uint64_t build_ns = 0;
    std::vector<VerifiedPolicyExactRowTransition> transitions;

    bool available() const {
        return status == VerifiedPolicyExactRowStatus::Complete;
    }
};

enum class VerifiedPolicyBellmanConstraintStatus : std::uint8_t {
    Complete = 0,
    SourceUnavailable,
    ExactRowUnavailable,
    AmbiguousPolicySuccessor,
    InvalidLower,
    IncompleteMass,
    DependencyClosureOpen,
    IdentityFailure,
};

struct VerifiedPolicyBellmanSuccessorEvidence {
    StableKey exact_item_identity;
    StableKey policy_entry_identity;
    double probability = 0.0;
    double policy_continuation =
        std::numeric_limits<double>::infinity();
    double existing_lower = 0.0;
    double applied_potential = 0.0;
    double contribution = 0.0;
    double required_lower_if_sole_closure = 0.0;
    bool terminal = false;
    bool policy_domain = false;
    bool ambiguous_policy_entry = false;
};

struct VerifiedPolicyPotentialLookupResult {
    StableKey policy_entry_identity;
    double continuation =
        std::numeric_limits<double>::infinity();
    bool available = false;
    bool ambiguous = false;
};

struct VerifiedPolicyBellmanTransitionInput {
    StableKey exact_item_identity;
    std::uint32_t projected_coarse_state = kNoId;
    bool terminal = false;
    double probability = 0.0;
};

struct VerifiedPolicyBellmanConstraintRequest {
    StableKey source_entry_identity;
    StableKey source_item_identity;
    StableKey action_identity;
    std::string action_id;
    std::uint32_t coarse_state = kNoId;
    std::uint32_t coarse_operator = kNoId;
    std::uint32_t action_family = 0;
    bool selected_action = false;
    bool selected_policy_equality_available = false;
    bool source_policy_available = false;
    bool exact_row_available = false;
    double source_policy_value =
        std::numeric_limits<double>::infinity();
    double source_policy_residual =
        std::numeric_limits<double>::infinity();
    double action_cost = 0.0;
    double probability_mass = 0.0;
    double root_expected_visits = 0.0;
    std::uint64_t proof_work_proxy = 0;
    std::uint32_t selection_reasons = 0;
    std::uint32_t exact_row_status = 0;
    std::string refusal_reason;
    double epsilon = 1e-12;
    std::vector<VerifiedPolicyBellmanTransitionInput> transitions;
};

using VerifiedPolicyPotentialLookup = std::function<
    VerifiedPolicyPotentialLookupResult(const StableKey&)>;
using VerifiedPolicyExistingLowerLookup =
    std::function<double(std::uint32_t)>;

struct VerifiedPolicyBellmanConstraint;
VerifiedPolicyBellmanConstraint
evaluate_verified_policy_bellman_constraint(
    VerifiedPolicyBellmanConstraintRequest request,
    const VerifiedPolicyPotentialLookup& policy_lookup,
    const VerifiedPolicyExistingLowerLookup& existing_lower_lookup);

struct VerifiedPolicyBellmanConstraint {
    VerifiedPolicyBellmanConstraintStatus status =
        VerifiedPolicyBellmanConstraintStatus::SourceUnavailable;
    StableKey source_entry_identity;
    StableKey source_item_identity;
    StableKey action_identity;
    std::string action_id;
    std::uint32_t coarse_state = kNoId;
    std::uint32_t coarse_operator = kNoId;
    std::uint32_t action_family = 0;
    bool selected_action = false;
    bool selected_policy_equality = false;
    double source_policy_value =
        std::numeric_limits<double>::infinity();
    double source_policy_residual =
        std::numeric_limits<double>::infinity();
    double action_cost = 0.0;
    double shadow_rhs = 0.0;
    double bellman_deficit = 0.0;
    double exact_policy_deviation =
        std::numeric_limits<double>::infinity();
    double boundary_probability_mass = 0.0;
    double root_expected_visits = 0.0;
    std::uint64_t proof_work_proxy = 0;
    std::uint32_t selection_reasons = 0;
    std::uint32_t exact_row_status = 0;
    std::string refusal_reason;
    std::uint32_t internal_policy_successors = 0;
    std::uint32_t boundary_successors = 0;
    bool exact_deviation_available = false;
    bool policy_improving = false;
    bool inequality_satisfied = false;
    std::vector<VerifiedPolicyBellmanSuccessorEvidence> successors;
};

/* Typed no-authority result for the policy-potential experiment. Complete
 * row constraints remain insufficient lower authority until action-complete
 * dependency SCC closure is independently established. */
struct VerifiedPolicyBellmanShadowCertificate {
    static constexpr std::uint64_t kSchemaVersion = 1;

    std::uint64_t schema_version = kSchemaVersion;
    ExecutableContinuationAuthorityContext authority;
    std::uint64_t strategy_identity_digest = 0;
    std::uint64_t strategy_identity_bytes = 0;
    std::uint64_t policy_entry_certificate_identity = 0;
    StableKey existing_lower_identity;
    bool requested = false;
    bool action_complete = false;
    bool dependency_closed = false;
    std::uint64_t exact_internal_constraints = 0;
    std::uint64_t boundary_escape_constraints = 0;
    std::uint64_t policy_improving_deviations = 0;
    std::uint64_t exact_bellman_closed_constraints = 0;
    std::uint64_t unresolved_constraints = 0;
    std::uint64_t exact_row_work = 0;
    std::uint64_t strict_states_created = 0;
    std::uint64_t retained_bytes = 0;
    std::uint64_t transient_bytes = 0;
    std::uint64_t build_ns = 0;
    std::vector<VerifiedPolicyBellmanConstraint> constraints;

    /* Intentionally no conversion or availability predicate. A future
     * authority owner must validate closed SCCs and every legal action. */
};

using RetentionCapacityCoarseProjector =
    std::function<std::uint32_t(std::uint32_t)>;

StableKey retention_capacity_fracture_shadow_row_semantic_identity(
    const RetentionCapacityFractureShadowRow& row);

RetentionCapacityFractureShadowRow
build_retention_capacity_fracture_shadow_row(
    CalcContext& exact,
    std::uint32_t exact_state,
    std::uint32_t primitive_action,
    double immediate_cost,
    StableKey exact_entry_identity,
    StableKey action_identity,
    const RetentionCapacityCoarseProjector& project_to_coarse);

struct VerifiedPolicyAlternativeAction {
    std::uint32_t coarse_operator = kNoId;
    StableKey operator_identity;
    std::string operator_id;
    bool selected = false;
    bool caller_authorized = false;
    bool exact_applicable = false;
    std::optional<RetentionCapacityFractureShadowRow>
        retention_capacity_fracture;
    std::optional<VerifiedPolicyExactActionRow> exact_action_row;
};

using VerifiedPolicyAlternativeObserver = std::function<void(
    const VerifiedPolicyStrictEntry&,
    const VerifiedPolicyAlternativeAction&)>;

/* Optional bounded second-pass selector. A selected row is an exact
 * diagnostic kernel only; selecting it cannot create a proof obligation or
 * grant Bellman/lower authority. */
using VerifiedPolicyExactRowSelector = std::function<bool(
    const VerifiedPolicyStrictEntry&,
    const VerifiedPolicyAlternativeAction&)>;
using VerifiedPolicyEntrySelector = std::function<bool(
    const StrategyPolicyEntryResult&)>;

struct VerifiedPolicyAlternativeShadowCensus {
    static constexpr std::size_t kEntryStatusCount = 12;

    VerifiedPolicyAlternativeShadowStatus status =
        VerifiedPolicyAlternativeShadowStatus::IncompleteCertificate;
    std::string failure_reason;
    std::string resource_cap;
    std::uint64_t certificate_identity = 0;
    std::uint64_t decisions_requested = 0;
    std::uint64_t decisions_reached = 0;
    std::uint64_t decisions_refused = 0;
    std::uint64_t entries_examined = 0;
    std::uint64_t entries_accepted = 0;
    std::uint64_t entries_refused = 0;
    std::array<std::uint64_t, kEntryStatusCount>
        certificate_entry_status_counts{};
    std::uint64_t binding_or_solve_identity_refusals = 0;
    std::uint64_t strict_terminal_refusals = 0;
    std::uint64_t strict_coarse_projection_refusals = 0;
    std::uint64_t selected_action_refusals = 0;
    std::uint64_t vocabulary_actions_examined = 0;
    std::uint64_t caller_authorized_actions = 0;
    std::uint64_t exact_inapplicabilities = 0;
    std::uint64_t selected_actions = 0;
    std::uint64_t alternative_obligations = 0;
    std::uint64_t retention_capacity_rows_examined = 0;
    std::uint64_t retention_capacity_rows_complete = 0;
    std::uint64_t retention_capacity_rows_refused = 0;
    std::uint64_t retention_capacity_transitions = 0;
    std::uint64_t retention_capacity_strict_states_created = 0;
    std::uint64_t retention_capacity_peak_transient_bytes = 0;
    std::uint64_t retention_capacity_build_ns = 0;
    std::uint64_t sampled_exact_rows_examined = 0;
    std::uint64_t sampled_exact_rows_complete = 0;
    std::uint64_t sampled_exact_rows_refused = 0;
    std::uint64_t sampled_exact_transitions = 0;
    std::uint64_t sampled_exact_strict_states_created = 0;
    std::uint64_t sampled_exact_peak_transient_bytes = 0;
    std::uint64_t sampled_exact_build_ns = 0;
    std::uint64_t observer_calls = 0;
    std::uint64_t lifecycle_mutations = 0;
    std::uint64_t retained_owned_bytes = 0;
    std::uint64_t peak_owned_bytes = 0;
    std::uint64_t build_ns = 0;
};

/* Read-only bridge from evaluator-owned exact strategy entries into the
 * existing strict policy oracle. It enumerates the complete caller-filtered
 * operator vocabulary one exact entry at a time and never creates or
 * transitions a quotient proof obligation. The observer must not retain
 * references after it returns. */
solve_detail::CooperativeTask<VerifiedPolicyAlternativeShadowCensus>
audit_verified_policy_alternative_shadow(
    CalcContext& coarse,
    const SolveResult& solved,
    const pc_item_state& exact_start,
    const std::unordered_map<std::string, double>& prices,
    const SolveOptions& options,
    const RetainedCompiledPolicyArtifact& artifact,
    ExecutableContinuationAuthorityContext current_authority,
    VerifiedPolicyAlternativeObserver observer = {},
    VerifiedPolicyExactRowSelector exact_row_selector = {},
    VerifiedPolicyEntrySelector entry_selector = {});

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
        const PolicyCompilationTelemetry* emitted_compilation = nullptr,
        bool request_root_continuation_upper = false,
        bool request_policy_decision_entries = false);
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
    /* A compiled, independently evaluated, proper executable policy may be
     * published before alternative-action refinement finishes.  Keep that
     * certified incumbent observable without promoting the unfinished lift
     * to an exact result. */
    double verified_executable_upper_bound =
        std::numeric_limits<double>::infinity();
    StrategyEvalProgress evaluation;
};

/* An independently parsed/evaluated executable policy in the caller's exact
 * goal, action-vocabulary, and economy scope. This is rollback-upper
 * authority only: it neither identifies the quotient-selected policy nor
 * supplies lower-bound, reconciliation, or exact-publication authority. */
struct PolicyExactLiftRollbackUpper {
    double exact_cost = std::numeric_limits<double>::infinity();
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
        const RefinementLimits* limits_override = nullptr,
        const PolicyExactLiftRollbackUpper* rollback_upper = nullptr);
    ~PolicyExactLiftWork();
    PolicyExactLiftWork(PolicyExactLiftWork&&) noexcept;
    PolicyExactLiftWork& operator=(PolicyExactLiftWork&&) noexcept;
    PolicyExactLiftWork(const PolicyExactLiftWork&) = delete;
    PolicyExactLiftWork& operator=(const PolicyExactLiftWork&) = delete;

    void step(std::uint32_t max_work_items);
    PolicyExactLiftProgress progress() const;
    const PolicyLiftAdapterTelemetry& live_adapter_telemetry();
    PolicyExactLiftCertificate take_result();
    std::uint64_t retained_bytes() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

enum class ExactBoundaryRecoveryStatus : std::uint8_t {
    NotRun = 0,
    Complete,
    InvalidPrefix,
    UnsupportedKernel,
    ResourceCap,
    WallTimeCap,
    ImproperPrefix,
    NoRequestedEntry,
};

const char* exact_boundary_recovery_status_name(
    ExactBoundaryRecoveryStatus status);

struct ExactBoundaryRecoveredMember {
    StableKey stable_key;
    std::uint32_t coarse_state = kNoId;
    pc_item_state item{};
};

/* One exact selected-row edge that terminates at a captured non-goal
 * observation stop. This is provenance only: it cannot supply an action or
 * turn the stop into a successful requested entry. */
struct ExactBoundaryReachedStop {
    StableKey predecessor_stable_key;
    std::uint32_t predecessor_coarse_state = kNoId;
    StableKey predecessor_coarse_state_key;
    std::uint32_t selected_coarse_operator = kNoId;
    std::uint32_t selected_strict_operator = kNoId;
    StableKey selected_action_semantic_key;
    StableKey stopped_stable_key;
    std::uint32_t stopped_coarse_state = kNoId;
    StableKey stopped_coarse_state_key;
    std::uint64_t probability_bits = 0;

    bool operator==(const ExactBoundaryReachedStop&) const = default;
};

struct ExactBoundaryRecoveryResult {
    ExactBoundaryRecoveryStatus status =
        ExactBoundaryRecoveryStatus::NotRun;
    std::string refusal;
    bool complete_support = false;
    bool absorption_proved = false;
    std::uint32_t exact_states = 0;
    std::uint32_t exact_rows = 0;
    std::uint64_t exact_transitions = 0;
    std::uint64_t work_items = 0;
    std::uint64_t retained_owned_bytes = 0;
    std::uint64_t peak_owned_bytes = 0;
    std::uint64_t wall_time_ms = 0;
    std::uint64_t member_identity = 0;
    std::uint64_t reached_stop_identity = 0;
    std::uint64_t reached_stop_count = 0;
    std::uint64_t reached_stop_samples_omitted = 0;
    PolicyLiftAdapterTelemetry adapter;
    std::vector<ExactBoundaryRecoveredMember> requested_entries;
    std::vector<ExactBoundaryReachedStop> reached_stops;
};

/* Internal closure input shared by production recovery and focused native
 * controls. A terminal exact state can be either goal success or a named
 * observation stop; only a non-goal terminal at requested_entry is an entry
 * member. */
struct ExactBoundaryClosureNode {
    ExactState state;
    std::uint32_t locator = kNoId;
    std::vector<std::uint32_t> successors;
};

struct ExactBoundaryClosureResult {
    ExactBoundaryRecoveryStatus status =
        ExactBoundaryRecoveryStatus::NotRun;
    std::string refusal;
    bool complete_support = false;
    bool absorption_proved = false;
    std::vector<std::uint32_t> requested_nodes;
};

ExactBoundaryClosureResult analyze_exact_boundary_closure(
    const std::vector<ExactBoundaryClosureNode>& nodes,
    std::uint32_t requested_entry);

/* Cooperative selected-prefix-only replay. Named coarse stops are absorbing
 * observations, never policy goals. The adapter is forbidden from falling
 * back to an alternative action or invoking local reoptimization. */
class ExactBoundaryRecoveryWork {
  public:
    ExactBoundaryRecoveryWork(
        CalcContext& coarse,
        const SolveResult& selected_prefix,
        const pc_item_state& exact_start,
        const std::unordered_map<std::string, double>& prices,
        const SolveOptions& options,
        std::set<std::uint32_t> observation_stops,
        std::uint32_t requested_entry,
        RefinementLimits limits,
        std::uint64_t max_work,
        std::uint64_t max_wall_time_ms);
    ~ExactBoundaryRecoveryWork();
    ExactBoundaryRecoveryWork(ExactBoundaryRecoveryWork&&) noexcept;
    ExactBoundaryRecoveryWork& operator=(
        ExactBoundaryRecoveryWork&&) noexcept;
    ExactBoundaryRecoveryWork(const ExactBoundaryRecoveryWork&) = delete;
    ExactBoundaryRecoveryWork& operator=(
        const ExactBoundaryRecoveryWork&) = delete;

    void step(std::uint32_t max_work_items);
    bool done() const;
    std::uint64_t retained_bytes() const;
    ExactBoundaryRecoveryResult take_result();

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
