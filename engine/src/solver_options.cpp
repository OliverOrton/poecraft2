#include "solver_internal.hpp"
#include "poecraft/bitset.h"

#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <functional>
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
    const pc_item_state& carrier) {
    const SessionImpl& session = calc.session();
    const GoalSpec& goal = calc.goal();
    const ActionRegistry& registry = calc.registry();
    const AbstractState& state = calc.state(state_id);
    AutomaticOptionSynthesis synthesis;
    std::vector<FixedOptionSpec>& result = synthesis.specs;
    if (!goal.automatic_candidates) return synthesis;
    const std::vector<std::uint32_t>& goal_bench =
        calc.automatic_goal_bench_actions();

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
                !action_legal(
                    session, registry.actions.at(effect.followup_action),
                    state) ||
                !action_legal(
                    session,
                    registry.actions.at(effect.blocker_actions.front()),
                    state) ||
                !temporary_blocker_applies(session, carrier, effect)) {
                continue;
            }
            synthesis.temporary_candidate_variants +=
                effect.blocker_actions.size();
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
                    effect.blocker_actions.front();
                created.followup_action = effect.followup_action;
                created.goal_slot = effect.goal_slot;
                synthesis.temporary_groups.push_back(std::move(created));
                group = &synthesis.temporary_groups.back();
            } else {
                group = &synthesis.temporary_groups.at(found->second);
            }
            for (const std::uint32_t blocker : effect.blocker_actions) {
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
    for (const std::int8_t side : {static_cast<std::int8_t>(PC_SIDE_PREFIX),
                                   static_cast<std::int8_t>(PC_SIDE_SUFFIX)}) {
        const int lock_code =
            side == PC_SIDE_PREFIX
                ? session.data->metamod_prefixes_locked_code
                : session.data->metamod_suffixes_locked_code;
        const bool lock_available = std::any_of(
            registry.actions.begin(), registry.actions.end(),
            [&](const ActionDescriptor& action) {
                return action.params.type == ActionType::Bench &&
                       action.params.mod_id < session.metamod_type.size() &&
                       session.metamod_type[action.params.mod_id] == lock_code;
            });
        if (!lock_available) continue;
        std::uint32_t protected_mask = 0;
        for (std::uint32_t slot = 0; slot < goal.slots.size(); ++slot) {
            if (goal_slot_side(session, goal.slots[slot]) == side) {
                protected_mask |= 1u << slot;
            }
        }
        if (protected_mask == 0) continue;
        if ((satisfied_goal_mask(state) & protected_mask) == 0) continue;
        for (const ActionDescriptor& followup : registry.actions) {
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
    if (registry.actions.at(out.back()).params.type == ActionType::Unveil) {
        if (!allow_unveil || effective < 2) {
            throw std::runtime_error(
                "fixed option: Unveil is allowed only as the final observed "
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
            action.params.type == ActionType::Unveil ||
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
        const bool observed = action.params.type == ActionType::Unveil;
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
                         group.states});
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
            action.params.type == ActionType::Unveil ||
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

} // namespace

std::vector<PlannerOperator> build_planner_operators(
    const SessionImpl& session,
    const GoalSpec& goal,
    const ActionRegistry& registry) {
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

    std::set<std::string> option_ids;
    for (const FixedOptionSpec& spec : goal.fixed_options) {
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
            if (spec.setup_action_ids.empty()) {
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
                join_ids(registry, std::vector<std::uint32_t>(
                    option.primitive_program.begin(),
                    option.primitive_program.end() - 1));
            option.display_name =
                std::string("Eldritch ") + side_name(spec.side) +
                " intent: " + craft.display_name;
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
            if (registry.actions.at(action).params.type == ActionType::Unveil &&
                option.option_kind != FixedOptionKind::Renewal) {
                throw std::runtime_error(
                    "fixed option: observed Unveil choices are not fixed "
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
        operators.push_back(std::move(option));
    }
    return operators;
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
    return left.kind == right.kind &&
           left.option_kind == right.option_kind &&
           left.primitive_program == right.primitive_program &&
           left.intended_side == right.intended_side &&
           left.exit_goal_slots == right.exit_goal_slots &&
           left.exit_min_satisfied == right.exit_min_satisfied &&
           left.carrier_goal_slot == right.carrier_goal_slot &&
           left.conditional_action == right.conditional_action &&
           left.bestiary_create_action == right.bestiary_create_action &&
           left.bestiary_restore_action == right.bestiary_restore_action &&
           left.automatic_kind == right.automatic_kind &&
           left.relevant_goal_mask == right.relevant_goal_mask &&
           left.resource_quantities == right.resource_quantities &&
           left.setup_action == right.setup_action &&
           left.followup_action == right.followup_action &&
           left.cleanup_action == right.cleanup_action;
}

std::uint64_t option_planner_hash(const PlannerOperator& planner) {
    std::uint64_t hash = 1469598103934665603ull;
    const auto mix = [&](const std::uint64_t value) {
        hash ^= value;
        hash *= 1099511628211ull;
    };
    mix(static_cast<std::uint8_t>(planner.kind));
    mix(static_cast<std::uint8_t>(planner.option_kind));
    mix(static_cast<std::uint8_t>(planner.automatic_kind));
    mix(static_cast<std::uint8_t>(planner.intended_side + 1));
    mix(planner.exit_min_satisfied);
    mix(planner.carrier_goal_slot);
    mix(planner.conditional_action);
    mix(planner.bestiary_create_action);
    mix(planner.bestiary_restore_action);
    mix(planner.relevant_goal_mask);
    mix(planner.setup_action);
    mix(planner.followup_action);
    mix(planner.cleanup_action);
    for (const std::uint32_t action : planner.primitive_program) mix(action);
    mix(kNoId);
    for (const std::uint32_t slot : planner.exit_goal_slots) mix(slot);
    mix(kNoId);
    for (const auto& [key, quantity] : planner.resource_quantities) {
        for (const unsigned char c : key) mix(c);
        mix(0xffu);
        mix(std::bit_cast<std::uint64_t>(quantity));
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
    mix(static_cast<std::uint8_t>(planner.option_kind));
    mix(static_cast<std::uint8_t>(planner.automatic_kind));
    mix(static_cast<std::uint8_t>(planner.intended_side + 1));
    mix(planner.exit_min_satisfied);
    mix(planner.carrier_goal_slot);
    mix(planner.conditional_action);
    mix(planner.bestiary_create_action);
    mix(planner.bestiary_restore_action);
    mix(planner.relevant_goal_mask);
    mix(planner.setup_action);
    mix(planner.followup_action);
    mix(planner.cleanup_action);
    for (const std::uint32_t action : planner.primitive_program) mix(action);
    mix(kNoId);
    for (const std::uint32_t slot : planner.exit_goal_slots) mix(slot);
    mix(kNoId);
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

} // namespace

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
        request.weight_kind = PoolWeightKind::HarvestSpawnOnly;
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
        state_local_automatic_operators_.emplace(
            state_id, std::vector<std::uint32_t>{});
        return batch;
    }

    pc_item_state carrier;
    if (!materialize(state_id, carrier)) {
        state_local_automatic_operators_.emplace(
            state_id, std::vector<std::uint32_t>{});
        return batch;
    }

    const auto shared_started = std::chrono::steady_clock::now();
    AutomaticOptionSynthesis synthesis =
        synthesize_automatic_options(*this, state_id, carrier);
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
    CalcContext local(
        session_, local_goal, registry_, local_candidates,
        false, false, true);
    local.set_solve_resource_caps(
        limits.max_discovered_states == 0
            ? std::numeric_limits<std::uint32_t>::max()
            : limits.max_discovered_states,
        limits.max_reforge_work == 0
            ? std::numeric_limits<std::uint64_t>::max()
            : limits.max_reforge_work,
        false);
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
        if (estimated_owned_bytes() + local.estimated_owned_bytes() >
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
        telemetry_.owned_byte_audit_requests +=
            work.owned_byte_audit_requests;
        telemetry_.owned_byte_audit_ns += work.owned_byte_audit_ns;
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
                    *session_, imprint_goal, registry_);
            for (std::uint32_t index =
                     static_cast<std::uint32_t>(registry_.actions.size());
                 index < imprint_operators.size(); ++index) {
                local.operators_.push_back(
                    std::move(imprint_operators[index]));
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
            const OutcomeDistribution& distribution =
                local.outcomes(local_state, action_index);
            decision.raw_outcomes = outcome_count(distribution);
            bool advances = false;
            for (const OutcomeEntry& exit : distribution.entries) {
                const AbstractState& successor = local.state(exit.state);
                for (std::uint32_t slot = 0;
                     slot < local.layout().slots.size(); ++slot) {
                    advances |=
                        (planner.relevant_goal_mask & (1u << slot)) != 0 &&
                        successor.slot_status[slot] >
                            local.state(local_state).slot_status[slot];
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
                    auto mapped = std::make_shared<OutcomeDistribution>(
                        map_local_distribution(
                            local, *this, distribution, mapped_states));
                    const std::uint64_t key =
                        (static_cast<std::uint64_t>(state_id) << 32) |
                        action_index;
                    distribution_cache_[key] = std::move(mapped);
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
                !has_prices(local_planner) && !direct_fracture) {
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
                const auto action_has_prices = [&](const std::uint32_t action) {
                    return std::all_of(
                        local.registry().actions.at(action).cost_keys.begin(),
                        local.registry().actions.at(action).cost_keys.end(),
                        [&](const std::string& key) {
                            return limits.prices->contains(key);
                        });
                };
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
                const std::uint64_t local_key =
                    (static_cast<std::uint64_t>(local_state) << 32) |
                    local_operator;
                local_kernel_ptr = kernel.get();
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
            base_decision.raw_outcomes = outcome_count(local_kernel);
            if (base_decision.kind == AutomaticCandidateKind::Imprint &&
                !imprint_time_attributed) {
                base_decision.admission_ns += imprint_discovery_ns;
                imprint_time_attributed = true;
            }
            base_decision.evidence = local_kernel.automatic;
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
            if (local_kernel.automatic.eligible && temporary_group == nullptr) {
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
            if (!local_kernel.automatic.eligible || collapse_non_temporary) {
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

            auto mapped = std::make_shared<OptionKernel>(
                map_local_option_kernel(
                    local, *this, local_kernel, mapped_states));
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
                option_transition_templates_[transition_template_id].push_back(
                    retained_kernel);
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
                    option_operator_templates_[planner_id].push_back(
                        operator_index);
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
    state_local_automatic_operators_.emplace(
        state_id, batch.admitted_operators);
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
                    option_kernel_templates_[template_id].push_back(
                        {operator_index, retained,
                         retained->expected_resources});
                }
            }
            const auto inserted = option_kernel_cache_.emplace(
                key, std::move(retained));
            return *inserted.first->second;
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

        const AttemptKernel attempt = execute_attempt(
            *this, option.primitive_program, state_id);
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
            const AttemptKernel baseline = execute_attempt(
                *this, {option.followup_action}, state_id);
            result->automatic.baseline_kernel_hash =
                attempt_kernel_hash(baseline);
            result->automatic.candidate_kernel_hash =
                attempt_kernel_hash(attempt);
            result->automatic.kernel_changed =
                baseline.supported && baseline.fully_legal &&
                !same_attempt_outcomes(baseline, attempt);
            result->automatic.setup_complete = setup_applies_exactly(
                *this, state_id, option.setup_action,
                option.option_kind == FixedOptionKind::ProtectedRepeat
                    ? (option.intended_side == PC_SIDE_PREFIX
                           ? kFlagPrefixesLocked
                           : kFlagSuffixesLocked)
                    : 0);

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
        if (option.primitive_program.size() == 1) {
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
                const std::uint32_t lock_flag =
                    option.intended_side == PC_SIDE_PREFIX
                        ? kFlagPrefixesLocked
                        : kFlagSuffixesLocked;
                if ((state(candidate).flags & lock_flag) != 0) return false;
            }
            if (candidate == state_id) return true;
            if (entry_reforge_kernel != nullptr &&
                action_legal(
                    *session_, registry_.actions.at(entry_reforge_action),
                    state(candidate)) &&
                &outcomes(candidate, entry_reforge_action) ==
                    entry_reforge_kernel) {
                return true;
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

        if (option_exit_matches(state(state_id), option)) {
            result->legal = false;
            result->terminates_almost_surely = false;
            return finish();
        }
        std::map<std::uint32_t, double> exits;
        std::map<std::vector<std::uint32_t>, double> choices;
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
            choices[successors] += group.probability;
            if (successors.size() == 1 && successors.front() == kNoId) {
                forced_retry_probability += group.probability;
            }
        }
        for (const OutcomeChoiceOption& choice : attempt.choice_options) {
            result->observation_choice_options.push_back(
                {choice.mod_id, normalize(choice.actual_state),
                 choice.observation_state, choice.actual_state});
        }
        result->expected_resources.assign(
            resources.begin(), resources.end());
        for (const auto& [exit, probability] : exits) {
            result->exits.push_back({exit, probability});
        }
        for (const auto& [successors, probability] : choices) {
            result->observation_choice_groups.push_back(
                {probability, successors});
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

    result->expected_primitive_actions =
        static_cast<double>(option.primitive_program.size());
    result->expected_resources = option.resource_quantities;

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
                frontier.clear();
                break;
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

    if (result->legal && result->supported) {
        for (const auto& [exit_state, probability] : frontier) {
            result->exits.push_back({exit_state, probability});
        }
        if (result->exits.empty()) {
            result->legal = false;
            result->terminates_almost_surely = false;
        }
    }
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
            const AttemptKernel baseline = execute_attempt(
                *this, {option.followup_action}, state_id);
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
    const auto inserted = option_kernel_cache_.emplace(key, std::move(result));
    return *inserted.first->second;
}

} // namespace solver
} // namespace poecraft
