#include "poecraft/solver.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "handles_internal.hpp"
#include "json.hpp"
#include "solver_internal.hpp"

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
    const char* goal_json,
    std::size_t goal_json_size) {
    const Value root = Parser(goal_json, goal_json_size).parse();
    if (root.type != Type::Object) {
        throw std::runtime_error("goal: root must be an object");
    }
    solver::ActionRegistryBuildOptions options;
    const Value* actions = root.find("actions");
    if (actions == nullptr) return options;
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
    const Value* fixed_options = root.find("options");
    if (fixed_options != nullptr && fixed_options->type == Type::Array) {
        for (const Value& option : fixed_options->array) {
            if (option.type != Type::Object) continue;
            const std::string action = string_member(option, "action");
            if (action.starts_with("fossil:")) {
                options.requested_fossil_action_ids.push_back(action);
            }
            for (const char* key : {"setup", "bench_crafts"}) {
                const Value* program = option.find(key);
                if (program == nullptr || program->type != Type::Array) {
                    continue;
                }
                for (const Value& step : program->array) {
                    if (step.type == Type::String &&
                        step.string.starts_with("fossil:")) {
                        options.requested_fossil_action_ids.push_back(
                            step.string);
                    }
                }
            }
        }
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
            } else {
                throw std::runtime_error(
                    "goal: unknown fixed option type: " + type);
            }

            if (option.kind == solver::FixedOptionKind::EldritchSideIntent ||
                option.kind == solver::FixedOptionKind::ProtectedSide) {
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
    std::unique_ptr<solver::SolveWork> solve_work;
    std::optional<solver::SolveResult> solved;
    std::optional<std::uint64_t> registry_generation_ns;
    std::optional<solver::PolicyCompilationTelemetry> compilation;
    std::string compiled_strategy; /* scratch for the buffer queries */
    std::string solve_log;
    std::string abandoned_telemetry;
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
    }
    return PC_SOLVE_PHASE_DONE;
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
        result.diagnostics.skipped_missing_price.size() +
        result.diagnostics.skipped_unsupported.size());
}

void commit_solve(pc_solver_handle solver, solver::SolveResult result) {
    solver->solved = std::move(result);
    solver->compilation.reset();
    solver->compiled_strategy.clear();
    solver->solve_log.clear();
    solver->abandoned_telemetry.clear();
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
            registry_build_options(goal_json, goal_json_size);
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
            !goal.primitive_actions_explicit);
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
        solver::CalcContext& calc = *solver->calc;
        const std::uint32_t state_id = calc.intern_item(*item);
        const solver::OutcomeDistribution& distribution =
            calc.outcomes(state_id, action_index);
        const bool legal = solver::action_legal(
            calc.session(), calc.registry().actions[action_index],
            calc.state(state_id));
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
        solver->abandoned_telemetry.clear();
        solver::SolveWork work(
            *solver->calc, *start_item, economy_prices(economy),
            solve_options(options));
        while (!work.progress().done) work.step(4096);
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
        auto work = std::make_unique<solver::SolveWork>(
            *solver->calc, *start_item, economy_prices(economy),
            solve_options(options));
        solver->solve_work = std::move(work);
        solver->solved.reset();
        solver->compilation.reset();
        solver->compiled_strategy.clear();
        solver->solve_log.clear();
        solver->abandoned_telemetry.clear();
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
    try {
        const solver::SolveTelemetrySnapshot snapshot =
            solver->solve_work->telemetry_snapshot(true);
        solver->abandoned_telemetry = solver::serialize_solver_telemetry(
            *solver->calc, nullptr, &snapshot,
            solver->registry_generation_ns, nullptr);
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
    if (options->struct_size < sizeof(pc_strategy_eval_options) ||
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
    try {
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
    } catch (const std::exception& ex) {
        set_error(out_error, PC_RESULT_INTERNAL_ERROR, ex.what());
        return PC_RESULT_INTERNAL_ERROR;
    }
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
    } catch (const std::exception& ex) {
        set_error(out_error, PC_RESULT_INTERNAL_ERROR, ex.what());
        return PC_RESULT_INTERNAL_ERROR;
    }
}

void pc_strategy_eval_destroy(pc_strategy_eval_work_handle work) {
    delete work;
}
