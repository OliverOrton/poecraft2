#pragma once

#include "solver_calc_types.hpp"
#include "solver_action_family_contract.hpp"

#include "poecraft/bitset.h"

#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <functional>
#include <iterator>
#include <limits>
#include <map>
#include <numeric>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace poecraft {
namespace solver {
namespace {

bool state_has_unfractured_crafted(const AbstractState& state);

AutomaticTelemetryKind telemetry_kind_for_candidate(
    const AutomaticCandidateKind kind) {
    return automatic_telemetry_kind_for_candidate(kind);
}

std::uint64_t outcome_count(const OutcomeDistribution& distribution) {
    std::uint64_t count = distribution.entries.size();
    for (const OutcomeChoiceGroup& group : distribution.choice_groups) {
        count += group.states.size();
    }
    return count;
}

std::uint64_t outcome_count(const OptionKernel& kernel) {
    std::uint64_t count = kernel.exits.size();
    for (const OutcomeChoiceGroup& group :
         kernel.observation_choice_groups) {
        count += group.states.size();
    }
    return count;
}

const ActionDescriptor& require_action(
    const ActionRegistry& registry,
    const std::string& id,
    std::uint32_t& out_index) {
    const auto found = registry.index_by_id.find(id);
    if (found == registry.index_by_id.end()) {
        throw std::runtime_error("fixed option: unknown action: " + id);
    }
    out_index = found->second;
    return registry.actions.at(out_index);
}

std::vector<std::pair<std::string, double>> aggregate_resources(
    const ActionRegistry& registry,
    const std::vector<std::uint32_t>& program) {
    std::map<std::string, double> quantities;
    for (const std::uint32_t index : program) {
        for (const std::string& key : registry.actions.at(index).cost_keys) {
            quantities[key] += 1.0;
        }
    }
    return {quantities.begin(), quantities.end()};
}

const char* side_name(const std::int8_t side) {
    return side == PC_SIDE_PREFIX ? "prefix" : "suffix";
}

void require_side(const FixedOptionSpec& spec) {
    if (spec.side != PC_SIDE_PREFIX && spec.side != PC_SIDE_SUFFIX) {
        throw std::runtime_error(
            "fixed option: side must be prefix or suffix");
    }
}

std::uint32_t find_metamod_action(
    const SessionImpl& session,
    const ActionRegistry& registry,
    const int metamod_code,
    const char* label) {
    if (metamod_code < 0) {
        throw std::runtime_error(
            std::string("fixed option: session has no ") + label +
            " metamod code");
    }
    std::uint32_t found = kNoId;
    for (std::uint32_t index = 0; index < registry.actions.size(); ++index) {
        const ActionDescriptor& action = registry.actions[index];
        if (action.params.type != ActionType::Bench ||
            action.params.mod_id >= session.metamod_type.size() ||
            session.metamod_type[action.params.mod_id] != metamod_code) {
            continue;
        }
        if (found != kNoId) {
            throw std::runtime_error(
                std::string("fixed option: multiple ") + label +
                " bench actions are available");
        }
        found = index;
    }
    if (found == kNoId) {
        throw std::runtime_error(
            std::string("fixed option: missing ") + label +
            " bench action");
    }
    return found;
}

std::string join_ids(
    const ActionRegistry& registry,
    const std::vector<std::uint32_t>& actions) {
    std::string result;
    for (std::size_t i = 0; i < actions.size(); ++i) {
        if (i > 0) result += '+';
        result += registry.actions.at(actions[i]).id;
    }
    return result;
}

bool eldritch_dominates(
    const AbstractState& state,
    const std::int8_t intended_side) {
    return intended_side == PC_SIDE_PREFIX
               ? state.searing_exarch_tier > state.eater_of_worlds_tier
               : state.eater_of_worlds_tier > state.searing_exarch_tier;
}

bool intended_eldritch_action_legal(
    const CalcContext& calc,
    const AbstractState& state,
    const ActionDescriptor& action,
    const std::int8_t side,
    const std::uint32_t state_id) {
    if ((state.flags & kFlagInfluenced) != 0) return false;
    if (!eldritch_dominates(state, side)) return false;
    if (action.params.type == ActionType::EldritchExalt) {
        const std::uint8_t count = side == PC_SIDE_PREFIX
                                       ? state.prefix_count
                                       : state.suffix_count;
        return count < rarity_affix_cap(calc.session(), state.rarity);
    }
    if (action.params.type == ActionType::EldritchAnnul) {
        pc_item_state item;
        if (!calc.materialize(state_id, item)) return false;
        const pc_mod_slot* slots = side == PC_SIDE_PREFIX ? item.prefixes
                                                          : item.suffixes;
        const std::uint8_t count = side == PC_SIDE_PREFIX
                                       ? item.prefix_count
                                       : item.suffix_count;
        for (std::uint8_t i = 0; i < count; ++i) {
            if ((slots[i].flags & PC_MOD_SLOT_FRACTURED) == 0) return true;
        }
        return false;
    }
    return action.params.type == ActionType::EldritchChaos;
}

bool approved_renewal_roll(const ActionDescriptor& action) {
    switch (action.params.type) {
    case ActionType::Alteration:
    case ActionType::Chaos:
    case ActionType::Essence:
    case ActionType::Fossil:
    case ActionType::HarvestReforge:
    case ActionType::VeiledChaos:
        return action.kind == TransitionKind::Reforge;
    default:
        return false;
    }
}

bool imprint_action_may_roll_influence_pool(
    const ActionDescriptor& action) {
    switch (action.params.type) {
    case ActionType::Transmute:
    case ActionType::Augment:
    case ActionType::Alteration:
    case ActionType::Regal:
    case ActionType::Alchemy:
    case ActionType::Chaos:
    case ActionType::Exalt:
    case ActionType::Essence:
    case ActionType::Fossil:
    case ActionType::HarvestReforge:
    case ActionType::HarvestAugment:
    case ActionType::HarvestResist:
    case ActionType::VeiledChaos:
        return true;
    default:
        return false;
    }
}

bool mod_satisfies_goal_slot(
    const SessionImpl& session,
    const std::uint32_t mod_id,
    const GoalSlot& slot) {
    if (mod_id >= session.mod_count) return false;
    bool member = false;
    if (slot.group_id != kNoId) {
        for (std::uint32_t row = session.group_offsets[mod_id];
             row < session.group_offsets[mod_id + 1]; ++row) {
            member |= session.group_ids[row] == slot.group_id;
        }
    } else if (slot.family_id != kNoId) {
        member = session.family_id[mod_id] == slot.family_id;
    }
    if (!member) return false;
    const std::uint32_t tier = session.family_tier_index[mod_id];
    return slot.min_tier == 0 ||
           (tier != 0 && tier <= slot.min_tier);
}

std::uint32_t goal_mask_for_mod(
    const SessionImpl& session,
    const GoalSpec& goal,
    const std::uint32_t mod_id) {
    std::uint32_t mask = 0;
    for (std::uint32_t slot = 0; slot < goal.slots.size(); ++slot) {
        if (mod_satisfies_goal_slot(session, mod_id, goal.slots[slot])) {
            mask |= 1u << slot;
        }
    }
    return mask;
}

std::int8_t goal_slot_side(
    const SessionImpl& session,
    const GoalSlot& slot) {
    std::int8_t side = -1;
    for (std::uint32_t mod = 0; mod < session.mod_count; ++mod) {
        if (!mod_satisfies_goal_slot(session, mod, slot)) continue;
        if (session.gen_type[mod] != PC_SIDE_PREFIX &&
            session.gen_type[mod] != PC_SIDE_SUFFIX) {
            continue;
        }
        if (side == -1) {
            side = session.gen_type[mod];
        } else if (side != session.gen_type[mod]) {
            return -1;
        }
    }
    return side;
}

bool mods_conflict(
    const SessionImpl& session,
    const std::uint32_t left,
    const std::uint32_t right) {
    if (left >= session.mod_count || right >= session.mod_count) return true;
    for (std::uint32_t a = session.group_offsets[left];
         a < session.group_offsets[left + 1]; ++a) {
        for (std::uint32_t b = session.group_offsets[right];
             b < session.group_offsets[right + 1]; ++b) {
            if (session.group_ids[a] == session.group_ids[b]) return true;
        }
    }
    return false;
}

bool temporary_followup(const ActionDescriptor& action) {
    if (!calc_supports(action) || action.synthetic ||
        action.kind != TransitionKind::SingleSlot) {
        return false;
    }
    switch (action.params.type) {
    case ActionType::Augment:
    case ActionType::Regal:
    case ActionType::Exalt:
    case ActionType::VeiledExalt:
    case ActionType::HarvestAugment:
    case ActionType::InfluenceExalt:
        return true;
    default:
        return false;
    }
}

std::uint32_t satisfied_goal_mask(const AbstractState& state);

bool target_slot_missing(
    const AbstractState& state,
    const std::uint32_t slot) {
    return slot < kMaxGoalSlots &&
           state.slot_status[slot] != static_cast<std::uint8_t>(
               GoalSlotStatus::Satisfied);
}

struct TemporaryBenchCandidateGroup {
    std::uint32_t representative_blocker = kNoId;
    std::uint32_t followup_action = kNoId;
    std::uint32_t goal_slot = kNoId;
    bool pool_tag_blocker = false;
    std::vector<std::uint32_t> blocker_variants;
};

struct AutomaticOptionSynthesis {
    std::vector<FixedOptionSpec> specs;
    std::vector<TemporaryBenchCandidateGroup> temporary_groups;
    std::uint64_t temporary_precompiled_classes = 0;
    std::uint64_t temporary_candidate_variants = 0;
    std::uint64_t temporary_effect_classes = 0;
    std::uint64_t temporary_collapsed_variants = 0;
    std::uint64_t temporary_enumeration_ns = 0;
};

bool mask_has_any(const std::vector<std::uint64_t>& mask) {
    return std::any_of(mask.begin(), mask.end(), [](const std::uint64_t word) {
        return word != 0;
    });
}

bool mask_intersects(
    const std::vector<std::uint64_t>& left,
    const std::vector<std::uint64_t>& right) {
    const std::size_t words = std::min(left.size(), right.size());
    for (std::size_t i = 0; i < words; ++i) {
        if ((left[i] & right[i]) != 0) return true;
    }
    return false;
}

std::string automatic_context_key(
    const std::vector<FixedOptionSpec>& specs,
    const std::vector<std::uint32_t>& candidates) {
    std::string out;
    const auto number = [&](const auto value) {
        out += std::to_string(value);
        out.push_back(';');
    };
    const auto string = [&](const std::string& value) {
        number(value.size());
        out.append(value);
    };
    const auto strings = [&](const std::vector<std::string>& values) {
        number(values.size());
        for (const std::string& value : values) string(value);
    };
    number(specs.size());
    for (const FixedOptionSpec& spec : specs) {
        number(static_cast<std::uint8_t>(spec.kind));
        number(static_cast<int>(spec.side));
        string(spec.action_id);
        strings(spec.setup_action_ids);
        strings(spec.bench_craft_ids);
        strings(spec.program_action_ids);
        number(spec.exit_goal_slots.size());
        for (const std::uint32_t slot : spec.exit_goal_slots) number(slot);
        number(spec.exit_min_satisfied);
        string(spec.constructive_finish_action_id);
        number(spec.carrier_goal_slot);
        number(static_cast<std::uint8_t>(spec.automatic_kind));
        number(spec.relevant_goal_mask);
    }
    number(candidates.size());
    for (const std::uint32_t candidate : candidates) number(candidate);
    return out;
}

bool temporary_blocker_applies(
    const SessionImpl& session,
    const pc_item_state& carrier,
    const TemporaryBenchEffectClass& effect) {
    const std::uint8_t cap = rarity_affix_cap(session, carrier.rarity);
    if ((effect.blocker_side == PC_SIDE_PREFIX &&
         carrier.prefix_count >= cap) ||
        (effect.blocker_side == PC_SIDE_SUFFIX &&
         carrier.suffix_count >= cap)) {
        return false;
    }
    const auto conflicts = [&](const pc_mod_slot* mods,
                               const std::uint8_t count) {
        for (std::uint8_t i = 0; i < count; ++i) {
            if (mods[i].mod_id < session.mod_count &&
                pc_bitset_test(effect.conflict_mask.data(), mods[i].mod_id)) {
                return true;
            }
        }
        return false;
    };
    if (effect.pool_tag_blocker) return true;
    return !conflicts(carrier.prefixes, carrier.prefix_count) &&
           !conflicts(carrier.suffixes, carrier.suffix_count);
}

AutomaticOptionSynthesis synthesize_automatic_options(
    CalcContext& calc,
    const std::uint32_t state_id,
    const pc_item_state& carrier,
    const std::unordered_map<std::string, double>* prices) {
    const SessionImpl& session = calc.session();
    const GoalSpec& goal = calc.goal();
    const ActionRegistry& registry = calc.registry();
    const AbstractState& state = calc.state(state_id);
    AutomaticOptionSynthesis synthesis;
    std::vector<FixedOptionSpec>& result = synthesis.specs;
    if (!goal.automatic_candidates) return synthesis;
    const auto action_has_prices = [&](const std::uint32_t action_index) {
        if (prices == nullptr) return true;
        return std::all_of(
            registry.actions.at(action_index).cost_keys.begin(),
            registry.actions.at(action_index).cost_keys.end(),
            [&](const std::string& key) { return prices->contains(key); });
    };
    const std::vector<std::uint32_t>& goal_bench =
        calc.automatic_goal_bench_actions();

    /*
     * Product Eldritch planning exposes state-local side intents.
     * Setup is selected here because prices and the exact carrier dominance
     * are both available. The resulting fixed option is still an ordinary
     * sequence of real currency operations; no hidden dominance flag enters
     * the state or evaluator.
     */
    if (session.eldritch_eligible &&
        state.rarity == PC_RARITY_RARE) {
        const auto action_cost =
            [&](const std::uint32_t action) {
                if (!action_has_prices(action)) {
                    return std::numeric_limits<double>::infinity();
                }
                double cost = 0.0;
                if (prices != nullptr) {
                    for (const std::string& key :
                         registry.actions.at(action).cost_keys) {
                        cost += prices->at(key);
                    }
                }
                return cost;
            };
        const auto setup_for_side =
            [&](const std::int8_t side)
                -> std::optional<std::vector<std::string>> {
                if (eldritch_dominates(state, side)) {
                    return std::vector<std::string>{};
                }
                const char* desired =
                    side == PC_SIDE_PREFIX
                        ? "eldritch_ember:"
                        : "eldritch_ichor:";
                const char* opposite =
                    side == PC_SIDE_PREFIX
                        ? "eldritch_ichor:"
                        : "eldritch_ember:";
                const std::uint32_t opposing_tier =
                    side == PC_SIDE_PREFIX
                        ? state.eater_of_worlds_tier
                        : state.searing_exarch_tier;
                double best_cost =
                    std::numeric_limits<double>::infinity();
                std::vector<std::string> best;
                std::vector<std::string> deterministic_fallback;
                const auto consider =
                    [&](std::vector<std::string> ids) {
                        double cost = 0.0;
                        bool complete = true;
                        for (const std::string& id : ids) {
                            const auto found =
                                registry.index_by_id.find(id);
                            if (found == registry.index_by_id.end()) {
                                return;
                            }
                            const double part = action_cost(found->second);
                            if (!std::isfinite(part)) complete = false;
                            cost += part;
                        }
                        if (deterministic_fallback.empty() ||
                            ids < deterministic_fallback) {
                            deterministic_fallback = ids;
                        }
                        if (!complete) return;
                        if (cost < best_cost ||
                            (cost == best_cost &&
                             (best.empty() || ids < best))) {
                            best_cost = cost;
                            best = std::move(ids);
                        }
                    };
                for (std::uint32_t desired_tier = 1;
                     desired_tier <= 4; ++desired_tier) {
                    if (desired_tier > opposing_tier) {
                        consider({
                            std::string(desired) +
                            std::to_string(desired_tier)});
                    }
                }
                for (std::uint32_t opposing = 1;
                     opposing <= 4; ++opposing) {
                    for (std::uint32_t desired_tier = opposing + 1;
                         desired_tier <= 4; ++desired_tier) {
                        consider({
                            std::string(opposite) +
                                std::to_string(opposing),
                            std::string(desired) +
                                std::to_string(desired_tier)});
                    }
                }
                if (std::isfinite(best_cost)) return best;
                if (!deterministic_fallback.empty()) {
                    return deterministic_fallback;
                }
                return std::nullopt;
            };

        const std::uint32_t satisfied = satisfied_goal_mask(state);
        const std::uint8_t cap =
            rarity_affix_cap(session, state.rarity);
        std::vector<std::uint64_t> ordinary_exalt_eligible;
        const auto ordinary_exalt = registry.index_by_id.find("exalt");
        if (ordinary_exalt != registry.index_by_id.end()) {
            /* Eldritch Exalt samples the same ordinary explicit pool as
             * Exalt, constrained to its dominant side. Querying the native
             * Exalt pool here keeps group conflicts, spawn weights, and the
             * exact carrier authoritative instead of re-deriving them. */
            ordinary_exalt_eligible =
                calc.temporary_followup_eligible_mask(
                    carrier, ordinary_exalt->second);
        }
        for (const std::int8_t side :
             {static_cast<std::int8_t>(PC_SIDE_PREFIX),
              static_cast<std::int8_t>(PC_SIDE_SUFFIX)}) {
            std::uint32_t side_goal_mask = 0;
            std::uint32_t opposite_goal_mask = 0;
            std::uint32_t strict_opposite_goal_mask = 0;
            for (std::uint32_t slot = 0;
                 slot < goal.slots.size(); ++slot) {
                const std::int8_t slot_side =
                    goal_slot_side(session, goal.slots[slot]);
                if (slot_side == side) {
                    side_goal_mask |= 1u << slot;
                } else {
                    opposite_goal_mask |= 1u << slot;
                }
                if (slot_side ==
                    (side == PC_SIDE_PREFIX
                         ? PC_SIDE_SUFFIX
                         : PC_SIDE_PREFIX)) {
                    strict_opposite_goal_mask |= 1u << slot;
                }
            }
            const std::uint32_t missing =
                side_goal_mask & ~satisfied;
            const std::uint32_t preserved =
                opposite_goal_mask & satisfied;
            const std::uint32_t strictly_preserved =
                strict_opposite_goal_mask & satisfied;
            const std::uint8_t count =
                side == PC_SIDE_PREFIX
                    ? state.prefix_count
                    : state.suffix_count;
            const std::uint32_t satisfied_on_side =
                std::popcount(side_goal_mask & satisfied);
            const bool has_unwanted =
                count > satisfied_on_side;
            const bool opens_goal_capacity =
                missing != 0 && count >= cap;
            const bool relevant =
                missing != 0 ||
                (preserved != 0 && has_unwanted) ||
                opens_goal_capacity;
            if (!relevant) continue;
            const auto setup = setup_for_side(side);
            if (!setup.has_value()) continue;
            for (const char* final_id :
                 {"eldritch_annul", "eldritch_chaos"}) {
                const auto final =
                    registry.index_by_id.find(final_id);
                if (final == registry.index_by_id.end()) {
                    continue;
                }
                FixedOptionSpec option;
                option.kind =
                    FixedOptionKind::EldritchSideIntent;
                option.side = side;
                option.action_id = final_id;
                option.setup_action_ids = *setup;
                option.automatic_kind =
                    AutomaticCandidateKind::EldritchSide;
                option.relevant_goal_mask =
                    side_goal_mask | preserved;
                result.push_back(std::move(option));
            }

            /* Eldritch Exalt is narrower than the destructive side actions:
             * it may only spend preserved opposite-side progress to add into
             * real target-side capacity when the carrier-local ordinary pool
             * can still roll an unmet goal on that side. */
            if (count >= cap || strictly_preserved == 0 || missing == 0 ||
                ordinary_exalt_eligible.empty()) {
                continue;
            }
            std::uint32_t rollable_missing = 0;
            for (std::uint32_t slot = 0;
                 slot < goal.slots.size(); ++slot) {
                const std::uint32_t bit = 1u << slot;
                if ((missing & bit) == 0 ||
                    slot >= calc.layout().slots.size() ||
                    !mask_intersects(
                        ordinary_exalt_eligible,
                        calc.layout().slots[slot].satisfying_mask)) {
                    continue;
                }
                rollable_missing |= bit;
            }
            if (rollable_missing == 0) continue;
            const auto final =
                registry.index_by_id.find("eldritch_exalt");
            if (final == registry.index_by_id.end()) continue;
            FixedOptionSpec exalt;
            exalt.kind = FixedOptionKind::EldritchSideIntent;
            exalt.side = side;
            exalt.action_id = "eldritch_exalt";
            exalt.setup_action_ids = *setup;
            exalt.automatic_kind =
                AutomaticCandidateKind::EldritchSide;
            exalt.relevant_goal_mask =
                rollable_missing | strictly_preserved;
            result.push_back(std::move(exalt));
        }
    }

    /* Deterministic Multimod finishes are generated only for pairs of legal
     * permanent goal crafts. The fixed kernel retains native group, crafted
     * count, replacement, and open-side legality. */
    const auto multimod_entry = std::find_if(
            registry.index_by_id.begin(), registry.index_by_id.end(),
            [&](const auto& entry) {
                const ActionDescriptor& action = registry.actions[entry.second];
                return action.params.type == ActionType::Bench &&
                       action.params.mod_id < session.metamod_type.size() &&
                       session.metamod_type[action.params.mod_id] ==
                           session.data->metamod_multimod_code;
            });
    if (multimod_entry != registry.index_by_id.end() &&
        action_legal(
            session, registry.actions.at(multimod_entry->second), state)) {
        for (std::size_t a = 0; a < goal_bench.size(); ++a) {
            for (std::size_t b = a + 1; b < goal_bench.size(); ++b) {
                const ActionDescriptor& left = registry.actions[goal_bench[a]];
                const ActionDescriptor& right = registry.actions[goal_bench[b]];
                const std::uint32_t mask =
                    goal_mask_for_mod(session, goal, left.params.mod_id) |
                    goal_mask_for_mod(session, goal, right.params.mod_id);
                if ((mask & (mask - 1)) == 0 ||
                    (mask & ~satisfied_goal_mask(state)) == 0 ||
                    mods_conflict(
                        session, left.params.mod_id, right.params.mod_id)) {
                    continue;
                }
                FixedOptionSpec option;
                option.kind = FixedOptionKind::MultimodFinish;
                option.bench_craft_ids = {left.id, right.id};
                option.automatic_kind =
                    AutomaticCandidateKind::MultimodFinish;
                option.relevant_goal_mask = mask;
                result.push_back(std::move(option));
            }
        }
    }

    const auto cleanup = registry.index_by_id.find(
        "remove_crafted_modifiers");
    const bool cleanup_before_setup =
        state_has_unfractured_crafted(state) &&
        state.crafted_goal_mask == 0;
    if (cleanup != registry.index_by_id.end() &&
        action_has_prices(cleanup->second) &&
        (!state_has_unfractured_crafted(state) || cleanup_before_setup)) {
        pc_item_state temporary_carrier = carrier;
        if (cleanup_before_setup) {
            for (int side : {PC_SIDE_PREFIX, PC_SIDE_SUFFIX}) {
                pc_mod_slot* mods = side == PC_SIDE_PREFIX
                                        ? temporary_carrier.prefixes
                                        : temporary_carrier.suffixes;
                std::uint8_t& count = side == PC_SIDE_PREFIX
                                          ? temporary_carrier.prefix_count
                                          : temporary_carrier.suffix_count;
                for (std::uint8_t i = count; i > 0; --i) {
                    if ((mods[i - 1].flags & PC_MOD_SLOT_CRAFTED) != 0) {
                        pc_item_remove_at(&temporary_carrier, side, i - 1);
                    }
                }
            }
        }
        const auto enumeration_started = std::chrono::steady_clock::now();
        const auto& precompiled = calc.temporary_bench_effect_classes();
        synthesis.temporary_precompiled_classes = precompiled.size();
        std::map<
            std::tuple<
                std::uint32_t,
                std::uint32_t,
                std::int8_t,
                bool,
                std::vector<std::uint64_t>>,
            std::size_t>
            group_by_effect;
        std::unordered_map<std::uint32_t, std::vector<std::uint64_t>>
            eligible_by_followup;
        for (const TemporaryBenchEffectClass& effect : precompiled) {
            if (!target_slot_missing(state, effect.goal_slot) ||
                !action_has_prices(effect.followup_action) ||
                !action_legal(
                    session, registry.actions.at(effect.followup_action),
                    state) ||
                !temporary_blocker_applies(
                    session, temporary_carrier, effect)) {
                continue;
            }
            const auto blocker_usable = [&](const std::uint32_t blocker) {
                return action_has_prices(blocker) &&
                       action_legal(
                           session, registry.actions.at(blocker), state);
            };
            const auto representative = std::find_if(
                effect.blocker_actions.begin(),
                effect.blocker_actions.end(), blocker_usable);
            if (representative == effect.blocker_actions.end()) continue;
            synthesis.temporary_candidate_variants +=
                static_cast<std::uint64_t>(std::count_if(
                    representative, effect.blocker_actions.end(),
                    blocker_usable));
            auto [eligible, inserted] = eligible_by_followup.try_emplace(
                effect.followup_action);
            if (inserted) {
                eligible->second = calc.temporary_followup_eligible_mask(
                    temporary_carrier, effect.followup_action);
            }
            if (!mask_intersects(eligible->second, effect.target_mask)) {
                continue;
            }
            std::vector<std::uint64_t> blocked(session.words, 0);
            for (std::size_t word = 0; word < session.words; ++word) {
                blocked[word] = effect.conflict_mask[word] &
                                eligible->second[word];
            }
            const ActionDescriptor& followup =
                registry.actions.at(effect.followup_action);
            std::uint8_t followup_rarity = carrier.rarity;
            if (followup.params.type == ActionType::Regal) {
                followup_rarity = PC_RARITY_RARE;
            }
            const std::uint8_t cap =
                rarity_affix_cap(session, followup_rarity);
            const bool closes_blocker_side =
                (effect.blocker_side == PC_SIDE_PREFIX &&
                 temporary_carrier.prefix_count + 1 >= cap) ||
                (effect.blocker_side == PC_SIDE_SUFFIX &&
                 temporary_carrier.suffix_count + 1 >= cap);
            if (closes_blocker_side) {
                const std::vector<std::uint64_t>& side_mask =
                    effect.blocker_side == PC_SIDE_PREFIX
                        ? session.prefix_mask
                        : session.suffix_mask;
                for (std::size_t word = 0; word < session.words; ++word) {
                    blocked[word] |= eligible->second[word] & side_mask[word];
                }
            }
            if (!mask_has_any(blocked) ||
                mask_intersects(blocked, effect.target_mask)) {
                continue;
            }
            auto [found, effect_inserted] = group_by_effect.emplace(
                std::make_tuple(
                    effect.followup_action, effect.goal_slot,
                    effect.blocker_side, effect.pool_tag_blocker, blocked),
                synthesis.temporary_groups.size());
            TemporaryBenchCandidateGroup* group = nullptr;
            if (effect_inserted) {
                TemporaryBenchCandidateGroup created;
                created.representative_blocker =
                    *representative;
                created.followup_action = effect.followup_action;
                created.goal_slot = effect.goal_slot;
                created.pool_tag_blocker = effect.pool_tag_blocker;
                synthesis.temporary_groups.push_back(std::move(created));
                group = &synthesis.temporary_groups.back();
            } else {
                group = &synthesis.temporary_groups.at(found->second);
            }
            for (const std::uint32_t blocker : effect.blocker_actions) {
                if (!blocker_usable(blocker)) continue;
                const auto duplicate = std::find_if(
                    group->blocker_variants.begin(),
                    group->blocker_variants.end(),
                    [&](const std::uint32_t existing) {
                        return registry.actions.at(existing).cost_keys ==
                               registry.actions.at(blocker).cost_keys;
                    });
                if (duplicate == group->blocker_variants.end()) {
                    group->blocker_variants.push_back(blocker);
                }
            }
        }
        synthesis.temporary_effect_classes =
            synthesis.temporary_groups.size();
        synthesis.temporary_collapsed_variants =
            synthesis.temporary_candidate_variants >
                    synthesis.temporary_effect_classes
                ? synthesis.temporary_candidate_variants -
                      synthesis.temporary_effect_classes
                : 0;
        for (const TemporaryBenchCandidateGroup& group :
             synthesis.temporary_groups) {
            const ActionDescriptor& blocker =
                registry.actions.at(group.representative_blocker);
            const ActionDescriptor& followup =
                registry.actions.at(group.followup_action);
            FixedOptionSpec option;
            option.kind = FixedOptionKind::TemporaryBenchRepeat;
            option.setup_action_ids = {blocker.id};
            if (cleanup_before_setup) {
                option.program_action_ids = {
                    "remove_crafted_modifiers"};
            }
            option.action_id = followup.id;
            option.exit_goal_slots = {group.goal_slot};
            option.exit_min_satisfied = 1;
            option.automatic_kind = group.pool_tag_blocker
                ? AutomaticCandidateKind::CannotRoll
                : AutomaticCandidateKind::TemporaryBenchBlocker;
            option.relevant_goal_mask = 1u << group.goal_slot;
            result.push_back(std::move(option));
        }
        synthesis.temporary_enumeration_ns = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - enumeration_started)
                .count());
    }

    /* Protected candidates are bounded to the exact S7 side-lock programs
     * and only actions whose native preservation facts say the lock matters.
     * Fossils and Essences therefore cannot enter this set. */
    std::vector<std::vector<std::uint64_t>> reachable_by_followup(
        registry.actions.size());
    std::vector<std::uint8_t> reachable_computed(
        registry.actions.size(), 0);
    const auto followup_can_produce = [&](const std::uint32_t followup,
                                          const std::uint32_t slot) {
        if (!reachable_computed.at(followup)) {
            reachable_by_followup.at(followup) =
                action_explicit_affix_reachable_mask(
                    session, registry.actions.at(followup));
            reachable_computed.at(followup) = 1;
        }
        return mask_intersects(
            reachable_by_followup.at(followup),
            calc.layout().slots.at(slot).satisfying_mask);
    };
    for (const std::int8_t side : {static_cast<std::int8_t>(PC_SIDE_PREFIX),
                                   static_cast<std::int8_t>(PC_SIDE_SUFFIX)}) {
        const int lock_code =
            side == PC_SIDE_PREFIX
                ? session.data->metamod_prefixes_locked_code
                : session.data->metamod_suffixes_locked_code;
        const auto lock_entry = std::find_if(
            registry.actions.begin(), registry.actions.end(),
            [&](const ActionDescriptor& action) {
                return action.params.type == ActionType::Bench &&
                       action.params.mod_id < session.metamod_type.size() &&
                       session.metamod_type[action.params.mod_id] == lock_code;
            });
        if (lock_entry == registry.actions.end()) continue;
        const std::uint32_t lock_action = static_cast<std::uint32_t>(
            lock_entry - registry.actions.begin());
        const std::uint32_t lock_flag =
            side == PC_SIDE_PREFIX ? kFlagPrefixesLocked
                                   : kFlagSuffixesLocked;
        if ((state.flags & lock_flag) != 0 ||
            !action_legal(
                session, registry.actions.at(lock_action), state)) {
            continue;
        }
        std::uint32_t protected_mask = 0;
        for (std::uint32_t slot = 0; slot < goal.slots.size(); ++slot) {
            if (goal_slot_side(session, goal.slots[slot]) == side) {
                protected_mask |= 1u << slot;
            }
        }
        if (protected_mask == 0) continue;
        if ((satisfied_goal_mask(state) & protected_mask) == 0) continue;
        for (std::uint32_t followup_index = 0;
             followup_index < registry.actions.size(); ++followup_index) {
            const ActionDescriptor& followup =
                registry.actions[followup_index];
            const bool respects =
                followup.params.type == ActionType::Scour ||
                (side == PC_SIDE_PREFIX
                     ? followup.preservation.respects_prefix_lock
                     : followup.preservation.respects_suffix_lock);
            if (!respects || !calc_supports(followup) ||
                (followup.params.type != ActionType::Scour &&
                 !approved_renewal_roll(followup))) {
                continue;
            }
            for (std::uint32_t slot = 0; slot < goal.slots.size(); ++slot) {
                const std::uint32_t target = 1u << slot;
                if ((protected_mask & target) != 0 ||
                    !target_slot_missing(state, slot)) {
                    continue;
                }
                /* A repeat can advance only if its exact engine-owned
                 * explicit-affix universe intersects the satisfying tier
                 * partition. In particular, a natural reforge cannot produce
                 * a crafted-only goal family. Protected Scour is preparation,
                 * not a repeat-to-target, and remains admitted below. */
                if (followup.params.type != ActionType::Scour &&
                    !followup_can_produce(followup_index, slot)) {
                    continue;
                }
                FixedOptionSpec option;
                option.kind = followup.params.type == ActionType::Scour
                                  ? FixedOptionKind::ProtectedSide
                                  : FixedOptionKind::ProtectedRepeat;
                option.side = side;
                option.action_id = followup.id;
                if (option.kind == FixedOptionKind::ProtectedRepeat) {
                    option.exit_goal_slots = {slot};
                    option.exit_min_satisfied = 1;
                }
                option.automatic_kind =
                    AutomaticCandidateKind::ProtectedMetamod;
                option.relevant_goal_mask = protected_mask | target;
                result.push_back(std::move(option));
            }
        }
    }
    return synthesis;
}

std::string exit_suffix(const FixedOptionSpec& spec) {
    std::string value = ":until:" +
                        std::to_string(spec.exit_min_satisfied) + ":";
    for (std::size_t i = 0; i < spec.exit_goal_slots.size(); ++i) {
        if (i > 0) value += '+';
        value += std::to_string(spec.exit_goal_slots[i]);
    }
    return value;
}

void resolve_renewal_program(
    const ActionRegistry& registry,
    const FixedOptionSpec& spec,
    std::vector<std::uint32_t>& out,
    const bool allow_unveil) {
    if (spec.program_action_ids.empty() ||
        spec.program_action_ids.size() > (allow_unveil ? 3u : 2u)) {
        throw std::runtime_error(
            "fixed option: renewal program has an unsupported length");
    }
    for (const std::string& id : spec.program_action_ids) {
        std::uint32_t index = kNoId;
        require_action(registry, id, index);
        out.push_back(index);
    }
    std::size_t effective = out.size();
    if (action_observes_modifier_offer(
            registry.actions.at(out.back()))) {
        if (!allow_unveil || effective < 2) {
            throw std::runtime_error(
                "fixed option: observed choice is allowed only as the final "
                "renewal step");
        }
        --effective;
    }
    const bool one_roll = effective == 1 &&
                          approved_renewal_roll(
                              registry.actions.at(out.front()));
    const bool scour_alchemy =
        effective == 2 &&
        registry.actions.at(out[0]).params.type == ActionType::Scour &&
        registry.actions.at(out[1]).params.type == ActionType::Alchemy;
    if (!one_roll && !scour_alchemy) {
        throw std::runtime_error(
            "fixed option: renewal preparation must be Alteration, Chaos, "
            "Essence, Fossil, Harvest reforge, Veiled Chaos, or explicit "
            "Scour then Alchemy");
    }
    if (effective != out.size() &&
        registry.actions.at(out[effective - 1]).params.type !=
            ActionType::VeiledChaos) {
        throw std::runtime_error(
            "fixed option: observed Unveil renewal requires Veiled Chaos");
    }
}

void resolve_imprint_program(
    const ActionRegistry& registry,
    const FixedOptionSpec& spec,
    std::vector<std::uint32_t>& out) {
    if (spec.program_action_ids.empty()) {
        throw std::runtime_error(
            "fixed option: imprint retry program must be non-empty");
    }
    for (const std::string& id : spec.program_action_ids) {
        std::uint32_t index = kNoId;
        const ActionDescriptor& action = require_action(registry, id, index);
        if (action.synthetic || action.uses_companion_state ||
            action_observes_modifier_offer(action) ||
            !calc_supports(action)) {
            throw std::runtime_error(
                "fixed option: imprint retry action is not an exact ordinary "
                "primitive: " + id);
        }
        out.push_back(index);
    }
}

std::uint32_t require_bestiary_action(
    const SessionImpl& session,
    const char* id) {
    const auto found = session.data->bestiary_action_by_id.find(id);
    if (found == session.data->bestiary_action_by_id.end()) {
        throw std::runtime_error(
            std::string("fixed option: missing native Bestiary action: ") + id);
    }
    return found->second;
}

bool option_exit_matches(
    const AbstractState& state,
    const PlannerOperator& option) {
    std::uint32_t satisfied = 0;
    for (const std::uint32_t slot : option.exit_goal_slots) {
        if (slot < kMaxGoalSlots &&
            state.slot_status[slot] == static_cast<std::uint8_t>(
                GoalSlotStatus::Satisfied)) {
            ++satisfied;
        }
    }
    return satisfied >= option.exit_min_satisfied;
}

struct AttemptKernel {
    bool supported = true;
    bool fully_legal = true;
    double expected_primitive_actions = 0.0;
    std::vector<std::pair<std::string, double>> expected_resources;
    std::vector<OutcomeEntry> entries;
    std::vector<OutcomeChoiceGroup> choice_groups;
    std::vector<OutcomeChoiceOption> choice_options;
    std::uint64_t action_state_evaluations = 0;
    std::uint64_t outcomes_merged = 0;
    std::uint64_t max_atomic_outcomes_ns = 0;
    /* Transient exact certificate used only while discovering one automatic
     * Imprint envelope. When every positive-mass carrier reaching the final
     * action has bit-identical canonical outcomes, retain that exact kernel.
     * It is never serialized, hashed, cached, or adopted by an operator. */
    std::vector<OutcomeEntry> terminal_uniform_entries;
};

std::uint64_t imprint_cursor_add(
    const std::uint64_t left,
    const std::uint64_t right) {
    return right > std::numeric_limits<std::uint64_t>::max() - left
               ? std::numeric_limits<std::uint64_t>::max()
               : left + right;
}

std::uint64_t imprint_attempt_kernel_nested_bytes(
    const AttemptKernel& attempt) {
    std::uint64_t bytes =
        attempt.expected_resources.capacity() *
            sizeof(std::pair<std::string, double>) +
        attempt.entries.capacity() * sizeof(OutcomeEntry) +
        attempt.terminal_uniform_entries.capacity() * sizeof(OutcomeEntry) +
        attempt.choice_groups.capacity() * sizeof(OutcomeChoiceGroup) +
        attempt.choice_options.capacity() * sizeof(OutcomeChoiceOption);
    for (const auto& [key, unused_quantity] :
         attempt.expected_resources) {
        (void)unused_quantity;
        bytes = imprint_cursor_add(bytes, key.capacity() + 1);
    }
    for (const OutcomeChoiceGroup& group : attempt.choice_groups) {
        bytes = imprint_cursor_add(
            bytes,
            group.states.capacity() * sizeof(std::uint32_t));
    }
    return bytes;
}

std::uint64_t imprint_resource_map_bytes(
    const std::map<std::string, double>& resources) {
    std::uint64_t bytes = resources.size() *
        (sizeof(std::pair<const std::string, double>) +
         3 * sizeof(void*));
    for (const auto& [key, unused_quantity] : resources) {
        (void)unused_quantity;
        bytes = imprint_cursor_add(bytes, key.capacity() + 1);
    }
    return bytes;
}

/*
 * One Imprint program can fan out through hundreds of thousands of exact
 * Calculator states even though the outer discovery budget counts it as one
 * program. Keep that fanout as flat, trivially destructible storage and
 * checkpoint its state/outcome aggregation. This makes cancellation cheap
 * and preserves the old std::map numeric order: active states and final
 * outputs remain ascending by state id, and contributions are accumulated in
 * the same active-state/distribution-entry order.
 *
 * calc.outcomes() remains one atomic exact-kernel leaf. The caller records its
 * maximum duration so a fixture can prove whether the deeper reforge cursor
 * is needed; every potentially much larger merge/scan around it is resumable.
 */
solve_detail::CooperativeTask<AttemptKernel>
execute_attempt_cooperatively(
    CalcContext& calc,
    std::vector<std::uint32_t> program,
    std::vector<OutcomeEntry> entry_support) {
    AttemptKernel result;
    std::map<std::string, double> resources;
    std::vector<std::pair<std::uint32_t, double>> active;
    active.reserve(entry_support.size());
    for (const OutcomeEntry& entry : entry_support) {
        active.push_back({entry.state, entry.probability});
    }
    std::vector<std::pair<std::uint32_t, double>> next_active;
    std::vector<double> next_probability;
    std::vector<std::uint32_t> next_generation;
    std::vector<double> stopped_probability;
    std::uint32_t generation = 0;
    const auto retained_bytes = [&]() {
        std::uint64_t bytes =
            program.capacity() * sizeof(std::uint32_t);
        bytes = imprint_cursor_add(
            bytes,
            entry_support.capacity() * sizeof(OutcomeEntry));
        bytes = imprint_cursor_add(
            bytes, imprint_attempt_kernel_nested_bytes(result));
        bytes = imprint_cursor_add(
            bytes, imprint_resource_map_bytes(resources));
        bytes = imprint_cursor_add(
            bytes,
            active.capacity() * sizeof(decltype(active)::value_type));
        bytes = imprint_cursor_add(
            bytes,
            next_active.capacity() *
                sizeof(decltype(next_active)::value_type));
        bytes = imprint_cursor_add(
            bytes, next_probability.capacity() * sizeof(double));
        bytes = imprint_cursor_add(
            bytes,
            next_generation.capacity() * sizeof(std::uint32_t));
        return imprint_cursor_add(
            bytes, stopped_probability.capacity() * sizeof(double));
    };
    constexpr std::size_t kCheckpointItems = 128;
    if (program.size() == 1 && entry_support.size() > 1) {
        const std::uint32_t action_index = program.front();
        const ActionDescriptor& action =
            calc.registry().actions.at(action_index);
        std::vector<std::uint64_t> shared_signature;
        std::vector<std::uint64_t> candidate_signature;
        bool shared_reforge_kernel = true;
        double active_probability = 0.0;
        for (std::size_t entry_index = 0;
             entry_index < entry_support.size(); ++entry_index) {
            const OutcomeEntry& entry = entry_support[entry_index];
            if (entry.probability <= 0.0) continue;
            if (!action_legal(
                    calc.session(), action, calc.state(entry.state))) {
                result.fully_legal = false;
                co_return result;
            }
            if (!calc.exact_reforge_kernel_signature(
                    entry.state, action_index, candidate_signature)) {
                shared_reforge_kernel = false;
                break;
            }
            if (shared_signature.empty()) {
                shared_signature = candidate_signature;
            } else if (candidate_signature != shared_signature) {
                shared_reforge_kernel = false;
                break;
            }
            active_probability += entry.probability;
            if ((entry_index + 1) % kCheckpointItems == 0) {
                co_await solve_detail::CooperativeCheckpoint{
                    retained_bytes()};
            }
        }
        if (shared_reforge_kernel && !shared_signature.empty()) {
            result.expected_primitive_actions = active_probability;
            for (const std::string& key : action.cost_keys) {
                resources[key] += active_probability;
            }
            const auto outcomes_started =
                std::chrono::steady_clock::now();
            const OutcomeDistribution& distribution = calc.outcomes(
                entry_support.front().state, action_index);
            result.max_atomic_outcomes_ns = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() - outcomes_started)
                    .count());
            result.action_state_evaluations = 1;
            if (!distribution.supported ||
                !distribution.choice_groups.empty() ||
                !distribution.choice_options.empty()) {
                result.supported = false;
                co_return result;
            }
            result.entries = distribution.entries;
            result.terminal_uniform_entries = distribution.entries;
            result.outcomes_merged = distribution.entries.size();
            result.expected_resources.assign(
                resources.begin(), resources.end());
            co_return result;
        }
    }
    for (std::size_t step = 0; step < program.size(); ++step) {
        const std::uint32_t action_index = program[step];
        const ActionDescriptor& action =
            calc.registry().actions.at(action_index);
        const bool observed = action_observes_modifier_offer(action);
        if (observed && step + 1 != program.size()) {
            result.supported = false;
            break;
        }
        if (++generation == 0) {
            std::fill(
                next_generation.begin(), next_generation.end(), 0);
            generation = 1;
        }
        next_active.clear();
        const bool final_step = step + 1 == program.size();
        result.terminal_uniform_entries.clear();
        bool terminal_kernel_initialized = false;
        bool terminal_kernel_uniform = final_step;
        for (std::size_t active_index = 0;
             active_index < active.size(); ++active_index) {
            co_await solve_detail::CooperativeCheckpoint{
                retained_bytes()};
            const auto [state_id, path_probability] =
                active[active_index];
            if (!action_legal(calc.session(), action, calc.state(state_id))) {
                result.fully_legal = false;
                /* Discovery publishes only programs legal for every
                 * positive-mass carrier. One counterexample is therefore a
                 * complete rejection certificate; scanning the remaining
                 * (occasionally hundreds of thousands of) carriers cannot
                 * change admission. */
                co_return result;
            }
            result.expected_primitive_actions += path_probability;
            for (const std::string& key : action.cost_keys) {
                resources[key] += path_probability;
            }
            const auto outcomes_started =
                std::chrono::steady_clock::now();
            const OutcomeDistribution& distribution =
                calc.outcomes(state_id, action_index);
            const std::uint64_t outcomes_ns =
                static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now() -
                        outcomes_started)
                        .count());
            result.action_state_evaluations = imprint_cursor_add(
                result.action_state_evaluations, 1);
            result.max_atomic_outcomes_ns = std::max(
                result.max_atomic_outcomes_ns, outcomes_ns);
            const std::size_t state_count = calc.state_count();
            if (next_probability.size() < state_count) {
                next_probability.resize(state_count, 0.0);
                next_generation.resize(state_count, 0);
            }
            if (stopped_probability.size() < state_count) {
                stopped_probability.resize(state_count, 0.0);
            }
            co_await solve_detail::CooperativeCheckpoint{
                retained_bytes()};
            if (!distribution.supported ||
                (!observed && !distribution.choice_groups.empty())) {
                result.supported = false;
                break;
            }
            if (final_step && terminal_kernel_uniform) {
                if (!distribution.choice_groups.empty() ||
                    !distribution.choice_options.empty() ||
                    distribution.entries.empty()) {
                    terminal_kernel_uniform = false;
                    result.terminal_uniform_entries.clear();
                } else if (!terminal_kernel_initialized) {
                    result.terminal_uniform_entries = distribution.entries;
                    terminal_kernel_initialized = true;
                } else if (result.terminal_uniform_entries !=
                           distribution.entries) {
                    terminal_kernel_uniform = false;
                    result.terminal_uniform_entries.clear();
                }
            }
            if (observed && !distribution.choice_groups.empty()) {
                for (std::size_t group_index = 0;
                     group_index < distribution.choice_groups.size();
                     ++group_index) {
                    const OutcomeChoiceGroup& group =
                        distribution.choice_groups[group_index];
                    result.choice_groups.push_back(
                        {path_probability * group.probability,
                         group.states, state_id});
                    result.outcomes_merged = imprint_cursor_add(
                        result.outcomes_merged, group.states.size());
                    co_await solve_detail::CooperativeCheckpoint{
                        retained_bytes()};
                }
                for (std::size_t choice_index = 0;
                     choice_index < distribution.choice_options.size();
                     ++choice_index) {
                    const OutcomeChoiceOption& choice =
                        distribution.choice_options[choice_index];
                    result.choice_options.push_back(
                        {choice.mod_id, choice.state, state_id,
                         choice.state});
                    result.outcomes_merged = imprint_cursor_add(
                        result.outcomes_merged, 1);
                    if ((choice_index + 1) % kCheckpointItems == 0) {
                        co_await solve_detail::CooperativeCheckpoint{
                            retained_bytes()};
                    }
                }
            } else {
                for (std::size_t outcome_index = 0;
                     outcome_index < distribution.entries.size();
                     ++outcome_index) {
                    const OutcomeEntry& outcome =
                        distribution.entries[outcome_index];
                    if (outcome.state >= next_probability.size()) {
                        const std::size_t needed =
                            static_cast<std::size_t>(outcome.state) + 1;
                        next_probability.resize(needed, 0.0);
                        next_generation.resize(needed, 0);
                        stopped_probability.resize(needed, 0.0);
                    }
                    if (next_generation[outcome.state] != generation) {
                        next_generation[outcome.state] = generation;
                        next_probability[outcome.state] = 0.0;
                    }
                    next_probability[outcome.state] +=
                        path_probability * outcome.probability;
                    result.outcomes_merged = imprint_cursor_add(
                        result.outcomes_merged, 1);
                    if ((outcome_index + 1) % kCheckpointItems == 0) {
                        co_await solve_detail::CooperativeCheckpoint{
                            retained_bytes()};
                    }
                }
            }
        }
        if (!result.supported) break;
        if (!(final_step && terminal_kernel_uniform &&
              terminal_kernel_initialized)) {
            result.terminal_uniform_entries.clear();
        }
        for (std::size_t state_id = 0;
             state_id < next_generation.size(); ++state_id) {
            if (next_generation[state_id] == generation) {
                next_active.push_back({
                    static_cast<std::uint32_t>(state_id),
                    next_probability[state_id]});
            }
            if ((state_id + 1) % kCheckpointItems == 0) {
                co_await solve_detail::CooperativeCheckpoint{
                    retained_bytes()};
            }
        }
        active.swap(next_active);
    }
    if (result.supported) {
        if (result.fully_legal &&
            !result.terminal_uniform_entries.empty()) {
            /* Exact algebraic canonicalization: composing any probability
             * distribution with a carrier-independent terminal kernel K is
             * K. Use the Calculator's canonical entries directly instead of
             * retaining roundoff from summing p(carrier) * K repeatedly. */
            result.entries = result.terminal_uniform_entries;
        } else {
            std::size_t active_cursor = 0;
            const std::size_t state_count = std::max(
                stopped_probability.size(),
                active.empty()
                    ? std::size_t{0}
                    : static_cast<std::size_t>(active.back().first) + 1);
            for (std::size_t state_id = 0;
                 state_id < state_count; ++state_id) {
                double probability = state_id < stopped_probability.size()
                                         ? stopped_probability[state_id]
                                         : 0.0;
                if (active_cursor < active.size() &&
                    active[active_cursor].first == state_id) {
                    probability += active[active_cursor].second;
                    ++active_cursor;
                }
                if (probability != 0.0) {
                    result.entries.push_back({
                        static_cast<std::uint32_t>(state_id), probability});
                }
                if ((state_id + 1) % kCheckpointItems == 0) {
                    co_await solve_detail::CooperativeCheckpoint{
                        retained_bytes()};
                }
            }
        }
        result.expected_resources.assign(resources.begin(), resources.end());
    }
    co_return result;
}

AttemptKernel execute_attempt(
    CalcContext& calc,
    const std::vector<std::uint32_t>& program,
    const std::uint32_t entry_state) {
    auto task = execute_attempt_cooperatively(
        calc, program, std::vector<OutcomeEntry>{{entry_state, 1.0}});
    while (!task.resume()) {
    }
    AttemptKernel result = task.take_result();
    task.reset();
    return result;
}

bool same_attempt(
    const AttemptKernel& left,
    const AttemptKernel& right) {
    return left.supported == right.supported &&
           left.fully_legal == right.fully_legal &&
           left.expected_primitive_actions ==
               right.expected_primitive_actions &&
           left.expected_resources == right.expected_resources &&
           left.entries == right.entries &&
           left.choice_groups == right.choice_groups &&
           left.choice_options == right.choice_options;
}

bool same_attempt_outcomes(
    const AttemptKernel& left,
    const AttemptKernel& right) {
    return left.supported == right.supported &&
           left.fully_legal == right.fully_legal &&
           left.entries == right.entries &&
           left.choice_groups == right.choice_groups &&
           left.choice_options == right.choice_options;
}

std::uint64_t attempt_kernel_hash(const AttemptKernel& attempt) {
    std::uint64_t hash = 1469598103934665603ull;
    auto mix = [&](const std::uint64_t value) {
        hash ^= value;
        hash *= 1099511628211ull;
    };
    mix(attempt.supported ? 1u : 0u);
    mix(attempt.fully_legal ? 1u : 0u);
    for (const OutcomeEntry& entry : attempt.entries) {
        mix(entry.state);
        mix(std::bit_cast<std::uint64_t>(entry.probability));
    }
    for (const OutcomeChoiceGroup& group : attempt.choice_groups) {
        mix(std::bit_cast<std::uint64_t>(group.probability));
        mix(group.observation_state);
        for (const std::uint32_t state : group.states) mix(state);
        mix(kNoId);
    }
    for (const OutcomeChoiceOption& choice : attempt.choice_options) {
        mix(choice.mod_id);
        mix(choice.state);
        mix(choice.observation_state);
        mix(choice.actual_state);
    }
    return hash;
}

struct ImprintDiscoveryResult {
    std::vector<FixedOptionSpec> specs;
    /* Bounded evidence for a mechanically open depth frontier. These are
     * diagnostic witnesses only; they never participate in admission or
     * cache identity. */
    std::vector<std::string> depth_deferred_samples;
    bool missing_price = false;
    bool depth_deferred = false;
    bool work_deferred = false;
    std::uint32_t depth_limit = 0;
    std::uint64_t work_limit = 0;
    std::uint64_t work_used = 0;
    std::uint64_t programs_pruned = 0;
    std::uint64_t distribution_dominated_programs = 0;
    std::uint64_t price_pruned_programs = 0;
    std::uint64_t price_bound_max_program_depth = 0;
    std::uint64_t max_evaluated_depth = 0;
    std::uint64_t max_frontier_size = 0;
    bool price_bound_complete = false;
    std::uint64_t action_state_evaluations = 0;
    std::uint64_t outcomes_merged = 0;
    std::uint64_t max_atomic_outcomes_ns = 0;
};

struct ImprintProgramPrefix {
    std::vector<std::uint32_t> program;
    /* Exact canonical carrier distribution after this prefix. Extending the
     * retained kernel by one action is bit-identical to replaying the whole
     * program because execute_attempt_cooperatively publishes active states
     * in ascending state-id order after every step. */
    std::shared_ptr<const std::vector<OutcomeEntry>> support;
    /* A deliberately downward-rounded lower bound on the fixed resource
     * price paid by every fully legal execution of this prefix. */
    double scalar_cost_lower = 0.0;
};

struct ImprintKernelParetoRepresentative {
    std::uint64_t kernel_hash = 0;
    /* Exact primitive multiplicities, sorted by registry action index. A
     * componentwise-smaller word consumes no more of any primitive resource
     * under every economy; unlike a rounded scalar price, this remains a
     * mechanically exact subtree-dominance certificate. */
    std::vector<std::pair<std::uint32_t, std::uint64_t>> primitive_counts;
    std::shared_ptr<const std::vector<OutcomeEntry>> entries;
};

struct ImprintScalarPrice {
    double exact = 0.0;
    double lower = 0.0;
    double upper = 0.0;
    bool finite_nonnegative = true;
};

struct ImprintProducerLower {
    /* Mechanical possibility is independent of whether the owning resource
     * has a usable scalar price. Conflating the two would falsely close an
     * open grammar when a possible producer has a missing/nonfinite price. */
    bool mechanically_possible = false;
    double priced_lower = std::numeric_limits<double>::infinity();
};

double imprint_downward_add(
    const double left,
    const double right) {
    if (right == 0.0) return left;
    const double sum = left + right;
    if (!std::isfinite(sum)) return sum;
    return std::nextafter(
        sum, -std::numeric_limits<double>::infinity());
}

double imprint_upward_add(
    const double left,
    const double right) {
    if (right == 0.0) return left;
    const double sum = left + right;
    if (!std::isfinite(sum)) return sum;
    return std::nextafter(
        sum, std::numeric_limits<double>::infinity());
}

ImprintScalarPrice imprint_price_keys(
    const std::vector<std::string>& keys,
    const std::unordered_map<std::string, double>* prices) {
    ImprintScalarPrice result;
    if (prices == nullptr) {
        result.finite_nonnegative = false;
        return result;
    }
    for (const std::string& key : keys) {
        const auto found = prices->find(key);
        if (found == prices->end() || !std::isfinite(found->second) ||
            found->second < 0.0) {
            result.finite_nonnegative = false;
            return result;
        }
        result.exact += found->second;
        result.lower = imprint_downward_add(
            result.lower, found->second);
        result.upper = imprint_upward_add(
            result.upper, found->second);
        if (!std::isfinite(result.exact) ||
            !std::isfinite(result.lower) ||
            !std::isfinite(result.upper)) {
            result.finite_nonnegative = false;
            return result;
        }
    }
    return result;
}

std::vector<std::pair<std::uint32_t, std::uint64_t>>
imprint_primitive_counts(const std::vector<std::uint32_t>& program) {
    std::map<std::uint32_t, std::uint64_t> counts;
    for (const std::uint32_t action : program) {
        std::uint64_t& count = counts[action];
        if (count == std::numeric_limits<std::uint64_t>::max()) {
            throw std::overflow_error(
                "Imprint primitive multiplicity exceeds uint64");
        }
        ++count;
    }
    return {counts.begin(), counts.end()};
}

bool imprint_primitive_counts_dominate(
    const std::vector<std::pair<std::uint32_t, std::uint64_t>>& left,
    const std::vector<std::pair<std::uint32_t, std::uint64_t>>& right) {
    std::size_t right_offset = 0;
    for (const auto& [left_action, left_count] : left) {
        while (right_offset < right.size() &&
               right[right_offset].first < left_action) {
            ++right_offset;
        }
        if (right_offset == right.size() ||
            right[right_offset].first != left_action ||
            right[right_offset].second < left_count) {
            return false;
        }
    }
    return true;
}

bool resource_keys_available(
    const std::vector<std::string>& keys,
    const std::unordered_map<std::string, double>* prices) {
    return prices == nullptr || std::all_of(
        keys.begin(), keys.end(), [&](const std::string& key) {
            return prices->contains(key);
        });
}

bool native_imprint_checkpoint_creation_legal(
    CalcContext& calc,
    const std::uint32_t state_id) {
    const auto found = calc.session().data->bestiary_action_by_id.find(
        "bestiary:imprint");
    if (found == calc.session().data->bestiary_action_by_id.end()) {
        return false;
    }
    pc_item_state item;
    if (!calc.materialize(state_id, item)) return false;
    BestiaryCraftState craft;
    craft.item = item;
    craft.live_item_identity = 1;
    return apply_bestiary_action(
               *calc.session().data, craft, found->second).applied;
}

std::uint64_t imprint_fixed_option_nested_bytes(
    const FixedOptionSpec& spec) {
    std::uint64_t bytes = spec.action_id.capacity() + 1;
    bytes = imprint_cursor_add(
        bytes, spec.constructive_finish_action_id.capacity() + 1);
    const auto strings = [&](const std::vector<std::string>& values) {
        std::uint64_t selected =
            values.capacity() * sizeof(std::string);
        for (const std::string& value : values) {
            selected = imprint_cursor_add(
                selected, value.capacity() + 1);
        }
        return selected;
    };
    bytes = imprint_cursor_add(bytes, strings(spec.setup_action_ids));
    bytes = imprint_cursor_add(bytes, strings(spec.bench_craft_ids));
    bytes = imprint_cursor_add(bytes, strings(spec.program_action_ids));
    return imprint_cursor_add(
        bytes,
        spec.exit_goal_slots.capacity() * sizeof(std::uint32_t));
}

std::uint64_t imprint_discovery_result_nested_bytes(
    const ImprintDiscoveryResult& result) {
    std::uint64_t bytes =
        result.specs.capacity() * sizeof(FixedOptionSpec) +
        result.depth_deferred_samples.capacity() * sizeof(std::string);
    for (const FixedOptionSpec& spec : result.specs) {
        bytes = imprint_cursor_add(
            bytes, imprint_fixed_option_nested_bytes(spec));
    }
    for (const std::string& sample : result.depth_deferred_samples) {
        bytes = imprint_cursor_add(bytes, sample.capacity() + 1);
    }
    return bytes;
}

std::uint64_t imprint_program_frontier_bytes(
    const std::vector<ImprintProgramPrefix>& frontier) {
    std::uint64_t bytes =
        frontier.capacity() * sizeof(ImprintProgramPrefix);
    for (const ImprintProgramPrefix& prefix : frontier) {
        bytes = imprint_cursor_add(
            bytes,
            prefix.program.capacity() * sizeof(std::uint32_t));
    }
    return bytes;
}

std::uint64_t imprint_kernel_pareto_bytes(
    const std::vector<ImprintKernelParetoRepresentative>& representatives) {
    std::uint64_t bytes = representatives.capacity() *
        sizeof(ImprintKernelParetoRepresentative);
    for (const ImprintKernelParetoRepresentative& representative :
         representatives) {
        bytes = imprint_cursor_add(
            bytes,
            representative.primitive_counts.capacity() *
                sizeof(std::pair<std::uint32_t, std::uint64_t>));
        bytes = imprint_cursor_add(
            bytes,
            representative.entries == nullptr
                ? 0
                : sizeof(std::vector<OutcomeEntry>) +
                      4 * sizeof(void*) +
                      representative.entries->capacity() *
                          sizeof(OutcomeEntry));
    }
    return bytes;
}

solve_detail::CooperativeTask<ImprintDiscoveryResult>
discover_automatic_imprint_options_cooperatively(
    CalcContext& calc,
    const std::uint32_t state_id,
    AutomaticAdmissionLimits limits) {
    ImprintDiscoveryResult result;
    result.depth_limit = limits.max_imprint_program_depth == 0
                             ? kDefaultImprintProgramDepth
                             : limits.max_imprint_program_depth;
    result.work_limit = limits.max_imprint_program_work == 0
                            ? kDefaultImprintProgramWork
                            : limits.max_imprint_program_work;
    if (!native_imprint_checkpoint_creation_legal(calc, state_id)) {
        co_return result;
    }

    const auto create = calc.session().data->bestiary_action_by_id.find(
        "bestiary:imprint");
    if (create == calc.session().data->bestiary_action_by_id.end()) {
        co_return result;
    }
    if (!resource_keys_available(
            calc.session().data->bestiary_actions.at(create->second).cost_keys,
            limits.prices)) {
        result.missing_price = true;
        co_return result;
    }

    std::vector<std::uint32_t> actions;
    for (const std::uint32_t index : calc.candidates()) {
        const ActionDescriptor& action = calc.registry().actions.at(index);
        if (action.synthetic || action.uses_companion_state ||
            action.product_role == ProductActionRole::AutomaticDependency ||
            action_observes_modifier_offer(action) ||
            !calc_supports(action) ||
            !resource_keys_available(action.cost_keys, limits.prices)) {
            continue;
        }
        actions.push_back(index);
    }
    std::sort(actions.begin(), actions.end(), [&](const std::uint32_t left,
                                                  const std::uint32_t right) {
        return calc.registry().actions.at(left).id <
               calc.registry().actions.at(right).id;
    });
    if (actions.empty()) co_return result;

    const std::uint32_t goal_mask = calc.goal().slots.size() == 32
                                        ? 0xffffffffu
                                        : (1u << calc.goal().slots.size()) - 1u;
    const std::uint32_t missing_mask =
        goal_mask & ~satisfied_goal_mask(calc.state(state_id));
    if (missing_mask == 0) co_return result;

    /* These masks become a carrier-support-aware suffix-cost authority, never
     * an exit gate. A statically nonproducing final action can preserve a goal
     * made by an earlier step, so exact terminal outcomes below always decide
     * exits. For ordinary pool rolls, goal influence membership is combined
     * with exact active carrier bits rather than the base-tag positive mask;
     * the latter can contain an inactive influence row or omit an active
     * selector-only row. */
    std::vector<std::vector<std::uint64_t>> action_base_reachable_masks;
    action_base_reachable_masks.reserve(actions.size());
    for (const std::uint32_t action_index : actions) {
        action_base_reachable_masks.push_back(
            action_explicit_affix_reachable_mask(
                calc.session(),
                calc.registry().actions.at(action_index)));
    }
    bool missing_goal_has_normal_mod = false;
    std::uint8_t missing_goal_influence_bits = 0;
    for (std::uint32_t slot = 0;
         slot < calc.goal().slots.size(); ++slot) {
        if ((missing_mask & (1u << slot)) == 0) continue;
        pc_bitset_for_each(
            calc.layout().slots.at(slot).satisfying_mask.data(),
            calc.session().words,
            [&](const std::size_t mod) {
                const int influence = mod < calc.session().influence_code.size()
                    ? calc.session().influence_code[mod]
                    : -1;
                if (influence > 0 && influence <= 8) {
                    missing_goal_influence_bits |= static_cast<std::uint8_t>(
                        1u << (influence - 1));
                } else {
                    missing_goal_has_normal_mod = true;
                }
            });
    }

    const BestiaryActionDescriptor& create_descriptor =
        calc.session().data->bestiary_actions.at(create->second);
    const ImprintScalarPrice checkpoint_price = imprint_price_keys(
        create_descriptor.cost_keys, limits.prices);
    std::vector<ImprintScalarPrice> action_prices;
    action_prices.reserve(actions.size());
    double minimum_step_cost_lower =
        std::numeric_limits<double>::infinity();
    bool all_steps_strictly_positive = true;
    for (std::size_t offset = 0; offset < actions.size(); ++offset) {
        const ImprintScalarPrice price = imprint_price_keys(
            calc.registry().actions.at(actions[offset]).cost_keys,
            limits.prices);
        action_prices.push_back(price);
        all_steps_strictly_positive &=
            price.finite_nonnegative && price.lower > 0.0;
        if (price.finite_nonnegative && price.lower > 0.0) {
            minimum_step_cost_lower = std::min(
                minimum_step_cost_lower, price.lower);
        }
    }
    const auto minimum_direct_producer_cost_lower =
        [&](const std::vector<OutcomeEntry>& support) {
            std::uint8_t active_influence_bits = 0;
            for (const OutcomeEntry& outcome : support) {
                if (outcome.probability <= 0.0) continue;
                active_influence_bits |=
                    calc.state(outcome.state).influence_bits;
            }
            ImprintProducerLower producer;
            for (std::size_t offset = 0; offset < actions.size(); ++offset) {
                const ActionDescriptor& descriptor =
                    calc.registry().actions.at(actions[offset]);
                const bool direct_mask_intersects = [&] {
                    for (std::uint32_t slot = 0;
                         slot < calc.goal().slots.size(); ++slot) {
                        if ((missing_mask & (1u << slot)) != 0 &&
                            mask_intersects(
                                action_base_reachable_masks[offset],
                                calc.layout().slots.at(slot).satisfying_mask)) {
                            return true;
                        }
                    }
                    return false;
                }();
                bool can_advance = false;
                if (descriptor.params.type == ActionType::InfluenceExalt) {
                    /* Generic influence is monotone throughout the supported
                     * Imprint grammar, and Influence Exalt forbids any active
                     * bit. Mixed influenced/uninfluenced support therefore
                     * cannot execute it as a fully legal common suffix. */
                    can_advance = active_influence_bits == 0 &&
                                  direct_mask_intersects;
                } else if (imprint_action_may_roll_influence_pool(
                               descriptor)) {
                    can_advance = missing_goal_has_normal_mod ||
                        (missing_goal_influence_bits &
                         active_influence_bits) != 0;
                    /* Essence guarantees and Fossil forced modifiers bypass
                     * the ordinary weighted pool. Do not reuse the full
                     * action reach mask here: it also contains the rolled
                     * pool and can include inactive base-positive influence
                     * rows. */
                    std::vector<std::uint32_t> direct_mods;
                    if (descriptor.params.type == ActionType::Essence &&
                        descriptor.params.essence_index <
                            calc.session().essence_guaranteed_mod_ids.size()) {
                        direct_mods.push_back(
                            calc.session().essence_guaranteed_mod_ids[
                                descriptor.params.essence_index]);
                    } else if (descriptor.params.type == ActionType::Fossil) {
                        for (const std::uint32_t fossil :
                             descriptor.params.fossil_indices) {
                            if (fossil >=
                                calc.session().fossil_forced_mod_ids.size()) {
                                continue;
                            }
                            direct_mods.insert(
                                direct_mods.end(),
                                calc.session().fossil_forced_mod_ids[fossil]
                                    .begin(),
                                calc.session().fossil_forced_mod_ids[fossil]
                                    .end());
                        }
                    }
                    for (const std::uint32_t mod : direct_mods) {
                        if (mod == kNoId || mod >= calc.session().mod_count) {
                            continue;
                        }
                        for (std::uint32_t slot = 0;
                             slot < calc.goal().slots.size(); ++slot) {
                            if ((missing_mask & (1u << slot)) != 0 &&
                                pc_bitset_test(
                                    calc.layout().slots.at(slot)
                                        .satisfying_mask.data(),
                                    mod)) {
                                can_advance = true;
                            }
                        }
                    }
                } else {
                    can_advance = direct_mask_intersects;
                }
                producer.mechanically_possible |= can_advance;
                if (can_advance &&
                    action_prices[offset].finite_nonnegative) {
                    producer.priced_lower = std::min(
                        producer.priced_lower,
                        action_prices[offset].lower);
                }
            }
            return producer;
        };
    /* A primitive renewal whose exact preserved-base signature is shared by
     * every non-goal outcome is itself a proper carrier-local policy. Its
     * outward-rounded price divided by a downward-rounded success
     * probability is therefore a certified upper even before this carrier's
     * sparse rows have been materialized. This breaks the admission cycle in
     * which an obviously dominated Imprint search needed a solver incumbent
     * that could only be built after automatic discovery returned. */
    double certified_carrier_upper = limits.incumbent_upper_bound;
    for (std::size_t offset = 0; offset < actions.size(); ++offset) {
        const std::uint32_t action_index = actions[offset];
        const ActionDescriptor& descriptor =
            calc.registry().actions.at(action_index);
        if (!action_transition_facts(descriptor.params.type).renewal ||
            !action_prices[offset].finite_nonnegative ||
            !std::isfinite(action_prices[offset].upper) ||
            action_prices[offset].upper < 0.0 ||
            !action_legal(
                calc.session(), descriptor, calc.state(state_id))) {
            continue;
        }
        std::vector<std::uint64_t> signature;
        if (!calc.exact_reforge_kernel_signature(
                state_id, action_index, signature) ||
            signature.empty()) {
            continue;
        }
        const OutcomeDistribution& renewal =
            calc.outcomes(state_id, action_index);
        if (!renewal.supported || !renewal.stable_shared_kernel ||
            !renewal.choice_groups.empty() ||
            !renewal.choice_options.empty()) {
            continue;
        }
        double success_probability_lower = 0.0;
        bool exact_retry = true;
        std::vector<std::uint32_t> progress_states;
        for (const OutcomeEntry& outcome : renewal.entries) {
            if (outcome.probability <= 0.0) continue;
            if (calc.is_goal_state(calc.state(outcome.state))) {
                success_probability_lower = imprint_downward_add(
                    success_probability_lower, outcome.probability);
                continue;
            }
            if ((satisfied_goal_mask(calc.state(outcome.state)) &
                 missing_mask) != 0) {
                success_probability_lower = imprint_downward_add(
                    success_probability_lower, outcome.probability);
                progress_states.push_back(outcome.state);
                continue;
            }
            std::vector<std::uint64_t> retry_signature;
            if (!action_legal(
                    calc.session(), descriptor,
                    calc.state(outcome.state)) ||
                !calc.exact_reforge_kernel_signature(
                    outcome.state, action_index, retry_signature) ||
                retry_signature != signature) {
                exact_retry = false;
                break;
            }
        }
        if (!exact_retry || !(success_probability_lower > 0.0) ||
            !std::isfinite(success_probability_lower)) {
            continue;
        }
        double finishing_cost_upper = 0.0;
        if (!progress_states.empty()) {
            std::sort(progress_states.begin(), progress_states.end());
            progress_states.erase(
                std::unique(
                    progress_states.begin(), progress_states.end()),
                progress_states.end());
            finishing_cost_upper =
                std::numeric_limits<double>::infinity();
            for (std::size_t finish_offset = 0;
                 finish_offset < actions.size(); ++finish_offset) {
                if (!action_prices[finish_offset].finite_nonnegative ||
                    !std::isfinite(action_prices[finish_offset].upper) ||
                    action_prices[finish_offset].upper < 0.0) {
                    continue;
                }
                const std::uint32_t finish_action =
                    actions[finish_offset];
                const ActionDescriptor& finish_descriptor =
                    calc.registry().actions.at(finish_action);
                bool finishes_every_progress_state = true;
                for (const std::uint32_t progress_state :
                     progress_states) {
                    if (!action_legal(
                            calc.session(), finish_descriptor,
                            calc.state(progress_state))) {
                        finishes_every_progress_state = false;
                        break;
                    }
                    const OutcomeDistribution& finish =
                        calc.outcomes(progress_state, finish_action);
                    if (!finish.supported ||
                        !finish.choice_groups.empty() ||
                        !finish.choice_options.empty() ||
                        finish.entries.empty() ||
                        std::any_of(
                            finish.entries.begin(), finish.entries.end(),
                            [&](const OutcomeEntry& outcome) {
                                return outcome.probability > 0.0 &&
                                    !calc.is_goal_state(
                                        calc.state(outcome.state));
                            })) {
                        finishes_every_progress_state = false;
                        break;
                    }
                }
                if (finishes_every_progress_state) {
                    finishing_cost_upper = std::min(
                        finishing_cost_upper,
                        action_prices[finish_offset].upper);
                }
            }
            if (!std::isfinite(finishing_cost_upper)) continue;
        }
        const double quotient =
            action_prices[offset].upper / success_probability_lower;
        const double upper = imprint_upward_add(
            std::nextafter(
                quotient, std::numeric_limits<double>::infinity()),
            finishing_cost_upper);
        if (std::isfinite(upper) && upper >= 0.0) {
            certified_carrier_upper = std::min(
                certified_carrier_upper, upper);
        }
    }
    const bool price_closure_enabled =
        std::isfinite(certified_carrier_upper) &&
        checkpoint_price.finite_nonnegative &&
        all_steps_strictly_positive &&
        std::isfinite(minimum_step_cost_lower);
    if (price_closure_enabled) {
        if (certified_carrier_upper > checkpoint_price.lower) {
            /* Outward rounding plus ceil deliberately overestimates the last
             * economically possible depth. Enforcing this integer boundary
             * makes positive-price termination independent of repeated
             * floating addition (which can stagnate once a step is below the
             * running sum's ULP). */
            const double remaining_upper = std::nextafter(
                certified_carrier_upper - checkpoint_price.lower,
                std::numeric_limits<double>::infinity());
            const double quotient_upper = std::nextafter(
                remaining_upper / minimum_step_cost_lower,
                std::numeric_limits<double>::infinity());
            const double depth = std::ceil(std::max(0.0, quotient_upper));
            result.price_bound_max_program_depth =
                !std::isfinite(depth) ||
                        depth >= static_cast<double>(
                             std::numeric_limits<std::uint64_t>::max())
                    ? std::numeric_limits<std::uint64_t>::max()
                    : static_cast<std::uint64_t>(depth);
        }
    }

    /* Exact full-kernel equality makes every common suffix identical. A
     * componentwise-smaller exact primitive-count vector therefore dominates
     * the entire dearer subtree under every economy. Hashes only shortlist
     * candidates; entry vectors remain the authority. Seed the entry kernel
     * so any exact return cycle is removed immediately. */
    std::vector<ImprintKernelParetoRepresentative>
        kernel_pareto_representatives;
    {
        const std::vector<OutcomeEntry> entry_support{{state_id, 1.0}};
        AttemptKernel entry_attempt;
        entry_attempt.supported = true;
        entry_attempt.fully_legal = true;
        entry_attempt.entries = entry_support;
        kernel_pareto_representatives.push_back({
            attempt_kernel_hash(entry_attempt), {},
            std::make_shared<const std::vector<OutcomeEntry>>(
                entry_support)});

        const ImprintProducerLower root_producer =
            minimum_direct_producer_cost_lower(entry_support);
        if (!root_producer.mechanically_possible) {
            ++result.programs_pruned;
            co_return result;
        }
        if (price_closure_enabled) {
            if (!std::isfinite(root_producer.priced_lower) ||
                imprint_downward_add(
                    checkpoint_price.lower,
                    root_producer.priced_lower) >
                    certified_carrier_upper ||
                result.price_bound_max_program_depth == 0) {
                ++result.programs_pruned;
                ++result.price_pruned_programs;
                result.price_bound_complete = true;
                co_return result;
            }
        }
    }

    std::vector<ImprintProgramPrefix> frontier(1);
    frontier.front().support =
        kernel_pareto_representatives.front().entries;
    std::vector<ImprintProgramPrefix> next;
    result.max_frontier_size = 1;
    std::vector<std::uint32_t> program;
    AttemptKernel attempt;
    constexpr std::size_t kMaxDepthDeferredSamples = 8;
    const auto retain_depth_deferred_sample = [&]
        (const std::vector<std::uint32_t>& prefix,
         const std::uint32_t final_action,
         const char* classification) {
        std::string sample;
        for (const std::uint32_t step : prefix) {
            if (!sample.empty()) sample += '>';
            sample += calc.registry().actions.at(step).id;
        }
        if (!sample.empty()) sample += '>';
        sample += calc.registry().actions.at(final_action).id;
        sample += ':';
        sample += classification;
        const std::string class_suffix =
            std::string(":") + classification;
        const auto same_class = [&](const std::string& existing) {
            return existing.size() >= class_suffix.size() &&
                   existing.compare(
                       existing.size() - class_suffix.size(),
                       class_suffix.size(), class_suffix) == 0;
        };
        if (result.depth_deferred_samples.size() >=
            kMaxDepthDeferredSamples) {
            if (std::any_of(
                    result.depth_deferred_samples.begin(),
                    result.depth_deferred_samples.end(), same_class)) {
                return;
            }
            /* Preserve at least one concrete witness from each frontier
             * class. In particular, early lexicographic nonproducing samples
             * must not hide the first actually evaluated changed program. */
            result.depth_deferred_samples.back() = std::move(sample);
            return;
        }
        result.depth_deferred_samples.push_back(std::move(sample));
    };
    const auto retained_bytes = [&]() {
        std::uint64_t bytes =
            imprint_discovery_result_nested_bytes(result);
        bytes = imprint_cursor_add(
            bytes, actions.capacity() * sizeof(std::uint32_t));
        bytes = imprint_cursor_add(
            bytes,
            action_base_reachable_masks.capacity() *
                sizeof(std::vector<std::uint64_t>));
        for (const std::vector<std::uint64_t>& mask :
             action_base_reachable_masks) {
            bytes = imprint_cursor_add(
                bytes, mask.capacity() * sizeof(std::uint64_t));
        }
        bytes = imprint_cursor_add(
            bytes,
            action_prices.capacity() * sizeof(ImprintScalarPrice));
        bytes = imprint_cursor_add(
            bytes,
            imprint_kernel_pareto_bytes(
                kernel_pareto_representatives));
        bytes = imprint_cursor_add(
            bytes, imprint_program_frontier_bytes(frontier));
        bytes = imprint_cursor_add(
            bytes, imprint_program_frontier_bytes(next));
        bytes = imprint_cursor_add(
            bytes, program.capacity() * sizeof(std::uint32_t));
        return imprint_cursor_add(
            bytes, imprint_attempt_kernel_nested_bytes(attempt));
    };
    constexpr std::size_t kCheckpointItems = 128;
    for (std::uint64_t depth = 1; !frontier.empty(); ++depth) {
        next.clear();
        for (std::size_t prefix_index = 0;
             prefix_index < frontier.size(); ++prefix_index) {
            for (std::size_t action_offset = 0;
                 action_offset < actions.size(); ++action_offset) {
                const std::uint32_t action = actions[action_offset];
                double candidate_cost_lower =
                    frontier[prefix_index].scalar_cost_lower;
                if (action_prices[action_offset].finite_nonnegative) {
                    candidate_cost_lower = imprint_downward_add(
                        candidate_cost_lower,
                        action_prices[action_offset].lower);
                }
                const double immediate_lower = imprint_downward_add(
                    checkpoint_price.lower, candidate_cost_lower);
                if (price_closure_enabled &&
                    immediate_lower > certified_carrier_upper) {
                    ++result.programs_pruned;
                    ++result.price_pruned_programs;
                    continue;
                }
                /* The work boundary owns exact program kernels, not action
                 * names that are mechanically illegal on one member of the
                 * current exact support. Reject those extensions before
                 * charging work. Besides making the counter meaningful,
                 * this avoids letting a large priced registry crowd out the
                 * few legal continuations of a magic Imprint carrier. The
                 * full support scan remains authoritative and cooperative;
                 * execute_attempt_cooperatively repeats the check so this
                 * prefilter cannot weaken publication invariants. */
                bool extension_fully_legal = true;
                for (std::size_t support_index = 0;
                     support_index <
                         frontier[prefix_index].support->size();
                     ++support_index) {
                    const OutcomeEntry& support_entry =
                        frontier[prefix_index].support->at(support_index);
                    if (support_entry.probability > 0.0 &&
                        !action_legal(
                            calc.session(),
                            calc.registry().actions.at(action),
                            calc.state(support_entry.state))) {
                        extension_fully_legal = false;
                        break;
                    }
                    if ((support_index + 1) % kCheckpointItems == 0) {
                        co_await solve_detail::CooperativeCheckpoint{
                            retained_bytes()};
                    }
                }
                if (!extension_fully_legal) continue;
                if (result.work_used >= result.work_limit) {
                    result.work_deferred = true;
                    co_return result;
                }
                program = frontier[prefix_index].program;
                program.push_back(action);
                ++result.work_used;
                result.max_evaluated_depth = std::max(
                    result.max_evaluated_depth, depth);
                std::vector<std::uint32_t> extension{action};
                auto attempt_task = execute_attempt_cooperatively(
                    calc, std::move(extension),
                    *frontier[prefix_index].support);
                while (!attempt_task.resume()) {
                    co_await solve_detail::CooperativeCheckpoint{
                        imprint_cursor_add(
                            retained_bytes(),
                            attempt_task.retained_bytes())};
                }
                attempt = attempt_task.take_result();
                attempt_task.reset();
                result.action_state_evaluations = imprint_cursor_add(
                    result.action_state_evaluations,
                    attempt.action_state_evaluations);
                result.outcomes_merged = imprint_cursor_add(
                    result.outcomes_merged,
                    attempt.outcomes_merged);
                result.max_atomic_outcomes_ns = std::max(
                    result.max_atomic_outcomes_ns,
                    attempt.max_atomic_outcomes_ns);
                co_await solve_detail::CooperativeCheckpoint{
                    retained_bytes()};
                if (!attempt.supported || !attempt.fully_legal ||
                    !attempt.choice_groups.empty() ||
                    !attempt.choice_options.empty() ||
                    attempt.entries.empty()) {
                    continue;
                }

                {
                    const std::uint64_t kernel_hash =
                        attempt_kernel_hash(attempt);
                    auto primitive_counts =
                        imprint_primitive_counts(program);
                    bool distribution_dominated = false;
                    for (const ImprintKernelParetoRepresentative&
                             representative :
                         kernel_pareto_representatives) {
                        if (representative.kernel_hash == kernel_hash &&
                            *representative.entries == attempt.entries &&
                            imprint_primitive_counts_dominate(
                                representative.primitive_counts,
                                primitive_counts)) {
                            distribution_dominated = true;
                            break;
                        }
                    }
                    if (distribution_dominated) {
                        ++result.programs_pruned;
                        ++result.distribution_dominated_programs;
                        continue;
                    }
                    /* Retain dominated representatives as shared storage for
                     * already-scheduled prefixes. They remain valid (if
                     * redundant) dominance certificates and let each exact
                     * support vector have one allocation owner. */
                    auto attempt_support = std::make_shared<
                        const std::vector<OutcomeEntry>>(
                            std::move(attempt.entries));
                    kernel_pareto_representatives.push_back({
                        kernel_hash, std::move(primitive_counts),
                        attempt_support});
                }
                /* The Pareto representative owns a potentially large copy of
                 * the exact entry vector. Reconcile retained bytes before any
                 * later publication so cancellation or a memory refusal rolls
                 * the whole admission transaction back. */
                co_await solve_detail::CooperativeCheckpoint{
                    retained_bytes()};

                std::uint32_t relevant_exit_mask = 0;
                bool changed = false;
                const std::shared_ptr<const std::vector<OutcomeEntry>>&
                    attempt_support =
                        kernel_pareto_representatives.back().entries;
                for (std::size_t outcome_index = 0;
                     outcome_index < attempt_support->size();
                     ++outcome_index) {
                    const OutcomeEntry& outcome =
                        attempt_support->at(outcome_index);
                    if (outcome.probability <= 0.0) continue;
                    changed |= outcome.state != state_id;
                    relevant_exit_mask |=
                        satisfied_goal_mask(
                            calc.state(outcome.state)) &
                        missing_mask;
                    if ((outcome_index + 1) % kCheckpointItems == 0) {
                        co_await solve_detail::CooperativeCheckpoint{
                            retained_bytes()};
                    }
                }
                if (relevant_exit_mask != 0) {
                    FixedOptionSpec option;
                    option.kind = FixedOptionKind::ImprintRetry;
                    for (const std::uint32_t step : program) {
                        option.program_action_ids.push_back(
                            calc.registry().actions.at(step).id);
                    }
                    for (std::uint32_t slot = 0;
                         slot < calc.goal().slots.size(); ++slot) {
                        if ((relevant_exit_mask & (1u << slot)) != 0) {
                            option.exit_goal_slots.push_back(slot);
                        }
                    }
                    option.exit_min_satisfied = 1;
                    option.automatic_kind = AutomaticCandidateKind::Imprint;
                    option.relevant_goal_mask = relevant_exit_mask;
                    result.specs.push_back(std::move(option));
                }
                if (!changed) continue;
                ImprintProducerLower producer;
                if (relevant_exit_mask == 0) {
                    producer = minimum_direct_producer_cost_lower(
                        *attempt_support);
                    if (!producer.mechanically_possible) {
                        ++result.programs_pruned;
                        continue;
                    }
                }
                if (price_closure_enabled) {
                    if (depth >=
                        result.price_bound_max_program_depth) {
                        ++result.programs_pruned;
                        ++result.price_pruned_programs;
                        continue;
                    }
                    const double suffix_cost_lower =
                        relevant_exit_mask != 0
                            ? minimum_step_cost_lower
                            : producer.priced_lower;
                    if (std::isfinite(suffix_cost_lower)) {
                        const double extension_lower =
                            imprint_downward_add(
                                immediate_lower, suffix_cost_lower);
                        if (extension_lower <=
                            certified_carrier_upper) {
                            next.push_back({
                                program, attempt_support,
                                candidate_cost_lower});
                        } else {
                            ++result.programs_pruned;
                            ++result.price_pruned_programs;
                        }
                    } else {
                        /* Exact zero exits plus an empty conservative producer
                         * universe proves that no descendant can become a
                         * useful Imprint program. */
                        ++result.programs_pruned;
                        ++result.price_pruned_programs;
                    }
                } else if (depth < result.depth_limit) {
                    next.push_back({
                        program, attempt_support, candidate_cost_lower});
                } else {
                    result.depth_deferred = true;
                    retain_depth_deferred_sample(
                        frontier[prefix_index].program, action,
                        relevant_exit_mask != 0
                            ? "evaluated_changed_goal_capable"
                            : "evaluated_changed_no_exit");
                }
            }
        }
        frontier.swap(next);
        result.max_frontier_size = std::max<std::uint64_t>(
            result.max_frontier_size, frontier.size());
        co_await solve_detail::CooperativeCheckpoint{
            retained_bytes()};
    }
    result.price_bound_complete =
        price_closure_enabled && !result.work_deferred &&
        !result.depth_deferred;
    co_return result;
}

bool state_has_unfractured_crafted(const AbstractState& state) {
    if (state.crafted_goal_mask != 0) return true;
    for (std::size_t i = 0; i < state.crafted_junk_counts.size(); ++i) {
        if (state.crafted_junk_counts[i] >
            state.fractured_crafted_junk_counts[i]) {
            return true;
        }
    }
    return false;
}

bool advances_goal_mask(
    const CalcContext& calc,
    const AbstractState& source,
    const std::vector<OutcomeEntry>& exits,
    const std::uint32_t goal_mask) {
    for (const OutcomeEntry& exit : exits) {
        if (exit.probability <= 0.0) continue;
        const AbstractState& successor = calc.state(exit.state);
        for (std::uint32_t slot = 0; slot < calc.layout().slots.size(); ++slot) {
            if ((goal_mask & (1u << slot)) != 0 &&
                successor.slot_status[slot] > source.slot_status[slot]) {
                return true;
            }
        }
    }
    return false;
}

bool clears_target_space(
    const CalcContext& calc,
    const AbstractState& source,
    const std::vector<OutcomeEntry>& exits,
    const std::uint32_t goal_mask) {
    for (const OutcomeEntry& exit : exits) {
        if (exit.probability <= 0.0) continue;
        const AbstractState& successor = calc.state(exit.state);
        for (std::uint32_t slot = 0; slot < calc.goal().slots.size(); ++slot) {
            if ((goal_mask & (1u << slot)) == 0) continue;
            if ((source.blocked_mask & (1u << slot)) != 0 &&
                (successor.blocked_mask & (1u << slot)) == 0) {
                return true;
            }
            const std::int8_t side =
                goal_slot_side(calc.session(), calc.goal().slots[slot]);
            if ((side == PC_SIDE_PREFIX &&
                 successor.prefix_count < source.prefix_count) ||
                (side == PC_SIDE_SUFFIX &&
                 successor.suffix_count < source.suffix_count)) {
                return true;
            }
        }
    }
    return false;
}

std::uint32_t satisfied_goal_mask(const AbstractState& state) {
    std::uint32_t mask = 0;
    for (std::uint32_t slot = 0; slot < kMaxGoalSlots; ++slot) {
        if (state.slot_status[slot] ==
            static_cast<std::uint8_t>(GoalSlotStatus::Satisfied)) {
            mask |= 1u << slot;
        }
    }
    return mask;
}

bool all_exits_without_flag(
    const CalcContext& calc,
    const std::vector<OutcomeEntry>& exits,
    const std::uint32_t flag) {
    return std::all_of(
        exits.begin(), exits.end(), [&](const OutcomeEntry& exit) {
            return (calc.state(exit.state).flags & flag) == 0;
        });
}

bool all_exits_cleaned(
    const CalcContext& calc,
    const std::vector<OutcomeEntry>& exits) {
    return std::all_of(
        exits.begin(), exits.end(), [&](const OutcomeEntry& exit) {
            return !state_has_unfractured_crafted(calc.state(exit.state));
        });
}

bool setup_applies_exactly(
    CalcContext& calc,
    const std::uint32_t state_id,
    const std::uint32_t setup_action,
    const std::uint32_t required_flag = 0) {
    if (setup_action == kNoId ||
        !action_legal(
            calc.session(), calc.registry().actions.at(setup_action),
            calc.state(state_id))) {
        return false;
    }
    const OutcomeDistribution& setup = calc.outcomes(state_id, setup_action);
    if (!setup.supported || !setup.choice_groups.empty() ||
        setup.entries.empty()) {
        return false;
    }
    return std::all_of(
        setup.entries.begin(), setup.entries.end(),
        [&](const OutcomeEntry& exit) {
            return exit.state != state_id &&
                   (required_flag == 0 ||
                    (calc.state(exit.state).flags & required_flag) != 0);
        });
}

void add_scaled_resources(
    std::map<std::string, double>& target,
    const std::vector<std::pair<std::string, double>>& source,
    const double scale = 1.0) {
    for (const auto& [key, quantity] : source) {
        target[key] += scale * quantity;
    }
}

void add_action_resources(
    std::map<std::string, double>& target,
    const ActionDescriptor& action,
    const double scale) {
    for (const std::string& key : action.cost_keys) {
        target[key] += scale;
    }
}

std::string registry_action_id(
    const ActionRegistry& registry,
    const std::uint32_t index) {
    if (index == kNoId) return {};
    if (index >= registry.actions.size()) {
        throw std::logic_error(
            "planner operator contains an out-of-range primitive action");
    }
    return registry.actions[index].id;
}

void bind_planner_primitive_action_ids(
    const ActionRegistry& registry,
    PlannerOperator& planner) {
    planner.primitive_action_id =
        registry_action_id(registry, planner.primitive_action);
    planner.primitive_program_action_ids.clear();
    planner.primitive_program_action_ids.reserve(
        planner.primitive_program.size());
    for (const std::uint32_t action : planner.primitive_program) {
        planner.primitive_program_action_ids.push_back(
            registry_action_id(registry, action));
    }
    planner.conditional_action_id =
        registry_action_id(registry, planner.conditional_action);
    planner.setup_action_id =
        registry_action_id(registry, planner.setup_action);
    planner.followup_action_id =
        registry_action_id(registry, planner.followup_action);
    planner.cleanup_action_id =
        registry_action_id(registry, planner.cleanup_action);
    planner.constructive_finish_action_id =
        registry_action_id(
            registry, planner.constructive_finish_action);
}

std::string bestiary_action_id(
    const SessionImpl& session,
    const std::uint32_t index) {
    if (index == kNoId) return {};
    if (index >= session.data->bestiary_actions.size()) {
        throw std::logic_error(
            "planner operator contains an out-of-range Bestiary action");
    }
    return session.data->bestiary_actions[index].id;
}

void bind_planner_bestiary_action_ids(
    const SessionImpl& session,
    PlannerOperator& planner) {
    planner.bestiary_create_action_id =
        bestiary_action_id(session, planner.bestiary_create_action);
    planner.bestiary_restore_action_id =
        bestiary_action_id(session, planner.bestiary_restore_action);
}

} // namespace

} // namespace solver
} // namespace poecraft
