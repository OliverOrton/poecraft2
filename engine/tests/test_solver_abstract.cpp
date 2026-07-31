#include "tests.hpp"

#include "../src/solver_internal.hpp"
#include "poecraft/bitset.h"
#include "poecraft/item_state.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <initializer_list>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace poecraft;
using namespace poecraft::solver;

namespace {

/*
 * White-box synthetic session for hand-computed junk classes.
 *
 * tags:  attack=1 caster=2 fire=3 cold=4 speed=5 resistance=6
 * mods (session ids):
 *   0 prefix life T1   groups {10}     family 100 tier 1
 *   1 prefix life T2   groups {10}     family 100 tier 2
 *   2 prefix hybrid    groups {10,11}  family 101 tier 1   (blocks family-100)
 *   3 prefix attack    groups {12}     family 102 tier 1   tags {attack}
 *   4 prefix caster    groups {13}     family 103 tier 1   tags {caster}
 *   5 suffix fire res  groups {20}     family 104 tier 1   tags {fire,res}
 *   6 suffix cold res  groups {21}     family 105 tier 1   tags {cold,res}
 *   7 suffix speed     groups {22}     family 106 tier 1   tags {speed}
 */
constexpr std::uint32_t kTagAttack = 1;
constexpr std::uint32_t kTagFire = 3;

std::shared_ptr<SessionImpl> make_solver_session() {
    auto data = std::make_shared<DataImpl>();
    data->mod_global_ids = {0, 1, 2, 3, 4, 5, 6, 7};

    /* Two real fossils plus one nameless RandomFossilOutcome-style row that
     * the registry must skip. Fossil A biases attack, fossil B biases fire. */
    data->strings = {"", "fossil_a", "Fossil A", "fossil_b", "Fossil B",
                     "fossil_x"};
    data->fossil_count = 3;
    data->fossil_key_sids = {1, 3, 5};
    data->fossil_name_sids = {2, 4, 0};
    data->fossil_weight_offsets = {0, 1, 2, 2};
    data->fossil_weight_tag_ids = {kTagAttack, kTagFire};
    data->fossil_weight_values = {1000, 0};
    data->fossil_weight_kind_codes = {0, 0};
    data->fossil_mod_offsets = {0, 0, 0, 0};

    auto session = std::make_shared<SessionImpl>();
    session->data = data;
    session->mod_count = 8;
    session->words = pc_bitset_words(8);
    session->global_index = {0, 1, 2, 3, 4, 5, 6, 7};
    session->gen_type = {0, 0, 0, 0, 0, 1, 1, 1};
    session->primary_group = {10, 10, 10, 12, 13, 20, 21, 22};
    session->required_level = {1, 1, 1, 1, 1, 1, 1, 1};
    session->group_offsets = {0, 1, 2, 4, 5, 6, 7, 8, 9};
    session->group_ids = {10, 10, 10, 11, 12, 13, 20, 21, 22};
    session->family_id = {100, 100, 101, 102, 103, 104, 105, 106};
    session->family_tier_index = {1, 2, 1, 1, 1, 1, 1, 1};
    session->metamod_type.assign(8, -1);
    session->special_kind.assign(8, -1);
    session->flags.assign(8, 0);
    session->influence_code.assign(8, -1);
    session->class_offsets = {0, 0, 0, 0, 1, 2, 4, 6, 7};
    session->class_tag_ids = {1, 2, 3, 6, 4, 6, 5};
    session->rare_affix_cap = 3;

    const std::size_t words = session->words;
    session->normal_random_roll_mask.assign(words, 0);
    session->positive_spawn_weight_mask.assign(words, 0);
    session->positive_base_weight_mask.assign(words, 0);
    session->prefix_mask.assign(words, 0);
    session->suffix_mask.assign(words, 0);
    session->unveiled_mask.assign(words, 0);
    session->implicit_tag_masks.assign(7, {});
    session->group_masks.assign(23, {});
    for (std::uint32_t mod = 0; mod < 8; ++mod) {
        pc_bitset_set(session->normal_random_roll_mask.data(), mod);
        pc_bitset_set(session->positive_spawn_weight_mask.data(), mod);
        pc_bitset_set(session->positive_base_weight_mask.data(), mod);
        pc_bitset_set((mod < 5 ? session->prefix_mask : session->suffix_mask)
                          .data(),
                      mod);
        const std::uint32_t begin = session->group_offsets[mod];
        const std::uint32_t end = session->group_offsets[mod + 1];
        for (std::uint32_t i = begin; i < end; ++i) {
            auto& mask = session->group_masks[session->group_ids[i]];
            if (mask.empty()) mask.assign(words, 0);
            pc_bitset_set(mask.data(), mod);
        }
        const std::uint32_t class_begin = session->class_offsets[mod];
        const std::uint32_t class_end = session->class_offsets[mod + 1];
        for (std::uint32_t i = class_begin; i < class_end; ++i) {
            auto& mask = session->implicit_tag_masks[
                session->class_tag_ids[i]];
            if (mask.empty()) mask.assign(words, 0);
            pc_bitset_set(mask.data(), mod);
        }
    }
    return session;
}

/* Indices of the nine basic currency actions: the hand-computed junk cases
 * pin their candidate sets explicitly so registry growth cannot shift them. */
std::vector<std::uint32_t> basic_indices(const ActionRegistry& registry) {
    std::vector<std::uint32_t> indices;
    for (const char* id : {"transmute", "augment", "alteration", "regal",
                           "alchemy", "chaos", "exalt", "annul", "scour"}) {
        indices.push_back(registry.index_by_id.at(id));
    }
    return indices;
}

GoalSpec family_goal(std::uint32_t family_id, std::uint32_t min_tier) {
    GoalSpec goal;
    GoalSlot slot;
    slot.family_id = family_id;
    slot.min_tier = min_tier;
    goal.slots.push_back(slot);
    return goal;
}

bool mask_equals(const std::vector<std::uint64_t>& mask,
                 std::initializer_list<std::uint32_t> mods) {
    std::uint64_t expected = 0;
    for (std::uint32_t mod : mods) {
        expected |= std::uint64_t{1} << mod;
    }
    return mask.size() == 1 && mask[0] == expected;
}

bool same_partition(const AbstractLayout& left,
                    const AbstractLayout& right) {
    if (left.discriminating_tag_ids != right.discriminating_tag_ids ||
        left.junk_class_by_mod != right.junk_class_by_mod ||
        left.junk_classes.size() != right.junk_classes.size() ||
        left.slots.size() != right.slots.size()) {
        return false;
    }
    for (std::size_t i = 0; i < left.junk_classes.size(); ++i) {
        const JunkClass& a = left.junk_classes[i];
        const JunkClass& b = right.junk_classes[i];
        if (a.gen_type != b.gen_type ||
            a.tag_bits != b.tag_bits ||
            a.goal_block_mask != b.goal_block_mask ||
            a.exclusion_effect_mask != b.exclusion_effect_mask ||
            a.count_observation_bits != b.count_observation_bits ||
            a.member_mask != b.member_mask ||
            a.member_count != b.member_count) {
            return false;
        }
    }
    for (std::size_t i = 0; i < left.slots.size(); ++i) {
        const ResolvedGoalSlot& a = left.slots[i];
        const ResolvedGoalSlot& b = right.slots[i];
        if (a.member_mask != b.member_mask ||
            a.satisfying_mask != b.satisfying_mask ||
            a.blocking_group_ids != b.blocking_group_ids ||
            a.member_class_token_by_mod !=
                b.member_class_token_by_mod ||
            a.member_classes.size() != b.member_classes.size()) {
            return false;
        }
        for (std::size_t c = 0; c < a.member_classes.size(); ++c) {
            const GoalMemberClass& ac = a.member_classes[c];
            const GoalMemberClass& bc = b.member_classes[c];
            if (ac.status != bc.status ||
                ac.gen_type != bc.gen_type ||
                ac.exclusion_effect_mask !=
                    bc.exclusion_effect_mask ||
                ac.count_observation_bits !=
                    bc.count_observation_bits ||
                ac.member_mask != bc.member_mask ||
                ac.member_count != bc.member_count) {
                return false;
            }
        }
    }
    return true;
}

void set_single_group(SessionImpl& session,
                      const std::uint32_t mod,
                      const std::uint32_t group) {
    const std::uint32_t begin = session.group_offsets.at(mod);
    const std::uint32_t end = session.group_offsets.at(mod + 1);
    PC_CHECK(end == begin + 1);
    if (end != begin + 1) return;
    session.primary_group.at(mod) = group;
    session.group_ids.at(begin) = group;
}

void rebuild_group_masks(SessionImpl& session) {
    const std::uint32_t largest_group =
        *std::max_element(
            session.group_ids.begin(), session.group_ids.end());
    session.group_masks.assign(
        static_cast<std::size_t>(largest_group) + 1,
        std::vector<std::uint64_t>(session.words, 0));
    for (std::uint32_t mod = 0; mod < session.mod_count; ++mod) {
        for (std::uint32_t i = session.group_offsets[mod];
             i < session.group_offsets[mod + 1]; ++i) {
            pc_bitset_set(
                session.group_masks[session.group_ids[i]].data(), mod);
        }
    }
}

void place(pc_item_state* item, int side, std::uint32_t mod_id,
           std::uint16_t group, std::uint8_t flags = 0) {
    pc_item_add_mod(item, side, mod_id, group, flags, nullptr);
}

bool item_contains_mod(
        const pc_item_state& item,
        const std::uint32_t mod_id) {
    for (std::uint8_t i = 0; i < item.prefix_count; ++i) {
        if (item.prefixes[i].mod_id == mod_id) return true;
    }
    for (std::uint8_t i = 0; i < item.suffix_count; ++i) {
        if (item.suffixes[i].mod_id == mod_id) return true;
    }
    return false;
}

void run_registry_tests(const SessionImpl& session) {
    const ActionRegistry registry = build_action_registry(session);

    /* Synthetic session: nine ordinary currencies, remove-crafted-modifiers,
     * fracture, restart,
     * and the fossil loadouts over the two *named* fossils (A, B, A+B).
     * The nameless RandomFossilOutcome-style row must not enumerate. */
    PC_CHECK(registry.actions.size() == 15);
    PC_CHECK(registry.index_by_id.size() == registry.actions.size());
    PC_CHECK(registry.index_by_id.count("chaos") == 1);
    PC_CHECK(registry.index_by_id.count("exalt") == 1);
    PC_CHECK(registry.index_by_id.count("restart") == 1);
    PC_CHECK(registry.index_by_id.count("remove_crafted_modifiers") == 1);
    PC_CHECK(registry.index_by_id.count("fossil:fossil_a") == 1);
    PC_CHECK(registry.index_by_id.count("fossil:fossil_b") == 1);
    PC_CHECK(registry.index_by_id.count("fossil:fossil_a+fossil_b") == 1);
    PC_CHECK(registry.index_by_id.count("fossil:fossil_x") == 0);
    for (const ActionDescriptor& action : registry.actions) {
        PC_CHECK(!action.cost_keys.empty());
        PC_CHECK(std::is_sorted(action.discriminating_tag_ids.begin(),
                                action.discriminating_tag_ids.end()));
        PC_CHECK(!action.uses_companion_state);
    }

    /* A combo unions its members' costs and discriminating tags. */
    const ActionDescriptor& combo =
        registry.actions[registry.index_by_id.at("fossil:fossil_a+fossil_b")];
    PC_CHECK(combo.params.fossil_indices ==
             (std::vector<std::uint32_t>{0, 1}));
    PC_CHECK(combo.cost_keys ==
             (std::vector<std::string>{"fossil:fossil_a", "fossil:fossil_b",
                                       "resonator:2"}));
    PC_CHECK(combo.discriminating_tag_ids.empty());
    PC_CHECK(combo.display_name == "Fossil A + Fossil B");

    const ActionDescriptor& restart =
        registry.actions[registry.index_by_id.at("restart")];
    PC_CHECK(restart.synthetic);
    PC_CHECK(restart.legality.forbidden_flags == 0);
    PC_CHECK(restart.cost_keys == std::vector<std::string>{"base"});
    const ActionDescriptor& remove_crafted = registry.actions[
        registry.index_by_id.at("remove_crafted_modifiers")];
    PC_CHECK(remove_crafted.cost_keys ==
             std::vector<std::string>{"scour"});
    PC_CHECK(remove_crafted.legality.required_flags == kFlagCraftedMod);
}

void run_refinement_contract_tests(const SessionImpl& session) {
    const ActionRegistry registry = build_action_registry(session);
    const ActionRegistry repeated = build_action_registry(session);
    PC_CHECK(registry.actions.size() == repeated.actions.size());
    for (const ActionDescriptor& action : registry.actions) {
        PC_CHECK(action.refinement.complete());
        bool validated = true;
        try {
            validate_action_refinement_contract(action);
        } catch (...) {
            validated = false;
        }
        PC_CHECK(validated);
        const ActionDescriptor& same =
            repeated.actions.at(repeated.index_by_id.at(action.id));
        PC_CHECK(
            action_refinement_contract_signature(action.refinement) ==
            action_refinement_contract_signature(same.refinement));
    }

    /* Complete modifier exclusion effects are deterministic and merge exact
     * ids only when they block the same future pool. */
    PC_CHECK(
        modifier_exclusion_effect_signature(session, 0) ==
        modifier_exclusion_effect_signature(session, 1));
    PC_CHECK(
        modifier_exclusion_effect_signature(session, 3) !=
        modifier_exclusion_effect_signature(session, 4));

    const auto descriptor = [&](const char* id) -> const ActionDescriptor& {
        return registry.actions.at(registry.index_by_id.at(id));
    };
    const auto observes_affix =
        [](const ActionDescriptor& action,
           const RefinementFeature feature,
           const std::uint16_t traits,
           const std::uint8_t item_traits = 0,
           const std::vector<std::uint32_t>& tags =
               std::vector<std::uint32_t>{}) {
            return refinement_contract_observes_affix(
                action.refinement, feature, traits, item_traits, tags);
        };
    const auto preserves =
        [](const ActionDescriptor& action,
           const std::uint16_t traits,
           const std::uint8_t item_traits = 0,
           const std::vector<std::uint32_t>& tags =
               std::vector<std::uint32_t>{}) {
            return refinement_selectors_match(
                action.refinement.preserved_affixes, traits, item_traits,
                tags);
        };
    const auto destroys =
        [](const ActionDescriptor& action,
           const std::uint16_t traits,
           const std::uint8_t item_traits = 0,
           const std::vector<std::uint32_t>& tags =
               std::vector<std::uint32_t>{}) {
            return refinement_selectors_match(
                action.refinement.destroyed_affixes, traits, item_traits,
                tags);
        };
    const auto has_flow =
        [](const ActionDescriptor& action,
           const std::uint16_t source_traits,
           const std::uint8_t item_traits,
           const std::uint16_t set_traits,
           const std::uint16_t cleared_traits,
           const RefinementFeature feature,
           const std::vector<std::uint32_t>& tags =
               std::vector<std::uint32_t>{}) {
            return std::any_of(
                action.refinement.affix_flows.begin(),
                action.refinement.affix_flows.end(),
                [&](const RefinementAffixFlow& flow) {
                    return refinement_selector_matches(
                               flow.source_selector,
                               source_traits, item_traits,
                               tags) &&
                           flow.set_affix_traits == set_traits &&
                           flow.cleared_affix_traits ==
                               cleared_traits &&
                           (flow.preserved_features &
                            refinement_feature(feature)) != 0;
                });
        };

    const std::uint16_t ordinary_prefix =
        kRefinementAffixPrefix;
    const std::uint16_t fractured_prefix =
        kRefinementAffixPrefix | kRefinementAffixFractured;
    const std::uint16_t crafted_prefix =
        kRefinementAffixPrefix | kRefinementAffixCrafted;
    const std::uint16_t fractured_crafted_prefix =
        crafted_prefix | kRefinementAffixFractured;
    const std::uint16_t locked_suffix =
        kRefinementAffixSuffix |
        kRefinementAffixOnLockedSide;

    /* Add-to-pool actions read the exclusion effect of every occupied
     * explicit, including exact members represented by a coarse goal slot.
     * Goal/tier/crafted/fractured/Veiled identity is preserved for downstream
     * observers but is not read by Exalt itself. */
    const ActionDescriptor& exalt = descriptor("exalt");
    PC_CHECK(observes_affix(
        exalt, RefinementFeature::ModifierExclusionSignature,
        ordinary_prefix));
    PC_CHECK(!observes_affix(
        exalt, RefinementFeature::GoalStatusTierClass,
        ordinary_prefix));
    PC_CHECK(!observes_affix(
        exalt, RefinementFeature::ModifierCrafted,
        ordinary_prefix));
    PC_CHECK(!observes_affix(
        exalt, RefinementFeature::ModifierFractured,
        fractured_prefix));
    PC_CHECK(preserves(exalt, ordinary_prefix));
    PC_CHECK(!destroys(exalt, ordinary_prefix));

    /* A lock-respecting renewal reads exclusion identity only on exact
     * carriers it preserves. Destroyed ordinary identity collapses. */
    const ActionDescriptor& chaos = descriptor("chaos");
    PC_CHECK(preserves(chaos, fractured_prefix));
    PC_CHECK(preserves(chaos, locked_suffix));
    PC_CHECK(!preserves(chaos, ordinary_prefix));
    PC_CHECK(!destroys(chaos, fractured_prefix));
    PC_CHECK(!destroys(chaos, locked_suffix));
    PC_CHECK(destroys(chaos, ordinary_prefix));
    PC_CHECK(observes_affix(
        chaos, RefinementFeature::ModifierExclusionSignature,
        fractured_prefix));
    PC_CHECK(observes_affix(
        chaos, RefinementFeature::ModifierExclusionSignature,
        locked_suffix));
    PC_CHECK(!observes_affix(
        chaos, RefinementFeature::ModifierExclusionSignature,
        ordinary_prefix));
    PC_CHECK(refinement_contract_observes_item(
        chaos.refinement, RefinementFeature::PrefixLock));
    PC_CHECK(refinement_contract_observes_item(
        chaos.refinement, RefinementFeature::CannotRollAttack));
    PC_CHECK(observes_affix(
        chaos, RefinementFeature::ModifierMetamodRole,
        fractured_prefix));
    PC_CHECK(observes_affix(
        chaos, RefinementFeature::ModifierMetamodRole,
        locked_suffix));

    /* Fossils ignore side locks. A non-fractured locked-side modifier is
     * destroyed and cannot widen the exact refinement key. */
    const ActionDescriptor& fossil = descriptor("fossil:fossil_a");
    PC_CHECK(preserves(fossil, fractured_prefix));
    PC_CHECK(!preserves(fossil, locked_suffix));
    PC_CHECK(destroys(fossil, locked_suffix));
    PC_CHECK(!observes_affix(
        fossil, RefinementFeature::ModifierExclusionSignature,
        locked_suffix));
    PC_CHECK(observes_affix(
        fossil, RefinementFeature::ModifierSide,
        fractured_prefix));
    PC_CHECK(!observes_affix(
        fossil, RefinementFeature::ModifierMetamodRole,
        fractured_prefix));

    const ActionDescriptor& scour = descriptor("scour");
    const std::uint8_t exactly_one_lock =
        kRefinementItemExactlyOneSideLocked;
    /* With neither or both sides locked, Scour preserves only fractured
     * affixes. With exactly one side locked, the lock takes authority:
     * every affix on that side survives and every affix on the opposite
     * side is removed, including fractured ones. */
    PC_CHECK(preserves(scour, fractured_prefix, 0));
    PC_CHECK(!destroys(scour, fractured_prefix, 0));
    PC_CHECK(preserves(scour, locked_suffix, exactly_one_lock));
    PC_CHECK(!destroys(scour, locked_suffix, exactly_one_lock));
    PC_CHECK(!preserves(scour, locked_suffix, 0));
    PC_CHECK(destroys(scour, locked_suffix, 0));
    PC_CHECK(!preserves(scour, fractured_prefix, exactly_one_lock));
    PC_CHECK(destroys(scour, fractured_prefix, exactly_one_lock));
    PC_CHECK(destroys(scour, ordinary_prefix, 0));
    PC_CHECK(refinement_contract_observes_item(
        scour.refinement, RefinementFeature::PrefixLock));
    PC_CHECK(refinement_contract_observes_item(
        scour.refinement, RefinementFeature::SuffixLock));
    PC_CHECK(!observes_affix(
        scour, RefinementFeature::ModifierExclusionSignature,
        fractured_prefix));
    PC_CHECK(
        (scour.refinement.destroyed_item_features &
         refinement_feature(
             RefinementFeature::HasFracturedModifier)) != 0);
    PC_CHECK(std::any_of(
        scour.refinement.item_affix_dependencies.begin(),
        scour.refinement.item_affix_dependencies.end(),
        [](const RefinementItemAffixDependency& dependency) {
            return (dependency.item_features &
                    refinement_feature(
                        RefinementFeature::Rarity)) != 0 &&
                   (dependency.survivor_affix_features &
                    refinement_feature(
                        RefinementFeature::ModifierSide)) != 0;
        }));

    const ActionDescriptor& remove_crafted =
        descriptor("remove_crafted_modifiers");
    PC_CHECK(preserves(remove_crafted, ordinary_prefix));
    PC_CHECK(!destroys(remove_crafted, ordinary_prefix));
    PC_CHECK(!preserves(remove_crafted, crafted_prefix));
    PC_CHECK(destroys(remove_crafted, crafted_prefix));
    PC_CHECK(preserves(
        remove_crafted, fractured_crafted_prefix));
    PC_CHECK(!destroys(
        remove_crafted, fractured_crafted_prefix));
    PC_CHECK(observes_affix(
        remove_crafted, RefinementFeature::ModifierCrafted,
        ordinary_prefix));
    PC_CHECK(observes_affix(
        remove_crafted, RefinementFeature::ModifierFractured,
        ordinary_prefix));
    PC_CHECK(observes_affix(
        remove_crafted, RefinementFeature::ModifierSide,
        crafted_prefix));

    const ActionDescriptor& fracture = descriptor("fracture");
    PC_CHECK(has_flow(
        fracture, ordinary_prefix, 0,
        kRefinementAffixFractured, 0,
        RefinementFeature::ModifierExclusionSignature));
    PC_CHECK(!has_flow(
        fracture, ordinary_prefix, 0,
        kRefinementAffixFractured, 0,
        RefinementFeature::ModifierFractured));

    ActionDescriptor unveil;
    unveil.id = "test:unveil";
    unveil.params.type = ActionType::Unveil;
    unveil.kind = TransitionKind::SingleSlot;
    unveil.refinement =
        derive_action_refinement_contract(session, unveil);
    validate_action_refinement_contract(unveil);
    PC_CHECK(action_observes_modifier_offer(unveil));
    PC_CHECK(has_flow(
        unveil,
        ordinary_prefix | kRefinementAffixVeiled,
        0, 0, kRefinementAffixVeiled,
        RefinementFeature::ModifierSide));
    PC_CHECK(has_flow(
        unveil,
        ordinary_prefix | kRefinementAffixVeiled,
        0, 0, kRefinementAffixVeiled,
        RefinementFeature::ModifierFractured));
    PC_CHECK(!has_flow(
        unveil,
        ordinary_prefix | kRefinementAffixVeiled,
        0, 0, kRefinementAffixVeiled,
        RefinementFeature::ModifierExclusionSignature));

    /* Applying one Eldritch implicit observes the opposite side's tier,
     * because that tier determines the resulting dominance relation. */
    ActionDescriptor ember;
    ember.id = "test:eldritch_ember";
    ember.params.type = ActionType::EldritchEmber;
    ember.kind = TransitionKind::Deterministic;
    ember.refinement =
        derive_action_refinement_contract(session, ember);
    validate_action_refinement_contract(ember);
    PC_CHECK(refinement_contract_observes_item(
        ember.refinement, RefinementFeature::EaterOfWorldsTier));

    ActionDescriptor ichor;
    ichor.id = "test:eldritch_ichor";
    ichor.params.type = ActionType::EldritchIchor;
    ichor.kind = TransitionKind::Deterministic;
    ichor.refinement =
        derive_action_refinement_contract(session, ichor);
    validate_action_refinement_contract(ichor);
    PC_CHECK(refinement_contract_observes_item(
        ichor.refinement, RefinementFeature::SearingExarchTier));

    const ActionDescriptor& restart = descriptor("restart");
    PC_CHECK(restart.refinement.resets_to_fresh_item);
    PC_CHECK(restart.refinement.observed_item_features == 0);
    PC_CHECK(restart.refinement.preserved_item_features == 0);
    PC_CHECK(
        restart.refinement.destroyed_item_features ==
        kAllRefinementItemFeatures);
    PC_CHECK(restart.refinement.affix_observations.empty());
    PC_CHECK(!preserves(restart, ordinary_prefix));
    PC_CHECK(destroys(restart, ordinary_prefix));

    /* Eldritch Chaos resolves its conditional side semantics through generic
     * affix/item traits. The future refinement algorithm need not know the
     * action name. */
    ActionDescriptor eldritch;
    eldritch.id = "test:eldritch_chaos";
    eldritch.params.type = ActionType::EldritchChaos;
    eldritch.kind = TransitionKind::Reforge;
    eldritch.legality.rarity_mask = 1u << PC_RARITY_RARE;
    eldritch.refinement =
        derive_action_refinement_contract(session, eldritch);
    validate_action_refinement_contract(eldritch);
    const std::uint8_t dominance =
        kRefinementItemHasEldritchDominance;
    const std::uint16_t dominant_prefix =
        kRefinementAffixPrefix |
        kRefinementAffixOnEldritchDominantSide;
    const std::uint16_t nondominant_suffix =
        kRefinementAffixSuffix |
        kRefinementAffixOnEldritchNonDominantSide;
    PC_CHECK(preserves(eldritch, nondominant_suffix, dominance));
    PC_CHECK(!destroys(eldritch, nondominant_suffix, dominance));
    PC_CHECK(!preserves(eldritch, dominant_prefix, dominance));
    PC_CHECK(destroys(eldritch, dominant_prefix, dominance));
    PC_CHECK(preserves(
        eldritch,
        dominant_prefix | kRefinementAffixFractured,
        dominance));
    PC_CHECK(preserves(eldritch, locked_suffix, 0));
    PC_CHECK(!preserves(eldritch, ordinary_prefix, 0));
    PC_CHECK(observes_affix(
        eldritch, RefinementFeature::ModifierExclusionSignature,
        nondominant_suffix, dominance));
    PC_CHECK(!observes_affix(
        eldritch, RefinementFeature::ModifierExclusionSignature,
        dominant_prefix, dominance));

    /* Resistance conversion observes only the declared source tag,
     * required-level class, side/locks/fracture, and survivor exclusions. */
    ActionDescriptor resist;
    resist.id = "test:harvest_resist";
    resist.params.type = ActionType::HarvestResist;
    resist.params.source_tag_id = kTagFire;
    resist.params.target_tag_id = 4;
    resist.kind = TransitionKind::SingleSlot;
    resist.legality.rarity_mask =
        (1u << PC_RARITY_MAGIC) | (1u << PC_RARITY_RARE);
    resist.refinement =
        derive_action_refinement_contract(session, resist);
    validate_action_refinement_contract(resist);
    PC_CHECK(std::find(
        resist.refinement.observed_modifier_tag_ids.begin(),
        resist.refinement.observed_modifier_tag_ids.end(),
        kTagFire) !=
        resist.refinement.observed_modifier_tag_ids.end());
    PC_CHECK(observes_affix(
        resist, RefinementFeature::ModifierRequiredLevel,
        ordinary_prefix, 0, {kTagFire}));
    PC_CHECK(!observes_affix(
        resist, RefinementFeature::ModifierRequiredLevel,
        ordinary_prefix, 0, {4}));
    PC_CHECK(observes_affix(
        resist, RefinementFeature::ModifierExclusionSignature,
        ordinary_prefix));
    PC_CHECK(has_flow(
        resist,
        ordinary_prefix, 0, 0,
        kRefinementAffixCrafted |
            kRefinementAffixFractured |
            kRefinementAffixVeiled,
        RefinementFeature::ModifierRequiredLevel,
        {kTagFire, 6}));
    PC_CHECK(!has_flow(
        resist,
        ordinary_prefix, 0, 0,
        kRefinementAffixCrafted |
            kRefinementAffixFractured |
            kRefinementAffixVeiled,
        RefinementFeature::ModifierExclusionSignature,
        {kTagFire, 6}));

    /* An engine action cannot enter a registry with an omitted contract. */
    ActionDescriptor future;
    future.id = "test:future_action";
    future.params.type = static_cast<ActionType>(999);
    future.refinement =
        derive_action_refinement_contract(session, future);
    PC_CHECK(!future.refinement.complete());
    bool rejected = false;
    try {
        validate_action_refinement_contract(future);
    } catch (const std::logic_error&) {
        rejected = true;
    }
    PC_CHECK(rejected);

    /* A complete schema is not enough: every valid occupied source trait
     * combination must have a survivor flow or explicit destruction. */
    ActionDescriptor gap;
    gap.id = "test:incomplete_affix_domain";
    gap.refinement.schema_version =
        kActionRefinementContractVersion;
    gap.refinement.preserved_item_features =
        kAllRefinementItemFeatures;
    RefinementAffixFlow only_plain_prefix;
    only_plain_prefix.source_selector.required_affix_traits =
        kRefinementAffixPrefix;
    only_plain_prefix.source_selector.forbidden_affix_traits =
        kRefinementAffixCrafted;
    only_plain_prefix.preserved_features =
        kAllRefinementAffixFeatures;
    gap.refinement.affix_flows.push_back(
        only_plain_prefix);
    RefinementAffixSelector suffix;
    suffix.required_affix_traits =
        kRefinementAffixSuffix;
    gap.refinement.destroyed_affixes.push_back(
        std::move(suffix));
    rejected = false;
    try {
        validate_action_refinement_contract(gap);
    } catch (const std::logic_error&) {
        rejected = true;
    }
    PC_CHECK(rejected);

    /* Observation declarations and exact calculator capability are admitted
     * together. Copying a valid offer contract onto a deterministic mechanic
     * is rejected before planner construction. */
    ActionDescriptor mismatched_offer = descriptor("unveil");
    mismatched_offer.id = "test:mismatched_modifier_offer";
    mismatched_offer.params.type = ActionType::Bench;
    rejected = false;
    try {
        canonicalize_and_validate_action_refinement_contract(
            session, mismatched_offer);
    } catch (const std::logic_error&) {
        rejected = true;
    }
    PC_CHECK(rejected);

    /* Authored and internal contracts use the same admission path. Equivalent
     * compatibility summaries are canonicalized once and receive an explicit
     * identity flow before any runtime consumer sees them. */
    ActionDescriptor canonical_identity;
    canonical_identity.id = "test:canonical_identity";
    canonical_identity.refinement.schema_version =
        kActionRefinementContractVersion;
    canonical_identity.refinement.preserved_item_features =
        kAllRefinementItemFeatures;
    canonical_identity.refinement.preserved_affixes = {{}, {}};
    canonicalize_and_validate_action_refinement_contract(
        canonical_identity);
    PC_CHECK(
        canonical_identity.refinement.preserved_affixes.size() == 1);
    PC_CHECK(canonical_identity.refinement.affix_flows.size() == 1);
    PC_CHECK(
        canonical_identity.refinement.affix_flows.front()
                .preserved_features ==
            kAllRefinementAffixFeatures);

    /* A supported schema number cannot conceal unknown selector vocabulary. */
    ActionDescriptor unknown_trait = canonical_identity;
    unknown_trait.id = "test:unknown_refinement_trait";
    unknown_trait.refinement.affix_flows.front()
        .source_selector.required_affix_traits = 1u << 15;
    rejected = false;
    try {
        canonicalize_and_validate_action_refinement_contract(
            unknown_trait);
    } catch (const std::logic_error&) {
        rejected = true;
    }
    PC_CHECK(rejected);

    ActionDescriptor unknown_tag = canonical_identity;
    unknown_tag.id = "test:unknown_refinement_tag";
    unknown_tag.refinement.observed_modifier_tag_ids.push_back(
        static_cast<std::uint32_t>(session.implicit_tag_masks.size()));
    unknown_tag.refinement.affix_observations.push_back({
        refinement_feature(
            RefinementFeature::ModifierClassificationTags),
        {}});
    rejected = false;
    try {
        canonicalize_and_validate_action_refinement_contract(
            session, unknown_tag);
    } catch (const std::logic_error&) {
        rejected = true;
    }
    PC_CHECK(rejected);

    /* Flow preservation claims are semantic equalities, so admission rejects
     * a transform that rewrites one of the values it claims to preserve. */
    ActionDescriptor contradictory_flow;
    contradictory_flow.id = "test:contradictory_affix_flow";
    contradictory_flow.refinement.schema_version =
        kActionRefinementContractVersion;
    contradictory_flow.refinement.preserved_item_features =
        kAllRefinementItemFeatures;
    RefinementAffixFlow fractures_every_affix;
    fractures_every_affix.set_affix_traits =
        kRefinementAffixFractured;
    fractures_every_affix.preserved_features =
        kAllRefinementAffixFeatures;
    contradictory_flow.refinement.affix_flows.push_back(
        fractures_every_affix);
    contradictory_flow.refinement.preserved_affixes.push_back({});
    rejected = false;
    try {
        canonicalize_and_validate_action_refinement_contract(
            contradictory_flow);
    } catch (const std::logic_error&) {
        rejected = true;
    }
    PC_CHECK(rejected);

    /* Fresh reset is a complete destruction contract, not an overlapping
     * preserved/destroyed item mask with a reset flag attached. */
    ActionDescriptor contradictory_reset;
    contradictory_reset.id = "test:contradictory_fresh_reset";
    contradictory_reset.refinement.schema_version =
        kActionRefinementContractVersion;
    contradictory_reset.refinement.preserved_item_features =
        kAllRefinementItemFeatures;
    contradictory_reset.refinement.destroyed_item_features =
        kAllRefinementItemFeatures;
    contradictory_reset.refinement.resets_to_fresh_item = true;
    contradictory_reset.refinement.destroyed_affixes.push_back({});
    rejected = false;
    try {
        canonicalize_and_validate_action_refinement_contract(
            contradictory_reset);
    } catch (const std::logic_error&) {
        rejected = true;
    }
    PC_CHECK(rejected);
}

void run_exact_goal_member_class_tests() {
    const auto session = make_solver_session();
    session->family_id[0] = 999;
    session->family_id[1] = 999;
    session->family_id[3] = 999;
    session->family_id[4] = 999;
    const ActionRegistry registry = build_action_registry(*session);
    GoalSpec goal;
    GoalSlot slot;
    slot.family_id = 999;
    slot.min_tier = 0;
    goal.slots.push_back(slot);

    const AbstractLayout coarse = build_abstract_layout(
        *session, goal, registry, basic_indices(registry));
    const AbstractLayout exact = build_abstract_layout(
        *session, goal, registry, basic_indices(registry),
        false, true, true);
    PC_CHECK(coarse.slots[0].member_classes.empty());
    PC_CHECK(coarse.slots[0].member_class_token_by_mod.empty());
    PC_CHECK(exact.slots[0].member_classes.size() == 3);
    PC_CHECK(
        exact.slots[0].member_class_token_by_mod[0] ==
        exact.slots[0].member_class_token_by_mod[1]);
    PC_CHECK(
        exact.slots[0].member_class_token_by_mod[3] !=
        exact.slots[0].member_class_token_by_mod[4]);

    const auto project_mod = [&](const AbstractLayout& layout,
                                 const std::uint32_t mod) {
        pc_item_state item;
        pc_item_clear(&item);
        item.rarity = PC_RARITY_MAGIC;
        place(
            &item, PC_SIDE_PREFIX, mod,
            static_cast<std::uint16_t>(session->primary_group[mod]));
        return project_item(*session, layout, item);
    };
    const AbstractState coarse_zero = project_mod(coarse, 0);
    const AbstractState coarse_one = project_mod(coarse, 1);
    PC_CHECK(coarse_zero.goal_member_class_tokens[0] == 0);
    PC_CHECK(coarse_one.goal_member_class_tokens[0] == 0);
    PC_CHECK(coarse_zero == coarse_one);
    PC_CHECK(
        abstract_state_hash(coarse_zero) ==
        abstract_state_hash(coarse_one));

    const AbstractState exact_zero = project_mod(exact, 0);
    const AbstractState exact_one = project_mod(exact, 1);
    const AbstractState exact_three = project_mod(exact, 3);
    const AbstractState exact_four = project_mod(exact, 4);
    PC_CHECK(exact_zero.goal_member_class_tokens[0] != 0);
    PC_CHECK(exact_zero == exact_one);
    PC_CHECK(!(exact_three == exact_four));
    PC_CHECK(
        abstract_state_hash(exact_three) !=
        abstract_state_hash(exact_four));

    /* A compiled exact-id count observation may split otherwise equivalent
     * exclusion classes, and no representative goal identity is selected. */
    CountObservation observes_zero;
    observes_zero.ids = {0};
    const AbstractLayout routed = build_abstract_layout(
        *session, goal, registry, basic_indices(registry),
        false, true, true, {observes_zero});
    PC_CHECK(routed.slots[0].member_classes.size() == 4);
    PC_CHECK(
        routed.slots[0].member_class_token_by_mod[0] !=
        routed.slots[0].member_class_token_by_mod[1]);
}

void run_selector_conditioned_strict_partition_tests() {
    /*
     * A tag named only by the semantic refinement contract must not widen
     * the product/coarse carrier. The same admitted observer does split the
     * strict carrier because its future action can distinguish the mods.
     */
    {
        auto session = make_solver_session();
        set_single_group(*session, 4, 12);
        rebuild_group_masks(*session);
        const ActionRegistry built = build_action_registry(*session);
        ActionDescriptor baseline =
            built.actions[built.index_by_id.at("exalt")];
        baseline.id = "test:baseline_observer";
        baseline.discriminating_tag_ids.clear();

        ActionRegistry baseline_registry;
        baseline_registry.index_by_id.emplace(baseline.id, 0);
        baseline_registry.actions.push_back(baseline);

        ActionDescriptor tagged = baseline;
        tagged.id = "test:contract_tag_observer";
        tagged.refinement.observed_modifier_tag_ids = {kTagAttack};
        PC_CHECK(tagged.refinement.affix_observations.size() == 1);
        tagged.refinement.affix_observations.front().features |=
            refinement_feature(
                RefinementFeature::ModifierClassificationTags);
        validate_action_refinement_contract(tagged);
        PC_CHECK(tagged.discriminating_tag_ids.empty());

        ActionRegistry tagged_registry;
        tagged_registry.index_by_id.emplace(tagged.id, 0);
        tagged_registry.actions.push_back(tagged);

        const GoalSpec goal = family_goal(100, 0);
        const AbstractLayout baseline_coarse = build_abstract_layout(
            *session, goal, baseline_registry, {0});
        const AbstractLayout tagged_coarse = build_abstract_layout(
            *session, goal, tagged_registry, {0});
        PC_CHECK(same_partition(baseline_coarse, tagged_coarse));
        PC_CHECK(tagged_coarse.discriminating_tag_ids.empty());
        PC_CHECK(
            tagged_coarse.junk_class_by_mod[3] ==
            tagged_coarse.junk_class_by_mod[4]);

        const auto project_mod =
            [&](const AbstractLayout& layout,
                const std::uint32_t mod) {
                pc_item_state item;
                pc_item_clear(&item);
                item.rarity = PC_RARITY_MAGIC;
                place(
                    &item, PC_SIDE_PREFIX, mod,
                    static_cast<std::uint16_t>(
                        session->primary_group[mod]));
                return project_item(*session, layout, item);
            };
        const AbstractState baseline_attack =
            project_mod(baseline_coarse, 3);
        const AbstractState tagged_attack =
            project_mod(tagged_coarse, 3);
        const AbstractState tagged_caster =
            project_mod(tagged_coarse, 4);
        PC_CHECK(baseline_attack == tagged_attack);
        PC_CHECK(tagged_attack == tagged_caster);
        PC_CHECK(
            abstract_state_hash(baseline_attack) ==
            abstract_state_hash(tagged_attack));
        PC_CHECK(
            abstract_state_hash(tagged_attack) ==
            abstract_state_hash(tagged_caster));

        const AbstractLayout tagged_strict = build_abstract_layout(
            *session, goal, tagged_registry, {0},
            false, true, true);
        PC_CHECK(
            tagged_strict.discriminating_tag_ids ==
            std::vector<std::uint32_t>{kTagAttack});
        PC_CHECK(
            tagged_strict.junk_class_by_mod[3] !=
            tagged_strict.junk_class_by_mod[4]);
    }

    /*
     * Harvest resistance conversion declares required-level observation
     * only for affixes carrying its source tag. Equal-effect source affixes
     * therefore split by level; non-source affixes stay merged even when
     * their levels differ.
     */
    {
        auto session = make_solver_session();
        constexpr std::uint32_t kTagResistance = 6;
        set_single_group(*session, 4, 12);
        set_single_group(*session, 6, 20);
        rebuild_group_masks(*session);
        session->required_level[3] = 31;
        session->required_level[4] = 47;
        session->required_level[5] = 55;
        session->required_level[6] = 72;

        const ActionRegistry built = build_action_registry(*session);
        ActionDescriptor chaos =
            built.actions[built.index_by_id.at("chaos")];
        chaos.id = "test:strict_chaos";
        ActionDescriptor resist;
        resist.id = "test:strict_harvest_resist";
        resist.params.type = ActionType::HarvestResist;
        resist.params.source_tag_id = kTagResistance;
        resist.params.target_tag_id = kTagFire;
        resist.kind = TransitionKind::SingleSlot;
        resist.cost_keys = {"test:strict_harvest_resist"};
        resist.legality.rarity_mask =
            (1u << PC_RARITY_MAGIC) | (1u << PC_RARITY_RARE);
        resist.refinement =
            derive_action_refinement_contract(*session, resist);
        validate_action_refinement_contract(resist);

        ActionRegistry custom;
        custom.index_by_id.emplace(chaos.id, 0);
        custom.actions.push_back(chaos);
        custom.index_by_id.emplace(resist.id, 1);
        custom.actions.push_back(resist);

        const GoalSpec goal = family_goal(100, 1);
        const AbstractLayout strict = build_abstract_layout(
            *session, goal, custom, {0, 1},
            false, true, true);
        PC_CHECK(
            strict.discriminating_tag_ids ==
            std::vector<std::uint32_t>{kTagResistance});
        PC_CHECK(
            strict.junk_class_by_mod[5] !=
            strict.junk_class_by_mod[6]);
        PC_CHECK(
            strict.junk_class_by_mod[3] ==
            strict.junk_class_by_mod[4]);

        const AbstractLayout repeated = build_abstract_layout(
            *session, goal, custom, {0, 1},
            false, true, true);
        const AbstractLayout reversed = build_abstract_layout(
            *session, goal, custom, {1, 0},
            false, true, true);
        PC_CHECK(same_partition(strict, repeated));
        PC_CHECK(same_partition(strict, reversed));

        /* Representative materialization may pick either member of a merged
         * class, but projection must recover the exact strict class token. */
        CalcContext calc(
            session, goal, custom, {0, 1},
            false, true, true);
        pc_item_state item;
        pc_item_clear(&item);
        item.rarity = PC_RARITY_RARE;
        place(&item, PC_SIDE_PREFIX, 0, 10);
        place(&item, PC_SIDE_PREFIX, 3, 12);
        place(&item, PC_SIDE_SUFFIX, 5, 20);
        const std::uint32_t state_id = calc.intern_item(item);
        const AbstractState before = calc.state(state_id);
        pc_item_state materialized;
        PC_CHECK(calc.materialize(state_id, materialized));
        const AbstractState after =
            project_item(*session, calc.layout(), materialized);
        PC_CHECK(before == after);
        PC_CHECK(abstract_state_hash(before) ==
                 abstract_state_hash(after));
        PC_CHECK(calc.intern_item(materialized) == state_id);
    }

    /*
     * Static Veiled-template recognition is mask-authoritative. A template
     * outside the two convenience carrier ids must join the same strict
     * observation class as an otherwise equivalent named template.
     */
    {
        auto session = make_solver_session();
        set_single_group(*session, 4, 12);
        rebuild_group_masks(*session);
        session->family_id[3] = 999;
        session->family_id[4] = 999;
        session->veiled_prefix_mod_id = 3;
        session->veiled_suffix_mod_id = 5;
        session->veiled_template_mask.assign(session->words, 0);
        pc_bitset_set(session->veiled_template_mask.data(), 4);
        PC_CHECK(modifier_is_veiled_template(*session, 3));
        PC_CHECK(modifier_is_veiled_template(*session, 4));
        PC_CHECK(modifier_is_veiled_template(*session, 5));
        PC_CHECK(!modifier_is_veiled_template(*session, 7));

        const ActionRegistry built = build_action_registry(*session);
        const std::vector<std::uint64_t> unveil_carriers =
            action_explicit_affix_reachable_mask(
                *session,
                built.actions.at(
                    built.index_by_id.at("unveil")));
        PC_CHECK(pc_bitset_test(unveil_carriers.data(), 3));
        PC_CHECK(pc_bitset_test(unveil_carriers.data(), 4));
        PC_CHECK(pc_bitset_test(unveil_carriers.data(), 5));
        ActionDescriptor observes_veiled =
            built.actions[built.index_by_id.at("exalt")];
        observes_veiled.id = "test:veiled_observer";
        PC_CHECK(
            observes_veiled.refinement.affix_observations.size() == 1);
        RefinementAffixObservation veiled_observation;
        veiled_observation.features =
            refinement_feature(RefinementFeature::ModifierVeiled);
        veiled_observation.selector.required_affix_traits =
            kRefinementAffixVeiled;
        observes_veiled.refinement.affix_observations.push_back(
            std::move(veiled_observation));
        validate_action_refinement_contract(observes_veiled);
        const std::vector<std::uint64_t> observed_carriers =
            action_explicit_affix_reachable_mask(
                *session, observes_veiled);
        PC_CHECK(pc_bitset_test(observed_carriers.data(), 3));
        PC_CHECK(pc_bitset_test(observed_carriers.data(), 4));
        PC_CHECK(pc_bitset_test(observed_carriers.data(), 5));
        ActionRegistry custom;
        custom.index_by_id.emplace(observes_veiled.id, 0);
        custom.actions.push_back(observes_veiled);

        const AbstractLayout strict = build_abstract_layout(
            *session, family_goal(999, 0), custom, {0},
            false, true, true);
        PC_CHECK(strict.slots[0].member_classes.size() == 1);
        PC_CHECK(
            strict.slots[0].member_class_token_by_mod[3] != 0);
        PC_CHECK(
            strict.slots[0].member_class_token_by_mod[3] ==
            strict.slots[0].member_class_token_by_mod[4]);
    }

    /*
     * Metamod role is strict carrier semantics even when two literal mods
     * otherwise have the same side, tags, goal blocking and complete
     * exclusion effect. The qualified coarse parent deliberately remains
     * unchanged.
     */
    {
        auto session = make_solver_session();
        set_single_group(*session, 4, 12);
        rebuild_group_masks(*session);
        session->metamod_type[3] = 17;
        const ActionRegistry registry =
            build_action_registry(*session);
        const GoalSpec goal = family_goal(100, 0);
        const std::vector<std::uint32_t> actions =
            basic_indices(registry);
        const AbstractLayout coarse = build_abstract_layout(
            *session, goal, registry, actions);
        const AbstractLayout strict = build_abstract_layout(
            *session, goal, registry, actions,
            false, true, true);
        PC_CHECK(
            coarse.junk_class_by_mod[3] ==
            coarse.junk_class_by_mod[4]);
        PC_CHECK(
            strict.junk_class_by_mod[3] !=
            strict.junk_class_by_mod[4]);
    }

    /*
     * Policy refinement and exact strategy evaluation keep concrete
     * modifier identities only until the shared kernel partition proves
     * them equivalent. This local proof layout must therefore have one
     * carrier per concrete mod even when the ordinary strict semantic
     * layout deliberately merges equal exclusion effects.
     */
    {
        auto session = make_solver_session();
        set_single_group(*session, 4, 12);
        rebuild_group_masks(*session);
        const ActionRegistry registry =
            build_action_registry(*session);
        const GoalSpec goal = family_goal(100, 0);
        const std::vector<std::uint32_t> actions =
            basic_indices(registry);
        const AbstractLayout semantic = build_abstract_layout(
            *session, goal, registry, actions,
            false, true, true);
        const AbstractLayout concrete = build_abstract_layout(
            *session, goal, registry, actions,
            false, true, true, {}, {}, true);
        PC_CHECK(
            semantic.junk_class_by_mod[3] ==
            semantic.junk_class_by_mod[4]);
        PC_CHECK(
            concrete.junk_class_by_mod[3] !=
            concrete.junk_class_by_mod[4]);
        PC_CHECK(
            semantic.slots[0].member_class_token_by_mod[0] ==
            semantic.slots[0].member_class_token_by_mod[1]);
        PC_CHECK(
            concrete.slots[0].member_class_token_by_mod[0] !=
            concrete.slots[0].member_class_token_by_mod[1]);
        for (const JunkClass& junk : concrete.junk_classes) {
            PC_CHECK(junk.member_count == 1);
        }
        for (const GoalMemberClass& member :
             concrete.slots[0].member_classes) {
            PC_CHECK(member.member_count == 1);
        }

        CalcContext calc(
            session, goal, registry, actions,
            false, true, true, std::nullopt, {}, false, {}, true);
        pc_item_state authored;
        pc_item_clear(&authored);
        authored.rarity = PC_RARITY_RARE;
        place(&authored, PC_SIDE_PREFIX, 1, 10);
        place(&authored, PC_SIDE_PREFIX, 4, 12);
        place(&authored, PC_SIDE_SUFFIX, 6, 21);
        const std::uint32_t state = calc.intern_item(authored);
        pc_item_state materialized;
        PC_CHECK(calc.materialize(state, materialized));
        PC_CHECK(item_contains_mod(materialized, 1));
        PC_CHECK(item_contains_mod(materialized, 4));
        PC_CHECK(item_contains_mod(materialized, 6));
        PC_CHECK(calc.intern_item(materialized) == state);
    }
}

void run_junk_class_tests(const std::shared_ptr<SessionImpl>& session) {
    const ActionRegistry registry = build_action_registry(*session);

    /* Case A: basic-currency set only, family-100 goal at tier 1. No action
     * discriminates on tags, so junk collapses to (side, blocks-goal):
     *   class 0: prefix, non-blocking      -> mods {3,4}
     *   class 1: prefix, blocks slot 0     -> mod  {2}
     *   class 2: suffix, non-blocking      -> mods {5,6,7}
     * This is the plan's rule that under chaos/alt/annul junk is fully
     * described by per-side counts (plus goal blocking). */
    {
        const AbstractLayout layout = build_abstract_layout(
            *session, family_goal(100, 1), registry,
            basic_indices(registry));
        PC_CHECK(layout.discriminating_tag_ids.empty());
        PC_CHECK(layout.junk_classes.size() == 3);
        if (layout.junk_classes.size() == 3) {
            PC_CHECK(layout.junk_classes[0].gen_type == 0);
            PC_CHECK(layout.junk_classes[0].goal_block_mask == 0);
            PC_CHECK(mask_equals(layout.junk_classes[0].member_mask, {3, 4}));
            PC_CHECK(layout.junk_classes[1].gen_type == 0);
            PC_CHECK(layout.junk_classes[1].goal_block_mask == 1);
            PC_CHECK(mask_equals(layout.junk_classes[1].member_mask, {2}));
            PC_CHECK(layout.junk_classes[2].gen_type == 1);
            PC_CHECK(layout.junk_classes[2].goal_block_mask == 0);
            PC_CHECK(
                mask_equals(layout.junk_classes[2].member_mask, {5, 6, 7}));
        }
        PC_CHECK(layout.slots.size() == 1);
        PC_CHECK(mask_equals(layout.slots[0].member_mask, {0, 1}));
        PC_CHECK(mask_equals(layout.slots[0].satisfying_mask, {0}));
        PC_CHECK(layout.junk_class_by_mod[0] == kNoId);
        PC_CHECK(layout.junk_class_by_mod[1] == kNoId);
        PC_CHECK(layout.junk_class_by_mod[2] == 1);
        PC_CHECK(layout.junk_class_by_mod[3] == 0);
        PC_CHECK(layout.junk_class_by_mod[7] == 2);
    }

    /* Case B: adding a fossil-like action that discriminates on the attack
     * tag splits the prefix junk class in two; suffixes stay collapsed. */
    {
        ActionRegistry custom;
        custom.actions.push_back(
            registry.actions[registry.index_by_id.at("chaos")]);
        ActionDescriptor fossil;
        fossil.id = "fossil:test";
        fossil.params.type = ActionType::Fossil;
        fossil.kind = TransitionKind::Reforge;
        fossil.cost_keys = {"fossil:test", "resonator:1"};
        fossil.discriminating_tag_ids = {kTagAttack};
        fossil.refinement =
            derive_action_refinement_contract(*session, fossil);
        validate_action_refinement_contract(fossil);
        custom.actions.push_back(fossil);

        const AbstractLayout layout = build_abstract_layout(
            *session, family_goal(100, 1), custom, {});
        PC_CHECK(layout.discriminating_tag_ids ==
                 std::vector<std::uint32_t>{kTagAttack});
        PC_CHECK(layout.junk_classes.size() == 4);
        if (layout.junk_classes.size() == 4) {
            /* (gen, tag_bits, block): (0,0,0)={4} (0,0,1)={2}
             * (0,1,0)={3} (1,0,0)={5,6,7} */
            PC_CHECK(mask_equals(layout.junk_classes[0].member_mask, {4}));
            PC_CHECK(mask_equals(layout.junk_classes[1].member_mask, {2}));
            PC_CHECK(mask_equals(layout.junk_classes[2].member_mask, {3}));
            PC_CHECK(layout.junk_classes[2].tag_bits == 1);
            PC_CHECK(
                mask_equals(layout.junk_classes[3].member_mask, {5, 6, 7}));
        }
    }

    /* Case C: a Harvest fire reforge discriminates on the fire tag, so the
     * fire-res suffix separates from the other suffix junk. */
    {
        ActionRegistry custom;
        custom.actions.push_back(
            registry.actions[registry.index_by_id.at("chaos")]);
        ActionDescriptor harvest;
        harvest.id = "harvest_reforge:fire";
        harvest.params.type = ActionType::HarvestReforge;
        harvest.params.target_tag_id = kTagFire;
        harvest.kind = TransitionKind::Reforge;
        harvest.cost_keys = {harvest.id};
        harvest.discriminating_tag_ids = {kTagFire};
        harvest.refinement =
            derive_action_refinement_contract(*session, harvest);
        validate_action_refinement_contract(harvest);
        custom.actions.push_back(harvest);

        const AbstractLayout layout = build_abstract_layout(
            *session, family_goal(100, 1), custom, {});
        PC_CHECK(layout.junk_classes.size() == 4);
        if (layout.junk_classes.size() == 4) {
            /* (0,0,0)={3,4} (0,0,1)={2} (1,0,0)={6,7} (1,1,0)={5} */
            PC_CHECK(mask_equals(layout.junk_classes[0].member_mask, {3, 4}));
            PC_CHECK(mask_equals(layout.junk_classes[1].member_mask, {2}));
            PC_CHECK(mask_equals(layout.junk_classes[3].member_mask, {5}));
            PC_CHECK(layout.junk_classes[3].tag_bits == 1);
        }
    }

    /* Case D: group-identified goal slot. Group 10 members are {0,1,2};
     * the tier-1 members {0,2} satisfy. Nothing outside the group carries
     * groups 10/11, so no blocking junk class exists. */
    {
        GoalSpec goal;
        GoalSlot slot;
        slot.group_id = 10;
        slot.min_tier = 1;
        goal.slots.push_back(slot);
        const AbstractLayout layout = build_abstract_layout(
            *session, goal, registry, basic_indices(registry));
        PC_CHECK(mask_equals(layout.slots[0].member_mask, {0, 1, 2}));
        PC_CHECK(mask_equals(layout.slots[0].satisfying_mask, {0, 2}));
        PC_CHECK(layout.junk_classes.size() == 2);
        if (layout.junk_classes.size() == 2) {
            PC_CHECK(mask_equals(layout.junk_classes[0].member_mask, {3, 4}));
            PC_CHECK(
                mask_equals(layout.junk_classes[1].member_mask, {5, 6, 7}));
        }
    }

    /* Invalid goals must throw. */
    {
        bool threw = false;
        try {
            build_abstract_layout(*session, GoalSpec{}, registry, {});
        } catch (const std::runtime_error&) {
            threw = true;
        }
        PC_CHECK(threw);
    }
    {
        /* Slot 0 (family 100) and slot 1 (group 10) share members {0,1}. */
        GoalSpec goal = family_goal(100, 1);
        GoalSlot overlapping;
        overlapping.group_id = 10;
        goal.slots.push_back(overlapping);
        bool threw = false;
        try {
            build_abstract_layout(*session, goal, registry, {});
        } catch (const std::runtime_error&) {
            threw = true;
        }
        PC_CHECK(threw);
    }
    {
        /* Exactly one of group/family must be set. */
        GoalSpec goal;
        goal.slots.emplace_back();
        bool threw = false;
        try {
            build_abstract_layout(*session, goal, registry, {});
        } catch (const std::runtime_error&) {
            threw = true;
        }
        PC_CHECK(threw);
    }
}

void run_projection_tests(const std::shared_ptr<SessionImpl>& session) {
    const ActionRegistry registry = build_action_registry(*session);
    const AbstractLayout layout = build_abstract_layout(
        *session, family_goal(100, 1), registry, basic_indices(registry));

    /* Satisfied goal plus one junk mod per side. */
    {
        pc_item_state item;
        pc_item_clear(&item);
        item.rarity = PC_RARITY_RARE;
        place(&item, PC_SIDE_PREFIX, 0, 10);
        place(&item, PC_SIDE_PREFIX, 3, 12);
        place(&item, PC_SIDE_SUFFIX, 5, 20);
        const AbstractState state = project_item(*session, layout, item);
        PC_CHECK(state.slot_status[0] ==
                 static_cast<std::uint8_t>(GoalSlotStatus::Satisfied));
        PC_CHECK(state.blocked_mask == 0);
        PC_CHECK(state.prefix_count == 2);
        PC_CHECK(state.suffix_count == 1);
        PC_CHECK(state.junk_counts ==
                 (std::vector<std::uint8_t>{1, 0, 1}));
        PC_CHECK(state.rarity == PC_RARITY_RARE);
        PC_CHECK(state.flags == 0);
    }

    /* Physical affix-array order and the visitation order of equivalent junk
     * must not split abstract state identity. */
    {
        pc_item_state left;
        pc_item_clear(&left);
        left.rarity = PC_RARITY_RARE;
        place(&left, PC_SIDE_PREFIX, 0, 10);
        place(&left, PC_SIDE_PREFIX, 3, 12);
        place(&left, PC_SIDE_SUFFIX, 5, 20);
        place(&left, PC_SIDE_SUFFIX, 7, 22);

        pc_item_state right;
        pc_item_clear(&right);
        right.rarity = PC_RARITY_RARE;
        place(&right, PC_SIDE_PREFIX, 3, 12);
        place(&right, PC_SIDE_PREFIX, 0, 10);
        place(&right, PC_SIDE_SUFFIX, 7, 22);
        place(&right, PC_SIDE_SUFFIX, 5, 20);

        const AbstractState projected_left =
            project_item(*session, layout, left);
        const AbstractState projected_right =
            project_item(*session, layout, right);
        PC_CHECK(projected_left == projected_right);
        PC_CHECK(abstract_state_hash(projected_left) ==
                 abstract_state_hash(projected_right));
    }

    /* Below-tier member: present but not satisfied, and not junk. */
    {
        pc_item_state item;
        pc_item_clear(&item);
        item.rarity = PC_RARITY_MAGIC;
        place(&item, PC_SIDE_PREFIX, 1, 10);
        const AbstractState state = project_item(*session, layout, item);
        PC_CHECK(state.slot_status[0] ==
                 static_cast<std::uint8_t>(GoalSlotStatus::PresentBelowTier));
        PC_CHECK(state.blocked_mask == 0);
        PC_CHECK(state.junk_counts ==
                 (std::vector<std::uint8_t>{0, 0, 0}));
    }

    /* Non-member sharing the goal's group: absent + blocked + counted in
     * the blocking junk class. */
    {
        pc_item_state item;
        pc_item_clear(&item);
        item.rarity = PC_RARITY_RARE;
        place(&item, PC_SIDE_PREFIX, 2, 10);
        const AbstractState state = project_item(*session, layout, item);
        PC_CHECK(state.slot_status[0] ==
                 static_cast<std::uint8_t>(GoalSlotStatus::Absent));
        PC_CHECK(state.blocked_mask == 1);
        PC_CHECK(state.junk_counts ==
                 (std::vector<std::uint8_t>{0, 1, 0}));
    }

    /* min_tier = 0 accepts any tier. */
    {
        const AbstractLayout any_tier = build_abstract_layout(
            *session, family_goal(100, 0), registry,
            basic_indices(registry));
        pc_item_state item;
        pc_item_clear(&item);
        item.rarity = PC_RARITY_MAGIC;
        place(&item, PC_SIDE_PREFIX, 1, 10);
        const AbstractState state = project_item(*session, any_tier, item);
        PC_CHECK(state.slot_status[0] ==
                 static_cast<std::uint8_t>(GoalSlotStatus::Satisfied));
    }

    /* Item and slot flags project onto abstract flags; equal states hash
     * equally and differing states compare unequal. */
    {
        pc_item_state item;
        pc_item_clear(&item);
        item.rarity = PC_RARITY_RARE;
        item.item_flags = PC_ITEM_CORRUPTED;
        place(&item, PC_SIDE_PREFIX, 3, 12, PC_MOD_SLOT_CRAFTED);
        const AbstractState state = project_item(*session, layout, item);
        PC_CHECK(state.flags & kFlagCorrupted);
        PC_CHECK(state.flags & kFlagCraftedMod);
        PC_CHECK(state.crafted_goal_mask == 0);
        PC_CHECK(state.crafted_junk_counts ==
                 (std::vector<std::uint8_t>{1, 0, 0}));

        const AbstractState again = project_item(*session, layout, item);
        PC_CHECK(state == again);
        PC_CHECK(abstract_state_hash(state) == abstract_state_hash(again));

        pc_item_state other = item;
        other.item_flags = 0;
        const AbstractState different =
            project_item(*session, layout, other);
        PC_CHECK(!(state == different));
    }

    /* Carrier flags remain attached to the exact goal slot or junk class,
     * including their intersection, rather than collapsing to item booleans. */
    {
        pc_item_state item;
        pc_item_clear(&item);
        item.rarity = PC_RARITY_RARE;
        place(&item, PC_SIDE_PREFIX, 0, 10,
              PC_MOD_SLOT_FRACTURED | PC_MOD_SLOT_CRAFTED);
        place(&item, PC_SIDE_PREFIX, 3, 12, PC_MOD_SLOT_FRACTURED);
        const AbstractState state = project_item(*session, layout, item);
        PC_CHECK(state.fractured_goal_mask == 1);
        PC_CHECK(state.crafted_goal_mask == 1);
        PC_CHECK(state.fractured_junk_counts ==
                 (std::vector<std::uint8_t>{1, 0, 0}));
        PC_CHECK(state.crafted_junk_counts ==
                 (std::vector<std::uint8_t>{0, 0, 0}));
        PC_CHECK(state.fractured_crafted_junk_counts ==
                 (std::vector<std::uint8_t>{0, 0, 0}));
    }
}

void run_exact_essence_relevance_test() {
    auto session = make_solver_session();
    auto data = std::make_shared<DataImpl>(*session->data);
    data->strings.push_back("essence_exact_t1");
    const std::uint32_t exact_sid =
        static_cast<std::uint32_t>(data->strings.size() - 1);
    data->strings.push_back("essence_same_family_t2");
    const std::uint32_t below_sid =
        static_cast<std::uint32_t>(data->strings.size() - 1);
    data->essence_count = 2;
    data->essence_key_sids = {exact_sid, below_sid};
    data->essence_item_level_restrictions = {-1, -1};
    session->data = std::move(data);
    session->essence_guaranteed_mod_ids = {0, 1};

    ActionRegistryBuildOptions options;
    options.goal_relevant_actions = true;
    options.fossil_goal_mod_ids = {{0}};
    const ActionRegistry registry = build_action_registry(*session, options);
    PC_CHECK(registry.index_by_id.contains("essence:essence_exact_t1"));
    PC_CHECK(!registry.index_by_id.contains(
        "essence:essence_same_family_t2"));
}

void run_legality_tests(const std::shared_ptr<SessionImpl>& session) {
    const ActionRegistry registry = build_action_registry(*session);
    const auto action = [&](const char* id) -> const ActionDescriptor& {
        return registry.actions[registry.index_by_id.at(id)];
    };
    AbstractState state;
    state.rarity = PC_RARITY_RARE;
    state.prefix_count = 3;
    state.suffix_count = 3;

    PC_CHECK(!action_legal(*session, action("exalt"), state)); /* full */
    state.suffix_count = 2;
    PC_CHECK(action_legal(*session, action("exalt"), state));
    PC_CHECK(!action_legal(*session, action("transmute"), state));
    PC_CHECK(action_legal(*session, action("chaos"), state));

    state.rarity = PC_RARITY_MAGIC;
    PC_CHECK(!action_legal(*session, action("chaos"), state));

    state.rarity = PC_RARITY_RARE;
    state.flags = kFlagCorrupted;
    PC_CHECK(!action_legal(*session, action("chaos"), state));
    PC_CHECK(action_legal(*session, action("restart"), state));

    state.flags = 0;
    state.prefix_count = 2;
    state.suffix_count = 1;
    PC_CHECK(!action_legal(*session, action("fracture"), state)); /* < 4 */
    state.suffix_count = 2;
    PC_CHECK(action_legal(*session, action("fracture"), state));
}

bool read_text_file(const std::string& path, std::string& out) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) return false;
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    out = buffer.str();
    return true;
}

/* Registry sanity over the real compiled artifact (Vaal Regalia fixture
 * base). Hand-computed classes stay in the synthetic cases above; this
 * checks the enumeration wires real data end to end. */
void run_artifact_registry_tests(const char* artifact_dir) {
    if (artifact_dir == nullptr) {
        std::printf("solver artifact suite skipped (missing path)\n");
        return;
    }
    const std::string dir = artifact_dir;
    std::string manifest_text;
    std::string strings_text;
    std::string game_text;
    if (!read_text_file(dir + "/manifest.json", manifest_text) ||
        !read_text_file(dir + "/strings.json", strings_text) ||
        !read_text_file(dir + "/game-data.json", game_text)) {
        std::printf("solver artifact suite skipped (unreadable artifact)\n");
        return;
    }
    std::shared_ptr<DataImpl> data;
    try {
        data = load_data_impl(manifest_text, strings_text, game_text);
    } catch (const std::exception& ex) {
        std::printf("solver artifact suite: load failed: %s\n", ex.what());
        PC_CHECK(false);
        return;
    }
    const auto base = data->base_by_path.find(
        "Metadata/Items/Armours/BodyArmours/BodyInt17");
    PC_CHECK(base != data->base_by_path.end());
    if (base == data->base_by_path.end()) return;

    auto session = std::make_shared<SessionImpl>();
    session->data = data;
    session->base_index = base->second;
    session->item_level = 86;
    try {
        build_session(*session);
    } catch (const std::exception& ex) {
        std::printf("solver artifact suite: session failed: %s\n", ex.what());
        PC_CHECK(false);
        return;
    }

    const ActionRegistry registry = build_action_registry(*session);
    PC_CHECK(registry.index_by_id.size() == registry.actions.size());
    PC_CHECK(registry.index_by_id.count("chaos") == 1);
    PC_CHECK(registry.index_by_id.count("restart") == 1);

    std::size_t essences = 0;
    std::size_t fossils = 0;
    std::size_t fossil_singles = 0;
    std::size_t bench = 0;
    std::size_t harvest = 0;
    std::size_t resist_pairs = 0;
    std::size_t discriminating_fossils = 0;
    for (const ActionDescriptor& action : registry.actions) {
        PC_CHECK(!action.cost_keys.empty() ||
                 action.params.type == ActionType::Unveil);
        PC_CHECK(std::is_sorted(action.discriminating_tag_ids.begin(),
                                action.discriminating_tag_ids.end()));
        /* RandomFossilOutcome rows are Tangled Fossil internals, never
         * plannable player currency. */
        PC_CHECK(action.id.find("RandomFossilOutcome") == std::string::npos);
        if (action.id.rfind("essence:", 0) == 0) ++essences;
        if (action.id.rfind("fossil:", 0) == 0) {
            ++fossils;
            if (action.params.fossil_indices.size() == 1) ++fossil_singles;
        }
        if (action.id.rfind("bench:", 0) == 0) ++bench;
        if (action.id.rfind("harvest_reforge:", 0) == 0) ++harvest;
        if (action.id.rfind("harvest_resist:", 0) == 0) {
            ++resist_pairs;
            const std::size_t target_separator = action.id.rfind(':');
            PC_CHECK(target_separator != std::string::npos);
            PC_CHECK(action.cost_keys.size() == 1);
            if (target_separator != std::string::npos &&
                action.cost_keys.size() == 1) {
                PC_CHECK(action.cost_keys[0] ==
                         "harvest_resist:" +
                             action.id.substr(target_separator + 1));
            }
        }
        if (action.params.type == ActionType::Fossil &&
            !action.discriminating_tag_ids.empty()) {
            ++discriminating_fossils;
        }
    }
    PC_CHECK(essences > 0);
    PC_CHECK(bench > 0);
    PC_CHECK(harvest > 0);
    PC_CHECK(registry.index_by_id.count("harvest_reforge:resistance") == 0);
    PC_CHECK(registry.index_by_id.count("harvest_augment:resistance") == 0);
    /* Fossil weight tags affect the current reforge pool, not later-state
     * predicates, so they intentionally do not widen junk classes. */
    PC_CHECK(discriminating_fossils == 0);
    /* Every 1-4 loadout over the named fossils enumerates. */
    const std::size_t n = fossil_singles;
    PC_CHECK(n > 0);
    PC_CHECK(fossils == n + n * (n - 1) / 2 + n * (n - 1) * (n - 2) / 6 +
                            n * (n - 1) * (n - 2) * (n - 3) / 24);
    /* Resistance conversion: six ordered fire/cold/lightning pairs. */
    PC_CHECK(resist_pairs == 6);

    /* A whole-registry layout for a real single-slot goal must partition
     * every reachable non-goal explicit mod exactly once. Use the group of
     * the first positively-weighted rollable prefix so the slot resolves
     * to something actions can actually hit. */
    std::uint32_t goal_group = kNoId;
    for (std::uint32_t mod = 0; mod < session->mod_count; ++mod) {
        if (session->gen_type[mod] == 0 &&
            pc_bitset_test(session->normal_random_roll_mask.data(), mod) &&
            pc_bitset_test(session->positive_base_weight_mask.data(), mod)) {
            goal_group = session->primary_group[mod];
            break;
        }
    }
    PC_CHECK(goal_group != kNoId);
    if (goal_group == kNoId) return;
    GoalSpec goal;
    GoalSlot slot;
    slot.group_id = goal_group;
    goal.slots.push_back(slot);
    const AbstractLayout layout =
        build_abstract_layout(*session, goal, registry, {});
    PC_CHECK(!layout.junk_classes.empty());
    std::size_t classified = 0;
    for (const JunkClass& junk : layout.junk_classes) {
        PC_CHECK(junk.member_count ==
                 pc_bitset_count(junk.member_mask.data(), session->words));
        classified += junk.member_count;
    }
    std::size_t mapped = 0;
    for (std::uint32_t mod = 0; mod < session->mod_count; ++mod) {
        if (layout.junk_class_by_mod[mod] != kNoId) ++mapped;
    }
    PC_CHECK(classified == mapped);
    std::printf(
        "solver artifact registry: %zu actions, %zu junk classes\n",
        registry.actions.size(), layout.junk_classes.size());
}

} // namespace

void run_solver_abstract_tests(const char* artifact_dir) {
    auto session = make_solver_session();
    run_registry_tests(*session);
    run_refinement_contract_tests(*session);
    run_exact_goal_member_class_tests();
    run_selector_conditioned_strict_partition_tests();
    run_junk_class_tests(session);
    run_projection_tests(session);
    run_exact_essence_relevance_test();
    run_legality_tests(session);
    run_artifact_registry_tests(artifact_dir);
}
