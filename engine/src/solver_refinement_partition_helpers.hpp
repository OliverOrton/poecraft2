#pragma once

#include "solver_refinement_observation_helpers.hpp"

namespace poecraft {
namespace solver {
namespace refinement {
namespace {

struct DeterministicSum {
    double high = 0.0;
    double low = 0.0;

    void add(const double value) {
        const double sum = high + value;
        const double correction =
            std::abs(high) >= std::abs(value)
                ? (high - sum) + value
                : (value - sum) + high;
        high = sum;
        low += correction;
        const double normalized = high + low;
        const double tail =
            std::abs(high) >= std::abs(low)
                ? (high - normalized) + low
                : (low - normalized) + high;
        high = normalized;
        low = tail;
    }

    void add(const DeterministicSum value) {
        add(value.high);
        add(value.low);
    }

    double value() const {
        return high + low;
    }

    bool operator==(const DeterministicSum&) const = default;
};

struct CanonicalClosedArc {
    StableKey label;
    std::optional<std::uint32_t> successor;
    DeterministicSum probability;

    bool operator==(const CanonicalClosedArc&) const = default;
};

struct CanonicalClosedNode {
    std::uint32_t original_index = 0;
    StableKey stable_key;
    StableKey observation_key;
    StableKey immediate_key;
    bool terminal = false;
    std::vector<CanonicalClosedArc> arcs;
    std::optional<std::uint32_t> arc_source;
    std::optional<std::uint32_t> observation_source;
    std::optional<std::uint32_t> immediate_source;
};

struct ClosedProjectionEntry {
    StableKey label;
    std::optional<std::uint32_t> successor_class;
    DeterministicSum probability;

    bool operator==(const ClosedProjectionEntry&) const = default;
};

using ClosedPartitionKey = std::vector<std::uint64_t>;

std::vector<ClosedProjectionEntry> project_closed_row(
        const std::vector<CanonicalClosedNode>& graph,
        const CanonicalClosedNode& node,
        const std::vector<std::uint32_t>& partition) {
    using ProjectionKey =
        std::pair<StableKey, std::optional<std::uint32_t>>;
    std::map<ProjectionKey, DeterministicSum> projected;
    const std::vector<CanonicalClosedArc>& arcs =
        node.arc_source.has_value()
            ? graph.at(*node.arc_source).arcs
            : node.arcs;
    for (const CanonicalClosedArc& arc : arcs) {
        const std::optional<std::uint32_t> successor_class =
            arc.successor.has_value()
                ? std::optional<std::uint32_t>{
                      partition.at(*arc.successor)}
                : std::nullopt;
        projected[
            ProjectionKey{arc.label, successor_class}].add(
                arc.probability);
    }
    std::vector<ClosedProjectionEntry> out;
    out.reserve(projected.size());
    for (auto& [key, probability] : projected) {
        out.push_back({
            std::move(key.first), key.second, probability});
    }
    return out;
}

const StableKey& closed_observation_key(
        const std::vector<CanonicalClosedNode>& graph,
        const CanonicalClosedNode& node) {
    return node.observation_source.has_value()
               ? graph.at(*node.observation_source).observation_key
               : node.observation_key;
}

const StableKey& closed_immediate_key(
        const std::vector<CanonicalClosedNode>& graph,
        const CanonicalClosedNode& node) {
    return node.immediate_source.has_value()
               ? graph.at(*node.immediate_source).immediate_key
               : node.immediate_key;
}

ClosedPartitionKey closed_initial_key(
        const std::vector<CanonicalClosedNode>& graph,
        const CanonicalClosedNode& node) {
    ClosedPartitionKey out{node.terminal ? 1u : 0u};
    append_tokens(out, closed_observation_key(graph, node));
    return out;
}

ClosedPartitionKey closed_refined_key(
        const std::vector<CanonicalClosedNode>& graph,
        const CanonicalClosedNode& node,
        const std::uint32_t current_class,
        const std::vector<ClosedProjectionEntry>& projected) {
    ClosedPartitionKey out{current_class};
    append_tokens(out, closed_immediate_key(graph, node));
    out.push_back(projected.size());
    for (const ClosedProjectionEntry& arc : projected) {
        append_tokens(out, arc.label);
        out.push_back(arc.successor_class.has_value() ? 1u : 0u);
        if (arc.successor_class.has_value()) {
            out.push_back(*arc.successor_class);
        }
        out.push_back(
            std::bit_cast<std::uint64_t>(arc.probability.high));
        out.push_back(
            std::bit_cast<std::uint64_t>(arc.probability.low));
    }
    return out;
}

bool canonicalize_closed_partition_graph(
        std::vector<ClosedPartitionNode> input,
        const ClosedPartitionLimits& limits,
        std::vector<CanonicalClosedNode>& output,
        std::string& failure_reason) {
    if (!std::isfinite(limits.probability_sum_tolerance) ||
        limits.probability_sum_tolerance < 0.0) {
        failure_reason =
            "closed partition has an invalid probability tolerance";
        return false;
    }
    std::vector<std::uint32_t> order(input.size());
    for (std::uint32_t index = 0; index < order.size(); ++index) {
        order[index] = index;
    }
    std::sort(
        order.begin(), order.end(),
        [&](const std::uint32_t left, const std::uint32_t right) {
            return input[left].stable_key <
                   input[right].stable_key;
        });
    std::vector<std::uint32_t> remap(input.size());
    for (std::uint32_t canonical = 0;
         canonical < order.size(); ++canonical) {
        const ClosedPartitionNode& node = input[order[canonical]];
        if (node.stable_key.empty()) {
            failure_reason =
                "closed partition has an empty stable node key";
            return false;
        }
        if (canonical > 0 &&
            input[order[canonical - 1]].stable_key ==
                node.stable_key) {
            failure_reason =
                "closed partition has duplicate stable node keys";
            return false;
        }
        remap[order[canonical]] = canonical;
    }
    for (std::uint32_t index = 0; index < input.size(); ++index) {
        const ClosedPartitionNode& node = input[index];
        const auto valid_source =
            [&](const std::optional<std::uint32_t> source,
                const auto member) {
                return !source.has_value() ||
                       (*source < input.size() &&
                        *source != index &&
                        !(input[*source].*member).has_value());
            };
        if (!valid_source(
                node.arc_source,
                &ClosedPartitionNode::arc_source) ||
            (node.arc_source.has_value() && !node.arcs.empty())) {
            failure_reason =
                "closed partition has an invalid shared arc authority";
            return false;
        }
        if (!valid_source(
                node.observation_source,
                &ClosedPartitionNode::observation_source) ||
            (node.observation_source.has_value() &&
             !node.observation_key.empty())) {
            failure_reason =
                "closed partition has an invalid shared observation "
                "authority";
            return false;
        }
        if (!valid_source(
                node.immediate_source,
                &ClosedPartitionNode::immediate_source) ||
            (node.immediate_source.has_value() &&
             !node.immediate_key.empty())) {
            failure_reason =
                "closed partition has an invalid shared immediate "
                "authority";
            return false;
        }
    }

    struct PendingArc {
        StableKey label;
        std::optional<std::uint32_t> successor;
        double probability = 0.0;
    };
    const auto pending_less =
        [](const PendingArc& left, const PendingArc& right) {
            if (left.label != right.label) {
                return left.label < right.label;
            }
            if (left.successor != right.successor) {
                return left.successor < right.successor;
            }
            return std::bit_cast<std::uint64_t>(
                       left.probability) <
                   std::bit_cast<std::uint64_t>(
                       right.probability);
        };

    output.reserve(input.size());
    for (std::uint32_t canonical = 0;
         canonical < order.size(); ++canonical) {
        const std::uint32_t original = order[canonical];
        ClosedPartitionNode& source = input[original];
        if (source.terminal &&
            (!source.arcs.empty() || source.arc_source.has_value())) {
            failure_reason =
                "closed partition terminal node has outgoing arcs";
            return false;
        }
        std::vector<PendingArc> pending;
        pending.reserve(source.arcs.size());
        for (ClosedPartitionArc& arc : source.arcs) {
            if (!std::isfinite(arc.probability) ||
                arc.probability < 0.0) {
                failure_reason =
                    "closed partition has an invalid probability";
                return false;
            }
            if (arc.successor.has_value() &&
                *arc.successor >= input.size()) {
                failure_reason =
                    "closed partition has an unknown successor";
                return false;
            }
            if (arc.probability == 0.0) continue;
            pending.push_back({
                std::move(arc.label),
                arc.successor.has_value()
                    ? std::optional<std::uint32_t>{
                          remap[*arc.successor]}
                    : std::nullopt,
                arc.probability});
        }
        std::sort(pending.begin(), pending.end(), pending_less);
        using ArcKey =
            std::pair<StableKey, std::optional<std::uint32_t>>;
        std::map<ArcKey, DeterministicSum> grouped;
        for (const PendingArc& arc : pending) {
            grouped[ArcKey{arc.label, arc.successor}].add(
                arc.probability);
        }
        CanonicalClosedNode node;
        node.original_index = original;
        node.stable_key = std::move(source.stable_key);
        node.observation_key = std::move(source.observation_key);
        node.immediate_key = std::move(source.immediate_key);
        node.terminal = source.terminal;
        if (source.arc_source.has_value()) {
            node.arc_source = remap[*source.arc_source];
        }
        if (source.observation_source.has_value()) {
            node.observation_source =
                remap[*source.observation_source];
        }
        if (source.immediate_source.has_value()) {
            node.immediate_source =
                remap[*source.immediate_source];
        }
        DeterministicSum total;
        node.arcs.reserve(grouped.size());
        for (auto& [key, probability] : grouped) {
            total.add(probability);
            node.arcs.push_back({
                std::move(key.first), key.second, probability});
        }
        if (!node.terminal && !node.arc_source.has_value() &&
            (node.arcs.empty() ||
             std::abs(total.value() - 1.0) >
                 limits.probability_sum_tolerance)) {
            failure_reason =
                "closed partition non-terminal row is not stochastic";
            return false;
        }
        output.push_back(std::move(node));
    }
    for (const CanonicalClosedNode& node : output) {
        if (node.arc_source.has_value() &&
            output[*node.arc_source].arcs.empty()) {
            failure_reason =
                "closed partition shared arc authority has no row";
            return false;
        }
        if (node.observation_source.has_value() &&
            output[*node.observation_source].observation_key.empty()) {
            failure_reason =
                "closed partition shared observation authority has no key";
            return false;
        }
        if (node.immediate_source.has_value() &&
            output[*node.immediate_source].immediate_key.empty()) {
            failure_reason =
                "closed partition shared immediate authority has no key";
            return false;
        }
    }
    return true;
}


} // namespace

} // namespace refinement
} // namespace solver
} // namespace poecraft
