#pragma once

#include "solver_refinement_observation_helpers.hpp"

namespace poecraft {
namespace solver {
namespace refinement {
namespace {

std::vector<std::uint32_t> mask_members(
        const SessionImpl& session,
        const std::vector<std::uint64_t>& mask) {
    std::vector<std::uint32_t> members;
    if (mask.size() < session.words) return members;
    for (std::uint32_t mod = 0; mod < session.mod_count; ++mod) {
        if (pc_bitset_test(mask.data(), mod)) members.push_back(mod);
    }
    return members;
}

std::vector<std::uint32_t> relevant_observation_tags(
        const ObservationRequirement& requirement) {
    std::vector<std::uint32_t> tags = requirement.modifier_tag_ids;
    for (const RefinementAffixObservation& observation :
         requirement.affix_observations) {
        tags.insert(
            tags.end(),
            observation.selector.required_tag_ids.begin(),
            observation.selector.required_tag_ids.end());
    }
    std::sort(tags.begin(), tags.end());
    tags.erase(std::unique(tags.begin(), tags.end()), tags.end());
    return tags;
}

std::vector<std::uint32_t> mod_relevant_tags(
        const SessionImpl& session,
        const std::uint32_t mod,
        const std::vector<std::uint32_t>& relevant) {
    std::vector<std::uint32_t> tags;
    if (mod + 1 >= session.class_offsets.size()) return tags;
    for (std::uint32_t offset = session.class_offsets[mod];
         offset < session.class_offsets[mod + 1]; ++offset) {
        const std::uint32_t tag = session.class_tag_ids.at(offset);
        if (std::binary_search(relevant.begin(), relevant.end(), tag)) {
            tags.push_back(tag);
        }
    }
    std::sort(tags.begin(), tags.end());
    tags.erase(std::unique(tags.begin(), tags.end()), tags.end());
    return tags;
}

StableKey mod_count_observation_bits(
        const AbstractLayout& layout,
        const std::uint32_t mod) {
    StableKey bits((layout.count_observations.size() + 63) / 64, 0);
    for (std::size_t observation = 0;
         observation < layout.count_observations.size();
         ++observation) {
        const std::vector<std::uint64_t>& mask =
            layout.count_observations[observation].member_mask;
        if (!mask.empty() && pc_bitset_test(mask.data(), mod)) {
            bits[observation / 64] |=
                std::uint64_t{1} << (observation % 64);
        }
    }
    return bits;
}

template <typename Value, typename Extract>
std::optional<Value> uniform_member_value(
        const std::vector<std::uint32_t>& members,
        Extract extract) {
    if (members.empty()) return std::nullopt;
    const Value first = extract(members.front());
    for (std::size_t i = 1; i < members.size(); ++i) {
        if (!(extract(members[i]) == first)) return std::nullopt;
    }
    return first;
}

std::uint16_t affix_traits(
        const AbstractState& state,
        const std::int8_t side,
        const bool crafted,
        const bool fractured,
        const bool veiled) {
    std::uint16_t traits =
        side == PC_SIDE_PREFIX
            ? kRefinementAffixPrefix
            : kRefinementAffixSuffix;
    if (crafted) traits |= kRefinementAffixCrafted;
    if (fractured) traits |= kRefinementAffixFractured;
    if (veiled) traits |= kRefinementAffixVeiled;
    if ((side == PC_SIDE_PREFIX &&
         (state.flags & kFlagPrefixesLocked) != 0) ||
        (side == PC_SIDE_SUFFIX &&
         (state.flags & kFlagSuffixesLocked) != 0)) {
        traits |= kRefinementAffixOnLockedSide;
    }
    if (state.searing_exarch_tier != state.eater_of_worlds_tier) {
        const std::int8_t dominant =
            state.searing_exarch_tier > state.eater_of_worlds_tier
                ? PC_SIDE_PREFIX
                : PC_SIDE_SUFFIX;
        traits |= side == dominant
                      ? kRefinementAffixOnEldritchDominantSide
                      : kRefinementAffixOnEldritchNonDominantSide;
    }
    return traits;
}

std::uint8_t refinement_item_traits(const AbstractState& state) {
    std::uint8_t traits =
        state.searing_exarch_tier != state.eater_of_worlds_tier
            ? kRefinementItemHasEldritchDominance
            : 0;
    const bool prefix_locked =
        (state.flags & kFlagPrefixesLocked) != 0;
    const bool suffix_locked =
        (state.flags & kFlagSuffixesLocked) != 0;
    if (prefix_locked != suffix_locked) {
        traits |= kRefinementItemExactlyOneSideLocked;
    }
    return traits;
}

} // namespace

} // namespace refinement
} // namespace solver
} // namespace poecraft
