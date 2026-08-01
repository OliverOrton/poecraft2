#include "solver_eval_helpers.hpp"

namespace poecraft {
namespace solver {

namespace {

std::uint32_t resolve_registry_strategy_action(
        const StrategyNode& node,
        const ActionRegistry& registry) {
    if (node.kind != StrategyNodeKind::Operation) return kNoId;
    if (node.action_type == kStrategyBestiaryImprintOperation ||
        node.action_type == kStrategyBestiaryRestoreImprintOperation) {
        return kNoId;
    }
    if (node.action_type != kStrategyRestartOperation &&
        (node.action_type < static_cast<int>(ActionType::Transmute) ||
         node.action_type >
             static_cast<int>(ActionType::RemoveCraftedModifiers) ||
         node.action_type != static_cast<int>(node.action.type))) {
        return kNoId;
    }
    for (std::uint32_t i = 0; i < registry.actions.size(); ++i) {
        const ActionDescriptor& descriptor = registry.actions[i];
        if (node.action_type == kStrategyRestartOperation) {
            if (descriptor.synthetic) return i;
            continue;
        }
        if (descriptor.synthetic) continue;
        if (same_action_parameters(node.action, descriptor)) return i;
    }
    return kNoId;
}

} // namespace

ResolvedStrategyOperation resolve_strategy_operation(
        const StrategyNode& node,
        const ActionRegistry& registry,
        const SessionImpl& session) {
    if (node.kind != StrategyNodeKind::Operation) return {};
    if (node.bestiary_action_index != kNoId) {
        if (node.bestiary_action_index >=
            session.data->bestiary_actions.size()) {
            return {};
        }
        const BestiaryActionDescriptor& descriptor =
            session.data->bestiary_actions[node.bestiary_action_index];
        const int expected_type =
            descriptor.operation == BestiaryOperationKind::Create
                ? kStrategyBestiaryImprintOperation
                : kStrategyBestiaryRestoreImprintOperation;
        if (node.action_type != expected_type) return {};
        return {
            ResolvedStrategyOperationKind::Bestiary,
            node.bestiary_action_index};
    }
    if (node.action_type == kStrategyBestiaryImprintOperation ||
        node.action_type == kStrategyBestiaryRestoreImprintOperation) {
        return {};
    }
    const std::uint32_t action =
        resolve_registry_strategy_action(node, registry);
    if (action == kNoId) return {};
    return {
        node.action_type == kStrategyRestartOperation
            ? ResolvedStrategyOperationKind::Restart
            : ResolvedStrategyOperationKind::Ordinary,
        action};
}

std::uint32_t resolve_strategy_action(
        const StrategyNode& node,
        const ActionRegistry& registry) {
    return resolve_registry_strategy_action(node, registry);
}

bool evaluate_abstract_condition(
    const CompiledCondition& condition,
    const SessionImpl& session,
    const AbstractLayout& layout,
    const AbstractState& state) {
    switch (condition.kind) {
    case ConditionKind::Always:
        return true;
    case ConditionKind::HasModGroup: {
        const std::size_t slot = layout_slot_for(condition, layout);
        const bool present = condition.min_value == 0
                                 ? state.slot_status[slot] !=
                                       static_cast<std::uint8_t>(
                                           GoalSlotStatus::Absent)
                                 : state.slot_status[slot] ==
                                       static_cast<std::uint8_t>(
                                           GoalSlotStatus::Satisfied);
        if (!present) return false;
        if ((condition.required_flags & PC_MOD_SLOT_FRACTURED) != 0 &&
            (state.fractured_goal_mask & (1u << slot)) == 0) {
            return false;
        }
        if ((condition.required_flags & PC_MOD_SLOT_CRAFTED) != 0 &&
            (state.crafted_goal_mask & (1u << slot)) == 0) {
            return false;
        }
        return true;
    }
    case ConditionKind::HasModFamily: {
        const std::size_t slot = layout_slot_for(condition, layout);
        const bool present = condition.min_value == 0
                                 ? state.slot_status[slot] !=
                                       static_cast<std::uint8_t>(
                                           GoalSlotStatus::Absent)
                                 : state.slot_status[slot] ==
                                       static_cast<std::uint8_t>(
                                           GoalSlotStatus::Satisfied);
        if (!present) return false;
        if ((condition.required_flags & PC_MOD_SLOT_FRACTURED) != 0 &&
            (state.fractured_goal_mask & (1u << slot)) == 0) {
            return false;
        }
        if ((condition.required_flags & PC_MOD_SLOT_CRAFTED) != 0 &&
            (state.crafted_goal_mask & (1u << slot)) == 0) {
            return false;
        }
        return true;
    }
    case ConditionKind::RarityIs:
        return state.rarity == condition.min_value;
    case ConditionKind::OpenPrefixCount: {
        const int open = std::max(
            0, static_cast<int>(rarity_affix_cap(session, state.rarity)) -
                   static_cast<int>(state.prefix_count));
        return open >= condition.min_value && open <= condition.max_value;
    }
    case ConditionKind::OpenSuffixCount: {
        const int open = std::max(
            0, static_cast<int>(rarity_affix_cap(session, state.rarity)) -
                   static_cast<int>(state.suffix_count));
        return open >= condition.min_value && open <= condition.max_value;
    }
    case ConditionKind::PrefixCountRange:
        return state.prefix_count >= condition.min_value &&
               state.prefix_count <= condition.max_value;
    case ConditionKind::SuffixCountRange:
        return state.suffix_count >= condition.min_value &&
               state.suffix_count <= condition.max_value;
    case ConditionKind::ItemFlag: {
        std::uint32_t flag = 0;
        switch (condition.item_flag) {
        case ItemFlagKind::Corrupted: flag = kFlagCorrupted; break;
        case ItemFlagKind::Mirrored: flag = kFlagMirrored; break;
        case ItemFlagKind::Split: flag = kFlagSplit; break;
        case ItemFlagKind::Synthesised: flag = kFlagSynthesised; break;
        case ItemFlagKind::Fractured: flag = kFlagFractured; break;
        case ItemFlagKind::Crafted: flag = kFlagCraftedMod; break;
        case ItemFlagKind::Veiled: flag = kFlagVeiledMod; break;
        case ItemFlagKind::Multimod: flag = kFlagMultimod; break;
        case ItemFlagKind::NoAttack: flag = kFlagNoAttack; break;
        case ItemFlagKind::NoCaster: flag = kFlagNoCaster; break;
        case ItemFlagKind::PrefixesLocked: flag = kFlagPrefixesLocked; break;
        case ItemFlagKind::SuffixesLocked: flag = kFlagSuffixesLocked; break;
        case ItemFlagKind::Influenced: flag = kFlagInfluenced; break;
        case ItemFlagKind::EldritchImplicit:
            flag = kFlagEldritchImplicit;
            break;
        case ItemFlagKind::VeiledPrefix:
            return state.veiled_side == PC_SIDE_PREFIX;
        case ItemFlagKind::VeiledSuffix:
            return state.veiled_side == PC_SIDE_SUFFIX;
        }
        return (state.flags & flag) != 0;
    }
    case ConditionKind::InfluenceBits:
        return state.influence_bits == condition.min_value;
    case ConditionKind::EldritchTier: {
        const int tier = condition.eldritch_side == 0
                             ? state.searing_exarch_tier
                             : state.eater_of_worlds_tier;
        return tier >= condition.min_value && tier <= condition.max_value;
    }
    case ConditionKind::ModCount:
    case ConditionKind::ModFamilyCount: {
        const bool by_family =
            condition.kind == ConditionKind::ModFamilyCount;
        const std::vector<std::uint32_t>& ids =
            by_family ? condition.family_ids : condition.mod_ids;
        auto found = layout.count_observations.end();
        if (condition.count_memo_slot <
            layout.count_observation_by_memo_slot.size()) {
            const std::uint32_t observation =
                layout.count_observation_by_memo_slot[
                    condition.count_memo_slot];
            if (observation != kNoId) {
                found = layout.count_observations.begin() + observation;
            }
        }
        if (found == layout.count_observations.end()) {
            found = std::find_if(
                layout.count_observations.begin(),
                layout.count_observations.end(),
                [&](const CountObservation& observation) {
                    return observation.by_family == by_family &&
                           observation.ids == ids;
                });
        }
        if (found == layout.count_observations.end()) {
            throw std::logic_error(
                "compiled count condition missing from exact layout");
        }
        const std::size_t observation_index =
            static_cast<std::size_t>(
                std::distance(layout.count_observations.begin(), found));

        const bool fractured =
            (condition.required_flags & PC_MOD_SLOT_FRACTURED) != 0;
        const bool crafted =
            (condition.required_flags & PC_MOD_SLOT_CRAFTED) != 0;
        const CompactCountVector& junk_counts =
            fractured && crafted
                ? state.fractured_crafted_junk_counts
                : (fractured ? state.fractured_junk_counts
                             : (crafted ? state.crafted_junk_counts
                                        : state.junk_counts));
        int count = 0;
        for (const std::uint32_t junk : found->junk_class_indices) {
            count += junk_counts[junk];
        }
        for (std::size_t slot = 0; slot < layout.slots.size(); ++slot) {
            if (fractured &&
                (state.fractured_goal_mask & (1u << slot)) == 0) {
                continue;
            }
            if (crafted &&
                (state.crafted_goal_mask & (1u << slot)) == 0) {
                continue;
            }
            const std::uint8_t status = state.slot_status[slot];
            const ResolvedGoalSlot& goal_slot = layout.slots[slot];
            if (goal_slot.member_classes.empty()) {
                count += found->goal_status_counts[slot][status];
                continue;
            }
            if (status ==
                static_cast<std::uint8_t>(GoalSlotStatus::Absent)) {
                continue;
            }
            const std::uint32_t token =
                state.goal_member_class_tokens[slot];
            if (token == 0 || token > goal_slot.member_classes.size()) {
                throw std::logic_error(
                    "exact goal state has an invalid member-class token");
            }
            const GoalMemberClass& member_class =
                goal_slot.member_classes[token - 1];
            if (static_cast<std::uint8_t>(member_class.status) != status) {
                throw std::logic_error(
                    "exact goal member class disagrees with tier status");
            }
            const std::size_t word = observation_index / 64;
            if (word < member_class.count_observation_bits.size() &&
                (member_class.count_observation_bits[word] &
                 (std::uint64_t{1} << (observation_index % 64))) != 0) {
                ++count;
            }
        }
        return count >= condition.min_value &&
               count <= condition.max_value;
    }
    case ConditionKind::HasUnveilOption:
        return false; /* rejected during model derivation */
    case ConditionKind::ObservationSignature: {
        if (condition.observation_program == nullptr) {
            throw std::logic_error(
                "observation-signature condition has no program");
        }
        const refinement::CompiledObservationProgram& program =
            *condition.observation_program;
        const bool observes_goal_context =
            std::any_of(
                program.requirement.affix_observations.begin(),
                program.requirement.affix_observations.end(),
                [](const RefinementAffixObservation& observation) {
                    return (
                        observation.features &
                        refinement_feature(
                            RefinementFeature::
                                GoalStatusTierClass)) != 0;
                });
        if (observes_goal_context) {
            for (std::uint32_t mod = 0;
                 mod < session.mod_count; ++mod) {
                refinement::StableKey expected{
                    0u,
                    static_cast<std::uint8_t>(
                        GoalSlotStatus::Absent)};
                for (std::size_t slot = 0;
                     slot < layout.slots.size(); ++slot) {
                    if (!pc_bitset_test(
                            layout.slots[slot].member_mask.data(),
                            mod)) {
                        continue;
                    }
                    expected = {
                        slot + 1,
                        static_cast<std::uint8_t>(
                            pc_bitset_test(
                                layout.slots[slot]
                                    .satisfying_mask.data(),
                                mod)
                                ? GoalSlotStatus::Satisfied
                                : GoalSlotStatus::
                                      PresentBelowTier)};
                    break;
                }
                const refinement::StableKey actual =
                    mod <
                                program.context
                                    .goal_status_tier_class_by_mod
                                    .size() &&
                            !program.context
                                 .goal_status_tier_class_by_mod[mod]
                                 .empty()
                        ? program.context
                              .goal_status_tier_class_by_mod[mod]
                        : refinement::StableKey{
                              0u,
                              static_cast<std::uint8_t>(
                                  GoalSlotStatus::Absent)};
                if (actual != expected) {
                    throw std::logic_error(
                        "observation-signature goal context disagrees "
                        "with the exact layout");
                }
            }
        }
        const std::uint32_t observation_count =
            condition.observation_signature
                .count_observation_count;
        if (layout.count_observations.size() <
            observation_count) {
            throw std::logic_error(
                "observation-signature count context is absent "
                "from the exact layout");
        }
        for (std::uint32_t observation = 0;
             observation < observation_count;
             ++observation) {
            const CountObservation& layout_observation =
                layout.count_observations[observation];
            if (layout_observation.by_family) {
                throw std::logic_error(
                    "observation-signature count context changed "
                    "kind in the exact layout");
            }
            for (std::uint32_t mod = 0;
                 mod < session.mod_count; ++mod) {
                const refinement::StableKey& membership =
                    program.context
                        .count_observation_membership_by_mod[mod];
                const bool expected_member =
                    observation / 64 < membership.size() &&
                    (membership[observation / 64] &
                     (std::uint64_t{1} <<
                      (observation % 64))) != 0;
                const bool actual_member =
                    std::binary_search(
                        layout_observation.ids.begin(),
                        layout_observation.ids.end(),
                        mod);
                if (actual_member != expected_member) {
                    throw std::logic_error(
                        "observation-signature count context "
                        "disagrees with the exact layout");
                }
            }
        }
        refinement::AbstractFeatureExtraction extraction =
            refinement::extract_strict_abstract_features(
                session, layout, state, program.requirement);
        if (!extraction.complete()) return false;
        const std::size_t count_words =
            (observation_count + 63) / 64;
        for (refinement::FeatureAtom& atom :
             extraction.features) {
            if (atom.feature !=
                RefinementFeature::
                    CountObservationMembership) {
                continue;
            }
            atom.value.resize(count_words, 0);
            if (!atom.value.empty() &&
                observation_count % 64 != 0) {
                atom.value.back() &=
                    (std::uint64_t{1} <<
                     (observation_count % 64)) -
                    1;
            }
        }
        return refinement::observe_features(
                   refinement::canonical_feature_signature(
                       std::move(extraction.features)),
                   program.requirement) ==
               program.signature;
    }
    case ConditionKind::All:
        return std::all_of(
            condition.children.begin(), condition.children.end(),
            [&](const CompiledCondition& child) {
                return evaluate_abstract_condition(
                    child, session, layout, state);
            });
    case ConditionKind::Any:
        return std::any_of(
            condition.children.begin(), condition.children.end(),
            [&](const CompiledCondition& child) {
                return evaluate_abstract_condition(
                    child, session, layout, state);
            });
    case ConditionKind::Not:
        return !evaluate_abstract_condition(
            condition.children.front(), session, layout, state);
    case ConditionKind::AtLeast: {
        int matches = 0;
        for (const CompiledCondition& child : condition.children) {
            if (evaluate_abstract_condition(child, session, layout, state)) {
                ++matches;
            }
        }
        return matches >= condition.min_value;
    }
    }
    return false;
}


} // namespace solver
} // namespace poecraft
