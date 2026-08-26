#include "poecraft/solver.h"
#include "poecraft/bitset.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "handles_internal.hpp"
#include "json.hpp"
#include "solver_internal.hpp"
#include "solver_diagnostic_options.hpp"

/*
 * C ABI for the solver/calculation engine. Thin translation layer: goal
 * JSON in, CalcContext + solve results behind an opaque handle, buffers
 * out. All crafting math lives in solver_*.cpp.
 */
namespace {

using poecraft::json::Parser;
using poecraft::json::Type;
using poecraft::json::Value;
namespace solver = poecraft::solver;

void set_error(pc_error_info* error, pc_result code, const char* message) {
    if (error == nullptr) return;
    error->struct_size = static_cast<uint32_t>(sizeof(pc_error_info));
    error->abi_version = PC_ABI_VERSION;
    error->code = static_cast<int32_t>(code);
    std::snprintf(error->message, sizeof(error->message), "%s",
                  message ? message : "");
}

void clear_error(pc_error_info* error) {
    set_error(error, PC_RESULT_OK, "");
}

std::string string_member(const Value& value, const char* key) {
    const Value* found = value.find(key);
    return found != nullptr && found->type == Type::String ? found->string
                                                           : std::string();
}

solver::ActionRegistryBuildOptions registry_build_options(
    const poecraft::SessionImpl& session,
    const char* goal_json,
    std::size_t goal_json_size) {
    const Value root = Parser(goal_json, goal_json_size).parse();
    if (root.type != Type::Object) {
        throw std::runtime_error("goal: root must be an object");
    }
    solver::ActionRegistryBuildOptions options;
    const Value* actions = root.find("actions");
    if (actions != nullptr) {
        if (actions->type != Type::Array) {
            throw std::runtime_error("goal: actions must be an array");
        }
        options.exhaustive_fossils = false;
        for (const Value& entry : actions->array) {
            if (entry.type == Type::String &&
                entry.string.starts_with("fossil:")) {
                options.requested_fossil_action_ids.push_back(entry.string);
            }
        }
    }
    const std::string fossil_mode = string_member(root, "fossil_mode");
    if (!fossil_mode.empty() && fossil_mode != "exhaustive" &&
        fossil_mode != "goal_relevant") {
        throw std::runtime_error(
            "goal: fossil_mode must be exhaustive or goal_relevant");
    }
    if (fossil_mode == "goal_relevant") {
        options.exhaustive_fossils = false;
        options.goal_relevant_fossils = true;
    }
    const std::string action_mode = string_member(root, "action_mode");
    if (!action_mode.empty() && action_mode != "goal_relevant") {
        throw std::runtime_error(
            "goal: action_mode must be goal_relevant when provided");
    }
    options.goal_relevant_actions = action_mode == "goal_relevant";
    /* S8.3 automatic candidates are an ordinary part of the bounded product
     * envelope. Explicit/manual action documents keep their authored S7
     * option set and historical behavior. */
    const Value* automatic_candidates = root.find("automatic_candidates");
    if (automatic_candidates != nullptr &&
        automatic_candidates->type != Type::Bool) {
        throw std::runtime_error(
            "goal: automatic_candidates must be a boolean");
    }
    options.automatic_candidates = options.goal_relevant_actions ||
        (automatic_candidates != nullptr &&
         automatic_candidates->boolean);
    if (options.automatic_candidates) {
        /* Automatic assembly always uses the bounded product registry even
         * when an explicit primitive subset is supplied for an analytic or
         * parity fixture. */
        options.goal_relevant_actions = true;
    }
    const Value* requested = root.find("requested_fossil_actions");
    if (requested != nullptr) {
        if (requested->type != Type::Array) {
            throw std::runtime_error(
                "goal: requested_fossil_actions must be an array");
        }
        for (const Value& entry : requested->array) {
            if (entry.type != Type::String ||
                !entry.string.starts_with("fossil:")) {
                throw std::runtime_error(
                    "goal: requested fossil actions must be fossil ids");
            }
            options.requested_fossil_action_ids.push_back(entry.string);
        }
    }
    const Value* fixed_options = root.find("options");
    if (fixed_options != nullptr && fixed_options->type == Type::Array) {
        for (const Value& option : fixed_options->array) {
            if (option.type != Type::Object) continue;
            const std::string type = string_member(option, "type");
            if (type == "imprint_retry") {
                throw std::runtime_error(
                    "goal: imprint_retry is discovered automatically and "
                    "cannot be user-authored");
            }
            const std::string action = string_member(option, "action");
            if (!action.empty()) {
                options.option_dependency_action_ids.push_back(action);
            }
            if (action.starts_with("fossil:")) {
                options.requested_fossil_action_ids.push_back(action);
            }
            for (const char* key : {
                     "setup", "bench_crafts", "actions", "preparation"}) {
                const Value* program = option.find(key);
                if (program == nullptr || program->type != Type::Array) {
                    continue;
                }
                for (const Value& step : program->array) {
                    if (step.type == Type::String) {
                        options.option_dependency_action_ids.push_back(
                            step.string);
                        if (step.string.starts_with("fossil:")) {
                            options.requested_fossil_action_ids.push_back(
                                step.string);
                        }
                    }
                }
            }
            if (type == "protected_side" ||
                type == "protected_repeat") {
                const std::string side = string_member(option, "side");
                options.needs_prefix_lock |= side == "prefix";
                options.needs_suffix_lock |= side == "suffix";
            } else if (type == "multimod_finish") {
                options.needs_multimod = true;
            } else if (type == "fracture_prepare") {
                options.needs_fracture = true;
            }
        }
    }
    if (options.goal_relevant_fossils || options.goal_relevant_actions) {
        const poecraft::DataImpl& data = *session.data;
        const Value* slots = root.find("slots");
        if (slots == nullptr || slots->type != Type::Array ||
            slots->array.empty()) {
            throw std::runtime_error(
                "goal: slots must be a non-empty array");
        }
        for (const Value& entry : slots->array) {
            if (entry.type != Type::Object) {
                throw std::runtime_error("goal: slot must be an object");
            }
            const std::string group = string_member(entry, "group");
            const std::string family =
                string_member(entry, "family_mod_key");
            std::uint32_t group_id = solver::kNoId;
            std::uint32_t family_id = solver::kNoId;
            if (!group.empty()) {
                const auto found = data.group_id_by_key.find(group);
                if (found == data.group_id_by_key.end()) {
                    throw std::runtime_error(
                        "goal: unknown group: " + group);
                }
                group_id = found->second;
            } else if (!family.empty()) {
                const auto position = data.mod_pos_by_key.find(family);
                if (position == data.mod_pos_by_key.end()) {
                    throw std::runtime_error(
                        "goal: unknown modifier key: " + family);
                }
                const auto session_mod = session.session_id_by_global_id.find(
                    data.mod_global_ids[position->second]);
                if (session_mod == session.session_id_by_global_id.end()) {
                    throw std::runtime_error(
                        "goal: modifier is not in this session: " + family);
                }
                family_id = session.family_id[session_mod->second];
            } else {
                throw std::runtime_error(
                    "goal: slot needs group or family_mod_key");
            }
            std::uint32_t min_tier = 0;
            const Value* tier = entry.find("min_tier");
            if (tier != nullptr && tier->type == Type::Number &&
                tier->number >= 0) {
                min_tier = static_cast<std::uint32_t>(tier->number);
            }
            std::vector<std::uint32_t> satisfying;
            for (std::uint32_t mod = 0; mod < session.mod_count; ++mod) {
                const bool member =
                    group_id != solver::kNoId
                        ? group_id < session.group_masks.size() &&
                              !session.group_masks[group_id].empty() &&
                              poecraft::pc_bitset_test(
                                  session.group_masks[group_id].data(), mod)
                        : session.family_id[mod] == family_id;
                const std::uint32_t mod_tier =
                    session.family_tier_index[mod];
                if (member &&
                    (session.gen_type[mod] == 0 ||
                     session.gen_type[mod] == 1) &&
                    (min_tier == 0 ||
                     (mod_tier != 0 && mod_tier <= min_tier))) {
                    satisfying.push_back(mod);
                }
            }
            if (satisfying.empty()) {
                throw std::runtime_error(
                    "goal: fossil relevance slot has no satisfying mods");
            }
            options.fossil_goal_mod_ids.push_back(std::move(satisfying));
        }
        const Value* minimum = root.find("min_satisfied_slots");
        options.required_satisfied_slots =
            minimum != nullptr && minimum->type == Type::Number &&
                    minimum->number >= 1
                ? static_cast<std::uint32_t>(minimum->number)
                : static_cast<std::uint32_t>(
                      options.fossil_goal_mod_ids.size());
    }
    return options;
}

solver::GoalSpec parse_goal(
    const poecraft::SessionImpl& session,
    const char* goal_json,
    std::size_t goal_json_size,
    std::vector<std::uint32_t>& out_candidates,
    const solver::ActionRegistry& registry) {
    const poecraft::DataImpl& data = *session.data;
    Value root = Parser(goal_json, goal_json_size).parse();
    if (root.type != Type::Object) {
        throw std::runtime_error("goal: root must be an object");
    }
    const std::string version = string_member(root, "version");
    if (!version.empty() && version != "v1") {
        throw std::runtime_error("goal: version must be v1");
    }

    solver::GoalSpec goal;
    const Value* automatic_candidates = root.find("automatic_candidates");
    if (automatic_candidates != nullptr &&
        automatic_candidates->type != Type::Bool) {
        throw std::runtime_error(
            "goal: automatic_candidates must be a boolean");
    }
    goal.automatic_candidates =
        string_member(root, "action_mode") == "goal_relevant" ||
        (automatic_candidates != nullptr && automatic_candidates->boolean);
    const std::string rarity = string_member(root, "rarity");
    if (rarity == "normal") {
        goal.rarity = PC_RARITY_NORMAL;
    } else if (rarity == "magic") {
        goal.rarity = PC_RARITY_MAGIC;
    } else if (rarity == "rare" || rarity.empty()) {
        goal.rarity = PC_RARITY_RARE;
    } else {
        throw std::runtime_error("goal: unknown rarity: " + rarity);
    }

    const Value* slots = root.find("slots");
    if (slots == nullptr || slots->type != Type::Array ||
        slots->array.empty()) {
        throw std::runtime_error("goal: slots must be a non-empty array");
    }
    for (const Value& entry : slots->array) {
        if (entry.type != Type::Object) {
            throw std::runtime_error("goal: slot must be an object");
        }
        solver::GoalSlot slot;
        const std::string group = string_member(entry, "group");
        const std::string family = string_member(entry, "family_mod_key");
        if (!group.empty()) {
            const auto it = data.group_id_by_key.find(group);
            if (it == data.group_id_by_key.end()) {
                throw std::runtime_error("goal: unknown group: " + group);
            }
            slot.group_id = it->second;
        } else if (!family.empty()) {
            const auto pos = data.mod_pos_by_key.find(family);
            if (pos == data.mod_pos_by_key.end()) {
                throw std::runtime_error(
                    "goal: unknown modifier key: " + family);
            }
            const auto session_mod = session.session_id_by_global_id.find(
                data.mod_global_ids[pos->second]);
            if (session_mod == session.session_id_by_global_id.end()) {
                throw std::runtime_error(
                    "goal: modifier is not in this session: " + family);
            }
            slot.family_id = session.family_id[session_mod->second];
        } else {
            throw std::runtime_error(
                "goal: slot needs group or family_mod_key");
        }
        const Value* tier = entry.find("min_tier");
        if (tier != nullptr && tier->type == Type::Number) {
            if (tier->number < 0) {
                throw std::runtime_error("goal: min_tier must be >= 0");
            }
            slot.min_tier = static_cast<std::uint32_t>(tier->number);
        }
        goal.slots.push_back(slot);
    }

    const Value* min_satisfied = root.find("min_satisfied_slots");
    if (min_satisfied != nullptr) {
        if (min_satisfied->type != Type::Number ||
            min_satisfied->number < 1 ||
            min_satisfied->number > goal.slots.size() ||
            min_satisfied->number !=
                static_cast<double>(
                    static_cast<std::uint32_t>(min_satisfied->number))) {
            throw std::runtime_error(
                "goal: min_satisfied_slots must be an integer from 1 to " +
                std::to_string(goal.slots.size()));
        }
        goal.min_satisfied_slots =
            static_cast<std::uint32_t>(min_satisfied->number);
    } else {
        goal.min_satisfied_slots =
            static_cast<std::uint32_t>(goal.slots.size());
    }

    const Value* actions = root.find("actions");
    if (actions != nullptr) {
        goal.primitive_actions_explicit = true;
        if (actions->type != Type::Array) {
            throw std::runtime_error("goal: actions must be an array");
        }
        for (const Value& entry : actions->array) {
            if (entry.type != Type::String) {
                throw std::runtime_error("goal: action ids must be strings");
            }
            const auto it = registry.index_by_id.find(entry.string);
            if (it == registry.index_by_id.end()) {
                throw std::runtime_error(
                    "goal: unknown action: " + entry.string);
            }
            if (registry.actions[it->second].product_role ==
                solver::ProductActionRole::AutomaticDependency) {
                throw std::runtime_error(
                    "goal: action is automatic-option dependency-only: " +
                    entry.string);
            }
            out_candidates.push_back(it->second);
        }
    }

    const Value* fixed_options = root.find("options");
    if (fixed_options != nullptr) {
        if (fixed_options->type != Type::Array) {
            throw std::runtime_error("goal: options must be an array");
        }
        const auto string_array = [](const Value& object, const char* key,
                                     bool required) {
            std::vector<std::string> values;
            const Value* found = object.find(key);
            if (found == nullptr) {
                if (required) {
                    throw std::runtime_error(
                        std::string("goal: option needs ") + key);
                }
                return values;
            }
            if (found->type != Type::Array) {
                throw std::runtime_error(
                    std::string("goal: option ") + key +
                    " must be an array");
            }
            for (const Value& entry : found->array) {
                if (entry.type != Type::String) {
                    throw std::runtime_error(
                        std::string("goal: option ") + key +
                        " entries must be action ids");
                }
                values.push_back(entry.string);
            }
            return values;
        };
        const auto parse_exit = [&](const Value& option,
                                    solver::FixedOptionSpec& spec) {
            const Value* exit = option.find("until");
            if (exit == nullptr || exit->type != Type::Object) {
                throw std::runtime_error(
                    "goal: repeat option needs an until object");
            }
            const Value* slots = exit->find("goal_slots");
            if (slots == nullptr || slots->type != Type::Array ||
                slots->array.empty()) {
                throw std::runtime_error(
                    "goal: option until.goal_slots must be a non-empty "
                    "array");
            }
            for (const Value& slot : slots->array) {
                if (slot.type != Type::Number || slot.number < 0 ||
                    slot.number != static_cast<double>(
                        static_cast<std::uint32_t>(slot.number)) ||
                    slot.number >= goal.slots.size()) {
                    throw std::runtime_error(
                        "goal: option until.goal_slots entries must be "
                        "valid integer goal-slot indices");
                }
                const auto index =
                    static_cast<std::uint32_t>(slot.number);
                if (std::find(
                        spec.exit_goal_slots.begin(),
                        spec.exit_goal_slots.end(), index) !=
                    spec.exit_goal_slots.end()) {
                    throw std::runtime_error(
                        "goal: option until.goal_slots contains a duplicate");
                }
                spec.exit_goal_slots.push_back(index);
            }
            std::sort(
                spec.exit_goal_slots.begin(),
                spec.exit_goal_slots.end());
            const Value* minimum = exit->find("min_satisfied");
            if (minimum == nullptr || minimum->type != Type::Number ||
                minimum->number < 1 ||
                minimum->number > spec.exit_goal_slots.size() ||
                minimum->number != static_cast<double>(
                    static_cast<std::uint32_t>(minimum->number))) {
                throw std::runtime_error(
                    "goal: option until.min_satisfied must be an integer "
                    "from 1 through the selected goal-slot count");
            }
            spec.exit_min_satisfied =
                static_cast<std::uint32_t>(minimum->number);
        };
        for (const Value& entry : fixed_options->array) {
            if (entry.type != Type::Object) {
                throw std::runtime_error(
                    "goal: fixed option must be an object");
            }
            solver::FixedOptionSpec option;
            const std::string type = string_member(entry, "type");
            if (type == "scour_alchemy") {
                option.kind = solver::FixedOptionKind::ScourAlchemy;
            } else if (type == "eldritch_side_intent") {
                option.kind = solver::FixedOptionKind::EldritchSideIntent;
            } else if (type == "protected_side") {
                option.kind = solver::FixedOptionKind::ProtectedSide;
            } else if (type == "multimod_finish") {
                option.kind = solver::FixedOptionKind::MultimodFinish;
            } else if (type == "renewal") {
                option.kind = solver::FixedOptionKind::Renewal;
            } else if (type == "protected_repeat") {
                option.kind = solver::FixedOptionKind::ProtectedRepeat;
            } else if (type == "fracture_prepare") {
                option.kind = solver::FixedOptionKind::FracturePrepare;
            } else if (type == "imprint_retry") {
                throw std::runtime_error(
                    "goal: imprint_retry is discovered automatically and "
                    "cannot be user-authored");
            } else {
                throw std::runtime_error(
                    "goal: unknown fixed option type: " + type);
            }

            if (option.kind == solver::FixedOptionKind::EldritchSideIntent ||
                option.kind == solver::FixedOptionKind::ProtectedSide ||
                option.kind == solver::FixedOptionKind::ProtectedRepeat) {
                const std::string side = string_member(entry, "side");
                if (side == "prefix") {
                    option.side = PC_SIDE_PREFIX;
                } else if (side == "suffix") {
                    option.side = PC_SIDE_SUFFIX;
                } else {
                    throw std::runtime_error(
                        "goal: side-specific option needs side prefix or "
                        "suffix");
                }
                option.action_id = string_member(entry, "action");
                if (option.action_id.empty()) {
                    throw std::runtime_error(
                        "goal: side-specific option needs action");
                }
            }
            if (option.kind == solver::FixedOptionKind::EldritchSideIntent) {
                option.setup_action_ids =
                    string_array(entry, "setup", true);
            } else if (option.kind ==
                       solver::FixedOptionKind::MultimodFinish) {
                option.bench_craft_ids =
                    string_array(entry, "bench_crafts", true);
            } else if (option.kind == solver::FixedOptionKind::Renewal) {
                option.program_action_ids =
                    string_array(entry, "actions", true);
                parse_exit(entry, option);
            } else if (option.kind ==
                       solver::FixedOptionKind::ProtectedRepeat) {
                parse_exit(entry, option);
            } else if (option.kind ==
                       solver::FixedOptionKind::FracturePrepare) {
                option.program_action_ids =
                    string_array(entry, "preparation", true);
                const Value* carrier = entry.find("carrier_goal_slot");
                if (carrier == nullptr || carrier->type != Type::Number ||
                    carrier->number < 0 ||
                    carrier->number >= goal.slots.size() ||
                    carrier->number != static_cast<double>(
                        static_cast<std::uint32_t>(carrier->number))) {
                    throw std::runtime_error(
                        "goal: fracture_prepare.carrier_goal_slot must be "
                        "a valid integer goal-slot index");
                }
                option.carrier_goal_slot =
                    static_cast<std::uint32_t>(carrier->number);
            }
            goal.fixed_options.push_back(std::move(option));
        }
    }
    return goal;
}

} // namespace

struct pc_solver {
    std::shared_ptr<const poecraft::SessionImpl> session;
    std::unique_ptr<solver::CalcContext> calc;
    /* Product solving may use a deliberately coarse parent abstraction.
     * Calculator queries remain strict and lazily allocate their own exact
     * context so primitive semantics and outcome identities do not change. */
    std::unique_ptr<solver::CalcContext> exact_calc;
    std::unique_ptr<solver::SolveWork> solve_work;
    std::optional<solver::SolveResult> solved;
    std::optional<std::uint64_t> registry_generation_ns;
    std::optional<solver::PolicyCompilationTelemetry> compilation;
    std::string compiled_strategy; /* scratch for the buffer queries */
    std::string solve_log;
    std::string abandoned_telemetry;
    bool abandoned_telemetry_capped = false;
    std::uint64_t abandoned_telemetry_limit = 0;
    std::uint64_t peak_owned_bytes = 0;
};

namespace {

solver::SolveOptions solve_options(const pc_solve_options* options) {
    solver::SolveOptions value;
    if (options == nullptr) return value;
    if (options->abi_version != PC_ABI_VERSION ||
        options->struct_size <
            offsetof(pc_solve_options, max_sweeps) +
                sizeof(options->max_sweeps)) {
        throw std::invalid_argument("invalid solve options ABI");
    }
#define PC_SOLVE_OPTION_HAS(field)                                      \
    (options->struct_size >=                                            \
     offsetof(pc_solve_options, field) + sizeof(options->field))
    if (options->epsilon > 0.0) value.epsilon = options->epsilon;
    if (options->max_states != 0) value.max_states = options->max_states;
    if (options->max_sweeps != 0) value.max_sweeps = options->max_sweeps;
    if (PC_SOLVE_OPTION_HAS(max_discovered_states) &&
        options->max_discovered_states != 0) {
        value.max_discovered_states = options->max_discovered_states;
    } else if (options->max_states != 0) {
        value.max_discovered_states = options->max_states;
    }
    if (PC_SOLVE_OPTION_HAS(max_expanded_states) &&
        options->max_expanded_states != 0) {
        value.max_expanded_states = options->max_expanded_states;
    } else if (options->max_states != 0) {
        value.max_expanded_states = options->max_states;
    }
    if (PC_SOLVE_OPTION_HAS(max_state_action_rows) &&
        options->max_state_action_rows != 0) {
        value.max_state_action_rows = options->max_state_action_rows;
    }
    if (PC_SOLVE_OPTION_HAS(max_transitions) &&
        options->max_transitions != 0) {
        value.max_transitions = options->max_transitions;
    }
    if (PC_SOLVE_OPTION_HAS(max_reforge_work) &&
        options->max_reforge_work != 0) {
        value.max_reforge_work = options->max_reforge_work;
    }
    if (PC_SOLVE_OPTION_HAS(max_solver_owned_bytes) &&
        options->max_solver_owned_bytes != 0) {
        value.max_solver_owned_bytes = options->max_solver_owned_bytes;
    }
    if (PC_SOLVE_OPTION_HAS(max_compiled_nodes) &&
        options->max_compiled_nodes != 0) {
        value.max_compiled_nodes = options->max_compiled_nodes;
    }
    if (PC_SOLVE_OPTION_HAS(max_compiled_edges) &&
        options->max_compiled_edges != 0) {
        value.max_compiled_edges = options->max_compiled_edges;
    }
    if (PC_SOLVE_OPTION_HAS(max_strategy_json_bytes) &&
        options->max_strategy_json_bytes != 0) {
        value.max_strategy_json_bytes = options->max_strategy_json_bytes;
    }
    if (PC_SOLVE_OPTION_HAS(max_diagnostic_samples) &&
        options->max_diagnostic_samples != 0) {
        value.max_diagnostic_samples = options->max_diagnostic_samples;
    }
    if (PC_SOLVE_OPTION_HAS(max_telemetry_json_bytes) &&
        options->max_telemetry_json_bytes != 0) {
        value.max_telemetry_json_bytes = options->max_telemetry_json_bytes;
    }
    if (PC_SOLVE_OPTION_HAS(solver_flags)) {
        value.full_evidence =
            (options->solver_flags & PC_SOLVER_FLAG_FULL_EVIDENCE) != 0;
        value.strict_states =
            (options->solver_flags & PC_SOLVER_FLAG_STRICT_STATES) != 0;
        value.kernel_reuse =
            (options->solver_flags &
             PC_SOLVER_FLAG_DISABLE_KERNEL_REUSE) == 0;
        value.goal_progress_gated_reforges =
            (options->solver_flags &
             PC_SOLVER_FLAG_GOAL_PROGRESS_GATED_REFORGES) != 0;
        value.allow_economic_restart =
            (options->solver_flags &
             PC_SOLVER_FLAG_DISABLE_ECONOMIC_RESTART) == 0;
        value.consider_imprint_programs =
            (options->solver_flags &
             PC_SOLVER_FLAG_DISABLE_IMPRINT_PROGRAMS) == 0;
        value.high_impact_executable_uppers =
            (options->solver_flags &
             solver::kHighImpactExecutableUppersDiagnosticFlag) != 0;
        value.projected_reforge_frontier_diagnostic =
            (options->solver_flags &
             solver::kProjectedReforgeFrontierDiagnosticFlag) != 0;
        value.factored_terminal_reforge_diagnostic =
            (options->solver_flags &
             solver::kFactoredTerminalReforgeDiagnosticFlag) != 0;
        value.reforge_resource_accounting =
            (options->solver_flags &
             solver::kDisableReforgeResourceAccountingDiagnosticFlag) == 0;
        value.raw_strict_reforge_oracle_diagnostic =
            (options->solver_flags &
             solver::kRawStrictReforgeOracleDiagnosticFlag) != 0;
    }
    if (PC_SOLVE_OPTION_HAS(max_absolute_optimality_gap) &&
        options->max_absolute_optimality_gap > 0.0) {
        value.max_absolute_optimality_gap =
            options->max_absolute_optimality_gap;
    }
    if (PC_SOLVE_OPTION_HAS(max_relative_optimality_gap) &&
        options->max_relative_optimality_gap > 0.0) {
        value.max_relative_optimality_gap =
            options->max_relative_optimality_gap;
    }
    if (PC_SOLVE_OPTION_HAS(max_policy_refinement_states) &&
        options->max_policy_refinement_states != 0) {
        value.max_policy_refinement_states =
            options->max_policy_refinement_states;
    }
    return value;
#undef PC_SOLVE_OPTION_HAS
}

std::unordered_map<std::string, double> economy_prices(
    const pc_economy_handle economy) {
    return std::unordered_map<std::string, double>(
        economy->impl->prices.begin(), economy->impl->prices.end());
}

int32_t solve_phase(const solver::SolvePhase phase) {
    switch (phase) {
    case solver::SolvePhase::Expanding: return PC_SOLVE_PHASE_EXPANDING;
    case solver::SolvePhase::Iterating: return PC_SOLVE_PHASE_ITERATING;
    case solver::SolvePhase::Done: return PC_SOLVE_PHASE_DONE;
    case solver::SolvePhase::Refining: return PC_SOLVE_PHASE_REFINING;
    case solver::SolvePhase::Compiling: return PC_SOLVE_PHASE_COMPILING;
    case solver::SolvePhase::Certifying: return PC_SOLVE_PHASE_CERTIFYING;
    }
    return PC_SOLVE_PHASE_DONE;
}

int32_t solve_policy_status(const solver::SolvePolicyStatus status) {
    switch (status) {
    case solver::SolvePolicyStatus::None:
        return PC_SOLVE_POLICY_NONE;
    case solver::SolvePolicyStatus::BoundedFeasible:
        return PC_SOLVE_POLICY_BOUNDED_FEASIBLE;
    case solver::SolvePolicyStatus::BoundedNearOptimal:
        return PC_SOLVE_POLICY_BOUNDED_NEAR_OPTIMAL;
    case solver::SolvePolicyStatus::Exact:
        return PC_SOLVE_POLICY_EXACT;
    }
    return PC_SOLVE_POLICY_NONE;
}

int32_t solve_termination(const solver::SolveTermination termination) {
    switch (termination) {
    case solver::SolveTermination::None:
        return PC_SOLVE_TERMINATION_NONE;
    case solver::SolveTermination::RefusedResourceCap:
        return PC_SOLVE_TERMINATION_REFUSED_RESOURCE_CAP;
    case solver::SolveTermination::TargetGap:
        return PC_SOLVE_TERMINATION_TARGET_GAP;
    case solver::SolveTermination::ExactClosed:
        return PC_SOLVE_TERMINATION_EXACT_CLOSED;
    case solver::SolveTermination::NoExecutablePolicy:
        return PC_SOLVE_TERMINATION_NO_EXECUTABLE_POLICY;
    case solver::SolveTermination::NumericalStability:
        return PC_SOLVE_TERMINATION_NUMERICAL_STABILITY;
    case solver::SolveTermination::RequestedBoundedFinish:
        return PC_SOLVE_TERMINATION_REQUESTED_BOUNDED_FINISH;
    }
    return PC_SOLVE_TERMINATION_NONE;
}

uint32_t solve_cap_hit_mask(const solver::SolveDiagnostics& diagnostics) {
    uint32_t mask = 0;
    for (const std::string& cap : diagnostics.cap_hits) {
        if (cap == "max_states" || cap == "max_discovered_states" ||
            cap == "max_expanded_states") {
            mask |= PC_SOLVE_CAP_STATE;
        } else if (cap == "max_transitions") {
            mask |= PC_SOLVE_CAP_TRANSITIONS;
        } else if (cap == "max_solver_owned_bytes" ||
                   cap == "max_owned_bytes" ||
                   cap == "max_estimated_memory_bytes") {
            mask |= PC_SOLVE_CAP_MEMORY;
        } else if (cap == "max_sweeps") {
            mask |= PC_SOLVE_CAP_SWEEPS;
        } else if (cap == "max_reforge_work") {
            mask |= PC_SOLVE_CAP_REFORGE_WORK;
        } else if (cap == "max_state_action_rows") {
            mask |= PC_SOLVE_CAP_STATE_ACTION_ROWS;
        } else if (cap == "max_compiled_nodes" ||
                   cap == "max_compiled_edges" ||
                   cap == "max_strategy_json_bytes" ||
                   cap == "max_telemetry_json_bytes") {
            mask |= PC_SOLVE_CAP_COMPILED_OUTPUT;
        } else {
            mask |= PC_SOLVE_CAP_OTHER;
        }
    }
    return mask;
}

int32_t solve_stop_cause(const solver::SolveResult& result) {
    const uint32_t caps = solve_cap_hit_mask(result.diagnostics);
    if ((caps & PC_SOLVE_CAP_STATE) != 0) return PC_SOLVE_STOP_STATE_CAP;
    if ((caps & PC_SOLVE_CAP_TRANSITIONS) != 0) {
        return PC_SOLVE_STOP_TRANSITION_CAP;
    }
    if ((caps & PC_SOLVE_CAP_MEMORY) != 0) return PC_SOLVE_STOP_MEMORY_CAP;
    if ((caps & PC_SOLVE_CAP_SWEEPS) != 0) return PC_SOLVE_STOP_SWEEP_CAP;
    if ((caps & PC_SOLVE_CAP_REFORGE_WORK) != 0) {
        return PC_SOLVE_STOP_REFORGE_WORK_CAP;
    }
    if ((caps & PC_SOLVE_CAP_STATE_ACTION_ROWS) != 0) {
        return PC_SOLVE_STOP_STATE_ACTION_ROW_CAP;
    }
    if ((caps & PC_SOLVE_CAP_COMPILED_OUTPUT) != 0) {
        return PC_SOLVE_STOP_COMPILED_OUTPUT_CAP;
    }
    if (caps != 0) return PC_SOLVE_STOP_OTHER_RESOURCE_CAP;
    switch (result.termination) {
    case solver::SolveTermination::ExactClosed:
        return PC_SOLVE_STOP_EXACT_CLOSED;
    case solver::SolveTermination::TargetGap:
        return PC_SOLVE_STOP_TARGET_GAP;
    case solver::SolveTermination::NoExecutablePolicy:
        return PC_SOLVE_STOP_NO_EXECUTABLE_POLICY;
    case solver::SolveTermination::NumericalStability:
        return PC_SOLVE_STOP_NUMERICAL_STABILITY;
    case solver::SolveTermination::RequestedBoundedFinish:
        return PC_SOLVE_STOP_REQUESTED_BOUNDED_FINISH;
    case solver::SolveTermination::RefusedResourceCap:
        return PC_SOLVE_STOP_OTHER_RESOURCE_CAP;
    case solver::SolveTermination::None:
        return PC_SOLVE_STOP_NONE;
    }
    return PC_SOLVE_STOP_NONE;
}

int32_t solve_gap_target(const solver::SolveGapTarget target) {
    switch (target) {
    case solver::SolveGapTarget::None:
        return PC_SOLVE_GAP_TARGET_NONE;
    case solver::SolveGapTarget::Absolute:
        return PC_SOLVE_GAP_TARGET_ABSOLUTE;
    case solver::SolveGapTarget::Relative:
        return PC_SOLVE_GAP_TARGET_RELATIVE;
    case solver::SolveGapTarget::Both:
        return PC_SOLVE_GAP_TARGET_BOTH;
    }
    return PC_SOLVE_GAP_TARGET_NONE;
}

int32_t solve_incumbent_kind(const std::string& kind) {
    if (kind.empty()) return PC_SOLVE_INCUMBENT_NONE;
    if (kind == "constructive_fallback") {
        return PC_SOLVE_INCUMBENT_CONSTRUCTIVE_FALLBACK;
    }
    if (kind == "progressive_fracture") {
        return PC_SOLVE_INCUMBENT_PROGRESSIVE_FRACTURE;
    }
    if (kind == "destructive_renewal") {
        return PC_SOLVE_INCUMBENT_DESTRUCTIVE_RENEWAL;
    }
    if (kind == "direct_executable_row") {
        return PC_SOLVE_INCUMBENT_DIRECT_EXECUTABLE_ROW;
    }
    if (kind == "partial_upper_plus_fallback") {
        return PC_SOLVE_INCUMBENT_PARTIAL_UPPER_FALLBACK;
    }
    if (kind == "partial_upper_plus_progressive_fracture") {
        return PC_SOLVE_INCUMBENT_PARTIAL_UPPER_PROGRESSIVE_FRACTURE;
    }
    if (kind == "partial_upper_plus_destructive_renewal") {
        return PC_SOLVE_INCUMBENT_PARTIAL_UPPER_DESTRUCTIVE_RENEWAL;
    }
    return PC_SOLVE_INCUMBENT_OTHER;
}

void copy_solve_progress(
    const solver::SolveProgress& source,
    pc_solve_progress& target) {
    target = {};
    target.struct_size = sizeof(target);
    target.abi_version = PC_ABI_VERSION;
    target.phase = solve_phase(source.phase);
    target.done = source.done ? 1 : 0;
    target.expanded_states = source.expanded_states;
    target.sweeps = source.sweeps;
    target.residual = source.residual;
    target.start_value_bound = source.start_value_bound;
    target.lower_bound = source.lower_bound;
    target.upper_bound = source.upper_bound;
    target.absolute_optimality_gap = source.absolute_optimality_gap;
    target.relative_optimality_gap = source.relative_optimality_gap;
    target.focused_round = source.focused_round;
    target.incumbent_kind = solve_incumbent_kind(source.incumbent_kind);
    target.discovered_states = source.discovered_states;
    target.frontier_states = source.frontier_states;
    target.state_action_rows = source.state_action_rows;
    target.transition_entries = source.transition_entries;
    target.reforge_work = source.reforge_work;
    target.live_owned_bytes = source.live_owned_bytes;
    target.peak_owned_bytes = source.peak_owned_bytes;
    target.finalization_work_items = source.finalization_work_items;
    target.refinement_states = source.refinement_states;
    target.refinement_kernels = source.refinement_kernels;
    target.refinement_transitions = source.refinement_transitions;
    target.refinement_rounds = source.refinement_rounds;
    target.refinement_classes = source.refinement_classes;
    target.certification_discovered_pairs =
        source.certification_discovered_pairs;
    target.certification_pending_pairs =
        source.certification_pending_pairs;
    target.certification_solved_sccs =
        source.certification_solved_sccs;
    target.certification_total_sccs =
        source.certification_total_sccs;
}

void copy_solve_summary(
    const solver::SolveResult& result,
    pc_solve_summary* out_summary) {
    if (out_summary == nullptr) return;
    *out_summary = {};
    out_summary->struct_size = sizeof(*out_summary);
    out_summary->abi_version = PC_ABI_VERSION;
    out_summary->converged = result.converged ? 1 : 0;
    out_summary->start_state = result.start_state;
    out_summary->start_value = result.values[result.start_state];
    out_summary->expanded_states = result.diagnostics.expanded_states;
    out_summary->sweeps = result.diagnostics.sweeps;
    out_summary->residual = result.diagnostics.residual;
    out_summary->skipped_action_count = static_cast<uint32_t>(
        result.diagnostics.skipped_missing_price_count +
        result.diagnostics.skipped_unsupported_count);
    out_summary->policy_available = result.policy_available ? 1 : 0;
    out_summary->policy_status = solve_policy_status(result.policy_status);
    out_summary->termination = solve_termination(result.termination);
    out_summary->lower_bound = result.lower_bound;
    out_summary->upper_bound = result.upper_bound;
    out_summary->evaluated_policy_cost = result.evaluated_policy_cost;
    out_summary->absolute_optimality_gap =
        result.absolute_optimality_gap;
    out_summary->relative_optimality_gap =
        result.relative_optimality_gap;
    out_summary->requested_absolute_optimality_gap =
        result.requested_absolute_optimality_gap;
    out_summary->requested_relative_optimality_gap =
        result.requested_relative_optimality_gap;
    out_summary->target_met = result.target_met ? 1 : 0;
    out_summary->target_fired = solve_gap_target(result.target_fired);
    out_summary->stop_cause = solve_stop_cause(result);
    out_summary->cap_hit_mask = solve_cap_hit_mask(result.diagnostics);
    out_summary->registry_action_count =
        result.diagnostics.registry_actions;
    out_summary->candidate_action_count =
        result.diagnostics.candidate_actions;
    out_summary->evaluator_supported_action_count =
        result.diagnostics.evaluator_supported_actions;
    out_summary->supported_priced_action_count =
        result.diagnostics.supported_priced_actions;
    out_summary->skipped_missing_price_count = static_cast<uint32_t>(
        std::min<std::uint64_t>(
            result.diagnostics.skipped_missing_price_count,
            std::numeric_limits<uint32_t>::max()));
    out_summary->skipped_unsupported_count = static_cast<uint32_t>(
        std::min<std::uint64_t>(
            result.diagnostics.skipped_unsupported_count,
            std::numeric_limits<uint32_t>::max()));
}

void commit_solve(pc_solver_handle solver, solver::SolveResult result) {
    solver->solved = std::move(result);
    solver->compilation.reset();
    std::string{}.swap(solver->compiled_strategy);
    solver->solve_log.clear();
    solver->abandoned_telemetry.clear();
    solver->abandoned_telemetry_capped = false;
    solver->abandoned_telemetry_limit = 0;
}

} // namespace

pc_result pc_solver_create(
    pc_session_handle session,
    const char* goal_json,
    size_t goal_json_size,
    pc_solver_handle* out_solver,
    pc_error_info* out_error) {
    if (session == nullptr || goal_json == nullptr || out_solver == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    *out_solver = nullptr;
    try {
        auto holder = std::make_unique<pc_solver>();
        holder->session = session->impl;
        const auto registry_started = std::chrono::steady_clock::now();
        const solver::ActionRegistryBuildOptions registry_options =
            registry_build_options(
                *holder->session, goal_json, goal_json_size);
        solver::ActionRegistry registry =
            solver::build_action_registry(*holder->session, registry_options);
        holder->registry_generation_ns = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - registry_started)
                .count());
        std::vector<std::uint32_t> candidates;
        const solver::GoalSpec goal = parse_goal(
            *holder->session, goal_json, goal_json_size, candidates,
            registry);
        holder->calc = std::make_unique<solver::CalcContext>(
            holder->session, goal, std::move(registry), candidates, false,
            !goal.primitive_actions_explicit, false, std::nullopt,
            std::vector<solver::CountObservation>{},
            goal.automatic_candidates);
        *out_solver = holder.release();
        clear_error(out_error);
        return PC_RESULT_OK;
    } catch (const std::exception& ex) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, ex.what());
        return PC_RESULT_INVALID_ARGUMENT;
    }
}

void pc_solver_destroy(pc_solver_handle solver) {
    delete solver;
}

pc_result pc_solver_action_count(
    pc_solver_handle solver,
    uint32_t* out_count,
    pc_error_info* out_error) {
    if (solver == nullptr || out_count == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    *out_count = static_cast<uint32_t>(
        solver->calc->registry().actions.size());
    clear_error(out_error);
    return PC_RESULT_OK;
}

pc_result pc_solver_get_action_info(
    pc_solver_handle solver,
    uint32_t action_index,
    pc_solver_action_info* out_info,
    pc_error_info* out_error) {
    if (solver == nullptr || out_info == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    const auto& actions = solver->calc->registry().actions;
    if (action_index >= actions.size()) {
        set_error(out_error, PC_RESULT_NOT_FOUND, "action index out of range");
        return PC_RESULT_NOT_FOUND;
    }
    const solver::ActionDescriptor& action = actions[action_index];
    static thread_local std::vector<const char*> cost_key_ptrs;
    cost_key_ptrs.clear();
    for (const std::string& key : action.cost_keys) {
        cost_key_ptrs.push_back(key.c_str());
    }
    out_info->struct_size = static_cast<uint32_t>(sizeof(*out_info));
    out_info->abi_version = PC_ABI_VERSION;
    out_info->action_index = action_index;
    out_info->id = action.id.c_str();
    out_info->display_name = action.display_name.c_str();
    out_info->transition_kind = static_cast<int32_t>(action.kind);
    out_info->synthetic = action.synthetic ? 1 : 0;
    out_info->cost_key_count =
        static_cast<uint32_t>(cost_key_ptrs.size());
    out_info->cost_keys = cost_key_ptrs.data();
    out_info->can_preserve = action.preservation.can_preserve;
    out_info->can_destroy = action.preservation.can_destroy;
    out_info->can_create = action.preservation.can_create;
    out_info->can_make_unreachable =
        action.preservation.can_make_unreachable;
    out_info->destructive_renewal =
        action.preservation.destructive_renewal ? 1 : 0;
    out_info->preserves_fractured_affixes =
        action.preservation.preserves_fractured_affixes ? 1 : 0;
    out_info->respects_prefix_lock =
        action.preservation.respects_prefix_lock ? 1 : 0;
    out_info->respects_suffix_lock =
        action.preservation.respects_suffix_lock ? 1 : 0;
    out_info->respects_cannot_roll_attack =
        action.preservation.respects_cannot_roll_attack ? 1 : 0;
    out_info->respects_cannot_roll_caster =
        action.preservation.respects_cannot_roll_caster ? 1 : 0;
    clear_error(out_error);
    return PC_RESULT_OK;
}

pc_result pc_solver_find_action(
    pc_solver_handle solver,
    const char* action_id,
    uint32_t* out_index,
    pc_error_info* out_error) {
    if (solver == nullptr || action_id == nullptr || out_index == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    const auto& index = solver->calc->registry().index_by_id;
    const auto it = index.find(action_id);
    if (it == index.end()) {
        set_error(out_error, PC_RESULT_NOT_FOUND, "unknown action id");
        return PC_RESULT_NOT_FOUND;
    }
    *out_index = it->second;
    clear_error(out_error);
    return PC_RESULT_OK;
}

pc_result pc_solver_candidates(
    pc_solver_handle solver,
    uint32_t* out_indices,
    uint32_t capacity,
    uint32_t* out_count,
    pc_error_info* out_error) {
    if (solver == nullptr || out_count == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    const std::vector<std::uint32_t>& candidates =
        solver->calc->candidates();
    *out_count = static_cast<uint32_t>(candidates.size());
    if (out_indices != nullptr) {
        const uint32_t writable = std::min<uint32_t>(capacity, *out_count);
        for (uint32_t i = 0; i < writable; ++i) {
            out_indices[i] = candidates[i];
        }
    }
    clear_error(out_error);
    return PC_RESULT_OK;
}

pc_result pc_solver_goal_feasibility(
    pc_solver_handle solver,
    const pc_item_state* start_item,
    pc_goal_feasibility* out_feasibility,
    pc_error_info* out_error) {
    if (solver == nullptr || start_item == nullptr ||
        out_feasibility == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    try {
        if (solver->calc->product_solver_parent() &&
            solver->exact_calc == nullptr) {
            solver->exact_calc = std::make_unique<solver::CalcContext>(
                solver->session, solver->calc->goal(),
                solver->calc->registry(), solver->calc->candidates(), false,
                false, true);
        }
        solver::CalcContext& calc =
            solver->exact_calc != nullptr ? *solver->exact_calc
                                          : *solver->calc;
        const poecraft::SessionImpl& session = calc.session();
        const auto& slots = calc.layout().slots;
        const std::size_t required =
            calc.goal().required_satisfied_slots();
        pc_goal_feasibility result{};
        result.struct_size = sizeof(result);
        result.abi_version = PC_ABI_VERSION;
        result.status = PC_GOAL_FEASIBILITY_UNKNOWN;
        result.reason = PC_GOAL_FEASIBILITY_REASON_NONE;
        result.goal_slot_count = static_cast<uint32_t>(slots.size());
        result.required_slot_count = static_cast<uint32_t>(required);
        result.witness_action_index = UINT32_MAX;
        std::fill_n(result.witness_mod_ids,
                    PC_SOLVER_MAX_GOAL_SLOTS, UINT32_MAX);

        auto natural_eligible = [&](const std::uint32_t mod) {
            return mod < session.mod_count &&
                (session.gen_type[mod] == PC_SIDE_PREFIX ||
                 session.gen_type[mod] == PC_SIDE_SUFFIX) &&
                session.required_level[mod] <= session.item_level &&
                poecraft::pc_bitset_test(
                    session.normal_random_roll_mask.data(), mod) &&
                poecraft::pc_bitset_test(
                    session.positive_spawn_weight_mask.data(), mod) &&
                poecraft::pc_bitset_test(
                    session.positive_base_weight_mask.data(), mod) &&
                session.base_roll_weight[mod] > 0;
        };

        for (std::uint32_t mod = 0; mod < session.mod_count; ++mod) {
            if (!natural_eligible(mod)) continue;
            ++result.natural_pool_mod_count;
            result.natural_pool_weight += session.base_roll_weight[mod];
            if (session.gen_type[mod] == PC_SIDE_PREFIX) {
                ++result.natural_prefix_mod_count;
                result.natural_prefix_weight += session.base_roll_weight[mod];
            } else {
                ++result.natural_suffix_mod_count;
                result.natural_suffix_weight += session.base_roll_weight[mod];
            }
        }

        std::vector<std::vector<std::uint32_t>> candidates(slots.size());
        for (std::size_t slot = 0; slot < slots.size(); ++slot) {
            for (std::uint32_t mod = 0; mod < session.mod_count; ++mod) {
                if (!natural_eligible(mod) ||
                    session.family_tier_index[mod] != 1 ||
                    !poecraft::pc_bitset_test(
                        slots[slot].satisfying_mask.data(), mod)) {
                    continue;
                }
                candidates[slot].push_back(mod);
                result.slot_natural_weights[slot] +=
                    session.base_roll_weight[mod];
            }
            result.slot_natural_mod_counts[slot] =
                static_cast<uint32_t>(candidates[slot].size());
            if (!candidates[slot].empty()) ++result.eligible_slot_count;
            if (result.natural_pool_weight != 0) {
                result.slot_single_draw_probabilities[slot] =
                    static_cast<double>(result.slot_natural_weights[slot]) /
                    static_cast<double>(result.natural_pool_weight);
            }
        }

        if (result.eligible_slot_count < required) {
            result.status = PC_GOAL_FEASIBILITY_INFEASIBLE;
            result.reason = PC_GOAL_FEASIBILITY_REASON_NO_NATURAL_T1;
            *out_feasibility = result;
            clear_error(out_error);
            return PC_RESULT_OK;
        }

        const std::uint32_t side_cap = session.rare_affix_cap;
        auto find_assignment = [&](const bool enforce_groups,
                                   std::vector<std::uint32_t>& witness) {
            std::unordered_set<std::uint32_t> occupied_groups;
            std::function<bool(std::size_t, std::size_t, std::uint32_t,
                               std::uint32_t)> visit;
            visit = [&](const std::size_t slot, const std::size_t chosen,
                        const std::uint32_t prefixes,
                        const std::uint32_t suffixes) -> bool {
                if (chosen >= required) return true;
                if (slot >= candidates.size() ||
                    chosen + candidates.size() - slot < required) {
                    return false;
                }
                for (const std::uint32_t mod : candidates[slot]) {
                    const bool prefix =
                        session.gen_type[mod] == PC_SIDE_PREFIX;
                    if ((prefix ? prefixes : suffixes) >= side_cap) continue;
                    std::vector<std::uint32_t> added_groups;
                    bool conflict = false;
                    if (enforce_groups) {
                        for (std::uint32_t index = session.group_offsets[mod];
                             index < session.group_offsets[mod + 1]; ++index) {
                            const std::uint32_t group =
                                session.group_ids[index];
                            if (occupied_groups.contains(group)) {
                                conflict = true;
                                break;
                            }
                            added_groups.push_back(group);
                        }
                    }
                    if (conflict) continue;
                    for (const std::uint32_t group : added_groups) {
                        occupied_groups.insert(group);
                    }
                    witness[slot] = mod;
                    if (visit(slot + 1, chosen + 1,
                              prefixes + (prefix ? 1u : 0u),
                              suffixes + (prefix ? 0u : 1u))) {
                        return true;
                    }
                    witness[slot] = UINT32_MAX;
                    for (const std::uint32_t group : added_groups) {
                        occupied_groups.erase(group);
                    }
                }
                return visit(slot + 1, chosen, prefixes, suffixes);
            };
            return visit(0, 0, 0, 0);
        };

        std::vector<std::uint32_t> witness(slots.size(), UINT32_MAX);
        if (!find_assignment(false, witness)) {
            result.status = PC_GOAL_FEASIBILITY_INFEASIBLE;
            result.reason = PC_GOAL_FEASIBILITY_REASON_SLOT_CAPACITY;
            *out_feasibility = result;
            clear_error(out_error);
            return PC_RESULT_OK;
        }
        std::fill(witness.begin(), witness.end(), UINT32_MAX);
        if (!find_assignment(true, witness)) {
            result.status = PC_GOAL_FEASIBILITY_INFEASIBLE;
            result.reason = PC_GOAL_FEASIBILITY_REASON_GROUP_CONFLICT;
            *out_feasibility = result;
            clear_error(out_error);
            return PC_RESULT_OK;
        }
        std::copy(witness.begin(), witness.end(),
                  result.witness_mod_ids);

        const std::uint32_t start_state = calc.intern_item(*start_item);
        if (calc.is_goal_state(calc.state(start_state))) {
            result.status = PC_GOAL_FEASIBILITY_FEASIBLE;
            result.reason = PC_GOAL_FEASIBILITY_REASON_ALREADY_SATISFIED;
            *out_feasibility = result;
            clear_error(out_error);
            return PC_RESULT_OK;
        }
        if (calc.goal().rarity != PC_RARITY_RARE ||
            start_item->prefix_count != 0 || start_item->suffix_count != 0 ||
            start_item->generic_influence_bits != 0 ||
            start_item->item_flags != 0) {
            result.reason = PC_GOAL_FEASIBILITY_REASON_UNSUPPORTED_START;
            *out_feasibility = result;
            clear_error(out_error);
            return PC_RESULT_OK;
        }

        const char* reforge_id = nullptr;
        if (start_item->rarity == PC_RARITY_NORMAL) {
            reforge_id = "alchemy";
        } else if (start_item->rarity == PC_RARITY_RARE) {
            reforge_id = "chaos";
        } else {
            result.reason = PC_GOAL_FEASIBILITY_REASON_UNSUPPORTED_START;
            *out_feasibility = result;
            clear_error(out_error);
            return PC_RESULT_OK;
        }
        const auto action_it =
            calc.registry().index_by_id.find(reforge_id);
        const bool admitted = action_it != calc.registry().index_by_id.end() &&
            std::find(calc.candidates().begin(), calc.candidates().end(),
                      action_it->second) != calc.candidates().end();
        if (!admitted) {
            result.reason = PC_GOAL_FEASIBILITY_REASON_NO_ADMITTED_REFORGE;
            *out_feasibility = result;
            clear_error(out_error);
            return PC_RESULT_OK;
        }
        const solver::ActionDescriptor& action =
            calc.registry().actions[action_it->second];
        const auto reachable =
            solver::action_explicit_affix_reachable_mask(session, action);
        bool reaches_witness = solver::action_legal(
            session, action, calc.state(start_state));
        for (const std::uint32_t mod : witness) {
            if (mod != UINT32_MAX &&
                !poecraft::pc_bitset_test(reachable.data(), mod)) {
                reaches_witness = false;
                break;
            }
        }
        if (!reaches_witness) {
            result.reason = PC_GOAL_FEASIBILITY_REASON_NO_ADMITTED_REFORGE;
            *out_feasibility = result;
            clear_error(out_error);
            return PC_RESULT_OK;
        }
        result.status = PC_GOAL_FEASIBILITY_FEASIBLE;
        result.reason = PC_GOAL_FEASIBILITY_REASON_NATURAL_REFORGE_WITNESS;
        result.witness_action_index = action_it->second;
        result.witness_action_id = action.id.c_str();
        *out_feasibility = result;
        clear_error(out_error);
        return PC_RESULT_OK;
    } catch (const std::exception& ex) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, ex.what());
        return PC_RESULT_INVALID_ARGUMENT;
    }
}

pc_result pc_calc_action_outcomes(
    pc_solver_handle solver,
    const pc_item_state* item,
    uint32_t action_index,
    pc_calc_outcome* entries,
    uint32_t capacity,
    uint32_t* out_count,
    pc_calc_summary* out_summary,
    pc_error_info* out_error) {
    if (solver == nullptr || item == nullptr || out_count == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    *out_count = 0;
    if (action_index >= solver->calc->registry().actions.size()) {
        set_error(out_error, PC_RESULT_NOT_FOUND, "action index out of range");
        return PC_RESULT_NOT_FOUND;
    }
    try {
        if (solver->calc->product_solver_parent() &&
            solver->exact_calc == nullptr) {
            solver->exact_calc = std::make_unique<solver::CalcContext>(
                solver->session, solver->calc->goal(),
                solver->calc->registry(), solver->calc->candidates(), false,
                false, true);
        }
        solver::CalcContext& calc =
            solver->exact_calc != nullptr ? *solver->exact_calc
                                          : *solver->calc;
        const std::uint32_t state_id = calc.intern_item(*item);
        const solver::OutcomeDistribution& distribution =
            calc.outcomes(state_id, action_index);
        const bool legal = solver::action_legal(
            calc.session(), calc.registry().actions[action_index],
            calc.state(state_id)) && distribution.applicable;
        *out_count = static_cast<uint32_t>(distribution.entries.size());
        if (out_summary != nullptr) {
            out_summary->struct_size =
                static_cast<uint32_t>(sizeof(*out_summary));
            out_summary->abi_version = PC_ABI_VERSION;
            out_summary->supported = distribution.supported ? 1 : 0;
            out_summary->legal = legal ? 1 : 0;
            out_summary->entry_count = *out_count;
            for (std::size_t i = 0; i < PC_SOLVER_MAX_GOAL_SLOTS; ++i) {
                out_summary->slot_satisfied_probability[i] =
                    distribution.slot_satisfied_probability[i];
            }
            out_summary->success_probability = 0.0;
            if (distribution.supported && legal) {
                for (const solver::OutcomeEntry& entry :
                     distribution.entries) {
                    if (calc.is_goal_state(calc.state(entry.state))) {
                        out_summary->success_probability +=
                            entry.probability;
                    }
                }
            }
        }
        const uint32_t writable =
            entries == nullptr
                ? 0
                : std::min<uint32_t>(capacity, *out_count);
        for (uint32_t i = 0; i < writable; ++i) {
            const solver::OutcomeEntry& entry = distribution.entries[i];
            const solver::AbstractState& state = calc.state(entry.state);
            pc_calc_outcome& out = entries[i];
            out.struct_size = static_cast<uint32_t>(sizeof(out));
            out.abi_version = PC_ABI_VERSION;
            out.state_id = entry.state;
            out.probability = entry.probability;
            out.rarity = state.rarity;
            out.prefix_count = state.prefix_count;
            out.suffix_count = state.suffix_count;
            out.influence_bits = state.influence_bits;
            out.flags = state.flags;
            out.blocked_mask = state.blocked_mask;
            for (std::size_t s = 0; s < PC_SOLVER_MAX_GOAL_SLOTS; ++s) {
                out.slot_status[s] = state.slot_status[s];
            }
        }
        clear_error(out_error);
        return PC_RESULT_OK;
    } catch (const std::exception& ex) {
        set_error(out_error, PC_RESULT_INTERNAL_ERROR, ex.what());
        return PC_RESULT_INTERNAL_ERROR;
    }
}

pc_result pc_solver_solve(
    pc_solver_handle solver,
    const pc_item_state* start_item,
    pc_economy_handle economy,
    const pc_solve_options* options,
    pc_solve_summary* out_summary,
    pc_error_info* out_error) {
    if (solver == nullptr || start_item == nullptr || economy == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT,
                  "solver, start item, and economy are required");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    try {
        solver->solve_work.reset();
        /*
         * A replacement solve starts a new handle-owned memory budget.
         * Release the previous retained policy (including an exact refined
         * strategy) and the ordinary compiled-output cache before SolveWork
         * begins, rather than carrying either full document beside the new
         * graph until commit.
         */
        solver->solved.reset();
        solver->compilation.reset();
        std::string{}.swap(solver->compiled_strategy);
        solver->abandoned_telemetry.clear();
        solver->abandoned_telemetry_capped = false;
        solver->abandoned_telemetry_limit = 0;
        solver::SolveWork work(
            *solver->calc, *start_item, economy_prices(economy),
            solve_options(options));
        while (!work.progress().done) work.step(4096);
        solver->peak_owned_bytes = std::max(
            solver->peak_owned_bytes,
            sizeof(*solver) + work.peak_owned_bytes());
        commit_solve(solver, work.finish());
        copy_solve_summary(*solver->solved, out_summary);
        clear_error(out_error);
        return PC_RESULT_OK;
    } catch (const std::exception& ex) {
        set_error(out_error, PC_RESULT_INTERNAL_ERROR, ex.what());
        return PC_RESULT_INTERNAL_ERROR;
    }
}

pc_result pc_solver_solve_begin(
    pc_solver_handle solver,
    const pc_item_state* start_item,
    pc_economy_handle economy,
    const pc_solve_options* options,
    pc_error_info* out_error) {
    if (solver == nullptr || start_item == nullptr || economy == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT,
                  "solver, start item, and economy are required");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    try {
        /*
         * Beginning stepped replacement invalidates the previous solve.
         * Release its retained strategy and any ordinary compiled cache
         * before constructing the new work object so both documents cannot
         * sit outside the new solve's declared byte cap.
         */
        solver->solve_work.reset();
        solver->solved.reset();
        solver->compilation.reset();
        std::string{}.swap(solver->compiled_strategy);
        auto work = std::make_unique<solver::SolveWork>(
            *solver->calc, *start_item, economy_prices(economy),
            solve_options(options));
        solver->solve_work = std::move(work);
        solver->solve_log.clear();
        solver->abandoned_telemetry.clear();
        solver->abandoned_telemetry_capped = false;
        solver->abandoned_telemetry_limit = 0;
        clear_error(out_error);
        return PC_RESULT_OK;
    } catch (const std::exception& ex) {
        set_error(out_error, PC_RESULT_INTERNAL_ERROR, ex.what());
        return PC_RESULT_INTERNAL_ERROR;
    }
}

pc_result pc_solver_solve_step(
    pc_solver_handle solver,
    uint32_t max_work_items,
    pc_solve_progress* out_progress,
    pc_error_info* out_error) {
    if (solver == nullptr || out_progress == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    if (!solver->solve_work) {
        set_error(out_error, PC_RESULT_NOT_FOUND,
                  "no stepped solve is in progress");
        return PC_RESULT_NOT_FOUND;
    }
    try {
        solver->solve_work->step(max_work_items);
        copy_solve_progress(solver->solve_work->progress(), *out_progress);
        clear_error(out_error);
        return PC_RESULT_OK;
    } catch (const std::exception& ex) {
        set_error(out_error, PC_RESULT_INTERNAL_ERROR, ex.what());
        return PC_RESULT_INTERNAL_ERROR;
    }
}

pc_result pc_solver_solve_request_bounded_finish(
    pc_solver_handle solver,
    pc_error_info* out_error) {
    if (solver == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    if (!solver->solve_work) {
        set_error(out_error, PC_RESULT_NOT_FOUND,
                  "no stepped solve is in progress");
        return PC_RESULT_NOT_FOUND;
    }
    try {
        solver->solve_work->request_bounded_finish();
        clear_error(out_error);
        return PC_RESULT_OK;
    } catch (const std::exception& ex) {
        set_error(out_error, PC_RESULT_INTERNAL_ERROR, ex.what());
        return PC_RESULT_INTERNAL_ERROR;
    }
}

pc_result pc_solver_solve_finish(
    pc_solver_handle solver,
    pc_solve_summary* out_summary,
    pc_error_info* out_error) {
    if (solver == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    if (!solver->solve_work) {
        set_error(out_error, PC_RESULT_NOT_FOUND,
                  "no stepped solve is in progress");
        return PC_RESULT_NOT_FOUND;
    }
    if (!solver->solve_work->progress().done) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT,
                  "stepped solve is not finished");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    try {
        solver->peak_owned_bytes = std::max(
            solver->peak_owned_bytes,
            sizeof(*solver) + solver->solve_work->peak_owned_bytes());
        commit_solve(solver, solver->solve_work->finish());
        solver->solve_work.reset();
        copy_solve_summary(*solver->solved, out_summary);
        clear_error(out_error);
        return PC_RESULT_OK;
    } catch (const std::exception& ex) {
        set_error(out_error, PC_RESULT_INTERNAL_ERROR, ex.what());
        return PC_RESULT_INTERNAL_ERROR;
    }
}

void pc_solver_solve_abandon(pc_solver_handle solver) {
    if (solver == nullptr || !solver->solve_work) return;
    solver->abandoned_telemetry_capped = false;
    solver->abandoned_telemetry_limit = 0;
    try {
        solver->peak_owned_bytes = std::max(
            solver->peak_owned_bytes,
            sizeof(*solver) + solver->solve_work->peak_owned_bytes());
        const solver::SolveTelemetrySnapshot snapshot =
            solver->solve_work->telemetry_snapshot(true);
        solver->abandoned_telemetry_limit =
            snapshot.diagnostics.telemetry_json_byte_limit;
        solver->abandoned_telemetry = solver::serialize_solver_telemetry(
            *solver->calc, nullptr, &snapshot,
            solver->registry_generation_ns, nullptr);
    } catch (const std::length_error&) {
        solver->abandoned_telemetry.clear();
        solver->abandoned_telemetry_capped = true;
    } catch (const std::exception&) {
        solver->abandoned_telemetry.clear();
    }
    solver->solve_work.reset();
}

pc_result pc_solver_state_value(
    pc_solver_handle solver,
    uint32_t state_id,
    double* out_value,
    const char** out_action_id,
    pc_error_info* out_error) {
    if (solver == nullptr || out_value == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    if (!solver->solved.has_value()) {
        set_error(out_error, PC_RESULT_NOT_FOUND, "no solve has run yet");
        return PC_RESULT_NOT_FOUND;
    }
    const solver::SolveResult& result = *solver->solved;
    if (!result.refined_policy_artifact.strategy_json.empty()) {
        /*
         * One coarse state can contain several exact subclasses with
         * different values or actions. The executable refined policy is the
         * retained strategy router; returning a coarse state/action pair here
         * would expose a different policy through the same solve handle.
         */
        set_error(
            out_error, PC_RESULT_UNSUPPORTED_FEATURE,
            "per-state coarse policy queries are unavailable for a "
            "policy-guided exact refined strategy");
        return PC_RESULT_UNSUPPORTED_FEATURE;
    }
    if (state_id >= result.values.size()) {
        set_error(out_error, PC_RESULT_NOT_FOUND,
                  "state id outside the solved set");
        return PC_RESULT_NOT_FOUND;
    }
    *out_value = result.values[state_id];
    if (out_action_id != nullptr) {
        const std::uint32_t action = result.policy[state_id];
        *out_action_id =
            action == solver::kNoId
                ? nullptr
                : solver->calc->operators().at(action).id.c_str();
    }
    clear_error(out_error);
    return PC_RESULT_OK;
}

pc_result pc_solver_project_item(
    pc_solver_handle solver,
    const pc_item_state* item,
    uint32_t* out_state_id,
    pc_error_info* out_error) {
    if (solver == nullptr || item == nullptr || out_state_id == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    *out_state_id = solver->calc->intern_item(*item);
    clear_error(out_error);
    return PC_RESULT_OK;
}

namespace {

pc_result copy_text(
    const std::string& text,
    char* buffer,
    size_t capacity,
    size_t* out_length,
    pc_error_info* out_error) {
    if (out_length == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    *out_length = text.size();
    if (buffer != nullptr && capacity > 0) {
        const size_t writable = std::min(capacity - 1, text.size());
        std::memcpy(buffer, text.data(), writable);
        buffer[writable] = '\0';
    }
    clear_error(out_error);
    return PC_RESULT_OK;
}

solver::StrategyEvalOptions strategy_eval_options(
    const pc_strategy_eval_options* options) {
    solver::StrategyEvalOptions result;
    if (options == nullptr) return result;
    constexpr std::size_t kLegacyOptionsSize =
        offsetof(pc_strategy_eval_options, economy);
    if (options->struct_size < kLegacyOptionsSize ||
        options->abi_version != PC_ABI_VERSION) {
        throw std::invalid_argument("invalid strategy evaluation options ABI");
    }
    if (options->epsilon > 0.0) result.epsilon = options->epsilon;
    if (options->max_sweeps != 0) result.max_sweeps = options->max_sweeps;
    if (options->max_states != 0) result.max_states = options->max_states;
    if (options->max_pairs != 0) result.max_pairs = options->max_pairs;
    if (options->max_transitions != 0) {
        result.max_transitions = options->max_transitions;
    }
    if (options->top_classes_per_node != 0) {
        result.top_classes_per_node = options->top_classes_per_node;
    }
    #define PC_EVAL_OPTION_HAS(field)                                    \
        (options->struct_size >=                                        \
         offsetof(pc_strategy_eval_options, field) + sizeof(options->field))
    if (PC_EVAL_OPTION_HAS(economy)) {
        if (options->economy != nullptr) {
            result.economy = options->economy->impl;
        }
    }
    if (PC_EVAL_OPTION_HAS(review_projection_json_size)) {
        if (options->review_projection_json_size != 0) {
            if (options->review_projection_json == nullptr) {
                throw std::invalid_argument(
                    "null strategy review projection JSON");
            }
            result.review_projection_json.assign(
                options->review_projection_json,
                options->review_projection_json_size);
        }
    }
    if (PC_EVAL_OPTION_HAS(include_success_normalized)) {
        result.include_success_normalized =
            options->include_success_normalized != 0;
    }
    if (PC_EVAL_OPTION_HAS(max_owned_bytes) &&
        options->max_owned_bytes != 0) {
        result.max_owned_bytes = options->max_owned_bytes;
    }
    if (PC_EVAL_OPTION_HAS(max_output_json_bytes) &&
        options->max_output_json_bytes != 0) {
        result.max_output_json_bytes = options->max_output_json_bytes;
    }
    #undef PC_EVAL_OPTION_HAS
    return result;
}

int32_t strategy_eval_phase(solver::StrategyEvalPhase phase) {
    switch (phase) {
    case solver::StrategyEvalPhase::Discovery:
        return PC_STRATEGY_EVAL_PHASE_DISCOVERY;
    case solver::StrategyEvalPhase::Solving:
        return PC_STRATEGY_EVAL_PHASE_SOLVING;
    case solver::StrategyEvalPhase::Fallback:
        return PC_STRATEGY_EVAL_PHASE_FALLBACK;
    case solver::StrategyEvalPhase::Finalization:
        return PC_STRATEGY_EVAL_PHASE_FINALIZATION;
    case solver::StrategyEvalPhase::Done:
        return PC_STRATEGY_EVAL_PHASE_DONE;
    }
    return PC_STRATEGY_EVAL_PHASE_DONE;
}

void copy_strategy_eval_progress(
    const solver::StrategyEvalProgress& source,
    pc_strategy_eval_progress& target) {
    target = {};
    target.struct_size = sizeof(target);
    target.abi_version = PC_ABI_VERSION;
    target.phase = strategy_eval_phase(source.phase);
    target.done = source.done ? 1 : 0;
    target.discovered_pairs = source.discovered_pairs;
    target.pending_pairs = source.pending_pairs;
    target.solved_sccs = source.solved_sccs;
    target.total_sccs = source.total_sccs;
    target.fallback_sweeps = source.fallback_sweeps;
    target.residual = source.residual;
}

} // namespace

struct pc_strategy_eval_work {
    std::unique_ptr<solver::StrategyEvalWork> impl;
    std::string result_json;
};

pc_result pc_solver_compile_strategy(
    pc_solver_handle solver,
    char* buffer,
    size_t capacity,
    size_t* out_length,
    pc_error_info* out_error) {
    if (solver == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    if (!solver->solved.has_value()) {
        set_error(out_error, PC_RESULT_NOT_FOUND, "no solve has run yet");
        return PC_RESULT_NOT_FOUND;
    }
    if (!solver->solved->policy_available) {
        set_error(out_error, PC_RESULT_NOT_FOUND,
                  "latest solve has no executable policy");
        return PC_RESULT_NOT_FOUND;
    }
    try {
        /*
         * Policy-guided refinement already retained the exact, parsed and
         * reconciled document inside SolveResult under the solve-owned byte
         * cap. Return that storage directly. Copying it into the handle cache
         * would make the first compile query own two complete strategy
         * documents without a second cap check.
         */
        const solver::RetainedCompiledPolicyArtifact& refined =
            solver->solved->refined_policy_artifact;
        if (!refined.strategy_json.empty()) {
            if (!solver->compilation.has_value()) {
                solver->compilation.emplace();
                solver::PolicyCompilationTelemetry& telemetry =
                    *solver->compilation;
                telemetry.working_states = refined.working_states;
                telemetry.behavioral_classes =
                    refined.behavioral_classes;
                telemetry.policy_regions = refined.policy_regions;
                telemetry.infrastructure_nodes =
                    refined.infrastructure_nodes;
                telemetry.policy_route_nodes = refined.policy_route_nodes;
                telemetry.local_gated_route_nodes =
                    refined.local_gated_route_nodes;
                telemetry.primitive_region_nodes =
                    refined.primitive_region_nodes;
                telemetry.additional_recipe_nodes =
                    refined.additional_recipe_nodes;
                telemetry.nodes = refined.nodes;
                telemetry.edges = refined.edges;
                telemetry.strategy_json_bytes =
                    refined.strategy_json.size();
                telemetry.total_condition_bytes =
                    refined.total_condition_bytes;
                telemetry.max_condition_bytes =
                    refined.max_condition_bytes;
                telemetry.condition_edges = refined.condition_edges;
                telemetry.unique_condition_literals =
                    refined.unique_condition_literals;
                telemetry.repeated_condition_occurrences =
                    refined.repeated_condition_occurrences;
                telemetry.repeated_condition_bytes =
                    refined.repeated_condition_bytes;
                telemetry.policy_route_nondefault_edges =
                    refined.policy_route_nondefault_edges;
                telemetry.policy_route_distinct_targets =
                    refined.policy_route_distinct_targets;
                telemetry.same_target_branch_groups =
                    refined.same_target_branch_groups;
                telemetry.same_target_branch_edges =
                    refined.same_target_branch_edges;
                telemetry.projected_same_target_edge_savings =
                    refined.projected_same_target_edge_savings;
                telemetry.max_policy_route_out_degree =
                    refined.max_policy_route_out_degree;
                telemetry.max_policy_route_distinct_targets =
                    refined.max_policy_route_distinct_targets;
                telemetry.exact_state_fallbacks =
                    refined.exact_state_fallbacks;
                telemetry.junk_predicates = refined.junk_predicates;
                telemetry.policy_route_default_edges =
                    refined.policy_route_default_edges;
                telemetry.policy_route_restart_default_edges =
                    refined.policy_route_restart_default_edges;
                telemetry.policy_route_offpolicy_default_edges =
                    refined.policy_route_offpolicy_default_edges;
                telemetry.policy_route_default_mode =
                    refined.policy_route_default_mode;
                telemetry.certification_policy_route_default_edges =
                    refined.certification_policy_route_default_edges;
                telemetry.certification_policy_route_offpolicy_default_edges =
                    refined
                        .certification_policy_route_offpolicy_default_edges;
                telemetry.certification_policy_route_default_mode =
                    refined.certification_policy_route_default_mode;
                telemetry.paired_default_only =
                    refined.paired_default_only;
                telemetry.previously_accounted_peak_owned_bytes = std::max(
                    refined.previously_accounted_peak_owned_bytes,
                    static_cast<std::uint64_t>(
                        refined.strategy_json.capacity()) + 1);
                telemetry.complete_peak_owned_bytes =
                    std::max(
                        refined.complete_peak_owned_bytes,
                        telemetry.previously_accounted_peak_owned_bytes);
                telemetry.peak_owned_bytes =
                    telemetry.complete_peak_owned_bytes;
            }
            return copy_text(
                refined.strategy_json, buffer, capacity, out_length,
                out_error);
        }
        if (solver->compiled_strategy.empty()) {
            const auto started = std::chrono::steady_clock::now();
            solver->compilation.emplace();
            solver::PolicyCompilationTelemetry& telemetry =
                *solver->compilation;
            solver->compiled_strategy =
                solver::compile_policy_strategy_json(
                    *solver->calc, *solver->solved, "solved policy",
                    &telemetry);
            telemetry.duration_ns = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() - started)
                    .count());
        }
        return copy_text(solver->compiled_strategy, buffer, capacity,
                         out_length, out_error);
    } catch (const std::exception& ex) {
        set_error(out_error, PC_RESULT_UNSUPPORTED_FEATURE, ex.what());
        return PC_RESULT_UNSUPPORTED_FEATURE;
    }
}

pc_result pc_solver_solve_log(
    pc_solver_handle solver,
    char* buffer,
    size_t capacity,
    size_t* out_length,
    pc_error_info* out_error) {
    if (solver == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    if (!solver->solved.has_value()) {
        set_error(out_error, PC_RESULT_NOT_FOUND, "no solve has run yet");
        return PC_RESULT_NOT_FOUND;
    }
    if (solver->solve_log.empty()) {
        solver->solve_log =
            solver::serialize_solve_log(*solver->calc, *solver->solved);
    }
    return copy_text(solver->solve_log, buffer, capacity, out_length,
                     out_error);
}

pc_result pc_solver_telemetry(
    pc_solver_handle solver,
    char* buffer,
    size_t capacity,
    size_t* out_length,
    pc_error_info* out_error) {
    if (solver == nullptr || out_length == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    try {
        if (!solver->solve_work && !solver->solved.has_value() &&
            solver->abandoned_telemetry_capped) {
            throw std::length_error(
                "solver telemetry exceeded max_telemetry_json_bytes (" +
                std::to_string(solver->abandoned_telemetry_limit) + ")");
        }
        if (!solver->solve_work && !solver->solved.has_value() &&
            !solver->abandoned_telemetry.empty()) {
            return copy_text(solver->abandoned_telemetry, buffer, capacity,
                             out_length, out_error);
        }
        std::optional<solver::SolveTelemetrySnapshot> snapshot;
        if (solver->solve_work) {
            snapshot = solver->solve_work->telemetry_snapshot();
        }
        const std::string telemetry = solver::serialize_solver_telemetry(
            *solver->calc,
            solver->solved.has_value() ? &*solver->solved : nullptr,
            snapshot.has_value() ? &*snapshot : nullptr,
            solver->registry_generation_ns,
            solver->compilation.has_value() ? &*solver->compilation
                                            : nullptr);
        return copy_text(
            telemetry, buffer, capacity, out_length, out_error);
    } catch (const std::length_error& ex) {
        set_error(out_error, PC_RESULT_CAPACITY_EXCEEDED, ex.what());
        return PC_RESULT_CAPACITY_EXCEEDED;
    } catch (const std::exception& ex) {
        set_error(out_error, PC_RESULT_INTERNAL_ERROR, ex.what());
        return PC_RESULT_INTERNAL_ERROR;
    }
}

pc_result pc_solver_memory_stats(
    pc_solver_handle solver,
    pc_native_memory_stats* out_stats,
    pc_error_info* out_error) {
    if (solver == nullptr || out_stats == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    std::uint64_t serialized =
        solver->compiled_strategy.capacity() + solver->solve_log.capacity() +
        solver->abandoned_telemetry.capacity();
    std::uint64_t live = sizeof(*solver) + serialized;
    std::uint64_t peak = live;
    if (solver->solve_work) {
        live += solver->solve_work->live_owned_bytes();
        peak += solver->solve_work->peak_owned_bytes();
    } else {
        live += solver::estimated_retained_solver_bytes(
            *solver->calc,
            solver->solved.has_value() ? &*solver->solved : nullptr);
        peak = live;
        if (solver->solved.has_value()) {
            peak = std::max(
                peak,
                solver->solved->diagnostics.solver_owned_bytes_estimate +
                    sizeof(*solver) + serialized);
        }
    }
    *out_stats = {};
    out_stats->struct_size = sizeof(*out_stats);
    out_stats->abi_version = PC_ABI_VERSION;
    out_stats->live_owned_bytes = live;
    solver->peak_owned_bytes = std::max(solver->peak_owned_bytes, peak);
    out_stats->peak_owned_bytes = solver->peak_owned_bytes;
    out_stats->serialized_output_bytes = serialized;
    clear_error(out_error);
    return PC_RESULT_OK;
}

pc_result pc_strategy_evaluate(
    pc_strategy_handle strategy,
    const pc_strategy_eval_options* options,
    char* buffer,
    size_t capacity,
    size_t* out_length,
    pc_error_info* out_error) {
    if (strategy == nullptr || out_length == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    try {
        solver::StrategyEvalWork work(
            strategy->impl, strategy_eval_options(options));
        while (!work.progress().done) work.step(4096);
        const std::string json =
            solver::serialize_strategy_eval(work.result());
        return copy_text(json, buffer, capacity, out_length, out_error);
    } catch (const solver::StrategyEvalUnsupported& ex) {
        set_error(out_error, PC_RESULT_UNSUPPORTED_FEATURE, ex.what());
        return PC_RESULT_UNSUPPORTED_FEATURE;
    } catch (const std::invalid_argument& ex) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, ex.what());
        return PC_RESULT_INVALID_ARGUMENT;
    } catch (const std::length_error& ex) {
        set_error(out_error, PC_RESULT_CAPACITY_EXCEEDED, ex.what());
        return PC_RESULT_CAPACITY_EXCEEDED;
    } catch (const std::exception& ex) {
        set_error(out_error, PC_RESULT_INTERNAL_ERROR, ex.what());
        return PC_RESULT_INTERNAL_ERROR;
    }
}

pc_result pc_strategy_eval_begin(
    pc_strategy_handle strategy,
    const pc_strategy_eval_options* options,
    pc_strategy_eval_work_handle* out_work,
    pc_error_info* out_error) {
    if (strategy == nullptr || out_work == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    *out_work = nullptr;
    try {
        auto work = std::make_unique<pc_strategy_eval_work>();
        work->impl = std::make_unique<solver::StrategyEvalWork>(
            strategy->impl, strategy_eval_options(options));
        *out_work = work.release();
        clear_error(out_error);
        return PC_RESULT_OK;
    } catch (const solver::StrategyEvalUnsupported& ex) {
        set_error(out_error, PC_RESULT_UNSUPPORTED_FEATURE, ex.what());
        return PC_RESULT_UNSUPPORTED_FEATURE;
    } catch (const std::invalid_argument& ex) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, ex.what());
        return PC_RESULT_INVALID_ARGUMENT;
    } catch (const std::length_error& ex) {
        set_error(out_error, PC_RESULT_CAPACITY_EXCEEDED, ex.what());
        return PC_RESULT_CAPACITY_EXCEEDED;
    } catch (const std::exception& ex) {
        set_error(out_error, PC_RESULT_INTERNAL_ERROR, ex.what());
        return PC_RESULT_INTERNAL_ERROR;
    }
}

pc_result pc_strategy_eval_step(
    pc_strategy_eval_work_handle work,
    uint32_t max_work_items,
    pc_strategy_eval_progress* out_progress,
    pc_error_info* out_error) {
    if (work == nullptr || out_progress == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    try {
        work->impl->step(max_work_items);
        copy_strategy_eval_progress(work->impl->progress(), *out_progress);
        clear_error(out_error);
        return PC_RESULT_OK;
    } catch (const solver::StrategyEvalUnsupported& ex) {
        set_error(out_error, PC_RESULT_UNSUPPORTED_FEATURE, ex.what());
        return PC_RESULT_UNSUPPORTED_FEATURE;
    } catch (const std::length_error& ex) {
        set_error(out_error, PC_RESULT_CAPACITY_EXCEEDED, ex.what());
        return PC_RESULT_CAPACITY_EXCEEDED;
    } catch (const std::invalid_argument& ex) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, ex.what());
        return PC_RESULT_INVALID_ARGUMENT;
    } catch (const std::exception& ex) {
        set_error(out_error, PC_RESULT_INTERNAL_ERROR, ex.what());
        return PC_RESULT_INTERNAL_ERROR;
    }
}

pc_result pc_strategy_eval_finish(
    pc_strategy_eval_work_handle work,
    char* buffer,
    size_t capacity,
    size_t* out_length,
    pc_error_info* out_error) {
    if (work == nullptr || out_length == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    if (!work->impl->progress().done) {
        set_error(
            out_error, PC_RESULT_INVALID_ARGUMENT,
            "strategy evaluation is not finished");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    try {
        if (work->result_json.empty()) {
            work->result_json =
                solver::serialize_strategy_eval(work->impl->result());
        }
        return copy_text(
            work->result_json, buffer, capacity, out_length, out_error);
    } catch (const std::length_error& ex) {
        set_error(out_error, PC_RESULT_CAPACITY_EXCEEDED, ex.what());
        return PC_RESULT_CAPACITY_EXCEEDED;
    } catch (const std::exception& ex) {
        set_error(out_error, PC_RESULT_INTERNAL_ERROR, ex.what());
        return PC_RESULT_INTERNAL_ERROR;
    }
}

void pc_strategy_eval_destroy(pc_strategy_eval_work_handle work) {
    delete work;
}

pc_result pc_strategy_eval_memory_stats(
    pc_strategy_eval_work_handle work,
    pc_native_memory_stats* out_stats,
    pc_error_info* out_error) {
    if (work == nullptr || out_stats == nullptr) {
        set_error(out_error, PC_RESULT_INVALID_ARGUMENT, "null argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    const std::uint64_t serialized = work->result_json.capacity();
    const std::uint64_t live = sizeof(*work) + serialized +
                               work->impl->live_owned_bytes();
    const std::uint64_t peak = sizeof(*work) + serialized +
        std::max(work->impl->peak_owned_bytes(),
                 work->impl->live_owned_bytes());
    *out_stats = {};
    out_stats->struct_size = sizeof(*out_stats);
    out_stats->abi_version = PC_ABI_VERSION;
    out_stats->live_owned_bytes = live;
    out_stats->peak_owned_bytes = peak;
    out_stats->serialized_output_bytes = serialized;
    clear_error(out_error);
    return PC_RESULT_OK;
}
