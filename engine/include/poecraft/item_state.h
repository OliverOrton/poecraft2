#ifndef POECRAFT_ITEM_STATE_H
#define POECRAFT_ITEM_STATE_H

#include <stdint.h>

#include "poecraft/result.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Compact, value-copyable item state. See docs/item-state-flow.md.
 *
 * These fixed capacities are provisional implementation limits, not assumed
 * game rules. pc_data_check_capacities() validates them against the current
 * ingested dataset before the ABI is frozen. Load/import must report an
 * explicit capacity error rather than truncating extra rolls or implicits.
 */
#define PC_MAX_PREFIXES 3
#define PC_MAX_SUFFIXES 3
#define PC_MAX_IMPLICITS 8
#define PC_MAX_ENCHANTS 4
#define PC_MAX_ROLL_VALUES 8
#define PC_MAX_VEILED_OPTIONS 3
#define PC_MAX_SOCKETS 6
#define PC_MOD_NONE UINT32_MAX

/* Mod-slot flags. Mirror docs/item-state-flow.md ModSlotFlags. */
typedef enum pc_mod_slot_flags {
    PC_MOD_SLOT_FRACTURED = 1 << 0,
    PC_MOD_SLOT_CRAFTED = 1 << 1,
    PC_MOD_SLOT_VEILED = 1 << 2,
    PC_MOD_SLOT_ELDRITCH = 1 << 3,
    PC_MOD_SLOT_SYNTH = 1 << 4
} pc_mod_slot_flags;

/* Item-wide flags. Mirror docs/item-state-flow.md ItemFlags. */
typedef enum pc_item_flags {
    PC_ITEM_CORRUPTED = 1 << 0,
    PC_ITEM_MIRRORED = 1 << 1,
    PC_ITEM_SPLIT = 1 << 2,
    PC_ITEM_SYNTHESISED = 1 << 3
} pc_item_flags;

typedef enum pc_rarity {
    PC_RARITY_NORMAL = 0,
    PC_RARITY_MAGIC = 1,
    PC_RARITY_RARE = 2
} pc_rarity;

/* A mod side. Implicits/enchantments use the dedicated arrays directly. */
typedef enum pc_affix_side {
    PC_SIDE_PREFIX = 0,
    PC_SIDE_SUFFIX = 1
} pc_affix_side;

typedef struct pc_mod_slot {
    uint32_t mod_id;   /* dense session mod id, or PC_MOD_NONE */
    uint16_t group_id; /* cached from SessionData for fast group blocking */
    uint8_t flags;     /* pc_mod_slot_flags */

    uint8_t roll_count;
    int32_t rolls[PC_MAX_ROLL_VALUES];

    uint8_t veiled_option_count;
    uint32_t veiled_option_mod_ids[PC_MAX_VEILED_OPTIONS];
    uint32_t veiled_chosen_mod_id; /* PC_MOD_NONE until chosen */
} pc_mod_slot;

typedef struct pc_item_state {
    uint8_t rarity; /* pc_rarity */
    uint8_t quality;
    uint8_t item_flags; /* pc_item_flags */

    uint8_t prefix_count;
    uint8_t suffix_count;
    uint8_t implicit_count;
    uint8_t enchantment_count;

    pc_mod_slot prefixes[PC_MAX_PREFIXES];
    pc_mod_slot suffixes[PC_MAX_SUFFIXES];
    pc_mod_slot implicits[PC_MAX_IMPLICITS];
    pc_mod_slot enchantments[PC_MAX_ENCHANTS];

    uint8_t generic_influence_bits;
    uint8_t searing_exarch_tier;
    uint8_t eater_of_worlds_tier;

    uint8_t socket_count;
    uint8_t socket_colors[PC_MAX_SOCKETS];
    uint8_t link_mask;
} pc_item_state;

/* Reset an item to an empty normal item. Never fails for a non-null pointer. */
pc_result pc_item_clear(pc_item_state* item);

/*
 * Slot helpers. Each returns PC_RESULT_OK on success and leaves *item
 * unchanged on any failure (full side, bad index, null pointer). group_id is
 * caller-supplied because it is owned by SessionData; pass 0 when no session
 * is involved (unit tests). slot_out, when non-null, receives a pointer to the
 * written slot so callers can set rolls/flags.
 */
pc_result pc_item_add_mod(
    pc_item_state* item,
    int side,
    uint32_t mod_id,
    uint16_t group_id,
    uint8_t flags,
    pc_mod_slot** slot_out);

pc_result pc_item_remove_at(pc_item_state* item, int side, uint32_t index);

pc_result pc_item_clear_side(pc_item_state* item, int side);

/*
 * Compact a side in place: collapse PC_MOD_NONE holes toward the front and
 * fix the side count. Returns the live count via out_count when non-null.
 */
pc_result pc_item_compact_side(
    pc_item_state* item,
    int side,
    uint8_t* out_count);

/* Derived helpers; return 0 on a null pointer. */
uint8_t pc_item_max_prefix(const pc_item_state* item);
uint8_t pc_item_max_suffix(const pc_item_state* item);
int pc_item_can_add_prefix(const pc_item_state* item);
int pc_item_can_add_suffix(const pc_item_state* item);

/* Find the first fractured/veiled explicit slot. Returns PC_RESULT_NOT_FOUND
 * when none exists; on success writes the side and index. */
pc_result pc_item_find_fractured(
    const pc_item_state* item,
    int* out_side,
    uint32_t* out_index);
pc_result pc_item_find_veiled(
    const pc_item_state* item,
    int* out_side,
    uint32_t* out_index);

#ifdef __cplusplus
}
#endif

#endif
