#include "solver_internal.hpp"

#include <algorithm>
#include <array>
#include <functional>
#include <limits>
#include <map>
#include <stdexcept>
#include <tuple>
#include <unordered_set>
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
 * Reforge-class and bespoke evaluators share the same projected state and
 * engine-owned pool semantics. Unveil additionally records sampled offer
 * sets so Bellman backups can choose the cheapest offered successor.
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

int dominant_eldritch(const pc_item_state& item) {
    if (item.searing_exarch_tier > item.eater_of_worlds_tier) return 0;
    if (item.eater_of_worlds_tier > item.searing_exarch_tier) return 1;
    return -1;
}

bool mod_groups_conflict(
    const SessionImpl& session,
    const pc_item_state& item,
    std::uint32_t mod_id,
    int skip_side = -1,
    std::uint32_t skip_index = kNoId) {
    const auto conflicts = [&](const pc_mod_slot& slot) {
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
    for (std::uint8_t i = 0; i < item.prefix_count; ++i) {
        if (skip_side == PC_SIDE_PREFIX && skip_index == i) continue;
        if (conflicts(item.prefixes[i])) return true;
    }
    for (std::uint8_t i = 0; i < item.suffix_count; ++i) {
        if (skip_side == PC_SIDE_SUFFIX && skip_index == i) continue;
        if (conflicts(item.suffixes[i])) return true;
    }
    return false;
}

std::uint32_t unveil_weight(
    const SessionImpl& session,
    std::uint32_t mod_id) {
    if (mod_id >= session.mod_count) return 0;
    const DataImpl& data = *session.data;
    const std::uint32_t global = session.global_index[mod_id];
    if (global + 1 >= data.spawn_offsets.size()) return 0;
    std::unordered_set<std::uint32_t> tags(
        session.effective_base_tag_ids.begin(),
        session.effective_base_tag_ids.end());
    for (std::uint32_t i = data.spawn_offsets[global];
         i < data.spawn_offsets[global + 1]; ++i) {
        if (tags.count(data.spawn_tag_ids[i])) {
            return data.spawn_weights[i] > 0
                       ? static_cast<std::uint32_t>(data.spawn_weights[i])
                       : 0;
        }
    }
    return 0;
}

} // namespace

CalcContext::CalcContext(
    std::shared_ptr<const SessionImpl> session,
    const GoalSpec& goal,
    ActionRegistry registry,
    const std::vector<std::uint32_t>& action_indices,
    bool allow_empty_goal,
    bool empty_actions_mean_all,
    bool distinguish_junk_exclusion_effects,
    std::optional<std::uint32_t> state_cap)
    : session_(std::move(session)),
      goal_(goal),
      registry_(std::move(registry)),
      candidates_(action_indices),
      context_(0),
      state_cap_(state_cap) {
    if (candidates_.empty() && empty_actions_mean_all) {
        candidates_.resize(registry_.actions.size());
        for (std::uint32_t i = 0; i < candidates_.size(); ++i) {
            candidates_[i] = i;
        }
    }
    const bool exact_group_effects =
        distinguish_junk_exclusion_effects ||
        std::any_of(
            candidates_.begin(), candidates_.end(),
            [&](std::uint32_t index) {
                return registry_.actions.at(index).params.type ==
                       ActionType::Unveil;
            });
    layout_ = build_abstract_layout(
        *session_, goal_, registry_, candidates_, allow_empty_goal,
        false, exact_group_effects);
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
    case ActionType::VeiledChaos:
    case ActionType::VeiledExalt:
    case ActionType::Unveil:
    case ActionType::EldritchEmber:
    case ActionType::EldritchIchor:
    case ActionType::EldritchExalt:
    case ActionType::EldritchChaos:
    case ActionType::EldritchAnnul:
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
    const auto found = state_ids_by_hash_.find(hash);
    if (found != state_ids_by_hash_.end()) {
        for (std::uint32_t id : found->second) {
            if (states_[id] == state) return id;
        }
    }
    if (state_cap_.has_value() && states_.size() >= *state_cap_) {
        throw std::length_error(
            "calculation context exceeded max_states (" +
            std::to_string(*state_cap_) + ")");
    }
    if (states_.size() >= std::numeric_limits<std::uint32_t>::max()) {
        throw std::length_error("calculation context state id space exhausted");
    }
    const std::uint32_t id = static_cast<std::uint32_t>(states_.size());
    states_.push_back(state);
    state_ids_by_hash_[hash].push_back(id);
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
    out.searing_exarch_tier = target.searing_exarch_tier;
    out.eater_of_worlds_tier = target.eater_of_worlds_tier;

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
        !decorate(kFlagFractured, PC_MOD_SLOT_FRACTURED)) {
        return false;
    }
    if (target.flags & kFlagVeiledMod) {
        pc_mod_slot* slots = target.veiled_side == PC_SIDE_SUFFIX
                                 ? out.suffixes
                                 : out.prefixes;
        const std::uint8_t count = target.veiled_side == PC_SIDE_SUFFIX
                                       ? out.suffix_count
                                       : out.prefix_count;
        if (count == 0) return false;
        const std::uint32_t expected =
            target.veiled_side == PC_SIDE_SUFFIX
                ? session.veiled_suffix_mod_id
                : session.veiled_prefix_mod_id;
        pc_mod_slot* carrier = nullptr;
        for (std::uint8_t i = 0; i < count; ++i) {
            if (slots[i].mod_id == expected) {
                carrier = &slots[i];
                break;
            }
        }
        if (carrier == nullptr) return false;
        carrier->flags |= PC_MOD_SLOT_VEILED;
    }

    return project_item(session, layout_, out) == target;
}

const OutcomeDistribution& CalcContext::outcomes(
    std::uint32_t state_id,
    std::uint32_t action_index) {
    const std::uint64_t key =
        (static_cast<std::uint64_t>(state_id) << 32) | action_index;
    const auto cached = distribution_cache_.find(key);
    if (cached != distribution_cache_.end()) return *cached->second;
    std::shared_ptr<const OutcomeDistribution> distribution =
        evaluate(state_id, action_index);
    return *distribution_cache_.emplace(key, std::move(distribution))
                .first->second;
}

std::shared_ptr<const OutcomeDistribution> CalcContext::evaluate(
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
               action.params.type == ActionType::HarvestReforge ||
               action.params.type == ActionType::VeiledChaos ||
               action.params.type == ActionType::EldritchChaos) {
        /* Sequential multi-mod rolls: the S3 roll DP (harvest adds a
         * guaranteed tag-targeted first pick). */
        return evaluate_reforge(state_id, action_index);
    } else if (action.params.type == ActionType::Unveil) {
        return evaluate_unveil(state_id);
    } else {
        pc_item_state item;
        if (!materialize(state_id, item)) {
            return std::make_shared<OutcomeDistribution>(std::move(result));
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
        case ActionType::VeiledExalt: {
            result.supported = true;
            const std::uint8_t cap = rarity_affix_cap(session, item.rarity);
            const bool prefix_open = item.prefix_count < cap;
            const bool suffix_open = item.suffix_count < cap;
            const double side_probability =
                prefix_open && suffix_open ? 0.5 : 1.0;
            const auto add_side = [&](int side, bool open) {
                if (!open) return;
                const std::uint32_t mod_id =
                    side == PC_SIDE_PREFIX ? session.veiled_prefix_mod_id
                                           : session.veiled_suffix_mod_id;
                pc_item_state copy = item;
                if (mod_id == kNoId ||
                    pc_item_add_mod(
                        &copy, side, mod_id,
                        static_cast<std::uint16_t>(
                            session.primary_group[mod_id]),
                        PC_MOD_SLOT_VEILED, nullptr) != PC_RESULT_OK) {
                    accumulated[state_id] += side_probability;
                    return;
                }
                add_successor(copy, side_probability);
            };
            add_side(PC_SIDE_PREFIX, prefix_open);
            add_side(PC_SIDE_SUFFIX, suffix_open);
            if (accumulated.empty()) self_loop();
            break;
        }
        case ActionType::EldritchEmber:
        case ActionType::EldritchIchor: {
            result.supported = true;
            pc_item_state copy = item;
            if (action.params.type == ActionType::EldritchEmber) {
                copy.searing_exarch_tier =
                    static_cast<std::uint8_t>(action.params.tier);
            } else {
                copy.eater_of_worlds_tier =
                    static_cast<std::uint8_t>(action.params.tier);
            }
            add_successor(copy, 1.0);
            break;
        }
        case ActionType::EldritchExalt: {
            result.supported = true;
            PoolBuildRequest request;
            request.side_filter = dominant_eldritch(item);
            if (!evaluate_pool_add(item, request, accumulated)) self_loop();
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
        case ActionType::Annul:
        case ActionType::EldritchAnnul: {
            result.supported = true;
            const int eldritch_side =
                action.params.type == ActionType::EldritchAnnul
                    ? dominant_eldritch(item)
                    : -1;
            const bool prefix_locked =
                eldritch_side == PC_SIDE_SUFFIX ||
                (eldritch_side < 0 &&
                 item_side_locked(session, item, PC_SIDE_PREFIX));
            const bool suffix_locked =
                eldritch_side == PC_SIDE_PREFIX ||
                (eldritch_side < 0 &&
                 item_side_locked(session, item, PC_SIDE_SUFFIX));
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
            /* Mechanics without an exact evaluator remain unsupported. */
            return std::make_shared<OutcomeDistribution>(std::move(result));
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
    return std::make_shared<OutcomeDistribution>(std::move(result));
}

std::shared_ptr<const OutcomeDistribution> CalcContext::evaluate_unveil(
    std::uint32_t state_id) {
    OutcomeDistribution result;
    result.supported = true;
    pc_item_state item;
    if (!materialize(state_id, item)) {
        result.supported = false;
        return std::make_shared<OutcomeDistribution>(std::move(result));
    }
    int side = -1;
    std::uint32_t index = 0;
    if (pc_item_find_veiled(&item, &side, &index) != PC_RESULT_OK) {
        result.entries.push_back({state_id, 1.0});
        return std::make_shared<OutcomeDistribution>(std::move(result));
    }

    struct Candidate {
        std::uint32_t mod_id = kNoId;
        std::uint32_t state = kNoId;
        double weight = 0.0;
    };
    std::vector<Candidate> candidates;
    const SessionImpl& session = *session_;
    pc_bitset_for_each(
        session.unveiled_generic_mask.data(), session.words,
        [&](std::size_t bit) {
            const std::uint32_t mod_id = static_cast<std::uint32_t>(bit);
            if (session.gen_type[mod_id] != side ||
                mod_groups_conflict(session, item, mod_id, side, index)) {
                return;
            }
            const std::uint32_t weight = unveil_weight(session, mod_id);
            if (weight == 0) return;
            pc_item_state copy = item;
            pc_mod_slot& slot = side == PC_SIDE_PREFIX
                                    ? copy.prefixes[index]
                                    : copy.suffixes[index];
            slot.mod_id = mod_id;
            slot.group_id = static_cast<std::uint16_t>(
                session.primary_group[mod_id]);
            slot.flags &= static_cast<std::uint8_t>(~PC_MOD_SLOT_VEILED);
            slot.veiled_chosen_mod_id = mod_id;
            slot.veiled_option_count = 0;
            candidates.push_back(
                {mod_id, intern_item(copy), static_cast<double>(weight)});
        });
    if (candidates.empty()) {
        result.entries.push_back({state_id, 1.0});
        return std::make_shared<OutcomeDistribution>(std::move(result));
    }
    std::sort(
        candidates.begin(), candidates.end(),
        [](const Candidate& a, const Candidate& b) {
            return a.mod_id < b.mod_id;
        });
    for (const Candidate& candidate : candidates) {
        result.choice_options.push_back(
            {candidate.mod_id, candidate.state});
    }

    /* The engine samples up to three weighted options without replacement.
     * Aggregate ordered draw paths by the set of abstract successors they
     * offer; Bellman backups consume these choice groups directly. */
    const std::size_t draw_count = std::min<std::size_t>(
        PC_MAX_VEILED_OPTIONS, candidates.size());
    double total_weight = 0.0;
    for (const Candidate& candidate : candidates) {
        total_weight += candidate.weight;
    }
    std::map<std::vector<std::uint32_t>, double> offered;
    std::vector<std::uint8_t> used(candidates.size(), 0);
    std::vector<std::uint32_t> picked_states;
    const std::function<void(std::size_t, double, double)> draw =
        [&](std::size_t depth, double remaining, double probability) {
            if (depth == draw_count) {
                std::vector<std::uint32_t> states = picked_states;
                std::sort(states.begin(), states.end());
                states.erase(std::unique(states.begin(), states.end()),
                             states.end());
                offered[std::move(states)] += probability;
                return;
            }
            for (std::size_t i = 0; i < candidates.size(); ++i) {
                if (used[i]) continue;
                used[i] = 1;
                picked_states.push_back(candidates[i].state);
                draw(depth + 1, remaining - candidates[i].weight,
                     probability * candidates[i].weight / remaining);
                picked_states.pop_back();
                used[i] = 0;
            }
        };
    draw(0, total_weight, 1.0);

    std::map<std::uint32_t, double> displayed;
    const auto immediate_better = [&](std::uint32_t a, std::uint32_t b) {
        const AbstractState& left = state(a);
        const AbstractState& right = state(b);
        const auto score = [&](const AbstractState& value) {
            int satisfied = 0;
            int below = 0;
            for (std::size_t i = 0; i < layout_.slots.size(); ++i) {
                satisfied += value.slot_status[i] ==
                             static_cast<std::uint8_t>(
                                 GoalSlotStatus::Satisfied);
                below += value.slot_status[i] ==
                         static_cast<std::uint8_t>(
                             GoalSlotStatus::PresentBelowTier);
            }
            return std::tuple<int, int, int>{
                is_goal_state(value) ? 1 : 0, satisfied, below};
        };
        const auto left_score = score(left);
        const auto right_score = score(right);
        return left_score != right_score ? left_score > right_score : a < b;
    };
    for (const auto& [states, probability] : offered) {
        result.choice_groups.push_back({probability, states});
        std::uint32_t chosen = states.front();
        for (std::uint32_t successor : states) {
            if (immediate_better(successor, chosen)) chosen = successor;
        }
        displayed[chosen] += probability;
    }
    for (const auto& [successor, probability] : displayed) {
        result.entries.push_back({successor, probability});
    }
    for (const OutcomeEntry& entry : result.entries) {
        const AbstractState& successor = state(entry.state);
        for (std::size_t i = 0; i < layout_.slots.size(); ++i) {
            if (successor.slot_status[i] ==
                static_cast<std::uint8_t>(GoalSlotStatus::Satisfied)) {
                result.slot_satisfied_probability[i] += entry.probability;
            }
        }
    }
    return std::make_shared<OutcomeDistribution>(std::move(result));
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
