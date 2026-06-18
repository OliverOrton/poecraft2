#include "engine_internal.hpp"

#include "poecraft/bitset.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_set>
#include <vector>

/*
 * Generic session builder for ordinary (non-cluster) bases. This is a faithful
 * C++ port of tools/ingest/poecraft_ingest/engine_selection.py so the engine's
 * session universe and weighted pools match the spec fixtures exactly.
 */
namespace poecraft {

namespace {

// Mirrors compiled_data.py FLAG_* bit positions.
constexpr std::int32_t kFlagEssenceOnly = 1 << 0;
constexpr std::int32_t kFlagCrafted = 1 << 1;

// engine_selection.py _normalize_item_class: lowercase, collapse runs of
// non-alphanumeric characters to a single underscore, strip leading/trailing.
std::string normalize_item_class(const std::string& value) {
    std::string out;
    bool pending_underscore = false;
    for (char raw : value) {
        const char c = static_cast<char>(
            std::tolower(static_cast<unsigned char>(raw)));
        const bool alnum = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
        if (alnum) {
            if (pending_underscore && !out.empty()) {
                out.push_back('_');
            }
            pending_underscore = false;
            out.push_back(c);
        } else {
            pending_underscore = true;
        }
    }
    return out;
}

// First-matching weight over a mod's ordered rows. A row matches when its tag
// is in base_set or equals extra_tag (used for the influence selector tag).
std::int32_t first_matching(
    const std::vector<std::uint32_t>& offsets,
    const std::vector<std::uint32_t>& tag_ids,
    const std::vector<std::int32_t>& weights,
    std::uint32_t mod_pos,
    const std::unordered_set<std::uint32_t>& base_set,
    std::int64_t extra_tag,
    std::int32_t fallback) {
    const std::uint32_t begin = offsets[mod_pos];
    const std::uint32_t end = offsets[mod_pos + 1];
    for (std::uint32_t i = begin; i < end; ++i) {
        const std::uint32_t tag = tag_ids[i];
        if (base_set.count(tag) ||
            (extra_tag >= 0 && tag == static_cast<std::uint32_t>(extra_tag))) {
            return weights[i];
        }
    }
    return fallback;
}

// Union of every exclusivity group occupied by the item's live explicit mods.
// Each slot's mod is a dense session id, so its full group list is read from the
// session; if a slot ever holds a mod outside the universe, its cached primary
// group_id is used as a fallback.
std::unordered_set<std::uint32_t> build_block_set(
    const SessionImpl& session,
    const pc_item_state* item) {
    std::unordered_set<std::uint32_t> block;
    if (item == nullptr) {
        return block;
    }
    auto add_slot = [&](const pc_mod_slot& slot) {
        if (slot.mod_id == PC_MOD_NONE) {
            return;
        }
        if (slot.mod_id < session.mod_count) {
            for (std::uint32_t i = session.group_offsets[slot.mod_id];
                 i < session.group_offsets[slot.mod_id + 1]; ++i) {
                block.insert(session.group_ids[i]);
            }
        } else {
            block.insert(slot.group_id);
        }
    };
    for (std::uint8_t i = 0; i < item->prefix_count; ++i) {
        add_slot(item->prefixes[i]);
    }
    for (std::uint8_t i = 0; i < item->suffix_count; ++i) {
        add_slot(item->suffixes[i]);
    }
    return block;
}

// A candidate is blocked when any of its exclusivity groups is already occupied.
bool candidate_blocked(
    const SessionImpl& session,
    std::uint32_t s,
    const std::unordered_set<std::uint32_t>& block) {
    if (block.empty()) {
        return false;
    }
    for (std::uint32_t i = session.group_offsets[s];
         i < session.group_offsets[s + 1]; ++i) {
        if (block.count(session.group_ids[i])) {
            return true;
        }
    }
    return false;
}

} // namespace

void build_session(SessionImpl& session) {
    const DataImpl& d = *session.data;
    const std::uint32_t base_index = session.base_index;

    // Base effective tag set (already includes "default" from ingest).
    std::unordered_set<std::uint32_t> base_set;
    const std::uint32_t tag_begin = d.base_tag_offsets[base_index];
    const std::uint32_t tag_end = d.base_tag_offsets[base_index + 1];
    for (std::uint32_t i = tag_begin; i < tag_end; ++i) {
        base_set.insert(d.base_tag_ids[i]);
    }
    session.effective_base_tag_ids.assign(base_set.begin(), base_set.end());
    std::sort(session.effective_base_tag_ids.begin(),
              session.effective_base_tag_ids.end());

    // Item class key and normalized influence selector tags per influence code.
    std::string item_class_key;
    const auto class_it =
        d.item_class_index_by_id.find(d.base_item_class_id[base_index]);
    if (class_it != d.item_class_index_by_id.end()) {
        item_class_key = d.string_at(d.item_class_key_sid[class_it->second]);
    }
    // Rare jewels and abyss jewels cap at 2 affixes per side; everything else 3.
    session.rare_affix_cap =
        (item_class_key == "Jewel" || item_class_key == "AbyssJewel") ? 2 : 3;

    const std::string class_selector = normalize_item_class(item_class_key);
    std::vector<std::int64_t> selector_tag_by_influence(
        d.influence_name_by_code.size(), -1);
    for (std::size_t code = 1; code < d.influence_name_by_code.size(); ++code) {
        const std::string selector =
            class_selector + "_" + d.influence_name_by_code[code];
        const auto tag_it = d.tag_id_by_name.find(selector);
        if (tag_it != d.tag_id_by_name.end()) {
            selector_tag_by_influence[code] =
                static_cast<std::int64_t>(tag_it->second);
        }
    }

    // Allowed source domains for the base/item class.
    std::unordered_set<int> allowed_domains;
    if (item_class_key == "Jewel") {
        const auto it = d.domain_code_by_name.find("misc");
        if (it != d.domain_code_by_name.end()) allowed_domains.insert(it->second);
    } else if (item_class_key == "AbyssJewel") {
        const auto it = d.domain_code_by_name.find("abyss_jewel");
        if (it != d.domain_code_by_name.end()) allowed_domains.insert(it->second);
    } else {
        allowed_domains.insert(d.base_domain_code[base_index]);
    }

    // --- select the dense session universe ---------------------------------
    for (std::uint32_t p = 0; p < d.mod_count; ++p) {
        const std::int32_t gen_code = d.mod_gen_type_code[p];
        if (gen_code != d.gen_prefix_code && gen_code != d.gen_suffix_code) {
            continue;
        }
        if (allowed_domains.count(d.mod_domain_code[p]) == 0) {
            continue;
        }
        if (d.mod_required_level[p] > session.item_level) {
            continue;
        }
        const std::int32_t flags = d.mod_flags[p];
        if ((flags & kFlagCrafted) || (flags & kFlagEssenceOnly)) {
            continue;
        }

        const std::int32_t base_spawn = first_matching(
            d.spawn_offsets, d.spawn_tag_ids, d.spawn_weights, p, base_set, -1,
            0);
        const std::int32_t influence = d.mod_influence_code[p]; // 0 == none

        ReachKind kind = ReachKind::Base;
        std::int32_t reach_influence = -1;
        std::string reach_via = "base";
        if (base_spawn <= 0) {
            if (influence <= 0) {
                continue; // not reachable from base and has no influence
            }
            const std::int64_t selector = selector_tag_by_influence[influence];
            if (selector < 0) {
                continue; // no selector tag for this influence on this class
            }
            const std::int32_t influence_spawn = first_matching(
                d.spawn_offsets, d.spawn_tag_ids, d.spawn_weights, p, base_set,
                selector, 0);
            if (influence_spawn <= 0) {
                continue;
            }
            kind = ReachKind::Influence;
            reach_influence = influence;
            reach_via = "influence:" + d.influence_name_by_code[influence];
        }

        session.global_index.push_back(p);
        session.gen_type.push_back(gen_code == d.gen_prefix_code ? 0 : 1);
        session.primary_group.push_back(d.mod_primary_group[p]);
        session.required_level.push_back(d.mod_required_level[p]);
        session.influence_code.push_back(influence);
        session.reach_kind.push_back(static_cast<std::uint8_t>(kind));
        session.reach_influence.push_back(reach_influence);
        session.reach_via.push_back(std::move(reach_via));
    }

    session.mod_count = static_cast<std::uint32_t>(session.global_index.size());
    session.words = pc_bitset_words(session.mod_count);

    // --- masks and base-signature weights ----------------------------------
    auto alloc = [&](std::vector<std::uint64_t>& mask) {
        mask.assign(session.words, 0);
    };
    alloc(session.universe_mask);
    alloc(session.prefix_mask);
    alloc(session.suffix_mask);
    alloc(session.normal_random_roll_mask);
    alloc(session.positive_spawn_weight_mask);
    alloc(session.positive_base_weight_mask);

    session.base_spawn_weight.resize(session.mod_count);
    session.base_gen_pct.resize(session.mod_count);
    session.base_roll_weight.resize(session.mod_count);

    // classification tags per session mod, packed with offsets
    session.class_offsets.assign(session.mod_count + 1, 0);
    for (std::uint32_t s = 0; s < session.mod_count; ++s) {
        const std::uint32_t p = session.global_index[s];
        for (std::uint32_t i = d.class_offsets[p]; i < d.class_offsets[p + 1];
             ++i) {
            session.class_tag_ids.push_back(d.class_tag_ids[i]);
        }
        session.class_offsets[s + 1] =
            static_cast<std::uint32_t>(session.class_tag_ids.size());
    }

    // full exclusivity-group membership per session mod, packed with offsets
    session.group_offsets.assign(session.mod_count + 1, 0);
    for (std::uint32_t s = 0; s < session.mod_count; ++s) {
        const std::uint32_t p = session.global_index[s];
        for (std::uint32_t i = d.mod_group_offsets[p];
             i < d.mod_group_offsets[p + 1]; ++i) {
            session.group_ids.push_back(d.mod_group_ids_flat[i]);
        }
        session.group_offsets[s + 1] =
            static_cast<std::uint32_t>(session.group_ids.size());
    }

    for (std::uint32_t s = 0; s < session.mod_count; ++s) {
        const std::uint32_t p = session.global_index[s];
        pc_bitset_set(session.universe_mask.data(), s);
        if (session.gen_type[s] == 0) {
            pc_bitset_set(session.prefix_mask.data(), s);
        } else {
            pc_bitset_set(session.suffix_mask.data(), s);
        }
        // Every universe mod here is a normal explicit roll (crafted, essence,
        // implicit and special mods are excluded above).
        pc_bitset_set(session.normal_random_roll_mask.data(), s);

        const std::int32_t spawn = first_matching(
            d.spawn_offsets, d.spawn_tag_ids, d.spawn_weights, p, base_set, -1,
            0);
        const std::int32_t gen = first_matching(
            d.gen_offsets, d.gen_tag_ids, d.gen_weights, p, base_set, -1, 100);
        const std::int32_t final_weight =
            spawn > 0 ? static_cast<std::int32_t>(
                            (static_cast<std::int64_t>(spawn) * gen) / 100)
                      : 0;

        session.base_spawn_weight[s] =
            spawn > 0 ? static_cast<std::uint32_t>(spawn) : 0;
        session.base_gen_pct[s] = static_cast<std::uint32_t>(gen);
        session.base_roll_weight[s] =
            final_weight > 0 ? static_cast<std::uint32_t>(final_weight) : 0;

        if (spawn > 0) {
            pc_bitset_set(session.positive_spawn_weight_mask.data(), s);
        }
        if (final_weight > 0) {
            pc_bitset_set(session.positive_base_weight_mask.data(), s);
        }
    }

    pc_bitset_zero_padding(session.universe_mask.data(), session.mod_count);
    pc_bitset_zero_padding(session.prefix_mask.data(), session.mod_count);
    pc_bitset_zero_padding(session.suffix_mask.data(), session.mod_count);
    pc_bitset_zero_padding(session.normal_random_roll_mask.data(),
                           session.mod_count);
    pc_bitset_zero_padding(session.positive_spawn_weight_mask.data(),
                           session.mod_count);
    pc_bitset_zero_padding(session.positive_base_weight_mask.data(),
                           session.mod_count);
}

std::vector<PoolEntry> build_normal_pool(
    const SessionImpl& session,
    const pc_item_state* item,
    int side_filter) {
    const DataImpl& d = *session.data;
    const std::unordered_set<std::uint32_t> block =
        build_block_set(session, item);

    std::vector<PoolEntry> pool;
    for (std::uint32_t s = 0; s < session.mod_count; ++s) {
        const std::uint8_t side = session.gen_type[s];
        if (side_filter == 0 && side != 0) continue;
        if (side_filter == 1 && side != 1) continue;
        if (!pc_bitset_test(session.positive_base_weight_mask.data(), s)) {
            continue; // weight <= 0
        }
        if (candidate_blocked(session, s, block)) {
            continue;
        }
        PoolEntry entry;
        entry.session_mod_id = s;
        entry.global_mod_id = d.mod_global_ids[session.global_index[s]];
        entry.primary_group = session.primary_group[s];
        entry.gen_type = side;
        entry.required_level = session.required_level[s];
        entry.spawn_weight = session.base_spawn_weight[s];
        entry.generation_pct = session.base_gen_pct[s];
        entry.final_weight = session.base_roll_weight[s];
        pool.push_back(entry);
    }
    return pool;
}

std::vector<PoolEntry> build_harvest_pool(
    const SessionImpl& session,
    const pc_item_state* item,
    std::uint32_t target_tag_id,
    int side_filter) {
    const DataImpl& d = *session.data;
    const std::unordered_set<std::uint32_t> block =
        build_block_set(session, item);

    auto has_tag = [&](std::uint32_t s) {
        for (std::uint32_t i = session.class_offsets[s];
             i < session.class_offsets[s + 1]; ++i) {
            if (session.class_tag_ids[i] == target_tag_id) return true;
        }
        return false;
    };

    std::vector<PoolEntry> pool;
    for (std::uint32_t s = 0; s < session.mod_count; ++s) {
        const std::uint8_t side = session.gen_type[s];
        if (side_filter == 0 && side != 0) continue;
        if (side_filter == 1 && side != 1) continue;
        // Harvest targeting uses active spawn weight (> 0), not base roll
        // weight; generation multipliers do not apply.
        if (!pc_bitset_test(session.positive_spawn_weight_mask.data(), s)) {
            continue;
        }
        if (!has_tag(s)) continue;
        if (candidate_blocked(session, s, block)) continue;
        PoolEntry entry;
        entry.session_mod_id = s;
        entry.global_mod_id = d.mod_global_ids[session.global_index[s]];
        entry.primary_group = session.primary_group[s];
        entry.gen_type = side;
        entry.required_level = session.required_level[s];
        entry.spawn_weight = session.base_spawn_weight[s];
        entry.generation_pct = 100; // not applied for Harvest targeting
        entry.final_weight = session.base_spawn_weight[s];
        pool.push_back(entry);
    }
    return pool;
}

} // namespace poecraft
