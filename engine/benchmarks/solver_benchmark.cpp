#include "poecraft/api.h"
#include "poecraft/item_state.h"
#include "poecraft/session.h"
#include "poecraft/simulator.h"
#include "poecraft/solver.h"

#include "json.hpp"
#include "solver_diagnostic_options.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
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
    fs::path partial_output;
    fs::path strategy_output;
    std::string case_id;
    bool validate_only = false;
    bool skip_verification = false;
    bool emit_progress = false;
    bool goal_progress_gated_reforges = false;
    std::uint32_t max_discovered_states_override = 0;
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
    struct CompiledOperationContractResult {
        std::string type;
        std::vector<std::pair<std::string, std::string>> string_parameters;
        bool checked = false;
        std::uint64_t matching_nodes = 0;
    };
    struct ActionCount {
        std::string node_id;
        std::int32_t action_type = -1;
        std::uint64_t count = 0;
    };
    struct ActionDescriptorCount {
        std::string action_id;
        std::uint64_t count = 0;
    };
    struct MaterialCount {
        std::string price_key;
        std::uint64_t count = 0;
    };
    struct BoundTraceEntry {
        double elapsed_ms = 0.0;
        std::int32_t phase = 0;
        std::uint32_t round = 0;
        double lower_bound = 0.0;
        double upper_bound = std::numeric_limits<double>::infinity();
        double absolute_gap = std::numeric_limits<double>::infinity();
        double relative_gap = std::numeric_limits<double>::infinity();
        std::int32_t incumbent_kind = PC_SOLVE_INCUMBENT_NONE;
        std::uint32_t discovered_states = 0;
        std::uint32_t expanded_states = 0;
        std::uint32_t frontier_states = 0;
        std::uint64_t rows = 0;
        std::uint64_t transitions = 0;
        std::uint64_t reforge_work = 0;
        std::uint64_t live_owned_bytes = 0;
        std::uint64_t peak_owned_bytes = 0;
        bool raised_lower = false;
        bool decreased_lower = false;
        bool lowered_upper = false;
        bool increased_upper = false;
    };
    std::string actual_status = "not_run";
    std::string solve_result_class = "not_run";
    std::string compile_status = "not_attempted";
    std::string exact_evaluation_status = "not_requested";
    std::string simulation_status = "not_requested";
    bool expectation_met = false;
    bool verification_skipped = false;
    double registry_layout_ms = 0.0;
    double solve_ms = 0.0;
    double compile_ms = 0.0;
    double verification_ms = 0.0;
    double total_ms = 0.0;
    std::uint64_t solve_steps = 0;
    std::vector<double> solve_step_wall_ms;
    std::uint64_t diagnostic_work_limit = 0;
    double diagnostic_time_limit_seconds = 0.0;
    std::string diagnostic_stop_reason;
    std::vector<std::string> product_action_ids;
    double max_solve_step_ms = 0.0;
    std::uint32_t max_solve_step_phase = 0;
    std::uint32_t max_solve_step_expanded_states = 0;
    std::uint32_t max_solve_step_sweeps = 0;
    std::uint64_t max_solve_step_finalization_work_item = 0;
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
    bool has_compiled_operation_contract = false;
    std::string compiled_operation_contract_type;
    std::vector<std::pair<std::string, std::string>>
        compiled_operation_contract_string_parameters;
    bool compiled_operation_contract_checked = false;
    std::uint64_t compiled_operation_contract_matching_nodes = 0;
    std::vector<CompiledOperationContractResult>
        compiled_operation_contracts;
    bool has_market_price_override_contracts = false;
    bool market_price_override_contracts_checked = false;
    std::uint64_t market_price_override_contract_count = 0;
    bool has_mechanic_family_control = false;
    std::string mechanic_family_control_id;
    std::vector<std::string> mechanic_family_registered_action_ids;
    std::vector<std::string> mechanic_family_synthetic_price_keys;
    std::string mechanic_family_synthetic_price_purpose;
    bool mechanic_family_registered = false;
    bool mechanic_family_legal_carrier_admitted = false;
    bool mechanic_family_scheduled = false;
    bool mechanic_family_exact_row_materialized = false;
    bool mechanic_family_selected_under_synthetic_price = false;
    bool mechanic_family_compiled = false;
    bool mechanic_family_independently_exact_evaluated = false;
    bool mechanic_family_control_passed = false;
    bool mechanic_family_synthetic_price_disclosed = false;
    std::string mechanic_family_failure_stage;
    bool has_material_ratio_contract = false;
    std::string material_ratio_numerator_key;
    std::string material_ratio_denominator_key;
    double material_ratio_expected = 0.0;
    bool material_ratio_require_positive_denominator = false;
    bool material_ratio_require_complete_pricing = false;
    bool material_ratio_exact_checked = false;
    bool material_ratio_exact_passed = false;
    double material_ratio_exact_numerator = 0.0;
    double material_ratio_exact_denominator = 0.0;
    bool material_ratio_exact_pricing_complete = false;
    bool material_ratio_sampled_checked = false;
    bool material_ratio_sampled_passed = false;
    std::uint64_t material_ratio_sampled_numerator = 0;
    std::uint64_t material_ratio_sampled_denominator = 0;
    bool material_ratio_sampled_pricing_complete = false;
    bool material_ratio_contract_checked = false;
    bool material_ratio_contract_passed = false;
    bool has_forced_winner_contract = false;
    std::string required_compiled_operation_type;
    bool required_compiled_operation_checked = false;
    std::uint64_t required_compiled_operation_node_count = 0;
    std::string dependency_action_id;
    bool dependency_must_not_be_primitive_candidate = false;
    bool dependency_primitive_candidate_checked = false;
    bool dependency_primitive_candidate_absent = false;
    double forced_winner_snapshot_action_price = 0.0;
    double forced_winner_action_price = 0.0;
    bool forced_winner_price_override_checked = false;
    bool has_bounded_best_policy_contract = false;
    bool bounded_best_policy_contract_checked = false;
    bool bounded_best_policy_contract_passed = false;
    bool bounded_best_policy_exact_path = false;
    bool bounded_best_policy_named_stop = false;
    bool bounded_best_policy_strict_gap = false;
    bool bounded_best_policy_open_obligations = false;
    bool bounded_best_policy_evaluated_upper = false;
    bool bounded_best_policy_cheapest_evaluated_incumbent = false;
    std::uint64_t bounded_best_policy_incremental_obligations = 0;
    std::uint64_t bounded_best_policy_refinement_obligations = 0;
    std::string bounded_best_policy_failure_reason;
    bool has_verification = false;
    bool verification_diagnostic = false;
    bool verification_finished = false;
    bool verification_time_limited = false;
    double watchdog_seconds = 0.0;
    bool watchdog_expired = false;
    std::uint64_t verification_requested_runs = 0;
    double verification_projected_ms = 0.0;
    double verification_cost_standard_deviation = 0.0;
    double verification_cost_standard_error = 0.0;
    bool has_verification_cost_variance = false;
    bool has_exact_strategy_evaluation = false;
    bool exact_strategy_evaluation_time_limited = false;
    double exact_strategy_evaluation_ms = 0.0;
    std::string exact_strategy_evaluation_json;
    bool exact_strategy_evaluation_converged = false;
    bool exact_strategy_evaluation_cost_complete = false;
    bool exact_strategy_evaluation_zero_off_policy = false;
    bool exact_strategy_evaluation_cost_reconciled = false;
    double exact_strategy_evaluation_cost = 0.0;
    double exact_strategy_evaluation_cost_delta_absolute = 0.0;
    double exact_strategy_evaluation_cost_delta_relative = 0.0;
    double exact_strategy_evaluation_terminal_success = 0.0;
    double exact_strategy_evaluation_off_policy_mass = 0.0;
    pc_simulation_summary verification{};
    double verification_mean_cost = 0.0;
    double cost_delta_absolute = 0.0;
    double cost_delta_relative = 0.0;
    std::vector<ActionCount> action_distribution;
    std::vector<ActionDescriptorCount> action_descriptor_distribution;
    std::vector<MaterialCount> material_distribution;
    std::vector<BoundTraceEntry> bound_trace;
    bool has_time_to_first_incumbent = false;
    double time_to_first_incumbent_ms = 0.0;
    std::vector<std::pair<double, double>> absolute_gap_milestones;
    std::vector<std::pair<double, double>> relative_gap_milestones;
    std::uint32_t max_discovered_states_override = 0;
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

void write_file_atomic(const fs::path& path, const std::string& text) {
    fs::path temporary = path;
    temporary += ".tmp";
    write_file(temporary, text);
#if defined(_WIN32)
    if (!MoveFileExW(
            temporary.c_str(), path.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        const DWORD error = GetLastError();
        std::error_code ignored;
        fs::remove(temporary, ignored);
        throw std::runtime_error(
            "unable to atomically replace " + path.string() +
            " (Windows error " + std::to_string(error) + ")");
    }
#else
    std::error_code error;
    fs::rename(temporary, path, error);
    if (error) {
        fs::remove(temporary);
        throw std::runtime_error(
            "unable to atomically replace " + path.string() +
            ": " + error.message());
    }
#endif
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

    const bool generated_reliability =
        required_string(specification, "category") ==
        "cross_base_reliability";
    const auto require_override_decision = [&economy](
                                               const char* object_name,
                                               const std::string& key) {
        const Value& decisions = required(economy, object_name, Type::Object);
        const Value* decision = decisions.find(key);
        if (decision == nullptr || decision->type != Type::String ||
            decision->string.empty()) {
            throw std::runtime_error(
                std::string("economy override requires a non-empty ") +
                object_name + " decision for " + key);
        }
    };

    const Value* forced_winner = optional(
        specification, "forced_winner_contract", Type::Object);
    const Value* disclosed_market_overrides = optional(
        specification, "market_price_override_contracts", Type::Array);
    if (disclosed_market_overrides != nullptr &&
        forced_winner == nullptr) {
        const Value* base_override = overrides.find("base");
        const std::size_t base_override_count =
            base_override == nullptr ? 0 : 1;
        if (required_string(economy, "override_purpose") !=
                "synthetic_forced_winner_gate_not_market_quote" ||
            overrides.object.size() !=
                disclosed_market_overrides->array.size() +
                    base_override_count ||
            (base_override != nullptr &&
             (base_override->type != Type::Number ||
              !std::isfinite(base_override->number) ||
              base_override->number <= 0.0))) {
            throw std::runtime_error(
                "disclosed market overrides plus an optional positive base "
                "must be the complete override set");
        }
        if (base_override != nullptr) {
            require_override_decision("missing_price_decisions", "base");
        }
        const auto same_price = [](const double lhs, const double rhs) {
            return std::fabs(lhs - rhs) <=
                   1e-12 * std::max({1.0, std::fabs(lhs), std::fabs(rhs)});
        };
        std::vector<std::pair<std::string, Value>> replacements;
        for (const Value& entry : disclosed_market_overrides->array) {
            if (entry.type != Type::Object) {
                throw std::runtime_error(
                    "market price override contracts must be objects");
            }
            const std::string key = required_string(entry, "price_key");
            const double declared_snapshot = required(
                entry, "snapshot_chaos_value", Type::Number).number;
            const double declared_override = required(
                entry, "override_chaos_value", Type::Number).number;
            const Value* source_price = source_prices.find(key);
            const Value* override = overrides.find(key);
            if (key == "base" || key.starts_with("beast:") ||
                source_price == nullptr ||
                source_price->type != Type::Number || override == nullptr ||
                override->type != Type::Number ||
                !same_price(source_price->number, declared_snapshot) ||
                !same_price(override->number, declared_override)) {
                throw std::runtime_error(
                    "disclosed market override does not match its pinned "
                    "quote and override: " + key);
            }
            if (std::any_of(
                    replacements.begin(), replacements.end(),
                    [&](const auto& replacement) {
                        return replacement.first == key;
                    })) {
                throw std::runtime_error(
                    "duplicate disclosed market override: " + key);
            }
            require_override_decision("manual_override_decisions", key);
            replacements.emplace_back(key, *override);
        }
        for (auto& [object_key, value] : snapshot.object) {
            if (object_key != "prices") continue;
            for (const auto& [replacement_key, replacement_value] :
                 replacements) {
                auto found = std::find_if(
                    value.object.begin(), value.object.end(),
                    [&](const auto& price) {
                        return price.first == replacement_key;
                    });
                if (found == value.object.end()) {
                    throw std::runtime_error(
                        "disclosed market key is absent from the pinned "
                        "snapshot: " + replacement_key);
                }
                found->second = replacement_value;
            }
            if (base_override != nullptr) {
                value.object.emplace_back("base", *base_override);
            }
            return json_of(snapshot);
        }
        throw std::runtime_error("pinned economy snapshot has no prices object");
    }
    if (forced_winner != nullptr) {
        if (required_string(economy, "override_purpose") !=
            "synthetic_forced_winner_gate_not_market_quote") {
            throw std::runtime_error(
                "forced winner price override requires the synthetic gate "
                "disclosure");
        }
        const std::string dependency = required_string(
            *forced_winner, "dependency_action_id");
        const double declared_snapshot_price = required(
            *forced_winner, "snapshot_action_price", Type::Number).number;
        const double declared_forced_price = required(
            *forced_winner, "forced_action_price", Type::Number).number;
        const Value* source_price = source_prices.find(dependency);
        const Value* base_override = overrides.find("base");
        const Value* action_override = overrides.find(dependency);
        const std::size_t additional_override_count =
            disclosed_market_overrides == nullptr
                ? 0
                : disclosed_market_overrides->array.size();
        const auto same_price = [](const double lhs, const double rhs) {
            return std::fabs(lhs - rhs) <=
                   1e-12 * std::max({1.0, std::fabs(lhs), std::fabs(rhs)});
        };
        if (overrides.object.size() != 2 + additional_override_count ||
            dependency == "base" ||
            source_price == nullptr || source_price->type != Type::Number ||
            base_override == nullptr || base_override->type != Type::Number ||
            base_override->number <= 0.0 || action_override == nullptr ||
            action_override->type != Type::Number ||
            action_override->number <= 0.0 ||
            !same_price(source_price->number, declared_snapshot_price) ||
            !same_price(action_override->number, declared_forced_price)) {
            throw std::runtime_error(
                "forced winner overrides must contain only a positive base "
                "value and the declared dependency value, with the pinned "
                "snapshot quote recorded exactly");
        }
        std::vector<std::pair<std::string, const Value*>>
            additional_replacements;
        if (disclosed_market_overrides != nullptr) {
            for (const Value& entry :
                 disclosed_market_overrides->array) {
                const std::string key = required_string(
                    entry, "price_key");
                const double declared_snapshot = required(
                    entry, "snapshot_chaos_value", Type::Number).number;
                const double declared_override = required(
                    entry, "override_chaos_value", Type::Number).number;
                const Value* additional_source = source_prices.find(key);
                const Value* additional_override = overrides.find(key);
                if (key == "base" || key == dependency ||
                    key.starts_with("beast:") ||
                    additional_source == nullptr ||
                    additional_source->type != Type::Number ||
                    additional_override == nullptr ||
                    additional_override->type != Type::Number ||
                    !same_price(
                        additional_source->number,
                        declared_snapshot) ||
                    !same_price(
                        additional_override->number,
                        declared_override) ||
                    std::any_of(
                        additional_replacements.begin(),
                        additional_replacements.end(),
                        [&](const auto& replacement) {
                            return replacement.first == key;
                        })) {
                    throw std::runtime_error(
                        "forced winner additional market override does not "
                        "match its pinned quote and override: " + key);
                }
                require_override_decision(
                    "manual_override_decisions", key);
                additional_replacements.emplace_back(
                    key, additional_override);
            }
        }
        require_override_decision("missing_price_decisions", "base");
        require_override_decision(
            "manual_override_decisions", dependency);
        for (auto& [key, value] : snapshot.object) {
            if (key != "prices") continue;
            bool replaced_dependency = false;
            for (auto& [price_key, price_value] : value.object) {
                if (price_key != dependency) continue;
                price_value = *action_override;
                replaced_dependency = true;
                break;
            }
            if (!replaced_dependency) {
                throw std::runtime_error(
                    "forced winner dependency is absent from the pinned "
                    "snapshot");
            }
            for (const auto& [replacement_key, replacement_value] :
                 additional_replacements) {
                const auto found = std::find_if(
                    value.object.begin(), value.object.end(),
                    [&](const auto& price) {
                        return price.first == replacement_key;
                    });
                if (found == value.object.end()) {
                    throw std::runtime_error(
                        "forced winner additional market key is absent "
                        "from the pinned snapshot: " + replacement_key);
                }
                found->second = *replacement_value;
            }
            value.object.emplace_back("base", *base_override);
            return json_of(snapshot);
        }
        throw std::runtime_error("pinned economy snapshot has no prices object");
    }

    const Value* override_purpose = economy.find("override_purpose");
    const bool legacy_base_disclosure =
        override_purpose != nullptr &&
        override_purpose->type == Type::String &&
        override_purpose->string ==
            "r3f_restart_route_gate_not_market_quote";
    if (overrides.object.size() != 1 ||
        overrides.object.front().first != "base" ||
        overrides.object.front().second.type != Type::Number ||
        overrides.object.front().second.number <= 0.0 ||
        (override_purpose != nullptr && !legacy_base_disclosure)) {
        throw std::runtime_error(
            "pinned benchmark permits only one disclosed positive base "
            "override when no forced-winner contract is present");
    }
    if (!legacy_base_disclosure && !generated_reliability) {
        require_override_decision("missing_price_decisions", "base");
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

double optional_nonnegative_double(
    const Value& object, const char* key, double fallback) {
    const Value* value = optional(object, key, Type::Number);
    if (value == nullptr) return fallback;
    if (!std::isfinite(value->number) || value->number < 0.0) {
        throw std::runtime_error(
            std::string("non-finite or negative field: ") + key);
    }
    return value->number;
}

void initialize_forced_winner_contract(
    const Value& specification, CaseResult& report) {
    const Value* contract = optional(
        specification, "forced_winner_contract", Type::Object);
    if (contract == nullptr) return;
    report.has_forced_winner_contract = true;
    report.required_compiled_operation_type = required_string(
        *contract, "required_compiled_operation_type");
    report.dependency_action_id = required_string(
        *contract, "dependency_action_id");
    report.dependency_must_not_be_primitive_candidate = required(
        *contract, "dependency_must_not_be_primitive_candidate",
        Type::Bool).boolean;
    report.forced_winner_snapshot_action_price = required(
        *contract, "snapshot_action_price", Type::Number).number;
    report.forced_winner_action_price = required(
        *contract, "forced_action_price", Type::Number).number;
}

void initialize_bounded_best_policy_contract(
    const Value& specification, CaseResult& report) {
    report.has_bounded_best_policy_contract = optional(
        specification, "bounded_best_policy_contract", Type::Object) !=
        nullptr;
}

void initialize_compiled_operation_contract(
    const Value& specification, CaseResult& report) {
    const auto append_contract = [&](const Value& contract) {
        CaseResult::CompiledOperationContractResult parsed;
        parsed.type = required_string(contract, "type");
        const Value* parameters = optional(
            contract, "required_string_parameters", Type::Object);
        if (parameters != nullptr) {
            for (const auto& [key, value] : parameters->object) {
                if (value.type != Type::String || key.empty() ||
                    value.string.empty()) {
                    throw std::runtime_error(
                        "compiled operation contract parameters must be "
                        "non-empty strings");
                }
                parsed.string_parameters.emplace_back(key, value.string);
            }
            std::sort(
                parsed.string_parameters.begin(),
                parsed.string_parameters.end());
        }
        report.compiled_operation_contracts.push_back(std::move(parsed));
    };
    const Value* contract = optional(
        specification, "compiled_operation_contract", Type::Object);
    if (contract != nullptr) {
        report.has_compiled_operation_contract = true;
        append_contract(*contract);
        report.compiled_operation_contract_type =
            report.compiled_operation_contracts.back().type;
        report.compiled_operation_contract_string_parameters =
            report.compiled_operation_contracts.back().string_parameters;
    }
    const Value* contracts = optional(
        specification, "compiled_operation_contracts", Type::Array);
    if (contracts != nullptr) {
        for (const Value& entry : contracts->array) {
            if (entry.type != Type::Object) {
                throw std::runtime_error(
                    "compiled operation contracts must be objects");
            }
            append_contract(entry);
        }
    }
}

void initialize_material_ratio_contract(
    const Value& specification, CaseResult& report) {
    const Value* contract = optional(
        specification, "material_ratio_contract", Type::Object);
    if (contract == nullptr) return;
    report.has_material_ratio_contract = true;
    report.material_ratio_numerator_key = required_string(
        *contract, "numerator_price_key");
    report.material_ratio_denominator_key = required_string(
        *contract, "denominator_price_key");
    report.material_ratio_expected = required(
        *contract, "expected_ratio", Type::Number).number;
    report.material_ratio_require_positive_denominator = required(
        *contract, "require_positive_denominator", Type::Bool).boolean;
    report.material_ratio_require_complete_pricing = required(
        *contract, "require_complete_pricing", Type::Bool).boolean;
}

void initialize_market_price_override_contracts(
    const Value& specification, CaseResult& report) {
    const Value* contracts = optional(
        specification, "market_price_override_contracts", Type::Array);
    if (contracts == nullptr) return;
    report.has_market_price_override_contracts = true;
    report.market_price_override_contract_count = contracts->array.size();
}

void initialize_mechanic_family_control(
    const Value& specification, CaseResult& report) {
    const Value* contract = optional(
        specification, "mechanic_family_control", Type::Object);
    if (contract == nullptr) return;
    report.has_mechanic_family_control = true;
    report.mechanic_family_control_id = required_string(*contract, "id");
    report.mechanic_family_synthetic_price_purpose = required_string(
        *contract, "synthetic_price_purpose");
    const auto read_strings = [](const Value& values, const char* field) {
        std::vector<std::string> result;
        for (const Value& value : values.array) {
            if (value.type != Type::String || value.string.empty()) {
                throw std::runtime_error(
                    std::string(field) + " must contain non-empty strings");
            }
            result.push_back(value.string);
        }
        if (result.empty()) {
            throw std::runtime_error(
                std::string(field) + " must be non-empty");
        }
        std::sort(result.begin(), result.end());
        if (std::adjacent_find(result.begin(), result.end()) != result.end()) {
            throw std::runtime_error(
                std::string(field) + " must not contain duplicates");
        }
        return result;
    };
    report.mechanic_family_registered_action_ids = read_strings(
        required(*contract, "registered_action_ids", Type::Array),
        "mechanic family registered_action_ids");
    report.mechanic_family_synthetic_price_keys = read_strings(
        required(*contract, "synthetic_price_keys", Type::Array),
        "mechanic family synthetic_price_keys");

    const Value& economy = required(specification, "economy", Type::Object);
    const Value* prices = optional(economy, "prices", Type::Object);
    const Value* overrides = optional(
        economy, "manual_overrides", Type::Object);
    report.mechanic_family_synthetic_price_disclosed =
        report.mechanic_family_synthetic_price_purpose.find(
            "not market evidence") != std::string::npos;
    for (const std::string& key :
         report.mechanic_family_synthetic_price_keys) {
        const Value* value = prices == nullptr ? nullptr : prices->find(key);
        if (value == nullptr && overrides != nullptr) {
            value = overrides->find(key);
        }
        if (value == nullptr || value->type != Type::Number ||
            !std::isfinite(value->number) || value->number <= 0.0) {
            report.mechanic_family_synthetic_price_disclosed = false;
        }
    }
}

std::uint64_t nonnegative_telemetry_count(const Value* value) {
    if (value == nullptr || value->type != Type::Number ||
        !std::isfinite(value->number) || value->number <= 0.0) {
        return 0;
    }
    const double maximum = static_cast<double>(
        std::numeric_limits<std::uint64_t>::max());
    if (value->number >= maximum) {
        return std::numeric_limits<std::uint64_t>::max();
    }
    return static_cast<std::uint64_t>(value->number);
}

std::uint64_t saturating_count_sum(
    const std::uint64_t left, const std::uint64_t right) {
    return right > std::numeric_limits<std::uint64_t>::max() - left
               ? std::numeric_limits<std::uint64_t>::max()
               : left + right;
}

bool costs_reconcile(
    const double left, const double right, const Value& verification) {
    if (!std::isfinite(left) || !std::isfinite(right)) return false;
    const double absolute_delta = std::fabs(left - right);
    const double relative_delta = std::fabs(right) > 1e-12
        ? absolute_delta / std::fabs(right)
        : absolute_delta;
    const Value* absolute = optional(
        verification, "exact_cost_absolute_tolerance", Type::Number);
    const Value* relative = optional(
        verification, "exact_cost_relative_tolerance", Type::Number);
    return absolute_delta <= (absolute == nullptr ? 1e-7 : absolute->number) ||
           relative_delta <= (relative == nullptr ? 1e-9 : relative->number);
}

void enforce_bounded_best_policy_contract(
    const Value& specification, CaseResult& report) {
    const Value* contract = optional(
        specification, "bounded_best_policy_contract", Type::Object);
    if (contract == nullptr) return;
    report.bounded_best_policy_contract_checked = true;
    const bool allow_exact = required(
        *contract, "allow_exact", Type::Bool).boolean;
    if (!report.has_solve_summary ||
        !report.solve_summary.policy_available) {
        report.bounded_best_policy_failure_reason =
            "the solve did not publish an executable policy";
    } else {
        report.bounded_best_policy_exact_path =
            report.solve_summary.policy_status == PC_SOLVE_POLICY_EXACT &&
            report.solve_summary.termination ==
                PC_SOLVE_TERMINATION_EXACT_CLOSED;
        if (report.bounded_best_policy_exact_path) {
            report.bounded_best_policy_contract_passed = allow_exact;
            if (!allow_exact) {
                report.bounded_best_policy_failure_reason =
                    "the contract does not permit the exact completion path";
                report.errors.push_back(
                    "bounded best-policy contract failed: " +
                    report.bounded_best_policy_failure_reason);
            }
            return;
        }

        const bool bounded =
            report.solve_summary.policy_status ==
                PC_SOLVE_POLICY_BOUNDED_FEASIBLE ||
            report.solve_summary.policy_status ==
                PC_SOLVE_POLICY_BOUNDED_NEAR_OPTIMAL;
        report.bounded_best_policy_named_stop =
            (report.solve_summary.termination ==
                 PC_SOLVE_TERMINATION_REFUSED_RESOURCE_CAP &&
             report.solve_summary.stop_cause >= PC_SOLVE_STOP_STATE_CAP &&
             report.solve_summary.stop_cause <=
                 PC_SOLVE_STOP_COMPILED_OUTPUT_CAP) ||
            (report.solve_summary.termination ==
                 PC_SOLVE_TERMINATION_NUMERICAL_STABILITY &&
             report.solve_summary.stop_cause ==
                 PC_SOLVE_STOP_NUMERICAL_STABILITY) ||
            (report.solve_summary.termination ==
                 PC_SOLVE_TERMINATION_REQUESTED_BOUNDED_FINISH &&
             report.solve_summary.stop_cause ==
                 PC_SOLVE_STOP_REQUESTED_BOUNDED_FINISH);
        report.bounded_best_policy_strict_gap =
            std::isfinite(report.solve_summary.lower_bound) &&
            std::isfinite(report.solve_summary.upper_bound) &&
            report.solve_summary.lower_bound < report.solve_summary.upper_bound;

        const Value& verification = required(
            specification, "verification", Type::Object);
        report.bounded_best_policy_evaluated_upper = costs_reconcile(
            report.solve_summary.upper_bound,
            report.solve_summary.evaluated_policy_cost, verification);

        if (!report.telemetry_json.empty()) {
            const Value telemetry = Parser(
                report.telemetry_json.data(),
                report.telemetry_json.size()).parse();
            const Value* envelope = optional(
                telemetry, "incremental_action_envelope", Type::Object);
            if (envelope != nullptr) {
                const Value* remaining = optional(
                    *envelope, "remaining_action_envelope", Type::Number);
                report.bounded_best_policy_incremental_obligations =
                    nonnegative_telemetry_count(remaining);
                const Value* enabled = optional(
                    *envelope, "enabled", Type::Bool);
                const Value* closed = optional(
                    *envelope, "closed", Type::Bool);
                report.bounded_best_policy_open_obligations =
                    enabled != nullptr && enabled->boolean &&
                    closed != nullptr && !closed->boolean &&
                    report.bounded_best_policy_incremental_obligations > 0;
            }

            const Value* refinement = optional(
                telemetry, "policy_refinement", Type::Object);
            const Value* certification = refinement == nullptr
                ? nullptr
                : optional(
                      *refinement, "certification_work", Type::Object);
            if (certification != nullptr &&
                certification->type == Type::Object) {
                for (const char* key : {
                         "unresolved_alternative_obligations",
                         "competitive_alternatives_remaining",
                         "obligations_resource_interrupted"}) {
                    report.bounded_best_policy_refinement_obligations =
                        saturating_count_sum(
                            report.bounded_best_policy_refinement_obligations,
                            nonnegative_telemetry_count(
                                certification->find(key)));
                }
                report.bounded_best_policy_open_obligations =
                    report.bounded_best_policy_open_obligations ||
                    report.bounded_best_policy_refinement_obligations > 0;
            }

            const Value* fallback_portfolio = refinement == nullptr
                ? nullptr
                : optional(
                      *refinement, "fallback_portfolio", Type::Object);
            const Value* cheapest_selected = fallback_portfolio == nullptr
                ? nullptr
                : optional(
                      *fallback_portfolio,
                      "cheapest_independently_evaluated_selected",
                      Type::Bool);
            report.bounded_best_policy_cheapest_evaluated_incumbent =
                cheapest_selected != nullptr && cheapest_selected->boolean;
        }

        const bool named_stop_required = required(
            *contract, "bounded_requires_named_stop", Type::Bool).boolean;
        const bool strict_gap_required = required(
            *contract, "bounded_requires_strict_gap", Type::Bool).boolean;
        const bool open_obligations_required = required(
            *contract, "bounded_requires_open_obligations", Type::Bool)
                                                   .boolean;
        const bool evaluated_upper_required = required(
            *contract, "require_evaluated_upper", Type::Bool).boolean;
        const bool cheapest_incumbent_required = required(
            *contract,
            "require_cheapest_independently_evaluated_incumbent",
            Type::Bool).boolean;
        report.bounded_best_policy_contract_passed =
            bounded &&
            (!named_stop_required ||
             report.bounded_best_policy_named_stop) &&
            (!strict_gap_required ||
             report.bounded_best_policy_strict_gap) &&
            (!open_obligations_required ||
             report.bounded_best_policy_open_obligations) &&
            (!evaluated_upper_required ||
             report.bounded_best_policy_evaluated_upper) &&
            (!cheapest_incumbent_required ||
             report
                 .bounded_best_policy_cheapest_evaluated_incumbent);
        if (!report.bounded_best_policy_contract_passed) {
            report.bounded_best_policy_failure_reason =
                !bounded
                    ? "the result was neither exact nor a bounded policy"
                    : "one or more bounded-result proof obligations failed";
        }
    }
    if (!report.bounded_best_policy_contract_passed) {
        report.errors.push_back(
            "bounded best-policy contract failed: " +
            report.bounded_best_policy_failure_reason);
    }
}

void enforce_dependency_candidate_contract(CaseResult& report) {
    if (!report.has_forced_winner_contract ||
        !report.dependency_must_not_be_primitive_candidate) {
        return;
    }
    report.dependency_primitive_candidate_checked = true;
    report.dependency_primitive_candidate_absent = std::find(
        report.product_action_ids.begin(), report.product_action_ids.end(),
        report.dependency_action_id) == report.product_action_ids.end();
    if (!report.dependency_primitive_candidate_absent) {
        report.actual_status = "forced_winner_contract_failed";
        throw std::runtime_error(
            "forced winner dependency unexpectedly appeared as a primitive "
            "product candidate: " + report.dependency_action_id);
    }
}

void enforce_required_compiled_operation(
    const std::string& strategy_json, CaseResult& report) {
    if (!report.has_forced_winner_contract &&
        report.compiled_operation_contracts.empty()) {
        return;
    }
    const Value strategy = Parser(
        strategy_json.data(), strategy_json.size()).parse();
    const Value& nodes = required(strategy, "nodes", Type::Array);
    std::uint64_t matching_nodes = 0;
    for (auto& contract : report.compiled_operation_contracts) {
        contract.checked = true;
        contract.matching_nodes = 0;
    }
    for (const Value& node : nodes.array) {
        if (node.type != Type::Object) continue;
        const Value* kind = optional(node, "kind", Type::String);
        if (kind == nullptr || kind->string != "operation") continue;
        const Value* operation = optional(node, "operation", Type::Object);
        if (operation == nullptr) continue;
        const Value* type = optional(*operation, "type", Type::String);
        if (report.has_forced_winner_contract && type != nullptr &&
            type->string == report.required_compiled_operation_type) {
            ++matching_nodes;
        }
        if (type == nullptr) continue;
        for (auto& contract : report.compiled_operation_contracts) {
            if (type->string != contract.type) continue;
            bool parameters_match = true;
            for (const auto& [key, expected] :
                 contract.string_parameters) {
                const Value* actual = optional(
                    *operation, key.c_str(), Type::String);
                if (actual == nullptr || actual->string != expected) {
                    parameters_match = false;
                    break;
                }
            }
            if (parameters_match) ++contract.matching_nodes;
        }
    }
    if (report.has_forced_winner_contract) {
        report.required_compiled_operation_checked = true;
        report.required_compiled_operation_node_count = matching_nodes;
    }
    if (report.has_compiled_operation_contract &&
        !report.compiled_operation_contracts.empty()) {
        report.compiled_operation_contract_checked =
            report.compiled_operation_contracts.front().checked;
        report.compiled_operation_contract_matching_nodes =
            report.compiled_operation_contracts.front().matching_nodes;
    }
    if (report.has_forced_winner_contract && matching_nodes == 0) {
        report.compile_status = "compiled_contract_mismatch";
        report.actual_status = "forced_winner_contract_failed";
        throw std::runtime_error(
            "compiled strategy omitted forced winner operation type: " +
            report.required_compiled_operation_type);
    }
    for (const auto& contract : report.compiled_operation_contracts) {
        if (contract.matching_nodes == 0) {
            report.compile_status = "compiled_contract_mismatch";
            throw std::runtime_error(
                "compiled strategy omitted required operation contract: " +
                contract.type);
        }
    }
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
        if (tier->number < 0 || tier->number > 4) {
            throw std::runtime_error(
                "start searing_exarch_tier must be 0-4");
        }
        item.searing_exarch_tier = static_cast<std::uint8_t>(tier->number);
    }
    if (const Value* tier = optional(start, "eater_of_worlds_tier", Type::Number)) {
        if (tier->number < 0 || tier->number > 4) {
            throw std::runtime_error(
                "start eater_of_worlds_tier must be 0-4");
        }
        item.eater_of_worlds_tier = static_cast<std::uint8_t>(tier->number);
    }
    if (const Value* influences = optional(
            start, "generic_influence_bits", Type::Number)) {
        if (influences->number < 0 || influences->number > 255) {
            throw std::runtime_error(
                "start generic_influence_bits must be 0-255");
        }
        item.generic_influence_bits =
            static_cast<std::uint8_t>(influences->number);
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
    if (expected == "reliability_classified") {
        return actual == "converged" ||
               actual == "bounded_near_optimal" ||
               actual == "bounded_feasible" ||
               actual == "no_executable_policy" ||
               actual == "refused_state_cap" ||
               actual == "refused_sweep_cap" ||
               actual == "refused_resource_cap" ||
               actual == "refused_unsupported_action" ||
               actual == "refused_unreachable_goal";
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

void finalize_mechanic_family_control(CaseResult& report) {
    if (!report.has_mechanic_family_control) return;

    try {
        const Value telemetry = Parser(
            report.telemetry_json.data(), report.telemetry_json.size()).parse();
        const Value* ledger = nested_member(
            telemetry,
            {"incremental_action_envelope", "typed_ledger"});
        const Value* authoritative = ledger == nullptr
            ? nullptr
            : ledger->find("authoritative_obligation_owner");
        const Value* lifecycles = ledger == nullptr
            ? nullptr
            : ledger->find("action_lifecycles");
        bool every_registered =
            authoritative != nullptr && authoritative->type == Type::Bool &&
            authoritative->boolean && lifecycles != nullptr &&
            lifecycles->type == Type::Array;
        bool every_scheduled = every_registered;
        bool every_admitted = every_registered;
        bool every_materialized = every_registered;
        for (const std::string& action_id :
             report.mechanic_family_registered_action_ids) {
            bool registered = false;
            bool scheduled = false;
            bool admitted = false;
            bool materialized = false;
            if (lifecycles != nullptr && lifecycles->type == Type::Array) {
                for (const Value& lifecycle : lifecycles->array) {
                    if (lifecycle.type != Type::Object) continue;
                    const Value* raw_action = lifecycle.find("action_id");
                    if (raw_action == nullptr ||
                        raw_action->type != Type::String ||
                        raw_action->string != action_id) {
                        continue;
                    }
                    const Value* raw_registered = lifecycle.find("registered");
                    const Value* raw_scheduled = lifecycle.find("scheduled");
                    const Value* raw_materialized = lifecycle.find(
                        "exact_row_complete");
                    const Value* legality = lifecycle.find(
                        "exact_registry_legality");
                    const Value* kernel = lifecycle.find(
                        "exact_option_kernel");
                    registered = raw_registered != nullptr &&
                        raw_registered->type == Type::Bool &&
                        raw_registered->boolean;
                    scheduled = raw_scheduled != nullptr &&
                        raw_scheduled->type == Type::Bool &&
                        raw_scheduled->boolean;
                    materialized = raw_materialized != nullptr &&
                        raw_materialized->type == Type::Bool &&
                        raw_materialized->boolean;
                    admitted = legality != nullptr &&
                        legality->type == Type::Bool && legality->boolean &&
                        kernel != nullptr && kernel->type == Type::Bool &&
                        kernel->boolean;
                    break;
                }
            }
            every_registered = every_registered && registered;
            every_scheduled = every_scheduled && scheduled;
            every_admitted = every_admitted && admitted;
            every_materialized = every_materialized && materialized;
        }
        report.mechanic_family_registered = every_registered;
        report.mechanic_family_scheduled = every_scheduled;
        report.mechanic_family_legal_carrier_admitted = every_admitted;
        report.mechanic_family_exact_row_materialized = every_materialized;
    } catch (const std::exception&) {
        report.mechanic_family_registered = false;
        report.mechanic_family_scheduled = false;
        report.mechanic_family_legal_carrier_admitted = false;
        report.mechanic_family_exact_row_materialized = false;
    }

    const bool selected_operation_present =
        !report.compiled_operation_contracts.empty() &&
        std::all_of(
            report.compiled_operation_contracts.begin(),
            report.compiled_operation_contracts.end(),
            [](const auto& contract) {
                return contract.checked && contract.matching_nodes > 0;
            });
    report.mechanic_family_selected_under_synthetic_price =
        report.mechanic_family_synthetic_price_disclosed &&
        selected_operation_present;
    report.mechanic_family_compiled =
        report.compile_status == "compiled";
    report.mechanic_family_independently_exact_evaluated =
        report.exact_evaluation_status == "matched";
    const std::pair<const char*, bool> stages[] = {
        {"registered", report.mechanic_family_registered},
        {"legal_carrier_admitted",
         report.mechanic_family_legal_carrier_admitted},
        {"scheduled", report.mechanic_family_scheduled},
        {"exact_row_materialized",
         report.mechanic_family_exact_row_materialized},
        {"selected_under_disclosed_synthetic_price",
         report.mechanic_family_selected_under_synthetic_price},
        {"compiled", report.mechanic_family_compiled},
        {"independently_exact_evaluated",
         report.mechanic_family_independently_exact_evaluated},
    };
    report.mechanic_family_control_passed = true;
    for (const auto& [name, passed] : stages) {
        if (passed) continue;
        report.mechanic_family_control_passed = false;
        report.mechanic_family_failure_stage = name;
        report.errors.push_back(
            "mechanic family control " +
            report.mechanic_family_control_id + " lost stage " + name);
        break;
    }
}

std::string solve_result_class(const pc_solve_summary& summary) {
    if (summary.cap_hit_mask != 0) {
        return summary.policy_available
                   ? "capped_with_policy"
                   : "capped_without_policy";
    }
    switch (summary.policy_status) {
    case PC_SOLVE_POLICY_EXACT: return "exact";
    case PC_SOLVE_POLICY_BOUNDED_NEAR_OPTIMAL:
    case PC_SOLVE_POLICY_BOUNDED_FEASIBLE:
        return "bounded";
    default:
        return summary.termination ==
                       PC_SOLVE_TERMINATION_NO_EXECUTABLE_POLICY
                   ? "no_executable_policy"
                   : "no_policy";
    }
}

void inspect_exact_strategy_evaluation(
    const Value& specification,
    CaseResult& report) {
    const Value evaluation = Parser(
        report.exact_strategy_evaluation_json.data(),
        report.exact_strategy_evaluation_json.size()).parse();
    const Value* converged = evaluation.find("converged");
    const Value* success = nested_member(
        evaluation, {"terminals", "success"});
    const Value* failure = nested_member(
        evaluation, {"terminals", "failure"});
    const Value* stop = nested_member(
        evaluation, {"terminals", "stop"});
    const Value* action_not_applied = nested_member(
        evaluation, {"terminals", "action_not_applied"});
    const Value* no_matching_edge = nested_member(
        evaluation, {"terminals", "no_matching_edge"});
    const Value* unresolved = nested_member(
        evaluation, {"terminals", "unresolved"});
    const Value* cost_complete = nested_member(
        evaluation,
        {"accounting", "totals", "per_invocation", "cost_complete"});
    const Value* cost = nested_member(
        evaluation,
        {"accounting", "totals", "per_invocation",
         "total_expected_cost"});
    if (converged == nullptr || converged->type != Type::Bool ||
        success == nullptr || success->type != Type::Number ||
        failure == nullptr || failure->type != Type::Number ||
        stop == nullptr || stop->type != Type::Number ||
        action_not_applied == nullptr ||
        action_not_applied->type != Type::Number ||
        no_matching_edge == nullptr ||
        no_matching_edge->type != Type::Number ||
        unresolved == nullptr || unresolved->type != Type::Number ||
        cost_complete == nullptr || cost_complete->type != Type::Bool ||
        cost == nullptr || cost->type != Type::Number) {
        throw std::runtime_error(
            "exact strategy evaluation omitted required reliability fields");
    }
    report.exact_strategy_evaluation_converged = converged->boolean;
    report.exact_strategy_evaluation_cost_complete =
        cost_complete->boolean;
    report.exact_strategy_evaluation_terminal_success = success->number;
    report.exact_strategy_evaluation_off_policy_mass =
        failure->number + stop->number +
        action_not_applied->number + no_matching_edge->number +
        unresolved->number;
    report.exact_strategy_evaluation_zero_off_policy =
        report.exact_strategy_evaluation_off_policy_mass <= 1e-10 &&
        success->number >= 1.0 - 1e-10;
    report.exact_strategy_evaluation_cost = cost->number;
    if (report.has_material_ratio_contract) {
        report.material_ratio_exact_checked = true;
        const Value& consumption = required(
            evaluation, "expected_consumption", Type::Array);
        for (const Value& row : consumption.array) {
            if (row.type != Type::Object) continue;
            const Value* key = optional(row, "key", Type::String);
            const Value* quantity = optional(row, "quantity", Type::Number);
            if (key == nullptr || quantity == nullptr) continue;
            if (key->string == report.material_ratio_numerator_key) {
                report.material_ratio_exact_numerator = quantity->number;
            } else if (
                key->string == report.material_ratio_denominator_key) {
                report.material_ratio_exact_denominator = quantity->number;
            }
        }
        const Value* pricing_status = nested_member(
            evaluation, {"accounting", "pricing", "status"});
        const Value* missing_prices = nested_member(
            evaluation,
            {"accounting", "pricing", "missing_price_keys"});
        report.material_ratio_exact_pricing_complete =
            pricing_status != nullptr && pricing_status->type == Type::String &&
            pricing_status->string == "complete" &&
            missing_prices != nullptr && missing_prices->type == Type::Array &&
            missing_prices->array.empty();
        const double expected_numerator =
            report.material_ratio_expected *
            report.material_ratio_exact_denominator;
        const double tolerance = 1e-10 * std::max(
            {1.0, std::fabs(report.material_ratio_exact_numerator),
             std::fabs(expected_numerator)});
        const bool positive_denominator =
            !report.material_ratio_require_positive_denominator ||
            report.material_ratio_exact_denominator > 0.0;
        report.material_ratio_exact_passed =
            positive_denominator &&
            std::fabs(
                report.material_ratio_exact_numerator -
                expected_numerator) <= tolerance &&
            (!report.material_ratio_require_complete_pricing ||
             report.material_ratio_exact_pricing_complete);
    }
    report.exact_strategy_evaluation_cost_delta_absolute = std::fabs(
        cost->number - report.solve_summary.evaluated_policy_cost);
    report.exact_strategy_evaluation_cost_delta_relative =
        std::fabs(report.solve_summary.evaluated_policy_cost) > 1e-12
            ? report.exact_strategy_evaluation_cost_delta_absolute /
                  std::fabs(report.solve_summary.evaluated_policy_cost)
            : report.exact_strategy_evaluation_cost_delta_absolute;
    const Value& verification =
        required(specification, "verification", Type::Object);
    const Value* absolute = optional(
        verification, "exact_cost_absolute_tolerance", Type::Number);
    const Value* relative = optional(
        verification, "exact_cost_relative_tolerance", Type::Number);
    const double absolute_tolerance =
        absolute == nullptr ? 1e-7 : absolute->number;
    const double relative_tolerance =
        relative == nullptr ? 1e-9 : relative->number;
    report.exact_strategy_evaluation_cost_reconciled =
        report.exact_strategy_evaluation_cost_delta_absolute <=
            absolute_tolerance ||
        report.exact_strategy_evaluation_cost_delta_relative <=
            relative_tolerance;
    if (report.exact_strategy_evaluation_converged &&
        report.exact_strategy_evaluation_cost_complete &&
        report.exact_strategy_evaluation_zero_off_policy &&
        report.exact_strategy_evaluation_cost_reconciled) {
        report.exact_evaluation_status = "matched";
    } else {
        report.exact_evaluation_status = "mismatch";
        report.errors.push_back(
            "exact strategy evaluation did not reconcile with the "
            "solver policy");
    }
}

void finalize_material_ratio_contract(
    CaseResult& report, const bool skip_verification) {
    if (!report.has_material_ratio_contract) return;
    if (report.has_verification) {
        report.material_ratio_sampled_checked = true;
        for (const auto& material : report.material_distribution) {
            if (material.price_key == report.material_ratio_numerator_key) {
                report.material_ratio_sampled_numerator = material.count;
            } else if (
                material.price_key == report.material_ratio_denominator_key) {
                report.material_ratio_sampled_denominator = material.count;
            }
        }
        report.material_ratio_sampled_pricing_complete =
            report.verification.cost_status == PC_COST_COMPLETE &&
            report.verification.missing_price_run_count == 0 &&
            report.verification.missing_price_action_count == 0;
        const double expected_numerator =
            report.material_ratio_expected *
            static_cast<double>(report.material_ratio_sampled_denominator);
        const double tolerance = 1e-10 * std::max(
            {1.0,
             static_cast<double>(report.material_ratio_sampled_numerator),
             std::fabs(expected_numerator)});
        const bool positive_denominator =
            !report.material_ratio_require_positive_denominator ||
            report.material_ratio_sampled_denominator > 0;
        report.material_ratio_sampled_passed =
            positive_denominator &&
            std::fabs(
                static_cast<double>(report.material_ratio_sampled_numerator) -
                expected_numerator) <= tolerance &&
            (!report.material_ratio_require_complete_pricing ||
             report.material_ratio_sampled_pricing_complete);
    }
    report.material_ratio_contract_checked =
        report.material_ratio_exact_checked &&
        (skip_verification || report.material_ratio_sampled_checked);
    report.material_ratio_contract_passed =
        report.material_ratio_exact_passed &&
        (skip_verification || report.material_ratio_sampled_passed);
    if (!skip_verification && !report.material_ratio_contract_passed) {
        report.errors.push_back(
            "material ratio contract failed exact or sampled accounting");
    }
}

void add_cap_check(
    CaseResult& report, const Value& telemetry,
    std::initializer_list<const char*> metric_path, const Value& caps,
    const char* cap_name, const double cap_override = 0.0,
    const bool named_cap_hit = false) {
    const Value* metric = nested_member(telemetry, metric_path);
    const Value* cap = optional(caps, cap_name, Type::Number);
    if (metric == nullptr || metric->type != Type::Number ||
        (cap == nullptr && cap_override <= 0.0)) {
        return;
    }
    report.cap_checks.emplace_back(
        cap_name,
        named_cap_hit ||
            metric->number <=
                (cap_override > 0.0 ? cap_override : cap->number));
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
        const Value* cap_hits = nested_member(
            telemetry, {"optimization", "cap_hits"});
        const auto named_cap_hit = [&](const char* name) {
            return cap_hits != nullptr && cap_hits->type == Type::Array &&
                   std::any_of(
                       cap_hits->array.begin(), cap_hits->array.end(),
                       [&](const Value& entry) {
                           return entry.type == Type::String &&
                                  entry.string == name;
                       });
        };
        add_cap_check(report, telemetry, {"states", "discovered"}, caps,
                      "max_discovered_states",
                      report.max_discovered_states_override,
                      named_cap_hit("max_discovered_states"));
        add_cap_check(report, telemetry, {"states", "expanded"}, caps,
                      "max_expanded_states", 0.0,
                      named_cap_hit("max_expanded_states"));
        add_cap_check(report, telemetry, {"work", "state_action_rows"}, caps,
                      "max_state_action_rows", 0.0,
                      named_cap_hit("max_state_action_rows"));
        add_cap_check(report, telemetry, {"work", "transition_entries"}, caps,
                      "max_transitions", 0.0,
                      named_cap_hit("max_transitions"));
        add_cap_check(report, telemetry,
                      {"cache", "reforge", "frontier_work"}, caps,
                      "max_reforge_work", 0.0,
                      named_cap_hit("max_reforge_work"));
        add_cap_check(report, telemetry,
                      {"memory", "solver_owned_bytes_estimate"}, caps,
                      "max_solver_owned_bytes", 0.0,
                      named_cap_hit("max_solver_owned_bytes"));
        add_cap_check(report, telemetry, {"compilation", "nodes"}, caps,
                      "max_compiled_nodes", 0.0,
                      named_cap_hit("max_compiled_nodes"));
        add_cap_check(report, telemetry, {"compilation", "edges"}, caps,
                      "max_compiled_edges", 0.0,
                      named_cap_hit("max_compiled_edges"));
        add_cap_check(report, telemetry,
                      {"compilation", "strategy_json_bytes"}, caps,
                      "max_strategy_json_bytes", 0.0,
                      named_cap_hit("max_strategy_json_bytes"));
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
    if (report.watchdog_expired || report.telemetry_json.empty() ||
        !report.errors.empty()) {
        return false;
    }
    if (report.has_forced_winner_contract) {
        if (!report.forced_winner_price_override_checked ||
            !report.required_compiled_operation_checked ||
            report.required_compiled_operation_node_count == 0) {
            return false;
        }
        if (report.dependency_must_not_be_primitive_candidate &&
            (!report.dependency_primitive_candidate_checked ||
             !report.dependency_primitive_candidate_absent)) {
            return false;
        }
    }
    if (report.has_bounded_best_policy_contract &&
        (!report.bounded_best_policy_contract_checked ||
         !report.bounded_best_policy_contract_passed)) {
        return false;
    }
    if (report.has_compiled_operation_contract &&
        (!report.compiled_operation_contract_checked ||
         report.compiled_operation_contract_matching_nodes == 0)) {
        return false;
    }
    if (std::any_of(
            report.compiled_operation_contracts.begin(),
            report.compiled_operation_contracts.end(),
            [](const auto& contract) {
                return !contract.checked || contract.matching_nodes == 0;
            })) {
        return false;
    }
    if (report.has_market_price_override_contracts &&
        !report.market_price_override_contracts_checked) {
        return false;
    }
    if (report.has_mechanic_family_control &&
        !report.mechanic_family_control_passed) {
        return false;
    }
    if (report.has_material_ratio_contract &&
        (!report.material_ratio_exact_passed ||
         (!skip_verification &&
          !report.material_ratio_contract_passed))) {
        return false;
    }
    const Value& verification =
        required(specification, "verification", Type::Object);
    const bool exact_evaluation_required =
        optional_bool(verification, "exact_evaluation", false);
    if (report.has_solve_summary &&
        report.solve_summary.policy_available &&
        exact_evaluation_required &&
        report.exact_evaluation_status != "matched") {
        return false;
    }
    const Value* expected_contract = specification.find("expected");
    if (expected_contract == nullptr) {
        if (!report.has_solve_summary ||
            !report.solve_summary.policy_available ||
            report.strategy_json_bytes == 0) {
            return false;
        }
        for (const auto& [name, passed] : report.cap_checks) {
            (void)name;
            if (!passed) return false;
        }
        return true;
    }
    if (!expectation_matches(expected_solve_status(specification),
                             report.actual_status)) {
        return false;
    }
    for (const auto& [name, passed] : report.cap_checks) {
        (void)name;
        if (!passed) return false;
    }
    const Value& expected = *expected_contract;
    const Value telemetry = Parser(
        report.telemetry_json.data(), report.telemetry_json.size()).parse();
    for (const char* section : {
             "availability", "actions", "policy_compatibility",
             "abstraction", "states", "work", "cache", "optimization",
             "timings_ns", "memory", "compilation", "value"}) {
        if (optional(telemetry, section, Type::Object) == nullptr) return false;
    }
    const std::string expected_optimality =
        required_string(expected, "optimality_status");
    if (expected_optimality == "exact" ||
        expected_optimality == "exact_supported_priced_subset") {
        const Value* status = nested_member(
            telemetry, {"optimization", "status"});
        const bool status_matches =
            status != nullptr && status->type == Type::String &&
            (expected_optimality == "exact"
                 ? status->string == "exact_abstract"
                 : status->string == "exact_supported_priced_subset" ||
                       status->string == "exact_abstract");
        /* exact_abstract is stronger than a requested
         * exact_supported_priced_subset expectation: it means every action
         * actually reached by the exact proof was supported and priced. A
         * constructive certificate can make formerly observed unpriced
         * states unreachable without weakening the requested envelope. */
        if (!status_matches) {
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
    const std::uint64_t required_runs = optional_u64(
        verification, "runs", 0);
    const std::uint64_t required_seed = optional_u64(
        verification, "seed", 20260715);
    if (!report.has_verification || !report.verification_finished ||
        report.verification_time_limited || required_runs == 0 ||
        report.verification_requested_runs != required_runs ||
        report.verification.completed_runs != required_runs ||
        report.verification.target_runs != required_runs ||
        report.verification.seed != required_seed ||
        report.verification.cost_status != PC_COST_COMPLETE ||
        report.verification.missing_price_run_count != 0 ||
        report.verification.missing_price_action_count != 0) {
        return false;
    }
    const std::uint64_t off_policy =
        report.verification.no_matching_edge_count +
        report.verification.action_not_applied_count;
    if (off_policy != 0) return false;
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
    if (summary.stop_cause == PC_SOLVE_STOP_STATE_CAP) {
        return "refused_state_cap";
    }
    if (summary.stop_cause == PC_SOLVE_STOP_SWEEP_CAP) {
        return "refused_sweep_cap";
    }
    if (summary.cap_hit_mask != 0 ||
        summary.termination ==
            PC_SOLVE_TERMINATION_REFUSED_RESOURCE_CAP) {
        return "refused_resource_cap";
    }
    const Value* missing_price = nested_member(
        telemetry, {"actions", "missing_price"});
    const Value* unsupported_requested = nested_member(
        telemetry, {"actions", "unsupported_requested"});
    const Value* unsupported_observed = nested_member(
        telemetry, {"actions", "unsupported_observed"});
    const bool has_missing_price =
        missing_price != nullptr && missing_price->type == Type::Number &&
        missing_price->number > 0;
    const bool has_unsupported =
        (unsupported_requested != nullptr &&
         unsupported_requested->type == Type::Number &&
         unsupported_requested->number > 0) ||
        (unsupported_observed != nullptr &&
         unsupported_observed->type == Type::Number &&
         unsupported_observed->number > 0);
    const bool priced_product_envelope =
        specification.find("product_action_envelope") != nullptr;
    if (has_missing_price && has_unsupported && !priced_product_envelope) {
        return "refused_missing_price_and_unsupported_action";
    }
    if (has_missing_price && !priced_product_envelope) {
        return "refused_missing_price";
    }
    if (has_unsupported) return "refused_unsupported_action";
    const Value* full_request_status = nested_member(
        telemetry, {"optimization", "full_request_status"});
    if (full_request_status != nullptr &&
        full_request_status->type == Type::String &&
        full_request_status->string == "incomplete_action_subset" &&
        !priced_product_envelope) {
        return "incomplete_action_subset";
    }
    const Value* raw_start_bound = nested_member(
        telemetry, {"value", "raw_start_bound"});
    if (raw_start_bound != nullptr &&
        raw_start_bound->type == Type::Number &&
        raw_start_bound->number >= 1e12 && summary.residual == 0.0) {
        return "refused_unreachable_goal";
    }
    if (summary.policy_status == PC_SOLVE_POLICY_BOUNDED_NEAR_OPTIMAL) {
        return "bounded_near_optimal";
    }
    if (summary.policy_status == PC_SOLVE_POLICY_BOUNDED_FEASIBLE) {
        return "bounded_feasible";
    }
    if (summary.termination ==
        PC_SOLVE_TERMINATION_NO_EXECUTABLE_POLICY) {
        return "no_executable_policy";
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
    if (const Value* watchdog = optional(
            specification, "watchdog_seconds", Type::Number)) {
        if (!std::isfinite(watchdog->number) || watchdog->number <= 0.0) {
            throw std::runtime_error(
                "case watchdog_seconds must be a positive finite number");
        }
    }
    if (const Value* bounded_finish = optional(
            specification, "requested_bounded_finish_seconds",
            Type::Number)) {
        if (!std::isfinite(bounded_finish->number) ||
            bounded_finish->number <= 0.0) {
            throw std::runtime_error(
                "case requested_bounded_finish_seconds must be a positive "
                "finite number");
        }
        if (const Value* watchdog = optional(
                specification, "watchdog_seconds", Type::Number);
            watchdog != nullptr &&
            bounded_finish->number >= watchdog->number) {
            throw std::runtime_error(
                "case requested bounded finish must precede its watchdog");
        }
    }
    required(specification, "benchmark_enabled", Type::Bool);
    if (const Value* expected = specification.find("expected")) {
        if (expected->type != Type::Object) {
            throw std::runtime_error("case expected must be an object");
        }
    }
    const Value* forced_winner = optional(
        specification, "forced_winner_contract", Type::Object);
    if (forced_winner != nullptr) {
        const std::string automatic_kind = required_string(
            *forced_winner, "automatic_candidate_kind");
        const std::string dependency = required_string(
            *forced_winner, "dependency_action_id");
        required(
            *forced_winner,
            "dependency_must_not_be_primitive_candidate", Type::Bool);
        const std::string operation = required_string(
            *forced_winner, "required_compiled_operation_type");
        const std::string target = required_string(
            *forced_winner, "target_side");
        const std::string preserved = required_string(
            *forced_winner, "preserved_goal_side");
        const double snapshot_action_price = required(
            *forced_winner, "snapshot_action_price", Type::Number).number;
        const double forced_action_price = required(
            *forced_winner, "forced_action_price", Type::Number).number;
        const auto valid_side = [](const std::string& side) {
            return side == "prefix" || side == "suffix";
        };
        if (automatic_kind.empty() || dependency.empty() ||
            operation.empty()) {
            throw std::runtime_error(
                "forced winner contract strings must be non-empty");
        }
        if (!valid_side(target) || !valid_side(preserved) ||
            target == preserved) {
            throw std::runtime_error(
                "forced winner target/preserved sides must be opposite "
                "prefix and suffix sides");
        }
        if (!std::isfinite(snapshot_action_price) ||
            !std::isfinite(forced_action_price) ||
            snapshot_action_price <= 0.0 || forced_action_price <= 0.0 ||
            forced_action_price >= snapshot_action_price) {
            throw std::runtime_error(
                "forced winner prices must be positive and the disclosed "
                "forced value must be below the snapshot quote");
        }
        const Value* expected = optional(
            specification, "expected", Type::Object);
        if (expected == nullptr ||
            required_string(*expected, "compile_status") != "compiled") {
            throw std::runtime_error(
                "forced winner contracts require expected compile_status "
                "compiled");
        }
    }
    const Value* bounded_best = optional(
        specification, "bounded_best_policy_contract", Type::Object);
    if (bounded_best != nullptr) {
        for (const char* key : {
                 "allow_exact", "bounded_requires_named_stop",
                 "bounded_requires_strict_gap",
                 "bounded_requires_open_obligations",
                 "require_evaluated_upper",
                 "require_cheapest_independently_evaluated_incumbent"}) {
            required(*bounded_best, key, Type::Bool);
        }
        const Value& expected = required(
            specification, "expected", Type::Object);
        if (required_string(expected, "compile_status") != "compiled") {
            throw std::runtime_error(
                "bounded best-policy contract requires compiled output");
        }
        const Value& verification = required(
            specification, "verification", Type::Object);
        if (!optional_bool(verification, "exact_evaluation", false)) {
            throw std::runtime_error(
                "bounded best-policy contract requires exact evaluation");
        }
    }
    const Value* mechanic_family = optional(
        specification, "mechanic_family_control", Type::Object);
    if (mechanic_family != nullptr) {
        required_string(*mechanic_family, "id");
        const std::string purpose = required_string(
            *mechanic_family, "synthetic_price_purpose");
        if (purpose.find("not market evidence") == std::string::npos) {
            throw std::runtime_error(
                "mechanic family synthetic_price_purpose must disclose "
                "that the price is not market evidence");
        }
        const auto validate_string_set = [](const Value& values,
                                            const char* name) {
            if (values.array.empty()) {
                throw std::runtime_error(
                    std::string(name) + " must be non-empty");
            }
            std::vector<std::string> seen;
            for (const Value& value : values.array) {
                if (value.type != Type::String || value.string.empty() ||
                    std::find(seen.begin(), seen.end(), value.string) !=
                        seen.end()) {
                    throw std::runtime_error(
                        std::string(name) +
                        " must contain unique non-empty strings");
                }
                seen.push_back(value.string);
            }
        };
        const Value& action_ids = required(
            *mechanic_family, "registered_action_ids", Type::Array);
        const Value& price_keys = required(
            *mechanic_family, "synthetic_price_keys", Type::Array);
        validate_string_set(
            action_ids, "mechanic family registered_action_ids");
        validate_string_set(
            price_keys, "mechanic family synthetic_price_keys");
        const Value& economy = required(
            specification, "economy", Type::Object);
        const Value* prices = optional(economy, "prices", Type::Object);
        const Value* overrides = optional(
            economy, "manual_overrides", Type::Object);
        for (const Value& price_key : price_keys.array) {
            const Value* price = prices == nullptr
                ? nullptr
                : prices->find(price_key.string);
            if (price == nullptr && overrides != nullptr) {
                price = overrides->find(price_key.string);
            }
            if (price == nullptr || price->type != Type::Number ||
                !std::isfinite(price->number) || price->number <= 0.0) {
                throw std::runtime_error(
                    "mechanic family synthetic price is missing or not "
                    "positive: " + price_key.string);
            }
        }
        const Value& expected = required(
            specification, "expected", Type::Object);
        if (required_string(expected, "compile_status") != "compiled") {
            throw std::runtime_error(
                "mechanic family controls require compiled output");
        }
        const Value& verification = required(
            specification, "verification", Type::Object);
        if (!optional_bool(verification, "exact_evaluation", false)) {
            throw std::runtime_error(
                "mechanic family controls require exact evaluation");
        }
    }
    const auto validate_compiled_operation = [](const Value& contract) {
        required_string(contract, "type");
        const Value& parameters = required(
            contract, "required_string_parameters", Type::Object);
        for (const auto& [key, value] : parameters.object) {
            if (key.empty() || value.type != Type::String ||
                value.string.empty()) {
                throw std::runtime_error(
                    "compiled operation contract parameters must be "
                    "non-empty strings");
            }
        }
    };
    const Value* compiled_operation = optional(
        specification, "compiled_operation_contract", Type::Object);
    if (compiled_operation != nullptr) {
        validate_compiled_operation(*compiled_operation);
    }
    const Value* compiled_operations = optional(
        specification, "compiled_operation_contracts", Type::Array);
    if (compiled_operations != nullptr) {
        if (compiled_operations->array.empty()) {
            throw std::runtime_error(
                "compiled operation contracts must be non-empty");
        }
        for (const Value& contract : compiled_operations->array) {
            if (contract.type != Type::Object) {
                throw std::runtime_error(
                    "compiled operation contracts must be objects");
            }
            validate_compiled_operation(contract);
        }
    }
    if (mechanic_family != nullptr && compiled_operation == nullptr &&
        compiled_operations == nullptr) {
        throw std::runtime_error(
            "mechanic family controls require a compiled operation "
            "contract");
    }
    const Value* material_ratio = optional(
        specification, "material_ratio_contract", Type::Object);
    if (material_ratio != nullptr) {
        const std::string numerator = required_string(
            *material_ratio, "numerator_price_key");
        const std::string denominator = required_string(
            *material_ratio, "denominator_price_key");
        const double ratio = required(
            *material_ratio, "expected_ratio", Type::Number).number;
        required(
            *material_ratio, "require_positive_denominator", Type::Bool);
        required(*material_ratio, "require_complete_pricing", Type::Bool);
        if (numerator == denominator || !std::isfinite(ratio) ||
            ratio <= 0.0) {
            throw std::runtime_error(
                "material ratio contract requires distinct keys and a "
                "positive finite ratio");
        }
    }
    const Value* market_overrides = optional(
        specification, "market_price_override_contracts", Type::Array);
    if (market_overrides != nullptr) {
        if (market_overrides->array.empty()) {
            throw std::runtime_error(
                "market price override contracts must be non-empty");
        }
        std::vector<std::string> keys;
        for (const Value& contract : market_overrides->array) {
            if (contract.type != Type::Object) {
                throw std::runtime_error(
                    "market price override contracts must be objects");
            }
            const std::string key = required_string(contract, "price_key");
            required_string(contract, "purpose");
            const double snapshot = required(
                contract, "snapshot_chaos_value", Type::Number).number;
            const double override_value = required(
                contract, "override_chaos_value", Type::Number).number;
            if (key == "base" || key.starts_with("beast:") ||
                std::find(keys.begin(), keys.end(), key) != keys.end() ||
                !std::isfinite(snapshot) || snapshot <= 0.0 ||
                !std::isfinite(override_value) || override_value <= 0.0) {
                throw std::runtime_error(
                    "market override contracts require unique non-Bestiary "
                    "keys and positive prices");
            }
            keys.push_back(key);
        }
    }
    if (compiled_operation != nullptr || compiled_operations != nullptr ||
        material_ratio != nullptr || mechanic_family != nullptr) {
        const Value& expected = required(
            specification, "expected", Type::Object);
        if (required_string(expected, "compile_status") != "compiled") {
            throw std::runtime_error(
                "compiled-policy contracts require compiled output");
        }
    }
    const std::string backend =
        optional_string(specification, "execution_backend", "artifact");
    if (backend == "native_unit_fixture") {
        if (forced_winner != nullptr || bounded_best != nullptr ||
            compiled_operation != nullptr || compiled_operations != nullptr ||
            material_ratio != nullptr || market_overrides != nullptr ||
            mechanic_family != nullptr) {
            throw std::runtime_error(
                "compiled-policy acceptance contracts require the artifact "
                "benchmark backend");
        }
        required(specification, "unit_fixture", Type::Object);
        return;
    }
    const Value& session = required(specification, "session", Type::Object);
    required_string(session, "base_metadata_path");
    required(session, "item_level", Type::Number);
    required(specification, "start", Type::Object);
    const Value& goal = required(specification, "goal", Type::Object);
    required(goal, "slots", Type::Array);
    const Value& economy = required(
        specification, "economy", Type::Object);
    if (forced_winner != nullptr) {
        required(economy, "snapshot_path", Type::String);
        if (required_string(economy, "override_purpose") !=
            "synthetic_forced_winner_gate_not_market_quote") {
            throw std::runtime_error(
                "forced winner economy must disclose its synthetic price "
                "override purpose");
        }
        const Value& overrides = required(
            economy, "manual_overrides", Type::Object);
        const std::string dependency = required_string(
            *forced_winner, "dependency_action_id");
        const double forced_action_price = required(
            *forced_winner, "forced_action_price", Type::Number).number;
        const Value* base_override = overrides.find("base");
        const Value* action_override = overrides.find(dependency);
        bool additional_overrides_valid = true;
        std::size_t additional_override_count = 0;
        if (market_overrides != nullptr) {
            additional_override_count = market_overrides->array.size();
            for (const Value& raw_contract : market_overrides->array) {
                const std::string key = required_string(
                    raw_contract, "price_key");
                const double declared = required(
                    raw_contract, "override_chaos_value",
                    Type::Number).number;
                const Value* value = overrides.find(key);
                if (key == dependency || value == nullptr ||
                    value->type != Type::Number ||
                    value->number != declared) {
                    additional_overrides_valid = false;
                }
            }
        }
        if (overrides.object.size() != 2 + additional_override_count ||
            dependency == "base" ||
            base_override == nullptr || base_override->type != Type::Number ||
            base_override->number <= 0.0 || action_override == nullptr ||
            action_override->type != Type::Number ||
            action_override->number != forced_action_price ||
            !additional_overrides_valid) {
            throw std::runtime_error(
                "forced winner economy must override only base, the "
                "declared dependency at the declared forced value, and "
                "fully declared additional market overrides");
        }
    }
    required(specification, "caps", Type::Object);
    const Value& verification = required(
        specification, "verification", Type::Object);
    if (material_ratio != nullptr &&
        (!optional_bool(verification, "exact_evaluation", false) ||
         optional_u64(verification, "runs", 0) == 0)) {
        throw std::runtime_error(
            "material ratio contract requires exact evaluation and sampled "
            "verification");
    }
}

void validate_case_manifest_contract(
    const Value& manifest, const Value& specification) {
    if (const Value* identity = optional(
            manifest, "benchmark_identity_contract", Type::Object)) {
        const std::string benchmark_identity =
            required_string(*identity, "id");
        required(*identity, "trajectory_required", Type::Bool);
        required(*identity, "fixed_work_identity_required", Type::Bool);
        required(
            *identity, "compiled_exact_evaluation_required", Type::Bool);
        const double repetitions = required(
            *identity, "native_repetitions", Type::Number).number;
        if (!std::isfinite(repetitions) || repetitions < 1.0 ||
            std::floor(repetitions) != repetitions) {
            throw std::runtime_error(
                "benchmark identity native_repetitions must be a positive "
                "integer");
        }
        required(*identity, "general_product_scope", Type::Object);
        required(*identity, "explicit_imprint_scope", Type::Object);
        required(*identity, "required_disclosures", Type::Array);
        required(manifest, "artifact", Type::Object);
        required(manifest, "source_checkpoint", Type::Object);

        const std::string case_id = required_string(specification, "id");
        const Value& roles = required(manifest, "case_roles", Type::Object);
        const Value* role = roles.find(case_id);
        if (role == nullptr || role->type != Type::String ||
            role->string.empty()) {
            throw std::runtime_error(
                case_id + " is missing its benchmark role identity");
        }
        const Value& economy = required(
            specification, "economy", Type::Object);
        required_string(economy, "id");
        if (economy.find("snapshot_path") == nullptr &&
            economy.find("prices") == nullptr) {
            throw std::runtime_error(
                case_id + " economy must pin a snapshot or frozen prices");
        }
        const Value& goal = required(specification, "goal", Type::Object);
        required(goal, "slots", Type::Array);
        required(goal, "min_satisfied_slots", Type::Number);
        if (specification.find("product_action_envelope") == nullptr &&
            goal.find("actions") == nullptr) {
            throw std::runtime_error(
                case_id + " must disclose product or explicit action scope");
        }
        required_string(specification, "comparison_profile");
        required(specification, "watchdog_seconds", Type::Number);
        const Value& caps = required(specification, "caps", Type::Object);
        if (benchmark_identity.starts_with(
                "calculator-product-quality-ladder-")) {
            required(caps, "solve_profile", Type::String);
        }
        required(caps, "solve_step_work_items", Type::Number);
        required(caps, "worker_step_ms", Type::Number);
        required(caps, "full_evidence", Type::Bool);
        const Value& verification = required(
            specification, "verification", Type::Object);
        if (!required(verification, "exact_evaluation", Type::Bool).boolean) {
            throw std::runtime_error(
                case_id + " benchmark identity requires exact evaluation");
        }
    }

    const Value* profile = optional(
        manifest, "comparison_profile", Type::Object);
    if (profile == nullptr) return;
    const std::string profile_id = required_string(*profile, "id");
    const bool bounded_profile =
        profile->find("product_scope") != nullptr ||
        profile->find("maximum_wall_seconds") != nullptr ||
        profile->find("exact_required") != nullptr;
    if (!bounded_profile) return;

    required_string(*profile, "product_scope");
    const bool exact_required = required(
        *profile, "exact_required", Type::Bool).boolean;
    const bool exact_evaluation_required = required(
        *profile, "compiled_exact_evaluation_required", Type::Bool).boolean;
    const double maximum_wall_seconds = required(
        *profile, "maximum_wall_seconds", Type::Number).number;
    const double verification_runs = required(
        *profile, "verification_runs", Type::Number).number;
    if (!std::isfinite(maximum_wall_seconds) ||
        maximum_wall_seconds <= 0.0) {
        throw std::runtime_error(
            "manifest maximum_wall_seconds must be a positive finite number");
    }
    if (!std::isfinite(verification_runs) || verification_runs < 0.0 ||
        std::floor(verification_runs) != verification_runs ||
        verification_runs > 9007199254740991.0) {
        throw std::runtime_error(
            "manifest verification_runs must be a non-negative safe integer");
    }
    const std::string case_id = required_string(specification, "id");
    if (required_string(specification, "comparison_profile") != profile_id) {
        throw std::runtime_error(
            case_id + " comparison profile does not match its corpus");
    }
    const Value& watchdog = required(
        specification, "watchdog_seconds", Type::Number);
    if (!std::isfinite(watchdog.number) || watchdog.number <= 0.0 ||
        watchdog.number > maximum_wall_seconds) {
        throw std::runtime_error(
            case_id +
            " watchdog must be positive and within its corpus profile");
    }
    const Value& expected = required(
        specification, "expected", Type::Object);
    if (exact_required &&
        required_string(expected, "optimality_status") != "exact") {
        throw std::runtime_error(
            case_id + " must require exact optimality under its corpus profile");
    }
    const Value& verification = required(
        specification, "verification", Type::Object);
    if (exact_evaluation_required &&
        !optional_bool(verification, "exact_evaluation", false)) {
        throw std::runtime_error(
            case_id + " must require independent exact evaluation");
    }
    const Value& case_runs = required(
        verification, "runs", Type::Number);
    if (!std::isfinite(case_runs.number) ||
        std::floor(case_runs.number) != case_runs.number ||
        case_runs.number != verification_runs) {
        throw std::runtime_error(
            case_id + " verification runs do not match its corpus profile");
    }
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
        const Value* expected = optional(
            *product, "expected_priced_action_ids", Type::Array);
        if (expected != nullptr && !expected->array.empty()) {
            std::vector<std::string> pinned;
            for (const Value& entry : expected->array) {
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
    const bool goal_progress_gated_reforges,
    const std::uint32_t max_discovered_states_override,
    const bool exact_strategy_evaluation,
    const double exact_strategy_evaluation_time_limit_seconds,
    const std::function<void(const CaseResult&)>& checkpoint) {
    CaseResult report;
    report.verification_skipped = skip_verification;
    report.max_discovered_states_override =
        max_discovered_states_override;
    initialize_forced_winner_contract(specification, report);
    initialize_bounded_best_policy_contract(specification, report);
    initialize_compiled_operation_contract(specification, report);
    initialize_material_ratio_contract(specification, report);
    initialize_market_price_override_contracts(specification, report);
    initialize_mechanic_family_control(specification, report);
    const auto total_begin = Clock::now();
    if (const Value* watchdog = optional(
            specification, "watchdog_seconds", Type::Number)) {
        report.watchdog_seconds = watchdog->number;
    }
    const auto case_deadline = report.watchdog_seconds > 0.0
        ? total_begin + std::chrono::duration_cast<Clock::duration>(
              std::chrono::duration<double>(report.watchdog_seconds))
        : Clock::time_point::max();
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
        report.actual_status = "running";
        pc_item_state start_item{};
        const auto create_begin = Clock::now();
        create_case_objects(
            data, specification, handles, start_item,
            &report.product_action_ids);
        if (report.has_mechanic_family_control) {
            /* Validation pins every declared control action into goal.actions.
             * Successful solver construction proves that the exact registry
             * resolved that caller-selected scope; unknown action IDs fail
             * construction before this point. */
            report.mechanic_family_registered = true;
        }
        report.forced_winner_price_override_checked =
            report.has_forced_winner_contract;
        report.market_price_override_contracts_checked =
            report.has_market_price_override_contracts;
        enforce_dependency_candidate_contract(report);
        report.registry_layout_ms = milliseconds(create_begin, Clock::now());
        if (Clock::now() >= case_deadline) {
            report.watchdog_expired = true;
            report.actual_status = "watchdog_expired";
            throw std::runtime_error(
                "case watchdog expired during session/economy setup");
        }

        const Value& caps = required(specification, "caps", Type::Object);
        pc_solve_options solve_options{};
        solve_options.struct_size = sizeof(solve_options);
        solve_options.abi_version = PC_ABI_VERSION;
        const std::string solve_profile = optional_string(
            caps, "solve_profile", "default");
        if (solve_profile == "calculator_product_v1") {
            solve_options.solve_profile =
                PC_SOLVE_PROFILE_CALCULATOR_PRODUCT_V1;
        } else if (solve_profile != "default") {
            throw std::runtime_error("unknown solve_profile: " + solve_profile);
        }
        solve_options.max_states = optional_u32(caps, "max_states", 200000);
        solve_options.max_sweeps = optional_u32(caps, "max_sweeps", 100000);
        solve_options.max_discovered_states = optional_u32(
            caps, "max_discovered_states", solve_options.max_states);
        if (max_discovered_states_override != 0) {
            solve_options.max_states =
                max_discovered_states_override;
            solve_options.max_discovered_states =
                max_discovered_states_override;
        }
        solve_options.max_expanded_states = optional_u32(
            caps, "max_expanded_states", solve_options.max_states);
        solve_options.max_state_action_rows = optional_u64(
            caps, "max_state_action_rows", 1215000);
        solve_options.max_transitions = optional_u64(
            caps, "max_transitions", 10000000);
        solve_options.max_reforge_work = optional_u64(
            caps, "max_reforge_work", 50000000);
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
        solve_options.max_policy_refinement_states = optional_u32(
            caps, "max_policy_refinement_states", 0);
        if (caps.find("max_policy_refinement_states") != nullptr) {
            solve_options.solve_profile_override_mask |=
                PC_SOLVE_PROFILE_OVERRIDE_POLICY_REFINEMENT_STATES;
        }
        solve_options.max_absolute_optimality_gap = optional_nonnegative_double(
            caps, "max_absolute_optimality_gap", 0.0);
        solve_options.max_relative_optimality_gap = optional_nonnegative_double(
            caps, "max_relative_optimality_gap", 0.0);
        if (optional_bool(caps, "full_evidence", false)) {
            solve_options.solver_flags |= PC_SOLVER_FLAG_FULL_EVIDENCE;
        }
        if (optional_bool(caps, "strict_states", false)) {
            solve_options.solver_flags |= PC_SOLVER_FLAG_STRICT_STATES;
        }
        if (!optional_bool(caps, "kernel_reuse", true)) {
            solve_options.solver_flags |=
                PC_SOLVER_FLAG_DISABLE_KERNEL_REUSE;
        }
        if (caps.find("goal_progress_gated_reforges") != nullptr) {
            solve_options.solve_profile_override_mask |=
                PC_SOLVE_PROFILE_OVERRIDE_GOAL_PROGRESS_GATED_REFORGES;
            if (optional_bool(
                    caps, "goal_progress_gated_reforges", false)) {
                solve_options.solver_flags |=
                    PC_SOLVER_FLAG_GOAL_PROGRESS_GATED_REFORGES;
            }
        }
        if (caps.find("allow_economic_restart") != nullptr) {
            solve_options.solve_profile_override_mask |=
                PC_SOLVE_PROFILE_OVERRIDE_ECONOMIC_RESTART;
            if (!optional_bool(caps, "allow_economic_restart", true)) {
                solve_options.solver_flags |=
                    PC_SOLVER_FLAG_DISABLE_ECONOMIC_RESTART;
            }
        }
        /* General benchmark cases exclude generated Imprint programs unless
         * the corpus explicitly opts in. Dedicated Imprint correctness
         * controls continue to request the family directly. */
        if (caps.find("consider_imprint_programs") != nullptr) {
            solve_options.solve_profile_override_mask |=
                PC_SOLVE_PROFILE_OVERRIDE_IMPRINT_PROGRAMS;
            if (!optional_bool(caps, "consider_imprint_programs", false)) {
                solve_options.solver_flags |=
                    PC_SOLVER_FLAG_DISABLE_IMPRINT_PROGRAMS;
            }
        } else if (solve_options.solve_profile == PC_SOLVE_PROFILE_DEFAULT) {
            solve_options.solver_flags |=
                PC_SOLVER_FLAG_DISABLE_IMPRINT_PROGRAMS;
        }
        if (goal_progress_gated_reforges) {
            solve_options.solve_profile_override_mask |=
                PC_SOLVE_PROFILE_OVERRIDE_GOAL_PROGRESS_GATED_REFORGES;
            solve_options.solver_flags |=
                PC_SOLVER_FLAG_GOAL_PROGRESS_GATED_REFORGES;
        }
        if (caps.find("high_impact_executable_uppers") != nullptr) {
            solve_options.solve_profile_override_mask |=
                PC_SOLVE_PROFILE_OVERRIDE_HIGH_IMPACT_EXECUTABLE_UPPERS;
            if (optional_bool(
                    caps, "high_impact_executable_uppers", false)) {
                solve_options.solver_flags |=
                    PC_SOLVER_FLAG_HIGH_IMPACT_EXECUTABLE_UPPERS;
            }
        }
        if (optional_bool(
                caps, "projected_reforge_frontier_diagnostic", false)) {
            solve_options.solver_flags |=
                poecraft::solver::
                    kProjectedReforgeFrontierDiagnosticFlag;
        }
        if (optional_bool(
                caps, "factored_terminal_reforge_diagnostic", false)) {
            solve_options.solver_flags |=
                poecraft::solver::
                    kFactoredTerminalReforgeDiagnosticFlag;
        }
        if (optional_bool(
                caps,
                "disable_reforge_resource_accounting_diagnostic",
                false)) {
            solve_options.solver_flags |=
                poecraft::solver::
                    kDisableReforgeResourceAccountingDiagnosticFlag;
        }
        if (optional_bool(
                caps, "raw_strict_reforge_oracle_diagnostic", false)) {
            solve_options.solver_flags |=
                poecraft::solver::
                    kRawStrictReforgeOracleDiagnosticFlag;
        }
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
        const bool bounded_incremental_diagnostic =
            optional_string(specification, "benchmark_mode") ==
            "bounded_incremental_diagnostic";
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
        const Value* trace_interval_value = optional(
            caps, "bound_trace_interval_seconds", Type::Number);
        const double trace_interval_seconds =
            trace_interval_value == nullptr
                ? 1.0
                : std::max(0.05, trace_interval_value->number);
        auto next_bound_trace = solve_begin;
        std::uint32_t last_trace_round =
            std::numeric_limits<std::uint32_t>::max();
        std::int32_t last_trace_incumbent = -1;
        const std::vector<double> absolute_thresholds{
            1000.0, 100.0, 50.0, 25.0, 10.0, 5.0, 1.0, 0.1};
        const std::vector<double> relative_thresholds{
            1.0, 0.5, 0.25, 0.1, 0.05, 0.01};
        const auto record_bound_trace = [&](const auto now) {
            CaseResult::BoundTraceEntry entry;
            entry.elapsed_ms = milliseconds(solve_begin, now);
            entry.phase = progress.phase;
            entry.round = progress.focused_round;
            entry.lower_bound = progress.lower_bound;
            entry.upper_bound = progress.upper_bound;
            entry.absolute_gap = progress.absolute_optimality_gap;
            entry.relative_gap = progress.relative_optimality_gap;
            entry.incumbent_kind = progress.incumbent_kind;
            entry.discovered_states = progress.discovered_states;
            entry.expanded_states = progress.expanded_states;
            entry.frontier_states = progress.frontier_states;
            entry.rows = progress.state_action_rows;
            entry.transitions = progress.transition_entries;
            entry.reforge_work = progress.reforge_work;
            entry.live_owned_bytes = progress.live_owned_bytes;
            entry.peak_owned_bytes = progress.peak_owned_bytes;
            if (!report.bound_trace.empty()) {
                const CaseResult::BoundTraceEntry& previous =
                    report.bound_trace.back();
                entry.raised_lower =
                    std::isfinite(entry.lower_bound) &&
                    entry.lower_bound > previous.lower_bound;
                entry.decreased_lower =
                    std::isfinite(entry.lower_bound) &&
                    std::isfinite(previous.lower_bound) &&
                    entry.lower_bound < previous.lower_bound;
                entry.lowered_upper =
                    std::isfinite(entry.upper_bound) &&
                    (!std::isfinite(previous.upper_bound) ||
                     entry.upper_bound < previous.upper_bound);
                entry.increased_upper =
                    entry.incumbent_kind != PC_SOLVE_INCUMBENT_NONE &&
                    previous.incumbent_kind != PC_SOLVE_INCUMBENT_NONE &&
                    std::isfinite(entry.upper_bound) &&
                    std::isfinite(previous.upper_bound) &&
                    entry.upper_bound > previous.upper_bound;
            }
            report.bound_trace.push_back(entry);
            if (!report.has_time_to_first_incumbent &&
                entry.incumbent_kind != PC_SOLVE_INCUMBENT_NONE &&
                std::isfinite(entry.upper_bound)) {
                report.has_time_to_first_incumbent = true;
                report.time_to_first_incumbent_ms = entry.elapsed_ms;
            }
            for (const double threshold : absolute_thresholds) {
                const bool recorded = std::any_of(
                    report.absolute_gap_milestones.begin(),
                    report.absolute_gap_milestones.end(),
                    [&](const auto& item) { return item.first == threshold; });
                if (!recorded && std::isfinite(entry.absolute_gap) &&
                    entry.absolute_gap <= threshold) {
                    report.absolute_gap_milestones.push_back(
                        {threshold, entry.elapsed_ms});
                }
            }
            for (const double threshold : relative_thresholds) {
                const bool recorded = std::any_of(
                    report.relative_gap_milestones.begin(),
                    report.relative_gap_milestones.end(),
                    [&](const auto& item) { return item.first == threshold; });
                if (!recorded && std::isfinite(entry.relative_gap) &&
                    entry.relative_gap <= threshold) {
                    report.relative_gap_milestones.push_back(
                        {threshold, entry.elapsed_ms});
                }
            }
            report.solve_ms = entry.elapsed_ms;
            report.total_ms = milliseconds(total_begin, now);
            report.working_set_after = process_working_set();
            if (checkpoint) checkpoint(report);
        };
        bool watchdog_abandoned = false;
        const Value* requested_bounded_finish_value = optional(
            specification, "requested_bounded_finish_seconds",
            Type::Number);
        const double requested_bounded_finish_seconds =
            requested_bounded_finish_value == nullptr
                ? 0.0
                : requested_bounded_finish_value->number;
        bool requested_bounded_finish = false;
        do {
            const auto step_begin = Clock::now();
            result = pc_solver_solve_step(
                handles.solver, work_items, &progress, &error);
            const double step_ms = milliseconds(step_begin, Clock::now());
            report.solve_step_wall_ms.push_back(step_ms);
            if (step_ms > report.max_solve_step_ms) {
                report.max_solve_step_ms = step_ms;
                report.max_solve_step_phase = progress.phase;
                report.max_solve_step_expanded_states =
                    progress.expanded_states;
                report.max_solve_step_sweeps = progress.sweeps;
                report.max_solve_step_finalization_work_item =
                    progress.finalization_work_items;
            }
            ++report.solve_steps;
            if (result != PC_RESULT_OK) {
                throw std::runtime_error(api_error("pc_solver_solve_step", result,
                                                   error));
            }
            const auto after_step = Clock::now();
            if (report.bound_trace.empty() || progress.done ||
                progress.focused_round != last_trace_round ||
                progress.incumbent_kind != last_trace_incumbent ||
                after_step >= next_bound_trace) {
                record_bound_trace(after_step);
                last_trace_round = progress.focused_round;
                last_trace_incumbent = progress.incumbent_kind;
                next_bound_trace = after_step +
                    std::chrono::duration_cast<Clock::duration>(
                        std::chrono::duration<double>(
                            trace_interval_seconds));
            }
            if (after_step >= case_deadline) {
                if (!progress.done) {
                    pc_solver_solve_abandon(handles.solver);
                }
                report.watchdog_expired = true;
                watchdog_abandoned = true;
                report.actual_status = "watchdog_expired";
                report.diagnostic_stop_reason = "watchdog_seconds";
                break;
            }
            if (!progress.done && !requested_bounded_finish &&
                requested_bounded_finish_seconds > 0.0 &&
                milliseconds(solve_begin, after_step) >=
                    requested_bounded_finish_seconds * 1000.0) {
                result = pc_solver_solve_request_bounded_finish(
                    handles.solver, &error);
                if (result != PC_RESULT_OK) {
                    throw std::runtime_error(api_error(
                        "pc_solver_solve_request_bounded_finish",
                        result, error));
                }
                requested_bounded_finish = true;
            }
            if (emit_progress && Clock::now() >= next_progress) {
                std::cout << required_string(specification, "id")
                          << ": progress phase=" << progress.phase
                          << " expanded=" << progress.expanded_states
                          << " sweeps=" << progress.sweeps
                          << " residual=" << progress.residual
                          << " start_bound=" << progress.start_value_bound
                          << " final_work="
                          << progress.finalization_work_items
                          << " refinement_states="
                          << progress.refinement_states
                          << " refinement_kernels="
                          << progress.refinement_kernels
                          << " refinement_transitions="
                          << progress.refinement_transitions
                          << " refinement_rounds="
                          << progress.refinement_rounds
                          << " refinement_classes="
                          << progress.refinement_classes
                          << " certification_pairs="
                          << progress.certification_discovered_pairs
                          << " certification_pending="
                          << progress.certification_pending_pairs
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
            if ((bounded_first_expansion ||
                 bounded_incremental_diagnostic) &&
                !progress.done &&
                report.diagnostic_work_limit != 0 &&
                report.solve_steps >= report.diagnostic_work_limit) {
                pc_solver_solve_abandon(handles.solver);
                report.actual_status = "diagnostic_work_cap";
                report.diagnostic_stop_reason =
                    "max_diagnostic_work_items";
                break;
            }
            if ((bounded_first_expansion ||
                 bounded_incremental_diagnostic) &&
                !progress.done &&
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
            (bounded_first_expansion ||
             bounded_incremental_diagnostic ||
             stop_after_bellman_entry) &&
            !progress.done;
        if (!cancel_after_first && !diagnostic_abandoned &&
            !watchdog_abandoned) {
            report.solve_summary = {};
            result = pc_solver_solve_finish(
                handles.solver, &report.solve_summary, &error);
            if (result != PC_RESULT_OK) {
                throw std::runtime_error(api_error("pc_solver_solve_finish", result,
                                                   error));
            }
            report.has_solve_summary = true;
            report.solve_result_class =
                solve_result_class(report.solve_summary);
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

        const Value* expected_contract = specification.find("expected");
        const std::string expected_compile =
            expected_contract == nullptr
                ? "compiled_if_policy_available"
                : required_string(*expected_contract, "compile_status");
        if (report.has_solve_summary &&
            !report.solve_summary.policy_available) {
            report.compile_status = "not_applicable_no_policy";
            report.exact_evaluation_status = "not_applicable_no_policy";
            report.simulation_status = "not_applicable_no_policy";
        }
        if (!bounded_first_expansion && report.has_solve_summary &&
            report.solve_summary.policy_available &&
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
                    report.compile_ms = milliseconds(
                        compile_begin, Clock::now());
                    enforce_required_compiled_operation(
                        strategy_json, report);
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
                        report.compile_status = "compile_failed";
                        report.errors.push_back(api_error(
                            "pc_strategy_compile_json", result, error));
                        report.actual_status = "compile_refused";
                    } else {
                        report.compile_status = "compiled";
                        const Value& verification = required(
                            specification, "verification", Type::Object);
                        const bool exact_evaluation_required =
                            exact_strategy_evaluation ||
                            optional_bool(
                                verification, "exact_evaluation", false);
                        if (exact_evaluation_required) {
                            report.exact_evaluation_status = "running";
                            const auto evaluation_started = Clock::now();
                            pc_strategy_eval_options evaluation_options{};
                            evaluation_options.struct_size =
                                sizeof(evaluation_options);
                            evaluation_options.abi_version = PC_ABI_VERSION;
                            evaluation_options.epsilon = 1e-12;
                            evaluation_options.max_sweeps = 100000;
                            evaluation_options.max_states = optional_u32(
                                verification, "exact_max_states",
                                optional_u32(
                                    caps, "max_discovered_states", 300000));
                            evaluation_options.max_pairs = optional_u32(
                                verification, "exact_max_pairs",
                                static_cast<std::uint32_t>(
                                    std::min<std::uint64_t>(
                                        optional_u64(
                                            caps, "max_state_action_rows",
                                            3000000),
                                        std::numeric_limits<
                                            std::uint32_t>::max())));
                            evaluation_options.max_transitions = optional_u32(
                                verification, "exact_max_transitions",
                                static_cast<std::uint32_t>(
                                    std::min<std::uint64_t>(
                                        optional_u64(
                                            caps, "max_transitions",
                                            30000000),
                                        std::numeric_limits<
                                            std::uint32_t>::max())));
                            evaluation_options.max_owned_bytes = optional_u64(
                                verification, "exact_max_owned_bytes",
                                512ull * 1024ull * 1024ull);
                            evaluation_options.economy = handles.economy;
                            result = pc_strategy_eval_begin(
                                handles.strategy, &evaluation_options,
                                &handles.strategy_evaluation, &error);
                            if (result == PC_RESULT_OK) {
                                pc_strategy_eval_progress evaluation_progress{};
                                const auto requested_evaluation_deadline =
                                    exact_strategy_evaluation_time_limit_seconds > 0.0
                                        ? evaluation_started +
                                              std::chrono::duration_cast<
                                                  Clock::duration>(
                                                  std::chrono::duration<double>(
                                                      exact_strategy_evaluation_time_limit_seconds))
                                        : Clock::time_point::max();
                                const auto evaluation_deadline = std::min(
                                    requested_evaluation_deadline,
                                    case_deadline);
                                auto next_evaluation_progress =
                                    evaluation_started +
                                    std::chrono::seconds(10);
                                while (!evaluation_progress.done) {
                                    /* Match the native evaluator's bounded
                                     * batch cadence. A one-item C-ABI step
                                     * recomputes aggregate progress after
                                     * every pair and makes large exact
                                     * qualification replays needlessly
                                     * quadratic in reporting work. */
                                    result = pc_strategy_eval_step(
                                        handles.strategy_evaluation, 4096,
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
                                        if (now >= case_deadline) {
                                            report.watchdog_expired = true;
                                            report.actual_status =
                                                "watchdog_expired";
                                            report.exact_evaluation_status =
                                                "watchdog_expired";
                                        } else {
                                            report.exact_evaluation_status =
                                                "time_cap";
                                        }
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
                                            inspect_exact_strategy_evaluation(
                                                specification, report);
                                        }
                                    }
                                }
                            }
                            if (result != PC_RESULT_OK) {
                                report.exact_evaluation_status =
                                    result == PC_RESULT_UNSUPPORTED_FEATURE
                                        ? "unsupported_vocabulary"
                                        : "evaluation_failed";
                                report.errors.push_back(api_error(
                                    "exact strategy evaluation", result,
                                    error));
                            } else if (
                                !report.has_exact_strategy_evaluation &&
                                !report
                                     .exact_strategy_evaluation_time_limited) {
                                report.exact_evaluation_status =
                                    "evaluation_incomplete";
                                report.errors.push_back(
                                    "exact strategy evaluation did not "
                                    "produce a completed result");
                            }
                            report.exact_strategy_evaluation_ms = milliseconds(
                                evaluation_started, Clock::now());
                            pc_strategy_eval_destroy(
                                handles.strategy_evaluation);
                            handles.strategy_evaluation = nullptr;
                            result = PC_RESULT_OK;
                        }
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
                        report.simulation_status =
                            runs == 0
                                ? "not_selected"
                                : (skip_verification ? "skipped"
                                                     : "running");
                        if (runs > 0 && !skip_verification &&
                            !report.watchdog_expired) {
                            const auto verification_begin = Clock::now();
                            result = pc_simulator_create(
                                handles.session, handles.strategy,
                                handles.economy, &handles.simulator, &error);
                            if (result != PC_RESULT_OK) {
                                report.simulation_status =
                                    "simulation_failed";
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
                                simulation_options.max_graph_steps_per_run =
                                    optional_u32(
                                        verification,
                                        "max_graph_steps_per_run", 0);
                                pc_simulation_progress simulation_progress{};
                                const auto simulation_started = Clock::now();
                                const auto requested_simulation_deadline =
                                    verification_time_limit_seconds > 0.0
                                        ? simulation_started +
                                              std::chrono::duration_cast<
                                                  Clock::duration>(
                                                  std::chrono::duration<double>(
                                                      verification_time_limit_seconds))
                                        : Clock::time_point::max();
                                const auto simulation_deadline = std::min(
                                    requested_simulation_deadline,
                                    case_deadline);
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
                                        if (now >= case_deadline) {
                                            report.watchdog_expired = true;
                                            report.actual_status =
                                                "watchdog_expired";
                                            report.simulation_status =
                                                "watchdog_expired";
                                        } else {
                                            report.simulation_status =
                                                "time_cap";
                                        }
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
                                    report.simulation_status =
                                        report.verification_finished
                                            ? "completed"
                                            : (report
                                                       .verification_time_limited
                                                   ? "time_cap"
                                                   : "incomplete");
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
                                    if (result != PC_RESULT_OK) {
                                        report.errors.push_back(api_error(
                                            "simulation action distribution",
                                            result, error));
                                    }
                                    std::uint32_t descriptor_count = 0;
                                    result =
                                        pc_simulator_action_descriptor_distribution_query(
                                            handles.simulator, nullptr, 0,
                                            &descriptor_count, &error);
                                    if (result == PC_RESULT_OK ||
                                        result == PC_RESULT_BUFFER_TOO_SMALL) {
                                        std::vector<
                                            pc_action_descriptor_sample_entry>
                                            entries(descriptor_count);
                                        result =
                                            pc_simulator_action_descriptor_distribution_query(
                                                handles.simulator,
                                                entries.data(),
                                                descriptor_count,
                                                &descriptor_count, &error);
                                        if (result == PC_RESULT_OK) {
                                            report
                                                .action_descriptor_distribution
                                                .reserve(descriptor_count);
                                            for (std::uint32_t i = 0;
                                                 i < descriptor_count; ++i) {
                                                report
                                                    .action_descriptor_distribution
                                                    .push_back({
                                                        entries[i].action_id ==
                                                                nullptr
                                                            ? std::string()
                                                            : entries[i]
                                                                  .action_id,
                                                        entries[i].count});
                                            }
                                        }
                                    }
                                    if (result != PC_RESULT_OK) {
                                        report.errors.push_back(api_error(
                                            "simulation action descriptor "
                                            "distribution",
                                            result, error));
                                    }

                                    std::uint32_t material_count = 0;
                                    result =
                                        pc_simulator_material_distribution_query(
                                            handles.simulator, nullptr, 0,
                                            &material_count, &error);
                                    if (result == PC_RESULT_OK ||
                                        result == PC_RESULT_BUFFER_TOO_SMALL) {
                                        std::vector<pc_material_sample_entry>
                                            entries(material_count);
                                        result =
                                            pc_simulator_material_distribution_query(
                                                handles.simulator,
                                                entries.data(), material_count,
                                                &material_count, &error);
                                        if (result == PC_RESULT_OK) {
                                            report.material_distribution.reserve(
                                                material_count);
                                            for (std::uint32_t i = 0;
                                                 i < material_count; ++i) {
                                                report.material_distribution
                                                    .push_back({
                                                        entries[i].price_key ==
                                                                nullptr
                                                            ? std::string()
                                                            : entries[i]
                                                                  .price_key,
                                                        entries[i].count});
                                            }
                                        }
                                    }
                                    if (result != PC_RESULT_OK) {
                                        report.errors.push_back(api_error(
                                            "simulation material "
                                            "distribution",
                                            result, error));
                                    }
                                } else {
                                    report.simulation_status =
                                        "simulation_failed";
                                    report.errors.push_back(api_error(
                                        "simulation verification", result, error));
                                }
                            }
                            report.verification_ms = milliseconds(
                                verification_begin, Clock::now());
                        }
                    }
                } else {
                    report.compile_status = "compile_failed";
                    report.compile_ms =
                        milliseconds(compile_begin, Clock::now());
                    report.errors.push_back(api_error(
                        "pc_solver_compile_strategy", result, error));
                    report.actual_status = "compile_refused";
                }
            } else {
                report.compile_status = "compile_failed";
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
        if (handles.solver != nullptr && report.telemetry_json.empty()) {
            /*
             * A failed step can still own a valid in-progress snapshot. Freeze
             * it before destroying the handle so diagnostic/resource failures
             * remain analyzable instead of discarding all completed work.
             */
            pc_solver_solve_abandon(handles.solver);
            report.telemetry_json =
                query_telemetry(handles.solver, report.errors);
        }
        if (report.actual_status == "not_run" ||
            report.actual_status == "running") {
            report.actual_status = "harness_error";
        }
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

    if (Clock::now() >= case_deadline) {
        report.watchdog_expired = true;
        if (report.actual_status != "covered_by_native_unit_gate" &&
            report.actual_status != "not_run_approval_pending") {
            report.actual_status = "watchdog_expired";
        }
    }
    finalize_material_ratio_contract(report, skip_verification);
    enforce_bounded_best_policy_contract(specification, report);
    finalize_mechanic_family_control(report);
    evaluate_cap_checks(specification, report);
    report.expectation_met = evaluate_expectation(
        specification, report, skip_verification);
    report.working_set_after = process_working_set();
    report.total_ms = milliseconds(total_begin, Clock::now());
    if (checkpoint) checkpoint(report);
    return report;
}

void append_nullable_number(std::ostringstream& out, bool present, double value) {
    if (!present || !std::isfinite(value)) out << "null";
    else out << std::setprecision(17) << value;
}

const char* policy_status_name(const int32_t status) {
    switch (status) {
    case PC_SOLVE_POLICY_BOUNDED_FEASIBLE: return "bounded_feasible";
    case PC_SOLVE_POLICY_BOUNDED_NEAR_OPTIMAL:
        return "bounded_near_optimal";
    case PC_SOLVE_POLICY_EXACT: return "exact";
    default: return "none";
    }
}

const char* termination_name(const int32_t termination) {
    switch (termination) {
    case PC_SOLVE_TERMINATION_REFUSED_RESOURCE_CAP:
        return "refused_resource_cap";
    case PC_SOLVE_TERMINATION_TARGET_GAP: return "target_gap";
    case PC_SOLVE_TERMINATION_EXACT_CLOSED: return "exact_closed";
    case PC_SOLVE_TERMINATION_NO_EXECUTABLE_POLICY:
        return "no_executable_policy";
    case PC_SOLVE_TERMINATION_NUMERICAL_STABILITY:
        return "numerical_stability";
    case PC_SOLVE_TERMINATION_REQUESTED_BOUNDED_FINISH:
        return "requested_bounded_finish";
    default: return "none";
    }
}

const char* stop_cause_name(const int32_t cause) {
    switch (cause) {
    case PC_SOLVE_STOP_EXACT_CLOSED: return "exact_closed";
    case PC_SOLVE_STOP_TARGET_GAP: return "target_gap";
    case PC_SOLVE_STOP_STATE_CAP: return "state_cap";
    case PC_SOLVE_STOP_TRANSITION_CAP: return "transition_cap";
    case PC_SOLVE_STOP_MEMORY_CAP: return "memory_cap";
    case PC_SOLVE_STOP_SWEEP_CAP: return "sweep_cap";
    case PC_SOLVE_STOP_REFORGE_WORK_CAP: return "reforge_work_cap";
    case PC_SOLVE_STOP_STATE_ACTION_ROW_CAP:
        return "state_action_row_cap";
    case PC_SOLVE_STOP_COMPILED_OUTPUT_CAP:
        return "compiled_output_cap";
    case PC_SOLVE_STOP_OTHER_RESOURCE_CAP:
        return "other_resource_cap";
    case PC_SOLVE_STOP_NO_EXECUTABLE_POLICY:
        return "no_executable_policy";
    case PC_SOLVE_STOP_NUMERICAL_STABILITY:
        return "numerical_stability";
    case PC_SOLVE_STOP_REQUESTED_BOUNDED_FINISH:
        return "requested_bounded_finish";
    default: return "none";
    }
}

const char* target_name(const int32_t target) {
    switch (target) {
    case PC_SOLVE_GAP_TARGET_ABSOLUTE: return "absolute";
    case PC_SOLVE_GAP_TARGET_RELATIVE: return "relative";
    case PC_SOLVE_GAP_TARGET_BOTH: return "both";
    default: return "none";
    }
}

const char* incumbent_kind_name(const int32_t kind) {
    switch (kind) {
    case PC_SOLVE_INCUMBENT_CONSTRUCTIVE_FALLBACK:
        return "constructive_fallback";
    case PC_SOLVE_INCUMBENT_PROGRESSIVE_FRACTURE:
        return "progressive_fracture";
    case PC_SOLVE_INCUMBENT_DESTRUCTIVE_RENEWAL:
        return "destructive_renewal";
    case PC_SOLVE_INCUMBENT_DIRECT_EXECUTABLE_ROW:
        return "direct_executable_row";
    case PC_SOLVE_INCUMBENT_PARTIAL_UPPER_FALLBACK:
        return "partial_upper_plus_fallback";
    case PC_SOLVE_INCUMBENT_PARTIAL_UPPER_PROGRESSIVE_FRACTURE:
        return "partial_upper_plus_progressive_fracture";
    case PC_SOLVE_INCUMBENT_PARTIAL_UPPER_DESTRUCTIVE_RENEWAL:
        return "partial_upper_plus_destructive_renewal";
    case PC_SOLVE_INCUMBENT_OTHER: return "other";
    default: return "none";
    }
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
    out << "  \"expected\":";
    if (const Value* expected = specification.find("expected")) {
        out << json_of(*expected);
    } else {
        out << "{\"solve_status\":\"policy_available\","
               "\"compile_status\":\"compiled_if_policy_available\","
               "\"verification_status\":\"deferred_to_b6\"}";
    }
    out << ",\n";
    out << "  \"actual_status\":" << escape_json(result.actual_status) << ",\n";
    out << "  \"workflow_status\":{\"solve_result_class\":"
        << escape_json(result.solve_result_class)
        << ",\"compile\":" << escape_json(result.compile_status)
        << ",\"exact_evaluation\":"
        << escape_json(result.exact_evaluation_status)
        << ",\"simulation\":"
        << escape_json(result.simulation_status) << "},\n";
    out << "  \"expectation_met\":" << (result.expectation_met ? "true" : "false") << ",\n";
    out << "  \"verification_skipped\":"
        << (result.verification_skipped ? "true" : "false") << ",\n";
    out << "  \"input\":{";
    bool first_input = true;
    for (const char* key : {"comparison_profile", "watchdog_seconds", "requested_bounded_finish_seconds", "session", "start", "goal", "corpus", "feasibility", "generation", "product_action_envelope", "allowed_mechanic_families", "mechanic_family_control", "compiled_operation_contract", "compiled_operation_contracts", "material_ratio_contract", "market_price_override_contracts", "forced_winner_contract", "bounded_best_policy_contract", "economy", "caps", "verification"}) {
        const Value* value = specification.find(key);
        if (value == nullptr) continue;
        if (!first_input) out << ',';
        first_input = false;
        out << escape_json(key) << ':' << json_of(*value);
    }
    if (result.max_discovered_states_override != 0) {
        if (!first_input) out << ',';
        out << "\"run_overrides\":{\"max_discovered_states\":"
            << result.max_discovered_states_override << '}';
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
            << ",\"status\":"
            << escape_json(result.exact_evaluation_status)
            << ",\"wall_ms\":";
        append_nullable_number(
            out, result.exact_strategy_evaluation_ms > 0.0,
            result.exact_strategy_evaluation_ms);
        out << "},\n";
    } else {
        out << "{\"completed\":true,\"time_limited\":false,\"status\":"
            << escape_json(result.exact_evaluation_status)
            << ",\"wall_ms\":";
        append_nullable_number(out, true, result.exact_strategy_evaluation_ms);
        out << ",\"converged\":"
            << (result.exact_strategy_evaluation_converged
                    ? "true" : "false")
            << ",\"cost_complete\":"
            << (result.exact_strategy_evaluation_cost_complete
                    ? "true" : "false")
            << ",\"zero_off_policy_mass\":"
            << (result.exact_strategy_evaluation_zero_off_policy
                    ? "true" : "false")
            << ",\"cost_reconciled\":"
            << (result.exact_strategy_evaluation_cost_reconciled
                    ? "true" : "false")
            << ",\"success_probability\":";
        append_nullable_number(
            out, true,
            result.exact_strategy_evaluation_terminal_success);
        out << ",\"off_policy_mass\":";
        append_nullable_number(
            out, true,
            result.exact_strategy_evaluation_off_policy_mass);
        out << ",\"total_expected_cost\":";
        append_nullable_number(
            out, result.exact_strategy_evaluation_cost_complete,
            result.exact_strategy_evaluation_cost);
        out << ",\"solver_cost_delta_absolute\":";
        append_nullable_number(
            out, true,
            result.exact_strategy_evaluation_cost_delta_absolute);
        out << ",\"solver_cost_delta_relative\":";
        append_nullable_number(
            out, true,
            result.exact_strategy_evaluation_cost_delta_relative);
        out << ",\"result\":" << result.exact_strategy_evaluation_json
            << "},\n";
    }
    out << "  \"product_action_ids\":[";
    for (std::size_t i = 0; i < result.product_action_ids.size(); ++i) {
        if (i != 0) out << ',';
        out << escape_json(result.product_action_ids[i]);
    }
    out << "],\n";
    out << "  \"mechanic_family_control\":";
    if (!result.has_mechanic_family_control) {
        out << "null,\n";
    } else {
        out << "{\"id\":"
            << escape_json(result.mechanic_family_control_id)
            << ",\"registered_action_ids\":[";
        for (std::size_t i = 0;
             i < result.mechanic_family_registered_action_ids.size(); ++i) {
            if (i != 0) out << ',';
            out << escape_json(
                result.mechanic_family_registered_action_ids[i]);
        }
        out << "],\"synthetic_price_keys\":[";
        for (std::size_t i = 0;
             i < result.mechanic_family_synthetic_price_keys.size(); ++i) {
            if (i != 0) out << ',';
            out << escape_json(
                result.mechanic_family_synthetic_price_keys[i]);
        }
        out << "],\"synthetic_price_purpose\":"
            << escape_json(
                result.mechanic_family_synthetic_price_purpose)
            << ",\"synthetic_price_disclosed\":"
            << (result.mechanic_family_synthetic_price_disclosed
                    ? "true" : "false")
            << ",\"stages\":{\"registered\":"
            << (result.mechanic_family_registered ? "true" : "false")
            << ",\"legal_carrier_admitted\":"
            << (result.mechanic_family_legal_carrier_admitted
                    ? "true" : "false")
            << ",\"scheduled\":"
            << (result.mechanic_family_scheduled ? "true" : "false")
            << ",\"exact_row_materialized\":"
            << (result.mechanic_family_exact_row_materialized
                    ? "true" : "false")
            << ",\"selected_under_disclosed_synthetic_price\":"
            << (result.mechanic_family_selected_under_synthetic_price
                    ? "true" : "false")
            << ",\"compiled\":"
            << (result.mechanic_family_compiled ? "true" : "false")
            << ",\"independently_exact_evaluated\":"
            << (result.mechanic_family_independently_exact_evaluated
                    ? "true" : "false")
            << "},\"passed\":"
            << (result.mechanic_family_control_passed ? "true" : "false")
            << ",\"failure_stage\":";
        if (result.mechanic_family_failure_stage.empty()) out << "null";
        else out << escape_json(result.mechanic_family_failure_stage);
        out << "},\n";
    }
    out << "  \"compiled_operation_contract\":";
    if (!result.has_compiled_operation_contract) {
        out << "null,\n";
    } else {
        out << "{\"type\":"
            << escape_json(result.compiled_operation_contract_type)
            << ",\"required_string_parameters\":{";
        for (std::size_t i = 0;
             i < result.compiled_operation_contract_string_parameters.size();
             ++i) {
            if (i != 0) out << ',';
            out << escape_json(
                       result.compiled_operation_contract_string_parameters[i]
                           .first)
                << ':'
                << escape_json(
                       result.compiled_operation_contract_string_parameters[i]
                           .second);
        }
        out << "},\"checked\":"
            << (result.compiled_operation_contract_checked
                    ? "true" : "false")
            << ",\"matching_node_count\":"
            << result.compiled_operation_contract_matching_nodes
            << ",\"present\":"
            << (result.compiled_operation_contract_matching_nodes > 0
                    ? "true" : "false")
            << "},\n";
    }
    out << "  \"compiled_operation_contracts\":[";
    for (std::size_t i = 0;
         i < result.compiled_operation_contracts.size(); ++i) {
        if (i != 0) out << ',';
        const auto& contract = result.compiled_operation_contracts[i];
        out << "{\"type\":" << escape_json(contract.type)
            << ",\"required_string_parameters\":{";
        for (std::size_t j = 0; j < contract.string_parameters.size(); ++j) {
            if (j != 0) out << ',';
            out << escape_json(contract.string_parameters[j].first) << ':'
                << escape_json(contract.string_parameters[j].second);
        }
        out << "},\"checked\":" << (contract.checked ? "true" : "false")
            << ",\"matching_node_count\":" << contract.matching_nodes
            << ",\"present\":"
            << (contract.matching_nodes > 0 ? "true" : "false") << '}';
    }
    out << "],\n";
    out << "  \"market_price_override_contracts\":";
    if (!result.has_market_price_override_contracts) {
        out << "null,\n";
    } else {
        out << "{\"checked\":"
            << (result.market_price_override_contracts_checked
                    ? "true" : "false")
            << ",\"declared_override_count\":"
            << result.market_price_override_contract_count << "},\n";
    }
    out << "  \"material_ratio_contract\":";
    if (!result.has_material_ratio_contract) {
        out << "null,\n";
    } else {
        out << "{\"numerator_price_key\":"
            << escape_json(result.material_ratio_numerator_key)
            << ",\"denominator_price_key\":"
            << escape_json(result.material_ratio_denominator_key)
            << ",\"expected_ratio\":";
        append_nullable_number(out, true, result.material_ratio_expected);
        out << ",\"checked\":"
            << (result.material_ratio_contract_checked ? "true" : "false")
            << ",\"passed\":"
            << (result.material_ratio_contract_passed ? "true" : "false")
            << ",\"exact_evaluation\":{\"checked\":"
            << (result.material_ratio_exact_checked ? "true" : "false")
            << ",\"numerator_quantity\":";
        append_nullable_number(
            out, result.material_ratio_exact_checked,
            result.material_ratio_exact_numerator);
        out << ",\"denominator_quantity\":";
        append_nullable_number(
            out, result.material_ratio_exact_checked,
            result.material_ratio_exact_denominator);
        out << ",\"complete_pricing\":"
            << (result.material_ratio_exact_pricing_complete
                    ? "true" : "false")
            << ",\"passed\":"
            << (result.material_ratio_exact_passed ? "true" : "false")
            << "},\"sampled_simulation\":{\"checked\":"
            << (result.material_ratio_sampled_checked ? "true" : "false")
            << ",\"numerator_count\":"
            << result.material_ratio_sampled_numerator
            << ",\"denominator_count\":"
            << result.material_ratio_sampled_denominator
            << ",\"complete_pricing\":"
            << (result.material_ratio_sampled_pricing_complete
                    ? "true" : "false")
            << ",\"passed\":"
            << (result.material_ratio_sampled_passed ? "true" : "false")
            << "}},\n";
    }
    out << "  \"forced_winner_contract\":";
    if (!result.has_forced_winner_contract) {
        out << "null,\n";
    } else {
        out << "{\"required_compiled_operation\":{\"type\":"
            << escape_json(result.required_compiled_operation_type)
            << ",\"checked\":"
            << (result.required_compiled_operation_checked
                    ? "true" : "false")
            << ",\"matching_node_count\":"
            << result.required_compiled_operation_node_count
            << ",\"present\":"
            << (result.required_compiled_operation_node_count > 0
                    ? "true" : "false")
            << "},\"synthetic_action_price\":{\"snapshot_quote\":";
        append_nullable_number(
            out, true, result.forced_winner_snapshot_action_price);
        out << ",\"forced_value\":";
        append_nullable_number(out, true, result.forced_winner_action_price);
        out << ",\"checked\":"
            << (result.forced_winner_price_override_checked
                    ? "true" : "false")
            << "},\"dependency_primitive_candidate\":{\"action_id\":"
            << escape_json(result.dependency_action_id)
            << ",\"must_be_absent\":"
            << (result.dependency_must_not_be_primitive_candidate
                    ? "true" : "false")
            << ",\"checked\":"
            << (result.dependency_primitive_candidate_checked
                    ? "true" : "false")
            << ",\"absent\":"
            << (result.dependency_primitive_candidate_absent
                    ? "true" : "false")
            << "}},\n";
    }
    out << "  \"bounded_best_policy_contract\":";
    if (!result.has_bounded_best_policy_contract) {
        out << "null,\n";
    } else {
        out << "{\"checked\":"
            << (result.bounded_best_policy_contract_checked
                    ? "true" : "false")
            << ",\"passed\":"
            << (result.bounded_best_policy_contract_passed
                    ? "true" : "false")
            << ",\"path\":"
            << escape_json(
                   result.bounded_best_policy_exact_path
                       ? "exact"
                       : "bounded")
            << ",\"checks\":{";
        if (result.bounded_best_policy_exact_path) {
            out << "\"named_stop\":null,\"strict_gap\":null,"
                   "\"open_obligations\":null,"
                   "\"evaluated_upper\":null,"
                   "\"cheapest_independently_evaluated_incumbent\":null";
        } else {
            out << "\"named_stop\":"
                << (result.bounded_best_policy_named_stop
                        ? "true" : "false")
                << ",\"strict_gap\":"
                << (result.bounded_best_policy_strict_gap
                        ? "true" : "false")
                << ",\"open_obligations\":"
                << (result.bounded_best_policy_open_obligations
                        ? "true" : "false")
                << ",\"evaluated_upper\":"
                << (result.bounded_best_policy_evaluated_upper
                        ? "true" : "false")
                << ",\"cheapest_independently_evaluated_incumbent\":"
                << (result
                            .bounded_best_policy_cheapest_evaluated_incumbent
                        ? "true" : "false");
        }
        out << "},\"open_work\":{\"incremental_actions\":"
            << result.bounded_best_policy_incremental_obligations
            << ",\"refinement_obligations\":"
            << result.bounded_best_policy_refinement_obligations
            << "},\"failure_reason\":";
        if (result.bounded_best_policy_failure_reason.empty()) {
            out << "null";
        } else {
            out << escape_json(result.bounded_best_policy_failure_reason);
        }
        out << "},\n";
    }
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
    out << ",\"max_solve_step_finalization_work_item\":";
    if (!measured || result.solve_steps == 0) out << "null";
    else out << result.max_solve_step_finalization_work_item;
    out << ",\"solve_step_wall_ms\":";
    if (!measured || result.solve_step_wall_ms.empty()) {
        out << "null";
    } else {
        std::vector<double> ordered = result.solve_step_wall_ms;
        std::sort(ordered.begin(), ordered.end());
        const auto nearest_rank = [&](const double percentile) {
            const std::size_t rank = std::max<std::size_t>(
                1, static_cast<std::size_t>(
                       std::ceil(percentile * ordered.size())));
            return ordered[std::min(rank, ordered.size()) - 1];
        };
        double total_step_ms = 0.0;
        for (const double value : result.solve_step_wall_ms) {
            total_step_ms += value;
        }
        out << "{\"percentile_method\":\"nearest_rank_ceiling\"";
        out << ",\"count\":" << result.solve_step_wall_ms.size();
        out << ",\"total\":";
        append_nullable_number(out, true, total_step_ms);
        out << ",\"median\":";
        append_nullable_number(out, true, nearest_rank(0.50));
        out << ",\"p95\":";
        append_nullable_number(out, true, nearest_rank(0.95));
        out << ",\"maximum\":";
        append_nullable_number(out, true, result.max_solve_step_ms);
        out << "}";
    }
    out << ",\"diagnostic_work_limit\":";
    if (result.diagnostic_work_limit == 0) out << "null";
    else out << result.diagnostic_work_limit;
    out << ",\"diagnostic_time_limit_seconds\":";
    append_nullable_number(
        out, result.diagnostic_time_limit_seconds > 0.0,
        result.diagnostic_time_limit_seconds);
    out << ",\"watchdog_seconds\":";
    append_nullable_number(
        out, result.watchdog_seconds > 0.0, result.watchdog_seconds);
    out << ",\"watchdog_mode\":"
        << (result.watchdog_seconds > 0.0
                ? "\"cooperative_case_deadline\""
                : "null")
        << ",\"watchdog_expired\":"
        << (result.watchdog_expired ? "true" : "false");
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
    const Value* caps_for_trace = specification.find("caps");
    const auto append_cap_ratio = [&](const char* name,
                                      const std::uint64_t current) {
        if (std::string_view(name) == "max_discovered_states" &&
            result.max_discovered_states_override != 0) {
            append_nullable_number(
                out, true,
                static_cast<double>(current) /
                    result.max_discovered_states_override);
            return;
        }
        if (caps_for_trace == nullptr ||
            caps_for_trace->type != Type::Object) {
            out << "null";
            return;
        }
        const Value* cap = caps_for_trace->find(name);
        if (cap == nullptr || cap->type != Type::Number ||
            cap->number <= 0.0) {
            out << "null";
            return;
        }
        append_nullable_number(
            out, true, static_cast<double>(current) / cap->number);
    };
    out << "  \"bound_trace\":{\"sampling\":{"
           "\"round_changes\":true,\"bounded_wall_intervals\":true},"
           "\"time_to_first_incumbent_ms\":";
    append_nullable_number(
        out, result.has_time_to_first_incumbent,
        result.time_to_first_incumbent_ms);
    out << ",\"gap_milestones\":{\"absolute\":[";
    for (std::size_t i = 0; i < result.absolute_gap_milestones.size(); ++i) {
        if (i != 0) out << ',';
        out << "{\"threshold\":";
        append_nullable_number(
            out, true, result.absolute_gap_milestones[i].first);
        out << ",\"elapsed_ms\":";
        append_nullable_number(
            out, true, result.absolute_gap_milestones[i].second);
        out << '}';
    }
    out << "],\"relative\":[";
    for (std::size_t i = 0; i < result.relative_gap_milestones.size(); ++i) {
        if (i != 0) out << ',';
        out << "{\"threshold\":";
        append_nullable_number(
            out, true, result.relative_gap_milestones[i].first);
        out << ",\"elapsed_ms\":";
        append_nullable_number(
            out, true, result.relative_gap_milestones[i].second);
        out << '}';
    }
    out << "]},\"samples\":[";
    for (std::size_t i = 0; i < result.bound_trace.size(); ++i) {
        if (i != 0) out << ',';
        const CaseResult::BoundTraceEntry& entry = result.bound_trace[i];
        out << "{\"elapsed_ms\":";
        append_nullable_number(out, true, entry.elapsed_ms);
        out << ",\"phase\":" << entry.phase
            << ",\"round\":" << entry.round
            << ",\"lower_bound\":";
        append_nullable_number(out, true, entry.lower_bound);
        out << ",\"upper_bound\":";
        append_nullable_number(out, true, entry.upper_bound);
        out << ",\"absolute_gap\":";
        append_nullable_number(out, true, entry.absolute_gap);
        out << ",\"relative_gap\":";
        append_nullable_number(out, true, entry.relative_gap);
        out << ",\"incumbent_kind\":"
            << escape_json(incumbent_kind_name(entry.incumbent_kind))
            << ",\"states\":{\"discovered\":"
            << entry.discovered_states << ",\"expanded\":"
            << entry.expanded_states << ",\"frontier\":"
            << entry.frontier_states << "},\"work\":{\"rows\":"
            << entry.rows << ",\"transitions\":" << entry.transitions
            << ",\"reforge_work\":" << entry.reforge_work
            << "},\"memory\":{\"live_bytes\":"
            << entry.live_owned_bytes << ",\"peak_bytes\":"
            << entry.peak_owned_bytes << "},\"cap_proximity\":{"
               "\"discovered_states\":";
        append_cap_ratio("max_discovered_states", entry.discovered_states);
        out << ",\"expanded_states\":";
        append_cap_ratio("max_expanded_states", entry.expanded_states);
        out << ",\"rows\":";
        append_cap_ratio("max_state_action_rows", entry.rows);
        out << ",\"transitions\":";
        append_cap_ratio("max_transitions", entry.transitions);
        out << ",\"reforge_work\":";
        append_cap_ratio("max_reforge_work", entry.reforge_work);
        out << ",\"memory\":";
        append_cap_ratio("max_solver_owned_bytes", entry.peak_owned_bytes);
        out << "},\"progress_effect\":{\"raised_lower_bound\":"
            << (entry.raised_lower ? "true" : "false")
            << ",\"decreased_lower_bound\":"
            << (entry.decreased_lower ? "true" : "false")
            << ",\"lowered_upper_bound\":"
            << (entry.lowered_upper ? "true" : "false")
            << ",\"increased_upper_bound\":"
            << (entry.increased_upper ? "true" : "false") << "}}";
    }
    out << "]},\n";
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
            << result.solve_summary.skipped_action_count
            << ",\"policy_available\":"
            << (result.solve_summary.policy_available ? "true" : "false")
            << ",\"policy_status\":"
            << escape_json(policy_status_name(
                   result.solve_summary.policy_status))
            << ",\"termination\":"
            << escape_json(termination_name(
                   result.solve_summary.termination))
            << ",\"stop_cause\":"
            << escape_json(stop_cause_name(
                   result.solve_summary.stop_cause))
            << ",\"cap_hit_mask\":"
            << result.solve_summary.cap_hit_mask
            << ",\"action_counts\":{\"registry\":"
            << result.solve_summary.registry_action_count
            << ",\"candidate\":"
            << result.solve_summary.candidate_action_count
            << ",\"evaluator_supported\":"
            << result.solve_summary.evaluator_supported_action_count
            << ",\"supported_priced\":"
            << result.solve_summary.supported_priced_action_count
            << ",\"skipped_missing_price\":"
            << result.solve_summary.skipped_missing_price_count
            << ",\"skipped_unsupported\":"
            << result.solve_summary.skipped_unsupported_count << '}'
            << ",\"lower_bound\":";
        append_nullable_number(
            out, true, result.solve_summary.lower_bound);
        out << ",\"upper_bound\":";
        append_nullable_number(
            out, true, result.solve_summary.upper_bound);
        out << ",\"evaluated_policy_cost\":";
        append_nullable_number(
            out, true, result.solve_summary.evaluated_policy_cost);
        out << ",\"absolute_optimality_gap\":";
        append_nullable_number(
            out, true, result.solve_summary.absolute_optimality_gap);
        out << ",\"relative_optimality_gap\":";
        append_nullable_number(
            out, true, result.solve_summary.relative_optimality_gap);
        out << ",\"requested_absolute_optimality_gap\":";
        append_nullable_number(
            out, true,
            result.solve_summary.requested_absolute_optimality_gap);
        out << ",\"requested_relative_optimality_gap\":";
        append_nullable_number(
            out, true,
            result.solve_summary.requested_relative_optimality_gap);
        out << ",\"target_met\":"
            << (result.solve_summary.target_met ? "true" : "false")
            << ",\"target_fired\":"
            << escape_json(target_name(result.solve_summary.target_fired))
            << '}';
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
                               result.solve_summary.policy_available,
                           result.solve_summary.evaluated_policy_cost);
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
        const std::uint64_t required_runs = optional_u64(
            verification_spec, "runs", 0);
        const std::uint64_t required_seed = optional_u64(
            verification_spec, "seed", 20260715);
        const bool full_fixture_run =
            result.verification_finished &&
            !result.verification_time_limited && required_runs > 0 &&
            result.verification_requested_runs == required_runs &&
            result.verification.completed_runs == required_runs &&
            result.verification.target_runs == required_runs &&
            result.verification.seed == required_seed;
        const bool complete_cost_accounting =
            result.verification.cost_status == PC_COST_COMPLETE &&
            result.verification.missing_price_run_count == 0 &&
            result.verification.missing_price_action_count == 0;
        const bool verification_passed =
            full_fixture_run && complete_cost_accounting && off_policy == 0 &&
            mean_within_tolerance && success_within_tolerance;
        out << "{\"simulation\":{\"summary\":{"
            << "\"completed_runs\":"
            << result.verification.completed_runs
            << ",\"success_count\":"
            << result.verification.success_count
            << ",\"failure_count\":"
            << result.verification.failure_count
            << ",\"stop_count\":" << result.verification.stop_count
            << ",\"total_actions\":"
            << result.verification.total_actions
            << ",\"action_limit_count\":"
            << result.verification.action_limit_count
            << ",\"cost_limit_count\":"
            << result.verification.cost_limit_count
            << ",\"step_limit_count\":"
            << result.verification.step_limit_count
            << ",\"no_matching_edge_count\":"
            << result.verification.no_matching_edge_count
            << ",\"action_not_applied_count\":"
            << result.verification.action_not_applied_count
            << ",\"missing_price_run_count\":"
            << result.verification.missing_price_run_count
            << ",\"costed_action_count\":"
            << result.verification.costed_action_count
            << ",\"missing_price_action_count\":"
            << result.verification.missing_price_action_count
            << ",\"known_total_cost\":"
            << std::setprecision(17)
            << result.verification.known_total_cost
            << ",\"cost_status\":"
            << escape_json(
                   result.verification.cost_status == PC_COST_DISABLED
                       ? "disabled"
                       : (result.verification.cost_status == PC_COST_COMPLETE
                              ? "complete"
                              : "incomplete"))
            << ",\"seed\":" << result.verification.seed
            << ",\"target_runs\":" << result.verification.target_runs
            << "},\"sampled_accounting\":{"
            << "\"evidence_source\":\"simulator_sample\","
            << "\"sample_count\":"
            << result.verification.completed_runs
            << ",\"seed\":" << result.verification.seed
            << ",\"actions\":[";
        for (std::size_t i = 0;
             i < result.action_descriptor_distribution.size(); ++i) {
            if (i != 0) out << ',';
            const auto& action = result.action_descriptor_distribution[i];
            out << "{\"action_id\":" << escape_json(action.action_id)
                << ",\"count\":" << action.count
                << ",\"average_per_invocation\":";
            append_nullable_number(
                out, result.verification.completed_runs > 0,
                result.verification.completed_runs == 0
                    ? 0.0
                    : static_cast<double>(action.count) /
                          static_cast<double>(
                              result.verification.completed_runs));
            out << '}';
        }
        out << "],\"materials\":[";
        for (std::size_t i = 0;
             i < result.material_distribution.size(); ++i) {
            if (i != 0) out << ',';
            const auto& material = result.material_distribution[i];
            out << "{\"price_key\":" << escape_json(material.price_key)
                << ",\"count\":" << material.count
                << ",\"average_per_invocation\":";
            append_nullable_number(
                out, result.verification.completed_runs > 0,
                result.verification.completed_runs == 0
                    ? 0.0
                    : static_cast<double>(material.count) /
                          static_cast<double>(
                              result.verification.completed_runs));
            out << '}';
        }
        out << "]}},"
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

std::string compiler_name();

void write_partial_case_report(
    const fs::path& path, const Value& manifest,
    const std::string& corpus_schema, const fs::path& corpus_path,
    const fs::path& artifact_manifest, const Value& artifact_json,
    const Value& specification, const CaseResult& result) {
    std::ostringstream output;
    output << "{\n"
           << "\"schema_version\":\"solver_benchmark_partial_v1\",\n"
           << "\"run_status\":\"partial\",\n"
           << "\"runner\":\"native\",\n"
           << "\"corpus\":{\"id\":"
           << escape_json(required_string(manifest, "corpus_id"))
           << ",\"schema_version\":" << escape_json(corpus_schema) << ','
           << "\"manifest_path\":" << escape_json(corpus_path.string())
           << ",\"manifest\":" << json_of(manifest) << "},\n"
           << "\"artifact\":{\"manifest_path\":"
           << escape_json(artifact_manifest.string())
           << ",\"manifest\":" << json_of(artifact_json) << "},\n"
           << "\"environment\":{\"abi_version\":" << pc_abi_version()
           << ",\"compiler\":" << escape_json(compiler_name())
           << ",\"process_working_set_bytes\":" << process_working_set()
           << "},\n\"cases\":[\n";
    append_case_report(output, specification, result);
    output << "\n],\n\"all_expectations_met\":false\n}\n";
    write_file_atomic(fs::absolute(path), output.str());
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
        else if (argument == "--partial-output") {
            args.partial_output = value("--partial-output");
        }
        else if (argument == "--strategy-output") {
            args.strategy_output = value("--strategy-output");
        }
        else if (argument == "--case") args.case_id = value("--case");
        else if (argument == "--validate-only") args.validate_only = true;
        else if (argument == "--progress") args.emit_progress = true;
        else if (argument == "--goal-progress-gated-reforges") {
            args.goal_progress_gated_reforges = true;
        }
        else if (argument == "--max-discovered-states") {
            args.max_discovered_states_override =
                static_cast<std::uint32_t>(
                    std::stoul(value("--max-discovered-states")));
            if (args.max_discovered_states_override == 0) {
                throw std::runtime_error(
                    "--max-discovered-states must be positive");
            }
        }
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
    if (!args.partial_output.empty() && args.case_id.empty()) {
        throw std::runtime_error(
            "--partial-output requires exactly one selected --case");
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
        const std::string corpus_schema =
            required_string(manifest, "schema_version");
        if (corpus_schema != "solver_benchmark_corpus_v1" &&
            corpus_schema != "natural_t1_benchmark_corpus_v1" &&
            corpus_schema !=
                "cross_base_reliability_corpus_v1") {
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
        std::uint32_t pinned_abi = optional_u32(
            artifact_pin, "engine_abi_version", 0);
        if (pinned_abi == 0 &&
            (corpus_schema == "natural_t1_benchmark_corpus_v1" ||
             corpus_schema == "cross_base_reliability_corpus_v1")) {
            pinned_abi = optional_u32(
                required(manifest, "generator", Type::Object),
                "engine_abi_version", 0);
        }
        if (pinned_abi != pc_abi_version()) {
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
                validate_case_manifest_contract(manifest, specification);
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
                    CaseResult contract_probe;
                    initialize_forced_winner_contract(
                        specification, contract_probe);
                    create_case_objects(
                        data, specification, handles, start_item,
                        &contract_probe.product_action_ids);
                    contract_probe.forced_winner_price_override_checked =
                        contract_probe.has_forced_winner_contract;
                    enforce_dependency_candidate_contract(contract_probe);
                    if (contract_probe.has_forced_winner_contract) {
                        const std::string strategy_probe =
                            "{\"nodes\":[{\"kind\":\"operation\","
                            "\"operation\":{\"type\":" +
                            escape_json(
                                contract_probe
                                    .required_compiled_operation_type) +
                            "}}]}";
                        enforce_required_compiled_operation(
                            strategy_probe, contract_probe);
                        std::ostringstream report_probe;
                        append_case_report(
                            report_probe, specification, contract_probe);
                        const std::string report_probe_json =
                            report_probe.str();
                        const Value parsed_report = Parser(
                            report_probe_json.data(),
                            report_probe_json.size()).parse();
                        const Value& reported_contract = required(
                            parsed_report, "forced_winner_contract",
                            Type::Object);
                        const Value& reported_operation = required(
                            reported_contract,
                            "required_compiled_operation", Type::Object);
                        const Value& reported_price = required(
                            reported_contract, "synthetic_action_price",
                            Type::Object);
                        const Value& reported_dependency = required(
                            reported_contract,
                            "dependency_primitive_candidate", Type::Object);
                        if (!required(
                                 reported_operation, "present",
                                 Type::Bool).boolean ||
                            required_string(
                                reported_operation, "type") !=
                                contract_probe
                                    .required_compiled_operation_type ||
                            required(
                                reported_operation, "matching_node_count",
                                Type::Number).number != 1.0 ||
                            !required(
                                 reported_price, "checked",
                                 Type::Bool).boolean ||
                            required(
                                reported_price, "snapshot_quote",
                                Type::Number).number !=
                                contract_probe
                                    .forced_winner_snapshot_action_price ||
                            required(
                                reported_price, "forced_value",
                                Type::Number).number !=
                                contract_probe.forced_winner_action_price ||
                            !required(
                                 reported_dependency, "absent",
                                 Type::Bool).boolean ||
                            required_string(
                                reported_dependency, "action_id") !=
                                contract_probe.dependency_action_id) {
                            throw std::runtime_error(
                                "forced winner validate-only report probe "
                                "failed");
                        }
                    }
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
                   << ",\"schema_version\":"
                   << escape_json(corpus_schema) << ','
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
                const std::function<void(const CaseResult&)> checkpoint =
                    args.partial_output.empty()
                        ? std::function<void(const CaseResult&)>{}
                        : [&](const CaseResult& partial) {
                              write_partial_case_report(
                                  args.partial_output, manifest, corpus_schema,
                                  corpus_path, artifact_manifest, artifact_json,
                                  specification, partial);
                          };
                const CaseResult result = run_case(
                    data, specification, args.skip_verification,
                    args.strategy_output, args.emit_progress,
                    args.verification_runs, args.verification_seed,
                    args.verification_chunk_runs,
                    args.verification_time_limit_seconds,
                    args.goal_progress_gated_reforges,
                    args.max_discovered_states_override,
                    args.exact_strategy_evaluation,
                    args.exact_strategy_evaluation_time_limit_seconds,
                    checkpoint);
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
