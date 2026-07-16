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

void place(pc_item_state* item, int side, std::uint32_t mod_id,
           std::uint16_t group, std::uint8_t flags = 0) {
    pc_item_add_mod(item, side, mod_id, group, flags, nullptr);
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
    PC_CHECK(combo.discriminating_tag_ids ==
             (std::vector<std::uint32_t>{kTagAttack, kTagFire}));
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
    /* Most fossils bias tags; specials (lucky rolls, mirrors, sockets)
     * legitimately discriminate on nothing. */
    PC_CHECK(discriminating_fossils > 0);
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
    run_junk_class_tests(session);
    run_projection_tests(session);
    run_legality_tests(session);
    run_artifact_registry_tests(artifact_dir);
}
