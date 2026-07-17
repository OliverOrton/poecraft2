#include "solver_internal.hpp"

#include <algorithm>
#include <cstring>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

#include "poecraft/bitset.h"

/*
 * Solver S1: goal resolution, junk-class derivation, and item projection.
 *
 * The junk equivalence rule (docs/solver/crafting-solver-plan.md): two non-goal mods
 * collapse into one class iff every candidate action treats them identically.
 * Only three features break equivalence: generation side, the restricted tag
 * signature (classification tags some candidate action discriminates on),
 * and which goal slots the mod's exclusivity groups block. The classes are
 * derived from the session masks, never designed by hand; a coarser action
 * set automatically yields a coarser state space.
 */
namespace poecraft {
namespace solver {

namespace {

[[noreturn]] void invalid(const std::string& message) {
    throw std::runtime_error("solver: " + message);
}

std::vector<std::uint64_t> empty_mask(const SessionImpl& session) {
    return std::vector<std::uint64_t>(session.words, 0);
}

bool mask_nonempty(const std::vector<std::uint64_t>& mask) {
    for (std::uint64_t word : mask) {
        if (word != 0) return true;
    }
    return false;
}

bool masks_intersect(const std::vector<std::uint64_t>& a,
                     const std::vector<std::uint64_t>& b) {
    const std::size_t words = std::min(a.size(), b.size());
    for (std::size_t w = 0; w < words; ++w) {
        if (a[w] & b[w]) return true;
    }
    return false;
}

void mask_or_into(std::vector<std::uint64_t>& out,
                  const std::vector<std::uint64_t>& mask) {
    const std::size_t words = std::min(out.size(), mask.size());
    for (std::size_t w = 0; w < words; ++w) {
        out[w] |= mask[w];
    }
}

/* Sorted exclusivity group ids of one session mod. */
void mod_groups(const SessionImpl& session,
                std::uint32_t mod_id,
                std::vector<std::uint32_t>& out) {
    out.clear();
    const std::uint32_t begin = session.group_offsets[mod_id];
    const std::uint32_t end = session.group_offsets[mod_id + 1];
    for (std::uint32_t i = begin; i < end; ++i) {
        out.push_back(session.group_ids[i]);
    }
}

bool sorted_contains(const std::vector<std::uint32_t>& sorted,
                     std::uint32_t value) {
    return std::binary_search(sorted.begin(), sorted.end(), value);
}

ResolvedGoalSlot resolve_slot(const SessionImpl& session,
                              const GoalSlot& spec,
                              std::size_t slot_index) {
    const bool by_group = spec.group_id != kNoId;
    const bool by_family = spec.family_id != kNoId;
    if (by_group == by_family) {
        invalid("goal slot " + std::to_string(slot_index) +
                " must set exactly one of group_id/family_id");
    }

    ResolvedGoalSlot slot;
    slot.spec = spec;
    slot.member_mask = empty_mask(session);
    slot.satisfying_mask = empty_mask(session);

    if (by_group) {
        if (spec.group_id >= session.group_masks.size() ||
            session.group_masks[spec.group_id].empty()) {
            invalid("goal slot " + std::to_string(slot_index) +
                    " references a group with no session mods");
        }
    }
    for (std::uint32_t mod = 0; mod < session.mod_count; ++mod) {
        const bool member =
            by_group
                ? pc_bitset_test(session.group_masks[spec.group_id].data(),
                                 mod)
                : session.family_id[mod] == spec.family_id;
        if (!member) continue;
        /* Only explicit affix mods can be goal members. */
        if (session.gen_type[mod] != 0 && session.gen_type[mod] != 1) {
            continue;
        }
        pc_bitset_set(slot.member_mask.data(), mod);
        const std::uint32_t tier = session.family_tier_index[mod];
        const bool tier_ok =
            spec.min_tier == 0 || (tier != 0 && tier <= spec.min_tier);
        if (tier_ok) {
            pc_bitset_set(slot.satisfying_mask.data(), mod);
        }
    }
    if (!mask_nonempty(slot.satisfying_mask)) {
        invalid("goal slot " + std::to_string(slot_index) +
                " has no session mod satisfying its tier requirement");
    }

    std::vector<std::uint32_t> groups;
    pc_bitset_for_each(
        slot.satisfying_mask.data(), session.words, [&](std::size_t mod) {
            const std::uint32_t begin =
                session.group_offsets[static_cast<std::uint32_t>(mod)];
            const std::uint32_t end =
                session.group_offsets[static_cast<std::uint32_t>(mod) + 1];
            for (std::uint32_t i = begin; i < end; ++i) {
                groups.push_back(session.group_ids[i]);
            }
        });
    std::sort(groups.begin(), groups.end());
    groups.erase(std::unique(groups.begin(), groups.end()), groups.end());
    slot.blocking_group_ids = std::move(groups);
    return slot;
}

/*
 * Union of session mods a candidate action can place in an explicit affix
 * slot. Supersets are safe: an unreachable mod in the universe only creates
 * a junk class whose count is always zero.
 */
void action_reachable_mask(const SessionImpl& session,
                           const ActionDescriptor& action,
                           std::vector<std::uint64_t>& scratch,
                           std::vector<std::uint64_t>& out) {
    if (session.words == 0) return;
    const auto normal_positive = [&]() {
        pc_bitset_and(scratch.data(), session.normal_random_roll_mask.data(),
                      session.positive_spawn_weight_mask.data(),
                      session.words);
        mask_or_into(out, scratch);
    };
    if (action.synthetic) return;
    switch (action.params.type) {
    case ActionType::Transmute:
    case ActionType::Augment:
    case ActionType::Alteration:
    case ActionType::Regal:
    case ActionType::Alchemy:
    case ActionType::Chaos:
    case ActionType::Exalt:
    case ActionType::EldritchExalt:
    case ActionType::EldritchChaos:
    case ActionType::HarvestReforge:
        normal_positive();
        break;
    case ActionType::Annul:
    case ActionType::EldritchAnnul:
    case ActionType::Scour:
    case ActionType::RemoveCraftedModifiers:
    case ActionType::Fracture:
    case ActionType::EldritchEmber:
    case ActionType::EldritchIchor: /* implicit-side; not explicit junk */
        break;
    case ActionType::Essence:
        normal_positive();
        if (action.params.essence_index <
                session.essence_guaranteed_mod_ids.size() &&
            session.essence_guaranteed_mod_ids[action.params.essence_index] !=
                kNoId) {
            pc_bitset_set(
                out.data(),
                session.essence_guaranteed_mod_ids[
                    action.params.essence_index]);
        }
        break;
    case ActionType::Fossil:
        normal_positive();
        for (std::uint32_t fossil : action.params.fossil_indices) {
            if (fossil < session.fossil_added_mod_ids.size()) {
                for (std::uint32_t mod :
                     session.fossil_added_mod_ids[fossil]) {
                    pc_bitset_set(out.data(), mod);
                }
            }
            if (fossil < session.fossil_forced_mod_ids.size()) {
                for (std::uint32_t mod :
                     session.fossil_forced_mod_ids[fossil]) {
                    pc_bitset_set(out.data(), mod);
                }
            }
        }
        break;
    case ActionType::Bench:
        if (action.params.mod_id != kNoId) {
            pc_bitset_set(out.data(), action.params.mod_id);
        }
        break;
    case ActionType::VeiledChaos:
    case ActionType::VeiledExalt:
        normal_positive();
        if (session.veiled_prefix_mod_id != kNoId) {
            pc_bitset_set(out.data(), session.veiled_prefix_mod_id);
        }
        if (session.veiled_suffix_mod_id != kNoId) {
            pc_bitset_set(out.data(), session.veiled_suffix_mod_id);
        }
        break;
    case ActionType::Unveil:
        mask_or_into(out, session.unveiled_mask);
        break;
    case ActionType::HarvestAugment:
    case ActionType::HarvestResist:
        if (action.params.target_tag_id != kNoId &&
            action.params.target_tag_id < session.implicit_tag_masks.size() &&
            !session.implicit_tag_masks[action.params.target_tag_id]
                 .empty()) {
            pc_bitset_and(
                scratch.data(),
                session.implicit_tag_masks[action.params.target_tag_id]
                    .data(),
                session.positive_spawn_weight_mask.data(), session.words);
            mask_or_into(out, scratch);
        }
        break;
    case ActionType::InfluenceExalt:
        if (action.params.influence_code > 0 &&
            static_cast<std::size_t>(action.params.influence_code) <
                session.influence_masks.size() &&
            !session.influence_masks[action.params.influence_code].empty()) {
            mask_or_into(
                out, session.influence_masks[action.params.influence_code]);
        }
        break;
    }
}

} // namespace

AbstractLayout build_abstract_layout(
    const SessionImpl& session,
    const GoalSpec& goal,
    const ActionRegistry& registry,
    const std::vector<std::uint32_t>& action_indices,
    bool allow_empty_goal,
    bool empty_actions_mean_all,
    bool distinguish_junk_exclusion_effects) {
    if (goal.slots.empty() && !allow_empty_goal) {
        invalid("goal spec has no slots");
    }
    if (goal.slots.size() > kMaxGoalSlots) {
        invalid("goal spec exceeds " + std::to_string(kMaxGoalSlots) +
                " slots");
    }
    if (!goal.slots.empty() &&
        (goal.required_satisfied_slots() == 0 ||
         goal.required_satisfied_slots() > goal.slots.size())) {
        invalid("goal min_satisfied_slots is outside the slot range");
    }

    AbstractLayout layout;
    for (std::size_t i = 0; i < goal.slots.size(); ++i) {
        layout.slots.push_back(resolve_slot(session, goal.slots[i], i));
    }
    for (std::size_t a = 0; a < layout.slots.size(); ++a) {
        for (std::size_t b = a + 1; b < layout.slots.size(); ++b) {
            if (masks_intersect(layout.slots[a].member_mask,
                                layout.slots[b].member_mask)) {
                invalid("goal slots " + std::to_string(a) + " and " +
                        std::to_string(b) + " have overlapping members");
            }
        }
    }

    std::vector<std::uint32_t> candidates = action_indices;
    if (candidates.empty() && empty_actions_mean_all) {
        candidates.resize(registry.actions.size());
        for (std::uint32_t i = 0; i < candidates.size(); ++i) {
            candidates[i] = i;
        }
    }

    /* Union of discriminating tags across the candidate action set. */
    std::vector<std::uint32_t> tags;
    for (std::uint32_t index : candidates) {
        if (index >= registry.actions.size()) {
            invalid("action index out of range");
        }
        const auto& ids = registry.actions[index].discriminating_tag_ids;
        tags.insert(tags.end(), ids.begin(), ids.end());
    }
    std::sort(tags.begin(), tags.end());
    tags.erase(std::unique(tags.begin(), tags.end()), tags.end());
    if (tags.size() > kMaxDiscriminatingTags) {
        invalid("candidate actions discriminate on " +
                std::to_string(tags.size()) + " tags; the packed limit is " +
                std::to_string(kMaxDiscriminatingTags));
    }
    layout.discriminating_tag_ids = std::move(tags);

    /* Explicit-reachable junk universe for this action set. */
    std::vector<std::uint64_t> reachable = empty_mask(session);
    std::vector<std::uint64_t> scratch = empty_mask(session);
    for (std::uint32_t index : candidates) {
        action_reachable_mask(session, registry.actions[index], scratch,
                              reachable);
    }
    for (const ResolvedGoalSlot& slot : layout.slots) {
        pc_bitset_andnot(reachable.data(), reachable.data(),
                         slot.member_mask.data(), session.words);
    }

    /* Partition by (generation side, restricted tag signature, blocked goal
     * slots), plus the complete group-exclusion effect for strict evaluation
     * layouts. std::map keeps the class order deterministic. */
    using ClassKey = std::tuple<
        std::int8_t, std::uint64_t, std::uint32_t, std::uint8_t,
        std::vector<std::uint64_t>>;
    std::map<ClassKey, std::vector<std::uint32_t>> classes;
    std::vector<std::uint32_t> groups;
    pc_bitset_for_each(reachable.data(), session.words, [&](std::size_t bit) {
        const std::uint32_t mod = static_cast<std::uint32_t>(bit);
        const std::int8_t gen = session.gen_type[mod];
        if (gen != 0 && gen != 1) return;

        std::uint64_t tag_bits = 0;
        const std::uint32_t begin = session.class_offsets[mod];
        const std::uint32_t end = session.class_offsets[mod + 1];
        for (std::uint32_t i = begin; i < end; ++i) {
            const auto it = std::lower_bound(
                layout.discriminating_tag_ids.begin(),
                layout.discriminating_tag_ids.end(),
                session.class_tag_ids[i]);
            if (it != layout.discriminating_tag_ids.end() &&
                *it == session.class_tag_ids[i]) {
                tag_bits |= std::uint64_t{1}
                            << (it - layout.discriminating_tag_ids.begin());
            }
        }

        std::uint32_t block_mask = 0;
        mod_groups(session, mod, groups);
        for (std::size_t s = 0; s < layout.slots.size(); ++s) {
            for (std::uint32_t group : groups) {
                if (sorted_contains(layout.slots[s].blocking_group_ids,
                                    group)) {
                    block_mask |= 1u << s;
                    break;
                }
            }
        }
        std::vector<std::uint64_t> exclusion_effect;
        if (distinguish_junk_exclusion_effects) {
            exclusion_effect = empty_mask(session);
            for (std::uint32_t group : groups) {
                if (group < session.group_masks.size() &&
                    !session.group_masks[group].empty()) {
                    pc_bitset_or(
                        exclusion_effect.data(), exclusion_effect.data(),
                        session.group_masks[group].data(), session.words);
                }
            }
        }
        const std::uint8_t special_role =
            mod == session.veiled_prefix_mod_id
                ? 1
                : (mod == session.veiled_suffix_mod_id ? 2 : 0);
        classes[{gen, tag_bits, block_mask, special_role,
                 std::move(exclusion_effect)}]
            .push_back(mod);
    });

    layout.junk_class_by_mod.assign(session.mod_count, kNoId);
    for (const auto& [key, members] : classes) {
        JunkClass junk;
        junk.gen_type = std::get<0>(key);
        junk.tag_bits = std::get<1>(key);
        junk.goal_block_mask = std::get<2>(key);
        junk.exclusion_effect_mask = std::get<4>(key);
        junk.member_mask = empty_mask(session);
        for (std::uint32_t mod : members) {
            pc_bitset_set(junk.member_mask.data(), mod);
            layout.junk_class_by_mod[mod] =
                static_cast<std::uint32_t>(layout.junk_classes.size());
        }
        junk.member_count = static_cast<std::uint32_t>(members.size());
        layout.junk_classes.push_back(std::move(junk));
    }
    return layout;
}

AbstractState project_item(
    const SessionImpl& session,
    const AbstractLayout& layout,
    const pc_item_state& item) {
    AbstractState state;
    state.prefix_count = item.prefix_count;
    state.suffix_count = item.suffix_count;
    state.rarity = item.rarity;
    state.influence_bits = item.generic_influence_bits;
    state.searing_exarch_tier = item.searing_exarch_tier;
    state.eater_of_worlds_tier = item.eater_of_worlds_tier;
    state.junk_counts.assign(layout.junk_classes.size(), 0);
    state.fractured_junk_counts.assign(layout.junk_classes.size(), 0);
    state.crafted_junk_counts.assign(layout.junk_classes.size(), 0);
    state.fractured_crafted_junk_counts.assign(
        layout.junk_classes.size(), 0);

    if (item.item_flags & PC_ITEM_CORRUPTED) state.flags |= kFlagCorrupted;
    if (item.item_flags & PC_ITEM_MIRRORED) state.flags |= kFlagMirrored;
    if (item.item_flags & PC_ITEM_SPLIT) state.flags |= kFlagSplit;
    if (item.item_flags & PC_ITEM_SYNTHESISED) state.flags |= kFlagSynthesised;
    if (item.generic_influence_bits) state.flags |= kFlagInfluenced;
    if (item.searing_exarch_tier || item.eater_of_worlds_tier) {
        state.flags |= kFlagEldritchImplicit;
    }

    const DataImpl& data = *session.data;
    std::vector<std::uint32_t> groups;
    const auto visit = [&](const pc_mod_slot& slot, int side) {
        const std::uint32_t mod = slot.mod_id;
        if (mod == PC_MOD_NONE || mod >= session.mod_count) return;

        if (slot.flags & PC_MOD_SLOT_FRACTURED) state.flags |= kFlagFractured;
        if (slot.flags & PC_MOD_SLOT_CRAFTED) state.flags |= kFlagCraftedMod;
        if (slot.flags & PC_MOD_SLOT_VEILED) {
            state.flags |= kFlagVeiledMod;
            state.veiled_side = static_cast<std::int8_t>(side);
        }
        const std::int32_t metamod = session.metamod_type[mod];
        if (metamod >= 0) {
            if (metamod == data.metamod_multimod_code) {
                state.flags |= kFlagMultimod;
            } else if (metamod == data.metamod_no_attack_code) {
                state.flags |= kFlagNoAttack;
            } else if (metamod == data.metamod_no_caster_code) {
                state.flags |= kFlagNoCaster;
            } else if (metamod == data.metamod_prefixes_locked_code) {
                state.flags |= kFlagPrefixesLocked;
            } else if (metamod == data.metamod_suffixes_locked_code) {
                state.flags |= kFlagSuffixesLocked;
            }
        }

        mod_groups(session, mod, groups);
        for (std::size_t s = 0; s < layout.slots.size(); ++s) {
            const ResolvedGoalSlot& goal_slot = layout.slots[s];
            const bool member =
                pc_bitset_test(goal_slot.member_mask.data(), mod);
            if (member) {
                if (slot.flags & PC_MOD_SLOT_FRACTURED) {
                    state.fractured_goal_mask |= 1u << s;
                }
                if (slot.flags & PC_MOD_SLOT_CRAFTED) {
                    state.crafted_goal_mask |= 1u << s;
                }
            }
            if (pc_bitset_test(goal_slot.satisfying_mask.data(), mod)) {
                state.slot_status[s] = static_cast<std::uint8_t>(
                    GoalSlotStatus::Satisfied);
                continue;
            }
            if (member) {
                if (state.slot_status[s] !=
                    static_cast<std::uint8_t>(GoalSlotStatus::Satisfied)) {
                    state.slot_status[s] = static_cast<std::uint8_t>(
                        GoalSlotStatus::PresentBelowTier);
                }
                continue;
            }
            for (std::uint32_t group : groups) {
                if (sorted_contains(goal_slot.blocking_group_ids, group)) {
                    state.blocked_mask |= 1u << s;
                    break;
                }
            }
        }

        const std::uint32_t junk_class = layout.junk_class_by_mod[mod];
        if (junk_class != kNoId &&
            state.junk_counts[junk_class] <
                std::numeric_limits<std::uint8_t>::max()) {
            ++state.junk_counts[junk_class];
            if (slot.flags & PC_MOD_SLOT_FRACTURED) {
                ++state.fractured_junk_counts[junk_class];
            }
            if (slot.flags & PC_MOD_SLOT_CRAFTED) {
                ++state.crafted_junk_counts[junk_class];
            }
            if ((slot.flags &
                 (PC_MOD_SLOT_FRACTURED | PC_MOD_SLOT_CRAFTED)) ==
                (PC_MOD_SLOT_FRACTURED | PC_MOD_SLOT_CRAFTED)) {
                ++state.fractured_crafted_junk_counts[junk_class];
            }
        }
    };
    for (std::uint8_t i = 0; i < item.prefix_count; ++i) {
        visit(item.prefixes[i], PC_SIDE_PREFIX);
    }
    for (std::uint8_t i = 0; i < item.suffix_count; ++i) {
        visit(item.suffixes[i], PC_SIDE_SUFFIX);
    }
    return state;
}

std::size_t abstract_state_hash(const AbstractState& state) {
    /* A 32-bit FNV-1a key is materially cheaper in wasm32. Equality remains
     * authoritative through the inline collision bucket, so hash width has
     * no semantic effect. Using the same key width on native and WASM also
     * keeps table behavior reproducible across the two runtimes. */
    std::uint32_t hash = 2166136261u;
    const auto mix = [&hash](std::uint32_t value) {
        hash ^= value;
        hash *= 16777619u;
    };
    for (std::uint8_t status : state.slot_status) mix(status);
    mix(state.fractured_goal_mask);
    mix(state.crafted_goal_mask);
    mix(state.blocked_mask);
    mix(state.prefix_count);
    mix(state.suffix_count);
    mix(state.rarity);
    mix(state.influence_bits);
    mix(static_cast<std::uint8_t>(state.veiled_side + 1));
    mix(state.searing_exarch_tier);
    mix(state.eater_of_worlds_tier);
    mix(state.flags);
    for (std::uint8_t count : state.junk_counts) mix(count);
    for (std::uint8_t count : state.fractured_junk_counts) mix(count);
    for (std::uint8_t count : state.crafted_junk_counts) mix(count);
    for (std::uint8_t count : state.fractured_crafted_junk_counts) mix(count);
    return hash;
}

bool action_legal(
    const SessionImpl& session,
    const ActionDescriptor& action,
    const AbstractState& state) {
    const LegalityPredicate& legality = action.legality;
    if ((legality.rarity_mask & (1u << state.rarity)) == 0) return false;
    if (state.flags & legality.forbidden_flags) return false;
    if ((state.flags & legality.required_flags) != legality.required_flags) {
        return false;
    }
    if (legality.requires_session_eldritch && !session.eldritch_eligible) {
        return false;
    }
    const std::uint8_t side_cap =
        state.rarity == PC_RARITY_RARE
            ? session.rare_affix_cap
            : (state.rarity == PC_RARITY_MAGIC ? std::uint8_t{1}
                                               : std::uint8_t{0});
    if (legality.requires_open_affix && state.prefix_count >= side_cap &&
        state.suffix_count >= side_cap) {
        return false;
    }
    if (legality.requires_removable_affix &&
        state.prefix_count + state.suffix_count == 0) {
        return false;
    }
    if (state.prefix_count + state.suffix_count <
        legality.min_total_affixes) {
        return false;
    }
    return true;
}

} // namespace solver
} // namespace poecraft
