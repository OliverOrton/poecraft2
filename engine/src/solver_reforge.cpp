#include "solver_calc_types.hpp"
#include "solver_solve_types.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <set>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "poecraft/bitset.h"
#include "poecraft/item_state.h"

/*
 * Solver S3: exact reforge evaluator (docs/solver/crafting-solver-plan.md,
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

std::uint64_t saturated_add(
    const std::uint64_t left,
    const std::uint64_t right) {
    return right > std::numeric_limits<std::uint64_t>::max() - left
               ? std::numeric_limits<std::uint64_t>::max()
               : left + right;
}

std::uint64_t saturated_bytes(
    const std::size_t count,
    const std::uint64_t element_size) {
    if (element_size != 0 &&
        count > std::numeric_limits<std::uint64_t>::max() /
                    element_size) {
        return std::numeric_limits<std::uint64_t>::max();
    }
    return static_cast<std::uint64_t>(count) * element_size;
}

std::uint64_t unordered_storage_bytes(
    const std::size_t entries,
    const std::size_t value_size) {
    /*
     * Selected-allocation upper estimate: one value plus three pointers of
     * node/link/hash-table overhead, and up to two bucket pointers per entry.
     * The same deliberately conservative convention is used by CalcContext's
     * retained-owned-byte ledger.
     */
    const std::uint64_t node_size = saturated_add(
        value_size, 3 * sizeof(void*));
    return saturated_add(
        saturated_bytes(entries, node_size),
        saturated_bytes(entries, 2 * sizeof(void*)));
}

std::uint64_t ordered_storage_bytes(
    const std::size_t entries,
    const std::size_t value_size) {
    return saturated_bytes(
        entries, saturated_add(value_size, 3 * sizeof(void*)));
}

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

struct ReforgeAttributionRecorder {
    CalcTelemetry& telemetry;
    bool enabled = false;
    ReforgeBuildAttribution row;
    std::uint64_t work_started = 0;
    std::uint64_t raw_equivalent_work_started = 0;
    std::uint64_t projected_work_started = 0;
    std::uint64_t factored_work_started = 0;
    std::chrono::steady_clock::time_point started =
        std::chrono::steady_clock::now();

    ~ReforgeAttributionRecorder() noexcept {
        if (!enabled) return;
        row.total_reforge_work =
            telemetry.reforge_frontier_work >= work_started
                ? telemetry.reforge_frontier_work - work_started
                : 0;
        row.raw_equivalent_reforge_work =
            telemetry.reforge_raw_equivalent_work >=
                    raw_equivalent_work_started
                ? telemetry.reforge_raw_equivalent_work -
                      raw_equivalent_work_started
                : 0;
        row.projected_reforge_work =
            telemetry.reforge_projected_work >= projected_work_started
                ? telemetry.reforge_projected_work -
                      projected_work_started
                : 0;
        row.factored_reforge_work =
            telemetry.reforge_factored_work >= factored_work_started
                ? telemetry.reforge_factored_work - factored_work_started
                : 0;
        row.total_build_ns = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - started)
                .count());
        try {
            constexpr std::size_t kSampleLimit = 64;
            if (telemetry.reforge_build_attribution_samples.size() <
                kSampleLimit) {
                telemetry.reforge_build_attribution_samples.push_back(
                    std::move(row));
            } else {
                ++telemetry.reforge_build_attribution_omitted;
            }
        } catch (...) {
            ++telemetry.reforge_build_attribution_omitted;
        }
    }
};

struct ReforgeEffortRecorder {
    CalcTelemetry& telemetry;
    ReforgeRowTelemetry row;
    std::uint64_t active_work_started = 0;
    std::uint64_t v1_work_started = 0;
    std::uint64_t v2_work_started = 0;
    std::uint64_t v3_work_started = 0;
    bool completed = false;

    ReforgeEffortRecorder(
        CalcTelemetry& telemetry_in,
        const std::uint32_t action_index,
        const ReforgeRowOwner owner,
        const ReforgeRowFamily family,
        const ReforgeEvaluatorVersion evaluator)
        : telemetry(telemetry_in) {
        row.sequence = telemetry.reforge_row_sequence;
        telemetry.reforge_row_sequence = saturated_add(
            telemetry.reforge_row_sequence, 1);
        row.action_index = action_index;
        row.owner = owner;
        row.family = family;
        row.evaluator = evaluator;
        row.components.rows_begun = 1;
        active_work_started = telemetry.reforge_frontier_work;
        v1_work_started = telemetry.reforge_raw_equivalent_work;
        v2_work_started = telemetry.reforge_projected_work;
        v3_work_started = telemetry.reforge_factored_work;
    }

    ~ReforgeEffortRecorder() noexcept {
        if (completed) {
            row.components.rows_completed = 1;
            row.disposition = ReforgeRowDisposition::Completed;
        } else {
            row.components.rows_interrupted = 1;
            row.disposition = ReforgeRowDisposition::Interrupted;
        }
        row.legacy_active_work =
            telemetry.reforge_frontier_work >= active_work_started
                ? telemetry.reforge_frontier_work - active_work_started
                : 0;
        row.logical_work_v1 =
            telemetry.reforge_raw_equivalent_work >= v1_work_started
                ? telemetry.reforge_raw_equivalent_work - v1_work_started
                : 0;
        row.evaluator_work_v1 = row.logical_work_v1;
        row.evaluator_work_v2 =
            telemetry.reforge_projected_work >= v2_work_started
                ? telemetry.reforge_projected_work - v2_work_started
                : 0;
        row.evaluator_work_v3 =
            telemetry.reforge_factored_work >= v3_work_started
                ? telemetry.reforge_factored_work - v3_work_started
                : 0;
        merge_reforge_effort(telemetry.reforge_effort, row.components);
        try {
            constexpr std::size_t kSampleLimit = 64;
            if (telemetry.reforge_row_samples.size() < kSampleLimit) {
                telemetry.reforge_row_samples.push_back(std::move(row));
            } else {
                telemetry.reforge_row_samples_omitted = saturated_add(
                    telemetry.reforge_row_samples_omitted, 1);
            }
        } catch (...) {
            telemetry.reforge_row_samples_omitted = saturated_add(
                telemetry.reforge_row_samples_omitted, 1);
        }
    }
};

ReforgeRowFamily reforge_row_family(const ActionType type) {
    switch (type) {
    case ActionType::Essence: return ReforgeRowFamily::Essence;
    case ActionType::HarvestReforge: return ReforgeRowFamily::Harvest;
    case ActionType::Fossil: return ReforgeRowFamily::Fossil;
    default: return ReforgeRowFamily::Ordinary;
    }
}

std::uint64_t elapsed_ns(
        const std::chrono::steady_clock::time_point started) {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - started)
            .count());
}

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

pc_item_state preserved_reforge_base(
    const SessionImpl& session,
    const ActionDescriptor& action,
    const pc_item_state& item) {
    const bool magic_reforge =
        action.params.type == ActionType::Transmute ||
        action.params.type == ActionType::Alteration;
    const bool eldritch_reforge =
        action.params.type == ActionType::EldritchChaos;
    const ActionTransitionFacts transition_facts =
        action_transition_facts(action.params.type);
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
        transition_facts.respects_metamod_side_locks &&
        reforge_side_locked(session, item, PC_SIDE_PREFIX);
    const bool suffix_locked =
        transition_facts.respects_metamod_side_locks &&
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
    return base;
}

std::vector<std::uint64_t> reforge_base_observation(
    const pc_item_state& base,
    std::uint64_t* out_hash = nullptr) {
    std::vector<std::uint64_t> observation;
    std::uint64_t hash = 1469598103934665603ull;
    const auto mix = [&hash, &observation](std::uint64_t value) {
        observation.push_back(value);
        hash ^= value;
        hash *= 1099511628211ull;
    };
    mix(base.rarity);
    mix(base.item_flags);
    mix(base.generic_influence_bits);
    mix(base.searing_exarch_tier);
    mix(base.eater_of_worlds_tier);
    const auto mix_slots = [&](const std::uint64_t side,
                               const pc_mod_slot* slots,
                               std::uint8_t count) {
        mix(side);
        mix(count);
        for (std::uint8_t i = 0; i < count; ++i) {
            mix(slots[i].mod_id);
            mix(slots[i].group_id);
            mix(slots[i].flags);
        }
    };
    mix_slots(0, base.prefixes, base.prefix_count);
    mix_slots(1, base.suffixes, base.suffix_count);
    if (out_hash != nullptr) *out_hash = hash;
    return observation;
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
    struct RawChoice {
        std::uint32_t mod_id = kNoId;
        std::uint32_t goal_member_class_token = 0;
        std::uint32_t junk_class = kNoId;
        std::uint64_t weight = 0;
        std::uint64_t guaranteed = 0;
    };

    std::int8_t side = 0;
    BucketKind kind = BucketKind::Junk;
    std::uint8_t slot = 0;              /* goal buckets */
    std::uint32_t goal_member_class_token = 0; /* exact goal buckets */
    std::uint32_t junk_class = kNoId;   /* junk buckets; kNoId = unclassified */
    std::uint32_t block_mask = 0;       /* goal groups a junk pick occupies */
    std::uint64_t weight = 0;           /* per family, normal fill pool */
    /* Per-family weight in the guaranteed first-pick pool (Harvest
     * reforge: ordinary naturally rollable weights restricted to the
     * target tag). */
    std::uint64_t guaranteed = 0;
    std::uint32_t multiplicity = 1;
    /* Product-parent symmetry compression may give one aggregate bucket
     * multiple interchangeable physical families. Ordinary buckets retain
     * the original per-bucket availability and conflict semantics. */
    std::uint32_t family_class = kNoId;
    bool interchangeable_family_class = false;
    /*
     * An identity-witness layout gives every literal modifier its own goal
     * token or junk class. Reforge availability, however, depends on the
     * physical exclusion family rather than the selected tier identity.
     * `raw_choices` delays that literal choice until an absorbing roll state;
     * every expanded successor still carries its singleton identity.
     */
    std::vector<RawChoice> raw_choices;
    /* Complete generation-group signature. Any overlap with a previously
     * picked bucket makes this family unavailable, matching pool blocking. */
    std::vector<std::uint32_t> exclusion_groups;
};

struct RollState {
    std::uint8_t sat_mask = 0;
    std::uint8_t below_mask = 0;
    std::uint8_t blocked_mask = 0;
    std::array<std::uint32_t, kMaxGoalSlots>
        goal_member_class_tokens{};
    std::uint8_t prefix_picks = 0;
    std::uint8_t suffix_picks = 0;
    std::uint8_t pick_count = 0;
    /* Only Harvest Reforge has a distinct guaranteed first-pick weight. */
    std::uint16_t guaranteed_bucket =
        std::numeric_limits<std::uint16_t>::max();
    /* Collision-checked interned exact availability signature. Raw-oracle
     * states leave this unset and derive availability from their pick list. */
    std::uint32_t availability_class = kNoId;
    /* sorted (bucket index, count), at most six picks total */
    std::array<std::pair<std::uint16_t, std::uint8_t>, 6> picks{};

    bool operator==(const RollState& other) const = default;

    bool operator<(const RollState& other) const {
        return std::tie(
                   sat_mask, below_mask, blocked_mask,
                   goal_member_class_tokens, prefix_picks, suffix_picks,
                   guaranteed_bucket, pick_count, picks,
                   availability_class) <
               std::tie(
                   other.sat_mask, other.below_mask, other.blocked_mask,
                   other.goal_member_class_tokens, other.prefix_picks,
                   other.suffix_picks, other.guaranteed_bucket,
                   other.pick_count, other.picks,
                   other.availability_class);
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
        if (pick_count >= picks.size()) {
            throw std::logic_error(
                "reforge roll exceeded the explicit-affix capacity");
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
        if (std::any_of(
                state.goal_member_class_tokens.begin(),
                state.goal_member_class_tokens.end(),
                [](const std::uint32_t token) { return token != 0; })) {
            mix(0x67636c73u); /* preserve coarse frontier hash behavior */
            for (const std::uint32_t token :
                 state.goal_member_class_tokens) {
                mix(token);
            }
        }
        mix(state.prefix_picks);
        mix(state.suffix_picks);
        if (state.guaranteed_bucket !=
            std::numeric_limits<std::uint16_t>::max()) {
            mix(0x67756172u); /* "guar": identity-witness Harvest only. */
            mix(state.guaranteed_bucket);
        }
        if (state.availability_class != kNoId) {
            mix(0x61766169u); /* "avai": projected sparse frontier only. */
            mix(state.availability_class);
        }
        for (std::uint8_t i = 0; i < state.pick_count; ++i) {
            mix((static_cast<std::uint64_t>(state.picks[i].first) << 8) |
                state.picks[i].second);
        }
        return hash;
    }
};

/*
 * Destination-driven V3 terminal recurrence identity. The local bucket ids
 * are scoped by the surrounding action, preserved base, goal layout, pool,
 * and artifact generation. They are stronger than a projected observation:
 * each id owns its exact side, weight channels, raw identities, generation
 * exclusions, and conflict masks. Availability remains collision checked on
 * the retained RollState value before a subset contribution is admitted.
 */
struct TerminalPickKey {
    std::uint16_t guaranteed_bucket =
        std::numeric_limits<std::uint16_t>::max();
    std::uint8_t pick_count = 0;
    std::array<std::pair<std::uint16_t, std::uint8_t>, 6> picks{};

    bool operator==(const TerminalPickKey& other) const = default;
};

struct TerminalPickKeyHash {
    std::size_t operator()(const TerminalPickKey& key) const {
        std::uint32_t hash = 2166136261u;
        const auto mix = [&hash](const std::uint32_t value) {
            hash ^= value;
            hash *= 16777619u;
        };
        mix(key.guaranteed_bucket);
        for (std::uint8_t i = 0; i < key.pick_count; ++i) {
            mix((static_cast<std::uint32_t>(key.picks[i].first) << 8) |
                key.picks[i].second);
        }
        return hash;
    }
};

TerminalPickKey terminal_pick_key(const RollState& roll) {
    TerminalPickKey key;
    key.guaranteed_bucket = roll.guaranteed_bucket;
    key.pick_count = roll.pick_count;
    key.picks = roll.picks;
    return key;
}

bool remove_terminal_pick(
    TerminalPickKey& key,
    const std::uint16_t bucket) {
    for (std::uint8_t i = 0; i < key.pick_count; ++i) {
        if (key.picks[i].first != bucket) continue;
        const std::uint8_t guaranteed =
            key.guaranteed_bucket == bucket ? 1 : 0;
        if (key.picks[i].second <= guaranteed) return false;
        --key.picks[i].second;
        if (key.picks[i].second == 0) {
            for (std::uint8_t j = i + 1; j < key.pick_count; ++j) {
                key.picks[j - 1] = key.picks[j];
            }
            key.picks[--key.pick_count] = {};
        }
        return true;
    }
    return false;
}

void add_terminal_pick(
    TerminalPickKey& key,
    const std::uint16_t bucket) {
    for (std::uint8_t i = 0; i < key.pick_count; ++i) {
        if (key.picks[i].first != bucket) continue;
        ++key.picks[i].second;
        return;
    }
    if (key.pick_count >= key.picks.size()) {
        throw std::logic_error(
            "reforge terminal subset exceeded affix capacity");
    }
    key.picks[key.pick_count++] = {bucket, 1};
    std::sort(key.picks.begin(), key.picks.begin() + key.pick_count);
}

struct FactoredTerminalPredecessor {
    RollState roll;
    double scaled_probability = 0.0;
    std::uint64_t exact_denominator = 0;
};

struct FactoredTerminalCandidate {
    RollState completed;

    bool operator<(const FactoredTerminalCandidate& other) const {
        return completed < other.completed;
    }
};

struct FinalSuccessorAttribution {
    std::uint32_t last_predecessor_sequence = kNoId;
    std::uint16_t first_terminal_bucket = 0;
    std::uint8_t first_predecessor_sat_mask = 0;
    std::uint8_t first_predecessor_below_mask = 0;
    std::uint8_t first_predecessor_blocked_mask = 0;
    std::uint8_t first_predecessor_prefix_picks = 0;
    std::uint8_t first_predecessor_suffix_picks = 0;
    std::uint32_t first_predecessor_availability_class = kNoId;
    std::uint64_t contributions = 0;
    std::uint64_t distinct_predecessors = 0;
    double probability_mass = 0.0;
};

struct ActiveTerminalBranch {
    RollState predecessor;
    std::uint32_t predecessor_sequence = 0;
    std::uint64_t predecessor_signature_hash = 0;
    std::uint64_t predecessor_order_multiplicity = 1;
    std::uint8_t target_count = 0;
    std::uint16_t terminal_bucket = 0;
    std::uint32_t available_family_choices = 0;
};

/* Goal-slot occupancy: satisfied, below-tier, or blocked all mean the
 * slot's exclusivity group is taken for the rest of the roll. */
std::uint8_t occupied_mask(const RollState& state, std::uint8_t base_mask) {
    return static_cast<std::uint8_t>(
        base_mask | state.sat_mask | state.below_mask | state.blocked_mask);
}

} // namespace

bool CalcContext::exact_reforge_kernel_signature(
    const std::uint32_t state_id,
    const std::uint32_t action_index,
    std::vector<std::uint64_t>& out_signature) const {
    out_signature.clear();
    if (action_index >= registry_.actions.size()) return false;
    const ActionDescriptor& action = registry_.actions[action_index];
    if (!action_transition_facts(action.params.type).renewal) return false;
    pc_item_state item;
    if (!materialize(state_id, item)) return false;
    out_signature = reforge_base_observation(
        preserved_reforge_base(*session_, action, item));
    out_signature.insert(out_signature.begin(), action_index);
    return true;
}

std::shared_ptr<const OutcomeDistribution> CalcContext::evaluate_reforge(
    std::uint32_t state_id,
    std::uint32_t action_index,
    const bool goal_progress_gated) {
    ++telemetry_.reforge_requests;
    ReforgeBuildTimer telemetry_timer{telemetry_};
    const ActionDescriptor& action = registry_.actions.at(action_index);
    const ReforgeRowFamily row_family =
        reforge_row_family_override_.value_or(
            reforge_row_family(action.params.type));
    const ReforgeEvaluatorVersion evaluator_version =
        use_factored_terminal_reforge_
            ? ReforgeEvaluatorVersion::V3Factored
            : use_projected_reforge_frontier_
                  ? ReforgeEvaluatorVersion::V2Sparse
                  : ReforgeEvaluatorVersion::V1Raw;
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
    const ActionTransitionFacts transition_facts =
        action_transition_facts(action.params.type);
    const int eldritch_side =
        item.searing_exarch_tier > item.eater_of_worlds_tier
            ? PC_SIDE_PREFIX
            : (item.eater_of_worlds_tier > item.searing_exarch_tier
                   ? PC_SIDE_SUFFIX
                   : -1);
    pc_item_state base = preserved_reforge_base(session, action, item);

    /* A reforge's distribution depends only on the preserved base, so
     * states differing only in wiped mods share one roll DP. */
    std::uint64_t base_hash = 0;
    std::vector<std::uint64_t> base_observation =
        reforge_base_observation(base, &base_hash);
    const std::tuple<std::uint32_t, std::uint64_t, bool> memo_key{
        action_index, base_hash, goal_progress_gated};
    const auto memo = reforge_cache_.find(memo_key);
    if (memo != reforge_cache_.end()) {
        for (const ReforgeCacheMemo& candidate : memo->second) {
            if (candidate.observation_signature == base_observation) {
                ++telemetry_.reforge_hits;
                if (reforge_resource_accounting_) {
                    ReforgeRowTelemetry row;
                    row.sequence = telemetry_.reforge_row_sequence;
                    telemetry_.reforge_row_sequence = saturated_add(
                        telemetry_.reforge_row_sequence, 1);
                    row.action_index = action_index;
                    row.owner = reforge_row_owner_;
                    row.family = row_family;
                    row.evaluator = evaluator_version;
                    row.disposition = ReforgeRowDisposition::Completed;
                    row.cache_reused = true;
                    row.components.rows_cache_reused = 1;
                    merge_reforge_effort(
                        telemetry_.reforge_effort, row.components);
                    constexpr std::size_t kRowSampleLimit = 64;
                    try {
                        if (telemetry_.reforge_row_samples.size() <
                            kRowSampleLimit) {
                            telemetry_.reforge_row_samples.push_back(
                                std::move(row));
                        } else {
                            telemetry_.reforge_row_samples_omitted =
                                saturated_add(
                                    telemetry_.reforge_row_samples_omitted,
                                    1);
                        }
                    } catch (...) {
                        telemetry_.reforge_row_samples_omitted = saturated_add(
                            telemetry_.reforge_row_samples_omitted, 1);
                    }
                }
                return candidate.distribution;
            }
        }
    }
    ++telemetry_.reforge_misses;
    telemetry_timer.miss = true;

    std::optional<ReforgeEffortRecorder> effort_recorder;
    if (reforge_resource_accounting_) {
        effort_recorder.emplace(
            telemetry_, action_index, reforge_row_owner_, row_family,
            evaluator_version);
    }
    ReforgeEffortBreakdown disabled_effort;
    ReforgeEffortBreakdown& effort = effort_recorder.has_value()
                                           ? effort_recorder->row.components
                                           : disabled_effort;
    const auto credit_effort = [&](
        std::uint64_t& counter,
        const std::uint64_t amount = 1) {
        if (!reforge_resource_accounting_) return;
        counter = saturated_add(counter, amount);
    };

    ReforgeAttributionRecorder attribution_recorder{
        telemetry_, capture_reforge_attribution_};
    const bool capture_attribution =
        attribution_recorder.enabled;
    ReforgeBuildAttribution& attribution = attribution_recorder.row;
    attribution.action_id = action.id;
    attribution.action_index = action_index;
    attribution.preserved_base_hash = base_hash;
    attribution.goal_progress_gated = goal_progress_gated;
    attribution.projected_sparse_frontier =
        use_projected_reforge_frontier_;
    attribution.factored_terminal_accumulator =
        use_factored_terminal_reforge_;
    attribution_recorder.work_started =
        telemetry_.reforge_frontier_work;
    attribution_recorder.raw_equivalent_work_started =
        telemetry_.reforge_raw_equivalent_work;
    attribution_recorder.projected_work_started =
        telemetry_.reforge_projected_work;
    attribution_recorder.factored_work_started =
        telemetry_.reforge_factored_work;
    const auto consume_common_reforge_work =
        [&](const std::uint64_t amount) {
            telemetry_.reforge_raw_equivalent_work = saturated_add(
                telemetry_.reforge_raw_equivalent_work, amount);
            telemetry_.reforge_projected_work = saturated_add(
                telemetry_.reforge_projected_work, amount);
            telemetry_.reforge_factored_work = saturated_add(
                telemetry_.reforge_factored_work, amount);
            consume_reforge_work(amount, amount);
        };

    /* A goal-progress-gated row can merge a very large number of tiny
     * terminal branches into one canonical successor.  Accumulate those
     * stored-double branch masses in the same deterministic double-double
     * arithmetic used by the independent policy evaluator.  Converting to
     * the public double once, after the canonical successor is complete,
     * avoids order-sensitive rare-success drift in cost / probability. */
    std::unordered_map<std::uint32_t, solve_detail::WideFloat> outcome_acc;
    std::size_t outcome_reserve = 65536;
    if (state_cap_.has_value()) {
        outcome_reserve = std::min<std::size_t>(
            outcome_reserve, *state_cap_);
    }
    if (solve_discovered_state_cap_.has_value()) {
        outcome_reserve = std::min<std::size_t>(
            outcome_reserve, *solve_discovered_state_cap_);
    }
    outcome_reserve = std::max<std::size_t>(1, outcome_reserve);
    const std::uint64_t outcome_storage_budget =
        unordered_storage_bytes(
            outcome_reserve,
            sizeof(decltype(outcome_acc)::value_type));
    std::unordered_map<std::uint32_t, FinalSuccessorAttribution>
        final_successor_attribution;
    std::size_t final_successor_preflight_entries =
        capture_attribution ? outcome_reserve : 0;
    constexpr std::size_t kTerminalContributionSampleLimit = 16;
    const std::uint64_t terminal_sample_storage_bytes =
        capture_attribution
            ? saturated_bytes(
                  kTerminalContributionSampleLimit,
                  sizeof(ReforgeBuildAttribution::
                             TerminalContributionSample))
            : 0;
    std::uint64_t terminal_attribution_scratch_bytes =
        capture_attribution
            ? saturated_add(
                  unordered_storage_bytes(
                      final_successor_preflight_entries,
                      sizeof(decltype(
                          final_successor_attribution)::value_type)),
                  terminal_sample_storage_bytes)
            : 0;
    std::uint64_t factored_terminal_scratch_bytes = 0;
    require_reforge_scratch_bytes(saturated_add(
        outcome_storage_budget,
        terminal_attribution_scratch_bytes));
    outcome_acc.reserve(outcome_reserve);
    if (capture_attribution) {
        final_successor_attribution.reserve(
            final_successor_preflight_entries);
        attribution.terminal_contribution_samples.reserve(
            kTerminalContributionSampleLimit);
    }
    std::size_t outcome_preflight_entries = outcome_reserve;
    std::uint64_t stationary_reforge_scratch_bytes = 0;
    std::uint64_t active_frontier_scratch_bytes = 0;
    std::uint64_t availability_scratch_bytes = 0;
    const auto accumulate_outcome =
        [&](const std::uint32_t state,
            const double probability) -> bool {
            credit_effort(effort.successor_publication_attempts);
            if (capture_attribution) {
                ++attribution.successor_commits;
            }
            const auto found = outcome_acc.find(state);
            if (found != outcome_acc.end()) {
                credit_effort(effort.successor_duplicate_merges);
                found->second += solve_detail::WideFloat{probability};
                if (capture_attribution) {
                    ++attribution.duplicate_projected_outcomes;
                    attribution.duplicate_projected_probability_mass +=
                        probability;
                }
                return true;
            }
            const std::size_t required = outcome_acc.size() + 1;
            if (required > outcome_preflight_entries) {
                const std::size_t doubled =
                    outcome_preflight_entries >
                            std::numeric_limits<std::size_t>::max() / 2
                        ? std::numeric_limits<std::size_t>::max()
                        : outcome_preflight_entries * 2;
                outcome_preflight_entries =
                    std::max(required, doubled);
                const std::uint64_t projected =
                    saturated_add(
                        saturated_add(
                            saturated_add(
                                saturated_add(
                                    stationary_reforge_scratch_bytes,
                                    active_frontier_scratch_bytes),
                                availability_scratch_bytes),
                            saturated_add(
                                terminal_attribution_scratch_bytes,
                                factored_terminal_scratch_bytes)),
                    unordered_storage_bytes(
                        outcome_preflight_entries,
                        sizeof(decltype(outcome_acc)::value_type)));
                require_reforge_scratch_bytes(projected);
                outcome_acc.reserve(outcome_preflight_entries);
            }
            outcome_acc.emplace(
                state, solve_detail::WideFloat{probability});
            credit_effort(effort.successor_unique_insertions);
            return false;
        };
    /* Self-loop results reference the querying state and must not be
     * shared through the base memo. */
    bool state_dependent = false;
    const auto finalize = [&](const bool allow_shared_kernel = false)
        -> std::shared_ptr<const OutcomeDistribution> {
        const auto finalize_started =
            std::chrono::steady_clock::now();
        if (capture_attribution) {
            attribution.final_depth_unique_canonical_successors =
                final_successor_attribution.size();
            attribution.final_depth_duplicate_canonical_commits =
                attribution.final_depth_canonical_commits >=
                        attribution
                            .final_depth_unique_canonical_successors
                    ? attribution.final_depth_canonical_commits -
                          attribution
                              .final_depth_unique_canonical_successors
                    : 0;
            for (const auto& [unused, successor] :
                 final_successor_attribution) {
                (void)unused;
                attribution.final_depth_max_predecessors_per_successor =
                    std::max(
                        attribution
                            .final_depth_max_predecessors_per_successor,
                        successor.distinct_predecessors);
                if (successor.distinct_predecessors > 1) {
                    ++attribution
                          .final_depth_successors_with_multiple_predecessors;
                    attribution
                        .final_depth_predecessor_convergence_excess =
                        saturated_add(
                            attribution
                                .final_depth_predecessor_convergence_excess,
                            successor.distinct_predecessors - 1);
                }
            }
            std::sort(
                attribution.terminal_contribution_samples.begin(),
                attribution.terminal_contribution_samples.end(),
                [](const auto& left, const auto& right) {
                    return std::tie(
                               left.target_count,
                               left.predecessor_sequence,
                               left.terminal_bucket,
                               left.canonical_successor_hash,
                               left.probability_mass) <
                           std::tie(
                               right.target_count,
                               right.predecessor_sequence,
                               right.terminal_bucket,
                               right.canonical_successor_hash,
                               right.probability_mass);
                });
        }
        std::vector<std::pair<std::uint32_t, double>> ordered_outcomes;
        ordered_outcomes.reserve(outcome_acc.size());
        for (const auto& [successor, probability] : outcome_acc) {
            ordered_outcomes.emplace_back(
                successor, probability.value());
        }
        std::sort(ordered_outcomes.begin(), ordered_outcomes.end());
        solve_detail::WideFloat committed_mass{0.0};
        for (const auto& [successor, probability] : ordered_outcomes) {
            (void)successor;
            committed_mass += solve_detail::WideFloat{probability};
        }
        double committed = committed_mass.value();
        if (committed <= 0.0) {
            outcome_acc.clear();
            accumulate_outcome(state_id, 1.0);
            ordered_outcomes.assign({{state_id, 1.0}});
            committed_mass = solve_detail::WideFloat{1.0};
            committed = 1.0;
            state_dependent = true;
        }
        if (goal_progress_gated &&
            std::abs(committed - 1.0) > 1e-10) {
            throw std::logic_error(
                "goal-progress-gated reforge failed probability "
                "conservation: committed=" + std::to_string(committed));
        }
        for (const auto& [successor, probability] : ordered_outcomes) {
            result.entries.push_back({
                successor,
                goal_progress_gated ? probability
                                    : (solve_detail::WideFloat{probability} /
                                       committed_mass)
                                          .value()});
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
        if (goal_progress_gated) {
            result.goal_progress_gated = true;
            solve_detail::WideFloat terminal_mass{0.0};
            solve_detail::WideFloat retry_mass{0.0};
            solve_detail::WideFloat partial_mass{0.0};
            for (const OutcomeEntry& entry : result.entries) {
                if (entry.state == result.gated_terminal_state) {
                    terminal_mass +=
                        solve_detail::WideFloat{entry.probability};
                } else if (entry.state == result.gated_retry_state) {
                    retry_mass +=
                        solve_detail::WideFloat{entry.probability};
                } else {
                    partial_mass +=
                        solve_detail::WideFloat{entry.probability};
                    ++result.gated_partial_states;
                }
            }
            result.gated_terminal_probability = terminal_mass.value();
            result.gated_retry_probability = retry_mass.value();
            result.gated_partial_probability = partial_mass.value();
            std::uint64_t kernel_hash = 1469598103934665603ull;
            const auto mix_hash = [&](const std::uint64_t value) {
                kernel_hash ^= value;
                kernel_hash *= 1099511628211ull;
            };
            std::vector<std::pair<std::uint64_t, std::uint64_t>>
                canonical_kernel_bits;
            canonical_kernel_bits.reserve(result.entries.size());
            for (const OutcomeEntry& entry : result.entries) {
                canonical_kernel_bits.push_back({
                    abstract_state_hash(states_.at(entry.state)),
                    std::bit_cast<std::uint64_t>(entry.probability)});
            }
            std::sort(
                canonical_kernel_bits.begin(),
                canonical_kernel_bits.end());
            for (const auto& [state_hash, probability_bits] :
                 canonical_kernel_bits) {
                mix_hash(state_hash);
                mix_hash(probability_bits);
            }
            result.gated_kernel_bits_hash = kernel_hash;
            if (telemetry_.gated_reforge_rows == 0) {
                telemetry_.gated_first_kernel_bits_hash = kernel_hash;
            }
            ++telemetry_.gated_reforge_rows;
            telemetry_.gated_terminal_probability =
                (solve_detail::WideFloat{
                     telemetry_.gated_terminal_probability} +
                 solve_detail::WideFloat{
                     result.gated_terminal_probability})
                    .value();
            telemetry_.gated_retry_probability =
                (solve_detail::WideFloat{
                     telemetry_.gated_retry_probability} +
                 solve_detail::WideFloat{
                     result.gated_retry_probability})
                    .value();
            telemetry_.gated_partial_probability =
                (solve_detail::WideFloat{
                     telemetry_.gated_partial_probability} +
                 solve_detail::WideFloat{
                     result.gated_partial_probability})
                    .value();
            telemetry_.gated_terminal_short_circuits +=
                result.gated_terminal_short_circuits;
            telemetry_.gated_retry_short_circuits +=
                result.gated_retry_short_circuits;
            telemetry_.gated_partial_states +=
                result.gated_partial_states;
        }
        result.supported = true;
        const bool retain_shared_kernel =
            allow_shared_kernel && !state_dependent &&
            can_retain_reforge_distribution(result);
        result.stable_shared_kernel = retain_shared_kernel;
        attribution.unique_projected_outcomes =
            result.entries.size();
        attribution.completed = true;
        attribution.finalize_ns += elapsed_ns(finalize_started);
        std::shared_ptr<const OutcomeDistribution> finalized =
            std::make_shared<OutcomeDistribution>(std::move(result));
        if (retain_shared_kernel) {
            auto& bucket = reforge_cache_[memo_key];
            const std::size_t old_capacity = bucket.capacity();
            bucket.push_back({std::move(base_observation), finalized});
            account_reforge_cache_insert(
                memo_key, old_capacity, bucket.capacity(), bucket.back());
        }
        if (effort_recorder.has_value()) {
            effort_recorder->completed = true;
        }
        return finalized;
    };
    const auto unapplied = [&]() -> std::shared_ptr<const OutcomeDistribution> {
        outcome_acc.clear();
        accumulate_outcome(state_id, 1.0);
        state_dependent = true;
        return finalize();
    };

    /* The retry carrier predates direct mods: a later destructive reforge
     * wipes Essence/Fossil direct modifiers together with every other
     * unpreserved affix. */
    pc_item_state retry_base = base;

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
    attribution.forced_modifier_count = directs.size();
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
    retry_base.item_flags |= special_flags;

    /* --- roll pool and buckets --------------------------------------------- */
    PoolBuildRequest request;
    request.respects_metamod_pool_blocks =
        transition_facts.respects_metamod_pool_blocks;
    if (action.params.type == ActionType::Fossil) {
        request.weight_kind = PoolWeightKind::Fossil;
        request.fossil_indices = action.params.fossil_indices;
    }
    if (eldritch_reforge && eldritch_side >= 0) {
        request.side_filter = eldritch_side;
    }
    const auto pool_started = std::chrono::steady_clock::now();
    const WeightedPool& pool = get_weighted_pool(context_, &base, request);
    attribution.pool_build_ns += elapsed_ns(pool_started);
    if (capture_attribution) {
        for (const PoolEntry& entry : pool.entries) {
            if (entry.final_weight == 0) continue;
            ++attribution.natural_pool_entries;
            attribution.natural_pool_weight += entry.final_weight;
            if (entry.gen_type == PC_SIDE_PREFIX) {
                ++attribution.natural_prefix_entries;
                attribution.natural_prefix_weight += entry.final_weight;
            } else if (entry.gen_type == PC_SIDE_SUFFIX) {
                ++attribution.natural_suffix_entries;
                attribution.natural_suffix_weight += entry.final_weight;
            }
        }
    }
    const auto bucket_started = std::chrono::steady_clock::now();

    /* Aggregate pool weights per complete (side, exclusion-group family)
     * with goal classification. Harvest reforge overlays a
     * second weight per family: the guaranteed first pick's targeted
     * naturally rollable pool. */
    const bool harvest = action.params.type == ActionType::HarvestReforge;
    const bool defer_identity_choice =
        distinguishes_modifier_identity() && !product_solver_parent_;
    struct WeightPair {
        struct RawWeight {
            std::uint32_t mod_id = kNoId;
            std::uint64_t normal = 0;
            std::uint64_t guaranteed = 0;
        };
        std::uint64_t normal = 0;
        std::uint64_t guaranteed = 0;
        std::vector<RawWeight> raw_weights;
    };
    struct FamilyAgg {
        std::array<std::map<std::uint32_t, WeightPair>, kMaxGoalSlots>
            sat{};
        std::array<std::map<std::uint32_t, WeightPair>, kMaxGoalSlots>
            below{};
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
            if (!defer_identity_choice) return;
            /*
             * Identity metadata is bounded work too. Charging before the
             * vector search/allocation prevents a strict kernel from building
             * an unmetered raw-choice table ahead of the frontier cap.
             */
            consume_common_reforge_work(1);
            credit_effort(effort.raw_choice_entries);
            if (capture_attribution) {
                ++attribution.raw_choice_table_work;
            }
            const auto found = std::lower_bound(
                pair.raw_weights.begin(), pair.raw_weights.end(), mod,
                [](const WeightPair::RawWeight& candidate,
                   const std::uint32_t target) {
                    return candidate.mod_id < target;
                });
            WeightPair::RawWeight* raw = nullptr;
            if (found == pair.raw_weights.end() ||
                found->mod_id != mod) {
                raw = &*pair.raw_weights.insert(
                    found, WeightPair::RawWeight{mod});
            } else {
                raw = &*found;
            }
            if (guaranteed) {
                raw->guaranteed += entry.final_weight;
            } else {
                raw->normal += entry.final_weight;
            }
        };
        for (std::size_t s = 0; s < layout_.slots.size(); ++s) {
            if (pc_bitset_test(layout_.slots[s].satisfying_mask.data(),
                               mod)) {
                const auto& tokens =
                    layout_.slots[s].member_class_token_by_mod;
                const std::uint32_t token =
                    tokens.empty() ? 0 : tokens[mod];
                if (!tokens.empty() && token == 0) {
                    throw std::logic_error(
                        "exact goal member has no refinement class");
                }
                credit(family.sat[s][token]);
                return;
            }
            if (pc_bitset_test(layout_.slots[s].member_mask.data(), mod)) {
                const auto& tokens =
                    layout_.slots[s].member_class_token_by_mod;
                const std::uint32_t token =
                    tokens.empty() ? 0 : tokens[mod];
                if (!tokens.empty() && token == 0) {
                    throw std::logic_error(
                        "exact goal member has no refinement class");
                }
                credit(family.below[s][token]);
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
        credit_effort(effort.pool_entries_scanned);
        if (entry.final_weight == 0) continue;
        classify(entry, false);
    }
    if (harvest) {
        PoolBuildRequest guaranteed_request;
        guaranteed_request.weight_kind = PoolWeightKind::TargetedNatural;
        guaranteed_request.target_tag_id = action.params.target_tag_id;
        const auto guaranteed_pool_started =
            std::chrono::steady_clock::now();
        const WeightedPool& guaranteed_pool =
            get_weighted_pool(context_, &base, guaranteed_request);
        attribution.pool_build_ns +=
            elapsed_ns(guaranteed_pool_started);
        for (const PoolEntry& entry : guaranteed_pool.entries) {
            credit_effort(effort.pool_entries_scanned);
            if (entry.final_weight == 0) continue;
            if (capture_attribution) {
                ++attribution.guaranteed_pool_entries;
                attribution.guaranteed_pool_weight +=
                    entry.final_weight;
            }
            classify(entry, true);
        }
    }
    attribution.physical_families = families.size();
    credit_effort(
        effort.physical_families_built, families.size());

    /*
     * Product-parent reforge compression.
     *
     * A junk-only exclusion family whose groups overlap no other family has
     * no interaction beyond consuming itself. Families with the same side,
     * coarse junk observation, block mask, and per-family weights are exact
     * exchangeable twins. Represent them as one availability class with a
     * multiplicity rather than building a strict physical-family frontier.
     *
     * This deliberately excludes goal, below-tier, multi-observation, and
     * externally conflicting families. Those retain their complete group
     * identity, making this a proved symmetry reduction rather than an
     * approximation.
     */
    using AggregateKey = std::tuple<
        std::int8_t, std::uint32_t, std::uint32_t,
        std::uint64_t, std::uint64_t>;
    std::vector<const std::pair<const FamilyKey, FamilyAgg>*> ordered_families;
    ordered_families.reserve(families.size());
    for (const auto& family : families) {
        ordered_families.push_back(&family);
    }
    const auto group_sets_intersect = [](
            const std::vector<std::uint32_t>& lhs,
            const std::vector<std::uint32_t>& rhs) {
        std::size_t a = 0;
        std::size_t b = 0;
        while (a < lhs.size() && b < rhs.size()) {
            if (lhs[a] == rhs[b]) return true;
            if (lhs[a] < rhs[b]) ++a;
            else ++b;
        }
        return false;
    };
    std::vector<std::optional<AggregateKey>> projectable_key_by_family(
        ordered_families.size());
    std::map<AggregateKey, std::uint32_t> projectable_multiplicity;
    if (product_solver_parent_ || capture_attribution) {
        for (std::size_t i = 0; i < ordered_families.size(); ++i) {
            const auto& [family_key, family] = *ordered_families[i];
            bool goal_observed = false;
            for (std::size_t slot = 0; slot < layout_.slots.size(); ++slot) {
                const auto has_weight =
                    [](const std::map<std::uint32_t, WeightPair>& classes) {
                        return std::any_of(
                            classes.begin(), classes.end(),
                            [](const auto& entry) {
                                return entry.second.normal != 0 ||
                                       entry.second.guaranteed != 0;
                            });
                    };
                goal_observed |= has_weight(family.sat[slot]) ||
                                 has_weight(family.below[slot]);
            }
            if (goal_observed || family.junk.size() != 1) continue;
            bool externally_disjoint = true;
            for (std::size_t j = 0; j < ordered_families.size(); ++j) {
                if (i == j) continue;
                if (group_sets_intersect(
                        family_key.second,
                        ordered_families[j]->first.second)) {
                    externally_disjoint = false;
                    break;
                }
            }
            if (!externally_disjoint) continue;
            const auto& [junk_key, weights] = *family.junk.begin();
            const AggregateKey aggregate{
                family_key.first, junk_key.first, junk_key.second,
                weights.normal, weights.guaranteed};
            projectable_key_by_family[i] = aggregate;
            ++projectable_multiplicity[aggregate];
        }
    }
    if (capture_attribution) {
        for (const auto& candidate : projectable_key_by_family) {
            if (candidate.has_value()) {
                ++attribution.projectable_physical_families;
            }
        }
        attribution.projectable_family_classes =
            projectable_multiplicity.size();
        attribution.projected_families_removed =
            attribution.projectable_physical_families >=
                    attribution.projectable_family_classes
                ? attribution.projectable_physical_families -
                      attribution.projectable_family_classes
                : 0;
    }

    std::vector<std::optional<AggregateKey>> aggregate_key_by_family(
        ordered_families.size());
    std::map<AggregateKey, std::uint32_t> aggregate_multiplicity;
    if (product_solver_parent_) {
        aggregate_key_by_family = projectable_key_by_family;
        aggregate_multiplicity = projectable_multiplicity;
    }

    std::vector<RollBucket> buckets;
    std::set<AggregateKey> emitted_aggregates;
    std::uint32_t next_family_class = 0;
    for (std::size_t family_index = 0;
         family_index < ordered_families.size(); ++family_index) {
        const auto& [key, family] = *ordered_families[family_index];
        if (aggregate_key_by_family[family_index].has_value()) {
            const AggregateKey& aggregate =
                *aggregate_key_by_family[family_index];
            if (!emitted_aggregates.insert(aggregate).second) continue;
            const auto& [junk_key, weights] = *family.junk.begin();
            RollBucket bucket;
            bucket.side = key.first;
            bucket.kind = BucketKind::Junk;
            bucket.junk_class = junk_key.first;
            bucket.block_mask = junk_key.second;
            bucket.weight = weights.normal;
            bucket.guaranteed = weights.guaranteed;
            bucket.multiplicity =
                aggregate_multiplicity.at(aggregate);
            bucket.family_class = next_family_class++;
            bucket.interchangeable_family_class = true;
            /* Proven externally disjoint above. */
            buckets.push_back(std::move(bucket));
            continue;
        }
        const std::uint32_t family_class = next_family_class++;
        /*
         * A non-empty exact group vector makes every modifier in this family
         * mutually exclusive. On an identity-witness layout, defer the
         * literal tier choice while retaining the structural roll effect.
         * Later availability and every denominator depend only on this
         * family, side, goal status, and block mask. The raw choice is expanded
         * at commit, before an AbstractState is interned.
         *
         * Empty-group modifiers are intentionally left as singleton buckets:
         * they can co-exist, so selecting one changes which literal choices
         * remain available.
         */
        if (defer_identity_choice && !key.second.empty()) {
            const auto append_raw_choices =
                [](RollBucket& bucket,
                   const WeightPair& weights,
                   const std::uint32_t goal_token,
                   const std::uint32_t junk_class) {
                    bucket.weight += weights.normal;
                    bucket.guaranteed += weights.guaranteed;
                    for (const WeightPair::RawWeight& raw :
                         weights.raw_weights) {
                        bucket.raw_choices.push_back({
                            raw.mod_id, goal_token, junk_class,
                            raw.normal, raw.guaranteed});
                    }
                };
            const auto finish_bucket =
                [&](RollBucket bucket) {
                    if (bucket.weight == 0 &&
                        bucket.guaranteed == 0) {
                        return;
                    }
                    std::sort(
                        bucket.raw_choices.begin(),
                        bucket.raw_choices.end(),
                        [](const RollBucket::RawChoice& left,
                           const RollBucket::RawChoice& right) {
                            return left.mod_id < right.mod_id;
                        });
                    if (bucket.raw_choices.empty() ||
                        std::adjacent_find(
                            bucket.raw_choices.begin(),
                            bucket.raw_choices.end(),
                            [](const RollBucket::RawChoice& left,
                               const RollBucket::RawChoice& right) {
                                return left.mod_id == right.mod_id;
                            }) != bucket.raw_choices.end()) {
                        throw std::logic_error(
                            "identity-witness reforge bucket has invalid "
                            "raw modifier choices");
                    }
                    buckets.push_back(std::move(bucket));
                };
            for (std::size_t s = 0;
                 s < layout_.slots.size(); ++s) {
                RollBucket satisfied;
                satisfied.side = key.first;
                satisfied.kind = BucketKind::GoalSat;
                satisfied.slot = static_cast<std::uint8_t>(s);
                satisfied.exclusion_groups = key.second;
                satisfied.family_class = family_class;
                for (const auto& [token, weights] :
                     family.sat[s]) {
                    append_raw_choices(
                        satisfied, weights, token, kNoId);
                }
                finish_bucket(std::move(satisfied));

                RollBucket below;
                below.side = key.first;
                below.kind = BucketKind::GoalBelow;
                below.slot = static_cast<std::uint8_t>(s);
                below.exclusion_groups = key.second;
                below.family_class = family_class;
                for (const auto& [token, weights] :
                     family.below[s]) {
                    append_raw_choices(
                        below, weights, token, kNoId);
                }
                finish_bucket(std::move(below));
            }
            std::map<std::uint32_t, RollBucket> junk_by_block;
            for (const auto& [junk_key, weights] : family.junk) {
                RollBucket& bucket = junk_by_block[junk_key.second];
                bucket.side = key.first;
                bucket.kind = BucketKind::Junk;
                bucket.block_mask = junk_key.second;
                bucket.exclusion_groups = key.second;
                bucket.family_class = family_class;
                append_raw_choices(
                    bucket, weights, 0, junk_key.first);
            }
            for (auto& [unused, bucket] : junk_by_block) {
                (void)unused;
                finish_bucket(std::move(bucket));
            }
            continue;
        }
        for (std::size_t s = 0; s < layout_.slots.size(); ++s) {
            for (const auto& [token, weights] : family.sat[s]) {
                if (weights.normal == 0 && weights.guaranteed == 0) {
                    continue;
                }
                RollBucket bucket;
                bucket.side = key.first;
                bucket.kind = BucketKind::GoalSat;
                bucket.slot = static_cast<std::uint8_t>(s);
                bucket.goal_member_class_token = token;
                bucket.weight = weights.normal;
                bucket.guaranteed = weights.guaranteed;
                bucket.exclusion_groups = key.second;
                bucket.family_class = family_class;
                buckets.push_back(std::move(bucket));
            }
            for (const auto& [token, weights] : family.below[s]) {
                if (weights.normal == 0 && weights.guaranteed == 0) {
                    continue;
                }
                RollBucket bucket;
                bucket.side = key.first;
                bucket.kind = BucketKind::GoalBelow;
                bucket.slot = static_cast<std::uint8_t>(s);
                bucket.goal_member_class_token = token;
                bucket.weight = weights.normal;
                bucket.guaranteed = weights.guaranteed;
                bucket.exclusion_groups = key.second;
                bucket.family_class = family_class;
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
            bucket.family_class = family_class;
            buckets.push_back(std::move(bucket));
        }
    }
    /* Group exclusion is immutable for this roll. Precompute the exact
     * pairwise relation once instead of repeatedly intersecting sorted group
     * vectors in every frontier state and candidate-bucket visit. */
    const std::size_t bucket_count = buckets.size();
    attribution.roll_buckets = bucket_count;
    credit_effort(effort.roll_buckets_built, bucket_count);
    if (capture_attribution) {
      for (const RollBucket& bucket : buckets) {
        if (bucket.side == PC_SIDE_PREFIX) {
            ++attribution.prefix_buckets;
        } else if (bucket.side == PC_SIDE_SUFFIX) {
            ++attribution.suffix_buckets;
        }
        if (bucket.kind == BucketKind::GoalSat) {
            ++attribution.goal_satisfied_buckets;
        } else if (bucket.kind == BucketKind::GoalBelow) {
            ++attribution.goal_below_buckets;
        } else {
            ++attribution.junk_buckets;
        }
        attribution.raw_choice_entries += bucket.raw_choices.size();
        attribution.exclusion_group_entries +=
            bucket.exclusion_groups.size();
      }
    }
    std::uint64_t structural_hash = 1469598103934665603ull;
    const auto mix_structural = [&](const std::uint64_t value) {
        structural_hash ^= value;
        structural_hash *= 1099511628211ull;
    };
    mix_structural(bucket_count);
    for (const RollBucket& bucket : buckets) {
        mix_structural(static_cast<std::uint8_t>(bucket.side));
        mix_structural(static_cast<std::uint8_t>(bucket.kind));
        mix_structural(bucket.slot);
        mix_structural(bucket.goal_member_class_token);
        mix_structural(bucket.junk_class);
        mix_structural(bucket.block_mask);
        mix_structural(bucket.weight);
        mix_structural(bucket.guaranteed);
        mix_structural(bucket.multiplicity);
        for (const std::uint32_t group : bucket.exclusion_groups) {
            mix_structural(group);
        }
    }
    attribution.structural_bits_hash = structural_hash;
    attribution.bucket_build_ns = elapsed_ns(bucket_started);
    if (bucket_count >
        std::numeric_limits<std::uint16_t>::max()) {
        throw SolverResourceLimit(
            "max_reforge_buckets",
            std::numeric_limits<std::uint16_t>::max());
    }
    stationary_reforge_scratch_bytes = 0;
    const auto add_stationary =
        [&](const std::uint64_t bytes) {
            stationary_reforge_scratch_bytes =
                saturated_add(
                    stationary_reforge_scratch_bytes, bytes);
        };
    add_stationary(ordered_storage_bytes(
        families.size(),
        sizeof(decltype(families)::value_type)));
    for (const auto& [family_key, family] : families) {
        add_stationary(saturated_bytes(
            family_key.second.capacity(),
            sizeof(std::uint32_t)));
        const auto add_weight_map =
            [&](const auto& weights) {
                add_stationary(ordered_storage_bytes(
                    weights.size(),
                    sizeof(typename std::decay_t<
                        decltype(weights)>::value_type)));
                for (const auto& [unused, pair] : weights) {
                    (void)unused;
                    add_stationary(saturated_bytes(
                        pair.raw_weights.capacity(),
                        sizeof(WeightPair::RawWeight)));
                }
            };
        for (std::size_t slot = 0;
             slot < layout_.slots.size(); ++slot) {
            add_weight_map(family.sat[slot]);
            add_weight_map(family.below[slot]);
        }
        add_weight_map(family.junk);
    }
    add_stationary(saturated_bytes(
        ordered_families.capacity(),
        sizeof(decltype(ordered_families)::value_type)));
    add_stationary(saturated_bytes(
        aggregate_key_by_family.capacity(),
        sizeof(decltype(aggregate_key_by_family)::value_type)));
    add_stationary(ordered_storage_bytes(
        aggregate_multiplicity.size(),
        sizeof(decltype(aggregate_multiplicity)::value_type)));
    add_stationary(ordered_storage_bytes(
        emitted_aggregates.size(),
        sizeof(decltype(emitted_aggregates)::value_type)));
    add_stationary(saturated_bytes(
        buckets.capacity(), sizeof(RollBucket)));
    for (const RollBucket& bucket : buckets) {
        add_stationary(saturated_bytes(
            bucket.raw_choices.capacity(),
            sizeof(RollBucket::RawChoice)));
        add_stationary(saturated_bytes(
            bucket.exclusion_groups.capacity(),
            sizeof(std::uint32_t)));
    }
    const std::size_t availability_word_count =
        (bucket_count + 63) / 64;
    const std::uint64_t conflict_bytes =
        use_projected_reforge_frontier_
            ? saturated_bytes(
                  bucket_count,
                  saturated_bytes(
                      availability_word_count,
                      sizeof(std::uint64_t)))
            : saturated_bytes(
                  bucket_count,
                  saturated_bytes(
                      bucket_count, sizeof(std::uint8_t)));
    add_stationary(conflict_bytes);
    if (use_projected_reforge_frontier_) {
        add_stationary(saturated_bytes(
            2 + layout_.slots.size(),
            saturated_bytes(
                availability_word_count,
                sizeof(std::uint64_t))));
        add_stationary(saturated_bytes(
            bucket_count, sizeof(std::uint16_t)));
    }
    add_stationary(saturated_bytes(
        bucket_count, sizeof(double)));
    require_reforge_scratch_bytes(saturated_add(
        saturated_add(
            stationary_reforge_scratch_bytes,
            saturated_add(
                terminal_attribution_scratch_bytes,
                factored_terminal_scratch_bytes)),
        unordered_storage_bytes(
            outcome_preflight_entries,
            sizeof(decltype(outcome_acc)::value_type))));
    std::vector<std::uint8_t> buckets_conflict;
    std::vector<std::vector<std::uint64_t>> bucket_conflict_masks;
    if (use_projected_reforge_frontier_) {
        bucket_conflict_masks.assign(
            bucket_count,
            std::vector<std::uint64_t>(availability_word_count, 0));
    } else {
        buckets_conflict.assign(bucket_count * bucket_count, 0);
    }
    const auto exclusion_started =
        std::chrono::steady_clock::now();
    for (std::size_t left = 0; left < bucket_count; ++left) {
        for (std::size_t right = left; right < bucket_count; ++right) {
            credit_effort(effort.exclusion_group_checks);
            if (capture_attribution) {
                ++attribution.exclusion_pair_checks;
            }
            if (left == right &&
                buckets[left].interchangeable_family_class) {
                continue;
            }
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
            if (use_projected_reforge_frontier_) {
                if (conflict) {
                    bucket_conflict_masks[left][right / 64] |=
                        std::uint64_t{1} << (right % 64);
                    bucket_conflict_masks[right][left / 64] |=
                        std::uint64_t{1} << (left % 64);
                }
            } else {
                buckets_conflict[left * bucket_count + right] = conflict;
                buckets_conflict[right * bucket_count + left] = conflict;
            }
            if (capture_attribution && conflict) {
                ++attribution.exclusion_conflicts;
            }
        }
    }
    attribution.exclusion_build_ns = elapsed_ns(exclusion_started);

    std::array<std::vector<std::uint64_t>, 2> side_bucket_masks;
    std::array<std::vector<std::uint64_t>, kMaxGoalSlots>
        occupied_bucket_masks;
    std::vector<std::vector<std::uint16_t>> family_class_buckets;
    if (use_projected_reforge_frontier_) {
        for (auto& mask : side_bucket_masks) {
            mask.assign(availability_word_count, 0);
        }
        for (auto& mask : occupied_bucket_masks) {
            mask.assign(availability_word_count, 0);
        }
        family_class_buckets.resize(next_family_class);
        for (std::uint16_t b = 0;
             b < static_cast<std::uint16_t>(bucket_count); ++b) {
            const RollBucket& bucket = buckets[b];
            side_bucket_masks[static_cast<std::size_t>(bucket.side)]
                              [b / 64] |=
                std::uint64_t{1} << (b % 64);
            if (bucket.kind != BucketKind::Junk) {
                occupied_bucket_masks[bucket.slot][b / 64] |=
                    std::uint64_t{1} << (b % 64);
            } else {
                for (std::size_t slot = 0;
                     slot < layout_.slots.size(); ++slot) {
                    if ((bucket.block_mask & (1u << slot)) != 0) {
                        occupied_bucket_masks[slot][b / 64] |=
                            std::uint64_t{1} << (b % 64);
                    }
                }
            }
            family_class_buckets[bucket.family_class].push_back(b);
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

    std::optional<ActiveTerminalBranch> active_terminal_branch;
    const auto roll_signature_hash = [](const RollState& roll) {
        std::uint64_t hash = 1469598103934665603ull;
        const auto mix = [&](const std::uint64_t value) {
            hash ^= value;
            hash *= 1099511628211ull;
        };
        mix(roll.sat_mask);
        mix(roll.below_mask);
        mix(roll.blocked_mask);
        for (const std::uint32_t token :
             roll.goal_member_class_tokens) {
            mix(token);
        }
        mix(roll.prefix_picks);
        mix(roll.suffix_picks);
        mix(roll.guaranteed_bucket);
        mix(roll.availability_class);
        for (std::uint8_t pick = 0; pick < roll.pick_count; ++pick) {
            mix(roll.picks[pick].first);
            mix(roll.picks[pick].second);
        }
        return hash;
    };
    const auto exclusion_signature_hash = [](const RollBucket& bucket) {
        std::uint64_t hash = 1469598103934665603ull;
        for (const std::uint32_t group : bucket.exclusion_groups) {
            hash ^= group;
            hash *= 1099511628211ull;
        }
        return hash;
    };
    const auto record_target_contribution =
        [&](const int target,
            const double probability,
            const bool final_modifier_branch) {
            if (!capture_attribution || !(probability > 0.0) ||
                target < 0 ||
                target >= static_cast<int>(
                    attribution.terminal_target_counts.size())) {
                return;
            }
            auto& target_row =
                attribution.terminal_target_counts[target];
            ++target_row.state_contributions;
            target_row.probability_mass += probability;
            if (final_modifier_branch) {
                ++target_row.final_modifier_branches;
                target_row.final_modifier_probability_mass +=
                    probability;
            }
        };
    const auto record_terminal_successor =
        [&](const std::uint32_t state,
            const double probability) {
            if (!capture_attribution ||
                !active_terminal_branch.has_value()) {
                return;
            }
            const ActiveTerminalBranch& branch =
                *active_terminal_branch;
            const RollBucket& bucket =
                buckets[branch.terminal_bucket];
            const std::size_t required =
                final_successor_attribution.size() + 1;
            if (final_successor_attribution.find(state) ==
                    final_successor_attribution.end() &&
                required > final_successor_preflight_entries) {
                const std::size_t doubled =
                    final_successor_preflight_entries >
                            std::numeric_limits<std::size_t>::max() / 2
                        ? std::numeric_limits<std::size_t>::max()
                        : final_successor_preflight_entries * 2;
                final_successor_preflight_entries =
                    std::max(required, doubled);
                terminal_attribution_scratch_bytes = saturated_add(
                    unordered_storage_bytes(
                        final_successor_preflight_entries,
                        sizeof(decltype(
                            final_successor_attribution)::value_type)),
                    terminal_sample_storage_bytes);
                const std::uint64_t projected = saturated_add(
                    saturated_add(
                        saturated_add(
                            saturated_add(
                                stationary_reforge_scratch_bytes,
                                active_frontier_scratch_bytes),
                            availability_scratch_bytes),
                        saturated_add(
                            terminal_attribution_scratch_bytes,
                            factored_terminal_scratch_bytes)),
                    unordered_storage_bytes(
                        outcome_preflight_entries,
                        sizeof(decltype(outcome_acc)::value_type)));
                require_reforge_scratch_bytes(projected);
                final_successor_attribution.reserve(
                    final_successor_preflight_entries);
            }
            auto [found, inserted] =
                final_successor_attribution.try_emplace(
                    state, FinalSuccessorAttribution{});
            FinalSuccessorAttribution& successor = found->second;
            const bool duplicate = !inserted;
            bool duplicate_within = false;
            bool duplicate_across = false;
            bool different_terminal_bucket = false;
            bool different_predecessor_observation = false;
            if (inserted) {
                successor.last_predecessor_sequence =
                    branch.predecessor_sequence;
                successor.distinct_predecessors = 1;
                successor.first_terminal_bucket =
                    branch.terminal_bucket;
                successor.first_predecessor_sat_mask =
                    branch.predecessor.sat_mask;
                successor.first_predecessor_below_mask =
                    branch.predecessor.below_mask;
                successor.first_predecessor_blocked_mask =
                    branch.predecessor.blocked_mask;
                successor.first_predecessor_prefix_picks =
                    branch.predecessor.prefix_picks;
                successor.first_predecessor_suffix_picks =
                    branch.predecessor.suffix_picks;
                successor.first_predecessor_availability_class =
                    branch.predecessor.availability_class;
            } else if (successor.last_predecessor_sequence ==
                       branch.predecessor_sequence) {
                duplicate_within = true;
            } else {
                duplicate_across = true;
                successor.last_predecessor_sequence =
                    branch.predecessor_sequence;
                ++successor.distinct_predecessors;
            }
            if (duplicate) {
                const RollBucket& first_bucket =
                    buckets[successor.first_terminal_bucket];
                different_terminal_bucket =
                    successor.first_terminal_bucket !=
                    branch.terminal_bucket;
                if (different_terminal_bucket) {
                    ++attribution
                          .final_depth_duplicates_different_terminal_bucket;
                    attribution
                        .final_depth_different_terminal_bucket_duplicate_probability_mass +=
                        probability;
                } else {
                    ++attribution
                          .final_depth_duplicates_same_terminal_bucket;
                    attribution
                        .final_depth_same_terminal_bucket_duplicate_probability_mass +=
                        probability;
                }
                if (first_bucket.side != bucket.side) {
                    ++attribution
                          .final_depth_duplicates_different_terminal_side;
                }
                if (first_bucket.kind != bucket.kind) {
                    ++attribution
                          .final_depth_duplicates_different_terminal_kind;
                }
                if (first_bucket.exclusion_groups !=
                    bucket.exclusion_groups) {
                    ++attribution
                          .final_depth_duplicates_different_terminal_exclusion_signature;
                }
                different_predecessor_observation =
                    successor.first_predecessor_sat_mask !=
                        branch.predecessor.sat_mask ||
                    successor.first_predecessor_below_mask !=
                        branch.predecessor.below_mask ||
                    successor.first_predecessor_blocked_mask !=
                        branch.predecessor.blocked_mask ||
                    successor.first_predecessor_prefix_picks !=
                        branch.predecessor.prefix_picks ||
                    successor.first_predecessor_suffix_picks !=
                        branch.predecessor.suffix_picks;
                if (different_predecessor_observation) {
                    ++attribution
                          .final_depth_duplicates_different_predecessor_observation;
                    attribution
                        .final_depth_different_predecessor_observation_duplicate_probability_mass +=
                        probability;
                }
                if (successor.first_predecessor_availability_class !=
                    branch.predecessor.availability_class) {
                    ++attribution
                          .final_depth_duplicates_different_predecessor_availability;
                }
            }
            ++successor.contributions;
            successor.probability_mass += probability;
            ++attribution.final_depth_canonical_commits;
            auto& side = attribution.terminal_side_counts.at(
                static_cast<std::size_t>(bucket.side));
            ++side.canonical_commits;
            auto& kind = attribution.terminal_bucket_kind_counts.at(
                static_cast<std::size_t>(bucket.kind));
            ++kind.canonical_commits;
            const auto credit_duplicate =
                [&](ReforgeBuildAttribution::TerminalAggregate& aggregate) {
                    ++aggregate.duplicate_canonical_commits;
                    aggregate.duplicate_probability_mass += probability;
                    if (duplicate_within) {
                        ++aggregate.duplicates_within_predecessor;
                    }
                    if (duplicate_across) {
                        ++aggregate.duplicates_across_predecessors;
                    }
                };
            if (duplicate) {
                credit_duplicate(side);
                credit_duplicate(kind);
                ++attribution.final_depth_duplicate_canonical_commits;
                attribution.final_depth_duplicate_probability_mass +=
                    probability;
            }
            if (duplicate_within) {
                ++attribution
                      .final_depth_duplicates_within_predecessor;
                attribution
                    .final_depth_within_predecessor_duplicate_probability_mass +=
                    probability;
            }
            if (duplicate_across) {
                ++attribution
                      .final_depth_duplicates_across_predecessors;
                attribution
                    .final_depth_across_predecessor_duplicate_probability_mass +=
                    probability;
            }
            const auto credit_predecessor_observation =
                [&](const std::size_t index) {
                    auto& observed = attribution
                        .terminal_predecessor_observation_counts.at(index);
                    ++observed.canonical_commits;
                    if (duplicate) credit_duplicate(observed);
                };
            if (branch.predecessor.sat_mask != 0) {
                credit_predecessor_observation(0);
            }
            if (branch.predecessor.below_mask != 0) {
                credit_predecessor_observation(1);
            }
            bool predecessor_has_junk = false;
            bool predecessor_has_exclusion = false;
            for (std::uint8_t pick = 0;
                 pick < branch.predecessor.pick_count; ++pick) {
                const RollBucket& selected = buckets[
                    branch.predecessor.picks[pick].first];
                predecessor_has_junk |=
                    selected.kind == BucketKind::Junk;
                predecessor_has_exclusion |=
                    !selected.exclusion_groups.empty();
            }
            if (predecessor_has_junk) {
                credit_predecessor_observation(2);
            }
            if (predecessor_has_exclusion ||
                !bucket.exclusion_groups.empty()) {
                credit_predecessor_observation(3);
            }
            if (attribution.terminal_contribution_samples.size() >=
                kTerminalContributionSampleLimit) {
                ++attribution.terminal_contribution_samples_omitted;
                return;
            }
            ReforgeBuildAttribution::TerminalContributionSample sample;
            sample.predecessor_sequence =
                branch.predecessor_sequence;
            sample.predecessor_signature_hash =
                branch.predecessor_signature_hash;
            sample.predecessor_order_multiplicity =
                branch.predecessor_order_multiplicity;
            sample.predecessor_sat_mask = branch.predecessor.sat_mask;
            sample.predecessor_below_mask =
                branch.predecessor.below_mask;
            sample.predecessor_blocked_mask =
                branch.predecessor.blocked_mask;
            sample.predecessor_prefix_picks =
                branch.predecessor.prefix_picks;
            sample.predecessor_suffix_picks =
                branch.predecessor.suffix_picks;
            sample.predecessor_availability_class =
                branch.predecessor.availability_class;
            sample.target_count = branch.target_count;
            sample.terminal_bucket = branch.terminal_bucket;
            sample.terminal_side = bucket.side;
            sample.terminal_bucket_kind =
                static_cast<std::uint8_t>(bucket.kind);
            sample.terminal_goal_slot = bucket.slot;
            sample.terminal_junk_class = bucket.junk_class;
            sample.terminal_block_mask = bucket.block_mask;
            sample.terminal_bucket_multiplicity = bucket.multiplicity;
            sample.terminal_available_family_choices =
                branch.available_family_choices;
            sample.terminal_exclusion_group_count =
                static_cast<std::uint32_t>(
                    bucket.exclusion_groups.size());
            sample.terminal_exclusion_signature_hash =
                exclusion_signature_hash(bucket);
            sample.canonical_successor_state = state;
            sample.canonical_successor_hash =
                abstract_state_hash(states_.at(state));
            sample.duplicate_canonical_successor = duplicate;
            sample.duplicate_within_predecessor = duplicate_within;
            sample.duplicate_across_predecessors = duplicate_across;
            sample.different_terminal_bucket_from_first =
                different_terminal_bucket;
            sample.different_predecessor_observation_from_first =
                different_predecessor_observation;
            sample.probability_mass = probability;
            attribution.terminal_contribution_samples.push_back(
                std::move(sample));
        };
    const auto begin_terminal_branch =
        [&](const RollState& predecessor,
            const std::uint32_t predecessor_sequence,
            const std::uint64_t predecessor_order_multiplicity,
            const std::uint16_t bucket_index,
            const std::uint32_t available_family_choices,
            const int target,
            const double probability) {
            if (!capture_attribution) return;
            const RollBucket& bucket = buckets[bucket_index];
            ++attribution.final_depth_branches;
            attribution.final_depth_probability_mass += probability;
            attribution.final_depth_physical_modifier_choices =
                saturated_add(
                    attribution.final_depth_physical_modifier_choices,
                    available_family_choices);
            attribution.final_depth_represented_order_paths =
                saturated_add(
                    attribution.final_depth_represented_order_paths,
                    predecessor_order_multiplicity);
            attribution.final_depth_terminal_order_excess =
                saturated_add(
                    attribution.final_depth_terminal_order_excess,
                    predecessor_order_multiplicity > 0
                        ? predecessor_order_multiplicity - 1
                        : 0);
            auto& side = attribution.terminal_side_counts.at(
                static_cast<std::size_t>(bucket.side));
            ++side.branches;
            side.probability_mass += probability;
            auto& kind = attribution.terminal_bucket_kind_counts.at(
                static_cast<std::size_t>(bucket.kind));
            ++kind.branches;
            kind.probability_mass += probability;
            const auto record_observation =
                [&](const std::size_t index) {
                    auto& observed = attribution
                        .terminal_predecessor_observation_counts.at(index);
                    ++observed.branches;
                    observed.probability_mass += probability;
                };
            if (predecessor.sat_mask != 0) record_observation(0);
            if (predecessor.below_mask != 0) record_observation(1);
            bool predecessor_has_junk = false;
            bool predecessor_has_exclusion = false;
            for (std::uint8_t pick = 0;
                 pick < predecessor.pick_count; ++pick) {
                const RollBucket& selected =
                    buckets[predecessor.picks[pick].first];
                predecessor_has_junk |=
                    selected.kind == BucketKind::Junk;
                predecessor_has_exclusion |=
                    !selected.exclusion_groups.empty();
            }
            if (predecessor_has_junk) record_observation(2);
            if (predecessor_has_exclusion ||
                !bucket.exclusion_groups.empty()) {
                record_observation(3);
                ++attribution
                      .final_depth_branches_with_exclusion_observation;
            }
            record_target_contribution(target, probability, true);
            active_terminal_branch = ActiveTerminalBranch{
                predecessor,
                predecessor_sequence,
                roll_signature_hash(predecessor),
                predecessor_order_multiplicity,
                static_cast<std::uint8_t>(target),
                bucket_index,
                available_family_choices};
        };

    /* --- forward frontier DP ------------------------------------------------ */
    std::optional<std::uint32_t> gated_retry_state;
    const auto retry_state = [&]() {
        if (!gated_retry_state.has_value()) {
            AbstractState retry =
                project_item(session, layout_, retry_base);
            retry.goal_progress_retry_basin = 1;
            credit_effort(effort.state_interning_attempts);
            gated_retry_state = intern_state(retry);
            result.gated_retry_state = *gated_retry_state;
        }
        return *gated_retry_state;
    };
    const auto commit_retry = [&](
        const double weight,
        const bool count_terminal_contribution = true) {
        if (!(weight > 0.0)) return;
        if (count_terminal_contribution) {
            credit_effort(effort.terminal_contributions);
        }
        if (accumulate_outcome(retry_state(), weight)) {
            credit_effort(effort.duplicate_terminal_contributions);
        } else {
            credit_effort(effort.canonical_terminal_successors);
        }
    };
    const auto base_satisfied_count = [&]() {
        std::uint32_t count = 0;
        for (std::size_t slot = 0; slot < layout_.slots.size(); ++slot) {
            if (base_state.slot_status[slot] ==
                static_cast<std::uint8_t>(
                    GoalSlotStatus::Satisfied)) {
                ++count;
            }
        }
        return count;
    }();
    const auto roll_satisfied_count = [&](const RollState& roll) {
        return base_satisfied_count +
               static_cast<std::uint32_t>(
                   std::popcount(roll.sat_mask));
    };
    const auto roll_has_zero_progress = [&](const RollState& roll) {
        return roll_satisfied_count(roll) == 0;
    };
    const auto commit_successor =
        [&](AbstractState successor,
            const double weight,
            const bool zero_progress) {
        if (special_flags & PC_ITEM_CORRUPTED) {
            successor.flags |= kFlagCorrupted;
        }
        if (special_flags & PC_ITEM_MIRRORED) {
            successor.flags |= kFlagMirrored;
        }
        if (goal_progress_gated && is_goal_state(successor)) {
            if (result.gated_terminal_state == kNoId) {
                credit_effort(effort.state_interning_attempts);
                result.gated_terminal_state = intern_state(successor);
            }
            if (accumulate_outcome(
                    result.gated_terminal_state, weight)) {
                credit_effort(effort.duplicate_terminal_contributions);
            } else {
                credit_effort(effort.canonical_terminal_successors);
            }
            record_terminal_successor(
                result.gated_terminal_state, weight);
            return;
        }
        if (goal_progress_gated && zero_progress) {
            commit_retry(weight, false);
            return;
        }
        if (!veiled_reforge) {
            credit_effort(effort.state_interning_attempts);
            const std::uint32_t state = intern_state(successor);
            if (accumulate_outcome(state, weight)) {
                credit_effort(effort.duplicate_terminal_contributions);
            } else {
                credit_effort(effort.canonical_terminal_successors);
            }
            record_terminal_successor(state, weight);
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
            credit_effort(effort.state_interning_attempts);
            const std::uint32_t rolled_state = intern_state(successor);
            if (mod_id == kNoId || !materialize(rolled_state, concrete) ||
                pc_item_add_mod(
                    &concrete, side, mod_id,
                    static_cast<std::uint16_t>(
                        session.primary_group[mod_id]),
                    PC_MOD_SLOT_VEILED, nullptr) != PC_RESULT_OK) {
                const double branch_weight = weight * side_probability;
                if (accumulate_outcome(state_id, branch_weight)) {
                    credit_effort(
                        effort.duplicate_terminal_contributions);
                } else {
                    credit_effort(
                        effort.canonical_terminal_successors);
                }
                record_terminal_successor(state_id, branch_weight);
                state_dependent = true;
                return;
            }
            credit_effort(effort.state_interning_attempts);
            const std::uint32_t state = intern_item(concrete);
            const double branch_weight = weight * side_probability;
            if (accumulate_outcome(state, branch_weight)) {
                credit_effort(effort.duplicate_terminal_contributions);
            } else {
                credit_effort(effort.canonical_terminal_successors);
            }
            record_terminal_successor(state, branch_weight);
        };
        add_veiled(PC_SIDE_PREFIX, prefix_open);
        add_veiled(PC_SIDE_SUFFIX, suffix_open);
        if (!prefix_open && !suffix_open) {
            if (accumulate_outcome(state_id, weight)) {
                credit_effort(effort.duplicate_terminal_contributions);
            } else {
                credit_effort(effort.canonical_terminal_successors);
            }
            record_terminal_successor(state_id, weight);
            state_dependent = true;
        }
    };
    const auto commit_outcome = [&](const RollState& roll, double weight) {
        credit_effort(effort.terminal_contributions);
        if (capture_attribution) {
            ++attribution.terminal_roll_states;
            const std::size_t terminal_prefix =
                std::min<std::size_t>(
                    attribution.terminal_prefix_counts.size() - 1,
                    base.prefix_count + roll.prefix_picks);
            const std::size_t terminal_suffix =
                std::min<std::size_t>(
                    attribution.terminal_suffix_counts.size() - 1,
                    base.suffix_count + roll.suffix_picks);
            ++attribution.terminal_prefix_counts[terminal_prefix];
            ++attribution.terminal_suffix_counts[terminal_suffix];
        }
        const bool zero_progress = roll_has_zero_progress(roll);
        AbstractState successor = base_state;
        successor.blocked_mask |= roll.blocked_mask;
        for (std::size_t s = 0; s < layout_.slots.size(); ++s) {
            const std::uint8_t bit = 1u << s;
            if (roll.sat_mask & bit) {
                successor.slot_status[s] = static_cast<std::uint8_t>(
                    GoalSlotStatus::Satisfied);
                successor.goal_member_class_tokens[s] =
                    roll.goal_member_class_tokens[s];
            } else if ((roll.below_mask & bit) &&
                       successor.slot_status[s] !=
                           static_cast<std::uint8_t>(
                               GoalSlotStatus::Satisfied)) {
                successor.slot_status[s] = static_cast<std::uint8_t>(
                    GoalSlotStatus::PresentBelowTier);
                successor.goal_member_class_tokens[s] =
                    roll.goal_member_class_tokens[s];
            }
        }
        successor.prefix_count =
            static_cast<std::uint8_t>(base.prefix_count + roll.prefix_picks);
        successor.suffix_count =
            static_cast<std::uint8_t>(base.suffix_count + roll.suffix_picks);
        std::vector<std::uint16_t> deferred_picks;
        deferred_picks.reserve(roll.pick_count);
        for (std::uint8_t i = 0; i < roll.pick_count; ++i) {
            const std::uint16_t bucket_index = roll.picks[i].first;
            const RollBucket& bucket = buckets[bucket_index];
            if (!bucket.raw_choices.empty()) {
                if (roll.picks[i].second != 1) {
                    throw std::logic_error(
                        "identity-witness reforge selected one physical "
                        "family more than once");
                }
                deferred_picks.push_back(bucket_index);
                continue;
            }
            if (bucket.kind == BucketKind::Junk &&
                bucket.junk_class != kNoId) {
                successor.junk_counts[bucket.junk_class] = static_cast<
                    std::uint8_t>(
                    successor.junk_counts[bucket.junk_class] +
                    roll.picks[i].second);
            }
        }
        if (deferred_picks.empty()) {
            commit_successor(
                std::move(successor), weight, zero_progress);
            return;
        }

        /*
         * Expand raw identities only at absorption. Conditional identity
         * weights factor across picked physical families because choosing one
         * member disables exactly the same group vector and therefore cannot
         * affect any later denominator.
         */
        const auto expand =
            [&](auto&& self,
                const std::size_t pick,
                AbstractState expanded,
                const double probability) -> void {
                /*
                 * Count the complete expansion tree, not only emitted
                 * leaves. This bounds internal branching and successor-copy
                 * work under the same exact-kernel resource cap.
                 */
                consume_common_reforge_work(1);
                credit_effort(effort.identity_tree_nodes);
                if (capture_attribution) {
                    ++attribution.raw_identity_tree_nodes;
                    ++attribution.raw_identity_tree_work;
                }
                if (pick == deferred_picks.size()) {
                    if (capture_attribution) {
                        ++attribution.raw_identity_tree_leaves;
                    }
                    commit_successor(
                        std::move(expanded), probability, zero_progress);
                    return;
                }
                const std::uint16_t bucket_index =
                    deferred_picks[pick];
                const RollBucket& bucket = buckets[bucket_index];
                const bool guaranteed =
                    roll.guaranteed_bucket == bucket_index;
                const std::uint64_t total =
                    guaranteed ? bucket.guaranteed : bucket.weight;
                if (total == 0) {
                    throw std::logic_error(
                        "identity-witness reforge has an empty raw choice "
                        "channel");
                }
                for (const RollBucket::RawChoice& raw :
                     bucket.raw_choices) {
                    const std::uint64_t raw_weight =
                        guaranteed ? raw.guaranteed : raw.weight;
                    if (raw_weight == 0) continue;
                    AbstractState child = expanded;
                    if (bucket.kind == BucketKind::GoalSat ||
                        bucket.kind == BucketKind::GoalBelow) {
                        if (raw.goal_member_class_token == 0) {
                            throw std::logic_error(
                                "identity-witness goal choice has no exact "
                                "member token");
                        }
                        child.goal_member_class_tokens[bucket.slot] =
                            raw.goal_member_class_token;
                    } else {
                        if (raw.junk_class == kNoId) {
                            throw std::logic_error(
                                "identity-witness junk choice has no exact "
                                "class");
                        }
                        child.junk_counts[raw.junk_class] =
                            static_cast<std::uint8_t>(
                                child.junk_counts[raw.junk_class] + 1);
                    }
                    self(
                        self, pick + 1, std::move(child),
                        probability *
                            (static_cast<double>(raw_weight) /
                             static_cast<double>(total)));
                }
            };
        expand(expand, 0, std::move(successor), weight);
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

    /*
     * Projected V2 availability is an exact structural signature, not a
     * heuristic filter. Each interned bit survives precisely when the bucket
     * has positive natural weight, an open side, an unoccupied observation,
     * an unexhausted multiplicity, and no group conflict with a prior pick.
     * Hash buckets are collision checked against the complete bit vector.
     */
    std::vector<std::vector<std::uint64_t>> availability_classes;
    std::unordered_map<std::uint64_t, std::vector<std::uint32_t>>
        availability_ids_by_hash;
    const auto availability_storage_bytes =
        [&](const std::size_t extra_classes = 0) {
            std::uint64_t bytes = saturated_bytes(
                availability_classes.size() + extra_classes,
                sizeof(decltype(availability_classes)::value_type));
            std::size_t word_capacity = 0;
            for (const auto& words : availability_classes) {
                word_capacity = saturated_add(
                    word_capacity, words.capacity());
            }
            word_capacity = saturated_add(
                word_capacity,
                extra_classes * availability_word_count);
            bytes = saturated_add(
                bytes,
                saturated_bytes(word_capacity, sizeof(std::uint64_t)));
            bytes = saturated_add(
                bytes,
                unordered_storage_bytes(
                    availability_classes.size() + extra_classes,
                    sizeof(decltype(
                        availability_ids_by_hash)::value_type)));
            std::size_t id_capacity = 0;
            for (const auto& [unused, ids] :
                 availability_ids_by_hash) {
                (void)unused;
                id_capacity = saturated_add(id_capacity, ids.capacity());
            }
            id_capacity = saturated_add(id_capacity, extra_classes);
            return saturated_add(
                bytes,
                saturated_bytes(id_capacity, sizeof(std::uint32_t)));
        };
    const auto require_availability_insert = [&]() {
        require_reforge_scratch_bytes(
            saturated_add(
                saturated_add(
                    saturated_add(
                        saturated_add(
                            stationary_reforge_scratch_bytes,
                            active_frontier_scratch_bytes),
                        availability_storage_bytes(1)),
                    saturated_add(
                        terminal_attribution_scratch_bytes,
                        factored_terminal_scratch_bytes)),
                unordered_storage_bytes(
                    outcome_preflight_entries,
                    sizeof(decltype(outcome_acc)::value_type))));
    };
    const auto intern_availability =
        [&](std::vector<std::uint64_t> words) {
            std::uint64_t hash = 1469598103934665603ull;
            for (const std::uint64_t word : words) {
                hash ^= word;
                hash *= 1099511628211ull;
            }
            const auto found = availability_ids_by_hash.find(hash);
            if (found != availability_ids_by_hash.end()) {
                for (const std::uint32_t id : found->second) {
                    if (availability_classes[id] == words) return id;
                }
            }
            if (availability_classes.size() >=
                std::numeric_limits<std::uint32_t>::max()) {
                throw SolverResourceLimit(
                    "max_reforge_availability_classes",
                    std::numeric_limits<std::uint32_t>::max());
            }
            require_availability_insert();
            const std::uint32_t id = static_cast<std::uint32_t>(
                availability_classes.size());
            const std::size_t word_count = words.size();
            availability_classes.push_back(std::move(words));
            availability_ids_by_hash[hash].push_back(id);
            credit_effort(effort.availability_classes_built);
            credit_effort(
                effort.availability_words_built, word_count);
            availability_scratch_bytes =
                availability_storage_bytes();
            return id;
        };
    std::uint32_t root_availability_class = kNoId;
    if (use_projected_reforge_frontier_) {
        std::vector<std::uint64_t> root_words(
            availability_word_count, 0);
        for (std::uint16_t b = 0;
             b < static_cast<std::uint16_t>(bucket_count); ++b) {
            const RollBucket& bucket = buckets[b];
            if (bucket.weight == 0) continue;
            const std::uint8_t side_count =
                bucket.side == PC_SIDE_PREFIX
                    ? base.prefix_count
                    : base.suffix_count;
            if (side_count >= cap) continue;
            if (bucket.kind != BucketKind::Junk) {
                if ((base_occupied & (1u << bucket.slot)) != 0) continue;
            } else if ((bucket.block_mask & base_occupied) != 0) {
                continue;
            }
            root_words[b / 64] |= std::uint64_t{1} << (b % 64);
        }
        root_availability_class =
            intern_availability(std::move(root_words));
    }
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
        std::uint32_t used = roll.picks_of(b);
        if (bucket.interchangeable_family_class) {
            used = 0;
            for (std::uint8_t pick = 0; pick < roll.pick_count; ++pick) {
                if (buckets[roll.picks[pick].first].family_class ==
                    bucket.family_class) {
                    used += roll.picks[pick].second;
                }
            }
        }
        if (used >= bucket.multiplicity) return 0.0;
        return static_cast<double>(bucket.weight) *
               (bucket.multiplicity - used);
    };
    const auto projected_bucket_remaining =
        [&](const RollState& roll, const std::uint16_t b) -> double {
            const RollBucket& bucket = buckets[b];
            std::uint32_t used = roll.picks_of(b);
            if (bucket.interchangeable_family_class) {
                used = 0;
                for (std::uint8_t pick = 0;
                     pick < roll.pick_count; ++pick) {
                    if (buckets[roll.picks[pick].first].family_class ==
                        bucket.family_class) {
                        used += roll.picks[pick].second;
                    }
                }
            }
            if (bucket.weight == 0 || used >= bucket.multiplicity) {
                throw std::logic_error(
                    "projected reforge availability retained an empty "
                    "bucket");
            }
            return static_cast<double>(bucket.weight) *
                   (bucket.multiplicity - used);
        };
    const auto available_family_choices =
        [&](const RollState& roll, const std::uint16_t b) {
            const RollBucket& bucket = buckets[b];
            std::uint32_t used = roll.picks_of(b);
            if (bucket.interchangeable_family_class) {
                used = 0;
                for (std::uint8_t pick = 0;
                     pick < roll.pick_count; ++pick) {
                    if (buckets[roll.picks[pick].first].family_class ==
                        bucket.family_class) {
                        used += roll.picks[pick].second;
                    }
                }
            }
            return used < bucket.multiplicity
                       ? bucket.multiplicity - used
                       : 0;
        };
    const auto exact_projected_bucket_remaining =
        [&](const RollState& roll, const std::uint16_t b) {
            const std::uint64_t choices =
                available_family_choices(roll, b);
            const std::uint64_t weight = buckets[b].weight;
            if (choices == 0 || weight == 0) return std::uint64_t{0};
            if (weight >
                std::numeric_limits<std::uint64_t>::max() / choices) {
                throw std::logic_error(
                    "reforge terminal exact weight overflow");
            }
            return weight * choices;
        };

    std::unordered_map<RollState, double, RollStateHash> frontier;
    std::unordered_map<RollState, std::uint64_t, RollStateHash>
        frontier_order_multiplicity;
    std::vector<std::pair<RollState, double>> ordered_frontier;
    std::vector<double> remaining_by_bucket(bucket_count, 0.0);
    std::vector<std::uint16_t> eligible_buckets;
    eligible_buckets.reserve(bucket_count);
    std::size_t frontier_preflight_entries =
        std::max<std::size_t>(
            1, harvest ? bucket_count : 1);
    active_frontier_scratch_bytes =
        unordered_storage_bytes(
            frontier_preflight_entries,
            sizeof(decltype(frontier)::value_type));
    if (capture_attribution) {
        active_frontier_scratch_bytes = saturated_add(
            active_frontier_scratch_bytes,
            unordered_storage_bytes(
                frontier_preflight_entries,
                sizeof(decltype(
                    frontier_order_multiplicity)::value_type)));
    }
    require_reforge_scratch_bytes(
        saturated_add(
            saturated_add(
                saturated_add(
                    saturated_add(
                        stationary_reforge_scratch_bytes,
                        active_frontier_scratch_bytes),
                    availability_scratch_bytes),
                saturated_add(
                    terminal_attribution_scratch_bytes,
                    factored_terminal_scratch_bytes)),
            unordered_storage_bytes(
                outcome_preflight_entries,
                sizeof(decltype(outcome_acc)::value_type))));
    frontier.reserve(frontier_preflight_entries);
    if (capture_attribution) {
        frontier_order_multiplicity.reserve(
            frontier_preflight_entries);
    }
    const auto frontier_started = std::chrono::steady_clock::now();
    const auto add_bucket_pick =
        [&](const RollState& roll,
            const std::uint16_t b,
            const bool guaranteed_pick = false) {
        const RollBucket& bucket = buckets[b];
        RollState child = roll;
        const std::uint8_t parent_occupied =
            occupied_mask(roll, base_occupied);
        if (bucket.side == 0) {
            ++child.prefix_picks;
        } else {
            ++child.suffix_picks;
        }
        child.add_pick(b);
        if (guaranteed_pick && !bucket.raw_choices.empty()) {
            child.guaranteed_bucket = b;
        }
        if (bucket.kind == BucketKind::GoalSat) {
            child.sat_mask |= 1u << bucket.slot;
            child.goal_member_class_tokens[bucket.slot] =
                bucket.goal_member_class_token;
        } else if (bucket.kind == BucketKind::GoalBelow) {
            child.below_mask |= 1u << bucket.slot;
            child.goal_member_class_tokens[bucket.slot] =
                bucket.goal_member_class_token;
        } else {
            child.blocked_mask |= bucket.block_mask;
        }
        if (use_projected_reforge_frontier_) {
            if (roll.availability_class == kNoId ||
                roll.availability_class >= availability_classes.size()) {
                throw std::logic_error(
                    "projected reforge state has no availability class");
            }
            std::vector<std::uint64_t> words =
                availability_classes[roll.availability_class];
            const auto clear_mask =
                [&](const std::vector<std::uint64_t>& mask) {
                    for (std::size_t word = 0;
                         word < words.size(); ++word) {
                        words[word] &= ~mask[word];
                    }
                };
            clear_mask(bucket_conflict_masks[b]);
            std::uint32_t used = child.picks_of(b);
            if (bucket.interchangeable_family_class) {
                used = 0;
                for (std::uint8_t pick = 0;
                     pick < child.pick_count; ++pick) {
                    if (buckets[child.picks[pick].first].family_class ==
                        bucket.family_class) {
                        used += child.picks[pick].second;
                    }
                }
            }
            if (used >= bucket.multiplicity) {
                if (bucket.interchangeable_family_class) {
                    for (const std::uint16_t member :
                         family_class_buckets[bucket.family_class]) {
                        words[member / 64] &=
                            ~(std::uint64_t{1} << (member % 64));
                    }
                } else {
                    words[b / 64] &=
                        ~(std::uint64_t{1} << (b % 64));
                }
            }
            const std::uint8_t child_side_count =
                bucket.side == PC_SIDE_PREFIX
                    ? static_cast<std::uint8_t>(
                          base.prefix_count + child.prefix_picks)
                    : static_cast<std::uint8_t>(
                          base.suffix_count + child.suffix_picks);
            if (child_side_count >= cap) {
                clear_mask(side_bucket_masks[
                    static_cast<std::size_t>(bucket.side)]);
            }
            const std::uint8_t child_occupied =
                occupied_mask(child, base_occupied);
            const std::uint8_t newly_occupied =
                static_cast<std::uint8_t>(
                    child_occupied & ~parent_occupied);
            for (std::size_t slot = 0;
                 slot < layout_.slots.size(); ++slot) {
                if ((newly_occupied & (1u << slot)) != 0) {
                    clear_mask(occupied_bucket_masks[slot]);
                }
            }
            child.availability_class =
                intern_availability(std::move(words));
        }
        return child;
    };
    if (!harvest) {
        RollState root;
        root.availability_class = root_availability_class;
        const auto inserted = frontier.emplace(root, 1.0);
        if (capture_attribution) {
            frontier_order_multiplicity.emplace(
                inserted.first->first, 1);
        }
    } else {
        /* Guaranteed first pick from the tag-targeted natural pool.
         * An empty pool means the engine action does not apply. */
        RollState root;
        root.availability_class = root_availability_class;
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
        consume_common_reforge_work(buckets.size());
        /* The historic ledger charges one bucket scan, while control flow
         * traverses guaranteed support once for its denominator and again
         * to publish branches. Preserve the legacy charge and report both
         * physical passes here. */
        credit_effort(effort.dense_bucket_probes, buckets.size());
        credit_effort(effort.dense_bucket_probes, buckets.size());
        attribution.guaranteed_scan_work += buckets.size();
        for (std::uint16_t b = 0;
             b < static_cast<std::uint16_t>(buckets.size()); ++b) {
            total += guaranteed_remaining(b);
        }
        if (total <= 0.0) return unapplied();
        const auto add_guaranteed_branch =
            [&](const std::uint16_t b) {
                const double remaining = guaranteed_remaining(b);
                if (remaining <= 0.0) return;
                RollState child =
                    add_bucket_pick(root, b, defer_identity_choice);
                auto [found, inserted] =
                    frontier.try_emplace(child, 0.0);
                (void)inserted;
                found->second += remaining / total;
                if (capture_attribution) {
                    auto [orders, unused] =
                        frontier_order_multiplicity.try_emplace(
                            child, 0);
                    (void)unused;
                    orders->second = saturated_add(
                        orders->second, 1);
                }
            };
        if (reverse_reforge_bucket_enumeration_) {
            for (std::size_t index = buckets.size(); index > 0; --index) {
                add_guaranteed_branch(
                    static_cast<std::uint16_t>(index - 1));
            }
        } else {
            for (std::uint16_t b = 0;
                 b < static_cast<std::uint16_t>(buckets.size()); ++b) {
                add_guaranteed_branch(b);
            }
        }
    }
    for (int depth = first_depth; depth <= max_target; ++depth) {
        const auto exact = targets.find(depth);
        const double stop_here =
            exact != targets.end() ? exact->second : 0.0;
        const double deeper = target_suffix(depth);
        std::unordered_map<RollState, double, RollStateHash> next;
        std::unordered_map<RollState, std::uint64_t, RollStateHash>
            next_order_multiplicity;
        std::unordered_map<
            TerminalPickKey,
            FactoredTerminalPredecessor,
            TerminalPickKeyHash>
            factored_terminal_predecessors;
        std::vector<FactoredTerminalCandidate>
            factored_terminal_candidates;
        std::size_t factored_predecessor_preflight_entries = 0;
        std::size_t factored_candidate_preflight_entries = 0;
        const bool use_factored_terminal_depth =
            use_factored_terminal_reforge_ && goal_progress_gated &&
            depth + 1 == max_target;
        if (frontier.size() >
            std::numeric_limits<std::size_t>::max() / 2) {
            require_reforge_scratch_bytes(
                std::numeric_limits<std::uint64_t>::max());
            throw std::length_error(
                "reforge frontier reserve overflow");
        }
        std::size_t next_preflight_entries =
            std::max<std::size_t>(1, frontier.size() * 2);
        const std::size_t ordered_preflight_entries =
            std::max(
                ordered_frontier.capacity(), frontier.size());
        const auto update_frontier_preflight =
            [&]() {
                active_frontier_scratch_bytes =
                    saturated_add(
                        saturated_add(
                            unordered_storage_bytes(
                                frontier_preflight_entries,
                                sizeof(decltype(frontier)::value_type)),
                            unordered_storage_bytes(
                                next_preflight_entries,
                                sizeof(decltype(next)::value_type))),
                        saturated_bytes(
                            ordered_preflight_entries,
                            sizeof(decltype(
                                ordered_frontier)::value_type)));
                if (capture_attribution) {
                    active_frontier_scratch_bytes = saturated_add(
                        active_frontier_scratch_bytes,
                        saturated_add(
                            unordered_storage_bytes(
                                frontier_preflight_entries,
                                sizeof(decltype(
                                    frontier_order_multiplicity)::value_type)),
                            unordered_storage_bytes(
                                next_preflight_entries,
                                sizeof(decltype(
                                    next_order_multiplicity)::value_type))));
                }
                require_reforge_scratch_bytes(
                    saturated_add(
                        saturated_add(
                            saturated_add(
                                saturated_add(
                                    stationary_reforge_scratch_bytes,
                                    active_frontier_scratch_bytes),
                                availability_scratch_bytes),
                            saturated_add(
                                terminal_attribution_scratch_bytes,
                                factored_terminal_scratch_bytes)),
                        unordered_storage_bytes(
                            outcome_preflight_entries,
                            sizeof(decltype(outcome_acc)::value_type))));
            };
        update_frontier_preflight();
        next.reserve(next_preflight_entries);
        if (capture_attribution) {
            next_order_multiplicity.reserve(
                next_preflight_entries);
        }
        ordered_frontier.reserve(
            ordered_preflight_entries);
        ordered_frontier.assign(frontier.begin(), frontier.end());
        if (capture_attribution) {
            attribution.maximum_frontier_states =
                std::max<std::uint64_t>(
                    attribution.maximum_frontier_states,
                    ordered_frontier.size());
        }
        std::sort(
            ordered_frontier.begin(), ordered_frontier.end(),
            [](const auto& left, const auto& right) {
                return left.first < right.first;
            });
        const auto update_factored_terminal_preflight = [&]() {
            factored_terminal_scratch_bytes = saturated_add(
                unordered_storage_bytes(
                    factored_predecessor_preflight_entries,
                    sizeof(decltype(
                        factored_terminal_predecessors)::value_type)),
                saturated_bytes(
                    factored_candidate_preflight_entries,
                    sizeof(FactoredTerminalCandidate)));
            require_reforge_scratch_bytes(
                saturated_add(
                    saturated_add(
                        saturated_add(
                            saturated_add(
                                stationary_reforge_scratch_bytes,
                                active_frontier_scratch_bytes),
                            availability_scratch_bytes),
                        saturated_add(
                            terminal_attribution_scratch_bytes,
                            factored_terminal_scratch_bytes)),
                    unordered_storage_bytes(
                        outcome_preflight_entries,
                        sizeof(decltype(outcome_acc)::value_type))));
        };
        if (use_factored_terminal_depth) {
            factored_predecessor_preflight_entries =
                std::max<std::size_t>(1, frontier.size());
            factored_candidate_preflight_entries =
                std::max<std::size_t>(1, frontier.size() * 2);
            update_factored_terminal_preflight();
            factored_terminal_predecessors.reserve(
                factored_predecessor_preflight_entries);
            factored_terminal_candidates.reserve(
                factored_candidate_preflight_entries);
            for (const auto& [roll, unused_probability] :
                 ordered_frontier) {
                (void)unused_probability;
                const TerminalPickKey key = terminal_pick_key(roll);
                auto [found, inserted] =
                    factored_terminal_predecessors.try_emplace(
                        key,
                        FactoredTerminalPredecessor{roll, 0.0, 0});
                if (!inserted && !(found->second.roll == roll)) {
                    if (capture_attribution) {
                        ++attribution
                              .factored_subset_identity_mismatches;
                    }
                    throw std::logic_error(
                        "reforge factored terminal preflight identity "
                        "collision");
                }
            }
            telemetry_.reforge_factored_work = saturated_add(
                telemetry_.reforge_factored_work,
                factored_terminal_predecessors.size());
            credit_effort(
                effort.v3_predecessor_index_entries,
                factored_terminal_predecessors.size());
            consume_reforge_work(
                factored_terminal_predecessors.size(), 0);
            if (capture_attribution) {
                attribution.frontier_work = saturated_add(
                    attribution.frontier_work,
                    factored_terminal_predecessors.size());
            }
        }
        std::uint32_t predecessor_sequence = 0;
        for (const auto& [roll, probability] : ordered_frontier) {
            credit_effort(effort.frontier_nodes);
            const std::uint32_t current_predecessor_sequence =
                predecessor_sequence++;
            const std::uint64_t predecessor_order_multiplicity =
                capture_attribution
                    ? frontier_order_multiplicity.at(roll)
                    : 1;
            const std::uint64_t raw_node_work = 1 + buckets.size();
            telemetry_.reforge_raw_equivalent_work = saturated_add(
                telemetry_.reforge_raw_equivalent_work,
                raw_node_work);
            telemetry_.reforge_projected_work = saturated_add(
                telemetry_.reforge_projected_work, 1);
            telemetry_.reforge_factored_work = saturated_add(
                telemetry_.reforge_factored_work, 1);
            const std::uint64_t active_node_work =
                use_projected_reforge_frontier_ ? 1 : raw_node_work;
            consume_reforge_work(active_node_work, raw_node_work);
            if (capture_attribution) {
                ++attribution.frontier_state_visits;
                attribution.frontier_work +=
                    active_node_work;
            }
            /* Exact explicit-affix goals cannot short-circuit a reforge at an
             * intermediate roll depth. A deeper pick may be another requested
             * goal or may be junk, and those outcomes no longer share terminal
             * status. Commit only at the action's real target depths and let
             * is_goal_state classify the finished carrier. */
            if (stop_here > 0.0) {
                record_target_contribution(
                    depth, probability * stop_here, false);
                commit_outcome(roll, probability * stop_here);
            }
            if (deeper <= 0.0) continue;
            const std::uint8_t occupied = occupied_mask(roll, base_occupied);
            double total = 0.0;
            eligible_buckets.clear();
            if (use_projected_reforge_frontier_) {
                if (roll.availability_class == kNoId ||
                    roll.availability_class >=
                        availability_classes.size()) {
                    throw std::logic_error(
                        "projected reforge frontier lost availability");
                }
                const auto& words =
                    availability_classes[roll.availability_class];
                const std::uint64_t word_work = words.size();
                credit_effort(
                    effort.availability_words_scanned, word_work);
                telemetry_.reforge_projected_work = saturated_add(
                    telemetry_.reforge_projected_work, word_work);
                telemetry_.reforge_factored_work = saturated_add(
                    telemetry_.reforge_factored_work, word_work);
                consume_reforge_work(word_work, 0);
                if (capture_attribution) {
                    attribution.frontier_work += word_work;
                }
                for (std::size_t word_index = 0;
                     word_index < words.size(); ++word_index) {
                    std::uint64_t word = words[word_index];
                    while (word != 0) {
                        const unsigned bit = std::countr_zero(word);
                        const std::size_t candidate =
                            word_index * 64 + bit;
                        if (candidate >= bucket_count) break;
                        const std::uint16_t b =
                            static_cast<std::uint16_t>(candidate);
                        remaining_by_bucket[b] =
                            projected_bucket_remaining(roll, b);
                        total += remaining_by_bucket[b];
                        eligible_buckets.push_back(b);
                        word &= word - 1;
                    }
                }
            } else {
                credit_effort(
                    effort.dense_bucket_probes, buckets.size());
                for (std::uint16_t b = 0;
                     b < static_cast<std::uint16_t>(buckets.size()); ++b) {
                    remaining_by_bucket[b] =
                        bucket_remaining(roll, b, occupied);
                    total += remaining_by_bucket[b];
                    if (remaining_by_bucket[b] > 0.0) {
                        eligible_buckets.push_back(b);
                    }
                }
                telemetry_.reforge_projected_work = saturated_add(
                    telemetry_.reforge_projected_work,
                    availability_word_count);
                telemetry_.reforge_factored_work = saturated_add(
                    telemetry_.reforge_factored_work,
                    availability_word_count);
            }
            if (reverse_reforge_bucket_enumeration_) {
                std::reverse(
                    eligible_buckets.begin(),
                    eligible_buckets.end());
            }
            const std::uint64_t edge_work = eligible_buckets.size();
            telemetry_.reforge_projected_work = saturated_add(
                telemetry_.reforge_projected_work, edge_work);
            const bool factored_final_depth =
                use_factored_terminal_reforge_ &&
                goal_progress_gated && depth + 1 == max_target;
            if (!factored_final_depth) {
                telemetry_.reforge_factored_work = saturated_add(
                    telemetry_.reforge_factored_work, edge_work);
            }
            if (use_projected_reforge_frontier_ &&
                !factored_final_depth) {
                consume_reforge_work(edge_work, 0);
                if (capture_attribution) {
                    attribution.frontier_work += edge_work;
                }
            }
            if (capture_attribution) {
                attribution.frontier_edges += edge_work;
            }
            if (goal_progress_gated &&
                roll_has_zero_progress(roll)) {
                bool can_make_progress = false;
                for (const std::uint16_t b : eligible_buckets) {
                    can_make_progress |=
                        buckets[b].kind == BucketKind::GoalSat;
                }
                if (!can_make_progress) {
                    if (capture_attribution) {
                        for (const auto& [target, target_probability] :
                             targets) {
                            if (target > depth) {
                                record_target_contribution(
                                    target,
                                    probability * target_probability,
                                    false);
                            }
                        }
                    }
                    commit_retry(probability * deeper);
                    ++result.gated_retry_short_circuits;
                    continue;
                }
            }
            if (total <= 0.0) {
                if (capture_attribution) {
                    for (const auto& [target, target_probability] :
                         targets) {
                        if (target > depth) {
                            record_target_contribution(
                                target,
                                probability * target_probability,
                                false);
                        }
                    }
                }
                commit_outcome(roll, probability * deeper);
                continue;
            }
            if (goal_progress_gated && depth + 1 == max_target) {
                if (capture_attribution && !eligible_buckets.empty()) {
                    ++attribution.final_depth_predecessors;
                    attribution
                        .final_depth_max_predecessor_order_multiplicity =
                        std::max(
                            attribution
                                .final_depth_max_predecessor_order_multiplicity,
                            predecessor_order_multiplicity);
                    if (predecessor_order_multiplicity > 1) {
                        ++attribution
                              .final_depth_predecessors_with_multiple_orders;
                        attribution.final_depth_predecessor_order_excess =
                            saturated_add(
                                attribution
                                    .final_depth_predecessor_order_excess,
                                predecessor_order_multiplicity - 1);
                    }
                }
                if (use_factored_terminal_depth) {
                    std::uint64_t exact_denominator = 0;
                    std::uint64_t exact_retry_numerator = 0;
                    std::uint64_t canonical_subset_checks = 0;
                    const bool zero_progress =
                        roll_has_zero_progress(roll);
                    telemetry_.reforge_factored_work = saturated_add(
                        telemetry_.reforge_factored_work,
                        eligible_buckets.size());
                    consume_reforge_work(eligible_buckets.size(), 0);
                    if (capture_attribution) {
                        attribution.frontier_work = saturated_add(
                            attribution.frontier_work,
                            eligible_buckets.size());
                        ++attribution.factored_terminal_predecessors;
                    }
                    for (const std::uint16_t b : eligible_buckets) {
                        credit_effort(effort.v3_denominator_edges);
                        const std::uint64_t remaining =
                            exact_projected_bucket_remaining(roll, b);
                        if (remaining >
                            std::numeric_limits<std::uint64_t>::max() -
                                exact_denominator) {
                            throw std::logic_error(
                                "reforge terminal denominator overflow");
                        }
                        exact_denominator += remaining;
                        const bool retry_branch =
                            zero_progress &&
                            buckets[b].kind != BucketKind::GoalSat;
                        if (retry_branch) {
                            if (remaining >
                                std::numeric_limits<std::uint64_t>::max() -
                                    exact_retry_numerator) {
                                throw std::logic_error(
                                    "reforge terminal retry numerator "
                                    "overflow");
                            }
                            exact_retry_numerator += remaining;
                            continue;
                        }
                        TerminalPickKey completed_key =
                            terminal_pick_key(roll);
                        add_terminal_pick(completed_key, b);
                        bool canonical_live_predecessor = false;
                        bool found_live_predecessor = false;
                        for (std::size_t reverse =
                                 completed_key.pick_count;
                             reverse > 0; --reverse) {
                            const std::uint16_t removed =
                                completed_key.picks[reverse - 1].first;
                            TerminalPickKey subset = completed_key;
                            if (!remove_terminal_pick(subset, removed)) {
                                continue;
                            }
                            ++canonical_subset_checks;
                            credit_effort(effort.v3_subset_checks);
                            const auto live =
                                factored_terminal_predecessors.find(subset);
                            if (live ==
                                factored_terminal_predecessors.end()) {
                                continue;
                            }
                            if (roll_has_zero_progress(
                                    live->second.roll) &&
                                buckets[removed].kind !=
                                    BucketKind::GoalSat) {
                                continue;
                            }
                            found_live_predecessor = true;
                            canonical_live_predecessor =
                                removed == b &&
                                subset == terminal_pick_key(roll);
                            break;
                        }
                        if (!found_live_predecessor) {
                            throw std::logic_error(
                                "reforge factored terminal completed set "
                                "has no live predecessor");
                        }
                        if (!canonical_live_predecessor) continue;
                        const std::size_t required =
                            factored_terminal_candidates.size() + 1;
                        if (required >
                            factored_candidate_preflight_entries) {
                            const std::size_t doubled =
                                factored_candidate_preflight_entries >
                                        std::numeric_limits<
                                            std::size_t>::max() /
                                            2
                                    ? std::numeric_limits<
                                          std::size_t>::max()
                                    : factored_candidate_preflight_entries *
                                          2;
                            factored_candidate_preflight_entries =
                                std::max(required, doubled);
                            update_factored_terminal_preflight();
                            factored_terminal_candidates.reserve(
                                factored_candidate_preflight_entries);
                        }
                        factored_terminal_candidates.push_back(
                            {add_bucket_pick(roll, b)});
                        credit_effort(effort.v3_candidate_sets);
                        telemetry_.reforge_factored_work = saturated_add(
                            telemetry_.reforge_factored_work, 1);
                        consume_reforge_work(1, 0);
                        if (capture_attribution) {
                            ++attribution.factored_terminal_candidates;
                            ++attribution.frontier_work;
                        }
                    }
                    telemetry_.reforge_factored_work = saturated_add(
                        telemetry_.reforge_factored_work,
                        canonical_subset_checks);
                    consume_reforge_work(canonical_subset_checks, 0);
                    if (capture_attribution) {
                        attribution.factored_canonical_subset_checks =
                            saturated_add(
                                attribution
                                    .factored_canonical_subset_checks,
                                canonical_subset_checks);
                        attribution.frontier_work = saturated_add(
                            attribution.frontier_work,
                            canonical_subset_checks);
                    }
                    if (exact_denominator == 0) {
                        throw std::logic_error(
                            "reforge factored terminal predecessor has an "
                            "empty denominator");
                    }
                    const TerminalPickKey key = terminal_pick_key(roll);
                    const auto found =
                        factored_terminal_predecessors.find(key);
                    if (found ==
                            factored_terminal_predecessors.end() ||
                        !(found->second.roll == roll)) {
                        if (capture_attribution) {
                            ++attribution
                                  .factored_subset_identity_mismatches;
                        }
                        throw std::logic_error(
                            "reforge factored terminal subset identity "
                            "collision");
                    }
                    if (found->second.exact_denominator != 0) {
                        throw std::logic_error(
                            "reforge factored terminal predecessor was "
                            "initialized twice");
                    }
                    found->second.scaled_probability =
                        probability * deeper;
                    found->second.exact_denominator = exact_denominator;
                    if (exact_retry_numerator != 0) {
                        const long double retry_ratio =
                            static_cast<long double>(
                                exact_retry_numerator) /
                            static_cast<long double>(exact_denominator);
                        commit_retry(static_cast<double>(
                            static_cast<long double>(probability * deeper) *
                            retry_ratio));
                        telemetry_.reforge_factored_work = saturated_add(
                            telemetry_.reforge_factored_work, 1);
                        consume_reforge_work(1, 0);
                        ++result.gated_retry_short_circuits;
                        if (capture_attribution) {
                            ++attribution.factored_retry_aggregates;
                            ++attribution.frontier_work;
                        }
                    }
                    continue;
                }
                double retry_weight = 0.0;
                for (const std::uint16_t b : eligible_buckets) {
                    const double remaining = remaining_by_bucket[b];
                    const double branch_weight =
                        probability * deeper * (remaining / total);
                    begin_terminal_branch(
                        roll,
                        current_predecessor_sequence,
                        predecessor_order_multiplicity,
                        b,
                        available_family_choices(roll, b),
                        max_target,
                        branch_weight);
                    if (roll_has_zero_progress(roll) &&
                        buckets[b].kind != BucketKind::GoalSat) {
                        record_terminal_successor(
                            retry_state(), branch_weight);
                        active_terminal_branch.reset();
                        retry_weight += branch_weight;
                        continue;
                    }
                    commit_outcome(
                        add_bucket_pick(roll, b), branch_weight);
                    active_terminal_branch.reset();
                }
                if (retry_weight > 0.0) {
                    commit_retry(retry_weight);
                    ++result.gated_retry_short_circuits;
                }
                continue;
            }
            for (const std::uint16_t b : eligible_buckets) {
                credit_effort(effort.eligible_nonterminal_edges);
                const double remaining = remaining_by_bucket[b];
                const double p = probability * (remaining / total);
                RollState child = add_bucket_pick(roll, b);
                if (capture_attribution) {
                    auto [orders, unused] =
                        next_order_multiplicity.try_emplace(child, 0);
                    (void)unused;
                    orders->second = saturated_add(
                        orders->second,
                        predecessor_order_multiplicity);
                }
                const auto existing = next.find(child);
                if (existing != next.end()) {
                    existing->second += p;
                    continue;
                }
                const std::size_t required = next.size() + 1;
                if (required > next_preflight_entries) {
                    const std::size_t doubled =
                        next_preflight_entries >
                                std::numeric_limits<std::size_t>::max() / 2
                            ? std::numeric_limits<std::size_t>::max()
                            : next_preflight_entries * 2;
                    next_preflight_entries =
                        std::max(required, doubled);
                    update_frontier_preflight();
                    next.reserve(next_preflight_entries);
                    if (capture_attribution) {
                        next_order_multiplicity.reserve(
                            next_preflight_entries);
                    }
                }
                next.emplace(std::move(child), p);
            }
        }
        if (use_factored_terminal_depth) {
            std::sort(
                factored_terminal_candidates.begin(),
                factored_terminal_candidates.end());
            for (std::size_t index = 1;
                 index < factored_terminal_candidates.size(); ++index) {
                if (factored_terminal_candidates[index - 1].completed ==
                    factored_terminal_candidates[index].completed) {
                    throw std::logic_error(
                        "reforge factored terminal canonical generation "
                        "published one completed set twice");
                }
            }
            for (const FactoredTerminalCandidate& candidate :
                 factored_terminal_candidates) {
                long double probability_mass = 0.0L;
                std::uint64_t recurrence_terms = 0;
                for (std::uint8_t pick = 0;
                     pick < candidate.completed.pick_count; ++pick) {
                    const std::uint16_t b =
                        candidate.completed.picks[pick].first;
                    TerminalPickKey predecessor_key =
                        terminal_pick_key(candidate.completed);
                    if (!remove_terminal_pick(predecessor_key, b)) {
                        continue;
                    }
                    const auto found =
                        factored_terminal_predecessors.find(
                            predecessor_key);
                    if (found == factored_terminal_predecessors.end()) {
                        if (capture_attribution) {
                            ++attribution.factored_subset_cache_misses;
                        }
                        continue;
                    }
                    const FactoredTerminalPredecessor& predecessor =
                        found->second;
                    if (predecessor.roll.availability_class == kNoId ||
                        predecessor.roll.availability_class >=
                            availability_classes.size()) {
                        throw std::logic_error(
                            "reforge factored terminal predecessor lost "
                            "availability identity");
                    }
                    const auto& words = availability_classes[
                        predecessor.roll.availability_class];
                    if (b / 64 >= words.size() ||
                        (words[b / 64] &
                         (std::uint64_t{1} << (b % 64))) == 0) {
                        throw std::logic_error(
                            "reforge factored terminal subset admitted an "
                            "unavailable last pick");
                    }
                    const RollState collision_check =
                        add_bucket_pick(predecessor.roll, b);
                    if (!(collision_check == candidate.completed)) {
                        if (capture_attribution) {
                            ++attribution
                                  .factored_subset_identity_mismatches;
                        }
                        throw std::logic_error(
                            "reforge factored terminal subset failed full "
                            "availability and observation identity check");
                    }
                    const std::uint64_t numerator =
                        exact_projected_bucket_remaining(
                            predecessor.roll, b);
                    if (numerator == 0 ||
                        predecessor.exact_denominator == 0) {
                        throw std::logic_error(
                            "reforge factored terminal recurrence has an "
                            "empty exact ratio");
                    }
                    probability_mass +=
                        static_cast<long double>(
                            predecessor.scaled_probability) *
                        (static_cast<long double>(numerator) /
                         static_cast<long double>(
                             predecessor.exact_denominator));
                    ++recurrence_terms;
                    credit_effort(effort.v3_recurrence_terms);
                    if (capture_attribution) {
                        ++attribution.factored_subset_cache_hits;
                    }
                }
                if (!(probability_mass > 0.0L)) {
                    throw std::logic_error(
                        "reforge factored terminal candidate has no exact "
                        "incoming mass");
                }
                const std::uint64_t active_work = saturated_add(
                    recurrence_terms, 1);
                telemetry_.reforge_factored_work = saturated_add(
                    telemetry_.reforge_factored_work, active_work);
                consume_reforge_work(active_work, 0);
                if (capture_attribution) {
                    attribution.factored_last_pick_terms = saturated_add(
                        attribution.factored_last_pick_terms,
                        recurrence_terms);
                    ++attribution.factored_terminal_commits;
                    attribution.frontier_work = saturated_add(
                        attribution.frontier_work, active_work);
                }
                commit_outcome(
                    candidate.completed,
                    static_cast<double>(probability_mass));
                credit_effort(effort.v3_commits);
            }
            factored_terminal_candidates.clear();
            factored_terminal_predecessors.clear();
            factored_terminal_scratch_bytes = 0;
        }
        frontier = std::move(next);
        if (capture_attribution) {
            frontier_order_multiplicity =
                std::move(next_order_multiplicity);
        }
        frontier_preflight_entries =
            next_preflight_entries;
    }
    attribution.frontier_build_ns = elapsed_ns(frontier_started);

    /* Pointer-identity kernel reuse is valid only while the reforge cache
     * retains the distribution object. Unretained exact distributions fall
     * back to the collision-checked content hash in SolveWork; advertising a
     * temporary object's address would leave a dangling identity key. */
    return finalize(true);
}

} // namespace solver
} // namespace poecraft
