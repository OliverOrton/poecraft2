#include "solver_refinement_feature_helpers.hpp"

namespace poecraft {
namespace solver {
namespace refinement {

FeatureSignature observe_features(
        const FeatureSignature& exact_features,
        const ObservationRequirement& input_requirement) {
    const ObservationRequirement requirement =
        canonical_observation_requirement(input_requirement);
    FeatureSignature item;
    std::map<std::uint32_t, FeatureSignature> affixes;
    for (const FeatureAtom& input : exact_features) {
        const RefinementFeatureMask bit =
            refinement_feature(input.feature);
        if (is_item_feature(input.feature)) {
            if ((requirement.item_features & bit) == 0) continue;
            FeatureAtom atom = input;
            atom.affix_traits = 0;
            atom.item_traits = 0;
            atom.modifier_tag_ids.clear();
            item.push_back(std::move(atom));
            continue;
        }
        bool observed = false;
        for (const RefinementAffixObservation& term :
             requirement.affix_observations) {
            if ((term.features & bit) != 0 &&
                refinement_selector_matches(
                    term.selector, input.affix_traits,
                    input.item_traits, input.modifier_tag_ids)) {
                observed = true;
                break;
            }
        }
        if (!observed) continue;
        FeatureAtom atom = input;
        if (input.feature ==
            RefinementFeature::ModifierClassificationTags) {
            atom.value.clear();
            std::set_intersection(
                input.modifier_tag_ids.begin(),
                input.modifier_tag_ids.end(),
                requirement.modifier_tag_ids.begin(),
                requirement.modifier_tag_ids.end(),
                std::back_inserter(atom.value));
        }
        atom.affix_traits = 0;
        atom.item_traits = 0;
        atom.modifier_tag_ids.clear();
        affixes[input.subject].push_back(std::move(atom));
    }
    struct Bundle {
        FeatureSignature atoms;
    };
    std::vector<Bundle> bundles;
    for (auto& [unused, atoms] : affixes) {
        (void)unused;
        for (FeatureAtom& atom : atoms) atom.subject = 0;
        std::sort(atoms.begin(), atoms.end(), atom_less);
        bundles.push_back({std::move(atoms)});
    }
    const auto bundle_less = [](const Bundle& left, const Bundle& right) {
        return std::lexicographical_compare(
            left.atoms.begin(), left.atoms.end(),
            right.atoms.begin(), right.atoms.end(), atom_less);
    };
    std::sort(bundles.begin(), bundles.end(), bundle_less);
    FeatureSignature out = std::move(item);
    for (std::uint32_t subject = 0;
         subject < bundles.size(); ++subject) {
        for (FeatureAtom atom : bundles[subject].atoms) {
            atom.subject = subject;
            out.push_back(std::move(atom));
        }
    }
    return canonical_feature_signature(std::move(out));
}

std::shared_ptr<const CompiledObservationProgram>
make_compiled_observation_program(
        const SessionImpl& session,
        const CompiledObservationSignature& compiled) {
    if (compiled.version !=
        kObservationSignatureConditionVersion) {
        throw std::invalid_argument(
            "unsupported compiled observation-signature version");
    }
    auto program =
        std::make_shared<CompiledObservationProgram>();
    program->requirement.item_features =
        compiled.item_features;
    program->requirement.modifier_tag_ids =
        compiled.modifier_tag_ids;
    for (const CompiledObservationAffixRequirement& input :
         compiled.affix_observations) {
        RefinementAffixObservation observation;
        observation.features = input.features;
        observation.selector.required_affix_traits =
            input.selector.required_affix_traits;
        observation.selector.forbidden_affix_traits =
            input.selector.forbidden_affix_traits;
        observation.selector.required_item_traits =
            input.selector.required_item_traits;
        observation.selector.forbidden_item_traits =
            input.selector.forbidden_item_traits;
        observation.selector.required_tag_ids =
            input.selector.required_tag_ids;
        program->requirement.affix_observations.push_back(
            std::move(observation));
    }
    program->requirement =
        canonical_observation_requirement(
            std::move(program->requirement));
    for (const CompiledObservationAtom& atom :
         compiled.atoms) {
        program->signature.push_back({
            static_cast<RefinementFeature>(atom.feature),
            atom.subject,
            atom.value,
            0,
            0,
            {}});
    }
    program->signature =
        canonical_feature_signature(
            std::move(program->signature));
    program->context.goal_status_tier_class_by_mod.resize(
        session.mod_count);
    for (const CompiledObservationModValue& entry :
         compiled.goal_status_tier_class_by_mod) {
        if (entry.mod_id >= session.mod_count) {
            throw std::invalid_argument(
                "compiled goal observation references an unknown "
                "modifier");
        }
        if (entry.value.size() != 2 ||
            entry.value[0] == 0 ||
            entry.value[1] >
                static_cast<std::uint8_t>(
                    GoalSlotStatus::Satisfied)) {
            throw std::invalid_argument(
                "compiled goal observation has an invalid semantic "
                "class");
        }
        program->context.goal_status_tier_class_by_mod[
            entry.mod_id] = entry.value;
    }
    program->context
        .count_observation_membership_by_mod.resize(
            session.mod_count,
            StableKey(
                (compiled.count_observation_count + 63) / 64,
                0));
    for (const CompiledObservationModValue& entry :
         compiled.count_observation_membership_by_mod) {
        if (entry.mod_id >= session.mod_count) {
            throw std::invalid_argument(
                "compiled count observation references an unknown "
                "modifier");
        }
        const std::size_t expected_words =
            (compiled.count_observation_count + 63) / 64;
        if (entry.value.size() != expected_words ||
            (!entry.value.empty() &&
             compiled.count_observation_count % 64 != 0 &&
             (entry.value.back() >>
              (compiled.count_observation_count % 64)) != 0)) {
            throw std::invalid_argument(
                "compiled count observation has an invalid semantic "
                "membership vector");
        }
        program->context
            .count_observation_membership_by_mod[
                entry.mod_id] = entry.value;
    }
    return program;
}

FeatureSignature observe_exact_item_features(
        const SessionImpl& session,
        const pc_item_state& item,
        const ObservationRequirement& input_requirement,
        const ExactObservationContext& context) {
    const ObservationRequirement requirement =
        canonical_observation_requirement(input_requirement);
    FeatureSignature exact;
    const auto emit_item =
        [&](const RefinementFeature feature, StableKey value) {
            exact.push_back(
                {feature, 0, std::move(value), 0, 0, {}});
        };

    bool crafted = false;
    bool fractured = false;
    bool veiled = false;
    bool multimod = false;
    bool no_attack = false;
    bool no_caster = false;
    bool prefix_lock = false;
    bool suffix_lock = false;
    const DataImpl& data = *session.data;
    const auto scan =
        [&](const pc_mod_slot* slots, const std::uint8_t count) {
            for (std::uint8_t index = 0; index < count; ++index) {
                const pc_mod_slot& slot = slots[index];
                crafted =
                    crafted ||
                    (slot.flags & PC_MOD_SLOT_CRAFTED) != 0;
                fractured =
                    fractured ||
                    (slot.flags & PC_MOD_SLOT_FRACTURED) != 0;
                veiled =
                    veiled ||
                    (slot.flags & PC_MOD_SLOT_VEILED) != 0;
                if (slot.mod_id >= session.metamod_type.size()) {
                    continue;
                }
                const std::int32_t role =
                    session.metamod_type[slot.mod_id];
                if (role < 0) {
                    continue;
                }
                multimod =
                    multimod ||
                    role == data.metamod_multimod_code;
                no_attack =
                    no_attack ||
                    role == data.metamod_no_attack_code;
                no_caster =
                    no_caster ||
                    role == data.metamod_no_caster_code;
                prefix_lock =
                    prefix_lock ||
                    role == data.metamod_prefixes_locked_code;
                suffix_lock =
                    suffix_lock ||
                    role == data.metamod_suffixes_locked_code;
            }
        };
    scan(item.prefixes, item.prefix_count);
    scan(item.suffixes, item.suffix_count);

    emit_item(RefinementFeature::Rarity, {item.rarity});
    emit_item(
        RefinementFeature::PrefixCount, {item.prefix_count});
    emit_item(
        RefinementFeature::SuffixCount, {item.suffix_count});
    emit_item(
        RefinementFeature::HasCraftedModifier,
        {crafted ? 1u : 0u});
    emit_item(
        RefinementFeature::HasFracturedModifier,
        {fractured ? 1u : 0u});
    emit_item(
        RefinementFeature::HasVeiledModifier,
        {veiled ? 1u : 0u});
    emit_item(
        RefinementFeature::Multimod, {multimod ? 1u : 0u});
    emit_item(
        RefinementFeature::PrefixLock,
        {prefix_lock ? 1u : 0u});
    emit_item(
        RefinementFeature::SuffixLock,
        {suffix_lock ? 1u : 0u});
    emit_item(
        RefinementFeature::CannotRollAttack,
        {no_attack ? 1u : 0u});
    emit_item(
        RefinementFeature::CannotRollCaster,
        {no_caster ? 1u : 0u});
    emit_item(
        RefinementFeature::Influence,
        {item.generic_influence_bits});
    emit_item(
        RefinementFeature::SearingExarchTier,
        {item.searing_exarch_tier});
    emit_item(
        RefinementFeature::EaterOfWorldsTier,
        {item.eater_of_worlds_tier});
    emit_item(
        RefinementFeature::EldritchPresence,
        {
            item.searing_exarch_tier != 0 ? 1u : 0u,
            item.eater_of_worlds_tier != 0 ? 1u : 0u,
        });
    const std::uint64_t dominance =
        item.searing_exarch_tier > item.eater_of_worlds_tier
            ? 1u
            : item.eater_of_worlds_tier >
                      item.searing_exarch_tier
                  ? 2u
                  : 0u;
    emit_item(
        RefinementFeature::EldritchDominance, {dominance});
    emit_item(
        RefinementFeature::Corrupted,
        {(item.item_flags & PC_ITEM_CORRUPTED) != 0 ? 1u : 0u});
    emit_item(
        RefinementFeature::Mirrored,
        {(item.item_flags & PC_ITEM_MIRRORED) != 0 ? 1u : 0u});
    emit_item(
        RefinementFeature::Split,
        {(item.item_flags & PC_ITEM_SPLIT) != 0 ? 1u : 0u});
    emit_item(
        RefinementFeature::Synthesised,
        {(item.item_flags & PC_ITEM_SYNTHESISED) != 0 ? 1u : 0u});

    const std::vector<std::uint32_t> relevant_tags =
        relevant_observation_tags(requirement);
    std::uint8_t item_traits = 0;
    if (item.searing_exarch_tier != item.eater_of_worlds_tier) {
        item_traits |= kRefinementItemHasEldritchDominance;
    }
    if (prefix_lock != suffix_lock) {
        item_traits |= kRefinementItemExactlyOneSideLocked;
    }
    std::uint32_t subject = 0;
    const auto visit =
        [&](const pc_mod_slot* slots, const std::uint8_t count,
            const std::int8_t side) {
            for (std::uint8_t index = 0; index < count; ++index) {
                const pc_mod_slot& slot = slots[index];
                if (slot.mod_id >= session.mod_count) continue;
                const std::uint32_t mod = slot.mod_id;
                const bool slot_crafted =
                    (slot.flags & PC_MOD_SLOT_CRAFTED) != 0;
                const bool slot_fractured =
                    (slot.flags & PC_MOD_SLOT_FRACTURED) != 0;
                const bool slot_veiled =
                    modifier_is_veiled_template(session, mod);
                std::uint16_t traits =
                    side == PC_SIDE_PREFIX
                        ? kRefinementAffixPrefix
                        : kRefinementAffixSuffix;
                if (slot_crafted) {
                    traits |= kRefinementAffixCrafted;
                }
                if (slot_fractured) {
                    traits |= kRefinementAffixFractured;
                }
                if (slot_veiled) {
                    traits |= kRefinementAffixVeiled;
                }
                if ((side == PC_SIDE_PREFIX && prefix_lock) ||
                    (side == PC_SIDE_SUFFIX && suffix_lock)) {
                    traits |= kRefinementAffixOnLockedSide;
                }
                if (item.searing_exarch_tier !=
                    item.eater_of_worlds_tier) {
                    const std::int8_t dominant_side =
                        item.searing_exarch_tier >
                                item.eater_of_worlds_tier
                            ? PC_SIDE_PREFIX
                            : PC_SIDE_SUFFIX;
                    traits |=
                        side == dominant_side
                            ? kRefinementAffixOnEldritchDominantSide
                            : kRefinementAffixOnEldritchNonDominantSide;
                }
                const std::vector<std::uint32_t> tags =
                    mod_relevant_tags(
                        session, mod, relevant_tags);
                const auto emit_affix =
                    [&](const RefinementFeature feature,
                        StableKey value) {
                        exact.push_back({
                            feature, subject, std::move(value),
                            traits, item_traits, tags});
                    };
                emit_affix(
                    RefinementFeature::ModifierSide,
                    {static_cast<std::uint64_t>(side)});
                emit_affix(
                    RefinementFeature::ModifierExclusionSignature,
                    modifier_exclusion_effect_signature(
                        session, mod));
                const StableKey default_goal{
                    0u,
                    static_cast<std::uint8_t>(
                        GoalSlotStatus::Absent)};
                emit_affix(
                    RefinementFeature::GoalStatusTierClass,
                    mod <
                            context
                                .goal_status_tier_class_by_mod
                                .size() &&
                            !context
                                 .goal_status_tier_class_by_mod[mod]
                                 .empty()
                        ? context
                              .goal_status_tier_class_by_mod[mod]
                        : default_goal);
                emit_affix(
                    RefinementFeature::ModifierCrafted,
                    {slot_crafted ? 1u : 0u});
                emit_affix(
                    RefinementFeature::ModifierFractured,
                    {slot_fractured ? 1u : 0u});
                emit_affix(
                    RefinementFeature::ModifierVeiled,
                    {slot_veiled ? 1u : 0u});
                emit_affix(
                    RefinementFeature::ModifierClassificationTags,
                    StableKey(
                        tags.begin(), tags.end()));
                emit_affix(
                    RefinementFeature::ModifierRequiredLevel,
                    {session.required_level.at(mod)});
                emit_affix(
                    RefinementFeature::CountObservationMembership,
                    mod <
                            context
                                .count_observation_membership_by_mod
                                .size()
                        ? context
                              .count_observation_membership_by_mod[mod]
                        : StableKey{});
                emit_affix(
                    RefinementFeature::ModifierMetamodRole,
                    {static_cast<std::uint32_t>(
                        mod < session.metamod_type.size()
                            ? session.metamod_type[mod]
                            : -1)});
                ++subject;
            }
        };
    visit(
        item.prefixes, item.prefix_count, PC_SIDE_PREFIX);
    visit(
        item.suffixes, item.suffix_count, PC_SIDE_SUFFIX);
    return observe_features(exact, requirement);
}

AbstractFeatureExtraction extract_strict_abstract_features(
        const SessionImpl& session,
        const AbstractLayout& layout,
        const AbstractState& state,
        const ObservationRequirement& input_requirement) {
    AbstractFeatureExtraction result;
    const ObservationRequirement requirement =
        canonical_observation_requirement(input_requirement);
    const auto item_requested =
        [&](const RefinementFeature feature) {
            return (requirement.item_features &
                    refinement_feature(feature)) != 0;
        };
    const auto emit_item =
        [&](const RefinementFeature feature, StableKey value) {
            if (!item_requested(feature)) return;
            result.features.push_back(
                {feature, 0, std::move(value), 0, 0, {}});
        };
    const auto flag = [&](const std::uint32_t bit) {
        return StableKey{(state.flags & bit) != 0 ? 1u : 0u};
    };

    emit_item(RefinementFeature::Rarity, {state.rarity});
    emit_item(RefinementFeature::PrefixCount, {state.prefix_count});
    emit_item(RefinementFeature::SuffixCount, {state.suffix_count});
    emit_item(
        RefinementFeature::HasCraftedModifier,
        flag(kFlagCraftedMod));
    emit_item(
        RefinementFeature::HasFracturedModifier,
        flag(kFlagFractured));
    emit_item(
        RefinementFeature::HasVeiledModifier,
        flag(kFlagVeiledMod));
    emit_item(RefinementFeature::Multimod, flag(kFlagMultimod));
    emit_item(
        RefinementFeature::PrefixLock,
        flag(kFlagPrefixesLocked));
    emit_item(
        RefinementFeature::SuffixLock,
        flag(kFlagSuffixesLocked));
    emit_item(
        RefinementFeature::CannotRollAttack,
        flag(kFlagNoAttack));
    emit_item(
        RefinementFeature::CannotRollCaster,
        flag(kFlagNoCaster));
    emit_item(
        RefinementFeature::Influence, {state.influence_bits});
    emit_item(
        RefinementFeature::SearingExarchTier,
        {state.searing_exarch_tier});
    emit_item(
        RefinementFeature::EaterOfWorldsTier,
        {state.eater_of_worlds_tier});
    emit_item(
        RefinementFeature::EldritchPresence,
        {
            state.searing_exarch_tier != 0 ? 1u : 0u,
            state.eater_of_worlds_tier != 0 ? 1u : 0u,
        });
    const std::uint64_t dominance =
        state.searing_exarch_tier > state.eater_of_worlds_tier
            ? 1u
            : state.eater_of_worlds_tier >
                      state.searing_exarch_tier
                  ? 2u
                  : 0u;
    emit_item(
        RefinementFeature::EldritchDominance, {dominance});
    emit_item(RefinementFeature::Corrupted, flag(kFlagCorrupted));
    emit_item(RefinementFeature::Mirrored, flag(kFlagMirrored));
    emit_item(RefinementFeature::Split, flag(kFlagSplit));
    emit_item(
        RefinementFeature::Synthesised, flag(kFlagSynthesised));

    RefinementFeatureMask requested_affix_features = 0;
    for (const RefinementAffixObservation& observation :
         requirement.affix_observations) {
        requested_affix_features |= observation.features;
    }
    if (requested_affix_features == 0) {
        result.features =
            canonical_feature_signature(std::move(result.features));
        return result;
    }

    const std::vector<std::uint32_t> relevant_tags =
        relevant_observation_tags(requirement);
    const std::uint8_t item_traits =
        refinement_item_traits(state);
    std::uint32_t next_subject = 0;
    std::uint32_t represented_prefixes = 0;
    std::uint32_t represented_suffixes = 0;

    const auto emit_carriers =
        [&](const std::vector<std::uint32_t>& members,
            const std::int8_t side_hint,
            const std::uint32_t goal_slot,
            const GoalSlotStatus goal_status,
            const std::optional<StableKey>& count_membership,
            const bool crafted,
            const bool fractured,
            const std::uint32_t count) {
            if (count == 0) return;
            if (members.empty()) {
                result.unavailable_features |=
                    requested_affix_features;
                return;
            }
            const auto side_value =
                uniform_member_value<std::int8_t>(
                    members, [&](const std::uint32_t mod) {
                        return session.gen_type.at(mod);
                    });
            const std::int8_t side =
                side_hint == PC_SIDE_PREFIX ||
                        side_hint == PC_SIDE_SUFFIX
                    ? side_hint
                    : side_value.value_or(-1);
            if (!side_value.has_value() || side < 0 ||
                *side_value != side) {
                result.unavailable_features |=
                    refinement_feature(
                        RefinementFeature::ModifierSide);
                return;
            }
            if (side == PC_SIDE_PREFIX) {
                represented_prefixes += count;
            } else {
                represented_suffixes += count;
            }
            const auto veiled_value =
                uniform_member_value<bool>(
                    members, [&](const std::uint32_t mod) {
                        return modifier_is_veiled_template(session, mod);
                    });
            const auto tags_value =
                uniform_member_value<std::vector<std::uint32_t>>(
                    members, [&](const std::uint32_t mod) {
                        return mod_relevant_tags(
                            session, mod, relevant_tags);
                    });
            if (!veiled_value.has_value() ||
                !tags_value.has_value()) {
                result.unavailable_features |=
                    requested_affix_features;
                return;
            }
            const std::uint16_t traits = affix_traits(
                state, side, crafted, fractured, *veiled_value);

            RefinementFeatureMask observed_here = 0;
            for (const RefinementAffixObservation& observation :
                 requirement.affix_observations) {
                if (refinement_selector_matches(
                        observation.selector, traits, item_traits,
                        *tags_value)) {
                    observed_here |= observation.features;
                }
            }
            if (observed_here == 0) return;

            std::optional<StableKey> exclusion;
            if ((observed_here &
                 refinement_feature(
                     RefinementFeature::
                         ModifierExclusionSignature)) != 0) {
                exclusion =
                    uniform_member_value<StableKey>(
                        members, [&](const std::uint32_t mod) {
                            return modifier_exclusion_effect_signature(
                                session, mod);
                        });
                if (!exclusion.has_value()) {
                    result.unavailable_features |=
                        refinement_feature(
                            RefinementFeature::
                                ModifierExclusionSignature);
                }
            }
            std::optional<std::uint32_t> required_level;
            if ((observed_here &
                 refinement_feature(
                     RefinementFeature::ModifierRequiredLevel)) != 0) {
                required_level =
                    uniform_member_value<std::uint32_t>(
                        members, [&](const std::uint32_t mod) {
                            return session.required_level.at(mod);
                        });
                if (!required_level.has_value()) {
                    result.unavailable_features |=
                        refinement_feature(
                            RefinementFeature::
                                ModifierRequiredLevel);
                }
            }
            std::optional<std::int32_t> metamod_role;
            if ((observed_here &
                 refinement_feature(
                     RefinementFeature::
                         ModifierMetamodRole)) != 0) {
                metamod_role =
                    uniform_member_value<std::int32_t>(
                        members, [&](const std::uint32_t mod) {
                            return mod <
                                           session.metamod_type.size()
                                       ? session.metamod_type[mod]
                                       : -1;
                        });
                if (!metamod_role.has_value()) {
                    result.unavailable_features |=
                        refinement_feature(
                            RefinementFeature::
                                ModifierMetamodRole);
                }
            }
            if ((observed_here &
                 refinement_feature(
                     RefinementFeature::
                         CountObservationMembership)) != 0 &&
                !count_membership.has_value()) {
                result.unavailable_features |=
                    refinement_feature(
                        RefinementFeature::
                            CountObservationMembership);
            }

            for (std::uint32_t occurrence = 0;
                 occurrence < count; ++occurrence) {
                const std::uint32_t subject = next_subject++;
                const auto emit_affix =
                    [&](const RefinementFeature feature,
                        StableKey value) {
                        if ((observed_here &
                             refinement_feature(feature)) == 0) {
                            return;
                        }
                        result.features.push_back({
                            feature,
                            subject,
                            std::move(value),
                            traits,
                            item_traits,
                            *tags_value});
                    };
                emit_affix(
                    RefinementFeature::ModifierSide,
                    {static_cast<std::uint64_t>(side)});
                if (exclusion.has_value()) {
                    emit_affix(
                        RefinementFeature::
                            ModifierExclusionSignature,
                        *exclusion);
                }
                emit_affix(
                    RefinementFeature::GoalStatusTierClass,
                    {
                        goal_slot == kNoId
                            ? 0u
                            : static_cast<std::uint64_t>(
                                  goal_slot + 1),
                        static_cast<std::uint8_t>(goal_status),
                    });
                emit_affix(
                    RefinementFeature::ModifierCrafted,
                    {crafted ? 1u : 0u});
                emit_affix(
                    RefinementFeature::ModifierFractured,
                    {fractured ? 1u : 0u});
                emit_affix(
                    RefinementFeature::ModifierVeiled,
                    {*veiled_value ? 1u : 0u});
                StableKey observed_tags;
                for (const std::uint32_t tag :
                     requirement.modifier_tag_ids) {
                    if (std::binary_search(
                            tags_value->begin(), tags_value->end(),
                            tag)) {
                        observed_tags.push_back(tag);
                    }
                }
                emit_affix(
                    RefinementFeature::ModifierClassificationTags,
                    std::move(observed_tags));
                if (required_level.has_value()) {
                    emit_affix(
                        RefinementFeature::ModifierRequiredLevel,
                        {*required_level});
                }
                if (count_membership.has_value()) {
                    emit_affix(
                        RefinementFeature::
                            CountObservationMembership,
                        *count_membership);
                }
                if (metamod_role.has_value()) {
                    emit_affix(
                        RefinementFeature::ModifierMetamodRole,
                        {static_cast<std::uint32_t>(
                            *metamod_role)});
                }
            }
        };

    for (std::uint32_t slot_index = 0;
         slot_index < layout.slots.size(); ++slot_index) {
        const GoalSlotStatus status =
            static_cast<GoalSlotStatus>(
                state.slot_status[slot_index]);
        if (status == GoalSlotStatus::Absent) continue;
        const ResolvedGoalSlot& slot = layout.slots[slot_index];
        std::vector<std::uint32_t> members;
        std::int8_t side = -1;
        std::optional<StableKey> count_membership;
        const std::uint32_t token =
            state.goal_member_class_tokens[slot_index];
        if (token != 0 && token <= slot.member_classes.size()) {
            const GoalMemberClass& member_class =
                slot.member_classes[token - 1];
            members = mask_members(
                session, member_class.member_mask);
            side = member_class.gen_type;
            count_membership =
                uniform_member_value<StableKey>(
                    members, [&](const std::uint32_t mod) {
                        return mod_count_observation_bits(
                            layout, mod);
                    });
        } else {
            for (const std::uint32_t mod :
                 mask_members(session, slot.member_mask)) {
                const bool satisfying = pc_bitset_test(
                    slot.satisfying_mask.data(), mod);
                if ((status == GoalSlotStatus::Satisfied) ==
                    satisfying) {
                    members.push_back(mod);
                }
            }
            count_membership =
                uniform_member_value<StableKey>(
                    members, [&](const std::uint32_t mod) {
                        return mod_count_observation_bits(
                            layout, mod);
                    });
        }
        emit_carriers(
            members, side, slot_index, status, count_membership,
            (state.crafted_goal_mask & (1u << slot_index)) != 0,
            (state.fractured_goal_mask & (1u << slot_index)) != 0,
            1);
    }

    const std::size_t junk_count = std::min({
        layout.junk_classes.size(),
        state.junk_counts.size(),
        state.fractured_junk_counts.size(),
        state.crafted_junk_counts.size(),
        state.fractured_crafted_junk_counts.size()});
    for (std::size_t index = 0; index < junk_count; ++index) {
        const JunkClass& junk = layout.junk_classes[index];
        const std::uint32_t total = state.junk_counts[index];
        const std::uint32_t fractured =
            state.fractured_junk_counts[index];
        const std::uint32_t crafted =
            state.crafted_junk_counts[index];
        const std::uint32_t both =
            state.fractured_crafted_junk_counts[index];
        if (both > fractured || both > crafted ||
            fractured + crafted - both > total) {
            result.unavailable_features |=
                requested_affix_features;
            continue;
        }
        const std::vector<std::uint32_t> members =
            mask_members(session, junk.member_mask);
        const std::optional<StableKey> count_membership =
            uniform_member_value<StableKey>(
                members, [&](const std::uint32_t mod) {
                    return mod_count_observation_bits(layout, mod);
                });
        emit_carriers(
            members, junk.gen_type, kNoId, GoalSlotStatus::Absent,
            count_membership,
            false, false, total - fractured - crafted + both);
        emit_carriers(
            members, junk.gen_type, kNoId, GoalSlotStatus::Absent,
            count_membership,
            true, false, crafted - both);
        emit_carriers(
            members, junk.gen_type, kNoId, GoalSlotStatus::Absent,
            count_membership,
            false, true, fractured - both);
        emit_carriers(
            members, junk.gen_type, kNoId, GoalSlotStatus::Absent,
            count_membership,
            true, true, both);
    }
    if (junk_count != layout.junk_classes.size()) {
        result.unavailable_features |= requested_affix_features;
    }
    if (represented_prefixes != state.prefix_count ||
        represented_suffixes != state.suffix_count) {
        /*
         * A compact layout can omit an occupied exact carrier (for example an
         * externally supplied metamod that no candidate action creates).
         * Refusing the signature is the only sound choice when an affix
         * observer is active.
         */
        result.unavailable_features |= requested_affix_features;
    }
    result.features =
        canonical_feature_signature(std::move(result.features));
    return result;
}

std::optional<StableKey> canonical_operation_state_signature(
        const SessionImpl& session,
        const AbstractLayout& layout,
        const AbstractState& state,
        const SelectedAction& action) {
    if (!action.contract.complete() ||
        action.semantic_key.empty() ||
        !selected_runtime_contracts_complete(action)) {
        return std::nullopt;
    }
    const ObservationRequirement requirement = merge_requirements(
        observation_requirement_from_selected_action(action),
        action.routing_observes);
    const AbstractFeatureExtraction extraction =
        extract_strict_abstract_features(
            session, layout, state, requirement);
    if (!extraction.complete()) return std::nullopt;
    const FeatureSignature observed =
        observe_features(extraction.features, requirement);
    StableKey signature{
        0x706372666e7631ull, /* "pcrfnv1" */
        action.action_id};
    append_tokens(signature, action.semantic_key);
    const StableKey contract_signature =
        selected_runtime_contract_signature(action);
    append_tokens(signature, contract_signature);
    append_requirement(signature, requirement);
    signature.push_back(observed.size());
    for (const FeatureAtom& atom : observed) {
        append_atom(signature, atom);
    }
    return signature;
}

} // namespace refinement
} // namespace solver
} // namespace poecraft
