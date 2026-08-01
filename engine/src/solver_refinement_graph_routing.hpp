#pragma once

#include "solver_refinement_graph_discovery.hpp"

namespace poecraft {
namespace solver {
namespace refinement {
namespace {

using BehaviorKey = std::vector<std::uint64_t>;

BehaviorKey initial_behavior_key(
        const Graph& graph,
        const Node& node,
        const FeatureSignature& observed) {
    BehaviorKey out;
    append_tokens(out, node_coarse_key(graph, node));
    out.push_back(node.state.goal ? 1u : 0u);
    out.push_back(node.state.terminal ? 1u : 0u);
    if (node.selection == nullptr) {
        out.push_back(0);
        return out;
    }
    out.push_back(1);
    out.push_back(node.selection->action_id);
    append_tokens(out, node.selection->semantic_key);
    append_requirement(out, node.required);
    out.push_back(observed.size());
    for (const FeatureAtom& atom : observed) append_atom(out, atom);
    return out;
}

bool same_selected_decision(
        const Node& left,
        const Node& right) {
    if ((left.selection != nullptr) !=
        (right.selection != nullptr)) {
        return false;
    }
    if (left.selection == nullptr) return true;
    return left.selection->action_id ==
               right.selection->action_id &&
           left.selection->semantic_key ==
               right.selection->semantic_key;
}

ObservationRequirement observation_for_atom(
        const FeatureAtom& atom) {
    ObservationRequirement requirement;
    const RefinementFeatureMask feature =
        refinement_feature(atom.feature);
    if (is_item_feature(atom.feature)) {
        requirement.item_features = feature;
        return requirement;
    }
    RefinementAffixSelector selector;
    selector.required_affix_traits =
        atom.affix_traits;
    selector.forbidden_affix_traits =
        static_cast<std::uint16_t>(
            kAllRefinementAffixTraits &
            ~atom.affix_traits);
    selector.required_item_traits =
        atom.item_traits;
    selector.forbidden_item_traits =
        static_cast<std::uint8_t>(
            kAllRefinementItemTraits &
            ~atom.item_traits);
    requirement.affix_observations.push_back(
        {feature, std::move(selector)});
    if (atom.feature ==
        RefinementFeature::ModifierClassificationTags) {
        requirement.modifier_tag_ids =
            atom.modifier_tag_ids;
    }
    return canonical_observation_requirement(
        std::move(requirement));
}

/*
 * A behavior partition may select different exact actions among the concrete
 * refinements of one policy-reachable coarse state. Existing policy control
 * flow already separates different coarse states, so comparing across coarse
 * parents would demand a fabricated router observation at points that can
 * never share a router.
 *
 * Routing predicates are class-local. A distinction needed between one pair
 * is added only to those two exact policy/state classes, then the ordinary
 * backward observation pass carries it through their predecessors. Applying
 * one graph-wide union would retain unrelated identity through actions that
 * cannot observe it and defeat destructive collapse.
 */
bool refine_selected_action_routing(
        Graph& graph,
        const RefinementLimits& limits,
        RefinementResult& result,
        const std::uint64_t adapter_memory) {
    std::vector<std::uint32_t> members;
    members.reserve(graph.nodes.size());
    for (std::uint32_t node = 0;
         node < graph.nodes.size(); ++node) {
        if (graph.nodes[node].state.terminal ||
            graph.nodes[node].selection == nullptr) {
            continue;
        }
        members.push_back(node);
    }
    /*
     * Keep the admitted/action-authored router requirement separate from
     * observations synthesized by exact policy lifting.  The latter may be
     * minimized below; the former is semantic authority and must never be
     * weakened merely because the current represented carrier does not need
     * all of it to distinguish its selected decisions.
     */
    std::vector<ObservationRequirement> base_requirements(
        graph.nodes.size());
    std::vector<ObservationRequirement> promoted_requirements(
        graph.nodes.size());
    for (const std::uint32_t node : members) {
        base_requirements[node] =
            graph.nodes[node].selection->routing_observes;
    }
    const auto routing_storage_memory = [&]() {
        std::uint64_t bytes = saturated_product(
            members.capacity(), sizeof(std::uint32_t));
        add_bytes(
            bytes,
            saturated_product(
                base_requirements.capacity(),
                sizeof(ObservationRequirement)));
        add_bytes(
            bytes,
            saturated_product(
                promoted_requirements.capacity(),
                sizeof(ObservationRequirement)));
        for (const ObservationRequirement& requirement :
             base_requirements) {
            add_bytes(bytes, estimate_requirement_bytes(requirement));
        }
        for (const ObservationRequirement& requirement :
             promoted_requirements) {
            add_bytes(bytes, estimate_requirement_bytes(requirement));
        }
        return bytes;
    };
    if (!check_refinement_memory(
            result, graph, limits, adapter_memory,
            routing_storage_memory())) {
        return false;
    }

    bool promoted = false;
    const auto route_matches =
        [](const Node& item, const Node& route,
           const ObservationRequirement& requirement) {
            return observe_features(
                       item.state.features, requirement) ==
                   observe_features(
                       route.state.features, requirement);
        };
    const auto routes_overlap =
        [&](const Node& left, const Node& right,
            const ObservationRequirement& left_requirement,
            const ObservationRequirement& right_requirement) {
            return route_matches(
                       left, right, right_requirement) ||
                   route_matches(
                       right, left, left_requirement);
        };
    const auto requirements_memory =
        [](const std::vector<ObservationRequirement>& requirements) {
            std::uint64_t bytes = saturated_product(
                requirements.capacity(),
                sizeof(ObservationRequirement));
            for (const ObservationRequirement& requirement :
                 requirements) {
                add_bytes(
                    bytes,
                    estimate_requirement_bytes(requirement));
            }
            return bytes;
        };
    std::size_t routing_parent_begin = 0;
    while (routing_parent_begin < members.size()) {
        std::size_t routing_parent_end = routing_parent_begin + 1;
        while (routing_parent_end < members.size() &&
               node_coarse_key(
                   graph,
                   graph.nodes[members[routing_parent_begin]]) ==
                   node_coarse_key(
                       graph,
                       graph.nodes[members[routing_parent_end]])) {
            ++routing_parent_end;
        }
        for (std::size_t left_index = routing_parent_begin;
             left_index < routing_parent_end; ++left_index) {
        for (std::size_t right_index = left_index + 1;
             right_index < routing_parent_end; ++right_index) {
            Node& left = graph.nodes[members[left_index]];
            Node& right = graph.nodes[members[right_index]];
            if (same_selected_decision(left, right) ||
                !routes_overlap(
                    left, right,
                    left.selection->routing_observes,
                    right.selection->routing_observes)) {
                continue;
            }
            if (!promoted) {
                if (result.telemetry
                        .selected_action_routing_rounds >=
                    limits.max_refinement_rounds) {
                    return refinement_round_cap(
                        result,
                        "selected-action routing reached "
                        "max_refinement_rounds");
                }
                ++result.telemetry
                      .selected_action_routing_rounds;
                promoted = true;
            }

            /*
             * A router may need a conjunction to observe correlation across
             * affix facts. For example, two states can have the same
             * exclusion-signature multiset and the same required-level
             * multiset but pair those facts on different affixes. Neither
             * feature alone distinguishes them; observing both does. Build
             * the composable vocabulary from the full union rather than the
             * raw symmetric difference, which can omit the correlation key.
             */
            std::uint64_t candidate_atom_peak =
                routing_storage_memory();
            add_bytes(
                candidate_atom_peak,
                estimate_feature_bytes(left.state.features));
            add_bytes(
                candidate_atom_peak,
                estimate_feature_bytes(right.state.features));
            if (!check_refinement_memory(
                    result, graph, limits,
                    adapter_memory, candidate_atom_peak)) {
                return false;
            }
            FeatureSignature candidate_atoms;
            candidate_atoms.reserve(
                left.state.features.size() +
                right.state.features.size());
            std::set_union(
                left.state.features.begin(),
                left.state.features.end(),
                right.state.features.begin(),
                right.state.features.end(),
                std::back_inserter(candidate_atoms),
                atom_less);
            std::uint64_t candidate_requirement_peak =
                routing_storage_memory();
            add_bytes(
                candidate_requirement_peak,
                estimate_feature_bytes(candidate_atoms));
            add_bytes(
                candidate_requirement_peak,
                saturated_product(
                    candidate_atoms.size(),
                    sizeof(ObservationRequirement) +
                        sizeof(RefinementAffixObservation)));
            for (const FeatureAtom& atom : candidate_atoms) {
                add_bytes(
                    candidate_requirement_peak,
                    saturated_product(
                        atom.modifier_tag_ids.capacity(),
                        sizeof(std::uint32_t)));
            }
            if (!check_refinement_memory(
                    result, graph, limits,
                    adapter_memory, candidate_requirement_peak)) {
                return false;
            }
            std::vector<ObservationRequirement> candidates;
            candidates.reserve(candidate_atoms.size());
            for (const FeatureAtom& atom : candidate_atoms) {
                ObservationRequirement observation =
                    observation_for_atom(atom);
                if (std::find(
                        candidates.begin(), candidates.end(),
                        observation) == candidates.end()) {
                    candidates.push_back(std::move(observation));
                    std::uint64_t candidate_live =
                        routing_storage_memory();
                    add_bytes(
                        candidate_live,
                        estimate_feature_bytes(candidate_atoms));
                    add_bytes(
                        candidate_live,
                        requirements_memory(candidates));
                    if (!check_refinement_memory(
                            result, graph, limits,
                            adapter_memory, candidate_live)) {
                        return false;
                    }
                }
            }

            const ObservationRequirement left_base =
                left.selection->routing_observes;
            const ObservationRequirement right_base =
                right.selection->routing_observes;
            ObservationRequirement left_trial = left_base;
            ObservationRequirement right_trial = right_base;
            std::vector<ObservationRequirement> additions;
            std::uint64_t addition_storage_peak =
                routing_storage_memory();
            add_bytes(
                addition_storage_peak,
                estimate_feature_bytes(candidate_atoms));
            add_bytes(
                addition_storage_peak,
                requirements_memory(candidates));
            add_bytes(
                addition_storage_peak,
                saturated_product(
                    candidates.size(),
                    sizeof(ObservationRequirement)));
            add_bytes(
                addition_storage_peak,
                estimate_requirement_bytes(left_base));
            add_bytes(
                addition_storage_peak,
                estimate_requirement_bytes(right_base));
            add_bytes(
                addition_storage_peak,
                estimate_requirement_bytes(left_trial));
            add_bytes(
                addition_storage_peak,
                estimate_requirement_bytes(right_trial));
            if (!check_refinement_memory(
                    result, graph, limits,
                    adapter_memory, addition_storage_peak)) {
                return false;
            }
            additions.reserve(candidates.size());
            std::uint64_t additions_peak =
                routing_storage_memory();
            add_bytes(
                additions_peak,
                estimate_feature_bytes(candidate_atoms));
            add_bytes(
                additions_peak,
                requirements_memory(candidates));
            add_bytes(
                additions_peak,
                requirements_memory(additions));
            add_bytes(
                additions_peak,
                estimate_requirement_bytes(left_base));
            add_bytes(
                additions_peak,
                estimate_requirement_bytes(right_base));
            add_bytes(
                additions_peak,
                estimate_requirement_bytes(left_trial));
            add_bytes(
                additions_peak,
                estimate_requirement_bytes(right_trial));
            if (!check_refinement_memory(
                    result, graph, limits,
                    adapter_memory, additions_peak)) {
                return false;
            }
            bool distinguished = false;
            for (const ObservationRequirement& observation : candidates) {
                ObservationRequirement next_left =
                    merge_requirements(left_trial, observation);
                ObservationRequirement next_right =
                    merge_requirements(right_trial, observation);
                if (next_left == left_trial &&
                    next_right == right_trial) {
                    continue;
                }
                std::uint64_t iteration_peak = additions_peak;
                add_bytes(
                    iteration_peak,
                    estimate_requirement_bytes(next_left));
                add_bytes(
                    iteration_peak,
                    estimate_requirement_bytes(next_right));
                add_bytes(
                    iteration_peak,
                    estimate_requirement_bytes(observation));
                if (!check_refinement_memory(
                        result, graph, limits,
                        adapter_memory, iteration_peak)) {
                    return false;
                }
                left_trial = std::move(next_left);
                right_trial = std::move(next_right);
                additions.push_back(observation);
                additions_peak = routing_storage_memory();
                add_bytes(
                    additions_peak,
                    estimate_feature_bytes(candidate_atoms));
                add_bytes(
                    additions_peak,
                    requirements_memory(candidates));
                add_bytes(
                    additions_peak,
                    requirements_memory(additions));
                add_bytes(
                    additions_peak,
                    estimate_requirement_bytes(left_base));
                add_bytes(
                    additions_peak,
                    estimate_requirement_bytes(right_base));
                add_bytes(
                    additions_peak,
                    estimate_requirement_bytes(left_trial));
                add_bytes(
                    additions_peak,
                    estimate_requirement_bytes(right_trial));
                if (!check_refinement_memory(
                        result, graph, limits,
                        adapter_memory, additions_peak)) {
                    return false;
                }
                if (!routes_overlap(
                        left, right, left_trial, right_trial)) {
                    distinguished = true;
                    break;
                }
            }
            std::uint64_t live_scratch =
                routing_storage_memory();
            add_bytes(
                live_scratch,
                estimate_feature_bytes(candidate_atoms));
            add_bytes(
                live_scratch,
                requirements_memory(candidates));
            add_bytes(
                live_scratch,
                requirements_memory(additions));
            add_bytes(
                live_scratch,
                estimate_requirement_bytes(left_base));
            add_bytes(
                live_scratch,
                estimate_requirement_bytes(right_base));
            add_bytes(
                live_scratch,
                estimate_requirement_bytes(left_trial));
            add_bytes(
                live_scratch,
                estimate_requirement_bytes(right_trial));
            /* Accumulation and pruning each hold their next left/right
             * requirement pair alongside the retained full trials. The full
             * conjunction is a conservative bound for every prefix/subset.
             */
            add_bytes(
                live_scratch,
                estimate_requirement_bytes(left_trial));
            add_bytes(
                live_scratch,
                estimate_requirement_bytes(right_trial));
            if (!check_refinement_memory(
                    result, graph, limits,
                    adapter_memory, live_scratch)) {
                return false;
            }
            if (!distinguished) {
                return fail(
                    result,
                    RefinementStatus::InvalidContract,
                    "different exact policy actions have no observable "
                    "distinguishing feature");
            }

            /* Remove every redundant promoted term while retaining a
             * deterministic subset-minimal conjunction. Reverse order keeps
             * the earliest semantic vocabulary term on equivalent choices.
             */
            for (std::size_t remove = additions.size();
                 remove-- > 0;) {
                ObservationRequirement pruned_left = left_base;
                ObservationRequirement pruned_right = right_base;
                for (std::size_t term = 0;
                     term < additions.size(); ++term) {
                    if (term == remove) continue;
                    pruned_left = merge_requirements(
                        std::move(pruned_left), additions[term]);
                    pruned_right = merge_requirements(
                        std::move(pruned_right), additions[term]);
                }
                if (!routes_overlap(
                        left, right,
                        pruned_left, pruned_right)) {
                    additions.erase(
                        additions.begin() +
                        static_cast<std::ptrdiff_t>(remove));
                }
            }
            left_trial = left_base;
            right_trial = right_base;
            for (const ObservationRequirement& observation : additions) {
                left_trial = merge_requirements(
                    std::move(left_trial), observation);
                right_trial = merge_requirements(
                    std::move(right_trial), observation);
            }
            ObservationRequirement next_left_promoted =
                promoted_requirements[members[left_index]];
            ObservationRequirement next_right_promoted =
                promoted_requirements[members[right_index]];
            for (const ObservationRequirement& observation : additions) {
                next_left_promoted = merge_requirements(
                    std::move(next_left_promoted), observation);
                next_right_promoted = merge_requirements(
                    std::move(next_right_promoted), observation);
            }
            std::uint64_t commit_peak = routing_storage_memory();
            add_bytes(
                commit_peak,
                estimate_requirement_bytes(next_left_promoted));
            add_bytes(
                commit_peak,
                estimate_requirement_bytes(next_right_promoted));
            add_bytes(
                commit_peak,
                estimate_requirement_bytes(left_trial));
            add_bytes(
                commit_peak,
                estimate_requirement_bytes(right_trial));
            if (!check_refinement_memory(
                    result, graph, limits, adapter_memory,
                    commit_peak)) {
                return false;
            }
            promoted_requirements[members[left_index]] =
                std::move(next_left_promoted);
            promoted_requirements[members[right_index]] =
                std::move(next_right_promoted);
            detach_selection(left);
            detach_selection(right);
            left.selection->routing_observes =
                std::move(left_trial);
            right.selection->routing_observes =
                std::move(right_trial);
            if (!check_refinement_memory(
                    result, graph, limits, adapter_memory,
                    routing_storage_memory())) {
                return false;
            }
        }
        }
        routing_parent_begin = routing_parent_end;
    }

    /*
     * The full atom selectors above are a sound starting point, but their
     * exact trait cube also names incidental facts: an exclusion signature
     * observed on a crafted/Veiled/locked affix does not thereby make those
     * traits part of the policy decision.  Minimize only synthesized
     * requirements, one deterministic literal/term at a time.  A deletion is
     * committed only when every represented pair with different decisions in
     * the same semantic coarse parent remains mutually disjoint.
     */
    const auto parent_routes_disjoint =
        [&](const std::size_t begin, const std::size_t end,
            const std::optional<std::uint32_t> override_node =
                std::nullopt,
            const ObservationRequirement* override_requirement =
                nullptr) {
            for (std::size_t left_index = begin;
                 left_index < end; ++left_index) {
                for (std::size_t right_index = left_index + 1;
                     right_index < end; ++right_index) {
                    const std::uint32_t left_node =
                        members[left_index];
                    const std::uint32_t right_node =
                        members[right_index];
                    const Node& left = graph.nodes[left_node];
                    const Node& right = graph.nodes[right_node];
                    if (same_selected_decision(left, right)) continue;
                    const ObservationRequirement& left_requirement =
                        override_node == left_node
                            ? *override_requirement
                            : left.selection->routing_observes;
                    const ObservationRequirement& right_requirement =
                        override_node == right_node
                            ? *override_requirement
                            : right.selection->routing_observes;
                    if (routes_overlap(
                            left, right, left_requirement,
                            right_requirement)) {
                        return false;
                    }
                }
            }
            return true;
        };
    std::uint64_t route_comparison_scratch = 0;
    for (const std::uint32_t node : members) {
        route_comparison_scratch = std::max(
            route_comparison_scratch,
            estimate_feature_bytes(graph.nodes[node].state.features));
    }
    route_comparison_scratch = saturated_add(
        route_comparison_scratch, route_comparison_scratch);

    bool minimization_memory_failed = false;
    const auto try_promoted_requirement =
        [&](const std::uint32_t node,
            const std::size_t parent_begin,
            const std::size_t parent_end,
            ObservationRequirement trial) {
            trial = canonical_observation_requirement(
                std::move(trial));
            ObservationRequirement combined = merge_requirements(
                base_requirements[node], trial);
            std::uint64_t trial_peak = routing_storage_memory();
            add_bytes(
                trial_peak, estimate_requirement_bytes(trial));
            add_bytes(
                trial_peak, estimate_requirement_bytes(combined));
            add_bytes(trial_peak, route_comparison_scratch);
            if (!check_refinement_memory(
                    result, graph, limits, adapter_memory,
                    trial_peak)) {
                minimization_memory_failed = true;
                return false;
            }
            if (!parent_routes_disjoint(
                    parent_begin, parent_end, node, &combined)) {
                return false;
            }
            promoted_requirements[node] = std::move(trial);
            detach_selection(graph.nodes[node]);
            graph.nodes[node].selection->routing_observes =
                std::move(combined);
            if (!check_refinement_memory(
                    result, graph, limits, adapter_memory,
                    routing_storage_memory())) {
                minimization_memory_failed = true;
                return false;
            }
            return true;
        };

    std::size_t parent_begin = 0;
    while (parent_begin < members.size()) {
        std::size_t parent_end = parent_begin + 1;
        while (parent_end < members.size() &&
               node_coarse_key(
                   graph, graph.nodes[members[parent_begin]]) ==
                   node_coarse_key(
                       graph, graph.nodes[members[parent_end]])) {
            ++parent_end;
        }
        if (!parent_routes_disjoint(parent_begin, parent_end)) {
            return fail(
                result, RefinementStatus::InvalidContract,
                "selected-action router promotion did not separate "
                "all represented decisions in one coarse parent");
        }
        for (std::size_t member = parent_begin;
             member < parent_end; ++member) {
            const std::uint32_t node = members[member];
            for (;;) {
                bool removed = false;

                /* Item observation bits are promoted terms too. */
                for (int feature =
                         static_cast<int>(
                             RefinementFeature::ModifierSide) - 1;
                     feature >= 0; --feature) {
                    const RefinementFeatureMask bit =
                        RefinementFeatureMask{1} << feature;
                    if ((promoted_requirements[node].item_features &
                         bit) == 0) {
                        continue;
                    }
                    ObservationRequirement trial =
                        promoted_requirements[node];
                    trial.item_features &= ~bit;
                    if (try_promoted_requirement(
                            node, parent_begin, parent_end,
                            std::move(trial))) {
                        removed = true;
                        break;
                    }
                    if (minimization_memory_failed) return false;
                }
                if (removed) continue;

                /* Global tag vocabulary is meaningful only to promoted
                 * classification observations and can be reduced under the
                 * same represented-pair proof. */
                for (std::size_t tag =
                         promoted_requirements[node]
                             .modifier_tag_ids.size();
                     tag-- > 0;) {
                    ObservationRequirement trial =
                        promoted_requirements[node];
                    trial.modifier_tag_ids.erase(
                        trial.modifier_tag_ids.begin() +
                        static_cast<std::ptrdiff_t>(tag));
                    if (try_promoted_requirement(
                            node, parent_begin, parent_end,
                            std::move(trial))) {
                        removed = true;
                        break;
                    }
                    if (minimization_memory_failed) return false;
                }
                if (removed) continue;

                /* Remove individual feature terms before weakening their
                 * selector. Canonicalization removes an empty term and
                 * merges terms that become selector-equivalent. */
                for (std::size_t observation =
                         promoted_requirements[node]
                             .affix_observations.size();
                     observation-- > 0 && !removed;) {
                    for (int feature =
                             static_cast<int>(
                                 RefinementFeature::Count) - 1;
                         feature >= static_cast<int>(
                             RefinementFeature::ModifierSide);
                         --feature) {
                        const RefinementFeatureMask bit =
                            RefinementFeatureMask{1} << feature;
                        if ((promoted_requirements[node]
                                 .affix_observations[observation]
                                 .features & bit) == 0) {
                            continue;
                        }
                        ObservationRequirement trial =
                            promoted_requirements[node];
                        trial.affix_observations[observation]
                            .features &= ~bit;
                        if (try_promoted_requirement(
                                node, parent_begin, parent_end,
                                std::move(trial))) {
                            removed = true;
                            break;
                        }
                        if (minimization_memory_failed) return false;
                    }
                }
                if (removed) continue;

                for (std::size_t observation =
                         promoted_requirements[node]
                             .affix_observations.size();
                     observation-- > 0 && !removed;) {
                    const RefinementAffixSelector& selector =
                        promoted_requirements[node]
                            .affix_observations[observation]
                            .selector;

                    /* Required tag ids are selector literals, unlike the
                     * observed classification vocabulary above. */
                    for (std::size_t tag =
                             selector.required_tag_ids.size();
                         tag-- > 0;) {
                        ObservationRequirement trial =
                            promoted_requirements[node];
                        trial.affix_observations[observation]
                            .selector.required_tag_ids.erase(
                                trial.affix_observations[observation]
                                        .selector.required_tag_ids.begin() +
                                static_cast<std::ptrdiff_t>(tag));
                        if (try_promoted_requirement(
                                node, parent_begin, parent_end,
                                std::move(trial))) {
                            removed = true;
                            break;
                        }
                        if (minimization_memory_failed) return false;
                    }
                    if (removed) break;

                    for (std::uint16_t bit =
                             std::uint16_t{1} << 15;
                         bit != 0; bit >>= 1) {
                        if ((selector.forbidden_affix_traits & bit) == 0) {
                            continue;
                        }
                        ObservationRequirement trial =
                            promoted_requirements[node];
                        trial.affix_observations[observation]
                            .selector.forbidden_affix_traits &= ~bit;
                        if (try_promoted_requirement(
                                node, parent_begin, parent_end,
                                std::move(trial))) {
                            removed = true;
                            break;
                        }
                        if (minimization_memory_failed) return false;
                    }
                    if (removed) break;
                    for (std::uint16_t bit =
                             std::uint16_t{1} << 15;
                         bit != 0; bit >>= 1) {
                        if ((selector.required_affix_traits & bit) == 0) {
                            continue;
                        }
                        ObservationRequirement trial =
                            promoted_requirements[node];
                        trial.affix_observations[observation]
                            .selector.required_affix_traits &= ~bit;
                        if (try_promoted_requirement(
                                node, parent_begin, parent_end,
                                std::move(trial))) {
                            removed = true;
                            break;
                        }
                        if (minimization_memory_failed) return false;
                    }
                    if (removed) break;
                    for (std::uint8_t bit =
                             std::uint8_t{1} << 7;
                         bit != 0; bit >>= 1) {
                        if ((selector.forbidden_item_traits & bit) == 0) {
                            continue;
                        }
                        ObservationRequirement trial =
                            promoted_requirements[node];
                        trial.affix_observations[observation]
                            .selector.forbidden_item_traits &= ~bit;
                        if (try_promoted_requirement(
                                node, parent_begin, parent_end,
                                std::move(trial))) {
                            removed = true;
                            break;
                        }
                        if (minimization_memory_failed) return false;
                    }
                    if (removed) break;
                    for (std::uint8_t bit =
                             std::uint8_t{1} << 7;
                         bit != 0; bit >>= 1) {
                        if ((selector.required_item_traits & bit) == 0) {
                            continue;
                        }
                        ObservationRequirement trial =
                            promoted_requirements[node];
                        trial.affix_observations[observation]
                            .selector.required_item_traits &= ~bit;
                        if (try_promoted_requirement(
                                node, parent_begin, parent_end,
                                std::move(trial))) {
                            removed = true;
                            break;
                        }
                        if (minimization_memory_failed) return false;
                    }
                }
                if (!removed) break;
            }
        }

        /* This is the publication invariant for synthesized routers, not a
         * best-effort diagnostic: no represented exact state may satisfy a
         * different selected decision's route within its semantic parent. */
        if (!parent_routes_disjoint(parent_begin, parent_end)) {
            return fail(
                result, RefinementStatus::InvalidContract,
                "selected-action router minimization lost represented "
                "pair disjointness");
        }
        parent_begin = parent_end;
    }

    return check_refinement_memory(
        result, graph, limits, adapter_memory);

}

std::vector<std::pair<std::uint32_t, DeterministicSum>>
project_kernel(
        const Graph& graph,
        const Node& node,
        const std::vector<std::uint32_t>& partition) {
    std::map<std::uint32_t, DeterministicSum> projected;
    for (const NodeEdge& edge : node_edges(graph, node)) {
        projected[partition.at(edge.successor)].add(edge.probability);
    }
    return {projected.begin(), projected.end()};
}

FeatureSignature relevant_difference(
        const Node& left,
        const Node& right) {
    FeatureSignature out;
    std::set_symmetric_difference(
        left.state.features.begin(), left.state.features.end(),
        right.state.features.begin(), right.state.features.end(),
        std::back_inserter(out), atom_less);
    return out;
}

CounterexampleKind counterexample_kind(
        const Graph& graph,
        const Node& left,
        const Node& right,
        const std::vector<std::uint32_t>& partition) {
    if ((left.selection != nullptr) !=
        (right.selection != nullptr)) {
        return CounterexampleKind::SelectedAction;
    }
    if (left.selection == nullptr) {
        return CounterexampleKind::Observation;
    }
    if (left.selection->action_id != right.selection->action_id ||
        left.selection->semantic_key !=
            right.selection->semantic_key) {
        return CounterexampleKind::SelectedAction;
    }
    if (std::bit_cast<std::uint64_t>(left.action_cost) !=
        std::bit_cast<std::uint64_t>(right.action_cost)) {
        return CounterexampleKind::ActionCost;
    }
    if (project_kernel(graph, left, partition) !=
        project_kernel(graph, right, partition)) {
        return CounterexampleKind::SuccessorProjection;
    }
    return CounterexampleKind::Observation;
}

bool collect_counterexamples(
        const Graph& graph,
        const std::vector<std::uint32_t>& initial,
        const std::vector<std::uint32_t>& final,
        const RefinementLimits& limits,
        RefinementResult& result,
        const std::uint64_t adapter_memory,
        const std::uint64_t retained_scratch) {
    std::map<std::uint32_t, std::vector<std::uint32_t>> by_coarse;
    const auto by_coarse_memory = [&]() {
        std::uint64_t bytes = estimate_ordered_nodes(by_coarse);
        for (const auto& [unused, states] : by_coarse) {
            (void)unused;
            add_bytes(
                bytes,
                saturated_product(
                    states.capacity(), sizeof(std::uint32_t)));
        }
        return bytes;
    };
    for (std::uint32_t state = 0; state < graph.nodes.size(); ++state) {
        by_coarse[graph.nodes[state].state.coarse_state].push_back(state);
        if (!check_refinement_memory(
                result, graph, limits, adapter_memory,
                saturated_add(
                    retained_scratch, by_coarse_memory()))) {
            return false;
        }
    }
    const std::size_t witness_capacity = std::min<std::size_t>(
        limits.max_witnesses, by_coarse.size());
    std::uint64_t witness_reserve =
        saturated_add(retained_scratch, by_coarse_memory());
    add_bytes(
        witness_reserve,
        saturated_product(
            witness_capacity,
            sizeof(RefinementCounterexample)));
    if (!check_refinement_memory(
            result, graph, limits, adapter_memory,
            witness_reserve)) {
        return false;
    }
    result.counterexamples.reserve(witness_capacity);
    for (const auto& [coarse, states] : by_coarse) {
        std::set<std::uint32_t> classes;
        for (const std::uint32_t state : states) {
            classes.insert(final[state]);
        }
        std::uint64_t class_scan =
            saturated_add(retained_scratch, by_coarse_memory());
        add_bytes(class_scan, estimate_ordered_nodes(classes));
        if (!check_refinement_memory(
                result, graph, limits, adapter_memory,
                class_scan)) {
            return false;
        }
        if (classes.size() <= 1) continue;
        ++result.telemetry.refinement_trigger_coarse_states;
        std::optional<std::pair<std::uint32_t, std::uint32_t>> pair;
        for (std::size_t pass = 0; pass < 2 && !pair; ++pass) {
            for (std::size_t i = 0; i < states.size() && !pair; ++i) {
                for (std::size_t j = i + 1; j < states.size(); ++j) {
                    if (final[states[i]] == final[states[j]]) continue;
                    if (pass == 0 &&
                        initial[states[i]] != initial[states[j]]) {
                        continue;
                    }
                    pair = {states[i], states[j]};
                    break;
                }
            }
        }
        if (!pair.has_value()) continue;
        if (result.counterexamples.size() >= limits.max_witnesses) {
            ++result.telemetry.witnesses_omitted;
            continue;
        }
        const Node& left = graph.nodes[pair->first];
        const Node& right = graph.nodes[pair->second];
        result.counterexamples.push_back({
            counterexample_kind(graph, left, right, final),
            coarse,
            left.state.stable_key,
            right.state.stable_key,
            relevant_difference(left, right)});
        std::uint64_t transient =
            saturated_add(retained_scratch, by_coarse_memory());
        add_bytes(transient, estimate_ordered_nodes(classes));
        if (!check_refinement_memory(
                result, graph, limits, adapter_memory,
                transient)) {
            return false;
        }
    }
    return true;
}

bool build_classes(
        Graph& graph,
        const std::vector<std::uint32_t>& partition,
        const std::uint32_t class_count,
        const RefinementLimits& limits,
        RefinementResult& result,
        const std::uint64_t adapter_memory,
        const std::uint64_t retained_scratch) {
    std::uint64_t projected_members = retained_scratch;
    add_bytes(
        projected_members,
        saturated_product(
            class_count, sizeof(std::size_t)));
    add_bytes(
        projected_members,
        saturated_product(
            class_count,
            sizeof(std::vector<std::uint32_t>)));
    add_bytes(
        projected_members,
        saturated_product(
            graph.nodes.size(), sizeof(std::uint32_t)));
    add_bytes(
        projected_members,
        saturated_product(
            graph.nodes.size(), sizeof(std::uint32_t)));
    add_bytes(
        projected_members,
        saturated_product(
            class_count, sizeof(RefinedPolicyClass)));
    add_bytes(
        projected_members,
        saturated_product(
            graph.nodes.size(), sizeof(StateClassAssignment)));
    if (!check_refinement_memory(
            result, graph, limits, adapter_memory,
            projected_members)) {
        return false;
    }
    std::vector<std::size_t> member_counts(class_count, 0);
    for (const std::uint32_t class_id : partition) {
        ++member_counts.at(class_id);
    }
    std::vector<std::vector<std::uint32_t>> members(class_count);
    std::vector<std::uint32_t> member_position(graph.nodes.size());
    for (std::uint32_t class_id = 0;
         class_id < class_count; ++class_id) {
        members[class_id].reserve(member_counts[class_id]);
    }
    for (std::uint32_t state = 0; state < graph.nodes.size(); ++state) {
        members.at(partition[state]).push_back(state);
    }
    result.classes.reserve(class_count);
    result.assignments.reserve(graph.nodes.size());
    std::uint64_t members_memory = saturated_product(
        members.capacity(), sizeof(std::vector<std::uint32_t>));
    for (const std::vector<std::uint32_t>& states : members) {
        add_bytes(
            members_memory,
            saturated_product(
                states.capacity(), sizeof(std::uint32_t)));
    }
    add_bytes(
        members_memory,
        saturated_product(
            member_counts.capacity(), sizeof(std::size_t)));
    add_bytes(
        members_memory,
        saturated_product(
            member_position.capacity(), sizeof(std::uint32_t)));
    std::uint64_t result_memory =
        estimate_refinement_result_memory(result);
    for (std::uint32_t class_id = 0;
         class_id < class_count; ++class_id) {
        const Node& authority =
            graph.nodes.at(members[class_id].front());
        std::uint64_t class_live =
            saturated_add(retained_scratch, members_memory);
        add_bytes(
            class_live,
            estimate_project_kernel_scratch(graph, authority));
        if (!check_refinement_memory_with_result_ledger(
                result, graph, limits, adapter_memory,
                result_memory, class_live)) {
            return false;
        }
        const auto projection =
            project_kernel(graph, authority, partition);
        const std::uint64_t projection_memory =
            saturated_product(
                projection.capacity(),
                sizeof(std::pair<
                    std::uint32_t, DeterministicSum>));
        std::uint64_t projected_output =
            saturated_product(
                members[class_id].size(), sizeof(StableKey));
        add_bytes(
            projected_output,
            estimate_stable_key_bytes(
                node_coarse_key(graph, authority)));
        /*
         * required_observations and observation_signature are copied into
         * the result while observe_features also owns canonicalization and
         * grouping scratch. Three feature payloads conservatively cover the
         * output plus its item/affix/bundle construction overlap.
         */
        add_bytes(
            projected_output,
            estimate_requirement_bytes(authority.required));
        add_bytes(
            projected_output,
            estimate_requirement_bytes(authority.required));
        const std::uint64_t exact_feature_memory =
            estimate_feature_bytes(authority.state.features);
        add_bytes(projected_output, exact_feature_memory);
        add_bytes(projected_output, exact_feature_memory);
        add_bytes(projected_output, exact_feature_memory);
        if (authority.selection != nullptr) {
            add_bytes(
                projected_output,
                estimate_selected_action_bytes(
                    *authority.selection));
        }
        add_bytes(
            projected_output,
            saturated_product(
                projection.size(),
                sizeof(ProjectedTransition)));
        class_live =
            saturated_add(retained_scratch, members_memory);
        add_bytes(class_live, projection_memory);
        add_bytes(class_live, projected_output);
        if (!check_refinement_memory_with_result_ledger(
                result, graph, limits, adapter_memory,
                result_memory, class_live)) {
            return false;
        }
        RefinedPolicyClass output;
        output.class_id = class_id;
        output.coarse_state = authority.state.coarse_state;
        output.coarse_state_key =
            node_coarse_key(graph, authority);
        output.goal = authority.state.goal;
        output.terminal = authority.state.terminal;
        output.required_observations = authority.required;
        output.observation_signature = authority.state.features;
        if (authority.selection != nullptr) {
            output.selected_action = *authority.selection;
        }
        output.action_cost = authority.action_cost;
        output.exact_members.reserve(members[class_id].size());
        output.transitions.reserve(projection.size());
        for (const std::uint32_t state : members[class_id]) {
            member_position[state] = static_cast<std::uint32_t>(
                output.exact_members.size());
            const std::uint64_t transferred_key_bytes =
                estimate_stable_key_bytes(
                    graph.nodes[state].state.stable_key);
            output.exact_members.push_back(
                std::move(graph.nodes[state].state.stable_key));
            note_graph_memory_release(
                graph, transferred_key_bytes);
        }
        for (const auto& [successor, probability] : projection) {
            output.transitions.push_back(
                {successor, probability.value()});
        }
        add_bytes(
            result_memory,
            estimate_stable_key_bytes(output.coarse_state_key));
        add_bytes(
            result_memory,
            estimate_refined_policy_class_memory(output));
        result.classes.push_back(std::move(output));
        class_live = saturated_add(
            retained_scratch, members_memory);
        add_bytes(class_live, projection_memory);
        if (!check_refinement_memory_with_result_ledger(
                result, graph, limits, adapter_memory,
                result_memory, class_live)) {
            return false;
        }
    }
    std::uint64_t assignment_live = saturated_add(
        retained_scratch, members_memory);
    if (!check_refinement_memory_with_result_ledger(
            result, graph, limits, adapter_memory,
            result_memory, assignment_live)) {
        return false;
    }
    for (std::uint32_t state = 0; state < graph.nodes.size(); ++state) {
        const std::uint32_t class_id = partition[state];
        result.assignments.push_back({
            graph.nodes[state].state.coarse_state,
            class_id,
            member_position[state]});
    }
    return check_refinement_memory_with_result_ledger(
        result, graph, limits, adapter_memory, result_memory,
        saturated_add(retained_scratch, members_memory));
}

} // namespace

} // namespace refinement
} // namespace solver
} // namespace poecraft
