#pragma once

#include "solver_refinement_graph_core.hpp"

namespace poecraft {
namespace solver {
namespace refinement {
namespace {

ExactState canonical_state(ExactState state) {
    state.features =
        canonical_feature_signature(std::move(state.features));
    return state;
}

bool add_or_verify_state(
        Graph& graph,
        ExactState state,
        std::uint32_t& index,
        RefinementResult& result,
        const RefinementLimits& limits,
        const std::uint64_t adapter_memory,
        const std::uint64_t transient_memory,
        const bool defer_memory_checks = false) {
    state = canonical_state(std::move(state));
    if (state.stable_key.empty()) {
        return fail(
            result, RefinementStatus::InvalidState,
            "exact refinement has an empty stable key");
    }
    if (state.coarse_state_key.empty()) {
        return fail(
            result, RefinementStatus::InvalidState,
            "exact refinement has an empty coarse-state key");
    }
    const auto numeric_parent =
        graph.coarse_key_by_state.find(state.coarse_state);
    if (numeric_parent != graph.coarse_key_by_state.end() &&
        numeric_parent->second != state.coarse_state_key) {
        return fail(
            result, RefinementStatus::InvalidState,
            "one numeric coarse state names incompatible semantic keys");
    }
    const auto semantic_parent =
        graph.coarse_state_by_key.find(state.coarse_state_key);
    if (semantic_parent != graph.coarse_state_by_key.end() &&
        semantic_parent->second != state.coarse_state) {
        return fail(
            result, RefinementStatus::InvalidState,
            "one semantic coarse-state key names incompatible numeric "
            "states");
    }
    const auto [unused_numeric, inserted_numeric] =
        graph.coarse_key_by_state.emplace(
            state.coarse_state, state.coarse_state_key);
    (void)unused_numeric;
    const auto [unused_semantic, inserted_semantic] =
        graph.coarse_state_by_key.emplace(
            state.coarse_state_key, state.coarse_state);
    (void)unused_semantic;
    if (inserted_numeric || inserted_semantic) {
        std::uint64_t parent_growth = 0;
        if (inserted_numeric) {
            add_bytes(
                parent_growth,
                sizeof(decltype(graph.coarse_key_by_state)::value_type) +
                    3 * sizeof(void*) +
                    estimate_stable_key_bytes(state.coarse_state_key));
        }
        if (inserted_semantic) {
            add_bytes(
                parent_growth,
                sizeof(decltype(graph.coarse_state_by_key)::value_type) +
                    3 * sizeof(void*) +
                    estimate_stable_key_bytes(state.coarse_state_key));
        }
        note_graph_memory_growth(graph, parent_growth);
    }
    if (state.goal && !state.terminal) {
        return fail(
            result, RefinementStatus::InvalidState,
            "goal exact refinement is not terminal");
    }
    const std::uint64_t key_hash =
        stable_key_hash(state.stable_key);
    const auto found =
        graph.indices_by_key_hash.find(key_hash);
    if (found != graph.indices_by_key_hash.end()) {
        for (const std::uint32_t candidate : found->second) {
            Node& existing = graph.nodes.at(candidate);
            if (existing.state.stable_key != state.stable_key) {
                continue;
            }
            index = candidate;
            if (!same_stored_exact_state(existing.state, state)) {
                return fail(
                    result, RefinementStatus::InvalidState,
                    "stable exact state key names incompatible records");
            }
            return true;
        }
    }
    if (graph.nodes.size() >= limits.max_exact_states) {
        return cap(
            result, "max_exact_states",
            "policy refinement reached max_exact_states");
    }
    index = static_cast<std::uint32_t>(graph.nodes.size());
    StableKey{}.swap(state.coarse_state_key);
    const std::size_t old_node_capacity = graph.nodes.capacity();
    if (graph.nodes.size() == graph.nodes.capacity()) {
        const std::size_t growth = std::max<std::size_t>(
            2048, graph.nodes.capacity() / 32);
        graph.nodes.reserve(
            saturated_add(graph.nodes.capacity(), growth));
    }
    std::uint64_t graph_growth =
        estimate_exact_state_bytes(state);
    add_bytes(
        graph_growth,
        saturated_product(
            graph.nodes.capacity() - old_node_capacity,
            sizeof(Node)));
    add_bytes(
        graph_growth,
        sizeof(decltype(graph.indices_by_key_hash)::value_type) +
            3 * sizeof(void*) + sizeof(std::uint32_t));
    Node node;
    node.state = std::move(state);
    graph.nodes.push_back(std::move(node));
    graph.indices_by_key_hash[key_hash].push_back(index);
    note_graph_memory_growth(graph, graph_growth);
    result.telemetry.exact_states =
        static_cast<std::uint32_t>(graph.nodes.size());
    return defer_memory_checks ||
           check_refinement_memory(
               result, graph, limits, adapter_memory,
               transient_memory);
}

bool note_reachable_coarse(
        const std::uint32_t coarse_state,
        std::set<std::uint32_t>& reachable_coarse,
        const RefinementLimits& limits,
        RefinementResult& result) {
    if (reachable_coarse.contains(coarse_state)) return true;
    if (reachable_coarse.size() >= limits.max_coarse_states) {
        return cap(
            result, "max_coarse_states",
            "policy refinement reached max_coarse_states");
    }
    reachable_coarse.insert(coarse_state);
    result.telemetry.policy_reachable_coarse_states =
        static_cast<std::uint32_t>(reachable_coarse.size());
    return true;
}

bool validate_selection(
        Graph& graph,
        SelectedAction& selection,
        RefinementResult& result,
        const RefinementLimits& limits,
        const std::uint64_t adapter_memory,
        const std::uint64_t caller_transient_memory,
        const bool defer_memory_check) {
    selection.routing_observes =
        canonical_observation_requirement(
            std::move(selection.routing_observes));
    if (selection.semantic_key.empty()) {
        return fail(
            result, RefinementStatus::InvalidContract,
            "selected action has no executable semantic key");
    }
    if (!selection.contract.complete()) {
        return fail(
            result, RefinementStatus::InvalidContract,
            "selected action has no admitted refinement contract");
    }
    if (!selected_runtime_contracts_complete(selection)) {
        return fail(
            result, RefinementStatus::InvalidContract,
            "selected action has an incomplete or empty runtime path "
            "contract");
    }
    const ActionKey key{
        selection.action_id, selection.semantic_key};
    const StableKey signature =
        selected_runtime_contract_signature(selection);
    const auto [it, inserted] =
        graph.contract_signature_by_action.emplace(key, signature);
    if (!inserted && it->second != signature) {
        return fail(
            result, RefinementStatus::InvalidContract,
            "one executable action has incompatible refinement contracts");
    }
    if (inserted) {
        std::uint64_t growth =
            sizeof(decltype(
                graph.contract_signature_by_action)::value_type) +
            3 * sizeof(void*);
        add_bytes(
            growth,
            estimate_stable_key_bytes(key.semantic_key));
        add_bytes(growth, estimate_stable_key_bytes(signature));
        note_graph_memory_growth(graph, growth);
    }
    std::uint64_t transient = caller_transient_memory;
    add_bytes(
        transient,
        estimate_selected_action_bytes(selection));
    add_bytes(
        transient,
        estimate_stable_key_bytes(signature));
    return defer_memory_check ||
           check_refinement_memory(
               result, graph, limits, adapter_memory, transient);
}

std::shared_ptr<SelectedAction> intern_selection(
        Graph& graph,
        SelectedAction selection) {
    StableKey key{
        0x70637273656c7631ull, /* "pcrselv1" */
        selection.action_id};
    append_tokens(key, selection.semantic_key);
    append_tokens(
        key, selected_runtime_contract_signature(selection));
    append_requirement(key, selection.routing_observes);
    const auto found = graph.selection_by_signature.find(key);
    if (found != graph.selection_by_signature.end()) {
        return found->second;
    }
    auto stored = std::make_shared<SelectedAction>(
        std::move(selection));
    const auto [inserted, unused] =
        graph.selection_by_signature.emplace(
            std::move(key), stored);
    (void)unused;
    std::uint64_t growth =
        sizeof(decltype(
            graph.selection_by_signature)::value_type) +
        5 * sizeof(void*) + sizeof(SelectedAction);
    add_bytes(
        growth,
        estimate_stable_key_bytes(inserted->first));
    add_bytes(
        growth,
        estimate_selected_action_bytes(*stored));
    note_graph_memory_growth(graph, growth);
    return stored;
}

void detach_selection(Node& node) {
    if (node.selection != nullptr && !node.selection.unique()) {
        node.selection =
            std::make_shared<SelectedAction>(*node.selection);
    }
}

void release_discovery_indices(Graph& graph) {
    decltype(graph.indices_by_key_hash){}.swap(
        graph.indices_by_key_hash);
    decltype(graph.kernel_source_by_reuse_key){}.swap(
        graph.kernel_source_by_reuse_key);
    decltype(graph.selection_by_signature){}.swap(
        graph.selection_by_signature);
    decltype(graph.coarse_state_by_key){}.swap(
        graph.coarse_state_by_key);
    decltype(graph.contract_signature_by_action){}.swap(
        graph.contract_signature_by_action);
    invalidate_graph_memory_cache(graph);
}

bool expand_node(
        PolicyRefinementOracle& oracle,
        Graph& graph,
        const std::uint32_t node_index,
        PendingExact& pending_exact,
        std::set<std::uint32_t>& reachable_coarse,
        const RefinementLimits& limits,
        RefinementResult& result,
        const std::uint64_t caller_transient_memory) {
    Node& node = graph.nodes.at(node_index);
    if (node.expanded) return true;
    node.state.coarse_state_key = node_coarse_key(graph, node);
    std::optional<SelectedAction> selected;
    try {
        selected = oracle.selected_action(node.state);
    } catch (const std::exception& error) {
        return fail(
            result, RefinementStatus::OracleFailure,
            std::string{"selected-action oracle failed: "} + error.what());
    } catch (...) {
        return fail(
            result, RefinementStatus::OracleFailure,
            "selected-action oracle failed");
    }
    if (node.state.terminal) {
        if (selected.has_value()) {
            return fail(
                result, RefinementStatus::InvalidState,
                "terminal exact refinement selects an action");
        }
        node.expanded = true;
        StableKey{}.swap(node.state.coarse_state_key);
        if (pending_exact.empty()) {
            pending_exact.release_storage();
            std::set<std::uint32_t>{}.swap(reachable_coarse);
            release_discovery_indices(graph);
        }
        return true;
    }
    if (!selected.has_value()) {
        return fail(
            result, RefinementStatus::MissingPolicyAction,
            "non-terminal exact refinement has no selected action");
    }
    std::uint64_t selection_context =
        estimate_discovery_worklists_memory(
            pending_exact, reachable_coarse);
    add_bytes(selection_context, caller_transient_memory);
    const bool possible_final_node = pending_exact.empty();
    if (!validate_selection(
            graph, *selected, result, limits,
            oracle.estimated_owned_bytes(), selection_context,
            possible_final_node)) {
        return false;
    }
    const std::uint64_t selected_memory =
        estimate_selected_action_bytes(*selected);
    std::uint64_t discovery_memory =
        estimate_discovery_worklists_memory(
            pending_exact, reachable_coarse);
    add_bytes(discovery_memory, caller_transient_memory);
    add_bytes(discovery_memory, selected_memory);
    if (!possible_final_node &&
        !check_refinement_memory(
            result, graph, limits,
            oracle.estimated_owned_bytes(),
            discovery_memory)) {
        return false;
    }
    std::optional<StableKey> reuse_key;
    try {
        reuse_key = oracle.exact_kernel_reuse_key(
            node.state, *selected);
    } catch (const RefinementOracleResourceLimit& error) {
        return cap(result, error.cap_name(), error.what());
    } catch (const std::exception& error) {
        return fail(
            result, RefinementStatus::OracleFailure,
            std::string{"exact-kernel reuse oracle failed: "} +
                error.what());
    } catch (...) {
        return fail(
            result, RefinementStatus::OracleFailure,
            "exact-kernel reuse oracle failed");
    }
    if (reuse_key.has_value()) {
        const auto reused =
            graph.kernel_source_by_reuse_key.find(*reuse_key);
        if (reused != graph.kernel_source_by_reuse_key.end()) {
            if (reused->second >= graph.nodes.size() ||
                !graph.nodes[reused->second].expanded) {
                return fail(
                    result, RefinementStatus::InvalidKernel,
                    "exact-kernel reuse key names an incomplete row");
            }
            const Node& source = graph.nodes[reused->second];
            StableKey{}.swap(
                graph.nodes[node_index].state.coarse_state_key);
            Node& refreshed = graph.nodes[node_index];
            refreshed.selection = intern_selection(
                graph, std::move(*selected));
            refreshed.action_cost = source.action_cost;
            refreshed.edge_source = reused->second;
            refreshed.expanded = true;
            std::uint64_t row_growth = 0;
            note_graph_memory_growth(graph, row_growth);
            oracle.note_exact_kernel_reuse();
            std::uint64_t transient =
                estimate_discovery_worklists_memory(
                    pending_exact, reachable_coarse);
            add_bytes(transient, caller_transient_memory);
            if (pending_exact.empty()) {
                pending_exact.release_storage();
                std::set<std::uint32_t>{}.swap(reachable_coarse);
                release_discovery_indices(graph);
                transient = caller_transient_memory;
            }
            return check_refinement_memory(
                result, graph, limits,
                oracle.estimated_owned_bytes(), transient);
        }
    }
    if (result.telemetry.exact_kernels >= limits.max_exact_kernels) {
        return cap(
            result, "max_exact_kernels",
            "policy refinement reached max_exact_kernels");
    }

    ExactActionKernel kernel;
    try {
        kernel = oracle.exact_kernel(node.state, *selected);
    } catch (const RefinementOracleResourceLimit& error) {
        return cap(
            result, error.cap_name(), error.what());
    } catch (const std::exception& error) {
        return fail(
            result, RefinementStatus::OracleFailure,
            std::string{"exact-kernel oracle failed: "} + error.what());
    } catch (...) {
        return fail(
            result, RefinementStatus::OracleFailure,
            "exact-kernel oracle failed");
    }
    StableKey{}.swap(
        graph.nodes[node_index].state.coarse_state_key);
    if (!std::isfinite(kernel.action_cost) ||
        kernel.action_cost < 0.0) {
        return fail(
            result, RefinementStatus::InvalidKernel,
            "exact kernel has an invalid action cost");
    }
    const std::uint64_t adapter_memory =
        oracle.estimated_owned_bytes();
    discovery_memory =
        estimate_discovery_worklists_memory(
            pending_exact, reachable_coarse);
    add_bytes(discovery_memory, caller_transient_memory);
    add_bytes(discovery_memory, selected_memory);
    add_bytes(
        discovery_memory,
        estimate_exact_kernel_bytes(kernel));
    if (!check_refinement_memory(
            result, graph, limits, adapter_memory,
            discovery_memory)) {
        return false;
    }

    DeterministicSum total;
    for (ExactTransition& transition : kernel.transitions) {
        if (!std::isfinite(transition.probability) ||
            transition.probability < 0.0) {
            return fail(
                result, RefinementStatus::InvalidKernel,
                "exact kernel contains an invalid probability");
        }
        if (transition.probability == 0.0) continue;
        transition.successor =
            canonical_state(std::move(transition.successor));
        total.add(transition.probability);
    }
    std::sort(
        kernel.transitions.begin(), kernel.transitions.end(),
        [](const ExactTransition& left,
           const ExactTransition& right) {
            return left.successor.stable_key <
                   right.successor.stable_key;
        });
    std::size_t combined_size = 0;
    for (std::size_t first = 0;
         first < kernel.transitions.size();) {
        if (kernel.transitions[first].probability == 0.0) {
            ++first;
            continue;
        }
        std::size_t end = first + 1;
        DeterministicSum probability;
        probability.add(kernel.transitions[first].probability);
        while (end < kernel.transitions.size() &&
               kernel.transitions[end].successor.stable_key ==
                   kernel.transitions[first].successor.stable_key) {
            if (!(kernel.transitions[end].successor ==
                  kernel.transitions[first].successor)) {
                return fail(
                    result, RefinementStatus::InvalidKernel,
                    "one exact successor key names incompatible records");
            }
            probability.add(kernel.transitions[end].probability);
            ++end;
        }
        if (combined_size != first) {
            kernel.transitions[combined_size].successor =
                std::move(kernel.transitions[first].successor);
        }
        kernel.transitions[combined_size].probability =
            probability.value();
        ++combined_size;
        first = end;
    }
    kernel.transitions.resize(combined_size);
    std::uint64_t working_kernel_memory =
        estimate_exact_kernel_bytes(kernel);
    {
        std::uint64_t transient =
            estimate_discovery_worklists_memory(
                pending_exact, reachable_coarse);
        add_bytes(transient, caller_transient_memory);
        add_bytes(transient, selected_memory);
        add_bytes(transient, working_kernel_memory);
        if (!check_refinement_memory(
                result, graph, limits, adapter_memory,
                transient)) {
            return false;
        }
    }
    if (kernel.transitions.empty() ||
        std::abs(total.value() - 1.0) >
            limits.probability_sum_tolerance) {
        return fail(
            result, RefinementStatus::InvalidKernel,
            "exact kernel is empty or does not sum to one");
    }
    if (result.telemetry.exact_transitions +
            kernel.transitions.size() >
        limits.max_transitions) {
        return cap(
            result, "max_transitions",
            "policy refinement reached max_transitions");
    }

    std::vector<NodeEdge> edges;
    edges.reserve(kernel.transitions.size());
    {
        std::uint64_t transient =
            estimate_discovery_worklists_memory(
                pending_exact, reachable_coarse);
        add_bytes(transient, caller_transient_memory);
        add_bytes(transient, selected_memory);
        add_bytes(transient, working_kernel_memory);
        add_bytes(transient, estimate_node_edges_memory(edges));
        if (!check_refinement_memory(
                result, graph, limits, adapter_memory,
                transient)) {
            return false;
        }
    }
    std::uint32_t ingested_since_audit = 0;
    for (ExactTransition& transition : kernel.transitions) {
        if (!note_reachable_coarse(
                transition.successor.coarse_state, reachable_coarse,
                limits, result)) {
            return false;
        }
        std::uint32_t successor = 0;
        const std::uint64_t moved_payload =
            estimate_exact_state_bytes(transition.successor);
        working_kernel_memory =
            moved_payload > working_kernel_memory
                ? 0
                : working_kernel_memory - moved_payload;
        if (!add_or_verify_state(
                graph, std::move(transition.successor), successor,
                result,
                limits, adapter_memory, 0, true)) {
            return false;
        }
        DeterministicSum probability;
        probability.add(transition.probability);
        edges.push_back({successor, probability});
        if (!graph.nodes.at(successor).expanded) {
            pending_exact.insert(successor);
        }
        ++ingested_since_audit;
        if (ingested_since_audit == 512) {
            std::uint64_t transient =
                estimate_discovery_worklists_memory(
                    pending_exact, reachable_coarse);
            add_bytes(transient, caller_transient_memory);
            add_bytes(transient, selected_memory);
            add_bytes(transient, working_kernel_memory);
            add_bytes(
                transient, estimate_node_edges_memory(edges));
            if (!check_refinement_memory(
                    result, graph, limits, adapter_memory,
                    transient)) {
                return false;
            }
            ingested_since_audit = 0;
        }
    }
    Node& refreshed = graph.nodes.at(node_index);
    refreshed.selection = intern_selection(
        graph, std::move(*selected));
    refreshed.action_cost = kernel.action_cost;
    refreshed.edges = std::move(edges);
    refreshed.expanded = true;
    std::uint64_t row_growth = 0;
    add_bytes(
        row_growth,
        estimate_node_edges_memory(refreshed.edges));
    ++result.telemetry.exact_kernels;
    result.telemetry.exact_transitions += refreshed.edges.size();
    if (reuse_key.has_value()) {
        const auto [unused, inserted] =
            graph.kernel_source_by_reuse_key.emplace(
                std::move(*reuse_key), node_index);
        (void)unused;
        if (!inserted) {
            return fail(
                result, RefinementStatus::InvalidKernel,
                "exact-kernel reuse authority changed during row "
                "construction");
        }
        add_bytes(
            row_growth,
            sizeof(decltype(
                graph.kernel_source_by_reuse_key)::value_type) +
                3 * sizeof(void*) +
                estimate_stable_key_bytes(unused->first));
    }
    note_graph_memory_growth(graph, row_growth);
    std::vector<ExactTransition>{}.swap(kernel.transitions);
    std::uint64_t transient =
        estimate_discovery_worklists_memory(
            pending_exact, reachable_coarse);
    add_bytes(transient, caller_transient_memory);
    if (pending_exact.empty()) {
        pending_exact.release_storage();
        std::set<std::uint32_t>{}.swap(reachable_coarse);
        release_discovery_indices(graph);
        transient = caller_transient_memory;
    }
    return check_refinement_memory(
        result, graph, limits, adapter_memory, transient);
}

bool discover_policy_graph(
        PolicyRefinementOracle& oracle,
        Graph& graph,
        const RefinementRequest& request,
        RefinementResult& result) {
    std::set<std::uint32_t> reachable_coarse;
    PendingExact pending_exact{&graph};

    /*
     * Enumerate only the requested roots. Every later exact state is supplied
     * by a strict kernel transition, which avoids eagerly expanding all
     * concrete members of a policy-reachable successor coarse parent.
     */
    for (const std::uint32_t coarse : request.coarse_roots) {
        if (!note_reachable_coarse(
                coarse, reachable_coarse, request.limits, result)) {
            return false;
        }
        std::vector<ExactState> states;
        try {
            states = oracle.enumerate_refinements(coarse);
        } catch (const std::exception& error) {
            return fail(
                result, RefinementStatus::OracleFailure,
                std::string{"refinement oracle failed: "} + error.what());
        } catch (...) {
            return fail(
                result, RefinementStatus::OracleFailure,
                "refinement oracle failed");
        }
        {
            std::uint64_t transient =
                estimate_discovery_worklists_memory(
                    pending_exact, reachable_coarse);
            add_bytes(
                transient,
                estimate_exact_states_memory(states));
            if (!check_refinement_memory(
                    result, graph, request.limits,
                    oracle.estimated_owned_bytes(), transient)) {
                return false;
            }
        }
        if (states.empty()) {
            return fail(
                result, RefinementStatus::InvalidState,
                "policy-reachable coarse state has no refinements");
        }
        for (ExactState& exact : states) {
            exact = canonical_state(std::move(exact));
        }
        std::sort(
            states.begin(), states.end(),
            [](const ExactState& left, const ExactState& right) {
                return left.stable_key < right.stable_key;
            });
        StableKey previous;
        bool have_previous = false;
        for (ExactState& exact : states) {
            if (exact.coarse_state != coarse ||
                (have_previous &&
                 exact.stable_key == previous)) {
                return fail(
                    result, RefinementStatus::InvalidState,
                    "refinement has wrong parent or duplicate key");
            }
            previous = exact.stable_key;
            have_previous = true;
            std::uint32_t index = 0;
            std::uint64_t transient =
                estimate_discovery_worklists_memory(
                    pending_exact, reachable_coarse);
            add_bytes(
                transient,
                estimate_exact_states_memory(states));
            add_bytes(
                transient,
                estimate_stable_key_bytes(previous));
            if (!add_or_verify_state(
                    graph, std::move(exact), index, result,
                    request.limits,
                    oracle.estimated_owned_bytes(),
                    transient)) {
                return false;
            }
            pending_exact.insert(index);
            transient = estimate_discovery_worklists_memory(
                pending_exact, reachable_coarse);
            add_bytes(
                transient,
                estimate_exact_states_memory(states));
            add_bytes(
                transient,
                estimate_stable_key_bytes(previous));
            if (!check_refinement_memory(
                    result, graph, request.limits,
                    oracle.estimated_owned_bytes(), transient)) {
                return false;
            }
        }
    }

    /*
     * The worklist retains graph ids instead of a second full copy of every
     * collision-free key. Its comparator reads the immutable graph keys, so
     * discovery order remains deterministic even when root enumeration or
     * kernel transition records arrive in a different order.
     */
    while (!pending_exact.empty()) {
        const std::uint32_t index = pending_exact.pop_next();
        if (index >= graph.nodes.size()) {
            return fail(
                result, RefinementStatus::InvalidState,
                "pending exact state disappeared during discovery");
        }
        if (!expand_node(
                oracle, graph, index, pending_exact,
                reachable_coarse, request.limits, result,
                0)) {
            return false;
        }
    }

    for (const Node& node : graph.nodes) {
        if (!node.expanded) {
            return fail(
                result, RefinementStatus::InvalidState,
                "policy-reachable exact successor was not expanded");
        }
    }
    release_discovery_indices(graph);
    return true;
}

void canonicalize_graph(Graph& graph) {
    std::vector<std::uint32_t> order(graph.nodes.size());
    for (std::uint32_t i = 0; i < order.size(); ++i) order[i] = i;
    std::sort(
        order.begin(), order.end(),
        [&](const std::uint32_t left, const std::uint32_t right) {
            const Node& a = graph.nodes[left];
            const Node& b = graph.nodes[right];
            return std::tie(
                       node_coarse_key(graph, a),
                       a.state.stable_key) <
                   std::tie(
                       node_coarse_key(graph, b),
                       b.state.stable_key);
        });
    std::vector<std::uint32_t> remap(order.size());
    for (std::uint32_t next = 0; next < order.size(); ++next) {
        remap[order[next]] = next;
    }
    /*
     * `remap` is the authoritative old-to-canonical id map needed by the
     * edges below. Reuse `order` as a mutable placement permutation so the
     * canonicalization does not briefly retain a second full Node vector.
     */
    order = remap;
    for (std::uint32_t current = 0;
         current < order.size(); ++current) {
        while (order[current] != current) {
            const std::uint32_t target = order[current];
            std::swap(
                graph.nodes[current], graph.nodes[target]);
            std::swap(order[current], order[target]);
        }
    }
    for (Node& node : graph.nodes) {
        if (node.edge_source != kNoEdgeSource) {
            node.edge_source = remap.at(node.edge_source);
        }
        for (NodeEdge& edge : node.edges) {
            edge.successor = remap.at(edge.successor);
        }
        std::sort(
            node.edges.begin(), node.edges.end(),
            [](const NodeEdge& left, const NodeEdge& right) {
                return left.successor < right.successor;
            });
    }
    invalidate_graph_memory_cache(graph);
}

bool propagate_observations(
        Graph& graph,
        const RefinementLimits& limits,
        RefinementResult& result,
        const std::uint64_t adapter_memory) {
    std::vector<PolicyObservationNode> nodes;
    nodes.reserve(graph.nodes.size());
    std::uint64_t observation_nodes_memory = saturated_product(
        nodes.capacity(), sizeof(PolicyObservationNode));
    {
        std::map<const SelectedAction*, std::uint32_t>
            selection_authority;
        std::uint32_t nodes_since_audit = 0;
        for (std::uint32_t index = 0;
             index < graph.nodes.size(); ++index) {
            PolicyObservationNode node;
            node.state_id = index;
            if (graph.nodes[index].selection != nullptr) {
                const auto [authority, inserted] =
                    selection_authority.emplace(
                        graph.nodes[index].selection.get(), index);
                if (inserted) {
                    node.selected_action =
                        *graph.nodes[index].selection;
                } else {
                    node.selected_action_source = authority->second;
                }
            }
            if (graph.nodes[index].edge_source != kNoEdgeSource) {
                node.successor_source =
                    graph.nodes[index].edge_source;
            } else {
                for (const NodeEdge& edge : graph.nodes[index].edges) {
                    node.successors.push_back(edge.successor);
                }
            }
            nodes.push_back(std::move(node));
            add_bytes(
                observation_nodes_memory,
                estimate_policy_observation_node_nested_memory(
                    nodes.back()));
            ++nodes_since_audit;
            if (nodes_since_audit == 512 ||
                nodes.size() == graph.nodes.size()) {
                std::uint64_t construction_memory =
                    observation_nodes_memory;
                add_bytes(
                    construction_memory,
                    estimate_ordered_nodes(selection_authority));
                if (!check_refinement_memory(
                        result, graph, limits, adapter_memory,
                        construction_memory)) {
                    return false;
                }
                nodes_since_audit = 0;
            }
        }
    }
    const std::size_t observation_node_count = nodes.size();
    PolicyObservationFixedPoint fixed =
        propagate_policy_observations(
            std::move(nodes), limits.max_refinement_rounds);
    std::uint64_t fixed_peak = observation_nodes_memory;
    add_bytes(
        fixed_peak,
        saturated_product(
            observation_node_count,
            sizeof(std::pair<const std::uint32_t, std::uint32_t>) +
                3 * sizeof(void*)));
    const std::uint64_t fixed_result_memory =
        estimate_policy_observation_fixed_memory(fixed);
    add_bytes(fixed_peak, fixed_result_memory);
    /*
     * The propagation loop holds `required` and its copied `next` together.
     * The returned assignments have the same canonical requirement payload.
     */
    add_bytes(fixed_peak, fixed_result_memory);
    std::uint64_t largest_requirement = 0;
    for (const PolicyObservationAssignment& assignment :
         fixed.assignments) {
        largest_requirement = std::max(
            largest_requirement,
            estimate_requirement_bytes(assignment.required));
    }
    /* Per-edge carried and merged requirements overlap both full vectors. */
    add_bytes(fixed_peak, largest_requirement);
    add_bytes(fixed_peak, largest_requirement);
    if (!check_refinement_memory(
            result, graph, limits, adapter_memory, fixed_peak)) {
        return false;
    }
    result.telemetry.observation_propagation_rounds =
        fixed.rounds;
    result.telemetry.collapse_events +=
        fixed.collapse_events;
    result.telemetry.collapse_destroyed_feature_mask |=
        fixed.collapse_destroyed_feature_mask;
    result.telemetry.collapse_preserved_feature_mask |=
        fixed.collapse_preserved_feature_mask;
    for (std::size_t feature = 0;
         feature < fixed.collapse_events_by_feature.size();
         ++feature) {
        result.telemetry.collapse_events_by_feature[feature] +=
            fixed.collapse_events_by_feature[feature];
        result.telemetry.preservation_events_by_feature[feature] +=
            fixed.preservation_events_by_feature[feature];
    }
    if (!fixed.complete) {
        if (fixed.round_cap) {
            return refinement_round_cap(
                result, fixed.failure_reason);
        }
        return fail(
            result, RefinementStatus::InvalidContract,
            fixed.failure_reason);
    }
    std::uint32_t assignments_since_audit = 0;
    for (PolicyObservationAssignment& assignment :
         fixed.assignments) {
        note_graph_memory_growth(
            graph,
            estimate_requirement_bytes(assignment.required));
        graph.nodes.at(assignment.state_id).required =
            std::move(assignment.required);
        ++assignments_since_audit;
        if (assignments_since_audit == 512 ||
            &assignment == &fixed.assignments.back()) {
            /*
             * Retaining the pre-move estimate is conservative: moved-from
             * assignments can only release payload while graph ownership
             * grows through the separately tracked ledger above.
             */
            if (!check_refinement_memory(
                    result, graph, limits, adapter_memory,
                    fixed_result_memory)) {
                return false;
            }
            assignments_since_audit = 0;
        }
    }
    return true;
}


} // namespace

} // namespace refinement
} // namespace solver
} // namespace poecraft
