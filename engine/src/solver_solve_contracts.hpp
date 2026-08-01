#pragma once

#include "solver_eval_types.hpp"

namespace poecraft {
namespace solver {

// --- DP solver core (S4) --------------------------------------------------------

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
    /* Exact work-scheduling controls. These do not change the admitted
     * action set, state identity, transition kernels, or production caps. */
    std::uint32_t focused_expansion_checkpoint = 32;
    std::uint32_t focused_expansion_queue_threshold = 1024;
    std::uint32_t focused_members_per_fringe_class = 64;
    std::uint32_t focused_expansion_batch_states = 256;
    std::uint32_t focused_lower_batch_states = 64;
    double focused_goal_progress_priority_multiplier = 256.0;
    /* Automatic Imprint discovery is deliberately bounded search, not a
     * mechanic-validity limit. Exhaustion is reported as a deferred solver
     * resource boundary in the automatic-candidate diagnostics. */
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
    /* Native benchmark investigation only; not exposed by the public ABI or
     * product bindings. */
    bool high_impact_executable_uppers = false;
    double max_absolute_optimality_gap = 0.0;
    double max_relative_optimality_gap = 0.0;
};

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
};

/*
 * Exact refinement changes publication proof, not the coarse solve's genuine
 * stopping cause. This helper is only for a successfully retained executable
 * lift; consequently it can never return NoExecutablePolicy.
 */
SolveTermination successful_refined_publication_termination(
    SolveTermination coarse_termination,
    bool resource_cap_hit);

enum class SolveGapTarget : std::uint8_t {
    None,
    Absolute,
    Relative,
    Both,
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
struct PolicyRefinementTelemetry {
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
    std::uint64_t memory_bytes = 0;
    std::uint64_t peak_memory_bytes = 0;
    std::uint64_t memory_limit_bytes = 0;
    std::uint64_t retained_artifact_bytes = 0;
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
    std::uint64_t algebraic_self_loops = 0;
    bool transition_cache_reused = false;
    bool focused_expansion = false;
    std::uint32_t focused_expansion_rounds = 0;
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
    std::string incremental_upper_policy_last_failure;
    double incremental_refinement_uncertainty = 0.0;
    std::vector<std::string> incremental_action_witnesses;
    std::uint64_t incremental_action_witnesses_omitted = 0;
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
    std::uint32_t working_states = 0;
    std::uint32_t policy_regions = 0;
    std::uint32_t nodes = 0;
    std::uint32_t edges = 0;
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

enum class SolvePhase {
    Expanding,
    Iterating,
    Done,
};

struct SolveProgress {
    SolvePhase phase = SolvePhase::Expanding;
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
 * extracts the policy only after the stepped work reports done. */
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
    SolveProgress progress() const;
    SolveTelemetrySnapshot telemetry_snapshot(bool abandoned = false) const;
    SolveResult finish();
    std::uint64_t live_owned_bytes() const;
    std::uint64_t peak_owned_bytes() const;

  private:
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
    std::uint32_t policy_regions = 0;
    std::uint32_t nodes = 0;
    std::uint32_t edges = 0;
    std::uint64_t strategy_json_bytes = 0;
    /* Peak compiler-owned buffers, excluding the retained CalcContext and
     * SolveResult charged by the caller. Structured refinement includes its
     * parent/member condition programs as well as the growing JSON document. */
    std::uint64_t peak_owned_bytes = 0;
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


} // namespace solver
} // namespace poecraft
