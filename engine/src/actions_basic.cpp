#include "engine_internal.hpp"

#include "poecraft/item_state.h"

#include <cstdint>
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

// Draw one mod from the open affix sides and append it. Returns false when no
// side is open or no positively-weighted, group-legal candidate exists.
bool add_random_mod(const SessionImpl& session, Rng& rng, pc_item_state* item) {
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

    const std::vector<PoolEntry> pool =
        build_normal_pool(session, item, side_filter);
    std::uint64_t total = 0;
    for (const PoolEntry& e : pool) {
        total += e.final_weight;
    }
    if (total == 0) {
        return false;
    }

    // Weight-proportional selection across the combined prefix/suffix
    // distribution (never a 50/50 side choice).
    std::uint64_t roll = rng.next_below(total);
    std::size_t idx = 0;
    std::uint64_t acc = 0;
    for (; idx < pool.size(); ++idx) {
        acc += pool[idx].final_weight;
        if (roll < acc) {
            break;
        }
    }
    const PoolEntry& chosen = pool[idx];
    return pc_item_add_mod(item,
                           chosen.gen_type == 0 ? PC_SIDE_PREFIX : PC_SIDE_SUFFIX,
                           chosen.session_mod_id,
                           static_cast<std::uint16_t>(chosen.primary_group), 0,
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
    const SessionImpl& session,
    Rng& rng,
    pc_item_state* item,
    std::uint8_t new_rarity,
    int target_total) {
    const int before = item->prefix_count + item->suffix_count;
    const std::vector<KeptSlot> kept = collect_preserved(item);
    restore_slots(item, kept);
    item->rarity = new_rarity;

    int total = item->prefix_count + item->suffix_count;
    const int preserved = total;
    while (total < target_total) {
        if (!add_random_mod(session, rng, item)) {
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

int magic_count(Rng& rng) {
    return 1 + static_cast<int>(rng.next_below(2)); // 1 or 2
}

int rare_count(Rng& rng) {
    return 4 + static_cast<int>(rng.next_below(3)); // 4, 5, or 6
}

ActionOutcome do_add_one(const SessionImpl& session, Rng& rng,
                         pc_item_state* item) {
    ActionOutcome out;
    if (add_random_mod(session, rng, item)) {
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
    const SessionImpl& session,
    Rng& rng,
    pc_item_state* item,
    ActionType action) {
    switch (action) {
    case ActionType::Transmute:
        if (item->rarity != PC_RARITY_NORMAL) {
            return {};
        }
        return reforge(session, rng, item, PC_RARITY_MAGIC, magic_count(rng));
    case ActionType::Augment:
        if (item->rarity != PC_RARITY_MAGIC) {
            return {};
        }
        return do_add_one(session, rng, item);
    case ActionType::Alteration:
        if (item->rarity != PC_RARITY_MAGIC) {
            return {};
        }
        return reforge(session, rng, item, PC_RARITY_MAGIC, magic_count(rng));
    case ActionType::Regal: {
        if (item->rarity != PC_RARITY_MAGIC) {
            return {};
        }
        item->rarity = PC_RARITY_RARE;
        ActionOutcome out = do_add_one(session, rng, item);
        out.applied = true; // the magic -> rare upgrade always applies
        return out;
    }
    case ActionType::Alchemy:
        if (item->rarity != PC_RARITY_NORMAL) {
            return {};
        }
        return reforge(session, rng, item, PC_RARITY_RARE, rare_count(rng));
    case ActionType::Chaos:
        if (item->rarity != PC_RARITY_RARE) {
            return {};
        }
        return reforge(session, rng, item, PC_RARITY_RARE, rare_count(rng));
    case ActionType::Exalt:
        if (item->rarity != PC_RARITY_RARE) {
            return {};
        }
        return do_add_one(session, rng, item);
    case ActionType::Annul:
        return do_annul(rng, item);
    case ActionType::Scour:
        return do_scour(item);
    }
    return {};
}

} // namespace poecraft
