#ifndef POECRAFT_SRC_SOLVER_INTERNAL_HPP
#define POECRAFT_SRC_SOLVER_INTERNAL_HPP

#include <array>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "engine_internal.hpp"

/*
 * Solver phase S1 (docs/crafting-solver-plan.md): action registry schema,
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

struct GoalSpec {
    std::vector<GoalSlot> slots; /* 1..kMaxGoalSlots */
    std::uint8_t rarity = PC_RARITY_RARE; /* required finished rarity */
    /* Minimum slots that must be satisfied together. Zero is the internal
     * default and means every slot, preserving callers that construct a
     * GoalSpec directly instead of going through the JSON parser. */
    std::uint32_t min_satisfied_slots = 0;

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
    /* Reserved for future two-item techniques (imprint/recombinator inside
     * the item-level solver). Nothing planned sets it; see plan. */
    bool uses_companion_state = false;
};

struct ActionRegistry {
    std::vector<ActionDescriptor> actions;
    std::unordered_map<std::string, std::uint32_t> index_by_id;
};

/*
 * Enumerate every plannable action instance for this session: base currency
 * operations, session-available essences, single fossils, bench crafts and
 * metamods, veiled/unveil, Harvest per-tag actions, eldritch actions,
 * influenced exalts, fracture, and the synthetic restart action. Mechanics
 * the engine build does not support for this session are simply absent.
 */
ActionRegistry build_action_registry(const SessionImpl& session);

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
 * candidate actions' discriminating tags), and blocking the same goal slots.
 */
struct JunkClass {
    std::int8_t gen_type = 0;           /* 0 prefix, 1 suffix */
    std::uint64_t tag_bits = 0;         /* over discriminating_tag_ids order */
    std::uint32_t goal_block_mask = 0;  /* bit per goal slot index */
    std::vector<std::uint64_t> member_mask;
    std::uint32_t member_count = 0;
};

struct AbstractLayout {
    std::vector<ResolvedGoalSlot> slots;
    /* Union of the candidate actions' discriminating tags, sorted. Bit i of
     * JunkClass::tag_bits corresponds to discriminating_tag_ids[i]. */
    std::vector<std::uint32_t> discriminating_tag_ids;
    /* Deterministic order: sorted by (gen_type, tag_bits, goal_block_mask). */
    std::vector<JunkClass> junk_classes;
    /* session mod id -> junk class index, or kNoId for goal members and mods
     * no candidate action can place in an explicit slot. */
    std::vector<std::uint32_t> junk_class_by_mod;
};

/*
 * Derive the abstract state layout for (goal, candidate action subset).
 * action_indices index into registry.actions; an empty vector means the full
 * registry. Throws std::runtime_error on an invalid goal (no slots, too many
 * slots, unknown group/family, overlapping slot member masks) or when the
 * discriminating tag union exceeds kMaxDiscriminatingTags.
 */
AbstractLayout build_abstract_layout(
    const SessionImpl& session,
    const GoalSpec& goal,
    const ActionRegistry& registry,
    const std::vector<std::uint32_t>& action_indices);

// --- abstract state -----------------------------------------------------------

struct AbstractState {
    std::array<std::uint8_t, kMaxGoalSlots> slot_status{}; /* GoalSlotStatus */
    std::uint32_t blocked_mask = 0; /* bit per goal slot: a non-member
                                       explicit occupies a blocking group */
    std::uint8_t prefix_count = 0;
    std::uint8_t suffix_count = 0;
    std::uint8_t rarity = PC_RARITY_NORMAL;
    std::uint8_t influence_bits = 0;
    std::uint32_t flags = 0; /* AbstractFlag bits */
    std::vector<std::uint8_t> junk_counts; /* parallel to layout.junk_classes */

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

// --- calculation engine (S2): exact transition provider ------------------------

struct OutcomeEntry {
    std::uint32_t state = kNoId; /* interned successor state id */
    double probability = 0.0;
};

struct OutcomeDistribution {
    /* True when an exact evaluator exists for this action. Reforge and
     * bespoke enumerators land in S3; until then those actions report
     * supported = false and the caller falls back or skips. */
    bool supported = false;
    std::vector<OutcomeEntry> entries; /* sorted by state id, sums to 1 */
    std::array<double, kMaxGoalSlots> slot_satisfied_probability{};
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
        const std::vector<std::uint32_t>& action_indices = {});

    const SessionImpl& session() const { return *session_; }
    const AbstractLayout& layout() const { return layout_; }
    const ActionRegistry& registry() const { return registry_; }
    const GoalSpec& goal() const { return goal_; }
    /* The candidate action subset the layout was derived for (resolved:
     * never empty; defaults to every registry action). */
    const std::vector<std::uint32_t>& candidates() const {
        return candidates_;
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
    bool materialize(std::uint32_t state_id, pc_item_state& out_item);

    /*
     * Exact successor distribution, cache-first. The result stays valid
     * until the CalcContext is destroyed. Distributions are
     * price-independent by construction; costs live on the descriptor.
     */
    const OutcomeDistribution& outcomes(
        std::uint32_t state_id,
        std::uint32_t action_index);

  private:
    std::shared_ptr<const SessionImpl> session_;
    GoalSpec goal_;
    AbstractLayout layout_;
    ActionRegistry registry_;
    std::vector<std::uint32_t> candidates_;
    ActionContextImpl context_;
    std::vector<AbstractState> states_;
    std::unordered_map<std::size_t, std::vector<std::uint32_t>>
        state_ids_by_hash_;
    std::unordered_map<std::uint64_t, OutcomeDistribution> distribution_cache_;
    /* Reforges depend only on the preserved base (fractured/locked slots,
     * rarity, item-wide flags), so states sharing one base share one roll
     * DP. Key: (action index, base signature hash). */
    std::map<std::pair<std::uint32_t, std::uint64_t>, OutcomeDistribution>
        reforge_cache_;

    OutcomeDistribution evaluate(
        std::uint32_t state_id,
        std::uint32_t action_index);
    OutcomeDistribution evaluate_reforge(
        std::uint32_t state_id,
        std::uint32_t action_index);
    bool evaluate_pool_add(
        const pc_item_state& item,
        const PoolBuildRequest& base_request,
        std::map<std::uint32_t, double>& accumulated);
};

// --- DP solver core (S4) --------------------------------------------------------

struct SolveOptions {
    double epsilon = 1e-9;          /* max Bellman residual, cost units */
    std::uint32_t max_states = 100000;
    std::uint32_t max_sweeps = 100000;
};

struct SolveDiagnostics {
    /* Actions the solve planned without, and why. */
    std::vector<std::string> skipped_missing_price;
    std::vector<std::string> skipped_unsupported;
    std::uint32_t expanded_states = 0;
    std::uint32_t sweeps = 0;
    double residual = 0.0;
    bool state_cap_hit = false;
};

/*
 * Value table and policy over the reachable abstract state set. Vectors are
 * indexed by CalcContext state id; states never expanded (past the cap)
 * keep an infinite value and no policy action.
 */
struct SolveResult {
    bool converged = false;
    std::uint32_t start_state = kNoId;
    std::vector<double> values;
    std::vector<std::uint32_t> policy; /* registry action index or kNoId */
    std::vector<std::uint8_t> expanded;
    std::vector<std::uint8_t> goal_states;
    std::vector<std::uint8_t> policy_reachable;
    SolveDiagnostics diagnostics;
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
 * ML corpus line format (docs/crafting-solver-plan.md, ML Data Logging):
 * one JSON object per state with features, value, policy action, and
 * policy reachability. Durable formatting/versioning happens at the
 * tooling layer; this provides the canonical per-solve records.
 */
std::string serialize_solve_log(
    const CalcContext& calc,
    const SolveResult& result);

// --- policy -> strategy graph compiler (S5) --------------------------------------

/*
 * Compile a solved policy into ordinary strategy JSON (the same format the
 * editor and simulator consume): a master router whose prioritized edges
 * test policy-reachable state membership with existing condition types,
 * one operation node per state annotated with its expected remaining cost,
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
    const CalcContext& calc,
    const SolveResult& result,
    const std::string& name);

} // namespace solver
} // namespace poecraft

#endif
