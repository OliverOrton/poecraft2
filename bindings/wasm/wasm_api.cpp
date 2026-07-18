/*
 * WebAssembly facade for the poecraft engine.
 *
 * This is the browser-facing boundary (Phase 8). It is deliberately
 * coarse-grained and string-shaped: every entry point takes and returns a
 * UTF-8 JSON document and operates on small integer handles managed here, so
 * the TypeScript `EngineClient`/worker never have to reach into WASM linear
 * memory or marshal the large `pc_item_state` struct. Data is loaded through
 * the C ABI memory path (`pc_data_load_memory`), the only option a browser has.
 *
 * The module is single-threaded inside a Web Worker, and Emscripten's `ccall`
 * copies a returned string out via `UTF8ToString` synchronously before the next
 * call runs, so reusing one static response buffer per call is safe.
 */

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include <emscripten.h>

#include "poecraft/bestiary.h"
#include "poecraft/api.h"
#include "poecraft/item_state.h"
#include "poecraft/session.h"
#include "poecraft/simulator.h"
#include "poecraft/solver.h"

#include "json.hpp"

namespace {

using poecraft::json::Parser;
using poecraft::json::Type;
using poecraft::json::Value;

// --- tiny JSON output builder ----------------------------------------------

void append_escaped(std::string& out, const char* s) {
    out.push_back('"');
    if (s != nullptr) {
        for (const char* p = s; *p != '\0'; ++p) {
            unsigned char c = static_cast<unsigned char>(*p);
            switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    static const char* hex = "0123456789abcdef";
                    out += "\\u00";
                    out.push_back(hex[(c >> 4) & 0xF]);
                    out.push_back(hex[c & 0xF]);
                } else {
                    out.push_back(static_cast<char>(c));
                }
            }
        }
    }
    out.push_back('"');
}

void append_u64(std::string& out, std::uint64_t value) {
    out += std::to_string(value);
}

// --- handle registries ------------------------------------------------------

std::unordered_map<std::uint32_t, pc_data_handle> g_data;
std::unordered_map<std::uint32_t, pc_session_handle> g_sessions;
std::unordered_map<std::uint32_t, pc_action_context_handle> g_contexts;
std::unordered_map<std::uint32_t, pc_item_state> g_items;
std::unordered_map<std::uint32_t, pc_bestiary_craft_state> g_bestiary_states;
std::unordered_map<std::uint32_t, pc_strategy_handle> g_strategies;
std::unordered_map<std::uint32_t, pc_economy_handle> g_economies;
std::unordered_map<std::uint32_t, pc_simulator_handle> g_simulators;
std::unordered_map<std::uint32_t, pc_solver_handle> g_solvers;
std::unordered_map<std::uint32_t, pc_strategy_eval_work_handle> g_evaluations;
std::uint32_t g_next_id = 1;

std::string g_response;

const char* respond(std::string&& body) {
    g_response = std::move(body);
    return g_response.c_str();
}

const char* fail(pc_result code, const char* message) {
    std::string out = "{\"ok\":false,\"code\":";
    out += std::to_string(static_cast<int>(code));
    out += ",\"message\":";
    append_escaped(out, message);
    out.push_back('}');
    return respond(std::move(out));
}

const char* fail(const pc_error_info& error) {
    return fail(static_cast<pc_result>(error.code), error.message);
}

pc_error_info make_error() {
    pc_error_info error;
    pc_error_info_init(&error);
    return error;
}

template <typename Map>
auto find(Map& map, std::uint32_t id) -> typename Map::mapped_type* {
    auto it = map.find(id);
    return it == map.end() ? nullptr : &it->second;
}

void reset_bestiary_state(std::uint32_t item_id,
                          const pc_item_state& item) {
    pc_bestiary_craft_state state{};
    pc_error_info error = make_error();
    if (pc_bestiary_state_init(&item, item_id, &state, &error) ==
        PC_RESULT_OK) {
        g_bestiary_states[item_id] = state;
    }
}

pc_bestiary_craft_state* sync_bestiary_state(
    std::uint32_t item_id,
    const pc_item_state& item) {
    auto* state = find(g_bestiary_states, item_id);
    if (state == nullptr) {
        reset_bestiary_state(item_id, item);
        state = find(g_bestiary_states, item_id);
    }
    if (state != nullptr) state->item = item;
    return state;
}

// --- action / option parsing ------------------------------------------------

bool action_type_from_name(const std::string& name, int32_t& out) {
    static const std::pair<const char*, int32_t> table[] = {
        {"transmute", PC_ACTION_TRANSMUTE}, {"augment", PC_ACTION_AUGMENT},
        {"alteration", PC_ACTION_ALTERATION}, {"regal", PC_ACTION_REGAL},
        {"alchemy", PC_ACTION_ALCHEMY}, {"chaos", PC_ACTION_CHAOS},
        {"exalt", PC_ACTION_EXALT}, {"annul", PC_ACTION_ANNUL},
        {"scour", PC_ACTION_SCOUR}, {"essence", PC_ACTION_ESSENCE},
        {"fossil", PC_ACTION_FOSSIL}, {"bench", PC_ACTION_BENCH},
        {"veiled_chaos", PC_ACTION_VEILED_CHAOS},
        {"veiled_exalt", PC_ACTION_VEILED_EXALT},
        {"unveil", PC_ACTION_UNVEIL},
        {"harvest_reforge", PC_ACTION_HARVEST_REFORGE},
        {"harvest_augment", PC_ACTION_HARVEST_AUGMENT},
        {"harvest_resist", PC_ACTION_HARVEST_RESIST},
        {"eldritch_ember", PC_ACTION_ELDRITCH_EMBER},
        {"eldritch_ichor", PC_ACTION_ELDRITCH_ICHOR},
        {"eldritch_exalt", PC_ACTION_ELDRITCH_EXALT},
        {"eldritch_chaos", PC_ACTION_ELDRITCH_CHAOS},
        {"eldritch_annul", PC_ACTION_ELDRITCH_ANNUL},
        {"influence_exalt", PC_ACTION_INFLUENCE_EXALT},
        {"fracture", PC_ACTION_FRACTURE},
        {"remove_crafted_modifiers", PC_ACTION_REMOVE_CRAFTED_MODIFIERS},
    };
    for (const auto& entry : table) {
        if (name == entry.first) {
            out = entry.second;
            return true;
        }
    }
    return false;
}

// Build a pc_action_request from a JSON action object. String key bytes are
// kept alive in `storage` for the duration of the engine call.
bool parse_action(const Value& spec, pc_action_request& request,
                  std::vector<std::string>& storage, std::string& error) {
    std::memset(&request, 0, sizeof(request));
    storage.reserve(16);
    request.struct_size = sizeof(request);
    request.abi_version = PC_ABI_VERSION;
    const Value* type = spec.find("type");
    if (type == nullptr || type->type != Type::String) {
        error = "action requires a string \"type\"";
        return false;
    }
    if (!action_type_from_name(type->string, request.action_type)) {
        error = "unknown action type: " + type->string;
        return false;
    }
    if (request.action_type == PC_ACTION_ESSENCE) {
        const Value* key = spec.find("essence");
        if (key == nullptr || key->type != Type::String || key->string.empty()) {
            error = "essence action requires a string \"essence\" key";
            return false;
        }
        storage.push_back(key->string);
        request.essence_key = storage.back().c_str();
    }
    if (request.action_type == PC_ACTION_FOSSIL) {
        const Value* fossils = spec.find("fossils");
        if (fossils == nullptr || fossils->type != Type::Array ||
            fossils->array.empty() ||
            fossils->array.size() > PC_MAX_FOSSILS_PER_ACTION) {
            error = "fossil action requires 1-4 \"fossils\" keys";
            return false;
        }
        request.fossil_count = static_cast<uint32_t>(fossils->array.size());
        storage.reserve(storage.size() + fossils->array.size());
        for (const auto& entry : fossils->array) {
            if (entry.type != Type::String) {
                error = "fossil keys must be strings";
                return false;
            }
            storage.push_back(entry.string);
        }
        // Pointers must be taken after all reservations to stay stable.
        std::size_t base = storage.size() - fossils->array.size();
        for (std::size_t i = 0; i < fossils->array.size(); ++i) {
            request.fossil_keys[i] = storage[base + i].c_str();
        }
    }
    auto string_param = [&](const char* name, const char** out,
                            bool required = true) {
        const Value* value = spec.find(name);
        if (value == nullptr || value->type != Type::String ||
            value->string.empty()) {
            if (required) {
                error = std::string("action requires a string \"") + name +
                        "\"";
                return false;
            }
            return true;
        }
        storage.push_back(value->string);
        *out = storage.back().c_str();
        return true;
    };
    if (request.action_type == PC_ACTION_BENCH ||
        request.action_type == PC_ACTION_UNVEIL) {
        if (!string_param("mod_key", &request.mod_key)) return false;
    }
    if (request.action_type == PC_ACTION_HARVEST_REFORGE ||
        request.action_type == PC_ACTION_HARVEST_AUGMENT) {
        if (!string_param("target_tag", &request.target_tag)) return false;
    }
    if (request.action_type == PC_ACTION_HARVEST_RESIST) {
        if (!string_param("source_tag", &request.source_tag) ||
            !string_param("target_tag", &request.target_tag)) {
            return false;
        }
    }
    if (request.action_type == PC_ACTION_INFLUENCE_EXALT) {
        if (!string_param("influence", &request.influence)) return false;
    }
    if (request.action_type == PC_ACTION_ELDRITCH_EMBER ||
        request.action_type == PC_ACTION_ELDRITCH_ICHOR) {
        const Value* tier = spec.find("tier");
        request.tier =
            tier != nullptr && tier->type == Type::Number
                ? static_cast<uint32_t>(tier->number)
                : 0;
        if (request.tier < 1 || request.tier > 4) {
            error = "Eldritch tier must be 1-4";
            return false;
        }
    }
    return true;
}

void append_item_max(std::string& out, pc_session_handle session,
                     const pc_item_state* item) {
    uint32_t max_prefix = 0;
    uint32_t max_suffix = 0;
    pc_error_info error = make_error();
    pc_session_item_max_prefix(session, item, &max_prefix, &error);
    error = make_error();
    pc_session_item_max_suffix(session, item, &max_suffix, &error);
    out += ",\"max_prefix\":" + std::to_string(max_prefix);
    out += ",\"max_suffix\":" + std::to_string(max_suffix);
}

void append_item_info(std::string& out, const pc_item_state& item) {
    static const char* rarities[] = {"normal", "magic", "rare"};
    out += "\"rarity\":";
    append_escaped(out, item.rarity < 3 ? rarities[item.rarity] : "normal");
    auto append_ids = [&out](const char* name, const pc_mod_slot* slots,
                             uint8_t count, bool fractured_only) {
        out += ",\"";
        out += name;
        out += "\":[";
        bool first = true;
        for (uint8_t i = 0; i < count; ++i) {
            if (fractured_only && (slots[i].flags & PC_MOD_SLOT_FRACTURED) == 0) {
                continue;
            }
            if (!first) out.push_back(',');
            first = false;
            out += std::to_string(slots[i].mod_id);
        }
        out.push_back(']');
    };
    append_ids("prefix_mod_ids", item.prefixes, item.prefix_count, false);
    append_ids("suffix_mod_ids", item.suffixes, item.suffix_count, false);
    append_ids("implicit_mod_ids", item.implicits, item.implicit_count, false);
    out += ",\"fractured_prefix_mod_ids\":[";
    {
        bool first = true;
        for (uint8_t i = 0; i < item.prefix_count; ++i) {
            if ((item.prefixes[i].flags & PC_MOD_SLOT_FRACTURED) == 0) continue;
            if (!first) out.push_back(',');
            first = false;
            out += std::to_string(item.prefixes[i].mod_id);
        }
    }
    out += "],\"fractured_suffix_mod_ids\":[";
    {
        bool first = true;
        for (uint8_t i = 0; i < item.suffix_count; ++i) {
            if ((item.suffixes[i].flags & PC_MOD_SLOT_FRACTURED) == 0) continue;
            if (!first) out.push_back(',');
            first = false;
            out += std::to_string(item.suffixes[i].mod_id);
        }
    }
    out += "],\"prefix_count\":";
    out += std::to_string(item.prefix_count);
    out += ",\"suffix_count\":";
    out += std::to_string(item.suffix_count);
    out += ",\"item_flags\":" + std::to_string(item.item_flags);
    out += ",\"generic_influence_bits\":" +
           std::to_string(item.generic_influence_bits);
    out += ",\"searing_exarch_tier\":" +
           std::to_string(item.searing_exarch_tier);
    out += ",\"eater_of_worlds_tier\":" +
           std::to_string(item.eater_of_worlds_tier);
    out += ",\"veiled_option_mod_ids\":[";
    bool first_option = true;
    auto append_veiled = [&](const pc_mod_slot* slots, uint8_t count) {
        for (uint8_t i = 0; i < count; ++i) {
            if ((slots[i].flags & PC_MOD_SLOT_VEILED) == 0) continue;
            for (uint8_t j = 0; j < slots[i].veiled_option_count; ++j) {
                if (!first_option) out.push_back(',');
                first_option = false;
                out += std::to_string(slots[i].veiled_option_mod_ids[j]);
            }
        }
    };
    append_veiled(item.prefixes, item.prefix_count);
    append_veiled(item.suffixes, item.suffix_count);
    out.push_back(']');
}

// --- full item state serialization (for Stash save/restore) ----------------

void append_slot(std::string& out, const pc_mod_slot& slot) {
    out += "{\"mod_id\":" + std::to_string(slot.mod_id);
    out += ",\"group_id\":" + std::to_string(slot.group_id);
    out += ",\"flags\":" + std::to_string(slot.flags);
    out += ",\"rolls\":[";
    for (uint8_t i = 0; i < slot.roll_count && i < PC_MAX_ROLL_VALUES; ++i) {
        if (i != 0) out.push_back(',');
        out += std::to_string(slot.rolls[i]);
    }
    out += "],\"veiled_option_mod_ids\":[";
    for (uint8_t i = 0;
         i < slot.veiled_option_count && i < PC_MAX_VEILED_OPTIONS; ++i) {
        if (i != 0) out.push_back(',');
        out += std::to_string(slot.veiled_option_mod_ids[i]);
    }
    out += "],\"veiled_chosen_mod_id\":" +
           std::to_string(slot.veiled_chosen_mod_id) + "}";
}

void append_slot_array(std::string& out, const char* name,
                       const pc_mod_slot* slots, uint8_t count) {
    out += "\"";
    out += name;
    out += "\":[";
    for (uint8_t i = 0; i < count; ++i) {
        if (i != 0) out.push_back(',');
        append_slot(out, slots[i]);
    }
    out.push_back(']');
}

void append_item_state(std::string& out, const pc_item_state& item) {
    out += "{\"rarity\":" + std::to_string(item.rarity);
    out += ",\"quality\":" + std::to_string(item.quality);
    out += ",\"item_flags\":" + std::to_string(item.item_flags);
    out += ",\"generic_influence_bits\":" +
           std::to_string(item.generic_influence_bits);
    out += ",\"searing_exarch_tier\":" +
           std::to_string(item.searing_exarch_tier);
    out += ",\"eater_of_worlds_tier\":" +
           std::to_string(item.eater_of_worlds_tier);
    out += ",\"link_mask\":" + std::to_string(item.link_mask);
    out += ",\"socket_count\":" + std::to_string(item.socket_count);
    out += ",\"socket_colors\":[";
    for (uint8_t i = 0; i < item.socket_count && i < PC_MAX_SOCKETS; ++i) {
        if (i != 0) out.push_back(',');
        out += std::to_string(item.socket_colors[i]);
    }
    out += "],";
    append_slot_array(out, "prefixes", item.prefixes, item.prefix_count);
    out.push_back(',');
    append_slot_array(out, "suffixes", item.suffixes, item.suffix_count);
    out.push_back(',');
    append_slot_array(out, "implicits", item.implicits, item.implicit_count);
    out.push_back(',');
    append_slot_array(
        out, "enchantments", item.enchantments, item.enchantment_count);
    out.push_back('}');
}

void append_compound_item_state(
    std::string& out,
    const pc_bestiary_craft_state& state) {
    append_item_state(out, state.item);
    out.pop_back();
    out += ",\"bestiary\":{\"checkpoint_present\":";
    out += state.checkpoint_present ? "true" : "false";
    if (state.checkpoint_present) {
        out += ",\"checkpoint\":";
        append_item_state(out, state.checkpoint);
    }
    out += "}}";
}

void append_bestiary_result(
    std::string& out,
    const pc_bestiary_action_result& result) {
    out += "{\"action_id\":";
    append_escaped(out, result.action_id);
    out += ",\"applied\":";
    out += result.applied ? "true" : "false";
    out += ",\"refusal_code\":" + std::to_string(result.refusal);
    out += ",\"refusal_key\":";
    append_escaped(out, result.refusal_key);
    out += ",\"refusal_reason\":";
    append_escaped(out, result.refusal_reason);
    out += ",\"cost_keys\":[";
    for (std::uint32_t i = 0; i < result.cost_key_count; ++i) {
        if (i != 0) out.push_back(',');
        append_escaped(out, result.cost_keys[i]);
    }
    out += "],\"consumed_price_keys\":[";
    for (std::uint32_t i = 0;
         i < result.consumed_price_key_count; ++i) {
        if (i != 0) out.push_back(',');
        append_escaped(out, result.consumed_price_keys[i]);
    }
    out += "],\"output_item_count\":" +
           std::to_string(result.output_item_count);
    out += ",\"output_checkpoint_count\":" +
           std::to_string(result.output_checkpoint_count);
    out += ",\"consumed_checkpoint_count\":" +
           std::to_string(result.consumed_checkpoint_count);
    out += ",\"checkpoint_present\":";
    out += result.checkpoint_present ? "true" : "false";
    out.push_back('}');
}

uint32_t obj_u32(const Value& object, const char* key, uint32_t fallback = 0) {
    const Value* value = object.find(key);
    return value != nullptr && value->type == Type::Number
               ? static_cast<uint32_t>(value->number)
               : fallback;
}

uint64_t obj_u64(const Value& object, const char* key, uint64_t fallback = 0) {
    const Value* value = object.find(key);
    return value != nullptr && value->type == Type::Number
               ? static_cast<uint64_t>(value->number)
               : fallback;
}

double obj_double(const Value& object, const char* key, double fallback = 0.0) {
    const Value* value = object.find(key);
    return value != nullptr && value->type == Type::Number
               ? value->number
               : fallback;
}

bool parse_strategy_eval_options(
    const char* options_json,
    pc_strategy_eval_options& options,
    std::string& review_projection_storage,
    std::string& error) {
    options = {};
    options.struct_size = sizeof(options);
    options.abi_version = PC_ABI_VERSION;
    if (options_json == nullptr || options_json[0] == '\0') return true;
    Value spec;
    try {
        spec = Parser(options_json, std::strlen(options_json)).parse();
    } catch (const std::exception& ex) {
        error = ex.what();
        return false;
    }
    if (spec.type != Type::Object) {
        error = "options must be an object";
        return false;
    }
    options.epsilon = obj_double(spec, "epsilon");
    options.max_sweeps = obj_u32(spec, "max_sweeps");
    options.max_states = obj_u32(spec, "max_states");
    options.max_pairs = obj_u32(spec, "max_pairs");
    options.max_transitions = obj_u32(spec, "max_transitions");
    options.top_classes_per_node =
        obj_u32(spec, "top_classes_per_node");
    options.max_owned_bytes = obj_u64(spec, "max_owned_bytes");
    options.max_output_json_bytes =
        obj_u64(spec, "max_output_json_bytes");
    const std::uint32_t economy_id = obj_u32(spec, "economy_id");
    if (economy_id != 0) {
        pc_economy_handle* economy = find(g_economies, economy_id);
        if (economy == nullptr) {
            error = "unknown strategy evaluation economy";
            return false;
        }
        options.economy = *economy;
    }
    const Value* review = spec.find("review_projection_json");
    if (review != nullptr) {
        if (review->type != Type::String) {
            error = "review_projection_json must be a string";
            return false;
        }
        review_projection_storage = review->string;
        options.review_projection_json = review_projection_storage.data();
        options.review_projection_json_size =
            review_projection_storage.size();
    }
    const Value* normalized = spec.find("include_success_normalized");
    if (normalized != nullptr) {
        if (normalized->type != Type::Bool) {
            error = "include_success_normalized must be a bool";
            return false;
        }
        options.include_success_normalized = normalized->boolean ? 1 : 0;
    }
    return true;
}

bool parse_solve_options(
    const char* options_json,
    pc_solve_options& options,
    std::string& error) {
    options = {};
    options.struct_size = sizeof(options);
    options.abi_version = PC_ABI_VERSION;
    if (options_json == nullptr || options_json[0] == '\0') return true;
    Value spec;
    try {
        spec = Parser(options_json, std::strlen(options_json)).parse();
    } catch (const std::exception& ex) {
        error = ex.what();
        return false;
    }
    if (spec.type != Type::Object) {
        error = "options must be an object";
        return false;
    }
    options.epsilon = obj_double(spec, "epsilon");
    options.max_states = obj_u32(spec, "max_states");
    options.max_sweeps = obj_u32(spec, "max_sweeps");
    options.max_discovered_states = obj_u32(spec, "max_discovered_states");
    options.max_expanded_states = obj_u32(spec, "max_expanded_states");
    options.max_state_action_rows = obj_u64(spec, "max_state_action_rows");
    options.max_transitions = obj_u64(spec, "max_transitions");
    options.max_reforge_work = obj_u64(spec, "max_reforge_work");
    options.max_solver_owned_bytes = obj_u64(spec, "max_solver_owned_bytes");
    options.max_compiled_nodes = obj_u32(spec, "max_compiled_nodes");
    options.max_compiled_edges = obj_u32(spec, "max_compiled_edges");
    options.max_strategy_json_bytes = obj_u64(
        spec, "max_strategy_json_bytes");
    options.max_diagnostic_samples =
        obj_u32(spec, "max_diagnostic_samples");
    options.max_telemetry_json_bytes = obj_u64(
        spec, "max_telemetry_json_bytes");
    return true;
}

const char* strategy_eval_phase_name(int32_t phase) {
    switch (phase) {
    case PC_STRATEGY_EVAL_PHASE_DISCOVERY: return "discovery";
    case PC_STRATEGY_EVAL_PHASE_SOLVING: return "solving";
    case PC_STRATEGY_EVAL_PHASE_FALLBACK: return "fallback";
    case PC_STRATEGY_EVAL_PHASE_FINALIZATION: return "finalization";
    case PC_STRATEGY_EVAL_PHASE_DONE: return "done";
    default: return "discovery";
    }
}

void append_strategy_eval_progress(
    std::string& out,
    const pc_strategy_eval_progress& progress) {
    out += "{\"phase\":\"";
    out += strategy_eval_phase_name(progress.phase);
    out += "\",\"done\":";
    out += progress.done ? "true" : "false";
    out += ",\"discovered_pairs\":";
    append_u64(out, progress.discovered_pairs);
    out += ",\"pending_pairs\":";
    append_u64(out, progress.pending_pairs);
    out += ",\"solved_sccs\":";
    append_u64(out, progress.solved_sccs);
    out += ",\"total_sccs\":";
    append_u64(out, progress.total_sccs);
    out += ",\"fallback_sweeps\":";
    append_u64(out, progress.fallback_sweeps);
    out += ",\"residual\":" + std::to_string(progress.residual) + '}';
}

const char* solve_phase_name(const int32_t phase) {
    switch (phase) {
    case PC_SOLVE_PHASE_EXPANDING: return "expanding";
    case PC_SOLVE_PHASE_ITERATING: return "iterating";
    case PC_SOLVE_PHASE_DONE: return "done";
    default: return "expanding";
    }
}

void append_precise_double(std::string& out, const double value) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.17g", value);
    out += buffer;
}

void append_solve_progress(
    std::string& out,
    const pc_solve_progress& progress) {
    out += "{\"phase\":\"";
    out += solve_phase_name(progress.phase);
    out += "\",\"done\":";
    out += progress.done ? "true" : "false";
    out += ",\"expanded_states\":" +
           std::to_string(progress.expanded_states);
    out += ",\"sweeps\":" + std::to_string(progress.sweeps);
    out += ",\"residual\":";
    append_precise_double(out, progress.residual);
    out += ",\"start_value_bound\":";
    append_precise_double(out, progress.start_value_bound);
    out.push_back('}');
}

void append_solve_summary(
    std::string& out,
    const pc_solve_summary& summary) {
    out += "\"converged\":";
    out += summary.converged ? "true" : "false";
    out += ",\"start_state\":" + std::to_string(summary.start_state);
    out += ",\"start_value\":";
    append_precise_double(out, summary.start_value);
    out += ",\"expanded_states\":" +
           std::to_string(summary.expanded_states);
    out += ",\"sweeps\":" + std::to_string(summary.sweeps);
    out += ",\"residual\":";
    append_precise_double(out, summary.residual);
    out += ",\"skipped_actions\":" +
           std::to_string(summary.skipped_action_count);
}

const char* terminal_name(int32_t kind) {
    if (kind == PC_TERMINAL_SUCCESS) return "success";
    if (kind == PC_TERMINAL_FAILURE) return "failure";
    if (kind == PC_TERMINAL_STOP) return "stop";
    return "";
}

pc_mod_slot parse_slot(const Value& object) {
    pc_mod_slot slot;
    std::memset(&slot, 0, sizeof(slot));
    slot.mod_id = obj_u32(object, "mod_id", PC_MOD_NONE);
    slot.group_id = static_cast<uint16_t>(obj_u32(object, "group_id"));
    slot.flags = static_cast<uint8_t>(obj_u32(object, "flags"));
    slot.veiled_chosen_mod_id =
        obj_u32(object, "veiled_chosen_mod_id", PC_MOD_NONE);
    const Value* rolls = object.find("rolls");
    if (rolls != nullptr && rolls->type == Type::Array) {
        for (std::size_t i = 0;
             i < rolls->array.size() && i < PC_MAX_ROLL_VALUES; ++i) {
            slot.rolls[i] = static_cast<int32_t>(rolls->array[i].number);
            slot.roll_count = static_cast<uint8_t>(i + 1);
        }
    }
    const Value* veiled = object.find("veiled_option_mod_ids");
    if (veiled != nullptr && veiled->type == Type::Array) {
        for (std::size_t i = 0;
             i < veiled->array.size() && i < PC_MAX_VEILED_OPTIONS; ++i) {
            slot.veiled_option_mod_ids[i] =
                static_cast<uint32_t>(veiled->array[i].number);
            slot.veiled_option_count = static_cast<uint8_t>(i + 1);
        }
    }
    return slot;
}

uint8_t parse_slot_array(const Value& state, const char* name,
                         pc_mod_slot* slots, uint8_t capacity) {
    const Value* array = state.find(name);
    if (array == nullptr || array->type != Type::Array) {
        return 0;
    }
    uint8_t count = 0;
    for (const auto& entry : array->array) {
        if (count >= capacity || entry.type != Type::Object) {
            break;
        }
        slots[count++] = parse_slot(entry);
    }
    return count;
}
pc_item_state parse_item_state(const Value& state) {
    pc_item_state item{};
    item.rarity = static_cast<uint8_t>(obj_u32(state, "rarity"));
    item.quality = static_cast<uint8_t>(obj_u32(state, "quality"));
    item.item_flags = static_cast<uint8_t>(obj_u32(state, "item_flags"));
    item.generic_influence_bits =
        static_cast<uint8_t>(obj_u32(state, "generic_influence_bits"));
    item.searing_exarch_tier =
        static_cast<uint8_t>(obj_u32(state, "searing_exarch_tier"));
    item.eater_of_worlds_tier =
        static_cast<uint8_t>(obj_u32(state, "eater_of_worlds_tier"));
    item.link_mask = static_cast<uint8_t>(obj_u32(state, "link_mask"));
    const Value* sockets = state.find("socket_colors");
    if (sockets != nullptr && sockets->type == Type::Array) {
        for (std::size_t i = 0;
             i < sockets->array.size() && i < PC_MAX_SOCKETS; ++i) {
            item.socket_colors[i] =
                static_cast<uint8_t>(sockets->array[i].number);
            item.socket_count = static_cast<uint8_t>(i + 1);
        }
    }
    item.prefix_count =
        parse_slot_array(state, "prefixes", item.prefixes, PC_MAX_PREFIXES);
    item.suffix_count =
        parse_slot_array(state, "suffixes", item.suffixes, PC_MAX_SUFFIXES);
    item.implicit_count =
        parse_slot_array(state, "implicits", item.implicits, PC_MAX_IMPLICITS);
    item.enchantment_count = parse_slot_array(
        state, "enchantments", item.enchantments, PC_MAX_ENCHANTS);
    return item;
}

} // namespace

extern "C" {

EMSCRIPTEN_KEEPALIVE
uint32_t pcw_abi_version(void) { return pc_abi_version(); }

// `bundle_json` points at a heap buffer the caller (worker) allocated with
// _malloc and frees afterwards; the compiled data bundle is multi-megabyte, so
// it is copied to the heap rather than passed as a stack-allocated ccall string.
EMSCRIPTEN_KEEPALIVE
const char* pcw_data_open(const char* bundle_json, uint32_t byte_count) {
    if (bundle_json == nullptr) {
        return fail(PC_RESULT_INVALID_ARGUMENT, "null bundle");
    }
    pc_data_handle data = nullptr;
    pc_error_info error = make_error();
    pc_result rc = pc_data_load_memory(bundle_json, byte_count, &data, &error);
    if (rc != PC_RESULT_OK) {
        return fail(error);
    }
    std::uint32_t id = g_next_id++;
    g_data[id] = data;
    std::string out = "{\"ok\":true,\"data\":";
    out += std::to_string(id);
    out.push_back('}');
    return respond(std::move(out));
}

EMSCRIPTEN_KEEPALIVE
const char* pcw_data_summary(uint32_t data_id) {
    pc_data_handle* data = find(g_data, data_id);
    if (data == nullptr) return fail(PC_RESULT_NOT_FOUND, "unknown data handle");
    pc_data_summary summary;
    summary.struct_size = sizeof(summary);
    pc_error_info error = make_error();
    pc_result rc = pc_data_get_summary(*data, &summary, &error);
    if (rc != PC_RESULT_OK) return fail(error);
    std::string out = "{\"ok\":true";
    out += ",\"artifact_schema_version\":" + std::to_string(summary.artifact_schema_version);
    out += ",\"complete_dataset\":";
    out += summary.complete_dataset ? "true" : "false";
    out += ",\"base_item_count\":" + std::to_string(summary.base_item_count);
    out += ",\"ordinary_session_base_count\":" + std::to_string(summary.ordinary_session_base_count);
    out += ",\"mod_count\":" + std::to_string(summary.mod_count);
    out += ",\"group_count\":" + std::to_string(summary.group_count);
    out.push_back('}');
    return respond(std::move(out));
}

// Enumerate every base by dense index, with its metadata path and session
// support classification. Drives the base picker. Returns one JSON document so
// the UI fetches the list in a single worker round-trip.
EMSCRIPTEN_KEEPALIVE
const char* pcw_data_bases(uint32_t data_id) {
    pc_data_handle* data = find(g_data, data_id);
    if (data == nullptr) return fail(PC_RESULT_NOT_FOUND, "unknown data handle");
    pc_data_summary summary;
    summary.struct_size = sizeof(summary);
    pc_error_info error = make_error();
    if (pc_data_get_summary(*data, &summary, &error) != PC_RESULT_OK) {
        return fail(error);
    }
    std::string out = "{\"ok\":true,\"bases\":[";
    for (uint32_t i = 0; i < summary.base_item_count; ++i) {
        const char* path = nullptr;
        int32_t support = 0;
        error = make_error();
        pc_result rc =
            pc_data_get_base_path(*data, i, &path, &support, &error);
        if (rc != PC_RESULT_OK) return fail(error);
        const char* name = nullptr;
        const char* item_class_key = nullptr;
        error = make_error();
        rc = pc_data_get_base_display(*data, i, &name, &item_class_key, &error);
        if (rc != PC_RESULT_OK) return fail(error);
        int32_t drop_level = -1;
        error = make_error();
        rc = pc_data_get_base_drop_level(*data, i, &drop_level, &error);
        if (rc != PC_RESULT_OK) return fail(error);
        if (i != 0) out.push_back(',');
        out += "{\"path\":";
        append_escaped(out, path);
        out += ",\"name\":";
        append_escaped(out, name ? name : "");
        out += ",\"item_class_key\":";
        append_escaped(out, item_class_key ? item_class_key : "");
        out += ",\"drop_level\":" + std::to_string(drop_level);
        out += ",\"support\":" + std::to_string(support);
        out.push_back('}');
    }
    out += "]}";
    return respond(std::move(out));
}

EMSCRIPTEN_KEEPALIVE
const char* pcw_bestiary_presentation(uint32_t data_id) {
    pc_data_handle* data = find(g_data, data_id);
    if (data == nullptr) return fail(PC_RESULT_NOT_FOUND, "unknown data handle");
    std::uint32_t action_count = 0;
    pc_error_info error = make_error();
    if (pc_bestiary_get_action_count(
            *data, &action_count, &error) != PC_RESULT_OK) {
        return fail(error);
    }
    std::string out = "{\"ok\":true,\"actions\":[";
    for (std::uint32_t index = 0; index < action_count; ++index) {
        pc_bestiary_action_info info{};
        error = make_error();
        if (pc_bestiary_get_action_info(
                *data, index, &info, &error) != PC_RESULT_OK) {
            return fail(error);
        }
        if (index != 0) out.push_back(',');
        out += "{\"index\":" + std::to_string(info.action_index);
        out += ",\"global_action_id\":" +
               std::to_string(info.global_action_id);
        out += ",\"global_recipe_id\":" +
               std::to_string(info.global_recipe_id);
        out += ",\"id\":";
        append_escaped(out, info.action_id);
        out += ",\"recipe_id\":";
        append_escaped(out, info.recipe_id);
        out += ",\"family_display_name\":";
        append_escaped(out, info.family_display_name);
        out += ",\"display_name\":";
        append_escaped(out, info.display_name);
        out += ",\"operation\":";
        append_escaped(
            out,
            info.operation == PC_BESTIARY_OPERATION_CREATE
                ? "create"
                : "restore");
        out += ",\"transition_kind\":\"deterministic\"";
        out += ",\"emulator_available\":";
        out += info.emulator_available ? "true" : "false";
        out += ",\"calculator_available\":";
        out += info.calculator_available ? "true" : "false";
        out += ",\"strategy_builder_available\":";
        out += info.strategy_builder_available ? "true" : "false";
        out += ",\"solver_available\":";
        out += info.solver_available ? "true" : "false";
        out += ",\"checkpoint_requirement\":";
        append_escaped(
            out,
            info.checkpoint_requirement == PC_BESTIARY_CHECKPOINT_ABSENT
                ? "absent"
                : "present");
        out += ",\"checkpoint_effect\":";
        append_escaped(
            out,
            info.checkpoint_effect == PC_BESTIARY_CHECKPOINT_CREATE
                ? "create"
                : "consume");
        out += ",\"identity_requirement\":";
        append_escaped(out, info.identity_requirement == 0
                                ? "current_item"
                                : "same_item");
        out += ",\"cost_keys\":[";
        for (std::uint32_t key = 0; key < info.cost_key_count; ++key) {
            if (key != 0) out.push_back(',');
            append_escaped(out, info.cost_keys[key]);
        }
        out += "]}";
    }
    out += "],\"solver_options\":[";
    std::uint32_t option_count = 0;
    error = make_error();
    if (pc_bestiary_get_solver_option_count(
            *data, &option_count, &error) != PC_RESULT_OK) {
        return fail(error);
    }
    for (std::uint32_t index = 0; index < option_count; ++index) {
        pc_bestiary_solver_option_info info{};
        error = make_error();
        if (pc_bestiary_get_solver_option_info(
                *data, index, &info, &error) != PC_RESULT_OK) {
            return fail(error);
        }
        if (index != 0) out.push_back(',');
        out += "{\"index\":" + std::to_string(info.option_index);
        out += ",\"id\":";
        append_escaped(out, info.option_id);
        out += ",\"display_name\":";
        append_escaped(out, info.display_name);
        out += ",\"goal_restriction_key\":";
        append_escaped(out, info.goal_restriction_key);
        out += ",\"goal_restriction\":";
        append_escaped(out, info.goal_restriction);
        out += ",\"goal_rarity\":\"magic\"";
        out += ",\"requires_complete_goal\":";
        out += info.requires_complete_goal ? "true" : "false";
        out += ",\"min_program_actions\":" +
               std::to_string(info.min_program_actions);
        out += ",\"max_program_actions\":" +
               std::to_string(info.max_program_actions);
        out.push_back('}');
    }
    out += "]}";
    return respond(std::move(out));
}

EMSCRIPTEN_KEEPALIVE
void pcw_data_close(uint32_t data_id) {
    pc_data_handle* data = find(g_data, data_id);
    if (data != nullptr) {
        pc_data_destroy(*data);
        g_data.erase(data_id);
    }
}

EMSCRIPTEN_KEEPALIVE
const char* pcw_session_open(uint32_t data_id, const char* options_json) {
    pc_data_handle* data = find(g_data, data_id);
    if (data == nullptr) return fail(PC_RESULT_NOT_FOUND, "unknown data handle");
    Value opts;
    try {
        opts = Parser(options_json, std::strlen(options_json)).parse();
    } catch (const std::exception& e) {
        return fail(PC_RESULT_INVALID_ARGUMENT, e.what());
    }
    const Value* base = opts.find("base");
    if (base == nullptr || base->type != Type::String) {
        return fail(PC_RESULT_INVALID_ARGUMENT, "session requires a string \"base\"");
    }
    const Value* level = opts.find("item_level");
    pc_session_options session_options;
    session_options.struct_size = sizeof(session_options);
    session_options.abi_version = PC_ABI_VERSION;
    session_options.base_metadata_path = base->string.c_str();
    session_options.item_level =
        level != nullptr && level->type == Type::Number
            ? static_cast<uint32_t>(level->number)
            : 0;
    pc_session_handle session = nullptr;
    pc_error_info error = make_error();
    pc_result rc = pc_session_create(*data, &session_options, &session, &error);
    if (rc != PC_RESULT_OK) return fail(error);
    std::uint32_t id = g_next_id++;
    g_sessions[id] = session;
    std::string out = "{\"ok\":true,\"session\":";
    out += std::to_string(id);
    out.push_back('}');
    return respond(std::move(out));
}

EMSCRIPTEN_KEEPALIVE
void pcw_session_close(uint32_t session_id) {
    pc_session_handle* session = find(g_sessions, session_id);
    if (session != nullptr) {
        pc_session_destroy(*session);
        g_sessions.erase(session_id);
    }
}

EMSCRIPTEN_KEEPALIVE
const char* pcw_session_mod_count(uint32_t session_id) {
    pc_session_handle* session = find(g_sessions, session_id);
    if (session == nullptr) return fail(PC_RESULT_NOT_FOUND, "unknown session");
    uint32_t count = 0;
    pc_error_info error = make_error();
    pc_result rc = pc_session_get_mod_count(*session, &count, &error);
    if (rc != PC_RESULT_OK) return fail(error);
    std::string out = "{\"ok\":true,\"count\":";
    out += std::to_string(count);
    out.push_back('}');
    return respond(std::move(out));
}

EMSCRIPTEN_KEEPALIVE
const char* pcw_session_mod_info(uint32_t session_id, uint32_t mod_id) {
    pc_session_handle* session = find(g_sessions, session_id);
    if (session == nullptr) return fail(PC_RESULT_NOT_FOUND, "unknown session");
    pc_mod_info info;
    info.struct_size = sizeof(info);
    pc_error_info error = make_error();
    pc_result rc = pc_session_get_mod_info(*session, mod_id, &info, &error);
    if (rc != PC_RESULT_OK) return fail(error);
    std::string out = "{\"ok\":true,\"session_mod_id\":";
    out += std::to_string(info.session_mod_id);
    out += ",\"global_mod_id\":" + std::to_string(info.global_mod_id);
    out += ",\"key\":";
    append_escaped(out, info.key);
    out += ",\"generation_type\":" + std::to_string(info.generation_type);
    out += ",\"reach_kind\":" + std::to_string(info.reach_kind);
    out += ",\"reach_influence\":" + std::to_string(info.reach_influence);
    out += ",\"reach_via\":";
    append_escaped(out, info.reach_via);
    out += ",\"primary_group_id\":" + std::to_string(info.primary_group_id);
    out += ",\"family_id\":" + std::to_string(info.family_id);
    out += ",\"required_level\":" + std::to_string(info.required_level);
    out += ",\"group_display_name\":";
    append_escaped(out, info.group_display_name ? info.group_display_name : "");
    out += ",\"family_tier_index\":" + std::to_string(info.family_tier_index);
    out += ",\"text_lines\":[";
    for (uint32_t i = 0; i < info.text_line_count; ++i) {
        if (i != 0) out.push_back(',');
        append_escaped(out, info.text_lines[i] ? info.text_lines[i] : "");
    }
    out += "]";
    out += ",\"classification_tags\":[";
    for (uint32_t i = 0; i < info.classification_tag_count; ++i) {
        if (i != 0) out.push_back(',');
        append_escaped(
            out,
            info.classification_tags[i] ? info.classification_tags[i] : "");
    }
    out += "]";
    out.push_back('}');
    return respond(std::move(out));
}

EMSCRIPTEN_KEEPALIVE
const char* pcw_context_open(uint32_t session_id, const char* options_json) {
    pc_session_handle* session = find(g_sessions, session_id);
    if (session == nullptr) return fail(PC_RESULT_NOT_FOUND, "unknown session");
    Value opts;
    try {
        opts = Parser(options_json, std::strlen(options_json)).parse();
    } catch (const std::exception& e) {
        return fail(PC_RESULT_INVALID_ARGUMENT, e.what());
    }
    const Value* seed = opts.find("seed");
    pc_action_context_options context_options;
    context_options.struct_size = sizeof(context_options);
    context_options.abi_version = PC_ABI_VERSION;
    context_options.seed = seed != nullptr && seed->type == Type::Number
                               ? static_cast<uint64_t>(seed->number)
                               : 0;
    pc_action_context_handle context = nullptr;
    pc_error_info error = make_error();
    pc_result rc =
        pc_action_context_create(*session, &context_options, &context, &error);
    if (rc != PC_RESULT_OK) return fail(error);
    std::uint32_t id = g_next_id++;
    g_contexts[id] = context;
    std::string out = "{\"ok\":true,\"context\":";
    out += std::to_string(id);
    out.push_back('}');
    return respond(std::move(out));
}

EMSCRIPTEN_KEEPALIVE
void pcw_context_close(uint32_t context_id) {
    pc_action_context_handle* context = find(g_contexts, context_id);
    if (context != nullptr) {
        pc_action_context_destroy(*context);
        g_contexts.erase(context_id);
    }
}

EMSCRIPTEN_KEEPALIVE
const char* pcw_item_create(uint32_t session_id, const char* options_json) {
    pc_session_handle* session = find(g_sessions, session_id);
    if (session == nullptr) return fail(PC_RESULT_NOT_FOUND, "unknown session");
    Value opts;
    try {
        opts = Parser(options_json, std::strlen(options_json)).parse();
    } catch (const std::exception& e) {
        return fail(PC_RESULT_INVALID_ARGUMENT, e.what());
    }
    static const std::pair<const char*, uint8_t> rarities[] = {
        {"normal", PC_RARITY_NORMAL}, {"magic", PC_RARITY_MAGIC},
        {"rare", PC_RARITY_RARE}};
    uint8_t rarity = PC_RARITY_NORMAL;
    const Value* rarity_value = opts.find("rarity");
    if (rarity_value != nullptr && rarity_value->type == Type::String) {
        bool matched = false;
        for (const auto& entry : rarities) {
            if (rarity_value->string == entry.first) {
                rarity = entry.second;
                matched = true;
                break;
            }
        }
        if (!matched) return fail(PC_RESULT_INVALID_ARGUMENT, "unknown rarity");
    }
    const Value* with_implicits = opts.find("with_implicits");
    pc_item_init_options init_options;
    init_options.struct_size = sizeof(init_options);
    init_options.abi_version = PC_ABI_VERSION;
    init_options.rarity = rarity;
    init_options.with_implicits =
        with_implicits != nullptr && with_implicits->type == Type::Bool
            ? (with_implicits->boolean ? 1 : 0)
            : 1;
    pc_item_state state;
    pc_error_info error = make_error();
    pc_result rc = pc_item_init(*session, &init_options, &state, &error);
    if (rc != PC_RESULT_OK) return fail(error);
    std::uint32_t id = g_next_id++;
    g_items[id] = state;
    reset_bestiary_state(id, state);
    std::string out = "{\"ok\":true,\"item\":";
    out += std::to_string(id);
    out.push_back('}');
    return respond(std::move(out));
}

EMSCRIPTEN_KEEPALIVE
const char* pcw_item_clone(uint32_t item_id) {
    pc_item_state* item = find(g_items, item_id);
    if (item == nullptr) return fail(PC_RESULT_NOT_FOUND, "unknown item");
    std::uint32_t id = g_next_id++;
    g_items[id] = *item;
    reset_bestiary_state(id, *item);
    std::string out = "{\"ok\":true,\"item\":";
    out += std::to_string(id);
    out.push_back('}');
    return respond(std::move(out));
}

EMSCRIPTEN_KEEPALIVE
void pcw_item_close(uint32_t item_id) {
    g_bestiary_states.erase(item_id);
    g_items.erase(item_id);
}

EMSCRIPTEN_KEEPALIVE
const char* pcw_item_info(uint32_t item_id, uint32_t session_id) {
    pc_item_state* item = find(g_items, item_id);
    if (item == nullptr) return fail(PC_RESULT_NOT_FOUND, "unknown item");
    std::string out = "{\"ok\":true,";
    append_item_info(out, *item);
    pc_bestiary_craft_state* bestiary =
        sync_bestiary_state(item_id, *item);
    out += ",\"checkpoint_present\":";
    out += bestiary != nullptr && bestiary->checkpoint_present
               ? "true" : "false";
    if (session_id != 0) {
        pc_session_handle* session = find(g_sessions, session_id);
        if (session != nullptr) {
            append_item_max(out, *session, item);
        }
    }
    out.push_back('}');
    return respond(std::move(out));
}

// Add an explicit mod to an item, resolving it by session mod key. Used to
// build the exact fixtures the native tests use (e.g. fractured reforge).
EMSCRIPTEN_KEEPALIVE
const char* pcw_item_add_mod(uint32_t item_id, uint32_t session_id,
                             const char* spec_json) {
    pc_item_state* item = find(g_items, item_id);
    if (item == nullptr) return fail(PC_RESULT_NOT_FOUND, "unknown item");
    pc_session_handle* session = find(g_sessions, session_id);
    if (session == nullptr) return fail(PC_RESULT_NOT_FOUND, "unknown session");
    Value spec;
    try {
        spec = Parser(spec_json, std::strlen(spec_json)).parse();
    } catch (const std::exception& e) {
        return fail(PC_RESULT_INVALID_ARGUMENT, e.what());
    }
    const Value* key = spec.find("key");
    if (key == nullptr || key->type != Type::String) {
        return fail(PC_RESULT_INVALID_ARGUMENT, "add_mod requires a string \"key\"");
    }
    uint32_t count = 0;
    pc_error_info error = make_error();
    if (pc_session_get_mod_count(*session, &count, &error) != PC_RESULT_OK) {
        return fail(error);
    }
    pc_mod_info match;
    bool found = false;
    for (uint32_t i = 0; i < count; ++i) {
        pc_mod_info info;
        info.struct_size = sizeof(info);
        error = make_error();
        if (pc_session_get_mod_info(*session, i, &info, &error) != PC_RESULT_OK) {
            return fail(error);
        }
        if (info.key != nullptr && key->string == info.key) {
            match = info;
            found = true;
            break;
        }
    }
    if (!found) return fail(PC_RESULT_NOT_FOUND, "mod key is not in this session");
    int side = match.generation_type; // 0 prefix, 1 suffix
    const Value* side_value = spec.find("side");
    if (side_value != nullptr && side_value->type == Type::String) {
        if (side_value->string == "prefix") side = PC_SIDE_PREFIX;
        else if (side_value->string == "suffix") side = PC_SIDE_SUFFIX;
        else return fail(PC_RESULT_INVALID_ARGUMENT, "side must be prefix or suffix");
    }
    const Value* fractured = spec.find("fractured");
    uint8_t flags = fractured != nullptr && fractured->type == Type::Bool &&
                            fractured->boolean
                        ? PC_MOD_SLOT_FRACTURED
                        : 0;
    pc_result rc = pc_item_add_mod(item, side, match.session_mod_id,
                                   static_cast<uint16_t>(match.primary_group_id),
                                   flags, nullptr);
    if (rc != PC_RESULT_OK) return fail(rc, "could not add mod to item");
    return respond("{\"ok\":true}");
}

// Remove one explicit mod from an item by side and session mod id. This is an
// emulator-only editing affordance, parallel to pcw_item_add_mod.
EMSCRIPTEN_KEEPALIVE
const char* pcw_item_remove_mod(uint32_t item_id, const char* spec_json) {
    pc_item_state* item = find(g_items, item_id);
    if (item == nullptr) return fail(PC_RESULT_NOT_FOUND, "unknown item");
    Value spec;
    try {
        spec = Parser(spec_json, std::strlen(spec_json)).parse();
    } catch (const std::exception& e) {
        return fail(PC_RESULT_INVALID_ARGUMENT, e.what());
    }
    const Value* side_value = spec.find("side");
    const Value* mod_id_value = spec.find("mod_id");
    if (side_value == nullptr || side_value->type != Type::String ||
        mod_id_value == nullptr || mod_id_value->type != Type::Number) {
        return fail(PC_RESULT_INVALID_ARGUMENT,
                    "remove_mod requires string \"side\" and number \"mod_id\"");
    }
    int side = -1;
    if (side_value->string == "prefix") side = PC_SIDE_PREFIX;
    else if (side_value->string == "suffix") side = PC_SIDE_SUFFIX;
    else {
        return fail(PC_RESULT_INVALID_ARGUMENT, "side must be prefix or suffix");
    }
    const uint32_t mod_id = static_cast<uint32_t>(mod_id_value->number);
    const pc_mod_slot* slots =
        side == PC_SIDE_PREFIX ? item->prefixes : item->suffixes;
    const uint8_t count =
        side == PC_SIDE_PREFIX ? item->prefix_count : item->suffix_count;
    for (uint8_t index = 0; index < count; ++index) {
        if (slots[index].mod_id != mod_id) continue;
        pc_result rc = pc_item_remove_at(item, side, index);
        if (rc != PC_RESULT_OK) return fail(rc, "could not remove mod from item");
        return respond("{\"ok\":true}");
    }
    return fail(PC_RESULT_NOT_FOUND, "mod is not on this item side");
}

EMSCRIPTEN_KEEPALIVE
const char* pcw_item_set_mod_fractured(uint32_t item_id, const char* spec_json) {
    pc_item_state* item = find(g_items, item_id);
    if (item == nullptr) return fail(PC_RESULT_NOT_FOUND, "unknown item");
    if (item->generic_influence_bits != 0) {
        return fail(
            PC_RESULT_INVALID_ARGUMENT,
            "ordinary influence and fractured modifiers are mutually exclusive");
    }
    Value spec;
    try {
        spec = Parser(spec_json, std::strlen(spec_json)).parse();
    } catch (const std::exception& e) {
        return fail(PC_RESULT_INVALID_ARGUMENT, e.what());
    }
    const Value* side_value = spec.find("side");
    const Value* mod_id_value = spec.find("mod_id");
    if (side_value == nullptr || side_value->type != Type::String ||
        mod_id_value == nullptr || mod_id_value->type != Type::Number) {
        return fail(
            PC_RESULT_INVALID_ARGUMENT,
            "set_mod_fractured requires string \"side\" and number \"mod_id\"");
    }
    pc_mod_slot* slots = nullptr;
    uint8_t count = 0;
    if (side_value->string == "prefix") {
        slots = item->prefixes;
        count = item->prefix_count;
    } else if (side_value->string == "suffix") {
        slots = item->suffixes;
        count = item->suffix_count;
    } else {
        return fail(PC_RESULT_INVALID_ARGUMENT, "side must be prefix or suffix");
    }
    const uint32_t mod_id = static_cast<uint32_t>(mod_id_value->number);
    for (uint8_t index = 0; index < count; ++index) {
        if (slots[index].mod_id != mod_id) continue;
        slots[index].flags |= PC_MOD_SLOT_FRACTURED;
        return respond("{\"ok\":true}");
    }
    return fail(PC_RESULT_NOT_FOUND, "mod is not on this item side");
}

// Serialize the complete item state so the UI can persist it to the Stash /
// crash-recovery store and reconstruct it exactly later.
EMSCRIPTEN_KEEPALIVE
const char* pcw_item_export(uint32_t item_id) {
    pc_item_state* item = find(g_items, item_id);
    if (item == nullptr) return fail(PC_RESULT_NOT_FOUND, "unknown item");
    std::string out = "{\"ok\":true,\"state\":";
    pc_bestiary_craft_state* bestiary =
        sync_bestiary_state(item_id, *item);
    if (bestiary == nullptr)
        return fail(PC_RESULT_INTERNAL_ERROR, "Bestiary state unavailable");
    append_compound_item_state(out, *bestiary);
    out.push_back('}');
    return respond(std::move(out));
}

// Reconstruct an item from a previously exported state document and register it
// as a fresh handle. The input is the {"state":{...}} document or the bare
// state object.
EMSCRIPTEN_KEEPALIVE
const char* pcw_item_import(const char* state_json) {
    Value document;
    try {
        document = Parser(state_json, std::strlen(state_json)).parse();
    } catch (const std::exception& e) {
        return fail(PC_RESULT_INVALID_ARGUMENT, e.what());
    }
    const Value* state = document.find("state");
    if (state == nullptr) {
        state = &document;
    }
    if (state->type != Type::Object) {
        return fail(PC_RESULT_INVALID_ARGUMENT, "item state must be an object");
    }
    pc_item_state item = parse_item_state(*state);
    if (item.generic_influence_bits != 0 &&
        pc_item_find_fractured(&item, nullptr, nullptr) == PC_RESULT_OK) {
        return fail(
            PC_RESULT_INVALID_ARGUMENT,
            "ordinary influence and fractured modifiers are mutually exclusive");
    }
    bool checkpoint_present = false;
    pc_item_state checkpoint{};
    const Value* bestiary = state->find("bestiary");
    if (bestiary != nullptr && bestiary->type == Type::Object) {
        const Value* present = bestiary->find("checkpoint_present");
        checkpoint_present =
            present != nullptr && present->type == Type::Bool &&
            present->boolean;
        if (checkpoint_present) {
            const Value* saved = bestiary->find("checkpoint");
            if (saved == nullptr || saved->type != Type::Object) {
                return fail(
                    PC_RESULT_INVALID_ARGUMENT,
                    "active Bestiary checkpoint requires saved item state");
            }
            checkpoint = parse_item_state(*saved);
            if (checkpoint.generic_influence_bits != 0 &&
                pc_item_find_fractured(
                    &checkpoint, nullptr, nullptr) == PC_RESULT_OK) {
                return fail(
                    PC_RESULT_INVALID_ARGUMENT,
                    "checkpoint influence and fractured modifiers are mutually exclusive");
            }
        }
    }
    std::uint32_t id = g_next_id++;
    g_items[id] = item;
    reset_bestiary_state(id, item);
    if (checkpoint_present) {
        pc_bestiary_craft_state* compound = find(g_bestiary_states, id);
        if (compound == nullptr) {
            g_items.erase(id);
            return fail(PC_RESULT_INTERNAL_ERROR, "Bestiary state unavailable");
        }
        compound->checkpoint_present = 1;
        compound->checkpoint_bound_identity = id;
        compound->checkpoint = checkpoint;
    }
    std::string out = "{\"ok\":true,\"item\":";
    out += std::to_string(id);
    out.push_back('}');
    return respond(std::move(out));
}

EMSCRIPTEN_KEEPALIVE
const char* pcw_bestiary_apply(
    uint32_t data_id,
    uint32_t item_id,
    const char* action_id) {
    pc_data_handle* data = find(g_data, data_id);
    if (data == nullptr) return fail(PC_RESULT_NOT_FOUND, "unknown data handle");
    pc_item_state* item = find(g_items, item_id);
    if (item == nullptr) return fail(PC_RESULT_NOT_FOUND, "unknown item");
    if (action_id == nullptr)
        return fail(PC_RESULT_INVALID_ARGUMENT, "null Bestiary action id");
    pc_bestiary_craft_state* state =
        sync_bestiary_state(item_id, *item);
    if (state == nullptr)
        return fail(PC_RESULT_INTERNAL_ERROR, "Bestiary state unavailable");
    pc_bestiary_action_request request{
        sizeof(pc_bestiary_action_request), PC_ABI_VERSION, action_id};
    pc_bestiary_action_result result{};
    pc_error_info error = make_error();
    if (pc_bestiary_apply_action(
            *data, state, &request, &result, &error) != PC_RESULT_OK) {
        return fail(error);
    }
    *item = state->item;
    std::string out = "{\"ok\":true,\"result\":";
    append_bestiary_result(out, result);
    out.push_back('}');
    return respond(std::move(out));
}

EMSCRIPTEN_KEEPALIVE
const char* pcw_bestiary_calculate(
    uint32_t data_id,
    uint32_t item_id,
    const char* action_id) {
    pc_data_handle* data = find(g_data, data_id);
    if (data == nullptr) return fail(PC_RESULT_NOT_FOUND, "unknown data handle");
    pc_item_state* item = find(g_items, item_id);
    if (item == nullptr) return fail(PC_RESULT_NOT_FOUND, "unknown item");
    if (action_id == nullptr)
        return fail(PC_RESULT_INVALID_ARGUMENT, "null Bestiary action id");
    pc_bestiary_craft_state* state =
        sync_bestiary_state(item_id, *item);
    if (state == nullptr)
        return fail(PC_RESULT_INTERNAL_ERROR, "Bestiary state unavailable");
    pc_bestiary_action_request request{
        sizeof(pc_bestiary_action_request), PC_ABI_VERSION, action_id};
    pc_bestiary_calculation calculation{};
    pc_error_info error = make_error();
    if (pc_bestiary_calculate_action(
            *data, state, &request, &calculation, &error) != PC_RESULT_OK) {
        return fail(error);
    }
    std::string out =
        "{\"ok\":true,\"deterministic\":true,\"probability\":";
    out += std::to_string(calculation.probability);
    out += ",\"result\":";
    append_bestiary_result(out, calculation.result);
    out += ",\"successor\":";
    append_compound_item_state(out, calculation.successor);
    out.push_back('}');
    return respond(std::move(out));
}

EMSCRIPTEN_KEEPALIVE
const char* pcw_apply(uint32_t context_id, uint32_t item_id,
                      const char* action_json) {
    pc_action_context_handle* context = find(g_contexts, context_id);
    if (context == nullptr) return fail(PC_RESULT_NOT_FOUND, "unknown context");
    pc_item_state* item = find(g_items, item_id);
    if (item == nullptr) return fail(PC_RESULT_NOT_FOUND, "unknown item");
    Value spec;
    try {
        spec = Parser(action_json, std::strlen(action_json)).parse();
    } catch (const std::exception& e) {
        return fail(PC_RESULT_INVALID_ARGUMENT, e.what());
    }
    pc_action_request request;
    std::vector<std::string> storage;
    std::string error_message;
    if (!parse_action(spec, request, storage, error_message)) {
        return fail(PC_RESULT_INVALID_ARGUMENT, error_message.c_str());
    }
    pc_action_result result;
    result.struct_size = sizeof(result);
    pc_error_info error = make_error();
    pc_result rc = pc_apply_action(*context, item, &request, &result, &error);
    if (rc != PC_RESULT_OK) return fail(error);
    std::string out = "{\"ok\":true,\"result\":{\"applied\":";
    out += result.applied ? "true" : "false";
    out += ",\"added\":" + std::to_string(result.added);
    out += ",\"removed\":" + std::to_string(result.removed);
    out += "}}";
    return respond(std::move(out));
}

// Coarse-grained batch primitive for strategy runs: apply the action to `count`
// independent copies of the item and return only the aggregate summary. The
// source item is not mutated. The worker calls this in chunks so it can post
// progress and honour cancellation between chunks.
EMSCRIPTEN_KEEPALIVE
const char* pcw_run_batch(uint32_t context_id, uint32_t item_id,
                          const char* action_json, uint32_t count) {
    pc_action_context_handle* context = find(g_contexts, context_id);
    if (context == nullptr) return fail(PC_RESULT_NOT_FOUND, "unknown context");
    pc_item_state* item = find(g_items, item_id);
    if (item == nullptr) return fail(PC_RESULT_NOT_FOUND, "unknown item");
    Value spec;
    try {
        spec = Parser(action_json, std::strlen(action_json)).parse();
    } catch (const std::exception& e) {
        return fail(PC_RESULT_INVALID_ARGUMENT, e.what());
    }
    pc_action_request request;
    std::vector<std::string> storage;
    std::string error_message;
    if (!parse_action(spec, request, storage, error_message)) {
        return fail(PC_RESULT_INVALID_ARGUMENT, error_message.c_str());
    }
    std::vector<pc_item_state> items(count ? count : 1, *item);
    pc_batch_summary summary;
    summary.struct_size = sizeof(summary);
    pc_error_info error = make_error();
    pc_result rc = pc_apply_action_batch(*context, items.data(), count, &request,
                                         nullptr, &summary, &error);
    if (rc != PC_RESULT_OK) return fail(error);
    std::string out = "{\"ok\":true,\"summary\":{\"item_count\":";
    out += std::to_string(summary.item_count);
    out += ",\"applied_count\":" + std::to_string(summary.applied_count);
    out += ",\"total_added\":";
    append_u64(out, summary.total_added);
    out += ",\"total_removed\":";
    append_u64(out, summary.total_removed);
    out += "}}";
    return respond(std::move(out));
}

EMSCRIPTEN_KEEPALIVE
const char* pcw_debug_pool(uint32_t context_id, uint32_t item_id,
                           const char* query_json) {
    pc_action_context_handle* context = find(g_contexts, context_id);
    if (context == nullptr) return fail(PC_RESULT_NOT_FOUND, "unknown context");
    pc_item_state* item = find(g_items, item_id);
    if (item == nullptr) return fail(PC_RESULT_NOT_FOUND, "unknown item");
    Value query;
    try {
        query = Parser(query_json, std::strlen(query_json)).parse();
    } catch (const std::exception& e) {
        return fail(PC_RESULT_INVALID_ARGUMENT, e.what());
    }
    const Value* action = query.find("action");
    if (action == nullptr || action->type != Type::Object) {
        return fail(PC_RESULT_INVALID_ARGUMENT, "pool query requires an \"action\" object");
    }
    pc_action_request request;
    std::vector<std::string> storage;
    std::string error_message;
    if (!parse_action(*action, request, storage, error_message)) {
        return fail(PC_RESULT_INVALID_ARGUMENT, error_message.c_str());
    }
    int32_t side_filter = -1;
    const Value* side = query.find("side");
    if (side != nullptr && side->type == Type::String) {
        if (side->string == "prefix") side_filter = 0;
        else if (side->string == "suffix") side_filter = 1;
        else if (side->string != "both")
            return fail(PC_RESULT_INVALID_ARGUMENT, "side must be both, prefix, or suffix");
    }
    const Value* include_rejected = query.find("include_rejected");
    pc_pool_query_request pool_request;
    pool_request.struct_size = sizeof(pool_request);
    pool_request.abi_version = PC_ABI_VERSION;
    pool_request.action = request;
    pool_request.side_filter = side_filter;
    pool_request.include_rejected =
        include_rejected != nullptr && include_rejected->type == Type::Bool &&
                include_rejected->boolean
            ? 1
            : 0;

    uint32_t needed = 0;
    pc_pool_debug_summary summary;
    summary.struct_size = sizeof(summary);
    pc_error_info error = make_error();
    pc_result rc = pc_debug_pool_query(*context, item, &pool_request, nullptr, 0,
                                       &needed, &summary, &error);
    if (rc != PC_RESULT_OK && rc != PC_RESULT_BUFFER_TOO_SMALL) {
        return fail(error);
    }
    std::vector<pc_pool_debug_entry> entries(needed ? needed : 1);
    for (auto& entry : entries) entry.struct_size = sizeof(entry);
    error = make_error();
    rc = pc_debug_pool_query(*context, item, &pool_request, entries.data(),
                             needed, &needed, &summary, &error);
    if (rc != PC_RESULT_OK) return fail(error);

    std::string out = "{\"ok\":true,\"entries\":[";
    for (uint32_t i = 0; i < needed; ++i) {
        const pc_pool_debug_entry& entry = entries[i];
        if (i != 0) out.push_back(',');
        out += "{\"session_mod_id\":" + std::to_string(entry.session_mod_id);
        out += ",\"global_mod_id\":" + std::to_string(entry.global_mod_id);
        out += ",\"key\":";
        append_escaped(out, entry.key);
        out += ",\"generation_type\":" + std::to_string(entry.generation_type);
        out += ",\"accepted\":";
        out += entry.first_failure == 0 ? "true" : "false";
        out += ",\"first_failure\":" + std::to_string(entry.first_failure);
        out += ",\"spawn_weight\":" + std::to_string(entry.active_spawn_weight);
        out += ",\"generation_multiplier_pct\":" + std::to_string(entry.active_generation_pct);
        out += ",\"special_multiplier_pct\":" + std::to_string(entry.special_multiplier_pct);
        out += ",\"final_weight\":" + std::to_string(entry.final_weight);
        out.push_back('}');
    }
    out += "],\"summary\":{\"tag_signature_id\":" + std::to_string(summary.tag_signature_id);
    out += ",\"cache_hit\":";
    out += summary.cache_hit ? "true" : "false";
    out += ",\"candidate_count\":" + std::to_string(summary.candidate_count);
    out += ",\"prefix_total_weight\":";
    append_u64(out, summary.prefix_total_weight);
    out += ",\"suffix_total_weight\":";
    append_u64(out, summary.suffix_total_weight);
    out += ",\"combined_total_weight\":";
    append_u64(out, summary.combined_total_weight);
    out += "}}";
    return respond(std::move(out));
}

EMSCRIPTEN_KEEPALIVE
const char* pcw_strategy_compile(uint32_t session_id,
                                 const char* strategy_json) {
    pc_session_handle* session = find(g_sessions, session_id);
    if (session == nullptr) return fail(PC_RESULT_NOT_FOUND, "unknown session");
    pc_strategy_handle strategy = nullptr;
    pc_error_info error = make_error();
    pc_result rc = pc_strategy_compile_json(
        *session, strategy_json, std::strlen(strategy_json), &strategy, &error);
    if (rc != PC_RESULT_OK) return fail(error);
    const uint32_t id = g_next_id++;
    g_strategies[id] = strategy;
    return respond(
        "{\"ok\":true,\"strategy\":" + std::to_string(id) + "}");
}

EMSCRIPTEN_KEEPALIVE
void pcw_strategy_close(uint32_t strategy_id) {
    auto it = g_strategies.find(strategy_id);
    if (it == g_strategies.end()) return;
    pc_strategy_destroy(it->second);
    g_strategies.erase(it);
}

EMSCRIPTEN_KEEPALIVE
const char* pcw_strategy_evaluate(uint32_t strategy_id,
                                  const char* options_json) {
    pc_strategy_handle* strategy = find(g_strategies, strategy_id);
    if (strategy == nullptr) {
        return fail(PC_RESULT_NOT_FOUND, "unknown strategy");
    }

    pc_strategy_eval_options options{};
    std::string review_projection_storage;
    std::string parse_error;
    if (!parse_strategy_eval_options(
            options_json, options, review_projection_storage, parse_error)) {
        return fail(PC_RESULT_INVALID_ARGUMENT, parse_error.c_str());
    }

    pc_error_info error = make_error();
    size_t length = 0;
    pc_result rc = pc_strategy_evaluate(
        *strategy, &options, nullptr, 0, &length, &error);
    if (rc != PC_RESULT_OK) return fail(error);
    std::string result(length + 1, '\0');
    rc = pc_strategy_evaluate(
        *strategy, &options, result.data(), result.size(), &length, &error);
    if (rc != PC_RESULT_OK) return fail(error);
    result.resize(length);

    std::string out = "{\"ok\":true,\"result\":";
    out += result;
    out.push_back('}');
    return respond(std::move(out));
}

EMSCRIPTEN_KEEPALIVE
const char* pcw_strategy_eval_begin(uint32_t strategy_id,
                                    const char* options_json) {
    pc_strategy_handle* strategy = find(g_strategies, strategy_id);
    if (strategy == nullptr) {
        return fail(PC_RESULT_NOT_FOUND, "unknown strategy");
    }
    pc_strategy_eval_options options{};
    std::string review_projection_storage;
    std::string parse_error;
    if (!parse_strategy_eval_options(
            options_json, options, review_projection_storage, parse_error)) {
        return fail(PC_RESULT_INVALID_ARGUMENT, parse_error.c_str());
    }
    pc_strategy_eval_work_handle work = nullptr;
    pc_error_info error = make_error();
    const pc_result rc =
        pc_strategy_eval_begin(*strategy, &options, &work, &error);
    if (rc != PC_RESULT_OK) return fail(error);
    const std::uint32_t id = g_next_id++;
    g_evaluations[id] = work;
    return respond(
        "{\"ok\":true,\"evaluation\":" + std::to_string(id) + '}');
}

EMSCRIPTEN_KEEPALIVE
const char* pcw_strategy_eval_step(uint32_t evaluation_id,
                                   uint32_t max_work_items) {
    pc_strategy_eval_work_handle* work =
        find(g_evaluations, evaluation_id);
    if (work == nullptr) {
        return fail(PC_RESULT_NOT_FOUND, "unknown strategy evaluation");
    }
    pc_strategy_eval_progress progress{};
    pc_error_info error = make_error();
    const pc_result rc = pc_strategy_eval_step(
        *work, max_work_items, &progress, &error);
    if (rc != PC_RESULT_OK) return fail(error);
    std::string out = "{\"ok\":true,\"progress\":";
    append_strategy_eval_progress(out, progress);
    out.push_back('}');
    return respond(std::move(out));
}

EMSCRIPTEN_KEEPALIVE
const char* pcw_strategy_eval_finish(uint32_t evaluation_id) {
    pc_strategy_eval_work_handle* work =
        find(g_evaluations, evaluation_id);
    if (work == nullptr) {
        return fail(PC_RESULT_NOT_FOUND, "unknown strategy evaluation");
    }
    pc_error_info error = make_error();
    std::size_t length = 0;
    pc_result rc = pc_strategy_eval_finish(
        *work, nullptr, 0, &length, &error);
    if (rc != PC_RESULT_OK) return fail(error);
    std::string result(length + 1, '\0');
    rc = pc_strategy_eval_finish(
        *work, result.data(), result.size(), &length, &error);
    if (rc != PC_RESULT_OK) return fail(error);
    result.resize(length);
    std::string out = "{\"ok\":true,\"result\":";
    out += result;
    out.push_back('}');
    return respond(std::move(out));
}

EMSCRIPTEN_KEEPALIVE
void pcw_strategy_eval_close(uint32_t evaluation_id) {
    auto it = g_evaluations.find(evaluation_id);
    if (it == g_evaluations.end()) return;
    pc_strategy_eval_destroy(it->second);
    g_evaluations.erase(it);
}

EMSCRIPTEN_KEEPALIVE
uint32_t pcw_live_handle_count() {
    return static_cast<std::uint32_t>(
        g_data.size() + g_sessions.size() + g_contexts.size() +
        g_items.size() + g_strategies.size() + g_economies.size() +
        g_simulators.size() + g_solvers.size() + g_evaluations.size());
}

EMSCRIPTEN_KEEPALIVE
const char* pcw_memory_stats() {
    const auto registry_bytes = [](const auto& registry) {
        return static_cast<std::uint64_t>(
            registry.bucket_count() * sizeof(void*) +
            registry.size() *
                (sizeof(typename std::decay_t<decltype(registry)>::value_type) +
                 2 * sizeof(void*)));
    };
    std::uint64_t registry_owned = g_response.capacity() + 1;
    registry_owned += registry_bytes(g_data);
    registry_owned += registry_bytes(g_sessions);
    registry_owned += registry_bytes(g_contexts);
    registry_owned += registry_bytes(g_items);
    registry_owned += registry_bytes(g_bestiary_states);
    registry_owned += registry_bytes(g_strategies);
    registry_owned += registry_bytes(g_economies);
    registry_owned += registry_bytes(g_simulators);
    registry_owned += registry_bytes(g_solvers);
    registry_owned += registry_bytes(g_evaluations);

    std::uint64_t live = registry_owned;
    std::uint64_t peak = registry_owned;
    std::uint64_t serialized = g_response.capacity();
    pc_error_info error = make_error();
    for (const auto& [unused, solver] : g_solvers) {
        (void)unused;
        pc_native_memory_stats stats{};
        if (pc_solver_memory_stats(solver, &stats, &error) == PC_RESULT_OK) {
            live += stats.live_owned_bytes;
            peak += stats.peak_owned_bytes;
            serialized += stats.serialized_output_bytes;
        }
    }
    for (const auto& [unused, evaluation] : g_evaluations) {
        (void)unused;
        pc_native_memory_stats stats{};
        if (pc_strategy_eval_memory_stats(
                evaluation, &stats, &error) == PC_RESULT_OK) {
            live += stats.live_owned_bytes;
            peak += stats.peak_owned_bytes;
            serialized += stats.serialized_output_bytes;
        }
    }

    std::string out = "{\"ok\":true,\"live_handles\":";
    append_u64(out, pcw_live_handle_count());
    out += ",\"native_live_owned_bytes\":";
    append_u64(out, live);
    out += ",\"native_peak_owned_bytes\":";
    append_u64(out, std::max(live, peak));
    out += ",\"native_serialized_output_bytes\":";
    append_u64(out, serialized);
    out += ",\"scope\":\"facade_registries_plus_solver_and_evaluator_owned_allocations\"}";
    return respond(std::move(out));
}

EMSCRIPTEN_KEEPALIVE
const char* pcw_economy_open(const char* economy_json) {
    pc_economy_handle economy = nullptr;
    pc_error_info error = make_error();
    pc_result rc = pc_economy_load_json(
        economy_json, std::strlen(economy_json), &economy, &error);
    if (rc != PC_RESULT_OK) return fail(error);
    const uint32_t id = g_next_id++;
    g_economies[id] = economy;
    return respond(
        "{\"ok\":true,\"economy\":" + std::to_string(id) + "}");
}

EMSCRIPTEN_KEEPALIVE
void pcw_economy_close(uint32_t economy_id) {
    auto it = g_economies.find(economy_id);
    if (it == g_economies.end()) return;
    pc_economy_destroy(it->second);
    g_economies.erase(it);
}

EMSCRIPTEN_KEEPALIVE
const char* pcw_simulator_open(uint32_t session_id, uint32_t strategy_id,
                               uint32_t economy_id) {
    pc_session_handle* session = find(g_sessions, session_id);
    pc_strategy_handle* strategy = find(g_strategies, strategy_id);
    if (session == nullptr) return fail(PC_RESULT_NOT_FOUND, "unknown session");
    if (strategy == nullptr) return fail(PC_RESULT_NOT_FOUND, "unknown strategy");
    pc_economy_handle economy = nullptr;
    if (economy_id != 0) {
        pc_economy_handle* found = find(g_economies, economy_id);
        if (found == nullptr) return fail(PC_RESULT_NOT_FOUND, "unknown economy");
        economy = *found;
    }
    pc_simulator_handle simulator = nullptr;
    pc_error_info error = make_error();
    pc_result rc = pc_simulator_create(
        *session, *strategy, economy, &simulator, &error);
    if (rc != PC_RESULT_OK) return fail(error);
    const uint32_t id = g_next_id++;
    g_simulators[id] = simulator;
    return respond(
        "{\"ok\":true,\"simulator\":" + std::to_string(id) + "}");
}

EMSCRIPTEN_KEEPALIVE
void pcw_simulator_close(uint32_t simulator_id) {
    auto it = g_simulators.find(simulator_id);
    if (it == g_simulators.end()) return;
    pc_simulator_destroy(it->second);
    g_simulators.erase(it);
}

EMSCRIPTEN_KEEPALIVE
const char* pcw_simulator_run_chunk(uint32_t simulator_id,
                                    const char* options_json,
                                    uint32_t max_completed_runs) {
    pc_simulator_handle* simulator = find(g_simulators, simulator_id);
    if (simulator == nullptr)
        return fail(PC_RESULT_NOT_FOUND, "unknown simulator");
    Value spec;
    try {
        spec = Parser(options_json, std::strlen(options_json)).parse();
    } catch (const std::exception& e) {
        return fail(PC_RESULT_INVALID_ARGUMENT, e.what());
    }
    if (spec.type != Type::Object) {
        return fail(PC_RESULT_INVALID_ARGUMENT, "options must be an object");
    }
    pc_simulation_options options{};
    options.struct_size = sizeof(options);
    options.abi_version = PC_ABI_VERSION;
    options.target_runs = obj_u64(spec, "target_runs");
    options.seed = obj_u64(spec, "seed");
    options.max_actions_per_run =
        obj_u32(spec, "max_actions_per_run", 100000);
    options.max_graph_steps_per_run =
        obj_u32(spec, "max_graph_steps_per_run");
    options.max_cost_per_run = obj_double(spec, "max_cost_per_run");
    options.retained_trace_count =
        obj_u32(spec, "retained_trace_count", 10);
    options.max_trace_entries = obj_u32(spec, "max_trace_entries", 512);
    options.retained_success_count =
        obj_u32(spec, "retained_success_count", 5);
    options.retained_failure_count =
        obj_u32(spec, "retained_failure_count", 5);
    pc_simulation_progress progress{};
    pc_error_info error = make_error();
    pc_result rc = pc_simulator_run_chunk(
        *simulator, &options, max_completed_runs, &progress, &error);
    if (rc != PC_RESULT_OK) return fail(error);
    std::string out = "{\"ok\":true,\"progress\":{\"completed_runs\":";
    append_u64(out, progress.completed_runs);
    out += ",\"target_runs\":";
    append_u64(out, progress.target_runs);
    out += ",\"finished\":";
    out += progress.finished ? "true" : "false";
    out += "}}";
    return respond(std::move(out));
}

EMSCRIPTEN_KEEPALIVE
const char* pcw_simulator_result(uint32_t simulator_id) {
    pc_simulator_handle* simulator = find(g_simulators, simulator_id);
    if (simulator == nullptr)
        return fail(PC_RESULT_NOT_FOUND, "unknown simulator");
    pc_error_info error = make_error();
    pc_simulation_summary summary{};
    pc_result rc = pc_simulator_get_summary(*simulator, &summary, &error);
    if (rc != PC_RESULT_OK) return fail(error);

    std::string out = "{\"ok\":true,\"summary\":{\"completed_runs\":";
    append_u64(out, summary.completed_runs);
    out += ",\"success_count\":";
    append_u64(out, summary.success_count);
    out += ",\"failure_count\":";
    append_u64(out, summary.failure_count);
    out += ",\"stop_count\":";
    append_u64(out, summary.stop_count);
    out += ",\"total_actions\":";
    append_u64(out, summary.total_actions);
    out += ",\"action_limit_count\":";
    append_u64(out, summary.action_limit_count);
    out += ",\"cost_limit_count\":";
    append_u64(out, summary.cost_limit_count);
    out += ",\"step_limit_count\":";
    append_u64(out, summary.step_limit_count);
    out += ",\"no_matching_edge_count\":";
    append_u64(out, summary.no_matching_edge_count);
    out += ",\"action_not_applied_count\":";
    append_u64(out, summary.action_not_applied_count);
    out += ",\"missing_price_run_count\":";
    append_u64(out, summary.missing_price_run_count);
    out += ",\"costed_action_count\":";
    append_u64(out, summary.costed_action_count);
    out += ",\"missing_price_action_count\":";
    append_u64(out, summary.missing_price_action_count);
    out += ",\"known_total_cost\":" + std::to_string(summary.known_total_cost);
    out += ",\"cost_status\":";
    append_escaped(
        out,
        summary.cost_status == PC_COST_DISABLED
            ? "disabled"
            : (summary.cost_status == PC_COST_COMPLETE ? "complete"
                                                       : "incomplete"));
    out += ",\"seed\":";
    append_u64(out, summary.seed);
    out += ",\"target_runs\":";
    append_u64(out, summary.target_runs);
    out += "},\"action_distribution\":[";

    uint32_t distribution_count = 0;
    rc = pc_simulator_action_distribution_query(
        *simulator, nullptr, 0, &distribution_count, &error);
    if (rc != PC_RESULT_OK && rc != PC_RESULT_BUFFER_TOO_SMALL)
        return fail(error);
    std::vector<pc_action_distribution_entry> distribution(
        distribution_count ? distribution_count : 1);
    rc = pc_simulator_action_distribution_query(
        *simulator, distribution.data(), distribution_count,
        &distribution_count, &error);
    if (rc != PC_RESULT_OK) return fail(error);
    for (uint32_t i = 0; i < distribution_count; ++i) {
        if (i != 0) out.push_back(',');
        out += "{\"node_id\":";
        append_escaped(out, distribution[i].node_id);
        out += ",\"action_type\":" +
               std::to_string(distribution[i].action_type);
        out += ",\"count\":";
        append_u64(out, distribution[i].count);
        out.push_back('}');
    }
    out += "],\"sampled_accounting\":{\"evidence_source\":"
           "\"simulator_sample\",\"sample_count\":";
    append_u64(out, summary.completed_runs);
    out += ",\"seed\":";
    append_u64(out, summary.seed);
    out += ",\"actions\":[";
    uint32_t descriptor_count = 0;
    rc = pc_simulator_action_descriptor_distribution_query(
        *simulator, nullptr, 0, &descriptor_count, &error);
    if (rc != PC_RESULT_OK && rc != PC_RESULT_BUFFER_TOO_SMALL)
        return fail(error);
    std::vector<pc_action_descriptor_sample_entry> descriptors(
        descriptor_count ? descriptor_count : 1);
    rc = pc_simulator_action_descriptor_distribution_query(
        *simulator, descriptors.data(), descriptor_count,
        &descriptor_count, &error);
    if (rc != PC_RESULT_OK) return fail(error);
    for (uint32_t i = 0; i < descriptor_count; ++i) {
        if (i != 0) out.push_back(',');
        out += "{\"action_id\":";
        append_escaped(out, descriptors[i].action_id);
        out += ",\"count\":";
        append_u64(out, descriptors[i].count);
        out += ",\"average_per_invocation\":";
        out += summary.completed_runs == 0
                   ? "0"
                   : std::to_string(
                         static_cast<double>(descriptors[i].count) /
                         static_cast<double>(summary.completed_runs));
        out.push_back('}');
    }
    out += "],\"materials\":[";
    uint32_t material_count = 0;
    rc = pc_simulator_material_distribution_query(
        *simulator, nullptr, 0, &material_count, &error);
    if (rc != PC_RESULT_OK && rc != PC_RESULT_BUFFER_TOO_SMALL)
        return fail(error);
    std::vector<pc_material_sample_entry> materials(
        material_count ? material_count : 1);
    rc = pc_simulator_material_distribution_query(
        *simulator, materials.data(), material_count, &material_count,
        &error);
    if (rc != PC_RESULT_OK) return fail(error);
    for (uint32_t i = 0; i < material_count; ++i) {
        if (i != 0) out.push_back(',');
        out += "{\"price_key\":";
        append_escaped(out, materials[i].price_key);
        out += ",\"count\":";
        append_u64(out, materials[i].count);
        out += ",\"average_per_invocation\":";
        out += summary.completed_runs == 0
                   ? "0"
                   : std::to_string(
                         static_cast<double>(materials[i].count) /
                         static_cast<double>(summary.completed_runs));
        out.push_back('}');
    }
    out += "]},\"traces\":[";

    uint32_t trace_count = 0;
    rc = pc_simulator_get_trace_count(*simulator, &trace_count, &error);
    if (rc != PC_RESULT_OK) return fail(error);
    for (uint32_t trace_index = 0; trace_index < trace_count; ++trace_index) {
        uint32_t count = 0;
        rc = pc_simulator_trace_query(
            *simulator, trace_index, nullptr, 0, &count, &error);
        if (rc != PC_RESULT_OK && rc != PC_RESULT_BUFFER_TOO_SMALL)
            return fail(error);
        std::vector<pc_trace_entry> entries(count ? count : 1);
        rc = pc_simulator_trace_query(
            *simulator, trace_index, entries.data(), count, &count, &error);
        if (rc != PC_RESULT_OK) return fail(error);
        if (trace_index != 0) out.push_back(',');
        out += "{\"entries\":[";
        for (uint32_t i = 0; i < count; ++i) {
            if (i != 0) out.push_back(',');
            const auto& entry = entries[i];
            out += "{\"step_index\":" + std::to_string(entry.step_index);
            out += ",\"node_id\":";
            append_escaped(out, entry.node_id);
            out += ",\"node_kind\":" + std::to_string(entry.node_kind);
            out += ",\"action_type\":" + std::to_string(entry.action_type);
            out += ",\"action_applied\":";
            out += entry.action_applied ? "true" : "false";
            out += ",\"matched_edge_id\":";
            append_escaped(out, entry.matched_edge_id);
            out += ",\"cumulative_actions\":";
            append_u64(out, entry.cumulative_actions);
            out += ",\"known_cumulative_cost\":" +
                   std::to_string(entry.known_cumulative_cost);
            out += ",\"cost_complete\":";
            out += entry.cost_complete ? "true" : "false";
            out += ",\"terminal_kind\":";
            if (entry.terminal_kind < 0) {
                out += "null";
            } else {
                append_escaped(out, terminal_name(entry.terminal_kind));
            }
            out += ",\"failure_reason\":" +
                   std::to_string(entry.failure_reason);
            out += ",\"item\":";
            append_item_state(out, entry.item);
            out.push_back('}');
        }
        out += "]}";
    }
    out += "],\"examples\":{";

    const int terminal_kinds[] = {
        PC_TERMINAL_SUCCESS, PC_TERMINAL_FAILURE, PC_TERMINAL_STOP};
    for (std::size_t kind_index = 0; kind_index < 3; ++kind_index) {
        const int kind = terminal_kinds[kind_index];
        if (kind_index != 0) out.push_back(',');
        append_escaped(out, terminal_name(kind));
        out += ":[";
        uint32_t count = 0;
        rc = pc_simulator_get_example_count(
            *simulator, kind, &count, &error);
        if (rc != PC_RESULT_OK) return fail(error);
        for (uint32_t i = 0; i < count; ++i) {
            pc_simulation_example example{};
            rc = pc_simulator_example_query(
                *simulator, kind, i, &example, &error);
            if (rc != PC_RESULT_OK) return fail(error);
            if (i != 0) out.push_back(',');
            out += "{\"terminal_kind\":";
            append_escaped(out, terminal_name(example.terminal_kind));
            out += ",\"failure_reason\":" +
                   std::to_string(example.failure_reason);
            out += ",\"terminal_node_id\":";
            append_escaped(out, example.terminal_node_id);
            out += ",\"action_count\":";
            append_u64(out, example.action_count);
            out += ",\"known_total_cost\":" +
                   std::to_string(example.known_total_cost);
            out += ",\"cost_complete\":";
            out += example.cost_complete ? "true" : "false";
            out += ",\"item\":";
            append_item_state(out, example.item);
            out.push_back('}');
        }
        out.push_back(']');
    }
    out += "},\"failure_summaries\":[";

    uint32_t failure_count = 0;
    rc = pc_simulator_failure_summary_query(
        *simulator, nullptr, 0, &failure_count, &error);
    if (rc != PC_RESULT_OK && rc != PC_RESULT_BUFFER_TOO_SMALL)
        return fail(error);
    std::vector<pc_failure_summary_entry> failures(
        failure_count ? failure_count : 1);
    rc = pc_simulator_failure_summary_query(
        *simulator, failures.data(), failure_count, &failure_count, &error);
    if (rc != PC_RESULT_OK) return fail(error);
    for (uint32_t i = 0; i < failure_count; ++i) {
        if (i != 0) out.push_back(',');
        out += "{\"failure_reason\":" +
               std::to_string(failures[i].failure_reason);
        out += ",\"node_id\":";
        append_escaped(out, failures[i].node_id);
        out += ",\"detail\":";
        append_escaped(out, failures[i].detail);
        out += ",\"count\":";
        append_u64(out, failures[i].count);
        out.push_back('}');
    }
    out += "],\"missing_prices\":[";

    uint32_t missing_count = 0;
    rc = pc_simulator_missing_price_query(
        *simulator, nullptr, 0, &missing_count, &error);
    if (rc != PC_RESULT_OK && rc != PC_RESULT_BUFFER_TOO_SMALL)
        return fail(error);
    std::vector<pc_price_key_entry> missing(missing_count ? missing_count : 1);
    rc = pc_simulator_missing_price_query(
        *simulator, missing.data(), missing_count, &missing_count, &error);
    if (rc != PC_RESULT_OK) return fail(error);
    for (uint32_t i = 0; i < missing_count; ++i) {
        if (i != 0) out.push_back(',');
        out += "{\"key\":";
        append_escaped(out, missing[i].key);
        out += ",\"missing_count\":";
        append_u64(out, missing[i].missing_count);
        out.push_back('}');
    }
    out += "]}";
    return respond(std::move(out));
}

// --- solver / calculation engine ---------------------------------------------

EMSCRIPTEN_KEEPALIVE
const char* pcw_solver_open(uint32_t session_id, const char* goal_json) {
    pc_session_handle* session = find(g_sessions, session_id);
    if (session == nullptr) return fail(PC_RESULT_NOT_FOUND, "unknown session");
    if (goal_json == nullptr) {
        return fail(PC_RESULT_INVALID_ARGUMENT, "goal JSON is required");
    }
    pc_solver_handle solver = nullptr;
    pc_error_info error = make_error();
    pc_result rc = pc_solver_create(
        *session, goal_json, std::strlen(goal_json), &solver, &error);
    if (rc != PC_RESULT_OK) return fail(error);
    std::uint32_t id = g_next_id++;
    g_solvers[id] = solver;
    std::string out = "{\"ok\":true,\"solver\":";
    out += std::to_string(id);
    out.push_back('}');
    return respond(std::move(out));
}

EMSCRIPTEN_KEEPALIVE
void pcw_solver_close(uint32_t solver_id) {
    pc_solver_handle* solver = find(g_solvers, solver_id);
    if (solver != nullptr) {
        pc_solver_destroy(*solver);
        g_solvers.erase(solver_id);
    }
}

/* Candidate actions the goal is scoped to, with ids and cost vectors, for
 * the Calculator's action picker. */
EMSCRIPTEN_KEEPALIVE
const char* pcw_solver_actions(uint32_t solver_id) {
    pc_solver_handle* solver = find(g_solvers, solver_id);
    if (solver == nullptr) return fail(PC_RESULT_NOT_FOUND, "unknown solver");
    pc_error_info error = make_error();
    uint32_t count = 0;
    pc_result rc =
        pc_solver_candidates(*solver, nullptr, 0, &count, &error);
    if (rc != PC_RESULT_OK) return fail(error);
    std::vector<uint32_t> indices(count);
    rc = pc_solver_candidates(*solver, indices.data(), count, &count, &error);
    if (rc != PC_RESULT_OK) return fail(error);
    std::string out = "{\"ok\":true,\"actions\":[";
    for (uint32_t i = 0; i < count; ++i) {
        pc_solver_action_info info;
        info.struct_size = sizeof(info);
        rc = pc_solver_get_action_info(*solver, indices[i], &info, &error);
        if (rc != PC_RESULT_OK) return fail(error);
        if (i != 0) out.push_back(',');
        out += "{\"index\":" + std::to_string(info.action_index);
        out += ",\"id\":";
        append_escaped(out, info.id);
        out += ",\"display_name\":";
        append_escaped(out, info.display_name);
        out += ",\"transition_kind\":" +
               std::to_string(info.transition_kind);
        out += ",\"synthetic\":";
        out += info.synthetic ? "true" : "false";
        out += ",\"cost_keys\":[";
        for (uint32_t k = 0; k < info.cost_key_count; ++k) {
            if (k != 0) out.push_back(',');
            append_escaped(out, info.cost_keys[k]);
        }
        out += "],\"preservation\":{";
        const auto append_properties = [&](const char* key,
                                           std::uint32_t mask) {
            out += "\"" + std::string(key) + "\":[";
            bool first = true;
            const auto append_property = [&](std::uint32_t bit,
                                             const char* name) {
                if ((mask & bit) == 0) return;
                if (!first) out.push_back(',');
                first = false;
                append_escaped(out, name);
            };
            append_property(1u << 0, "goal_families");
            append_property(1u << 1, "satisfied_goal_subset");
            append_property(1u << 2, "junk_blockers");
            append_property(1u << 3, "crafted_state");
            append_property(1u << 4, "fractured_state");
            append_property(1u << 5, "prefix_side");
            append_property(1u << 6, "suffix_side");
            append_property(1u << 7, "active_protection");
            out.push_back(']');
        };
        append_properties("can_preserve", info.can_preserve);
        out.push_back(',');
        append_properties("can_destroy", info.can_destroy);
        out.push_back(',');
        append_properties("can_create", info.can_create);
        out.push_back(',');
        append_properties(
            "can_make_unreachable", info.can_make_unreachable);
        out += ",\"destructive_renewal\":";
        out += info.destructive_renewal ? "true" : "false";
        out += ",\"preserves_fractured_affixes\":";
        out += info.preserves_fractured_affixes ? "true" : "false";
        out += ",\"protection\":{";
        out += "\"prefix_lock\":";
        out += info.respects_prefix_lock ? "true" : "false";
        out += ",\"suffix_lock\":";
        out += info.respects_suffix_lock ? "true" : "false";
        out += ",\"cannot_roll_attack\":";
        out += info.respects_cannot_roll_attack ? "true" : "false";
        out += ",\"cannot_roll_caster\":";
        out += info.respects_cannot_roll_caster ? "true" : "false";
        out += "}}}";
    }
    out += "]}";
    return respond(std::move(out));
}

/* Exact outcome distribution for one action on a live item: the
 * Calculator's "odds before you click". */
EMSCRIPTEN_KEEPALIVE
const char* pcw_solver_calc(uint32_t solver_id, uint32_t item_id,
                            const char* action_id) {
    pc_solver_handle* solver = find(g_solvers, solver_id);
    if (solver == nullptr) return fail(PC_RESULT_NOT_FOUND, "unknown solver");
    pc_item_state* item = find(g_items, item_id);
    if (item == nullptr) return fail(PC_RESULT_NOT_FOUND, "unknown item");
    if (action_id == nullptr) {
        return fail(PC_RESULT_INVALID_ARGUMENT, "action id is required");
    }
    pc_error_info error = make_error();
    uint32_t action_index = 0;
    pc_result rc =
        pc_solver_find_action(*solver, action_id, &action_index, &error);
    if (rc != PC_RESULT_OK) return fail(error);
    uint32_t count = 0;
    pc_calc_summary summary{};
    summary.struct_size = sizeof(summary);
    rc = pc_calc_action_outcomes(*solver, item, action_index, nullptr, 0,
                                 &count, &summary, &error);
    if (rc != PC_RESULT_OK) return fail(error);
    std::vector<pc_calc_outcome> outcomes(count);
    if (count > 0) {
        rc = pc_calc_action_outcomes(*solver, item, action_index,
                                     outcomes.data(), count, &count,
                                     &summary, &error);
        if (rc != PC_RESULT_OK) return fail(error);
    }
    std::string out = "{\"ok\":true,\"supported\":";
    out += summary.supported ? "true" : "false";
    out += ",\"legal\":";
    out += summary.legal ? "true" : "false";
    out += ",\"success_probability\":";
    out += std::to_string(summary.success_probability);
    out += ",\"slot_satisfied\":[";
    for (uint32_t s = 0; s < PC_SOLVER_MAX_GOAL_SLOTS; ++s) {
        if (s != 0) out.push_back(',');
        out += std::to_string(summary.slot_satisfied_probability[s]);
    }
    out += "],\"outcomes\":[";
    for (uint32_t i = 0; i < count; ++i) {
        const pc_calc_outcome& entry = outcomes[i];
        if (i != 0) out.push_back(',');
        out += "{\"state\":" + std::to_string(entry.state_id);
        out += ",\"probability\":" + std::to_string(entry.probability);
        out += ",\"rarity\":" + std::to_string(entry.rarity);
        out += ",\"prefixes\":" + std::to_string(entry.prefix_count);
        out += ",\"suffixes\":" + std::to_string(entry.suffix_count);
        out += ",\"flags\":" + std::to_string(entry.flags);
        out += ",\"blocked\":" + std::to_string(entry.blocked_mask);
        out += ",\"slots\":[";
        for (uint32_t s = 0; s < PC_SOLVER_MAX_GOAL_SLOTS; ++s) {
            if (s != 0) out.push_back(',');
            out += std::to_string(entry.slot_status[s]);
        }
        out += "]}";
    }
    out += "]}";
    return respond(std::move(out));
}

EMSCRIPTEN_KEEPALIVE
const char* pcw_solver_solve_begin(uint32_t solver_id, uint32_t item_id,
                                   uint32_t economy_id,
                                   const char* options_json) {
    pc_solver_handle* solver = find(g_solvers, solver_id);
    if (solver == nullptr) return fail(PC_RESULT_NOT_FOUND, "unknown solver");
    pc_item_state* item = find(g_items, item_id);
    if (item == nullptr) return fail(PC_RESULT_NOT_FOUND, "unknown item");
    pc_economy_handle* economy = find(g_economies, economy_id);
    if (economy == nullptr) {
        return fail(PC_RESULT_NOT_FOUND, "unknown economy");
    }
    pc_solve_options options{};
    std::string parse_error;
    if (!parse_solve_options(options_json, options, parse_error)) {
        return fail(PC_RESULT_INVALID_ARGUMENT, parse_error.c_str());
    }
    pc_error_info error = make_error();
    const pc_result rc = pc_solver_solve_begin(
        *solver, item, *economy, &options, &error);
    if (rc != PC_RESULT_OK) return fail(error);
    return respond("{\"ok\":true}");
}

EMSCRIPTEN_KEEPALIVE
const char* pcw_solver_solve_step(uint32_t solver_id,
                                  uint32_t max_work_items) {
    pc_solver_handle* solver = find(g_solvers, solver_id);
    if (solver == nullptr) return fail(PC_RESULT_NOT_FOUND, "unknown solver");
    pc_solve_progress progress{};
    pc_error_info error = make_error();
    const pc_result rc = pc_solver_solve_step(
        *solver, max_work_items, &progress, &error);
    if (rc != PC_RESULT_OK) return fail(error);
    std::string out = "{\"ok\":true,\"progress\":";
    append_solve_progress(out, progress);
    out.push_back('}');
    return respond(std::move(out));
}

EMSCRIPTEN_KEEPALIVE
const char* pcw_solver_solve_finish(uint32_t solver_id) {
    pc_solver_handle* solver = find(g_solvers, solver_id);
    if (solver == nullptr) return fail(PC_RESULT_NOT_FOUND, "unknown solver");
    pc_solve_summary summary{};
    pc_error_info error = make_error();
    const pc_result rc = pc_solver_solve_finish(*solver, &summary, &error);
    if (rc != PC_RESULT_OK) return fail(error);
    std::string out = "{\"ok\":true,";
    append_solve_summary(out, summary);
    out.push_back('}');
    return respond(std::move(out));
}

EMSCRIPTEN_KEEPALIVE
void pcw_solver_solve_abandon(uint32_t solver_id) {
    pc_solver_handle* solver = find(g_solvers, solver_id);
    if (solver != nullptr) pc_solver_solve_abandon(*solver);
}

EMSCRIPTEN_KEEPALIVE
const char* pcw_solver_solve(uint32_t solver_id, uint32_t item_id,
                             uint32_t economy_id, const char* options_json) {
    pc_solver_handle* solver = find(g_solvers, solver_id);
    if (solver == nullptr) return fail(PC_RESULT_NOT_FOUND, "unknown solver");
    pc_item_state* item = find(g_items, item_id);
    if (item == nullptr) return fail(PC_RESULT_NOT_FOUND, "unknown item");
    pc_economy_handle* economy = find(g_economies, economy_id);
    if (economy == nullptr) {
        return fail(PC_RESULT_NOT_FOUND, "unknown economy");
    }
    pc_solve_options options{};
    std::string parse_error;
    if (!parse_solve_options(options_json, options, parse_error)) {
        return fail(PC_RESULT_INVALID_ARGUMENT, parse_error.c_str());
    }
    pc_solve_summary summary{};
    pc_error_info error = make_error();
    pc_result rc = pc_solver_solve(*solver, item, *economy, &options,
                                   &summary, &error);
    if (rc != PC_RESULT_OK) return fail(error);
    std::string out = "{\"ok\":true,";
    append_solve_summary(out, summary);
    out.push_back('}');
    return respond(std::move(out));
}

EMSCRIPTEN_KEEPALIVE
const char* pcw_solver_state_value(uint32_t solver_id, uint32_t state_id) {
    pc_solver_handle* solver = find(g_solvers, solver_id);
    if (solver == nullptr) return fail(PC_RESULT_NOT_FOUND, "unknown solver");
    double value = 0.0;
    const char* action_id = nullptr;
    pc_error_info error = make_error();
    pc_result rc = pc_solver_state_value(*solver, state_id, &value,
                                         &action_id, &error);
    if (rc != PC_RESULT_OK) return fail(error);
    std::string out = "{\"ok\":true,\"value\":";
    append_precise_double(out, value);
    out += ",\"action\":";
    if (action_id == nullptr) {
        out += "null";
    } else {
        append_escaped(out, action_id);
    }
    out.push_back('}');
    return respond(std::move(out));
}

EMSCRIPTEN_KEEPALIVE
const char* pcw_solver_project(uint32_t solver_id, uint32_t item_id) {
    pc_solver_handle* solver = find(g_solvers, solver_id);
    if (solver == nullptr) return fail(PC_RESULT_NOT_FOUND, "unknown solver");
    pc_item_state* item = find(g_items, item_id);
    if (item == nullptr) return fail(PC_RESULT_NOT_FOUND, "unknown item");
    uint32_t state_id = 0;
    pc_error_info error = make_error();
    pc_result rc = pc_solver_project_item(*solver, item, &state_id, &error);
    if (rc != PC_RESULT_OK) return fail(error);
    std::string out = "{\"ok\":true,\"state\":";
    out += std::to_string(state_id);
    out.push_back('}');
    return respond(std::move(out));
}

EMSCRIPTEN_KEEPALIVE
const char* pcw_solver_compile(uint32_t solver_id) {
    pc_solver_handle* solver = find(g_solvers, solver_id);
    if (solver == nullptr) return fail(PC_RESULT_NOT_FOUND, "unknown solver");
    pc_error_info error = make_error();
    size_t length = 0;
    pc_result rc =
        pc_solver_compile_strategy(*solver, nullptr, 0, &length, &error);
    if (rc != PC_RESULT_OK) return fail(error);
    std::string strategy(length + 1, '\0');
    rc = pc_solver_compile_strategy(*solver, strategy.data(),
                                    strategy.size(), &length, &error);
    if (rc != PC_RESULT_OK) return fail(error);
    strategy.resize(length);
    std::string out = "{\"ok\":true,\"strategy\":";
    append_escaped(out, strategy.c_str());
    out.push_back('}');
    return respond(std::move(out));
}

EMSCRIPTEN_KEEPALIVE
const char* pcw_solver_log(uint32_t solver_id) {
    pc_solver_handle* solver = find(g_solvers, solver_id);
    if (solver == nullptr) return fail(PC_RESULT_NOT_FOUND, "unknown solver");
    pc_error_info error = make_error();
    size_t length = 0;
    pc_result rc = pc_solver_solve_log(*solver, nullptr, 0, &length, &error);
    if (rc != PC_RESULT_OK) return fail(error);
    std::string log(length + 1, '\0');
    rc = pc_solver_solve_log(*solver, log.data(), log.size(), &length,
                             &error);
    if (rc != PC_RESULT_OK) return fail(error);
    log.resize(length);
    std::string out = "{\"ok\":true,\"log\":";
    append_escaped(out, log.c_str());
    out.push_back('}');
    return respond(std::move(out));
}

EMSCRIPTEN_KEEPALIVE
const char* pcw_solver_telemetry(uint32_t solver_id) {
    pc_solver_handle* solver = find(g_solvers, solver_id);
    if (solver == nullptr) return fail(PC_RESULT_NOT_FOUND, "unknown solver");
    pc_error_info error = make_error();
    size_t length = 0;
    pc_result rc =
        pc_solver_telemetry(*solver, nullptr, 0, &length, &error);
    if (rc != PC_RESULT_OK) return fail(error);
    std::string telemetry(length + 1, '\0');
    rc = pc_solver_telemetry(*solver, telemetry.data(), telemetry.size(),
                             &length, &error);
    if (rc != PC_RESULT_OK) return fail(error);
    telemetry.resize(length);
    std::string out = "{\"ok\":true,\"telemetry\":";
    out += telemetry;
    out.push_back('}');
    return respond(std::move(out));
}

} // extern "C"
