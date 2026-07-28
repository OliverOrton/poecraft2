#include "engine_internal.hpp"

#include "poecraft/bitset.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstring>
#include <limits>
#include <string>
#include <unordered_set>
#include <utility>

namespace poecraft {

namespace {

constexpr std::int32_t kFlagEssenceOnly = 1 << 0;
constexpr std::int32_t kFlagCrafted = 1 << 1;
constexpr std::int32_t kFlagMetamod = 1 << 2;
constexpr std::int32_t kFlagDelve = 1 << 4;
constexpr std::int32_t kFlagImplicit = 1 << 5;
constexpr std::int32_t kFlagCorruptedImplicit = 1 << 6;
constexpr std::int32_t kFlagEldritchImplicit = 1 << 7;
constexpr std::int32_t kFlagVeiledTemplate = 1 << 8;
constexpr std::int32_t kFlagUnveiled = 1 << 9;
constexpr std::uint32_t kNoMod = std::numeric_limits<std::uint32_t>::max();
constexpr std::uint32_t kNoTag = std::numeric_limits<std::uint32_t>::max();
constexpr std::size_t kMaxPoolCacheEntries = 4096;

struct Match {
    std::int32_t weight = 0;
    std::uint32_t tag_id = kNoTag;
    std::int32_t ordinal = -1;
};

struct WeightView {
    const std::vector<std::uint32_t>* spawn;
    const std::vector<std::uint32_t>* generation;
    const std::vector<std::uint32_t>* base;
    const std::vector<std::uint32_t>* spawn_tag;
    const std::vector<std::uint32_t>* generation_tag;
    const std::vector<std::int32_t>* spawn_row;
    const std::vector<std::int32_t>* generation_row;
    const std::vector<std::uint64_t>* positive_spawn;
    const std::vector<std::uint64_t>* positive_base;
};

std::string normalize_key(const std::string& value) {
    std::string out;
    bool pending_underscore = false;
    for (char raw : value) {
        const char c = static_cast<char>(
            std::tolower(static_cast<unsigned char>(raw)));
        const bool alnum = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
        if (alnum) {
            if (pending_underscore && !out.empty()) out.push_back('_');
            pending_underscore = false;
            out.push_back(c);
        } else {
            pending_underscore = true;
        }
    }
    return out;
}

std::string signature_key(const std::vector<std::uint32_t>& tags) {
    std::string key;
    key.resize(tags.size() * sizeof(std::uint32_t));
    if (!tags.empty()) {
        std::memcpy(key.data(), tags.data(), key.size());
    }
    return key;
}

Match first_matching(
    const std::vector<std::uint32_t>& offsets,
    const std::vector<std::uint32_t>& tag_ids,
    const std::vector<std::int32_t>& weights,
    std::uint32_t mod_pos,
    const std::unordered_set<std::uint32_t>& tag_set,
    std::int32_t fallback) {
    const std::uint32_t begin = offsets[mod_pos];
    const std::uint32_t end = offsets[mod_pos + 1];
    for (std::uint32_t i = begin; i < end; ++i) {
        if (tag_set.count(tag_ids[i])) {
            return {weights[i], tag_ids[i],
                    static_cast<std::int32_t>(i - begin)};
        }
    }
    return {fallback, kNoTag, -1};
}

void alloc_mask(std::vector<std::uint64_t>& mask, std::size_t words) {
    mask.assign(words, 0);
}

void or_into(
    std::vector<std::uint64_t>& out,
    const std::vector<std::uint64_t>& value) {
    for (std::size_t i = 0; i < out.size(); ++i) out[i] |= value[i];
}

bool has_class_tag(
    const SessionImpl& session,
    std::uint32_t mod_id,
    std::uint32_t tag_id) {
    for (std::uint32_t i = session.class_offsets[mod_id];
         i < session.class_offsets[mod_id + 1]; ++i) {
        if (session.class_tag_ids[i] == tag_id) return true;
    }
    return false;
}

std::uint32_t first_blocking_group(
    const SessionImpl& session,
    std::uint32_t mod_id,
    const pc_item_state* item) {
    if (item == nullptr) return kNoMod;
    std::unordered_set<std::uint32_t> occupied;
    auto add_slot = [&](const pc_mod_slot& slot) {
        if (slot.mod_id == PC_MOD_NONE) return;
        if (slot.mod_id < session.mod_count) {
            for (std::uint32_t i = session.group_offsets[slot.mod_id];
                 i < session.group_offsets[slot.mod_id + 1]; ++i) {
                occupied.insert(session.group_ids[i]);
            }
        } else {
            occupied.insert(slot.group_id);
        }
    };
    for (std::uint8_t i = 0; i < item->prefix_count; ++i) {
        add_slot(item->prefixes[i]);
    }
    for (std::uint8_t i = 0; i < item->suffix_count; ++i) {
        add_slot(item->suffixes[i]);
    }
    for (std::uint32_t i = session.group_offsets[mod_id];
         i < session.group_offsets[mod_id + 1]; ++i) {
        if (occupied.count(session.group_ids[i])) return session.group_ids[i];
    }
    return kNoMod;
}

void build_current_group_block_mask(
    ActionContextImpl& context,
    const std::vector<std::uint32_t>& occupied_groups) {
    const SessionImpl& session = *context.session;
    context.block_mask_scratch.assign(session.words, 0);
    for (std::uint32_t group : occupied_groups) {
        if (group < session.group_masks.size()) {
            const auto& mask = session.group_masks[group];
            if (!mask.empty()) {
                or_into(context.block_mask_scratch, mask);
            }
        }
    }
}

void collect_occupied_groups(
    const SessionImpl& session,
    const pc_item_state* item,
    std::vector<std::uint32_t>& out) {
    out.clear();
    if (item == nullptr) return;
    auto add_slot = [&](const pc_mod_slot& slot) {
        if (slot.mod_id == PC_MOD_NONE) return;
        if (slot.mod_id < session.mod_count) {
            out.insert(
                out.end(),
                session.group_ids.begin() + session.group_offsets[slot.mod_id],
                session.group_ids.begin() +
                    session.group_offsets[slot.mod_id + 1]);
        } else {
            out.push_back(slot.group_id);
        }
    };
    for (std::uint8_t i = 0; i < item->prefix_count; ++i) {
        add_slot(item->prefixes[i]);
    }
    for (std::uint8_t i = 0; i < item->suffix_count; ++i) {
        add_slot(item->suffixes[i]);
    }
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
}

std::vector<std::uint64_t> influence_allowed_mask(
    const SessionImpl& session,
    const pc_item_state* item) {
    std::vector<std::uint64_t> out(session.words, 0);
    if (!session.influence_masks.empty()) out = session.influence_masks[0];
    const std::uint8_t bits = item != nullptr ? item->generic_influence_bits : 0;
    for (std::size_t code = 1; code < session.influence_masks.size(); ++code) {
        if (code <= 8 && (bits & (std::uint8_t{1} << (code - 1)))) {
            or_into(out, session.influence_masks[code]);
        }
    }
    return out;
}

void fill_weight_table(
    const SessionImpl& session,
    const std::vector<std::uint32_t>& tags,
    WeightTable& table) {
    const DataImpl& d = *session.data;
    std::unordered_set<std::uint32_t> tag_set(tags.begin(), tags.end());
    table.tag_ids = tags;
    table.spawn_weight.assign(session.mod_count, 0);
    table.generation_pct.assign(session.mod_count, 100);
    table.base_roll_weight.assign(session.mod_count, 0);
    table.spawn_tag_id.assign(session.mod_count, kNoTag);
    table.generation_tag_id.assign(session.mod_count, kNoTag);
    table.spawn_row_ordinal.assign(session.mod_count, -1);
    table.generation_row_ordinal.assign(session.mod_count, -1);
    alloc_mask(table.positive_spawn_mask, session.words);
    alloc_mask(table.positive_base_mask, session.words);

    for (std::uint32_t s = 0; s < session.mod_count; ++s) {
        const std::uint32_t p = session.global_index[s];
        const Match spawn = first_matching(
            d.spawn_offsets, d.spawn_tag_ids, d.spawn_weights, p, tag_set, 0);
        const Match generation = first_matching(
            d.gen_offsets, d.gen_tag_ids, d.gen_weights, p, tag_set, 100);
        table.spawn_tag_id[s] = spawn.tag_id;
        table.generation_tag_id[s] = generation.tag_id;
        table.spawn_row_ordinal[s] = spawn.ordinal;
        table.generation_row_ordinal[s] = generation.ordinal;
        table.generation_pct[s] =
            generation.weight > 0
                ? static_cast<std::uint32_t>(generation.weight)
                : 0;
        if (spawn.weight <= 0) continue;
        table.spawn_weight[s] = static_cast<std::uint32_t>(spawn.weight);
        pc_bitset_set(table.positive_spawn_mask.data(), s);
        const std::int64_t weight =
            (static_cast<std::int64_t>(spawn.weight) * generation.weight) / 100;
        if (weight > 0) {
            table.base_roll_weight[s] = static_cast<std::uint32_t>(weight);
            pc_bitset_set(table.positive_base_mask.data(), s);
        }
    }
    pc_bitset_zero_padding(table.positive_spawn_mask.data(), session.mod_count);
    pc_bitset_zero_padding(table.positive_base_mask.data(), session.mod_count);
}

WeightView weight_view(
    const ActionContextImpl& context,
    std::uint32_t signature_id) {
    const SessionImpl& s = *context.session;
    if (signature_id == 0) {
        static const std::vector<std::uint32_t> no_tags;
        static const std::vector<std::int32_t> no_rows;
        return {&s.base_spawn_weight,
                &s.base_gen_pct,
                &s.base_roll_weight,
                &no_tags,
                &no_tags,
                &no_rows,
                &no_rows,
                &s.positive_spawn_weight_mask,
                &s.positive_base_weight_mask};
    }
    const WeightTable& t = context.uncommon_weight_tables[signature_id - 1];
    return {&t.spawn_weight,
            &t.generation_pct,
            &t.base_roll_weight,
            &t.spawn_tag_id,
            &t.generation_tag_id,
            &t.spawn_row_ordinal,
            &t.generation_row_ordinal,
            &t.positive_spawn_mask,
            &t.positive_base_mask};
}

std::uint32_t base_active_tag(
    const SessionImpl& session,
    std::uint32_t mod_id,
    bool generation,
    std::int32_t* out_row) {
    const DataImpl& d = *session.data;
    const std::uint32_t p = session.global_index[mod_id];
    std::unordered_set<std::uint32_t> tags(
        session.effective_base_tag_ids.begin(),
        session.effective_base_tag_ids.end());
    const Match match = generation
                            ? first_matching(d.gen_offsets, d.gen_tag_ids,
                                             d.gen_weights, p, tags, 100)
                            : first_matching(d.spawn_offsets, d.spawn_tag_ids,
                                             d.spawn_weights, p, tags, 0);
    if (out_row != nullptr) *out_row = match.ordinal;
    return match.tag_id;
}

std::uint64_t mix_hash(std::uint64_t hash, std::uint64_t value) {
    hash ^= value + 0x9e3779b97f4a7c15ULL + (hash << 6) + (hash >> 2);
    return hash;
}

constexpr std::uint64_t kFossilMultiplierScale = 1'000'000;

std::uint64_t fossil_multiplier_fixed(
    const SessionImpl& session,
    std::uint32_t mod_id,
    const std::vector<std::uint32_t>& fossils) {
    const DataImpl& d = *session.data;
    std::uint64_t multiplier = kFossilMultiplierScale;
    for (std::uint32_t fossil : fossils) {
        for (std::uint32_t i = d.fossil_weight_offsets[fossil];
             i < d.fossil_weight_offsets[fossil + 1]; ++i) {
            const int kind = d.fossil_weight_kind_codes[i];
            if (kind != d.fossil_weight_negative_code &&
                kind != d.fossil_weight_positive_code) {
                continue;
            }
            if (!has_class_tag(session, mod_id, d.fossil_weight_tag_ids[i])) {
                continue;
            }
            const std::int32_t value = d.fossil_weight_values[i];
            if (value <= 0) return 0;
            const std::uint64_t unsigned_value =
                static_cast<std::uint32_t>(value);
            if (multiplier >
                std::numeric_limits<std::uint64_t>::max() / unsigned_value) {
                return std::numeric_limits<std::uint64_t>::max();
            }
            multiplier = (multiplier * unsigned_value) / 100;
        }
    }
    return multiplier;
}

std::uint64_t apply_fossil_multiplier(
    std::uint64_t weight,
    std::uint64_t multiplier_fixed) {
    if (weight == 0 || multiplier_fixed == 0) return 0;
    if (weight >
        std::numeric_limits<std::uint64_t>::max() / multiplier_fixed) {
        return std::numeric_limits<std::uint64_t>::max();
    }
    return (weight * multiplier_fixed) / kFossilMultiplierScale;
}

std::uint32_t fossil_multiplier_debug_pct(std::uint64_t multiplier_fixed) {
    const std::uint64_t pct =
        multiplier_fixed / (kFossilMultiplierScale / 100);
    return static_cast<std::uint32_t>(
        std::min<std::uint64_t>(
            pct, std::numeric_limits<std::uint32_t>::max()));
}

bool mask_test(const std::vector<std::uint64_t>& mask, std::uint32_t id) {
    return !mask.empty() && pc_bitset_test(mask.data(), id);
}

bool item_has_metamod(
    const SessionImpl& session,
    const pc_item_state* item,
    int metamod_code) {
    if (item == nullptr || metamod_code < 0) return false;
    auto scan = [&](const pc_mod_slot* slots, std::uint8_t count) {
        for (std::uint8_t i = 0; i < count; ++i) {
            const std::uint32_t id = slots[i].mod_id;
            if (id < session.metamod_type.size() &&
                session.metamod_type[id] == metamod_code) {
                return true;
            }
        }
        return false;
    };
    return scan(item->prefixes, item->prefix_count) ||
           scan(item->suffixes, item->suffix_count);
}

void apply_metamod_pool_blocks(
    const SessionImpl& session,
    bool block_attack,
    bool block_caster,
    std::vector<std::uint64_t>& candidate) {
    const DataImpl& d = *session.data;
    auto block_tag = [&](const char* name) {
        const auto tag = d.tag_id_by_name.find(name);
        if (tag == d.tag_id_by_name.end()) return;
        if (tag->second >= session.implicit_tag_masks.size()) return;
        const auto& mask = session.implicit_tag_masks[tag->second];
        if (mask.empty()) return;
        pc_bitset_andnot(candidate.data(), candidate.data(),
                         mask.data(), session.words);
    };
    if (block_attack) block_tag("attack");
    if (block_caster) block_tag("caster");
}

} // namespace

std::size_t hash_pool_cache_parts(
    const std::vector<std::uint64_t>& candidate_mask,
    std::uint32_t tag_signature_id,
    PoolWeightKind weight_kind,
    std::uint32_t target_tag_id,
    const std::vector<std::uint32_t>& fossil_indices) {
    std::uint64_t hash = 1469598103934665603ULL;
    for (std::uint64_t word : candidate_mask) {
        hash ^= word;
        hash *= 1099511628211ULL;
    }
    hash = mix_hash(hash, tag_signature_id);
    hash = mix_hash(hash, static_cast<std::uint64_t>(weight_kind));
    hash = mix_hash(hash, target_tag_id);
    for (std::uint32_t fossil : fossil_indices) {
        hash = mix_hash(hash, fossil);
    }
    return static_cast<std::size_t>(hash);
}

std::size_t PoolCacheKeyHash::operator()(const PoolCacheKey& key) const {
    return hash_pool_cache_parts(
        key.candidate_mask, key.tag_signature_id, key.weight_kind,
        key.target_tag_id, key.fossil_indices);
}

std::size_t PoolCacheKeyHash::operator()(const PoolCacheLookup& key) const {
    return hash_pool_cache_parts(
        *key.candidate_mask, key.tag_signature_id, key.weight_kind,
        key.target_tag_id, *key.fossil_indices);
}

bool PoolCacheKeyEqual::operator()(
    const PoolCacheKey& a,
    const PoolCacheLookup& b) const {
    return a.candidate_mask == *b.candidate_mask &&
           a.tag_signature_id == b.tag_signature_id &&
           a.weight_kind == b.weight_kind &&
           a.target_tag_id == b.target_tag_id &&
           a.fossil_indices == *b.fossil_indices;
}

std::size_t hash_refill_pool_cache_parts(
    const std::vector<std::uint64_t>& group_block_mask,
    std::uint32_t tag_signature_id,
    PoolWeightKind weight_kind,
    std::uint32_t target_tag_id,
    std::uint8_t influence_bits,
    std::int8_t side_filter,
    std::int8_t influence_only_code,
    bool block_attack,
    bool block_caster,
    bool respects_metamod_pool_blocks,
    const std::vector<std::uint32_t>& fossil_indices) {
    std::uint64_t hash = 1469598103934665603ULL;
    for (std::uint64_t word : group_block_mask) {
        hash ^= word;
        hash *= 1099511628211ULL;
    }
    hash = mix_hash(hash, tag_signature_id);
    hash = mix_hash(hash, static_cast<std::uint64_t>(weight_kind));
    hash = mix_hash(hash, target_tag_id);
    hash = mix_hash(hash, influence_bits);
    hash = mix_hash(
        hash, static_cast<std::uint64_t>(side_filter + 1));
    hash = mix_hash(
        hash, static_cast<std::uint64_t>(influence_only_code + 1));
    hash = mix_hash(hash, block_attack);
    hash = mix_hash(hash, block_caster);
    hash = mix_hash(hash, respects_metamod_pool_blocks);
    for (std::uint32_t fossil : fossil_indices) {
        hash = mix_hash(hash, fossil);
    }
    return static_cast<std::size_t>(hash);
}

std::size_t RefillPoolCacheHash::operator()(
    const RefillPoolCacheKey& key) const {
    return hash_refill_pool_cache_parts(
        key.group_block_mask, key.tag_signature_id, key.weight_kind,
        key.target_tag_id, key.influence_bits, key.side_filter,
        key.influence_only_code, key.block_attack, key.block_caster,
        key.respects_metamod_pool_blocks, key.fossil_indices);
}

std::size_t RefillPoolCacheHash::operator()(
    const RefillPoolCacheLookup& key) const {
    return hash_refill_pool_cache_parts(
        *key.group_block_mask, key.tag_signature_id, key.weight_kind,
        key.target_tag_id, key.influence_bits, key.side_filter,
        key.influence_only_code, key.block_attack, key.block_caster,
        key.respects_metamod_pool_blocks, *key.fossil_indices);
}

bool RefillPoolCacheEqual::operator()(
    const RefillPoolCacheKey& a,
    const RefillPoolCacheLookup& b) const {
    return a.group_block_mask == *b.group_block_mask &&
           a.tag_signature_id == b.tag_signature_id &&
           a.weight_kind == b.weight_kind &&
           a.target_tag_id == b.target_tag_id &&
           a.influence_bits == b.influence_bits &&
           a.side_filter == b.side_filter &&
           a.influence_only_code == b.influence_only_code &&
           a.block_attack == b.block_attack &&
           a.block_caster == b.block_caster &&
           a.respects_metamod_pool_blocks ==
               b.respects_metamod_pool_blocks &&
           a.fossil_indices == *b.fossil_indices;
}

void build_session(SessionImpl& session) {
    const DataImpl& d = *session.data;
    const std::uint32_t base_index = session.base_index;
    const std::uint32_t item_class_id = d.base_item_class_id[base_index];

    std::unordered_set<std::uint32_t> base_set;
    for (std::uint32_t i = d.base_tag_offsets[base_index];
         i < d.base_tag_offsets[base_index + 1]; ++i) {
        base_set.insert(d.base_tag_ids[i]);
    }
    session.effective_base_tag_ids.assign(base_set.begin(), base_set.end());
    std::sort(session.effective_base_tag_ids.begin(),
              session.effective_base_tag_ids.end());
    const auto default_tag = d.tag_id_by_name.find("default");
    const std::uint32_t default_tag_id =
        default_tag == d.tag_id_by_name.end() ? kNoTag : default_tag->second;

    std::string item_class_key;
    const auto class_it = d.item_class_index_by_id.find(item_class_id);
    if (class_it != d.item_class_index_by_id.end()) {
        item_class_key = d.string_at(d.item_class_key_sid[class_it->second]);
    }
    session.rare_affix_cap =
        (item_class_key == "Jewel" || item_class_key == "AbyssJewel") ? 2 : 3;
    session.eldritch_eligible =
        item_class_key == "Helmet" || item_class_key == "Body Armour" ||
        item_class_key == "Gloves" || item_class_key == "Boots";

    const std::string class_selector = normalize_key(item_class_key);
    session.selector_tag_by_influence.assign(
        d.influence_name_by_code.size(), -1);
    for (std::size_t code = 1; code < d.influence_name_by_code.size(); ++code) {
        const auto it = d.tag_id_by_name.find(
            class_selector + "_" + d.influence_name_by_code[code]);
        if (it != d.tag_id_by_name.end()) {
            session.selector_tag_by_influence[code] = it->second;
        }
    }

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

    std::vector<std::uint32_t> essence_global_ids(d.essence_count, kNoMod);
    std::vector<std::vector<std::uint32_t>> fossil_added_globals(d.fossil_count);
    std::vector<std::vector<std::uint32_t>> fossil_forced_globals(d.fossil_count);
    std::vector<std::vector<std::uint32_t>> fossil_sell_globals(d.fossil_count);
    std::vector<std::uint32_t> base_implicit_globals;
    std::vector<std::vector<std::uint32_t>> searing_globals(5);
    std::vector<std::vector<std::uint32_t>> eater_globals(5);

    auto add_mod = [&](std::uint32_t p, ReachKind kind,
                       const std::string& via) {
        const std::uint32_t global = d.mod_global_ids[p];
        if (session.session_id_by_global_id.count(global)) return;
        const std::uint32_t s =
            static_cast<std::uint32_t>(session.global_index.size());
        session.session_id_by_global_id.emplace(global, s);
        session.global_index.push_back(p);
        const int gen = d.mod_gen_type_code[p];
        session.gen_type.push_back(
            gen == d.gen_prefix_code ? 0 : gen == d.gen_suffix_code ? 1 : -1);
        session.primary_group.push_back(d.mod_primary_group[p]);
        session.flags.push_back(d.mod_flags[p]);
        session.metamod_type.push_back(d.mod_metamod_type_code[p]);
        session.special_kind.push_back(d.mod_special_kind_code[p]);
        session.required_level.push_back(d.mod_required_level[p]);
        session.influence_code.push_back(d.mod_influence_code[p]);
        session.reach_kind.push_back(static_cast<std::uint8_t>(kind));
        session.reach_influence.push_back(
            kind == ReachKind::Influence ? d.mod_influence_code[p] : -1);
        session.reach_via.push_back(via);
    };

    // Base and reachable influence explicit rows.
    for (std::uint32_t p = 0; p < d.mod_count; ++p) {
        const int gen = d.mod_gen_type_code[p];
        if (gen != d.gen_prefix_code && gen != d.gen_suffix_code) continue;
        if (!allowed_domains.count(d.mod_domain_code[p])) continue;
        if (d.mod_required_level[p] > session.item_level) continue;
        const std::int32_t flags = d.mod_flags[p];
        if (flags & (kFlagCrafted | kFlagEssenceOnly | kFlagDelve)) continue;
        const Match base_spawn = first_matching(
            d.spawn_offsets, d.spawn_tag_ids, d.spawn_weights, p, base_set, 0);
        if (base_spawn.weight > 0) {
            add_mod(p, ReachKind::Base, "base");
            continue;
        }
        const int influence = d.mod_influence_code[p];
        if (influence <= 0 ||
            static_cast<std::size_t>(influence) >=
                session.selector_tag_by_influence.size()) {
            continue;
        }
        const std::int64_t selector =
            session.selector_tag_by_influence[influence];
        if (selector < 0) continue;
        std::unordered_set<std::uint32_t> influenced = base_set;
        influenced.insert(static_cast<std::uint32_t>(selector));
        const Match influenced_spawn = first_matching(
            d.spawn_offsets, d.spawn_tag_ids, d.spawn_weights, p, influenced,
            0);
        if (influenced_spawn.weight > 0) {
            add_mod(p, ReachKind::Influence,
                    "influence:" + d.influence_name_by_code[influence]);
        }
    }

    // Crafted mods available through a bench option for this item class.
    for (std::size_t option = 0; option < d.bench_global_mod_ids.size();
         ++option) {
        if (d.bench_global_mod_ids[option] < 0) continue;
        bool class_match = false;
        for (std::uint32_t i = d.bench_class_offsets[option];
             i < d.bench_class_offsets[option + 1]; ++i) {
            if (d.bench_class_global_ids[i] ==
                static_cast<std::int32_t>(item_class_id)) {
                class_match = true;
                break;
            }
        }
        if (!class_match) continue;
        const auto it = d.mod_pos_by_global_id.find(
            static_cast<std::uint32_t>(d.bench_global_mod_ids[option]));
        if (it != d.mod_pos_by_global_id.end()) {
            add_mod(it->second, ReachKind::Crafted, "crafted:bench");
        }
    }

    // Direct mechanic registries. They remain outside normal random rolling,
    // but are part of the session universe so every runtime uses dense ids.
    for (std::uint32_t p = 0; p < d.mod_count; ++p) {
        if (d.mod_required_level[p] > session.item_level) continue;
        const int special = d.mod_special_kind_code[p];
        const std::string& key = d.string_at(d.mod_key_sid[p]);
        if (special == d.special_veiled_template_code) {
            if (key == "VeiledPrefix" || key == "VeiledSuffix") {
                add_mod(p, ReachKind::Veiled, "veiled:template");
            }
            continue;
        }
        if (special == d.special_unveiled_code) {
            const Match spawn = first_matching(
                d.spawn_offsets, d.spawn_tag_ids, d.spawn_weights, p, base_set,
                0);
            if (spawn.weight > 0 && spawn.tag_id != default_tag_id) {
                add_mod(p, ReachKind::Unveiled, "veiled:unveil");
            }
            continue;
        }
        if (special == d.special_corrupted_implicit_code) {
            const Match spawn = first_matching(
                d.spawn_offsets, d.spawn_tag_ids, d.spawn_weights, p, base_set,
                0);
            if (spawn.weight > 0 && spawn.tag_id != default_tag_id) {
                add_mod(p, ReachKind::CorruptedImplicit,
                        "implicit:corrupted");
            }
            continue;
        }
        if (special == d.special_eldritch_implicit_code &&
            session.eldritch_eligible) {
            const bool searing =
                d.mod_gen_type_code[p] == d.gen_searing_implicit_code;
            const bool eater =
                d.mod_gen_type_code[p] == d.gen_eater_implicit_code;
            if (!searing && !eater) continue;
            for (std::uint32_t tier = 1; tier <= 4; ++tier) {
                std::unordered_set<std::uint32_t> tags = base_set;
                const auto tier_tag = d.tag_id_by_name.find(
                    "no_tier_" + std::to_string(tier) +
                    "_eldritch_implicit");
                if (tier_tag != d.tag_id_by_name.end()) {
                    tags.insert(tier_tag->second);
                }
                const Match spawn = first_matching(
                    d.spawn_offsets, d.spawn_tag_ids, d.spawn_weights, p, tags,
                    0);
                if (spawn.weight <= 0 || spawn.tag_id == default_tag_id)
                    continue;
                add_mod(p, ReachKind::EldritchImplicit,
                        searing ? "implicit:searing_exarch"
                                : "implicit:eater_of_worlds");
                (searing ? searing_globals[tier] : eater_globals[tier])
                    .push_back(d.mod_global_ids[p]);
            }
        }
    }

    // Essence guaranteed mods for this item class.
    const std::string normalized_class = normalize_key(item_class_key);
    for (std::uint32_t essence = 0; essence < d.essence_count; ++essence) {
        for (std::uint32_t i = d.essence_mod_offsets[essence];
             i < d.essence_mod_offsets[essence + 1]; ++i) {
            if (normalize_key(d.string_at(d.essence_class_key_sids[i])) !=
                normalized_class) {
                continue;
            }
            if (d.essence_linked_global_mod_ids[i] < 0) continue;
            const std::uint32_t global =
                static_cast<std::uint32_t>(d.essence_linked_global_mod_ids[i]);
            essence_global_ids[essence] = global;
            const auto it = d.mod_pos_by_global_id.find(global);
            if (it != d.mod_pos_by_global_id.end()) {
                add_mod(it->second, ReachKind::Essence, "essence");
            }
            break;
        }
    }

    // Base implicits are direct additions during item initialization.
    for (std::uint32_t i = d.base_implicit_offsets[base_index];
         i < d.base_implicit_offsets[base_index + 1]; ++i) {
        if (d.base_implicit_global_mod_ids[i] < 0) continue;
        const std::uint32_t global =
            static_cast<std::uint32_t>(d.base_implicit_global_mod_ids[i]);
        const auto it = d.mod_pos_by_global_id.find(global);
        if (it != d.mod_pos_by_global_id.end()) {
            add_mod(it->second, ReachKind::BaseImplicit, "base_implicit");
            base_implicit_globals.push_back(global);
        }
    }

    // Fossil added/forced mods, filtered by the selected base signature.
    for (std::uint32_t fossil = 0; fossil < d.fossil_count; ++fossil) {
        for (std::uint32_t i = d.fossil_mod_offsets[fossil];
             i < d.fossil_mod_offsets[fossil + 1]; ++i) {
            const int kind = d.fossil_mod_kind_codes[i];
            if (kind != d.fossil_mod_added_code &&
                kind != d.fossil_mod_forced_code &&
                kind != d.fossil_mod_sell_price_code) {
                continue;
            }
            if (d.fossil_linked_global_mod_ids[i] < 0) continue;
            const std::uint32_t global =
                static_cast<std::uint32_t>(d.fossil_linked_global_mod_ids[i]);
            const auto it = d.mod_pos_by_global_id.find(global);
            if (it == d.mod_pos_by_global_id.end()) continue;
            const std::uint32_t p = it->second;
            if (d.mod_required_level[p] > session.item_level) continue;
            if (kind == d.fossil_mod_sell_price_code) {
                add_mod(p, ReachKind::Fossil, "fossil:sell_price");
                fossil_sell_globals[fossil].push_back(global);
                continue;
            }
            const Match spawn = first_matching(
                d.spawn_offsets, d.spawn_tag_ids, d.spawn_weights, p, base_set,
                0);
            if (spawn.weight <= 0) continue;
            add_mod(p, ReachKind::Fossil, "fossil");
            auto& target = kind == d.fossil_mod_added_code
                               ? fossil_added_globals[fossil]
                               : fossil_forced_globals[fossil];
            target.push_back(global);
        }
    }

    session.mod_count = static_cast<std::uint32_t>(session.global_index.size());
    session.words = pc_bitset_words(session.mod_count);
    if (!d.mod_group_ids_flat.empty()) {
        session.group_masks.resize(
            static_cast<std::size_t>(
                *std::max_element(
                    d.mod_group_ids_flat.begin(),
                    d.mod_group_ids_flat.end())) +
            1);
    }
    if (!d.class_tag_ids.empty()) {
        session.implicit_tag_masks.resize(
            static_cast<std::size_t>(
                *std::max_element(
                    d.class_tag_ids.begin(), d.class_tag_ids.end())) +
            1);
    }
    session.influence_masks.assign(d.influence_name_by_code.size(),
                                   std::vector<std::uint64_t>(
                                       session.words, 0));
    alloc_mask(session.universe_mask, session.words);
    alloc_mask(session.prefix_mask, session.words);
    alloc_mask(session.suffix_mask, session.words);
    alloc_mask(session.base_explicit_universe_mask, session.words);
    alloc_mask(session.normal_random_roll_mask, session.words);
    alloc_mask(session.crafted_mask, session.words);
    alloc_mask(session.essence_only_mask, session.words);
    alloc_mask(session.implicit_mask, session.words);
    alloc_mask(session.delve_mask, session.words);
    alloc_mask(session.veiled_template_mask, session.words);
    alloc_mask(session.unveiled_mask, session.words);
    alloc_mask(session.unveiled_generic_mask, session.words);
    alloc_mask(session.corrupted_implicit_mask, session.words);
    alloc_mask(session.eldritch_implicit_mask, session.words);
    alloc_mask(session.eldritch_searing_mask, session.words);
    alloc_mask(session.eldritch_eater_mask, session.words);

    session.class_offsets.assign(session.mod_count + 1, 0);
    session.classification_tag_name_ptrs.resize(session.mod_count);
    session.group_offsets.assign(session.mod_count + 1, 0);
    for (std::uint32_t s = 0; s < session.mod_count; ++s) {
        const std::uint32_t p = session.global_index[s];
        for (std::uint32_t i = d.class_offsets[p]; i < d.class_offsets[p + 1];
             ++i) {
            const std::uint32_t tag = d.class_tag_ids[i];
            session.class_tag_ids.push_back(tag);
            const auto name = d.tag_name_by_id.find(tag);
            if (name != d.tag_name_by_id.end()) {
                session.classification_tag_name_ptrs[s].push_back(
                    name->second.c_str());
            }
            auto& mask = session.implicit_tag_masks[tag];
            if (mask.empty()) alloc_mask(mask, session.words);
            pc_bitset_set(mask.data(), s);
        }
        session.class_offsets[s + 1] =
            static_cast<std::uint32_t>(session.class_tag_ids.size());

        for (std::uint32_t i = d.mod_group_offsets[p];
             i < d.mod_group_offsets[p + 1]; ++i) {
            const std::uint32_t group = d.mod_group_ids_flat[i];
            session.group_ids.push_back(group);
            auto& mask = session.group_masks[group];
            if (mask.empty()) alloc_mask(mask, session.words);
            pc_bitset_set(mask.data(), s);
        }
        session.group_offsets[s + 1] =
            static_cast<std::uint32_t>(session.group_ids.size());

        pc_bitset_set(session.universe_mask.data(), s);
        if (session.gen_type[s] == 0) {
            pc_bitset_set(session.prefix_mask.data(), s);
        } else if (session.gen_type[s] == 1) {
            pc_bitset_set(session.suffix_mask.data(), s);
        }
        const ReachKind reach =
            static_cast<ReachKind>(session.reach_kind[s]);
        if (reach == ReachKind::Base || reach == ReachKind::Influence) {
            pc_bitset_set(session.base_explicit_universe_mask.data(), s);
            pc_bitset_set(session.normal_random_roll_mask.data(), s);
        }
        if (session.flags[s] & kFlagCrafted)
            pc_bitset_set(session.crafted_mask.data(), s);
        if (session.flags[s] & kFlagEssenceOnly)
            pc_bitset_set(session.essence_only_mask.data(), s);
        if (session.flags[s] & kFlagImplicit)
            pc_bitset_set(session.implicit_mask.data(), s);
        if (session.flags[s] & kFlagDelve)
            pc_bitset_set(session.delve_mask.data(), s);
        if (session.flags[s] & kFlagVeiledTemplate)
            pc_bitset_set(session.veiled_template_mask.data(), s);
        if (session.flags[s] & kFlagUnveiled) {
            pc_bitset_set(session.unveiled_mask.data(), s);
            bool named = false;
            for (std::uint32_t i = session.class_offsets[s];
                 i < session.class_offsets[s + 1]; ++i) {
                const auto it = d.tag_name_by_id.find(session.class_tag_ids[i]);
                if (it == d.tag_name_by_id.end()) continue;
                const std::string& tag = it->second;
                if (tag != "unveiled_mod" &&
                    (tag.find("_veiled_prefix") != std::string::npos ||
                     tag.find("_veiled_suffix") != std::string::npos)) {
                    named = true;
                    break;
                }
            }
            if (!named)
                pc_bitset_set(session.unveiled_generic_mask.data(), s);
        }
        if (session.flags[s] & kFlagCorruptedImplicit)
            pc_bitset_set(session.corrupted_implicit_mask.data(), s);
        if (session.flags[s] & kFlagEldritchImplicit) {
            pc_bitset_set(session.eldritch_implicit_mask.data(), s);
            const std::uint32_t p = session.global_index[s];
            if (d.mod_gen_type_code[p] == d.gen_searing_implicit_code)
                pc_bitset_set(session.eldritch_searing_mask.data(), s);
            if (d.mod_gen_type_code[p] == d.gen_eater_implicit_code)
                pc_bitset_set(session.eldritch_eater_mask.data(), s);
        }

        int influence = session.influence_code[s];
        if (influence < 0 ||
            static_cast<std::size_t>(influence) >=
                session.influence_masks.size()) {
            influence = 0;
        }
        pc_bitset_set(session.influence_masks[influence].data(), s);
    }

    WeightTable base_table;
    fill_weight_table(session, session.effective_base_tag_ids, base_table);
    session.base_spawn_weight = std::move(base_table.spawn_weight);
    session.base_gen_pct = std::move(base_table.generation_pct);
    session.base_roll_weight = std::move(base_table.base_roll_weight);
    session.positive_spawn_weight_mask =
        std::move(base_table.positive_spawn_mask);
    session.positive_base_weight_mask =
        std::move(base_table.positive_base_mask);

    session.base_implicit_mod_ids.clear();
    for (std::uint32_t global : base_implicit_globals) {
        session.base_implicit_mod_ids.push_back(
            session.session_id_by_global_id.at(global));
    }
    for (std::uint32_t s = 0; s < session.mod_count; ++s) {
        if (session.flags[s] & kFlagCrafted) {
            session.bench_mod_ids.push_back(s);
        }
        const std::string& key =
            d.string_at(d.mod_key_sid[session.global_index[s]]);
        if (key == "VeiledPrefix") session.veiled_prefix_mod_id = s;
        if (key == "VeiledSuffix") session.veiled_suffix_mod_id = s;
    }
    auto map_globals = [&](const std::vector<std::vector<std::uint32_t>>& src,
                           std::vector<std::vector<std::uint32_t>>& dst) {
        dst.resize(src.size());
        for (std::size_t i = 0; i < src.size(); ++i) {
            for (std::uint32_t global : src[i]) {
                const auto it = session.session_id_by_global_id.find(global);
                if (it != session.session_id_by_global_id.end())
                    dst[i].push_back(it->second);
            }
        }
    };
    map_globals(searing_globals, session.eldritch_searing_tier_mod_ids);
    map_globals(eater_globals, session.eldritch_eater_tier_mod_ids);
    map_globals(fossil_sell_globals, session.fossil_sell_price_mod_ids);
    session.essence_guaranteed_mod_ids.assign(d.essence_count, kNoMod);
    for (std::uint32_t i = 0; i < d.essence_count; ++i) {
        const auto it = session.session_id_by_global_id.find(essence_global_ids[i]);
        if (it != session.session_id_by_global_id.end()) {
            session.essence_guaranteed_mod_ids[i] = it->second;
        }
    }
    session.fossil_added_mod_ids.resize(d.fossil_count);
    session.fossil_forced_mod_ids.resize(d.fossil_count);
    for (std::uint32_t f = 0; f < d.fossil_count; ++f) {
        for (std::uint32_t global : fossil_added_globals[f]) {
            session.fossil_added_mod_ids[f].push_back(
                session.session_id_by_global_id.at(global));
        }
        for (std::uint32_t global : fossil_forced_globals[f]) {
            session.fossil_forced_mod_ids[f].push_back(
                session.session_id_by_global_id.at(global));
        }
    }

    // Build display families using the same identity as the old UI:
    // exclusion group + ordered stat signature, separated by generation side
    // and acquisition source. This must not replace primary_group for
    // exclusivity checks.
    session.family_id.assign(session.mod_count, 0);
    std::unordered_map<std::string, std::uint32_t> family_by_signature;
    for (std::uint32_t s = 0; s < session.mod_count; ++s) {
        const std::uint32_t p = session.global_index[s];
        std::vector<std::uint32_t> signature;
        signature.reserve(
            4 + (d.stat_offsets[p + 1] - d.stat_offsets[p]));
        signature.push_back(session.primary_group[s]);
        signature.push_back(
            static_cast<std::uint32_t>(session.gen_type[s] + 2));
        signature.push_back(
            static_cast<std::uint32_t>(session.reach_kind[s] + 1));
        signature.push_back(
            static_cast<std::uint32_t>(session.reach_influence[s] + 2));
        for (std::uint32_t i = d.stat_offsets[p]; i < d.stat_offsets[p + 1];
             ++i) {
            signature.push_back(d.stat_key_sids[i]);
        }
        const std::string key = signature_key(signature);
        const auto [it, inserted] = family_by_signature.emplace(
            key, static_cast<std::uint32_t>(family_by_signature.size()));
        session.family_id[s] = it->second;
        (void)inserted;
    }

    // Compute 1-based rank within each display family, ordered by required
    // level descending. Ties get the same rank.
    session.family_tier_index.assign(session.mod_count, 0);
    {
        std::unordered_map<std::uint32_t, std::vector<std::uint32_t>>
            by_family;
        for (std::uint32_t s = 0; s < session.mod_count; ++s) {
            by_family[session.family_id[s]].push_back(s);
        }
        for (auto& [_, members] : by_family) {
            std::sort(members.begin(), members.end(),
                      [&](std::uint32_t a, std::uint32_t b) {
                          return session.required_level[a] >
                                 session.required_level[b];
                      });
            std::uint32_t rank = 0;
            std::uint32_t last_level =
                std::numeric_limits<std::uint32_t>::max();
            for (std::uint32_t s : members) {
                if (session.required_level[s] != last_level) {
                    rank += 1;
                    last_level = session.required_level[s];
                }
                session.family_tier_index[s] = rank;
            }
        }
    }

    auto zero_padding = [&](std::vector<std::uint64_t>& mask) {
        pc_bitset_zero_padding(mask.data(), session.mod_count);
    };
    zero_padding(session.universe_mask);
    zero_padding(session.prefix_mask);
    zero_padding(session.suffix_mask);
    zero_padding(session.base_explicit_universe_mask);
    zero_padding(session.normal_random_roll_mask);
    zero_padding(session.crafted_mask);
    zero_padding(session.essence_only_mask);
    zero_padding(session.implicit_mask);
    zero_padding(session.delve_mask);
    zero_padding(session.veiled_template_mask);
    zero_padding(session.unveiled_mask);
    zero_padding(session.unveiled_generic_mask);
    zero_padding(session.corrupted_implicit_mask);
    zero_padding(session.eldritch_implicit_mask);
    zero_padding(session.eldritch_searing_mask);
    zero_padding(session.eldritch_eater_mask);
    for (auto& mask : session.group_masks)
        if (!mask.empty()) zero_padding(mask);
    for (auto& mask : session.implicit_tag_masks)
        if (!mask.empty()) zero_padding(mask);
    for (auto& mask : session.influence_masks) zero_padding(mask);
}

std::uint32_t intern_item_tag_signature(
    ActionContextImpl& context,
    const pc_item_state* item) {
    const SessionImpl& session = *context.session;
    const std::uint8_t bits = item != nullptr ? item->generic_influence_bits : 0;
    const std::uint32_t cached = context.signature_by_influence_bits[bits];
    if (cached != std::numeric_limits<std::uint32_t>::max()) return cached;
    if (context.signature_by_key.empty()) {
        context.signature_by_key.emplace(
            signature_key(session.effective_base_tag_ids), 0);
    }
    std::vector<std::uint32_t> tags = session.effective_base_tag_ids;
    for (std::size_t code = 1;
         code < session.selector_tag_by_influence.size() && code <= 8; ++code) {
        if ((bits & (std::uint8_t{1} << (code - 1))) &&
            session.selector_tag_by_influence[code] >= 0) {
            tags.push_back(static_cast<std::uint32_t>(
                session.selector_tag_by_influence[code]));
        }
    }
    std::sort(tags.begin(), tags.end());
    tags.erase(std::unique(tags.begin(), tags.end()), tags.end());
    const std::string key = signature_key(tags);
    const auto found = context.signature_by_key.find(key);
    if (found != context.signature_by_key.end()) {
        context.signature_by_influence_bits[bits] = found->second;
        return found->second;
    }

    WeightTable table;
    fill_weight_table(session, tags, table);
    const std::uint32_t id =
        static_cast<std::uint32_t>(context.uncommon_weight_tables.size() + 1);
    context.uncommon_weight_tables.push_back(std::move(table));
    context.signature_by_key.emplace(key, id);
    context.signature_by_influence_bits[bits] = id;
    return id;
}

const WeightedPool& get_weighted_pool(
    ActionContextImpl& context,
    const pc_item_state* item,
    const PoolBuildRequest& request,
    bool* out_cache_hit,
    const PoolBuildHints* hints) {
    const SessionImpl& session = *context.session;
    const std::uint32_t signature_id =
        intern_item_tag_signature(context, item);
    const WeightView weights = weight_view(context, signature_id);
    const bool use_refill_cache =
        hints != nullptr && hints->group_block_mask != nullptr;
    RefillPoolCacheLookup refill_lookup;
    if (use_refill_cache) {
        refill_lookup = {
            hints->group_block_mask,
            signature_id,
            request.weight_kind,
            request.target_tag_id,
            static_cast<std::uint8_t>(
                item != nullptr ? item->generic_influence_bits : 0),
            static_cast<std::int8_t>(request.side_filter),
            static_cast<std::int8_t>(request.influence_only_code),
            hints->block_attack,
            hints->block_caster,
            request.respects_metamod_pool_blocks,
            &request.fossil_indices};
        if (context.last_refill_pool_key != nullptr &&
            context.last_refill_pool != nullptr &&
            RefillPoolCacheEqual{}(
                *context.last_refill_pool_key, refill_lookup)) {
            ++context.pool_cache_hits;
            if (out_cache_hit != nullptr) *out_cache_hit = true;
            return *context.last_refill_pool;
        }
        const auto refill_found =
            context.refill_pool_cache.find(refill_lookup);
        if (refill_found != context.refill_pool_cache.end()) {
            context.last_refill_pool_key = &refill_found->first;
            context.last_refill_pool = refill_found->second;
            ++context.pool_cache_hits;
            if (out_cache_hit != nullptr) *out_cache_hit = true;
            return *refill_found->second;
        }
    }
    auto remember_refill_pool = [&](const WeightedPool* pool) {
        if (!use_refill_cache) return;
        if (context.refill_pool_cache.size() >= kMaxPoolCacheEntries) {
            context.last_refill_pool_key = nullptr;
            context.last_refill_pool = nullptr;
            context.refill_pool_cache.clear();
        }
        RefillPoolCacheKey key;
        key.group_block_mask = *refill_lookup.group_block_mask;
        key.tag_signature_id = refill_lookup.tag_signature_id;
        key.weight_kind = refill_lookup.weight_kind;
        key.target_tag_id = refill_lookup.target_tag_id;
        key.influence_bits = refill_lookup.influence_bits;
        key.side_filter = refill_lookup.side_filter;
        key.influence_only_code = refill_lookup.influence_only_code;
        key.block_attack = refill_lookup.block_attack;
        key.block_caster = refill_lookup.block_caster;
        key.respects_metamod_pool_blocks =
            refill_lookup.respects_metamod_pool_blocks;
        key.fossil_indices = *refill_lookup.fossil_indices;
        const auto inserted =
            context.refill_pool_cache.emplace(std::move(key), pool).first;
        context.last_refill_pool_key = &inserted->first;
        context.last_refill_pool = inserted->second;
    };

    std::chrono::steady_clock::time_point candidate_started;
    if (context.perf_timing_enabled) {
        candidate_started = std::chrono::steady_clock::now();
    }

    std::vector<std::uint64_t>& candidate = context.candidate_mask_scratch;
    if (request.weight_kind == PoolWeightKind::Fossil) {
        candidate = session.normal_random_roll_mask;
        for (std::uint32_t fossil : request.fossil_indices) {
            for (std::uint32_t mod : session.fossil_added_mod_ids[fossil]) {
                pc_bitset_set(candidate.data(), mod);
            }
        }
    } else {
        candidate = session.normal_random_roll_mask;
    }

    if (request.side_filter == 0) {
        pc_bitset_and(candidate.data(), candidate.data(),
                      session.prefix_mask.data(), session.words);
    } else if (request.side_filter == 1) {
        pc_bitset_and(candidate.data(), candidate.data(),
                      session.suffix_mask.data(), session.words);
    }

    const std::vector<std::uint64_t>* positive =
        weights.positive_base;
    pc_bitset_and(candidate.data(), candidate.data(), positive->data(),
                  session.words);

    if (request.weight_kind == PoolWeightKind::TargetedNatural) {
        if (request.target_tag_id >= session.implicit_tag_masks.size() ||
            session.implicit_tag_masks[request.target_tag_id].empty()) {
            pc_bitset_zero(candidate.data(), session.words);
        } else {
            pc_bitset_and(candidate.data(), candidate.data(),
                          session.implicit_tag_masks[request.target_tag_id].data(),
                          session.words);
        }
    }

    if (request.influence_only_code >= 0 &&
        static_cast<std::size_t>(request.influence_only_code) <
            session.influence_masks.size()) {
        pc_bitset_and(
            candidate.data(), candidate.data(),
            session.influence_masks[request.influence_only_code].data(),
            session.words);
    } else {
        if (session.influence_masks.empty()) {
            context.influence_mask_scratch.assign(session.words, 0);
        } else {
            context.influence_mask_scratch = session.influence_masks[0];
        }
        const std::uint8_t influence_bits =
            item != nullptr ? item->generic_influence_bits : 0;
        for (std::size_t code = 1;
             code < session.influence_masks.size() && code <= 8; ++code) {
            if (influence_bits & (std::uint8_t{1} << (code - 1))) {
                or_into(context.influence_mask_scratch,
                        session.influence_masks[code]);
            }
        }
        pc_bitset_and(candidate.data(), candidate.data(),
                      context.influence_mask_scratch.data(), session.words);
    }
    if (request.respects_metamod_pool_blocks) {
        apply_metamod_pool_blocks(
            session,
            hints != nullptr
                ? hints->block_attack
                : item_has_metamod(
                      session, item, session.data->metamod_no_attack_code),
            hints != nullptr
                ? hints->block_caster
                : item_has_metamod(
                      session, item, session.data->metamod_no_caster_code),
            candidate);
    }
    const std::vector<std::uint64_t>* group_block =
        hints != nullptr ? hints->group_block_mask : nullptr;
    if (group_block == nullptr) {
        collect_occupied_groups(
            session, item, context.occupied_groups_scratch);
        build_current_group_block_mask(
            context, context.occupied_groups_scratch);
        group_block = &context.block_mask_scratch;
    }
    pc_bitset_andnot(candidate.data(), candidate.data(),
                     group_block->data(), session.words);
    pc_bitset_zero_padding(candidate.data(), session.mod_count);

    if (context.perf_timing_enabled) {
        context.candidate_build_ns +=
            static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() - candidate_started)
                    .count());
    }
    const PoolCacheLookup lookup{
        &candidate,
        signature_id,
        request.weight_kind,
        request.target_tag_id,
        &request.fossil_indices};
    const auto found = context.pool_cache.find(lookup);
    if (found != context.pool_cache.end()) {
        ++context.pool_cache_hits;
        if (out_cache_hit != nullptr) *out_cache_hit = true;
        remember_refill_pool(&found->second);
        return found->second;
    }
    std::chrono::steady_clock::time_point pool_started;
    if (context.perf_timing_enabled) {
        pool_started = std::chrono::steady_clock::now();
    }
    WeightedPool pool;
    const std::size_t candidate_count =
        pc_bitset_count(candidate.data(), session.words);
    pool.entries.reserve(candidate_count);
    pool.prefix_sums.reserve(candidate_count);
    pc_bitset_for_each(candidate.data(), session.words, [&](std::size_t raw) {
        if (raw >= session.mod_count) return;
        const std::uint32_t s = static_cast<std::uint32_t>(raw);
        std::uint64_t final_weight = (*weights.base)[s];
        if (request.weight_kind == PoolWeightKind::Fossil) {
            final_weight = apply_fossil_multiplier(
                final_weight,
                fossil_multiplier_fixed(
                    session, s, request.fossil_indices));
            for (std::uint32_t fossil : request.fossil_indices) {
                if (fossil < session.data->fossil_rolls_lucky.size() &&
                    session.data->fossil_rolls_lucky[fossil] != 0) {
                    const std::uint64_t level_pct =
                        100 + (session.required_level[s] > 40
                                   ? session.required_level[s] - 40
                                   : 0);
                    final_weight = (final_weight * level_pct) / 100;
                }
            }
        }
        if (final_weight == 0) return;

        const std::uint32_t p = session.global_index[s];
        PoolEntry entry;
        entry.session_mod_id = s;
        entry.global_mod_id = session.data->mod_global_ids[p];
        entry.primary_group = session.primary_group[s];
        entry.gen_type = session.gen_type[s];
        entry.required_level = session.required_level[s];
        entry.spawn_weight = (*weights.spawn)[s];
        entry.generation_pct = (*weights.generation)[s];
        entry.final_weight = static_cast<std::uint32_t>(
            std::min<std::uint64_t>(
                final_weight, std::numeric_limits<std::uint32_t>::max()));
        pool.total_weight += entry.final_weight;
        if (entry.gen_type == 0) {
            pool.prefix_total_weight += entry.final_weight;
        } else if (entry.gen_type == 1) {
            pool.suffix_total_weight += entry.final_weight;
        }
        pool.entries.push_back(entry);
        pool.prefix_sums.push_back(pool.total_weight);
    });

    ++context.pool_cache_misses;
    if (context.perf_timing_enabled) {
        context.weighted_pool_build_ns +=
            static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() - pool_started)
                    .count());
    }
    if (out_cache_hit != nullptr) *out_cache_hit = false;
    if (context.pool_cache.size() >= kMaxPoolCacheEntries) {
        context.last_refill_pool_key = nullptr;
        context.last_refill_pool = nullptr;
        context.refill_pool_cache.clear();
        context.pool_cache.clear();
    }
    PoolCacheKey key;
    key.candidate_mask = candidate;
    key.tag_signature_id = signature_id;
    key.weight_kind = request.weight_kind;
    key.target_tag_id = request.target_tag_id;
    key.fossil_indices = request.fossil_indices;
    const auto inserted =
        context.pool_cache.emplace(std::move(key), std::move(pool)).first;
    remember_refill_pool(&inserted->second);
    return inserted->second;
}

void build_pool_debug_rows(
    ActionContextImpl& context,
    const pc_item_state* item,
    const PoolBuildRequest& request,
    bool include_rejected,
    std::vector<PoolDebugRow>& out_rows,
    WeightedPool* out_summary,
    bool* out_cache_hit) {
    const SessionImpl& session = *context.session;
    bool cache_hit = false;
    const WeightedPool& pool =
        get_weighted_pool(context, item, request, &cache_hit);
    if (out_summary != nullptr) *out_summary = pool;
    if (out_cache_hit != nullptr) *out_cache_hit = cache_hit;

    const std::uint32_t signature_id =
        intern_item_tag_signature(context, item);
    const WeightView weights = weight_view(context, signature_id);
    std::vector<std::uint64_t> influence;
    if (request.influence_only_code >= 0 &&
        static_cast<std::size_t>(request.influence_only_code) <
            session.influence_masks.size()) {
        influence = session.influence_masks[request.influence_only_code];
    } else {
        influence = influence_allowed_mask(session, item);
    }
    std::vector<std::uint64_t> metamod_allowed(session.words, ~std::uint64_t{0});
    pc_bitset_zero_padding(metamod_allowed.data(), session.mod_count);
    if (request.respects_metamod_pool_blocks) {
        apply_metamod_pool_blocks(
            session,
            item_has_metamod(
                session, item, session.data->metamod_no_attack_code),
            item_has_metamod(
                session, item, session.data->metamod_no_caster_code),
            metamod_allowed);
    }
    std::vector<std::uint32_t> occupied_groups;
    collect_occupied_groups(session, item, occupied_groups);
    build_current_group_block_mask(context, occupied_groups);
    std::unordered_map<std::uint32_t, const PoolEntry*> accepted;
    for (const PoolEntry& entry : pool.entries) {
        accepted.emplace(entry.session_mod_id, &entry);
    }

    out_rows.clear();
    for (std::uint32_t s = 0; s < session.mod_count; ++s) {
        const auto accepted_it = accepted.find(s);
        if (!include_rejected && accepted_it == accepted.end()) continue;
        PoolDebugRow row;
        row.tag_signature_id = signature_id;
        row.entry.session_mod_id = s;
        row.entry.global_mod_id =
            session.data->mod_global_ids[session.global_index[s]];
        row.entry.primary_group = session.primary_group[s];
        row.entry.gen_type = session.gen_type[s];
        row.entry.required_level = session.required_level[s];
        row.entry.spawn_weight = (*weights.spawn)[s];
        row.entry.generation_pct = (*weights.generation)[s];
        row.normal_random_member =
            mask_test(session.normal_random_roll_mask, s);
        row.side_allowed =
            request.side_filter < 0 ||
            (request.side_filter == 0 && session.gen_type[s] == 0) ||
            (request.side_filter == 1 && session.gen_type[s] == 1);
        row.influence_allowed = mask_test(influence, s);
        row.group_allowed = !mask_test(context.block_mask_scratch, s);
        row.blocking_group_id = row.group_allowed
                                    ? kNoMod
                                    : first_blocking_group(session, s, item);
        row.mechanic_allowed =
            row.normal_random_member && mask_test(metamod_allowed, s);
        std::uint64_t fossil_multiplier = kFossilMultiplierScale;
        if (request.weight_kind == PoolWeightKind::Fossil) {
            for (std::uint32_t fossil : request.fossil_indices) {
                if (std::find(session.fossil_added_mod_ids[fossil].begin(),
                              session.fossil_added_mod_ids[fossil].end(),
                              s) !=
                    session.fossil_added_mod_ids[fossil].end()) {
                    row.mechanic_allowed =
                        mask_test(metamod_allowed, s);
                }
            }
            fossil_multiplier =
                fossil_multiplier_fixed(
                    session, s, request.fossil_indices);
            for (std::uint32_t fossil : request.fossil_indices) {
                if (fossil < session.data->fossil_rolls_lucky.size() &&
                    session.data->fossil_rolls_lucky[fossil] != 0) {
                    const std::uint64_t level_pct =
                        100 + (session.required_level[s] > 40
                                   ? session.required_level[s] - 40
                                   : 0);
                    fossil_multiplier =
                        (fossil_multiplier * level_pct) / 100;
                }
            }
            row.special_multiplier_pct =
                fossil_multiplier_debug_pct(fossil_multiplier);
        } else if (request.weight_kind ==
                   PoolWeightKind::TargetedNatural) {
            row.mechanic_allowed =
                row.normal_random_member &&
                has_class_tag(session, s, request.target_tag_id) &&
                mask_test(metamod_allowed, s);
        }
        row.positively_weighted =
            (*weights.base)[s] > 0 && fossil_multiplier > 0;
        if (signature_id == 0) {
            row.active_spawn_tag_id =
                base_active_tag(session, s, false, &row.active_spawn_row);
            row.active_generation_tag_id =
                base_active_tag(session, s, true, &row.active_generation_row);
        } else {
            row.active_spawn_tag_id = (*weights.spawn_tag)[s];
            row.active_generation_tag_id = (*weights.generation_tag)[s];
            row.active_spawn_row = (*weights.spawn_row)[s];
            row.active_generation_row = (*weights.generation_row)[s];
        }
        if (!row.normal_random_member &&
            request.weight_kind != PoolWeightKind::Fossil) {
            row.first_failure = 1;
        } else if (!row.side_allowed) {
            row.first_failure = 2;
        } else if (!row.mechanic_allowed) {
            row.first_failure = 3;
        } else if (!row.influence_allowed) {
            row.first_failure = 4;
        } else if (!row.group_allowed) {
            row.first_failure = 5;
        } else if (!row.positively_weighted) {
            row.first_failure = 6;
        }
        if (accepted_it != accepted.end()) {
            row.entry = *accepted_it->second;
            row.first_failure = 0;
        }
        out_rows.push_back(row);
    }
}

} // namespace poecraft
