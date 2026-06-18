#include "tests.hpp"

#include "../src/engine_internal.hpp"
#include "poecraft/bitset.h"
#include "poecraft/item_state.h"

#include <memory>

using namespace poecraft;

/*
 * White-box test of multi-group (exclusivity) blocking. In the current
 * normal-roll universe no multi-group mod co-occurs with a group-sharing
 * sibling at positive weight, so a data-driven test cannot distinguish
 * multi-group blocking from primary-only blocking. This builds a tiny synthetic
 * session where the distinction is observable: a placed mod blocks a candidate
 * via its SECONDARY group, which primary-only blocking would miss.
 */
void run_blocking_tests() {
    auto data = std::make_shared<DataImpl>();
    data->mod_global_ids = {1000, 1001, 1002};

    SessionImpl s;
    s.data = data;
    s.mod_count = 3;
    s.words = pc_bitset_words(3);
    s.global_index = {0, 1, 2};
    s.gen_type = {0, 0, 0}; // all prefixes
    s.primary_group = {10, 11, 12};
    s.required_level = {1, 1, 1};
    s.base_spawn_weight = {100, 100, 100};
    s.base_gen_pct = {100, 100, 100};
    s.base_roll_weight = {100, 100, 100};
    // mod0 -> groups {10, 11} (multi-group), mod1 -> {11}, mod2 -> {12}
    s.group_offsets = {0, 2, 3, 4};
    s.group_ids = {10, 11, 11, 12};
    s.positive_base_weight_mask.assign(s.words, 0);
    for (uint32_t i = 0; i < 3; ++i) {
        pc_bitset_set(s.positive_base_weight_mask.data(), i);
    }

    // Empty item: all three candidates are eligible.
    pc_item_state empty;
    pc_item_clear(&empty);
    PC_CHECK(build_normal_pool(s, &empty, -1).size() == 3);

    // Place mod0 (groups {10, 11}). It must block mod1 via the shared SECONDARY
    // group 11 — primary-only blocking (group 10 only) would leave mod1 in.
    pc_item_state item;
    pc_item_clear(&item);
    item.rarity = PC_RARITY_RARE;
    item.prefixes[0].mod_id = 0;
    item.prefixes[0].group_id = 10; // cached primary; full set comes from session
    item.prefix_count = 1;

    const std::vector<PoolEntry> pool = build_normal_pool(s, &item, -1);
    PC_CHECK(pool.size() == 1);
    PC_CHECK(pool.size() == 1 && pool[0].session_mod_id == 2);

    // Placing a disjoint-group mod blocks nothing extra.
    pc_item_state other;
    pc_item_clear(&other);
    other.rarity = PC_RARITY_RARE;
    other.prefixes[0].mod_id = 2; // groups {12}
    other.prefixes[0].group_id = 12;
    other.prefix_count = 1;
    PC_CHECK(build_normal_pool(s, &other, -1).size() == 2); // mod0, mod1 remain
}
