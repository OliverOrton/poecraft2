#include "solver_eval_helpers.hpp"

namespace poecraft {
namespace solver {

namespace {

void append_material_totals_json(
    BoundedJson& out,
    const std::vector<StrategyEvalMaterialTotal>& materials,
    double divisor = 1.0) {
    out.push_back('[');
    for (std::size_t i = 0; i < materials.size(); ++i) {
        if (i != 0) out.push_back(',');
        const StrategyEvalMaterialTotal& material = materials[i];
        out += "{\"price_key\":\"" + json_escape(material.price_key) +
               "\",\"expected_quantity\":";
        append_number(out, material.expected_quantity / divisor);
        out += ",\"price_status\":\"";
        out += material.priced ? "priced" : "missing";
        out += "\",\"unit_price\":";
        if (material.priced) {
            append_number(out, material.unit_price);
        } else {
            out += "null";
        }
        out += ",\"cost_contribution\":";
        if (material.priced) {
            append_number(out, material.cost_contribution / divisor);
        } else {
            out += "null";
        }
        out.push_back('}');
    }
    out.push_back(']');
}

void append_techniques_json(
    BoundedJson& out,
    const std::map<std::string, double>& techniques,
    double divisor = 1.0) {
    out.push_back('{');
    std::size_t index = 0;
    for (const auto& [key, value] : techniques) {
        if (index++ != 0) out.push_back(',');
        out += "\"" + json_escape(key) + "\":";
        append_number(out, value / divisor);
    }
    out.push_back('}');
}

void append_action_totals_json(
    BoundedJson& out,
    const std::vector<StrategyEvalActionTotal>& actions,
    const std::vector<StrategyEvalMaterialTotal>& priced_materials,
    double divisor = 1.0) {
    std::map<std::string, const StrategyEvalMaterialTotal*> price_by_key;
    for (const StrategyEvalMaterialTotal& material : priced_materials) {
        price_by_key.emplace(material.price_key, &material);
    }
    double known_total_cost = 0.0;
    for (const StrategyEvalMaterialTotal& material : priced_materials) {
        if (material.priced) known_total_cost += material.cost_contribution;
    }
    out.push_back('[');
    for (std::size_t i = 0; i < actions.size(); ++i) {
        if (i != 0) out.push_back(',');
        const StrategyEvalActionTotal& action = actions[i];
        out += "{\"id\":\"" + json_escape(action.id) +
               "\",\"display_name\":\"" +
               json_escape(action.display_name) +
               "\",\"expected_visits\":";
        append_number(out, action.expected_visits / divisor);
        out += ",\"expected_applied\":";
        append_number(out, action.expected_applied / divisor);
        double action_known_spend = 0.0;
        bool action_spend_complete = true;
        for (const std::string& key : action.price_keys) {
            const auto price = price_by_key.find(key);
            if (price == price_by_key.end() || !price->second->priced) {
                action_spend_complete = false;
            } else {
                action_known_spend +=
                    action.expected_applied * price->second->unit_price /
                    divisor;
            }
        }
        out += ",\"expected_spend_known\":";
        append_number(out, action_known_spend);
        out += ",\"expected_spend_complete\":";
        out += action_spend_complete ? "true" : "false";
        out += ",\"known_cost_share\":";
        if (known_total_cost > 0.0) {
            append_number(
                out, action_known_spend /
                         (known_total_cost / divisor));
        } else {
            out += "null";
        }
        out += ",\"probability_of_any_use\":{";
        if (action.expected_visits == 0.0) {
            out += "\"status\":\"exact_zero\",\"value\":0}";
        } else {
            out += "\"status\":\"not_computable_from_occupancy\","
                   "\"value\":null}";
        }
        out += ",\"reachable_states\":" +
               std::to_string(action.reachable_states);
        out += ",\"reachable_regions\":" +
               std::to_string(action.regions.size());
        out += ",\"classifications\":[";
        for (std::size_t role = 0; role < action.classifications.size();
             ++role) {
            if (role != 0) out.push_back(',');
            out += "\"" + json_escape(action.classifications[role]) + "\"";
        }
        out += "],\"materials\":[";
        std::map<std::string, std::uint32_t> quantities;
        for (const std::string& key : action.price_keys) ++quantities[key];
        std::size_t material_index = 0;
        for (const auto& [key, count] : quantities) {
            if (material_index++ != 0) out.push_back(',');
            const double quantity =
                action.expected_applied * static_cast<double>(count) /
                divisor;
            out += "{\"price_key\":\"" + json_escape(key) +
                   "\",\"expected_quantity\":";
            append_number(out, quantity);
            const auto found = price_by_key.find(key);
            const bool priced =
                found != price_by_key.end() && found->second->priced;
            out += ",\"price_status\":\"";
            out += priced ? "priced" : "missing";
            out += "\",\"unit_price\":";
            if (priced) {
                append_number(out, found->second->unit_price);
            } else {
                out += "null";
            }
            out += ",\"cost_contribution\":";
            if (priced) {
                append_number(out, quantity * found->second->unit_price);
            } else {
                out += "null";
            }
            out.push_back('}');
        }
        out += "],\"regions\":[";
        for (std::size_t region_index = 0;
             region_index < action.regions.size(); ++region_index) {
            if (region_index != 0) out.push_back(',');
            const StrategyEvalActionRegion& region =
                action.regions[region_index];
            out += "{\"goal_progress\":" +
                   std::to_string(region.goal_progress);
            out += ",\"rarity\":" + std::to_string(region.rarity);
            out += ",\"blocker_count\":" +
                   std::to_string(region.blocker_count);
            out += ",\"crafted_count\":" +
                   std::to_string(region.crafted_count);
            out += ",\"fractured_goal_mask\":" +
                   std::to_string(region.fractured_goal_mask);
            out += ",\"fractured_count\":" +
                   std::to_string(region.fractured_count);
            out += ",\"reachable_states\":" +
                   std::to_string(region.reachable_states);
            out += ",\"expected_visits\":";
            append_number(out, region.expected_visits / divisor);
            out += ",\"expected_applied\":";
            append_number(out, region.expected_applied / divisor);
            out.push_back('}');
        }
        out += "],\"raw_nodes\":[";
        for (std::size_t node = 0; node < action.nodes.size(); ++node) {
            if (node != 0) out.push_back(',');
            const StrategyEvalActionNode& entry = action.nodes[node];
            out += "{\"node_id\":\"" + json_escape(entry.node_id) +
                   "\",\"expected_visits\":";
            append_number(out, entry.expected_visits / divisor);
            out += ",\"expected_applied\":";
            append_number(out, entry.expected_applied / divisor);
            out.push_back('}');
        }
        out += "]}";
    }
    out.push_back(']');
}

void append_cost_totals_json(
    BoundedJson& out,
    double expected_actions,
    double known_cost,
    bool complete,
    double total_cost,
    double divisor = 1.0) {
    out += "{\"expected_actions\":";
    append_number(out, expected_actions / divisor);
    out += ",\"known_expected_cost\":";
    append_number(out, known_cost / divisor);
    out += ",\"total_expected_cost\":";
    if (complete) {
        append_number(out, total_cost / divisor);
    } else {
        out += "null";
    }
    out += ",\"cost_complete\":";
    out += complete ? "true" : "false";
    out.push_back('}');
}

void append_reforge_effort_json(
    BoundedJson& out,
    const ReforgeEffortBreakdown& effort) {
    struct Field {
        const char* name;
        std::uint64_t ReforgeEffortBreakdown::*member;
    };
    static constexpr Field fields[] = {
        {"rows_begun", &ReforgeEffortBreakdown::rows_begun},
        {"rows_completed", &ReforgeEffortBreakdown::rows_completed},
        {"rows_interrupted", &ReforgeEffortBreakdown::rows_interrupted},
        {"rows_cache_reused", &ReforgeEffortBreakdown::rows_cache_reused},
        {"rows_discarded", &ReforgeEffortBreakdown::rows_discarded},
        {"rows_published", &ReforgeEffortBreakdown::rows_published},
        {"pool_entries_scanned", &ReforgeEffortBreakdown::pool_entries_scanned},
        {"physical_families_built", &ReforgeEffortBreakdown::physical_families_built},
        {"roll_buckets_built", &ReforgeEffortBreakdown::roll_buckets_built},
        {"exclusion_group_checks", &ReforgeEffortBreakdown::exclusion_group_checks},
        {"availability_classes_built", &ReforgeEffortBreakdown::availability_classes_built},
        {"availability_words_built", &ReforgeEffortBreakdown::availability_words_built},
        {"frontier_nodes", &ReforgeEffortBreakdown::frontier_nodes},
        {"dense_bucket_probes", &ReforgeEffortBreakdown::dense_bucket_probes},
        {"availability_words_scanned", &ReforgeEffortBreakdown::availability_words_scanned},
        {"eligible_nonterminal_edges", &ReforgeEffortBreakdown::eligible_nonterminal_edges},
        {"terminal_contributions", &ReforgeEffortBreakdown::terminal_contributions},
        {"canonical_terminal_successors", &ReforgeEffortBreakdown::canonical_terminal_successors},
        {"duplicate_terminal_contributions", &ReforgeEffortBreakdown::duplicate_terminal_contributions},
        {"raw_choice_entries", &ReforgeEffortBreakdown::raw_choice_entries},
        {"identity_tree_nodes", &ReforgeEffortBreakdown::identity_tree_nodes},
        {"successor_publication_attempts", &ReforgeEffortBreakdown::successor_publication_attempts},
        {"successor_unique_insertions", &ReforgeEffortBreakdown::successor_unique_insertions},
        {"successor_duplicate_merges", &ReforgeEffortBreakdown::successor_duplicate_merges},
        {"state_interning_attempts", &ReforgeEffortBreakdown::state_interning_attempts},
        {"v3_predecessor_index_entries", &ReforgeEffortBreakdown::v3_predecessor_index_entries},
        {"v3_denominator_edges", &ReforgeEffortBreakdown::v3_denominator_edges},
        {"v3_subset_checks", &ReforgeEffortBreakdown::v3_subset_checks},
        {"v3_candidate_sets", &ReforgeEffortBreakdown::v3_candidate_sets},
        {"v3_recurrence_terms", &ReforgeEffortBreakdown::v3_recurrence_terms},
        {"v3_commits", &ReforgeEffortBreakdown::v3_commits},
        {"nested_automatic_child_logical_work", &ReforgeEffortBreakdown::nested_automatic_child_logical_work},
        {"nested_automatic_child_active_work", &ReforgeEffortBreakdown::nested_automatic_child_active_work},
    };
    out.push_back('{');
    for (std::size_t i = 0; i < std::size(fields); ++i) {
        if (i != 0) out.push_back(',');
        out += "\"";
        out += fields[i].name;
        out += "\":" + std::to_string(effort.*fields[i].member);
    }
    out.push_back('}');
}

void append_reforge_resource_accounting_json(
    BoundedJson& out,
    const StrategyEvalResult& result) {
    out += "{\"schema_version\":2,";
    out += "\"cap_contract\":{\"version\":1,";
    out += "\"cap\":\"max_reforge_work\",";
    out += "\"basis\":\"logical_work_v1\"},";
    out += "\"legacy_active_work\":" +
           std::to_string(result.reforge_work);
    out += ",\"logical_work_v1\":" +
           std::to_string(result.reforge_logical_work_v1);
    out += ",\"evaluator_work\":{\"v1\":" +
           std::to_string(result.reforge_evaluator_work_v1);
    out += ",\"v2\":" +
           std::to_string(result.reforge_evaluator_work_v2);
    out += ",\"v3\":" +
           std::to_string(result.reforge_evaluator_work_v3) + "}";
    out += ",\"components\":";
    append_reforge_effort_json(out, result.reforge_effort);
    out += ",\"row_samples\":[";
    for (std::size_t i = 0; i < result.reforge_row_samples.size(); ++i) {
        if (i != 0) out.push_back(',');
        const ReforgeRowTelemetry& row = result.reforge_row_samples[i];
        out += "{\"sequence\":" + std::to_string(row.sequence);
        out += ",\"action_index\":" + std::to_string(row.action_index);
        out += ",\"owner\":\"";
        out += reforge_row_owner_name(row.owner);
        out += "\",\"family\":\"";
        out += reforge_row_family_name(row.family);
        out += "\",\"evaluator\":\"";
        out += reforge_evaluator_version_name(row.evaluator);
        out += "\",\"cache\":\"";
        out += (row.cache_reused ? "hit" : "miss");
        out += "\",\"cache_reused\":";
        out += (row.cache_reused ? "true" : "false");
        out += ",\"disposition\":\"";
        out += reforge_row_disposition_name(row.disposition);
        out += "\",\"legacy_active_work\":" +
               std::to_string(row.legacy_active_work);
        out += ",\"logical_work_v1\":" +
               std::to_string(row.logical_work_v1);
        out += ",\"evaluator_work\":{\"v1\":" +
               std::to_string(row.evaluator_work_v1);
        out += ",\"v2\":" + std::to_string(row.evaluator_work_v2);
        out += ",\"v3\":" + std::to_string(row.evaluator_work_v3) + "}";
        out += ",\"components\":";
        append_reforge_effort_json(out, row.components);
        out.push_back('}');
    }
    out += "],\"row_samples_omitted\":" +
           std::to_string(result.reforge_row_samples_omitted) + "}";
}

} // namespace

std::string serialize_strategy_eval(const StrategyEvalResult& result) {
    BoundedJson out(result.max_output_json_bytes);
    out += "{\"version\":\"v1\",\"converged\":";
    out += result.converged ? "true" : "false";
    out += ",\"sweeps\":" + std::to_string(result.sweeps);
    out += ",\"residual_mass\":";
    append_number(out, result.residual_mass);
    out += ",\"terminals\":{";
    out += "\"success\":";
    append_number(out, result.success_probability);
    out += ",\"failure\":";
    append_number(out, result.failure_probability);
    out += ",\"stop\":";
    append_number(out, result.stop_probability);
    out += ",\"action_not_applied\":";
    append_number(out, result.action_not_applied_probability);
    out += ",\"no_matching_edge\":";
    append_number(out, result.no_matching_edge_probability);
    out += ",\"unresolved\":";
    append_number(out, result.unresolved_probability);
    out += ",\"by_node\":[";
    for (std::size_t i = 0; i < result.terminal_nodes.size(); ++i) {
        if (i != 0) out += ',';
        const StrategyEvalTerminalNode& node = result.terminal_nodes[i];
        out += "{\"node_id\":\"" + json_escape(node.node_id) +
               "\",\"kind\":\"" + terminal_name(node.terminal_kind) +
               "\",\"p\":";
        append_number(out, node.probability);
        out += '}';
    }
    out += "]}";

    out += ",\"unresolved_by_node\":[";
    for (std::size_t i = 0; i < result.unresolved_by_node.size(); ++i) {
        if (i != 0) out += ',';
        const StrategyEvalNodeMass& node = result.unresolved_by_node[i];
        out += "{\"node_id\":\"" + json_escape(node.node_id) +
               "\",\"mass\":";
        append_number(out, node.mass);
        out += '}';
    }
    out += ']';

    const auto& observation = result.observation_propagation;
    out += ",\"observation_propagation\":{\"nodes\":" +
           std::to_string(observation.nodes);
    out += ",\"groups\":" + std::to_string(observation.groups);
    out += ",\"rounds\":" + std::to_string(observation.rounds);
    out += ",\"unique_canonical_requirements\":" +
           std::to_string(observation.unique_canonical_requirements);
    out += ",\"direct_payload_bytes\":{\"p50\":" +
           std::to_string(observation.direct_payload_p50_bytes) +
           ",\"p95\":" +
           std::to_string(observation.direct_payload_p95_bytes) +
           ",\"max\":" +
           std::to_string(observation.direct_payload_max_bytes) + "}";
    out += ",\"propagated_payload_bytes\":{\"p50\":" +
           std::to_string(observation.propagated_payload_p50_bytes) +
           ",\"p95\":" +
           std::to_string(observation.propagated_payload_p95_bytes) +
           ",\"max\":" +
           std::to_string(observation.propagated_payload_max_bytes) + "}";
    out += ",\"projected_peak_bytes\":" +
           std::to_string(observation.projected_peak_bytes);
    out += ",\"actual_peak_bytes\":" +
           std::to_string(observation.actual_peak_bytes);
    out += ",\"retained_bytes\":" +
           std::to_string(observation.retained_bytes) + "}";

    const auto& row_census = result.operation_row_census;
    out += ",\"operation_row_census\":{\"materialized_rows\":" +
           std::to_string(row_census.materialized_rows);
    out += ",\"shared_row_reuses\":" +
           std::to_string(row_census.shared_row_reuses);
    out += ",\"replayable_rows\":" +
           std::to_string(row_census.replayable_rows);
    out += ",\"stable_shared_rows\":" +
           std::to_string(row_census.stable_shared_rows);
    out += ",\"unique_stable_kernels\":" +
           std::to_string(row_census.unique_stable_kernels);
    out += ",\"state_local_rows\":" +
           std::to_string(row_census.state_local_rows);
    out += ",\"route_shapes\":{\"direct_repeat_rows\":" +
           std::to_string(row_census.direct_repeat_rows);
    out += ",\"local_gated_route_rows\":" +
           std::to_string(row_census.local_gated_route_rows);
    out += ",\"other_operation_rows\":" +
           std::to_string(row_census.other_operation_rows) + "}";
    out += ",\"kernel_authority\":{\"goal_progress_gated_rows\":" +
           std::to_string(row_census.goal_progress_gated_rows);
    out += ",\"full_physical_rows\":" +
           std::to_string(row_census.full_physical_rows) + "}";
    out += ",\"local_gated_route_proof\":{\"proved_rows\":" +
           std::to_string(row_census.local_gated_route_proved_rows);
    out += ",\"shape_rejections\":" +
           std::to_string(row_census.local_gated_route_shape_rejections);
    out += ",\"condition_rejections\":" +
           std::to_string(
               row_census.local_gated_route_condition_rejections);
    out += ",\"target_rejections\":" +
           std::to_string(row_census.local_gated_route_target_rejections);
    out += ",\"root_rejections\":" +
           std::to_string(row_census.local_gated_route_root_rejections) +
           "}";
    out += ",\"local_gated_full_rows\":{\"outcome_entries\":" +
           std::to_string(row_census.local_gated_full_outcome_entries);
    out += ",\"routed_transitions\":" +
           std::to_string(
               row_census.local_gated_full_routed_transitions);
    out += ",\"outcome_payload_bytes\":" +
           std::to_string(
               row_census.local_gated_full_outcome_payload_bytes);
    out += ",\"routed_payload_bytes\":" +
           std::to_string(
               row_census.local_gated_full_routed_payload_bytes) + "}";
    out += ",\"exact_outcome_entries\":" +
           std::to_string(row_census.exact_outcome_entries);
    out += ",\"routed_transitions\":" +
           std::to_string(row_census.routed_transitions);
    out += ",\"absorptions\":" +
           std::to_string(row_census.absorptions);
    out += ",\"exact_outcome_payload_bytes\":" +
           std::to_string(row_census.exact_outcome_payload_bytes);
    out += ",\"unique_stable_kernel_payload_bytes\":" +
           std::to_string(row_census.unique_stable_kernel_payload_bytes);
    out += ",\"routed_payload_bytes\":" +
           std::to_string(row_census.routed_payload_bytes);
    out += ",\"replay_route_token_bytes\":" +
           std::to_string(row_census.replay_route_token_bytes);
    out += ",\"replay_route_result_authorities\":" +
           std::to_string(row_census.replay_route_result_authorities);
    out += ",\"compact_attribution_rows\":" +
           std::to_string(row_census.compact_attribution_rows);
    out += ",\"compact_attribution_edges\":" +
           std::to_string(row_census.compact_attribution_edges);
    out += ",\"projected_u32_route_tokens_bytes\":" +
           std::to_string(row_census.projected_u32_route_tokens_bytes);
    out += ",\"source_edge_selections\":" +
           std::to_string(row_census.source_edge_selections);
    out += ",\"deterministic_route_resolutions\":" +
           std::to_string(row_census.deterministic_route_resolutions);
    out += ",\"sampled_source_edge_selections\":" +
           std::to_string(row_census.sampled_source_edge_selections);
    out += ",\"sampled_source_edge_ns\":" +
           std::to_string(row_census.sampled_source_edge_ns);
    out += ",\"sampled_deterministic_routes\":" +
           std::to_string(row_census.sampled_deterministic_routes);
    out += ",\"sampled_deterministic_route_ns\":" +
           std::to_string(row_census.sampled_deterministic_route_ns);
    out += ",\"sampled_route_keys\":" +
           std::to_string(row_census.sampled_route_keys);
    out += ",\"sampled_route_key_reuses\":" +
           std::to_string(row_census.sampled_route_key_reuses);
    out += ",\"sampled_row_completion_ns\":" +
           std::to_string(row_census.sampled_row_completion_ns) + "}";

    out += ",\"failures_by_node\":[";
    for (std::size_t i = 0; i < result.failures_by_node.size(); ++i) {
        if (i != 0) out += ',';
        const StrategyEvalFailure& failure = result.failures_by_node[i];
        out += "{\"node_id\":\"" + json_escape(failure.node_id) +
               "\",\"reason\":\"" + json_escape(failure.reason) +
               "\",\"p\":";
        append_number(out, failure.probability);
        out += '}';
    }
    out += ']';

    out += ",\"expected_actions\":";
    append_number(out, result.expected_actions);
    out += ",\"expected_consumption\":[";
    std::size_t consumption_index = 0;
    for (const auto& [key, quantity] : result.expected_consumption) {
        if (consumption_index++ != 0) out += ',';
        out += "{\"key\":\"" + json_escape(key) +
               "\",\"quantity\":";
        append_number(out, quantity);
        out += '}';
    }
    out += ']';

    out += ",\"accounting\":{\"version\":\"s8.4_v1\",\"semantics\":{";
    out += "\"primary\":\"per_strategy_invocation\",";
    out += "\"terminal_mass_separate\":true,";
    out += "\"success_normalized_basis\":";
    if (result.success_normalized_enabled) {
        out += "\"independent_whole_strategy_retries\"";
    } else {
        out += "null";
    }
    out += ",\"success_normalized_is_conditional_path_expectation\":false}";
    out += ",\"pricing\":{\"status\":\"";
    out += !result.pricing_enabled
               ? "disabled"
               : (result.cost_complete ? "complete" : "incomplete");
    out += "\",\"economy_id\":";
    if (result.pricing_enabled) {
        out += "\"" + json_escape(result.economy_id) + "\"";
    } else {
        out += "null";
    }
    out += ",\"missing_price_keys\":[";
    std::size_t missing_price_index = 0;
    for (const StrategyEvalMaterialTotal& material : result.material_totals) {
        if (material.priced) continue;
        if (missing_price_index++ != 0) out.push_back(',');
        out += "\"" + json_escape(material.price_key) + "\"";
    }
    out += "]}";
    out += ",\"totals\":{\"per_invocation\":";
    append_cost_totals_json(
        out, result.expected_actions, result.known_expected_cost,
        result.cost_complete, result.total_expected_cost);
    out += ",\"success_normalized\":";
    if (result.success_normalized_enabled) {
        out += "{\"basis\":\"independent_whole_strategy_retries\",";
        out += "\"success_probability_denominator\":";
        append_number(out, result.success_probability);
        out += ",\"expected_invocations\":";
        append_number(out, 1.0 / result.success_probability);
        out += ",\"work\":";
        append_cost_totals_json(
            out, result.expected_actions, result.known_expected_cost,
            result.cost_complete, result.total_expected_cost,
            result.success_probability);
        out.push_back('}');
    } else {
        out += "null";
    }
    out.push_back('}');
    out += ",\"actions\":{\"per_invocation\":";
    append_action_totals_json(
        out, result.action_totals, result.material_totals);
    out += ",\"success_normalized\":";
    if (result.success_normalized_enabled) {
        append_action_totals_json(
            out, result.action_totals, result.material_totals,
            result.success_probability);
    } else {
        out += "null";
    }
    out.push_back('}');
    out += ",\"materials\":{\"per_invocation\":";
    append_material_totals_json(out, result.material_totals);
    out += ",\"success_normalized\":";
    if (result.success_normalized_enabled) {
        append_material_totals_json(
            out, result.material_totals, result.success_probability);
    } else {
        out += "null";
    }
    out.push_back('}');
    out += ",\"techniques\":{\"per_invocation\":";
    append_techniques_json(out, result.technique_totals);
    out += ",\"success_normalized\":";
    if (result.success_normalized_enabled) {
        append_techniques_json(
            out, result.technique_totals, result.success_probability);
    } else {
        out += "null";
    }
    out.push_back('}');
    out += ",\"review_sections\":{\"enabled\":";
    out += result.review_sections_enabled ? "true" : "false";
    out += ",\"items\":[";
    for (std::size_t i = 0; i < result.review_sections.size(); ++i) {
        if (i != 0) out.push_back(',');
        const StrategyEvalReviewSection& section = result.review_sections[i];
        out += "{\"id\":\"" + json_escape(section.id) +
               "\",\"label\":\"" + json_escape(section.label) +
               "\",\"role\":\"" + json_escape(section.role) +
               "\",\"raw_references\":{\"node_ids\":[";
        for (std::size_t node = 0; node < section.raw_node_ids.size(); ++node) {
            if (node != 0) out.push_back(',');
            out += "\"" + json_escape(section.raw_node_ids[node]) + "\"";
        }
        out += "],\"edge_ids\":[";
        for (std::size_t edge = 0; edge < section.raw_edge_ids.size(); ++edge) {
            if (edge != 0) out.push_back(',');
            out += "\"" + json_escape(section.raw_edge_ids[edge]) + "\"";
        }
        out += "]},\"per_invocation\":";
        append_cost_totals_json(
            out, section.expected_actions, section.known_expected_cost,
            section.cost_complete, section.total_expected_cost);
        out += ",\"expected_edge_traversals\":";
        append_number(out, section.expected_edge_traversals);
        out += ",\"actions\":";
        append_action_totals_json(
            out, section.actions, section.materials);
        out += ",\"materials\":";
        append_material_totals_json(out, section.materials);
        out += ",\"techniques\":";
        append_techniques_json(out, section.techniques);
        out += ",\"success_normalized\":";
        if (result.success_normalized_enabled) {
            out += "{\"work\":";
            append_cost_totals_json(
                out, section.expected_actions, section.known_expected_cost,
                section.cost_complete, section.total_expected_cost,
                result.success_probability);
            out += ",\"expected_edge_traversals\":";
            append_number(
                out, section.expected_edge_traversals /
                         result.success_probability);
            out += ",\"actions\":";
            append_action_totals_json(
                out, section.actions, section.materials,
                result.success_probability);
            out += ",\"materials\":";
            append_material_totals_json(
                out, section.materials, result.success_probability);
            out += ",\"techniques\":";
            append_techniques_json(
                out, section.techniques, result.success_probability);
            out.push_back('}');
        } else {
            out += "null";
        }
        out.push_back('}');
    }
    out += "]}";
    out += ",\"reconciliation\":{\"action_descriptor_visits_difference\":";
    append_number(out, result.action_descriptor_visits_difference);
    out += ",\"action_descriptor_applied_difference\":";
    append_number(out, result.action_descriptor_applied_difference);
    out += ",\"node_operation_visits_difference\":";
    append_number(out, result.node_operation_visits_difference);
    out += ",\"material_quantity_differences\":{";
    std::size_t difference_index = 0;
    for (const auto& [key, difference] :
         result.material_quantity_differences) {
        if (difference_index++ != 0) out.push_back(',');
        out += "\"" + json_escape(key) + "\":";
        append_number(out, difference);
    }
    out += "},\"cost_dot_product_difference\":";
    append_number(out, result.cost_dot_product_difference);
    out += ",\"section_actions_difference\":";
    append_number(out, result.section_actions_difference);
    out += ",\"section_material_differences\":{";
    difference_index = 0;
    for (const auto& [key, difference] :
         result.section_material_differences) {
        if (difference_index++ != 0) out.push_back(',');
        out += "\"" + json_escape(key) + "\":";
        append_number(out, difference);
    }
    out += "}}}";

    out += ",\"memory\":{\"owned_bytes_estimate\":" +
           std::to_string(result.owned_bytes_estimate) +
           ",\"peak_owned_bytes_estimate\":" +
           std::to_string(result.peak_owned_bytes_estimate) +
           ",\"retained_output_owned_bytes_estimate\":" +
           std::to_string(
               result.retained_output_owned_bytes_estimate) +
           ",\"max_owned_bytes\":" +
           std::to_string(result.max_owned_bytes) +
           ",\"max_output_json_bytes\":" +
           std::to_string(result.max_output_json_bytes) + "}";
    out += ",\"reforge_resource_accounting\":";
    append_reforge_resource_accounting_json(out, result);

    out += ",\"targets\":[";
    for (std::size_t i = 0; i < result.targets.size(); ++i) {
        if (i != 0) out += ',';
        const GoalSlot& target = result.targets[i];
        if (target.family_id != kNoId) {
            out += "{\"kind\":\"family\",\"family_id\":" +
                   std::to_string(target.family_id) +
                   ",\"min_tier\":" + std::to_string(target.min_tier) +
                   '}';
        } else {
            out += "{\"kind\":\"group\",\"group_id\":" +
                   std::to_string(target.group_id) + '}';
        }
    }
    out += ']';

    out += ",\"nodes\":[";
    for (std::size_t i = 0; i < result.nodes.size(); ++i) {
        if (i != 0) out += ',';
        const StrategyEvalNode& node = result.nodes[i];
        out += "{\"id\":\"" + json_escape(node.id) +
               "\",\"expected_visits\":";
        append_number(out, node.expected_visits);
        out += ",\"classes\":[";
        for (std::size_t c = 0; c < node.classes.size(); ++c) {
            if (c != 0) out += ',';
            const StrategyEvalClass& entry = node.classes[c];
            const AbstractState& state = entry.state;
            out += "{\"share\":";
            append_number(out, entry.share);
            out += ",\"rarity\":" + std::to_string(state.rarity) +
                   ",\"prefixes\":" +
                   std::to_string(state.prefix_count) +
                   ",\"suffixes\":" +
                   std::to_string(state.suffix_count) +
                   ",\"flags\":" + std::to_string(state.flags) +
                   ",\"blocked\":" +
                   std::to_string(state.blocked_mask) + ",\"slots\":[";
            for (std::size_t slot = 0; slot < result.targets.size(); ++slot) {
                if (slot != 0) out += ',';
                out += std::to_string(state.slot_status[slot]);
            }
            out += "]}";
        }
        out += "],\"classes_truncated_share\":";
        append_number(out, node.classes_truncated_share);
        out += '}';
    }
    out += ']';

    out += ",\"edges\":[";
    for (std::size_t i = 0; i < result.edges.size(); ++i) {
        if (i != 0) out += ',';
        const StrategyEvalEdge& edge = result.edges[i];
        out += "{\"id\":\"" + json_escape(edge.id) +
               "\",\"expected_traversals\":";
        append_number(out, edge.expected_traversals);
        out += '}';
    }
    out += "]}";
    return std::move(out).take();
}

} // namespace solver
} // namespace poecraft
