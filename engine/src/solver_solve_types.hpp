#pragma once

#include "solver_action_envelope_ledger.hpp"
#include "solver_anytime_scheduler.hpp"
#include "solver_executable_carrier_planner.hpp"
#include "solver_joint_policy_continuation.hpp"
#include "solver_proof_pattern_manager.hpp"
#include "solver_solve_contracts.hpp"

#include "poecraft/bitset.h"

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <chrono>
#include <cmath>
#include <deque>
#include <functional>
#include <limits>
#include <memory>
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

/* Solver-local quotient for an automatic product-goal Fracture row. The
 * primitive evaluator remains exact; this kernel exists only at the coarse
 * product parent and never interns the physical non-goal Fracture outcomes. */
struct ProductFractureKernel {
    bool eligible = false;
    std::uint32_t raw_affix_count = 0;
    std::uint32_t acceptable_affix_count = 0;
    std::uint32_t acceptable_goal_mask = 0;
    std::uint32_t restart_state = kNoId;
    double hit_probability = 0.0;
    double miss_probability = 0.0;
    double probability_sum = 0.0;
    std::vector<OutcomeEntry> exits;
};

ProductFractureKernel build_product_fracture_kernel(
    CalcContext& calc,
    std::uint32_t state,
    std::uint32_t relevant_goal_mask);

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

/*
 * Shared fixed-policy workspace. The broad SolveWork and policy-guided exact
 * refinement both operate on the same sparse policy equations, so SCC
 * discovery and resumed sparse linear solves must have one implementation and
 * one deterministic state representation.
 */
struct SparsePolicyComponentWorkspace {
    bool components_ready = false;
    std::vector<std::vector<std::uint32_t>> components;
    std::vector<std::uint32_t> component_by_state;
    std::vector<std::int32_t> local;
    std::vector<std::uint32_t> tarjan_index;
    std::vector<std::uint32_t> tarjan_lowlink;
    std::vector<std::uint8_t> tarjan_on_stack;
    std::vector<std::uint32_t> tarjan_stack;
    std::vector<PolicyTarjanFrame> tarjan_dfs;
    std::uint32_t tarjan_next_index = 0;
    std::uint32_t tarjan_root_cursor = 0;
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

enum class SparsePolicySolveMode : std::uint8_t {
    BiCGSTAB,
    GaussSeidel,
};

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
    SparsePolicySolveMode mode = SparsePolicySolveMode::BiCGSTAB;
    double last_true_residual =
        std::numeric_limits<double>::infinity();
    std::uint8_t true_residual_stagnation = 0;
};

} // namespace

using namespace solve_detail;

namespace quotient {
class ProofStore;
}

/* A completed reachable closure is independent of the economy. Equivalent
 * kernels retain all operator/resource variants, so a later solve may change
 * relative prices without rebuilding transitions or reusing a stale action
 * representative. */
struct SolveTransitionCache {
    enum class ProductFractureQReason : std::uint8_t {
        None = 0,
        RowOrSourceNotRetained,
        NonfiniteSuccessorOrSelectedQ,
        SelectedStrictArgmin,
        CheaperThanCapturedPolicy,
        ExactTieLostByStableRowOrder,
        CostlierThanSelectedQ,
    };

    struct ProductFractureRowWitness {
        std::uint32_t source_state = kNoId;
        std::uint64_t row_index =
            std::numeric_limits<std::uint64_t>::max();
        std::uint32_t operator_index = kNoId;
        std::uint32_t raw_affix_count = 0;
        std::uint32_t acceptable_affix_count = 0;
        std::uint32_t acceptable_goal_mask = 0;
        std::uint32_t restart_state = kNoId;
        std::array<std::uint32_t, kMaxGoalSlots> hit_states{};
        std::array<double, kMaxGoalSlots> hit_probabilities{};
        std::uint32_t hit_state_count = 0;
        double hit_probability = 0.0;
        double miss_probability = 0.0;
        double probability_sum = 0.0;
        double restart_resource_quantity = 0.0;
        double fracture_action_cost = kInfinity;
        double restart_action_cost = kInfinity;
        double base_unit_cost = kInfinity;
        std::uint32_t restart_operator_index = kNoId;
        std::uint64_t action_vocabulary_identity = 0;
        std::uint64_t kernel_identity = 0;
        bool selected_in_policy = false;
        bool properness_checked = false;
        bool proper = false;
        std::uint32_t parent_miss_state_count = 0;
        bool final_q_evaluated = false;
        double final_source_value = kInfinity;
        double final_fracture_q = kInfinity;
        double final_selected_q = kInfinity;
        std::uint64_t final_selected_row =
            std::numeric_limits<std::uint64_t>::max();
        std::uint32_t final_selected_operator = kNoId;
        ProductFractureQReason final_selection_reason =
            ProductFractureQReason::None;
    };

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
    bool consider_imprint_programs = true;
    bool allow_economic_restart = true;
    std::uint32_t discovered_states = 0;
    std::uint32_t expanded_states = 0;
    std::uint32_t strict_discovered_states = 0;
    std::uint32_t quotient_states = 0;
    bool exact_quotient = false;
    std::vector<std::uint32_t> behavioral_representative_by_state;
    std::vector<std::uint8_t> expanded;
    std::vector<StateRowSpan> state_rows;
    std::vector<SparseRow> rows;
    /* Proof-carrying quotient rows retain their immutable payloads and
     * generation-stamped dependency sidecar directly beside stable row IDs. */
    std::shared_ptr<quotient::ProofStore> quotient_proofs;
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
    /* Full fixed-width provenance: one record for every retained product-local
     * Fracture candidate row, independent of the diagnostic sample cap. */
    std::vector<ProductFractureRowWitness> product_fracture_rows;
    std::uint64_t algebraic_self_loops = 0;
    bool focused_partial = false;
    bool reusable_closure = false;

    bool compatible(
        const std::uint32_t requested_start,
        const std::vector<PricedOperator>& priced,
        const SolveOptions& options) const;
    std::string compatibility_mismatch(
        std::uint32_t requested_start,
        const std::vector<PricedOperator>& priced,
        const SolveOptions& options) const;

    static std::uint64_t automatic_sample_nested_bytes(
        const AutomaticCandidateRecord& record);

    void retain_automatic_sample(AutomaticCandidateRecord record);

    void reconcile_automatic_sample_owned_bytes();

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

/* Pure portfolio contract used by the solver and focused tests. It keeps
 * invalidation and ordering independent of any named crafting action. */
struct CertifiedFallbackContract {
    double certified_upper_bound = kInfinity;
    double evaluated_policy_cost = kInfinity;
    std::uint64_t goal_identity = 0;
    std::uint64_t economy_identity = 0;
    std::uint64_t action_vocabulary_identity = 0;
    std::uint64_t action_vocabulary_size = 0;
    std::uint64_t caller_scope_identity = 0;
    std::uint64_t artifact_identity = 0;
    std::uint64_t source_generation = 0;
    std::uint64_t target_generation = 0;
    std::uint64_t graph_prefix_identity = 0;
    std::uint64_t witness_identity = 0;
    std::uint64_t portfolio_identity = 0;
    std::uint32_t root_operator = kNoId;
    std::string_view kind;
    bool complete_policy_or_witness = false;
    bool compiled_payload_present = false;
    bool compilation_provenance_present = false;
    bool independently_evaluated = false;
    bool proper = false;
    bool executable = false;
};

struct CertifiedFallbackCurrentContext {
    std::uint64_t goal_identity = 0;
    std::uint64_t economy_identity = 0;
    std::uint64_t action_vocabulary_identity = 0;
    std::uint64_t action_vocabulary_size = 0;
    std::uint64_t caller_scope_identity = 0;
    std::uint64_t artifact_identity = 0;
    std::uint64_t source_generation = 0;
    std::uint64_t target_generation = 0;
    std::uint64_t graph_prefix_identity = 0;
};

const char* certified_fallback_invalid_reason(
    const CertifiedFallbackContract& candidate,
    const CertifiedFallbackCurrentContext& current,
    double epsilon);

const char* retained_fallback_invalid_reason(
    const CertifiedFallbackContract& candidate,
    const CertifiedFallbackCurrentContext& current);

bool certified_fallback_precedes(
    const CertifiedFallbackContract& left,
    const CertifiedFallbackContract& right);

bool certified_fallback_fits_memory(
    std::uint64_t current_owned_bytes,
    std::uint64_t candidate_owned_bytes,
    std::uint64_t maximum_owned_bytes);

/* Proof lowers and ordering scores are intentionally non-interchangeable.
 * Only ProofLowerValue may cross a pruning/publication boundary; carrier
 * ordering remains a schedule-only authority. */
enum class CarrierOrderingMode : std::uint8_t {
    FocusedLegacy,
    IncrementalLegacy,
    CooperativeHighProgress,
};

struct CarrierOrderingScore {
    std::uint32_t state = kNoId;
    std::size_t stable_state_hash = 0;
    std::uint32_t goal_subset = 0;
    std::uint32_t satisfied_goals = 0;
    std::uint32_t fractured_goals = 0;
    std::uint32_t active_protection = 0;
    std::uint32_t useful_protection = 0;
    std::uint32_t capacity_obstructions = 0;
    std::uint32_t blocked_missing_goals = 0;
    std::uint32_t unrelated_occupancy = 0;
    double focused_priority = 0.0;
};

struct CarrierActionOrderingScore {
    std::uint32_t operator_index = kNoId;
    std::string_view stable_operator_id;
    std::uint32_t immediately_reachable_missing_goals = 0;
    std::uint32_t obstruction_removal = 0;
    std::uint32_t preserved_satisfied_goals = 0;
    std::uint32_t preserved_useful_protection = 0;
    std::uint32_t reachable_missing_goals = 0;
};

static_assert(!std::is_convertible_v<CarrierOrderingScore, ProofLowerValue>);
static_assert(!std::is_convertible_v<CarrierOrderingScore, double>);
static_assert(
    !std::is_convertible_v<CarrierActionOrderingScore, ProofLowerValue>);

struct CarrierPriorityBuckets {
    std::map<std::uint32_t, std::vector<std::uint32_t>> by_goal_subset;
    std::vector<std::uint32_t> subset_order;
};

CarrierOrderingScore make_carrier_ordering_score(
    const AbstractState& carrier,
    std::uint32_t state,
    std::uint32_t goal_subset,
    std::uint32_t prefix_goal_mask,
    std::uint32_t suffix_goal_mask,
    double focused_priority = 0.0);

CarrierPriorityBuckets build_carrier_priority_buckets(
    const std::vector<CarrierOrderingScore>& candidates,
    CarrierOrderingMode mode);

bool carrier_action_ordering_precedes(
    const CarrierActionOrderingScore& left,
    const CarrierActionOrderingScore& right);

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

struct SolveWork::Impl : solve_detail::ProofPatternManager {
    CalcContext& calc;
    const SessionImpl& session;
    /* Preserve the exact solve input for policy-guided publication
     * refinement. The coarse parent state is not an exact substitute. */
    pc_item_state exact_start_item{};
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
    bool requested_bounded_finish = false;
    bool incremental_restricted_values_ready = false;
    /* Exactly optimized values of a closed restricted action graph. They are
     * feasible per-carrier uppers after later actions are added; never use
     * focused heuristic/lower values here. */
    std::vector<double> incremental_certified_upper_values;
    bool incremental_reclassify_all = false;
    solve_detail::SolveScheduler anytime_scheduler;
    solve_detail::SolveScheduler focused_anytime_scheduler{
        solve_detail::kFocusedAnytimeSchedulingProfile};
    solve_detail::AnytimeSchedulerLane incremental_last_scheduled_lane =
        solve_detail::AnytimeSchedulerLane::LegacyFairness;
    bool incremental_warm_start_continuation_refined = false;
    struct IncrementalPriorityTask {
        std::uint32_t state = kNoId;
        std::uint32_t operator_index = kNoId;
    };
    std::vector<IncrementalPriorityTask> incremental_priority_tasks;
    std::size_t incremental_priority_task_cursor = 0;
    std::uint32_t incremental_warm_start_policy_wave = 0;
    bool incremental_upper_policy_dirty = true;
    bool incremental_upper_policy_pass = false;
    bool incremental_upper_fixed_policy_proved = false;
    bool incremental_post_upper_scheduling_active = false;
    std::size_t incremental_post_upper_proof_cursor = 0;
    double incremental_upper_policy_prior_bound = kInfinity;
    std::vector<std::uint64_t> incremental_upper_temporary_rows;
    bool incremental_epoch_added_states = false;
    std::vector<std::uint32_t> incremental_carriers;
    std::size_t incremental_carrier_cursor = 0;
    /* High-impact delayed rows use operator-major scheduling and therefore
     * cannot share incremental_carrier_cursor with carrier-local automatic
     * preparation. This cursor is the exact automatic-envelope obligation. */
    std::size_t incremental_automatic_carrier_cursor = 0;
    std::size_t incremental_automatic_epoch_end = 0;
    std::vector<std::uint32_t> incremental_automatic_carrier_order;
    std::size_t incremental_automatic_order_cursor = 0;
    std::vector<std::uint32_t> incremental_fairness_carrier_order;
    std::size_t incremental_fairness_epoch_end = 0;
    std::size_t incremental_fairness_carrier_cursor = 0;
    std::size_t incremental_fairness_operator_cursor = 0;
    std::vector<std::uint32_t> incremental_high_progress_carrier_order;
    std::size_t incremental_high_progress_epoch_end = 0;
    std::size_t incremental_high_progress_carrier_cursor = 0;
    std::vector<std::uint32_t> incremental_high_progress_operator_order;
    std::size_t incremental_high_progress_operator_cursor = 0;
    std::size_t incremental_closure_carrier_cursor = 0;
    std::size_t incremental_closure_operator_cursor = 0;
    std::size_t incremental_operator_cursor = 0;
    bool incremental_dynamic_prepared = false;
    bool incremental_dynamic_prepare_active = false;
    bool incremental_resume_epoch_after_dynamic_prepare = false;
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
    enum class IncrementalClassificationUpper : std::uint8_t {
        None,
        ResultValues,
        OutputIncumbent,
    };
    bool incremental_classification_active = false;
    bool incremental_classification_admitted = false;
    bool incremental_classification_reclassify_all = false;
    bool incremental_classification_restricted_graph_closed = false;
    std::size_t incremental_classification_cursor = 0;
    IncrementalClassificationUpper incremental_classification_upper =
        IncrementalClassificationUpper::None;
    std::vector<double> incremental_classification_certified_lower;
    /* The Gate 3 fallback keeps the proven completed-pair scheduler view.
     * The typed ledger remains the complete observational lifecycle and can
     * become authoritative only after a profile qualifies all controls. */
    std::unordered_set<std::uint64_t> incremental_completed_pairs;
    solve_detail::ActionEnvelopeLedger action_envelope_ledger;
    std::uint64_t incremental_unevaluated_actions = 0;
    std::uint64_t incremental_inapplicable_actions = 0;
    std::uint64_t incremental_resource_unresolved_actions = 0;
    std::uint64_t incremental_unique_kernel_evaluations = 0;
    std::uint64_t incremental_carrier_kernel_reuses = 0;
    std::uint64_t descriptor_proof_evaluations = 0;
    std::uint64_t descriptor_proof_separations = 0;
    std::uint64_t incremental_carrier_ladder_epochs = 0;
    std::uint64_t incremental_carrier_ladder_candidates = 0;
    std::uint64_t incremental_carrier_ladder_goal_subsets = 0;
    std::uint64_t incremental_reoptimizations = 0;
    std::uint32_t incremental_first_alternative_expanded_states = 0;
    bool incremental_refinement_active = false;
    std::uint32_t incremental_refinement_target_expanded = 0;
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
    /* A single completed row can look non-improving against the current
     * incumbent even when several completed rows form a much better proper
     * policy together. Periodically synthesize that joint executable witness
     * at geometrically growing row checkpoints; this never admits the rows to
     * the lower problem or closes their exact envelope. */
    std::size_t incremental_anytime_next_row_checkpoint =
        solve_detail::kAnytimeSchedulingProfile
            .first_incumbent_checkpoint_rows;
    std::uint64_t incremental_anytime_policy_attempts = 0;
    std::uint64_t incremental_anytime_policy_successes = 0;
    std::array<
        std::uint64_t,
        static_cast<std::size_t>(AutomaticCandidateKind::Veiled) + 1>
        incremental_joint_policy_attempt_kinds{};
    std::array<
        std::uint64_t,
        static_cast<std::size_t>(AutomaticCandidateKind::Veiled) + 1>
        incremental_joint_policy_success_kinds{};
    std::uint64_t incremental_anytime_policy_last_completed_rows = 0;
    double incremental_anytime_policy_best_upper = kInfinity;
    double incremental_anytime_checkpoint_upper = kInfinity;
    std::string incremental_anytime_policy_last_failure;
    /* A joint upper proof can expose a stochastic successor whose incumbent
     * continuation is not valid for that carrier shape. Feed that concrete
     * state back into ordinary exact expansion before retrying composition. */
    std::vector<std::uint32_t> incremental_anytime_missing_frontier_states;
    std::uint64_t incremental_missing_frontier_discovered = 0;
    std::uint64_t incremental_missing_frontier_priority_offers = 0;
    std::uint64_t incremental_missing_frontier_service_completions = 0;
    std::uint64_t incremental_missing_frontier_max_open = 0;
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
    /* The priced synthetic action is always retained as mechanic-owned
     * replacement recovery infrastructure. Ordinary Bellman authority uses
     * restart_operator_index/cost only when economic Restart is enabled. */
    std::uint32_t replacement_recovery_operator_index = kNoId;
    double replacement_recovery_cost = kInfinity;
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
    /* An Imprint grammar refusal is a solve-wide open family obligation.
     * Retry the interrupted carrier without Imprint so unrelated automatic
     * families can still close, but never spend the same family budget again
     * on a later carrier in this solve. */
    bool imprint_family_resource_deferred = false;
    std::string incremental_deferred_resource_cap;
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
    std::unique_ptr<SparsePolicyResume> sparse_policy_resume;
    struct SharedPolicyKernelRepresentative {
        std::uint32_t state = kNoId;
        std::vector<std::uint64_t> exact_signature;
    };
    struct PolicyKernelPreparation : SparsePolicyComponentWorkspace {
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
        std::uint32_t component_cursor = 0;
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
    bool policy_strict_order_reconciled = true;
    std::uint32_t unreconciled_stable_policy_rounds = 0;
    bool numerical_stability_stop = false;
    double policy_selection_residual = 0.0;
    std::uint64_t peak_policy_scratch_bytes = 0;
    std::uint64_t current_policy_scratch_bytes = 0;
    /* Resource-stop anytime construction temporarily retains the completed
     * lower-policy snapshot while the shared fixed-policy evaluator proves a
     * start-reachable executable policy. Those local vectors are outside the
     * ordinary result/policy fields and therefore need their own selected-byte
     * authority while the proof is live. */
    std::uint64_t anytime_policy_scratch_bytes = 0;
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
    enum class FocusedLowerPreparationStage : std::uint8_t {
        Idle,
        ProofValues,
        RetainDirect,
        RetainClassMinimum,
        ApplyClassMinimum,
        GoalStates,
        Finalize,
    };
    FocusedLowerPreparationStage focused_lower_preparation_stage =
        FocusedLowerPreparationStage::Idle;
    std::uint32_t focused_lower_preparation_cursor = 0;
    std::vector<double> focused_lower_previous_values;
    std::vector<double> focused_lower_retained_minimum;
    /* Proof-only snapshot produced by the cooperative focused initializer.
     * Unlike result.values, this never absorbs retained restricted-policy
     * values and is therefore safe for open-envelope row classification. */
    std::vector<double> focused_lower_completion_proof_values;
    bool focused_lower_completion_proof_snapshot_initialized = false;
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
    /* Constructive fallback discovery can inspect every exact renewal exit
     * on every expanded carrier. Retain its cursor so one public solve step
     * never has to synthesize the entire proof synchronously. */
    bool constructive_policy_active = false;
    bool constructive_fallback_pending = false;
    std::optional<FocusedFallbackPolicy> constructive_progress_fallback;
    struct PrimitiveDestructiveRenewalWork {
        bool active = false;
        std::vector<std::uint64_t> materialized_alternatives;
        std::vector<std::uint32_t> renewal_sources;
        std::size_t renewal_source_cursor = 0;
        FocusedFallbackPolicy best;
        double best_start = kInfinity;
    } primitive_destructive_renewal_work;
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
        /* Independently publishable candidates retain their already-checked
         * compiler artifact separately from a cheaper policy that may still
         * require strict lifting. The artifact is never scalar-only: it is
         * coupled to the complete captured policy/witness and identities
         * below. */
        RetainedCompiledPolicyArtifact compiled_artifact;
        std::uint32_t restart_operator = kNoId;
        std::uint32_t restart_state = kNoId;
        std::uint32_t fallback_anchor_state = kNoId;
        std::uint32_t round = 0;
        std::string kind;
        std::uint64_t goal_identity = 0;
        std::uint64_t economy_identity = 0;
        std::uint64_t action_vocabulary_identity = 0;
        std::uint64_t action_vocabulary_size = 0;
        std::uint64_t caller_scope_identity = 0;
        std::uint64_t graph_identity = 0;
        std::uint64_t artifact_identity = 0;
        std::uint64_t source_generation = 0;
        std::uint64_t target_generation = 0;
        std::uint64_t graph_prefix_identity = 0;
        std::uint64_t graph_row_count = 0;
        std::uint64_t graph_priced_row_count = 0;
        std::uint64_t graph_successor_count = 0;
        std::uint64_t graph_probability_count = 0;
        std::uint64_t graph_choice_count = 0;
        std::uint64_t graph_choice_successor_count = 0;
        std::uint64_t graph_choice_option_count = 0;
        std::uint64_t portfolio_identity = 0;
        std::uint64_t retained_owned_bytes = 0;
        std::string compilation_provenance;
        std::string final_graph_verification_failure;
        double reconciliation_absolute_delta = kInfinity;
        double reconciliation_relative_delta = kInfinity;
        bool strict_state_provenance = true;
        bool policy_materialized = false;
        bool independently_certified = false;
        bool independently_evaluated = false;
        bool proper = false;
        bool executable = false;
    };
    struct CarrierLadderBoundaryCapture {
        enum class StopKind : std::uint8_t {
            RequestedEntry = 0,
            GoalSuccess,
            CertifiedFrontier,
            UnresolvedMissing,
        };
        enum class RowServiceDisposition : std::uint8_t {
            NotObserved = 0,
            Undiscovered,
            Goal,
            SelectableRowAvailable,
            CertifiedFrontier,
            RequestedBoundedFinish,
            ResourceCap,
            NoRowSpan,
            RowsNotScheduled,
            RowsScheduledIncomplete,
            CompletedRowsInvalidOrUnpriced,
            OtherMissing,
        };
        struct RowServiceWitness {
            static constexpr std::uint64_t kNoPosition =
                std::numeric_limits<std::uint64_t>::max();

            std::uint32_t state = kNoId;
            std::uint32_t goal_mask = 0;
            std::uint32_t goal_progress = 0;
            std::uint32_t certified_frontier_operator = kNoId;
            std::uint32_t planner_operator_count = 0;
            std::uint32_t priced_operator_count = 0;
            std::uint32_t static_candidate_operator_count = 0;
            std::uint32_t delayed_candidate_operator_count = 0;
            std::uint32_t dynamic_candidate_operator_count = 0;
            std::uint32_t declared_row_count = 0;
            std::uint32_t owned_row_count = 0;
            std::uint32_t row_owner_mismatch_count = 0;
            std::uint32_t admitted_row_count = 0;
            std::uint32_t completed_row_count = 0;
            std::uint32_t alternative_row_count = 0;
            std::uint32_t alternative_completed_row_count = 0;
            std::uint32_t priced_row_count = 0;
            std::uint32_t valid_operator_row_count = 0;
            std::uint32_t finite_nonnegative_priced_row_count = 0;
            std::uint32_t selectable_row_count = 0;
            std::uint32_t selectable_operator_count = 0;
            std::uint32_t priority_task_count = 0;
            std::uint32_t pending_priority_task_count = 0;
            std::uint32_t completed_pair_count = 0;
            std::uint64_t variant_count = 0;
            std::uint64_t transition_count = 0;
            std::uint64_t choice_count = 0;
            std::uint64_t queue_position = kNoPosition;
            std::uint64_t carrier_position = kNoPosition;
            std::uint64_t automatic_order_position = kNoPosition;
            std::uint64_t fairness_order_position = kNoPosition;
            std::uint64_t high_progress_order_position = kNoPosition;
            std::uint64_t missing_frontier_position = kNoPosition;
            std::uint64_t first_priority_task_position = kNoPosition;
            std::uint64_t carrier_cursor = 0;
            std::uint64_t automatic_carrier_cursor = 0;
            std::uint64_t automatic_order_cursor = 0;
            std::uint64_t fairness_carrier_cursor = 0;
            std::uint64_t fairness_operator_cursor = 0;
            std::uint64_t high_progress_carrier_cursor = 0;
            std::uint64_t high_progress_operator_cursor = 0;
            std::uint64_t closure_carrier_cursor = 0;
            std::uint64_t closure_operator_cursor = 0;
            std::uint64_t priority_task_cursor = 0;
            std::uint64_t action_envelope_transition_count = 0;
            std::uint64_t action_envelope_entry_count = 0;
            std::uint64_t action_envelope_row_entry_count = 0;
            std::uint64_t action_envelope_scheduling_complete_count = 0;
            std::uint64_t observation_state_count = 0;
            std::uint64_t observation_row_count = 0;
            std::uint64_t observation_priced_row_count = 0;
            std::uint64_t observation_successor_count = 0;
            std::uint64_t observation_probability_count = 0;
            std::uint64_t observation_choice_count = 0;
            std::uint64_t observation_choice_successor_count = 0;
            std::uint64_t observation_choice_option_count = 0;
            std::array<
                std::uint64_t,
                solve_detail::ActionEnvelopeLedger::kStateCount>
                action_envelope_lifecycle_counts{};
            std::array<
                std::uint64_t,
                solve_detail::ActionEnvelopeLedger::kLaneCount>
                action_envelope_lane_counts{};
            std::array<
                std::uint64_t,
                solve_detail::ActionEnvelopeLedger::kAuthorityCount>
                action_envelope_authority_counts{};
            std::array<
                std::uint64_t,
                solve_detail::ActionEnvelopeLedger::kStopOwnerCount>
                action_envelope_stop_owner_counts{};
            std::uint64_t selectable_operator_identity = 0;
            std::uint64_t row_identity = 0;
            std::uint64_t action_envelope_identity = 0;
            std::uint64_t scheduler_identity = 0;
            std::uint64_t observation_graph_identity = 0;
            std::uint64_t observation_graph_prefix_identity = 0;
            std::uint64_t facts_identity = 0;
            std::uint64_t identity = 0;
            RowServiceDisposition disposition =
                RowServiceDisposition::NotObserved;
            bool observed = false;
            bool state_in_calc = false;
            bool goal = false;
            bool broad_expanded = false;
            bool ordinary_result_expanded = false;
            bool transition_cache_expanded = false;
            bool focused_strict_expanded = false;
            bool queued = false;
            bool queue_contains_state = false;
            bool row_span_present = false;
            bool carrier = false;
            bool missing_frontier = false;
            bool expansion_active_for_state = false;
            bool expansion_incremental_alternative = false;
            bool incremental_refinement_active = false;
            bool incremental_refinement_targets_state = false;
            bool action_envelope_scheduler_view_enabled = false;
            bool requested_bounded_finish = false;
            bool resource_cap_hit = false;
        };
        struct Stop {
            std::uint32_t state = kNoId;
            std::uint32_t operator_index = kNoId;
            StopKind kind = StopKind::UnresolvedMissing;
            std::vector<std::uint64_t> coarse_state_key;
            std::uint64_t operator_semantic_identity = 0;
            std::uint64_t continuation_identity = 0;
            std::uint64_t incumbent_identity = 0;
            std::uint64_t incumbent_graph_prefix_identity = 0;
            std::uint64_t incumbent_artifact_identity = 0;
            std::uint64_t frontier_value_bits = 0;
            bool independently_certified = false;
            bool independently_evaluated = false;
            bool proper = false;
            bool executable = false;
        };
        struct RecoveredMember {
            std::vector<std::uint64_t> stable_key;
            std::uint32_t coarse_state = kNoId;
            pc_item_state item{};
        };
        struct ReachedStop {
            std::vector<std::uint64_t> predecessor_stable_key;
            std::uint32_t predecessor_coarse_state = kNoId;
            std::vector<std::uint64_t> predecessor_coarse_state_key;
            std::uint32_t selected_coarse_operator = kNoId;
            std::uint32_t selected_strict_operator = kNoId;
            std::vector<std::uint64_t> selected_action_semantic_key;
            std::vector<std::uint64_t> stopped_stable_key;
            std::uint32_t stopped_coarse_state = kNoId;
            std::vector<std::uint64_t> stopped_coarse_state_key;
            std::uint64_t probability_bits = 0;
            std::uint64_t captured_policy_row =
                std::numeric_limits<std::uint64_t>::max();
            StopKind kind = StopKind::UnresolvedMissing;
            bool captured_coarse_reachable = false;
            bool completed_selected_row = false;
        };
        BoundedPolicyIncumbent prefix;
        std::vector<Stop> stops;
        RowServiceWitness row_service_witness;
        RowServiceWitness terminal_row_service_witness;
        std::uint32_t target_state = kNoId;
        std::vector<std::uint64_t> target_state_key;
        std::uint64_t selection_identity = 0;
        std::uint64_t prefix_identity = 0;
        std::uint64_t exit_contract_identity = 0;
        std::uint64_t goal_identity = 0;
        std::uint64_t economy_identity = 0;
        std::uint64_t caller_scope_identity = 0;
        std::uint64_t action_vocabulary_identity = 0;
        std::uint64_t graph_identity = 0;
        std::uint64_t graph_prefix_identity = 0;
        std::uint64_t artifact_identity = 0;
        std::uint64_t executable_identity = 0;
        std::uint64_t source_identity = 0;
        std::uint64_t retained_owned_bytes = 0;
        std::uint64_t recovery_peak_owned_bytes = 0;
        std::uint64_t recovery_work = 0;
        std::uint64_t recovery_wall_time_ms = 0;
        std::uint64_t exact_member_identity = 0;
        std::uint64_t reached_stop_identity = 0;
        std::uint64_t reached_stop_count = 0;
        std::uint64_t reached_stop_samples_omitted = 0;
        std::uint32_t selected_states = 0;
        std::uint32_t goal_stops = 0;
        std::uint32_t certified_frontier_stops = 0;
        std::uint32_t unresolved_stops = 0;
        std::uint32_t exact_states = 0;
        std::uint32_t exact_rows = 0;
        std::uint64_t exact_transitions = 0;
        bool complete_support = false;
        bool absorption_proved = false;
        std::string status = "not_captured";
        std::string refusal;
        std::string recovery_status = "not_run";
        std::string recovery_refusal;
        std::vector<RecoveredMember> recovered_members;
        std::vector<ReachedStop> reached_stops;
    };
    struct JointAnytimeAttemptLineage {
        enum class Trigger : std::uint8_t {
            IncrementalCheckpoint = 0,
            PublicationPreflight,
            TerminalPublication,
        };
        struct SelectedDecision {
            std::uint32_t state = kNoId;
            std::uint64_t row = std::numeric_limits<std::uint64_t>::max();
            std::uint32_t operator_index = kNoId;
        };

        std::uint64_t ordinal = 0;
        Trigger trigger = Trigger::IncrementalCheckpoint;
        std::uint64_t source_generation = 0;
        std::uint64_t target_generation = 0;
        std::uint64_t ordinary_admitted_rows = 0;
        std::uint64_t completed_row_count = 0;
        std::uint64_t completed_row_identity = 0;
        std::uint64_t completed_row_delta = 0;
        std::uint64_t completed_alternative_rows = 0;
        std::uint64_t alternative_row_identity = 0;
        std::uint64_t alternative_row_delta = 0;
        std::uint64_t incumbent_identity_before = 0;
        double incumbent_estimate_before = kInfinity;
        double incumbent_exact_cost_before = kInfinity;
        double candidate_root_estimate = kInfinity;
        std::uint64_t selection_identity = 0;
        std::uint64_t selected_state_count = 0;
        std::uint64_t selected_samples_omitted = 0;
        std::uint64_t certified_frontier_uses = 0;
        std::uint64_t renewal_boundary_attempts = 0;
        std::uint64_t renewal_boundary_successes = 0;
        std::uint32_t first_missing_state = kNoId;
        std::uint32_t first_missing_goal_mask = 0;
        std::uint64_t candidate_portfolio_identity = 0;
        std::uint64_t compiled_nodes = 0;
        std::uint64_t compiled_edges = 0;
        double independently_evaluated_cost = kInfinity;
        bool incumbent_independently_evaluated_before = false;
        bool requested_bounded_finish_at_attempt = false;
        bool missing_refinement_selected = false;
        std::uint32_t missing_refinement_selected_at_expanded_states = kNoId;
        bool completed_rows_monotone = true;
        bool terminal_sees_all_completed_alternative_rows = false;
        bool fixed_policy_proper = false;
        bool installed_for_finalization = false;
        bool compilation_attempted = false;
        bool compilation_succeeded = false;
        bool independent_evaluation_attempted = false;
        bool independent_evaluation_succeeded = false;
        std::string candidate_kind;
        std::string failure;
        std::string compilation_result;
        std::string independent_evaluation_result;
        std::string portfolio_decision;
        std::string portfolio_reason;
        std::vector<SelectedDecision> selected_decisions;
    };
    /*
     * A Bellman-selected row policy is not a certified upper bound. Keep its
     * complete captured materialization in a distinct wrapper so no
     * unverified estimate can enter the executable fallback portfolio by
     * accident. Only retained finalization may convert the snapshot after
     * independent compiled-graph evaluation succeeds; finish() only moves an
     * already finalized result.
     */
    struct UnverifiedSelectedPolicyCandidate {
        BoundedPolicyIncumbent snapshot;
        bool has_exact_start_item = false;
        pc_item_state exact_start_item{};
        double selected_estimate = kInfinity;
        std::uint64_t selected_policy_hash = 0;
        std::uint64_t capture_identity = 0;
        bool numerical_stability_stop = false;
    };
    /*
     * One owner for candidates produced by coarse selection, constructive
     * renewal, direct certification, strict lift, and final graph assertion.
     * The compatibility aliases below keep Gate 2 behavior-neutral while
     * later gates migrate the remaining call sites to portfolio methods.
     */
    struct IncumbentPortfolio {
        std::optional<BoundedPolicyIncumbent> output;
        std::optional<UnverifiedSelectedPolicyCandidate> pending_candidate;
        /* Best-first, deterministic, bounded collection of materialized or
         * independently verified candidates displaced by another preferred
         * output. Verification state remains explicit on every entry. */
        std::vector<BoundedPolicyIncumbent> retained_candidates;
        double finalization_verified_upper = kInfinity;
        double best_verified_upper = kInfinity;
        std::uint64_t best_verified_identity = 0;
        std::uint64_t best_verified_goal_identity = 0;
        std::uint64_t best_verified_economy_identity = 0;
        std::uint64_t best_verified_action_vocabulary_identity = 0;
        std::uint64_t best_verified_caller_scope_identity = 0;
        std::uint64_t best_verified_graph_prefix_identity = 0;
        std::uint64_t best_verified_artifact_identity = 0;
        std::uint64_t best_verified_source_generation = 0;
        std::uint64_t verified_observations = 0;
        std::uint64_t verified_replacements = 0;
        bool monotonicity_violation = false;

        void observe_verified(const BoundedPolicyIncumbent& candidate) {
            if (!candidate.independently_certified ||
                !candidate.independently_evaluated || !candidate.proper ||
                !candidate.executable ||
                !std::isfinite(candidate.evaluated_policy_cost) ||
                candidate.evaluated_policy_cost < 0.0) {
                return;
            }
            ++verified_observations;
            const double upper = candidate.evaluated_policy_cost;
            if (upper < best_verified_upper) {
                if (std::isfinite(best_verified_upper)) {
                    ++verified_replacements;
                }
                best_verified_upper = upper;
                best_verified_identity = candidate.portfolio_identity;
                best_verified_goal_identity = candidate.goal_identity;
                best_verified_economy_identity = candidate.economy_identity;
                best_verified_action_vocabulary_identity =
                    candidate.action_vocabulary_identity;
                best_verified_caller_scope_identity =
                    candidate.caller_scope_identity;
                best_verified_graph_prefix_identity =
                    candidate.graph_prefix_identity;
                best_verified_artifact_identity =
                    candidate.artifact_identity;
                best_verified_source_generation =
                    candidate.source_generation;
            }
        }

        double verified_executable_upper() const {
            return std::min(
                best_verified_upper, finalization_verified_upper);
        }
    } incumbent_portfolio;
    struct ResumableJointPolicyCandidateState {
        solve_detail::ResumableJointPolicyContinuation continuation;
        FocusedFallbackWitness certified_fallback;
        PrimitiveRenewalWitness certified_renewal;
        std::vector<std::uint8_t> certified_boundary_reachable;
        std::uint64_t carrier_epochs_at_capture = 0;
        std::uint64_t refinement_rounds_at_capture = 0;
        std::uint64_t sweeps_at_capture = 0;
        std::uint64_t carrier_epochs_before_last_resume = 0;
        std::uint64_t refinement_rounds_before_last_resume = 0;
        std::uint64_t sweeps_before_last_resume = 0;
        std::uint64_t ordinary_interleave_events = 0;
        std::uint64_t handoff_count = 0;
        std::uint64_t current_owned_payload_bytes = 0;
        bool handed_off_for_evaluation = false;

        std::uint64_t audited_owned_payload_bytes() const {
            return continuation.retained_owned_bytes() +
                certified_boundary_reachable.capacity() *
                    sizeof(std::uint8_t) +
                certified_renewal.kernel_signature.capacity() *
                    sizeof(std::uint64_t);
        }

        void refresh_owned_payload_bytes() {
            current_owned_payload_bytes = audited_owned_payload_bytes();
        }

        void release_owned_payload() {
            certified_fallback.reset();
            certified_renewal = PrimitiveRenewalWitness{};
            std::vector<std::uint8_t>().swap(
                certified_boundary_reachable);
            refresh_owned_payload_bytes();
        }
    };
    std::optional<ResumableJointPolicyCandidateState>
        resumable_joint_policy_candidate;
    std::optional<BoundedPolicyIncumbent>& output_incumbent =
        incumbent_portfolio.output;
    std::optional<UnverifiedSelectedPolicyCandidate>&
        unverified_selected_policy_candidate =
            incumbent_portfolio.pending_candidate;
    std::vector<BoundedPolicyIncumbent>& certified_fallback_portfolio =
        incumbent_portfolio.retained_candidates;
    /* Separate observational owner. Nothing in incumbent_portfolio,
     * scheduling, Bellman optimization, or publication reads this record. */
    std::optional<CarrierLadderBoundaryCapture>
        carrier_ladder_exact_boundary_capture;
    std::vector<JointAnytimeAttemptLineage>
        joint_anytime_attempt_lineage;
    std::uint64_t carrier_ladder_exact_boundary_private_wall_ns = 0;
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
    struct GoalSurvivalPath {
        /* kNoId is the internal identity-only checkpoint step used by
         * Imprint retry runtime semantics. */
        std::vector<std::uint32_t> actions;
    };
    struct OperatorGoalSurvivalPaths {
        std::vector<GoalSurvivalPath> paths;
    };
    std::vector<OperatorGoalSurvivalPaths>
        operator_goal_survival_paths;
    std::vector<std::uint8_t> operator_goal_survival_computed;
    std::uint64_t owned_goal_survival_nested_bytes = 0;
    bool ordering_goal_masks_ready = false;
    std::uint32_t ordering_prefix_goal_mask = 0;
    std::uint32_t ordering_suffix_goal_mask = 0;
    /* Carrier-aware extension of the clean goal-progress relaxation over
     * rarity x satisfied-goal subset. It grants free junk removal and perfect
     * goal/carrier preservation, but retains exact rarity legality and uses
     * the same optimistic pool-probability authority as the clean MDP. */
    /* Final one-step lower value of every non-refined action in the clean
     * goal-progress relaxation. The strict normal/magic pattern database
     * uses this as an optimistic escape while evaluating the productive
     * currency actions against their exact blocker identities. */
    bool price_bound_state_pruning = false;
    std::vector<double> certified_state_upper;
    std::vector<std::uint64_t> certified_state_row;
    struct CarrierBoundAttributionWork {
        static constexpr std::size_t kGoalMaskCount =
            std::size_t{1} << kMaxGoalSlots;
        static constexpr std::size_t kSideCapacityCount = 16;
        static constexpr std::size_t kProtectionModeCount = 4;
        static constexpr std::size_t kFractureShapeCount =
            (kMaxGoalSlots + 1) * 4;
        static constexpr std::size_t kUnrelatedOccupancyCount = 7;
        static constexpr std::size_t kPrimitiveFamilyCount =
            static_cast<std::size_t>(PrimitiveTelemetryFamily::Count);
        static constexpr std::size_t kAutomaticFamilyCount =
            static_cast<std::size_t>(AutomaticTelemetryKind::Count);
        static constexpr std::size_t kOperatorFamilyCount =
            kPrimitiveFamilyCount + kAutomaticFamilyCount + 2;
        enum CleanCoverRejection : std::uint32_t {
            InvalidState = 1u << 0,
            ActiveProtection = 1u << 1,
            FracturedGoal = 1u << 2,
            FracturedMetamod = 1u << 3,
            InfluenceIdentity = 1u << 4,
            SearingIdentity = 1u << 5,
            EaterIdentity = 1u << 6,
            FracturedJunk = 1u << 7,
            FracturedCraftedJunk = 1u << 8,
        };
        static constexpr std::size_t kCleanCoverRejectionCount = 9;

        struct CarrierShapeHistogram {
            std::uint64_t total = 0;
            std::array<std::uint64_t, kGoalMaskCount> goal_subset{};
            std::array<std::uint64_t, kSideCapacityCount> side_capacity{};
            std::array<std::uint64_t, kGoalMaskCount> blocked_mask{};
            std::array<std::uint64_t, kProtectionModeCount> protection{};
            std::array<std::uint64_t, kFractureShapeCount> fracture{};
            std::array<std::uint64_t, kUnrelatedOccupancyCount>
                unrelated_occupancy{};
        };

        struct OperatorLowerStats {
            std::uint64_t evaluations = 0;
            std::uint64_t finite_values = 0;
            std::uint64_t margin_evaluations = 0;
            std::uint64_t state_incumbent_prunes = 0;
            std::uint64_t constructive_prunes = 0;
            double minimum_value = kInfinity;
            double maximum_value = -kInfinity;
            /* Positive means the lower lies above the incumbent/constructive
             * upper and can separate after the epsilon allowance. */
            double minimum_margin = kInfinity;
            double maximum_margin = -kInfinity;
        };

        static constexpr std::size_t kOperatorShadowSampleLimit = 32;
        struct OperatorShadowSample {
            std::uint32_t state = kNoId;
            std::uint32_t operator_index = kNoId;
            std::uint32_t satisfied_goal_mask = 0;
            std::uint32_t blocked_mask = 0;
            std::uint8_t prefix_count = 0;
            std::uint8_t suffix_count = 0;
            std::uint8_t unrelated_occupancy = 0;
            ActionEnvelopeState lifecycle = ActionEnvelopeState::Queued;
            double lower = kInfinity;
            double upper = kInfinity;
            double absolute_margin = kInfinity;
            double retirement_margin = -kInfinity;
        };

        struct VerifiedIncumbentOperatorShadow {
            std::uint64_t incumbent_identity = 0;
            std::uint64_t audits = 0;
            ExecutableContinuationReuseStatus reuse_status =
                ExecutableContinuationReuseStatus::IncompleteCertificate;
            std::uint64_t ledger_entries = 0;
            std::uint64_t uncertified_upper_entries = 0;
            std::uint64_t finite_upper_entries = 0;
            std::uint64_t finite_lower_entries = 0;
            std::uint64_t comparable_entries = 0;
            std::uint64_t would_retire = 0;
            std::uint64_t still_competitive = 0;
            std::uint64_t live_ledger_entries = 0;
            std::uint64_t live_finite_upper_entries = 0;
            std::uint64_t live_finite_lower_entries = 0;
            std::uint64_t live_comparable_entries = 0;
            std::uint64_t live_would_retire = 0;
            std::uint64_t live_still_competitive = 0;
            std::uint64_t strict_obligations_examined = 0;
            std::uint64_t strict_rows_begun_before_comparison = 0;
            std::uint64_t strict_alternative_rows_begun_before_comparison = 0;
            std::uint64_t ledger_transitions_before_comparison = 0;
            std::uint64_t solver_rows_before_comparison = 0;
            std::uint64_t comparison_available_wall_ns = 0;
            std::uint32_t certificate_requested_members = 0;
            std::uint32_t certificate_certified_members = 0;
            std::uint32_t certificate_refused_members = 0;
            std::uint32_t certificate_represented_states = 0;
            std::uint32_t certificate_certified_states = 0;
            std::uint32_t certificate_refused_states = 0;
            std::uint32_t certificate_maximum_member_multiplicity = 0;
            double certificate_maximum_member_value_spread = 0.0;
            double certificate_maximum_bellman_residual = 0.0;
            std::uint64_t certificate_retained_bytes = 0;
            std::uint64_t certificate_transient_bytes = 0;
            std::uint64_t certificate_build_ns = 0;
            std::array<std::uint64_t, kOperatorFamilyCount>
                would_retire_by_family{};
            std::array<std::uint64_t, kOperatorFamilyCount>
                still_competitive_by_family{};
            CarrierShapeHistogram comparable_shapes;
            CarrierShapeHistogram would_retire_shapes;
            std::array<OperatorShadowSample, kOperatorShadowSampleLimit>
                closest_competitive{};
            std::size_t closest_competitive_count = 0;
            std::array<OperatorShadowSample, kOperatorShadowSampleLimit>
                largest_retirement_margins{};
            std::size_t largest_retirement_margin_count = 0;
        };

        enum class OperatorLowerSkipReason : std::uint8_t {
            NoFiniteIncumbent = 0,
            Count,
        };
        static constexpr std::size_t kOperatorLowerSkipReasonCount =
            static_cast<std::size_t>(OperatorLowerSkipReason::Count);

        struct UpperMilestone {
            bool present = false;
            double value = kInfinity;
            std::uint64_t wall_ns = 0;
            std::uint32_t discovered_states = 0;
            std::uint32_t expanded_states = 0;
            std::uint64_t rows = 0;
            std::uint64_t transitions = 0;
            std::uint64_t reforge_work = 0;
        };

        enum class ScheduleStage : std::uint8_t {
            FocusedCandidate = 0,
            FocusedAdmission,
            FocusedLadderAdmission,
            IncrementalCandidate,
            IncrementalCarrierAdmission,
            CarrierActionAdmission,
            Count,
        };
        static constexpr std::size_t kScheduleStageCount =
            static_cast<std::size_t>(ScheduleStage::Count);

        std::chrono::steady_clock::time_point started_at =
            std::chrono::steady_clock::now();
        std::array<CarrierShapeHistogram, kScheduleStageCount> schedules{};
        std::array<OperatorLowerStats, kOperatorFamilyCount> operator_lower{};
        std::array<
            std::array<std::uint64_t, kOperatorLowerSkipReasonCount>,
            kOperatorFamilyCount>
            operator_lower_skips{};
        std::array<std::uint64_t, kOperatorFamilyCount>
            carrier_action_admissions_by_family{};
        VerifiedIncumbentOperatorShadow verified_incumbent_operator_shadow;
        UpperMilestone first_finite_upper;
        UpperMilestone first_verified_upper;
    };
    std::unique_ptr<CarrierBoundAttributionWork> carrier_bound_attribution;
    std::uint64_t peak_owned_bytes = 0;
    SolvePhase phase = SolvePhase::Expanding;
    /* Proof-model setup is measured before the first public work boundary.
     * A configured CalcContext cap reached there must still cross the same
     * ordinary step catch/publication path as a cap reached by expansion. */
    std::optional<std::pair<std::string, std::uint64_t>>
        setup_resource_limit;
    /*
     * One owner for direct assertion, strict repair, publication
     * classification, packaging, and the continuation that makes those
     * stages observable. Compatibility references keep the established
     * coroutine body behavior-neutral while its state is no longer scattered
     * across SolveWork::Impl.
     */
    struct PublicationPipeline {
        std::uint64_t work_items = 0;
        std::uint32_t refinement_states = 0;
        std::uint32_t refinement_kernels = 0;
        std::uint64_t refinement_transitions = 0;
        std::uint32_t refinement_rounds = 0;
        std::uint32_t refinement_classes = 0;
        StrategyEvalProgress evaluation_progress;
        std::optional<solve_detail::CooperativeTask<SolveResult>> task;
        std::optional<SolveResult> result;
        bool consumed = false;
    } publication_pipeline;
    std::uint64_t& finalization_work_items =
        publication_pipeline.work_items;
    std::uint32_t& finalization_refinement_states =
        publication_pipeline.refinement_states;
    std::uint32_t& finalization_refinement_kernels =
        publication_pipeline.refinement_kernels;
    std::uint64_t& finalization_refinement_transitions =
        publication_pipeline.refinement_transitions;
    std::uint32_t& finalization_refinement_rounds =
        publication_pipeline.refinement_rounds;
    std::uint32_t& finalization_refinement_classes =
        publication_pipeline.refinement_classes;
    /* Best independently verified executable strategy observed while a
     * cooperative finalization child continues proving alternatives. */
    double& finalization_verified_upper_bound =
        incumbent_portfolio.finalization_verified_upper;
    StrategyEvalProgress& finalization_evaluation_progress =
        publication_pipeline.evaluation_progress;
    std::optional<solve_detail::CooperativeTask<SolveResult>>&
        finalization_task = publication_pipeline.task;
    std::optional<SolveResult>& finalized_result =
        publication_pipeline.result;
    bool& consumed = publication_pipeline.consumed;

    static std::uint64_t priced_operator_nested_bytes(
        const PricedOperator& priced);

    void initialize_owned_bytes_ledger();

    void retain_action_reason(std::string reason);

    Impl(
        CalcContext& context,
        const pc_item_state& start_item,
        const std::unordered_map<std::string, double>& prices,
        const SolveOptions& solve_options);
    ~Impl();

    solve_detail::CooperativeTask<SolveResult>
    run_publication_pipeline();
    bool can_prepare_requested_bounded_finish() const;
    void prepare_requested_bounded_finish();

    void begin_publication_pipeline();

    void advance_publication_pipeline();

    static void identity_mix(
        std::uint64_t& hash, const std::uint64_t value);

    static void identity_mix_string(
        std::uint64_t& hash, const std::string_view value);

    std::uint64_t goal_identity() const;

    std::uint64_t economy_identity() const;

    std::uint64_t caller_scope_identity() const;

    std::uint64_t action_vocabulary_prefix_identity(
        const std::size_t count) const;

    std::uint64_t action_vocabulary_identity() const;

    ExecutableContinuationAuthorityContext
    executable_continuation_authority_context() const;

    std::uint64_t graph_identity() const;
    std::uint64_t artifact_identity() const;
    std::uint64_t incumbent_graph_prefix_identity(
        std::uint64_t row_count,
        std::uint64_t priced_row_count,
        std::uint64_t successor_count,
        std::uint64_t probability_count,
        std::uint64_t choice_count,
        std::uint64_t choice_successor_count,
        std::uint64_t choice_option_count) const;
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

    FocusedFallbackWitness acquire_focused_fallback(bool& complete);

    void capture_incumbent_policy(BoundedPolicyIncumbent& candidate);

    void capture_incumbent_state(
        BoundedPolicyIncumbent& candidate,
        std::uint32_t state,
        std::uint64_t row);

    void populate_incumbent_policy(BoundedPolicyIncumbent& candidate);

    std::uint64_t incumbent_owned_bytes(
        const BoundedPolicyIncumbent& incumbent) const;

    std::uint64_t carrier_ladder_boundary_owned_bytes(
        const CarrierLadderBoundaryCapture& capture) const;

    void capture_carrier_ladder_exact_boundary(
        std::uint32_t target_state,
        const std::vector<double>& candidate_values,
        const std::vector<std::uint64_t>& candidate_policy_rows,
        const std::vector<std::uint8_t>& candidate_reachable,
        const std::vector<std::uint32_t>& certified_frontier_operators,
        std::vector<CarrierLadderBoundaryCapture::Stop> stops,
        CarrierLadderBoundaryCapture::RowServiceWitness row_service_witness,
        const char* refusal = nullptr);

    CarrierLadderBoundaryCapture::RowServiceWitness
    capture_carrier_ladder_row_service_witness(
        std::uint32_t state,
        const std::vector<std::uint8_t>& completed_rows,
        const std::vector<std::uint8_t>& ordinary_result_expanded,
        std::uint32_t certified_frontier_operator) const;

    static CarrierLadderBoundaryCapture::RowServiceDisposition
    classify_carrier_ladder_row_service_witness(
        const CarrierLadderBoundaryCapture::RowServiceWitness& witness);

    void bind_carrier_ladder_row_service_witness_identity(
        CarrierLadderBoundaryCapture::RowServiceWitness& witness,
        const CarrierLadderBoundaryCapture& capture) const;

    void refresh_carrier_ladder_terminal_row_service_witness();

    void refresh_carrier_ladder_exact_boundary_diagnostics(
        SolveDiagnostics& diagnostics) const;

    std::uint64_t fallback_policy_dynamic_owned_bytes(
        const FocusedFallbackPolicy& fallback) const;

    bool incumbent_precedes(
        const BoundedPolicyIncumbent& left,
        const BoundedPolicyIncumbent& right) const;

    const char* certified_incumbent_invalid_reason(
        const BoundedPolicyIncumbent& incumbent) const;

    const char* retained_incumbent_invalid_reason(
        const BoundedPolicyIncumbent& incumbent) const;

    bool certify_incumbent_for_fallback(
        BoundedPolicyIncumbent& incumbent);

    bool retain_certified_incumbent(
        const BoundedPolicyIncumbent& incumbent,
        std::uint64_t additional_live_bytes = 0);

    bool retain_current_certified_incumbent();

    BoundedPolicyIncumbent* best_current_certified_fallback();

    bool commit_output_incumbent(BoundedPolicyIncumbent candidate);

    void install_output_incumbent(
        const double upper,
        const std::vector<double>& values,
        const std::vector<std::uint64_t>& selected_rows,
        const std::vector<std::uint32_t>& frontier_operators,
        const FocusedFallbackWitness& fallback,
        std::string kind,
        const std::vector<std::uint8_t>* policy_reachable = nullptr,
        const PrimitiveRenewalWitness* primitive_renewal_witness = nullptr,
        bool replace_equal_incumbent = false,
        bool record_memory_refusal = false,
        bool prefer_materialized_over_unverified = false);

    void install_fallback_output_incumbent(
        const FocusedFallbackWitness& witness);

    void install_direct_output_incumbent(
        const double upper, const std::uint64_t row);

    bool try_install_reachable_incumbent(
        bool require_resource_stop,
        JointAnytimeAttemptLineage::Trigger trigger =
            JointAnytimeAttemptLineage::Trigger::IncrementalCheckpoint);

    solve_detail::JointPolicyContinuationContext
    current_joint_policy_continuation_context(
        const ResumableJointPolicyCandidateState* retained = nullptr) const;

    solve_detail::JointPolicyContinuationNode
    joint_policy_continuation_node(std::uint32_t state) const;

    bool joint_policy_row_completed(std::uint64_t row) const;

    std::uint64_t select_joint_policy_seed_row(
        std::uint32_t state,
        const std::vector<double>& selection_values) const;

    solve_detail::JointPolicySemanticKey joint_policy_row_semantic_key(
        std::uint32_t state,
        std::uint64_t row,
        const std::vector<double>& selection_values) const;

    solve_detail::JointPolicyContinuationResolvedState
    resolve_joint_policy_continuation_state(
        const solve_detail::JointPolicyContinuationNode& state,
        const ResumableJointPolicyCandidateState& retained) const;

    solve_detail::JointPolicyContinuationRefusal
    validate_joint_policy_continuation_decision(
        const solve_detail::JointPolicyContinuationDecision& decision,
        const ResumableJointPolicyCandidateState& retained) const;

    bool capture_resumable_joint_policy_candidate(
        std::uint32_t expected_missing_state,
        const std::vector<double>& selection_values,
        const std::vector<double>& certified_boundary_values,
        const std::vector<std::uint32_t>& certified_frontier_operators,
        const std::vector<std::uint8_t>& certified_boundary_reachable,
        const FocusedFallbackWitness& certified_fallback,
        const PrimitiveRenewalWitness& certified_renewal,
        std::uint64_t root_estimate_provenance);

    bool resumable_joint_policy_continuation_enabled() const {
        return options.carrier_ladder_exact_boundary_mode ==
                CarrierLadderExactBoundaryMode::ResumableContinuation ||
            (options.carrier_ladder_exact_boundary_mode ==
                 CarrierLadderExactBoundaryMode::Off &&
             options.high_impact_executable_uppers);
    }

    enum class ResumableJointPolicyAdvance : std::uint8_t {
        NoProgress = 0,
        Yielded,
        Complete,
        Released,
    };

    ResumableJointPolicyAdvance
    resume_joint_policy_candidate_if_ready();

    bool maybe_install_incremental_anytime_incumbent();

    void try_install_gated_root_renewal_incumbent(
        std::uint32_t state,
        std::uint64_t row,
        const PricedOperator& priced,
        const OutcomeDistribution& kernel);

    double certified_global_lower_bound() const;

    SolveGapTarget satisfied_gap_target() const;

    bool stop_for_satisfied_gap_target();

    bool ensure_priced_operator(const std::uint32_t index);

    std::uint32_t action_goal_reach_mask(
        const std::uint32_t action_index) const;

    std::uint32_t planner_goal_reach_mask(
        const std::uint32_t operator_index);

    std::uint32_t planner_goal_may_survive_mask(
        const std::uint32_t state,
        const std::uint32_t operator_index);

    void prepare_goal_cover_cost();

    std::uint32_t satisfied_goal_mask_for_state(
        const std::uint32_t state) const;

    bool restart_row_allowed(const std::uint32_t state) const;

    double optimistic_completion_cost(
        const std::uint32_t satisfied_mask,
        const bool clean_carrier = false,
        const std::uint8_t carrier_rarity = PC_RARITY_NORMAL,
        const std::uint8_t carrier_prefixes = 0,
        const std::uint8_t carrier_suffixes = 0);

    bool clean_goal_cover_eligible(const std::uint32_t state) const;

    std::uint32_t clean_goal_cover_rejection_mask(
        const std::uint32_t state) const;

    bool carrier_goal_progress_eligible(
        std::uint32_t state) const;

    double carrier_goal_progress_lower_value(
        std::uint32_t state) const;

    bool identity_clean_goal_progress_eligible(
        std::uint32_t state) const;

    double identity_clean_goal_progress_lower_value(
        std::uint32_t state) const;

    double carrier_terminal_debt_lower_value(
        std::uint32_t state) const;

    double completion_proof_lower_value(
        const std::uint32_t state);

    solve_detail::ProofLowerValue completion_proof_lower(
        const std::uint32_t state);

    double refresh_envelope_bellman_pattern();

    void prepare_strict_clean_goal_cover();

    double authored_fixed_program_cost_lower(
        const PlannerOperator& planner) const;

    double operator_proof_lower_value(
        const std::uint32_t state,
        const std::uint32_t operator_index,
        bool record_pattern_owners = true);

    double carrier_action_bellman_lower_value(
        std::uint32_t state,
        bool record_pattern_owners = true) const;

    solve_detail::ProofLowerValue operator_proof_lower(
        const std::uint32_t state,
        const std::uint32_t operator_index);

    std::size_t carrier_bound_operator_family(
        const std::uint32_t operator_index) const;

    void record_operator_lower_attribution(
        std::uint32_t operator_index,
        double lower,
        double incumbent,
        bool state_incumbent_prune,
        bool constructive_prune);

    void record_operator_lower_skip(
        std::uint32_t operator_index,
        CarrierBoundAttributionWork::OperatorLowerSkipReason reason);

    void record_carrier_schedule_attribution(
        CarrierBoundAttributionWork::ScheduleStage stage,
        std::uint32_t state,
        std::uint32_t operator_index = kNoId);

    void record_upper_attribution_milestone(
        double value,
        bool independently_verified);

    solve_detail::CooperativeTask<bool>
    finalize_carrier_bound_attribution();

    solve_detail::CarrierOrderingScore carrier_ordering_score(
        std::uint32_t state,
        double focused_priority = 0.0);

    void prepare_ordering_goal_masks();

    CarrierEffectSummary carrier_ordering_effect(
        std::uint32_t state,
        std::uint32_t operator_index);

    solve_detail::CarrierActionOrderingScore
    carrier_action_ordering_score(
        std::uint32_t state,
        std::uint32_t operator_index);

    bool cooperative_high_progress_ordering_enabled() const;

    void prioritize_carrier_actions(
        std::uint32_t state,
        std::vector<std::uint32_t>& operator_indices);

    std::optional<double> constructive_row_upper(
        const std::uint32_t state,
        const std::uint64_t row_index);

    bool try_constructive_state_certificate(
        const std::uint32_t state,
        const std::uint64_t row_index);

    ProductFractureKernel product_fracture_kernel(
        std::uint32_t state,
        std::uint32_t relevant_goal_mask);

    SolveTransitionCache::AutomaticCandidateRecord automatic_record_from(
        const std::uint32_t state,
        const StateLocalAutomaticCandidate& decision) const;

    bool prepare_state_expansion(
        const std::uint32_t state,
        bool include_state_local_automatic = true);

    bool advance_incremental_dynamic_preparation();

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

    bool schedule_next_incremental_alternative(
        bool continue_current_epoch = false);

    bool classify_incremental_alternatives();

    void begin_incremental_classification();

    bool advance_incremental_classification();

    double sparse_row_q_for_values(
        std::size_t row_index,
        const std::vector<double>& values) const;

    std::vector<double> certified_incremental_lower_values();

    bool schedule_incremental_refinement(bool force = false);

    bool schedule_warm_start_continuation_refinement();

    bool prepare_warm_start_policy_wave(std::uint32_t wave);

    bool begin_incremental_upper_policy_pass();

    void begin_incremental_post_upper_scheduling();

    bool advance_incremental_post_upper_scheduling();

    void refresh_incremental_upper_incumbent();

    void capture_initial_incremental_selected_policy();

    bool continue_open_incremental_envelope();

    bool retire_unmaterialized_by_operator_proof(
        std::uint32_t state,
        std::uint32_t operator_index);

    void audit_verified_incumbent_operator_proof_shadow(
        const BoundedPolicyIncumbent& incumbent);

    void retire_certified_unmaterialized_obligations();

    void restart_incremental_optimization();

    void finalize_incremental_diagnostics();

    void refresh_action_envelope_ledger_diagnostics(
        SolveDiagnostics& diagnostics) const;

    void refresh_anytime_scheduler_diagnostics(
        SolveDiagnostics& diagnostics) const;

    void refresh_operator_lineage_diagnostics(
        SolveDiagnostics& diagnostics,
        const SolveResult* published = nullptr) const;

    SolvePhaseOwner current_phase_owner() const;

    void refresh_incumbent_portfolio_diagnostics(
        SolveDiagnostics& diagnostics,
        const SolveResult* published = nullptr) const;

    void finalize_upper_policy_provenance();

    void finalize_upper_cap_zero_progress_audit();

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

    bool advance_focused_lower_preparation();

    bool collect_focused_fringe(
        std::vector<std::uint32_t>& fringe,
        std::vector<double>& priority,
        const std::vector<double>* gap_lower_values = nullptr,
        std::vector<std::uint8_t>* policy_reachable = nullptr);

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

    bool advance_primitive_destructive_renewal_fallback(
        std::optional<FocusedFallbackPolicy>& completed);

    std::optional<FocusedFallbackPolicy> progressive_fracture_fallback(
        const FocusedFallbackPolicy& bootstrap);

    std::optional<FocusedFallbackPolicy> focused_fallback(bool& complete);

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

    void abort_incremental_upper_policy_pass_for_bounded_finish();

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
