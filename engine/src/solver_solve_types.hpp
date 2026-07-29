#pragma once

#include "solver_internal.hpp"

#include "poecraft/bitset.h"

#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <deque>
#include <functional>
#include <limits>
#include <numeric>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

/*
 * Solver S4: value iteration over the reachable abstract state set
 * (docs/solver/crafting-solver-plan.md, DP Solver).
 *
 *   V(goal) = 0
 *   V(s)    = min over legal actions a of cost(a) + sum P(s'|s,a) V(s')
 *
 * Restarting is an ordinary action whose successor is the clean base, so
 * it upper-bounds every value and salvage-versus-restart falls out of the
 * minimization. Reforge-class cycles make this value iteration rather
 * than settling; iteration starts from +infinity and converges because
 * the restart bound pulls every state that can reach the goal to a finite
 * value.
 */
namespace poecraft {
namespace solver {

namespace solve_detail {

constexpr double kInfinity = std::numeric_limits<double>::infinity();
/* Finite upper-bound initialization (the restart bound makes every
 * goal-connected value finite; genuinely unreachable states stay here).
 * Value iteration descends monotonically from above, so a plain +infinity
 * start would never lift off: a backup is only finite once every
 * successor is, and no such state exists initially. */
constexpr double kValueCeiling = 1e12;
/* Pivoted elimination becomes cubic and dominates focused endgame solves
 * well before the old 1,024-state cutoff. Medium and large SCCs use the
 * sparse solver below, which accepts values only after its residual check. */
constexpr std::size_t kDensePolicyComponentLimit = 96;

struct PricedOperator {
    std::uint32_t index = kNoId;
    double cost = 0.0;
    std::vector<std::pair<std::string, double>> resource_prices;
};

struct SparseChoiceGroup {
    std::uint64_t successor_offset = 0;
    std::uint32_t successor_count = 0;
    double probability = 0.0;
    bool has_self = false;
};

struct SparseVariant {
    std::uint32_t operator_index = kNoId;
    std::uint64_t quantity_offset = 0;
    std::uint32_t quantity_count = 0;
    std::uint64_t choice_option_offset = 0;
    std::uint32_t choice_option_count = 0;
};

struct CarrierFacts {
    std::uint32_t goal_family_mask = 0;
    std::uint32_t satisfied_goal_mask = 0;
    std::uint32_t blocked_mask = 0;
    std::uint32_t crafted_goal_mask = 0;
    std::uint32_t fractured_goal_mask = 0;
    std::uint32_t active_protection = 0;
    std::uint32_t junk_count = 0;
    std::uint32_t crafted_junk_count = 0;
    std::uint32_t fractured_junk_count = 0;
    std::uint8_t prefix_count = 0;
    std::uint8_t suffix_count = 0;
    std::uint8_t rarity = PC_RARITY_NORMAL;
    std::size_t state_hash = 0;
};

struct CarrierEffectSummary {
    std::uint32_t preserved_properties = 0;
    std::uint32_t destroyed_properties = 0;
    std::uint32_t created_properties = 0;
    std::uint32_t unreachable_properties = 0;
    std::uint32_t preserved_goal_family_mask = 0;
    std::uint32_t destroyed_goal_family_mask = 0;
    std::uint32_t created_goal_family_mask = 0;
    std::uint32_t unreachable_goal_family_mask = 0;
    std::uint32_t preserved_satisfied_goal_mask = 0;
    std::uint32_t destroyed_satisfied_goal_mask = 0;
    std::uint32_t created_satisfied_goal_mask = 0;
    std::uint32_t unreachable_satisfied_goal_mask = 0;
    std::uint32_t preserved_fractured_goal_mask = 0;
    std::uint32_t destroyed_fractured_goal_mask = 0;
    std::uint32_t preserved_crafted_goal_mask = 0;
    std::uint32_t destroyed_crafted_goal_mask = 0;
    std::uint32_t preserved_protection = 0;
    std::uint32_t destroyed_protection = 0;
    std::uint8_t min_prefix_count = 0;
    std::uint8_t max_prefix_count = 0;
    std::uint8_t min_suffix_count = 0;
    std::uint8_t max_suffix_count = 0;
};

struct SparseRow {
    std::uint32_t owner_state = kNoId;
    /*
     * Rows were originally appended as one contiguous block per state.  The
     * incremental action scheduler may admit another row after other states
     * have published theirs, so row ownership is now an index-stable linked
     * chain.  Global row ids and every transition arena offset remain stable.
     */
    std::uint64_t next_owner_row =
        std::numeric_limits<std::uint64_t>::max();
    std::uint64_t variant_offset = 0;
    std::uint32_t variant_count = 0;
    std::uint32_t variant_capacity = 0;
    std::uint64_t transition_offset = 0;
    std::uint32_t transition_count = 0;
    double self_probability = 0.0;
    double embedded_self_probability = 0.0;
    bool self_probability_embedded = false;
    std::uint64_t choice_offset = 0;
    std::uint32_t choice_count = 0;
    CarrierEffectSummary preservation_effect;
    bool admitted = true;
};

struct StateRowSpan {
    std::uint64_t offset = 0;
    std::uint64_t tail = 0;
    std::uint32_t count = 0;
};

struct PendingSparseRow {
    std::uint32_t state = kNoId;
    std::uint32_t operator_index = kNoId;
    const std::vector<std::pair<std::string, double>>* resources = nullptr;
    const std::vector<OutcomeEntry>* transitions = nullptr;
    const std::vector<OutcomeChoiceGroup>* choices = nullptr;
    const std::vector<OutcomeChoiceOption>* choice_options = nullptr;
    const OutcomeDistribution* shared_kernel_identity = nullptr;
    std::optional<std::size_t> exact_kernel_hash;
    bool entry_relative_self = false;
    bool admitted = true;
    bool collapse_equivalent = true;
};

struct PricedSparseRow {
    std::uint32_t operator_index = kNoId;
    double cost = kInfinity;
    std::uint64_t choice_option_offset = 0;
    std::uint32_t choice_option_count = 0;
};

struct PolicyEdge {
    std::uint32_t target = kNoId;
    double probability = 0.0;
};

struct PolicyRow {
    std::uint64_t edge_offset = 0;
    std::uint32_t edge_count = 0;
    double cost = 0.0;
};

struct PolicyTarjanFrame {
    std::uint32_t state = kNoId;
    std::uint32_t next_edge = 0;
};

template <typename T>
std::size_t selected_growth_capacity(
    const std::vector<T>& values, const std::size_t additional);

struct SparseVariantArena {
    std::vector<SparseVariant> variants;
    std::vector<std::uint32_t> row_variant_indices;
    std::vector<double> variant_quantities;

    std::uint64_t selected_bytes() const;
};

constexpr std::uint32_t kProtectionFlags =
    kFlagMultimod | kFlagNoAttack | kFlagNoCaster |
    kFlagPrefixesLocked | kFlagSuffixesLocked;

AutomaticTelemetryKind automatic_telemetry_kind(
    const PlannerOperator& planner);

const char* automatic_telemetry_kind_name(
    const AutomaticTelemetryKind kind);

const char* primitive_telemetry_family_name(
    const PrimitiveTelemetryFamily family);

std::uint32_t compact_count_total(const CompactCountVector& counts);

CarrierFacts carrier_facts(const AbstractState& state);

void classify_slot_mask(
    const std::uint32_t source,
    const std::uint32_t all_successors,
    const std::uint32_t any_successor,
    std::uint32_t& preserved,
    std::uint32_t& destroyed,
    std::uint32_t& created,
    std::uint32_t& unreachable);

struct CarrierSuccessorEnvelope {
    std::uint32_t all_goal = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t any_goal = 0;
    std::uint32_t all_satisfied = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t any_satisfied = 0;
    std::uint32_t all_fractured = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t any_fractured = 0;
    std::uint32_t all_crafted = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t any_crafted = 0;
    std::uint32_t all_protection = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t any_protection = 0;
    std::uint32_t all_blocked = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t any_blocked = 0;
    std::uint32_t min_junk_count = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t max_junk_count = 0;
    std::uint32_t min_crafted_junk_count =
        std::numeric_limits<std::uint32_t>::max();
    std::uint32_t max_crafted_junk_count = 0;
    std::uint32_t min_fractured_junk_count =
        std::numeric_limits<std::uint32_t>::max();
    std::uint32_t max_fractured_junk_count = 0;
    std::uint8_t min_prefix_count =
        std::numeric_limits<std::uint8_t>::max();
    std::uint8_t max_prefix_count = 0;
    std::uint8_t min_suffix_count =
        std::numeric_limits<std::uint8_t>::max();
    std::uint8_t max_suffix_count = 0;
    CompactCountVector junk_counts;
    CompactCountVector crafted_junk_counts;
    CompactCountVector fractured_junk_counts;
    bool junk_counts_uniform = true;
    bool crafted_junk_counts_uniform = true;
    bool fractured_junk_counts_uniform = true;
};

CarrierSuccessorEnvelope carrier_successor_envelope(
    const CalcContext& calc,
    std::vector<std::uint32_t> successor_ids);

CarrierEffectSummary carrier_effect(
    const CalcContext& calc,
    const std::uint32_t source_state,
    const CarrierSuccessorEnvelope& envelope);

CarrierEffectSummary carrier_effect(
    const CalcContext& calc,
    const std::uint32_t source_state,
    std::vector<std::uint32_t> successor_ids);

/* Deterministic double-double arithmetic for the ill-conditioned recurrent
 * policy systems. Every transition coefficient remains its exact stored
 * double; the wider accumulator prevents platform-specific last-bit noise
 * from being amplified into visible native/WASM value drift. */
struct WideFloat {
    double high = 0.0;
    double low = 0.0;

    WideFloat() = default;
    WideFloat(const double value);
    WideFloat(const double high_value, const double low_value);
    bool operator==(const WideFloat&) const = default;
    double value() const;
};

std::pair<double, double> exact_sum(const double a, const double b);

WideFloat wide_normalize(const double high, const double low);

WideFloat operator+(const WideFloat a, const WideFloat b);

WideFloat operator-(const WideFloat value);

WideFloat operator-(const WideFloat a, const WideFloat b);

WideFloat& operator+=(WideFloat& target, const WideFloat value);

WideFloat& operator-=(WideFloat& target, const WideFloat value);

WideFloat operator*(const WideFloat a, const WideFloat b);

WideFloat operator*(const double a, const WideFloat b);

WideFloat operator*(const WideFloat a, const double b);

WideFloat operator/(const WideFloat numerator, const WideFloat denominator);

} // namespace

using namespace solve_detail;

/* A completed reachable closure is independent of the economy. Equivalent
 * kernels retain all operator/resource variants, so a later solve may change
 * relative prices without rebuilding transitions or reusing a stale action
 * representative. */
struct SolveTransitionCache {
    struct AutomaticCandidateRecord {
        std::uint32_t state_id = kNoId;
        std::uint32_t operator_index = kNoId;
        std::string candidate_id;
        AutomaticCandidateKind candidate_kind =
            AutomaticCandidateKind::None;
        std::string setup_action_id;
        std::string followup_action_id;
        std::string cleanup_action_id;
        bool eligible = false;
        bool collapsed = false;
        bool deferred = false;
        bool missing_price = false;
        AutomaticTelemetryKind telemetry_kind =
            AutomaticTelemetryKind::None;
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
        std::uint64_t precompiled_classes = 0;
        std::uint64_t precompile_ns = 0;
        std::uint64_t precompiled_bytes = 0;
        std::uint64_t candidate_variants = 0;
        std::uint64_t effect_classes = 0;
        std::uint64_t collapsed_variants = 0;
        std::uint64_t enumeration_ns = 0;
        std::uint64_t row_ns = 0;
        std::uint64_t selected_bytes = 0;
        std::uint64_t retained_rows = 0;
        std::uint64_t retained_transitions = 0;
        bool count_candidate = true;
        OptionKernel::AutomaticEvidence evidence;
    };
    std::uint32_t start_state = kNoId;
    std::vector<std::uint32_t> operator_indices;
    std::uint32_t max_states = 0;
    std::uint32_t max_discovered_states = 0;
    std::uint32_t max_expanded_states = 0;
    std::uint64_t max_state_action_rows = 0;
    std::uint64_t max_transitions = 0;
    std::uint64_t max_reforge_work = 0;
    std::uint64_t max_solver_owned_bytes = 0;
    std::uint32_t max_diagnostic_samples = 0;
    bool full_evidence = false;
    bool kernel_reuse = true;
    bool goal_progress_gated_reforges = false;
    std::uint32_t discovered_states = 0;
    std::uint32_t expanded_states = 0;
    std::uint32_t strict_discovered_states = 0;
    std::uint32_t quotient_states = 0;
    bool exact_quotient = false;
    std::vector<std::uint32_t> behavioral_representative_by_state;
    std::vector<std::uint8_t> expanded;
    std::vector<StateRowSpan> state_rows;
    std::vector<SparseRow> rows;
    std::shared_ptr<SparseVariantArena> variant_arena =
        std::make_shared<SparseVariantArena>();
    bool accounts_variant_arena = true;
    std::vector<std::uint32_t> successors;
    std::vector<double> probabilities;
    std::vector<SparseChoiceGroup> choices;
    std::vector<std::uint32_t> choice_successors;
    std::vector<OutcomeChoiceOption> choice_options;
    std::uint32_t automatic_rows_considered = 0;
    std::uint32_t automatic_rows_eligible = 0;
    std::uint32_t automatic_rows_rejected = 0;
    std::uint32_t automatic_rows_collapsed = 0;
    std::uint32_t automatic_rows_deferred = 0;
    std::array<AutomaticKindTelemetry, kAutomaticTelemetryKindCount>
        automatic_kind_telemetry{};
    AutomaticAdmissionPhaseTelemetry automatic_admission_phases;
    std::vector<AutomaticCandidateRecord> automatic_candidate_samples;
    std::uint64_t owned_automatic_sample_nested_bytes = 0;
    std::uint64_t algebraic_self_loops = 0;
    bool focused_partial = false;

    bool compatible(
        const std::uint32_t requested_start,
        const std::vector<PricedOperator>& priced,
        const SolveOptions& options) const;

    static std::uint64_t automatic_sample_nested_bytes(
        const AutomaticCandidateRecord& record);

    void retain_automatic_sample(AutomaticCandidateRecord record);

    std::uint64_t shallow_estimated_owned_bytes() const;

    std::uint64_t fast_estimated_owned_bytes() const;

    std::uint64_t audited_estimated_owned_bytes() const;

    std::uint64_t estimated_owned_bytes() const;
};

/*
 * Iterate the stable global row ids owned by one state.  The first rows of a
 * freshly expanded state are normally adjacent, but late incremental actions
 * are not required to be contiguous.
 */
struct StateRowIndexIterator {
    const std::vector<SparseRow>* rows = nullptr;
    std::uint64_t current = 0;
    std::uint32_t remaining = 0;

    std::uint64_t operator*() const { return current; }
    bool operator!=(const StateRowIndexIterator& other) const {
        return remaining != other.remaining;
    }
    StateRowIndexIterator& operator++() {
        if (remaining != 0) {
            --remaining;
            if (remaining != 0) {
                current = rows->at(current).next_owner_row;
            }
        }
        return *this;
    }
};

struct StateRowIndexRange {
    const std::vector<SparseRow>* rows = nullptr;
    StateRowSpan span;

    StateRowIndexIterator begin() const {
        return {rows, span.offset, span.count};
    }
    StateRowIndexIterator end() const {
        return {rows, 0, 0};
    }
};

inline StateRowIndexRange state_row_indices(
        const SolveTransitionCache& cache,
        const std::uint32_t state) {
    return {&cache.rows, cache.state_rows.at(state)};
}

namespace solve_detail {

struct CapturedBoundedPolicyRow {
    PolicyOperatorRef policy;
    double cost = kInfinity;
    std::vector<OutcomeChoiceOption> choice_options;
};

CapturedBoundedPolicyRow capture_bounded_policy_row(
    const CalcContext& calc,
    const SolveTransitionCache& transition_cache,
    const std::vector<PricedSparseRow>& priced_rows,
    std::uint32_t state,
    std::uint64_t row,
    std::uint32_t fallback_operator);

std::uint64_t string_vector_owned_bytes(
    const std::vector<std::string>& values);

std::uint64_t diagnostics_owned_bytes(const SolveDiagnostics& diagnostics);

std::uint64_t solve_result_owned_bytes(const SolveResult& result);

} // namespace

struct SolveWork::Impl {
    CalcContext& calc;
    const SessionImpl& session;
    SolveOptions options;
    std::unordered_map<std::string, double> prices;
    SolveResult result;
    std::vector<PricedOperator> operators;
    std::vector<std::uint32_t> static_operator_indices;
    std::vector<std::uint32_t> delayed_operator_indices;
    std::vector<std::uint32_t> expansion_operator_indices;
    std::vector<bool> reported_unsupported;
    std::vector<std::uint8_t> expanded;
    std::vector<std::uint8_t> queued;
    std::deque<std::uint32_t> queue;
    std::uint32_t expanded_count = 0;
    bool expansion_active = false;
    bool expansion_prepared = false;
    std::uint32_t expansion_state = kNoId;
    std::uint32_t expansion_operator_cursor = 0;
    bool expansion_is_incremental_alternative = false;
    bool expansion_incremental_resource_limited = false;
    std::uint64_t expansion_appended_row =
        std::numeric_limits<std::uint64_t>::max();
    bool incremental_action_generation = false;
    bool incremental_envelope_closed = false;
    bool incremental_restricted_values_ready = false;
    std::vector<std::uint32_t> incremental_carriers;
    std::size_t incremental_carrier_cursor = 0;
    std::size_t incremental_operator_cursor = 0;
    bool incremental_dynamic_prepared = false;
    std::size_t incremental_dynamic_operator_cursor = 0;
    std::vector<std::uint32_t> incremental_dynamic_operator_indices;
    struct IncrementalAlternativeRow {
        enum class Status : std::uint8_t {
            PendingValues,
            Admitted,
            NonImproving,
            Unresolved,
        };
        std::uint32_t state = kNoId;
        std::uint32_t operator_index = kNoId;
        std::uint64_t row_index =
            std::numeric_limits<std::uint64_t>::max();
        Status status = Status::PendingValues;
        double lower_q = kInfinity;
        double upper_q = kInfinity;
        double improvement_margin = 0.0;
        std::uint32_t states_added = 0;
    };
    std::vector<IncrementalAlternativeRow> incremental_alternative_rows;
    std::uint64_t incremental_unevaluated_actions = 0;
    std::uint64_t incremental_inapplicable_actions = 0;
    std::uint64_t incremental_resource_unresolved_actions = 0;
    std::uint64_t incremental_unique_kernel_evaluations = 0;
    std::uint64_t incremental_carrier_kernel_reuses = 0;
    std::uint64_t incremental_reoptimizations = 0;
    std::uint32_t incremental_first_alternative_expanded_states = 0;
    bool incremental_refinement_active = false;
    std::uint32_t incremental_refinement_target_expanded = 0;
    std::uint64_t incremental_refinement_rounds = 0;
    std::uint64_t incremental_refinement_states_selected = 0;
    std::uint64_t incremental_rows_reconsidered = 0;
    std::uint64_t incremental_upper_policy_updates = 0;
    double incremental_refinement_uncertainty = 0.0;
    std::uint32_t expansion_states_outside_chaos_support = 0;
    std::vector<std::uint8_t> incremental_chaos_support;
    std::vector<std::uint8_t> incremental_nonchaos_states_seen;
    std::uint32_t peak_queue_size = 0;
    std::uint32_t sweeps = 0;
    double residual = kValueCeiling;
    std::shared_ptr<SolveTransitionCache> transition_cache;
    std::vector<PricedSparseRow> priced_rows;
    std::size_t pricing_diagnostics_cursor = 0;
    std::vector<std::int32_t> priced_operator_position;
    std::uint32_t restart_operator_index = kNoId;
    std::uint32_t restart_state = kNoId;
    double restart_cost = kInfinity;
    struct KernelRowMemo {
        std::uint64_t row_index = 0;
        std::optional<CarrierSuccessorEnvelope> successor_envelope;
    };
    std::unordered_map<std::size_t, std::vector<KernelRowMemo>>
        kernel_rows_by_hash;
    struct SharedKernelMemo {
        std::uint64_t row_index = 0;
        bool fringe_enqueued = false;
        std::optional<CarrierSuccessorEnvelope> successor_envelope;
    };
    std::unordered_map<const OutcomeDistribution*, SharedKernelMemo>
        shared_kernel_rows;
    std::unordered_set<std::uint64_t> automatic_admission_records;
    struct AutomaticCarrierWork {
        std::uint64_t candidates = 0;
        std::uint64_t candidate_variants = 0;
        std::uint64_t effect_classes = 0;
        std::uint64_t templates = 0;
        std::uint64_t rows = 0;
    };
    std::unordered_map<std::uint64_t, AutomaticCarrierWork>
        automatic_carrier_work;
    enum class BackupStage : std::uint8_t { Measure, Apply };
    BackupStage backup_stage = BackupStage::Measure;
    std::uint32_t backup_cursor = 0;
    double measured_residual = kValueCeiling;
    std::vector<std::pair<double, std::uint32_t>> prioritized_states;
    bool backup_active = false;
    bool cache_pending = false;
    std::vector<std::uint64_t> policy_rows;
    bool policy_initialized = false;
    bool policy_stable = false;
    bool policy_iteration_failed = false;
    bool policy_evaluation_incomplete = false;
    struct SparsePolicyResume {
        std::vector<std::uint32_t> members;
        std::vector<WideFloat> b;
        std::vector<WideFloat> x;
        std::vector<WideFloat> r;
        std::vector<WideFloat> r0;
        std::vector<WideFloat> p;
        std::vector<WideFloat> v;
        std::vector<WideFloat> s;
        std::vector<WideFloat> t;
        WideFloat rho_previous = 1.0;
        WideFloat alpha = 1.0;
        WideFloat omega = 1.0;
        std::uint32_t iterations = 0;
        std::uint32_t refinement_count = 0;
    };
    std::unique_ptr<SparsePolicyResume> sparse_policy_resume;
    struct SharedPolicyKernelRepresentative {
        std::uint32_t state = kNoId;
        std::vector<std::uint64_t> exact_signature;
    };
    struct PolicyKernelPreparation {
        std::size_t state_count = 0;
        std::vector<std::uint32_t> active_states;
        std::uint32_t cursor = 0;
        std::vector<std::uint32_t> kernel_owner;
        std::vector<std::vector<PolicyEdge>> full_kernel;
        std::unordered_map<std::size_t, std::vector<std::uint32_t>>
            representatives_by_hash;
        std::unordered_map<
            std::size_t, std::vector<SharedPolicyKernelRepresentative>>
            shared_transition_representatives;
        std::vector<std::uint32_t> representative;
        std::vector<std::vector<std::uint32_t>> group_members;
        std::vector<PolicyRow> rows;
        std::vector<PolicyEdge> edges;
        std::uint32_t grouping_cursor = 0;
        std::uint32_t quotient_cursor = 0;
        bool source_kernels_released = false;
        bool components_ready = false;
        std::vector<std::vector<std::uint32_t>> components;
        std::vector<std::uint32_t> component_by_state;
        std::vector<std::int32_t> local;
        std::uint32_t component_cursor = 0;
        std::vector<std::uint32_t> tarjan_index;
        std::vector<std::uint32_t> tarjan_lowlink;
        std::vector<std::uint8_t> tarjan_on_stack;
        std::vector<std::uint32_t> tarjan_stack;
        std::vector<PolicyTarjanFrame> tarjan_dfs;
        std::uint32_t tarjan_next_index = 0;
        std::uint32_t tarjan_root_cursor = 0;
    };
    std::unique_ptr<PolicyKernelPreparation> policy_kernel_preparation;
    enum class PolicyUnitStage : std::uint8_t {
        Seed,
        InitialSelect,
        Evaluate,
        ImproveSelect,
    };
    PolicyUnitStage policy_unit_stage = PolicyUnitStage::Seed;
    std::uint32_t policy_seed_pass = 0;
    std::uint32_t policy_seed_cursor = 0;
    std::vector<std::uint32_t> policy_seed_states;
    bool policy_selection_active = false;
    std::uint32_t policy_selection_cursor = 0;
    std::vector<std::uint32_t> policy_selection_states;
    bool policy_selection_improved = false;
    double policy_selection_residual = 0.0;
    std::uint64_t peak_policy_scratch_bytes = 0;
    std::uint64_t current_policy_scratch_bytes = 0;
    std::uint64_t owned_prices_nested_bytes = 0;
    std::uint64_t owned_operators_nested_bytes = 0;
    std::uint64_t owned_kernel_row_bucket_bytes = 0;
    std::uint64_t owned_kernel_value_cache_nested_bytes = 0;
    std::uint64_t owned_result_nested_bytes = 0;
    mutable std::uint64_t owned_byte_ledger_requests = 0;
    mutable std::uint64_t owned_byte_reconciliations = 0;
    mutable std::uint64_t owned_byte_ledger_max_overestimate = 0;
    std::vector<std::uint32_t> improper_policy_states;
    struct KernelValueCache {
        std::uint64_t transition_offset = 0;
        std::uint32_t transition_count = 0;
        double finite_sum = 0.0;
        std::uint32_t infinite_count = 0;
        bool sorted_successors = true;
    };
    bool kernel_value_cache_active = false;
    std::vector<KernelValueCache> kernel_value_caches;
    std::unordered_map<std::uint64_t, std::size_t> kernel_value_cache_by_offset;
    bool focused_mode = false;
    bool focus_optimizing = false;
    bool focused_lower_mode = false;
    bool focused_upper_mode = false;
    bool focused_closure_proved = false;
    bool focused_bound_proved = false;
    bool full_closure_after_focused_fallback = false;
    struct FocusedFallbackPolicy {
        struct PrimitiveRenewalMode {
            double value = kInfinity;
            std::uint32_t operator_index = kNoId;
            std::vector<std::uint64_t> kernel_signature;
        };
        std::uint32_t anchor_state = kNoId;
        double anchor_state_value = kInfinity;
        std::uint64_t anchor_row =
            std::numeric_limits<std::uint64_t>::max();
        std::uint32_t anchor_operator = kNoId;
        double renewal_state_value = kInfinity;
        std::uint64_t renewal_row =
            std::numeric_limits<std::uint64_t>::max();
        std::uint32_t renewal_operator = kNoId;
        std::uint32_t finish_action = kNoId;
        std::uint8_t renewal_rarity = PC_RARITY_NORMAL;
        std::uint8_t renewal_influence_bits = 0;
        std::uint8_t renewal_searing_exarch_tier = 0;
        std::uint8_t renewal_eater_of_worlds_tier = 0;
        /* Primitive destructive renewal is admitted only when every retry
         * carrier reproduces this complete engine-owned reforge signature.
         * Fixed options retain their own exact OptionKernel retry witness. */
        std::vector<std::uint64_t> renewal_kernel_signature;
        std::vector<PrimitiveRenewalMode> primitive_renewal_modes;
        /* Exact policy-selected magic acquisition -> Regal -> deterministic
         * finish terminals. These are ordinary primitive operators on strict
         * states; the map is an executable fallback witness, not a quotient. */
        std::unordered_map<std::uint32_t, double> progress_state_value;
        std::unordered_map<std::uint32_t, std::uint32_t>
            progress_state_operator;
        /* A fallback may be retained across monotonic focused-graph growth
         * only as an executable upper-bound/output witness. These immutable
         * identities and the referenced row/operator checks below are its
         * refresh boundary; none of them is search guidance. */
        std::uint64_t goal_identity = 0;
        std::uint64_t economy_identity = 0;
        std::uint64_t action_vocabulary_identity = 0;
        std::uint32_t action_vocabulary_size = 0;
        std::uint64_t synthesis_graph_identity = 0;
    };
    using FocusedFallbackWitness =
        std::shared_ptr<const FocusedFallbackPolicy>;
    FocusedFallbackWitness focused_fallback_policy;
    /*
     * A successful validation may outlive later focused-graph appends, but
     * never a change to the prefix on which it was proved. Sparse rows and
     * transition payloads are append-only after publication; the stored
     * counts delimit that immutable prefix. Policy ownership is exact
     * (shared_ptr identity), while the hashes cover every value read by the
     * validation and make accidental in-place mutation invalidate reuse.
     */
    struct SuccessfulFallbackPropernessProof {
        static constexpr std::uint64_t kVersion = 1;
        std::uint64_t version = kVersion;
        FocusedFallbackWitness policy;
        const SolveTransitionCache* graph = nullptr;
        const SessionImpl* mechanics = nullptr;
        std::uint64_t goal_identity = 0;
        std::uint64_t economy_identity = 0;
        std::uint64_t action_vocabulary_identity = 0;
        std::uint64_t policy_identity = 0;
        std::uint64_t graph_prefix_identity = 0;
        std::uint64_t transition_prefix_identity = 0;
        std::uint32_t action_vocabulary_size = 0;
        std::uint64_t row_count = 0;
        std::uint64_t priced_row_count = 0;
        std::uint64_t successor_count = 0;
        std::uint64_t probability_count = 0;
        std::uint64_t choice_count = 0;
        std::uint64_t choice_successor_count = 0;
        std::uint64_t choice_option_count = 0;
    };
    std::optional<SuccessfulFallbackPropernessProof>
        successful_fallback_properness_proof;
    struct BoundedPolicyIncumbent {
        struct ChoiceSource {
            std::uint32_t state = kNoId;
            std::vector<OutcomeChoiceOption> choices;
        };
        double certified_upper_bound = kInfinity;
        double evaluated_policy_cost = kInfinity;
        std::vector<double> values;
        std::vector<std::uint64_t> policy_rows;
        std::vector<double> policy_row_costs;
        std::vector<PolicyOperatorRef> policy;
        std::vector<ChoiceSource> choice_sources;
        std::vector<std::vector<std::uint32_t>> unveil_preferences;
        std::vector<std::vector<ObservedUnveilPreference>>
            option_unveil_preferences;
        std::vector<std::uint32_t> frontier_operators;
        FocusedFallbackWitness fallback;
        std::vector<std::uint32_t> behavioral_representative_by_state;
        std::vector<std::uint8_t> policy_reachable;
        PrimitiveRenewalWitness primitive_renewal_witness;
        std::uint32_t restart_operator = kNoId;
        std::uint32_t restart_state = kNoId;
        std::uint32_t fallback_anchor_state = kNoId;
        std::uint32_t round = 0;
        std::string kind;
        std::uint64_t goal_identity = 0;
        std::uint64_t economy_identity = 0;
        std::uint64_t action_vocabulary_identity = 0;
        std::uint64_t graph_identity = 0;
        bool strict_state_provenance = true;
        bool policy_materialized = false;
    };
    std::optional<BoundedPolicyIncumbent> output_incumbent;
    bool target_gap_stop = false;
    SolveGapTarget target_gap_fired = SolveGapTarget::None;
    std::uint64_t focused_direct_upper_row =
        std::numeric_limits<std::uint64_t>::max();
    std::vector<double> focused_previous_upper_values;
    std::vector<std::uint64_t> focused_previous_upper_policy_rows;
    std::vector<std::uint32_t> focused_frontier_upper_operator;
    std::vector<std::uint32_t> focused_previous_frontier_upper_operator;
    std::vector<double> focused_round_lower_values;
    std::vector<std::uint64_t> focused_round_lower_policy_rows;
    std::vector<std::uint32_t> focused_pending_lower_fringe;
    std::vector<std::uint32_t> focused_pending_upper_fringe;
    std::vector<double> focused_pending_upper_priority;
    bool focused_pending_upper_complete = false;
    double focused_partial_upper_bound = kInfinity;
    std::shared_ptr<SolveTransitionCache> focused_strict_transition_cache;
    std::vector<std::uint8_t> focused_strict_expanded;
    std::vector<std::uint32_t> focused_behavioral_representative;
    std::uint32_t focused_strict_expanded_count = 0;
    std::uint32_t next_focus_checkpoint = 32;
    /* Exact price-bound state pruning. A constructive row supplies an
     * executable upper bound; the cover table supplies an optimistic lower
     * bound for every other admitted operator. No graph using this proof is
     * retained as the price-independent transition cache. */
    std::vector<std::uint32_t> operator_goal_reach_mask;
    std::vector<std::uint8_t> operator_goal_reach_computed;
    std::vector<double> goal_cover_cost;
    std::vector<double> clean_goal_cover_cost;
    /* Final one-step lower value of every non-refined action in the clean
     * goal-progress relaxation. The strict normal/magic pattern database
     * uses this as an optimistic escape while evaluating the productive
     * currency actions against their exact blocker identities. */
    std::vector<double> clean_goal_escape_cost;
    std::vector<std::uint32_t> clean_goal_escape_action;
    std::vector<double> clean_goal_no_exalt_escape_cost;
    std::vector<std::uint32_t> clean_goal_no_exalt_escape_action;
    std::vector<double> strict_clean_goal_cover_cost;
    std::uint32_t strict_clean_goal_cover_state_count = 0;
    bool strict_clean_goal_cover_refresh_needed = false;
    bool goal_cover_cost_ready = false;
    bool price_bound_state_pruning = false;
    std::vector<double> certified_state_upper;
    std::vector<std::uint64_t> certified_state_row;
    std::uint64_t peak_owned_bytes = 0;
    SolvePhase phase = SolvePhase::Expanding;
    bool consumed = false;

    static std::uint64_t priced_operator_nested_bytes(
        const PricedOperator& priced);

    void initialize_owned_bytes_ledger();

    void retain_action_reason(std::string reason);

    Impl(
        CalcContext& context,
        const pc_item_state& start_item,
        const std::unordered_map<std::string, double>& prices,
        const SolveOptions& solve_options);

    static void identity_mix(
        std::uint64_t& hash, const std::uint64_t value);

    static void identity_mix_string(
        std::uint64_t& hash, const std::string_view value);

    std::uint64_t goal_identity() const;

    std::uint64_t economy_identity() const;

    std::uint64_t action_vocabulary_prefix_identity(
        const std::size_t count) const;

    std::uint64_t action_vocabulary_identity() const;

    std::uint64_t graph_identity() const;
    std::uint64_t fallback_policy_identity(
        const FocusedFallbackPolicy& fallback) const;
    std::uint64_t fallback_graph_prefix_identity(
        std::uint64_t row_count,
        std::uint64_t priced_row_count) const;
    std::uint64_t fallback_transition_prefix_identity(
        std::uint64_t successor_count,
        std::uint64_t probability_count,
        std::uint64_t choice_count,
        std::uint64_t choice_successor_count,
        std::uint64_t choice_option_count) const;
    bool reuse_successful_fallback_properness_proof(
        const FocusedFallbackPolicy& fallback);
    void remember_successful_fallback_properness_proof(
        const FocusedFallbackWitness& fallback);

    /* Existing exact closure uses an absolute numerical tolerance. Product
     * gap targets are separate and must never relax this proof. */
    double exact_gap_proof_tolerance() const;

    /* Some pre-existing value comparisons scale roundoff by the value's
     * magnitude. Keep that behavior named separately from exact closure and
     * from product stopping targets. */
    double value_comparison_tolerance(const double value) const;

    void stamp_fallback_provenance(FocusedFallbackPolicy& fallback) const;

    const char* retained_fallback_invalid_reason(
        const FocusedFallbackPolicy& fallback);

    FocusedFallbackWitness acquire_focused_fallback();

    void capture_incumbent_policy(BoundedPolicyIncumbent& candidate);

    void capture_incumbent_state(
        BoundedPolicyIncumbent& candidate,
        std::uint32_t state,
        std::uint64_t row);

    void populate_incumbent_policy(BoundedPolicyIncumbent& candidate);

    void commit_output_incumbent(BoundedPolicyIncumbent candidate);

    void install_output_incumbent(
        const double upper,
        const std::vector<double>& values,
        const std::vector<std::uint64_t>& selected_rows,
        const std::vector<std::uint32_t>& frontier_operators,
        const FocusedFallbackWitness& fallback,
        std::string kind,
        const std::vector<std::uint8_t>* policy_reachable = nullptr,
        const PrimitiveRenewalWitness* primitive_renewal_witness = nullptr,
        bool replace_equal_incumbent = false);

    void install_fallback_output_incumbent(
        const FocusedFallbackWitness& witness);

    void install_direct_output_incumbent(
        const double upper, const std::uint64_t row);

    void try_install_gated_root_renewal_incumbent(
        std::uint32_t state,
        std::uint64_t row,
        const PricedOperator& priced,
        const OutcomeDistribution& kernel);

    SolveGapTarget satisfied_gap_target() const;

    bool stop_for_satisfied_gap_target();

    bool ensure_priced_operator(const std::uint32_t index);

    std::uint32_t action_goal_reach_mask(
        const std::uint32_t action_index) const;

    std::uint32_t planner_goal_reach_mask(
        const std::uint32_t operator_index);

    void prepare_goal_cover_cost();

    std::uint32_t satisfied_goal_mask_for_state(
        const std::uint32_t state) const;

    double optimistic_completion_cost(
        const std::uint32_t satisfied_mask,
        const bool clean_carrier = false,
        const std::uint8_t carrier_rarity = PC_RARITY_NORMAL,
        const std::uint8_t carrier_prefixes = 0,
        const std::uint8_t carrier_suffixes = 0);

    bool clean_goal_cover_eligible(const std::uint32_t state) const;

    double optimistic_completion_cost_for_state(
        const std::uint32_t state);

    void prepare_strict_clean_goal_cover();

    double optimistic_operator_lower(
        const std::uint32_t state,
        const std::uint32_t operator_index);

    std::optional<double> constructive_row_upper(
        const std::uint32_t state,
        const std::uint64_t row_index);

    bool try_constructive_state_certificate(
        const std::uint32_t state,
        const std::uint64_t row_index);

    SolveTransitionCache::AutomaticCandidateRecord automatic_record_from(
        const std::uint32_t state,
        const StateLocalAutomaticCandidate& decision) const;

    void prepare_state_expansion(
        const std::uint32_t state,
        bool include_state_local_automatic = true);

    double acceptable_residual() const;

    bool optimization_converged() const;

    void enqueue(const std::uint32_t state);

    void enqueue_front(const std::uint32_t state);

    bool same_kernel(
        const SparseRow& stored,
        const PendingSparseRow& pending,
        const std::size_t transition_count,
        const double self_probability) const;

    std::size_t kernel_hash(const PendingSparseRow& pending) const;

    void add_action_reason(
        const char* disposition,
        const std::string& action,
        const std::string& reason);

    void record_skipped_missing_price(const std::string& action);

    void record_skipped_unsupported(const std::string& action);

    enum class PreservationDisposition : std::uint8_t {
        NotApplicable,
        RetainedDisposable,
        RetainedPreserving,
        RetainedUncertain,
        PrunedByRestartBound,
    };

    struct PreservationDecision {
        PreservationDisposition disposition =
            PreservationDisposition::NotApplicable;
        std::uint32_t destroyed_progress = 0;
        double candidate_lower_bound = 0.0;
        double restart_upper_bound = kInfinity;
    };

    PreservationDecision preservation_decision(
        const std::uint64_t row_index) const;

    bool preservation_prunes(const std::uint64_t row_index) const;

    static void append_json_string(
        std::string& out,
        const std::string& value);

    static std::string finite_json(const double value);

    static std::string property_mask_json(const std::uint32_t mask);

    static std::string count_vector_json(const CompactCountVector& counts);

    std::string preservation_witness_json(
        const std::uint64_t row_index,
        const PreservationDecision& decision) const;

    static const char* automatic_kind_name(
        const AutomaticCandidateKind kind);

    static std::string automatic_mechanisms_json(
        const std::uint32_t mechanisms);

    std::string automatic_candidate_witness_json(
        const SolveTransitionCache::AutomaticCandidateRecord& record,
        const char* disposition,
        const char* decision_reason) const;

    void retain_automatic_candidate_record(
        SolveTransitionCache::AutomaticCandidateRecord record);

    void finalize_automatic_candidate_diagnostics();

    void finalize_preservation_diagnostics();

    void record_cap(const std::string& name, bool state_cap = false);

    bool check_solver_byte_cap_from(
        const std::uint64_t current,
        const std::uint64_t transient_bytes = 0);

    bool check_solver_byte_cap(const std::uint64_t transient_bytes = 0);

    bool check_solver_byte_cap_fast(
        const std::uint64_t transient_bytes = 0);

    template <typename T>
    void reserve_selected_growth(
        std::vector<T>& values, const std::size_t additional);

    std::pair<bool, std::uint64_t> append_sparse_row(
        const std::uint32_t state,
        PendingSparseRow pending);

    bool expand_one_unit();

    bool incremental_alternative_type(
        const std::uint32_t operator_index) const;

    void retain_incremental_carrier(const std::uint32_t state);

    bool schedule_next_incremental_alternative();

    bool classify_incremental_alternatives();

    double sparse_row_q_for_values(
        std::size_t row_index,
        const std::vector<double>& values) const;

    std::vector<double> certified_incremental_lower_values();

    bool schedule_incremental_refinement(bool force = false);

    void refresh_incremental_upper_incumbent();

    void restart_incremental_optimization();

    void finalize_incremental_diagnostics();

    bool priced_variant_cost(
        const SparseVariant& variant,
        double& cost) const;

    void update_priced_row(const std::size_t row_index);

    void prepare_priced_rows();

    static void signature_string(
        std::vector<std::uint64_t>& out,
        const std::string& value);

    void planner_observation_signature(
        std::vector<std::uint64_t>& out,
        const PlannerOperator& planner) const;

    struct RowObservationRepresentative {
        std::uint32_t row_index = kNoId;
        std::uint32_t class_id = kNoId;
    };
    struct KernelProjectionMemo {
        std::array<std::uint64_t, 6> exact_key{};
        std::uint32_t class_id = kNoId;
    };
    struct KernelProjectionRepresentative {
        std::array<std::uint64_t, 6> exact_key{};
        std::uint32_t class_id = kNoId;
    };
    struct RowObservationCache {
        std::unordered_map<
            std::uint64_t, std::vector<RowObservationRepresentative>>
            exact_key_buckets;
        std::unordered_map<
            std::uint64_t, std::vector<RowObservationRepresentative>>
            behavior_buckets;
        std::uint32_t next_class_id = 0;
        std::unordered_map<
            std::uint64_t, std::vector<KernelProjectionMemo>>
            kernel_projection_buckets;
        std::unordered_map<
            std::uint64_t, std::vector<KernelProjectionRepresentative>>
            kernel_projection_behavior_buckets;
        std::uint32_t next_kernel_projection_class_id = 0;
        std::vector<WideFloat> transition_sums;
        std::vector<std::uint32_t> transition_epochs;
        std::vector<std::uint32_t> touched_classes;
        std::vector<WideFloat> secondary_transition_sums;
        std::vector<std::uint32_t> secondary_transition_epochs;
        std::vector<std::uint32_t> secondary_touched_classes;
        std::vector<std::uint32_t> radix_scratch;
        std::array<std::uint32_t, 65536> radix_counts{};
        std::uint32_t transition_epoch = 0;
        std::uint32_t secondary_transition_epoch = 0;
    };

    std::vector<std::uint64_t> row_observation_cache_key(
        const SolveTransitionCache& graph,
        const SparseRow& row,
        const std::vector<std::uint32_t>& partition) const;

    void sort_projection_classes(
        std::vector<std::uint32_t>& classes,
        RowObservationCache& cache) const;

    void fill_kernel_projection(
        const SolveTransitionCache& graph,
        const std::array<std::uint64_t, 6>& key,
        const std::vector<std::uint32_t>& partition,
        RowObservationCache& cache,
        const bool secondary) const;

    std::vector<std::uint32_t> projected_choice_classes(
        const SolveTransitionCache& graph,
        const SparseChoiceGroup& group,
        const std::vector<std::uint32_t>& partition) const;

    std::uint64_t kernel_projection_hash(
        const SolveTransitionCache& graph,
        const std::array<std::uint64_t, 6>& key,
        const std::vector<std::uint32_t>& partition,
        RowObservationCache& cache) const;

    bool same_kernel_projection(
        const SolveTransitionCache& graph,
        const std::array<std::uint64_t, 6>& current,
        const std::array<std::uint64_t, 6>& candidate,
        const std::vector<std::uint32_t>& partition,
        RowObservationCache& cache) const;

    std::uint32_t intern_kernel_projection(
        const SolveTransitionCache& graph,
        const std::array<std::uint64_t, 6>& key,
        const std::vector<std::uint32_t>& partition,
        RowObservationCache& cache) const;

    std::vector<std::uint64_t> row_behavior_signature(
        const SolveTransitionCache& graph,
        const SparseRow& row,
        const std::vector<std::uint32_t>& partition,
        RowObservationCache& cache,
        const std::vector<std::uint64_t>& exact_row_key) const;

    std::uint32_t intern_row_behavior(
        const SolveTransitionCache& graph,
        const std::uint32_t row_index,
        const std::vector<std::uint32_t>& partition,
        RowObservationCache& cache) const;

    std::vector<std::uint64_t> state_behavior_signature(
        const SolveTransitionCache& graph,
        const std::uint32_t state,
        const std::vector<std::uint32_t>& partition,
        RowObservationCache* cache) const;

    std::vector<std::uint64_t> coarse_state_signature(
        const std::uint32_t state_id) const;

    std::vector<std::uint64_t> focused_schedule_signature(
        const std::uint32_t state_id);

    static std::uint64_t observation_signature_hash(
        const std::vector<std::uint64_t>& signature);

    template <typename SignatureBuilder>
    std::pair<std::vector<std::uint32_t>, std::uint32_t> exact_partition(
        const std::uint32_t state_count,
        SignatureBuilder signature_for) const {
        struct Representative {
            std::uint32_t state = kNoId;
            std::uint32_t class_id = kNoId;
            std::vector<std::uint64_t> exact_signature;
        };
        std::unordered_map<std::uint64_t, std::vector<Representative>> buckets;
        std::vector<std::uint32_t> partition(state_count, kNoId);
        std::uint32_t class_count = 0;
        for (std::uint32_t state = 0; state < state_count; ++state) {
            std::vector<std::uint64_t> signature = signature_for(state);
            const std::uint64_t hash = observation_signature_hash(signature);
            auto& candidates = buckets[hash];
            std::uint32_t selected = kNoId;
            for (Representative& candidate : candidates) {
                if (candidate.exact_signature.empty()) {
                    candidate.exact_signature = signature_for(candidate.state);
                }
                if (candidate.exact_signature == signature) {
                    selected = candidate.class_id;
                    break;
                }
            }
            if (selected == kNoId) {
                selected = class_count++;
                candidates.push_back({state, selected, {}});
            }
            partition[state] = selected;
        }
        return {std::move(partition), class_count};
    }

    std::string first_equivalence_witness(
        const SolveTransitionCache& graph,
        const std::uint32_t left,
        const std::uint32_t right) const;

    void collect_action_observation_cardinalities(
        const SolveTransitionCache& graph);

    std::vector<std::uint64_t> shadow_state_signature(
        const SolveTransitionCache& graph,
        const std::uint32_t state) const;

    void build_quotient_graph(
        const std::vector<std::uint32_t>& partition,
        const std::uint32_t class_count);

    void prepare_focused_exact_quotient();

    void prepare_exact_outer_quotient();

    void prepare_iteration();

    double operator_q(
        const std::uint32_t state,
        const PricedOperator& priced);

    void reset_kernel_value_cache(bool active = false);

    KernelValueCache& value_cache_for(const SparseRow& row);

    void update_kernel_value_cache(
        const std::uint32_t state,
        const double before,
        const double after);

    double sparse_row_q(
        const std::size_t row_index,
        std::uint32_t& transition_work);

    void begin_policy_selection();

    bool initialize_focused_proper_policy();

    bool advance_policy_selection(bool& improved);

    void reset_policy_iteration_units();

    bool evaluate_fixed_policy();

    bool repair_improper_policy();

    bool run_policy_iteration_unit();

    void begin_focused_lower_solve();

    bool collect_focused_fringe(
        std::vector<std::uint32_t>& fringe,
        std::vector<double>& priority,
        const std::vector<double>* gap_lower_values = nullptr);

    std::pair<double, std::uint32_t> constructive_direct_action_upper(
        const std::uint32_t state,
        const std::uint32_t action_index) const;

    bool renewal_fallback_eligible(
        const std::uint32_t state,
        const FocusedFallbackPolicy& fallback) const;

    bool primitive_renewal_mode_eligible(
        const std::uint32_t state,
        const FocusedFallbackPolicy::PrimitiveRenewalMode& mode) const;

    double fallback_terminal_upper(
        const std::uint32_t state,
        const FocusedFallbackPolicy& fallback,
        std::uint32_t* selected_operator = nullptr) const;

    std::optional<FocusedFallbackPolicy> magic_regal_fallback();

    std::optional<FocusedFallbackPolicy>
    primitive_destructive_renewal_fallback();

    std::optional<FocusedFallbackPolicy> progressive_fracture_fallback(
        const FocusedFallbackPolicy& bootstrap);

    std::optional<FocusedFallbackPolicy> focused_fallback();

    double focused_start_upper_bound(
        const FocusedFallbackPolicy& fallback) const;

    std::pair<double, std::uint64_t> focused_direct_state_upper(
        const std::uint32_t state) const;

    std::pair<double, std::uint64_t> focused_direct_start_upper() const;

    void reset_focused_optimization_state();

    void schedule_next_focused_expansion(
        std::vector<std::uint32_t> fringe,
        const bool complete,
        const std::vector<double>& priority,
        FocusedScheduleRoundTelemetry telemetry);

    bool begin_focused_upper_solve();

    void sync_constructive_discovered_states();

    void finish_focused_lower_solve(
        const bool allow_upper_pass = true);

    void finish_focused_upper_solve(const bool succeeded);

    void run_focused_lower_unit();

    double backup_state(
        const std::uint32_t state,
        std::uint32_t& transition_work);

    void begin_priority_measurement();

    void run_bellman_unit();

    void step(std::uint32_t max_work_items);

    SolveProgress progress() const;

    SolveTelemetrySnapshot telemetry_snapshot(bool abandoned) const;

    void count_policy_actions(
        const std::vector<std::uint64_t>& rows,
        const std::vector<std::uint32_t>* frontier,
        std::map<std::string, std::uint64_t>& counts) const;

    void count_policy_actions(
        const std::vector<PolicyOperatorRef>& policy,
        std::map<std::string, std::uint64_t>& counts) const;

    SolveResult finish();

    std::uint64_t estimated_owned_bytes() const;

    std::uint64_t audited_estimated_owned_bytes() const;

    std::uint64_t output_incumbent_owned_bytes() const;

    std::uint64_t fast_estimated_owned_bytes() const;

    std::uint64_t fast_estimated_owned_bytes_with_calc(
        const std::uint64_t calc_bytes) const;

    std::uint64_t estimated_owned_bytes_with_calc(
        const std::uint64_t calc_bytes) const;
};

}
}
