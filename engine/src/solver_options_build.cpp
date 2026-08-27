#include "solver_options_helpers.hpp"

namespace poecraft {
namespace solver {

namespace {

bool named_action_disabled(
    const GoalSpec& goal,
    const ActionRegistry& registry,
    const std::string& id) {
    if (id.empty()) return false;
    const auto found = registry.index_by_id.find(id);
    return found != registry.index_by_id.end() &&
        solver_action_disabled(goal, registry.actions.at(found->second));
}

bool fixed_option_disabled(
    const GoalSpec& goal,
    const ActionRegistry& registry,
    const FixedOptionSpec& spec) {
    if (solver_automatic_candidate_disabled(goal, spec.automatic_kind)) {
        return true;
    }
    const auto disabled = [&](const SolverActionFamily family) {
        return solver_action_family_disabled(goal, family);
    };
    switch (spec.kind) {
    case FixedOptionKind::ScourAlchemy:
        if (disabled(SolverActionFamily::Currency)) return true;
        break;
    case FixedOptionKind::EldritchSideIntent:
        if (disabled(SolverActionFamily::Eldritch)) return true;
        break;
    case FixedOptionKind::ProtectedSide:
    case FixedOptionKind::ProtectedRepeat:
    case FixedOptionKind::MultimodFinish:
        if (disabled(SolverActionFamily::Metamod) ||
            disabled(SolverActionFamily::Bench)) {
            return true;
        }
        break;
    case FixedOptionKind::FracturePrepare:
        if (disabled(SolverActionFamily::Fracture)) return true;
        break;
    case FixedOptionKind::ImprintRetry:
        if (disabled(SolverActionFamily::Imprint)) return true;
        break;
    case FixedOptionKind::TemporaryBenchRepeat:
        if (disabled(SolverActionFamily::TemporaryBench) ||
            disabled(SolverActionFamily::Bench)) {
            return true;
        }
        break;
    case FixedOptionKind::Renewal:
        break;
    }
    if (named_action_disabled(goal, registry, spec.action_id) ||
        named_action_disabled(
            goal, registry, spec.constructive_finish_action_id)) {
        return true;
    }
    const auto any_disabled = [&](const std::vector<std::string>& ids) {
        return std::any_of(
            ids.begin(), ids.end(),
            [&](const std::string& id) {
                return named_action_disabled(goal, registry, id);
            });
    };
    return any_disabled(spec.setup_action_ids) ||
        any_disabled(spec.bench_craft_ids) ||
        any_disabled(spec.program_action_ids);
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
        if (goal.automatic_candidates &&
            !solver_action_disabled(goal, action)) {
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

    std::erase_if(
        option_specs,
        [&](const FixedOptionSpec& spec) {
            return fixed_option_disabled(goal, registry, spec);
        });

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
                    (craft.params.type == ActionType::EldritchExalt
                         ? "Exalt "
                         : craft.params.type == ActionType::EldritchAnnul
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
                spec.exit_min_satisfied != 1 ||
                (!spec.program_action_ids.empty() &&
                 spec.program_action_ids !=
                     std::vector<std::string>{
                         "remove_crafted_modifiers"})) {
                throw std::runtime_error(
                    "fixed option: temporary bench repeat needs one blocker "
                    "and one exact goal-slot exit");
            }
            std::uint32_t blocker_index = kNoId;
            const ActionDescriptor& blocker = require_action(
                registry, spec.setup_action_ids.front(), blocker_index);
            const int blocker_metamod =
                blocker.params.mod_id < session.metamod_type.size()
                    ? session.metamod_type[blocker.params.mod_id]
                    : -2;
            const bool supported_pool_blocker =
                blocker_metamod == session.data->metamod_no_attack_code ||
                blocker_metamod == session.data->metamod_no_caster_code;
            if (blocker.params.type != ActionType::Bench ||
                blocker.params.mod_id >= session.metamod_type.size() ||
                (blocker_metamod >= 0 && !supported_pool_blocker)) {
                throw std::runtime_error(
                    "fixed option: temporary blocker must be an ordinary "
                    "bench craft or supported Cannot Roll metamod");
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
            option.primitive_program = spec.program_action_ids.empty()
                ? std::vector<std::uint32_t>{
                      blocker_index, followup_index, cleanup_index}
                : std::vector<std::uint32_t>{
                      cleanup_index, blocker_index, followup_index,
                      cleanup_index};
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


} // namespace solver
} // namespace poecraft
