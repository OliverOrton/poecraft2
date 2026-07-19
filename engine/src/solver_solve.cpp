#include "solver_internal.hpp"

#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <deque>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>
#include <string_view>
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

namespace {

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
    std::uint64_t variant_offset = 0;
    std::uint32_t variant_count = 0;
    std::uint64_t transition_offset = 0;
    std::uint32_t transition_count = 0;
    double self_probability = 0.0;
    std::uint64_t choice_offset = 0;
    std::uint32_t choice_count = 0;
    CarrierEffectSummary preservation_effect;
};

struct StateRowSpan {
    std::uint64_t offset = 0;
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
    bool entry_relative_self = false;
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

constexpr std::uint32_t kProtectionFlags =
    kFlagMultimod | kFlagNoAttack | kFlagNoCaster |
    kFlagPrefixesLocked | kFlagSuffixesLocked;

AutomaticTelemetryKind automatic_telemetry_kind(
    const PlannerOperator& planner) {
    if (planner.kind == PlannerOperatorKind::FixedOption) {
        switch (planner.option_kind) {
        case FixedOptionKind::ImprintRetry:
            return AutomaticTelemetryKind::Imprint;
        case FixedOptionKind::Renewal:
            return AutomaticTelemetryKind::Renewal;
        case FixedOptionKind::ProtectedSide:
        case FixedOptionKind::ProtectedRepeat:
            return AutomaticTelemetryKind::ProtectedSide;
        case FixedOptionKind::TemporaryBenchRepeat:
            return AutomaticTelemetryKind::TemporaryBench;
        case FixedOptionKind::FracturePrepare:
            return AutomaticTelemetryKind::FracturePrepare;
        case FixedOptionKind::MultimodFinish:
            return AutomaticTelemetryKind::MultimodFinish;
        default:
            break;
        }
    }
    switch (planner.automatic_kind) {
    case AutomaticCandidateKind::PermanentBench:
        return AutomaticTelemetryKind::PermanentBench;
    case AutomaticCandidateKind::Fracture:
        return AutomaticTelemetryKind::PrimitiveFracture;
    case AutomaticCandidateKind::MultimodFinish:
        return AutomaticTelemetryKind::MultimodFinish;
    case AutomaticCandidateKind::Imprint:
        return AutomaticTelemetryKind::Imprint;
    case AutomaticCandidateKind::TemporaryBenchBlocker:
        return AutomaticTelemetryKind::TemporaryBench;
    case AutomaticCandidateKind::ProtectedMetamod:
        return AutomaticTelemetryKind::ProtectedSide;
    case AutomaticCandidateKind::None:
        break;
    }
    return AutomaticTelemetryKind::None;
}

const char* automatic_telemetry_kind_name(
    const AutomaticTelemetryKind kind) {
    switch (kind) {
    case AutomaticTelemetryKind::Imprint: return "imprint";
    case AutomaticTelemetryKind::Renewal: return "renewal";
    case AutomaticTelemetryKind::ProtectedSide: return "protected_side";
    case AutomaticTelemetryKind::TemporaryBench: return "temporary_bench";
    case AutomaticTelemetryKind::FracturePrepare:
        return "fracture_prepare";
    case AutomaticTelemetryKind::PermanentBench: return "permanent_bench";
    case AutomaticTelemetryKind::MultimodFinish: return "multimod_finish";
    case AutomaticTelemetryKind::PrimitiveFracture:
        return "primitive_fracture";
    case AutomaticTelemetryKind::Count:
    case AutomaticTelemetryKind::None:
        return "none";
    }
    return "none";
}

const char* primitive_telemetry_family_name(
    const PrimitiveTelemetryFamily family) {
    switch (family) {
    case PrimitiveTelemetryFamily::Currency: return "currency";
    case PrimitiveTelemetryFamily::Essence: return "essence";
    case PrimitiveTelemetryFamily::Fossil: return "fossil";
    case PrimitiveTelemetryFamily::Harvest: return "harvest";
    case PrimitiveTelemetryFamily::Bench: return "bench";
    case PrimitiveTelemetryFamily::Bestiary: return "bestiary";
    case PrimitiveTelemetryFamily::Fracture: return "fracture";
    case PrimitiveTelemetryFamily::Other: return "other";
    case PrimitiveTelemetryFamily::Count: return "none";
    }
    return "none";
}

std::uint32_t compact_count_total(const CompactCountVector& counts) {
    std::uint32_t total = 0;
    for (const std::uint8_t count : counts) total += count;
    return total;
}

CarrierFacts carrier_facts(const AbstractState& state) {
    CarrierFacts facts;
    for (std::uint32_t slot = 0; slot < kMaxGoalSlots; ++slot) {
        if (state.slot_status[slot] !=
            static_cast<std::uint8_t>(GoalSlotStatus::Absent)) {
            facts.goal_family_mask |= 1u << slot;
        }
        if (state.slot_status[slot] ==
            static_cast<std::uint8_t>(GoalSlotStatus::Satisfied)) {
            facts.satisfied_goal_mask |= 1u << slot;
        }
    }
    facts.blocked_mask = state.blocked_mask;
    facts.crafted_goal_mask = state.crafted_goal_mask;
    facts.fractured_goal_mask = state.fractured_goal_mask;
    facts.active_protection = state.flags & kProtectionFlags;
    facts.junk_count = compact_count_total(state.junk_counts);
    facts.crafted_junk_count =
        compact_count_total(state.crafted_junk_counts);
    facts.fractured_junk_count =
        compact_count_total(state.fractured_junk_counts);
    facts.prefix_count = state.prefix_count;
    facts.suffix_count = state.suffix_count;
    facts.rarity = state.rarity;
    facts.state_hash = abstract_state_hash(state);
    return facts;
}

void classify_slot_mask(
    const std::uint32_t source,
    const std::uint32_t all_successors,
    const std::uint32_t any_successor,
    std::uint32_t& preserved,
    std::uint32_t& destroyed,
    std::uint32_t& created,
    std::uint32_t& unreachable) {
    preserved = source & all_successors;
    destroyed = source & ~all_successors;
    created = ~source & any_successor;
    unreachable = source & ~any_successor;
}

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
    std::vector<std::uint32_t> successor_ids) {
    std::sort(successor_ids.begin(), successor_ids.end());
    successor_ids.erase(
        std::unique(successor_ids.begin(), successor_ids.end()),
        successor_ids.end());
    if (successor_ids.empty()) {
        throw std::logic_error("carrier successor envelope is empty");
    }
    CarrierSuccessorEnvelope envelope;
    bool first = true;
    for (const std::uint32_t successor_id : successor_ids) {
        const AbstractState& successor = calc.state(successor_id);
        const CarrierFacts facts = carrier_facts(successor);
        envelope.all_goal &= facts.goal_family_mask;
        envelope.any_goal |= facts.goal_family_mask;
        envelope.all_satisfied &= facts.satisfied_goal_mask;
        envelope.any_satisfied |= facts.satisfied_goal_mask;
        envelope.all_fractured &= facts.fractured_goal_mask;
        envelope.any_fractured |= facts.fractured_goal_mask;
        envelope.all_crafted &= facts.crafted_goal_mask;
        envelope.any_crafted |= facts.crafted_goal_mask;
        envelope.all_protection &= facts.active_protection;
        envelope.any_protection |= facts.active_protection;
        envelope.all_blocked &= facts.blocked_mask;
        envelope.any_blocked |= facts.blocked_mask;
        envelope.min_junk_count =
            std::min(envelope.min_junk_count, facts.junk_count);
        envelope.max_junk_count =
            std::max(envelope.max_junk_count, facts.junk_count);
        envelope.min_crafted_junk_count = std::min(
            envelope.min_crafted_junk_count, facts.crafted_junk_count);
        envelope.max_crafted_junk_count = std::max(
            envelope.max_crafted_junk_count, facts.crafted_junk_count);
        envelope.min_fractured_junk_count = std::min(
            envelope.min_fractured_junk_count, facts.fractured_junk_count);
        envelope.max_fractured_junk_count = std::max(
            envelope.max_fractured_junk_count, facts.fractured_junk_count);
        envelope.min_prefix_count =
            std::min(envelope.min_prefix_count, facts.prefix_count);
        envelope.max_prefix_count =
            std::max(envelope.max_prefix_count, facts.prefix_count);
        envelope.min_suffix_count =
            std::min(envelope.min_suffix_count, facts.suffix_count);
        envelope.max_suffix_count =
            std::max(envelope.max_suffix_count, facts.suffix_count);
        if (first) {
            envelope.junk_counts = successor.junk_counts;
            envelope.crafted_junk_counts = successor.crafted_junk_counts;
            envelope.fractured_junk_counts = successor.fractured_junk_counts;
            first = false;
        } else {
            envelope.junk_counts_uniform &=
                envelope.junk_counts == successor.junk_counts;
            envelope.crafted_junk_counts_uniform &=
                envelope.crafted_junk_counts == successor.crafted_junk_counts;
            envelope.fractured_junk_counts_uniform &=
                envelope.fractured_junk_counts ==
                successor.fractured_junk_counts;
        }
    }
    return envelope;
}

CarrierEffectSummary carrier_effect(
    const CalcContext& calc,
    const std::uint32_t source_state,
    const CarrierSuccessorEnvelope& envelope) {
    CarrierEffectSummary effect;
    const AbstractState& source = calc.state(source_state);
    const CarrierFacts source_facts = carrier_facts(source);
    const bool all_junk_exact = envelope.junk_counts_uniform &&
                                envelope.junk_counts == source.junk_counts;
    const bool any_junk_decrease =
        envelope.min_junk_count < source_facts.junk_count;
    const bool any_junk_increase =
        envelope.max_junk_count > source_facts.junk_count;
    const bool all_junk_zero = envelope.max_junk_count == 0;
    const bool all_blockers_exact =
        envelope.all_blocked == source_facts.blocked_mask &&
        envelope.any_blocked == source_facts.blocked_mask;
    const bool any_blocker_lost =
        (source_facts.blocked_mask & ~envelope.all_blocked) != 0;
    const bool any_blocker_created =
        (~source_facts.blocked_mask & envelope.any_blocked) != 0;
    const bool all_blockers_zero = envelope.any_blocked == 0;
    const bool all_crafted_junk_exact =
        envelope.crafted_junk_counts_uniform &&
        envelope.crafted_junk_counts == source.crafted_junk_counts;
    const bool any_crafted_junk_lost =
        envelope.min_crafted_junk_count < source_facts.crafted_junk_count;
    const bool any_crafted_junk_created =
        envelope.max_crafted_junk_count > source_facts.crafted_junk_count;
    const bool all_crafted_junk_zero =
        envelope.max_crafted_junk_count == 0;
    const bool all_fractured_junk_exact =
        envelope.fractured_junk_counts_uniform &&
        envelope.fractured_junk_counts == source.fractured_junk_counts;
    const bool any_fractured_junk_lost =
        envelope.min_fractured_junk_count < source_facts.fractured_junk_count;
    const bool any_fractured_junk_created =
        envelope.max_fractured_junk_count > source_facts.fractured_junk_count;
    const bool all_fractured_junk_zero =
        envelope.max_fractured_junk_count == 0;
    effect.min_prefix_count = envelope.min_prefix_count;
    effect.max_prefix_count = envelope.max_prefix_count;
    effect.min_suffix_count = envelope.min_suffix_count;
    effect.max_suffix_count = envelope.max_suffix_count;

    classify_slot_mask(
        source_facts.goal_family_mask, envelope.all_goal, envelope.any_goal,
        effect.preserved_goal_family_mask,
        effect.destroyed_goal_family_mask,
        effect.created_goal_family_mask,
        effect.unreachable_goal_family_mask);
    classify_slot_mask(
        source_facts.satisfied_goal_mask, envelope.all_satisfied,
        envelope.any_satisfied,
        effect.preserved_satisfied_goal_mask,
        effect.destroyed_satisfied_goal_mask,
        effect.created_satisfied_goal_mask,
        effect.unreachable_satisfied_goal_mask);
    std::uint32_t unused_created = 0;
    std::uint32_t unused_unreachable = 0;
    classify_slot_mask(
        source_facts.fractured_goal_mask, envelope.all_fractured,
        envelope.any_fractured,
        effect.preserved_fractured_goal_mask,
        effect.destroyed_fractured_goal_mask,
        unused_created, unused_unreachable);
    classify_slot_mask(
        source_facts.crafted_goal_mask, envelope.all_crafted,
        envelope.any_crafted,
        effect.preserved_crafted_goal_mask,
        effect.destroyed_crafted_goal_mask,
        unused_created, unused_unreachable);
    classify_slot_mask(
        source_facts.active_protection, envelope.all_protection,
        envelope.any_protection,
        effect.preserved_protection, effect.destroyed_protection,
        unused_created, unused_unreachable);

    const auto set_mask_effect = [&](const std::uint32_t bit,
                                     const std::uint32_t preserved,
                                     const std::uint32_t destroyed,
                                     const std::uint32_t created,
                                     const std::uint32_t unreachable) {
        if (preserved != 0) effect.preserved_properties |= bit;
        if (destroyed != 0) effect.destroyed_properties |= bit;
        if (created != 0) effect.created_properties |= bit;
        if (unreachable != 0) effect.unreachable_properties |= bit;
    };
    set_mask_effect(
        kCarrierGoalFamilies, effect.preserved_goal_family_mask,
        effect.destroyed_goal_family_mask,
        effect.created_goal_family_mask,
        effect.unreachable_goal_family_mask);
    set_mask_effect(
        kCarrierSatisfiedGoalSubset,
        effect.preserved_satisfied_goal_mask,
        effect.destroyed_satisfied_goal_mask,
        effect.created_satisfied_goal_mask,
        effect.unreachable_satisfied_goal_mask);
    if (all_junk_exact && source_facts.junk_count != 0 ||
        all_blockers_exact && source_facts.blocked_mask != 0) {
        effect.preserved_properties |= kCarrierJunkBlockers;
    }
    if (any_junk_decrease || any_blocker_lost) {
        effect.destroyed_properties |= kCarrierJunkBlockers;
    }
    if (any_junk_increase || any_blocker_created) {
        effect.created_properties |= kCarrierJunkBlockers;
    }
    if ((source_facts.junk_count != 0 && all_junk_zero) ||
        (source_facts.blocked_mask != 0 && all_blockers_zero)) {
        effect.unreachable_properties |= kCarrierJunkBlockers;
    }
    if ((effect.preserved_crafted_goal_mask != 0) ||
        (all_crafted_junk_exact && source_facts.crafted_junk_count != 0)) {
        effect.preserved_properties |= kCarrierCraftedState;
    }
    if (effect.destroyed_crafted_goal_mask != 0 || any_crafted_junk_lost) {
        effect.destroyed_properties |= kCarrierCraftedState;
    }
    if (any_crafted_junk_created ||
        (envelope.any_crafted & ~source_facts.crafted_goal_mask) != 0) {
        effect.created_properties |= kCarrierCraftedState;
    }
    if ((source_facts.crafted_goal_mask != 0 &&
         envelope.any_crafted == 0) ||
        (source_facts.crafted_junk_count != 0 && all_crafted_junk_zero)) {
        effect.unreachable_properties |= kCarrierCraftedState;
    }
    if ((effect.preserved_fractured_goal_mask != 0) ||
        (all_fractured_junk_exact && source_facts.fractured_junk_count != 0)) {
        effect.preserved_properties |= kCarrierFracturedState;
    }
    if (effect.destroyed_fractured_goal_mask != 0 || any_fractured_junk_lost) {
        effect.destroyed_properties |= kCarrierFracturedState;
    }
    if (any_fractured_junk_created ||
        (envelope.any_fractured & ~source_facts.fractured_goal_mask) != 0) {
        effect.created_properties |= kCarrierFracturedState;
    }
    if ((source_facts.fractured_goal_mask != 0 &&
         envelope.any_fractured == 0) ||
        (source_facts.fractured_junk_count != 0 && all_fractured_junk_zero)) {
        effect.unreachable_properties |= kCarrierFracturedState;
    }
    const auto side_effect = [&](const std::uint32_t bit,
                                 const std::uint8_t source_count,
                                 const std::uint8_t min_count,
                                 const std::uint8_t max_count) {
        if (source_count != 0 && min_count >= source_count) {
            effect.preserved_properties |= bit;
        }
        if (min_count < source_count) effect.destroyed_properties |= bit;
        if (max_count > source_count) effect.created_properties |= bit;
        if (source_count != 0 && max_count == 0) {
            effect.unreachable_properties |= bit;
        }
    };
    side_effect(
        kCarrierPrefixSide, source_facts.prefix_count,
        effect.min_prefix_count, effect.max_prefix_count);
    side_effect(
        kCarrierSuffixSide, source_facts.suffix_count,
        effect.min_suffix_count, effect.max_suffix_count);
    set_mask_effect(
        kCarrierActiveProtection, effect.preserved_protection,
        effect.destroyed_protection,
        envelope.any_protection & ~source_facts.active_protection,
        source_facts.active_protection & ~envelope.any_protection);
    return effect;
}

CarrierEffectSummary carrier_effect(
    const CalcContext& calc,
    const std::uint32_t source_state,
    std::vector<std::uint32_t> successor_ids) {
    if (successor_ids.empty()) successor_ids.push_back(source_state);
    return carrier_effect(
        calc, source_state,
        carrier_successor_envelope(calc, std::move(successor_ids)));
}

/* Deterministic double-double arithmetic for the ill-conditioned recurrent
 * policy systems. Every transition coefficient remains its exact stored
 * double; the wider accumulator prevents platform-specific last-bit noise
 * from being amplified into visible native/WASM value drift. */
struct WideFloat {
    double high = 0.0;
    double low = 0.0;

    WideFloat() = default;
    WideFloat(const double value) : high(value) {}
    WideFloat(const double high_value, const double low_value)
        : high(high_value), low(low_value) {}
    bool operator==(const WideFloat&) const = default;
    double value() const { return high + low; }
};

std::pair<double, double> exact_sum(const double a, const double b) {
    const double sum = a + b;
    const double b_virtual = sum - a;
    const double error =
        (a - (sum - b_virtual)) + (b - b_virtual);
    return {sum, error};
}

WideFloat wide_normalize(const double high, const double low) {
    const auto [sum, error] = exact_sum(high, low);
    return {sum, error};
}

WideFloat operator+(const WideFloat a, const WideFloat b) {
    const auto [sum, error] = exact_sum(a.high, b.high);
    return wide_normalize(sum, error + a.low + b.low);
}

WideFloat operator-(const WideFloat value) {
    return {-value.high, -value.low};
}

WideFloat operator-(const WideFloat a, const WideFloat b) {
    return a + (-b);
}

WideFloat& operator+=(WideFloat& target, const WideFloat value) {
    target = target + value;
    return target;
}

WideFloat& operator-=(WideFloat& target, const WideFloat value) {
    target = target - value;
    return target;
}

WideFloat operator*(const WideFloat a, const WideFloat b) {
    const double product = a.high * b.high;
    constexpr double split = 134217729.0; /* 2^27 + 1 */
    const double a_split = split * a.high;
    const double a_high = a_split - (a_split - a.high);
    const double a_low = a.high - a_high;
    const double b_split = split * b.high;
    const double b_high = b_split - (b_split - b.high);
    const double b_low = b.high - b_high;
    const double product_error =
        ((a_high * b_high - product) + a_high * b_low +
         a_low * b_high) +
        a_low * b_low;
    const double error = product_error + a.high * b.low +
                         a.low * b.high + a.low * b.low;
    return wide_normalize(product, error);
}

WideFloat operator*(const double a, const WideFloat b) {
    return WideFloat{a} * b;
}

WideFloat operator*(const WideFloat a, const double b) {
    return a * WideFloat{b};
}

WideFloat operator/(const WideFloat numerator, const WideFloat denominator) {
    const double first = numerator.high / denominator.high;
    const WideFloat remainder = numerator - denominator * first;
    const double second = remainder.value() / denominator.high;
    const WideFloat partial = WideFloat{first} + WideFloat{second};
    const WideFloat final_remainder = numerator - denominator * partial;
    return partial + WideFloat{final_remainder.value() / denominator.high};
}

} // namespace

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
        AutomaticTelemetryKind telemetry_kind =
            AutomaticTelemetryKind::None;
        bool template_hit = false;
        std::uint64_t template_id = 0;
        std::uint64_t raw_outcomes = 0;
        std::uint64_t admission_ns = 0;
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
    std::uint32_t max_discovered_states = 0;
    std::uint32_t max_expanded_states = 0;
    std::uint64_t max_state_action_rows = 0;
    std::uint64_t max_transitions = 0;
    std::uint64_t max_reforge_work = 0;
    std::uint32_t max_diagnostic_samples = 0;
    std::uint32_t discovered_states = 0;
    std::uint32_t expanded_states = 0;
    std::vector<std::uint8_t> expanded;
    std::vector<StateRowSpan> state_rows;
    std::vector<SparseRow> rows;
    std::vector<SparseVariant> variants;
    std::vector<std::uint32_t> row_variant_indices;
    std::vector<double> variant_quantities;
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
    std::vector<AutomaticCandidateRecord> automatic_candidate_samples;
    std::uint64_t algebraic_self_loops = 0;
    bool focused_partial = false;

    bool compatible(
        const std::uint32_t requested_start,
        const std::vector<PricedOperator>& priced,
        const SolveOptions& options) const {
        if (start_state != requested_start ||
            max_discovered_states != options.max_discovered_states ||
            max_expanded_states != options.max_expanded_states ||
            max_state_action_rows != options.max_state_action_rows ||
            max_transitions != options.max_transitions ||
            max_reforge_work != options.max_reforge_work ||
            max_diagnostic_samples != options.max_diagnostic_samples ||
            operator_indices.size() != priced.size()) {
            return false;
        }
        for (std::size_t i = 0; i < priced.size(); ++i) {
            if (operator_indices[i] != priced[i].index) return false;
        }
        return true;
    }

    std::uint64_t estimated_owned_bytes() const {
        std::uint64_t bytes = sizeof(*this);
        bytes += operator_indices.capacity() * sizeof(std::uint32_t);
        bytes += expanded.capacity() * sizeof(std::uint8_t);
        bytes += state_rows.capacity() * sizeof(StateRowSpan);
        bytes += rows.capacity() * sizeof(SparseRow);
        bytes += variants.capacity() * sizeof(SparseVariant);
        bytes += row_variant_indices.capacity() * sizeof(std::uint32_t);
        bytes += variant_quantities.capacity() * sizeof(double);
        bytes += successors.capacity() * sizeof(std::uint32_t);
        bytes += probabilities.capacity() * sizeof(double);
        bytes += choices.capacity() * sizeof(SparseChoiceGroup);
        bytes += choice_successors.capacity() * sizeof(std::uint32_t);
        bytes += choice_options.capacity() * sizeof(OutcomeChoiceOption);
        bytes += automatic_candidate_samples.capacity() *
                 sizeof(AutomaticCandidateRecord);
        for (const AutomaticCandidateRecord& record :
             automatic_candidate_samples) {
            bytes += record.evidence.legality_result.capacity() +
                     record.evidence.reason.capacity() +
                     record.candidate_id.capacity() +
                     record.setup_action_id.capacity() +
                     record.followup_action_id.capacity() +
                     record.cleanup_action_id.capacity();
        }
        return bytes;
    }
};

namespace {

std::uint64_t string_vector_owned_bytes(
    const std::vector<std::string>& values) {
    std::uint64_t total = values.capacity() * sizeof(std::string);
    for (const std::string& value : values) total += value.capacity() + 1;
    return total;
}

std::uint64_t diagnostics_owned_bytes(const SolveDiagnostics& diagnostics) {
    return string_vector_owned_bytes(diagnostics.skipped_missing_price) +
           string_vector_owned_bytes(diagnostics.skipped_unsupported) +
           string_vector_owned_bytes(diagnostics.cap_hits) +
           string_vector_owned_bytes(
               diagnostics.action_inclusion_reasons) +
           string_vector_owned_bytes(diagnostics.preservation_witnesses) +
           string_vector_owned_bytes(
               diagnostics.automatic_candidate_witnesses) +
           diagnostics.policy_evaluation_failure.capacity() + 1;
}

std::uint64_t solve_result_owned_bytes(const SolveResult& result) {
    std::uint64_t bytes = sizeof(result);
    bytes += result.values.capacity() * sizeof(double);
    bytes += result.policy.capacity() * sizeof(PolicyOperatorRef);
    bytes += result.expanded.capacity() * sizeof(std::uint8_t);
    bytes += result.goal_states.capacity() * sizeof(std::uint8_t);
    bytes += result.policy_reachable.capacity() * sizeof(std::uint8_t);
    bytes += result.unveil_preferences.capacity() *
             sizeof(std::vector<std::uint32_t>);
    for (const auto& preferences : result.unveil_preferences) {
        bytes += preferences.capacity() * sizeof(std::uint32_t);
    }
    bytes += result.option_unveil_preferences.capacity() *
             sizeof(std::vector<ObservedUnveilPreference>);
    for (const auto& preferences : result.option_unveil_preferences) {
        bytes += preferences.capacity() * sizeof(ObservedUnveilPreference);
        for (const ObservedUnveilPreference& preference : preferences) {
            bytes += preference.choices.capacity() *
                     sizeof(ObservedUnveilChoice);
        }
    }
    bytes += diagnostics_owned_bytes(result.diagnostics);
    return bytes;
}

} // namespace

struct SolveWork::Impl {
    CalcContext& calc;
    const SessionImpl& session;
    SolveOptions options;
    std::unordered_map<std::string, double> prices;
    SolveResult result;
    std::vector<PricedOperator> operators;
    std::vector<std::uint32_t> static_operator_indices;
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
    std::unordered_map<std::size_t, std::vector<std::uint64_t>>
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
    struct PolicyKernelPreparation {
        std::size_t state_count = 0;
        std::uint32_t cursor = 0;
        std::vector<std::uint32_t> kernel_owner;
        std::vector<std::vector<PolicyEdge>> full_kernel;
        std::unordered_map<std::size_t, std::vector<std::uint32_t>>
            representatives_by_hash;
        std::unordered_map<std::size_t, std::vector<std::uint32_t>>
            shared_transition_representatives;
        std::vector<std::uint32_t> representative;
        std::vector<std::vector<std::uint32_t>> group_members;
        std::vector<PolicyRow> rows;
        std::vector<PolicyEdge> edges;
        std::uint32_t grouping_cursor = 0;
        std::uint32_t quotient_cursor = 0;
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
    bool policy_selection_active = false;
    std::uint32_t policy_selection_cursor = 0;
    bool policy_selection_improved = false;
    double policy_selection_residual = 0.0;
    std::uint64_t peak_policy_scratch_bytes = 0;
    std::vector<std::uint32_t> improper_policy_states;
    struct KernelValueCache {
        std::uint64_t transition_offset = 0;
        std::uint32_t transition_count = 0;
        double finite_sum = 0.0;
        std::uint32_t infinite_count = 0;
        std::vector<double> probability_by_state;
    };
    bool kernel_value_cache_active = false;
    std::vector<KernelValueCache> kernel_value_caches;
    std::unordered_map<std::uint64_t, std::size_t> kernel_value_cache_by_offset;
    bool focused_mode = false;
    bool focus_optimizing = false;
    bool focused_lower_mode = false;
    bool focused_closure_proved = false;
    bool full_closure_after_focused_fallback = false;
    std::uint32_t next_focus_checkpoint = 32;
    std::uint64_t peak_owned_bytes = 0;
    SolvePhase phase = SolvePhase::Expanding;
    bool consumed = false;

    void retain_action_reason(std::string reason) {
        if (result.diagnostics.action_inclusion_reasons.size() <
            options.max_diagnostic_samples) {
            result.diagnostics.action_inclusion_reasons.push_back(
                std::move(reason));
        } else {
            ++result.diagnostics.action_inclusion_reasons_omitted;
        }
    }

    Impl(
        CalcContext& context,
        const pc_item_state& start_item,
        const std::unordered_map<std::string, double>& prices,
        const SolveOptions& solve_options)
        : calc(context), session(context.session()), options(solve_options),
          prices(prices),
          reported_unsupported(context.operators().size(), false) {
        const auto setup_started = std::chrono::steady_clock::now();
        options.max_expanded_states = std::min(
            options.max_expanded_states, options.max_states);
        calc.reset_solve_telemetry();
        calc.set_solve_resource_caps(
            options.max_discovered_states, options.max_reforge_work);
        result.options = options;
        result.diagnostics.diagnostic_sample_limit =
            options.max_diagnostic_samples;
        result.diagnostics.telemetry_json_byte_limit =
            options.max_telemetry_json_bytes;
        result.diagnostics.registry_actions = static_cast<std::uint32_t>(
            calc.registry().actions.size());
        result.diagnostics.candidate_actions = static_cast<std::uint32_t>(
            calc.candidates().size());
        const ActionControlSummary& control = calc.action_control();
        result.diagnostics.relevance_reduced_actions =
            control.pruned_outside_goal_relevance +
            control.pruned_outside_envelope;
        result.diagnostics.dependency_actions =
            control.dependency_primitives;
        result.diagnostics.deferred_actions =
            control.deferred_fossil_loadouts;
        retain_action_reason(
            std::string(
                control.explicit_envelope
                    ? "included:explicit_goal_envelope:"
                    : calc.registry().fossil_generation_goal_relevant
                          ? "included:bounded_goal_relevant_envelope:"
                          : "included:conservative_exhaustive_envelope:") +
            std::to_string(control.included_primitives));
        if (control.pruned_outside_envelope != 0) {
            retain_action_reason(
                "pruned:not_permitted_by_explicit_goal_envelope:" +
                std::to_string(control.pruned_outside_envelope));
        }
        if (control.pruned_outside_goal_relevance != 0) {
            retain_action_reason(
                "pruned:outside_product_goal_relevance:" +
                std::to_string(
                    control.pruned_outside_goal_relevance));
        }
        if (control.dependency_primitives != 0) {
            retain_action_reason(
                "included:fixed_option_structural_dependency:" +
                std::to_string(control.dependency_primitives));
        }
        if (control.deferred_fossil_loadouts != 0) {
            retain_action_reason(
                std::string(
                    calc.registry().fossil_generation_goal_relevant
                        ? "deferred:outside_bounded_goal_relevant_fossil_beam:"
                        : "deferred:lazy_fossil_signature_not_requested:") +
                std::to_string(control.deferred_fossil_loadouts));
        }
        if (control.automatic_options != 0) {
            retain_action_reason(
                "included:native_price_independent_automatic_options:" +
                std::to_string(control.automatic_options));
        }
        /* Primitive support remains action-registry telemetry. Fixed options
         * are priced and evaluated alongside those primitive wrappers. */
        for (const std::uint32_t index : calc.candidates()) {
            const ActionDescriptor& descriptor =
                calc.registry().actions.at(index);
            const bool supported = calc_supports(descriptor);
            if (supported) {
                ++result.diagnostics.evaluator_supported_actions;
            }
        }
        for (const std::uint32_t index : calc.candidate_operators()) {
            const PlannerOperator& planner = calc.operators().at(index);
            double cost = 0.0;
            bool priced = true;
            std::vector<std::pair<std::string, double>> resource_prices;
            for (const auto& [key, quantity] :
                 planner.resource_quantities) {
                const auto found = prices.find(key);
                if (found == prices.end()) {
                    priced = false;
                    break;
                }
                cost += quantity * found->second;
                resource_prices.push_back({key, found->second});
            }
            if (!priced) {
                record_skipped_missing_price(planner.id);
                add_action_reason(
                    "unpriced", planner.id,
                    "missing_one_or_more_resource_prices");
                if (planner.automatic_kind !=
                    AutomaticCandidateKind::None) {
                    add_action_reason(
                        "rejected", planner.id,
                        "automatic_candidate_missing_price");
                }
                continue;
            }
            ++result.diagnostics.priced_scanned_actions;
            const bool supported =
                planner.kind == PlannerOperatorKind::FixedOption ||
                calc_supports(calc.registry().actions.at(
                    planner.primitive_action));
            if (!supported) {
                reported_unsupported[index] = true;
                record_skipped_unsupported(planner.id);
                add_action_reason(
                    "unsupported", planner.id,
                    "no_exact_evaluator_for_requested_primitive");
                continue;
            }
            operators.push_back(
                {index, cost, std::move(resource_prices)});
            if (planner.kind == PlannerOperatorKind::Primitive &&
                calc.registry().actions.at(planner.primitive_action).synthetic) {
                restart_operator_index = index;
                restart_cost = cost;
            }
            ++result.diagnostics.supported_priced_actions;
        }
        const bool priced_automatic_fracture = std::any_of(
            operators.begin(), operators.end(),
            [&](const PricedOperator& priced) {
                const PlannerOperator& planner =
                    calc.operators().at(priced.index);
                return planner.kind == PlannerOperatorKind::Primitive &&
                       planner.automatic_kind ==
                           AutomaticCandidateKind::Fracture;
            });
        if (priced_automatic_fracture && restart_operator_index == kNoId) {
            throw std::invalid_argument(
                "goal-relevant Fracture planning requires a priced base for "
                "Restart miss recovery");
        }
        for (std::size_t i = 0;
             i < calc.static_candidate_operator_count(); ++i) {
            const std::uint32_t index = calc.candidate_operators().at(i);
            if (index < reported_unsupported.size() &&
                !reported_unsupported[index] &&
                std::find_if(
                    operators.begin(), operators.end(),
                    [&](const PricedOperator& priced) {
                        return priced.index == index;
                    }) != operators.end()) {
                static_operator_indices.push_back(index);
            }
        }

        result.start_state = calc.intern_item(start_item);
        priced_operator_position.assign(calc.operators().size(), -1);
        for (std::uint32_t i = 0; i < operators.size(); ++i) {
            priced_operator_position[operators[i].index] =
                static_cast<std::int32_t>(i);
        }
        const auto& cached = calc.solve_transition_cache();
        if (cached != nullptr &&
            cached->compatible(result.start_state, operators, options)) {
            transition_cache = cached;
            expanded_count = transition_cache->expanded_states;
            expanded = transition_cache->expanded;
            focused_mode = transition_cache->focused_partial;
            cache_pending = !focused_mode;
            result.diagnostics.transition_cache_reused = true;
        } else {
            transition_cache = std::make_shared<SolveTransitionCache>();
            transition_cache->start_state = result.start_state;
            transition_cache->max_discovered_states =
                options.max_discovered_states;
            transition_cache->max_expanded_states =
                options.max_expanded_states;
            transition_cache->max_state_action_rows =
                options.max_state_action_rows;
            transition_cache->max_transitions = options.max_transitions;
            transition_cache->max_reforge_work = options.max_reforge_work;
            transition_cache->max_diagnostic_samples =
                options.max_diagnostic_samples;
            for (const PricedOperator& priced : operators) {
                transition_cache->operator_indices.push_back(priced.index);
            }
            enqueue(result.start_state);
        }
        result.diagnostics.solve_setup_ns = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - setup_started)
                .count());
    }

    bool ensure_priced_operator(const std::uint32_t index) {
        if (index >= priced_operator_position.size()) {
            priced_operator_position.resize(index + 1, -1);
        }
        if (priced_operator_position[index] >= 0) return true;
        if (index >= reported_unsupported.size()) {
            reported_unsupported.resize(index + 1, false);
        }
        const PlannerOperator& planner = calc.operators().at(index);
        double cost = 0.0;
        std::vector<std::pair<std::string, double>> resource_prices;
        for (const auto& [key, quantity] : planner.resource_quantities) {
            const auto found = prices.find(key);
            if (found == prices.end()) {
                record_skipped_missing_price(planner.id);
                add_action_reason(
                    "unpriced", planner.id,
                    "missing_one_or_more_resource_prices");
                if (planner.automatic_kind !=
                    AutomaticCandidateKind::None) {
                    add_action_reason(
                        "rejected", planner.id,
                        "automatic_candidate_missing_price");
                }
                return false;
            }
            cost += quantity * found->second;
            resource_prices.push_back({key, found->second});
        }
        const bool supported =
            planner.kind == PlannerOperatorKind::FixedOption ||
            calc_supports(calc.registry().actions.at(
                planner.primitive_action));
        if (!supported) {
            reported_unsupported[index] = true;
            record_skipped_unsupported(planner.id);
            add_action_reason(
                "unsupported", planner.id,
                "no_exact_evaluator_for_requested_primitive");
            return false;
        }
        priced_operator_position[index] =
            static_cast<std::int32_t>(operators.size());
        operators.push_back({index, cost, std::move(resource_prices)});
        transition_cache->operator_indices.push_back(index);
        ++result.diagnostics.priced_scanned_actions;
        ++result.diagnostics.supported_priced_actions;
        return true;
    }

    SolveTransitionCache::AutomaticCandidateRecord automatic_record_from(
        const std::uint32_t state,
        const StateLocalAutomaticCandidate& decision) const {
        SolveTransitionCache::AutomaticCandidateRecord record;
        record.state_id = state;
        record.operator_index = decision.operator_index;
        record.candidate_id = decision.id;
        record.candidate_kind = decision.kind;
        record.eligible = decision.evidence.eligible;
        record.collapsed = decision.collapsed;
        record.deferred = decision.deferred;
        record.telemetry_kind = decision.telemetry_kind;
        record.template_hit = decision.template_hit;
        record.template_id = decision.template_id;
        record.raw_outcomes = decision.raw_outcomes;
        record.admission_ns = decision.admission_ns;
        record.selected_bytes = decision.selected_bytes;
        record.evidence = decision.evidence;
        if (decision.operator_index < calc.operators().size()) {
            const PlannerOperator& planner =
                calc.operators().at(decision.operator_index);
            const auto id = [&](const std::uint32_t action) {
                return action == kNoId
                           ? std::string()
                           : calc.registry().actions.at(action).id;
            };
            record.setup_action_id = id(planner.setup_action);
            record.followup_action_id = id(planner.followup_action);
            record.cleanup_action_id = id(planner.cleanup_action);
        }
        return record;
    }

    void prepare_state_expansion(const std::uint32_t state) {
        const auto prepare_started = std::chrono::steady_clock::now();
        expansion_operator_indices = static_operator_indices;
        AutomaticAdmissionLimits limits;
        limits.max_discovered_states = options.max_discovered_states;
        limits.max_state_action_rows = options.max_state_action_rows;
        limits.max_transitions = options.max_transitions;
        limits.max_reforge_work = options.max_reforge_work;
        const auto byte_audit_started = std::chrono::steady_clock::now();
        const std::uint64_t calc_bytes =
            calc.fast_estimated_owned_bytes();
        const std::uint64_t total_bytes =
            fast_estimated_owned_bytes();
        result.diagnostics.expansion_prepare_byte_audit_ns +=
            static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() - byte_audit_started)
                    .count());
        const std::uint64_t other_bytes =
            total_bytes > calc_bytes ? total_bytes - calc_bytes : 0;
        limits.max_solver_owned_bytes =
            options.max_solver_owned_bytes > other_bytes
                ? options.max_solver_owned_bytes - other_bytes
                : 1;
        limits.max_imprint_program_depth =
            options.max_imprint_program_depth;
        limits.max_imprint_program_work =
            options.max_imprint_program_work;
        limits.prices = &prices;
        const auto admission_started = std::chrono::steady_clock::now();
        StateLocalAutomaticBatch batch =
            calc.admit_state_local_automatic_candidates(state, limits);
        result.diagnostics.expansion_prepare_admission_ns +=
            static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() - admission_started)
                    .count());
        const auto diagnostics_started = std::chrono::steady_clock::now();
        if (batch.temporary_precompiled_classes != 0 ||
            batch.temporary_precompile_ns != 0 ||
            batch.temporary_candidate_variants != 0 ||
            batch.temporary_effect_classes != 0 ||
            batch.temporary_enumeration_ns != 0) {
            SolveTransitionCache::AutomaticCandidateRecord timing;
            timing.state_id = state;
            timing.telemetry_kind = AutomaticTelemetryKind::TemporaryBench;
            timing.count_candidate = false;
            timing.precompiled_classes =
                batch.temporary_precompiled_classes;
            timing.precompile_ns = batch.temporary_precompile_ns;
            timing.precompiled_bytes = batch.temporary_precompiled_bytes;
            timing.candidate_variants =
                batch.temporary_candidate_variants;
            timing.effect_classes = batch.temporary_effect_classes;
            timing.collapsed_variants =
                batch.temporary_collapsed_variants;
            timing.enumeration_ns = batch.temporary_enumeration_ns;
            retain_automatic_candidate_record(std::move(timing));
        }
        for (std::size_t i = 0;
             i < batch.shared_admission_ns.size(); ++i) {
            if (batch.shared_admission_ns[i] == 0) continue;
            SolveTransitionCache::AutomaticCandidateRecord timing;
            timing.state_id = state;
            timing.telemetry_kind =
                static_cast<AutomaticTelemetryKind>(i);
            timing.count_candidate = false;
            timing.admission_ns = batch.shared_admission_ns[i];
            retain_automatic_candidate_record(std::move(timing));
        }
        for (const StateLocalAutomaticCandidate& decision : batch.decisions) {
            if (decision.missing_price) {
                record_skipped_missing_price(decision.id);
                add_action_reason(
                    "unpriced", decision.id,
                    "missing_one_or_more_resource_prices");
                add_action_reason(
                    "rejected", decision.id,
                    "automatic_candidate_missing_price");
            }
            retain_automatic_candidate_record(
                automatic_record_from(state, decision));
            if (decision.admitted && decision.operator_index != kNoId) {
                automatic_admission_records.insert(
                    (static_cast<std::uint64_t>(state) << 32) |
                    decision.operator_index);
            }
        }
        result.diagnostics.expansion_prepare_diagnostics_ns +=
            static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() - diagnostics_started)
                    .count());
        const auto pricing_started = std::chrono::steady_clock::now();
        for (const std::uint32_t index : batch.admitted_operators) {
            if (ensure_priced_operator(index) &&
                std::find(
                    expansion_operator_indices.begin(),
                    expansion_operator_indices.end(), index) ==
                    expansion_operator_indices.end()) {
                expansion_operator_indices.push_back(index);
            }
        }
        result.diagnostics.expansion_prepare_pricing_ns +=
            static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() - pricing_started)
                    .count());
        result.diagnostics.expansion_prepare_ns +=
            static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() - prepare_started)
                    .count());
    }

    double acceptable_residual() const {
        /* Preserve the measured absolute residual, but terminate a stable
         * policy using epsilon as a relative tolerance at V(start)'s scale.
         * Large endgame values otherwise repeat the identical fixed policy
         * indefinitely on last-bit double-precision noise. */
        double scale = 1.0;
        if (result.start_state < result.values.size()) {
            const double start = result.values[result.start_state];
            if (std::isfinite(start) && start < kValueCeiling) {
                scale = std::max(1.0, std::abs(start));
            }
        }
        return options.epsilon * scale;
    }

    bool optimization_converged() const {
        if (policy_iteration_failed) {
            return residual <= options.epsilon;
        }
        return policy_initialized && policy_stable &&
               residual <= acceptable_residual();
    }

    void enqueue(const std::uint32_t state) {
        if (state >= queued.size()) queued.resize(state + 1, 0);
        if (queued[state]) return;
        queued[state] = 1;
        queue.push_back(state);
        peak_queue_size = std::max<std::uint32_t>(
            peak_queue_size, static_cast<std::uint32_t>(queue.size()));
    }

    bool same_kernel(
        const SparseRow& stored,
        const PendingSparseRow& pending) const {
        const auto is_self = [&](const std::uint32_t successor) {
            return pending.entry_relative_self ? successor == kNoId
                                               : successor == pending.state;
        };
        std::size_t transition_count = 0;
        double self_probability = 0.0;
        if (pending.transitions != nullptr) {
            for (const OutcomeEntry& entry : *pending.transitions) {
                if (pending.entry_relative_self && is_self(entry.state)) {
                    self_probability += entry.probability;
                } else {
                    ++transition_count;
                    if (!pending.entry_relative_self && is_self(entry.state)) {
                        self_probability += entry.probability;
                    }
                }
            }
        }
        const std::size_t choice_count =
            pending.choices == nullptr ? 0 : pending.choices->size();
        if (stored.transition_count != transition_count ||
            stored.choice_count != choice_count ||
            (pending.entry_relative_self &&
             stored.self_probability != self_probability)) {
            return false;
        }
        std::size_t stored_transition = 0;
        if (pending.transitions != nullptr) {
            for (const OutcomeEntry& right : *pending.transitions) {
                if (pending.entry_relative_self && is_self(right.state)) {
                    continue;
                }
                const std::uint64_t offset =
                    stored.transition_offset + stored_transition++;
                if (transition_cache->successors.at(offset) != right.state ||
                    transition_cache->probabilities.at(offset) !=
                        right.probability) {
                    return false;
                }
            }
        }
        for (std::size_t i = 0; i < choice_count; ++i) {
            const SparseChoiceGroup& left = transition_cache->choices.at(
                stored.choice_offset + i);
            const OutcomeChoiceGroup& right = pending.choices->at(i);
            const bool has_self = std::find(
                right.states.begin(), right.states.end(),
                pending.entry_relative_self ? kNoId : pending.state) !=
                right.states.end();
            const std::size_t successor_count =
                right.states.size() - (has_self ? 1u : 0u);
            if (left.probability != right.probability ||
                left.has_self != has_self ||
                left.successor_count != successor_count) {
                return false;
            }
            std::size_t stored_successor = 0;
            for (const std::uint32_t successor : right.states) {
                if (is_self(successor)) continue;
                if (transition_cache->choice_successors.at(
                        left.successor_offset + stored_successor++) !=
                    successor) {
                    return false;
                }
            }
        }
        return true;
    }

    std::size_t kernel_hash(const PendingSparseRow& pending) const {
        const auto mix = [](std::size_t& hash, std::size_t value) {
            hash ^= value + 0x9e3779b97f4a7c15ULL + (hash << 6) +
                    (hash >> 2);
        };
        std::size_t hash = 2166136261u;
        if (pending.transitions != nullptr) {
            for (const OutcomeEntry& entry : *pending.transitions) {
                if (pending.entry_relative_self && entry.state == kNoId) {
                    mix(hash, 0x73656c66u);
                    mix(hash, std::hash<double>{}(entry.probability));
                    continue;
                }
                mix(hash, entry.state);
                mix(hash, std::hash<double>{}(entry.probability));
            }
            mix(hash, pending.transitions->size());
        }
        if (pending.choices != nullptr) {
            mix(hash, pending.choices->size());
            for (const OutcomeChoiceGroup& group : *pending.choices) {
                mix(hash, std::hash<double>{}(group.probability));
                const bool has_self = std::find(
                    group.states.begin(), group.states.end(),
                    pending.entry_relative_self ? kNoId : pending.state) !=
                    group.states.end();
                mix(hash, has_self ? 1u : 0u);
                mix(hash, group.states.size() - (has_self ? 1u : 0u));
                for (const std::uint32_t state : group.states) {
                    if ((pending.entry_relative_self && state == kNoId) ||
                        (!pending.entry_relative_self &&
                         state == pending.state)) {
                        continue;
                    }
                    mix(hash, state);
                }
            }
        }
        return hash;
    }

    void add_action_reason(
        const char* disposition,
        const std::string& action,
        const std::string& reason) {
        retain_action_reason(
            std::string(disposition) + ":" + reason + ":" + action);
    }

    void record_skipped_missing_price(const std::string& action) {
        ++result.diagnostics.skipped_missing_price_count;
        if (result.diagnostics.skipped_missing_price.size() <
            options.max_diagnostic_samples) {
            result.diagnostics.skipped_missing_price.push_back(action);
        }
    }

    void record_skipped_unsupported(const std::string& action) {
        ++result.diagnostics.skipped_unsupported_count;
        if (result.diagnostics.skipped_unsupported.size() <
            options.max_diagnostic_samples) {
            result.diagnostics.skipped_unsupported.push_back(action);
        }
    }

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
        const std::uint64_t row_index) const {
        PreservationDecision decision;
        if (!options.preservation_control || focused_lower_mode ||
            row_index >= transition_cache->rows.size() ||
            row_index >= priced_rows.size()) {
            return decision;
        }
        const PricedSparseRow& priced = priced_rows[row_index];
        if (priced.operator_index == kNoId) return decision;
        const PlannerOperator& planner =
            calc.operators().at(priced.operator_index);
        if (planner.kind != PlannerOperatorKind::Primitive) return decision;
        const ActionDescriptor& action = calc.registry().actions.at(
            planner.primitive_action);
        if (!action.preservation.destructive_renewal) return decision;

        const SparseRow& row = transition_cache->rows.at(row_index);
        const CarrierFacts facts = carrier_facts(calc.state(row.owner_state));
        std::uint32_t progressed = 0;
        if (facts.goal_family_mask != 0) progressed |= kCarrierGoalFamilies;
        if (facts.satisfied_goal_mask != 0) {
            progressed |= kCarrierSatisfiedGoalSubset;
        }
        if (facts.junk_count != 0 || facts.blocked_mask != 0) {
            progressed |= kCarrierJunkBlockers;
        }
        if (facts.crafted_goal_mask != 0 || facts.crafted_junk_count != 0) {
            progressed |= kCarrierCraftedState;
        }
        if (facts.fractured_goal_mask != 0 ||
            facts.fractured_junk_count != 0) {
            progressed |= kCarrierFracturedState;
        }
        if (facts.prefix_count != 0) progressed |= kCarrierPrefixSide;
        if (facts.suffix_count != 0) progressed |= kCarrierSuffixSide;
        if (facts.active_protection != 0) {
            progressed |= kCarrierActiveProtection;
        }
        decision.destroyed_progress =
            row.preservation_effect.destroyed_properties & progressed;
        decision.candidate_lower_bound = priced.cost;

        /* Exact state identity with the synthetic Restart successor is the
         * deliberately strict disposable-carrier certificate. No label,
         * depth, price, or UI stage participates in this proof. */
        if (restart_state != kNoId && row.owner_state == restart_state) {
            decision.disposition =
                PreservationDisposition::RetainedDisposable;
            return decision;
        }
        if (decision.destroyed_progress == 0) {
            decision.disposition =
                PreservationDisposition::RetainedPreserving;
            return decision;
        }

        if (restart_operator_index == kNoId || restart_state == kNoId ||
            restart_state >= result.values.size() ||
            !std::isfinite(result.values[restart_state]) ||
            result.values[restart_state] >= kValueCeiling ||
            !std::isfinite(restart_cost) || restart_cost < 0.0 ||
            priced.cost < 0.0) {
            decision.disposition =
                PreservationDisposition::RetainedUncertain;
            return decision;
        }
        decision.restart_upper_bound =
            restart_cost + result.values[restart_state];
        /* Costs and continuation values are non-negative. Therefore the
         * candidate's immediate cost is an admissible lower bound on its
         * complete Q value, while Restart plus the current monotone value of
         * its exact successor is a constructive upper bound. Strict
         * inequality preserves price/action ties. */
        if (decision.candidate_lower_bound >
            decision.restart_upper_bound + options.epsilon) {
            decision.disposition =
                PreservationDisposition::PrunedByRestartBound;
        } else {
            decision.disposition =
                PreservationDisposition::RetainedUncertain;
        }
        return decision;
    }

    bool preservation_prunes(const std::uint64_t row_index) const {
        return preservation_decision(row_index).disposition ==
               PreservationDisposition::PrunedByRestartBound;
    }

    static void append_json_string(
        std::string& out,
        const std::string& value) {
        out.push_back('"');
        for (const char c : value) {
            if (c == '"' || c == '\\') out.push_back('\\');
            if (c == '\n') {
                out += "\\n";
            } else {
                out.push_back(c);
            }
        }
        out.push_back('"');
    }

    static std::string finite_json(const double value) {
        if (!std::isfinite(value)) return "null";
        char buffer[64];
        std::snprintf(buffer, sizeof(buffer), "%.17g", value);
        return buffer;
    }

    static std::string property_mask_json(const std::uint32_t mask) {
        std::string out = "[";
        bool first = true;
        const auto add = [&](const std::uint32_t bit, const char* name) {
            if ((mask & bit) == 0) return;
            if (!first) out.push_back(',');
            first = false;
            append_json_string(out, name);
        };
        add(kCarrierGoalFamilies, "goal_families");
        add(kCarrierSatisfiedGoalSubset, "satisfied_goal_subset");
        add(kCarrierJunkBlockers, "junk_blockers");
        add(kCarrierCraftedState, "crafted_state");
        add(kCarrierFracturedState, "fractured_state");
        add(kCarrierPrefixSide, "prefix_side");
        add(kCarrierSuffixSide, "suffix_side");
        add(kCarrierActiveProtection, "active_protection");
        out.push_back(']');
        return out;
    }

    static std::string count_vector_json(const CompactCountVector& counts) {
        std::string out = "[";
        for (std::size_t i = 0; i < counts.size(); ++i) {
            if (i != 0) out.push_back(',');
            out += std::to_string(counts[i]);
        }
        out.push_back(']');
        return out;
    }

    std::string preservation_witness_json(
        const std::uint64_t row_index,
        const PreservationDecision& decision) const {
        const SparseRow& row = transition_cache->rows.at(row_index);
        const PricedSparseRow& priced = priced_rows.at(row_index);
        const PlannerOperator& planner =
            calc.operators().at(priced.operator_index);
        const ActionDescriptor& action = calc.registry().actions.at(
            planner.primitive_action);
        const AbstractState& state = calc.state(row.owner_state);
        const CarrierFacts facts = carrier_facts(state);
        const CarrierEffectSummary& effect = row.preservation_effect;
        const bool disposable = decision.disposition ==
                                PreservationDisposition::RetainedDisposable;
        const bool pruned = decision.disposition ==
                            PreservationDisposition::PrunedByRestartBound;
        const char* disposition =
            pruned
                ? "pruned"
                : decision.disposition ==
                          PreservationDisposition::RetainedUncertain
                      ? "included_uncertain"
                      : "included";
        const char* reason =
            disposable
                ? "certified_genuine_restart_carrier"
                : decision.disposition ==
                          PreservationDisposition::RetainedPreserving
                      ? "exact_transition_preserves_current_progress"
                      : pruned
                            ? "candidate_lower_bound_exceeds_restart_route_upper_bound"
                            : "no_exact_dominance_proof_retain_candidate";

        std::string out = "{";
        out += "\"state_id\":" + std::to_string(row.owner_state);
        out += ",\"action\":";
        append_json_string(out, planner.id);
        out += ",\"disposition\":";
        append_json_string(out, disposition);
        out += ",\"reason\":";
        append_json_string(out, reason);
        out += ",\"carrier\":{";
        out += "\"state_hash\":" + std::to_string(facts.state_hash);
        out += ",\"rarity\":" + std::to_string(facts.rarity);
        out += ",\"prefix_count\":" +
               std::to_string(facts.prefix_count);
        out += ",\"suffix_count\":" +
               std::to_string(facts.suffix_count);
        out += ",\"goal_family_mask\":" +
               std::to_string(facts.goal_family_mask);
        out += ",\"satisfied_goal_mask\":" +
               std::to_string(facts.satisfied_goal_mask);
        out += ",\"blocked_mask\":" +
               std::to_string(facts.blocked_mask);
        out += ",\"junk_counts\":" + count_vector_json(state.junk_counts);
        out += ",\"crafted_goal_mask\":" +
               std::to_string(facts.crafted_goal_mask);
        out += ",\"crafted_junk_counts\":" +
               count_vector_json(state.crafted_junk_counts);
        out += ",\"fractured_goal_mask\":" +
               std::to_string(facts.fractured_goal_mask);
        out += ",\"fractured_junk_counts\":" +
               count_vector_json(state.fractured_junk_counts);
        out += ",\"fractured_metamod_flags\":" +
               std::to_string(state.fractured_metamod_flags);
        out += ",\"active_protection_flags\":" +
               std::to_string(facts.active_protection) + "}";
        out += ",\"satisfied_goal_subset\":" +
               std::to_string(facts.satisfied_goal_mask);
        out += ",\"effects\":{";
        out += "\"preserved\":{";
        out += "\"properties\":" +
               property_mask_json(effect.preserved_properties);
        out += ",\"goal_family_mask\":" +
               std::to_string(effect.preserved_goal_family_mask);
        out += ",\"satisfied_goal_mask\":" +
               std::to_string(effect.preserved_satisfied_goal_mask);
        out += ",\"crafted_goal_mask\":" +
               std::to_string(effect.preserved_crafted_goal_mask);
        out += ",\"fractured_goal_mask\":" +
               std::to_string(effect.preserved_fractured_goal_mask) + "}";
        out += ",\"destroyed\":{";
        out += "\"properties\":" +
               property_mask_json(effect.destroyed_properties);
        out += ",\"goal_family_mask\":" +
               std::to_string(effect.destroyed_goal_family_mask);
        out += ",\"satisfied_goal_mask\":" +
               std::to_string(effect.destroyed_satisfied_goal_mask);
        out += ",\"crafted_goal_mask\":" +
               std::to_string(effect.destroyed_crafted_goal_mask);
        out += ",\"fractured_goal_mask\":" +
               std::to_string(effect.destroyed_fractured_goal_mask) + "}";
        out += ",\"created\":{";
        out += "\"properties\":" +
               property_mask_json(effect.created_properties);
        out += ",\"goal_family_mask\":" +
               std::to_string(effect.created_goal_family_mask);
        out += ",\"satisfied_goal_mask\":" +
               std::to_string(effect.created_satisfied_goal_mask) + "}";
        out += ",\"made_unreachable\":{";
        out += "\"properties\":" +
               property_mask_json(effect.unreachable_properties);
        out += ",\"goal_family_mask\":" +
               std::to_string(effect.unreachable_goal_family_mask);
        out += ",\"satisfied_goal_mask\":" +
               std::to_string(effect.unreachable_satisfied_goal_mask) + "}";
        out += ",\"prefix_count_range\":[" +
               std::to_string(effect.min_prefix_count) + "," +
               std::to_string(effect.max_prefix_count) + "]";
        out += ",\"suffix_count_range\":[" +
               std::to_string(effect.min_suffix_count) + "," +
               std::to_string(effect.max_suffix_count) + "]}";
        out += ",\"protection\":{";
        out += "\"active_flags\":" +
               std::to_string(facts.active_protection);
        out += ",\"respects_prefix_lock\":" +
               std::string(action.preservation.respects_prefix_lock
                               ? "true"
                               : "false");
        out += ",\"respects_suffix_lock\":" +
               std::string(action.preservation.respects_suffix_lock
                               ? "true"
                               : "false");
        out += ",\"respects_cannot_roll_attack\":" +
               std::string(
                   action.preservation.respects_cannot_roll_attack
                       ? "true"
                       : "false");
        out += ",\"respects_cannot_roll_caster\":" +
               std::string(
                   action.preservation.respects_cannot_roll_caster
                       ? "true"
                       : "false");
        out += ",\"preserved_flags\":" +
               std::to_string(effect.preserved_protection);
        out += ",\"destroyed_flags\":" +
               std::to_string(effect.destroyed_protection);
        out += ",\"fractured_preservation_independent\":" +
               std::string(
                   action.preservation.preserves_fractured_affixes
                       ? "true"
                       : "false") + "}";
        out += ",\"restart_equivalence\":{";
        out += "\"certified\":" +
               std::string(disposable ? "true" : "false");
        out += ",\"kind\":";
        if (disposable) {
            append_json_string(out, "genuine_restart_state_identity");
        } else {
            out += "null";
        }
        out += ",\"restart_state_id\":" +
               (restart_state == kNoId ? std::string("null")
                                       : std::to_string(restart_state));
        out += ",\"carrier_equals_restart_state\":" +
               std::string(disposable ? "true" : "false") + "}";
        out += ",\"dominance\":{";
        out += "\"dominating_action\":";
        if (pruned) append_json_string(out, "restart");
        else out += "null";
        out += ",\"candidate_lower_bound\":" +
               finite_json(decision.candidate_lower_bound);
        out += ",\"restart_cost\":" + finite_json(restart_cost);
        out += ",\"continuation_state_id\":" +
               (restart_state == kNoId ? std::string("null")
                                       : std::to_string(restart_state));
        const double continuation =
            restart_state < result.values.size()
                ? result.values[restart_state]
                : kInfinity;
        out += ",\"continuation_upper_bound\":" +
               finite_json(continuation);
        out += ",\"route_upper_bound\":" +
               finite_json(decision.restart_upper_bound);
        out += ",\"comparison\":";
        append_json_string(
            out,
            pruned ? "candidate_lower_bound > restart_route_upper_bound"
                   : "no_strict_dominance");
        out += "}";
        out += ",\"uncertain_retention\":{";
        out += "\"retained\":" +
               std::string(
                   decision.disposition ==
                           PreservationDisposition::RetainedUncertain
                       ? "true"
                       : "false");
        out += ",\"reason\":";
        if (decision.disposition ==
            PreservationDisposition::RetainedUncertain) {
            append_json_string(
                out, "no_exact_dominance_or_equivalence_proof");
        } else {
            out += "null";
        }
        out += "}}";
        return out;
    }

    static const char* automatic_kind_name(
        const AutomaticCandidateKind kind) {
        switch (kind) {
        case AutomaticCandidateKind::Fracture:
            return "fracture";
        case AutomaticCandidateKind::PermanentBench:
            return "permanent_bench";
        case AutomaticCandidateKind::TemporaryBenchBlocker:
            return "temporary_bench_blocker";
        case AutomaticCandidateKind::ProtectedMetamod:
            return "protected_metamod";
        case AutomaticCandidateKind::MultimodFinish:
            return "multimod_finish";
        case AutomaticCandidateKind::Imprint:
            return "imprint";
        case AutomaticCandidateKind::None:
            return "none";
        }
        return "none";
    }

    static std::string automatic_mechanisms_json(
        const std::uint32_t mechanisms) {
        std::string out = "[";
        bool first = true;
        const auto add = [&](const std::uint32_t bit, const char* name) {
            if ((mechanisms & bit) == 0) return;
            if (!first) out.push_back(',');
            first = false;
            append_json_string(out, name);
        };
        add(kAutomaticGroupConflict, "mod_group_conflict");
        add(kAutomaticPrefixSlot, "occupied_prefix_slot");
        add(kAutomaticSuffixSlot, "occupied_suffix_slot");
        add(kAutomaticMetamodProtection, "metamod_protection");
        add(kAutomaticCarrierFracture, "carrier_exact_fracture");
        add(kAutomaticDeterministicFinish, "deterministic_finish");
        add(kAutomaticImprintCheckpoint, "imprint_checkpoint_restore");
        out.push_back(']');
        return out;
    }

    std::string automatic_candidate_witness_json(
        const SolveTransitionCache::AutomaticCandidateRecord& record,
        const char* disposition,
        const char* decision_reason) const {
        const PlannerOperator* planner =
            record.operator_index < calc.operators().size()
                ? &calc.operators().at(record.operator_index)
                : nullptr;
        const AbstractState& state = calc.state(record.state_id);
        const CarrierFacts carrier = carrier_facts(state);
        const auto action_id = [&](const std::uint32_t index) {
            return index == kNoId ? std::string()
                                  : calc.registry().actions.at(index).id;
        };
        std::string out = "{";
        out += "\"candidate_kind\":";
        append_json_string(
            out, automatic_kind_name(
                record.candidate_kind != AutomaticCandidateKind::None
                    ? record.candidate_kind
                    : planner != nullptr
                          ? planner->automatic_kind
                          : AutomaticCandidateKind::None));
        out += ",\"candidate\":";
        append_json_string(
            out, record.candidate_id.empty() && planner != nullptr
                     ? planner->id
                     : record.candidate_id);
        out += ",\"carrier_identity\":{\"state_id\":" +
               std::to_string(record.state_id) +
               ",\"state_hash\":" + std::to_string(carrier.state_hash) +
               ",\"rarity\":" + std::to_string(carrier.rarity) +
               ",\"prefix_count\":" +
               std::to_string(carrier.prefix_count) +
               ",\"suffix_count\":" +
               std::to_string(carrier.suffix_count) +
               ",\"crafted_goal_mask\":" +
               std::to_string(carrier.crafted_goal_mask) +
               ",\"fractured_goal_mask\":" +
               std::to_string(carrier.fractured_goal_mask) +
               ",\"active_protection_flags\":" +
               std::to_string(carrier.active_protection) + "}";
        out += ",\"relevant_goal_subset\":" +
               std::to_string(record.evidence.relevant_goal_mask);
        out += ",\"legality_result\":";
        append_json_string(out, record.evidence.legality_result);
        out += ",\"exact_kernel_change\":{\"changed\":" +
               std::string(record.evidence.kernel_changed ? "true" : "false") +
               ",\"baseline_hash\":" +
               std::to_string(record.evidence.baseline_kernel_hash) +
               ",\"candidate_hash\":" +
               std::to_string(record.evidence.candidate_kernel_hash) +
               ",\"mechanisms\":" + automatic_mechanisms_json(
                   record.evidence.kernel_change_mechanisms) + "}";
        out += ",\"setup_operations\":[";
        if (!record.setup_action_id.empty()) {
            append_json_string(out, record.setup_action_id);
        } else if (planner != nullptr && planner->setup_action != kNoId) {
            append_json_string(out, action_id(planner->setup_action));
        }
        out += "],\"followup_operation\":";
        if (!record.followup_action_id.empty()) {
            append_json_string(out, record.followup_action_id);
        } else if (planner == nullptr || planner->followup_action == kNoId) {
            out += "null";
        } else {
            append_json_string(out, action_id(planner->followup_action));
        }
        out += ",\"cleanup_operations\":[";
        if (!record.cleanup_action_id.empty()) {
            append_json_string(out, record.cleanup_action_id);
        } else if (planner != nullptr && planner->cleanup_action != kNoId) {
            append_json_string(out, action_id(planner->cleanup_action));
        }
        out += "],\"coverage\":{\"setup_complete\":" +
               std::string(record.evidence.setup_complete ? "true" : "false") +
               ",\"cleanup_complete\":" +
               std::string(record.evidence.cleanup_complete ? "true" : "false") +
               ",\"recovery_complete\":" +
               std::string(record.evidence.recovery_complete ? "true" : "false") +
               ",\"outer_exits_complete\":" +
               std::string(record.evidence.exits_complete ? "true" : "false") +
               "}";
        out += ",\"disposition\":";
        append_json_string(out, disposition);
        out += ",\"reason\":";
        append_json_string(out, decision_reason);
        out += ",\"eligibility_reason\":";
        append_json_string(out, record.evidence.reason);
        out += "}";
        return out;
    }

    void retain_automatic_candidate_record(
        SolveTransitionCache::AutomaticCandidateRecord record) {
        if (record.count_candidate) {
            ++transition_cache->automatic_rows_considered;
            if (record.deferred) {
                ++transition_cache->automatic_rows_deferred;
            } else if (!record.eligible) {
                ++transition_cache->automatic_rows_rejected;
            } else {
                ++transition_cache->automatic_rows_eligible;
                if (record.collapsed) {
                    ++transition_cache->automatic_rows_collapsed;
                }
            }
        }
        if (record.telemetry_kind != AutomaticTelemetryKind::None) {
            AutomaticKindTelemetry& kind =
                transition_cache->automatic_kind_telemetry.at(
                    static_cast<std::size_t>(record.telemetry_kind));
            if (record.count_candidate) {
                ++kind.candidates;
                if (record.template_id != 0) {
                    if (record.template_hit) ++kind.template_hits;
                    else ++kind.unique_templates;
                }
                kind.raw_outcomes += record.raw_outcomes;
                kind.admission_ns += record.admission_ns;
            }
            kind.precompiled_classes = std::max(
                kind.precompiled_classes, record.precompiled_classes);
            kind.precompile_ns = std::max(
                kind.precompile_ns, record.precompile_ns);
            kind.precompiled_bytes = std::max(
                kind.precompiled_bytes, record.precompiled_bytes);
            kind.candidate_variants += record.candidate_variants;
            kind.effect_classes += record.effect_classes;
            kind.collapsed_variants += record.collapsed_variants;
            kind.enumeration_ns += record.enumeration_ns;
            kind.rows += record.retained_rows;
            kind.retained_transitions += record.retained_transitions;
            kind.row_ns += record.row_ns;
            kind.selected_bytes += record.selected_bytes;
            const std::uint64_t carrier_key =
                (static_cast<std::uint64_t>(record.state_id) << 8) |
                static_cast<std::uint64_t>(record.telemetry_kind);
            auto [carrier, inserted] = automatic_carrier_work.try_emplace(
                carrier_key, AutomaticCarrierWork{});
            if (inserted) ++kind.carriers;
            if (record.count_candidate) {
                ++carrier->second.candidates;
                if (record.template_id != 0 && !record.template_hit) {
                    ++carrier->second.templates;
                }
            }
            carrier->second.candidate_variants +=
                record.candidate_variants;
            carrier->second.effect_classes += record.effect_classes;
            carrier->second.rows += record.retained_rows;
            kind.max_candidates_per_carrier = std::max(
                kind.max_candidates_per_carrier,
                carrier->second.candidates);
            kind.max_templates_per_carrier = std::max(
                kind.max_templates_per_carrier,
                carrier->second.templates);
            kind.max_candidate_variants_per_carrier = std::max(
                kind.max_candidate_variants_per_carrier,
                carrier->second.candidate_variants);
            kind.max_effect_classes_per_carrier = std::max(
                kind.max_effect_classes_per_carrier,
                carrier->second.effect_classes);
            kind.max_rows_per_carrier = std::max(
                kind.max_rows_per_carrier, carrier->second.rows);
        }
        if (record.count_candidate &&
            transition_cache->automatic_candidate_samples.size() <
            options.max_diagnostic_samples) {
            transition_cache->automatic_candidate_samples.push_back(
                std::move(record));
        }
    }

    void finalize_automatic_candidate_diagnostics() {
        result.diagnostics.automatic_rows_considered =
            transition_cache->automatic_rows_considered;
        result.diagnostics.automatic_rows_eligible =
            transition_cache->automatic_rows_eligible;
        result.diagnostics.automatic_rows_rejected =
            transition_cache->automatic_rows_rejected;
        result.diagnostics.automatic_rows_collapsed =
            transition_cache->automatic_rows_collapsed;
        result.diagnostics.automatic_rows_selected = 0;
        result.diagnostics.automatic_rows_deferred =
            transition_cache->automatic_rows_deferred;
        result.diagnostics.automatic_kind_telemetry =
            transition_cache->automatic_kind_telemetry;
        result.diagnostics.automatic_candidate_witnesses.clear();
        result.diagnostics.automatic_candidate_witnesses_omitted =
            result.diagnostics.automatic_rows_considered -
            transition_cache->automatic_candidate_samples.size();
        for (std::uint32_t state = 0; state < result.policy.size(); ++state) {
            const std::uint32_t operator_index = result.policy[state].index;
            if (operator_index != kNoId &&
                calc.operators().at(operator_index).automatic_kind !=
                    AutomaticCandidateKind::None) {
                ++result.diagnostics.automatic_rows_selected;
            }
        }
        for (const auto& record :
             transition_cache->automatic_candidate_samples) {
            const bool selected =
                record.operator_index != kNoId &&
                record.state_id < result.policy.size() &&
                result.policy[record.state_id].index == record.operator_index;
            const char* disposition = "included";
            const char* reason = "retained_for_minimum_expected_cost_bellman_step";
            if (record.deferred) {
                disposition = "deferred";
                reason = "solver_resource_cap_before_exact_row_completion";
            } else if (!record.eligible) {
                disposition = "rejected";
                reason = "exact_invalidity_legality_or_relevance";
            } else {
                if (record.collapsed) {
                    disposition = "collapsed";
                    reason = "exact_kernel_variant_retained_for_price_choice";
                }
                if (selected) {
                    disposition = "selected";
                    reason = "minimum_complete_expected_downstream_cost";
                }
            }
            result.diagnostics.automatic_candidate_witnesses.push_back(
                automatic_candidate_witness_json(
                    record, disposition, reason));
        }
    }

    void finalize_preservation_diagnostics() {
        result.diagnostics.preservation_rows_considered = 0;
        result.diagnostics.preservation_rows_pruned = 0;
        result.diagnostics.preservation_rows_retained = 0;
        result.diagnostics.certified_disposable_rows = 0;
        result.diagnostics.preservation_witnesses.clear();
        result.diagnostics.preservation_witnesses_omitted = 0;
        if (!options.preservation_control) return;
        for (std::uint32_t state = 0;
             state < transition_cache->state_rows.size(); ++state) {
            const StateRowSpan& span = transition_cache->state_rows[state];
            for (std::uint32_t i = 0; i < span.count; ++i) {
                const std::uint64_t row_index = span.offset + i;
                const PreservationDecision decision =
                    preservation_decision(row_index);
                if (decision.disposition ==
                    PreservationDisposition::NotApplicable) {
                    continue;
                }
                ++result.diagnostics.preservation_rows_considered;
                const PricedSparseRow& priced = priced_rows[row_index];
                const std::string& action =
                    calc.operators().at(priced.operator_index).id;
                if (decision.disposition ==
                    PreservationDisposition::PrunedByRestartBound) {
                    ++result.diagnostics.preservation_rows_pruned;
                    add_action_reason(
                        "pruned", action,
                        "preservation_restart_bound_state_" +
                            std::to_string(state));
                } else {
                    ++result.diagnostics.preservation_rows_retained;
                    if (decision.disposition ==
                        PreservationDisposition::RetainedDisposable) {
                        ++result.diagnostics.certified_disposable_rows;
                        add_action_reason(
                            "included", action,
                            "certified_genuine_restart_carrier_state_" +
                                std::to_string(state));
                    } else if (decision.disposition ==
                               PreservationDisposition::RetainedUncertain) {
                        add_action_reason(
                            "included", action,
                            "uncertain_preservation_retained_state_" +
                                std::to_string(state));
                    }
                }
                if (result.diagnostics.preservation_witnesses.size() <
                    options.max_diagnostic_samples) {
                    result.diagnostics.preservation_witnesses.push_back(
                        preservation_witness_json(row_index, decision));
                } else {
                    ++result.diagnostics.preservation_witnesses_omitted;
                }
            }
        }
    }

    void record_cap(const std::string& name, bool state_cap = false) {
        if (std::find(result.diagnostics.cap_hits.begin(),
                      result.diagnostics.cap_hits.end(), name) ==
            result.diagnostics.cap_hits.end()) {
            result.diagnostics.cap_hits.push_back(name);
        }
        result.diagnostics.resource_cap_hit = true;
        if (state_cap) result.diagnostics.state_cap_hit = true;
    }

    bool check_solver_byte_cap_from(
        const std::uint64_t current,
        const std::uint64_t transient_bytes = 0) {
        const std::uint64_t projected =
            transient_bytes > std::numeric_limits<std::uint64_t>::max() - current
                ? std::numeric_limits<std::uint64_t>::max()
                : current + transient_bytes;
        peak_owned_bytes = std::max(peak_owned_bytes, projected);
        if (projected > options.max_solver_owned_bytes) {
            record_cap("max_solver_owned_bytes");
            return true;
        }
        return false;
    }

    bool check_solver_byte_cap(const std::uint64_t transient_bytes = 0) {
        return check_solver_byte_cap_from(
            audited_estimated_owned_bytes(), transient_bytes);
    }

    bool check_solver_byte_cap_fast(
        const std::uint64_t transient_bytes = 0) {
        return check_solver_byte_cap_from(
            fast_estimated_owned_bytes(), transient_bytes);
    }

    bool append_sparse_row(
        const std::uint32_t state,
        PendingSparseRow pending) {
        static const std::vector<OutcomeEntry> empty_transitions;
        static const std::vector<OutcomeChoiceGroup> empty_choices;
        static const std::vector<OutcomeChoiceOption> empty_choice_options;
        const auto& transitions = pending.transitions == nullptr
                                      ? empty_transitions
                                      : *pending.transitions;
        const auto& choices = pending.choices == nullptr
                                  ? empty_choices
                                  : *pending.choices;
        const auto& choice_options = pending.choice_options == nullptr
                                         ? empty_choice_options
                                         : *pending.choice_options;
        const auto identity_found =
            pending.shared_kernel_identity == nullptr
                ? shared_kernel_rows.end()
                : shared_kernel_rows.find(pending.shared_kernel_identity);
        pending.state = state;
        if (state >= transition_cache->state_rows.size()) {
            transition_cache->state_rows.resize(state + 1);
        }
        StateRowSpan& span = transition_cache->state_rows[state];
        const auto is_self = [&](const std::uint32_t successor) {
            return pending.entry_relative_self ? successor == kNoId
                                               : successor == state;
        };
        std::uint64_t transition_count = 0;
        double self_probability = 0.0;
        if (pending.entry_relative_self) {
            for (const OutcomeEntry& entry : transitions) {
                if (entry.state == kNoId) {
                    self_probability += entry.probability;
                } else {
                    ++transition_count;
                }
            }
        } else if (pending.shared_kernel_identity != nullptr) {
            transition_count = transitions.size();
            const auto self = std::lower_bound(
                transitions.begin(), transitions.end(), state,
                [](const OutcomeEntry& entry, const std::uint32_t value) {
                    return entry.state < value;
                });
            if (self != transitions.end() && self->state == state) {
                self_probability = self->probability;
            }
        } else {
            transition_count = transitions.size();
            for (const OutcomeEntry& entry : transitions) {
                if (entry.state == state) {
                    self_probability += entry.probability;
                }
            }
        }
        for (const OutcomeChoiceGroup& group : choices) {
            for (const std::uint32_t successor : group.states) {
                if (!is_self(successor)) ++transition_count;
            }
        }
        SparseRow* equivalent = nullptr;
        if (identity_found != shared_kernel_rows.end()) {
            const SparseRow& shared = transition_cache->rows.at(
                identity_found->second.row_index);
            for (std::uint32_t i = 0; i < span.count; ++i) {
                SparseRow& stored = transition_cache->rows.at(span.offset + i);
                if (stored.transition_offset != shared.transition_offset ||
                    stored.transition_count != shared.transition_count ||
                    stored.choice_offset != shared.choice_offset ||
                    stored.choice_count != shared.choice_count ||
                    stored.self_probability != self_probability) {
                    continue;
                }
                equivalent = &stored;
                break;
            }
        } else {
            for (std::uint32_t i = 0; i < span.count; ++i) {
                SparseRow& stored = transition_cache->rows.at(span.offset + i);
                if (!same_kernel(stored, pending)) continue;
                equivalent = &stored;
                break;
            }
        }
        if (equivalent == nullptr &&
            transition_cache->rows.size() >= options.max_state_action_rows) {
            throw SolverResourceLimit(
                "max_state_action_rows", options.max_state_action_rows);
        }

        SparseRow* stored_row = equivalent;
        if (stored_row == nullptr) {
            SparseRow row;
            row.owner_state = state;
            row.variant_offset = transition_cache->row_variant_indices.size();
            row.self_probability = self_probability;
            if (identity_found != shared_kernel_rows.end() &&
                identity_found->second.successor_envelope.has_value()) {
                row.preservation_effect = carrier_effect(
                    calc, state,
                    *identity_found->second.successor_envelope);
            } else {
                std::vector<std::uint32_t> effect_successors;
                effect_successors.reserve(transition_count);
                for (const OutcomeEntry& entry : transitions) {
                    if (entry.probability != 0.0) {
                        effect_successors.push_back(
                            is_self(entry.state) ? state : entry.state);
                    }
                }
                for (const OutcomeChoiceGroup& group : choices) {
                    if (group.probability == 0.0) continue;
                    for (const std::uint32_t successor : group.states) {
                        effect_successors.push_back(
                            is_self(successor) ? state : successor);
                    }
                }
                row.preservation_effect = carrier_effect(
                    calc, state, effect_successors);
                if (pending.shared_kernel_identity != nullptr) {
                    const CarrierSuccessorEnvelope envelope =
                        carrier_successor_envelope(
                            calc, std::move(effect_successors));
                    if (identity_found != shared_kernel_rows.end()) {
                        identity_found->second.successor_envelope = envelope;
                    }
                }
            }
            const std::size_t hash =
                identity_found == shared_kernel_rows.end()
                    ? kernel_hash(pending)
                    : 0;
            const SparseRow* shared_kernel = nullptr;
            if (identity_found != shared_kernel_rows.end()) {
                shared_kernel = &transition_cache->rows.at(
                    identity_found->second.row_index);
            } else {
                const auto found = kernel_rows_by_hash.find(hash);
                if (found != kernel_rows_by_hash.end()) {
                    for (const std::uint64_t row_index : found->second) {
                        const SparseRow& candidate =
                            transition_cache->rows.at(row_index);
                        if (same_kernel(candidate, pending)) {
                            shared_kernel = &candidate;
                            break;
                        }
                    }
                }
            }
            if (shared_kernel != nullptr) {
                row.transition_offset = shared_kernel->transition_offset;
                row.transition_count = shared_kernel->transition_count;
                row.choice_offset = shared_kernel->choice_offset;
                row.choice_count = shared_kernel->choice_count;
            } else {
                if (transition_cache->successors.size() +
                        transition_cache->choice_successors.size() +
                        transition_count > options.max_transitions) {
                    throw SolverResourceLimit(
                        "max_transitions", options.max_transitions);
                }
                row.transition_offset = transition_cache->successors.size();
                for (const OutcomeEntry& entry : transitions) {
                    if (pending.entry_relative_self &&
                        entry.state == kNoId) {
                        continue;
                    }
                    transition_cache->successors.push_back(entry.state);
                    transition_cache->probabilities.push_back(
                        entry.probability);
                }
                row.transition_count = static_cast<std::uint32_t>(
                    transition_cache->successors.size() -
                    row.transition_offset);
                row.choice_offset = transition_cache->choices.size();
                row.choice_count = static_cast<std::uint32_t>(choices.size());
                for (const OutcomeChoiceGroup& group : choices) {
                    SparseChoiceGroup stored;
                    stored.successor_offset =
                        transition_cache->choice_successors.size();
                    stored.probability = group.probability;
                    for (const std::uint32_t successor : group.states) {
                        if (is_self(successor)) {
                            stored.has_self = true;
                        } else {
                            transition_cache->choice_successors.push_back(
                                successor);
                        }
                    }
                    stored.successor_count = static_cast<std::uint32_t>(
                        transition_cache->choice_successors.size() -
                        stored.successor_offset);
                    transition_cache->choices.push_back(stored);
                }
            }
            if (row.self_probability > 0.0) {
                ++transition_cache->algebraic_self_loops;
            }
            for (std::uint32_t i = 0; i < row.choice_count; ++i) {
                if (transition_cache->choices[row.choice_offset + i]
                        .has_self) {
                    ++transition_cache->algebraic_self_loops;
                }
            }
            if (span.count == 0) span.offset = transition_cache->rows.size();
            transition_cache->rows.push_back(row);
            stored_row = &transition_cache->rows.back();
            if (shared_kernel == nullptr) {
                kernel_rows_by_hash[hash].push_back(
                    transition_cache->rows.size() - 1);
                if (pending.shared_kernel_identity != nullptr) {
                    shared_kernel_rows.emplace(
                        pending.shared_kernel_identity,
                        SharedKernelMemo{
                            transition_cache->rows.size() - 1, false,
                            carrier_successor_envelope(
                                calc,
                                [&]() {
                                    std::vector<std::uint32_t> ids;
                                    ids.reserve(transitions.size());
                                    for (const OutcomeEntry& entry :
                                         transitions) {
                                        if (entry.probability != 0.0) {
                                            ids.push_back(entry.state);
                                        }
                                    }
                                    return ids;
                                }())});
                }
            }
            ++span.count;
        }
        if (pending.shared_kernel_identity != nullptr &&
            shared_kernel_rows.find(pending.shared_kernel_identity) ==
                shared_kernel_rows.end()) {
            shared_kernel_rows.emplace(
                pending.shared_kernel_identity,
                SharedKernelMemo{
                    static_cast<std::uint64_t>(
                        stored_row - transition_cache->rows.data()),
                    false,
                    carrier_successor_envelope(
                        calc,
                        [&]() {
                            std::vector<std::uint32_t> ids;
                            ids.reserve(transitions.size());
                            for (const OutcomeEntry& entry : transitions) {
                                if (entry.probability != 0.0) {
                                    ids.push_back(entry.state);
                                }
                            }
                            return ids;
                        }())});
        }

        SparseVariant variant;
        variant.operator_index = pending.operator_index;
        variant.quantity_offset = transition_cache->variant_quantities.size();
        const PricedOperator& priced = operators.at(
            static_cast<std::size_t>(
                priced_operator_position.at(pending.operator_index)));
        for (const auto& [key, unused_price] : priced.resource_prices) {
            (void)unused_price;
            double quantity = 0.0;
            if (pending.resources != nullptr) {
                const auto found = std::find_if(
                    pending.resources->begin(), pending.resources->end(),
                    [&](const auto& resource) {
                        return resource.first == key;
                    });
                if (found != pending.resources->end()) quantity = found->second;
            }
            transition_cache->variant_quantities.push_back(quantity);
        }
        variant.quantity_count = static_cast<std::uint32_t>(
            transition_cache->variant_quantities.size() -
            variant.quantity_offset);
        variant.choice_option_offset = transition_cache->choice_options.size();
        variant.choice_option_count = static_cast<std::uint32_t>(
            choice_options.size());
        for (OutcomeChoiceOption choice : choice_options) {
            if (pending.entry_relative_self && choice.state == kNoId) {
                choice.state = state;
            }
            transition_cache->choice_options.push_back(choice);
        }
        transition_cache->variants.push_back(variant);
        const std::uint32_t variant_index = static_cast<std::uint32_t>(
            transition_cache->variants.size() - 1);
        if (stored_row->variant_count != 0 &&
            stored_row->variant_offset + stored_row->variant_count !=
                transition_cache->row_variant_indices.size()) {
            const std::uint64_t relocated =
                transition_cache->row_variant_indices.size();
            for (std::uint32_t i = 0; i < stored_row->variant_count; ++i) {
                transition_cache->row_variant_indices.push_back(
                    transition_cache->row_variant_indices.at(
                        stored_row->variant_offset + i));
            }
            stored_row->variant_offset = relocated;
        }
        transition_cache->row_variant_indices.push_back(variant_index);
        ++stored_row->variant_count;

        result.diagnostics.sparse_rows = transition_cache->rows.size();
        result.diagnostics.sparse_transitions =
            transition_cache->successors.size() +
            transition_cache->choice_successors.size();
        const std::size_t stored_row_index = static_cast<std::size_t>(
            stored_row - transition_cache->rows.data());
        if (priced_rows.size() < transition_cache->rows.size()) {
            priced_rows.resize(transition_cache->rows.size());
        }
        update_priced_row(stored_row_index);

        bool enqueue_fringe = !focused_mode;
        if (enqueue_fringe && pending.shared_kernel_identity != nullptr) {
            SharedKernelMemo& memo =
                shared_kernel_rows.at(pending.shared_kernel_identity);
            enqueue_fringe = !memo.fringe_enqueued;
            memo.fringe_enqueued = true;
        }
        if (enqueue_fringe) {
            for (const OutcomeEntry& entry : transitions) {
                if (!is_self(entry.state)) enqueue(entry.state);
            }
            for (const OutcomeChoiceGroup& group : choices) {
                for (const std::uint32_t successor : group.states) {
                    if (!is_self(successor)) enqueue(successor);
                }
            }
        }
        return equivalent != nullptr;
    }

    bool expand_one_unit() {
        const auto started = std::chrono::steady_clock::now();
        if (!expansion_active) {
            expansion_state = queue.front();
            queue.pop_front();
            if (expansion_state >= expanded.size()) {
                expanded.resize(expansion_state + 1, 0);
            }
            expanded[expansion_state] = 1;
            ++expanded_count;
            expansion_operator_cursor = 0;
            expansion_active = true;
            expansion_prepared = false;
            expansion_operator_indices.clear();
        }
        const std::uint32_t state = expansion_state;
        if (calc.is_goal_state(calc.state(state))) {
            expansion_active = false;
            result.diagnostics.expansion_ns += static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() - started)
                    .count());
            return true;
        }

        try {
            if (!expansion_prepared) {
                prepare_state_expansion(state);
                expansion_prepared = true;
            }
            if (expansion_operator_cursor <
                expansion_operator_indices.size()) {
                const std::uint32_t operator_index =
                    expansion_operator_indices[expansion_operator_cursor++];
                const std::int32_t priced_position =
                    priced_operator_position.at(operator_index);
                if (priced_position < 0) {
                    throw std::logic_error(
                        "state-local automatic operator is not priced");
                }
                const PricedOperator& priced = operators.at(
                    static_cast<std::size_t>(priced_position));
                const PlannerOperator& planner =
                    calc.operators().at(priced.index);
                const auto row_started = std::chrono::steady_clock::now();
                const auto kernel_started = row_started;
                PendingSparseRow pending;
                pending.state = state;
                pending.operator_index = priced.index;
                pending.resources = &planner.resource_quantities;
                std::optional<SolveTransitionCache::AutomaticCandidateRecord>
                    automatic_record;
                const AutomaticTelemetryKind telemetry_kind =
                    automatic_telemetry_kind(planner);
                if (telemetry_kind != AutomaticTelemetryKind::None) {
                    automatic_record.emplace();
                    automatic_record->state_id = state;
                    automatic_record->operator_index = priced.index;
                    automatic_record->candidate_kind =
                        planner.automatic_kind;
                    automatic_record->telemetry_kind = telemetry_kind;
                    const std::uint64_t admission_key =
                        (static_cast<std::uint64_t>(state) << 32) |
                        priced.index;
                    automatic_record->count_candidate =
                        !automatic_admission_records.contains(admission_key);
                    automatic_record->evidence.candidate = true;
                    automatic_record->evidence.relevant_goal_mask =
                        planner.relevant_goal_mask;
                }
                bool append = true;
                if (planner.kind == PlannerOperatorKind::FixedOption) {
                    const OptionKernel& kernel =
                        calc.option_kernel(state, priced.index);
                    if (automatic_record.has_value()) {
                        automatic_record->evidence = kernel.automatic;
                        automatic_record->eligible =
                            kernel.supported && kernel.legal;
                        if (automatic_record->count_candidate) {
                            automatic_record->template_id =
                                kernel.retained_template_id;
                            automatic_record->template_hit =
                                calc.option_kernel_template_hit(
                                    state, priced.index);
                            automatic_record->raw_outcomes =
                                kernel.exits.size();
                            for (const OutcomeChoiceGroup& group :
                                 kernel.observation_choice_groups) {
                                automatic_record->raw_outcomes +=
                                    group.states.size();
                            }
                            if (!automatic_record->template_hit &&
                                kernel.retained_template_id != 0) {
                                automatic_record->selected_bytes +=
                                    sizeof(OptionKernel) +
                                    kernel.exits.capacity() *
                                        sizeof(OutcomeEntry) +
                                    kernel.retry_states.capacity() *
                                        sizeof(std::uint32_t) +
                                    kernel.continuation_states.capacity() *
                                        sizeof(std::uint32_t);
                            }
                        }
                    }
                    if (!kernel.supported) {
                        if (!reported_unsupported[priced.index]) {
                            reported_unsupported[priced.index] = true;
                            record_skipped_unsupported(planner.id);
                            add_action_reason(
                                "unsupported", planner.id,
                                "fixed_option_kernel_unavailable");
                        }
                        append = false;
                    } else if (!kernel.legal) {
                        append = false;
                    } else {
                        pending.transitions = &kernel.exits;
                        pending.choices =
                            &kernel.observation_choice_groups;
                        pending.choice_options =
                            &kernel.observation_choice_options;
                        pending.resources =
                            calc.is_state_local_automatic_operator(
                                priced.index)
                                ? &planner.resource_quantities
                                : &kernel.expected_resources;
                        pending.entry_relative_self = std::any_of(
                            kernel.exits.begin(), kernel.exits.end(),
                            [](const OutcomeEntry& entry) {
                                return entry.state == kNoId;
                            });
                        if (!pending.entry_relative_self) {
                            pending.entry_relative_self = std::any_of(
                                kernel.observation_choice_groups.begin(),
                                kernel.observation_choice_groups.end(),
                                [](const OutcomeChoiceGroup& group) {
                                    return std::find(
                                        group.states.begin(),
                                        group.states.end(), kNoId) !=
                                        group.states.end();
                                });
                        }
                    }
                } else {
                    const std::uint32_t action_index =
                        planner.primitive_action;
                    const AbstractState& entry = calc.state(state);
                    std::uint32_t fracture_relevant_mask = 0;
                    if (planner.automatic_kind ==
                        AutomaticCandidateKind::Fracture) {
                        for (std::uint32_t slot = 0;
                             slot < calc.layout().slots.size(); ++slot) {
                            if ((planner.relevant_goal_mask & (1u << slot)) !=
                                    0 &&
                                entry.slot_status[slot] ==
                                    static_cast<std::uint8_t>(
                                        GoalSlotStatus::Satisfied) &&
                                (entry.fractured_goal_mask & (1u << slot)) ==
                                    0) {
                                fracture_relevant_mask |= 1u << slot;
                            }
                        }
                    }
                    if (!action_legal(
                            session, calc.registry().actions[action_index],
                            entry)) {
                        append = false;
                    } else if (planner.automatic_kind ==
                                   AutomaticCandidateKind::Fracture &&
                               fracture_relevant_mask == 0) {
                        append = false;
                        OptionKernel::AutomaticEvidence& evidence =
                            automatic_record->evidence;
                        evidence.eligible = false;
                        evidence.legality_result = "irrelevant";
                        evidence.reason =
                            "no_unfractured_satisfied_goal_carrier";
                        automatic_record->eligible = false;
                    } else {
                        const OutcomeDistribution& distribution =
                            calc.outcomes(state, action_index);
                        if (!distribution.supported) {
                            if (!reported_unsupported[priced.index]) {
                                reported_unsupported[priced.index] = true;
                                record_skipped_unsupported(planner.id);
                                add_action_reason(
                                    "unsupported", planner.id,
                                    "exact_evaluator_unavailable");
                            }
                            append = false;
                        } else if (distribution.choice_groups.empty()) {
                            pending.transitions = &distribution.entries;
                            if (calc.registry().actions[action_index].synthetic &&
                                distribution.entries.size() == 1) {
                                restart_state = distribution.entries[0].state;
                            }
                            if (distribution.stable_shared_kernel) {
                                pending.shared_kernel_identity = &distribution;
                            }
                        } else {
                            pending.choices = &distribution.choice_groups;
                            pending.choice_options =
                                &distribution.choice_options;
                        }
                        if (append && automatic_record.has_value()) {
                            bool relevant_change = false;
                            if (planner.automatic_kind ==
                                AutomaticCandidateKind::Fracture) {
                                for (const OutcomeEntry& exit :
                                     distribution.entries) {
                                    if (exit.probability <= 0.0) continue;
                                    relevant_change |=
                                        (calc.state(exit.state)
                                             .fractured_goal_mask &
                                         fracture_relevant_mask) != 0;
                                }
                            } else {
                                /* Outcome interning may move the state table,
                                 * so reacquire the carrier afterwards. */
                                const AbstractState& source = calc.state(state);
                                for (const OutcomeEntry& exit :
                                     distribution.entries) {
                                    if (exit.probability <= 0.0) continue;
                                    const AbstractState& successor =
                                        calc.state(exit.state);
                                    for (std::uint32_t slot = 0;
                                         slot < calc.layout().slots.size();
                                         ++slot) {
                                        if ((planner.relevant_goal_mask &
                                             (1u << slot)) != 0 &&
                                            successor.slot_status[slot] >
                                                source.slot_status[slot]) {
                                            relevant_change = true;
                                        }
                                    }
                                }
                            }
                            OptionKernel::AutomaticEvidence& evidence =
                                automatic_record->evidence;
                            evidence.eligible = relevant_change;
                            evidence.kernel_changed = relevant_change;
                            evidence.setup_complete = relevant_change;
                            evidence.cleanup_complete = true;
                            evidence.recovery_complete = true;
                            evidence.exits_complete =
                                !distribution.entries.empty();
                            evidence.kernel_change_mechanisms =
                                planner.automatic_kind ==
                                        AutomaticCandidateKind::Fracture
                                    ? kAutomaticCarrierFracture
                                    : kAutomaticDeterministicFinish;
                            evidence.legality_result =
                                relevant_change ? "legal" : "irrelevant";
                            if (planner.automatic_kind ==
                                AutomaticCandidateKind::Fracture) {
                                evidence.relevant_goal_mask =
                                    fracture_relevant_mask;
                                evidence.reason = relevant_change
                                    ? "exact_goal_relevant_primitive_fracture_distribution"
                                    : "no_unfractured_satisfied_goal_carrier";
                            } else {
                                evidence.reason = relevant_change
                                    ? "legal_permanent_goal_bench_successor"
                                    : "permanent_bench_does_not_advance_goal";
                            }
                            automatic_record->eligible = relevant_change;
                            if (!relevant_change) append = false;
                        }
                    }
                }
                result.diagnostics.expansion_kernel_ns +=
                    static_cast<std::uint64_t>(
                        std::chrono::duration_cast<std::chrono::nanoseconds>(
                            std::chrono::steady_clock::now() - kernel_started)
                            .count());
                try {
                    if (append) {
                        if (automatic_record.has_value() &&
                            automatic_record->evidence.candidate_kernel_hash ==
                                0) {
                            automatic_record->evidence.candidate_kernel_hash =
                                static_cast<std::uint64_t>(
                                    kernel_hash(pending));
                        }
                        const std::uint64_t rows_before =
                            transition_cache->rows.size();
                        const std::uint64_t transitions_before =
                            transition_cache->successors.size() +
                            transition_cache->choice_successors.size();
                        const auto row_byte_audit_started =
                            std::chrono::steady_clock::now();
                        const std::uint64_t bytes_before =
                            transition_cache->estimated_owned_bytes();
                        result.diagnostics.expansion_row_byte_audit_ns +=
                            static_cast<std::uint64_t>(
                                std::chrono::duration_cast<
                                    std::chrono::nanoseconds>(
                                    std::chrono::steady_clock::now() -
                                    row_byte_audit_started)
                                    .count());
                        const auto sparse_row_started =
                            std::chrono::steady_clock::now();
                        const bool collapsed =
                            append_sparse_row(state, std::move(pending));
                        result.diagnostics.expansion_sparse_row_ns +=
                            static_cast<std::uint64_t>(
                                std::chrono::duration_cast<
                                    std::chrono::nanoseconds>(
                                    std::chrono::steady_clock::now() -
                                    sparse_row_started)
                                    .count());
                        if (automatic_record.has_value()) {
                            automatic_record->collapsed = collapsed;
                            automatic_record->eligible = true;
                            automatic_record->retained_rows =
                                transition_cache->rows.size() - rows_before;
                            automatic_record->retained_transitions =
                                transition_cache->successors.size() +
                                transition_cache->choice_successors.size() -
                                transitions_before;
                            const auto selected_byte_audit_started =
                                std::chrono::steady_clock::now();
                            const std::uint64_t bytes_after =
                                transition_cache->estimated_owned_bytes();
                            result.diagnostics.expansion_row_byte_audit_ns +=
                                static_cast<std::uint64_t>(
                                    std::chrono::duration_cast<
                                        std::chrono::nanoseconds>(
                                        std::chrono::steady_clock::now() -
                                        selected_byte_audit_started)
                                        .count());
                            automatic_record->selected_bytes +=
                                bytes_after - bytes_before;
                        }
                    }
                } catch (...) {
                    if (planner.kind == PlannerOperatorKind::FixedOption) {
                        calc.release_option_kernel(state, priced.index);
                    } else {
                        calc.release_outcome(
                            state, planner.primitive_action);
                    }
                    throw;
                }
                const std::uint64_t row_ns = static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now() - row_started)
                        .count());
                if (planner.kind == PlannerOperatorKind::Primitive) {
                    calc.record_primitive_row_time(
                        planner.primitive_action, row_ns);
                }
                if (automatic_record.has_value()) {
                    automatic_record->row_ns = row_ns;
                    if (automatic_record->evidence.reason.empty()) {
                        automatic_record->evidence.eligible = false;
                        automatic_record->evidence.legality_result = "illegal";
                        automatic_record->evidence.reason =
                            "native_carrier_legality_refused";
                    }
                    const auto diagnostics_started =
                        std::chrono::steady_clock::now();
                    retain_automatic_candidate_record(
                        std::move(*automatic_record));
                    result.diagnostics.expansion_diagnostics_ns +=
                        static_cast<std::uint64_t>(
                            std::chrono::duration_cast<
                                std::chrono::nanoseconds>(
                                std::chrono::steady_clock::now() -
                                diagnostics_started)
                                .count());
                }
                const auto release_started =
                    std::chrono::steady_clock::now();
                if (planner.kind == PlannerOperatorKind::FixedOption) {
                    calc.release_option_kernel(state, priced.index);
                } else {
                    calc.release_outcome(state, planner.primitive_action);
                }
                result.diagnostics.expansion_release_ns +=
                    static_cast<std::uint64_t>(
                        std::chrono::duration_cast<std::chrono::nanoseconds>(
                            std::chrono::steady_clock::now() - release_started)
                            .count());
            }
        } catch (const SolverResourceLimit& limit) {
            if (expansion_operator_cursor != 0 &&
                expansion_operator_cursor <=
                    expansion_operator_indices.size()) {
                const std::uint32_t operator_index =
                    expansion_operator_indices[
                        expansion_operator_cursor - 1];
                const std::int32_t priced_position =
                    priced_operator_position.at(operator_index);
                const PricedOperator& priced = operators.at(
                    static_cast<std::size_t>(priced_position));
                const PlannerOperator& planner =
                    calc.operators().at(priced.index);
                const AutomaticTelemetryKind telemetry_kind =
                    automatic_telemetry_kind(planner);
                if (telemetry_kind != AutomaticTelemetryKind::None) {
                    SolveTransitionCache::AutomaticCandidateRecord record;
                    record.state_id = state;
                    record.operator_index = priced.index;
                    record.deferred = true;
                    record.telemetry_kind = telemetry_kind;
                    const std::uint64_t admission_key =
                        (static_cast<std::uint64_t>(state) << 32) |
                        priced.index;
                    record.count_candidate =
                        !automatic_admission_records.contains(admission_key);
                    record.evidence.candidate = true;
                    record.evidence.relevant_goal_mask =
                        planner.relevant_goal_mask;
                    record.evidence.legality_result =
                        "deferred_resource_cap";
                    record.evidence.reason =
                        "price_independent_kernel_generation_resource_cap";
                    retain_automatic_candidate_record(std::move(record));
                }
            }
            record_cap(
                limit.cap_name(),
                limit.cap_name() == "max_discovered_states");
        }
        const bool completed = result.diagnostics.resource_cap_hit ||
                               expansion_operator_cursor >=
                                   expansion_operator_indices.size();
        if (completed) expansion_active = false;
        if (completed && !result.diagnostics.resource_cap_hit &&
            expanded_count % 64 == 0) {
            const auto byte_audit_started =
                std::chrono::steady_clock::now();
            /* Preserve the 64-carrier cap cadence with the exact incremental
             * ledger. The full selected-allocation walk is now a
             * reconciliation oracle rather than the hot-path cap check. */
            if (expanded_count % 1024 == 0) {
                check_solver_byte_cap();
            } else {
                check_solver_byte_cap_fast();
            }
            result.diagnostics.expansion_cap_byte_audit_ns +=
                static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now() -
                        byte_audit_started)
                        .count());
        }
        result.diagnostics.expansion_ns += static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - started)
                .count());
        return completed;
    }

    bool priced_variant_cost(
        const SparseVariant& variant,
        double& cost) const {
        const std::int32_t priced_position =
            priced_operator_position.at(variant.operator_index);
        if (priced_position < 0) return false;
        const PricedOperator& priced =
            operators.at(static_cast<std::size_t>(priced_position));
        if (variant.quantity_count != priced.resource_prices.size()) {
            throw std::logic_error(
                "cached solver resource vector is incompatible");
        }
        cost = 0.0;
        for (std::uint32_t quantity = 0;
             quantity < variant.quantity_count; ++quantity) {
            cost += transition_cache->variant_quantities.at(
                        variant.quantity_offset + quantity) *
                    priced.resource_prices[quantity].second;
        }
        return true;
    }

    void update_priced_row(const std::size_t row_index) {
        const SparseRow& row = transition_cache->rows.at(row_index);
        PricedSparseRow selected;
        for (std::uint32_t i = 0; i < row.variant_count; ++i) {
            const SparseVariant& variant = transition_cache->variants.at(
                transition_cache->row_variant_indices.at(
                    row.variant_offset + i));
            double cost = 0.0;
            if (!priced_variant_cost(variant, cost)) continue;
            if (cost < selected.cost - 1e-12 ||
                (std::abs(cost - selected.cost) <= 1e-12 &&
                 variant.operator_index < selected.operator_index)) {
                selected.operator_index = variant.operator_index;
                selected.cost = cost;
                selected.choice_option_offset =
                    variant.choice_option_offset;
                selected.choice_option_count =
                    variant.choice_option_count;
            }
        }
        priced_rows.at(row_index) = selected;
    }

    void prepare_priced_rows() {
        const std::size_t first_unpriced = priced_rows.size();
        priced_rows.resize(transition_cache->rows.size());
        for (std::size_t row_index = first_unpriced;
             row_index < transition_cache->rows.size(); ++row_index) {
            update_priced_row(row_index);
        }
        if (restart_state == kNoId && restart_operator_index != kNoId) {
            for (const SparseRow& row : transition_cache->rows) {
                bool has_restart = false;
                for (std::uint32_t i = 0; i < row.variant_count; ++i) {
                    const SparseVariant& variant =
                        transition_cache->variants.at(
                            transition_cache->row_variant_indices.at(
                                row.variant_offset + i));
                    has_restart |=
                        variant.operator_index == restart_operator_index;
                }
                if (!has_restart || row.transition_count != 1) continue;
                restart_state = transition_cache->successors.at(
                    row.transition_offset);
                break;
            }
        }
        for (std::size_t row_index = pricing_diagnostics_cursor;
             row_index < transition_cache->rows.size(); ++row_index) {
            const SparseRow& row = transition_cache->rows[row_index];
            const PricedSparseRow& selected = priced_rows[row_index];
            std::uint32_t priced_variant_count = 0;
            for (std::uint32_t i = 0; i < row.variant_count; ++i) {
                const SparseVariant& variant = transition_cache->variants.at(
                    transition_cache->row_variant_indices.at(
                        row.variant_offset + i));
                double unused_cost = 0.0;
                if (priced_variant_cost(variant, unused_cost)) {
                    ++priced_variant_count;
                }
            }
            if (priced_variant_count <= 1) continue;
            result.diagnostics.equivalent_actions_collapsed +=
                priced_variant_count - 1;
            const std::string& representative = calc.operators().at(
                selected.operator_index).id;
            for (std::uint32_t i = 0; i < row.variant_count; ++i) {
                const SparseVariant& variant = transition_cache->variants.at(
                    transition_cache->row_variant_indices.at(
                        row.variant_offset + i));
                double cost = 0.0;
                if (!priced_variant_cost(variant, cost)) continue;
                const std::uint32_t operator_index = variant.operator_index;
                if (operator_index == selected.operator_index) continue;
                const std::string& candidate =
                    calc.operators().at(operator_index).id;
                if (std::abs(cost - selected.cost) <= 1e-12) {
                    ++result.diagnostics.equivalent_price_ties;
                    add_action_reason(
                        "included", candidate,
                        "certified_equivalent_kernel_price_tie_with_" +
                            representative);
                } else {
                    add_action_reason(
                        "pruned", candidate,
                        "certified_equivalent_kernel_more_expensive_than_" +
                            representative);
                }
            }
        }
        pricing_diagnostics_cursor = transition_cache->rows.size();
    }

    void prepare_iteration() {
        const auto started = std::chrono::steady_clock::now();
        if (!cache_pending && !queue.empty() &&
            expanded_count >= options.max_expanded_states) {
            result.diagnostics.state_cap_hit = true;
            record_cap("max_expanded_states", true);
        }
        result.diagnostics.expanded_states = expanded_count;

        if (cache_pending) {
            expanded = transition_cache->expanded;
        } else {
            transition_cache->discovered_states = calc.state_count();
            transition_cache->expanded_states = expanded_count;
            transition_cache->expanded = expanded;
            transition_cache->expanded.resize(
                transition_cache->discovered_states, 0);
            transition_cache->state_rows.resize(
                transition_cache->discovered_states);
        }
        const std::uint32_t state_count = transition_cache->discovered_states;
        result.diagnostics.discovered_states = state_count;
        result.diagnostics.frontier_states = state_count - expanded_count;
        expanded.resize(state_count, 0);
        result.expanded = expanded;
        result.values.assign(state_count, kValueCeiling);
        result.policy.assign(state_count, PolicyOperatorRef{});
        result.unveil_preferences.assign(state_count, {});
        result.option_unveil_preferences.assign(state_count, {});
        result.goal_states.assign(state_count, 0);
        for (std::uint32_t state = 0; state < state_count; ++state) {
            if (calc.is_goal_state(calc.state(state))) {
                result.goal_states[state] = 1;
                ++result.diagnostics.goal_states;
                result.values[state] = 0.0;
            } else if (!result.expanded[state]) {
                /* Past-the-cap frontier: no Bellman backing, keep infinite so
                 * plans through it are never preferred. */
                result.values[state] = kInfinity;
            }
        }
        residual = kValueCeiling;
        phase = SolvePhase::Iterating;
        result.diagnostics.sparse_rows = transition_cache->rows.size();
        result.diagnostics.sparse_transitions =
            transition_cache->successors.size() +
            transition_cache->choice_successors.size();
        result.diagnostics.algebraic_self_loops =
            transition_cache->algebraic_self_loops;
        result.diagnostics.reforge_frontier_work =
            calc.telemetry().reforge_frontier_work;
        prepare_priced_rows();
        const auto final_cap_audit_started =
            std::chrono::steady_clock::now();
        check_solver_byte_cap();
        result.diagnostics.expansion_finalize_byte_audit_ns +=
            static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() -
                    final_cap_audit_started)
                    .count());
        /* The solve now owns the compact CSR rows used by every later phase;
         * release evaluator distributions so transitions are stored once. A
         * reusable price-only context is the separate S7.5 cache-reuse pass. */
        calc.release_solve_transition_caches();
        if (!cache_pending && !result.diagnostics.resource_cap_hit &&
            queue.empty()) {
            transition_cache->focused_partial = focused_mode;
            calc.retain_solve_transition_cache(transition_cache);
        }
        kernel_rows_by_hash.clear();
        kernel_rows_by_hash.rehash(0);
        shared_kernel_rows.clear();
        shared_kernel_rows.rehash(0);
        cache_pending = false;
        const auto peak_byte_audit_started =
            std::chrono::steady_clock::now();
        peak_owned_bytes = std::max(
            peak_owned_bytes, estimated_owned_bytes());
        result.diagnostics.expansion_finalize_byte_audit_ns +=
            static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() -
                    peak_byte_audit_started)
                    .count());
        result.diagnostics.expansion_finalize_ns +=
            static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() - started)
                    .count());
        result.diagnostics.expansion_ns += static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - started)
                .count());
    }

    double operator_q(
        const std::uint32_t state,
        const PricedOperator& priced) {
        const PlannerOperator& planner = calc.operators().at(priced.index);
        if (planner.kind == PlannerOperatorKind::FixedOption) {
            const OptionKernel& kernel =
                calc.option_kernel(state, priced.index);
            if (!kernel.supported || !kernel.legal) return kInfinity;
            double expected = 0.0;
            const auto& resources =
                calc.is_state_local_automatic_operator(priced.index)
                    ? planner.resource_quantities
                    : kernel.expected_resources;
            for (const auto& [key, quantity] : resources) {
                const auto found = std::find_if(
                    priced.resource_prices.begin(),
                    priced.resource_prices.end(),
                    [&](const auto& price) { return price.first == key; });
                if (found == priced.resource_prices.end()) return kInfinity;
                expected += quantity * found->second;
            }
            for (const OutcomeEntry& exit : kernel.exits) {
                const std::uint32_t successor =
                    exit.state == kNoId ? state : exit.state;
                const double value = result.values[successor];
                if (value == kInfinity) return kInfinity;
                expected += exit.probability * value;
            }
            for (const OutcomeChoiceGroup& group :
                 kernel.observation_choice_groups) {
                double best = kInfinity;
                for (const std::uint32_t successor : group.states) {
                    best = std::min(
                        best,
                        result.values[successor == kNoId ? state
                                                         : successor]);
                }
                if (best == kInfinity) return kInfinity;
                expected += group.probability * best;
            }
            return expected;
        }
        const std::uint32_t action_index = planner.primitive_action;
        if (!action_legal(session, calc.registry().actions[action_index],
                          calc.state(state))) {
            return kInfinity;
        }
        const OutcomeDistribution& distribution =
            calc.outcomes(state, action_index);
        if (!distribution.supported) return kInfinity;
        double expected = priced.cost;
        if (!distribution.choice_groups.empty()) {
            for (const OutcomeChoiceGroup& group :
                 distribution.choice_groups) {
                double best = kInfinity;
                for (std::uint32_t successor : group.states) {
                    best = std::min(best, result.values[successor]);
                }
                if (best == kInfinity) return kInfinity;
                expected += group.probability * best;
            }
            return expected;
        }
        for (const OutcomeEntry& entry : distribution.entries) {
            const double value = result.values[entry.state];
            if (value == kInfinity) return kInfinity;
            expected += entry.probability * value;
        }
        return expected;
    }

    void reset_kernel_value_cache(bool active = false) {
        kernel_value_cache_active = active;
        kernel_value_caches.clear();
        kernel_value_cache_by_offset.clear();
    }

    KernelValueCache& value_cache_for(const SparseRow& row) {
        const auto found =
            kernel_value_cache_by_offset.find(row.transition_offset);
        if (found != kernel_value_cache_by_offset.end()) {
            return kernel_value_caches.at(found->second);
        }
        KernelValueCache cache;
        cache.transition_offset = row.transition_offset;
        cache.transition_count = row.transition_count;
        cache.probability_by_state.assign(result.values.size(), 0.0);
        for (std::uint32_t i = 0; i < row.transition_count; ++i) {
            const std::uint64_t offset = row.transition_offset + i;
            const std::uint32_t successor =
                transition_cache->successors.at(offset);
            const double probability =
                transition_cache->probabilities.at(offset);
            cache.probability_by_state.at(successor) += probability;
        }
        for (std::uint32_t state = 0;
             state < cache.probability_by_state.size(); ++state) {
            const double probability = cache.probability_by_state[state];
            if (probability == 0.0) continue;
            const double value = result.values.at(state);
            if (value == kInfinity) ++cache.infinite_count;
            else cache.finite_sum += probability * value;
        }
        const std::size_t index = kernel_value_caches.size();
        kernel_value_caches.push_back(std::move(cache));
        kernel_value_cache_by_offset.emplace(row.transition_offset, index);
        return kernel_value_caches.back();
    }

    void update_kernel_value_cache(
        const std::uint32_t state,
        const double before,
        const double after) {
        if (!kernel_value_cache_active || before == after) return;
        for (KernelValueCache& cache : kernel_value_caches) {
            const double probability = cache.probability_by_state.at(state);
            if (probability == 0.0) continue;
            if (before == kInfinity) {
                --cache.infinite_count;
            } else {
                cache.finite_sum -= probability * before;
            }
            if (after == kInfinity) {
                ++cache.infinite_count;
            } else {
                cache.finite_sum += probability * after;
            }
        }
    }

    double sparse_row_q(
        const std::size_t row_index,
        std::uint32_t& transition_work) {
        const SparseRow& row = transition_cache->rows.at(row_index);
        double constant = priced_rows.at(row_index).cost;
        transition_work = 0;
        if (kernel_value_cache_active && row.choice_count == 0 &&
            row.transition_count >= 1024) {
            KernelValueCache& cache = value_cache_for(row);
            const double self_value = result.values.at(row.owner_state);
            const bool infinite_self =
                row.self_probability > 0.0 && self_value == kInfinity;
            const std::uint32_t non_self_infinite =
                cache.infinite_count - (infinite_self ? 1u : 0u);
            transition_work += row.transition_count;
            if (non_self_infinite != 0) return kInfinity;
            constant += cache.finite_sum;
            if (!infinite_self) {
                constant -= row.self_probability * self_value;
            }
        } else {
            for (std::uint32_t i = 0; i < row.transition_count; ++i) {
                const std::uint64_t offset = row.transition_offset + i;
                if (transition_cache->successors.at(offset) ==
                    row.owner_state) {
                    continue;
                }
                const double value = result.values[
                    transition_cache->successors.at(offset)];
                ++transition_work;
                if (value == kInfinity) return kInfinity;
                constant += transition_cache->probabilities.at(offset) *
                            value;
            }
        }
        std::vector<std::pair<double, double>> self_choices;
        for (std::uint32_t i = 0; i < row.choice_count; ++i) {
            const SparseChoiceGroup& group = transition_cache->choices.at(
                row.choice_offset + i);
            double best = kInfinity;
            for (std::uint32_t s = 0; s < group.successor_count; ++s) {
                best = std::min(
                    best,
                    result.values[transition_cache->choice_successors.at(
                        group.successor_offset + s)]);
            }
            transition_work += group.successor_count;
            if (group.has_self) {
                self_choices.push_back({best, group.probability});
            } else {
                if (best == kInfinity) return kInfinity;
                constant += group.probability * best;
            }
        }
        /* Solve the row-local fixed point exactly:
         *
         * x = constant + p_self*x + sum g*min(x, alternate_g)
         *
         * Sorting the observed-choice thresholds identifies which offers
         * return to the outer policy and which take a concrete successor. */
        std::sort(
            self_choices.begin(), self_choices.end(),
            [](const auto& left, const auto& right) {
                return left.first < right.first;
            });
        double loop_probability = row.self_probability;
        for (const auto& [unused, probability] : self_choices) {
            (void)unused;
            loop_probability += probability;
        }
        std::size_t fixed_choices = 0;
        while (true) {
            const double denominator = 1.0 - loop_probability;
            const double value = denominator > 1e-15
                                     ? constant / denominator
                                     : kInfinity;
            if (fixed_choices >= self_choices.size() ||
                value <= self_choices[fixed_choices].first + 1e-12) {
                return value;
            }
            const auto [alternate, probability] =
                self_choices[fixed_choices++];
            if (!std::isfinite(alternate)) return kInfinity;
            constant += probability * alternate;
            loop_probability -= probability;
        }
    }

    void begin_policy_selection() {
        reset_kernel_value_cache(true);
        const std::uint64_t no_row =
            std::numeric_limits<std::uint64_t>::max();
        if (policy_rows.size() != result.values.size()) {
            policy_rows.assign(result.values.size(), no_row);
        }
        policy_selection_active = true;
        policy_selection_cursor = 0;
        policy_selection_improved = false;
        policy_selection_residual = 0.0;
    }

    bool advance_policy_selection(bool& improved) {
        if (!policy_selection_active) begin_policy_selection();
        const std::uint64_t no_row =
            std::numeric_limits<std::uint64_t>::max();
        constexpr std::uint32_t kStatesPerSelectionUnit = 128;
        const std::uint32_t end = std::min<std::uint32_t>(
            static_cast<std::uint32_t>(result.values.size()),
            policy_selection_cursor + kStatesPerSelectionUnit);
        for (std::uint32_t state = policy_selection_cursor;
             state < end; ++state) {
            if (!result.expanded[state] || result.goal_states[state]) continue;
            const StateRowSpan& span = transition_cache->state_rows.at(state);
            double best = kInfinity;
            std::uint64_t best_row = no_row;
            for (std::uint32_t row = 0; row < span.count; ++row) {
                std::uint32_t work = 0;
                const std::uint64_t absolute = span.offset + row;
                if (preservation_prunes(absolute)) continue;
                const double q = sparse_row_q(absolute, work);
                ++result.diagnostics.bellman_action_evaluations;
                if (q < best - options.epsilon) {
                    best = q;
                    best_row = absolute;
                }
            }
            ++result.diagnostics.bellman_backups;
            if (std::isfinite(best)) {
                policy_selection_residual = std::max(
                    policy_selection_residual,
                    std::abs(result.values[state] - best));
            }
            if (best_row != no_row && policy_rows[state] != best_row) {
                policy_rows[state] = best_row;
                policy_selection_improved = true;
            }
        }
        policy_selection_cursor = end;
        if (policy_selection_cursor < result.values.size()) return false;
        residual = policy_selection_residual;
        result.diagnostics.residual = residual;
        improved = policy_selection_improved;
        policy_selection_active = false;
        return true;
    }

    void reset_policy_iteration_units() {
        policy_unit_stage = PolicyUnitStage::Seed;
        policy_seed_pass = 0;
        policy_seed_cursor = 0;
        policy_selection_active = false;
        policy_selection_cursor = 0;
        policy_selection_improved = false;
        policy_selection_residual = 0.0;
        sparse_policy_resume.reset();
        policy_kernel_preparation.reset();
        reset_kernel_value_cache();
    }

    bool evaluate_fixed_policy() {
        reset_kernel_value_cache();
        policy_evaluation_incomplete = false;
        improper_policy_states.clear();
        result.diagnostics.policy_evaluation_failure.clear();
        const auto fail = [&](const char* reason) {
            result.diagnostics.policy_evaluation_failure = reason;
            policy_kernel_preparation.reset();
            sparse_policy_resume.reset();
            return false;
        };
        const std::size_t state_count = result.values.size();
        const std::uint64_t no_row =
            std::numeric_limits<std::uint64_t>::max();

        /* Exact fixed-policy quotient. A policy state's value equation is
         * determined entirely by its immediate cost and full transition row.
         * States with byte-identical equations therefore have identical
         * values and can share one variable. Reforge-heavy policies contain
         * thousands of such states; quotienting them turns the giant retry
         * SCC into the small exact system it represents without changing a
         * probability, carrier distinction, action, or policy choice. */
        if (policy_kernel_preparation == nullptr ||
            policy_kernel_preparation->state_count != state_count) {
            policy_kernel_preparation =
                std::make_unique<PolicyKernelPreparation>();
            policy_kernel_preparation->state_count = state_count;
            policy_kernel_preparation->kernel_owner.assign(
                state_count, kNoId);
            policy_kernel_preparation->full_kernel.resize(state_count);
            ++result.diagnostics.policy_evaluation_calls;
        }
        PolicyKernelPreparation& preparation =
            *policy_kernel_preparation;
        std::vector<std::uint32_t>& kernel_owner =
            preparation.kernel_owner;
        std::vector<std::vector<PolicyEdge>>& full_kernel =
            preparation.full_kernel;
        auto& representatives_by_hash =
            preparation.representatives_by_hash;
        auto& shared_transition_representatives =
            preparation.shared_transition_representatives;
        const auto hash_combine = [](std::size_t& hash, std::uint64_t value) {
            hash ^= static_cast<std::size_t>(value) +
                    static_cast<std::size_t>(0x9e3779b97f4a7c15ULL) +
                    (hash << 6) + (hash >> 2);
        };
        const auto kernels_equal = [&](const std::uint32_t left,
                                       const std::uint32_t right,
                                       const std::vector<PolicyEdge>& candidate) {
            if (priced_rows.at(policy_rows[left]).cost !=
                priced_rows.at(policy_rows[right]).cost) {
                return false;
            }
            const std::vector<PolicyEdge>& stored = full_kernel[left];
            if (stored.size() != candidate.size()) return false;
            for (std::size_t i = 0; i < stored.size(); ++i) {
                if (stored[i].target != candidate[i].target ||
                    stored[i].probability != candidate[i].probability) {
                    return false;
                }
            }
            return true;
        };
        constexpr std::uint32_t kKernelStatesPerWorkUnit = 64;
        const std::uint32_t kernel_end = std::min<std::uint32_t>(
            static_cast<std::uint32_t>(state_count),
            preparation.cursor + kKernelStatesPerWorkUnit);
        for (std::uint32_t state = preparation.cursor;
             state < kernel_end; ++state) {
            if (!result.expanded[state] || result.goal_states[state]) continue;
            if (policy_rows[state] == no_row) continue;
            const std::uint64_t row_index = policy_rows[state];
            const SparseRow& sparse = transition_cache->rows.at(row_index);
            if (sparse.choice_count == 0) {
                std::size_t shared_hash = static_cast<std::size_t>(
                    1469598103934665603ULL);
                hash_combine(
                    shared_hash, std::bit_cast<std::uint64_t>(
                                     priced_rows.at(row_index).cost));
                hash_combine(shared_hash, sparse.transition_offset);
                hash_combine(shared_hash, sparse.transition_count);
                std::uint32_t shared_owner = kNoId;
                for (const std::uint32_t possible :
                     shared_transition_representatives[shared_hash]) {
                    const SparseRow& other = transition_cache->rows.at(
                        policy_rows[possible]);
                    if (other.transition_offset == sparse.transition_offset &&
                        other.transition_count == sparse.transition_count &&
                        priced_rows.at(policy_rows[possible]).cost ==
                            priced_rows.at(row_index).cost) {
                        shared_owner = possible;
                        break;
                    }
                }
                if (shared_owner != kNoId) {
                    kernel_owner[state] = shared_owner;
                    continue;
                }
                shared_transition_representatives[shared_hash].push_back(
                    state);
            }
            std::vector<PolicyEdge> candidate;
            candidate.reserve(
                sparse.transition_count + sparse.choice_count + 1);
            for (std::uint32_t i = 0; i < sparse.transition_count; ++i) {
                const std::uint64_t offset = sparse.transition_offset + i;
                if (transition_cache->successors.at(offset) == state) {
                    continue;
                }
                candidate.push_back({
                    transition_cache->successors.at(offset),
                    transition_cache->probabilities.at(offset)});
            }
            if (sparse.self_probability > 0.0) {
                candidate.push_back({state, sparse.self_probability});
            }
            for (std::uint32_t i = 0; i < sparse.choice_count; ++i) {
                const SparseChoiceGroup& choice =
                    transition_cache->choices.at(sparse.choice_offset + i);
                std::uint32_t selected =
                    choice.has_self ? state : kNoId;
                double selected_value =
                    choice.has_self ? result.values[state] : kInfinity;
                for (std::uint32_t successor = 0;
                     successor < choice.successor_count; ++successor) {
                    const std::uint32_t candidate =
                        transition_cache->choice_successors.at(
                            choice.successor_offset + successor);
                    const double candidate_value = result.values[candidate];
                    if (candidate_value < selected_value - options.epsilon ||
                        (std::abs(candidate_value - selected_value) <=
                             options.epsilon &&
                         candidate < selected)) {
                        selected = candidate;
                        selected_value = candidate_value;
                    }
                }
                if (selected == kNoId) return fail("empty_observation_choice");
                candidate.push_back({selected, choice.probability});
            }
            std::sort(
                candidate.begin(), candidate.end(),
                [](const PolicyEdge& left, const PolicyEdge& right) {
                    return left.target < right.target;
                });
            std::size_t merged = 0;
            for (const PolicyEdge& entry : candidate) {
                if (entry.probability == 0.0) continue;
                if (merged != 0 &&
                    candidate[merged - 1].target == entry.target) {
                    candidate[merged - 1].probability += entry.probability;
                } else {
                    candidate[merged++] = entry;
                }
            }
            candidate.resize(merged);

            std::size_t hash =
                static_cast<std::size_t>(1469598103934665603ULL);
            hash_combine(
                hash, std::bit_cast<std::uint64_t>(
                          priced_rows.at(row_index).cost));
            for (const PolicyEdge& entry : candidate) {
                hash_combine(hash, entry.target);
                hash_combine(
                    hash, std::bit_cast<std::uint64_t>(entry.probability));
            }
            std::uint32_t owner = kNoId;
            for (const std::uint32_t possible :
                 representatives_by_hash[hash]) {
                if (kernels_equal(possible, state, candidate)) {
                    owner = possible;
                    break;
                }
            }
            if (owner == kNoId) {
                owner = state;
                representatives_by_hash[hash].push_back(state);
                full_kernel[state] = std::move(candidate);
            }
            kernel_owner[state] = owner;
        }
        preparation.cursor = kernel_end;
        if (preparation.cursor < state_count) {
            policy_evaluation_incomplete = true;
            return false;
        }

        if (preparation.representative.empty()) {
            preparation.representative = kernel_owner;
            preparation.group_members.resize(state_count);
            preparation.rows.resize(state_count);
            preparation.edges.reserve(transition_cache->successors.size());
        }
        std::vector<std::uint32_t>& representative =
            preparation.representative;
        std::vector<std::vector<std::uint32_t>>& group_members =
            preparation.group_members;
        std::vector<PolicyRow>& rows = preparation.rows;
        std::vector<PolicyEdge>& edges = preparation.edges;
        constexpr std::uint32_t kGroupingStatesPerWorkUnit = 256;
        const std::uint32_t grouping_end = std::min<std::uint32_t>(
            static_cast<std::uint32_t>(state_count),
            preparation.grouping_cursor + kGroupingStatesPerWorkUnit);
        for (std::uint32_t state = preparation.grouping_cursor;
             state < grouping_end; ++state) {
            if (representative[state] != kNoId) {
                group_members[representative[state]].push_back(state);
            }
            if (representative[state] == state) {
                ++result.diagnostics.policy_kernel_groups;
            } else if (representative[state] != kNoId) {
                ++result.diagnostics.policy_states_collapsed;
            }
        }
        preparation.grouping_cursor = grouping_end;
        if (preparation.grouping_cursor < state_count) {
            policy_evaluation_incomplete = true;
            return false;
        }
        constexpr std::uint32_t kQuotientStatesPerWorkUnit = 64;
        const std::uint32_t quotient_end = std::min<std::uint32_t>(
            static_cast<std::uint32_t>(state_count),
            preparation.quotient_cursor + kQuotientStatesPerWorkUnit);
        for (std::uint32_t state = preparation.quotient_cursor;
             state < quotient_end; ++state) {
            if (representative[state] != state) continue;
            PolicyRow& row = rows[state];
            row.edge_offset = edges.size();
            row.cost = priced_rows.at(policy_rows[state]).cost;
            std::vector<PolicyEdge> quotient;
            quotient.reserve(full_kernel[kernel_owner[state]].size());
            for (PolicyEdge entry : full_kernel[kernel_owner[state]]) {
                if (entry.target < representative.size() &&
                    representative[entry.target] != kNoId) {
                    entry.target = representative[entry.target];
                }
                quotient.push_back(entry);
            }
            std::sort(
                quotient.begin(), quotient.end(),
                [](const PolicyEdge& left, const PolicyEdge& right) {
                    return left.target < right.target;
                });
            WideFloat self_probability = 0.0;
            for (std::size_t begin = 0; begin < quotient.size();) {
                const std::uint32_t target = quotient[begin].target;
                WideFloat probability = 0.0;
                std::size_t end = begin;
                while (end < quotient.size() &&
                       quotient[end].target == target) {
                    probability = probability +
                                  WideFloat{quotient[end].probability};
                    ++end;
                }
                if (target == state) {
                    self_probability = self_probability + probability;
                } else {
                    edges.push_back({target, probability.value()});
                }
                begin = end;
            }
            const WideFloat denominator =
                WideFloat{1.0} - self_probability;
            if (denominator.value() > 1e-15) {
                const double divisor = denominator.value();
                row.cost /= divisor;
                for (std::size_t edge = row.edge_offset;
                     edge < edges.size(); ++edge) {
                    edges[edge].probability /= divisor;
                }
            }
            row.edge_count = static_cast<std::uint32_t>(
                edges.size() - row.edge_offset);
        }
        preparation.quotient_cursor = quotient_end;
        if (preparation.quotient_cursor < state_count) {
            policy_evaluation_incomplete = true;
            return false;
        }

        std::vector<std::vector<std::uint32_t>>& components =
            preparation.components;
        if (!preparation.components_ready) {
            if (preparation.tarjan_index.empty()) {
                preparation.tarjan_index.assign(state_count, kNoId);
                preparation.tarjan_lowlink.assign(state_count, kNoId);
                preparation.tarjan_on_stack.assign(state_count, 0);
            }
            const auto push = [&](const std::uint32_t state) {
                preparation.tarjan_index[state] =
                    preparation.tarjan_lowlink[state] =
                        preparation.tarjan_next_index++;
                preparation.tarjan_stack.push_back(state);
                preparation.tarjan_on_stack[state] = 1;
                preparation.tarjan_dfs.push_back({state, 0});
            };
            constexpr std::uint32_t kTarjanWorkPerUnit = 4096;
            std::uint32_t tarjan_work = 0;
            while (tarjan_work < kTarjanWorkPerUnit) {
                if (preparation.tarjan_dfs.empty()) {
                    while (preparation.tarjan_root_cursor < state_count &&
                           tarjan_work < kTarjanWorkPerUnit) {
                        const std::uint32_t root =
                            preparation.tarjan_root_cursor++;
                        ++tarjan_work;
                        if (!result.expanded[root] ||
                            result.goal_states[root] ||
                            policy_rows[root] == no_row ||
                            representative[root] != root ||
                            preparation.tarjan_index[root] != kNoId) {
                            continue;
                        }
                        push(root);
                        break;
                    }
                    if (preparation.tarjan_dfs.empty()) break;
                }
                PolicyTarjanFrame& frame = preparation.tarjan_dfs.back();
                const PolicyRow& row = rows[frame.state];
                if (frame.next_edge < row.edge_count) {
                    const std::uint32_t target = edges.at(
                        row.edge_offset + frame.next_edge++).target;
                    ++tarjan_work;
                    if (target >= state_count || !result.expanded[target] ||
                        result.goal_states[target] ||
                        policy_rows[target] == no_row) {
                        continue;
                    }
                    if (preparation.tarjan_index[target] == kNoId) {
                        push(target);
                    } else if (preparation.tarjan_on_stack[target]) {
                        preparation.tarjan_lowlink[frame.state] = std::min(
                            preparation.tarjan_lowlink[frame.state],
                            preparation.tarjan_index[target]);
                    }
                    continue;
                }
                const std::uint32_t completed = frame.state;
                preparation.tarjan_dfs.pop_back();
                ++tarjan_work;
                if (!preparation.tarjan_dfs.empty()) {
                    const std::uint32_t parent =
                        preparation.tarjan_dfs.back().state;
                    preparation.tarjan_lowlink[parent] = std::min(
                        preparation.tarjan_lowlink[parent],
                        preparation.tarjan_lowlink[completed]);
                }
                if (preparation.tarjan_lowlink[completed] ==
                    preparation.tarjan_index[completed]) {
                    components.emplace_back();
                    while (true) {
                        const std::uint32_t member =
                            preparation.tarjan_stack.back();
                        preparation.tarjan_stack.pop_back();
                        preparation.tarjan_on_stack[member] = 0;
                        components.back().push_back(member);
                        if (member == completed) break;
                    }
                    std::sort(
                        components.back().begin(), components.back().end());
                }
            }

            if (preparation.tarjan_root_cursor >= state_count &&
                preparation.tarjan_dfs.empty()) {
                preparation.component_by_state.assign(state_count, kNoId);
                for (std::uint32_t component = 0;
                     component < components.size(); ++component) {
                    for (const std::uint32_t state : components[component]) {
                        preparation.component_by_state[state] = component;
                    }
                }
                preparation.local.assign(state_count, -1);
                preparation.components_ready = true;
            }
            if (!preparation.components_ready) {
                policy_evaluation_incomplete = true;
                return false;
            }
        }
        std::vector<std::uint32_t>& component_by_state =
            preparation.component_by_state;
        std::vector<std::int32_t>& local = preparation.local;
        std::uint64_t policy_scratch =
            preparation.kernel_owner.capacity() * sizeof(std::uint32_t) +
            preparation.full_kernel.capacity() *
                sizeof(std::vector<PolicyEdge>) +
            preparation.representative.capacity() * sizeof(std::uint32_t) +
            preparation.group_members.capacity() *
                sizeof(std::vector<std::uint32_t>) +
            rows.capacity() * sizeof(PolicyRow) +
            edges.capacity() * sizeof(PolicyEdge) +
            component_by_state.capacity() * sizeof(std::uint32_t) +
            local.capacity() * sizeof(std::int32_t) +
            components.capacity() * sizeof(std::vector<std::uint32_t>) +
            preparation.tarjan_index.capacity() * sizeof(std::uint32_t) +
            preparation.tarjan_lowlink.capacity() * sizeof(std::uint32_t) +
            preparation.tarjan_on_stack.capacity() * sizeof(std::uint8_t) +
            preparation.tarjan_stack.capacity() * sizeof(std::uint32_t) +
            preparation.tarjan_dfs.capacity() * sizeof(PolicyTarjanFrame);
        for (const auto& kernel : preparation.full_kernel) {
            policy_scratch += kernel.capacity() * sizeof(PolicyEdge);
        }
        for (const auto& members : preparation.group_members) {
            policy_scratch += members.capacity() * sizeof(std::uint32_t);
        }
        for (const auto& component : components) {
            policy_scratch += component.capacity() * sizeof(std::uint32_t);
        }
        const auto map_vectors_bytes = [](const auto& values) {
            std::uint64_t bytes = values.bucket_count() * sizeof(void*);
            bytes += values.size() *
                     (sizeof(typename std::decay_t<decltype(values)>::value_type) +
                      2 * sizeof(void*));
            for (const auto& [unused, entries] : values) {
                (void)unused;
                bytes += entries.capacity() * sizeof(std::uint32_t);
            }
            return bytes;
        };
        policy_scratch += map_vectors_bytes(
            preparation.representatives_by_hash);
        policy_scratch += map_vectors_bytes(
            preparation.shared_transition_representatives);
        peak_policy_scratch_bytes = std::max(
            peak_policy_scratch_bytes, policy_scratch);
        constexpr std::uint32_t kComponentsPerWorkUnit = 64;
        const std::uint32_t component_end = std::min<std::uint32_t>(
            static_cast<std::uint32_t>(components.size()),
            preparation.component_cursor + kComponentsPerWorkUnit);
        for (std::uint32_t component = preparation.component_cursor;
             component < component_end; ++component) {
            const std::vector<std::uint32_t>& members = components[component];
            const std::size_t n = members.size();
            result.diagnostics.largest_policy_component = std::max(
                result.diagnostics.largest_policy_component,
                static_cast<std::uint32_t>(n));
            bool has_exit = false;
            for (const std::uint32_t state : members) {
                const PolicyRow& row = rows[state];
                for (std::uint32_t e = 0; e < row.edge_count; ++e) {
                    const PolicyEdge& edge = edges.at(row.edge_offset + e);
                    if (result.goal_states[edge.target] ||
                        component_by_state[edge.target] != component) {
                        has_exit = true;
                        break;
                    }
                }
                if (has_exit) break;
            }
            if (!has_exit) {
                improper_policy_states.clear();
                for (const std::uint32_t state : members) {
                    improper_policy_states.insert(
                        improper_policy_states.end(),
                        group_members[state].begin(),
                        group_members[state].end());
                }
                return fail("improper_closed_component");
            }
            for (std::size_t i = 0; i < n; ++i) local[members[i]] =
                static_cast<std::int32_t>(i);
            /* Keep policy numerics identical on native and wasm32. On x86,
             * long double uses an 80-bit accumulator while WebAssembly maps
             * it to 64-bit double, which made otherwise identical fixed
             * policies diverge beyond the approved start-value tolerance. */
            std::vector<double> rhs(n, 0.0);
            for (std::size_t i = 0; i < n; ++i) {
                const std::uint32_t state = members[i];
                double external_sum = 0.0;
                double external_correction = 0.0;
                const PolicyRow& row = rows[state];
                for (std::uint32_t e = 0; e < row.edge_count; ++e) {
                    const PolicyEdge& edge = edges.at(row.edge_offset + e);
                    if (edge.target >= state_count) {
                        return fail("successor_outside_cached_closure");
                    }
                    if (result.goal_states[edge.target]) continue;
                    if (!result.expanded[edge.target]) {
                        if (focused_lower_mode) continue;
                        return fail("policy_reaches_unexpanded_frontier");
                    }
                    if (!std::isfinite(result.values[edge.target])) {
                        return fail("policy_reaches_unexpanded_frontier");
                    }
                    if (component_by_state[edge.target] != component) {
                        const double product =
                            edge.probability * result.values[edge.target];
                        const double adjusted =
                            product - external_correction;
                        const double updated = external_sum + adjusted;
                        external_correction =
                            (updated - external_sum) - adjusted;
                        external_sum = updated;
                    }
                }
                rhs[i] = rows[state].cost + external_sum;
            }

            std::vector<double> solved(n, 0.0);
            if (n <= kDensePolicyComponentLimit) {
                peak_policy_scratch_bytes = std::max(
                    peak_policy_scratch_bytes,
                    policy_scratch + n * (n + 1) * sizeof(WideFloat));
                const std::size_t stride = n + 1;
                std::vector<WideFloat> matrix(n * stride, 0.0);
                const auto cell = [&](const std::size_t row,
                                      const std::size_t column) -> WideFloat& {
                    return matrix[row * stride + column];
                };
                for (std::size_t i = 0; i < n; ++i) {
                    cell(i, i) = 1.0;
                    cell(i, n) = rhs[i];
                    const PolicyRow& row = rows[members[i]];
                    for (std::uint32_t e = 0; e < row.edge_count; ++e) {
                        const PolicyEdge& edge = edges.at(row.edge_offset + e);
                        if (component_by_state[edge.target] == component) {
                            cell(i, static_cast<std::size_t>(
                                local[edge.target])) -=
                                WideFloat{edge.probability};
                        }
                    }
                }
                for (std::size_t column = 0; column < n; ++column) {
                    std::size_t pivot = column;
                    for (std::size_t row = column + 1; row < n; ++row) {
                        if (std::fabs(cell(row, column).value()) >
                            std::fabs(cell(pivot, column).value())) {
                            pivot = row;
                        }
                    }
                    if (std::fabs(cell(pivot, column).value()) <= 1e-15) {
                        return fail("dense_policy_component_is_singular");
                    }
                    if (pivot != column) {
                        for (std::size_t k = column; k <= n; ++k) {
                            std::swap(cell(pivot, k), cell(column, k));
                        }
                    }
                    for (std::size_t row = column + 1; row < n; ++row) {
                        const WideFloat factor =
                            cell(row, column) / cell(column, column);
                        if (factor.value() == 0.0) continue;
                        for (std::size_t k = column; k <= n; ++k) {
                            cell(row, k) -= factor * cell(column, k);
                        }
                    }
                }
                for (std::size_t back = n; back-- > 0;) {
                    WideFloat value = cell(back, n);
                    for (std::size_t column = back + 1; column < n; ++column) {
                        value -= cell(back, column) * solved[column];
                    }
                    solved[back] =
                        (value / cell(back, back)).value();
                }
            } else {
                peak_policy_scratch_bytes = std::max(
                    peak_policy_scratch_bytes,
                    policy_scratch + n * 8 * sizeof(WideFloat));
                /* Large fixed-policy components use BiCGSTAB on the sparse
                 * M-matrix (I-P). It avoids quadratic storage while retaining
                 * an explicit residual check before values are accepted. */
                std::vector<WideFloat> b(n);
                for (std::size_t i = 0; i < n; ++i) {
                    b[i] = rhs[i];
                }
                const bool can_resume =
                    sparse_policy_resume != nullptr &&
                    sparse_policy_resume->members == members &&
                    sparse_policy_resume->b == b;
                std::vector<WideFloat> x(n), r(n), r0(n), p(n, 0.0),
                    v(n, 0.0), s(n), t(n);
                WideFloat rho_previous = 1.0;
                WideFloat alpha = 1.0;
                WideFloat omega = 1.0;
                std::uint32_t resumed_iterations = 0;
                std::uint32_t refinement_count = 0;
                if (can_resume) {
                    x = std::move(sparse_policy_resume->x);
                    r = std::move(sparse_policy_resume->r);
                    r0 = std::move(sparse_policy_resume->r0);
                    p = std::move(sparse_policy_resume->p);
                    v = std::move(sparse_policy_resume->v);
                    s = std::move(sparse_policy_resume->s);
                    t = std::move(sparse_policy_resume->t);
                    rho_previous = sparse_policy_resume->rho_previous;
                    alpha = sparse_policy_resume->alpha;
                    omega = sparse_policy_resume->omega;
                    resumed_iterations = sparse_policy_resume->iterations;
                    refinement_count =
                        sparse_policy_resume->refinement_count;
                    sparse_policy_resume.reset();
                } else {
                    for (std::size_t i = 0; i < n; ++i) {
                        const double previous = result.values[members[i]];
                        x[i] = std::isfinite(previous) && previous >= 0.0 &&
                                       previous < kValueCeiling
                                   ? previous
                                   : 0.0;
                    }
                }
                const auto dot = [](const auto& left, const auto& right) {
                    WideFloat value = 0.0;
                    for (std::size_t i = 0; i < left.size(); ++i) {
                        value = value + left[i] * right[i];
                    }
                    return value;
                };
                const auto multiply = [&](
                    const std::vector<WideFloat>& input,
                    std::vector<WideFloat>& output) {
                    for (std::size_t i = 0; i < n; ++i) {
                        WideFloat internal_sum = 0.0;
                        const PolicyRow& row = rows[members[i]];
                        for (std::uint32_t e = 0; e < row.edge_count; ++e) {
                            const PolicyEdge& edge = edges.at(
                                row.edge_offset + e);
                            if (component_by_state[edge.target] == component) {
                                const WideFloat product =
                                    edge.probability * input[
                                    static_cast<std::size_t>(
                                        local[edge.target])];
                                internal_sum = internal_sum + product;
                            }
                        }
                        output[i] = input[i] - internal_sum;
                    }
                };
                const auto norm = [&](const auto& values) {
                    return std::sqrt(std::max(
                        0.0, dot(values, values).value()));
                };
                /* Fixed-policy evaluation is the numerical ground truth used
                 * by policy improvement. Solve it substantially tighter than
                 * the outer Bellman stopping tolerance so independently
                 * rounded native/WASM iterates agree at endgame value scales.
                 * Warm-starting from the preceding exact policy values makes
                 * the tighter solve cheaper than restarting BiCGSTAB at zero. */
                if (!can_resume) {
                    multiply(x, v);
                    for (std::size_t i = 0; i < n; ++i) {
                        r[i] = r0[i] = b[i] - v[i];
                    }
                }
                const double fixed_policy_relative_tolerance = 1e-18;
                const double tolerance =
                    fixed_policy_relative_tolerance * std::max(
                        1.0, norm(b));
                bool converged = norm(r) <= tolerance;
                const std::size_t max_iterations = std::min<std::size_t>(
                    100000, std::max<std::size_t>(1000, n * 10));
                constexpr std::size_t kIterationsPerWorkUnit = 4;
                const std::size_t work_unit_iterations =
                    std::min(max_iterations, kIterationsPerWorkUnit);
                std::size_t iterations = 0;
                for (; !converged && iterations < work_unit_iterations;
                     ++iterations) {
                    const WideFloat rho = dot(r0, r);
                    if (rho.value() == 0.0 || omega.value() == 0.0 ||
                        !std::isfinite(rho.value()) ||
                        !std::isfinite(omega.value())) break;
                    const WideFloat beta =
                        (rho / rho_previous) * (alpha / omega);
                    for (std::size_t i = 0; i < n; ++i) {
                        p[i] = r[i] + beta * (p[i] - omega * v[i]);
                    }
                    multiply(p, v);
                    const WideFloat denominator = dot(r0, v);
                    if (denominator.value() == 0.0 ||
                        !std::isfinite(denominator.value())) break;
                    alpha = rho / denominator;
                    for (std::size_t i = 0; i < n; ++i) {
                        s[i] = r[i] - alpha * v[i];
                    }
                    if (norm(s) <= tolerance) {
                        for (std::size_t i = 0; i < n; ++i) {
                            x[i] += alpha * p[i];
                        }
                        converged = true;
                        break;
                    }
                    multiply(s, t);
                    const WideFloat tt = dot(t, t);
                    if (tt.value() == 0.0 ||
                        !std::isfinite(tt.value())) break;
                    omega = dot(t, s) / tt;
                    for (std::size_t i = 0; i < n; ++i) {
                        x[i] += alpha * p[i] + omega * s[i];
                        r[i] = s[i] - omega * t[i];
                    }
                    converged = norm(r) <= tolerance;
                    rho_previous = rho;
                }
                bool refinement_restart = false;
                if (converged && refinement_count < 10) {
                    /* BiCGSTAB's recursively updated residual can look
                     * converged before b-Ax is equally small on a highly
                     * recurrent policy. Recompute the true residual and use
                     * it as an exact iterative-refinement restart. This keeps
                     * forward error, not only recurrence error, stable across
                     * native and WebAssembly floating-point implementations. */
                    multiply(x, t);
                    for (std::size_t i = 0; i < n; ++i) {
                        r[i] = b[i] - t[i];
                    }
                    ++refinement_count;
                    converged = norm(r) <= tolerance;
                    if (!converged) {
                        r0 = r;
                        std::fill(p.begin(), p.end(), 0.0);
                        std::fill(v.begin(), v.end(), 0.0);
                        rho_previous = 1.0;
                        alpha = 1.0;
                        omega = 1.0;
                        refinement_restart = true;
                    }
                }
                result.diagnostics.sparse_policy_iterations += iterations;
                const std::size_t total_iterations =
                    resumed_iterations + iterations;
                if (!converged &&
                    (iterations >= work_unit_iterations ||
                     refinement_restart) &&
                    total_iterations < max_iterations) {
                    sparse_policy_resume =
                        std::make_unique<SparsePolicyResume>();
                    sparse_policy_resume->members = members;
                    sparse_policy_resume->b = std::move(b);
                    sparse_policy_resume->x = std::move(x);
                    sparse_policy_resume->r = std::move(r);
                    sparse_policy_resume->r0 = std::move(r0);
                    sparse_policy_resume->p = std::move(p);
                    sparse_policy_resume->v = std::move(v);
                    sparse_policy_resume->s = std::move(s);
                    sparse_policy_resume->t = std::move(t);
                    sparse_policy_resume->rho_previous = rho_previous;
                    sparse_policy_resume->alpha = alpha;
                    sparse_policy_resume->omega = omega;
                    sparse_policy_resume->iterations =
                        static_cast<std::uint32_t>(total_iterations);
                    sparse_policy_resume->refinement_count =
                        refinement_count;
                    policy_evaluation_incomplete = true;
                    return false;
                }
                if (!converged) {
                    sparse_policy_resume.reset();
                    return fail("sparse_policy_component_did_not_converge");
                }
                result.diagnostics.max_sparse_policy_iterations = std::max(
                    result.diagnostics.max_sparse_policy_iterations,
                    resumed_iterations +
                        static_cast<std::uint32_t>(iterations));
                for (std::size_t i = 0; i < n; ++i) {
                    solved[i] = x[i].value();
                }
            }
            for (std::size_t i = 0; i < n; ++i) {
                if (!std::isfinite(solved[i]) || solved[i] < -1e-8) {
                    return fail("fixed_policy_value_is_invalid");
                }
                result.values[members[i]] = std::max(0.0, solved[i]);
                for (const std::uint32_t member :
                     group_members[members[i]]) {
                    result.values[member] = result.values[members[i]];
                }
                local[members[i]] = -1;
            }
            preparation.component_cursor = component + 1;
        }
        if (preparation.component_cursor < components.size()) {
            policy_evaluation_incomplete = true;
            return false;
        }
        policy_kernel_preparation.reset();
        sparse_policy_resume.reset();
        return true;
    }

    bool repair_improper_policy() {
        if (improper_policy_states.empty()) return false;
        std::vector<std::uint8_t> in_component(result.values.size(), 0);
        for (const std::uint32_t state : improper_policy_states) {
            in_component[state] = 1;
        }
        bool repaired = false;
        for (const std::uint32_t state : improper_policy_states) {
            double best_q = kInfinity;
            std::uint64_t best_row =
                std::numeric_limits<std::uint64_t>::max();
            const StateRowSpan& span = transition_cache->state_rows.at(state);
            for (std::uint32_t i = 0; i < span.count; ++i) {
                const std::uint64_t row_index = span.offset + i;
                if (policy_rows[state] == row_index) continue;
                if (preservation_prunes(row_index)) continue;
                const SparseRow& row = transition_cache->rows.at(row_index);
                bool exits = false;
                for (std::uint32_t transition = 0;
                     transition < row.transition_count; ++transition) {
                    const std::uint32_t successor =
                        transition_cache->successors.at(
                            row.transition_offset + transition);
                    if (successor == state) continue;
                    if (result.goal_states[successor] ||
                        !in_component[successor]) {
                        exits = true;
                        break;
                    }
                }
                for (std::uint32_t choice_index = 0;
                     !exits && choice_index < row.choice_count;
                     ++choice_index) {
                    const SparseChoiceGroup& choice =
                        transition_cache->choices.at(
                            row.choice_offset + choice_index);
                    std::uint32_t selected =
                        choice.has_self ? state : kNoId;
                    double selected_value =
                        choice.has_self ? result.values[state] : kInfinity;
                    for (std::uint32_t successor_index = 0;
                         successor_index < choice.successor_count;
                         ++successor_index) {
                        const std::uint32_t successor =
                            transition_cache->choice_successors.at(
                                choice.successor_offset + successor_index);
                        const double value = result.values[successor];
                        if (value < selected_value - options.epsilon ||
                            (std::abs(value - selected_value) <=
                                 options.epsilon &&
                             successor < selected)) {
                            selected = successor;
                            selected_value = value;
                        }
                    }
                    exits = selected != kNoId &&
                            (result.goal_states[selected] ||
                             !in_component[selected]);
                }
                if (!exits) continue;
                std::uint32_t work = 0;
                const double q = sparse_row_q(row_index, work);
                if (q < best_q - options.epsilon) {
                    best_q = q;
                    best_row = row_index;
                }
            }
            if (best_row != std::numeric_limits<std::uint64_t>::max()) {
                /* Every selected row has a certified exit from the closed
                 * component. Repair them together; the next fixed-policy
                 * evaluation proves properness and ordinary Howard
                 * improvement is still responsible for optimality. */
                policy_rows[state] = best_row;
                repaired = true;
            }
        }
        if (!repaired) return false;
        improper_policy_states.clear();
        return true;
    }

    bool run_policy_iteration_unit() {
        const auto started = std::chrono::steady_clock::now();
        const auto finish_unit = [&]() {
            result.diagnostics.optimization_ns +=
                static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now() - started)
                        .count());
        };
        if (policy_unit_stage == PolicyUnitStage::Seed) {
            /* A few alternating algebraic Gauss-Seidel passes propagate the
             * proper restart/goal bound before Howard initialization. Starting
             * directly from the finite ceiling can otherwise choose a closed
             * cross-state cycle whose one-step Q is finite but whose fixed
             * policy is improper. This is a bounded seed, not the convergence
             * algorithm. */
            if (policy_seed_cursor == 0) {
                reset_kernel_value_cache(true);
            }
            constexpr std::uint32_t kStatesPerSeedUnit = 128;
            const std::uint32_t end = std::min<std::uint32_t>(
                static_cast<std::uint32_t>(result.values.size()),
                policy_seed_cursor + kStatesPerSeedUnit);
            for (std::uint32_t offset = policy_seed_cursor;
                 offset < end; ++offset) {
                const std::uint32_t state =
                        policy_seed_pass % 2 == 0
                            ? static_cast<std::uint32_t>(
                                  result.values.size() - 1 - offset)
                            : offset;
                if (!result.expanded[state] || result.goal_states[state]) {
                    continue;
                }
                std::uint32_t work = 0;
                const double best = backup_state(state, work);
                if (best < result.values[state]) {
                    update_kernel_value_cache(
                        state, result.values[state], best);
                    result.values[state] = best;
                }
            }
            policy_seed_cursor = end;
            if (policy_seed_cursor >= result.values.size()) {
                policy_seed_cursor = 0;
                if (++policy_seed_pass >= 4) {
                    policy_unit_stage = PolicyUnitStage::InitialSelect;
                }
            }
            backup_active = true;
            finish_unit();
            return true;
        }
        if (policy_unit_stage == PolicyUnitStage::InitialSelect) {
            bool unused_improved = false;
            if (!advance_policy_selection(unused_improved)) {
                backup_active = true;
                finish_unit();
                return true;
            }
            policy_initialized = true;
            policy_stable = false;
            policy_unit_stage = PolicyUnitStage::Evaluate;
            backup_active = true;
            finish_unit();
            return true;
        }
        if (policy_unit_stage == PolicyUnitStage::Evaluate &&
            !evaluate_fixed_policy()) {
            if (policy_evaluation_incomplete) {
                backup_active = true;
                finish_unit();
                return true;
            }
            if (repair_improper_policy()) {
                policy_stable = false;
                ++sweeps;
                result.diagnostics.sweeps = sweeps;
                result.diagnostics.policy_improvement_rounds = sweeps;
                policy_unit_stage = PolicyUnitStage::Evaluate;
                finish_unit();
                backup_active = true;
                return true;
            }
            policy_iteration_failed = true;
            policy_stable = false;
            result.diagnostics.policy_iteration_fallback = true;
            backup_active = false;
            finish_unit();
            return false;
        }
        if (policy_unit_stage == PolicyUnitStage::Evaluate) {
            policy_unit_stage = PolicyUnitStage::ImproveSelect;
            backup_active = true;
            finish_unit();
            return true;
        }
        bool improved = false;
        if (!advance_policy_selection(improved)) {
            backup_active = true;
            finish_unit();
            return true;
        }
        policy_stable = !improved;
        ++sweeps;
        result.diagnostics.sweeps = sweeps;
        result.diagnostics.policy_improvement_rounds = sweeps;
        ++result.diagnostics.bellman_work_units;
        policy_unit_stage = PolicyUnitStage::Evaluate;
        finish_unit();
        if ((policy_stable && residual <= acceptable_residual()) ||
            sweeps >= options.max_sweeps) {
            backup_active = false;
        } else {
            backup_active = true;
        }
        return true;
    }

    void begin_focused_lower_solve() {
        focused_mode = true;
        focus_optimizing = true;
        focused_lower_mode = true;
        result.diagnostics.focused_expansion = true;
        const std::uint32_t state_count = calc.state_count();
        transition_cache->state_rows.resize(state_count);
        std::vector<double> previous_values = std::move(result.values);
        result.values.assign(state_count, 0.0);
        /* Expanding a previously zero-valued frontier can only raise the
         * focused lower bound. Preserve the last round's admissible values
         * for already-known states instead of restarting every exact policy
         * evaluation from zero. */
        const std::size_t retained = std::min<std::size_t>(
            previous_values.size(), result.values.size());
        for (std::size_t state = 0; state < retained; ++state) {
            if (std::isfinite(previous_values[state]) &&
                previous_values[state] >= 0.0) {
                result.values[state] = previous_values[state];
            }
        }
        result.expanded = expanded;
        result.expanded.resize(state_count, 0);
        result.goal_states.assign(state_count, 0);
        for (std::uint32_t state = 0; state < state_count; ++state) {
            if (calc.is_goal_state(calc.state(state))) {
                result.goal_states[state] = 1;
            }
        }
        prepare_priced_rows();
        policy_rows.clear();
        improper_policy_states.clear();
        policy_initialized = false;
        policy_stable = false;
        policy_iteration_failed = false;
        reset_policy_iteration_units();
        backup_active = false;
        sweeps = 0;
        residual = kValueCeiling;
        result.diagnostics.policy_evaluation_failure.clear();
    }

    bool collect_focused_fringe(std::vector<std::uint32_t>& fringe) const {
        const std::uint64_t no_row =
            std::numeric_limits<std::uint64_t>::max();
        if (result.start_state >= result.values.size()) return false;
        std::vector<std::uint8_t> visited(result.values.size(), 0);
        std::vector<std::uint8_t> queued_fringe(result.values.size(), 0);
        std::unordered_set<std::uint64_t> routed_transition_kernels;
        std::deque<std::uint32_t> walk{result.start_state};
        const auto route = [&](const std::uint32_t successor) {
            if (result.goal_states[successor]) return;
            if (!result.expanded[successor]) {
                if (!queued_fringe[successor]) {
                    queued_fringe[successor] = 1;
                    fringe.push_back(successor);
                }
            } else if (!visited[successor]) {
                walk.push_back(successor);
            }
        };
        while (!walk.empty()) {
            const std::uint32_t state = walk.front();
            walk.pop_front();
            if (visited[state] || result.goal_states[state]) continue;
            visited[state] = 1;
            if (!result.expanded[state] || state >= policy_rows.size() ||
                policy_rows[state] == no_row) {
                return false;
            }
            const SparseRow& row = transition_cache->rows.at(
                policy_rows[state]);
            const bool route_transitions =
                row.choice_count != 0 || row.transition_count == 0 ||
                routed_transition_kernels.insert(row.transition_offset)
                    .second;
            if (route_transitions) {
                for (std::uint32_t i = 0; i < row.transition_count; ++i) {
                    const std::uint32_t successor =
                        transition_cache->successors.at(
                            row.transition_offset + i);
                    if (successor != state) route(successor);
                }
            }
            for (std::uint32_t i = 0; i < row.choice_count; ++i) {
                const SparseChoiceGroup& choice =
                    transition_cache->choices.at(row.choice_offset + i);
                std::uint32_t selected = choice.has_self ? state : kNoId;
                double selected_value =
                    choice.has_self ? result.values[state] : kInfinity;
                for (std::uint32_t s = 0; s < choice.successor_count; ++s) {
                    const std::uint32_t successor =
                        transition_cache->choice_successors.at(
                            choice.successor_offset + s);
                    const double value = result.values[successor];
                    if (value < selected_value - options.epsilon ||
                        (std::abs(value - selected_value) <=
                             options.epsilon &&
                         successor < selected)) {
                        selected = successor;
                        selected_value = value;
                    }
                }
                if (selected != state && selected != kNoId) route(selected);
            }
        }
        return true;
    }

    void finish_focused_lower_solve() {
        ++result.diagnostics.focused_expansion_rounds;
        result.diagnostics.focused_lower_bound =
            result.values.at(result.start_state);
        result.diagnostics.focused_expansion_ns +=
            result.diagnostics.optimization_ns;
        std::vector<std::uint32_t> fringe;
        const bool complete = collect_focused_fringe(fringe);
        focused_closure_proved = complete && fringe.empty();

        queue.clear();
        queued.assign(calc.state_count(), 0);
        for (std::uint32_t state = 0; state < expanded.size(); ++state) {
            if (expanded[state]) queued[state] = 1;
        }
        if (!focused_closure_proved) {
            if (!complete) {
                /* A lower policy gap should be rare; fall back to the
                 * previously discovered frontier without claiming a proof. */
                for (std::uint32_t state = 0; state < calc.state_count();
                     ++state) {
                    if (!queued[state]) fringe.push_back(state);
                }
            }
            for (const std::uint32_t state : fringe) enqueue(state);
        }
        /* With every discovered state already expanded, an incomplete
         * focused proof has no remaining fringe to refine. Continue with the
         * ordinary full-closure solve instead of repeating the same lower
         * solve indefinitely. */
        if (!focused_closure_proved && queue.empty()) {
            focused_mode = false;
            full_closure_after_focused_fallback = true;
        }
        peak_queue_size = std::max<std::uint32_t>(
            peak_queue_size, static_cast<std::uint32_t>(queue.size()));

        focus_optimizing = false;
        focused_lower_mode = false;
        policy_rows.clear();
        improper_policy_states.clear();
        policy_initialized = false;
        policy_stable = false;
        policy_iteration_failed = false;
        reset_policy_iteration_units();
        backup_active = false;
        sweeps = 0;
        residual = kValueCeiling;
        result.diagnostics.sweeps = 0;
        result.diagnostics.policy_improvement_rounds = 0;
        result.diagnostics.residual = kValueCeiling;
        result.diagnostics.bellman_backups = 0;
        result.diagnostics.bellman_action_evaluations = 0;
        result.diagnostics.bellman_work_units = 0;
        result.diagnostics.optimization_ns = 0;
        result.diagnostics.policy_evaluation_failure.clear();
    }

    void run_focused_lower_unit() {
        if (!policy_iteration_failed) {
            if (!run_policy_iteration_unit()) {
                /* Focused lower bounds cannot use the descending prioritized
                 * fallback. Preserve the bound and resume full expansion. */
                focus_optimizing = false;
                focused_lower_mode = false;
                focused_mode = false;
                policy_iteration_failed = false;
                policy_initialized = false;
                policy_stable = false;
                reset_policy_iteration_units();
                return;
            }
        }
        if (!backup_active && optimization_converged()) {
            finish_focused_lower_solve();
        }
    }

    double backup_state(
        const std::uint32_t state,
        std::uint32_t& transition_work) {
        double best = kInfinity;
        transition_work = 0;
        const StateRowSpan& span = transition_cache->state_rows.at(state);
        ++result.diagnostics.bellman_backups;
        for (std::uint32_t row = 0; row < span.count; ++row) {
            std::uint32_t row_work = 0;
            if (preservation_prunes(span.offset + row)) continue;
            best = std::min(
                best, sparse_row_q(span.offset + row, row_work));
            transition_work += row_work;
            ++result.diagnostics.bellman_action_evaluations;
        }
        return best;
    }

    void begin_priority_measurement() {
        reset_kernel_value_cache();
        backup_active = true;
        backup_stage = BackupStage::Measure;
        backup_cursor = 0;
        measured_residual = 0.0;
        prioritized_states.clear();
    }

    void run_bellman_unit() {
        const auto started = std::chrono::steady_clock::now();
        if (!backup_active) begin_priority_measurement();
        const std::uint32_t state_count =
            static_cast<std::uint32_t>(result.values.size());
        constexpr std::uint32_t kStatesPerUnit = 256;
        constexpr std::uint32_t kTransitionsPerUnit = 4096;
        std::uint32_t states_done = 0;
        std::uint32_t transitions_done = 0;
        const std::uint32_t work_count =
            backup_stage == BackupStage::Measure
                ? state_count
                : static_cast<std::uint32_t>(prioritized_states.size());
        while (backup_cursor < work_count &&
               states_done < kStatesPerUnit &&
               (transitions_done < kTransitionsPerUnit || states_done == 0)) {
            const std::uint32_t state =
                backup_stage == BackupStage::Measure
                    ? backup_cursor
                    : prioritized_states[backup_cursor].second;
            ++backup_cursor;
            if (!result.expanded[state] || result.goal_states[state]) {
                continue;
            }
            std::uint32_t state_work = 0;
            const double best = backup_state(state, state_work);
            transitions_done += state_work;
            ++states_done;
            const double before = result.values[state];
            const double improvement =
                best < before ? before - best : 0.0;
            if (backup_stage == BackupStage::Measure) {
                measured_residual = std::max(measured_residual, improvement);
                if (improvement > options.epsilon) {
                    prioritized_states.push_back({improvement, state});
                }
            } else if (best < before) {
                result.values[state] = best;
            }
        }
        if (backup_cursor >= work_count) {
            if (backup_stage == BackupStage::Measure) {
                residual = measured_residual;
                result.diagnostics.residual = residual;
                if (residual <= options.epsilon ||
                    sweeps >= options.max_sweeps) {
                    backup_active = false;
                } else {
                    std::sort(
                        prioritized_states.begin(),
                        prioritized_states.end(),
                        [](const auto& left, const auto& right) {
                            return left.first != right.first
                                       ? left.first > right.first
                                       : left.second < right.second;
                        });
                    backup_stage = BackupStage::Apply;
                    backup_cursor = 0;
                }
            } else {
                ++sweeps;
                result.diagnostics.sweeps = sweeps;
                begin_priority_measurement();
            }
        }
        ++result.diagnostics.bellman_work_units;
        result.diagnostics.max_bellman_unit_transitions = std::max(
            result.diagnostics.max_bellman_unit_transitions,
            transitions_done);
        result.diagnostics.optimization_ns += static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - started)
                .count());
    }

    void step(std::uint32_t max_work_items) {
        std::uint32_t remaining = std::max<std::uint32_t>(1, max_work_items);
        while (remaining > 0 && phase != SolvePhase::Done) {
            if (phase == SolvePhase::Expanding) {
                if (focus_optimizing) {
                    run_focused_lower_unit();
                    --remaining;
                    continue;
                }
                if (!expansion_active && queue.empty() && focused_mode &&
                    !focused_closure_proved &&
                    expanded_count < options.max_expanded_states) {
                    begin_focused_lower_solve();
                    --remaining;
                    continue;
                }
                if ((!expansion_active && queue.empty()) ||
                    (!expansion_active &&
                     expanded_count >= options.max_expanded_states) ||
                    focused_closure_proved) {
                    prepare_iteration();
                    if (result.diagnostics.resource_cap_hit) {
                        phase = SolvePhase::Done;
                    }
                    break; /* expose the phase boundary to callers */
                }
                const bool completed_state = expand_one_unit();
                --remaining;
                if (completed_state && !focused_mode &&
                    expanded_count >= next_focus_checkpoint &&
                    queue.size() > 1024 &&
                    expanded_count < options.max_expanded_states) {
                    begin_focused_lower_solve();
                    continue;
                }
                if (result.diagnostics.resource_cap_hit ||
                    (completed_state &&
                     expanded_count >= options.max_expanded_states)) {
                    prepare_iteration();
                    if (result.diagnostics.resource_cap_hit) {
                        phase = SolvePhase::Done;
                    }
                    break;
                }
                if (completed_state && !expansion_active && queue.empty()) {
                    if (focused_mode && !focused_closure_proved) {
                        begin_focused_lower_solve();
                    } else {
                        prepare_iteration();
                        if (result.diagnostics.resource_cap_hit) {
                            phase = SolvePhase::Done;
                        }
                        break;
                    }
                }
                continue;
            }

            if (!backup_active &&
                (optimization_converged() ||
                 sweeps >= options.max_sweeps)) {
                phase = SolvePhase::Done;
                break;
            }
            if (!policy_iteration_failed) {
                if (!run_policy_iteration_unit()) {
                    begin_priority_measurement();
                }
            } else {
                run_bellman_unit();
            }
            --remaining;
            if (!backup_active &&
                (optimization_converged() ||
                 sweeps >= options.max_sweeps)) {
                phase = SolvePhase::Done;
            }
        }
    }

    SolveProgress progress() const {
        SolveProgress value;
        value.phase = phase;
        value.done = phase == SolvePhase::Done;
        value.expanded_states = expanded_count;
        value.sweeps = sweeps;
        value.residual = residual;
        value.start_value_bound = kValueCeiling;
        if (!result.values.empty() &&
            result.start_state < result.values.size()) {
            value.start_value_bound = result.values[result.start_state];
        }
        return value;
    }

    SolveTelemetrySnapshot telemetry_snapshot(bool abandoned) const {
        SolveTelemetrySnapshot snapshot;
        snapshot.phase = phase;
        snapshot.abandoned = abandoned;
        snapshot.diagnostics = result.diagnostics;
        snapshot.diagnostics.expanded_states = expanded_count;
        snapshot.diagnostics.discovered_states = calc.state_count();
        snapshot.diagnostics.frontier_states =
            snapshot.diagnostics.discovered_states - expanded_count;
        snapshot.diagnostics.goal_states = 0;
        for (std::uint32_t state = 0; state < calc.state_count(); ++state) {
            if (calc.is_goal_state(calc.state(state))) {
                ++snapshot.diagnostics.goal_states;
            }
        }
        snapshot.diagnostics.sweeps = sweeps;
        snapshot.diagnostics.residual = residual;
        snapshot.diagnostics.automatic_rows_considered =
            transition_cache->automatic_rows_considered;
        snapshot.diagnostics.automatic_rows_eligible =
            transition_cache->automatic_rows_eligible;
        snapshot.diagnostics.automatic_rows_rejected =
            transition_cache->automatic_rows_rejected;
        snapshot.diagnostics.automatic_rows_collapsed =
            transition_cache->automatic_rows_collapsed;
        snapshot.diagnostics.automatic_rows_deferred =
            transition_cache->automatic_rows_deferred;
        snapshot.diagnostics.automatic_kind_telemetry =
            transition_cache->automatic_kind_telemetry;
        snapshot.diagnostics.automatic_candidate_witnesses_omitted =
            snapshot.diagnostics.automatic_rows_considered;
        const std::uint64_t live_bytes = estimated_owned_bytes();
        snapshot.diagnostics.solver_owned_bytes_estimate =
            std::max(peak_owned_bytes, live_bytes);
        snapshot.diagnostics.solver_live_owned_bytes_estimate = live_bytes;
        snapshot.diagnostics.diagnostics_retained_bytes_estimate =
            diagnostics_owned_bytes(snapshot.diagnostics);
        snapshot.raw_start_bound = progress().start_value_bound;
        return snapshot;
    }

    SolveResult finish() {
        if (phase != SolvePhase::Done) {
            throw std::logic_error("solver work is not finished");
        }
        if (consumed) {
            throw std::logic_error("solver work was already finished");
        }
        const auto extraction_started = std::chrono::steady_clock::now();

        const std::uint32_t state_count =
            static_cast<std::uint32_t>(result.values.size());
        finalize_preservation_diagnostics();
        const std::uint64_t extraction_base_bytes = estimated_owned_bytes();
        bool finalization_capped =
            check_solver_byte_cap_from(extraction_base_bytes);
        /* Deterministic argmin: cost ties break toward lower cost-to-go
         * variance, then lower action index by stable registry traversal. */
        for (std::uint32_t state = 0; state < state_count; ++state) {
            if (finalization_capped) break;
            if (!result.expanded[state] || result.goal_states[state]) continue;
            double best_q = kInfinity;
            double best_variance = kInfinity;
            std::uint32_t best_operator = kNoId;
            std::uint64_t best_row_index = std::numeric_limits<std::uint64_t>::max();
            const StateRowSpan& span =
                transition_cache->state_rows.at(state);
            for (std::uint32_t row_index = 0; row_index < span.count;
                 ++row_index) {
                const std::uint64_t absolute_row = span.offset + row_index;
                if (preservation_prunes(absolute_row)) continue;
                const SparseRow& row = transition_cache->rows.at(absolute_row);
                const std::uint64_t variance_scratch_bytes =
                    (static_cast<std::uint64_t>(row.transition_count) +
                     row.choice_count + 1) *
                    sizeof(std::pair<double, double>);
                if (check_solver_byte_cap_from(
                        extraction_base_bytes,
                        variance_scratch_bytes)) {
                    finalization_capped = true;
                    break;
                }
                ++result.diagnostics.extraction_action_evaluations;
                std::uint32_t transition_work = 0;
                const double q = sparse_row_q(absolute_row, transition_work);
                if (q == kInfinity) continue;
                double mean = 0.0;
                std::vector<std::pair<double, double>> random_values;
                if (row.self_probability > 0.0) {
                    random_values.push_back(
                        {row.self_probability, result.values[state]});
                    mean += row.self_probability * result.values[state];
                }
                for (std::uint32_t i = 0;
                     i < row.transition_count; ++i) {
                    const std::uint64_t offset =
                        row.transition_offset + i;
                    if (transition_cache->successors.at(offset) == state) {
                        continue;
                    }
                    random_values.push_back(
                        {transition_cache->probabilities.at(offset),
                         result.values[
                             transition_cache->successors.at(offset)]});
                    mean += transition_cache->probabilities.at(offset) *
                            result.values[
                                transition_cache->successors.at(offset)];
                }
                for (std::uint32_t i = 0; i < row.choice_count; ++i) {
                    const SparseChoiceGroup& group =
                        transition_cache->choices.at(row.choice_offset + i);
                    double chosen = group.has_self
                                        ? result.values[state]
                                        : kInfinity;
                    for (std::uint32_t s = 0;
                         s < group.successor_count; ++s) {
                        chosen = std::min(
                            chosen,
                            result.values[transition_cache->choice_successors.at(
                                group.successor_offset + s)]);
                    }
                    random_values.push_back(
                        {group.probability, chosen});
                    mean += group.probability * chosen;
                }
                double variance = 0.0;
                for (const auto& [probability, value] : random_values) {
                    const double delta = value - mean;
                    variance += probability * delta * delta;
                }
                const bool better =
                    q < best_q - options.epsilon ||
                    (q < best_q + options.epsilon &&
                     variance < best_variance - options.epsilon);
                if (better) {
                    best_q = q;
                    best_variance = variance;
                    best_operator = priced_rows[absolute_row].operator_index;
                    best_row_index = absolute_row;
                }
            }
            result.policy[state] =
                best_operator == kNoId
                    ? PolicyOperatorRef{}
                    : PolicyOperatorRef{
                          calc.operators()[best_operator].kind,
                          best_operator};
            if (best_operator != kNoId &&
                best_row_index != std::numeric_limits<std::uint64_t>::max() &&
                calc.operators()[best_operator].kind ==
                    PlannerOperatorKind::Primitive &&
                calc.registry()
                        .actions[calc.operators()[best_operator]
                                     .primitive_action]
                        .params.type == ActionType::Unveil) {
                std::vector<OutcomeChoiceOption> choice_options;
                const PricedSparseRow& best_row = priced_rows.at(
                    best_row_index);
                if (check_solver_byte_cap(
                        static_cast<std::uint64_t>(
                            best_row.choice_option_count) *
                        sizeof(OutcomeChoiceOption))) {
                    finalization_capped = true;
                    break;
                }
                for (std::uint32_t i = 0;
                     i < best_row.choice_option_count; ++i) {
                    choice_options.push_back(
                        transition_cache->choice_options.at(
                            best_row.choice_option_offset + i));
                }
                std::sort(
                    choice_options.begin(), choice_options.end(),
                    [&](const OutcomeChoiceOption& a,
                        const OutcomeChoiceOption& b) {
                        const double left = result.values[a.state];
                        const double right = result.values[b.state];
                        return left != right ? left < right
                                             : a.mod_id < b.mod_id;
                    });
                for (const OutcomeChoiceOption& option : choice_options) {
                    result.unveil_preferences[state].push_back(
                        option.mod_id);
                }
            } else if (best_operator != kNoId &&
                       best_row_index !=
                           std::numeric_limits<std::uint64_t>::max() &&
                       calc.operators()[best_operator].kind ==
                           PlannerOperatorKind::FixedOption &&
                       priced_rows[best_row_index].choice_option_count != 0) {
                std::map<std::uint32_t, std::vector<OutcomeChoiceOption>>
                    by_observation;
                const PricedSparseRow& best_row = priced_rows.at(
                    best_row_index);
                if (check_solver_byte_cap(
                        static_cast<std::uint64_t>(
                            best_row.choice_option_count) * 128ull)) {
                    finalization_capped = true;
                    break;
                }
                for (std::uint32_t i = 0;
                     i < best_row.choice_option_count; ++i) {
                    const OutcomeChoiceOption& choice =
                        transition_cache->choice_options.at(
                            best_row.choice_option_offset + i);
                    by_observation[choice.observation_state].push_back(
                        choice);
                }
                for (auto& [observation_state, choices] : by_observation) {
                    std::sort(
                        choices.begin(), choices.end(),
                        [&](const OutcomeChoiceOption& a,
                            const OutcomeChoiceOption& b) {
                            const double left = result.values[a.state];
                            const double right = result.values[b.state];
                            return left != right ? left < right
                                                 : a.mod_id < b.mod_id;
                        });
                    ObservedUnveilPreference preference;
                    preference.observation_state = observation_state;
                    for (const OutcomeChoiceOption& choice : choices) {
                        preference.choices.push_back(
                            {choice.mod_id, choice.state,
                             choice.actual_state});
                    }
                    result.option_unveil_preferences[state].push_back(
                        std::move(preference));
                }
            }
        }
        finalize_automatic_candidate_diagnostics();
        finalization_capped =
            check_solver_byte_cap() || finalization_capped;

        if (!finalization_capped &&
            check_solver_byte_cap(
                static_cast<std::uint64_t>(state_count) *
                (sizeof(std::uint8_t) + sizeof(std::uint32_t)))) {
            finalization_capped = true;
        }
        if (!finalization_capped) {
            result.policy_reachable.assign(state_count, 0);
        } else {
            result.policy_reachable.clear();
        }
        bool reachable_policy_complete = true;
        if (!finalization_capped && result.start_state < state_count) {
            std::deque<std::uint32_t> walk{result.start_state};
            while (!walk.empty()) {
                const std::uint32_t state = walk.front();
                walk.pop_front();
                if (result.policy_reachable[state]) continue;
                result.policy_reachable[state] = 1;
                ++result.diagnostics.policy_reachable_states;
                if (result.goal_states[state]) continue;
                const std::uint32_t operator_index = result.policy[state];
                if (operator_index == kNoId) {
                    reachable_policy_complete = false;
                    continue;
                }
                const StateRowSpan& span =
                    transition_cache->state_rows.at(state);
                const SparseRow* selected = nullptr;
                for (std::uint32_t i = 0; i < span.count; ++i) {
                    const SparseRow& row =
                        transition_cache->rows.at(span.offset + i);
                    if (priced_rows[span.offset + i].operator_index ==
                        operator_index) {
                        selected = &row;
                        break;
                    }
                }
                if (selected == nullptr) {
                    reachable_policy_complete = false;
                    continue;
                }
                for (std::uint32_t i = 0;
                     i < selected->transition_count; ++i) {
                    const std::uint32_t successor =
                        transition_cache->successors.at(
                            selected->transition_offset + i);
                    if (successor == state) continue;
                    if (!result.policy_reachable[successor]) {
                        walk.push_back(successor);
                    }
                }
                for (std::uint32_t i = 0; i < selected->choice_count; ++i) {
                    const SparseChoiceGroup& group =
                        transition_cache->choices.at(
                            selected->choice_offset + i);
                    std::uint32_t chosen = group.has_self ? state : kNoId;
                    double chosen_value =
                        group.has_self ? result.values[state] : kInfinity;
                    for (std::uint32_t s = 0; s < group.successor_count; ++s) {
                        const std::uint32_t successor =
                            transition_cache->choice_successors.at(
                                group.successor_offset + s);
                        const double successor_value = result.values[successor];
                        if (successor_value < chosen_value - options.epsilon ||
                            (std::abs(successor_value - chosen_value) <=
                                 options.epsilon &&
                             successor < chosen)) {
                            chosen = successor;
                            chosen_value = successor_value;
                        }
                    }
                    /* An observation choice follows only the policy-selected
                     * successor. Unselected unveil alternatives are not
                     * policy-reachable and may intentionally have no action. */
                    if (chosen != kNoId && chosen != state &&
                        !result.policy_reachable[chosen]) {
                        walk.push_back(chosen);
                    }
                }
            }
        }
        if (finalization_capped) reachable_policy_complete = false;

        bool full_non_goal_closure = true;
        for (std::uint32_t state = 0; state < result.values.size(); ++state) {
            if (!result.expanded[state] && !result.goal_states[state]) {
                full_non_goal_closure = false;
                break;
            }
        }
        if (result.diagnostics.focused_expansion &&
            result.start_state < result.values.size()) {
            result.diagnostics.focused_upper_bound =
                result.values[result.start_state];
            if (full_closure_after_focused_fallback ||
                full_non_goal_closure) {
                result.diagnostics.focused_lower_bound =
                    result.diagnostics.focused_upper_bound;
                result.diagnostics.focused_optimality_gap = 0.0;
            } else {
                result.diagnostics.focused_optimality_gap =
                    std::max(
                        0.0,
                        result.diagnostics.focused_upper_bound -
                            result.diagnostics.focused_lower_bound);
            }
        }
        const bool focused_exact =
            !result.diagnostics.focused_expansion ||
            full_closure_after_focused_fallback ||
            full_non_goal_closure ||
            (focused_closure_proved &&
             result.diagnostics.focused_optimality_gap <=
                 options.epsilon * 10.0);
        result.converged = focused_exact &&
                           !result.diagnostics.state_cap_hit &&
                           !result.diagnostics.resource_cap_hit &&
                           optimization_converged() &&
                           reachable_policy_complete &&
                           result.start_state < state_count &&
                           result.values[result.start_state] < kValueCeiling;
        {
            std::uint64_t hash = 1469598103934665603ULL;
            const auto mix = [&hash](const std::uint64_t value) {
                hash ^= value;
                hash *= 1099511628211ULL;
            };
            for (std::size_t i = 0;
                 i < transition_cache->successors.size(); ++i) {
                mix(transition_cache->successors[i]);
                mix(std::bit_cast<std::uint64_t>(
                    transition_cache->probabilities[i]));
            }
            for (const SparseRow& row : transition_cache->rows) {
                mix(row.owner_state);
                mix(row.transition_offset);
                mix(row.transition_count);
                mix(std::bit_cast<std::uint64_t>(row.self_probability));
            }
            result.diagnostics.transition_bits_hash = hash;
            hash = 1469598103934665603ULL;
            for (std::size_t state = 0; state < result.policy.size(); ++state) {
                mix(state);
                mix(result.policy[state].index);
                mix(result.policy[state].kind == PlannerOperatorKind::Primitive
                        ? 0u
                        : 1u);
                mix(state < policy_rows.size() ? policy_rows[state]
                                               : std::uint64_t{0});
                if (state < policy_rows.size() &&
                    policy_rows[state] < priced_rows.size()) {
                    mix(std::bit_cast<std::uint64_t>(
                        priced_rows[policy_rows[state]].cost));
                }
            }
            result.diagnostics.policy_bits_hash = hash;
        }
        result.diagnostics.extraction_ns = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - extraction_started)
                .count());
        const std::uint64_t final_live_bytes = estimated_owned_bytes();
        peak_owned_bytes = std::max(peak_owned_bytes, final_live_bytes);
        if (final_live_bytes > options.max_solver_owned_bytes) {
            record_cap("max_solver_owned_bytes");
            result.converged = false;
        }
        result.diagnostics.solver_owned_bytes_estimate =
            std::max(peak_owned_bytes, estimated_owned_bytes());
        result.diagnostics.solver_live_owned_bytes_estimate =
            estimated_retained_solver_bytes(calc, &result);
        result.diagnostics.diagnostics_retained_bytes_estimate =
            diagnostics_owned_bytes(result.diagnostics);
        consumed = true;
        return std::move(result);
    }

    std::uint64_t estimated_owned_bytes() const {
        return estimated_owned_bytes_with_calc(
            calc.estimated_owned_bytes());
    }

    std::uint64_t audited_estimated_owned_bytes() const {
        return estimated_owned_bytes_with_calc(
            calc.audited_estimated_owned_bytes());
    }

    std::uint64_t fast_estimated_owned_bytes() const {
        return estimated_owned_bytes_with_calc(
            calc.fast_estimated_owned_bytes());
    }

    std::uint64_t estimated_owned_bytes_with_calc(
        const std::uint64_t calc_bytes) const {
        std::uint64_t bytes = sizeof(*this) + calc_bytes;
        bytes += prices.bucket_count() * sizeof(void*);
        bytes += prices.size() *
                 (sizeof(std::pair<const std::string, double>) +
                  2 * sizeof(void*));
        for (const auto& [key, price] : prices) {
            (void)price;
            bytes += key.capacity() + 1;
        }
        bytes += operators.capacity() * sizeof(PricedOperator);
        for (const PricedOperator& priced : operators) {
            bytes += priced.resource_prices.capacity() *
                     sizeof(std::pair<std::string, double>);
            for (const auto& [key, price] : priced.resource_prices) {
                (void)price;
                bytes += key.capacity() + 1;
            }
        }
        bytes += (reported_unsupported.capacity() + 7) / 8;
        bytes += static_operator_indices.capacity() * sizeof(std::uint32_t);
        bytes += expansion_operator_indices.capacity() *
                 sizeof(std::uint32_t);
        bytes += expanded.capacity() * sizeof(std::uint8_t);
        bytes += queued.capacity() * sizeof(std::uint8_t);
        bytes += static_cast<std::uint64_t>(peak_queue_size) *
                 sizeof(std::uint32_t);
        if (transition_cache != nullptr) {
            bytes += transition_cache->estimated_owned_bytes();
        }
        bytes += priced_rows.capacity() * sizeof(PricedSparseRow);
        bytes += priced_operator_position.capacity() * sizeof(std::int32_t);
        bytes += prioritized_states.capacity() *
                 sizeof(std::pair<double, std::uint32_t>);
        bytes += policy_rows.capacity() * sizeof(std::uint64_t);
        bytes += peak_policy_scratch_bytes;
        bytes += kernel_rows_by_hash.bucket_count() * sizeof(void*);
        bytes += kernel_rows_by_hash.size() *
                 (sizeof(std::pair<const std::size_t,
                                   std::vector<std::uint64_t>>) +
                  2 * sizeof(void*));
        for (const auto& [unused, rows] : kernel_rows_by_hash) {
            (void)unused;
            bytes += rows.capacity() * sizeof(std::uint64_t);
        }
        bytes += automatic_admission_records.bucket_count() * sizeof(void*);
        bytes += automatic_admission_records.size() *
                 (sizeof(std::uint64_t) + 2 * sizeof(void*));
        bytes += automatic_carrier_work.bucket_count() * sizeof(void*);
        bytes += automatic_carrier_work.size() *
                 (sizeof(std::uint64_t) + sizeof(AutomaticCarrierWork) +
                  2 * sizeof(void*));
        bytes += result.values.capacity() * sizeof(double);
        bytes += result.policy.capacity() * sizeof(PolicyOperatorRef);
        bytes += result.expanded.capacity() * sizeof(std::uint8_t);
        bytes += result.goal_states.capacity() * sizeof(std::uint8_t);
        bytes += result.policy_reachable.capacity() * sizeof(std::uint8_t);
        bytes += result.unveil_preferences.capacity() *
                 sizeof(std::vector<std::uint32_t>);
        for (const auto& preferences : result.unveil_preferences) {
            bytes += preferences.capacity() * sizeof(std::uint32_t);
        }
        bytes += result.option_unveil_preferences.capacity() *
                 sizeof(std::vector<ObservedUnveilPreference>);
        for (const auto& preferences :
             result.option_unveil_preferences) {
            bytes += preferences.capacity() *
                     sizeof(ObservedUnveilPreference);
            for (const ObservedUnveilPreference& preference : preferences) {
                bytes += preference.choices.capacity() *
                         sizeof(ObservedUnveilChoice);
            }
        }
        bytes += diagnostics_owned_bytes(result.diagnostics);
        return bytes;
    }
};

SolveWork::SolveWork(
    CalcContext& calc,
    const pc_item_state& start_item,
    const std::unordered_map<std::string, double>& prices,
    const SolveOptions& options)
    : impl_(std::make_unique<Impl>(calc, start_item, prices, options)) {}

SolveWork::~SolveWork() = default;
SolveWork::SolveWork(SolveWork&&) noexcept = default;
SolveWork& SolveWork::operator=(SolveWork&&) noexcept = default;

void SolveWork::step(const std::uint32_t max_work_items) {
    impl_->step(max_work_items);
}

SolveProgress SolveWork::progress() const {
    return impl_->progress();
}

SolveTelemetrySnapshot SolveWork::telemetry_snapshot(bool abandoned) const {
    return impl_->telemetry_snapshot(abandoned);
}

SolveResult SolveWork::finish() {
    return impl_->finish();
}

std::uint64_t SolveWork::live_owned_bytes() const {
    return impl_->estimated_owned_bytes();
}

std::uint64_t SolveWork::peak_owned_bytes() const {
    return std::max(
        impl_->peak_owned_bytes, impl_->estimated_owned_bytes());
}

std::uint64_t estimated_retained_solver_bytes(
    const CalcContext& calc,
    const SolveResult* result) {
    std::uint64_t bytes = calc.estimated_owned_bytes();
    if (calc.solve_transition_cache() != nullptr) {
        bytes += calc.solve_transition_cache()->estimated_owned_bytes();
    }
    if (result != nullptr) bytes += solve_result_owned_bytes(*result);
    return bytes;
}

SolveResult solve(
    CalcContext& calc,
    const pc_item_state& start_item,
    const std::unordered_map<std::string, double>& prices,
    const SolveOptions& options) {
    SolveWork work(calc, start_item, prices, options);
    while (!work.progress().done) work.step(4096);
    return work.finish();
}

std::string serialize_solve_log(
    const CalcContext& calc,
    const SolveResult& result) {
    std::string log;
    char buffer[256];
    for (std::uint32_t state = 0; state < result.values.size(); ++state) {
        if (!result.expanded[state]) continue;
        const AbstractState& features = calc.state(state);
        std::snprintf(buffer, sizeof(buffer),
                      "{\"state\":%u,\"value\":%.9g,\"action\":", state,
                      result.values[state]);
        log += buffer;
        const std::uint32_t action = result.policy[state];
        if (action == kNoId) {
            log += "null";
        } else {
            log += '"';
            log += calc.operators().at(action).id;
            log += '"';
        }
        std::snprintf(
            buffer, sizeof(buffer),
            ",\"goal\":%d,\"reachable\":%d,\"rarity\":%u,\"prefixes\":%u,"
            "\"suffixes\":%u,\"blocked\":%u,\"flags\":%u,"
            "\"veiled_side\":%d,\"searing_tier\":%u,\"eater_tier\":%u,"
            "\"slots\":[",
            result.goal_states[state] ? 1 : 0,
            result.policy_reachable[state] ? 1 : 0, features.rarity,
            features.prefix_count, features.suffix_count,
            features.blocked_mask, features.flags, features.veiled_side,
            features.searing_exarch_tier,
            features.eater_of_worlds_tier);
        log += buffer;
        for (std::size_t i = 0; i < calc.layout().slots.size(); ++i) {
            if (i > 0) log += ',';
            log += std::to_string(features.slot_status[i]);
        }
        log += "],\"junk\":[";
        for (std::size_t i = 0; i < features.junk_counts.size(); ++i) {
            if (i > 0) log += ',';
            log += std::to_string(features.junk_counts[i]);
        }
        log += "]}\n";
    }
    return log;
}

class BoundedTelemetryJson {
  public:
    explicit BoundedTelemetryJson(std::uint64_t limit) : limit_(limit) {}

    BoundedTelemetryJson& operator+=(const std::string& text) {
        append(text);
        return *this;
    }
    BoundedTelemetryJson& operator+=(const char* text) {
        append(text == nullptr ? std::string_view{} : std::string_view(text));
        return *this;
    }
    BoundedTelemetryJson& operator+=(char value) {
        push_back(value);
        return *this;
    }
    void push_back(char value) {
        ensure(1);
        value_.push_back(value);
    }
    std::size_t size() const { return value_.size(); }
    std::string take() && { return std::move(value_); }

  private:
    void append(std::string_view text) {
        ensure(text.size());
        value_.append(text.data(), text.size());
    }
    void ensure(std::size_t additional) const {
        if (value_.size() > limit_ || additional > limit_ - value_.size()) {
            throw std::length_error(
                "solver telemetry exceeded max_telemetry_json_bytes (" +
                std::to_string(limit_) + ")");
        }
    }
    std::string value_;
    std::uint64_t limit_;
};

std::string serialize_solver_telemetry(
    const CalcContext& calc,
    const SolveResult* result,
    const SolveTelemetrySnapshot* snapshot,
    const std::optional<std::uint64_t>& registry_generation_ns,
    const PolicyCompilationTelemetry* compilation) {
    const CalcTelemetry& cache = calc.telemetry();
    const SolveDiagnostics* diagnostics =
        result != nullptr
            ? &result->diagnostics
            : (snapshot == nullptr ? nullptr : &snapshot->diagnostics);
    const bool qualified_action_subset =
        diagnostics != nullptr &&
        (diagnostics->skipped_missing_price_count != 0 ||
         diagnostics->evaluator_supported_actions <
             diagnostics->candidate_actions ||
         diagnostics->skipped_unsupported_count != 0);
    const auto count_or_null = [](const std::uint64_t* value) {
        return value == nullptr ? std::string("null")
                                : std::to_string(*value);
    };
    const auto optional_count = [](const auto& value) {
        return value.has_value() ? std::to_string(*value)
                                 : std::string("null");
    };
    const auto bool_json = [](bool value) {
        return value ? "true" : "false";
    };

    const std::uint64_t output_limit =
        diagnostics == nullptr
            ? SolveOptions{}.max_telemetry_json_bytes
            : diagnostics->telemetry_json_byte_limit;
    BoundedTelemetryJson json(output_limit);
    json += "{\"version\":\"solver_telemetry_v1\"";
    json += ",\"availability\":{";
    json += "\"evaluator_support\":\"applied_before_expansion\"";
    json += ",\"relevance_filter\":\"explicit_envelope_or_conservative_include\"";
    json += ",\"dominance_filter\":\"certified_kernel_equivalence_or_preservation_restart_bound\"";
    json += ",\"policy_improvement_rounds\":\"available\"";
    json += ",\"optimality_gap\":\"available_for_focused_expansion\"";
    json += ",\"verification\":\"external_harness\"}";

    json += ",\"execution\":{";
    if (result != nullptr) {
        json += "\"status\":\"complete\",\"phase\":\"done\"";
    } else if (snapshot != nullptr) {
        const char* phase = snapshot->phase == SolvePhase::Expanding
                                ? "expanding"
                                : (snapshot->phase == SolvePhase::Iterating
                                       ? "iterating"
                                       : "ready_to_finish");
        json += "\"status\":\"";
        json += snapshot->abandoned ? "abandoned" : "in_progress";
        json += "\",\"phase\":\"" + std::string(phase) + "\"";
    } else {
        json += "\"status\":\"not_started\",\"phase\":null";
    }
    json += "}";

    const std::uint64_t registry_actions =
        diagnostics == nullptr
            ? calc.registry().actions.size()
            : diagnostics->registry_actions;
    const std::uint64_t candidate_actions =
        diagnostics == nullptr ? calc.candidates().size()
                               : diagnostics->candidate_actions;
    json += ",\"actions\":{\"registry\":" +
            std::to_string(registry_actions);
    json += ",\"registry_before_lazy\":" + std::to_string(
        registry_actions + calc.registry().fossil_loadouts_deferred);
    json += ",\"candidate\":" + std::to_string(candidate_actions);
    if (diagnostics == nullptr) {
        json += ",\"evaluator_supported\":null";
        json += ",\"priced_scanned\":null,\"supported_priced\":null";
        json += ",\"unsupported_requested\":null";
        json += ",\"relevance_reduced\":null,\"dominance_reduced\":null";
        json += ",\"deferred\":null,\"equivalent_price_ties\":null";
        json += ",\"missing_price\":null,\"unsupported_observed\":null";
    } else {
        json += ",\"evaluator_supported\":" +
                std::to_string(diagnostics->evaluator_supported_actions);
        json += ",\"priced_scanned\":" +
                std::to_string(diagnostics->priced_scanned_actions);
        json += ",\"supported_priced\":" +
                std::to_string(diagnostics->supported_priced_actions);
        json += ",\"unsupported_requested\":" +
                std::to_string(diagnostics->candidate_actions -
                               diagnostics->evaluator_supported_actions);
        json += ",\"relevance_reduced\":" + std::to_string(
                    diagnostics->relevance_reduced_actions);
        json += ",\"dominance_reduced\":" + std::to_string(
                    diagnostics->equivalent_actions_collapsed +
                    diagnostics->preservation_rows_pruned);
        json += ",\"deferred\":" + std::to_string(
                    diagnostics->deferred_actions);
        json += ",\"equivalent_price_ties\":" + std::to_string(
                    diagnostics->equivalent_price_ties);
        json += ",\"missing_price\":" + std::to_string(
                    diagnostics->skipped_missing_price_count);
        json += ",\"unsupported_observed\":" + std::to_string(
                    diagnostics->skipped_unsupported_count);
    }
    json += "}";

    json += ",\"action_control\":{";
    json += "\"explicit_envelope\":" + std::string(bool_json(
        calc.action_control().explicit_envelope));
    json += ",\"dependency_primitives\":" + std::to_string(
        calc.action_control().dependency_primitives);
    json += ",\"goal_relevant_pruned\":" + std::to_string(
        calc.action_control().pruned_outside_goal_relevance);
    if (diagnostics == nullptr) {
        json += ",\"preservation_rows\":null";
    } else {
        json += ",\"preservation_rows\":{";
        json += "\"considered\":" + std::to_string(
            diagnostics->preservation_rows_considered);
        json += ",\"included\":" + std::to_string(
            diagnostics->preservation_rows_retained);
        json += ",\"pruned\":" + std::to_string(
            diagnostics->preservation_rows_pruned);
        json += ",\"certified_disposable\":" + std::to_string(
            diagnostics->certified_disposable_rows) + "}";
    }
    json += ",\"fossil_loadouts\":{";
    json += "\"possible\":" + std::to_string(
        calc.registry().fossil_loadouts_possible);
    json += ",\"generated\":" + std::to_string(
        calc.registry().fossil_loadouts_generated);
    json += ",\"deferred\":" + std::to_string(
        calc.registry().fossil_loadouts_deferred);
    json += ",\"lazy\":" + std::string(bool_json(
        calc.registry().fossil_generation_lazy));
    json += ",\"mode\":\"";
    json += calc.registry().fossil_generation_goal_relevant
                ? "goal_relevant"
                : calc.registry().fossil_generation_lazy ? "requested"
                                                         : "exhaustive";
    json += "\"}";
    json += ",\"reasons\":[";
    if (diagnostics != nullptr) {
        for (std::size_t i = 0;
             i < diagnostics->action_inclusion_reasons.size(); ++i) {
            if (i != 0) json += ',';
            json += '"';
            for (const char c : diagnostics->action_inclusion_reasons[i]) {
                if (c == '"' || c == '\\') json += '\\';
                json += c;
            }
            json += '"';
        }
    }
    json += "],\"reason_samples\":{\"limit\":";
    json += diagnostics == nullptr
                ? "null"
                : std::to_string(diagnostics->diagnostic_sample_limit);
    json += ",\"omitted\":";
    json += diagnostics == nullptr
                ? "null"
                : std::to_string(
                      diagnostics->action_inclusion_reasons_omitted);
    json += "},\"preservation_witnesses\":[";
    if (diagnostics != nullptr) {
        for (std::size_t i = 0;
             i < diagnostics->preservation_witnesses.size(); ++i) {
            if (i != 0) json.push_back(',');
            json += diagnostics->preservation_witnesses[i];
        }
    }
    json += "],\"preservation_witness_samples\":{\"retained\":";
    json += diagnostics == nullptr
                ? "null"
                : std::to_string(
                      diagnostics->preservation_witnesses.size());
    json += ",\"omitted\":";
    json += diagnostics == nullptr
                ? "null"
                : std::to_string(
                      diagnostics->preservation_witnesses_omitted);
    json += "},\"automatic_candidates\":{";
    json += "\"enabled\":" + std::string(bool_json(
        calc.goal().automatic_candidates));
    json += ",\"operators\":" + std::to_string(
        calc.action_control().automatic_options);
    json += ",\"dependency_primitives\":" + std::to_string(
        calc.action_control().automatic_dependency_primitives);
    if (diagnostics == nullptr) {
        json += ",\"rows\":null,\"by_kind\":null,\"witnesses\":[]";
    } else {
        json += ",\"rows\":{\"considered\":" + std::to_string(
            diagnostics->automatic_rows_considered);
        json += ",\"eligible\":" + std::to_string(
            diagnostics->automatic_rows_eligible);
        json += ",\"rejected\":" + std::to_string(
            diagnostics->automatic_rows_rejected);
        json += ",\"collapsed\":" + std::to_string(
            diagnostics->automatic_rows_collapsed);
        json += ",\"selected\":" + std::to_string(
            diagnostics->automatic_rows_selected);
        json += ",\"deferred\":" + std::to_string(
            diagnostics->automatic_rows_deferred) + "}";
        json += ",\"by_kind\":{";
        for (std::size_t i = 0; i < kAutomaticTelemetryKindCount; ++i) {
            if (i != 0) json.push_back(',');
            const AutomaticTelemetryKind kind =
                static_cast<AutomaticTelemetryKind>(i);
            const AutomaticKindTelemetry& values =
                diagnostics->automatic_kind_telemetry[i];
            json += '"';
            json += automatic_telemetry_kind_name(kind);
            json += "\":{\"candidates\":" +
                    std::to_string(values.candidates);
            json += ",\"carriers\":" +
                    std::to_string(values.carriers);
            json += ",\"max_candidates_per_carrier\":" +
                    std::to_string(values.max_candidates_per_carrier);
            json += ",\"precompiled_classes\":" +
                    std::to_string(values.precompiled_classes);
            json += ",\"precompile_time_ns\":" +
                    std::to_string(values.precompile_ns);
            json += ",\"precompiled_bytes\":" +
                    std::to_string(values.precompiled_bytes);
            json += ",\"candidate_variants\":" +
                    std::to_string(values.candidate_variants);
            json += ",\"max_candidate_variants_per_carrier\":" +
                    std::to_string(
                        values.max_candidate_variants_per_carrier);
            json += ",\"effect_classes\":" +
                    std::to_string(values.effect_classes);
            json += ",\"max_effect_classes_per_carrier\":" +
                    std::to_string(values.max_effect_classes_per_carrier);
            json += ",\"collapsed_variants\":" +
                    std::to_string(values.collapsed_variants);
            json += ",\"unique_templates\":" +
                    std::to_string(values.unique_templates);
            json += ",\"template_hits\":" +
                    std::to_string(values.template_hits);
            json += ",\"max_templates_per_carrier\":" +
                    std::to_string(values.max_templates_per_carrier);
            json += ",\"rows\":" + std::to_string(values.rows);
            json += ",\"max_rows_per_carrier\":" +
                    std::to_string(values.max_rows_per_carrier);
            json += ",\"raw_outcomes\":" +
                    std::to_string(values.raw_outcomes);
            json += ",\"retained_transitions\":" +
                    std::to_string(values.retained_transitions);
            json += ",\"time_ns\":" +
                    std::to_string(values.admission_ns);
            json += ",\"enumeration_time_ns\":" +
                    std::to_string(values.enumeration_ns);
            json += ",\"row_time_ns\":" +
                    std::to_string(values.row_ns);
            json += ",\"selected_bytes\":" +
                    std::to_string(values.selected_bytes) + "}";
        }
        json += "}";
        json += ",\"witnesses\":[";
        for (std::size_t i = 0;
             i < diagnostics->automatic_candidate_witnesses.size(); ++i) {
            if (i != 0) json.push_back(',');
            json += diagnostics->automatic_candidate_witnesses[i];
        }
        json += "],\"witness_samples\":{\"retained\":" +
                std::to_string(
                    diagnostics->automatic_candidate_witnesses.size()) +
                ",\"omitted\":" +
                std::to_string(
                    diagnostics->automatic_candidate_witnesses_omitted) +
                ",\"limit\":" +
                std::to_string(diagnostics->diagnostic_sample_limit) + "}";
    }
    json += "}}";

    json += ",\"planner\":{\"registry\":" +
            std::to_string(calc.operators().size());
    json += ",\"candidate\":" +
            std::to_string(calc.candidate_operators().size());
    json += ",\"fixed_options\":" +
            std::to_string(calc.operators().size() -
                           calc.registry().actions.size()) +
            "}";

    json += ",\"abstraction\":{\"discriminating_tags\":" +
            std::to_string(calc.layout().discriminating_tag_ids.size());
    json += ",\"junk_classes\":" +
            std::to_string(calc.layout().junk_classes.size()) + "}";

    json += ",\"states\":{";
    if (diagnostics == nullptr) {
        json += "\"discovered\":null,\"expanded\":null,\"frontier\":null";
        json += ",\"goal\":null,\"policy_reachable\":null";
    } else {
        json += "\"discovered\":" +
                std::to_string(diagnostics->discovered_states);
        json += ",\"expanded\":" +
                std::to_string(diagnostics->expanded_states);
        json += ",\"frontier\":" +
                std::to_string(diagnostics->frontier_states);
        json += ",\"goal\":" +
                std::to_string(diagnostics->goal_states);
        if (result == nullptr) {
            json += ",\"policy_reachable\":null";
        } else {
            json += ",\"policy_reachable\":" +
                    std::to_string(diagnostics->policy_reachable_states);
        }
    }
    json += "}";

    json += ",\"focused_expansion\":{";
    if (diagnostics == nullptr) {
        json += "\"used\":null,\"rounds\":null,\"lower_bound\":null";
        json += ",\"upper_bound\":null,\"optimality_gap\":null";
        json += ",\"duration_ns\":null";
    } else {
        json += "\"used\":" + std::string(bool_json(
            diagnostics->focused_expansion));
        json += ",\"rounds\":" + std::to_string(
            diagnostics->focused_expansion_rounds);
        const auto append_bound = [&](const double value) {
            if (std::isfinite(value)) {
                char buffer[40];
                std::snprintf(buffer, sizeof(buffer), "%.17g", value);
                json += buffer;
            } else {
                json += "null";
            }
        };
        json += ",\"lower_bound\":";
        append_bound(diagnostics->focused_lower_bound);
        json += ",\"upper_bound\":";
        append_bound(diagnostics->focused_upper_bound);
        json += ",\"optimality_gap\":";
        append_bound(diagnostics->focused_optimality_gap);
        json += ",\"duration_ns\":" + std::to_string(
            diagnostics->focused_expansion_ns);
    }
    json += "}";

    json += ",\"work\":{\"state_action_rows\":" +
            std::to_string(diagnostics == nullptr
                               ? cache.state_action_rows
                               : diagnostics->sparse_rows);
    json += ",\"transition_entries\":" +
            std::to_string(diagnostics == nullptr
                               ? cache.transition_entries
                               : diagnostics->sparse_transitions);
    json += ",\"outcome_entries\":" +
            std::to_string(cache.outcome_entries);
    json += ",\"choice_groups\":" +
            std::to_string(cache.choice_groups);
    json += ",\"choice_successor_entries\":" +
            std::to_string(cache.choice_successor_entries);
    if (diagnostics == nullptr) {
        json += ",\"bellman_backups\":null";
        json += ",\"bellman_action_evaluations\":null";
        json += ",\"extraction_action_evaluations\":null";
        json += ",\"bellman_work_units\":null";
        json += ",\"max_bellman_unit_transitions\":null";
        json += ",\"algebraic_self_loops\":null";
    } else {
        json += ",\"bellman_backups\":" +
                std::to_string(diagnostics->bellman_backups);
        json += ",\"bellman_action_evaluations\":" +
                std::to_string(diagnostics->bellman_action_evaluations);
        json += ",\"extraction_action_evaluations\":" +
                std::to_string(diagnostics->extraction_action_evaluations);
        json += ",\"bellman_work_units\":" +
                std::to_string(diagnostics->bellman_work_units);
        json += ",\"max_bellman_unit_transitions\":" +
                std::to_string(
                    diagnostics->max_bellman_unit_transitions);
        json += ",\"algebraic_self_loops\":" +
                std::to_string(diagnostics->algebraic_self_loops);
        char hash_buffer[17];
        std::snprintf(
            hash_buffer, sizeof(hash_buffer), "%016llx",
            static_cast<unsigned long long>(
                diagnostics->transition_bits_hash));
        json += ",\"transition_bits_hash\":\"";
        json += hash_buffer;
        json += '"';
        std::snprintf(
            hash_buffer, sizeof(hash_buffer), "%016llx",
            static_cast<unsigned long long>(
                diagnostics->policy_bits_hash));
        json += ",\"policy_bits_hash\":\"";
        json += hash_buffer;
        json += '"';
    }
    json += "}";

    json += ",\"cache\":{\"distribution\":{\"requests\":" +
            std::to_string(cache.distribution_requests);
    json += ",\"hits\":" + std::to_string(cache.distribution_hits);
    json += ",\"misses\":" + std::to_string(cache.distribution_misses);
    json += ",\"entries\":" +
            std::to_string(calc.cached_distribution_count());
    json += ",\"build_ns\":" +
            std::to_string(cache.distribution_build_ns);
    json += ",\"released_after_sparse_copy\":" +
            std::string(bool_json(diagnostics != nullptr)) + "}";
    json += ",\"transition_graph\":{\"reused\":";
    json += diagnostics == nullptr
                ? "null"
                : bool_json(diagnostics->transition_cache_reused);
    json += "}";
    json += ",\"reforge\":{\"requests\":" +
            std::to_string(cache.reforge_requests);
    json += ",\"hits\":" + std::to_string(cache.reforge_hits);
    json += ",\"misses\":" + std::to_string(cache.reforge_misses);
    json += ",\"entries\":" +
            std::to_string(calc.cached_reforge_count());
    json += ",\"build_ns\":" +
            std::to_string(cache.reforge_build_ns);
    json += ",\"frontier_work\":" +
            std::to_string(cache.reforge_frontier_work) + "}";
    json += ",\"primitive_families\":{";
    for (std::size_t i = 0; i < kPrimitiveTelemetryFamilyCount; ++i) {
        if (i != 0) json.push_back(',');
        const PrimitiveTelemetryFamily family =
            static_cast<PrimitiveTelemetryFamily>(i);
        const PrimitiveFamilyTelemetry& values =
            cache.primitive_families[i];
        json += '"';
        json += primitive_telemetry_family_name(family);
        json += "\":{\"requests\":" + std::to_string(values.requests);
        json += ",\"cache_hits\":" +
                std::to_string(values.cache_hits);
        json += ",\"rows\":" + std::to_string(values.rows);
        json += ",\"raw_outcomes\":" +
                std::to_string(values.raw_outcomes);
        json += ",\"transitions\":" +
                std::to_string(values.transitions);
        json += ",\"time_ns\":" + std::to_string(values.build_ns);
        json += ",\"row_time_ns\":" +
                std::to_string(values.row_ns);
        json += ",\"selected_bytes\":" +
                std::to_string(values.selected_bytes) + "}";
    }
    json += "}}";

    json += ",\"optimization\":{";
    json += "\"method\":\"";
    json += diagnostics != nullptr && diagnostics->policy_iteration_fallback
                ? "policy_iteration_scc_with_prioritized_fallback"
                : "policy_iteration_scc";
    json += "\"";
    json += ",\"policy_evaluation_calls\":" + std::to_string(
        diagnostics == nullptr ? 0 : diagnostics->policy_evaluation_calls);
    json += ",\"largest_policy_component\":" + std::to_string(
        diagnostics == nullptr ? 0 : diagnostics->largest_policy_component);
    json += ",\"sparse_policy_iterations\":" + std::to_string(
        diagnostics == nullptr ? 0 : diagnostics->sparse_policy_iterations);
    json += ",\"max_sparse_policy_iterations\":" + std::to_string(
        diagnostics == nullptr ? 0
                               : diagnostics->max_sparse_policy_iterations);
    json += ",\"policy_kernel_groups\":" + std::to_string(
        diagnostics == nullptr ? 0 : diagnostics->policy_kernel_groups);
    json += ",\"policy_states_collapsed\":" + std::to_string(
        diagnostics == nullptr ? 0 : diagnostics->policy_states_collapsed);
    json += ",\"policy_evaluation_failure\":";
    if (diagnostics == nullptr ||
        diagnostics->policy_evaluation_failure.empty()) {
        json += "null";
    } else {
        json += "\"" + diagnostics->policy_evaluation_failure + "\"";
    }
    if (result == nullptr && snapshot == nullptr) {
        json += ",\"status\":\"not_run\",\"converged\":null";
        json += ",\"sweeps\":null,\"policy_improvement_rounds\":null";
        json += ",\"residual\":null,\"optimality_gap\":null";
        json += ",\"state_cap_hit\":null";
        json += ",\"resource_cap_hit\":null,\"cap_hits\":[]";
        json += ",\"full_request_status\":\"not_run\"";
    } else if (result == nullptr) {
        json += ",\"status\":\"";
        json += snapshot->abandoned ? "abandoned" : "in_progress";
        json += "\",\"converged\":null";
        json += ",\"sweeps\":" + std::to_string(diagnostics->sweeps);
        json += ",\"policy_improvement_rounds\":" + std::to_string(
                    diagnostics->policy_improvement_rounds);
        json += ",\"residual\":" + std::to_string(diagnostics->residual);
        json += ",\"optimality_gap\":";
        if (diagnostics->focused_expansion &&
            std::isfinite(diagnostics->focused_optimality_gap)) {
            char buffer[40];
            std::snprintf(
                buffer, sizeof(buffer), "%.17g",
                diagnostics->focused_optimality_gap);
            json += buffer;
        } else {
            json += "null";
        }
        json += ",\"state_cap_hit\":" +
                std::string(bool_json(diagnostics->state_cap_hit));
        json += ",\"resource_cap_hit\":" +
                std::string(bool_json(diagnostics->resource_cap_hit));
        json += ",\"cap_hits\":[]";
        json += ",\"full_request_status\":\"incomplete_not_finished\"";
    } else {
        const char* status = result->converged
                                 ? (qualified_action_subset
                                        ? "exact_supported_priced_subset"
                                        : "exact_abstract")
                                 : (diagnostics->state_cap_hit
                                        ? "incomplete_state_cap"
                                        : (diagnostics->resource_cap_hit
                                               ? "incomplete_resource_cap"
                                               : "not_converged"));
        json += ",\"status\":\"" + std::string(status) + "\"";
        json += ",\"converged\":" +
                std::string(bool_json(result->converged));
        json += ",\"sweeps\":" + std::to_string(diagnostics->sweeps);
        json += ",\"policy_improvement_rounds\":" + std::to_string(
                    diagnostics->policy_improvement_rounds);
        json += ",\"residual\":" + std::to_string(diagnostics->residual);
        json += ",\"optimality_gap\":";
        if (diagnostics->focused_expansion &&
            std::isfinite(diagnostics->focused_optimality_gap)) {
            char buffer[40];
            std::snprintf(
                buffer, sizeof(buffer), "%.17g",
                diagnostics->focused_optimality_gap);
            json += buffer;
        } else {
            json += "null";
        }
        json += ",\"state_cap_hit\":" +
                std::string(bool_json(diagnostics->state_cap_hit));
        json += ",\"resource_cap_hit\":" +
                std::string(bool_json(diagnostics->resource_cap_hit));
        json += ",\"cap_hits\":[";
        for (std::size_t i = 0; i < diagnostics->cap_hits.size(); ++i) {
            if (i != 0) json += ',';
            json += "\"" + diagnostics->cap_hits[i] + "\"";
        }
        json += "]";
        json += ",\"full_request_status\":\"";
        if (diagnostics->state_cap_hit) {
            json += "incomplete_state_cap";
        } else if (diagnostics->resource_cap_hit) {
            json += "incomplete_resource_cap";
        } else if (!result->converged) {
            json += "incomplete_solve";
        } else if (qualified_action_subset) {
            json += "incomplete_action_subset";
        } else {
            json += "exact_abstract_within_tolerance";
        }
        json += "\"";
    }
    json += "}";

    json += ",\"timings_ns\":{\"registry_generation\":" +
            optional_count(registry_generation_ns);
    json += ",\"abstract_layout\":" +
            std::to_string(calc.layout_build_ns());
    if (diagnostics == nullptr) {
        json += ",\"solve_setup\":null,\"expansion\":null";
        json += ",\"expansion_phases\":null";
        json += ",\"transition_calculation\":null,\"optimization\":null";
        json += ",\"extraction\":null";
    } else {
        json += ",\"solve_setup\":" +
                std::to_string(diagnostics->solve_setup_ns);
        json += ",\"expansion\":" +
                std::to_string(diagnostics->expansion_ns);
        const std::uint64_t expansion_attributed_ns =
            diagnostics->expansion_prepare_ns +
            diagnostics->expansion_kernel_ns +
            diagnostics->expansion_sparse_row_ns +
            diagnostics->expansion_row_byte_audit_ns +
            diagnostics->expansion_diagnostics_ns +
            diagnostics->expansion_release_ns +
            diagnostics->expansion_cap_byte_audit_ns +
            diagnostics->expansion_finalize_ns;
        const std::uint64_t expansion_unattributed_ns =
            diagnostics->expansion_ns > expansion_attributed_ns
                ? diagnostics->expansion_ns - expansion_attributed_ns
                : 0;
        json += ",\"expansion_phases\":{";
        json += "\"prepare\":" +
                std::to_string(diagnostics->expansion_prepare_ns);
        json += ",\"prepare_detail\":{";
        json += "\"byte_audit\":" + std::to_string(
            diagnostics->expansion_prepare_byte_audit_ns);
        json += ",\"automatic_admission\":" + std::to_string(
            diagnostics->expansion_prepare_admission_ns);
        json += ",\"diagnostics\":" + std::to_string(
            diagnostics->expansion_prepare_diagnostics_ns);
        json += ",\"operator_pricing\":" + std::to_string(
            diagnostics->expansion_prepare_pricing_ns) + "}";
        json += ",\"kernel\":" +
                std::to_string(diagnostics->expansion_kernel_ns);
        json += ",\"sparse_row\":" +
                std::to_string(diagnostics->expansion_sparse_row_ns);
        json += ",\"row_byte_audit\":" + std::to_string(
            diagnostics->expansion_row_byte_audit_ns);
        json += ",\"diagnostics\":" +
                std::to_string(diagnostics->expansion_diagnostics_ns);
        json += ",\"cache_release\":" +
                std::to_string(diagnostics->expansion_release_ns);
        json += ",\"periodic_cap_byte_audit\":" + std::to_string(
            diagnostics->expansion_cap_byte_audit_ns);
        json += ",\"finalize\":" +
                std::to_string(diagnostics->expansion_finalize_ns);
        json += ",\"finalize_byte_audit\":" + std::to_string(
            diagnostics->expansion_finalize_byte_audit_ns);
        json += ",\"attributed\":" +
                std::to_string(expansion_attributed_ns);
        json += ",\"unattributed\":" +
                std::to_string(expansion_unattributed_ns) + "}";
        json += ",\"transition_calculation\":" +
                std::to_string(cache.distribution_build_ns);
        json += ",\"optimization\":" +
                std::to_string(diagnostics->optimization_ns);
        if (result == nullptr) {
            json += ",\"extraction\":null";
        } else {
            json += ",\"extraction\":" +
                    std::to_string(diagnostics->extraction_ns);
        }
    }
    json += ",\"compilation\":" +
            count_or_null(compilation == nullptr ? nullptr
                                                 : &compilation->duration_ns);
    json += ",\"verification\":null}";

    json += ",\"diagnostic_cost\":{";
    json += "\"calc_owned_byte_audit_requests\":" +
            std::to_string(cache.owned_byte_audit_requests);
    json += ",\"calc_owned_byte_audit_ns\":" +
            std::to_string(cache.owned_byte_audit_ns);
    json += ",\"calc_owned_byte_ledger_requests\":" +
            std::to_string(cache.owned_byte_ledger_requests);
    json += ",\"calc_owned_byte_ledger_ns\":" +
            std::to_string(cache.owned_byte_ledger_ns);
    json += ",\"calc_owned_byte_reconciliations\":" +
            std::to_string(cache.owned_byte_reconciliations);
    json += ",\"calc_owned_byte_ledger_max_overestimate\":" +
            std::to_string(
                cache.owned_byte_ledger_max_overestimate) + "}";

    const std::uint64_t current_bytes = calc.estimated_owned_bytes();
    json += ",\"memory\":{\"solver_owned_bytes_estimate\":" +
            std::to_string(diagnostics == nullptr
                               ? current_bytes
                               : diagnostics->solver_owned_bytes_estimate);
    json += ",\"solver_live_owned_bytes_estimate\":" +
            std::to_string(diagnostics == nullptr
                               ? current_bytes
                               : diagnostics->solver_live_owned_bytes_estimate);
    json += ",\"diagnostics_retained_bytes_estimate\":" +
            std::to_string(
                diagnostics == nullptr
                    ? 0
                    : diagnostics->diagnostics_retained_bytes_estimate);
    json += ",\"telemetry_json_byte_limit\":" +
            std::to_string(
                diagnostics == nullptr
                    ? SolveOptions{}.max_telemetry_json_bytes
                    : diagnostics->telemetry_json_byte_limit);
    json += ",\"estimate_kind\":\"selected_allocations_not_process_heap\"";
    json += ",\"abstract_state_bytes\":" +
            std::to_string(sizeof(AbstractState));
    json += ",\"state_payload\":\"inline_sparse_junk_counts\"}";

    json += ",\"compilation\":{";
    if (compilation == nullptr) {
        json += "\"available\":false,\"working_states\":null";
        json += ",\"policy_regions\":null";
        json += ",\"nodes\":null,\"edges\":null";
        json += ",\"strategy_json_bytes\":null";
        json += ",\"cap_hit\":null";
    } else {
        json += "\"available\":true,\"working_states\":" +
                std::to_string(compilation->working_states);
        json += ",\"policy_regions\":" +
                std::to_string(compilation->policy_regions);
        json += ",\"nodes\":" + std::to_string(compilation->nodes);
        json += ",\"edges\":" + std::to_string(compilation->edges);
        json += ",\"strategy_json_bytes\":" +
                std::to_string(compilation->strategy_json_bytes);
        json += ",\"cap_hit\":";
        if (compilation->cap_hit.empty()) {
            json += "null";
        } else {
            json += "\"" + compilation->cap_hit + "\"";
        }
    }
    json += "}";

    const bool has_result_bound =
        result != nullptr && result->start_state < result->values.size() &&
        std::isfinite(result->values[result->start_state]);
    const bool has_snapshot_bound =
        snapshot != nullptr && std::isfinite(snapshot->raw_start_bound);
    json += ",\"value\":{\"start\":";
    if (!has_result_bound || !result->converged) {
        json += "null";
    } else {
        char buffer[40];
        std::snprintf(buffer, sizeof(buffer), "%.17g",
                      result->values[result->start_state]);
        json += buffer;
    }
    json += ",\"start_status\":";
    if (result == nullptr && snapshot == nullptr) {
        json += "\"not_run\"";
    } else if (result == nullptr) {
        json += snapshot->abandoned ? "\"abandoned_before_completion\""
                                    : "\"solve_in_progress\"";
    } else if (result->converged) {
        json += qualified_action_subset
                    ? "\"exact_supported_priced_subset_within_tolerance\""
                    : "\"exact_abstract_within_tolerance\"";
    } else {
        json += "\"unavailable_incomplete_solve\"";
    }
    json += ",\"start_scope\":";
    if (result == nullptr || !result->converged) {
        json += "null";
    } else {
        json += qualified_action_subset ? "\"supported_priced_subset\""
                                        : "\"full_requested_action_set\"";
    }
    json += ",\"raw_start_bound\":";
    if (has_result_bound) {
        char buffer[40];
        std::snprintf(buffer, sizeof(buffer), "%.17g",
                      result->values[result->start_state]);
        json += buffer;
    } else if (has_snapshot_bound) {
        char buffer[40];
        std::snprintf(buffer, sizeof(buffer), "%.17g",
                      snapshot->raw_start_bound);
        json += buffer;
    } else {
        json += "null";
    }
    json += "},\"verification\":null}";
    return std::move(json).take();
}

} // namespace solver
} // namespace poecraft
