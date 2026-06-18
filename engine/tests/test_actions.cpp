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
    for (uint32_t i = 0; i < 3; ++i) {
        pc_bitset_set(s.positive_base_weight_mask.data(), i);
    }
    return s;
}

void place(pc_item_state* item, int side, uint32_t mod_id, uint16_t group,
           uint8_t flags) {
    pc_item_add_mod(item, side, mod_id, group, flags, nullptr);
}

void run_reforge_unit_tests() {
    const SessionImpl s = make_synth_session();
    Rng rng(7);

    // A) A removed (non-fractured) mod's group is freed: chaos rerolls a fresh
    //    group-10 prefix and a group-20 suffix. If the block were not cleared,
    //    the prefix side would stay empty.
    {
        pc_item_state item;
        pc_item_clear(&item);
        item.rarity = PC_RARITY_RARE;
        place(&item, PC_SIDE_PREFIX, 0, 10, 0);
        ActionOutcome out = apply_action(s, rng, &item, ActionType::Chaos);
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
        ActionOutcome out = apply_action(s, rng, &item, ActionType::Chaos);
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
        ActionOutcome out = apply_action(s, rng, &item, ActionType::Scour);
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
        ActionOutcome out = apply_action(s, rng, &item, ActionType::Scour);
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
        ActionOutcome out = apply_action(s, rng, &item, ActionType::Annul);
        PC_CHECK(out.applied && out.removed == 1);
        // only the non-fractured suffix can go
        PC_CHECK(item.prefix_count == 1 && item.suffix_count == 0);
        PC_CHECK(item.prefixes[0].flags & PC_MOD_SLOT_FRACTURED);

        // annul again: only the fractured mod remains -> nothing removable
        ActionOutcome out2 = apply_action(s, rng, &item, ActionType::Annul);
        PC_CHECK(!out2.applied);
        PC_CHECK(item.prefix_count == 1);
    }
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
    pc_action_request req;
    req.struct_size = sizeof(req);
    req.abi_version = PC_ABI_VERSION;
    req.action_type = action;
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

    // exalt on a normal item is a no-op (wrong rarity)
    {
        pc_item_state item = make_item(session, PC_RARITY_NORMAL);
        pc_action_result r = apply(ctx, &item, PC_ACTION_EXALT);
        PC_CHECK(r.applied == 0);
        PC_CHECK(item.prefix_count == 0 && item.suffix_count == 0);
    }

    pc_action_context_destroy(ctx);
    pc_session_destroy(session);
    pc_data_destroy(data);
}

} // namespace

void run_action_tests(const char* artifact_dir) {
    run_reforge_unit_tests(); // always runs (synthetic, no data needed)
    if (artifact_dir == nullptr) {
        std::printf("action integration suite skipped (no artifact dir)\n");
        return;
    }
    run_integration_tests(artifact_dir);
}
