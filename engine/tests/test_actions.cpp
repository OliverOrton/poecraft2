#include "tests.hpp"

#include "../src/engine_internal.hpp"
#include "poecraft/api.h"
#include "poecraft/bitset.h"
#include "poecraft/item_state.h"
#include "poecraft/session.h"

#include <memory>
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

void place(pc_item_state* item, int side, uint32_t mod_id, uint16_t group,
           uint8_t flags) {
    pc_item_add_mod(item, side, mod_id, group, flags, nullptr);
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

void run_action_tests(const char* artifact_dir) {
    run_reforge_unit_tests(); // always runs (synthetic, no data needed)
    run_fossil_precision_unit_test();
    if (artifact_dir == nullptr) {
        std::printf("action integration suite skipped (no artifact dir)\n");
        return;
    }
    run_integration_tests(artifact_dir);
}
