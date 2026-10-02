#pragma once

#include "engine_internal.hpp"
#include <algorithm>
#include <array>
#include <stdexcept>
#include <tuple>
#include <vector>

namespace poecraft::solver {

inline bool authored_dominance_action(const ActionType type) {
    return type == ActionType::Dominance || type == ActionType::Annul ||
           type == ActionType::Scour || type == ActionType::RemoveCraftedModifiers;
}

// Exact structural scope. Numeric rolls cannot be observed by this graph
// vocabulary; Dominance's native elevated-value reroll is integrated out.
inline void validate_authored_dominance_item(
        const SessionImpl& session, const pc_item_state& item) {
    if (item.lifecycle != PC_ITEM_LIVE || item.enchantment_count ||
        item.memory_strands || item.implicit_count || item.quality ||
        item.socket_count || item.link_mask || item.searing_exarch_tier ||
        item.eater_of_worlds_tier || item.rarity > PC_RARITY_RARE ||
        (item.item_flags & ~(PC_ITEM_CORRUPTED | PC_ITEM_MIRRORED |
                            PC_ITEM_SPLIT | PC_ITEM_SYNTHESISED)) ||
        item.prefix_count > PC_MAX_PREFIXES || item.suffix_count > PC_MAX_SUFFIXES)
        throw std::invalid_argument("Authored Dominance has unsupported item context (strands, enchantment, implicit, socket, quality or absent state)");
    for (int side : {PC_SIDE_PREFIX, PC_SIDE_SUFFIX}) {
        const auto* slots = side == PC_SIDE_PREFIX ? item.prefixes : item.suffixes;
        const auto count = side == PC_SIDE_PREFIX ? item.prefix_count : item.suffix_count;
        for (std::uint8_t i = 0; i < count; ++i) {
            const auto& slot = slots[i];
            if (slot.mod_id >= session.mod_count ||
                session.gen_type[slot.mod_id] != side ||
                slot.group_id != session.primary_group[slot.mod_id] ||
                (slot.flags & ~(PC_MOD_SLOT_FRACTURED | PC_MOD_SLOT_CRAFTED)) ||
                slot.veiled_option_count)
                throw std::invalid_argument("Authored Dominance requires exact explicit affix side, identity and supported flags; Veiled is unsupported");
        }
    }
}

inline auto authored_dominance_affixes(const pc_item_state& item) {
    std::vector<std::tuple<int, std::uint32_t, std::uint16_t, std::uint8_t>> result;
    for (int side : {PC_SIDE_PREFIX, PC_SIDE_SUFFIX}) {
        const auto* slots = side == PC_SIDE_PREFIX ? item.prefixes : item.suffixes;
        const auto count = side == PC_SIDE_PREFIX ? item.prefix_count : item.suffix_count;
        for (std::uint8_t i = 0; i < count; ++i)
            result.emplace_back(side, slots[i].mod_id, slots[i].group_id, slots[i].flags);
    }
    std::sort(result.begin(), result.end());
    return result;
}

inline bool same_authored_dominance_item(
        const pc_item_state& a, const pc_item_state& b) {
    return a.rarity == b.rarity && a.item_flags == b.item_flags &&
           a.generic_influence_bits == b.generic_influence_bits &&
           authored_dominance_affixes(a) == authored_dominance_affixes(b);
}

} // namespace poecraft::solver
