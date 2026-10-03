#include "recombination_solver.hpp"
#include "handles_internal.hpp"
#include "poecraft/recombination_solver.h"
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>

struct pc_recombination_solver {
    std::shared_ptr<const poecraft::SessionImpl> session;
    poecraft::RecombSolverResult result;
    std::string json;
};
namespace {
pc_result fail(pc_error_info* error, pc_result code, const char* reason) {
    if (error) { pc_error_info_init(error); error->code = code;
        std::snprintf(error->message, sizeof(error->message), "%s", reason); }
    return code;
}
std::string quote(const std::string& value) {
    std::string out = "\"";
    for (unsigned char c : value) {
        if (c == '"' || c == '\\') { out += '\\'; out += char(c); }
        else if (c < 32) { char escaped[7]; std::snprintf(escaped, sizeof(escaped), "\\u%04x", c); out += escaped; }
        else out += char(c);
    }
    return out + "\"";
}
const char* decision_kind(poecraft::RecombDecisionKind kind) {
    switch (kind) {
    case poecraft::RecombDecisionKind::Acquire: return "acquire";
    case poecraft::RecombDecisionKind::Recombine: return "recombine";
    case poecraft::RecombDecisionKind::Discard: return "discard";
    }
    throw std::logic_error("Invalid inventory decision");
}
std::string result_json(const poecraft::RecombSolverRequest& request, const poecraft::RecombSolverResult& result) {
    std::ostringstream out; out << std::setprecision(17);
    const auto& session = *request.session;
    out << "{\"version\":\"random_recombination_inventory_v1\",\"model_id\":" << quote(result.model_id)
        << ",\"projection_id\":" << quote(poecraft::kRandomRecombProjection)
        << ",\"game_odds_estimated\":true,\"global_optimality_claim\":false,"
           "\"economic_inputs_declared\":true,\"native_feeder_cost_certified\":false,"
           "\"inventory_capacity\":2,\"cost_complete\":true,\"search_converged\":" << (result.search_converged ? "true" : "false")
        << ",\"policy_iterations\":" << result.policy_iterations << ",\"work_spent\":" << result.work_spent
        << ",\"price_identity\":" << quote(request.price_identity) << ",\"base_metadata_path\":"
        << quote(session.data->string_at(session.data->base_metadata_path_sid.at(session.base_index)))
        << ",\"item_level\":" << session.item_level << ",\"data_identity\":[";
    const auto& data = *session.data; bool comma = false;
    for (const auto& hash : {data.artifact_data_hash, data.artifact_source_hash, data.artifact_game_data_hash, data.artifact_strings_hash}) {
        if (comma) out << ','; comma = true; out << quote(hash);
    }
    out << "],\"goal_set\":" << request.goal_set_json << ",\"initial_state\":" << result.initial_state
        << ",\"entry_cost_chaos\":" << result.entry_cost_chaos << ",\"expected_cost_chaos\":" << result.expected_cost_chaos
        << ",\"expected_recombinations\":" << result.expected_recombinations
        << ",\"expected_discards\":" << result.expected_discards
        << ",\"recombination_cost_chaos\":" << *request.recombination_cost_chaos << ",\"acquisitions\":[";
    for (unsigned a = 0; a < request.acquisitions.size(); ++a) {
        if (a) out << ','; const auto& offer = request.acquisitions[a];
        out << "{\"index\":" << a << ",\"id\":" << quote(offer.id) << ",\"source_kind\":" << quote(offer.source_kind)
            << ",\"quote_identity\":" << quote(offer.quote_identity) << ",\"total_cost_chaos\":" << offer.total_cost_chaos
            << ",\"expected_invocations\":" << result.expected_acquisitions[a] << ",\"item\":"
            << poecraft::random_recomb_item_json(offer.item, session) << '}';
    }
    out << "],\"items\":[";
    for (unsigned i = 0; i < result.items.size(); ++i) {
        if (i) out << ','; out << "{\"id\":" << i << ",\"item\":" << poecraft::random_recomb_item_json(result.items[i], session) << '}';
    }
    out << "],\"states\":[";
    for (unsigned s = 0; s < result.inventories.size(); ++s) {
        if (s) out << ','; out << "{\"id\":" << s << ",\"terminal\":" << (result.terminal[s] ? "true" : "false") << ",\"items\":[";
        for (unsigned i = 0; i < result.inventories[s].size(); ++i) { if (i) out << ','; out << result.inventories[s][i]; }
        out << "]}";
    }
    out << "],\"policy\":[";
    for (unsigned i = 0; i < result.policy.size(); ++i) {
        if (i) out << ','; const auto& decision = result.policy[i];
        out << "{\"state\":" << decision.state << ",\"kind\":" << quote(decision_kind(decision.kind));
        if (decision.kind == poecraft::RecombDecisionKind::Acquire) out << ",\"acquisition_index\":" << decision.acquisition;
        if (decision.kind == poecraft::RecombDecisionKind::Recombine)
            out << ",\"input_a_spec\":" << decision.input_a << ",\"input_b_spec\":" << decision.input_b;
        if (decision.kind == poecraft::RecombDecisionKind::Discard) out << ",\"inventory_index\":" << decision.input_a;
        out << ",\"outcomes\":[";
        for (unsigned j = 0; j < decision.outcomes.size(); ++j) {
            if (j) out << ','; out << "{\"state\":" << decision.outcomes[j].first
                << ",\"probability\":" << decision.outcomes[j].second << '}';
        }
        out << "]}";
    }
    out << "],\"exclusions\":[";
    for (unsigned i = 0; i < result.exclusions.size(); ++i) { if (i) out << ','; out << quote(result.exclusions[i]); }
    out << "],\"unobserved_goal_properties\":[\"recorded_rolls\",\"memory_strands\",\"sockets\",\"enchantments\",\"defence_percentiles\"]}";
    auto text = out.str();
    if (text.size() > 8 * 1024 * 1024) throw std::length_error("Inventory solver result byte cap reached");
    return text;
}
}
pc_result pc_recombination_solver_create(pc_session_handle session,
        const pc_recombination_solver_options* options, pc_recombination_solver_handle* out, pc_error_info* error) {
    if (!session || !options || !out || options->struct_size != sizeof(*options) ||
        options->abi_version != PC_ABI_VERSION || options->solver_version != PC_RECOMBINATION_SOLVER_VERSION ||
        !options->model_id || std::strcmp(options->model_id, poecraft::kRandomRecombModel) ||
        !options->price_identity || !options->goal_set_json ||
        !options->acquisitions || !options->acquisition_count || options->acquisition_count > 32 ||
        options->initial_item_count > 2 || (options->initial_item_count && (!options->initial_items || !options->initial_item_costs)) ||
        options->goal_set_json_size > 256 * 1024)
        return fail(error, PC_RESULT_INVALID_ARGUMENT, "Invalid versioned recombination inventory request");
    try {
        poecraft::RecombSolverRequest request; request.session = session->impl;
        request.price_identity = options->price_identity;
        request.goal_set_json.assign(options->goal_set_json, options->goal_set_json_size);
        request.recombination_cost_chaos = options->recombination_cost_chaos;
        request.recombination_cost_complete = options->recombination_cost_complete != 0;
        if (options->max_items) request.max_items = options->max_items;
        if (options->max_states) request.max_states = options->max_states;
        if (options->max_policy_iterations) request.max_policy_iterations = options->max_policy_iterations;
        if (options->max_work) request.max_work = options->max_work;
        for (unsigned a = 0; a < options->acquisition_count; ++a) {
            const auto& offer = options->acquisitions[a];
            if (!offer.id || !offer.source_kind || !offer.quote_identity || !offer.item)
                return fail(error, PC_RESULT_INVALID_ARGUMENT, "Missing exact acquisition specification");
            request.acquisitions.push_back({offer.id, offer.source_kind, offer.quote_identity, *offer.item,
                offer.total_cost_chaos, offer.cost_complete != 0});
        }
        for (unsigned i = 0; i < options->initial_item_count; ++i) {
            request.initial_items.push_back(options->initial_items[i]); request.initial_item_costs.push_back(options->initial_item_costs[i]);
        }
        auto owned = std::make_unique<pc_recombination_solver>(); owned->session = request.session;
        owned->result = poecraft::solve_random_recomb_inventory(request);
        owned->json = result_json(request, owned->result);
        *out = owned.release(); if (error) pc_error_info_init(error); return PC_RESULT_OK;
    } catch (const std::length_error& ex) { return fail(error, PC_RESULT_CAPACITY_EXCEEDED, ex.what()); }
      catch (const std::invalid_argument& ex) { return fail(error, PC_RESULT_UNSUPPORTED_FEATURE, ex.what()); }
      catch (const std::exception& ex) { return fail(error, PC_RESULT_INTERNAL_ERROR, ex.what()); }
}
void pc_recombination_solver_destroy(pc_recombination_solver_handle solver) { delete solver; }
pc_result pc_recombination_solver_result_json(pc_recombination_solver_handle solver,
        char* buffer, size_t size, size_t* length, pc_error_info* error) {
    if (!solver || !length) return fail(error, PC_RESULT_INVALID_ARGUMENT, "Invalid inventory result query");
    *length = solver->json.size();
    if (!buffer || size < solver->json.size() + 1) return fail(error, PC_RESULT_BUFFER_TOO_SMALL, "Inventory result buffer required");
    std::memcpy(buffer, solver->json.c_str(), solver->json.size() + 1);
    if (error) pc_error_info_init(error); return PC_RESULT_OK;
}
pc_result pc_recombination_solver_session(pc_recombination_solver_handle solver,
        pc_session_handle* out, pc_error_info* error) {
    if (!solver || !out) return fail(error, PC_RESULT_INVALID_ARGUMENT, "Invalid inventory session query");
    try {
        auto owned = std::make_unique<pc_session>();
        owned->impl = std::const_pointer_cast<poecraft::SessionImpl>(solver->session);
        *out = owned.release(); if (error) pc_error_info_init(error); return PC_RESULT_OK;
    } catch (const std::exception& ex) { return fail(error, PC_RESULT_INTERNAL_ERROR, ex.what()); }
}
pc_result pc_recombination_solver_item(pc_recombination_solver_handle solver,
        uint32_t id, pc_item_state* out, pc_error_info* error) {
    if (!solver || !out || id >= solver->result.items.size())
        return fail(error, PC_RESULT_INVALID_ARGUMENT, "Invalid inventory item specification query");
    *out = solver->result.items[id]; if (error) pc_error_info_init(error); return PC_RESULT_OK;
}
