#include "solver_refinement_graph_routing.hpp"

namespace poecraft {
namespace solver {
namespace refinement {

RefinementResult refine_policy_exact(
        PolicyRefinementOracle& oracle,
        RefinementRequest request) {
    RefinementResult result;
    std::sort(request.coarse_roots.begin(), request.coarse_roots.end());
    request.coarse_roots.erase(
        std::unique(
            request.coarse_roots.begin(), request.coarse_roots.end()),
        request.coarse_roots.end());
    if (request.coarse_roots.empty()) {
        fail(
            result, RefinementStatus::EmptyRequest,
            "policy refinement has no coarse roots");
        return result;
    }
    Graph graph;
    if (!discover_policy_graph(oracle, graph, request, result)) {
        return result;
    }
    const std::uint64_t adapter_memory =
        oracle.estimated_owned_bytes();
    if (!check_refinement_memory(
            result, graph, request.limits, adapter_memory,
            estimate_graph_canonicalization_scratch(graph))) {
        return result;
    }
    canonicalize_graph(graph);
    if (!check_refinement_memory(
            result, graph, request.limits, adapter_memory)) {
        return result;
    }
    if (!refine_selected_action_routing(
            graph, request.limits, result, adapter_memory)) {
        return result;
    }
    if (result.telemetry.selected_action_routing_rounds >=
        request.limits.max_refinement_rounds) {
        refinement_round_cap(
            result,
            "selected-action routing and policy observation reached "
            "max_refinement_rounds");
        return result;
    }
    RefinementLimits observation_limits = request.limits;
    observation_limits.max_refinement_rounds -=
        result.telemetry.selected_action_routing_rounds;
    if (!propagate_observations(
            graph, observation_limits, result, adapter_memory)) {
        return result;
    }
    if (!check_refinement_memory(
            result, graph, request.limits, adapter_memory)) {
        return result;
    }

    std::vector<ClosedPartitionNode> closed_nodes;
    closed_nodes.reserve(graph.nodes.size());
    std::uint64_t closed_nodes_memory = saturated_product(
        closed_nodes.capacity(), sizeof(ClosedPartitionNode));
    if (!check_refinement_memory(
            result, graph, request.limits, adapter_memory,
            closed_nodes_memory)) {
        return result;
    }
    std::map<std::uint64_t, std::vector<std::uint32_t>>
        observation_authorities;
    std::map<std::uint64_t, std::uint32_t>
        immediate_authorities;
    std::uint32_t closed_nodes_since_audit = 0;
    for (std::uint32_t node_index = 0;
         node_index < graph.nodes.size(); ++node_index) {
        Node& node = graph.nodes[node_index];
        ClosedPartitionNode closed;
        /*
         * canonicalize_graph() has already ordered the exact collision-free
         * keys and remapped every absolute edge. The closed partition uses
         * its stable key only for deterministic ordering and returned member
         * labels, which this caller discards, so retain the canonical id
         * instead of a second full copy of every exact identity. Semantic
         * observation and immediate behavior remain in their own keys.
         */
        closed.stable_key = {
            0x706372636e6f6431ull, /* "pcrcnod1" */
            node_index};
        /*
         * Observation propagation is now at its fixed point.  The shared
         * contract says that only this canonical projection may influence
         * the closed partition or its published class, so the wider strict
         * feature payload is dead after the projection is materialized.
         * Retain the projected signature in the graph node for class output
         * and counterexamples, and release the unobserved carrier payload
         * before the partition's canonicalization overlap.
         */
        FeatureSignature observed =
            observe_features(node.state.features, node.required);
        closed.observation_key =
            initial_behavior_key(graph, node, observed);
        const std::uint64_t exact_feature_memory =
            estimate_feature_bytes(node.state.features);
        const std::uint64_t observed_feature_memory =
            estimate_feature_bytes(observed);
        node.state.features = std::move(observed);
        if (exact_feature_memory > observed_feature_memory) {
            note_graph_memory_release(
                graph,
                exact_feature_memory - observed_feature_memory);
        } else if (observed_feature_memory > exact_feature_memory) {
            note_graph_memory_growth(
                graph,
                observed_feature_memory - exact_feature_memory);
        }
        const std::uint64_t observation_hash =
            stable_key_hash(closed.observation_key);
        auto& observation_candidates =
            observation_authorities[observation_hash];
        for (const std::uint32_t authority :
             observation_candidates) {
            if (closed_nodes[authority].observation_key ==
                closed.observation_key) {
                closed.observation_source = authority;
                StableKey{}.swap(closed.observation_key);
                break;
            }
        }
        if (!closed.observation_source.has_value()) {
            observation_candidates.push_back(node_index);
        }
        if (node.selection != nullptr) {
            closed.immediate_key = {
                std::bit_cast<std::uint64_t>(node.action_cost)};
            const std::uint64_t immediate =
                closed.immediate_key.front();
            const auto [authority, inserted] =
                immediate_authorities.emplace(immediate, node_index);
            if (!inserted) {
                closed.immediate_source = authority->second;
                StableKey{}.swap(closed.immediate_key);
            }
        }
        closed.terminal = node.state.terminal;
        if (node.edge_source != kNoEdgeSource) {
            closed.arc_source = node.edge_source;
        } else {
            closed.arcs.reserve(node.edges.size());
            for (const NodeEdge& edge : node.edges) {
                closed.arcs.push_back({
                    {},
                    std::optional<std::uint32_t>{edge.successor},
                    edge.probability.value()});
            }
        }
        closed_nodes.push_back(std::move(closed));
        add_bytes(
            closed_nodes_memory,
            estimate_closed_node_nested_memory(
                closed_nodes.back()));
        ++closed_nodes_since_audit;
        if (closed_nodes_since_audit == 512 ||
            !closed_nodes.back().arcs.empty() ||
            closed_nodes.size() == graph.nodes.size()) {
            std::uint64_t construction_memory =
                closed_nodes_memory;
            add_bytes(
                construction_memory,
                estimate_ordered_nodes(observation_authorities));
            for (const auto& [unused, candidates] :
                 observation_authorities) {
                (void)unused;
                add_bytes(
                    construction_memory,
                    saturated_product(
                        candidates.capacity(),
                        sizeof(std::uint32_t)));
            }
            add_bytes(
                construction_memory,
                estimate_ordered_nodes(immediate_authorities));
            if (!check_refinement_memory(
                    result, graph, request.limits, adapter_memory,
                    construction_memory)) {
                return result;
            }
            closed_nodes_since_audit = 0;
        }
    }
    decltype(observation_authorities){}.swap(
        observation_authorities);
    decltype(immediate_authorities){}.swap(
        immediate_authorities);
    ClosedPartitionLimits partition_limits;
    partition_limits.max_classes =
        request.limits.max_refinement_classes;
    const std::uint64_t pre_partition_rounds =
        static_cast<std::uint64_t>(
            result.telemetry.selected_action_routing_rounds) +
        result.telemetry.observation_propagation_rounds;
    if (pre_partition_rounds >=
        request.limits.max_refinement_rounds) {
        refinement_round_cap(
            result,
            "policy observation and kernel refinement reached "
            "max_refinement_rounds");
        return result;
    }
    partition_limits.max_rounds =
        request.limits.max_refinement_rounds -
        static_cast<std::uint32_t>(pre_partition_rounds);
    partition_limits.retained_estimated_memory_bytes =
        saturated_add(
            saturated_add(
                estimate_graph_memory(graph), adapter_memory),
            estimate_refinement_result_memory(result));
    partition_limits.max_estimated_memory_bytes =
        request.limits.max_estimated_memory_bytes;
    partition_limits.probability_sum_tolerance =
        request.limits.probability_sum_tolerance;
    ClosedPartitionResult partitioned =
        refine_closed_probabilistic_partition(
            std::move(closed_nodes), partition_limits);
    result.telemetry.partition_refinement_rounds =
        partitioned.rounds;
    result.telemetry.initial_observation_classes =
        partitioned.initial_class_count;
    result.telemetry.final_refinement_classes =
        partitioned.final_class_count;
    result.telemetry.lumpability_checks =
        partitioned.lumpability_checks;
    result.telemetry.estimated_memory_bytes =
        partitioned.estimated_memory_bytes;
    result.telemetry.peak_estimated_memory_bytes = std::max(
        result.telemetry.peak_estimated_memory_bytes,
        partitioned.peak_estimated_memory_bytes);
    if (partitioned.status != ClosedPartitionStatus::Complete) {
        if (partitioned.status ==
            ClosedPartitionStatus::ResourceCap) {
            if (partitioned.resource_cap ==
                "max_estimated_memory_bytes") {
                cap(
                    result, "max_estimated_memory_bytes",
                    "policy refinement reached "
                    "max_estimated_memory_bytes");
            } else {
                cap(
                    result, "max_refinement_classes",
                    "policy refinement reached "
                    "max_refinement_classes");
            }
        } else if (
            partitioned.status ==
            ClosedPartitionStatus::RefinementRoundCap) {
            refinement_round_cap(
                result,
                "kernel refinement reached max_refinement_rounds");
        } else if (
            partitioned.status ==
            ClosedPartitionStatus::NonLumpable) {
            fail(
                result, RefinementStatus::NonLumpable,
                partitioned.failure_reason);
        } else {
            fail(
                result, RefinementStatus::InvalidKernel,
                partitioned.failure_reason);
        }
        return result;
    }
    std::vector<std::uint32_t> initial =
        std::move(partitioned.initial_class_by_node);
    std::vector<std::uint32_t> partition =
        std::move(partitioned.class_by_node);
    const std::uint32_t initial_count =
        partitioned.initial_class_count;
    const std::uint32_t class_count =
        partitioned.final_class_count;
    partitioned.classes = {};
    partitioned.initial_class_by_node = {};
    partitioned.class_by_node = {};
    result.telemetry.final_refinement_classes = class_count;
    result.telemetry.behavior_splits = class_count - initial_count;
    result.telemetry.merged_exact_states =
        static_cast<std::uint32_t>(graph.nodes.size()) - class_count;
    std::uint64_t retained_scratch =
        saturated_product(
            initial.capacity(), sizeof(std::uint32_t));
    add_bytes(
        retained_scratch,
        saturated_product(
            partition.capacity(), sizeof(std::uint32_t)));
    add_bytes(
        retained_scratch,
        estimate_closed_partition_result_memory(partitioned));
    if (!collect_counterexamples(
            graph, initial, partition, request.limits, result,
            adapter_memory, retained_scratch)) {
        return result;
    }
    if (!build_classes(
            graph, partition, class_count, request.limits, result,
            adapter_memory, retained_scratch)) {
        return result;
    }
    if (!check_refinement_memory(
            result, graph, request.limits, adapter_memory,
            retained_scratch)) {
        return result;
    }
    result.status = RefinementStatus::Complete;
    result.executable = true;
    result.lumpable = true;
    return result;
}

} // namespace refinement
} // namespace solver
} // namespace poecraft
