#include "engine_internal.hpp"

#include "poecraft/bitset.h"
#include "poecraft/item_state.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <limits>
#include <string>
#include <unordered_set>
#include <vector>

/*
 * Core crafting actions. Actions mutate ItemState by sampling the Phase 5
 * normal explicit pool with the action context's RNG. Affix counts follow
 * standard Path of Exile behaviour (magic 1-2 mods, rare 4-6); the old app is a
 * reference, not a compatibility target, and no fixture pins exact RNG.
 *
 * Phase 13 direct mechanics share this same mutation path so native, Python,
 * WASM, and compiled strategies cannot drift.
 */
namespace poecraft {

namespace {

constexpr std::uint32_t kNoMod = std::numeric_limits<std::uint32_t>::max();
constexpr std::uint32_t kNoTag = std::numeric_limits<std::uint32_t>::max();

bool item_craftable(const pc_item_state* item) {
    return item != nullptr &&
           !(item->item_flags & (PC_ITEM_CORRUPTED | PC_ITEM_MIRRORED));
}

bool item_has_metamod(
    const SessionImpl& session,
    const pc_item_state* item,
    int code) {
    if (code < 0) return false;
    auto scan = [&](const pc_mod_slot* slots, std::uint8_t count) {
        for (std::uint8_t i = 0; i < count; ++i) {
            const std::uint32_t id = slots[i].mod_id;
            if (id < session.metamod_type.size() &&
                session.metamod_type[id] == code) {
                return true;
            }
        }
        return false;
    };
    return scan(item->prefixes, item->prefix_count) ||
           scan(item->suffixes, item->suffix_count);
}

bool side_locked(
    const SessionImpl& session,
    const pc_item_state* item,
    int side) {
    return item_has_metamod(
        session, item,
        side == PC_SIDE_PREFIX
            ? session.data->metamod_prefixes_locked_code
            : session.data->metamod_suffixes_locked_code);
}

bool has_class_tag(
    const SessionImpl& session,
    std::uint32_t mod_id,
    std::uint32_t tag_id) {
    if (mod_id >= session.mod_count) return false;
    for (std::uint32_t i = session.class_offsets[mod_id];
         i < session.class_offsets[mod_id + 1]; ++i) {
        if (session.class_tag_ids[i] == tag_id) return true;
    }
    return false;
}

bool groups_conflict(
    const SessionImpl& session,
    const pc_item_state* item,
    std::uint32_t mod_id,
    int skip_side = -1,
    std::uint32_t skip_index = kNoMod) {
    if (mod_id >= session.mod_count) return true;
    auto conflicts = [&](const pc_mod_slot& slot) {
        if (slot.mod_id >= session.mod_count) return false;
        for (std::uint32_t a = session.group_offsets[slot.mod_id];
             a < session.group_offsets[slot.mod_id + 1]; ++a) {
            for (std::uint32_t b = session.group_offsets[mod_id];
                 b < session.group_offsets[mod_id + 1]; ++b) {
                if (session.group_ids[a] == session.group_ids[b]) return true;
            }
        }
        return false;
    };
    for (std::uint8_t i = 0; i < item->prefix_count; ++i) {
        if (skip_side == PC_SIDE_PREFIX && skip_index == i) continue;
        if (conflicts(item->prefixes[i])) return true;
    }
    for (std::uint8_t i = 0; i < item->suffix_count; ++i) {
        if (skip_side == PC_SIDE_SUFFIX && skip_index == i) continue;
        if (conflicts(item->suffixes[i])) return true;
    }
    return false;
}

std::uint32_t active_spawn_weight(
    const SessionImpl& session,
    std::uint32_t mod_id,
    std::uint32_t extra_tag = kNoTag) {
    if (mod_id >= session.mod_count) return 0;
    std::unordered_set<std::uint32_t> tags(
        session.effective_base_tag_ids.begin(),
        session.effective_base_tag_ids.end());
    if (extra_tag != kNoTag) tags.insert(extra_tag);
    const DataImpl& d = *session.data;
    const std::uint32_t p = session.global_index[mod_id];
    for (std::uint32_t i = d.spawn_offsets[p];
         i < d.spawn_offsets[p + 1]; ++i) {
        if (tags.count(d.spawn_tag_ids[i])) {
            return d.spawn_weights[i] > 0
                       ? static_cast<std::uint32_t>(d.spawn_weights[i])
                       : 0;
        }
    }
    return 0;
}

std::uint32_t pick_weighted_id(
    ActionContextImpl& context,
    const pc_item_state* item,
    const std::vector<std::uint32_t>& ids,
    int side = -1,
    std::uint32_t extra_tag = kNoTag,
    int skip_side = -1,
    std::uint32_t skip_index = kNoMod) {
    const SessionImpl& session = *context.session;
    std::vector<std::pair<std::uint32_t, std::uint64_t>> candidates;
    std::uint64_t total = 0;
    for (std::uint32_t id : ids) {
        if (id >= session.mod_count) continue;
        if (side >= 0 && session.gen_type[id] != side) continue;
        if (item != nullptr &&
            groups_conflict(session, item, id, skip_side, skip_index)) {
            continue;
        }
        const std::uint32_t weight =
            active_spawn_weight(session, id, extra_tag);
        if (weight == 0) continue;
        total += weight;
        candidates.push_back({id, total});
    }
    if (total == 0) return kNoMod;
    const std::uint64_t roll = context.rng.next_below(total);
    const auto it = std::lower_bound(
        candidates.begin(), candidates.end(), roll + 1,
        [](const auto& entry, std::uint64_t value) {
            return entry.second < value;
        });
    return it == candidates.end() ? kNoMod : it->first;
}

std::uint8_t max_affix(const SessionImpl& session, const pc_item_state* item) {
    switch (item->rarity) {
    case PC_RARITY_NORMAL:
        return 0;
    case PC_RARITY_MAGIC:
        return 1;
    case PC_RARITY_RARE:
    default:
        return session.rare_affix_cap;
    }
}

int open_side_filter(
    const SessionImpl& session,
    const pc_item_state* item,
    const PoolBuildRequest& base_request) {
    const std::uint8_t cap = max_affix(session, item);
    const bool prefix_open =
        item->prefix_count < cap && base_request.side_filter != 1;
    const bool suffix_open =
        item->suffix_count < cap && base_request.side_filter != 0;
    if (prefix_open && suffix_open) return -1;
    if (prefix_open) return 0;
    if (suffix_open) return 1;
    return -2;
}

void record_weighted_pick(
    ActionContextImpl& context,
    const pc_item_state* item,
    const PoolBuildRequest& request,
    int side_filter,
    bool cache_hit,
    std::uint64_t prefix_total_weight,
    std::uint64_t suffix_total_weight,
    std::uint64_t total_weight,
    std::uint64_t roll,
    const PoolEntry& chosen) {
    if (!context.capture_action_trace) return;
    ActionTraceStage stage;
    stage.stage_index =
        static_cast<std::uint32_t>(context.last_action_trace.size());
    stage.cache_hit = cache_hit;
    stage.tag_signature_id = intern_item_tag_signature(context, item);
    stage.weight_kind = request.weight_kind;
    stage.side_filter = side_filter;
    stage.prefix_total_weight = prefix_total_weight;
    stage.suffix_total_weight = suffix_total_weight;
    stage.combined_total_weight = total_weight;
    stage.roll = roll;
    stage.chosen_mod_id = chosen.session_mod_id;
    stage.chosen_side = chosen.gen_type;
    context.last_action_trace.push_back(stage);
}

void or_mod_group_masks(
    const SessionImpl& session,
    std::uint32_t mod_id,
    std::vector<std::uint64_t>& block_mask) {
    if (mod_id >= session.mod_count) return;
    for (std::uint32_t i = session.group_offsets[mod_id];
         i < session.group_offsets[mod_id + 1]; ++i) {
        const std::uint32_t group = session.group_ids[i];
        if (group < session.group_masks.size()) {
            const auto& mask = session.group_masks[group];
            if (!mask.empty()) {
                pc_bitset_or(
                    block_mask.data(), block_mask.data(), mask.data(),
                    session.words);
            }
        }
    }
}

void build_refill_group_block_mask(
    const SessionImpl& session,
    const pc_item_state* item,
    std::vector<std::uint64_t>& block_mask) {
    block_mask.assign(session.words, 0);
    auto add_slot = [&](const pc_mod_slot& slot) {
        if (slot.mod_id == PC_MOD_NONE) return;
        if (slot.mod_id < session.mod_count) {
            or_mod_group_masks(session, slot.mod_id, block_mask);
        } else if (slot.group_id < session.group_masks.size()) {
            const auto& mask = session.group_masks[slot.group_id];
            if (!mask.empty()) {
                pc_bitset_or(
                    block_mask.data(), block_mask.data(), mask.data(),
                    session.words);
            }
        }
    };
    for (std::uint8_t i = 0; i < item->prefix_count; ++i) {
        add_slot(item->prefixes[i]);
    }
    for (std::uint8_t i = 0; i < item->suffix_count; ++i) {
        add_slot(item->suffixes[i]);
    }
}

void build_refill_metamod_hints(
    const SessionImpl& session,
    const pc_item_state* item,
    PoolBuildHints& hints) {
    const int attack_code = session.data->metamod_no_attack_code;
    const int caster_code = session.data->metamod_no_caster_code;
    auto scan = [&](const pc_mod_slot* slots, std::uint8_t count) {
        for (std::uint8_t i = 0; i < count; ++i) {
            const std::uint32_t id = slots[i].mod_id;
            if (id >= session.metamod_type.size()) continue;
            hints.block_attack |=
                session.metamod_type[id] == attack_code;
            hints.block_caster |=
                session.metamod_type[id] == caster_code;
        }
    };
    scan(item->prefixes, item->prefix_count);
    scan(item->suffixes, item->suffix_count);
}

// Draw one mod from the open affix sides and append it. Prefix/suffix candidates
// share one prefix-sum table, so the side is always selected by total weight.
bool add_random_mod(
    ActionContextImpl& context,
    const PoolBuildRequest& base_request,
    pc_item_state* item,
    PoolBuildHints* refill_hints = nullptr,
    std::uint32_t* out_mod_id = nullptr) {
    const SessionImpl& session = *context.session;
    const int side_filter =
        open_side_filter(session, item, base_request);
    if (side_filter == -2) return false;

    PoolBuildRequest request = base_request;
    request.side_filter = side_filter;
    bool cache_hit = false;
    const WeightedPool& pool =
        get_weighted_pool(
            context, item, request, &cache_hit, refill_hints);
    if (pool.total_weight == 0) {
        return false;
    }

    std::chrono::steady_clock::time_point sampling_started;
    if (context.perf_timing_enabled) {
        sampling_started = std::chrono::steady_clock::now();
    }
    const std::uint64_t roll = context.rng.next_below(pool.total_weight);
    const auto it =
        std::lower_bound(pool.prefix_sums.begin(), pool.prefix_sums.end(),
                         roll + 1);
    if (context.perf_timing_enabled) {
        ++context.sampling_calls;
        context.sampling_ns +=
            static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() - sampling_started)
                    .count());
    }
    const PoolEntry& chosen =
        pool.entries[static_cast<std::size_t>(it - pool.prefix_sums.begin())];
    if (out_mod_id != nullptr) *out_mod_id = chosen.session_mod_id;
    const bool added =
        pc_item_add_mod(item,
                        chosen.gen_type == 0 ? PC_SIDE_PREFIX : PC_SIDE_SUFFIX,
                        chosen.session_mod_id,
                        static_cast<std::uint16_t>(chosen.primary_group), 0,
                        nullptr) == PC_RESULT_OK;
    if (added) {
        record_weighted_pick(
            context, item, request, side_filter, cache_hit,
            pool.prefix_total_weight, pool.suffix_total_weight,
            pool.total_weight, roll, chosen);
        if (refill_hints != nullptr) {
            or_mod_group_masks(
                session, chosen.session_mod_id,
                *refill_hints->group_block_mask);
            if (chosen.session_mod_id < session.metamod_type.size()) {
                refill_hints->block_attack |=
                    session.metamod_type[chosen.session_mod_id] ==
                    session.data->metamod_no_attack_code;
                refill_hints->block_caster |=
                    session.metamod_type[chosen.session_mod_id] ==
                    session.data->metamod_no_caster_code;
            }
        }
    }
    return added;
}

bool add_random_mod_from_superset(
    ActionContextImpl& context,
    const WeightedPool& superset,
    const PoolBuildRequest& base_request,
    pc_item_state* item,
    PoolBuildHints& refill_hints,
    std::uint32_t max_rejections) {
    const SessionImpl& session = *context.session;
    auto tag_mask = [&](bool blocked, const char* name)
        -> const std::vector<std::uint64_t>* {
        if (!blocked) return nullptr;
        const auto tag = session.data->tag_id_by_name.find(name);
        if (tag == session.data->tag_id_by_name.end() ||
            tag->second >= session.implicit_tag_masks.size()) {
            return nullptr;
        }
        const auto& mask = session.implicit_tag_masks[tag->second];
        return mask.empty() ? nullptr : &mask;
    };
    const auto* attack_mask =
        tag_mask(refill_hints.block_attack, "attack");
    const auto* caster_mask =
        tag_mask(refill_hints.block_caster, "caster");
    for (std::uint32_t attempt = 0; attempt <= max_rejections; ++attempt) {
        if (superset.total_weight == 0) return false;
        const std::uint64_t roll =
            context.rng.next_below(superset.total_weight);
        const auto it = std::lower_bound(
            superset.prefix_sums.begin(), superset.prefix_sums.end(),
            roll + 1);
        const PoolEntry& chosen =
            superset.entries[
                static_cast<std::size_t>(
                    it - superset.prefix_sums.begin())];
        const int side_filter =
            open_side_filter(session, item, base_request);
        if (side_filter == -2) return false;
        const bool side_allowed =
            side_filter < 0 || chosen.gen_type == side_filter;
        const bool group_allowed =
            !pc_bitset_test(
                refill_hints.group_block_mask->data(),
                chosen.session_mod_id);
        const bool metamod_allowed =
            (attack_mask == nullptr ||
             !pc_bitset_test(
                 attack_mask->data(), chosen.session_mod_id)) &&
            (caster_mask == nullptr ||
             !pc_bitset_test(
                 caster_mask->data(), chosen.session_mod_id));
        if (!side_allowed || !group_allowed || !metamod_allowed) continue;

        const bool added =
            pc_item_add_mod(
                item,
                chosen.gen_type == 0 ? PC_SIDE_PREFIX : PC_SIDE_SUFFIX,
                chosen.session_mod_id,
                static_cast<std::uint16_t>(chosen.primary_group), 0,
                nullptr) == PC_RESULT_OK;
        if (!added) return false;
        or_mod_group_masks(
            session, chosen.session_mod_id,
            *refill_hints.group_block_mask);
        if (chosen.session_mod_id < session.metamod_type.size()) {
            refill_hints.block_attack |=
                session.metamod_type[chosen.session_mod_id] ==
                session.data->metamod_no_attack_code;
            refill_hints.block_caster |=
                session.metamod_type[chosen.session_mod_id] ==
                session.data->metamod_no_caster_code;
        }
        return true;
    }
    return add_random_mod(
        context, base_request, item, &refill_hints);
}

void fill_random_mods(
    ActionContextImpl& context,
    const PoolBuildRequest& base_request,
    pc_item_state* item,
    int target_total) {
    int total = item->prefix_count + item->suffix_count;
    const SessionImpl& session = *context.session;
    PoolBuildHints hints;
    PoolBuildHints* refill_hints = nullptr;
    if (context.incremental_refill_enabled && target_total - total > 1) {
        build_refill_group_block_mask(
            session, item, context.block_mask_scratch);
        hints.group_block_mask = &context.block_mask_scratch;
        build_refill_metamod_hints(session, item, hints);
        refill_hints = &hints;
    }
    const WeightedPool* rejection_superset = nullptr;
    if (refill_hints != nullptr && !context.capture_action_trace) {
        if (context.empty_group_mask.size() != session.words) {
            context.empty_group_mask.assign(session.words, 0);
        }
        PoolBuildHints superset_hints = hints;
        superset_hints.group_block_mask = &context.empty_group_mask;
        PoolBuildRequest superset_request = base_request;
        superset_request.side_filter =
            open_side_filter(session, item, base_request);
        rejection_superset = &get_weighted_pool(
            context, item, superset_request, nullptr, &superset_hints);
    }
    while (total < target_total) {
        const bool added =
            rejection_superset != nullptr
                ? add_random_mod_from_superset(
                      context, *rejection_superset, base_request, item,
                      *refill_hints, 4)
                : add_random_mod(
                      context, base_request, item, refill_hints);
        if (!added) {
            break;
        }
        ++total;
    }
}

bool add_direct_mod(
    const SessionImpl& session,
    pc_item_state* item,
    std::uint32_t mod_id,
    std::uint8_t flags = 0) {
    if (mod_id >= session.mod_count) return false;
    const int side = session.gen_type[mod_id];
    if (side != 0 && side != 1) return false;
    const std::uint8_t cap = max_affix(session, item);
    if ((side == 0 && item->prefix_count >= cap) ||
        (side == 1 && item->suffix_count >= cap)) {
        return false;
    }
    if (groups_conflict(session, item, mod_id)) return false;
    return pc_item_add_mod(
               item, side == 0 ? PC_SIDE_PREFIX : PC_SIDE_SUFFIX, mod_id,
               static_cast<std::uint16_t>(session.primary_group[mod_id]), flags,
               nullptr) == PC_RESULT_OK;
}

bool add_implicit(
    const SessionImpl& session,
    pc_item_state* item,
    std::uint32_t mod_id,
    std::uint8_t flags = 0) {
    if (mod_id >= session.mod_count ||
        item->implicit_count >= PC_MAX_IMPLICITS) {
        return false;
    }
    pc_mod_slot& slot = item->implicits[item->implicit_count++];
    slot = {};
    slot.mod_id = mod_id;
    slot.group_id =
        static_cast<std::uint16_t>(session.primary_group[mod_id]);
    slot.flags = flags;
    slot.veiled_chosen_mod_id = PC_MOD_NONE;
    for (std::uint32_t i = 0; i < PC_MAX_VEILED_OPTIONS; ++i)
        slot.veiled_option_mod_ids[i] = PC_MOD_NONE;
    return true;
}

void remove_implicit_at(pc_item_state* item, std::uint32_t index) {
    if (index >= item->implicit_count) return;
    const std::uint8_t last = static_cast<std::uint8_t>(
        item->implicit_count - 1);
    if (index != last) item->implicits[index] = item->implicits[last];
    item->implicits[last] = {};
    item->implicits[last].mod_id = PC_MOD_NONE;
    item->implicit_count = last;
}

struct KeptSlot {
    int side;
    std::uint32_t mod_id;
    std::uint16_t group_id;
    std::uint8_t flags;
};

// Collect the slots a reforge must keep: fractured slots, and every slot on a
// locked side.
std::vector<KeptSlot> collect_preserved(
    const SessionImpl& session,
    const pc_item_state* item) {
    std::vector<KeptSlot> kept;
    auto scan = [&](int side, const pc_mod_slot* slots, std::uint8_t count) {
        const bool locked = side_locked(session, item, side);
        for (std::uint8_t i = 0; i < count; ++i) {
            const bool fractured = slots[i].flags & PC_MOD_SLOT_FRACTURED;
            if (fractured || locked) {
                kept.push_back({side, slots[i].mod_id, slots[i].group_id,
                                slots[i].flags});
            }
        }
    };
    scan(PC_SIDE_PREFIX, item->prefixes, item->prefix_count);
    scan(PC_SIDE_SUFFIX, item->suffixes, item->suffix_count);
    return kept;
}

void restore_slots(pc_item_state* item, const std::vector<KeptSlot>& kept) {
    pc_item_clear_side(item, PC_SIDE_PREFIX);
    pc_item_clear_side(item, PC_SIDE_SUFFIX);
    for (const KeptSlot& k : kept) {
        pc_mod_slot* slot = nullptr;
        if (pc_item_add_mod(item, k.side, k.mod_id, k.group_id, k.flags,
                            &slot) == PC_RESULT_OK &&
            slot != nullptr) {
            // flags already carried (fractured etc.)
        }
    }
}

// Reforge: preserve fractured/locked slots, drop the rest, then refill to a
// target total. Removed groups no longer block because the group set is rebuilt
// from the preserved slots only.
ActionOutcome reforge(
    ActionContextImpl& context,
    pc_item_state* item,
    std::uint8_t new_rarity,
    int target_total,
    const PoolBuildRequest& pool_request,
    const std::vector<std::uint32_t>& direct_mods = {}) {
    const SessionImpl& session = *context.session;
    const int before = item->prefix_count + item->suffix_count;
    const std::vector<KeptSlot> kept = collect_preserved(session, item);
    restore_slots(item, kept);
    item->rarity = new_rarity;

    int total = item->prefix_count + item->suffix_count;
    const int preserved = total;
    for (std::uint32_t direct : direct_mods) {
        if (!add_direct_mod(session, item, direct)) {
            context.last_action_trace.clear();
            return {};
        }
        if (context.capture_action_trace) {
            ActionTraceStage stage;
            stage.stage_index =
                static_cast<std::uint32_t>(context.last_action_trace.size());
            stage.direct = true;
            stage.tag_signature_id = intern_item_tag_signature(context, item);
            stage.weight_kind = pool_request.weight_kind;
            stage.chosen_mod_id = direct;
            stage.chosen_side = session.gen_type[direct];
            context.last_action_trace.push_back(stage);
        }
        ++total;
    }
    fill_random_mods(context, pool_request, item, target_total);
    total = item->prefix_count + item->suffix_count;
    ActionOutcome out;
    out.applied = true;
    out.added = total - preserved;
    out.removed = before - preserved;
    return out;
}

int magic_count(ActionContextImpl& context) {
    return 1 + static_cast<int>(context.rng.next_below(2)); // 1 or 2
}

int rare_count(ActionContextImpl& context) {
    return 4 + static_cast<int>(context.rng.next_below(3)); // 4, 5, or 6
}

ActionOutcome do_add_one(ActionContextImpl& context, pc_item_state* item) {
    ActionOutcome out;
    if (add_random_mod(context, PoolBuildRequest{}, item)) {
        out.applied = true;
        out.added = 1;
    }
    return out;
}

ActionOutcome do_annul(
    const SessionImpl& session,
    Rng& rng,
    pc_item_state* item) {
    ActionOutcome out;
    const bool prefix_locked = side_locked(session, item, PC_SIDE_PREFIX);
    const bool suffix_locked = side_locked(session, item, PC_SIDE_SUFFIX);
    if (prefix_locked && suffix_locked) {
        return out;
    }
    struct Ref {
        int side;
        std::uint32_t index;
    };
    std::vector<Ref> removable;
    if (!prefix_locked) {
        for (std::uint8_t i = 0; i < item->prefix_count; ++i) {
            if (!(item->prefixes[i].flags & PC_MOD_SLOT_FRACTURED)) {
                removable.push_back({PC_SIDE_PREFIX, i});
            }
        }
    }
    if (!suffix_locked) {
        for (std::uint8_t i = 0; i < item->suffix_count; ++i) {
            if (!(item->suffixes[i].flags & PC_MOD_SLOT_FRACTURED)) {
                removable.push_back({PC_SIDE_SUFFIX, i});
            }
        }
    }
    if (removable.empty()) {
        return out;
    }
    const Ref& pick = removable[rng.next_below(removable.size())];
    if (pc_item_remove_at(item, pick.side, pick.index) == PC_RESULT_OK) {
        out.applied = true;
        out.removed = 1;
    }
    return out;
}

ActionOutcome do_scour(
    const SessionImpl& session,
    pc_item_state* item) {
    ActionOutcome out;
    if (item->rarity == PC_RARITY_NORMAL) {
        return out; // nothing to scour
    }
    const int before = item->prefix_count + item->suffix_count;
    const bool prefix_locked = side_locked(session, item, PC_SIDE_PREFIX);
    const bool suffix_locked = side_locked(session, item, PC_SIDE_SUFFIX);

    std::vector<KeptSlot> kept;
    if (prefix_locked && !suffix_locked) {
        // keep prefixes, drop suffixes
        for (std::uint8_t i = 0; i < item->prefix_count; ++i) {
            kept.push_back({PC_SIDE_PREFIX, item->prefixes[i].mod_id,
                            item->prefixes[i].group_id, item->prefixes[i].flags});
        }
    } else if (suffix_locked && !prefix_locked) {
        for (std::uint8_t i = 0; i < item->suffix_count; ++i) {
            kept.push_back({PC_SIDE_SUFFIX, item->suffixes[i].mod_id,
                            item->suffixes[i].group_id, item->suffixes[i].flags});
        }
    } else {
        // keep only fractured slots
        auto keep_fractured = [&](int side, const pc_mod_slot* slots,
                                  std::uint8_t count) {
            for (std::uint8_t i = 0; i < count; ++i) {
                if (slots[i].flags & PC_MOD_SLOT_FRACTURED) {
                    kept.push_back({side, slots[i].mod_id, slots[i].group_id,
                                    slots[i].flags});
                }
            }
        };
        keep_fractured(PC_SIDE_PREFIX, item->prefixes, item->prefix_count);
        keep_fractured(PC_SIDE_SUFFIX, item->suffixes, item->suffix_count);
    }

    restore_slots(item, kept);
    const int remaining = item->prefix_count + item->suffix_count;
    if (prefix_locked || suffix_locked) {
        item->rarity = remaining > 0 ? PC_RARITY_RARE : PC_RARITY_NORMAL;
    } else {
        item->rarity = remaining > 0 ? PC_RARITY_MAGIC : PC_RARITY_NORMAL;
    }
    out.applied = before != remaining || item->rarity == PC_RARITY_NORMAL;
    out.removed = before - remaining;
    return out;
}

ActionOutcome do_remove_crafted_modifiers(pc_item_state* item) {
    ActionOutcome out;
    if (item->rarity != PC_RARITY_MAGIC && item->rarity != PC_RARITY_RARE) {
        return out;
    }
    const auto remove_crafted = [&](int side) {
        pc_mod_slot* slots = side == PC_SIDE_PREFIX ? item->prefixes
                                                    : item->suffixes;
        std::uint8_t& count = side == PC_SIDE_PREFIX ? item->prefix_count
                                                     : item->suffix_count;
        for (std::uint8_t i = count; i > 0; --i) {
            if ((slots[i - 1].flags & PC_MOD_SLOT_CRAFTED) != 0 &&
                (slots[i - 1].flags & PC_MOD_SLOT_FRACTURED) == 0 &&
                pc_item_remove_at(item, side, i - 1) == PC_RESULT_OK) {
                ++out.removed;
            }
        }
    };
    remove_crafted(PC_SIDE_PREFIX);
    remove_crafted(PC_SIDE_SUFFIX);
    out.applied = out.removed > 0;
    return out;
}

void record_direct(
    ActionContextImpl& context,
    const pc_item_state* item,
    std::uint32_t mod_id,
    int side) {
    if (!context.capture_action_trace) return;
    ActionTraceStage stage;
    stage.stage_index =
        static_cast<std::uint32_t>(context.last_action_trace.size());
    stage.direct = true;
    stage.tag_signature_id = intern_item_tag_signature(context, item);
    stage.chosen_mod_id = mod_id;
    stage.chosen_side = static_cast<std::int8_t>(side);
    context.last_action_trace.push_back(stage);
}

ActionOutcome do_fracture(
    ActionContextImpl& context,
    pc_item_state* item) {
    if (item->rarity != PC_RARITY_RARE ||
        item->generic_influence_bits != 0 ||
        (item->item_flags & PC_ITEM_SYNTHESISED) != 0 ||
        pc_item_find_fractured(item, nullptr, nullptr) == PC_RESULT_OK) {
        return {};
    }
    const std::uint32_t total =
        static_cast<std::uint32_t>(item->prefix_count + item->suffix_count);
    if (total < 4) return {};

    const std::uint32_t pick =
        static_cast<std::uint32_t>(context.rng.next_below(total));
    const int side =
        pick < item->prefix_count ? PC_SIDE_PREFIX : PC_SIDE_SUFFIX;
    const std::uint32_t index =
        side == PC_SIDE_PREFIX ? pick : pick - item->prefix_count;
    pc_mod_slot& slot =
        side == PC_SIDE_PREFIX ? item->prefixes[index]
                               : item->suffixes[index];
    slot.flags |= PC_MOD_SLOT_FRACTURED;
    record_direct(context, item, slot.mod_id, side);
    return {true, 0, 0};
}

std::vector<std::uint32_t> ids_from_mask(
    const SessionImpl& session,
    const std::vector<std::uint64_t>& mask) {
    std::vector<std::uint32_t> ids;
    for (std::uint32_t id = 0; id < session.mod_count; ++id) {
        if (pc_bitset_test(mask.data(), id)) ids.push_back(id);
    }
    return ids;
}

bool generate_unveil_options(
    ActionContextImpl& context,
    pc_item_state* item,
    int side,
    std::uint32_t index) {
    const SessionImpl& session = *context.session;
    pc_mod_slot& slot =
        side == PC_SIDE_PREFIX ? item->prefixes[index] : item->suffixes[index];
    std::vector<std::uint32_t> available =
        ids_from_mask(session, session.unveiled_generic_mask);
    slot.veiled_option_count = 0;
    for (std::uint32_t option = 0; option < PC_MAX_VEILED_OPTIONS; ++option) {
        const std::uint32_t chosen = pick_weighted_id(
            context, item, available, side, kNoTag, side, index);
        if (chosen == kNoMod) break;
        slot.veiled_option_mod_ids[slot.veiled_option_count++] = chosen;
        available.erase(
            std::remove(available.begin(), available.end(), chosen),
            available.end());
    }
    return slot.veiled_option_count > 0;
}

bool add_veiled_mod(
    ActionContextImpl& context,
    pc_item_state* item) {
    const SessionImpl& session = *context.session;
    const std::uint8_t cap = max_affix(session, item);
    const bool prefix_open = item->prefix_count < cap;
    const bool suffix_open = item->suffix_count < cap;
    if (!prefix_open && !suffix_open) return false;
    int side = prefix_open && suffix_open
                   ? static_cast<int>(context.rng.next_below(2))
                   : prefix_open ? PC_SIDE_PREFIX : PC_SIDE_SUFFIX;
    const std::uint32_t mod_id =
        side == PC_SIDE_PREFIX ? session.veiled_prefix_mod_id
                               : session.veiled_suffix_mod_id;
    if (mod_id == kNoMod) return false;
    pc_mod_slot* slot = nullptr;
    if (pc_item_add_mod(
            item, side, mod_id,
            static_cast<std::uint16_t>(session.primary_group[mod_id]),
            PC_MOD_SLOT_VEILED, &slot) != PC_RESULT_OK) {
        return false;
    }
    const std::uint32_t index =
        side == PC_SIDE_PREFIX ? item->prefix_count - 1
                               : item->suffix_count - 1;
    if (!generate_unveil_options(context, item, side, index)) {
        pc_item_remove_at(item, side, index);
        return false;
    }
    record_direct(context, item, mod_id, side);
    return true;
}

ActionOutcome do_bench(
    ActionContextImpl& context,
    pc_item_state* item,
    std::uint32_t mod_id) {
    const SessionImpl& session = *context.session;
    if ((item->rarity != PC_RARITY_MAGIC &&
         item->rarity != PC_RARITY_RARE) ||
        mod_id >= session.mod_count ||
        !(session.flags[mod_id] & (1 << 1)) ||
        std::find(session.bench_mod_ids.begin(), session.bench_mod_ids.end(),
                  mod_id) == session.bench_mod_ids.end()) {
        return {};
    }
    int crafted_count = 0;
    auto count_crafted = [&](const pc_mod_slot* slots, std::uint8_t count) {
        for (std::uint8_t i = 0; i < count; ++i)
            if (slots[i].flags & PC_MOD_SLOT_CRAFTED) ++crafted_count;
    };
    count_crafted(item->prefixes, item->prefix_count);
    count_crafted(item->suffixes, item->suffix_count);
    const bool has_multimod = item_has_metamod(
        session, item, session.data->metamod_multimod_code);
    const bool target_multimod =
        session.metamod_type[mod_id] == session.data->metamod_multimod_code;
    if ((!target_multimod && crafted_count > 0 && !has_multimod) ||
        crafted_count >= (has_multimod || target_multimod ? 3 : 1)) {
        return {};
    }
    if (!add_direct_mod(session, item, mod_id, PC_MOD_SLOT_CRAFTED)) return {};
    record_direct(context, item, mod_id, session.gen_type[mod_id]);
    return {true, 1, 0};
}

ActionOutcome do_unveil(
    ActionContextImpl& context,
    pc_item_state* item,
    std::uint32_t chosen) {
    const SessionImpl& session = *context.session;
    int side = -1;
    std::uint32_t index = 0;
    if (pc_item_find_veiled(item, &side, &index) != PC_RESULT_OK ||
        chosen >= session.mod_count) {
        return {};
    }
    pc_mod_slot& slot =
        side == PC_SIDE_PREFIX ? item->prefixes[index] : item->suffixes[index];
    bool offered = false;
    for (std::uint8_t i = 0; i < slot.veiled_option_count; ++i)
        offered |= slot.veiled_option_mod_ids[i] == chosen;
    if (!offered || session.gen_type[chosen] != side ||
        groups_conflict(session, item, chosen, side, index)) {
        return {};
    }
    slot.mod_id = chosen;
    slot.group_id =
        static_cast<std::uint16_t>(session.primary_group[chosen]);
    slot.flags &= static_cast<std::uint8_t>(~PC_MOD_SLOT_VEILED);
    slot.veiled_chosen_mod_id = chosen;
    record_direct(context, item, chosen, side);
    return {true, 1, 1};
}

ActionOutcome do_harvest_reforge(
    ActionContextImpl& context,
    pc_item_state* item,
    std::uint32_t tag_id) {
    const SessionImpl& session = *context.session;
    const int before = item->prefix_count + item->suffix_count;
    const std::vector<KeptSlot> kept = collect_preserved(session, item);
    restore_slots(item, kept);
    item->rarity = PC_RARITY_RARE;
    PoolBuildRequest guaranteed;
    guaranteed.weight_kind = PoolWeightKind::HarvestSpawnOnly;
    guaranteed.target_tag_id = tag_id;
    if (!add_random_mod(context, guaranteed, item)) return {};
    int target = std::min<int>(rare_count(context), session.rare_affix_cap * 2);
    fill_random_mods(context, PoolBuildRequest{}, item, target);
    const int after = item->prefix_count + item->suffix_count;
    return {true, after - static_cast<int>(kept.size()),
            before - static_cast<int>(kept.size())};
}

ActionOutcome do_harvest_augment(
    ActionContextImpl& context,
    pc_item_state* item,
    std::uint32_t tag_id) {
    if (item->rarity != PC_RARITY_MAGIC && item->rarity != PC_RARITY_RARE)
        return {};
    if (item->generic_influence_bits || item->searing_exarch_tier ||
        item->eater_of_worlds_tier) {
        return {};
    }
    PoolBuildRequest request;
    request.weight_kind = PoolWeightKind::HarvestSpawnOnly;
    request.target_tag_id = tag_id;
    std::uint32_t added_id = kNoMod;
    if (!add_random_mod(
            context, request, item, nullptr, &added_id)) {
        return {};
    }
    struct Ref { int side; std::uint32_t index; };
    std::vector<Ref> removable;
    auto collect = [&](int side, const pc_mod_slot* slots, std::uint8_t count) {
        if (side_locked(*context.session, item, side)) return;
        for (std::uint8_t i = 0; i < count; ++i) {
            if (!(slots[i].flags & PC_MOD_SLOT_FRACTURED) &&
                slots[i].mod_id != added_id) {
                removable.push_back({side, i});
            }
        }
    };
    collect(PC_SIDE_PREFIX, item->prefixes, item->prefix_count);
    collect(PC_SIDE_SUFFIX, item->suffixes, item->suffix_count);
    if (removable.empty()) return {true, 1, 0};
    const Ref pick = removable[context.rng.next_below(removable.size())];
    pc_item_remove_at(item, pick.side, pick.index);
    return {true, 1, 1};
}

ActionOutcome do_harvest_resist(
    ActionContextImpl& context,
    pc_item_state* item,
    std::uint32_t source_tag,
    std::uint32_t target_tag) {
    const SessionImpl& session = *context.session;
    if (item->rarity != PC_RARITY_MAGIC &&
        item->rarity != PC_RARITY_RARE) {
        return {};
    }
    const auto resistance = session.data->tag_id_by_name.find("resistance");
    if (resistance == session.data->tag_id_by_name.end()) return {};
    struct Ref { int side; std::uint32_t index; };
    std::unordered_set<std::uint32_t> attempted;
    while (true) {
        std::vector<Ref> sources;
        auto collect = [&](int side, const pc_mod_slot* slots,
                           std::uint8_t count) {
            if (side_locked(session, item, side)) return;
            for (std::uint8_t i = 0; i < count; ++i) {
                const std::uint32_t id = slots[i].mod_id;
                if (!(slots[i].flags & PC_MOD_SLOT_FRACTURED) &&
                    !attempted.count(id) &&
                    has_class_tag(session, id, resistance->second) &&
                    has_class_tag(session, id, source_tag)) {
                    sources.push_back({side, i});
                }
            }
        };
        collect(PC_SIDE_PREFIX, item->prefixes, item->prefix_count);
        collect(PC_SIDE_SUFFIX, item->suffixes, item->suffix_count);
        if (sources.empty()) return {};
        const std::size_t pick_index =
            static_cast<std::size_t>(context.rng.next_below(sources.size()));
        const Ref ref = sources[pick_index];
        pc_mod_slot original =
            ref.side == PC_SIDE_PREFIX ? item->prefixes[ref.index]
                                       : item->suffixes[ref.index];
        pc_item_remove_at(item, ref.side, ref.index);
        PoolBuildRequest request;
        request.weight_kind = PoolWeightKind::HarvestSpawnOnly;
        request.target_tag_id = target_tag;
        request.side_filter = ref.side;
        const WeightedPool& pool = get_weighted_pool(context, item, request);
        std::vector<std::pair<std::uint32_t, std::uint64_t>> candidates;
        std::uint64_t total = 0;
        for (const PoolEntry& entry : pool.entries) {
            if (entry.required_level != session.required_level[original.mod_id] ||
                !has_class_tag(session, entry.session_mod_id,
                               resistance->second) ||
                has_class_tag(session, entry.session_mod_id, source_tag)) {
                continue;
            }
            total += entry.final_weight;
            candidates.push_back({entry.session_mod_id, total});
        }
        if (total > 0) {
            const std::uint64_t roll = context.rng.next_below(total);
            const auto chosen = std::lower_bound(
                candidates.begin(), candidates.end(), roll + 1,
                [](const auto& entry, std::uint64_t value) {
                    return entry.second < value;
                });
            if (chosen != candidates.end() &&
                add_direct_mod(session, item, chosen->first)) {
                record_direct(context, item, chosen->first, ref.side);
                return {true, 1, 1};
            }
        }
        pc_mod_slot* restored = nullptr;
        pc_item_add_mod(item, ref.side, original.mod_id, original.group_id,
                        original.flags, &restored);
        if (restored) *restored = original;
        attempted.insert(original.mod_id);
    }
}

bool add_eldritch_implicit(
    ActionContextImpl& context,
    pc_item_state* item,
    bool searing,
    std::uint32_t tier) {
    const SessionImpl& session = *context.session;
    if (!session.eldritch_eligible || tier < 1 || tier > 4 ||
        item->generic_influence_bits != 0) {
        return false;
    }
    const auto& by_tier = searing ? session.eldritch_searing_tier_mod_ids
                                  : session.eldritch_eater_tier_mod_ids;
    if (tier >= by_tier.size()) return false;
    const auto tag = session.data->tag_id_by_name.find(
        "no_tier_" + std::to_string(tier) + "_eldritch_implicit");
    const std::uint32_t chosen = pick_weighted_id(
        context, nullptr, by_tier[tier], -1,
        tag == session.data->tag_id_by_name.end() ? kNoTag : tag->second);
    if (chosen == kNoMod) return false;
    const int expected_gen = searing ? session.data->gen_searing_implicit_code
                                     : session.data->gen_eater_implicit_code;
    for (std::uint32_t i = 0; i < item->implicit_count;) {
        const std::uint32_t id = item->implicits[i].mod_id;
        if ((item->implicits[i].flags & PC_MOD_SLOT_ELDRITCH) &&
            id < session.mod_count &&
            session.data->mod_gen_type_code[session.global_index[id]] ==
                expected_gen) {
            remove_implicit_at(item, i);
        } else {
            ++i;
        }
    }
    if (!add_implicit(session, item, chosen, PC_MOD_SLOT_ELDRITCH))
        return false;
    if (searing) item->searing_exarch_tier = static_cast<std::uint8_t>(tier);
    else item->eater_of_worlds_tier = static_cast<std::uint8_t>(tier);
    record_direct(context, item, chosen, -1);
    return true;
}

int dominant_eldritch(const pc_item_state* item) {
    if (item->searing_exarch_tier > item->eater_of_worlds_tier) return 0;
    if (item->eater_of_worlds_tier > item->searing_exarch_tier) return 1;
    return -1;
}

ActionOutcome do_eldritch_chaos(
    ActionContextImpl& context,
    pc_item_state* item) {
    const int side = dominant_eldritch(item);
    if (side < 0)
        return reforge(context, item, PC_RARITY_RARE, rare_count(context),
                       PoolBuildRequest{});
    pc_mod_slot fractured{};
    bool has_fractured = false;
    pc_mod_slot* slots =
        side == PC_SIDE_PREFIX ? item->prefixes : item->suffixes;
    const std::uint8_t count =
        side == PC_SIDE_PREFIX ? item->prefix_count : item->suffix_count;
    for (std::uint8_t i = 0; i < count; ++i) {
        if (slots[i].flags & PC_MOD_SLOT_FRACTURED) {
            fractured = slots[i];
            has_fractured = true;
            break;
        }
    }
    pc_item_clear_side(item, side);
    if (has_fractured) {
        pc_mod_slot* restored = nullptr;
        pc_item_add_mod(item, side, fractured.mod_id, fractured.group_id,
                        fractured.flags, &restored);
        if (restored) *restored = fractured;
    }
    const int target = 2 + static_cast<int>(context.rng.next_below(2));
    PoolBuildRequest request;
    request.side_filter = side;
    fill_random_mods(
        context, request, item,
        item->prefix_count + item->suffix_count +
            target -
            (side == PC_SIDE_PREFIX ? item->prefix_count
                                    : item->suffix_count));
    return {true, target - (has_fractured ? 1 : 0),
            static_cast<int>(count) - (has_fractured ? 1 : 0)};
}

ActionOutcome do_eldritch_annul(
    ActionContextImpl& context,
    pc_item_state* item) {
    const int side = dominant_eldritch(item);
    if (side < 0) return do_annul(*context.session, context.rng, item);
    const pc_mod_slot* slots =
        side == PC_SIDE_PREFIX ? item->prefixes : item->suffixes;
    const std::uint8_t count =
        side == PC_SIDE_PREFIX ? item->prefix_count : item->suffix_count;
    std::vector<std::uint32_t> removable;
    for (std::uint8_t i = 0; i < count; ++i)
        if (!(slots[i].flags & PC_MOD_SLOT_FRACTURED))
            removable.push_back(i);
    if (removable.empty()) return {};
    pc_item_remove_at(
        item, side, removable[context.rng.next_below(removable.size())]);
    return {true, 0, 1};
}

void apply_fossil_specials(
    ActionContextImpl& context,
    pc_item_state* item,
    const std::vector<std::uint32_t>& fossils) {
    const SessionImpl& session = *context.session;
    for (std::uint32_t fossil : fossils) {
        const std::string& name =
            session.data->string_at(session.data->fossil_name_sids[fossil]);
        if (name == "Bloodstained Fossil") {
            const std::vector<std::uint32_t> ids =
                ids_from_mask(session, session.corrupted_implicit_mask);
            const std::uint32_t chosen =
                pick_weighted_id(context, nullptr, ids);
            if (chosen != kNoMod &&
                add_implicit(session, item, chosen)) {
                item->item_flags |= PC_ITEM_CORRUPTED;
                record_direct(context, item, chosen, -1);
            }
        }
        if (fossil < session.fossil_sell_price_mod_ids.size() &&
            !session.fossil_sell_price_mod_ids[fossil].empty()) {
            const auto& ids = session.fossil_sell_price_mod_ids[fossil];
            const std::uint32_t chosen =
                ids[context.rng.next_below(ids.size())];
            if (add_implicit(session, item, chosen))
                record_direct(context, item, chosen, -1);
        }
        if (fossil < session.data->fossil_mirrors.size() &&
            session.data->fossil_mirrors[fossil]) {
            item->item_flags |= PC_ITEM_MIRRORED;
        }
    }
}

} // namespace

ActionOutcome apply_action(
    ActionContextImpl& context,
    pc_item_state* item,
    const ActionParameters& action) {
    const SessionImpl& session = *context.session;
    if (context.capture_action_trace) {
        context.last_action_trace.clear();
    }
    if (!item_craftable(item)) return {};
    switch (action.type) {
    case ActionType::Transmute:
        if (item->rarity != PC_RARITY_NORMAL) {
            return {};
        }
        return reforge(context, item, PC_RARITY_MAGIC, magic_count(context),
                       PoolBuildRequest{});
    case ActionType::Augment:
        if (item->rarity != PC_RARITY_MAGIC) {
            return {};
        }
        return do_add_one(context, item);
    case ActionType::Alteration:
        if (item->rarity != PC_RARITY_MAGIC) {
            return {};
        }
        return reforge(context, item, PC_RARITY_MAGIC, magic_count(context),
                       PoolBuildRequest{});
    case ActionType::Regal: {
        if (item->rarity != PC_RARITY_MAGIC) {
            return {};
        }
        item->rarity = PC_RARITY_RARE;
        ActionOutcome out = do_add_one(context, item);
        out.applied = true; // the magic -> rare upgrade always applies
        return out;
    }
    case ActionType::Alchemy:
        if (item->rarity != PC_RARITY_NORMAL) {
            return {};
        }
        return reforge(context, item, PC_RARITY_RARE, rare_count(context),
                       PoolBuildRequest{});
    case ActionType::Chaos:
        if (item->rarity != PC_RARITY_RARE) {
            return {};
        }
        return reforge(context, item, PC_RARITY_RARE, rare_count(context),
                       PoolBuildRequest{});
    case ActionType::Exalt:
        if (item->rarity != PC_RARITY_RARE) {
            return {};
        }
        return do_add_one(context, item);
    case ActionType::Annul:
        return do_annul(session, context.rng, item);
    case ActionType::Scour:
        return do_scour(session, item);
    case ActionType::RemoveCraftedModifiers:
        return do_remove_crafted_modifiers(item);
    case ActionType::Essence: {
        if (action.essence_index >=
            session.essence_guaranteed_mod_ids.size()) {
            return {};
        }
        const std::int32_t restriction =
            session.data->essence_item_level_restrictions[
                action.essence_index];
        if (restriction >= 0 &&
            session.item_level > static_cast<std::uint32_t>(restriction)) {
            return {};
        }
        const std::uint32_t guaranteed =
            session.essence_guaranteed_mod_ids[action.essence_index];
        if (guaranteed == std::numeric_limits<std::uint32_t>::max()) return {};
        const int target = rare_count(context);
        return reforge(context, item, PC_RARITY_RARE,
                       std::min<int>(target, session.rare_affix_cap * 2),
                       PoolBuildRequest{}, {guaranteed});
    }
    case ActionType::Fossil: {
        if (action.fossil_indices.empty()) return {};
        std::vector<std::uint32_t> forced;
        for (std::uint32_t fossil : action.fossil_indices) {
            if (fossil >= session.fossil_forced_mod_ids.size()) return {};
            forced.insert(forced.end(),
                          session.fossil_forced_mod_ids[fossil].begin(),
                          session.fossil_forced_mod_ids[fossil].end());
        }
        std::sort(forced.begin(), forced.end());
        forced.erase(std::unique(forced.begin(), forced.end()), forced.end());
        PoolBuildRequest pool_request;
        pool_request.weight_kind = PoolWeightKind::Fossil;
        pool_request.fossil_indices = action.fossil_indices;
        ActionOutcome out = reforge(
            context, item, PC_RARITY_RARE,
            std::min<int>(rare_count(context), session.rare_affix_cap * 2),
            pool_request, forced);
        if (out.applied)
            apply_fossil_specials(context, item, action.fossil_indices);
        return out;
    }
    case ActionType::Bench:
        return do_bench(context, item, action.mod_id);
    case ActionType::VeiledChaos: {
        if (item->rarity != PC_RARITY_RARE ||
            pc_item_find_veiled(item, nullptr, nullptr) == PC_RESULT_OK) {
            return {};
        }
        const int before = item->prefix_count + item->suffix_count;
        const std::vector<KeptSlot> kept = collect_preserved(session, item);
        restore_slots(item, kept);
        const int target = std::min<int>(
            rare_count(context), session.rare_affix_cap * 2);
        fill_random_mods(
            context, PoolBuildRequest{}, item, target - 1);
        if (!add_veiled_mod(context, item)) return {};
        const int after = item->prefix_count + item->suffix_count;
        return {true, after - static_cast<int>(kept.size()),
                before - static_cast<int>(kept.size())};
    }
    case ActionType::VeiledExalt:
        if (item->rarity != PC_RARITY_RARE ||
            pc_item_find_veiled(item, nullptr, nullptr) == PC_RESULT_OK ||
            !add_veiled_mod(context, item)) {
            return {};
        }
        return {true, 1, 0};
    case ActionType::Unveil:
        return do_unveil(context, item, action.mod_id);
    case ActionType::HarvestReforge:
        if (item->rarity != PC_RARITY_RARE ||
            action.target_tag_id == kNoTag) {
            return {};
        }
        return do_harvest_reforge(context, item, action.target_tag_id);
    case ActionType::HarvestAugment:
        if (action.target_tag_id == kNoTag) return {};
        return do_harvest_augment(context, item, action.target_tag_id);
    case ActionType::HarvestResist:
        if (action.source_tag_id == kNoTag ||
            action.target_tag_id == kNoTag ||
            action.source_tag_id == action.target_tag_id) {
            return {};
        }
        return do_harvest_resist(
            context, item, action.source_tag_id, action.target_tag_id);
    case ActionType::EldritchEmber:
        return add_eldritch_implicit(
                   context, item, true, action.tier)
                   ? ActionOutcome{true, 1, 0}
                   : ActionOutcome{};
    case ActionType::EldritchIchor:
        return add_eldritch_implicit(
                   context, item, false, action.tier)
                   ? ActionOutcome{true, 1, 0}
                   : ActionOutcome{};
    case ActionType::EldritchExalt: {
        if (!session.eldritch_eligible ||
            item->rarity != PC_RARITY_RARE) {
            return {};
        }
        PoolBuildRequest request;
        request.side_filter = dominant_eldritch(item);
        ActionOutcome out;
        if (add_random_mod(context, request, item)) {
            out = {true, 1, 0};
        }
        return out;
    }
    case ActionType::EldritchChaos:
        if (!session.eldritch_eligible ||
            item->rarity != PC_RARITY_RARE) {
            return {};
        }
        return do_eldritch_chaos(context, item);
    case ActionType::EldritchAnnul:
        if (!session.eldritch_eligible) return {};
        return do_eldritch_annul(context, item);
    case ActionType::InfluenceExalt: {
        if (item->rarity != PC_RARITY_RARE ||
            action.influence_code <= 0 || action.influence_code > 8 ||
            item->generic_influence_bits || item->searing_exarch_tier ||
            item->eater_of_worlds_tier) {
            return {};
        }
        const std::uint8_t bit =
            static_cast<std::uint8_t>(1u << (action.influence_code - 1));
        item->generic_influence_bits |= bit;
        PoolBuildRequest request;
        request.influence_only_code = action.influence_code;
        if (!add_random_mod(context, request, item)) {
            item->generic_influence_bits &= static_cast<std::uint8_t>(~bit);
            return {};
        }
        return {true, 1, 0};
    }
    case ActionType::Fracture:
        return do_fracture(context, item);
    }
    return {};
}

} // namespace poecraft
