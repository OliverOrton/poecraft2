#include "solver_internal.hpp"

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
    std::uint32_t target = kNoId;
    double probability = 0.0;
    std::uint32_t edge = kNoId;
    /* When pass-through contraction rewrites this transition, the pair it
     * originally entered (the head of the folded deterministic chain).
     * Flow committed through the transition is credited to that chain. */
    std::uint32_t via = kNoId;
    /* First compiler-generated policy_route_* node skipped during discovery.
     * Its exact state-specific path is replayed once flow is known. */
    std::uint32_t policy_route = kNoId;
    /* Exact state used to traverse policy_route. Operation-pair refinement
     * may merge input states after that router selected the same action, so
     * the target pair's representative is not authoritative for replay. */
    std::uint32_t policy_state = kNoId;
};

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
    std::vector<EvalAbsorption> absorptions;
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
    bool operation = false;
    bool consumes = false;
    std::uint32_t action = kNoId;
    std::uint32_t row = kNoId;
};

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
    const EvalModel& model,
    const std::uint32_t max_rounds,
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
    check_memory(direct_bytes);

    std::vector<refinement::PolicyObservationNode> nodes;
    nodes.reserve(node_count);
    for (std::uint32_t node = 0; node < node_count; ++node) {
        refinement::PolicyObservationNode observation;
        observation.state_id = node;
        observation.direct_observes = std::move(direct[node]);
        if (strategy.nodes[node].kind ==
            StrategyNodeKind::Operation) {
            const ResolvedStrategyOperation& operation =
                model.operation_by_node.at(node);
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
                model.calc->registry().actions.at(action_index)
                    .refinement;
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
    std::uint64_t requirement_vocabulary_bytes = 0;
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
            requirement_vocabulary_bytes = capped_add(
                requirement_vocabulary_bytes,
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
    const std::uint64_t one_requirement_bytes = capped_add(
        capped_add(
            sizeof(ObservationRequirement),
            observation_requirement_payload_bytes(union_requirement)),
        requirement_vocabulary_bytes);
    /* propagate_policy_observations owns the graph, its ordered state index,
     * two fixed-point vectors, and finally one assignment vector. Every
     * propagated requirement is a subset of this deterministic union. */
    std::uint64_t propagation_peak = nodes_bytes;
    propagation_peak = capped_add(
        propagation_peak,
        capped_product(
            node_count,
            sizeof(std::pair<const std::uint32_t, std::uint32_t>) +
                3 * sizeof(void*)));
    propagation_peak = capped_add(
        propagation_peak,
        capped_product(
            node_count,
            capped_product(one_requirement_bytes, 3)));
    check_memory(propagation_peak);
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
    check_memory(conversion_bytes);
    for (refinement::PolicyObservationAssignment& assignment :
         fixed.assignments) {
        required.at(assignment.state_id) =
            std::move(assignment.required);
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

EvalModel derive_model(
    const StrategyImpl& strategy,
    std::optional<std::uint32_t> state_cap) {
    const auto session = strategy.session;
    ActionRegistry registry = build_action_registry(*session);
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
    const bool semantic_strict_carrier =
        !clean_start_carrier ||
        direct_router_observes_fresh_exclusion ||
        operation_preserves_fresh_exclusion;

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
    try {
        model.calc = std::make_unique<CalcContext>(
            session, goal, std::move(registry), used_actions,
            true,  /* allow count/rarity-only graphs */
            false, /* no operations must not mean the full registry */
            semantic_strict_carrier,
            /* observer-conditioned exact carrier when required */
            state_cap, count_observations, false,
            exact_start_mods,
            false); /* observer-derived semantic strict carrier */
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

EvalModel derive_checked_model(
    const std::shared_ptr<const StrategyImpl>& strategy,
    std::uint32_t max_states) {
    if (strategy == nullptr) {
        throw std::invalid_argument("invalid compiled strategy");
    }
    return derive_model(*strategy, max_states);
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

namespace {

std::uint32_t resolve_registry_strategy_action(
        const StrategyNode& node,
        const ActionRegistry& registry) {
    if (node.kind != StrategyNodeKind::Operation) return kNoId;
    if (node.action_type == kStrategyBestiaryImprintOperation ||
        node.action_type == kStrategyBestiaryRestoreImprintOperation) {
        return kNoId;
    }
    if (node.action_type != kStrategyRestartOperation &&
        (node.action_type < static_cast<int>(ActionType::Transmute) ||
         node.action_type >
             static_cast<int>(ActionType::RemoveCraftedModifiers) ||
         node.action_type != static_cast<int>(node.action.type))) {
        return kNoId;
    }
    for (std::uint32_t i = 0; i < registry.actions.size(); ++i) {
        const ActionDescriptor& descriptor = registry.actions[i];
        if (node.action_type == kStrategyRestartOperation) {
            if (descriptor.synthetic) return i;
            continue;
        }
        if (descriptor.synthetic) continue;
        if (same_action_parameters(node.action, descriptor)) return i;
    }
    return kNoId;
}

} // namespace

ResolvedStrategyOperation resolve_strategy_operation(
        const StrategyNode& node,
        const ActionRegistry& registry,
        const SessionImpl& session) {
    if (node.kind != StrategyNodeKind::Operation) return {};
    if (node.bestiary_action_index != kNoId) {
        if (node.bestiary_action_index >=
            session.data->bestiary_actions.size()) {
            return {};
        }
        const BestiaryActionDescriptor& descriptor =
            session.data->bestiary_actions[node.bestiary_action_index];
        const int expected_type =
            descriptor.operation == BestiaryOperationKind::Create
                ? kStrategyBestiaryImprintOperation
                : kStrategyBestiaryRestoreImprintOperation;
        if (node.action_type != expected_type) return {};
        return {
            ResolvedStrategyOperationKind::Bestiary,
            node.bestiary_action_index};
    }
    if (node.action_type == kStrategyBestiaryImprintOperation ||
        node.action_type == kStrategyBestiaryRestoreImprintOperation) {
        return {};
    }
    const std::uint32_t action =
        resolve_registry_strategy_action(node, registry);
    if (action == kNoId) return {};
    return {
        node.action_type == kStrategyRestartOperation
            ? ResolvedStrategyOperationKind::Restart
            : ResolvedStrategyOperationKind::Ordinary,
        action};
}

std::uint32_t resolve_strategy_action(
        const StrategyNode& node,
        const ActionRegistry& registry) {
    return resolve_registry_strategy_action(node, registry);
}

bool evaluate_abstract_condition(
    const CompiledCondition& condition,
    const SessionImpl& session,
    const AbstractLayout& layout,
    const AbstractState& state) {
    switch (condition.kind) {
    case ConditionKind::Always:
        return true;
    case ConditionKind::HasModGroup: {
        const std::size_t slot = layout_slot_for(condition, layout);
        const bool present = condition.min_value == 0
                                 ? state.slot_status[slot] !=
                                       static_cast<std::uint8_t>(
                                           GoalSlotStatus::Absent)
                                 : state.slot_status[slot] ==
                                       static_cast<std::uint8_t>(
                                           GoalSlotStatus::Satisfied);
        if (!present) return false;
        if ((condition.required_flags & PC_MOD_SLOT_FRACTURED) != 0 &&
            (state.fractured_goal_mask & (1u << slot)) == 0) {
            return false;
        }
        if ((condition.required_flags & PC_MOD_SLOT_CRAFTED) != 0 &&
            (state.crafted_goal_mask & (1u << slot)) == 0) {
            return false;
        }
        return true;
    }
    case ConditionKind::HasModFamily: {
        const std::size_t slot = layout_slot_for(condition, layout);
        const bool present = condition.min_value == 0
                                 ? state.slot_status[slot] !=
                                       static_cast<std::uint8_t>(
                                           GoalSlotStatus::Absent)
                                 : state.slot_status[slot] ==
                                       static_cast<std::uint8_t>(
                                           GoalSlotStatus::Satisfied);
        if (!present) return false;
        if ((condition.required_flags & PC_MOD_SLOT_FRACTURED) != 0 &&
            (state.fractured_goal_mask & (1u << slot)) == 0) {
            return false;
        }
        if ((condition.required_flags & PC_MOD_SLOT_CRAFTED) != 0 &&
            (state.crafted_goal_mask & (1u << slot)) == 0) {
            return false;
        }
        return true;
    }
    case ConditionKind::RarityIs:
        return state.rarity == condition.min_value;
    case ConditionKind::OpenPrefixCount: {
        const int open = std::max(
            0, static_cast<int>(rarity_affix_cap(session, state.rarity)) -
                   static_cast<int>(state.prefix_count));
        return open >= condition.min_value && open <= condition.max_value;
    }
    case ConditionKind::OpenSuffixCount: {
        const int open = std::max(
            0, static_cast<int>(rarity_affix_cap(session, state.rarity)) -
                   static_cast<int>(state.suffix_count));
        return open >= condition.min_value && open <= condition.max_value;
    }
    case ConditionKind::PrefixCountRange:
        return state.prefix_count >= condition.min_value &&
               state.prefix_count <= condition.max_value;
    case ConditionKind::SuffixCountRange:
        return state.suffix_count >= condition.min_value &&
               state.suffix_count <= condition.max_value;
    case ConditionKind::ItemFlag: {
        std::uint32_t flag = 0;
        switch (condition.item_flag) {
        case ItemFlagKind::Corrupted: flag = kFlagCorrupted; break;
        case ItemFlagKind::Mirrored: flag = kFlagMirrored; break;
        case ItemFlagKind::Split: flag = kFlagSplit; break;
        case ItemFlagKind::Synthesised: flag = kFlagSynthesised; break;
        case ItemFlagKind::Fractured: flag = kFlagFractured; break;
        case ItemFlagKind::Crafted: flag = kFlagCraftedMod; break;
        case ItemFlagKind::Veiled: flag = kFlagVeiledMod; break;
        case ItemFlagKind::Multimod: flag = kFlagMultimod; break;
        case ItemFlagKind::NoAttack: flag = kFlagNoAttack; break;
        case ItemFlagKind::NoCaster: flag = kFlagNoCaster; break;
        case ItemFlagKind::PrefixesLocked: flag = kFlagPrefixesLocked; break;
        case ItemFlagKind::SuffixesLocked: flag = kFlagSuffixesLocked; break;
        case ItemFlagKind::Influenced: flag = kFlagInfluenced; break;
        case ItemFlagKind::EldritchImplicit:
            flag = kFlagEldritchImplicit;
            break;
        case ItemFlagKind::VeiledPrefix:
            return state.veiled_side == PC_SIDE_PREFIX;
        case ItemFlagKind::VeiledSuffix:
            return state.veiled_side == PC_SIDE_SUFFIX;
        }
        return (state.flags & flag) != 0;
    }
    case ConditionKind::InfluenceBits:
        return state.influence_bits == condition.min_value;
    case ConditionKind::EldritchTier: {
        const int tier = condition.eldritch_side == 0
                             ? state.searing_exarch_tier
                             : state.eater_of_worlds_tier;
        return tier >= condition.min_value && tier <= condition.max_value;
    }
    case ConditionKind::ModCount:
    case ConditionKind::ModFamilyCount: {
        const bool by_family =
            condition.kind == ConditionKind::ModFamilyCount;
        const std::vector<std::uint32_t>& ids =
            by_family ? condition.family_ids : condition.mod_ids;
        auto found = layout.count_observations.end();
        if (condition.count_memo_slot <
            layout.count_observation_by_memo_slot.size()) {
            const std::uint32_t observation =
                layout.count_observation_by_memo_slot[
                    condition.count_memo_slot];
            if (observation != kNoId) {
                found = layout.count_observations.begin() + observation;
            }
        }
        if (found == layout.count_observations.end()) {
            found = std::find_if(
                layout.count_observations.begin(),
                layout.count_observations.end(),
                [&](const CountObservation& observation) {
                    return observation.by_family == by_family &&
                           observation.ids == ids;
                });
        }
        if (found == layout.count_observations.end()) {
            throw std::logic_error(
                "compiled count condition missing from exact layout");
        }
        const std::size_t observation_index =
            static_cast<std::size_t>(
                std::distance(layout.count_observations.begin(), found));

        const bool fractured =
            (condition.required_flags & PC_MOD_SLOT_FRACTURED) != 0;
        const bool crafted =
            (condition.required_flags & PC_MOD_SLOT_CRAFTED) != 0;
        const CompactCountVector& junk_counts =
            fractured && crafted
                ? state.fractured_crafted_junk_counts
                : (fractured ? state.fractured_junk_counts
                             : (crafted ? state.crafted_junk_counts
                                        : state.junk_counts));
        int count = 0;
        for (const std::uint32_t junk : found->junk_class_indices) {
            count += junk_counts[junk];
        }
        for (std::size_t slot = 0; slot < layout.slots.size(); ++slot) {
            if (fractured &&
                (state.fractured_goal_mask & (1u << slot)) == 0) {
                continue;
            }
            if (crafted &&
                (state.crafted_goal_mask & (1u << slot)) == 0) {
                continue;
            }
            const std::uint8_t status = state.slot_status[slot];
            const ResolvedGoalSlot& goal_slot = layout.slots[slot];
            if (goal_slot.member_classes.empty()) {
                count += found->goal_status_counts[slot][status];
                continue;
            }
            if (status ==
                static_cast<std::uint8_t>(GoalSlotStatus::Absent)) {
                continue;
            }
            const std::uint32_t token =
                state.goal_member_class_tokens[slot];
            if (token == 0 || token > goal_slot.member_classes.size()) {
                throw std::logic_error(
                    "exact goal state has an invalid member-class token");
            }
            const GoalMemberClass& member_class =
                goal_slot.member_classes[token - 1];
            if (static_cast<std::uint8_t>(member_class.status) != status) {
                throw std::logic_error(
                    "exact goal member class disagrees with tier status");
            }
            const std::size_t word = observation_index / 64;
            if (word < member_class.count_observation_bits.size() &&
                (member_class.count_observation_bits[word] &
                 (std::uint64_t{1} << (observation_index % 64))) != 0) {
                ++count;
            }
        }
        return count >= condition.min_value &&
               count <= condition.max_value;
    }
    case ConditionKind::HasUnveilOption:
        return false; /* rejected during model derivation */
    case ConditionKind::ObservationSignature: {
        if (condition.observation_program == nullptr) {
            throw std::logic_error(
                "observation-signature condition has no program");
        }
        const refinement::CompiledObservationProgram& program =
            *condition.observation_program;
        const bool observes_goal_context =
            std::any_of(
                program.requirement.affix_observations.begin(),
                program.requirement.affix_observations.end(),
                [](const RefinementAffixObservation& observation) {
                    return (
                        observation.features &
                        refinement_feature(
                            RefinementFeature::
                                GoalStatusTierClass)) != 0;
                });
        if (observes_goal_context) {
            for (std::uint32_t mod = 0;
                 mod < session.mod_count; ++mod) {
                refinement::StableKey expected{
                    0u,
                    static_cast<std::uint8_t>(
                        GoalSlotStatus::Absent)};
                for (std::size_t slot = 0;
                     slot < layout.slots.size(); ++slot) {
                    if (!pc_bitset_test(
                            layout.slots[slot].member_mask.data(),
                            mod)) {
                        continue;
                    }
                    expected = {
                        slot + 1,
                        static_cast<std::uint8_t>(
                            pc_bitset_test(
                                layout.slots[slot]
                                    .satisfying_mask.data(),
                                mod)
                                ? GoalSlotStatus::Satisfied
                                : GoalSlotStatus::
                                      PresentBelowTier)};
                    break;
                }
                const refinement::StableKey actual =
                    mod <
                                program.context
                                    .goal_status_tier_class_by_mod
                                    .size() &&
                            !program.context
                                 .goal_status_tier_class_by_mod[mod]
                                 .empty()
                        ? program.context
                              .goal_status_tier_class_by_mod[mod]
                        : refinement::StableKey{
                              0u,
                              static_cast<std::uint8_t>(
                                  GoalSlotStatus::Absent)};
                if (actual != expected) {
                    throw std::logic_error(
                        "observation-signature goal context disagrees "
                        "with the exact layout");
                }
            }
        }
        const std::uint32_t observation_count =
            condition.observation_signature
                .count_observation_count;
        if (layout.count_observations.size() <
            observation_count) {
            throw std::logic_error(
                "observation-signature count context is absent "
                "from the exact layout");
        }
        for (std::uint32_t observation = 0;
             observation < observation_count;
             ++observation) {
            const CountObservation& layout_observation =
                layout.count_observations[observation];
            if (layout_observation.by_family) {
                throw std::logic_error(
                    "observation-signature count context changed "
                    "kind in the exact layout");
            }
            for (std::uint32_t mod = 0;
                 mod < session.mod_count; ++mod) {
                const refinement::StableKey& membership =
                    program.context
                        .count_observation_membership_by_mod[mod];
                const bool expected_member =
                    observation / 64 < membership.size() &&
                    (membership[observation / 64] &
                     (std::uint64_t{1} <<
                      (observation % 64))) != 0;
                const bool actual_member =
                    std::binary_search(
                        layout_observation.ids.begin(),
                        layout_observation.ids.end(),
                        mod);
                if (actual_member != expected_member) {
                    throw std::logic_error(
                        "observation-signature count context "
                        "disagrees with the exact layout");
                }
            }
        }
        refinement::AbstractFeatureExtraction extraction =
            refinement::extract_strict_abstract_features(
                session, layout, state, program.requirement);
        if (!extraction.complete()) return false;
        const std::size_t count_words =
            (observation_count + 63) / 64;
        for (refinement::FeatureAtom& atom :
             extraction.features) {
            if (atom.feature !=
                RefinementFeature::
                    CountObservationMembership) {
                continue;
            }
            atom.value.resize(count_words, 0);
            if (!atom.value.empty() &&
                observation_count % 64 != 0) {
                atom.value.back() &=
                    (std::uint64_t{1} <<
                     (observation_count % 64)) -
                    1;
            }
        }
        return refinement::observe_features(
                   refinement::canonical_feature_signature(
                       std::move(extraction.features)),
                   program.requirement) ==
               program.signature;
    }
    case ConditionKind::All:
        return std::all_of(
            condition.children.begin(), condition.children.end(),
            [&](const CompiledCondition& child) {
                return evaluate_abstract_condition(
                    child, session, layout, state);
            });
    case ConditionKind::Any:
        return std::any_of(
            condition.children.begin(), condition.children.end(),
            [&](const CompiledCondition& child) {
                return evaluate_abstract_condition(
                    child, session, layout, state);
            });
    case ConditionKind::Not:
        return !evaluate_abstract_condition(
            condition.children.front(), session, layout, state);
    case ConditionKind::AtLeast: {
        int matches = 0;
        for (const CompiledCondition& child : condition.children) {
            if (evaluate_abstract_condition(child, session, layout, state)) {
                ++matches;
            }
        }
        return matches >= condition.min_value;
    }
    }
    return false;
}

namespace {

bool bestiary_action_legal(
        const BestiaryActionDescriptor& action,
        const AbstractState& state,
        const std::uint32_t checkpoint_state) {
    const std::uint8_t rarity_bit =
        state.rarity < 8
            ? static_cast<std::uint8_t>(1u << state.rarity)
            : 0;
    if ((action.rarity_mask & rarity_bit) == 0) return false;
    if ((action.forbidden_item_flags & PC_ITEM_CORRUPTED) != 0 &&
        (state.flags & kFlagCorrupted) != 0) {
        return false;
    }
    if ((action.forbidden_item_flags & PC_ITEM_MIRRORED) != 0 &&
        (state.flags & kFlagMirrored) != 0) {
        return false;
    }
    const bool checkpoint_active = checkpoint_state != kNoId;
    if (action.checkpoint_requirement ==
            BestiaryCheckpointRequirement::Absent &&
        checkpoint_active) {
        return false;
    }
    if (action.checkpoint_requirement ==
            BestiaryCheckpointRequirement::Present &&
        !checkpoint_active) {
        return false;
    }
    /* Every evaluator checkpoint is created from the current live item.
     * Ordinary crafts keep item identity, while Restart clears the checkpoint
     * before changing it. Therefore an active checkpoint is necessarily bound
     * to this live item. */
    if (action.identity_requirement ==
            BestiaryIdentityRequirement::SameItem &&
        !checkpoint_active) {
        return false;
    }
    return true;
}

} // namespace

struct StrategyEvalWork::Impl {
    struct PolicyRouteResolution {
        std::uint32_t target_node = kNoId;
        std::uint32_t failure_node = kNoId;
        bool resolved = false;
    };

    struct FallbackState {
        std::uint32_t component = kNoId;
        std::vector<std::uint32_t> members;
        std::vector<std::uint32_t> local_index_by_pair;
        std::vector<double> wave;
        std::vector<double> visits;
        std::vector<double> incoming;
        std::vector<double> shadow;
        std::vector<double> direction;
        std::vector<double> image;
        std::vector<double> intermediate;
        std::vector<double> image_intermediate;
        std::vector<double> diagonal;
        double input_mass = 0.0;
        double rho_previous = 1.0;
        double alpha = 1.0;
        double omega = 1.0;
        std::uint32_t sweeps = 0;
    };

    std::shared_ptr<const StrategyImpl> strategy;
    StrategyEvalOptions options;
    EvalModel model;
    std::vector<ReviewSectionSpec> review_sections;
    StrategyEvalResult output;
    StrategyEvalPhase phase = StrategyEvalPhase::Discovery;

    std::map<
        std::tuple<
            std::uint32_t, std::uint32_t, std::uint32_t,
            std::uint32_t>,
        std::uint32_t>
        pair_by_key;
    std::vector<EvalPair> pairs;
    std::map<std::vector<std::uint32_t>, std::uint32_t>
        unveil_offer_by_mods;
    std::vector<std::vector<std::uint32_t>> unveil_offer_sets;
    std::vector<EvalRow> rows;
    /* The behavioral quotient solves value/flow at the minimum observable
     * carrier. Retain the pre-quotient graph until finalization solely to
     * disaggregate that flow back onto exact evaluator states and terminal
     * states; no representative state is authoritative for reporting. */
    std::vector<EvalPair> attribution_pairs;
    std::vector<EvalRow> attribution_rows;
    std::vector<std::uint32_t> attribution_class_by_pair;
    std::uint32_t attribution_start_pair = kNoId;
    std::uint64_t attribution_row_payload_owned_bytes = 0;
    std::map<
        std::tuple<
            std::uint32_t, std::uint32_t,
            const OutcomeDistribution*>,
        std::uint32_t> row_by_distribution;
    std::uint64_t stored_transitions = 0;
    std::uint64_t row_payload_owned_bytes = 0;
    std::uint64_t component_payload_owned_bytes = 0;
    std::uint64_t review_payload_owned_bytes = 0;
    std::uint64_t edge_index_owned_bytes = 0;
    std::uint64_t terminal_incoming_owned_bytes = 0;
    std::uint64_t compressed_policy_incoming_owned_bytes = 0;
    std::uint64_t observation_requirement_owned_bytes = 0;
    std::size_t discover_index = 0;
    std::uint32_t start_pair = kNoId;

    std::vector<std::vector<std::uint32_t>> components;
    std::vector<std::uint32_t> component_by_pair;
    std::size_t component_index = 0;
    std::vector<double> external_incoming;
    std::vector<double> pair_visits;
    std::vector<double> unresolved_pair;
    /* Deterministic pass-through contraction (contract_pass_through).
     * A contracted pair keeps its single outgoing transition in
     * chain_next/chain_edge; chain_inflow accumulates the mass that
     * entered it so its visits and edge traversal are settled during
     * finalization. */
    std::vector<std::uint8_t> pair_contracted;
    std::vector<std::uint32_t> chain_next;
    std::vector<std::uint32_t> chain_edge;
    std::vector<std::uint32_t> chain_policy_route;
    std::vector<std::uint32_t> chain_policy_state;
    std::vector<std::uint32_t> chain_terminal;
    std::vector<double> chain_inflow;
    std::unique_ptr<FallbackState> fallback;
    std::uint64_t fallback_sweeps = 0;
    bool hard_unresolved = false;

    std::vector<double> terminal_mass;
    std::vector<double> action_not_applied;
    std::vector<double> no_matching_edge;
    std::vector<std::map<std::uint32_t, double>> terminal_incoming;
    std::vector<std::map<std::uint32_t, double>>
        compressed_policy_incoming;
    std::map<std::string, std::uint32_t> edge_index_by_id;
    std::vector<double> edge_traversals;
    std::uint64_t peak_owned_bytes_value = 0;
    bool compress_policy_routes = false;
    std::uint32_t compressed_policy_root = kNoId;
    std::vector<PolicyRouteResolution> policy_route_cache;
    std::vector<ObservationRequirement>
        node_observation_requirements;

    bool is_policy_route_node(std::uint32_t node) const {
        return node < strategy->nodes.size() &&
               strategy->nodes[node].kind == StrategyNodeKind::Router &&
               strategy->nodes[node].id.rfind("policy_route_", 0) == 0;
    }

    static std::uint64_t string_bytes(const std::string& value) {
        return sizeof(std::string) + value.capacity() + 1;
    }

    static std::uint64_t string_vector_bytes(
        const std::vector<std::string>& values) {
        std::uint64_t bytes = values.capacity() * sizeof(std::string);
        for (const std::string& value : values) {
            bytes += value.capacity() + 1;
        }
        return bytes;
    }

    static std::uint64_t observation_requirement_nested_bytes(
        const ObservationRequirement& requirement) {
        std::uint64_t bytes =
            requirement.modifier_tag_ids.capacity() *
            sizeof(std::uint32_t);
        bytes += requirement.affix_observations.capacity() *
                 sizeof(RefinementAffixObservation);
        for (const RefinementAffixObservation& observation :
             requirement.affix_observations) {
            bytes +=
                observation.selector.required_tag_ids.capacity() *
                sizeof(std::uint32_t);
        }
        return bytes;
    }

    static std::uint64_t action_total_bytes(
        const StrategyEvalActionTotal& action) {
        std::uint64_t bytes = sizeof(action);
        bytes += action.id.capacity() + action.display_name.capacity() + 2;
        bytes += string_vector_bytes(action.price_keys);
        bytes += string_vector_bytes(action.classifications);
        bytes += action.nodes.capacity() * sizeof(StrategyEvalActionNode);
        for (const StrategyEvalActionNode& node : action.nodes) {
            bytes += node.node_id.capacity() + 1;
        }
        bytes += action.regions.capacity() *
                 sizeof(StrategyEvalActionRegion);
        return bytes;
    }

    std::uint64_t output_owned_bytes() const {
        std::uint64_t bytes = sizeof(output);
        bytes += output.economy_id.capacity() + 1;
        bytes += output.targets.capacity() * sizeof(GoalSlot);
        bytes += output.terminal_nodes.capacity() *
                 sizeof(StrategyEvalTerminalNode);
        for (const auto& node : output.terminal_nodes) {
            bytes += node.node_id.capacity() + 1;
        }
        bytes += output.unresolved_by_node.capacity() *
                 sizeof(StrategyEvalNodeMass);
        for (const auto& node : output.unresolved_by_node) {
            bytes += node.node_id.capacity() + 1;
        }
        bytes += output.failures_by_node.capacity() * sizeof(StrategyEvalFailure);
        for (const auto& failure : output.failures_by_node) {
            bytes += failure.node_id.capacity() + failure.reason.capacity() + 2;
        }
        bytes += output.nodes.capacity() * sizeof(StrategyEvalNode);
        for (const StrategyEvalNode& node : output.nodes) {
            bytes += node.id.capacity() + 1;
            bytes += node.classes.capacity() * sizeof(StrategyEvalClass);
        }
        bytes += output.edges.capacity() * sizeof(StrategyEvalEdge);
        for (const StrategyEvalEdge& edge : output.edges) {
            bytes += edge.id.capacity() + 1;
        }
        bytes += output.occupancy_states.capacity() * sizeof(AbstractState);
        bytes += output.occupancy.capacity() *
                 sizeof(StrategyEvalOccupancyEntry);
        bytes += output.action_totals.capacity() * sizeof(StrategyEvalActionTotal);
        for (const auto& action : output.action_totals) {
            bytes += action_total_bytes(action) - sizeof(action);
        }
        bytes += output.material_totals.capacity() *
                 sizeof(StrategyEvalMaterialTotal);
        for (const auto& material : output.material_totals) {
            bytes += material.price_key.capacity() + 1;
        }
        bytes += output.review_sections.capacity() *
                 sizeof(StrategyEvalReviewSection);
        for (const StrategyEvalReviewSection& section : output.review_sections) {
            bytes += section.id.capacity() + section.label.capacity() +
                     section.role.capacity() + 3;
            bytes += string_vector_bytes(section.raw_node_ids);
            bytes += string_vector_bytes(section.raw_edge_ids);
            bytes += section.actions.capacity() * sizeof(StrategyEvalActionTotal);
            for (const auto& action : section.actions) {
                bytes += action_total_bytes(action) - sizeof(action);
            }
            bytes += section.materials.capacity() *
                     sizeof(StrategyEvalMaterialTotal);
            for (const auto& material : section.materials) {
                bytes += material.price_key.capacity() + 1;
            }
            bytes += section.techniques.size() *
                     (sizeof(std::pair<const std::string, double>) +
                      3 * sizeof(void*));
            for (const auto& [key, unused] : section.techniques) {
                (void)unused;
                bytes += key.capacity() + 1;
            }
        }
        const auto add_string_map = [&](const auto& values) {
            std::uint64_t map_bytes = values.size() *
                (sizeof(typename std::decay_t<decltype(values)>::value_type) +
                 3 * sizeof(void*));
            for (const auto& [key, unused] : values) {
                (void)unused;
                map_bytes += key.capacity() + 1;
            }
            return map_bytes;
        };
        bytes += add_string_map(output.expected_consumption);
        bytes += add_string_map(output.technique_totals);
        bytes += add_string_map(output.material_quantity_differences);
        bytes += add_string_map(output.section_material_differences);
        return bytes;
    }

    std::uint64_t estimated_owned_bytes() const {
        std::uint64_t bytes = sizeof(*this);
        if (model.calc != nullptr) bytes += model.calc->estimated_owned_bytes();
        bytes += model.operation_by_node.capacity() *
                 sizeof(ResolvedStrategyOperation);
        bytes += model.action_by_node.capacity() * sizeof(std::uint32_t);
        bytes += model.targets.capacity() * sizeof(GoalSlot);
        bytes += review_sections.capacity() * sizeof(ReviewSectionSpec);
        for (const ReviewSectionSpec& section : review_sections) {
            bytes += section.id.capacity() + section.label.capacity() +
                     section.role.capacity() + 3;
            bytes += section.nodes.capacity() * sizeof(std::uint32_t);
            bytes += string_vector_bytes(section.edges);
        }
        bytes += pair_by_key.size() *
                 (sizeof(decltype(pair_by_key)::value_type) + 3 * sizeof(void*));
        bytes += pairs.capacity() * sizeof(EvalPair);
        bytes += node_observation_requirements.capacity() *
                 sizeof(ObservationRequirement);
        for (const ObservationRequirement& requirement :
             node_observation_requirements) {
            bytes += observation_requirement_nested_bytes(requirement);
        }
        bytes += unveil_offer_by_mods.size() *
                 (sizeof(decltype(unveil_offer_by_mods)::value_type) +
                  3 * sizeof(void*));
        for (const auto& [mods, unused] : unveil_offer_by_mods) {
            (void)unused;
            bytes += mods.capacity() * sizeof(std::uint32_t);
        }
        bytes += unveil_offer_sets.capacity() *
                 sizeof(std::vector<std::uint32_t>);
        for (const auto& mods : unveil_offer_sets) {
            bytes += mods.capacity() * sizeof(std::uint32_t);
        }
        bytes += rows.capacity() * sizeof(EvalRow);
        for (const EvalRow& row : rows) {
            bytes += row.transitions.capacity() * sizeof(EvalTransition);
            bytes += row.absorptions.capacity() * sizeof(EvalAbsorption);
        }
        bytes += attribution_pairs.capacity() * sizeof(EvalPair);
        bytes += attribution_rows.capacity() * sizeof(EvalRow);
        bytes += attribution_class_by_pair.capacity() *
                 sizeof(std::uint32_t);
        for (const EvalRow& row : attribution_rows) {
            bytes += row.transitions.capacity() * sizeof(EvalTransition);
            bytes += row.absorptions.capacity() * sizeof(EvalAbsorption);
        }
        bytes += row_by_distribution.size() *
                 (sizeof(decltype(row_by_distribution)::value_type) +
                  3 * sizeof(void*));
        bytes += components.capacity() * sizeof(std::vector<std::uint32_t>);
        for (const auto& component : components) {
            bytes += component.capacity() * sizeof(std::uint32_t);
        }
        bytes += component_by_pair.capacity() * sizeof(std::uint32_t);
        bytes += external_incoming.capacity() * sizeof(double);
        bytes += pair_visits.capacity() * sizeof(double);
        bytes += unresolved_pair.capacity() * sizeof(double);
        bytes += pair_contracted.capacity() * sizeof(std::uint8_t);
        bytes += chain_next.capacity() * sizeof(std::uint32_t);
        bytes += chain_edge.capacity() * sizeof(std::uint32_t);
        bytes += chain_policy_route.capacity() * sizeof(std::uint32_t);
        bytes += chain_policy_state.capacity() * sizeof(std::uint32_t);
        bytes += chain_terminal.capacity() * sizeof(std::uint32_t);
        bytes += chain_inflow.capacity() * sizeof(double);
        if (fallback != nullptr) {
            bytes += sizeof(FallbackState);
            bytes += fallback->members.capacity() * sizeof(std::uint32_t);
            bytes += fallback->local_index_by_pair.capacity() *
                     sizeof(std::uint32_t);
            bytes += fallback->wave.capacity() * sizeof(double);
            bytes += fallback->visits.capacity() * sizeof(double);
            bytes += fallback->incoming.capacity() * sizeof(double);
            bytes += fallback->shadow.capacity() * sizeof(double);
            bytes += fallback->direction.capacity() * sizeof(double);
            bytes += fallback->image.capacity() * sizeof(double);
            bytes += fallback->intermediate.capacity() * sizeof(double);
            bytes += fallback->image_intermediate.capacity() * sizeof(double);
            bytes += fallback->diagonal.capacity() * sizeof(double);
        }
        bytes += terminal_mass.capacity() * sizeof(double);
        bytes += action_not_applied.capacity() * sizeof(double);
        bytes += no_matching_edge.capacity() * sizeof(double);
        bytes += terminal_incoming.capacity() *
                 sizeof(std::map<std::uint32_t, double>);
        for (const auto& incoming : terminal_incoming) {
            bytes += incoming.size() *
                     (sizeof(std::decay_t<decltype(incoming)>::value_type) +
                      3 * sizeof(void*));
        }
        bytes += compressed_policy_incoming.capacity() *
                 sizeof(std::map<std::uint32_t, double>);
        for (const auto& incoming : compressed_policy_incoming) {
            bytes += incoming.size() *
                     (sizeof(std::decay_t<decltype(incoming)>::value_type) +
                      3 * sizeof(void*));
        }
        bytes += edge_index_by_id.size() *
                 (sizeof(decltype(edge_index_by_id)::value_type) +
                  3 * sizeof(void*));
        for (const auto& [id, unused] : edge_index_by_id) {
            (void)unused;
            bytes += id.capacity() + 1;
        }
        bytes += edge_traversals.capacity() * sizeof(double);
        bytes += policy_route_cache.capacity() *
                 sizeof(PolicyRouteResolution);
        bytes += output_owned_bytes();
        return bytes;
    }

    /* Constant-time selected-allocation estimate for per-work-item cap
     * enforcement. The full estimator remains the audit authority; these
     * counters mirror its nested-capacity terms at each mutation boundary. */
    std::uint64_t fast_estimated_owned_bytes() const {
        std::uint64_t bytes = sizeof(*this);
        if (model.calc != nullptr) {
            bytes += model.calc->fast_estimated_owned_bytes();
        }
        bytes += model.operation_by_node.capacity() *
                 sizeof(ResolvedStrategyOperation);
        bytes += model.action_by_node.capacity() * sizeof(std::uint32_t);
        bytes += model.targets.capacity() * sizeof(GoalSlot);
        bytes += review_sections.capacity() * sizeof(ReviewSectionSpec);
        bytes += review_payload_owned_bytes;
        bytes += pair_by_key.size() *
                 (sizeof(decltype(pair_by_key)::value_type) +
                  3 * sizeof(void*));
        bytes += pairs.capacity() * sizeof(EvalPair);
        bytes += node_observation_requirements.capacity() *
                 sizeof(ObservationRequirement);
        bytes += observation_requirement_owned_bytes;
        bytes += unveil_offer_by_mods.size() *
                 (sizeof(decltype(unveil_offer_by_mods)::value_type) +
                  3 * sizeof(void*));
        bytes += unveil_offer_sets.capacity() *
                 sizeof(std::vector<std::uint32_t>);
        for (const auto& [mods, unused] : unveil_offer_by_mods) {
            (void)unused;
            bytes += mods.capacity() * sizeof(std::uint32_t);
        }
        for (const auto& mods : unveil_offer_sets) {
            bytes += mods.capacity() * sizeof(std::uint32_t);
        }
        bytes += rows.capacity() * sizeof(EvalRow);
        bytes += row_payload_owned_bytes;
        bytes += attribution_pairs.capacity() * sizeof(EvalPair);
        bytes += attribution_rows.capacity() * sizeof(EvalRow);
        bytes += attribution_class_by_pair.capacity() *
                 sizeof(std::uint32_t);
        bytes += attribution_row_payload_owned_bytes;
        bytes += row_by_distribution.size() *
                 (sizeof(decltype(row_by_distribution)::value_type) +
                  3 * sizeof(void*));
        bytes += components.capacity() * sizeof(std::vector<std::uint32_t>);
        bytes += component_payload_owned_bytes;
        bytes += component_by_pair.capacity() * sizeof(std::uint32_t);
        bytes += external_incoming.capacity() * sizeof(double);
        bytes += pair_visits.capacity() * sizeof(double);
        bytes += unresolved_pair.capacity() * sizeof(double);
        bytes += pair_contracted.capacity() * sizeof(std::uint8_t);
        bytes += chain_next.capacity() * sizeof(std::uint32_t);
        bytes += chain_edge.capacity() * sizeof(std::uint32_t);
        bytes += chain_policy_route.capacity() * sizeof(std::uint32_t);
        bytes += chain_policy_state.capacity() * sizeof(std::uint32_t);
        bytes += chain_terminal.capacity() * sizeof(std::uint32_t);
        bytes += chain_inflow.capacity() * sizeof(double);
        if (fallback != nullptr) {
            bytes += sizeof(FallbackState);
            bytes += fallback->members.capacity() * sizeof(std::uint32_t);
            bytes += fallback->local_index_by_pair.capacity() *
                     sizeof(std::uint32_t);
            bytes += fallback->wave.capacity() * sizeof(double);
            bytes += fallback->visits.capacity() * sizeof(double);
            bytes += fallback->incoming.capacity() * sizeof(double);
            bytes += fallback->shadow.capacity() * sizeof(double);
            bytes += fallback->direction.capacity() * sizeof(double);
            bytes += fallback->image.capacity() * sizeof(double);
            bytes += fallback->intermediate.capacity() * sizeof(double);
            bytes += fallback->image_intermediate.capacity() * sizeof(double);
            bytes += fallback->diagonal.capacity() * sizeof(double);
        }
        bytes += terminal_mass.capacity() * sizeof(double);
        bytes += action_not_applied.capacity() * sizeof(double);
        bytes += no_matching_edge.capacity() * sizeof(double);
        bytes += terminal_incoming.capacity() *
                 sizeof(std::map<std::uint32_t, double>);
        bytes += terminal_incoming_owned_bytes;
        bytes += compressed_policy_incoming.capacity() *
                 sizeof(std::map<std::uint32_t, double>);
        bytes += compressed_policy_incoming_owned_bytes;
        bytes += edge_index_owned_bytes;
        bytes += edge_traversals.capacity() * sizeof(double);
        bytes += policy_route_cache.capacity() *
                 sizeof(PolicyRouteResolution);
        bytes += output_owned_bytes();
        return bytes;
    }

    void audit_owned_bytes() {
        const std::uint64_t fast = fast_estimated_owned_bytes();
        const std::uint64_t audited = estimated_owned_bytes();
        if (fast < audited) {
            throw std::logic_error(
                "strategy evaluation owned-byte ledger undercounted by " +
                std::to_string(audited - fast) + " bytes");
        }
        peak_owned_bytes_value = std::max(peak_owned_bytes_value, audited);
    }

    void check_owned_cap(std::uint64_t transient_bytes = 0) {
        const std::uint64_t owned = fast_estimated_owned_bytes();
        const std::uint64_t live =
            transient_bytes > std::numeric_limits<std::uint64_t>::max() - owned
                ? std::numeric_limits<std::uint64_t>::max()
                : owned + transient_bytes;
        peak_owned_bytes_value = std::max(peak_owned_bytes_value, live);
        output.owned_bytes_estimate = owned;
        output.peak_owned_bytes_estimate = peak_owned_bytes_value;
        if (live > options.max_owned_bytes) {
            throw std::length_error(
                "strategy evaluation exceeded max_owned_bytes (" +
                std::to_string(options.max_owned_bytes) + ")");
        }
    }

    void refresh_row_payload_owned_bytes() {
        row_payload_owned_bytes = 0;
        for (const EvalRow& row : rows) {
            row_payload_owned_bytes +=
                row.transitions.capacity() * sizeof(EvalTransition) +
                row.absorptions.capacity() * sizeof(EvalAbsorption);
        }
    }

    void add_terminal_incoming(
        std::uint32_t node, std::uint32_t state, double mass) {
        auto [entry, inserted] =
            terminal_incoming[node].try_emplace(state, 0.0);
        if (inserted) {
            terminal_incoming_owned_bytes +=
                sizeof(std::map<std::uint32_t, double>::value_type) +
                3 * sizeof(void*);
        }
        entry->second += mass;
    }

    void add_compressed_policy_incoming(
        std::uint32_t node, std::uint32_t state, double mass) {
        if (node == kNoId || !(mass > 0.0)) return;
        auto [entry, inserted] =
            compressed_policy_incoming[node].try_emplace(state, 0.0);
        if (inserted) {
            compressed_policy_incoming_owned_bytes +=
                sizeof(std::map<std::uint32_t, double>::value_type) +
                3 * sizeof(void*);
        }
        entry->second += mass;
    }

    void check_owned_projection(
        std::uint64_t owned,
        std::uint64_t transient_bytes) {
        const std::uint64_t live =
            transient_bytes > std::numeric_limits<std::uint64_t>::max() - owned
                ? std::numeric_limits<std::uint64_t>::max()
                : owned + transient_bytes;
        peak_owned_bytes_value = std::max(peak_owned_bytes_value, live);
        if (live > options.max_owned_bytes) {
            throw std::length_error(
                "strategy evaluation exceeded max_owned_bytes (" +
                std::to_string(options.max_owned_bytes) + ")");
        }
    }

    Impl(
        std::shared_ptr<const StrategyImpl> strategy_in,
        const StrategyEvalOptions& options_in)
        : strategy(std::move(strategy_in)),
          options(options_in),
          model(derive_checked_model(strategy, options_in.max_states)),
          review_sections(parse_review_sections(
              *strategy, options_in.review_projection_json)) {
        if (strategy == nullptr || strategy->session == nullptr ||
            strategy->start_node >= strategy->nodes.size()) {
            throw std::invalid_argument("invalid compiled strategy");
        }
        if (!std::isfinite(options.epsilon) || options.epsilon <= 0.0 ||
            options.max_sweeps == 0 || options.max_states == 0 ||
            options.max_pairs == 0 || options.max_transitions == 0 ||
            options.max_owned_bytes == 0 ||
            options.max_output_json_bytes == 0) {
            throw std::invalid_argument("invalid strategy evaluation options");
        }
        output.max_owned_bytes = options.max_owned_bytes;
        output.max_output_json_bytes = options.max_output_json_bytes;
        model.calc->set_solve_resource_caps(
            options.max_states,
            options.max_reforge_work,
            false,
            options.max_owned_bytes);
        output.targets = model.targets;
        for (const ReviewSectionSpec& section : review_sections) {
            review_payload_owned_bytes +=
                section.id.capacity() + section.label.capacity() +
                section.role.capacity() + 3;
            review_payload_owned_bytes +=
                section.nodes.capacity() * sizeof(std::uint32_t);
            review_payload_owned_bytes += string_vector_bytes(section.edges);
        }
        const std::size_t node_count = strategy->nodes.size();
        check_owned_cap();
        node_observation_requirements =
            derive_node_observation_requirements(
                *strategy, model, options.max_sweeps,
                [&](const std::uint64_t transient_bytes) {
                    check_owned_cap(transient_bytes);
                });
        for (const ObservationRequirement& requirement :
             node_observation_requirements) {
            observation_requirement_owned_bytes +=
                observation_requirement_nested_bytes(requirement);
        }
        std::vector<std::uint32_t> policy_roots;
        for (std::uint32_t source = 0; source < node_count; ++source) {
            if (is_policy_route_node(source)) continue;
            for (const StrategyEdge& edge : strategy->nodes[source].edges) {
                if (is_policy_route_node(edge.target) &&
                    std::find(
                        policy_roots.begin(), policy_roots.end(),
                        edge.target) == policy_roots.end()) {
                    policy_roots.push_back(edge.target);
                }
            }
        }
        /* A single external root means each state has one deterministic walk
         * through the compiler DAG. This permits exact online contraction and
         * bounded top-class selection without per-(router,state) graph rows. */
        compress_policy_routes = policy_roots.size() == 1;
        if (compress_policy_routes) {
            compressed_policy_root = policy_roots.front();
            policy_route_cache.reserve(options.max_states);
        }
        terminal_mass.assign(node_count, 0.0);
        action_not_applied.assign(node_count, 0.0);
        no_matching_edge.assign(node_count, 0.0);
        terminal_incoming.resize(node_count);
        compressed_policy_incoming.resize(node_count);
        for (const StrategyNode& node : strategy->nodes) {
            for (const StrategyEdge& edge : node.edges) {
                const std::uint32_t index =
                    static_cast<std::uint32_t>(edge_traversals.size());
                const auto [entry, inserted] =
                    edge_index_by_id.emplace(edge.id, index);
                if (!inserted) {
                    throw std::logic_error(
                        "compiled strategy contains a duplicate edge id");
                }
                edge_index_owned_bytes +=
                    sizeof(decltype(edge_index_by_id)::value_type) +
                    3 * sizeof(void*) + entry->first.capacity() + 1;
                edge_traversals.push_back(0.0);
            }
        }

        const std::uint32_t start_state =
            model.calc->intern_item(strategy->start_item);
        ensure_state_limit();
        if (strategy->nodes[strategy->start_node].kind ==
            StrategyNodeKind::Terminal) {
            terminal_mass[strategy->start_node] = 1.0;
            add_terminal_incoming(
                strategy->start_node, start_state, 1.0);
            phase = StrategyEvalPhase::Finalization;
        } else {
            start_pair = intern_pair(strategy->start_node, start_state);
        }
        check_owned_cap();
    }

    void ensure_state_limit() const {
        if (model.calc->state_count() > options.max_states) {
            throw std::length_error(
                "strategy evaluation exceeded max_states (" +
                std::to_string(options.max_states) + ")");
        }
    }

    std::uint32_t intern_unveil_offer(
        std::vector<std::uint32_t> mods) {
        std::sort(mods.begin(), mods.end());
        mods.erase(std::unique(mods.begin(), mods.end()), mods.end());
        const auto found = unveil_offer_by_mods.find(mods);
        if (found != unveil_offer_by_mods.end()) return found->second;
        const std::uint32_t id =
            static_cast<std::uint32_t>(unveil_offer_sets.size());
        unveil_offer_sets.push_back(mods);
        unveil_offer_by_mods.emplace(std::move(mods), id);
        return id;
    }

    std::uint32_t intern_pair(
        std::uint32_t node,
        std::uint32_t state,
        std::uint32_t unveil_offer = kNoId,
        std::uint32_t checkpoint_state = kNoId) {
        /*
         * This is deliberately the raw exact pair identity. Observation
         * equality is only a seed for the shared split-only fixed point after
         * the reachable graph is closed; it must never irreversibly select a
         * representative during discovery.
         */
        const auto key = std::make_tuple(
            node, state, unveil_offer, checkpoint_state);
        const auto found = pair_by_key.find(key);
        if (found != pair_by_key.end()) return found->second;
        const std::uint32_t id = static_cast<std::uint32_t>(pairs.size());
        pair_by_key.emplace(key, id);
        EvalPair pair;
        pair.node = node;
        pair.state = state;
        pair.checkpoint_state = checkpoint_state;
        pair.unveil_offer = unveil_offer;
        pairs.push_back(std::move(pair));
        return id;
    }

    const EvalRow& pair_row(std::uint32_t pair) const {
        return rows.at(pairs.at(pair).row);
    }

    void ensure_transition_budget(std::uint64_t additional) const {
        if (stored_transitions > options.max_transitions ||
            additional > options.max_transitions - stored_transitions) {
            throw std::length_error(
                "strategy evaluation exceeded max_transitions (" +
                std::to_string(options.max_transitions) + ")");
        }
    }

    const StrategyEdge* select_edge(
        const StrategyNode& node,
        std::uint32_t state,
        const std::vector<std::uint32_t>* offered_mods = nullptr) const {
        const std::function<bool(const CompiledCondition&)>
            evaluate_condition =
                [&](const CompiledCondition& condition) -> bool {
                    switch (condition.kind) {
                    case ConditionKind::HasUnveilOption:
                        return offered_mods != nullptr &&
                               std::any_of(
                                   condition.mod_ids.begin(),
                                   condition.mod_ids.end(),
                                   [&](const std::uint32_t mod) {
                                       return std::find(
                                                  offered_mods->begin(),
                                                  offered_mods->end(),
                                                  mod) != offered_mods->end();
                                   });
                    case ConditionKind::All:
                        return std::all_of(
                            condition.children.begin(),
                            condition.children.end(),
                            evaluate_condition);
                    case ConditionKind::Any:
                        return std::any_of(
                            condition.children.begin(),
                            condition.children.end(),
                            evaluate_condition);
                    case ConditionKind::Not:
                        return !evaluate_condition(
                            condition.children.front());
                    case ConditionKind::AtLeast:
                        return std::count_if(
                                   condition.children.begin(),
                                   condition.children.end(),
                                   evaluate_condition) >=
                               condition.min_value;
                    default:
                        return evaluate_abstract_condition(
                            condition, model.calc->session(),
                            model.calc->layout(),
                            model.calc->state(state));
                    }
                };
        const StrategyEdge* fallback_edge = nullptr;
        for (const StrategyEdge& edge : node.edges) {
            if (edge.is_default) {
                fallback_edge = &edge;
            } else if (evaluate_condition(edge.condition)) {
                return &edge;
            }
        }
        return fallback_edge;
    }

    bool node_observes_modifier_offer(const StrategyNode& node) const {
        const std::function<bool(const CompiledCondition&)> contains =
            [&](const CompiledCondition& condition) {
                return condition.kind == ConditionKind::HasUnveilOption ||
                       std::any_of(
                           condition.children.begin(),
                           condition.children.end(), contains);
            };
        return std::any_of(
            node.edges.begin(), node.edges.end(),
            [&](const StrategyEdge& edge) {
                return !edge.is_default && contains(edge.condition);
            });
    }

    std::uint32_t modifier_offer_action_index(
            const StrategyNode& node) const {
        std::uint32_t selected = kNoId;
        for (const StrategyEdge& edge : node.edges) {
            if (edge.is_default ||
                edge.target >= model.action_by_node.size()) {
                continue;
            }
            const std::uint32_t action =
                model.action_by_node[edge.target];
            if (action == kNoId ||
                !action_observes_modifier_offer(
                    model.calc->registry().actions[action])) {
                continue;
            }
            if (selected != kNoId && selected != action) {
                throw StrategyEvalUnsupported(
                    "strategy evaluation unsupported:\n- one modifier-offer "
                    "router selects multiple observation actions");
            }
            selected = action;
        }
        if (selected != kNoId) return selected;

        for (std::uint32_t action = 0;
             action < model.calc->registry().actions.size(); ++action) {
            if (!action_observes_modifier_offer(
                    model.calc->registry().actions[action])) {
                continue;
            }
            if (selected != kNoId) {
                return kNoId;
            }
            selected = action;
        }
        return selected;
    }

    void expand_pair(std::uint32_t pair_id) {
        const std::uint64_t owned_before_expansion =
            fast_estimated_owned_bytes();
        const std::uint32_t node_index = pairs.at(pair_id).node;
        const std::uint32_t state_id = pairs.at(pair_id).state;
        const std::uint32_t checkpoint_state_id =
            pairs.at(pair_id).checkpoint_state;
        const std::uint32_t unveil_offer_id =
            pairs.at(pair_id).unveil_offer;
        const std::vector<std::uint32_t>* active_unveil_offer =
            unveil_offer_id == kNoId
                ? nullptr
                : &unveil_offer_sets.at(unveil_offer_id);
        const StrategyNode& node = strategy->nodes.at(node_index);
        bool operation = false;
        bool consumes = false;
        std::uint32_t action_index = kNoId;
        const OutcomeDistribution* shared_distribution = nullptr;
        std::map<
            std::tuple<
                std::uint32_t, std::uint32_t, std::uint32_t,
                std::uint32_t>,
            double> transitions;
        std::map<
            std::tuple<
                int, std::uint32_t, std::uint32_t, std::uint32_t,
                std::uint32_t>,
            double> absorptions;

        const auto add_transition = [&](const std::tuple<
                                            std::uint32_t,
                                            std::uint32_t,
                                            std::uint32_t,
                                            std::uint32_t>& key,
                                        double probability) {
            auto found = transitions.find(key);
            if (found == transitions.end()) {
                ensure_transition_budget(
                    transitions.size() + absorptions.size() + 1);
                check_owned_projection(
                    owned_before_expansion,
                    (transitions.size() + absorptions.size() + 1) * 96ull);
                transitions.emplace(key, probability);
            } else {
                found->second += probability;
            }
        };
        const auto add_absorption = [&](const std::tuple<
                                            int,
                                            std::uint32_t,
                                            std::uint32_t,
                                            std::uint32_t,
                                            std::uint32_t>& key,
                                        double probability) {
            auto found = absorptions.find(key);
            if (found == absorptions.end()) {
                ensure_transition_budget(
                    transitions.size() + absorptions.size() + 1);
                check_owned_projection(
                    owned_before_expansion,
                    (transitions.size() + absorptions.size() + 1) * 112ull);
                absorptions.emplace(key, probability);
            } else {
                found->second += probability;
            }
        };

        const auto route = [&](
                               std::uint32_t state,
                               double probability,
                               const std::vector<std::uint32_t>*
                                   offered_mods,
                               std::uint32_t checkpoint_state) {
            if (!(probability > 0.0)) return;
            if (!std::isfinite(probability)) {
                throw std::runtime_error(
                    "strategy evaluation found a non-finite transition");
            }
            const StrategyEdge* selected =
                select_edge(node, state, offered_mods);
            if (selected == nullptr) {
                add_absorption(
                    {static_cast<int>(EvalAbsorptionKind::NoMatchingEdge),
                     node_index, state, kNoId, kNoId},
                    probability);
                return;
            }
            const std::uint32_t edge = edge_index_by_id.at(selected->id);
            std::uint32_t target_node = selected->target;
            std::uint32_t policy_route = kNoId;
            if (compress_policy_routes &&
                target_node == compressed_policy_root) {
                policy_route = target_node;
                if (policy_route_cache.size() <= state) {
                    policy_route_cache.resize(
                        static_cast<std::size_t>(state) + 1);
                }
                PolicyRouteResolution& resolution =
                    policy_route_cache[state];
                if (!resolution.resolved) {
                    std::uint32_t cursor = target_node;
                    std::size_t policy_steps = 0;
                    while (is_policy_route_node(cursor)) {
                        if (++policy_steps > strategy->nodes.size()) {
                            throw std::logic_error(
                                "compiled policy router contains a cycle");
                        }
                        const StrategyEdge* route_edge =
                            select_edge(strategy->nodes[cursor], state);
                        if (route_edge == nullptr) {
                            resolution.failure_node = cursor;
                            break;
                        }
                        cursor = route_edge->target;
                    }
                    resolution.target_node = cursor;
                    resolution.resolved = true;
                }
                if (resolution.failure_node != kNoId) {
                    add_absorption(
                        {static_cast<int>(
                             EvalAbsorptionKind::NoMatchingEdge),
                         resolution.failure_node, state, edge, policy_route},
                        probability);
                    return;
                }
                target_node = resolution.target_node;
            }
            const StrategyNode& target = strategy->nodes.at(target_node);
            if (target.kind == StrategyNodeKind::Terminal) {
                add_absorption(
                    {static_cast<int>(EvalAbsorptionKind::Terminal),
                     target_node, state, edge, policy_route},
                    probability);
                return;
            }
            const std::uint32_t target_unveil_offer =
                offered_mods == nullptr
                    ? kNoId
                    : intern_unveil_offer(*offered_mods);
            const std::uint32_t target_pair =
                intern_pair(
                    target_node, state, target_unveil_offer,
                    checkpoint_state);
            add_transition(
                {target_pair, edge, policy_route, state}, probability);
        };

        if (node.kind != StrategyNodeKind::Operation) {
            if (!node_observes_modifier_offer(node)) {
                route(
                    state_id, 1.0, active_unveil_offer,
                    checkpoint_state_id);
            } else if (active_unveil_offer != nullptr) {
                route(
                    state_id, 1.0, active_unveil_offer,
                    checkpoint_state_id);
            } else {
                const std::uint32_t observed_action =
                    modifier_offer_action_index(node);
                if (observed_action == kNoId) {
                    throw StrategyEvalUnsupported(
                        "strategy evaluation unsupported:\n- node '" +
                        node.id +
                        "' observes modifier offers but has no unique "
                        "admitted observation action");
                }
                const ActionDescriptor& action =
                    model.calc->registry().actions.at(observed_action);
                if (!action_legal(
                        model.calc->session(), action,
                        model.calc->state(state_id))) {
                    route(
                        state_id, 1.0, nullptr,
                        checkpoint_state_id);
                } else {
                    const OutcomeDistribution& outcomes =
                        model.calc->outcomes(state_id, observed_action);
                    if (!outcomes.supported) {
                        throw StrategyEvalUnsupported(
                            "strategy evaluation unsupported:\n- node '" +
                            node.id +
                            "' has no exact modifier-offer distribution for "
                            "a reachable state");
                    }
                    ensure_state_limit();
                    if (outcomes.choice_groups.empty()) {
                        route(
                            state_id, 1.0, nullptr,
                            checkpoint_state_id);
                    } else {
                        double distribution_mass = 0.0;
                        for (const OutcomeChoiceGroup& group :
                             outcomes.choice_groups) {
                            std::vector<std::uint32_t> offered_mods;
                            for (const OutcomeChoiceOption& option :
                                 outcomes.choice_options) {
                                if (std::find(
                                        group.states.begin(),
                                        group.states.end(),
                                        option.state) !=
                                    group.states.end()) {
                                    offered_mods.push_back(option.mod_id);
                                }
                            }
                            std::sort(
                                offered_mods.begin(), offered_mods.end());
                            offered_mods.erase(
                                std::unique(
                                    offered_mods.begin(),
                                    offered_mods.end()),
                                offered_mods.end());
                            distribution_mass += group.probability;
                            route(
                                state_id, group.probability,
                                &offered_mods,
                                checkpoint_state_id);
                        }
                        if (std::fabs(distribution_mass - 1.0) > 1e-9) {
                            throw std::runtime_error(
                                "strategy evaluation modifier-offer "
                                "distribution does not sum to one at node '" +
                                node.id + "'");
                        }
                    }
                }
            }
        } else {
            operation = true;
            const ResolvedStrategyOperation& resolved =
                model.operation_by_node.at(node_index);
            if (resolved.kind ==
                ResolvedStrategyOperationKind::Bestiary) {
                const BestiaryActionDescriptor& action =
                    model.calc->session().data->bestiary_actions.at(
                        resolved.descriptor_index);
                if (!bestiary_action_legal(
                        action, model.calc->state(state_id),
                        checkpoint_state_id)) {
                    add_absorption(
                        {static_cast<int>(
                             EvalAbsorptionKind::ActionNotApplied),
                         node_index, state_id, kNoId, kNoId},
                        1.0);
                } else {
                    consumes = true;
                    std::uint32_t successor_state = state_id;
                    std::uint32_t successor_checkpoint =
                        checkpoint_state_id;
                    if (action.checkpoint_effect ==
                        BestiaryCheckpointEffect::Create) {
                        successor_checkpoint = state_id;
                    } else {
                        successor_state = checkpoint_state_id;
                        successor_checkpoint = kNoId;
                    }
                    route(
                        successor_state, 1.0, nullptr,
                        successor_checkpoint);
                }
            } else {
                action_index = resolved.descriptor_index;
                const ActionDescriptor& action =
                    model.calc->registry().actions.at(action_index);
                if (!action_legal(
                        model.calc->session(), action,
                        model.calc->state(state_id))) {
                    add_absorption(
                        {static_cast<int>(
                             EvalAbsorptionKind::ActionNotApplied),
                         node_index, state_id, kNoId, kNoId},
                        1.0);
                } else {
                    consumes = true;
                    const OutcomeDistribution& outcomes =
                        model.calc->outcomes(state_id, action_index);
                    if (!outcomes.supported) {
                        throw StrategyEvalUnsupported(
                            "strategy evaluation unsupported:\n- node '" +
                            node.id + "' operation '" + action.id +
                            "' has no exact distribution for a reachable "
                            "state");
                    }
                    ensure_state_limit();
                    const auto shared = row_by_distribution.find(
                        {node_index, checkpoint_state_id, &outcomes});
                    if (shared != row_by_distribution.end()) {
                        EvalPair& pair = pairs.at(pair_id);
                        pair.operation = operation;
                        pair.consumes = consumes;
                        pair.action = action_index;
                        pair.row = shared->second;
                        return;
                    }
                    double distribution_mass = 0.0;
                    const std::uint32_t successor_checkpoint =
                        resolved.kind ==
                                ResolvedStrategyOperationKind::Restart
                            ? kNoId
                            : checkpoint_state_id;
                    if (action_observes_modifier_offer(action)) {
                        if (active_unveil_offer == nullptr ||
                            std::find(
                                active_unveil_offer->begin(),
                                active_unveil_offer->end(),
                                node.action.mod_id) ==
                                active_unveil_offer->end()) {
                            throw std::logic_error(
                                "authored modifier selection is not present "
                                "in the sampled offer carried to node '" +
                                node.id + "'");
                        }
                        const auto selected = std::find_if(
                            outcomes.choice_options.begin(),
                            outcomes.choice_options.end(),
                            [&](const OutcomeChoiceOption& option) {
                                return option.mod_id == node.action.mod_id;
                            });
                        if (selected == outcomes.choice_options.end()) {
                            throw std::logic_error(
                                "authored modifier selection is absent from "
                                "its reachable exact offer vocabulary at "
                                "node '" + node.id + "'");
                        }
                        const std::uint32_t successor =
                            selected->actual_state != kNoId
                                ? selected->actual_state
                                : selected->state;
                        distribution_mass = 1.0;
                        route(
                            successor, 1.0, nullptr,
                            successor_checkpoint);
                    } else {
                        shared_distribution = &outcomes;
                        for (const OutcomeEntry& outcome : outcomes.entries) {
                            distribution_mass += outcome.probability;
                            route(
                                outcome.state, outcome.probability,
                                nullptr, successor_checkpoint);
                        }
                    }
                    if (std::fabs(distribution_mass - 1.0) > 1e-9) {
                        throw std::runtime_error(
                            "strategy evaluation action distribution does "
                            "not sum to one at node '" + node.id + "'");
                    }
                }
            }
        }

        EvalPair& pair = pairs.at(pair_id);
        pair.operation = operation;
        pair.consumes = consumes;
        pair.action = action_index;
        EvalRow row;
        row.transitions.reserve(transitions.size());
        for (const auto& [key, probability] : transitions) {
            row.transitions.push_back(
                {std::get<0>(key), probability, std::get<1>(key), kNoId,
                 std::get<2>(key), std::get<3>(key)});
        }
        row.absorptions.reserve(absorptions.size());
        for (const auto& [key, probability] : absorptions) {
            row.absorptions.push_back(
                {static_cast<EvalAbsorptionKind>(std::get<0>(key)),
                  std::get<1>(key), std::get<2>(key), probability,
                  std::get<3>(key), std::get<4>(key)});
        }
        double row_mass = 0.0;
        for (const EvalTransition& transition : row.transitions) {
            row_mass += transition.probability;
        }
        for (const EvalAbsorption& absorption : row.absorptions) {
            row_mass += absorption.probability;
        }
        if (std::fabs(row_mass - 1.0) > 1e-9) {
            throw std::runtime_error(
                "strategy evaluation transition row does not sum to one at "
                "node '" + node.id + "'");
        }
        stored_transitions += row.transitions.size() + row.absorptions.size();
        row_payload_owned_bytes +=
            row.transitions.capacity() * sizeof(EvalTransition) +
            row.absorptions.capacity() * sizeof(EvalAbsorption);
        pair.row = static_cast<std::uint32_t>(rows.size());
        rows.push_back(std::move(row));
        if (shared_distribution != nullptr) {
            row_by_distribution.emplace(
                std::make_tuple(
                    node_index, checkpoint_state_id,
                    shared_distribution),
                pair.row);
        }
    }

    void append_unveil_offer(
        refinement::StableKey& key,
        const std::uint32_t offer) const {
        if (offer == kNoId) {
            key.push_back(0);
            return;
        }
        key.push_back(1);
        const std::vector<std::uint32_t>& mods =
            unveil_offer_sets.at(offer);
        key.push_back(static_cast<std::uint64_t>(mods.size()));
        key.insert(key.end(), mods.begin(), mods.end());
    }

    void append_compressed_policy_trace(
        refinement::StableKey& key,
        const std::uint32_t root,
        const std::uint32_t state) const {
        append_optional_u32(key, root);
        if (root == kNoId) return;
        if (state == kNoId) {
            throw std::logic_error(
                "compressed policy trace has no exact state");
        }
        std::uint32_t cursor = root;
        std::size_t steps = 0;
        while (is_policy_route_node(cursor)) {
            if (++steps > strategy->nodes.size()) {
                throw std::logic_error(
                    "compiled policy router contains a cycle");
            }
            const StrategyEdge* selected =
                select_edge(strategy->nodes[cursor], state);
            if (selected == nullptr) {
                key.push_back(0); /* no-matching-edge trace */
                key.push_back(cursor);
                return;
            }
            key.push_back(1); /* selected router edge */
            key.push_back(edge_index_by_id.at(selected->id));
            cursor = selected->target;
        }
        key.push_back(2); /* resolved non-router target */
        key.push_back(cursor);
    }

    refinement::StableKey raw_pair_stable_key(
        const EvalPair& pair) const {
        refinement::StableKey key{
            0x6576616c70616972ull, /* "evalpair" */
            pair.node};
        if (pair.state == kNoId ||
            pair.state >= model.calc->state_count()) {
            throw std::logic_error(
                "strategy evaluation pair has no exact semantic state");
        }
        append_stable_tokens(
            key,
            exact_abstract_state_key(
                model.calc->state(pair.state), kNoId));
        append_optional_u32(key, pair.checkpoint_state);
        if (pair.checkpoint_state != kNoId) {
            if (pair.checkpoint_state >= model.calc->state_count()) {
                throw std::logic_error(
                    "strategy evaluation pair has an invalid checkpoint "
                    "state");
            }
            append_stable_tokens(
                key,
                exact_abstract_state_key(
                    model.calc->state(pair.checkpoint_state), kNoId));
        }
        append_unveil_offer(key, pair.unveil_offer);
        return key;
    }

    refinement::StableKey pair_observation_key(
        const EvalPair& pair) const {
        const ObservationRequirement& requirement =
            node_observation_requirements.at(pair.node);
        const refinement::AbstractFeatureExtraction extraction =
            refinement::extract_strict_abstract_features(
                model.calc->session(),
                model.calc->layout(),
                model.calc->state(pair.state),
                requirement);
        if (!extraction.complete()) {
            throw StrategyEvalUnsupported(
                "strategy evaluation unsupported:\n- node '" +
                strategy->nodes.at(pair.node).id +
                "' requires an exact observation discarded by the "
                "evaluation carrier");
        }
        const refinement::FeatureSignature observed =
            refinement::observe_features(
                extraction.features, requirement);
        refinement::StableKey key{
            0x6576616c6f627331ull}; /* "evalobs1" */
        append_feature_signature(key, observed);
        return key;
    }

    refinement::StableKey pair_immediate_key(
        const EvalPair& pair) const {
        refinement::StableKey key{
            0x6576616c696d6d31ull, /* "evalimm1" */
            pair.node,
            pair.operation ? 1u : 0u,
            pair.consumes ? 1u : 0u};
        append_optional_u32(key, pair.action);
        append_unveil_offer(key, pair.unveil_offer);
        return key;
    }

    refinement::StableKey transition_partition_label(
        const EvalTransition& transition) const {
        refinement::StableKey key{
            0x6576616c74726e31ull}; /* "evaltrn1" */
        append_optional_u32(key, transition.edge);
        append_optional_u32(key, transition.via);
        append_unveil_offer(
            key, pairs.at(transition.target).unveil_offer);
        append_compressed_policy_trace(
            key, transition.policy_route,
            transition.policy_state);
        return key;
    }

    refinement::StableKey absorption_partition_label(
        const EvalAbsorption& absorption) const {
        refinement::StableKey key{
            0x6576616c61627331ull, /* "evalabs1" */
            static_cast<std::uint64_t>(absorption.kind),
            absorption.node};
        append_optional_u32(key, absorption.edge);
        if (absorption.kind == EvalAbsorptionKind::Terminal) {
            key.push_back(
                static_cast<std::uint64_t>(
                    strategy->nodes.at(absorption.node)
                        .terminal_kind));
        }
        append_compressed_policy_trace(
            key, absorption.policy_route, absorption.state);
        return key;
    }

    void refine_pair_graph() {
        using refinement::ClosedPartitionArc;
        using refinement::ClosedPartitionLimits;
        using refinement::ClosedPartitionNode;
        using refinement::ClosedPartitionResult;
        using refinement::ClosedPartitionStatus;

        if (pairs.empty()) return;
        output.raw_pairs_discovered =
            static_cast<std::uint32_t>(pairs.size());

        const auto saturated_add = [](
                const std::uint64_t left,
                const std::uint64_t right) {
            return right >
                           std::numeric_limits<std::uint64_t>::max() -
                               left
                       ? std::numeric_limits<std::uint64_t>::max()
                       : left + right;
        };
        const auto saturated_product = [](
                const std::size_t count,
                const std::size_t width) {
            return width != 0 &&
                           count >
                               std::numeric_limits<std::uint64_t>::max() /
                                   width
                       ? std::numeric_limits<std::uint64_t>::max()
                       : static_cast<std::uint64_t>(count) * width;
        };
        const auto stable_key_bytes =
            [&](const refinement::StableKey& key) {
                return saturated_product(
                    key.capacity(), sizeof(std::uint64_t));
            };

        /*
         * `closed` is transferred into the shared partitioner, while
         * `stable_keys` remains live in this caller for representative
         * selection. Track those two ownership domains separately so the
         * shared cap receives the exact caller-retained amount and does not
         * rely on a heuristic multiplier.
         */
        const std::uint64_t projected_closed_outer =
            saturated_product(
                pairs.size(), sizeof(ClosedPartitionNode));
        const std::uint64_t projected_stable_outer =
            saturated_product(
                pairs.size(), sizeof(refinement::StableKey));
        check_owned_cap(saturated_add(
            projected_closed_outer, projected_stable_outer));
        std::vector<ClosedPartitionNode> closed;
        closed.reserve(pairs.size());
        std::vector<refinement::StableKey> stable_keys;
        stable_keys.reserve(pairs.size());
        std::uint64_t closed_owned_bytes =
            saturated_product(
                closed.capacity(), sizeof(ClosedPartitionNode));
        std::uint64_t stable_keys_owned_bytes =
            saturated_product(
                stable_keys.capacity(), sizeof(refinement::StableKey));
        check_owned_cap(saturated_add(
            closed_owned_bytes, stable_keys_owned_bytes));
        for (std::uint32_t pair_id = 0;
             pair_id < pairs.size(); ++pair_id) {
            const EvalPair& pair = pairs[pair_id];
            ClosedPartitionNode node;
            node.stable_key = raw_pair_stable_key(pair);
            closed_owned_bytes = saturated_add(
                closed_owned_bytes,
                stable_key_bytes(node.stable_key));
            check_owned_cap(saturated_add(
                closed_owned_bytes, stable_keys_owned_bytes));
            node.observation_key = pair_observation_key(pair);
            closed_owned_bytes = saturated_add(
                closed_owned_bytes,
                stable_key_bytes(node.observation_key));
            check_owned_cap(saturated_add(
                closed_owned_bytes, stable_keys_owned_bytes));
            node.immediate_key = pair_immediate_key(pair);
            closed_owned_bytes = saturated_add(
                closed_owned_bytes,
                stable_key_bytes(node.immediate_key));
            check_owned_cap(saturated_add(
                closed_owned_bytes, stable_keys_owned_bytes));
            const EvalRow& row = pair_row(pair_id);
            const std::size_t arc_count =
                row.transitions.size() + row.absorptions.size();
            check_owned_cap(saturated_add(
                saturated_add(
                    closed_owned_bytes, stable_keys_owned_bytes),
                saturated_product(
                    arc_count, sizeof(ClosedPartitionArc))));
            node.arcs.reserve(
                arc_count);
            closed_owned_bytes = saturated_add(
                closed_owned_bytes,
                saturated_product(
                    node.arcs.capacity(),
                    sizeof(ClosedPartitionArc)));
            check_owned_cap(saturated_add(
                closed_owned_bytes, stable_keys_owned_bytes));
            for (const EvalTransition& transition :
                 row.transitions) {
                refinement::StableKey label =
                    transition_partition_label(transition);
                const std::uint64_t label_bytes =
                    stable_key_bytes(label);
                check_owned_cap(saturated_add(
                    saturated_add(
                        closed_owned_bytes,
                        stable_keys_owned_bytes),
                    label_bytes));
                node.arcs.push_back(ClosedPartitionArc{
                    std::move(label),
                    std::optional<std::uint32_t>{
                        transition.target},
                    transition.probability});
                closed_owned_bytes = saturated_add(
                    closed_owned_bytes,
                    stable_key_bytes(node.arcs.back().label));
                check_owned_cap(saturated_add(
                    closed_owned_bytes, stable_keys_owned_bytes));
            }
            for (const EvalAbsorption& absorption :
                 row.absorptions) {
                refinement::StableKey label =
                    absorption_partition_label(absorption);
                const std::uint64_t label_bytes =
                    stable_key_bytes(label);
                check_owned_cap(saturated_add(
                    saturated_add(
                        closed_owned_bytes,
                        stable_keys_owned_bytes),
                    label_bytes));
                node.arcs.push_back(ClosedPartitionArc{
                    std::move(label),
                    std::nullopt,
                    absorption.probability});
                closed_owned_bytes = saturated_add(
                    closed_owned_bytes,
                    stable_key_bytes(node.arcs.back().label));
                check_owned_cap(saturated_add(
                    closed_owned_bytes, stable_keys_owned_bytes));
            }
            check_owned_cap(saturated_add(
                saturated_add(
                    closed_owned_bytes, stable_keys_owned_bytes),
                saturated_product(
                    node.stable_key.size(),
                    sizeof(std::uint64_t))));
            stable_keys.push_back(node.stable_key);
            stable_keys_owned_bytes = saturated_add(
                stable_keys_owned_bytes,
                stable_key_bytes(stable_keys.back()));
            closed.push_back(std::move(node));
            check_owned_cap(saturated_add(
                closed_owned_bytes, stable_keys_owned_bytes));
        }

        ClosedPartitionLimits limits;
        limits.max_classes = options.max_pairs;
        limits.max_rounds = options.max_pairs;
        limits.retained_estimated_memory_bytes =
            saturated_add(
                fast_estimated_owned_bytes(),
                stable_keys_owned_bytes);
        limits.max_estimated_memory_bytes =
            options.max_owned_bytes;
        limits.probability_sum_tolerance = 1e-9;
        ClosedPartitionResult refined =
            refinement::refine_closed_probabilistic_partition(
                std::move(closed), limits);
        peak_owned_bytes_value = std::max(
            peak_owned_bytes_value,
            refined.peak_estimated_memory_bytes);
        output.peak_owned_bytes_estimate =
            peak_owned_bytes_value;
        if (refined.status != ClosedPartitionStatus::Complete ||
            !refined.lumpable) {
            if (refined.status ==
                    ClosedPartitionStatus::ResourceCap &&
                refined.resource_cap == "max_classes") {
                throw std::length_error(
                    "strategy evaluation exceeded max_pairs (" +
                    std::to_string(options.max_pairs) + ")");
            }
            if (refined.status ==
                    ClosedPartitionStatus::ResourceCap &&
                refined.resource_cap ==
                    "max_estimated_memory_bytes") {
                throw std::length_error(
                    "strategy evaluation exceeded max_owned_bytes (" +
                    std::to_string(options.max_owned_bytes) + ")");
            }
            throw std::runtime_error(
                refined.failure_reason.empty()
                    ? "strategy evaluation pair refinement failed"
                    : "strategy evaluation pair refinement failed: " +
                          refined.failure_reason);
        }
        if (refined.final_class_count > options.max_pairs) {
            throw std::length_error(
                "strategy evaluation exceeded max_pairs (" +
                std::to_string(options.max_pairs) + ")");
        }
        output.refined_pairs = refined.final_class_count;
        output.pair_refinement_rounds = refined.rounds;
        output.pair_lumpability_checks =
            refined.lumpability_checks;

        /* A singleton partition already retains exact attribution in the
         * ordinary evaluator graph. Avoid duplicating and re-solving it; the
         * secondary attribution graph exists only when quotienting actually
         * merges concrete evaluator pairs. */
        if (refined.final_class_count == pairs.size()) {
            check_owned_cap();
            return;
        }

        if (refined.estimated_memory_bytes <
            limits.retained_estimated_memory_bytes) {
            throw std::logic_error(
                "strategy evaluation partition memory ledger regressed");
        }
        const std::uint64_t refined_result_owned_bytes =
            refined.estimated_memory_bytes -
            limits.retained_estimated_memory_bytes;
        const std::uint64_t projected_representative_bytes =
            saturated_product(
                refined.final_class_count,
                sizeof(std::uint32_t));
        check_owned_cap(saturated_add(
            saturated_add(
                stable_keys_owned_bytes,
                refined_result_owned_bytes),
            projected_representative_bytes));
        std::vector<std::uint32_t> representative(
            refined.final_class_count, kNoId);
        check_owned_cap(saturated_add(
            saturated_add(
                stable_keys_owned_bytes,
                refined_result_owned_bytes),
            saturated_product(
                representative.capacity(),
                sizeof(std::uint32_t))));
        for (std::uint32_t raw = 0; raw < pairs.size(); ++raw) {
            const std::uint32_t class_id =
                refined.class_by_node.at(raw);
            std::uint32_t& selected = representative.at(class_id);
            if (selected == kNoId ||
                stable_keys[raw] < stable_keys[selected]) {
                selected = raw;
            }
        }

        const std::uint32_t refined_class_count =
            refined.final_class_count;
        std::vector<std::uint32_t> class_by_node =
            std::move(refined.class_by_node);
        refined = ClosedPartitionResult{};
        std::vector<refinement::StableKey>().swap(stable_keys);
        const auto conversion_local_bytes = [&]() {
            return saturated_add(
                saturated_product(
                    representative.capacity(),
                    sizeof(std::uint32_t)),
                saturated_product(
                    class_by_node.capacity(),
                    sizeof(std::uint32_t)));
        };
        check_owned_cap(conversion_local_bytes());

        {
            std::vector<EvalPair> raw_pairs = std::move(pairs);
            std::vector<EvalRow> raw_rows = std::move(rows);
            attribution_start_pair = start_pair;
            stored_transitions = 0;
            row_payload_owned_bytes = 0;
            std::uint64_t raw_graph_bytes =
                saturated_add(
                    saturated_product(
                        raw_pairs.capacity(),
                        sizeof(EvalPair)),
                    saturated_product(
                        raw_rows.capacity(),
                        sizeof(EvalRow)));
            for (const EvalRow& row : raw_rows) {
                raw_graph_bytes = saturated_add(
                    raw_graph_bytes,
                    saturated_add(
                        saturated_product(
                            row.transitions.capacity(),
                            sizeof(EvalTransition)),
                        saturated_product(
                            row.absorptions.capacity(),
                            sizeof(EvalAbsorption))));
            }
            const auto check_conversion =
                [&](const std::uint64_t scratch = 0) {
                    check_owned_cap(saturated_add(
                        saturated_add(
                            raw_graph_bytes,
                            conversion_local_bytes()),
                        scratch));
                };
            check_conversion();
            check_conversion(saturated_add(
                saturated_product(
                    refined_class_count, sizeof(EvalPair)),
                saturated_product(
                    refined_class_count, sizeof(EvalRow))));
            pairs.assign(refined_class_count, EvalPair{});
            rows.clear();
            rows.reserve(refined_class_count);
            check_conversion();

            using TransitionKey = std::tuple<
                std::uint32_t, std::uint32_t, std::uint32_t,
                std::uint32_t, std::uint32_t>;
            using AbsorptionKey = std::tuple<
                int, std::uint32_t, std::uint32_t,
                std::uint32_t, std::uint32_t>;
            using TransitionMap =
                std::map<TransitionKey, double>;
            using AbsorptionMap =
                std::map<AbsorptionKey, double>;
            const std::uint64_t transition_map_node_bytes =
                sizeof(TransitionMap::value_type) +
                3 * sizeof(void*);
            const std::uint64_t absorption_map_node_bytes =
                sizeof(AbsorptionMap::value_type) +
                3 * sizeof(void*);

            for (std::uint32_t class_id = 0;
                 class_id < refined_class_count; ++class_id) {
                const std::uint32_t raw =
                    representative.at(class_id);
                if (raw == kNoId) {
                    throw std::logic_error(
                        "strategy evaluation refinement produced an empty "
                        "pair class");
                }
                EvalPair pair = raw_pairs.at(raw);
                const EvalRow& source =
                    raw_rows.at(pair.row);
                TransitionMap transitions;
                AbsorptionMap absorptions;
                const auto map_bytes = [&]() {
                    return saturated_add(
                        saturated_product(
                            transitions.size(),
                            transition_map_node_bytes),
                        saturated_product(
                            absorptions.size(),
                            absorption_map_node_bytes));
                };
                for (const EvalTransition& transition :
                     source.transitions) {
                    const TransitionKey key{
                        class_by_node.at(transition.target),
                        transition.edge,
                        transition.via,
                        transition.policy_route,
                        transition.policy_state};
                    const auto found = transitions.find(key);
                    if (found == transitions.end()) {
                        check_conversion(saturated_add(
                            map_bytes(),
                            transition_map_node_bytes));
                        transitions.emplace(
                            key, transition.probability);
                    } else {
                        found->second += transition.probability;
                    }
                }
                for (const EvalAbsorption& absorption :
                     source.absorptions) {
                    const AbsorptionKey key{
                        static_cast<int>(absorption.kind),
                        absorption.node,
                        absorption.state,
                        absorption.edge,
                        absorption.policy_route};
                    const auto found = absorptions.find(key);
                    if (found == absorptions.end()) {
                        check_conversion(saturated_add(
                            map_bytes(),
                            absorption_map_node_bytes));
                        absorptions.emplace(
                            key, absorption.probability);
                    } else {
                        found->second += absorption.probability;
                    }
                }

                const std::uint64_t projected_row_bytes =
                    saturated_add(
                        saturated_product(
                            transitions.size(),
                            sizeof(EvalTransition)),
                        saturated_product(
                            absorptions.size(),
                            sizeof(EvalAbsorption)));
                check_conversion(saturated_add(
                    map_bytes(), projected_row_bytes));
                EvalRow row;
                row.transitions.reserve(transitions.size());
                row.absorptions.reserve(absorptions.size());
                const auto row_bytes = [&]() {
                    return saturated_add(
                        saturated_product(
                            row.transitions.capacity(),
                            sizeof(EvalTransition)),
                        saturated_product(
                            row.absorptions.capacity(),
                            sizeof(EvalAbsorption)));
                };
                check_conversion(saturated_add(
                    map_bytes(), row_bytes()));
                for (const auto& [key, probability] : transitions) {
                    row.transitions.push_back({
                        std::get<0>(key),
                        probability,
                        std::get<1>(key),
                        std::get<2>(key),
                        std::get<3>(key),
                        std::get<4>(key)});
                }
                for (const auto& [key, probability] : absorptions) {
                    row.absorptions.push_back({
                        static_cast<EvalAbsorptionKind>(
                            std::get<0>(key)),
                        std::get<1>(key),
                        std::get<2>(key),
                        probability,
                        std::get<3>(key),
                        std::get<4>(key)});
                }
                check_conversion(saturated_add(
                    map_bytes(), row_bytes()));
                pair.row =
                    static_cast<std::uint32_t>(rows.size());
                pairs[class_id] = std::move(pair);
                stored_transitions +=
                    row.transitions.size() +
                    row.absorptions.size();
                const std::uint64_t retained_row_bytes =
                    row_bytes();
                rows.push_back(std::move(row));
                row_payload_owned_bytes = saturated_add(
                    row_payload_owned_bytes,
                    retained_row_bytes);
                check_conversion(map_bytes());
            }

            if (start_pair != kNoId) {
                start_pair = class_by_node.at(start_pair);
            }
            discover_index = pairs.size();
            pair_by_key.clear();
            row_by_distribution.clear();
            attribution_pairs = std::move(raw_pairs);
            attribution_rows = std::move(raw_rows);
            attribution_class_by_pair = std::move(class_by_node);
            attribution_row_payload_owned_bytes = 0;
            for (const EvalRow& row : attribution_rows) {
                attribution_row_payload_owned_bytes = capped_add(
                    attribution_row_payload_owned_bytes,
                    capped_add(
                        capped_product(
                            row.transitions.capacity(),
                            sizeof(EvalTransition)),
                        capped_product(
                            row.absorptions.capacity(),
                            sizeof(EvalAbsorption))));
            }
            check_owned_cap();
        }
        check_owned_cap();
    }

    /* Fold deterministic pass-through pairs — exactly one outgoing
     * transition with probability exactly 1 and no absorptions — out of
     * the transition relation before components are built. Every
     * transition entering such a pair is redirected to the first
     * non-pass-through pair down its chain, so hub-and-spoke loops
     * (reforge ↔ deterministic scour, solver router/restart graphs)
     * become self-loops the closed-form solvers handle instead of
     * multi-thousand-member fallback components. The rewrite is exact:
     * probabilities are only regrouped, never truncated, and the folded
     * pairs' visits and edge traversals are settled from chain_inflow at
     * finalization. Pairs on a purely deterministic cycle are left in
     * place so recurrent classes keep their unresolved treatment. */
    void contract_pass_through() {
        const std::size_t count = pairs.size();
        pair_contracted.assign(count, 0);
        chain_next.assign(count, kNoId);
        chain_edge.assign(count, kNoId);
        chain_policy_route.assign(count, kNoId);
        chain_policy_state.assign(count, kNoId);
        chain_terminal.assign(count, kNoId);
        chain_inflow.assign(count, 0.0);
        if (count == 0) return;

        std::vector<std::uint8_t> pass(count, 0);
        for (std::uint32_t pair = 0; pair < count; ++pair) {
            const EvalRow& row = pair_row(pair);
            if (row.absorptions.empty() && row.transitions.size() == 1 &&
                row.transitions.front().probability == 1.0) {
                pass[pair] = 1;
                chain_next[pair] = row.transitions.front().target;
                chain_edge[pair] = row.transitions.front().edge;
                chain_policy_route[pair] =
                    row.transitions.front().policy_route;
                chain_policy_state[pair] =
                    row.transitions.front().policy_state;
            }
        }

        /* Resolve each pair's forward target: itself when it is not a
         * pass-through, otherwise the end of its deterministic chain.
         * state: 0 unvisited, 1 on the current walk, 2 resolved. */
        std::vector<std::uint8_t> state(count, 0);
        std::vector<std::uint32_t> forward(count, kNoId);
        std::vector<std::uint32_t> path;
        bool any_contracted = false;
        for (std::uint32_t root = 0; root < count; ++root) {
            if (state[root] == 2) continue;
            path.clear();
            std::uint32_t cursor = root;
            while (state[cursor] != 2) {
                if (!pass[cursor]) {
                    state[cursor] = 2;
                    forward[cursor] = cursor;
                    break;
                }
                if (state[cursor] == 1) {
                    /* The walk re-entered itself: cursor..path.back()
                     * form a deterministic cycle. Keep those pairs. */
                    std::size_t cycle = path.size();
                    while (path[cycle - 1] != cursor) --cycle;
                    --cycle;
                    for (std::size_t i = cycle; i < path.size(); ++i) {
                        state[path[i]] = 2;
                        forward[path[i]] = path[i];
                        pass[path[i]] = 0;
                    }
                    path.resize(cycle);
                    break;
                }
                state[cursor] = 1;
                path.push_back(cursor);
                cursor = chain_next[cursor];
            }
            for (std::size_t i = path.size(); i-- > 0;) {
                const std::uint32_t pair = path[i];
                state[pair] = 2;
                forward[pair] = forward[chain_next[pair]];
                chain_terminal[pair] = forward[pair];
                pair_contracted[pair] = 1;
                any_contracted = true;
            }
        }
        if (!any_contracted) return;

        std::uint64_t remaining_transitions = 0;
        for (EvalRow& row : rows) {
            bool touched = false;
            for (const EvalTransition& transition : row.transitions) {
                if (pair_contracted[transition.target]) {
                    touched = true;
                    break;
                }
            }
            if (touched) {
                std::map<
                    std::tuple<
                        std::uint32_t, std::uint32_t, std::uint32_t,
                        std::uint32_t, std::uint32_t>,
                    double> merged;
                for (const EvalTransition& transition : row.transitions) {
                    const std::uint32_t via =
                        pair_contracted[transition.target]
                            ? transition.target
                            : kNoId;
                    const std::uint32_t target =
                        via == kNoId ? transition.target
                                     : forward[transition.target];
                    merged[{target, transition.edge, via,
                            transition.policy_route,
                            transition.policy_state}] +=
                        transition.probability;
                }
                row.transitions.clear();
                row.transitions.reserve(merged.size());
                for (const auto& [key, probability] : merged) {
                    row.transitions.push_back(
                        {std::get<0>(key), probability, std::get<1>(key),
                         std::get<2>(key), std::get<3>(key),
                         std::get<4>(key)});
                }
            }
            remaining_transitions +=
                row.transitions.size() + row.absorptions.size();
        }
        stored_transitions = remaining_transitions;
        refresh_row_payload_owned_bytes();
    }

    /* Settle the mass that flowed through contracted pairs: each visit
     * of a folded chain and its single edge, in chain order. Runs once,
     * at finalization, after every component (or the reference sweep)
     * has committed its flows into chain_inflow. */
    void propagate_chain_inflow() {
        const std::size_t count = pair_contracted.size();
        std::vector<std::uint32_t> indegree(count, 0);
        for (std::uint32_t pair = 0; pair < count; ++pair) {
            if (!pair_contracted[pair]) continue;
            const std::uint32_t next = chain_next[pair];
            if (pair_contracted[next]) ++indegree[next];
        }
        std::vector<std::uint32_t> ready;
        for (std::uint32_t pair = 0; pair < count; ++pair) {
            if (pair_contracted[pair] && indegree[pair] == 0) {
                ready.push_back(pair);
            }
        }
        while (!ready.empty()) {
            const std::uint32_t pair = ready.back();
            ready.pop_back();
            const double inflow = chain_inflow[pair];
            pair_visits[pair] += inflow;
            if (chain_edge[pair] != kNoId) {
                edge_traversals.at(chain_edge[pair]) += inflow;
            }
            add_compressed_policy_incoming(
                chain_policy_route[pair], chain_policy_state[pair], inflow);
            const std::uint32_t next = chain_next[pair];
            if (pair_contracted[next]) {
                chain_inflow[next] += inflow;
                if (--indegree[next] == 0) ready.push_back(next);
            }
        }
    }

    void build_components() {
        refine_pair_graph();
        contract_pass_through();
        const std::size_t count = pairs.size();
        struct Frame {
            std::uint32_t pair = kNoId;
            std::size_t next_transition = 0;
        };
        std::vector<std::uint32_t> index(count, kNoId);
        std::vector<std::uint32_t> lowlink(count, kNoId);
        std::vector<std::uint8_t> on_stack(count, 0);
        std::vector<std::uint32_t> tarjan_stack;
        tarjan_stack.reserve(count);
        std::vector<std::vector<std::uint32_t>> raw_components;
        std::uint32_t next_index = 0;

        const auto push_pair = [&](std::uint32_t pair,
                                   std::vector<Frame>& dfs) {
            index[pair] = next_index;
            lowlink[pair] = next_index;
            ++next_index;
            tarjan_stack.push_back(pair);
            on_stack[pair] = 1;
            dfs.push_back({pair, 0});
        };
        for (std::uint32_t root = 0; root < count; ++root) {
            if (pair_contracted[root] || index[root] != kNoId) continue;
            std::vector<Frame> dfs;
            push_pair(root, dfs);
            while (!dfs.empty()) {
                Frame& frame = dfs.back();
                const auto& transitions = pair_row(frame.pair).transitions;
                if (frame.next_transition < transitions.size()) {
                    const std::uint32_t target =
                        transitions[frame.next_transition++].target;
                    if (index[target] == kNoId) {
                        push_pair(target, dfs);
                    } else if (on_stack[target]) {
                        lowlink[frame.pair] = std::min(
                            lowlink[frame.pair], index[target]);
                    }
                    continue;
                }

                const std::uint32_t completed = frame.pair;
                dfs.pop_back();
                if (!dfs.empty()) {
                    const std::uint32_t parent = dfs.back().pair;
                    lowlink[parent] = std::min(
                        lowlink[parent], lowlink[completed]);
                }
                if (lowlink[completed] == index[completed]) {
                    raw_components.emplace_back();
                    while (true) {
                        const std::uint32_t member = tarjan_stack.back();
                        tarjan_stack.pop_back();
                        on_stack[member] = 0;
                        raw_components.back().push_back(member);
                        if (member == completed) break;
                    }
                    std::sort(
                        raw_components.back().begin(),
                        raw_components.back().end());
                }
            }
        }

        /* Tarjan emits sink components first. Reverse that order so forward
         * mass reaches every component before it is solved, without copying
         * the (potentially dense) edge relation into adjacency lists. */
        components.clear();
        components.reserve(raw_components.size());
        for (auto it = raw_components.rbegin();
             it != raw_components.rend(); ++it) {
            components.push_back(std::move(*it));
        }
        component_by_pair.assign(count, kNoId);
        for (std::uint32_t component = 0;
             component < components.size(); ++component) {
            for (const std::uint32_t pair : components[component]) {
                component_by_pair[pair] = component;
            }
        }
        component_payload_owned_bytes = 0;
        for (const auto& component : components) {
            component_payload_owned_bytes +=
                component.capacity() * sizeof(std::uint32_t);
        }

        external_incoming.assign(count, 0.0);
        pair_visits.assign(count, 0.0);
        unresolved_pair.assign(count, 0.0);
        if (start_pair != kNoId) {
            if (pair_contracted[start_pair]) {
                chain_inflow[start_pair] = 1.0;
                external_incoming.at(chain_terminal[start_pair]) = 1.0;
            } else {
                external_incoming[start_pair] = 1.0;
            }
        }
        component_index = 0;
    }

    bool component_has_exit(
        std::uint32_t component,
        const std::vector<std::uint32_t>& members) const {
        for (const std::uint32_t pair : members) {
            const EvalRow& row = pair_row(pair);
            if (!row.absorptions.empty()) return true;
            for (const EvalTransition& transition : row.transitions) {
                if (component_by_pair[transition.target] != component &&
                    transition.probability > 0.0) {
                    return true;
                }
            }
        }
        return false;
    }

    bool dense_solve(
        std::uint32_t component,
        const std::vector<std::uint32_t>& members,
        const std::vector<double>& incoming,
        std::vector<double>& visits) const {
        const std::size_t n = members.size();
        if (n == std::numeric_limits<std::size_t>::max()) return false;
        std::map<std::uint32_t, std::size_t> local;
        for (std::size_t i = 0; i < n; ++i) local[members[i]] = i;
        std::vector<std::vector<long double>> matrix(
            n, std::vector<long double>(n + 1, 0.0L));
        for (std::size_t row = 0; row < n; ++row) {
            matrix[row][row] = 1.0L;
            matrix[row][n] = incoming[row];
        }
        for (std::size_t source = 0; source < n; ++source) {
            for (const EvalTransition& transition :
                 pair_row(members[source]).transitions) {
                if (component_by_pair[transition.target] != component) continue;
                matrix[local.at(transition.target)][source] -=
                    static_cast<long double>(transition.probability);
            }
        }

        long double matrix_scale = 0.0L;
        for (const auto& row : matrix) {
            for (std::size_t col = 0; col < n; ++col) {
                matrix_scale = std::max(matrix_scale, std::fabs(row[col]));
            }
        }
        long double min_pivot = std::numeric_limits<long double>::infinity();
        long double max_pivot = 0.0L;
        for (std::size_t col = 0; col < n; ++col) {
            std::size_t pivot = col;
            long double pivot_abs = std::fabs(matrix[col][col]);
            for (std::size_t row = col + 1; row < n; ++row) {
                const long double candidate = std::fabs(matrix[row][col]);
                if (candidate > pivot_abs) {
                    pivot = row;
                    pivot_abs = candidate;
                }
            }
            if (pivot_abs <= std::max(1e-18L, matrix_scale * 1e-14L)) {
                return false;
            }
            if (pivot != col) std::swap(matrix[pivot], matrix[col]);
            min_pivot = std::min(min_pivot, pivot_abs);
            max_pivot = std::max(max_pivot, pivot_abs);
            for (std::size_t row = col + 1; row < n; ++row) {
                const long double factor = matrix[row][col] / matrix[col][col];
                if (factor == 0.0L) continue;
                for (std::size_t k = col; k <= n; ++k) {
                    matrix[row][k] -= factor * matrix[col][k];
                }
            }
        }
        if (max_pivot > 0.0L && min_pivot / max_pivot < 1e-13L) {
            return false;
        }

        std::vector<long double> solved(n, 0.0L);
        for (std::size_t back = n; back-- > 0;) {
            long double value = matrix[back][n];
            for (std::size_t col = back + 1; col < n; ++col) {
                value -= matrix[back][col] * solved[col];
            }
            solved[back] = value / matrix[back][back];
            if (!std::isfinite(solved[back])) return false;
        }
        long double max_value = 0.0L;
        for (const long double value : solved) {
            max_value = std::max(max_value, std::fabs(value));
        }
        const long double negative_tolerance =
            1e-12L * std::max(1.0L, max_value);
        visits.resize(n);
        for (std::size_t i = 0; i < n; ++i) {
            if (solved[i] < -negative_tolerance) return false;
            visits[i] = static_cast<double>(std::max(0.0L, solved[i]));
            if (!std::isfinite(visits[i])) return false;
        }

        long double max_residual = 0.0L;
        for (std::size_t target = 0; target < n; ++target) {
            long double expected = incoming[target];
            for (std::size_t source = 0; source < n; ++source) {
                for (const EvalTransition& transition :
                     pair_row(members[source]).transitions) {
                    if (transition.target == members[target]) {
                        expected += static_cast<long double>(visits[source]) *
                                    transition.probability;
                    }
                }
            }
            max_residual = std::max(
                max_residual,
                std::fabs(static_cast<long double>(visits[target]) - expected));
        }
        const long double tolerance = std::max(
            1e-12L,
            static_cast<long double>(options.epsilon) *
                std::max(1.0L, max_value) * 10.0L);
        return max_residual <= tolerance;
    }

    bool rank_one_solve(
        std::uint32_t component,
        const std::vector<std::uint32_t>& members,
        const std::vector<double>& incoming,
        std::vector<double>& visits) const {
        if (members.size() < 2) return false;
        const auto row = [&](std::uint32_t pair) {
            std::map<std::uint32_t, double> probabilities;
            for (const EvalTransition& transition : pair_row(pair).transitions) {
                if (component_by_pair[transition.target] == component) {
                    probabilities[transition.target] += transition.probability;
                }
            }
            return probabilities;
        };
        const std::map<std::uint32_t, double> reference = row(members.front());
        for (std::size_t source = 1; source < members.size(); ++source) {
            if (pairs[members[source]].row == pairs[members.front()].row) {
                continue;
            }
            const auto candidate = row(members[source]);
            if (candidate.size() != reference.size()) return false;
            auto a = reference.begin();
            auto b = candidate.begin();
            for (; a != reference.end(); ++a, ++b) {
                if (a->first != b->first ||
                    std::fabs(a->second - b->second) > 1e-15) {
                    return false;
                }
            }
        }
        double internal_probability = 0.0;
        for (const auto& [target, probability] : reference) {
            (void)target;
            internal_probability += probability;
        }
        const double denominator = 1.0 - internal_probability;
        if (!(denominator > 1e-14)) return false;
        double total_incoming = 0.0;
        for (const double mass : incoming) total_incoming += mass;
        const double total_visits = total_incoming / denominator;
        if (!std::isfinite(total_visits)) return false;
        visits = incoming;
        for (std::size_t target = 0; target < members.size(); ++target) {
            const auto found = reference.find(members[target]);
            if (found != reference.end()) {
                visits[target] += found->second * total_visits;
            }
        }
        return true;
    }

    void add_absorption(const EvalAbsorption& absorption, double mass) {
        if (!(mass > 0.0)) return;
        if (absorption.edge != kNoId) {
            edge_traversals.at(absorption.edge) += mass;
        }
        add_compressed_policy_incoming(
            absorption.policy_route, absorption.state, mass);
        switch (absorption.kind) {
        case EvalAbsorptionKind::Terminal:
            terminal_mass[absorption.node] += mass;
            add_terminal_incoming(
                absorption.node, absorption.state, mass);
            break;
        case EvalAbsorptionKind::ActionNotApplied:
            action_not_applied[absorption.node] += mass;
            break;
        case EvalAbsorptionKind::NoMatchingEdge:
            no_matching_edge[absorption.node] += mass;
            break;
        }
    }

    void commit_component(
        std::uint32_t component,
        const std::vector<std::uint32_t>& members,
        const std::vector<double>& visits) {
        for (std::size_t i = 0; i < members.size(); ++i) {
            const std::uint32_t pair = members[i];
            const double visit = visits[i];
            pair_visits[pair] += visit;
            const EvalRow& row = pair_row(pair);
            for (const EvalTransition& transition : row.transitions) {
                const double flow = visit * transition.probability;
                if (transition.edge != kNoId) {
                    edge_traversals.at(transition.edge) += flow;
                }
                add_compressed_policy_incoming(
                    transition.policy_route,
                    transition.policy_state, flow);
                if (transition.via != kNoId) {
                    chain_inflow.at(transition.via) += flow;
                }
                if (component_by_pair[transition.target] != component) {
                    external_incoming[transition.target] += flow;
                }
            }
            for (const EvalAbsorption& absorption : row.absorptions) {
                add_absorption(
                    absorption, visit * absorption.probability);
            }
        }
    }

    void add_unresolved(
        const std::vector<std::uint32_t>& members,
        const std::vector<double>& mass,
        bool hard) {
        for (std::size_t i = 0; i < members.size(); ++i) {
            unresolved_pair[members[i]] += mass[i];
        }
        hard_unresolved = hard_unresolved || hard;
    }

    void begin_fallback(
        std::uint32_t component,
        const std::vector<std::uint32_t>& members,
        const std::vector<double>& incoming) {
        fallback = std::make_unique<FallbackState>();
        fallback->component = component;
        fallback->members = members;
        fallback->local_index_by_pair.assign(pairs.size(), kNoId);
        fallback->incoming = incoming;
        fallback->visits.assign(members.size(), 0.0);
        for (std::size_t i = 0; i < members.size(); ++i) {
            fallback->local_index_by_pair[members[i]] =
                static_cast<std::uint32_t>(i);
            fallback->input_mass += incoming[i];
        }
        fallback->diagonal.assign(members.size(), 1.0);
        for (std::size_t source = 0; source < members.size(); ++source) {
            for (const EvalTransition& transition :
                 pair_row(members[source]).transitions) {
                if (transition.target == members[source]) {
                    fallback->diagonal[source] -= transition.probability;
                }
            }
            if (!(fallback->diagonal[source] > 1e-14)) {
                fallback->diagonal[source] = 1.0;
            }
        }
        fallback->wave.resize(members.size());
        for (std::size_t i = 0; i < members.size(); ++i) {
            fallback->wave[i] = incoming[i] / fallback->diagonal[i];
        }
        fallback->shadow = fallback->wave;
        fallback->direction.assign(members.size(), 0.0);
        fallback->image.assign(members.size(), 0.0);
        fallback->intermediate.assign(members.size(), 0.0);
        fallback->image_intermediate.assign(members.size(), 0.0);
        phase = StrategyEvalPhase::Fallback;
    }

    void finish_component() {
        ++component_index;
        phase = component_index < components.size()
                    ? StrategyEvalPhase::Solving
                    : StrategyEvalPhase::Finalization;
    }

    void solve_component() {
        if (component_index >= components.size()) {
            phase = StrategyEvalPhase::Finalization;
            return;
        }
        const std::uint32_t component =
            static_cast<std::uint32_t>(component_index);
        const auto& members = components[component_index];
        std::vector<double> incoming;
        incoming.reserve(members.size());
        double input_mass = 0.0;
        for (const std::uint32_t pair : members) {
            incoming.push_back(external_incoming[pair]);
            input_mass += external_incoming[pair];
        }
        if (input_mass == 0.0) {
            commit_component(component, members, incoming);
            finish_component();
            return;
        }
        if (!component_has_exit(component, members)) {
            /* A recurrent class has infinite visit counts. Preserve a finite,
             * useful entry snapshot and attribute all entering probability as
             * unresolved without fabricating repeated traversals. */
            for (std::size_t i = 0; i < members.size(); ++i) {
                pair_visits[members[i]] += incoming[i];
            }
            add_unresolved(members, incoming, true);
            finish_component();
            return;
        }

        std::vector<double> visits;
        bool solved = false;
        if (members.size() == 1) {
            double self_probability = 0.0;
            for (const EvalTransition& transition :
                 pair_row(members.front()).transitions) {
                if (transition.target == members.front()) {
                    self_probability += transition.probability;
                }
            }
            const double denominator = 1.0 - self_probability;
            if (denominator > 1e-14) {
                const double value = incoming.front() / denominator;
                if (std::isfinite(value) && value >= 0.0) {
                    visits = {value};
                    solved = true;
                }
            }
        } else if (rank_one_solve(
                       component, members, incoming, visits)) {
            solved = true;
        } else if (members.size() <= 64) {
            solved = dense_solve(component, members, incoming, visits);
        }
        if (solved) {
            commit_component(component, members, visits);
            finish_component();
        } else {
            begin_fallback(component, members, incoming);
        }
    }

    void run_fallback_batch() {
        FallbackState& state = *fallback;
        constexpr std::uint32_t kBatchSweeps = 4;
        const auto dot = [](const std::vector<double>& left,
                            const std::vector<double>& right) {
            long double value = 0.0L;
            for (std::size_t i = 0; i < left.size(); ++i) {
                value += static_cast<long double>(left[i]) * right[i];
            }
            return static_cast<double>(value);
        };
        const auto norm = [](const std::vector<double>& values) {
            double value = 0.0;
            for (const double entry : values) {
                value = std::max(value, std::fabs(entry));
            }
            return value;
        };
        const auto apply = [&](const std::vector<double>& input,
                               std::vector<double>& result) {
            result = input;
            for (std::size_t source = 0; source < state.members.size();
                 ++source) {
                if (input[source] == 0.0) continue;
                for (const EvalTransition& transition :
                     pair_row(state.members[source]).transitions) {
                    if (component_by_pair[transition.target] !=
                        state.component) {
                        continue;
                    }
                    const std::uint32_t target =
                        state.local_index_by_pair[transition.target];
                    result[target] -=
                        transition.probability * input[source];
                }
            }
            for (std::size_t i = 0; i < result.size(); ++i) {
                result[i] /= state.diagonal[i];
            }
        };
        const double tolerance =
            options.epsilon * std::max(1.0, state.input_mass);
        bool finished = false;
        bool failed = false;
        for (std::uint32_t batch = 0;
             batch < kBatchSweeps && state.sweeps < options.max_sweeps;
             ++batch) {
            const double rho = dot(state.shadow, state.wave);
            if (!std::isfinite(rho) || std::fabs(rho) <= 1e-30 ||
                !std::isfinite(state.omega) ||
                std::fabs(state.omega) <= 1e-30) {
                failed = true;
                break;
            }
            if (state.sweeps == 0) {
                state.direction = state.wave;
            } else {
                const double beta =
                    (rho / state.rho_previous) *
                    (state.alpha / state.omega);
                for (std::size_t i = 0; i < state.direction.size(); ++i) {
                    state.direction[i] =
                        state.wave[i] +
                        beta * (state.direction[i] -
                                state.omega * state.image[i]);
                }
            }
            apply(state.direction, state.image);
            const double denominator = dot(state.shadow, state.image);
            if (!std::isfinite(denominator) ||
                std::fabs(denominator) <= 1e-30) {
                failed = true;
                break;
            }
            state.alpha = rho / denominator;
            for (std::size_t i = 0; i < state.wave.size(); ++i) {
                state.intermediate[i] =
                    state.wave[i] - state.alpha * state.image[i];
            }
            if (norm(state.intermediate) <= tolerance) {
                for (std::size_t i = 0; i < state.visits.size(); ++i) {
                    state.visits[i] += state.alpha * state.direction[i];
                }
                state.wave = state.intermediate;
                finished = true;
                ++state.sweeps;
                ++fallback_sweeps;
                break;
            }
            apply(state.intermediate, state.image_intermediate);
            const double image_norm =
                dot(state.image_intermediate, state.image_intermediate);
            if (!std::isfinite(image_norm) || image_norm <= 1e-30) {
                failed = true;
                break;
            }
            state.omega =
                dot(state.image_intermediate, state.intermediate) /
                image_norm;
            if (!std::isfinite(state.omega)) {
                failed = true;
                break;
            }
            for (std::size_t i = 0; i < state.visits.size(); ++i) {
                state.visits[i] +=
                    state.alpha * state.direction[i] +
                    state.omega * state.intermediate[i];
                state.wave[i] =
                    state.intermediate[i] -
                    state.omega * state.image_intermediate[i];
            }
            state.rho_previous = rho;
            ++state.sweeps;
            ++fallback_sweeps;
            if (norm(state.wave) <= tolerance) {
                finished = true;
                break;
            }
        }
        if (!finished && !failed && state.sweeps < options.max_sweeps) {
            return;
        }

        std::vector<double> checked;
        apply(state.visits, checked);
        double residual = 0.0;
        for (std::size_t i = 0; i < checked.size(); ++i) {
            const double rhs = state.incoming[i] / state.diagonal[i];
            residual = std::max(residual, std::fabs(checked[i] - rhs));
        }
        if (!failed && residual <= tolerance * 10.0) {
            for (double& visit : state.visits) {
                if (visit < -1e-8 || !std::isfinite(visit)) {
                    failed = true;
                    break;
                }
                visit = std::max(0.0, visit);
            }
        } else {
            failed = true;
        }
        if (!failed) {
            commit_component(state.component, state.members, state.visits);
        } else {
            for (std::size_t i = 0; i < state.members.size(); ++i) {
                pair_visits[state.members[i]] += state.incoming[i];
            }
            add_unresolved(state.members, state.incoming, true);
        }
        fallback.reset();
        finish_component();
    }

    std::vector<double> solve_exact_attribution() {
        using solve_detail::PolicyEdge;
        using solve_detail::PolicyRow;
        using solve_detail::SparsePolicyComponentResult;
        using solve_detail::SparsePolicyComponentStatus;
        using solve_detail::SparsePolicyComponentView;
        using solve_detail::SparsePolicyComponentWorkspace;
        using solve_detail::SparsePolicyResume;
        using solve_detail::SparsePolicyTarjanView;

        const std::size_t count = attribution_pairs.size();
        if (count == 0 || attribution_start_pair >= count ||
            attribution_rows.empty() ||
            attribution_class_by_pair.size() != count) {
            throw std::logic_error(
                "strategy evaluation exact attribution graph is incomplete");
        }

        std::vector<std::uint32_t> incoming_counts(count, 0);
        std::uint64_t edge_count = 0;
        for (const EvalPair& pair : attribution_pairs) {
            if (pair.row >= attribution_rows.size()) {
                throw std::logic_error(
                    "strategy evaluation exact attribution row is missing");
            }
            for (const EvalTransition& transition :
                 attribution_rows[pair.row].transitions) {
                if (transition.target >= count) {
                    throw std::logic_error(
                        "strategy evaluation exact attribution target is "
                        "missing");
                }
                if (incoming_counts[transition.target] ==
                    std::numeric_limits<std::uint32_t>::max()) {
                    throw std::length_error(
                        "strategy evaluation exact attribution edge count "
                        "overflowed");
                }
                ++incoming_counts[transition.target];
                ++edge_count;
            }
        }
        if (edge_count > std::numeric_limits<std::size_t>::max() ||
            edge_count > std::numeric_limits<std::uint32_t>::max()) {
            throw std::length_error(
                "strategy evaluation exact attribution edge count "
                "overflowed");
        }

        std::vector<PolicyRow> transpose_rows(count);
        std::vector<PolicyEdge> transpose_edges(
            static_cast<std::size_t>(edge_count));
        std::vector<std::uint32_t> cursors(count, 0);
        std::uint64_t offset = 0;
        for (std::uint32_t target = 0; target < count; ++target) {
            transpose_rows[target].edge_offset = offset;
            transpose_rows[target].edge_count = incoming_counts[target];
            cursors[target] = static_cast<std::uint32_t>(offset);
            offset += incoming_counts[target];
        }
        for (std::uint32_t source = 0; source < count; ++source) {
            const EvalRow& row =
                attribution_rows[attribution_pairs[source].row];
            for (const EvalTransition& transition : row.transitions) {
                transpose_edges[cursors[transition.target]++] = {
                    source, transition.probability};
            }
        }

        std::vector<std::uint32_t> active_states(count);
        std::vector<std::uint8_t> active(count, 1);
        std::vector<std::uint8_t> terminal(count, 0);
        std::vector<std::uint64_t> policy_rows(count);
        for (std::uint32_t state = 0; state < count; ++state) {
            active_states[state] = state;
            policy_rows[state] = state;
        }
        const std::vector<std::uint32_t> no_representatives;
        SparsePolicyComponentWorkspace workspace;
        std::vector<double> external_incoming_exact(count, 0.0);
        std::vector<double> exact_visits(count, 0.0);
        std::vector<double> previous_values(count, 0.0);
        std::vector<double> visits_by_class(pairs.size(), 0.0);
        external_incoming_exact[attribution_start_pair] = 1.0;

        const auto transient_bytes = [&](const std::uint64_t scratch = 0) {
            std::uint64_t bytes = scratch;
            const auto add_vector = [&](const auto& values) {
                using Value = typename std::decay_t<decltype(values)>::value_type;
                bytes = capped_add(
                    bytes,
                    capped_product(values.capacity(), sizeof(Value)));
            };
            add_vector(incoming_counts);
            add_vector(transpose_rows);
            add_vector(transpose_edges);
            add_vector(cursors);
            add_vector(active_states);
            add_vector(active);
            add_vector(terminal);
            add_vector(policy_rows);
            add_vector(workspace.components);
            for (const auto& component : workspace.components) {
                add_vector(component);
            }
            add_vector(workspace.component_by_state);
            add_vector(workspace.local);
            add_vector(workspace.tarjan_index);
            add_vector(workspace.tarjan_lowlink);
            add_vector(workspace.tarjan_on_stack);
            add_vector(workspace.tarjan_stack);
            add_vector(workspace.tarjan_dfs);
            add_vector(external_incoming_exact);
            add_vector(exact_visits);
            add_vector(previous_values);
            add_vector(visits_by_class);
            return bytes;
        };
        check_owned_cap(transient_bytes());
        const SparsePolicyTarjanView tarjan{
            active_states, active, terminal, policy_rows,
            no_representatives, transpose_rows, transpose_edges};
        while (!solve_detail::advance_sparse_policy_components(
            tarjan, workspace, 65536)) {
            check_owned_cap(transient_bytes());
        }
        check_owned_cap(transient_bytes());

        for (std::uint32_t component = 0;
             component < workspace.components.size(); ++component) {
            const std::vector<std::uint32_t>& members =
                workspace.components[component];
            std::vector<double> rhs;
            rhs.reserve(members.size());
            double input_mass = 0.0;
            for (std::size_t local = 0; local < members.size(); ++local) {
                const std::uint32_t state = members[local];
                workspace.local[state] = static_cast<std::int32_t>(local);
                rhs.push_back(external_incoming_exact[state]);
                input_mass += rhs.back();
            }
            if (!(input_mass > 0.0)) continue;

            bool has_exit = false;
            for (const std::uint32_t source : members) {
                const EvalRow& row =
                    attribution_rows[attribution_pairs[source].row];
                if (!row.absorptions.empty()) has_exit = true;
                for (const EvalTransition& transition : row.transitions) {
                    if (workspace.component_by_state[transition.target] !=
                        component) {
                        has_exit = true;
                    }
                }
            }
            if (!has_exit) {
                for (std::size_t local = 0; local < members.size(); ++local) {
                    exact_visits[members[local]] += rhs[local];
                }
                continue;
            }

            std::unique_ptr<SparsePolicyResume> resume;
            SparsePolicyComponentResult solved;
            do {
                const std::uint64_t scratch = capped_add(
                    solve_detail::sparse_policy_component_scratch_bytes(
                        members.size(), true),
                    capped_product(
                        members.size(),
                        sizeof(double) + sizeof(std::uint32_t)));
                std::uint64_t solve_transient =
                    capped_add(transient_bytes(), scratch);
                solve_transient = capped_add(
                    solve_transient,
                    capped_product(rhs.capacity(), sizeof(double)));
                /* The shared scratch authority includes both the retained
                 * incomplete result capacity and retained resume, plus the
                 * fresh allocations that coexist with them during this call. */
                check_owned_cap(solve_transient);
                solved = solve_detail::advance_sparse_policy_component(
                    SparsePolicyComponentView{
                        members, component,
                        workspace.component_by_state, workspace.local,
                        transpose_rows, transpose_edges, rhs,
                        previous_values,
                        options.max_sweeps},
                    resume);
                std::uint64_t retained_solve = capped_add(
                    transient_bytes(),
                    capped_add(
                        capped_product(rhs.capacity(), sizeof(double)),
                        capped_product(
                            solved.values.capacity(), sizeof(double))));
                if (resume != nullptr) {
                    retained_solve = capped_add(
                        retained_solve, sizeof(SparsePolicyResume));
                    retained_solve = capped_add(
                        retained_solve,
                        capped_product(
                            resume->members.capacity(),
                            sizeof(std::uint32_t)));
                    const auto add_wide = [&](const auto& values) {
                        retained_solve = capped_add(
                            retained_solve,
                            capped_product(
                                values.capacity(),
                                sizeof(solve_detail::WideFloat)));
                    };
                    add_wide(resume->b);
                    add_wide(resume->x);
                    add_wide(resume->r);
                    add_wide(resume->r0);
                    add_wide(resume->p);
                    add_wide(resume->v);
                    add_wide(resume->s);
                    add_wide(resume->t);
                }
                check_owned_cap(retained_solve);
            } while (solved.status ==
                     SparsePolicyComponentStatus::Incomplete);
            if (solved.status ==
                SparsePolicyComponentStatus::DidNotConverge) {
                throw std::length_error(
                    "strategy evaluation exact attribution reached "
                    "max_sweeps (" +
                    std::to_string(options.max_sweeps) + ")");
            }
            if (solved.status != SparsePolicyComponentStatus::Complete ||
                solved.values.size() != members.size()) {
                throw std::runtime_error(
                    "strategy evaluation exact attribution solve failed "
                    "(component_size=" +
                    std::to_string(members.size()) +
                    ", status=" +
                    std::to_string(static_cast<unsigned int>(
                        solved.status)) +
                    ", iterations=" +
                    std::to_string(solved.total_iterations) + ")");
            }
            /* Validate every raw attribution equation before quotient-class
             * aggregation can cancel opposing errors. The shared solver
             * proves its WideFloat iterate; this second check also covers the
             * returned-double conversion and the dense solve path. */
            for (std::size_t local = 0;
                 local < members.size(); ++local) {
                solve_detail::WideFloat expected = rhs[local];
                const PolicyRow& row =
                    transpose_rows[members[local]];
                for (std::uint32_t edge_index = 0;
                     edge_index < row.edge_count; ++edge_index) {
                    const PolicyEdge& edge =
                        transpose_edges.at(
                            row.edge_offset + edge_index);
                    if (workspace.component_by_state[edge.target] !=
                        component) {
                        continue;
                    }
                    const std::int32_t successor_local =
                        workspace.local.at(edge.target);
                    if (successor_local < 0) {
                        throw std::logic_error(
                            "strategy evaluation exact attribution has an "
                            "invalid component-local successor");
                    }
                    expected +=
                        solve_detail::WideFloat{edge.probability} *
                        solve_detail::WideFloat{
                            solved.values[static_cast<std::size_t>(
                                successor_local)]};
                }
                const double raw_residual = std::fabs(
                    (solve_detail::WideFloat{solved.values[local]} -
                     expected)
                        .value());
                const double residual_scale = std::max(
                    {1.0,
                     std::fabs(solved.values[local]),
                     std::fabs(expected.value())});
                if (!std::isfinite(raw_residual) ||
                    raw_residual >
                        options.epsilon * residual_scale) {
                    throw std::runtime_error(
                        "strategy evaluation exact attribution raw "
                        "component residual exceeded epsilon");
                }
            }
            for (std::size_t local = 0; local < members.size(); ++local) {
                const double value = solved.values[local];
                if (!std::isfinite(value) || value < -1e-10) {
                    throw std::runtime_error(
                        "strategy evaluation exact attribution produced an "
                        "invalid occupancy");
                }
                exact_visits[members[local]] = std::max(0.0, value);
            }
            for (const std::uint32_t source : members) {
                const EvalRow& row =
                    attribution_rows[attribution_pairs[source].row];
                for (const EvalTransition& transition : row.transitions) {
                    if (workspace.component_by_state[transition.target] !=
                        component) {
                        external_incoming_exact[transition.target] +=
                            exact_visits[source] * transition.probability;
                    }
                }
            }
        }

        for (std::size_t raw = 0; raw < count; ++raw) {
            const std::uint32_t class_id =
                attribution_class_by_pair[raw];
            if (class_id >= visits_by_class.size()) {
                throw std::logic_error(
                    "strategy evaluation exact attribution class is "
                    "missing");
            }
            visits_by_class[class_id] += exact_visits[raw];
        }
        for (std::size_t class_id = 0;
             class_id < visits_by_class.size(); ++class_id) {
            const double tolerance = std::max(
                1e-9,
                options.epsilon *
                    std::max(
                        1.0,
                        std::max(
                            std::fabs(visits_by_class[class_id]),
                            std::fabs(pair_visits[class_id]))) *
                    100.0);
            if (std::fabs(
                    visits_by_class[class_id] -
                    pair_visits[class_id]) > tolerance) {
                throw std::runtime_error(
                    "strategy evaluation exact attribution does not "
                    "reconcile with the behavioral quotient");
            }
        }
        check_owned_cap(transient_bytes());
        return exact_visits;
    }

    void finalize() {
        propagate_chain_inflow();
        std::vector<double> exact_pair_visits_owned;
        const std::vector<double>* exact_pair_visits = &pair_visits;
        const std::vector<EvalPair>* exact_pairs = &pairs;
        if (!attribution_pairs.empty()) {
            exact_pair_visits_owned = solve_exact_attribution();
            exact_pair_visits = &exact_pair_visits_owned;
            exact_pairs = &attribution_pairs;
            for (auto& incoming : terminal_incoming) incoming.clear();
            for (auto& incoming : compressed_policy_incoming) {
                incoming.clear();
            }
            terminal_incoming_owned_bytes = 0;
            compressed_policy_incoming_owned_bytes = 0;
            for (std::size_t raw = 0;
                 raw < attribution_pairs.size(); ++raw) {
                const double visits = exact_pair_visits->at(raw);
                if (!(visits > 0.0)) continue;
                const EvalRow& row = attribution_rows.at(
                    attribution_pairs[raw].row);
                for (const EvalTransition& transition : row.transitions) {
                    add_compressed_policy_incoming(
                        transition.policy_route,
                        transition.policy_state,
                        visits * transition.probability);
                }
                for (const EvalAbsorption& absorption : row.absorptions) {
                    const double mass = visits * absorption.probability;
                    add_compressed_policy_incoming(
                        absorption.policy_route,
                        absorption.state, mass);
                    if (absorption.kind == EvalAbsorptionKind::Terminal) {
                        add_terminal_incoming(
                            absorption.node, absorption.state, mass);
                    }
                }
                if ((raw & 255u) == 255u) {
                    check_owned_cap(capped_product(
                        exact_pair_visits_owned.capacity(),
                        sizeof(double)));
                }
            }
        }
        CalcContext& calc = *model.calc;
        output.reforge_work =
            calc.telemetry().reforge_frontier_work;
        const std::size_t node_count = strategy->nodes.size();
        const std::size_t operation_pair_count = static_cast<std::size_t>(
            std::count_if(
                exact_pairs->begin(), exact_pairs->end(),
                [](const EvalPair& pair) { return pair.operation; }));
        std::uint64_t finalization_transient_floor =
            capped_product(
                exact_pair_visits_owned.capacity(), sizeof(double)) +
            node_count *
                (4ull * sizeof(double) +
                 sizeof(std::map<std::uint32_t, double>)) +
            exact_pairs->size() *
                (sizeof(std::pair<const std::uint32_t, double>) +
                 3ull * sizeof(void*));
        check_owned_cap(
            finalization_transient_floor +
            static_cast<std::uint64_t>(calc.state_count()) *
                sizeof(AbstractState) +
            static_cast<std::uint64_t>(operation_pair_count) *
                sizeof(StrategyEvalOccupancyEntry));
        output.occupancy_states.reserve(calc.state_count());
        for (std::uint32_t state = 0; state < calc.state_count(); ++state) {
            output.occupancy_states.push_back(calc.state(state));
        }
        output.occupancy.reserve(operation_pair_count);
        output.occupancy_reward_complete = options.economy != nullptr;
        std::vector<double> node_visits(node_count, 0.0);
        std::vector<double> operation_visits(node_count, 0.0);
        std::vector<double> operation_applied(node_count, 0.0);
        std::vector<double> unresolved_by_node(node_count, 0.0);
        std::vector<std::map<std::uint32_t, double>> incoming(node_count);
        std::vector<std::vector<std::pair<std::uint32_t, double>>>
            compressed_top_classes(node_count);
        std::uint64_t compressed_top_owned_bytes =
            compressed_top_classes.capacity() *
            sizeof(std::vector<std::pair<std::uint32_t, double>>);

        std::uint64_t compressed_routes_seen = 0;
        for (std::uint32_t root = 0; root < node_count; ++root) {
            for (const auto& [state, mass] :
                 compressed_policy_incoming[root]) {
                std::uint32_t cursor = root;
                std::size_t steps = 0;
                while (is_policy_route_node(cursor)) {
                    if (++steps > node_count) {
                        throw std::logic_error(
                            "compiled policy router contains a cycle");
                    }
                    node_visits[cursor] += mass;
                    auto& top = compressed_top_classes[cursor];
                    if (options.top_classes_per_node != 0) {
                        const std::size_t capacity_before = top.capacity();
                        top.push_back({state, mass});
                        if (top.capacity() != capacity_before) {
                            compressed_top_owned_bytes +=
                                (top.capacity() - capacity_before) *
                                sizeof(std::pair<std::uint32_t, double>);
                            check_owned_cap(
                                finalization_transient_floor +
                                compressed_top_owned_bytes);
                        }
                        std::stable_sort(
                            top.begin(), top.end(),
                            [](const auto& left, const auto& right) {
                                if (left.second != right.second) {
                                    return left.second > right.second;
                                }
                                return left.first < right.first;
                            });
                        if (top.size() > options.top_classes_per_node) {
                            top.pop_back();
                        }
                    }
                    const StrategyEdge* selected =
                        select_edge(strategy->nodes[cursor], state);
                    if (selected == nullptr) break;
                    edge_traversals.at(
                        edge_index_by_id.at(selected->id)) += mass;
                    cursor = selected->target;
                }
                if ((++compressed_routes_seen & 255u) == 0) {
                    check_owned_cap(
                        finalization_transient_floor +
                        compressed_top_owned_bytes);
                }
            }
        }
        finalization_transient_floor += compressed_top_owned_bytes;

        for (std::size_t pair = 0; pair < pairs.size(); ++pair) {
            const EvalPair& record = pairs[pair];
            unresolved_by_node[record.node] += unresolved_pair[pair];
            output.residual_mass += unresolved_pair[pair];
        }
        for (std::size_t pair = 0;
             pair < exact_pairs->size(); ++pair) {
            const EvalPair& record = exact_pairs->at(pair);
            const double visits = exact_pair_visits->at(pair);
            node_visits[record.node] += visits;
            incoming[record.node][record.state] += visits;
            if (record.operation) {
                output.expected_actions += visits;
                operation_visits[record.node] += visits;
                StrategyEvalOccupancyEntry retained;
                retained.state = record.state;
                retained.node = record.node;
                const ResolvedStrategyOperation& operation =
                    model.operation_by_node.at(record.node);
                retained.action =
                    operation.kind ==
                            ResolvedStrategyOperationKind::Bestiary
                        ? kNoId
                        : operation.descriptor_index;
                retained.expected_visits = visits;
                retained.expected_applied = record.consumes ? visits : 0.0;
                retained.reward_complete = options.economy != nullptr;
                const std::vector<std::string>& cost_keys =
                    operation_cost_keys(
                        operation, calc.registry(), calc.session());
                if (record.consumes && options.economy != nullptr) {
                    for (const std::string& key : cost_keys) {
                        const auto price = options.economy->prices.find(key);
                        if (price == options.economy->prices.end()) {
                            retained.reward_complete = false;
                            output.occupancy_reward_complete = false;
                        } else {
                            retained.immediate_reward += price->second;
                        }
                    }
                }
                output.occupancy_expected_reward +=
                    retained.expected_applied * retained.immediate_reward;
                output.occupancy.push_back(retained);
                if (record.consumes) {
                    operation_applied[record.node] += visits;
                    for (const std::string& key : cost_keys) {
                        output.expected_consumption[key] += visits;
                    }
                }
            }
            if ((pair & 255u) == 255u) {
                check_owned_cap(finalization_transient_floor);
            }
        }
        std::vector<EvalPair>().swap(attribution_pairs);
        std::vector<EvalRow>().swap(attribution_rows);
        std::vector<std::uint32_t>().swap(
            attribution_class_by_pair);
        attribution_row_payload_owned_bytes = 0;
        for (std::size_t node = 0; node < node_count; ++node) {
            node_visits[node] += terminal_mass[node];
            for (const auto& [state, mass] : terminal_incoming[node]) {
                incoming[node][state] += mass;
            }
            const StrategyNode& source = strategy->nodes[node];
            if (source.kind == StrategyNodeKind::Terminal) {
                output.terminal_nodes.push_back(
                    {source.id, source.terminal_kind, terminal_mass[node]});
                if (source.terminal_kind == PC_TERMINAL_SUCCESS) {
                    output.success_probability += terminal_mass[node];
                } else if (source.terminal_kind == PC_TERMINAL_FAILURE) {
                    output.failure_probability += terminal_mass[node];
                } else {
                    output.stop_probability += terminal_mass[node];
                }
            }
            if (unresolved_by_node[node] > 0.0) {
                output.unresolved_by_node.push_back(
                    {source.id, unresolved_by_node[node]});
            }
            if (action_not_applied[node] > 0.0) {
                output.failures_by_node.push_back(
                    {source.id, "action_not_applied",
                     action_not_applied[node]});
                output.action_not_applied_probability +=
                    action_not_applied[node];
            }
            if (no_matching_edge[node] > 0.0) {
                output.failures_by_node.push_back(
                    {source.id, "no_matching_edge",
                     no_matching_edge[node]});
                output.no_matching_edge_probability +=
                    no_matching_edge[node];
            }

            StrategyEvalNode output_node;
            output_node.id = source.id;
            output_node.expected_visits = node_visits[node];
            std::vector<std::pair<std::uint32_t, double>> classes;
            if (compress_policy_routes && is_policy_route_node(
                    static_cast<std::uint32_t>(node))) {
                classes = compressed_top_classes[node];
            } else {
                classes.assign(
                    incoming[node].begin(), incoming[node].end());
            }
            std::stable_sort(
                classes.begin(), classes.end(), [](const auto& a, const auto& b) {
                    if (a.second != b.second) return a.second > b.second;
                    return a.first < b.first;
                });
            const std::size_t keep = std::min<std::size_t>(
                options.top_classes_per_node, classes.size());
            for (std::size_t c = 0; c < keep; ++c) {
                output_node.classes.push_back(
                    {node_visits[node] == 0.0
                         ? 0.0
                         : classes[c].second / node_visits[node],
                     calc.state(classes[c].first)});
            }
            double truncated = 0.0;
            if (compress_policy_routes && is_policy_route_node(
                    static_cast<std::uint32_t>(node))) {
                double retained = 0.0;
                for (std::size_t c = 0; c < keep; ++c) {
                    retained += classes[c].second;
                }
                truncated = std::max(0.0, node_visits[node] - retained);
            } else {
                for (std::size_t c = keep; c < classes.size(); ++c) {
                    truncated += classes[c].second;
                }
            }
            output_node.classes_truncated_share =
                node_visits[node] == 0.0
                    ? 0.0
                    : truncated / node_visits[node];
            output.nodes.push_back(std::move(output_node));

            for (const StrategyEdge& edge : source.edges) {
                output.edges.push_back(
                    {edge.id, edge_traversals.at(edge_index_by_id.at(edge.id))});
            }
            if ((node & 255u) == 255u) {
                check_owned_cap(finalization_transient_floor);
            }
        }

        output.technique_totals = empty_technique_totals();
        std::vector<std::map<std::string, double>> node_techniques(
            node_count);
        std::vector<std::vector<std::string>> node_classifications(
            node_count);
        std::map<std::string, StrategyEvalActionTotal> actions_by_id;
        double total_applied_actions = 0.0;
        for (std::size_t node_index = 0; node_index < node_count;
             ++node_index) {
            const StrategyNode& node = strategy->nodes[node_index];
            if (node.kind != StrategyNodeKind::Operation) continue;
            const ResolvedStrategyOperation& operation =
                model.operation_by_node.at(node_index);
            const std::string& descriptor_id = operation_id(
                operation, calc.registry(), calc.session());
            const std::string& display_name = operation_display_name(
                operation, calc.registry(), calc.session());
            const std::vector<std::string>& cost_keys =
                operation_cost_keys(
                    operation, calc.registry(), calc.session());
            StrategyEvalActionTotal& action =
                actions_by_id[descriptor_id];
            if (action.id.empty()) {
                action.id = descriptor_id;
                action.display_name = display_name;
                action.price_keys = cost_keys;
            }
            action.expected_visits += operation_visits[node_index];
            action.expected_applied += operation_applied[node_index];
            action.nodes.push_back(
                {node.id, operation_visits[node_index],
                 operation_applied[node_index]});
            total_applied_actions += operation_applied[node_index];

            std::vector<std::string> roles = node.accounting_roles;
            if (operation.kind ==
                ResolvedStrategyOperationKind::Restart) {
                add_classification(roles, "restart");
            } else {
                add_classification(roles, "ordinary_crafting");
            }
            if (operation.kind ==
                ResolvedStrategyOperationKind::Bestiary) {
                /* Descriptor-owned Bestiary operations need no ordinary
                 * ActionType classification. */
            } else {
                const ActionDescriptor& descriptor =
                    calc.registry().actions.at(
                        operation.descriptor_index);
                if (descriptor.params.type == ActionType::Fracture) {
                    add_classification(roles, "fracture");
                } else if (
                    descriptor.params.type ==
                        ActionType::RemoveCraftedModifiers) {
                    add_classification(roles, "cleanup_or_replacement");
                } else if (descriptor.params.type == ActionType::Bench) {
                    const std::uint32_t mod = descriptor.params.mod_id;
                    if (mod < calc.session().metamod_type.size()) {
                        const int metamod =
                            calc.session().metamod_type[mod];
                        if (metamod == calc.session().data
                                            ->metamod_prefixes_locked_code ||
                            metamod == calc.session().data
                                            ->metamod_suffixes_locked_code) {
                            add_classification(roles, "protection_setup");
                        } else if (
                            metamod == calc.session().data
                                           ->metamod_multimod_code) {
                            add_classification(roles, "multimod_setup");
                        }
                    }
                    const bool goal_bench = std::any_of(
                        output.targets.begin(), output.targets.end(),
                        [&](const GoalSlot& target) {
                            return target_contains_mod(
                                calc.session(), target, mod);
                        });
                    if (goal_bench) {
                        add_classification(
                            roles, "permanent_goal_bench");
                        add_classification(
                            roles, "deterministic_finish");
                    }
                }
            }
            for (const std::string& role : roles) {
                add_classification(
                    node_classifications[node_index], role);
                add_classification(action.classifications, role);
                add_role_work(
                    output.technique_totals, role,
                    operation_visits[node_index],
                    operation_applied[node_index]);
                add_role_work(
                    node_techniques[node_index], role,
                    operation_visits[node_index],
                    operation_applied[node_index]);
            }
            if ((node_index & 255u) == 255u) {
                check_owned_cap(finalization_transient_floor);
            }
        }
        struct RetainedRegion {
            StrategyEvalActionRegion totals;
            std::set<std::uint32_t> states;
        };
        using RegionKey = std::tuple<
            std::uint32_t, std::uint8_t, std::uint32_t, std::uint32_t,
            std::uint32_t, std::uint32_t>;
        std::map<std::string, std::map<RegionKey, RetainedRegion>>
            regions_by_action;
        std::map<std::string, std::set<std::uint32_t>> states_by_action;
        const auto bit_count = [](std::uint32_t value) {
            std::uint32_t count = 0;
            while (value != 0) {
                count += value & 1u;
                value >>= 1u;
            }
            return count;
        };
        const auto vector_count = [](const CompactCountVector& values) {
            std::uint32_t count = 0;
            for (const std::uint8_t value : values) count += value;
            return count;
        };
        for (const StrategyEvalOccupancyEntry& entry : output.occupancy) {
            if (entry.state >= output.occupancy_states.size() ||
                entry.node >= model.operation_by_node.size() ||
                entry.expected_visits <= 0.0) {
                continue;
            }
            const ResolvedStrategyOperation& operation =
                model.operation_by_node[entry.node];
            if (!operation.resolved()) continue;
            const AbstractState& state =
                output.occupancy_states[entry.state];
            std::uint32_t progress = 0;
            for (std::size_t slot = 0; slot < output.targets.size(); ++slot) {
                if (state.slot_status[slot] == static_cast<std::uint8_t>(
                        GoalSlotStatus::Satisfied)) {
                    ++progress;
                }
            }
            const std::uint32_t crafted =
                bit_count(state.crafted_goal_mask) +
                vector_count(state.crafted_junk_counts);
            const std::uint32_t fractured =
                bit_count(state.fractured_goal_mask) +
                bit_count(state.fractured_metamod_flags) +
                vector_count(state.fractured_junk_counts);
            const RegionKey key{
                progress, state.rarity, bit_count(state.blocked_mask),
                crafted, state.fractured_goal_mask, fractured};
            const std::string& id = operation_id(
                operation, calc.registry(), calc.session());
            RetainedRegion& region = regions_by_action[id][key];
            region.totals.goal_progress = progress;
            region.totals.rarity = state.rarity;
            region.totals.blocker_count = bit_count(state.blocked_mask);
            region.totals.crafted_count = crafted;
            region.totals.fractured_goal_mask =
                state.fractured_goal_mask;
            region.totals.fractured_count = fractured;
            region.totals.expected_visits += entry.expected_visits;
            region.totals.expected_applied += entry.expected_applied;
            region.states.insert(entry.state);
            states_by_action[id].insert(entry.state);
        }
        for (auto& [id, action] : actions_by_id) {
            const auto action_states = states_by_action.find(id);
            action.reachable_states =
                action_states == states_by_action.end()
                    ? 0
                    : static_cast<std::uint32_t>(
                          action_states->second.size());
            const auto retained_regions = regions_by_action.find(id);
            if (retained_regions != regions_by_action.end()) {
                for (auto& [unused_key, retained] :
                     retained_regions->second) {
                    (void)unused_key;
                    retained.totals.reachable_states =
                        static_cast<std::uint32_t>(
                            retained.states.size());
                    action.regions.push_back(retained.totals);
                }
            }
        }
        for (const auto& [unused, action] : actions_by_id) {
            (void)unused;
            output.action_totals.push_back(action);
        }

        std::unordered_map<std::string, double> traversal_by_edge;
        for (const StrategyEvalEdge& edge : output.edges) {
            traversal_by_edge.emplace(edge.id, edge.expected_traversals);
        }
        std::unordered_map<std::string, std::map<std::string, double>>
            edge_techniques;
        for (const StrategyNode& node : strategy->nodes) {
            for (const StrategyEdge& edge : node.edges) {
                const double traversals = traversal_by_edge.at(edge.id);
                for (const std::string& role : edge.accounting_roles) {
                    add_role_work(
                        output.technique_totals, role, traversals,
                        traversals);
                    add_role_work(
                        edge_techniques[edge.id], role, traversals,
                        traversals);
                }
            }
        }

        output.pricing_enabled = options.economy != nullptr;
        if (options.economy != nullptr) {
            output.economy_id = options.economy->id;
        }
        output.material_totals = price_materials(
            output.expected_consumption, options.economy,
            output.known_expected_cost, output.cost_complete);
        if (output.cost_complete) {
            output.total_expected_cost = output.known_expected_cost;
        }

        double descriptor_visits = 0.0;
        double descriptor_applied = 0.0;
        std::map<std::string, double> action_materials;
        for (const StrategyEvalActionTotal& action : output.action_totals) {
            descriptor_visits += action.expected_visits;
            descriptor_applied += action.expected_applied;
            for (const std::string& key : action.price_keys) {
                action_materials[key] += action.expected_applied;
            }
        }
        output.action_descriptor_visits_difference =
            descriptor_visits - output.expected_actions;
        output.action_descriptor_applied_difference =
            descriptor_applied - total_applied_actions;
        double operation_visit_sum = 0.0;
        for (const double visits : operation_visits) {
            operation_visit_sum += visits;
        }
        output.node_operation_visits_difference =
            operation_visit_sum - output.expected_actions;
        for (const auto& [key, quantity] : output.expected_consumption) {
            output.material_quantity_differences[key] =
                action_materials[key] - quantity;
        }
        double priced_dot_product = 0.0;
        for (const StrategyEvalMaterialTotal& material :
             output.material_totals) {
            if (material.priced) {
                priced_dot_product += material.expected_quantity *
                                      material.unit_price;
            }
        }
        output.cost_dot_product_difference =
            priced_dot_product - output.known_expected_cost;
        output.occupancy_reward_difference =
            output.occupancy_expected_reward - output.known_expected_cost;

        output.review_sections_enabled = !review_sections.empty();
        if (!review_sections.empty()) {
            std::vector<std::size_t> section_by_node(node_count);
            for (std::size_t section = 0; section < review_sections.size();
                 ++section) {
                for (const std::uint32_t node :
                     review_sections[section].nodes) {
                    section_by_node[node] = section;
                }
            }
            std::vector<std::map<std::string, StrategyEvalActionTotal>>
                section_actions(review_sections.size());
            std::vector<std::map<std::string, double>> section_materials(
                review_sections.size());
            output.review_sections.resize(review_sections.size());
            for (std::size_t section = 0; section < review_sections.size();
                 ++section) {
                StrategyEvalReviewSection& target =
                    output.review_sections[section];
                target.id = review_sections[section].id;
                target.label = review_sections[section].label;
                target.role = review_sections[section].role;
                target.raw_edge_ids = review_sections[section].edges;
                target.techniques = empty_technique_totals();
                for (const std::uint32_t node_index :
                     review_sections[section].nodes) {
                    target.raw_node_ids.push_back(
                        strategy->nodes[node_index].id);
                    target.expected_actions += operation_visits[node_index];
                    for (const auto& [key, value] :
                         node_techniques[node_index]) {
                        target.techniques[key] += value;
                    }
                    if (strategy->nodes[node_index].kind !=
                        StrategyNodeKind::Operation) {
                        continue;
                    }
                    const ResolvedStrategyOperation& operation =
                        model.operation_by_node.at(node_index);
                    const std::string& descriptor_id = operation_id(
                        operation, calc.registry(), calc.session());
                    const std::string& display_name =
                        operation_display_name(
                            operation, calc.registry(), calc.session());
                    const std::vector<std::string>& cost_keys =
                        operation_cost_keys(
                            operation, calc.registry(), calc.session());
                    StrategyEvalActionTotal& action =
                        section_actions[section][descriptor_id];
                    if (action.id.empty()) {
                        action.id = descriptor_id;
                        action.display_name = display_name;
                        action.price_keys = cost_keys;
                    }
                    for (const std::string& role :
                         node_classifications[node_index]) {
                        add_classification(action.classifications, role);
                    }
                    action.expected_visits += operation_visits[node_index];
                    action.expected_applied += operation_applied[node_index];
                    action.nodes.push_back(
                        {strategy->nodes[node_index].id,
                         operation_visits[node_index],
                         operation_applied[node_index]});
                    for (const std::string& key : cost_keys) {
                        section_materials[section][key] +=
                            operation_applied[node_index];
                    }
                }
                for (const std::string& edge :
                     review_sections[section].edges) {
                    target.expected_edge_traversals +=
                        traversal_by_edge.at(edge);
                    for (const auto& [key, value] : edge_techniques[edge]) {
                        target.techniques[key] += value;
                    }
                }
                for (auto& [unused, action] : section_actions[section]) {
                    (void)unused;
                    target.actions.push_back(std::move(action));
                }
                target.materials = price_materials(
                    section_materials[section], options.economy,
                    target.known_expected_cost, target.cost_complete);
                if (target.cost_complete) {
                    target.total_expected_cost = target.known_expected_cost;
                }
                check_owned_cap(finalization_transient_floor);
            }
            double section_actions_sum = 0.0;
            std::map<std::string, double> section_material_sum;
            for (const StrategyEvalReviewSection& section :
                 output.review_sections) {
                section_actions_sum += section.expected_actions;
                for (const StrategyEvalMaterialTotal& material :
                     section.materials) {
                    section_material_sum[material.price_key] +=
                        material.expected_quantity;
                }
            }
            output.section_actions_difference =
                section_actions_sum - output.expected_actions;
            for (const auto& [key, quantity] :
                 output.expected_consumption) {
                output.section_material_differences[key] =
                    section_material_sum[key] - quantity;
            }
        }
        output.success_normalized_enabled =
            options.include_success_normalized &&
            output.success_probability > 0.0 &&
            output.success_probability < 1.0;
        output.unresolved_probability = output.residual_mass;
        output.sweeps = static_cast<std::uint32_t>(std::min<std::uint64_t>(
            fallback_sweeps,
            std::numeric_limits<std::uint32_t>::max()));
        output.converged =
            !hard_unresolved && output.residual_mass < options.epsilon;
        const double conservation_error = std::fabs(
            absorbed_probability(output) + output.residual_mass - 1.0);
        output.max_mass_conservation_error = std::max(
            output.max_mass_conservation_error, conservation_error);
        if (conservation_error > 1e-8) {
            throw std::runtime_error(
                "strategy evaluation mass conservation failed");
        }
        check_owned_cap();
        phase = StrategyEvalPhase::Done;
    }

    StrategyEvalResult forward_reference() {
        if (phase == StrategyEvalPhase::Finalization) {
            unresolved_pair.assign(pairs.size(), 0.0);
            pair_visits.assign(pairs.size(), 0.0);
            finalize();
            return output;
        }
        if (discover_index != pairs.size()) {
            throw std::logic_error(
                "strategy evaluation reference requires completed discovery");
        }
        pair_visits.assign(pairs.size(), 0.0);
        unresolved_pair.assign(pairs.size(), 0.0);
        std::vector<double> wave = external_incoming;
        std::uint32_t sweeps = 0;
        for (; sweeps < options.max_sweeps; ++sweeps) {
            std::vector<double> next(pairs.size(), 0.0);
            for (std::size_t pair = 0; pair < pairs.size(); ++pair) {
                const double mass = wave[pair];
                if (!(mass > 0.0)) continue;
                pair_visits[pair] += mass;
                const EvalRow& row = pair_row(static_cast<std::uint32_t>(pair));
                for (const EvalTransition& transition :
                     row.transitions) {
                    const double flow = mass * transition.probability;
                    next[transition.target] += flow;
                    if (transition.edge != kNoId) {
                        edge_traversals.at(transition.edge) += flow;
                    }
                    add_compressed_policy_incoming(
                        transition.policy_route,
                        transition.policy_state, flow);
                    if (transition.via != kNoId) {
                        chain_inflow.at(transition.via) += flow;
                    }
                }
                for (const EvalAbsorption& absorption :
                     row.absorptions) {
                    add_absorption(
                        absorption, mass * absorption.probability);
                }
            }
            double transient = 0.0;
            for (const double mass : next) transient += mass;
            double absorbed = 0.0;
            for (const double mass : terminal_mass) absorbed += mass;
            for (const double mass : action_not_applied) absorbed += mass;
            for (const double mass : no_matching_edge) absorbed += mass;
            const double error = std::fabs(absorbed + transient - 1.0);
            output.max_mass_conservation_error = std::max(
                output.max_mass_conservation_error, error);
            if (error > 1e-8) {
                throw std::runtime_error(
                    "strategy evaluation reference mass conservation failed");
            }
            wave = std::move(next);
            if (transient < options.epsilon) {
                ++sweeps;
                break;
            }
        }
        double residual = 0.0;
        for (std::size_t pair = 0; pair < wave.size(); ++pair) {
            unresolved_pair[pair] = wave[pair];
            residual += wave[pair];
        }
        hard_unresolved = residual >= options.epsilon;
        fallback_sweeps = sweeps;
        phase = StrategyEvalPhase::Finalization;
        finalize();
        return output;
    }

    void step(std::uint32_t max_work_items) {
        std::uint32_t remaining = std::max<std::uint32_t>(1, max_work_items);
        while (remaining-- > 0 && phase != StrategyEvalPhase::Done) {
            switch (phase) {
            case StrategyEvalPhase::Discovery:
                if (discover_index < pairs.size()) {
                    expand_pair(static_cast<std::uint32_t>(discover_index));
                    ++discover_index;
                } else {
                    build_components();
                    phase = StrategyEvalPhase::Solving;
                }
                break;
            case StrategyEvalPhase::Solving:
                solve_component();
                break;
            case StrategyEvalPhase::Fallback:
                run_fallback_batch();
                break;
            case StrategyEvalPhase::Finalization:
                finalize();
                break;
            case StrategyEvalPhase::Done:
                break;
            }
            if (phase == StrategyEvalPhase::Discovery &&
                discover_index != 0 &&
                (discover_index & 4095u) == 0) {
                audit_owned_bytes();
            } else if (phase == StrategyEvalPhase::Done) {
                audit_owned_bytes();
            }
            check_owned_cap();
        }
    }

    StrategyEvalProgress progress() const {
        StrategyEvalProgress value;
        value.phase = phase;
        value.done = phase == StrategyEvalPhase::Done;
        value.discovered_pairs = discover_index;
        value.pending_pairs = pairs.size() - discover_index;
        value.solved_sccs = component_index;
        value.total_sccs = components.size();
        value.fallback_sweeps = fallback_sweeps;
        if (fallback != nullptr) {
            for (const double mass : fallback->wave) {
                value.residual += std::fabs(mass);
            }
        } else {
            for (const double mass : unresolved_pair) value.residual += mass;
        }
        return value;
    }
};

StrategyEvalWork::StrategyEvalWork(
    std::shared_ptr<const StrategyImpl> strategy,
    const StrategyEvalOptions& options)
    : impl_(std::make_unique<Impl>(std::move(strategy), options)) {}

StrategyEvalWork::~StrategyEvalWork() = default;
StrategyEvalWork::StrategyEvalWork(StrategyEvalWork&&) noexcept = default;
StrategyEvalWork& StrategyEvalWork::operator=(StrategyEvalWork&&) noexcept =
    default;

void StrategyEvalWork::step(std::uint32_t max_work_items) {
    impl_->step(max_work_items);
}

StrategyEvalProgress StrategyEvalWork::progress() const {
    return impl_->progress();
}

const StrategyEvalResult& StrategyEvalWork::result() const {
    if (impl_->phase != StrategyEvalPhase::Done) {
        throw std::logic_error("strategy evaluation is not finished");
    }
    return impl_->output;
}

std::uint64_t StrategyEvalWork::live_owned_bytes() const {
    return impl_->fast_estimated_owned_bytes();
}

std::uint64_t StrategyEvalWork::peak_owned_bytes() const {
    return std::max(
        impl_->peak_owned_bytes_value, impl_->fast_estimated_owned_bytes());
}

StrategyEvalResult evaluate_strategy(
    const StrategyImpl& strategy,
    const StrategyEvalOptions& options) {
    std::shared_ptr<const StrategyImpl> borrowed(
        &strategy, [](const StrategyImpl*) {});
    StrategyEvalWork work(std::move(borrowed), options);
    while (!work.progress().done) work.step(4096);
    return work.result();
}

StrategyEvalResult evaluate_strategy_forward_reference_for_test(
    const StrategyImpl& strategy,
    const StrategyEvalOptions& options) {
    std::shared_ptr<const StrategyImpl> borrowed(
        &strategy, [](const StrategyImpl*) {});
    StrategyEvalWork work(std::move(borrowed), options);
    while (work.impl_->phase == StrategyEvalPhase::Discovery) {
        work.impl_->step(1);
    }
    return work.impl_->forward_reference();
}

namespace {

void append_material_totals_json(
    BoundedJson& out,
    const std::vector<StrategyEvalMaterialTotal>& materials,
    double divisor = 1.0) {
    out.push_back('[');
    for (std::size_t i = 0; i < materials.size(); ++i) {
        if (i != 0) out.push_back(',');
        const StrategyEvalMaterialTotal& material = materials[i];
        out += "{\"price_key\":\"" + json_escape(material.price_key) +
               "\",\"expected_quantity\":";
        append_number(out, material.expected_quantity / divisor);
        out += ",\"price_status\":\"";
        out += material.priced ? "priced" : "missing";
        out += "\",\"unit_price\":";
        if (material.priced) {
            append_number(out, material.unit_price);
        } else {
            out += "null";
        }
        out += ",\"cost_contribution\":";
        if (material.priced) {
            append_number(out, material.cost_contribution / divisor);
        } else {
            out += "null";
        }
        out.push_back('}');
    }
    out.push_back(']');
}

void append_techniques_json(
    BoundedJson& out,
    const std::map<std::string, double>& techniques,
    double divisor = 1.0) {
    out.push_back('{');
    std::size_t index = 0;
    for (const auto& [key, value] : techniques) {
        if (index++ != 0) out.push_back(',');
        out += "\"" + json_escape(key) + "\":";
        append_number(out, value / divisor);
    }
    out.push_back('}');
}

void append_action_totals_json(
    BoundedJson& out,
    const std::vector<StrategyEvalActionTotal>& actions,
    const std::vector<StrategyEvalMaterialTotal>& priced_materials,
    double divisor = 1.0) {
    std::map<std::string, const StrategyEvalMaterialTotal*> price_by_key;
    for (const StrategyEvalMaterialTotal& material : priced_materials) {
        price_by_key.emplace(material.price_key, &material);
    }
    double known_total_cost = 0.0;
    for (const StrategyEvalMaterialTotal& material : priced_materials) {
        if (material.priced) known_total_cost += material.cost_contribution;
    }
    out.push_back('[');
    for (std::size_t i = 0; i < actions.size(); ++i) {
        if (i != 0) out.push_back(',');
        const StrategyEvalActionTotal& action = actions[i];
        out += "{\"id\":\"" + json_escape(action.id) +
               "\",\"display_name\":\"" +
               json_escape(action.display_name) +
               "\",\"expected_visits\":";
        append_number(out, action.expected_visits / divisor);
        out += ",\"expected_applied\":";
        append_number(out, action.expected_applied / divisor);
        double action_known_spend = 0.0;
        bool action_spend_complete = true;
        for (const std::string& key : action.price_keys) {
            const auto price = price_by_key.find(key);
            if (price == price_by_key.end() || !price->second->priced) {
                action_spend_complete = false;
            } else {
                action_known_spend +=
                    action.expected_applied * price->second->unit_price /
                    divisor;
            }
        }
        out += ",\"expected_spend_known\":";
        append_number(out, action_known_spend);
        out += ",\"expected_spend_complete\":";
        out += action_spend_complete ? "true" : "false";
        out += ",\"known_cost_share\":";
        if (known_total_cost > 0.0) {
            append_number(
                out, action_known_spend /
                         (known_total_cost / divisor));
        } else {
            out += "null";
        }
        out += ",\"probability_of_any_use\":{";
        if (action.expected_visits == 0.0) {
            out += "\"status\":\"exact_zero\",\"value\":0}";
        } else {
            out += "\"status\":\"not_computable_from_occupancy\","
                   "\"value\":null}";
        }
        out += ",\"reachable_states\":" +
               std::to_string(action.reachable_states);
        out += ",\"reachable_regions\":" +
               std::to_string(action.regions.size());
        out += ",\"classifications\":[";
        for (std::size_t role = 0; role < action.classifications.size();
             ++role) {
            if (role != 0) out.push_back(',');
            out += "\"" + json_escape(action.classifications[role]) + "\"";
        }
        out += "],\"materials\":[";
        std::map<std::string, std::uint32_t> quantities;
        for (const std::string& key : action.price_keys) ++quantities[key];
        std::size_t material_index = 0;
        for (const auto& [key, count] : quantities) {
            if (material_index++ != 0) out.push_back(',');
            const double quantity =
                action.expected_applied * static_cast<double>(count) /
                divisor;
            out += "{\"price_key\":\"" + json_escape(key) +
                   "\",\"expected_quantity\":";
            append_number(out, quantity);
            const auto found = price_by_key.find(key);
            const bool priced =
                found != price_by_key.end() && found->second->priced;
            out += ",\"price_status\":\"";
            out += priced ? "priced" : "missing";
            out += "\",\"unit_price\":";
            if (priced) {
                append_number(out, found->second->unit_price);
            } else {
                out += "null";
            }
            out += ",\"cost_contribution\":";
            if (priced) {
                append_number(out, quantity * found->second->unit_price);
            } else {
                out += "null";
            }
            out.push_back('}');
        }
        out += "],\"regions\":[";
        for (std::size_t region_index = 0;
             region_index < action.regions.size(); ++region_index) {
            if (region_index != 0) out.push_back(',');
            const StrategyEvalActionRegion& region =
                action.regions[region_index];
            out += "{\"goal_progress\":" +
                   std::to_string(region.goal_progress);
            out += ",\"rarity\":" + std::to_string(region.rarity);
            out += ",\"blocker_count\":" +
                   std::to_string(region.blocker_count);
            out += ",\"crafted_count\":" +
                   std::to_string(region.crafted_count);
            out += ",\"fractured_goal_mask\":" +
                   std::to_string(region.fractured_goal_mask);
            out += ",\"fractured_count\":" +
                   std::to_string(region.fractured_count);
            out += ",\"reachable_states\":" +
                   std::to_string(region.reachable_states);
            out += ",\"expected_visits\":";
            append_number(out, region.expected_visits / divisor);
            out += ",\"expected_applied\":";
            append_number(out, region.expected_applied / divisor);
            out.push_back('}');
        }
        out += "],\"raw_nodes\":[";
        for (std::size_t node = 0; node < action.nodes.size(); ++node) {
            if (node != 0) out.push_back(',');
            const StrategyEvalActionNode& entry = action.nodes[node];
            out += "{\"node_id\":\"" + json_escape(entry.node_id) +
                   "\",\"expected_visits\":";
            append_number(out, entry.expected_visits / divisor);
            out += ",\"expected_applied\":";
            append_number(out, entry.expected_applied / divisor);
            out.push_back('}');
        }
        out += "]}";
    }
    out.push_back(']');
}

void append_cost_totals_json(
    BoundedJson& out,
    double expected_actions,
    double known_cost,
    bool complete,
    double total_cost,
    double divisor = 1.0) {
    out += "{\"expected_actions\":";
    append_number(out, expected_actions / divisor);
    out += ",\"known_expected_cost\":";
    append_number(out, known_cost / divisor);
    out += ",\"total_expected_cost\":";
    if (complete) {
        append_number(out, total_cost / divisor);
    } else {
        out += "null";
    }
    out += ",\"cost_complete\":";
    out += complete ? "true" : "false";
    out.push_back('}');
}

} // namespace

std::string serialize_strategy_eval(const StrategyEvalResult& result) {
    BoundedJson out(result.max_output_json_bytes);
    out += "{\"version\":\"v1\",\"converged\":";
    out += result.converged ? "true" : "false";
    out += ",\"sweeps\":" + std::to_string(result.sweeps);
    out += ",\"residual_mass\":";
    append_number(out, result.residual_mass);
    out += ",\"terminals\":{";
    out += "\"success\":";
    append_number(out, result.success_probability);
    out += ",\"failure\":";
    append_number(out, result.failure_probability);
    out += ",\"stop\":";
    append_number(out, result.stop_probability);
    out += ",\"action_not_applied\":";
    append_number(out, result.action_not_applied_probability);
    out += ",\"no_matching_edge\":";
    append_number(out, result.no_matching_edge_probability);
    out += ",\"unresolved\":";
    append_number(out, result.unresolved_probability);
    out += ",\"by_node\":[";
    for (std::size_t i = 0; i < result.terminal_nodes.size(); ++i) {
        if (i != 0) out += ',';
        const StrategyEvalTerminalNode& node = result.terminal_nodes[i];
        out += "{\"node_id\":\"" + json_escape(node.node_id) +
               "\",\"kind\":\"" + terminal_name(node.terminal_kind) +
               "\",\"p\":";
        append_number(out, node.probability);
        out += '}';
    }
    out += "]}";

    out += ",\"unresolved_by_node\":[";
    for (std::size_t i = 0; i < result.unresolved_by_node.size(); ++i) {
        if (i != 0) out += ',';
        const StrategyEvalNodeMass& node = result.unresolved_by_node[i];
        out += "{\"node_id\":\"" + json_escape(node.node_id) +
               "\",\"mass\":";
        append_number(out, node.mass);
        out += '}';
    }
    out += ']';

    out += ",\"failures_by_node\":[";
    for (std::size_t i = 0; i < result.failures_by_node.size(); ++i) {
        if (i != 0) out += ',';
        const StrategyEvalFailure& failure = result.failures_by_node[i];
        out += "{\"node_id\":\"" + json_escape(failure.node_id) +
               "\",\"reason\":\"" + json_escape(failure.reason) +
               "\",\"p\":";
        append_number(out, failure.probability);
        out += '}';
    }
    out += ']';

    out += ",\"expected_actions\":";
    append_number(out, result.expected_actions);
    out += ",\"expected_consumption\":[";
    std::size_t consumption_index = 0;
    for (const auto& [key, quantity] : result.expected_consumption) {
        if (consumption_index++ != 0) out += ',';
        out += "{\"key\":\"" + json_escape(key) +
               "\",\"quantity\":";
        append_number(out, quantity);
        out += '}';
    }
    out += ']';

    out += ",\"accounting\":{\"version\":\"s8.4_v1\",\"semantics\":{";
    out += "\"primary\":\"per_strategy_invocation\",";
    out += "\"terminal_mass_separate\":true,";
    out += "\"success_normalized_basis\":";
    if (result.success_normalized_enabled) {
        out += "\"independent_whole_strategy_retries\"";
    } else {
        out += "null";
    }
    out += ",\"success_normalized_is_conditional_path_expectation\":false}";
    out += ",\"pricing\":{\"status\":\"";
    out += !result.pricing_enabled
               ? "disabled"
               : (result.cost_complete ? "complete" : "incomplete");
    out += "\",\"economy_id\":";
    if (result.pricing_enabled) {
        out += "\"" + json_escape(result.economy_id) + "\"";
    } else {
        out += "null";
    }
    out += ",\"missing_price_keys\":[";
    std::size_t missing_price_index = 0;
    for (const StrategyEvalMaterialTotal& material : result.material_totals) {
        if (material.priced) continue;
        if (missing_price_index++ != 0) out.push_back(',');
        out += "\"" + json_escape(material.price_key) + "\"";
    }
    out += "]}";
    out += ",\"totals\":{\"per_invocation\":";
    append_cost_totals_json(
        out, result.expected_actions, result.known_expected_cost,
        result.cost_complete, result.total_expected_cost);
    out += ",\"success_normalized\":";
    if (result.success_normalized_enabled) {
        out += "{\"basis\":\"independent_whole_strategy_retries\",";
        out += "\"success_probability_denominator\":";
        append_number(out, result.success_probability);
        out += ",\"expected_invocations\":";
        append_number(out, 1.0 / result.success_probability);
        out += ",\"work\":";
        append_cost_totals_json(
            out, result.expected_actions, result.known_expected_cost,
            result.cost_complete, result.total_expected_cost,
            result.success_probability);
        out.push_back('}');
    } else {
        out += "null";
    }
    out.push_back('}');
    out += ",\"actions\":{\"per_invocation\":";
    append_action_totals_json(
        out, result.action_totals, result.material_totals);
    out += ",\"success_normalized\":";
    if (result.success_normalized_enabled) {
        append_action_totals_json(
            out, result.action_totals, result.material_totals,
            result.success_probability);
    } else {
        out += "null";
    }
    out.push_back('}');
    out += ",\"materials\":{\"per_invocation\":";
    append_material_totals_json(out, result.material_totals);
    out += ",\"success_normalized\":";
    if (result.success_normalized_enabled) {
        append_material_totals_json(
            out, result.material_totals, result.success_probability);
    } else {
        out += "null";
    }
    out.push_back('}');
    out += ",\"techniques\":{\"per_invocation\":";
    append_techniques_json(out, result.technique_totals);
    out += ",\"success_normalized\":";
    if (result.success_normalized_enabled) {
        append_techniques_json(
            out, result.technique_totals, result.success_probability);
    } else {
        out += "null";
    }
    out.push_back('}');
    out += ",\"review_sections\":{\"enabled\":";
    out += result.review_sections_enabled ? "true" : "false";
    out += ",\"items\":[";
    for (std::size_t i = 0; i < result.review_sections.size(); ++i) {
        if (i != 0) out.push_back(',');
        const StrategyEvalReviewSection& section = result.review_sections[i];
        out += "{\"id\":\"" + json_escape(section.id) +
               "\",\"label\":\"" + json_escape(section.label) +
               "\",\"role\":\"" + json_escape(section.role) +
               "\",\"raw_references\":{\"node_ids\":[";
        for (std::size_t node = 0; node < section.raw_node_ids.size(); ++node) {
            if (node != 0) out.push_back(',');
            out += "\"" + json_escape(section.raw_node_ids[node]) + "\"";
        }
        out += "],\"edge_ids\":[";
        for (std::size_t edge = 0; edge < section.raw_edge_ids.size(); ++edge) {
            if (edge != 0) out.push_back(',');
            out += "\"" + json_escape(section.raw_edge_ids[edge]) + "\"";
        }
        out += "]},\"per_invocation\":";
        append_cost_totals_json(
            out, section.expected_actions, section.known_expected_cost,
            section.cost_complete, section.total_expected_cost);
        out += ",\"expected_edge_traversals\":";
        append_number(out, section.expected_edge_traversals);
        out += ",\"actions\":";
        append_action_totals_json(
            out, section.actions, section.materials);
        out += ",\"materials\":";
        append_material_totals_json(out, section.materials);
        out += ",\"techniques\":";
        append_techniques_json(out, section.techniques);
        out += ",\"success_normalized\":";
        if (result.success_normalized_enabled) {
            out += "{\"work\":";
            append_cost_totals_json(
                out, section.expected_actions, section.known_expected_cost,
                section.cost_complete, section.total_expected_cost,
                result.success_probability);
            out += ",\"expected_edge_traversals\":";
            append_number(
                out, section.expected_edge_traversals /
                         result.success_probability);
            out += ",\"actions\":";
            append_action_totals_json(
                out, section.actions, section.materials,
                result.success_probability);
            out += ",\"materials\":";
            append_material_totals_json(
                out, section.materials, result.success_probability);
            out += ",\"techniques\":";
            append_techniques_json(
                out, section.techniques, result.success_probability);
            out.push_back('}');
        } else {
            out += "null";
        }
        out.push_back('}');
    }
    out += "]}";
    out += ",\"reconciliation\":{\"action_descriptor_visits_difference\":";
    append_number(out, result.action_descriptor_visits_difference);
    out += ",\"action_descriptor_applied_difference\":";
    append_number(out, result.action_descriptor_applied_difference);
    out += ",\"node_operation_visits_difference\":";
    append_number(out, result.node_operation_visits_difference);
    out += ",\"material_quantity_differences\":{";
    std::size_t difference_index = 0;
    for (const auto& [key, difference] :
         result.material_quantity_differences) {
        if (difference_index++ != 0) out.push_back(',');
        out += "\"" + json_escape(key) + "\":";
        append_number(out, difference);
    }
    out += "},\"cost_dot_product_difference\":";
    append_number(out, result.cost_dot_product_difference);
    out += ",\"section_actions_difference\":";
    append_number(out, result.section_actions_difference);
    out += ",\"section_material_differences\":{";
    difference_index = 0;
    for (const auto& [key, difference] :
         result.section_material_differences) {
        if (difference_index++ != 0) out.push_back(',');
        out += "\"" + json_escape(key) + "\":";
        append_number(out, difference);
    }
    out += "}}}";

    out += ",\"memory\":{\"owned_bytes_estimate\":" +
           std::to_string(result.owned_bytes_estimate) +
           ",\"peak_owned_bytes_estimate\":" +
           std::to_string(result.peak_owned_bytes_estimate) +
           ",\"max_owned_bytes\":" +
           std::to_string(result.max_owned_bytes) +
           ",\"max_output_json_bytes\":" +
           std::to_string(result.max_output_json_bytes) + "}";

    out += ",\"targets\":[";
    for (std::size_t i = 0; i < result.targets.size(); ++i) {
        if (i != 0) out += ',';
        const GoalSlot& target = result.targets[i];
        if (target.family_id != kNoId) {
            out += "{\"kind\":\"family\",\"family_id\":" +
                   std::to_string(target.family_id) +
                   ",\"min_tier\":" + std::to_string(target.min_tier) +
                   '}';
        } else {
            out += "{\"kind\":\"group\",\"group_id\":" +
                   std::to_string(target.group_id) + '}';
        }
    }
    out += ']';

    out += ",\"nodes\":[";
    for (std::size_t i = 0; i < result.nodes.size(); ++i) {
        if (i != 0) out += ',';
        const StrategyEvalNode& node = result.nodes[i];
        out += "{\"id\":\"" + json_escape(node.id) +
               "\",\"expected_visits\":";
        append_number(out, node.expected_visits);
        out += ",\"classes\":[";
        for (std::size_t c = 0; c < node.classes.size(); ++c) {
            if (c != 0) out += ',';
            const StrategyEvalClass& entry = node.classes[c];
            const AbstractState& state = entry.state;
            out += "{\"share\":";
            append_number(out, entry.share);
            out += ",\"rarity\":" + std::to_string(state.rarity) +
                   ",\"prefixes\":" +
                   std::to_string(state.prefix_count) +
                   ",\"suffixes\":" +
                   std::to_string(state.suffix_count) +
                   ",\"flags\":" + std::to_string(state.flags) +
                   ",\"blocked\":" +
                   std::to_string(state.blocked_mask) + ",\"slots\":[";
            for (std::size_t slot = 0; slot < result.targets.size(); ++slot) {
                if (slot != 0) out += ',';
                out += std::to_string(state.slot_status[slot]);
            }
            out += "]}";
        }
        out += "],\"classes_truncated_share\":";
        append_number(out, node.classes_truncated_share);
        out += '}';
    }
    out += ']';

    out += ",\"edges\":[";
    for (std::size_t i = 0; i < result.edges.size(); ++i) {
        if (i != 0) out += ',';
        const StrategyEvalEdge& edge = result.edges[i];
        out += "{\"id\":\"" + json_escape(edge.id) +
               "\",\"expected_traversals\":";
        append_number(out, edge.expected_traversals);
        out += '}';
    }
    out += "]}";
    return std::move(out).take();
}

} // namespace solver
} // namespace poecraft
