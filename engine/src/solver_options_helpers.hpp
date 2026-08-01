#pragma once

#include "solver_calc_types.hpp"

#include "poecraft/bitset.h"

#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <functional>
#include <iterator>
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

AutomaticTelemetryKind telemetry_kind_for_candidate(
    const AutomaticCandidateKind kind) {
    switch (kind) {
    case AutomaticCandidateKind::Imprint:
        return AutomaticTelemetryKind::Imprint;
    case AutomaticCandidateKind::TemporaryBenchBlocker:
        return AutomaticTelemetryKind::TemporaryBench;
    case AutomaticCandidateKind::ProtectedMetamod:
        return AutomaticTelemetryKind::ProtectedSide;
    case AutomaticCandidateKind::MultimodFinish:
        return AutomaticTelemetryKind::MultimodFinish;
    case AutomaticCandidateKind::PermanentBench:
        return AutomaticTelemetryKind::PermanentBench;
    case AutomaticCandidateKind::Fracture:
        return AutomaticTelemetryKind::PrimitiveFracture;
    case AutomaticCandidateKind::ConstructiveRenewal:
        return AutomaticTelemetryKind::Renewal;
    case AutomaticCandidateKind::EldritchSide:
        return AutomaticTelemetryKind::EldritchSide;
    case AutomaticCandidateKind::None:
        return AutomaticTelemetryKind::None;
    }
    return AutomaticTelemetryKind::None;
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
     * Product Eldritch planning exposes only four state-local side intents.
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
                const auto consider =
                    [&](std::vector<std::string> ids) {
                        double cost = 0.0;
                        for (const std::string& id : ids) {
                            const auto found =
                                registry.index_by_id.find(id);
                            if (found == registry.index_by_id.end()) {
                                return;
                            }
                            const double part = action_cost(found->second);
                            if (!std::isfinite(part)) return;
                            cost += part;
                        }
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
                if (!std::isfinite(best_cost)) return std::nullopt;
                return best;
            };

        const std::uint32_t satisfied = satisfied_goal_mask(state);
        const std::uint8_t cap =
            rarity_affix_cap(session, state.rarity);
        for (const std::int8_t side :
             {static_cast<std::int8_t>(PC_SIDE_PREFIX),
              static_cast<std::int8_t>(PC_SIDE_SUFFIX)}) {
            std::uint32_t side_goal_mask = 0;
            std::uint32_t opposite_goal_mask = 0;
            for (std::uint32_t slot = 0;
                 slot < goal.slots.size(); ++slot) {
                if (goal_slot_side(
                        session, goal.slots[slot]) == side) {
                    side_goal_mask |= 1u << slot;
                } else {
                    opposite_goal_mask |= 1u << slot;
                }
            }
            const std::uint32_t missing =
                side_goal_mask & ~satisfied;
            const std::uint32_t preserved =
                opposite_goal_mask & satisfied;
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
                if (final == registry.index_by_id.end() ||
                    !action_has_prices(final->second)) {
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
    if (cleanup != registry.index_by_id.end() &&
        action_has_prices(cleanup->second) &&
        (state.flags & kFlagCraftedMod) == 0) {
        const auto enumeration_started = std::chrono::steady_clock::now();
        const auto& precompiled = calc.temporary_bench_effect_classes();
        synthesis.temporary_precompiled_classes = precompiled.size();
        std::map<
            std::tuple<
                std::uint32_t,
                std::uint32_t,
                std::int8_t,
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
                !temporary_blocker_applies(session, carrier, effect)) {
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
                    carrier, effect.followup_action);
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
                 carrier.prefix_count + 1 >= cap) ||
                (effect.blocker_side == PC_SIDE_SUFFIX &&
                 carrier.suffix_count + 1 >= cap);
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
                    effect.blocker_side, blocked),
                synthesis.temporary_groups.size());
            TemporaryBenchCandidateGroup* group = nullptr;
            if (effect_inserted) {
                TemporaryBenchCandidateGroup created;
                created.representative_blocker =
                    *representative;
                created.followup_action = effect.followup_action;
                created.goal_slot = effect.goal_slot;
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
            option.action_id = followup.id;
            option.exit_goal_slots = {group.goal_slot};
            option.exit_min_satisfied = 1;
            option.automatic_kind =
                AutomaticCandidateKind::TemporaryBenchBlocker;
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
};

AttemptKernel execute_attempt(
    CalcContext& calc,
    const std::vector<std::uint32_t>& program,
    const std::uint32_t entry_state) {
    AttemptKernel result;
    std::map<std::string, double> resources;
    std::map<std::uint32_t, double> active{{entry_state, 1.0}};
    std::map<std::uint32_t, double> stopped;
    for (std::size_t step = 0; step < program.size(); ++step) {
        const std::uint32_t action_index = program[step];
        const ActionDescriptor& action =
            calc.registry().actions.at(action_index);
        const bool observed = action_observes_modifier_offer(action);
        if (observed && step + 1 != program.size()) {
            result.supported = false;
            break;
        }
        std::map<std::uint32_t, double> next;
        for (const auto& [state_id, path_probability] : active) {
            if (!action_legal(calc.session(), action, calc.state(state_id))) {
                result.fully_legal = false;
                stopped[state_id] += path_probability;
                continue;
            }
            result.expected_primitive_actions += path_probability;
            for (const std::string& key : action.cost_keys) {
                resources[key] += path_probability;
            }
            const OutcomeDistribution& distribution =
                calc.outcomes(state_id, action_index);
            if (!distribution.supported ||
                (!observed && !distribution.choice_groups.empty())) {
                result.supported = false;
                break;
            }
            if (observed && !distribution.choice_groups.empty()) {
                for (const OutcomeChoiceGroup& group :
                     distribution.choice_groups) {
                    result.choice_groups.push_back(
                        {path_probability * group.probability,
                         group.states, state_id});
                }
                for (const OutcomeChoiceOption& choice :
                     distribution.choice_options) {
                    result.choice_options.push_back(
                        {choice.mod_id, choice.state, state_id,
                         choice.state});
                }
            } else {
                for (const OutcomeEntry& outcome : distribution.entries) {
                    next[outcome.state] +=
                        path_probability * outcome.probability;
                }
            }
        }
        if (!result.supported) break;
        active = std::move(next);
    }
    if (result.supported) {
        for (const auto& [state, probability] : stopped) {
            active[state] += probability;
        }
        for (const auto& [state, probability] : active) {
            result.entries.push_back({state, probability});
        }
        result.expected_resources.assign(resources.begin(), resources.end());
    }
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
    bool missing_price = false;
    bool depth_deferred = false;
    bool work_deferred = false;
    std::uint32_t depth_limit = 0;
    std::uint64_t work_limit = 0;
    std::uint64_t work_used = 0;
};

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

ImprintDiscoveryResult discover_automatic_imprint_options(
    CalcContext& calc,
    const std::uint32_t state_id,
    const AutomaticAdmissionLimits& limits,
    const std::function<void()>& check_limits) {
    ImprintDiscoveryResult result;
    result.depth_limit = limits.max_imprint_program_depth == 0
                             ? kDefaultImprintProgramDepth
                             : limits.max_imprint_program_depth;
    result.work_limit = limits.max_imprint_program_work == 0
                            ? kDefaultImprintProgramWork
                            : limits.max_imprint_program_work;
    if (!native_imprint_checkpoint_creation_legal(calc, state_id)) {
        return result;
    }

    const auto create = calc.session().data->bestiary_action_by_id.find(
        "bestiary:imprint");
    if (create == calc.session().data->bestiary_action_by_id.end()) {
        return result;
    }
    if (!resource_keys_available(
            calc.session().data->bestiary_actions.at(create->second).cost_keys,
            limits.prices)) {
        result.missing_price = true;
        return result;
    }

    std::vector<std::uint32_t> actions;
    for (const std::uint32_t index : calc.candidates()) {
        const ActionDescriptor& action = calc.registry().actions.at(index);
        if (action.synthetic || action.uses_companion_state ||
            action.automatic_dependency_only ||
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
    if (actions.empty()) return result;

    const AbstractState& entry = calc.state(state_id);
    const std::uint32_t goal_mask = calc.goal().slots.size() == 32
                                        ? 0xffffffffu
                                        : (1u << calc.goal().slots.size()) - 1u;
    const std::uint32_t missing_mask =
        goal_mask & ~satisfied_goal_mask(entry);
    if (missing_mask == 0) return result;

    std::vector<std::vector<std::uint32_t>> frontier(1);
    for (std::uint32_t depth = 1;
         depth <= result.depth_limit && !frontier.empty(); ++depth) {
        std::vector<std::vector<std::uint32_t>> next;
        for (const std::vector<std::uint32_t>& prefix : frontier) {
            for (const std::uint32_t action : actions) {
                if (result.work_used >= result.work_limit) {
                    result.work_deferred = true;
                    return result;
                }
                std::vector<std::uint32_t> program = prefix;
                program.push_back(action);
                ++result.work_used;
                const AttemptKernel attempt = execute_attempt(
                    calc, program, state_id);
                check_limits();
                if (!attempt.supported || !attempt.fully_legal ||
                    !attempt.choice_groups.empty() ||
                    !attempt.choice_options.empty() ||
                    attempt.entries.empty()) {
                    continue;
                }

                std::uint32_t relevant_exit_mask = 0;
                bool changed = false;
                for (const OutcomeEntry& outcome : attempt.entries) {
                    if (outcome.probability <= 0.0) continue;
                    changed |= outcome.state != state_id;
                    relevant_exit_mask |=
                        satisfied_goal_mask(calc.state(outcome.state)) &
                        missing_mask;
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
                if (changed && depth < result.depth_limit) {
                    next.push_back(std::move(program));
                } else if (changed && depth == result.depth_limit) {
                    result.depth_deferred = true;
                }
            }
        }
        frontier = std::move(next);
    }
    return result;
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
