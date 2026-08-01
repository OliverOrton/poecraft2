#pragma once

#include "solver_refinement.hpp"

#include "solver_sparse_policy.hpp"

#include "poecraft/bitset.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <iterator>
#include <map>
#include <set>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace poecraft {
namespace solver {
namespace refinement {
namespace {

bool is_item_feature(const RefinementFeature feature) {
    return (refinement_feature(feature) &
            kAllRefinementItemFeatures) != 0;
}

bool selector_less(
        const RefinementAffixSelector& left,
        const RefinementAffixSelector& right) {
    return std::tie(
               left.required_affix_traits,
               left.forbidden_affix_traits,
               left.required_item_traits,
               left.forbidden_item_traits,
               left.required_tag_ids) <
           std::tie(
               right.required_affix_traits,
               right.forbidden_affix_traits,
               right.required_item_traits,
               right.forbidden_item_traits,
               right.required_tag_ids);
}

bool observation_less(
        const RefinementAffixObservation& left,
        const RefinementAffixObservation& right) {
    if (selector_less(left.selector, right.selector)) return true;
    if (selector_less(right.selector, left.selector)) return false;
    return left.features < right.features;
}

bool atom_less(const FeatureAtom& left, const FeatureAtom& right) {
    return std::tie(
               left.feature, left.subject, left.value,
               left.affix_traits, left.item_traits,
               left.modifier_tag_ids) <
           std::tie(
               right.feature, right.subject, right.value,
               right.affix_traits, right.item_traits,
               right.modifier_tag_ids);
}

RefinementAffixSelector canonical_selector(
        RefinementAffixSelector selector) {
    std::sort(
        selector.required_tag_ids.begin(),
        selector.required_tag_ids.end());
    selector.required_tag_ids.erase(
        std::unique(
            selector.required_tag_ids.begin(),
            selector.required_tag_ids.end()),
        selector.required_tag_ids.end());
    return selector;
}

std::optional<RefinementAffixSelector> intersect_selectors(
        const RefinementAffixSelector& left,
        const RefinementAffixSelector& right) {
    RefinementAffixSelector out;
    out.required_affix_traits =
        left.required_affix_traits | right.required_affix_traits;
    out.forbidden_affix_traits =
        left.forbidden_affix_traits | right.forbidden_affix_traits;
    out.required_item_traits =
        left.required_item_traits | right.required_item_traits;
    out.forbidden_item_traits =
        left.forbidden_item_traits | right.forbidden_item_traits;
    out.required_tag_ids = left.required_tag_ids;
    out.required_tag_ids.insert(
        out.required_tag_ids.end(),
        right.required_tag_ids.begin(),
        right.required_tag_ids.end());
    out = canonical_selector(std::move(out));
    if ((out.required_affix_traits &
         out.forbidden_affix_traits) != 0 ||
        (out.required_item_traits &
         out.forbidden_item_traits) != 0 ||
        (out.required_affix_traits &
         (kRefinementAffixPrefix | kRefinementAffixSuffix)) ==
            (kRefinementAffixPrefix | kRefinementAffixSuffix) ||
        (out.required_affix_traits &
         (kRefinementAffixOnEldritchDominantSide |
          kRefinementAffixOnEldritchNonDominantSide)) ==
            (kRefinementAffixOnEldritchDominantSide |
             kRefinementAffixOnEldritchNonDominantSide)) {
        return std::nullopt;
    }
    return out;
}

std::optional<RefinementAffixSelector> flow_selector_preimage(
        const RefinementAffixSelector& target,
        const RefinementAffixFlow& flow,
        const ActionRefinementContract& contract) {
    std::uint16_t unstable_affix_traits = 0;
    std::uint8_t unstable_item_traits = 0;
    if ((contract.destroyed_item_features &
         (refinement_feature(RefinementFeature::PrefixLock) |
          refinement_feature(RefinementFeature::SuffixLock))) != 0) {
        unstable_affix_traits |=
            kRefinementAffixOnLockedSide;
        unstable_item_traits |=
            kRefinementItemExactlyOneSideLocked;
    }
    if ((contract.destroyed_item_features &
         refinement_feature(
             RefinementFeature::EldritchDominance)) != 0) {
        unstable_affix_traits |=
            kRefinementAffixOnEldritchDominantSide |
            kRefinementAffixOnEldritchNonDominantSide;
        unstable_item_traits |=
            kRefinementItemHasEldritchDominance;
    }
    RefinementAffixSelector preimage;
    preimage.required_item_traits =
        target.required_item_traits &
        ~unstable_item_traits;
    preimage.forbidden_item_traits =
        target.forbidden_item_traits &
        ~unstable_item_traits;
    if (flow.preserves_modifier_classification) {
        preimage.required_tag_ids = target.required_tag_ids;
    }
    for (std::uint16_t bit = 1;
         bit <= kAllRefinementAffixTraits;
         bit = static_cast<std::uint16_t>(bit << 1)) {
        const bool required =
            (target.required_affix_traits & bit) != 0;
        const bool forbidden =
            (target.forbidden_affix_traits & bit) != 0;
        if ((unstable_affix_traits & bit) != 0) {
            continue;
        }
        const bool set = (flow.set_affix_traits & bit) != 0;
        const bool cleared =
            (flow.cleared_affix_traits & bit) != 0;
        if ((required && cleared) || (forbidden && set)) {
            return std::nullopt;
        }
        if (!set && !cleared) {
            if (required) {
                preimage.required_affix_traits |= bit;
            }
            if (forbidden) {
                preimage.forbidden_affix_traits |= bit;
            }
        }
    }
    return intersect_selectors(
        preimage, flow.source_selector);
}

ObservationRequirement merge_requirements(
        ObservationRequirement target,
        const ObservationRequirement& addition) {
    target.item_features |= addition.item_features;
    target.modifier_tag_ids.insert(
        target.modifier_tag_ids.end(),
        addition.modifier_tag_ids.begin(),
        addition.modifier_tag_ids.end());
    target.affix_observations.insert(
        target.affix_observations.end(),
        addition.affix_observations.begin(),
        addition.affix_observations.end());
    return canonical_observation_requirement(std::move(target));
}

ObservationRequirement contract_observations(
        const ActionRefinementContract& contract) {
    return canonical_observation_requirement({
        contract.observed_item_features,
        contract.observed_modifier_tag_ids,
        contract.affix_observations});
}

ObservationRequirement preserved_requirement(
        const ObservationRequirement& downstream,
        const ActionRefinementContract& contract) {
    ObservationRequirement out;
    if (contract.resets_to_fresh_item) return out;
    out.item_features =
        downstream.item_features & contract.preserved_item_features;
    for (const RefinementAffixObservation& observation :
         downstream.affix_observations) {
        if (!contract.affix_flows.empty()) {
            for (const RefinementAffixFlow& flow :
                 contract.affix_flows) {
                const RefinementFeatureMask features =
                    observation.features &
                    flow.preserved_features;
                if (features == 0) continue;
                const auto preimage =
                    flow_selector_preimage(
                        observation.selector, flow, contract);
                if (!preimage.has_value()) continue;
                out.affix_observations.push_back(
                    {features, *preimage});
            }
        } else {
            /* Compatibility for direct unit fixtures constructed before
             * admission canonicalizes identity survivor flows. */
            for (const RefinementAffixSelector& preserved :
                 contract.preserved_affixes) {
                const auto intersection =
                    intersect_selectors(
                        observation.selector, preserved);
                if (!intersection.has_value()) continue;
                out.affix_observations.push_back(
                    {observation.features, *intersection});
            }
        }
    }

    /*
     * Scalar summaries that may be rewritten are correlated with surviving
     * carriers. Carrying only the old boolean/count would merge, for example,
     * a crafted prefix removal with a crafted suffix removal or two survivor
     * sets with different metamod roles.
     */
    for (const RefinementItemAffixDependency& dependency :
         contract.item_affix_dependencies) {
        if ((downstream.item_features &
             dependency.item_features) == 0) {
            continue;
        }
        if (!contract.affix_flows.empty()) {
            for (const RefinementAffixFlow& flow :
                 contract.affix_flows) {
                const RefinementFeatureMask features =
                    dependency.survivor_affix_features &
                    flow.preserved_features;
                if (features == 0) continue;
                out.affix_observations.push_back(
                    {features, flow.source_selector});
            }
        } else {
            for (const RefinementAffixSelector& preserved :
                 contract.preserved_affixes) {
                out.affix_observations.push_back(
                    {
                        dependency.survivor_affix_features,
                        preserved});
            }
        }
    }
    const bool tags_required = std::any_of(
        out.affix_observations.begin(),
        out.affix_observations.end(),
        [](const RefinementAffixObservation& observation) {
            return (observation.features &
                    refinement_feature(
                        RefinementFeature::
                            ModifierClassificationTags)) != 0;
        });
    if (tags_required) {
        out.modifier_tag_ids = downstream.modifier_tag_ids;
    }
    return canonical_observation_requirement(std::move(out));
}

bool may_destroy_requirement(
    const ObservationRequirement& requirement,
    const ActionRefinementContract& contract);

RefinementFeatureMask requirement_feature_mask(
        const ObservationRequirement& requirement) {
    RefinementFeatureMask mask = requirement.item_features;
    for (const RefinementAffixObservation& observation :
         requirement.affix_observations) {
        mask |= observation.features;
    }
    if (!requirement.modifier_tag_ids.empty()) {
        mask |= refinement_feature(
            RefinementFeature::ModifierClassificationTags);
    }
    return mask;
}

RefinementFeatureMask destroyed_requirement_feature_mask(
        const ObservationRequirement& requirement,
        const ActionRefinementContract& contract) {
    if (contract.resets_to_fresh_item) {
        return requirement_feature_mask(requirement);
    }
    RefinementFeatureMask mask =
        requirement.item_features &
        contract.destroyed_item_features;
    bool destroys_observed_affix = false;
    for (const RefinementAffixObservation& observation :
         requirement.affix_observations) {
        for (const RefinementAffixSelector& destroyed :
             contract.destroyed_affixes) {
            if (!intersect_selectors(
                    observation.selector, destroyed).has_value()) {
                continue;
            }
            mask |= observation.features;
            destroys_observed_affix = true;
        }
    }
    if (destroys_observed_affix &&
        !requirement.modifier_tag_ids.empty()) {
        mask |= refinement_feature(
            RefinementFeature::ModifierClassificationTags);
    }
    return mask;
}

ObservationRequirement selected_contract_observations(
        const SelectedAction& selected) {
    if (selected.execution_paths.empty() &&
        selected.ordered_program.empty()) {
        return contract_observations(selected.contract);
    }
    const auto observe_path =
        [&](const std::vector<ActionRefinementContract>& path) {
            ObservationRequirement path_result;
            for (std::size_t step = 0;
                 step < path.size(); ++step) {
                ObservationRequirement at_entry =
                    contract_observations(path[step]);
                for (std::size_t prior = step;
                     prior > 0; --prior) {
                    at_entry = preserved_requirement(
                        at_entry, path[prior - 1]);
                }
                path_result = merge_requirements(
                    std::move(path_result), at_entry);
            }
            return path_result;
        };
    ObservationRequirement result;
    if (!selected.execution_paths.empty()) {
        for (const std::vector<ActionRefinementContract>& path :
             selected.execution_paths) {
            result = merge_requirements(
                std::move(result), observe_path(path));
        }
    } else {
        result = merge_requirements(
            std::move(result),
            observe_path(selected.ordered_program));
    }
    return canonical_observation_requirement(
        std::move(result));
}

ObservationRequirement selected_preserved_requirement(
        const ObservationRequirement& downstream,
        const SelectedAction& selected) {
    if (selected.execution_paths.empty() &&
        selected.ordered_program.empty()) {
        return preserved_requirement(
            downstream, selected.contract);
    }
    const auto preimage =
        [&](const std::vector<ActionRefinementContract>& path) {
            ObservationRequirement result = downstream;
            for (auto step = path.rbegin();
                 step != path.rend(); ++step) {
                result = preserved_requirement(result, *step);
            }
            return result;
        };
    ObservationRequirement result;
    if (!selected.execution_paths.empty()) {
        for (const std::vector<ActionRefinementContract>& path :
             selected.execution_paths) {
            result = merge_requirements(
                std::move(result), preimage(path));
        }
    } else {
        result = preimage(selected.ordered_program);
    }
    return canonical_observation_requirement(
        std::move(result));
}

RefinementFeatureMask selected_destroyed_feature_mask(
        const ObservationRequirement& downstream,
        const SelectedAction& selected) {
    if (selected.execution_paths.empty() &&
        selected.ordered_program.empty()) {
        return destroyed_requirement_feature_mask(
            downstream, selected.contract);
    }
    const auto path_destroyed_features =
        [&](const std::vector<ActionRefinementContract>& path) {
            ObservationRequirement current = downstream;
            RefinementFeatureMask destroyed = 0;
            for (auto step = path.rbegin();
                 step != path.rend(); ++step) {
                destroyed |= destroyed_requirement_feature_mask(
                    current, *step);
                current = preserved_requirement(current, *step);
            }
            return destroyed;
        };
    RefinementFeatureMask destroyed = 0;
    if (!selected.execution_paths.empty()) {
        for (const std::vector<ActionRefinementContract>& path :
             selected.execution_paths) {
            destroyed |= path_destroyed_features(path);
        }
        return destroyed;
    }
    return path_destroyed_features(selected.ordered_program);
}

RefinementFeatureMask selected_preserved_feature_mask(
        const ObservationRequirement& downstream,
        const SelectedAction& selected) {
    const ObservationRequirement preserved =
        selected_preserved_requirement(downstream, selected);
    return requirement_feature_mask(downstream) &
           requirement_feature_mask(preserved);
}

bool may_destroy_requirement(
        const ObservationRequirement& requirement,
        const ActionRefinementContract& contract) {
    return destroyed_requirement_feature_mask(
               requirement, contract) != 0;
}

void append_tokens(
        std::vector<std::uint64_t>& out,
        const StableKey& tokens) {
    out.push_back(tokens.size());
    out.insert(out.end(), tokens.begin(), tokens.end());
}

bool selected_runtime_contracts_complete(
        const SelectedAction& selected) {
    const auto incomplete =
        [](const ActionRefinementContract& contract) {
            return !contract.complete();
        };
    if (std::any_of(
            selected.ordered_program.begin(),
            selected.ordered_program.end(),
            incomplete)) {
        return false;
    }
    if (std::any_of(
        selected.execution_paths.begin(),
        selected.execution_paths.end(),
        [&](const std::vector<ActionRefinementContract>& path) {
            return path.empty() ||
                   std::any_of(
                       path.begin(), path.end(), incomplete);
        })) {
        return false;
    }
    /*
     * Once explicit alternatives are supplied they replace the longest-path
     * fallback in every transfer function. Requiring the canonical program
     * among them prevents a malformed adapter from silently under-refining.
     */
    if (selected.execution_paths.empty()) return true;
    if (selected.ordered_program.empty()) return false;
    return std::any_of(
        selected.execution_paths.begin(),
        selected.execution_paths.end(),
        [&](const std::vector<ActionRefinementContract>& path) {
            if (path.size() !=
                selected.ordered_program.size()) {
                return false;
            }
            for (std::size_t step = 0;
                 step < path.size(); ++step) {
                if (action_refinement_contract_signature(path[step]) !=
                    action_refinement_contract_signature(
                        selected.ordered_program[step])) {
                    return false;
                }
            }
            return true;
        });
}

StableKey selected_runtime_contract_signature(
        const SelectedAction& selected) {
    std::vector<StableKey> path_signatures;
    path_signatures.reserve(selected.execution_paths.size());
    for (const std::vector<ActionRefinementContract>& path :
         selected.execution_paths) {
        StableKey path_signature{path.size()};
        for (const ActionRefinementContract& contract : path) {
            append_tokens(
                path_signature,
                action_refinement_contract_signature(contract));
        }
        path_signatures.push_back(std::move(path_signature));
    }
    std::sort(
        path_signatures.begin(), path_signatures.end());
    path_signatures.erase(
        std::unique(
            path_signatures.begin(), path_signatures.end()),
        path_signatures.end());
    StableKey signature{
        0x7063726374727633ull, /* "pcrctrv3" */
        selected.ordered_program.size(),
        path_signatures.size()};
    append_tokens(
        signature,
        action_refinement_contract_signature(
            selected.contract));
    for (const ActionRefinementContract& contract :
         selected.ordered_program) {
        append_tokens(
            signature,
            action_refinement_contract_signature(contract));
    }
    for (const StableKey& path_signature : path_signatures) {
        append_tokens(signature, path_signature);
    }
    return signature;
}

void append_selector(
        std::vector<std::uint64_t>& out,
        const RefinementAffixSelector& selector) {
    out.push_back(selector.required_affix_traits);
    out.push_back(selector.forbidden_affix_traits);
    out.push_back(selector.required_item_traits);
    out.push_back(selector.forbidden_item_traits);
    out.push_back(selector.required_tag_ids.size());
    for (const std::uint32_t tag : selector.required_tag_ids) {
        out.push_back(tag);
    }
}

void append_requirement(
        std::vector<std::uint64_t>& out,
        const ObservationRequirement& requirement) {
    out.push_back(requirement.item_features);
    out.push_back(requirement.modifier_tag_ids.size());
    for (const std::uint32_t tag : requirement.modifier_tag_ids) {
        out.push_back(tag);
    }
    out.push_back(requirement.affix_observations.size());
    for (const RefinementAffixObservation& observation :
         requirement.affix_observations) {
        out.push_back(observation.features);
        append_selector(out, observation.selector);
    }
}

void append_atom(
        std::vector<std::uint64_t>& out,
        const FeatureAtom& atom) {
    out.push_back(static_cast<std::uint8_t>(atom.feature));
    out.push_back(atom.subject);
    append_tokens(out, atom.value);
}


} // namespace

} // namespace refinement
} // namespace solver
} // namespace poecraft
