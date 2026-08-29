#pragma once

#include "solver_eval_types.hpp"

namespace poecraft {
namespace solver {

// --- DP solver core (S4) --------------------------------------------------------

enum class SolveProfile : std::uint32_t {
    Default = 0,
    CalculatorProductV1 = 1,
};

inline const char* solve_profile_name(const SolveProfile profile) {
    switch (profile) {
    case SolveProfile::Default: return "default";
    case SolveProfile::CalculatorProductV1:
        return "calculator_product_v1";
    }
    return "unknown";
}

struct SolveOptions {
    double epsilon = 1e-9;          /* max Bellman residual, cost units */
    std::uint32_t max_states = 200000;
    std::uint32_t max_sweeps = 100000;
    std::uint32_t max_discovered_states = 200000;
    std::uint32_t max_expanded_states = 200000;
    std::uint64_t max_state_action_rows = 1215000;
    std::uint64_t max_transitions = 10000000;
    std::uint64_t max_reforge_work = 50000000;
    std::uint64_t max_solver_owned_bytes = 1073741824;
    std::uint32_t max_compiled_nodes = 100000;
    std::uint32_t max_compiled_edges = 400000;
    std::uint64_t max_strategy_json_bytes = 67108864;
    std::uint32_t max_diagnostic_samples = 32;
    std::uint64_t max_telemetry_json_bytes = 1048576;
    /* Zero inherits max_discovered_states. A nonzero value bounds each
     * optional post-solve policy certification/lift state space; exhausting
     * it must retain an already independently verified executable policy. */
    std::uint32_t max_policy_refinement_states = 0;
    /* Exact work-scheduling controls. These do not change the admitted
     * action set, state identity, transition kernels, or production caps. */
    std::uint32_t focused_expansion_checkpoint = 32;
    std::uint32_t focused_expansion_queue_threshold = 1024;
    std::uint32_t focused_members_per_fringe_class = 64;
    std::uint32_t focused_expansion_batch_states = 256;
    std::uint32_t focused_lower_batch_states = 64;
    double focused_goal_progress_priority_multiplier = 256.0;
    /* Automatic Imprint discovery uses a certified incumbent/positive-price
     * proof depth when available. max_imprint_program_depth is deliberately a
     * fallback refusal boundary only for grammars without that proof (no
     * finite upper or a nonpositive reachable step); it does not truncate a
     * proved finite grammar. max_imprint_program_work remains an unconditional
     * cooperative work refusal, including during price-proved search. */
    std::uint32_t max_imprint_program_depth = kDefaultImprintProgramDepth;
    std::uint64_t max_imprint_program_work = kDefaultImprintProgramWork;
    /* White-box oracle comparison switch. Product/API solves leave exact
     * preservation control enabled. */
    bool preservation_control = true;
    /* White-box oracle comparison switch for the price-bound constructive
     * state certificate. Certified partial transition graphs are never
     * retained as price-independent re-solve caches. */
    bool state_certificate_control = true;
    /* White-box parity switch for the versioned successful fallback
     * properness-proof cache. Product/API solves keep reuse enabled. */
    bool fallback_properness_reuse_control = true;
    bool full_evidence = false;
    bool strict_states = false;
    bool kernel_reuse = true;
    bool goal_progress_gated_reforges = false;
    /* Caller-selected automatic-action scope. False excludes only generated
     * Imprint checkpoint/retry programs; all other automatic families retain
     * their normal admission authority. */
    bool consider_imprint_programs = true;
    /* Ordinary Bellman authority to abandon the current carrier and buy a
     * fresh base. Mechanic-owned replacement recovery (currently product
     * Fracture miss handling) remains available when this is false. */
    bool allow_economic_restart = true;
    /* Exact operator-major delayed-action scheduling. Calculator release-WASM
     * solves opt in; native callers retain the stable default unless they use
     * the historical diagnostic-bit bridge. */
    bool high_impact_executable_uppers = false;
    bool projected_reforge_frontier_diagnostic = false;
    bool factored_terminal_reforge_diagnostic = false;
    bool reforge_resource_accounting = true;
    bool raw_strict_reforge_oracle_diagnostic = false;
    double max_absolute_optimality_gap = 0.0;
    double max_relative_optimality_gap = 0.0;
    SolveProfile solve_profile = SolveProfile::Default;
    std::uint32_t solve_profile_override_mask = 0;
};

inline void apply_solve_profile_defaults(
    SolveOptions& options,
    const SolveProfile profile) {
    options.solve_profile = profile;
    if (profile != SolveProfile::CalculatorProductV1) return;
    options.goal_progress_gated_reforges = true;
    options.allow_economic_restart = false;
    options.consider_imprint_programs = false;
    options.high_impact_executable_uppers = true;
    options.max_absolute_optimality_gap = 0.0;
    options.max_relative_optimality_gap = 0.0;
    options.max_policy_refinement_states = 200000;
}

enum class SolvePolicyStatus : std::uint8_t {
    None,
    BoundedFeasible,
    BoundedNearOptimal,
    Exact,
};

enum class SolveTermination : std::uint8_t {
    None,
    RefusedResourceCap,
    TargetGap,
    ExactClosed,
    NoExecutablePolicy,
    NumericalStability,
    RequestedBoundedFinish,
};

inline bool advance_unreconciled_stable_policy_latch(
    const bool policy_improved,
    const bool strict_order_reconciled,
    std::uint32_t& consecutive_rounds) {
    if (policy_improved || strict_order_reconciled) {
        consecutive_rounds = 0;
        return false;
    }
    ++consecutive_rounds;
    return consecutive_rounds >= 2;
}

inline bool closed_coarse_candidate_requires_strict_lift(
        const bool coarse_discovery_closed,
        const std::uint64_t compatibility_triggers,
        const bool publication_already_final) {
    /* With no compatibility witness, direct compilation used to look like a
     * reason to skip strict work. A closed coarse envelope plus a failed
     * direct exact publication is instead precisely the small finite boundary
     * at which strict action-accounting can reconcile the quotient and earn a
     * global lower. Open/capped discovery retains the anytime bounded path. */
    return coarse_discovery_closed && compatibility_triggers == 0 &&
           !publication_already_final;
}

inline bool strict_lift_requires_eager_fallback_verification(
        const bool verified_executable_fallback_available) {
    /* Strict work can fail or exhaust its allowance without publishing a
     * partial proof. It therefore needs one independently evaluated executable
     * fallback, not eager evaluation of every retained estimate. When that
     * authority already exists, starting the lift cannot erase it. */
    return !verified_executable_fallback_available;
}

inline bool strict_result_requires_fallback_cost_comparison(
        const bool globally_exact) {
    /* A globally closed strict Bellman envelope proves that no requested
     * executable policy is cheaper. Unverified retained estimates cannot
     * displace that authority and need no final-graph evaluation. Bounded
     * strict results still compare independently evaluated uppers. */
    return !globally_exact;
}

inline bool closed_coarse_exact_path_defers_selected_snapshot(
        const bool coarse_discovery_closed,
        const bool current_policy_available) {
    /* A pre-extraction snapshot is an executable-upper opportunity only. Once
     * the coarse state/action envelope is fully closed, the current extracted
     * policy is the source for direct assertion and (when needed) strict
     * global closure. Certifying an older snapshot first cannot close the
     * lower and can consume most of the exact-proof wall budget. */
    return coarse_discovery_closed && current_policy_available;
}

/*
 * Bounded refinement preserves the coarse solve's genuine stopping cause. A
 * globally exact strict envelope is a new closure proof and supersedes that
 * earlier cause. This helper is only for a successfully retained executable
 * lift; consequently it can never return NoExecutablePolicy.
 */
SolveTermination successful_refined_publication_termination(
    SolveTermination coarse_termination,
    bool resource_cap_hit,
    bool globally_exact = false,
    bool coarse_discovery_closed = false);

enum class SolveGapTarget : std::uint8_t {
    None,
    Absolute,
    Relative,
    Both,
};

enum class SolveLowerBoundProvenance : std::uint8_t {
    None,
    OpenIncrementalEnvelopeUniversalZero,
    UnclosedStrictRefinementUniversalZero,
    ClosedIncrementalActionEnvelope,
    GlobalActionRelaxation,
    ExactPolicyClosure,
};

struct FocusedScheduleRoundTelemetry {
    std::uint32_t round = 0;
    std::uint64_t lower_candidates = 0;
    std::uint64_t upper_candidates = 0;
    std::uint64_t batch_states = 0;
    std::uint64_t lower_quota = 0;
    std::uint64_t upper_quota = 0;
    std::uint64_t lower_quota_admissions = 0;
    std::uint64_t upper_quota_admissions = 0;
    std::uint64_t lower_fill_admissions = 0;
    std::uint64_t upper_fill_admissions = 0;
    std::uint64_t schedule_candidates = 0;
    std::uint64_t schedule_admissions = 0;
    std::uint64_t carrier_ladder_candidates = 0;
    std::uint64_t carrier_ladder_goal_subsets = 0;
    std::uint64_t carrier_ladder_admissions = 0;
    std::uint64_t global_batch_cap_hits = 0;
    std::uint64_t per_class_cap_hits = 0;
};

struct FallbackValidationTelemetry {
    struct Component {
        std::uint64_t checks = 0;
        std::uint64_t duration_ns = 0;
    };

    std::uint64_t calls = 0;
    std::uint64_t total_ns = 0;
    std::uint64_t proof_version = 1;
    std::uint64_t successful_proof_cache_checks = 0;
    std::uint64_t successful_proof_cache_hits = 0;
    std::uint64_t successful_proof_cache_misses = 0;
    std::string successful_proof_last_miss_reason;
    Component goal_identity;
    Component economy_identity;
    Component action_vocabulary_identity;
    Component successful_proof_identity;
    Component structural;
    Component anchor_properness;
    Component start_properness;
};

/*
 * Bounded policy-guided exact-refinement telemetry. Counters are aggregate;
 * retained samples are capped by SolveDiagnostics::diagnostic_sample_limit
 * at the producer and report their omitted population explicitly.
 */
struct PolicyBroadRowAttribution {
    bool alternative = false;
    std::uint64_t row_sequence = 0;
    std::uint32_t source_strict_state = kNoId;
    std::uint32_t source_coarse_state = kNoId;
    std::uint64_t source_stable_key_hash = 0;
    std::uint32_t operator_index = kNoId;
    std::string planner_id;
    std::vector<std::string> primitive_program_action_ids;
    RefinementFeatureMask observation_item_features = 0;
    std::vector<std::uint32_t> observation_modifier_tag_ids;
    std::uint64_t observation_affix_selectors = 0;
    std::uint64_t raw_transitions = 0;
    ReforgeBuildAttribution reforge;
};

struct PolicyRefinementTelemetry {
    struct SuppressedStrictImprovementSample {
        std::uint32_t state = kNoId;
        std::uint64_t retained_row =
            std::numeric_limits<std::uint64_t>::max();
        std::uint64_t preferred_row =
            std::numeric_limits<std::uint64_t>::max();
        double retained_value = std::numeric_limits<double>::infinity();
        double preferred_value = std::numeric_limits<double>::infinity();
    };
    static constexpr std::size_t kSuppressedStrictImprovementSampleLimit = 8;

    std::uint64_t triggers = 0;
    /* Structured compatibility witnesses consumed by exact refinement. The
     * vector is unique, deterministic in discovery order, and bounded by the
     * already resource-capped coarse policy table; local refinement never
     * parses JSON samples. */
    std::vector<std::uint32_t> trigger_coarse_states;
    std::uint64_t trigger_coarse_states_omitted = 0;
    /* Stable publication-stage status; "not_run" means no policy required
     * exact refinement or assertion yet. */
    std::string status = "not_run";
    /* Canonical public Solve cap name, never an internal refinement alias. */
    std::string resource_cap;
    /* Publication pipeline stages are recorded separately so a later strict
     * refinement refusal cannot erase the selected core-policy evidence. */
    bool core_policy_candidate_present = false;
    std::string core_policy_status = "not_run";
    bool pre_extraction_non_goal_closed = false;
    bool coarse_action_envelope_closed = false;
    bool coarse_discovery_closed = false;
    double core_policy_lower_bound =
        std::numeric_limits<double>::infinity();
    double core_policy_upper_bound =
        std::numeric_limits<double>::infinity();
    double core_policy_evaluated_cost =
        std::numeric_limits<double>::infinity();
    std::uint64_t core_policy_transition_bits_hash = 0;
    std::uint64_t core_policy_bits_hash = 0;
    std::uint64_t core_policy_selected_states = 0;
    std::uint64_t core_policy_distinct_actions = 0;
    std::string core_policy_root_action;
    std::uint64_t core_policy_goal_identity = 0;
    std::uint64_t core_policy_economy_identity = 0;
    std::uint64_t core_policy_action_vocabulary_identity = 0;
    std::uint64_t core_policy_graph_identity = 0;
    std::uint64_t core_policy_artifact_identity = 0;
    std::uint64_t core_policy_owned_bytes = 0;
    std::string direct_certification_status = "not_run";
    std::string direct_certification_failure_reason;
    std::string direct_certification_failure_classification;
    std::string direct_certification_resource_cap;
    double direct_certification_solver_cost =
        std::numeric_limits<double>::infinity();
    double direct_certification_exact_cost =
        std::numeric_limits<double>::infinity();
    double direct_certification_offpolicy_probability =
        std::numeric_limits<double>::infinity();
    std::uint64_t direct_certification_reforge_work = 0;
    std::uint64_t direct_certification_artifact_bytes = 0;
    std::uint64_t direct_certification_peak_owned_bytes = 0;
    StrategyEvalStageTimings direct_certification_evaluation_stages;
    std::uint64_t direct_certification_evaluation_max_work_item_ns = 0;
    StrategyEvalSubphase
        direct_certification_evaluation_max_work_item_subphase =
            StrategyEvalSubphase::ModelSetup;
    StrategyEvalSubphase direct_certification_evaluation_boundary =
        StrategyEvalSubphase::ModelSetup;
    std::uint64_t
        direct_certification_pair_discovery_index_peak_bytes = 0;
    std::uint32_t direct_certification_raw_pairs = 0;
    std::uint32_t direct_certification_refined_pairs = 0;
    std::uint32_t direct_certification_refined_pair_limit = 0;
    std::uint64_t direct_certification_route_default_edges = 0;
    std::uint64_t direct_product_route_default_edges = 0;
    std::string direct_certification_route_default_mode;
    std::string direct_product_route_default_mode;
    bool direct_paired_default_only = false;
    bool direct_certification_executable = false;
    bool direct_certification_proper = false;
    bool direct_certification_cost_complete = false;
    bool direct_certification_zero_off_policy = false;
    bool direct_certification_cost_reconciled = false;
    bool direct_candidate_retained = false;
    std::string strict_lift_status = "not_run";
    std::string strict_lift_failure_reason;
    std::string strict_lift_resource_cap;
    bool strict_global_lower_bound_closed = false;
    std::string publication_status = "not_run";
    std::string published_candidate_kind;
    /* Gate-0 observational snapshot of the selected sparse row policy before
     * a certified output incumbent can replace it. This snapshot is never a
     * candidate, upper bound, or publication input. */
    bool pre_restore_policy_present = false;
    bool pre_restore_policy_materializable = false;
    bool pre_restore_policy_numerical_stop = false;
    double pre_restore_policy_start_value =
        std::numeric_limits<double>::infinity();
    double pre_restore_policy_residual =
        std::numeric_limits<double>::infinity();
    std::uint64_t pre_restore_policy_bits_hash = 0;
    std::uint64_t pre_restore_policy_selected_rows = 0;
    std::uint64_t pre_restore_policy_reachable_states = 0;
    std::uint64_t pre_restore_policy_reachable_rows = 0;
    std::uint64_t pre_restore_policy_distinct_actions = 0;
    std::uint64_t pre_restore_policy_choice_groups = 0;
    std::uint64_t pre_restore_policy_choice_options = 0;
    std::uint64_t pre_restore_policy_goal_identity = 0;
    std::uint64_t pre_restore_policy_economy_identity = 0;
    std::uint64_t pre_restore_policy_action_vocabulary_identity = 0;
    std::uint64_t pre_restore_policy_graph_identity = 0;
    std::uint64_t pre_restore_policy_artifact_identity = 0;
    std::uint64_t pre_restore_policy_source_generation = 0;
    std::uint64_t pre_restore_policy_target_generation = 0;
    std::uint64_t pre_restore_policy_snapshot_ns = 0;
    std::uint64_t pre_restore_policy_snapshot_peak_bytes = 0;
    bool selected_candidate_capture_attempted = false;
    bool selected_candidate_captured = false;
    bool selected_candidate_memory_rejected = false;
    bool selected_candidate_identity_valid = false;
    bool selected_candidate_certification_attempted = false;
    bool selected_candidate_independently_evaluated = false;
    bool selected_candidate_retained = false;
    double selected_candidate_estimated_cost =
        std::numeric_limits<double>::infinity();
    double selected_candidate_exact_cost =
        std::numeric_limits<double>::infinity();
    std::uint64_t selected_candidate_owned_bytes = 0;
    std::uint64_t selected_candidate_identity = 0;
    std::uint64_t selected_candidate_capture_ns = 0;
    std::uint64_t selected_candidate_certification_ns = 0;
    std::string selected_candidate_status = "not_run";
    std::string selected_candidate_failure_reason;
    std::uint64_t strict_order_suppressed_comparisons = 0;
    std::array<
        SuppressedStrictImprovementSample,
        kSuppressedStrictImprovementSampleLimit>
        strict_order_suppressed_samples{};
    std::uint32_t strict_order_suppressed_samples_retained = 0;
    std::uint64_t strict_order_suppressed_samples_omitted = 0;
    /* Wall attribution is diagnostic only; logical work and proof status
     * remain the deterministic authorities. */
    std::uint64_t incumbent_restore_ns = 0;
    std::uint64_t extraction_materialization_ns = 0;
    std::uint64_t direct_certification_ns = 0;
    std::uint64_t strict_lift_total_ns = 0;
    std::uint64_t strict_carrier_discovery_ns = 0;
    std::uint64_t strict_partition_refinement_ns = 0;
    std::uint64_t strict_policy_evaluation_ns = 0;
    std::uint64_t strict_local_reoptimization_ns = 0;
    std::uint64_t strategy_compilation_ns = 0;
    std::uint64_t exact_graph_evaluation_ns = 0;
    std::uint64_t strict_session_constructions = 0;
    std::uint64_t strict_full_restarts = 0;
    std::uint64_t strict_partition_updates = 0;
    std::uint64_t strict_frontier_insertions = 0;
    std::uint64_t strict_frontier_states_inserted = 0;
    std::uint64_t strict_frontier_update_max_ns = 0;
    std::uint64_t strict_cells_retained = 0;
    std::uint64_t strict_cells_created = 0;
    std::uint64_t strict_cells_superseded = 0;
    std::uint64_t policy_reachable_coarse_states = 0;
    /* Cumulative strict carrier materializations across lift/re-opt passes. */
    std::uint64_t exact_states = 0;
    /* States retained by the final shared refinement graph. */
    std::uint64_t retained_exact_states = 0;
    std::uint64_t exact_classes = 0;
    std::uint64_t initial_observation_classes = 0;
    std::uint64_t behavior_splits = 0;
    std::uint64_t merged_exact_states = 0;
    std::uint64_t exact_transitions = 0;
    std::uint64_t exact_kernels = 0;
    std::uint64_t exact_kernel_cache_hits = 0;
    /* Bounded selected-vs-alternative certification attribution. These
     * counters are observational only and carry no proof authority. */
    std::uint64_t selected_rows_begun = 0;
    std::uint64_t selected_rows_completed = 0;
    std::uint64_t selected_reforge_work = 0;
    std::uint64_t selected_transitions = 0;
    std::uint64_t alternative_rows_begun = 0;
    std::uint64_t alternative_rows_completed = 0;
    std::uint64_t alternative_reforge_work = 0;
    std::uint64_t alternative_transitions = 0;
    std::uint64_t strict_reforge_active_work = 0;
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
    /* A partial alternative row that exposes a strict successor outside the
     * current closed partition must return that frontier to the persistent
     * grow-in-place owner before unrelated obligations are replayed. */
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
    std::uint64_t memory_bytes = 0;
    std::uint64_t peak_memory_bytes = 0;
    std::uint64_t memory_limit_bytes = 0;
    std::uint64_t retained_artifact_bytes = 0;
    std::uint64_t fallback_portfolio_candidates = 0;
    std::uint64_t fallback_portfolio_invalidations = 0;
    std::uint64_t fallback_portfolio_compilation_failures = 0;
    std::uint64_t fallback_portfolio_memory_rejections = 0;
    std::uint64_t fallback_portfolio_owned_bytes = 0;
    std::uint64_t fallback_publication_attempts = 0;
    std::uint64_t fallback_publication_successes = 0;
    /* Authoritative, non-sampled publication contract. Candidate samples are
     * bounded diagnostics and may be omitted; harnesses use this bit to prove
     * that the published result was selected from the complete independently
     * evaluated portfolio by strict finite cost and stable identity. */
    bool cheapest_independently_evaluated_selected = false;
    double preferred_candidate_upper =
        std::numeric_limits<double>::infinity();
    double published_fallback_upper =
        std::numeric_limits<double>::infinity();
    double published_fallback_evaluated_cost =
        std::numeric_limits<double>::infinity();
    std::uint64_t published_fallback_witness_hash = 0;
    std::string published_fallback_kind;
    std::string preferred_publication_failure_reason;
    /* Complete JSON objects, bounded jointly by diagnostic sample count and
     * one quarter of the telemetry byte cap. */
    std::vector<std::string> publication_candidate_samples;
    std::uint64_t publication_candidate_samples_omitted = 0;
    std::uint64_t publication_candidate_sample_bytes = 0;
    std::vector<std::string> structural_failure_samples;
    std::uint64_t structural_failure_samples_omitted = 0;
    std::uint64_t structural_failure_sample_bytes = 0;
    std::vector<std::string> evaluator_memory_samples;
    std::uint64_t evaluator_memory_samples_omitted = 0;
    std::uint64_t evaluator_memory_sample_bytes = 0;
    /* Exact-state interner reuse is not an identity-destruction collapse. */
    std::uint64_t exact_state_reuses = 0;
    std::uint64_t collapse_events = 0;
    RefinementFeatureMask collapse_destroyed_feature_mask = 0;
    RefinementFeatureMask collapse_preserved_feature_mask = 0;
    std::array<
        std::uint64_t,
        static_cast<std::size_t>(RefinementFeature::Count)>
        collapse_events_by_feature{};
    std::array<
        std::uint64_t,
        static_cast<std::size_t>(RefinementFeature::Count)>
        preservation_events_by_feature{};
    std::uint64_t refinement_rounds = 0;
    std::uint64_t backward_observation_rounds = 0;
    std::uint64_t selected_action_routing_rounds = 0;
    std::uint64_t observation_propagation_rounds = 0;
    std::uint64_t partition_refinement_rounds = 0;
    std::uint64_t local_reoptimization_rounds = 0;
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
    std::uint64_t lumpability_checks = 0;
    bool fixed_point_checked = false;
    bool fixed_point_complete = false;
    bool lumpability_checked = false;
    bool lumpable = false;
    bool class_policy_checked = false;
    bool class_policy_proper = false;
    bool compiled_assertion_checked = false;
    bool compiled_policy_proper = false;
    bool zero_off_policy = false;
    bool cost_reconciled = false;
    bool policy_changed = false;
    bool coarse_value_reconciled = false;
    std::uint64_t counterexamples = 0;
    /* Each retained counterexample is one complete JSON object. */
    std::vector<std::string> counterexample_samples;
    std::uint64_t counterexample_samples_omitted = 0;
    std::uint64_t refusal_causes = 0;
    /* Refusal causes are stable engine-owned reason strings. */
    std::vector<std::string> refusal_cause_samples;
    std::uint64_t refusal_cause_samples_omitted = 0;
};

struct SolveDiagnostics {
    /* Actions the solve planned without, and why. */
    std::vector<std::string> skipped_missing_price;
    std::vector<std::string> skipped_unsupported;
    std::uint64_t skipped_missing_price_count = 0;
    std::uint64_t skipped_unsupported_count = 0;
    bool policy_compatibility_supported = true;
    std::uint32_t policy_compatibility_state = kNoId;
    std::string policy_compatibility_action;
    std::string policy_compatibility_reason;
    std::string policy_publication_failure_reason;
    PolicyRefinementTelemetry policy_refinement;
    std::uint32_t expanded_states = 0;
    std::uint32_t sweeps = 0;
    std::uint32_t policy_improvement_rounds = 0;
    bool policy_iteration_fallback = false;
    std::string policy_evaluation_failure;
    std::uint32_t policy_evaluation_calls = 0;
    std::uint32_t largest_policy_component = 0;
    std::uint64_t sparse_policy_iterations = 0;
    std::uint32_t max_sparse_policy_iterations = 0;
    std::uint32_t policy_kernel_groups = 0;
    std::uint32_t policy_states_collapsed = 0;
    double residual = 0.0;
    bool state_cap_hit = false;
    bool resource_cap_hit = false;
    /* The stepped host asked discovery to stop at a cooperative boundary and
     * publish the best independently verified executable policy retained so
     * far. This is neither graph closure nor a solver resource refusal. */
    bool requested_bounded_finish = false;
    std::uint32_t requested_bounded_finish_expanded_states = 0;
    std::uint64_t requested_bounded_finish_rows = 0;
    std::vector<std::string> cap_hits;
    std::uint32_t registry_actions = 0;
    std::uint32_t candidate_actions = 0;
    std::uint32_t evaluator_supported_actions = 0;
    std::uint32_t priced_scanned_actions = 0;
    std::uint32_t supported_priced_actions = 0;
    std::uint32_t relevance_reduced_actions = 0;
    std::uint32_t dependency_actions = 0;
    std::uint32_t deferred_actions = 0;
    std::uint32_t equivalent_actions_collapsed = 0;
    std::uint32_t equivalent_price_ties = 0;
    std::vector<std::string> action_inclusion_reasons;
    std::uint64_t action_inclusion_reasons_omitted = 0;
    std::uint32_t preservation_rows_considered = 0;
    std::uint32_t preservation_rows_pruned = 0;
    std::uint32_t preservation_rows_retained = 0;
    std::uint32_t certified_disposable_rows = 0;
    std::uint32_t constructive_state_certificates = 0;
    std::uint64_t constructive_state_operators_pruned = 0;
    std::uint32_t constructive_upper_first_expanded_state = 0;
    double constructive_upper_bound =
        std::numeric_limits<double>::infinity();
    std::vector<std::string> constructive_state_witnesses;
    std::uint64_t constructive_state_witnesses_omitted = 0;
    /* Each entry is one complete JSON object. Kept separately from the
     * legacy reason strings so exact carrier/control evidence stays typed. */
    std::vector<std::string> preservation_witnesses;
    std::uint64_t preservation_witnesses_omitted = 0;
    std::uint32_t automatic_rows_considered = 0;
    std::uint32_t automatic_rows_eligible = 0;
    std::uint32_t automatic_rows_rejected = 0;
    std::uint32_t automatic_rows_collapsed = 0;
    std::uint32_t automatic_rows_selected = 0;
    std::uint32_t automatic_rows_deferred = 0;
    std::array<AutomaticKindTelemetry, kAutomaticTelemetryKindCount>
        automatic_kind_telemetry{};
    AutomaticAdmissionPhaseTelemetry automatic_admission_phases;
    std::vector<std::string> automatic_candidate_witnesses;
    std::uint64_t automatic_candidate_witnesses_omitted = 0;
    std::uint64_t product_fracture_rows = 0;
    std::uint64_t product_fracture_raw_outcomes = 0;
    std::uint64_t product_fracture_hit_entries = 0;
    std::uint64_t product_fracture_miss_entries = 0;
    std::uint64_t product_fracture_parent_miss_states_interned = 0;
    std::uint64_t product_fracture_selected_rows = 0;
    std::uint64_t product_fracture_selected_properness_checked = 0;
    std::uint64_t product_fracture_selected_proper_rows = 0;
    std::uint64_t product_fracture_selected_improper_rows = 0;
    std::uint64_t product_fracture_selected_unproved_rows = 0;
    std::uint64_t product_fracture_q_evaluated_rows = 0;
    std::uint64_t product_fracture_q_cheaper_rows = 0;
    std::uint64_t product_fracture_q_tied_rows = 0;
    std::uint64_t product_fracture_q_costlier_rows = 0;
    std::uint64_t product_fracture_q_unresolved_rows = 0;
    double product_fracture_best_q_advantage = 0.0;
    double product_fracture_max_probability_error = 0.0;
    std::array<
        std::array<std::uint64_t, kMaxGoalSlots + 1>,
        7> product_fracture_shape_rows{};
    std::vector<std::string> product_fracture_witnesses;
    std::uint64_t product_fracture_witnesses_omitted = 0;
    std::uint32_t discovered_states = 0;
    std::uint32_t frontier_states = 0;
    std::uint32_t goal_states = 0;
    std::uint32_t policy_reachable_states = 0;
    std::uint64_t bellman_backups = 0;
    std::uint64_t bellman_action_evaluations = 0;
    std::uint64_t extraction_action_evaluations = 0;
    std::uint64_t sparse_rows = 0;
    std::uint64_t sparse_transitions = 0;
    std::uint64_t transition_bits_hash = 0;
    std::uint64_t policy_bits_hash = 0;
    std::string solution_scope = "globally_optimal_unrestricted";
    std::string solve_profile_id = "default";
    std::uint32_t solve_profile_override_mask = 0;
    /* Compact is the stable default aggregate document. Full evidence adds
     * bounded carrier/action/proof samples without changing calculations. */
    bool full_evidence = false;
    bool consider_imprint_programs = true;
    std::uint64_t algebraic_self_loops = 0;
    bool transition_cache_reused = false;
    bool focused_expansion = false;
    std::uint32_t focused_expansion_rounds = 0;
    /* Action-envelope-independent admissible floor supplied by the
     * optimistic goal-cover relaxation. Unlike the currently admitted-row
     * optimum, this remains globally valid while delayed actions are open. */
    double independent_goal_cover_lower_bound = 0.0;
    double focused_lower_bound = 0.0;
    double focused_upper_bound = std::numeric_limits<double>::infinity();
    double focused_partial_policy_upper_bound =
        std::numeric_limits<double>::infinity();
    std::uint32_t focused_partial_policy_rounds = 0;
    double focused_optimality_gap = std::numeric_limits<double>::infinity();
    double focused_exact_gap_proof_tolerance = 0.0;
    std::string incumbent_kind;
    std::uint32_t incumbent_round = 0;
    std::uint32_t incumbent_restart_state = kNoId;
    std::uint32_t incumbent_anchor_state = kNoId;
    std::uint64_t incumbent_goal_identity = 0;
    std::uint64_t incumbent_economy_identity = 0;
    std::uint64_t incumbent_action_vocabulary_identity = 0;
    std::uint64_t incumbent_graph_identity = 0;
    bool incumbent_strict_state_provenance = true;
    std::uint64_t focused_expansion_ns = 0;
    std::vector<FocusedScheduleRoundTelemetry> focused_schedule_rounds;
    FallbackValidationTelemetry fallback_validation;
    std::uint64_t constructive_policy_ns = 0;
    std::uint64_t strict_clean_goal_cover_ns = 0;
    std::uint64_t constructive_policy_syntheses = 0;
    std::uint64_t constructive_policy_reuses = 0;
    std::uint64_t constructive_policy_refreshes = 0;
    std::string constructive_policy_last_refresh_reason;
    std::uint64_t constructive_policy_anchor_checks = 0;
    std::uint64_t constructive_policy_anchor_eligible = 0;
    std::uint64_t constructive_policy_renewal_variants = 0;
    std::uint64_t constructive_policy_exit_checks = 0;
    std::uint64_t constructive_policy_finishable_exits = 0;
    std::uint64_t constructive_policy_feasible_policies = 0;
    std::string destructive_renewal_action_id;
    double destructive_renewal_value =
        std::numeric_limits<double>::infinity();
    double destructive_renewal_anchor_value =
        std::numeric_limits<double>::infinity();
    double destructive_renewal_start_value =
        std::numeric_limits<double>::infinity();
    std::uint64_t gated_root_renewal_candidates = 0;
    std::uint64_t gated_root_renewal_rejections = 0;
    std::uint64_t gated_root_renewal_validated_non_goal_states = 0;
    std::uint64_t gated_root_renewal_witness_hash = 0;
    double gated_root_renewal_success_probability = 0.0;
    std::string progressive_fracture_roll_action_id;
    std::string progressive_fracture_status;
    double progressive_fracture_value =
        std::numeric_limits<double>::infinity();
    double progressive_fracture_anchor_value =
        std::numeric_limits<double>::infinity();
    double progressive_fracture_start_value =
        std::numeric_limits<double>::infinity();
    std::uint32_t progressive_fracture_class_mask = 0;
    std::uint32_t progressive_fracture_class_mod_count = 0;
    double progressive_fracture_class_probability = 0.0;
    std::uint32_t progressive_fracture_post_modes = 0;
    std::uint64_t reforge_frontier_work = 0;
    std::uint64_t reforge_logical_work_v1 = 0;
    bool incremental_action_generation = false;
    bool incremental_action_envelope_closed = true;
    std::uint64_t incremental_actions_admitted = 0;
    std::uint64_t incremental_actions_non_improving = 0;
    std::uint64_t incremental_actions_unevaluated = 0;
    std::uint64_t incremental_actions_evaluating = 0;
    std::uint64_t incremental_actions_unresolved = 0;
    std::uint64_t incremental_actions_inapplicable = 0;
    std::uint64_t incremental_unique_kernel_evaluations = 0;
    std::uint64_t incremental_carrier_kernel_reuses = 0;
    std::uint64_t incremental_carrier_ladder_epochs = 0;
    std::uint64_t incremental_carrier_ladder_candidates = 0;
    std::uint64_t incremental_carrier_ladder_goal_subsets = 0;
    std::uint64_t incremental_states_outside_chaos_support = 0;
    std::uint64_t incremental_bellman_reoptimizations = 0;
    std::uint32_t incremental_first_alternative_expanded_states = 0;
    std::uint64_t incremental_refinement_rounds = 0;
    std::uint64_t incremental_refinement_states_selected = 0;
    std::uint64_t incremental_rows_reconsidered = 0;
    std::uint64_t incremental_upper_policy_updates = 0;
    std::uint64_t incremental_upper_policy_passes_requested = 0;
    std::uint64_t incremental_upper_policy_passes_started = 0;
    std::uint64_t incremental_upper_policy_passes_proper = 0;
    std::uint64_t incremental_upper_policy_passes_rejected = 0;
    std::uint64_t incremental_upper_policy_fixed_policy_proofs = 0;
    std::string incremental_upper_policy_last_failure;
    std::uint64_t incremental_anytime_policy_attempts = 0;
    std::uint64_t incremental_anytime_policy_successes = 0;
    std::uint64_t incremental_anytime_policy_last_completed_rows = 0;
    double incremental_anytime_policy_best_upper =
        std::numeric_limits<double>::infinity();
    std::string incremental_anytime_policy_last_failure;
    std::uint64_t incremental_missing_frontier_discovered = 0;
    std::uint64_t incremental_missing_frontier_priority_offers = 0;
    std::uint64_t incremental_missing_frontier_service_completions = 0;
    std::uint64_t incremental_missing_frontier_max_open = 0;
    std::uint64_t incremental_missing_frontier_open = 0;
    double incremental_refinement_uncertainty = 0.0;
    std::vector<std::string> incremental_action_witnesses;
    std::uint64_t incremental_action_witnesses_omitted = 0;
    /* Gate-1 typed ownership view for carrier/operator obligations. The
     * serialized aggregate is observational and the scheduler-consumer flag
     * remains false until a later gate explicitly migrates work ordering. */
    std::string action_envelope_ledger_json;
    std::string anytime_scheduler_json;
    /* Bounded observational join over existing registry, automatic-candidate,
     * ledger, scheduler, row, and selected-policy owners. It is never read by
     * mechanics, admission, scheduling, Bellman, or publication. */
    std::string operator_lineage_json;
    struct IncumbentPortfolioSnapshot {
        struct Identity {
            std::uint64_t portfolio = 0;
            std::uint64_t goal = 0;
            std::uint64_t economy = 0;
            std::uint64_t action_vocabulary = 0;
            std::uint64_t caller_scope = 0;
            std::uint64_t graph_prefix = 0;
            std::uint64_t artifact = 0;
            std::uint64_t source_generation = 0;
        };
        bool candidate_present = false;
        double candidate_estimate =
            std::numeric_limits<double>::infinity();
        std::string candidate_source;
        std::string candidate_stage = "none";
        bool candidate_verified = false;
        Identity candidate_identity;
        bool verified_upper_present = false;
        double verified_executable_upper =
            std::numeric_limits<double>::infinity();
        std::uint64_t verified_portfolio_identity = 0;
        Identity verified_identity;
        std::uint64_t verified_observations = 0;
        std::uint64_t verified_replacements = 0;
        bool verified_upper_monotone = true;
        double independent_global_lower = 0.0;
        SolveLowerBoundProvenance independent_global_lower_provenance =
            SolveLowerBoundProvenance::None;
        bool independent_global_lower_certified = false;
        double restricted_search_lower = 0.0;
        bool restricted_search_envelope_global = false;
        bool exact_closure_proved = false;
        double exact_closure_value =
            std::numeric_limits<double>::infinity();
    } incumbent_portfolio;
    /*
     * Finalization-only observational samples explaining the executable
     * upper inherited by high-impact successors of completed root
     * Fossil/Harvest rows. These strings are appended after solve resource
     * accounting is frozen, so collecting them cannot change scheduling or a
     * cap boundary.
     */
    std::vector<std::string> upper_policy_provenance_samples;
    std::uint64_t upper_policy_provenance_samples_omitted = 0;
    std::uint64_t upper_policy_provenance_candidate_count = 0;
    std::uint64_t upper_policy_provenance_retained_bytes = 0;
    /* Finalization-only aggregate for the run-local cap/renewal
     * investigation. Empty for ordinary product solves. */
    std::string upper_cap_zero_progress_audit_json;
    /* Gate-0 full-evidence attribution. This is assembled only after the
     * final policy/reachability result is frozen and is never consulted by
     * scheduling, pruning, proof, or publication. Empty means the caller did
     * not request full evidence or finalization did not complete. */
    std::string carrier_bound_attribution_json;
    std::uint64_t bellman_work_units = 0;
    std::uint32_t max_bellman_unit_transitions = 0;
    std::uint64_t solve_setup_ns = 0;
    std::uint64_t expansion_ns = 0;
    std::uint64_t expansion_prepare_ns = 0;
    std::uint64_t expansion_prepare_byte_audit_ns = 0;
    std::uint64_t expansion_prepare_admission_ns = 0;
    std::uint64_t expansion_prepare_diagnostics_ns = 0;
    std::uint64_t expansion_prepare_pricing_ns = 0;
    std::uint64_t expansion_kernel_ns = 0;
    std::uint64_t expansion_sparse_row_ns = 0;
    std::uint64_t expansion_row_byte_audit_ns = 0;
    std::uint64_t expansion_diagnostics_ns = 0;
    std::uint64_t expansion_release_ns = 0;
    std::uint64_t expansion_cap_byte_audit_ns = 0;
    std::uint64_t expansion_finalize_ns = 0;
    std::uint64_t expansion_finalize_byte_audit_ns = 0;
    std::uint64_t solve_owned_byte_ledger_requests = 0;
    std::uint64_t solve_owned_byte_reconciliations = 0;
    std::uint64_t solve_owned_byte_ledger_max_overestimate = 0;
    std::uint32_t strict_discovered_states = 0;
    std::uint32_t quotient_states = 0;
    std::uint32_t quotient_refinement_rounds = 0;
    std::uint32_t coarse_candidate_classes = 0;
    std::uint32_t max_strict_states_per_coarse_class = 0;
    bool state_scaling_shadow_only = false;
    std::uint32_t shadow_behavioral_classes = 0;
    std::uint32_t shadow_expanded_states_observed = 0;
    std::uint64_t literal_duplicate_states = 0;
    std::uint64_t exact_behavioral_merges = 0;
    std::uint64_t witnessed_non_equivalences = 0;
    std::uint64_t projected_successor_class_mismatches = 0;
    std::uint64_t exact_kernel_payload_reuses = 0;
    std::uint64_t exact_kernel_payload_bytes_saved = 0;
    std::uint64_t observation_signature_mismatches = 0;
    /* Each entry is a complete JSON object with an action id and the exact
     * cardinality of its observed strict-state kernels. */
    std::vector<std::string> action_observation_cardinalities;
    std::vector<std::string> equivalence_witnesses;
    std::uint64_t equivalence_witnesses_omitted = 0;
    std::uint64_t optimization_ns = 0;
    std::uint64_t extraction_ns = 0;
    std::uint64_t solver_owned_bytes_estimate = 0;
    std::uint64_t solver_live_owned_bytes_estimate = 0;
    std::uint64_t diagnostics_retained_bytes_estimate = 0;
    struct ActionSearchCost {
        std::uint64_t rows = 0;
        std::uint64_t raw_outcomes = 0;
        std::uint64_t retained_transitions = 0;
        std::uint64_t reforge_work = 0;
        std::uint64_t cache_requests = 0;
        std::uint64_t cache_hits = 0;
        std::uint64_t wall_ns = 0;
        std::uint64_t retained_bytes = 0;
        std::uint64_t root_rows = 0;
        std::uint64_t root_raw_outcomes = 0;
        std::uint64_t root_retained_transitions = 0;
        std::uint64_t interrupted_rows = 0;
        std::uint32_t last_interrupted_state = kNoId;
        std::uint32_t last_interrupted_operator = kNoId;
        std::uint32_t last_interrupted_cursor = 0;
        bool last_interrupted_root = false;
        std::string last_interrupted_cap;
    };
    std::map<std::string, ActionSearchCost> action_search_costs;
    /* Exact nested allocation ledger for action_search_costs. The product
     * envelope can contain tens of thousands of actions, so cap checkpoints
     * must not rescan every diagnostic row. */
    std::uint64_t action_search_costs_owned_bytes = 0;
    std::map<std::string, std::uint64_t> lower_policy_action_states;
    std::map<std::string, std::uint64_t> upper_policy_action_states;
    std::uint32_t diagnostic_sample_limit = 0;
    std::uint64_t telemetry_json_byte_limit = 0;
};

/*
 * Exact action-local witness for one executable fixed policy: repeat the
 * selected primitive destructive reforge until the configured goal is
 * reached. It is an upper-bound witness only. The complete action envelope
 * remains authoritative for lower bounds and exactness.
 */
struct PrimitiveRenewalWitness {
    bool valid = false;
    std::uint32_t operator_index = kNoId;
    std::uint32_t primitive_action = kNoId;
    double success_probability = 0.0;
    double value = std::numeric_limits<double>::infinity();
    std::uint64_t gated_kernel_bits_hash = 0;
    std::uint64_t witness_hash = 0;
    std::uint64_t validated_non_goal_states = 0;
    std::vector<std::uint64_t> kernel_signature;
};

/*
 * A policy-guided exact lift is compiled and independently evaluated while
 * its strict child CalcContext is alive. Retain that proven ordinary strategy
 * artifact so the public compiler returns the exact routers that were
 * certified instead of recompiling the rejected coarse parent.
 */
struct RetainedCompiledPolicyArtifact {
    std::string strategy_json;
    std::string certification_strategy_json;
    std::uint32_t working_states = 0;
    std::uint32_t behavioral_classes = 0;
    std::uint32_t policy_regions = 0;
    std::uint32_t infrastructure_nodes = 0;
    std::uint32_t policy_route_nodes = 0;
    std::uint32_t local_gated_route_nodes = 0;
    std::uint32_t primitive_region_nodes = 0;
    std::uint32_t additional_recipe_nodes = 0;
    std::uint32_t nodes = 0;
    std::uint32_t edges = 0;
    std::uint64_t total_condition_bytes = 0;
    std::uint64_t max_condition_bytes = 0;
    std::uint64_t condition_edges = 0;
    std::uint64_t unique_condition_literals = 0;
    std::uint64_t repeated_condition_occurrences = 0;
    std::uint64_t repeated_condition_bytes = 0;
    std::uint64_t policy_route_nondefault_edges = 0;
    std::uint64_t policy_route_distinct_targets = 0;
    std::uint64_t same_target_branch_groups = 0;
    std::uint64_t same_target_branch_edges = 0;
    std::uint64_t projected_same_target_edge_savings = 0;
    std::uint32_t max_policy_route_out_degree = 0;
    std::uint32_t max_policy_route_distinct_targets = 0;
    std::uint64_t exact_state_fallbacks = 0;
    std::uint64_t junk_predicates = 0;
    std::uint64_t policy_route_default_edges = 0;
    std::uint64_t policy_route_restart_default_edges = 0;
    std::uint64_t policy_route_offpolicy_default_edges = 0;
    std::string policy_route_default_mode;
    std::uint64_t certification_policy_route_default_edges = 0;
    std::uint64_t certification_policy_route_offpolicy_default_edges = 0;
    std::string certification_policy_route_default_mode;
    bool paired_default_only = false;
    std::uint64_t peak_owned_bytes = 0;
    std::uint64_t previously_accounted_peak_owned_bytes = 0;
    std::uint64_t complete_peak_owned_bytes = 0;
};

/*
 * Value table and policy over the reachable abstract state set. Vectors are
 * indexed by CalcContext state id; states never expanded (past the cap)
 * keep an infinite value and no policy action.
 */
struct SolveResult {
    bool converged = false;
    bool policy_available = false;
    SolvePolicyStatus policy_status = SolvePolicyStatus::None;
    SolveTermination termination = SolveTermination::None;
    SolveGapTarget target_fired = SolveGapTarget::None;
    bool target_met = false;
    double lower_bound = 0.0;
    bool global_lower_bound_certified = false;
    SolveLowerBoundProvenance lower_bound_provenance =
        SolveLowerBoundProvenance::None;
    double upper_bound = std::numeric_limits<double>::infinity();
    double evaluated_policy_cost = std::numeric_limits<double>::infinity();
    double absolute_optimality_gap = std::numeric_limits<double>::infinity();
    double relative_optimality_gap = std::numeric_limits<double>::infinity();
    double requested_absolute_optimality_gap = 0.0;
    double requested_relative_optimality_gap = 0.0;
    std::uint32_t start_state = kNoId;
    /*
     * Preserve the caller-authored concrete starting carrier for strategy
     * emission. A strict semantic class may intentionally merge modifier ids
     * that every admitted observer treats equivalently; materializing an
     * arbitrary member of that class must not rewrite the authored item.
     */
    bool has_exact_start_item = false;
    pc_item_state exact_start_item{};
    std::vector<double> values;
    /* Planner operator index or kNoId. Primitive operator indices are exactly
     * their registry action indices; appended options are tagged by kind. */
    std::vector<PolicyOperatorRef> policy;
    std::vector<std::uint8_t> expanded;
    std::vector<std::uint8_t> goal_states;
    std::vector<std::uint8_t> policy_reachable;
    /* For an abstract unveil policy action, concrete option mod ids in
     * Bellman-optimal preference order, parallel to the state table. */
    std::vector<std::vector<std::uint32_t>> unveil_preferences;
    /* State-local preferences for Unveil observations owned by a selected
     * fixed option. Unlike primitive Unveil, one option attempt can expose
     * several pre-Unveil abstract states. */
    std::vector<std::vector<ObservedUnveilPreference>>
        option_unveil_preferences;
    /* Exact outer-state quotient. Empty means strict state mode. Otherwise
     * every strict state maps deterministically to one representative strict
     * state id; representatives own Bellman rows and compiled policy. */
    std::vector<std::uint32_t> behavioral_representative_by_state;
    PrimitiveRenewalWitness primitive_renewal_witness;
    RetainedCompiledPolicyArtifact refined_policy_artifact;
    SolveDiagnostics diagnostics;
    SolveOptions options;
};

inline double solve_result_start_value(const SolveResult& result) {
    return result.start_state < result.values.size()
               ? result.values[result.start_state]
               : std::numeric_limits<double>::infinity();
}

namespace solve_detail {

/* A Bellman optimum over a strict subset of delayed actions is scheduling
 * evidence, not a certificate for the requested full action envelope. */
double globally_certified_action_envelope_lower_bound(
    double restricted_lower_bound,
    bool incremental_action_generation,
    bool incremental_action_envelope_closed,
    double independent_goal_cover_lower_bound = 0.0);

struct SolveLowerBoundAuthority {
    bool globally_certified = false;
    SolveLowerBoundProvenance provenance =
        SolveLowerBoundProvenance::None;
};

/* Classify the proof family behind the already-normalized public lower. Open
 * incremental work may publish a separately proved goal-cover floor; without
 * that authority it retains the universal-zero fail-safe. A closed envelope
 * owns the stronger admitted-row solver proof. */
SolveLowerBoundAuthority classify_public_lower_bound_authority(
    double lower_bound,
    SolvePolicyStatus policy_status,
    bool incremental_action_generation,
    bool incremental_action_envelope_closed,
    bool unclosed_strict_refinement = false,
    double independent_goal_cover_lower_bound = 0.0);

/* One final normalizer shared by direct, strict, and fallback publication.
 * Equality without Exact status is not a global closure certificate. */
void normalize_publication_result(SolveResult& result);

/* Returns null only when every public scalar/result-status claim has the
 * executable artifact required to witness it. */
const char* publication_invariant_invalid_reason(
    const SolveResult& result);

} // namespace solve_detail

enum class SolvePhase : std::uint8_t {
    Expanding = 0,
    Iterating = 1,
    Done = 2,
    Refining = 3,
    Compiling = 4,
    Certifying = 5,
};

enum class SolvePhaseOwner : std::uint8_t {
    Setup = 0,
    PlannerConstruction,
    TemporaryEffectPrecompile,
    DependencyPreparation,
    PrimitiveRows,
    StateLocalAutomaticSynthesis,
    LadderScheduling,
    BellmanOptimization,
    PolicyAssembly,
    Compilation,
    ExactEvaluation,
    Done,
};

struct SolveProgress {
    SolvePhase phase = SolvePhase::Expanding;
    SolvePhaseOwner phase_owner = SolvePhaseOwner::Setup;
    bool done = false;
    std::uint32_t expanded_states = 0;
    std::uint32_t sweeps = 0;
    double residual = 0.0;
    double start_value_bound = 0.0;
    double lower_bound = 0.0;
    double upper_bound = std::numeric_limits<double>::infinity();
    double absolute_optimality_gap = std::numeric_limits<double>::infinity();
    double relative_optimality_gap = std::numeric_limits<double>::infinity();
    std::uint32_t focused_round = 0;
    std::string incumbent_kind;
    std::uint32_t discovered_states = 0;
    std::uint32_t frontier_states = 0;
    std::uint64_t state_action_rows = 0;
    std::uint64_t transition_entries = 0;
    std::uint64_t reforge_work = 0;
    std::uint64_t finalization_work_items = 0;
    std::uint32_t refinement_states = 0;
    std::uint32_t refinement_kernels = 0;
    std::uint64_t refinement_transitions = 0;
    std::uint32_t refinement_rounds = 0;
    std::uint32_t refinement_classes = 0;
    std::uint64_t certification_discovered_pairs = 0;
    std::uint64_t certification_pending_pairs = 0;
    std::uint64_t certification_solved_sccs = 0;
    std::uint64_t certification_total_sccs = 0;
    std::uint64_t live_owned_bytes = 0;
    std::uint64_t peak_owned_bytes = 0;
};

/* Read-only, non-finalizing view of stepped work. It never extracts a policy
 * or changes convergence state, and can therefore be frozen on abandon. */
struct SolveTelemetrySnapshot {
    SolvePhase phase = SolvePhase::Expanding;
    bool abandoned = false;
    SolveDiagnostics diagnostics;
    double raw_start_bound = 0.0;
};

/* Stateful counterpart of solve(). Each expansion work item processes one
 * reachable abstract state; each iteration work item processes a bounded
 * sparse-row/transition unit within a deterministic Bellman sweep. finish()
 * extracts and certifies the policy before stepped work reports done. */
class SolveWork {
  public:
    SolveWork(
        CalcContext& calc,
        const pc_item_state& start_item,
        const std::unordered_map<std::string, double>& prices,
        const SolveOptions& options = {});
    ~SolveWork();
    SolveWork(SolveWork&&) noexcept;
    SolveWork& operator=(SolveWork&&) noexcept;
    SolveWork(const SolveWork&) = delete;
    SolveWork& operator=(const SolveWork&) = delete;

    void step(std::uint32_t max_work_items);
    /* Stop graph/action discovery at the next cooperative step boundary and
     * run normal bounded-policy finalization. Ordinary abandon remains the
     * prompt cancellation path and does not publish partial work. */
    void request_bounded_finish();
    SolveProgress progress() const;
    SolveTelemetrySnapshot telemetry_snapshot(bool abandoned = false) const;
    SolveResult finish();
    std::uint64_t live_owned_bytes() const;
    std::uint64_t peak_owned_bytes() const;

  private:
    friend struct SolveWorkTestAccess;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/*
 * Value iteration over the reachable closure of the start item. Action
 * costs are the descriptor cost vectors dotted with `prices`; actions with
 * unpriced keys or unsupported evaluators are excluded and reported. The
 * synthetic restart action upper-bounds every value, so iteration from
 * +infinity converges whenever the goal is reachable at all.
 */
SolveResult solve(
    CalcContext& calc,
    const pc_item_state& start_item,
    const std::unordered_map<std::string, double>& prices,
    const SolveOptions& options = {});

double q_directed_uncertainty_contribution(
    double probability, double lower, double upper);

/*
 * ML corpus line format (docs/solver/crafting-solver-plan.md, ML Data Logging):
 * one JSON object per state with features, value, policy action, and
 * policy reachability. Durable formatting/versioning happens at the
 * tooling layer; this provides the canonical per-solve records.
 */
std::string serialize_solve_log(
    const CalcContext& calc,
    const SolveResult& result);

struct PolicyCompilationTelemetry {
    std::uint64_t duration_ns = 0;
    std::uint32_t working_states = 0;
    std::uint32_t behavioral_classes = 0;
    std::uint32_t policy_regions = 0;
    /* Exact emitted graph census. Policy-route nodes include the root;
     * infrastructure excludes it and includes start/goal/off-policy plus any
     * compiler-owned bounded Restart node. Primitive region nodes are the
     * single-operation region leaders; additional recipe nodes cover observed
     * choices and multi-step fixed options. */
    std::uint32_t infrastructure_nodes = 0;
    std::uint32_t policy_route_nodes = 0;
    std::uint32_t local_gated_route_nodes = 0;
    std::uint32_t primitive_region_nodes = 0;
    std::uint32_t additional_recipe_nodes = 0;
    std::uint32_t nodes = 0;
    std::uint32_t edges = 0;
    std::uint64_t strategy_json_bytes = 0;
    std::uint64_t total_condition_bytes = 0;
    std::uint64_t max_condition_bytes = 0;
    /* Exact emitted-condition and generated policy-route density census.
     * Same-target groups are structurally disjoint siblings produced from
     * distinct values of one selected compiler feature. */
    std::uint64_t condition_edges = 0;
    std::uint64_t unique_condition_literals = 0;
    std::uint64_t repeated_condition_occurrences = 0;
    std::uint64_t repeated_condition_bytes = 0;
    std::uint64_t policy_route_nondefault_edges = 0;
    std::uint64_t policy_route_distinct_targets = 0;
    std::uint64_t same_target_branch_groups = 0;
    std::uint64_t same_target_branch_edges = 0;
    std::uint64_t projected_same_target_edge_savings = 0;
    std::uint32_t max_policy_route_out_degree = 0;
    std::uint32_t max_policy_route_distinct_targets = 0;
    std::uint64_t exact_state_fallbacks = 0;
    std::uint64_t junk_predicates = 0;
    /* Compiler-designated policy-router defaults only. Product graphs may
     * route these misses through Restart; certification graphs must fail
     * closed through the off-policy terminal. */
    std::uint64_t policy_route_default_edges = 0;
    std::uint64_t policy_route_restart_default_edges = 0;
    std::uint64_t policy_route_offpolicy_default_edges = 0;
    std::string policy_route_default_mode;
    std::uint64_t certification_policy_route_default_edges = 0;
    std::uint64_t certification_policy_route_offpolicy_default_edges = 0;
    std::string certification_policy_route_default_mode;
    bool paired_default_only = false;
    /* Peak compiler-owned buffers, excluding the retained CalcContext and
     * SolveResult charged by the caller. Structured refinement includes its
     * parent/member condition programs as well as the growing JSON document. */
    std::uint64_t peak_owned_bytes = 0;
    /* Keep the historic partial estimate and audited complete estimate side
     * by side. `peak_owned_bytes` and cap enforcement use the complete value;
     * the historic column exists only for attributable before/after reports. */
    std::uint64_t previously_accounted_peak_owned_bytes = 0;
    std::uint64_t complete_peak_owned_bytes = 0;
    std::string cap_hit;
};

std::string serialize_solver_telemetry(
    const CalcContext& calc,
    const SolveResult* result,
    const SolveTelemetrySnapshot* snapshot,
    const std::optional<std::uint64_t>& registry_generation_ns,
    const PolicyCompilationTelemetry* compilation);

std::uint64_t estimated_retained_solver_bytes(
    const CalcContext& calc,
    const SolveResult* result);

std::uint64_t fast_estimated_retained_solver_bytes(
    const CalcContext& calc,
    const SolveResult* result);


} // namespace solver
} // namespace poecraft
