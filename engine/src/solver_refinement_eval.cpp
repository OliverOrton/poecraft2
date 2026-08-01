#include "solver_refinement_graph_core.hpp"

namespace poecraft {
namespace solver {
namespace refinement {

namespace {

PolicyEvaluationResult evaluation_failure(
        PolicyEvaluationResult result,
        const PolicyEvaluationStatus status,
        std::string reason) {
    result.status = status;
    result.converged = false;
    result.failure_reason = std::move(reason);
    return result;
}

PolicyEvaluationResult evaluation_cap(
        PolicyEvaluationResult result,
        std::string name) {
    result.resource_cap = name;
    return evaluation_failure(
        std::move(result), PolicyEvaluationStatus::ResourceCap,
        std::move(name));
}

std::uint64_t estimate_policy_evaluation_result_memory(
        const PolicyEvaluationResult& result) {
    std::uint64_t bytes = saturated_product(
        result.class_values.capacity(), sizeof(RefinedClassValue));
    add_bytes(
        bytes,
        saturated_product(
            result.start_values.capacity(), sizeof(double)));
    add_bytes(
        bytes,
        saturated_product(
            result.improper_component_classes.capacity(),
            sizeof(std::uint32_t)));
    return bytes;
}

struct PolicyEvaluationMemoryLedger {
    std::uint64_t live = 0;
};

bool acquire_policy_evaluation_memory(
        PolicyEvaluationMemoryLedger& ledger,
        PolicyEvaluationResult& result,
        const PolicyEvaluationLimits& limits,
        const std::uint64_t bytes) {
    const bool overflow =
        bytes == std::numeric_limits<std::uint64_t>::max() ||
        ledger.live >
            std::numeric_limits<std::uint64_t>::max() - bytes;
    const std::uint64_t projected =
        overflow
            ? std::numeric_limits<std::uint64_t>::max()
            : ledger.live + bytes;
    result.peak_estimated_memory_bytes = std::max(
        result.peak_estimated_memory_bytes, projected);
    if (overflow ||
        projected > limits.max_estimated_memory_bytes) {
        result.status = PolicyEvaluationStatus::ResourceCap;
        result.converged = false;
        result.resource_cap = "max_estimated_memory_bytes";
        result.failure_reason = "max_estimated_memory_bytes";
        result.estimated_memory_bytes =
            estimate_policy_evaluation_result_memory(result);
        return false;
    }
    ledger.live = projected;
    result.estimated_memory_bytes = ledger.live;
    return true;
}

void release_policy_evaluation_memory(
        PolicyEvaluationMemoryLedger& ledger,
        PolicyEvaluationResult& result,
        const std::uint64_t bytes) {
    ledger.live = bytes > ledger.live ? 0 : ledger.live - bytes;
    result.estimated_memory_bytes = ledger.live;
}

} // namespace

PolicyEvaluationResult evaluate_refined_policy_exact(
        const RefinementResult& refinement,
        PolicyEvaluationRequest request) {
    PolicyEvaluationResult result;
    PolicyEvaluationMemoryLedger memory;
    if (refinement.status != RefinementStatus::Complete ||
        !refinement.executable || !refinement.lumpable ||
        refinement.classes.empty()) {
        return evaluation_failure(
            std::move(result),
            PolicyEvaluationStatus::InvalidRefinement,
            "refinement_not_executable");
    }
    if (request.start_classes.empty()) {
        return evaluation_failure(
            std::move(result), PolicyEvaluationStatus::EmptyStartSet,
            "empty_start_classes");
    }
    if (!std::isfinite(
            request.limits.probability_sum_tolerance) ||
        request.limits.probability_sum_tolerance < 0.0 ||
        !std::isfinite(request.limits.residual_tolerance) ||
        request.limits.residual_tolerance < 0.0) {
        return evaluation_failure(
            std::move(result), PolicyEvaluationStatus::InvalidPolicy,
            "invalid_evaluation_tolerance");
    }

    const std::size_t class_count = refinement.classes.size();
    const std::uint64_t class_table_bytes =
        saturated_product(
            class_count, sizeof(const RefinedPolicyClass*));
    if (!acquire_policy_evaluation_memory(
            memory, result, request.limits,
            class_table_bytes)) {
        return result;
    }
    std::vector<const RefinedPolicyClass*> classes(
        class_count, nullptr);
    const std::uint64_t actual_class_table_bytes =
        saturated_product(
            classes.capacity(), sizeof(const RefinedPolicyClass*));
    if (actual_class_table_bytes > class_table_bytes &&
        !acquire_policy_evaluation_memory(
            memory, result, request.limits,
            actual_class_table_bytes - class_table_bytes)) {
        return result;
    }
    for (const RefinedPolicyClass& refined : refinement.classes) {
        if (refined.class_id >= class_count ||
            classes[refined.class_id] != nullptr) {
            return evaluation_failure(
                std::move(result),
                PolicyEvaluationStatus::InvalidRefinement,
                "invalid_refinement_class_table");
        }
        classes[refined.class_id] = &refined;
    }
    if (std::any_of(
            classes.begin(), classes.end(),
            [](const RefinedPolicyClass* refined) {
                return refined == nullptr;
            })) {
        return evaluation_failure(
            std::move(result),
            PolicyEvaluationStatus::InvalidRefinement,
            "invalid_refinement_class_table");
    }

    for (const std::uint32_t start : request.start_classes) {
        if (start >= class_count) {
            return evaluation_failure(
                std::move(result),
                PolicyEvaluationStatus::InvalidStartClass,
                "invalid_start_class");
        }
    }

    for (const RefinedPolicyClass* row : classes) {
        if (!std::isfinite(row->action_cost) ||
            row->action_cost < 0.0 ||
            (row->goal && !row->terminal)) {
            return evaluation_failure(
                std::move(result),
                PolicyEvaluationStatus::InvalidPolicy,
                "invalid_policy_row");
        }
        if (row->terminal) {
            if (row->selected_action.has_value() ||
                !row->transitions.empty() ||
                row->action_cost != 0.0) {
                return evaluation_failure(
                    std::move(result),
                    PolicyEvaluationStatus::InvalidPolicy,
                    "invalid_terminal_policy_row");
            }
            continue;
        }
        if (!row->selected_action.has_value() ||
            row->transitions.empty()) {
            return evaluation_failure(
                std::move(result),
                PolicyEvaluationStatus::InvalidPolicy,
                "missing_nonterminal_policy_row");
        }
        solve_detail::WideFloat probability_sum = 0.0;
        std::uint32_t previous_successor = 0;
        bool have_previous = false;
        for (const ProjectedTransition& transition :
             row->transitions) {
            if (transition.successor_class >= class_count ||
                !std::isfinite(transition.probability) ||
                transition.probability < 0.0 ||
                (have_previous &&
                 transition.successor_class <=
                     previous_successor)) {
                return evaluation_failure(
                    std::move(result),
                    PolicyEvaluationStatus::InvalidPolicy,
                    "invalid_policy_transition");
            }
            previous_successor = transition.successor_class;
            have_previous = true;
            probability_sum +=
                solve_detail::WideFloat{transition.probability};
        }
        if (std::fabs(probability_sum.value() - 1.0) >
            request.limits.probability_sum_tolerance) {
            return evaluation_failure(
                std::move(result),
                PolicyEvaluationStatus::InvalidPolicy,
                "policy_transition_sum_mismatch");
        }
    }

    const std::uint64_t reachable_bytes =
        saturated_product(
            class_count, sizeof(std::uint8_t));
    if (!acquire_policy_evaluation_memory(
            memory, result, request.limits, reachable_bytes)) {
        return result;
    }
    std::vector<std::uint8_t> reachable(class_count, 0);
    const std::uint64_t actual_reachable_bytes =
        saturated_product(
            reachable.capacity(), sizeof(std::uint8_t));
    if (actual_reachable_bytes > reachable_bytes &&
        !acquire_policy_evaluation_memory(
            memory, result, request.limits,
            actual_reachable_bytes - reachable_bytes)) {
        return result;
    }
    std::set<std::uint32_t> pending;
    const std::uint64_t pending_node_bytes =
        sizeof(std::set<std::uint32_t>::value_type) +
        3 * sizeof(void*);
    for (const std::uint32_t start : request.start_classes) {
        if (pending.contains(start)) continue;
        if (!acquire_policy_evaluation_memory(
                memory, result, request.limits,
                pending_node_bytes)) {
            return result;
        }
        pending.insert(start);
    }
    while (!pending.empty()) {
        const std::uint32_t current = *pending.begin();
        pending.erase(pending.begin());
        release_policy_evaluation_memory(
            memory, result, pending_node_bytes);
        if (reachable[current]) continue;
        if (result.reachable_classes >=
            request.limits.max_reachable_classes) {
            return evaluation_cap(
                std::move(result), "max_reachable_classes");
        }
        reachable[current] = 1;
        ++result.reachable_classes;
        for (const ProjectedTransition& transition :
             classes[current]->transitions) {
            if (transition.probability > 0.0 &&
                !reachable[transition.successor_class] &&
                !pending.contains(transition.successor_class)) {
                if (!acquire_policy_evaluation_memory(
                        memory, result, request.limits,
                        pending_node_bytes)) {
                    return result;
                }
                pending.insert(transition.successor_class);
            }
        }
    }

    constexpr std::uint32_t kNoIndex =
        std::numeric_limits<std::uint32_t>::max();
    struct TarjanFrame {
        std::uint32_t class_id = kNoIndex;
        std::size_t next_transition = 0;
    };
    const std::uint64_t tarjan_fixed_bytes =
        saturated_add(
            saturated_product(
                class_count,
                2 * sizeof(std::uint32_t) +
                    sizeof(std::uint8_t)),
            saturated_product(
                result.reachable_classes,
                sizeof(std::uint32_t) +
                    sizeof(TarjanFrame) +
                    sizeof(std::vector<std::uint32_t>)));
    if (!acquire_policy_evaluation_memory(
            memory, result, request.limits,
            tarjan_fixed_bytes)) {
        return result;
    }
    std::vector<std::uint32_t> index(class_count, kNoIndex);
    std::vector<std::uint32_t> lowlink(class_count, kNoIndex);
    std::vector<std::uint8_t> on_stack(class_count, 0);
    std::vector<std::uint32_t> tarjan_stack;
    tarjan_stack.reserve(result.reachable_classes);
    std::vector<std::vector<std::uint32_t>> components;
    components.reserve(result.reachable_classes);
    std::vector<TarjanFrame> dfs;
    dfs.reserve(result.reachable_classes);
    std::uint64_t actual_tarjan_fixed_bytes =
        saturated_product(
            index.capacity(), sizeof(std::uint32_t));
    add_bytes(
        actual_tarjan_fixed_bytes,
        saturated_product(
            lowlink.capacity(), sizeof(std::uint32_t)));
    add_bytes(
        actual_tarjan_fixed_bytes,
        saturated_product(
            on_stack.capacity(), sizeof(std::uint8_t)));
    add_bytes(
        actual_tarjan_fixed_bytes,
        saturated_product(
            tarjan_stack.capacity(), sizeof(std::uint32_t)));
    add_bytes(
        actual_tarjan_fixed_bytes,
        saturated_product(
            components.capacity(),
            sizeof(std::vector<std::uint32_t>)));
    add_bytes(
        actual_tarjan_fixed_bytes,
        saturated_product(
            dfs.capacity(), sizeof(TarjanFrame)));
    if (actual_tarjan_fixed_bytes > tarjan_fixed_bytes &&
        !acquire_policy_evaluation_memory(
            memory, result, request.limits,
            actual_tarjan_fixed_bytes - tarjan_fixed_bytes)) {
        return result;
    }
    std::uint32_t next_index = 0;
    const auto push_class = [&](
            const std::uint32_t class_id,
            std::vector<TarjanFrame>& dfs) {
        index[class_id] = next_index;
        lowlink[class_id] = next_index;
        ++next_index;
        tarjan_stack.push_back(class_id);
        on_stack[class_id] = 1;
        dfs.push_back({class_id, 0});
    };
    for (std::uint32_t root = 0; root < class_count; ++root) {
        if (!reachable[root] || index[root] != kNoIndex) continue;
        dfs.clear();
        push_class(root, dfs);
        while (!dfs.empty()) {
            TarjanFrame& frame = dfs.back();
            const auto& transitions =
                classes[frame.class_id]->transitions;
            bool descended = false;
            while (frame.next_transition < transitions.size()) {
                const ProjectedTransition& transition =
                    transitions[frame.next_transition++];
                if (transition.probability <= 0.0) continue;
                const std::uint32_t target =
                    transition.successor_class;
                if (index[target] == kNoIndex) {
                    push_class(target, dfs);
                    descended = true;
                    break;
                }
                if (on_stack[target]) {
                    lowlink[frame.class_id] = std::min(
                        lowlink[frame.class_id], index[target]);
                }
            }
            if (descended) continue;

            const std::uint32_t completed = frame.class_id;
            dfs.pop_back();
            if (!dfs.empty()) {
                const std::uint32_t parent =
                    dfs.back().class_id;
                lowlink[parent] = std::min(
                    lowlink[parent], lowlink[completed]);
            }
            if (lowlink[completed] == index[completed]) {
                components.emplace_back();
                while (true) {
                    const std::uint32_t member =
                        tarjan_stack.back();
                    tarjan_stack.pop_back();
                    on_stack[member] = 0;
                    if (components.back().size() ==
                        components.back().capacity()) {
                        const std::size_t old_capacity =
                            components.back().capacity();
                        const std::size_t doubled =
                            old_capacity == 0
                                ? 1
                                : old_capacity >
                                          std::numeric_limits<
                                              std::size_t>::max() /
                                              2
                                      ? std::numeric_limits<
                                            std::size_t>::max()
                                      : old_capacity * 2;
                        const std::size_t required =
                            old_capacity ==
                                    std::numeric_limits<
                                        std::size_t>::max()
                                ? old_capacity
                                : old_capacity + 1;
                        const std::size_t next_capacity =
                            std::min<std::size_t>(
                                result.reachable_classes,
                                std::max(
                                    required, doubled));
                        const std::uint64_t next_bytes =
                            saturated_product(
                                next_capacity,
                                sizeof(std::uint32_t));
                        if (!acquire_policy_evaluation_memory(
                                memory, result, request.limits,
                                next_bytes)) {
                            return result;
                        }
                        components.back().reserve(next_capacity);
                        const std::size_t actual_capacity =
                            components.back().capacity();
                        if (actual_capacity > next_capacity &&
                            !acquire_policy_evaluation_memory(
                                memory, result, request.limits,
                                saturated_product(
                                    actual_capacity - next_capacity,
                                    sizeof(std::uint32_t)))) {
                            return result;
                        }
                        release_policy_evaluation_memory(
                            memory, result,
                            saturated_product(
                                old_capacity,
                                sizeof(std::uint32_t)));
                    }
                    components.back().push_back(member);
                    if (member == completed) break;
                }
                std::sort(
                    components.back().begin(),
                    components.back().end());
                result.largest_component_classes = std::max(
                    result.largest_component_classes,
                    static_cast<std::uint32_t>(
                        components.back().size()));
            }
        }
    }
    result.strongly_connected_components =
        static_cast<std::uint32_t>(components.size());

    const std::uint64_t component_index_bytes =
        saturated_product(
            class_count, sizeof(std::uint32_t));
    if (!acquire_policy_evaluation_memory(
            memory, result, request.limits,
            component_index_bytes)) {
        return result;
    }
    std::vector<std::uint32_t> component_by_class(
        class_count, kNoIndex);
    const std::uint64_t actual_component_index_bytes =
        saturated_product(
            component_by_class.capacity(),
            sizeof(std::uint32_t));
    if (actual_component_index_bytes > component_index_bytes &&
        !acquire_policy_evaluation_memory(
            memory, result, request.limits,
            actual_component_index_bytes -
                component_index_bytes)) {
        return result;
    }
    for (std::uint32_t component = 0;
         component < components.size(); ++component) {
        for (const std::uint32_t member : components[component]) {
            component_by_class[member] = component;
        }
    }

    std::uint32_t canonical_improper_component = kNoIndex;
    for (std::uint32_t component = 0;
         component < components.size(); ++component) {
        bool has_absorption_or_exit = false;
        bool has_terminal = false;
        for (const std::uint32_t member : components[component]) {
            if (classes[member]->terminal) {
                has_terminal = true;
                has_absorption_or_exit = true;
            }
            for (const ProjectedTransition& transition :
                 classes[member]->transitions) {
                if (transition.probability > 0.0 &&
                    component_by_class[
                        transition.successor_class] != component) {
                    has_absorption_or_exit = true;
                }
            }
        }
        if (has_terminal && components[component].size() != 1) {
            return evaluation_failure(
                std::move(result),
                PolicyEvaluationStatus::InvalidPolicy,
                "terminal_class_in_nontrivial_component");
        }
        if (!has_absorption_or_exit) {
            ++result.improper_component_count;
            if (canonical_improper_component == kNoIndex ||
                std::lexicographical_compare(
                    components[component].begin(),
                    components[component].end(),
                    components[canonical_improper_component].begin(),
                    components[canonical_improper_component].end())) {
                canonical_improper_component = component;
            }
        }
    }
    if (canonical_improper_component != kNoIndex) {
        const std::vector<std::uint32_t>& witness =
            components[canonical_improper_component];
        const std::uint64_t projected_witness_bytes =
            saturated_product(
                witness.size(), sizeof(std::uint32_t));
        if (!acquire_policy_evaluation_memory(
                memory, result, request.limits,
                projected_witness_bytes)) {
            return result;
        }
        result.improper_component_classes.reserve(witness.size());
        const std::uint64_t actual_witness_bytes =
            saturated_product(
                result.improper_component_classes.capacity(),
                sizeof(std::uint32_t));
        if (actual_witness_bytes > projected_witness_bytes &&
            !acquire_policy_evaluation_memory(
                memory, result, request.limits,
                actual_witness_bytes - projected_witness_bytes)) {
            return result;
        }
        result.improper_component_classes.insert(
            result.improper_component_classes.end(),
            witness.begin(), witness.end());
        return evaluation_failure(
            std::move(result),
            PolicyEvaluationStatus::ImproperPolicy,
            "improper_closed_component");
    }
    result.proper = true;

    std::uint64_t reachable_edge_count = 0;
    for (std::uint32_t class_id = 0;
         class_id < class_count; ++class_id) {
        if (!reachable[class_id]) continue;
        const std::size_t row_edges = static_cast<std::size_t>(
            std::count_if(
                classes[class_id]->transitions.begin(),
                classes[class_id]->transitions.end(),
                [](const ProjectedTransition& transition) {
                    return transition.probability > 0.0;
                }));
        if (row_edges >
            std::numeric_limits<std::uint32_t>::max()) {
            return evaluation_failure(
                std::move(result),
                PolicyEvaluationStatus::InvalidPolicy,
                "policy_row_is_too_wide");
        }
        reachable_edge_count = saturated_add(
            reachable_edge_count,
            static_cast<std::uint64_t>(row_edges));
    }
    if (reachable_edge_count ==
            std::numeric_limits<std::uint64_t>::max() ||
        reachable_edge_count >
            std::numeric_limits<std::size_t>::max()) {
        acquire_policy_evaluation_memory(
            memory, result, request.limits,
            std::numeric_limits<std::uint64_t>::max());
        return result;
    }

    std::uint64_t value_storage_bytes =
        saturated_product(
            class_count,
            sizeof(double) + sizeof(std::uint8_t) +
                sizeof(solve_detail::PolicyRow) +
                sizeof(std::int32_t));
    add_bytes(
        value_storage_bytes,
        saturated_product(
            static_cast<std::size_t>(reachable_edge_count),
            sizeof(solve_detail::PolicyEdge)));
    if (!acquire_policy_evaluation_memory(
            memory, result, request.limits,
            value_storage_bytes)) {
        return result;
    }
    std::vector<double> values(class_count, 0.0);
    std::vector<std::uint8_t> solved(class_count, 0);
    std::vector<solve_detail::PolicyRow> policy_rows(class_count);
    std::vector<solve_detail::PolicyEdge> policy_edges;
    policy_edges.reserve(
        static_cast<std::size_t>(reachable_edge_count));
    std::vector<std::int32_t> local_by_class(class_count, -1);
    std::uint64_t actual_value_storage_bytes =
        saturated_product(
            values.capacity(), sizeof(double));
    add_bytes(
        actual_value_storage_bytes,
        saturated_product(
            solved.capacity(), sizeof(std::uint8_t)));
    add_bytes(
        actual_value_storage_bytes,
        saturated_product(
            policy_rows.capacity(),
            sizeof(solve_detail::PolicyRow)));
    add_bytes(
        actual_value_storage_bytes,
        saturated_product(
            policy_edges.capacity(),
            sizeof(solve_detail::PolicyEdge)));
    add_bytes(
        actual_value_storage_bytes,
        saturated_product(
            local_by_class.capacity(), sizeof(std::int32_t)));
    if (actual_value_storage_bytes > value_storage_bytes &&
        !acquire_policy_evaluation_memory(
            memory, result, request.limits,
                actual_value_storage_bytes -
                    value_storage_bytes)) {
        return result;
    }
    for (std::uint32_t class_id = 0;
         class_id < class_count; ++class_id) {
        if (!reachable[class_id]) continue;
        const RefinedPolicyClass& source = *classes[class_id];
        solve_detail::PolicyRow& row = policy_rows[class_id];
        row.edge_offset = policy_edges.size();
        row.cost = source.action_cost;
        for (const ProjectedTransition& transition :
             source.transitions) {
            if (!(transition.probability > 0.0)) continue;
            policy_edges.push_back({
                transition.successor_class,
                transition.probability});
        }
        row.edge_count = static_cast<std::uint32_t>(
            policy_edges.size() - row.edge_offset);
    }

    for (std::uint32_t component = 0;
         component < components.size(); ++component) {
        const std::vector<std::uint32_t>& members =
            components[component];
        if (members.size() == 1 &&
            classes[members.front()]->terminal) {
            solved[members.front()] = 1;
            continue;
        }
        const std::size_t order = members.size();
        std::uint64_t component_scratch_bytes =
            solve_detail::sparse_policy_component_scratch_bytes(
                order, true);
        add_bytes(
            component_scratch_bytes,
            saturated_product(order, sizeof(double)));
        if (!acquire_policy_evaluation_memory(
                memory, result, request.limits,
                component_scratch_bytes)) {
            return result;
        }
        std::vector<double> rhs(order, 0.0);
        const std::uint64_t actual_rhs_bytes =
            saturated_product(
                rhs.capacity(), sizeof(double));
        const std::uint64_t projected_rhs_bytes =
            saturated_product(order, sizeof(double));
        if (actual_rhs_bytes > projected_rhs_bytes &&
            !acquire_policy_evaluation_memory(
                memory, result, request.limits,
                actual_rhs_bytes - projected_rhs_bytes)) {
            return result;
        }
        add_bytes(
            component_scratch_bytes,
            actual_rhs_bytes > projected_rhs_bytes
                ? actual_rhs_bytes - projected_rhs_bytes
                : 0);
        for (std::size_t row = 0; row < order; ++row) {
            local_by_class[members[row]] =
                static_cast<std::int32_t>(row);
        }
        for (std::size_t row = 0; row < order; ++row) {
            const std::uint32_t member = members[row];
            solve_detail::WideFloat external =
                policy_rows[member].cost;
            const solve_detail::PolicyRow& policy =
                policy_rows[member];
            for (std::uint32_t edge_index = 0;
                 edge_index < policy.edge_count;
                 ++edge_index) {
                const solve_detail::PolicyEdge& transition =
                    policy_edges.at(
                        policy.edge_offset + edge_index);
                if (component_by_class[
                        transition.target] == component) {
                    if (local_by_class[transition.target] < 0) {
                        return evaluation_failure(
                            std::move(result),
                            PolicyEvaluationStatus::InvalidPolicy,
                            "invalid_component_member");
                    }
                } else {
                    if (!solved[transition.target]) {
                        return evaluation_failure(
                            std::move(result),
                            PolicyEvaluationStatus::InvalidPolicy,
                            "invalid_component_evaluation_order");
                    }
                    external +=
                        solve_detail::WideFloat{
                            transition.probability} *
                        solve_detail::WideFloat{
                            values[transition.target]};
                }
            }
            rhs[row] = external.value();
            if (!std::isfinite(rhs[row])) {
                return evaluation_failure(
                    std::move(result),
                    PolicyEvaluationStatus::NumericFailure,
                    "non_finite_policy_rhs");
            }
        }
        std::unique_ptr<solve_detail::SparsePolicyResume> resume;
        solve_detail::SparsePolicyComponentResult component_result;
        do {
            component_result =
                solve_detail::advance_sparse_policy_component(
                    solve_detail::SparsePolicyComponentView{
                        members,
                        component,
                        component_by_class,
                        local_by_class,
                        policy_rows,
                        policy_edges,
                        rhs,
                        values,
                        request.limits.max_component_iterations},
                    resume);
        } while (
            component_result.status ==
            solve_detail::SparsePolicyComponentStatus::Incomplete);
        if (component_result.status ==
            solve_detail::SparsePolicyComponentStatus::Singular) {
            return evaluation_failure(
                std::move(result),
                PolicyEvaluationStatus::SingularSystem,
                "singular_policy_component");
        }
        if (component_result.status ==
            solve_detail::SparsePolicyComponentStatus::DidNotConverge) {
            return evaluation_cap(
                std::move(result),
                "max_component_iterations");
        }
        if (component_result.status ==
            solve_detail::SparsePolicyComponentStatus::NumericFailure) {
            return evaluation_failure(
                std::move(result),
                PolicyEvaluationStatus::NumericFailure,
                "policy_component_numeric_failure");
        }
        if (component_result.status !=
                solve_detail::SparsePolicyComponentStatus::Complete ||
            component_result.values.size() != order) {
            return evaluation_failure(
                std::move(result),
                PolicyEvaluationStatus::NumericFailure,
                "invalid_policy_component_result");
        }
        double magnitude = 1.0;
        for (const double value : component_result.values) {
            magnitude = std::max(magnitude, std::fabs(value));
        }
        const double negative_tolerance =
            request.limits.residual_tolerance * magnitude;
        for (std::size_t i = 0; i < order; ++i) {
            if (!std::isfinite(component_result.values[i])) {
                return evaluation_failure(
                    std::move(result),
                    PolicyEvaluationStatus::NumericFailure,
                    "non_finite_policy_value");
            }
            if (component_result.values[i] <
                -negative_tolerance) {
                return evaluation_failure(
                    std::move(result),
                    PolicyEvaluationStatus::NumericFailure,
                    "negative_policy_value");
            }
            values[members[i]] =
                std::max(0.0, component_result.values[i]);
            solved[members[i]] = 1;
            local_by_class[members[i]] = -1;
        }
        release_policy_evaluation_memory(
            memory, result, component_scratch_bytes);
    }

    const std::uint64_t projected_result_bytes =
        saturated_add(
            saturated_product(
                result.reachable_classes,
                sizeof(RefinedClassValue)),
            saturated_product(
                request.start_classes.size(), sizeof(double)));
    if (!acquire_policy_evaluation_memory(
            memory, result, request.limits,
            projected_result_bytes)) {
        return result;
    }
    result.class_values.reserve(result.reachable_classes);
    result.start_values.reserve(request.start_classes.size());
    const std::uint64_t actual_result_bytes =
        estimate_policy_evaluation_result_memory(result);
    if (actual_result_bytes > projected_result_bytes &&
        !acquire_policy_evaluation_memory(
            memory, result, request.limits,
            actual_result_bytes - projected_result_bytes)) {
        return result;
    }
    for (std::uint32_t class_id = 0;
         class_id < class_count; ++class_id) {
        if (!reachable[class_id]) continue;
        const RefinedPolicyClass& row = *classes[class_id];
        solve_detail::WideFloat expected = 0.0;
        if (!row.terminal) {
            expected = row.action_cost;
            for (const ProjectedTransition& transition :
                 row.transitions) {
                if (!(transition.probability > 0.0)) continue;
                expected +=
                    solve_detail::WideFloat{
                        transition.probability} *
                    solve_detail::WideFloat{
                        values[transition.successor_class]};
            }
        }
        const double expected_value = expected.value();
        const double residual = std::fabs(
            (solve_detail::WideFloat{values[class_id]} -
             expected)
                .value());
        result.max_residual = std::max(
            result.max_residual, residual);
        const double scale = std::max(
            {1.0, std::fabs(values[class_id]),
             std::fabs(expected_value)});
        if (residual >
            request.limits.residual_tolerance * scale) {
            return evaluation_failure(
                std::move(result),
                PolicyEvaluationStatus::ResidualFailure,
                "policy_value_residual_exceeded");
        }
        const double value = values[class_id];
        if (!std::isfinite(value)) {
            return evaluation_failure(
                std::move(result),
                PolicyEvaluationStatus::NumericFailure,
                "non_finite_policy_value");
        }
        result.class_values.push_back({class_id, value});
    }
    for (const std::uint32_t start : request.start_classes) {
        const double value = values[start];
        if (!std::isfinite(value)) {
            return evaluation_failure(
                std::move(result),
                PolicyEvaluationStatus::NumericFailure,
                "non_finite_start_value");
        }
        result.start_values.push_back(value);
    }
    result.status = PolicyEvaluationStatus::Complete;
    result.converged = true;
    result.estimated_memory_bytes =
        estimate_policy_evaluation_result_memory(result);
    return result;
}

} // namespace refinement
} // namespace solver
} // namespace poecraft
