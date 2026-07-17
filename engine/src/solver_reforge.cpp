#include "solver_internal.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <limits>
#include <map>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "poecraft/bitset.h"
#include "poecraft/item_state.h"

/*
 * Solver S3: exact reforge evaluator (docs/crafting-solver-plan.md,
 * Calculation Engine, evaluation path 3).
 *
 * A reforge is: preserve fractured/locked slots, set rarity, add direct
 * mods (essence guaranteed, fossil forced), then sequentially fill to a
 * random target total. The sequential fill is evaluated as a forward
 * frontier DP over roll buckets:
 *
 *   goal buckets   one satisfied + one below-tier bucket per goal slot,
 *                  holding the exact pool weight of the slot's members.
 *                  Any pick against a slot occupies its exclusivity
 *                  group, killing both buckets and its blocker junk.
 *   junk buckets   (side, junk class, block mask, family weight) with a
 *                  multiplicity counting families sharing that weight.
 *                  Picking one consumes exactly one family's weight, so
 *                  group removal between rolls is exact; families inside
 *                  a bucket are interchangeable by construction.
 *
 * Frontier states record per-bucket pick counts, so remaining weights are
 * derived exactly. Every positive-probability path is retained; operational
 * reforge-work and solver-memory caps refuse oversized requests explicitly
 * instead of silently changing the transition distribution.
 */
namespace poecraft {
namespace solver {

namespace {

struct ReforgeBuildTimer {
    CalcTelemetry& telemetry;
    std::chrono::steady_clock::time_point started =
        std::chrono::steady_clock::now();
    bool miss = false;

    ~ReforgeBuildTimer() {
        if (!miss) return;
        telemetry.reforge_build_ns += static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - started)
                .count());
    }
};

std::uint8_t rarity_cap(const SessionImpl& session, std::uint8_t rarity) {
    switch (rarity) {
    case PC_RARITY_NORMAL:
        return 0;
    case PC_RARITY_MAGIC:
        return 1;
    default:
        return session.rare_affix_cap;
    }
}

bool slot_has_metamod(
    const SessionImpl& session,
    const pc_mod_slot& slot,
    int code) {
    return code >= 0 && slot.mod_id < session.metamod_type.size() &&
           session.metamod_type[slot.mod_id] == code;
}

bool reforge_side_locked(
    const SessionImpl& session,
    const pc_item_state& item,
    int side) {
    const int code = side == PC_SIDE_PREFIX
                         ? session.data->metamod_prefixes_locked_code
                         : session.data->metamod_suffixes_locked_code;
    for (std::uint8_t i = 0; i < item.prefix_count; ++i) {
        if (slot_has_metamod(session, item.prefixes[i], code)) return true;
    }
    for (std::uint8_t i = 0; i < item.suffix_count; ++i) {
        if (slot_has_metamod(session, item.suffixes[i], code)) return true;
    }
    return false;
}

/* Mirrors add_direct_mod: cap and group-conflict checks. */
bool direct_add(
    const SessionImpl& session,
    pc_item_state& item,
    std::uint32_t mod_id) {
    if (mod_id >= session.mod_count) return false;
    const std::int8_t side = session.gen_type[mod_id];
    if (side != 0 && side != 1) return false;
    const std::uint8_t cap = rarity_cap(session, item.rarity);
    if ((side == 0 && item.prefix_count >= cap) ||
        (side == 1 && item.suffix_count >= cap)) {
        return false;
    }
    for (std::uint8_t i = 0; i < item.prefix_count; ++i) {
        for (std::uint32_t a =
                 session.group_offsets[item.prefixes[i].mod_id];
             a < session.group_offsets[item.prefixes[i].mod_id + 1]; ++a) {
            for (std::uint32_t b = session.group_offsets[mod_id];
                 b < session.group_offsets[mod_id + 1]; ++b) {
                if (session.group_ids[a] == session.group_ids[b]) {
                    return false;
                }
            }
        }
    }
    for (std::uint8_t i = 0; i < item.suffix_count; ++i) {
        for (std::uint32_t a =
                 session.group_offsets[item.suffixes[i].mod_id];
             a < session.group_offsets[item.suffixes[i].mod_id + 1]; ++a) {
            for (std::uint32_t b = session.group_offsets[mod_id];
                 b < session.group_offsets[mod_id + 1]; ++b) {
                if (session.group_ids[a] == session.group_ids[b]) {
                    return false;
                }
            }
        }
    }
    return pc_item_add_mod(
               &item, side == 0 ? PC_SIDE_PREFIX : PC_SIDE_SUFFIX, mod_id,
               static_cast<std::uint16_t>(session.primary_group[mod_id]), 0,
               nullptr) == PC_RESULT_OK;
}

enum class BucketKind : std::uint8_t { GoalSat = 0, GoalBelow = 1, Junk = 2 };

struct RollBucket {
    std::int8_t side = 0;
    BucketKind kind = BucketKind::Junk;
    std::uint8_t slot = 0;              /* goal buckets */
    std::uint32_t junk_class = kNoId;   /* junk buckets; kNoId = unclassified */
    std::uint32_t block_mask = 0;       /* goal groups a junk pick occupies */
    std::uint64_t weight = 0;           /* per family, normal fill pool */
    /* Per family weight in the guaranteed first-pick pool (harvest
     * reforge: spawn-only weights restricted to the target tag). */
    std::uint64_t guaranteed = 0;
    std::uint32_t multiplicity = 1;
    /* Complete generation-group signature. Any overlap with a previously
     * picked bucket makes this family unavailable, matching pool blocking. */
    std::vector<std::uint32_t> exclusion_groups;
};

struct RollState {
    std::uint8_t sat_mask = 0;
    std::uint8_t below_mask = 0;
    std::uint8_t blocked_mask = 0;
    std::uint8_t prefix_picks = 0;
    std::uint8_t suffix_picks = 0;
    std::uint8_t pick_count = 0;
    /* sorted (bucket index, count), at most six picks total */
    std::array<std::pair<std::uint16_t, std::uint8_t>, 6> picks{};

    bool operator==(const RollState& other) const = default;

    bool operator<(const RollState& other) const {
        return std::tie(
                   sat_mask, below_mask, blocked_mask, prefix_picks,
                   suffix_picks, pick_count, picks) <
               std::tie(
                   other.sat_mask, other.below_mask, other.blocked_mask,
                   other.prefix_picks, other.suffix_picks,
                   other.pick_count, other.picks);
    }

    std::uint8_t picks_of(std::uint16_t bucket) const {
        for (std::uint8_t i = 0; i < pick_count; ++i) {
            if (picks[i].first == bucket) return picks[i].second;
        }
        return 0;
    }

    void add_pick(std::uint16_t bucket) {
        for (std::uint8_t i = 0; i < pick_count; ++i) {
            if (picks[i].first == bucket) {
                ++picks[i].second;
                return;
            }
        }
        picks[pick_count++] = {bucket, 1};
        std::sort(picks.begin(), picks.begin() + pick_count);
    }
};

struct RollStateHash {
    std::size_t operator()(const RollState& state) const {
        std::uint32_t hash = 2166136261u;
        const auto mix = [&hash](std::uint32_t value) {
            hash ^= value;
            hash *= 16777619u;
        };
        mix(state.sat_mask);
        mix(state.below_mask);
        mix(state.blocked_mask);
        mix(state.prefix_picks);
        mix(state.suffix_picks);
        for (std::uint8_t i = 0; i < state.pick_count; ++i) {
            mix((static_cast<std::uint64_t>(state.picks[i].first) << 8) |
                state.picks[i].second);
        }
        return hash;
    }
};

/* Goal-slot occupancy: satisfied, below-tier, or blocked all mean the
 * slot's exclusivity group is taken for the rest of the roll. */
std::uint8_t occupied_mask(const RollState& state, std::uint8_t base_mask) {
    return static_cast<std::uint8_t>(
        base_mask | state.sat_mask | state.below_mask | state.blocked_mask);
}

} // namespace

std::shared_ptr<const OutcomeDistribution> CalcContext::evaluate_reforge(
    std::uint32_t state_id,
    std::uint32_t action_index) {
    ++telemetry_.reforge_requests;
    ReforgeBuildTimer telemetry_timer{telemetry_};
    const ActionDescriptor& action = registry_.actions.at(action_index);
    const SessionImpl& session = *session_;
    const DataImpl& data = *session.data;
    OutcomeDistribution result;

    pc_item_state item;
    if (!materialize(state_id, item)) {
        ++telemetry_.reforge_misses;
        telemetry_timer.miss = true;
        return std::make_shared<OutcomeDistribution>(std::move(result));
    }

    /* --- preserved base: fractured slots and locked sides ----------------- */
    const bool magic_reforge =
        action.params.type == ActionType::Transmute ||
        action.params.type == ActionType::Alteration;
    const bool veiled_reforge =
        action.params.type == ActionType::VeiledChaos;
    const bool eldritch_reforge =
        action.params.type == ActionType::EldritchChaos;
    const int eldritch_side =
        item.searing_exarch_tier > item.eater_of_worlds_tier
            ? PC_SIDE_PREFIX
            : (item.eater_of_worlds_tier > item.searing_exarch_tier
                   ? PC_SIDE_SUFFIX
                   : -1);
    pc_item_state base;
    pc_item_clear(&base);
    base.rarity = magic_reforge ? PC_RARITY_MAGIC : PC_RARITY_RARE;
    base.item_flags = item.item_flags;
    base.generic_influence_bits = item.generic_influence_bits;
    base.searing_exarch_tier = item.searing_exarch_tier;
    base.eater_of_worlds_tier = item.eater_of_worlds_tier;
    const bool prefix_locked =
        reforge_side_locked(session, item, PC_SIDE_PREFIX);
    const bool suffix_locked =
        reforge_side_locked(session, item, PC_SIDE_SUFFIX);
    const auto preserve = [&](int side, const pc_mod_slot* slots,
                              std::uint8_t count, bool locked) {
        for (std::uint8_t i = 0; i < count; ++i) {
            const bool preserve_eldritch =
                eldritch_reforge && eldritch_side >= 0 &&
                side != eldritch_side;
            const bool clear_eldritch =
                eldritch_reforge && eldritch_side >= 0 &&
                side == eldritch_side;
            if (preserve_eldritch ||
                (!clear_eldritch && locked) ||
                (slots[i].flags & PC_MOD_SLOT_FRACTURED)) {
                pc_mod_slot* restored = nullptr;
                pc_item_add_mod(&base, side, slots[i].mod_id,
                                slots[i].group_id, slots[i].flags,
                                &restored);
                if (restored != nullptr) *restored = slots[i];
            }
        }
    };
    preserve(PC_SIDE_PREFIX, item.prefixes, item.prefix_count, prefix_locked);
    preserve(PC_SIDE_SUFFIX, item.suffixes, item.suffix_count, suffix_locked);

    /* A reforge's distribution depends only on the preserved base, so
     * states differing only in wiped mods share one roll DP. */
    std::uint64_t base_hash = 1469598103934665603ull;
    {
        const auto mix = [&base_hash](std::uint64_t value) {
            base_hash ^= value;
            base_hash *= 1099511628211ull;
        };
        mix(base.rarity);
        mix(base.item_flags);
        mix(base.generic_influence_bits);
        mix(base.searing_exarch_tier);
        mix(base.eater_of_worlds_tier);
        const auto mix_slots = [&](const pc_mod_slot* slots,
                                   std::uint8_t count) {
            for (std::uint8_t i = 0; i < count; ++i) {
                mix((static_cast<std::uint64_t>(slots[i].mod_id) << 8) |
                    slots[i].flags);
            }
        };
        mix_slots(base.prefixes, base.prefix_count);
        mix_slots(base.suffixes, base.suffix_count);
    }
    const std::pair<std::uint32_t, std::uint64_t> memo_key{action_index,
                                                           base_hash};
    const auto memo = reforge_cache_.find(memo_key);
    if (memo != reforge_cache_.end()) {
        ++telemetry_.reforge_hits;
        return memo->second;
    }
    ++telemetry_.reforge_misses;
    telemetry_timer.miss = true;

    std::unordered_map<std::uint32_t, double> outcome_acc;
    outcome_acc.reserve(65536);
    /* Self-loop results reference the querying state and must not be
     * shared through the base memo. */
    bool state_dependent = false;
    const auto finalize = [&]() -> std::shared_ptr<const OutcomeDistribution> {
        std::vector<std::pair<std::uint32_t, double>> ordered_outcomes(
            outcome_acc.begin(), outcome_acc.end());
        std::sort(ordered_outcomes.begin(), ordered_outcomes.end());
        double committed = 0.0;
        for (const auto& [successor, probability] : ordered_outcomes) {
            (void)successor;
            committed += probability;
        }
        if (committed <= 0.0) {
            outcome_acc.clear();
            outcome_acc[state_id] = 1.0;
            ordered_outcomes.assign({{state_id, 1.0}});
            committed = 1.0;
            state_dependent = true;
        }
        for (const auto& [successor, probability] : ordered_outcomes) {
            result.entries.push_back({successor, probability / committed});
        }
        for (const OutcomeEntry& entry : result.entries) {
            const AbstractState& successor = states_.at(entry.state);
            for (std::size_t i = 0; i < layout_.slots.size(); ++i) {
                if (successor.slot_status[i] ==
                    static_cast<std::uint8_t>(GoalSlotStatus::Satisfied)) {
                    result.slot_satisfied_probability[i] +=
                        entry.probability;
                }
            }
        }
        result.supported = true;
        return std::make_shared<OutcomeDistribution>(std::move(result));
    };
    const auto unapplied = [&]() -> std::shared_ptr<const OutcomeDistribution> {
        outcome_acc.clear();
        outcome_acc[state_id] = 1.0;
        state_dependent = true;
        return finalize();
    };

    /* --- direct mods (essence guaranteed, fossil forced) ------------------ */
    std::vector<std::uint32_t> directs;
    if (action.params.type == ActionType::Essence) {
        if (action.params.essence_index >=
                session.essence_guaranteed_mod_ids.size() ||
            session.essence_guaranteed_mod_ids[action.params.essence_index] ==
                kNoId) {
            return unapplied();
        }
        directs.push_back(
            session.essence_guaranteed_mod_ids[action.params.essence_index]);
    } else if (action.params.type == ActionType::Fossil) {
        for (std::uint32_t fossil : action.params.fossil_indices) {
            if (fossil >= session.fossil_forced_mod_ids.size()) {
                return unapplied();
            }
            directs.insert(directs.end(),
                           session.fossil_forced_mod_ids[fossil].begin(),
                           session.fossil_forced_mod_ids[fossil].end());
        }
        std::sort(directs.begin(), directs.end());
        directs.erase(std::unique(directs.begin(), directs.end()),
                      directs.end());
    }
    for (std::uint32_t direct : directs) {
        if (!direct_add(session, base, direct)) {
            return unapplied();
        }
    }

    /* --- fossil special flag effects -------------------------------------- */
    std::uint8_t special_flags = 0;
    if (action.params.type == ActionType::Fossil) {
        for (std::uint32_t fossil : action.params.fossil_indices) {
            const std::string& name =
                data.string_at(data.fossil_name_sids[fossil]);
            if (name == "Bloodstained Fossil") {
                bool has_candidate = false;
                for (std::uint64_t word : session.corrupted_implicit_mask) {
                    if (word != 0) has_candidate = true;
                }
                if (has_candidate) special_flags |= PC_ITEM_CORRUPTED;
            }
            if (fossil < data.fossil_mirrors.size() &&
                data.fossil_mirrors[fossil]) {
                special_flags |= PC_ITEM_MIRRORED;
            }
        }
    }

    /* --- roll pool and buckets --------------------------------------------- */
    PoolBuildRequest request;
    if (action.params.type == ActionType::Fossil) {
        request.weight_kind = PoolWeightKind::Fossil;
        request.fossil_indices = action.params.fossil_indices;
    }
    if (eldritch_reforge && eldritch_side >= 0) {
        request.side_filter = eldritch_side;
    }
    const WeightedPool& pool = get_weighted_pool(context_, &base, request);

    /* Aggregate pool weights per complete (side, exclusion-group family)
     * with goal classification. Harvest reforge overlays a
     * second weight per family: the guaranteed first pick's spawn-only
     * tag pool. A family can be guaranteed-only (positive spawn weight,
     * zero normal fill weight). */
    const bool harvest = action.params.type == ActionType::HarvestReforge;
    struct WeightPair {
        std::uint64_t normal = 0;
        std::uint64_t guaranteed = 0;
    };
    struct FamilyAgg {
        std::array<WeightPair, kMaxGoalSlots> sat{};
        std::array<WeightPair, kMaxGoalSlots> below{};
        std::map<std::pair<std::uint32_t, std::uint32_t>, WeightPair>
            junk; /* (class, block mask) -> weights */
    };
    using FamilyKey =
        std::pair<std::int8_t, std::vector<std::uint32_t>>;
    std::map<FamilyKey, FamilyAgg> families;
    std::vector<std::uint32_t> scratch_groups;
    const auto classify = [&](const PoolEntry& entry, bool guaranteed) {
        const std::uint32_t mod = entry.session_mod_id;
        std::vector<std::uint32_t> exclusion_groups;
        exclusion_groups.reserve(
            session.group_offsets[mod + 1] - session.group_offsets[mod]);
        for (std::uint32_t group = session.group_offsets[mod];
             group < session.group_offsets[mod + 1]; ++group) {
            exclusion_groups.push_back(session.group_ids[group]);
        }
        std::sort(exclusion_groups.begin(), exclusion_groups.end());
        exclusion_groups.erase(
            std::unique(
                exclusion_groups.begin(), exclusion_groups.end()),
            exclusion_groups.end());
        FamilyAgg& family =
            families[{entry.gen_type, std::move(exclusion_groups)}];
        const auto credit = [&](WeightPair& pair) {
            if (guaranteed) {
                pair.guaranteed += entry.final_weight;
            } else {
                pair.normal += entry.final_weight;
            }
        };
        for (std::size_t s = 0; s < layout_.slots.size(); ++s) {
            if (pc_bitset_test(layout_.slots[s].satisfying_mask.data(),
                               mod)) {
                credit(family.sat[s]);
                return;
            }
            if (pc_bitset_test(layout_.slots[s].member_mask.data(), mod)) {
                credit(family.below[s]);
                return;
            }
        }
        const std::uint32_t junk_class = layout_.junk_class_by_mod[mod];
        std::uint32_t block_mask = 0;
        if (junk_class != kNoId) {
            block_mask = layout_.junk_classes[junk_class].goal_block_mask;
        } else {
            scratch_groups.clear();
            for (std::uint32_t i = session.group_offsets[mod];
                 i < session.group_offsets[mod + 1]; ++i) {
                scratch_groups.push_back(session.group_ids[i]);
            }
            for (std::size_t s = 0; s < layout_.slots.size(); ++s) {
                for (std::uint32_t group : scratch_groups) {
                    if (std::binary_search(
                            layout_.slots[s].blocking_group_ids.begin(),
                            layout_.slots[s].blocking_group_ids.end(),
                            group)) {
                        block_mask |= 1u << s;
                        break;
                    }
                }
            }
        }
        credit(family.junk[{junk_class, block_mask}]);
    };
    for (const PoolEntry& entry : pool.entries) {
        if (entry.final_weight == 0) continue;
        classify(entry, false);
    }
    if (harvest) {
        PoolBuildRequest guaranteed_request;
        guaranteed_request.weight_kind = PoolWeightKind::HarvestSpawnOnly;
        guaranteed_request.target_tag_id = action.params.target_tag_id;
        const WeightedPool& guaranteed_pool =
            get_weighted_pool(context_, &base, guaranteed_request);
        for (const PoolEntry& entry : guaranteed_pool.entries) {
            if (entry.final_weight == 0) continue;
            classify(entry, true);
        }
    }

    std::vector<RollBucket> buckets;
    for (const auto& [key, family] : families) {
        for (std::size_t s = 0; s < layout_.slots.size(); ++s) {
            if (family.sat[s].normal > 0 ||
                family.sat[s].guaranteed > 0) {
                RollBucket bucket;
                bucket.side = key.first;
                bucket.kind = BucketKind::GoalSat;
                bucket.slot = static_cast<std::uint8_t>(s);
                bucket.weight = family.sat[s].normal;
                bucket.guaranteed = family.sat[s].guaranteed;
                bucket.exclusion_groups = key.second;
                buckets.push_back(std::move(bucket));
            }
            if (family.below[s].normal > 0 ||
                family.below[s].guaranteed > 0) {
                RollBucket bucket;
                bucket.side = key.first;
                bucket.kind = BucketKind::GoalBelow;
                bucket.slot = static_cast<std::uint8_t>(s);
                bucket.weight = family.below[s].normal;
                bucket.guaranteed = family.below[s].guaranteed;
                bucket.exclusion_groups = key.second;
                buckets.push_back(std::move(bucket));
            }
        }
        for (const auto& [junk_key, weights] : family.junk) {
            RollBucket bucket;
            bucket.side = key.first;
            bucket.kind = BucketKind::Junk;
            bucket.junk_class = junk_key.first;
            bucket.block_mask = junk_key.second;
            bucket.weight = weights.normal;
            bucket.guaranteed = weights.guaranteed;
            bucket.exclusion_groups = key.second;
            buckets.push_back(std::move(bucket));
        }
    }
    /* Group exclusion is immutable for this roll. Precompute the exact
     * pairwise relation once instead of repeatedly intersecting sorted group
     * vectors in every frontier state and candidate-bucket visit. */
    const std::size_t bucket_count = buckets.size();
    std::vector<std::uint8_t> buckets_conflict(
        bucket_count * bucket_count, 0);
    for (std::size_t left = 0; left < bucket_count; ++left) {
        for (std::size_t right = left; right < bucket_count; ++right) {
            const auto& a = buckets[left].exclusion_groups;
            const auto& b = buckets[right].exclusion_groups;
            std::size_t ai = 0;
            std::size_t bi = 0;
            bool conflict = false;
            while (ai < a.size() && bi < b.size()) {
                if (a[ai] == b[bi]) {
                    conflict = true;
                    break;
                }
                if (a[ai] < b[bi]) {
                    ++ai;
                } else {
                    ++bi;
                }
            }
            buckets_conflict[left * bucket_count + right] = conflict;
            buckets_conflict[right * bucket_count + left] = conflict;
        }
    }

    /* --- base abstract features -------------------------------------------- */
    const AbstractState base_state = project_item(session, layout_, base);
    std::uint8_t base_occupied = 0;
    for (std::size_t s = 0; s < layout_.slots.size(); ++s) {
        if (base_state.slot_status[s] !=
                static_cast<std::uint8_t>(GoalSlotStatus::Absent) ||
            (base_state.blocked_mask & (1u << s)) != 0) {
            base_occupied |= 1u << s;
        }
    }
    const std::uint8_t cap = rarity_cap(session, base.rarity);
    const int start_total = base.prefix_count + base.suffix_count;

    /* --- target-count distribution -----------------------------------------
     * Clamped from both sides: side caps bound the top, and the already-
     * present total (preserved slots, plus the harvest guaranteed pick)
     * bounds the bottom — the engine's fill loop simply does nothing when
     * the target is already met. */
    const int first_depth = start_total + (harvest ? 1 : 0);
    std::map<int, double> targets;
    const auto add_target = [&](int target, double probability) {
        targets[std::max(std::min<int>(target, cap * 2), first_depth)] +=
            probability;
    };
    if (magic_reforge) {
        add_target(1, 0.5);
        add_target(2, 0.5);
    } else if (eldritch_reforge && eldritch_side >= 0) {
        const int other_count =
            eldritch_side == PC_SIDE_PREFIX ? base.suffix_count
                                            : base.prefix_count;
        add_target(other_count + 2, 0.5);
        add_target(other_count + 3, 0.5);
    } else {
        for (int t = 4; t <= 6; ++t) {
            add_target(t - (veiled_reforge ? 1 : 0), 1.0 / 3.0);
        }
    }
    const int max_target = targets.rbegin()->first;

    /* --- forward frontier DP ------------------------------------------------ */
    const auto commit_outcome = [&](const RollState& roll, double weight) {
        AbstractState successor = base_state;
        successor.blocked_mask |= roll.blocked_mask;
        for (std::size_t s = 0; s < layout_.slots.size(); ++s) {
            const std::uint8_t bit = 1u << s;
            if (roll.sat_mask & bit) {
                successor.slot_status[s] = static_cast<std::uint8_t>(
                    GoalSlotStatus::Satisfied);
            } else if ((roll.below_mask & bit) &&
                       successor.slot_status[s] !=
                           static_cast<std::uint8_t>(
                               GoalSlotStatus::Satisfied)) {
                successor.slot_status[s] = static_cast<std::uint8_t>(
                    GoalSlotStatus::PresentBelowTier);
            }
        }
        successor.prefix_count =
            static_cast<std::uint8_t>(base.prefix_count + roll.prefix_picks);
        successor.suffix_count =
            static_cast<std::uint8_t>(base.suffix_count + roll.suffix_picks);
        for (std::uint8_t i = 0; i < roll.pick_count; ++i) {
            const RollBucket& bucket = buckets[roll.picks[i].first];
            if (bucket.kind == BucketKind::Junk &&
                bucket.junk_class != kNoId) {
                successor.junk_counts[bucket.junk_class] = static_cast<
                    std::uint8_t>(
                    successor.junk_counts[bucket.junk_class] +
                    roll.picks[i].second);
            }
        }
        if (special_flags & PC_ITEM_CORRUPTED) {
            successor.flags |= kFlagCorrupted;
        }
        if (special_flags & PC_ITEM_MIRRORED) {
            successor.flags |= kFlagMirrored;
        }
        if (!veiled_reforge) {
            outcome_acc[intern_state(successor)] += weight;
            return;
        }

        /* Veiled chaos reserves its final affix for a uniformly selected
         * open side. Materialize the roll state before adding the placeholder
         * so goal/block/junk projection remains identical to the engine. */
        const std::uint8_t side_cap = rarity_cap(session, successor.rarity);
        const bool prefix_open = successor.prefix_count < side_cap;
        const bool suffix_open = successor.suffix_count < side_cap;
        const double side_probability =
            prefix_open && suffix_open ? 0.5 : 1.0;
        const auto add_veiled = [&](int side, bool open) {
            if (!open) return;
            const std::uint32_t mod_id =
                side == PC_SIDE_PREFIX ? session.veiled_prefix_mod_id
                                       : session.veiled_suffix_mod_id;
            pc_item_state concrete;
            const std::uint32_t rolled_state = intern_state(successor);
            if (mod_id == kNoId || !materialize(rolled_state, concrete) ||
                pc_item_add_mod(
                    &concrete, side, mod_id,
                    static_cast<std::uint16_t>(
                        session.primary_group[mod_id]),
                    PC_MOD_SLOT_VEILED, nullptr) != PC_RESULT_OK) {
                outcome_acc[state_id] += weight * side_probability;
                state_dependent = true;
                return;
            }
            outcome_acc[intern_item(concrete)] +=
                weight * side_probability;
        };
        add_veiled(PC_SIDE_PREFIX, prefix_open);
        add_veiled(PC_SIDE_SUFFIX, suffix_open);
        if (!prefix_open && !suffix_open) {
            outcome_acc[state_id] += weight;
            state_dependent = true;
        }
    };

    /*
     * The frontier evolves as the pure roll process (probabilities
     * unconditioned on the target draw). At each depth the mass of
     * targets that stop exactly there is committed; a state with no
     * eligible pick absorbs the mass of every deeper target, mirroring
     * the engine's fill loop breaking early.
     */
    const auto target_suffix = [&](int depth) {
        double sum = 0.0;
        for (const auto& [t, p] : targets) {
            if (t > depth) sum += p;
        }
        return sum;
    };
    const auto bucket_remaining =
        [&](const RollState& roll, std::uint16_t b,
            std::uint8_t occupied) -> double {
        const RollBucket& bucket = buckets[b];
        const std::uint8_t side_count =
            bucket.side == 0
                ? static_cast<std::uint8_t>(base.prefix_count +
                                            roll.prefix_picks)
                : static_cast<std::uint8_t>(base.suffix_count +
                                            roll.suffix_picks);
        if (side_count >= cap) return 0.0;
        if (bucket.kind != BucketKind::Junk) {
            if (occupied & (1u << bucket.slot)) return 0.0;
        } else {
            if (bucket.block_mask & occupied) return 0.0;
        }
        for (std::uint8_t pick = 0; pick < roll.pick_count; ++pick) {
            if (buckets_conflict[
                    static_cast<std::size_t>(b) * bucket_count +
                    roll.picks[pick].first]) {
                return 0.0;
            }
        }
        const std::uint32_t used = roll.picks_of(b);
        if (used >= bucket.multiplicity) return 0.0;
        return static_cast<double>(bucket.weight) *
               (bucket.multiplicity - used);
    };

    std::unordered_map<RollState, double, RollStateHash> frontier;
    std::vector<std::pair<RollState, double>> ordered_frontier;
    std::vector<double> remaining_by_bucket(bucket_count, 0.0);
    if (!harvest) {
        frontier.emplace(RollState{}, 1.0);
    } else {
        /* Guaranteed first pick from the tag-targeted spawn-only pool.
         * An empty pool means the engine action does not apply. */
        const RollState root;
        const std::uint8_t occupied = occupied_mask(root, base_occupied);
        const auto guaranteed_remaining = [&](std::uint16_t b) -> double {
            const RollBucket& bucket = buckets[b];
            if (bucket.guaranteed == 0) return 0.0;
            const std::uint8_t side_count =
                bucket.side == 0 ? base.prefix_count : base.suffix_count;
            if (side_count >= cap) return 0.0;
            if (bucket.kind != BucketKind::Junk) {
                if (occupied & (1u << bucket.slot)) return 0.0;
                return static_cast<double>(bucket.guaranteed);
            }
            if (bucket.block_mask & occupied) return 0.0;
            return static_cast<double>(bucket.guaranteed) *
                   bucket.multiplicity;
        };
        double total = 0.0;
        consume_reforge_work(buckets.size());
        for (std::uint16_t b = 0;
             b < static_cast<std::uint16_t>(buckets.size()); ++b) {
            total += guaranteed_remaining(b);
        }
        if (total <= 0.0) return unapplied();
        for (std::uint16_t b = 0;
             b < static_cast<std::uint16_t>(buckets.size()); ++b) {
            const double remaining = guaranteed_remaining(b);
            if (remaining <= 0.0) continue;
            const RollBucket& bucket = buckets[b];
            RollState child = root;
            if (bucket.side == 0) {
                ++child.prefix_picks;
            } else {
                ++child.suffix_picks;
            }
            child.add_pick(b);
            if (bucket.kind == BucketKind::GoalSat) {
                child.sat_mask |= 1u << bucket.slot;
            } else if (bucket.kind == BucketKind::GoalBelow) {
                child.below_mask |= 1u << bucket.slot;
            } else {
                child.blocked_mask |= bucket.block_mask;
            }
            frontier[child] += remaining / total;
        }
    }
    for (int depth = first_depth; depth <= max_target; ++depth) {
        const auto exact = targets.find(depth);
        const double stop_here =
            exact != targets.end() ? exact->second : 0.0;
        const double deeper = target_suffix(depth);
        std::unordered_map<RollState, double, RollStateHash> next;
        next.reserve(frontier.size() * 2);
        ordered_frontier.assign(frontier.begin(), frontier.end());
        std::sort(
            ordered_frontier.begin(), ordered_frontier.end(),
            [](const auto& left, const auto& right) {
                return left.first < right.first;
            });
        for (const auto& [roll, probability] : ordered_frontier) {
            consume_reforge_work(1 + buckets.size());
            if (stop_here > 0.0) {
                commit_outcome(roll, probability * stop_here);
            }
            if (deeper <= 0.0) continue;
            const std::uint8_t occupied = occupied_mask(roll, base_occupied);
            double total = 0.0;
            for (std::uint16_t b = 0;
                 b < static_cast<std::uint16_t>(buckets.size()); ++b) {
                remaining_by_bucket[b] =
                    bucket_remaining(roll, b, occupied);
                total += remaining_by_bucket[b];
            }
            if (total <= 0.0) {
                commit_outcome(roll, probability * deeper);
                continue;
            }
            for (std::uint16_t b = 0;
                 b < static_cast<std::uint16_t>(buckets.size()); ++b) {
                const double remaining = remaining_by_bucket[b];
                if (remaining <= 0.0) continue;
                const double p = probability * (remaining / total);
                const RollBucket& bucket = buckets[b];
                RollState child = roll;
                if (bucket.side == 0) {
                    ++child.prefix_picks;
                } else {
                    ++child.suffix_picks;
                }
                child.add_pick(b);
                if (bucket.kind == BucketKind::GoalSat) {
                    child.sat_mask |= 1u << bucket.slot;
                } else if (bucket.kind == BucketKind::GoalBelow) {
                    child.below_mask |= 1u << bucket.slot;
                } else {
                    child.blocked_mask |= bucket.block_mask;
                }
                next[child] += p;
            }
        }
        frontier = std::move(next);
    }

    result.stable_shared_kernel = !state_dependent;
    std::shared_ptr<const OutcomeDistribution> finalized = finalize();
    if (!state_dependent) {
        reforge_cache_.emplace(memo_key, finalized);
    }
    return finalized;
}

} // namespace solver
} // namespace poecraft
