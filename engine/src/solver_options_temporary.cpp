#include "solver_options_runtime_helpers.hpp"

namespace poecraft {
namespace solver {

namespace {

std::uint32_t temporary_tag_id(
    const SessionImpl& session,
    const char* name) {
    const auto found = session.data->tag_id_by_name.find(name);
    return found == session.data->tag_id_by_name.end()
               ? kNoId
               : found->second;
}

bool temporary_mod_has_tag(
    const SessionImpl& session,
    const std::uint32_t mod,
    const std::uint32_t tag) {
    if (mod >= session.mod_count || tag == kNoId) return false;
    for (std::uint32_t row = session.class_offsets[mod];
         row < session.class_offsets[mod + 1]; ++row) {
        if (session.class_tag_ids[row] == tag) return true;
    }
    return false;
}

} // namespace

void CalcContext::initialize_temporary_bench_effect_classes() {
    const auto started = std::chrono::steady_clock::now();
    automatic_goal_bench_actions_.clear();
    temporary_bench_effect_classes_.clear();
    temporary_bench_precompiled_bytes_ = 0;
    if (!goal_.automatic_candidates ||
        solver_action_family_disabled(
            goal_, SolverActionFamily::Bench)) {
        temporary_bench_precompile_ns_ = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - started)
                .count());
        return;
    }

    const bool temporary_bench_enabled =
        !solver_action_family_disabled(
            goal_, SolverActionFamily::TemporaryBench) &&
        registry_.index_by_id.contains("remove_crafted_modifiers");
    std::vector<std::uint32_t> temporary_bench;
    for (std::uint32_t index = 0; index < registry_.actions.size(); ++index) {
        const ActionDescriptor& action = registry_.actions[index];
        if (action.params.type != ActionType::Bench ||
            action.params.mod_id >= session_->metamod_type.size()) {
            continue;
        }
        const int metamod = session_->metamod_type[action.params.mod_id];
        const bool cannot_roll =
            metamod == session_->data->metamod_no_attack_code ||
            metamod == session_->data->metamod_no_caster_code;
        if (metamod < 0 &&
            goal_mask_for_mod(*session_, goal_, action.params.mod_id) != 0) {
            automatic_goal_bench_actions_.push_back(index);
        } else if (temporary_bench_enabled &&
                   (metamod < 0 || cannot_roll)) {
            temporary_bench.push_back(index);
        }
    }
    if (!temporary_bench_enabled) {
        temporary_bench_precompile_ns_ = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - started)
                .count());
        return;
    }

    std::vector<std::vector<std::uint64_t>> target_masks(
        goal_.slots.size(), std::vector<std::uint64_t>(session_->words, 0));
    for (std::uint32_t slot = 0; slot < goal_.slots.size(); ++slot) {
        for (std::uint32_t mod = 0; mod < session_->mod_count; ++mod) {
            if (mod_satisfies_goal_slot(*session_, mod, goal_.slots[slot])) {
                pc_bitset_set(target_masks[slot].data(), mod);
            }
        }
    }

    for (std::uint32_t followup = 0;
         followup < registry_.actions.size(); ++followup) {
        if (!temporary_followup(registry_.actions[followup])) continue;
        for (std::uint32_t slot = 0; slot < goal_.slots.size(); ++slot) {
            const std::int8_t target_side =
                goal_slot_side(*session_, goal_.slots[slot]);
            for (const std::uint32_t blocker_index : temporary_bench) {
                const ActionDescriptor& blocker =
                    registry_.actions[blocker_index];
                const std::uint32_t blocker_mod = blocker.params.mod_id;
                std::vector<std::uint64_t> conflict_mask(
                    session_->words, 0);
                const int metamod = session_->metamod_type[blocker_mod];
                const bool pool_tag_blocker =
                    metamod == session_->data->metamod_no_attack_code ||
                    metamod == session_->data->metamod_no_caster_code;
                const std::uint32_t blocked_tag = pool_tag_blocker
                    ? temporary_tag_id(
                          *session_,
                          metamod == session_->data->metamod_no_attack_code
                              ? "attack"
                              : "caster")
                    : kNoId;
                bool conflicts_positive = false;
                for (std::uint32_t mod = 0; mod < session_->mod_count; ++mod) {
                    const bool blocked = pool_tag_blocker
                        ? blocked_tag != kNoId &&
                              temporary_mod_has_tag(*session_, mod, blocked_tag)
                        : mods_conflict(*session_, blocker_mod, mod);
                    if (!blocked) continue;
                    pc_bitset_set(conflict_mask.data(), mod);
                    conflicts_positive |=
                        mod < session_->base_roll_weight.size() &&
                        session_->base_roll_weight[mod] > 0;
                }
                if (mask_intersects(conflict_mask, target_masks[slot])) {
                    continue;
                }
                const std::int8_t blocker_side =
                    session_->gen_type[blocker_mod];
                const bool can_change_capacity =
                    target_side >= 0 && target_side != blocker_side;
                if (!conflicts_positive && !can_change_capacity) continue;

                auto existing = std::find_if(
                    temporary_bench_effect_classes_.begin(),
                    temporary_bench_effect_classes_.end(),
                    [&](const TemporaryBenchEffectClass& candidate) {
                        return candidate.followup_action == followup &&
                               candidate.goal_slot == slot &&
                               candidate.blocker_side == blocker_side &&
                               candidate.pool_tag_blocker == pool_tag_blocker &&
                               candidate.conflict_mask == conflict_mask;
                    });
                if (existing == temporary_bench_effect_classes_.end()) {
                    TemporaryBenchEffectClass effect;
                    effect.followup_action = followup;
                    effect.goal_slot = slot;
                    effect.blocker_side = blocker_side;
                    effect.pool_tag_blocker = pool_tag_blocker;
                    effect.conflict_mask = std::move(conflict_mask);
                    effect.target_mask = target_masks[slot];
                    temporary_bench_effect_classes_.push_back(
                        std::move(effect));
                    existing = std::prev(
                        temporary_bench_effect_classes_.end());
                }
                const auto duplicate = std::find_if(
                    existing->blocker_actions.begin(),
                    existing->blocker_actions.end(),
                    [&](const std::uint32_t action) {
                        return registry_.actions[action].cost_keys ==
                               blocker.cost_keys;
                    });
                if (duplicate == existing->blocker_actions.end()) {
                    existing->blocker_actions.push_back(blocker_index);
                }
            }
        }
    }
    temporary_bench_precompiled_bytes_ =
        automatic_goal_bench_actions_.capacity() * sizeof(std::uint32_t) +
        temporary_bench_effect_classes_.capacity() *
            sizeof(TemporaryBenchEffectClass);
    for (const TemporaryBenchEffectClass& effect :
         temporary_bench_effect_classes_) {
        temporary_bench_precompiled_bytes_ +=
            effect.conflict_mask.capacity() * sizeof(std::uint64_t) +
            effect.target_mask.capacity() * sizeof(std::uint64_t) +
            effect.blocker_actions.capacity() * sizeof(std::uint32_t);
    }
    temporary_bench_precompile_ns_ = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - started)
            .count());
}

std::vector<std::uint64_t> CalcContext::temporary_followup_eligible_mask(
    const pc_item_state& carrier,
    const std::uint32_t followup_action) {
    std::vector<std::uint64_t> result(session_->words, 0);
    if (followup_action >= registry_.actions.size()) return result;
    const ActionDescriptor& followup = registry_.actions[followup_action];
    if (!temporary_followup(followup)) return result;

    pc_item_state pool_carrier = carrier;
    PoolBuildRequest request;
    switch (followup.params.type) {
    case ActionType::Augment:
        if (pool_carrier.rarity != PC_RARITY_MAGIC) return result;
        break;
    case ActionType::Regal:
        if (pool_carrier.rarity != PC_RARITY_MAGIC) return result;
        pool_carrier.rarity = PC_RARITY_RARE;
        break;
    case ActionType::Exalt:
        if (pool_carrier.rarity != PC_RARITY_RARE) return result;
        break;
    case ActionType::InfluenceExalt:
        if (pool_carrier.rarity != PC_RARITY_RARE ||
            followup.params.influence_code <= 0 ||
            followup.params.influence_code > 8) {
            return result;
        }
        pool_carrier.generic_influence_bits |= static_cast<std::uint8_t>(
            1u << (followup.params.influence_code - 1));
        request.influence_only_code = followup.params.influence_code;
        break;
    case ActionType::HarvestAugment:
        if (followup.params.target_tag_id == kNoId) return result;
        request.weight_kind = PoolWeightKind::TargetedNatural;
        request.target_tag_id = followup.params.target_tag_id;
        break;
    case ActionType::VeiledExalt: {
        if (pool_carrier.rarity != PC_RARITY_RARE) return result;
        const std::uint8_t cap =
            rarity_affix_cap(*session_, pool_carrier.rarity);
        if (pool_carrier.prefix_count < cap &&
            session_->veiled_prefix_mod_id < session_->mod_count) {
            pc_bitset_set(result.data(), session_->veiled_prefix_mod_id);
        }
        if (pool_carrier.suffix_count < cap &&
            session_->veiled_suffix_mod_id < session_->mod_count) {
            pc_bitset_set(result.data(), session_->veiled_suffix_mod_id);
        }
        return result;
    }
    default:
        return result;
    }

    const std::uint8_t cap =
        rarity_affix_cap(*session_, pool_carrier.rarity);
    const bool prefix_open = pool_carrier.prefix_count < cap;
    const bool suffix_open = pool_carrier.suffix_count < cap;
    if (!prefix_open && !suffix_open) return result;
    request.side_filter =
        prefix_open && suffix_open ? -1 : (prefix_open ? 0 : 1);
    const WeightedPool& pool =
        get_weighted_pool(context_, &pool_carrier, request);
    for (const PoolEntry& entry : pool.entries) {
        if (entry.final_weight != 0 && entry.session_mod_id < session_->mod_count) {
            pc_bitset_set(result.data(), entry.session_mod_id);
        }
    }
    return result;
}

CalcContext::NativeGoalDrawBound CalcContext::phase_goal_draw_bound(
        const pc_item_state& anchor, std::uint32_t action_index,
        std::uint32_t goal_slot, bool guaranteed) {
    const auto& action = registry_.actions.at(action_index);
    const auto& target = layout_.slots.at(goal_slot).satisfying_mask;
    NativeGoalDrawBound result;
    result.action = action_index; result.slot = goal_slot; result.guaranteed = guaranteed;
    for (std::uint32_t mod = 0; mod < session_->mod_count; ++mod) {
        if (!pc_bitset_test(target.data(), mod)) continue;
        const auto side = session_->gen_type[mod];
        if (side != PC_SIDE_PREFIX && side != PC_SIDE_SUFFIX)
            throw std::invalid_argument("phase probability needs explicit goal affixes");
        if (result.side >= 0 && result.side != side)
            throw std::invalid_argument("phase probability needs single-side goal events");
        result.side = side;
    }
    if (result.side < 0) throw std::invalid_argument("phase probability empty goal event");
    pc_item_state carrier = anchor;
    pc_item_clear_side(&carrier, PC_SIDE_PREFIX);
    pc_item_clear_side(&carrier, PC_SIDE_SUFFIX);
    carrier.rarity = PC_RARITY_RARE;
    PoolBuildRequest request;
    request.side_filter = result.side;
    if (action.params.type == ActionType::Fossil) {
        request.weight_kind = PoolWeightKind::Fossil;
        request.fossil_indices = action.params.fossil_indices;
    } else if (guaranteed) {
        if (action.params.type != ActionType::HarvestReforge &&
            action.params.type != ActionType::HarvestAugment)
            throw std::invalid_argument("phase probability unknown guaranteed pool");
        request.weight_kind = PoolWeightKind::TargetedNatural;
        request.target_tag_id = action.params.target_tag_id;
    }
    const auto& pool = get_weighted_pool(context_, &carrier, request);
    const auto add = [](std::uint64_t& sum, std::uint64_t x) {
        if (x > UINT64_MAX-sum) throw std::overflow_error("phase native weight overflow");
        sum += x;
    };
    for (const auto& entry : pool.entries) {
        if (!entry.final_weight) continue;
        if (entry.session_mod_id >= session_->mod_count)
            throw std::invalid_argument("phase native pool contains foreign modifier");
        ++result.entries;
        result.frame_escape |= session_->metamod_type.at(entry.session_mod_id) >= 0;
        add(pc_bitset_test(target.data(), entry.session_mod_id) ? result.target_weight :
            result.other_weight, entry.final_weight);
    }
    if (result.target_weight > UINT64_MAX-result.other_weight ||
        result.target_weight+result.other_weight != pool.total_weight)
        throw std::invalid_argument("phase native pool mass incomplete");
    // Do NOT union hypothetical satisfying members and delete their target
    // weight. For each real blocker, bound only its non-target exclusion mass.
    // If N' <= N and B' >= B-D then N'/(N'+B') <= N/(N+max(0,B-D)).
    // Sum the strongest distinct effects, counting overlaps repeatedly. Every
    // legal group-exclusive physical history uses no more blockers than this.
    for (std::uint32_t blocker = 0; blocker < session_->mod_count; ++blocker) {
        const int side = session_->gen_type[blocker];
        if (side != PC_SIDE_PREFIX && side != PC_SIDE_SUFFIX) continue;
        std::uint64_t removed = 0;
        for (const auto& entry : pool.entries) {
            if (pc_bitset_test(target.data(), entry.session_mod_id)) continue;
            if (mods_conflict(*session_, blocker, entry.session_mod_id)) add(removed, entry.final_weight);
        }
        auto& top = result.strongest_other_removal[side];
        for (auto& value : top) if (removed > value) std::swap(removed, value);
    }
    return result;
}

double CalcContext::optimistic_goal_draw_probability(
    const std::uint32_t carrier_state,
    const std::uint32_t action_index,
    const std::uint32_t goal_slot,
    const std::uint32_t satisfied_mask,
    const std::uint8_t prefix_blockers,
    const std::uint8_t suffix_blockers,
    const bool guaranteed_pool) {
    if (carrier_state >= state_count() ||
        action_index >= registry_.actions.size() ||
        goal_slot >= layout_.slots.size()) {
        return 0.0;
    }
    pc_item_state carrier;
    if (!materialize(carrier_state, carrier)) return 0.0;
    pc_item_clear_side(&carrier, PC_SIDE_PREFIX);
    pc_item_clear_side(&carrier, PC_SIDE_SUFFIX);

    const ActionDescriptor& action = registry_.actions[action_index];
    PoolBuildRequest request;
    if (action.params.type == ActionType::Fossil) {
        request.weight_kind = PoolWeightKind::Fossil;
        request.fossil_indices = action.params.fossil_indices;
    } else if (guaranteed_pool) {
        if ((action.params.type != ActionType::HarvestReforge &&
             action.params.type != ActionType::HarvestAugment) ||
            action.params.target_tag_id == kNoId) {
            return 0.0;
        }
        request.weight_kind = PoolWeightKind::TargetedNatural;
        request.target_tag_id = action.params.target_tag_id;
    }

    std::int8_t side = -1;
    bool side_seen = false;
    bool mixed_side = false;
    const ResolvedGoalSlot& target = layout_.slots[goal_slot];
    for (std::uint32_t mod = 0; mod < session_->mod_count; ++mod) {
        if (!pc_bitset_test(target.satisfying_mask.data(), mod)) continue;
        if (!side_seen) {
            side = session_->gen_type[mod];
            side_seen = true;
        } else if (side != session_->gen_type[mod]) {
            mixed_side = true;
        }
    }
    if (mixed_side) side = -1;
    request.side_filter = side;
    const WeightedPool& pool = get_weighted_pool(context_, &carrier, request);

    const auto groups_intersect = [&](const std::uint32_t mod,
                                      const std::vector<std::uint8_t>& groups) {
        for (std::uint32_t i = session_->group_offsets[mod];
             i < session_->group_offsets[mod + 1]; ++i) {
            const std::uint32_t group = session_->group_ids[i];
            if (group < groups.size() && groups[group]) return true;
        }
        return false;
    };
    std::vector<std::uint8_t> target_groups(
        session_->group_masks.size(), 0);
    std::vector<std::uint8_t> satisfied_groups(
        session_->group_masks.size(), 0);
    const auto include_slot_groups = [&](const ResolvedGoalSlot& slot,
                                         std::vector<std::uint8_t>& groups) {
        for (std::uint32_t mod = 0; mod < session_->mod_count; ++mod) {
            if (!pc_bitset_test(slot.satisfying_mask.data(), mod)) continue;
            for (std::uint32_t i = session_->group_offsets[mod];
                 i < session_->group_offsets[mod + 1]; ++i) {
                const std::uint32_t group = session_->group_ids[i];
                if (group < groups.size()) groups[group] = 1;
            }
        }
    };
    include_slot_groups(target, target_groups);
    for (std::uint32_t slot = 0; slot < layout_.slots.size(); ++slot) {
        if (slot == goal_slot || (satisfied_mask & (1u << slot)) == 0) {
            continue;
        }
        include_slot_groups(layout_.slots[slot], satisfied_groups);
    }

    struct WeightedMod {
        std::uint32_t mod = kNoId;
        std::uint64_t weight = 0;
    };
    std::vector<WeightedMod> remaining;
    remaining.reserve(pool.entries.size());
    std::uint64_t total_weight = 0;
    std::uint64_t target_weight = 0;
    for (const PoolEntry& entry : pool.entries) {
        if (entry.final_weight == 0 ||
            entry.session_mod_id >= session_->mod_count ||
            groups_intersect(entry.session_mod_id, satisfied_groups)) {
            continue;
        }
        remaining.push_back({entry.session_mod_id, entry.final_weight});
        total_weight += entry.final_weight;
        if (pc_bitset_test(
                target.satisfying_mask.data(), entry.session_mod_id)) {
            target_weight += entry.final_weight;
        }
    }
    if (target_weight == 0 || total_weight == 0) return 0.0;

    /* One occupied non-goal affix can contribute only its own complete group
     * exclusion effect. Give each optimistic blocker the strongest distinct
     * non-metamod effect in the whole session; summing effects even when they
     * overlap is deliberately more favorable than any real carrier. */
    std::array<std::vector<std::uint64_t>, 2> blocker_effects;
    blocker_effects[0].reserve(session_->mod_count);
    blocker_effects[1].reserve(session_->mod_count);
    for (std::uint32_t blocker = 0;
         blocker < session_->mod_count; ++blocker) {
        if (session_->gen_type[blocker] != PC_SIDE_PREFIX &&
            session_->gen_type[blocker] != PC_SIDE_SUFFIX) {
            continue;
        }
        if (blocker < session_->metamod_type.size() &&
            session_->metamod_type[blocker] >= 0) {
            continue;
        }
        if (groups_intersect(blocker, target_groups)) continue;
        std::uint64_t removed = 0;
        for (const WeightedMod& candidate : remaining) {
            bool conflict = false;
            for (std::uint32_t i = session_->group_offsets[blocker];
                 !conflict && i < session_->group_offsets[blocker + 1]; ++i) {
                const std::uint32_t group = session_->group_ids[i];
                for (std::uint32_t j =
                         session_->group_offsets[candidate.mod];
                     j < session_->group_offsets[candidate.mod + 1]; ++j) {
                    conflict |= group == session_->group_ids[j];
                }
            }
            if (conflict) removed += candidate.weight;
        }
        blocker_effects[session_->gen_type[blocker]].push_back(removed);
    }
    std::uint64_t optimistic_removed = 0;
    const std::array<std::uint8_t, 2> blocker_limits{
        prefix_blockers, suffix_blockers};
    for (std::size_t blocker_side = 0;
         blocker_side < blocker_effects.size(); ++blocker_side) {
        auto& effects = blocker_effects[blocker_side];
        std::sort(effects.begin(), effects.end(), std::greater<>());
        for (std::uint32_t i = 0;
             i < blocker_limits[blocker_side] && i < effects.size(); ++i) {
            optimistic_removed = std::min<std::uint64_t>(
                total_weight - target_weight,
                optimistic_removed + effects[i]);
        }
    }
    const std::uint64_t denominator = std::max<std::uint64_t>(
        target_weight, total_weight - optimistic_removed);
    return std::min(
        1.0, static_cast<double>(target_weight) /
                 static_cast<double>(denominator));
}

bool CalcContext::is_candidate_operator_admitted_for_state(
    const std::uint32_t state_id,
    const std::uint32_t operator_index) const {
    if (state_id >= states_.size() || operator_index >= operators_.size()) {
        return false;
    }

    const std::size_t static_count = std::min(
        static_candidate_operator_count_, candidate_operators_.size());
    const auto static_end =
        candidate_operators_.begin() +
        static_cast<std::ptrdiff_t>(static_count);
    if (std::find(
            candidate_operators_.begin(), static_end,
            operator_index) != static_end) {
        return true;
    }

    if (std::find(
            static_end, candidate_operators_.end(),
            operator_index) == candidate_operators_.end()) {
        return false;
    }
    if (!is_state_local_automatic_operator(operator_index)) {
        return true;
    }

    const auto retained =
        state_local_automatic_operators_.find(state_id);
    return retained != state_local_automatic_operators_.end() &&
           std::binary_search(
               retained->second.begin(), retained->second.end(),
               operator_index);
}

} // namespace solver
} // namespace poecraft
