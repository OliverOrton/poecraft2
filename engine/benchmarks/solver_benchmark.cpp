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
    std::string case_id;
    bool validate_only = false;
};

struct NativeHandles {
    pc_session_handle session = nullptr;
    pc_solver_handle solver = nullptr;
    pc_economy_handle economy = nullptr;
    pc_strategy_handle strategy = nullptr;
    pc_simulator_handle simulator = nullptr;

    ~NativeHandles() {
        pc_simulator_destroy(simulator);
        pc_strategy_destroy(strategy);
        pc_economy_destroy(economy);
        pc_solver_destroy(solver);
        pc_session_destroy(session);
    }
};

struct CaseResult {
    std::string actual_status = "not_run";
    bool expectation_met = false;
    double registry_layout_ms = 0.0;
    double solve_ms = 0.0;
    double compile_ms = 0.0;
    double verification_ms = 0.0;
    double total_ms = 0.0;
    std::uint64_t solve_steps = 0;
    double max_solve_step_ms = 0.0;
    double cooperative_abandon_ms = 0.0;
    bool has_cooperative_abandon = false;
    std::uint64_t working_set_before = 0;
    std::uint64_t working_set_after = 0;
    bool has_solve_summary = false;
    pc_solve_summary solve_summary{};
    std::string telemetry_json;
    std::size_t strategy_json_bytes = 0;
    std::uint64_t compiled_nodes = 0;
    std::uint64_t compiled_edges = 0;
    bool has_compiled_graph = false;
    bool has_verification = false;
    pc_simulation_summary verification{};
    double verification_mean_cost = 0.0;
    double cost_delta_absolute = 0.0;
    double cost_delta_relative = 0.0;
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
    const Value& specification, const CaseResult& report) {
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
    if (required_string(expected, "verification_status") != "run") {
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
    pc_item_state& start_item) {
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
    const std::string goal =
        json_of(required(specification, "goal", Type::Object));
    result = pc_solver_create(handles.session, goal.data(), goal.size(),
                              &handles.solver, &error);
    if (result != PC_RESULT_OK) {
        throw std::runtime_error(api_error("pc_solver_create", result, error));
    }
    const std::string economy =
        json_of(required(specification, "economy", Type::Object));
    result = pc_economy_load_json(economy.data(), economy.size(),
                                  &handles.economy, &error);
    if (result != PC_RESULT_OK) {
        throw std::runtime_error(api_error("pc_economy_load_json", result, error));
    }
}

CaseResult run_case(pc_data_handle data, const Value& specification) {
    CaseResult report;
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

    try {
        NativeHandles handles;
        pc_item_state start_item{};
        const auto create_begin = Clock::now();
        create_case_objects(data, specification, handles, start_item);
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
        pc_solve_progress progress{};
        do {
            const auto step_begin = Clock::now();
            result = pc_solver_solve_step(
                handles.solver, work_items, &progress, &error);
            const double step_ms = milliseconds(step_begin, Clock::now());
            report.max_solve_step_ms =
                std::max(report.max_solve_step_ms, step_ms);
            ++report.solve_steps;
            if (result != PC_RESULT_OK) {
                throw std::runtime_error(api_error("pc_solver_solve_step", result,
                                                   error));
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
        } while (!progress.done);
        report.solve_ms = milliseconds(solve_begin, Clock::now());

        if (!cancel_after_first) {
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
        } else {
            report.telemetry_json =
                query_telemetry(handles.solver, report.errors);
        }

        const std::string expected_compile = required_string(
            required(specification, "expected", Type::Object),
            "compile_status");
        if (report.actual_status == "converged" &&
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
                        const Value& verification = required(
                            specification, "verification", Type::Object);
                        const std::uint64_t runs =
                            optional_u64(verification, "runs", 0);
                        if (runs > 0) {
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
                                simulation_options.seed = optional_u64(
                                    verification, "seed", 20260715);
                                simulation_options.max_actions_per_run =
                                    optional_u32(verification,
                                                 "max_actions_per_run", 100000);
                                pc_simulation_progress simulation_progress{};
                                while (!simulation_progress.finished) {
                                    const std::uint64_t left =
                                        runs - simulation_progress.completed_runs;
                                    const std::uint32_t chunk =
                                        static_cast<std::uint32_t>(
                                            std::min<std::uint64_t>(left, 10000));
                                    result = pc_simulator_run_chunk(
                                        handles.simulator, &simulation_options,
                                        chunk, &simulation_progress, &error);
                                    if (result != PC_RESULT_OK) break;
                                }
                                if (result == PC_RESULT_OK) {
                                    result = pc_simulator_get_summary(
                                        handles.simulator,
                                        &report.verification, &error);
                                }
                                if (result == PC_RESULT_OK) {
                                    report.has_verification = true;
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

    evaluate_cap_checks(specification, report);
    report.expectation_met = evaluate_expectation(specification, report);
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
    out << "  \"input\":{";
    bool first_input = true;
    for (const char* key : {"comparison_profile", "session", "start", "goal", "allowed_mechanic_families", "economy", "caps", "verification"}) {
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
    out << "  \"execution\":{\"solve_steps\":";
    if (!measured || result.solve_steps == 0) out << "null";
    else out << result.solve_steps;
    out << ",\"max_solve_step_ms\":";
    append_nullable_number(out, measured && result.solve_steps > 0,
                           result.max_solve_step_ms);
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
        << "\"wasm_heap_growth_bytes\":null},\n";
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
            << "\"success_count\":" << result.verification.success_count << ','
            << "\"failure_count\":" << result.verification.failure_count << ','
            << "\"mean_cost\":" << result.verification_mean_cost << ','
            << "\"off_policy_failures\":" << off_policy << ','
            << "\"cost_delta_absolute\":" << result.cost_delta_absolute << ','
            << "\"cost_delta_relative\":" << result.cost_delta_relative << ','
            << "\"success_rate\":" << success_rate << ','
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
        else if (argument == "--case") args.case_id = value("--case");
        else if (argument == "--validate-only") args.validate_only = true;
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
                    create_case_objects(data, specification, handles, start_item);
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
                const CaseResult result = run_case(data, specification);
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
