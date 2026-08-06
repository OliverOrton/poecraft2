#pragma once

#include "solver_model.hpp"

namespace poecraft {
namespace solver {

// --- calculation engine (S2): exact transition provider ------------------------

struct OutcomeEntry {
    std::uint32_t state = kNoId; /* interned successor state id */
    double probability = 0.0;

    bool operator==(const OutcomeEntry&) const = default;
};

/* An unveil first samples an offered set, then the policy chooses the
 * cheapest successor in that set. Ordinary action distributions leave these
 * vectors empty. */
struct OutcomeChoiceGroup {
    double probability = 0.0;
    std::vector<std::uint32_t> states;
    /* State on which this offered successor set was observed. Primitive
     * observed actions use their entry state; fixed programs retain the
     * intermediate carrier. kNoId is reserved for legacy/non-observed rows. */
    std::uint32_t observation_state = kNoId;

    bool operator==(const OutcomeChoiceGroup&) const = default;
};

struct OutcomeChoiceOption {
    std::uint32_t mod_id = kNoId;
    /* Bellman successor. A renewal option may normalize an exact-equivalent
     * retry result to its option entry state. */
    std::uint32_t state = kNoId;
    /* Concrete abstract state where the offer is visible, and the concrete
     * successor produced by selecting this modifier. Primitive Unveil uses
     * the action entry and state respectively. */
    std::uint32_t observation_state = kNoId;
    std::uint32_t actual_state = kNoId;

    bool operator==(const OutcomeChoiceOption&) const = default;
};

struct OutcomeDistribution {
    /* True when an exact evaluator exists for this action. Unsupported
     * mechanics report false so callers can skip or surface the gap. */
    bool supported = false;
    /* Reforge memo entries with no query-state-dependent fallback keep one
     * immutable absolute successor kernel alive for the solve. Consumers may
     * share storage and route its fringe once by object identity. */
    bool stable_shared_kernel = false;
    bool goal_progress_gated = false;
    std::vector<OutcomeEntry> entries; /* sorted by state id, sums to 1 */
    std::vector<OutcomeChoiceGroup> choice_groups;
    std::vector<OutcomeChoiceOption> choice_options;
    std::array<double, kMaxGoalSlots> slot_satisfied_probability{};
    double gated_terminal_probability = 0.0;
    double gated_retry_probability = 0.0;
    double gated_partial_probability = 0.0;
    std::uint32_t gated_terminal_state = kNoId;
    std::uint32_t gated_retry_state = kNoId;
    std::uint64_t gated_terminal_short_circuits = 0;
    std::uint64_t gated_retry_short_circuits = 0;
    std::uint64_t gated_partial_states = 0;
    std::uint64_t gated_kernel_bits_hash = 0;
};

/*
 * Exact price-independent result of one option decision row. S7.3 programs
 * return every finite exit; S7.4 may normalize a certified retry to the entry
 * state and may retain observation-owned choices. The Bellman row pays the
 * returned expected resources again on every normalized retry.
 */
struct OptionKernel {
    bool legal = false;
    bool supported = false;
    bool terminates_almost_surely = false;
    double expected_primitive_actions = 0.0;
    std::vector<std::pair<std::string, double>> expected_resources;
    std::vector<OutcomeEntry> exits;
    /* Observe-then-decide groups retain the sampled Unveil offer. */
    std::vector<OutcomeChoiceGroup> observation_choice_groups;
    std::vector<OutcomeChoiceOption> observation_choice_options;
    /* Concrete states hidden behind an exact-equivalent retry self-loop.
     * Fracture preparation additionally records states that route to the
     * conditional primitive Fracture step during graph expansion. */
    std::vector<std::uint32_t> retry_states;
    std::vector<std::uint32_t> continuation_states;
    /* Transient state-local raw attempt used only to compare an automatic
     * protected program with a cross-carrier baseline. Mapping to the parent
     * clears it before any retained Bellman kernel is stored. */
    std::vector<OutcomeEntry> automatic_candidate_attempt_entries;
    bool entry_continues = false;
    std::uint64_t retained_template_id = 0;
    /* Incremental selected-allocation accounting counts a shared kernel in
     * template storage once rather than once per carrier cache key. */
    mutable bool retained_template_storage = false;
    struct AutomaticEvidence {
        bool candidate = false;
        bool eligible = false;
        bool kernel_changed = false;
        bool setup_complete = false;
        bool cleanup_complete = false;
        bool recovery_complete = false;
        bool exits_complete = false;
        std::uint32_t relevant_goal_mask = 0;
        std::uint32_t kernel_change_mechanisms = 0;
        std::uint64_t baseline_kernel_hash = 0;
        std::uint64_t candidate_kernel_hash = 0;
        std::uint32_t fracture_raw_affix_count = 0;
        std::uint32_t fracture_acceptable_affix_count = 0;
        std::uint32_t fracture_restart_state = kNoId;
        double fracture_hit_probability = 0.0;
        double fracture_miss_probability = 0.0;
        double fracture_probability_sum = 0.0;
        std::string legality_result;
        std::string reason;
    } automatic;
};

struct ObservedUnveilChoice {
    std::uint32_t mod_id = kNoId;
    std::uint32_t successor_state = kNoId; /* Bellman-normalized successor */
    std::uint32_t actual_state = kNoId;

    bool operator==(const ObservedUnveilChoice&) const = default;
};

struct ObservedUnveilPreference {
    std::uint32_t observation_state = kNoId;
    std::vector<ObservedUnveilChoice> choices;

    bool operator==(const ObservedUnveilPreference&) const = default;
};

/* Legacy sidecar names remain stable, while this shared finalizer is keyed by
 * the action's semantic outcome-observation contract. */
void order_observed_modifier_choices(
    const ActionDescriptor& action,
    std::vector<OutcomeChoiceOption>& choices,
    const std::vector<double>& values);

/*
 * Collision-free exact state serialization shared by refinement and
 * executable-option routing. `coarse_parent` is a caller-owned namespace
 * component; zero denotes a context-independent strict-state key.
 */
std::vector<std::uint64_t> exact_abstract_state_key(
    const AbstractState& state,
    std::uint32_t coarse_parent);

struct ExecutableFixedOptionChoice {
    std::uint32_t mod_id = kNoId;
    bool retry_local = false;

    bool operator==(const ExecutableFixedOptionChoice&) const = default;
};

struct ExecutableFixedOptionOffer {
    std::vector<std::uint64_t> observation_state_key;
    std::vector<std::uint32_t> offered_mod_ids;
    std::vector<ExecutableFixedOptionChoice> ordered_choices;

    bool operator==(const ExecutableFixedOptionOffer&) const = default;
};

/*
 * The context-independent control recipe actually emitted for one fixed
 * option. Outer exit probabilities and expected resources belong to exact
 * evaluation, not routing identity. State keys are structural, never
 * CalcContext-local numeric handles.
 */
struct ExecutableFixedOptionRecipe {
    bool entry_continues = false;
    std::vector<std::vector<std::uint64_t>> retry_state_keys;
    std::vector<std::vector<std::uint64_t>> continuation_state_keys;
    std::vector<ExecutableFixedOptionOffer> offers;

    bool operator==(const ExecutableFixedOptionRecipe&) const = default;
};

/*
 * Reforge resource telemetry has three deliberately separate dimensions:
 * the historic active ledger, parallel evaluator ledgers, and physical
 * component counts. Component fields can overlap and therefore have no
 * additive "total". They explain implementations; wall, memory, and step
 * latency qualify them.
 */
enum class ReforgeRowOwner : std::uint8_t {
    Coarse,
    StrictSelected,
    StrictAlternative,
    ExactEvaluation,
};

enum class ReforgeRowFamily : std::uint8_t {
    Ordinary,
    Essence,
    Harvest,
    Fossil,
    AutomaticOption,
};

enum class ReforgeEvaluatorVersion : std::uint8_t {
    V1Raw,
    V2Sparse,
    V3Factored,
};

enum class ReforgeRowDisposition : std::uint8_t {
    Completed,
    Interrupted,
    Discarded,
    Published,
};

struct ReforgeEffortBreakdown {
    std::uint64_t rows_begun = 0;
    std::uint64_t rows_completed = 0;
    std::uint64_t rows_interrupted = 0;
    std::uint64_t rows_cache_reused = 0;
    std::uint64_t rows_discarded = 0;
    std::uint64_t rows_published = 0;
    std::uint64_t pool_entries_scanned = 0;
    std::uint64_t physical_families_built = 0;
    std::uint64_t roll_buckets_built = 0;
    std::uint64_t exclusion_group_checks = 0;
    std::uint64_t availability_classes_built = 0;
    std::uint64_t availability_words_built = 0;
    std::uint64_t frontier_nodes = 0;
    std::uint64_t dense_bucket_probes = 0;
    std::uint64_t availability_words_scanned = 0;
    std::uint64_t eligible_nonterminal_edges = 0;
    std::uint64_t terminal_contributions = 0;
    std::uint64_t canonical_terminal_successors = 0;
    std::uint64_t duplicate_terminal_contributions = 0;
    std::uint64_t raw_choice_entries = 0;
    std::uint64_t identity_tree_nodes = 0;
    std::uint64_t successor_publication_attempts = 0;
    std::uint64_t successor_unique_insertions = 0;
    std::uint64_t successor_duplicate_merges = 0;
    std::uint64_t state_interning_attempts = 0;
    std::uint64_t v3_predecessor_index_entries = 0;
    std::uint64_t v3_denominator_edges = 0;
    std::uint64_t v3_subset_checks = 0;
    std::uint64_t v3_candidate_sets = 0;
    std::uint64_t v3_recurrence_terms = 0;
    std::uint64_t v3_commits = 0;
    std::uint64_t nested_automatic_child_logical_work = 0;
    std::uint64_t nested_automatic_child_active_work = 0;

    bool operator==(const ReforgeEffortBreakdown&) const = default;
};

struct ReforgeRowTelemetry {
    std::uint64_t sequence = 0;
    std::uint32_t action_index = kNoId;
    ReforgeRowOwner owner = ReforgeRowOwner::Coarse;
    ReforgeRowFamily family = ReforgeRowFamily::Ordinary;
    ReforgeEvaluatorVersion evaluator = ReforgeEvaluatorVersion::V1Raw;
    ReforgeRowDisposition disposition = ReforgeRowDisposition::Completed;
    bool cache_reused = false;
    std::uint64_t legacy_active_work = 0;
    std::uint64_t logical_work_v1 = 0;
    std::uint64_t evaluator_work_v1 = 0;
    std::uint64_t evaluator_work_v2 = 0;
    std::uint64_t evaluator_work_v3 = 0;
    ReforgeEffortBreakdown components;
};

const char* reforge_row_owner_name(ReforgeRowOwner value);
const char* reforge_row_family_name(ReforgeRowFamily value);
const char* reforge_evaluator_version_name(ReforgeEvaluatorVersion value);
const char* reforge_row_disposition_name(ReforgeRowDisposition value);
void merge_reforge_effort(
    ReforgeEffortBreakdown& target,
    const ReforgeEffortBreakdown& source);
ReforgeEffortBreakdown reforge_effort_delta(
    const ReforgeEffortBreakdown& after,
    const ReforgeEffortBreakdown& before);

/*
 * One source-authoritative destructive-reforge build profile. These samples
 * describe the raw evaluator's actual intermediate representation rather
 * than inferring phase ownership from the final transition count. They are
 * diagnostic only: no solver decision or proof may depend on them.
 */
struct ReforgeBuildAttribution {
    struct TerminalAggregate {
        std::uint64_t branches = 0;
        std::uint64_t canonical_commits = 0;
        std::uint64_t duplicate_canonical_commits = 0;
        std::uint64_t duplicates_within_predecessor = 0;
        std::uint64_t duplicates_across_predecessors = 0;
        double probability_mass = 0.0;
        double duplicate_probability_mass = 0.0;
    };

    struct TerminalTargetAggregate {
        std::uint64_t state_contributions = 0;
        std::uint64_t final_modifier_branches = 0;
        double probability_mass = 0.0;
        double final_modifier_probability_mass = 0.0;
    };

    struct TerminalContributionSample {
        std::uint32_t predecessor_sequence = 0;
        std::uint64_t predecessor_signature_hash = 0;
        std::uint64_t predecessor_order_multiplicity = 0;
        std::uint8_t predecessor_sat_mask = 0;
        std::uint8_t predecessor_below_mask = 0;
        std::uint8_t predecessor_blocked_mask = 0;
        std::uint8_t predecessor_prefix_picks = 0;
        std::uint8_t predecessor_suffix_picks = 0;
        std::uint32_t predecessor_availability_class = kNoId;
        std::uint8_t target_count = 0;
        std::uint32_t terminal_bucket = kNoId;
        std::int8_t terminal_side = -1;
        std::uint8_t terminal_bucket_kind = 0;
        std::uint8_t terminal_goal_slot = 0;
        std::uint32_t terminal_junk_class = kNoId;
        std::uint32_t terminal_block_mask = 0;
        std::uint32_t terminal_bucket_multiplicity = 0;
        std::uint32_t terminal_available_family_choices = 0;
        std::uint32_t terminal_exclusion_group_count = 0;
        std::uint64_t terminal_exclusion_signature_hash = 0;
        std::uint32_t canonical_successor_state = kNoId;
        std::uint64_t canonical_successor_hash = 0;
        bool duplicate_canonical_successor = false;
        bool duplicate_within_predecessor = false;
        bool duplicate_across_predecessors = false;
        bool different_terminal_bucket_from_first = false;
        bool different_predecessor_observation_from_first = false;
        double probability_mass = 0.0;
    };

    std::string action_id;
    std::uint32_t action_index = kNoId;
    std::uint64_t preserved_base_hash = 0;
    bool goal_progress_gated = false;
    bool projected_sparse_frontier = false;
    bool factored_terminal_accumulator = false;
    bool completed = false;
    std::uint32_t forced_modifier_count = 0;
    std::uint64_t natural_pool_entries = 0;
    std::uint64_t natural_pool_weight = 0;
    std::uint64_t natural_prefix_entries = 0;
    std::uint64_t natural_prefix_weight = 0;
    std::uint64_t natural_suffix_entries = 0;
    std::uint64_t natural_suffix_weight = 0;
    std::uint64_t guaranteed_pool_entries = 0;
    std::uint64_t guaranteed_pool_weight = 0;
    std::uint64_t physical_families = 0;
    std::uint64_t roll_buckets = 0;
    std::uint64_t prefix_buckets = 0;
    std::uint64_t suffix_buckets = 0;
    std::uint64_t goal_satisfied_buckets = 0;
    std::uint64_t goal_below_buckets = 0;
    std::uint64_t junk_buckets = 0;
    std::uint64_t raw_choice_entries = 0;
    std::uint64_t exclusion_group_entries = 0;
    std::uint64_t exclusion_pair_checks = 0;
    std::uint64_t exclusion_conflicts = 0;
    /* Families/classes that the generic isolated-family proof could merge
     * under the current abstract projection. Raw mode measures these without
     * applying the merge. */
    std::uint64_t projectable_physical_families = 0;
    std::uint64_t projectable_family_classes = 0;
    std::uint64_t projected_families_removed = 0;
    std::uint64_t frontier_state_visits = 0;
    std::uint64_t frontier_edges = 0;
    std::uint64_t maximum_frontier_states = 0;
    std::uint64_t terminal_roll_states = 0;
    std::uint64_t raw_identity_tree_nodes = 0;
    std::uint64_t raw_identity_tree_leaves = 0;
    std::uint64_t successor_commits = 0;
    std::uint64_t unique_projected_outcomes = 0;
    std::uint64_t duplicate_projected_outcomes = 0;
    double duplicate_projected_probability_mass = 0.0;
    std::array<std::uint64_t, kMaxExplicitAffixes + 1>
        terminal_prefix_counts{};
    std::array<std::uint64_t, kMaxExplicitAffixes + 1>
        terminal_suffix_counts{};
    /* Final-pick causal accounting. This is bounded, diagnostic-only
     * telemetry: aggregate ledgers are exact for the completed row and the
     * sample vector retains only a deterministic semantic prefix. */
    std::array<TerminalTargetAggregate, kMaxExplicitAffixes + 1>
        terminal_target_counts{};
    std::array<TerminalAggregate, 2> terminal_side_counts{};
    std::array<TerminalAggregate, 3> terminal_bucket_kind_counts{};
    std::array<TerminalAggregate, 4>
        terminal_predecessor_observation_counts{};
    std::uint64_t final_depth_predecessors = 0;
    std::uint64_t final_depth_branches = 0;
    std::uint64_t final_depth_canonical_commits = 0;
    std::uint64_t final_depth_unique_canonical_successors = 0;
    std::uint64_t final_depth_duplicate_canonical_commits = 0;
    std::uint64_t final_depth_duplicates_within_predecessor = 0;
    std::uint64_t final_depth_duplicates_across_predecessors = 0;
    std::uint64_t final_depth_duplicates_same_terminal_bucket = 0;
    std::uint64_t final_depth_duplicates_different_terminal_bucket = 0;
    std::uint64_t final_depth_duplicates_different_terminal_side = 0;
    std::uint64_t final_depth_duplicates_different_terminal_kind = 0;
    std::uint64_t
        final_depth_duplicates_different_terminal_exclusion_signature = 0;
    std::uint64_t
        final_depth_duplicates_different_predecessor_observation = 0;
    std::uint64_t
        final_depth_duplicates_different_predecessor_availability = 0;
    std::uint64_t final_depth_successors_with_multiple_predecessors = 0;
    std::uint64_t final_depth_predecessor_convergence_excess = 0;
    std::uint64_t final_depth_max_predecessors_per_successor = 0;
    std::uint64_t final_depth_predecessors_with_multiple_orders = 0;
    std::uint64_t final_depth_max_predecessor_order_multiplicity = 0;
    std::uint64_t final_depth_predecessor_order_excess = 0;
    std::uint64_t final_depth_represented_order_paths = 0;
    std::uint64_t final_depth_terminal_order_excess = 0;
    std::uint64_t final_depth_physical_modifier_choices = 0;
    std::uint64_t final_depth_branches_with_exclusion_observation = 0;
    double final_depth_probability_mass = 0.0;
    double final_depth_duplicate_probability_mass = 0.0;
    double final_depth_within_predecessor_duplicate_probability_mass = 0.0;
    double final_depth_across_predecessor_duplicate_probability_mass = 0.0;
    double final_depth_same_terminal_bucket_duplicate_probability_mass = 0.0;
    double
        final_depth_different_terminal_bucket_duplicate_probability_mass = 0.0;
    double
        final_depth_different_predecessor_observation_duplicate_probability_mass =
            0.0;
    std::vector<TerminalContributionSample> terminal_contribution_samples;
    std::uint64_t terminal_contribution_samples_omitted = 0;
    std::uint64_t raw_choice_table_work = 0;
    std::uint64_t guaranteed_scan_work = 0;
    std::uint64_t frontier_work = 0;
    std::uint64_t raw_identity_tree_work = 0;
    std::uint64_t total_reforge_work = 0;
    std::uint64_t raw_equivalent_reforge_work = 0;
    std::uint64_t projected_reforge_work = 0;
    std::uint64_t factored_reforge_work = 0;
    std::uint64_t factored_terminal_predecessors = 0;
    std::uint64_t factored_terminal_candidates = 0;
    std::uint64_t factored_canonical_subset_checks = 0;
    std::uint64_t factored_last_pick_terms = 0;
    std::uint64_t factored_terminal_commits = 0;
    std::uint64_t factored_retry_aggregates = 0;
    std::uint64_t factored_subset_cache_hits = 0;
    std::uint64_t factored_subset_cache_misses = 0;
    std::uint64_t factored_subset_identity_mismatches = 0;
    std::uint64_t pool_build_ns = 0;
    std::uint64_t bucket_build_ns = 0;
    std::uint64_t exclusion_build_ns = 0;
    std::uint64_t frontier_build_ns = 0;
    std::uint64_t finalize_ns = 0;
    std::uint64_t total_build_ns = 0;
    std::uint64_t structural_bits_hash = 0;
};

/* Per-solve transition-provider telemetry. The distribution cache itself
 * survives price-only re-solves; reset_solve_telemetry clears only counters
 * and the set of rows touched by the next solve. */
struct CalcTelemetry {
    std::uint64_t distribution_requests = 0;
    std::uint64_t distribution_hits = 0;
    std::uint64_t distribution_misses = 0;
    std::uint64_t distribution_build_ns = 0;
    std::uint64_t state_action_rows = 0;
    std::uint64_t transition_entries = 0;
    std::uint64_t outcome_entries = 0;
    std::uint64_t choice_groups = 0;
    std::uint64_t choice_successor_entries = 0;
    std::uint64_t reforge_requests = 0;
    std::uint64_t reforge_hits = 0;
    std::uint64_t reforge_misses = 0;
    std::uint64_t reforge_build_ns = 0;
    /* Historic active-evaluator ledger. Retained verbatim for comparisons
     * with prior evidence; it no longer defines search admission. */
    std::uint64_t reforge_frontier_work = 0;
    /* Stable V1-equivalent logical envelope consumed by max_reforge_work.
     * This is distinct from raw-equivalent attempted effort so an interrupted
     * over-cap charge remains observable without admitting or publishing it. */
    std::uint64_t reforge_logical_work_v1 = 0;
    /* Parallel effort ledgers. V1 raw-equivalent work retains the historic
     * one-node-plus-all-buckets definition. V2 projected work counts one
     * sparse node, availability words inspected, and eligible edges. V3
     * factored work retains V2's nonterminal charges, then counts canonical
     * terminal candidates, every exact last-pick recurrence term, and each
     * completed terminal publication. These are observational; the public
     * cap applies only to reforge_logical_work_v1. */
    std::uint64_t reforge_raw_equivalent_work = 0;
    std::uint64_t reforge_projected_work = 0;
    std::uint64_t reforge_factored_work = 0;
    ReforgeEffortBreakdown reforge_effort;
    std::vector<ReforgeRowTelemetry> reforge_row_samples;
    std::uint64_t reforge_row_samples_omitted = 0;
    std::uint64_t reforge_row_sequence = 0;
    std::vector<ReforgeBuildAttribution> reforge_build_attribution_samples;
    std::uint64_t reforge_build_attribution_omitted = 0;
    std::uint64_t gated_reforge_rows = 0;
    double gated_terminal_probability = 0.0;
    double gated_retry_probability = 0.0;
    double gated_partial_probability = 0.0;
    std::uint64_t gated_terminal_short_circuits = 0;
    std::uint64_t gated_retry_short_circuits = 0;
    std::uint64_t gated_partial_states = 0;
    std::uint64_t gated_first_kernel_bits_hash = 0;
    std::uint64_t protected_retry_checks = 0;
    std::uint64_t protected_retry_certificates = 0;
    std::uint64_t protected_retry_fallbacks = 0;
    std::uint64_t protected_attempt_ns = 0;
    std::uint64_t protected_baseline_ns = 0;
    std::uint64_t protected_normalization_ns = 0;
    std::uint64_t protected_finish_ns = 0;
    std::uint64_t owned_byte_audit_requests = 0;
    std::uint64_t owned_byte_audit_ns = 0;
    std::uint64_t owned_byte_ledger_requests = 0;
    std::uint64_t owned_byte_ledger_ns = 0;
    std::uint64_t owned_byte_ledger_child_context_visits = 0;
    std::uint64_t owned_byte_ledger_max_recursion_depth = 0;
    std::uint64_t owned_byte_reconciliations = 0;
    std::uint64_t owned_byte_ledger_max_overestimate = 0;
    std::array<PrimitiveFamilyTelemetry, kPrimitiveTelemetryFamilyCount>
        primitive_families{};
};

/* Price-independent sparse closure retained by CalcContext between compatible
 * solves. Its definition is private to solver_solve.cpp; the context only
 * owns the last completed closure so a price-only solve can reuse it. */
struct SolveTransitionCache;

class SolverResourceLimit : public std::length_error {
  public:
    SolverResourceLimit(std::string cap_name, std::uint64_t limit)
        : std::length_error(
              "solver exceeded " + cap_name + " (" +
              std::to_string(limit) + ")"),
          cap_name_(std::move(cap_name)) {}
    const std::string& cap_name() const { return cap_name_; }

  private:
    std::string cap_name_;
};

struct ActionControlSummary {
    bool explicit_envelope = false;
    std::uint32_t registry_actions = 0;
    std::uint32_t included_primitives = 0;
    std::uint32_t dependency_primitives = 0;
    std::uint32_t pruned_outside_goal_relevance = 0;
    std::uint32_t pruned_outside_envelope = 0;
    std::uint32_t deferred_fossil_loadouts = 0;
    std::uint32_t automatic_options = 0;
    std::uint32_t automatic_dependency_primitives = 0;
};

struct AutomaticAdmissionLimits {
    std::uint32_t max_discovered_states = 0;
    std::uint64_t max_state_action_rows = 0;
    std::uint64_t max_transitions = 0;
    std::uint64_t max_reforge_work = 0;
    std::uint64_t max_solver_owned_bytes = 0;
    std::uint32_t max_imprint_program_depth = 0;
    std::uint64_t max_imprint_program_work = 0;
    const std::unordered_map<std::string, double>* prices = nullptr;
    double incumbent_upper_bound = std::numeric_limits<double>::infinity();
};

struct StateLocalAutomaticCandidate {
    std::string id;
    AutomaticCandidateKind kind = AutomaticCandidateKind::None;
    std::uint32_t operator_index = kNoId;
    bool admitted = false;
    bool collapsed = false;
    bool deferred = false;
    bool missing_price = false;
    AutomaticTelemetryKind telemetry_kind = AutomaticTelemetryKind::None;
    bool template_hit = false;
    std::uint64_t template_id = 0;
    std::uint64_t raw_outcomes = 0;
    std::uint64_t admission_ns = 0;
    std::uint64_t kernel_evaluation_ns = 0;
    std::uint64_t outcome_mapping_ns = 0;
    std::uint64_t template_matching_ns = 0;
    std::uint64_t protected_side_evaluations = 0;
    std::uint64_t protected_repeat_evaluations = 0;
    std::uint64_t protected_retry_checks = 0;
    std::uint64_t protected_retry_certificates = 0;
    std::uint64_t protected_retry_fallbacks = 0;
    std::uint64_t protected_attempt_ns = 0;
    std::uint64_t protected_baseline_ns = 0;
    std::uint64_t protected_normalization_ns = 0;
    std::uint64_t protected_finish_ns = 0;
    std::uint64_t selected_bytes = 0;
    OptionKernel::AutomaticEvidence evidence;
};

struct StateLocalAutomaticBatch {
    bool cached = false;
    AutomaticAdmissionPhaseTelemetry phases;
    std::array<std::uint64_t, kAutomaticTelemetryKindCount>
        shared_admission_ns{};
    std::uint64_t temporary_precompiled_classes = 0;
    std::uint64_t temporary_precompile_ns = 0;
    std::uint64_t temporary_precompiled_bytes = 0;
    std::uint64_t temporary_candidate_variants = 0;
    std::uint64_t temporary_effect_classes = 0;
    std::uint64_t temporary_collapsed_variants = 0;
    std::uint64_t temporary_enumeration_ns = 0;
    std::vector<StateLocalAutomaticCandidate> decisions;
    std::vector<std::uint32_t> admitted_operators;
};

/* State-independent temporary-bench vocabulary. A class records the exact
 * mod conflict mask shared by one or more separately priced bench actions;
 * carrier admission intersects it with the following add-mod action's exact
 * eligible pool before constructing any fixed option. */
struct TemporaryBenchEffectClass {
    std::uint32_t followup_action = kNoId;
    std::uint32_t goal_slot = kNoId;
    std::int8_t blocker_side = -1;
    std::vector<std::uint64_t> conflict_mask;
    std::vector<std::uint64_t> target_mask;
    std::vector<std::uint32_t> blocker_actions;
};

/* True when CalcContext has an exact evaluator dispatch for this descriptor,
 * independent of the current state. */
bool calc_supports(const ActionDescriptor& action);

/* Outcome-observation capability of the exact calculator handler. Contract
 * admission cross-checks this against ActionRefinementContract so an action
 * cannot advertise a choice protocol its mechanic kernel never emits. */
RefinementOutcomeObservation calc_outcome_observation(
    const ActionDescriptor& action);

struct ReforgeProvenanceCheckpoint {
    ReforgeRowOwner previous_owner = ReforgeRowOwner::Coarse;
    std::optional<ReforgeRowFamily> previous_family_override;
    std::uint64_t first_sequence = 0;
    std::uint64_t completed_before = 0;
    std::uint64_t cache_reused_before = 0;
};

/*
 * The solver's inner loop and the Calculator's backend: from abstract state
 * s, applying action a, the exact distribution over abstract successors.
 * Owns the state table, the price-independent distribution cache, and a
 * private worker context for pool construction. One CalcContext belongs to
 * one thread at a time.
 */
class CalcContext {
  public:
    CalcContext(
        std::shared_ptr<const SessionImpl> session,
        const GoalSpec& goal,
        ActionRegistry registry,
        const std::vector<std::uint32_t>& action_indices = {},
        bool allow_empty_goal = false,
        bool empty_actions_mean_all = true,
        bool distinguish_junk_exclusion_effects = false,
        std::optional<std::uint32_t> state_cap = std::nullopt,
        const std::vector<CountObservation>& count_observations = {},
        bool product_solver_parent = false,
        const std::vector<std::uint64_t>&
            required_reachable_mod_mask = {},
        bool distinguish_modifier_identity = false,
        bool capture_reforge_attribution = false,
        bool use_projected_reforge_frontier = false,
        bool reverse_reforge_bucket_enumeration = false,
        bool use_factored_terminal_reforge = false);

    const SessionImpl& session() const { return *session_; }
    const AbstractLayout& layout() const { return layout_; }
    const ActionRegistry& registry() const { return registry_; }
    const std::vector<PlannerOperator>& operators() const {
        return operators_;
    }
    const GoalSpec& goal() const { return goal_; }
    bool product_solver_parent() const { return product_solver_parent_; }
    bool distinguishes_modifier_identity() const {
        return distinguish_modifier_identity_;
    }
    /* The candidate action subset the layout was derived for. Normal solver
     * construction defaults an empty input to every registry action; an
     * operation-free strategy evaluation deliberately retains an empty set. */
    const std::vector<std::uint32_t>& candidates() const {
        return candidates_;
    }
    const std::vector<std::uint32_t>& candidate_operators() const {
        return candidate_operators_;
    }
    const std::vector<std::uint32_t>& automatic_goal_bench_actions() const {
        return automatic_goal_bench_actions_;
    }
    const std::vector<TemporaryBenchEffectClass>&
    temporary_bench_effect_classes() const {
        return temporary_bench_effect_classes_;
    }
    std::uint64_t temporary_bench_precompile_ns() const {
        return temporary_bench_precompile_ns_;
    }
    std::uint64_t temporary_bench_precompiled_bytes() const {
        return temporary_bench_precompiled_bytes_;
    }
    std::vector<std::uint64_t> temporary_followup_eligible_mask(
        const pc_item_state& carrier,
        std::uint32_t followup_action);
    /* A collision-free analytical ceiling for drawing a satisfying member of
     * one goal slot. Existing satisfied goal families are treated as exact
     * group blockers; each additional junk blocker receives the strongest
     * non-metamod exclusion effect available on the carrier. */
    double optimistic_goal_draw_probability(
        std::uint32_t carrier_state,
        std::uint32_t action_index,
        std::uint32_t goal_slot,
        std::uint32_t satisfied_mask,
        std::uint8_t prefix_blockers,
        std::uint8_t suffix_blockers,
        bool guaranteed_pool = false);
    std::size_t static_candidate_operator_count() const {
        return static_candidate_operator_count_;
    }
    /*
     * Read-only membership in the already-filtered planner vocabulary.
     * Static/non-state-local candidates are admitted for every valid state.
     * A generated state-local automatic candidate is admitted only for a
     * state whose retained admission batch contains that operator. This never
     * synthesizes candidates or populates the state-local admission cache.
     */
    bool is_candidate_operator_admitted_for_state(
        std::uint32_t state_id,
        std::uint32_t operator_index) const;
    StateLocalAutomaticBatch admit_state_local_automatic_candidates(
        std::uint32_t state_id,
        const AutomaticAdmissionLimits& limits);
    bool is_state_local_automatic_operator(std::uint32_t index) const {
        return state_local_automatic_operator_indices_.contains(index);
    }
    /* The configured slot threshold satisfied at the required rarity. */
    bool is_goal_state(const AbstractState& state) const;

    /* Interning gives stable dense ids; equal states share one id. */
    std::uint32_t intern_state(const AbstractState& state);
    const AbstractState& state(std::uint32_t state_id) const;
    std::uint32_t state_count() const;
    std::uint32_t intern_item(const pc_item_state& item);

    /*
     * Materialize one concrete item consistent with the abstract state
     * (representative materialization). Returns false when no consistent
     * item exists (contradictory flags, unfillable junk counts).
     */
    bool materialize(std::uint32_t state_id, pc_item_state& out_item) const;

    /*
     * Exact successor distribution, cache-first. The result stays valid
     * until the CalcContext is destroyed. Distributions are
     * price-independent by construction; costs live on the descriptor.
     */
    const OutcomeDistribution& outcomes(
        std::uint32_t state_id,
        std::uint32_t action_index,
        bool goal_progress_gated = false);

    /* Complete collision-checked identity used by the exact reforge memo.
     * The action id is included because two actions with the same preserved
     * carrier can still have different direct mods, weights, or guarantees.
     * Returns false when the state cannot be materialized or the action is
     * not a destructive reforge. */
    bool exact_reforge_kernel_signature(
        std::uint32_t state_id,
        std::uint32_t action_index,
        std::vector<std::uint64_t>& out_signature) const;

    /* Exact fixed-program or renewal kernel. operator_index must identify a
     * PlannerOperatorKind::FixedOption entry. */
    const OptionKernel& option_kernel(
        std::uint32_t state_id,
        std::uint32_t operator_index);
    /*
     * Import one context-independent operator. Primitive dependencies are
     * resolved by canonical id in this registry; numeric index parity with
     * the source context is never assumed. Structurally equal operators are
     * interned. `state_local` marks dynamically admitted automatic options.
     */
    std::uint32_t import_planner_operator(
        const PlannerOperator& planner,
        bool state_local);
    bool option_kernel_template_hit(
        std::uint32_t state_id,
        std::uint32_t operator_index) const {
        const std::uint64_t key =
            (static_cast<std::uint64_t>(state_id) << 32) | operator_index;
        return option_kernel_template_hit_keys_.contains(key);
    }

    void reset_solve_telemetry();
    void set_reforge_resource_accounting(const bool enabled);
    bool reforge_resource_accounting() const {
        return reforge_resource_accounting_;
    }
    void set_defer_automatic_protected_baseline(const bool value) {
        defer_automatic_protected_baseline_ = value;
    }
    void set_solve_resource_caps(
        std::uint32_t max_discovered_states,
        std::uint64_t max_reforge_work,
        bool reserve_storage = true,
        std::optional<std::uint64_t> max_owned_bytes = std::nullopt);
    void refresh_solve_owned_bytes_cap(
        std::optional<std::uint64_t> max_owned_bytes);
    void consume_reforge_work(
        std::uint64_t active_amount,
        std::uint64_t logical_v1_amount);
    ReforgeProvenanceCheckpoint begin_reforge_provenance(
        ReforgeRowOwner owner,
        std::optional<ReforgeRowFamily> family_override = std::nullopt);
    void finish_reforge_provenance(
        const ReforgeProvenanceCheckpoint& checkpoint,
        ReforgeRowDisposition disposition);
    void set_reforge_provenance_context(
        ReforgeRowOwner owner,
        std::optional<ReforgeRowFamily> family_override = std::nullopt);
    ReforgeRowOwner reforge_row_owner() const {
        return reforge_row_owner_;
    }
    std::optional<ReforgeRowFamily> reforge_row_family_override() const {
        return reforge_row_family_override_;
    }
    void merge_nested_reforge_telemetry(
        const CalcTelemetry& child,
        const CalcTelemetry* before = nullptr);
    void record_primitive_row_time(
        std::uint32_t action_index,
        std::uint64_t elapsed_ns);
    void release_solve_transition_caches();
    void release_outcome(
        std::uint32_t state_id,
        std::uint32_t action_index,
        bool goal_progress_gated = false);
    /* Release one published primitive row. Policy certification may retain
     * an immutable shared reforge memo after dropping the state-local cache
     * entry so a later exact carrier can reuse already-paid work. */
    void release_published_outcome_storage(
        std::uint32_t state_id,
        std::uint32_t action_index,
        bool goal_progress_gated = false,
        bool retain_stable_shared_kernel = false);
    void release_option_kernel(
        std::uint32_t state_id,
        std::uint32_t operator_index);
    const CalcTelemetry& telemetry() const { return telemetry_; }
    const ActionControlSummary& action_control() const {
        return action_control_;
    }
    std::uint64_t layout_build_ns() const { return layout_build_ns_; }
    std::uint64_t planner_build_ns() const { return planner_build_ns_; }
    std::uint64_t owned_byte_ledger_init_ns() const {
        return owned_byte_ledger_init_ns_;
    }
    std::uint64_t estimated_owned_bytes() const;
    std::uint64_t audited_estimated_owned_bytes() const;
    std::uint64_t fast_estimated_owned_bytes() const;
    std::uint64_t cached_distribution_count() const {
        return distribution_cache_.size();
    }
    std::uint64_t cached_reforge_count() const {
        return reforge_cache_.size();
    }
    const std::shared_ptr<SolveTransitionCache>& solve_transition_cache()
        const {
        return solve_transition_cache_;
    }
    void retain_solve_transition_cache(
        std::shared_ptr<SolveTransitionCache> cache) {
        solve_transition_cache_ = std::move(cache);
    }

  private:
    std::shared_ptr<const SessionImpl> session_;
    GoalSpec goal_;
    AbstractLayout layout_;
    ActionRegistry registry_;
    std::vector<std::uint32_t> candidates_;
    std::vector<PlannerOperator> operators_;
    std::vector<std::uint32_t> candidate_operators_;
    std::size_t static_candidate_operator_count_ = 0;
    std::unordered_map<std::uint32_t, std::vector<std::uint32_t>>
        state_local_automatic_operators_;
    std::unordered_set<std::uint32_t> admitted_automatic_dependencies_;
    std::unordered_set<std::uint32_t>
        state_local_automatic_operator_indices_;
    std::vector<std::uint32_t> automatic_goal_bench_actions_;
    std::vector<TemporaryBenchEffectClass> temporary_bench_effect_classes_;
    std::uint64_t temporary_bench_precompile_ns_ = 0;
    std::uint64_t temporary_bench_precompiled_bytes_ = 0;
    ActionContextImpl context_;
    std::vector<AbstractState> states_;
    std::optional<std::uint32_t> state_cap_;
    std::optional<std::uint32_t> solve_discovered_state_cap_;
    std::optional<std::uint64_t> solve_reforge_work_cap_;
    std::optional<std::uint64_t> solve_owned_bytes_cap_;
    struct StateHashBucket {
        std::uint32_t first = kNoId;
        std::vector<std::uint32_t> collisions;
    };
    std::unordered_map<std::size_t, StateHashBucket> state_ids_by_hash_;
    std::unordered_map<
        std::uint64_t,
        std::shared_ptr<const OutcomeDistribution>> distribution_cache_;
    std::unordered_map<
        std::uint64_t,
        std::shared_ptr<const OptionKernel>> option_kernel_cache_;
    struct OptionKernelTemplateMemo {
        std::uint32_t operator_index = kNoId;
        std::shared_ptr<const OptionKernel> kernel;
        std::vector<std::pair<std::string, double>> expected_resources;
    };
    std::unordered_map<
        std::uint64_t,
        std::vector<OptionKernelTemplateMemo>> option_kernel_templates_;
    std::unordered_map<
        std::uint64_t,
        std::vector<std::shared_ptr<const OptionKernel>>>
        option_transition_templates_;
    std::unordered_map<std::uint64_t, std::vector<std::uint32_t>>
        option_operator_templates_;
    std::unordered_set<std::uint64_t> option_kernel_template_hit_keys_;
    struct ReforgeCacheMemo {
        std::vector<std::uint64_t> observation_signature;
        std::shared_ptr<const OutcomeDistribution> distribution;
    };
    /* Reforges depend only on the preserved base (fractured/locked slots,
     * rarity, item-wide flags), so states sharing one exact observation share
     * one roll DP. Hashes select buckets only; the observation vector is the
     * equality authority. */
    std::map<
        std::tuple<std::uint32_t, std::uint64_t, bool>,
        std::vector<ReforgeCacheMemo>> reforge_cache_;
    mutable CalcTelemetry telemetry_;
    ActionControlSummary action_control_;
    std::unordered_map<std::uint64_t, std::uint8_t> telemetry_rows_;
    std::shared_ptr<SolveTransitionCache> solve_transition_cache_;
    std::unique_ptr<CalcContext> automatic_comparison_context_;
    std::unordered_map<std::string, std::unique_ptr<CalcContext>>
        automatic_admission_contexts_;
    std::uint64_t layout_build_ns_ = 0;
    std::uint64_t planner_build_ns_ = 0;
    std::uint64_t owned_byte_ledger_init_ns_ = 0;
    bool defer_automatic_protected_baseline_ = false;
    std::uint64_t owned_bytes_base_ = 0;
    std::uint64_t owned_bytes_dynamic_shallow_base_ = 0;
    std::uint64_t owned_state_hash_collision_bytes_ = 0;
    std::uint64_t owned_state_local_operator_bytes_ = 0;
    std::uint64_t owned_added_operator_nested_bytes_ = 0;
    std::uint64_t owned_distribution_payload_bytes_ = 0;
    std::unordered_map<const OutcomeDistribution*, std::uint32_t>
        owned_distribution_payload_refs_;
    std::uint64_t owned_option_cache_payload_bytes_ = 0;
    std::uint64_t owned_option_template_nested_bytes_ = 0;
    std::uint64_t owned_transition_template_nested_bytes_ = 0;
    std::uint64_t owned_operator_template_nested_bytes_ = 0;
    std::uint64_t owned_reforge_payload_bytes_ = 0;
    std::uint64_t retained_reforge_distribution_bytes_ = 0;
    bool owned_bytes_ledger_initialized_ = false;
    bool product_solver_parent_ = false;
    bool distinguish_modifier_identity_ = false;
    bool capture_reforge_attribution_ = false;
    bool reforge_resource_accounting_ = true;
    bool use_projected_reforge_frontier_ = false;
    bool reverse_reforge_bucket_enumeration_ = false;
    bool use_factored_terminal_reforge_ = false;
    ReforgeRowOwner reforge_row_owner_ = ReforgeRowOwner::Coarse;
    std::optional<ReforgeRowFamily> reforge_row_family_override_;

    void initialize_temporary_bench_effect_classes();
    void initialize_owned_bytes_ledger();
    std::uint64_t calculate_owned_bytes() const;
    std::uint64_t dynamic_shallow_owned_bytes() const;
    void account_new_operator(const PlannerOperator& value);
    void account_state_local_operators(
        const std::vector<std::uint32_t>& values);
    void account_distribution_cache_insert(
        std::uint64_t key,
        const std::shared_ptr<const OutcomeDistribution>& value);
    void account_distribution_cache_erase(std::uint64_t key);
    void retain_distribution_payload(
        const std::shared_ptr<const OutcomeDistribution>& value);
    void release_distribution_payload(
        const std::shared_ptr<const OutcomeDistribution>& value);
    void account_option_cache_insert(
        std::uint64_t key,
        const std::shared_ptr<const OptionKernel>& value);
    void account_option_cache_erase(std::uint64_t key);
    void account_option_template_insert(
        std::size_t old_capacity,
        const OptionKernelTemplateMemo& value);
    void account_transition_template_insert(
        std::size_t old_capacity,
        const std::shared_ptr<const OptionKernel>& value);
    void account_operator_template_insert(
        std::size_t old_capacity,
        const std::vector<std::uint32_t>& values);
    void account_reforge_cache_insert(
        std::size_t old_capacity, std::size_t new_capacity,
        const ReforgeCacheMemo& value);
    bool can_retain_reforge_distribution(
        const OutcomeDistribution& value) const;
    void require_reforge_scratch_bytes(
        std::uint64_t scratch_bytes) const;

    std::shared_ptr<const OutcomeDistribution> evaluate(
        std::uint32_t state_id,
        std::uint32_t action_index,
        bool goal_progress_gated);
    std::shared_ptr<const OutcomeDistribution> evaluate_reforge(
        std::uint32_t state_id,
        std::uint32_t action_index,
        bool goal_progress_gated);
    std::shared_ptr<const OutcomeDistribution> evaluate_unveil(
        std::uint32_t state_id);
    bool evaluate_pool_add(
        const pc_item_state& item,
        const PoolBuildRequest& base_request,
        std::map<std::uint32_t, double>& accumulated);
};

class ScopedReforgeRowProvenance {
  public:
    ScopedReforgeRowProvenance(
        CalcContext& context,
        ReforgeRowOwner owner,
        std::optional<ReforgeRowFamily> family_override = std::nullopt);
    ~ScopedReforgeRowProvenance();
    ScopedReforgeRowProvenance(const ScopedReforgeRowProvenance&) = delete;
    ScopedReforgeRowProvenance& operator=(
        const ScopedReforgeRowProvenance&) = delete;
    void publish() { disposition_ = ReforgeRowDisposition::Published; }
    void complete() { disposition_ = ReforgeRowDisposition::Completed; }

  private:
    CalcContext& context_;
    ReforgeProvenanceCheckpoint checkpoint_;
    ReforgeRowDisposition disposition_ = ReforgeRowDisposition::Discarded;
};

/*
 * Shared fixed-option execution authority used by policy refinement and
 * strategy compilation. The optional quotient projection is used only for
 * an already-completed policy; an empty projection means exact state
 * identity. Invalid or incomplete choice sidecars are rejected.
 */
bool fixed_option_choice_retries_locally(
    std::uint32_t entry_state,
    const OptionKernel& kernel,
    std::uint32_t successor_state,
    std::uint32_t actual_state,
    const std::vector<std::uint32_t>&
        behavioral_representative_by_state = {});
ExecutableFixedOptionRecipe fixed_option_executable_recipe(
    const CalcContext& calc,
    std::uint32_t entry_state,
    const PlannerOperator& planner,
    const OptionKernel& kernel,
    const std::vector<ObservedUnveilPreference>& preferences,
    const std::vector<std::uint32_t>&
        behavioral_representative_by_state = {});
std::vector<std::uint64_t> fixed_option_executable_recipe_key(
    const ExecutableFixedOptionRecipe& recipe);


} // namespace solver
} // namespace poecraft
