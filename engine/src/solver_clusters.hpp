#pragma once
#include "currency_outcomes.hpp"
#include "poecraft/bitset.h"
#include <algorithm>
#include <stdexcept>
#include <tuple>
#include <vector>

namespace poecraft::solver {
inline void validate_cluster_exact_item(const SessionImpl& session, const pc_item_state& item) {
    if (!session.is_cluster() || item.lifecycle != PC_ITEM_LIVE || item.enchantment_count ||
        item.memory_strands || item.implicit_count || item.quality || item.socket_count ||
        item.link_mask || item.generic_influence_bits || item.searing_exarch_tier ||
        item.eater_of_worlds_tier || item.rarity > PC_RARITY_RARE ||
        (item.item_flags & ~(PC_ITEM_CORRUPTED | PC_ITEM_MIRRORED | PC_ITEM_SPLIT | PC_ITEM_SYNTHESISED)) ||
        item.prefix_count > session.rare_affix_cap || item.suffix_count > session.rare_affix_cap ||
        (item.rarity == PC_RARITY_NORMAL && (item.prefix_count || item.suffix_count)) ||
        (item.rarity == PC_RARITY_MAGIC && (item.prefix_count > 1 || item.suffix_count > 1)))
        throw std::invalid_argument("Configured cluster exact carrier has unsupported item context; implicit, socket, quality, strand, influence and absent states are refused");
    std::vector<std::uint32_t> seen;
    bool prefix_lock = false, suffix_lock = false;
    for (int side : {PC_SIDE_PREFIX, PC_SIDE_SUFFIX}) {
        const auto* slots = side == PC_SIDE_PREFIX ? item.prefixes : item.suffixes;
        const auto count = side == PC_SIDE_PREFIX ? item.prefix_count : item.suffix_count;
        for (unsigned i = 0; i < count; ++i) {
            const auto& slot = slots[i];
            if (slot.mod_id >= session.mod_count || session.gen_type[slot.mod_id] != side ||
                !pc_bitset_test(session.base_explicit_universe_mask.data(), slot.mod_id) ||
                slot.group_id != session.primary_group[slot.mod_id] ||
                (slot.flags & ~(PC_MOD_SLOT_FRACTURED | PC_MOD_SLOT_CRAFTED)) || slot.veiled_option_count)
                throw std::invalid_argument("Configured cluster exact carrier requires native explicit identity, side, group and supported flags");
            for (const auto previous : seen)
                for (auto a = session.group_offsets[previous]; a < session.group_offsets[previous + 1]; ++a)
                    for (auto b = session.group_offsets[slot.mod_id]; b < session.group_offsets[slot.mod_id + 1]; ++b)
                        if (session.group_ids[a] == session.group_ids[b])
                            throw std::invalid_argument("Configured cluster exact carrier has conflicting native groups");
            seen.push_back(slot.mod_id);
            const auto metamod = session.metamod_type[slot.mod_id];
            prefix_lock |= metamod >= 0 && metamod == session.data->metamod_prefixes_locked_code;
            suffix_lock |= metamod >= 0 && metamod == session.data->metamod_suffixes_locked_code;
        }
    }
    if (prefix_lock && suffix_lock)
        throw std::invalid_argument("Configured cluster exact carrier refuses the owner-unapproved dual-lock Scour law");
}
inline auto cluster_exact_affixes(const pc_item_state& item) {
    std::vector<std::tuple<int, std::uint32_t, std::uint16_t, std::uint8_t>> result;
    for (int side : {PC_SIDE_PREFIX, PC_SIDE_SUFFIX}) {
        const auto* slots = side == PC_SIDE_PREFIX ? item.prefixes : item.suffixes;
        const auto count = side == PC_SIDE_PREFIX ? item.prefix_count : item.suffix_count;
        for (unsigned i = 0; i < count; ++i)
            result.emplace_back(side, slots[i].mod_id, slots[i].group_id, slots[i].flags);
    }
    std::sort(result.begin(), result.end()); return result;
}
inline bool same_cluster_exact_item(const pc_item_state& a, const pc_item_state& b) {
    return a.rarity == b.rarity && a.item_flags == b.item_flags &&
        cluster_exact_affixes(a) == cluster_exact_affixes(b);
}
inline std::string cluster_strategy_identity_fields(const SessionImpl& session) {
    if (!session.is_cluster()) return {};
    const auto& c = session.data->clusters[session.cluster_index];
    const auto key = session.data->string_at(c.passives[session.cluster_passive_index].key_sid);
    std::string escaped;
    for (char ch : key) { if (ch == '\"' || ch == '\\') escaped += '\\'; escaped += ch; }
    return ",\"cluster\":{\"passive_key\":\"" + escaped + "\",\"passive_count\":" +
        std::to_string(session.cluster_passive_count) + "}";
}
} // namespace poecraft::solver
