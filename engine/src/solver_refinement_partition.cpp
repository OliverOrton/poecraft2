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

ClosedPartitionResult refine_closed_probabilistic_partition_replay(
        const std::uint32_t node_count,
        const ClosedPartitionReplayFunction& replay,
        const std::vector<std::uint32_t>& previous_partition,
        const bool retain_member_keys,
        const ClosedPartitionLimits limits) {
    ClosedPartitionResult result;
    if (node_count == 0) {
        result.failure_reason =
            "replay-backed closed partition graph is empty";
        return result;
    }
    if (!replay ||
        (!previous_partition.empty() &&
         previous_partition.size() != node_count) ||
        !std::isfinite(limits.probability_sum_tolerance) ||
        limits.probability_sum_tolerance < 0.0) {
        result.status = ClosedPartitionStatus::InvalidGraph;
        result.failure_reason =
            "replay-backed closed partition has invalid authority";
        return result;
    }

    const auto fail_memory = [&](const std::uint64_t live) {
        result.peak_estimated_memory_bytes = std::max(
            result.peak_estimated_memory_bytes, live);
        if (live <= limits.max_estimated_memory_bytes) return false;
        result.status = ClosedPartitionStatus::ResourceCap;
        result.resource_cap = "max_estimated_memory_bytes";
        result.failure_reason =
            "replay-backed closed partition reached memory cap";
        return true;
    };
    const std::uint64_t dense_bytes = saturated_add(
        limits.retained_estimated_memory_bytes,
        saturated_product(
            node_count,
            3 * sizeof(std::uint32_t) + sizeof(std::uint64_t)));
    if (fail_memory(dense_bytes)) return result;

    const auto canonical_node = [&](const std::uint32_t index) {
        ClosedPartitionNode input = replay(index);
        if (input.stable_key.empty() || input.observation_key.empty() ||
            input.immediate_key.empty() || input.arc_source.has_value() ||
            input.observation_source.has_value() ||
            input.immediate_source.has_value()) {
            throw std::invalid_argument(
                "replayed closed node has invalid identity authority");
        }
        if (input.terminal && !input.arcs.empty()) {
            throw std::invalid_argument(
                "replayed terminal closed node has outgoing arcs");
        }
        using ArcKey =
            std::pair<StableKey, std::optional<std::uint32_t>>;
        std::map<ArcKey, DeterministicSum> accumulated;
        DeterministicSum total;
        for (const ClosedPartitionArc& arc : input.arcs) {
            if (!std::isfinite(arc.probability) ||
                arc.probability < 0.0 ||
                (arc.successor.has_value() &&
                 *arc.successor >= node_count)) {
                throw std::invalid_argument(
                    "replayed closed node has an invalid arc");
            }
            if (arc.probability == 0.0) continue;
            accumulated[{arc.label, arc.successor}].add(arc.probability);
            total.add(arc.probability);
        }
        if (!input.terminal &&
            (accumulated.empty() ||
             std::fabs(total.value() - 1.0) >
                 limits.probability_sum_tolerance)) {
            throw std::invalid_argument(
                "replayed closed node row is not stochastic");
        }
        CanonicalClosedNode out;
        out.original_index = index;
        out.stable_key = std::move(input.stable_key);
        out.observation_key = std::move(input.observation_key);
        out.immediate_key = std::move(input.immediate_key);
        out.terminal = input.terminal;
        out.arcs.reserve(accumulated.size());
        for (auto& [key, probability] : accumulated) {
            out.arcs.push_back({
                std::move(key.first), key.second, probability});
        }
        return out;
    };
    const auto projection = [&](const CanonicalClosedNode& node,
                                const std::vector<std::uint32_t>& partition) {
        using Key =
            std::pair<StableKey, std::optional<std::uint32_t>>;
        std::map<Key, DeterministicSum> accumulated;
        for (const CanonicalClosedArc& arc : node.arcs) {
            accumulated[{
                arc.label,
                arc.successor.has_value()
                    ? std::optional<std::uint32_t>{
                          partition.at(*arc.successor)}
                    : std::nullopt}].add(arc.probability);
        }
        std::vector<ClosedProjectionEntry> projected;
        projected.reserve(accumulated.size());
        for (auto& [key, probability] : accumulated) {
            projected.push_back({
                std::move(key.first), key.second, probability});
        }
        return projected;
    };
    const auto partition_pass =
            [&](const std::vector<std::uint32_t>* current,
               const bool initial,
               std::vector<std::uint32_t>& assigned,
               std::uint32_t& class_count) {
        std::map<ClosedPartitionKey, std::uint32_t> temporary;
        assigned.assign(node_count, 0);
        std::uint64_t key_bytes = 0;
        for (std::uint32_t node = 0; node < node_count; ++node) {
            CanonicalClosedNode canonical = canonical_node(node);
            ClosedPartitionKey key;
            if (initial) {
                if (!previous_partition.empty()) {
                    key.push_back(previous_partition[node]);
                }
                key.push_back(canonical.terminal ? 1u : 0u);
                append_tokens(key, canonical.observation_key);
            } else {
                const std::vector<ClosedProjectionEntry> projected =
                    projection(canonical, *current);
                key = closed_refined_key(
                    std::vector<CanonicalClosedNode>{canonical},
                    canonical, (*current)[node], projected);
            }
            auto [found, inserted] = temporary.emplace(
                std::move(key),
                static_cast<std::uint32_t>(temporary.size()));
            assigned[node] = found->second;
            if (inserted) {
                key_bytes = saturated_add(
                    key_bytes,
                    saturated_product(
                        found->first.capacity(), sizeof(std::uint64_t)));
                key_bytes = saturated_add(
                    key_bytes,
                    sizeof(decltype(temporary)::value_type) +
                        3 * sizeof(void*));
                if (temporary.size() > limits.max_classes ||
                    fail_memory(saturated_add(dense_bytes, key_bytes))) {
                    if (result.status != ClosedPartitionStatus::ResourceCap) {
                        result.status = ClosedPartitionStatus::ResourceCap;
                        result.resource_cap = "max_classes";
                        result.failure_reason =
                            "replay-backed closed partition reached max_classes";
                    }
                    return false;
                }
            }
        }
        std::vector<std::uint32_t> canonical_id(temporary.size());
        std::uint32_t next = 0;
        for (const auto& [unused, temporary_id] : temporary) {
            (void)unused;
            canonical_id[temporary_id] = next++;
        }
        for (std::uint32_t& value : assigned) {
            value = canonical_id[value];
        }
        class_count = next;
        return true;
    };

    try {
        std::vector<std::uint32_t> partition;
        std::uint32_t class_count = 0;
        if (!partition_pass(
                nullptr, true, partition, class_count)) {
            return result;
        }
        result.initial_class_by_node = partition;
        result.initial_class_count = class_count;
        result.final_class_count = class_count;
        for (;;) {
            if (result.rounds >= limits.max_rounds) {
                result.status = ClosedPartitionStatus::RefinementRoundCap;
                result.failure_reason =
                    "replay-backed closed partition reached max_rounds";
                return result;
            }
            std::vector<std::uint32_t> next;
            std::uint32_t next_count = 0;
            if (!partition_pass(
                    &partition, false, next, next_count)) {
                return result;
            }
            ++result.rounds;
            if (next_count < class_count) {
                result.status = ClosedPartitionStatus::NonLumpable;
                result.failure_reason =
                    "replay-backed closed partition violated split-only refinement";
                return result;
            }
            result.final_class_count = next_count;
            if (next == partition) break;
            partition = std::move(next);
            class_count = next_count;
        }
        result.class_by_node = partition;

        std::vector<std::uint32_t> representatives(
            result.final_class_count, kNoId);
        std::vector<std::uint32_t> member_counts(
            result.final_class_count, 0);
        for (std::uint32_t node = 0; node < node_count; ++node) {
            const std::uint32_t cls = partition[node];
            if (representatives[cls] == kNoId) representatives[cls] = node;
            ++member_counts[cls];
        }
        result.classes.resize(result.final_class_count);
        for (std::uint32_t cls = 0;
             cls < result.final_class_count; ++cls) {
            CanonicalClosedNode authority =
                canonical_node(representatives[cls]);
            const std::vector<ClosedProjectionEntry> projected =
                projection(authority, partition);
            ClosedPartitionClass& output = result.classes[cls];
            output.class_id = cls;
            output.observation_key = authority.observation_key;
            output.immediate_key = authority.immediate_key;
            output.terminal = authority.terminal;
            for (const ClosedProjectionEntry& arc : projected) {
                output.arcs.push_back({
                    arc.label, arc.successor_class,
                    arc.probability.value()});
            }
            if (retain_member_keys) {
                output.member_keys.reserve(member_counts[cls]);
            }
        }
        for (std::uint32_t node = 0; node < node_count; ++node) {
            const std::uint32_t cls = partition[node];
            CanonicalClosedNode candidate = canonical_node(node);
            const ClosedPartitionClass& authority = result.classes[cls];
            const std::vector<ClosedProjectionEntry> projected =
                projection(candidate, partition);
            std::vector<ClosedPartitionProjectedArc> candidate_arcs;
            candidate_arcs.reserve(projected.size());
            for (const ClosedProjectionEntry& arc : projected) {
                candidate_arcs.push_back({
                    arc.label, arc.successor_class,
                    arc.probability.value()});
            }
            ++result.lumpability_checks;
            if (candidate.terminal != authority.terminal ||
                candidate.observation_key != authority.observation_key ||
                candidate.immediate_key != authority.immediate_key ||
                candidate_arcs != authority.arcs) {
                result.status = ClosedPartitionStatus::NonLumpable;
                result.failure_reason =
                    "replay-backed closed partition failed final proof";
                return result;
            }
            if (retain_member_keys) {
                result.classes[cls].member_keys.push_back(
                    std::move(candidate.stable_key));
            }
        }
        result.estimated_memory_bytes = saturated_add(
            dense_bytes,
            estimate_closed_partition_result_memory(result));
        if (fail_memory(result.estimated_memory_bytes)) return result;
        result.status = ClosedPartitionStatus::Complete;
        result.lumpable = true;
        return result;
    } catch (const std::exception& error) {
        result.status = ClosedPartitionStatus::InvalidGraph;
        result.failure_reason = error.what();
        return result;
    }
}

} // namespace refinement
} // namespace solver
} // namespace poecraft
