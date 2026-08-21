#pragma once

#include "solver_condition_expr.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
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

enum class PolicyRouteDomainMode {
    StopAtUniformTarget,
    ProveRepresentedDomain,
};

/* Shared reduced multi-valued decision-DAG construction. Callbacks preserve
 * the semantic differences between exact-policy and refined-parent routing:
 * the former may stop at a uniform continuation, while the latter keeps
 * testing every varying feature to prove the represented domain. */
template <
    typename Entry,
    typename ValueFn,
    typename TargetFn,
    typename ConditionFn,
    typename MakeNodeFn>
PolicyRouteBranch build_policy_decision_dag(
    const std::vector<Entry>& root_entries,
    const std::vector<std::size_t>& root_features,
    const PolicyRouteDomainMode mode,
    ValueFn value_of,
    TargetFn target_of,
    ConditionFn condition_of,
    MakeNodeFn make_node,
    const char* empty_error,
    const char* conflict_error) {
    std::function<PolicyRouteBranch(
        const std::vector<Entry>&,
        const std::vector<std::size_t>&)> build;
    build = [&](const std::vector<Entry>& entries,
                const std::vector<std::size_t>& features)
        -> PolicyRouteBranch {
        if (entries.empty()) {
            throw std::runtime_error(
                std::string("policy compile: ") + empty_error);
        }
        const std::string first_target = target_of(entries.front());
        const bool one_target = std::all_of(
            entries.begin() + 1, entries.end(),
            [&](const Entry& entry) {
                return target_of(entry) == first_target;
            });
        if (mode == PolicyRouteDomainMode::StopAtUniformTarget &&
            one_target) {
            return {first_target, ConditionExpr::always()};
        }

        std::vector<ConditionExpr> constants;
        std::vector<std::size_t> varying;
        varying.reserve(features.size());
        for (const std::size_t feature : features) {
            const std::uint64_t first =
                static_cast<std::uint64_t>(
                    value_of(entries.front(), feature));
            const bool constant = std::all_of(
                entries.begin() + 1, entries.end(),
                [&](const Entry& entry) {
                    return static_cast<std::uint64_t>(
                               value_of(entry, feature)) == first;
                });
            if (constant) {
                constants.push_back(
                    condition_of(entries.front(), feature));
            } else {
                varying.push_back(feature);
            }
        }
        const ConditionExpr guard =
            ConditionExpr::all(std::move(constants));
        if (varying.empty()) {
            if (!one_target) {
                throw std::runtime_error(
                    std::string("policy compile: ") + conflict_error);
            }
            return {first_target, guard};
        }

        std::size_t selected = varying.front();
        std::size_t selected_distinct = 0;
        std::uint64_t selected_square_sum =
            std::numeric_limits<std::uint64_t>::max();
        for (const std::size_t feature : varying) {
            std::map<std::uint64_t, std::uint32_t> counts;
            for (const Entry& entry : entries) {
                ++counts[static_cast<std::uint64_t>(
                    value_of(entry, feature))];
            }
            std::uint64_t square_sum = 0;
            for (const auto& [unused_value, count] : counts) {
                (void)unused_value;
                square_sum +=
                    static_cast<std::uint64_t>(count) * count;
            }
            if (counts.size() > selected_distinct ||
                (counts.size() == selected_distinct &&
                 square_sum < selected_square_sum)) {
                selected = feature;
                selected_distinct = counts.size();
                selected_square_sum = square_sum;
            }
        }

        std::vector<std::size_t> child_features;
        child_features.reserve(varying.size() - 1);
        for (const std::size_t feature : varying) {
            if (feature != selected) child_features.push_back(feature);
        }
        std::map<std::uint64_t, std::vector<Entry>> groups;
        for (const Entry& entry : entries) {
            groups[static_cast<std::uint64_t>(
                value_of(entry, selected))]
                .push_back(entry);
        }
        std::vector<PolicyRouteEdge> edges;
        edges.reserve(groups.size());
        for (auto& [value, members] : groups) {
            const PolicyRouteBranch child =
                build(members, child_features);
            edges.push_back({
                child.to,
                ConditionExpr::all({
                    condition_of(members.front(), selected),
                    child.guard}),
                {selected, value},
                true});
        }
        edges = coalesce_priority_safe_policy_route_edges(
            std::move(edges));
        return {make_node(std::move(edges)), guard};
    };
    return build(root_entries, root_features);
}

} // namespace solver
} // namespace poecraft
