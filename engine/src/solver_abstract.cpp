#include "solver_model.hpp"

#include <algorithm>
#include <bit>
#include <cstring>
#include <limits>
#include <map>
#include <optional>
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

std::uint8_t uniform_membership(
    const std::vector<std::uint64_t>& category,
    const std::vector<std::uint64_t>& observation,
    const std::vector<std::uint64_t>* excluded = nullptr) {
    bool inside = false;
    bool outside = false;
    const std::size_t words = std::min(category.size(), observation.size());
    for (std::size_t w = 0; w < words; ++w) {
        const std::uint64_t members =
            category[w] & (excluded == nullptr ? ~std::uint64_t{0}
                                                : ~(*excluded)[w]);
        inside |= (members & observation[w]) != 0;
        outside |= (members & ~observation[w]) != 0;
    }
    if (inside && outside) return 2;
    return inside ? 1 : 0;
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
 * Union of session mods a candidate action can place in, or semantically
 * require from, an explicit affix slot. Supersets are safe: an unreachable
 * mod in the universe only creates a junk class whose count is always zero.
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

    /*
     * Some actions consume a transient semantic carrier that they cannot
     * themselves create. Retain that input class from the admitted contract,
     * rather than naming the action. In particular, an Unveil-only envelope
     * must still represent an imported Veiled placeholder exactly enough to
     * invoke the primitive kernel.
     */
    const auto requires_veiled_carrier =
        [](const RefinementAffixSelector& selector) {
            return (selector.required_affix_traits &
                    kRefinementAffixVeiled) != 0;
        };
    bool retain_veiled_templates = std::any_of(
        action.refinement.preserved_affixes.begin(),
        action.refinement.preserved_affixes.end(),
        requires_veiled_carrier);
    retain_veiled_templates =
        retain_veiled_templates ||
        std::any_of(
            action.refinement.affix_observations.begin(),
            action.refinement.affix_observations.end(),
            [&](const RefinementAffixObservation& observation) {
                return requires_veiled_carrier(
                    observation.selector);
            });
    retain_veiled_templates =
        retain_veiled_templates ||
        std::any_of(
            action.refinement.destroyed_affixes.begin(),
            action.refinement.destroyed_affixes.end(),
            requires_veiled_carrier);
    retain_veiled_templates =
        retain_veiled_templates ||
        std::any_of(
            action.refinement.affix_flows.begin(),
            action.refinement.affix_flows.end(),
            [&](const RefinementAffixFlow& flow) {
                return requires_veiled_carrier(
                    flow.source_selector);
            });
    if (retain_veiled_templates) {
        if (session.veiled_template_mask.size() >= session.words) {
            mask_or_into(out, session.veiled_template_mask);
        }
        if (session.veiled_prefix_mod_id != kNoId) {
            pc_bitset_set(out.data(), session.veiled_prefix_mod_id);
        }
        if (session.veiled_suffix_mod_id != kNoId) {
            pc_bitset_set(out.data(), session.veiled_suffix_mod_id);
        }
    }
}

} // namespace

std::vector<std::uint64_t> action_explicit_affix_reachable_mask(
    const SessionImpl& session,
    const ActionDescriptor& action) {
    std::vector<std::uint64_t> result(session.words, 0);
    std::vector<std::uint64_t> scratch(session.words, 0);
    action_reachable_mask(session, action, scratch, result);
    return result;
}

AbstractLayout build_abstract_layout(
    const SessionImpl& session,
    const GoalSpec& goal,
    const ActionRegistry& registry,
    const std::vector<std::uint32_t>& action_indices,
    bool allow_empty_goal,
    bool empty_actions_mean_all,
    bool distinguish_junk_exclusion_effects,
    const std::vector<CountObservation>& count_observations,
    const std::vector<std::uint64_t>&
        required_reachable_mod_mask,
    const bool distinguish_modifier_identity,
    const AbstractLayout* refinement_parent_layout) {
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
    if (refinement_parent_layout != nullptr &&
        (refinement_parent_layout->slots.size() !=
             layout.slots.size() ||
         refinement_parent_layout->junk_class_by_mod.size() !=
             session.mod_count)) {
        invalid("refinement parent layout is incompatible");
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

    /*
     * The strict partition is derived from the same admitted semantic
     * observer vocabulary as policy refinement. Coarse layouts retain their
     * established fields and hashes; exact layouts add a dimension only when
     * some admitted action can observe it.
     */
    std::vector<std::uint32_t> tags;
    for (const std::uint32_t index : candidates) {
        if (index >= registry.actions.size()) {
            invalid("action index out of range");
        }
        const ActionDescriptor& action = registry.actions[index];
        tags.insert(
            tags.end(), action.discriminating_tag_ids.begin(),
            action.discriminating_tag_ids.end());
        if (distinguish_junk_exclusion_effects) {
            tags.insert(
                tags.end(),
                action.refinement.observed_modifier_tag_ids.begin(),
                action.refinement.observed_modifier_tag_ids.end());
        }
    }
    std::sort(tags.begin(), tags.end());
    tags.erase(std::unique(tags.begin(), tags.end()), tags.end());
    if (tags.size() > kMaxDiscriminatingTags) {
        invalid("candidate actions discriminate on " +
                std::to_string(tags.size()) +
                " tags; the packed limit is " +
                std::to_string(kMaxDiscriminatingTags));
    }
    layout.discriminating_tag_ids = std::move(tags);
    const auto restricted_tag_bits =
        [&](const std::uint32_t mod) {
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
                    tag_bits |=
                        std::uint64_t{1}
                        << (it - layout.discriminating_tag_ids.begin());
                }
            }
            return tag_bits;
        };
    const auto selector_can_match_static_mod =
        [&](const RefinementAffixSelector& selector,
            const std::uint32_t mod) {
            const std::int8_t side = session.gen_type[mod];
            std::uint16_t static_traits =
                side == PC_SIDE_PREFIX
                    ? kRefinementAffixPrefix
                    : kRefinementAffixSuffix;
            if (modifier_is_veiled_template(session, mod)) {
                static_traits |= kRefinementAffixVeiled;
            }
            constexpr std::uint16_t kStaticTraits =
                kRefinementAffixPrefix |
                kRefinementAffixSuffix |
                kRefinementAffixVeiled;
            if ((selector.required_affix_traits & kStaticTraits &
                 ~static_traits) != 0 ||
                (selector.forbidden_affix_traits &
                 static_traits) != 0) {
                return false;
            }
            for (const std::uint32_t required_tag :
                 selector.required_tag_ids) {
                bool found = false;
                for (std::uint32_t i = session.class_offsets[mod];
                     i < session.class_offsets[mod + 1]; ++i) {
                    if (session.class_tag_ids[i] == required_tag) {
                        found = true;
                        break;
                    }
                }
                if (!found) return false;
            }
            /*
             * Crafted, fractured, lock and Eldritch-dominance traits are
             * state-local. Treat them existentially here; AbstractState
             * carries their dynamic partition.
             */
            return true;
        };
    std::vector<std::uint8_t> observes_exclusion_by_mod(
        session.mod_count, 0);
    std::vector<std::uint8_t> observes_required_level_by_mod(
        session.mod_count, 0);
    if (distinguish_junk_exclusion_effects) {
        for (std::uint32_t mod = 0; mod < session.mod_count; ++mod) {
            if (session.gen_type[mod] != PC_SIDE_PREFIX &&
                session.gen_type[mod] != PC_SIDE_SUFFIX) {
                continue;
            }
            for (const std::uint32_t index : candidates) {
                for (const RefinementAffixObservation& observation :
                     registry.actions[index]
                         .refinement.affix_observations) {
                    if (!selector_can_match_static_mod(
                            observation.selector, mod)) {
                        continue;
                    }
                    observes_exclusion_by_mod[mod] |=
                        (observation.features &
                         refinement_feature(
                             RefinementFeature::
                                 ModifierExclusionSignature)) != 0;
                    observes_required_level_by_mod[mod] |=
                        (observation.features &
                         refinement_feature(
                             RefinementFeature::
                                 ModifierRequiredLevel)) != 0;
                }
            }
        }
    }

    layout.count_observations = count_observations;
    std::uint32_t max_count_memo_slot = 0;
    bool has_count_memo_slot = false;
    for (const CountObservation& observation : layout.count_observations) {
        for (const std::uint32_t slot : observation.memo_slots) {
            max_count_memo_slot = std::max(max_count_memo_slot, slot);
            has_count_memo_slot = true;
        }
    }
    if (has_count_memo_slot) {
        layout.count_observation_by_memo_slot.assign(
            static_cast<std::size_t>(max_count_memo_slot) + 1, kNoId);
    }
    for (std::size_t observation_index = 0;
         observation_index < layout.count_observations.size();
         ++observation_index) {
        CountObservation& observation =
            layout.count_observations[observation_index];
        observation.member_mask = empty_mask(session);
        observation.junk_class_indices.clear();
        for (const std::uint32_t slot : observation.memo_slots) {
            layout.count_observation_by_memo_slot[slot] =
                static_cast<std::uint32_t>(observation_index);
        }
        for (std::uint32_t mod = 0; mod < session.mod_count; ++mod) {
            if (session.gen_type[mod] != 0 && session.gen_type[mod] != 1) {
                continue;
            }
            const std::uint32_t identity = observation.by_family
                                               ? session.family_id[mod]
                                               : mod;
            if (sorted_contains(observation.ids, identity)) {
                pc_bitset_set(observation.member_mask.data(), mod);
            }
        }
    }

    /*
     * Strict layouts retain only the exact occupied-goal features that can
     * change a primitive kernel or a compiled router. The token is derived
     * from semantic effects, never from a representative modifier id.
     */
    if (distinguish_junk_exclusion_effects ||
        distinguish_modifier_identity) {
        using GoalClassKey = std::tuple<
            std::uint8_t, std::int8_t, std::uint64_t, std::uint8_t,
            std::int32_t, std::optional<std::uint32_t>,
            std::vector<std::uint64_t>,
            std::vector<std::uint64_t>,
            std::optional<std::uint32_t>,
            std::optional<std::uint32_t>>;
        for (std::size_t slot_index = 0;
             slot_index < layout.slots.size(); ++slot_index) {
            ResolvedGoalSlot& slot = layout.slots[slot_index];
            std::map<GoalClassKey, std::vector<std::uint32_t>> classes;
            pc_bitset_for_each(
                slot.member_mask.data(), session.words,
                [&](const std::size_t bit) {
                    const std::uint32_t mod =
                        static_cast<std::uint32_t>(bit);
                    const GoalSlotStatus status =
                        pc_bitset_test(slot.satisfying_mask.data(), mod)
                            ? GoalSlotStatus::Satisfied
                            : GoalSlotStatus::PresentBelowTier;
                    const std::uint8_t special_role =
                        modifier_is_veiled_template(session, mod)
                            ? (session.gen_type[mod] == PC_SIDE_PREFIX
                                   ? 1
                                   : 2)
                            : 0;
                    std::vector<std::uint64_t> exclusion_effect;
                    if (observes_exclusion_by_mod[mod]) {
                        exclusion_effect =
                            modifier_exclusion_effect_signature(
                                session, mod);
                    }
                    std::vector<std::uint64_t> observation_bits(
                        (layout.count_observations.size() + 63) / 64, 0);
                    for (std::size_t observation = 0;
                         observation < layout.count_observations.size();
                         ++observation) {
                        if (pc_bitset_test(
                                layout.count_observations[observation]
                                    .member_mask.data(),
                                mod)) {
                            observation_bits[observation / 64] |=
                                std::uint64_t{1} << (observation % 64);
                        }
                    }
                    classes[{
                        static_cast<std::uint8_t>(status),
                        session.gen_type[mod],
                        restricted_tag_bits(mod),
                        special_role,
                        session.metamod_type[mod],
                        observes_required_level_by_mod[mod]
                            ? std::optional<std::uint32_t>(
                                  session.required_level[mod])
                            : std::nullopt,
                        std::move(exclusion_effect),
                        std::move(observation_bits),
                        distinguish_modifier_identity
                            ? std::optional<std::uint32_t>(mod)
                            : std::nullopt,
                        refinement_parent_layout != nullptr
                            ? std::optional<std::uint32_t>(
                                  refinement_parent_layout->slots[slot_index]
                                          .member_class_token_by_mod.empty()
                                      ? 0
                                      : refinement_parent_layout
                                            ->slots[slot_index]
                                            .member_class_token_by_mod.at(mod))
                            : std::nullopt}]
                        .push_back(mod);
                });

            slot.member_class_token_by_mod.assign(session.mod_count, 0);
            for (const auto& [key, members] : classes) {
                GoalMemberClass member_class;
                member_class.status =
                    static_cast<GoalSlotStatus>(std::get<0>(key));
                member_class.gen_type = std::get<1>(key);
                member_class.strict_observation_metadata_complete = true;
                member_class.exclusion_effect_observation_complete =
                    std::all_of(
                        members.begin(), members.end(),
                        [&](const std::uint32_t mod) {
                            return observes_exclusion_by_mod[mod] != 0;
                        });
                member_class.required_level_observation_complete =
                    std::all_of(
                        members.begin(), members.end(),
                        [&](const std::uint32_t mod) {
                            return observes_required_level_by_mod[mod] != 0;
                        });
                member_class.classification_tag_bits = std::get<2>(key);
                member_class.veiled_template = std::get<3>(key) != 0;
                member_class.metamod_role = std::get<4>(key);
                member_class.observed_required_level =
                    std::get<5>(key).value_or(kNoId);
                member_class.exclusion_effect_mask = std::get<6>(key);
                member_class.count_observation_bits = std::get<7>(key);
                member_class.member_mask = empty_mask(session);
                const std::uint32_t token = static_cast<std::uint32_t>(
                    slot.member_classes.size() + 1);
                for (const std::uint32_t mod : members) {
                    pc_bitset_set(member_class.member_mask.data(), mod);
                    slot.member_class_token_by_mod[mod] = token;
                }
                member_class.member_count =
                    static_cast<std::uint32_t>(members.size());
                slot.member_classes.push_back(std::move(member_class));
            }
        }
    }

    for (std::size_t observation_index = 0;
         observation_index < layout.count_observations.size();
         ++observation_index) {
        CountObservation& observation =
            layout.count_observations[observation_index];
        for (std::size_t slot_index = 0;
             slot_index < layout.slots.size(); ++slot_index) {
            const ResolvedGoalSlot& slot = layout.slots[slot_index];
            const std::uint8_t below = uniform_membership(
                slot.member_mask, observation.member_mask,
                &slot.satisfying_mask);
            const std::uint8_t satisfying = uniform_membership(
                slot.satisfying_mask, observation.member_mask);
            if (!distinguish_junk_exclusion_effects &&
                (below == 2 || satisfying == 2)) {
                invalid(
                    "count observation " +
                    std::to_string(observation_index) +
                    " partially overlaps one tier-status partition of goal "
                    "slot " + std::to_string(slot_index));
            }
            observation.goal_status_counts[slot_index]
                                                  [static_cast<std::size_t>(
                                                      GoalSlotStatus::Absent)] =
                0;
            observation.goal_status_counts[slot_index]
                                                  [static_cast<std::size_t>(
                                                      GoalSlotStatus::PresentBelowTier)] =
                below == 2 ? 0 : below;
            observation.goal_status_counts[slot_index]
                                                  [static_cast<std::size_t>(
                                                      GoalSlotStatus::Satisfied)] =
                satisfying == 2 ? 0 : satisfying;
        }
    }

    /* Explicit-reachable junk universe for this action set. */
    std::vector<std::uint64_t> reachable = empty_mask(session);
    std::vector<std::uint64_t> scratch = empty_mask(session);
    for (std::uint32_t index : candidates) {
        action_reachable_mask(session, registry.actions[index], scratch,
                              reachable);
    }
    if (!required_reachable_mod_mask.empty()) {
        if (required_reachable_mod_mask.size() != session.words) {
            invalid(
                "required reachable modifier mask has the wrong word "
                "count");
        }
        mask_or_into(reachable, required_reachable_mod_mask);
    }
    /* State-local automatic candidates materialize the current carrier before
     * deciding which exact operations to admit. A narrow explicit envelope
     * may not itself roll the carrier's existing non-goal modifiers, so keep
     * every ordinary affix identity in this mode. Admission must never drop
     * or substitute an occupied group merely because its mod is unreachable
     * through the caller-selected primitive actions. */
    if (goal.automatic_candidates) {
        for (std::uint32_t mod = 0; mod < session.mod_count; ++mod) {
            if (session.gen_type[mod] == PC_SIDE_PREFIX ||
                session.gen_type[mod] == PC_SIDE_SUFFIX) {
                pc_bitset_set(reachable.data(), mod);
            }
        }
    }
    /* A strategy can begin with an observed modifier that none of its
     * operations can add. Retain those identities so count predicates remain
     * exact for the initial state and every reachable successor. */
    for (const CountObservation& observation : layout.count_observations) {
        mask_or_into(reachable, observation.member_mask);
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
        std::optional<std::int32_t>, std::optional<std::uint32_t>,
        std::vector<std::uint64_t>,
        std::vector<std::uint64_t>,
        std::optional<std::uint32_t>,
        std::optional<std::uint32_t>>;
    std::map<ClassKey, std::vector<std::uint32_t>> classes;
    std::vector<std::uint32_t> groups;
    pc_bitset_for_each(reachable.data(), session.words, [&](std::size_t bit) {
        const std::uint32_t mod = static_cast<std::uint32_t>(bit);
        const std::int8_t gen = session.gen_type[mod];
        if (gen != 0 && gen != 1) return;

        const std::uint64_t tag_bits = restricted_tag_bits(mod);

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
        if (distinguish_junk_exclusion_effects &&
            observes_exclusion_by_mod[mod]) {
            exclusion_effect =
                modifier_exclusion_effect_signature(session, mod);
        }
        /*
         * The broad carrier's Veiled placeholders are a frozen abstraction:
         * only its two authored placeholder ids split this class. Richer
         * template-mask membership belongs to an observer-driven strict
         * child and must not widen the coarse parent.
         */
        const bool strict_semantic_layout =
            distinguish_junk_exclusion_effects ||
            distinguish_modifier_identity;
        const bool veiled_special =
            strict_semantic_layout
                ? modifier_is_veiled_template(session, mod)
                : mod == session.veiled_prefix_mod_id ||
                      mod == session.veiled_suffix_mod_id;
        const std::uint8_t special_role =
            veiled_special ? (gen == PC_SIDE_PREFIX ? 1 : 2) : 0;
        std::vector<std::uint64_t> observation_bits(
            (layout.count_observations.size() + 63) / 64, 0);
        for (std::size_t observation = 0;
             observation < layout.count_observations.size(); ++observation) {
            if (pc_bitset_test(
                    layout.count_observations[observation].member_mask.data(),
                    mod)) {
                observation_bits[observation / 64] |=
                    std::uint64_t{1} << (observation % 64);
            }
        }
        const std::optional<std::uint32_t> required_level =
            distinguish_junk_exclusion_effects &&
                    observes_required_level_by_mod[mod]
                ? std::optional<std::uint32_t>(
                      session.required_level[mod])
                : std::nullopt;
        const std::optional<std::int32_t> metamod_role =
            distinguish_junk_exclusion_effects ||
                    distinguish_modifier_identity
                ? std::optional<std::int32_t>(
                      session.metamod_type[mod])
                : std::nullopt;
        classes[{gen, tag_bits, block_mask, special_role,
                 metamod_role, required_level,
                 std::move(exclusion_effect), std::move(observation_bits),
                 distinguish_modifier_identity
                     ? std::optional<std::uint32_t>(mod)
                     : std::nullopt,
                 refinement_parent_layout != nullptr
                     ? std::optional<std::uint32_t>(
                           refinement_parent_layout
                               ->junk_class_by_mod.at(mod))
                     : std::nullopt}]
            .push_back(mod);
    });

    layout.junk_class_by_mod.assign(session.mod_count, kNoId);
    for (const auto& [key, members] : classes) {
        JunkClass junk;
        junk.gen_type = std::get<0>(key);
        junk.tag_bits = std::get<1>(key);
        junk.goal_block_mask = std::get<2>(key);
        junk.strict_observation_metadata_complete =
            distinguish_junk_exclusion_effects ||
            distinguish_modifier_identity;
        junk.exclusion_effect_observation_complete =
            std::all_of(
                members.begin(), members.end(),
                [&](const std::uint32_t mod) {
                    return observes_exclusion_by_mod[mod] != 0;
                });
        junk.required_level_observation_complete =
            std::all_of(
                members.begin(), members.end(),
                [&](const std::uint32_t mod) {
                    return observes_required_level_by_mod[mod] != 0;
                });
        junk.veiled_template = std::get<3>(key) != 0;
        junk.metamod_role = std::get<4>(key).value_or(-1);
        junk.observed_required_level =
            std::get<5>(key).value_or(kNoId);
        junk.exclusion_effect_mask = std::get<6>(key);
        junk.count_observation_bits = std::get<7>(key);
        junk.member_mask = empty_mask(session);
        for (std::uint32_t mod : members) {
            pc_bitset_set(junk.member_mask.data(), mod);
            layout.junk_class_by_mod[mod] =
                static_cast<std::uint32_t>(layout.junk_classes.size());
        }
        junk.member_count = static_cast<std::uint32_t>(members.size());
        const std::uint32_t junk_index =
            static_cast<std::uint32_t>(layout.junk_classes.size());
        for (std::size_t observation = 0;
             observation < layout.count_observations.size(); ++observation) {
            if ((junk.count_observation_bits[observation / 64] &
                 (std::uint64_t{1} << (observation % 64))) != 0) {
                layout.count_observations[observation]
                    .junk_class_indices.push_back(junk_index);
            }
        }
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
        std::uint32_t metamod_flag = 0;
        if (metamod >= 0) {
            if (metamod == data.metamod_multimod_code) {
                metamod_flag = kFlagMultimod;
            } else if (metamod == data.metamod_no_attack_code) {
                metamod_flag = kFlagNoAttack;
            } else if (metamod == data.metamod_no_caster_code) {
                metamod_flag = kFlagNoCaster;
            } else if (metamod == data.metamod_prefixes_locked_code) {
                metamod_flag = kFlagPrefixesLocked;
            } else if (metamod == data.metamod_suffixes_locked_code) {
                metamod_flag = kFlagSuffixesLocked;
            }
            state.flags |= metamod_flag;
            if (slot.flags & PC_MOD_SLOT_FRACTURED) {
                state.fractured_metamod_flags |= metamod_flag;
            }
        }

        mod_groups(session, mod, groups);
        for (std::size_t s = 0; s < layout.slots.size(); ++s) {
            const ResolvedGoalSlot& goal_slot = layout.slots[s];
            const bool member =
                pc_bitset_test(goal_slot.member_mask.data(), mod);
            if (member) {
                if (!goal_slot.member_class_token_by_mod.empty()) {
                    state.goal_member_class_tokens[s] =
                        goal_slot.member_class_token_by_mod[mod];
                }
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
    if (std::any_of(
            state.goal_member_class_tokens.begin(),
            state.goal_member_class_tokens.end(),
            [](const std::uint32_t token) { return token != 0; })) {
        mix(0x67636c73u); /* "gcls": preserve every coarse-state hash. */
        for (const std::uint32_t token :
             state.goal_member_class_tokens) {
            mix(token);
        }
    }
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
    if (state.fractured_metamod_flags != 0) {
        mix(0x6d657461u); /* "meta": preserve hashes for ordinary states. */
        mix(state.fractured_metamod_flags);
    }
    if (state.goal_progress_retry_basin != 0) {
        mix(0x62617369u); /* "basi": preserve ordinary-state hashes. */
        mix(state.goal_progress_retry_basin);
    }
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
    if (!action.synthetic && action.params.type == ActionType::Scour) {
        const bool prefix_locked =
            (state.flags & kFlagPrefixesLocked) != 0;
        const bool suffix_locked =
            (state.flags & kFlagSuffixesLocked) != 0;
        const std::uint32_t before =
            static_cast<std::uint32_t>(state.prefix_count) +
            state.suffix_count;
        std::uint32_t remaining = 0;
        std::uint8_t resulting_rarity = PC_RARITY_NORMAL;
        if (prefix_locked != suffix_locked) {
            remaining = prefix_locked ? state.prefix_count
                                      : state.suffix_count;
            resulting_rarity =
                remaining > 0 ? PC_RARITY_RARE : PC_RARITY_NORMAL;
        } else {
            remaining = static_cast<std::uint32_t>(
                std::popcount(state.fractured_goal_mask));
            for (const std::uint8_t count :
                 state.fractured_junk_counts) {
                remaining += count;
            }
            resulting_rarity =
                remaining > 0 ? PC_RARITY_MAGIC : PC_RARITY_NORMAL;
        }
        /* Scour is an operation only when native execution would remove an
         * affix or change rarity. In particular, a magic carrier containing
         * only fractured affixes is a carrier-local rejection, not an
         * unsupported evaluator family and not a paid self-loop. */
        if (remaining == before && resulting_rarity == state.rarity) {
            return false;
        }
    }
    return true;
}

} // namespace solver
} // namespace poecraft
