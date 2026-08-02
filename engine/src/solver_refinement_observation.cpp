#include "solver_refinement_observation_helpers.hpp"

namespace poecraft {
namespace solver {
namespace refinement {

StableKey canonical_selected_runtime_contract_identity(
        const SelectedAction& action) {
    return selected_runtime_contract_signature(action);
}

FeatureSignature canonical_feature_signature(
        FeatureSignature signature) {
    for (FeatureAtom& atom : signature) {
        std::sort(
            atom.modifier_tag_ids.begin(),
            atom.modifier_tag_ids.end());
        atom.modifier_tag_ids.erase(
            std::unique(
                atom.modifier_tag_ids.begin(),
                atom.modifier_tag_ids.end()),
            atom.modifier_tag_ids.end());
    }
    std::sort(signature.begin(), signature.end(), atom_less);
    signature.erase(
        std::unique(signature.begin(), signature.end()),
        signature.end());
    return signature;
}

ObservationRequirement canonical_observation_requirement(
        ObservationRequirement requirement) {
    requirement.item_features &= kAllRefinementItemFeatures;
    std::sort(
        requirement.modifier_tag_ids.begin(),
        requirement.modifier_tag_ids.end());
    requirement.modifier_tag_ids.erase(
        std::unique(
            requirement.modifier_tag_ids.begin(),
            requirement.modifier_tag_ids.end()),
        requirement.modifier_tag_ids.end());
    for (RefinementAffixObservation& observation :
         requirement.affix_observations) {
        observation.features &= kAllRefinementAffixFeatures;
        observation.selector =
            canonical_selector(std::move(observation.selector));
    }
    requirement.affix_observations.erase(
        std::remove_if(
            requirement.affix_observations.begin(),
            requirement.affix_observations.end(),
            [](const RefinementAffixObservation& observation) {
                return observation.features == 0;
            }),
        requirement.affix_observations.end());
    std::sort(
        requirement.affix_observations.begin(),
        requirement.affix_observations.end(), observation_less);
    std::vector<RefinementAffixObservation> merged;
    for (const RefinementAffixObservation& observation :
         requirement.affix_observations) {
        if (!merged.empty() &&
            merged.back().selector == observation.selector) {
            merged.back().features |= observation.features;
        } else {
            merged.push_back(observation);
        }
    }
    requirement.affix_observations = std::move(merged);
    return requirement;
}

ObservationRequirement merge_observation_requirements(
        ObservationRequirement target,
        const ObservationRequirement& addition) {
    return merge_requirements(std::move(target), addition);
}

StableKey canonical_observation_identity(
        const StableKey& coarse_parent,
        const ObservationRequirement& requirement,
        const FeatureSignature& exact_features) {
    if (coarse_parent.empty()) return {};
    const ObservationRequirement canonical_requirement =
        canonical_observation_requirement(requirement);
    const FeatureSignature observed = canonical_feature_signature(
        observe_features(exact_features, canonical_requirement));
    StableKey out{0x7063726f627331ull}; /* "pcrobs1" */
    append_tokens(out, coarse_parent);
    append_requirement(out, canonical_requirement);
    out.push_back(observed.size());
    for (const FeatureAtom& atom : observed) append_atom(out, atom);
    return out;
}

ObservationRequirement observation_requirement_from_contract(
        const ActionRefinementContract& contract) {
    return canonical_observation_requirement({
        contract.observed_item_features,
        contract.observed_modifier_tag_ids,
        contract.affix_observations});
}

ObservationRequirement preserved_observation_requirement(
        const ObservationRequirement& downstream,
        const ActionRefinementContract& contract) {
    return preserved_requirement(downstream, contract);
}

ObservationRequirement observation_requirement_from_selected_action(
        const SelectedAction& action) {
    return selected_contract_observations(action);
}

ObservationRequirement preserved_observation_requirement(
        const ObservationRequirement& downstream,
        const SelectedAction& action) {
    return selected_preserved_requirement(
        downstream, action);
}

PolicyObservationFixedPoint propagate_policy_observations(
        std::vector<PolicyObservationNode> nodes,
        const std::uint32_t max_rounds) {
    PolicyObservationFixedPoint result;
    if (nodes.empty()) {
        result.failure_reason =
            "policy observation graph is empty";
        return result;
    }
    std::sort(
        nodes.begin(), nodes.end(),
        [](const PolicyObservationNode& left,
           const PolicyObservationNode& right) {
            return left.state_id < right.state_id;
        });
    std::map<std::uint32_t, std::uint32_t> index_by_state;
    for (std::uint32_t index = 0; index < nodes.size(); ++index) {
        if (!index_by_state.emplace(
                nodes[index].state_id, index).second) {
            result.failure_reason =
                "policy observation graph has duplicate state ids";
            return result;
        }
        std::sort(
            nodes[index].successors.begin(),
            nodes[index].successors.end());
        nodes[index].successors.erase(
            std::unique(
                nodes[index].successors.begin(),
                nodes[index].successors.end()),
            nodes[index].successors.end());
    }
    const auto referenced_node =
        [&](const std::uint32_t state_id)
            -> const PolicyObservationNode* {
            const auto found = index_by_state.find(state_id);
            return found == index_by_state.end()
                       ? nullptr
                       : &nodes[found->second];
        };
    for (const PolicyObservationNode& node : nodes) {
        if (node.selected_action_source.has_value()) {
            const PolicyObservationNode* source =
                referenced_node(*node.selected_action_source);
            if (source == nullptr ||
                source->selected_action_source.has_value() ||
                !source->selected_action.has_value()) {
                result.failure_reason =
                    "policy observation graph has an invalid shared "
                    "action authority";
                return result;
            }
        }
        if (node.successor_source.has_value()) {
            const PolicyObservationNode* source =
                referenced_node(*node.successor_source);
            if (source == nullptr ||
                source->successor_source.has_value()) {
                result.failure_reason =
                    "policy observation graph has an invalid shared "
                    "successor authority";
                return result;
            }
        }
    }
    const auto selected_action =
        [&](const PolicyObservationNode& node)
            -> const std::optional<SelectedAction>& {
            return node.selected_action_source.has_value()
                       ? referenced_node(
                             *node.selected_action_source)
                             ->selected_action
                       : node.selected_action;
        };
    const auto successors =
        [&](const PolicyObservationNode& node)
            -> const std::vector<std::uint32_t>& {
            return node.successor_source.has_value()
                       ? referenced_node(*node.successor_source)
                             ->successors
                       : node.successors;
        };
    std::vector<std::vector<std::uint32_t>> propagation_groups;
    {
        std::map<StableKey, std::uint32_t> group_by_signature;
        for (std::uint32_t index = 0; index < nodes.size(); ++index) {
            const PolicyObservationNode& node = nodes[index];
            StableKey key{0x7063726f62736731ull}; /* "pcrobsg1" */
            key.push_back(
                node.selected_action_source.has_value()
                    ? static_cast<std::uint64_t>(
                          *node.selected_action_source) + 1
                    : (node.selected_action.has_value()
                           ? static_cast<std::uint64_t>(node.state_id) + 1
                           : 0));
            key.push_back(
                node.successor_source.has_value()
                    ? static_cast<std::uint64_t>(
                          *node.successor_source) + 1
                    : static_cast<std::uint64_t>(node.state_id) + 1);
            append_requirement(key, node.direct_observes);
            const auto [group, inserted] =
                group_by_signature.emplace(
                    std::move(key),
                    static_cast<std::uint32_t>(
                        propagation_groups.size()));
            if (inserted) {
                propagation_groups.emplace_back();
            }
            propagation_groups[group->second].push_back(index);
        }
    }
    for (const PolicyObservationNode& node : nodes) {
        const std::optional<SelectedAction>& selected =
            selected_action(node);
        if (selected.has_value() &&
            !selected->contract.complete()) {
            result.failure_reason =
                "policy observation graph has an incomplete action "
                "contract";
            return result;
        }
        if (selected.has_value() &&
            !selected_runtime_contracts_complete(
                *selected)) {
            result.failure_reason =
                "policy observation graph has an incomplete or empty "
                "runtime path contract";
            return result;
        }
    }
    std::vector<ObservationRequirement> required(nodes.size());
    for (std::uint32_t index = 0; index < nodes.size(); ++index) {
        const std::optional<SelectedAction>& selected_value =
            selected_action(nodes[index]);
        if (selected_value.has_value()) {
            const SelectedAction& selected =
                *selected_value;
            required[index] = merge_requirements(
                merge_requirements(
                    selected_contract_observations(selected),
                    selected.routing_observes),
                selected_preserved_requirement(
                    nodes[index].direct_observes,
                    selected));
        } else {
            required[index] = canonical_observation_requirement(
                nodes[index].direct_observes);
        }
    }
    for (;;) {
        if (result.rounds >= max_rounds) {
            result.round_cap = true;
            result.failure_reason =
                "policy observation propagation reached "
                "max_refinement_rounds";
            return result;
        }
        ++result.rounds;
        bool changed = false;
        std::vector<ObservationRequirement> next = required;
        for (const std::vector<std::uint32_t>& group :
             propagation_groups) {
            const std::uint32_t source = group.front();
            const PolicyObservationNode& node = nodes[source];
            ObservationRequirement propagated = required[source];
            for (const std::uint32_t successor :
                 successors(node)) {
                const auto target =
                    index_by_state.find(successor);
                if (target == index_by_state.end()) {
                    result.failure_reason =
                        "policy observation graph has an unknown "
                        "successor";
                    return result;
                }
                const std::optional<SelectedAction>& selected =
                    selected_action(node);
                const ObservationRequirement carried =
                    selected.has_value()
                        ? selected_preserved_requirement(
                              required[target->second],
                              *selected)
                        : required[target->second];
                ObservationRequirement merged =
                    merge_requirements(
                        std::move(propagated), carried);
                propagated = std::move(merged);
            }
            for (const std::uint32_t member : group) {
                if (!(propagated == required[member])) {
                    next[member] = propagated;
                    changed = true;
                }
            }
        }
        required = std::move(next);
        if (!changed) break;
    }
    result.assignments.reserve(nodes.size());
    for (std::uint32_t index = 0; index < nodes.size(); ++index) {
        result.assignments.push_back(
            {nodes[index].state_id, required[index]});
        const std::optional<SelectedAction>& selected =
            selected_action(nodes[index]);
        if (!selected.has_value()) {
            continue;
        }
        const RefinementFeatureMask destroyed =
            selected_destroyed_feature_mask(
                required[index],
                *selected);
        const RefinementFeatureMask preserved =
            selected_preserved_feature_mask(
                required[index],
                *selected);
        result.collapse_destroyed_feature_mask |= destroyed;
        result.collapse_preserved_feature_mask |= preserved;
        if (destroyed != 0) {
            ++result.collapse_events;
        }
        for (std::size_t feature = 0;
             feature < static_cast<std::size_t>(
                           RefinementFeature::Count);
             ++feature) {
            const RefinementFeatureMask bit =
                RefinementFeatureMask{1} << feature;
            if ((destroyed & bit) != 0) {
                ++result.collapse_events_by_feature[feature];
            }
            if ((preserved & bit) != 0) {
                ++result.preservation_events_by_feature[feature];
            }
        }
    }
    result.complete = true;
    return result;
}

} // namespace refinement
} // namespace solver
} // namespace poecraft
