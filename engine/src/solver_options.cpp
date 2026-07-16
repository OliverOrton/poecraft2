#include "solver_internal.hpp"

#include <algorithm>
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
    operators.reserve(registry.actions.size() + goal.fixed_options.size());
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
            break;
        }
        }

        if (!option_ids.insert(option.id).second) {
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

    if (option.option_kind == FixedOptionKind::Renewal ||
        option.option_kind == FixedOptionKind::ProtectedRepeat ||
        option.option_kind == FixedOptionKind::FracturePrepare) {
        const auto finish = [&]() -> const OptionKernel& {
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
            return finish();
        }
        if (option.option_kind == FixedOptionKind::ProtectedRepeat) {
            const std::uint32_t lock_flag =
                option.intended_side == PC_SIDE_PREFIX
                    ? kFlagPrefixesLocked
                    : kFlagSuffixesLocked;
            if ((state(state_id).flags & lock_flag) != 0) {
                result->legal = false;
                result->terminates_almost_surely = false;
                return finish();
            }
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
                return finish();
            }
            if ((entry.flags &
                 (kFlagInfluenced | kFlagSynthesised | kFlagFractured)) !=
                0) {
                result->legal = false;
                result->terminates_almost_surely = false;
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
            return finish();
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
                            !result->exits.empty();
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
    const auto inserted = option_kernel_cache_.emplace(key, std::move(result));
    return *inserted.first->second;
}

} // namespace solver
} // namespace poecraft
