#include "tests.hpp"
#include <cstring>

#include "../src/engine_internal.hpp"
#include "poecraft/api.h"
#include "poecraft/bitset.h"
#include "poecraft/item_state.h"
#include "poecraft/session.h"
#include "poecraft/solver.h"
#include "../src/multi_item.hpp"
#include "poecraft/multi_item.h"

#include <memory>
#include <cmath>
#include <set>
#include <string>
#include <vector>

using namespace poecraft;

namespace {

// --- white-box synthetic session --------------------------------------------
// Three mods: two prefixes sharing group 10, one suffix in group 20.
SessionImpl make_synth_session() {
    auto data = std::make_shared<DataImpl>();
    data->mod_global_ids = {0, 1, 2};

    SessionImpl s;
    s.data = data;
    s.mod_count = 3;
    s.words = pc_bitset_words(3);
    s.global_index = {0, 1, 2};
    s.gen_type = {0, 0, 1}; // prefix, prefix, suffix
    s.primary_group = {10, 10, 20};
    s.required_level = {1, 1, 1};
    s.base_spawn_weight = {100, 100, 100};
    s.base_gen_pct = {100, 100, 100};
    s.base_roll_weight = {100, 100, 100};
    s.group_offsets = {0, 1, 2, 3};
    s.group_ids = {10, 10, 20};
    s.rare_affix_cap = 3;
    s.positive_base_weight_mask.assign(s.words, 0);
    s.positive_spawn_weight_mask.assign(s.words, 0);
    s.normal_random_roll_mask.assign(s.words, 0);
    s.prefix_mask.assign(s.words, 0);
    s.suffix_mask.assign(s.words, 0);
    s.influence_masks.assign(1, std::vector<uint64_t>(s.words, 0));
    for (uint32_t i = 0; i < 3; ++i) {
        pc_bitset_set(s.positive_base_weight_mask.data(), i);
        pc_bitset_set(s.positive_spawn_weight_mask.data(), i);
        pc_bitset_set(s.normal_random_roll_mask.data(), i);
        pc_bitset_set((i < 2 ? s.prefix_mask : s.suffix_mask).data(), i);
        pc_bitset_set(s.influence_masks[0].data(), i);
        if (s.primary_group[i] >= s.group_masks.size()) {
            s.group_masks.resize(s.primary_group[i] + 1);
        }
        auto& group_mask = s.group_masks[s.primary_group[i]];
        if (group_mask.empty()) group_mask.assign(s.words, 0);
        pc_bitset_set(group_mask.data(), i);
    }
    return s;
}

SessionImpl make_metamod_renewal_session() {
    constexpr std::uint32_t kModCount = 12;
    auto data = std::make_shared<DataImpl>();
    data->strings = {"", "Test Fossil"};
    data->mod_global_ids.resize(kModCount);
    for (std::uint32_t i = 0; i < kModCount; ++i) {
        data->mod_global_ids[i] = i;
    }
    data->tag_id_by_name = {{"attack", 0}, {"caster", 1}};
    data->metamod_prefixes_locked_code = 1;
    data->metamod_suffixes_locked_code = 2;
    data->metamod_no_attack_code = 3;
    data->metamod_no_caster_code = 4;
    data->metamod_multimod_code = 5;
    data->fossil_weight_offsets = {0, 0};
    data->fossil_name_sids = {1};
    data->fossil_rolls_lucky = {0};
    data->fossil_mirrors = {0};
    data->essence_item_level_restrictions = {-1};

    SessionImpl s;
    s.data = data;
    s.mod_count = kModCount;
    s.words = pc_bitset_words(kModCount);
    s.global_index.resize(kModCount);
    for (std::uint32_t i = 0; i < kModCount; ++i) s.global_index[i] = i;
    s.gen_type = {
        0, 1, 0, 1, 0, 0, 1, 1, 0, 1, 0, 1};
    s.primary_group.resize(kModCount);
    s.required_level.assign(kModCount, 1);
    s.base_spawn_weight.assign(kModCount, 100);
    s.base_gen_pct.assign(kModCount, 100);
    s.base_roll_weight.assign(kModCount, 100);
    s.group_offsets.resize(kModCount + 1);
    s.group_ids.resize(kModCount);
    s.class_offsets.assign(kModCount + 1, 0);
    s.class_tag_ids = {0, 1};
    for (std::uint32_t i = 0; i < kModCount; ++i) {
        s.primary_group[i] = static_cast<std::uint16_t>(10 + i);
        s.group_offsets[i] = i;
        s.group_ids[i] = 10 + i;
    }
    s.group_offsets[kModCount] = kModCount;
    s.class_offsets[0] = 0;
    s.class_offsets[1] = 1;
    for (std::uint32_t i = 2; i <= kModCount; ++i) {
        s.class_offsets[i] = 2;
    }
    s.rare_affix_cap = 3;
    s.positive_base_weight_mask.assign(s.words, 0);
    s.positive_spawn_weight_mask.assign(s.words, 0);
    s.normal_random_roll_mask.assign(s.words, 0);
    s.prefix_mask.assign(s.words, 0);
    s.suffix_mask.assign(s.words, 0);
    s.influence_masks.assign(1, std::vector<uint64_t>(s.words, 0));
    s.implicit_tag_masks.assign(2, std::vector<uint64_t>(s.words, 0));
    for (std::uint32_t i = 0; i < kModCount; ++i) {
        pc_bitset_set((s.gen_type[i] == 0 ? s.prefix_mask : s.suffix_mask).data(),
                      i);
        pc_bitset_set(s.influence_masks[0].data(), i);
        if (s.primary_group[i] >= s.group_masks.size()) {
            s.group_masks.resize(s.primary_group[i] + 1);
        }
        auto& group_mask = s.group_masks[s.primary_group[i]];
        if (group_mask.empty()) group_mask.assign(s.words, 0);
        pc_bitset_set(group_mask.data(), i);
    }
    for (std::uint32_t i = 0; i <= 4; ++i) {
        pc_bitset_set(s.positive_base_weight_mask.data(), i);
        pc_bitset_set(s.positive_spawn_weight_mask.data(), i);
        if (i < 4) pc_bitset_set(s.normal_random_roll_mask.data(), i);
    }
    pc_bitset_set(s.implicit_tag_masks[0].data(), 0);
    pc_bitset_set(s.implicit_tag_masks[1].data(), 1);
    s.metamod_type.assign(kModCount, -1);
    s.metamod_type[7] = data->metamod_prefixes_locked_code;
    s.metamod_type[8] = data->metamod_suffixes_locked_code;
    s.metamod_type[9] = data->metamod_no_attack_code;
    s.metamod_type[10] = data->metamod_no_caster_code;
    s.metamod_type[11] = data->metamod_multimod_code;
    s.essence_guaranteed_mod_ids = {4};
    s.fossil_added_mod_ids.resize(1);
    s.fossil_forced_mod_ids.resize(1);
    return s;
}

void place(pc_item_state* item, int side, uint32_t mod_id, uint16_t group,
           uint8_t flags) {
    pc_item_add_mod(item, side, mod_id, group, flags, nullptr);
}

bool contains_mod(const pc_item_state& item, const std::uint32_t mod_id) {
    for (std::uint8_t i = 0; i < item.prefix_count; ++i) {
        if (item.prefixes[i].mod_id == mod_id) return true;
    }
    for (std::uint8_t i = 0; i < item.suffix_count; ++i) {
        if (item.suffixes[i].mod_id == mod_id) return true;
    }
    return false;
}

// Pin both branches independently of the production artifact's fossil flags.
// Identical seeds and pools must yield identical one-item carriers, with only
// the declared mirroring flag differing. This tests the existing abstraction;
// it does not approve a Fractured Fossil fracture or copy-output mechanic.
void run_fossil_mirror_flag_tests() {
    for (std::uint64_t seed = 0; seed < 8; ++seed) {
        pc_item_state results[2]{};
        for (int mirrors = 0; mirrors <= 1; ++mirrors) {
            auto session = std::make_shared<SessionImpl>(
                make_metamod_renewal_session());
            auto data = std::make_shared<DataImpl>(*session->data);
            data->strings[1] = "Fractured Fossil";
            data->fossil_mirrors[0] = mirrors;
            session->data = data;
            ActionContextImpl context(seed);
            context.session = session;
            ActionParameters action;
            action.type = ActionType::Fossil;
            action.fossil_indices = {0};
            pc_item_clear(&results[mirrors]);
            PC_CHECK(apply_action(context, &results[mirrors], action).applied);
            PC_CHECK(results[mirrors].rarity == PC_RARITY_RARE);
            PC_CHECK(results[mirrors].prefix_count +
                         results[mirrors].suffix_count > 0);
            PC_CHECK(results[mirrors].item_flags ==
                     (mirrors ? PC_ITEM_MIRRORED : 0));
        }
        results[1].item_flags &= ~PC_ITEM_MIRRORED;
        PC_CHECK(std::memcmp(&results[0], &results[1],
                             sizeof(pc_item_state)) == 0);
    }
}

void run_current_fossil_refusal_tests() {
    auto session = std::make_shared<SessionImpl>(make_metamod_renewal_session());
    auto data = std::make_shared<DataImpl>(*session->data);
    data->strings.push_back("Metadata/Items/Currency/CurrencyDelveCraftingMirror");
    data->fossil_count = 2;
    data->fossil_key_sids = {1, 2};
    data->fossil_name_sids = {1, 1};
    data->fossil_mirrors = {0, 0};
    data->fossil_rolls_lucky = {0, 0};
    data->fossil_weight_offsets = {0, 0, 0};
    session->data = data;
    session->fossil_added_mod_ids.resize(2);
    session->fossil_forced_mod_ids.resize(2);
    session->fossil_sell_price_mod_ids.resize(2);
    for (const auto loadout : {std::vector<std::uint32_t>{1}, {0, 1}, {1, 0}}) {
        for (int carrier = 0; carrier < 4; ++carrier) {
            pc_item_state item{};
            pc_item_clear(&item);
            item.rarity = PC_RARITY_RARE;
            if (carrier == 1) place(&item, PC_SIDE_PREFIX, 0, 10, PC_MOD_SLOT_FRACTURED);
            if (carrier == 2) item.generic_influence_bits = 1;
            if (carrier == 3) item.item_flags = PC_ITEM_CORRUPTED;
            const auto before = item;
            ActionContextImpl context(99);
            context.session = session;
            auto expected_rng = context.rng;
            ActionParameters action;
            action.type = ActionType::Fossil;
            action.fossil_indices = loadout;
            bool rejected = false;
            try { apply_action(context, &item, action); }
            catch (const std::invalid_argument& e) {
                rejected = std::strstr(e.what(), "Fractured Fossil is unavailable") != nullptr;
            }
            PC_CHECK(rejected);
            PC_CHECK(std::memcmp(&before, &item, sizeof(item)) == 0);
            auto actual_rng = context.rng;
            for (int draw = 0; draw < 4; ++draw)
                PC_CHECK(actual_rng.next_u64() == expected_rng.next_u64());
            PC_CHECK(context.pool_cache_hits == 0 && context.pool_cache_misses == 0);
        }
    }
    // The stable-key guard must not disable an explicit historical mirror law.
    data->fossil_mirrors[1] = 1;
    ActionContextImpl legacy(99);
    legacy.session = session;
    pc_item_state old_result{};
    pc_item_clear(&old_result);
    ActionParameters old_action;
    old_action.type = ActionType::Fossil;
    old_action.fossil_indices = {1};
    PC_CHECK(apply_action(legacy, &old_result, old_action).applied);
    PC_CHECK((old_result.item_flags & PC_ITEM_MIRRORED) != 0);
    PC_CHECK(unavailable_fossil_reason(*data, {0}) == nullptr);
}

void run_metamod_renewal_unit_tests() {
    auto session =
        std::make_shared<SessionImpl>(make_metamod_renewal_session());
    const auto apply_renewal = [&](const ActionType type,
                                   pc_item_state& item,
                                   const std::uint64_t seed) {
        ActionContextImpl context(seed);
        context.session = session;
        ActionParameters action;
        action.type = type;
        if (type == ActionType::Essence) action.essence_index = 0;
        if (type == ActionType::Fossil) action.fossil_indices = {0};
        return apply_action(context, &item, action);
    };

    for (const ActionType type : {ActionType::Fossil, ActionType::Essence}) {
        /* Prefix and suffix locks are ordinary removable crafted affixes for
         * these two owner-corrected renewals. */
        {
            pc_item_state item;
            pc_item_clear(&item);
            item.rarity = PC_RARITY_RARE;
            place(&item, PC_SIDE_PREFIX, 5, 15, 0);
            place(&item, PC_SIDE_SUFFIX, 7, 17, PC_MOD_SLOT_CRAFTED);
            PC_CHECK(apply_renewal(type, item, 11).applied);
            PC_CHECK(!contains_mod(item, 5));
            PC_CHECK(!contains_mod(item, 7));
        }
        {
            pc_item_state item;
            pc_item_clear(&item);
            item.rarity = PC_RARITY_RARE;
            place(&item, PC_SIDE_SUFFIX, 6, 16, 0);
            place(&item, PC_SIDE_PREFIX, 8, 18, PC_MOD_SLOT_CRAFTED);
            PC_CHECK(apply_renewal(type, item, 13).applied);
            PC_CHECK(!contains_mod(item, 6));
            PC_CHECK(!contains_mod(item, 8));
        }

        /* A fractured metamod remains as a fractured affix, but its active
         * cannot-roll effect is ignored by Fossil/Essence pool generation. */
        bool saw_attack = false;
        bool saw_caster = false;
        for (std::uint64_t seed = 1; seed <= 256; ++seed) {
            pc_item_state attack;
            pc_item_clear(&attack);
            attack.rarity = PC_RARITY_RARE;
            place(&attack, PC_SIDE_SUFFIX, 9, 19,
                  PC_MOD_SLOT_CRAFTED | PC_MOD_SLOT_FRACTURED);
            PC_CHECK(apply_renewal(type, attack, seed).applied);
            PC_CHECK(contains_mod(attack, 9));
            saw_attack |= contains_mod(attack, 0);

            pc_item_state caster;
            pc_item_clear(&caster);
            caster.rarity = PC_RARITY_RARE;
            place(&caster, PC_SIDE_PREFIX, 10, 20,
                  PC_MOD_SLOT_CRAFTED | PC_MOD_SLOT_FRACTURED);
            PC_CHECK(apply_renewal(type, caster, seed).applied);
            PC_CHECK(contains_mod(caster, 10));
            saw_caster |= contains_mod(caster, 1);
        }
        PC_CHECK(saw_attack);
        PC_CHECK(saw_caster);

        /* Fractured preservation is independent of metamod protection. */
        pc_item_state fractured;
        pc_item_clear(&fractured);
        fractured.rarity = PC_RARITY_RARE;
        place(&fractured, PC_SIDE_PREFIX, 5, 15, PC_MOD_SLOT_FRACTURED);
        PC_CHECK(apply_renewal(type, fractured, 17).applied);
        PC_CHECK(contains_mod(fractured, 5));
        PC_CHECK(fractured.prefixes[0].flags & PC_MOD_SLOT_FRACTURED);
    }

    /* A renewal that genuinely respects metamods retains the existing native
     * behavior: Chaos preserves the locked side and obeys cannot-roll. */
    {
        pc_item_state item;
        pc_item_clear(&item);
        item.rarity = PC_RARITY_RARE;
        place(&item, PC_SIDE_PREFIX, 5, 15, 0);
        place(&item, PC_SIDE_SUFFIX, 7, 17, PC_MOD_SLOT_CRAFTED);
        PC_CHECK(apply_renewal(ActionType::Chaos, item, 19).applied);
        PC_CHECK(contains_mod(item, 5));
    }
    for (std::uint64_t seed = 1; seed <= 64; ++seed) {
        pc_item_state item;
        pc_item_clear(&item);
        item.rarity = PC_RARITY_RARE;
        place(&item, PC_SIDE_SUFFIX, 9, 19,
              PC_MOD_SLOT_CRAFTED | PC_MOD_SLOT_FRACTURED);
        PC_CHECK(apply_renewal(ActionType::Chaos, item, seed).applied);
        PC_CHECK(!contains_mod(item, 0));
    }
}

void run_reforge_unit_tests() {
    auto session = std::make_shared<SessionImpl>(make_synth_session());
    ActionContextImpl context(7);
    context.session = session;
    auto run = [&](pc_item_state* item, ActionType type) {
        ActionParameters action;
        action.type = type;
        return apply_action(context, item, action);
    };

    // The incremental refill sampler must preserve the reference path's RNG
    // mapping, selected mods, and per-pick pool totals.
    {
        ActionContextImpl incremental(12345);
        incremental.session = session;
        incremental.incremental_refill_enabled = true;
        ActionContextImpl reference(12345);
        reference.session = session;
        reference.incremental_refill_enabled = false;
        pc_item_state fast;
        pc_item_state slow;
        pc_item_clear(&fast);
        pc_item_clear(&slow);
        fast.rarity = slow.rarity = PC_RARITY_RARE;
        ActionParameters action;
        action.type = ActionType::Chaos;
        for (int iteration = 0; iteration < 32; ++iteration) {
            const ActionOutcome fast_out =
                apply_action(incremental, &fast, action);
            const ActionOutcome slow_out =
                apply_action(reference, &slow, action);
            PC_CHECK(fast_out.applied == slow_out.applied);
            PC_CHECK(fast_out.added == slow_out.added);
            PC_CHECK(fast_out.removed == slow_out.removed);
            PC_CHECK(fast.prefix_count == slow.prefix_count);
            PC_CHECK(fast.suffix_count == slow.suffix_count);
            for (std::uint8_t i = 0; i < fast.prefix_count; ++i) {
                PC_CHECK(
                    fast.prefixes[i].mod_id == slow.prefixes[i].mod_id);
            }
            for (std::uint8_t i = 0; i < fast.suffix_count; ++i) {
                PC_CHECK(
                    fast.suffixes[i].mod_id == slow.suffixes[i].mod_id);
            }
            PC_CHECK(incremental.last_action_trace.size() ==
                     reference.last_action_trace.size());
            for (std::size_t i = 0;
                 i < incremental.last_action_trace.size(); ++i) {
                const auto& a = incremental.last_action_trace[i];
                const auto& b = reference.last_action_trace[i];
                PC_CHECK(a.roll == b.roll);
                PC_CHECK(a.chosen_mod_id == b.chosen_mod_id);
                PC_CHECK(a.chosen_side == b.chosen_side);
                PC_CHECK(a.prefix_total_weight == b.prefix_total_weight);
                PC_CHECK(a.suffix_total_weight == b.suffix_total_weight);
                PC_CHECK(a.combined_total_weight == b.combined_total_weight);
            }
        }
        PC_CHECK(!incremental.refill_pool_cache.empty());
    }

    // Simulator-mode superset rejection remains unbiased over surviving mods.
    // Mods 0 and 1 share equal weight/group, so exactly one survives each
    // craft and their long-run selection rates should remain near 50/50.
    {
        ActionContextImpl rejection(98765);
        rejection.session = session;
        rejection.capture_action_trace = false;
        int picked_zero = 0;
        int picked_one = 0;
        ActionParameters action;
        action.type = ActionType::Chaos;
        for (int iteration = 0; iteration < 10'000; ++iteration) {
            pc_item_state item;
            pc_item_clear(&item);
            item.rarity = PC_RARITY_RARE;
            const ActionOutcome out =
                apply_action(rejection, &item, action);
            PC_CHECK(out.applied);
            PC_CHECK(item.prefix_count == 1);
            PC_CHECK(item.suffix_count == 1);
            PC_CHECK(item.suffixes[0].mod_id == 2);
            picked_zero += item.prefixes[0].mod_id == 0;
            picked_one += item.prefixes[0].mod_id == 1;
        }
        PC_CHECK(picked_zero + picked_one == 10'000);
        PC_CHECK(picked_zero > 4'700 && picked_zero < 5'300);
    }

    // A) A removed (non-fractured) mod's group is freed: chaos rerolls a fresh
    //    group-10 prefix and a group-20 suffix. If the block were not cleared,
    //    the prefix side would stay empty.
    {
        pc_item_state item;
        pc_item_clear(&item);
        item.rarity = PC_RARITY_RARE;
        place(&item, PC_SIDE_PREFIX, 0, 10, 0);
        ActionOutcome out = run(&item, ActionType::Chaos);
        PC_CHECK(out.applied);
        PC_CHECK(item.rarity == PC_RARITY_RARE);
        PC_CHECK(item.prefix_count == 1); // group 10 re-rolled
        PC_CHECK(item.suffix_count == 1); // group 20
    }

    // B) A fractured mod is preserved and keeps blocking its group: chaos cannot
    //    add a second group-10 prefix.
    {
        pc_item_state item;
        pc_item_clear(&item);
        item.rarity = PC_RARITY_RARE;
        place(&item, PC_SIDE_PREFIX, 0, 10, PC_MOD_SLOT_FRACTURED);
        ActionOutcome out = run(&item, ActionType::Chaos);
        PC_CHECK(out.applied);
        PC_CHECK(item.prefix_count == 1);
        PC_CHECK(item.prefixes[0].mod_id == 0);
        PC_CHECK(item.prefixes[0].flags & PC_MOD_SLOT_FRACTURED);
        PC_CHECK(item.suffix_count == 1); // suffix still rolls
    }

    // C) Scour keeps a fractured mod and drops to magic.
    {
        pc_item_state item;
        pc_item_clear(&item);
        item.rarity = PC_RARITY_RARE;
        place(&item, PC_SIDE_PREFIX, 0, 10, PC_MOD_SLOT_FRACTURED);
        place(&item, PC_SIDE_SUFFIX, 2, 20, 0);
        ActionOutcome out = run(&item, ActionType::Scour);
        PC_CHECK(out.applied);
        PC_CHECK(item.rarity == PC_RARITY_MAGIC);
        PC_CHECK(item.prefix_count == 1 && item.suffix_count == 0);
        PC_CHECK(item.prefixes[0].flags & PC_MOD_SLOT_FRACTURED);
    }

    // C2) A rarity-only Scour is still an applied action. The simulator must
    // not reject the same rare-to-magic transition modeled by the solver.
    {
        pc_item_state item;
        pc_item_clear(&item);
        item.rarity = PC_RARITY_RARE;
        place(&item, PC_SIDE_PREFIX, 0, 10, PC_MOD_SLOT_FRACTURED);
        ActionOutcome out = run(&item, ActionType::Scour);
        PC_CHECK(out.applied);
        PC_CHECK(out.removed == 0);
        PC_CHECK(item.rarity == PC_RARITY_MAGIC);
        PC_CHECK(item.prefix_count == 1 && item.suffix_count == 0);
    }

    // D) Scour with no fractured mod returns to normal with no mods.
    {
        pc_item_state item;
        pc_item_clear(&item);
        item.rarity = PC_RARITY_RARE;
        place(&item, PC_SIDE_PREFIX, 0, 10, 0);
        place(&item, PC_SIDE_SUFFIX, 2, 20, 0);
        ActionOutcome out = run(&item, ActionType::Scour);
        PC_CHECK(out.applied);
        PC_CHECK(item.rarity == PC_RARITY_NORMAL);
        PC_CHECK(item.prefix_count == 0 && item.suffix_count == 0);
    }

    // E) Annul never removes a fractured mod.
    {
        pc_item_state item;
        pc_item_clear(&item);
        item.rarity = PC_RARITY_RARE;
        place(&item, PC_SIDE_PREFIX, 0, 10, PC_MOD_SLOT_FRACTURED);
        place(&item, PC_SIDE_SUFFIX, 2, 20, 0);
        ActionOutcome out = run(&item, ActionType::Annul);
        PC_CHECK(out.applied && out.removed == 1);
        // only the non-fractured suffix can go
        PC_CHECK(item.prefix_count == 1 && item.suffix_count == 0);
        PC_CHECK(item.prefixes[0].flags & PC_MOD_SLOT_FRACTURED);

        // annul again: only the fractured mod remains -> nothing removable
        ActionOutcome out2 = run(&item, ActionType::Annul);
        PC_CHECK(!out2.applied);
        PC_CHECK(item.prefix_count == 1);
    }

    // F) Bench removal consumes the action once and removes every crafted
    //    explicit modifier without changing rarity or ordinary modifiers.
    {
        pc_item_state item;
        pc_item_clear(&item);
        item.rarity = PC_RARITY_RARE;
        place(&item, PC_SIDE_PREFIX, 0, 10, PC_MOD_SLOT_CRAFTED);
        place(&item, PC_SIDE_SUFFIX, 2, 20, 0);
        ActionOutcome out =
            run(&item, ActionType::RemoveCraftedModifiers);
        PC_CHECK(out.applied && out.removed == 1);
        PC_CHECK(item.rarity == PC_RARITY_RARE);
        PC_CHECK(item.prefix_count == 0 && item.suffix_count == 1);
        PC_CHECK(item.suffixes[0].mod_id == 2);
        PC_CHECK(!run(&item, ActionType::RemoveCraftedModifiers).applied);

        item.suffixes[0].flags =
            PC_MOD_SLOT_CRAFTED | PC_MOD_SLOT_FRACTURED;
        PC_CHECK(!run(&item, ActionType::RemoveCraftedModifiers).applied);
        PC_CHECK(item.suffix_count == 1);
    }
}

void run_fossil_precision_unit_test() {
    auto data = std::make_shared<DataImpl>();
    data->mod_global_ids = {0};
    data->fossil_weight_offsets = {0, 1, 2};
    data->fossil_weight_kind_codes = {1, 1};
    data->fossil_weight_tag_ids = {7, 7};
    data->fossil_weight_values = {33, 33};
    data->fossil_weight_positive_code = 1;
    data->fossil_weight_negative_code = 0;

    auto session = std::make_shared<SessionImpl>();
    session->data = data;
    session->mod_count = 1;
    session->words = pc_bitset_words(1);
    session->global_index = {0};
    session->gen_type = {0};
    session->primary_group = {10};
    session->required_level = {1};
    session->base_spawn_weight = {10'000};
    session->base_gen_pct = {100};
    session->base_roll_weight = {10'000};
    session->group_offsets = {0, 1};
    session->group_ids = {10};
    session->class_offsets = {0, 1};
    session->class_tag_ids = {7};
    session->fossil_added_mod_ids.resize(2);
    session->fossil_forced_mod_ids.resize(2);
    session->normal_random_roll_mask.assign(session->words, 0);
    session->positive_base_weight_mask.assign(session->words, 0);
    session->prefix_mask.assign(session->words, 0);
    session->suffix_mask.assign(session->words, 0);
    session->influence_masks.assign(
        1, std::vector<uint64_t>(session->words, 0));
    pc_bitset_set(session->normal_random_roll_mask.data(), 0);
    pc_bitset_set(session->positive_base_weight_mask.data(), 0);
    pc_bitset_set(session->prefix_mask.data(), 0);
    pc_bitset_set(session->influence_masks[0].data(), 0);

    ActionContextImpl context(1);
    context.session = session;
    pc_item_state item;
    pc_item_clear(&item);
    item.rarity = PC_RARITY_RARE;

    PoolBuildRequest request;
    request.weight_kind = PoolWeightKind::Fossil;
    request.fossil_indices = {0, 1};
    const WeightedPool& pool =
        get_weighted_pool(context, &item, request);
    PC_CHECK(pool.entries.size() == 1);
    // 10,000 * 0.33 * 0.33 = 1,089. The old percent accumulator
    // truncated 33% * 33% to 10% first and incorrectly returned 1,000.
    PC_CHECK(pool.entries[0].final_weight == 1089);
}

// --- integration against the real artifact ----------------------------------

std::set<uint32_t> live_mod_ids(const pc_item_state* item) {
    std::set<uint32_t> ids;
    for (uint8_t i = 0; i < item->prefix_count; ++i)
        ids.insert(item->prefixes[i].mod_id);
    for (uint8_t i = 0; i < item->suffix_count; ++i)
        ids.insert(item->suffixes[i].mod_id);
    return ids;
}

// No exclusivity group may appear on more than one live mod.
void check_groups_distinct(pc_session_handle session, const pc_item_state* item) {
    pc_error_info error;
    std::set<uint32_t> seen;
    bool ok = true;
    for (uint32_t id : live_mod_ids(item)) {
        uint32_t n = 0;
        pc_session_dump_mod_groups(session, id, nullptr, 0, &n, &error);
        std::vector<uint32_t> groups(n);
        pc_session_dump_mod_groups(session, id, groups.data(), n, &n, &error);
        for (uint32_t g : groups) {
            if (!seen.insert(g).second) ok = false;
        }
    }
    PC_CHECK(ok);
}

pc_item_state make_item(pc_session_handle session, uint8_t rarity) {
    pc_error_info error;
    pc_item_init_options opt;
    opt.struct_size = sizeof(opt);
    opt.abi_version = PC_ABI_VERSION;
    opt.rarity = rarity;
    opt.with_implicits = 0;
    pc_item_state item;
    pc_item_init(session, &opt, &item, &error);
    return item;
}

pc_action_result apply(pc_action_context_handle ctx, pc_item_state* item,
                       int action) {
    pc_error_info error;
    pc_action_request req{};
    req.struct_size = sizeof(req);
    req.abi_version = PC_ABI_VERSION;
    req.action_type = action;
    pc_action_result result;
    pc_apply_action(ctx, item, &req, &result, &error);
    return result;
}

pc_action_result apply_special(
    pc_action_context_handle ctx,
    pc_item_state* item,
    int action,
    const char* key) {
    pc_error_info error;
    pc_action_request req{};
    req.struct_size = sizeof(req);
    req.abi_version = PC_ABI_VERSION;
    req.action_type = action;
    if (action == PC_ACTION_ESSENCE) {
        req.essence_key = key;
    } else {
        req.fossil_count = 1;
        req.fossil_keys[0] = key;
    }
    pc_action_result result;
    pc_apply_action(ctx, item, &req, &result, &error);
    return result;
}

void run_integration_tests(const char* artifact_dir) {
    pc_error_info error;
    pc_error_info_init(&error);
    pc_data_handle data = nullptr;
    if (pc_data_load_file((std::string(artifact_dir) + "/manifest.json").c_str(),
                          &data, &error) != PC_RESULT_OK) {
        PC_CHECK(false);
        return;
    }
    pc_session_options sopt;
    sopt.struct_size = sizeof(sopt);
    sopt.abi_version = PC_ABI_VERSION;
    sopt.base_metadata_path = "Metadata/Items/Armours/BodyArmours/BodyInt17";
    sopt.item_level = 86;
    pc_session_handle session = nullptr;
    PC_CHECK(pc_session_create(data, &sopt, &session, &error) == PC_RESULT_OK);

    // ABI guard: a struct declaring an incompatible abi_version is rejected.
    {
        pc_session_options bad = sopt;
        bad.abi_version = PC_ABI_VERSION + 1;
        pc_session_handle rejected = nullptr;
        PC_CHECK(pc_session_create(data, &bad, &rejected, &error) ==
                 PC_RESULT_INVALID_ARGUMENT);
        PC_CHECK(rejected == nullptr);
    }

    pc_action_context_options copt;
    copt.struct_size = sizeof(copt);
    copt.abi_version = PC_ABI_VERSION;
    copt.seed = 0xC0FFEE;
    pc_action_context_handle ctx = nullptr;
    pc_action_context_create(session, &copt, &ctx, &error);

    // transmute: normal -> magic, 1-2 mods, <=1 per side
    {
        pc_item_state item = make_item(session, PC_RARITY_NORMAL);
        pc_action_result r = apply(ctx, &item, PC_ACTION_TRANSMUTE);
        PC_CHECK(r.applied == 1);
        PC_CHECK(item.rarity == PC_RARITY_MAGIC);
        int total = item.prefix_count + item.suffix_count;
        PC_CHECK(total >= 1 && total <= 2);
        PC_CHECK(item.prefix_count <= 1 && item.suffix_count <= 1);
        check_groups_distinct(session, &item);

        // transmute again on a magic item is a no-op
        pc_item_state before = item;
        pc_action_result r2 = apply(ctx, &item, PC_ACTION_TRANSMUTE);
        PC_CHECK(r2.applied == 0);
        PC_CHECK(live_mod_ids(&item) == live_mod_ids(&before));
    }

    // alchemy: normal -> rare, 4-6 mods
    {
        pc_item_state item = make_item(session, PC_RARITY_NORMAL);
        pc_action_result r = apply(ctx, &item, PC_ACTION_ALCHEMY);
        PC_CHECK(r.applied == 1);
        PC_CHECK(item.rarity == PC_RARITY_RARE);
        int total = item.prefix_count + item.suffix_count;
        PC_CHECK(total >= 4 && total <= 6);
        PC_CHECK(item.prefix_count <= 3 && item.suffix_count <= 3);
        check_groups_distinct(session, &item);

        // exalt until full, then exalt is a no-op leaving the item unchanged
        for (int i = 0; i < 8; ++i) apply(ctx, &item, PC_ACTION_EXALT);
        PC_CHECK(item.prefix_count + item.suffix_count == 6);
        pc_item_state full = item;
        pc_action_result rex = apply(ctx, &item, PC_ACTION_EXALT);
        PC_CHECK(rex.applied == 0);
        PC_CHECK(live_mod_ids(&item) == live_mod_ids(&full));
        check_groups_distinct(session, &item);

        // annul removes exactly one
        pc_action_result ran = apply(ctx, &item, PC_ACTION_ANNUL);
        PC_CHECK(ran.applied == 1 && ran.removed == 1);
        PC_CHECK(item.prefix_count + item.suffix_count == 5);

        // chaos rerolls to a fresh 4-6 rare
        pc_action_result rc = apply(ctx, &item, PC_ACTION_CHAOS);
        PC_CHECK(rc.applied == 1);
        int total2 = item.prefix_count + item.suffix_count;
        PC_CHECK(total2 >= 4 && total2 <= 6);
        check_groups_distinct(session, &item);

        // scour clears to a normal item
        pc_action_result rs = apply(ctx, &item, PC_ACTION_SCOUR);
        PC_CHECK(rs.applied == 1);
        PC_CHECK(item.rarity == PC_RARITY_NORMAL);
        PC_CHECK(item.prefix_count == 0 && item.suffix_count == 0);
    }

    // magic path: augment then regal
    {
        pc_item_state item = make_item(session, PC_RARITY_NORMAL);
        apply(ctx, &item, PC_ACTION_TRANSMUTE);
        const int after_transmute = item.prefix_count + item.suffix_count;
        pc_action_result raug = apply(ctx, &item, PC_ACTION_AUGMENT);
        if (after_transmute < 2) {
            PC_CHECK(raug.applied == 1);
            PC_CHECK(item.prefix_count + item.suffix_count == 2);
        }
        check_groups_distinct(session, &item);

        pc_action_result rreg = apply(ctx, &item, PC_ACTION_REGAL);
        PC_CHECK(rreg.applied == 1);
        PC_CHECK(item.rarity == PC_RARITY_RARE);
        check_groups_distinct(session, &item);
    }

    // alteration rerolls a magic item to 1-2 mods
    {
        pc_item_state item = make_item(session, PC_RARITY_NORMAL);
        apply(ctx, &item, PC_ACTION_TRANSMUTE);
        pc_action_result ralt = apply(ctx, &item, PC_ACTION_ALTERATION);
        PC_CHECK(ralt.applied == 1);
        PC_CHECK(item.rarity == PC_RARITY_MAGIC);
        int total = item.prefix_count + item.suffix_count;
        PC_CHECK(total >= 1 && total <= 2);
        check_groups_distinct(session, &item);
    }

    // Fracturing Orb marks exactly one random explicit modifier, keeps the
    // item otherwise unchanged, and cannot be applied twice.
    {
        pc_item_state item = make_item(session, PC_RARITY_NORMAL);
        PC_CHECK(apply(ctx, &item, PC_ACTION_ALCHEMY).applied == 1);
        const std::set<uint32_t> before = live_mod_ids(&item);
        PC_CHECK(before.size() >= 4);

        pc_action_result fractured = apply(ctx, &item, PC_ACTION_FRACTURE);
        PC_CHECK(fractured.applied == 1);
        PC_CHECK(fractured.added == 0 && fractured.removed == 0);
        PC_CHECK(live_mod_ids(&item) == before);
        int fractured_count = 0;
        for (uint8_t i = 0; i < item.prefix_count; ++i)
            fractured_count +=
                (item.prefixes[i].flags & PC_MOD_SLOT_FRACTURED) != 0;
        for (uint8_t i = 0; i < item.suffix_count; ++i)
            fractured_count +=
                (item.suffixes[i].flags & PC_MOD_SLOT_FRACTURED) != 0;
        PC_CHECK(fractured_count == 1);
        PC_CHECK(apply(ctx, &item, PC_ACTION_FRACTURE).applied == 0);

        uint32_t trace_count = 0;
        PC_CHECK(pc_action_context_debug_last_trace(
                     ctx, nullptr, 0, &trace_count, &error) ==
                 PC_RESULT_BUFFER_TOO_SMALL);
        PC_CHECK(trace_count == 0); // failed second use clears the trace
    }

    // Generic influence and synthesised state block fracturing, while
    // Eldritch implicits do not.
    {
        pc_item_state influenced = make_item(session, PC_RARITY_NORMAL);
        PC_CHECK(apply(ctx, &influenced, PC_ACTION_ALCHEMY).applied == 1);
        influenced.generic_influence_bits = 1;
        PC_CHECK(apply(ctx, &influenced, PC_ACTION_FRACTURE).applied == 0);

        pc_item_state synthesised = make_item(session, PC_RARITY_NORMAL);
        PC_CHECK(apply(ctx, &synthesised, PC_ACTION_ALCHEMY).applied == 1);
        synthesised.item_flags |= PC_ITEM_SYNTHESISED;
        PC_CHECK(apply(ctx, &synthesised, PC_ACTION_FRACTURE).applied == 0);

        pc_item_state eldritch = make_item(session, PC_RARITY_NORMAL);
        PC_CHECK(apply(ctx, &eldritch, PC_ACTION_ALCHEMY).applied == 1);
        pc_action_request ember{};
        ember.struct_size = sizeof(ember);
        ember.abi_version = PC_ABI_VERSION;
        ember.action_type = PC_ACTION_ELDRITCH_EMBER;
        ember.tier = 1;
        pc_action_result ember_result{};
        PC_CHECK(pc_apply_action(
                     ctx, &eldritch, &ember, &ember_result, &error) ==
                 PC_RESULT_OK);
        PC_CHECK(ember_result.applied == 1);
        PC_CHECK(apply(ctx, &eldritch, PC_ACTION_FRACTURE).applied == 1);
    }

    // exalt on a normal item is a no-op (wrong rarity)
    {
        pc_item_state item = make_item(session, PC_RARITY_NORMAL);
        pc_action_result r = apply(ctx, &item, PC_ACTION_EXALT);
        PC_CHECK(r.applied == 0);
        PC_CHECK(item.prefix_count == 0 && item.suffix_count == 0);
    }

    // Essence guarantees its direct mod, then fills the remaining rare slots.
    {
        pc_item_state item = make_item(session, PC_RARITY_NORMAL);
        pc_action_result r = apply_special(
            ctx, &item, PC_ACTION_ESSENCE,
            "Metadata/Items/Currency/CurrencyEssenceAnguish2");
        PC_CHECK(r.applied == 1);
        PC_CHECK(item.rarity == PC_RARITY_RARE);
        PC_CHECK(item.prefix_count + item.suffix_count >= 4);
        bool saw_essence_reach = false;
        for (uint32_t id : live_mod_ids(&item)) {
            pc_mod_info info;
            pc_session_get_mod_info(session, id, &info, &error);
            if (info.reach_kind == PC_MOD_REACH_ESSENCE) {
                saw_essence_reach = true;
            }
        }
        PC_CHECK(saw_essence_reach);

        uint32_t trace_count = 0;
        pc_action_context_debug_last_trace(ctx, nullptr, 0, &trace_count,
                                           &error);
        PC_CHECK(trace_count >= 2);
        std::vector<pc_action_trace_stage> trace(trace_count);
        pc_action_context_debug_last_trace(ctx, trace.data(), trace_count,
                                           &trace_count, &error);
        PC_CHECK(trace[0].direct == 1);
        bool saw_weighted_stage = false;
        for (const auto& stage : trace) {
            if (!stage.direct) {
                PC_CHECK(stage.combined_total_weight ==
                         stage.prefix_total_weight +
                             stage.suffix_total_weight);
                PC_CHECK(stage.chosen_side == PC_SIDE_PREFIX ||
                         stage.chosen_side == PC_SIDE_SUFFIX);
                saw_weighted_stage = true;
            }
        }
        PC_CHECK(saw_weighted_stage);
    }

    // A basic fossil craft uses the fossil-weighted pool and returns a rare.
    {
        pc_item_state item = make_item(session, PC_RARITY_NORMAL);
        pc_action_result r = apply_special(
            ctx, &item, PC_ACTION_FOSSIL,
            "Metadata/Items/Currency/CurrencyDelveCraftingRandom");
        PC_CHECK(r.applied == 1);
        PC_CHECK(item.rarity == PC_RARITY_RARE);
        PC_CHECK(item.prefix_count + item.suffix_count >= 4);
        check_groups_distinct(session, &item);
    }

    // Sanctified applies its required-level weight multiplier. Numeric lucky
    // rolls are inert in structural simulation, but the craft is supported.
    {
        pc_item_state item = make_item(session, PC_RARITY_NORMAL);
        pc_action_request req{};
        req.struct_size = sizeof(req);
        req.abi_version = PC_ABI_VERSION;
        req.action_type = PC_ACTION_FOSSIL;
        req.fossil_count = 1;
        req.fossil_keys[0] =
            "Metadata/Items/Currency/CurrencyDelveCraftingLuckyModRolls";
        pc_action_result result{};
        PC_CHECK(pc_apply_action(ctx, &item, &req, &result, &error) ==
                 PC_RESULT_OK);
        PC_CHECK(result.applied == 1);
        PC_CHECK(item.rarity == PC_RARITY_RARE);
    }

    // Phase 13 direct mechanics are available through the same C ABI.
    {
        pc_item_state item = make_item(session, PC_RARITY_RARE);
        pc_action_request req{};
        req.struct_size = sizeof(req);
        req.abi_version = PC_ABI_VERSION;
        pc_action_result result{};

        req.action_type = PC_ACTION_BENCH;
        req.mod_key = "StrMasterItemGenerationCannotChangePrefixes";
        PC_CHECK(pc_apply_action(ctx, &item, &req, &result, &error) ==
                 PC_RESULT_OK);
        PC_CHECK(result.applied == 1);

        req = {};
        req.struct_size = sizeof(req);
        req.abi_version = PC_ABI_VERSION;
        req.action_type = PC_ACTION_VEILED_EXALT;
        PC_CHECK(pc_apply_action(ctx, &item, &req, &result, &error) ==
                 PC_RESULT_OK);
        PC_CHECK(result.applied == 1);
        int veiled_side = -1;
        uint32_t veiled_index = 0;
        PC_CHECK(pc_item_find_veiled(
                     &item, &veiled_side, &veiled_index) == PC_RESULT_OK);
        const pc_mod_slot& veiled =
            veiled_side == PC_SIDE_PREFIX ? item.prefixes[veiled_index]
                                          : item.suffixes[veiled_index];
        PC_CHECK(veiled.veiled_option_count == 3);
        pc_mod_info choice{};
        PC_CHECK(pc_session_get_mod_info(
                     session, veiled.veiled_option_mod_ids[0], &choice,
                     &error) == PC_RESULT_OK);
        req = {};
        req.struct_size = sizeof(req);
        req.abi_version = PC_ABI_VERSION;
        req.action_type = PC_ACTION_UNVEIL;
        req.mod_key = choice.key;
        PC_CHECK(pc_apply_action(ctx, &item, &req, &result, &error) ==
                 PC_RESULT_OK);
        PC_CHECK(result.applied == 1);

        item = make_item(session, PC_RARITY_RARE);
        req = {};
        req.struct_size = sizeof(req);
        req.abi_version = PC_ABI_VERSION;
        req.action_type = PC_ACTION_HARVEST_REFORGE;
        req.target_tag = "life";
        PC_CHECK(pc_apply_action(ctx, &item, &req, &result, &error) ==
                 PC_RESULT_OK);
        PC_CHECK(result.applied == 1);

        item = make_item(session, PC_RARITY_RARE);
        req = {};
        req.struct_size = sizeof(req);
        req.abi_version = PC_ABI_VERSION;
        req.action_type = PC_ACTION_ELDRITCH_EMBER;
        req.tier = 1;
        PC_CHECK(pc_apply_action(ctx, &item, &req, &result, &error) ==
                 PC_RESULT_OK);
        PC_CHECK(result.applied == 1);
        PC_CHECK(item.searing_exarch_tier == 1);
        PC_CHECK(item.implicit_count == 1);

        item = make_item(session, PC_RARITY_RARE);
        req = {};
        req.struct_size = sizeof(req);
        req.abi_version = PC_ABI_VERSION;
        req.action_type = PC_ACTION_INFLUENCE_EXALT;
        req.influence = "crusader";
        PC_CHECK(pc_apply_action(ctx, &item, &req, &result, &error) ==
                 PC_RESULT_OK);
        PC_CHECK(result.applied == 1);
        PC_CHECK(item.generic_influence_bits != 0);

        /* Public currency names resolve to RePoE's internal influence codes;
         * legacy compiled inputs remain accepted for the four real
         * Conqueror currencies only. */
        for (const char* influence : {"warlord", "adjudicator"}) {
            item = make_item(session, PC_RARITY_RARE);
            req.influence = influence;
            PC_CHECK(pc_apply_action(ctx, &item, &req, &result, &error) ==
                     PC_RESULT_OK);
            PC_CHECK(result.applied == 1);
            PC_CHECK(item.generic_influence_bits != 0);
        }
        for (const char* influence : {"elder", "shaper"}) {
            item = make_item(session, PC_RARITY_RARE);
            req.influence = influence;
            PC_CHECK(pc_apply_action(ctx, &item, &req, &result, &error) ==
                     PC_RESULT_OK);
            PC_CHECK(result.applied == 1);
            PC_CHECK(item.generic_influence_bits != 0);
            for (const auto flag : {PC_ITEM_CORRUPTED, PC_ITEM_MIRRORED, PC_ITEM_SYNTHESISED}) {
                item = make_item(session, PC_RARITY_RARE);
                item.item_flags = flag;
                const auto before = item;
                PC_CHECK(pc_apply_action(ctx, &item, &req, &result, &error) == PC_RESULT_OK);
                PC_CHECK(!result.applied);
                PC_CHECK(std::memcmp(&before, &item, sizeof(item)) == 0);
            }
        }
    }

    // Native batch application parses once and reuses one context/cache.
    {
        std::vector<pc_item_state> items(256);
        for (auto& item : items) {
            item = make_item(session, PC_RARITY_NORMAL);
        }
        pc_action_request req{};
        req.struct_size = sizeof(req);
        req.abi_version = PC_ABI_VERSION;
        req.action_type = PC_ACTION_ALCHEMY;
        std::vector<pc_action_result> results(items.size());
        pc_batch_summary summary{};
        PC_CHECK(pc_apply_action_batch(
                     ctx, items.data(), static_cast<uint32_t>(items.size()),
                     &req, results.data(), &summary, &error) == PC_RESULT_OK);
        PC_CHECK(summary.item_count == items.size());
        PC_CHECK(summary.applied_count == items.size());
        for (std::size_t i = 0; i < items.size(); ++i) {
            PC_CHECK(results[i].applied == 1);
            PC_CHECK(items[i].rarity == PC_RARITY_RARE);
        }
    }

    pc_action_context_destroy(ctx);
    pc_session_destroy(session);
    pc_data_destroy(data);
}

} // namespace

void run_foulborn_weight_tests() {
    const unsigned keep[] = {0, 1, 1, 2, 2, 3, 3, 4, 5, 5, 6, 6, 7, 7};
    for (unsigned n = 1; n <= 13; ++n) {
        auto data = std::make_shared<DataImpl>();
        data->strings = {"type"};
        SessionImpl session;
        session.data = data;
        WeightedPool pool;
        for (unsigned i = 0; i < n; ++i) {
            data->mod_type_key_sid.push_back(0);
            session.global_index.push_back(i);
            PoolEntry entry{};
            entry.session_mod_id = i;
            entry.required_level = i + 1;
            entry.final_weight = 100;
            pool.entries.push_back(entry);
        }
        apply_foulborn_transform(session, pool);
        PC_CHECK(pool.entries.size() == keep[n]);
        for (const auto& entry : pool.entries) {
            PC_CHECK(entry.required_level > n - keep[n]);
            PC_CHECK(std::abs(double(entry.final_weight) / pool.total_weight - 1.0 / keep[n]) < 1e-12);
        }
    }
    // Unequal-weight witness: low -> high 800/400/200/100 against
    // an independent weight-600 type gives 1/3, 1/6, 1/2.
    auto data = std::make_shared<DataImpl>();
    data->strings = {"first", "other"};
    data->mod_type_key_sid = {0,0,0,0,1};
    SessionImpl session;
    session.data = data;
    session.global_index = {0,1,2,3,4};
    WeightedPool pool;
    const unsigned weights[] = {800,400,200,100,600};
    for (unsigned i=0; i<5; ++i) {
        PoolEntry e{}; e.session_mod_id=i; e.required_level=i+1; e.final_weight=weights[i];
        pool.entries.push_back(e);
    }
    apply_foulborn_transform(session,pool);
    PC_CHECK(pool.entries.size()==3 && pool.total_weight==1200);
    PC_CHECK(pool.entries[0].final_weight==400);
    PC_CHECK(pool.entries[1].final_weight==200);
    PC_CHECK(pool.entries[2].final_weight==600);
    // Fractional witness: N=3,K=2 with high weights 1,2 versus
    // a separate weight-1 type gives ratio 3:6:2, without truncation.
    data->mod_type_key_sid={0,0,0,1}; session.global_index={0,1,2,3};
    pool={};
    for (unsigned i=0; i<4; ++i) {
        PoolEntry e{}; e.session_mod_id=i; e.required_level=i+1; e.final_weight=i==2?2:1;
        pool.entries.push_back(e);
    }
    apply_foulborn_transform(session,pool);
    PC_CHECK(pool.total_weight==11 && pool.entries.size()==3);
    PC_CHECK(pool.entries[0].final_weight==3 && pool.entries[1].final_weight==6 && pool.entries[2].final_weight==2);
}

void run_currency_contract_tests(const char* artifact_dir) {
    run_fossil_mirror_flag_tests();
    run_current_fossil_refusal_tests();
    run_fossil_guard_exact_tests();
    // Role-neutral foundation: consume both sources and create a third identity.
    auto session = std::make_shared<SessionImpl>(make_synth_session());
    CraftResource a{"a", "left", session, {}}, b{"b", "right", session, {}};
    a.item.memory_strands = 17;
    CraftResource gone_a = a, gone_b = b, made{"c", "output", session, {}};
    gone_a.item.lifecycle = gone_b.item.lifecycle = PC_ITEM_CONSUMED;
    CraftTransaction transaction{{{"a", PC_RESOURCE_CONSUMED, a, gone_a},
        {"b", PC_RESOURCE_CONSUMED, b, gone_b}, {"c", PC_RESOURCE_CREATED, {}, made}}, {}};
    std::vector<CraftResource> resources{a, b};
    commit_craft_transaction(resources, transaction);
    PC_CHECK(resources.size() == 3 && resources[0].item.lifecycle == PC_ITEM_CONSUMED);
    PC_CHECK(resources[1].item.lifecycle == PC_ITEM_CONSUMED && resources[2].identity == "c");
    PC_CHECK(resources[0].item.memory_strands == 17);
    bool rejected = false;
    try { commit_craft_transaction(resources, transaction); } catch (...) { rejected = true; }
    PC_CHECK(rejected && resources.size() == 3 && resources[2].item.lifecycle == PC_ITEM_LIVE);
    if (!artifact_dir) return;
    pc_error_info error{}; pc_error_info_init(&error);
    pc_data_handle data{}; const auto manifest = std::string(artifact_dir) + "/manifest.json";
    PC_CHECK(pc_data_load_file(manifest.c_str(), &data, &error) == PC_RESULT_OK);
    pc_session_options opts{}; opts.struct_size = sizeof(opts); opts.abi_version = PC_ABI_VERSION;
    opts.base_metadata_path = "Metadata/Items/Armours/BodyArmours/BodyInt17"; opts.item_level = 86;
    pc_session_handle native{};
    PC_CHECK(pc_session_create(data, &opts, &native, &error) == PC_RESULT_OK);
    const std::string goal = R"({"version":"v1","rarity":"magic","slots":[{"family_mod_key":"LocalIncreasedEnergyShield11","min_tier":1}],"actions":["foulborn_augment"]})";
    const std::string prices = R"({"version":"v1","prices":{"foulborn_augment":1}})";
    pc_solver_handle solver{}; pc_economy_handle economy{};
    PC_CHECK(pc_solver_create(native, goal.c_str(), goal.size(), &solver, &error) == PC_RESULT_OK);
    PC_CHECK(pc_economy_load_json(prices.c_str(), prices.size(), &economy, &error) == PC_RESULT_OK);
    if (solver && economy) {
        pc_item_state start{}; start.rarity = PC_RARITY_MAGIC;
        for (int dimension = 0; dimension < 3; ++dimension) {
            auto affected = start;
            if (dimension == 0) affected.memory_strands = 50;
            if (dimension == 1) affected.lifecycle = PC_ITEM_DESTROYED;
            if (dimension == 2) affected.enchantment_count = 1;
            for (const auto mode : {PC_SOLVER_MODE_CURRENT, PC_SOLVER_MODE_STRATEGY_FINDER}) {
                pc_solve_options options{}; options.struct_size = sizeof(options); options.abi_version = PC_ABI_VERSION;
                options.solver_mode = mode;
                PC_CHECK(pc_solver_solve_begin(solver, &affected, economy, &options, &error) == PC_RESULT_UNSUPPORTED_FEATURE);
                PC_CHECK(std::strstr(error.message, "Pro") != nullptr);
                pc_solve_summary summary{};
                PC_CHECK(pc_solver_solve(solver, &affected, economy, &options, &summary, &error) == PC_RESULT_UNSUPPORTED_FEATURE);
            }
        }
        std::uint32_t state = 0;
        PC_CHECK(pc_solver_project_item(solver, &start, &state, &error) == PC_RESULT_OK);
        const auto disabled_goal = goal.substr(0, goal.size()-1) + ",\"disabled_action_families\":[\"foulborn\"]}";
        pc_solver_handle disabled{};
        PC_CHECK(pc_solver_create(native, disabled_goal.c_str(), disabled_goal.size(), &disabled, &error) == PC_RESULT_OK);
        std::uint32_t action=0, count=0;
        PC_CHECK(pc_solver_find_action(disabled, "foulborn_augment", &action, &error) == PC_RESULT_OK);
        PC_CHECK(pc_calc_action_outcomes(disabled, &start, action, nullptr, 0, &count, nullptr, &error) == PC_RESULT_UNSUPPORTED_FEATURE);
        pc_solver_destroy(disabled);
    }
    pc_solver_destroy(solver); pc_economy_destroy(economy); pc_session_destroy(native); pc_data_destroy(data);
}

void run_action_tests(const char* artifact_dir) {
    run_currency_contract_tests(artifact_dir);
    run_foulborn_weight_tests();
    run_reforge_unit_tests(); // always runs (synthetic, no data needed)
    run_metamod_renewal_unit_tests();
    run_fossil_precision_unit_test();
    if (artifact_dir == nullptr) {
        std::printf("action integration suite skipped (no artifact dir)\n");
        return;
    }
    run_integration_tests(artifact_dir);
}

// Ordered generation rows depend on tags added by the *current* modifier set.
void run_dynamic_tag_tests() {
    for (const bool large : {false, true}) {
        auto session = std::make_shared<SessionImpl>(make_synth_session());
        auto d = std::const_pointer_cast<DataImpl>(session->data);
        session->rare_affix_cap = 2;
        session->has_added_tags = true;
        session->effective_base_tag_ids = {0, large ? 2u : 1u};
        session->primary_group = {10, 11, 20};
        session->group_ids = session->primary_group;
        session->group_masks.assign(21, {});
        for (uint32_t i = 0; i < 3; ++i) {
            session->group_masks[session->primary_group[i]].assign(1, uint64_t{1} << i);
        }
        d->spawn_offsets = {0, 1, 2, 3};
        d->spawn_tag_ids = {0, 0, 0};
        d->spawn_weights = {100, 100, 100};
        d->gen_offsets = {0, 3, 6, 6};
        d->gen_tag_ids = {2, 3, 1, 2, 3, 1};
        d->gen_weights = {100, 0, 100, 100, 0, 100};
        d->adds_tag_offsets = {0, 1, 2, 2};
        d->adds_tag_ids = {3, 3};
        session->class_offsets = {0, 0, 0, 0};
        ActionContextImpl context(17);
        context.session = session;
        pc_item_state item;
        pc_item_clear(&item);
        item.rarity = PC_RARITY_RARE;
        PoolBuildRequest request;
        PC_CHECK(intern_item_tag_signature(context, &item) == 0);
        PC_CHECK(get_weighted_pool(context, &item, request).total_weight == 300);
        place(&item, PC_SIDE_PREFIX, 0, 10, 0);
        const auto tagged = intern_item_tag_signature(context, &item);
        PC_CHECK(tagged != 0);
        PC_CHECK(get_weighted_pool(context, &item, request).total_weight == (large ? 200 : 100));
        // A retained implicit contributes its added tags too, even with no explicit.
        pc_item_state implicit;
        pc_item_clear(&implicit);
        implicit.rarity = PC_RARITY_RARE;
        implicit.implicit_count = 1;
        implicit.implicits[0].mod_id = 0;
        PC_CHECK(intern_item_tag_signature(context, &implicit) == tagged);
        // Cache reuse after removing/replacing a modifier must rebuild the signature.
        pc_item_clear(&item);
        item.rarity = PC_RARITY_RARE;
        PC_CHECK(intern_item_tag_signature(context, &item) == 0);
        PC_CHECK(get_weighted_pool(context, &item, request).total_weight == 300);
        for (uint64_t seed = 0; seed < 200; ++seed) {
            context.rng.reseed(seed);
            pc_item_clear(&item);
            ActionParameters action;
            action.type = ActionType::Transmute;
            PC_CHECK(apply_action(context, &item, action).applied);
            unsigned notables = 0;
            for (uint8_t i = 0; i < item.prefix_count; ++i)
                if (item.prefixes[i].mod_id < 2) ++notables;
            PC_CHECK(large || notables <= 1);
            pc_item_clear(&item);
            item.rarity = PC_RARITY_RARE;
            action.type = ActionType::Chaos;
            PC_CHECK(apply_action(context, &item, action).applied);
            notables = 0;
            for (uint8_t i = 0; i < item.prefix_count; ++i)
                if (item.prefixes[i].mod_id < 2) ++notables;
            PC_CHECK(large ? notables == 2 : notables <= 1);

        }
    }
}
