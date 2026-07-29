#ifndef POECRAFT_SRC_SOLVER_INTERNAL_HPP
#define POECRAFT_SRC_SOLVER_INTERNAL_HPP

#include <array>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "engine_internal.hpp"

/*
 * Solver phase S1 (docs/solver/crafting-solver-plan.md): action registry schema,
 * goal specification, abstract state layout, and junk-class derivation.
 *
 * Nothing here executes crafting actions. The registry describes the actions
 * the engine implements as data (costs, legality, transition kind,
 * discriminating tags) so the calculation engine (S2/S3) and the DP solver
 * (S4) consume mechanics without hardcoding them. The abstraction projects
 * the concrete item space onto only the features that change transition
 * probabilities or action legality for a given goal and candidate action set.
 */
namespace poecraft {
namespace solver {

inline constexpr std::uint32_t kNoId = std::numeric_limits<std::uint32_t>::max();
inline constexpr std::uint32_t kMaxGoalSlots = 8;
inline constexpr std::uint32_t kMaxDiscriminatingTags = 64;
inline constexpr std::size_t kMaxExplicitAffixes =
    PC_MAX_PREFIXES + PC_MAX_SUFFIXES;
inline constexpr std::uint32_t kDefaultImprintProgramDepth = 3;
inline constexpr std::uint64_t kDefaultImprintProgramWork = 256;

// --- goal specification ------------------------------------------------------

/*
 * One required mod on the finished item. Exactly one of group_id/family_id
 * identifies the slot, matching the strategy condition vocabulary
 * (HasModGroup / HasModFamily). min_tier is a family_tier_index threshold
 * (1 = best); 0 accepts any tier. Tier-perfect goals use min_tier = 1.
 */
struct GoalSlot {
    std::uint32_t group_id = kNoId;
    std::uint32_t family_id = kNoId;
    std::uint32_t min_tier = 0;
};

enum class FixedOptionKind : std::uint8_t {
    ScourAlchemy = 0,
    EldritchSideIntent = 1,
    ProtectedSide = 2,
    MultimodFinish = 3,
    Renewal = 4,
    ProtectedRepeat = 5,
    FracturePrepare = 6,
    ImprintRetry = 7,
    TemporaryBenchRepeat = 8,
};

enum class AutomaticCandidateKind : std::uint8_t {
    None = 0,
    Fracture = 1,
    PermanentBench = 2,
    TemporaryBenchBlocker = 3,
    ProtectedMetamod = 4,
    MultimodFinish = 5,
    Imprint = 6,
    ConstructiveRenewal = 7,
};

/* R3A retention/accounting categories. These are deliberately independent
 * of AutomaticCandidateKind: explicit renewal/fracture envelopes use the
 * same carrier-relative kernel representation even though they are not
 * native product candidates. */
enum class AutomaticTelemetryKind : std::uint8_t {
    Imprint = 0,
    Renewal = 1,
    ProtectedSide = 2,
    TemporaryBench = 3,
    FracturePrepare = 4,
    PermanentBench = 5,
    MultimodFinish = 6,
    PrimitiveFracture = 7,
    Count = 8,
    None = 255,
};

inline constexpr std::size_t kAutomaticTelemetryKindCount =
    static_cast<std::size_t>(AutomaticTelemetryKind::Count);

enum class PrimitiveTelemetryFamily : std::uint8_t {
    Currency = 0,
    Essence = 1,
    Fossil = 2,
    Harvest = 3,
    Bench = 4,
    Bestiary = 5,
    Fracture = 6,
    Other = 7,
    Count = 8,
};

inline constexpr std::size_t kPrimitiveTelemetryFamilyCount =
    static_cast<std::size_t>(PrimitiveTelemetryFamily::Count);

struct AutomaticKindTelemetry {
    std::uint64_t candidates = 0;
    std::uint64_t eligible_candidates = 0;
    std::uint64_t rejected_candidates = 0;
    std::uint64_t collapsed_candidates = 0;
    std::uint64_t deferred_candidates = 0;
    std::uint64_t missing_price_candidates = 0;
    std::uint64_t setup_rejections = 0;
    std::uint64_t neutral_kernel_rejections = 0;
    std::uint64_t relevance_rejections = 0;
    std::uint64_t cleanup_rejections = 0;
    std::uint64_t other_rejections = 0;
    std::uint64_t carriers = 0;
    std::uint64_t max_candidates_per_carrier = 0;
    std::uint64_t precompiled_classes = 0;
    std::uint64_t precompile_ns = 0;
    std::uint64_t precompiled_bytes = 0;
    std::uint64_t candidate_variants = 0;
    std::uint64_t max_candidate_variants_per_carrier = 0;
    std::uint64_t effect_classes = 0;
    std::uint64_t max_effect_classes_per_carrier = 0;
    std::uint64_t collapsed_variants = 0;
    std::uint64_t unique_templates = 0;
    std::uint64_t template_hits = 0;
    std::uint64_t max_templates_per_carrier = 0;
    std::uint64_t rows = 0;
    std::uint64_t max_rows_per_carrier = 0;
    std::uint64_t raw_outcomes = 0;
    std::uint64_t retained_transitions = 0;
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
    std::uint64_t enumeration_ns = 0;
    std::uint64_t row_ns = 0;
    std::uint64_t selected_bytes = 0;
};

struct AutomaticAdmissionPhaseTelemetry {
    std::uint64_t carriers = 0;
    std::uint64_t synthesis_ns = 0;
    std::uint64_t local_context_ns = 0;
    std::uint64_t local_planner_build_ns = 0;
    std::uint64_t local_layout_build_ns = 0;
    std::uint64_t local_ledger_init_ns = 0;
    std::uint64_t local_context_other_ns = 0;
};

struct PrimitiveFamilyTelemetry {
    std::uint64_t requests = 0;
    std::uint64_t cache_hits = 0;
    std::uint64_t rows = 0;
    std::uint64_t raw_outcomes = 0;
    std::uint64_t transitions = 0;
    std::uint64_t build_ns = 0;
    std::uint64_t row_ns = 0;
    std::uint64_t selected_bytes = 0;
};

enum AutomaticKernelMechanism : std::uint32_t {
    kAutomaticGroupConflict = 1u << 0,
    kAutomaticPrefixSlot = 1u << 1,
    kAutomaticSuffixSlot = 1u << 2,
    kAutomaticMetamodProtection = 1u << 3,
    kAutomaticCarrierFracture = 1u << 4,
    kAutomaticDeterministicFinish = 1u << 5,
    kAutomaticImprintCheckpoint = 1u << 6,
};

/*
 * User-selected, price-independent fixed option definition. Action ids name
 * ordinary registry primitives; the planner resolves them once and never
 * substitutes a cheaper setup or finish behind the caller's back.
 */
struct FixedOptionSpec {
    FixedOptionKind kind = FixedOptionKind::ScourAlchemy;
    std::int8_t side = -1; /* pc_mod_side for side-specific options */
    std::string action_id; /* Eldritch craft or protected operation */
    std::vector<std::string> setup_action_ids; /* explicit dominance setup */
    std::vector<std::string> bench_craft_ids;  /* explicit Multimod finish */
    /* S7.4 renewal programs. Unveil may appear only as the final observed
     * step. The exit predicate is a threshold over the named goal slots. */
    std::vector<std::string> program_action_ids;
    std::vector<std::uint32_t> exit_goal_slots;
    std::uint32_t exit_min_satisfied = 0;
    /* Automatic constructive renewals name the admitted deterministic
     * primitive that finishes a successful intermediate carrier. It is
     * metadata for the executable upper policy, not part of the renewal
     * attempt kernel itself. */
    std::string constructive_finish_action_id;
    /* Fracture preparation waits for this exact satisfying goal carrier.
     * Wrong-carrier fracture outcomes remain outer-policy exits. */
    std::uint32_t carrier_goal_slot = kNoId;
    /* S8.3 native synthesis metadata. Automatic programs use the same
     * bounded fixed-option vocabulary as explicit S7 programs. */
    AutomaticCandidateKind automatic_kind = AutomaticCandidateKind::None;
    std::uint32_t relevant_goal_mask = 0;
};

struct GoalSpec {
    std::vector<GoalSlot> slots; /* 1..kMaxGoalSlots */
    std::uint8_t rarity = PC_RARITY_RARE; /* required finished rarity */
    /* Minimum slots that must be satisfied together. Zero is the internal
     * default and means every slot, preserving callers that construct a
     * GoalSpec directly instead of going through the JSON parser. */
    std::uint32_t min_satisfied_slots = 0;
    bool primitive_actions_explicit = false;
    /* Product goal-relevant solves enable native S8.3 candidate synthesis.
     * Explicit historical/manual goal documents remain unchanged. */
    bool automatic_candidates = false;
    std::vector<FixedOptionSpec> fixed_options;

    std::size_t required_satisfied_slots() const {
        return min_satisfied_slots == 0 ? slots.size()
                                        : min_satisfied_slots;
    }
};

// --- action registry ---------------------------------------------------------

enum class TransitionKind : std::uint8_t {
    Deterministic = 0, /* probability-1 successor (scour, bench, restart) */
    SingleSlot = 1,    /* one weighted-pool pick (exalt, aug, annul, ...) */
    Reforge = 2,       /* sequential multi-mod roll (chaos, essence, ...) */
    Special = 3        /* bespoke enumerator (unveil choice, fracture, ...) */
};

/*
 * Abstract mechanic flags. Shared vocabulary between legality predicates and
 * the abstract state so legality never needs the concrete item.
 */
enum AbstractFlag : std::uint32_t {
    kFlagCorrupted = 1u << 0,
    kFlagMirrored = 1u << 1,
    kFlagSplit = 1u << 2,
    kFlagSynthesised = 1u << 3,
    kFlagFractured = 1u << 4,       /* any fractured explicit slot */
    kFlagCraftedMod = 1u << 5,      /* any bench-crafted explicit slot */
    kFlagVeiledMod = 1u << 6,       /* an unresolved veiled explicit slot */
    kFlagMultimod = 1u << 7,
    kFlagNoAttack = 1u << 8,
    kFlagNoCaster = 1u << 9,
    kFlagPrefixesLocked = 1u << 10,
    kFlagSuffixesLocked = 1u << 11,
    kFlagInfluenced = 1u << 12,     /* any generic influence bit set */
    kFlagEldritchImplicit = 1u << 13 /* either eldritch implicit tier set */
};

/*
 * Declarative legality over abstract features. Mirrors the precondition
 * checks in apply_action (actions_basic.cpp); that dispatch stays the
 * execution authority, this is the planning view of it.
 */
struct LegalityPredicate {
    std::uint8_t rarity_mask = 0x7; /* bit (1 << pc_rarity) allowed */
    std::uint32_t forbidden_flags = kFlagCorrupted | kFlagMirrored;
    std::uint32_t required_flags = 0;
    bool requires_open_affix = false;
    bool requires_removable_affix = false;
    bool requires_session_eldritch = false;
    std::uint8_t min_total_affixes = 0;
};

/* Symbolic carrier vocabulary for S8.2 preservation control. Descriptor
 * masks are conservative action capabilities; state-local witnesses refine
 * them to exact goal-slot masks, counts, flags, and transition outcomes. */
enum CarrierProperty : std::uint32_t {
    kCarrierGoalFamilies = 1u << 0,
    kCarrierSatisfiedGoalSubset = 1u << 1,
    kCarrierJunkBlockers = 1u << 2,
    kCarrierCraftedState = 1u << 3,
    kCarrierFracturedState = 1u << 4,
    kCarrierPrefixSide = 1u << 5,
    kCarrierSuffixSide = 1u << 6,
    kCarrierActiveProtection = 1u << 7,
};

inline constexpr std::uint32_t kAllCarrierProperties =
    kCarrierGoalFamilies | kCarrierSatisfiedGoalSubset |
    kCarrierJunkBlockers | kCarrierCraftedState |
    kCarrierFracturedState | kCarrierPrefixSide | kCarrierSuffixSide |
    kCarrierActiveProtection;

struct ActionPreservationMetadata {
    std::uint32_t can_preserve = 0;
    std::uint32_t can_destroy = 0;
    std::uint32_t can_create = 0;
    std::uint32_t can_make_unreachable = 0;
    bool destructive_renewal = false;
    bool preserves_fractured_affixes = false;
    bool respects_prefix_lock = false;
    bool respects_suffix_lock = false;
    bool respects_cannot_roll_attack = false;
    bool respects_cannot_roll_caster = false;
};

/*
 * One plannable action instance. Parameterized mechanics enumerate one
 * descriptor per parameter value (per essence, per fossil, per Harvest tag,
 * per bench mod, per influence) so the solver's action set is a flat list.
 */
struct ActionDescriptor {
    std::string id; /* canonical: strategy operation vocabulary, e.g.
                       "chaos", "essence:<key>", "harvest_reforge:<tag>" */
    std::string display_name;
    bool synthetic = false; /* restart: not executable by apply_action */
    ActionParameters params;
    TransitionKind kind = TransitionKind::Deterministic;
    /* Currency-quantity cost vector: one entry per unit consumed, using the
     * simulator economy price-key vocabulary. Duplicate keys mean quantity. */
    std::vector<std::string> cost_keys;
    LegalityPredicate legality;
    /* Classification tag ids this action's weighting or targeting
     * discriminates on. Drives junk-class derivation: mods differing only in
     * tags no candidate action reads collapse into one class. Sorted. */
    std::vector<std::uint32_t> discriminating_tag_ids;
    std::uint32_t sets_flags = 0;   /* AbstractFlag bits the action can set */
    std::uint32_t clears_flags = 0; /* AbstractFlag bits it can clear */
    ActionPreservationMetadata preservation;
    /* Reserved for future two-item techniques (imprint/recombinator inside
     * the item-level solver). Nothing planned sets it; see plan. */
    bool uses_companion_state = false;
    /* Retained in a goal-relevant registry only as an automatic option
     * dependency. It participates in abstraction and exact evaluation but is
     * not independently selectable unless the caller explicitly names it. */
    bool automatic_dependency_only = false;
};

struct ActionRegistry {
    std::vector<ActionDescriptor> actions;
    std::unordered_map<std::string, std::uint32_t> index_by_id;
    std::uint32_t goal_relevant_actions_pruned = 0;
    std::uint32_t fossil_loadouts_possible = 0;
    std::uint32_t fossil_loadouts_generated = 0;
    std::uint32_t fossil_loadouts_deferred = 0;
    bool fossil_generation_lazy = false;
    bool fossil_generation_goal_relevant = false;
};

struct ActionRegistryBuildOptions {
    /* Full enumeration remains available for explicit exhaustive diagnostics.
     * Explicit action envelopes provide only the fossil ids they request.
     * Product solves ask the engine for a bounded goal-relevant beam instead
     * of enumerating the complete 1-4 fossil powerset. */
    bool exhaustive_fossils = true;
    bool goal_relevant_fossils = false;
    bool goal_relevant_actions = false;
    std::vector<std::string> requested_fossil_action_ids;
    std::vector<std::string> option_dependency_action_ids;
    bool needs_prefix_lock = false;
    bool needs_suffix_lock = false;
    bool needs_multimod = false;
    bool needs_fracture = false;
    bool automatic_candidates = false;
    std::vector<std::vector<std::uint32_t>> fossil_goal_mod_ids;
};

// --- solver-only planner operators -------------------------------------------

enum class PlannerOperatorKind : std::uint8_t {
    Primitive = 0,
    FixedOption = 1,
};

/*
 * A primitive wrapper or one finite fixed option. Primitive wrappers occupy
 * the same indices as ActionRegistry::actions for backwards-compatible policy
 * ids; fixed options are appended and therefore remain explicitly tagged by
 * kind. Option programs contain only ordinary primitive registry indices.
 */
struct PlannerOperator {
    std::string id;
    std::string display_name;
    PlannerOperatorKind kind = PlannerOperatorKind::Primitive;
    FixedOptionKind option_kind = FixedOptionKind::ScourAlchemy;
    std::uint32_t primitive_action = kNoId;
    std::vector<std::uint32_t> primitive_program;
    std::int8_t intended_side = -1;
    std::vector<std::uint32_t> exit_goal_slots;
    std::uint32_t exit_min_satisfied = 0;
    std::uint32_t carrier_goal_slot = kNoId;
    std::uint32_t conditional_action = kNoId; /* Fracture after preparation */
    std::uint32_t bestiary_create_action = kNoId;
    std::uint32_t bestiary_restore_action = kNoId;
    AutomaticCandidateKind automatic_kind = AutomaticCandidateKind::None;
    std::uint32_t relevant_goal_mask = 0;
    std::uint32_t setup_action = kNoId;
    std::uint32_t followup_action = kNoId;
    std::uint32_t cleanup_action = kNoId;
    std::uint32_t constructive_finish_action = kNoId;
    /* Sorted dependency quantities. S7.3 fixed programs execute each entry
     * exactly once; S7.4 kernels return the exact state-dependent expected
     * quantities used for pricing conditional and observed paths. */
    std::vector<std::pair<std::string, double>> resource_quantities;
};

/* Selected policy reference. The index resolves in CalcContext::operators();
 * the explicit kind tag prevents an appended option from masquerading as a
 * primitive registry action. Primitive indices remain numerically identical
 * to their ActionRegistry index. */
struct PolicyOperatorRef {
    PlannerOperatorKind kind = PlannerOperatorKind::Primitive;
    std::uint32_t index = kNoId;

    PolicyOperatorRef() = default;
    explicit PolicyOperatorRef(std::uint32_t value) : index(value) {}
    PolicyOperatorRef(PlannerOperatorKind value_kind, std::uint32_t value)
        : kind(value_kind), index(value) {}

    operator std::uint32_t() const { return index; }
    bool operator==(const PolicyOperatorRef&) const = default;
    bool operator==(std::uint32_t value) const { return index == value; }
    friend bool operator==(
        std::uint32_t value,
        const PolicyOperatorRef& reference) {
        return reference == value;
    }
};

/*
 * Enumerate every plannable action instance for this session: base currency
 * operations, session-available essences, single fossils, bench crafts and
 * metamods, veiled/unveil, Harvest per-tag actions, eldritch actions,
 * influenced exalts, fracture, and the synthetic restart action. Mechanics
 * the engine build does not support for this session are simply absent.
 */
ActionRegistry build_action_registry(
    const SessionImpl& session,
    const ActionRegistryBuildOptions& options = {});

/* Resolve the goal's explicitly selected fixed options. The returned vector
 * begins with one primitive wrapper per registry action at the same index. */
std::vector<PlannerOperator> build_planner_operators(
    const SessionImpl& session,
    const GoalSpec& goal,
    const ActionRegistry& registry,
    const std::vector<std::uint32_t>& admitted_primitives);

// --- goal resolution and abstract layout --------------------------------------

struct ResolvedGoalSlot {
    GoalSlot spec;
    std::vector<std::uint64_t> member_mask;     /* any-tier group/family mods */
    std::vector<std::uint64_t> satisfying_mask; /* members at/above min_tier */
    /* Exclusivity groups covering the satisfying mods; a non-member explicit
     * occupying any of them blocks this slot. Sorted. */
    std::vector<std::uint32_t> blocking_group_ids;
};

enum class GoalSlotStatus : std::uint8_t {
    Absent = 0,
    PresentBelowTier = 1,
    Satisfied = 2
};

/*
 * One junk equivalence class. Two non-goal mods share a class iff every
 * candidate action treats them identically: same generation side, same
 * restricted tag signature (classification tags intersected with the
 * candidate actions' discriminating tags), blocking the same goal slots, and
 * (when relevant) the same veiled-template role.
 */
struct JunkClass {
    std::int8_t gen_type = 0;           /* 0 prefix, 1 suffix */
    std::uint64_t tag_bits = 0;         /* over discriminating_tag_ids order */
    std::uint32_t goal_block_mask = 0;  /* bit per goal slot index */
    /* Exact-evaluation layouts additionally partition junk by the complete
     * session-mod mask excluded by its group memberships. Empty for the
     * solver's legacy compact abstraction. */
    std::vector<std::uint64_t> exclusion_effect_mask;
    /* Bit i is set when every member of this exact-evaluation class belongs
     * to count_observations[i]. The observation signature is part of the
     * class key, so membership is constant across the class. */
    std::vector<std::uint64_t> count_observation_bits;
    std::vector<std::uint64_t> member_mask;
    std::uint32_t member_count = 0;
};

/* One distinct modifier-set membership query used by compiled mod_count or
 * mod_family_count conditions. Count thresholds and required slot flags do
 * not affect the partition and therefore share one observation. */
struct CountObservation {
    bool by_family = false;
    std::vector<std::uint32_t> ids;
    /* Compiled condition count-memo slots sharing this membership set. */
    std::vector<std::uint32_t> memo_slots;
    std::vector<std::uint64_t> member_mask;
    std::vector<std::uint32_t> junk_class_indices;
    /* Exact contribution of a present goal member for each slot/status. */
    std::array<std::array<std::uint8_t, 3>, kMaxGoalSlots>
        goal_status_counts{};
};

struct AbstractLayout {
    std::vector<ResolvedGoalSlot> slots;
    /* Union of the candidate actions' discriminating tags, sorted. Bit i of
     * JunkClass::tag_bits corresponds to discriminating_tag_ids[i]. */
    std::vector<std::uint32_t> discriminating_tag_ids;
    /* Exact compiled-strategy count observations. Empty in normal solves. */
    std::vector<CountObservation> count_observations;
    /* count_memo_slot -> count_observations index, or kNoId. */
    std::vector<std::uint32_t> count_observation_by_memo_slot;
    /* Deterministic order: sorted by (gen_type, tag_bits, goal_block_mask,
     * veiled-template role, exclusion_effect_mask). */
    std::vector<JunkClass> junk_classes;
    /* session mod id -> junk class index, or kNoId for goal members and mods
     * no candidate action can place in an explicit slot. */
    std::vector<std::uint32_t> junk_class_by_mod;
};

/* Exact/superset engine-owned set of explicit affix mods that one action can
 * place. This is the same producibility authority used to construct the
 * abstract layout; callers may use an empty intersection as an exact proof
 * that an action cannot directly satisfy a goal partition. */
std::vector<std::uint64_t> action_explicit_affix_reachable_mask(
    const SessionImpl& session,
    const ActionDescriptor& action);

/*
 * Derive the abstract state layout for (goal, candidate action subset).
 * action_indices index into registry.actions; an empty vector means the full
 * registry unless empty_actions_mean_all is false. Throws std::runtime_error
 * on an invalid goal (no slots unless explicitly allowed, too many slots,
 * unknown group/family, overlapping slot member masks) or when the
 * discriminating tag union exceeds kMaxDiscriminatingTags.
 */
AbstractLayout build_abstract_layout(
    const SessionImpl& session,
    const GoalSpec& goal,
    const ActionRegistry& registry,
    const std::vector<std::uint32_t>& action_indices,
    bool allow_empty_goal = false,
    bool empty_actions_mean_all = true,
    bool distinguish_junk_exclusion_effects = false,
    const std::vector<CountObservation>& count_observations = {});

// --- abstract state -----------------------------------------------------------

/*
 * Logical byte vector whose non-zero entries are stored inline. An item can
 * carry at most six explicit affixes, so a junk-count vector can never have
 * more than six non-zero classes even when the abstract layout contains more
 * than one hundred classes. This keeps AbstractState trivially self-contained
 * and removes four heap allocations per interned state while retaining the
 * vector-like indexing used by the exact evaluators.
 */
class CompactCountVector {
  public:
    class Reference {
      public:
        Reference(CompactCountVector& owner, std::size_t index)
            : owner_(owner), index_(index) {}
        operator std::uint8_t() const { return owner_.get(index_); }
        Reference& operator=(std::uint8_t value) {
            owner_.set(index_, value);
            return *this;
        }
        Reference& operator+=(std::uint8_t value) {
            owner_.set(index_, static_cast<std::uint8_t>(
                owner_.get(index_) + value));
            return *this;
        }
        Reference& operator++() {
            *this += 1;
            return *this;
        }

      private:
        CompactCountVector& owner_;
        std::size_t index_;
    };

    class ConstIterator {
      public:
        ConstIterator(const CompactCountVector& owner, std::size_t index)
            : owner_(&owner), index_(index) {}
        std::uint8_t operator*() const { return owner_->get(index_); }
        ConstIterator& operator++() {
            ++index_;
            return *this;
        }
        bool operator!=(const ConstIterator& other) const {
            return index_ != other.index_;
        }

      private:
        const CompactCountVector* owner_;
        std::size_t index_;
    };

    void assign(std::size_t size, std::uint8_t value) {
        if (size > std::numeric_limits<std::uint16_t>::max()) {
            throw std::length_error("solver junk-class vector is too large");
        }
        logical_size_ = static_cast<std::uint16_t>(size);
        entry_count_ = 0;
        entries_ = {};
        if (value != 0) {
            if (size > entries_.size()) {
                throw std::length_error(
                    "dense non-zero junk payload exceeds affix capacity");
            }
            for (std::size_t i = 0; i < size; ++i) {
                entries_[entry_count_++] = {
                    static_cast<std::uint16_t>(i), value};
            }
        }
    }

    std::size_t size() const { return logical_size_; }
    std::size_t capacity() const { return 0; }
    Reference operator[](std::size_t index) {
        return Reference(*this, index);
    }
    std::uint8_t operator[](std::size_t index) const { return get(index); }
    ConstIterator begin() const { return ConstIterator(*this, 0); }
    ConstIterator end() const { return ConstIterator(*this, logical_size_); }

    bool operator==(const CompactCountVector& other) const {
        if (logical_size_ != other.logical_size_ ||
            entry_count_ != other.entry_count_) {
            return false;
        }
        for (std::size_t i = 0; i < entry_count_; ++i) {
            if (entries_[i].index != other.entries_[i].index ||
                entries_[i].value != other.entries_[i].value) {
                return false;
            }
        }
        return true;
    }
    bool operator==(const std::vector<std::uint8_t>& other) const {
        if (other.size() != logical_size_) return false;
        for (std::size_t i = 0; i < other.size(); ++i) {
            if (get(i) != other[i]) return false;
        }
        return true;
    }

  private:
    struct Entry {
        std::uint16_t index = 0;
        std::uint8_t value = 0;
    };

    std::uint8_t get(std::size_t index) const {
        if (index >= logical_size_) throw std::out_of_range("junk count");
        for (std::size_t i = 0; i < entry_count_; ++i) {
            if (entries_[i].index == index) return entries_[i].value;
            if (entries_[i].index > index) break;
        }
        return 0;
    }

    void set(std::size_t index, std::uint8_t value) {
        if (index >= logical_size_) throw std::out_of_range("junk count");
        std::size_t position = 0;
        while (position < entry_count_ && entries_[position].index < index) {
            ++position;
        }
        if (position < entry_count_ && entries_[position].index == index) {
            if (value != 0) {
                entries_[position].value = value;
                return;
            }
            for (std::size_t i = position + 1; i < entry_count_; ++i) {
                entries_[i - 1] = entries_[i];
            }
            entries_[--entry_count_] = {};
            return;
        }
        if (value == 0) return;
        if (entry_count_ >= entries_.size()) {
            throw std::length_error(
                "sparse junk payload exceeds explicit-affix capacity");
        }
        for (std::size_t i = entry_count_; i > position; --i) {
            entries_[i] = entries_[i - 1];
        }
        entries_[position] = {
            static_cast<std::uint16_t>(index), value};
        ++entry_count_;
    }

    std::array<Entry, kMaxExplicitAffixes> entries_{};
    std::uint16_t logical_size_ = 0;
    std::uint8_t entry_count_ = 0;
};

struct AbstractState {
    std::array<std::uint8_t, kMaxGoalSlots> slot_status{}; /* GoalSlotStatus */
    /* Exact carrier identity for slot-local mechanics. Bit i means the
     * modifier occupying goal slot i carries the corresponding slot flag. */
    std::uint32_t fractured_goal_mask = 0;
    std::uint32_t crafted_goal_mask = 0;
    std::uint32_t blocked_mask = 0; /* bit per goal slot: a non-member
                                       explicit occupies a blocking group */
    std::uint8_t prefix_count = 0;
    std::uint8_t suffix_count = 0;
    std::uint8_t rarity = PC_RARITY_NORMAL;
    std::uint8_t influence_bits = 0;
    /* Exact mechanic routing state. Veiled side is -1/0/1 for none/prefix/
     * suffix; eldritch tiers determine which explicit side the dominance
     * currencies modify. */
    std::int8_t veiled_side = -1;
    std::uint8_t searing_exarch_tier = 0;
    std::uint8_t eater_of_worlds_tier = 0;
    std::uint32_t flags = 0; /* AbstractFlag bits */
    /* Subset of active metamod AbstractFlag bits whose concrete affix is
     * fractured. Metamods may be queried on an input item even when no
     * candidate action can create them, so this identity cannot depend on
     * the action-reachable junk partition. */
    std::uint32_t fractured_metamod_flags = 0;
    /* Virtual Bellman identity for the opt-in zero-progress-reroll policy.
     * Materialization is the exact preserved physical boundary, but ordinary
     * salvage actions are not admitted from this state. */
    std::uint8_t goal_progress_retry_basin = 0;
    CompactCountVector junk_counts; /* logical size: layout.junk_classes */
    CompactCountVector fractured_junk_counts;
    CompactCountVector crafted_junk_counts;
    /* Required only for exact reconstruction when one carrier has both
     * flags; always bounded by both corresponding per-class counts. */
    CompactCountVector fractured_crafted_junk_counts;

    bool operator==(const AbstractState& other) const = default;
};

/* Project a concrete item onto the layout's abstract features. */
AbstractState project_item(
    const SessionImpl& session,
    const AbstractLayout& layout,
    const pc_item_state& item);

std::size_t abstract_state_hash(const AbstractState& state);

/* Evaluate a descriptor's legality against an abstract state. */
bool action_legal(
    const SessionImpl& session,
    const ActionDescriptor& action,
    const AbstractState& state);

/* Shared affix-cap rule used by exact transition and graph-condition
 * evaluation. */
std::uint8_t rarity_affix_cap(
    const SessionImpl& session,
    std::uint8_t rarity);

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
        const std::vector<CountObservation>& count_observations = {});

    const SessionImpl& session() const { return *session_; }
    const AbstractLayout& layout() const { return layout_; }
    const ActionRegistry& registry() const { return registry_; }
    const std::vector<PlannerOperator>& operators() const {
        return operators_;
    }
    const GoalSpec& goal() const { return goal_; }
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
        bool reserve_storage = true);
    void consume_reforge_work(std::uint64_t amount);
    void record_primitive_row_time(
        std::uint32_t action_index,
        std::uint64_t elapsed_ns);
    void release_solve_transition_caches();
    void release_outcome(
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

// --- exact compiled-strategy evaluator ---------------------------------------

struct StrategyEvalOptions {
    double epsilon = 1e-12;
    std::uint32_t max_sweeps = 100000;
    std::uint32_t max_states = 100000;
    std::uint32_t max_pairs = 1000000;
    std::uint32_t max_transitions = 10000000;
    std::uint32_t top_classes_per_node = 16;
    std::shared_ptr<const EconomyImpl> economy;
    std::string review_projection_json;
    bool include_success_normalized = false;
    std::uint64_t max_owned_bytes = 536870912;
    std::uint64_t max_output_json_bytes = 67108864;
};

enum class StrategyEvalPhase {
    Discovery,
    Solving,
    Fallback,
    Finalization,
    Done,
};

struct StrategyEvalProgress {
    StrategyEvalPhase phase = StrategyEvalPhase::Discovery;
    bool done = false;
    std::uint64_t discovered_pairs = 0;
    std::uint64_t pending_pairs = 0;
    std::uint64_t solved_sccs = 0;
    std::uint64_t total_sccs = 0;
    std::uint64_t fallback_sweeps = 0;
    double residual = 0.0;
};

struct StrategyEvalClass {
    double share = 0.0;
    AbstractState state;
};

struct StrategyEvalNode {
    std::string id;
    double expected_visits = 0.0;
    std::vector<StrategyEvalClass> classes;
    double classes_truncated_share = 0.0;
};

struct StrategyEvalEdge {
    std::string id;
    double expected_traversals = 0.0;
};

struct StrategyEvalTerminalNode {
    std::string node_id;
    int terminal_kind = PC_TERMINAL_FAILURE;
    double probability = 0.0;
};

struct StrategyEvalNodeMass {
    std::string node_id;
    double mass = 0.0;
};

struct StrategyEvalFailure {
    std::string node_id;
    std::string reason;
    double probability = 0.0;
};

struct StrategyEvalActionNode {
    std::string node_id;
    double expected_visits = 0.0;
    double expected_applied = 0.0;
};

struct StrategyEvalActionRegion {
    std::uint32_t goal_progress = 0;
    std::uint8_t rarity = PC_RARITY_NORMAL;
    std::uint32_t blocker_count = 0;
    std::uint32_t crafted_count = 0;
    std::uint32_t fractured_goal_mask = 0;
    std::uint32_t fractured_count = 0;
    std::uint32_t reachable_states = 0;
    double expected_visits = 0.0;
    double expected_applied = 0.0;
};

struct StrategyEvalActionTotal {
    std::string id;
    std::string display_name;
    std::vector<std::string> price_keys;
    double expected_visits = 0.0;
    double expected_applied = 0.0;
    std::vector<std::string> classifications;
    std::vector<StrategyEvalActionNode> nodes;
    std::uint32_t reachable_states = 0;
    std::vector<StrategyEvalActionRegion> regions;
};

struct StrategyEvalMaterialTotal {
    std::string price_key;
    double expected_quantity = 0.0;
    bool priced = false;
    double unit_price = 0.0;
    double cost_contribution = 0.0;
};

struct StrategyEvalReviewSection {
    std::string id;
    std::string label;
    std::string role;
    std::vector<std::string> raw_node_ids;
    std::vector<std::string> raw_edge_ids;
    double expected_actions = 0.0;
    double expected_edge_traversals = 0.0;
    double known_expected_cost = 0.0;
    bool cost_complete = false;
    double total_expected_cost = 0.0;
    std::vector<StrategyEvalActionTotal> actions;
    std::vector<StrategyEvalMaterialTotal> materials;
    std::map<std::string, double> techniques;
};

/* Retained exact state-action occupancy. `state` indexes occupancy_states and
 * `node`/`action` preserve the deterministic compiled-policy control choice.
 * Reward is the immediate priced cost of one applied action; the expected
 * contribution is expected_applied * immediate_reward. These records remain
 * internal until B4 adds the action-utility report surface. */
struct StrategyEvalOccupancyEntry {
    std::uint32_t state = kNoId;
    std::uint32_t node = kNoId;
    std::uint32_t action = kNoId;
    double expected_visits = 0.0;
    double expected_applied = 0.0;
    double immediate_reward = 0.0;
    bool reward_complete = false;
};

struct StrategyEvalResult {
    bool converged = false;
    std::uint32_t sweeps = 0;
    double residual_mass = 0.0;
    double success_probability = 0.0;
    double failure_probability = 0.0;
    double stop_probability = 0.0;
    double action_not_applied_probability = 0.0;
    double no_matching_edge_probability = 0.0;
    double unresolved_probability = 0.0;
    double expected_actions = 0.0;
    std::map<std::string, double> expected_consumption;
    std::vector<StrategyEvalActionTotal> action_totals;
    std::vector<StrategyEvalMaterialTotal> material_totals;
    std::map<std::string, double> technique_totals;
    std::vector<StrategyEvalReviewSection> review_sections;
    bool review_sections_enabled = false;
    bool pricing_enabled = false;
    std::string economy_id;
    bool cost_complete = false;
    double known_expected_cost = 0.0;
    double total_expected_cost = 0.0;
    bool success_normalized_enabled = false;
    double action_descriptor_visits_difference = 0.0;
    double action_descriptor_applied_difference = 0.0;
    double node_operation_visits_difference = 0.0;
    std::map<std::string, double> material_quantity_differences;
    double cost_dot_product_difference = 0.0;
    double section_actions_difference = 0.0;
    std::map<std::string, double> section_material_differences;
    std::vector<GoalSlot> targets;
    std::vector<StrategyEvalTerminalNode> terminal_nodes;
    std::vector<StrategyEvalNodeMass> unresolved_by_node;
    std::vector<StrategyEvalFailure> failures_by_node;
    std::vector<StrategyEvalNode> nodes;
    std::vector<StrategyEvalEdge> edges;
    std::vector<AbstractState> occupancy_states;
    std::vector<StrategyEvalOccupancyEntry> occupancy;
    double occupancy_expected_reward = 0.0;
    double occupancy_reward_difference = 0.0;
    bool occupancy_reward_complete = false;
    /* White-box diagnostic for the per-sweep conservation property. */
    double max_mass_conservation_error = 0.0;
    std::uint64_t owned_bytes_estimate = 0;
    std::uint64_t peak_owned_bytes_estimate = 0;
    std::uint64_t max_owned_bytes = 0;
    std::uint64_t max_output_json_bytes = 0;
};

class StrategyEvalUnsupported : public std::runtime_error {
  public:
    explicit StrategyEvalUnsupported(const std::string& message)
        : std::runtime_error(message) {}
};

class StrategyEvalWork {
  public:
    StrategyEvalWork(
        std::shared_ptr<const StrategyImpl> strategy,
        const StrategyEvalOptions& options = {});
    ~StrategyEvalWork();
    StrategyEvalWork(StrategyEvalWork&&) noexcept;
    StrategyEvalWork& operator=(StrategyEvalWork&&) noexcept;
    StrategyEvalWork(const StrategyEvalWork&) = delete;
    StrategyEvalWork& operator=(const StrategyEvalWork&) = delete;

    void step(std::uint32_t max_work_items);
    StrategyEvalProgress progress() const;
    const StrategyEvalResult& result() const;
    std::uint64_t live_owned_bytes() const;
    std::uint64_t peak_owned_bytes() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    friend StrategyEvalResult evaluate_strategy_forward_reference_for_test(
        const StrategyImpl& strategy,
        const StrategyEvalOptions& options);
};

/* Resolve one compiled operation to its registry descriptor. kNoId means no
 * exact parameter match exists. Restart resolves to the synthetic descriptor. */
std::uint32_t resolve_strategy_action(
    const StrategyNode& node,
    const ActionRegistry& registry);

/* Abstract-state counterpart of the simulator's compiled condition
 * predicate. The evaluation derivation guarantees all referenced targets are
 * present in layout. */
bool evaluate_abstract_condition(
    const CompiledCondition& condition,
    const SessionImpl& session,
    const AbstractLayout& layout,
    const AbstractState& state);

StrategyEvalResult evaluate_strategy(
    const StrategyImpl& strategy,
    const StrategyEvalOptions& options = {});

/* Test-only numerical oracle: discovers the same finite pair graph, then uses
 * high-precision whole-graph forward propagation instead of the production
 * SCC solver. */
StrategyEvalResult evaluate_strategy_forward_reference_for_test(
    const StrategyImpl& strategy,
    const StrategyEvalOptions& options = {});

std::string serialize_strategy_eval(const StrategyEvalResult& result);

// --- DP solver core (S4) --------------------------------------------------------

struct SolveOptions {
    double epsilon = 1e-9;          /* max Bellman residual, cost units */
    std::uint32_t max_states = 200000;
    std::uint32_t max_sweeps = 100000;
    std::uint32_t max_discovered_states = 200000;
    std::uint32_t max_expanded_states = 200000;
    std::uint64_t max_state_action_rows = 1215000;
    std::uint64_t max_transitions = 10000000;
    std::uint64_t max_reforge_work = 11000000;
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

struct SolveDiagnostics {
    /* Actions the solve planned without, and why. */
    std::vector<std::string> skipped_missing_price;
    std::vector<std::string> skipped_unsupported;
    std::uint64_t skipped_missing_price_count = 0;
    std::uint64_t skipped_unsupported_count = 0;
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
    std::vector<std::string> incremental_action_witnesses;
    std::uint64_t incremental_action_witnesses_omitted = 0;
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

// --- policy -> strategy graph compiler (S5) --------------------------------------

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
    PolicyCompilationTelemetry* telemetry = nullptr);

} // namespace solver
} // namespace poecraft

#endif
