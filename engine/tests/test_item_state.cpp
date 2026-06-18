#include "tests.hpp"

#include "poecraft/item_state.h"

#include <type_traits>

void run_item_state_tests() {
    /* ItemState must be a trivially copyable value type (cheap branch copy). */
    PC_CHECK(std::is_trivially_copyable<pc_item_state>::value);
    PC_CHECK(std::is_trivially_copyable<pc_mod_slot>::value);

    pc_item_state item;
    PC_CHECK(pc_item_clear(&item) == PC_RESULT_OK);
    PC_CHECK(item.rarity == PC_RARITY_NORMAL);
    PC_CHECK(item.prefix_count == 0 && item.suffix_count == 0);
    PC_CHECK(item.prefixes[0].mod_id == PC_MOD_NONE);

    /* max prefix/suffix per rarity */
    item.rarity = PC_RARITY_NORMAL;
    PC_CHECK(pc_item_max_prefix(&item) == 0);
    item.rarity = PC_RARITY_MAGIC;
    PC_CHECK(pc_item_max_prefix(&item) == 1);
    item.rarity = PC_RARITY_RARE;
    PC_CHECK(pc_item_max_prefix(&item) == 3);
    PC_CHECK(pc_item_max_suffix(&item) == 3);

    /* add prefixes up to the slot capacity */
    pc_mod_slot* slot = nullptr;
    PC_CHECK(pc_item_add_mod(&item, PC_SIDE_PREFIX, 10, 5, 0, &slot) ==
             PC_RESULT_OK);
    PC_CHECK(slot != nullptr && slot->mod_id == 10 && slot->group_id == 5);
    PC_CHECK(pc_item_add_mod(&item, PC_SIDE_PREFIX, 11, 6, 0, nullptr) ==
             PC_RESULT_OK);
    PC_CHECK(pc_item_add_mod(&item, PC_SIDE_PREFIX, 12, 7, 0, nullptr) ==
             PC_RESULT_OK);
    PC_CHECK(item.prefix_count == PC_MAX_PREFIXES);

    /* a full side fails and leaves the item unchanged */
    pc_item_state before = item;
    PC_CHECK(pc_item_add_mod(&item, PC_SIDE_PREFIX, 99, 9, 0, nullptr) ==
             PC_RESULT_CAPACITY_EXCEEDED);
    PC_CHECK(item.prefix_count == before.prefix_count);
    PC_CHECK(item.prefixes[0].mod_id == before.prefixes[0].mod_id);

    /* an invalid side is rejected and changes nothing */
    before = item;
    PC_CHECK(pc_item_add_mod(&item, 7, 1, 1, 0, nullptr) ==
             PC_RESULT_INVALID_ARGUMENT);
    PC_CHECK(item.prefix_count == before.prefix_count);

    /* remove swaps the last live slot into the hole and decrements */
    PC_CHECK(pc_item_remove_at(&item, PC_SIDE_PREFIX, 0) == PC_RESULT_OK);
    PC_CHECK(item.prefix_count == 2);
    PC_CHECK(item.prefixes[0].mod_id == 12); /* last (12) moved into slot 0 */
    PC_CHECK(item.prefixes[2].mod_id == PC_MOD_NONE);

    /* out-of-range remove fails, item unchanged */
    before = item;
    PC_CHECK(pc_item_remove_at(&item, PC_SIDE_PREFIX, 5) ==
             PC_RESULT_INVALID_ARGUMENT);
    PC_CHECK(item.prefix_count == before.prefix_count);

    /* compact collapses holes punched directly into the array */
    pc_item_clear(&item);
    pc_item_add_mod(&item, PC_SIDE_SUFFIX, 20, 1, 0, nullptr);
    pc_item_add_mod(&item, PC_SIDE_SUFFIX, 21, 2, 0, nullptr);
    pc_item_add_mod(&item, PC_SIDE_SUFFIX, 22, 3, 0, nullptr);
    item.suffixes[1].mod_id = PC_MOD_NONE; /* poke a hole in the middle */
    uint8_t live = 0;
    PC_CHECK(pc_item_compact_side(&item, PC_SIDE_SUFFIX, &live) == PC_RESULT_OK);
    PC_CHECK(live == 2 && item.suffix_count == 2);
    PC_CHECK(item.suffixes[0].mod_id == 20 && item.suffixes[1].mod_id == 22);
    PC_CHECK(item.suffixes[2].mod_id == PC_MOD_NONE);

    /* clear side */
    PC_CHECK(pc_item_clear_side(&item, PC_SIDE_SUFFIX) == PC_RESULT_OK);
    PC_CHECK(item.suffix_count == 0 && item.suffixes[0].mod_id == PC_MOD_NONE);

    /* can_add reflects rarity caps */
    pc_item_clear(&item);
    item.rarity = PC_RARITY_MAGIC;
    PC_CHECK(pc_item_can_add_prefix(&item) == 1);
    pc_item_add_mod(&item, PC_SIDE_PREFIX, 1, 1, 0, nullptr);
    PC_CHECK(pc_item_can_add_prefix(&item) == 0);

    /* find fractured / veiled */
    pc_item_clear(&item);
    item.rarity = PC_RARITY_RARE;
    pc_item_add_mod(&item, PC_SIDE_PREFIX, 1, 1, 0, nullptr);
    pc_item_add_mod(&item, PC_SIDE_SUFFIX, 2, 2, PC_MOD_SLOT_FRACTURED, nullptr);
    int side = -1;
    uint32_t index = 99;
    PC_CHECK(pc_item_find_fractured(&item, &side, &index) == PC_RESULT_OK);
    PC_CHECK(side == PC_SIDE_SUFFIX && index == 0);
    PC_CHECK(pc_item_find_veiled(&item, &side, &index) == PC_RESULT_NOT_FOUND);

    /* value copy independence: mutating a copy never touches the original */
    pc_item_state copy = item;
    pc_item_add_mod(&copy, PC_SIDE_PREFIX, 50, 50, 0, nullptr);
    PC_CHECK(copy.prefix_count == 2);
    PC_CHECK(item.prefix_count == 1); /* original unchanged */
}
