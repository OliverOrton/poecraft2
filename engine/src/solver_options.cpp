#include "solver_internal.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace poecraft {
namespace solver {

namespace {

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

std::vector<FixedOptionSpec> synthesize_automatic_options(
    const SessionImpl& session,
    const GoalSpec& goal,
    const ActionRegistry& registry) {
    std::vector<FixedOptionSpec> result;
    if (!goal.automatic_candidates) return result;

    /* Carrier-exact Fracture candidates reuse every already-approved exact
     * renewal preparation present in the bounded registry. Exact row
     * eligibility later refuses programs that cannot reach the named carrier
     * and a legal Fracture continuation. */
    if (registry.index_by_id.contains("fracture")) {
        std::vector<std::vector<std::string>> preparations;
        for (const ActionDescriptor& action : registry.actions) {
            if (approved_renewal_roll(action) && calc_supports(action)) {
                preparations.push_back({action.id});
            }
        }
        if (registry.index_by_id.contains("scour") &&
            registry.index_by_id.contains("alchemy")) {
            preparations.push_back({"scour", "alchemy"});
        }
        for (std::uint32_t slot = 0; slot < goal.slots.size(); ++slot) {
            for (const auto& preparation : preparations) {
                FixedOptionSpec option;
                option.kind = FixedOptionKind::FracturePrepare;
                option.program_action_ids = preparation;
                option.carrier_goal_slot = slot;
                option.automatic_kind = AutomaticCandidateKind::Fracture;
                option.relevant_goal_mask = 1u << slot;
                result.push_back(std::move(option));
            }
        }
    }

    std::vector<std::uint32_t> ordinary_bench;
    std::vector<std::uint32_t> goal_bench;
    for (std::uint32_t index = 0; index < registry.actions.size(); ++index) {
        const ActionDescriptor& action = registry.actions[index];
        if (action.params.type != ActionType::Bench ||
            action.params.mod_id >= session.metamod_type.size() ||
            session.metamod_type[action.params.mod_id] >= 0) {
            continue;
        }
        ordinary_bench.push_back(index);
        if (goal_mask_for_mod(session, goal, action.params.mod_id) != 0) {
            goal_bench.push_back(index);
        }
    }

    /* Deterministic Multimod finishes are generated only for pairs of legal
     * permanent goal crafts. The fixed kernel retains native group, crafted
     * count, replacement, and open-side legality. */
    if (registry.index_by_id.end() != std::find_if(
            registry.index_by_id.begin(), registry.index_by_id.end(),
            [&](const auto& entry) {
                const ActionDescriptor& action = registry.actions[entry.second];
                return action.params.type == ActionType::Bench &&
                       action.params.mod_id < session.metamod_type.size() &&
                       session.metamod_type[action.params.mod_id] ==
                           session.data->metamod_multimod_code;
            })) {
        for (std::size_t a = 0; a < goal_bench.size(); ++a) {
            for (std::size_t b = a + 1; b < goal_bench.size(); ++b) {
                const ActionDescriptor& left = registry.actions[goal_bench[a]];
                const ActionDescriptor& right = registry.actions[goal_bench[b]];
                const std::uint32_t mask =
                    goal_mask_for_mod(session, goal, left.params.mod_id) |
                    goal_mask_for_mod(session, goal, right.params.mod_id);
                if ((mask & (mask - 1)) == 0 ||
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
    if (cleanup != registry.index_by_id.end()) {
        for (const std::uint32_t blocker_index : ordinary_bench) {
            const ActionDescriptor& blocker = registry.actions[blocker_index];
            if (goal_mask_for_mod(
                    session, goal, blocker.params.mod_id) != 0) {
                continue; /* permanent goal craft, never temporary */
            }
            for (const ActionDescriptor& followup : registry.actions) {
                if (!temporary_followup(followup)) continue;
                for (std::uint32_t slot = 0; slot < goal.slots.size(); ++slot) {
                    FixedOptionSpec option;
                    option.kind = FixedOptionKind::TemporaryBenchRepeat;
                    option.setup_action_ids = {blocker.id};
                    option.action_id = followup.id;
                    option.exit_goal_slots = {slot};
                    option.exit_min_satisfied = 1;
                    option.automatic_kind =
                        AutomaticCandidateKind::TemporaryBenchBlocker;
                    option.relevant_goal_mask = 1u << slot;
                    result.push_back(std::move(option));
                }
            }
        }
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
                if ((protected_mask & target) != 0) continue;
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
    return result;
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
    if (spec.program_action_ids.empty() ||
        spec.program_action_ids.size() > 3) {
        throw std::runtime_error(
            "fixed option: imprint retry program must contain 1-3 actions");
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
    const std::vector<FixedOptionSpec> automatic =
        synthesize_automatic_options(session, goal, registry);
    operators.reserve(
        registry.actions.size() + goal.fixed_options.size() +
        automatic.size());
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
        if (goal.automatic_candidates &&
            action.params.type == ActionType::Bench &&
            action.params.mod_id < session.mod_count) {
            primitive.relevant_goal_mask =
                goal_mask_for_mod(session, goal, action.params.mod_id);
            if (primitive.relevant_goal_mask != 0) {
                primitive.automatic_kind =
                    AutomaticCandidateKind::PermanentBench;
            }
        }
        operators.push_back(std::move(primitive));
    }

    std::set<std::string> option_ids;
    std::vector<FixedOptionSpec> specs = goal.fixed_options;
    specs.insert(specs.end(), automatic.begin(), automatic.end());
    for (const FixedOptionSpec& spec : specs) {
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
            if (goal.rarity != PC_RARITY_MAGIC ||
                spec.exit_goal_slots.size() != goal.slots.size() ||
                spec.exit_min_satisfied != goal.required_satisfied_slots()) {
                throw std::runtime_error(
                    "fixed option: imprint retry exit must be the complete "
                    "magic-item solve goal");
            }
            for (std::uint32_t slot = 0; slot < goal.slots.size(); ++slot) {
                if (spec.exit_goal_slots[slot] != slot) {
                    throw std::runtime_error(
                        "fixed option: imprint retry must name every goal "
                        "slot in order");
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
            const auto inserted = option_kernel_cache_.emplace(
                key, std::move(result));
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
            const AbstractState& entry = state(state_id);
            const BestiaryActionDescriptor& create =
                session_->data->bestiary_actions.at(
                    option.bestiary_create_action);
            const std::uint8_t rarity_bit =
                entry.rarity < 8
                    ? static_cast<std::uint8_t>(1u << entry.rarity)
                    : 0;
            const bool forbidden =
                ((create.forbidden_item_flags & PC_ITEM_CORRUPTED) != 0 &&
                 (entry.flags & kFlagCorrupted) != 0) ||
                ((create.forbidden_item_flags & PC_ITEM_MIRRORED) != 0 &&
                 (entry.flags & kFlagMirrored) != 0);
            if ((create.rarity_mask & rarity_bit) == 0 || forbidden) {
                result->legal = false;
                result->terminates_almost_surely = false;
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
            const AbstractState& entry = state(state_id);
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
            const AbstractState& entry = state(state_id);
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
                if (is_goal_state(state(outcome.state))) {
                    exits[outcome.state] += outcome.probability;
                } else {
                    exits[state_id] += outcome.probability;
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
                        exits[state_id] += prepared.probability;
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
            const std::uint32_t successor = retry ? state_id : actual;
            normalized.emplace(actual, successor);
            if (retry) result->retry_states.push_back(actual);
            return successor;
        };
        double forced_retry_probability = 0.0;
        for (const OutcomeEntry& outcome : attempt.entries) {
            const std::uint32_t successor = normalize(outcome.state);
            exits[successor] += outcome.probability;
            if (successor == state_id) {
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
            if (successors.size() == 1 && successors.front() == state_id) {
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
        const AbstractState& entry = state(state_id);
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
