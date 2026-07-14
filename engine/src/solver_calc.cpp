#include "solver_internal.hpp"

#include <algorithm>
#include <limits>
#include <map>
#include <stdexcept>
#include <vector>

#include "poecraft/bitset.h"
#include "poecraft/item_state.h"

/*
 * Solver S2: the calculation engine's exact deterministic and single-slot
 * transition paths (docs/crafting-solver-plan.md, Calculation Engine).
 *
 * Exactness strategy: materialize one representative concrete item for the
 * abstract state, then either apply the engine action directly (RNG-free
 * deterministic actions) or enumerate the same weighted pool the action
 * samples (get_weighted_pool with the action's request) and group candidate
 * outcomes by projected abstract successor. The engine's action code stays
 * the single execution authority; nothing here re-derives weight rules.
 *
 * Reforge-class actions and bespoke enumerators (unveil, veiled add,
 * eldritch, harvest) are S3; their distributions report supported = false.
 */
namespace poecraft {
namespace solver {

std::uint8_t rarity_affix_cap(const SessionImpl& session, std::uint8_t rarity) {
    switch (rarity) {
    case PC_RARITY_NORMAL:
        return 0;
    case PC_RARITY_MAGIC:
        return 1;
    default:
        return session.rare_affix_cap;
    }
}

namespace {

bool slot_metamod_matches(
    const SessionImpl& session,
    const pc_mod_slot& slot,
    int code) {
    return code >= 0 && slot.mod_id < session.metamod_type.size() &&
           session.metamod_type[slot.mod_id] == code;
}

bool item_side_locked(
    const SessionImpl& session,
    const pc_item_state& item,
    int side) {
    const int code = side == PC_SIDE_PREFIX
                         ? session.data->metamod_prefixes_locked_code
                         : session.data->metamod_suffixes_locked_code;
    for (std::uint8_t i = 0; i < item.prefix_count; ++i) {
        if (slot_metamod_matches(session, item.prefixes[i], code)) return true;
    }
    for (std::uint8_t i = 0; i < item.suffix_count; ++i) {
        if (slot_metamod_matches(session, item.suffixes[i], code)) return true;
    }
    return false;
}

void mod_group_list(
    const SessionImpl& session,
    std::uint32_t mod_id,
    std::vector<std::uint32_t>& out) {
    for (std::uint32_t i = session.group_offsets[mod_id];
         i < session.group_offsets[mod_id + 1]; ++i) {
        out.push_back(session.group_ids[i]);
    }
}

} // namespace

CalcContext::CalcContext(
    std::shared_ptr<const SessionImpl> session,
    const GoalSpec& goal,
    ActionRegistry registry,
    const std::vector<std::uint32_t>& action_indices,
    bool allow_empty_goal,
    bool empty_actions_mean_all,
    bool distinguish_junk_exclusion_effects)
    : session_(std::move(session)),
      goal_(goal),
      registry_(std::move(registry)),
      candidates_(action_indices),
      context_(0) {
    layout_ = build_abstract_layout(
        *session_, goal_, registry_, action_indices, allow_empty_goal,
        empty_actions_mean_all, distinguish_junk_exclusion_effects);
    if (candidates_.empty() && empty_actions_mean_all) {
        candidates_.resize(registry_.actions.size());
        for (std::uint32_t i = 0; i < candidates_.size(); ++i) {
            candidates_[i] = i;
        }
    }
    context_.session = session_;
    /* Exact paths never sample, and evaluation must not depend on trace
     * bookkeeping from earlier queries. */
    context_.capture_action_trace = false;
}

bool calc_supports(const ActionDescriptor& action) {
    if (action.synthetic) return true;
    switch (action.params.type) {
    case ActionType::Transmute:
    case ActionType::Augment:
    case ActionType::Alteration:
    case ActionType::Regal:
    case ActionType::Alchemy:
    case ActionType::Chaos:
    case ActionType::Exalt:
    case ActionType::Annul:
    case ActionType::Scour:
    case ActionType::Essence:
    case ActionType::Fossil:
    case ActionType::Bench:
    case ActionType::HarvestReforge:
    case ActionType::HarvestAugment:
    case ActionType::InfluenceExalt:
        return true;
    default:
        return false;
    }
}

bool CalcContext::is_goal_state(const AbstractState& state) const {
    if (state.rarity != goal_.rarity) return false;
    std::size_t satisfied = 0;
    for (std::size_t i = 0; i < layout_.slots.size(); ++i) {
        if (state.slot_status[i] ==
            static_cast<std::uint8_t>(GoalSlotStatus::Satisfied)) {
            ++satisfied;
        }
    }
    return satisfied >= goal_.required_satisfied_slots();
}

std::uint32_t CalcContext::intern_state(const AbstractState& state) {
    const std::size_t hash = abstract_state_hash(state);
    auto& bucket = state_ids_by_hash_[hash];
    for (std::uint32_t id : bucket) {
        if (states_[id] == state) return id;
    }
    const std::uint32_t id = static_cast<std::uint32_t>(states_.size());
    states_.push_back(state);
    bucket.push_back(id);
    return id;
}

const AbstractState& CalcContext::state(std::uint32_t state_id) const {
    return states_.at(state_id);
}

std::uint32_t CalcContext::state_count() const {
    return static_cast<std::uint32_t>(states_.size());
}

std::uint32_t CalcContext::intern_item(const pc_item_state& item) {
    return intern_state(project_item(*session_, layout_, item));
}

bool CalcContext::materialize(std::uint32_t state_id, pc_item_state& out) {
    const SessionImpl& session = *session_;
    const AbstractState& target = states_.at(state_id);
    pc_item_clear(&out);
    out.rarity = target.rarity;
    if (target.flags & kFlagCorrupted) out.item_flags |= PC_ITEM_CORRUPTED;
    if (target.flags & kFlagMirrored) out.item_flags |= PC_ITEM_MIRRORED;
    if (target.flags & kFlagSplit) out.item_flags |= PC_ITEM_SPLIT;
    if (target.flags & kFlagSynthesised) {
        out.item_flags |= PC_ITEM_SYNTHESISED;
    }
    out.generic_influence_bits = target.influence_bits;
    if (target.flags & kFlagEldritchImplicit) out.searing_exarch_tier = 1;

    /* Metamod abstract flags must come from actual metamod mods so the
     * engine's own side-lock/blocking checks see them on the item. */
    const DataImpl& data = *session.data;
    std::vector<int> needed_codes;
    const auto need = [&](std::uint32_t flag, int code) {
        if ((target.flags & flag) == 0) return true;
        if (code < 0) return false;
        needed_codes.push_back(code);
        return true;
    };
    if (!need(kFlagMultimod, data.metamod_multimod_code) ||
        !need(kFlagNoAttack, data.metamod_no_attack_code) ||
        !need(kFlagNoCaster, data.metamod_no_caster_code) ||
        !need(kFlagPrefixesLocked, data.metamod_prefixes_locked_code) ||
        !need(kFlagSuffixesLocked, data.metamod_suffixes_locked_code)) {
        return false;
    }

    std::vector<std::uint32_t> occupied_groups;
    std::vector<std::uint32_t> used_mods;
    std::vector<std::uint32_t> scratch_groups;
    const auto try_add = [&](std::uint32_t mod) {
        if (std::find(used_mods.begin(), used_mods.end(), mod) !=
            used_mods.end()) {
            return false;
        }
        scratch_groups.clear();
        mod_group_list(session, mod, scratch_groups);
        for (std::uint32_t group : scratch_groups) {
            if (std::find(occupied_groups.begin(), occupied_groups.end(),
                          group) != occupied_groups.end()) {
                return false;
            }
        }
        const std::int8_t gen = session.gen_type[mod];
        if (gen != 0 && gen != 1) return false;
        if (pc_item_add_mod(
                &out, gen == 0 ? PC_SIDE_PREFIX : PC_SIDE_SUFFIX, mod,
                static_cast<std::uint16_t>(session.primary_group[mod]), 0,
                nullptr) != PC_RESULT_OK) {
            return false;
        }
        occupied_groups.insert(occupied_groups.end(), scratch_groups.begin(),
                               scratch_groups.end());
        used_mods.push_back(mod);
        return true;
    };
    const auto first_from_mask =
        [&](const std::vector<std::uint64_t>& include,
            const std::vector<std::uint64_t>* exclude) {
            bool added = false;
            pc_bitset_for_each(
                include.data(), session.words, [&](std::size_t bit) {
                    if (added) return;
                    const std::uint32_t mod =
                        static_cast<std::uint32_t>(bit);
                    if (exclude != nullptr &&
                        pc_bitset_test(exclude->data(), bit)) {
                        return;
                    }
                    added = try_add(mod);
                });
            return added;
        };

    for (std::size_t i = 0; i < layout_.slots.size(); ++i) {
        const ResolvedGoalSlot& slot = layout_.slots[i];
        const auto status =
            static_cast<GoalSlotStatus>(target.slot_status[i]);
        if (status == GoalSlotStatus::Absent) continue;
        const bool satisfied = status == GoalSlotStatus::Satisfied;
        if (!first_from_mask(
                satisfied ? slot.satisfying_mask : slot.member_mask,
                satisfied ? nullptr : &slot.satisfying_mask)) {
            return false;
        }
    }

    for (std::size_t c = 0; c < layout_.junk_classes.size(); ++c) {
        std::uint32_t remaining =
            c < target.junk_counts.size() ? target.junk_counts[c] : 0;
        if (remaining == 0) continue;
        const JunkClass& junk = layout_.junk_classes[c];
        /* Prefer members that satisfy a still-needed metamod flag. */
        for (auto it = needed_codes.begin();
             remaining > 0 && it != needed_codes.end();) {
            bool found = false;
            pc_bitset_for_each(
                junk.member_mask.data(), session.words,
                [&](std::size_t bit) {
                    if (found) return;
                    const std::uint32_t mod =
                        static_cast<std::uint32_t>(bit);
                    if (session.metamod_type[mod] == *it && try_add(mod)) {
                        found = true;
                    }
                });
            if (found) {
                --remaining;
                it = needed_codes.erase(it);
            } else {
                ++it;
            }
        }
        bool exhausted = false;
        pc_bitset_for_each(
            junk.member_mask.data(), session.words, [&](std::size_t bit) {
                if (remaining == 0 || exhausted) return;
                if (try_add(static_cast<std::uint32_t>(bit))) --remaining;
            });
        if (remaining > 0) return false;
    }
    if (!needed_codes.empty()) return false;
    if (out.prefix_count != target.prefix_count ||
        out.suffix_count != target.suffix_count) {
        return false;
    }

    /* Slot-level decorations the abstraction tracks only as flags. The
     * choice of carrier slot is the accepted approximation; the S5 gate
     * measures whether it ever matters. */
    const auto decorate = [&](std::uint32_t flag, std::uint8_t slot_flag) {
        if ((target.flags & flag) == 0) return true;
        if (out.prefix_count > 0) {
            out.prefixes[0].flags |= slot_flag;
            return true;
        }
        if (out.suffix_count > 0) {
            out.suffixes[0].flags |= slot_flag;
            return true;
        }
        return false;
    };
    if (!decorate(kFlagCraftedMod, PC_MOD_SLOT_CRAFTED) ||
        !decorate(kFlagVeiledMod, PC_MOD_SLOT_VEILED) ||
        !decorate(kFlagFractured, PC_MOD_SLOT_FRACTURED)) {
        return false;
    }

    return project_item(session, layout_, out) == target;
}

const OutcomeDistribution& CalcContext::outcomes(
    std::uint32_t state_id,
    std::uint32_t action_index) {
    const std::uint64_t key =
        (static_cast<std::uint64_t>(state_id) << 32) | action_index;
    const auto cached = distribution_cache_.find(key);
    if (cached != distribution_cache_.end()) return cached->second;
    OutcomeDistribution distribution = evaluate(state_id, action_index);
    return distribution_cache_.emplace(key, std::move(distribution))
        .first->second;
}

OutcomeDistribution CalcContext::evaluate(
    std::uint32_t state_id,
    std::uint32_t action_index) {
    const ActionDescriptor& action = registry_.actions.at(action_index);
    const SessionImpl& session = *session_;
    OutcomeDistribution result;

    std::map<std::uint32_t, double> accumulated;
    const auto self_loop = [&]() { accumulated[state_id] += 1.0; };
    const auto add_successor = [&](const pc_item_state& item, double p) {
        accumulated[intern_item(item)] += p;
    };

    /* Illegal actions leave the item unchanged (engine semantics). */
    if (!action_legal(session, action, states_.at(state_id))) {
        result.supported = true;
        self_loop();
    } else if (action.synthetic) {
        /* restart: a fresh base with probability 1. */
        pc_item_state fresh;
        pc_item_clear(&fresh);
        result.supported = true;
        add_successor(fresh, 1.0);
    } else if (action.params.type == ActionType::Transmute ||
               action.params.type == ActionType::Alteration ||
               action.params.type == ActionType::Alchemy ||
               action.params.type == ActionType::Chaos ||
               action.params.type == ActionType::Essence ||
               action.params.type == ActionType::Fossil ||
               action.params.type == ActionType::HarvestReforge) {
        /* Sequential multi-mod rolls: the S3 roll DP (harvest adds a
         * guaranteed tag-targeted first pick). */
        return evaluate_reforge(state_id, action_index);
    } else {
        pc_item_state item;
        if (!materialize(state_id, item)) {
            return result; /* unsupported: no consistent representative */
        }
        switch (action.params.type) {
        case ActionType::Scour:
        case ActionType::Bench: {
            /* RNG-free engine actions: apply and project. */
            pc_item_state copy = item;
            apply_action(context_, &copy, action.params);
            result.supported = true;
            add_successor(copy, 1.0);
            break;
        }
        case ActionType::Augment:
        case ActionType::Exalt: {
            result.supported = true;
            if (!evaluate_pool_add(item, PoolBuildRequest{}, accumulated)) {
                self_loop();
            }
            break;
        }
        case ActionType::Regal: {
            /* Magic -> rare always applies; the added mod comes from the
             * pool of the upgraded item. An empty pool still upgrades. */
            pc_item_state upgraded = item;
            upgraded.rarity = PC_RARITY_RARE;
            result.supported = true;
            if (!evaluate_pool_add(
                    upgraded, PoolBuildRequest{}, accumulated)) {
                add_successor(upgraded, 1.0);
            }
            break;
        }
        case ActionType::InfluenceExalt: {
            pc_item_state influenced = item;
            const std::uint8_t bit = static_cast<std::uint8_t>(
                1u << (action.params.influence_code - 1));
            influenced.generic_influence_bits |= bit;
            PoolBuildRequest request;
            request.influence_only_code = action.params.influence_code;
            result.supported = true;
            /* The engine reverts the influence bit when no mod can be
             * added, leaving the item unchanged. */
            if (!evaluate_pool_add(influenced, request, accumulated)) {
                self_loop();
            }
            break;
        }
        case ActionType::HarvestAugment: {
            /* Add one tag-targeted mod (spawn-weight-only pool), then
             * remove one uniform other non-fractured mod on an unlocked
             * side — the add-then-remove semantics are intentional. */
            result.supported = true;
            PoolBuildRequest request;
            request.weight_kind = PoolWeightKind::HarvestSpawnOnly;
            request.target_tag_id = action.params.target_tag_id;
            const std::uint8_t cap = rarity_affix_cap(session, item.rarity);
            const bool prefix_open = item.prefix_count < cap;
            const bool suffix_open = item.suffix_count < cap;
            if (!prefix_open && !suffix_open) {
                self_loop();
                break;
            }
            request.side_filter =
                prefix_open && suffix_open ? -1 : (prefix_open ? 0 : 1);
            const WeightedPool& pool =
                get_weighted_pool(context_, &item, request);
            if (pool.total_weight == 0) {
                self_loop();
                break;
            }
            for (const PoolEntry& entry : pool.entries) {
                if (entry.final_weight == 0) continue;
                pc_item_state added = item;
                if (pc_item_add_mod(
                        &added,
                        entry.gen_type == 0 ? PC_SIDE_PREFIX
                                            : PC_SIDE_SUFFIX,
                        entry.session_mod_id,
                        static_cast<std::uint16_t>(entry.primary_group), 0,
                        nullptr) != PC_RESULT_OK) {
                    continue;
                }
                const double p_add =
                    static_cast<double>(entry.final_weight) /
                    static_cast<double>(pool.total_weight);
                struct Ref {
                    int side;
                    std::uint32_t index;
                };
                std::vector<Ref> removable;
                const auto collect = [&](int side, const pc_mod_slot* slots,
                                         std::uint8_t count) {
                    if (item_side_locked(session, added, side)) return;
                    for (std::uint8_t i = 0; i < count; ++i) {
                        if (!(slots[i].flags & PC_MOD_SLOT_FRACTURED) &&
                            slots[i].mod_id != entry.session_mod_id) {
                            removable.push_back({side, i});
                        }
                    }
                };
                collect(PC_SIDE_PREFIX, added.prefixes, added.prefix_count);
                collect(PC_SIDE_SUFFIX, added.suffixes, added.suffix_count);
                if (removable.empty()) {
                    add_successor(added, p_add);
                    continue;
                }
                const double each =
                    p_add / static_cast<double>(removable.size());
                for (const Ref& pick : removable) {
                    pc_item_state removed = added;
                    pc_item_remove_at(&removed, pick.side, pick.index);
                    add_successor(removed, each);
                }
            }
            if (accumulated.empty()) self_loop();
            break;
        }
        case ActionType::Annul: {
            result.supported = true;
            const bool prefix_locked =
                item_side_locked(session, item, PC_SIDE_PREFIX);
            const bool suffix_locked =
                item_side_locked(session, item, PC_SIDE_SUFFIX);
            struct Ref {
                int side;
                std::uint32_t index;
            };
            std::vector<Ref> removable;
            if (!prefix_locked) {
                for (std::uint8_t i = 0; i < item.prefix_count; ++i) {
                    if (!(item.prefixes[i].flags & PC_MOD_SLOT_FRACTURED)) {
                        removable.push_back({PC_SIDE_PREFIX, i});
                    }
                }
            }
            if (!suffix_locked) {
                for (std::uint8_t i = 0; i < item.suffix_count; ++i) {
                    if (!(item.suffixes[i].flags & PC_MOD_SLOT_FRACTURED)) {
                        removable.push_back({PC_SIDE_SUFFIX, i});
                    }
                }
            }
            if (removable.empty()) {
                self_loop();
                break;
            }
            const double each = 1.0 / static_cast<double>(removable.size());
            for (const Ref& pick : removable) {
                pc_item_state copy = item;
                pc_item_remove_at(&copy, pick.side, pick.index);
                add_successor(copy, each);
            }
            break;
        }
        default:
            /* Reforge and bespoke enumerators are S3. */
            return result;
        }
    }

    for (const auto& [successor, probability] : accumulated) {
        result.entries.push_back({successor, probability});
    }
    for (const OutcomeEntry& entry : result.entries) {
        const AbstractState& successor = states_.at(entry.state);
        for (std::size_t i = 0; i < layout_.slots.size(); ++i) {
            if (successor.slot_status[i] ==
                static_cast<std::uint8_t>(GoalSlotStatus::Satisfied)) {
                result.slot_satisfied_probability[i] += entry.probability;
            }
        }
    }
    return result;
}

/*
 * Exact single-add distribution: enumerate the same weighted pool the
 * engine's add_random_mod samples (open_side_filter semantics: caps and the
 * request's side filter only — metamod locks never close a side for adds)
 * and accumulate each candidate's projected successor at weight/total.
 * Returns false when no side is open or the pool is empty, which the engine
 * reports as an unapplied action.
 */
bool CalcContext::evaluate_pool_add(
    const pc_item_state& item,
    const PoolBuildRequest& base_request,
    std::map<std::uint32_t, double>& accumulated) {
    const SessionImpl& session = *session_;
    const std::uint8_t cap = rarity_affix_cap(session, item.rarity);
    const bool prefix_open =
        item.prefix_count < cap && base_request.side_filter != 1;
    const bool suffix_open =
        item.suffix_count < cap && base_request.side_filter != 0;
    if (!prefix_open && !suffix_open) return false;
    PoolBuildRequest request = base_request;
    request.side_filter =
        prefix_open && suffix_open ? -1 : (prefix_open ? 0 : 1);
    const WeightedPool& pool = get_weighted_pool(context_, &item, request);
    if (pool.total_weight == 0) return false;
    for (const PoolEntry& entry : pool.entries) {
        if (entry.final_weight == 0) continue;
        pc_item_state copy = item;
        if (pc_item_add_mod(
                &copy,
                entry.gen_type == 0 ? PC_SIDE_PREFIX : PC_SIDE_SUFFIX,
                entry.session_mod_id,
                static_cast<std::uint16_t>(entry.primary_group), 0,
                nullptr) != PC_RESULT_OK) {
            continue;
        }
        accumulated[intern_item(copy)] +=
            static_cast<double>(entry.final_weight) /
            static_cast<double>(pool.total_weight);
    }
    return true;
}

} // namespace solver
} // namespace poecraft
