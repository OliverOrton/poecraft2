#pragma once

#include "solver_policy_refinement.hpp"

#include "poecraft/bitset.h"
#include "solver_sparse_policy.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <deque>
#include <map>
#include <memory>
#include <numeric>
#include <optional>
#include <set>
#include <stdexcept>
#include <utility>

namespace poecraft {
namespace solver {
namespace refinement {
namespace {

constexpr double kAbsoluteCostTolerance = 1e-7;
constexpr double kRelativeCostTolerance = 1e-9;
constexpr double kOffPolicyTolerance = 1e-10;

std::uint32_t bounded_u32(const std::uint64_t value) {
    return static_cast<std::uint32_t>(std::min<std::uint64_t>(
        value, std::numeric_limits<std::uint32_t>::max()));
}

void saturating_add(
        std::uint64_t& target,
        const std::uint64_t addition) {
    target = addition >
                     std::numeric_limits<std::uint64_t>::max() -
                         target
                 ? std::numeric_limits<std::uint64_t>::max()
                 : target + addition;
}

std::string resource_cap_from_message(const std::string& message) {
    static constexpr const char* names[] = {
        "max_discovered_states",
        "max_coarse_states",
        "max_exact_states",
        "max_states",
        "max_state_action_rows",
        "max_exact_kernels",
        "max_pairs",
        "max_transitions",
        "max_reforge_work",
        "max_solver_owned_bytes",
        "max_estimated_memory_bytes",
        "max_owned_bytes",
        "max_classes",
        "max_refinement_classes",
        "max_reachable_classes",
        "max_component_iterations",
        "max_refinement_rounds",
        "max_sweeps",
        "max_output_json_bytes",
        "max_compiled_nodes",
        "max_compiled_edges",
        "max_strategy_json_bytes",
    };
    for (const char* name : names) {
        if (message.find(name) != std::string::npos) return name;
    }
    /*
     * A standard-container length_error is an allocation/size failure. The
     * refinement layer owns no unnamed generic cap; attribute it to the
     * declared aggregate memory limit when the originating message does not
     * name a narrower resource.
     */
    return "max_estimated_memory_bytes";
}

std::string adapter_resource_cap_name(
        const std::string& cap_name) {
    return cap_name == "max_owned_bytes"
               ? "max_estimated_memory_bytes"
               : cap_name;
}

std::shared_ptr<const SessionImpl> borrow_session(CalcContext& calc) {
    /*
     * CalcContext deliberately exposes its immutable session by reference,
     * not shared ownership. Both children are call-scoped and destroyed
     * before `calc`, so this non-owning shared_ptr cannot escape.
     */
    return std::shared_ptr<const SessionImpl>(
        &calc.session(), [](const SessionImpl*) {});
}

bool reconciled(
        const double exact,
        const double expected,
        double& absolute,
        double& relative) {
    absolute = std::abs(exact - expected);
    relative = std::abs(expected) > 1e-12
                   ? absolute / std::abs(expected)
                   : absolute;
    return absolute <= kAbsoluteCostTolerance ||
           relative <= kRelativeCostTolerance;
}

StableKey exact_state_key(
        const AbstractState& state,
        const std::uint32_t coarse_parent) {
    return exact_abstract_state_key(
        state, coarse_parent);
}

StableKey exact_refinement_state_key(
        const AbstractState& state,
        const StableKey& coarse_parent_key) {
    StableKey key{
        0x7063727265667331ull, /* "pcrrefs1" */
        coarse_parent_key.size()};
    key.insert(
        key.end(),
        coarse_parent_key.begin(),
        coarse_parent_key.end());
    StableKey carrier = exact_abstract_state_key(state, 0);
    key.push_back(carrier.size());
    key.insert(
        key.end(), carrier.begin(), carrier.end());
    return key;
}

std::uint64_t stable_key_bytes(const StableKey& key) {
    return key.capacity() * sizeof(std::uint64_t);
}

std::uint64_t compiled_condition_nested_bytes(
        const CompiledCondition& condition,
        std::set<const CompiledObservationProgram*>&
            counted_programs) {
    std::uint64_t bytes =
        condition.mod_ids.capacity() * sizeof(std::uint32_t) +
        condition.family_ids.capacity() * sizeof(std::uint32_t) +
        condition.children.capacity() * sizeof(CompiledCondition);
    const CompiledObservationSignature& observation =
        condition.observation_signature;
    bytes += observation.modifier_tag_ids.capacity() *
             sizeof(std::uint32_t);
    bytes += observation.affix_observations.capacity() *
             sizeof(CompiledObservationAffixRequirement);
    for (const CompiledObservationAffixRequirement& affix :
         observation.affix_observations) {
        bytes += affix.selector.required_tag_ids.capacity() *
                 sizeof(std::uint32_t);
    }
    bytes += observation.atoms.capacity() *
             sizeof(CompiledObservationAtom);
    for (const CompiledObservationAtom& atom :
         observation.atoms) {
        bytes += atom.value.capacity() * sizeof(std::uint64_t);
    }
    const auto add_mod_values =
        [&](const std::vector<CompiledObservationModValue>& values) {
            bytes += values.capacity() *
                     sizeof(CompiledObservationModValue);
            for (const CompiledObservationModValue& value : values) {
                bytes += value.value.capacity() *
                         sizeof(std::uint64_t);
            }
        };
    add_mod_values(
        observation.goal_status_tier_class_by_mod);
    add_mod_values(
        observation.count_observation_membership_by_mod);
    if (condition.observation_program != nullptr &&
        counted_programs.insert(
            condition.observation_program.get()).second) {
        const CompiledObservationProgram& program =
            *condition.observation_program;
        bytes += sizeof(CompiledObservationProgram) +
                 2 * sizeof(void*);
        bytes += program.requirement.modifier_tag_ids.capacity() *
                 sizeof(std::uint32_t);
        bytes += program.requirement.affix_observations.capacity() *
                 sizeof(RefinementAffixObservation);
        for (const RefinementAffixObservation& affix :
             program.requirement.affix_observations) {
            bytes += affix.selector.required_tag_ids.capacity() *
                     sizeof(std::uint32_t);
        }
        bytes += program.signature.capacity() *
                 sizeof(FeatureAtom);
        for (const FeatureAtom& atom : program.signature) {
            bytes += atom.value.capacity() *
                         sizeof(std::uint64_t) +
                     atom.modifier_tag_ids.capacity() *
                         sizeof(std::uint32_t);
        }
        const auto add_context =
            [&](const std::vector<StableKey>& values) {
                bytes += values.capacity() * sizeof(StableKey);
                for (const StableKey& value : values) {
                    bytes += value.capacity() *
                             sizeof(std::uint64_t);
                }
            };
        add_context(
            program.context.goal_status_tier_class_by_mod);
        add_context(
            program.context
                .count_observation_membership_by_mod);
    }
    for (const CompiledCondition& child : condition.children) {
        bytes += compiled_condition_nested_bytes(
            child, counted_programs);
    }
    return bytes;
}

std::uint64_t strategy_impl_owned_bytes(
        const StrategyImpl& strategy) {
    std::set<const CompiledObservationProgram*>
        counted_observation_programs;
    std::uint64_t bytes =
        sizeof(StrategyImpl) + strategy.name.capacity() + 1;
    bytes += strategy.nodes.capacity() * sizeof(StrategyNode);
    bytes += strategy.node_by_id.bucket_count() * sizeof(void*);
    bytes += strategy.node_by_id.size() *
             (sizeof(decltype(strategy.node_by_id)::value_type) +
              2 * sizeof(void*));
    for (const auto& [key, unused] : strategy.node_by_id) {
        (void)unused;
        bytes += key.capacity() + 1;
    }
    for (const StrategyNode& node : strategy.nodes) {
        bytes += node.id.capacity() + node.reason.capacity() + 2;
        bytes += node.action.fossil_indices.capacity() *
                 sizeof(std::uint32_t);
        bytes += node.price_keys.capacity() * sizeof(std::string);
        for (const std::string& key : node.price_keys) {
            bytes += key.capacity() + 1;
        }
        bytes += node.accounting_roles.capacity() *
                 sizeof(std::string);
        for (const std::string& role : node.accounting_roles) {
            bytes += role.capacity() + 1;
        }
        bytes += node.edges.capacity() * sizeof(StrategyEdge);
        for (const StrategyEdge& edge : node.edges) {
            bytes += edge.id.capacity() + 1;
            bytes += edge.accounting_roles.capacity() *
                     sizeof(std::string);
            for (const std::string& role :
                 edge.accounting_roles) {
                bytes += role.capacity() + 1;
            }
            bytes += compiled_condition_nested_bytes(
                edge.condition,
                counted_observation_programs);
        }
        bytes += node.dispatch_conditions.capacity() *
                 sizeof(CompiledCondition);
        for (const CompiledCondition& condition :
             node.dispatch_conditions) {
            bytes += compiled_condition_nested_bytes(
                condition,
                counted_observation_programs);
        }
        bytes += node.dispatch_nodes.capacity() *
                 sizeof(StrategyDispatchNode);
        bytes += node.dispatch_signatures.bucket_count() *
                 sizeof(void*);
        bytes += node.dispatch_signatures.size() *
                 (sizeof(decltype(
                      node.dispatch_signatures)::value_type) +
                  2 * sizeof(void*));
        for (const auto& [unused, signatures] :
             node.dispatch_signatures) {
            (void)unused;
            bytes += signatures.capacity() *
                     sizeof(StrategyDispatchSignature);
            for (const StrategyDispatchSignature& signature :
                 signatures) {
                bytes += signature.true_conditions.capacity() *
                         sizeof(std::uint32_t);
            }
        }
        bytes += node.direct_dispatch_features.capacity() *
                 sizeof(CompiledCondition);
        for (const CompiledCondition& condition :
             node.direct_dispatch_features) {
            bytes += compiled_condition_nested_bytes(
                condition,
                counted_observation_programs);
        }
        bytes += node.direct_dispatch_signatures.bucket_count() *
                 sizeof(void*);
        bytes += node.direct_dispatch_signatures.size() *
                 (sizeof(decltype(
                      node.direct_dispatch_signatures)::value_type) +
                  2 * sizeof(void*));
        for (const auto& [unused, signatures] :
             node.direct_dispatch_signatures) {
            (void)unused;
            bytes += signatures.capacity() *
                     sizeof(StrategyDirectDispatchSignature);
            for (const StrategyDirectDispatchSignature& signature :
                 signatures) {
                bytes += signature.values.capacity() *
                         sizeof(std::int16_t);
            }
        }
    }
    return bytes + 3 * sizeof(void*);
}

std::uint64_t economy_owned_bytes(
        const std::unordered_map<std::string, double>& prices,
        const std::size_t id_capacity) {
    std::uint64_t bytes =
        sizeof(EconomyImpl) + id_capacity + 1 +
        prices.bucket_count() * sizeof(void*);
    bytes += prices.size() *
             (sizeof(std::pair<const std::string, double>) +
              2 * sizeof(void*));
    for (const auto& [key, unused] : prices) {
        (void)unused;
        bytes += key.capacity() + 1;
    }
    return bytes + 3 * sizeof(void*);
}

std::uint64_t requirement_bytes(
        const ObservationRequirement& requirement) {
    std::uint64_t bytes =
        requirement.modifier_tag_ids.capacity() *
        sizeof(std::uint32_t);
    bytes += requirement.affix_observations.capacity() *
             sizeof(RefinementAffixObservation);
    for (const RefinementAffixObservation& observation :
         requirement.affix_observations) {
        bytes += observation.selector.required_tag_ids.capacity() *
                 sizeof(std::uint32_t);
    }
    return bytes;
}

std::uint64_t feature_signature_bytes(
        const FeatureSignature& signature) {
    std::uint64_t bytes =
        signature.capacity() * sizeof(FeatureAtom);
    for (const FeatureAtom& atom : signature) {
        bytes += stable_key_bytes(atom.value);
        bytes += atom.modifier_tag_ids.capacity() *
                 sizeof(std::uint32_t);
    }
    return bytes;
}

std::uint64_t contract_bytes(
        const ActionRefinementContract& contract) {
    std::uint64_t bytes =
        contract.observed_modifier_tag_ids.capacity() *
        sizeof(std::uint32_t);
    bytes += contract.affix_observations.capacity() *
             sizeof(RefinementAffixObservation);
    for (const RefinementAffixObservation& observation :
         contract.affix_observations) {
        bytes += observation.selector.required_tag_ids.capacity() *
                 sizeof(std::uint32_t);
    }
    bytes += contract.item_affix_dependencies.capacity() *
             sizeof(RefinementItemAffixDependency);
    bytes += contract.affix_flows.capacity() *
             sizeof(RefinementAffixFlow);
    for (const RefinementAffixFlow& flow :
         contract.affix_flows) {
        bytes +=
            flow.source_selector.required_tag_ids.capacity() *
            sizeof(std::uint32_t);
    }
    const auto selector_bytes =
        [](const std::vector<RefinementAffixSelector>& selectors) {
            std::uint64_t selected =
                selectors.capacity() *
                sizeof(RefinementAffixSelector);
            for (const RefinementAffixSelector& selector :
                 selectors) {
                selected +=
                    selector.required_tag_ids.capacity() *
                    sizeof(std::uint32_t);
            }
            return selected;
        };
    bytes += selector_bytes(contract.preserved_affixes);
    bytes += selector_bytes(contract.destroyed_affixes);
    return bytes;
}

std::uint64_t selected_action_bytes(
        const SelectedAction& selected) {
    std::uint64_t bytes =
        stable_key_bytes(selected.semantic_key) +
        contract_bytes(selected.contract) +
        requirement_bytes(selected.routing_observes) +
        selected.ordered_program.capacity() *
            sizeof(ActionRefinementContract);
    for (const ActionRefinementContract& contract :
         selected.ordered_program) {
        saturating_add(bytes, contract_bytes(contract));
    }
    saturating_add(
        bytes,
        selected.execution_paths.capacity() *
            sizeof(std::vector<ActionRefinementContract>));
    for (const std::vector<ActionRefinementContract>& path :
         selected.execution_paths) {
        saturating_add(
            bytes,
            path.capacity() *
                sizeof(ActionRefinementContract));
        for (const ActionRefinementContract& contract : path) {
            saturating_add(bytes, contract_bytes(contract));
        }
    }
    return bytes;
}

std::uint64_t planner_runtime_semantics_bytes(
        const PlannerOperatorRuntimeSemantics& runtime) {
    std::uint64_t bytes =
        runtime.ordered_program.capacity() *
            sizeof(PlannerOperatorRuntimeStep) +
        runtime.action_dependencies.capacity() *
            sizeof(std::uint32_t) +
        contract_bytes(runtime.compatibility_refinement);
    for (const PlannerOperatorRuntimeStep& step :
         runtime.ordered_program) {
        saturating_add(bytes, contract_bytes(step.refinement));
    }
    saturating_add(
        bytes,
        runtime.execution_paths.capacity() *
            sizeof(std::vector<PlannerOperatorRuntimeStep>));
    for (const std::vector<PlannerOperatorRuntimeStep>& path :
         runtime.execution_paths) {
        saturating_add(
            bytes,
            path.capacity() *
                sizeof(PlannerOperatorRuntimeStep));
        for (const PlannerOperatorRuntimeStep& step : path) {
            saturating_add(bytes, contract_bytes(step.refinement));
        }
    }
    return bytes;
}

std::uint64_t u32_set_bytes(
        const std::set<std::uint32_t>& values) {
    return values.size() *
           (sizeof(std::uint32_t) + 3 * sizeof(void*));
}

std::uint64_t policy_observation_nodes_bytes(
        const std::vector<PolicyObservationNode>& nodes) {
    std::uint64_t bytes =
        nodes.capacity() * sizeof(PolicyObservationNode);
    for (const PolicyObservationNode& node : nodes) {
        if (node.selected_action.has_value()) {
            saturating_add(
                bytes, selected_action_bytes(*node.selected_action));
        }
        saturating_add(
            bytes,
            node.successors.capacity() * sizeof(std::uint32_t));
    }
    return bytes;
}

std::uint64_t policy_observation_fixed_bytes(
        const PolicyObservationFixedPoint& fixed) {
    std::uint64_t bytes =
        fixed.failure_reason.capacity() + 1 +
        fixed.assignments.capacity() *
            sizeof(PolicyObservationAssignment);
    for (const PolicyObservationAssignment& assignment :
         fixed.assignments) {
        saturating_add(bytes, requirement_bytes(assignment.required));
    }
    return bytes;
}

std::uint64_t exact_state_bytes(const ExactState& state) {
    return stable_key_bytes(state.stable_key) +
           stable_key_bytes(state.coarse_state_key) +
           feature_signature_bytes(state.features);
}

std::uint64_t exact_action_kernel_bytes(
        const ExactActionKernel& kernel) {
    std::uint64_t bytes =
        kernel.transitions.capacity() * sizeof(ExactTransition);
    for (const ExactTransition& transition : kernel.transitions) {
        saturating_add(
            bytes, exact_state_bytes(transition.successor));
    }
    return bytes;
}

std::uint64_t refinement_result_bytes(
        const RefinementResult& result) {
    std::uint64_t bytes =
        result.failure_reason.capacity() + 1 +
        result.resource_cap.capacity() + 1;
    bytes += result.assignments.capacity() *
             sizeof(StateClassAssignment);
    bytes += result.classes.capacity() *
             sizeof(RefinedPolicyClass);
    for (const RefinedPolicyClass& policy_class :
         result.classes) {
        bytes += stable_key_bytes(
            policy_class.coarse_state_key);
        bytes += policy_class.exact_members.capacity() *
                 sizeof(StableKey);
        for (const StableKey& member :
             policy_class.exact_members) {
            bytes += stable_key_bytes(member);
        }
        bytes += requirement_bytes(
            policy_class.required_observations);
        bytes += feature_signature_bytes(
            policy_class.observation_signature);
        if (policy_class.selected_action.has_value()) {
            bytes += selected_action_bytes(
                *policy_class.selected_action);
        }
        bytes += policy_class.transitions.capacity() *
                 sizeof(ProjectedTransition);
    }
    bytes += result.counterexamples.capacity() *
             sizeof(RefinementCounterexample);
    for (const RefinementCounterexample& counterexample :
         result.counterexamples) {
        bytes += stable_key_bytes(counterexample.left_state);
        bytes += stable_key_bytes(counterexample.right_state);
        bytes += feature_signature_bytes(
            counterexample.differing_features);
    }
    return bytes;
}

std::uint64_t refined_compile_routing_bytes(
        const RefinedPolicyCompileRouting& routing) {
    std::uint64_t bytes =
        routing.classes.capacity() *
        sizeof(RefinedPolicyCompileClass);
    for (const RefinedPolicyCompileClass& policy_class :
         routing.classes) {
        bytes += stable_key_bytes(
            policy_class.coarse_state_key);
        bytes += policy_class.strict_members.capacity() *
                 sizeof(std::uint32_t);
        bytes += requirement_bytes(
            policy_class.required_observations);
        bytes += feature_signature_bytes(
            policy_class.observation_signature);
        if (policy_class.selected_action.has_value()) {
            bytes += selected_action_bytes(
                *policy_class.selected_action);
        }
        bytes += policy_class.transitions.capacity() *
                 sizeof(ProjectedTransition);
    }
    return bytes;
}

std::uint64_t policy_evaluation_result_bytes(
        const PolicyEvaluationResult& result) {
    return result.failure_reason.capacity() + 1 +
           result.resource_cap.capacity() + 1 +
           result.class_values.capacity() *
               sizeof(RefinedClassValue) +
           result.start_values.capacity() * sizeof(double) +
           result.improper_component_classes.capacity() *
               sizeof(std::uint32_t);
}

struct AdapterFailure : std::runtime_error {
    AdapterFailure(
        const PolicyExactLiftStatus value_status,
        std::string message,
        std::string value_cap = {})
        : std::runtime_error(std::move(message)),
          status(value_status),
          cap(std::move(value_cap)) {}

    PolicyExactLiftStatus status;
    std::string cap;
};

struct CoarsePolicyNode {
    std::uint32_t coarse_operator = kNoId;
    std::optional<SelectedAction> selected;
    std::vector<std::uint32_t> successors;
    ObservationRequirement required;
    bool observations_propagated = false;
};

std::uint64_t coarse_policy_node_bytes(
        const CoarsePolicyNode& node) {
    std::uint64_t bytes =
        node.successors.capacity() * sizeof(std::uint32_t) +
        requirement_bytes(node.required);
    if (node.selected.has_value()) {
        saturating_add(bytes, selected_action_bytes(*node.selected));
    }
    return bytes;
}

struct ExactPolicyRun {
    RefinementResult refinement;
    PolicyEvaluationResult evaluation;
    std::vector<StableKey> roots;
    /*
     * Exact carrier witnesses for the current policy defect. Illegal
     * selected actions contribute their carrier directly. An improper
     * evaluator result is converted from quotient class ids immediately,
     * before the refinement that gives those ids can leave this run.
     */
    std::vector<StableKey> defect_witnesses;
    std::map<StableKey, double> value_by_exact;
    bool complete = false;

    double root_value() const {
        return evaluation.start_values.size() == 1
                   ? evaluation.start_values.front()
                   : std::numeric_limits<double>::infinity();
    }
};

struct PolicyMutation {
    const StableKey* key = nullptr;
    bool had_override = false;
    std::optional<SelectedAction> previous;
};

enum class ExactChoiceRecipeKind : std::uint8_t {
    None = 0,
    PrimitiveObservedChoice = 1,
    FixedOption = 2,
};

/*
 * One exact carrier's immutable observe-then-decide program. The selected
 * semantic key names this recipe, and every later consumer resolves choices
 * from this stored authority instead of rebuilding a preference from coarse
 * values.
 */
struct ExactChoiceRecipe {
    ExactChoiceRecipeKind kind = ExactChoiceRecipeKind::None;
    std::vector<std::uint32_t> primitive_preferences;
    std::vector<ObservedUnveilPreference> option_preferences;

    bool operator==(const ExactChoiceRecipe&) const = default;
};

struct OperatorVocabularyWideningWitness {
    std::uint32_t coarse_state = kNoId;
    std::uint32_t coarse_operator = kNoId;
};

struct ReoptimizationSeed {
    std::set<std::uint32_t> coarse_parents;
    std::map<std::uint32_t, RefinementFeatureMask>
        causal_features_by_parent;

    bool empty() const {
        return coarse_parents.empty();
    }

    void merge(const ReoptimizationSeed& other) {
        coarse_parents.insert(
            other.coarse_parents.begin(),
            other.coarse_parents.end());
        for (const auto& [parent, features] :
             other.causal_features_by_parent) {
            causal_features_by_parent[parent] |= features;
        }
    }
};

/*
 * One candidate operator is compared only inside an incumbent exact policy
 * class.  The shared closed-partition engine proves which of those concrete
 * members have the same candidate observation, semantic decision, exact
 * price, and strict successor row before one Bellman comparison is reused for
 * the resulting subclass.  `candidate_by_exact` deliberately retains the
 * exact decision for every member; no modifier identity is selected as a
 * representative implementation.
 */
struct CandidateExactPartition {
    std::map<StableKey, SelectedAction> candidate_by_exact;
    std::vector<std::vector<StableKey>> subclasses;
};

std::uint64_t candidate_exact_partition_bytes(
        const CandidateExactPartition& partition) {
    std::uint64_t bytes =
        partition.candidate_by_exact.size() *
            (sizeof(decltype(
                 partition.candidate_by_exact)::value_type) +
             3 * sizeof(void*)) +
        partition.subclasses.capacity() *
            sizeof(std::vector<StableKey>);
    for (const auto& [key, selected] :
         partition.candidate_by_exact) {
        saturating_add(bytes, stable_key_bytes(key));
        saturating_add(bytes, selected_action_bytes(selected));
    }
    for (const std::vector<StableKey>& members :
         partition.subclasses) {
        saturating_add(
            bytes,
            members.capacity() * sizeof(StableKey));
        for (const StableKey& member : members) {
            saturating_add(bytes, stable_key_bytes(member));
        }
    }
    return bytes;
}

std::uint64_t reoptimization_seed_bytes(
        const ReoptimizationSeed& seed) {
    std::uint64_t bytes =
        seed.coarse_parents.size() *
        (sizeof(std::uint32_t) + 3 * sizeof(void*));
    saturating_add(
        bytes,
        seed.causal_features_by_parent.size() *
            (sizeof(std::pair<
                 const std::uint32_t,
                 RefinementFeatureMask>) +
             3 * sizeof(void*)));
    return bytes;
}

std::uint64_t exact_choice_recipe_bytes(
        const ExactChoiceRecipe& recipe) {
    std::uint64_t bytes =
        recipe.primitive_preferences.capacity() *
        sizeof(std::uint32_t);
    saturating_add(
        bytes,
        recipe.option_preferences.capacity() *
            sizeof(ObservedUnveilPreference));
    for (const ObservedUnveilPreference& preference :
         recipe.option_preferences) {
        saturating_add(
            bytes,
            preference.choices.capacity() *
                sizeof(ObservedUnveilChoice));
    }
    return bytes;
}

/*
 * Exact observed choices use the same deterministic selector as ordinary
 * sparse Bellman rows. Local ids are assigned in strict-successor-id order,
 * with modifier id breaking duplicate-successor ties, so the shared total
 * order is identical to strict Bellman selection. Repeated selection
 * produces the complete immutable preference order consumed by both the
 * exact kernel and compiler.
 */
template <typename Offer, typename ExactValue>
void order_observed_offers_by_sparse_choice(
        std::vector<Offer>& offers,
        ExactValue&& exact_value) {
    std::sort(
        offers.begin(), offers.end(),
        [](const Offer& left, const Offer& right) {
            return std::tie(
                       left.successor_state,
                       left.mod_id) <
                   std::tie(
                       right.successor_state,
                       right.mod_id);
        });
    SolveTransitionCache graph;
    std::vector<double> values(offers.size(), kInfinity);
    std::vector<std::uint32_t> remaining(offers.size());
    std::iota(remaining.begin(), remaining.end(), 0);
    for (std::size_t index = 0; index < offers.size(); ++index) {
        values[index] = exact_value(offers[index]);
        if (std::isnan(values[index])) {
            throw AdapterFailure(
                PolicyExactLiftStatus::InvalidSolveState,
                "observed exact continuation has a NaN value");
        }
    }
    std::vector<Offer> ordered;
    ordered.reserve(offers.size());
    while (!remaining.empty()) {
        graph.choice_successors = remaining;
        SparseChoiceGroup choice;
        choice.successor_offset = 0;
        choice.successor_count = static_cast<std::uint32_t>(
            graph.choice_successors.size());
        const std::uint32_t selected =
            solve_detail::select_sparse_policy_choice_successor(
                graph, choice, 0, values);
        if (selected == kNoId || selected >= offers.size()) {
            throw AdapterFailure(
                PolicyExactLiftStatus::InvalidSolveState,
                "shared sparse choice selector found no exact offer");
        }
        ordered.push_back(std::move(offers[selected]));
        const auto found =
            std::find(remaining.begin(), remaining.end(), selected);
        if (found == remaining.end()) {
            throw AdapterFailure(
                PolicyExactLiftStatus::InvalidSolveState,
                "shared sparse choice selector returned a stale offer");
        }
        remaining.erase(found);
    }
    offers = std::move(ordered);
}

std::uint64_t stable_keys_bytes(
        const std::vector<StableKey>& keys) {
    std::uint64_t bytes =
        keys.capacity() * sizeof(StableKey);
    for (const StableKey& key : keys) {
        saturating_add(bytes, stable_key_bytes(key));
    }
    return bytes;
}

std::uint64_t stable_key_set_bytes(
        const std::set<StableKey>& keys) {
    std::uint64_t bytes =
        keys.size() *
        (sizeof(StableKey) + 3 * sizeof(void*));
    for (const StableKey& key : keys) {
        saturating_add(bytes, stable_key_bytes(key));
    }
    return bytes;
}

std::uint64_t exact_policy_run_bytes(
        const ExactPolicyRun& run) {
    std::uint64_t bytes = sizeof(ExactPolicyRun);
    saturating_add(
        bytes, refinement_result_bytes(run.refinement));
    saturating_add(
        bytes,
        policy_evaluation_result_bytes(run.evaluation));
    saturating_add(bytes, stable_keys_bytes(run.roots));
    saturating_add(
        bytes, stable_keys_bytes(run.defect_witnesses));
    saturating_add(
        bytes,
        run.value_by_exact.size() *
            (sizeof(decltype(
                 run.value_by_exact)::value_type) +
             3 * sizeof(void*)));
    for (const auto& [key, unused] :
         run.value_by_exact) {
        (void)unused;
        saturating_add(bytes, stable_key_bytes(key));
    }
    return bytes;
}

std::uint64_t changed_exact_value_count(
        const std::map<StableKey, double>& before,
        const std::map<StableKey, double>& after) {
    std::uint64_t changed = 0;
    auto left = before.begin();
    auto right = after.begin();
    while (left != before.end() || right != after.end()) {
        if (right == after.end() ||
            (left != before.end() && left->first < right->first)) {
            ++changed;
            ++left;
            continue;
        }
        if (left == before.end() || right->first < left->first) {
            ++changed;
            ++right;
            continue;
        }
        const double before_value = left->second;
        const double after_value = right->second;
        bool equal = before_value == after_value;
        if (!equal && std::isfinite(before_value) &&
            std::isfinite(after_value)) {
            double absolute = 0.0;
            double relative = 0.0;
            equal = reconciled(
                after_value, before_value, absolute, relative);
        }
        if (!equal) ++changed;
        ++left;
        ++right;
    }
    return changed;
}

std::uint64_t policy_mutations_bytes(
        const std::vector<PolicyMutation>& mutations) {
    std::uint64_t bytes =
        mutations.capacity() * sizeof(PolicyMutation);
    for (const PolicyMutation& mutation : mutations) {
        if (mutation.previous.has_value()) {
            saturating_add(
                bytes,
                selected_action_bytes(
                    *mutation.previous));
        }
    }
    return bytes;
}


} // namespace

} // namespace refinement
} // namespace solver
} // namespace poecraft
