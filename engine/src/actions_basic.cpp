#include "currency_outcomes.hpp"
#include "engine_internal.hpp"
#include "multi_item.hpp"

#include "poecraft/bitset.h"
#include "poecraft/item_state.h"
#include "poecraft/session.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <limits>
#include <map>
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
           item->lifecycle == PC_ITEM_LIVE &&
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
    const PoolBuildRequest& request,
    PoolBuildHints& hints) {
    if (!request.respects_metamod_pool_blocks) return;
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
        build_refill_metamod_hints(session, item, base_request, hints);
        refill_hints = &hints;
    }
    const WeightedPool* rejection_superset = nullptr;
    if (refill_hints != nullptr && !context.capture_action_trace &&
        !session.has_added_tags) {
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
    pc_mod_slot value;
};

// Collect the slots a reforge must keep: fractured slots, and every slot on a
// locked side.
std::vector<KeptSlot> collect_preserved(
    const SessionImpl& session,
    const pc_item_state* item,
    const bool respects_metamod_side_locks = true) {
    std::vector<KeptSlot> kept;
    auto scan = [&](int side, const pc_mod_slot* slots, std::uint8_t count) {
        const bool locked =
            respects_metamod_side_locks && side_locked(session, item, side);
        for (std::uint8_t i = 0; i < count; ++i) {
            const bool fractured = slots[i].flags & PC_MOD_SLOT_FRACTURED;
            if (fractured || locked) {
                kept.push_back({side, slots[i]});
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
        if (pc_item_add_mod(item, k.side, k.value.mod_id, k.value.group_id, k.value.flags,
                            &slot) == PC_RESULT_OK &&
            slot != nullptr) {
            *slot = k.value;
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
    const std::vector<std::uint32_t>& direct_mods = {},
    const bool respects_metamod_side_locks = true) {
    const SessionImpl& session = *context.session;
    const int before = item->prefix_count + item->suffix_count;
    const std::vector<KeptSlot> kept = collect_preserved(
        session, item, respects_metamod_side_locks);
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
    const auto law = rare_reforge_count_law(context.session->rare_reforge_count_kind);
    return law.select(context.rng.next_below(law.denominator));
}

// Ordinary rare_count is owned by the separate 8:3:1 correction. A configured
// cluster samples its approved total directly; clamping a six-mod law is wrong.
int configured_rare_count(ActionContextImpl& context) {
    if (context.session->is_cluster())
        return context.rng.next_below(100) < 65 ? 3 : 4;
    return rare_count(context);
}

ActionOutcome do_add_one(ActionContextImpl& context, pc_item_state* item,
                         bool foulborn = false) {
    ActionOutcome out;
    PoolBuildRequest request;
    if (foulborn) request.weight_kind = PoolWeightKind::Foulborn;
    if (add_random_mod(context, request, item)) {
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
    const std::uint8_t rarity_before = item->rarity;
    const int before = item->prefix_count + item->suffix_count;
    const bool prefix_locked = side_locked(session, item, PC_SIDE_PREFIX);
    const bool suffix_locked = side_locked(session, item, PC_SIDE_SUFFIX);

    std::vector<KeptSlot> kept;
    if (prefix_locked && !suffix_locked) {
        for (std::uint8_t i = 0; i < item->prefix_count; ++i) {
            kept.push_back({PC_SIDE_PREFIX, item->prefixes[i]});
        }
        for (std::uint8_t i = 0; i < item->suffix_count; ++i) {
            if (item->suffixes[i].flags & PC_MOD_SLOT_FRACTURED)
                kept.push_back({PC_SIDE_SUFFIX, item->suffixes[i]});
        }
    } else if (suffix_locked && !prefix_locked) {
        for (std::uint8_t i = 0; i < item->suffix_count; ++i) {
            kept.push_back({PC_SIDE_SUFFIX, item->suffixes[i]});
        }
        for (std::uint8_t i = 0; i < item->prefix_count; ++i) {
            if (item->prefixes[i].flags & PC_MOD_SLOT_FRACTURED)
                kept.push_back({PC_SIDE_PREFIX, item->prefixes[i]});
        }
    } else {
        // keep only fractured slots
        auto keep_fractured = [&](int side, const pc_mod_slot* slots,
                                  std::uint8_t count) {
            for (std::uint8_t i = 0; i < count; ++i) {
                if (slots[i].flags & PC_MOD_SLOT_FRACTURED) {
                    kept.push_back({side, slots[i]});
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
    out.applied = before != remaining || item->rarity != rarity_before;
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
    guaranteed.weight_kind = PoolWeightKind::TargetedNatural;
    guaranteed.target_tag_id = tag_id;
    if (!add_random_mod(context, guaranteed, item)) return {};
    int target = std::min<int>(configured_rare_count(context), session.rare_affix_cap * 2);
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
    request.weight_kind = PoolWeightKind::TargetedNatural;
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
        request.weight_kind = PoolWeightKind::TargetedNatural;
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
    if (!set_eldritch_implicit(session, *item, searing, tier, chosen)) return false;
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
        return reforge(context, item, PC_RARITY_RARE, configured_rare_count(context),
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

void roll_mod_values(ActionContextImpl& context, pc_mod_slot& slot) {
    const auto& session = *context.session;
    const auto& data = *session.data;
    const auto p = session.global_index.at(slot.mod_id);
    const auto begin = data.stat_offsets[p], end = data.stat_offsets[p + 1];
    if (end - begin > PC_MAX_ROLL_VALUES)
        throw std::invalid_argument("Modifier roll count exceeds item capacity");
    slot.roll_count = static_cast<std::uint8_t>(end - begin);
    std::fill(std::begin(slot.rolls), std::end(slot.rolls), 0);
    for (auto i = begin; i < end; ++i) {
        const auto low = data.stat_min_values.at(i), high = data.stat_max_values.at(i);
        if (low > high) throw std::invalid_argument("Invalid modifier value range");
        slot.rolls[i - begin] = static_cast<std::int32_t>(
            static_cast<std::int64_t>(low) + context.rng.next_below(
                static_cast<std::uint64_t>(static_cast<std::int64_t>(high) - low) + 1));
    }
}

ActionOutcome dominance(ActionContextImpl& context, pc_item_state* item) {
    const auto& s = *context.session;
    const auto choices = dominance_choices(s, *item);
    if (choices.size() < 2) return {};
    const auto saved_rng = context.rng;
    try {
        const auto upgraded = context.rng.next_below(choices.size());
        auto removed = context.rng.next_below(choices.size() - 1);
        if (removed >= upgraded) ++removed;
        const auto a = choices[upgraded], b = choices[removed];
        auto next = dominance_result(s, *item, a, b);
        const auto index = a.side == b.side && a.index == (a.side == PC_SIDE_PREFIX ? item->prefix_count : item->suffix_count) - 1
            ? b.index : a.index;
        roll_mod_values(context, a.side == PC_SIDE_PREFIX ? next.prefixes[index] : next.suffixes[index]);
        record_direct(context, &next, a.upgrade, a.side);
        *item = next;
        return {true, 1, 2};
    } catch (...) { context.rng = saved_rng; context.last_action_trace.clear(); throw; }
}

} // namespace

std::vector<std::pair<std::uint32_t, std::uint64_t>> eldritch_implicit_weights(
        const SessionImpl& session, bool searing, std::uint32_t tier) {
    const auto& by_tier = searing ? session.eldritch_searing_tier_mod_ids : session.eldritch_eater_tier_mod_ids;
    std::vector<std::pair<std::uint32_t, std::uint64_t>> weights;
    if (!session.eldritch_eligible || tier < 1 || tier > 4 || tier >= by_tier.size()) return weights;
    const auto tag = session.data->tag_id_by_name.find("no_tier_" + std::to_string(tier) + "_eldritch_implicit");
    for (const auto id : by_tier[tier]) {
        const auto weight = active_spawn_weight(session, id, tag == session.data->tag_id_by_name.end() ? kNoTag : tag->second);
        if (weight) weights.emplace_back(id, weight);
    }
    return weights;
}

bool set_eldritch_implicit(const SessionImpl& session, pc_item_state& item,
        bool searing, std::uint32_t tier, std::uint32_t chosen) {
    const int expected_gen = searing ? session.data->gen_searing_implicit_code : session.data->gen_eater_implicit_code;
    for (std::uint32_t i = 0; i < item.implicit_count;) {
        const auto id = item.implicits[i].mod_id;
        if ((item.implicits[i].flags & PC_MOD_SLOT_ELDRITCH) && id < session.mod_count &&
                session.data->mod_gen_type_code[session.global_index[id]] == expected_gen) remove_implicit_at(&item, i);
        else ++i;
    }
    if (!add_implicit(session, &item, chosen, PC_MOD_SLOT_ELDRITCH)) return false;
    if (searing) item.searing_exarch_tier = static_cast<std::uint8_t>(tier);
    else item.eater_of_worlds_tier = static_cast<std::uint8_t>(tier);
    return true;
}

std::vector<std::pair<pc_item_state, long double>> fossil_implicit_outcomes(
        const SessionImpl& session, const pc_item_state& item, const std::vector<std::uint32_t>& fossils) {
    if (const char* reason = unavailable_fossil_reason(*session.data, fossils))
        throw std::invalid_argument(reason);
    std::vector<std::pair<pc_item_state, long double>> results{{item, 1.0L}};
    const auto append_choices = [&](const std::vector<std::pair<std::uint32_t, std::uint64_t>>& weights, bool corrupts) {
        std::uint64_t total = 0;
        for (const auto& [id, weight] : weights) total += weight;
        if (!total) return;
        std::vector<std::pair<pc_item_state, long double>> next;
        if (results.size() > 100000 / weights.size()) throw std::invalid_argument("Fossil implicit outcomes exceed Calculator work limit");
        for (const auto& [base, probability] : results) for (const auto& [id, weight] : weights) {
            auto copy = base;
            if (add_implicit(session, &copy, id) && corrupts) copy.item_flags |= PC_ITEM_CORRUPTED;
            next.emplace_back(copy, probability * weight / total);
        }
        results = std::move(next);
    };
    for (const auto fossil : fossils) {
        const auto& data = *session.data;
        if (data.string_at(data.fossil_name_sids.at(fossil)) == "Bloodstained Fossil") {
            std::vector<std::pair<std::uint32_t, std::uint64_t>> weights;
            for (const auto id : ids_from_mask(session, session.corrupted_implicit_mask)) {
                const auto weight = active_spawn_weight(session, id);
                if (weight) weights.emplace_back(id, weight);
            }
            append_choices(weights, true);
        }
        if (fossil < session.fossil_sell_price_mod_ids.size()) {
            std::vector<std::pair<std::uint32_t, std::uint64_t>> weights;
            for (const auto id : session.fossil_sell_price_mod_ids[fossil]) weights.emplace_back(id, 1);
            append_choices(weights, false);
        }
        if (fossil < data.fossil_mirrors.size() && data.fossil_mirrors[fossil])
            for (auto& [copy, p] : results) copy.item_flags |= PC_ITEM_MIRRORED;
    }
    return results;
}

std::vector<DominanceChoice> dominance_choices(const SessionImpl& s, const pc_item_state& source) {
    const auto* item = &source;
    const auto& d = *s.data;
    const auto& cls = d.string_at(d.item_class_key_sid.at(d.item_class_index_by_id.at(d.base_item_class_id[s.base_index])));
    if (item->rarity != PC_RARITY_MAGIC && item->rarity != PC_RARITY_RARE) return {};
    if (cls != "Helmet" && cls != "Body Armour" && cls != "Gloves" && cls != "Boots") return {};
    std::vector<DominanceChoice> choices;
    for (int side : {PC_SIDE_PREFIX, PC_SIDE_SUFFIX}) {
        if (side_locked(s, item, side)) continue;
        const auto* slots = side == PC_SIDE_PREFIX ? item->prefixes : item->suffixes;
        const auto count = side == PC_SIDE_PREFIX ? item->prefix_count : item->suffix_count;
        for (std::uint8_t i = 0; i < count; ++i) {
            const auto id = slots[i].mod_id;
            if (id >= s.mod_count || slots[i].flags & (PC_MOD_SLOT_FRACTURED | PC_MOD_SLOT_CRAFTED | PC_MOD_SLOT_VEILED)) continue;
            const int influence = s.influence_code[id];
            if (influence <= 0) continue;
            const auto p = s.global_index[id], global = d.mod_global_ids[p];
            auto destination = kNoMod;
            if (std::any_of(d.influence_elevations.begin(), d.influence_elevations.end(),
                    [&](const auto& link) { return link.second == global; })) {
                destination = id; // Already elevated: reroll the values.
            } else {
                const auto selector = s.selector_tag_by_influence.at(influence);
                if (selector < 0 || !active_spawn_weight(s, id, selector)) continue; // Retired tier.
                const auto elevation = d.influence_elevations.find(global);
                if (elevation != d.influence_elevations.end()) {
                    const auto found = s.session_id_by_global_id.find(elevation->second);
                    if (found != s.session_id_by_global_id.end()) destination = found->second;
                } else {
                    // Tier identity is canonical mod type, generation side and influence.
                    // Item level limits rolling a tier, but not upgrading to it.
                    auto next_level = std::numeric_limits<std::uint32_t>::max();
                    bool ambiguous = false;
                    for (std::uint32_t candidate = 0; candidate < s.mod_count; ++candidate) {
                        const auto q = s.global_index[candidate];
                        if (s.gen_type[candidate] != side || s.influence_code[candidate] != influence ||
                            d.mod_type_key_sid[q] != d.mod_type_key_sid[p] ||
                            d.mod_required_level[q] <= d.mod_required_level[p] ||
                            !active_spawn_weight(s, candidate, selector)) continue;
                        const auto level = d.mod_required_level[q];
                        if (level < next_level) { next_level = level; destination = candidate; ambiguous = false; }
                        else if (level == next_level) ambiguous = true;
                    }
                    if (ambiguous) throw std::invalid_argument("Dominance tier progression is ambiguous for this modifier");
                }
            }
            if (destination == kNoMod || s.gen_type[destination] != side)
                throw std::invalid_argument("Dominance upgrade mapping is unavailable for this modifier in the current data");
            choices.push_back({side, i, destination});
        }
    }
    return choices;
}

pc_item_state dominance_result(const SessionImpl& s, const pc_item_state& item,
        const DominanceChoice& a, const DominanceChoice& b) {
    pc_item_state next = item;
    auto& slot = a.side == PC_SIDE_PREFIX ? next.prefixes[a.index] : next.suffixes[a.index];
    slot.mod_id = a.upgrade;
    slot.group_id = static_cast<std::uint16_t>(s.primary_group[a.upgrade]);
    slot.roll_count = 0;
    pc_item_remove_at(&next, b.side, b.index);
    const auto index = a.side == b.side && a.index == (a.side == PC_SIDE_PREFIX ? item.prefix_count : item.suffix_count) - 1
        ? b.index : a.index;
    if (groups_conflict(s, &next, a.upgrade, a.side, index))
        throw std::invalid_argument("Dominance upgrade conflicts with another retained modifier group");
    return next;
}

// Owner-approved affix projection: sockets are not observed or sampled, but
// their 25% outcome retains its probability mass. Existing socket fields are
// carried through, not a claim about the in-game result of a reforge.
std::vector<std::pair<std::uint32_t, std::uint64_t>> vaal_implicit_weights(const SessionImpl& session) {
    const auto& d = *session.data;
    const std::string& cls = d.string_at(d.item_class_key_sid.at(d.item_class_index_by_id.at(d.base_item_class_id[session.base_index])));
    static const std::unordered_set<std::string> equipment{
        "Amulet", "Belt", "Ring", "Quiver", "Helmet", "Body Armour", "Gloves", "Boots", "Shield",
        "Claw", "Dagger", "Rune Dagger", "Wand", "One Hand Sword", "Thrusting One Hand Sword",
        "One Hand Axe", "One Hand Mace", "Sceptre", "Bow", "Staff", "Warstaff",
        "Two Hand Sword", "Two Hand Axe", "Two Hand Mace"};
    if (!equipment.count(cls))
        throw std::invalid_argument("Vaal supports ordinary equipment; jewel and unique transformation outcomes are not modelled");
    const auto ids = ids_from_mask(session, session.corrupted_implicit_mask);
    std::vector<std::pair<std::uint32_t, std::uint64_t>> weights;
    for (const auto id : ids) {
        const auto weight = active_spawn_weight(session, id);
        if (weight) weights.emplace_back(id, weight);
    }
    if (weights.empty()) throw std::invalid_argument("Vaal corruption implicit pool is unavailable for this base");
    return weights;
}

pc_item_state vaal_implicit_result(const SessionImpl& session, const pc_item_state& item,
        std::uint32_t chosen, std::uint32_t removed) {
    const auto& d = *session.data;
    auto next = item;
    if (next.implicit_count) {
        if (removed >= next.implicit_count) throw std::invalid_argument("Invalid Vaal implicit replacement");
        const auto p = session.global_index.at(next.implicits[removed].mod_id);
        if (d.mod_gen_type_code[p] == d.gen_searing_implicit_code) next.searing_exarch_tier = 0;
        if (d.mod_gen_type_code[p] == d.gen_eater_implicit_code) next.eater_of_worlds_tier = 0;
        remove_implicit_at(&next, removed);
    }
    if (!add_implicit(session, &next, chosen))
        throw std::invalid_argument("Vaal implicit result exceeds item capacity");
    next.item_flags |= PC_ITEM_CORRUPTED;
    return next;
}

ActionOutcome vaal_equipment(ActionContextImpl& context, pc_item_state* item) {
    const auto& session = *context.session;
    std::vector<std::uint32_t> ids;
    for (const auto& [id, weight] : vaal_implicit_weights(session)) ids.push_back(id);
    const auto saved_rng = context.rng;
    try {
        pc_item_state next = *item;
        ActionOutcome result{true, 0, 0};
        switch (context.rng.next_below(4)) {
        case 0: {
            const auto chosen = pick_weighted_id(context, nullptr, ids);
            const auto removed = next.implicit_count ? context.rng.next_below(next.implicit_count) : 0;
            next = vaal_implicit_result(session, next, chosen, removed);
            roll_mod_values(context, next.implicits[next.implicit_count - 1]);
            record_direct(context, &next, chosen, -1);
            break;
        }
        case 1: break; // Socket-only outcome is a no-op in this affix projection.
        case 2: {
            const auto kept = collect_preserved(session, &next);
            std::uint8_t prefixes = 0, suffixes = 0;
            for (const auto& slot : kept) (slot.side == PC_SIDE_PREFIX ? prefixes : suffixes)++;
            result = reforge(context, &next, PC_RARITY_RARE, 6, PoolBuildRequest{});
            // Use the ordinary reforge exhaustion rule if a very low-level
            // pool cannot supply six mutually compatible affixes.
            for (auto i = prefixes; i < next.prefix_count; ++i) roll_mod_values(context, next.prefixes[i]);
            for (auto i = suffixes; i < next.suffix_count; ++i) roll_mod_values(context, next.suffixes[i]);
            break;
        }
        case 3: break;
        }
        next.item_flags |= PC_ITEM_CORRUPTED;
        *item = next;
        return result;
    } catch (...) { context.rng = saved_rng; throw; }
}

bool ensure_unveil_options(
    ActionContextImpl& context,
    pc_item_state* item) {
    if (item == nullptr) return false;
    const auto ensure_side = [&](
                                 pc_mod_slot* slots,
                                 const std::uint8_t count,
                                 const int side) {
        for (std::uint8_t index = 0; index < count; ++index) {
            pc_mod_slot& slot = slots[index];
            if ((slot.flags & PC_MOD_SLOT_VEILED) == 0 ||
                slot.veiled_option_count != 0) {
                continue;
            }
            if (!generate_unveil_options(
                    context, item, side, index)) {
                return false;
            }
        }
        return true;
    };
    return ensure_side(
               item->prefixes, item->prefix_count, PC_SIDE_PREFIX) &&
           ensure_side(
               item->suffixes, item->suffix_count, PC_SIDE_SUFFIX);
}

ActionOutcome apply_action(
    ActionContextImpl& context,
    pc_item_state* item,
    const ActionParameters& action) {
    const SessionImpl& session = *context.session;
    if (action.type == ActionType::Fossil)
        if (const char* reason = unavailable_fossil_reason(*session.data, action.fossil_indices))
            throw std::invalid_argument(reason);
    if (item->item_flags & PC_ITEM_FORESEEN)
        throw std::invalid_argument("Foreseeing item requires its original native Lock context; ordinary action sampling cannot drop foresight");
    if (session.is_cluster() && !cluster_currency_qualified(action.type))
        throw std::invalid_argument("This cluster action law is not yet approved and qualified");
    if (item->enchantment_count && action.type != ActionType::Vaal && action.type != ActionType::Dominance)
        throw std::invalid_argument("Crafting on retained enchantments is unavailable until their effect and socket contracts are implemented");
    if (item->memory_strands > 100 || item->lifecycle > PC_ITEM_DESTROYED)
        throw std::invalid_argument("Invalid memory strand count or item lifecycle");
    if (const char* reason = unavailable_currency_reason(action.type))
        throw std::invalid_argument(reason);
    if (item->memory_strands > 0 || action.type == ActionType::Remembrance ||
        action.type == ActionType::Unravelling) {
        pc_memory_interaction interaction{};
        pc_action_memory_interaction(static_cast<int>(action.type), &interaction);
        throw std::invalid_argument(interaction.unavailable_reason);
    }
    if (context.capture_action_trace) {
        context.last_action_trace.clear();
    }
    if (!item_craftable(item)) return {};
    switch (action.type) {
    case ActionType::Vaal:
        return vaal_equipment(context, item);
    case ActionType::Dominance:
        return dominance(context, item);
    case ActionType::Tempering:
    case ActionType::Tailoring:
    case ActionType::DoubleCorruption:
        return {}; // Explicit evidence guard above rejects before sampling.
    case ActionType::Remembrance:
    case ActionType::Unravelling:
        return {}; // Unknown-law guard above always rejects these actions.
    case ActionType::Transmute:
        if (item->rarity != PC_RARITY_NORMAL) {
            return {};
        }
        return reforge(context, item, PC_RARITY_MAGIC, magic_count(context),
                       PoolBuildRequest{});
    case ActionType::Augment:
    case ActionType::FoulbornAugment:
        if (item->rarity != PC_RARITY_MAGIC) {
            return {};
        }
        return do_add_one(context, item, is_foulborn(action.type));
    case ActionType::Alteration:
        if (item->rarity != PC_RARITY_MAGIC) {
            return {};
        }
        return reforge(context, item, PC_RARITY_MAGIC, magic_count(context),
                       PoolBuildRequest{});
    case ActionType::Regal:
    case ActionType::FoulbornRegal: {
        if (item->rarity != PC_RARITY_MAGIC) {
            return {};
        }
        pc_item_state upgraded = *item;
        upgraded.rarity = PC_RARITY_RARE;
        ActionOutcome out = do_add_one(context, &upgraded, is_foulborn(action.type));
        *item = upgraded;
        out.applied = true; // the magic -> rare upgrade always applies
        return out;
    }
    case ActionType::Alchemy:
        if (item->rarity != PC_RARITY_NORMAL) {
            return {};
        }
        return reforge(context, item, PC_RARITY_RARE, configured_rare_count(context),
                       PoolBuildRequest{});
    case ActionType::Chaos:
        if (item->rarity != PC_RARITY_RARE) {
            return {};
        }
        return reforge(context, item, PC_RARITY_RARE, configured_rare_count(context),
                       PoolBuildRequest{});
    case ActionType::Exalt:
    case ActionType::FoulbornExalt:
        if (item->rarity != PC_RARITY_RARE) {
            return {};
        }
        return do_add_one(context, item, is_foulborn(action.type));
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
        const int target = configured_rare_count(context);
        const ActionTransitionFacts facts =
            action_transition_facts(ActionType::Essence);
        PoolBuildRequest pool_request;
        pool_request.respects_metamod_pool_blocks =
            facts.respects_metamod_pool_blocks;
        return reforge(
            context, item, PC_RARITY_RARE,
            std::min<int>(target, session.rare_affix_cap * 2), pool_request,
            {guaranteed}, facts.respects_metamod_side_locks);
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
        const ActionTransitionFacts facts =
            action_transition_facts(ActionType::Fossil);
        pool_request.respects_metamod_pool_blocks =
            facts.respects_metamod_pool_blocks;
        ActionOutcome out = reforge(
            context, item, PC_RARITY_RARE,
            std::min<int>(configured_rare_count(context), session.rare_affix_cap * 2),
            pool_request, forced, facts.respects_metamod_side_locks);
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
            configured_rare_count(context), session.rare_affix_cap * 2);
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
            (item->item_flags & PC_ITEM_SYNTHESISED) ||
            action.influence_code <= 0 || action.influence_code > 8 ||
            item->generic_influence_bits || item->searing_exarch_tier ||
            item->eater_of_worlds_tier ||
            pc_item_find_fractured(item, nullptr, nullptr) == PC_RESULT_OK) {
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

AwakenerChoices awakener_choices(const SessionImpl& session,
        const SessionImpl& donor_session, const pc_item_state& donor,
        const pc_item_state& receiver) {
    const auto& a = *donor_session.data;
    const auto& b = *session.data;
    if (donor_session.data != session.data &&
        (a.artifact_game_data_hash.empty() || a.artifact_game_data_hash != b.artifact_game_data_hash ||
         a.artifact_strings_hash != b.artifact_strings_hash))
        throw std::invalid_argument("Resource data identities are incompatible");
    if (a.base_item_class_id[donor_session.base_index] != b.base_item_class_id[session.base_index])
        throw std::invalid_argument("Awakener input item classes differ");
    const auto eligible = [](const pc_item_state& item) {
        const auto influence = item.generic_influence_bits;
        return item_craftable(&item) && influence && !(influence & (influence - 1)) &&
            !item.searing_exarch_tier && !item.eater_of_worlds_tier && !item.memory_strands &&
            !(item.item_flags & PC_ITEM_SYNTHESISED) &&
            pc_item_find_fractured(&item, nullptr, nullptr) != PC_RESULT_OK;
    };
    if (!eligible(donor) || !eligible(receiver) || donor.generic_influence_bits == receiver.generic_influence_bits)
        throw std::invalid_argument("Awakener requires live, craftable items with distinct single influences, no fractures, synthesised state, Eldritch influence or memory strands");
    auto candidates = [&](const SessionImpl& source, const pc_item_state& item) {
        std::vector<std::uint32_t> result;
        const auto visit = [&](const pc_mod_slot* slots, unsigned count) {
            for (unsigned i = 0; i < count; ++i) {
                const auto& slot = slots[i];
                if (slot.mod_id >= source.mod_count) throw std::invalid_argument("Input modifier is outside its session");
                if (slot.roll_count) throw std::invalid_argument("Awakener currently supports structural modifiers only; numerical reroll law is not implemented");
                const auto influence = source.influence_code[slot.mod_id];
                if (influence <= 0 || influence > 8 || !(item.generic_influence_bits & (1u << (influence - 1)))) continue;
                if (slot.flags) throw std::invalid_argument("Awakener special influenced modifier retention is unresolved");
                const auto source_global = source.global_index[slot.mod_id];
                const auto& key = source.data->string_at(source.data->mod_key_sid[source_global]);
                const auto target_global = session.data->mod_pos_by_key.find(key);
                if (target_global == session.data->mod_pos_by_key.end()) throw std::invalid_argument("Transferred modifier key is unavailable in result data");
                const auto target = session.session_id_by_global_id.find(session.data->mod_global_ids[target_global->second]);
                if (target == session.session_id_by_global_id.end()) throw std::invalid_argument("Transferred tier cannot be retained on the receiver base");
                result.push_back(target->second);
            }
        };
        visit(item.prefixes, item.prefix_count); visit(item.suffixes, item.suffix_count);
        if (result.empty()) throw std::invalid_argument("Awakener requires an eligible influenced modifier on each input");
        return result;
    };
    const auto from_donor = candidates(donor_session, donor);
    const auto from_receiver = candidates(session, receiver);
    // Preflight every possible pair before sampling. Never reject-and-resample
    // collisions, which would silently change the approved selection law.
    for (auto a : from_donor) for (auto b : from_receiver) {
        for (auto x = session.group_offsets[a]; x < session.group_offsets[a + 1]; ++x)
            for (auto y = session.group_offsets[b]; y < session.group_offsets[b + 1]; ++y)
                if (session.group_ids[x] == session.group_ids[y])
                    throw std::invalid_argument("Awakener group collision discard probabilities are unresolved");
    }
    return {from_donor, from_receiver};
}

pc_item_state awakener_base(const SessionImpl& session, const pc_item_state& donor,
        const pc_item_state& receiver, std::uint32_t donor_mod, std::uint32_t receiver_mod) {
    pc_item_state result = receiver;
    pc_item_clear_side(&result, PC_SIDE_PREFIX); pc_item_clear_side(&result, PC_SIDE_SUFFIX);
    result.rarity = PC_RARITY_RARE;
    result.generic_influence_bits |= donor.generic_influence_bits;
    for (const auto id : {donor_mod, receiver_mod}) {
        if (pc_item_add_mod(&result, session.gen_type[id], id,
                static_cast<std::uint16_t>(session.primary_group[id]), 0, nullptr) != PC_RESULT_OK)
            throw std::invalid_argument("Retained Awakener modifiers exceed receiver capacity");
    }
    return result;
}

pc_item_state awaken_item(ActionContextImpl& context,
        const SessionImpl& donor_session, const pc_item_state& donor,
        const pc_item_state& receiver) {
    const auto choices = awakener_choices(*context.session, donor_session, donor, receiver);
    const auto a = choices.donor[context.rng.next_below(choices.donor.size())];
    const auto b = choices.receiver[context.rng.next_below(choices.receiver.size())];
    auto result = awakener_base(*context.session, donor, receiver, a, b);
    PoolBuildRequest request;
    request.respects_metamod_pool_blocks = false;
    fill_random_mods(context, request, &result, configured_rare_count(context));
    return result;
}

ActionOutcome visit_cluster_currency_outcomes(ActionContextImpl& context,
    const pc_item_state& original, const ActionParameters& action,
    const std::function<void(const pc_item_state&, long double)>& visit,
    std::uint64_t max_work,
    const std::function<std::uint32_t(const pc_item_state&)>& terminal_observation,
    const std::function<void(std::uint64_t)>& require_scratch_bytes) {
    const auto& s = *context.session;
    if (!s.is_cluster()) throw std::invalid_argument("Concrete cluster outcomes require a configured session");
    if (!cluster_currency_qualified(action.type))
        throw std::invalid_argument("This cluster action law is not yet approved and qualified");
    if (!item_craftable(&original)) { visit(original, 1); return {}; }
    if (original.memory_strands || original.enchantment_count)
        throw std::invalid_argument("Unsupported cluster input carrier");
    std::uint64_t work = 0;
    std::uint64_t frontier_bytes = 0;
    const auto require_memory = [&](std::uint64_t bytes) {
        frontier_bytes = bytes;
        if (require_scratch_bytes) require_scratch_bytes(bytes);
        else if (bytes > 128ull * 1024 * 1024)
            throw std::length_error("Concrete cluster outcome scratch byte limit exceeded");
    };
    const auto emit = [&](const pc_item_state& item, long double probability) {
        if (action.type == ActionType::Fossil) {
            const auto special = fossil_implicit_outcomes(s, item, action.fossil_indices);
            const auto saved_frontier = frontier_bytes;
            require_memory(saved_frontier + special.capacity() * sizeof(special.front()));
            for (const auto& [child, conditional] : special) visit(child, probability * conditional);
            require_memory(saved_frontier);
        } else visit(item, probability);
    };
    const auto fill = [&](const pc_item_state& base, int target, long double mass,
                          const PoolBuildRequest& request) -> bool {
        using Key = std::vector<std::uint64_t>;
        struct Entry { pc_item_state item; long double mass; };
        std::map<Key, Entry> current, next;
        const auto key_for = [&](const pc_item_state& item) {
            Key key{item.rarity, item.prefix_count, item.suffix_count, item.item_flags};
            if (terminal_observation) {
                // This within-addition equivalence retains the COMPLETE native
                // blocker mask and installed added-tag signature. Its separate
                // observation token is used only by the terminal Calculator.
                // No removals or continuations consume a chosen representative.
                build_refill_group_block_mask(s, &item, context.block_mask_scratch);
                key.insert(key.end(), context.block_mask_scratch.begin(), context.block_mask_scratch.end());
                key.push_back(intern_item_tag_signature(context, &item));
                key.push_back(item_has_metamod(s, &item, s.data->metamod_no_attack_code));
                key.push_back(item_has_metamod(s, &item, s.data->metamod_no_caster_code));
                key.push_back(terminal_observation(item));
            } else {
                // Authored continuation uses exact physical IDs and flags.
                for (int side : {PC_SIDE_PREFIX, PC_SIDE_SUFFIX}) {
                    const auto* slots = side == PC_SIDE_PREFIX ? item.prefixes : item.suffixes;
                    const auto count = side == PC_SIDE_PREFIX ? item.prefix_count : item.suffix_count;
                    std::vector<std::uint64_t> values;
                    for (unsigned i = 0; i < count; ++i)
                        values.push_back((std::uint64_t(slots[i].mod_id) << 32) | (std::uint64_t(slots[i].group_id) << 16) | slots[i].flags);
                    std::sort(values.begin(), values.end());
                    key.insert(key.end(), values.begin(), values.end());
                }
            }
            return key;
        };
        const auto per_node = sizeof(Entry) + sizeof(Key) + 128 +
            2 * (4 + s.words + PC_MAX_PREFIXES + PC_MAX_SUFFIXES + 4) * sizeof(std::uint64_t);
        require_memory(per_node);
        current.emplace(key_for(base), Entry{base, mass});
        bool applied = base.prefix_count + base.suffix_count >= target;
        while (!current.empty()) {
            next.clear();
            for (const auto& [key, entry] : current) {
                (void)key;
                const auto& item = entry.item;
                if (item.prefix_count + item.suffix_count >= target) { emit(item, entry.mass); continue; }
                auto prepared = request;
                const auto side = open_side_filter(s, &item, prepared);
                if (side == -2) { emit(item, entry.mass); continue; }
                prepared.side_filter = side;
                const auto& pool = get_weighted_pool(context, &item, prepared);
                if (!pool.total_weight) { emit(item, entry.mass); continue; }
                applied = true;
                const auto rows = pool.entries;
                const auto total = pool.total_weight;
                for (const auto& row : rows) {
                    if (++work > max_work) throw std::length_error("Concrete cluster outcome work limit exceeded");
                    auto child = item;
                    if (!add_direct_mod(s, &child, row.session_mod_id))
                        throw std::logic_error("Native cluster pool contains an illegal addition");
                    const auto p = entry.mass * static_cast<long double>(row.final_weight) / total;
                    if (child.prefix_count + child.suffix_count >= target) {
                        emit(child, p);
                        continue;
                    }
                    auto child_key = key_for(child);
                    // Conservative node/key allocation bound checked before allocation.
                    require_memory((current.size() + next.size() + 1) * per_node +
                        rows.capacity() * sizeof(PoolEntry));
                    const auto [position, inserted] = next.try_emplace(std::move(child_key), Entry{child, 0});
                    position->second.mass += p;
                }
            }
            current.swap(next);
            // next still owns the preceding frontier until the next clear.
            require_memory((current.size() + next.size()) * per_node);
        }
        require_memory(0);
        return applied;
    };
    const auto rare_fill = [&](const pc_item_state& base, long double mass, const PoolBuildRequest& request) {
        fill(base, 3, mass * 0.65L, request);
        fill(base, 4, mass * 0.35L, request);
    };
    switch (action.type) {
    case ActionType::Transmute: case ActionType::Alteration: {
        if (original.rarity != (action.type == ActionType::Transmute ? PC_RARITY_NORMAL : PC_RARITY_MAGIC)) {
            visit(original, 1); return {};
        }
        auto base = original;
        restore_slots(&base, collect_preserved(s, &original, true));
        base.rarity = PC_RARITY_MAGIC;
        fill(base, 1, 0.5L, {}); fill(base, 2, 0.5L, {});
        return {true, 0, 0};
    }
    case ActionType::Augment: case ActionType::Regal: case ActionType::Exalt:
    case ActionType::FoulbornAugment: case ActionType::FoulbornRegal: case ActionType::FoulbornExalt: {
        const auto ordinary = ordinary_add_equivalent(action.type);
        if (original.rarity != (ordinary == ActionType::Exalt ? PC_RARITY_RARE : PC_RARITY_MAGIC)) {
            visit(original, 1); return {};
        }
        auto base = original;
        if (ordinary == ActionType::Regal) base.rarity = PC_RARITY_RARE;
        PoolBuildRequest request;
        if (is_foulborn(action.type)) request.weight_kind = PoolWeightKind::Foulborn;
        const auto applied = fill(base, base.prefix_count + base.suffix_count + 1, 1, request);
        return {applied || ordinary == ActionType::Regal, 0, 0};
    }
    case ActionType::Alchemy: case ActionType::Chaos: case ActionType::Fossil: case ActionType::HarvestReforge: {
        if ((action.type == ActionType::Alchemy && original.rarity != PC_RARITY_NORMAL) ||
            ((action.type == ActionType::Chaos || action.type == ActionType::HarvestReforge) && original.rarity != PC_RARITY_RARE)) {
            visit(original, 1); return {};
        }
        auto base = original;
        const bool fossil = action.type == ActionType::Fossil;
        restore_slots(&base, collect_preserved(s, &original, !fossil));
        base.rarity = PC_RARITY_RARE;
        PoolBuildRequest request;
        if (fossil) {
            if (action.fossil_indices.empty()) { visit(original, 1); return {}; }
            request.weight_kind = PoolWeightKind::Fossil;
            request.fossil_indices = action.fossil_indices;
            request.respects_metamod_pool_blocks = false;
            std::vector<std::uint32_t> forced;
            for (const auto f : action.fossil_indices) {
                if (f >= s.fossil_forced_mod_ids.size()) { visit(original, 1); return {}; }
                forced.insert(forced.end(), s.fossil_forced_mod_ids[f].begin(), s.fossil_forced_mod_ids[f].end());
            }
            std::sort(forced.begin(), forced.end());
            forced.erase(std::unique(forced.begin(), forced.end()), forced.end());
            for (const auto id : forced)
                if (!add_direct_mod(s, &base, id)) { visit(original, 1); return {}; }
        }
        if (action.type == ActionType::HarvestReforge) {
            if (action.target_tag_id == kNoTag) { visit(original, 1); return {}; }
            auto guaranteed = request;
            guaranteed.weight_kind = PoolWeightKind::TargetedNatural;
            guaranteed.target_tag_id = action.target_tag_id;
            guaranteed.side_filter = open_side_filter(s, &base, guaranteed);
            if (guaranteed.side_filter == -2) { visit(original, 1); return {}; }
            const auto& pool = get_weighted_pool(context, &base, guaranteed);
            const auto rows = pool.entries; const auto total = pool.total_weight;
            if (!total) { visit(original, 1); return {}; }
            for (const auto& row : rows) {
                auto child = base;
                if (!add_direct_mod(s, &child, row.session_mod_id)) throw std::logic_error("Harvest guaranteed pool has an illegal addition");
                rare_fill(child, static_cast<long double>(row.final_weight) / total, request);
            }
        } else rare_fill(base, 1, request);
        return {true, 0, 0};
    }
    case ActionType::HarvestAugment: {
        if ((original.rarity != PC_RARITY_MAGIC && original.rarity != PC_RARITY_RARE) ||
            original.generic_influence_bits || original.searing_exarch_tier || original.eater_of_worlds_tier ||
            action.target_tag_id == kNoTag) { visit(original, 1); return {}; }
        PoolBuildRequest request;
        request.weight_kind = PoolWeightKind::TargetedNatural;
        request.target_tag_id = action.target_tag_id;
        request.side_filter = open_side_filter(s, &original, request);
        if (request.side_filter == -2) { visit(original, 1); return {}; }
        const auto& pool = get_weighted_pool(context, &original, request);
        const auto rows = pool.entries; const auto total = pool.total_weight;
        if (!total) { visit(original, 1); return {}; }
        for (const auto& row : rows) {
            if (++work > max_work) throw std::length_error("Concrete cluster outcome work limit exceeded");
            auto child = original;
            if (!add_direct_mod(s, &child, row.session_mod_id)) throw std::logic_error("Harvest augment pool has an illegal addition");
            std::vector<std::pair<int, std::uint8_t>> removable;
            for (int side : {PC_SIDE_PREFIX, PC_SIDE_SUFFIX}) {
                if (side_locked(s, &child, side)) continue;
                const auto* slots = side == PC_SIDE_PREFIX ? child.prefixes : child.suffixes;
                const auto count = side == PC_SIDE_PREFIX ? child.prefix_count : child.suffix_count;
                for (std::uint8_t i = 0; i < count; ++i)
                    if (!(slots[i].flags & PC_MOD_SLOT_FRACTURED) && slots[i].mod_id != row.session_mod_id)
                        removable.emplace_back(side, i);
            }
            const auto mass = static_cast<long double>(row.final_weight) / total;
            if (removable.empty()) visit(child, mass);
            else for (const auto& [side, index] : removable) {
                auto next = child; pc_item_remove_at(&next, side, index);
                visit(next, mass / removable.size());
            }
        }
        return {true, 1, 1};
    }
    case ActionType::HarvestResist: {
        if ((original.rarity != PC_RARITY_MAGIC && original.rarity != PC_RARITY_RARE) ||
            action.source_tag_id == kNoTag || action.target_tag_id == kNoTag ||
            action.source_tag_id == action.target_tag_id) { visit(original, 1); return {}; }
        const auto resistance = s.data->tag_id_by_name.find("resistance");
        if (resistance == s.data->tag_id_by_name.end()) { visit(original, 1); return {}; }
        struct Source { pc_item_state base; std::vector<PoolEntry> rows; std::uint64_t total = 0; };
        std::vector<Source> viable;
        for (int side : {PC_SIDE_PREFIX, PC_SIDE_SUFFIX}) {
            if (side_locked(s, &original, side)) continue;
            const auto* slots = side == PC_SIDE_PREFIX ? original.prefixes : original.suffixes;
            const auto count = side == PC_SIDE_PREFIX ? original.prefix_count : original.suffix_count;
            for (std::uint8_t i = 0; i < count; ++i) {
                const auto id = slots[i].mod_id;
                if ((slots[i].flags & PC_MOD_SLOT_FRACTURED) || !has_class_tag(s, id, resistance->second) ||
                    !has_class_tag(s, id, action.source_tag_id)) continue;
                Source source{original, {}, 0}; pc_item_remove_at(&source.base, side, i);
                PoolBuildRequest request;
                request.weight_kind = PoolWeightKind::TargetedNatural;
                request.target_tag_id = action.target_tag_id; request.side_filter = side;
                const auto& pool = get_weighted_pool(context, &source.base, request);
                for (const auto& row : pool.entries) {
                    if (row.required_level != s.required_level[id] || !has_class_tag(s, row.session_mod_id, resistance->second) ||
                        has_class_tag(s, row.session_mod_id, action.source_tag_id)) continue;
                    source.rows.push_back(row); source.total += row.final_weight;
                }
                if (source.total) viable.push_back(std::move(source));
            }
        }
        if (viable.empty()) { visit(original, 1); return {}; }
        // Native retries impossible sources without replacement and restores all
        // physical facts. The first viable source is uniform over viable sources.
        for (const auto& source : viable) for (const auto& row : source.rows) {
            if (++work > max_work) throw std::length_error("Concrete cluster outcome work limit exceeded");
            auto next = source.base;
            if (!add_direct_mod(s, &next, row.session_mod_id)) throw std::logic_error("Harvest resistance pool has an illegal replacement");
            visit(next, static_cast<long double>(row.final_weight) / source.total / viable.size());
        }
        return {true, 1, 1};
    }
    case ActionType::RemoveCraftedModifiers: {
        auto next = original; const auto result = do_remove_crafted_modifiers(&next);
        visit(next, 1); return result;
    }
    case ActionType::Annul: {
        std::vector<std::pair<int, std::uint8_t>> removable;
        for (const auto side : {PC_SIDE_PREFIX, PC_SIDE_SUFFIX}) {
            if (side_locked(s, &original, side)) continue;
            const auto* slots = side == PC_SIDE_PREFIX ? original.prefixes : original.suffixes;
            const auto count = side == PC_SIDE_PREFIX ? original.prefix_count : original.suffix_count;
            for (std::uint8_t i = 0; i < count; ++i)
                if (!(slots[i].flags & PC_MOD_SLOT_FRACTURED)) removable.emplace_back(side, i);
        }
        if (removable.empty()) { visit(original, 1); return {}; }
        for (const auto& [side, index] : removable) {
            auto next = original; pc_item_remove_at(&next, side, index);
            visit(next, 1.0L / removable.size());
        }
        return {true, 0, 1};
    }
    case ActionType::Scour: {
        auto next = original; const auto result = do_scour(s, &next);
        visit(next, 1); return result;
    }
    default: throw std::invalid_argument("This cluster action law is not yet approved and qualified");
    }
}

} // namespace poecraft
