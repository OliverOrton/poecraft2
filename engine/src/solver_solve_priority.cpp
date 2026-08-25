#include "solver_solve_types.hpp"

namespace poecraft {
namespace solver {
namespace solve_detail {

CarrierOrderingScore make_carrier_ordering_score(
        const AbstractState& carrier,
        const std::uint32_t state,
        const std::uint32_t goal_subset,
        const std::uint32_t prefix_goal_mask,
        const std::uint32_t suffix_goal_mask,
        const double focused_priority) {
    const std::uint32_t satisfied = std::popcount(goal_subset);
    const std::uint32_t missing_prefixes = std::popcount(
        prefix_goal_mask & ~goal_subset);
    const std::uint32_t missing_suffixes = std::popcount(
        suffix_goal_mask & ~goal_subset);
    const std::uint32_t free_prefixes =
        carrier.prefix_count < 3 ? 3 - carrier.prefix_count : 0;
    const std::uint32_t free_suffixes =
        carrier.suffix_count < 3 ? 3 - carrier.suffix_count : 0;
    const bool useful_prefix_lock =
        (carrier.flags & kFlagPrefixesLocked) != 0 &&
        (goal_subset & prefix_goal_mask) != 0 && missing_suffixes != 0;
    const bool useful_suffix_lock =
        (carrier.flags & kFlagSuffixesLocked) != 0 &&
        (goal_subset & suffix_goal_mask) != 0 && missing_prefixes != 0;
    const std::uint32_t explicit_affixes =
        carrier.prefix_count + carrier.suffix_count;
    return {
        state,
        abstract_state_hash(carrier),
        goal_subset,
        satisfied,
        static_cast<std::uint32_t>(
            std::popcount(carrier.fractured_goal_mask)),
        static_cast<std::uint32_t>(std::popcount(
            carrier.flags &
            (kFlagPrefixesLocked | kFlagSuffixesLocked))),
        static_cast<std::uint32_t>(useful_prefix_lock) +
            static_cast<std::uint32_t>(useful_suffix_lock),
        (missing_prefixes > free_prefixes ? 1u : 0u) +
            (missing_suffixes > free_suffixes ? 1u : 0u),
        static_cast<std::uint32_t>(
            std::popcount(carrier.blocked_mask & ~goal_subset)),
        explicit_affixes > satisfied
            ? explicit_affixes - satisfied
            : 0,
        focused_priority,
    };
}

CarrierPriorityBuckets build_carrier_priority_buckets(
        const std::vector<CarrierOrderingScore>& candidates,
        const CarrierOrderingMode mode) {
    CarrierPriorityBuckets result;
    std::map<std::uint32_t, CarrierOrderingScore> first_score;
    std::uint32_t maximum_state = 0;
    for (const CarrierOrderingScore& candidate : candidates) {
        maximum_state = std::max(maximum_state, candidate.state);
    }
    std::vector<const CarrierOrderingScore*> score_by_state(
        candidates.empty() ? 0 : static_cast<std::size_t>(maximum_state) + 1,
        nullptr);
    for (const CarrierOrderingScore& candidate : candidates) {
        score_by_state.at(candidate.state) = &candidate;
        auto& bucket = result.by_goal_subset[candidate.goal_subset];
        if (bucket.empty()) first_score.emplace(
            candidate.goal_subset, candidate);
        bucket.push_back(candidate.state);
    }
    result.subset_order.reserve(result.by_goal_subset.size());
    for (auto& [mask, carriers] : result.by_goal_subset) {
        if (mode != CarrierOrderingMode::FocusedLegacy) {
            std::stable_sort(
                carriers.begin(), carriers.end(),
                [&](const std::uint32_t left,
                    const std::uint32_t right) {
                    const auto score_for = [&](const std::uint32_t state)
                            -> const CarrierOrderingScore& {
                        if (state >= score_by_state.size() ||
                            score_by_state[state] == nullptr) {
                            throw std::logic_error(
                                "carrier priority lost a candidate score");
                        }
                        return *score_by_state[state];
                    };
                    const CarrierOrderingScore& a = score_for(left);
                    const CarrierOrderingScore& b = score_for(right);
                    if (mode == CarrierOrderingMode::CooperativeHighProgress) {
                        if (a.capacity_obstructions !=
                            b.capacity_obstructions) {
                            return a.capacity_obstructions <
                                   b.capacity_obstructions;
                        }
                        if (a.blocked_missing_goals !=
                            b.blocked_missing_goals) {
                            return a.blocked_missing_goals <
                                   b.blocked_missing_goals;
                        }
                    }
                    if (a.fractured_goals != b.fractured_goals) {
                        return a.fractured_goals > b.fractured_goals;
                    }
                    if (a.active_protection != b.active_protection) {
                        if (mode !=
                            CarrierOrderingMode::CooperativeHighProgress) {
                            return a.active_protection >
                                   b.active_protection;
                        }
                    }
                    if (mode == CarrierOrderingMode::CooperativeHighProgress &&
                        a.useful_protection != b.useful_protection) {
                        return a.useful_protection > b.useful_protection;
                    }
                    if (a.unrelated_occupancy != b.unrelated_occupancy) {
                        return a.unrelated_occupancy < b.unrelated_occupancy;
                    }
                    if (mode == CarrierOrderingMode::IncrementalLegacy) {
                        return a.state < b.state;
                    }
                    return a.stable_state_hash != b.stable_state_hash
                        ? a.stable_state_hash < b.stable_state_hash
                        : a.state < b.state;
                });
        }
        result.subset_order.push_back(mask);
    }
    std::stable_sort(
        result.subset_order.begin(), result.subset_order.end(),
        [&](const std::uint32_t left, const std::uint32_t right) {
            const CarrierOrderingScore& a = first_score.at(left);
            const CarrierOrderingScore& b = first_score.at(right);
            if (a.satisfied_goals != b.satisfied_goals) {
                return a.satisfied_goals > b.satisfied_goals;
            }
            if (mode != CarrierOrderingMode::IncrementalLegacy &&
                a.focused_priority != b.focused_priority) {
                return a.focused_priority > b.focused_priority;
            }
            return left < right;
        });
    return result;
}

bool carrier_action_ordering_precedes(
    const CarrierActionOrderingScore& left,
    const CarrierActionOrderingScore& right) {
    const auto left_key = std::tie(
        left.obstruction_removal,
        left.preserved_satisfied_goals,
        left.preserved_useful_protection,
        left.immediately_reachable_missing_goals,
        left.reachable_missing_goals);
    const auto right_key = std::tie(
        right.obstruction_removal,
        right.preserved_satisfied_goals,
        right.preserved_useful_protection,
        right.immediately_reachable_missing_goals,
        right.reachable_missing_goals);
    return left_key != right_key
        ? left_key > right_key
        : left.stable_operator_id != right.stable_operator_id
            ? left.stable_operator_id < right.stable_operator_id
            : left.operator_index < right.operator_index;
}

} // namespace solve_detail

using namespace solve_detail;

void SolveWork::Impl::prepare_ordering_goal_masks() {
    if (ordering_goal_masks_ready) return;
    ordering_goal_masks_ready = true;
    for (std::uint32_t slot = 0; slot < calc.layout().slots.size(); ++slot) {
        bool prefix = false;
        bool suffix = false;
        const std::vector<std::uint64_t>& satisfying =
            calc.layout().slots[slot].satisfying_mask;
        for (std::uint32_t mod = 0; mod < session.mod_count; ++mod) {
            if (satisfying.empty() ||
                !pc_bitset_test(satisfying.data(), mod)) {
                continue;
            }
            prefix |= session.gen_type[mod] == PC_SIDE_PREFIX;
            suffix |= session.gen_type[mod] == PC_SIDE_SUFFIX;
        }
        if (prefix && !suffix) ordering_prefix_goal_mask |= 1u << slot;
        if (suffix && !prefix) ordering_suffix_goal_mask |= 1u << slot;
    }
}

CarrierOrderingScore SolveWork::Impl::carrier_ordering_score(
        const std::uint32_t state,
        const double focused_priority) {
    prepare_ordering_goal_masks();
    return make_carrier_ordering_score(
        calc.state(state), state, satisfied_goal_mask_for_state(state),
        ordering_prefix_goal_mask, ordering_suffix_goal_mask,
        focused_priority);
}

bool SolveWork::Impl::cooperative_high_progress_ordering_enabled() const {
    /* Gate 3's matched corpus found no quota/profile that qualified both the
     * clean acquisition target and retained controls. Keep the scheduler as
     * typed neutral infrastructure; the legacy compatibility producer remains
     * product authority until a later executable planner can replace it. */
    return false;
}

CarrierEffectSummary SolveWork::Impl::carrier_ordering_effect(
        const std::uint32_t state,
        const std::uint32_t operator_index) {
    CarrierEffectSummary effect;
    if (state >= calc.state_count() ||
        operator_index >= calc.operators().size()) {
        return effect;
    }
    prepare_ordering_goal_masks();
    const AbstractState& source = calc.state(state);
    const PlannerOperator& planner = calc.operators()[operator_index];
    const std::uint32_t satisfied = satisfied_goal_mask_for_state(state);
    const std::uint32_t surviving =
        planner_goal_may_survive_mask(state, operator_index) & satisfied;
    const std::uint32_t reachable = planner_goal_reach_mask(operator_index);
    effect.preserved_satisfied_goal_mask = surviving;
    effect.destroyed_satisfied_goal_mask = satisfied & ~surviving;
    effect.created_satisfied_goal_mask = reachable & ~satisfied;
    effect.min_prefix_count = source.prefix_count;
    effect.max_prefix_count = source.prefix_count;
    effect.min_suffix_count = source.suffix_count;
    effect.max_suffix_count = source.suffix_count;
    effect.preserved_protection = source.flags & kProtectionFlags;

    std::vector<std::uint32_t> actions = planner.primitive_program;
    if (actions.empty() &&
        planner.primitive_action < calc.registry().actions.size()) {
        actions.push_back(planner.primitive_action);
    }
    const auto selector_can_match_side = [](
        const RefinementAffixSelector& selector,
        const std::uint16_t side,
        const std::uint16_t opposite) {
        return (selector.required_affix_traits & opposite) == 0 &&
               (selector.forbidden_affix_traits & side) == 0;
    };
    for (const std::uint32_t action_index : actions) {
        if (action_index >= calc.registry().actions.size()) continue;
        const ActionDescriptor& action = calc.registry().actions[action_index];
        const ActionRefinementContract& contract = action.refinement;
        if (contract.resets_to_fresh_item) {
            effect.min_prefix_count = 0;
            effect.max_prefix_count = 0;
            effect.min_suffix_count = 0;
            effect.max_suffix_count = 0;
            effect.destroyed_properties |= kCarrierJunkBlockers;
            effect.destroyed_protection |= effect.preserved_protection;
            effect.preserved_protection = 0;
        }
        for (const RefinementAffixSelector& selector :
             contract.destroyed_affixes) {
            if (selector_can_match_side(
                    selector, kRefinementAffixPrefix,
                    kRefinementAffixSuffix)) {
                effect.min_prefix_count = 0;
                effect.destroyed_properties |= kCarrierJunkBlockers;
            }
            if (selector_can_match_side(
                    selector, kRefinementAffixSuffix,
                    kRefinementAffixPrefix)) {
                effect.min_suffix_count = 0;
                effect.destroyed_properties |= kCarrierJunkBlockers;
            }
        }
        const std::uint32_t cleared = action.clears_flags & kProtectionFlags;
        effect.destroyed_protection |=
            effect.preserved_protection & cleared;
        effect.preserved_protection &= ~cleared;
    }
    if ((reachable & ordering_prefix_goal_mask) != 0) {
        effect.max_prefix_count = 3;
    }
    if ((reachable & ordering_suffix_goal_mask) != 0) {
        effect.max_suffix_count = 3;
    }
    return effect;
}

CarrierActionOrderingScore SolveWork::Impl::carrier_action_ordering_score(
        const std::uint32_t state,
        const std::uint32_t operator_index) {
    prepare_ordering_goal_masks();
    const AbstractState& carrier = calc.state(state);
    const PlannerOperator& planner = calc.operators().at(operator_index);
    const CarrierOrderingScore carrier_score = carrier_ordering_score(state);
    const CarrierEffectSummary effect =
        carrier_ordering_effect(state, operator_index);
    const std::uint32_t satisfied = carrier_score.goal_subset;
    const std::uint32_t missing =
        (ordering_prefix_goal_mask | ordering_suffix_goal_mask) & ~satisfied;
    const std::uint32_t reachable =
        effect.created_satisfied_goal_mask & missing;
    const std::uint32_t free_prefixes =
        carrier.prefix_count < 3 ? 3 - carrier.prefix_count : 0;
    const std::uint32_t free_suffixes =
        carrier.suffix_count < 3 ? 3 - carrier.suffix_count : 0;
    const bool removes_prefix =
        effect.min_prefix_count < carrier.prefix_count;
    const bool removes_suffix =
        effect.min_suffix_count < carrier.suffix_count;
    const std::uint32_t immediately_reachable =
        static_cast<std::uint32_t>(std::popcount(
            reachable & ordering_prefix_goal_mask)) *
            static_cast<std::uint32_t>(free_prefixes != 0 || removes_prefix) +
        static_cast<std::uint32_t>(std::popcount(
            reachable & ordering_suffix_goal_mask)) *
            static_cast<std::uint32_t>(free_suffixes != 0 || removes_suffix);
    std::uint32_t obstruction_removal = 0;
    if (carrier_score.capacity_obstructions != 0) {
        const std::uint32_t missing_prefixes = std::popcount(
            missing & ordering_prefix_goal_mask);
        const std::uint32_t missing_suffixes = std::popcount(
            missing & ordering_suffix_goal_mask);
        obstruction_removal += static_cast<std::uint32_t>(
            missing_prefixes > free_prefixes && removes_prefix);
        obstruction_removal += static_cast<std::uint32_t>(
            missing_suffixes > free_suffixes && removes_suffix);
    }
    if (carrier_score.blocked_missing_goals != 0 &&
        (effect.destroyed_properties & kCarrierJunkBlockers) != 0) {
        obstruction_removal += carrier_score.blocked_missing_goals;
    }
    const std::uint32_t useful_protection =
        ((carrier.flags & kFlagPrefixesLocked) != 0 &&
         (satisfied & ordering_prefix_goal_mask) != 0 &&
         (missing & ordering_suffix_goal_mask) != 0
             ? kFlagPrefixesLocked
             : 0) |
        ((carrier.flags & kFlagSuffixesLocked) != 0 &&
         (satisfied & ordering_suffix_goal_mask) != 0 &&
         (missing & ordering_prefix_goal_mask) != 0
             ? kFlagSuffixesLocked
             : 0);
    return {
        operator_index,
        planner.id,
        immediately_reachable,
        obstruction_removal,
        static_cast<std::uint32_t>(std::popcount(
            effect.preserved_satisfied_goal_mask)),
        static_cast<std::uint32_t>(std::popcount(
            effect.preserved_protection & useful_protection)),
        static_cast<std::uint32_t>(std::popcount(reachable)),
    };
}

void SolveWork::Impl::prioritize_carrier_actions(
        const std::uint32_t state,
        std::vector<std::uint32_t>& operator_indices) {
    if (operator_indices.size() < 2) return;
    std::map<std::uint32_t, CarrierActionOrderingScore> score;
    for (const std::uint32_t operator_index : operator_indices) {
        score.emplace(
            operator_index,
            carrier_action_ordering_score(state, operator_index));
    }
    std::stable_sort(
        operator_indices.begin(), operator_indices.end(),
        [&](const std::uint32_t left, const std::uint32_t right) {
            return carrier_action_ordering_precedes(
                score.at(left), score.at(right));
        });
}

} // namespace solver
} // namespace poecraft
