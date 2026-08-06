#pragma once

#include "solver_refinement_partition_helpers.hpp"

namespace poecraft {
namespace solver {
namespace refinement {
namespace {

struct NodeEdge {
    std::uint32_t successor = 0;
    DeterministicSum probability;
};

constexpr std::uint32_t kNoEdgeSource =
    std::numeric_limits<std::uint32_t>::max();

struct Node {
    ExactState state;
    bool expanded = false;
    std::uint32_t edge_source = kNoEdgeSource;
    std::shared_ptr<SelectedAction> selection;
    double action_cost = 0.0;
    std::vector<NodeEdge> edges;
    ObservationRequirement required;
};

struct ActionKey {
    std::uint32_t action_id = 0;
    StableKey semantic_key;

    bool operator<(const ActionKey& other) const {
        return std::tie(action_id, semantic_key) <
               std::tie(other.action_id, other.semantic_key);
    }
};

struct Graph {
    std::vector<Node> nodes;
    /*
     * The exact key remains authoritative in Node. This compact lookup owns
     * only hash buckets of graph ids and verifies the full key on every hit,
     * so collisions cannot merge carriers and the graph does not retain a
     * second copy of every large collision-free state identity.
     */
    std::map<std::uint64_t, std::vector<std::uint32_t>>
        indices_by_key_hash;
    std::map<StableKey, std::uint32_t> kernel_source_by_reuse_key;
    std::map<std::uint32_t, StableKey> coarse_key_by_state;
    std::map<StableKey, std::uint32_t> coarse_state_by_key;
    std::map<ActionKey, StableKey> contract_signature_by_action;
    std::map<StableKey, std::shared_ptr<SelectedAction>>
        selection_by_signature;
    mutable bool memory_cache_initialized = false;
    mutable std::uint32_t memory_checks_since_audit = 0;
    mutable std::uint64_t audited_memory_bytes = 0;
    mutable std::uint64_t projected_growth_bytes = 0;
};

struct PendingExactLess {
    const Graph* graph = nullptr;

    bool operator()(
            const std::uint32_t left,
            const std::uint32_t right) const {
        if (left == right) return false;
        const StableKey& left_key =
            graph->nodes.at(left).state.stable_key;
        const StableKey& right_key =
            graph->nodes.at(right).state.stable_key;
        if (left_key != right_key) return left_key < right_key;
        return left < right;
    }
};

struct PendingExact {
    Graph* graph = nullptr;
    std::vector<std::uint32_t> entries;
    std::vector<bool> scheduled;

    explicit PendingExact(Graph* graph_value)
        : graph(graph_value) {}

    bool empty() const { return entries.empty(); }
    std::size_t size() const { return entries.size(); }
    std::size_t capacity() const { return entries.capacity(); }
    std::size_t scheduled_capacity() const {
        return scheduled.capacity();
    }

    void release_storage() {
        std::vector<std::uint32_t>{}.swap(entries);
        std::vector<bool>{}.swap(scheduled);
    }

    void insert(const std::uint32_t index) {
        (void)graph->nodes.at(index);
        if (scheduled.size() <= index) {
            scheduled.resize(
                static_cast<std::size_t>(index) + 1, false);
        }
        if (scheduled[index]) return;
        scheduled[index] = true;
        entries.push_back(index);
        const auto priority_less = [&](const std::uint32_t left,
                                       const std::uint32_t right) {
            return PendingExactLess{graph}(right, left);
        };
        std::push_heap(
            entries.begin(), entries.end(), priority_less);
    }

    std::uint32_t pop_next() {
        const auto priority_less = [&](const std::uint32_t left,
                                       const std::uint32_t right) {
            return PendingExactLess{graph}(right, left);
        };
        std::pop_heap(
            entries.begin(), entries.end(), priority_less);
        const std::uint32_t next = entries.back();
        entries.pop_back();
        scheduled[next] = false;
        /* A broad shared row can seed hundreds of thousands of ids, then
         * consume them without adding successors. Do not retain the heap's
         * high-water allocation through every late publication audit. */
        if (entries.capacity() > 16384 &&
            entries.size() <= entries.capacity() / 2) {
            std::vector<std::uint32_t> compact(entries);
            entries.swap(compact);
        }
        return next;
    }
};

const std::vector<NodeEdge>& node_edges(
        const Graph& graph,
        const Node& node) {
    return node.edge_source == kNoEdgeSource
               ? node.edges
               : graph.nodes.at(node.edge_source).edges;
}

const StableKey& node_coarse_key(
        const Graph& graph,
        const Node& node) {
    return graph.coarse_key_by_state.at(node.state.coarse_state);
}

bool same_stored_exact_state(
        const ExactState& stored,
        const ExactState& incoming) {
    return stored.stable_key == incoming.stable_key &&
           stored.coarse_state == incoming.coarse_state &&
           stored.features == incoming.features &&
           stored.goal == incoming.goal &&
           stored.terminal == incoming.terminal;
}

std::uint64_t saturated_add(
        const std::uint64_t left,
        const std::uint64_t right) {
    return right >
                   std::numeric_limits<std::uint64_t>::max() - left
               ? std::numeric_limits<std::uint64_t>::max()
               : left + right;
}

std::uint64_t saturated_product(
        const std::size_t count,
        const std::size_t element_size) {
    if (element_size != 0 &&
        count >
            std::numeric_limits<std::uint64_t>::max() /
                element_size) {
        return std::numeric_limits<std::uint64_t>::max();
    }
    return static_cast<std::uint64_t>(count) *
           static_cast<std::uint64_t>(element_size);
}

void add_bytes(
        std::uint64_t& total,
        const std::uint64_t addition) {
    total = saturated_add(total, addition);
}

std::uint64_t estimate_stable_key_bytes(
        const StableKey& key) {
    return saturated_product(
        key.capacity(), sizeof(std::uint64_t));
}

std::uint64_t stable_key_hash(const StableKey& key) {
    std::uint64_t hash = 1469598103934665603ull;
    for (const std::uint64_t token : key) {
        for (std::uint32_t shift = 0; shift < 64; shift += 8) {
            hash ^= (token >> shift) & 0xffu;
            hash *= 1099511628211ull;
        }
    }
    return hash;
}

std::uint64_t estimate_selector_bytes(
        const RefinementAffixSelector& selector) {
    return saturated_product(
        selector.required_tag_ids.capacity(),
        sizeof(std::uint32_t));
}

std::uint64_t estimate_feature_bytes(
        const FeatureSignature& signature) {
    std::uint64_t bytes = saturated_product(
        signature.capacity(), sizeof(FeatureAtom));
    for (const FeatureAtom& atom : signature) {
        add_bytes(bytes, estimate_stable_key_bytes(atom.value));
        add_bytes(
            bytes,
            saturated_product(
                atom.modifier_tag_ids.capacity(),
                sizeof(std::uint32_t)));
    }
    return bytes;
}

std::uint64_t estimate_requirement_bytes(
        const ObservationRequirement& requirement) {
    std::uint64_t bytes = saturated_product(
        requirement.modifier_tag_ids.capacity(),
        sizeof(std::uint32_t));
    add_bytes(
        bytes,
        saturated_product(
            requirement.affix_observations.capacity(),
            sizeof(RefinementAffixObservation)));
    for (const RefinementAffixObservation& observation :
         requirement.affix_observations) {
        add_bytes(
            bytes,
            estimate_selector_bytes(observation.selector));
    }
    return bytes;
}

std::uint64_t estimate_contract_bytes(
        const ActionRefinementContract& contract) {
    std::uint64_t bytes = saturated_product(
        contract.observed_modifier_tag_ids.capacity(),
        sizeof(std::uint32_t));
    add_bytes(
        bytes,
        saturated_product(
            contract.affix_observations.capacity(),
            sizeof(RefinementAffixObservation)));
    for (const RefinementAffixObservation& observation :
         contract.affix_observations) {
        add_bytes(
            bytes,
            estimate_selector_bytes(observation.selector));
    }
    const auto add_selectors =
        [&](const std::vector<RefinementAffixSelector>& selectors) {
            add_bytes(
                bytes,
                saturated_product(
                    selectors.capacity(),
                    sizeof(RefinementAffixSelector)));
            for (const RefinementAffixSelector& selector : selectors) {
                add_bytes(bytes, estimate_selector_bytes(selector));
            }
    };
    add_bytes(
        bytes,
        saturated_product(
            contract.item_affix_dependencies.capacity(),
            sizeof(RefinementItemAffixDependency)));
    add_bytes(
        bytes,
        saturated_product(
            contract.affix_flows.capacity(),
            sizeof(RefinementAffixFlow)));
    for (const RefinementAffixFlow& flow :
         contract.affix_flows) {
        add_bytes(
            bytes,
            estimate_selector_bytes(flow.source_selector));
    }
    add_selectors(contract.preserved_affixes);
    add_selectors(contract.destroyed_affixes);
    return bytes;
}

std::uint64_t estimate_selected_action_bytes(
        const SelectedAction& selected) {
    std::uint64_t bytes =
        estimate_stable_key_bytes(selected.semantic_key);
    add_bytes(bytes, estimate_contract_bytes(selected.contract));
    add_bytes(
        bytes,
        saturated_product(
            selected.ordered_program.capacity(),
            sizeof(ActionRefinementContract)));
    for (const ActionRefinementContract& contract :
         selected.ordered_program) {
        add_bytes(bytes, estimate_contract_bytes(contract));
    }
    add_bytes(
        bytes,
        saturated_product(
            selected.execution_paths.capacity(),
            sizeof(std::vector<ActionRefinementContract>)));
    for (const std::vector<ActionRefinementContract>& path :
         selected.execution_paths) {
        add_bytes(
            bytes,
            saturated_product(
                path.capacity(),
                sizeof(ActionRefinementContract)));
        for (const ActionRefinementContract& contract : path) {
            add_bytes(bytes, estimate_contract_bytes(contract));
        }
    }
    add_bytes(
        bytes,
        estimate_requirement_bytes(selected.routing_observes));
    return bytes;
}

std::uint64_t estimate_exact_state_bytes(
        const ExactState& state) {
    std::uint64_t bytes =
        estimate_stable_key_bytes(state.stable_key);
    add_bytes(
        bytes,
        estimate_stable_key_bytes(state.coarse_state_key));
    add_bytes(bytes, estimate_feature_bytes(state.features));
    return bytes;
}

std::uint64_t estimate_exact_kernel_bytes(
        const ExactActionKernel& kernel) {
    std::uint64_t bytes = saturated_product(
        kernel.transitions.capacity(), sizeof(ExactTransition));
    for (const ExactTransition& transition : kernel.transitions) {
        add_bytes(
            bytes,
            estimate_exact_state_bytes(transition.successor));
    }
    return bytes;
}

template <typename OrderedContainer>
std::uint64_t estimate_ordered_nodes(
        const OrderedContainer& container) {
    return saturated_product(
        container.size(),
        sizeof(typename OrderedContainer::value_type) +
            3 * sizeof(void*));
}

std::uint64_t estimate_graph_memory(const Graph& graph) {
    std::uint64_t bytes = saturated_product(
        graph.nodes.capacity(), sizeof(Node));
    std::set<const SelectedAction*> counted_selections;
    for (const Node& node : graph.nodes) {
        add_bytes(bytes, estimate_exact_state_bytes(node.state));
        add_bytes(
            bytes,
            saturated_product(
                node.edges.capacity(), sizeof(NodeEdge)));
        add_bytes(bytes, estimate_requirement_bytes(node.required));
        if (node.selection != nullptr &&
            counted_selections.insert(node.selection.get()).second) {
            add_bytes(
                bytes,
                sizeof(SelectedAction) + 2 * sizeof(void*) +
                    estimate_selected_action_bytes(*node.selection));
        }
    }
    add_bytes(
        bytes,
        estimate_ordered_nodes(graph.indices_by_key_hash));
    for (const auto& [unused, indices] :
         graph.indices_by_key_hash) {
        (void)unused;
        add_bytes(
            bytes,
            saturated_product(
                indices.capacity(), sizeof(std::uint32_t)));
    }
    add_bytes(
        bytes,
        estimate_ordered_nodes(graph.kernel_source_by_reuse_key));
    for (const auto& [key, unused] :
         graph.kernel_source_by_reuse_key) {
        (void)unused;
        add_bytes(bytes, estimate_stable_key_bytes(key));
    }
    add_bytes(
        bytes,
        estimate_ordered_nodes(graph.coarse_key_by_state));
    for (const auto& [unused, key] :
         graph.coarse_key_by_state) {
        (void)unused;
        add_bytes(bytes, estimate_stable_key_bytes(key));
    }
    add_bytes(
        bytes,
        estimate_ordered_nodes(graph.coarse_state_by_key));
    for (const auto& [key, unused] :
         graph.coarse_state_by_key) {
        (void)unused;
        add_bytes(bytes, estimate_stable_key_bytes(key));
    }
    add_bytes(
        bytes,
        estimate_ordered_nodes(
            graph.contract_signature_by_action));
    for (const auto& [key, signature] :
         graph.contract_signature_by_action) {
        add_bytes(
            bytes,
            estimate_stable_key_bytes(key.semantic_key));
        add_bytes(bytes, estimate_stable_key_bytes(signature));
    }
    add_bytes(
        bytes,
        estimate_ordered_nodes(graph.selection_by_signature));
    for (const auto& [key, unused] :
         graph.selection_by_signature) {
        (void)unused;
        add_bytes(bytes, estimate_stable_key_bytes(key));
    }
    return bytes;
}

void note_graph_memory_growth(
        Graph& graph,
        const std::uint64_t bytes) {
    add_bytes(graph.projected_growth_bytes, bytes);
}

void note_graph_memory_release(
        Graph& graph,
        const std::uint64_t bytes) {
    if (!graph.memory_cache_initialized) return;
    const std::uint64_t projected_release = std::min(
        graph.projected_growth_bytes, bytes);
    graph.projected_growth_bytes -= projected_release;
    const std::uint64_t audited_release = bytes - projected_release;
    graph.audited_memory_bytes =
        audited_release > graph.audited_memory_bytes
            ? 0
            : graph.audited_memory_bytes - audited_release;
}

void invalidate_graph_memory_cache(Graph& graph) {
    graph.memory_cache_initialized = false;
    graph.memory_checks_since_audit = 0;
    graph.projected_growth_bytes = 0;
}

std::uint64_t fast_estimate_graph_memory(const Graph& graph) {
    constexpr std::uint32_t kAuditInterval = 128;
    if (!graph.memory_cache_initialized ||
        graph.memory_checks_since_audit >= kAuditInterval) {
        graph.audited_memory_bytes = estimate_graph_memory(graph);
        graph.projected_growth_bytes = 0;
        graph.memory_checks_since_audit = 0;
        graph.memory_cache_initialized = true;
        return graph.audited_memory_bytes;
    }
    ++graph.memory_checks_since_audit;
    return saturated_add(
        graph.audited_memory_bytes,
        graph.projected_growth_bytes);
}

std::uint64_t estimate_refinement_result_memory(
        const RefinementResult& result) {
    std::uint64_t bytes =
        saturated_add(
            result.failure_reason.capacity() + 1,
            result.resource_cap.capacity() + 1);
    add_bytes(
        bytes,
        saturated_product(
            result.assignments.capacity(),
            sizeof(StateClassAssignment)));
    add_bytes(
        bytes,
        saturated_product(
            result.classes.capacity(),
            sizeof(RefinedPolicyClass)));
    for (const RefinedPolicyClass& policy : result.classes) {
        add_bytes(
            bytes,
            estimate_stable_key_bytes(
                policy.coarse_state_key));
        add_bytes(
            bytes,
            saturated_product(
                policy.exact_members.capacity(),
                sizeof(StableKey)));
        for (const StableKey& member : policy.exact_members) {
            add_bytes(bytes, estimate_stable_key_bytes(member));
        }
        add_bytes(
            bytes,
            estimate_requirement_bytes(
                policy.required_observations));
        add_bytes(
            bytes,
            estimate_feature_bytes(
                policy.observation_signature));
        if (policy.selected_action.has_value()) {
            add_bytes(
                bytes,
                estimate_selected_action_bytes(
                    *policy.selected_action));
        }
        add_bytes(
            bytes,
            saturated_product(
                policy.transitions.capacity(),
                sizeof(ProjectedTransition)));
    }
    add_bytes(
        bytes,
        saturated_product(
            result.counterexamples.capacity(),
            sizeof(RefinementCounterexample)));
    for (const RefinementCounterexample& witness :
         result.counterexamples) {
        add_bytes(
            bytes,
            estimate_stable_key_bytes(witness.left_state));
        add_bytes(
            bytes,
            estimate_stable_key_bytes(witness.right_state));
        add_bytes(
            bytes,
            estimate_feature_bytes(witness.differing_features));
    }
    return bytes;
}

std::uint64_t estimate_refined_policy_class_memory(
        const RefinedPolicyClass& policy) {
    std::uint64_t bytes = saturated_product(
        policy.exact_members.capacity(), sizeof(StableKey));
    for (const StableKey& member : policy.exact_members) {
        add_bytes(bytes, estimate_stable_key_bytes(member));
    }
    add_bytes(
        bytes,
        estimate_requirement_bytes(policy.required_observations));
    add_bytes(
        bytes,
        estimate_feature_bytes(policy.observation_signature));
    if (policy.selected_action.has_value()) {
        add_bytes(
            bytes,
            estimate_selected_action_bytes(
                *policy.selected_action));
    }
    add_bytes(
        bytes,
        saturated_product(
            policy.transitions.capacity(),
            sizeof(ProjectedTransition)));
    return bytes;
}

std::uint64_t estimate_project_kernel_scratch(
        const Graph& graph,
        const Node& node) {
    using ProjectionMap =
        std::map<std::uint32_t, DeterministicSum>;
    const std::vector<NodeEdge>& edges = node_edges(graph, node);
    std::uint64_t bytes = saturated_product(
        edges.size(),
        sizeof(typename ProjectionMap::value_type) +
            3 * sizeof(void*));
    add_bytes(
        bytes,
        saturated_product(
            edges.size(),
            sizeof(std::pair<std::uint32_t, DeterministicSum>)));
    return bytes;
}

std::uint64_t estimate_exact_states_memory(
        const std::vector<ExactState>& states) {
    std::uint64_t bytes = saturated_product(
        states.capacity(), sizeof(ExactState));
    for (const ExactState& state : states) {
        add_bytes(bytes, estimate_exact_state_bytes(state));
    }
    return bytes;
}

std::uint64_t estimate_node_edges_memory(
        const std::vector<NodeEdge>& edges) {
    return saturated_product(
        edges.capacity(), sizeof(NodeEdge));
}

std::uint64_t estimate_discovery_worklists_memory(
        const PendingExact& pending_exact,
        const std::set<std::uint32_t>& reachable_coarse) {
    std::uint64_t bytes =
        saturated_product(
            pending_exact.capacity(), sizeof(std::uint32_t));
    add_bytes(
        bytes,
        saturated_add(
            pending_exact.scheduled_capacity(), 7) / 8);
    add_bytes(bytes, estimate_ordered_nodes(reachable_coarse));
    return bytes;
}

std::uint64_t estimate_graph_canonicalization_scratch(
        const Graph& graph) {
    std::uint64_t bytes = saturated_product(
        graph.nodes.size(),
        2 * sizeof(std::uint32_t));
    return bytes;
}

std::uint64_t estimate_policy_observation_nodes_memory(
        const std::vector<PolicyObservationNode>& nodes) {
    std::uint64_t bytes = saturated_product(
        nodes.capacity(), sizeof(PolicyObservationNode));
    for (const PolicyObservationNode& node : nodes) {
        if (node.selected_action.has_value()) {
            add_bytes(
                bytes,
                estimate_selected_action_bytes(
                    *node.selected_action));
        }
        add_bytes(
            bytes,
            estimate_requirement_bytes(node.direct_observes));
        add_bytes(
            bytes,
            saturated_product(
                node.successors.capacity(),
                sizeof(std::uint32_t)));
    }
    return bytes;
}

std::uint64_t estimate_policy_observation_node_nested_memory(
        const PolicyObservationNode& node) {
    std::uint64_t bytes = 0;
    if (node.selected_action.has_value()) {
        add_bytes(
            bytes,
            estimate_selected_action_bytes(*node.selected_action));
    }
    add_bytes(
        bytes,
        estimate_requirement_bytes(node.direct_observes));
    add_bytes(
        bytes,
        saturated_product(
            node.successors.capacity(), sizeof(std::uint32_t)));
    return bytes;
}

std::uint64_t estimate_policy_observation_fixed_memory(
        const PolicyObservationFixedPoint& fixed) {
    std::uint64_t bytes = fixed.failure_reason.capacity() + 1;
    add_bytes(
        bytes,
        saturated_product(
            fixed.assignments.capacity(),
            sizeof(PolicyObservationAssignment)));
    for (const PolicyObservationAssignment& assignment :
         fixed.assignments) {
        add_bytes(
            bytes,
            estimate_requirement_bytes(assignment.required));
    }
    return bytes;
}

std::uint64_t estimate_closed_nodes_memory(
        const std::vector<ClosedPartitionNode>& nodes) {
    std::uint64_t bytes = saturated_product(
        nodes.capacity(), sizeof(ClosedPartitionNode));
    for (const ClosedPartitionNode& node : nodes) {
        add_bytes(bytes, estimate_stable_key_bytes(node.stable_key));
        add_bytes(
            bytes,
            estimate_stable_key_bytes(node.observation_key));
        add_bytes(
            bytes,
            estimate_stable_key_bytes(node.immediate_key));
        add_bytes(
            bytes,
            saturated_product(
                node.arcs.capacity(), sizeof(ClosedPartitionArc)));
        for (const ClosedPartitionArc& arc : node.arcs) {
            add_bytes(bytes, estimate_stable_key_bytes(arc.label));
        }
    }
    return bytes;
}

std::uint64_t estimate_closed_node_nested_memory(
        const ClosedPartitionNode& node) {
    std::uint64_t bytes =
        estimate_stable_key_bytes(node.stable_key);
    add_bytes(
        bytes, estimate_stable_key_bytes(node.observation_key));
    add_bytes(
        bytes, estimate_stable_key_bytes(node.immediate_key));
    add_bytes(
        bytes,
        saturated_product(
            node.arcs.capacity(), sizeof(ClosedPartitionArc)));
    for (const ClosedPartitionArc& arc : node.arcs) {
        add_bytes(bytes, estimate_stable_key_bytes(arc.label));
    }
    return bytes;
}

std::uint64_t estimate_canonical_closed_nodes_memory(
        const std::vector<CanonicalClosedNode>& nodes) {
    std::uint64_t bytes = saturated_product(
        nodes.capacity(), sizeof(CanonicalClosedNode));
    for (const CanonicalClosedNode& node : nodes) {
        add_bytes(bytes, estimate_stable_key_bytes(node.stable_key));
        add_bytes(
            bytes,
            estimate_stable_key_bytes(node.observation_key));
        add_bytes(
            bytes,
            estimate_stable_key_bytes(node.immediate_key));
        add_bytes(
            bytes,
            saturated_product(
                node.arcs.capacity(), sizeof(CanonicalClosedArc)));
        for (const CanonicalClosedArc& arc : node.arcs) {
            add_bytes(bytes, estimate_stable_key_bytes(arc.label));
        }
    }
    return bytes;
}

std::uint64_t estimate_closed_partition_result_memory(
        const ClosedPartitionResult& result) {
    std::uint64_t bytes =
        saturated_add(
            result.failure_reason.capacity() + 1,
            result.resource_cap.capacity() + 1);
    add_bytes(
        bytes,
        saturated_product(
            result.initial_class_by_node.capacity(),
            sizeof(std::uint32_t)));
    add_bytes(
        bytes,
        saturated_product(
            result.class_by_node.capacity(),
            sizeof(std::uint32_t)));
    add_bytes(
        bytes,
        saturated_product(
            result.classes.capacity(),
            sizeof(ClosedPartitionClass)));
    for (const ClosedPartitionClass& partition_class :
         result.classes) {
        add_bytes(
            bytes,
            saturated_product(
                partition_class.member_keys.capacity(),
                sizeof(StableKey)));
        for (const StableKey& key : partition_class.member_keys) {
            add_bytes(bytes, estimate_stable_key_bytes(key));
        }
        add_bytes(
            bytes,
            estimate_stable_key_bytes(
                partition_class.observation_key));
        add_bytes(
            bytes,
            estimate_stable_key_bytes(
                partition_class.immediate_key));
        add_bytes(
            bytes,
            saturated_product(
                partition_class.arcs.capacity(),
                sizeof(ClosedPartitionProjectedArc)));
        for (const ClosedPartitionProjectedArc& arc :
             partition_class.arcs) {
            add_bytes(bytes, estimate_stable_key_bytes(arc.label));
        }
    }
    return bytes;
}

std::uint64_t estimate_closed_partition_class_memory(
        const ClosedPartitionClass& partition_class) {
    std::uint64_t bytes = saturated_product(
        partition_class.member_keys.capacity(),
        sizeof(StableKey));
    for (const StableKey& key : partition_class.member_keys) {
        add_bytes(bytes, estimate_stable_key_bytes(key));
    }
    add_bytes(
        bytes,
        estimate_stable_key_bytes(
            partition_class.observation_key));
    add_bytes(
        bytes,
        estimate_stable_key_bytes(
            partition_class.immediate_key));
    add_bytes(
        bytes,
        saturated_product(
            partition_class.arcs.capacity(),
            sizeof(ClosedPartitionProjectedArc)));
    for (const ClosedPartitionProjectedArc& arc :
         partition_class.arcs) {
        add_bytes(bytes, estimate_stable_key_bytes(arc.label));
    }
    return bytes;
}

std::uint64_t estimate_closed_members_memory(
        const std::vector<std::vector<std::uint32_t>>& members) {
    std::uint64_t bytes = saturated_product(
        members.capacity(), sizeof(std::vector<std::uint32_t>));
    for (const std::vector<std::uint32_t>& partition_class :
         members) {
        add_bytes(
            bytes,
            saturated_product(
                partition_class.capacity(),
                sizeof(std::uint32_t)));
    }
    return bytes;
}

std::uint64_t estimate_closed_projection_memory(
        const std::vector<ClosedProjectionEntry>& projection) {
    std::uint64_t bytes = saturated_product(
        projection.capacity(), sizeof(ClosedProjectionEntry));
    for (const ClosedProjectionEntry& arc : projection) {
        add_bytes(bytes, estimate_stable_key_bytes(arc.label));
    }
    return bytes;
}

std::uint64_t estimate_closed_projection_scratch(
        const std::vector<CanonicalClosedNode>& graph,
        const CanonicalClosedNode& node) {
    using ProjectionKey =
        std::pair<StableKey, std::optional<std::uint32_t>>;
    using ProjectionMap =
        std::map<ProjectionKey, DeterministicSum>;
    const std::vector<CanonicalClosedArc>& arcs =
        node.arc_source.has_value()
            ? graph.at(*node.arc_source).arcs
            : node.arcs;
    std::uint64_t bytes = saturated_product(
        arcs.size(), sizeof(ClosedProjectionEntry));
    add_bytes(
        bytes,
        saturated_product(
            arcs.size(),
            sizeof(typename ProjectionMap::value_type) +
                3 * sizeof(void*)));
    for (const CanonicalClosedArc& arc : arcs) {
        /*
         * The map key and returned projection each own a label copy while
         * project_closed_row() transfers the row out.
         */
        const std::uint64_t label_bytes =
            estimate_stable_key_bytes(arc.label);
        add_bytes(bytes, label_bytes);
        add_bytes(bytes, label_bytes);
    }
    return bytes;
}

std::uint64_t estimate_max_closed_projection_scratch(
        const std::vector<CanonicalClosedNode>& nodes) {
    std::uint64_t maximum = 0;
    for (const CanonicalClosedNode& node : nodes) {
        maximum = std::max(
            maximum,
            estimate_closed_projection_scratch(nodes, node));
    }
    return maximum;
}

std::uint64_t estimate_closed_canonicalization_scratch(
        const std::vector<ClosedPartitionNode>& nodes) {
    using ArcKey =
        std::pair<StableKey, std::optional<std::uint32_t>>;
    using GroupedMap = std::map<ArcKey, DeterministicSum>;
    std::uint64_t bytes = saturated_product(
        nodes.size(),
        sizeof(CanonicalClosedNode) +
            2 * sizeof(std::uint32_t));
    std::uint64_t largest_row = 0;
    for (const ClosedPartitionNode& node : nodes) {
        /*
         * Stable keys and labels move from the input into the canonical
         * graph. The input estimate already owns those buffers; only the new
         * vector element storage is additional across the full graph.
         */
        add_bytes(
            bytes,
            saturated_product(
                node.arcs.size(), sizeof(CanonicalClosedArc)));

        std::uint64_t row = saturated_product(
            node.arcs.size(), sizeof(ClosedPartitionArc));
        add_bytes(
            row,
            saturated_product(
                node.arcs.size(),
                sizeof(typename GroupedMap::value_type) +
                    3 * sizeof(void*)));
        for (const ClosedPartitionArc& arc : node.arcs) {
            const std::uint64_t label_bytes =
                estimate_stable_key_bytes(arc.label);
            add_bytes(row, label_bytes);
            add_bytes(row, label_bytes);
        }
        largest_row = std::max(largest_row, row);
    }
    add_bytes(bytes, largest_row);
    return bytes;
}

std::uint64_t estimate_closed_partition_map_memory(
        const std::map<ClosedPartitionKey, std::uint32_t>& classes) {
    std::uint64_t bytes = estimate_ordered_nodes(classes);
    for (const auto& [key, unused] : classes) {
        (void)unused;
        add_bytes(bytes, estimate_stable_key_bytes(key));
    }
    return bytes;
}

bool check_closed_partition_memory(
        ClosedPartitionResult& result,
        const ClosedPartitionLimits& limits,
        const std::uint64_t owned_live_memory) {
    const std::uint64_t total = saturated_add(
        limits.retained_estimated_memory_bytes,
        owned_live_memory);
    result.estimated_memory_bytes = total;
    result.peak_estimated_memory_bytes = std::max(
        result.peak_estimated_memory_bytes, total);
    if (total != std::numeric_limits<std::uint64_t>::max() &&
        total <= limits.max_estimated_memory_bytes) {
        return true;
    }
    result.status = ClosedPartitionStatus::ResourceCap;
    result.lumpable = false;
    result.resource_cap = "max_estimated_memory_bytes";
    result.failure_reason =
        "closed probabilistic partition reached "
        "max_estimated_memory_bytes";
    return false;
}

template <typename Signature>
bool exact_closed_partition(
        const std::size_t node_count,
        Signature signature,
        const std::uint64_t retained_owned_memory,
        const std::uint64_t per_signature_scratch,
        const ClosedPartitionLimits& limits,
        ClosedPartitionResult& result,
        std::vector<std::uint32_t>& output,
        std::uint32_t& class_count) {
    std::map<ClosedPartitionKey, std::uint32_t> classes;
    std::uint64_t projected = retained_owned_memory;
    add_bytes(projected, per_signature_scratch);
    add_bytes(
        projected,
        saturated_product(
            node_count, sizeof(std::uint32_t)));
    if (!check_closed_partition_memory(
            result, limits, projected)) {
        return false;
    }
    std::vector<std::uint32_t> partition(node_count);
    std::uint32_t next = 0;
    std::uint64_t classes_owned_bytes = 0;
    const std::uint64_t class_map_node_bytes =
        sizeof(typename decltype(classes)::value_type) +
        3 * sizeof(void*);
    for (std::uint32_t node = 0; node < node_count; ++node) {
        ClosedPartitionKey key = signature(node);
        std::uint64_t live = retained_owned_memory;
        add_bytes(live, per_signature_scratch);
        add_bytes(
            live,
            saturated_product(
                partition.capacity(), sizeof(std::uint32_t)));
        add_bytes(live, classes_owned_bytes);
        add_bytes(live, estimate_stable_key_bytes(key));
        if (!check_closed_partition_memory(
                result, limits, live)) {
            return false;
        }
        const auto [it, inserted] =
            classes.emplace(std::move(key), next);
        if (inserted) {
            ++next;
            add_bytes(classes_owned_bytes, class_map_node_bytes);
            add_bytes(
                classes_owned_bytes,
                estimate_stable_key_bytes(it->first));
        }
        partition[node] = it->second;
        live = retained_owned_memory;
        add_bytes(live, per_signature_scratch);
        add_bytes(
            live,
            saturated_product(
                partition.capacity(), sizeof(std::uint32_t)));
        add_bytes(live, classes_owned_bytes);
        if (!check_closed_partition_memory(
                result, limits, live)) {
            return false;
        }
    }
    output = std::move(partition);
    class_count = next;
    return true;
}

void update_memory(
        RefinementResult& result,
        const Graph& graph,
        const std::uint64_t adapter_memory = 0,
        const std::uint64_t transient_memory = 0) {
    std::uint64_t live = fast_estimate_graph_memory(graph);
    add_bytes(live, adapter_memory);
    add_bytes(live, estimate_refinement_result_memory(result));
    add_bytes(live, transient_memory);
    result.telemetry.estimated_memory_bytes = live;
    result.telemetry.peak_estimated_memory_bytes = std::max(
        result.telemetry.peak_estimated_memory_bytes,
        result.telemetry.estimated_memory_bytes);
}

bool fail(
        RefinementResult& result,
        const RefinementStatus status,
        std::string reason) {
    result.status = status;
    result.executable = false;
    result.lumpable = false;
    result.failure_reason = std::move(reason);
    return false;
}

bool cap(
        RefinementResult& result,
        std::string name,
        std::string reason) {
    result.resource_cap = std::move(name);
    return fail(result, RefinementStatus::ResourceCap, std::move(reason));
}

bool refinement_round_cap(
        RefinementResult& result,
        std::string reason) {
    result.resource_cap = "max_refinement_rounds";
    return fail(
        result, RefinementStatus::RefinementRoundCap,
        std::move(reason));
}

bool check_refinement_memory(
        RefinementResult& result,
        const Graph& graph,
        const RefinementLimits& limits,
        const std::uint64_t adapter_memory,
        const std::uint64_t transient_memory = 0) {
    update_memory(
        result, graph, adapter_memory, transient_memory);
    if (result.telemetry.estimated_memory_bytes !=
            std::numeric_limits<std::uint64_t>::max() &&
        result.telemetry.estimated_memory_bytes <=
            limits.max_estimated_memory_bytes) {
        return true;
    }
    return cap(
        result, "max_estimated_memory_bytes",
        "policy refinement reached max_estimated_memory_bytes");
}

bool check_refinement_memory_with_result_ledger(
        RefinementResult& result,
        const Graph& graph,
        const RefinementLimits& limits,
        const std::uint64_t adapter_memory,
        const std::uint64_t result_memory,
        const std::uint64_t transient_memory = 0) {
    std::uint64_t live = fast_estimate_graph_memory(graph);
    add_bytes(live, adapter_memory);
    add_bytes(live, result_memory);
    add_bytes(live, transient_memory);
    result.telemetry.estimated_memory_bytes = live;
    result.telemetry.peak_estimated_memory_bytes = std::max(
        result.telemetry.peak_estimated_memory_bytes, live);
    if (live != std::numeric_limits<std::uint64_t>::max() &&
        live <= limits.max_estimated_memory_bytes) {
        return true;
    }
    return cap(
        result, "max_estimated_memory_bytes",
        "policy refinement reached max_estimated_memory_bytes");
}


} // namespace

} // namespace refinement
} // namespace solver
} // namespace poecraft
