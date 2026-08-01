#include "solver_refinement_graph_core.hpp"

namespace poecraft {
namespace solver {
namespace refinement {

ClosedPartitionResult refine_closed_probabilistic_partition(
        std::vector<ClosedPartitionNode> nodes,
        const ClosedPartitionLimits limits) {
    ClosedPartitionResult result;
    if (nodes.empty()) {
        result.failure_reason =
            "closed probabilistic partition graph is empty";
        return result;
    }
    if (nodes.size() >
        std::numeric_limits<std::uint32_t>::max()) {
        result.status = ClosedPartitionStatus::ResourceCap;
        result.resource_cap = "max_classes";
        result.failure_reason =
            "closed probabilistic partition reached max_classes";
        return result;
    }
    const std::uint64_t input_memory =
        estimate_closed_nodes_memory(nodes);
    if (!check_closed_partition_memory(
            result, limits,
            saturated_add(
                input_memory,
                estimate_closed_partition_result_memory(result)))) {
        return result;
    }
    std::uint64_t canonicalization_peak = input_memory;
    add_bytes(
        canonicalization_peak,
        estimate_closed_canonicalization_scratch(nodes));
    add_bytes(
        canonicalization_peak,
        estimate_closed_partition_result_memory(result));
    if (!check_closed_partition_memory(
            result, limits, canonicalization_peak)) {
        return result;
    }

    std::vector<CanonicalClosedNode> canonical;
    if (!canonicalize_closed_partition_graph(
            std::move(nodes), limits, canonical,
            result.failure_reason)) {
        result.status = ClosedPartitionStatus::InvalidGraph;
        return result;
    }

    const std::uint64_t canonical_memory =
        estimate_canonical_closed_nodes_memory(canonical);
    if (!check_closed_partition_memory(
            result, limits,
            saturated_add(
                canonical_memory,
                estimate_closed_partition_result_memory(result)))) {
        return result;
    }
    std::vector<std::uint32_t> initial;
    std::uint32_t initial_count = 0;
    if (!exact_closed_partition(
            canonical.size(),
            [&](const std::uint32_t node) {
                return closed_initial_key(
                    canonical, canonical[node]);
            },
            saturated_add(
                canonical_memory,
                estimate_closed_partition_result_memory(result)),
            0, limits, result, initial, initial_count)) {
        return result;
    }
    result.initial_class_count = initial_count;
    result.final_class_count = initial_count;
    if (initial_count > limits.max_classes) {
        result.status = ClosedPartitionStatus::ResourceCap;
        result.resource_cap = "max_classes";
        result.failure_reason =
            "closed probabilistic partition reached max_classes";
        return result;
    }

    std::uint64_t partition_copy_peak = canonical_memory;
    add_bytes(
        partition_copy_peak,
        estimate_closed_partition_result_memory(result));
    add_bytes(
        partition_copy_peak,
        saturated_product(
            initial.capacity(), sizeof(std::uint32_t)));
    add_bytes(
        partition_copy_peak,
        saturated_product(
            initial.size(), sizeof(std::uint32_t)));
    if (!check_closed_partition_memory(
            result, limits, partition_copy_peak)) {
        return result;
    }
    std::vector<std::uint32_t> partition = initial;
    std::uint32_t class_count = initial_count;
    for (;;) {
        if (result.rounds >= limits.max_rounds) {
            result.status =
                ClosedPartitionStatus::RefinementRoundCap;
            result.failure_reason =
                "closed probabilistic partition reached max_rounds";
            return result;
        }
        std::uint64_t retained = canonical_memory;
        add_bytes(
            retained,
            estimate_closed_partition_result_memory(result));
        add_bytes(
            retained,
            saturated_product(
                initial.capacity(), sizeof(std::uint32_t)));
        add_bytes(
            retained,
            saturated_product(
                partition.capacity(), sizeof(std::uint32_t)));
        std::vector<std::uint32_t> next;
        std::uint32_t next_count = 0;
        std::vector<bool> shared_arc_authority(canonical.size(), false);
        for (const CanonicalClosedNode& node : canonical) {
            if (node.arc_source.has_value()) {
                shared_arc_authority[*node.arc_source] = true;
            }
        }
        std::map<std::uint32_t,
                 std::vector<ClosedProjectionEntry>>
            shared_projections;
        if (!exact_closed_partition(
                canonical.size(),
                [&](const std::uint32_t node) {
                    const std::uint32_t authority =
                        canonical[node].arc_source.value_or(node);
                    if (canonical[node].arc_source.has_value() ||
                        shared_arc_authority[node]) {
                        const auto [projection, inserted] =
                            shared_projections.try_emplace(authority);
                        if (inserted) {
                            projection->second = project_closed_row(
                                canonical,
                                canonical[authority], partition);
                        }
                        return closed_refined_key(
                            canonical, canonical[node], partition[node],
                            projection->second);
                    }
                    const std::vector<ClosedProjectionEntry> projected =
                        project_closed_row(
                            canonical, canonical[node], partition);
                    return closed_refined_key(
                        canonical, canonical[node],
                        partition[node], projected);
                },
                retained,
                estimate_max_closed_projection_scratch(canonical),
                limits, result, next, next_count)) {
            return result;
        }
        ++result.rounds;
        result.final_class_count = next_count;
        if (next_count < class_count) {
            result.status = ClosedPartitionStatus::NonLumpable;
            result.failure_reason =
                "closed probabilistic partition violated split-only "
                "refinement";
            return result;
        }
        if (next_count > limits.max_classes) {
            result.status = ClosedPartitionStatus::ResourceCap;
            result.resource_cap = "max_classes";
            result.failure_reason =
                "closed probabilistic partition reached max_classes";
            return result;
        }
        class_count = next_count;
        if (next == partition) break;
        partition = std::move(next);
    }

    std::uint64_t members_peak = canonical_memory;
    add_bytes(
        members_peak,
        estimate_closed_partition_result_memory(result));
    add_bytes(
        members_peak,
        saturated_product(
            initial.capacity(), sizeof(std::uint32_t)));
    add_bytes(
        members_peak,
        saturated_product(
            partition.capacity(), sizeof(std::uint32_t)));
    add_bytes(
        members_peak,
        saturated_product(
            class_count, sizeof(std::size_t)));
    add_bytes(
        members_peak,
        saturated_product(
            class_count,
            sizeof(std::vector<std::uint32_t>)));
    add_bytes(
        members_peak,
        saturated_product(
            canonical.size(), sizeof(std::uint32_t)));
    add_bytes(
        members_peak,
        saturated_product(
            class_count, sizeof(ClosedPartitionClass)));
    if (!check_closed_partition_memory(
            result, limits, members_peak)) {
        return result;
    }
    std::vector<std::size_t> member_counts(class_count, 0);
    for (const std::uint32_t class_id : partition) {
        ++member_counts.at(class_id);
    }
    std::vector<std::vector<std::uint32_t>> members(class_count);
    for (std::uint32_t class_id = 0;
         class_id < class_count; ++class_id) {
        members[class_id].reserve(member_counts[class_id]);
    }
    for (std::uint32_t node = 0;
         node < canonical.size(); ++node) {
        members.at(partition[node]).push_back(node);
    }
    result.classes.reserve(class_count);
    {
        std::uint64_t live = canonical_memory;
        add_bytes(
            live,
            estimate_closed_partition_result_memory(result));
        add_bytes(
            live,
            saturated_product(
                initial.capacity(), sizeof(std::uint32_t)));
        add_bytes(
            live,
            saturated_product(
                partition.capacity(), sizeof(std::uint32_t)));
        add_bytes(
            live,
            saturated_product(
                member_counts.capacity(), sizeof(std::size_t)));
        add_bytes(live, estimate_closed_members_memory(members));
        if (!check_closed_partition_memory(
                result, limits, live)) {
            return result;
        }
    }
    for (std::uint32_t class_id = 0;
         class_id < class_count; ++class_id) {
        if (members[class_id].empty()) {
            result.status = ClosedPartitionStatus::NonLumpable;
            result.failure_reason =
                "closed probabilistic partition has an empty class";
            return result;
        }
        const std::uint32_t authority_index =
            members[class_id].front();
        const CanonicalClosedNode& authority =
            canonical[authority_index];
        const std::uint32_t authority_arc_source =
            authority.arc_source.value_or(authority_index);
        std::uint64_t class_base = canonical_memory;
        add_bytes(
            class_base,
            estimate_closed_partition_result_memory(result));
        add_bytes(
            class_base,
            saturated_product(
                initial.capacity(), sizeof(std::uint32_t)));
        add_bytes(
            class_base,
            saturated_product(
                partition.capacity(), sizeof(std::uint32_t)));
        add_bytes(
            class_base,
            saturated_product(
                member_counts.capacity(), sizeof(std::size_t)));
        add_bytes(
            class_base,
            estimate_closed_members_memory(members));
        if (!check_closed_partition_memory(
                result, limits,
                saturated_add(
                    class_base,
                    estimate_closed_projection_scratch(
                        canonical, authority)))) {
            return result;
        }
        const std::vector<ClosedProjectionEntry> projection =
            project_closed_row(canonical, authority, partition);
        if (!check_closed_partition_memory(
                result, limits,
                saturated_add(
                    class_base,
                    estimate_closed_projection_memory(
                        projection)))) {
            return result;
        }
        for (std::size_t member = 1;
             member < members[class_id].size(); ++member) {
            const CanonicalClosedNode& candidate =
                canonical[members[class_id][member]];
            const std::uint32_t candidate_index =
                members[class_id][member];
            const bool shares_absolute_row =
                candidate.arc_source.value_or(candidate_index) ==
                authority_arc_source;
            ++result.lumpability_checks;
            std::uint64_t proof_live = class_base;
            add_bytes(
                proof_live,
                estimate_closed_projection_memory(projection));
            if (!shares_absolute_row) {
                add_bytes(
                    proof_live,
                    estimate_closed_projection_scratch(
                        canonical, candidate));
            }
            if (!check_closed_partition_memory(
                    result, limits, proof_live)) {
                return result;
            }
            if (candidate.terminal != authority.terminal ||
                closed_observation_key(canonical, candidate) !=
                    closed_observation_key(canonical, authority) ||
                closed_immediate_key(canonical, candidate) !=
                    closed_immediate_key(canonical, authority) ||
                (!shares_absolute_row &&
                 project_closed_row(
                     canonical, candidate, partition) !=
                     projection)) {
                result.status =
                    ClosedPartitionStatus::NonLumpable;
                result.failure_reason =
                    "closed probabilistic partition failed final "
                    "lumpability proof";
                return result;
            }
        }

        std::uint64_t projected_output =
            saturated_product(
                members[class_id].size(), sizeof(StableKey));
        for (const std::uint32_t member : members[class_id]) {
            add_bytes(
                projected_output,
                estimate_stable_key_bytes(
                    canonical[member].stable_key));
        }
        add_bytes(
            projected_output,
            estimate_stable_key_bytes(
                closed_observation_key(canonical, authority)));
        add_bytes(
            projected_output,
            estimate_stable_key_bytes(
                closed_immediate_key(canonical, authority)));
        add_bytes(
            projected_output,
            saturated_product(
                projection.size(),
                sizeof(ClosedPartitionProjectedArc)));
        for (const ClosedProjectionEntry& arc : projection) {
            add_bytes(
                projected_output,
                estimate_stable_key_bytes(arc.label));
        }
        std::uint64_t output_peak = class_base;
        add_bytes(
            output_peak,
            estimate_closed_projection_memory(projection));
        add_bytes(output_peak, projected_output);
        if (!check_closed_partition_memory(
                result, limits, output_peak)) {
            return result;
        }
        ClosedPartitionClass output;
        output.class_id = class_id;
        output.observation_key =
            closed_observation_key(canonical, authority);
        output.immediate_key =
            closed_immediate_key(canonical, authority);
        output.terminal = authority.terminal;
        output.member_keys.reserve(members[class_id].size());
        output.arcs.reserve(projection.size());
        for (const std::uint32_t member : members[class_id]) {
            output.member_keys.push_back(
                canonical[member].stable_key);
            std::uint64_t live = class_base;
            add_bytes(
                live,
                estimate_closed_projection_memory(projection));
            add_bytes(
                live,
                estimate_closed_partition_class_memory(output));
            if (!check_closed_partition_memory(
                    result, limits, live)) {
                return result;
            }
        }
        for (const ClosedProjectionEntry& arc : projection) {
            output.arcs.push_back({
                arc.label, arc.successor_class,
                arc.probability.value()});
            std::uint64_t live = class_base;
            add_bytes(
                live,
                estimate_closed_projection_memory(projection));
            add_bytes(
                live,
                estimate_closed_partition_class_memory(output));
            if (!check_closed_partition_memory(
                    result, limits, live)) {
                return result;
            }
        }
        result.classes.push_back(std::move(output));
        class_base = canonical_memory;
        add_bytes(
            class_base,
            estimate_closed_partition_result_memory(result));
        add_bytes(
            class_base,
            saturated_product(
                initial.capacity(), sizeof(std::uint32_t)));
        add_bytes(
            class_base,
            saturated_product(
                partition.capacity(), sizeof(std::uint32_t)));
        add_bytes(
            class_base,
            saturated_product(
                member_counts.capacity(), sizeof(std::size_t)));
        add_bytes(
            class_base,
            estimate_closed_members_memory(members));
        add_bytes(
            class_base,
            estimate_closed_projection_memory(projection));
        if (!check_closed_partition_memory(
                result, limits, class_base)) {
            return result;
        }
    }

    std::uint64_t mapping_peak = canonical_memory;
    add_bytes(
        mapping_peak,
        estimate_closed_partition_result_memory(result));
    add_bytes(
        mapping_peak,
        saturated_product(
            initial.capacity(), sizeof(std::uint32_t)));
    add_bytes(
        mapping_peak,
        saturated_product(
            partition.capacity(), sizeof(std::uint32_t)));
    add_bytes(
        mapping_peak,
        saturated_product(
            member_counts.capacity(), sizeof(std::size_t)));
    add_bytes(
        mapping_peak,
        estimate_closed_members_memory(members));
    const std::uint64_t one_mapping =
        saturated_product(
            canonical.size(), sizeof(std::uint32_t));
    add_bytes(mapping_peak, one_mapping);
    add_bytes(mapping_peak, one_mapping);
    if (!check_closed_partition_memory(
            result, limits, mapping_peak)) {
        return result;
    }
    result.initial_class_by_node.resize(canonical.size());
    result.class_by_node.resize(canonical.size());
    for (std::uint32_t node = 0;
         node < canonical.size(); ++node) {
        const std::uint32_t original =
            canonical[node].original_index;
        result.initial_class_by_node[original] = initial[node];
        result.class_by_node[original] = partition[node];
    }
    result.final_class_count = class_count;
    result.status = ClosedPartitionStatus::Complete;
    result.lumpable = true;
    check_closed_partition_memory(
        result, limits,
        estimate_closed_partition_result_memory(result));
    return result;
}

} // namespace refinement
} // namespace solver
} // namespace poecraft
