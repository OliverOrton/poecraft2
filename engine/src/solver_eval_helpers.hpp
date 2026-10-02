#pragma once

#include "solver_eval_types.hpp"
#include "solver_dominance.hpp"
#include "solver_clusters.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <functional>
#include <iomanip>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "json.hpp"
#include "poecraft/bitset.h"
#include "solver_refinement.hpp"
#include "solver_segmented_vector.hpp"
#include "solver_sparse_policy.hpp"

namespace poecraft {
namespace solver {

namespace {

using json::Parser;
using json::Type;
using json::Value;

struct ReviewSectionSpec {
    std::string id;
    std::string label;
    std::string role;
    std::vector<std::uint32_t> nodes;
    std::vector<std::string> edges;
};

struct TargetEntry {
    GoalSlot slot;
    std::string origin;
};

struct EvalModel {
    std::unique_ptr<CalcContext> calc;
    std::vector<ResolvedStrategyOperation> operation_by_node;
    std::vector<std::uint32_t> action_by_node;
    std::vector<GoalSlot> targets;
};

enum class EvalAbsorptionKind {
    Terminal,
    ActionNotApplied,
    NoMatchingEdge,
};

struct EvalTransition {
    double probability = 0.0;
    std::uint32_t target = kNoId;
    std::uint32_t edge = kNoId;
    /* Exact skipped-route authority. A compiled-node id names the retained
     * single-root policy-route cache; an evaluator-encoded id names an
     * interned deterministic-router trace. The exact path is replayed once
     * flow is known. */
    std::uint32_t policy_route = kNoId;
    /* Exact state used by the skipped route. Operation-pair refinement may
     * merge input states after routing selected the same action, so the
     * target pair's representative is not authoritative for replay. */
    std::uint32_t policy_state = kNoId;

    EvalTransition() = default;
    EvalTransition(
        const std::uint32_t target_in,
        const double probability_in,
        const std::uint32_t edge_in,
        const std::uint32_t policy_route_in,
        const std::uint32_t policy_state_in)
        : probability(probability_in),
          target(target_in),
          edge(edge_in),
          policy_route(policy_route_in),
          policy_state(policy_state_in) {}
};

static_assert(
    sizeof(EvalTransition) == 24,
    "raw exact-evaluator transitions must remain compact");

struct EvalAbsorption {
    EvalAbsorptionKind kind = EvalAbsorptionKind::Terminal;
    std::uint32_t node = kNoId;
    std::uint32_t state = kNoId;
    double probability = 0.0;
    std::uint32_t edge = kNoId;
    std::uint32_t policy_route = kNoId;
};

struct EvalRow {
    std::vector<EvalTransition> transitions;
    /* Empty during discovery and pair refinement. Pass-through contraction
     * allocates a parallel vector only for a rewritten row; each entry is the
     * original pair entered before the transition was redirected. */
    std::vector<std::uint32_t> transition_via;
    std::vector<EvalAbsorption> absorptions;
    /* Broad immutable operation kernels retain one compact exact route-result
     * token per sorted OutcomeDistribution entry during discovery. The
     * distribution is owned by CalcContext's stable shared-kernel memo. */
    const OutcomeDistribution* replay_distribution = nullptr;
    std::uint32_t replay_checkpoint_state = kNoId;
    std::vector<std::uint32_t> replay_route_tokens;

    bool replayable() const {
        return replay_distribution != nullptr;
    }
};

struct EvalPair {
    std::uint32_t node = kNoId;
    std::uint32_t state = kNoId;
    /* Exact saved-item carrier for companion-state operations. kNoId means
     * no checkpoint exists. The checkpoint is bound to the current live item;
     * Restart clears it before changing item identity. */
    std::uint32_t checkpoint_state = kNoId;
    /* Interned sampled Unveil offer carried through its routing DAG. */
    std::uint32_t unveil_offer = kNoId;
    std::uint32_t row = kNoId;
    /* Whether this operation was legal and consumed its material. Operation
     * kind and action descriptor are exact functions of the compiled node and
     * are deliberately not duplicated in every raw pair. */
    bool consumes = false;
};

static_assert(
    sizeof(EvalPair) == 24,
    "raw exact-evaluator pair records must remain compact");

void add_gap(std::vector<std::string>& gaps, const std::string& gap) {
    if (std::find(gaps.begin(), gaps.end(), gap) == gaps.end()) {
        gaps.push_back(gap);
    }
}

std::string join_gaps(const std::vector<std::string>& gaps) {
    std::string message = "strategy evaluation unsupported:";
    for (const std::string& gap : gaps) {
        message += "\n- " + gap;
    }
    return message;
}

using refinement::ObservationRequirement;

std::uint64_t capped_add(
        const std::uint64_t left,
        const std::uint64_t right) {
    return right > std::numeric_limits<std::uint64_t>::max() - left
               ? std::numeric_limits<std::uint64_t>::max()
               : left + right;
}

std::uint64_t capped_product(
        const std::uint64_t left,
        const std::uint64_t right) {
    return left != 0 &&
                   right > std::numeric_limits<std::uint64_t>::max() / left
               ? std::numeric_limits<std::uint64_t>::max()
               : left * right;
}

std::uint64_t observation_requirement_payload_bytes(
        const ObservationRequirement& requirement) {
    std::uint64_t bytes = capped_product(
        requirement.modifier_tag_ids.capacity(), sizeof(std::uint32_t));
    bytes = capped_add(
        bytes,
        capped_product(
            requirement.affix_observations.capacity(),
            sizeof(RefinementAffixObservation)));
    for (const RefinementAffixObservation& observation :
         requirement.affix_observations) {
        bytes = capped_add(
            bytes,
            capped_product(
                observation.selector.required_tag_ids.capacity(),
                sizeof(std::uint32_t)));
    }
    return bytes;
}

std::uint64_t refinement_contract_payload_bytes(
        const ActionRefinementContract& contract) {
    std::uint64_t bytes = capped_product(
        contract.observed_modifier_tag_ids.capacity(),
        sizeof(std::uint32_t));
    bytes = capped_add(
        bytes,
        capped_product(
            contract.affix_observations.capacity(),
            sizeof(RefinementAffixObservation)));
    for (const RefinementAffixObservation& observation :
         contract.affix_observations) {
        bytes = capped_add(
            bytes,
            capped_product(
                observation.selector.required_tag_ids.capacity(),
                sizeof(std::uint32_t)));
    }
    bytes = capped_add(
        bytes,
        capped_product(
            contract.item_affix_dependencies.capacity(),
            sizeof(RefinementItemAffixDependency)));
    bytes = capped_add(
        bytes,
        capped_product(
            contract.affix_flows.capacity(),
            sizeof(RefinementAffixFlow)));
    for (const RefinementAffixFlow& flow : contract.affix_flows) {
        bytes = capped_add(
            bytes,
            capped_product(
                flow.source_selector.required_tag_ids.capacity(),
                sizeof(std::uint32_t)));
    }
    const auto append_selectors =
        [&](const std::vector<RefinementAffixSelector>& selectors) {
            bytes = capped_add(
                bytes,
                capped_product(
                    selectors.capacity(),
                    sizeof(RefinementAffixSelector)));
            for (const RefinementAffixSelector& selector : selectors) {
                bytes = capped_add(
                    bytes,
                    capped_product(
                        selector.required_tag_ids.capacity(),
                        sizeof(std::uint32_t)));
            }
        };
    append_selectors(contract.preserved_affixes);
    append_selectors(contract.destroyed_affixes);
    return bytes;
}

void observe_item_feature(
    ObservationRequirement& requirement,
    const RefinementFeature feature) {
    requirement.item_features |= refinement_feature(feature);
}

void observe_affix_features(
    ObservationRequirement& requirement,
    const RefinementFeatureMask features,
    RefinementAffixSelector selector = {}) {
    requirement.affix_observations.push_back(
        {features, std::move(selector)});
}

void observe_required_slot_flags(
    ObservationRequirement& requirement,
    const std::uint8_t required_flags) {
    RefinementFeatureMask features = 0;
    if ((required_flags & PC_MOD_SLOT_FRACTURED) != 0) {
        features |= refinement_feature(
            RefinementFeature::ModifierFractured);
    }
    if ((required_flags & PC_MOD_SLOT_CRAFTED) != 0) {
        features |= refinement_feature(
            RefinementFeature::ModifierCrafted);
    }
    observe_affix_features(requirement, features);
}

ObservationRequirement condition_observation_requirement(
    const CompiledCondition& condition) {
    ObservationRequirement requirement;
    switch (condition.kind) {
    case ConditionKind::Always:
    case ConditionKind::HasUnveilOption:
        break;
    case ConditionKind::ObservationSignature:
        if (condition.observation_program == nullptr) {
            throw std::logic_error(
                "observation-signature condition has no program");
        }
        requirement =
            condition.observation_program->requirement;
        break;
    case ConditionKind::HasModGroup:
    case ConditionKind::HasModFamily:
        observe_affix_features(
            requirement,
            refinement_feature(
                RefinementFeature::GoalStatusTierClass));
        observe_required_slot_flags(
            requirement, condition.required_flags);
        break;
    case ConditionKind::RarityIs:
        observe_item_feature(
            requirement, RefinementFeature::Rarity);
        break;
    case ConditionKind::OpenPrefixCount:
        observe_item_feature(
            requirement, RefinementFeature::Rarity);
        observe_item_feature(
            requirement, RefinementFeature::PrefixCount);
        break;
    case ConditionKind::OpenSuffixCount:
        observe_item_feature(
            requirement, RefinementFeature::Rarity);
        observe_item_feature(
            requirement, RefinementFeature::SuffixCount);
        break;
    case ConditionKind::PrefixCountRange:
        observe_item_feature(
            requirement, RefinementFeature::PrefixCount);
        break;
    case ConditionKind::SuffixCountRange:
        observe_item_feature(
            requirement, RefinementFeature::SuffixCount);
        break;
    case ConditionKind::ModCount:
    case ConditionKind::ModFamilyCount:
        observe_affix_features(
            requirement,
            refinement_feature(
                RefinementFeature::CountObservationMembership));
        observe_required_slot_flags(
            requirement, condition.required_flags);
        break;
    case ConditionKind::ItemFlag:
        switch (condition.item_flag) {
        case ItemFlagKind::Corrupted:
            observe_item_feature(
                requirement, RefinementFeature::Corrupted);
            break;
        case ItemFlagKind::Mirrored:
            observe_item_feature(
                requirement, RefinementFeature::Mirrored);
            break;
        case ItemFlagKind::Split:
            observe_item_feature(
                requirement, RefinementFeature::Split);
            break;
        case ItemFlagKind::Synthesised:
            observe_item_feature(
                requirement, RefinementFeature::Synthesised);
            break;
        case ItemFlagKind::Fractured:
            observe_item_feature(
                requirement,
                RefinementFeature::HasFracturedModifier);
            break;
        case ItemFlagKind::Crafted:
            observe_item_feature(
                requirement,
                RefinementFeature::HasCraftedModifier);
            break;
        case ItemFlagKind::Veiled:
            observe_item_feature(
                requirement,
                RefinementFeature::HasVeiledModifier);
            break;
        case ItemFlagKind::VeiledPrefix:
        case ItemFlagKind::VeiledSuffix: {
            observe_item_feature(
                requirement,
                RefinementFeature::HasVeiledModifier);
            RefinementAffixSelector veiled;
            veiled.required_affix_traits =
                kRefinementAffixVeiled;
            observe_affix_features(
                requirement,
                refinement_feature(
                    RefinementFeature::ModifierSide) |
                    refinement_feature(
                        RefinementFeature::ModifierVeiled),
                std::move(veiled));
            break;
        }
        case ItemFlagKind::Multimod:
            observe_item_feature(
                requirement, RefinementFeature::Multimod);
            break;
        case ItemFlagKind::NoAttack:
            observe_item_feature(
                requirement,
                RefinementFeature::CannotRollAttack);
            break;
        case ItemFlagKind::NoCaster:
            observe_item_feature(
                requirement,
                RefinementFeature::CannotRollCaster);
            break;
        case ItemFlagKind::PrefixesLocked:
            observe_item_feature(
                requirement, RefinementFeature::PrefixLock);
            break;
        case ItemFlagKind::SuffixesLocked:
            observe_item_feature(
                requirement, RefinementFeature::SuffixLock);
            break;
        case ItemFlagKind::Influenced:
            observe_item_feature(
                requirement, RefinementFeature::Influence);
            break;
        case ItemFlagKind::EldritchImplicit:
            observe_item_feature(
                requirement,
                RefinementFeature::EldritchPresence);
            break;
        }
        break;
    case ConditionKind::InfluenceBits:
        observe_item_feature(
            requirement, RefinementFeature::Influence);
        break;
    case ConditionKind::EldritchTier:
        observe_item_feature(
            requirement,
            condition.eldritch_side == 0
                ? RefinementFeature::SearingExarchTier
                : RefinementFeature::EaterOfWorldsTier);
        break;
    case ConditionKind::All:
    case ConditionKind::Any:
    case ConditionKind::Not:
    case ConditionKind::AtLeast:
        break;
    }
    for (const CompiledCondition& child : condition.children) {
        requirement = refinement::merge_observation_requirements(
            std::move(requirement),
            condition_observation_requirement(child));
    }
    return refinement::canonical_observation_requirement(
        std::move(requirement));
}

template <typename MemoryCheck>
std::vector<ObservationRequirement>
derive_node_observation_requirements(
    const StrategyImpl& strategy,
    const ActionRegistry& registry,
    const std::vector<ResolvedStrategyOperation>& operation_by_node,
    const std::uint32_t max_rounds,
    StrategyEvalResult::ObservationPropagationTelemetry* telemetry,
    MemoryCheck&& check_memory) {
    const std::size_t node_count = strategy.nodes.size();
    std::vector<ObservationRequirement> direct(node_count);
    for (std::uint32_t source = 0; source < node_count; ++source) {
        for (const StrategyEdge& edge : strategy.nodes[source].edges) {
            if (!edge.is_default) {
                direct[source] =
                    refinement::merge_observation_requirements(
                        std::move(direct[source]),
                        condition_observation_requirement(
                            edge.condition));
            }
        }
    }
    std::uint64_t direct_bytes = capped_product(
        direct.capacity(), sizeof(ObservationRequirement));
    for (const ObservationRequirement& requirement : direct) {
        direct_bytes = capped_add(
            direct_bytes,
            observation_requirement_payload_bytes(requirement));
    }
    if (telemetry != nullptr) {
        telemetry->nodes = static_cast<std::uint32_t>(node_count);
        const auto percentile = [&](const std::uint32_t numerator) {
            if (direct.empty()) return std::uint64_t{0};
            const std::size_t rank = std::max<std::size_t>(
                1, (direct.size() * numerator + 99) / 100);
            std::uint64_t selected = 0;
            bool has_selected = false;
            std::size_t cumulative = 0;
            while (cumulative < rank) {
                std::uint64_t next =
                    std::numeric_limits<std::uint64_t>::max();
                for (const ObservationRequirement& requirement : direct) {
                    const std::uint64_t payload =
                        observation_requirement_payload_bytes(requirement);
                    if ((!has_selected || payload > selected) &&
                        payload < next) {
                        next = payload;
                    }
                }
                if (next == std::numeric_limits<std::uint64_t>::max()) {
                    return selected;
                }
                selected = next;
                has_selected = true;
                cumulative = 0;
                for (const ObservationRequirement& requirement : direct) {
                    if (observation_requirement_payload_bytes(requirement) <=
                        selected) {
                        ++cumulative;
                    }
                }
            }
            return selected;
        };
        telemetry->direct_payload_p50_bytes = percentile(50);
        telemetry->direct_payload_p95_bytes = percentile(95);
        for (const ObservationRequirement& requirement : direct) {
            telemetry->direct_payload_max_bytes = std::max(
                telemetry->direct_payload_max_bytes,
                observation_requirement_payload_bytes(requirement));
        }
    }
    check_memory(
        direct_bytes, "observation_direct_requirements",
        node_count, sizeof(ObservationRequirement));

    std::vector<refinement::PolicyObservationNode> nodes;
    nodes.reserve(node_count);
    for (std::uint32_t node = 0; node < node_count; ++node) {
        refinement::PolicyObservationNode observation;
        observation.state_id = node;
        observation.direct_observes = std::move(direct[node]);
        if (strategy.nodes[node].kind ==
            StrategyNodeKind::Operation) {
            const ResolvedStrategyOperation& operation =
                operation_by_node.at(node);
            if (operation.kind ==
                ResolvedStrategyOperationKind::Bestiary) {
                /* Companion-state flow is represented explicitly in EvalPair.
                 * Treat the live-item channel conservatively as pass-through;
                 * the closed pair partition proves equivalence across the
                 * saved-state channel after discovery. */
                for (const StrategyEdge& edge :
                     strategy.nodes[node].edges) {
                    observation.successors.push_back(edge.target);
                }
                nodes.push_back(std::move(observation));
                continue;
            }
            const std::uint32_t action_index =
                operation.descriptor_index;
            if (action_index == kNoId) {
                throw std::logic_error(
                    "operation observation fixed point has no action");
            }
            const ActionRefinementContract& contract =
                registry.actions.at(action_index).refinement;
            refinement::SelectedAction selected;
            selected.action_id = action_index;
            selected.semantic_key = {
                static_cast<std::uint64_t>(node) + 1};
            selected.contract = contract;
            observation.selected_action = std::move(selected);
        }
        for (const StrategyEdge& edge :
             strategy.nodes[node].edges) {
            observation.successors.push_back(edge.target);
        }
        nodes.push_back(std::move(observation));
    }
    std::uint64_t nodes_bytes = capped_product(
        nodes.capacity(), sizeof(refinement::PolicyObservationNode));
    ObservationRequirement union_requirement;
    for (const refinement::PolicyObservationNode& node : nodes) {
        nodes_bytes = capped_add(
            nodes_bytes,
            observation_requirement_payload_bytes(node.direct_observes));
        nodes_bytes = capped_add(
            nodes_bytes,
            capped_product(
                node.successors.capacity(), sizeof(std::uint32_t)));
        union_requirement = refinement::merge_observation_requirements(
            std::move(union_requirement), node.direct_observes);
        if (node.selected_action.has_value()) {
            nodes_bytes = capped_add(
                nodes_bytes,
                capped_product(
                    node.selected_action->semantic_key.capacity(),
                    sizeof(std::uint64_t)));
            nodes_bytes = capped_add(
                nodes_bytes,
                refinement_contract_payload_bytes(
                    node.selected_action->contract));
            union_requirement =
                refinement::merge_observation_requirements(
                    std::move(union_requirement),
                    refinement::observation_requirement_from_selected_action(
                        *node.selected_action));
            union_requirement =
                refinement::merge_observation_requirements(
                    std::move(union_requirement),
                    node.selected_action->routing_observes);
        }
    }
    std::vector<ObservationRequirement>().swap(direct);
    /* Selected-action contracts remain owned once by `nodes_bytes`. The
     * propagated vector stores ObservationRequirement payloads only; charging
     * the sum of every contract vocabulary inside every node requirement was
     * an N-by-vocabulary phantom allocation. */
    /* A canonical requirement's logical content is bounded by the union,
     * but repeated merge/canonicalize cycles may retain geometric-growth
     * slack in both outer and selector vectors. Four union payloads plus the
     * larger dense wrapper conservatively cover the supported vector growth;
     * the actual retained peak is checked again after the fixed point. */
    const std::uint64_t one_requirement_bytes = capped_add(
        std::max(
            sizeof(ObservationRequirement),
            sizeof(refinement::PolicyObservationAssignment)),
        capped_product(
            observation_requirement_payload_bytes(union_requirement), 4));
    /* The fixed point owns the graph, ordered state index and grouping
     * vectors. Group construction also owns one signature-map entry per node
     * in the conservative case. During propagation/publication it owns two
     * requirement vectors plus bounded merge scratch, never three dense
     * vectors. Every propagated requirement is a subset of the deterministic
     * union. */
    const std::uint64_t index_bytes = capped_product(
        node_count,
        sizeof(std::pair<const std::uint32_t, std::uint32_t>) +
            3 * sizeof(void*));
    const std::uint64_t group_bytes = capped_add(
        capped_product(
            capped_product(node_count, 3),
            sizeof(std::vector<std::uint32_t>)),
        capped_product(
            capped_product(node_count, 3),
            sizeof(std::uint32_t)));
    std::uint64_t largest_group_signature_tokens =
        6 + union_requirement.modifier_tag_ids.size();
    for (const RefinementAffixObservation& observation :
         union_requirement.affix_observations) {
        largest_group_signature_tokens = capped_add(
            largest_group_signature_tokens,
            6 + observation.selector.required_tag_ids.size());
    }
    /* StableKey grows geometrically from the three-token prefix. Twice the
     * final token count safely covers its retained capacity. */
    largest_group_signature_tokens = capped_product(
        largest_group_signature_tokens, 2);
    const std::uint64_t signature_map_bytes = capped_product(
        node_count,
        capped_add(
            sizeof(std::pair<const refinement::StableKey, std::uint32_t>) +
                3 * sizeof(void*),
            capped_product(
                largest_group_signature_tokens,
                sizeof(std::uint64_t))));
    std::uint64_t common_graph_bytes = nodes_bytes;
    common_graph_bytes = capped_add(common_graph_bytes, index_bytes);
    common_graph_bytes = capped_add(common_graph_bytes, group_bytes);
    const std::uint64_t group_build_peak = capped_add(
        common_graph_bytes, signature_map_bytes);
    std::uint64_t fixed_point_peak = common_graph_bytes;
    fixed_point_peak = capped_add(
        fixed_point_peak,
        capped_product(
            node_count,
            capped_product(one_requirement_bytes, 2)));
    fixed_point_peak = capped_add(
        fixed_point_peak,
        capped_product(one_requirement_bytes, 3));
    const std::uint64_t propagation_peak = std::max(
        group_build_peak, fixed_point_peak);
    check_memory(
        propagation_peak, "observation_fixed_point_dense_nodes",
        node_count, one_requirement_bytes);
    refinement::PolicyObservationFixedPoint fixed =
        refinement::propagate_policy_observations(
            std::move(nodes), max_rounds);
    if (!fixed.complete) {
        if (fixed.round_cap) {
            throw std::length_error(
                "strategy evaluation observation propagation exceeded "
                "max_sweeps (" + std::to_string(max_rounds) + ")");
        }
        throw std::logic_error(
            fixed.failure_reason.empty()
                ? "strategy observation fixed point did not converge"
                : fixed.failure_reason);
    }
    const std::uint64_t actual_scratch_unit = capped_add(
        sizeof(ObservationRequirement),
        fixed.required_payload_max_bytes);
    const std::uint64_t actual_propagation_peak = capped_add(
        common_graph_bytes,
        capped_add(
            fixed.estimated_peak_owned_bytes,
            capped_product(actual_scratch_unit, 3)));
    check_memory(
        actual_propagation_peak,
        "observation_fixed_point_actual_owned",
        node_count, one_requirement_bytes);
    if (telemetry != nullptr) {
        telemetry->groups = fixed.propagation_groups;
        telemetry->rounds = fixed.rounds;
        telemetry->unique_canonical_requirements =
            fixed.unique_canonical_requirements;
        telemetry->propagated_payload_p50_bytes =
            fixed.required_payload_p50_bytes;
        telemetry->propagated_payload_p95_bytes =
            fixed.required_payload_p95_bytes;
        telemetry->propagated_payload_max_bytes =
            fixed.required_payload_max_bytes;
        telemetry->projected_peak_bytes = propagation_peak;
        telemetry->actual_peak_bytes = actual_propagation_peak;
    }
    std::vector<ObservationRequirement> required(node_count);
    std::uint64_t conversion_bytes = capped_product(
        required.capacity(), sizeof(ObservationRequirement));
    conversion_bytes = capped_add(
        conversion_bytes,
        capped_product(
            fixed.assignments.capacity(),
            sizeof(refinement::PolicyObservationAssignment)));
    for (const refinement::PolicyObservationAssignment& assignment :
         fixed.assignments) {
        conversion_bytes = capped_add(
            conversion_bytes,
            observation_requirement_payload_bytes(assignment.required));
    }
    check_memory(
        conversion_bytes, "observation_assignment_conversion",
        fixed.assignments.size(),
        sizeof(refinement::PolicyObservationAssignment));
    for (refinement::PolicyObservationAssignment& assignment :
         fixed.assignments) {
        required.at(assignment.state_id) =
            std::move(assignment.required);
    }
    if (telemetry != nullptr) {
        telemetry->retained_bytes = conversion_bytes;
        telemetry->actual_peak_bytes = std::max(
            telemetry->actual_peak_bytes, conversion_bytes);
    }
    return required;
}

void append_stable_tokens(
    refinement::StableKey& target,
    const refinement::StableKey& tokens) {
    target.push_back(static_cast<std::uint64_t>(tokens.size()));
    target.insert(target.end(), tokens.begin(), tokens.end());
}

void append_feature_signature(
    refinement::StableKey& target,
    const refinement::FeatureSignature& features) {
    target.push_back(static_cast<std::uint64_t>(features.size()));
    for (const refinement::FeatureAtom& atom : features) {
        target.push_back(
            static_cast<std::uint64_t>(atom.feature));
        target.push_back(atom.subject);
        target.push_back(atom.affix_traits);
        target.push_back(atom.item_traits);
        target.push_back(
            static_cast<std::uint64_t>(
                atom.modifier_tag_ids.size()));
        target.insert(
            target.end(),
            atom.modifier_tag_ids.begin(),
            atom.modifier_tag_ids.end());
        append_stable_tokens(target, atom.value);
    }
}

void append_optional_u32(
    refinement::StableKey& target,
    const std::uint32_t value) {
    target.push_back(value == kNoId ? 0u : 1u);
    if (value != kNoId) target.push_back(value);
}

bool same_action_parameters(
    const ActionParameters& compiled,
    const ActionDescriptor& descriptor) {
    if (compiled.type != descriptor.params.type) return false;
    if (action_observes_modifier_offer(descriptor)) {
        /* The registry descriptor is the sampled-observation template; the
         * authored modifier selection lives only on the concrete graph node.
         * Every other parameter remains stable semantic identity and must
         * still select the correct template when a future mechanic exposes
         * multiple modifier-offer descriptors. */
        return compiled.essence_index ==
                   descriptor.params.essence_index &&
               compiled.fossil_indices ==
                   descriptor.params.fossil_indices &&
               compiled.target_tag_id ==
                   descriptor.params.target_tag_id &&
               compiled.source_tag_id ==
                   descriptor.params.source_tag_id &&
               compiled.influence_code ==
                   descriptor.params.influence_code &&
               compiled.tier == descriptor.params.tier;
    }
    switch (compiled.type) {
    case ActionType::Essence:
        return compiled.essence_index == descriptor.params.essence_index;
    case ActionType::Fossil:
        return compiled.fossil_indices == descriptor.params.fossil_indices;
    case ActionType::Bench:
        return compiled.mod_id == descriptor.params.mod_id;
    case ActionType::HarvestReforge:
    case ActionType::HarvestAugment:
        return compiled.target_tag_id == descriptor.params.target_tag_id;
    case ActionType::HarvestResist:
        return compiled.source_tag_id == descriptor.params.source_tag_id &&
               compiled.target_tag_id == descriptor.params.target_tag_id;
    case ActionType::EldritchEmber:
    case ActionType::EldritchIchor:
        return compiled.tier == descriptor.params.tier;
    case ActionType::InfluenceExalt:
        return compiled.influence_code == descriptor.params.influence_code;
    default:
        return true;
    }
}

void collect_condition_targets(
    const CompiledCondition& condition,
    const std::string& edge_id,
    std::vector<TargetEntry>& targets,
    std::vector<CountObservation>& count_observations,
    std::vector<std::string>& gaps) {
    if (condition.kind == ConditionKind::HasModFamily) {
        auto found = std::find_if(
            targets.begin(), targets.end(), [&](const TargetEntry& target) {
                return target.slot.family_id == condition.family_id;
            });
        const std::uint32_t threshold = static_cast<std::uint32_t>(
            std::max(0, condition.min_value));
        if (found == targets.end()) {
            TargetEntry target;
            target.slot.family_id = condition.family_id;
            target.slot.min_tier = threshold;
            target.origin = edge_id;
            targets.push_back(std::move(target));
        } else if (threshold != 0) {
            if (found->slot.min_tier != 0 &&
                found->slot.min_tier != threshold) {
                add_gap(
                    gaps,
                    "edge '" + edge_id + "' and edge '" + found->origin +
                        "' use different non-zero tier thresholds for "
                        "family " + std::to_string(condition.family_id) +
                        "; align the tiers");
            } else {
                found->slot.min_tier = threshold;
            }
        }
    } else if (condition.kind == ConditionKind::HasModGroup) {
        const auto found = std::find_if(
            targets.begin(), targets.end(), [&](const TargetEntry& target) {
                return target.slot.group_id == condition.group_id;
            });
        if (found == targets.end()) {
            TargetEntry target;
            target.slot.group_id = condition.group_id;
            target.slot.min_tier = static_cast<std::uint32_t>(
                std::max(0, condition.min_value));
            target.origin = edge_id;
            targets.push_back(std::move(target));
        } else if (condition.min_value != 0) {
            const std::uint32_t threshold = static_cast<std::uint32_t>(
                condition.min_value);
            if (found->slot.min_tier != 0 &&
                found->slot.min_tier != threshold) {
                add_gap(
                    gaps,
                    "edge '" + edge_id + "' and edge '" + found->origin +
                        "' use different non-zero tier thresholds for "
                        "group " + std::to_string(condition.group_id) +
                        "; align the tiers");
            } else {
                found->slot.min_tier = threshold;
            }
        }
    } else if (condition.kind == ConditionKind::ModCount ||
               condition.kind == ConditionKind::ModFamilyCount) {
        CountObservation observation;
        observation.by_family =
            condition.kind == ConditionKind::ModFamilyCount;
        observation.ids = observation.by_family ? condition.family_ids
                                                : condition.mod_ids;
        auto found = std::find_if(
            count_observations.begin(), count_observations.end(),
            [&](const CountObservation& existing) {
                return existing.by_family == observation.by_family &&
                       existing.ids == observation.ids;
            });
        if (found == count_observations.end()) {
            count_observations.push_back(std::move(observation));
            found = std::prev(count_observations.end());
        }
        if (condition.count_memo_slot != kNoId &&
            std::find(
                found->memo_slots.begin(), found->memo_slots.end(),
                condition.count_memo_slot) == found->memo_slots.end()) {
            found->memo_slots.push_back(condition.count_memo_slot);
        }
    } else if (condition.kind ==
               ConditionKind::ObservationSignature) {
        if (condition.observation_program == nullptr) {
            add_gap(
                gaps,
                "edge '" + edge_id +
                    "' has no observation-signature program");
        } else {
            const std::uint32_t observation_count =
                condition.observation_signature
                    .count_observation_count;
            for (std::uint32_t observation = 0;
                 observation < observation_count;
                 ++observation) {
                CountObservation exact;
                exact.by_family = false;
                const std::size_t word = observation / 64;
                const std::uint64_t bit =
                    std::uint64_t{1} << (observation % 64);
                for (std::uint32_t mod = 0;
                     mod <
                     condition.observation_program->context
                         .count_observation_membership_by_mod
                         .size();
                     ++mod) {
                    const refinement::StableKey& membership =
                        condition.observation_program->context
                            .count_observation_membership_by_mod[mod];
                    if (word < membership.size() &&
                        (membership[word] & bit) != 0) {
                        exact.ids.push_back(mod);
                    }
                }
                if (exact.ids.empty()) {
                    add_gap(
                        gaps,
                        "edge '" + edge_id +
                            "' observation-signature count context "
                            "contains an empty membership class");
                    continue;
                }
                if (observation < count_observations.size()) {
                    if (count_observations[observation].by_family ||
                        count_observations[observation].ids !=
                            exact.ids) {
                        add_gap(
                            gaps,
                            "edge '" + edge_id +
                                "' observation-signature count "
                                "context conflicts with an earlier "
                                "condition observer");
                    }
                } else if (observation ==
                           count_observations.size()) {
                    count_observations.push_back(
                        std::move(exact));
                } else {
                    add_gap(
                        gaps,
                        "edge '" + edge_id +
                            "' observation-signature count context "
                            "is non-contiguous");
                }
            }
        }
    } else if (condition.kind == ConditionKind::HasUnveilOption) {
        /*
         * Offer routing is evaluated directly from OutcomeChoiceOption below,
         * rather than from the projected item. Concrete modifiers that share
         * one exact successor state are behaviorally interchangeable for this
         * evaluator: routing through either choice produces the same state.
         * Do not add them as CountObservations, because one concrete unveil
         * option can legitimately refine (partially overlap) a goal-family
         * tier partition.
         */
        (void)count_observations;
    }
    for (const CompiledCondition& child : condition.children) {
        collect_condition_targets(
            child, edge_id, targets, count_observations, gaps);
    }
}

bool target_contains_mod(
    const SessionImpl& session,
    const GoalSlot& target,
    std::uint32_t mod) {
    if (target.family_id != kNoId) {
        return mod < session.family_id.size() &&
               session.family_id[mod] == target.family_id;
    }
    if (target.group_id >= session.group_masks.size() ||
        session.group_masks[target.group_id].empty()) {
        return false;
    }
    return pc_bitset_test(
        session.group_masks[target.group_id].data(), mod);
}

bool targets_overlap(
    const SessionImpl& session,
    const GoalSlot& a,
    const GoalSlot& b) {
    for (std::uint32_t mod = 0; mod < session.mod_count; ++mod) {
        if (target_contains_mod(session, a, mod) &&
            target_contains_mod(session, b, mod)) {
            return true;
        }
    }
    return false;
}

const std::vector<std::string>& operation_cost_keys(
        const ResolvedStrategyOperation& operation,
        const ActionRegistry& registry,
        const SessionImpl& session) {
    if (operation.kind == ResolvedStrategyOperationKind::Bestiary) {
        return session.data->bestiary_actions.at(
            operation.descriptor_index).cost_keys;
    }
    return registry.actions.at(operation.descriptor_index).cost_keys;
}

const std::string& operation_id(
        const ResolvedStrategyOperation& operation,
        const ActionRegistry& registry,
        const SessionImpl& session) {
    if (operation.kind == ResolvedStrategyOperationKind::Bestiary) {
        return session.data->bestiary_actions.at(
            operation.descriptor_index).id;
    }
    return registry.actions.at(operation.descriptor_index).id;
}

const std::string& operation_display_name(
        const ResolvedStrategyOperation& operation,
        const ActionRegistry& registry,
        const SessionImpl& session) {
    if (operation.kind == ResolvedStrategyOperationKind::Bestiary) {
        return session.data->bestiary_actions.at(
            operation.descriptor_index).display_name;
    }
    return registry.actions.at(operation.descriptor_index).display_name;
}

bool selector_matches_fresh_explicit(
        const RefinementAffixSelector& selector) {
    return refinement_selector_matches(selector, 0, 0, {});
}

bool requirement_observes_fresh_exclusion_identity(
        const ObservationRequirement& requirement) {
    const RefinementFeatureMask exclusion = refinement_feature(
        RefinementFeature::ModifierExclusionSignature);
    return std::any_of(
        requirement.affix_observations.begin(),
        requirement.affix_observations.end(),
        [&](const RefinementAffixObservation& observation) {
            return (observation.features & exclusion) != 0 &&
                   selector_matches_fresh_explicit(
                       observation.selector);
        });
}

bool contract_preserves_fresh_exclusion_identity(
        const ActionRefinementContract& contract) {
    const RefinementFeatureMask exclusion = refinement_feature(
        RefinementFeature::ModifierExclusionSignature);
    return std::any_of(
        contract.affix_flows.begin(), contract.affix_flows.end(),
        [&](const RefinementAffixFlow& flow) {
            return (flow.preserved_features & exclusion) != 0 &&
                   selector_matches_fresh_explicit(
                       flow.source_selector);
        });
}

std::uint64_t prelayout_string_vector_bytes(const std::vector<std::string>& values) {
    auto bytes = capped_product(values.capacity(), sizeof(std::string));
    for (const auto& value : values)
        bytes = capped_add(bytes, value.capacity() + 1);
    return bytes;
}

bool uniform_removal_observations_admitted(
        const std::vector<ObservationRequirement>& requirements) {
    constexpr auto allowed =
        refinement_feature(RefinementFeature::GoalStatusTierClass) |
        refinement_feature(RefinementFeature::ModifierSide) |
        refinement_feature(RefinementFeature::ModifierCrafted) |
        refinement_feature(RefinementFeature::ModifierFractured) |
        refinement_feature(RefinementFeature::ModifierVeiled) |
        refinement_feature(RefinementFeature::ModifierMetamodRole);
    for (const auto& requirement : requirements) {
        if (!requirement.modifier_tag_ids.empty()) return false;
        for (const auto& observation : requirement.affix_observations) {
            // A trait-scoped read of fractured/locked/veiled affixes is
            // impossible under the separately proved invariant. Test both
            // physical sides; a prefix-only exclusion read is still a read.
            const auto& selector = observation.selector;
            const bool matches =
                refinement_selector_matches(selector, kRefinementAffixPrefix, 0, {}) ||
                refinement_selector_matches(selector, kRefinementAffixSuffix, 0, {});
            if (matches && (observation.features & ~allowed) != 0) return false;
            if (!selector.required_tag_ids.empty()) return false;
        }
    }
    return true;
}

bool uniform_removal_condition_admitted(const CompiledCondition& condition) {
    // Structured signatures and offer/checkpoint reads retain physical
    // evaluation even when an individual authored condition is dead today.
    if (condition.kind == ConditionKind::ObservationSignature ||
        condition.kind == ConditionKind::HasUnveilOption ||
        condition.kind == ConditionKind::ModCount ||
        condition.kind == ConditionKind::ModFamilyCount ||
        condition.required_flags != 0) return false;
    return std::all_of(condition.children.begin(), condition.children.end(),
        uniform_removal_condition_admitted);
}

template <typename MemoryCheck>
EvalModel derive_model(
    const StrategyImpl& strategy,
    const StrategyEvalOptions& options,
    std::vector<ObservationRequirement>& node_observations,
    StrategyEvalResult::ObservationPropagationTelemetry* observation_telemetry,
    MemoryCheck&& check_memory) {
    if (!strategy.resources.empty()) {
        throw std::invalid_argument("Exact multi-item strategy evaluation requires inventory/control identity and is reserved for Pro; donor resources cannot be projected into one item");
    }

    for (const auto& node : strategy.nodes) {
        if (node.kind == StrategyNodeKind::Operation && node.action.type == ActionType::Vaal)
            throw std::invalid_argument("Exact corruption strategy evaluation requires implicit and terminal-state modelling and is reserved for Pro; use the supported native sampler");
    }
    const auto session = strategy.session;
    ActionRegistryBuildOptions registry_options;
    registry_options.exhaustive_fossils = false;
    const bool authored_dominance = std::any_of(strategy.nodes.begin(), strategy.nodes.end(),
        [](const StrategyNode& node) {
            return node.kind == StrategyNodeKind::Operation &&
                   node.action.type == ActionType::Dominance;
        });
    if (authored_dominance) {
        if (!options.continuation_entries.empty() ||
            std::any_of(options.policy_decision_entries.begin(), options.policy_decision_entries.end(),
                [](const auto& entry) { return !entry.graph_local; }))
            throw StrategyEvalUnsupported("Authored Dominance supports original-root graph evaluation only");
        validate_authored_dominance_item(*session, strategy.start_item);
        const std::function<bool(const CompiledCondition&)> condition_supported =
            [&](const CompiledCondition& condition) {
                if (condition.kind == ConditionKind::HasUnveilOption ||
                    condition.kind == ConditionKind::ObservationSignature ||
                    (condition.required_flags & ~(PC_MOD_SLOT_FRACTURED | PC_MOD_SLOT_CRAFTED)))
                    return false;
                return std::all_of(condition.children.begin(), condition.children.end(), condition_supported);
            };
        for (const auto& node : strategy.nodes) {
            for (const auto& edge : node.edges)
                if (!edge.is_default && !condition_supported(edge.condition))
                    throw StrategyEvalUnsupported("Authored Dominance has unsupported offer/signature or affix-flag conditions");
            if (node.kind == StrategyNodeKind::Operation &&
                (node.action_type != static_cast<int>(node.action.type) ||
                 node.bestiary_action_index != kNoId ||
                 !authored_dominance_action(node.action.type)) &&
                node.action_type != kStrategyRestartOperation)
                throw StrategyEvalUnsupported("Dominance supports exact structural acquisition, protection, addition and cleanup continuations only");
        }
    }
    const bool configured_cluster = session->is_cluster();
    if (configured_cluster) {
        if (!options.continuation_entries.empty() || !options.policy_decision_entries.empty() ||
            !options.graph_local_provenance.decisions.empty())
            throw StrategyEvalUnsupported("Configured cluster evaluation supports original-root graphs only");
        validate_cluster_exact_item(*session, strategy.start_item);
        const std::function<bool(const CompiledCondition&)> supported_condition =
            [&](const CompiledCondition& condition) {
                return condition.kind != ConditionKind::HasUnveilOption &&
                    !(condition.required_flags & ~(PC_MOD_SLOT_FRACTURED | PC_MOD_SLOT_CRAFTED)) &&
                    std::all_of(condition.children.begin(), condition.children.end(), supported_condition);
            };
        for (const auto& node : strategy.nodes) {
            for (const auto& edge : node.edges)
                if (!edge.is_default && !supported_condition(edge.condition))
                    throw StrategyEvalUnsupported("Configured cluster graph has an unsupported condition");
            if (node.kind == StrategyNodeKind::Operation &&
                (node.action_type != static_cast<int>(node.action.type) || node.bestiary_action_index != kNoId ||
                 !cluster_currency_qualified(node.action.type)))
                throw StrategyEvalUnsupported("Configured cluster graph action law is not qualified");
        }
    }
    registry_options.authored_dominance = authored_dominance;
    for (const StrategyNode& node : strategy.nodes) {
        if (node.kind != StrategyNodeKind::Operation ||
            node.action_type != static_cast<int>(ActionType::Fossil)) {
            continue;
        }
        std::vector<std::string> fossil_keys;
        bool valid = !node.action.fossil_indices.empty();
        for (const std::uint32_t fossil : node.action.fossil_indices) {
            if (fossil >= session->data->fossil_key_sids.size()) {
                valid = false;
                break;
            }
            fossil_keys.push_back(session->data->string_at(
                session->data->fossil_key_sids[fossil]));
        }
        if (!valid) continue;
        std::sort(fossil_keys.begin(), fossil_keys.end());
        std::string action_id = "fossil:";
        for (std::size_t i = 0; i < fossil_keys.size(); ++i) {
            if (i != 0) action_id += "+";
            action_id += fossil_keys[i];
        }
        registry_options.requested_fossil_action_ids.push_back(
            std::move(action_id));
    }
    ActionRegistry registry =
        build_action_registry(*session, registry_options);
    std::vector<std::string> gaps;
    std::vector<ResolvedStrategyOperation> operation_by_node(
        strategy.nodes.size());
    std::vector<std::uint32_t> action_by_node(
        strategy.nodes.size(), kNoId);
    std::vector<std::uint32_t> used_actions;

    for (std::size_t i = 0; i < strategy.nodes.size(); ++i) {
        const StrategyNode& node = strategy.nodes[i];
        if (node.kind != StrategyNodeKind::Operation) continue;
        const ResolvedStrategyOperation operation =
            resolve_strategy_operation(node, registry, *session);
        operation_by_node[i] = operation;
        if (!operation.resolved()) {
            add_gap(
                gaps,
                "node '" + node.id +
                    "' operation does not resolve to an engine descriptor");
            continue;
        }
        if (operation_cost_keys(operation, registry, *session) !=
            node.price_keys) {
            throw std::runtime_error(
                "strategy evaluation price-key mismatch at node '" +
                node.id + "'");
        }
        if (operation.kind == ResolvedStrategyOperationKind::Bestiary) {
            continue;
        }
        const std::uint32_t action = operation.descriptor_index;
        action_by_node[i] = action;
        const ActionDescriptor& descriptor = registry.actions[action];
        if (!calc_supports(descriptor)) {
            add_gap(
                gaps,
                "node '" + node.id + "' operation '" + descriptor.id +
                    "' has no exact calculator evaluator");
        }
        if (std::find(used_actions.begin(), used_actions.end(), action) ==
            used_actions.end()) {
            used_actions.push_back(action);
        }
    }

    std::vector<TargetEntry> target_entries;
    std::vector<CountObservation> count_observations;
    for (const StrategyNode& node : strategy.nodes) {
        for (const StrategyEdge& edge : node.edges) {
            if (!edge.is_default) {
                collect_condition_targets(
                    edge.condition, edge.id, target_entries,
                    count_observations, gaps);
            }
        }
    }
    for (std::size_t target = 0; target < strategy.nodes.size(); ++target) {
        const StrategyNode& node = strategy.nodes[target];
        const std::uint32_t action = action_by_node[target];
        if (node.kind != StrategyNodeKind::Operation ||
            action == kNoId ||
            !action_observes_modifier_offer(
                registry.actions[action])) {
            continue;
        }
        bool has_incoming = false;
        for (const StrategyNode& source : strategy.nodes) {
            for (const StrategyEdge& edge : source.edges) {
                if (edge.target != target) continue;
                has_incoming = true;
                const std::function<bool(const CompiledCondition&)>
                    contains_selected_offer =
                        [&](const CompiledCondition& condition) {
                            if (condition.kind ==
                                    ConditionKind::HasUnveilOption &&
                                std::find(
                                    condition.mod_ids.begin(),
                                    condition.mod_ids.end(),
                                    node.action.mod_id) !=
                                    condition.mod_ids.end()) {
                                return true;
                            }
                            return std::any_of(
                                condition.children.begin(),
                                condition.children.end(),
                                contains_selected_offer);
                        };
                if (edge.is_default ||
                    !contains_selected_offer(edge.condition)) {
                    add_gap(
                        gaps,
                        "node '" + node.id +
                            "' authored observed selection must be entered "
                            "through a matching has_unveil_option edge");
                }
            }
        }
        if (!has_incoming) {
            add_gap(
                gaps,
                "node '" + node.id +
                "' authored observed selection has no offer-routing edge");
        }
    }
    if (target_entries.size() > kMaxGoalSlots) {
        const std::string offender =
            target_entries[kMaxGoalSlots].origin;
        add_gap(
            gaps,
            "edge '" + offender + "' brings the graph to " +
                std::to_string(target_entries.size()) +
                " distinct condition targets; the exact limit is " +
                std::to_string(kMaxGoalSlots));
    }
    for (std::size_t a = 0; a < target_entries.size(); ++a) {
        for (std::size_t b = a + 1; b < target_entries.size(); ++b) {
            if (targets_overlap(
                    *session, target_entries[a].slot,
                    target_entries[b].slot)) {
                add_gap(
                    gaps,
                    "edge '" + target_entries[a].origin + "' and edge '" +
                        target_entries[b].origin +
                        "' reference overlapping family/group targets; "
                        "align the conditions");
            }
        }
    }
    if (!gaps.empty()) throw StrategyEvalUnsupported(join_gaps(gaps));

    GoalSpec goal;
    for (const TargetEntry& target : target_entries) {
        goal.slots.push_back(target.slot);
    }
    goal.min_satisfied_slots =
        static_cast<std::uint32_t>(goal.slots.size());

    // Run the existing backward observation fixed point before selecting a
    // calculator layout. Reads seed requirements; survivor flows only carry
    // downstream requirements. Do not create a second propagation owner.
    std::uint64_t prelayout_bytes = capped_product(
        registry.actions.capacity(), sizeof(ActionDescriptor));
    for (const auto& descriptor : registry.actions) {
        prelayout_bytes = capped_add(prelayout_bytes,
            descriptor.id.capacity() + descriptor.display_name.capacity() + 2);
        prelayout_bytes = capped_add(prelayout_bytes,
            prelayout_string_vector_bytes(descriptor.cost_keys));
        prelayout_bytes = capped_add(prelayout_bytes,
            capped_product(descriptor.discriminating_tag_ids.capacity() +
                descriptor.params.fossil_indices.capacity(), sizeof(std::uint32_t)));
        prelayout_bytes = capped_add(prelayout_bytes,
            refinement_contract_payload_bytes(descriptor.refinement));
    }
    prelayout_bytes = capped_add(prelayout_bytes,
        capped_product(registry.index_by_id.bucket_count(), sizeof(void*)));
    prelayout_bytes = capped_add(prelayout_bytes,
        capped_product(registry.index_by_id.size(),
            sizeof(typename decltype(registry.index_by_id)::value_type) + 3 * sizeof(void*)));
    for (const auto& [id, index] : registry.index_by_id) {
        (void)index;
        prelayout_bytes = capped_add(prelayout_bytes, id.capacity() + 1);
    }
    prelayout_bytes = capped_add(prelayout_bytes,
        capped_product(operation_by_node.capacity(), sizeof(ResolvedStrategyOperation)));
    prelayout_bytes = capped_add(prelayout_bytes,
        capped_product(action_by_node.capacity() + used_actions.capacity(), sizeof(std::uint32_t)));
    prelayout_bytes = capped_add(prelayout_bytes,
        capped_product(target_entries.capacity(), sizeof(TargetEntry)));
    for (const auto& target : target_entries)
        prelayout_bytes = capped_add(prelayout_bytes, target.origin.capacity() + 1);
    prelayout_bytes = capped_add(prelayout_bytes,
        capped_product(goal.slots.capacity(), sizeof(GoalSlot)));
    prelayout_bytes = capped_add(prelayout_bytes,
        capped_product(count_observations.capacity(), sizeof(CountObservation)));
    for (const auto& observation : count_observations) {
        prelayout_bytes = capped_add(prelayout_bytes,
            capped_product(observation.ids.capacity() + observation.memo_slots.capacity(),
                sizeof(std::uint32_t)));
    }
    prelayout_bytes = capped_add(prelayout_bytes,
        prelayout_string_vector_bytes(registry_options.requested_fossil_action_ids));
    const auto observation_started = std::chrono::steady_clock::now();
    node_observations = derive_node_observation_requirements(
        strategy, registry, operation_by_node, options.max_sweeps,
        observation_telemetry,
        [&](const std::uint64_t transient, const char* stage,
            const std::uint64_t units, const std::uint64_t unit_bytes) {
            check_memory(capped_add(prelayout_bytes, transient), stage, units, unit_bytes);
        });
    if (observation_telemetry != nullptr)
        observation_telemetry->duration_ns = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - observation_started).count());
    std::uint64_t observation_bytes = capped_product(
        node_observations.capacity(), sizeof(ObservationRequirement));
    for (const auto& requirement : node_observations)
        observation_bytes = capped_add(observation_bytes,
            observation_requirement_payload_bytes(requirement));
    // Native capability/slot proofs own only bounded session masks and at
    // most kMaxGoalSlots resolved member masks/group lists, never state rows.
    std::uint64_t max_contract_scratch = 0;
    for (const auto action : used_actions)
        max_contract_scratch = std::max(max_contract_scratch,
            capped_product(capped_add(sizeof(ActionRefinementContract),
                refinement_contract_payload_bytes(registry.actions[action].refinement)), 4));
    // Each resolved slot owns two masks plus a group vector. Resolution's
    // temporary dedup vector and the common-group intersection can retain
    // geometric capacity. Bound all of them together with the registry's
    // native-contract derivation scratch, before invoking either proof.
    const auto proof_scratch = capped_add(max_contract_scratch, capped_add(
        capped_product(kMaxGoalSlots, sizeof(ResolvedGoalSlot)), capped_add(
        capped_product(session->words, (2 * kMaxGoalSlots + 3) * sizeof(std::uint64_t)),
        capped_product(session->group_ids.size(),
            (2 * kMaxGoalSlots + 4) * sizeof(std::uint32_t)))));
    check_memory(capped_add(prelayout_bytes, capped_add(observation_bytes, proof_scratch)),
        "uniform_removal_native_admission", session->words, sizeof(std::uint64_t));

    /*
     * Choose the evaluator carrier from the same scoped semantic contracts
     * used by policy refinement. A clean graph whose operations destroy every
     * ordinary freshly rolled explicit before any operation or router can
     * observe its exclusion identity has no cross-operation group state. The
     * coarse layout is exact there and avoids eagerly enumerating concrete
     * junk combinations for a destructive renewal cycle.
     *
     * Exact authored carriers always remain strict. So do direct structured
     * observations and any operation whose declared survivor flow can carry
     * a fresh explicit's exclusion identity. Trait-scoped flows such as a
     * fractured-only survivor do not force global strictness from a clean
     * start: no admitted operation that passes this test can create that
     * trait without also exposing an identity-preserving fresh-affix flow.
     */
    const bool clean_start_carrier =
        strategy.start_item.prefix_count == 0 &&
        strategy.start_item.suffix_count == 0 &&
        strategy.start_item.item_flags == 0 &&
        strategy.start_item.generic_influence_bits == 0 &&
        strategy.start_item.searing_exarch_tier == 0 &&
        strategy.start_item.eater_of_worlds_tier == 0;
    bool direct_router_observes_fresh_exclusion = false;
    for (const StrategyNode& node : strategy.nodes) {
        for (const StrategyEdge& edge : node.edges) {
            if (edge.is_default) continue;
            if (requirement_observes_fresh_exclusion_identity(
                    condition_observation_requirement(
                        edge.condition))) {
                direct_router_observes_fresh_exclusion = true;
                break;
            }
        }
        if (direct_router_observes_fresh_exclusion) break;
    }
    const bool operation_preserves_fresh_exclusion =
        std::any_of(
            used_actions.begin(), used_actions.end(),
            [&](const std::uint32_t action) {
                return action >= registry.actions.size() ||
                       contract_preserves_fresh_exclusion_identity(
                           registry.actions[action].refinement);
            });
    bool all_unprotected_laws = !used_actions.empty();
    bool has_uniform_removal = false;
    for (const auto action : used_actions) {
        const auto law = native_unprotected_affix_law(*session, registry.actions[action]);
        all_unprotected_laws &= law != UnprotectedAffixLaw::Unsupported;
        has_uniform_removal |= law == UnprotectedAffixLaw::UniformRemoval;
    }
    bool all_conditions_admitted = true;
    for (const auto& node : strategy.nodes) {
        if (node.kind == StrategyNodeKind::Operation &&
            operation_by_node[&node - strategy.nodes.data()].kind ==
                ResolvedStrategyOperationKind::Bestiary) all_unprotected_laws = false;
        for (const auto& edge : node.edges)
            if (!edge.is_default)
                all_conditions_admitted &= uniform_removal_condition_admitted(edge.condition);
    }
    const bool certified_uniform_removal =
        !configured_cluster && options.use_exact_exchangeable_family_compression &&
        clean_start_carrier && strategy.start_item.implicit_count == 0 &&
        strategy.start_item.quality == 0 &&
        strategy.start_item.lifecycle == PC_ITEM_LIVE &&
        strategy.start_item.enchantment_count == 0 &&
        strategy.start_item.memory_strands == 0 &&
        strategy.start_item.socket_count == 0 && strategy.start_item.link_mask == 0 &&
        options.continuation_entries.empty() &&
        options.policy_decision_entries.empty() &&
        options.graph_local_provenance.decisions.empty() &&
        count_observations.empty() && all_unprotected_laws && has_uniform_removal &&
        all_conditions_admitted && uniform_removal_observations_admitted(node_observations) &&
        prove_uniform_removal_goals(*session, goal).has_value();
    const bool semantic_strict_carrier =
        configured_cluster || authored_dominance || !clean_start_carrier ||
        direct_router_observes_fresh_exclusion ||
        (operation_preserves_fresh_exclusion && !certified_uniform_removal);
    /* This flag selects an exact calculator implementation, not a solver-row
     * reuse path. Keep strict/identity-observing strategies on physical
     * families even when the focused evaluator compression is enabled. */
    const bool product_exact_reforge_carrier =
        options.use_exact_exchangeable_family_compression &&
        !semantic_strict_carrier;

    EvalModel model;
    model.operation_by_node = std::move(operation_by_node);
    model.action_by_node = std::move(action_by_node);
    model.targets = goal.slots;
    std::vector<std::uint64_t> exact_start_mods(session->words, 0);
    const auto retain_start_mods =
        [&](const pc_mod_slot* slots,
            const std::uint8_t count) {
            for (std::uint8_t i = 0; i < count; ++i) {
                if (slots[i].mod_id < session->mod_count) {
                    pc_bitset_set(
                        exact_start_mods.data(), slots[i].mod_id);
                }
            }
        };
    retain_start_mods(
        strategy.start_item.prefixes,
        strategy.start_item.prefix_count);
    retain_start_mods(
        strategy.start_item.suffixes,
        strategy.start_item.suffix_count);
    if (authored_dominance) {
        // Session inventory includes upgrade destinations above item level.
        // A safe superset avoids duplicating native progression/mapping law.
        for (std::uint32_t mod = 0; mod < session->mod_count; ++mod)
            if (session->gen_type[mod] == PC_SIDE_PREFIX ||
                session->gen_type[mod] == PC_SIDE_SUFFIX)
                pc_bitset_set(exact_start_mods.data(), mod);
    }
    try {
        model.calc = std::make_unique<CalcContext>(
            session, goal, std::move(registry), used_actions,
            true,  /* allow count/rarity-only graphs */
            false, /* no operations must not mean the full registry */
            semantic_strict_carrier,
            /* observer-conditioned exact carrier when required */
            options.max_states, count_observations,
            product_exact_reforge_carrier,
            exact_start_mods,
            authored_dominance, /* singleton identity before every continuation */
            false, /* reforge attribution is reported by the evaluator */
            false, /* do not alter physical frontier enumeration */
            false, /* retain canonical bucket order */
            true,  /* factor unobserved terminal mass in gated renewals */
            nullptr,
            /* build_action_registry() canonicalizes and validates every
             * refinement contract before returning. Exact evaluation owns
             * that freshly built registry, so repeating the full registry
             * proof in CalcContext only delays the first cooperative step. */
            true, certified_uniform_removal);
    } catch (const std::exception& ex) {
        std::string origin;
        for (const TargetEntry& target : target_entries) {
            if (!origin.empty()) origin += ", ";
            origin += "'" + target.origin + "'";
        }
        throw StrategyEvalUnsupported(
            "strategy evaluation unsupported:\n- condition targets on "
            "edge(s) " + origin + " cannot share one exact abstraction: " +
            ex.what());
    }
    return model;
}

template <typename MemoryCheck>
EvalModel derive_checked_model(
    const std::shared_ptr<const StrategyImpl>& strategy,
    const StrategyEvalOptions& options,
    std::vector<ObservationRequirement>& node_observations,
    StrategyEvalResult::ObservationPropagationTelemetry* observation_telemetry,
    MemoryCheck&& check_memory) {
    if (strategy == nullptr) {
        throw std::invalid_argument("invalid compiled strategy");
    }
    return derive_model(
        *strategy, options, node_observations, observation_telemetry,
        std::forward<MemoryCheck>(check_memory));
}

std::size_t layout_slot_for(
    const CompiledCondition& condition,
    const AbstractLayout& layout) {
    for (std::size_t i = 0; i < layout.slots.size(); ++i) {
        const GoalSlot& slot = layout.slots[i].spec;
        if (condition.kind == ConditionKind::HasModFamily &&
            slot.family_id == condition.family_id) {
            return i;
        }
        if (condition.kind == ConditionKind::HasModGroup &&
            slot.group_id == condition.group_id) {
            return i;
        }
    }
    throw std::logic_error("compiled evaluation condition target is absent");
}

double absorbed_probability(const StrategyEvalResult& result) {
    return result.success_probability + result.failure_probability +
           result.stop_probability +
           result.action_not_applied_probability +
           result.no_matching_edge_probability;
}

std::string json_escape(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (unsigned char c : text) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c < 0x20) {
                const char* hex = "0123456789abcdef";
                out += "\\u00";
                out.push_back(hex[(c >> 4) & 0xf]);
                out.push_back(hex[c & 0xf]);
            } else {
                out.push_back(static_cast<char>(c));
            }
        }
    }
    return out;
}

class BoundedJson {
  public:
    explicit BoundedJson(std::uint64_t limit) : limit_(limit) {}

    BoundedJson& operator+=(const std::string& text) {
        append(text);
        return *this;
    }

    BoundedJson& operator+=(const char* text) {
        append(text == nullptr ? std::string_view{} : std::string_view(text));
        return *this;
    }

    BoundedJson& operator+=(char value) {
        push_back(value);
        return *this;
    }

    void push_back(char value) {
        ensure(1);
        value_.push_back(value);
    }

    std::string take() && { return std::move(value_); }

  private:
    void append(std::string_view text) {
        ensure(text.size());
        value_.append(text.data(), text.size());
    }

    void ensure(std::size_t additional) const {
        if (value_.size() > limit_ || additional > limit_ - value_.size()) {
            throw std::length_error(
                "strategy evaluation exceeded max_output_json_bytes (" +
                std::to_string(limit_) + ")");
        }
    }

    std::string value_;
    std::uint64_t limit_;
};

void append_number(BoundedJson& out, double value) {
    if (value == 0.0) value = 0.0; /* canonicalize negative zero */
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << std::setprecision(17) << value;
    out += stream.str();
}

const char* terminal_name(int kind) {
    switch (kind) {
    case PC_TERMINAL_SUCCESS: return "success";
    case PC_TERMINAL_FAILURE: return "failure";
    case PC_TERMINAL_STOP: return "stop";
    default: return "failure";
    }
}

void add_classification(
    std::vector<std::string>& classifications,
    const std::string& value) {
    if (std::find(
            classifications.begin(), classifications.end(), value) ==
        classifications.end()) {
        classifications.push_back(value);
    }
}

std::map<std::string, double> empty_technique_totals() {
    return {
        {"ordinary_crafting_actions", 0.0},
        {"restart_actions", 0.0},
        {"base_consumptions", 0.0},
        {"fracture_preparation_actions", 0.0},
        {"fracture_actions", 0.0},
        {"retry_actions", 0.0},
        {"retry_count", 0.0},
        {"temporary_blocker_applications", 0.0},
        {"permanent_goal_bench_finishes", 0.0},
        {"multimod_setup_actions", 0.0},
        {"multimod_finishing_bench_actions", 0.0},
        {"protection_setup_actions", 0.0},
        {"protection_reapplications", 0.0},
        {"crafted_mod_cleanup_or_replacement_actions", 0.0},
        {"deterministic_finishing_actions", 0.0},
    };
}

void add_role_work(
    std::map<std::string, double>& totals,
    const std::string& role,
    double visits,
    double applied) {
    if (role == "ordinary_crafting") {
        totals["ordinary_crafting_actions"] += visits;
    } else if (role == "restart") {
        totals["restart_actions"] += visits;
        totals["base_consumptions"] += applied;
    } else if (role == "fracture_preparation") {
        totals["fracture_preparation_actions"] += visits;
    } else if (role == "fracture") {
        totals["fracture_actions"] += visits;
    } else if (role == "retry_action") {
        totals["retry_actions"] += visits;
    } else if (role == "retry") {
        totals["retry_count"] += visits;
    } else if (role == "temporary_blocker") {
        totals["temporary_blocker_applications"] += applied;
    } else if (role == "permanent_goal_bench") {
        totals["permanent_goal_bench_finishes"] += applied;
    } else if (role == "multimod_setup") {
        totals["multimod_setup_actions"] += applied;
    } else if (role == "multimod_finish") {
        totals["multimod_finishing_bench_actions"] += applied;
    } else if (role == "protection_setup") {
        totals["protection_setup_actions"] += applied;
    } else if (role == "protection_reapplication") {
        totals["retry_count"] += visits;
        totals["protection_reapplications"] += visits;
    } else if (role == "cleanup_or_replacement") {
        totals["crafted_mod_cleanup_or_replacement_actions"] += applied;
    } else if (role == "deterministic_finish") {
        totals["deterministic_finishing_actions"] += applied;
    }
}

std::vector<StrategyEvalMaterialTotal> price_materials(
    const std::map<std::string, double>& quantities,
    const std::shared_ptr<const EconomyImpl>& economy,
    double& known_cost,
    bool& complete) {
    known_cost = 0.0;
    complete = economy != nullptr;
    std::vector<StrategyEvalMaterialTotal> materials;
    materials.reserve(quantities.size());
    for (const auto& [key, quantity] : quantities) {
        StrategyEvalMaterialTotal material;
        material.price_key = key;
        material.expected_quantity = quantity;
        if (economy != nullptr) {
            const auto found = economy->prices.find(key);
            if (found != economy->prices.end()) {
                material.priced = true;
                material.unit_price = found->second;
                material.cost_contribution = quantity * found->second;
                known_cost += material.cost_contribution;
            } else {
                complete = false;
            }
        }
        materials.push_back(std::move(material));
    }
    return materials;
}

std::vector<ReviewSectionSpec> parse_review_sections(
    const StrategyImpl& strategy,
    const std::string& document) {
    if (document.empty()) return {};
    Value root = Parser(document.data(), document.size()).parse();
    if (root.type != Type::Object) {
        throw std::invalid_argument("review projection root must be an object");
    }
    const Value* schema = root.find("schema_version");
    if (schema == nullptr || schema->type != Type::String ||
        schema->string != "solver_review_projection_v1") {
        throw std::invalid_argument(
            "review projection must use solver_review_projection_v1");
    }
    const Value* raw = root.find("raw_strategy");
    if (raw == nullptr || raw->type != Type::Object) {
        throw std::invalid_argument("review projection requires raw_strategy");
    }
    const Value* authority = raw->find("execution_authority");
    if (authority == nullptr || authority->type != Type::String ||
        authority->string != "raw_strategy_only") {
        throw std::invalid_argument(
            "review projection cannot have execution authority");
    }
    const Value* sections = root.find("sections");
    if (sections == nullptr || sections->type != Type::Array ||
        sections->array.empty()) {
        throw std::invalid_argument("review projection requires sections");
    }

    std::unordered_map<std::string, std::uint32_t> node_by_id;
    std::unordered_set<std::string> edge_ids;
    for (std::uint32_t node = 0; node < strategy.nodes.size(); ++node) {
        node_by_id.emplace(strategy.nodes[node].id, node);
        for (const StrategyEdge& edge : strategy.nodes[node].edges) {
            edge_ids.emplace(edge.id);
        }
    }
    std::unordered_set<std::string> seen_sections;
    std::unordered_set<std::string> seen_nodes;
    std::unordered_set<std::string> seen_edges;
    std::vector<ReviewSectionSpec> result;
    for (const Value& section_value : sections->array) {
        if (section_value.type != Type::Object) {
            throw std::invalid_argument("review section must be an object");
        }
        const auto required_string = [&](const char* key) -> std::string {
            const Value* value = section_value.find(key);
            if (value == nullptr || value->type != Type::String ||
                value->string.empty()) {
                throw std::invalid_argument(
                    std::string("review section requires ") + key);
            }
            return value->string;
        };
        ReviewSectionSpec section;
        section.id = required_string("id");
        section.label = required_string("label");
        section.role = required_string("role");
        if (!seen_sections.emplace(section.id).second) {
            throw std::invalid_argument("duplicate review section id");
        }
        const Value* references = section_value.find("raw_references");
        if (references == nullptr || references->type != Type::Array) {
            throw std::invalid_argument(
                "review section requires raw_references");
        }
        for (const Value& reference : references->array) {
            if (reference.type != Type::Object) {
                throw std::invalid_argument(
                    "review raw reference must be an object");
            }
            const Value* node_id = reference.find("node_id");
            const Value* edge_id = reference.find("edge_id");
            if ((node_id == nullptr) == (edge_id == nullptr)) {
                throw std::invalid_argument(
                    "review reference must name one raw node or edge");
            }
            if (node_id != nullptr) {
                if (node_id->type != Type::String ||
                    !node_by_id.contains(node_id->string) ||
                    !seen_nodes.emplace(node_id->string).second) {
                    throw std::invalid_argument(
                        "review projection has an unresolved or duplicate raw node");
                }
                section.nodes.push_back(node_by_id.at(node_id->string));
            } else {
                if (edge_id->type != Type::String ||
                    !edge_ids.contains(edge_id->string) ||
                    !seen_edges.emplace(edge_id->string).second) {
                    throw std::invalid_argument(
                        "review projection has an unresolved or duplicate raw edge");
                }
                section.edges.push_back(edge_id->string);
            }
        }
        result.push_back(std::move(section));
    }
    if (seen_nodes.size() != strategy.nodes.size() ||
        seen_edges.size() != edge_ids.size()) {
        throw std::invalid_argument(
            "review projection must cover every raw node and edge exactly once");
    }
    std::vector<std::size_t> section_by_node(strategy.nodes.size());
    for (std::size_t section = 0; section < result.size(); ++section) {
        for (const std::uint32_t node : result[section].nodes) {
            section_by_node[node] = section;
        }
    }
    std::unordered_map<std::string, std::size_t> section_by_edge;
    for (std::size_t section = 0; section < result.size(); ++section) {
        for (const std::string& edge : result[section].edges) {
            section_by_edge.emplace(edge, section);
        }
    }
    for (std::size_t source = 0; source < strategy.nodes.size(); ++source) {
        for (const StrategyEdge& edge : strategy.nodes[source].edges) {
            if (section_by_edge.at(edge.id) != section_by_node[source]) {
                throw std::invalid_argument(
                    "review edge must be owned by its source-node section");
            }
        }
    }
    return result;
}

} // namespace

} // namespace solver
} // namespace poecraft
