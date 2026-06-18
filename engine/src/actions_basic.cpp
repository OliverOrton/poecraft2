#include "engine_internal.hpp"

#include "poecraft/item_state.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

/*
 * Core crafting actions. Actions mutate ItemState by sampling the Phase 5
 * normal explicit pool with the action context's RNG. Affix counts follow
 * standard Path of Exile behaviour (magic 1-2 mods, rare 4-6); the old app is a
 * reference, not a compatibility target, and no fixture pins exact RNG.
 *
 * Metamod prefix/suffix locks are honoured structurally but cannot occur yet
 * (bench metamods arrive with Phase 13); until then both locks are false.
 */
namespace poecraft {

namespace {

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

// Draw one mod from the open affix sides and append it. Prefix/suffix candidates
// share one prefix-sum table, so the side is always selected by total weight.
bool add_random_mod(
    ActionContextImpl& context,
    const PoolBuildRequest& base_request,
    pc_item_state* item) {
    const SessionImpl& session = *context.session;
    const std::uint8_t cap = max_affix(session, item);
    const bool prefix_open = item->prefix_count < cap;
    const bool suffix_open = item->suffix_count < cap;
    int side_filter;
    if (prefix_open && suffix_open) {
        side_filter = -1;
    } else if (prefix_open) {
        side_filter = 0;
    } else if (suffix_open) {
        side_filter = 1;
    } else {
        return false;
    }

    PoolBuildRequest request = base_request;
    request.side_filter = side_filter;
    bool cache_hit = false;
    const WeightedPool& pool =
        get_weighted_pool(context, item, request, &cache_hit);
    if (pool.total_weight == 0) {
        return false;
    }

    const std::uint64_t roll = context.rng.next_below(pool.total_weight);
    const auto it =
        std::lower_bound(pool.prefix_sums.begin(), pool.prefix_sums.end(),
                         roll + 1);
    const PoolEntry& chosen =
        pool.entries[static_cast<std::size_t>(it - pool.prefix_sums.begin())];
    const bool added =
        pc_item_add_mod(item,
                        chosen.gen_type == 0 ? PC_SIDE_PREFIX : PC_SIDE_SUFFIX,
                        chosen.session_mod_id,
                        static_cast<std::uint16_t>(chosen.primary_group), 0,
                        nullptr) == PC_RESULT_OK;
    if (added) {
        ActionTraceStage stage;
        stage.stage_index =
            static_cast<std::uint32_t>(context.last_action_trace.size());
        stage.cache_hit = cache_hit;
        stage.tag_signature_id = intern_item_tag_signature(context, item);
        stage.weight_kind = request.weight_kind;
        stage.side_filter = side_filter;
        stage.prefix_total_weight = pool.prefix_total_weight;
        stage.suffix_total_weight = pool.suffix_total_weight;
        stage.combined_total_weight = pool.total_weight;
        stage.roll = roll;
        stage.chosen_mod_id = chosen.session_mod_id;
        stage.chosen_side = chosen.gen_type;
        context.last_action_trace.push_back(stage);
    }
    return added;
}

bool add_direct_mod(
    const SessionImpl& session,
    pc_item_state* item,
    std::uint32_t mod_id) {
    if (mod_id >= session.mod_count) return false;
    const int side = session.gen_type[mod_id];
    if (side != 0 && side != 1) return false;
    const std::uint8_t cap = max_affix(session, item);
    if ((side == 0 && item->prefix_count >= cap) ||
        (side == 1 && item->suffix_count >= cap)) {
        return false;
    }
    for (std::uint8_t i = 0; i < item->prefix_count; ++i) {
        const std::uint32_t current = item->prefixes[i].mod_id;
        if (current >= session.mod_count) continue;
        for (std::uint32_t a = session.group_offsets[current];
             a < session.group_offsets[current + 1]; ++a) {
            for (std::uint32_t b = session.group_offsets[mod_id];
                 b < session.group_offsets[mod_id + 1]; ++b) {
                if (session.group_ids[a] == session.group_ids[b]) return false;
            }
        }
    }
    for (std::uint8_t i = 0; i < item->suffix_count; ++i) {
        const std::uint32_t current = item->suffixes[i].mod_id;
        if (current >= session.mod_count) continue;
        for (std::uint32_t a = session.group_offsets[current];
             a < session.group_offsets[current + 1]; ++a) {
            for (std::uint32_t b = session.group_offsets[mod_id];
                 b < session.group_offsets[mod_id + 1]; ++b) {
                if (session.group_ids[a] == session.group_ids[b]) return false;
            }
        }
    }
    return pc_item_add_mod(
               item, side == 0 ? PC_SIDE_PREFIX : PC_SIDE_SUFFIX, mod_id,
               static_cast<std::uint16_t>(session.primary_group[mod_id]), 0,
               nullptr) == PC_RESULT_OK;
}

struct KeptSlot {
    int side;
    std::uint32_t mod_id;
    std::uint16_t group_id;
    std::uint8_t flags;
};

bool side_locked(const pc_item_state*, int /*side*/) {
    // No bench metamods yet; neither side can be locked.
    return false;
}

// Collect the slots a reforge must keep: fractured slots, and every slot on a
// locked side.
std::vector<KeptSlot> collect_preserved(const pc_item_state* item) {
    std::vector<KeptSlot> kept;
    auto scan = [&](int side, const pc_mod_slot* slots, std::uint8_t count) {
        const bool locked = side_locked(item, side);
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
    const std::vector<KeptSlot> kept = collect_preserved(item);
    restore_slots(item, kept);
    item->rarity = new_rarity;

    int total = item->prefix_count + item->suffix_count;
    const int preserved = total;
    for (std::uint32_t direct : direct_mods) {
        if (!add_direct_mod(session, item, direct)) {
            context.last_action_trace.clear();
            return {};
        }
        ActionTraceStage stage;
        stage.stage_index =
            static_cast<std::uint32_t>(context.last_action_trace.size());
        stage.direct = true;
        stage.tag_signature_id = intern_item_tag_signature(context, item);
        stage.weight_kind = pool_request.weight_kind;
        stage.chosen_mod_id = direct;
        stage.chosen_side = session.gen_type[direct];
        context.last_action_trace.push_back(stage);
        ++total;
    }
    while (total < target_total) {
        if (!add_random_mod(context, pool_request, item)) {
            break;
        }
        ++total;
    }
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

ActionOutcome do_annul(Rng& rng, pc_item_state* item) {
    ActionOutcome out;
    const bool prefix_locked = side_locked(item, PC_SIDE_PREFIX);
    const bool suffix_locked = side_locked(item, PC_SIDE_SUFFIX);
    if (prefix_locked && suffix_locked) {
        return out;
    }
    struct Ref {
        int side;
        std::uint32_t index;
    };
    std::vector<Ref> removable;
    if (!suffix_locked) {
        for (std::uint8_t i = 0; i < item->prefix_count; ++i) {
            if (!(item->prefixes[i].flags & PC_MOD_SLOT_FRACTURED)) {
                removable.push_back({PC_SIDE_PREFIX, i});
            }
        }
    }
    if (!prefix_locked) {
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

ActionOutcome do_scour(pc_item_state* item) {
    ActionOutcome out;
    if (item->rarity == PC_RARITY_NORMAL) {
        return out; // nothing to scour
    }
    const int before = item->prefix_count + item->suffix_count;
    const bool prefix_locked = side_locked(item, PC_SIDE_PREFIX);
    const bool suffix_locked = side_locked(item, PC_SIDE_SUFFIX);

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

} // namespace

ActionOutcome apply_action(
    ActionContextImpl& context,
    pc_item_state* item,
    const ActionParameters& action) {
    const SessionImpl& session = *context.session;
    context.last_action_trace.clear();
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
        return do_annul(context.rng, item);
    case ActionType::Scour:
        return do_scour(item);
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
        return reforge(context, item, PC_RARITY_RARE, target,
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
        return reforge(context, item, PC_RARITY_RARE, rare_count(context),
                       pool_request, forced);
    }
    }
    return {};
}

} // namespace poecraft
