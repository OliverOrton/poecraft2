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
};

struct ClosedProjectionEntry {
    StableKey label;
    std::optional<std::uint32_t> successor_class;
    DeterministicSum probability;

    bool operator==(const ClosedProjectionEntry&) const = default;
};

using ClosedPartitionKey = std::vector<std::uint64_t>;

std::vector<ClosedProjectionEntry> project_closed_row(
        const CanonicalClosedNode& node,
        const std::vector<std::uint32_t>& partition) {
    using ProjectionKey =
        std::pair<StableKey, std::optional<std::uint32_t>>;
    std::map<ProjectionKey, DeterministicSum> projected;
    for (const CanonicalClosedArc& arc : node.arcs) {
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

ClosedPartitionKey closed_initial_key(
        const CanonicalClosedNode& node) {
    ClosedPartitionKey out{node.terminal ? 1u : 0u};
    append_tokens(out, node.observation_key);
    return out;
}

ClosedPartitionKey closed_refined_key(
        const CanonicalClosedNode& node,
        const std::uint32_t current_class,
        const std::vector<std::uint32_t>& partition) {
    ClosedPartitionKey out{current_class};
    append_tokens(out, node.immediate_key);
    const std::vector<ClosedProjectionEntry> projected =
        project_closed_row(node, partition);
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
        if (source.terminal && !source.arcs.empty()) {
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
        DeterministicSum total;
        node.arcs.reserve(grouped.size());
        for (auto& [key, probability] : grouped) {
            total.add(probability);
            node.arcs.push_back({
                std::move(key.first), key.second, probability});
        }
        if (!node.terminal &&
            (node.arcs.empty() ||
             std::abs(total.value() - 1.0) >
                 limits.probability_sum_tolerance)) {
            failure_reason =
                "closed partition non-terminal row is not stochastic";
            return false;
        }
        output.push_back(std::move(node));
    }
    return true;
}

struct NodeEdge {
    std::uint32_t successor = 0;
    DeterministicSum probability;
};

struct Node {
    ExactState state;
    bool expanded = false;
    std::optional<SelectedAction> selection;
    double action_cost = 0.0;
    std::vector<NodeEdge> edges;
    ObservationRequirement required;
};

struct PendingEdge {
    ExactState successor;
    DeterministicSum probability;
};

struct ActionKey {
    std::uint32_t action_id = 0;
    StableKey semantic_key;

    bool operator<(const ActionKey& other) const {
        return std::tie(action_id, semantic_key) <
               std::tie(other.action_id, other.semantic_key);
    }
};

struct Graph {
    std::vector<Node> nodes;
    std::map<StableKey, std::uint32_t> index_by_key;
    std::map<std::uint32_t, StableKey> coarse_key_by_state;
    std::map<StableKey, std::uint32_t> coarse_state_by_key;
    std::map<ActionKey, StableKey> contract_signature_by_action;
};

std::uint64_t saturated_add(
        const std::uint64_t left,
        const std::uint64_t right) {
    return right >
                   std::numeric_limits<std::uint64_t>::max() - left
               ? std::numeric_limits<std::uint64_t>::max()
               : left + right;
}

std::uint64_t saturated_product(
        const std::size_t count,
        const std::size_t element_size) {
    if (element_size != 0 &&
        count >
            std::numeric_limits<std::uint64_t>::max() /
                element_size) {
        return std::numeric_limits<std::uint64_t>::max();
    }
    return static_cast<std::uint64_t>(count) *
           static_cast<std::uint64_t>(element_size);
}

void add_bytes(
        std::uint64_t& total,
        const std::uint64_t addition) {
    total = saturated_add(total, addition);
}

std::uint64_t estimate_stable_key_bytes(
        const StableKey& key) {
    return saturated_product(
        key.capacity(), sizeof(std::uint64_t));
}

std::uint64_t estimate_selector_bytes(
        const RefinementAffixSelector& selector) {
    return saturated_product(
        selector.required_tag_ids.capacity(),
        sizeof(std::uint32_t));
}

std::uint64_t estimate_feature_bytes(
        const FeatureSignature& signature) {
    std::uint64_t bytes = saturated_product(
        signature.capacity(), sizeof(FeatureAtom));
    for (const FeatureAtom& atom : signature) {
        add_bytes(bytes, estimate_stable_key_bytes(atom.value));
        add_bytes(
            bytes,
            saturated_product(
                atom.modifier_tag_ids.capacity(),
                sizeof(std::uint32_t)));
    }
    return bytes;
}

std::uint64_t estimate_requirement_bytes(
        const ObservationRequirement& requirement) {
    std::uint64_t bytes = saturated_product(
        requirement.modifier_tag_ids.capacity(),
        sizeof(std::uint32_t));
    add_bytes(
        bytes,
        saturated_product(
            requirement.affix_observations.capacity(),
            sizeof(RefinementAffixObservation)));
    for (const RefinementAffixObservation& observation :
         requirement.affix_observations) {
        add_bytes(
            bytes,
            estimate_selector_bytes(observation.selector));
    }
    return bytes;
}

std::uint64_t estimate_contract_bytes(
        const ActionRefinementContract& contract) {
    std::uint64_t bytes = saturated_product(
        contract.observed_modifier_tag_ids.capacity(),
        sizeof(std::uint32_t));
    add_bytes(
        bytes,
        saturated_product(
            contract.affix_observations.capacity(),
            sizeof(RefinementAffixObservation)));
    for (const RefinementAffixObservation& observation :
         contract.affix_observations) {
        add_bytes(
            bytes,
            estimate_selector_bytes(observation.selector));
    }
    const auto add_selectors =
        [&](const std::vector<RefinementAffixSelector>& selectors) {
            add_bytes(
                bytes,
                saturated_product(
                    selectors.capacity(),
                    sizeof(RefinementAffixSelector)));
            for (const RefinementAffixSelector& selector : selectors) {
                add_bytes(bytes, estimate_selector_bytes(selector));
            }
    };
    add_bytes(
        bytes,
        saturated_product(
            contract.item_affix_dependencies.capacity(),
            sizeof(RefinementItemAffixDependency)));
    add_bytes(
        bytes,
        saturated_product(
            contract.affix_flows.capacity(),
            sizeof(RefinementAffixFlow)));
    for (const RefinementAffixFlow& flow :
         contract.affix_flows) {
        add_bytes(
            bytes,
            estimate_selector_bytes(flow.source_selector));
    }
    add_selectors(contract.preserved_affixes);
    add_selectors(contract.destroyed_affixes);
    return bytes;
}

std::uint64_t estimate_selected_action_bytes(
        const SelectedAction& selected) {
    std::uint64_t bytes =
        estimate_stable_key_bytes(selected.semantic_key);
    add_bytes(bytes, estimate_contract_bytes(selected.contract));
    add_bytes(
        bytes,
        saturated_product(
            selected.ordered_program.capacity(),
            sizeof(ActionRefinementContract)));
    for (const ActionRefinementContract& contract :
         selected.ordered_program) {
        add_bytes(bytes, estimate_contract_bytes(contract));
    }
    add_bytes(
        bytes,
        saturated_product(
            selected.execution_paths.capacity(),
            sizeof(std::vector<ActionRefinementContract>)));
    for (const std::vector<ActionRefinementContract>& path :
         selected.execution_paths) {
        add_bytes(
            bytes,
            saturated_product(
                path.capacity(),
                sizeof(ActionRefinementContract)));
        for (const ActionRefinementContract& contract : path) {
            add_bytes(bytes, estimate_contract_bytes(contract));
        }
    }
    add_bytes(
        bytes,
        estimate_requirement_bytes(selected.routing_observes));
    return bytes;
}

std::uint64_t estimate_exact_state_bytes(
        const ExactState& state) {
    std::uint64_t bytes =
        estimate_stable_key_bytes(state.stable_key);
    add_bytes(
        bytes,
        estimate_stable_key_bytes(state.coarse_state_key));
    add_bytes(bytes, estimate_feature_bytes(state.features));
    return bytes;
}

std::uint64_t estimate_exact_kernel_bytes(
        const ExactActionKernel& kernel) {
    std::uint64_t bytes = saturated_product(
        kernel.transitions.capacity(), sizeof(ExactTransition));
    for (const ExactTransition& transition : kernel.transitions) {
        add_bytes(
            bytes,
            estimate_exact_state_bytes(transition.successor));
    }
    return bytes;
}

template <typename OrderedContainer>
std::uint64_t estimate_ordered_nodes(
        const OrderedContainer& container) {
    return saturated_product(
        container.size(),
        sizeof(typename OrderedContainer::value_type) +
            3 * sizeof(void*));
}

std::uint64_t estimate_graph_memory(const Graph& graph) {
    std::uint64_t bytes = saturated_product(
        graph.nodes.capacity(), sizeof(Node));
    for (const Node& node : graph.nodes) {
        add_bytes(bytes, estimate_exact_state_bytes(node.state));
        add_bytes(
            bytes,
            saturated_product(
                node.edges.capacity(), sizeof(NodeEdge)));
        add_bytes(bytes, estimate_requirement_bytes(node.required));
        if (node.selection.has_value()) {
            add_bytes(
                bytes,
                estimate_selected_action_bytes(*node.selection));
        }
    }
    add_bytes(bytes, estimate_ordered_nodes(graph.index_by_key));
    for (const auto& [key, unused] : graph.index_by_key) {
        (void)unused;
        add_bytes(bytes, estimate_stable_key_bytes(key));
    }
    add_bytes(
        bytes,
        estimate_ordered_nodes(graph.coarse_key_by_state));
    for (const auto& [unused, key] :
         graph.coarse_key_by_state) {
        (void)unused;
        add_bytes(bytes, estimate_stable_key_bytes(key));
    }
    add_bytes(
        bytes,
        estimate_ordered_nodes(graph.coarse_state_by_key));
    for (const auto& [key, unused] :
         graph.coarse_state_by_key) {
        (void)unused;
        add_bytes(bytes, estimate_stable_key_bytes(key));
    }
    add_bytes(
        bytes,
        estimate_ordered_nodes(
            graph.contract_signature_by_action));
    for (const auto& [key, signature] :
         graph.contract_signature_by_action) {
        add_bytes(
            bytes,
            estimate_stable_key_bytes(key.semantic_key));
        add_bytes(bytes, estimate_stable_key_bytes(signature));
    }
    return bytes;
}

std::uint64_t estimate_refinement_result_memory(
        const RefinementResult& result) {
    std::uint64_t bytes =
        saturated_add(
            result.failure_reason.capacity() + 1,
            result.resource_cap.capacity() + 1);
    add_bytes(
        bytes,
        saturated_product(
            result.assignments.capacity(),
            sizeof(StateClassAssignment)));
    for (const StateClassAssignment& assignment :
         result.assignments) {
        add_bytes(
            bytes,
            estimate_stable_key_bytes(assignment.exact_state));
        add_bytes(
            bytes,
            estimate_stable_key_bytes(
                assignment.coarse_state_key));
    }
    add_bytes(
        bytes,
        saturated_product(
            result.classes.capacity(),
            sizeof(RefinedPolicyClass)));
    for (const RefinedPolicyClass& policy : result.classes) {
        add_bytes(
            bytes,
            estimate_stable_key_bytes(
                policy.coarse_state_key));
        add_bytes(
            bytes,
            saturated_product(
                policy.exact_members.capacity(),
                sizeof(StableKey)));
        for (const StableKey& member : policy.exact_members) {
            add_bytes(bytes, estimate_stable_key_bytes(member));
        }
        add_bytes(
            bytes,
            estimate_requirement_bytes(
                policy.required_observations));
        add_bytes(
            bytes,
            estimate_feature_bytes(
                policy.observation_signature));
        if (policy.selected_action.has_value()) {
            add_bytes(
                bytes,
                estimate_selected_action_bytes(
                    *policy.selected_action));
        }
        add_bytes(
            bytes,
            saturated_product(
                policy.transitions.capacity(),
                sizeof(ProjectedTransition)));
    }
    add_bytes(
        bytes,
        saturated_product(
            result.counterexamples.capacity(),
            sizeof(RefinementCounterexample)));
    for (const RefinementCounterexample& witness :
         result.counterexamples) {
        add_bytes(
            bytes,
            estimate_stable_key_bytes(witness.left_state));
        add_bytes(
            bytes,
            estimate_stable_key_bytes(witness.right_state));
        add_bytes(
            bytes,
            estimate_feature_bytes(witness.differing_features));
    }
    return bytes;
}

std::uint64_t estimate_refined_policy_class_memory(
        const RefinedPolicyClass& policy) {
    std::uint64_t bytes = saturated_product(
        policy.exact_members.capacity(), sizeof(StableKey));
    for (const StableKey& member : policy.exact_members) {
        add_bytes(bytes, estimate_stable_key_bytes(member));
    }
    add_bytes(
        bytes,
        estimate_requirement_bytes(policy.required_observations));
    add_bytes(
        bytes,
        estimate_feature_bytes(policy.observation_signature));
    if (policy.selected_action.has_value()) {
        add_bytes(
            bytes,
            estimate_selected_action_bytes(
                *policy.selected_action));
    }
    add_bytes(
        bytes,
        saturated_product(
            policy.transitions.capacity(),
            sizeof(ProjectedTransition)));
    return bytes;
}

std::uint64_t estimate_project_kernel_scratch(
        const Node& node) {
    using ProjectionMap =
        std::map<std::uint32_t, DeterministicSum>;
    std::uint64_t bytes = saturated_product(
        node.edges.size(),
        sizeof(typename ProjectionMap::value_type) +
            3 * sizeof(void*));
    add_bytes(
        bytes,
        saturated_product(
            node.edges.size(),
            sizeof(std::pair<std::uint32_t, DeterministicSum>)));
    return bytes;
}

std::uint64_t estimate_exact_states_memory(
        const std::vector<ExactState>& states) {
    std::uint64_t bytes = saturated_product(
        states.capacity(), sizeof(ExactState));
    for (const ExactState& state : states) {
        add_bytes(bytes, estimate_exact_state_bytes(state));
    }
    return bytes;
}

std::uint64_t estimate_pending_edges_memory(
        const std::map<StableKey, PendingEdge>& combined) {
    std::uint64_t bytes = estimate_ordered_nodes(combined);
    for (const auto& [key, edge] : combined) {
        add_bytes(bytes, estimate_stable_key_bytes(key));
        add_bytes(
            bytes,
            estimate_exact_state_bytes(edge.successor));
    }
    return bytes;
}

std::uint64_t estimate_node_edges_memory(
        const std::vector<NodeEdge>& edges) {
    return saturated_product(
        edges.capacity(), sizeof(NodeEdge));
}

std::uint64_t estimate_discovery_worklists_memory(
        const std::set<StableKey>& pending_exact,
        const std::set<std::uint32_t>& reachable_coarse) {
    std::uint64_t bytes =
        estimate_ordered_nodes(pending_exact);
    for (const StableKey& key : pending_exact) {
        add_bytes(bytes, estimate_stable_key_bytes(key));
    }
    add_bytes(bytes, estimate_ordered_nodes(reachable_coarse));
    return bytes;
}

std::uint64_t estimate_graph_canonicalization_scratch(
        const Graph& graph) {
    std::uint64_t bytes = saturated_product(
        graph.nodes.size(),
        sizeof(Node) + 2 * sizeof(std::uint32_t));
    return bytes;
}

std::uint64_t estimate_policy_observation_nodes_memory(
        const std::vector<PolicyObservationNode>& nodes) {
    std::uint64_t bytes = saturated_product(
        nodes.capacity(), sizeof(PolicyObservationNode));
    for (const PolicyObservationNode& node : nodes) {
        if (node.selected_action.has_value()) {
            add_bytes(
                bytes,
                estimate_selected_action_bytes(
                    *node.selected_action));
        }
        add_bytes(
            bytes,
            estimate_requirement_bytes(node.direct_observes));
        add_bytes(
            bytes,
            saturated_product(
                node.successors.capacity(),
                sizeof(std::uint32_t)));
    }
    return bytes;
}

std::uint64_t estimate_policy_observation_fixed_memory(
        const PolicyObservationFixedPoint& fixed) {
    std::uint64_t bytes = fixed.failure_reason.capacity() + 1;
    add_bytes(
        bytes,
        saturated_product(
            fixed.assignments.capacity(),
            sizeof(PolicyObservationAssignment)));
    for (const PolicyObservationAssignment& assignment :
         fixed.assignments) {
        add_bytes(
            bytes,
            estimate_requirement_bytes(assignment.required));
    }
    return bytes;
}

std::uint64_t estimate_closed_nodes_memory(
        const std::vector<ClosedPartitionNode>& nodes) {
    std::uint64_t bytes = saturated_product(
        nodes.capacity(), sizeof(ClosedPartitionNode));
    for (const ClosedPartitionNode& node : nodes) {
        add_bytes(bytes, estimate_stable_key_bytes(node.stable_key));
        add_bytes(
            bytes,
            estimate_stable_key_bytes(node.observation_key));
        add_bytes(
            bytes,
            estimate_stable_key_bytes(node.immediate_key));
        add_bytes(
            bytes,
            saturated_product(
                node.arcs.capacity(), sizeof(ClosedPartitionArc)));
        for (const ClosedPartitionArc& arc : node.arcs) {
            add_bytes(bytes, estimate_stable_key_bytes(arc.label));
        }
    }
    return bytes;
}

std::uint64_t estimate_canonical_closed_nodes_memory(
        const std::vector<CanonicalClosedNode>& nodes) {
    std::uint64_t bytes = saturated_product(
        nodes.capacity(), sizeof(CanonicalClosedNode));
    for (const CanonicalClosedNode& node : nodes) {
        add_bytes(bytes, estimate_stable_key_bytes(node.stable_key));
        add_bytes(
            bytes,
            estimate_stable_key_bytes(node.observation_key));
        add_bytes(
            bytes,
            estimate_stable_key_bytes(node.immediate_key));
        add_bytes(
            bytes,
            saturated_product(
                node.arcs.capacity(), sizeof(CanonicalClosedArc)));
        for (const CanonicalClosedArc& arc : node.arcs) {
            add_bytes(bytes, estimate_stable_key_bytes(arc.label));
        }
    }
    return bytes;
}

std::uint64_t estimate_closed_partition_result_memory(
        const ClosedPartitionResult& result) {
    std::uint64_t bytes =
        saturated_add(
            result.failure_reason.capacity() + 1,
            result.resource_cap.capacity() + 1);
    add_bytes(
        bytes,
        saturated_product(
            result.initial_class_by_node.capacity(),
            sizeof(std::uint32_t)));
    add_bytes(
        bytes,
        saturated_product(
            result.class_by_node.capacity(),
            sizeof(std::uint32_t)));
    add_bytes(
        bytes,
        saturated_product(
            result.classes.capacity(),
            sizeof(ClosedPartitionClass)));
    for (const ClosedPartitionClass& partition_class :
         result.classes) {
        add_bytes(
            bytes,
            saturated_product(
                partition_class.member_keys.capacity(),
                sizeof(StableKey)));
        for (const StableKey& key : partition_class.member_keys) {
            add_bytes(bytes, estimate_stable_key_bytes(key));
        }
        add_bytes(
            bytes,
            estimate_stable_key_bytes(
                partition_class.observation_key));
        add_bytes(
            bytes,
            estimate_stable_key_bytes(
                partition_class.immediate_key));
        add_bytes(
            bytes,
            saturated_product(
                partition_class.arcs.capacity(),
                sizeof(ClosedPartitionProjectedArc)));
        for (const ClosedPartitionProjectedArc& arc :
             partition_class.arcs) {
            add_bytes(bytes, estimate_stable_key_bytes(arc.label));
        }
    }
    return bytes;
}

std::uint64_t estimate_closed_partition_class_memory(
        const ClosedPartitionClass& partition_class) {
    std::uint64_t bytes = saturated_product(
        partition_class.member_keys.capacity(),
        sizeof(StableKey));
    for (const StableKey& key : partition_class.member_keys) {
        add_bytes(bytes, estimate_stable_key_bytes(key));
    }
    add_bytes(
        bytes,
        estimate_stable_key_bytes(
            partition_class.observation_key));
    add_bytes(
        bytes,
        estimate_stable_key_bytes(
            partition_class.immediate_key));
    add_bytes(
        bytes,
        saturated_product(
            partition_class.arcs.capacity(),
            sizeof(ClosedPartitionProjectedArc)));
    for (const ClosedPartitionProjectedArc& arc :
         partition_class.arcs) {
        add_bytes(bytes, estimate_stable_key_bytes(arc.label));
    }
    return bytes;
}

std::uint64_t estimate_closed_members_memory(
        const std::vector<std::vector<std::uint32_t>>& members) {
    std::uint64_t bytes = saturated_product(
        members.capacity(), sizeof(std::vector<std::uint32_t>));
    for (const std::vector<std::uint32_t>& partition_class :
         members) {
        add_bytes(
            bytes,
            saturated_product(
                partition_class.capacity(),
                sizeof(std::uint32_t)));
    }
    return bytes;
}

std::uint64_t estimate_closed_projection_memory(
        const std::vector<ClosedProjectionEntry>& projection) {
    std::uint64_t bytes = saturated_product(
        projection.capacity(), sizeof(ClosedProjectionEntry));
    for (const ClosedProjectionEntry& arc : projection) {
        add_bytes(bytes, estimate_stable_key_bytes(arc.label));
    }
    return bytes;
}

std::uint64_t estimate_closed_projection_scratch(
        const CanonicalClosedNode& node) {
    using ProjectionKey =
        std::pair<StableKey, std::optional<std::uint32_t>>;
    using ProjectionMap =
        std::map<ProjectionKey, DeterministicSum>;
    std::uint64_t bytes = saturated_product(
        node.arcs.size(), sizeof(ClosedProjectionEntry));
    add_bytes(
        bytes,
        saturated_product(
            node.arcs.size(),
            sizeof(typename ProjectionMap::value_type) +
                3 * sizeof(void*)));
    for (const CanonicalClosedArc& arc : node.arcs) {
        /*
         * The map key and returned projection each own a label copy while
         * project_closed_row() transfers the row out.
         */
        const std::uint64_t label_bytes =
            estimate_stable_key_bytes(arc.label);
        add_bytes(bytes, label_bytes);
        add_bytes(bytes, label_bytes);
    }
    return bytes;
}

std::uint64_t estimate_max_closed_projection_scratch(
        const std::vector<CanonicalClosedNode>& nodes) {
    std::uint64_t maximum = 0;
    for (const CanonicalClosedNode& node : nodes) {
        maximum = std::max(
            maximum,
            estimate_closed_projection_scratch(node));
    }
    return maximum;
}

std::uint64_t estimate_closed_canonicalization_scratch(
        const std::vector<ClosedPartitionNode>& nodes) {
    using ArcKey =
        std::pair<StableKey, std::optional<std::uint32_t>>;
    using GroupedMap = std::map<ArcKey, DeterministicSum>;
    std::uint64_t bytes = saturated_product(
        nodes.size(),
        sizeof(CanonicalClosedNode) +
            2 * sizeof(std::uint32_t));
    std::uint64_t largest_row = 0;
    for (const ClosedPartitionNode& node : nodes) {
        /*
         * Stable keys and labels move from the input into the canonical
         * graph. The input estimate already owns those buffers; only the new
         * vector element storage is additional across the full graph.
         */
        add_bytes(
            bytes,
            saturated_product(
                node.arcs.size(), sizeof(CanonicalClosedArc)));

        std::uint64_t row = saturated_product(
            node.arcs.size(), sizeof(ClosedPartitionArc));
        add_bytes(
            row,
            saturated_product(
                node.arcs.size(),
                sizeof(typename GroupedMap::value_type) +
                    3 * sizeof(void*)));
        for (const ClosedPartitionArc& arc : node.arcs) {
            const std::uint64_t label_bytes =
                estimate_stable_key_bytes(arc.label);
            add_bytes(row, label_bytes);
            add_bytes(row, label_bytes);
        }
        largest_row = std::max(largest_row, row);
    }
    add_bytes(bytes, largest_row);
    return bytes;
}

std::uint64_t estimate_closed_partition_map_memory(
        const std::map<ClosedPartitionKey, std::uint32_t>& classes) {
    std::uint64_t bytes = estimate_ordered_nodes(classes);
    for (const auto& [key, unused] : classes) {
        (void)unused;
        add_bytes(bytes, estimate_stable_key_bytes(key));
    }
    return bytes;
}

bool check_closed_partition_memory(
        ClosedPartitionResult& result,
        const ClosedPartitionLimits& limits,
        const std::uint64_t owned_live_memory) {
    const std::uint64_t total = saturated_add(
        limits.retained_estimated_memory_bytes,
        owned_live_memory);
    result.estimated_memory_bytes = total;
    result.peak_estimated_memory_bytes = std::max(
        result.peak_estimated_memory_bytes, total);
    if (total != std::numeric_limits<std::uint64_t>::max() &&
        total <= limits.max_estimated_memory_bytes) {
        return true;
    }
    result.status = ClosedPartitionStatus::ResourceCap;
    result.lumpable = false;
    result.resource_cap = "max_estimated_memory_bytes";
    result.failure_reason =
        "closed probabilistic partition reached "
        "max_estimated_memory_bytes";
    return false;
}

template <typename Signature>
bool exact_closed_partition(
        const std::size_t node_count,
        Signature signature,
        const std::uint64_t retained_owned_memory,
        const std::uint64_t per_signature_scratch,
        const ClosedPartitionLimits& limits,
        ClosedPartitionResult& result,
        std::vector<std::uint32_t>& output,
        std::uint32_t& class_count) {
    std::map<ClosedPartitionKey, std::uint32_t> classes;
    std::uint64_t projected = retained_owned_memory;
    add_bytes(projected, per_signature_scratch);
    add_bytes(
        projected,
        saturated_product(
            node_count, sizeof(std::uint32_t)));
    if (!check_closed_partition_memory(
            result, limits, projected)) {
        return false;
    }
    std::vector<std::uint32_t> partition(node_count);
    std::uint32_t next = 0;
    for (std::uint32_t node = 0; node < node_count; ++node) {
        ClosedPartitionKey key = signature(node);
        std::uint64_t live = retained_owned_memory;
        add_bytes(live, per_signature_scratch);
        add_bytes(
            live,
            saturated_product(
                partition.capacity(), sizeof(std::uint32_t)));
        add_bytes(
            live,
            estimate_closed_partition_map_memory(classes));
        add_bytes(live, estimate_stable_key_bytes(key));
        if (!check_closed_partition_memory(
                result, limits, live)) {
            return false;
        }
        const auto [it, inserted] =
            classes.emplace(std::move(key), next);
        if (inserted) ++next;
        partition[node] = it->second;
        live = retained_owned_memory;
        add_bytes(live, per_signature_scratch);
        add_bytes(
            live,
            saturated_product(
                partition.capacity(), sizeof(std::uint32_t)));
        add_bytes(
            live,
            estimate_closed_partition_map_memory(classes));
        if (!check_closed_partition_memory(
                result, limits, live)) {
            return false;
        }
    }
    output = std::move(partition);
    class_count = next;
    return true;
}

void update_memory(
        RefinementResult& result,
        const Graph& graph,
        const std::uint64_t adapter_memory = 0,
        const std::uint64_t transient_memory = 0) {
    std::uint64_t live = estimate_graph_memory(graph);
    add_bytes(live, adapter_memory);
    add_bytes(live, estimate_refinement_result_memory(result));
    add_bytes(live, transient_memory);
    result.telemetry.estimated_memory_bytes = live;
    result.telemetry.peak_estimated_memory_bytes = std::max(
        result.telemetry.peak_estimated_memory_bytes,
        result.telemetry.estimated_memory_bytes);
}

bool fail(
        RefinementResult& result,
        const RefinementStatus status,
        std::string reason) {
    result.status = status;
    result.executable = false;
    result.lumpable = false;
    result.failure_reason = std::move(reason);
    return false;
}

bool cap(
        RefinementResult& result,
        std::string name,
        std::string reason) {
    result.resource_cap = std::move(name);
    return fail(result, RefinementStatus::ResourceCap, std::move(reason));
}

bool refinement_round_cap(
        RefinementResult& result,
        std::string reason) {
    result.resource_cap = "max_refinement_rounds";
    return fail(
        result, RefinementStatus::RefinementRoundCap,
        std::move(reason));
}

bool check_refinement_memory(
        RefinementResult& result,
        const Graph& graph,
        const RefinementLimits& limits,
        const std::uint64_t adapter_memory,
        const std::uint64_t transient_memory = 0) {
    update_memory(
        result, graph, adapter_memory, transient_memory);
    if (result.telemetry.estimated_memory_bytes !=
            std::numeric_limits<std::uint64_t>::max() &&
        result.telemetry.estimated_memory_bytes <=
            limits.max_estimated_memory_bytes) {
        return true;
    }
    return cap(
        result, "max_estimated_memory_bytes",
        "policy refinement reached max_estimated_memory_bytes");
}

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
        const std::uint64_t transient_memory) {
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
    graph.coarse_key_by_state.emplace(
        state.coarse_state, state.coarse_state_key);
    graph.coarse_state_by_key.emplace(
        state.coarse_state_key, state.coarse_state);
    if (state.goal && !state.terminal) {
        return fail(
            result, RefinementStatus::InvalidState,
            "goal exact refinement is not terminal");
    }
    const auto found = graph.index_by_key.find(state.stable_key);
    if (found != graph.index_by_key.end()) {
        index = found->second;
        Node& existing = graph.nodes.at(index);
        if (!(existing.state == state)) {
            return fail(
                result, RefinementStatus::InvalidState,
                "stable exact state key names incompatible records");
        }
        return true;
    }
    if (graph.nodes.size() >= limits.max_exact_states) {
        return cap(
            result, "max_exact_states",
            "policy refinement reached max_exact_states");
    }
    index = static_cast<std::uint32_t>(graph.nodes.size());
    graph.index_by_key.emplace(state.stable_key, index);
    if (!check_refinement_memory(
            result, graph, limits, adapter_memory,
            transient_memory)) {
        return false;
    }
    Node node;
    node.state = std::move(state);
    graph.nodes.push_back(std::move(node));
    result.telemetry.exact_states =
        static_cast<std::uint32_t>(graph.nodes.size());
    return check_refinement_memory(
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
        const std::uint64_t caller_transient_memory) {
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
    std::uint64_t transient = caller_transient_memory;
    add_bytes(
        transient,
        estimate_selected_action_bytes(selection));
    add_bytes(
        transient,
        estimate_stable_key_bytes(signature));
    return check_refinement_memory(
        result, graph, limits, adapter_memory, transient);
}

bool expand_node(
        PolicyRefinementOracle& oracle,
        Graph& graph,
        const std::uint32_t node_index,
        std::set<StableKey>& pending_exact,
        std::set<std::uint32_t>& reachable_coarse,
        const RefinementLimits& limits,
        RefinementResult& result,
        const std::uint64_t caller_transient_memory) {
    Node& node = graph.nodes.at(node_index);
    if (node.expanded) return true;
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
    if (!validate_selection(
            graph, *selected, result, limits,
            oracle.estimated_owned_bytes(), selection_context)) {
        return false;
    }
    const std::uint64_t selected_memory =
        estimate_selected_action_bytes(*selected);
    std::uint64_t discovery_memory =
        estimate_discovery_worklists_memory(
            pending_exact, reachable_coarse);
    add_bytes(discovery_memory, caller_transient_memory);
    add_bytes(discovery_memory, selected_memory);
    if (!check_refinement_memory(
            result, graph, limits,
            oracle.estimated_owned_bytes(),
            discovery_memory)) {
        return false;
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

    std::map<StableKey, PendingEdge> combined;
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
        auto [it, inserted] = combined.emplace(
            transition.successor.stable_key,
            PendingEdge{transition.successor, {}});
        if (!inserted &&
            !(it->second.successor == transition.successor)) {
            return fail(
                result, RefinementStatus::InvalidKernel,
                "one exact successor key names incompatible records");
        }
        it->second.probability.add(transition.probability);
        total.add(transition.probability);
        std::uint64_t transient =
            estimate_discovery_worklists_memory(
                pending_exact, reachable_coarse);
        add_bytes(transient, caller_transient_memory);
        add_bytes(transient, selected_memory);
        add_bytes(
            transient, estimate_exact_kernel_bytes(kernel));
        add_bytes(
            transient,
            estimate_pending_edges_memory(combined));
        if (!check_refinement_memory(
                result, graph, limits, adapter_memory,
                transient)) {
            return false;
        }
    }
    if (combined.empty() ||
        std::abs(total.value() - 1.0) >
            limits.probability_sum_tolerance) {
        return fail(
            result, RefinementStatus::InvalidKernel,
            "exact kernel is empty or does not sum to one");
    }
    if (result.telemetry.exact_transitions + combined.size() >
        limits.max_transitions) {
        return cap(
            result, "max_transitions",
            "policy refinement reached max_transitions");
    }

    std::vector<NodeEdge> edges;
    edges.reserve(combined.size());
    {
        std::uint64_t transient =
            estimate_discovery_worklists_memory(
                pending_exact, reachable_coarse);
        add_bytes(transient, caller_transient_memory);
        add_bytes(transient, selected_memory);
        add_bytes(
            transient, estimate_exact_kernel_bytes(kernel));
        add_bytes(
            transient,
            estimate_pending_edges_memory(combined));
        add_bytes(transient, estimate_node_edges_memory(edges));
        if (!check_refinement_memory(
                result, graph, limits, adapter_memory,
                transient)) {
            return false;
        }
    }
    for (auto& [unused, edge] : combined) {
        (void)unused;
        if (!note_reachable_coarse(
                edge.successor.coarse_state, reachable_coarse,
                limits, result)) {
            return false;
        }
        std::uint32_t successor = 0;
        std::uint64_t transient =
            estimate_discovery_worklists_memory(
                pending_exact, reachable_coarse);
        add_bytes(transient, caller_transient_memory);
        add_bytes(transient, selected_memory);
        add_bytes(
            transient, estimate_exact_kernel_bytes(kernel));
        add_bytes(
            transient,
            estimate_pending_edges_memory(combined));
        add_bytes(transient, estimate_node_edges_memory(edges));
        if (!add_or_verify_state(
                graph, std::move(edge.successor), successor, result,
                limits, adapter_memory, transient)) {
            return false;
        }
        edges.push_back({successor, edge.probability});
        if (!graph.nodes.at(successor).expanded) {
            pending_exact.insert(
                graph.nodes.at(successor).state.stable_key);
        }
        transient =
            estimate_discovery_worklists_memory(
                pending_exact, reachable_coarse);
        add_bytes(transient, caller_transient_memory);
        add_bytes(transient, selected_memory);
        add_bytes(
            transient, estimate_exact_kernel_bytes(kernel));
        add_bytes(
            transient,
            estimate_pending_edges_memory(combined));
        add_bytes(transient, estimate_node_edges_memory(edges));
        if (!check_refinement_memory(
                result, graph, limits, adapter_memory,
                transient)) {
            return false;
        }
    }
    Node& refreshed = graph.nodes.at(node_index);
    refreshed.selection = std::move(selected);
    refreshed.action_cost = kernel.action_cost;
    refreshed.edges = std::move(edges);
    refreshed.expanded = true;
    ++result.telemetry.exact_kernels;
    result.telemetry.exact_transitions += refreshed.edges.size();
    std::uint64_t transient =
        estimate_discovery_worklists_memory(
            pending_exact, reachable_coarse);
    add_bytes(transient, caller_transient_memory);
    add_bytes(
        transient, estimate_exact_kernel_bytes(kernel));
    add_bytes(
        transient, estimate_pending_edges_memory(combined));
    return check_refinement_memory(
        result, graph, limits, adapter_memory, transient);
}

bool discover_policy_graph(
        PolicyRefinementOracle& oracle,
        Graph& graph,
        const RefinementRequest& request,
        RefinementResult& result) {
    std::set<std::uint32_t> reachable_coarse;
    std::set<StableKey> pending_exact;

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
            pending_exact.insert(graph.nodes.at(index).state.stable_key);
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
     * Stable keys make the worklist deterministic even when root enumeration
     * or kernel transition records arrive in a different order.
     */
    while (!pending_exact.empty()) {
        const StableKey key = *pending_exact.begin();
        pending_exact.erase(pending_exact.begin());
        const auto found = graph.index_by_key.find(key);
        if (found == graph.index_by_key.end()) {
            return fail(
                result, RefinementStatus::InvalidState,
                "pending exact state disappeared during discovery");
        }
        if (!expand_node(
                oracle, graph, found->second, pending_exact,
                reachable_coarse, request.limits, result,
                estimate_stable_key_bytes(key))) {
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
                       a.state.coarse_state_key,
                       a.state.stable_key) <
                   std::tie(
                       b.state.coarse_state_key,
                       b.state.stable_key);
        });
    std::vector<std::uint32_t> remap(order.size());
    std::vector<Node> sorted;
    sorted.reserve(order.size());
    for (std::uint32_t next = 0; next < order.size(); ++next) {
        remap[order[next]] = next;
        sorted.push_back(std::move(graph.nodes[order[next]]));
    }
    for (Node& node : sorted) {
        for (NodeEdge& edge : node.edges) {
            edge.successor = remap.at(edge.successor);
        }
        std::sort(
            node.edges.begin(), node.edges.end(),
            [](const NodeEdge& left, const NodeEdge& right) {
                return left.successor < right.successor;
            });
    }
    graph.nodes = std::move(sorted);
}

bool propagate_observations(
        Graph& graph,
        const RefinementLimits& limits,
        RefinementResult& result,
        const std::uint64_t adapter_memory) {
    std::vector<PolicyObservationNode> nodes;
    nodes.reserve(graph.nodes.size());
    for (std::uint32_t index = 0;
         index < graph.nodes.size(); ++index) {
        PolicyObservationNode node;
        node.state_id = index;
        node.selected_action = graph.nodes[index].selection;
        for (const NodeEdge& edge : graph.nodes[index].edges) {
            node.successors.push_back(edge.successor);
        }
        nodes.push_back(std::move(node));
        if (!check_refinement_memory(
                result, graph, limits, adapter_memory,
                estimate_policy_observation_nodes_memory(nodes))) {
            return false;
        }
    }
    const std::uint64_t observation_nodes_memory =
        estimate_policy_observation_nodes_memory(nodes);
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
    for (PolicyObservationAssignment& assignment :
         fixed.assignments) {
        graph.nodes.at(assignment.state_id).required =
            std::move(assignment.required);
        if (!check_refinement_memory(
                result, graph, limits, adapter_memory,
                estimate_policy_observation_fixed_memory(fixed))) {
            return false;
        }
    }
    return true;
}

using BehaviorKey = std::vector<std::uint64_t>;

BehaviorKey initial_behavior_key(const Node& node) {
    BehaviorKey out;
    append_tokens(out, node.state.coarse_state_key);
    out.push_back(node.state.goal ? 1u : 0u);
    out.push_back(node.state.terminal ? 1u : 0u);
    if (!node.selection.has_value()) {
        out.push_back(0);
        return out;
    }
    out.push_back(1);
    out.push_back(node.selection->action_id);
    append_tokens(out, node.selection->semantic_key);
    append_requirement(out, node.required);
    const FeatureSignature observed =
        observe_features(node.state.features, node.required);
    out.push_back(observed.size());
    for (const FeatureAtom& atom : observed) append_atom(out, atom);
    return out;
}

bool same_selected_decision(
        const Node& left,
        const Node& right) {
    if (left.selection.has_value() !=
        right.selection.has_value()) {
        return false;
    }
    if (!left.selection.has_value()) return true;
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
            !graph.nodes[node].selection.has_value()) {
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
    for (std::size_t left_index = 0;
         left_index < members.size(); ++left_index) {
        for (std::size_t right_index = left_index + 1;
             right_index < members.size(); ++right_index) {
            Node& left = graph.nodes[members[left_index]];
            Node& right = graph.nodes[members[right_index]];
            if (left.state.coarse_state_key !=
                    right.state.coarse_state_key ||
                same_selected_decision(left, right) ||
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
               graph.nodes[members[parent_begin]]
                       .state.coarse_state_key ==
                   graph.nodes[members[parent_end]]
                       .state.coarse_state_key) {
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
        const Node& node,
        const std::vector<std::uint32_t>& partition) {
    std::map<std::uint32_t, DeterministicSum> projected;
    for (const NodeEdge& edge : node.edges) {
        projected[partition.at(edge.successor)].add(edge.probability);
    }
    return {projected.begin(), projected.end()};
}

FeatureSignature relevant_difference(
        const Node& left,
        const Node& right) {
    const ObservationRequirement requirement =
        merge_requirements(left.required, right.required);
    const FeatureSignature a =
        observe_features(left.state.features, requirement);
    const FeatureSignature b =
        observe_features(right.state.features, requirement);
    FeatureSignature out;
    std::set_symmetric_difference(
        a.begin(), a.end(), b.begin(), b.end(),
        std::back_inserter(out), atom_less);
    return out;
}

CounterexampleKind counterexample_kind(
        const Node& left,
        const Node& right,
        const std::vector<std::uint32_t>& partition) {
    if (left.selection.has_value() != right.selection.has_value()) {
        return CounterexampleKind::SelectedAction;
    }
    if (!left.selection.has_value()) {
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
    if (project_kernel(left, partition) !=
        project_kernel(right, partition)) {
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
            counterexample_kind(left, right, final),
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
        const Graph& graph,
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
    for (std::uint32_t class_id = 0;
         class_id < class_count; ++class_id) {
        members[class_id].reserve(member_counts[class_id]);
    }
    for (std::uint32_t state = 0; state < graph.nodes.size(); ++state) {
        members.at(partition[state]).push_back(state);
    }
    result.classes.reserve(class_count);
    result.assignments.reserve(graph.nodes.size());
    const auto members_memory = [&]() {
        std::uint64_t bytes = saturated_product(
            members.capacity(),
            sizeof(std::vector<std::uint32_t>));
        for (const std::vector<std::uint32_t>& states : members) {
            add_bytes(
                bytes,
                saturated_product(
                    states.capacity(), sizeof(std::uint32_t)));
        }
        add_bytes(
            bytes,
            saturated_product(
                member_counts.capacity(), sizeof(std::size_t)));
        return bytes;
    };
    for (std::uint32_t class_id = 0;
         class_id < class_count; ++class_id) {
        const Node& authority =
            graph.nodes.at(members[class_id].front());
        std::uint64_t class_live =
            saturated_add(retained_scratch, members_memory());
        add_bytes(
            class_live,
            estimate_project_kernel_scratch(authority));
        if (!check_refinement_memory(
                result, graph, limits, adapter_memory,
                class_live)) {
            return false;
        }
        const auto projection = project_kernel(authority, partition);
        const std::uint64_t projection_memory =
            saturated_product(
                projection.capacity(),
                sizeof(std::pair<
                    std::uint32_t, DeterministicSum>));
        std::uint64_t projected_output =
            saturated_product(
                members[class_id].size(), sizeof(StableKey));
        for (const std::uint32_t state : members[class_id]) {
            add_bytes(
                projected_output,
                estimate_stable_key_bytes(
                    graph.nodes[state].state.stable_key));
        }
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
        if (authority.selection.has_value()) {
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
            saturated_add(retained_scratch, members_memory());
        add_bytes(class_live, projection_memory);
        add_bytes(class_live, projected_output);
        if (!check_refinement_memory(
                result, graph, limits, adapter_memory,
                class_live)) {
            return false;
        }
        RefinedPolicyClass output;
        output.class_id = class_id;
        output.coarse_state = authority.state.coarse_state;
        output.coarse_state_key =
            authority.state.coarse_state_key;
        output.goal = authority.state.goal;
        output.terminal = authority.state.terminal;
        output.required_observations = authority.required;
        output.observation_signature = observe_features(
            authority.state.features, authority.required);
        output.selected_action = authority.selection;
        output.action_cost = authority.action_cost;
        output.exact_members.reserve(members[class_id].size());
        output.transitions.reserve(projection.size());
        for (const std::uint32_t state : members[class_id]) {
            output.exact_members.push_back(
                graph.nodes[state].state.stable_key);
            class_live =
                saturated_add(retained_scratch, members_memory());
            add_bytes(class_live, projection_memory);
            add_bytes(
                class_live,
                estimate_refined_policy_class_memory(output));
            if (!check_refinement_memory(
                    result, graph, limits, adapter_memory,
                    class_live)) {
                return false;
            }
        }
        for (const auto& [successor, probability] : projection) {
            output.transitions.push_back(
                {successor, probability.value()});
            class_live =
                saturated_add(retained_scratch, members_memory());
            add_bytes(class_live, projection_memory);
            add_bytes(
                class_live,
                estimate_refined_policy_class_memory(output));
            if (!check_refinement_memory(
                    result, graph, limits, adapter_memory,
                    class_live)) {
                return false;
            }
        }
        result.classes.push_back(std::move(output));
        if (!check_refinement_memory(
                result, graph, limits, adapter_memory,
                saturated_add(
                    retained_scratch, members_memory()))) {
            return false;
        }
    }
    for (std::uint32_t state = 0; state < graph.nodes.size(); ++state) {
        std::uint64_t assignment_live =
            saturated_add(retained_scratch, members_memory());
        add_bytes(
            assignment_live,
            estimate_stable_key_bytes(
                graph.nodes[state].state.stable_key));
        if (!check_refinement_memory(
                result, graph, limits, adapter_memory,
                assignment_live)) {
            return false;
        }
        result.assignments.push_back({
            graph.nodes[state].state.stable_key,
            graph.nodes[state].state.coarse_state,
            partition[state],
            graph.nodes[state].state.coarse_state_key});
        if (!check_refinement_memory(
                result, graph, limits, adapter_memory,
                saturated_add(
                    retained_scratch, members_memory()))) {
            return false;
        }
    }
    return true;
}

std::vector<std::uint32_t> mask_members(
        const SessionImpl& session,
        const std::vector<std::uint64_t>& mask) {
    std::vector<std::uint32_t> members;
    if (mask.size() < session.words) return members;
    for (std::uint32_t mod = 0; mod < session.mod_count; ++mod) {
        if (pc_bitset_test(mask.data(), mod)) members.push_back(mod);
    }
    return members;
}

std::vector<std::uint32_t> relevant_observation_tags(
        const ObservationRequirement& requirement) {
    std::vector<std::uint32_t> tags = requirement.modifier_tag_ids;
    for (const RefinementAffixObservation& observation :
         requirement.affix_observations) {
        tags.insert(
            tags.end(),
            observation.selector.required_tag_ids.begin(),
            observation.selector.required_tag_ids.end());
    }
    std::sort(tags.begin(), tags.end());
    tags.erase(std::unique(tags.begin(), tags.end()), tags.end());
    return tags;
}

std::vector<std::uint32_t> mod_relevant_tags(
        const SessionImpl& session,
        const std::uint32_t mod,
        const std::vector<std::uint32_t>& relevant) {
    std::vector<std::uint32_t> tags;
    if (mod + 1 >= session.class_offsets.size()) return tags;
    for (std::uint32_t offset = session.class_offsets[mod];
         offset < session.class_offsets[mod + 1]; ++offset) {
        const std::uint32_t tag = session.class_tag_ids.at(offset);
        if (std::binary_search(relevant.begin(), relevant.end(), tag)) {
            tags.push_back(tag);
        }
    }
    std::sort(tags.begin(), tags.end());
    tags.erase(std::unique(tags.begin(), tags.end()), tags.end());
    return tags;
}

StableKey mod_count_observation_bits(
        const AbstractLayout& layout,
        const std::uint32_t mod) {
    StableKey bits((layout.count_observations.size() + 63) / 64, 0);
    for (std::size_t observation = 0;
         observation < layout.count_observations.size();
         ++observation) {
        const std::vector<std::uint64_t>& mask =
            layout.count_observations[observation].member_mask;
        if (!mask.empty() && pc_bitset_test(mask.data(), mod)) {
            bits[observation / 64] |=
                std::uint64_t{1} << (observation % 64);
        }
    }
    return bits;
}

template <typename Value, typename Extract>
std::optional<Value> uniform_member_value(
        const std::vector<std::uint32_t>& members,
        Extract extract) {
    if (members.empty()) return std::nullopt;
    const Value first = extract(members.front());
    for (std::size_t i = 1; i < members.size(); ++i) {
        if (!(extract(members[i]) == first)) return std::nullopt;
    }
    return first;
}

std::uint16_t affix_traits(
        const AbstractState& state,
        const std::int8_t side,
        const bool crafted,
        const bool fractured,
        const bool veiled) {
    std::uint16_t traits =
        side == PC_SIDE_PREFIX
            ? kRefinementAffixPrefix
            : kRefinementAffixSuffix;
    if (crafted) traits |= kRefinementAffixCrafted;
    if (fractured) traits |= kRefinementAffixFractured;
    if (veiled) traits |= kRefinementAffixVeiled;
    if ((side == PC_SIDE_PREFIX &&
         (state.flags & kFlagPrefixesLocked) != 0) ||
        (side == PC_SIDE_SUFFIX &&
         (state.flags & kFlagSuffixesLocked) != 0)) {
        traits |= kRefinementAffixOnLockedSide;
    }
    if (state.searing_exarch_tier != state.eater_of_worlds_tier) {
        const std::int8_t dominant =
            state.searing_exarch_tier > state.eater_of_worlds_tier
                ? PC_SIDE_PREFIX
                : PC_SIDE_SUFFIX;
        traits |= side == dominant
                      ? kRefinementAffixOnEldritchDominantSide
                      : kRefinementAffixOnEldritchNonDominantSide;
    }
    return traits;
}

std::uint8_t refinement_item_traits(const AbstractState& state) {
    std::uint8_t traits =
        state.searing_exarch_tier != state.eater_of_worlds_tier
            ? kRefinementItemHasEldritchDominance
            : 0;
    const bool prefix_locked =
        (state.flags & kFlagPrefixesLocked) != 0;
    const bool suffix_locked =
        (state.flags & kFlagSuffixesLocked) != 0;
    if (prefix_locked != suffix_locked) {
        traits |= kRefinementItemExactlyOneSideLocked;
    }
    return traits;
}

} // namespace

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
        if (nodes[index].selected_action.has_value() &&
            !nodes[index].selected_action->contract.complete()) {
            result.failure_reason =
                "policy observation graph has an incomplete action "
                "contract";
            return result;
        }
        if (nodes[index].selected_action.has_value() &&
            !selected_runtime_contracts_complete(
                *nodes[index].selected_action)) {
            result.failure_reason =
                "policy observation graph has an incomplete or empty "
                "runtime path contract";
            return result;
        }
    }
    std::vector<ObservationRequirement> required(nodes.size());
    for (std::uint32_t index = 0; index < nodes.size(); ++index) {
        if (nodes[index].selected_action.has_value()) {
            const SelectedAction& selected =
                *nodes[index].selected_action;
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
        for (std::uint32_t source = 0;
             source < nodes.size(); ++source) {
            const PolicyObservationNode& node = nodes[source];
            for (const std::uint32_t successor :
                 node.successors) {
                const auto target =
                    index_by_state.find(successor);
                if (target == index_by_state.end()) {
                    result.failure_reason =
                        "policy observation graph has an unknown "
                        "successor";
                    return result;
                }
                const ObservationRequirement carried =
                    node.selected_action.has_value()
                        ? selected_preserved_requirement(
                              required[target->second],
                              *node.selected_action)
                        : required[target->second];
                ObservationRequirement merged =
                    merge_requirements(
                        next[source], carried);
                if (!(merged == next[source])) {
                    next[source] = std::move(merged);
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
        if (!nodes[index].selected_action.has_value()) {
            continue;
        }
        const RefinementFeatureMask destroyed =
            selected_destroyed_feature_mask(
                required[index],
                *nodes[index].selected_action);
        const RefinementFeatureMask preserved =
            selected_preserved_feature_mask(
                required[index],
                *nodes[index].selected_action);
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
                return closed_initial_key(canonical[node]);
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
        if (!exact_closed_partition(
                canonical.size(),
                [&](const std::uint32_t node) {
                    return closed_refined_key(
                        canonical[node], partition[node],
                        partition);
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
        const CanonicalClosedNode& authority =
            canonical[members[class_id].front()];
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
                        authority)))) {
            return result;
        }
        const std::vector<ClosedProjectionEntry> projection =
            project_closed_row(authority, partition);
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
            ++result.lumpability_checks;
            std::uint64_t proof_live = class_base;
            add_bytes(
                proof_live,
                estimate_closed_projection_memory(projection));
            add_bytes(
                proof_live,
                estimate_closed_projection_scratch(candidate));
            if (!check_closed_partition_memory(
                    result, limits, proof_live)) {
                return result;
            }
            if (candidate.terminal != authority.terminal ||
                candidate.observation_key !=
                    authority.observation_key ||
                candidate.immediate_key != authority.immediate_key ||
                project_closed_row(candidate, partition) !=
                    projection) {
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
                authority.observation_key));
        add_bytes(
            projected_output,
            estimate_stable_key_bytes(
                authority.immediate_key));
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
        output.observation_key = authority.observation_key;
        output.immediate_key = authority.immediate_key;
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

FeatureSignature observe_features(
        const FeatureSignature& exact_features,
        const ObservationRequirement& input_requirement) {
    const ObservationRequirement requirement =
        canonical_observation_requirement(input_requirement);
    FeatureSignature item;
    std::map<std::uint32_t, FeatureSignature> affixes;
    for (const FeatureAtom& input : exact_features) {
        const RefinementFeatureMask bit =
            refinement_feature(input.feature);
        if (is_item_feature(input.feature)) {
            if ((requirement.item_features & bit) == 0) continue;
            FeatureAtom atom = input;
            atom.affix_traits = 0;
            atom.item_traits = 0;
            atom.modifier_tag_ids.clear();
            item.push_back(std::move(atom));
            continue;
        }
        bool observed = false;
        for (const RefinementAffixObservation& term :
             requirement.affix_observations) {
            if ((term.features & bit) != 0 &&
                refinement_selector_matches(
                    term.selector, input.affix_traits,
                    input.item_traits, input.modifier_tag_ids)) {
                observed = true;
                break;
            }
        }
        if (!observed) continue;
        FeatureAtom atom = input;
        if (input.feature ==
            RefinementFeature::ModifierClassificationTags) {
            atom.value.clear();
            std::set_intersection(
                input.modifier_tag_ids.begin(),
                input.modifier_tag_ids.end(),
                requirement.modifier_tag_ids.begin(),
                requirement.modifier_tag_ids.end(),
                std::back_inserter(atom.value));
        }
        atom.affix_traits = 0;
        atom.item_traits = 0;
        atom.modifier_tag_ids.clear();
        affixes[input.subject].push_back(std::move(atom));
    }
    struct Bundle {
        FeatureSignature atoms;
    };
    std::vector<Bundle> bundles;
    for (auto& [unused, atoms] : affixes) {
        (void)unused;
        for (FeatureAtom& atom : atoms) atom.subject = 0;
        std::sort(atoms.begin(), atoms.end(), atom_less);
        bundles.push_back({std::move(atoms)});
    }
    const auto bundle_less = [](const Bundle& left, const Bundle& right) {
        return std::lexicographical_compare(
            left.atoms.begin(), left.atoms.end(),
            right.atoms.begin(), right.atoms.end(), atom_less);
    };
    std::sort(bundles.begin(), bundles.end(), bundle_less);
    FeatureSignature out = std::move(item);
    for (std::uint32_t subject = 0;
         subject < bundles.size(); ++subject) {
        for (FeatureAtom atom : bundles[subject].atoms) {
            atom.subject = subject;
            out.push_back(std::move(atom));
        }
    }
    return canonical_feature_signature(std::move(out));
}

std::shared_ptr<const CompiledObservationProgram>
make_compiled_observation_program(
        const SessionImpl& session,
        const CompiledObservationSignature& compiled) {
    if (compiled.version !=
        kObservationSignatureConditionVersion) {
        throw std::invalid_argument(
            "unsupported compiled observation-signature version");
    }
    auto program =
        std::make_shared<CompiledObservationProgram>();
    program->requirement.item_features =
        compiled.item_features;
    program->requirement.modifier_tag_ids =
        compiled.modifier_tag_ids;
    for (const CompiledObservationAffixRequirement& input :
         compiled.affix_observations) {
        RefinementAffixObservation observation;
        observation.features = input.features;
        observation.selector.required_affix_traits =
            input.selector.required_affix_traits;
        observation.selector.forbidden_affix_traits =
            input.selector.forbidden_affix_traits;
        observation.selector.required_item_traits =
            input.selector.required_item_traits;
        observation.selector.forbidden_item_traits =
            input.selector.forbidden_item_traits;
        observation.selector.required_tag_ids =
            input.selector.required_tag_ids;
        program->requirement.affix_observations.push_back(
            std::move(observation));
    }
    program->requirement =
        canonical_observation_requirement(
            std::move(program->requirement));
    for (const CompiledObservationAtom& atom :
         compiled.atoms) {
        program->signature.push_back({
            static_cast<RefinementFeature>(atom.feature),
            atom.subject,
            atom.value,
            0,
            0,
            {}});
    }
    program->signature =
        canonical_feature_signature(
            std::move(program->signature));
    program->context.goal_status_tier_class_by_mod.resize(
        session.mod_count);
    for (const CompiledObservationModValue& entry :
         compiled.goal_status_tier_class_by_mod) {
        if (entry.mod_id >= session.mod_count) {
            throw std::invalid_argument(
                "compiled goal observation references an unknown "
                "modifier");
        }
        if (entry.value.size() != 2 ||
            entry.value[0] == 0 ||
            entry.value[1] >
                static_cast<std::uint8_t>(
                    GoalSlotStatus::Satisfied)) {
            throw std::invalid_argument(
                "compiled goal observation has an invalid semantic "
                "class");
        }
        program->context.goal_status_tier_class_by_mod[
            entry.mod_id] = entry.value;
    }
    program->context
        .count_observation_membership_by_mod.resize(
            session.mod_count,
            StableKey(
                (compiled.count_observation_count + 63) / 64,
                0));
    for (const CompiledObservationModValue& entry :
         compiled.count_observation_membership_by_mod) {
        if (entry.mod_id >= session.mod_count) {
            throw std::invalid_argument(
                "compiled count observation references an unknown "
                "modifier");
        }
        const std::size_t expected_words =
            (compiled.count_observation_count + 63) / 64;
        if (entry.value.size() != expected_words ||
            (!entry.value.empty() &&
             compiled.count_observation_count % 64 != 0 &&
             (entry.value.back() >>
              (compiled.count_observation_count % 64)) != 0)) {
            throw std::invalid_argument(
                "compiled count observation has an invalid semantic "
                "membership vector");
        }
        program->context
            .count_observation_membership_by_mod[
                entry.mod_id] = entry.value;
    }
    return program;
}

FeatureSignature observe_exact_item_features(
        const SessionImpl& session,
        const pc_item_state& item,
        const ObservationRequirement& input_requirement,
        const ExactObservationContext& context) {
    const ObservationRequirement requirement =
        canonical_observation_requirement(input_requirement);
    FeatureSignature exact;
    const auto emit_item =
        [&](const RefinementFeature feature, StableKey value) {
            exact.push_back(
                {feature, 0, std::move(value), 0, 0, {}});
        };

    bool crafted = false;
    bool fractured = false;
    bool veiled = false;
    bool multimod = false;
    bool no_attack = false;
    bool no_caster = false;
    bool prefix_lock = false;
    bool suffix_lock = false;
    const DataImpl& data = *session.data;
    const auto scan =
        [&](const pc_mod_slot* slots, const std::uint8_t count) {
            for (std::uint8_t index = 0; index < count; ++index) {
                const pc_mod_slot& slot = slots[index];
                crafted =
                    crafted ||
                    (slot.flags & PC_MOD_SLOT_CRAFTED) != 0;
                fractured =
                    fractured ||
                    (slot.flags & PC_MOD_SLOT_FRACTURED) != 0;
                veiled =
                    veiled ||
                    (slot.flags & PC_MOD_SLOT_VEILED) != 0;
                if (slot.mod_id >= session.metamod_type.size()) {
                    continue;
                }
                const std::int32_t role =
                    session.metamod_type[slot.mod_id];
                if (role < 0) {
                    continue;
                }
                multimod =
                    multimod ||
                    role == data.metamod_multimod_code;
                no_attack =
                    no_attack ||
                    role == data.metamod_no_attack_code;
                no_caster =
                    no_caster ||
                    role == data.metamod_no_caster_code;
                prefix_lock =
                    prefix_lock ||
                    role == data.metamod_prefixes_locked_code;
                suffix_lock =
                    suffix_lock ||
                    role == data.metamod_suffixes_locked_code;
            }
        };
    scan(item.prefixes, item.prefix_count);
    scan(item.suffixes, item.suffix_count);

    emit_item(RefinementFeature::Rarity, {item.rarity});
    emit_item(
        RefinementFeature::PrefixCount, {item.prefix_count});
    emit_item(
        RefinementFeature::SuffixCount, {item.suffix_count});
    emit_item(
        RefinementFeature::HasCraftedModifier,
        {crafted ? 1u : 0u});
    emit_item(
        RefinementFeature::HasFracturedModifier,
        {fractured ? 1u : 0u});
    emit_item(
        RefinementFeature::HasVeiledModifier,
        {veiled ? 1u : 0u});
    emit_item(
        RefinementFeature::Multimod, {multimod ? 1u : 0u});
    emit_item(
        RefinementFeature::PrefixLock,
        {prefix_lock ? 1u : 0u});
    emit_item(
        RefinementFeature::SuffixLock,
        {suffix_lock ? 1u : 0u});
    emit_item(
        RefinementFeature::CannotRollAttack,
        {no_attack ? 1u : 0u});
    emit_item(
        RefinementFeature::CannotRollCaster,
        {no_caster ? 1u : 0u});
    emit_item(
        RefinementFeature::Influence,
        {item.generic_influence_bits});
    emit_item(
        RefinementFeature::SearingExarchTier,
        {item.searing_exarch_tier});
    emit_item(
        RefinementFeature::EaterOfWorldsTier,
        {item.eater_of_worlds_tier});
    emit_item(
        RefinementFeature::EldritchPresence,
        {
            item.searing_exarch_tier != 0 ? 1u : 0u,
            item.eater_of_worlds_tier != 0 ? 1u : 0u,
        });
    const std::uint64_t dominance =
        item.searing_exarch_tier > item.eater_of_worlds_tier
            ? 1u
            : item.eater_of_worlds_tier >
                      item.searing_exarch_tier
                  ? 2u
                  : 0u;
    emit_item(
        RefinementFeature::EldritchDominance, {dominance});
    emit_item(
        RefinementFeature::Corrupted,
        {(item.item_flags & PC_ITEM_CORRUPTED) != 0 ? 1u : 0u});
    emit_item(
        RefinementFeature::Mirrored,
        {(item.item_flags & PC_ITEM_MIRRORED) != 0 ? 1u : 0u});
    emit_item(
        RefinementFeature::Split,
        {(item.item_flags & PC_ITEM_SPLIT) != 0 ? 1u : 0u});
    emit_item(
        RefinementFeature::Synthesised,
        {(item.item_flags & PC_ITEM_SYNTHESISED) != 0 ? 1u : 0u});

    const std::vector<std::uint32_t> relevant_tags =
        relevant_observation_tags(requirement);
    std::uint8_t item_traits = 0;
    if (item.searing_exarch_tier != item.eater_of_worlds_tier) {
        item_traits |= kRefinementItemHasEldritchDominance;
    }
    if (prefix_lock != suffix_lock) {
        item_traits |= kRefinementItemExactlyOneSideLocked;
    }
    std::uint32_t subject = 0;
    const auto visit =
        [&](const pc_mod_slot* slots, const std::uint8_t count,
            const std::int8_t side) {
            for (std::uint8_t index = 0; index < count; ++index) {
                const pc_mod_slot& slot = slots[index];
                if (slot.mod_id >= session.mod_count) continue;
                const std::uint32_t mod = slot.mod_id;
                const bool slot_crafted =
                    (slot.flags & PC_MOD_SLOT_CRAFTED) != 0;
                const bool slot_fractured =
                    (slot.flags & PC_MOD_SLOT_FRACTURED) != 0;
                const bool slot_veiled =
                    modifier_is_veiled_template(session, mod);
                std::uint16_t traits =
                    side == PC_SIDE_PREFIX
                        ? kRefinementAffixPrefix
                        : kRefinementAffixSuffix;
                if (slot_crafted) {
                    traits |= kRefinementAffixCrafted;
                }
                if (slot_fractured) {
                    traits |= kRefinementAffixFractured;
                }
                if (slot_veiled) {
                    traits |= kRefinementAffixVeiled;
                }
                if ((side == PC_SIDE_PREFIX && prefix_lock) ||
                    (side == PC_SIDE_SUFFIX && suffix_lock)) {
                    traits |= kRefinementAffixOnLockedSide;
                }
                if (item.searing_exarch_tier !=
                    item.eater_of_worlds_tier) {
                    const std::int8_t dominant_side =
                        item.searing_exarch_tier >
                                item.eater_of_worlds_tier
                            ? PC_SIDE_PREFIX
                            : PC_SIDE_SUFFIX;
                    traits |=
                        side == dominant_side
                            ? kRefinementAffixOnEldritchDominantSide
                            : kRefinementAffixOnEldritchNonDominantSide;
                }
                const std::vector<std::uint32_t> tags =
                    mod_relevant_tags(
                        session, mod, relevant_tags);
                const auto emit_affix =
                    [&](const RefinementFeature feature,
                        StableKey value) {
                        exact.push_back({
                            feature, subject, std::move(value),
                            traits, item_traits, tags});
                    };
                emit_affix(
                    RefinementFeature::ModifierSide,
                    {static_cast<std::uint64_t>(side)});
                emit_affix(
                    RefinementFeature::ModifierExclusionSignature,
                    modifier_exclusion_effect_signature(
                        session, mod));
                const StableKey default_goal{
                    0u,
                    static_cast<std::uint8_t>(
                        GoalSlotStatus::Absent)};
                emit_affix(
                    RefinementFeature::GoalStatusTierClass,
                    mod <
                            context
                                .goal_status_tier_class_by_mod
                                .size() &&
                            !context
                                 .goal_status_tier_class_by_mod[mod]
                                 .empty()
                        ? context
                              .goal_status_tier_class_by_mod[mod]
                        : default_goal);
                emit_affix(
                    RefinementFeature::ModifierCrafted,
                    {slot_crafted ? 1u : 0u});
                emit_affix(
                    RefinementFeature::ModifierFractured,
                    {slot_fractured ? 1u : 0u});
                emit_affix(
                    RefinementFeature::ModifierVeiled,
                    {slot_veiled ? 1u : 0u});
                emit_affix(
                    RefinementFeature::ModifierClassificationTags,
                    StableKey(
                        tags.begin(), tags.end()));
                emit_affix(
                    RefinementFeature::ModifierRequiredLevel,
                    {session.required_level.at(mod)});
                emit_affix(
                    RefinementFeature::CountObservationMembership,
                    mod <
                            context
                                .count_observation_membership_by_mod
                                .size()
                        ? context
                              .count_observation_membership_by_mod[mod]
                        : StableKey{});
                emit_affix(
                    RefinementFeature::ModifierMetamodRole,
                    {static_cast<std::uint32_t>(
                        mod < session.metamod_type.size()
                            ? session.metamod_type[mod]
                            : -1)});
                ++subject;
            }
        };
    visit(
        item.prefixes, item.prefix_count, PC_SIDE_PREFIX);
    visit(
        item.suffixes, item.suffix_count, PC_SIDE_SUFFIX);
    return observe_features(exact, requirement);
}

AbstractFeatureExtraction extract_strict_abstract_features(
        const SessionImpl& session,
        const AbstractLayout& layout,
        const AbstractState& state,
        const ObservationRequirement& input_requirement) {
    AbstractFeatureExtraction result;
    const ObservationRequirement requirement =
        canonical_observation_requirement(input_requirement);
    const auto item_requested =
        [&](const RefinementFeature feature) {
            return (requirement.item_features &
                    refinement_feature(feature)) != 0;
        };
    const auto emit_item =
        [&](const RefinementFeature feature, StableKey value) {
            if (!item_requested(feature)) return;
            result.features.push_back(
                {feature, 0, std::move(value), 0, 0, {}});
        };
    const auto flag = [&](const std::uint32_t bit) {
        return StableKey{(state.flags & bit) != 0 ? 1u : 0u};
    };

    emit_item(RefinementFeature::Rarity, {state.rarity});
    emit_item(RefinementFeature::PrefixCount, {state.prefix_count});
    emit_item(RefinementFeature::SuffixCount, {state.suffix_count});
    emit_item(
        RefinementFeature::HasCraftedModifier,
        flag(kFlagCraftedMod));
    emit_item(
        RefinementFeature::HasFracturedModifier,
        flag(kFlagFractured));
    emit_item(
        RefinementFeature::HasVeiledModifier,
        flag(kFlagVeiledMod));
    emit_item(RefinementFeature::Multimod, flag(kFlagMultimod));
    emit_item(
        RefinementFeature::PrefixLock,
        flag(kFlagPrefixesLocked));
    emit_item(
        RefinementFeature::SuffixLock,
        flag(kFlagSuffixesLocked));
    emit_item(
        RefinementFeature::CannotRollAttack,
        flag(kFlagNoAttack));
    emit_item(
        RefinementFeature::CannotRollCaster,
        flag(kFlagNoCaster));
    emit_item(
        RefinementFeature::Influence, {state.influence_bits});
    emit_item(
        RefinementFeature::SearingExarchTier,
        {state.searing_exarch_tier});
    emit_item(
        RefinementFeature::EaterOfWorldsTier,
        {state.eater_of_worlds_tier});
    emit_item(
        RefinementFeature::EldritchPresence,
        {
            state.searing_exarch_tier != 0 ? 1u : 0u,
            state.eater_of_worlds_tier != 0 ? 1u : 0u,
        });
    const std::uint64_t dominance =
        state.searing_exarch_tier > state.eater_of_worlds_tier
            ? 1u
            : state.eater_of_worlds_tier >
                      state.searing_exarch_tier
                  ? 2u
                  : 0u;
    emit_item(
        RefinementFeature::EldritchDominance, {dominance});
    emit_item(RefinementFeature::Corrupted, flag(kFlagCorrupted));
    emit_item(RefinementFeature::Mirrored, flag(kFlagMirrored));
    emit_item(RefinementFeature::Split, flag(kFlagSplit));
    emit_item(
        RefinementFeature::Synthesised, flag(kFlagSynthesised));

    RefinementFeatureMask requested_affix_features = 0;
    for (const RefinementAffixObservation& observation :
         requirement.affix_observations) {
        requested_affix_features |= observation.features;
    }
    if (requested_affix_features == 0) {
        result.features =
            canonical_feature_signature(std::move(result.features));
        return result;
    }

    const std::vector<std::uint32_t> relevant_tags =
        relevant_observation_tags(requirement);
    const std::uint8_t item_traits =
        refinement_item_traits(state);
    std::uint32_t next_subject = 0;
    std::uint32_t represented_prefixes = 0;
    std::uint32_t represented_suffixes = 0;

    const auto emit_carriers =
        [&](const std::vector<std::uint32_t>& members,
            const std::int8_t side_hint,
            const std::uint32_t goal_slot,
            const GoalSlotStatus goal_status,
            const std::optional<StableKey>& count_membership,
            const bool crafted,
            const bool fractured,
            const std::uint32_t count) {
            if (count == 0) return;
            if (members.empty()) {
                result.unavailable_features |=
                    requested_affix_features;
                return;
            }
            const auto side_value =
                uniform_member_value<std::int8_t>(
                    members, [&](const std::uint32_t mod) {
                        return session.gen_type.at(mod);
                    });
            const std::int8_t side =
                side_hint == PC_SIDE_PREFIX ||
                        side_hint == PC_SIDE_SUFFIX
                    ? side_hint
                    : side_value.value_or(-1);
            if (!side_value.has_value() || side < 0 ||
                *side_value != side) {
                result.unavailable_features |=
                    refinement_feature(
                        RefinementFeature::ModifierSide);
                return;
            }
            if (side == PC_SIDE_PREFIX) {
                represented_prefixes += count;
            } else {
                represented_suffixes += count;
            }
            const auto veiled_value =
                uniform_member_value<bool>(
                    members, [&](const std::uint32_t mod) {
                        return modifier_is_veiled_template(session, mod);
                    });
            const auto tags_value =
                uniform_member_value<std::vector<std::uint32_t>>(
                    members, [&](const std::uint32_t mod) {
                        return mod_relevant_tags(
                            session, mod, relevant_tags);
                    });
            if (!veiled_value.has_value() ||
                !tags_value.has_value()) {
                result.unavailable_features |=
                    requested_affix_features;
                return;
            }
            const std::uint16_t traits = affix_traits(
                state, side, crafted, fractured, *veiled_value);

            RefinementFeatureMask observed_here = 0;
            for (const RefinementAffixObservation& observation :
                 requirement.affix_observations) {
                if (refinement_selector_matches(
                        observation.selector, traits, item_traits,
                        *tags_value)) {
                    observed_here |= observation.features;
                }
            }
            if (observed_here == 0) return;

            std::optional<StableKey> exclusion;
            if ((observed_here &
                 refinement_feature(
                     RefinementFeature::
                         ModifierExclusionSignature)) != 0) {
                exclusion =
                    uniform_member_value<StableKey>(
                        members, [&](const std::uint32_t mod) {
                            return modifier_exclusion_effect_signature(
                                session, mod);
                        });
                if (!exclusion.has_value()) {
                    result.unavailable_features |=
                        refinement_feature(
                            RefinementFeature::
                                ModifierExclusionSignature);
                }
            }
            std::optional<std::uint32_t> required_level;
            if ((observed_here &
                 refinement_feature(
                     RefinementFeature::ModifierRequiredLevel)) != 0) {
                required_level =
                    uniform_member_value<std::uint32_t>(
                        members, [&](const std::uint32_t mod) {
                            return session.required_level.at(mod);
                        });
                if (!required_level.has_value()) {
                    result.unavailable_features |=
                        refinement_feature(
                            RefinementFeature::
                                ModifierRequiredLevel);
                }
            }
            std::optional<std::int32_t> metamod_role;
            if ((observed_here &
                 refinement_feature(
                     RefinementFeature::
                         ModifierMetamodRole)) != 0) {
                metamod_role =
                    uniform_member_value<std::int32_t>(
                        members, [&](const std::uint32_t mod) {
                            return mod <
                                           session.metamod_type.size()
                                       ? session.metamod_type[mod]
                                       : -1;
                        });
                if (!metamod_role.has_value()) {
                    result.unavailable_features |=
                        refinement_feature(
                            RefinementFeature::
                                ModifierMetamodRole);
                }
            }
            if ((observed_here &
                 refinement_feature(
                     RefinementFeature::
                         CountObservationMembership)) != 0 &&
                !count_membership.has_value()) {
                result.unavailable_features |=
                    refinement_feature(
                        RefinementFeature::
                            CountObservationMembership);
            }

            for (std::uint32_t occurrence = 0;
                 occurrence < count; ++occurrence) {
                const std::uint32_t subject = next_subject++;
                const auto emit_affix =
                    [&](const RefinementFeature feature,
                        StableKey value) {
                        if ((observed_here &
                             refinement_feature(feature)) == 0) {
                            return;
                        }
                        result.features.push_back({
                            feature,
                            subject,
                            std::move(value),
                            traits,
                            item_traits,
                            *tags_value});
                    };
                emit_affix(
                    RefinementFeature::ModifierSide,
                    {static_cast<std::uint64_t>(side)});
                if (exclusion.has_value()) {
                    emit_affix(
                        RefinementFeature::
                            ModifierExclusionSignature,
                        *exclusion);
                }
                emit_affix(
                    RefinementFeature::GoalStatusTierClass,
                    {
                        goal_slot == kNoId
                            ? 0u
                            : static_cast<std::uint64_t>(
                                  goal_slot + 1),
                        static_cast<std::uint8_t>(goal_status),
                    });
                emit_affix(
                    RefinementFeature::ModifierCrafted,
                    {crafted ? 1u : 0u});
                emit_affix(
                    RefinementFeature::ModifierFractured,
                    {fractured ? 1u : 0u});
                emit_affix(
                    RefinementFeature::ModifierVeiled,
                    {*veiled_value ? 1u : 0u});
                StableKey observed_tags;
                for (const std::uint32_t tag :
                     requirement.modifier_tag_ids) {
                    if (std::binary_search(
                            tags_value->begin(), tags_value->end(),
                            tag)) {
                        observed_tags.push_back(tag);
                    }
                }
                emit_affix(
                    RefinementFeature::ModifierClassificationTags,
                    std::move(observed_tags));
                if (required_level.has_value()) {
                    emit_affix(
                        RefinementFeature::ModifierRequiredLevel,
                        {*required_level});
                }
                if (count_membership.has_value()) {
                    emit_affix(
                        RefinementFeature::
                            CountObservationMembership,
                        *count_membership);
                }
                if (metamod_role.has_value()) {
                    emit_affix(
                        RefinementFeature::ModifierMetamodRole,
                        {static_cast<std::uint32_t>(
                            *metamod_role)});
                }
            }
        };

    for (std::uint32_t slot_index = 0;
         slot_index < layout.slots.size(); ++slot_index) {
        const GoalSlotStatus status =
            static_cast<GoalSlotStatus>(
                state.slot_status[slot_index]);
        if (status == GoalSlotStatus::Absent) continue;
        const ResolvedGoalSlot& slot = layout.slots[slot_index];
        std::vector<std::uint32_t> members;
        std::int8_t side = -1;
        std::optional<StableKey> count_membership;
        const std::uint32_t token =
            state.goal_member_class_tokens[slot_index];
        if (token != 0 && token <= slot.member_classes.size()) {
            const GoalMemberClass& member_class =
                slot.member_classes[token - 1];
            members = mask_members(
                session, member_class.member_mask);
            side = member_class.gen_type;
            count_membership =
                uniform_member_value<StableKey>(
                    members, [&](const std::uint32_t mod) {
                        return mod_count_observation_bits(
                            layout, mod);
                    });
        } else {
            for (const std::uint32_t mod :
                 mask_members(session, slot.member_mask)) {
                const bool satisfying = pc_bitset_test(
                    slot.satisfying_mask.data(), mod);
                if ((status == GoalSlotStatus::Satisfied) ==
                    satisfying) {
                    members.push_back(mod);
                }
            }
            count_membership =
                uniform_member_value<StableKey>(
                    members, [&](const std::uint32_t mod) {
                        return mod_count_observation_bits(
                            layout, mod);
                    });
        }
        emit_carriers(
            members, side, slot_index, status, count_membership,
            (state.crafted_goal_mask & (1u << slot_index)) != 0,
            (state.fractured_goal_mask & (1u << slot_index)) != 0,
            1);
    }

    const std::size_t junk_count = std::min({
        layout.junk_classes.size(),
        state.junk_counts.size(),
        state.fractured_junk_counts.size(),
        state.crafted_junk_counts.size(),
        state.fractured_crafted_junk_counts.size()});
    for (std::size_t index = 0; index < junk_count; ++index) {
        const JunkClass& junk = layout.junk_classes[index];
        const std::uint32_t total = state.junk_counts[index];
        const std::uint32_t fractured =
            state.fractured_junk_counts[index];
        const std::uint32_t crafted =
            state.crafted_junk_counts[index];
        const std::uint32_t both =
            state.fractured_crafted_junk_counts[index];
        if (both > fractured || both > crafted ||
            fractured + crafted - both > total) {
            result.unavailable_features |=
                requested_affix_features;
            continue;
        }
        const std::vector<std::uint32_t> members =
            mask_members(session, junk.member_mask);
        const std::optional<StableKey> count_membership =
            uniform_member_value<StableKey>(
                members, [&](const std::uint32_t mod) {
                    return mod_count_observation_bits(layout, mod);
                });
        emit_carriers(
            members, junk.gen_type, kNoId, GoalSlotStatus::Absent,
            count_membership,
            false, false, total - fractured - crafted + both);
        emit_carriers(
            members, junk.gen_type, kNoId, GoalSlotStatus::Absent,
            count_membership,
            true, false, crafted - both);
        emit_carriers(
            members, junk.gen_type, kNoId, GoalSlotStatus::Absent,
            count_membership,
            false, true, fractured - both);
        emit_carriers(
            members, junk.gen_type, kNoId, GoalSlotStatus::Absent,
            count_membership,
            true, true, both);
    }
    if (junk_count != layout.junk_classes.size()) {
        result.unavailable_features |= requested_affix_features;
    }
    if (represented_prefixes != state.prefix_count ||
        represented_suffixes != state.suffix_count) {
        /*
         * A compact layout can omit an occupied exact carrier (for example an
         * externally supplied metamod that no candidate action creates).
         * Refusing the signature is the only sound choice when an affix
         * observer is active.
         */
        result.unavailable_features |= requested_affix_features;
    }
    result.features =
        canonical_feature_signature(std::move(result.features));
    return result;
}

std::optional<StableKey> canonical_operation_state_signature(
        const SessionImpl& session,
        const AbstractLayout& layout,
        const AbstractState& state,
        const SelectedAction& action) {
    if (!action.contract.complete() ||
        action.semantic_key.empty() ||
        !selected_runtime_contracts_complete(action)) {
        return std::nullopt;
    }
    const ObservationRequirement requirement = merge_requirements(
        observation_requirement_from_selected_action(action),
        action.routing_observes);
    const AbstractFeatureExtraction extraction =
        extract_strict_abstract_features(
            session, layout, state, requirement);
    if (!extraction.complete()) return std::nullopt;
    const FeatureSignature observed =
        observe_features(extraction.features, requirement);
    StableKey signature{
        0x706372666e7631ull, /* "pcrfnv1" */
        action.action_id};
    append_tokens(signature, action.semantic_key);
    const StableKey contract_signature =
        selected_runtime_contract_signature(action);
    append_tokens(signature, contract_signature);
    append_requirement(signature, requirement);
    signature.push_back(observed.size());
    for (const FeatureAtom& atom : observed) {
        append_atom(signature, atom);
    }
    return signature;
}

RefinementResult refine_policy_exact(
        PolicyRefinementOracle& oracle,
        RefinementRequest request) {
    RefinementResult result;
    std::sort(request.coarse_roots.begin(), request.coarse_roots.end());
    request.coarse_roots.erase(
        std::unique(
            request.coarse_roots.begin(), request.coarse_roots.end()),
        request.coarse_roots.end());
    if (request.coarse_roots.empty()) {
        fail(
            result, RefinementStatus::EmptyRequest,
            "policy refinement has no coarse roots");
        return result;
    }
    Graph graph;
    if (!discover_policy_graph(oracle, graph, request, result)) {
        return result;
    }
    const std::uint64_t adapter_memory =
        oracle.estimated_owned_bytes();
    if (!check_refinement_memory(
            result, graph, request.limits, adapter_memory,
            estimate_graph_canonicalization_scratch(graph))) {
        return result;
    }
    canonicalize_graph(graph);
    if (!check_refinement_memory(
            result, graph, request.limits, adapter_memory)) {
        return result;
    }
    if (!refine_selected_action_routing(
            graph, request.limits, result, adapter_memory)) {
        return result;
    }
    if (result.telemetry.selected_action_routing_rounds >=
        request.limits.max_refinement_rounds) {
        refinement_round_cap(
            result,
            "selected-action routing and policy observation reached "
            "max_refinement_rounds");
        return result;
    }
    RefinementLimits observation_limits = request.limits;
    observation_limits.max_refinement_rounds -=
        result.telemetry.selected_action_routing_rounds;
    if (!propagate_observations(
            graph, observation_limits, result, adapter_memory)) {
        return result;
    }
    if (!check_refinement_memory(
            result, graph, request.limits, adapter_memory)) {
        return result;
    }

    std::vector<ClosedPartitionNode> closed_nodes;
    closed_nodes.reserve(graph.nodes.size());
    if (!check_refinement_memory(
            result, graph, request.limits, adapter_memory,
            estimate_closed_nodes_memory(closed_nodes))) {
        return result;
    }
    for (const Node& node : graph.nodes) {
        ClosedPartitionNode closed;
        /*
         * Both parts are length-delimited semantic identities. Numeric
         * coarse discovery ids remain lookup-only.
         */
        append_tokens(
            closed.stable_key,
            node.state.coarse_state_key);
        append_tokens(
            closed.stable_key,
            node.state.stable_key);
        closed.observation_key = initial_behavior_key(node);
        if (node.selection.has_value()) {
            closed.immediate_key = {
                std::bit_cast<std::uint64_t>(node.action_cost)};
        }
        closed.terminal = node.state.terminal;
        closed.arcs.reserve(node.edges.size());
        for (const NodeEdge& edge : node.edges) {
            closed.arcs.push_back({
                {},
                std::optional<std::uint32_t>{edge.successor},
                edge.probability.value()});
        }
        closed_nodes.push_back(std::move(closed));
        if (!check_refinement_memory(
                result, graph, request.limits, adapter_memory,
                estimate_closed_nodes_memory(closed_nodes))) {
            return result;
        }
    }
    ClosedPartitionLimits partition_limits;
    partition_limits.max_classes =
        request.limits.max_refinement_classes;
    const std::uint64_t pre_partition_rounds =
        static_cast<std::uint64_t>(
            result.telemetry.selected_action_routing_rounds) +
        result.telemetry.observation_propagation_rounds;
    if (pre_partition_rounds >=
        request.limits.max_refinement_rounds) {
        refinement_round_cap(
            result,
            "policy observation and kernel refinement reached "
            "max_refinement_rounds");
        return result;
    }
    partition_limits.max_rounds =
        request.limits.max_refinement_rounds -
        static_cast<std::uint32_t>(pre_partition_rounds);
    partition_limits.retained_estimated_memory_bytes =
        saturated_add(
            saturated_add(
                estimate_graph_memory(graph), adapter_memory),
            estimate_refinement_result_memory(result));
    partition_limits.max_estimated_memory_bytes =
        request.limits.max_estimated_memory_bytes;
    partition_limits.probability_sum_tolerance =
        request.limits.probability_sum_tolerance;
    ClosedPartitionResult partitioned =
        refine_closed_probabilistic_partition(
            std::move(closed_nodes), partition_limits);
    result.telemetry.partition_refinement_rounds =
        partitioned.rounds;
    result.telemetry.initial_observation_classes =
        partitioned.initial_class_count;
    result.telemetry.final_refinement_classes =
        partitioned.final_class_count;
    result.telemetry.lumpability_checks =
        partitioned.lumpability_checks;
    result.telemetry.estimated_memory_bytes =
        partitioned.estimated_memory_bytes;
    result.telemetry.peak_estimated_memory_bytes = std::max(
        result.telemetry.peak_estimated_memory_bytes,
        partitioned.peak_estimated_memory_bytes);
    if (partitioned.status != ClosedPartitionStatus::Complete) {
        if (partitioned.status ==
            ClosedPartitionStatus::ResourceCap) {
            if (partitioned.resource_cap ==
                "max_estimated_memory_bytes") {
                cap(
                    result, "max_estimated_memory_bytes",
                    "policy refinement reached "
                    "max_estimated_memory_bytes");
            } else {
                cap(
                    result, "max_refinement_classes",
                    "policy refinement reached "
                    "max_refinement_classes");
            }
        } else if (
            partitioned.status ==
            ClosedPartitionStatus::RefinementRoundCap) {
            refinement_round_cap(
                result,
                "kernel refinement reached max_refinement_rounds");
        } else if (
            partitioned.status ==
            ClosedPartitionStatus::NonLumpable) {
            fail(
                result, RefinementStatus::NonLumpable,
                partitioned.failure_reason);
        } else {
            fail(
                result, RefinementStatus::InvalidKernel,
                partitioned.failure_reason);
        }
        return result;
    }
    std::vector<std::uint32_t> initial =
        std::move(partitioned.initial_class_by_node);
    std::vector<std::uint32_t> partition =
        std::move(partitioned.class_by_node);
    const std::uint32_t initial_count =
        partitioned.initial_class_count;
    const std::uint32_t class_count =
        partitioned.final_class_count;
    partitioned.classes = {};
    partitioned.initial_class_by_node = {};
    partitioned.class_by_node = {};
    result.telemetry.final_refinement_classes = class_count;
    result.telemetry.behavior_splits = class_count - initial_count;
    result.telemetry.merged_exact_states =
        static_cast<std::uint32_t>(graph.nodes.size()) - class_count;
    std::uint64_t retained_scratch =
        saturated_product(
            initial.capacity(), sizeof(std::uint32_t));
    add_bytes(
        retained_scratch,
        saturated_product(
            partition.capacity(), sizeof(std::uint32_t)));
    add_bytes(
        retained_scratch,
        estimate_closed_partition_result_memory(partitioned));
    if (!collect_counterexamples(
            graph, initial, partition, request.limits, result,
            adapter_memory, retained_scratch)) {
        return result;
    }
    if (!build_classes(
            graph, partition, class_count, request.limits, result,
            adapter_memory, retained_scratch)) {
        return result;
    }
    if (!check_refinement_memory(
            result, graph, request.limits, adapter_memory,
            retained_scratch)) {
        return result;
    }
    result.status = RefinementStatus::Complete;
    result.executable = true;
    result.lumpable = true;
    return result;
}

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
