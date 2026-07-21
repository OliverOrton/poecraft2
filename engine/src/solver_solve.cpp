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
    std::uint32_t variant_capacity = 0;
    std::uint64_t transition_offset = 0;
    std::uint32_t transition_count = 0;
    double self_probability = 0.0;
    double embedded_self_probability = 0.0;
    bool self_probability_embedded = false;
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
    std::optional<std::size_t> exact_kernel_hash;
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

template <typename T>
std::size_t selected_growth_capacity(
    const std::vector<T>& values, const std::size_t additional) {
    if (additional > values.max_size() - values.size()) {
        throw std::length_error("selected solver allocation overflow");
    }
    const std::size_t required = values.size() + additional;
    if (required <= values.capacity()) return values.capacity();
    const std::size_t capacity = values.capacity();
    const std::size_t increment = std::max<std::size_t>(capacity / 4, 64);
    const std::size_t grown =
        capacity > values.max_size() - increment
            ? values.max_size()
            : capacity + increment;
    return std::max(required, grown);
}

struct SparseVariantArena {
    std::vector<SparseVariant> variants;
    std::vector<std::uint32_t> row_variant_indices;
    std::vector<double> variant_quantities;

    std::uint64_t selected_bytes() const {
        return sizeof(*this) +
               variants.capacity() * sizeof(SparseVariant) +
               row_variant_indices.capacity() * sizeof(std::uint32_t) +
               variant_quantities.capacity() * sizeof(double);
    }
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
    case AutomaticCandidateKind::ConstructiveRenewal:
        return AutomaticTelemetryKind::Renewal;
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
        const SolveOptions& options) const {
        if (start_state != requested_start ||
            max_states != options.max_states ||
            max_discovered_states != options.max_discovered_states ||
            max_expanded_states != options.max_expanded_states ||
            max_state_action_rows != options.max_state_action_rows ||
            max_transitions != options.max_transitions ||
            max_reforge_work != options.max_reforge_work ||
            max_solver_owned_bytes != options.max_solver_owned_bytes ||
            max_diagnostic_samples != options.max_diagnostic_samples ||
            full_evidence != options.full_evidence ||
            kernel_reuse != options.kernel_reuse ||
            exact_quotient == options.strict_states ||
            operator_indices.size() != priced.size()) {
            return false;
        }
        for (std::size_t i = 0; i < priced.size(); ++i) {
            if (operator_indices[i] != priced[i].index) return false;
        }
        return true;
    }

    static std::uint64_t automatic_sample_nested_bytes(
        const AutomaticCandidateRecord& record) {
        return record.evidence.legality_result.capacity() +
               record.evidence.reason.capacity() +
               record.candidate_id.capacity() +
               record.setup_action_id.capacity() +
               record.followup_action_id.capacity() +
               record.cleanup_action_id.capacity();
    }

    void retain_automatic_sample(AutomaticCandidateRecord record) {
        owned_automatic_sample_nested_bytes +=
            automatic_sample_nested_bytes(record);
        automatic_candidate_samples.push_back(std::move(record));
    }

    std::uint64_t shallow_estimated_owned_bytes() const {
        std::uint64_t bytes = sizeof(*this);
        bytes += operator_indices.capacity() * sizeof(std::uint32_t);
        bytes += expanded.capacity() * sizeof(std::uint8_t);
        bytes += behavioral_representative_by_state.capacity() *
                 sizeof(std::uint32_t);
        bytes += state_rows.capacity() * sizeof(StateRowSpan);
        bytes += rows.capacity() * sizeof(SparseRow);
        if (accounts_variant_arena && variant_arena != nullptr) {
            bytes += variant_arena->selected_bytes();
        }
        bytes += successors.capacity() * sizeof(std::uint32_t);
        bytes += probabilities.capacity() * sizeof(double);
        bytes += choices.capacity() * sizeof(SparseChoiceGroup);
        bytes += choice_successors.capacity() * sizeof(std::uint32_t);
        bytes += choice_options.capacity() * sizeof(OutcomeChoiceOption);
        bytes += automatic_candidate_samples.capacity() *
                 sizeof(AutomaticCandidateRecord);
        return bytes;
    }

    std::uint64_t fast_estimated_owned_bytes() const {
        return shallow_estimated_owned_bytes() +
               owned_automatic_sample_nested_bytes;
    }

    std::uint64_t audited_estimated_owned_bytes() const {
        std::uint64_t bytes = shallow_estimated_owned_bytes();
        for (const AutomaticCandidateRecord& record :
             automatic_candidate_samples) {
            bytes += automatic_sample_nested_bytes(record);
        }
        return bytes;
    }

    std::uint64_t estimated_owned_bytes() const {
        return audited_estimated_owned_bytes();
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
               diagnostics.constructive_state_witnesses) +
           string_vector_owned_bytes(
               diagnostics.automatic_candidate_witnesses) +
           string_vector_owned_bytes(diagnostics.equivalence_witnesses) +
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
    bytes += result.behavioral_representative_by_state.capacity() *
             sizeof(std::uint32_t);
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
        std::uint32_t anchor_state = kNoId;
        double anchor_state_value = kInfinity;
        double renewal_state_value = kInfinity;
        std::uint64_t renewal_row =
            std::numeric_limits<std::uint64_t>::max();
        std::uint32_t renewal_operator = kNoId;
        std::uint32_t finish_action = kNoId;
        std::uint8_t renewal_rarity = PC_RARITY_NORMAL;
        std::uint8_t renewal_influence_bits = 0;
        std::uint8_t renewal_searing_exarch_tier = 0;
        std::uint8_t renewal_eater_of_worlds_tier = 0;
        /* Exact policy-selected magic acquisition -> Regal -> deterministic
         * finish terminals. These are ordinary primitive operators on strict
         * states; the map is an executable fallback witness, not a quotient. */
        std::unordered_map<std::uint32_t, double> progress_state_value;
        std::unordered_map<std::uint32_t, std::uint32_t>
            progress_state_operator;
    };
    std::optional<FocusedFallbackPolicy> focused_fallback_policy;
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
        const PricedOperator& priced) {
        std::uint64_t bytes = priced.resource_prices.capacity() *
                              sizeof(std::pair<std::string, double>);
        for (const auto& [key, unused_price] : priced.resource_prices) {
            (void)unused_price;
            bytes += key.capacity() + 1;
        }
        return bytes;
    }

    void initialize_owned_bytes_ledger() {
        for (const auto& [key, unused_price] : prices) {
            (void)unused_price;
            owned_prices_nested_bytes += key.capacity() + 1;
        }
        for (const PricedOperator& priced : operators) {
            owned_operators_nested_bytes +=
                priced_operator_nested_bytes(priced);
        }
    }

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
        const bool has_constructive_renewal = std::any_of(
            operators.begin(), operators.end(),
            [&](const PricedOperator& priced) {
                return calc.operators().at(priced.index).automatic_kind ==
                       AutomaticCandidateKind::ConstructiveRenewal;
            });
        next_focus_checkpoint = has_constructive_renewal
                                    ? 1
                                    : std::max<std::uint32_t>(
                                          1,
                                          options.focused_expansion_checkpoint);
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
            result.behavioral_representative_by_state =
                transition_cache->behavioral_representative_by_state;
            focused_mode = transition_cache->focused_partial;
            cache_pending = !focused_mode;
            result.diagnostics.transition_cache_reused = true;
        } else {
            transition_cache = std::make_shared<SolveTransitionCache>();
            transition_cache->exact_quotient = !options.strict_states;
            transition_cache->start_state = result.start_state;
            transition_cache->max_states = options.max_states;
            transition_cache->max_discovered_states =
                options.max_discovered_states;
            transition_cache->max_expanded_states =
                options.max_expanded_states;
            transition_cache->max_state_action_rows =
                options.max_state_action_rows;
            transition_cache->max_transitions = options.max_transitions;
            transition_cache->max_reforge_work = options.max_reforge_work;
            transition_cache->max_solver_owned_bytes =
                options.max_solver_owned_bytes;
            transition_cache->max_diagnostic_samples =
                options.max_diagnostic_samples;
            transition_cache->full_evidence = options.full_evidence;
            transition_cache->kernel_reuse = options.kernel_reuse;
            for (const PricedOperator& priced : operators) {
                transition_cache->operator_indices.push_back(priced.index);
            }
            enqueue(result.start_state);
        }
        result.diagnostics.solve_setup_ns = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - setup_started)
                .count());
        initialize_owned_bytes_ledger();
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
        owned_operators_nested_bytes +=
            priced_operator_nested_bytes(operators.back());
        transition_cache->operator_indices.push_back(index);
        ++result.diagnostics.priced_scanned_actions;
        ++result.diagnostics.supported_priced_actions;
        return true;
    }

    std::uint32_t action_goal_reach_mask(
        const std::uint32_t action_index) const {
        if (action_index == kNoId ||
            action_index >= calc.registry().actions.size()) {
            return 0;
        }
        const std::vector<std::uint64_t> reachable =
            action_explicit_affix_reachable_mask(
                session, calc.registry().actions.at(action_index));
        std::uint32_t mask = 0;
        for (std::uint32_t slot = 0;
             slot < calc.layout().slots.size(); ++slot) {
            const std::vector<std::uint64_t>& satisfying =
                calc.layout().slots.at(slot).satisfying_mask;
            bool intersects = false;
            for (std::size_t word = 0;
                 word < reachable.size() && word < satisfying.size();
                 ++word) {
                intersects |= (reachable[word] & satisfying[word]) != 0;
            }
            if (intersects) mask |= 1u << slot;
        }
        return mask;
    }

    std::uint32_t planner_goal_reach_mask(
        const std::uint32_t operator_index) {
        if (operator_index >= operator_goal_reach_mask.size()) {
            operator_goal_reach_mask.resize(operator_index + 1, 0);
            operator_goal_reach_computed.resize(operator_index + 1, 0);
        }
        if (operator_goal_reach_computed[operator_index]) {
            return operator_goal_reach_mask[operator_index];
        }
        const PlannerOperator& planner =
            calc.operators().at(operator_index);
        std::uint32_t mask = 0;
        const auto include = [&](const std::uint32_t action) {
            mask |= action_goal_reach_mask(action);
        };
        if (planner.kind == PlannerOperatorKind::Primitive) {
            include(planner.primitive_action);
        } else {
            for (const std::uint32_t action : planner.primitive_program) {
                include(action);
            }
            include(planner.conditional_action);
            include(planner.setup_action);
            include(planner.followup_action);
            include(planner.cleanup_action);
        }
        operator_goal_reach_mask[operator_index] = mask;
        operator_goal_reach_computed[operator_index] = 1;
        return mask;
    }

    void prepare_goal_cover_cost() {
        if (goal_cover_cost_ready) return;
        goal_cover_cost_ready = true;
        const std::size_t slot_count = calc.layout().slots.size();
        const std::size_t mask_count = std::size_t{1} << slot_count;
        goal_cover_cost.assign(mask_count, kInfinity);
        goal_cover_cost[0] = 0.0;
        clean_goal_cover_cost.assign(mask_count, kInfinity);
        clean_goal_cover_cost[0] = 0.0;
        std::vector<std::uint32_t> cover_predecessor(mask_count, kNoId);
        std::vector<std::uint32_t> cover_action(mask_count, kNoId);
        std::vector<std::uint32_t> cover_subset(mask_count, 0);
        /* Probability-aware optimistic cover. Every stochastic primitive is
         * replaced by a stronger macro that retries for a requested nonempty
         * goal subset, preserves all prior progress, receives the best legal
         * junk blockers that can still leave the requested affix slots open,
         * and pays c / p_upper. A real strategy can only have lower success
         * probability or lose more carrier state, so this relaxed acyclic MDP
         * remains an admissible lower bound. Unknown transition families keep
         * the former p=1 set-cover behavior. */
        std::vector<std::int8_t> slot_side(slot_count, -1);
        for (std::uint32_t slot = 0; slot < slot_count; ++slot) {
            for (std::uint32_t mod = 0; mod < session.mod_count; ++mod) {
                if (pc_bitset_test(
                        calc.layout().slots[slot].satisfying_mask.data(),
                        mod)) {
                    slot_side[slot] = session.gen_type[mod];
                    break;
                }
            }
        }
        using DrawKey = std::tuple<
            std::uint32_t, std::uint32_t, std::uint32_t,
            std::uint8_t, std::uint8_t, bool>;
        std::map<DrawKey, double> draw_probability;
        const auto draw_upper = [&] (
            const std::uint32_t action,
            const std::uint32_t slot,
            const std::uint32_t satisfied,
            const std::uint8_t prefix_blockers,
            const std::uint8_t suffix_blockers,
            const bool guaranteed) {
            const DrawKey key{
                action, slot, satisfied, prefix_blockers,
                suffix_blockers, guaranteed};
            const auto found = draw_probability.find(key);
            if (found != draw_probability.end()) return found->second;
            const double probability =
                calc.optimistic_goal_draw_probability(
                    result.start_state, action, slot, satisfied,
                    prefix_blockers, suffix_blockers, guaranteed);
            draw_probability.emplace(key, probability);
            return probability;
        };
        const auto priced_action_cost = [&](const ActionDescriptor& action) {
            double cost = 0.0;
            for (const std::string& key : action.cost_keys) {
                const auto found = prices.find(key);
                if (found == prices.end() ||
                    !std::isfinite(found->second) || found->second < 0.0) {
                    return kInfinity;
                }
                cost += found->second;
            }
            return cost;
        };
        const auto probabilistic_shape = [](const ActionType type) {
            /* first: maximum prefix/suffix draws; second: one total draw. */
            switch (type) {
            case ActionType::Transmute:
            case ActionType::Alteration:
                return std::pair<std::uint8_t, bool>{1, false};
            case ActionType::Alchemy:
            case ActionType::Chaos:
            case ActionType::Fossil:
            case ActionType::HarvestReforge:
                return std::pair<std::uint8_t, bool>{3, false};
            case ActionType::Augment:
            case ActionType::Regal:
            case ActionType::Exalt:
            case ActionType::HarvestAugment:
                return std::pair<std::uint8_t, bool>{1, true};
            default:
                return std::pair<std::uint8_t, bool>{0, false};
            }
        };
        const auto subset_probability = [&](const std::uint32_t action,
                                             const std::uint32_t existing,
                                             const std::uint32_t subset,
                                             const std::uint8_t known_prefix_blockers,
                                             const std::uint8_t known_suffix_blockers) {
            const ActionDescriptor& descriptor =
                calc.registry().actions.at(action);
            const auto [draws_per_side, one_total_draw] =
                probabilistic_shape(descriptor.params.type);
            if (draws_per_side == 0) return 1.0;
            const std::uint32_t subset_count = std::popcount(subset);
            if (one_total_draw && subset_count > 1) return 0.0;

            std::array<std::vector<std::uint32_t>, 2> by_side;
            for (std::uint32_t slot = 0; slot < slot_count; ++slot) {
                const std::int8_t side = slot_side[slot];
                if (side != PC_SIDE_PREFIX && side != PC_SIDE_SUFFIX) {
                    if ((subset & (1u << slot)) != 0) return 0.0;
                    continue;
                }
                if ((subset & (1u << slot)) != 0) {
                    by_side[side].push_back(slot);
                }
            }
            if (by_side[0].size() > draws_per_side ||
                by_side[1].size() > draws_per_side) {
                return 0.0;
            }
            const bool destructive_renewal =
                descriptor.params.type == ActionType::Transmute ||
                descriptor.params.type == ActionType::Alteration ||
                descriptor.params.type == ActionType::Alchemy ||
                descriptor.params.type == ActionType::Chaos ||
                descriptor.params.type == ActionType::Fossil ||
                descriptor.params.type == ActionType::HarvestReforge;
            /* A clean unprotected carrier cannot carry junk blockers through
             * a destructive renewal. The relaxed action may still preserve
             * already-satisfied goal slots, which is strictly more favorable
             * than the real renewal, but its fresh roll uses the clean pool.
             * Additive actions retain the stronger free-blocker relaxation. */
            const std::uint8_t prefix_blockers = destructive_renewal
                ? 0
                : known_prefix_blockers;
            const std::uint8_t suffix_blockers = destructive_renewal
                ? 0
                : known_suffix_blockers;
            double probability = 1.0;
            for (std::size_t side = 0; side < by_side.size(); ++side) {
                auto slots = by_side[side];
                if (slots.empty()) continue;
                std::sort(slots.begin(), slots.end());
                double best_order = 0.0;
                do {
                    double order = 1.0;
                    std::uint32_t acquired = existing;
                    for (const std::uint32_t slot : slots) {
                        double p = draw_upper(
                            action, slot, acquired, prefix_blockers,
                            suffix_blockers, false);
                        if (descriptor.params.type ==
                                ActionType::HarvestReforge ||
                            descriptor.params.type ==
                                ActionType::HarvestAugment) {
                            p = std::max(
                                p,
                                draw_upper(
                                    action, slot, acquired,
                                    prefix_blockers, suffix_blockers, true));
                        }
                        order *= p;
                        acquired |= 1u << slot;
                    }
                    best_order = std::max(best_order, order);
                } while (std::next_permutation(slots.begin(), slots.end()));
                double placements = 1.0;
                for (std::size_t i = 0; i < slots.size(); ++i) {
                    placements *= static_cast<double>(draws_per_side - i);
                }
                probability *= std::min(1.0, placements * best_order);
            }
            return std::min(1.0, probability);
        };

        std::vector<std::uint32_t> relaxation_actions = calc.candidates();
        const auto include_action = [&](const std::uint32_t action) {
            if (action != kNoId && action < calc.registry().actions.size() &&
                std::find(
                    relaxation_actions.begin(), relaxation_actions.end(),
                    action) == relaxation_actions.end()) {
                relaxation_actions.push_back(action);
            }
        };
        for (const std::uint32_t operator_index :
             calc.candidate_operators()) {
            const PlannerOperator& planner =
                calc.operators().at(operator_index);
            include_action(planner.primitive_action);
            for (const std::uint32_t action : planner.primitive_program) {
                include_action(action);
            }
            include_action(planner.conditional_action);
            include_action(planner.setup_action);
            include_action(planner.followup_action);
            include_action(planner.cleanup_action);
        }
        for (const std::uint32_t action :
             calc.automatic_goal_bench_actions()) {
            include_action(action);
        }
        for (const TemporaryBenchEffectClass& effect :
             calc.temporary_bench_effect_classes()) {
            include_action(effect.followup_action);
            for (const std::uint32_t action : effect.blocker_actions) {
                include_action(action);
            }
        }

        /* Keep a probability-free cover as the universal proof used for
         * price-bound action pruning and for carriers whose preserved
         * structure can change the pool. It gives every action any reachable
         * goal subset deterministically for one immediate price. */
        const auto relax_cover = [&] (
            std::vector<double>& cover,
            const bool probability_aware) {
            for (std::uint32_t mask = 0; mask < mask_count; ++mask) {
                if (!std::isfinite(cover[mask])) continue;
                for (const std::uint32_t action : relaxation_actions) {
                    const ActionDescriptor& descriptor =
                        calc.registry().actions.at(action);
                    const double cost = priced_action_cost(descriptor);
                    if (!std::isfinite(cost) || cost < 0.0) continue;
                    const std::uint32_t missing_reach =
                        action_goal_reach_mask(action) & ~mask;
                    for (std::uint32_t subset = missing_reach;
                         subset != 0;
                         subset = (subset - 1) & missing_reach) {
                        const double probability = probability_aware
                            ? subset_probability(
                                  action, mask, subset, 0, 0)
                            : 1.0;
                        if (!(probability > 0.0) ||
                            !std::isfinite(probability)) {
                            continue;
                        }
                        const std::uint32_t produced = mask | subset;
                        const double candidate =
                            cover[mask] + cost / probability;
                        if (candidate < cover[produced]) {
                            cover[produced] = candidate;
                            if (probability_aware) {
                                cover_predecessor[produced] = mask;
                                cover_action[produced] = action;
                                cover_subset[produced] = subset;
                            }
                        }
                    }
                }
            }
        };
        relax_cover(goal_cover_cost, false);
        (void)cover_predecessor;
        (void)cover_action;
        (void)cover_subset;
        if (slot_count < 2) {
            clean_goal_cover_cost.clear();
            clean_goal_escape_cost.clear();
            clean_goal_escape_action.clear();
            clean_goal_no_exalt_escape_cost.clear();
            clean_goal_no_exalt_escape_action.clear();
            return;
        }

        /* Goal-progress/rarity relaxation for clean carriers. It is a real
         * finite MDP rather than an acyclic set cover: destructive rolls
         * replace the prior goal subset, their zero-target outcomes land at
         * the action's output rarity with an empty subset, and Restart/Scour
         * must return through normal rarity. Outcome identities are made
         * optimistically clairvoyant and cumulative probabilities are union
         * bounds, so this MDP can only be easier than the exact item process. */
        constexpr std::uint32_t kRarityCount = 3;
        constexpr std::uint32_t kAffixCountStates = 4;
        const auto abstract_index = [&](const std::uint8_t rarity,
                                        const std::uint32_t mask,
                                        const std::uint8_t prefixes,
                                        const std::uint8_t suffixes) {
            return (((static_cast<std::size_t>(rarity) * mask_count + mask) *
                      kAffixCountStates + prefixes) *
                     kAffixCountStates + suffixes);
        };
        clean_goal_cover_cost.assign(
            kRarityCount * mask_count * kAffixCountStates *
                kAffixCountStates,
            0.0);
        clean_goal_escape_cost.assign(
            clean_goal_cover_cost.size(), kInfinity);
        clean_goal_escape_action.assign(
            clean_goal_cover_cost.size(), kNoId);
        clean_goal_no_exalt_escape_cost.assign(
            clean_goal_cover_cost.size(), kInfinity);
        clean_goal_no_exalt_escape_action.assign(
            clean_goal_cover_cost.size(), kNoId);
        std::vector<std::uint32_t> clean_goal_policy(
            clean_goal_cover_cost.size(), kNoId);
        const std::uint32_t required =
            calc.goal().required_satisfied_slots();
        const auto is_abstract_goal = [&](const std::uint8_t rarity,
                                          const std::uint32_t mask) {
            return rarity == calc.goal().rarity &&
                   std::popcount(mask) >= required;
        };
        const auto output_rarity = [](const ActionType type,
                                      const std::uint8_t input) {
            switch (type) {
            case ActionType::Transmute:
            case ActionType::Alteration:
                return static_cast<std::uint8_t>(PC_RARITY_MAGIC);
            case ActionType::Alchemy:
            case ActionType::Chaos:
            case ActionType::Essence:
            case ActionType::Fossil:
            case ActionType::HarvestReforge:
            case ActionType::Regal:
                return static_cast<std::uint8_t>(PC_RARITY_RARE);
            case ActionType::Scour:
                return static_cast<std::uint8_t>(PC_RARITY_NORMAL);
            default:
                return input;
            }
        };
        const auto is_destructive = [](const ActionType type) {
            return type == ActionType::Transmute ||
                   type == ActionType::Alteration ||
                   type == ActionType::Alchemy ||
                   type == ActionType::Chaos ||
                   type == ActionType::Essence ||
                   type == ActionType::Fossil ||
                   type == ActionType::HarvestReforge;
        };
        std::vector<std::array<std::uint8_t, 2>> minimum_goal_affixes(
            mask_count,
            {std::numeric_limits<std::uint8_t>::max(),
             std::numeric_limits<std::uint8_t>::max()});
        minimum_goal_affixes[0] = {0, 0};
        for (std::size_t side = 0; side < 2; ++side) {
            std::vector<std::uint8_t> minimum(
                mask_count, std::numeric_limits<std::uint8_t>::max());
            minimum[0] = 0;
            for (std::uint32_t covered = 0; covered < mask_count; ++covered) {
                if (minimum[covered] ==
                    std::numeric_limits<std::uint8_t>::max()) {
                    continue;
                }
                for (std::uint32_t mod = 0;
                     mod < session.mod_count; ++mod) {
                    if (session.gen_type[mod] != side) continue;
                    std::uint32_t mod_mask = 0;
                    for (std::uint32_t slot = 0; slot < slot_count; ++slot) {
                        if (slot_side[slot] == side &&
                            pc_bitset_test(
                                calc.layout().slots[slot]
                                    .satisfying_mask.data(),
                                mod)) {
                            mod_mask |= 1u << slot;
                        }
                    }
                    if (mod_mask == 0) continue;
                    const std::uint32_t produced = covered | mod_mask;
                    minimum[produced] = std::min<std::uint8_t>(
                        minimum[produced],
                        static_cast<std::uint8_t>(minimum[covered] + 1));
                }
            }
            for (std::uint32_t mask = 0; mask < mask_count; ++mask) {
                const std::uint32_t side_mask = [&] {
                    std::uint32_t value = 0;
                    for (std::uint32_t slot = 0; slot < slot_count; ++slot) {
                        if (slot_side[slot] == side &&
                            (mask & (1u << slot)) != 0) {
                            value |= 1u << slot;
                        }
                    }
                    return value;
                }();
                minimum_goal_affixes[mask][side] = minimum[side_mask];
            }
        }
        std::unordered_map<std::uint32_t, std::size_t>
            relaxation_action_position;
        for (std::size_t i = 0; i < relaxation_actions.size(); ++i) {
            relaxation_action_position.emplace(relaxation_actions[i], i);
        }
        std::vector<double> subset_probability_cache(
            relaxation_actions.size() * mask_count * mask_count *
                kAffixCountStates * kAffixCountStates,
            -1.0);
        const auto cached_subset_probability = [&] (
            const std::uint32_t action,
            const std::uint32_t existing,
            const std::uint32_t subset,
            const std::uint8_t prefix_blockers,
            const std::uint8_t suffix_blockers) {
            const std::size_t action_position =
                relaxation_action_position.at(action);
            const std::size_t index =
                ((((action_position * mask_count + existing) * mask_count +
                    subset) * kAffixCountStates + prefix_blockers) *
                  kAffixCountStates + suffix_blockers);
            double& cached = subset_probability_cache[index];
            if (cached < 0.0) {
                cached = subset_probability(
                    action, existing, subset,
                    prefix_blockers, suffix_blockers);
            }
            return cached;
        };
        struct RelaxedStochasticEnvelope {
            bool ready = false;
            double failure_probability = 1.0;
            std::vector<std::size_t> failure_successors;
            std::vector<double> success_probability;
            std::vector<std::vector<std::size_t>> success_successors;
        };
        std::vector<RelaxedStochasticEnvelope> stochastic_envelopes(
            clean_goal_cover_cost.size() * relaxation_actions.size());
        struct ExactRelaxedEntry {
            std::size_t successor = 0;
            double probability = 0.0;
        };
        std::unordered_map<std::uint32_t, std::vector<ExactRelaxedEntry>>
            exact_destructive_envelopes;
        const AbstractState& probability_anchor =
            calc.state(result.start_state);
        for (const std::uint32_t action : relaxation_actions) {
            const ActionDescriptor& descriptor =
                calc.registry().actions.at(action);
            const auto [draws, unused_one_total] =
                probabilistic_shape(descriptor.params.type);
            (void)unused_one_total;
            if (draws == 0 || !is_destructive(descriptor.params.type)) {
                continue;
            }
            std::uint32_t carrier = kNoId;
            for (std::uint32_t state = 0; state < calc.state_count(); ++state) {
                const AbstractState& candidate = calc.state(state);
                if (candidate.influence_bits !=
                        probability_anchor.influence_bits ||
                    candidate.searing_exarch_tier !=
                        probability_anchor.searing_exarch_tier ||
                    candidate.eater_of_worlds_tier !=
                        probability_anchor.eater_of_worlds_tier ||
                    candidate.fractured_goal_mask != 0 ||
                    candidate.fractured_metamod_flags != 0 ||
                    (candidate.flags & kProtectionFlags) != 0 ||
                    !action_legal(session, descriptor, candidate)) {
                    continue;
                }
                bool fractured_junk = false;
                for (const std::uint8_t count :
                     candidate.fractured_junk_counts) {
                    fractured_junk |= count != 0;
                }
                if (fractured_junk) continue;
                carrier = state;
                break;
            }
            if (carrier == kNoId) continue;
            const OutcomeDistribution& distribution =
                calc.outcomes(carrier, action);
            if (!distribution.supported ||
                !distribution.choice_groups.empty() ||
                !distribution.choice_options.empty()) {
                continue;
            }
            std::map<std::size_t, double> aggregated;
            for (const OutcomeEntry& outcome : distribution.entries) {
                const AbstractState& successor = calc.state(outcome.state);
                if (successor.rarity > PC_RARITY_RARE ||
                    successor.prefix_count >= kAffixCountStates ||
                    successor.suffix_count >= kAffixCountStates) {
                    continue;
                }
                aggregated[abstract_index(
                    successor.rarity,
                    satisfied_goal_mask_for_state(outcome.state),
                    successor.prefix_count,
                    successor.suffix_count)] += outcome.probability;
            }
            if (aggregated.empty()) continue;
            auto& stored = exact_destructive_envelopes[action];
            stored.reserve(aggregated.size());
            for (const auto& [successor, probability] : aggregated) {
                stored.push_back({successor, probability});
            }
        }
        constexpr std::uint32_t kRelaxationSweeps = 2048;
        std::uint32_t relaxation_sweeps = 0;
        double relaxation_delta = kInfinity;
        for (std::uint32_t sweep = 0; sweep < kRelaxationSweeps; ++sweep) {
            relaxation_sweeps = sweep + 1;
            double delta = 0.0;
            for (std::uint8_t rarity = PC_RARITY_NORMAL;
                 rarity <= PC_RARITY_RARE; ++rarity) {
                for (std::uint32_t mask = 0; mask < mask_count; ++mask) {
                    const std::uint8_t affix_cap = rarity_affix_cap(
                        session, rarity);
                    for (std::uint8_t prefixes = 0;
                         prefixes <= affix_cap; ++prefixes) {
                        for (std::uint8_t suffixes = 0;
                             suffixes <= affix_cap; ++suffixes) {
                            const std::size_t current = abstract_index(
                                rarity, mask, prefixes, suffixes);
                            const double previous_current =
                                clean_goal_cover_cost[current];
                            if (is_abstract_goal(rarity, mask)) {
                                clean_goal_cover_cost[current] = 0.0;
                                continue;
                            }
                            double best = kInfinity;
                            std::uint32_t best_action = kNoId;
                            const auto consider = [&](const double candidate,
                                                      const std::uint32_t action) {
                                if (candidate < best) {
                                    best = candidate;
                                    best_action = action;
                                }
                                if (action >= calc.registry().actions.size()) {
                                    return;
                                }
                                const ActionDescriptor& considered =
                                    calc.registry().actions.at(action);
                                const ActionType considered_type =
                                    considered.params.type;
                                const bool destructive_refined =
                                    considered_type == ActionType::Transmute ||
                                    considered_type == ActionType::Alteration ||
                                    considered_type == ActionType::Alchemy ||
                                    considered_type == ActionType::Chaos ||
                                    considered_type == ActionType::Essence ||
                                    considered_type == ActionType::Fossil ||
                                    considered_type ==
                                        ActionType::HarvestReforge;
                                const bool goal_bench_refined =
                                    considered_type == ActionType::Bench &&
                                    (considered.sets_flags &
                                     kProtectionFlags) == 0 &&
                                    action_goal_reach_mask(action) != 0;
                                const bool refined = considered.synthetic ||
                                    destructive_refined ||
                                    considered_type ==
                                        ActionType::Augment ||
                                    considered_type ==
                                        ActionType::Regal ||
                                    considered_type == ActionType::Scour ||
                                    goal_bench_refined;
                                if (!refined) {
                                    if (candidate <
                                        clean_goal_escape_cost[current]) {
                                        clean_goal_escape_cost[current] =
                                            candidate;
                                        clean_goal_escape_action[current] =
                                            action;
                                    }
                                    if (considered_type !=
                                            ActionType::Exalt &&
                                        candidate <
                                            clean_goal_no_exalt_escape_cost[
                                                current]) {
                                        clean_goal_no_exalt_escape_cost[
                                            current] = candidate;
                                        clean_goal_no_exalt_escape_action[
                                            current] = action;
                                    }
                                }
                            };
                            clean_goal_escape_cost[current] = kInfinity;
                            clean_goal_escape_action[current] = kNoId;
                            clean_goal_no_exalt_escape_cost[current] =
                                kInfinity;
                            clean_goal_no_exalt_escape_action[current] =
                                kNoId;
                            for (const std::uint32_t action : relaxation_actions) {
                        const ActionDescriptor& descriptor =
                            calc.registry().actions.at(action);
                        if ((descriptor.legality.rarity_mask &
                             (1u << rarity)) == 0) {
                            continue;
                        }
                        const double cost = priced_action_cost(descriptor);
                        if (!std::isfinite(cost) || cost < 0.0) continue;

                        if (descriptor.synthetic) {
                            const std::size_t successor = abstract_index(
                                PC_RARITY_NORMAL, 0, 0, 0);
                            if (successor != current) {
                                consider(
                                    cost + clean_goal_cover_cost[successor],
                                    action);
                            }
                            continue;
                        }
                        if ((descriptor.sets_flags & kProtectionFlags) != 0 ||
                            descriptor.params.type == ActionType::Fracture) {
                            /* Any route that leaves the clean domain first
                             * pays this action. Grant it the whole goal. */
                            consider(cost, action);
                            continue;
                        }
                        if (descriptor.params.type == ActionType::Scour) {
                            const std::size_t successor = abstract_index(
                                PC_RARITY_NORMAL, 0, 0, 0);
                            if (successor != current) {
                                consider(
                                    cost + clean_goal_cover_cost[successor],
                                    action);
                            }
                            continue;
                        }

                        const std::uint8_t next_rarity = output_rarity(
                            descriptor.params.type, rarity);
                        const std::uint32_t reach =
                            action_goal_reach_mask(action);
                        const auto [draws, one_total_draw] =
                            probabilistic_shape(descriptor.params.type);
                        if (draws == 0) {
                            const std::uint32_t next_mask = mask | reach;
                            std::uint8_t next_prefixes = prefixes;
                            std::uint8_t next_suffixes = suffixes;
                            if (next_mask != mask &&
                                descriptor.params.type == ActionType::Bench &&
                                descriptor.params.mod_id < session.mod_count) {
                                if (session.gen_type[descriptor.params.mod_id] ==
                                    PC_SIDE_PREFIX) {
                                    ++next_prefixes;
                                } else if (
                                    session.gen_type[descriptor.params.mod_id] ==
                                    PC_SIDE_SUFFIX) {
                                    ++next_suffixes;
                                }
                            }
                            const std::uint8_t next_cap = rarity_affix_cap(
                                session, next_rarity);
                            if (next_prefixes > next_cap ||
                                next_suffixes > next_cap) {
                                continue;
                            }
                            const std::size_t successor = abstract_index(
                                next_rarity, next_mask,
                                next_prefixes, next_suffixes);
                            if (successor != current) {
                                consider(
                                    cost + clean_goal_cover_cost[successor],
                                    action);
                            }
                            continue;
                        }

                        const bool destructive =
                            is_destructive(descriptor.params.type);
                        const auto exact_destructive =
                            exact_destructive_envelopes.find(action);
                        if (destructive &&
                            exact_destructive !=
                                exact_destructive_envelopes.end()) {
                            double self_probability = 0.0;
                            double continuation = 0.0;
                            for (const ExactRelaxedEntry& entry :
                                 exact_destructive->second) {
                                if (entry.successor == current) {
                                    self_probability += entry.probability;
                                } else {
                                    continuation += entry.probability *
                                        clean_goal_cover_cost[entry.successor];
                                }
                            }
                            if (self_probability < 1.0) {
                                consider(
                                    (cost + continuation) /
                                        (1.0 - self_probability),
                                    action);
                            }
                            continue;
                        }
                        const std::uint32_t available = destructive
                            ? reach
                            : (reach & ~mask);
                        bool disjoint_single_draw = one_total_draw;
                        for (std::uint32_t left = 0;
                             disjoint_single_draw && left < slot_count; ++left) {
                            if ((available & (1u << left)) == 0) continue;
                            for (std::uint32_t right = left + 1;
                                 right < slot_count; ++right) {
                                if ((available & (1u << right)) == 0) continue;
                                const auto& left_mask =
                                    calc.layout().slots[left].satisfying_mask;
                                const auto& right_mask =
                                    calc.layout().slots[right].satisfying_mask;
                                for (std::size_t word = 0;
                                     word < left_mask.size() &&
                                     word < right_mask.size(); ++word) {
                                    if ((left_mask[word] & right_mask[word]) !=
                                        0) {
                                        disjoint_single_draw = false;
                                        break;
                                    }
                                }
                            }
                        }
                        if (!destructive && disjoint_single_draw &&
                            available != 0) {
                            const std::uint8_t goal_prefixes =
                                minimum_goal_affixes[mask][PC_SIDE_PREFIX];
                            const std::uint8_t goal_suffixes =
                                minimum_goal_affixes[mask][PC_SIDE_SUFFIX];
                            const std::uint8_t prefix_blockers =
                                static_cast<std::uint8_t>(
                                    prefixes > goal_prefixes
                                        ? prefixes - goal_prefixes
                                        : 0);
                            const std::uint8_t suffix_blockers =
                                static_cast<std::uint8_t>(
                                    suffixes > goal_suffixes
                                        ? suffixes - goal_suffixes
                                        : 0);
                            double total_success = 0.0;
                            double continuation = 0.0;
                            double self_probability = 0.0;
                            bool feasible_single_draw = true;
                            for (std::uint32_t slot = 0;
                                 slot < slot_count; ++slot) {
                                const std::uint32_t subset = 1u << slot;
                                if ((available & subset) == 0) continue;
                                const double probability =
                                    cached_subset_probability(
                                        action, mask, subset,
                                        prefix_blockers, suffix_blockers);
                                const std::uint8_t added_prefixes =
                                    minimum_goal_affixes[subset]
                                                        [PC_SIDE_PREFIX];
                                const std::uint8_t added_suffixes =
                                    minimum_goal_affixes[subset]
                                                        [PC_SIDE_SUFFIX];
                                const std::uint8_t next_prefixes =
                                    static_cast<std::uint8_t>(
                                        prefixes + added_prefixes);
                                const std::uint8_t next_suffixes =
                                    static_cast<std::uint8_t>(
                                        suffixes + added_suffixes);
                                const std::uint8_t next_cap = rarity_affix_cap(
                                    session, next_rarity);
                                if (next_prefixes > next_cap ||
                                    next_suffixes > next_cap) {
                                    continue;
                                }
                                total_success += probability;
                                const std::size_t successor = abstract_index(
                                    next_rarity, mask | subset,
                                    next_prefixes, next_suffixes);
                                if (successor == current) {
                                    self_probability += probability;
                                } else {
                                    continuation += probability *
                                        clean_goal_cover_cost[successor];
                                }
                            }
                            if (total_success > 1.0 + 1e-12) {
                                feasible_single_draw = false;
                            }
                            if (feasible_single_draw) {
                                const std::size_t failure = abstract_index(
                                    next_rarity, mask, prefixes, suffixes);
                                const double failure_probability =
                                    std::max(0.0, 1.0 - total_success);
                                if (failure == current) {
                                    self_probability += failure_probability;
                                } else {
                                    continuation += failure_probability *
                                        clean_goal_cover_cost[failure];
                                }
                                if (self_probability < 1.0) {
                                    consider(
                                        (cost + continuation) /
                                            (1.0 - self_probability),
                                        action);
                                }
                                continue;
                            }
                        }
                        const std::size_t action_position =
                            relaxation_action_position.at(action);
                        RelaxedStochasticEnvelope& envelope =
                            stochastic_envelopes.at(
                                current * relaxation_actions.size() +
                                action_position);
                        if (!envelope.ready) {
                            envelope.ready = true;
                            const std::uint32_t max_count =
                                std::popcount(available);
                            std::vector<double> cumulative(
                                max_count + 2, 0.0);
                            envelope.success_probability.assign(
                                max_count + 1, 0.0);
                            envelope.success_successors.resize(max_count + 1);
                            const std::uint8_t goal_prefixes =
                                minimum_goal_affixes[mask][PC_SIDE_PREFIX];
                            const std::uint8_t goal_suffixes =
                                minimum_goal_affixes[mask][PC_SIDE_SUFFIX];
                            const std::uint8_t prefix_blockers = destructive
                                ? 0
                                : static_cast<std::uint8_t>(
                                      prefixes > goal_prefixes
                                          ? prefixes - goal_prefixes
                                          : 0);
                            const std::uint8_t suffix_blockers = destructive
                                ? 0
                                : static_cast<std::uint8_t>(
                                      suffixes > goal_suffixes
                                          ? suffixes - goal_suffixes
                                          : 0);
                            for (std::uint32_t subset = available; subset != 0;
                                 subset = (subset - 1u) & available) {
                                const std::uint8_t added_prefixes =
                                    minimum_goal_affixes[subset]
                                                        [PC_SIDE_PREFIX];
                                const std::uint8_t added_suffixes =
                                    minimum_goal_affixes[subset]
                                                        [PC_SIDE_SUFFIX];
                                if (added_prefixes ==
                                        std::numeric_limits<
                                            std::uint8_t>::max() ||
                                    added_suffixes ==
                                        std::numeric_limits<
                                            std::uint8_t>::max()) {
                                    continue;
                                }
                                const std::uint8_t next_prefixes = destructive
                                    ? added_prefixes
                                    : static_cast<std::uint8_t>(
                                          prefixes + added_prefixes);
                                const std::uint8_t next_suffixes = destructive
                                    ? added_suffixes
                                    : static_cast<std::uint8_t>(
                                          suffixes + added_suffixes);
                                const std::uint8_t next_cap = rarity_affix_cap(
                                    session, next_rarity);
                                if (next_prefixes > next_cap ||
                                    next_suffixes > next_cap) {
                                    continue;
                                }
                                const std::uint32_t count =
                                    std::popcount(subset);
                                cumulative[count] = std::min(
                                    1.0,
                                    cumulative[count] +
                                        cached_subset_probability(
                                            action,
                                            destructive ? 0u : mask,
                                            subset,
                                            prefix_blockers,
                                            suffix_blockers));
                                envelope.success_successors[count].push_back(
                                    abstract_index(
                                        next_rarity,
                                        destructive ? subset : (mask | subset),
                                        next_prefixes, next_suffixes));
                            }
                            for (std::uint32_t count = 2;
                                 count < cumulative.size(); ++count) {
                                cumulative[count] = std::min(
                                    cumulative[count], cumulative[count - 1]);
                            }
                            envelope.failure_probability =
                                1.0 - cumulative[1];
                            for (std::uint32_t count = 1;
                                 count <= max_count; ++count) {
                                envelope.success_probability[count] =
                                    cumulative[count] - cumulative[count + 1];
                            }
                            if (destructive &&
                                (descriptor.params.type ==
                                     ActionType::Transmute ||
                                 descriptor.params.type ==
                                     ActionType::Alteration) &&
                                cumulative[1] > 0.0) {
                                envelope.failure_successors.push_back(
                                    abstract_index(next_rarity, 0, 1, 0));
                                envelope.failure_successors.push_back(
                                    abstract_index(next_rarity, 0, 0, 1));
                            } else {
                                envelope.failure_successors.push_back(
                                    abstract_index(
                                        next_rarity,
                                        destructive ? 0 : mask,
                                        destructive ? 0 : prefixes,
                                        destructive ? 0 : suffixes));
                            }
                        }

                        double self_probability = 0.0;
                        double continuation = 0.0;
                        const auto best_successor = [&](const auto& candidates) {
                            return std::min_element(
                                candidates.begin(), candidates.end(),
                                [&](const std::size_t left,
                                    const std::size_t right) {
                                    return clean_goal_cover_cost[left] <
                                           clean_goal_cover_cost[right];
                                });
                        };
                        const auto failure =
                            best_successor(envelope.failure_successors);
                        if (failure != envelope.failure_successors.end()) {
                            if (*failure == current) {
                                self_probability +=
                                    envelope.failure_probability;
                            } else {
                                continuation += envelope.failure_probability *
                                    clean_goal_cover_cost[*failure];
                            }
                        }
                        for (std::size_t count = 1;
                             count < envelope.success_probability.size();
                             ++count) {
                            const double probability =
                                envelope.success_probability[count];
                            if (!(probability > 0.0)) continue;
                            const auto successor = best_successor(
                                envelope.success_successors[count]);
                            if (successor ==
                                envelope.success_successors[count].end()) {
                                continue;
                            }
                            if (*successor == current) {
                                self_probability += probability;
                            } else {
                                continuation += probability *
                                    clean_goal_cover_cost[*successor];
                            }
                        }
                        if (self_probability < 1.0) {
                            consider(
                                (cost + continuation) /
                                    (1.0 - self_probability),
                                action);
                        }
                    }
                    if (!std::isfinite(best)) continue;
                    const double next = std::max(previous_current, best);
                    clean_goal_cover_cost[current] = next;
                    if (next == best) clean_goal_policy[current] = best_action;
                    delta = std::max(delta, next - previous_current);
                            }
                        }
                    }
            }
            relaxation_delta = delta;
            if (delta <= options.epsilon * 0.1) break;
        }
        const AbstractState& start = calc.state(result.start_state);
        const double start_lower = clean_goal_cover_cost[abstract_index(
            start.rarity, satisfied_goal_mask_for_state(result.start_state),
            start.prefix_count, start.suffix_count)];
        const std::uint32_t start_policy = clean_goal_policy[abstract_index(
            start.rarity, satisfied_goal_mask_for_state(result.start_state),
            start.prefix_count, start.suffix_count)];
        retain_action_reason(
            "included:clean_goal_progress_mdp:" + finite_json(start_lower) +
            ":sweeps=" + std::to_string(relaxation_sweeps) + ":" +
            "delta=" + finite_json(relaxation_delta) + ":" +
            (start_policy == kNoId
                ? std::string("none")
                : calc.registry().actions.at(start_policy).id));
        const std::size_t normal_empty = abstract_index(
            PC_RARITY_NORMAL, 0, 0, 0);
        const std::uint32_t normal_policy = clean_goal_policy[normal_empty];
        retain_action_reason(
            "included:clean_goal_progress_normal_empty:" +
            finite_json(clean_goal_cover_cost[normal_empty]) + ":" +
            (normal_policy == kNoId
                ? std::string("none")
                : calc.registry().actions.at(normal_policy).id));
        const std::uint32_t normal_escape =
            clean_goal_escape_action[normal_empty];
        retain_action_reason(
            "included:clean_goal_progress_normal_escape:" +
            finite_json(clean_goal_escape_cost[normal_empty]) + ":" +
            (normal_escape == kNoId
                ? std::string("none")
                : calc.registry().actions.at(normal_escape).id));
        for (const std::uint8_t rarity : {
                 static_cast<std::uint8_t>(PC_RARITY_MAGIC),
                 static_cast<std::uint8_t>(PC_RARITY_RARE)}) {
            for (std::uint32_t mask = 0; mask < mask_count; ++mask) {
                if (is_abstract_goal(rarity, mask)) continue;
                const std::uint8_t prefixes =
                    minimum_goal_affixes[mask][PC_SIDE_PREFIX];
                const std::uint8_t suffixes =
                    minimum_goal_affixes[mask][PC_SIDE_SUFFIX];
                const std::uint8_t cap = rarity_affix_cap(session, rarity);
                if (prefixes > cap || suffixes > cap) continue;
                const std::size_t index = abstract_index(
                    rarity, mask, prefixes, suffixes);
                const std::uint32_t policy = clean_goal_policy[index];
                const std::uint32_t escape =
                    clean_goal_escape_action[index];
                retain_action_reason(
                    "included:clean_goal_progress_state:" +
                    std::to_string(rarity) + ":" +
                    std::to_string(mask) + ":" +
                    finite_json(clean_goal_cover_cost[index]) + ":" +
                    (policy == kNoId
                        ? std::string("none")
                        : calc.registry().actions.at(policy).id) +
                    ":escape=" +
                    finite_json(clean_goal_escape_cost[index]) + ":" +
                    (escape == kNoId
                        ? std::string("none")
                        : calc.registry().actions.at(escape).id));
            }
        }
    }

    std::uint32_t satisfied_goal_mask_for_state(
        const std::uint32_t state) const {
        std::uint32_t mask = 0;
        const AbstractState& carrier = calc.state(state);
        for (std::uint32_t slot = 0;
             slot < calc.layout().slots.size(); ++slot) {
            if (carrier.slot_status[slot] ==
                static_cast<std::uint8_t>(GoalSlotStatus::Satisfied)) {
                mask |= 1u << slot;
            }
        }
        return mask;
    }

    double optimistic_completion_cost(
        const std::uint32_t satisfied_mask,
        const bool clean_carrier = false,
        const std::uint8_t carrier_rarity = PC_RARITY_NORMAL,
        const std::uint8_t carrier_prefixes = 0,
        const std::uint8_t carrier_suffixes = 0) {
        prepare_goal_cover_cost();
        const std::uint32_t required =
            calc.goal().required_satisfied_slots();
        if (std::popcount(satisfied_mask) >= required &&
            (!clean_carrier || carrier_rarity == calc.goal().rarity)) {
            return 0.0;
        }
        if (clean_carrier) {
            const std::size_t mask_count = goal_cover_cost.size();
            constexpr std::size_t kAffixCountStates = 4;
            const std::size_t index =
                (((static_cast<std::size_t>(carrier_rarity) * mask_count +
                   satisfied_mask) * kAffixCountStates + carrier_prefixes) *
                 kAffixCountStates + carrier_suffixes);
            return index < clean_goal_cover_cost.size()
                ? clean_goal_cover_cost[index]
                : 0.0;
        }
        double best = kInfinity;
        for (std::uint32_t produced = 0;
             produced < goal_cover_cost.size(); ++produced) {
            if (std::popcount(satisfied_mask | produced) < required) {
                continue;
            }
            best = std::min(best, goal_cover_cost[produced]);
        }
        return best;
    }

    bool clean_goal_cover_eligible(const std::uint32_t state) const {
        if (state >= calc.state_count() ||
            result.start_state >= calc.state_count()) {
            return false;
        }
        const AbstractState& carrier = calc.state(state);
        const AbstractState& start = calc.state(result.start_state);
        if ((carrier.flags & kProtectionFlags) != 0 ||
            carrier.fractured_goal_mask != 0 ||
            carrier.fractured_metamod_flags != 0 ||
            carrier.influence_bits != start.influence_bits ||
            carrier.searing_exarch_tier != start.searing_exarch_tier ||
            carrier.eater_of_worlds_tier != start.eater_of_worlds_tier) {
            return false;
        }
        for (const std::uint8_t count : carrier.fractured_junk_counts) {
            if (count != 0) return false;
        }
        for (const std::uint8_t count :
             carrier.fractured_crafted_junk_counts) {
            if (count != 0) return false;
        }
        return true;
    }

    double optimistic_completion_cost_for_state(
        const std::uint32_t state) {
        if (state >= calc.state_count()) return 0.0;
        const AbstractState& carrier = calc.state(state);
        const double coarse = optimistic_completion_cost(
            satisfied_goal_mask_for_state(state),
            clean_goal_cover_eligible(state), carrier.rarity,
            carrier.prefix_count, carrier.suffix_count);
        if (state < strict_clean_goal_cover_cost.size() &&
            std::isfinite(strict_clean_goal_cover_cost[state])) {
            return std::max(coarse, strict_clean_goal_cover_cost[state]);
        }
        return coarse;
    }

    void prepare_strict_clean_goal_cover() {
        strict_clean_goal_cover_refresh_needed = false;
        const std::uint32_t initial_state_count = calc.state_count();
        if (strict_clean_goal_cover_state_count == initial_state_count) return;
        strict_clean_goal_cover_state_count = 0;
        strict_clean_goal_cover_cost.clear();
        if (!goal_cover_cost_ready || clean_goal_escape_cost.empty()) return;

        const auto refined_action = [&](const std::uint32_t action) {
            if (action >= calc.registry().actions.size()) return false;
            const ActionDescriptor& descriptor =
                calc.registry().actions.at(action);
            const ActionType type = descriptor.params.type;
            const bool destructive =
                type == ActionType::Transmute ||
                type == ActionType::Alteration ||
                type == ActionType::Alchemy ||
                type == ActionType::Chaos ||
                type == ActionType::Essence ||
                type == ActionType::Fossil ||
                type == ActionType::HarvestReforge;
            const bool goal_bench = type == ActionType::Bench &&
                (descriptor.sets_flags & kProtectionFlags) == 0 &&
                action_goal_reach_mask(action) != 0;
            return descriptor.synthetic ||
                   destructive || type == ActionType::Augment ||
                   type == ActionType::Regal ||
                   type == ActionType::Exalt ||
                   type == ActionType::Scour ||
                   goal_bench;
        };
        std::vector<std::uint32_t> actions;
        for (const std::uint32_t action : calc.candidates()) {
            if (refined_action(action)) actions.push_back(action);
        }
        if (actions.empty()) return;
        const auto action_cost = [&](const std::uint32_t action) {
            double cost = 0.0;
            for (const std::string& key :
                 calc.registry().actions.at(action).cost_keys) {
                const auto found = prices.find(key);
                if (found == prices.end() ||
                    !std::isfinite(found->second) || found->second < 0.0) {
                    return kInfinity;
                }
                cost += found->second;
            }
            return cost;
        };
        const auto strict_state = [&](const std::uint32_t state) {
            if (state >= calc.state_count() ||
                calc.is_goal_state(calc.state(state)) ||
                !clean_goal_cover_eligible(state)) {
                return false;
            }
            const std::uint8_t rarity = calc.state(state).rarity;
            return rarity == PC_RARITY_NORMAL || rarity == PC_RARITY_MAGIC;
        };
        const std::size_t mask_count = goal_cover_cost.size();
        constexpr std::size_t kAffixCountStates = 4;
        const auto clean_index = [&](const std::uint32_t state) {
            const AbstractState& carrier = calc.state(state);
            return (((static_cast<std::size_t>(carrier.rarity) * mask_count +
                      satisfied_goal_mask_for_state(state)) *
                     kAffixCountStates + carrier.prefix_count) *
                    kAffixCountStates + carrier.suffix_count);
        };
        const auto coarse_value = [&](const std::uint32_t state) {
            if (state >= calc.state_count() ||
                calc.is_goal_state(calc.state(state))) {
                return 0.0;
            }
            const AbstractState& carrier = calc.state(state);
            return optimistic_completion_cost(
                satisfied_goal_mask_for_state(state),
                clean_goal_cover_eligible(state), carrier.rarity,
                carrier.prefix_count, carrier.suffix_count);
        };

        struct StrictRow {
            std::uint32_t action = kNoId;
            double immediate = 0.0;
            double fixed_continuation = 0.0;
            std::vector<OutcomeEntry> strict_entries;
            std::vector<std::pair<std::size_t, double>> rare_entries;
            std::vector<OutcomeEntry> concrete_rare_entries;
        };
        std::vector<std::uint32_t> strict_states;
        std::vector<std::uint32_t> concrete_rare_states;
        std::vector<std::uint8_t> included(initial_state_count, 0);
        std::vector<std::uint8_t> concrete_rare_included(
            initial_state_count, 0);
        for (std::uint32_t state = 0; state < initial_state_count; ++state) {
            if (strict_state(state)) {
                included[state] = 1;
                strict_states.push_back(state);
            }
        }
        using StrictRowPtr = std::shared_ptr<const StrictRow>;
        std::vector<std::vector<StrictRowPtr>> rows(initial_state_count);
        const auto destructive_kernel_action = [&](const std::uint32_t action) {
            const ActionType type =
                calc.registry().actions.at(action).params.type;
            return type == ActionType::Transmute ||
                   type == ActionType::Alteration ||
                   type == ActionType::Alchemy ||
                   type == ActionType::Chaos ||
                   type == ActionType::Essence ||
                   type == ActionType::Fossil ||
                   type == ActionType::HarvestReforge;
        };
        std::unordered_map<std::uint32_t, const OutcomeDistribution*>
            shared_destructive_kernels;
        std::unordered_map<std::uint32_t, StrictRowPtr>
            shared_destructive_rows;
        bool exact = true;
        for (std::size_t cursor = 0; cursor < strict_states.size() && exact;
             ++cursor) {
            const std::uint32_t state = strict_states[cursor];
            if (rows.size() < calc.state_count()) rows.resize(calc.state_count());
            for (const std::uint32_t action : actions) {
                const ActionDescriptor& descriptor =
                    calc.registry().actions.at(action);
                if (!action_legal(session, descriptor, calc.state(state))) {
                    continue;
                }
                const double immediate = action_cost(action);
                if (!std::isfinite(immediate)) continue;
                if (destructive_kernel_action(action)) {
                    const auto shared = shared_destructive_rows.find(action);
                    if (shared != shared_destructive_rows.end()) {
                        rows[state].push_back(shared->second);
                        continue;
                    }
                }
                const OutcomeDistribution* distribution_pointer = nullptr;
                if (destructive_kernel_action(action)) {
                    const auto shared =
                        shared_destructive_kernels.find(action);
                    if (shared != shared_destructive_kernels.end()) {
                        distribution_pointer = shared->second;
                    }
                }
                if (distribution_pointer == nullptr) {
                    distribution_pointer = &calc.outcomes(state, action);
                    if (destructive_kernel_action(action)) {
                        shared_destructive_kernels.emplace(
                            action, distribution_pointer);
                    }
                }
                const OutcomeDistribution& distribution =
                    *distribution_pointer;
                if (!distribution.supported ||
                    !distribution.choice_groups.empty() ||
                    !distribution.choice_options.empty()) {
                    exact = false;
                    break;
                }
                if (included.size() < calc.state_count()) {
                    included.resize(calc.state_count(), 0);
                    concrete_rare_included.resize(calc.state_count(), 0);
                    rows.resize(calc.state_count());
                }
                StrictRow row;
                row.action = action;
                row.immediate = immediate;
                row.strict_entries.reserve(distribution.entries.size());
                for (const OutcomeEntry& outcome : distribution.entries) {
                    if (strict_state(outcome.state)) {
                        row.strict_entries.push_back(outcome);
                        if (!included[outcome.state]) {
                            included[outcome.state] = 1;
                            strict_states.push_back(outcome.state);
                        }
                    } else if (
                        clean_goal_cover_eligible(outcome.state) &&
                        calc.state(outcome.state).rarity == PC_RARITY_RARE) {
                        row.rare_entries.push_back(
                            {clean_index(outcome.state),
                             outcome.probability});
                    } else {
                        row.fixed_continuation +=
                            outcome.probability * coarse_value(outcome.state);
                    }
                }
                std::sort(
                    row.rare_entries.begin(), row.rare_entries.end(),
                    [](const auto& left, const auto& right) {
                        return left.first < right.first;
                    });
                std::size_t write = 0;
                for (const auto& entry : row.rare_entries) {
                    if (write != 0 &&
                        row.rare_entries[write - 1].first == entry.first) {
                        row.rare_entries[write - 1].second += entry.second;
                    } else {
                        row.rare_entries[write++] = entry;
                    }
                }
                row.rare_entries.resize(write);
                StrictRowPtr retained =
                    std::make_shared<StrictRow>(std::move(row));
                if (destructive_kernel_action(action)) {
                    shared_destructive_rows.emplace(action, retained);
                }
                rows[state].push_back(std::move(retained));
            }
        }
        if (!exact) return;

        const auto destructive_action = [&](const std::uint32_t action) {
            const ActionType type =
                calc.registry().actions.at(action).params.type;
            return type == ActionType::Transmute ||
                   type == ActionType::Alteration ||
                   type == ActionType::Alchemy ||
                   type == ActionType::Chaos ||
                   type == ActionType::Essence ||
                   type == ActionType::Fossil ||
                   type == ActionType::HarvestReforge;
        };
        std::vector<StrictRowPtr> rare_rows;
        if (result.start_state < calc.state_count() &&
            calc.state(result.start_state).rarity == PC_RARITY_RARE &&
            clean_goal_cover_eligible(result.start_state)) {
            for (const std::uint32_t action : actions) {
                if (!destructive_action(action)) continue;
                const ActionDescriptor& descriptor =
                    calc.registry().actions.at(action);
                if (!action_legal(
                        session, descriptor,
                        calc.state(result.start_state))) {
                    continue;
                }
                const double immediate = action_cost(action);
                if (!std::isfinite(immediate)) continue;
                const auto retained_row =
                    shared_destructive_rows.find(action);
                if (retained_row != shared_destructive_rows.end()) {
                    rare_rows.push_back(retained_row->second);
                    continue;
                }
                const OutcomeDistribution* distribution_pointer = nullptr;
                const auto shared = shared_destructive_kernels.find(action);
                if (shared != shared_destructive_kernels.end()) {
                    distribution_pointer = shared->second;
                } else {
                    distribution_pointer =
                        &calc.outcomes(result.start_state, action);
                    shared_destructive_kernels.emplace(
                        action, distribution_pointer);
                }
                const OutcomeDistribution& distribution =
                    *distribution_pointer;
                if (!distribution.supported ||
                    !distribution.choice_groups.empty() ||
                    !distribution.choice_options.empty()) {
                    exact = false;
                    break;
                }
                StrictRow row;
                row.action = action;
                row.immediate = immediate;
                for (const OutcomeEntry& outcome : distribution.entries) {
                    if (strict_state(outcome.state)) {
                        row.strict_entries.push_back(outcome);
                    } else if (
                        clean_goal_cover_eligible(outcome.state) &&
                        calc.state(outcome.state).rarity == PC_RARITY_RARE) {
                        row.rare_entries.push_back(
                            {clean_index(outcome.state),
                             outcome.probability});
                    } else {
                        row.fixed_continuation +=
                            outcome.probability * coarse_value(outcome.state);
                    }
                }
                std::sort(
                    row.rare_entries.begin(), row.rare_entries.end(),
                    [](const auto& left, const auto& right) {
                        return left.first < right.first;
                    });
                std::size_t write = 0;
                for (const auto& entry : row.rare_entries) {
                    if (write != 0 &&
                        row.rare_entries[write - 1].first == entry.first) {
                        row.rare_entries[write - 1].second += entry.second;
                    } else {
                        row.rare_entries[write++] = entry;
                    }
                }
                row.rare_entries.resize(write);
                StrictRowPtr retained =
                    std::make_shared<StrictRow>(std::move(row));
                shared_destructive_rows.emplace(action, retained);
                rare_rows.push_back(std::move(retained));
            }
        }
        if (!exact) return;

        std::uint32_t strict_exalt = kNoId;
        for (const std::uint32_t action : actions) {
            if (calc.registry().actions.at(action).params.type ==
                ActionType::Exalt) {
                strict_exalt = action;
                break;
            }
        }
        std::unordered_map<std::uint32_t, StrictRowPtr>
            concrete_exalt_rows;
        for (std::size_t cursor = 0;
             cursor < concrete_rare_states.size() && exact; ++cursor) {
            const std::uint32_t state = concrete_rare_states[cursor];
            if (strict_exalt == kNoId ||
                !action_legal(
                    session, calc.registry().actions.at(strict_exalt),
                    calc.state(state))) {
                continue;
            }
            const double immediate = action_cost(strict_exalt);
            if (!std::isfinite(immediate)) continue;
            const OutcomeDistribution& distribution =
                calc.outcomes(state, strict_exalt);
            if (!distribution.supported ||
                !distribution.choice_groups.empty() ||
                !distribution.choice_options.empty()) {
                exact = false;
                break;
            }
            if (concrete_rare_included.size() < calc.state_count()) {
                concrete_rare_included.resize(calc.state_count(), 0);
            }
            StrictRow row;
            row.action = strict_exalt;
            row.immediate = immediate;
            bool acyclic = true;
            for (const OutcomeEntry& outcome : distribution.entries) {
                if (calc.is_goal_state(calc.state(outcome.state))) continue;
                if (clean_goal_cover_eligible(outcome.state) &&
                    calc.state(outcome.state).rarity == PC_RARITY_RARE &&
                    calc.state(outcome.state).prefix_count +
                            calc.state(outcome.state).suffix_count >
                        calc.state(state).prefix_count +
                            calc.state(state).suffix_count) {
                    row.concrete_rare_entries.push_back(outcome);
                    if (!concrete_rare_included[outcome.state]) {
                        concrete_rare_included[outcome.state] = 1;
                        concrete_rare_states.push_back(outcome.state);
                    }
                } else {
                    acyclic = false;
                    break;
                }
            }
            if (acyclic) {
                concrete_exalt_rows.emplace(
                    state,
                    std::make_shared<StrictRow>(std::move(row)));
            }
        }
        if (!exact) return;
        std::stable_sort(
            concrete_rare_states.begin(), concrete_rare_states.end(),
            [&](const std::uint32_t left, const std::uint32_t right) {
                const std::uint8_t left_count =
                    calc.state(left).prefix_count +
                    calc.state(left).suffix_count;
                const std::uint8_t right_count =
                    calc.state(right).prefix_count +
                    calc.state(right).suffix_count;
                return left_count != right_count
                    ? left_count > right_count
                    : left < right;
            });

        strict_clean_goal_cover_cost.assign(calc.state_count(), kInfinity);
        std::vector<std::uint32_t> strict_policy(
            calc.state_count(), kNoId);
        for (const std::uint32_t state : strict_states) {
            strict_clean_goal_cover_cost[state] = 0.0;
        }
        for (const std::uint32_t state : concrete_rare_states) {
            strict_clean_goal_cover_cost[state] = 0.0;
        }
        std::vector<double> rare_value(
            clean_goal_cover_cost.size(), kInfinity);
        std::vector<std::uint32_t> rare_policy(
            clean_goal_cover_cost.size(), kNoId);
        for (std::uint32_t mask = 0; mask < mask_count; ++mask) {
            for (std::uint8_t prefixes = 0; prefixes <= 3; ++prefixes) {
                for (std::uint8_t suffixes = 0; suffixes <= 3; ++suffixes) {
                    const std::size_t index =
                        (((static_cast<std::size_t>(PC_RARITY_RARE) *
                           mask_count + mask) * kAffixCountStates +
                          prefixes) * kAffixCountStates + suffixes);
                    rare_value[index] =
                        calc.goal().rarity == PC_RARITY_RARE &&
                                std::popcount(mask) >=
                                    calc.goal().required_satisfied_slots()
                            ? 0.0
                            : 0.0;
                }
            }
        }
        double cheapest_reset = kInfinity;
        std::uint32_t cheapest_reset_action = kNoId;
        for (const std::uint32_t action : actions) {
            const ActionDescriptor& descriptor =
                calc.registry().actions.at(action);
            if (descriptor.synthetic ||
                descriptor.params.type == ActionType::Scour) {
                const double cost = action_cost(action);
                if (cost < cheapest_reset) {
                    cheapest_reset = cost;
                    cheapest_reset_action = action;
                }
            }
        }
        constexpr std::uint32_t kStrictRelaxationSweeps = 4096;
        std::uint32_t sweeps = 0;
        double delta = kInfinity;
        for (std::uint32_t sweep = 0;
             sweep < kStrictRelaxationSweeps; ++sweep) {
            sweeps = sweep + 1;
            delta = 0.0;
            const double anchor_value =
                restart_state < strict_clean_goal_cover_cost.size()
                    ? strict_clean_goal_cover_cost[restart_state]
                    : 0.0;
            for (std::uint32_t mask = 0; mask < mask_count; ++mask) {
                for (std::uint8_t prefixes = 0; prefixes <= 3; ++prefixes) {
                    for (std::uint8_t suffixes = 0; suffixes <= 3;
                         ++suffixes) {
                        const std::size_t index =
                            (((static_cast<std::size_t>(PC_RARITY_RARE) *
                               mask_count + mask) * kAffixCountStates +
                              prefixes) * kAffixCountStates + suffixes);
                        if (calc.goal().rarity == PC_RARITY_RARE &&
                            std::popcount(mask) >=
                                calc.goal().required_satisfied_slots()) {
                            rare_value[index] = 0.0;
                            continue;
                        }
                        const double previous = rare_value[index];
                        double best = clean_goal_escape_cost[index];
                        std::uint32_t best_action =
                            clean_goal_escape_action[index];
                        if (std::isfinite(cheapest_reset)) {
                            const double candidate =
                                cheapest_reset + anchor_value;
                            if (candidate < best) {
                                best = candidate;
                                best_action = cheapest_reset_action;
                            }
                        }
                        for (const StrictRowPtr& row_pointer : rare_rows) {
                            const StrictRow& row = *row_pointer;
                            double continuation = row.fixed_continuation;
                            double self_probability = 0.0;
                            for (const OutcomeEntry& outcome :
                                 row.strict_entries) {
                                continuation += outcome.probability *
                                    strict_clean_goal_cover_cost[
                                        outcome.state];
                            }
                            for (const auto& [successor, probability] :
                                 row.rare_entries) {
                                if (successor == index) {
                                    self_probability += probability;
                                } else {
                                    continuation +=
                                        probability * rare_value[successor];
                                }
                            }
                            if (self_probability < 1.0) {
                                const double candidate =
                                    (row.immediate + continuation) /
                                    (1.0 - self_probability);
                                if (candidate < best) {
                                    best = candidate;
                                    best_action = row.action;
                                }
                            }
                        }
                        for (const std::uint32_t action : actions) {
                            const ActionDescriptor& descriptor =
                                calc.registry().actions.at(action);
                            if (descriptor.params.type != ActionType::Bench ||
                                (descriptor.sets_flags & kProtectionFlags) !=
                                    0) {
                                continue;
                            }
                            const std::uint32_t reach =
                                action_goal_reach_mask(action);
                            if (reach == 0 || (reach & ~mask) == 0) continue;
                            const double immediate = action_cost(action);
                            if (!std::isfinite(immediate)) continue;
                            const std::uint32_t next_mask = mask | reach;
                            const std::size_t successor =
                                (((static_cast<std::size_t>(PC_RARITY_RARE) *
                                   mask_count + next_mask) *
                                  kAffixCountStates + prefixes) *
                                 kAffixCountStates + suffixes);
                            const double candidate =
                                immediate + rare_value[successor];
                            if (candidate < best) {
                                best = candidate;
                                best_action = action;
                            }
                        }
                        if (!std::isfinite(best)) continue;
                        const double next = std::max(previous, best);
                        rare_value[index] = next;
                        if (next == best) rare_policy[index] = best_action;
                        delta = std::max(delta, next - previous);
                    }
                }
            }
            for (const std::uint32_t state : concrete_rare_states) {
                const double previous =
                    strict_clean_goal_cover_cost[state];
                const std::size_t index = clean_index(state);
                double best = clean_goal_no_exalt_escape_cost[index];
                std::uint32_t best_action =
                    clean_goal_no_exalt_escape_action[index];
                if (std::isfinite(cheapest_reset)) {
                    const double candidate =
                        cheapest_reset + anchor_value;
                    if (candidate < best) {
                        best = candidate;
                        best_action = cheapest_reset_action;
                    }
                }
                for (const StrictRowPtr& row_pointer : rare_rows) {
                    const StrictRow& row = *row_pointer;
                    double continuation = row.fixed_continuation;
                    double self_probability = 0.0;
                    for (const OutcomeEntry& outcome :
                         row.concrete_rare_entries) {
                        if (outcome.state == state) {
                            self_probability += outcome.probability;
                        } else {
                            continuation += outcome.probability *
                                strict_clean_goal_cover_cost[outcome.state];
                        }
                    }
                    for (const OutcomeEntry& outcome : row.strict_entries) {
                        continuation += outcome.probability *
                            strict_clean_goal_cover_cost[outcome.state];
                    }
                    if (self_probability < 1.0) {
                        const double candidate =
                            (row.immediate + continuation) /
                            (1.0 - self_probability);
                        if (candidate < best) {
                            best = candidate;
                            best_action = row.action;
                        }
                    }
                }
                const auto exalt_row = concrete_exalt_rows.find(state);
                if (exalt_row != concrete_exalt_rows.end()) {
                    double candidate = exalt_row->second->immediate;
                    for (const OutcomeEntry& outcome :
                         exalt_row->second->concrete_rare_entries) {
                        candidate += outcome.probability *
                            strict_clean_goal_cover_cost[outcome.state];
                    }
                    if (candidate < best) {
                        best = candidate;
                        best_action = strict_exalt;
                    }
                }
                if (!std::isfinite(best)) continue;
                const double next = std::max(previous, best);
                strict_clean_goal_cover_cost[state] = next;
                if (next == best) strict_policy[state] = best_action;
                delta = std::max(delta, next - previous);
            }
            for (const std::uint32_t state : strict_states) {
                const double previous = strict_clean_goal_cover_cost[state];
                const std::size_t index = clean_index(state);
                double best = index < clean_goal_escape_cost.size()
                    ? clean_goal_escape_cost[index]
                    : kInfinity;
                std::uint32_t best_action =
                    index < clean_goal_escape_action.size()
                        ? clean_goal_escape_action[index]
                        : kNoId;
                for (const StrictRowPtr& row_pointer : rows[state]) {
                    const StrictRow& row = *row_pointer;
                    double continuation = row.fixed_continuation;
                    double self_probability = 0.0;
                    for (const OutcomeEntry& outcome : row.strict_entries) {
                        if (outcome.state == state) {
                            self_probability += outcome.probability;
                        } else {
                            continuation += outcome.probability *
                                strict_clean_goal_cover_cost[outcome.state];
                        }
                    }
                    for (const auto& [successor, probability] :
                         row.rare_entries) {
                        continuation += probability * rare_value[successor];
                    }
                    for (const OutcomeEntry& outcome :
                         row.concrete_rare_entries) {
                        continuation += outcome.probability *
                            strict_clean_goal_cover_cost[outcome.state];
                    }
                    if (self_probability < 1.0) {
                        const double candidate =
                            (row.immediate + continuation) /
                            (1.0 - self_probability);
                        if (candidate < best) {
                            best = candidate;
                            best_action = row.action;
                        }
                    }
                }
                if (!std::isfinite(best)) continue;
                const double next = std::max(previous, best);
                strict_clean_goal_cover_cost[state] = next;
                if (next == best) strict_policy[state] = best_action;
                delta = std::max(delta, next - previous);
            }
            if (delta <= options.epsilon * 0.1) break;
        }
        if (strict_clean_goal_cover_cost.size() < calc.state_count()) {
            strict_clean_goal_cover_cost.resize(
                calc.state_count(), kInfinity);
        }
        for (std::uint32_t state = 0; state < calc.state_count(); ++state) {
            if (!calc.is_goal_state(calc.state(state)) &&
                clean_goal_cover_eligible(state) &&
                calc.state(state).rarity == PC_RARITY_RARE &&
                (state >= concrete_rare_included.size() ||
                 !concrete_rare_included[state])) {
                strict_clean_goal_cover_cost[state] =
                    rare_value[clean_index(state)];
            }
        }
        strict_clean_goal_cover_state_count = calc.state_count();
        strict_clean_goal_cover_refresh_needed = true;
        const double anchor = restart_state < strict_clean_goal_cover_cost.size()
            ? strict_clean_goal_cover_cost[restart_state]
            : kInfinity;
        std::map<std::uint32_t, std::uint32_t> policy_counts;
        std::map<std::uint32_t, std::uint32_t> rare_policy_counts;
        for (const std::uint32_t state : strict_states) {
            if (strict_policy[state] != kNoId) {
                ++policy_counts[strict_policy[state]];
            }
        }
        for (std::uint32_t mask = 0; mask < mask_count; ++mask) {
            for (std::uint8_t prefixes = 0; prefixes <= 3; ++prefixes) {
                for (std::uint8_t suffixes = 0; suffixes <= 3; ++suffixes) {
                    const std::size_t index =
                        (((static_cast<std::size_t>(PC_RARITY_RARE) *
                           mask_count + mask) * kAffixCountStates +
                          prefixes) * kAffixCountStates + suffixes);
                    if (rare_policy[index] != kNoId) {
                        ++rare_policy_counts[rare_policy[index]];
                    }
                }
            }
        }
        std::string strict_reason =
            "included:strict_clean_goal_progress_mdp:" +
            finite_json(anchor) + ":states=" +
            std::to_string(strict_states.size()) + ":sweeps=" +
            std::to_string(sweeps) + ":delta=" + finite_json(delta) +
            ":anchor_action=" +
            (restart_state < strict_policy.size() &&
                     strict_policy[restart_state] != kNoId
                 ? calc.registry().actions.at(
                       strict_policy[restart_state]).id
                 : std::string("none")) +
            ":policies=";
        bool first_policy = true;
        for (const auto& [action, count] : policy_counts) {
            if (!first_policy) strict_reason += ',';
            first_policy = false;
            strict_reason += calc.registry().actions.at(action).id + '=' +
                std::to_string(count);
        }
        strict_reason += ":rare_policies=";
        first_policy = true;
        for (const auto& [action, count] : rare_policy_counts) {
            if (!first_policy) strict_reason += ',';
            first_policy = false;
            strict_reason += calc.registry().actions.at(action).id + '=' +
                std::to_string(count);
        }
        if (!result.diagnostics.action_inclusion_reasons.empty() &&
            result.diagnostics.action_inclusion_reasons.size() >=
                options.max_diagnostic_samples) {
            result.diagnostics.action_inclusion_reasons.back() =
                std::move(strict_reason);
            ++result.diagnostics.action_inclusion_reasons_omitted;
        } else {
            retain_action_reason(std::move(strict_reason));
        }
    }

    double optimistic_operator_lower(
        const std::uint32_t state,
        const std::uint32_t operator_index) {
        if (operator_index >= priced_operator_position.size()) {
            return kInfinity;
        }
        const std::int32_t position =
            priced_operator_position[operator_index];
        if (position < 0) return kInfinity;
        const PlannerOperator& planner =
            calc.operators().at(operator_index);
        double immediate =
            operators.at(static_cast<std::size_t>(position)).cost;
        if (planner.kind == PlannerOperatorKind::FixedOption) {
            /* Some fixed programs have conditional later primitives whose
             * aggregate planner quantity is not an admissible immediate
             * lower bound. Every legal program executes its first ordinary
             * primitive at least once, so price only that guaranteed step.
             * Granting all later reachability below remains optimistic. */
            if (planner.primitive_program.empty()) return -kInfinity;
            immediate = 0.0;
            for (const std::string& key :
                 calc.registry().actions.at(
                     planner.primitive_program.front()).cost_keys) {
                const auto found = prices.find(key);
                if (found == prices.end() ||
                    !std::isfinite(found->second) ||
                    found->second < 0.0) {
                    return -kInfinity;
                }
                immediate += found->second;
            }
        }
        if (!std::isfinite(immediate) || immediate < 0.0) {
            return -kInfinity;
        }
        /* Grant the operator every slot any constituent could possibly
         * produce before pricing the remaining optimistic cover. */
        const std::uint32_t optimistic_satisfied =
            satisfied_goal_mask_for_state(state) |
            planner_goal_reach_mask(operator_index);
        const double continuation =
            optimistic_completion_cost(optimistic_satisfied);
        return immediate + continuation;
    }

    std::optional<double> constructive_row_upper(
        const std::uint32_t state,
        const std::uint64_t row_index) {
        if (row_index >= transition_cache->rows.size() ||
            row_index >= priced_rows.size()) {
            return std::nullopt;
        }
        if (certified_state_upper.size() < calc.state_count()) {
            certified_state_upper.resize(calc.state_count(), kInfinity);
            certified_state_row.resize(
                calc.state_count(),
                std::numeric_limits<std::uint64_t>::max());
        }
        const SparseRow& row = transition_cache->rows.at(row_index);
        const PricedSparseRow& priced = priced_rows.at(row_index);
        if (priced.operator_index == kNoId ||
            !std::isfinite(priced.cost) || priced.cost < 0.0) {
            return std::nullopt;
        }
        const auto successor_upper = [&](const std::uint32_t successor) {
            if (calc.is_goal_state(calc.state(successor))) return 0.0;
            return successor < certified_state_upper.size()
                       ? certified_state_upper[successor]
                       : kInfinity;
        };
        double constant = priced.cost;
        double loop_probability = row.self_probability;
        for (std::uint32_t i = 0; i < row.transition_count; ++i) {
            const std::uint64_t offset = row.transition_offset + i;
            const std::uint32_t successor =
                transition_cache->successors.at(offset);
            if (successor == state) continue;
            const double upper = successor_upper(successor);
            if (!std::isfinite(upper)) return std::nullopt;
            constant += transition_cache->probabilities.at(offset) * upper;
        }
        for (std::uint32_t i = 0; i < row.choice_count; ++i) {
            const SparseChoiceGroup& group = transition_cache->choices.at(
                row.choice_offset + i);
            double selected = kInfinity;
            for (std::uint32_t s = 0; s < group.successor_count; ++s) {
                selected = std::min(
                    selected,
                    successor_upper(
                        transition_cache->choice_successors.at(
                            group.successor_offset + s)));
            }
            if (std::isfinite(selected)) {
                constant += group.probability * selected;
            } else if (group.has_self) {
                loop_probability += group.probability;
            } else {
                return std::nullopt;
            }
        }
        const double denominator = 1.0 - loop_probability;
        if (denominator <= 1e-15) return std::nullopt;
        const double upper = constant / denominator;
        if (!std::isfinite(upper) || upper >= kValueCeiling) {
            return std::nullopt;
        }
        return upper;
    }

    bool try_constructive_state_certificate(
        const std::uint32_t state,
        const std::uint64_t row_index) {
        if (!options.state_certificate_control || focused_mode ||
            cache_pending) {
            return false;
        }
        const std::optional<double> candidate =
            constructive_row_upper(state, row_index);
        if (!candidate.has_value()) return false;
        const double upper = *candidate;
        const std::uint32_t selected_operator =
            priced_rows.at(row_index).operator_index;
        double strict_min_other_lower = kInfinity;
        for (const std::uint32_t other : expansion_operator_indices) {
            if (other == selected_operator) continue;
            const double lower = optimistic_operator_lower(state, other);
            strict_min_other_lower =
                std::min(strict_min_other_lower, lower);
            const double separation = options.epsilon *
                std::max({1.0, std::abs(upper), std::abs(lower)});
            if (!std::isfinite(lower) ||
                !(lower > upper + separation)) {
                return false;
            }
        }
        const std::uint64_t pruned =
            expansion_operator_indices.size() -
            expansion_operator_cursor;
        if (pruned == 0) return false;
        price_bound_state_pruning = true;
        if (certified_state_upper.size() < calc.state_count()) {
            certified_state_upper.resize(calc.state_count(), kInfinity);
            certified_state_row.resize(
                calc.state_count(),
                std::numeric_limits<std::uint64_t>::max());
        }
        certified_state_upper[state] = upper;
        certified_state_row[state] = row_index;
        ++result.diagnostics.constructive_state_certificates;
        result.diagnostics.constructive_state_operators_pruned += pruned;
        if (state == result.start_state &&
            !std::isfinite(
                result.diagnostics.constructive_upper_bound)) {
            result.diagnostics.constructive_upper_bound = upper;
            result.diagnostics.constructive_upper_first_expanded_state =
                expanded_count;
        }
        if (result.diagnostics.constructive_state_witnesses.size() <
            options.max_diagnostic_samples) {
            std::string witness = "{\"state\":" +
                std::to_string(state) + ",\"operator\":";
            append_json_string(
                witness,
                calc.operators().at(selected_operator).id);
            witness += ",\"constructive_upper\":" +
                finite_json(upper) +
                ",\"strict_min_other_lower\":" +
                finite_json(strict_min_other_lower) +
                ",\"operators_pruned\":" +
                std::to_string(pruned) +
                ",\"proof\":\"optimistic_goal_production_cover\"}";
            result.diagnostics.constructive_state_witnesses.push_back(
                std::move(witness));
        } else {
            ++result.diagnostics.constructive_state_witnesses_omitted;
        }
        retain_action_reason(
            "pruned:constructive_state_certificate:" +
            std::to_string(pruned));
        expansion_operator_cursor =
            static_cast<std::uint32_t>(
                expansion_operator_indices.size());
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
        record.missing_price = decision.missing_price;
        record.telemetry_kind = decision.telemetry_kind;
        record.template_hit = decision.template_hit;
        record.template_id = decision.template_id;
        record.raw_outcomes = decision.raw_outcomes;
        record.admission_ns = decision.admission_ns;
        record.kernel_evaluation_ns = decision.kernel_evaluation_ns;
        record.outcome_mapping_ns = decision.outcome_mapping_ns;
        record.template_matching_ns = decision.template_matching_ns;
        record.protected_side_evaluations =
            decision.protected_side_evaluations;
        record.protected_repeat_evaluations =
            decision.protected_repeat_evaluations;
        record.protected_retry_checks = decision.protected_retry_checks;
        record.protected_retry_certificates =
            decision.protected_retry_certificates;
        record.protected_retry_fallbacks =
            decision.protected_retry_fallbacks;
        record.protected_attempt_ns = decision.protected_attempt_ns;
        record.protected_baseline_ns = decision.protected_baseline_ns;
        record.protected_normalization_ns =
            decision.protected_normalization_ns;
        record.protected_finish_ns = decision.protected_finish_ns;
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
        expansion_operator_indices.erase(
            std::remove_if(
                expansion_operator_indices.begin(),
                expansion_operator_indices.end(),
                [&](const std::uint32_t index) {
                    const PlannerOperator& planner =
                        calc.operators().at(index);
                    return planner.automatic_kind ==
                               AutomaticCandidateKind::ConstructiveRenewal &&
                           restart_state != kNoId && state != restart_state;
                }),
            expansion_operator_indices.end());
        AutomaticAdmissionLimits limits;
        limits.max_discovered_states = options.max_discovered_states;
        limits.max_state_action_rows = options.max_state_action_rows;
        limits.max_transitions = options.max_transitions;
        limits.max_reforge_work = options.max_reforge_work;
        const auto byte_audit_started = std::chrono::steady_clock::now();
        const std::uint64_t calc_bytes =
            calc.fast_estimated_owned_bytes();
        ++owned_byte_ledger_requests;
        const std::uint64_t total_bytes =
            fast_estimated_owned_bytes_with_calc(calc_bytes);
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
        if (state < focused_previous_upper_values.size() &&
            std::isfinite(focused_previous_upper_values[state])) {
            limits.incumbent_upper_bound =
                focused_previous_upper_values[state];
        } else if (focused_fallback_policy.has_value()) {
            const FocusedFallbackPolicy& fallback =
                *focused_fallback_policy;
            const double terminal =
                fallback_terminal_upper(state, fallback);
            limits.incumbent_upper_bound =
                state == fallback.anchor_state
                    ? fallback.anchor_state_value
                    : std::min(
                          terminal,
                          restart_cost + fallback.anchor_state_value);
        }
        const auto admission_started = std::chrono::steady_clock::now();
        StateLocalAutomaticBatch batch =
            calc.admit_state_local_automatic_candidates(state, limits);
        result.diagnostics.expansion_prepare_admission_ns +=
            static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() - admission_started)
                    .count());
        AutomaticAdmissionPhaseTelemetry& phases =
            transition_cache->automatic_admission_phases;
        phases.carriers += batch.phases.carriers;
        phases.synthesis_ns += batch.phases.synthesis_ns;
        phases.local_context_ns += batch.phases.local_context_ns;
        phases.local_planner_build_ns +=
            batch.phases.local_planner_build_ns;
        phases.local_layout_build_ns +=
            batch.phases.local_layout_build_ns;
        phases.local_ledger_init_ns +=
            batch.phases.local_ledger_init_ns;
        phases.local_context_other_ns +=
            batch.phases.local_context_other_ns;
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
        if (std::isfinite(limits.incumbent_upper_bound)) {
            const double incumbent = limits.incumbent_upper_bound;
            const std::size_t before = expansion_operator_indices.size();
            expansion_operator_indices.erase(
                std::remove_if(
                    expansion_operator_indices.begin(),
                    expansion_operator_indices.end(),
                    [&](const std::uint32_t index) {
                        const double lower =
                            optimistic_operator_lower(state, index);
                        const double separation = options.epsilon *
                            std::max({1.0, std::abs(incumbent),
                                      std::abs(lower)});
                        return std::isfinite(lower) &&
                               lower > incumbent + separation;
                    }),
                expansion_operator_indices.end());
            const std::size_t pruned =
                before - expansion_operator_indices.size();
            if (pruned != 0) {
                price_bound_state_pruning = true;
                retain_action_reason(
                    "pruned:state_incumbent_operator_lower:" +
                    std::to_string(pruned));
            }
        }
        /* Constructive deterministic goal finishes and Restart are exact
         * upper-bound producers. Evaluate them before broad stochastic
         * kernels so a strict state certificate can avoid discovering an
         * already-dominated fringe. This is only an evaluation schedule;
         * absent a certificate every admitted operator is still evaluated. */
        std::stable_sort(
            expansion_operator_indices.begin(),
            expansion_operator_indices.end(),
            [&](const std::uint32_t left, const std::uint32_t right) {
                const auto priority = [&](const std::uint32_t index) {
                    const PlannerOperator& planner =
                        calc.operators().at(index);
                    if (planner.automatic_kind ==
                            AutomaticCandidateKind::PermanentBench ||
                        (planner.kind == PlannerOperatorKind::Primitive &&
                         calc.registry().actions.at(
                             planner.primitive_action).params.type ==
                             ActionType::Bench &&
                         planner_goal_reach_mask(index) != 0)) {
                        return 0;
                    }
                    if (index == restart_operator_index) return 1;
                    return 2;
                };
                return priority(left) < priority(right);
            });
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

    void enqueue_front(const std::uint32_t state) {
        if (state >= queued.size()) queued.resize(state + 1, 0);
        if (queued[state]) return;
        queued[state] = 1;
        queue.push_front(state);
        peak_queue_size = std::max<std::uint32_t>(
            peak_queue_size, static_cast<std::uint32_t>(queue.size()));
    }

    bool same_kernel(
        const SparseRow& stored,
        const PendingSparseRow& pending,
        const std::size_t transition_count,
        const double self_probability) const {
        const auto is_self = [&](const std::uint32_t successor) {
            return pending.entry_relative_self ? successor == kNoId
                                               : successor == pending.state;
        };
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
        case AutomaticCandidateKind::ConstructiveRenewal:
            return "constructive_renewal";
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
                if (record.deferred) {
                    ++kind.deferred_candidates;
                } else if (!record.eligible) {
                    ++kind.rejected_candidates;
                } else {
                    ++kind.eligible_candidates;
                }
                if (record.collapsed) ++kind.collapsed_candidates;
                if (record.missing_price) {
                    ++kind.missing_price_candidates;
                }
                if (!record.eligible && !record.deferred &&
                    !record.missing_price) {
                    const std::string& reason = record.evidence.reason;
                    if (reason.find("cleanup") != std::string::npos) {
                        ++kind.cleanup_rejections;
                    } else if (reason.find("neutral") != std::string::npos) {
                        ++kind.neutral_kernel_rejections;
                    } else if (reason.find("relevant") != std::string::npos ||
                               reason.find("advance") != std::string::npos ||
                               reason.find("protected_carrier") !=
                                   std::string::npos) {
                        ++kind.relevance_rejections;
                    } else if (reason.find("setup") != std::string::npos ||
                               reason.find("protection_already") !=
                                   std::string::npos) {
                        ++kind.setup_rejections;
                    } else {
                        ++kind.other_rejections;
                    }
                }
                if (record.template_id != 0) {
                    if (record.template_hit) ++kind.template_hits;
                    else ++kind.unique_templates;
                }
                kind.raw_outcomes += record.raw_outcomes;
                kind.admission_ns += record.admission_ns;
                kind.kernel_evaluation_ns +=
                    record.kernel_evaluation_ns;
                kind.outcome_mapping_ns += record.outcome_mapping_ns;
                kind.template_matching_ns +=
                    record.template_matching_ns;
                kind.protected_side_evaluations +=
                    record.protected_side_evaluations;
                kind.protected_repeat_evaluations +=
                    record.protected_repeat_evaluations;
                kind.protected_retry_checks +=
                    record.protected_retry_checks;
                kind.protected_retry_certificates +=
                    record.protected_retry_certificates;
                kind.protected_retry_fallbacks +=
                    record.protected_retry_fallbacks;
                kind.protected_attempt_ns += record.protected_attempt_ns;
                kind.protected_baseline_ns += record.protected_baseline_ns;
                kind.protected_normalization_ns +=
                    record.protected_normalization_ns;
                kind.protected_finish_ns += record.protected_finish_ns;
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
            transition_cache->retain_automatic_sample(std::move(record));
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
        result.diagnostics.automatic_admission_phases =
            transition_cache->automatic_admission_phases;
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
            if (result.diagnostics.policy_evaluation_failure.empty()) {
                const std::uint64_t active_graph =
                    transition_cache == nullptr
                        ? 0
                        : transition_cache->fast_estimated_owned_bytes();
                const std::uint64_t strict_graph =
                    focused_strict_transition_cache == nullptr ||
                            focused_strict_transition_cache == transition_cache
                        ? 0
                        : focused_strict_transition_cache
                              ->fast_estimated_owned_bytes();
                result.diagnostics.policy_evaluation_failure =
                    "selected_byte_cap_breakdown:current=" +
                    std::to_string(current) + ",transient=" +
                    std::to_string(transient_bytes) + ",calc=" +
                    std::to_string(calc.fast_estimated_owned_bytes()) +
                    ",active_graph=" + std::to_string(active_graph) +
                    ",strict_graph=" + std::to_string(strict_graph) +
                    ",policy_scratch=" +
                    std::to_string(current_policy_scratch_bytes) +
                    (transition_cache == nullptr
                         ? std::string{}
                         : ",graph_caps=rows:" +
                               std::to_string(
                                   transition_cache->rows.capacity()) +
                               ",variants:" +
                               std::to_string(
                                   transition_cache->variant_arena->variants
                                       .capacity()) +
                               ",row_variants:" +
                               std::to_string(transition_cache
                                                  ->variant_arena
                                                  ->row_variant_indices
                                                  .capacity()) +
                               ",quantities:" +
                               std::to_string(transition_cache
                                                  ->variant_arena
                                                  ->variant_quantities
                                                  .capacity()) +
                               ",successors:" +
                               std::to_string(
                                   transition_cache->successors.capacity()) +
                               ",choices:" +
                               std::to_string(
                                   transition_cache->choices.capacity()) +
                               ",choice_successors:" +
                               std::to_string(transition_cache
                                                  ->choice_successors
                                                  .capacity()) +
                               ",choice_options:" +
                               std::to_string(transition_cache
                                                  ->choice_options
                                                  .capacity()));
            }
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

    template <typename T>
    void reserve_selected_growth(
        std::vector<T>& values, const std::size_t additional) {
        const std::size_t capacity =
            selected_growth_capacity(values, additional);
        if (capacity == values.capacity()) return;
        const std::uint64_t added =
            static_cast<std::uint64_t>(capacity - values.capacity()) *
            sizeof(T);
        if (check_solver_byte_cap_fast(added)) {
            throw SolverResourceLimit(
                "max_solver_owned_bytes", options.max_solver_owned_bytes);
        }
        values.reserve(capacity);
    }

    std::pair<bool, std::uint64_t> append_sparse_row(
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
            !options.kernel_reuse ||
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
        } else {
            transition_count = transitions.size();
            const auto self = std::lower_bound(
                transitions.begin(), transitions.end(), state,
                [](const OutcomeEntry& entry, const std::uint32_t value) {
                    return entry.state < value;
                });
            if (self != transitions.end() && self->state == state) {
                self_probability = self->probability;
            }
        }
        const std::size_t kernel_transition_count =
            static_cast<std::size_t>(transition_count);
        for (const OutcomeChoiceGroup& group : choices) {
            for (const std::uint32_t successor : group.states) {
                if (!is_self(successor)) ++transition_count;
            }
        }
        const std::size_t hash =
            identity_found == shared_kernel_rows.end()
                ? pending.exact_kernel_hash.has_value()
                      ? *pending.exact_kernel_hash
                      : kernel_hash(pending)
                : 0;
        const SparseRow* shared_kernel = nullptr;
        KernelRowMemo* shared_hash_memo = nullptr;
        if (identity_found != shared_kernel_rows.end()) {
            shared_kernel = &transition_cache->rows.at(
                identity_found->second.row_index);
        } else if (options.kernel_reuse) {
            const auto found = kernel_rows_by_hash.find(hash);
            if (found != kernel_rows_by_hash.end()) {
                for (KernelRowMemo& memo : found->second) {
                    const SparseRow& candidate =
                        transition_cache->rows.at(memo.row_index);
                    if (same_kernel(
                            candidate, pending, kernel_transition_count,
                            self_probability)) {
                        shared_kernel = &candidate;
                        shared_hash_memo = &memo;
                        break;
                    }
                }
            }
        }
        SparseRow* equivalent = nullptr;
        if (shared_kernel != nullptr) {
            for (std::uint32_t i = 0; i < span.count; ++i) {
                SparseRow& stored = transition_cache->rows.at(span.offset + i);
                if (stored.transition_offset !=
                        shared_kernel->transition_offset ||
                    stored.transition_count !=
                        shared_kernel->transition_count ||
                    stored.choice_offset != shared_kernel->choice_offset ||
                    stored.choice_count != shared_kernel->choice_count ||
                    stored.self_probability != self_probability) {
                    continue;
                }
                equivalent = &stored;
                break;
            }
        } else {
            for (std::uint32_t i = 0; i < span.count; ++i) {
                SparseRow& stored = transition_cache->rows.at(span.offset + i);
                if (!same_kernel(
                        stored, pending, kernel_transition_count,
                        self_probability)) {
                    continue;
                }
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
            row.variant_offset = transition_cache->variant_arena
                                     ->row_variant_indices.size();
            row.self_probability = self_probability;
            row.embedded_self_probability =
                pending.entry_relative_self ? 0.0 : self_probability;
            row.self_probability_embedded =
                row.embedded_self_probability > 0.0;
            const bool shareable_successor_envelope =
                !pending.entry_relative_self && choices.empty();
            const CarrierSuccessorEnvelope* cached_envelope = nullptr;
            if (identity_found != shared_kernel_rows.end() &&
                identity_found->second.successor_envelope.has_value()) {
                cached_envelope =
                    &*identity_found->second.successor_envelope;
            } else if (shareable_successor_envelope &&
                       shared_hash_memo != nullptr &&
                       shared_hash_memo->successor_envelope.has_value()) {
                cached_envelope =
                    &*shared_hash_memo->successor_envelope;
            }
            std::optional<CarrierSuccessorEnvelope> new_kernel_envelope;
            if (cached_envelope != nullptr) {
                row.preservation_effect = carrier_effect(
                    calc, state, *cached_envelope);
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
                CarrierSuccessorEnvelope envelope =
                    carrier_successor_envelope(
                        calc, std::move(effect_successors));
                row.preservation_effect = carrier_effect(
                    calc, state, envelope);
                if (shareable_successor_envelope) {
                    if (identity_found != shared_kernel_rows.end()) {
                        identity_found->second.successor_envelope = envelope;
                    } else if (shared_hash_memo != nullptr) {
                        shared_hash_memo->successor_envelope = envelope;
                    } else {
                        new_kernel_envelope = std::move(envelope);
                    }
                }
            }
            if (shared_kernel != nullptr) {
                ++result.diagnostics.exact_kernel_payload_reuses;
                result.diagnostics.exact_kernel_payload_bytes_saved +=
                    transition_count *
                        (sizeof(std::uint32_t) + sizeof(double)) +
                    choices.size() * sizeof(SparseChoiceGroup);
                for (const OutcomeChoiceGroup& group : choices) {
                    result.diagnostics.exact_kernel_payload_bytes_saved +=
                        group.states.size() * sizeof(std::uint32_t);
                }
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
                reserve_selected_growth(
                    transition_cache->successors, transition_count);
                reserve_selected_growth(
                    transition_cache->probabilities, transition_count);
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
                reserve_selected_growth(
                    transition_cache->choices, choices.size());
                std::size_t choice_successor_count = 0;
                for (const OutcomeChoiceGroup& group : choices) {
                    choice_successor_count += group.states.size();
                }
                reserve_selected_growth(
                    transition_cache->choice_successors,
                    choice_successor_count);
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
            reserve_selected_growth(transition_cache->rows, 1);
            transition_cache->rows.push_back(row);
            stored_row = &transition_cache->rows.back();
            if (shared_kernel == nullptr && options.kernel_reuse) {
                auto& bucket = kernel_rows_by_hash[hash];
                const std::size_t old_capacity = bucket.capacity();
                bucket.push_back(KernelRowMemo{
                    transition_cache->rows.size() - 1,
                    std::move(new_kernel_envelope)});
                owned_kernel_row_bucket_bytes +=
                    (bucket.capacity() - old_capacity) *
                    sizeof(KernelRowMemo);
                if (pending.shared_kernel_identity != nullptr) {
                    shared_kernel_rows.emplace(
                        pending.shared_kernel_identity,
                        SharedKernelMemo{
                            transition_cache->rows.size() - 1, false,
                            bucket.back().successor_envelope});
                }
            }
            ++span.count;
        }
        if (pending.shared_kernel_identity != nullptr &&
            shared_kernel_rows.find(pending.shared_kernel_identity) ==
                shared_kernel_rows.end()) {
            std::optional<CarrierSuccessorEnvelope> successor_envelope;
            if (shared_hash_memo != nullptr &&
                shared_hash_memo->successor_envelope.has_value()) {
                successor_envelope = shared_hash_memo->successor_envelope;
            } else {
                std::vector<std::uint32_t> ids;
                ids.reserve(transitions.size());
                for (const OutcomeEntry& entry : transitions) {
                    if (entry.probability != 0.0) {
                        ids.push_back(entry.state);
                    }
                }
                successor_envelope = carrier_successor_envelope(
                    calc, std::move(ids));
            }
            shared_kernel_rows.emplace(
                pending.shared_kernel_identity,
                SharedKernelMemo{
                    static_cast<std::uint64_t>(
                        stored_row - transition_cache->rows.data()),
                    false, std::move(successor_envelope)});
        }

        SparseVariant variant;
        variant.operator_index = pending.operator_index;
        variant.quantity_offset = transition_cache->variant_arena
                                      ->variant_quantities.size();
        const PricedOperator& priced = operators.at(
            static_cast<std::size_t>(
                priced_operator_position.at(pending.operator_index)));
        reserve_selected_growth(
            transition_cache->variant_arena->variant_quantities,
            priced.resource_prices.size());
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
            transition_cache->variant_arena->variant_quantities.push_back(
                quantity);
        }
        variant.quantity_count = static_cast<std::uint32_t>(
            transition_cache->variant_arena->variant_quantities.size() -
            variant.quantity_offset);
        variant.choice_option_offset = transition_cache->choice_options.size();
        variant.choice_option_count = static_cast<std::uint32_t>(
            choice_options.size());
        reserve_selected_growth(
            transition_cache->choice_options, choice_options.size());
        for (OutcomeChoiceOption choice : choice_options) {
            if (pending.entry_relative_self && choice.state == kNoId) {
                choice.state = state;
            }
            transition_cache->choice_options.push_back(choice);
        }
        reserve_selected_growth(
            transition_cache->variant_arena->variants, 1);
        transition_cache->variant_arena->variants.push_back(variant);
        const std::uint32_t variant_index = static_cast<std::uint32_t>(
            transition_cache->variant_arena->variants.size() - 1);
        if (stored_row->variant_count == stored_row->variant_capacity) {
            const std::uint32_t grown_capacity =
                stored_row->variant_capacity == 0
                    ? 4
                    : stored_row->variant_capacity * 2;
            if (grown_capacity < stored_row->variant_capacity) {
                throw SolverResourceLimit(
                    "max_state_action_rows",
                    options.max_state_action_rows);
            }
            const std::uint64_t grown_offset =
                transition_cache->variant_arena->row_variant_indices.size();
            reserve_selected_growth(
                transition_cache->variant_arena->row_variant_indices,
                grown_capacity);
            transition_cache->variant_arena->row_variant_indices.resize(
                transition_cache->variant_arena->row_variant_indices.size() +
                    grown_capacity,
                kNoId);
            for (std::uint32_t i = 0; i < stored_row->variant_count; ++i) {
                transition_cache->variant_arena->row_variant_indices.at(
                    grown_offset + i) =
                    transition_cache->variant_arena->row_variant_indices.at(
                        stored_row->variant_offset + i);
            }
            stored_row->variant_offset = grown_offset;
            stored_row->variant_capacity = grown_capacity;
        }
        transition_cache->variant_arena->row_variant_indices.at(
            stored_row->variant_offset + stored_row->variant_count) =
            variant_index;
        ++stored_row->variant_count;

        result.diagnostics.sparse_rows = transition_cache->rows.size();
        result.diagnostics.sparse_transitions =
            transition_cache->successors.size() +
            transition_cache->choice_successors.size();
        const std::size_t stored_row_index = static_cast<std::size_t>(
            stored_row - transition_cache->rows.data());
        if (priced_rows.size() < transition_cache->rows.size()) {
            reserve_selected_growth(
                priced_rows,
                transition_cache->rows.size() - priced_rows.size());
            priced_rows.resize(transition_cache->rows.size());
        }
        /* Expansion appends exactly one new variant at this boundary. Price
         * that variant incrementally instead of rescanning every previously
         * retained variant on the row. Large equivalence rows can carry more
         * than a thousand action/resource variants; the old rescan made one
         * carrier quadratic in its admitted action count. A later economy
         * solve still rebuilds all row prices once in prepare_priced_rows(). */
        PricedSparseRow& selected = priced_rows.at(stored_row_index);
        const SparseVariant& appended_variant =
            transition_cache->variant_arena->variants.at(variant_index);
        double appended_cost = 0.0;
        if (priced_variant_cost(appended_variant, appended_cost) &&
            (appended_cost < selected.cost - 1e-12 ||
             (std::abs(appended_cost - selected.cost) <= 1e-12 &&
              appended_variant.operator_index < selected.operator_index))) {
            selected.operator_index = appended_variant.operator_index;
            selected.cost = appended_cost;
            selected.choice_option_offset =
                appended_variant.choice_option_offset;
            selected.choice_option_count =
                appended_variant.choice_option_count;
        }

        bool enqueue_fringe = !focused_mode;
        if (enqueue_fringe && pending.shared_kernel_identity != nullptr) {
            SharedKernelMemo& memo =
                shared_kernel_rows.at(pending.shared_kernel_identity);
            enqueue_fringe = !memo.fringe_enqueued;
            memo.fringe_enqueued = true;
        }
        if (enqueue_fringe) {
            for (const OutcomeEntry& entry : transitions) {
                if (is_self(entry.state)) continue;
                if (pending.operator_index == restart_operator_index) {
                    /* Establish the executable clean-base continuation before
                     * the broad stochastic fringe. This changes only the work
                     * schedule; every strict state and all-action row remains
                     * available to the same exact focused proof. */
                    enqueue_front(entry.state);
                } else {
                    enqueue(entry.state);
                }
            }
            for (const OutcomeChoiceGroup& group : choices) {
                for (const std::uint32_t successor : group.states) {
                    if (!is_self(successor)) enqueue(successor);
                }
            }
        }
        return {
            equivalent != nullptr,
            static_cast<std::uint64_t>(stored_row_index)};
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
                            pending.exact_kernel_hash = kernel_hash(pending);
                            automatic_record->evidence.candidate_kernel_hash =
                                static_cast<std::uint64_t>(
                                    *pending.exact_kernel_hash);
                        }
                        const std::uint64_t rows_before =
                            transition_cache->rows.size();
                        const std::uint64_t transitions_before =
                            transition_cache->successors.size() +
                            transition_cache->choice_successors.size();
                        const auto row_byte_audit_started =
                            std::chrono::steady_clock::now();
                        const std::uint64_t bytes_before =
                            transition_cache->fast_estimated_owned_bytes();
                        result.diagnostics.expansion_row_byte_audit_ns +=
                            static_cast<std::uint64_t>(
                                std::chrono::duration_cast<
                                    std::chrono::nanoseconds>(
                                    std::chrono::steady_clock::now() -
                                    row_byte_audit_started)
                                    .count());
                        const auto sparse_row_started =
                            std::chrono::steady_clock::now();
                        const auto [collapsed, appended_row] =
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
                                transition_cache->fast_estimated_owned_bytes();
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
                        try_constructive_state_certificate(
                            state, appended_row);
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
            cost += transition_cache->variant_arena->variant_quantities.at(
                        variant.quantity_offset + quantity) *
                    priced.resource_prices[quantity].second;
        }
        return std::isfinite(cost);
    }

    void update_priced_row(const std::size_t row_index) {
        const SparseRow& row = transition_cache->rows.at(row_index);
        PricedSparseRow selected;
        for (std::uint32_t i = 0; i < row.variant_count; ++i) {
            const SparseVariant& variant =
                transition_cache->variant_arena->variants.at(
                transition_cache->variant_arena->row_variant_indices.at(
                    row.variant_offset + i));
            if (variant.operator_index == kNoId) {
                throw std::logic_error(
                    "priced sparse row has no operator: row=" +
                    std::to_string(row_index) + " variant=" +
                    std::to_string(i) + " variant_offset=" +
                    std::to_string(row.variant_offset));
            }
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
        const std::size_t first_unpriced = 0;
        priced_rows.assign(
            transition_cache->rows.size(), PricedSparseRow{});
        for (std::size_t row_index = 0;
             row_index < transition_cache->rows.size(); ++row_index) {
            update_priced_row(row_index);
        }
        if (restart_state == kNoId && restart_operator_index != kNoId) {
            for (const SparseRow& row : transition_cache->rows) {
                bool has_restart = false;
                for (std::uint32_t i = 0; i < row.variant_count; ++i) {
                    const SparseVariant& variant =
                        transition_cache->variant_arena->variants.at(
                            transition_cache->variant_arena
                                ->row_variant_indices.at(
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
                const SparseVariant& variant =
                    transition_cache->variant_arena->variants.at(
                    transition_cache->variant_arena->row_variant_indices.at(
                        row.variant_offset + i));
                if (variant.operator_index == kNoId) {
                    throw std::logic_error(
                        "pricing diagnostics row has no operator: row=" +
                        std::to_string(row_index) + " variant=" +
                        std::to_string(i) + " variant_offset=" +
                        std::to_string(row.variant_offset) +
                        " first_unpriced=" +
                        std::to_string(first_unpriced));
                }
                double unused_cost = 0.0;
                if (priced_variant_cost(variant, unused_cost)) {
                    ++priced_variant_count;
                }
            }
            if (priced_variant_count <= 1) continue;
            if (selected.operator_index == kNoId) {
                throw std::logic_error(
                    "pricing diagnostics row has no selected operator: row=" +
                    std::to_string(row_index) + " variants=" +
                    std::to_string(row.variant_count) + " priced=" +
                    std::to_string(priced_variant_count) +
                    " first_unpriced=" +
                    std::to_string(first_unpriced));
            }
            result.diagnostics.equivalent_actions_collapsed +=
                priced_variant_count - 1;
            const std::string& representative = calc.operators().at(
                selected.operator_index).id;
            for (std::uint32_t i = 0; i < row.variant_count; ++i) {
                const SparseVariant& variant =
                    transition_cache->variant_arena->variants.at(
                    transition_cache->variant_arena->row_variant_indices.at(
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

    static void signature_string(
        std::vector<std::uint64_t>& out,
        const std::string& value) {
        out.push_back(value.size());
        for (const unsigned char byte : value) out.push_back(byte);
    }

    void planner_observation_signature(
        std::vector<std::uint64_t>& out,
        const PlannerOperator& planner) const {
        out.push_back(static_cast<std::uint8_t>(planner.kind));
        out.push_back(static_cast<std::uint8_t>(planner.option_kind));
        out.push_back(static_cast<std::uint8_t>(planner.automatic_kind));
        out.push_back(static_cast<std::uint8_t>(planner.intended_side + 1));
        out.push_back(planner.primitive_action);
        out.push_back(planner.exit_min_satisfied);
        out.push_back(planner.carrier_goal_slot);
        out.push_back(planner.conditional_action);
        out.push_back(planner.bestiary_create_action);
        out.push_back(planner.bestiary_restore_action);
        out.push_back(planner.relevant_goal_mask);
        out.push_back(planner.setup_action);
        out.push_back(planner.followup_action);
        out.push_back(planner.cleanup_action);
        out.push_back(planner.primitive_program.size());
        for (const std::uint32_t action : planner.primitive_program) {
            out.push_back(action);
        }
        out.push_back(planner.exit_goal_slots.size());
        for (const std::uint32_t slot : planner.exit_goal_slots) {
            out.push_back(slot);
        }
        out.push_back(planner.resource_quantities.size());
        for (const auto& [key, quantity] : planner.resource_quantities) {
            signature_string(out, key);
            out.push_back(std::bit_cast<std::uint64_t>(quantity));
        }
    }

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
        const std::vector<std::uint32_t>& partition) const {
        const double detached_self_probability =
            row.self_probability - row.embedded_self_probability;
        bool observes_owner_class = detached_self_probability > 0.0;
        for (std::uint32_t i = 0;
             !observes_owner_class && i < row.choice_count; ++i) {
            observes_owner_class =
                graph.choices.at(row.choice_offset + i).has_self;
        }
        std::vector<std::uint64_t> key{
            row.transition_offset,
            row.transition_count,
            row.choice_offset,
            row.choice_count,
            observes_owner_class ? partition.at(row.owner_state) : kNoId,
            std::bit_cast<std::uint64_t>(detached_self_probability)};
        std::vector<std::vector<std::uint64_t>> variants;
        variants.reserve(row.variant_count);
        for (std::uint32_t i = 0; i < row.variant_count; ++i) {
            const SparseVariant& variant = graph.variant_arena->variants.at(
                graph.variant_arena->row_variant_indices.at(
                    row.variant_offset + i));
            std::vector<std::uint64_t> tokens;
            planner_observation_signature(
                tokens, calc.operators().at(variant.operator_index));
            const std::int32_t priced_position =
                priced_operator_position.at(variant.operator_index);
            if (priced_position < 0) {
                throw std::logic_error(
                    "quotient row contains an unpriced operator");
            }
            const PricedOperator& priced =
                operators.at(static_cast<std::size_t>(priced_position));
            tokens.push_back(variant.quantity_count);
            if (variant.quantity_count != priced.resource_prices.size()) {
                throw std::logic_error(
                    "quotient resource observation is incompatible");
            }
            for (std::uint32_t q = 0; q < variant.quantity_count; ++q) {
                signature_string(tokens, priced.resource_prices[q].first);
                tokens.push_back(std::bit_cast<std::uint64_t>(
                    graph.variant_arena->variant_quantities.at(
                        variant.quantity_offset + q)));
            }
            tokens.push_back(variant.choice_option_count);
            for (std::uint32_t c = 0;
                 c < variant.choice_option_count; ++c) {
                const OutcomeChoiceOption& choice = graph.choice_options.at(
                    variant.choice_option_offset + c);
                const auto observed_class = [&](const std::uint32_t state) {
                    return state == kNoId ? kNoId : partition.at(state);
                };
                tokens.push_back(choice.mod_id);
                tokens.push_back(observed_class(choice.state));
                tokens.push_back(observed_class(choice.observation_state));
                tokens.push_back(observed_class(choice.actual_state));
            }
            variants.push_back(std::move(tokens));
        }
        std::sort(variants.begin(), variants.end());
        key.push_back(variants.size());
        for (const auto& variant : variants) {
            key.push_back(variant.size());
            key.insert(key.end(), variant.begin(), variant.end());
        }
        return key;
    }

    void sort_projection_classes(
        std::vector<std::uint32_t>& classes,
        RowObservationCache& cache) const {
        if (classes.size() < 4096) {
            std::sort(classes.begin(), classes.end());
            return;
        }
        cache.radix_scratch.resize(classes.size());
        const auto radix_pass = [&cache](
            const std::vector<std::uint32_t>& source,
            std::vector<std::uint32_t>& target,
            const std::uint32_t shift) {
            cache.radix_counts.fill(0);
            for (const std::uint32_t value : source) {
                ++cache.radix_counts[(value >> shift) & 0xffffu];
            }
            std::uint32_t offset = 0;
            for (std::uint32_t& count : cache.radix_counts) {
                const std::uint32_t next = offset + count;
                count = offset;
                offset = next;
            }
            for (const std::uint32_t value : source) {
                target[cache.radix_counts[(value >> shift) & 0xffffu]++] =
                    value;
            }
        };
        radix_pass(classes, cache.radix_scratch, 0);
        radix_pass(cache.radix_scratch, classes, 16);
    }

    void fill_kernel_projection(
        const SolveTransitionCache& graph,
        const std::array<std::uint64_t, 6>& key,
        const std::vector<std::uint32_t>& partition,
        RowObservationCache& cache,
        const bool secondary) const {
        auto& sums = secondary ? cache.secondary_transition_sums
                               : cache.transition_sums;
        auto& epochs = secondary ? cache.secondary_transition_epochs
                                 : cache.transition_epochs;
        auto& touched = secondary ? cache.secondary_touched_classes
                                  : cache.touched_classes;
        auto& epoch = secondary ? cache.secondary_transition_epoch
                                : cache.transition_epoch;
        if (sums.size() < partition.size()) {
            sums.resize(partition.size());
            epochs.resize(partition.size(), 0);
        }
        if (++epoch == 0) {
            std::fill(epochs.begin(), epochs.end(), 0);
            ++epoch;
        }
        touched.clear();
        const double self_probability =
            std::bit_cast<double>(key[5]);
        if (self_probability > 0.0) {
            const std::uint32_t owner_class =
                static_cast<std::uint32_t>(key[4]);
            epochs[owner_class] = epoch;
            sums[owner_class] = WideFloat{self_probability};
            touched.push_back(owner_class);
        }
        for (std::uint64_t i = 0; i < key[1]; ++i) {
            const std::uint64_t offset = key[0] + i;
            const std::uint32_t successor_class =
                partition.at(graph.successors.at(offset));
            if (epochs[successor_class] != epoch) {
                epochs[successor_class] = epoch;
                sums[successor_class] = WideFloat{0.0};
                touched.push_back(successor_class);
            }
            sums[successor_class] = sums[successor_class] +
                                    WideFloat{graph.probabilities.at(offset)};
        }
        sort_projection_classes(touched, cache);
    }

    std::vector<std::uint32_t> projected_choice_classes(
        const SolveTransitionCache& graph,
        const SparseChoiceGroup& group,
        const std::vector<std::uint32_t>& partition) const {
        std::vector<std::uint32_t> classes;
        classes.reserve(group.successor_count);
        for (std::uint32_t s = 0; s < group.successor_count; ++s) {
            classes.push_back(partition.at(
                graph.choice_successors.at(group.successor_offset + s)));
        }
        std::sort(classes.begin(), classes.end());
        classes.erase(
            std::unique(classes.begin(), classes.end()), classes.end());
        return classes;
    }

    std::uint64_t kernel_projection_hash(
        const SolveTransitionCache& graph,
        const std::array<std::uint64_t, 6>& key,
        const std::vector<std::uint32_t>& partition,
        RowObservationCache& cache) const {
        fill_kernel_projection(graph, key, partition, cache, false);
        std::uint64_t hash = 1469598103934665603ull;
        const auto append = [&hash](const std::uint64_t token) {
            hash ^= token;
            hash *= 1099511628211ull;
        };
        append(cache.touched_classes.size());
        for (const std::uint32_t successor_class : cache.touched_classes) {
            append(successor_class);
            append(std::bit_cast<std::uint64_t>(
                cache.transition_sums[successor_class].value()));
        }
        append(key[3]);
        for (std::uint64_t i = 0; i < key[3]; ++i) {
            const SparseChoiceGroup& group = graph.choices.at(key[2] + i);
            append(std::bit_cast<std::uint64_t>(group.probability));
            std::vector<std::uint32_t> classes =
                projected_choice_classes(graph, group, partition);
            if (group.has_self) {
                classes.push_back(static_cast<std::uint32_t>(key[4]));
                std::sort(classes.begin(), classes.end());
                classes.erase(
                    std::unique(classes.begin(), classes.end()),
                    classes.end());
            }
            append(classes.size());
            for (const std::uint32_t value : classes) append(value);
        }
        return hash;
    }

    bool same_kernel_projection(
        const SolveTransitionCache& graph,
        const std::array<std::uint64_t, 6>& current,
        const std::array<std::uint64_t, 6>& candidate,
        const std::vector<std::uint32_t>& partition,
        RowObservationCache& cache) const {
        fill_kernel_projection(graph, candidate, partition, cache, true);
        if (cache.touched_classes != cache.secondary_touched_classes) {
            return false;
        }
        for (const std::uint32_t successor_class : cache.touched_classes) {
            if (cache.transition_sums[successor_class].value() !=
                cache.secondary_transition_sums[successor_class].value()) {
                return false;
            }
        }
        if (current[3] != candidate[3]) return false;
        for (std::uint64_t i = 0; i < current[3]; ++i) {
            const SparseChoiceGroup& left = graph.choices.at(current[2] + i);
            const SparseChoiceGroup& right =
                graph.choices.at(candidate[2] + i);
            if (left.probability != right.probability) {
                return false;
            }
            std::vector<std::uint32_t> left_classes =
                projected_choice_classes(graph, left, partition);
            std::vector<std::uint32_t> right_classes =
                projected_choice_classes(graph, right, partition);
            if (left.has_self) {
                left_classes.push_back(
                    static_cast<std::uint32_t>(current[4]));
            }
            if (right.has_self) {
                right_classes.push_back(
                    static_cast<std::uint32_t>(candidate[4]));
            }
            std::sort(left_classes.begin(), left_classes.end());
            left_classes.erase(
                std::unique(left_classes.begin(), left_classes.end()),
                left_classes.end());
            std::sort(right_classes.begin(), right_classes.end());
            right_classes.erase(
                std::unique(right_classes.begin(), right_classes.end()),
                right_classes.end());
            if (left_classes != right_classes) return false;
        }
        return true;
    }

    std::uint32_t intern_kernel_projection(
        const SolveTransitionCache& graph,
        const std::array<std::uint64_t, 6>& key,
        const std::vector<std::uint32_t>& partition,
        RowObservationCache& cache) const {
        std::uint64_t key_hash = 1469598103934665603ull;
        for (const std::uint64_t token : key) {
            key_hash ^= token;
            key_hash *= 1099511628211ull;
        }
        auto& key_candidates = cache.kernel_projection_buckets[key_hash];
        for (const KernelProjectionMemo& memo : key_candidates) {
            if (memo.exact_key == key) return memo.class_id;
        }

        const std::uint64_t behavior_hash =
            kernel_projection_hash(graph, key, partition, cache);
        auto& behavior_candidates =
            cache.kernel_projection_behavior_buckets[behavior_hash];
        std::uint32_t class_id = kNoId;
        for (const KernelProjectionRepresentative& candidate :
             behavior_candidates) {
            if (same_kernel_projection(
                    graph, key, candidate.exact_key, partition, cache)) {
                class_id = candidate.class_id;
                break;
            }
        }
        if (class_id == kNoId) {
            class_id = cache.next_kernel_projection_class_id++;
            behavior_candidates.push_back({key, class_id});
        }
        key_candidates.push_back({key, class_id});
        return class_id;
    }

    std::vector<std::uint64_t> row_behavior_signature(
        const SolveTransitionCache& graph,
        const SparseRow& row,
        const std::vector<std::uint32_t>& partition,
        RowObservationCache& cache,
        const std::vector<std::uint64_t>& exact_row_key) const {
        const double detached_self_probability =
            row.self_probability - row.embedded_self_probability;
        bool observes_owner_class = detached_self_probability > 0.0;
        for (std::uint32_t i = 0;
             !observes_owner_class && i < row.choice_count; ++i) {
            observes_owner_class =
                graph.choices.at(row.choice_offset + i).has_self;
        }
        const std::uint32_t observed_owner_class =
            observes_owner_class ? partition.at(row.owner_state) : kNoId;
        const std::array<std::uint64_t, 6> projection_key{
            row.transition_offset,
            row.transition_count,
            row.choice_offset,
            row.choice_count,
            observed_owner_class,
            std::bit_cast<std::uint64_t>(detached_self_probability)};
        const std::uint32_t projection_class = intern_kernel_projection(
            graph, projection_key, partition, cache);

        std::vector<std::uint64_t> out;
        out.reserve(1 + exact_row_key.size() - 6);
        out.push_back(projection_class);
        out.insert(
            out.end(), exact_row_key.begin() + 6, exact_row_key.end());
        return out;
    }

    std::uint32_t intern_row_behavior(
        const SolveTransitionCache& graph,
        const std::uint32_t row_index,
        const std::vector<std::uint32_t>& partition,
        RowObservationCache& cache) const {
        const SparseRow& row = graph.rows.at(row_index);
        const std::vector<std::uint64_t> key =
            row_observation_cache_key(graph, row, partition);
        const std::uint64_t key_hash = observation_signature_hash(key);
        auto& key_candidates = cache.exact_key_buckets[key_hash];
        for (const RowObservationRepresentative& candidate : key_candidates) {
            if (row_observation_cache_key(
                    graph, graph.rows.at(candidate.row_index), partition) ==
                key) {
                return candidate.class_id;
            }
        }

        const std::vector<std::uint64_t> signature =
            row_behavior_signature(graph, row, partition, cache, key);
        const std::uint64_t behavior_hash =
            observation_signature_hash(signature);
        auto& behavior_candidates = cache.behavior_buckets[behavior_hash];
        for (const RowObservationRepresentative& candidate :
             behavior_candidates) {
            const std::vector<std::uint64_t> candidate_key =
                row_observation_cache_key(
                    graph, graph.rows.at(candidate.row_index), partition);
            if (row_behavior_signature(
                    graph, graph.rows.at(candidate.row_index), partition,
                    cache, candidate_key) ==
                signature) {
                key_candidates.push_back({row_index, candidate.class_id});
                return candidate.class_id;
            }
        }
        const std::uint32_t class_id = cache.next_class_id++;
        behavior_candidates.push_back({row_index, class_id});
        key_candidates.push_back({row_index, class_id});
        return class_id;
    }

    std::vector<std::uint64_t> state_behavior_signature(
        const SolveTransitionCache& graph,
        const std::uint32_t state,
        const std::vector<std::uint32_t>& partition,
        RowObservationCache* cache) const {
        std::vector<std::uint64_t> out;
        out.push_back(calc.is_goal_state(calc.state(state)) ? 1u : 0u);
        out.push_back(
            state < graph.expanded.size() && graph.expanded[state] ? 1u : 0u);
        /* Refinement is monotone: a round may split an existing candidate
         * class but can never merge two classes that the exact coarse
         * observation already distinguished. This also makes termination
         * independent of incidental numeric class renumbering. */
        out.push_back(partition.at(state));
        const StateRowSpan span = state < graph.state_rows.size()
                                      ? graph.state_rows[state]
                                      : StateRowSpan{};
        std::vector<std::uint32_t> rows;
        rows.reserve(span.count);
        for (std::uint32_t i = 0; i < span.count; ++i) {
            rows.push_back(intern_row_behavior(
                graph, span.offset + i, partition, *cache));
        }
        std::sort(rows.begin(), rows.end());
        out.push_back(rows.size());
        for (const std::uint32_t row_class : rows) out.push_back(row_class);
        return out;
    }

    std::vector<std::uint64_t> coarse_state_signature(
        const std::uint32_t state_id) const {
        const AbstractState& state = calc.state(state_id);
        /* Exact probabilistic bisimulation needs only terminal observation as
         * its seed. Every non-terminal mechanic fact (including rarity,
         * affix counts, carriers, groups, influence, and junk identity) is
         * observable only through admission or an action kernel, and the
         * complete row multiset is collision-checked in every refinement
         * round. Starting from the literal representation would make the
         * monotone refinement unable to prove an unobserved difference
         * irrelevant. */
        return {calc.is_goal_state(state) ? 1u : 0u};
    }

    std::vector<std::uint64_t> focused_schedule_signature(
        const std::uint32_t state_id) {
        const AbstractState& state = calc.state(state_id);
        /* Scheduling classes are not equivalence classes and never merge a
         * state. Preserve representatives from distinct exact goal-progress
         * and affix-capacity regions so a large renewal outcome set cannot
         * make its most common zero-progress carriers monopolize a round. */
        return {
            calc.is_goal_state(state) ? 1u : 0u,
            satisfied_goal_mask_for_state(state_id),
            carrier_facts(state).goal_family_mask,
            state.blocked_mask,
            state.rarity,
            state.prefix_count,
            state.suffix_count,
            static_cast<std::uint64_t>(
                state.flags &
                (kFlagCraftedMod | kFlagPrefixesLocked |
                 kFlagSuffixesLocked))};
    }

    static std::uint64_t observation_signature_hash(
        const std::vector<std::uint64_t>& signature) {
        std::uint64_t hash = 1469598103934665603ull;
        for (const std::uint64_t token : signature) {
            hash ^= token;
            hash *= 1099511628211ull;
        }
        return hash;
    }

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
        const std::uint32_t right) const {
        const StateRowSpan left_span = graph.state_rows.at(left);
        const StateRowSpan right_span = graph.state_rows.at(right);
        std::string action = "action_availability_or_projected_successor";
        const auto first_action = [&](const StateRowSpan span)
            -> std::string {
            if (span.count == 0) return {};
            const SparseRow& row = graph.rows.at(span.offset);
            if (row.variant_count == 0) return {};
            const SparseVariant& variant = graph.variant_arena->variants.at(
                graph.variant_arena->row_variant_indices.at(
                    row.variant_offset));
            return calc.operators().at(variant.operator_index).id;
        };
        const std::string left_action = first_action(left_span);
        const std::string right_action = first_action(right_span);
        if (!left_action.empty()) action = left_action;
        else if (!right_action.empty()) action = right_action;
        return "{\"left_state\":" + std::to_string(left) +
               ",\"right_state\":" + std::to_string(right) +
               ",\"action\":\"" + action +
               "\",\"reason\":\"exact_all_action_partition_split\"}";
    }

    void collect_action_observation_cardinalities(
        const SolveTransitionCache& graph) {
        using ExactBuckets = std::unordered_map<
            std::uint64_t, std::vector<std::vector<std::uint64_t>>>;
        std::vector<ExactBuckets> observed(calc.operators().size());
        for (const SparseRow& row : graph.rows) {
            for (std::uint32_t i = 0; i < row.variant_count; ++i) {
                const SparseVariant& variant =
                    graph.variant_arena->variants.at(
                    graph.variant_arena->row_variant_indices.at(
                        row.variant_offset + i));
                std::vector<std::uint64_t> signature{
                    row.transition_offset,
                    row.transition_count,
                    row.choice_offset,
                    row.choice_count,
                    std::bit_cast<std::uint64_t>(row.self_probability),
                    variant.quantity_count};
                for (std::uint32_t q = 0; q < variant.quantity_count; ++q) {
                    signature.push_back(std::bit_cast<std::uint64_t>(
                        graph.variant_arena->variant_quantities.at(
                            variant.quantity_offset + q)));
                }
                signature.push_back(variant.choice_option_count);
                for (std::uint32_t c = 0;
                     c < variant.choice_option_count; ++c) {
                    const OutcomeChoiceOption& choice = graph.choice_options.at(
                        variant.choice_option_offset + c);
                    signature.push_back(choice.mod_id);
                    signature.push_back(choice.state);
                    signature.push_back(choice.observation_state);
                    signature.push_back(choice.actual_state);
                }
                ExactBuckets& action = observed.at(variant.operator_index);
                const std::uint64_t hash = observation_signature_hash(signature);
                auto& collisions = action[hash];
                if (std::find(collisions.begin(), collisions.end(), signature) ==
                    collisions.end()) {
                    collisions.push_back(std::move(signature));
                }
            }
        }
        result.diagnostics.action_observation_cardinalities.clear();
        for (std::uint32_t operator_index = 0;
             operator_index < observed.size(); ++operator_index) {
            std::uint64_t cardinality = 0;
            for (const auto& [unused_hash, collisions] :
                 observed[operator_index]) {
                (void)unused_hash;
                cardinality += collisions.size();
            }
            if (cardinality == 0) continue;
            std::string entry = "{\"action\":";
            append_json_string(entry, calc.operators().at(operator_index).id);
            entry += ",\"strict_observation_signatures\":" +
                     std::to_string(cardinality) + "}";
            result.diagnostics.action_observation_cardinalities.push_back(
                std::move(entry));
        }
    }

    std::vector<std::uint64_t> shadow_state_signature(
        const SolveTransitionCache& graph,
        const std::uint32_t state) const {
        std::vector<std::uint64_t> out;
        out.push_back(calc.is_goal_state(calc.state(state)) ? 1u : 0u);
        out.push_back(
            state < graph.expanded.size() && graph.expanded[state] ? 1u : 0u);
        const StateRowSpan span = state < graph.state_rows.size()
                                      ? graph.state_rows[state]
                                      : StateRowSpan{};
        std::vector<std::vector<std::uint64_t>> rows;
        rows.reserve(span.count);
        for (std::uint32_t r = 0; r < span.count; ++r) {
            const SparseRow& row = graph.rows.at(span.offset + r);
            std::vector<std::uint64_t> signature{
                row.transition_offset,
                row.transition_count,
                row.choice_offset,
                row.choice_count,
                std::bit_cast<std::uint64_t>(row.self_probability),
                row.variant_count};
            for (std::uint32_t i = 0; i < row.variant_count; ++i) {
                const SparseVariant& variant =
                    graph.variant_arena->variants.at(
                    graph.variant_arena->row_variant_indices.at(
                        row.variant_offset + i));
                planner_observation_signature(
                    signature, calc.operators().at(variant.operator_index));
                signature.push_back(variant.quantity_count);
                for (std::uint32_t q = 0; q < variant.quantity_count; ++q) {
                    signature.push_back(std::bit_cast<std::uint64_t>(
                        graph.variant_arena->variant_quantities.at(
                            variant.quantity_offset + q)));
                }
                signature.push_back(variant.choice_option_count);
                for (std::uint32_t c = 0;
                     c < variant.choice_option_count; ++c) {
                    const OutcomeChoiceOption& choice = graph.choice_options.at(
                        variant.choice_option_offset + c);
                    signature.push_back(choice.mod_id);
                    signature.push_back(choice.state);
                    signature.push_back(choice.observation_state);
                    signature.push_back(choice.actual_state);
                }
            }
            rows.push_back(std::move(signature));
        }
        std::sort(rows.begin(), rows.end());
        out.push_back(rows.size());
        for (const auto& row : rows) {
            out.push_back(row.size());
            out.insert(out.end(), row.begin(), row.end());
        }
        return out;
    }

    void build_quotient_graph(
        const std::vector<std::uint32_t>& partition,
        const std::uint32_t class_count) {
        const std::shared_ptr<SolveTransitionCache> strict = transition_cache;
        const std::uint32_t strict_count = strict->discovered_states;
        std::vector<std::uint32_t> representative(class_count, kNoId);
        if (result.start_state < strict_count) {
            representative[partition[result.start_state]] = result.start_state;
        }
        for (std::uint32_t state = 0; state < strict_count; ++state) {
            if (representative[partition[state]] == kNoId) {
                representative[partition[state]] = state;
            }
        }
        result.behavioral_representative_by_state.resize(strict_count);
        for (std::uint32_t state = 0; state < strict_count; ++state) {
            result.behavioral_representative_by_state[state] =
                representative[partition[state]];
        }

        auto quotient = std::make_shared<SolveTransitionCache>();
        quotient->start_state = result.start_state;
        quotient->operator_indices = strict->operator_indices;
        quotient->max_states = strict->max_states;
        quotient->max_discovered_states = strict->max_discovered_states;
        quotient->max_expanded_states = strict->max_expanded_states;
        quotient->max_state_action_rows = strict->max_state_action_rows;
        quotient->max_transitions = strict->max_transitions;
        quotient->max_reforge_work = strict->max_reforge_work;
        quotient->max_solver_owned_bytes = strict->max_solver_owned_bytes;
        quotient->max_diagnostic_samples = strict->max_diagnostic_samples;
        quotient->full_evidence = strict->full_evidence;
        quotient->kernel_reuse = strict->kernel_reuse;
        quotient->discovered_states = strict_count;
        quotient->strict_discovered_states = strict_count;
        quotient->quotient_states = class_count;
        quotient->exact_quotient = true;
        quotient->behavioral_representative_by_state =
            result.behavioral_representative_by_state;
        quotient->expanded.assign(strict_count, 0);
        quotient->state_rows.resize(strict_count);
        /* Variant/resource payloads are independent of the successor
         * projection. Copy each strict arena once, then let quotient rows
         * retain their original slices. Re-copying a shared strict row's
         * variants for every behavioral representative turns exact kernel
         * reuse back into graph-sized duplication. Choice-option state IDs
         * are projected once below because those IDs are observable. */
        quotient->variant_arena = strict->variant_arena;
        quotient->accounts_variant_arena =
            focused_strict_transition_cache == nullptr;
        quotient->choice_options = strict->choice_options;
        for (OutcomeChoiceOption& choice : quotient->choice_options) {
            const auto map_state = [&](std::uint32_t& state) {
                if (state != kNoId) {
                    state = result.behavioral_representative_by_state.at(
                        state);
                }
            };
            map_state(choice.state);
            map_state(choice.observation_state);
            map_state(choice.actual_state);
        }
        quotient->automatic_rows_considered =
            strict->automatic_rows_considered;
        quotient->automatic_rows_eligible = strict->automatic_rows_eligible;
        quotient->automatic_rows_rejected = strict->automatic_rows_rejected;
        quotient->automatic_rows_collapsed = strict->automatic_rows_collapsed;
        quotient->automatic_rows_deferred = strict->automatic_rows_deferred;
        quotient->automatic_kind_telemetry =
            strict->automatic_kind_telemetry;
        quotient->automatic_admission_phases =
            strict->automatic_admission_phases;
        quotient->automatic_candidate_samples =
            strict->automatic_candidate_samples;
        quotient->owned_automatic_sample_nested_bytes =
            strict->owned_automatic_sample_nested_bytes;
        quotient->focused_partial = strict->focused_partial;
        std::map<
            std::pair<std::uint64_t, std::uint32_t>,
            std::pair<std::uint64_t, std::uint32_t>>
            projected_transition_spans;

        std::uint32_t representative_expanded = 0;
        for (const std::uint32_t owner : representative) {
            if (owner == kNoId || owner >= strict->expanded.size() ||
                !strict->expanded[owner]) {
                continue;
            }
            quotient->expanded[owner] = 1;
            ++representative_expanded;
            const StateRowSpan source_span = strict->state_rows.at(owner);
            StateRowSpan& target_span = quotient->state_rows[owner];
            target_span.offset = quotient->rows.size();
            for (std::uint32_t r = 0; r < source_span.count; ++r) {
                const SparseRow& source =
                    strict->rows.at(source_span.offset + r);
                SparseRow row = source;
                row.owner_state = owner;
                const auto source_transition_span = std::make_pair(
                    source.transition_offset, source.transition_count);
                auto projected = projected_transition_spans.find(
                    source_transition_span);
                if (projected == projected_transition_spans.end()) {
                    const std::uint64_t projected_offset =
                        quotient->successors.size();
                    std::map<std::uint32_t, WideFloat> mapped;
                    for (std::uint32_t i = 0;
                         i < source.transition_count; ++i) {
                        const std::uint64_t offset =
                            source.transition_offset + i;
                        const std::uint32_t successor =
                            result.behavioral_representative_by_state.at(
                                strict->successors.at(offset));
                        mapped[successor] = mapped[successor] +
                            WideFloat{strict->probabilities.at(offset)};
                    }
                    for (const auto& [successor, probability] : mapped) {
                        quotient->successors.push_back(successor);
                        quotient->probabilities.push_back(
                            probability.value());
                    }
                    projected = projected_transition_spans.emplace(
                        source_transition_span,
                        std::make_pair(
                            projected_offset,
                            static_cast<std::uint32_t>(mapped.size())))
                                    .first;
                }
                row.transition_offset = projected->second.first;
                row.transition_count = projected->second.second;
                row.embedded_self_probability = 0.0;
                for (std::uint32_t i = 0; i < row.transition_count; ++i) {
                    const std::uint64_t offset = row.transition_offset + i;
                    if (quotient->successors.at(offset) == owner) {
                        row.embedded_self_probability +=
                            quotient->probabilities.at(offset);
                    }
                }
                const double detached_self_probability =
                    source.self_probability -
                    source.embedded_self_probability;
                row.self_probability = detached_self_probability +
                                       row.embedded_self_probability;
                row.self_probability_embedded =
                    row.embedded_self_probability > 0.0;
                row.choice_offset = quotient->choices.size();
                for (std::uint32_t i = 0; i < source.choice_count; ++i) {
                    const SparseChoiceGroup& source_group =
                        strict->choices.at(source.choice_offset + i);
                    SparseChoiceGroup group;
                    group.successor_offset =
                        quotient->choice_successors.size();
                    group.probability = source_group.probability;
                    std::vector<std::uint32_t> successors;
                    if (source_group.has_self) successors.push_back(owner);
                    for (std::uint32_t s = 0;
                         s < source_group.successor_count; ++s) {
                        successors.push_back(
                            result.behavioral_representative_by_state.at(
                                strict->choice_successors.at(
                                    source_group.successor_offset + s)));
                    }
                    std::sort(successors.begin(), successors.end());
                    successors.erase(
                        std::unique(successors.begin(), successors.end()),
                        successors.end());
                    for (const std::uint32_t successor : successors) {
                        if (successor == owner) group.has_self = true;
                        else quotient->choice_successors.push_back(successor);
                    }
                    group.successor_count = static_cast<std::uint32_t>(
                        quotient->choice_successors.size() -
                        group.successor_offset);
                    quotient->choices.push_back(group);
                }
                row.choice_count = source.choice_count;
                row.variant_offset = source.variant_offset;
                for (std::uint32_t i = 0; i < source.variant_count; ++i) {
                    const SparseVariant& source_variant =
                        strict->variant_arena->variants.at(
                        strict->variant_arena->row_variant_indices.at(
                            source.variant_offset + i));
                    if (source_variant.operator_index == kNoId) {
                        throw std::logic_error(
                            "exact quotient source row has no operator: state=" +
                            std::to_string(owner) + " row=" +
                            std::to_string(source_span.offset + r));
                    }
                }
                row.variant_count = source.variant_count;
                if (row.self_probability > 0.0) {
                    ++quotient->algebraic_self_loops;
                }
                for (std::uint32_t i = 0; i < row.choice_count; ++i) {
                    if (quotient->choices[row.choice_offset + i].has_self) {
                        ++quotient->algebraic_self_loops;
                    }
                }
                quotient->rows.push_back(row);
                ++target_span.count;
            }
        }
        quotient->expanded_states = representative_expanded;
        transition_cache = std::move(quotient);
        expanded = transition_cache->expanded;
        expanded_count = representative_expanded;
        priced_rows.clear();
        pricing_diagnostics_cursor = 0;
        policy_rows.clear();
    }

    void prepare_focused_exact_quotient() {
        /* Focused lower solves retain strict Bellman identities. Completed
         * graphs are still refined by the all-action exact quotient; partial
         * frontier grouping remains scheduling-only. */
    }

    void prepare_exact_outer_quotient() {
        const std::uint32_t state_count = transition_cache->discovered_states;
        result.diagnostics.strict_discovered_states = state_count;
        transition_cache->strict_discovered_states = state_count;
        /* A closed lower/constructive-upper bracket is already an exact
         * optimality proof. Its frontier states deliberately carry the
         * executable Restart fallback, not complete all-action rows, so they
         * must remain strict compiler identities rather than being treated as
         * candidates for the completed-graph behavioral quotient. */
        if (focused_bound_proved) {
            result.diagnostics.quotient_states = state_count;
            transition_cache->quotient_states = state_count;
            transition_cache->exact_quotient = false;
            result.behavioral_representative_by_state.clear();
            transition_cache->behavioral_representative_by_state.clear();
            return;
        }
        if (state_count == 0) {
            result.diagnostics.quotient_states = state_count;
            transition_cache->quotient_states = state_count;
            return;
        }

        auto [coarse, coarse_count] = exact_partition(
            state_count, [&](const std::uint32_t state) {
                return coarse_state_signature(state);
            });
        result.diagnostics.coarse_candidate_classes = coarse_count;
        std::vector<std::uint32_t> coarse_sizes(coarse_count, 0);
        for (const std::uint32_t value : coarse) ++coarse_sizes[value];
        for (const std::uint32_t size : coarse_sizes) {
            result.diagnostics.max_strict_states_per_coarse_class = std::max(
                result.diagnostics.max_strict_states_per_coarse_class, size);
        }

        const bool incomplete = result.diagnostics.resource_cap_hit ||
                                !queue.empty();
        if (incomplete && !options.full_evidence) {
            result.diagnostics.quotient_states = state_count;
            transition_cache->quotient_states = state_count;
            return;
        }
        if (options.full_evidence) {
            collect_action_observation_cardinalities(*transition_cache);
        }

        if (incomplete) {
            auto [shadow, shadow_count] = exact_partition(
                state_count, [&](const std::uint32_t state) {
                    return shadow_state_signature(*transition_cache, state);
                });
            result.diagnostics.state_scaling_shadow_only = true;
            result.diagnostics.shadow_behavioral_classes = shadow_count;
            result.diagnostics.shadow_expanded_states_observed = expanded_count;
            result.diagnostics.quotient_refinement_rounds = 1;
            result.diagnostics.quotient_states = state_count;
            transition_cache->quotient_states = state_count;
            std::vector<std::uint32_t> first_shadow(coarse_count, kNoId);
            std::vector<std::uint32_t> first_state(coarse_count, kNoId);
            for (std::uint32_t state = 0; state < state_count; ++state) {
                const std::uint32_t candidate = coarse[state];
                if (first_shadow[candidate] == kNoId) {
                    first_shadow[candidate] = shadow[state];
                    first_state[candidate] = state;
                } else if (first_shadow[candidate] != shadow[state]) {
                    ++result.diagnostics.witnessed_non_equivalences;
                    ++result.diagnostics.projected_successor_class_mismatches;
                    if (result.diagnostics.equivalence_witnesses.size() <
                        options.max_diagnostic_samples) {
                        result.diagnostics.equivalence_witnesses.push_back(
                            first_equivalence_witness(
                                *transition_cache, first_state[candidate],
                                state));
                    } else {
                        ++result.diagnostics.equivalence_witnesses_omitted;
                    }
                }
            }
            transition_cache->exact_quotient = !options.strict_states;
            return;
        }

        std::vector<std::uint32_t> partition = coarse;
        std::uint32_t class_count = coarse_count;
        for (;;) {
            RowObservationCache row_cache;
            auto [next_partition, next_class_count] = exact_partition(
                state_count, [&](const std::uint32_t state) {
                    return state_behavior_signature(
                        *transition_cache, state, partition, &row_cache);
                });
            ++result.diagnostics.quotient_refinement_rounds;
            if (next_partition == partition) {
                class_count = next_class_count;
                break;
            }
            partition = std::move(next_partition);
            class_count = next_class_count;
        }
        result.diagnostics.quotient_states = class_count;
        result.diagnostics.exact_behavioral_merges = state_count - class_count;
        transition_cache->quotient_states = class_count;

        std::vector<std::uint32_t> first_partition(coarse_count, kNoId);
        std::vector<std::uint32_t> first_state(coarse_count, kNoId);
        for (std::uint32_t state = 0; state < state_count; ++state) {
            const std::uint32_t candidate = coarse[state];
            if (first_partition[candidate] == kNoId) {
                first_partition[candidate] = partition[state];
                first_state[candidate] = state;
            } else if (first_partition[candidate] != partition[state]) {
                ++result.diagnostics.witnessed_non_equivalences;
                ++result.diagnostics.projected_successor_class_mismatches;
                if (options.full_evidence &&
                    result.diagnostics.equivalence_witnesses.size() <
                        options.max_diagnostic_samples) {
                    result.diagnostics.equivalence_witnesses.push_back(
                        first_equivalence_witness(
                            *transition_cache, first_state[candidate], state));
                } else {
                    ++result.diagnostics.equivalence_witnesses_omitted;
                }
            }
        }
        if (!options.strict_states && class_count < state_count) {
            build_quotient_graph(partition, class_count);
        } else {
            transition_cache->exact_quotient = !options.strict_states;
            if (!options.strict_states) {
                result.behavioral_representative_by_state.resize(state_count);
                std::iota(
                    result.behavioral_representative_by_state.begin(),
                    result.behavioral_representative_by_state.end(), 0u);
                transition_cache->behavioral_representative_by_state =
                    result.behavioral_representative_by_state;
            }
        }
    }

    void prepare_iteration() {
        const auto started = std::chrono::steady_clock::now();
        if (!cache_pending &&
            expanded_count >= options.max_expanded_states &&
            calc.state_count() > expanded_count) {
            /* A focused batch can consume its last scheduled state exactly as
             * it reaches the expansion cap. Queue emptiness is not a closure
             * certificate: generated strict successors still witness the
             * unfinished graph. Record the cap before outer quotienting can
             * reduce the visible expanded-class count. */
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
        if (price_bound_state_pruning) {
            /* Certified graphs contain every row needed by the exact current
             * optimum, but deliberately omit price-dominated action rows.
             * Keep their concrete state identities and never present the
             * retained subset as an all-action behavioral quotient. */
            const std::uint32_t state_count =
                transition_cache->discovered_states;
            result.diagnostics.strict_discovered_states = state_count;
            result.diagnostics.quotient_states = state_count;
            transition_cache->strict_discovered_states = state_count;
            transition_cache->quotient_states = state_count;
            transition_cache->exact_quotient = false;
            transition_cache->behavioral_representative_by_state.clear();
            result.behavioral_representative_by_state.clear();
        } else if (cache_pending && transition_cache->exact_quotient &&
            !transition_cache->behavioral_representative_by_state.empty()) {
            /* A retained price-only graph already owns its exact quotient.
             * Re-refining representative-only rows would treat lifted strict
             * members as unexpanded frontier and erase their policy/value
             * lift on the second solve. */
            result.diagnostics.strict_discovered_states =
                transition_cache->strict_discovered_states;
            result.diagnostics.quotient_states =
                transition_cache->quotient_states;
        } else {
            prepare_exact_outer_quotient();
        }
        result.diagnostics.expanded_states = expanded_count;
        const std::uint32_t state_count = transition_cache->discovered_states;
        result.diagnostics.discovered_states =
            transition_cache->quotient_states == 0
                ? state_count
                : transition_cache->quotient_states;
        result.diagnostics.frontier_states =
            result.diagnostics.discovered_states - expanded_count;
        expanded.resize(state_count, 0);
        result.expanded = expanded;
        if (focused_bound_proved &&
            focused_previous_upper_values.size() == state_count) {
            result.values = std::move(focused_previous_upper_values);
        } else {
            result.values.assign(state_count, kValueCeiling);
        }
        result.values.resize(state_count, kValueCeiling);
        result.policy.assign(state_count, PolicyOperatorRef{});
        result.unveil_preferences.assign(state_count, {});
        result.option_unveil_preferences.assign(state_count, {});
        result.goal_states.assign(state_count, 0);
        for (std::uint32_t state = 0; state < state_count; ++state) {
            if (!result.behavioral_representative_by_state.empty() &&
                result.behavioral_representative_by_state[state] != state) {
                result.values[state] = kInfinity;
                continue;
            }
            if (calc.is_goal_state(calc.state(state))) {
                result.goal_states[state] = 1;
                ++result.diagnostics.goal_states;
                result.values[state] = 0.0;
            } else if (!result.expanded[state] && !focused_bound_proved) {
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
        if (focused_bound_proved) {
            const std::uint64_t no_row =
                std::numeric_limits<std::uint64_t>::max();
            for (std::uint32_t state = 0; state < state_count; ++state) {
                if (result.goal_states[state]) continue;
                std::uint32_t operator_index = restart_operator_index;
                if (state <
                        focused_previous_frontier_upper_operator.size() &&
                    focused_previous_frontier_upper_operator[state] !=
                        kNoId) {
                    operator_index =
                        focused_previous_frontier_upper_operator[state];
                }
                if (state < policy_rows.size() &&
                    policy_rows[state] != no_row &&
                    policy_rows[state] < priced_rows.size()) {
                    operator_index =
                        priced_rows[policy_rows[state]].operator_index;
                }
                if (operator_index != kNoId) {
                    result.policy[state] = PolicyOperatorRef{
                        calc.operators()[operator_index].kind,
                        operator_index};
                }
            }
            /* The equal constructive upper and global lower bounds are the
             * convergence proof. prepare_iteration() normally resets these
             * fields for a new Bellman phase; preserve the proved terminal
             * state instead of reporting an artificial ceiling residual. */
            residual = 0.0;
            result.diagnostics.residual = 0.0;
            policy_initialized = true;
            policy_stable = true;
            policy_iteration_failed = false;
            backup_active = false;
        }
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
        if (!cache_pending && !result.diagnostics.state_cap_hit &&
            !result.diagnostics.resource_cap_hit &&
            queue.empty() && !focused_bound_proved &&
            !price_bound_state_pruning) {
            transition_cache->focused_partial = focused_mode;
            calc.retain_solve_transition_cache(transition_cache);
        }
        kernel_rows_by_hash.clear();
        kernel_rows_by_hash.rehash(0);
        owned_kernel_row_bucket_bytes = 0;
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
        if (focused_bound_proved) phase = SolvePhase::Done;
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
        owned_kernel_value_cache_nested_bytes = 0;
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
        std::uint32_t previous = 0;
        bool first = true;
        for (std::uint32_t i = 0; i < row.transition_count; ++i) {
            const std::uint64_t offset = row.transition_offset + i;
            const std::uint32_t successor =
                transition_cache->successors.at(offset);
            const double probability =
                transition_cache->probabilities.at(offset);
            if (!first && successor < previous) {
                cache.sorted_successors = false;
            }
            first = false;
            previous = successor;
            const double value = result.values.at(successor);
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
            const auto begin = transition_cache->successors.begin() +
                               cache.transition_offset;
            const auto end = begin + cache.transition_count;
            double probability = 0.0;
            if (cache.sorted_successors) {
                auto found = std::lower_bound(begin, end, state);
                while (found != end && *found == state) {
                    const std::uint64_t offset =
                        cache.transition_offset + (found - begin);
                    probability += transition_cache->probabilities.at(offset);
                    ++found;
                }
            } else {
                for (auto found = begin; found != end; ++found) {
                    if (*found != state) continue;
                    const std::uint64_t offset =
                        cache.transition_offset + (found - begin);
                    probability += transition_cache->probabilities.at(offset);
                }
            }
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
                row.embedded_self_probability > 0.0 &&
                self_value == kInfinity;
            const std::uint32_t non_self_infinite =
                cache.infinite_count - (infinite_self ? 1u : 0u);
            transition_work += row.transition_count;
            if (non_self_infinite != 0) return kInfinity;
            constant += cache.finite_sum;
            if (!infinite_self) {
                constant -=
                    row.embedded_self_probability * self_value;
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
        policy_selection_states.clear();
        policy_selection_states.reserve(expanded_count);
        for (std::uint32_t state = 0;
             state < result.values.size(); ++state) {
            if (result.expanded[state] && !result.goal_states[state]) {
                policy_selection_states.push_back(state);
            }
        }
        policy_selection_improved = false;
        policy_selection_residual = 0.0;
    }

    bool initialize_focused_proper_policy() {
        if (!policy_selection_active) begin_policy_selection();
        const std::uint64_t transient_bytes = result.values.size() +
            policy_selection_states.size() * sizeof(std::uint32_t);
        if (check_solver_byte_cap_fast(transient_bytes)) return false;
        std::vector<std::uint8_t> assigned(result.values.size(), 0);
        std::vector<std::uint32_t> order = policy_selection_states;
        const auto start = std::find(
            order.begin(), order.end(), result.start_state);
        if (start != order.end()) {
            std::rotate(order.begin(), start, start + 1);
        }
        std::size_t remaining = order.size();
        bool progress = true;
        while (remaining != 0 && progress) {
            progress = false;
            for (const std::uint32_t state : order) {
                if (assigned[state]) continue;
                const StateRowSpan& span =
                    transition_cache->state_rows.at(state);
                std::uint64_t best_row =
                    std::numeric_limits<std::uint64_t>::max();
                for (std::uint32_t relative = 0;
                     relative < span.count; ++relative) {
                    const std::uint64_t absolute = span.offset + relative;
                    if (preservation_prunes(absolute)) continue;
                    const SparseRow& row =
                        transition_cache->rows.at(absolute);
                    bool valid = true;
                    bool exits_rank = false;
                    const auto route = [&](const std::uint32_t successor) {
                        if (!valid || successor == state) return;
                        if (successor >= result.values.size()) {
                            valid = false;
                        } else if (result.goal_states[successor] ||
                                   !result.expanded[successor] ||
                                   assigned[successor]) {
                            exits_rank = true;
                        } else {
                            valid = false;
                        }
                    };
                    for (std::uint32_t i = 0;
                         valid && i < row.transition_count; ++i) {
                        const std::uint64_t offset = row.transition_offset + i;
                        if (transition_cache->probabilities.at(offset) > 0.0) {
                            route(transition_cache->successors.at(offset));
                        }
                    }
                    for (std::uint32_t i = 0;
                         valid && i < row.choice_count; ++i) {
                        const SparseChoiceGroup& choice =
                            transition_cache->choices.at(row.choice_offset + i);
                        if (choice.probability <= 0.0) continue;
                        std::uint32_t selected =
                            choice.has_self ? state : kNoId;
                        double selected_value =
                            choice.has_self ? result.values[state] : kInfinity;
                        for (std::uint32_t s = 0;
                             s < choice.successor_count; ++s) {
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
                        if (selected == kNoId) valid = false;
                        else route(selected);
                    }
                    if (!valid || !exits_rank) continue;
                    /* Initialization needs any ranked proper row. Operator
                     * scheduling places deterministic finishes before broad
                     * stochastic kernels, so take the first certificate and
                     * leave cost optimality to ordinary Howard improvement. */
                    best_row = absolute;
                    break;
                }
                if (best_row == std::numeric_limits<std::uint64_t>::max()) {
                    continue;
                }
                policy_rows[state] = best_row;
                assigned[state] = 1;
                --remaining;
                progress = true;
            }
        }
        if (remaining != 0) return false;
        policy_selection_active = false;
        policy_selection_cursor = 0;
        policy_selection_states.clear();
        policy_selection_improved = false;
        policy_selection_residual = 0.0;
        return true;
    }

    bool advance_policy_selection(bool& improved) {
        if (!policy_selection_active) begin_policy_selection();
        const std::uint64_t no_row =
            std::numeric_limits<std::uint64_t>::max();
        constexpr std::uint32_t kStatesPerSelectionUnit = 128;
        const std::uint32_t end = std::min<std::uint32_t>(
            static_cast<std::uint32_t>(policy_selection_states.size()),
            policy_selection_cursor + kStatesPerSelectionUnit);
        for (std::uint32_t active = policy_selection_cursor;
             active < end; ++active) {
            const std::uint32_t state = policy_selection_states[active];
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
                bool improving = policy_rows[state] == no_row;
                if (!improving) {
                    std::uint32_t current_work = 0;
                    const double current = sparse_row_q(
                        policy_rows[state], current_work);
                    improving = best < current - options.epsilon;
                }
                if (improving) {
                    policy_rows[state] = best_row;
                    policy_selection_improved = true;
                }
            }
        }
        if (policy_selection_cursor < policy_selection_states.size()) {
            policy_selection_cursor = end;
        }
        if (policy_selection_cursor < policy_selection_states.size()) {
            return false;
        }
        residual = policy_selection_residual;
        result.diagnostics.residual = residual;
        improved = policy_selection_improved;
        policy_selection_active = false;
        policy_selection_states.clear();
        return true;
    }

    void reset_policy_iteration_units() {
        policy_unit_stage = PolicyUnitStage::Seed;
        policy_seed_pass = 0;
        policy_seed_cursor = 0;
        policy_seed_states.clear();
        policy_selection_active = false;
        policy_selection_cursor = 0;
        policy_selection_states.clear();
        policy_selection_improved = false;
        policy_selection_residual = 0.0;
        sparse_policy_resume.reset();
        policy_kernel_preparation.reset();
        current_policy_scratch_bytes = 0;
        reset_kernel_value_cache();
    }

    bool evaluate_fixed_policy() {
        reset_kernel_value_cache();
        policy_evaluation_incomplete = false;
        improper_policy_states.clear();
        if (!result.diagnostics.policy_evaluation_failure.starts_with(
                "selected_byte_cap_breakdown:")) {
            result.diagnostics.policy_evaluation_failure.clear();
        }
        const auto fail = [&](const char* reason) {
            result.diagnostics.policy_evaluation_failure = reason;
            policy_kernel_preparation.reset();
            current_policy_scratch_bytes = 0;
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
            policy_kernel_preparation->active_states.reserve(
                expanded_count);
            for (std::uint32_t state = 0; state < state_count; ++state) {
                if (result.expanded[state] && !result.goal_states[state]) {
                    policy_kernel_preparation->active_states.push_back(
                        state);
                }
            }
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
            static_cast<std::uint32_t>(preparation.active_states.size()),
            preparation.cursor + kKernelStatesPerWorkUnit);
        for (std::uint32_t active = preparation.cursor;
             active < kernel_end; ++active) {
            const std::uint32_t state = preparation.active_states[active];
            if (!result.expanded[state] || result.goal_states[state]) continue;
            if (policy_rows[state] == no_row) continue;
            const std::uint64_t row_index = policy_rows[state];
            const SparseRow& sparse = transition_cache->rows.at(row_index);
            std::vector<std::uint32_t> selected_choices;
            selected_choices.reserve(sparse.choice_count);
            const double detached_self_probability =
                sparse.self_probability -
                sparse.embedded_self_probability;
            std::vector<std::uint64_t> shared_signature{
                std::bit_cast<std::uint64_t>(
                    priced_rows.at(row_index).cost),
                sparse.transition_offset,
                sparse.transition_count,
                sparse.choice_count,
                detached_self_probability > 0.0 ? state : kNoId,
                std::bit_cast<std::uint64_t>(
                    detached_self_probability)};
            for (std::uint32_t i = 0; i < sparse.choice_count; ++i) {
                const SparseChoiceGroup& choice =
                    transition_cache->choices.at(sparse.choice_offset + i);
                std::uint32_t selected = choice.has_self ? state : kNoId;
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
                selected_choices.push_back(selected);
                shared_signature.push_back(selected);
                shared_signature.push_back(
                    std::bit_cast<std::uint64_t>(choice.probability));
            }
            std::size_t shared_hash = static_cast<std::size_t>(
                1469598103934665603ULL);
            for (const std::uint64_t token : shared_signature) {
                hash_combine(shared_hash, token);
            }
            std::uint32_t shared_owner = kNoId;
            for (const SharedPolicyKernelRepresentative& possible :
                 shared_transition_representatives[shared_hash]) {
                if (possible.exact_signature == shared_signature) {
                    shared_owner = possible.state;
                    break;
                }
            }
            if (shared_owner != kNoId) {
                kernel_owner[state] = shared_owner;
                continue;
            }
            std::vector<PolicyEdge> candidate;
            candidate.reserve(
                sparse.transition_count + sparse.choice_count + 1);
            for (std::uint32_t i = 0; i < sparse.transition_count; ++i) {
                const std::uint64_t offset = sparse.transition_offset + i;
                const double probability =
                    transition_cache->probabilities.at(offset);
                if (probability == 0.0) continue;
                candidate.push_back({
                    transition_cache->successors.at(offset),
                    probability});
            }
            if (detached_self_probability > 0.0) {
                const auto self_position = std::lower_bound(
                    candidate.begin(), candidate.end(), state,
                    [](const PolicyEdge& edge, const std::uint32_t target) {
                        return edge.target < target;
                    });
                if (self_position != candidate.end() &&
                    self_position->target == state) {
                    self_position->probability += detached_self_probability;
                } else {
                    candidate.insert(
                        self_position, {state, detached_self_probability});
                }
            }
            for (std::uint32_t i = 0; i < sparse.choice_count; ++i) {
                const SparseChoiceGroup& choice =
                    transition_cache->choices.at(sparse.choice_offset + i);
                candidate.push_back(
                    {selected_choices.at(i), choice.probability});
            }
            if (sparse.choice_count != 0) {
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
            }

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
            shared_transition_representatives[shared_hash].push_back(
                {owner, std::move(shared_signature)});
            kernel_owner[state] = owner;
        }
        preparation.cursor = kernel_end;
        if (preparation.cursor < preparation.active_states.size()) {
            policy_evaluation_incomplete = true;
            return false;
        }

        if (preparation.representative.empty()) {
            preparation.representative = kernel_owner;
            preparation.group_members.resize(state_count);
            preparation.rows.resize(state_count);
            /* The fixed policy retains one chosen row per expanded state,
             * not the entire action graph. Reserving every graph successor
             * duplicated tens of MiB at focused-quotient peak even when
             * exact kernel sharing reduced the policy to a small edge set.
             * Start with one edge per state and let the vector grow exactly
             * if a genuinely wider selected policy requires it. */
            preparation.edges.reserve(std::min<std::size_t>(
                transition_cache->successors.size(), state_count));
        }
        std::vector<std::uint32_t>& representative =
            preparation.representative;
        std::vector<std::vector<std::uint32_t>>& group_members =
            preparation.group_members;
        std::vector<PolicyRow>& rows = preparation.rows;
        std::vector<PolicyEdge>& edges = preparation.edges;
        constexpr std::uint32_t kGroupingStatesPerWorkUnit = 256;
        const std::uint32_t grouping_end = std::min<std::uint32_t>(
            static_cast<std::uint32_t>(preparation.active_states.size()),
            preparation.grouping_cursor + kGroupingStatesPerWorkUnit);
        for (std::uint32_t active = preparation.grouping_cursor;
             active < grouping_end; ++active) {
            const std::uint32_t state = preparation.active_states[active];
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
        if (preparation.grouping_cursor < preparation.active_states.size()) {
            policy_evaluation_incomplete = true;
            return false;
        }
        constexpr std::uint32_t kQuotientStatesPerWorkUnit = 64;
        const std::uint32_t quotient_end = std::min<std::uint32_t>(
            static_cast<std::uint32_t>(preparation.active_states.size()),
            preparation.quotient_cursor + kQuotientStatesPerWorkUnit);
        for (std::uint32_t active = preparation.quotient_cursor;
             active < quotient_end; ++active) {
            const std::uint32_t state = preparation.active_states[active];
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
        if (preparation.quotient_cursor < preparation.active_states.size()) {
            policy_evaluation_incomplete = true;
            return false;
        }
        if (!preparation.source_kernels_released) {
            preparation.kernel_owner.clear();
            preparation.kernel_owner.shrink_to_fit();
            preparation.full_kernel.clear();
            preparation.full_kernel.shrink_to_fit();
            preparation.representatives_by_hash.clear();
            preparation.representatives_by_hash.rehash(0);
            preparation.shared_transition_representatives.clear();
            preparation.shared_transition_representatives.rehash(0);
            preparation.source_kernels_released = true;
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
                    while (preparation.tarjan_root_cursor <
                               preparation.active_states.size() &&
                           tarjan_work < kTarjanWorkPerUnit) {
                        const std::uint32_t root =
                            preparation.active_states[
                                preparation.tarjan_root_cursor++];
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

            if (preparation.tarjan_root_cursor >=
                    preparation.active_states.size() &&
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
            preparation.active_states.capacity() * sizeof(std::uint32_t) +
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
        policy_scratch +=
            preparation.shared_transition_representatives.bucket_count() *
            sizeof(void*);
        policy_scratch +=
            preparation.shared_transition_representatives.size() *
            (sizeof(decltype(preparation.shared_transition_representatives)::
                        value_type) +
             2 * sizeof(void*));
        for (const auto& [unused, entries] :
             preparation.shared_transition_representatives) {
            (void)unused;
            policy_scratch += entries.capacity() *
                              sizeof(SharedPolicyKernelRepresentative);
            for (const SharedPolicyKernelRepresentative& entry : entries) {
                policy_scratch += entry.exact_signature.capacity() *
                                  sizeof(std::uint64_t);
            }
        }
        current_policy_scratch_bytes = policy_scratch;
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
                    /* Focused lower solves use the exact state's admissible
                     * frontier value as their terminal cost. The former zero
                     * shortcut was valid only while every frontier value was
                     * initialized to zero; retaining it after goal-cover
                     * initialization made fixed-policy evaluation disagree
                     * with the Bellman backup and re-evaluate a stable policy
                     * forever. The frontier value is included in external_sum
                     * below. */
                    if (!result.expanded[edge.target] &&
                        !focused_lower_mode) {
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
                const std::uint64_t matrix_bytes =
                    n * (n + 1) * sizeof(WideFloat);
                check_solver_byte_cap_fast(matrix_bytes);
                peak_policy_scratch_bytes = std::max(
                    peak_policy_scratch_bytes,
                    policy_scratch + matrix_bytes);
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
                const std::uint64_t iterative_bytes =
                    n * 8 * sizeof(WideFloat);
                check_solver_byte_cap_fast(iterative_bytes);
                peak_policy_scratch_bytes = std::max(
                    peak_policy_scratch_bytes,
                    policy_scratch + iterative_bytes);
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
        current_policy_scratch_bytes = 0;
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
            if (policy_seed_states.empty()) {
                policy_seed_states.reserve(expanded_count);
                for (std::uint32_t state = 0;
                     state < result.values.size(); ++state) {
                    if (result.expanded[state] && !result.goal_states[state]) {
                        policy_seed_states.push_back(state);
                    }
                }
            }
            if (policy_seed_cursor == 0) {
                reset_kernel_value_cache(true);
            }
            constexpr std::uint32_t kStatesPerSeedUnit = 128;
            const std::uint32_t end = std::min<std::uint32_t>(
                static_cast<std::uint32_t>(policy_seed_states.size()),
                policy_seed_cursor + kStatesPerSeedUnit);
            for (std::uint32_t offset = policy_seed_cursor;
                 offset < end; ++offset) {
                const std::uint32_t state =
                        policy_seed_pass % 2 == 0
                            ? policy_seed_states[
                                  policy_seed_states.size() - 1 - offset]
                            : policy_seed_states[offset];
                std::uint32_t work = 0;
                const double best = backup_state(state, work);
                if (best < result.values[state]) {
                    update_kernel_value_cache(
                        state, result.values[state], best);
                    result.values[state] = best;
                }
            }
            policy_seed_cursor = end;
            if (policy_seed_cursor >= policy_seed_states.size()) {
                policy_seed_cursor = 0;
                if (++policy_seed_pass >= 4) {
                    policy_seed_states.clear();
                    policy_unit_stage = PolicyUnitStage::InitialSelect;
                }
            }
            backup_active = true;
            finish_unit();
            return true;
        }
        if (policy_unit_stage == PolicyUnitStage::InitialSelect) {
            if (focused_lower_mode && !policy_initialized &&
                initialize_focused_proper_policy()) {
                policy_initialized = true;
                policy_stable = false;
                policy_unit_stage = PolicyUnitStage::Evaluate;
                backup_active = true;
                finish_unit();
                return true;
            }
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
        for (std::uint32_t state = 0; state < state_count; ++state) {
            if (calc.is_goal_state(calc.state(state))) continue;
            result.values[state] =
                optimistic_completion_cost_for_state(state);
        }
        /* Expanding a previously zero-valued frontier can only raise the
         * focused lower bound. Preserve the last round's admissible values
         * for already-known states instead of restarting every exact policy
         * evaluation from zero. */
        const std::size_t retained = std::min<std::size_t>(
            previous_values.size(), result.values.size());
        if (focused_behavioral_representative.empty()) {
            for (std::size_t state = 0; state < retained; ++state) {
                if (std::isfinite(previous_values[state]) &&
                    previous_values[state] >= 0.0 &&
                    previous_values[state] < kValueCeiling) {
                    result.values[state] = std::max(
                        result.values[state], previous_values[state]);
                }
            }
        } else {
            /* Exact class members have the same value in the current lower
             * problem. Retaining their minimum prior lower bound preserves
             * admissibility even if an earlier solve stopped at tolerance. */
            std::vector<double> retained_min(state_count, kInfinity);
            for (std::size_t state = 0; state < retained; ++state) {
                const double value = previous_values[state];
                if (!std::isfinite(value) || value < 0.0 ||
                    value >= kValueCeiling) {
                    continue;
                }
                const std::uint32_t representative =
                    focused_behavioral_representative.at(state);
                retained_min[representative] = std::min(
                    retained_min[representative], value);
            }
            for (std::uint32_t representative = 0;
                 representative < state_count; ++representative) {
                if (std::isfinite(retained_min[representative])) {
                    result.values[representative] = std::max(
                        result.values[representative],
                        retained_min[representative]);
                }
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
        prepare_focused_exact_quotient();
        prepare_priced_rows();
        policy_rows.clear();
        improper_policy_states.clear();
        policy_initialized = false;
        policy_stable = false;
        policy_iteration_failed = false;
        reset_policy_iteration_units();
        /* Focused solves construct an explicitly proper ranked policy before
         * their first evaluation. The legacy four all-action seed passes were
         * only a heuristic defense against improper initialization and become
         * pure duplicate transition work once that exact initializer exists. */
        policy_unit_stage = PolicyUnitStage::InitialSelect;
        backup_active = false;
        sweeps = 0;
        residual = kValueCeiling;
        if (!result.diagnostics.policy_evaluation_failure.starts_with(
                "selected_byte_cap_breakdown:")) {
            result.diagnostics.policy_evaluation_failure.clear();
        }
        if (expanded_count == 1 &&
            result.start_state < result.expanded.size() &&
            result.expanded[result.start_state] &&
            !result.goal_states[result.start_state]) {
            const std::uint64_t no_row =
                std::numeric_limits<std::uint64_t>::max();
            policy_rows.assign(state_count, no_row);
            const StateRowSpan& span = transition_cache->state_rows.at(
                result.start_state);
            double best = kInfinity;
            std::uint64_t best_row = no_row;
            for (std::uint32_t relative = 0;
                 relative < span.count; ++relative) {
                const std::uint64_t absolute = span.offset + relative;
                if (preservation_prunes(absolute)) continue;
                std::uint32_t work = 0;
                const double candidate = sparse_row_q(absolute, work);
                ++result.diagnostics.bellman_action_evaluations;
                if (candidate < best - options.epsilon ||
                    (std::abs(candidate - best) <= options.epsilon &&
                     absolute < best_row)) {
                    best = candidate;
                    best_row = absolute;
                }
            }
            ++result.diagnostics.bellman_backups;
            if (best_row != no_row && std::isfinite(best)) {
                result.values[result.start_state] = best;
                policy_rows[result.start_state] = best_row;
                policy_initialized = true;
                policy_stable = true;
                residual = 0.0;
                result.diagnostics.residual = 0.0;
                finish_focused_lower_solve();
            }
        }
    }

    bool collect_focused_fringe(
        std::vector<std::uint32_t>& fringe,
        std::vector<double>& priority,
        const std::vector<double>* gap_lower_values = nullptr) {
        const std::uint64_t no_row =
            std::numeric_limits<std::uint64_t>::max();
        if (result.start_state >= result.values.size()) return false;
        std::vector<std::uint8_t> visited(result.values.size(), 0);
        std::vector<std::uint8_t> queued_fringe(result.values.size(), 0);
        priority.assign(result.values.size(), 0.0);
        std::vector<double> path_mass(result.values.size(), 0.0);
        path_mass[result.start_state] = 1.0;
        std::unordered_set<std::uint64_t> routed_transition_kernels;
        std::deque<std::uint32_t> walk{result.start_state};
        const auto route = [&](const std::uint32_t successor,
                               const double mass) {
            if (result.goal_states[successor]) return;
            if (!result.expanded[successor]) {
                double contribution = mass;
                if (gap_lower_values != nullptr &&
                    successor < gap_lower_values->size()) {
                    const double state_upper = result.values[successor];
                    const double state_lower = (*gap_lower_values)[successor];
                    if (std::isfinite(state_upper) &&
                        std::isfinite(state_lower)) {
                        contribution *= std::max(
                            0.0, state_upper - state_lower);
                    }
                } else if (successor <
                               focused_previous_upper_values.size()) {
                    const double state_upper =
                        focused_previous_upper_values[successor];
                    const double state_lower = result.values[successor];
                    if (std::isfinite(state_upper) &&
                        std::isfinite(state_lower)) {
                        contribution *= std::max(
                            0.0, state_upper - state_lower);
                    }
                } else if (focused_fallback_policy.has_value()) {
                    const FocusedFallbackPolicy& fallback =
                        *focused_fallback_policy;
                    const double state_upper = std::min(
                        fallback_terminal_upper(successor, fallback),
                        restart_cost + fallback.anchor_state_value);
                    const double state_lower = result.values[successor];
                    if (std::isfinite(state_upper) &&
                        std::isfinite(state_lower)) {
                        contribution *= std::max(
                            0.0, state_upper - state_lower);
                    }
                }
                if (gap_lower_values != nullptr) {
                    const std::uint32_t progress = std::popcount(
                        satisfied_goal_mask_for_state(successor));
                    contribution *= 1.0 +
                        options.focused_goal_progress_priority_multiplier *
                            static_cast<double>(progress);
                }
                priority[successor] += contribution;
                if (!queued_fringe[successor]) {
                    queued_fringe[successor] = 1;
                    fringe.push_back(successor);
                }
            } else if (!visited[successor]) {
                path_mass[successor] += mass;
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
            const double source_mass = path_mass[state];
            double loop_probability = row.self_probability;
            for (std::uint32_t i = 0; i < row.choice_count; ++i) {
                const SparseChoiceGroup& choice =
                    transition_cache->choices.at(row.choice_offset + i);
                if (choice.has_self) loop_probability += choice.probability;
            }
            const double exit_probability = 1.0 - loop_probability;
            const double normalization = exit_probability > 1e-15
                                             ? source_mass / exit_probability
                                             : source_mass;
            const bool route_transitions =
                row.choice_count != 0 || row.transition_count == 0 ||
                routed_transition_kernels.insert(row.transition_offset)
                    .second;
            if (route_transitions) {
                for (std::uint32_t i = 0; i < row.transition_count; ++i) {
                    const std::uint32_t successor =
                        transition_cache->successors.at(
                            row.transition_offset + i);
                    route(
                        successor,
                        normalization * transition_cache->probabilities.at(
                            row.transition_offset + i));
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
                if (selected != state && selected != kNoId) {
                    route(selected, normalization * choice.probability);
                }
            }
        }
        return true;
    }

    std::pair<double, std::uint32_t> constructive_direct_action_upper(
        const std::uint32_t state,
        const std::uint32_t action_index) const {
        if (action_index >= calc.registry().actions.size() ||
            action_index >= priced_operator_position.size()) {
            return {kInfinity, kNoId};
        }
        /* Primitive planner wrappers deliberately share registry indices. */
        const std::int32_t priced_position =
            priced_operator_position[action_index];
        if (priced_position < 0) return {kInfinity, kNoId};
        const PricedOperator& priced =
            operators.at(static_cast<std::size_t>(priced_position));
        const PlannerOperator& planner = calc.operators().at(priced.index);
        const ActionDescriptor& action =
            calc.registry().actions.at(action_index);
        const AbstractState& carrier = calc.state(state);
        if (planner.kind != PlannerOperatorKind::Primitive ||
            planner.primitive_action != action_index ||
            action.params.type != ActionType::Bench ||
            action.kind != TransitionKind::Deterministic ||
            action.params.mod_id >= session.mod_count ||
            action.params.mod_id >= session.metamod_type.size() ||
            session.metamod_type[action.params.mod_id] >= 0 ||
            !std::isfinite(priced.cost) || priced.cost < 0.0 ||
            !action_legal(session, action, carrier) ||
            (carrier.flags & kFlagCraftedMod) != 0 ||
            carrier.rarity != calc.goal().rarity) {
            return {kInfinity, kNoId};
        }
        std::uint32_t satisfied = satisfied_goal_mask_for_state(state);
        std::uint32_t finish_mask = 0;
        for (std::uint32_t slot = 0;
             slot < calc.layout().slots.size(); ++slot) {
            if (pc_bitset_test(
                    calc.layout().slots[slot].satisfying_mask.data(),
                    action.params.mod_id)) {
                finish_mask |= 1u << slot;
                if (carrier.slot_status[slot] !=
                        static_cast<std::uint8_t>(GoalSlotStatus::Absent) ||
                    (carrier.blocked_mask & (1u << slot)) != 0) {
                    return {kInfinity, kNoId};
                }
            }
        }
        if (finish_mask == 0 ||
            std::popcount(satisfied | finish_mask) <
                calc.goal().required_satisfied_slots()) {
            return {kInfinity, kNoId};
        }
        const std::uint8_t cap = rarity_affix_cap(session, carrier.rarity);
        const std::int8_t side = session.gen_type[action.params.mod_id];
        if ((side == PC_SIDE_PREFIX && carrier.prefix_count >= cap) ||
            (side == PC_SIDE_SUFFIX && carrier.suffix_count >= cap) ||
            (side != PC_SIDE_PREFIX && side != PC_SIDE_SUFFIX)) {
                return {kInfinity, kNoId};
        }
        return {priced.cost, priced.index};
    }

    bool renewal_fallback_eligible(
        const std::uint32_t state,
        const FocusedFallbackPolicy& fallback) const {
        if (state >= calc.state_count() ||
            calc.is_goal_state(calc.state(state)) ||
            fallback.renewal_operator == kNoId ||
            fallback.renewal_operator >= calc.operators().size()) {
            return false;
        }
        const PlannerOperator& renewal =
            calc.operators().at(fallback.renewal_operator);
        if (renewal.kind != PlannerOperatorKind::FixedOption ||
            renewal.primitive_program.empty()) {
            return false;
        }
        const std::uint32_t first_action = renewal.primitive_program.front();
        if (first_action >= calc.registry().actions.size()) return false;
        const ActionDescriptor& descriptor =
            calc.registry().actions.at(first_action);
        if (!action_transition_facts(descriptor.params.type).renewal) {
            return false;
        }
        const AbstractState& carrier = calc.state(state);
        if (carrier.rarity != fallback.renewal_rarity ||
            carrier.influence_bits != fallback.renewal_influence_bits ||
            carrier.searing_exarch_tier !=
                fallback.renewal_searing_exarch_tier ||
            carrier.eater_of_worlds_tier !=
                fallback.renewal_eater_of_worlds_tier ||
            carrier.fractured_goal_mask != 0 ||
            carrier.fractured_metamod_flags != 0 ||
            (carrier.flags & kProtectionFlags) != 0 ||
            !action_legal(session, descriptor, carrier)) {
            return false;
        }
        for (const std::uint8_t count : carrier.fractured_junk_counts) {
            if (count != 0) return false;
        }
        for (const std::uint8_t count :
             carrier.fractured_crafted_junk_counts) {
            if (count != 0) return false;
        }
        return true;
    }

    double fallback_terminal_upper(
        const std::uint32_t state,
        const FocusedFallbackPolicy& fallback,
        std::uint32_t* selected_operator = nullptr) const {
        if (calc.is_goal_state(calc.state(state))) {
            if (selected_operator != nullptr) *selected_operator = kNoId;
            return 0.0;
        }
        double best = kInfinity;
        std::uint32_t best_operator = kNoId;
        const auto progress = fallback.progress_state_value.find(state);
        if (progress != fallback.progress_state_value.end() &&
            std::isfinite(progress->second)) {
            best = progress->second;
            const auto progress_operator =
                fallback.progress_state_operator.find(state);
            if (progress_operator !=
                fallback.progress_state_operator.end()) {
                best_operator = progress_operator->second;
            }
        }
        if (renewal_fallback_eligible(state, fallback)) {
            if (fallback.renewal_state_value < best) {
                best = fallback.renewal_state_value;
                best_operator = fallback.renewal_operator;
            }
        }
        const auto [finish, finish_operator] =
            constructive_direct_action_upper(state, fallback.finish_action);
        if (finish < best) {
            best = finish;
            best_operator = finish_operator;
        }
        if (selected_operator != nullptr) *selected_operator = best_operator;
        return best;
    }

    std::optional<FocusedFallbackPolicy> magic_regal_fallback() {
        if (restart_state == kNoId || restart_state >= calc.state_count() ||
            !std::isfinite(restart_cost) || restart_cost < 0.0) {
            return std::nullopt;
        }
        const AbstractState& anchor = calc.state(restart_state);
        if (anchor.rarity != PC_RARITY_NORMAL || anchor.prefix_count != 0 ||
            anchor.suffix_count != 0 ||
            (anchor.flags & (kFlagCraftedMod | kProtectionFlags)) != 0 ||
            anchor.fractured_goal_mask != 0 ||
            anchor.fractured_metamod_flags != 0) {
            return std::nullopt;
        }

        const auto primitive_of_type = [&](const ActionType type) {
            for (const std::uint32_t action : calc.candidates()) {
                if (action < calc.registry().actions.size() &&
                    calc.registry().actions[action].params.type == type) {
                    return action;
                }
            }
            return kNoId;
        };
        const std::uint32_t transmute =
            primitive_of_type(ActionType::Transmute);
        const std::uint32_t alteration =
            primitive_of_type(ActionType::Alteration);
        const std::uint32_t augment =
            primitive_of_type(ActionType::Augment);
        const std::uint32_t regal = primitive_of_type(ActionType::Regal);
        const std::uint32_t exalt = primitive_of_type(ActionType::Exalt);
        if (transmute == kNoId || alteration == kNoId || regal == kNoId) {
            return std::nullopt;
        }
        const auto primitive_cost = [&](const std::uint32_t action) {
            if (action >= priced_operator_position.size()) return kInfinity;
            const std::int32_t position = priced_operator_position[action];
            if (position < 0) return kInfinity;
            const PricedOperator& priced =
                operators.at(static_cast<std::size_t>(position));
            const PlannerOperator& planner = calc.operators().at(priced.index);
            return planner.kind == PlannerOperatorKind::Primitive &&
                           planner.primitive_action == action &&
                           std::isfinite(priced.cost) && priced.cost >= 0.0
                       ? priced.cost
                       : kInfinity;
        };
        const double transmute_cost = primitive_cost(transmute);
        const double alteration_cost = primitive_cost(alteration);
        const double augment_cost = primitive_cost(augment);
        const double regal_cost = primitive_cost(regal);
        const double exalt_cost = primitive_cost(exalt);
        if (!std::isfinite(transmute_cost) ||
            !std::isfinite(alteration_cost) ||
            !std::isfinite(regal_cost)) {
            return std::nullopt;
        }
        if (!action_legal(
                session, calc.registry().actions.at(transmute), anchor)) {
            return std::nullopt;
        }

        const auto exact_bench_mask = [&](const std::uint32_t action) {
            std::uint32_t mask = 0;
            if (action >= calc.registry().actions.size()) return mask;
            const ActionDescriptor& descriptor =
                calc.registry().actions.at(action);
            if (descriptor.params.type != ActionType::Bench ||
                descriptor.params.mod_id >= session.mod_count) {
                return mask;
            }
            for (std::uint32_t slot = 0;
                 slot < calc.layout().slots.size(); ++slot) {
                if (pc_bitset_test(
                        calc.layout().slots[slot].satisfying_mask.data(),
                        descriptor.params.mod_id)) {
                    mask |= 1u << slot;
                }
            }
            return mask;
        };
        const auto same_kernel = [](const OutcomeDistribution& left,
                                    const OutcomeDistribution& right) {
            return left.supported == right.supported &&
                   left.entries == right.entries &&
                   left.choice_groups == right.choice_groups &&
                   left.choice_options == right.choice_options;
        };
        const auto progress_mask = [&](const std::uint32_t state,
                                       const std::uint32_t acquisition) {
            return satisfied_goal_mask_for_state(state) & acquisition;
        };

        FocusedFallbackPolicy best;
        best.anchor_state = restart_state;
        const std::uint32_t required =
            calc.goal().required_satisfied_slots();
        const std::uint32_t all_slots =
            calc.layout().slots.size() == 32
                ? 0xffffffffu
                : (1u << calc.layout().slots.size()) - 1u;
        const std::uint32_t alteration_reach =
            action_goal_reach_mask(alteration);
        const std::uint32_t regal_reach = action_goal_reach_mask(regal);

        for (const std::uint32_t finish_action :
             calc.automatic_goal_bench_actions()) {
            const std::uint32_t finish_mask = exact_bench_mask(finish_action);
            const double finish_cost = primitive_cost(finish_action);
            if (finish_mask == 0 || std::popcount(finish_mask) >= required) {
                continue;
            }
            if (!std::isfinite(finish_cost)) continue;
            const std::uint32_t acquisition_count =
                required - std::popcount(finish_mask);
            if (acquisition_count != 2) continue;
            const std::uint32_t available = all_slots & ~finish_mask;
            for (std::uint32_t acquisition = available; acquisition != 0;
                 acquisition = (acquisition - 1u) & available) {
                if (std::popcount(acquisition) != acquisition_count ||
                    (alteration_reach & acquisition) != acquisition ||
                    (regal_reach & acquisition) != acquisition) {
                    continue;
                }

                const OutcomeDistribution& transmute_kernel =
                    calc.outcomes(restart_state, transmute);
                if (!transmute_kernel.supported ||
                    !transmute_kernel.choice_groups.empty() ||
                    !transmute_kernel.choice_options.empty()) {
                    continue;
                }
                std::map<std::uint32_t, double> direct_carriers;
                std::vector<std::uint32_t> no_target_states;
                double no_target_probability = 0.0;
                for (const OutcomeEntry& outcome : transmute_kernel.entries) {
                    if (progress_mask(outcome.state, acquisition) != 0) {
                        direct_carriers[outcome.state] += outcome.probability;
                    } else {
                        no_target_probability += outcome.probability;
                        no_target_states.push_back(outcome.state);
                    }
                }
                if (no_target_states.empty()) continue;

                const OutcomeDistribution& alteration_kernel =
                    calc.outcomes(no_target_states.front(), alteration);
                if (!alteration_kernel.supported ||
                    !alteration_kernel.choice_groups.empty() ||
                    !alteration_kernel.choice_options.empty()) {
                    continue;
                }
                bool retry_kernel_exact = true;
                for (const std::uint32_t state : no_target_states) {
                    if (!action_legal(
                            session,
                            calc.registry().actions.at(alteration),
                            calc.state(state)) ||
                        !same_kernel(
                            alteration_kernel,
                            calc.outcomes(state, alteration))) {
                        retry_kernel_exact = false;
                        break;
                    }
                }
                std::map<std::uint32_t, double> alteration_exits;
                double alteration_exit_probability = 0.0;
                if (retry_kernel_exact) {
                    for (const OutcomeEntry& outcome :
                         alteration_kernel.entries) {
                        if (progress_mask(outcome.state, acquisition) != 0) {
                            alteration_exits[outcome.state] +=
                                outcome.probability;
                            alteration_exit_probability += outcome.probability;
                        } else if (!same_kernel(
                                       alteration_kernel,
                                       calc.outcomes(
                                           outcome.state, alteration))) {
                            retry_kernel_exact = false;
                            break;
                        }
                    }
                }
                if (!retry_kernel_exact ||
                    alteration_exit_probability <= 1e-15) {
                    continue;
                }

                std::map<std::uint32_t, double> carrier_probability =
                    direct_carriers;
                for (const auto& [state, probability] : alteration_exits) {
                    carrier_probability[state] +=
                        no_target_probability * probability /
                        alteration_exit_probability;
                }

                std::map<std::uint32_t, const OutcomeDistribution*>
                    augment_kernels;
                if (augment != kNoId && std::isfinite(augment_cost)) {
                    std::set<std::uint32_t> augment_sources(
                        no_target_states.begin(), no_target_states.end());
                    for (const OutcomeEntry& outcome :
                         alteration_kernel.entries) {
                        if (progress_mask(outcome.state, acquisition) == 0) {
                            augment_sources.insert(outcome.state);
                        }
                    }
                    for (const std::uint32_t state : augment_sources) {
                        if (!action_legal(
                                session,
                                calc.registry().actions.at(augment),
                                calc.state(state))) {
                            continue;
                        }
                        const OutcomeDistribution& kernel =
                            calc.outcomes(state, augment);
                        if (!kernel.supported ||
                            !kernel.choice_groups.empty() ||
                            !kernel.choice_options.empty()) {
                            continue;
                        }
                        bool valid = true;
                        for (const OutcomeEntry& outcome : kernel.entries) {
                            if (progress_mask(outcome.state, acquisition) != 0) {
                                carrier_probability.try_emplace(
                                    outcome.state, 0.0);
                            } else if (!same_kernel(
                                           alteration_kernel,
                                           calc.outcomes(
                                               outcome.state, alteration))) {
                                valid = false;
                                break;
                            }
                        }
                        if (valid) augment_kernels.emplace(state, &kernel);
                    }
                }

                struct CarrierEquation {
                    double probability = 0.0;
                    double constant = 0.0;
                    double restart_coefficient = 0.0;
                };
                std::unordered_map<std::uint32_t, CarrierEquation>
                    salvage_equations;
                std::unordered_map<std::uint32_t, std::uint32_t>
                    salvage_operator;
                std::unordered_set<std::uint32_t> salvage_active;
                const double salvage_anchor_guess =
                    focused_fallback_policy.has_value()
                        ? focused_fallback_policy->anchor_state_value
                        : kInfinity;
                std::function<CarrierEquation(std::uint32_t)> salvage;
                salvage = [&](const std::uint32_t state) -> CarrierEquation {
                    if (calc.is_goal_state(calc.state(state))) return {};
                    const auto cached = salvage_equations.find(state);
                    if (cached != salvage_equations.end()) {
                        return cached->second;
                    }
                    CarrierEquation equation;
                    const auto [finish, finish_operator] =
                        constructive_direct_action_upper(
                            state, finish_action);
                    if (progress_mask(state, acquisition) == acquisition &&
                        std::isfinite(finish)) {
                        equation.constant = finish;
                        salvage_operator[state] = finish_operator;
                        salvage_equations.emplace(state, equation);
                        return equation;
                    }
                    const bool exalt_relevant = exalt != kNoId &&
                        std::isfinite(exalt_cost) &&
                        (action_goal_reach_mask(exalt) & acquisition &
                         ~progress_mask(state, acquisition)) != 0 &&
                        action_legal(
                            session, calc.registry().actions.at(exalt),
                            calc.state(state));
                    if (exalt_relevant &&
                        std::isfinite(salvage_anchor_guess) &&
                        salvage_active.insert(state).second) {
                        const OutcomeDistribution& kernel =
                            calc.outcomes(state, exalt);
                        bool acyclic = kernel.supported &&
                            kernel.choice_groups.empty() &&
                            kernel.choice_options.empty();
                        for (const OutcomeEntry& outcome : kernel.entries) {
                            if (!calc.is_goal_state(calc.state(outcome.state)) &&
                                calc.state(outcome.state).prefix_count +
                                        calc.state(outcome.state).suffix_count <=
                                    calc.state(state).prefix_count +
                                        calc.state(state).suffix_count) {
                                acyclic = false;
                                break;
                            }
                        }
                        if (acyclic) {
                            CarrierEquation exalt_equation;
                            exalt_equation.constant = exalt_cost;
                            for (const OutcomeEntry& outcome : kernel.entries) {
                                const CarrierEquation continuation =
                                    salvage(outcome.state);
                                exalt_equation.constant +=
                                    outcome.probability *
                                    continuation.constant;
                                exalt_equation.restart_coefficient +=
                                    outcome.probability *
                                    continuation.restart_coefficient;
                            }
                            const double exalt_value =
                                exalt_equation.constant +
                                exalt_equation.restart_coefficient *
                                    salvage_anchor_guess;
                            const double restart_value = restart_cost +
                                salvage_anchor_guess;
                            if (exalt_value <
                                restart_value - options.epsilon) {
                                salvage_operator[state] = exalt;
                                salvage_active.erase(state);
                                salvage_equations.emplace(
                                    state, exalt_equation);
                                return exalt_equation;
                            }
                        }
                        salvage_active.erase(state);
                    }
                    equation.constant = restart_cost;
                    equation.restart_coefficient = 1.0;
                    salvage_operator[state] = restart_operator_index;
                    salvage_equations.emplace(state, equation);
                    return equation;
                };
                std::map<std::uint32_t, CarrierEquation> equations;
                std::map<std::uint32_t, CarrierEquation> direct_equations;
                std::map<std::uint32_t, CarrierEquation> early_equations;
                std::map<std::uint32_t, CarrierEquation>
                    intermediate_equations;
                std::unordered_map<std::uint32_t, std::uint32_t>
                    early_intermediate_state;
                std::unordered_map<std::uint32_t, std::uint32_t>
                    carrier_operator;
                std::unordered_map<std::uint32_t, std::uint32_t>
                    terminal_operator;
                std::unordered_map<std::uint32_t, double> terminal_constant;
                bool feasible = true;
                for (const auto& [carrier, probability] :
                     carrier_probability) {
                    if (!action_legal(
                            session, calc.registry().actions.at(regal),
                            calc.state(carrier))) {
                        feasible = false;
                        break;
                    }
                    const OutcomeDistribution& regal_kernel =
                        calc.outcomes(carrier, regal);
                    if (!regal_kernel.supported ||
                        !regal_kernel.choice_groups.empty() ||
                        !regal_kernel.choice_options.empty()) {
                        feasible = false;
                        break;
                    }
                    CarrierEquation equation;
                    equation.probability = probability;
                    equation.constant = regal_cost;
                    for (const OutcomeEntry& outcome : regal_kernel.entries) {
                        const CarrierEquation continuation =
                            salvage(outcome.state);
                        equation.constant += outcome.probability *
                            continuation.constant;
                        equation.restart_coefficient +=
                            outcome.probability *
                            continuation.restart_coefficient;
                    }
                    carrier_operator[carrier] = regal;

                    /* A deterministic goal bench can be installed on the
                     * magic carrier before Regal. This preserves the one
                     * acquired natural goal and lets Regal pursue the other
                     * natural goal directly. The two primitive kernels below
                     * are retained as ordinary executable policy states. */
                    if (std::popcount(
                            progress_mask(carrier, acquisition)) == 1 &&
                        (calc.state(carrier).flags & kFlagCraftedMod) == 0 &&
                        action_legal(
                            session,
                            calc.registry().actions.at(finish_action),
                            calc.state(carrier))) {
                        const OutcomeDistribution& bench_kernel =
                            calc.outcomes(carrier, finish_action);
                        if (bench_kernel.supported &&
                            bench_kernel.choice_groups.empty() &&
                            bench_kernel.choice_options.empty() &&
                            bench_kernel.entries.size() == 1 &&
                            std::abs(
                                bench_kernel.entries.front().probability -
                                1.0) <= 1e-12) {
                            const std::uint32_t benched =
                                bench_kernel.entries.front().state;
                            if (action_legal(
                                    session,
                                    calc.registry().actions.at(regal),
                                    calc.state(benched))) {
                                const OutcomeDistribution& early_regal =
                                    calc.outcomes(benched, regal);
                                if (early_regal.supported &&
                                    early_regal.choice_groups.empty() &&
                                    early_regal.choice_options.empty()) {
                                    CarrierEquation intermediate;
                                    intermediate.constant = regal_cost;
                                    for (const OutcomeEntry& outcome :
                                         early_regal.entries) {
                                        const CarrierEquation continuation =
                                            salvage(outcome.state);
                                        intermediate.constant +=
                                            outcome.probability *
                                            continuation.constant;
                                        intermediate.restart_coefficient +=
                                            outcome.probability *
                                            continuation.restart_coefficient;
                                    }
                                    equation.constant = finish_cost +
                                        intermediate.constant;
                                    equation.restart_coefficient =
                                        intermediate.restart_coefficient;
                                    early_equations[carrier] = equation;
                                    intermediate_equations[benched] =
                                        intermediate;
                                    early_intermediate_state[carrier] =
                                        benched;
                                }
                            }
                        }
                    }
                    /* `equation` may now contain the early-bench choice.
                     * Reconstruct the direct Regal equation when an early
                     * alternative was recorded so policy improvement can
                     * choose independently for every strict carrier. */
                    if (early_equations.contains(carrier)) {
                        CarrierEquation direct;
                        direct.probability = probability;
                        direct.constant = regal_cost;
                        for (const OutcomeEntry& outcome :
                             regal_kernel.entries) {
                            const CarrierEquation continuation =
                                salvage(outcome.state);
                            direct.constant += outcome.probability *
                                continuation.constant;
                            direct.restart_coefficient +=
                                outcome.probability *
                                continuation.restart_coefficient;
                        }
                        direct_equations.emplace(carrier, direct);
                    } else {
                        direct_equations.emplace(carrier, equation);
                    }
                }
                equations = direct_equations;
                if (!feasible || equations.empty()) continue;

                struct MagicAffine {
                    double constant = 0.0;
                    double restart_coefficient = 0.0;
                    double alteration_coefficient = 0.0;
                };
                struct MagicSolution {
                    double anchor_value = kInfinity;
                    double alteration_constant = 0.0;
                    double alteration_coefficient = 0.0;
                    double constant = 0.0;
                    double coefficient = 0.0;
                };
                const auto solve_magic = [&]() -> MagicSolution {
                    const auto expression_for = [&] (
                        const std::uint32_t state) {
                        MagicAffine expression;
                        const auto target = equations.find(state);
                        if (target != equations.end()) {
                            expression.constant = target->second.constant;
                            expression.restart_coefficient =
                                target->second.restart_coefficient;
                            return expression;
                        }
                        const auto augmented = augment_kernels.find(state);
                        if (augmented == augment_kernels.end()) {
                            expression.alteration_coefficient = 1.0;
                            return expression;
                        }
                        expression.constant = augment_cost;
                        for (const OutcomeEntry& outcome :
                             augmented->second->entries) {
                            const auto exit = equations.find(outcome.state);
                            if (exit == equations.end()) {
                                expression.alteration_coefficient +=
                                    outcome.probability;
                            } else {
                                expression.constant += outcome.probability *
                                    exit->second.constant;
                                expression.restart_coefficient +=
                                    outcome.probability *
                                    exit->second.restart_coefficient;
                            }
                        }
                        return expression;
                    };
                    MagicAffine alteration_expression;
                    alteration_expression.constant = alteration_cost;
                    for (const OutcomeEntry& outcome :
                         alteration_kernel.entries) {
                        const MagicAffine expression =
                            expression_for(outcome.state);
                        alteration_expression.constant +=
                            outcome.probability * expression.constant;
                        alteration_expression.restart_coefficient +=
                            outcome.probability *
                            expression.restart_coefficient;
                        alteration_expression.alteration_coefficient +=
                            outcome.probability *
                            expression.alteration_coefficient;
                    }
                    const double alteration_denominator =
                        1.0 -
                        alteration_expression.alteration_coefficient;
                    if (alteration_denominator <= 1e-15) return {};
                    MagicSolution solved;
                    solved.alteration_constant =
                        alteration_expression.constant /
                        alteration_denominator;
                    solved.alteration_coefficient =
                        alteration_expression.restart_coefficient /
                        alteration_denominator;
                    solved.constant = transmute_cost;
                    for (const OutcomeEntry& outcome :
                         transmute_kernel.entries) {
                        const MagicAffine expression =
                            expression_for(outcome.state);
                        solved.constant += outcome.probability *
                            (expression.constant +
                             expression.alteration_coefficient *
                                 solved.alteration_constant);
                        solved.coefficient += outcome.probability *
                            (expression.restart_coefficient +
                             expression.alteration_coefficient *
                                 solved.alteration_coefficient);
                    }
                    const double denominator = 1.0 - solved.coefficient;
                    if (denominator <= 1e-15) return {};
                    solved.anchor_value = solved.constant / denominator;
                    return solved;
                };
                MagicSolution solved = solve_magic();
                for (std::uint32_t improvement = 0;
                     improvement < 32 &&
                     std::isfinite(solved.anchor_value);
                     ++improvement) {
                    bool changed = false;
                    for (const auto& [carrier, early] : early_equations) {
                        const CarrierEquation& direct =
                            direct_equations.at(carrier);
                        const double direct_value = direct.constant +
                            direct.restart_coefficient * solved.anchor_value;
                        const double early_value = early.constant +
                            early.restart_coefficient * solved.anchor_value;
                        const bool choose_early =
                            early_value < direct_value - options.epsilon;
                        const bool was_early =
                            carrier_operator.at(carrier) == finish_action;
                        if (choose_early == was_early) continue;
                        equations[carrier] = choose_early ? early : direct;
                        carrier_operator[carrier] =
                            choose_early ? finish_action : regal;
                        changed = true;
                    }
                    if (!changed) break;
                    solved = solve_magic();
                }
                const double anchor_value = solved.anchor_value;
                const double alteration_constant =
                    solved.alteration_constant;
                const double alteration_coefficient =
                    solved.alteration_coefficient;
                const double constant = solved.constant;
                const double coefficient = solved.coefficient;
                if (!std::isfinite(anchor_value) ||
                    anchor_value >= best.anchor_state_value) {
                    continue;
                }

                const auto magic_expression = [&](const std::uint32_t state) {
                    MagicAffine expression;
                    const auto target = equations.find(state);
                    if (target != equations.end()) {
                        expression.constant = target->second.constant;
                        expression.restart_coefficient =
                            target->second.restart_coefficient;
                        return expression;
                    }
                    const auto augmented = augment_kernels.find(state);
                    if (augmented == augment_kernels.end()) {
                        expression.alteration_coefficient = 1.0;
                        return expression;
                    }
                    expression.constant = augment_cost;
                    for (const OutcomeEntry& outcome :
                         augmented->second->entries) {
                        const auto exit = equations.find(outcome.state);
                        if (exit == equations.end()) {
                            expression.alteration_coefficient +=
                                outcome.probability;
                        } else {
                            expression.constant +=
                                outcome.probability * exit->second.constant;
                            expression.restart_coefficient +=
                                outcome.probability *
                                exit->second.restart_coefficient;
                        }
                    }
                    return expression;
                };

                FocusedFallbackPolicy candidate;
                candidate.anchor_state = restart_state;
                candidate.anchor_state_value = anchor_value;
                candidate.finish_action = finish_action;
                candidate.progress_state_value[restart_state] = anchor_value;
                candidate.progress_state_operator[restart_state] = transmute;
                const double alteration_value =
                    alteration_constant +
                    alteration_coefficient * anchor_value;
                std::set<std::uint32_t> no_target_policy_states(
                    no_target_states.begin(), no_target_states.end());
                for (const OutcomeEntry& outcome : alteration_kernel.entries) {
                    if (progress_mask(outcome.state, acquisition) == 0) {
                        no_target_policy_states.insert(outcome.state);
                    }
                }
                for (const auto& [unused_state, kernel] : augment_kernels) {
                    (void)unused_state;
                    for (const OutcomeEntry& outcome : kernel->entries) {
                        if (progress_mask(outcome.state, acquisition) == 0) {
                            no_target_policy_states.insert(outcome.state);
                        }
                    }
                }
                for (const std::uint32_t state : no_target_policy_states) {
                    const MagicAffine expression = magic_expression(state);
                    candidate.progress_state_value[state] =
                        expression.constant +
                        expression.restart_coefficient * anchor_value +
                        expression.alteration_coefficient * alteration_value;
                    candidate.progress_state_operator[state] =
                        augment_kernels.contains(state) ? augment : alteration;
                }
                for (const auto& [state, equation] : equations) {
                    candidate.progress_state_value[state] =
                        equation.constant +
                        equation.restart_coefficient * anchor_value;
                    candidate.progress_state_operator[state] =
                        carrier_operator.at(state);
                }
                for (const auto& [state, equation] :
                     intermediate_equations) {
                    candidate.progress_state_value[state] =
                        equation.constant +
                        equation.restart_coefficient * anchor_value;
                    candidate.progress_state_operator[state] = regal;
                }
                for (const auto& [state, equation] : salvage_equations) {
                    candidate.progress_state_value[state] =
                        equation.constant +
                        equation.restart_coefficient * anchor_value;
                    candidate.progress_state_operator[state] =
                        salvage_operator.at(state);
                }
                for (const auto& [state, operator_index] :
                     terminal_operator) {
                    if (operator_index == restart_operator_index) {
                        candidate.progress_state_value[state] =
                            restart_cost + anchor_value;
                    } else {
                        candidate.progress_state_value[state] =
                            terminal_constant.at(state);
                    }
                    candidate.progress_state_operator[state] = operator_index;
                }
                ++result.diagnostics.constructive_policy_feasible_policies;
                retain_action_reason(
                    "included:magic_augment_regal_policy:" +
                    finite_json(anchor_value) + ":constant=" +
                    finite_json(constant) + ":retry=" +
                    finite_json(coefficient) + ":augment_states=" +
                    std::to_string(augment_kernels.size()));
                best = std::move(candidate);
            }
        }
        if (!std::isfinite(best.anchor_state_value)) return std::nullopt;
        return best;
    }

    std::optional<FocusedFallbackPolicy> destructive_rare_fallback() {
        if (restart_state == kNoId || restart_state >= calc.state_count() ||
            result.start_state >= calc.state_count()) {
            return std::nullopt;
        }
        const AbstractState& anchor = calc.state(restart_state);
        const AbstractState& rare_entry = calc.state(result.start_state);
        if (anchor.rarity != PC_RARITY_NORMAL ||
            anchor.prefix_count != 0 || anchor.suffix_count != 0 ||
            rare_entry.rarity != PC_RARITY_RARE ||
            (anchor.flags & (kFlagCraftedMod | kProtectionFlags)) != 0 ||
            (rare_entry.flags & (kFlagCraftedMod | kProtectionFlags)) != 0 ||
            anchor.fractured_goal_mask != 0 ||
            rare_entry.fractured_goal_mask != 0) {
            return std::nullopt;
        }
        const auto cost = [&](const std::uint32_t action) {
            if (action >= priced_operator_position.size()) return kInfinity;
            const std::int32_t position = priced_operator_position[action];
            if (position < 0) return kInfinity;
            const PricedOperator& priced =
                operators.at(static_cast<std::size_t>(position));
            const PlannerOperator& planner = calc.operators().at(priced.index);
            return planner.kind == PlannerOperatorKind::Primitive &&
                    planner.primitive_action == action &&
                    std::isfinite(priced.cost) && priced.cost >= 0.0
                ? priced.cost
                : kInfinity;
        };
        const auto destructive_to_rare = [&](const std::uint32_t action) {
            if (action >= calc.registry().actions.size()) return false;
            const ActionType type =
                calc.registry().actions.at(action).params.type;
            return type == ActionType::Alchemy ||
                   type == ActionType::Chaos ||
                   type == ActionType::Essence ||
                   type == ActionType::Fossil ||
                   type == ActionType::HarvestReforge;
        };
        const auto same_kernel = [](const OutcomeDistribution& left,
                                    const OutcomeDistribution& right) {
            return left.supported == right.supported &&
                   left.entries == right.entries &&
                   left.choice_groups == right.choice_groups &&
                   left.choice_options == right.choice_options;
        };
        struct Terminal {
            double value = kInfinity;
            std::uint32_t operation = kNoId;
        };
        FocusedFallbackPolicy best;
        best.anchor_state = restart_state;
        for (const std::uint32_t finish_action :
             calc.automatic_goal_bench_actions()) {
            const auto terminal = [&](const std::uint32_t state) {
                if (calc.is_goal_state(calc.state(state))) {
                    return Terminal{0.0, kNoId};
                }
                const auto [finish, operation] =
                    constructive_direct_action_upper(state, finish_action);
                return Terminal{finish, operation};
            };
            for (const std::uint32_t renewal_action : calc.candidates()) {
                if (!destructive_to_rare(renewal_action) ||
                    !std::isfinite(cost(renewal_action)) ||
                    !action_legal(
                        session,
                        calc.registry().actions.at(renewal_action),
                        rare_entry)) {
                    continue;
                }
                const OutcomeDistribution& renewal_kernel =
                    calc.outcomes(result.start_state, renewal_action);
                if (!renewal_kernel.supported ||
                    !renewal_kernel.choice_groups.empty() ||
                    !renewal_kernel.choice_options.empty()) {
                    continue;
                }
                double renewal_constant = cost(renewal_action);
                double success_probability = 0.0;
                std::vector<std::uint32_t> renewal_failures;
                std::unordered_map<std::uint32_t, Terminal>
                    renewal_terminals;
                bool exact_retry = true;
                for (const OutcomeEntry& outcome : renewal_kernel.entries) {
                    const Terminal finish = terminal(outcome.state);
                    if (std::isfinite(finish.value)) {
                        renewal_constant +=
                            outcome.probability * finish.value;
                        success_probability += outcome.probability;
                        renewal_terminals[outcome.state] = finish;
                    } else {
                        renewal_failures.push_back(outcome.state);
                        if (!action_legal(
                                session,
                                calc.registry().actions.at(renewal_action),
                                calc.state(outcome.state))) {
                            exact_retry = false;
                            break;
                        }
                    }
                }
                if (!exact_retry || success_probability <= 1e-15) continue;
                const double renewal_value =
                    renewal_constant / success_probability;
                if (!std::isfinite(renewal_value)) continue;

                for (const std::uint32_t setup_action : calc.candidates()) {
                    if (!destructive_to_rare(setup_action) ||
                        !std::isfinite(cost(setup_action)) ||
                        !action_legal(
                            session,
                            calc.registry().actions.at(setup_action),
                            anchor)) {
                        continue;
                    }
                    const OutcomeDistribution& setup_kernel =
                        calc.outcomes(restart_state, setup_action);
                    if (!setup_kernel.supported ||
                        !setup_kernel.choice_groups.empty() ||
                        !setup_kernel.choice_options.empty()) {
                        continue;
                    }
                    double anchor_value = cost(setup_action);
                    bool feasible = true;
                    std::unordered_map<std::uint32_t, Terminal>
                        setup_terminals;
                    std::vector<std::uint32_t> setup_failures;
                    for (const OutcomeEntry& outcome : setup_kernel.entries) {
                        const Terminal finish = terminal(outcome.state);
                        if (std::isfinite(finish.value)) {
                            anchor_value +=
                                outcome.probability * finish.value;
                            setup_terminals[outcome.state] = finish;
                        } else if (action_legal(
                            session,
                            calc.registry().actions.at(renewal_action),
                            calc.state(outcome.state))) {
                            anchor_value +=
                                outcome.probability * renewal_value;
                            setup_failures.push_back(outcome.state);
                        } else {
                            feasible = false;
                            break;
                        }
                    }
                    if (!feasible || !std::isfinite(anchor_value) ||
                        anchor_value >= best.anchor_state_value) {
                        continue;
                    }
                    FocusedFallbackPolicy candidate;
                    candidate.anchor_state = restart_state;
                    candidate.anchor_state_value = anchor_value;
                    candidate.finish_action = finish_action;
                    candidate.progress_state_value[restart_state] =
                        anchor_value;
                    candidate.progress_state_operator[restart_state] =
                        setup_action;
                    candidate.progress_state_value[result.start_state] =
                        renewal_value;
                    candidate.progress_state_operator[result.start_state] =
                        renewal_action;
                    for (const std::uint32_t state : renewal_failures) {
                        candidate.progress_state_value[state] =
                            renewal_value;
                        candidate.progress_state_operator[state] =
                            renewal_action;
                    }
                    for (const std::uint32_t state : setup_failures) {
                        candidate.progress_state_value[state] =
                            renewal_value;
                        candidate.progress_state_operator[state] =
                            renewal_action;
                    }
                    const auto add_terminals = [&](const auto& terminals) {
                        for (const auto& [state, finish] : terminals) {
                            candidate.progress_state_value[state] =
                                finish.value;
                            candidate.progress_state_operator[state] =
                                finish.operation;
                        }
                    };
                    add_terminals(renewal_terminals);
                    add_terminals(setup_terminals);
                    best = std::move(candidate);
                }
            }
        }
        if (!std::isfinite(best.anchor_state_value)) return std::nullopt;
        return best;
    }

    std::optional<FocusedFallbackPolicy> focused_fallback() {
        const std::uint32_t renewal_source = result.start_state;
        ++result.diagnostics.constructive_policy_anchor_checks;
        if (renewal_source >= expanded.size() ||
            !expanded[renewal_source] ||
            renewal_source >= transition_cache->state_rows.size() ||
            restart_state == kNoId || restart_state >= expanded.size() ||
            !expanded[restart_state] ||
            restart_state >= transition_cache->state_rows.size()) {
            return std::nullopt;
        }
        std::optional<FocusedFallbackPolicy> progress_fallback =
            magic_regal_fallback();
        const AbstractState& renewal_carrier = calc.state(renewal_source);
        if (renewal_carrier.prefix_count != 0 ||
            renewal_carrier.suffix_count != 0 ||
            (renewal_carrier.flags & kFlagCraftedMod) != 0 ||
            renewal_carrier.fractured_goal_mask != 0 ||
            renewal_carrier.fractured_metamod_flags != 0 ||
            (renewal_carrier.flags & kProtectionFlags) != 0) {
            return progress_fallback;
        }
        for (const std::uint8_t count :
             renewal_carrier.fractured_junk_counts) {
            if (count != 0) return progress_fallback;
        }
        ++result.diagnostics.constructive_policy_anchor_eligible;
        const StateRowSpan& span =
            transition_cache->state_rows.at(renewal_source);
        FocusedFallbackPolicy best;
        best.anchor_state = restart_state;
        best.renewal_rarity = renewal_carrier.rarity;
        best.renewal_influence_bits = renewal_carrier.influence_bits;
        best.renewal_searing_exarch_tier =
            renewal_carrier.searing_exarch_tier;
        best.renewal_eater_of_worlds_tier =
            renewal_carrier.eater_of_worlds_tier;
        for (std::uint32_t relative = 0; relative < span.count; ++relative) {
            const std::uint64_t absolute = span.offset + relative;
            const SparseRow& row = transition_cache->rows.at(absolute);
            if (row.choice_count != 0) continue;
            for (std::uint32_t variant_offset = 0;
                 variant_offset < row.variant_count; ++variant_offset) {
                const SparseVariant& variant =
                    transition_cache->variant_arena->variants.at(
                        transition_cache->variant_arena
                            ->row_variant_indices.at(
                                row.variant_offset + variant_offset));
                const PlannerOperator& renewal =
                    calc.operators().at(variant.operator_index);
                if (renewal.automatic_kind !=
                        AutomaticCandidateKind::ConstructiveRenewal ||
                    renewal.constructive_finish_action == kNoId) {
                    continue;
                }
                ++result.diagnostics.constructive_policy_renewal_variants;
                double renewal_cost = 0.0;
                if (!priced_variant_cost(variant, renewal_cost) ||
                    !std::isfinite(renewal_cost) || renewal_cost < 0.0) {
                    continue;
                }
                double loop_probability = row.self_probability;
                double constant = renewal_cost;
                bool has_finish = false;
                for (std::uint32_t i = 0; i < row.transition_count; ++i) {
                    const std::uint64_t offset = row.transition_offset + i;
                    const std::uint32_t successor =
                        transition_cache->successors.at(offset);
                    const double probability =
                        transition_cache->probabilities.at(offset);
                    if (successor == renewal_source) {
                        loop_probability += probability;
                        continue;
                    }
                    if (calc.is_goal_state(calc.state(successor))) {
                        has_finish = true;
                        continue;
                    }
                    ++result.diagnostics.constructive_policy_exit_checks;
                    const auto [finish, unused_operator] =
                        constructive_direct_action_upper(
                            successor,
                            renewal.constructive_finish_action);
                    (void)unused_operator;
                    if (std::isfinite(finish)) {
                        ++result.diagnostics
                              .constructive_policy_finishable_exits;
                        constant += probability * finish;
                        has_finish = true;
                    } else {
                        /* The approved destructive renewal is applied again
                         * on this non-fractured exit carrier. Its next
                         * attempt has the same exact clean-carrier kernel;
                         * no synthetic Restart or free setup is implied. */
                        loop_probability += probability;
                    }
                }
                const double denominator = 1.0 - loop_probability;
                if (!has_finish || denominator <= 1e-15) continue;
                const double value = constant / denominator;
                if (!std::isfinite(value) || value >= kValueCeiling) {
                    continue;
                }
                ++result.diagnostics.constructive_policy_feasible_policies;
                if (value < best.renewal_state_value - options.epsilon ||
                    (std::abs(value - best.renewal_state_value) <=
                         options.epsilon &&
                     std::tie(
                         absolute, variant.operator_index,
                         renewal.constructive_finish_action) <
                         std::tie(
                             best.renewal_row, best.renewal_operator,
                             best.finish_action))) {
                    best.renewal_state_value = value;
                    best.renewal_row = absolute;
                    best.renewal_operator = variant.operator_index;
                    best.finish_action =
                        renewal.constructive_finish_action;
                }
            }
        }
        if (!std::isfinite(best.renewal_state_value)) return progress_fallback;

        if (renewal_fallback_eligible(best.anchor_state, best)) {
            best.anchor_state_value = best.renewal_state_value;
            if (progress_fallback.has_value() &&
                progress_fallback->anchor_state_value <
                    best.anchor_state_value) {
                return progress_fallback;
            }
            return best;
        }

        /* Restart returns the engine's real fresh carrier, which need not
         * have the rarity of the diagnostic start. Compose an exact setup row
         * at that carrier with the renewable rare-state fallback; never
         * substitute the original start state for Restart's successor. */
        const StateRowSpan& anchor_span =
            transition_cache->state_rows.at(best.anchor_state);
        for (std::uint32_t relative = 0;
             relative < anchor_span.count; ++relative) {
            const std::uint64_t absolute = anchor_span.offset + relative;
            const SparseRow& row = transition_cache->rows.at(absolute);
            const PricedSparseRow& priced = priced_rows.at(absolute);
            if (priced.operator_index == kNoId ||
                !std::isfinite(priced.cost) || priced.cost < 0.0) {
                continue;
            }
            double constant = priced.cost;
            double loop_probability = row.self_probability;
            bool feasible = true;
            for (std::uint32_t i = 0; i < row.transition_count; ++i) {
                const std::uint64_t offset = row.transition_offset + i;
                const std::uint32_t successor =
                    transition_cache->successors.at(offset);
                if (successor == best.anchor_state) {
                    loop_probability +=
                        transition_cache->probabilities.at(offset);
                    continue;
                }
                const double continuation =
                    fallback_terminal_upper(successor, best);
                if (!std::isfinite(continuation)) {
                    feasible = false;
                    break;
                }
                constant += transition_cache->probabilities.at(offset) *
                            continuation;
            }
            if (!feasible) continue;
            for (std::uint32_t i = 0; i < row.choice_count; ++i) {
                const SparseChoiceGroup& group =
                    transition_cache->choices.at(row.choice_offset + i);
                double selected = group.has_self ? kInfinity : kInfinity;
                for (std::uint32_t s = 0;
                     s < group.successor_count; ++s) {
                    selected = std::min(
                        selected,
                        fallback_terminal_upper(
                            transition_cache->choice_successors.at(
                                group.successor_offset + s),
                            best));
                }
                if (group.has_self) {
                    /* The observation can explicitly choose the carrier and
                     * therefore selects self only if its solved value beats
                     * every executable alternate. Conservatively treating
                     * self as the whole group remains feasible. */
                    loop_probability += group.probability;
                } else if (std::isfinite(selected)) {
                    constant += group.probability * selected;
                } else {
                    feasible = false;
                    break;
                }
            }
            const double denominator = 1.0 - loop_probability;
            if (!feasible || denominator <= 1e-15) continue;
            const double value = constant / denominator;
            best.anchor_state_value = std::min(
                best.anchor_state_value, value);
        }
        if (!std::isfinite(best.anchor_state_value)) return progress_fallback;
        if (progress_fallback.has_value() &&
            progress_fallback->anchor_state_value < best.anchor_state_value) {
            return progress_fallback;
        }
        return best;
    }

    double focused_start_upper_bound(
        const FocusedFallbackPolicy& fallback) const {
        if (result.start_state >= transition_cache->state_rows.size()) {
            return kInfinity;
        }
        const double failure_value =
            restart_cost + fallback.anchor_state_value;
        const auto continuation_upper = [&](const std::uint32_t state) {
            if (state == fallback.anchor_state) {
                return fallback.anchor_state_value;
            }
            return std::min(
                failure_value, fallback_terminal_upper(state, fallback));
        };
        double best = continuation_upper(result.start_state);
        const StateRowSpan& span =
            transition_cache->state_rows.at(result.start_state);
        for (std::uint32_t relative = 0; relative < span.count; ++relative) {
            const std::uint64_t absolute = span.offset + relative;
            const SparseRow& row = transition_cache->rows.at(absolute);
            const PricedSparseRow& priced = priced_rows.at(absolute);
            if (priced.operator_index == kNoId ||
                !std::isfinite(priced.cost) || priced.cost < 0.0) {
                continue;
            }
            double constant = priced.cost;
            for (std::uint32_t i = 0; i < row.transition_count; ++i) {
                const std::uint64_t offset = row.transition_offset + i;
                const std::uint32_t successor =
                    transition_cache->successors.at(offset);
                if (successor == result.start_state ||
                    calc.is_goal_state(calc.state(successor))) {
                    continue;
                }
                constant += transition_cache->probabilities.at(offset) *
                            continuation_upper(successor);
            }
            std::vector<std::pair<double, double>> self_choices;
            for (std::uint32_t i = 0; i < row.choice_count; ++i) {
                const SparseChoiceGroup& group =
                    transition_cache->choices.at(row.choice_offset + i);
                double alternate = kInfinity;
                for (std::uint32_t s = 0; s < group.successor_count; ++s) {
                    alternate = std::min(
                        alternate,
                        continuation_upper(
                            transition_cache->choice_successors.at(
                                group.successor_offset + s)));
                }
                if (group.has_self) {
                    self_choices.push_back(
                        {alternate, group.probability});
                } else if (std::isfinite(alternate)) {
                    constant += group.probability * alternate;
                } else {
                    constant = kInfinity;
                    break;
                }
            }
            if (!std::isfinite(constant)) continue;
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
                    best = std::min(best, value);
                    break;
                }
                const auto [alternate, probability] =
                    self_choices[fixed_choices++];
                if (!std::isfinite(alternate)) break;
                constant += probability * alternate;
                loop_probability -= probability;
            }
        }
        return best;
    }

    std::pair<double, std::uint64_t> focused_direct_state_upper(
        const std::uint32_t state) const {
        const std::uint64_t no_row =
            std::numeric_limits<std::uint64_t>::max();
        if (state >= transition_cache->state_rows.size()) {
            return {kInfinity, no_row};
        }
        double best = kInfinity;
        std::uint64_t best_row = no_row;
        const StateRowSpan& span =
            transition_cache->state_rows.at(state);
        for (std::uint32_t relative = 0; relative < span.count; ++relative) {
            const std::uint64_t absolute = span.offset + relative;
            const SparseRow& row = transition_cache->rows.at(absolute);
            const PricedSparseRow& priced = priced_rows.at(absolute);
            if (priced.operator_index == kNoId ||
                !std::isfinite(priced.cost) || priced.cost < 0.0) {
                continue;
            }
            double goal_probability = 0.0;
            double self_probability = row.self_probability;
            bool feasible = true;
            for (std::uint32_t i = 0; i < row.transition_count; ++i) {
                const std::uint64_t offset = row.transition_offset + i;
                const std::uint32_t successor =
                    transition_cache->successors.at(offset);
                if (successor == state) continue;
                if (!calc.is_goal_state(calc.state(successor))) {
                    feasible = false;
                    break;
                }
                goal_probability +=
                    transition_cache->probabilities.at(offset);
            }
            if (!feasible) continue;
            for (std::uint32_t i = 0; i < row.choice_count; ++i) {
                const SparseChoiceGroup& group =
                    transition_cache->choices.at(row.choice_offset + i);
                bool has_goal = false;
                for (std::uint32_t s = 0; s < group.successor_count; ++s) {
                    has_goal |= calc.is_goal_state(calc.state(
                        transition_cache->choice_successors.at(
                            group.successor_offset + s)));
                }
                if (has_goal) {
                    goal_probability += group.probability;
                } else if (group.has_self) {
                    self_probability += group.probability;
                } else {
                    feasible = false;
                    break;
                }
            }
            const double total = goal_probability + self_probability;
            if (!feasible || goal_probability <= 0.0 ||
                std::abs(total - 1.0) > 1e-9) {
                continue;
            }
            const double value = priced.cost / goal_probability;
            if (value < best - options.epsilon ||
                (std::abs(value - best) <= options.epsilon &&
                 absolute < best_row)) {
                best = value;
                best_row = absolute;
            }
        }
        return {best, best_row};
    }

    std::pair<double, std::uint64_t> focused_direct_start_upper() const {
        return focused_direct_state_upper(result.start_state);
    }

    void reset_focused_optimization_state() {
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
        if (!result.diagnostics.policy_evaluation_failure.starts_with(
                "selected_byte_cap_breakdown:")) {
            result.diagnostics.policy_evaluation_failure.clear();
        }
    }

    void schedule_next_focused_expansion(
        std::vector<std::uint32_t> fringe,
        const bool complete,
        const std::vector<double>& priority) {
        queue.clear();
        queued.assign(calc.state_count(), 0);
        for (std::uint32_t state = 0; state < expanded.size(); ++state) {
            if (expanded[state]) queued[state] = 1;
        }
        if (!complete) {
            /* An incomplete policy walk cannot certify that it found the
             * whole relevant frontier. Retain exactness by admitting every
             * state already discovered by strict transition generation. */
            for (std::uint32_t state = 0; state < calc.state_count(); ++state) {
                if (!queued[state]) fringe.push_back(state);
            }
        }
        std::sort(fringe.begin(), fringe.end());
        fringe.erase(std::unique(fringe.begin(), fringe.end()), fringe.end());
        std::stable_sort(
            fringe.begin(), fringe.end(),
            [&](const std::uint32_t left, const std::uint32_t right) {
                const double left_priority =
                    left < priority.size() ? priority[left] : 0.0;
                const double right_priority =
                    right < priority.size() ? priority[right] : 0.0;
                return left_priority != right_priority
                           ? left_priority > right_priority
                           : left < right;
            });
        /* A focused round is a work schedule, not a quotient. Bound a broad
         * reforge outcome fringe while retaining strict IDs; later rounds
         * select the next unexpanded members of the same coarse class. */
        const std::uint32_t members_per_class = std::max<std::uint32_t>(
            1, options.focused_members_per_fringe_class);
        auto [coarse, coarse_count] = exact_partition(
            calc.state_count(), [&](const std::uint32_t state) {
                return focused_schedule_signature(state);
            });
        std::vector<std::uint32_t> selected_per_class(coarse_count, 0);
        std::vector<std::uint32_t> selected_fringe;
        selected_fringe.reserve(
            std::min<std::size_t>(
                fringe.size(),
                static_cast<std::uint64_t>(coarse_count) *
                    members_per_class));
        if (!focused_fallback_policy.has_value() &&
            restart_state != kNoId && restart_state < queued.size() &&
            !queued[restart_state]) {
            selected_fringe.push_back(restart_state);
            ++selected_per_class[coarse.at(restart_state)];
        }
        for (const std::uint32_t state : fringe) {
            if (selected_fringe.size() >=
                options.focused_expansion_batch_states) {
                break;
            }
            if (queued.at(state) ||
                (state == restart_state &&
                 !focused_fallback_policy.has_value())) {
                continue;
            }
            const std::uint32_t candidate = coarse.at(state);
            if (selected_per_class[candidate] >= members_per_class) {
                continue;
            }
            ++selected_per_class[candidate];
            selected_fringe.push_back(state);
        }
        fringe = std::move(selected_fringe);
        for (const std::uint32_t state : fringe) enqueue(state);
        if (queue.empty()) {
            /* No scheduling filter is a closure proof. If the lower-selected
             * fringe produced no work, resume strict closure from every
             * discovered unexpanded state. Only an actually exhausted strict
             * graph may set the full-closure flag. */
            for (std::uint32_t state = 0;
                 state < calc.state_count(); ++state) {
                if (state >= expanded.size() || !expanded[state]) {
                    enqueue(state);
                }
            }
            if (queue.empty()) {
                focused_mode = false;
                full_closure_after_focused_fallback = true;
            } else {
                full_closure_after_focused_fallback = false;
            }
        }
        peak_queue_size = std::max<std::uint32_t>(
            peak_queue_size, static_cast<std::uint32_t>(queue.size()));
        focus_optimizing = false;
        focused_lower_mode = false;
        reset_focused_optimization_state();
    }

    bool begin_focused_upper_solve() {
        if (!focused_fallback_policy.has_value() ||
            focused_strict_transition_cache != nullptr ||
            !std::isfinite(restart_cost) || restart_cost < 0.0) {
            return false;
        }
        const FocusedFallbackPolicy& fallback = *focused_fallback_policy;
        const double frontier_upper =
            restart_cost + fallback.anchor_state_value;
        if (!std::isfinite(frontier_upper) ||
            frontier_upper >= kValueCeiling) {
            return false;
        }
        focused_round_lower_values = std::move(result.values);
        focused_round_lower_policy_rows = std::move(policy_rows);
        reset_focused_optimization_state();
        result.values.assign(calc.state_count(), frontier_upper);
        focused_frontier_upper_operator.assign(
            calc.state_count(), restart_operator_index);
        for (std::uint32_t state = 0; state < result.values.size(); ++state) {
            if (result.goal_states[state]) {
                result.values[state] = 0.0;
                focused_frontier_upper_operator[state] = kNoId;
                continue;
            }
            std::uint32_t terminal_operator = kNoId;
            const double terminal = fallback_terminal_upper(
                state, fallback, &terminal_operator);
            if (terminal < result.values[state]) {
                result.values[state] = terminal;
                focused_frontier_upper_operator[state] = terminal_operator;
            }
        }
        if (fallback.anchor_state < result.values.size()) {
            result.values[fallback.anchor_state] =
                fallback.anchor_state_value;
        }
        /* Every finite frontier terminal means the executable policy
         * `Restart; fallback-at-anchor`. Howard iteration is therefore
         * optimizing a real feasible policy on the expanded exact graph, not
         * a heuristic terminal estimate. The ranked initializer supplies a
         * proper first row at every expanded state. */
        focused_upper_mode = true;
        focus_optimizing = true;
        focused_lower_mode = true;
        policy_unit_stage = PolicyUnitStage::InitialSelect;
        return true;
    }

    void sync_constructive_discovered_states() {
        const std::uint32_t state_count = calc.state_count();
        const std::uint32_t previous =
            static_cast<std::uint32_t>(result.values.size());
        if (state_count <= previous) return;
        transition_cache->state_rows.resize(state_count);
        transition_cache->discovered_states = state_count;
        expanded.resize(state_count, 0);
        queued.resize(state_count, 0);
        result.expanded.resize(state_count, 0);
        result.goal_states.resize(state_count, 0);
        result.values.resize(state_count, 0.0);
        for (std::uint32_t state = previous; state < state_count; ++state) {
            if (calc.is_goal_state(calc.state(state))) {
                result.goal_states[state] = 1;
                continue;
            }
            result.values[state] =
                optimistic_completion_cost_for_state(state);
        }
    }

    void finish_focused_lower_solve(
        const bool allow_upper_pass = true) {
        if (allow_upper_pass) {
            ++result.diagnostics.focused_expansion_rounds;
            result.diagnostics.focused_lower_bound =
                result.values.at(result.start_state);
            result.diagnostics.focused_expansion_ns +=
                result.diagnostics.optimization_ns;
            focused_fallback_policy = focused_fallback();
            prepare_strict_clean_goal_cover();
            sync_constructive_discovered_states();
            if (strict_clean_goal_cover_refresh_needed) {
                strict_clean_goal_cover_refresh_needed = false;
                begin_focused_lower_solve();
                return;
            }
            if (begin_focused_upper_solve()) return;
        }
        std::vector<std::uint32_t> fringe;
        std::vector<double> fringe_priority;
        const bool complete = collect_focused_fringe(
            fringe, fringe_priority);
        std::stable_sort(
            fringe.begin(), fringe.end(),
            [&](const std::uint32_t left, const std::uint32_t right) {
                const double left_priority = fringe_priority[left];
                const double right_priority = fringe_priority[right];
                return left_priority != right_priority
                           ? left_priority > right_priority
                           : left < right;
            });
        focused_closure_proved = complete && fringe.empty();
        if (!focused_closure_proved && !focused_pending_upper_fringe.empty()) {
            std::stable_sort(
                focused_pending_upper_fringe.begin(),
                focused_pending_upper_fringe.end(),
                [&](const std::uint32_t left, const std::uint32_t right) {
                    const double left_priority =
                        left < focused_pending_upper_priority.size()
                            ? focused_pending_upper_priority[left]
                            : 0.0;
                    const double right_priority =
                        right < focused_pending_upper_priority.size()
                            ? focused_pending_upper_priority[right]
                            : 0.0;
                    return left_priority != right_priority
                               ? left_priority > right_priority
                               : left < right;
                });
            /* Reserve half of every exact expansion batch for each bound.
             * Combining unlike lower-gap and incumbent-improvement scores in
             * one scalar allowed either policy to starve the other. The
             * balanced union changes only work order and remains complete. */
            const std::size_t batch = std::max<std::uint32_t>(
                1, options.focused_expansion_batch_states);
            const std::size_t lower_quota = std::min<std::size_t>(
                batch, options.focused_lower_batch_states);
            const std::size_t upper_quota = batch - lower_quota;
            std::vector<std::uint8_t> selected(result.values.size(), 0);
            std::vector<std::uint32_t> balanced;
            balanced.reserve(batch);
            const auto take = [&](const std::vector<std::uint32_t>& source,
                                  const std::size_t quota) {
                std::size_t admitted = 0;
                for (const std::uint32_t state : source) {
                    if (balanced.size() >= batch || admitted >= quota) break;
                    if (state >= selected.size() || selected[state]) continue;
                    selected[state] = 1;
                    balanced.push_back(state);
                    ++admitted;
                    if (state < focused_pending_upper_priority.size()) {
                        fringe_priority[state] = std::max(
                            fringe_priority[state],
                            focused_pending_upper_priority[state]);
                    }
                }
            };
            take(fringe, lower_quota);
            take(focused_pending_upper_fringe, upper_quota);
            if (balanced.size() < batch) take(fringe, batch);
            if (balanced.size() < batch) {
                take(focused_pending_upper_fringe, batch);
            }
            fringe = std::move(balanced);
        }
        focused_pending_upper_fringe.clear();
        focused_pending_upper_priority.clear();
        focused_pending_upper_complete = false;

        if (focused_strict_transition_cache != nullptr) {
            std::vector<std::uint32_t> strict_fringe;
            /* Amortize refinement of the intentionally broad exact
             * zero-terminal frontier class. These remain strict IDs selected
             * only for work scheduling; the next lower solve may distinguish
             * every member through its complete all-action rows. */
            const std::uint32_t members_per_class =
                std::max<std::uint32_t>(
                    1, options.focused_members_per_fringe_class);
            std::vector<std::uint8_t> fringe_class(
                result.values.size(), 0);
            std::vector<std::uint32_t> selected_per_class(
                result.values.size(), 0);
            for (const std::uint32_t representative : fringe) {
                fringe_class.at(representative) = 1;
            }
            strict_fringe.reserve(
                focused_behavioral_representative.size());
            for (std::uint32_t state = 0;
                 state < focused_behavioral_representative.size(); ++state) {
                const std::uint32_t representative =
                    focused_behavioral_representative[state];
                if (!fringe_class.at(representative) ||
                    selected_per_class.at(representative) >=
                        members_per_class ||
                    (state < focused_strict_expanded.size() &&
                     focused_strict_expanded[state])) {
                    continue;
                }
                ++selected_per_class[representative];
                strict_fringe.push_back(state);
            }
            const std::vector<double> quotient_values = result.values;
            for (std::uint32_t state = 0;
                 state < focused_behavioral_representative.size(); ++state) {
                const std::uint32_t representative =
                    focused_behavioral_representative[state];
                result.values[state] = quotient_values.at(representative);
            }
            fringe = std::move(strict_fringe);
            transition_cache = std::move(focused_strict_transition_cache);
            expanded = std::move(focused_strict_expanded);
            expanded_count = focused_strict_expanded_count;
            focused_strict_expanded_count = 0;
            focused_behavioral_representative.clear();
            result.behavioral_representative_by_state.clear();
            result.expanded = expanded;
            priced_rows.clear();
            pricing_diagnostics_cursor = 0;
            policy_rows.clear();
            prepare_priced_rows();
        } else if (!fringe.empty()) {
            /* Bound each focused round without treating the scheduling
             * classes as solver equivalence. All selected IDs remain strict
             * states, and subsequent exact rows may distinguish every one. */
            const std::uint32_t members_per_class =
                std::max<std::uint32_t>(
                    1, options.focused_members_per_fringe_class);
            auto [coarse, coarse_count] = exact_partition(
                calc.state_count(), [&](const std::uint32_t state) {
                    return focused_schedule_signature(state);
                });
            std::vector<std::uint32_t> selected_per_class(coarse_count, 0);
            std::vector<std::uint32_t> selected_fringe;
            selected_fringe.reserve(fringe.size());
            for (const std::uint32_t state : fringe) {
                const std::uint32_t candidate = coarse.at(state);
                if (selected_per_class[candidate] >= members_per_class) {
                    continue;
                }
                ++selected_per_class[candidate];
                selected_fringe.push_back(state);
            }
            fringe = std::move(selected_fringe);
        }

        focused_pending_lower_fringe = std::move(fringe);
        const auto [direct_upper, direct_row] =
            focused_direct_start_upper();
        if (std::isfinite(direct_upper)) {
            if (direct_upper < result.diagnostics.focused_upper_bound) {
                result.diagnostics.focused_upper_bound = direct_upper;
                focused_direct_upper_row = direct_row;
            }
            result.diagnostics.focused_optimality_gap = std::max(
                0.0,
                result.diagnostics.focused_upper_bound -
                    result.diagnostics.focused_lower_bound);
            const double proof_tolerance = options.epsilon *
                std::max(1.0, std::abs(
                    result.diagnostics.focused_upper_bound)) * 10.0;
            if (focused_direct_upper_row !=
                    std::numeric_limits<std::uint64_t>::max() &&
                std::abs(
                    result.diagnostics.focused_upper_bound -
                    result.diagnostics.focused_lower_bound) <=
                    proof_tolerance) {
                const std::uint64_t no_row =
                    std::numeric_limits<std::uint64_t>::max();
                policy_rows.assign(result.values.size(), no_row);
                policy_rows[result.start_state] =
                    focused_direct_upper_row;
                result.values[result.start_state] =
                    result.diagnostics.focused_upper_bound;
                result.diagnostics.focused_lower_bound =
                    result.diagnostics.focused_upper_bound;
                result.diagnostics.focused_optimality_gap = 0.0;
                focused_bound_proved = true;
                focused_closure_proved = true;
                focused_previous_upper_values = std::move(result.values);
                queue.clear();
                focus_optimizing = false;
                focused_lower_mode = false;
                return;
            }
        }
        if (focused_fallback_policy.has_value()) {
            const double fallback_start = focused_start_upper_bound(
                *focused_fallback_policy);
            result.diagnostics.focused_upper_bound = std::min(
                result.diagnostics.focused_upper_bound, fallback_start);
            result.diagnostics.focused_optimality_gap = std::max(
                0.0,
                result.diagnostics.focused_upper_bound -
                result.diagnostics.focused_lower_bound);
        }
        if (std::isfinite(focused_partial_upper_bound) &&
            focused_previous_upper_values.size() == result.values.size() &&
            focused_previous_upper_policy_rows.size() == result.values.size()) {
            const double proof_tolerance = options.epsilon *
                std::max(1.0, std::abs(focused_partial_upper_bound)) * 10.0;
            if (std::abs(
                    focused_partial_upper_bound -
                    result.diagnostics.focused_lower_bound) <=
                proof_tolerance) {
                result.values = focused_previous_upper_values;
                policy_rows = focused_previous_upper_policy_rows;
                result.diagnostics.focused_upper_bound =
                    focused_partial_upper_bound;
                result.diagnostics.focused_lower_bound =
                    focused_partial_upper_bound;
                result.diagnostics.focused_optimality_gap = 0.0;
                focused_bound_proved = true;
                focused_closure_proved = true;
                queue.clear();
                focus_optimizing = false;
                focused_lower_mode = false;
                return;
            }
        }
        if (!std::isfinite(direct_upper) &&
            !focused_fallback_policy.has_value() &&
            (restart_state == kNoId ||
             restart_state >= expanded.size() ||
             !expanded[restart_state])) {
            if (restart_state != kNoId) {
                focused_pending_lower_fringe.push_back(restart_state);
            }
            focused_closure_proved = false;
            schedule_next_focused_expansion(
                std::move(focused_pending_lower_fringe), complete,
                fringe_priority);
            return;
        }
        if (focused_closure_proved) {
            /* The lower-selected policy reaches no optimistic frontier, so
             * it is itself a feasible policy with the same value as the
             * global lower bound. This is an exact bracket even when the
             * generic clean-base renewal route is unavailable. */
            focused_bound_proved = true;
            result.diagnostics.focused_upper_bound =
                result.diagnostics.focused_lower_bound;
            result.diagnostics.focused_optimality_gap = 0.0;
            focused_previous_upper_values = std::move(result.values);
            queue.clear();
            focus_optimizing = false;
            focused_lower_mode = false;
            return;
        }
        schedule_next_focused_expansion(
            std::move(focused_pending_lower_fringe), complete,
            fringe_priority);
    }

    void finish_focused_upper_solve(const bool succeeded) {
        result.diagnostics.focused_expansion_ns +=
            result.diagnostics.optimization_ns;
        focused_pending_upper_fringe.clear();
        focused_pending_upper_priority.clear();
        focused_pending_upper_complete = false;
        if (succeeded && result.start_state < result.values.size() &&
            std::isfinite(result.values[result.start_state])) {
            focused_pending_upper_complete = collect_focused_fringe(
                focused_pending_upper_fringe,
                focused_pending_upper_priority,
                &focused_round_lower_values);
            if (focused_pending_upper_complete) {
                focused_partial_upper_bound =
                    result.values[result.start_state];
                result.diagnostics.focused_partial_policy_upper_bound =
                    focused_partial_upper_bound;
                ++result.diagnostics.focused_partial_policy_rounds;
                focused_previous_upper_values = result.values;
                focused_previous_upper_policy_rows = policy_rows;
                focused_previous_frontier_upper_operator =
                    focused_frontier_upper_operator;
                result.diagnostics.focused_upper_bound = std::min(
                    result.diagnostics.focused_upper_bound,
                    focused_partial_upper_bound);
                result.diagnostics.focused_optimality_gap = std::max(
                    0.0,
                    result.diagnostics.focused_upper_bound -
                        result.diagnostics.focused_lower_bound);
            }
        }
        result.values = std::move(focused_round_lower_values);
        policy_rows = std::move(focused_round_lower_policy_rows);
        focused_frontier_upper_operator.clear();
        focused_upper_mode = false;
        policy_iteration_failed = false;
        policy_initialized = true;
        policy_stable = true;
        reset_policy_iteration_units();
        finish_focused_lower_solve(false);
    }

    void run_focused_lower_unit() {
        if (!policy_iteration_failed) {
            if (!run_policy_iteration_unit()) {
                if (focused_upper_mode) {
                    finish_focused_upper_solve(false);
                    return;
                }
                /* Focused lower bounds cannot use the descending prioritized
                 * fallback. Preserve the bound and resume full expansion.
                 * The absence of a focused queue is not a closure proof: make
                 * every discovered strict frontier state available before
                 * leaving focused mode. */
                queue.clear();
                queued.assign(calc.state_count(), 0);
                for (std::uint32_t state = 0;
                     state < calc.state_count(); ++state) {
                    if (state < expanded.size() && expanded[state]) {
                        queued[state] = 1;
                    } else {
                        enqueue(state);
                    }
                }
                full_closure_after_focused_fallback = false;
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
            if (focused_upper_mode) {
                finish_focused_upper_solve(true);
            } else {
                finish_focused_lower_solve();
            }
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
                    queue.size() >
                        options.focused_expansion_queue_threshold &&
                    expanded_count < options.max_expanded_states) {
                    begin_focused_lower_solve();
                    continue;
                }
                if (result.diagnostics.resource_cap_hit) {
                    prepare_iteration();
                    if (result.diagnostics.resource_cap_hit) {
                        phase = SolvePhase::Done;
                    }
                    break;
                }
                if (completed_state &&
                    expanded_count >= options.max_expanded_states) {
                    /* Measure the final exact partial graph before reporting
                     * its cap. Otherwise a focused batch that lands exactly
                     * on the limit exposes bounds from the preceding round and
                     * discards every state in the last batch. */
                    if (focused_mode && !focused_closure_proved) {
                        begin_focused_lower_solve();
                        continue;
                    }
                    prepare_iteration();
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
            if (!result.behavioral_representative_by_state.empty() &&
                result.behavioral_representative_by_state[state] != state) {
                continue;
            }
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
        snapshot.diagnostics.automatic_admission_phases =
            transition_cache->automatic_admission_phases;
        snapshot.diagnostics.automatic_candidate_witnesses_omitted =
            snapshot.diagnostics.automatic_rows_considered;
        snapshot.diagnostics.solve_owned_byte_ledger_requests =
            owned_byte_ledger_requests;
        snapshot.diagnostics.solve_owned_byte_reconciliations =
            owned_byte_reconciliations;
        snapshot.diagnostics.solve_owned_byte_ledger_max_overestimate =
            owned_byte_ledger_max_overestimate;
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
        const bool authoritative_policy_available =
            !result.diagnostics.state_cap_hit &&
            !result.diagnostics.resource_cap_hit &&
            !focused_bound_proved;
        for (std::uint32_t state = 0;
             authoritative_policy_available && state < state_count; ++state) {
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
                    auto& preferences = result.unveil_preferences[state];
                    const std::size_t old_capacity = preferences.capacity();
                    preferences.push_back(option.mod_id);
                    owned_result_nested_bytes +=
                        (preferences.capacity() - old_capacity) *
                        sizeof(std::uint32_t);
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
                    auto& preferences =
                        result.option_unveil_preferences[state];
                    const std::size_t old_capacity = preferences.capacity();
                    owned_result_nested_bytes +=
                        preference.choices.capacity() *
                        sizeof(ObservedUnveilChoice);
                    preferences.push_back(std::move(preference));
                    owned_result_nested_bytes +=
                        (preferences.capacity() - old_capacity) *
                        sizeof(ObservedUnveilPreference);
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
                if (operator_index == restart_operator_index &&
                    restart_state != kNoId) {
                    if (!result.policy_reachable[restart_state]) {
                        walk.push_back(restart_state);
                    }
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
            if (!result.behavioral_representative_by_state.empty() &&
                result.behavioral_representative_by_state[state] != state) {
                continue;
            }
            if (!result.expanded[state] && !result.goal_states[state]) {
                full_non_goal_closure = false;
                break;
            }
        }
        if (result.diagnostics.focused_expansion &&
            result.start_state < result.values.size()) {
            if (focused_bound_proved ||
                full_closure_after_focused_fallback ||
                full_non_goal_closure ||
                !std::isfinite(result.diagnostics.focused_upper_bound)) {
                result.diagnostics.focused_upper_bound =
                    result.values[result.start_state];
            }
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
        const bool final_optimization_converged = optimization_converged();
        result.converged = focused_exact &&
                           !result.diagnostics.state_cap_hit &&
                           !result.diagnostics.resource_cap_hit &&
                           final_optimization_converged &&
                           reachable_policy_complete &&
                           result.start_state < state_count &&
                           result.values[result.start_state] < kValueCeiling;
        if (!result.converged &&
            result.diagnostics.policy_evaluation_failure.empty()) {
            result.diagnostics.policy_evaluation_failure =
                "final_convergence_gate:focused_exact=" +
                std::to_string(focused_exact) +
                ",optimization=" +
                std::to_string(final_optimization_converged) +
                ",reachable_policy=" +
                std::to_string(reachable_policy_complete) +
                ",state_in_range=" +
                std::to_string(result.start_state < state_count) +
                ",finite_start=" + std::to_string(
                    result.start_state < state_count &&
                    result.values[result.start_state] < kValueCeiling);
        }
        if (!result.behavioral_representative_by_state.empty()) {
            for (std::uint32_t state = 0;
                 state < result.behavioral_representative_by_state.size();
                 ++state) {
                const std::uint32_t representative =
                    result.behavioral_representative_by_state[state];
                if (representative == state) continue;
                result.values[state] = result.values[representative];
                result.policy[state] = result.policy[representative];
                result.unveil_preferences[state] =
                    result.unveil_preferences[representative];
                result.option_unveil_preferences[state] =
                    result.option_unveil_preferences[representative];
            }
        }
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
                mix(std::bit_cast<std::uint64_t>(
                    row.embedded_self_probability));
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
        result.diagnostics.solve_owned_byte_ledger_requests =
            owned_byte_ledger_requests;
        result.diagnostics.solve_owned_byte_reconciliations =
            owned_byte_reconciliations;
        result.diagnostics.solve_owned_byte_ledger_max_overestimate =
            owned_byte_ledger_max_overestimate;
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
        const std::uint64_t calc_bytes =
            calc.audited_estimated_owned_bytes();
        const std::uint64_t audited =
            estimated_owned_bytes_with_calc(calc_bytes);
        const std::uint64_t fast =
            fast_estimated_owned_bytes_with_calc(calc.fast_estimated_owned_bytes());
        ++owned_byte_reconciliations;
        if (fast < audited) {
            throw std::logic_error(
                "incremental SolveWork selected-owned-byte ledger "
                "undercounted by " + std::to_string(audited - fast) +
                " bytes (audited=" + std::to_string(audited) +
                ", ledger=" + std::to_string(fast) +
                ", calc=" + std::to_string(calc_bytes) +
                ", transition_cache_audited=" +
                std::to_string(
                    transition_cache == nullptr
                        ? 0
                        : transition_cache->audited_estimated_owned_bytes()) +
                ", transition_cache_ledger=" +
                std::to_string(
                    transition_cache == nullptr
                        ? 0
                        : transition_cache->fast_estimated_owned_bytes()) +
                ", operator_nested=" +
                std::to_string(owned_operators_nested_bytes) +
                ", kernel_bucket_nested=" +
                std::to_string(owned_kernel_row_bucket_bytes) +
                ", result_nested=" +
                std::to_string(owned_result_nested_bytes) + ")");
        }
        owned_byte_ledger_max_overestimate = std::max(
            owned_byte_ledger_max_overestimate, fast - audited);
        return audited;
    }

    std::uint64_t fast_estimated_owned_bytes() const {
        ++owned_byte_ledger_requests;
        return fast_estimated_owned_bytes_with_calc(
            calc.fast_estimated_owned_bytes());
    }

    std::uint64_t fast_estimated_owned_bytes_with_calc(
        const std::uint64_t calc_bytes) const {
        std::uint64_t bytes = sizeof(*this) + calc_bytes;
        bytes += prices.bucket_count() * sizeof(void*);
        bytes += prices.size() *
                 (sizeof(std::pair<const std::string, double>) +
                  2 * sizeof(void*));
        bytes += owned_prices_nested_bytes;
        bytes += operators.capacity() * sizeof(PricedOperator);
        bytes += owned_operators_nested_bytes;
        bytes += (reported_unsupported.capacity() + 7) / 8;
        bytes += static_operator_indices.capacity() * sizeof(std::uint32_t);
        bytes += expansion_operator_indices.capacity() *
                 sizeof(std::uint32_t);
        bytes += expanded.capacity() * sizeof(std::uint8_t);
        bytes += queued.capacity() * sizeof(std::uint8_t);
        bytes += static_cast<std::uint64_t>(peak_queue_size) *
                 sizeof(std::uint32_t);
        if (transition_cache != nullptr) {
            bytes += transition_cache->fast_estimated_owned_bytes();
        }
        if (focused_strict_transition_cache != nullptr &&
            focused_strict_transition_cache != transition_cache) {
            bytes +=
                focused_strict_transition_cache->fast_estimated_owned_bytes();
        }
        bytes += focused_strict_expanded.capacity() * sizeof(std::uint8_t);
        bytes += focused_behavioral_representative.capacity() *
                 sizeof(std::uint32_t);
        bytes += focused_previous_upper_values.capacity() * sizeof(double);
        bytes += focused_previous_upper_policy_rows.capacity() *
                 sizeof(std::uint64_t);
        bytes += focused_frontier_upper_operator.capacity() *
                 sizeof(std::uint32_t);
        bytes += focused_previous_frontier_upper_operator.capacity() *
                 sizeof(std::uint32_t);
        bytes += focused_round_lower_values.capacity() * sizeof(double);
        bytes += focused_round_lower_policy_rows.capacity() *
                 sizeof(std::uint64_t);
        bytes += focused_pending_lower_fringe.capacity() *
                 sizeof(std::uint32_t);
        bytes += focused_pending_upper_fringe.capacity() *
                 sizeof(std::uint32_t);
        bytes += focused_pending_upper_priority.capacity() * sizeof(double);
        bytes += operator_goal_reach_mask.capacity() * sizeof(std::uint32_t);
        bytes += operator_goal_reach_computed.capacity() *
                 sizeof(std::uint8_t);
        bytes += goal_cover_cost.capacity() * sizeof(double);
        bytes += clean_goal_cover_cost.capacity() * sizeof(double);
        bytes += clean_goal_escape_cost.capacity() * sizeof(double);
        bytes += clean_goal_escape_action.capacity() *
                 sizeof(std::uint32_t);
        bytes += clean_goal_no_exalt_escape_cost.capacity() *
                 sizeof(double);
        bytes += clean_goal_no_exalt_escape_action.capacity() *
                 sizeof(std::uint32_t);
        bytes += strict_clean_goal_cover_cost.capacity() * sizeof(double);
        if (focused_fallback_policy.has_value()) {
            const FocusedFallbackPolicy& fallback =
                *focused_fallback_policy;
            bytes += fallback.progress_state_value.bucket_count() *
                sizeof(void*);
            bytes += fallback.progress_state_value.size() *
                (sizeof(std::pair<const std::uint32_t, double>) +
                 2 * sizeof(void*));
            bytes += fallback.progress_state_operator.bucket_count() *
                sizeof(void*);
            bytes += fallback.progress_state_operator.size() *
                (sizeof(std::pair<const std::uint32_t, std::uint32_t>) +
                 2 * sizeof(void*));
        }
        bytes += certified_state_upper.capacity() * sizeof(double);
        bytes += certified_state_row.capacity() * sizeof(std::uint64_t);
        bytes += priced_rows.capacity() * sizeof(PricedSparseRow);
        bytes += priced_operator_position.capacity() * sizeof(std::int32_t);
        bytes += prioritized_states.capacity() *
                 sizeof(std::pair<double, std::uint32_t>);
        bytes += policy_rows.capacity() * sizeof(std::uint64_t);
        bytes += policy_seed_states.capacity() * sizeof(std::uint32_t);
        bytes += policy_selection_states.capacity() * sizeof(std::uint32_t);
        bytes += current_policy_scratch_bytes;
        bytes += kernel_value_caches.capacity() * sizeof(KernelValueCache);
        bytes += kernel_value_cache_by_offset.bucket_count() * sizeof(void*);
        bytes += kernel_value_cache_by_offset.size() *
                 (sizeof(std::pair<const std::uint64_t, std::size_t>) +
                  2 * sizeof(void*));
        bytes += owned_kernel_value_cache_nested_bytes;
        bytes += kernel_rows_by_hash.bucket_count() * sizeof(void*);
        bytes += kernel_rows_by_hash.size() *
                 (sizeof(std::pair<const std::size_t,
                                   std::vector<KernelRowMemo>>) +
                  2 * sizeof(void*));
        bytes += owned_kernel_row_bucket_bytes;
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
        bytes += result.option_unveil_preferences.capacity() *
                 sizeof(std::vector<ObservedUnveilPreference>);
        bytes += result.behavioral_representative_by_state.capacity() *
                 sizeof(std::uint32_t);
        bytes += owned_result_nested_bytes;
        /* Diagnostic samples are strictly bounded and are not graph-sized.
         * Keep their exact current allocation in both ledger paths. */
        bytes += diagnostics_owned_bytes(result.diagnostics);
        return bytes;
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
        if (focused_strict_transition_cache != nullptr &&
            focused_strict_transition_cache != transition_cache) {
            bytes += focused_strict_transition_cache->estimated_owned_bytes();
        }
        bytes += focused_strict_expanded.capacity() * sizeof(std::uint8_t);
        bytes += focused_behavioral_representative.capacity() *
                 sizeof(std::uint32_t);
        bytes += focused_previous_upper_values.capacity() * sizeof(double);
        bytes += focused_previous_upper_policy_rows.capacity() *
                 sizeof(std::uint64_t);
        bytes += focused_frontier_upper_operator.capacity() *
                 sizeof(std::uint32_t);
        bytes += focused_previous_frontier_upper_operator.capacity() *
                 sizeof(std::uint32_t);
        bytes += focused_round_lower_values.capacity() * sizeof(double);
        bytes += focused_round_lower_policy_rows.capacity() *
                 sizeof(std::uint64_t);
        bytes += focused_pending_lower_fringe.capacity() *
                 sizeof(std::uint32_t);
        bytes += focused_pending_upper_fringe.capacity() *
                 sizeof(std::uint32_t);
        bytes += focused_pending_upper_priority.capacity() * sizeof(double);
        bytes += operator_goal_reach_mask.capacity() * sizeof(std::uint32_t);
        bytes += operator_goal_reach_computed.capacity() *
                 sizeof(std::uint8_t);
        bytes += goal_cover_cost.capacity() * sizeof(double);
        bytes += clean_goal_cover_cost.capacity() * sizeof(double);
        bytes += clean_goal_escape_cost.capacity() * sizeof(double);
        bytes += clean_goal_escape_action.capacity() *
                 sizeof(std::uint32_t);
        bytes += clean_goal_no_exalt_escape_cost.capacity() *
                 sizeof(double);
        bytes += clean_goal_no_exalt_escape_action.capacity() *
                 sizeof(std::uint32_t);
        bytes += strict_clean_goal_cover_cost.capacity() * sizeof(double);
        if (focused_fallback_policy.has_value()) {
            const FocusedFallbackPolicy& fallback =
                *focused_fallback_policy;
            bytes += fallback.progress_state_value.bucket_count() *
                sizeof(void*);
            bytes += fallback.progress_state_value.size() *
                (sizeof(std::pair<const std::uint32_t, double>) +
                 2 * sizeof(void*));
            bytes += fallback.progress_state_operator.bucket_count() *
                sizeof(void*);
            bytes += fallback.progress_state_operator.size() *
                (sizeof(std::pair<const std::uint32_t, std::uint32_t>) +
                 2 * sizeof(void*));
        }
        bytes += certified_state_upper.capacity() * sizeof(double);
        bytes += certified_state_row.capacity() * sizeof(std::uint64_t);
        bytes += priced_rows.capacity() * sizeof(PricedSparseRow);
        bytes += priced_operator_position.capacity() * sizeof(std::int32_t);
        bytes += prioritized_states.capacity() *
                 sizeof(std::pair<double, std::uint32_t>);
        bytes += policy_rows.capacity() * sizeof(std::uint64_t);
        bytes += policy_seed_states.capacity() * sizeof(std::uint32_t);
        bytes += policy_selection_states.capacity() * sizeof(std::uint32_t);
        bytes += current_policy_scratch_bytes;
        bytes += kernel_value_caches.capacity() * sizeof(KernelValueCache);
        bytes += kernel_value_cache_by_offset.bucket_count() * sizeof(void*);
        bytes += kernel_value_cache_by_offset.size() *
                 (sizeof(std::pair<const std::uint64_t, std::size_t>) +
                  2 * sizeof(void*));
        bytes += kernel_rows_by_hash.bucket_count() * sizeof(void*);
        bytes += kernel_rows_by_hash.size() *
                 (sizeof(std::pair<const std::size_t,
                                   std::vector<KernelRowMemo>>) +
                  2 * sizeof(void*));
        for (const auto& [unused, rows] : kernel_rows_by_hash) {
            (void)unused;
            bytes += rows.capacity() * sizeof(KernelRowMemo);
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
        bytes += result.behavioral_representative_by_state.capacity() *
                 sizeof(std::uint32_t);
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

void append_telemetry_json_string(
    BoundedTelemetryJson& out, const std::string_view value) {
    static constexpr char kHex[] = "0123456789abcdef";
    out += '"';
    for (const unsigned char c : value) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    out += "\\u00";
                    out += kHex[c >> 4];
                    out += kHex[c & 0x0f];
                } else {
                    out += static_cast<char>(c);
                }
                break;
        }
    }
    out += '"';
}

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
    json += ",\"dominance_filter\":\"certified_kernel_equivalence_preservation_or_constructive_goal_bound\"";
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
                    diagnostics->preservation_rows_pruned +
                    diagnostics->constructive_state_operators_pruned);
        json += ",\"deferred\":" + std::to_string(
                    diagnostics->deferred_actions);
        json += ",\"equivalent_price_ties\":" + std::to_string(
                    diagnostics->equivalent_price_ties);
        json += ",\"missing_price\":" + std::to_string(
                    diagnostics->skipped_missing_price_count);
        json += ",\"unsupported_observed\":" + std::to_string(
                    diagnostics->skipped_unsupported_count);
        json += ",\"missing_price_samples\":[";
        for (std::size_t i = 0;
             i < diagnostics->skipped_missing_price.size(); ++i) {
            if (i != 0) json += ',';
            append_telemetry_json_string(
                json, diagnostics->skipped_missing_price[i]);
        }
        json += "]";
        json += ",\"unsupported_samples\":[";
        for (std::size_t i = 0;
             i < diagnostics->skipped_unsupported.size(); ++i) {
            if (i != 0) json += ',';
            append_telemetry_json_string(
                json, diagnostics->skipped_unsupported[i]);
        }
        json += "]";
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
        json += ",\"constructive_state_certificates\":null";
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
        json += ",\"constructive_state_certificates\":{";
        json += "\"accepted\":" + std::to_string(
            diagnostics->constructive_state_certificates);
        json += ",\"operators_pruned\":" + std::to_string(
            diagnostics->constructive_state_operators_pruned);
        json += ",\"start_upper_bound\":";
        if (std::isfinite(diagnostics->constructive_upper_bound)) {
            json += std::to_string(
                diagnostics->constructive_upper_bound);
        } else {
            json += "null";
        }
        json += ",\"first_expanded_state\":";
        json += diagnostics->constructive_upper_first_expanded_state == 0
                    ? "null"
                    : std::to_string(
                          diagnostics
                              ->constructive_upper_first_expanded_state);
        json += "}";
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
    json += "},\"constructive_state_witnesses\":[";
    if (diagnostics != nullptr) {
        for (std::size_t i = 0;
             i < diagnostics->constructive_state_witnesses.size(); ++i) {
            if (i != 0) json.push_back(',');
            json += diagnostics->constructive_state_witnesses[i];
        }
    }
    json += "],\"constructive_state_witness_samples\":{";
    json += "\"retained\":";
    json += diagnostics == nullptr
                ? "null"
                : std::to_string(
                      diagnostics->constructive_state_witnesses.size());
    json += ",\"omitted\":";
    json += diagnostics == nullptr
                ? "null"
                : std::to_string(
                      diagnostics->constructive_state_witnesses_omitted);
    json += "},\"automatic_candidates\":{";
    json += "\"enabled\":" + std::string(bool_json(
        calc.goal().automatic_candidates));
    json += ",\"operators\":" + std::to_string(
        calc.action_control().automatic_options);
    json += ",\"dependency_primitives\":" + std::to_string(
        calc.action_control().automatic_dependency_primitives);
    if (diagnostics == nullptr) {
        json += ",\"rows\":null,\"admission_phases\":null";
        json += ",\"by_kind\":null,\"witnesses\":[]";
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
        const AutomaticAdmissionPhaseTelemetry& phases =
            diagnostics->automatic_admission_phases;
        json += ",\"admission_phases\":{\"carriers\":" +
                std::to_string(phases.carriers);
        json += ",\"synthesis_ns\":" +
                std::to_string(phases.synthesis_ns);
        json += ",\"local_context_ns\":" +
                std::to_string(phases.local_context_ns);
        json += ",\"local_planner_build_ns\":" +
                std::to_string(phases.local_planner_build_ns);
        json += ",\"local_layout_build_ns\":" +
                std::to_string(phases.local_layout_build_ns);
        json += ",\"local_ledger_init_ns\":" +
                std::to_string(phases.local_ledger_init_ns);
        json += ",\"local_context_other_ns\":" +
                std::to_string(phases.local_context_other_ns) + "}";
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
            json += ",\"eligible\":" +
                    std::to_string(values.eligible_candidates);
            json += ",\"rejected\":" +
                    std::to_string(values.rejected_candidates);
            json += ",\"collapsed\":" +
                    std::to_string(values.collapsed_candidates);
            json += ",\"deferred\":" +
                    std::to_string(values.deferred_candidates);
            json += ",\"missing_price\":" +
                    std::to_string(values.missing_price_candidates);
            json += ",\"rejection_reasons\":{\"setup\":" +
                    std::to_string(values.setup_rejections);
            json += ",\"neutral_kernel\":" +
                    std::to_string(values.neutral_kernel_rejections);
            json += ",\"relevance\":" +
                    std::to_string(values.relevance_rejections);
            json += ",\"cleanup\":" +
                    std::to_string(values.cleanup_rejections);
            json += ",\"other\":" +
                    std::to_string(values.other_rejections) + "}";
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
            json += ",\"kernel_evaluation_ns\":" +
                    std::to_string(values.kernel_evaluation_ns);
            json += ",\"outcome_mapping_ns\":" +
                    std::to_string(values.outcome_mapping_ns);
            json += ",\"template_matching_ns\":" +
                    std::to_string(values.template_matching_ns);
            json += ",\"protected_detail\":{\"side_evaluations\":" +
                    std::to_string(values.protected_side_evaluations);
            json += ",\"repeat_evaluations\":" +
                    std::to_string(values.protected_repeat_evaluations);
            json += ",\"retry_checks\":" +
                    std::to_string(values.protected_retry_checks);
            json += ",\"retry_certificates\":" +
                    std::to_string(values.protected_retry_certificates);
            json += ",\"retry_fallbacks\":" +
                    std::to_string(values.protected_retry_fallbacks);
            json += ",\"attempt_ns\":" +
                    std::to_string(values.protected_attempt_ns);
            json += ",\"baseline_ns\":" +
                    std::to_string(values.protected_baseline_ns);
            json += ",\"normalization_ns\":" +
                    std::to_string(values.protected_normalization_ns);
            json += ",\"finish_ns\":" +
                    std::to_string(values.protected_finish_ns) + "}";
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

    json += ",\"exact_state_scaling\":{";
    if (diagnostics == nullptr) {
        json += "\"strict_states\":null,\"quotient_states\":null";
        json += ",\"refinement_rounds\":null";
        json += ",\"coarse_candidate_classes\":null";
        json += ",\"max_strict_states_per_coarse_class\":null";
        json += ",\"shadow_only\":null";
        json += ",\"shadow_behavioral_classes\":null";
        json += ",\"shadow_expanded_states_observed\":null";
        json += ",\"literal_duplicates\":null";
        json += ",\"exact_behavioral_merges\":null";
        json += ",\"witnessed_non_equivalences\":null";
        json += ",\"projected_successor_class_mismatches\":null";
        json += ",\"kernel_payload_reuses\":null";
        json += ",\"kernel_payload_bytes_saved\":null";
        json += ",\"observation_signature_mismatches\":null";
        json += ",\"action_observation_cardinality\":[]";
        json += ",\"witnesses\":[]";
    } else {
        json += "\"strict_states\":" +
                std::to_string(diagnostics->strict_discovered_states);
        json += ",\"quotient_states\":" +
                std::to_string(diagnostics->quotient_states);
        json += ",\"refinement_rounds\":" +
                std::to_string(diagnostics->quotient_refinement_rounds);
        json += ",\"coarse_candidate_classes\":" +
                std::to_string(diagnostics->coarse_candidate_classes);
        json += ",\"max_strict_states_per_coarse_class\":" +
                std::to_string(
                    diagnostics->max_strict_states_per_coarse_class);
        json += ",\"shadow_only\":" + std::string(
                    bool_json(diagnostics->state_scaling_shadow_only));
        json += ",\"shadow_behavioral_classes\":" +
                std::to_string(diagnostics->shadow_behavioral_classes);
        json += ",\"shadow_expanded_states_observed\":" +
                std::to_string(
                    diagnostics->shadow_expanded_states_observed);
        json += ",\"literal_duplicates\":" +
                std::to_string(diagnostics->literal_duplicate_states);
        json += ",\"exact_behavioral_merges\":" +
                std::to_string(diagnostics->exact_behavioral_merges);
        json += ",\"witnessed_non_equivalences\":" +
                std::to_string(diagnostics->witnessed_non_equivalences);
        json += ",\"projected_successor_class_mismatches\":" +
                std::to_string(
                    diagnostics->projected_successor_class_mismatches);
        json += ",\"kernel_payload_reuses\":" +
                std::to_string(diagnostics->exact_kernel_payload_reuses);
        json += ",\"kernel_payload_bytes_saved\":" +
                std::to_string(
                    diagnostics->exact_kernel_payload_bytes_saved);
        json += ",\"observation_signature_mismatches\":" +
                std::to_string(
                    diagnostics->observation_signature_mismatches);
        json += ",\"action_observation_cardinality\":[";
        for (std::size_t i = 0;
             i < diagnostics->action_observation_cardinalities.size(); ++i) {
            if (i != 0) json.push_back(',');
            json += diagnostics->action_observation_cardinalities[i];
        }
        json += "]";
        json += ",\"witnesses\":[";
        for (std::size_t i = 0;
             i < diagnostics->equivalence_witnesses.size(); ++i) {
            if (i != 0) json.push_back(',');
            json += diagnostics->equivalence_witnesses[i];
        }
        json += "],\"witness_samples\":{\"retained\":" +
                std::to_string(diagnostics->equivalence_witnesses.size()) +
                ",\"omitted\":" +
                std::to_string(
                    diagnostics->equivalence_witnesses_omitted) +
                ",\"limit\":" +
                std::to_string(diagnostics->diagnostic_sample_limit) + "}";
    }
    json += "}";

    json += ",\"states\":{";
    if (diagnostics == nullptr) {
        json += "\"discovered\":null,\"expanded\":null,\"frontier\":null";
        json += ",\"goal\":null,\"policy_reachable\":null";
    } else {
        json += "\"discovered\":" +
                std::to_string(diagnostics->discovered_states);
        json += ",\"strict_discovered\":" +
                std::to_string(diagnostics->strict_discovered_states);
        json += ",\"quotient\":" +
                std::to_string(diagnostics->quotient_states);
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
        json += ",\"upper_bound\":null,\"partial_policy_upper_bound\":null";
        json += ",\"partial_policy_rounds\":null,\"optimality_gap\":null";
        json += ",\"duration_ns\":null,\"constructive_policy\":null";
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
        json += ",\"partial_policy_upper_bound\":";
        append_bound(
            diagnostics->focused_partial_policy_upper_bound);
        json += ",\"partial_policy_rounds\":" + std::to_string(
            diagnostics->focused_partial_policy_rounds);
        json += ",\"optimality_gap\":";
        append_bound(diagnostics->focused_optimality_gap);
        json += ",\"duration_ns\":" + std::to_string(
            diagnostics->focused_expansion_ns);
        json += ",\"constructive_policy\":{\"anchor_checks\":" +
                std::to_string(
                    diagnostics->constructive_policy_anchor_checks);
        json += ",\"anchor_eligible\":" + std::to_string(
            diagnostics->constructive_policy_anchor_eligible);
        json += ",\"renewal_variants\":" + std::to_string(
            diagnostics->constructive_policy_renewal_variants);
        json += ",\"exit_checks\":" + std::to_string(
            diagnostics->constructive_policy_exit_checks);
        json += ",\"finishable_exits\":" + std::to_string(
            diagnostics->constructive_policy_finishable_exits);
        json += ",\"feasible_policies\":" + std::to_string(
            diagnostics->constructive_policy_feasible_policies) + "}";
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
                cache.owned_byte_ledger_max_overestimate);
    json += ",\"solve_owned_byte_ledger_requests\":" +
            std::to_string(
                diagnostics == nullptr
                    ? 0
                    : diagnostics->solve_owned_byte_ledger_requests);
    json += ",\"solve_owned_byte_reconciliations\":" +
            std::to_string(
                diagnostics == nullptr
                    ? 0
                    : diagnostics->solve_owned_byte_reconciliations);
    json += ",\"solve_owned_byte_ledger_max_overestimate\":" +
            std::to_string(
                diagnostics == nullptr
                    ? 0
                    : diagnostics
                          ->solve_owned_byte_ledger_max_overestimate) + "}";

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
