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
 * One source-authoritative destructive-reforge build profile. These samples
 * describe the raw evaluator's actual intermediate representation rather
 * than inferring phase ownership from the final transition count. They are
 * diagnostic only: no solver decision or proof may depend on them.
 */
struct ReforgeBuildAttribution {
    std::string action_id;
    std::uint32_t action_index = kNoId;
    std::uint64_t preserved_base_hash = 0;
    bool goal_progress_gated = false;
    bool projected_sparse_frontier = false;
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
    std::uint64_t raw_choice_table_work = 0;
    std::uint64_t guaranteed_scan_work = 0;
    std::uint64_t frontier_work = 0;
    std::uint64_t raw_identity_tree_work = 0;
    std::uint64_t total_reforge_work = 0;
    std::uint64_t raw_equivalent_reforge_work = 0;
    std::uint64_t projected_reforge_work = 0;
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
    std::uint64_t reforge_frontier_work = 0;
    /* Parallel effort ledgers. V1 raw-equivalent work retains the historic
     * one-node-plus-all-buckets definition. V2 projected work counts one
     * sparse node, availability words inspected, and eligible edges. The
     * public cap continues to apply to reforge_frontier_work, which equals
     * the active evaluator's ledger. */
    std::uint64_t reforge_raw_equivalent_work = 0;
    std::uint64_t reforge_projected_work = 0;
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
        bool use_projected_reforge_frontier = false);

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
    void set_defer_automatic_protected_baseline(const bool value) {
        defer_automatic_protected_baseline_ = value;
    }
    void set_solve_resource_caps(
        std::uint32_t max_discovered_states,
        std::uint64_t max_reforge_work,
        bool reserve_storage = true,
        std::optional<std::uint64_t> max_owned_bytes = std::nullopt);
    void consume_reforge_work(std::uint64_t amount);
    void record_primitive_row_time(
        std::uint32_t action_index,
        std::uint64_t elapsed_ns);
    void release_solve_transition_caches();
    void release_outcome(
        std::uint32_t state_id,
        std::uint32_t action_index,
        bool goal_progress_gated = false);
    /* Release one published primitive row and, when that row is backed by a
     * retained stable reforge memo, release only that memo's payload. Other
     * already-paid transition rows remain available to the solve. */
    void release_published_outcome_storage(
        std::uint32_t state_id,
        std::uint32_t action_index,
        bool goal_progress_gated = false);
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
    bool use_projected_reforge_frontier_ = false;

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
