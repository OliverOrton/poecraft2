#include "poecraft/item_state.h"

#include <cstring>

namespace {

struct SideView {
    pc_mod_slot* slots;
    uint8_t* count;
    uint8_t capacity;
};

bool resolve_side(pc_item_state* item, int side, SideView* out) {
    switch (side) {
    case PC_SIDE_PREFIX:
        out->slots = item->prefixes;
        out->count = &item->prefix_count;
        out->capacity = PC_MAX_PREFIXES;
        return true;
    case PC_SIDE_SUFFIX:
        out->slots = item->suffixes;
        out->count = &item->suffix_count;
        out->capacity = PC_MAX_SUFFIXES;
        return true;
    default:
        return false;
    }
}

void init_slot(pc_mod_slot* slot) {
    slot->mod_id = PC_MOD_NONE;
    slot->group_id = 0;
    slot->flags = 0;
    slot->roll_count = 0;
    std::memset(slot->rolls, 0, sizeof(slot->rolls));
    slot->veiled_option_count = 0;
    for (uint32_t i = 0; i < PC_MAX_VEILED_OPTIONS; ++i) {
        slot->veiled_option_mod_ids[i] = PC_MOD_NONE;
    }
    slot->veiled_chosen_mod_id = PC_MOD_NONE;
}

} // namespace

pc_result pc_item_clear(pc_item_state* item) {
    if (item == nullptr) {
        return PC_RESULT_INVALID_ARGUMENT;
    }
    std::memset(item, 0, sizeof(*item));
    item->rarity = PC_RARITY_NORMAL;
    for (auto& slot : item->prefixes) init_slot(&slot);
    for (auto& slot : item->suffixes) init_slot(&slot);
    for (auto& slot : item->implicits) init_slot(&slot);
    for (auto& slot : item->enchantments) init_slot(&slot);
    return PC_RESULT_OK;
}

pc_result pc_item_add_mod(
    pc_item_state* item,
    int side,
    uint32_t mod_id,
    uint16_t group_id,
    uint8_t flags,
    pc_mod_slot** slot_out) {
    if (item == nullptr) {
        return PC_RESULT_INVALID_ARGUMENT;
    }
    SideView view;
    if (!resolve_side(item, side, &view)) {
        return PC_RESULT_INVALID_ARGUMENT;
    }
    if (*view.count >= view.capacity) {
        return PC_RESULT_CAPACITY_EXCEEDED;
    }
    pc_mod_slot* slot = &view.slots[*view.count];
    init_slot(slot);
    slot->mod_id = mod_id;
    slot->group_id = group_id;
    slot->flags = flags;
    *view.count = static_cast<uint8_t>(*view.count + 1);
    if (slot_out != nullptr) {
        *slot_out = slot;
    }
    return PC_RESULT_OK;
}

pc_result pc_item_remove_at(pc_item_state* item, int side, uint32_t index) {
    if (item == nullptr) {
        return PC_RESULT_INVALID_ARGUMENT;
    }
    SideView view;
    if (!resolve_side(item, side, &view)) {
        return PC_RESULT_INVALID_ARGUMENT;
    }
    if (index >= *view.count) {
        return PC_RESULT_INVALID_ARGUMENT;
    }
    /* Swap the last live slot into the hole; affix order is not significant for
     * simulation (see docs/engine/item-state-flow.md). */
    const uint8_t last = static_cast<uint8_t>(*view.count - 1);
    if (index != last) {
        view.slots[index] = view.slots[last];
    }
    init_slot(&view.slots[last]);
    *view.count = last;
    return PC_RESULT_OK;
}

pc_result pc_item_clear_side(pc_item_state* item, int side) {
    if (item == nullptr) {
        return PC_RESULT_INVALID_ARGUMENT;
    }
    SideView view;
    if (!resolve_side(item, side, &view)) {
        return PC_RESULT_INVALID_ARGUMENT;
    }
    for (uint8_t i = 0; i < view.capacity; ++i) {
        init_slot(&view.slots[i]);
    }
    *view.count = 0;
    return PC_RESULT_OK;
}

pc_result pc_item_compact_side(
    pc_item_state* item,
    int side,
    uint8_t* out_count) {
    if (item == nullptr) {
        return PC_RESULT_INVALID_ARGUMENT;
    }
    SideView view;
    if (!resolve_side(item, side, &view)) {
        return PC_RESULT_INVALID_ARGUMENT;
    }
    uint8_t write = 0;
    for (uint8_t read = 0; read < view.capacity; ++read) {
        if (view.slots[read].mod_id != PC_MOD_NONE) {
            if (write != read) {
                view.slots[write] = view.slots[read];
                init_slot(&view.slots[read]);
            }
            ++write;
        }
    }
    /* Clear any slots between the new count and the old count. */
    for (uint8_t i = write; i < view.capacity; ++i) {
        init_slot(&view.slots[i]);
    }
    *view.count = write;
    if (out_count != nullptr) {
        *out_count = write;
    }
    return PC_RESULT_OK;
}

uint8_t pc_item_max_prefix(const pc_item_state* item) {
    if (item == nullptr) {
        return 0;
    }
    switch (item->rarity) {
    case PC_RARITY_NORMAL:
        return 0;
    case PC_RARITY_MAGIC:
        return 1;
    case PC_RARITY_RARE:
    default:
        return 3;
    }
}

uint8_t pc_item_max_suffix(const pc_item_state* item) {
    return pc_item_max_prefix(item);
}

int pc_item_can_add_prefix(const pc_item_state* item) {
    if (item == nullptr) {
        return 0;
    }
    return item->prefix_count < pc_item_max_prefix(item) ? 1 : 0;
}

int pc_item_can_add_suffix(const pc_item_state* item) {
    if (item == nullptr) {
        return 0;
    }
    return item->suffix_count < pc_item_max_suffix(item) ? 1 : 0;
}

static pc_result find_flagged(
    const pc_item_state* item,
    uint8_t flag,
    int* out_side,
    uint32_t* out_index) {
    if (item == nullptr) {
        return PC_RESULT_INVALID_ARGUMENT;
    }
    for (uint8_t i = 0; i < item->prefix_count; ++i) {
        if (item->prefixes[i].flags & flag) {
            if (out_side) *out_side = PC_SIDE_PREFIX;
            if (out_index) *out_index = i;
            return PC_RESULT_OK;
        }
    }
    for (uint8_t i = 0; i < item->suffix_count; ++i) {
        if (item->suffixes[i].flags & flag) {
            if (out_side) *out_side = PC_SIDE_SUFFIX;
            if (out_index) *out_index = i;
            return PC_RESULT_OK;
        }
    }
    return PC_RESULT_NOT_FOUND;
}

pc_result pc_item_find_fractured(
    const pc_item_state* item,
    int* out_side,
    uint32_t* out_index) {
    return find_flagged(item, PC_MOD_SLOT_FRACTURED, out_side, out_index);
}

pc_result pc_item_find_veiled(
    const pc_item_state* item,
    int* out_side,
    uint32_t* out_index) {
    return find_flagged(item, PC_MOD_SLOT_VEILED, out_side, out_index);
}
