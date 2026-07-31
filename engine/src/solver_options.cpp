#include "solver_internal.hpp"

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

std::vector<PlannerOperator> build_planner_operators(
    const SessionImpl& session,
    const GoalSpec& goal,
    const ActionRegistry& registry,
    const std::vector<std::uint32_t>& admitted_primitives) {
    std::vector<PlannerOperator> operators;
    operators.reserve(
        registry.actions.size() + goal.fixed_options.size());
    for (std::uint32_t index = 0; index < registry.actions.size(); ++index) {
        const ActionDescriptor& action = registry.actions[index];
        PlannerOperator primitive;
        primitive.id = action.id;
        primitive.display_name = action.display_name;
        primitive.kind = PlannerOperatorKind::Primitive;
        primitive.primitive_action = index;
        primitive.primitive_program.push_back(index);
        primitive.resource_quantities =
            aggregate_resources(registry, primitive.primitive_program);
        bind_planner_primitive_action_ids(registry, primitive);
        bind_planner_bestiary_action_ids(session, primitive);
        if (goal.automatic_candidates) {
            if (action.params.type == ActionType::Fracture) {
                primitive.relevant_goal_mask =
                    goal.slots.size() == 32
                        ? 0xffffffffu
                        : (1u << goal.slots.size()) - 1u;
                primitive.automatic_kind = AutomaticCandidateKind::Fracture;
            } else if (action.params.type == ActionType::Bench &&
                       action.params.mod_id < session.mod_count) {
                primitive.relevant_goal_mask =
                    goal_mask_for_mod(session, goal, action.params.mod_id);
                if (primitive.relevant_goal_mask != 0) {
                    primitive.automatic_kind =
                        AutomaticCandidateKind::PermanentBench;
                }
            }
        }
        operators.push_back(std::move(primitive));
    }

    std::vector<FixedOptionSpec> option_specs = goal.fixed_options;
    if (goal.automatic_candidates && goal.required_satisfied_slots() > 1) {
        const std::uint32_t all_goal_slots =
            goal.slots.size() == 32
                ? 0xffffffffu
                : (1u << goal.slots.size()) - 1u;
        for (const std::uint32_t bench_index : admitted_primitives) {
            if (bench_index >= registry.actions.size()) continue;
            const ActionDescriptor& bench = registry.actions[bench_index];
            if (bench.params.type != ActionType::Bench ||
                bench.params.mod_id >= session.mod_count ||
                bench.params.mod_id >= session.metamod_type.size() ||
                session.metamod_type[bench.params.mod_id] >= 0) {
                continue;
            }
            const std::uint32_t finish_mask =
                goal_mask_for_mod(session, goal, bench.params.mod_id);
            if (finish_mask == 0) continue;
            const std::uint32_t finish_count = std::popcount(finish_mask);
            const std::uint32_t required = static_cast<std::uint32_t>(
                goal.required_satisfied_slots());
            if (finish_count >= required) continue;
            const std::uint32_t exit_count = required - finish_count;
            const std::uint32_t available = all_goal_slots & ~finish_mask;
            for (std::uint32_t subset = available; subset != 0;
                 subset = (subset - 1u) & available) {
                if (static_cast<std::uint32_t>(
                        std::popcount(subset)) != exit_count) {
                    continue;
                }
                std::vector<std::uint32_t> exit_slots;
                for (std::uint32_t slot = 0; slot < goal.slots.size(); ++slot) {
                    if ((subset & (1u << slot)) != 0) {
                        exit_slots.push_back(slot);
                    }
                }
                for (const std::uint32_t roll_index : admitted_primitives) {
                    if (roll_index >= registry.actions.size()) continue;
                    const ActionDescriptor& roll = registry.actions[roll_index];
                    if (!approved_renewal_roll(roll)) continue;
                    FixedOptionSpec renewal;
                    renewal.kind = FixedOptionKind::Renewal;
                    renewal.program_action_ids = {roll.id};
                    renewal.exit_goal_slots = exit_slots;
                    renewal.exit_min_satisfied = exit_count;
                    renewal.automatic_kind =
                        AutomaticCandidateKind::ConstructiveRenewal;
                    renewal.relevant_goal_mask = subset | finish_mask;
                    renewal.constructive_finish_action_id = bench.id;
                    option_specs.push_back(std::move(renewal));
                }
            }
        }
    }

    std::set<std::string> option_ids;
    for (const FixedOptionSpec& spec : option_specs) {
        PlannerOperator option;
        option.kind = PlannerOperatorKind::FixedOption;
        option.option_kind = spec.kind;
        option.intended_side = spec.side;
        option.exit_goal_slots = spec.exit_goal_slots;
        option.exit_min_satisfied = spec.exit_min_satisfied;
        option.carrier_goal_slot = spec.carrier_goal_slot;
        option.automatic_kind = spec.automatic_kind;
        option.relevant_goal_mask = spec.relevant_goal_mask;

        switch (spec.kind) {
        case FixedOptionKind::ScourAlchemy: {
            std::uint32_t scour = kNoId;
            std::uint32_t alchemy = kNoId;
            require_action(registry, "scour", scour);
            require_action(registry, "alchemy", alchemy);
            option.id = "option:scour_alchemy";
            option.display_name = "Scour then Alchemy";
            option.primitive_program = {scour, alchemy};
            break;
        }
        case FixedOptionKind::EldritchSideIntent: {
            require_side(spec);
            if (spec.setup_action_ids.empty() &&
                spec.automatic_kind !=
                    AutomaticCandidateKind::EldritchSide) {
                throw std::runtime_error(
                    "fixed option: Eldritch side intent needs an explicit "
                    "non-empty setup array");
            }
            for (const std::string& id : spec.setup_action_ids) {
                std::uint32_t index = kNoId;
                const ActionDescriptor& setup =
                    require_action(registry, id, index);
                if (setup.params.type != ActionType::EldritchEmber &&
                    setup.params.type != ActionType::EldritchIchor) {
                    throw std::runtime_error(
                        "fixed option: Eldritch setup action must be an "
                        "ember or ichor: " + id);
                }
                option.primitive_program.push_back(index);
            }
            std::uint32_t craft_index = kNoId;
            const ActionDescriptor& craft =
                require_action(registry, spec.action_id, craft_index);
            if (craft.params.type != ActionType::EldritchExalt &&
                craft.params.type != ActionType::EldritchChaos &&
                craft.params.type != ActionType::EldritchAnnul) {
                throw std::runtime_error(
                    "fixed option: Eldritch intent action must be exalt, "
                    "chaos, or annul");
            }
            option.primitive_program.push_back(craft_index);
            option.id =
                std::string("option:eldritch_side_intent:") +
                side_name(spec.side) + ':' + craft.id + ':' +
                (option.primitive_program.size() == 1
                     ? std::string("direct")
                     : join_ids(
                           registry,
                           std::vector<std::uint32_t>(
                               option.primitive_program.begin(),
                               option.primitive_program.end() - 1)));
            if (spec.automatic_kind ==
                AutomaticCandidateKind::EldritchSide) {
                option.display_name =
                    std::string("Eldritch ") +
                    (craft.params.type == ActionType::EldritchAnnul
                         ? "Annul "
                         : "Chaos ") +
                    (spec.side == PC_SIDE_PREFIX
                         ? "Prefix"
                         : "Suffix");
            } else {
                option.display_name =
                    std::string("Eldritch ") +
                    side_name(spec.side) +
                    " intent: " + craft.display_name;
            }
            break;
        }
        case FixedOptionKind::ProtectedSide: {
            require_side(spec);
            const int lock_code =
                spec.side == PC_SIDE_PREFIX
                    ? session.data->metamod_prefixes_locked_code
                    : session.data->metamod_suffixes_locked_code;
            const std::uint32_t lock = find_metamod_action(
                session, registry, lock_code,
                spec.side == PC_SIDE_PREFIX ? "prefix-lock"
                                            : "suffix-lock");
            std::uint32_t action_index = kNoId;
            const ActionDescriptor& action =
                require_action(registry, spec.action_id, action_index);
            if (action.params.type != ActionType::Scour &&
                action.kind != TransitionKind::Reforge) {
                throw std::runtime_error(
                    "fixed option: protected-side action must be Scour or "
                    "an exact reforge");
            }
            if (!calc_supports(action)) {
                throw std::runtime_error(
                    "fixed option: protected-side reforge has no exact "
                    "evaluator: " + action.id);
            }
            option.id = std::string("option:protected_side:") +
                        side_name(spec.side) + ':' + action.id;
            option.display_name = std::string("Protect ") +
                                  side_name(spec.side) + " then " +
                                  action.display_name;
            option.primitive_program = {lock, action_index};
            option.setup_action = lock;
            option.followup_action = action_index;
            if (spec.automatic_kind != AutomaticCandidateKind::None) {
                option.id += ":goal:" +
                             std::to_string(spec.relevant_goal_mask);
            }
            break;
        }
        case FixedOptionKind::MultimodFinish: {
            if (spec.bench_craft_ids.empty() ||
                spec.bench_craft_ids.size() > 2) {
                throw std::runtime_error(
                    "fixed option: Multimod finish needs one or two "
                    "explicit bench crafts");
            }
            const std::uint32_t multimod = find_metamod_action(
                session, registry, session.data->metamod_multimod_code,
                "Multimod");
            option.primitive_program.push_back(multimod);
            std::set<std::uint32_t> unique_crafts;
            for (const std::string& id : spec.bench_craft_ids) {
                std::uint32_t index = kNoId;
                const ActionDescriptor& craft =
                    require_action(registry, id, index);
                if (craft.params.type != ActionType::Bench ||
                    craft.params.mod_id >= session.metamod_type.size() ||
                    session.metamod_type[craft.params.mod_id] >= 0) {
                    throw std::runtime_error(
                        "fixed option: Multimod finish entries must be "
                        "ordinary bench crafts: " + id);
                }
                if (!unique_crafts.insert(index).second) {
                    throw std::runtime_error(
                        "fixed option: duplicate Multimod finish craft: " +
                        id);
                }
                option.primitive_program.push_back(index);
            }
            option.id = "option:multimod_finish:" +
                        join_ids(registry, std::vector<std::uint32_t>(
                            option.primitive_program.begin() + 1,
                            option.primitive_program.end()));
            option.display_name = "Multimod deterministic finish";
            option.setup_action = multimod;
            break;
        }
        case FixedOptionKind::Renewal: {
            resolve_renewal_program(
                registry, spec, option.primitive_program, true);
            option.id = "option:renewal:" +
                        join_ids(registry, option.primitive_program) +
                        exit_suffix(spec);
            option.display_name = "Repeat " +
                                  join_ids(registry,
                                           option.primitive_program) +
                                  " to fixed exit";
            if (spec.automatic_kind ==
                AutomaticCandidateKind::ConstructiveRenewal) {
                if (spec.constructive_finish_action_id.empty()) {
                    throw std::runtime_error(
                        "fixed option: constructive renewal needs an exact "
                        "finish action");
                }
                const ActionDescriptor& finish = require_action(
                    registry, spec.constructive_finish_action_id,
                    option.constructive_finish_action);
                if (finish.params.type != ActionType::Bench ||
                    finish.kind != TransitionKind::Deterministic ||
                    finish.params.mod_id >= session.metamod_type.size() ||
                    session.metamod_type[finish.params.mod_id] >= 0) {
                    throw std::runtime_error(
                        "fixed option: constructive renewal finish must be "
                        "an ordinary deterministic bench craft");
                }
                option.id += ":finish:" + finish.id;
            }
            break;
        }
        case FixedOptionKind::ProtectedRepeat: {
            require_side(spec);
            const int lock_code =
                spec.side == PC_SIDE_PREFIX
                    ? session.data->metamod_prefixes_locked_code
                    : session.data->metamod_suffixes_locked_code;
            const std::uint32_t lock = find_metamod_action(
                session, registry, lock_code,
                spec.side == PC_SIDE_PREFIX ? "prefix-lock"
                                            : "suffix-lock");
            std::uint32_t action_index = kNoId;
            const ActionDescriptor& action =
                require_action(registry, spec.action_id, action_index);
            if (!approved_renewal_roll(action)) {
                throw std::runtime_error(
                    "fixed option: protected repeat action must be an "
                    "approved exact rolling/reforge method");
            }
            option.primitive_program = {lock, action_index};
            option.setup_action = lock;
            option.followup_action = action_index;
            option.id = std::string("option:protected_repeat:") +
                        side_name(spec.side) + ':' + action.id +
                        exit_suffix(spec);
            option.display_name = std::string("Repeat protected ") +
                                  side_name(spec.side) + ' ' +
                                  action.display_name;
            break;
        }
        case FixedOptionKind::FracturePrepare: {
            resolve_renewal_program(
                registry, spec, option.primitive_program, false);
            std::uint32_t fracture = kNoId;
            require_action(registry, "fracture", fracture);
            option.conditional_action = fracture;
            option.id = "option:fracture_prepare:" +
                        std::to_string(spec.carrier_goal_slot) + ':' +
                        join_ids(registry, option.primitive_program);
            option.display_name = "Prepare and Fracture exact goal carrier";
            option.followup_action = fracture;
            break;
        }
        case FixedOptionKind::ImprintRetry: {
            if (spec.exit_goal_slots.empty() ||
                spec.exit_min_satisfied == 0 ||
                spec.exit_min_satisfied > spec.exit_goal_slots.size()) {
                throw std::runtime_error(
                    "fixed option: imprint retry needs a non-empty exact "
                    "intermediate exit");
            }
            std::set<std::uint32_t> unique_exit_slots;
            for (const std::uint32_t slot : spec.exit_goal_slots) {
                if (slot >= goal.slots.size() ||
                    !unique_exit_slots.insert(slot).second) {
                    throw std::runtime_error(
                        "fixed option: imprint retry exit has an invalid or "
                        "duplicate goal slot");
                }
            }
            resolve_imprint_program(
                registry, spec, option.primitive_program);
            option.bestiary_create_action = require_bestiary_action(
                session, "bestiary:imprint");
            option.bestiary_restore_action = require_bestiary_action(
                session, "bestiary:restore_imprint");
            const BestiaryActionDescriptor& create =
                session.data->bestiary_actions.at(
                    option.bestiary_create_action);
            const BestiaryActionDescriptor& restore =
                session.data->bestiary_actions.at(
                    option.bestiary_restore_action);
            if (create.operation != BestiaryOperationKind::Create ||
                restore.operation != BestiaryOperationKind::Restore ||
                !restore.cost_keys.empty()) {
                throw std::runtime_error(
                    "fixed option: native Imprint descriptors are inconsistent");
            }
            option.id = "option:imprint_retry:" +
                        join_ids(registry, option.primitive_program) +
                        exit_suffix(spec);
            option.display_name = "Imprint and retry " +
                                  join_ids(registry,
                                           option.primitive_program);
            break;
        }
        case FixedOptionKind::TemporaryBenchRepeat: {
            if (spec.setup_action_ids.size() != 1 ||
                spec.exit_goal_slots.size() != 1 ||
                spec.exit_min_satisfied != 1) {
                throw std::runtime_error(
                    "fixed option: temporary bench repeat needs one blocker "
                    "and one exact goal-slot exit");
            }
            std::uint32_t blocker_index = kNoId;
            const ActionDescriptor& blocker = require_action(
                registry, spec.setup_action_ids.front(), blocker_index);
            if (blocker.params.type != ActionType::Bench ||
                blocker.params.mod_id >= session.metamod_type.size() ||
                session.metamod_type[blocker.params.mod_id] >= 0) {
                throw std::runtime_error(
                    "fixed option: temporary blocker must be an ordinary "
                    "bench craft");
            }
            std::uint32_t followup_index = kNoId;
            const ActionDescriptor& followup = require_action(
                registry, spec.action_id, followup_index);
            if (!temporary_followup(followup)) {
                throw std::runtime_error(
                    "fixed option: temporary blocker follow-up must be an "
                    "exact supported single-slot action");
            }
            std::uint32_t cleanup_index = kNoId;
            require_action(
                registry, "remove_crafted_modifiers", cleanup_index);
            option.primitive_program = {
                blocker_index, followup_index, cleanup_index};
            option.setup_action = blocker_index;
            option.followup_action = followup_index;
            option.cleanup_action = cleanup_index;
            option.id = "option:temporary_bench_repeat:" + blocker.id +
                        ':' + followup.id + exit_suffix(spec);
            option.display_name = "Temporary " + blocker.display_name +
                                  " then repeat " + followup.display_name;
            break;
        }
        }

        if (!option_ids.insert(option.id).second) {
            if (spec.automatic_kind != AutomaticCandidateKind::None) {
                continue;
            }
            throw std::runtime_error(
                "fixed option: duplicate definition: " + option.id);
        }
        for (const std::uint32_t action : option.primitive_program) {
            if (!calc_supports(registry.actions.at(action))) {
                throw std::runtime_error(
                    "fixed option: primitive has no exact evaluator: " +
                    registry.actions.at(action).id);
            }
            if (action_observes_modifier_offer(
                    registry.actions.at(action)) &&
                option.option_kind != FixedOptionKind::Renewal) {
                throw std::runtime_error(
                    "fixed option: observed choices are not fixed "
                    "program steps");
            }
        }
        if (option.conditional_action != kNoId) {
            if (!calc_supports(
                    registry.actions.at(option.conditional_action))) {
                throw std::runtime_error(
                    "fixed option: conditional primitive has no exact "
                    "evaluator");
            }
            option.primitive_program.push_back(option.conditional_action);
        }
        option.resource_quantities =
            aggregate_resources(registry, option.primitive_program);
        if (option.option_kind == FixedOptionKind::ImprintRetry) {
            std::map<std::string, double> quantities(
                option.resource_quantities.begin(),
                option.resource_quantities.end());
            for (const std::string& key :
                 session.data->bestiary_actions.at(
                     option.bestiary_create_action).cost_keys) {
                quantities[key] += 1.0;
            }
            option.resource_quantities.assign(
                quantities.begin(), quantities.end());
        }
        if (option.conditional_action != kNoId) {
            option.primitive_program.pop_back();
        }
        bind_planner_primitive_action_ids(registry, option);
        bind_planner_bestiary_action_ids(session, option);
        operators.push_back(std::move(option));
    }
    /*
     * Planner construction is the admission boundary for composite runtime
     * semantics. Reject an incomplete dependency contract or inconsistent
     * execution-role program before any policy can select the operator.
     */
    for (const PlannerOperator& planner : operators) {
        (void)planner_operator_runtime_semantics(
            planner, registry);
    }
    return operators;
}

namespace {

void append_semantic_string(
    std::vector<std::uint64_t>& key,
    const std::string& value) {
    key.push_back(static_cast<std::uint64_t>(value.size()));
    for (std::size_t offset = 0; offset < value.size(); offset += 8) {
        std::uint64_t word = 0;
        const std::size_t count =
            std::min<std::size_t>(8, value.size() - offset);
        for (std::size_t byte = 0; byte < count; ++byte) {
            word |= static_cast<std::uint64_t>(
                        static_cast<unsigned char>(value[offset + byte]))
                    << (byte * 8);
        }
        key.push_back(word);
    }
}

void append_semantic_strings(
    std::vector<std::uint64_t>& key,
    const std::vector<std::string>& values) {
    key.push_back(static_cast<std::uint64_t>(values.size()));
    for (const std::string& value : values) {
        append_semantic_string(key, value);
    }
}

void append_semantic_u32s(
    std::vector<std::uint64_t>& key,
    const std::vector<std::uint32_t>& values) {
    key.push_back(static_cast<std::uint64_t>(values.size()));
    for (const std::uint32_t value : values) key.push_back(value);
}

bool same_resource_quantities(
    const std::vector<std::pair<std::string, double>>& left,
    const std::vector<std::pair<std::string, double>>& right) {
    if (left.size() != right.size()) return false;
    for (std::size_t i = 0; i < left.size(); ++i) {
        if (left[i].first != right[i].first ||
            std::bit_cast<std::uint64_t>(left[i].second) !=
                std::bit_cast<std::uint64_t>(right[i].second)) {
            return false;
        }
    }
    return true;
}

} // namespace

std::vector<std::uint64_t> exact_abstract_state_key(
        const AbstractState& state,
        const std::uint32_t coarse_parent) {
    std::vector<std::uint64_t> key{
        0x7063727374617432ull, /* "pcrstat2" */
        coarse_parent,
        state.fractured_goal_mask,
        state.crafted_goal_mask,
        state.blocked_mask,
        state.prefix_count,
        state.suffix_count,
        state.rarity,
        state.influence_bits,
        static_cast<std::uint64_t>(state.veiled_side + 1),
        state.searing_exarch_tier,
        state.eater_of_worlds_tier,
        state.flags,
        state.fractured_metamod_flags,
        state.goal_progress_retry_basin,
    };
    for (const std::uint8_t status : state.slot_status) {
        key.push_back(status);
    }
    for (const std::uint32_t token :
         state.goal_member_class_tokens) {
        key.push_back(token);
    }
    const auto append_counts =
        [&](const CompactCountVector& counts) {
            key.push_back(counts.size());
            std::uint64_t nonzero = 0;
            for (std::size_t index = 0;
                 index < counts.size(); ++index) {
                if (counts[index] != 0) ++nonzero;
            }
            key.push_back(nonzero);
            for (std::size_t index = 0;
                 index < counts.size(); ++index) {
                const std::uint8_t value = counts[index];
                if (value == 0) continue;
                key.push_back(index);
                key.push_back(value);
            }
        };
    append_counts(state.junk_counts);
    append_counts(state.fractured_junk_counts);
    append_counts(state.crafted_junk_counts);
    append_counts(state.fractured_crafted_junk_counts);
    return key;
}

std::vector<std::uint64_t> planner_operator_semantic_key(
    const PlannerOperator& planner) {
    std::vector<std::uint64_t> key;
    key.reserve(
        32 + planner.primitive_program_action_ids.size() +
        planner.exit_goal_slots.size() +
        planner.resource_quantities.size() * 3);
    /* Versioned, length-delimited logical serialization. No hash is used:
     * equality of these vectors is collision-free semantic equality. */
    key.push_back(1);
    key.push_back(static_cast<std::uint8_t>(planner.kind));
    key.push_back(static_cast<std::uint8_t>(planner.option_kind));
    append_semantic_string(key, planner.primitive_action_id);
    append_semantic_strings(
        key, planner.primitive_program_action_ids);
    key.push_back(
        static_cast<std::uint8_t>(planner.intended_side));
    append_semantic_u32s(key, planner.exit_goal_slots);
    key.push_back(planner.exit_min_satisfied);
    key.push_back(planner.carrier_goal_slot);
    append_semantic_string(key, planner.conditional_action_id);
    append_semantic_string(key, planner.bestiary_create_action_id);
    append_semantic_string(key, planner.bestiary_restore_action_id);
    key.push_back(static_cast<std::uint8_t>(planner.automatic_kind));
    key.push_back(planner.relevant_goal_mask);
    append_semantic_string(key, planner.setup_action_id);
    append_semantic_string(key, planner.followup_action_id);
    append_semantic_string(key, planner.cleanup_action_id);
    append_semantic_string(
        key, planner.constructive_finish_action_id);
    key.push_back(
        static_cast<std::uint64_t>(
            planner.resource_quantities.size()));
    for (const auto& [resource, quantity] :
         planner.resource_quantities) {
        append_semantic_string(key, resource);
        key.push_back(std::bit_cast<std::uint64_t>(quantity));
    }
    return key;
}

bool planner_operator_structurally_equal(
    const PlannerOperator& left,
    const PlannerOperator& right) {
    return left.kind == right.kind &&
           left.option_kind == right.option_kind &&
           left.primitive_action_id == right.primitive_action_id &&
           left.primitive_program_action_ids ==
               right.primitive_program_action_ids &&
           left.intended_side == right.intended_side &&
           left.exit_goal_slots == right.exit_goal_slots &&
           left.exit_min_satisfied == right.exit_min_satisfied &&
           left.carrier_goal_slot == right.carrier_goal_slot &&
           left.conditional_action_id == right.conditional_action_id &&
           left.bestiary_create_action_id ==
               right.bestiary_create_action_id &&
           left.bestiary_restore_action_id ==
               right.bestiary_restore_action_id &&
           left.automatic_kind == right.automatic_kind &&
           left.relevant_goal_mask == right.relevant_goal_mask &&
           left.setup_action_id == right.setup_action_id &&
           left.followup_action_id == right.followup_action_id &&
           left.cleanup_action_id == right.cleanup_action_id &&
           left.constructive_finish_action_id ==
               right.constructive_finish_action_id &&
           same_resource_quantities(
               left.resource_quantities,
               right.resource_quantities);
}

PlannerOperatorRuntimeSemantics planner_operator_runtime_semantics(
        const PlannerOperator& planner,
        const ActionRegistry& registry) {
    PlannerOperatorRuntimeSemantics semantics;
    if (planner.option_kind == FixedOptionKind::ImprintRetry) {
        if (planner.kind != PlannerOperatorKind::FixedOption ||
            planner.bestiary_create_action == kNoId ||
            planner.bestiary_create_action_id.empty() ||
            planner.bestiary_restore_action == kNoId ||
            planner.bestiary_restore_action_id.empty()) {
            throw std::invalid_argument(
                "imprint retry planner operator has incomplete "
                "checkpoint dependencies");
        }
        /*
         * Checkpoint creation preserves the live item, but its legality
         * observes rarity and the immutable corruption/mirror flags. Restore
         * is the option kernel's internal exact return-to-entry loop, so it
         * has no outgoing policy continuation path of its own.
         */
        ActionDescriptor checkpoint_create;
        checkpoint_create.id =
            "internal:bestiary_imprint_checkpoint_create";
        checkpoint_create.refinement.schema_version =
            kActionRefinementContractVersion;
        checkpoint_create.refinement.observed_item_features =
            refinement_feature(RefinementFeature::Rarity) |
            refinement_feature(RefinementFeature::Corrupted) |
            refinement_feature(RefinementFeature::Mirrored);
        checkpoint_create.refinement.preserved_item_features =
            kAllRefinementItemFeatures;
        checkpoint_create.refinement.preserved_affixes.push_back({});
        RefinementAffixFlow identity;
        identity.preserved_features =
            kAllRefinementAffixFeatures;
        checkpoint_create.refinement.affix_flows.push_back(
            std::move(identity));
        canonicalize_and_validate_action_refinement_contract(
            checkpoint_create);
        semantics.ordered_program.push_back(
            {kNoId, std::move(checkpoint_create.refinement)});
    } else if (
        planner.bestiary_create_action != kNoId ||
        !planner.bestiary_create_action_id.empty() ||
        planner.bestiary_restore_action != kNoId ||
        !planner.bestiary_restore_action_id.empty()) {
        throw std::invalid_argument(
            "non-imprint planner operator has checkpoint dependencies");
    }
    const auto append_step =
        [&](const std::uint32_t action) {
            if (action == kNoId) {
                throw std::invalid_argument(
                    "planner operator runtime program contains no action");
            }
            if (action >= registry.actions.size()) {
                throw std::invalid_argument(
                    "planner operator has a runtime dependency outside "
                    "the action registry");
            }
            ActionDescriptor admitted = registry.actions[action];
            canonicalize_and_validate_action_refinement_contract(
                admitted);
            semantics.ordered_program.push_back(
                {action, std::move(admitted.refinement)});
        };
    for (const std::uint32_t action :
         planner.primitive_program) {
        append_step(action);
    }
    if (planner.conditional_action != kNoId &&
        std::none_of(
            semantics.ordered_program.begin(),
            semantics.ordered_program.end(),
            [&](const PlannerOperatorRuntimeStep& step) {
                return step.action ==
                       planner.conditional_action;
            })) {
        append_step(planner.conditional_action);
    }
    /*
     * constructive_finish_action witnesses synthesis/admission of an upper
     * policy. It is not executed by this PlannerOperator and therefore must
     * not broaden its observation or preservation contract.
     */
    if (semantics.ordered_program.empty()) {
        throw std::invalid_argument(
            "planner operator has no runtime action program");
    }
    const auto require_role_in_program =
        [&](const std::uint32_t action,
            const char* role) {
            if (action == kNoId) return;
            if (std::none_of(
                    semantics.ordered_program.begin(),
                    semantics.ordered_program.end(),
                    [&](const PlannerOperatorRuntimeStep& step) {
                        return step.action == action;
                    })) {
                throw std::invalid_argument(
                    std::string{"planner operator "} + role +
                    " is absent from its runtime action program");
            }
        };
    require_role_in_program(
        planner.setup_action, "setup action");
    require_role_in_program(
        planner.followup_action, "followup action");
    require_role_in_program(
        planner.cleanup_action, "cleanup action");
    semantics.action_dependencies.reserve(
        semantics.ordered_program.size());
    for (const PlannerOperatorRuntimeStep& step :
         semantics.ordered_program) {
        if (step.action != kNoId) {
            semantics.action_dependencies.push_back(step.action);
        }
    }
    std::sort(
        semantics.action_dependencies.begin(),
        semantics.action_dependencies.end());
    semantics.action_dependencies.erase(
        std::unique(
            semantics.action_dependencies.begin(),
            semantics.action_dependencies.end()),
        semantics.action_dependencies.end());
    if (semantics.action_dependencies.empty()) {
        throw std::invalid_argument(
            "planner operator has no runtime action dependency");
    }
    const auto add_execution_path =
        [&](std::vector<PlannerOperatorRuntimeStep> path) {
            if (path.empty()) return;
            const auto same_path =
                [&](const std::vector<PlannerOperatorRuntimeStep>&
                        existing) {
                    if (existing.size() != path.size()) return false;
                    for (std::size_t index = 0;
                         index < path.size(); ++index) {
                        if (existing[index].action !=
                            path[index].action) {
                            return false;
                        }
                    }
                    return true;
                };
            if (std::none_of(
                    semantics.execution_paths.begin(),
                    semantics.execution_paths.end(),
                    same_path)) {
                semantics.execution_paths.push_back(
                    std::move(path));
            }
        };
    if (planner.kind == PlannerOperatorKind::Primitive) {
        if (planner.primitive_action == kNoId ||
            semantics.ordered_program.size() != 1 ||
            semantics.ordered_program.front().action !=
                planner.primitive_action ||
            semantics.action_dependencies.size() != 1) {
            throw std::invalid_argument(
                "primitive planner operator has an inconsistent "
                "runtime program");
        }
        add_execution_path(semantics.ordered_program);
        semantics.compatibility_refinement =
            semantics.ordered_program.front().refinement;
        return semantics;
    }

    const std::size_t preparation_steps =
        planner.primitive_program.size();
    if (planner.conditional_action != kNoId) {
        const auto conditional = std::find_if(
            semantics.ordered_program.begin(),
            semantics.ordered_program.end(),
            [&](const PlannerOperatorRuntimeStep& step) {
                return step.action == planner.conditional_action;
            });
        if (conditional ==
            semantics.ordered_program.end()) {
            throw std::invalid_argument(
                "conditional planner action is absent from its "
                "runtime program");
        }
        /*
         * The option may enter with its carrier already prepared, finish a
         * complete preparation attempt without taking the conditional step,
         * or execute the conditional step after that complete attempt.
         * Individual preparation prefixes are not executable exits.
         */
        add_execution_path({*conditional});
        std::vector<PlannerOperatorRuntimeStep> preparation{
            semantics.ordered_program.begin(),
            semantics.ordered_program.begin() +
                static_cast<std::ptrdiff_t>(preparation_steps)};
        add_execution_path(preparation);
        if (preparation.empty() ||
            preparation.back().action !=
                planner.conditional_action) {
            preparation.push_back(*conditional);
        }
        add_execution_path(std::move(preparation));
    } else if (
        !semantics.ordered_program.empty() &&
        action_observes_modifier_offer(
            registry.actions.at(
                semantics.ordered_program.back().action))) {
        /*
         * An observed modifier offer may be empty. In that branch the
         * completed preparation is the whole path; otherwise the selected
         * choice is the final step.
         */
        add_execution_path(
            std::vector<PlannerOperatorRuntimeStep>{
                semantics.ordered_program.begin(),
                semantics.ordered_program.end() - 1});
        add_execution_path(semantics.ordered_program);
    } else {
        add_execution_path(semantics.ordered_program);
    }
    if (semantics.execution_paths.empty()) {
        add_execution_path(semantics.ordered_program);
    }
    std::sort(
        semantics.execution_paths.begin(),
        semantics.execution_paths.end(),
        [](const std::vector<PlannerOperatorRuntimeStep>& left,
           const std::vector<PlannerOperatorRuntimeStep>& right) {
            return std::lexicographical_compare(
                left.begin(), left.end(),
                right.begin(), right.end(),
                [](const PlannerOperatorRuntimeStep& a,
                   const PlannerOperatorRuntimeStep& b) {
                    return a.action < b.action;
                });
        });

    ActionRefinementContract& composite =
        semantics.compatibility_refinement;
    composite.schema_version =
        kActionRefinementContractVersion;
    for (const PlannerOperatorRuntimeStep& step :
         semantics.ordered_program) {
        const ActionRefinementContract& dependency =
            step.refinement;
        composite.observed_item_features |=
            dependency.observed_item_features;
        composite.preserved_item_features |=
            dependency.preserved_item_features;
        composite.destroyed_item_features |=
            dependency.destroyed_item_features;
        composite.observed_modifier_tag_ids.insert(
            composite.observed_modifier_tag_ids.end(),
            dependency.observed_modifier_tag_ids.begin(),
            dependency.observed_modifier_tag_ids.end());
        composite.affix_observations.insert(
            composite.affix_observations.end(),
            dependency.affix_observations.begin(),
            dependency.affix_observations.end());
        composite.item_affix_dependencies.insert(
            composite.item_affix_dependencies.end(),
            dependency.item_affix_dependencies.begin(),
            dependency.item_affix_dependencies.end());
        composite.affix_flows.insert(
            composite.affix_flows.end(),
            dependency.affix_flows.begin(),
            dependency.affix_flows.end());
        composite.preserved_affixes.insert(
            composite.preserved_affixes.end(),
            dependency.preserved_affixes.begin(),
            dependency.preserved_affixes.end());
        composite.destroyed_affixes.insert(
            composite.destroyed_affixes.end(),
            dependency.destroyed_affixes.begin(),
            dependency.destroyed_affixes.end());
        if (dependency.outcome_observation !=
            RefinementOutcomeObservation::None) {
            if (composite.outcome_observation !=
                    RefinementOutcomeObservation::None &&
                composite.outcome_observation !=
                    dependency.outcome_observation) {
                throw std::invalid_argument(
                    "planner runtime combines incompatible observed-choice "
                    "vocabularies");
            }
            composite.outcome_observation =
                dependency.outcome_observation;
        }
    }
    composite.resets_to_fresh_item = false;

    std::sort(
        composite.observed_modifier_tag_ids.begin(),
        composite.observed_modifier_tag_ids.end());
    composite.observed_modifier_tag_ids.erase(
        std::unique(
            composite.observed_modifier_tag_ids.begin(),
            composite.observed_modifier_tag_ids.end()),
        composite.observed_modifier_tag_ids.end());
    const auto selector_less =
        [](const RefinementAffixSelector& left,
           const RefinementAffixSelector& right) {
            const auto left_scalars = std::tie(
                left.required_affix_traits,
                left.forbidden_affix_traits,
                left.required_item_traits,
                left.forbidden_item_traits);
            const auto right_scalars = std::tie(
                right.required_affix_traits,
                right.forbidden_affix_traits,
                right.required_item_traits,
                right.forbidden_item_traits);
            return left_scalars != right_scalars
                       ? left_scalars < right_scalars
                       : left.required_tag_ids <
                             right.required_tag_ids;
        };
    const auto canonicalize_selectors =
        [&](std::vector<RefinementAffixSelector>& selectors) {
            std::sort(
                selectors.begin(), selectors.end(),
                selector_less);
            selectors.erase(
                std::unique(
                    selectors.begin(), selectors.end()),
                selectors.end());
        };
    canonicalize_selectors(composite.preserved_affixes);
    canonicalize_selectors(composite.destroyed_affixes);

    std::sort(
        composite.affix_observations.begin(),
        composite.affix_observations.end(),
        [&](const RefinementAffixObservation& left,
            const RefinementAffixObservation& right) {
            return selector_less(
                left.selector, right.selector);
        });
    std::vector<RefinementAffixObservation>
        merged_observations;
    for (RefinementAffixObservation observation :
         composite.affix_observations) {
        if (!merged_observations.empty() &&
            merged_observations.back().selector ==
                observation.selector) {
            merged_observations.back().features |=
                observation.features;
        } else {
            merged_observations.push_back(
                std::move(observation));
        }
    }
    composite.affix_observations =
        std::move(merged_observations);

    std::sort(
        composite.item_affix_dependencies.begin(),
        composite.item_affix_dependencies.end(),
        [](const RefinementItemAffixDependency& left,
           const RefinementItemAffixDependency& right) {
            return std::tie(
                       left.item_features,
                       left.survivor_affix_features) <
                   std::tie(
                       right.item_features,
                       right.survivor_affix_features);
        });
    composite.item_affix_dependencies.erase(
        std::unique(
            composite.item_affix_dependencies.begin(),
            composite.item_affix_dependencies.end()),
        composite.item_affix_dependencies.end());
    std::sort(
        composite.affix_flows.begin(),
        composite.affix_flows.end(),
        [&](const RefinementAffixFlow& left,
            const RefinementAffixFlow& right) {
            if (selector_less(
                    left.source_selector,
                    right.source_selector)) {
                return true;
            }
            if (selector_less(
                    right.source_selector,
                    left.source_selector)) {
                return false;
            }
            return std::tie(
                       left.set_affix_traits,
                       left.cleared_affix_traits,
                       left.preserved_features,
                       left.preserves_modifier_classification) <
                   std::tie(
                       right.set_affix_traits,
                       right.cleared_affix_traits,
                       right.preserved_features,
                       right.preserves_modifier_classification);
        });
    composite.affix_flows.erase(
        std::unique(
            composite.affix_flows.begin(),
            composite.affix_flows.end()),
        composite.affix_flows.end());
    return semantics;
}

bool fixed_option_choice_retries_locally(
        const std::uint32_t entry_state,
        const OptionKernel& kernel,
        const std::uint32_t successor_state,
        const std::uint32_t actual_state,
        const std::vector<std::uint32_t>&
            behavioral_representative_by_state) {
    const auto same_behavioral_state =
        [&](const std::uint32_t left,
            const std::uint32_t right) {
            if (left == right) return true;
            if (left == kNoId || right == kNoId ||
                behavioral_representative_by_state.empty() ||
                left >=
                    behavioral_representative_by_state.size()) {
                return false;
            }
            const std::uint32_t projected_left =
                behavioral_representative_by_state[left];
            if (projected_left == right) return true;
            return right <
                       behavioral_representative_by_state.size() &&
                   projected_left ==
                       behavioral_representative_by_state[right];
        };
    if (successor_state == kNoId ||
        same_behavioral_state(successor_state, entry_state) ||
        std::find(
            kernel.retry_states.begin(),
            kernel.retry_states.end(),
            actual_state) != kernel.retry_states.end()) {
        return true;
    }
    return std::any_of(
        kernel.retry_states.begin(),
        kernel.retry_states.end(),
        [&](const std::uint32_t retry_state) {
            return same_behavioral_state(
                retry_state, actual_state);
        });
}

ExecutableFixedOptionRecipe fixed_option_executable_recipe(
        const CalcContext& calc,
        const std::uint32_t entry_state,
        const PlannerOperator& planner,
        const OptionKernel& kernel,
        const std::vector<ObservedUnveilPreference>& preferences,
        const std::vector<std::uint32_t>&
            behavioral_representative_by_state) {
    if (planner.kind != PlannerOperatorKind::FixedOption) {
        throw std::invalid_argument(
            "executable fixed-option recipe received a primitive "
            "operator");
    }
    if (entry_state >= calc.state_count()) {
        throw std::invalid_argument(
            "executable fixed-option recipe entry is outside the "
            "state table");
    }
    if (!kernel.supported || !kernel.legal ||
        !kernel.terminates_almost_surely) {
        throw std::invalid_argument(
            "fixed-option member has no legal exact kernel");
    }

    const auto state_key =
        [&](const std::uint32_t state,
            const char* subject) {
            const std::uint32_t resolved =
                state == kNoId ? entry_state : state;
            if (resolved >= calc.state_count()) {
                throw std::invalid_argument(
                    std::string(subject) +
                    " references a state outside the exact table");
            }
            return exact_abstract_state_key(
                calc.state(resolved), 0);
        };
    const auto canonical_state_keys =
        [&](const std::vector<std::uint32_t>& states,
            const char* subject) {
            std::vector<std::vector<std::uint64_t>> keys;
            keys.reserve(states.size());
            for (const std::uint32_t state : states) {
                keys.push_back(state_key(state, subject));
            }
            std::sort(keys.begin(), keys.end());
            keys.erase(
                std::unique(keys.begin(), keys.end()),
                keys.end());
            return keys;
        };

    ExecutableFixedOptionRecipe recipe;
    recipe.entry_continues = kernel.entry_continues;
    const bool renewal_route =
        planner.option_kind == FixedOptionKind::Renewal ||
        planner.option_kind == FixedOptionKind::ProtectedRepeat ||
        planner.option_kind ==
            FixedOptionKind::TemporaryBenchRepeat;
    bool observed = false;
    if (!planner.primitive_program.empty()) {
        const std::uint32_t final_action =
            planner.primitive_program.back();
        if (final_action >= calc.registry().actions.size()) {
            throw std::invalid_argument(
                "fixed option has an invalid primitive program");
        }
        observed = action_observes_modifier_offer(
            calc.registry().actions[final_action]);
    }
    if (observed && !renewal_route) {
        throw std::invalid_argument(
            "fixed option has an unsupported observed execution "
            "recipe");
    }

    if (planner.option_kind == FixedOptionKind::ImprintRetry ||
        (renewal_route && !observed) ||
        (planner.option_kind ==
             FixedOptionKind::FracturePrepare &&
         !kernel.entry_continues)) {
        recipe.retry_state_keys =
            canonical_state_keys(
                kernel.retry_states,
                "fixed-option retry predicate");
    }
    if (planner.option_kind ==
            FixedOptionKind::FracturePrepare &&
        !kernel.entry_continues) {
        recipe.continuation_state_keys =
            canonical_state_keys(
                kernel.continuation_states,
                "fixed-option continuation predicate");
        std::vector<std::vector<std::uint64_t>> overlap;
        std::set_intersection(
            recipe.retry_state_keys.begin(),
            recipe.retry_state_keys.end(),
            recipe.continuation_state_keys.begin(),
            recipe.continuation_state_keys.end(),
            std::back_inserter(overlap));
        if (!overlap.empty()) {
            throw std::invalid_argument(
                "fixed option has overlapping retry and "
                "continuation predicates");
        }
    }

    if (!observed) {
        if (!preferences.empty() ||
            !kernel.observation_choice_options.empty() ||
            !kernel.observation_choice_groups.empty()) {
            throw std::invalid_argument(
                "unobserved fixed option has an unexpected choice "
                "sidecar");
        }
        return recipe;
    }
    if (preferences.empty() ||
        kernel.observation_choice_options.empty()) {
        throw std::invalid_argument(
            "observed fixed option has no populated choice sidecar");
    }

    std::map<
        std::vector<std::uint64_t>,
        std::map<std::uint32_t, bool>>
        offered_by_observation;
    for (const OutcomeChoiceOption& option :
         kernel.observation_choice_options) {
        if (option.mod_id >= calc.session().mod_count) {
            throw std::invalid_argument(
                "observed fixed option exposes an invalid modifier");
        }
        const std::vector<std::uint64_t> observation =
            state_key(
                option.observation_state,
                "fixed-option observation");
        const bool retry_local =
            fixed_option_choice_retries_locally(
                entry_state, kernel, option.state,
                option.actual_state,
                behavioral_representative_by_state);
        const auto [found, inserted] =
            offered_by_observation[observation].emplace(
                option.mod_id, retry_local);
        if (!inserted && found->second != retry_local) {
            throw std::invalid_argument(
                "one observed modifier has conflicting branch roles");
        }
    }

    std::set<std::vector<std::uint64_t>>
        seen_observations;
    recipe.offers.reserve(preferences.size());
    for (const ObservedUnveilPreference& preference :
         preferences) {
        ExecutableFixedOptionOffer offer;
        offer.observation_state_key =
            state_key(
                preference.observation_state,
                "fixed-option preference observation");
        if (!seen_observations.insert(
                offer.observation_state_key).second) {
            throw std::invalid_argument(
                "fixed-option preference repeats an observation");
        }
        const auto available =
            offered_by_observation.find(
                offer.observation_state_key);
        if (available == offered_by_observation.end() ||
            preference.choices.empty()) {
            throw std::invalid_argument(
                "fixed-option preference does not match an observed "
                "offer");
        }
        for (const auto& [mod, unused] : available->second) {
            (void)unused;
            offer.offered_mod_ids.push_back(mod);
        }
        std::set<std::uint32_t> seen_mods;
        for (const ObservedUnveilChoice& choice :
             preference.choices) {
            if (choice.mod_id >= calc.session().mod_count ||
                !seen_mods.insert(choice.mod_id).second) {
                throw std::invalid_argument(
                    "fixed-option preference has an invalid or "
                    "repeated modifier");
            }
            const auto offered =
                available->second.find(choice.mod_id);
            if (offered == available->second.end()) {
                throw std::invalid_argument(
                    "fixed-option preference chooses outside its "
                    "offer");
            }
            const bool retry_local =
                fixed_option_choice_retries_locally(
                    entry_state, kernel,
                    choice.successor_state,
                    choice.actual_state,
                    behavioral_representative_by_state);
            if (retry_local != offered->second) {
                throw std::invalid_argument(
                    "fixed-option preference gives an offered "
                    "modifier the wrong branch role");
            }
            offer.ordered_choices.push_back({
                choice.mod_id, retry_local});
        }
        if (seen_mods.size() != available->second.size()) {
            throw std::invalid_argument(
                "fixed-option preference does not cover its offer");
        }
        recipe.offers.push_back(std::move(offer));
    }
    if (recipe.offers.size() !=
        offered_by_observation.size()) {
        throw std::invalid_argument(
            "fixed-option preferences do not cover every offer");
    }
    std::sort(
        recipe.offers.begin(), recipe.offers.end(),
        [](const ExecutableFixedOptionOffer& left,
           const ExecutableFixedOptionOffer& right) {
            return left.observation_state_key <
                   right.observation_state_key;
        });
    return recipe;
}

std::vector<std::uint64_t> fixed_option_executable_recipe_key(
        const ExecutableFixedOptionRecipe& recipe) {
    std::vector<std::uint64_t> key{
        0x706366786f707431ull, /* "pcfxopt1" */
        recipe.entry_continues ? 1u : 0u};
    const auto append_state_keys =
        [&](const std::vector<std::vector<std::uint64_t>>& states) {
            key.push_back(states.size());
            for (const std::vector<std::uint64_t>& state : states) {
                key.push_back(state.size());
                key.insert(
                    key.end(), state.begin(), state.end());
            }
        };
    append_state_keys(recipe.retry_state_keys);
    append_state_keys(recipe.continuation_state_keys);
    key.push_back(recipe.offers.size());
    for (const ExecutableFixedOptionOffer& offer :
         recipe.offers) {
        key.push_back(offer.observation_state_key.size());
        key.insert(
            key.end(),
            offer.observation_state_key.begin(),
            offer.observation_state_key.end());
        key.push_back(offer.offered_mod_ids.size());
        for (const std::uint32_t mod :
             offer.offered_mod_ids) {
            key.push_back(mod);
        }
        key.push_back(offer.ordered_choices.size());
        for (const ExecutableFixedOptionChoice& choice :
             offer.ordered_choices) {
            key.push_back(choice.mod_id);
            key.push_back(choice.retry_local ? 1u : 0u);
        }
    }
    return key;
}

namespace {

bool same_complete_option_kernel(
    const OptionKernel& left,
    const OptionKernel& right) {
    return left.legal == right.legal &&
           left.supported == right.supported &&
           left.terminates_almost_surely == right.terminates_almost_surely &&
           left.expected_primitive_actions ==
               right.expected_primitive_actions &&
           left.expected_resources == right.expected_resources &&
           left.exits == right.exits &&
           left.observation_choice_groups ==
               right.observation_choice_groups &&
           left.observation_choice_options ==
               right.observation_choice_options &&
           left.retry_states == right.retry_states &&
           left.continuation_states == right.continuation_states &&
           left.entry_continues == right.entry_continues;
}

bool same_option_transition_kernel(
    const OptionKernel& left,
    const OptionKernel& right) {
    return left.legal == right.legal &&
           left.supported == right.supported &&
           left.terminates_almost_surely == right.terminates_almost_surely &&
           left.expected_primitive_actions ==
               right.expected_primitive_actions &&
           left.exits == right.exits &&
           left.observation_choice_groups ==
               right.observation_choice_groups &&
           left.observation_choice_options ==
               right.observation_choice_options &&
           left.retry_states == right.retry_states &&
           left.continuation_states == right.continuation_states &&
           left.entry_continues == right.entry_continues;
}

bool same_option_template_planner(
    const PlannerOperator& left,
    const PlannerOperator& right) {
    return planner_operator_structurally_equal(left, right);
}

std::uint64_t option_planner_hash(const PlannerOperator& planner) {
    std::uint64_t hash = 1469598103934665603ull;
    const auto mix = [&](const std::uint64_t value) {
        hash ^= value;
        hash *= 1099511628211ull;
    };
    for (const std::uint64_t word :
         planner_operator_semantic_key(planner)) {
        mix(word);
    }
    return hash == 0 ? 1 : hash;
}

std::uint64_t option_template_hash(
    const PlannerOperator& planner,
    const OptionKernel& kernel) {
    std::uint64_t hash = 1469598103934665603ull;
    const auto mix = [&](const std::uint64_t value) {
        hash ^= value;
        hash *= 1099511628211ull;
    };
    for (const std::uint64_t word :
         planner_operator_semantic_key(planner)) {
        mix(word);
    }
    mix(kernel.legal ? 1u : 0u);
    mix(kernel.supported ? 1u : 0u);
    mix(kernel.terminates_almost_surely ? 1u : 0u);
    mix(kernel.entry_continues ? 1u : 0u);
    mix(std::bit_cast<std::uint64_t>(kernel.expected_primitive_actions));
    for (const auto& [key, quantity] : kernel.expected_resources) {
        for (const unsigned char c : key) mix(c);
        mix(0xffu);
        mix(std::bit_cast<std::uint64_t>(quantity));
    }
    mix(kNoId);
    for (const OutcomeEntry& exit : kernel.exits) {
        mix(exit.state);
        mix(std::bit_cast<std::uint64_t>(exit.probability));
    }
    mix(kNoId);
    for (const OutcomeChoiceGroup& group :
         kernel.observation_choice_groups) {
        mix(std::bit_cast<std::uint64_t>(group.probability));
        mix(group.observation_state);
        for (const std::uint32_t state : group.states) mix(state);
        mix(kNoId);
    }
    for (const OutcomeChoiceOption& choice :
         kernel.observation_choice_options) {
        mix(choice.mod_id);
        mix(choice.state);
        mix(choice.observation_state);
        mix(choice.actual_state);
    }
    for (const std::uint32_t state : kernel.retry_states) mix(state);
    mix(kNoId);
    for (const std::uint32_t state : kernel.continuation_states) mix(state);
    return hash == 0 ? 1 : hash;
}

std::uint64_t option_transition_hash(const OptionKernel& kernel) {
    std::uint64_t hash = 1469598103934665603ull;
    const auto mix = [&](const std::uint64_t value) {
        hash ^= value;
        hash *= 1099511628211ull;
    };
    mix(kernel.legal ? 1u : 0u);
    mix(kernel.supported ? 1u : 0u);
    mix(kernel.terminates_almost_surely ? 1u : 0u);
    mix(kernel.entry_continues ? 1u : 0u);
    mix(std::bit_cast<std::uint64_t>(kernel.expected_primitive_actions));
    for (const OutcomeEntry& exit : kernel.exits) {
        mix(exit.state);
        mix(std::bit_cast<std::uint64_t>(exit.probability));
    }
    mix(kNoId);
    for (const OutcomeChoiceGroup& group :
         kernel.observation_choice_groups) {
        mix(std::bit_cast<std::uint64_t>(group.probability));
        mix(group.observation_state);
        for (const std::uint32_t state : group.states) mix(state);
        mix(kNoId);
    }
    for (const OutcomeChoiceOption& choice :
         kernel.observation_choice_options) {
        mix(choice.mod_id);
        mix(choice.state);
        mix(choice.observation_state);
        mix(choice.actual_state);
    }
    for (const std::uint32_t state : kernel.retry_states) mix(state);
    mix(kNoId);
    for (const std::uint32_t state : kernel.continuation_states) mix(state);
    return hash == 0 ? 1 : hash;
}

std::uint64_t option_kernel_selected_bytes(const OptionKernel& kernel) {
    std::uint64_t bytes = sizeof(kernel);
    bytes += kernel.expected_resources.capacity() *
             sizeof(std::pair<std::string, double>);
    for (const auto& [key, unused] : kernel.expected_resources) {
        (void)unused;
        bytes += key.capacity() + 1;
    }
    bytes += kernel.exits.capacity() * sizeof(OutcomeEntry);
    bytes += kernel.observation_choice_groups.capacity() *
             sizeof(OutcomeChoiceGroup);
    for (const OutcomeChoiceGroup& group :
         kernel.observation_choice_groups) {
        bytes += group.states.capacity() * sizeof(std::uint32_t);
    }
    bytes += kernel.observation_choice_options.capacity() *
             sizeof(OutcomeChoiceOption);
    bytes += kernel.retry_states.capacity() * sizeof(std::uint32_t);
    bytes += kernel.continuation_states.capacity() * sizeof(std::uint32_t);
    bytes += kernel.automatic_candidate_attempt_entries.capacity() *
             sizeof(OutcomeEntry);
    bytes += kernel.automatic.legality_result.capacity() + 1;
    bytes += kernel.automatic.reason.capacity() + 1;
    return bytes;
}

std::string temporary_evaluation_key(
    const SessionImpl& session,
    const ActionRegistry& registry,
    const PlannerOperator& planner) {
    if (planner.option_kind != FixedOptionKind::TemporaryBenchRepeat ||
        planner.setup_action == kNoId ||
        planner.followup_action == kNoId ||
        planner.cleanup_action == kNoId) {
        return {};
    }
    const ActionDescriptor& blocker =
        registry.actions.at(planner.setup_action);
    if (blocker.params.mod_id >= session.mod_count) return {};
    std::string key =
        std::to_string(planner.followup_action) + ':' +
        std::to_string(planner.cleanup_action) + ':' +
        std::to_string(planner.exit_min_satisfied) + ':' +
        std::to_string(
            static_cast<int>(session.gen_type[blocker.params.mod_id])) + ':';
    for (const std::uint32_t slot : planner.exit_goal_slots) {
        key += std::to_string(slot) + ',';
    }
    key.push_back(':');
    std::vector<std::uint32_t> groups;
    for (std::uint32_t row = session.group_offsets[blocker.params.mod_id];
         row < session.group_offsets[blocker.params.mod_id + 1]; ++row) {
        groups.push_back(session.group_ids[row]);
    }
    std::sort(groups.begin(), groups.end());
    groups.erase(std::unique(groups.begin(), groups.end()), groups.end());
    for (const std::uint32_t group : groups) {
        key += std::to_string(group) + ',';
    }
    return key;
}

PlannerOperator temporary_variant_planner(
    const ActionRegistry& registry,
    const PlannerOperator& representative,
    const OptionKernel& kernel,
    const std::uint32_t blocker_action) {
    PlannerOperator variant = representative;
    const ActionDescriptor& old_blocker =
        registry.actions.at(representative.setup_action);
    const ActionDescriptor& blocker = registry.actions.at(blocker_action);
    const ActionDescriptor& followup =
        registry.actions.at(representative.followup_action);
    variant.setup_action = blocker_action;
    if (!variant.primitive_program.empty()) {
        variant.primitive_program.front() = blocker_action;
    }
    variant.id = "option:temporary_bench_repeat:" + blocker.id + ':' +
                 followup.id + ":until:" +
                 std::to_string(variant.exit_min_satisfied) + ':';
    for (std::size_t i = 0; i < variant.exit_goal_slots.size(); ++i) {
        if (i != 0) variant.id.push_back('+');
        variant.id += std::to_string(variant.exit_goal_slots[i]);
    }
    variant.display_name = "Temporary " + blocker.display_name +
                           " then repeat " + followup.display_name;

    std::map<std::string, double> resources(
        kernel.expected_resources.begin(), kernel.expected_resources.end());
    for (const std::string& key : old_blocker.cost_keys) {
        auto found = resources.find(key);
        if (found == resources.end()) continue;
        found->second -= 1.0;
        if (std::abs(found->second) <= 1e-15) resources.erase(found);
    }
    for (const std::string& key : blocker.cost_keys) resources[key] += 1.0;
    variant.resource_quantities.assign(resources.begin(), resources.end());
    bind_planner_primitive_action_ids(registry, variant);
    return variant;
}

bool same_complete_distribution(
    const OutcomeDistribution& left,
    const OutcomeDistribution& right) {
    return left.supported == right.supported &&
           left.entries == right.entries &&
           left.choice_groups == right.choice_groups &&
           left.choice_options == right.choice_options &&
           left.slot_satisfied_probability ==
               right.slot_satisfied_probability;
}

std::uint64_t mapped_kernel_hash(const OptionKernel& kernel) {
    AttemptKernel attempt;
    attempt.supported = kernel.supported;
    attempt.fully_legal = kernel.legal;
    attempt.entries = kernel.exits;
    attempt.choice_groups = kernel.observation_choice_groups;
    attempt.choice_options = kernel.observation_choice_options;
    return attempt_kernel_hash(attempt);
}

std::uint32_t map_local_state(
    CalcContext& local,
    CalcContext& destination,
    const std::uint32_t local_state,
    std::unordered_map<std::uint32_t, std::uint32_t>& mapped) {
    if (local_state == kNoId) return kNoId;
    const auto found = mapped.find(local_state);
    if (found != mapped.end()) return found->second;
    pc_item_state item;
    if (!local.materialize(local_state, item)) {
        throw std::runtime_error(
            "automatic candidate local state cannot be materialized");
    }
    const std::uint32_t result = destination.intern_item(item);
    mapped.emplace(local_state, result);
    return result;
}

OutcomeDistribution map_local_distribution(
    CalcContext& local,
    CalcContext& destination,
    const OutcomeDistribution& source,
    std::unordered_map<std::uint32_t, std::uint32_t>& mapped) {
    OutcomeDistribution result = source;
    std::map<std::uint32_t, double> entries;
    for (const OutcomeEntry& entry : source.entries) {
        entries[map_local_state(
            local, destination, entry.state, mapped)] += entry.probability;
    }
    result.entries.clear();
    for (const auto& [state, probability] : entries) {
        result.entries.push_back({state, probability});
    }
    for (OutcomeChoiceGroup& group : result.choice_groups) {
        group.observation_state = map_local_state(
            local, destination, group.observation_state, mapped);
        for (std::uint32_t& state : group.states) {
            state = map_local_state(local, destination, state, mapped);
        }
        std::sort(group.states.begin(), group.states.end());
        group.states.erase(
            std::unique(group.states.begin(), group.states.end()),
            group.states.end());
    }
    for (OutcomeChoiceOption& choice : result.choice_options) {
        choice.state = map_local_state(
            local, destination, choice.state, mapped);
        choice.observation_state = map_local_state(
            local, destination, choice.observation_state, mapped);
        choice.actual_state = map_local_state(
            local, destination, choice.actual_state, mapped);
    }
    result.stable_shared_kernel = false;
    return result;
}

OptionKernel map_local_option_kernel(
    CalcContext& local,
    CalcContext& destination,
    const OptionKernel& source,
    std::unordered_map<std::uint32_t, std::uint32_t>& mapped) {
    OptionKernel result = source;
    result.retained_template_storage = false;
    std::map<std::uint32_t, double> exits;
    for (const OutcomeEntry& exit : source.exits) {
        exits[map_local_state(
            local, destination, exit.state, mapped)] += exit.probability;
    }
    result.exits.clear();
    for (const auto& [state, probability] : exits) {
        result.exits.push_back({state, probability});
    }
    for (OutcomeChoiceGroup& group : result.observation_choice_groups) {
        group.observation_state = map_local_state(
            local, destination, group.observation_state, mapped);
        for (std::uint32_t& state : group.states) {
            state = map_local_state(local, destination, state, mapped);
        }
        std::sort(group.states.begin(), group.states.end());
        group.states.erase(
            std::unique(group.states.begin(), group.states.end()),
            group.states.end());
    }
    for (OutcomeChoiceOption& choice : result.observation_choice_options) {
        choice.state = map_local_state(
            local, destination, choice.state, mapped);
        choice.observation_state = map_local_state(
            local, destination, choice.observation_state, mapped);
        choice.actual_state = map_local_state(
            local, destination, choice.actual_state, mapped);
    }
    for (std::uint32_t& state : result.retry_states) {
        state = map_local_state(local, destination, state, mapped);
    }
    for (std::uint32_t& state : result.continuation_states) {
        state = map_local_state(local, destination, state, mapped);
    }
    result.automatic_candidate_attempt_entries.clear();
    result.automatic_candidate_attempt_entries.shrink_to_fit();
    std::sort(result.retry_states.begin(), result.retry_states.end());
    result.retry_states.erase(
        std::unique(result.retry_states.begin(), result.retry_states.end()),
        result.retry_states.end());
    std::sort(
        result.continuation_states.begin(),
        result.continuation_states.end());
    result.continuation_states.erase(
        std::unique(
            result.continuation_states.begin(),
            result.continuation_states.end()),
        result.continuation_states.end());
    result.automatic.candidate_kernel_hash = mapped_kernel_hash(result);
    return result;
}

void require_import_reference_shape(
    const std::uint32_t index,
    const std::string& id,
    const char* field) {
    if ((index == kNoId) != id.empty()) {
        throw std::invalid_argument(
            std::string("planner operator has an incomplete ") +
            field + " reference");
    }
}

std::uint32_t resolve_imported_primitive(
    const ActionRegistry& registry,
    const std::string& id,
    const char* field) {
    const auto found = registry.index_by_id.find(id);
    if (found == registry.index_by_id.end() ||
        found->second >= registry.actions.size() ||
        registry.actions[found->second].id != id) {
        throw std::invalid_argument(
            std::string("planner operator ") + field +
            " dependency is absent from the destination registry: " + id);
    }
    const ActionDescriptor& action = registry.actions[found->second];
    if (!calc_supports(action)) {
        throw std::invalid_argument(
            std::string("planner operator ") + field +
            " dependency has no exact calculator authority: " + id);
    }
    validate_action_refinement_contract(action);
    return found->second;
}

std::uint32_t resolve_imported_bestiary(
    const SessionImpl& session,
    const std::string& id,
    const char* field) {
    std::uint32_t result = kNoId;
    for (std::uint32_t index = 0;
         index < session.data->bestiary_actions.size(); ++index) {
        if (session.data->bestiary_actions[index].id != id) continue;
        if (result != kNoId) {
            throw std::invalid_argument(
                std::string("planner operator ") + field +
                " dependency is ambiguous in the destination session: " +
                id);
        }
        result = index;
    }
    if (result == kNoId) {
        throw std::invalid_argument(
            std::string("planner operator ") + field +
            " dependency is absent from the destination session: " + id);
    }
    return result;
}

} // namespace

std::uint32_t CalcContext::import_planner_operator(
    const PlannerOperator& planner,
    const bool state_local) {
    require_import_reference_shape(
        planner.primitive_action,
        planner.primitive_action_id,
        "primitive_action");
    if (planner.primitive_program.size() !=
        planner.primitive_program_action_ids.size()) {
        throw std::invalid_argument(
            "planner operator primitive program is missing canonical ids");
    }
    for (std::size_t i = 0;
         i < planner.primitive_program.size(); ++i) {
        require_import_reference_shape(
            planner.primitive_program[i],
            planner.primitive_program_action_ids[i],
            "primitive_program");
    }
    require_import_reference_shape(
        planner.conditional_action,
        planner.conditional_action_id,
        "conditional_action");
    require_import_reference_shape(
        planner.bestiary_create_action,
        planner.bestiary_create_action_id,
        "bestiary_create_action");
    require_import_reference_shape(
        planner.bestiary_restore_action,
        planner.bestiary_restore_action_id,
        "bestiary_restore_action");
    require_import_reference_shape(
        planner.setup_action,
        planner.setup_action_id,
        "setup_action");
    require_import_reference_shape(
        planner.followup_action,
        planner.followup_action_id,
        "followup_action");
    require_import_reference_shape(
        planner.cleanup_action,
        planner.cleanup_action_id,
        "cleanup_action");
    require_import_reference_shape(
        planner.constructive_finish_action,
        planner.constructive_finish_action_id,
        "constructive_finish_action");

    PlannerOperator mapped = planner;
    const auto map_optional_primitive =
        [&](const std::string& id,
            const char* field) -> std::uint32_t {
        return id.empty()
                   ? kNoId
                   : resolve_imported_primitive(
                         registry_, id, field);
    };
    mapped.primitive_action = map_optional_primitive(
        planner.primitive_action_id, "primitive_action");
    mapped.primitive_program.clear();
    mapped.primitive_program.reserve(
        planner.primitive_program_action_ids.size());
    for (const std::string& id :
         planner.primitive_program_action_ids) {
        if (id.empty()) {
            throw std::invalid_argument(
                "planner operator primitive program contains an empty id");
        }
        mapped.primitive_program.push_back(
            resolve_imported_primitive(
                registry_, id, "primitive_program"));
    }
    mapped.conditional_action = map_optional_primitive(
        planner.conditional_action_id, "conditional_action");
    mapped.setup_action = map_optional_primitive(
        planner.setup_action_id, "setup_action");
    mapped.followup_action = map_optional_primitive(
        planner.followup_action_id, "followup_action");
    mapped.cleanup_action = map_optional_primitive(
        planner.cleanup_action_id, "cleanup_action");
    mapped.constructive_finish_action = map_optional_primitive(
        planner.constructive_finish_action_id,
        "constructive_finish_action");
    mapped.bestiary_create_action =
        planner.bestiary_create_action_id.empty()
            ? kNoId
            : resolve_imported_bestiary(
                  *session_,
                  planner.bestiary_create_action_id,
                  "bestiary_create_action");
    mapped.bestiary_restore_action =
        planner.bestiary_restore_action_id.empty()
            ? kNoId
            : resolve_imported_bestiary(
                  *session_,
                  planner.bestiary_restore_action_id,
                  "bestiary_restore_action");

    if (mapped.kind == PlannerOperatorKind::Primitive) {
        if (mapped.primitive_action == kNoId ||
            mapped.primitive_program.size() != 1 ||
            mapped.primitive_program.front() !=
                mapped.primitive_action) {
            throw std::invalid_argument(
                "primitive planner operator must name its one exact "
                "primitive dependency");
        }
        if (mapped.primitive_action >= operators_.size() ||
            operators_[mapped.primitive_action].kind !=
                PlannerOperatorKind::Primitive ||
            !planner_operator_structurally_equal(
                operators_[mapped.primitive_action], mapped)) {
            throw std::invalid_argument(
                "primitive planner operator semantics differ from the "
                "destination registry wrapper");
        }
        return mapped.primitive_action;
    }
    if (mapped.kind != PlannerOperatorKind::FixedOption) {
        throw std::invalid_argument(
            "planner operator kind is not supported");
    }
    if (mapped.primitive_action != kNoId ||
        mapped.primitive_program.empty()) {
        throw std::invalid_argument(
            "fixed planner option has an invalid primitive dependency "
            "shape");
    }
    /*
     * Imported automatic operators cross CalcContext boundaries. Apply the
     * same complete runtime-path admission contract as initial planner
     * construction before an invalid composite can enter the candidate set.
     */
    (void)planner_operator_runtime_semantics(mapped, registry_);

    for (std::uint32_t index = 0; index < operators_.size(); ++index) {
        if (!planner_operator_structurally_equal(
                operators_[index], mapped)) {
            continue;
        }
        if (state_local) {
            state_local_automatic_operator_indices_.insert(index);
        }
        return index;
    }
    if (operators_.size() >=
        static_cast<std::size_t>(kNoId)) {
        throw std::overflow_error(
            "planner operator index space exhausted");
    }
    const std::uint32_t result =
        static_cast<std::uint32_t>(operators_.size());
    operators_.push_back(std::move(mapped));
    account_new_operator(operators_.back());
    const std::uint64_t template_id =
        option_planner_hash(operators_.back());
    auto& bucket = option_operator_templates_[template_id];
    const std::size_t old_capacity = bucket.capacity();
    bucket.push_back(result);
    account_operator_template_insert(old_capacity, bucket);
    if (state_local) {
        state_local_automatic_operator_indices_.insert(result);
    }
    return result;
}

void CalcContext::initialize_temporary_bench_effect_classes() {
    const auto started = std::chrono::steady_clock::now();
    automatic_goal_bench_actions_.clear();
    temporary_bench_effect_classes_.clear();
    temporary_bench_precompiled_bytes_ = 0;
    if (!goal_.automatic_candidates ||
        !registry_.index_by_id.contains("remove_crafted_modifiers")) {
        temporary_bench_precompile_ns_ = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - started)
                .count());
        return;
    }

    std::vector<std::uint32_t> ordinary_bench;
    for (std::uint32_t index = 0; index < registry_.actions.size(); ++index) {
        const ActionDescriptor& action = registry_.actions[index];
        if (action.params.type != ActionType::Bench ||
            action.params.mod_id >= session_->metamod_type.size() ||
            session_->metamod_type[action.params.mod_id] >= 0) {
            continue;
        }
        if (goal_mask_for_mod(*session_, goal_, action.params.mod_id) != 0) {
            automatic_goal_bench_actions_.push_back(index);
        } else {
            ordinary_bench.push_back(index);
        }
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
            for (const std::uint32_t blocker_index : ordinary_bench) {
                const ActionDescriptor& blocker =
                    registry_.actions[blocker_index];
                const std::uint32_t blocker_mod = blocker.params.mod_id;
                std::vector<std::uint64_t> conflict_mask(
                    session_->words, 0);
                bool conflicts_positive = false;
                for (std::uint32_t mod = 0; mod < session_->mod_count; ++mod) {
                    if (!mods_conflict(*session_, blocker_mod, mod)) continue;
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
                               candidate.conflict_mask == conflict_mask;
                    });
                if (existing == temporary_bench_effect_classes_.end()) {
                    TemporaryBenchEffectClass effect;
                    effect.followup_action = followup;
                    effect.goal_slot = slot;
                    effect.blocker_side = blocker_side;
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

StateLocalAutomaticBatch CalcContext::admit_state_local_automatic_candidates(
    const std::uint32_t state_id,
    const AutomaticAdmissionLimits& limits) {
    StateLocalAutomaticBatch batch;
    const auto cached = state_local_automatic_operators_.find(state_id);
    if (cached != state_local_automatic_operators_.end()) {
        batch.cached = true;
        batch.admitted_operators = cached->second;
        return batch;
    }
    if (!goal_.automatic_candidates || is_goal_state(state(state_id))) {
        const auto [stored, inserted] =
            state_local_automatic_operators_.emplace(
            state_id, std::vector<std::uint32_t>{});
        if (inserted) account_state_local_operators(stored->second);
        return batch;
    }

    pc_item_state carrier;
    if (!materialize(state_id, carrier)) {
        const auto [stored, inserted] =
            state_local_automatic_operators_.emplace(
            state_id, std::vector<std::uint32_t>{});
        if (inserted) account_state_local_operators(stored->second);
        return batch;
    }

    const auto shared_started = std::chrono::steady_clock::now();
    const auto synthesis_started = std::chrono::steady_clock::now();
    AutomaticOptionSynthesis synthesis =
        synthesize_automatic_options(
            *this, state_id, carrier, limits.prices);
    /*
     * Eldritch side intents operate on the parent carrier's exact preserved
     * side and can add parent-layout delta states. Do not reproject them
     * through the temporary admission context: an option-specific finer junk
     * partition can choose a different representative and fail to
     * rematerialize even though the parent raw actions are exact. Evaluate
     * these four one-shot compounds directly on the parent state lifecycle.
     */
    std::vector<FixedOptionSpec> parent_eldritch_specs;
    for (auto it = synthesis.specs.begin();
         it != synthesis.specs.end();) {
        if (it->kind == FixedOptionKind::EldritchSideIntent &&
            it->automatic_kind ==
                AutomaticCandidateKind::EldritchSide) {
            parent_eldritch_specs.push_back(std::move(*it));
            it = synthesis.specs.erase(it);
        } else {
            ++it;
        }
    }
    batch.phases.carriers = 1;
    batch.phases.synthesis_ns = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - synthesis_started)
            .count());
    batch.temporary_precompiled_classes =
        synthesis.temporary_precompiled_classes;
    batch.temporary_precompile_ns = temporary_bench_precompile_ns_;
    batch.temporary_precompiled_bytes = temporary_bench_precompiled_bytes_;
    batch.temporary_candidate_variants =
        synthesis.temporary_candidate_variants;
    batch.temporary_effect_classes =
        synthesis.temporary_effect_classes;
    batch.temporary_collapsed_variants =
        synthesis.temporary_collapsed_variants;
    batch.temporary_enumeration_ns =
        synthesis.temporary_enumeration_ns;
    std::vector<std::uint32_t> permanent_benches;
    std::vector<std::uint32_t> local_candidates = candidates_;
    for (std::uint32_t index = 0; index < registry_.actions.size(); ++index) {
        const PlannerOperator& planner = operators_.at(index);
        if (planner.automatic_kind !=
                AutomaticCandidateKind::PermanentBench ||
            std::find(candidates_.begin(), candidates_.end(), index) !=
                candidates_.end() ||
            !action_legal(*session_, registry_.actions[index], state(state_id))) {
            continue;
        }
        permanent_benches.push_back(index);
        if (std::find(
                local_candidates.begin(), local_candidates.end(), index) ==
            local_candidates.end()) {
            local_candidates.push_back(index);
        }
    }

    GoalSpec local_goal = goal_;
    local_goal.automatic_candidates = false;
    local_goal.fixed_options = std::move(synthesis.specs);
    const auto local_context_started = std::chrono::steady_clock::now();
    const std::string context_key = automatic_context_key(
        local_goal.fixed_options, local_candidates);
    constexpr std::size_t kRetainedAutomaticAdmissionContexts = 64;
    bool admission_context_created = false;
    std::unique_ptr<CalcContext> transient_context;
    CalcContext* local_pointer = nullptr;
    const auto retained = automatic_admission_contexts_.find(context_key);
    if (retained != automatic_admission_contexts_.end()) {
        local_pointer = retained->second.get();
        local_pointer->reset_solve_telemetry();
    } else {
        auto created = std::make_unique<CalcContext>(
            session_, local_goal, registry_, local_candidates,
            false, false, true);
        created->set_defer_automatic_protected_baseline(true);
        admission_context_created = true;
        if (automatic_admission_contexts_.size() <
            kRetainedAutomaticAdmissionContexts) {
            local_pointer = created.get();
            automatic_admission_contexts_.emplace(
                context_key, std::move(created));
        } else {
            transient_context = std::move(created);
            local_pointer = transient_context.get();
        }
    }
    CalcContext& local = *local_pointer;
    local.set_defer_automatic_protected_baseline(true);
    batch.phases.local_context_ns = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - local_context_started)
            .count());
    batch.phases.local_planner_build_ns =
        admission_context_created ? local.planner_build_ns() : 0;
    batch.phases.local_layout_build_ns =
        admission_context_created ? local.layout_build_ns() : 0;
    batch.phases.local_ledger_init_ns =
        admission_context_created ? local.owned_byte_ledger_init_ns() : 0;
    const std::uint64_t local_attributed_ns =
        batch.phases.local_planner_build_ns +
        batch.phases.local_layout_build_ns +
        batch.phases.local_ledger_init_ns;
    batch.phases.local_context_other_ns =
        batch.phases.local_context_ns > local_attributed_ns
            ? batch.phases.local_context_ns - local_attributed_ns
            : 0;
    local.set_solve_resource_caps(
        limits.max_discovered_states == 0
            ? std::numeric_limits<std::uint32_t>::max()
            : limits.max_discovered_states,
        limits.max_reforge_work == 0
            ? std::numeric_limits<std::uint64_t>::max()
            : limits.max_reforge_work,
        false,
        limits.max_solver_owned_bytes == 0
            ? std::nullopt
            : std::optional<std::uint64_t>{
                  limits.max_solver_owned_bytes});
    const std::uint32_t local_state = local.intern_item(carrier);
    const std::uint32_t base_operator_count =
        static_cast<std::uint32_t>(local.operators().size());
    std::vector<std::uint32_t> local_option_indices;
    local_option_indices.reserve(
        base_operator_count - registry_.actions.size() + 8);
    for (std::uint32_t index =
             static_cast<std::uint32_t>(registry_.actions.size());
         index < base_operator_count; ++index) {
        local_option_indices.push_back(index);
    }
    std::array<std::uint64_t, kAutomaticTelemetryKindCount> shared_weights{};
    ++shared_weights[static_cast<std::size_t>(AutomaticTelemetryKind::Imprint)];
    for (const std::uint32_t index : permanent_benches) {
        (void)index;
        ++shared_weights[static_cast<std::size_t>(
            AutomaticTelemetryKind::PermanentBench)];
    }
    for (const std::uint32_t index : local_option_indices) {
        const AutomaticTelemetryKind kind =
            telemetry_kind_for_candidate(
                local.operators().at(index).automatic_kind);
        if (kind != AutomaticTelemetryKind::None) {
            ++shared_weights[static_cast<std::size_t>(kind)];
        }
    }
    const std::uint64_t shared_ns = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - shared_started)
            .count());
    const std::uint64_t total_shared_weight = std::accumulate(
        shared_weights.begin(), shared_weights.end(), std::uint64_t{0});
    if (total_shared_weight != 0) {
        for (std::size_t i = 0; i < shared_weights.size(); ++i) {
            batch.shared_admission_ns[i] =
                shared_ns * shared_weights[i] / total_shared_weight;
        }
    }
    std::unordered_map<std::uint32_t, std::uint32_t> mapped_states;
    mapped_states.emplace(local_state, state_id);
    std::vector<const OptionKernel*> seen_option_kernels;
    std::vector<std::pair<
        const OutcomeDistribution*,
        std::vector<std::pair<std::string, double>>>>
        seen_primitive_kernels;

    const auto check_limits = [&](const bool force_bytes = false) {
        const CalcTelemetry& work = local.telemetry();
        if (limits.max_state_action_rows != 0 &&
            work.state_action_rows > limits.max_state_action_rows) {
            throw SolverResourceLimit(
                "max_state_action_rows", limits.max_state_action_rows);
        }
        if (limits.max_transitions != 0 &&
            work.transition_entries > limits.max_transitions) {
            throw SolverResourceLimit(
                "max_transitions", limits.max_transitions);
        }
        if (limits.max_solver_owned_bytes == 0) return;
        if (!force_bytes) return;
        const std::uint64_t owned_bytes = fast_estimated_owned_bytes() +
            (transient_context != nullptr
                 ? local.fast_estimated_owned_bytes()
                 : 0);
        if (owned_bytes >
            limits.max_solver_owned_bytes) {
            throw SolverResourceLimit(
                "max_solver_owned_bytes", limits.max_solver_owned_bytes);
        }
    };

    const auto add_dependency = [&](const std::uint32_t action) {
        if (std::find(candidates_.begin(), candidates_.end(), action) ==
                candidates_.end() &&
            admitted_automatic_dependencies_.insert(action).second) {
            ++action_control_.automatic_dependency_primitives;
        }
    };
    const auto admit_operator = [&](const std::uint32_t index) {
        if (std::find(
                candidate_operators_.begin(), candidate_operators_.end(),
                index) == candidate_operators_.end()) {
            candidate_operators_.push_back(index);
            ++action_control_.automatic_options;
        }
        batch.admitted_operators.push_back(index);
    };
    const auto has_prices = [&](const PlannerOperator& planner) {
        if (limits.prices == nullptr) return true;
        return std::all_of(
            planner.resource_quantities.begin(),
            planner.resource_quantities.end(),
            [&](const auto& resource) {
                return limits.prices->contains(resource.first);
            });
    };
    const auto action_has_prices = [&](const std::uint32_t action) {
        if (limits.prices == nullptr) return true;
        return std::all_of(
            registry_.actions.at(action).cost_keys.begin(),
            registry_.actions.at(action).cost_keys.end(),
            [&](const std::string& key) {
                return limits.prices->contains(key);
            });
    };
    const auto program_has_prices = [&](const PlannerOperator& planner) {
        return std::all_of(
                   planner.primitive_program.begin(),
                   planner.primitive_program.end(), action_has_prices) &&
               (planner.conditional_action == kNoId ||
                action_has_prices(planner.conditional_action));
    };
    if (!parent_eldritch_specs.empty()) {
        GoalSpec parent_goal = goal_;
        parent_goal.automatic_candidates = false;
        parent_goal.fixed_options =
            std::move(parent_eldritch_specs);
        std::vector<PlannerOperator> parent_options =
            build_planner_operators(
                *session_, parent_goal, registry_, candidates_);
        for (std::uint32_t local_index =
                 static_cast<std::uint32_t>(registry_.actions.size());
             local_index < parent_options.size(); ++local_index) {
            PlannerOperator& proposed =
                parent_options[local_index];
            if (proposed.option_kind !=
                    FixedOptionKind::EldritchSideIntent ||
                proposed.automatic_kind !=
                    AutomaticCandidateKind::EldritchSide) {
                continue;
            }
            StateLocalAutomaticCandidate decision;
            decision.id = proposed.id;
            decision.kind =
                AutomaticCandidateKind::EldritchSide;
            decision.telemetry_kind =
                AutomaticTelemetryKind::EldritchSide;
            const auto existing = std::find_if(
                operators_.begin(), operators_.end(),
                [&](const PlannerOperator& candidate) {
                    return candidate.id == proposed.id &&
                           candidate.kind == proposed.kind &&
                           candidate.option_kind ==
                               proposed.option_kind &&
                           candidate.intended_side ==
                               proposed.intended_side &&
                           candidate.primitive_program ==
                               proposed.primitive_program;
                });
            std::uint32_t operator_index = kNoId;
            if (existing == operators_.end()) {
                operator_index = static_cast<std::uint32_t>(
                    operators_.size());
                operators_.push_back(std::move(proposed));
                account_new_operator(operators_.back());
                decision.selected_bytes =
                    sizeof(PlannerOperator);
            } else {
                operator_index = static_cast<std::uint32_t>(
                    std::distance(operators_.begin(), existing));
            }
            decision.operator_index = operator_index;
            const PlannerOperator& planner =
                operators_.at(operator_index);
            const auto kernel_started =
                std::chrono::steady_clock::now();
            const OptionKernel& kernel =
                option_kernel(state_id, operator_index);
            decision.kernel_evaluation_ns =
                static_cast<std::uint64_t>(
                    std::chrono::duration_cast<
                        std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now() -
                        kernel_started)
                        .count());
            decision.raw_outcomes = outcome_count(kernel);
            decision.evidence = kernel.automatic;
            decision.selected_bytes +=
                option_kernel_selected_bytes(kernel);
            if (!program_has_prices(planner)) {
                decision.missing_price = true;
                decision.evidence.eligible = false;
                decision.evidence.legality_result =
                    "not_admitted_missing_price";
                decision.evidence.reason =
                    "automatic_candidate_missing_price";
            } else if (decision.evidence.eligible) {
                decision.admitted = true;
                admit_operator(operator_index);
                state_local_automatic_operator_indices_.insert(
                    operator_index);
                for (const std::uint32_t dependency :
                     planner.primitive_program) {
                    add_dependency(dependency);
                }
            }
            batch.decisions.push_back(std::move(decision));
            if (limits.max_state_action_rows != 0 &&
                telemetry_.state_action_rows >
                    limits.max_state_action_rows) {
                throw SolverResourceLimit(
                    "max_state_action_rows",
                    limits.max_state_action_rows);
            }
            if (limits.max_transitions != 0 &&
                telemetry_.transition_entries >
                    limits.max_transitions) {
                throw SolverResourceLimit(
                    "max_transitions", limits.max_transitions);
            }
            check_limits(true);
        }
    }
    bool local_work_merged = false;
    const auto merge_local_work = [&]() {
        if (local_work_merged) return;
        local_work_merged = true;
        const CalcTelemetry& work = local.telemetry();
        const auto add_bounded = [&](std::uint64_t& target,
                                     const std::uint64_t amount,
                                     const std::uint64_t limit,
                                     const char* cap) {
            if (limit != 0 && amount > limit - std::min(target, limit)) {
                target = limit;
                throw SolverResourceLimit(cap, limit);
            }
            target += amount;
        };
        telemetry_.distribution_requests += work.distribution_requests;
        telemetry_.distribution_hits += work.distribution_hits;
        telemetry_.distribution_misses += work.distribution_misses;
        telemetry_.distribution_build_ns += work.distribution_build_ns;
        add_bounded(
            telemetry_.state_action_rows, work.state_action_rows,
            limits.max_state_action_rows, "max_state_action_rows");
        add_bounded(
            telemetry_.transition_entries, work.transition_entries,
            limits.max_transitions, "max_transitions");
        telemetry_.outcome_entries += work.outcome_entries;
        telemetry_.choice_groups += work.choice_groups;
        telemetry_.choice_successor_entries +=
            work.choice_successor_entries;
        telemetry_.reforge_requests += work.reforge_requests;
        telemetry_.reforge_hits += work.reforge_hits;
        telemetry_.reforge_misses += work.reforge_misses;
        telemetry_.reforge_build_ns += work.reforge_build_ns;
        telemetry_.protected_retry_checks +=
            work.protected_retry_checks;
        telemetry_.protected_retry_certificates +=
            work.protected_retry_certificates;
        telemetry_.protected_retry_fallbacks +=
            work.protected_retry_fallbacks;
        telemetry_.protected_attempt_ns += work.protected_attempt_ns;
        telemetry_.protected_baseline_ns += work.protected_baseline_ns;
        telemetry_.protected_normalization_ns +=
            work.protected_normalization_ns;
        telemetry_.protected_finish_ns += work.protected_finish_ns;
        telemetry_.owned_byte_audit_requests +=
            work.owned_byte_audit_requests;
        telemetry_.owned_byte_audit_ns += work.owned_byte_audit_ns;
        telemetry_.owned_byte_ledger_requests +=
            work.owned_byte_ledger_requests;
        telemetry_.owned_byte_ledger_ns +=
            work.owned_byte_ledger_ns;
        telemetry_.owned_byte_reconciliations +=
            work.owned_byte_reconciliations;
        telemetry_.owned_byte_ledger_max_overestimate = std::max(
            telemetry_.owned_byte_ledger_max_overestimate,
            work.owned_byte_ledger_max_overestimate);
        for (std::size_t i = 0; i < kPrimitiveTelemetryFamilyCount; ++i) {
            PrimitiveFamilyTelemetry& target =
                telemetry_.primitive_families[i];
            const PrimitiveFamilyTelemetry& source =
                work.primitive_families[i];
            target.requests += source.requests;
            target.cache_hits += source.cache_hits;
            target.rows += source.rows;
            target.raw_outcomes += source.raw_outcomes;
            target.transitions += source.transitions;
            target.build_ns += source.build_ns;
            target.row_ns += source.row_ns;
            target.selected_bytes += source.selected_bytes;
        }
        consume_reforge_work(work.reforge_frontier_work);
    };

    try {
        const auto imprint_started = std::chrono::steady_clock::now();
        const ImprintDiscoveryResult imprint =
            discover_automatic_imprint_options(
                local, local_state, limits, check_limits);
        const std::uint64_t imprint_discovery_ns =
            static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() - imprint_started)
                    .count());
        bool imprint_time_attributed = false;
        if (!imprint.specs.empty()) {
            GoalSpec imprint_goal = local_goal;
            imprint_goal.fixed_options = imprint.specs;
            std::vector<PlannerOperator> imprint_operators =
                build_planner_operators(
                    *session_, imprint_goal, registry_, local_candidates);
            for (std::uint32_t index =
                     static_cast<std::uint32_t>(registry_.actions.size());
                 index < imprint_operators.size(); ++index) {
                local.operators_.push_back(
                    std::move(imprint_operators[index]));
                local.account_new_operator(local.operators_.back());
                local_option_indices.push_back(
                    static_cast<std::uint32_t>(
                        local.operators_.size() - 1));
            }
            check_limits();
        }
        if (imprint.missing_price) {
            StateLocalAutomaticCandidate missing;
            missing.id = "automatic:imprint_discovery";
            missing.kind = AutomaticCandidateKind::Imprint;
            missing.telemetry_kind = AutomaticTelemetryKind::Imprint;
            missing.admission_ns = imprint_discovery_ns;
            imprint_time_attributed = true;
            missing.missing_price = true;
            missing.evidence.candidate = true;
            missing.evidence.legality_result =
                "not_evaluated_missing_price";
            missing.evidence.reason =
                "automatic_imprint_checkpoint_price_missing";
            batch.decisions.push_back(std::move(missing));
        }
        const auto add_imprint_boundary = [&](const char* cap,
                                              const std::uint64_t limit) {
            StateLocalAutomaticCandidate deferred;
            deferred.id = "automatic:imprint_program_discovery";
            deferred.kind = AutomaticCandidateKind::Imprint;
            deferred.telemetry_kind = AutomaticTelemetryKind::Imprint;
            if (!imprint_time_attributed) {
                deferred.admission_ns = imprint_discovery_ns;
                imprint_time_attributed = true;
            }
            deferred.deferred = true;
            deferred.evidence.candidate = true;
            deferred.evidence.kernel_change_mechanisms =
                kAutomaticImprintCheckpoint;
            deferred.evidence.legality_result = "deferred_resource_cap";
            deferred.evidence.reason =
                std::string("price_independent_kernel_generation_") + cap +
                "_limit_" + std::to_string(limit) + "_work_" +
                std::to_string(imprint.work_used);
            batch.decisions.push_back(std::move(deferred));
        };
        if (imprint.depth_deferred) {
            add_imprint_boundary(
                "max_imprint_program_depth", imprint.depth_limit);
        }
        if (imprint.work_deferred) {
            add_imprint_boundary(
                "max_imprint_program_work", imprint.work_limit);
        }

        for (const std::uint32_t action_index : permanent_benches) {
            const auto candidate_started = std::chrono::steady_clock::now();
            StateLocalAutomaticCandidate decision;
            const PlannerOperator& planner = operators_.at(action_index);
            decision.id = planner.id;
            decision.kind = planner.automatic_kind;
            decision.telemetry_kind =
                telemetry_kind_for_candidate(decision.kind);
            decision.evidence.candidate = true;
            decision.evidence.relevant_goal_mask = planner.relevant_goal_mask;
            if (!has_prices(planner)) {
                decision.missing_price = true;
                decision.evidence.legality_result =
                    "not_evaluated_missing_price";
                decision.evidence.reason =
                    "automatic_candidate_missing_price";
                decision.admission_ns = static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now() - candidate_started)
                        .count());
                batch.decisions.push_back(std::move(decision));
                continue;
            }
            auto exact_distribution =
                std::make_shared<OutcomeDistribution>();
            pc_item_state successor = carrier;
            (void)apply_action(
                context_, &successor,
                registry_.actions.at(action_index).params);
            exact_distribution->supported = true;
            exact_distribution->entries.push_back(
                {intern_item(successor), 1.0});
            const OutcomeDistribution& distribution = *exact_distribution;
            decision.raw_outcomes = outcome_count(distribution);
            bool advances = false;
            for (const OutcomeEntry& exit : distribution.entries) {
                const AbstractState& next = state(exit.state);
                for (std::uint32_t slot = 0;
                     slot < layout_.slots.size(); ++slot) {
                    advances |=
                        (planner.relevant_goal_mask & (1u << slot)) != 0 &&
                        next.slot_status[slot] >
                            state(state_id).slot_status[slot];
                }
            }
            decision.evidence.eligible = distribution.supported && advances;
            decision.evidence.kernel_changed = advances;
            decision.evidence.setup_complete = advances;
            decision.evidence.cleanup_complete = true;
            decision.evidence.recovery_complete = true;
            decision.evidence.exits_complete = !distribution.entries.empty();
            decision.evidence.kernel_change_mechanisms =
                kAutomaticDeterministicFinish;
            decision.evidence.legality_result =
                advances ? "legal" : "irrelevant";
            decision.evidence.reason =
                advances ? "legal_permanent_goal_bench_successor"
                         : "permanent_bench_does_not_advance_goal";
            if (decision.evidence.eligible) {
                const auto resources = planner.resource_quantities;
                const auto duplicate = std::find_if(
                    seen_primitive_kernels.begin(),
                    seen_primitive_kernels.end(),
                    [&](const auto& seen) {
                        return same_complete_distribution(
                                   *seen.first, distribution) &&
                               seen.second == resources;
                    });
                if (duplicate != seen_primitive_kernels.end()) {
                    decision.collapsed = true;
                } else {
                    seen_primitive_kernels.push_back(
                        {&distribution, resources});
                    const std::uint64_t key =
                        (static_cast<std::uint64_t>(state_id) << 32) |
                        action_index;
                    account_distribution_cache_insert(
                        key, exact_distribution);
                    distribution_cache_[key] =
                        std::move(exact_distribution);
                    decision.operator_index = action_index;
                    decision.admitted = true;
                    admit_operator(action_index);
                }
            }
            decision.admission_ns = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() - candidate_started)
                    .count());
            batch.decisions.push_back(std::move(decision));
            check_limits();
        }

        std::unordered_map<
            std::string,
            std::shared_ptr<const OptionKernel>>
            temporary_evaluation_memo;
        struct ProtectedKernelComparison {
            bool supported = false;
            bool fully_legal = false;
            bool changed = false;
            std::uint64_t baseline_hash = 0;
            std::uint64_t candidate_hash = 0;
        };
        std::map<
            std::pair<std::uint32_t, std::uint32_t>,
            ProtectedKernelComparison>
            protected_kernel_comparisons;
        const auto temporary_group_for = [&](const PlannerOperator& planner)
            -> const TemporaryBenchCandidateGroup* {
            if (planner.option_kind !=
                FixedOptionKind::TemporaryBenchRepeat) {
                return nullptr;
            }
            const auto found = std::find_if(
                synthesis.temporary_groups.begin(),
                synthesis.temporary_groups.end(),
                [&](const TemporaryBenchCandidateGroup& group) {
                    return group.representative_blocker ==
                               planner.setup_action &&
                           group.followup_action ==
                               planner.followup_action &&
                           group.goal_slot < kMaxGoalSlots &&
                           planner.exit_goal_slots.size() == 1 &&
                           planner.exit_goal_slots.front() == group.goal_slot;
                });
            return found == synthesis.temporary_groups.end()
                       ? nullptr
                       : &*found;
        };
        for (const std::uint32_t local_operator : local_option_indices) {
            const auto candidate_started = std::chrono::steady_clock::now();
            const PlannerOperator& local_planner =
                local.operators().at(local_operator);
            const TemporaryBenchCandidateGroup* temporary_group =
                temporary_group_for(local_planner);
            StateLocalAutomaticCandidate base_decision;
            base_decision.id = local_planner.id;
            base_decision.kind = local_planner.automatic_kind;
            base_decision.telemetry_kind =
                telemetry_kind_for_candidate(base_decision.kind);
            const bool measure_protected =
                base_decision.telemetry_kind ==
                AutomaticTelemetryKind::ProtectedSide;
            const bool direct_fracture =
                local_planner.option_kind ==
                    FixedOptionKind::FracturePrepare &&
                local_planner.carrier_goal_slot < local.layout().slots.size() &&
                local.state(local_state).slot_status[
                    local_planner.carrier_goal_slot] ==
                    static_cast<std::uint8_t>(GoalSlotStatus::Satisfied) &&
                local_planner.conditional_action != kNoId &&
                action_legal(
                    local.session(),
                    local.registry().actions.at(
                        local_planner.conditional_action),
                    local.state(local_state));
            if (temporary_group == nullptr &&
                (!has_prices(local_planner) ||
                 (measure_protected &&
                  !program_has_prices(local_planner))) &&
                !direct_fracture) {
                base_decision.missing_price = true;
                base_decision.evidence.candidate = true;
                base_decision.evidence.relevant_goal_mask =
                    local_planner.relevant_goal_mask;
                base_decision.evidence.legality_result =
                    "not_evaluated_missing_price";
                base_decision.evidence.reason =
                    "automatic_candidate_missing_price";
                base_decision.admission_ns = static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now() - candidate_started)
                        .count());
                batch.decisions.push_back(std::move(base_decision));
                continue;
            }
            if (temporary_group != nullptr && limits.prices != nullptr) {
                const bool common_prices =
                    action_has_prices(local_planner.followup_action) &&
                    action_has_prices(local_planner.cleanup_action);
                const bool any_priced_variant = common_prices && std::any_of(
                    temporary_group->blocker_variants.begin(),
                    temporary_group->blocker_variants.end(),
                    action_has_prices);
                if (!any_priced_variant) {
                    bool first = true;
                    for (const std::uint32_t blocker :
                         temporary_group->blocker_variants) {
                        StateLocalAutomaticCandidate missing = base_decision;
                        missing.id =
                            "option:temporary_bench_repeat:" +
                            local.registry().actions.at(blocker).id + ':' +
                            local.registry().actions.at(
                                local_planner.followup_action).id +
                            ":until:" +
                            std::to_string(
                                local_planner.exit_min_satisfied) + ':' +
                            std::to_string(
                                local_planner.exit_goal_slots.front());
                        missing.missing_price = true;
                        missing.evidence.candidate = true;
                        missing.evidence.relevant_goal_mask =
                            local_planner.relevant_goal_mask;
                        missing.evidence.legality_result =
                            "not_evaluated_missing_price";
                        missing.evidence.reason =
                            "automatic_candidate_missing_price";
                        if (first) {
                            missing.admission_ns = static_cast<std::uint64_t>(
                                std::chrono::duration_cast<
                                    std::chrono::nanoseconds>(
                                    std::chrono::steady_clock::now() -
                                    candidate_started)
                                    .count());
                            first = false;
                        }
                        batch.decisions.push_back(std::move(missing));
                    }
                    continue;
                }
            }
            const auto kernel_evaluation_started =
                measure_protected
                    ? std::chrono::steady_clock::now()
                    : std::chrono::steady_clock::time_point{};
            const CalcTelemetry protected_before =
                measure_protected ? local.telemetry() : CalcTelemetry{};
            const std::string evaluation_key = temporary_evaluation_key(
                local.session(), local.registry(), local_planner);
            const OptionKernel* local_kernel_ptr = nullptr;
            const auto reused = evaluation_key.empty()
                                    ? temporary_evaluation_memo.end()
                                    : temporary_evaluation_memo.find(
                                          evaluation_key);
            if (reused != temporary_evaluation_memo.end()) {
                auto kernel = std::make_shared<OptionKernel>(
                    *reused->second);
                kernel->expected_resources =
                    local_planner.resource_quantities;
                kernel->retained_template_id = 0;
                kernel->retained_template_storage = false;
                const std::uint64_t local_key =
                    (static_cast<std::uint64_t>(local_state) << 32) |
                    local_operator;
                local_kernel_ptr = kernel.get();
                local.account_option_cache_insert(local_key, kernel);
                local.option_kernel_cache_[local_key] = std::move(kernel);
            } else {
                local_kernel_ptr = &local.option_kernel(
                    local_state, local_operator);
                if (!evaluation_key.empty()) {
                    const std::uint64_t local_key =
                        (static_cast<std::uint64_t>(local_state) << 32) |
                        local_operator;
                    temporary_evaluation_memo.emplace(
                        evaluation_key,
                        local.option_kernel_cache_.at(local_key));
                }
            }
            const OptionKernel& local_kernel = *local_kernel_ptr;
            const ProtectedKernelComparison* protected_comparison = nullptr;
            if (local_planner.option_kind ==
                    FixedOptionKind::ProtectedRepeat &&
                local_kernel.automatic.eligible) {
                const std::pair<std::uint32_t, std::uint32_t> comparison_key{
                    local_planner.setup_action,
                    local_planner.followup_action};
                auto found = protected_kernel_comparisons.find(
                    comparison_key);
                if (found == protected_kernel_comparisons.end()) {
                    if (automatic_comparison_context_ == nullptr) {
                        GoalSpec comparison_goal = goal_;
                        comparison_goal.automatic_candidates = false;
                        comparison_goal.fixed_options.clear();
                        automatic_comparison_context_ =
                            std::make_unique<CalcContext>(
                                session_, comparison_goal, registry_,
                                candidates_, false, false, true,
                                solve_discovered_state_cap_);
                        automatic_comparison_context_->set_solve_resource_caps(
                            solve_discovered_state_cap_.value_or(
                                std::numeric_limits<std::uint32_t>::max()),
                            std::numeric_limits<std::uint64_t>::max(), false,
                            solve_owned_bytes_cap_);
                    }
                    CalcContext& comparison_context =
                        *automatic_comparison_context_;
                    const CalcTelemetry comparison_before =
                        comparison_context.telemetry();
                    const std::uint32_t comparison_state =
                        comparison_context.intern_item(carrier);
                    const ActionDescriptor& baseline_action =
                        comparison_context.registry().actions.at(
                            local_planner.followup_action);
                    const bool baseline_legal = action_legal(
                        comparison_context.session(), baseline_action,
                        comparison_context.state(comparison_state));
                    const OutcomeDistribution* baseline_distribution =
                        baseline_legal
                            ? &comparison_context.outcomes(
                                  comparison_state,
                                  local_planner.followup_action)
                            : nullptr;
                    const CalcTelemetry& comparison_after =
                        comparison_context.telemetry();
                    telemetry_.distribution_requests +=
                        comparison_after.distribution_requests -
                        comparison_before.distribution_requests;
                    telemetry_.distribution_hits +=
                        comparison_after.distribution_hits -
                        comparison_before.distribution_hits;
                    telemetry_.distribution_misses +=
                        comparison_after.distribution_misses -
                        comparison_before.distribution_misses;
                    telemetry_.distribution_build_ns +=
                        comparison_after.distribution_build_ns -
                        comparison_before.distribution_build_ns;
                    telemetry_.outcome_entries +=
                        comparison_after.outcome_entries -
                        comparison_before.outcome_entries;
                    telemetry_.choice_groups +=
                        comparison_after.choice_groups -
                        comparison_before.choice_groups;
                    telemetry_.choice_successor_entries +=
                        comparison_after.choice_successor_entries -
                        comparison_before.choice_successor_entries;
                    telemetry_.reforge_requests +=
                        comparison_after.reforge_requests -
                        comparison_before.reforge_requests;
                    telemetry_.reforge_hits +=
                        comparison_after.reforge_hits -
                        comparison_before.reforge_hits;
                    telemetry_.reforge_misses +=
                        comparison_after.reforge_misses -
                        comparison_before.reforge_misses;
                    telemetry_.reforge_build_ns +=
                        comparison_after.reforge_build_ns -
                        comparison_before.reforge_build_ns;
                    consume_reforge_work(
                        comparison_after.reforge_frontier_work -
                        comparison_before.reforge_frontier_work);
                    ProtectedKernelComparison comparison;
                    comparison.supported =
                        baseline_distribution != nullptr &&
                        baseline_distribution->supported &&
                        baseline_distribution->choice_groups.empty();
                    comparison.fully_legal = baseline_legal;
                    bool same_outcomes = comparison.supported &&
                        baseline_distribution->entries.size() ==
                            local_kernel
                                .automatic_candidate_attempt_entries.size();
                    if (same_outcomes) {
                        std::unordered_multimap<
                            std::size_t,
                            std::pair<const AbstractState*, double>>
                            candidate_outcomes;
                        candidate_outcomes.reserve(
                            local_kernel
                                .automatic_candidate_attempt_entries.size());
                        for (const OutcomeEntry& entry :
                             local_kernel
                                 .automatic_candidate_attempt_entries) {
                            const AbstractState& candidate_state =
                                local.state(entry.state);
                            candidate_outcomes.emplace(
                                abstract_state_hash(candidate_state),
                                std::pair{
                                    &candidate_state, entry.probability});
                        }
                        for (const OutcomeEntry& entry :
                             baseline_distribution->entries) {
                            const AbstractState& baseline_state =
                                comparison_context.state(entry.state);
                            const auto [first, last] =
                                candidate_outcomes.equal_range(
                                    abstract_state_hash(baseline_state));
                            const bool matched = std::any_of(
                                first, last, [&](const auto& candidate) {
                                    return *candidate.second.first ==
                                               baseline_state &&
                                           candidate.second.second ==
                                               entry.probability;
                                });
                            if (!matched) {
                                same_outcomes = false;
                                break;
                            }
                        }
                    }
                    comparison.changed =
                        comparison.supported && comparison.fully_legal &&
                        !same_outcomes;
                    if (baseline_distribution != nullptr) {
                        AttemptKernel baseline;
                        baseline.supported =
                            baseline_distribution->supported;
                        baseline.fully_legal = baseline_legal;
                        baseline.entries = baseline_distribution->entries;
                        comparison.baseline_hash =
                            attempt_kernel_hash(baseline);
                        comparison_context.release_outcome(
                            comparison_state,
                            local_planner.followup_action);
                    }
                    AttemptKernel candidate;
                    candidate.entries =
                        local_kernel.automatic_candidate_attempt_entries;
                    comparison.candidate_hash =
                        attempt_kernel_hash(candidate);
                    found = protected_kernel_comparisons.emplace(
                        comparison_key, comparison).first;
                }
                protected_comparison = &found->second;
            }
            if (measure_protected) {
                const CalcTelemetry& protected_after = local.telemetry();
                base_decision.kernel_evaluation_ns =
                    static_cast<std::uint64_t>(
                        std::chrono::duration_cast<std::chrono::nanoseconds>(
                            std::chrono::steady_clock::now() -
                            kernel_evaluation_started)
                            .count());
                base_decision.protected_side_evaluations =
                    local_planner.option_kind ==
                            FixedOptionKind::ProtectedSide
                        ? 1
                        : 0;
                base_decision.protected_repeat_evaluations =
                    local_planner.option_kind ==
                            FixedOptionKind::ProtectedRepeat
                        ? 1
                        : 0;
                base_decision.protected_retry_checks =
                    protected_after.protected_retry_checks -
                    protected_before.protected_retry_checks;
                base_decision.protected_retry_certificates =
                    protected_after.protected_retry_certificates -
                    protected_before.protected_retry_certificates;
                base_decision.protected_retry_fallbacks =
                    protected_after.protected_retry_fallbacks -
                    protected_before.protected_retry_fallbacks;
                base_decision.protected_attempt_ns =
                    protected_after.protected_attempt_ns -
                    protected_before.protected_attempt_ns;
                base_decision.protected_baseline_ns =
                    protected_after.protected_baseline_ns -
                    protected_before.protected_baseline_ns;
                base_decision.protected_normalization_ns =
                    protected_after.protected_normalization_ns -
                    protected_before.protected_normalization_ns;
                base_decision.protected_finish_ns =
                    protected_after.protected_finish_ns -
                    protected_before.protected_finish_ns;
            }
            base_decision.raw_outcomes = outcome_count(local_kernel);
            if (base_decision.kind == AutomaticCandidateKind::Imprint &&
                !imprint_time_attributed) {
                base_decision.admission_ns += imprint_discovery_ns;
                imprint_time_attributed = true;
            }
            base_decision.evidence = local_kernel.automatic;
            if (protected_comparison != nullptr) {
                base_decision.evidence.baseline_kernel_hash =
                    protected_comparison->baseline_hash;
                base_decision.evidence.candidate_kernel_hash =
                    protected_comparison->candidate_hash;
                base_decision.evidence.kernel_changed =
                    protected_comparison->changed;
                if (!protected_comparison->supported ||
                    !protected_comparison->fully_legal ||
                    !protected_comparison->changed) {
                    base_decision.evidence.eligible = false;
                    base_decision.evidence.legality_result = "illegal";
                    base_decision.evidence.reason =
                        !protected_comparison->supported
                            ? "exact_kernel_unsupported"
                            : !protected_comparison->fully_legal
                                  ? "one_or_more_program_steps_illegal"
                                  : "exact_successor_kernel_neutral";
                }
            }
            if (temporary_group == nullptr && limits.prices != nullptr &&
                std::any_of(
                    local_kernel.expected_resources.begin(),
                    local_kernel.expected_resources.end(),
                    [&](const auto& resource) {
                        return !limits.prices->contains(resource.first);
                    })) {
                base_decision.missing_price = true;
                base_decision.evidence.legality_result =
                    "not_admitted_missing_price";
                base_decision.evidence.reason =
                    "automatic_candidate_missing_price";
                base_decision.admission_ns += static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now() - candidate_started)
                        .count());
                batch.decisions.push_back(std::move(base_decision));
                continue;
            }
            bool collapse_non_temporary = false;
            if (base_decision.evidence.eligible &&
                temporary_group == nullptr) {
                const auto duplicate = std::find_if(
                    seen_option_kernels.begin(), seen_option_kernels.end(),
                    [&](const OptionKernel* seen) {
                        return same_complete_option_kernel(
                            *seen, local_kernel);
                    });
                if (duplicate != seen_option_kernels.end()) {
                    collapse_non_temporary = true;
                } else {
                    seen_option_kernels.push_back(&local_kernel);
                }
            }
            if (!base_decision.evidence.eligible || collapse_non_temporary) {
                base_decision.collapsed = collapse_non_temporary;
                base_decision.admission_ns += static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now() - candidate_started)
                        .count());
                batch.decisions.push_back(std::move(base_decision));
                check_limits();
                continue;
            }

            std::vector<PlannerOperator> admitted_variants;
            if (temporary_group == nullptr) {
                PlannerOperator admitted = local_planner;
                admitted.resource_quantities = local_kernel.expected_resources;
                admitted_variants.push_back(std::move(admitted));
            } else {
                admitted_variants.reserve(
                    temporary_group->blocker_variants.size());
                for (const std::uint32_t blocker :
                     temporary_group->blocker_variants) {
                    admitted_variants.push_back(temporary_variant_planner(
                        local.registry(), local_planner, local_kernel,
                        blocker));
                }
            }

            std::vector<PlannerOperator> priced_variants;
            priced_variants.reserve(admitted_variants.size());
            const std::size_t first_variant_decision =
                batch.decisions.size();
            for (PlannerOperator& admitted : admitted_variants) {
                if (!has_prices(admitted)) {
                    StateLocalAutomaticCandidate missing = base_decision;
                    missing.id = admitted.id;
                    missing.raw_outcomes = 0;
                    missing.missing_price = true;
                    missing.evidence.legality_result =
                        "not_admitted_missing_price";
                    missing.evidence.reason =
                        "automatic_candidate_missing_price";
                    batch.decisions.push_back(std::move(missing));
                    continue;
                }
                double exact_immediate_cost = 0.0;
                if (limits.prices != nullptr) {
                    for (const auto& [key, quantity] :
                         admitted.resource_quantities) {
                        exact_immediate_cost +=
                            limits.prices->at(key) * quantity;
                    }
                }
                if (std::isfinite(limits.incumbent_upper_bound) &&
                    exact_immediate_cost >
                        limits.incumbent_upper_bound + 1e-12) {
                    StateLocalAutomaticCandidate dominated = base_decision;
                    dominated.id = admitted.id;
                    dominated.raw_outcomes = 0;
                    dominated.evidence.eligible = false;
                    dominated.evidence.legality_result =
                        "dominated_by_incumbent";
                    dominated.evidence.reason =
                        "exact_expected_cost_exceeds_feasible_state_upper";
                    batch.decisions.push_back(std::move(dominated));
                    continue;
                }
                priced_variants.push_back(std::move(admitted));
            }
            if (priced_variants.empty()) {
                const std::uint64_t elapsed = static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now() - candidate_started)
                        .count());
                if (batch.decisions.size() > first_variant_decision) {
                    batch.decisions[first_variant_decision].admission_ns +=
                        elapsed;
                } else {
                    base_decision.admission_ns +=
                        elapsed;
                    batch.decisions.push_back(std::move(base_decision));
                }
                check_limits();
                continue;
            }

            const auto outcome_mapping_started =
                measure_protected
                    ? std::chrono::steady_clock::now()
                    : std::chrono::steady_clock::time_point{};
            auto mapped = std::make_shared<OptionKernel>(
                map_local_option_kernel(
                    local, *this, local_kernel, mapped_states));
            if (protected_comparison != nullptr) {
                mapped->automatic.baseline_kernel_hash =
                    base_decision.evidence.baseline_kernel_hash;
                mapped->automatic.kernel_changed =
                    base_decision.evidence.kernel_changed;
                mapped->automatic.eligible =
                    base_decision.evidence.eligible;
                mapped->automatic.legality_result =
                    base_decision.evidence.legality_result;
                mapped->automatic.reason =
                    base_decision.evidence.reason;
            }
            if (measure_protected) {
                base_decision.outcome_mapping_ns =
                    static_cast<std::uint64_t>(
                        std::chrono::duration_cast<std::chrono::nanoseconds>(
                            std::chrono::steady_clock::now() -
                            outcome_mapping_started)
                            .count());
            }
            const auto template_matching_started =
                measure_protected
                    ? std::chrono::steady_clock::now()
                    : std::chrono::steady_clock::time_point{};
            const std::uint64_t transition_template_id =
                option_transition_hash(*mapped);
            mapped->retained_template_id = transition_template_id;
            std::shared_ptr<const OptionKernel> retained_kernel;
            bool transition_template_hit = false;
            bool new_transition_template = false;
            const auto transition_bucket =
                option_transition_templates_.find(transition_template_id);
            if (transition_bucket != option_transition_templates_.end()) {
                for (const auto& candidate : transition_bucket->second) {
                    if (!same_option_transition_kernel(*candidate, *mapped)) {
                        continue;
                    }
                    retained_kernel = candidate;
                    transition_template_hit = true;
                    break;
                }
            }
            if (retained_kernel == nullptr) {
                retained_kernel = mapped;
                auto& transition_templates =
                    option_transition_templates_[transition_template_id];
                const std::size_t old_capacity =
                    transition_templates.capacity();
                transition_templates.push_back(retained_kernel);
                account_transition_template_insert(
                    old_capacity, retained_kernel);
                new_transition_template = true;
            }

            bool first_variant = true;
            for (PlannerOperator& admitted : priced_variants) {
                StateLocalAutomaticCandidate decision = base_decision;
                decision.id = admitted.id;
                decision.raw_outcomes = first_variant
                                            ? base_decision.raw_outcomes
                                            : 0;
                decision.template_id = transition_template_id;
                decision.template_hit = transition_template_hit ||
                                        !first_variant;
                std::uint32_t operator_index = kNoId;
                bool new_operator = false;
                const std::uint64_t planner_id =
                    option_planner_hash(admitted);
                const auto planner_bucket =
                    option_operator_templates_.find(planner_id);
                if (planner_bucket != option_operator_templates_.end()) {
                    for (const std::uint32_t candidate :
                         planner_bucket->second) {
                        if (candidate < operators_.size() &&
                            same_option_template_planner(
                                operators_.at(candidate), admitted)) {
                            operator_index = candidate;
                            break;
                        }
                    }
                }
                if (operator_index == kNoId) {
                    operator_index = static_cast<std::uint32_t>(
                        operators_.size());
                    operators_.push_back(admitted);
                    account_new_operator(operators_.back());
                    auto& operator_templates =
                        option_operator_templates_[planner_id];
                    const std::size_t old_capacity =
                        operator_templates.capacity();
                    operator_templates.push_back(operator_index);
                    account_operator_template_insert(
                        old_capacity, operator_templates);
                    new_operator = true;
                }
                if (new_operator) {
                    decision.selected_bytes = sizeof(PlannerOperator);
                }
                if (new_transition_template && first_variant) {
                    decision.selected_bytes +=
                        option_kernel_selected_bytes(*retained_kernel);
                }
                const std::uint64_t key =
                    (static_cast<std::uint64_t>(state_id) << 32) |
                    operator_index;
                account_option_cache_insert(key, retained_kernel);
                option_kernel_cache_[key] = retained_kernel;
                if (decision.template_hit) {
                    option_kernel_template_hit_keys_.insert(key);
                }
                state_local_automatic_operator_indices_.insert(
                    operator_index);
                decision.operator_index = operator_index;
                decision.admitted = true;
                admit_operator(operator_index);
                if (new_operator) {
                    for (const std::uint32_t dependency :
                         operators_.at(operator_index).primitive_program) {
                        add_dependency(dependency);
                    }
                    if (operators_.at(operator_index).conditional_action !=
                        kNoId) {
                        add_dependency(
                            operators_.at(operator_index).conditional_action);
                    }
                }
                if (first_variant) {
                    if (decision.telemetry_kind ==
                        AutomaticTelemetryKind::ProtectedSide) {
                        decision.template_matching_ns =
                            static_cast<std::uint64_t>(
                                std::chrono::duration_cast<
                                    std::chrono::nanoseconds>(
                                    std::chrono::steady_clock::now() -
                                    template_matching_started)
                                    .count());
                    }
                    decision.admission_ns += static_cast<std::uint64_t>(
                        std::chrono::duration_cast<std::chrono::nanoseconds>(
                            std::chrono::steady_clock::now() -
                            candidate_started)
                            .count());
                }
                batch.decisions.push_back(std::move(decision));
                first_variant = false;
            }
            check_limits();
        }

        if (!imprint_time_attributed && imprint_discovery_ns != 0) {
            StateLocalAutomaticCandidate timing;
            timing.id = "automatic:imprint_discovery";
            timing.kind = AutomaticCandidateKind::Imprint;
            timing.telemetry_kind = AutomaticTelemetryKind::Imprint;
            timing.admission_ns = imprint_discovery_ns;
            timing.evidence.candidate = true;
            timing.evidence.legality_result = "not_applicable";
            timing.evidence.reason = "no_legal_imprint_checkpoint_carrier";
            batch.decisions.push_back(std::move(timing));
        }

        check_limits(true);
        merge_local_work();
    } catch (const SolverResourceLimit& limit) {
        if (!local_work_merged) {
            try {
                merge_local_work();
            } catch (const SolverResourceLimit&) {
                /* The deferred witness below owns the exact cap name from
                 * the operation that first stopped admission. */
            }
        }
        StateLocalAutomaticCandidate deferred;
        deferred.id = "automatic:state_local_generation";
        deferred.deferred = true;
        deferred.evidence.candidate = true;
        deferred.evidence.legality_result = "deferred_resource_cap";
        deferred.evidence.reason =
            "price_independent_kernel_generation_" + limit.cap_name();
        batch.decisions.push_back(std::move(deferred));
    }
    std::sort(
        batch.admitted_operators.begin(), batch.admitted_operators.end());
    batch.admitted_operators.erase(
        std::unique(
            batch.admitted_operators.begin(),
            batch.admitted_operators.end()),
        batch.admitted_operators.end());
    const auto [stored, inserted] =
        state_local_automatic_operators_.emplace(
            state_id, batch.admitted_operators);
    if (inserted) account_state_local_operators(stored->second);
    return batch;
}

const OptionKernel& CalcContext::option_kernel(
    const std::uint32_t state_id,
    const std::uint32_t operator_index) {
    const PlannerOperator& option = operators_.at(operator_index);
    if (option.kind != PlannerOperatorKind::FixedOption) {
        throw std::invalid_argument("option kernel requested for primitive");
    }
    const std::uint64_t key =
        (static_cast<std::uint64_t>(state_id) << 32) | operator_index;
    const auto cached = option_kernel_cache_.find(key);
    if (cached != option_kernel_cache_.end()) return *cached->second;

    auto result = std::make_shared<OptionKernel>();
    result->supported = true;
    result->legal = true;
    result->terminates_almost_surely = true;
    result->automatic.candidate =
        option.automatic_kind != AutomaticCandidateKind::None;
    result->automatic.relevant_goal_mask = option.relevant_goal_mask;
    if (result->automatic.candidate) {
        result->automatic.legality_result = "pending_exact_kernel";
    }

    if (option.option_kind == FixedOptionKind::Renewal ||
        option.option_kind == FixedOptionKind::ProtectedRepeat ||
        option.option_kind == FixedOptionKind::FracturePrepare ||
        option.option_kind == FixedOptionKind::ImprintRetry ||
        option.option_kind == FixedOptionKind::TemporaryBenchRepeat) {
        const auto finish = [&]() -> const OptionKernel& {
            const auto finish_started =
                option.option_kind == FixedOptionKind::ProtectedRepeat
                    ? std::chrono::steady_clock::now()
                    : std::chrono::steady_clock::time_point{};
            if (result->automatic.candidate) {
                result->automatic.eligible =
                    result->supported && result->legal &&
                    result->terminates_almost_surely;
                if (result->automatic.reason.empty()) {
                    result->automatic.reason =
                        result->automatic.eligible
                            ? "complete_exact_automatic_kernel"
                            : !result->supported
                                  ? "exact_kernel_unsupported"
                                  : !result->legal
                                        ? "exact_legality_or_relevance_refused"
                                        : "retry_chain_not_almost_sure";
                }
                if (result->automatic.legality_result ==
                    "pending_exact_kernel") {
                    result->automatic.legality_result =
                        result->automatic.eligible ? "legal" : "illegal";
                }
            }
            std::sort(result->retry_states.begin(),
                      result->retry_states.end());
            result->retry_states.erase(
                std::unique(result->retry_states.begin(),
                            result->retry_states.end()),
                result->retry_states.end());
            std::sort(result->continuation_states.begin(),
                      result->continuation_states.end());
            result->continuation_states.erase(
                std::unique(result->continuation_states.begin(),
                            result->continuation_states.end()),
                result->continuation_states.end());
            std::shared_ptr<const OptionKernel> retained = result;
            if (result->supported && result->legal) {
                const std::uint64_t template_id =
                    option_template_hash(option, *result);
                result->retained_template_id = template_id;
                const auto bucket = option_kernel_templates_.find(template_id);
                if (bucket != option_kernel_templates_.end()) {
                    for (const OptionKernelTemplateMemo& memo :
                         bucket->second) {
                        if (memo.operator_index >= operators_.size() ||
                            !same_option_template_planner(
                                operators_.at(memo.operator_index), option) ||
                            memo.expected_resources !=
                                result->expected_resources ||
                            !same_complete_option_kernel(
                                *memo.kernel, *result)) {
                            continue;
                        }
                        retained = memo.kernel;
                        option_kernel_template_hit_keys_.insert(key);
                        break;
                    }
                }
                if (retained.get() == result.get()) {
                    auto& templates =
                        option_kernel_templates_[template_id];
                    const std::size_t old_capacity =
                        templates.capacity();
                    templates.push_back(
                        {operator_index, retained,
                         retained->expected_resources});
                    account_option_template_insert(
                        old_capacity, templates.back());
                }
            }
            account_option_cache_insert(key, retained);
            const auto inserted = option_kernel_cache_.emplace(
                key, std::move(retained));
            const OptionKernel& stored = *inserted.first->second;
            if (option.option_kind == FixedOptionKind::ProtectedRepeat) {
                telemetry_.protected_finish_ns +=
                    static_cast<std::uint64_t>(
                        std::chrono::duration_cast<std::chrono::nanoseconds>(
                            std::chrono::steady_clock::now() - finish_started)
                            .count());
            }
            return stored;
        };
        if (option.primitive_program.empty() ||
            (option.option_kind != FixedOptionKind::FracturePrepare &&
             !action_legal(
                 *session_, registry_.actions.at(
                                option.primitive_program.front()),
                 state(state_id)))) {
            result->legal = false;
            result->terminates_almost_surely = false;
            result->automatic.reason = "setup_or_first_step_illegal";
            return finish();
        }
        if (option.option_kind == FixedOptionKind::ImprintRetry) {
            if (!native_imprint_checkpoint_creation_legal(*this, state_id)) {
                result->legal = false;
                result->terminates_almost_surely = false;
                result->automatic.legality_result =
                    "checkpoint_creation_illegal";
                result->automatic.reason =
                    "native_imprint_checkpoint_creation_refused";
                return finish();
            }
        }
        if (option.option_kind == FixedOptionKind::ProtectedRepeat) {
            const std::uint32_t lock_flag =
                option.intended_side == PC_SIDE_PREFIX
                    ? kFlagPrefixesLocked
                    : kFlagSuffixesLocked;
            if ((state(state_id).flags & lock_flag) != 0) {
                result->legal = false;
                result->terminates_almost_surely = false;
                result->automatic.reason =
                    "protection_already_active_no_exact_reapplication";
                return finish();
            }
            if (!setup_applies_exactly(
                    *this, state_id, option.setup_action, lock_flag)) {
                result->legal = false;
                result->terminates_almost_surely = false;
                result->automatic.setup_complete = false;
                result->automatic.kernel_change_mechanisms =
                    kAutomaticMetamodProtection;
                result->automatic.reason =
                    "setup_did_not_apply_exactly";
                return finish();
            }
        }
        if (option.option_kind == FixedOptionKind::TemporaryBenchRepeat &&
            state_has_unfractured_crafted(state(state_id))) {
            result->legal = false;
            result->terminates_almost_surely = false;
            result->automatic.reason =
                "cleanup_would_remove_preexisting_crafted_carrier";
            return finish();
        }
        if (option.option_kind == FixedOptionKind::FracturePrepare) {
            const ActionDescriptor& fracture =
                registry_.actions.at(option.conditional_action);
            const AbstractState entry = state(state_id);
            const bool carrier_ready =
                option.carrier_goal_slot < layout_.slots.size() &&
                entry.slot_status[option.carrier_goal_slot] ==
                    static_cast<std::uint8_t>(GoalSlotStatus::Satisfied) &&
                action_legal(*session_, fracture, entry);
            if (carrier_ready) {
                std::map<std::string, double> resources;
                add_action_resources(resources, fracture, 1.0);
                result->expected_resources.assign(
                    resources.begin(), resources.end());
                result->expected_primitive_actions = 1.0;
                result->entry_continues = true;
                const OutcomeDistribution& distribution = outcomes(
                    state_id, option.conditional_action);
                if (!distribution.supported ||
                    !distribution.choice_groups.empty()) {
                    result->supported = false;
                    result->legal = false;
                    result->terminates_almost_surely = false;
                    return finish();
                }
                result->exits = distribution.entries;
                result->legal = !result->exits.empty();
                if (result->automatic.candidate) {
                    AttemptKernel fractured;
                    fractured.supported = distribution.supported;
                    fractured.fully_legal = result->legal;
                    fractured.entries = distribution.entries;
                    result->automatic.kernel_changed = true;
                    result->automatic.kernel_change_mechanisms =
                        kAutomaticCarrierFracture;
                    result->automatic.candidate_kernel_hash =
                        attempt_kernel_hash(fractured);
                    result->automatic.setup_complete = true;
                    result->automatic.cleanup_complete = true;
                    result->automatic.recovery_complete = true;
                    result->automatic.exits_complete = result->legal;
                    result->automatic.legality_result =
                        result->legal ? "legal" : "illegal";
                    result->automatic.reason =
                        result->legal
                            ? "exact_satisfying_carrier_fracture_route"
                            : "fracture_has_no_complete_exact_exits";
                }
                return finish();
            }
            if ((entry.flags &
                 (kFlagInfluenced | kFlagSynthesised | kFlagFractured)) !=
                0) {
                result->legal = false;
                result->terminates_almost_surely = false;
                result->automatic.reason =
                    "carrier_flags_make_fracture_path_illegal";
                return finish();
            }
        }

        const auto attempt_started =
            option.option_kind == FixedOptionKind::ProtectedRepeat
                ? std::chrono::steady_clock::now()
                : std::chrono::steady_clock::time_point{};
        const AttemptKernel attempt = execute_attempt(
            *this, option.primitive_program, state_id);
        if (option.option_kind == FixedOptionKind::ProtectedRepeat) {
            telemetry_.protected_attempt_ns +=
                static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now() - attempt_started)
                        .count());
        }
        if (!attempt.supported) {
            result->supported = false;
            result->legal = false;
            result->terminates_almost_surely = false;
            return finish();
        }
        if (!attempt.fully_legal) {
            result->legal = false;
            result->terminates_almost_surely = false;
            result->automatic.reason =
                option.option_kind == FixedOptionKind::TemporaryBenchRepeat
                    ? "missing_setup_followup_or_cleanup_route"
                    : "one_or_more_program_steps_illegal";
            return finish();
        }
        if (option.option_kind == FixedOptionKind::TemporaryBenchRepeat ||
            (option.option_kind == FixedOptionKind::ProtectedRepeat &&
             result->automatic.candidate)) {
            const AbstractState entry = state(state_id);
            const auto baseline_started =
                option.option_kind == FixedOptionKind::ProtectedRepeat
                    ? std::chrono::steady_clock::now()
                    : std::chrono::steady_clock::time_point{};
            if (option.option_kind == FixedOptionKind::ProtectedRepeat &&
                result->automatic.candidate &&
                defer_automatic_protected_baseline_) {
                /* State-local automatic admission compares this exact
                 * candidate with the parent-context baseline after local
                 * normalization. The parent owns cross-carrier reforge
                 * sharing; no candidate is retained before that comparison. */
                result->automatic.kernel_changed = true;
                result->automatic_candidate_attempt_entries =
                    attempt.entries;
            } else {
                const AttemptKernel baseline = execute_attempt(
                    *this, {option.followup_action}, state_id);
                if (option.option_kind == FixedOptionKind::ProtectedRepeat) {
                    telemetry_.protected_baseline_ns +=
                        static_cast<std::uint64_t>(
                            std::chrono::duration_cast<
                                std::chrono::nanoseconds>(
                                std::chrono::steady_clock::now() -
                                baseline_started)
                                .count());
                }
                result->automatic.baseline_kernel_hash =
                    attempt_kernel_hash(baseline);
                result->automatic.candidate_kernel_hash =
                    attempt_kernel_hash(attempt);
                result->automatic.kernel_changed =
                    baseline.supported && baseline.fully_legal &&
                    !same_attempt_outcomes(baseline, attempt);
            }
            result->automatic.setup_complete = setup_applies_exactly(
                *this, state_id, option.setup_action,
                option.option_kind == FixedOptionKind::ProtectedRepeat
                    ? static_cast<std::uint32_t>(
                          option.intended_side == PC_SIDE_PREFIX
                              ? kFlagPrefixesLocked
                              : kFlagSuffixesLocked)
                    : std::uint32_t{0});

            bool carrier_relevant = true;
            bool cleanup_complete = true;
            std::uint32_t target_mask = option.relevant_goal_mask;
            if (option.option_kind == FixedOptionKind::ProtectedRepeat) {
                std::uint32_t protected_mask = 0;
                for (std::uint32_t slot = 0;
                     slot < goal_.slots.size(); ++slot) {
                    if (goal_slot_side(session(), goal_.slots[slot]) ==
                        option.intended_side) {
                        protected_mask |= 1u << slot;
                    }
                }
                carrier_relevant =
                    (satisfied_goal_mask(entry) & protected_mask) != 0;
                target_mask &= ~protected_mask;
                const std::uint32_t lock_flag =
                    option.intended_side == PC_SIDE_PREFIX
                        ? kFlagPrefixesLocked
                        : kFlagSuffixesLocked;
                cleanup_complete =
                    all_exits_without_flag(*this, attempt.entries, lock_flag);
                result->automatic.kernel_change_mechanisms =
                    kAutomaticMetamodProtection;
            } else {
                cleanup_complete = all_exits_cleaned(*this, attempt.entries);
                const ActionDescriptor& blocker =
                    registry_.actions.at(option.setup_action);
                if (blocker.params.mod_id < session().mod_count &&
                    session().group_offsets[blocker.params.mod_id] <
                        session().group_offsets[blocker.params.mod_id + 1]) {
                    result->automatic.kernel_change_mechanisms |=
                        kAutomaticGroupConflict;
                }
                if (blocker.params.mod_id < session().gen_type.size()) {
                    result->automatic.kernel_change_mechanisms |=
                        session().gen_type[blocker.params.mod_id] ==
                                PC_SIDE_PREFIX
                            ? kAutomaticPrefixSlot
                            : kAutomaticSuffixSlot;
                }
            }
            result->automatic.cleanup_complete = cleanup_complete;
            const bool relevant = carrier_relevant &&
                advances_goal_mask(*this, entry, attempt.entries, target_mask);
            if (!result->automatic.setup_complete) {
                result->legal = false;
                result->terminates_almost_surely = false;
                result->automatic.reason = "setup_did_not_apply_exactly";
                return finish();
            }
            if (!result->automatic.kernel_changed) {
                result->legal = false;
                result->terminates_almost_surely = false;
                result->automatic.reason = "exact_successor_kernel_neutral";
                return finish();
            }
            if (!relevant) {
                result->legal = false;
                result->terminates_almost_surely = false;
                result->automatic.reason =
                    option.option_kind == FixedOptionKind::ProtectedRepeat
                        ? "no_satisfied_protected_carrier_or_goal_relevant_exit"
                        : "following_action_does_not_advance_relevant_goal";
                return finish();
            }
            if (!cleanup_complete) {
                result->legal = false;
                result->terminates_almost_surely = false;
                result->automatic.reason = "cleanup_or_replacement_incomplete";
                return finish();
            }
            result->automatic.legality_result = "legal";
        }
        const OutcomeDistribution* entry_reforge_kernel = nullptr;
        std::uint32_t entry_reforge_action = kNoId;
        struct ProtectedReforgeCertificate {
            bool exact = false;
            const OutcomeDistribution* distribution = nullptr;
        };
        const auto protected_reforge_kernel =
            [&](const std::uint32_t candidate)
                -> ProtectedReforgeCertificate {
            if (option.option_kind != FixedOptionKind::ProtectedRepeat ||
                option.primitive_program.size() != 2 ||
                option.primitive_program.front() != option.setup_action ||
                option.primitive_program.back() != option.followup_action) {
                return {};
            }
            const std::uint32_t lock_flag =
                option.intended_side == PC_SIDE_PREFIX
                    ? kFlagPrefixesLocked
                    : kFlagSuffixesLocked;
            if (!setup_applies_exactly(
                    *this, candidate, option.setup_action, lock_flag)) {
                /* The entry attempt is fully legal. An inapplicable exact
                 * setup therefore cannot describe the same attempt. */
                return {true, nullptr};
            }
            const OutcomeDistribution& setup =
                outcomes(candidate, option.setup_action);
            const OutcomeEntry* deterministic_exit = nullptr;
            for (const OutcomeEntry& exit : setup.entries) {
                if (exit.probability <= 0.0) continue;
                if (deterministic_exit != nullptr ||
                    exit.probability != 1.0) {
                    return {};
                }
                deterministic_exit = &exit;
            }
            if (deterministic_exit == nullptr) return {};
            if (!action_legal(
                    *session_,
                    registry_.actions.at(option.followup_action),
                    state(deterministic_exit->state))) {
                return {true, nullptr};
            }
            const OutcomeDistribution& reforge = outcomes(
                deterministic_exit->state, option.followup_action);
            if (!reforge.supported || !reforge.choice_groups.empty()) {
                return {true, nullptr};
            }
            return {true, &reforge};
        };
        const bool protected_reforge_certificate =
            option.option_kind == FixedOptionKind::ProtectedRepeat;
        if (protected_reforge_certificate) {
            entry_reforge_action = option.followup_action;
            entry_reforge_kernel =
                protected_reforge_kernel(state_id).distribution;
        } else if (option.primitive_program.size() == 1) {
            entry_reforge_action = option.primitive_program.front();
            const ActionDescriptor& action =
                registry_.actions.at(entry_reforge_action);
            if (approved_renewal_roll(action)) {
                /* The reforge cache owns immutable shared distributions keyed
                 * by action and preserved base. Pointer identity is therefore
                 * an exact same-kernel certificate for a one-action renewal. */
                entry_reforge_kernel =
                    &outcomes(state_id, entry_reforge_action);
            }
        }
        std::map<std::uint32_t, AttemptKernel> retry_attempts;
        const auto retry_equivalent = [&](const std::uint32_t candidate) {
            if (option.option_kind == FixedOptionKind::ProtectedRepeat) {
                ++telemetry_.protected_retry_checks;
                const std::uint32_t lock_flag =
                    option.intended_side == PC_SIDE_PREFIX
                        ? kFlagPrefixesLocked
                        : kFlagSuffixesLocked;
                if ((state(candidate).flags & lock_flag) != 0) return false;
            }
            if (candidate == state_id) return true;
            if (entry_reforge_kernel != nullptr &&
                protected_reforge_certificate) {
                const ProtectedReforgeCertificate candidate_kernel =
                    protected_reforge_kernel(candidate);
                if (candidate_kernel.exact) {
                    if (candidate_kernel.distribution == nullptr) {
                        return false;
                    }
                    const bool same_kernel =
                        (entry_reforge_kernel->stable_shared_kernel &&
                         candidate_kernel.distribution ==
                             entry_reforge_kernel) ||
                        (attempt.choice_groups.empty() &&
                         attempt.choice_options.empty() &&
                         candidate_kernel.distribution->entries ==
                             attempt.entries);
                    if (same_kernel) {
                        ++telemetry_.protected_retry_certificates;
                    }
                    /* With a deterministic exact setup, the follow-up
                     * distribution is the complete attempt kernel. Equal
                     * entries certify retry equivalence; unequal entries
                     * certify that it is an outer exit. */
                    return same_kernel;
                }
            }
            if (entry_reforge_kernel != nullptr &&
                !protected_reforge_certificate &&
                action_legal(
                    *session_, registry_.actions.at(entry_reforge_action),
                    state(candidate)) &&
                &outcomes(candidate, entry_reforge_action) ==
                    entry_reforge_kernel) {
                return true;
            }
            if (protected_reforge_certificate) {
                ++telemetry_.protected_retry_fallbacks;
            }
            auto found = retry_attempts.find(candidate);
            if (found == retry_attempts.end()) {
                found = retry_attempts.emplace(
                    candidate,
                    execute_attempt(
                        *this, option.primitive_program, candidate)).first;
            }
            return same_attempt(attempt, found->second);
        };

        std::map<std::string, double> resources;
        add_scaled_resources(resources, attempt.expected_resources);
        result->expected_primitive_actions =
            attempt.expected_primitive_actions;

        if (option.option_kind == FixedOptionKind::ImprintRetry) {
            if (!attempt.choice_groups.empty() ||
                !attempt.choice_options.empty()) {
                result->supported = false;
                result->legal = false;
                result->terminates_almost_surely = false;
                return finish();
            }
            const BestiaryActionDescriptor& create =
                session_->data->bestiary_actions.at(
                    option.bestiary_create_action);
            for (const std::string& price_key : create.cost_keys) {
                resources[price_key] += 1.0;
            }
            result->expected_primitive_actions += 1.0;
            std::map<std::uint32_t, double> exits;
            double retry_probability = 0.0;
            for (const OutcomeEntry& outcome : attempt.entries) {
                if (option_exit_matches(state(outcome.state), option)) {
                    exits[outcome.state] += outcome.probability;
                } else {
                    exits[kNoId] += outcome.probability;
                    retry_probability += outcome.probability;
                    result->expected_primitive_actions += outcome.probability;
                    result->retry_states.push_back(outcome.state);
                }
            }
            result->expected_resources.assign(
                resources.begin(), resources.end());
            for (const auto& [exit, probability] : exits) {
                result->exits.push_back({exit, probability});
            }
            result->terminates_almost_surely =
                retry_probability < 1.0 - 1e-15;
            result->legal = result->supported &&
                            result->terminates_almost_surely &&
                            !result->exits.empty();
            if (result->automatic.candidate) {
                AttemptKernel restored;
                restored.supported = result->supported;
                restored.fully_legal = result->legal;
                restored.expected_primitive_actions =
                    result->expected_primitive_actions;
                restored.expected_resources = result->expected_resources;
                restored.entries = result->exits;
                result->automatic.baseline_kernel_hash =
                    attempt_kernel_hash(attempt);
                result->automatic.candidate_kernel_hash =
                    attempt_kernel_hash(restored);
                result->automatic.kernel_changed =
                    retry_probability < 1.0 - 1e-15;
                result->automatic.kernel_change_mechanisms =
                    kAutomaticImprintCheckpoint;
                result->automatic.setup_complete = true;
                result->automatic.cleanup_complete = true;
                result->automatic.recovery_complete =
                    result->terminates_almost_surely;
                result->automatic.exits_complete = !result->exits.empty();
                result->automatic.legality_result =
                    result->legal ? "legal" : "illegal";
                result->automatic.reason = result->legal
                    ? "exact_imprint_checkpoint_attempt_restore_and_intermediate_exit"
                    : "imprint_attempt_has_no_almost_sure_intermediate_exit";
            }
            return finish();
        }

        if (option.option_kind == FixedOptionKind::FracturePrepare) {
            const ActionDescriptor& fracture =
                registry_.actions.at(option.conditional_action);
            const auto carrier_ready = [&](const std::uint32_t candidate) {
                const AbstractState& candidate_state = state(candidate);
                return option.carrier_goal_slot < layout_.slots.size() &&
                       candidate_state.slot_status[
                           option.carrier_goal_slot] ==
                           static_cast<std::uint8_t>(
                               GoalSlotStatus::Satisfied) &&
                       action_legal(
                           *session_, fracture, candidate_state);
            };
            std::map<std::uint32_t, double> exits;
            double retry_probability = 0.0;
            if (!attempt.choice_groups.empty()) {
                result->supported = false;
                result->legal = false;
                result->terminates_almost_surely = false;
                return finish();
            }
            for (const OutcomeEntry& prepared : attempt.entries) {
                if (carrier_ready(prepared.state)) {
                        result->continuation_states.push_back(
                            prepared.state);
                        result->expected_primitive_actions +=
                            prepared.probability;
                        add_action_resources(
                            resources, fracture, prepared.probability);
                        const OutcomeDistribution& fractured = outcomes(
                            prepared.state, option.conditional_action);
                        if (!fractured.supported ||
                            !fractured.choice_groups.empty()) {
                            result->supported = false;
                            result->legal = false;
                            result->terminates_almost_surely = false;
                            return finish();
                        }
                        for (const OutcomeEntry& outcome :
                             fractured.entries) {
                            exits[outcome.state] +=
                                prepared.probability * outcome.probability;
                        }
                } else if (retry_equivalent(prepared.state)) {
                        exits[kNoId] += prepared.probability;
                        retry_probability += prepared.probability;
                        result->retry_states.push_back(prepared.state);
                } else {
                        /* A changed preparation carrier, illegal Fracture,
                         * or other brick/salvage state remains visible. */
                        exits[prepared.state] += prepared.probability;
                }
            }
            result->expected_resources.assign(
                resources.begin(), resources.end());
            for (const auto& [exit, probability] : exits) {
                result->exits.push_back({exit, probability});
            }
            result->terminates_almost_surely =
                retry_probability < 1.0 - 1e-15;
            result->legal = result->supported &&
                            result->terminates_almost_surely &&
                            !result->exits.empty() &&
                            (!result->automatic.candidate ||
                             !result->continuation_states.empty());
            if (result->automatic.candidate) {
                AttemptKernel fractured;
                fractured.supported = result->supported;
                fractured.fully_legal = result->legal;
                fractured.entries = result->exits;
                result->automatic.kernel_changed =
                    !result->continuation_states.empty();
                result->automatic.kernel_change_mechanisms =
                    kAutomaticCarrierFracture;
                result->automatic.candidate_kernel_hash =
                    attempt_kernel_hash(fractured);
                result->automatic.setup_complete = attempt.fully_legal;
                result->automatic.cleanup_complete = true;
                result->automatic.recovery_complete =
                    result->terminates_almost_surely;
                result->automatic.exits_complete = !result->exits.empty();
                result->automatic.legality_result =
                    result->legal ? "legal" : "illegal";
                result->automatic.reason =
                    result->continuation_states.empty()
                        ? "preparation_never_reaches_exact_legal_carrier"
                        : result->legal
                              ? "exact_preparation_retry_and_fracture_exits"
                              : "fracture_retry_or_outer_exit_incomplete";
            }
            return finish();
        }

        const auto normalization_started =
            option.option_kind == FixedOptionKind::ProtectedRepeat
                ? std::chrono::steady_clock::now()
                : std::chrono::steady_clock::time_point{};
        if (option_exit_matches(state(state_id), option)) {
            result->legal = false;
            result->terminates_almost_surely = false;
            return finish();
        }
        std::map<std::uint32_t, double> exits;
        std::map<
            std::pair<std::uint32_t, std::vector<std::uint32_t>>,
            double> choices;
        std::map<std::uint32_t, std::uint32_t> normalized;
        const auto normalize = [&](const std::uint32_t actual) {
            const auto cached = normalized.find(actual);
            if (cached != normalized.end()) return cached->second;
            const bool retry =
                !option_exit_matches(state(actual), option) &&
                retry_equivalent(actual);
            const std::uint32_t successor = retry ? kNoId : actual;
            normalized.emplace(actual, successor);
            if (retry) result->retry_states.push_back(actual);
            return successor;
        };
        double forced_retry_probability = 0.0;
        for (const OutcomeEntry& outcome : attempt.entries) {
            const std::uint32_t successor = normalize(outcome.state);
            exits[successor] += outcome.probability;
            if (successor == kNoId) {
                forced_retry_probability += outcome.probability;
            }
        }
        for (const OutcomeChoiceGroup& group : attempt.choice_groups) {
            std::vector<std::uint32_t> successors;
            for (const std::uint32_t actual : group.states) {
                successors.push_back(normalize(actual));
            }
            std::sort(successors.begin(), successors.end());
            successors.erase(
                std::unique(successors.begin(), successors.end()),
                successors.end());
            choices[{group.observation_state, successors}] +=
                group.probability;
            if (successors.size() == 1 && successors.front() == kNoId) {
                forced_retry_probability += group.probability;
            }
        }
        for (const OutcomeChoiceOption& choice : attempt.choice_options) {
            result->observation_choice_options.push_back(
                {choice.mod_id, normalize(choice.actual_state),
                 choice.observation_state, choice.actual_state});
        }
        if (option.option_kind == FixedOptionKind::ProtectedRepeat) {
            telemetry_.protected_normalization_ns +=
                static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now() -
                        normalization_started)
                        .count());
        }
        result->expected_resources.assign(
            resources.begin(), resources.end());
        for (const auto& [exit, probability] : exits) {
            result->exits.push_back({exit, probability});
        }
        for (const auto& [choice, probability] : choices) {
            result->observation_choice_groups.push_back(
                {probability, choice.second, choice.first});
        }
        result->terminates_almost_surely =
            forced_retry_probability < 1.0 - 1e-15;
        result->legal = result->supported &&
                        result->terminates_almost_surely &&
                        (!result->exits.empty() ||
                         !result->observation_choice_groups.empty());
        if (result->automatic.candidate) {
            result->automatic.recovery_complete =
                result->terminates_almost_surely;
            result->automatic.exits_complete =
                !result->exits.empty() ||
                !result->observation_choice_groups.empty();
            if (!result->legal && result->automatic.reason.empty()) {
                result->automatic.reason =
                    "success_failure_recovery_or_outer_exit_incomplete";
            }
        }
        return finish();
    }

    if (option.option_kind == FixedOptionKind::ProtectedSide) {
        const std::uint32_t lock_flag =
            option.intended_side == PC_SIDE_PREFIX
                ? kFlagPrefixesLocked
                : kFlagSuffixesLocked;
        if (!setup_applies_exactly(
                *this, state_id, option.setup_action, lock_flag)) {
            result->legal = false;
            result->terminates_almost_surely = false;
            if (result->automatic.candidate) {
                result->automatic.setup_complete = false;
                result->automatic.kernel_change_mechanisms =
                    kAutomaticMetamodProtection;
                result->automatic.exits_complete = false;
                result->automatic.recovery_complete = false;
                result->automatic.eligible = false;
                result->automatic.legality_result = "illegal";
                result->automatic.reason =
                    "setup_did_not_apply_exactly";
            }
            std::shared_ptr<const OptionKernel> retained = result;
            account_option_cache_insert(key, retained);
            const auto inserted = option_kernel_cache_.emplace(
                key, std::move(retained));
            return *inserted.first->second;
        }
    }

    result->expected_primitive_actions =
        static_cast<double>(option.primitive_program.size());
    result->expected_resources = option.resource_quantities;

    const auto protected_side_program_started =
        option.option_kind == FixedOptionKind::ProtectedSide
            ? std::chrono::steady_clock::now()
            : std::chrono::steady_clock::time_point{};
    std::map<std::uint32_t, double> frontier{{state_id, 1.0}};
    for (std::size_t step = 0; step < option.primitive_program.size(); ++step) {
        const std::uint32_t action_index = option.primitive_program[step];
        const ActionDescriptor& action = registry_.actions.at(action_index);
        const bool final_step = step + 1 == option.primitive_program.size();
        std::map<std::uint32_t, double> next;
        for (const auto& [entry_state, entry_probability] : frontier) {
            const AbstractState& abstract = state(entry_state);
            if (!action_legal(*session_, action, abstract)) {
                result->legal = false;
                result->terminates_almost_surely = false;
                if (option.option_kind ==
                        FixedOptionKind::EldritchSideIntent &&
                    option.automatic_kind ==
                        AutomaticCandidateKind::EldritchSide) {
                    result->automatic.reason =
                        "eldritch_side_program_step_illegal:" +
                        action.id;
                }
                frontier.clear();
                break;
            }
            if (option.option_kind == FixedOptionKind::EldritchSideIntent &&
                final_step &&
                !intended_eldritch_action_legal(
                    *this, abstract, action, option.intended_side,
                    entry_state)) {
                result->legal = false;
                result->terminates_almost_surely = false;
                if (option.automatic_kind ==
                    AutomaticCandidateKind::EldritchSide) {
                    result->automatic.reason =
                        "eldritch_side_intended_dominance_illegal:" +
                        action.id;
                }
                frontier.clear();
                break;
            }

            const OutcomeDistribution& distribution =
                outcomes(entry_state, action_index);
            if (!distribution.supported ||
                !distribution.choice_groups.empty()) {
                result->supported = false;
                result->legal = false;
                result->terminates_almost_surely = false;
                if (option.option_kind ==
                        FixedOptionKind::EldritchSideIntent &&
                    option.automatic_kind ==
                        AutomaticCandidateKind::EldritchSide) {
                    result->automatic.reason =
                        !distribution.supported
                            ? "eldritch_side_distribution_unsupported:" +
                                  action.id
                            : "eldritch_side_choice_distribution_unsupported:" +
                                  action.id;
                }
                frontier.clear();
                break;
            }
            if (distribution.entries.empty() &&
                option.option_kind ==
                    FixedOptionKind::EldritchSideIntent &&
                option.automatic_kind ==
                    AutomaticCandidateKind::EldritchSide) {
                result->automatic.reason =
                    "eldritch_side_distribution_empty:" +
                    action.id;
            }
            for (const OutcomeEntry& exit : distribution.entries) {
                /* Lock and Multimod setup primitives must actually apply.
                 * This is the exact crafted-count/open-side initiation check,
                 * not a charged no-op hidden inside the fixed option. */
                const bool requires_state_change =
                    (option.option_kind == FixedOptionKind::ProtectedSide &&
                     step == 0) ||
                    option.option_kind == FixedOptionKind::MultimodFinish;
                if (requires_state_change && exit.state == entry_state) {
                    result->legal = false;
                    result->terminates_almost_surely = false;
                    frontier.clear();
                    next.clear();
                    break;
                }
                next[exit.state] += entry_probability * exit.probability;
            }
            if (!result->legal) break;
        }
        if (!result->legal || !result->supported) break;
        frontier = std::move(next);

        if (option.option_kind == FixedOptionKind::ProtectedSide && step == 0) {
            const std::uint32_t required_flag =
                option.intended_side == PC_SIDE_PREFIX
                    ? kFlagPrefixesLocked
                    : kFlagSuffixesLocked;
            for (const auto& [exit_state, probability] : frontier) {
                (void)probability;
                if ((state(exit_state).flags & required_flag) == 0) {
                    result->legal = false;
                    result->terminates_almost_surely = false;
                    frontier.clear();
                    break;
                }
            }
        }
    }
    if (option.option_kind == FixedOptionKind::ProtectedSide) {
        telemetry_.protected_attempt_ns +=
            static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() -
                    protected_side_program_started)
                    .count());
    }

    if (result->legal && result->supported) {
        for (const auto& [exit_state, probability] : frontier) {
            result->exits.push_back({exit_state, probability});
        }
        if (result->exits.empty()) {
            result->legal = false;
            result->terminates_almost_surely = false;
        }
    }
    const auto protected_side_evidence_started =
        option.option_kind == FixedOptionKind::ProtectedSide
            ? std::chrono::steady_clock::now()
            : std::chrono::steady_clock::time_point{};
    if (result->automatic.candidate) {
        const AbstractState entry = state(state_id);
        result->automatic.exits_complete = !result->exits.empty();
        result->automatic.recovery_complete =
            result->automatic.exits_complete;
        if (option.option_kind == FixedOptionKind::MultimodFinish) {
            result->automatic.kernel_changed = true;
            result->automatic.kernel_change_mechanisms =
                kAutomaticDeterministicFinish;
            result->automatic.setup_complete = result->legal;
            result->automatic.cleanup_complete = true;
            if (!advances_goal_mask(
                    *this, entry, result->exits,
                    option.relevant_goal_mask)) {
                result->legal = false;
                result->terminates_almost_surely = false;
                result->automatic.reason =
                    "multimod_program_does_not_advance_goal_subset";
            } else {
                result->automatic.reason =
                    "legal_exact_multimod_goal_finish";
            }
        } else if (
            option.option_kind ==
                FixedOptionKind::EldritchSideIntent &&
            option.automatic_kind ==
                AutomaticCandidateKind::EldritchSide) {
            result->automatic.kernel_changed = true;
            result->automatic.kernel_change_mechanisms =
                kAutomaticEldritchDominance |
                (option.intended_side == PC_SIDE_PREFIX
                     ? kAutomaticPrefixSlot
                     : kAutomaticSuffixSlot);
            result->automatic.setup_complete =
                result->legal && result->supported;
            result->automatic.cleanup_complete = true;
            if (!result->automatic.exits_complete) {
                result->legal = false;
                result->terminates_almost_surely = false;
                if (result->automatic.reason.empty()) {
                    result->automatic.reason =
                        "eldritch_side_exit_coverage_incomplete";
                }
            } else {
                result->automatic.reason =
                    option.primitive_program.size() == 1
                        ? "existing_dominance_exact_side_action"
                        : "exact_paid_dominance_setup_and_side_action";
            }
        } else if (option.option_kind == FixedOptionKind::ProtectedSide) {
            std::uint32_t protected_mask = 0;
            for (std::uint32_t slot = 0; slot < goal_.slots.size(); ++slot) {
                if (goal_slot_side(session(), goal_.slots[slot]) ==
                    option.intended_side) {
                    protected_mask |= 1u << slot;
                }
            }
            const std::uint32_t target_mask =
                option.relevant_goal_mask & ~protected_mask;
            const auto baseline_started =
                std::chrono::steady_clock::now();
            const AttemptKernel baseline = execute_attempt(
                *this, {option.followup_action}, state_id);
            telemetry_.protected_baseline_ns +=
                static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now() - baseline_started)
                        .count());
            AttemptKernel candidate;
            candidate.supported = result->supported;
            candidate.fully_legal = result->legal;
            candidate.entries = result->exits;
            result->automatic.baseline_kernel_hash =
                attempt_kernel_hash(baseline);
            result->automatic.candidate_kernel_hash =
                attempt_kernel_hash(candidate);
            result->automatic.kernel_changed =
                baseline.supported && baseline.fully_legal &&
                !same_attempt_outcomes(baseline, candidate);
            const std::uint32_t lock_flag =
                option.intended_side == PC_SIDE_PREFIX
                    ? kFlagPrefixesLocked
                    : kFlagSuffixesLocked;
            result->automatic.setup_complete = setup_applies_exactly(
                *this, state_id, option.setup_action, lock_flag);
            result->automatic.cleanup_complete = all_exits_without_flag(
                *this, result->exits, lock_flag);
            result->automatic.kernel_change_mechanisms =
                kAutomaticMetamodProtection;
            const bool carrier_relevant =
                (satisfied_goal_mask(entry) & protected_mask) != 0;
            const bool target_relevant = advances_goal_mask(
                *this, entry, result->exits, target_mask) ||
                clears_target_space(
                    *this, entry, result->exits, target_mask);
            if (!result->automatic.setup_complete ||
                !result->automatic.kernel_changed || !carrier_relevant ||
                !target_relevant || !result->automatic.cleanup_complete ||
                !result->automatic.exits_complete) {
                result->legal = false;
                result->terminates_almost_surely = false;
                result->automatic.reason =
                    !result->automatic.setup_complete
                        ? "setup_did_not_apply_exactly"
                        : !result->automatic.kernel_changed
                              ? "exact_successor_kernel_neutral"
                              : !carrier_relevant || !target_relevant
                                    ? "unsupported_or_irrelevant_protection_combination"
                                    : !result->automatic.cleanup_complete
                                          ? "cleanup_or_replacement_incomplete"
                                          : "outer_exit_coverage_incomplete";
            } else {
                result->automatic.reason =
                    "exact_protected_side_kernel_and_complete_exits";
            }
        }
        result->automatic.eligible = result->supported && result->legal &&
                                     result->terminates_almost_surely;
        result->automatic.legality_result =
            result->automatic.eligible ? "legal" : "illegal";
    }
    if (option.option_kind == FixedOptionKind::ProtectedSide) {
        telemetry_.protected_normalization_ns +=
            static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() -
                    protected_side_evidence_started)
                    .count());
    }
    const auto protected_side_finish_started =
        option.option_kind == FixedOptionKind::ProtectedSide
            ? std::chrono::steady_clock::now()
            : std::chrono::steady_clock::time_point{};
    std::shared_ptr<const OptionKernel> retained = result;
    account_option_cache_insert(key, retained);
    const auto inserted =
        option_kernel_cache_.emplace(key, std::move(retained));
    const OptionKernel& stored = *inserted.first->second;
    if (option.option_kind == FixedOptionKind::ProtectedSide) {
        telemetry_.protected_finish_ns +=
            static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() -
                    protected_side_finish_started)
                    .count());
    }
    return stored;
}

} // namespace solver
} // namespace poecraft
