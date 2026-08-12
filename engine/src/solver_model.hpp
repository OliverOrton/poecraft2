#pragma once
#include <algorithm>
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

namespace refinement {
struct RefinedPolicyCompileRouting;
}

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
    EldritchSide = 8,
    CannotRoll = 9,
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
    EldritchSide = 8,
    CannotRoll = 9,
    Count = 10,
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
    /* Retained as an additive compatibility bucket. Every current ordinary
     * registry descriptor maps to a more specific family below. */
    Other = 7,
    EldritchSetup = 8,
    EldritchChaos = 9,
    EldritchAnnul = 10,
    EldritchExalt = 11,
    InfluenceExalt = 12,
    VeiledChaos = 13,
    VeiledExalt = 14,
    Unveil = 15,
    Count = 16,
};

inline constexpr std::size_t kPrimitiveTelemetryFamilyCount =
    static_cast<std::size_t>(PrimitiveTelemetryFamily::Count);

/* Product goal filtering classifies every native primitive into exactly one
 * of these roles. Candidate and dependency rows remain in the registry;
 * filtered rows are retained only as compact admission evidence. */
enum class ProductActionRole : std::uint8_t {
    Candidate = 0,
    AutomaticDependency = 1,
    Filtered = 2,
};

inline constexpr std::size_t kProductActionRoleCount = 3;

struct ProductActionAdmissionRecord {
    std::string id;
    PrimitiveTelemetryFamily family = PrimitiveTelemetryFamily::Other;
    ProductActionRole role = ProductActionRole::Filtered;
    std::string reason;
};

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
    /* Cumulative transient Calculator work performed while discovering and
     * validating carrier-local automatic options. These counters are
     * observational: retained sparse-graph row/transition limits are enforced
     * only when rows are appended to SolveTransitionCache. */
    std::uint64_t discovered_states = 0;
    std::uint64_t state_action_rows = 0;
    std::uint64_t transition_entries = 0;
    /* Automatic option synthesis/evaluation is transient admission work. It
     * remains observable here but does not consume the retained parent
     * graph's max_reforge_work allowance. */
    std::uint64_t reforge_active_work = 0;
    std::uint64_t reforge_logical_work_v1 = 0;
    /* Imprint discovery's nominal program budget hides a second exact-work
     * dimension: every program step may fan out over many Calculator states
     * and outcomes. Keep both dimensions and the remaining atomic outcomes()
     * leaf visible so worker-slice claims are measured rather than inferred. */
    std::uint64_t imprint_programs_evaluated = 0;
    std::uint64_t imprint_programs_pruned = 0;
    std::uint64_t imprint_distribution_dominated_programs = 0;
    std::uint64_t imprint_price_pruned_programs = 0;
    std::uint64_t imprint_price_bound_max_program_depth = 0;
    std::uint64_t imprint_max_evaluated_depth = 0;
    std::uint64_t imprint_max_frontier_size = 0;
    std::uint64_t imprint_price_bound_complete_carriers = 0;
    std::uint64_t imprint_action_state_evaluations = 0;
    std::uint64_t imprint_outcomes_merged = 0;
    std::uint64_t imprint_max_atomic_outcomes_ns = 0;
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
    kAutomaticEldritchDominance = 1u << 7,
    kAutomaticMetamodPoolBlock = 1u << 8,
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

/*
 * Exact-refinement observation vocabulary.
 *
 * These are semantic features, not fields copied from AbstractState. A future
 * refinement engine can therefore derive its key from an admitted action's
 * contract without switching on the action id/type. Item features describe
 * scalar item observations. Affix features describe one occupied explicit
 * and are scoped by RefinementAffixSelector below.
 *
 * GoalStatusTierClass is also the terminal/objective observer vocabulary. It
 * deliberately distinguishes goal membership/tier without requiring raw
 * modifier identity. ModifierExclusionSignature is the complete engine group
 * exclusion effect; equal signatures remain mergeable even when modifier ids
 * differ.
 */
enum class RefinementFeature : std::uint8_t {
    Rarity = 0,
    PrefixCount,
    SuffixCount,
    HasCraftedModifier,
    HasFracturedModifier,
    HasVeiledModifier,
    Multimod,
    PrefixLock,
    SuffixLock,
    CannotRollAttack,
    CannotRollCaster,
    Influence,
    SearingExarchTier,
    EaterOfWorldsTier,
    EldritchPresence,
    EldritchDominance,
    Corrupted,
    Mirrored,
    Split,
    Synthesised,
    ModifierSide,
    ModifierExclusionSignature,
    GoalStatusTierClass,
    ModifierCrafted,
    ModifierFractured,
    ModifierVeiled,
    ModifierClassificationTags,
    ModifierRequiredLevel,
    /* Membership vector over AbstractLayout::count_observations. This is the
     * engine-owned observer for compiled mod_count/mod_family_count routing;
     * it is semantic class identity, never a representative modifier id. */
    CountObservationMembership,
    /* Engine semantic role for crafted metamods (multimod, side locks, and
     * cannot-roll blockers). Equal exclusion signatures do not imply equal
     * refill behavior when this role differs. */
    ModifierMetamodRole,
    Count
};

using RefinementFeatureMask = std::uint64_t;

inline constexpr RefinementFeatureMask refinement_feature(
    const RefinementFeature feature) {
    return RefinementFeatureMask{1}
           << static_cast<std::uint8_t>(feature);
}

inline constexpr RefinementFeatureMask kAllRefinementItemFeatures =
    (refinement_feature(RefinementFeature::ModifierSide) - 1);
inline constexpr RefinementFeatureMask kAllRefinementAffixFeatures =
    ((refinement_feature(RefinementFeature::Count) - 1) &
     ~kAllRefinementItemFeatures);
static_assert(
    static_cast<std::uint8_t>(RefinementFeature::Count) <
    std::numeric_limits<RefinementFeatureMask>::digits);

/*
 * Affix selectors are conjunctions of required/forbidden traits. A vector of
 * selectors is a union, which is enough to express fractured-or-locked
 * preservation and Eldritch dominance without baking action names into the
 * refinement algorithm.
 */
enum RefinementAffixTrait : std::uint16_t {
    kRefinementAffixPrefix = 1u << 0,
    kRefinementAffixSuffix = 1u << 1,
    kRefinementAffixCrafted = 1u << 2,
    kRefinementAffixFractured = 1u << 3,
    kRefinementAffixVeiled = 1u << 4,
    kRefinementAffixOnLockedSide = 1u << 5,
    kRefinementAffixOnEldritchDominantSide = 1u << 6,
    kRefinementAffixOnEldritchNonDominantSide = 1u << 7,
};
inline constexpr std::uint16_t kAllRefinementAffixTraits =
    kRefinementAffixPrefix |
    kRefinementAffixSuffix |
    kRefinementAffixCrafted |
    kRefinementAffixFractured |
    kRefinementAffixVeiled |
    kRefinementAffixOnLockedSide |
    kRefinementAffixOnEldritchDominantSide |
    kRefinementAffixOnEldritchNonDominantSide;

enum RefinementItemTrait : std::uint8_t {
    kRefinementItemHasEldritchDominance = 1u << 0,
    /* Scour preserves the locked side only when exactly one side is locked;
     * with neither or both locks it instead preserves fractured affixes. */
    kRefinementItemExactlyOneSideLocked = 1u << 1,
};
inline constexpr std::uint8_t kAllRefinementItemTraits =
    kRefinementItemHasEldritchDominance |
    kRefinementItemExactlyOneSideLocked;

struct RefinementAffixSelector {
    std::uint16_t required_affix_traits = 0;
    std::uint16_t forbidden_affix_traits = 0;
    std::uint8_t required_item_traits = 0;
    std::uint8_t forbidden_item_traits = 0;
    /* Existing-mod classification tags required by this selector. Sorted. */
    std::vector<std::uint32_t> required_tag_ids;

    bool operator==(const RefinementAffixSelector&) const = default;
};

struct RefinementAffixObservation {
    RefinementFeatureMask features = 0;
    RefinementAffixSelector selector;

    bool operator==(const RefinementAffixObservation&) const = default;
};

/*
 * One possible existing-affix flow through an action. `source_selector`
 * identifies carriers that can survive. The target traits are obtained by
 * setting and clearing the declared bits; `preserved_features` names only
 * semantic values that remain equal across that flow.
 *
 * This is deliberately relational rather than an action-name switch. It can
 * express identity-preserving survival, Fracture's unfractured->fractured
 * carrier mutation, and Unveil's identity replacement while retaining
 * side/crafted/fractured carrier properties. When modifier classification is
 * not preserved, downstream tag scopes are widened to the source selector:
 * the replacement kernel, rather than a representative source id, determines
 * target tag membership.
 */
struct RefinementAffixFlow {
    RefinementAffixSelector source_selector;
    std::uint16_t set_affix_traits = 0;
    std::uint16_t cleared_affix_traits = 0;
    RefinementFeatureMask preserved_features = 0;
    bool preserves_modifier_classification = true;

    bool operator==(const RefinementAffixFlow&) const = default;
};

/*
 * A conditionally rewritten scalar may depend on the multiset of surviving
 * carriers rather than only on its old scalar value. Each dependency maps
 * downstream item observations to source-affix observations through the same
 * affix flows (for example Scour rarity -> survivor side/count structure).
 */
struct RefinementItemAffixDependency {
    RefinementFeatureMask item_features = 0;
    RefinementFeatureMask survivor_affix_features = 0;

    bool operator==(const RefinementItemAffixDependency&) const = default;
};

inline bool refinement_selector_matches(
    const RefinementAffixSelector& selector,
    const std::uint16_t affix_traits,
    const std::uint8_t item_traits,
    const std::vector<std::uint32_t>& affix_tag_ids = {}) {
    if ((affix_traits & selector.required_affix_traits) !=
            selector.required_affix_traits ||
        (affix_traits & selector.forbidden_affix_traits) != 0 ||
        (item_traits & selector.required_item_traits) !=
            selector.required_item_traits ||
        (item_traits & selector.forbidden_item_traits) != 0) {
        return false;
    }
    return std::all_of(
        selector.required_tag_ids.begin(),
        selector.required_tag_ids.end(),
        [&](const std::uint32_t tag) {
            return std::binary_search(
                affix_tag_ids.begin(), affix_tag_ids.end(), tag);
        });
}

inline bool refinement_selectors_match(
    const std::vector<RefinementAffixSelector>& selectors,
    const std::uint16_t affix_traits,
    const std::uint8_t item_traits,
    const std::vector<std::uint32_t>& affix_tag_ids = {}) {
    return std::any_of(
        selectors.begin(), selectors.end(),
        [&](const RefinementAffixSelector& selector) {
            return refinement_selector_matches(
                selector, affix_traits, item_traits, affix_tag_ids);
        });
}

inline constexpr std::uint32_t kActionRefinementContractVersion = 2;

/*
 * A primitive may expose a sampled observation before the strategy chooses
 * its concrete operation parameter.  The legacy implementation names the
 * current modifier-offer sidecars after Unveil, but this contract is the
 * semantic authority: consumers must not infer observed choice from an
 * ActionType.  Future actions that expose the same modifier-offer vocabulary
 * opt in here and automatically reuse the solve/compile/evaluation path.
 */
enum class RefinementOutcomeObservation : std::uint8_t {
    None = 0,
    ModifierOffer = 1,
};

struct ActionRefinementContract {
    /* Zero means admission has not supplied a complete semantic contract. */
    std::uint32_t schema_version = 0;
    RefinementFeatureMask observed_item_features = 0;
    /* Source scalar values that can respectively survive or be rewritten.
     * Conditional/stochastic transforms may set a bit in both masks. */
    RefinementFeatureMask preserved_item_features = 0;
    RefinementFeatureMask destroyed_item_features = 0;
    /* Existing-mod tags read by the action, independent of selector tags.
     * Sorted. Action parameters that only choose a new roll pool do not
     * belong here. */
    std::vector<std::uint32_t> observed_modifier_tag_ids;
    /* Exact-affix observations. Separate scoped terms prevent, for example,
     * a destructive reforge from retaining exclusion effects of affixes it
     * first destroys. */
    std::vector<RefinementAffixObservation> affix_observations;
    std::vector<RefinementItemAffixDependency>
        item_affix_dependencies;
    /* Feature-specific survivor relations used by the shared backward
     * refinement engine. Compatibility selectors below remain the compact
     * may-survive/may-destroy summary consumed by older diagnostics. */
    std::vector<RefinementAffixFlow> affix_flows;
    /* Exact affixes whose identity may survive the action. */
    std::vector<RefinementAffixSelector> preserved_affixes;
    /* Exact affixes whose identity may be destroyed by the action. */
    std::vector<RefinementAffixSelector> destroyed_affixes;
    /* Restart is the one admitted operation that replaces the complete item
     * rather than transforming its current explicit state. */
    bool resets_to_fresh_item = false;
    RefinementOutcomeObservation outcome_observation =
        RefinementOutcomeObservation::None;

    bool complete() const {
        return schema_version == kActionRefinementContractVersion;
    }
};

inline bool refinement_contract_observes_modifier_offer(
    const ActionRefinementContract& contract) {
    return contract.outcome_observation ==
           RefinementOutcomeObservation::ModifierOffer;
}

inline bool refinement_contract_observes_item(
    const ActionRefinementContract& contract,
    const RefinementFeature feature) {
    return (contract.observed_item_features &
            refinement_feature(feature)) != 0;
}

inline bool refinement_contract_observes_affix(
    const ActionRefinementContract& contract,
    const RefinementFeature feature,
    const std::uint16_t affix_traits,
    const std::uint8_t item_traits,
    const std::vector<std::uint32_t>& affix_tag_ids = {}) {
    const RefinementFeatureMask bit = refinement_feature(feature);
    return std::any_of(
        contract.affix_observations.begin(),
        contract.affix_observations.end(),
        [&](const RefinementAffixObservation& observation) {
            return (observation.features & bit) != 0 &&
                   refinement_selector_matches(
                       observation.selector, affix_traits, item_traits,
                       affix_tag_ids);
        });
}

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
    ActionRefinementContract refinement;
    ActionPreservationMetadata preservation;
    /* Reserved for future two-item techniques (imprint/recombinator inside
     * the item-level solver). Nothing planned sets it; see plan. */
    bool uses_companion_state = false;
    /* Engine-owned product role. Dependencies enter abstraction and exact
     * evaluation only through a materialized automatic option and never
     * enter the independently selectable candidate list. */
    ProductActionRole product_role = ProductActionRole::Candidate;
    std::string product_admission_reason = "candidate_unfiltered";
};

inline bool action_observes_modifier_offer(
    const ActionDescriptor& action) {
    return refinement_contract_observes_modifier_offer(
        action.refinement);
}

struct ActionRegistry {
    std::vector<ActionDescriptor> actions;
    std::unordered_map<std::string, std::uint32_t> index_by_id;
    std::uint32_t goal_relevant_actions_pruned = 0;
    std::uint32_t fossil_loadouts_possible = 0;
    std::uint32_t fossil_loadouts_generated = 0;
    std::uint32_t fossil_loadouts_deferred = 0;
    bool fossil_generation_lazy = false;
    bool fossil_generation_goal_relevant = false;
    bool product_goal_filtering = false;
    std::array<std::uint32_t, kProductActionRoleCount>
        product_role_counts{};
    std::array<
        std::array<std::uint32_t, kPrimitiveTelemetryFamilyCount>,
        kProductActionRoleCount>
        product_role_family_counts{};
    std::map<std::string, std::uint32_t> product_reason_counts;
    std::vector<ProductActionAdmissionRecord> product_filtered_actions;
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
    std::uint32_t required_satisfied_slots = 0;
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
    /*
     * Numeric action references are context-local execution handles. Their
     * adjacent ids are the context-independent semantic authority used when
     * an operator is compared with or imported into another CalcContext.
     * Every populated numeric reference must have a populated id companion,
     * and vice versa.
     */
    std::uint32_t primitive_action = kNoId;
    std::string primitive_action_id;
    std::vector<std::uint32_t> primitive_program;
    std::vector<std::string> primitive_program_action_ids;
    std::int8_t intended_side = -1;
    std::vector<std::uint32_t> exit_goal_slots;
    std::uint32_t exit_min_satisfied = 0;
    std::uint32_t carrier_goal_slot = kNoId;
    std::uint32_t conditional_action = kNoId; /* Fracture after preparation */
    std::string conditional_action_id;
    std::uint32_t bestiary_create_action = kNoId;
    std::string bestiary_create_action_id;
    std::uint32_t bestiary_restore_action = kNoId;
    std::string bestiary_restore_action_id;
    AutomaticCandidateKind automatic_kind = AutomaticCandidateKind::None;
    std::uint32_t relevant_goal_mask = 0;
    std::uint32_t setup_action = kNoId;
    std::string setup_action_id;
    std::uint32_t followup_action = kNoId;
    std::string followup_action_id;
    std::uint32_t cleanup_action = kNoId;
    std::string cleanup_action_id;
    std::uint32_t constructive_finish_action = kNoId;
    std::string constructive_finish_action_id;
    /* Sorted dependency quantities. S7.3 fixed programs execute each entry
     * exactly once; S7.4 kernels return the exact state-dependent expected
     * quantities used for pricing conditional and observed paths. */
    std::vector<std::pair<std::string, double>> resource_quantities;
};

/*
 * Collision-free serialization and equality for operator behavior/pricing.
 * Presentation fields and context-local numeric handles are deliberately
 * excluded; every action dependency is represented by its canonical id.
 */
std::vector<std::uint64_t> planner_operator_semantic_key(
    const PlannerOperator& planner);
bool planner_operator_structurally_equal(
    const PlannerOperator& left,
    const PlannerOperator& right);

struct PlannerOperatorRuntimeStep {
    std::uint32_t action = kNoId;
    ActionRefinementContract refinement;
};

/*
 * Runtime semantic authority for one planner operator. `ordered_program`
 * retains the longest canonical execution order; `execution_paths` names the
 * possible prefixes/conditional paths used for composable backward
 * observation/preimage analysis. Dependencies are the same ordinary registry
 * actions sorted and unique in the current CalcContext.
 *
 * `compatibility_refinement` is only a conservative flat union for direct
 * product-parent compatibility triggering. It is not a sequential
 * preservation/flow proof: callers proving downstream equivalence must
 * reverse-compose `execution_paths` instead. Proof-only synthesis metadata
 * (notably constructive_finish_action) is intentionally excluded throughout.
 */
struct PlannerOperatorRuntimeSemantics {
    std::vector<PlannerOperatorRuntimeStep> ordered_program;
    /*
     * Every engine-authorized runtime path through the operator. Fixed
     * programs may exit after a prefix; an optional conditional action may
     * run alone or after preparation. Backward observation authority merges
     * the preimages of these paths rather than pretending every transition
     * executed the longest program.
     */
    std::vector<std::vector<PlannerOperatorRuntimeStep>> execution_paths;
    std::vector<std::uint32_t> action_dependencies;
    ActionRefinementContract compatibility_refinement;
};

PlannerOperatorRuntimeSemantics planner_operator_runtime_semantics(
    const PlannerOperator& planner,
    const ActionRegistry& registry);

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

/* Complete exclusion effect of one exact modifier. This is shared authority
 * for junk and occupied-goal refinement: pool-add actions observe both. */
std::vector<std::uint64_t> modifier_exclusion_effect_signature(
    const SessionImpl& session,
    std::uint32_t mod_id);

/* Engine-owned Veiled-template identity used by strict carrier partitioning
 * and refinement feature extraction. */
bool modifier_is_veiled_template(
    const SessionImpl& session,
    std::uint32_t mod_id);

/* Derive one descriptor's semantic contract. Registry admission calls this
 * for every action, then validates the result before exposing the registry. */
ActionRefinementContract derive_action_refinement_contract(
    const SessionImpl& session,
    const ActionDescriptor& action);

/* Stable semantic signature used to intern/compare contracts. It excludes
 * action id and prices: equal action-state observers intentionally share it. */
std::vector<std::uint64_t> action_refinement_contract_signature(
    const ActionRefinementContract& contract);

/* Throws std::logic_error when an action reaches registry admission without a
 * complete/canonical semantic contract. */
void validate_action_refinement_contract(const ActionDescriptor& action);

/* The single admission path for authored/internal action contracts. It
 * canonicalizes equivalent selector/flow spellings, synthesizes compatibility
 * identity flows where appropriate, then performs the complete structural
 * proof. Callers must use this before exposing a descriptor to CalcContext or
 * planner runtime semantics. */
void canonicalize_and_validate_action_refinement_contract(
    ActionDescriptor& action);
void canonicalize_and_validate_action_refinement_contract(
    const SessionImpl& session,
    ActionDescriptor& action);

/* Resolve the goal's explicitly selected fixed options. The returned vector
 * begins with one primitive wrapper per registry action at the same index. */
std::vector<PlannerOperator> build_planner_operators(
    const SessionImpl& session,
    const GoalSpec& goal,
    const ActionRegistry& registry,
    const std::vector<std::uint32_t>& admitted_primitives);

// --- goal resolution and abstract layout --------------------------------------

enum class GoalSlotStatus : std::uint8_t {
    Absent = 0,
    PresentBelowTier = 1,
    Satisfied = 2
};

/*
 * One occupied-goal equivalence class in an exact-evaluation layout.
 * Modifier identity is intentionally absent: members remain merged when
 * every admitted observer sees the same side, tier status, restricted tag
 * signature, Veiled-template role, selector-conditioned required level and
 * exclusion effect, and compiled count-observation membership.
 */
struct GoalMemberClass {
    GoalSlotStatus status = GoalSlotStatus::Absent;
    std::int8_t gen_type = 0; /* 0 prefix, 1 suffix */
    std::vector<std::uint64_t> exclusion_effect_mask;
    std::vector<std::uint64_t> count_observation_bits;
    std::vector<std::uint64_t> member_mask;
    std::uint32_t member_count = 0;
};

struct ResolvedGoalSlot {
    GoalSlot spec;
    std::vector<std::uint64_t> member_mask;     /* any-tier group/family mods */
    std::vector<std::uint64_t> satisfying_mask; /* members at/above min_tier */
    /* Exclusivity groups covering the satisfying mods; a non-member explicit
     * occupying any of them blocks this slot. Sorted. */
    std::vector<std::uint32_t> blocking_group_ids;
    /* Exact layouts only. Token n addresses member_classes[n - 1]; token zero
     * means absent (or that the coarse layout deliberately omits identity).
     * Both vectors remain empty in product/coarse layouts. */
    std::vector<GoalMemberClass> member_classes;
    std::vector<std::uint32_t> member_class_token_by_mod;
};

/*
 * One junk equivalence class. Two non-goal mods share a class iff every
 * candidate action treats them identically: same generation side, same
 * restricted tag signature (classification tags intersected with the
 * candidate actions' discriminating/observed tags), blocking the same goal
 * slots, and (when relevant) the same Veiled-template role. Exact layouts
 * additionally apply selector-conditioned required-level and exclusion
 * observations plus compiled count-observation membership.
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
    const std::vector<CountObservation>& count_observations = {},
    const std::vector<std::uint64_t>&
        required_reachable_mod_mask = {},
    bool distinguish_modifier_identity = false,
    const AbstractLayout* refinement_parent_layout = nullptr);

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
    /* Exact occupied-goal observation class. Zero in coarse layouts and for
     * absent slots; otherwise token n addresses
     * AbstractLayout::slots[i].member_classes[n - 1]. */
    std::array<std::uint32_t, kMaxGoalSlots> goal_member_class_tokens{};
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


} // namespace solver
} // namespace poecraft
