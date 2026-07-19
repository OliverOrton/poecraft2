#include "poecraft/api.h"
#include "poecraft/item_state.h"
#include "poecraft/session.h"
#include "poecraft/simulator.h"
#include "poecraft/solver.h"

#include "json.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;
using poecraft::json::Parser;
using poecraft::json::Type;
using poecraft::json::Value;

namespace {

struct Arguments {
    fs::path artifact;
    fs::path corpus;
    fs::path output;
    fs::path strategy_output;
    std::string case_id;
    bool validate_only = false;
    bool skip_verification = false;
    bool emit_progress = false;
    std::uint64_t verification_runs = 0;
    std::uint64_t verification_seed = 0;
    std::uint32_t verification_chunk_runs = 32;
    double verification_time_limit_seconds = 0.0;
    bool exact_strategy_evaluation = false;
    double exact_strategy_evaluation_time_limit_seconds = 0.0;
};

struct NativeHandles {
    pc_session_handle session = nullptr;
    pc_solver_handle solver = nullptr;
    pc_economy_handle economy = nullptr;
    pc_strategy_handle strategy = nullptr;
    pc_simulator_handle simulator = nullptr;
    pc_strategy_eval_work_handle strategy_evaluation = nullptr;

    ~NativeHandles() {
        pc_strategy_eval_destroy(strategy_evaluation);
        pc_simulator_destroy(simulator);
        pc_strategy_destroy(strategy);
        pc_economy_destroy(economy);
        pc_solver_destroy(solver);
        pc_session_destroy(session);
    }
};

struct CaseResult {
    struct ActionCount {
        std::string node_id;
        std::int32_t action_type = -1;
        std::uint64_t count = 0;
    };
    std::string actual_status = "not_run";
    bool expectation_met = false;
    bool verification_skipped = false;
    double registry_layout_ms = 0.0;
    double solve_ms = 0.0;
    double compile_ms = 0.0;
    double verification_ms = 0.0;
    double total_ms = 0.0;
    std::uint64_t solve_steps = 0;
    std::uint64_t diagnostic_work_limit = 0;
    double diagnostic_time_limit_seconds = 0.0;
    std::string diagnostic_stop_reason;
    std::vector<std::string> product_action_ids;
    double max_solve_step_ms = 0.0;
    std::uint32_t max_solve_step_phase = 0;
    std::uint32_t max_solve_step_expanded_states = 0;
    std::uint32_t max_solve_step_sweeps = 0;
    double cooperative_abandon_ms = 0.0;
    bool has_cooperative_abandon = false;
    std::uint64_t working_set_before = 0;
    std::uint64_t working_set_after = 0;
    bool has_native_memory = false;
    pc_native_memory_stats native_memory{};
    bool has_solve_summary = false;
    pc_solve_summary solve_summary{};
    std::string telemetry_json;
    std::size_t strategy_json_bytes = 0;
    std::string strategy_output_path;
    std::uint64_t compiled_nodes = 0;
    std::uint64_t compiled_edges = 0;
    bool has_compiled_graph = false;
    bool has_verification = false;
    bool verification_diagnostic = false;
    bool verification_finished = false;
    bool verification_time_limited = false;
    std::uint64_t verification_requested_runs = 0;
    double verification_projected_ms = 0.0;
    double verification_cost_standard_deviation = 0.0;
    double verification_cost_standard_error = 0.0;
    bool has_verification_cost_variance = false;
    bool has_exact_strategy_evaluation = false;
    bool exact_strategy_evaluation_time_limited = false;
    double exact_strategy_evaluation_ms = 0.0;
    std::string exact_strategy_evaluation_json;
    pc_simulation_summary verification{};
    double verification_mean_cost = 0.0;
    double cost_delta_absolute = 0.0;
    double cost_delta_relative = 0.0;
    std::vector<ActionCount> action_distribution;
    std::vector<std::pair<std::string, bool>> cap_checks;
    std::vector<std::string> errors;
};

std::string read_file(const fs::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("unable to read " + path.string());
    }
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

void write_file(const fs::path& path, const std::string& text) {
    if (path.has_parent_path()) fs::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) {
        throw std::runtime_error("unable to write " + path.string());
    }
    stream << text;
    if (!stream) {
        throw std::runtime_error("failed while writing " + path.string());
    }
}

Value parse_json(const std::string& text, const fs::path& path) {
    try {
        return Parser(text.data(), text.size()).parse();
    } catch (const std::exception& ex) {
        throw std::runtime_error(path.string() + ": " + ex.what());
    }

}

std::string escape_json(std::string_view value) {
    std::ostringstream out;
    out << '"';
    for (const unsigned char c : value) {
        switch (c) {
        case '"': out << "\\\""; break;
        case '\\': out << "\\\\"; break;
        case '\b': out << "\\b"; break;
        case '\f': out << "\\f"; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default:
            if (c < 0x20) {
                out << "\\u" << std::hex << std::setw(4)
                    << std::setfill('0') << static_cast<unsigned>(c)
                    << std::dec << std::setfill(' ');
            } else {
                out << static_cast<char>(c);
            }
        }
    }
    out << '"';
    return out.str();
}

void append_json(std::ostringstream& out, const Value& value) {
    switch (value.type) {
    case Type::Null: out << "null"; break;
    case Type::Bool: out << (value.boolean ? "true" : "false"); break;
    case Type::Number:
        if (std::isfinite(value.number)) {
            out << std::setprecision(17) << value.number;
        } else {
            out << "null";
        }
        break;
    case Type::String: out << escape_json(value.string); break;
    case Type::Array:
        out << '[';
        for (std::size_t i = 0; i < value.array.size(); ++i) {
            if (i != 0) out << ',';
            append_json(out, value.array[i]);
        }
        out << ']';
        break;
    case Type::Object:
        out << '{';
        for (std::size_t i = 0; i < value.object.size(); ++i) {
            if (i != 0) out << ',';
            out << escape_json(value.object[i].first) << ':';
            append_json(out, value.object[i].second);
        }
        out << '}';
        break;
    }
}

std::string json_of(const Value& value) {
    std::ostringstream out;
    append_json(out, value);
    return out.str();
}

const Value& required(const Value& object, const char* key, Type type);
const Value* optional(const Value& object, const char* key, Type type);
std::string required_string(const Value& object, const char* key);
std::string api_error(
    const char* operation, pc_result result, const pc_error_info& error);

std::string load_case_economy_json(const Value& specification) {
    const Value& economy = required(specification, "economy", Type::Object);
    const Value* snapshot_path =
        optional(economy, "snapshot_path", Type::String);
    if (snapshot_path == nullptr) return json_of(economy);

    const fs::path path = fs::absolute(snapshot_path->string);
    const std::string text = read_file(path);
    Value snapshot = parse_json(text, path);
    if (required_string(snapshot, "id") != required_string(economy, "id")) {
        throw std::runtime_error("pinned economy snapshot id mismatch");
    }
    const Value& metadata = required(snapshot, "metadata", Type::Object);
    if (required_string(metadata, "content_sha256") !=
        required_string(economy, "content_sha256")) {
        throw std::runtime_error("pinned economy content hash mismatch");
    }
    if (required_string(metadata, "source_cutoff_at_utc") !=
        required_string(economy, "source_cutoff_at_utc")) {
        throw std::runtime_error("pinned economy cutoff mismatch");
    }
    const Value& source_prices = required(snapshot, "prices", Type::Object);
    if (source_prices.find("base") != nullptr) {
        throw std::runtime_error(
            "pinned regression requires Restart/base to remain unpriced");
    }
    const Value& overrides = required(economy, "manual_overrides", Type::Object);
    const Value* fallback = economy.find("fallback_price");
    if (fallback == nullptr || fallback->type != Type::Null) {
        throw std::runtime_error(
            "pinned regression requires a null price fallback");
    }
    if (overrides.object.empty()) return text;

    if (overrides.object.size() != 1 ||
        overrides.object.front().first != "base" ||
        overrides.object.front().second.type != Type::Number ||
        overrides.object.front().second.number <= 0.0 ||
        required_string(economy, "override_purpose") !=
            "r3f_restart_route_gate_not_market_quote") {
        throw std::runtime_error(
            "pinned regression permits only the disclosed positive R3F base "
            "override");
    }
    for (auto& [key, value] : snapshot.object) {
        if (key == "prices") {
            value.object.emplace_back(
                "base", overrides.object.front().second);
            return json_of(snapshot);
        }
    }
    throw std::runtime_error("pinned economy snapshot has no prices object");
}

std::vector<std::string> priced_product_action_ids(
    pc_session_handle session, const Value& specification,
    const std::string& economy_json) {
    const Value* product =
        optional(specification, "product_action_envelope", Type::Object);
    if (product == nullptr) return {};
    if (required_string(*product, "mode") !=
        "calculator_goal_relevant_priced_v1") {
        throw std::runtime_error("unsupported product action-envelope mode");
    }
    const std::string envelope_goal =
        json_of(required(*product, "envelope_goal", Type::Object));
    pc_error_info error;
    pc_error_info_init(&error);
    pc_solver_handle envelope_solver = nullptr;
    pc_result result = pc_solver_create(
        session, envelope_goal.data(), envelope_goal.size(),
        &envelope_solver, &error);
    if (result != PC_RESULT_OK) {
        throw std::runtime_error(api_error(
            "pc_solver_create(product envelope)", result, error));
    }
    struct SolverCleanup {
        pc_solver_handle value;
        ~SolverCleanup() { pc_solver_destroy(value); }
    } cleanup{envelope_solver};

    const Value economy = Parser(
        economy_json.data(), economy_json.size()).parse();
    const Value& prices = required(economy, "prices", Type::Object);
    std::uint32_t count = 0;
    result = pc_solver_candidates(
        envelope_solver, nullptr, 0, &count, &error);
    if (result != PC_RESULT_OK) {
        throw std::runtime_error(api_error(
            "pc_solver_candidates(product envelope size)", result, error));
    }
    std::vector<std::uint32_t> indices(count);
    result = pc_solver_candidates(
        envelope_solver, indices.data(), count, &count, &error);
    if (result != PC_RESULT_OK) {
        throw std::runtime_error(api_error(
            "pc_solver_candidates(product envelope)", result, error));
    }
    std::vector<std::string> ids;
    for (const std::uint32_t index : indices) {
        pc_solver_action_info info{};
        info.struct_size = sizeof(info);
        info.abi_version = PC_ABI_VERSION;
        result = pc_solver_get_action_info(
            envelope_solver, index, &info, &error);
        if (result != PC_RESULT_OK) {
            throw std::runtime_error(api_error(
                "pc_solver_get_action_info(product envelope)", result,
                error));
        }
        bool priced = true;
        for (std::uint32_t key = 0; key < info.cost_key_count; ++key) {
            priced &= prices.find(info.cost_keys[key]) != nullptr;
        }
        if (priced) ids.emplace_back(info.id);
    }
    return ids;
}

std::string goal_with_actions(
    const Value& goal, const std::vector<std::string>& action_ids) {
    std::string json = json_of(goal);
    if (action_ids.empty()) return json;
    if (json.empty() || json.back() != '}') {
        throw std::logic_error("solver goal did not serialize as an object");
    }
    json.pop_back();
    json += ",\"actions\":[";
    for (std::size_t i = 0; i < action_ids.size(); ++i) {
        if (i != 0) json.push_back(',');
        json += escape_json(action_ids[i]);
    }
    json += "]}";
    return json;
}

const Value& required(const Value& object, const char* key, Type type) {
    const Value* value = object.find(key);
    if (value == nullptr) {
        throw std::runtime_error(std::string("missing field: ") + key);
    }
    if (value->type != type) {
        throw std::runtime_error(std::string("wrong type for field: ") + key);
    }
    return *value;
}

const Value* optional(const Value& object, const char* key, Type type) {
    const Value* value = object.find(key);
    if (value != nullptr && value->type != type) {
        throw std::runtime_error(std::string("wrong type for field: ") + key);
    }
    return value;
}

std::string required_string(const Value& object, const char* key) {
    return required(object, key, Type::String).string;
}

std::string optional_string(
    const Value& object, const char* key, const std::string& fallback = {}) {
    const Value* value = optional(object, key, Type::String);
    return value == nullptr ? fallback : value->string;
}

bool optional_bool(const Value& object, const char* key, bool fallback) {
    const Value* value = optional(object, key, Type::Bool);
    return value == nullptr ? fallback : value->boolean;
}

std::uint32_t optional_u32(
    const Value& object, const char* key, std::uint32_t fallback) {
    const Value* value = optional(object, key, Type::Number);
    if (value == nullptr) return fallback;
    if (value->number < 0 ||
        value->number > std::numeric_limits<std::uint32_t>::max()) {
        throw std::runtime_error(std::string("out-of-range field: ") + key);
    }
    return static_cast<std::uint32_t>(value->number);
}

std::uint64_t optional_u64(
    const Value& object, const char* key, std::uint64_t fallback) {
    const Value* value = optional(object, key, Type::Number);
    if (value == nullptr) return fallback;
    if (value->number < 0) {
        throw std::runtime_error(std::string("negative field: ") + key);
    }
    return static_cast<std::uint64_t>(value->number);
}

double milliseconds(Clock::time_point begin, Clock::time_point end) {
    return std::chrono::duration<double, std::milli>(end - begin).count();
}

std::uint64_t process_working_set() {
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS_EX counters{};
    counters.cb = sizeof(counters);
    if (GetProcessMemoryInfo(
            GetCurrentProcess(),
            reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
            sizeof(counters))) {
        return static_cast<std::uint64_t>(counters.WorkingSetSize);
    }
#elif defined(__linux__)
    std::ifstream stream("/proc/self/statm");
    std::uint64_t total_pages = 0;
    std::uint64_t resident_pages = 0;
    if (stream >> total_pages >> resident_pages) {
        const long page_size = sysconf(_SC_PAGESIZE);
        if (page_size > 0) {
            return resident_pages * static_cast<std::uint64_t>(page_size);
        }
    }
#endif
    return 0;
}

std::string result_name(pc_result result) {
    switch (result) {
    case PC_RESULT_OK: return "ok";
    case PC_RESULT_INVALID_ARGUMENT: return "invalid_argument";
    case PC_RESULT_IO_ERROR: return "io_error";
    case PC_RESULT_DATA_ERROR: return "data_error";
    case PC_RESULT_UNSUPPORTED_FEATURE: return "unsupported_feature";
    case PC_RESULT_INTERNAL_ERROR: return "internal_error";
    case PC_RESULT_NOT_FOUND: return "not_found";
    case PC_RESULT_BUFFER_TOO_SMALL: return "buffer_too_small";
    case PC_RESULT_CAPACITY_EXCEEDED: return "capacity_exceeded";
    }
    return "unknown";
}

std::string api_error(
    const char* operation, pc_result result, const pc_error_info& error) {
    return std::string(operation) + " returned " + result_name(result) +
           ": " + error.message;
}

std::uint8_t rarity_code(const std::string& rarity) {
    if (rarity == "normal") return PC_RARITY_NORMAL;
    if (rarity == "magic") return PC_RARITY_MAGIC;
    if (rarity == "rare") return PC_RARITY_RARE;
    throw std::runtime_error("unknown start rarity: " + rarity);
}

std::uint8_t mod_flags(const Value& mod) {
    std::uint8_t flags = 0;
    const Value& values = required(mod, "flags", Type::Array);
    for (const Value& value : values.array) {
        if (value.type != Type::String) {
            throw std::runtime_error("start mod flags must be strings");
        }
        if (value.string == "fractured") flags |= PC_MOD_SLOT_FRACTURED;
        else if (value.string == "crafted") flags |= PC_MOD_SLOT_CRAFTED;
        else if (value.string == "veiled") flags |= PC_MOD_SLOT_VEILED;
        else throw std::runtime_error("unknown start mod flag: " + value.string);
    }
    return flags;
}

pc_mod_info find_mod(pc_session_handle session, const std::string& key) {
    pc_error_info error;
    pc_error_info_init(&error);
    std::uint32_t count = 0;
    pc_result result = pc_session_get_mod_count(session, &count, &error);
    if (result != PC_RESULT_OK) {
        throw std::runtime_error(api_error("pc_session_get_mod_count", result,
                                           error));
    }
    for (std::uint32_t index = 0; index < count; ++index) {
        pc_mod_info info{};
        result = pc_session_get_mod_info(session, index, &info, &error);
        if (result != PC_RESULT_OK) {
            throw std::runtime_error(api_error("pc_session_get_mod_info", result,
                                               error));
        }
        if (info.key != nullptr && key == info.key) return info;
    }
    throw std::runtime_error("start mod is not in session: " + key);
}

pc_item_state build_start_item(pc_session_handle session, const Value& start) {
    pc_error_info error;
    pc_error_info_init(&error);
    pc_item_init_options options{};
    options.struct_size = sizeof(options);
    options.abi_version = PC_ABI_VERSION;
    options.rarity = rarity_code(required_string(start, "rarity"));
    options.with_implicits = optional_bool(start, "with_implicits", false) ? 1 : 0;
    pc_item_state item{};
    pc_result result = pc_item_init(session, &options, &item, &error);
    if (result != PC_RESULT_OK) {
        throw std::runtime_error(api_error("pc_item_init", result, error));
    }

    if (const Value* tier = optional(start, "searing_exarch_tier", Type::Number)) {
        item.searing_exarch_tier = static_cast<std::uint8_t>(tier->number);
    }
    if (const Value* tier = optional(start, "eater_of_worlds_tier", Type::Number)) {
        item.eater_of_worlds_tier = static_cast<std::uint8_t>(tier->number);
    }
    const Value& mods = required(start, "mods", Type::Array);
    for (const Value& mod : mods.array) {
        if (mod.type != Type::Object) {
            throw std::runtime_error("start mods must be objects");
        }
        const pc_mod_info info = find_mod(session, required_string(mod, "key"));
        result = pc_item_add_mod(
            &item, info.generation_type, info.session_mod_id,
            static_cast<std::uint16_t>(info.primary_group_id), mod_flags(mod),
            nullptr);
        if (result != PC_RESULT_OK) {
            throw std::runtime_error("unable to place start mod " +
                                     required_string(mod, "key"));
        }
    }
    return item;
}

std::string query_telemetry(
    pc_solver_handle solver, std::vector<std::string>& errors) {
    pc_error_info error;
    pc_error_info_init(&error);
    std::size_t length = 0;
    pc_result result =
        pc_solver_telemetry(solver, nullptr, 0, &length, &error);
    if (result != PC_RESULT_OK && result != PC_RESULT_BUFFER_TOO_SMALL) {
        errors.push_back(api_error("pc_solver_telemetry(size)", result, error));
        return {};
    }
    std::string json(length + 1, '\0');
    result = pc_solver_telemetry(
        solver, json.data(), json.size(), &length, &error);
    if (result != PC_RESULT_OK) {
        errors.push_back(api_error("pc_solver_telemetry", result, error));
        return {};
    }
    json.resize(length);
    try {
        const Value parsed = Parser(json.data(), json.size()).parse();
        if (required_string(parsed, "version") != "solver_telemetry_v1") {
            errors.push_back("solver telemetry returned an unexpected version");
            return {};
        }
    } catch (const std::exception& ex) {
        errors.push_back(std::string("invalid solver telemetry JSON: ") + ex.what());
        return {};
    }
    return json;
}

std::string expected_solve_status(const Value& specification) {
    return required_string(required(specification, "expected", Type::Object),
                           "solve_status");
}

bool expectation_matches(const std::string& expected,
                         const std::string& actual) {
    if (expected == actual) return true;
    if (expected == "bounded_diagnostic") {
        return actual == "bounded_first_expansion_complete" ||
               actual == "diagnostic_time_cap" ||
               actual == "diagnostic_work_cap" ||
               actual == "refused_state_cap" ||
               actual == "refused_resource_cap";
    }
    if (expected == "baseline_cap_or_compile_refusal_allowed") {
        return actual == "converged" || actual == "refused_state_cap" ||
               actual == "refused_resource_cap" ||
               actual == "not_converged" || actual == "compile_refused" ||
               actual == "refused_unreachable_goal";
    }
    if (expected == "refused_state_cap_with_filtered_actions") {
        return actual == "refused_state_cap";
    }
    return false;
}

const Value* nested_member(
    const Value& root, std::initializer_list<const char*> path) {
    const Value* current = &root;
    for (const char* key : path) {
        if (current->type != Type::Object) return nullptr;
        current = current->find(key);
        if (current == nullptr) return nullptr;
    }
    return current;
}

void add_cap_check(
    CaseResult& report, const Value& telemetry,
    std::initializer_list<const char*> metric_path, const Value& caps,
    const char* cap_name) {
    const Value* metric = nested_member(telemetry, metric_path);
    const Value* cap = optional(caps, cap_name, Type::Number);
    if (metric == nullptr || metric->type != Type::Number || cap == nullptr) {
        return;
    }
    report.cap_checks.emplace_back(cap_name, metric->number <= cap->number);
}

void evaluate_cap_checks(const Value& specification, CaseResult& report) {
    if (report.telemetry_json.empty() ||
        specification.find("caps") == nullptr) {
        return;
    }
    try {
        const Value telemetry = Parser(
            report.telemetry_json.data(), report.telemetry_json.size()).parse();
        const Value& caps = required(specification, "caps", Type::Object);
        add_cap_check(report, telemetry, {"states", "discovered"}, caps,
                      "max_discovered_states");
        add_cap_check(report, telemetry, {"states", "expanded"}, caps,
                      "max_expanded_states");
        add_cap_check(report, telemetry, {"work", "state_action_rows"}, caps,
                      "max_state_action_rows");
        add_cap_check(report, telemetry, {"work", "transition_entries"}, caps,
                      "max_transitions");
        add_cap_check(report, telemetry,
                      {"cache", "reforge", "frontier_work"}, caps,
                      "max_reforge_work");
        add_cap_check(report, telemetry,
                      {"memory", "solver_owned_bytes_estimate"}, caps,
                      "max_solver_owned_bytes");
        add_cap_check(report, telemetry, {"compilation", "nodes"}, caps,
                      "max_compiled_nodes");
        add_cap_check(report, telemetry, {"compilation", "edges"}, caps,
                      "max_compiled_edges");
        add_cap_check(report, telemetry,
                      {"compilation", "strategy_json_bytes"}, caps,
                      "max_strategy_json_bytes");
        const Value* available = nested_member(
            telemetry, {"compilation", "available"});
        const Value* nodes = nested_member(telemetry, {"compilation", "nodes"});
        const Value* edges = nested_member(telemetry, {"compilation", "edges"});
        if (available != nullptr && available->type == Type::Bool &&
            available->boolean && nodes != nullptr && edges != nullptr &&
            nodes->type == Type::Number && edges->type == Type::Number) {
            report.has_compiled_graph = true;
            report.compiled_nodes = static_cast<std::uint64_t>(nodes->number);
            report.compiled_edges = static_cast<std::uint64_t>(edges->number);
        }
    } catch (const std::exception& ex) {
        report.errors.push_back(std::string("unable to evaluate cap checks: ") +
                                ex.what());
    }
}

bool evaluate_expectation(
    const Value& specification, const CaseResult& report,
    const bool skip_verification) {
    if (report.actual_status == "covered_by_native_unit_gate" ||
        report.actual_status == "not_run_approval_pending") {
        return true;
    }
    if (report.telemetry_json.empty() || !report.errors.empty()) return false;
    if (!expectation_matches(expected_solve_status(specification),
                             report.actual_status)) {
        return false;
    }
    for (const auto& [name, passed] : report.cap_checks) {
        (void)name;
        if (!passed) return false;
    }
    const Value& expected = required(specification, "expected", Type::Object);
    const Value telemetry = Parser(
        report.telemetry_json.data(), report.telemetry_json.size()).parse();
    for (const char* section : {
             "availability", "actions", "abstraction", "states", "work",
             "cache", "optimization", "timings_ns", "memory", "compilation",
             "value"}) {
        if (optional(telemetry, section, Type::Object) == nullptr) return false;
    }
    if (required_string(expected, "optimality_status") == "exact") {
        const Value* status = nested_member(
            telemetry, {"optimization", "status"});
        if (status == nullptr || status->type != Type::String ||
            status->string != "exact_abstract") {
            return false;
        }
    }
    if (required_string(expected, "compile_status") == "compiled" &&
        report.strategy_json_bytes == 0) {
        return false;
    }
    if (skip_verification) return true;
    const std::string verification_status =
        required_string(expected, "verification_status");
    const bool verification_required =
        verification_status == "run" ||
        (verification_status == "run_if_compiled" &&
         report.strategy_json_bytes != 0);
    if (!verification_required) {
        return true;
    }
    if (!report.has_verification || report.verification.completed_runs == 0) {
        return false;
    }
    const std::uint64_t off_policy =
        report.verification.no_matching_edge_count +
        report.verification.action_not_applied_count;
    if (off_policy != 0) return false;
    const Value& verification =
        required(specification, "verification", Type::Object);
    const Value* minimum_success =
        optional(verification, "minimum_success_rate", Type::Number);
    if (minimum_success != nullptr) {
        const double success_rate =
            static_cast<double>(report.verification.success_count) /
            static_cast<double>(report.verification.completed_runs);
        if (success_rate < minimum_success->number) return false;
    }
    const Value* absolute =
        optional(verification, "mean_cost_absolute_tolerance", Type::Number);
    if (absolute != nullptr && report.cost_delta_absolute > absolute->number) {
        return false;
    }
    const Value* relative =
        optional(verification, "mean_cost_relative_tolerance", Type::Number);
    if (relative != nullptr && report.cost_delta_relative > relative->number) {
        return false;
    }
    return true;
}

std::string classify_completed_solve(
    const Value& specification, const pc_solve_summary& summary,
    const std::string& telemetry_json) {
    if (telemetry_json.empty()) return "telemetry_unavailable";
    const Value telemetry =
        Parser(telemetry_json.data(), telemetry_json.size()).parse();
    const Value* state_cap = nested_member(
        telemetry, {"optimization", "state_cap_hit"});
    if (state_cap != nullptr && state_cap->type == Type::Bool &&
        state_cap->boolean) {
        return "refused_state_cap";
    }
    const Value* resource_cap = nested_member(
        telemetry, {"optimization", "resource_cap_hit"});
    if (resource_cap != nullptr && resource_cap->type == Type::Bool &&
        resource_cap->boolean) {
        return "refused_resource_cap";
    }
    const Value* missing_price = nested_member(
        telemetry, {"actions", "missing_price"});
    const Value* unsupported = nested_member(
        telemetry, {"actions", "unsupported_requested"});
    if (unsupported == nullptr) {
        unsupported = nested_member(
            telemetry, {"actions", "unsupported_observed"});
    }
    const bool has_missing_price =
        missing_price != nullptr && missing_price->type == Type::Number &&
        missing_price->number > 0;
    const bool has_unsupported =
        unsupported != nullptr && unsupported->type == Type::Number &&
        unsupported->number > 0;
    if (has_missing_price && has_unsupported) {
        return "refused_missing_price_and_unsupported_action";
    }
    if (has_missing_price) return "refused_missing_price";
    if (has_unsupported) return "refused_unsupported_action";
    const Value* full_request_status = nested_member(
        telemetry, {"optimization", "full_request_status"});
    if (full_request_status != nullptr &&
        full_request_status->type == Type::String &&
        full_request_status->string == "incomplete_action_subset") {
        return "incomplete_action_subset";
    }
    const Value* raw_start_bound = nested_member(
        telemetry, {"value", "raw_start_bound"});
    if (raw_start_bound != nullptr &&
        raw_start_bound->type == Type::Number &&
        raw_start_bound->number >= 1e12 && summary.residual == 0.0) {
        return "refused_unreachable_goal";
    }
    if (!summary.converged) {
        const Value& caps = required(specification, "caps", Type::Object);
        const std::uint32_t max_sweeps =
            optional_u32(caps, "max_sweeps", 100000);
        if (summary.sweeps >= max_sweeps) return "refused_sweep_cap";
        return "not_converged";
    }
    const Value* canonical_value = nested_member(
        telemetry, {"value", "start"});
    if (canonical_value == nullptr || canonical_value->type != Type::Number ||
        !std::isfinite(canonical_value->number) ||
        canonical_value->number >= 1e12) {
        return "refused_unreachable_goal";
    }
    return "converged";
}

void validate_case_shape(const Value& specification) {
    if (required_string(specification, "schema_version") !=
        "solver_benchmark_case_v1") {
        throw std::runtime_error("case schema_version must be solver_benchmark_case_v1");
    }
    required_string(specification, "id");
    required_string(specification, "category");
    required_string(specification, "approval_status");
    required_string(specification, "comparison_profile");
    required(specification, "benchmark_enabled", Type::Bool);
    required(specification, "expected", Type::Object);
    const std::string backend =
        optional_string(specification, "execution_backend", "artifact");
    if (backend == "native_unit_fixture") {
        required(specification, "unit_fixture", Type::Object);
        return;
    }
    const Value& session = required(specification, "session", Type::Object);
    required_string(session, "base_metadata_path");
    required(session, "item_level", Type::Number);
    required(specification, "start", Type::Object);
    const Value& goal = required(specification, "goal", Type::Object);
    required(goal, "slots", Type::Array);
    required(specification, "economy", Type::Object);
    required(specification, "caps", Type::Object);
    required(specification, "verification", Type::Object);
}

void create_case_objects(
    pc_data_handle data, const Value& specification, NativeHandles& handles,
    pc_item_state& start_item,
    std::vector<std::string>* product_action_ids = nullptr) {
    const Value& session_spec = required(specification, "session", Type::Object);
    const std::string base = required_string(session_spec, "base_metadata_path");
    pc_session_options session_options{};
    session_options.struct_size = sizeof(session_options);
    session_options.abi_version = PC_ABI_VERSION;
    session_options.base_metadata_path = base.c_str();
    session_options.item_level = optional_u32(session_spec, "item_level", 86);
    pc_error_info error;
    pc_error_info_init(&error);
    pc_result result = pc_session_create(
        data, &session_options, &handles.session, &error);
    if (result != PC_RESULT_OK) {
        throw std::runtime_error(api_error("pc_session_create", result, error));
    }
    start_item = build_start_item(
        handles.session, required(specification, "start", Type::Object));
    const std::string economy = load_case_economy_json(specification);
    const std::vector<std::string> derived_actions =
        priced_product_action_ids(handles.session, specification, economy);
    if (product_action_ids != nullptr) *product_action_ids = derived_actions;
    if (const Value* product = optional(
            specification, "product_action_envelope", Type::Object)) {
        const Value& expected = required(
            *product, "expected_priced_action_ids", Type::Array);
        if (!expected.array.empty()) {
            std::vector<std::string> pinned;
            for (const Value& entry : expected.array) {
                if (entry.type != Type::String) {
                    throw std::runtime_error(
                        "pinned product action ids must be strings");
                }
                pinned.push_back(entry.string);
            }
            if (pinned != derived_actions) {
                throw std::runtime_error(
                    "derived Calculator product action envelope changed");
            }
        }
    }
    const std::string goal = goal_with_actions(
        required(specification, "goal", Type::Object), derived_actions);
    result = pc_solver_create(handles.session, goal.data(), goal.size(),
                              &handles.solver, &error);
    if (result != PC_RESULT_OK) {
        throw std::runtime_error(api_error("pc_solver_create", result, error));
    }
    result = pc_economy_load_json(economy.data(), economy.size(),
                                  &handles.economy, &error);
    if (result != PC_RESULT_OK) {
        throw std::runtime_error(api_error("pc_economy_load_json", result, error));
    }
}

CaseResult run_case(
    pc_data_handle data, const Value& specification,
    const bool skip_verification, const fs::path& strategy_output,
    const bool emit_progress, const std::uint64_t verification_runs_override,
    const std::uint64_t verification_seed_override,
    const std::uint32_t verification_chunk_runs,
    const double verification_time_limit_seconds,
    const bool exact_strategy_evaluation,
    const double exact_strategy_evaluation_time_limit_seconds) {
    CaseResult report;
    report.verification_skipped = skip_verification;
    const auto total_begin = Clock::now();
    report.working_set_before = process_working_set();
    const std::string backend =
        optional_string(specification, "execution_backend", "artifact");
    if (backend == "native_unit_fixture") {
        report.actual_status = "covered_by_native_unit_gate";
        report.expectation_met = true;
        report.working_set_after = process_working_set();
        report.total_ms = milliseconds(total_begin, Clock::now());
        return report;
    }
    if (!optional_bool(specification, "benchmark_enabled", false)) {
        report.actual_status = "not_run_approval_pending";
        report.expectation_met = true;
        report.working_set_after = process_working_set();
        report.total_ms = milliseconds(total_begin, Clock::now());
        return report;
    }

    NativeHandles handles;
    try {
        pc_item_state start_item{};
        const auto create_begin = Clock::now();
        create_case_objects(
            data, specification, handles, start_item,
            &report.product_action_ids);
        report.registry_layout_ms = milliseconds(create_begin, Clock::now());

        const Value& caps = required(specification, "caps", Type::Object);
        pc_solve_options solve_options{};
        solve_options.struct_size = sizeof(solve_options);
        solve_options.abi_version = PC_ABI_VERSION;
        solve_options.max_states = optional_u32(caps, "max_states", 100000);
        solve_options.max_sweeps = optional_u32(caps, "max_sweeps", 100000);
        solve_options.max_discovered_states = optional_u32(
            caps, "max_discovered_states", solve_options.max_states);
        solve_options.max_expanded_states = optional_u32(
            caps, "max_expanded_states", solve_options.max_states);
        solve_options.max_state_action_rows = optional_u64(
            caps, "max_state_action_rows", 1000000);
        solve_options.max_transitions = optional_u64(
            caps, "max_transitions", 10000000);
        solve_options.max_reforge_work = optional_u64(
            caps, "max_reforge_work", solve_options.max_transitions);
        solve_options.max_solver_owned_bytes = optional_u64(
            caps, "max_solver_owned_bytes", 1073741824);
        solve_options.max_compiled_nodes = optional_u32(
            caps, "max_compiled_nodes", 100000);
        solve_options.max_compiled_edges = optional_u32(
            caps, "max_compiled_edges", 400000);
        solve_options.max_strategy_json_bytes = optional_u64(
            caps, "max_strategy_json_bytes", 67108864);
        solve_options.max_diagnostic_samples = optional_u32(
            caps, "max_diagnostic_samples", 32);
        solve_options.max_telemetry_json_bytes = optional_u64(
            caps, "max_telemetry_json_bytes", 1048576);
        const std::uint32_t work_items =
            optional_u32(caps, "solve_step_work_items", 1);
        pc_error_info error;
        pc_error_info_init(&error);
        const auto solve_begin = Clock::now();
        pc_result result = pc_solver_solve_begin(
            handles.solver, &start_item, handles.economy, &solve_options,
            &error);
        if (result != PC_RESULT_OK) {
            throw std::runtime_error(api_error("pc_solver_solve_begin", result,
                                               error));
        }
        const bool cancel_after_first =
            optional_string(specification, "benchmark_mode") ==
            "cancel_after_first_step";
        const bool bounded_first_expansion =
            optional_string(specification, "benchmark_mode") ==
            "bounded_first_expansion";
        const bool stop_after_bellman_entry =
            optional_string(specification, "benchmark_mode") ==
            "stop_after_bellman_entry";
        report.diagnostic_work_limit = optional_u64(
            caps, "max_diagnostic_work_items", 0);
        if (const Value* time_limit = optional(
                caps, "diagnostic_time_limit_seconds", Type::Number)) {
            report.diagnostic_time_limit_seconds = time_limit->number;
        }
        const auto diagnostic_deadline =
            report.diagnostic_time_limit_seconds > 0.0
                ? solve_begin + std::chrono::duration_cast<Clock::duration>(
                      std::chrono::duration<double>(
                          report.diagnostic_time_limit_seconds))
                : Clock::time_point::max();
        pc_solve_progress progress{};
        auto next_progress = Clock::now() + std::chrono::seconds(10);
        do {
            const auto step_begin = Clock::now();
            result = pc_solver_solve_step(
                handles.solver, work_items, &progress, &error);
            const double step_ms = milliseconds(step_begin, Clock::now());
            if (step_ms > report.max_solve_step_ms) {
                report.max_solve_step_ms = step_ms;
                report.max_solve_step_phase = progress.phase;
                report.max_solve_step_expanded_states =
                    progress.expanded_states;
                report.max_solve_step_sweeps = progress.sweeps;
            }
            ++report.solve_steps;
            if (result != PC_RESULT_OK) {
                throw std::runtime_error(api_error("pc_solver_solve_step", result,
                                                   error));
            }
            if (emit_progress && Clock::now() >= next_progress) {
                std::cout << required_string(specification, "id")
                          << ": progress phase=" << progress.phase
                          << " expanded=" << progress.expanded_states
                          << " sweeps=" << progress.sweeps
                          << " residual=" << progress.residual
                          << " start_bound=" << progress.start_value_bound
                          << std::endl;
                next_progress = Clock::now() + std::chrono::seconds(10);
            }
            if (cancel_after_first) {
                const auto cancel_begin = Clock::now();
                pc_solver_solve_abandon(handles.solver);
                report.cooperative_abandon_ms =
                    milliseconds(cancel_begin, Clock::now());
                report.has_cooperative_abandon = true;
                report.actual_status = "cancelled";
                break;
            }
            if (stop_after_bellman_entry && !progress.done &&
                progress.phase == PC_SOLVE_PHASE_ITERATING) {
                pc_solver_solve_abandon(handles.solver);
                report.actual_status = "bellman_entry_reached";
                report.diagnostic_stop_reason =
                    "focused_gate_after_complete_expansion";
                break;
            }
            if (bounded_first_expansion && !progress.done &&
                report.diagnostic_work_limit != 0 &&
                report.solve_steps >= report.diagnostic_work_limit) {
                pc_solver_solve_abandon(handles.solver);
                report.actual_status = "diagnostic_work_cap";
                report.diagnostic_stop_reason =
                    "max_diagnostic_work_items";
                break;
            }
            if (bounded_first_expansion && !progress.done &&
                Clock::now() >= diagnostic_deadline) {
                pc_solver_solve_abandon(handles.solver);
                report.actual_status = "diagnostic_time_cap";
                report.diagnostic_stop_reason =
                    "diagnostic_time_limit_seconds";
                break;
            }
        } while (!progress.done);
        report.solve_ms = milliseconds(solve_begin, Clock::now());

        const bool diagnostic_abandoned =
            (bounded_first_expansion || stop_after_bellman_entry) &&
            !progress.done;
        if (!cancel_after_first && !diagnostic_abandoned) {
            report.solve_summary = {};
            result = pc_solver_solve_finish(
                handles.solver, &report.solve_summary, &error);
            if (result != PC_RESULT_OK) {
                throw std::runtime_error(api_error("pc_solver_solve_finish", result,
                                                   error));
            }
            report.has_solve_summary = true;
            report.telemetry_json =
                query_telemetry(handles.solver, report.errors);
            report.actual_status = classify_completed_solve(
                specification, report.solve_summary, report.telemetry_json);
            if (bounded_first_expansion) {
                report.diagnostic_stop_reason = report.actual_status;
                report.actual_status =
                    report.solve_summary.expanded_states <= 1
                        ? "bounded_first_expansion_complete"
                        : "harness_error";
            }
        } else {
            report.telemetry_json =
                query_telemetry(handles.solver, report.errors);
        }

        const std::string expected_compile = required_string(
            required(specification, "expected", Type::Object),
            "compile_status");
        if (!bounded_first_expansion && report.actual_status == "converged" &&
            expected_compile != "not_required" &&
            expected_compile != "not_expected_in_s7_0") {
            const auto compile_begin = Clock::now();
            std::size_t strategy_length = 0;
            result = pc_solver_compile_strategy(
                handles.solver, nullptr, 0, &strategy_length, &error);
            if (result == PC_RESULT_OK || result == PC_RESULT_BUFFER_TOO_SMALL) {
                std::string strategy_json(strategy_length + 1, '\0');
                result = pc_solver_compile_strategy(
                    handles.solver, strategy_json.data(), strategy_json.size(),
                    &strategy_length, &error);
                if (result == PC_RESULT_OK) {
                    strategy_json.resize(strategy_length);
                    report.strategy_json_bytes = strategy_length;
                    if (!strategy_output.empty()) {
                        std::string file_id =
                            required_string(specification, "id");
                        for (char& c : file_id) {
                            const bool safe =
                                (c >= 'a' && c <= 'z') ||
                                (c >= 'A' && c <= 'Z') ||
                                (c >= '0' && c <= '9') || c == '-' ||
                                c == '_' || c == '.';
                            if (!safe) c = '_';
                        }
                        const fs::path strategy_path = fs::absolute(
                            strategy_output /
                            (file_id + ".strategy.json"));
                        write_file(strategy_path, strategy_json);
                        report.strategy_output_path =
                            strategy_path.string();
                    }
                    result = pc_strategy_compile_json(
                        handles.session, strategy_json.data(), strategy_json.size(),
                        &handles.strategy, &error);
                    report.compile_ms =
                        milliseconds(compile_begin, Clock::now());
                    if (result != PC_RESULT_OK) {
                        report.errors.push_back(api_error(
                            "pc_strategy_compile_json", result, error));
                        report.actual_status = "compile_refused";
                    } else {
                        if (exact_strategy_evaluation) {
                            const auto evaluation_started = Clock::now();
                            pc_strategy_eval_options evaluation_options{};
                            evaluation_options.struct_size =
                                sizeof(evaluation_options);
                            evaluation_options.abi_version = PC_ABI_VERSION;
                            evaluation_options.epsilon = 1e-12;
                            evaluation_options.max_sweeps = 100000;
                            evaluation_options.max_states = optional_u32(
                                caps, "max_discovered_states", 300000);
                            evaluation_options.max_pairs = static_cast<
                                std::uint32_t>(std::min<std::uint64_t>(
                                optional_u64(
                                    caps, "max_state_action_rows", 3000000),
                                std::numeric_limits<std::uint32_t>::max()));
                            evaluation_options.max_transitions = static_cast<
                                std::uint32_t>(std::min<std::uint64_t>(
                                optional_u64(
                                    caps, "max_transitions", 30000000),
                                std::numeric_limits<std::uint32_t>::max()));
                            result = pc_strategy_eval_begin(
                                handles.strategy, &evaluation_options,
                                &handles.strategy_evaluation, &error);
                            if (result == PC_RESULT_OK) {
                                pc_strategy_eval_progress evaluation_progress{};
                                const auto evaluation_deadline =
                                    exact_strategy_evaluation_time_limit_seconds > 0.0
                                        ? evaluation_started +
                                              std::chrono::duration_cast<
                                                  Clock::duration>(
                                                  std::chrono::duration<double>(
                                                      exact_strategy_evaluation_time_limit_seconds))
                                        : Clock::time_point::max();
                                auto next_evaluation_progress =
                                    evaluation_started +
                                    std::chrono::seconds(10);
                                while (!evaluation_progress.done) {
                                    result = pc_strategy_eval_step(
                                        handles.strategy_evaluation, 1,
                                        &evaluation_progress, &error);
                                    if (result != PC_RESULT_OK) break;
                                    const auto now = Clock::now();
                                    if (emit_progress &&
                                        now >= next_evaluation_progress) {
                                        std::cout
                                            << required_string(
                                                   specification, "id")
                                            << ": exact evaluation phase="
                                            << evaluation_progress.phase
                                            << " pairs="
                                            << evaluation_progress.discovered_pairs
                                            << " pending="
                                            << evaluation_progress.pending_pairs
                                            << " sccs="
                                            << evaluation_progress.solved_sccs
                                            << '/'
                                            << evaluation_progress.total_sccs
                                            << std::endl;
                                        next_evaluation_progress =
                                            now + std::chrono::seconds(10);
                                    }
                                    if (!evaluation_progress.done &&
                                        now >= evaluation_deadline) {
                                        report.exact_strategy_evaluation_time_limited =
                                            true;
                                        break;
                                    }
                                }
                                if (result == PC_RESULT_OK &&
                                    evaluation_progress.done) {
                                    std::size_t length = 0;
                                    result = pc_strategy_eval_finish(
                                        handles.strategy_evaluation, nullptr, 0,
                                        &length, &error);
                                    if (result == PC_RESULT_OK ||
                                        result == PC_RESULT_BUFFER_TOO_SMALL) {
                                        std::string json(length + 1, '\0');
                                        result = pc_strategy_eval_finish(
                                            handles.strategy_evaluation,
                                            json.data(), json.size(), &length,
                                            &error);
                                        if (result == PC_RESULT_OK) {
                                            json.resize(length);
                                            report.has_exact_strategy_evaluation =
                                                true;
                                            report.exact_strategy_evaluation_json =
                                                std::move(json);
                                        }
                                    }
                                }
                            }
                            if (result != PC_RESULT_OK) {
                                report.errors.push_back(api_error(
                                    "exact strategy evaluation", result,
                                    error));
                            }
                            report.exact_strategy_evaluation_ms = milliseconds(
                                evaluation_started, Clock::now());
                            pc_strategy_eval_destroy(
                                handles.strategy_evaluation);
                            handles.strategy_evaluation = nullptr;
                            result = PC_RESULT_OK;
                        }
                        const Value& verification = required(
                            specification, "verification", Type::Object);
                        const std::uint64_t corpus_runs =
                            optional_u64(verification, "runs", 0);
                        const std::uint64_t runs =
                            verification_runs_override != 0
                                ? verification_runs_override
                                : corpus_runs;
                        report.verification_diagnostic =
                            verification_runs_override != 0 ||
                            verification_seed_override != 0 ||
                            verification_time_limit_seconds > 0.0;
                        report.verification_requested_runs = runs;
                        if (runs > 0 && !skip_verification) {
                            const auto verification_begin = Clock::now();
                            result = pc_simulator_create(
                                handles.session, handles.strategy,
                                handles.economy, &handles.simulator, &error);
                            if (result != PC_RESULT_OK) {
                                report.errors.push_back(api_error(
                                    "pc_simulator_create", result, error));
                            } else {
                                pc_simulation_options simulation_options{};
                                simulation_options.struct_size =
                                    sizeof(simulation_options);
                                simulation_options.abi_version = PC_ABI_VERSION;
                                simulation_options.target_runs = runs;
                                simulation_options.seed =
                                    verification_seed_override != 0
                                        ? verification_seed_override
                                        : optional_u64(
                                              verification, "seed", 20260715);
                                simulation_options.max_actions_per_run =
                                    optional_u32(verification,
                                                 "max_actions_per_run", 100000);
                                pc_simulation_progress simulation_progress{};
                                const auto simulation_started = Clock::now();
                                const auto simulation_deadline =
                                    verification_time_limit_seconds > 0.0
                                        ? simulation_started +
                                              std::chrono::duration_cast<
                                                  Clock::duration>(
                                                  std::chrono::duration<double>(
                                                      verification_time_limit_seconds))
                                        : Clock::time_point::max();
                                auto next_simulation_progress =
                                    simulation_started +
                                    std::chrono::seconds(10);
                                double variance_mean = 0.0;
                                double variance_m2 = 0.0;
                                std::uint64_t variance_count = 0;
                                double previous_total_cost = 0.0;
                                while (!simulation_progress.finished) {
                                    const std::uint64_t left =
                                        runs - simulation_progress.completed_runs;
                                    const std::uint32_t chunk =
                                        static_cast<std::uint32_t>(
                                            std::min<std::uint64_t>(
                                                left,
                                                std::max<std::uint32_t>(
                                                    1, verification_chunk_runs)));
                                    result = pc_simulator_run_chunk(
                                        handles.simulator, &simulation_options,
                                        chunk, &simulation_progress, &error);
                                    if (result != PC_RESULT_OK) break;
                                    pc_simulation_summary partial{};
                                    result = pc_simulator_get_summary(
                                        handles.simulator, &partial, &error);
                                    if (result != PC_RESULT_OK) break;
                                    if (chunk == 1 &&
                                        partial.completed_runs ==
                                            variance_count + 1) {
                                        const double sample =
                                            partial.known_total_cost -
                                            previous_total_cost;
                                        previous_total_cost =
                                            partial.known_total_cost;
                                        ++variance_count;
                                        const double delta =
                                            sample - variance_mean;
                                        variance_mean +=
                                            delta / variance_count;
                                        variance_m2 +=
                                            delta * (sample - variance_mean);
                                    }
                                    const auto now = Clock::now();
                                    const double elapsed_ms = milliseconds(
                                        simulation_started, now);
                                    if (partial.completed_runs > 0) {
                                        report.verification_projected_ms =
                                            elapsed_ms *
                                            static_cast<double>(runs) /
                                            static_cast<double>(
                                                partial.completed_runs);
                                    }
                                    if (emit_progress &&
                                        now >= next_simulation_progress) {
                                        const double remaining_ms = std::max(
                                            0.0,
                                            report.verification_projected_ms -
                                                elapsed_ms);
                                        std::cout
                                            << required_string(
                                                   specification, "id")
                                            << ": verification progress "
                                            << partial.completed_runs << '/'
                                            << runs << " elapsed_ms="
                                            << elapsed_ms << " eta_ms="
                                            << remaining_ms << std::endl;
                                        next_simulation_progress =
                                            now + std::chrono::seconds(10);
                                    }
                                    if (!simulation_progress.finished &&
                                        now >= simulation_deadline) {
                                        report.verification_time_limited = true;
                                        break;
                                    }
                                }
                                if (result == PC_RESULT_OK) {
                                    result = pc_simulator_get_summary(
                                        handles.simulator,
                                        &report.verification, &error);
                                }
                                if (result == PC_RESULT_OK) {
                                    report.has_verification = true;
                                    report.verification_finished =
                                        report.verification.completed_runs ==
                                        runs;
                                    if (variance_count > 1) {
                                        report.has_verification_cost_variance =
                                            true;
                                        report.verification_cost_standard_deviation =
                                            std::sqrt(
                                                variance_m2 /
                                                static_cast<double>(
                                                    variance_count - 1));
                                        report.verification_cost_standard_error =
                                            report.verification_cost_standard_deviation /
                                            std::sqrt(static_cast<double>(
                                                variance_count));
                                    }
                                    if (report.verification.completed_runs > 0) {
                                        report.verification_mean_cost =
                                            report.verification.known_total_cost /
                                            static_cast<double>(
                                                report.verification.completed_runs);
                                        report.cost_delta_absolute = std::fabs(
                                            report.verification_mean_cost -
                                            report.solve_summary.start_value);
                                        report.cost_delta_relative =
                                            report.solve_summary.start_value != 0.0
                                                ? report.cost_delta_absolute /
                                                      std::fabs(report.solve_summary.start_value)
                                                : 0.0;
                                    }
                                    std::uint32_t action_count = 0;
                                    result =
                                        pc_simulator_action_distribution_query(
                                            handles.simulator, nullptr, 0,
                                            &action_count, &error);
                                    if (result == PC_RESULT_OK ||
                                        result == PC_RESULT_BUFFER_TOO_SMALL) {
                                        std::vector<
                                            pc_action_distribution_entry>
                                            entries(action_count);
                                        result =
                                            pc_simulator_action_distribution_query(
                                                handles.simulator,
                                                entries.data(), action_count,
                                                &action_count, &error);
                                        if (result == PC_RESULT_OK) {
                                            report.action_distribution.reserve(
                                                action_count);
                                            for (std::uint32_t i = 0;
                                                 i < action_count; ++i) {
                                                report.action_distribution.push_back({
                                                    entries[i].node_id == nullptr
                                                        ? std::string()
                                                        : entries[i].node_id,
                                                    entries[i].action_type,
                                                    entries[i].count});
                                            }
                                        }
                                    }
                                } else {
                                    report.errors.push_back(api_error(
                                        "simulation verification", result, error));
                                }
                            }
                            report.verification_ms = milliseconds(
                                verification_begin, Clock::now());
                        }
                    }
                } else {
                    report.compile_ms =
                        milliseconds(compile_begin, Clock::now());
                    report.errors.push_back(api_error(
                        "pc_solver_compile_strategy", result, error));
                    report.actual_status = "compile_refused";
                }
            } else {
                report.compile_ms =
                    milliseconds(compile_begin, Clock::now());
                report.errors.push_back(api_error(
                    "pc_solver_compile_strategy(size)", result, error));
                report.actual_status = "compile_refused";
            }
            report.telemetry_json = query_telemetry(handles.solver, report.errors);
        }
    } catch (const std::exception& ex) {
        report.errors.push_back(ex.what());
        if (report.actual_status == "not_run") report.actual_status = "harness_error";
    }

    if (handles.solver != nullptr) {
        pc_error_info memory_error{};
        memory_error.struct_size = sizeof(memory_error);
        memory_error.abi_version = PC_ABI_VERSION;
        const pc_result memory_result = pc_solver_memory_stats(
            handles.solver, &report.native_memory, &memory_error);
        if (memory_result == PC_RESULT_OK) {
            report.has_native_memory = true;
        } else {
            report.errors.push_back(api_error(
                "pc_solver_memory_stats", memory_result, memory_error));
        }
    }

    evaluate_cap_checks(specification, report);
    report.expectation_met = evaluate_expectation(
        specification, report,
        skip_verification || report.verification_diagnostic);
    report.working_set_after = process_working_set();
    report.total_ms = milliseconds(total_begin, Clock::now());
    return report;
}

void append_nullable_number(std::ostringstream& out, bool present, double value) {
    if (!present || !std::isfinite(value)) out << "null";
    else out << std::setprecision(17) << value;
}

void append_case_report(
    std::ostringstream& out, const Value& specification,
    const CaseResult& result) {
    const bool measured =
        result.actual_status != "covered_by_native_unit_gate" &&
        result.actual_status != "not_run_approval_pending";
    out << "{\n";
    out << "  \"id\":" << escape_json(required_string(specification, "id")) << ",\n";
    out << "  \"category\":" << escape_json(required_string(specification, "category")) << ",\n";
    out << "  \"approval_status\":" << escape_json(required_string(specification, "approval_status")) << ",\n";
    out << "  \"benchmark_enabled\":"
        << (optional_bool(specification, "benchmark_enabled", false) ? "true" : "false") << ",\n";
    out << "  \"expected\":" << json_of(required(specification, "expected", Type::Object)) << ",\n";
    out << "  \"actual_status\":" << escape_json(result.actual_status) << ",\n";
    out << "  \"expectation_met\":" << (result.expectation_met ? "true" : "false") << ",\n";
    out << "  \"verification_skipped\":"
        << (result.verification_skipped ? "true" : "false") << ",\n";
    out << "  \"input\":{";
    bool first_input = true;
    for (const char* key : {"comparison_profile", "session", "start", "goal", "product_action_envelope", "allowed_mechanic_families", "economy", "caps", "verification"}) {
        const Value* value = specification.find(key);
        if (value == nullptr) continue;
        if (!first_input) out << ',';
        first_input = false;
        out << escape_json(key) << ':' << json_of(*value);
    }
    out << "},\n";
    out << "  \"phase_wall_ms\":{\"registry_layout\":";
    append_nullable_number(out, measured, result.registry_layout_ms);
    out << ",\"solve\":";
    append_nullable_number(out, measured, result.solve_ms);
    out << ",\"compile\":";
    append_nullable_number(out, result.compile_ms > 0.0, result.compile_ms);
    out << ",\"verification\":";
    append_nullable_number(out, result.verification_ms > 0.0,
                           result.verification_ms);
    out << ",\"total\":";
    append_nullable_number(out, measured, result.total_ms);
    out << "},\n";
    out << "  \"exact_strategy_evaluation\":";
    if (!result.has_exact_strategy_evaluation) {
        out << "{\"completed\":false,\"time_limited\":"
            << (result.exact_strategy_evaluation_time_limited
                    ? "true" : "false")
            << ",\"wall_ms\":";
        append_nullable_number(
            out, result.exact_strategy_evaluation_ms > 0.0,
            result.exact_strategy_evaluation_ms);
        out << "},\n";
    } else {
        out << "{\"completed\":true,\"time_limited\":false,\"wall_ms\":";
        append_nullable_number(out, true, result.exact_strategy_evaluation_ms);
        out << ",\"result\":" << result.exact_strategy_evaluation_json
            << "},\n";
    }
    out << "  \"product_action_ids\":[";
    for (std::size_t i = 0; i < result.product_action_ids.size(); ++i) {
        if (i != 0) out << ',';
        out << escape_json(result.product_action_ids[i]);
    }
    out << "],\n";
    out << "  \"execution\":{\"solve_steps\":";
    if (!measured || result.solve_steps == 0) out << "null";
    else out << result.solve_steps;
    out << ",\"max_solve_step_ms\":";
    append_nullable_number(out, measured && result.solve_steps > 0,
                           result.max_solve_step_ms);
    out << ",\"max_solve_step_phase\":";
    if (!measured || result.solve_steps == 0) out << "null";
    else out << result.max_solve_step_phase;
    out << ",\"max_solve_step_expanded_states\":";
    if (!measured || result.solve_steps == 0) out << "null";
    else out << result.max_solve_step_expanded_states;
    out << ",\"max_solve_step_sweeps\":";
    if (!measured || result.solve_steps == 0) out << "null";
    else out << result.max_solve_step_sweeps;
    out << ",\"diagnostic_work_limit\":";
    if (result.diagnostic_work_limit == 0) out << "null";
    else out << result.diagnostic_work_limit;
    out << ",\"diagnostic_time_limit_seconds\":";
    append_nullable_number(
        out, result.diagnostic_time_limit_seconds > 0.0,
        result.diagnostic_time_limit_seconds);
    out << ",\"diagnostic_stop_reason\":";
    if (result.diagnostic_stop_reason.empty()) out << "null";
    else out << escape_json(result.diagnostic_stop_reason);
    out << ','
        << "\"worker_max_slice_ms\":null,\"cancellation_ack_ms\":null,"
        << "\"cancellation_mode\":"
        << (result.has_cooperative_abandon
                ? "\"cooperative_abandon_after_step\""
                : "null")
        << ",\"cooperative_abandon_ms\":";
    append_nullable_number(out, result.has_cooperative_abandon,
                           result.cooperative_abandon_ms);
    out << "},\n";
    const std::int64_t delta =
        static_cast<std::int64_t>(result.working_set_after) -
        static_cast<std::int64_t>(result.working_set_before);
    out << "  \"memory\":{\"measurement_kind\":"
        << (measured ? "\"working_set_snapshots_not_peak\""
                     : "\"not_measured\"")
        << ",\"process_working_set_before_bytes\":";
    if (measured) out << result.working_set_before;
    else out << "null";
    out << ",\"process_working_set_after_bytes\":";
    if (measured) out << result.working_set_after;
    else out << "null";
    out << ",\"process_working_set_delta_bytes\":";
    if (measured) out << delta;
    else out << "null";
    out << ','
        << "\"process_working_set_peak_bytes\":null,"
        << "\"wasm_heap_before_bytes\":null,\"wasm_heap_after_bytes\":null,"
        << "\"wasm_heap_growth_bytes\":null,"
        << "\"native_live_owned_bytes\":";
    if (result.has_native_memory) out << result.native_memory.live_owned_bytes;
    else out << "null";
    out << ",\"native_peak_owned_bytes\":";
    if (result.has_native_memory) out << result.native_memory.peak_owned_bytes;
    else out << "null";
    out << ",\"native_serialized_output_bytes\":";
    if (result.has_native_memory) {
        out << result.native_memory.serialized_output_bytes;
    } else {
        out << "null";
    }
    out << ','
        << "\"solver_telemetry_json_bytes\":"
        << result.telemetry_json.size() << "},\n";
    out << "  \"solve_summary\":";
    if (!result.has_solve_summary) {
        out << "null";
    } else {
        out << '{'
            << "\"converged\":" << (result.solve_summary.converged ? "true" : "false") << ','
            << "\"start_state\":" << result.solve_summary.start_state << ','
            << "\"start_value\":";
        append_nullable_number(out, true, result.solve_summary.start_value);
        out << ",\"expanded_states\":" << result.solve_summary.expanded_states
            << ",\"sweeps\":" << result.solve_summary.sweeps
            << ",\"residual\":";
        append_nullable_number(out, true, result.solve_summary.residual);
        out << ",\"skipped_action_count\":"
            << result.solve_summary.skipped_action_count << '}';
    }
    out << ",\n  \"solver_telemetry\":"
        << (result.telemetry_json.empty() ? "null" : result.telemetry_json)
        << ",\n";
    out << "  \"compiled_graph\":";
    if (!result.has_compiled_graph) {
        out << "null";
    } else {
        out << "{\"nodes\":" << result.compiled_nodes
            << ",\"edges\":" << result.compiled_edges
            << ",\"strategy_json_bytes\":" << result.strategy_json_bytes
            << ",\"strategy_output_path\":";
        if (result.strategy_output_path.empty()) out << "null";
        else out << escape_json(result.strategy_output_path);
        out
            << '}';
    }
    out << ",\n";
    out << "  \"value\":{\"start\":";
    append_nullable_number(out,
                           result.has_solve_summary &&
                               result.solve_summary.converged &&
                               result.actual_status == "converged",
                           result.solve_summary.start_value);
    out << "},\n";
    out << "  \"verification\":";
    if (!result.has_verification) {
        out << "null";
    } else {
        const std::uint64_t off_policy =
            result.verification.no_matching_edge_count +
            result.verification.action_not_applied_count;
        const double success_rate =
            result.verification.completed_runs > 0
                ? static_cast<double>(result.verification.success_count) /
                      static_cast<double>(result.verification.completed_runs)
                : 0.0;
        const Value& verification_spec =
            required(specification, "verification", Type::Object);
        const Value* absolute_tolerance = optional(
            verification_spec, "mean_cost_absolute_tolerance", Type::Number);
        const Value* relative_tolerance = optional(
            verification_spec, "mean_cost_relative_tolerance", Type::Number);
        const Value* minimum_success = optional(
            verification_spec, "minimum_success_rate", Type::Number);
        const bool has_mean_tolerance =
            absolute_tolerance != nullptr || relative_tolerance != nullptr;
        const bool mean_within_tolerance =
            absolute_tolerance != nullptr
                ? result.cost_delta_absolute <= absolute_tolerance->number
                : relative_tolerance == nullptr ||
                      result.cost_delta_relative <= relative_tolerance->number;
        const bool has_success_tolerance = minimum_success != nullptr;
        const bool success_within_tolerance =
            minimum_success == nullptr || success_rate >= minimum_success->number;
        const bool verification_passed =
            off_policy == 0 && mean_within_tolerance && success_within_tolerance;
        out << '{'
            << "\"runs\":" << result.verification.completed_runs << ','
            << "\"requested_runs\":"
            << result.verification_requested_runs << ','
            << "\"finished\":"
            << (result.verification_finished ? "true" : "false") << ','
            << "\"diagnostic\":"
            << (result.verification_diagnostic ? "true" : "false") << ','
            << "\"time_limited\":"
            << (result.verification_time_limited ? "true" : "false") << ','
            << "\"projected_total_ms\":";
        append_nullable_number(
            out, result.verification_projected_ms > 0.0,
            result.verification_projected_ms);
        out << ",\"cost_standard_deviation\":";
        append_nullable_number(
            out, result.has_verification_cost_variance,
            result.verification_cost_standard_deviation);
        out << ",\"cost_standard_error\":";
        append_nullable_number(
            out, result.has_verification_cost_variance,
            result.verification_cost_standard_error);
        out << ','
            << "\"success_count\":" << result.verification.success_count << ','
            << "\"failure_count\":" << result.verification.failure_count << ','
            << "\"mean_cost\":" << result.verification_mean_cost << ','
            << "\"off_policy_failures\":" << off_policy << ','
            << "\"cost_delta_absolute\":" << result.cost_delta_absolute << ','
            << "\"cost_delta_relative\":" << result.cost_delta_relative << ','
            << "\"success_rate\":" << success_rate << ','
            << "\"action_distribution\":[";
        for (std::size_t i = 0;
             i < result.action_distribution.size(); ++i) {
            if (i != 0) out << ',';
            const auto& action = result.action_distribution[i];
            out << "{\"node_id\":" << escape_json(action.node_id)
                << ",\"action_type\":" << action.action_type
                << ",\"count\":" << action.count << '}';
        }
        out << "],"
            << "\"mean_cost_within_tolerance\":";
        if (!has_mean_tolerance) out << "null";
        else out << (mean_within_tolerance ? "true" : "false");
        out << ",\"success_rate_within_tolerance\":";
        if (!has_success_tolerance) out << "null";
        else out << (success_within_tolerance ? "true" : "false");
        out << ",\"verification_passed\":"
            << (verification_passed ? "true" : "false") << '}';
    }
    out << ",\n  \"cap_checks\":{\"all_passed\":";
    const bool caps_passed = std::all_of(
        result.cap_checks.begin(), result.cap_checks.end(),
        [](const auto& check) { return check.second; });
    out << (caps_passed ? "true" : "false") << ",\"checks\":{";
    for (std::size_t index = 0; index < result.cap_checks.size(); ++index) {
        if (index != 0) out << ',';
        out << escape_json(result.cap_checks[index].first) << ':'
            << (result.cap_checks[index].second ? "true" : "false");
    }
    out << "}},\n  \"errors\":[";
    for (std::size_t index = 0; index < result.errors.size(); ++index) {
        if (index != 0) out << ',';
        out << escape_json(result.errors[index]);
    }
    out << "]\n}";
}

Arguments parse_arguments(int argc, char** argv) {
    Arguments args;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        const auto value = [&](const char* name) -> std::string {
            if (++index >= argc) {
                throw std::runtime_error(std::string("missing value for ") + name);
            }
            return argv[index];
        };
        if (argument == "--artifact") args.artifact = value("--artifact");
        else if (argument == "--corpus") args.corpus = value("--corpus");
        else if (argument == "--output") args.output = value("--output");
        else if (argument == "--strategy-output") {
            args.strategy_output = value("--strategy-output");
        }
        else if (argument == "--case") args.case_id = value("--case");
        else if (argument == "--validate-only") args.validate_only = true;
        else if (argument == "--progress") args.emit_progress = true;
        else if (argument == "--verification-runs") {
            args.verification_runs = std::stoull(value("--verification-runs"));
        }
        else if (argument == "--verification-seed") {
            args.verification_seed = std::stoull(value("--verification-seed"));
        }
        else if (argument == "--verification-chunk-runs") {
            args.verification_chunk_runs = static_cast<std::uint32_t>(
                std::stoul(value("--verification-chunk-runs")));
            if (args.verification_chunk_runs == 0) {
                throw std::runtime_error(
                    "--verification-chunk-runs must be positive");
            }
        }
        else if (argument == "--verification-time-limit-seconds") {
            args.verification_time_limit_seconds = std::stod(
                value("--verification-time-limit-seconds"));
            if (!(args.verification_time_limit_seconds > 0.0)) {
                throw std::runtime_error(
                    "--verification-time-limit-seconds must be positive");
            }
        }
        else if (argument == "--exact-strategy-evaluation") {
            args.exact_strategy_evaluation = true;
        }
        else if (argument ==
                 "--exact-strategy-evaluation-time-limit-seconds") {
            args.exact_strategy_evaluation_time_limit_seconds = std::stod(
                value("--exact-strategy-evaluation-time-limit-seconds"));
            if (!(args.exact_strategy_evaluation_time_limit_seconds > 0.0)) {
                throw std::runtime_error(
                    "--exact-strategy-evaluation-time-limit-seconds must be positive");
            }
        }
        else if (argument == "--skip-verification") {
            args.skip_verification = true;
        }
        else throw std::runtime_error("unknown argument: " + argument);
    }
    if (args.artifact.empty()) throw std::runtime_error("--artifact is required");
    if (args.corpus.empty()) throw std::runtime_error("--corpus is required");
    if (!args.validate_only && args.output.empty()) {
        throw std::runtime_error("--output is required unless --validate-only is used");
    }
    return args;
}

std::string compiler_name() {
#if defined(_MSC_VER)
    return "msvc-" + std::to_string(_MSC_VER);
#elif defined(__clang__)
    return "clang-" + std::string(__clang_version__);
#elif defined(__GNUC__)
    return "gcc-" + std::string(__VERSION__);
#else
    return "unknown";
#endif
}

} // namespace

int main(int argc, char** argv) {
    try {
        const Arguments args = parse_arguments(argc, argv);
        const fs::path corpus_path = fs::absolute(args.corpus);
        const fs::path corpus_dir = corpus_path.parent_path();
        const std::string manifest_text = read_file(corpus_path);
        const Value manifest = parse_json(manifest_text, corpus_path);
        if (required_string(manifest, "schema_version") !=
            "solver_benchmark_corpus_v1") {
            throw std::runtime_error("unsupported corpus schema_version");
        }
        const Value& case_paths = required(manifest, "cases", Type::Array);

        pc_error_info error;
        pc_error_info_init(&error);
        pc_data_handle data = nullptr;
        const fs::path artifact_manifest =
            fs::absolute(args.artifact) / "manifest.json";
        const Value artifact_json = parse_json(
            read_file(artifact_manifest), artifact_manifest);
        const Value& artifact_pin = required(manifest, "artifact", Type::Object);
        if (optional_u32(artifact_pin, "engine_abi_version", 0) !=
            pc_abi_version()) {
            throw std::runtime_error("corpus engine ABI pin does not match this build");
        }
        if (optional_u32(artifact_pin, "artifact_schema_version", 0) !=
            optional_u32(artifact_json, "artifact_schema_version", 0)) {
            throw std::runtime_error("corpus artifact schema pin does not match the artifact");
        }
        const Value& artifact_source =
            required(artifact_json, "source", Type::Object);
        if (required_string(artifact_pin, "source_version") !=
                required_string(artifact_source, "source_version") ||
            required_string(artifact_pin, "source_data_hash") !=
                required_string(artifact_source, "data_hash")) {
            throw std::runtime_error("corpus source version/hash pin does not match the artifact");
        }
        const Value& artifact_files =
            required(artifact_json, "files", Type::Object);
        if (required_string(artifact_pin, "game_data_sha256") !=
                required_string(required(artifact_files, "game-data.json", Type::Object),
                                "sha256") ||
            required_string(artifact_pin, "strings_sha256") !=
                required_string(required(artifact_files, "strings.json", Type::Object),
                                "sha256")) {
            throw std::runtime_error("corpus file hash pins do not match the artifact");
        }
        const std::string artifact_manifest_string = artifact_manifest.string();
        const pc_result load_result = pc_data_load_file(
            artifact_manifest_string.c_str(), &data, &error);
        if (load_result != PC_RESULT_OK) {
            throw std::runtime_error(api_error("pc_data_load_file", load_result,
                                               error));
        }

        std::vector<Value> specifications;
        specifications.reserve(case_paths.array.size());
        try {
            for (const Value& relative : case_paths.array) {
                if (relative.type != Type::String) {
                    throw std::runtime_error("corpus case paths must be strings");
                }
                const fs::path path = corpus_dir / relative.string;
                Value specification = parse_json(read_file(path), path);
                validate_case_shape(specification);
                const std::string id = required_string(specification, "id");
                if (!args.case_id.empty() && id != args.case_id) continue;
                specifications.push_back(std::move(specification));
            }
            if (!args.case_id.empty() && specifications.empty()) {
                throw std::runtime_error("unknown corpus case: " + args.case_id);
            }

            if (args.validate_only) {
                for (const Value& specification : specifications) {
                    if (optional_string(specification, "execution_backend", "artifact") ==
                        "native_unit_fixture") {
                        continue;
                    }
                    NativeHandles handles;
                    pc_item_state start_item{};
                    create_case_objects(
                        data, specification, handles, start_item);
                }
                std::cout << "Validated " << specifications.size()
                          << " solver benchmark specifications.\n";
                pc_data_destroy(data);
                return 0;
            }

            std::ostringstream output;
            output << "{\n"
                   << "\"schema_version\":\"solver_benchmark_report_v1\",\n"
                   << "\"runner\":\"native\",\n"
                   << "\"corpus\":{\"id\":"
                   << escape_json(required_string(manifest, "corpus_id"))
                   << ",\"schema_version\":\"solver_benchmark_corpus_v1\","
                   << "\"manifest_path\":" << escape_json(corpus_path.string())
                   << ",\"manifest\":" << json_of(manifest)
                   << "},\n"
                   << "\"artifact\":{\"manifest_path\":"
                   << escape_json(artifact_manifest.string())
                   << ",\"manifest\":" << json_of(artifact_json) << "},\n"
                   << "\"environment\":{\"abi_version\":" << pc_abi_version()
                   << ",\"compiler\":" << escape_json(compiler_name())
                   << ",\"process_working_set_bytes\":" << process_working_set()
                   << "},\n\"cases\":[\n";
            bool first = true;
            bool all_expected = true;
            for (const Value& specification : specifications) {
                const CaseResult result = run_case(
                    data, specification, args.skip_verification,
                    args.strategy_output, args.emit_progress,
                    args.verification_runs, args.verification_seed,
                    args.verification_chunk_runs,
                    args.verification_time_limit_seconds,
                    args.exact_strategy_evaluation,
                    args.exact_strategy_evaluation_time_limit_seconds);
                if (!first) output << ",\n";
                first = false;
                append_case_report(output, specification, result);
                all_expected = all_expected && result.expectation_met;
                std::cout << required_string(specification, "id") << ": "
                          << result.actual_status << " (" << std::fixed
                          << std::setprecision(2) << result.total_ms << " ms)\n";
            }
            output << "\n],\n\"all_expectations_met\":"
                   << (all_expected ? "true" : "false") << "\n}\n";
            write_file(fs::absolute(args.output), output.str());
            pc_data_destroy(data);
            return all_expected ? 0 : 2;
        } catch (...) {
            pc_data_destroy(data);
            throw;
        }
    } catch (const std::exception& ex) {
        std::cerr << "solver benchmark: " << ex.what() << '\n';
        return 1;
    }
}
