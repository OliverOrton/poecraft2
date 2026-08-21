#pragma once

#include "solver_condition_expr.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace poecraft {
namespace solver {

/* Provenance carried only by compiler-generated siblings. Equal feature ids
 * and different values prove mutual exclusion independently of the opaque v1
 * predicate encoding. */
struct PolicyRoutePartitionValue {
    std::size_t feature = std::numeric_limits<std::size_t>::max();
    std::uint64_t value = 0;
};

struct PolicyRouteEdge {
    std::string to;
    ConditionExpr condition;
    PolicyRoutePartitionValue partition;
    bool structurally_disjoint = false;
};

struct PolicyRouteNode {
    std::string id;
    std::vector<PolicyRouteEdge> edges;
};

struct PolicyRouteBranch {
    std::string to;
    ConditionExpr guard;
};

/* Coalesce only a complete sibling set whose origin proves that no two edge
 * predicates can match the same item. First target occurrence determines the
 * stable output order; every full old predicate is retained under `any` and
 * the caller retains the unchanged default edge. */
inline std::vector<PolicyRouteEdge> coalesce_disjoint_policy_route_edges(
    std::vector<PolicyRouteEdge> edges) {
    if (edges.size() < 2) return edges;
    const std::size_t feature = edges.front().partition.feature;
    std::set<std::uint64_t> values;
    for (const PolicyRouteEdge& edge : edges) {
        if (!edge.structurally_disjoint ||
            edge.partition.feature != feature ||
            !values.insert(edge.partition.value).second) {
            return edges;
        }
    }

    std::map<std::string, std::size_t> output_by_target;
    std::vector<PolicyRouteEdge> result;
    result.reserve(edges.size());
    for (PolicyRouteEdge& edge : edges) {
        const auto found = output_by_target.find(edge.to);
        if (found == output_by_target.end()) {
            output_by_target.emplace(edge.to, result.size());
            result.push_back(std::move(edge));
            continue;
        }
        PolicyRouteEdge& retained = result.at(found->second);
        retained.condition = ConditionExpr::any({
            retained.condition, edge.condition});
    }
    return result;
}

/* Ordered first-match semantics also permit a second exact rewrite that does
 * not need mutual-exclusion provenance: a consecutive run of equal targets
 * can become one `any` at the run's first priority. Nothing can observe which
 * equal-target member won, and no different-target edge moves across it. */
inline std::vector<PolicyRouteEdge> coalesce_priority_safe_policy_route_edges(
    std::vector<PolicyRouteEdge> edges) {
    if (edges.size() < 2) return edges;
    bool complete_disjoint_partition = true;
    const std::size_t feature = edges.front().partition.feature;
    std::set<std::uint64_t> values;
    for (const PolicyRouteEdge& edge : edges) {
        complete_disjoint_partition =
            complete_disjoint_partition &&
            edge.structurally_disjoint &&
            edge.partition.feature == feature &&
            values.insert(edge.partition.value).second;
    }
    if (complete_disjoint_partition) {
        return coalesce_disjoint_policy_route_edges(std::move(edges));
    }

    std::vector<PolicyRouteEdge> result;
    result.reserve(edges.size());
    for (PolicyRouteEdge& edge : edges) {
        if (result.empty() || result.back().to != edge.to) {
            result.push_back(std::move(edge));
            continue;
        }
        result.back().condition = ConditionExpr::any({
            result.back().condition, edge.condition});
    }
    return result;
}

inline std::string policy_route_signature(
    const std::vector<PolicyRouteEdge>& edges) {
    std::string signature;
    for (const PolicyRouteEdge& edge : edges) {
        signature += std::to_string(edge.to.size()) + ":" + edge.to +
                     ":" + std::to_string(edge.condition.json().size()) +
                     ":" + edge.condition.json() + ";";
    }
    return signature;
}

} // namespace solver
} // namespace poecraft
