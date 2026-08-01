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

class ProductionPolicyOracle final : public PolicyRefinementOracle {
  public:
    ProductionPolicyOracle(
        CalcContext& coarse,
        const SolveResult& solved,
        const pc_item_state& exact_start,
        const std::unordered_map<std::string, double>& prices,
        const SolveOptions& options,
        RefinementLimits limits,
        PolicyLiftAdapterTelemetry& telemetry,
        const ReoptimizationSeed* reoptimization_seed)
        : coarse_(coarse),
          solved_(solved),
          exact_start_(exact_start),
          prices_(prices),
          options_(options),
          limits_(std::move(limits)),
          telemetry_(telemetry),
          coarse_retained_bytes_at_entry_(
              coarse.fast_estimated_owned_bytes()),
          borrowed_session_(borrow_session(coarse_)),
          enable_local_reoptimization_(
              reoptimization_seed != nullptr),
          reoptimization_parents_(
              reoptimization_seed == nullptr
                  ? std::set<std::uint32_t>{}
                  : reoptimization_seed->coarse_parents),
          caller_seed_retained_bytes_(
              reoptimization_seed == nullptr
                  ? 0
                  : reoptimization_seed_bytes(
                        *reoptimization_seed)) {
        require_local_memory(
            0, "exact-refinement adapter construction");
        discover_coarse_policy();
        index_coarse_states();
        /*
         * The strict child retains the solver's already-filtered operator
         * vocabulary, not merely its primitive leaves. Only the dependency
         * primitives participate in strict layout construction; complete
         * PlannerOperators are imported after construction and their kernels
         * remain lazy.
         */
        const auto append_admitted_coarse_operator =
            [&](const std::uint32_t operator_index) {
                admitted_coarse_operators_.push_back(operator_index);
                require_local_memory(
                    0, "filtered planner vocabulary construction");
            };
        if (enable_local_reoptimization_) {
            for (const std::uint32_t parent :
                 reoptimization_parents_) {
                if (parent >= coarse_.state_count()) {
                    throw AdapterFailure(
                        PolicyExactLiftStatus::InvalidSolveState,
                        "local reoptimization seed names an unknown "
                        "coarse state");
                }
            }
            for (const std::uint32_t operator_index :
                 coarse_.candidate_operators()) {
                const bool admitted = std::any_of(
                    reoptimization_parents_.begin(),
                    reoptimization_parents_.end(),
                    [&](const std::uint32_t parent) {
                        return coarse_
                            .is_candidate_operator_admitted_for_state(
                                parent, operator_index);
                    });
                if (admitted) {
                    append_admitted_coarse_operator(
                        operator_index);
                }
            }
        } else {
            for (const auto& [unused, node] :
                 coarse_policy_) {
                (void)unused;
                if (node.coarse_operator != kNoId) {
                    append_admitted_coarse_operator(
                        node.coarse_operator);
                }
            }
        }
        for (const auto& [unused, node] : coarse_policy_) {
            (void)unused;
            if (node.coarse_operator != kNoId) {
                append_admitted_coarse_operator(
                    node.coarse_operator);
            }
        }
        std::sort(
            admitted_coarse_operators_.begin(),
            admitted_coarse_operators_.end());
        admitted_coarse_operators_.erase(
            std::unique(
                admitted_coarse_operators_.begin(),
                admitted_coarse_operators_.end()),
            admitted_coarse_operators_.end());
        for (const std::uint32_t operator_index :
             admitted_coarse_operators_) {
            if (operator_index >= coarse_.operators().size()) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::InvalidSolveState,
                    "admitted planner operator is outside the filtered "
                    "vocabulary");
            }
            const PlannerOperator& planner =
                coarse_.operators()[operator_index];
            const PlannerOperatorRuntimeSemantics runtime =
                operator_runtime_semantics(planner);
            require_local_memory(
                planner_runtime_semantics_bytes(runtime),
                "planner runtime-semantics construction");
            SelectedAction runtime_selection;
            populate_ordered_runtime_contracts(
                runtime_selection, runtime);
            carrier_requirement_ = merge_observation_requirements(
                std::move(carrier_requirement_),
                observation_requirement_from_selected_action(
                    runtime_selection));
            for (const std::uint32_t action :
                 runtime.action_dependencies) {
                admitted_actions_.push_back(action);
                require_local_memory(
                    planner_runtime_semantics_bytes(runtime) +
                        selected_action_bytes(runtime_selection),
                    "strict primitive vocabulary construction");
            }
        }
        std::sort(admitted_actions_.begin(), admitted_actions_.end());
        admitted_actions_.erase(
            std::unique(
                admitted_actions_.begin(), admitted_actions_.end()),
            admitted_actions_.end());
        std::vector<std::uint64_t> exact_start_mods(
            coarse_.session().words, 0);
        require_local_memory(
            exact_start_mods.capacity() * sizeof(std::uint64_t),
            "strict start-modifier mask construction");
        const auto retain_start_mods =
            [&](const pc_mod_slot* slots,
                const std::uint8_t count) {
                for (std::uint8_t i = 0; i < count; ++i) {
                    if (slots[i].mod_id <
                        coarse_.session().mod_count) {
                        pc_bitset_set(
                            exact_start_mods.data(),
                            slots[i].mod_id);
                    }
                }
            };
        retain_start_mods(
            exact_start.prefixes, exact_start.prefix_count);
        retain_start_mods(
            exact_start.suffixes, exact_start.suffix_count);
        strict_ = std::make_unique<CalcContext>(
            borrowed_session_,
            coarse_.goal(),
            coarse_.registry(),
            admitted_actions_,
            false,
            false,
            true,
            std::optional<std::uint32_t>{
                std::max<std::uint32_t>(
                    1, limits_.max_exact_states)},
            std::vector<CountObservation>{},
            false,
            exact_start_mods,
            false); /* contract-derived semantic strict carrier */
        require_local_memory(
            exact_start_mods.capacity() * sizeof(std::uint64_t),
            "strict calculation construction");
        refresh_strict_resource_cap(
            "strict calculation construction",
            exact_start_mods.capacity() * sizeof(std::uint64_t));
        import_admitted_operators();
        for (auto& [state, node] : coarse_policy_) {
            if (node.coarse_operator == kNoId) continue;
            node.selected = make_selected(
                state,
                strict_operator_for(node.coarse_operator),
                std::nullopt);
            require_local_memory(
                0, "coarse selected-action construction");
        }
        propagate_backward_observations();
        for (const auto& [unused, node] : coarse_policy_) {
            (void)unused;
            carrier_requirement_ =
                merge_observation_requirements(
                    std::move(carrier_requirement_),
                    node.required);
            require_local_memory(
                0, "policy carrier-requirement construction");
        }
        build_strict_to_coarse_junk_map();

        const std::uint32_t strict_start =
            strict_->intern_item(exact_start);
        const std::uint32_t coarse_start =
            coarse_parent(strict_start);
        if (coarse_start != solved_.start_state) {
            throw AdapterFailure(
                PolicyExactLiftStatus::InvalidSolveState,
                "exact refinement start item does not project to the "
                "solved start state");
        }
        auto [root, inserted] = intern_strict_state(strict_start);
        (void)inserted;
        root_key_ = root.stable_key;
        require_local_memory(
            exact_state_bytes(root),
            "exact-refinement root construction");
    }

    const StableKey& root_key() const {
        return root_key_;
    }

    ExactPolicyRun evaluate_current_policy(
        std::vector<StableKey> exact_roots,
        std::uint64_t caller_retained_bytes = 0);

    ExactPolicyRun repair_invalid_policy();

    ExactPolicyRun improve_policy(
        ExactPolicyRun incumbent);

    ExactPolicyRun evaluate_fixed_policy() {
        return evaluate_current_policy({root_key_});
    }

    bool has_policy_defect_witness(
            const ExactPolicyRun& run) const {
        return !run.defect_witnesses.empty();
    }

    bool requires_operator_vocabulary_widening(
            const ExactPolicyRun& run) const {
        (void)run;
        return operator_vocabulary_widening_witness_.has_value();
    }

    ReoptimizationSeed reoptimization_seed(
            const ExactPolicyRun& run) const;

    bool has_local_reoptimization_witness(
            const ExactPolicyRun& run) const {
        return run.complete &&
               !mismatch_witnesses(run).empty();
    }

    bool final_policy_changed(
        const RefinementResult& refinement) const {
        for (const StateClassAssignment& assignment :
             refinement.assignments) {
            const StableKey& exact_state =
                assignment_exact_state(refinement, assignment);
            const auto known =
                known_.find(exact_state);
            if (known == known_.end() ||
                known->second.terminal) {
                continue;
            }
            const auto parent = coarse_policy_.find(
                assignment.coarse_state);
            if (parent == coarse_policy_.end() ||
                !parent->second.selected.has_value()) {
                return true;
            }
            const auto exact =
                exact_policy_.find(
                    exact_state);
            if (exact != exact_policy_.end() &&
                exact->second.semantic_key !=
                    parent->second.selected
                        ->semantic_key) {
                return true;
            }
        }
        return false;
    }

    CompiledPolicyAssertion assert_lifted_policy(
            const std::string& strategy_name,
            const RefinementResult& refinement,
            const PolicyEvaluationResult& evaluation) {
        SolveOptions remaining_options = options_;
        const std::uint64_t coarse_bytes =
            estimated_retained_solver_bytes(coarse_, &solved_);
        const std::uint64_t strict_bytes =
            strict_->audited_estimated_owned_bytes();
        const std::uint64_t adapter_bytes =
            audit_adapter_owned_bytes();
        const std::uint64_t adapter_cache_bytes =
            adapter_bytes > strict_bytes
                ? adapter_bytes - strict_bytes
                : 0;
        const std::uint64_t refinement_bytes =
            refinement_result_bytes(refinement);
        const std::uint64_t evaluation_bytes =
            policy_evaluation_result_bytes(evaluation);
        const std::uint64_t projected_lift_bytes =
            projected_lifted_construction_bytes(refinement);
        std::uint64_t projected_construction_live =
            coarse_bytes;
        const auto add_projected =
            [&](const std::uint64_t bytes) {
                if (bytes >
                    std::numeric_limits<std::uint64_t>::max() -
                        projected_construction_live) {
                    projected_construction_live =
                        std::numeric_limits<std::uint64_t>::max();
                } else {
                    projected_construction_live += bytes;
                }
            };
        add_projected(adapter_bytes);
        add_projected(refinement_bytes);
        add_projected(evaluation_bytes);
        add_projected(projected_lift_bytes);
        if (projected_construction_live >=
            remaining_options.max_solver_owned_bytes) {
            CompiledPolicyAssertion capped;
            capped.status =
                CompiledPolicyAssertionStatus::ResourceCap;
            capped.resource_cap = "max_solver_owned_bytes";
            capped.failure_reason =
                "projected lifted policy construction reached "
                "max_solver_owned_bytes";
            capped.publication_peak_owned_bytes =
                projected_construction_live;
            return capped;
        }
        std::uint64_t required_before_lift = coarse_bytes;
        const auto add_required =
            [&](const std::uint64_t bytes) {
                if (bytes >
                    std::numeric_limits<std::uint64_t>::max() -
                        required_before_lift) {
                    required_before_lift =
                        std::numeric_limits<std::uint64_t>::max();
                } else {
                    required_before_lift += bytes;
                }
            };
        add_required(adapter_cache_bytes);
        add_required(refinement_bytes);
        add_required(evaluation_bytes);
        if (required_before_lift >=
            remaining_options.max_solver_owned_bytes) {
            CompiledPolicyAssertion capped;
            capped.status =
                CompiledPolicyAssertionStatus::ResourceCap;
            capped.resource_cap = "max_solver_owned_bytes";
            capped.failure_reason =
                "live coarse/refinement state leaves no memory for the "
                "lifted strict artifact";
            capped.publication_peak_owned_bytes =
                required_before_lift;
            return capped;
        }
        remaining_options.max_solver_owned_bytes -=
            required_before_lift;
        RefinedPolicyCompileRouting refined_routing;
        SolveResult lifted =
            make_lifted_solve_result(
                refinement, evaluation, &refined_routing);
        const std::uint64_t refined_routing_bytes =
            refined_compile_routing_bytes(refined_routing);
        if (refined_routing_bytes >=
            remaining_options.max_solver_owned_bytes) {
            CompiledPolicyAssertion capped;
            capped.status =
                CompiledPolicyAssertionStatus::ResourceCap;
            capped.resource_cap = "max_solver_owned_bytes";
            capped.failure_reason =
                "refined compile routing leaves no memory for the "
                "compiled exact artifact";
            capped.publication_peak_owned_bytes =
                required_before_lift;
            saturating_add(
                capped.publication_peak_owned_bytes,
                refined_routing_bytes);
            return capped;
        }
        remaining_options.max_solver_owned_bytes -=
            refined_routing_bytes;
        const std::uint64_t strict_reforge_work =
            std::min(
                strict_->telemetry().reforge_frontier_work,
                remaining_options.max_reforge_work);
        remaining_options.max_reforge_work -=
            strict_reforge_work;
        CompiledPolicyAssertion compiled =
            assert_compiled_policy_exact(
            *strict_,
            lifted,
            prices_,
            remaining_options,
            strategy_name,
            &refined_routing);
        std::uint64_t publication_compile_peak =
            required_before_lift;
        saturating_add(
            publication_compile_peak,
            refined_routing_bytes);
        saturating_add(
            publication_compile_peak,
            compiled.publication_peak_owned_bytes);
        compiled.publication_peak_owned_bytes =
            std::max(
                projected_construction_live,
                publication_compile_peak);
        if (compiled.status ==
                CompiledPolicyAssertionStatus::Complete) {
            std::uint32_t expected_working_classes = 0;
            for (const RefinedClassValue& value :
                 evaluation.class_values) {
                if (value.class_id >= refinement.classes.size() ||
                    refinement.classes[value.class_id].class_id !=
                        value.class_id) {
                    compiled.status =
                        CompiledPolicyAssertionStatus::
                            CompilationFailure;
                    compiled.executable = false;
                    compiled.failure_reason =
                        "exact evaluator returned a non-canonical "
                        "refinement class id";
                    return compiled;
                }
                if (!refinement.classes[value.class_id].terminal) {
                    ++expected_working_classes;
                }
            }
            if (compiled.compilation.working_states !=
                expected_working_classes) {
                compiled.status =
                    CompiledPolicyAssertionStatus::
                        CompilationFailure;
                compiled.executable = false;
                compiled.failure_reason =
                    "compiled exact quotient working-state count does "
                    "not match reachable non-terminal refinement "
                    "classes";
            }
        }
        return compiled;
    }

    std::vector<ExactState> enumerate_refinements(
            const std::uint32_t coarse_state) override {
        if (exact_root_filter_.has_value()) {
            const auto filtered =
                exact_root_filter_->find(coarse_state);
            if (filtered == exact_root_filter_->end()) return {};
            std::vector<ExactState> result;
            result.reserve(filtered->second.size());
            for (const StableKey& key : filtered->second) {
                result.push_back(materialize_exact_state(
                    key, known_.at(key)));
            }
            return result;
        }
        if (root_key_.empty()) return {};
        const KnownState& root = known_.at(root_key_);
        return root.coarse_state == coarse_state
                   ? std::vector<ExactState>{
                         materialize_exact_state(root_key_, root)}
                   : std::vector<ExactState>{};
    }

    std::optional<SelectedAction> selected_action(
            const ExactState& state) override {
        if (state.terminal) return std::nullopt;
        const auto exact = exact_policy_.find(state.stable_key);
        if (exact != exact_policy_.end()) return exact->second;
        const auto found = coarse_policy_.find(state.coarse_state);
        const auto carrier = known_.find(state.stable_key);
        if (found == coarse_policy_.end() ||
            carrier == known_.end() ||
            found->second.coarse_operator == kNoId) {
            throw AdapterFailure(
                PolicyExactLiftStatus::InvalidSolveState,
                "exact state maps outside the reachable coarse policy");
        }
        const std::uint32_t strict_operator = strict_operator_for(
            found->second.coarse_operator);
        SelectedAction selected = make_selected(
            state.coarse_state,
            strict_operator,
            carrier->second.strict_state);
        const StableKey raw_key = exact_refinement_state_key(
            strict_->state(carrier->second.strict_state),
            state.coarse_state_key);
        const PlannerOperator& selected_planner =
            strict_->operators().at(selected.action_id);
        if (raw_key != state.stable_key &&
            !has_proven_unobserved_primitive_recipe(
                carrier->second.strict_state,
                selected_planner)) {
            store_choice_recipe(
                state.stable_key,
                selected.semantic_key,
                choice_recipe(raw_key, selected));
        }
        return selected;
    }

    std::optional<StableKey> exact_kernel_reuse_key(
            const ExactState& state,
            const SelectedAction& selected) override {
        const auto known = known_.find(state.stable_key);
        if (known == known_.end() ||
            selected.action_id >= strict_->operators().size()) {
            return std::nullopt;
        }
        const PlannerOperator& planner =
            strict_->operators()[selected.action_id];
        if (planner.kind != PlannerOperatorKind::Primitive) {
            return std::nullopt;
        }
        const std::uint32_t action = planner.primitive_action;
        std::optional<StableKey> key = stable_reforge_reuse_key(
            known->second.strict_state, action);
        if (!key.has_value()) {
            return std::nullopt;
        }
        if (stable_kernel_reuse_keys_.contains(*key)) {
            return key;
        }
        refresh_strict_resource_cap(
            "strict shared-kernel reuse proof");
        const OutcomeDistribution& distribution =
            strict_->outcomes(
                known->second.strict_state, action,
                options_.goal_progress_gated_reforges);
        if (!distribution.supported ||
            !distribution.stable_shared_kernel ||
            !distribution.choice_groups.empty() ||
            !distribution.choice_options.empty()) {
            return std::nullopt;
        }
        require_local_memory(
            sizeof(StableKey) + 3 * sizeof(void*) +
                stable_key_bytes(*key),
            "strict shared-kernel reuse proof");
        stable_kernel_reuse_keys_.insert(*key);
        return key;
    }

    void note_exact_kernel_reuse() override {
        ++telemetry_.strict_kernel_cache_hits;
    }

    ExactActionKernel exact_kernel(
            const ExactState& state,
            const SelectedAction& selected) override {
        /*
         * Only kernels requested by the shared current-policy refinement may
         * establish an incumbent defect witness. Candidate Bellman kernels
         * call exact_kernel_ref() directly, so an inadmissible alternative
         * can never poison the policy repair worklist.
         */
        const auto known = known_.find(state.stable_key);
        if (known != known_.end() &&
            selected.action_id < strict_->operators().size()) {
            const PlannerOperator& planner =
                strict_->operators()[selected.action_id];
            if (planner.kind == PlannerOperatorKind::Primitive) {
                if (!action_legal(
                        strict_->session(),
                        strict_->registry().actions[
                            planner.primitive_action],
                        strict_->state(
                            known->second.strict_state))) {
                    illegal_action_witness_ = state.stable_key;
                }
            } else {
                refresh_strict_resource_cap(
                    "strict selected-option legality");
                const OptionKernel& kernel =
                    strict_->option_kernel(
                        known->second.strict_state,
                        selected.action_id);
                if (!kernel.supported || !kernel.legal ||
                    !kernel.terminates_almost_surely) {
                    illegal_action_witness_ = state.stable_key;
                }
            }
        }
        try {
            const KernelKey key{
                state.stable_key, selected.semantic_key};
            const auto cached = kernels_.find(key);
            if (cached != kernels_.end()) {
                ++telemetry_.strict_kernel_cache_hits;
                return cached->second;
            }
            ExactActionKernel kernel =
                build_counted_kernel(state, selected);
            /*
             * The oracle API returns a kernel by value. Retaining a second
             * adapter-owned copy until every later policy pass ends consumes
             * the publication budget without saving work. Release only this
             * published strict row; unrelated already-paid rows must remain
             * cached so publication cannot spend the reforge-work cap twice.
             * Candidate Bellman rows still use exact_kernel_ref() because
             * their caller requires stable retained storage across
             * comparisons.
             */
            release_published_kernel_storage(state, selected);
            return kernel;
        } catch (const AdapterFailure& error) {
            /*
             * A legal coarse decision can still fail only on one concrete
             * refinement (missing observation, unsupported exact kernel, or
             * a successor outside the solved coarse carrier). Preserve that
             * state as the compatibility counterexample so pass two repairs
             * the decision instead of terminating as a generic oracle error.
             * Resource and internal-contract failures remain fatal.
             */
            if (known != known_.end() &&
                (error.status ==
                     PolicyExactLiftStatus::ObservationUnavailable ||
                 error.status ==
                     PolicyExactLiftStatus::
                         UnsupportedPrimitiveKernel ||
                 error.status ==
                     PolicyExactLiftStatus::CoarseMappingFailure)) {
                illegal_action_witness_ = state.stable_key;
            }
            throw;
        }
    }

    const ExactActionKernel& exact_kernel_ref(
            const ExactState& state,
            const SelectedAction& selected) {
        const KernelKey key{
            state.stable_key, selected.semantic_key};
        const auto cached = kernels_.find(key);
        if (cached != kernels_.end()) {
            ++telemetry_.strict_kernel_cache_hits;
            return cached->second;
        }
        ExactActionKernel kernel =
            build_counted_kernel(state, selected);
        return cache_kernel(key, std::move(kernel));
    }

    ExactActionKernel build_counted_kernel(
            const ExactState& state,
            const SelectedAction& selected) {
        if (telemetry_.strict_kernels_built >=
            limits_.max_exact_kernels) {
            throw AdapterFailure(
                PolicyExactLiftStatus::ResourceCap,
                "strict policy discovery reached max_exact_kernels",
                "max_exact_kernels");
        }
        ExactActionKernel kernel;
        try {
            kernel = build_kernel(state.stable_key, selected);
        } catch (const AdapterFailure& error) {
            if (error.status == PolicyExactLiftStatus::ResourceCap) {
                throw RefinementOracleResourceLimit(
                    error.cap.empty()
                        ? "max_estimated_memory_bytes"
                        : error.cap,
                    error.what());
            }
            throw;
        } catch (const SolverResourceLimit& error) {
            throw RefinementOracleResourceLimit(
                adapter_resource_cap_name(error.cap_name()),
                error.what());
        }
        if (telemetry_.strict_transitions_built +
                kernel.transitions.size() >
            limits_.max_transitions) {
            throw AdapterFailure(
                PolicyExactLiftStatus::ResourceCap,
                "strict policy discovery reached max_transitions",
                "max_transitions");
        }
        telemetry_.strict_transitions_built +=
            kernel.transitions.size();
        ++telemetry_.strict_kernels_built;
        telemetry_.strict_calc_owned_bytes =
            strict_->fast_estimated_owned_bytes();
        return kernel;
    }

    void release_published_kernel_storage(
            const ExactState& state,
            const SelectedAction& selected) {
        const auto known = known_.find(state.stable_key);
        if (known == known_.end() ||
            selected.action_id >= strict_->operators().size()) {
            return;
        }
        const PlannerOperator& planner =
            strict_->operators()[selected.action_id];
        if (planner.kind == PlannerOperatorKind::Primitive) {
            strict_->release_published_outcome_storage(
                known->second.strict_state,
                planner.primitive_action,
                options_.goal_progress_gated_reforges);
        } else {
            strict_->release_option_kernel(
                known->second.strict_state,
                selected.action_id);
        }
    }

    const ExactActionKernel& cache_kernel(
            const std::pair<StableKey, StableKey>& key,
            ExactActionKernel kernel) {
        std::uint64_t projected =
            sizeof(decltype(kernels_)::value_type) +
            3 * sizeof(void*);
        saturating_add(
            projected, exact_action_kernel_bytes(kernel));
        saturating_add(
            projected, stable_key_bytes(key.first));
        saturating_add(
            projected, stable_key_bytes(key.second));
        saturating_add(
            projected, stable_key_bytes(key.first));
        saturating_add(
            projected, stable_key_bytes(key.second));
        require_local_memory(
            projected, "strict exact-kernel cache construction");
        auto [stored, inserted] = kernels_.emplace(
            key, std::move(kernel));
        if (!inserted) {
            throw AdapterFailure(
                PolicyExactLiftStatus::InvalidSolveState,
                "strict policy kernel cache identity changed");
        }
        std::uint64_t local_key_bytes =
            stable_key_bytes(key.first);
        saturating_add(
            local_key_bytes, stable_key_bytes(key.second));
        require_local_memory(
            local_key_bytes,
            "strict exact-kernel cache construction");
        return stored->second;
    }

    std::uint64_t dynamic_child_owned_bytes() const {
        std::uint64_t bytes = 0;
        const std::uint64_t current_coarse_bytes =
            coarse_.fast_estimated_owned_bytes();
        if (current_coarse_bytes >
            coarse_retained_bytes_at_entry_) {
            saturating_add(
                bytes,
                current_coarse_bytes -
                    coarse_retained_bytes_at_entry_);
        }
        if (strict_ != nullptr) {
            saturating_add(
                bytes,
                strict_->fast_estimated_owned_bytes());
        }
        return bytes;
    }

    std::uint64_t calculate_adapter_owned_bytes() const {
        std::uint64_t bytes = sizeof(*this);
        saturating_add(
            bytes, caller_seed_retained_bytes_);
        saturating_add(bytes, dynamic_child_owned_bytes());
        bytes += strict_junk_to_coarse_.capacity() *
                 sizeof(std::uint32_t);
        for (const std::vector<std::uint32_t>& slot_map :
             strict_goal_member_to_coarse_) {
            bytes += slot_map.capacity() *
                     sizeof(std::uint32_t);
        }
        bytes += admitted_actions_.capacity() *
                 sizeof(std::uint32_t);
        bytes += admitted_coarse_operators_.capacity() *
                 sizeof(std::uint32_t);
        bytes += admitted_operators_.capacity() *
                 sizeof(std::uint32_t);
        bytes += reoptimization_parents_.size() *
                 (sizeof(std::uint32_t) + 3 * sizeof(void*));
        bytes +=
            (strict_operator_by_coarse_.size() +
             coarse_operator_by_strict_.size()) *
            (sizeof(std::pair<const std::uint32_t, std::uint32_t>) +
             3 * sizeof(void*));
        bytes += coarse_state_by_key_.size() *
                 (sizeof(decltype(
                      coarse_state_by_key_)::value_type) +
                  3 * sizeof(void*));
        for (const auto& [key, unused] :
             coarse_state_by_key_) {
            (void)unused;
            bytes += stable_key_bytes(key);
        }
        bytes += requirement_bytes(carrier_requirement_);
        bytes += stable_key_bytes(root_key_);
        bytes += coarse_policy_.size() *
                 (sizeof(decltype(coarse_policy_)::value_type) +
                  3 * sizeof(void*));
        for (const auto& [unused, node] : coarse_policy_) {
            (void)unused;
            bytes += node.successors.capacity() *
                     sizeof(std::uint32_t);
            bytes += requirement_bytes(node.required);
            if (node.selected.has_value()) {
                bytes += selected_action_bytes(*node.selected);
            }
        }
        bytes += known_.size() *
                 (sizeof(decltype(known_)::value_type) +
                  3 * sizeof(void*));
        for (const auto& [key, known] : known_) {
            bytes += stable_key_bytes(key);
            bytes += known.equivalent_strict_states.capacity() *
                     sizeof(std::uint32_t);
            bytes += feature_signature_bytes(
                known.collapsed_features);
        }
        bytes += kernels_.size() *
                 (sizeof(decltype(kernels_)::value_type) +
                  3 * sizeof(void*));
        for (const auto& [key, kernel] : kernels_) {
            bytes += stable_key_bytes(key.first);
            bytes += stable_key_bytes(key.second);
            bytes += kernel.transitions.capacity() *
                     sizeof(ExactTransition);
            for (const ExactTransition& transition :
                 kernel.transitions) {
                bytes += exact_state_bytes(
                    transition.successor);
            }
        }
        bytes += choice_recipes_.size() *
                 (sizeof(decltype(choice_recipes_)::value_type) +
                  3 * sizeof(void*));
        for (const auto& [key, recipe] : choice_recipes_) {
            bytes += stable_key_bytes(key.first);
            bytes += stable_key_bytes(key.second);
            bytes += exact_choice_recipe_bytes(recipe);
        }
        bytes += stable_kernel_reuse_keys_.size() *
                 (sizeof(StableKey) + 3 * sizeof(void*));
        for (const StableKey& key : stable_kernel_reuse_keys_) {
            bytes += stable_key_bytes(key);
        }
        bytes += exact_policy_.size() *
                 (sizeof(decltype(exact_policy_)::value_type) +
                  3 * sizeof(void*));
        for (const auto& [key, selected] : exact_policy_) {
            bytes += stable_key_bytes(key);
            bytes += selected_action_bytes(selected);
        }
        bytes += stable_key_set_bytes(repair_carriers_);
        if (exact_root_filter_.has_value()) {
            bytes += exact_root_filter_->size() *
                     (sizeof(std::pair<
                          const std::uint32_t,
                          std::vector<StableKey>>) +
                      3 * sizeof(void*));
            for (const auto& [unused, keys] :
                 *exact_root_filter_) {
                (void)unused;
                bytes += keys.capacity() * sizeof(StableKey);
                for (const StableKey& key : keys) {
                    bytes += stable_key_bytes(key);
                }
            }
        }
        if (illegal_action_witness_.has_value()) {
            bytes += stable_key_bytes(*illegal_action_witness_);
        }
        telemetry_.adapter_owned_bytes = bytes;
        telemetry_.peak_adapter_owned_bytes = std::max(
            telemetry_.peak_adapter_owned_bytes, bytes);
        if (strict_ != nullptr) {
            telemetry_.strict_reforge_work =
                strict_->telemetry().reforge_frontier_work;
        }
        return bytes;
    }

    std::uint64_t audit_adapter_owned_bytes() const {
        const std::uint64_t bytes =
            calculate_adapter_owned_bytes();
        const std::uint64_t child_bytes =
            dynamic_child_owned_bytes();
        adapter_memory_cached_non_child_bytes_ =
            bytes > child_bytes ? bytes - child_bytes : 0;
        adapter_memory_projected_growth_bytes_ = 0;
        adapter_memory_checks_since_audit_ = 0;
        adapter_memory_cache_initialized_ = true;
        return bytes;
    }

    std::uint64_t estimated_owned_bytes() const override {
        constexpr std::uint32_t kAuditInterval = 512;
        if (!adapter_memory_cache_initialized_ ||
            adapter_memory_checks_since_audit_ >=
                kAuditInterval) {
            return audit_adapter_owned_bytes();
        }
        ++adapter_memory_checks_since_audit_;
        std::uint64_t bytes =
            adapter_memory_cached_non_child_bytes_;
        saturating_add(
            bytes, adapter_memory_projected_growth_bytes_);
        saturating_add(bytes, dynamic_child_owned_bytes());
        telemetry_.adapter_owned_bytes = bytes;
        telemetry_.peak_adapter_owned_bytes = std::max(
            telemetry_.peak_adapter_owned_bytes, bytes);
        if (strict_ != nullptr) {
            telemetry_.strict_reforge_work =
                strict_->telemetry().reforge_frontier_work;
        }
        return bytes;
    }

  private:
    struct KnownState {
        std::uint32_t strict_state = kNoId;
        std::uint32_t coarse_state = kNoId;
        bool terminal = false;
        bool policy_collapsed = false;
        std::vector<std::uint32_t> equivalent_strict_states;
        FeatureSignature collapsed_features;
    };
    using KernelKey = std::pair<StableKey, StableKey>;

    CalcContext& coarse_;
    const SolveResult& solved_;
    pc_item_state exact_start_{};
    const std::unordered_map<std::string, double>& prices_;
    const SolveOptions& options_;
    RefinementLimits limits_;
    PolicyLiftAdapterTelemetry& telemetry_;
    std::uint64_t coarse_retained_bytes_at_entry_ = 0;
    std::shared_ptr<const SessionImpl> borrowed_session_;
    bool enable_local_reoptimization_ = false;
    std::set<std::uint32_t> reoptimization_parents_;
    std::uint64_t caller_seed_retained_bytes_ = 0;
    std::unique_ptr<CalcContext> strict_;
    std::vector<std::uint32_t> strict_junk_to_coarse_;
    std::array<
        std::vector<std::uint32_t>,
        kMaxGoalSlots> strict_goal_member_to_coarse_;
    std::vector<std::uint32_t> admitted_actions_;
    std::vector<std::uint32_t> admitted_coarse_operators_;
    std::vector<std::uint32_t> admitted_operators_;
    std::map<std::uint32_t, std::uint32_t>
        strict_operator_by_coarse_;
    std::map<std::uint32_t, std::uint32_t>
        coarse_operator_by_strict_;
    std::map<StableKey, std::uint32_t> coarse_state_by_key_;
    ObservationRequirement carrier_requirement_;
    std::map<std::uint32_t, CoarsePolicyNode> coarse_policy_;
    std::map<StableKey, KnownState> known_;
    std::map<KernelKey, ExactActionKernel> kernels_;
    std::map<KernelKey, ExactChoiceRecipe> choice_recipes_;
    std::set<StableKey> stable_kernel_reuse_keys_;
    std::map<StableKey, SelectedAction> exact_policy_;
    std::set<StableKey> repair_carriers_;
    std::optional<std::map<std::uint32_t, std::vector<StableKey>>>
        exact_root_filter_;
    std::optional<StableKey> illegal_action_witness_;
    std::optional<OperatorVocabularyWideningWitness>
        operator_vocabulary_widening_witness_;
    StableKey root_key_;
    std::uint64_t refinement_rounds_consumed_ = 0;
    mutable bool adapter_memory_cache_initialized_ = false;
    mutable std::uint32_t adapter_memory_checks_since_audit_ = 0;
    mutable std::uint64_t adapter_memory_cached_non_child_bytes_ = 0;
    mutable std::uint64_t adapter_memory_projected_growth_bytes_ = 0;

    ExactState materialize_exact_state(
            const StableKey& key,
            const KnownState& known) const {
        if (known.strict_state >= strict_->state_count() ||
            known.coarse_state >= coarse_.state_count()) {
            throw AdapterFailure(
                PolicyExactLiftStatus::InvalidSolveState,
                "retained strict carrier has an invalid state index");
        }
        ExactState exact;
        exact.stable_key = key;
        exact.coarse_state = known.coarse_state;
        exact.coarse_state_key = abstract_state_key(
            coarse_.state(known.coarse_state));
        exact.goal = known.terminal;
        exact.terminal = known.terminal;
        if (known.policy_collapsed) {
            exact.features = known.collapsed_features;
            return exact;
        }
        const auto policy = coarse_policy_.find(
            known.coarse_state);
        const bool witness_parent =
            enable_local_reoptimization_ &&
            reoptimization_parents_.contains(
                known.coarse_state);
        const ObservationRequirement& requirement =
            witness_parent || exact_policy_.contains(key) ||
                    policy == coarse_policy_.end() ||
                    !policy->second.observations_propagated
                ? carrier_requirement_
                : policy->second.required;
        const AbstractFeatureExtraction extraction =
            extract_strict_abstract_features(
                strict_->session(), strict_->layout(),
                strict_->state(known.strict_state),
                requirement);
        if (!extraction.complete()) {
            throw AdapterFailure(
                PolicyExactLiftStatus::ObservationUnavailable,
                "retained strict carrier lost a policy-required "
                "observation");
        }
        exact.features = extraction.features;
        return exact;
    }

    std::optional<StableKey> stable_reforge_reuse_key(
            const std::uint32_t strict_state,
            const std::uint32_t action) const {
        StableKey reforge_signature;
        if (!strict_->exact_reforge_kernel_signature(
                strict_state, action, reforge_signature)) {
            return std::nullopt;
        }
        StableKey key{
            0x7063726b72657531ull, /* "pcrkreu1" */
            options_.goal_progress_gated_reforges ? 1ull : 0ull,
            reforge_signature.size()};
        key.insert(
            key.end(), reforge_signature.begin(),
            reforge_signature.end());
        return key;
    }

    bool has_proven_unobserved_primitive_recipe(
            const std::uint32_t strict_state,
            const PlannerOperator& planner) const {
        if (planner.kind != PlannerOperatorKind::Primitive) {
            return false;
        }
        const std::optional<StableKey> key =
            stable_reforge_reuse_key(
                strict_state, planner.primitive_action);
        return key.has_value() &&
               stable_kernel_reuse_keys_.contains(*key);
    }

    std::optional<StableKey> policy_collapse_key(
            const std::uint32_t strict_state,
            const StableKey& coarse_key,
            const bool terminal,
            const FeatureSignature& features,
            const CoarsePolicyNode& policy) const {
        if (enable_local_reoptimization_) return std::nullopt;
        if (terminal) {
            StableKey key{
                0x7063727465726d31ull, /* "pcrterm1" */
                coarse_key.size()};
            key.insert(key.end(), coarse_key.begin(), coarse_key.end());
            return key;
        }
        if (!policy.selected.has_value() ||
            policy.selected->action_id >= strict_->operators().size()) {
            return std::nullopt;
        }
        const std::optional<StableKey> operation_signature =
            canonical_operation_state_signature(
                strict_->session(), strict_->layout(),
                strict_->state(strict_state), *policy.selected);
        if (!operation_signature.has_value()) return std::nullopt;
        const PlannerOperator& planner =
            strict_->operators()[policy.selected->action_id];
        StableKey kernel_signature;
        if (planner.kind == PlannerOperatorKind::Primitive) {
            (void)strict_->exact_reforge_kernel_signature(
                strict_state, planner.primitive_action,
                kernel_signature);
        }
        StableKey key{
            0x70637270636f6c31ull, /* "pcrpcol1" */
            coarse_key.size()};
        key.insert(key.end(), coarse_key.begin(), coarse_key.end());
        key.push_back(operation_signature->size());
        key.insert(
            key.end(), operation_signature->begin(),
            operation_signature->end());
        key.push_back(kernel_signature.size());
        key.insert(
            key.end(), kernel_signature.begin(), kernel_signature.end());
        (void)features;
        return key;
    }

    std::optional<SelectedAction> candidate_selection(
        const StableKey& key,
        std::uint32_t operator_index,
        std::uint64_t caller_retained_bytes);

    const ExactActionKernel* candidate_kernel(
        const StableKey& key,
        const SelectedAction& selected);

    CandidateExactPartition partition_candidate_subclasses(
        const RefinedPolicyClass& incumbent_class,
        std::uint32_t operator_index,
        std::uint64_t caller_retained_bytes);

    bool shared_bellman_prefers_candidate(
        const StableKey& key,
        const SelectedAction& current,
        const ExactActionKernel& current_kernel,
        const SelectedAction& candidate,
        const ExactActionKernel& candidate_kernel,
        const std::map<StableKey, double>& incumbent_values,
        const std::map<StableKey, double>& continuation_values,
        std::uint64_t caller_retained_bytes);

    bool repair_invalid_policy_recursive(
        const std::vector<StableKey>& roots,
        ExactPolicyRun& repaired,
        std::set<StableKey>& repairing,
        std::vector<PolicyMutation>& mutations,
        std::uint32_t depth,
        std::uint64_t caller_retained_bytes,
        bool evaluate_before_repair = true);

    std::set<StableKey> mismatch_witnesses(
        const ExactPolicyRun& run) const;

    bool note_missing_candidate_vocabulary(
        const StableKey& key);

    void apply_policy_override(
        const StableKey& key,
        SelectedAction selected,
        std::vector<PolicyMutation>& mutations);

    void rollback_policy_mutations(
        std::vector<PolicyMutation>& mutations,
        std::size_t checkpoint);

    void release_completed_evaluation_storage(
        const std::vector<StableKey>& roots);

    void require_local_memory(
        std::uint64_t caller_owned_bytes,
        const char* subject) const;

    void refresh_strict_resource_cap(
        const char* subject,
        std::uint64_t caller_owned_bytes = 0);

    void consume_refinement_rounds(
        std::uint64_t rounds,
        const char* subject);

    static bool mask_has(
            const std::vector<std::uint64_t>& mask,
            const std::uint32_t mod) {
        return mod / 64 < mask.size() &&
               pc_bitset_test(mask.data(), mod);
    }

    void build_strict_to_coarse_junk_map() {
        const AbstractLayout& strict_layout = strict_->layout();
        const AbstractLayout& coarse_layout = coarse_.layout();
        for (std::size_t slot = 0;
             slot < strict_goal_member_to_coarse_.size();
             ++slot) {
            std::vector<std::uint32_t>& mapping =
                strict_goal_member_to_coarse_[slot];
            mapping.clear();
            if (slot >= strict_layout.slots.size()) {
                continue;
            }
            const ResolvedGoalSlot& strict_slot =
                strict_layout.slots[slot];
            mapping.assign(
                strict_slot.member_classes.size(), kNoId);
            require_local_memory(
                0, "strict-to-coarse goal-member map construction");
            for (std::uint32_t exact_class = 0;
                 exact_class < strict_slot.member_classes.size();
                 ++exact_class) {
                const GoalMemberClass& member_class =
                    strict_slot.member_classes[exact_class];
                std::uint32_t parent_token = kNoId;
                bool saw_member = false;
                for (std::uint32_t mod = 0;
                     mod < coarse_.session().mod_count;
                     ++mod) {
                    if (!mask_has(
                            member_class.member_mask, mod)) {
                        continue;
                    }
                    saw_member = true;
                    const std::uint32_t candidate =
                        slot < coarse_layout.slots.size() &&
                                !coarse_layout.slots[slot]
                                     .member_class_token_by_mod
                                     .empty()
                            ? coarse_layout.slots[slot]
                                  .member_class_token_by_mod
                                  .at(mod)
                            : 0;
                    if (parent_token == kNoId) {
                        parent_token = candidate;
                    } else if (parent_token != candidate) {
                        throw AdapterFailure(
                            PolicyExactLiftStatus::
                                CoarseMappingFailure,
                            "one strict goal-member class spans "
                            "multiple coarse parents");
                    }
                }
                if (!saw_member || parent_token == kNoId) {
                    throw AdapterFailure(
                        PolicyExactLiftStatus::
                            CoarseMappingFailure,
                        "strict goal-member class has no concrete "
                        "coarse parent");
                }
                mapping[exact_class] = parent_token;
            }
        }
        require_local_memory(
            strict_layout.junk_classes.size() *
                sizeof(std::uint32_t),
            "strict-to-coarse junk map construction");
        strict_junk_to_coarse_.assign(
            strict_layout.junk_classes.size(), kNoId);
        require_local_memory(
            0, "strict-to-coarse junk map construction");
        for (std::uint32_t exact_class = 0;
             exact_class < strict_layout.junk_classes.size();
            ++exact_class) {
            std::uint32_t parent = kNoId;
            bool saw_member = false;
            bool saw_mapping = false;
            const JunkClass& junk =
                strict_layout.junk_classes[exact_class];
            for (std::uint32_t mod = 0;
                 mod < coarse_.session().mod_count;
                 ++mod) {
                if (!mask_has(junk.member_mask, mod)) continue;
                saw_member = true;
                if (mod >= coarse_layout.junk_class_by_mod.size()) {
                    throw AdapterFailure(
                        PolicyExactLiftStatus::CoarseMappingFailure,
                        "strict junk carrier exceeds coarse modifier "
                        "mapping");
                }
                const std::uint32_t candidate =
                    coarse_layout.junk_class_by_mod[mod];
                if (!saw_mapping) {
                    parent = candidate;
                    saw_mapping = true;
                } else if (parent != candidate) {
                    throw AdapterFailure(
                        PolicyExactLiftStatus::CoarseMappingFailure,
                        "one strict junk class spans multiple coarse "
                        "parents");
                }
            }
            if (!saw_member) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::CoarseMappingFailure,
                    "strict junk class has no concrete members");
            }
            strict_junk_to_coarse_[exact_class] = parent;
        }
    }

    static void append_key(
            StableKey& target,
            const StableKey& addition) {
        target.push_back(addition.size());
        target.insert(
            target.end(), addition.begin(), addition.end());
    }

    static StableKey abstract_state_key(
            const AbstractState& state) {
        return exact_state_key(state, 0);
    }

    void index_coarse_states() {
        coarse_state_by_key_.clear();
        for (std::uint32_t state = 0;
             state < coarse_.state_count();
             ++state) {
            StableKey key =
                abstract_state_key(coarse_.state(state));
            std::uint64_t transient =
                stable_key_bytes(key);
            saturating_add(
                transient,
                sizeof(decltype(
                    coarse_state_by_key_)::value_type) +
                    3 * sizeof(void*));
            require_local_memory(
                transient, "coarse structural-index construction");
            const auto [unused, inserted] =
                coarse_state_by_key_.emplace(
                    std::move(key),
                    state);
            (void)unused;
            if (!inserted) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::InvalidSolveState,
                    "coarse calculation contains duplicate structural "
                    "state identities");
            }
            require_local_memory(
                0, "coarse structural-index construction");
        }
    }

    const PlannerOperator& coarse_operator(
            const std::uint32_t coarse_state) const {
        if (coarse_state >= solved_.policy.size()) {
            throw AdapterFailure(
                PolicyExactLiftStatus::InvalidSolveState,
                "policy-reachable coarse state is outside the policy "
                "table");
        }
        const PolicyOperatorRef selected = solved_.policy[coarse_state];
        if (selected.index == kNoId) {
            throw AdapterFailure(
                PolicyExactLiftStatus::InvalidSolveState,
                "non-goal coarse state has no selected policy action");
        }
        if (selected.index >= coarse_.operators().size()) {
            throw AdapterFailure(
                PolicyExactLiftStatus::InvalidSolveState,
                "selected policy operator is outside the admitted "
                "vocabulary");
        }
        const PlannerOperator& planner =
            coarse_.operators()[selected.index];
        if (selected.kind != planner.kind) {
            throw AdapterFailure(
                PolicyExactLiftStatus::InvalidSolveState,
                "selected policy operator kind disagrees with the "
                "admitted vocabulary");
        }
        return planner;
    }

    PlannerOperatorRuntimeSemantics operator_runtime_semantics(
            const PlannerOperator& planner) const {
        try {
            return planner_operator_runtime_semantics(
                planner, coarse_.registry());
        } catch (const std::exception& error) {
            throw AdapterFailure(
                PolicyExactLiftStatus::ObservationUnavailable,
                std::string{
                    "planner operator has no complete runtime "
                    "refinement semantics: "} +
                    error.what());
        }
    }

    void populate_ordered_runtime_contracts(
            SelectedAction& selected,
            const PlannerOperatorRuntimeSemantics& runtime) const {
        const std::uint64_t runtime_bytes =
            planner_runtime_semantics_bytes(runtime);
        const auto require_copy_memory =
            [&](const std::uint64_t extra,
                const char* subject) {
                std::uint64_t live = runtime_bytes;
                saturating_add(
                    live, selected_action_bytes(selected));
                saturating_add(live, extra);
                require_local_memory(live, subject);
            };
        selected.contract =
            runtime.compatibility_refinement;
        selected.ordered_program.clear();
        selected.ordered_program.reserve(
            runtime.ordered_program.size());
        require_copy_memory(
            0, "planner runtime-contract copy");
        for (const PlannerOperatorRuntimeStep& step :
             runtime.ordered_program) {
            selected.ordered_program.push_back(
                step.refinement);
            require_copy_memory(
                0, "planner runtime-contract copy");
        }
        selected.execution_paths.clear();
        selected.execution_paths.reserve(
            runtime.execution_paths.size());
        require_copy_memory(
            0, "planner runtime-path copy");
        for (const std::vector<PlannerOperatorRuntimeStep>& path :
             runtime.execution_paths) {
            std::vector<ActionRefinementContract> contracts;
            contracts.reserve(path.size());
            std::uint64_t path_bytes =
                contracts.capacity() *
                sizeof(ActionRefinementContract);
            require_copy_memory(
                path_bytes, "planner runtime-path copy");
            for (const PlannerOperatorRuntimeStep& step : path) {
                contracts.push_back(step.refinement);
                path_bytes =
                    contracts.capacity() *
                    sizeof(ActionRefinementContract);
                for (const ActionRefinementContract& contract :
                     contracts) {
                    saturating_add(
                        path_bytes, contract_bytes(contract));
                }
                require_copy_memory(
                    path_bytes, "planner runtime-path copy");
            }
            selected.execution_paths.push_back(
                std::move(contracts));
            require_copy_memory(
                0, "planner runtime-path copy");
        }
    }

    void populate_ordered_runtime_contracts(
            SelectedAction& selected,
            const PlannerOperator& planner) const {
        const PlannerOperatorRuntimeSemantics runtime =
            operator_runtime_semantics(planner);
        require_local_memory(
            planner_runtime_semantics_bytes(runtime),
            "planner runtime-semantics construction");
        populate_ordered_runtime_contracts(selected, runtime);
    }

    void import_admitted_operators() {
        std::vector<std::uint32_t> order =
            admitted_coarse_operators_;
        std::uint64_t order_transient =
            order.capacity() * sizeof(std::uint32_t);
        std::uint64_t largest_key = 0;
        std::uint64_t second_largest_key = 0;
        for (const std::uint32_t coarse_index : order) {
            const StableKey key =
                planner_operator_semantic_key(
                    coarse_.operators().at(coarse_index));
            const std::uint64_t key_bytes =
                stable_key_bytes(key);
            if (key_bytes >= largest_key) {
                second_largest_key = largest_key;
                largest_key = key_bytes;
            } else if (key_bytes > second_largest_key) {
                second_largest_key = key_bytes;
            }
            require_local_memory(
                order_transient + key_bytes,
                "planner import ordering");
        }
        saturating_add(order_transient, largest_key);
        saturating_add(order_transient, second_largest_key);
        require_local_memory(
            order_transient, "planner import ordering");
        std::sort(
            order.begin(), order.end(),
            [&](const std::uint32_t left,
                const std::uint32_t right) {
                const StableKey left_key =
                    planner_operator_semantic_key(
                        coarse_.operators().at(left));
                const StableKey right_key =
                    planner_operator_semantic_key(
                        coarse_.operators().at(right));
                return left_key != right_key
                           ? left_key < right_key
                           : left < right;
            });
        for (const std::uint32_t coarse_index : order) {
            refresh_strict_resource_cap(
                "planner operator import", order_transient);
            const std::uint32_t strict_index =
                strict_->import_planner_operator(
                    coarse_.operators().at(coarse_index),
                    coarse_.is_state_local_automatic_operator(
                        coarse_index));
            strict_operator_by_coarse_.emplace(
                coarse_index, strict_index);
            coarse_operator_by_strict_.try_emplace(
                strict_index, coarse_index);
            require_local_memory(
                order_transient, "planner operator import");
        }
        std::uint64_t admitted_projection = order_transient;
        saturating_add(
            admitted_projection,
            admitted_coarse_operators_.size() *
                sizeof(std::uint32_t));
        require_local_memory(
            admitted_projection,
            "strict planner vocabulary construction");
        admitted_operators_.reserve(
            admitted_coarse_operators_.size());
        for (const std::uint32_t coarse_index :
             admitted_coarse_operators_) {
            admitted_operators_.push_back(
                strict_operator_for(coarse_index));
            require_local_memory(
                order_transient,
                "strict planner vocabulary construction");
        }
        std::sort(
            admitted_operators_.begin(),
            admitted_operators_.end());
        admitted_operators_.erase(
            std::unique(
                admitted_operators_.begin(),
                admitted_operators_.end()),
            admitted_operators_.end());
        require_local_memory(
            order_transient,
            "strict planner vocabulary construction");
    }

    std::uint32_t strict_operator_for(
            const std::uint32_t coarse_index) const {
        const auto found =
            strict_operator_by_coarse_.find(coarse_index);
        if (found == strict_operator_by_coarse_.end()) {
            throw AdapterFailure(
                PolicyExactLiftStatus::InvalidSolveState,
                "planner operator was not imported into the strict "
                "calculation context");
        }
        return found->second;
    }

    std::uint32_t coarse_operator_for(
            const std::uint32_t strict_index) const {
        const auto found =
            coarse_operator_by_strict_.find(strict_index);
        if (found == coarse_operator_by_strict_.end()) {
            throw AdapterFailure(
                PolicyExactLiftStatus::InvalidSolveState,
                "strict planner operator has no coarse semantic source");
        }
        return found->second;
    }

    double primitive_action_cost(
            const ActionDescriptor& action) const {
        double total = 0.0;
        for (const std::string& key : action.cost_keys) {
            const auto found = prices_.find(key);
            if (found == prices_.end() ||
                !std::isfinite(found->second) ||
                found->second < 0.0) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::MissingPrice,
                    "selected primitive action '" + action.id +
                        "' has a missing or invalid price for '" +
                        key + "'");
            }
            total += found->second;
        }
        if (!std::isfinite(total)) {
            throw AdapterFailure(
                PolicyExactLiftStatus::MissingPrice,
                "selected primitive action has a non-finite total price");
        }
        return total;
    }

    double resource_cost(
            const std::vector<std::pair<std::string, double>>&
                resources) const {
        double total = 0.0;
        for (const auto& [key, quantity] : resources) {
            const auto found = prices_.find(key);
            if (found == prices_.end() ||
                !std::isfinite(found->second) ||
                found->second < 0.0 ||
                !std::isfinite(quantity) ||
                quantity < 0.0) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::MissingPrice,
                    "selected planner option has a missing or invalid "
                    "exact resource price for '" +
                        key + "'");
            }
            total += quantity * found->second;
        }
        if (!std::isfinite(total)) {
            throw AdapterFailure(
                PolicyExactLiftStatus::MissingPrice,
                "selected planner option has a non-finite exact price");
        }
        return total;
    }

    std::uint32_t project_state_to_coarse(
            CalcContext& source,
            const std::uint32_t state,
            const std::uint32_t entry_state) {
        const std::uint32_t resolved =
            state == kNoId ? entry_state : state;
        if (&source == &coarse_) return resolved;
        if (&source != strict_.get() ||
            resolved >= strict_->state_count()) {
            throw AdapterFailure(
                PolicyExactLiftStatus::CoarseMappingFailure,
                "planner observation state has no strict-to-coarse "
                "projection");
        }
        const auto parent =
            coarse_state_by_key_.find(
                abstract_state_key(
                    coarsen(strict_->state(resolved))));
        return parent == coarse_state_by_key_.end()
                   ? kNoId
                   : parent->second;
    }

    double projected_solved_value(
            CalcContext& source,
            const std::uint32_t state,
            const std::uint32_t entry_state) {
        const std::uint32_t parent =
            project_state_to_coarse(source, state, entry_state);
        return parent < solved_.values.size() &&
                       std::isfinite(solved_.values[parent])
                   ? solved_.values[parent]
                   : std::numeric_limits<double>::infinity();
    }

    std::vector<std::uint32_t> primitive_preferences(
            const std::uint32_t coarse_state,
            CalcContext& source,
            const std::uint32_t entry_state,
            const OutcomeDistribution& distribution,
            const bool require_authored) {
        std::vector<std::uint32_t> authored;
        if (coarse_state < solved_.unveil_preferences.size()) {
            authored = solved_.unveil_preferences[coarse_state];
        }
        if (require_authored && authored.empty() &&
            !distribution.choice_groups.empty()) {
            throw AdapterFailure(
                PolicyExactLiftStatus::UnsupportedPrimitiveKernel,
                "candidate observed primitive has no authored coarse "
                "preference");
        }
        struct Ranked {
            std::uint32_t mod_id = kNoId;
            std::uint32_t successor_state = kNoId;
            double value = std::numeric_limits<double>::infinity();
        };
        std::map<std::uint32_t,
                 const OutcomeChoiceOption*> offered;
        for (const OutcomeChoiceOption& option :
             distribution.choice_options) {
            auto [found, inserted] =
                offered.emplace(option.mod_id, &option);
            if (!inserted) {
                const std::uint32_t left_state =
                    found->second->state == kNoId
                        ? entry_state
                        : found->second->state;
                const std::uint32_t right_state =
                    option.state == kNoId
                        ? entry_state
                        : option.state;
                const StableKey left = abstract_state_key(
                    source.state(left_state));
                const StableKey right = abstract_state_key(
                    source.state(right_state));
                if (left != right) {
                    throw AdapterFailure(
                        PolicyExactLiftStatus::
                            UnsupportedPrimitiveKernel,
                        "one primitive observed modifier has multiple "
                        "semantic successors");
                }
                if (right_state < left_state) {
                    found->second = &option;
                }
            }
        }
        std::vector<std::uint32_t> result;
        for (const std::uint32_t mod : authored) {
            if (offered.contains(mod) &&
                std::find(result.begin(), result.end(), mod) ==
                    result.end()) {
                result.push_back(mod);
            }
        }
        std::vector<Ranked> remaining;
        for (const auto& [mod, choice] : offered) {
            if (std::find(result.begin(), result.end(), mod) ==
                    result.end()) {
                const std::uint32_t successor_state =
                    choice->state == kNoId
                        ? entry_state
                        : choice->state;
                remaining.push_back({
                    mod,
                    successor_state,
                    projected_solved_value(
                        source, choice->state, entry_state)});
            }
        }
        order_observed_offers_by_sparse_choice(
            remaining,
            [](const Ranked& choice) {
                return choice.value;
            });
        for (const Ranked& ranked : remaining) {
            result.push_back(ranked.mod_id);
        }
        return result;
    }

    std::vector<ObservedUnveilPreference>
    option_preferences(
            const std::uint32_t coarse_state,
            CalcContext& source,
            const std::uint32_t entry_state,
            const OptionKernel& kernel,
            const bool require_authored) {
        std::map<std::uint32_t,
                 std::vector<const OutcomeChoiceOption*>>
            offered_by_observation;
        for (const OutcomeChoiceOption& choice :
             kernel.observation_choice_options) {
            offered_by_observation[choice.observation_state]
                .push_back(&choice);
        }
        std::vector<ObservedUnveilPreference> result;
        result.reserve(offered_by_observation.size());
        for (const auto& [observation_state, offered] :
             offered_by_observation) {
            const std::uint32_t coarse_observation =
                project_state_to_coarse(
                    source, observation_state, entry_state);
            const ObservedUnveilPreference* authored = nullptr;
            if (coarse_state <
                solved_.option_unveil_preferences.size()) {
                const auto& candidates =
                    solved_.option_unveil_preferences[coarse_state];
                const auto found = std::find_if(
                    candidates.begin(), candidates.end(),
                    [&](const ObservedUnveilPreference& candidate) {
                        return candidate.observation_state ==
                               coarse_observation;
                    });
                if (found != candidates.end()) {
                    authored = &*found;
                }
            }
            if (require_authored && authored == nullptr) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::UnsupportedPrimitiveKernel,
                    "candidate observed planner option has no authored "
                    "coarse preference");
            }

            std::map<std::uint32_t,
                     const OutcomeChoiceOption*> by_mod;
            for (const OutcomeChoiceOption* choice : offered) {
                const auto [found, inserted] =
                    by_mod.emplace(choice->mod_id, choice);
                if (!inserted) {
                    const std::uint32_t left_state =
                        found->second->state == kNoId
                            ? entry_state
                            : found->second->state;
                    const std::uint32_t right_state =
                        choice->state == kNoId
                            ? entry_state
                            : choice->state;
                    const StableKey left = abstract_state_key(
                        source.state(left_state));
                    const StableKey right = abstract_state_key(
                        source.state(right_state));
                    if (left != right) {
                        throw AdapterFailure(
                            PolicyExactLiftStatus::
                                UnsupportedPrimitiveKernel,
                            "one observed option modifier has multiple "
                            "semantic successors");
                    }
                    if (right_state < left_state) {
                        found->second = choice;
                    }
                }
            }

            std::vector<std::uint32_t> ordered_mods;
            if (authored != nullptr) {
                for (const ObservedUnveilChoice& choice :
                     authored->choices) {
                    if (by_mod.contains(choice.mod_id) &&
                        std::find(
                            ordered_mods.begin(),
                            ordered_mods.end(),
                            choice.mod_id) ==
                            ordered_mods.end()) {
                        ordered_mods.push_back(choice.mod_id);
                    }
                }
            }
            struct Ranked {
                std::uint32_t mod_id = kNoId;
                std::uint32_t successor_state = kNoId;
                double value =
                    std::numeric_limits<double>::infinity();
            };
            std::vector<Ranked> remaining;
            for (const auto& [mod, choice] : by_mod) {
                if (std::find(
                        ordered_mods.begin(),
                        ordered_mods.end(), mod) !=
                    ordered_mods.end()) {
                    continue;
                }
                remaining.push_back({
                    mod,
                    choice->state == kNoId
                        ? entry_state
                        : choice->state,
                    projected_solved_value(
                        source, choice->state, entry_state)});
            }
            order_observed_offers_by_sparse_choice(
                remaining,
                [](const Ranked& choice) {
                    return choice.value;
                });
            for (const Ranked& choice : remaining) {
                ordered_mods.push_back(choice.mod_id);
            }

            ObservedUnveilPreference preference;
            preference.observation_state =
                observation_state == kNoId
                    ? entry_state
                    : observation_state;
            for (const std::uint32_t mod : ordered_mods) {
                const OutcomeChoiceOption& choice =
                    *by_mod.at(mod);
                preference.choices.push_back({
                    mod,
                    choice.state == kNoId
                        ? entry_state
                        : choice.state,
                    choice.actual_state == kNoId
                        ? entry_state
                        : choice.actual_state});
            }
            result.push_back(std::move(preference));
        }
        std::sort(
            result.begin(), result.end(),
            [&](const ObservedUnveilPreference& left,
                const ObservedUnveilPreference& right) {
                const StableKey left_key =
                    abstract_state_key(
                        source.state(left.observation_state));
                const StableKey right_key =
                    abstract_state_key(
                        source.state(right.observation_state));
                return left_key != right_key
                           ? left_key < right_key
                           : left.observation_state <
                                 right.observation_state;
            });
        if (!kernel.observation_choice_groups.empty() &&
            result.empty()) {
            throw AdapterFailure(
                PolicyExactLiftStatus::UnsupportedPrimitiveKernel,
                "observed planner option has no resolvable choice "
                "program");
        }
        return result;
    }

    ExactChoiceRecipe seeded_choice_recipe(
            const std::uint32_t coarse_state,
            CalcContext& source,
            const std::uint32_t entry_state,
            const std::uint32_t operator_index,
            const bool require_authored_choices = false) {
        const PlannerOperator& planner =
            source.operators().at(operator_index);
        ExactChoiceRecipe recipe;
        if (planner.kind == PlannerOperatorKind::Primitive) {
            const OutcomeDistribution& distribution =
                source.outcomes(
                    entry_state, planner.primitive_action,
                    options_.goal_progress_gated_reforges);
            if (!distribution.supported) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::
                        UnsupportedPrimitiveKernel,
                    "selected observed primitive has no exact kernel");
            }
            if (distribution.choice_groups.empty() &&
                distribution.choice_options.empty()) {
                return recipe;
            }
            recipe.kind =
                ExactChoiceRecipeKind::PrimitiveObservedChoice;
            recipe.primitive_preferences =
                primitive_preferences(
                    coarse_state, source, entry_state,
                    distribution, require_authored_choices);
            return recipe;
        }
        const OptionKernel& kernel =
            source.option_kernel(entry_state, operator_index);
        if (!kernel.supported || !kernel.legal ||
            !kernel.terminates_almost_surely) {
            throw AdapterFailure(
                PolicyExactLiftStatus::ObservationUnavailable,
                "selected planner option has no legal exact kernel");
        }
        recipe.kind = ExactChoiceRecipeKind::FixedOption;
        recipe.option_preferences =
            option_preferences(
                coarse_state, source, entry_state, kernel,
                require_authored_choices);
        return recipe;
    }

    StableKey operator_decision_key(
            CalcContext& source,
            const std::uint32_t entry_state,
            const std::uint32_t operator_index,
            const ExactChoiceRecipe& recipe) {
        const PlannerOperator& planner =
            source.operators().at(operator_index);
        StableKey key{0x7063726465637631ull}; /* "pcrdecv1" */
        append_key(
            key, planner_operator_semantic_key(planner));
        if (planner.kind == PlannerOperatorKind::Primitive) {
            if (recipe.kind ==
                ExactChoiceRecipeKind::PrimitiveObservedChoice) {
                if (!recipe.option_preferences.empty()) {
                    throw AdapterFailure(
                        PolicyExactLiftStatus::InvalidSolveState,
                        "observed primitive decision has no matching "
                        "exact choice recipe");
                }
                key.push_back(1);
                key.push_back(
                    recipe.primitive_preferences.size());
                key.insert(
                    key.end(),
                    recipe.primitive_preferences.begin(),
                    recipe.primitive_preferences.end());
            } else {
                if (recipe.kind !=
                        ExactChoiceRecipeKind::None ||
                    !recipe.primitive_preferences.empty() ||
                    !recipe.option_preferences.empty()) {
                    throw AdapterFailure(
                        PolicyExactLiftStatus::InvalidSolveState,
                        "unobserved primitive decision has an exact "
                        "choice recipe");
                }
                key.push_back(0);
            }
            return key;
        }
        if (recipe.kind !=
                ExactChoiceRecipeKind::FixedOption ||
            !recipe.primitive_preferences.empty()) {
            throw AdapterFailure(
                PolicyExactLiftStatus::InvalidSolveState,
                "fixed-option decision has no matching exact choice "
                "recipe");
        }
        const OptionKernel& kernel =
            source.option_kernel(entry_state, operator_index);
        if (!kernel.supported || !kernel.legal ||
            !kernel.terminates_almost_surely) {
            throw AdapterFailure(
                PolicyExactLiftStatus::ObservationUnavailable,
                "selected planner option has no legal exact kernel");
        }
        key.push_back(2);
        try {
            append_key(
                key,
                fixed_option_executable_recipe_key(
                    fixed_option_executable_recipe(
                        source, entry_state, planner, kernel,
                        recipe.option_preferences)));
        } catch (const std::exception& error) {
            throw AdapterFailure(
                PolicyExactLiftStatus::
                    UnsupportedPrimitiveKernel,
                std::string{
                    "fixed-option exact choice recipe is not "
                    "executable: "} +
                    error.what());
        }
        return key;
    }

    void store_choice_recipe(
            const StableKey& exact_state,
            const StableKey& semantic_key,
            ExactChoiceRecipe recipe) {
        std::uint64_t local_key_bytes = sizeof(KernelKey);
        saturating_add(
            local_key_bytes, stable_key_bytes(exact_state));
        saturating_add(
            local_key_bytes, stable_key_bytes(semantic_key));
        require_local_memory(
            local_key_bytes,
            "exact observed-choice recipe lookup");
        const KernelKey key{exact_state, semantic_key};
        const auto existing = choice_recipes_.find(key);
        if (existing != choice_recipes_.end()) {
            if (!(existing->second == recipe)) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::InvalidSolveState,
                    "one exact semantic decision changed its stored "
                    "choice recipe");
            }
            return;
        }
        std::uint64_t projected =
            local_key_bytes +
            sizeof(decltype(choice_recipes_)::value_type) +
            3 * sizeof(void*);
        saturating_add(
            projected, stable_key_bytes(exact_state));
        saturating_add(
            projected, stable_key_bytes(semantic_key));
        saturating_add(
            projected, exact_choice_recipe_bytes(recipe));
        require_local_memory(
            projected, "exact observed-choice recipe construction");
        choice_recipes_.emplace(key, std::move(recipe));
        require_local_memory(
            0, "exact observed-choice recipe construction");
    }

    const ExactChoiceRecipe& choice_recipe(
            const StableKey& exact_state,
            const SelectedAction& selected) const {
        const auto found = choice_recipes_.find(
            {exact_state, selected.semantic_key});
        if (found == choice_recipes_.end()) {
            const auto known = known_.find(exact_state);
            if (known != known_.end() &&
                selected.action_id < strict_->operators().size() &&
                has_proven_unobserved_primitive_recipe(
                    known->second.strict_state,
                    strict_->operators()[selected.action_id])) {
                static const ExactChoiceRecipe kNoChoiceRecipe;
                return kNoChoiceRecipe;
            }
            throw AdapterFailure(
                PolicyExactLiftStatus::InvalidSolveState,
                "exact selected decision has no stored choice "
                "recipe");
        }
        return found->second;
    }

    SelectedAction make_selected(
            const std::uint32_t coarse_state,
            const std::uint32_t strict_operator,
            const std::optional<std::uint32_t> strict_state,
            const ExactChoiceRecipe* exact_recipe = nullptr) {
        const PlannerOperator& planner =
            strict_->operators().at(strict_operator);
        SelectedAction selected;
        selected.action_id = strict_operator;
        populate_ordered_runtime_contracts(
            selected, planner);
        if (strict_state.has_value()) {
            refresh_strict_resource_cap(
                "strict selected-operation recipe");
            const bool proven_unobserved_primitive =
                exact_recipe == nullptr &&
                has_proven_unobserved_primitive_recipe(
                    *strict_state, planner);
            ExactChoiceRecipe recipe =
                exact_recipe == nullptr
                    ? (proven_unobserved_primitive
                           ? ExactChoiceRecipe{}
                           : seeded_choice_recipe(
                                 coarse_state, *strict_,
                                 *strict_state, strict_operator))
                    : *exact_recipe;
            selected.semantic_key = operator_decision_key(
                *strict_, *strict_state,
                strict_operator, recipe);
            if (!proven_unobserved_primitive) {
                store_choice_recipe(
                    exact_refinement_state_key(
                        strict_->state(*strict_state),
                        abstract_state_key(
                            coarse_.state(coarse_state))),
                    selected.semantic_key,
                    std::move(recipe));
            }
        } else {
            if (exact_recipe != nullptr) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::InvalidSolveState,
                    "coarse decision received an exact choice "
                    "recipe");
            }
            const std::uint32_t coarse_index =
                coarse_operator_for(strict_operator);
            const ExactChoiceRecipe recipe =
                seeded_choice_recipe(
                    coarse_state, coarse_, coarse_state,
                    coarse_index);
            selected.semantic_key = operator_decision_key(
                coarse_, coarse_state, coarse_index,
                recipe);
        }
        const auto found = coarse_policy_.find(coarse_state);
        if (found != coarse_policy_.end()) {
            selected.routing_observes = found->second.required;
        }
        return selected;
    }

    SelectedAction make_coarse_selected(
            const std::uint32_t coarse_state,
            const std::uint32_t operator_index) {
        const PlannerOperator& planner =
            coarse_.operators().at(operator_index);
        SelectedAction selected;
        selected.action_id = operator_index;
        const ExactChoiceRecipe recipe =
            seeded_choice_recipe(
                coarse_state, coarse_, coarse_state,
                operator_index);
        selected.semantic_key = operator_decision_key(
            coarse_, coarse_state, operator_index,
            recipe);
        populate_ordered_runtime_contracts(
            selected, planner);
        return selected;
    }

    std::uint32_t resolve_primitive_choice(
            const std::vector<std::uint32_t>& preferences,
            const std::uint32_t entry_state,
            const OutcomeChoiceGroup& group,
            const OutcomeDistribution& distribution) {
        for (const std::uint32_t preferred : preferences) {
            const auto option = std::find_if(
                distribution.choice_options.begin(),
                distribution.choice_options.end(),
                [&](const OutcomeChoiceOption& candidate) {
                    return candidate.mod_id == preferred &&
                           std::find(
                               group.states.begin(),
                               group.states.end(),
                               candidate.state) !=
                               group.states.end();
                });
            if (option != distribution.choice_options.end()) {
                return option->state == kNoId
                           ? entry_state
                           : option->state;
            }
        }
        throw AdapterFailure(
            PolicyExactLiftStatus::UnsupportedPrimitiveKernel,
            "observed primitive policy cannot resolve one offered "
            "successor set");
    }

    std::uint32_t resolve_option_choice(
            const std::vector<ObservedUnveilPreference>&
                preferences,
            const std::uint32_t entry_state,
            const OutcomeChoiceGroup& group,
            const OptionKernel& kernel) {
        if (group.observation_state == kNoId) {
            throw AdapterFailure(
                PolicyExactLiftStatus::UnsupportedPrimitiveKernel,
                "observed planner option group omitted its observation "
                "state");
        }
        for (const ObservedUnveilPreference& preference :
              preferences) {
            if (preference.observation_state !=
                group.observation_state) {
                continue;
            }
            for (const ObservedUnveilChoice& choice :
                  preference.choices) {
                const auto offered = std::find_if(
                    kernel.observation_choice_options.begin(),
                    kernel.observation_choice_options.end(),
                    [&](const OutcomeChoiceOption& candidate) {
                        const std::uint32_t successor =
                            candidate.state == kNoId
                                ? entry_state
                                : candidate.state;
                        const std::uint32_t observation =
                            candidate.observation_state == kNoId
                                ? entry_state
                                : candidate.observation_state;
                        return observation ==
                                   preference.observation_state &&
                               candidate.mod_id == choice.mod_id &&
                               std::find(
                                   group.states.begin(),
                                   group.states.end(),
                                   candidate.state) !=
                                   group.states.end() &&
                               successor ==
                                   choice.successor_state;
                    });
                if (offered !=
                    kernel.observation_choice_options.end()) {
                    return offered->state == kNoId
                               ? entry_state
                               : offered->state;
                }
            }
        }
        throw AdapterFailure(
            PolicyExactLiftStatus::UnsupportedPrimitiveKernel,
            "observed planner option cannot resolve one offered "
            "successor set");
    }

    std::vector<std::uint32_t> chosen_successors(
            const std::uint32_t coarse_state,
            const PlannerOperator& planner) {
        if (planner.kind == PlannerOperatorKind::FixedOption) {
            const std::uint32_t operator_index =
                solved_.policy.at(coarse_state).index;
            const OptionKernel& kernel =
                coarse_.option_kernel(
                    coarse_state, operator_index);
            if (!kernel.supported || !kernel.legal ||
                !kernel.terminates_almost_surely) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::
                        UnsupportedPrimitiveKernel,
                    "selected planner option has no legal exact "
                    "kernel");
            }
            (void)resource_cost(kernel.expected_resources);
            std::vector<std::uint32_t> result;
            const std::vector<ObservedUnveilPreference>
                preferences =
                    option_preferences(
                        coarse_state, coarse_, coarse_state,
                        kernel, false);
            for (const OutcomeEntry& exit : kernel.exits) {
                if (exit.probability != 0.0) {
                    result.push_back(
                        exit.state == kNoId
                            ? coarse_state
                            : exit.state);
                }
            }
            for (const OutcomeChoiceGroup& group :
                 kernel.observation_choice_groups) {
                if (group.probability == 0.0) continue;
                result.push_back(resolve_option_choice(
                    preferences, coarse_state,
                    group, kernel));
            }
            return result;
        }
        const std::uint32_t action =
            planner.primitive_action;
        const ActionDescriptor& descriptor =
            coarse_.registry().actions.at(action);
        if (!action_legal(
                coarse_.session(), descriptor,
                coarse_.state(coarse_state))) {
            throw AdapterFailure(
                PolicyExactLiftStatus::InvalidSolveState,
                "coarse policy selects an illegal primitive action");
        }
        (void)primitive_action_cost(descriptor);
        const OutcomeDistribution& distribution =
            coarse_.outcomes(
                coarse_state, action,
                options_.goal_progress_gated_reforges);
        if (!distribution.supported) {
            throw AdapterFailure(
                PolicyExactLiftStatus::
                    UnsupportedPrimitiveKernel,
                "selected primitive action has no exact kernel");
        }
        if (distribution.choice_groups.empty()) {
            std::vector<std::uint32_t> result;
            result.reserve(distribution.entries.size());
            for (const OutcomeEntry& entry : distribution.entries) {
                if (entry.probability != 0.0) {
                    result.push_back(entry.state);
                }
            }
            return result;
        }
        std::vector<std::uint32_t> result;
        result.reserve(distribution.choice_groups.size());
        const std::vector<std::uint32_t> preferences =
            primitive_preferences(
                coarse_state, coarse_, coarse_state,
                distribution, false);
        for (const OutcomeChoiceGroup& group :
             distribution.choice_groups) {
            if (group.probability == 0.0) continue;
            result.push_back(resolve_primitive_choice(
                preferences, coarse_state,
                group, distribution));
        }
        return result;
    }

    void discover_coarse_policy() {
        if (!solved_.policy_available ||
            solved_.start_state == kNoId ||
            solved_.start_state >= coarse_.state_count()) {
            throw AdapterFailure(
                solved_.policy_available
                    ? PolicyExactLiftStatus::InvalidSolveState
                    : PolicyExactLiftStatus::NoPolicy,
                solved_.policy_available
                    ? "solved policy has an invalid start state"
                    : "solve did not publish a policy");
        }
        std::set<std::uint32_t> pending{solved_.start_state};
        require_local_memory(
            u32_set_bytes(pending),
            "coarse policy discovery worklist");
        while (!pending.empty()) {
            const std::uint32_t state = *pending.begin();
            pending.erase(pending.begin());
            if (coarse_policy_.contains(state)) continue;
            if (coarse_policy_.size() >= limits_.max_coarse_states) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::ResourceCap,
                    "coarse policy discovery reached max_coarse_states",
                    "max_coarse_states");
            }
            CoarsePolicyNode node;
            if (!coarse_.is_goal_state(coarse_.state(state))) {
                const PlannerOperator& planner =
                    coarse_operator(state);
                node.coarse_operator =
                    solved_.policy[state].index;
                node.selected = make_coarse_selected(
                    state, node.coarse_operator);
                require_local_memory(
                    u32_set_bytes(pending) +
                        coarse_policy_node_bytes(node),
                    "coarse selected-policy discovery");
                node.successors =
                    chosen_successors(state, planner);
                require_local_memory(
                    u32_set_bytes(pending) +
                        coarse_policy_node_bytes(node),
                    "coarse selected-policy kernel discovery");
                std::sort(
                    node.successors.begin(), node.successors.end());
                node.successors.erase(
                    std::unique(
                        node.successors.begin(),
                        node.successors.end()),
                    node.successors.end());
                for (const std::uint32_t successor :
                     node.successors) {
                    if (successor == kNoId ||
                        successor >= solved_.policy.size()) {
                        throw AdapterFailure(
                            PolicyExactLiftStatus::InvalidSolveState,
                            "coarse primitive kernel escapes the solved "
                            "policy table");
                    }
                    pending.insert(successor);
                    require_local_memory(
                        u32_set_bytes(pending) +
                            coarse_policy_node_bytes(node),
                        "coarse policy discovery worklist");
                }
                telemetry_.coarse_policy_edges +=
                    node.successors.size();
            }
            coarse_policy_.emplace(state, std::move(node));
            telemetry_.coarse_policy_states =
                static_cast<std::uint32_t>(coarse_policy_.size());
            require_local_memory(
                u32_set_bytes(pending),
                "coarse policy discovery");
        }
    }

    void propagate_backward_observations() {
        std::vector<PolicyObservationNode> nodes;
        nodes.reserve(coarse_policy_.size());
        require_local_memory(
            nodes.capacity() * sizeof(PolicyObservationNode),
            "coarse observation fixed-point input");
        for (const auto& [state, policy] : coarse_policy_) {
            nodes.push_back({
                state, policy.selected, {},
                policy.successors});
            require_local_memory(
                policy_observation_nodes_bytes(nodes),
                "coarse observation fixed-point input");
        }
        const std::uint64_t nodes_bytes =
            policy_observation_nodes_bytes(nodes);
        std::uint64_t fixed_point_peak = nodes_bytes;
        saturating_add(fixed_point_peak, nodes_bytes);
        saturating_add(fixed_point_peak, nodes_bytes);
        saturating_add(
            fixed_point_peak,
            coarse_policy_.size() *
                (sizeof(std::pair<
                     const std::uint32_t, std::uint32_t>) +
                 3 * sizeof(void*)));
        require_local_memory(
            fixed_point_peak,
            "coarse observation fixed-point work");
        PolicyObservationFixedPoint fixed =
            propagate_policy_observations(
                std::move(nodes),
                limits_.max_refinement_rounds);
        require_local_memory(
            policy_observation_fixed_bytes(fixed),
            "coarse observation fixed-point result");
        telemetry_.backward_observation_rounds =
            fixed.rounds;
        if (!fixed.complete) {
            throw AdapterFailure(
                fixed.round_cap
                    ? PolicyExactLiftStatus::ResourceCap
                    : PolicyExactLiftStatus::InvalidSolveState,
                fixed.failure_reason,
                fixed.round_cap
                    ? "max_refinement_rounds"
                    : std::string{});
        }
        consume_refinement_rounds(
            fixed.rounds,
            "coarse-policy observation fixed point");
        for (PolicyObservationAssignment& assignment :
             fixed.assignments) {
            CoarsePolicyNode& node =
                coarse_policy_.at(assignment.state_id);
            node.required = std::move(assignment.required);
            node.observations_propagated = true;
            require_local_memory(
                policy_observation_fixed_bytes(fixed),
                "coarse observation assignment");
        }
        for (auto& [state, node] : coarse_policy_) {
            (void)state;
            if (!node.selected.has_value()) continue;
            node.selected->routing_observes = node.required;
            require_local_memory(
                policy_observation_fixed_bytes(fixed),
                "coarse observation routing assignment");
        }
    }

    void ensure_coarse_policy_parent(
            const std::uint32_t state) {
        if (coarse_policy_.contains(state)) return;
        if (coarse_policy_.size() >= limits_.max_coarse_states) {
            throw AdapterFailure(
                PolicyExactLiftStatus::ResourceCap,
                "local exact policy discovery reached "
                "max_coarse_states",
                "max_coarse_states");
        }
        if (state >= solved_.policy.size()) {
            throw AdapterFailure(
                PolicyExactLiftStatus::CoarseMappingFailure,
                "local exact successor maps outside the solved policy "
                "table");
        }
        CoarsePolicyNode node;
        if (!coarse_.is_goal_state(coarse_.state(state))) {
            const PlannerOperator& planner =
                coarse_operator(state);
            (void)planner;
            node.coarse_operator =
                solved_.policy[state].index;
            if (!strict_operator_by_coarse_.contains(
                    node.coarse_operator)) {
                operator_vocabulary_widening_witness_ =
                    OperatorVocabularyWideningWitness{
                        state, node.coarse_operator};
                throw AdapterFailure(
                    PolicyExactLiftStatus::
                        CoarseMappingFailure,
                    "policy-reachable exact successor selected an "
                    "operator outside the current witness-driven "
                    "runtime vocabulary");
            }
            node.selected = make_selected(
                state,
                strict_operator_for(node.coarse_operator),
                std::nullopt);
            require_local_memory(
                coarse_policy_node_bytes(node),
                "local coarse-policy parent construction");
        }
        coarse_policy_.emplace(state, std::move(node));
        telemetry_.coarse_policy_states =
            static_cast<std::uint32_t>(coarse_policy_.size());
        require_local_memory(
            0, "local coarse-policy parent construction");
    }

    AbstractState coarsen(const AbstractState& strict_state) const {
        AbstractState result;
        result.slot_status = strict_state.slot_status;
        for (std::size_t slot = 0;
             slot < strict_state.goal_member_class_tokens.size();
             ++slot) {
            const std::uint32_t strict_token =
                strict_state.goal_member_class_tokens[slot];
            if (strict_token == 0) continue;
            const std::vector<std::uint32_t>& mapping =
                strict_goal_member_to_coarse_[slot];
            if (strict_token > mapping.size()) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::CoarseMappingFailure,
                    "strict goal-member token has no coarse parent");
            }
            result.goal_member_class_tokens[slot] =
                mapping[strict_token - 1];
        }
        result.fractured_goal_mask =
            strict_state.fractured_goal_mask;
        result.crafted_goal_mask = strict_state.crafted_goal_mask;
        result.blocked_mask = strict_state.blocked_mask;
        result.prefix_count = strict_state.prefix_count;
        result.suffix_count = strict_state.suffix_count;
        result.rarity = strict_state.rarity;
        result.influence_bits = strict_state.influence_bits;
        result.veiled_side = strict_state.veiled_side;
        result.searing_exarch_tier =
            strict_state.searing_exarch_tier;
        result.eater_of_worlds_tier =
            strict_state.eater_of_worlds_tier;
        result.flags = strict_state.flags;
        result.fractured_metamod_flags =
            strict_state.fractured_metamod_flags;
        result.goal_progress_retry_basin =
            strict_state.goal_progress_retry_basin;
        const std::size_t parent_count =
            coarse_.layout().junk_classes.size();
        result.junk_counts.assign(parent_count, 0);
        result.fractured_junk_counts.assign(parent_count, 0);
        result.crafted_junk_counts.assign(parent_count, 0);
        result.fractured_crafted_junk_counts.assign(
            parent_count, 0);

        const auto add_count =
            [&](CompactCountVector& target,
                const std::uint32_t parent,
                const std::uint8_t amount) {
                if (amount == 0) return;
                if (parent == kNoId || parent >= target.size()) {
                    throw AdapterFailure(
                        PolicyExactLiftStatus::CoarseMappingFailure,
                        "occupied strict junk carrier has no unique "
                        "coarse parent");
                }
                const std::uint32_t total =
                    static_cast<std::uint32_t>(target[parent]) +
                    amount;
                if (total > kMaxExplicitAffixes) {
                    throw AdapterFailure(
                        PolicyExactLiftStatus::CoarseMappingFailure,
                        "strict-to-coarse junk count overflow");
                }
                target[parent] =
                    static_cast<std::uint8_t>(total);
            };
        if (strict_state.junk_counts.size() !=
                strict_junk_to_coarse_.size() ||
            strict_state.fractured_junk_counts.size() !=
                strict_junk_to_coarse_.size() ||
            strict_state.crafted_junk_counts.size() !=
                strict_junk_to_coarse_.size() ||
            strict_state.fractured_crafted_junk_counts.size() !=
                strict_junk_to_coarse_.size()) {
            throw AdapterFailure(
                PolicyExactLiftStatus::CoarseMappingFailure,
                "strict state junk vector disagrees with its layout");
        }
        for (std::uint32_t index = 0;
             index < strict_junk_to_coarse_.size();
             ++index) {
            const std::uint32_t parent =
                strict_junk_to_coarse_[index];
            add_count(
                result.junk_counts, parent,
                strict_state.junk_counts[index]);
            add_count(
                result.fractured_junk_counts, parent,
                strict_state.fractured_junk_counts[index]);
            add_count(
                result.crafted_junk_counts, parent,
                strict_state.crafted_junk_counts[index]);
            add_count(
                result.fractured_crafted_junk_counts, parent,
                strict_state.fractured_crafted_junk_counts[index]);
        }
        return result;
    }

    std::uint32_t coarse_parent(
            const std::uint32_t strict_state) {
        const AbstractState parent_state =
            coarsen(strict_->state(strict_state));
        const auto parent =
            coarse_state_by_key_.find(
                abstract_state_key(parent_state));
        if (parent == coarse_state_by_key_.end() ||
            parent->second >= solved_.policy.size()) {
            throw AdapterFailure(
                PolicyExactLiftStatus::CoarseMappingFailure,
                "strict carrier " +
                    std::to_string(strict_state) +
                    " maps outside the solved coarse graph");
        }
        return parent->second;
    }

    std::pair<ExactState, bool> intern_strict_state(
            const std::uint32_t strict_state) {
        const std::uint32_t parent = coarse_parent(strict_state);
        ensure_coarse_policy_parent(parent);
        const auto policy = coarse_policy_.find(parent);

        ExactState exact;
        exact.coarse_state = parent;
        exact.coarse_state_key =
            abstract_state_key(coarse_.state(parent));
        const StableKey raw_key =
            exact_refinement_state_key(
                strict_->state(strict_state),
                exact.coarse_state_key);
        exact.stable_key = raw_key;
        exact.goal =
            strict_->is_goal_state(strict_->state(strict_state));
        exact.terminal = exact.goal;
        if (exact.goal !=
            coarse_.is_goal_state(coarse_.state(parent))) {
            throw AdapterFailure(
                PolicyExactLiftStatus::CoarseMappingFailure,
                "strict and coarse goal classification disagree");
        }
        const AbstractFeatureExtraction extraction =
            extract_strict_abstract_features(
                strict_->session(),
                strict_->layout(),
                strict_->state(strict_state),
                (enable_local_reoptimization_ &&
                 reoptimization_parents_.contains(parent)) ||
                        exact_policy_.contains(raw_key) ||
                        !policy->second.observations_propagated
                    ? carrier_requirement_
                    : policy->second.required);
        if (!extraction.complete()) {
            throw AdapterFailure(
                PolicyExactLiftStatus::ObservationUnavailable,
                "strict layout discarded a policy-required observation "
                "(mask " +
                    std::to_string(extraction.unavailable_features) +
                    ")");
        }
        exact.features = extraction.features;
        const std::optional<StableKey> collapse_key =
            policy_collapse_key(
                strict_state, exact.coarse_state_key,
                exact.terminal, exact.features,
                policy->second);
        if (collapse_key.has_value()) {
            exact.stable_key = *collapse_key;
            if (exact.terminal) exact.features.clear();
        }
        if (!exact.terminal) {
            if (!policy->second.selected.has_value()) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::InvalidSolveState,
                    "non-terminal strict carrier has no coarse policy "
                    "action");
            }
            const std::optional<StableKey> operation =
                canonical_operation_state_signature(
                    strict_->session(),
                    strict_->layout(),
                    strict_->state(strict_state),
                    *policy->second.selected);
            if (!operation.has_value()) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::ObservationUnavailable,
                    "strict carrier cannot form its canonical operation "
                    "signature");
            }
        }
        /*
         * Witness-driven/local carriers retain their complete strict key.
         * During the initial fixed-policy lift, terminal carriers and rows
         * with the same engine-authored selected-operation observation
         * signature may share a semantic key. Stable destructive reforges
         * additionally include their collision-checked exact kernel
         * signature. Backward observation propagation puts every downstream
         * survivor in the operation signature, and every strict member is
         * retained below for compilation; no representative identity is
         * discarded.
         */
        const auto existing = known_.find(exact.stable_key);
        if (existing != known_.end()) {
            if (existing->second.strict_state != strict_state ||
                existing->second.coarse_state != parent ||
                existing->second.terminal != exact.terminal) {
                if (!existing->second.policy_collapsed ||
                    existing->second.coarse_state != parent ||
                    existing->second.terminal != exact.terminal ||
                    existing->second.collapsed_features !=
                        exact.features) {
                    throw AdapterFailure(
                        PolicyExactLiftStatus::ObservationUnavailable,
                        "collision-free strict state identity changed");
                }
                existing->second.equivalent_strict_states.push_back(
                    strict_state);
                require_local_memory(
                    existing->second.equivalent_strict_states.capacity() *
                        sizeof(std::uint32_t),
                    "contract-proved policy carrier collapse");
            } else if (existing->second.policy_collapsed &&
                       existing->second.collapsed_features !=
                           exact.features) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::ObservationUnavailable,
                    "collision-free strict state identity changed");
            }
            ++telemetry_.canonical_successor_collapses;
            return {std::move(exact), false};
        }
        if (known_.size() >= limits_.max_exact_states) {
            throw AdapterFailure(
                PolicyExactLiftStatus::ResourceCap,
                "strict policy discovery reached max_exact_states",
                "max_exact_states");
        }
        std::uint64_t state_projection =
            sizeof(decltype(known_)::value_type) +
            3 * sizeof(void*);
        saturating_add(
            state_projection,
            exact_state_bytes(exact));
        require_local_memory(
            state_projection,
            "strict exact-carrier construction");
        known_.emplace(
            exact.stable_key,
            KnownState{
                strict_state,
                parent,
                exact.terminal,
                collapse_key.has_value(),
                {},
                collapse_key.has_value()
                    ? exact.features
                    : FeatureSignature{}});
        ++telemetry_.strict_states_discovered;
        ++telemetry_.strict_carriers_materialized;
        require_local_memory(
            exact_state_bytes(exact),
            "strict exact-carrier construction");
        return {std::move(exact), true};
    }

    ExactActionKernel build_kernel(
            const StableKey& key,
            const SelectedAction& selected) {
        const KnownState& known = known_.at(key);
        if (selected.action_id >= strict_->operators().size()) {
            throw AdapterFailure(
                PolicyExactLiftStatus::InvalidSolveState,
                "refined selected operator is outside the admitted "
                "vocabulary");
        }
        const PlannerOperator& planner =
            strict_->operators().at(selected.action_id);
        const ExactChoiceRecipe& recipe =
            choice_recipe(key, selected);
        refresh_strict_resource_cap(
            "strict exact-kernel evaluation");
        ExactActionKernel kernel;
        const auto add =
            [&](const std::uint32_t successor,
                const double probability) {
                if (probability == 0.0) return;
                auto [exact, inserted] =
                    intern_strict_state(
                        successor == kNoId
                            ? known.strict_state
                            : successor);
                (void)inserted;
                kernel.transitions.push_back(
                    {std::move(exact), probability});
            };
        if (planner.kind == PlannerOperatorKind::Primitive) {
            const std::uint32_t action =
                planner.primitive_action;
            const ActionDescriptor& descriptor =
                strict_->registry().actions.at(action);
            if (!action_legal(
                    strict_->session(), descriptor,
                    strict_->state(known.strict_state))) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::ObservationUnavailable,
                    "one exact refinement makes its coarse selected "
                    "primitive illegal");
            }
            const OutcomeDistribution& distribution =
                strict_->outcomes(
                    known.strict_state, action,
                    options_.goal_progress_gated_reforges);
            if (!distribution.supported) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::
                        UnsupportedPrimitiveKernel,
                    "selected primitive has no strict exact kernel");
            }
            kernel.action_cost =
                primitive_action_cost(descriptor);
            if (distribution.choice_groups.empty()) {
                for (const OutcomeEntry& entry :
                     distribution.entries) {
                    add(entry.state, entry.probability);
                }
            } else {
                for (const OutcomeChoiceGroup& group :
                     distribution.choice_groups) {
                    add(
                        resolve_primitive_choice(
                            recipe.primitive_preferences,
                            known.strict_state,
                            group, distribution),
                        group.probability);
                }
            }
        } else {
            const OptionKernel& option =
                strict_->option_kernel(
                    known.strict_state,
                    selected.action_id);
            if (!option.supported || !option.legal ||
                !option.terminates_almost_surely) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::ObservationUnavailable,
                    "one exact refinement makes its selected planner "
                    "option illegal or unsupported");
            }
            kernel.action_cost =
                resource_cost(option.expected_resources);
            for (const OutcomeEntry& exit : option.exits) {
                add(exit.state, exit.probability);
            }
            for (const OutcomeChoiceGroup& group :
                  option.observation_choice_groups) {
                const std::uint32_t chosen = resolve_option_choice(
                    recipe.option_preferences,
                    known.strict_state,
                    group, option);
                add(chosen, group.probability);
            }
        }
        if (kernel.transitions.empty()) {
            throw AdapterFailure(
                PolicyExactLiftStatus::UnsupportedPrimitiveKernel,
                "selected planner operator produced an empty strict "
                "kernel");
        }
        return kernel;
    }

    std::uint64_t projected_lifted_construction_bytes(
            const RefinementResult& refinement) {
        const std::uint64_t states = strict_->state_count();
        std::uint64_t bytes = sizeof(SolveResult);
        const auto add =
            [&](const std::uint64_t amount) {
                if (amount >
                    std::numeric_limits<std::uint64_t>::max() -
                        bytes) {
                    bytes =
                        std::numeric_limits<std::uint64_t>::max();
                } else {
                    bytes += amount;
                }
            };
        add(states * sizeof(double));
        add(states * sizeof(PolicyOperatorRef));
        add(states * 3ull * sizeof(std::uint8_t));
        add(
            states *
            (sizeof(std::vector<std::uint32_t>) +
             sizeof(std::vector<ObservedUnveilPreference>)));
        /* Returned quotient projection plus deterministic class scratch. */
        add(states * sizeof(std::uint32_t));
        add(
            refinement.classes.size() *
            sizeof(std::uint32_t));
        add(
            refinement.classes.size() *
            sizeof(double));
        refresh_strict_resource_cap(
            "strict lifted-sidecar projection");
        for (const StateClassAssignment& assignment :
             refinement.assignments) {
            if (assignment.class_id >= refinement.classes.size()) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::InvalidSolveState,
                    "lifted projection assignment has no refined "
                    "class");
            }
            const std::optional<SelectedAction>& selected =
                refinement.classes[assignment.class_id]
                    .selected_action;
            if (!selected.has_value()) {
                continue;
            }
            add(exact_choice_recipe_bytes(
                choice_recipe(
                    assignment_exact_state(
                        refinement, assignment),
                    *selected)));
        }
        add(
            refinement.classes.size() *
            sizeof(RefinedPolicyCompileClass));
        add(
            states * sizeof(std::uint32_t));
        for (const RefinedPolicyClass& policy_class :
             refinement.classes) {
            add(requirement_bytes(
                policy_class.required_observations));
            add(feature_signature_bytes(
                policy_class.observation_signature));
            if (policy_class.selected_action.has_value()) {
                add(selected_action_bytes(
                    *policy_class.selected_action));
            }
            add(
                policy_class.transitions.size() *
                sizeof(ProjectedTransition));
        }
        return bytes;
    }

    SolveResult make_lifted_solve_result(
            const RefinementResult& refinement,
            const PolicyEvaluationResult& evaluation,
            RefinedPolicyCompileRouting* refined_routing) {
        SolveResult lifted;
        lifted.converged = solved_.converged;
        lifted.policy_available = true;
        /*
         * This temporary result describes an exactly evaluated fixed strict
         * policy, not the outer solver's global-optimality claim. Compile it
         * in exact-domain mode so the generic bounded compiler does not
         * manufacture a Restart default outside the proven refined domain.
         * The outer SolveResult retains the coarse lower bound and derives
         * its public bounded status from the refined exact J.
         */
        lifted.policy_status = SolvePolicyStatus::Exact;
        lifted.termination = solved_.termination;
        lifted.target_fired = solved_.target_fired;
        lifted.target_met = solved_.target_met;
        /*
         * A coarse value is not an admissible bound for a split that just
         * disproved lumpability. Primitive prices are nonnegative, so zero
         * remains the conservative exact-domain lower bound.
         */
        lifted.lower_bound = 0.0;
        if (evaluation.start_values.size() != 1 ||
            !std::isfinite(evaluation.start_values.front())) {
            throw AdapterFailure(
                PolicyExactLiftStatus::InvalidSolveState,
                "lifted exact policy has no finite root value");
        }
        lifted.upper_bound = evaluation.start_values.front();
        lifted.evaluated_policy_cost =
            evaluation.start_values.front();
        lifted.absolute_optimality_gap = std::max(
            0.0, lifted.upper_bound - lifted.lower_bound);
        lifted.relative_optimality_gap =
            std::abs(lifted.upper_bound) > 1e-12
                ? lifted.absolute_optimality_gap /
                      std::abs(lifted.upper_bound)
                : lifted.absolute_optimality_gap;
        lifted.requested_absolute_optimality_gap =
            solved_.requested_absolute_optimality_gap;
        lifted.requested_relative_optimality_gap =
            solved_.requested_relative_optimality_gap;
        lifted.options = options_;
        lifted.has_exact_start_item = true;
        /*
         * `root.strict_state` was interned from the authored item, but a
         * semantic strict class may contain several equivalent modifier ids.
         * Preserve the actual input instead of materializing a class member.
         */
        lifted.exact_start_item = exact_start_;

        const std::uint32_t state_count = strict_->state_count();
        lifted.values.assign(
            state_count, std::numeric_limits<double>::infinity());
        lifted.policy.assign(state_count, PolicyOperatorRef{});
        lifted.expanded.assign(state_count, 0);
        lifted.goal_states.assign(state_count, 0);
        lifted.policy_reachable.assign(state_count, 0);
        lifted.unveil_preferences.resize(state_count);
        lifted.option_unveil_preferences.resize(state_count);
        lifted.behavioral_representative_by_state.resize(state_count);
        std::iota(
            lifted.behavioral_representative_by_state.begin(),
            lifted.behavioral_representative_by_state.end(), 0u);

        std::vector<std::uint32_t> representative_by_class(
            refinement.classes.size(), kNoId);
        if (refined_routing != nullptr) {
            refined_routing->classes.clear();
            refined_routing->classes.resize(refinement.classes.size());
        }
        for (const RefinedPolicyClass& policy_class :
             refinement.classes) {
            if (policy_class.class_id >=
                    representative_by_class.size() ||
                policy_class.exact_members.empty()) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::InvalidSolveState,
                    "refinement class has no deterministic strict "
                    "representative");
            }
            const StableKey& representative_key =
                *std::min_element(
                    policy_class.exact_members.begin(),
                    policy_class.exact_members.end());
            const auto representative =
                known_.find(representative_key);
            if (representative == known_.end() ||
                representative->second.strict_state >= state_count) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::InvalidSolveState,
                    "refinement class representative has no strict "
                    "carrier");
            }
            representative_by_class[policy_class.class_id] =
                representative->second.strict_state;
            if (refined_routing != nullptr) {
                RefinedPolicyCompileClass& compile_class =
                    refined_routing->classes[policy_class.class_id];
                compile_class.class_id = policy_class.class_id;
                compile_class.coarse_state = policy_class.coarse_state;
                compile_class.coarse_state_key =
                    policy_class.coarse_state_key;
                if (compile_class.coarse_state_key !=
                    abstract_state_key(
                        coarse_.state(
                            policy_class.coarse_state))) {
                    throw AdapterFailure(
                        PolicyExactLiftStatus::InvalidSolveState,
                        "refinement class semantic coarse-state key "
                        "disagrees with its numeric parent");
                }
                compile_class.representative_state =
                    representative->second.strict_state;
                compile_class.terminal = policy_class.terminal;
                compile_class.required_observations =
                    policy_class.required_observations;
                compile_class.observation_signature =
                    policy_class.observation_signature;
                compile_class.selected_action =
                    policy_class.selected_action;
                compile_class.action_cost =
                    policy_class.action_cost;
                compile_class.transitions =
                    policy_class.transitions;
            }
        }
        for (const StateClassAssignment& assignment :
             refinement.assignments) {
            const StableKey& exact_state =
                assignment_exact_state(refinement, assignment);
            const auto known = known_.find(exact_state);
            if (known == known_.end() ||
                known->second.strict_state >= state_count ||
                assignment.class_id >=
                    representative_by_class.size() ||
                representative_by_class[assignment.class_id] == kNoId) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::InvalidSolveState,
                    "refinement assignment has no strict carrier or "
                    "class representative");
            }
            const std::uint32_t representative =
                representative_by_class[assignment.class_id];
            lifted.behavioral_representative_by_state[
                known->second.strict_state] = representative;
            for (const std::uint32_t equivalent :
                 known->second.equivalent_strict_states) {
                if (equivalent >= state_count) {
                    throw AdapterFailure(
                        PolicyExactLiftStatus::InvalidSolveState,
                        "collapsed policy carrier exceeds the strict "
                        "state table");
                }
                lifted.behavioral_representative_by_state[equivalent] =
                    representative;
            }
            if (refined_routing != nullptr) {
                std::vector<std::uint32_t>& members =
                    refined_routing->classes[assignment.class_id]
                        .strict_members;
                members.push_back(known->second.strict_state);
                members.insert(
                    members.end(),
                    known->second.equivalent_strict_states.begin(),
                    known->second.equivalent_strict_states.end());
            }
        }
        if (refined_routing != nullptr) {
            for (RefinedPolicyCompileClass& compile_class :
                 refined_routing->classes) {
                std::sort(
                    compile_class.strict_members.begin(),
                    compile_class.strict_members.end());
                compile_class.strict_members.erase(
                    std::unique(
                        compile_class.strict_members.begin(),
                        compile_class.strict_members.end()),
                    compile_class.strict_members.end());
            }
        }
        const KnownState& root = known_.at(root_key_);
        lifted.start_state =
            lifted.behavioral_representative_by_state.at(
                root.strict_state);
        std::vector<double> value_by_class(
            refinement.classes.size(),
            std::numeric_limits<double>::infinity());
        for (const RefinedClassValue& value :
             evaluation.class_values) {
            if (value.class_id >= value_by_class.size()) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::InvalidSolveState,
                    "exact policy value has an invalid class id");
            }
            value_by_class[value.class_id] = value.value;
        }
        for (const RefinedPolicyClass& policy_class :
             refinement.classes) {
            if (policy_class.class_id >=
                    representative_by_class.size() ||
                representative_by_class[policy_class.class_id] ==
                    kNoId) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::InvalidSolveState,
                    "lifted class has no strict representative");
            }
            const std::uint32_t strict_state =
                representative_by_class[policy_class.class_id];
            if (!coarse_policy_.contains(
                    policy_class.coarse_state)) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::InvalidSolveState,
                    "lifted carrier has no coarse policy parent");
            }
            if (strict_state >= state_count) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::InvalidSolveState,
                    "lifted carrier exceeds strict state table");
            }
            lifted.policy_reachable[strict_state] = 1;
        }
        for (const StateClassAssignment& assignment :
             refinement.assignments) {
            if (assignment.class_id >= refinement.classes.size() ||
                assignment.class_id >= value_by_class.size() ||
                !std::isfinite(
                    value_by_class[assignment.class_id])) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::InvalidSolveState,
                    "lifted strict member has no exact class value");
            }
            const StableKey& exact_state =
                assignment_exact_state(refinement, assignment);
            const auto carrier = known_.find(exact_state);
            if (carrier == known_.end() ||
                carrier->second.strict_state >= state_count) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::InvalidSolveState,
                    "lifted strict member has no exact carrier");
            }
            const RefinedPolicyClass& policy_class =
                refinement.classes[assignment.class_id];
            std::optional<SelectedAction> selected;
            const PlannerOperator* planner = nullptr;
            const ExactChoiceRecipe* recipe = nullptr;
            if (!policy_class.terminal) {
                selected = selected_action(materialize_exact_state(
                    exact_state, carrier->second));
                if (!selected.has_value() ||
                    !policy_class.selected_action.has_value() ||
                    selected->action_id !=
                        policy_class.selected_action->action_id ||
                    selected->semantic_key !=
                        policy_class.selected_action->semantic_key ||
                    selected->action_id >=
                        strict_->operators().size()) {
                    throw AdapterFailure(
                        PolicyExactLiftStatus::InvalidSolveState,
                        "lifted strict member changed its refined "
                        "planner decision");
                }
                planner = &strict_->operators()[selected->action_id];
                recipe = &choice_recipe(
                    exact_state, *selected);
            }
            const auto apply_member =
                [&](const std::uint32_t strict_state) {
                    if (strict_state >= state_count) {
                        throw AdapterFailure(
                            PolicyExactLiftStatus::InvalidSolveState,
                            "lifted collapsed member exceeds the "
                            "strict state table");
                    }
                    lifted.expanded[strict_state] = 1;
                    lifted.goal_states[strict_state] =
                        policy_class.goal ? 1 : 0;
                    lifted.values[strict_state] =
                        value_by_class[assignment.class_id];
                    if (policy_class.terminal) return;
                    lifted.policy[strict_state] = PolicyOperatorRef{
                        planner->kind, selected->action_id};
                    if (planner->kind ==
                        PlannerOperatorKind::Primitive) {
                        if (recipe->kind ==
                            ExactChoiceRecipeKind::
                                PrimitiveObservedChoice) {
                            lifted.unveil_preferences[strict_state] =
                                recipe->primitive_preferences;
                        }
                    } else {
                        lifted.option_unveil_preferences[strict_state] =
                            recipe->option_preferences;
                    }
                };
            apply_member(carrier->second.strict_state);
            for (const std::uint32_t equivalent :
                 carrier->second.equivalent_strict_states) {
                apply_member(equivalent);
            }
        }
        return lifted;
    }
};

ExactPolicyRun ProductionPolicyOracle::evaluate_current_policy(
        std::vector<StableKey> exact_roots,
        const std::uint64_t caller_retained_bytes) {
    ExactPolicyRun run;
    run.roots = std::move(exact_roots);
    std::sort(run.roots.begin(), run.roots.end());
    run.roots.erase(
        std::unique(run.roots.begin(), run.roots.end()),
        run.roots.end());
    if (run.roots.empty()) {
        run.refinement.status = RefinementStatus::EmptyRequest;
        run.refinement.failure_reason =
            "local exact policy evaluation has no roots";
        return run;
    }

    std::map<std::uint32_t, std::vector<StableKey>> filtered;
    for (const StableKey& key : run.roots) {
        const auto found = known_.find(key);
        if (found == known_.end()) {
            run.refinement.status = RefinementStatus::InvalidState;
            run.refinement.failure_reason =
                "local exact policy root has no strict carrier";
            return run;
        }
        filtered[found->second.coarse_state].push_back(key);
    }
    for (auto& [unused, keys] : filtered) {
        (void)unused;
        std::sort(keys.begin(), keys.end());
        keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
    }

    std::uint64_t retained_before_refinement =
        caller_retained_bytes;
    saturating_add(
        retained_before_refinement,
        stable_keys_bytes(run.roots));
    if (retained_before_refinement >=
        limits_.max_estimated_memory_bytes) {
        run.refinement.status = RefinementStatus::ResourceCap;
        run.refinement.resource_cap =
            "max_estimated_memory_bytes";
        run.refinement.failure_reason =
            "caller-retained exact policy reached "
            "max_estimated_memory_bytes";
        return run;
    }
    RefinementLimits run_limits = limits_;
    if (refinement_rounds_consumed_ >=
        limits_.max_refinement_rounds) {
        run.refinement.status = RefinementStatus::ResourceCap;
        run.refinement.resource_cap =
            "max_refinement_rounds";
        run.refinement.failure_reason =
            "local exact policy reached max_refinement_rounds";
        return run;
    }
    run_limits.max_refinement_rounds =
        static_cast<std::uint32_t>(
            limits_.max_refinement_rounds -
            refinement_rounds_consumed_);
    run_limits.max_estimated_memory_bytes -=
        retained_before_refinement;
    RefinementRequest request;
    for (const auto& [coarse_state, unused] : filtered) {
        (void)unused;
        request.coarse_roots.push_back(coarse_state);
    }
    request.limits = run_limits;

    illegal_action_witness_.reset();
    exact_root_filter_ = std::move(filtered);
    try {
        run.refinement =
            refine_policy_exact(*this, std::move(request));
    } catch (...) {
        exact_root_filter_.reset();
        throw;
    }
    exact_root_filter_.reset();
    const std::uint64_t fixed_point_rounds =
        static_cast<std::uint64_t>(
            run.refinement.telemetry
                .selected_action_routing_rounds) +
        static_cast<std::uint64_t>(
            run.refinement.telemetry
                .observation_propagation_rounds) +
        run.refinement.telemetry
            .partition_refinement_rounds;
    consume_refinement_rounds(
        fixed_point_rounds,
        "local exact policy fixed point");
    telemetry_.exact_fixed_point_rounds +=
        fixed_point_rounds;
    if (run.refinement.status ==
        RefinementStatus::RefinementRoundCap) {
        run.refinement.status =
            RefinementStatus::ResourceCap;
        run.refinement.resource_cap =
            "max_refinement_rounds";
    }
    if (run.refinement.status != RefinementStatus::Complete ||
        !run.refinement.executable ||
        !run.refinement.lumpable) {
        if (illegal_action_witness_.has_value()) {
            std::uint64_t retained = caller_retained_bytes;
            saturating_add(retained, exact_policy_run_bytes(run));
            saturating_add(
                retained, sizeof(StableKey));
            saturating_add(
                retained,
                stable_key_bytes(*illegal_action_witness_));
            require_local_memory(
                retained,
                "local exact illegal-action witness");
            run.defect_witnesses.push_back(
                *illegal_action_witness_);
        }
        return run;
    }

    PolicyEvaluationRequest evaluation_request;
    for (const StableKey& key : run.roots) {
        const auto found = std::find_if(
            run.refinement.assignments.begin(),
            run.refinement.assignments.end(),
            [&](const StateClassAssignment& assignment) {
                return assignment_exact_state(
                           run.refinement, assignment) == key;
            });
        if (found == run.refinement.assignments.end()) {
            run.evaluation.status =
                PolicyEvaluationStatus::InvalidStartClass;
            run.evaluation.failure_reason =
                "refined local policy omitted an exact root";
            return run;
        }
        evaluation_request.start_classes.push_back(
            found->class_id);
    }
    evaluation_request.limits.max_reachable_classes =
        run_limits.max_refinement_classes;
    evaluation_request.limits.max_component_iterations =
        std::max<std::uint32_t>(1, options_.max_sweeps);

    /*
     * The refinement and oracle remain live during exact SCC evaluation.
     * Give the evaluator only the residual of the one publication budget;
     * its own ledger then bounds scratch and returned values inside that
     * residual.
     */
    std::uint64_t retained = estimated_owned_bytes();
    const auto add_retained =
        [&](const std::uint64_t bytes) {
            retained =
                bytes >
                        std::numeric_limits<std::uint64_t>::max() -
                            retained
                    ? std::numeric_limits<std::uint64_t>::max()
                    : retained + bytes;
    };
    add_retained(refinement_result_bytes(run.refinement));
    add_retained(
        evaluation_request.start_classes.capacity() *
        sizeof(std::uint32_t));
    if (retained >=
        run_limits.max_estimated_memory_bytes) {
        run.evaluation.status =
            PolicyEvaluationStatus::ResourceCap;
        run.evaluation.resource_cap =
            "max_estimated_memory_bytes";
        run.evaluation.failure_reason =
            "oracle and refinement leave no exact-policy evaluation "
            "memory";
        return run;
    }
    evaluation_request.limits.max_estimated_memory_bytes =
        run_limits.max_estimated_memory_bytes - retained;
    run.evaluation = evaluate_refined_policy_exact(
        run.refinement, std::move(evaluation_request));
    if (run.evaluation.status ==
            PolicyEvaluationStatus::ImproperPolicy) {
        if (run.evaluation.improper_component_count == 0 ||
            run.evaluation.improper_component_classes.empty()) {
            run.evaluation.status =
                PolicyEvaluationStatus::InvalidPolicy;
            run.evaluation.failure_reason =
                "improper policy omitted its canonical component";
            return run;
        }
        std::size_t projected_members = 0;
        std::uint64_t projected_member_bytes = 0;
        for (const std::uint32_t class_id :
             run.evaluation.improper_component_classes) {
            const auto found = std::find_if(
                run.refinement.classes.begin(),
                run.refinement.classes.end(),
                [class_id](const RefinedPolicyClass& candidate) {
                    return candidate.class_id == class_id;
                });
            if (found == run.refinement.classes.end() ||
                found->terminal ||
                found->exact_members.empty() ||
                projected_members >
                    std::numeric_limits<std::size_t>::max() -
                        found->exact_members.size()) {
                run.evaluation.status =
                    PolicyEvaluationStatus::InvalidRefinement;
                run.evaluation.failure_reason =
                    "improper component has no exact carrier members";
                return run;
            }
            projected_members += found->exact_members.size();
            for (const StableKey& member : found->exact_members) {
                saturating_add(
                    projected_member_bytes,
                    sizeof(StableKey));
                saturating_add(
                    projected_member_bytes,
                    stable_key_bytes(member));
            }
        }
        std::uint64_t retained = caller_retained_bytes;
        saturating_add(retained, exact_policy_run_bytes(run));
        saturating_add(retained, projected_member_bytes);
        require_local_memory(
            retained,
            "local exact improper-component witnesses");
        run.defect_witnesses.reserve(projected_members);
        for (const std::uint32_t class_id :
             run.evaluation.improper_component_classes) {
            const auto found = std::find_if(
                run.refinement.classes.begin(),
                run.refinement.classes.end(),
                [class_id](const RefinedPolicyClass& candidate) {
                    return candidate.class_id == class_id;
                });
            run.defect_witnesses.insert(
                run.defect_witnesses.end(),
                found->exact_members.begin(),
                found->exact_members.end());
        }
        std::sort(
            run.defect_witnesses.begin(),
            run.defect_witnesses.end());
        run.defect_witnesses.erase(
            std::unique(
                run.defect_witnesses.begin(),
                run.defect_witnesses.end()),
            run.defect_witnesses.end());
        if (run.defect_witnesses.empty()) {
            run.evaluation.status =
                PolicyEvaluationStatus::InvalidRefinement;
            run.evaluation.failure_reason =
                "improper component produced no exact witnesses";
            return run;
        }
        std::uint64_t actual_retained = caller_retained_bytes;
        saturating_add(
            actual_retained, exact_policy_run_bytes(run));
        require_local_memory(
            actual_retained,
            "returned local exact improper-component witnesses");
    }
    if (run.evaluation.status !=
            PolicyEvaluationStatus::Complete ||
        !run.evaluation.converged ||
        !run.evaluation.proper ||
        run.evaluation.start_values.size() !=
            run.roots.size()) {
        return run;
    }

    std::vector<double> value_by_class(
        run.refinement.classes.size(),
        std::numeric_limits<double>::infinity());
    std::uint64_t value_construction_bytes =
        caller_retained_bytes;
    saturating_add(
        value_construction_bytes,
        exact_policy_run_bytes(run));
    saturating_add(
        value_construction_bytes,
        value_by_class.capacity() * sizeof(double));
    std::uint64_t projected_value_map_bytes =
        run.refinement.assignments.size() *
        (sizeof(decltype(
             run.value_by_exact)::value_type) +
         3 * sizeof(void*));
    for (const StateClassAssignment& assignment :
         run.refinement.assignments) {
        saturating_add(
            projected_value_map_bytes,
            stable_key_bytes(assignment_exact_state(
                run.refinement, assignment)));
    }
    saturating_add(
        value_construction_bytes,
        projected_value_map_bytes);
    require_local_memory(
        value_construction_bytes,
        "local exact value-map construction");
    for (const RefinedClassValue& value :
         run.evaluation.class_values) {
        if (value.class_id >= value_by_class.size()) {
            run.evaluation.status =
                PolicyEvaluationStatus::InvalidRefinement;
            run.evaluation.failure_reason =
                "local exact class value has an invalid id";
            return run;
        }
        value_by_class[value.class_id] = value.value;
    }
    for (const StateClassAssignment& assignment :
         run.refinement.assignments) {
        if (assignment.class_id >= value_by_class.size() ||
            !std::isfinite(value_by_class[assignment.class_id])) {
            run.evaluation.status =
                PolicyEvaluationStatus::InvalidRefinement;
            run.evaluation.failure_reason =
                "local exact assignment has no finite class value";
            return run;
        }
        run.value_by_exact.emplace(
            assignment_exact_state(run.refinement, assignment),
            value_by_class[assignment.class_id]);
    }
    std::uint64_t returned_bytes =
        caller_retained_bytes;
    saturating_add(
        returned_bytes, exact_policy_run_bytes(run));
    require_local_memory(
        returned_bytes,
        "the returned local exact policy");
    run.complete = true;
    return run;
}

void ProductionPolicyOracle::refresh_strict_resource_cap(
        const char* subject,
        const std::uint64_t caller_owned_bytes) {
    /*
     * CalcContext preflights only memory it owns. Refresh its child budget
     * before every call that can build a strict primitive/option kernel so
     * already-retained adapter maps and exact kernels share the same named
     * publication cap.
     */
    const std::uint64_t strict_owned_bytes =
        strict_->fast_estimated_owned_bytes();
    const std::uint64_t adapter_owned_bytes =
        estimated_owned_bytes();
    std::uint64_t non_strict_adapter_bytes =
        adapter_owned_bytes > strict_owned_bytes
            ? adapter_owned_bytes - strict_owned_bytes
            : 0;
    saturating_add(
        non_strict_adapter_bytes, caller_owned_bytes);
    if (non_strict_adapter_bytes >=
        limits_.max_estimated_memory_bytes) {
        throw AdapterFailure(
            PolicyExactLiftStatus::ResourceCap,
            std::string(subject) +
                " has no remaining exact-refinement memory",
            "max_estimated_memory_bytes");
    }
    strict_->set_solve_resource_caps(
        std::max<std::uint32_t>(
            1, limits_.max_exact_states),
        options_.max_reforge_work,
        false,
        limits_.max_estimated_memory_bytes -
            non_strict_adapter_bytes);
}

void ProductionPolicyOracle::consume_refinement_rounds(
        const std::uint64_t rounds,
        const char* subject) {
    const std::uint64_t limit =
        limits_.max_refinement_rounds;
    if (rounds > limit ||
        refinement_rounds_consumed_ > limit - rounds) {
        throw AdapterFailure(
            PolicyExactLiftStatus::ResourceCap,
            std::string(subject) +
                " reached max_refinement_rounds",
            "max_refinement_rounds");
    }
    refinement_rounds_consumed_ += rounds;
}

void ProductionPolicyOracle::require_local_memory(
        const std::uint64_t caller_owned_bytes,
        const char* subject) const {
    std::uint64_t total = estimated_owned_bytes();
    saturating_add(total, caller_owned_bytes);
    if (total >= limits_.max_estimated_memory_bytes) {
        total = audit_adapter_owned_bytes();
        saturating_add(total, caller_owned_bytes);
    }
    telemetry_.peak_adapter_owned_bytes = std::max(
        telemetry_.peak_adapter_owned_bytes, total);
    if (strict_ != nullptr) {
        telemetry_.strict_calc_owned_bytes = std::max(
            telemetry_.strict_calc_owned_bytes,
            strict_->fast_estimated_owned_bytes());
    }
    if (total >= limits_.max_estimated_memory_bytes) {
        throw AdapterFailure(
            PolicyExactLiftStatus::ResourceCap,
            std::string{subject} +
                " reached max_estimated_memory_bytes",
            "max_estimated_memory_bytes");
    }
    /*
     * `caller_owned_bytes` is a live projection for this check, not an
     * allocation delta.  The same scratch/vector projection is commonly
     * presented before and after one mutation, so accumulating it would
     * turn repeated cap checks into fictitious retained ownership.  Keep
     * the largest unaudited projection as the conservative bridge to the
     * next exact adapter audit; the periodic audit accounts for everything
     * that was actually retained.
     */
    adapter_memory_projected_growth_bytes_ = std::max(
        adapter_memory_projected_growth_bytes_,
        caller_owned_bytes);
}

void ProductionPolicyOracle::apply_policy_override(
        const StableKey& key,
        SelectedAction selected,
        std::vector<PolicyMutation>& mutations) {
    const auto known = known_.find(key);
    if (known == known_.end()) {
        throw AdapterFailure(
            PolicyExactLiftStatus::InvalidSolveState,
            "local policy mutation has no exact carrier");
    }
    const auto previous = exact_policy_.find(key);
    std::uint64_t projected =
        sizeof(PolicyMutation) +
        sizeof(decltype(exact_policy_)::value_type) +
        6 * sizeof(void*);
    saturating_add(projected, stable_key_bytes(key));
    saturating_add(
        projected, selected_action_bytes(selected));
    if (previous != exact_policy_.end()) {
        saturating_add(
            projected,
            selected_action_bytes(previous->second));
    }
    require_local_memory(
        projected, "local exact policy mutation");
    PolicyMutation mutation;
    mutation.key = &known->first;
    if (previous != exact_policy_.end()) {
        mutation.had_override = true;
        mutation.previous = previous->second;
    }
    mutations.push_back(std::move(mutation));
    exact_policy_.insert_or_assign(key, std::move(selected));
    require_local_memory(
        policy_mutations_bytes(mutations),
        "local exact policy mutation");
}

void ProductionPolicyOracle::rollback_policy_mutations(
        std::vector<PolicyMutation>& mutations,
        const std::size_t checkpoint) {
    while (mutations.size() > checkpoint) {
        PolicyMutation mutation =
            std::move(mutations.back());
        mutations.pop_back();
        if (mutation.key == nullptr) {
            throw AdapterFailure(
                PolicyExactLiftStatus::InvalidSolveState,
                "local policy rollback lost its exact key");
        }
        if (mutation.had_override) {
            exact_policy_.insert_or_assign(
                *mutation.key,
                std::move(*mutation.previous));
        } else {
            exact_policy_.erase(*mutation.key);
        }
    }
}

void ProductionPolicyOracle::release_completed_evaluation_storage(
        const std::vector<StableKey>& roots) {
    /*
     * A repair trial re-evaluates the policy from its roots. The strict
     * calculation remains the canonical carrier store, so retaining every
     * adapter-side KnownState from the completed failed evaluation merely
     * duplicates identities while the next (potentially very large) root
     * kernel is materialized. Keep only roots and active repair witnesses;
     * graph discovery deterministically recreates every other carrier needed
     * by the next evaluation. std::map erasure preserves pointers to the
     * retained witness nodes held by PolicyMutation.
     */
    for (auto known = known_.begin(); known != known_.end();) {
        const bool root = std::find(
            roots.begin(), roots.end(), known->first) != roots.end();
        if (root || repair_carriers_.contains(known->first)) {
            ++known;
        } else {
            known = known_.erase(known);
        }
    }
    std::map<KernelKey, ExactActionKernel>{}.swap(kernels_);
    adapter_memory_cache_initialized_ = false;
    adapter_memory_checks_since_audit_ = 0;
    adapter_memory_cached_non_child_bytes_ = 0;
    adapter_memory_projected_growth_bytes_ = 0;
}

std::optional<SelectedAction>
ProductionPolicyOracle::candidate_selection(
        const StableKey& key,
        const std::uint32_t operator_index,
        const std::uint64_t caller_retained_bytes) {
    if (!std::binary_search(
            admitted_operators_.begin(),
            admitted_operators_.end(), operator_index) ||
        operator_index >= strict_->operators().size()) {
        return std::nullopt;
    }
    const auto known = known_.find(key);
    if (known == known_.end() || known->second.terminal) {
        return std::nullopt;
    }
    const std::uint32_t coarse_operator =
        coarse_operator_for(operator_index);
    if (!coarse_.is_candidate_operator_admitted_for_state(
            known->second.coarse_state,
            coarse_operator)) {
        return std::nullopt;
    }
    try {
        const PlannerOperator& planner =
            strict_->operators()[operator_index];
        ExactChoiceRecipe recipe;
        std::vector<StableKey> continuation_roots;
        struct PrimitiveOffer {
            std::uint32_t mod_id = kNoId;
            std::uint32_t successor_state = kNoId;
            StableKey successor;
        };
        std::vector<PrimitiveOffer> primitive_offers;
        struct FixedOffer {
            std::uint32_t mod_id = kNoId;
            std::uint32_t successor_state = kNoId;
            std::uint32_t actual_state = kNoId;
            StableKey successor;
        };
        struct FixedObservation {
            std::uint32_t observation_state = kNoId;
            StableKey observation;
            std::vector<FixedOffer> offers;
        };
        std::vector<FixedObservation> fixed_observations;

        const auto intern_choice_successor =
            [&](const std::uint32_t state) {
                auto [exact, inserted] =
                    intern_strict_state(
                        state == kNoId
                            ? known->second.strict_state
                            : state);
                (void)inserted;
                continuation_roots.push_back(
                    exact.stable_key);
                return exact.stable_key;
            };
        if (planner.kind == PlannerOperatorKind::Primitive) {
            const ActionDescriptor& descriptor =
                strict_->registry()
                    .actions[planner.primitive_action];
            if (!descriptor.refinement.complete() ||
                !action_legal(
                    strict_->session(), descriptor,
                    strict_->state(
                        known->second.strict_state))) {
                return std::nullopt;
            }
            (void)primitive_action_cost(descriptor);
            refresh_strict_resource_cap(
                "strict candidate primitive evaluation");
            const OutcomeDistribution& distribution =
                strict_->outcomes(
                    known->second.strict_state,
                    planner.primitive_action,
                    options_.goal_progress_gated_reforges);
            if (!distribution.supported) {
                return std::nullopt;
            }
            if (!distribution.choice_groups.empty() ||
                !distribution.choice_options.empty()) {
                recipe.kind =
                    ExactChoiceRecipeKind::PrimitiveObservedChoice;
                std::map<
                    std::uint32_t,
                    std::pair<std::uint32_t, StableKey>>
                    successor_by_mod;
                for (const OutcomeChoiceOption& option :
                     distribution.choice_options) {
                    const std::uint32_t successor_state =
                        option.state == kNoId
                            ? known->second.strict_state
                            : option.state;
                    StableKey successor =
                        intern_choice_successor(option.state);
                    const auto [stored, inserted] =
                        successor_by_mod.emplace(
                            option.mod_id,
                            std::pair{
                                successor_state,
                                successor});
                    if (!inserted &&
                        stored->second.second != successor) {
                        throw AdapterFailure(
                            PolicyExactLiftStatus::
                                UnsupportedPrimitiveKernel,
                            "one primitive observed modifier has "
                            "multiple exact semantic successors");
                    }
                    if (!inserted) {
                        stored->second.first = std::min(
                            stored->second.first,
                            successor_state);
                    }
                }
                primitive_offers.reserve(
                    successor_by_mod.size());
                for (auto& [mod, successor] :
                     successor_by_mod) {
                    primitive_offers.push_back(
                        {mod, successor.first,
                         std::move(successor.second)});
                }
                if (!distribution.choice_groups.empty() &&
                    primitive_offers.empty()) {
                    return std::nullopt;
                }
            }
        } else {
            refresh_strict_resource_cap(
                "strict candidate-option evaluation");
            const OptionKernel& kernel =
                strict_->option_kernel(
                    known->second.strict_state,
                    operator_index);
            if (!kernel.supported || !kernel.legal ||
                !kernel.terminates_almost_surely) {
                return std::nullopt;
            }
            (void)resource_cost(kernel.expected_resources);
            recipe.kind =
                ExactChoiceRecipeKind::FixedOption;
            std::map<std::uint32_t, std::size_t>
                observation_index;
            for (const OutcomeChoiceOption& option :
                 kernel.observation_choice_options) {
                const std::uint32_t observation_state =
                    option.observation_state == kNoId
                        ? known->second.strict_state
                        : option.observation_state;
                if (observation_state >=
                    strict_->state_count()) {
                    throw AdapterFailure(
                        PolicyExactLiftStatus::
                            UnsupportedPrimitiveKernel,
                        "observed fixed option references an "
                        "invalid exact observation state");
                }
                auto [index, inserted] =
                    observation_index.emplace(
                        observation_state,
                        fixed_observations.size());
                if (inserted) {
                    fixed_observations.push_back({
                        observation_state,
                        exact_abstract_state_key(
                            strict_->state(
                                observation_state),
                            0),
                        {}});
                }
                FixedObservation& observation =
                    fixed_observations[index->second];
                const std::uint32_t successor_state =
                    option.state == kNoId
                        ? known->second.strict_state
                        : option.state;
                const std::uint32_t actual_state =
                    option.actual_state == kNoId
                        ? known->second.strict_state
                        : option.actual_state;
                StableKey successor =
                    intern_choice_successor(option.state);
                const auto duplicate = std::find_if(
                    observation.offers.begin(),
                    observation.offers.end(),
                    [&](const FixedOffer& candidate) {
                        return candidate.mod_id ==
                               option.mod_id;
                    });
                if (duplicate != observation.offers.end()) {
                    if (duplicate->successor != successor) {
                        throw AdapterFailure(
                            PolicyExactLiftStatus::
                                UnsupportedPrimitiveKernel,
                            "one fixed observed modifier has "
                            "multiple exact semantic successors");
                    }
                    continue;
                }
                observation.offers.push_back({
                    option.mod_id,
                    successor_state,
                    actual_state,
                    std::move(successor)});
            }
            if (!kernel.observation_choice_groups.empty() &&
                fixed_observations.empty()) {
                return std::nullopt;
            }
        }

        std::sort(
            continuation_roots.begin(),
            continuation_roots.end());
        continuation_roots.erase(
            std::unique(
                continuation_roots.begin(),
                continuation_roots.end()),
            continuation_roots.end());
        if (!continuation_roots.empty()) {
            std::uint64_t retained =
                caller_retained_bytes;
            saturating_add(
                retained,
                continuation_roots.capacity() *
                    sizeof(StableKey));
            for (const StableKey& root :
                 continuation_roots) {
                saturating_add(
                    retained, stable_key_bytes(root));
            }
            const auto propagate_continuation_cap =
                [&](const ExactPolicyRun& continuation) {
                    if (continuation.refinement.status ==
                        RefinementStatus::ResourceCap) {
                        throw AdapterFailure(
                            PolicyExactLiftStatus::ResourceCap,
                            continuation.refinement.failure_reason,
                            continuation.refinement.resource_cap);
                    }
                    if (continuation.evaluation.status ==
                        PolicyEvaluationStatus::ResourceCap) {
                        throw AdapterFailure(
                            PolicyExactLiftStatus::ResourceCap,
                            continuation.evaluation.failure_reason,
                            continuation.evaluation.resource_cap);
                    }
                };
            std::map<StableKey, double>
                continuation_values;
            ExactPolicyRun continuation =
                evaluate_current_policy(
                    continuation_roots, retained);
            propagate_continuation_cap(continuation);
            if (continuation.complete) {
                continuation_values =
                    continuation.value_by_exact;
                continuation = ExactPolicyRun{};
            } else {
                /*
                 * A current policy may be improper precisely because every
                 * authored observed choice retries. Evaluate alternatives
                 * independently so proper successors retain their exact
                 * values while an improper continuation ranks as +infinity.
                 * This is still one deterministic Bellman ordering, never a
                 * preference permutation search.
                 */
                continuation = ExactPolicyRun{};
                for (const StableKey& root :
                     continuation_roots) {
                    if (known_.at(root).terminal) {
                        continuation_values.emplace(root, 0.0);
                        continue;
                    }
                    std::uint64_t individual_retained =
                        retained;
                    saturating_add(
                        individual_retained,
                        continuation_values.size() *
                            (sizeof(decltype(
                                 continuation_values)::
                                 value_type) +
                             3 * sizeof(void*)));
                    ExactPolicyRun individual =
                        evaluate_current_policy(
                            {root}, individual_retained);
                    propagate_continuation_cap(individual);
                    if (individual.complete) {
                        const auto value =
                            individual.value_by_exact.find(root);
                        if (value ==
                                individual.value_by_exact.end() ||
                            !std::isfinite(value->second)) {
                            throw AdapterFailure(
                                PolicyExactLiftStatus::
                                    InvalidSolveState,
                                "observed candidate successor has "
                                "no exact continuation-policy "
                                "value");
                        }
                        continuation_values.emplace(
                            root, value->second);
                    } else if (
                        individual.evaluation.status ==
                            PolicyEvaluationStatus::
                                ImproperPolicy &&
                        !individual.defect_witnesses.empty()) {
                        continuation_values.emplace(
                            root,
                            std::numeric_limits<double>::
                                infinity());
                    } else {
                        return std::nullopt;
                    }
                }
            }
            std::uint64_t choice_retained = retained;
            saturating_add(
                choice_retained,
                continuation_values.size() *
                    (sizeof(decltype(
                         continuation_values)::value_type) +
                     3 * sizeof(void*)));
            for (const auto& [successor, unused] :
                 continuation_values) {
                (void)unused;
                saturating_add(
                    choice_retained,
                    stable_key_bytes(successor));
            }
            const auto exact_value =
                [&](const StableKey& successor) {
                    const auto found =
                        continuation_values.find(
                            successor);
                    if (found ==
                        continuation_values.end()) {
                        throw AdapterFailure(
                            PolicyExactLiftStatus::
                                InvalidSolveState,
                            "observed candidate successor has no "
                            "exact continuation-policy value");
                    }
                    return found->second;
                };
            if (recipe.kind ==
                ExactChoiceRecipeKind::PrimitiveObservedChoice) {
                std::uint64_t choice_scratch =
                    choice_retained;
                saturating_add(
                    choice_scratch,
                    sizeof(SolveTransitionCache));
                saturating_add(
                    choice_scratch,
                    primitive_offers.size() *
                        (sizeof(PrimitiveOffer) +
                         2 * sizeof(std::uint32_t) +
                         sizeof(double)));
                require_local_memory(
                    choice_scratch,
                    "shared sparse primitive-choice ordering");
                order_observed_offers_by_sparse_choice(
                    primitive_offers,
                    [&](const PrimitiveOffer& offer) {
                        return exact_value(offer.successor);
                    });
                recipe.primitive_preferences.reserve(
                    primitive_offers.size());
                for (const PrimitiveOffer& offer :
                     primitive_offers) {
                    recipe.primitive_preferences.push_back(
                        offer.mod_id);
                }
            } else if (
                recipe.kind ==
                ExactChoiceRecipeKind::FixedOption) {
                for (FixedObservation& observation :
                      fixed_observations) {
                    std::uint64_t choice_scratch =
                        choice_retained;
                    saturating_add(
                        choice_scratch,
                        sizeof(SolveTransitionCache));
                    saturating_add(
                        choice_scratch,
                        observation.offers.size() *
                            (sizeof(FixedOffer) +
                             2 * sizeof(std::uint32_t) +
                             sizeof(double)));
                    require_local_memory(
                        choice_scratch,
                        "shared sparse fixed-choice ordering");
                    order_observed_offers_by_sparse_choice(
                        observation.offers,
                        [&](const FixedOffer& offer) {
                            return exact_value(offer.successor);
                        });
                }
                std::sort(
                    fixed_observations.begin(),
                    fixed_observations.end(),
                    [](const FixedObservation& left,
                       const FixedObservation& right) {
                        return left.observation !=
                                       right.observation
                                   ? left.observation <
                                         right.observation
                                   : left.observation_state <
                                         right.observation_state;
                    });
                recipe.option_preferences.reserve(
                    fixed_observations.size());
                for (const FixedObservation& observation :
                     fixed_observations) {
                    ObservedUnveilPreference preference;
                    preference.observation_state =
                        observation.observation_state;
                    preference.choices.reserve(
                        observation.offers.size());
                    for (const FixedOffer& offer :
                         observation.offers) {
                        preference.choices.push_back({
                            offer.mod_id,
                            offer.successor_state,
                            offer.actual_state});
                    }
                    recipe.option_preferences.push_back(
                        std::move(preference));
                }
            }
            std::uint64_t live = choice_retained;
            saturating_add(
                live,
                exact_choice_recipe_bytes(recipe));
            require_local_memory(
                live,
                "Bellman-greedy exact choice recipe");
        }
        SelectedAction selected = make_selected(
            known->second.coarse_state,
            operator_index,
            known->second.strict_state,
            &recipe);
        /*
         * A local override owns only its semantic contract. The shared exact
         * graph computes downstream observation preimages from the resulting
         * policy; retaining the old coarse action's router requirement would
         * mix incompatible policy witnesses.
         */
        selected.routing_observes = {};
        return selected;
    } catch (const AdapterFailure& error) {
        if (error.status ==
            PolicyExactLiftStatus::ResourceCap) {
            throw;
        }
        if (error.status ==
                PolicyExactLiftStatus::MissingPrice ||
            error.status ==
                PolicyExactLiftStatus::ObservationUnavailable ||
            error.status ==
                PolicyExactLiftStatus::UnsupportedPrimitiveKernel) {
            return std::nullopt;
        }
        throw;
    }
}

const ExactActionKernel*
ProductionPolicyOracle::candidate_kernel(
        const StableKey& key,
        const SelectedAction& selected) {
    try {
        return &exact_kernel_ref(
            materialize_exact_state(key, known_.at(key)), selected);
    } catch (const RefinementOracleResourceLimit& error) {
        throw AdapterFailure(
            PolicyExactLiftStatus::ResourceCap,
            error.what(), error.cap_name());
    } catch (const AdapterFailure& error) {
        if (error.status == PolicyExactLiftStatus::ResourceCap) {
            throw;
        }
        return nullptr;
    } catch (const SolverResourceLimit& error) {
        throw AdapterFailure(
            PolicyExactLiftStatus::ResourceCap,
            error.what(),
            adapter_resource_cap_name(error.cap_name()));
    }
}

CandidateExactPartition
ProductionPolicyOracle::partition_candidate_subclasses(
        const RefinedPolicyClass& incumbent_class,
        const std::uint32_t operator_index,
        const std::uint64_t caller_retained_bytes) {
    CandidateExactPartition output;
    if (incumbent_class.terminal ||
        incumbent_class.exact_members.empty()) {
        return output;
    }

    struct ObservationGroup {
        ObservationRequirement requirement;
        FeatureSignature signature;
        StableKey stable_anchor;
    };
    std::vector<ObservationGroup> observation_groups;
    std::vector<ClosedPartitionNode> nodes;
    std::vector<StableKey> members =
        incumbent_class.exact_members;
    std::sort(members.begin(), members.end());
    nodes.reserve(members.size());
    observation_groups.reserve(members.size());

    const auto local_scratch_bytes = [&]() {
        std::uint64_t bytes =
            members.capacity() * sizeof(StableKey) +
            nodes.capacity() * sizeof(ClosedPartitionNode) +
            observation_groups.capacity() *
                sizeof(ObservationGroup);
        for (const StableKey& member : members) {
            saturating_add(bytes, stable_key_bytes(member));
        }
        for (const ClosedPartitionNode& node : nodes) {
            saturating_add(bytes, stable_key_bytes(node.stable_key));
            saturating_add(
                bytes, stable_key_bytes(node.observation_key));
            saturating_add(
                bytes, stable_key_bytes(node.immediate_key));
            saturating_add(
                bytes,
                node.arcs.capacity() *
                    sizeof(ClosedPartitionArc));
            for (const ClosedPartitionArc& arc : node.arcs) {
                saturating_add(bytes, stable_key_bytes(arc.label));
            }
        }
        for (const ObservationGroup& group : observation_groups) {
            saturating_add(
                bytes, requirement_bytes(group.requirement));
            saturating_add(
                bytes, feature_signature_bytes(group.signature));
            saturating_add(
                bytes, stable_key_bytes(group.stable_anchor));
        }
        saturating_add(
            bytes, candidate_exact_partition_bytes(output));
        return bytes;
    };

    for (const StableKey& member : members) {
        saturating_add(
            telemetry_.local_state_action_rows_scheduled, 1);
        std::uint64_t retained = caller_retained_bytes;
        saturating_add(retained, local_scratch_bytes());
        std::optional<SelectedAction> candidate =
            candidate_selection(
                member, operator_index, retained);
        if (!candidate.has_value()) continue;
        if (incumbent_class.selected_action.has_value() &&
            candidate->action_id ==
                incumbent_class.selected_action->action_id &&
            candidate->semantic_key ==
                incumbent_class.selected_action->semantic_key) {
            continue;
        }
        const ExactActionKernel* kernel =
            candidate_kernel(member, *candidate);
        if (kernel == nullptr) continue;
        saturating_add(
            telemetry_.local_state_action_rows_evaluated, 1);

        /*
         * Candidate source equivalence observes the candidate's direct
         * contract and the preimage of every retained policy observation.
         * The latter is deliberately a conservative union used only for
         * scheduling; the final selected-policy pass recomputes the minimum
         * reachable requirement and publishes that router.
         */
        ObservationRequirement requirement =
            merge_observation_requirements(
                observation_requirement_from_selected_action(
                    *candidate),
                preserved_observation_requirement(
                    carrier_requirement_, *candidate));
        requirement = canonical_observation_requirement(
            std::move(requirement));
        const KnownState& candidate_carrier =
            known_.at(member);
        const AbstractFeatureExtraction candidate_extraction =
            extract_strict_abstract_features(
                strict_->session(), strict_->layout(),
                strict_->state(candidate_carrier.strict_state),
                requirement);
        if (!candidate_extraction.complete()) {
            throw AdapterFailure(
                PolicyExactLiftStatus::ObservationUnavailable,
                "candidate exact subclass lost a required "
                "observation");
        }
        const FeatureSignature signature = observe_features(
            candidate_extraction.features,
            requirement);
        auto observation_group = std::find_if(
            observation_groups.begin(),
            observation_groups.end(),
            [&](const ObservationGroup& group) {
                return group.requirement == requirement &&
                       group.signature == signature;
            });
        if (observation_group == observation_groups.end()) {
            observation_groups.push_back({
                std::move(requirement), signature, member});
            observation_group = std::prev(
                observation_groups.end());
        }

        ClosedPartitionNode node;
        node.stable_key = member;
        node.observation_key = {
            0x63616e646f627331ull, /* "candobs1" */
            observation_group->stable_anchor.size()};
        node.observation_key.insert(
            node.observation_key.end(),
            observation_group->stable_anchor.begin(),
            observation_group->stable_anchor.end());
        node.immediate_key = {
            0x63616e64696d6d31ull, /* "candimm1" */
            candidate->action_id,
            candidate->semantic_key.size()};
        node.immediate_key.insert(
            node.immediate_key.end(),
            candidate->semantic_key.begin(),
            candidate->semantic_key.end());
        node.immediate_key.push_back(
            std::bit_cast<std::uint64_t>(kernel->action_cost));
        node.arcs.reserve(kernel->transitions.size());
        for (const ExactTransition& transition :
             kernel->transitions) {
            if (transition.probability <= 0.0) continue;
            node.arcs.push_back({
                transition.successor.stable_key,
                std::nullopt,
                transition.probability});
        }
        output.candidate_by_exact.emplace(
            member, std::move(*candidate));
        nodes.push_back(std::move(node));
        retained = caller_retained_bytes;
        saturating_add(retained, local_scratch_bytes());
        require_local_memory(
            retained,
            "candidate exact-subclass construction");
    }
    if (nodes.empty()) return output;

    /* The shared engine owns deterministic fixed-point partitioning and the
     * exact lumpability proof; this adapter supplies only candidate rows. */
    std::vector<ObservationGroup>{}.swap(observation_groups);
    std::vector<StableKey>{}.swap(members);
    ClosedPartitionLimits partition_limits;
    partition_limits.max_classes =
        limits_.max_refinement_classes;
    if (refinement_rounds_consumed_ >=
        limits_.max_refinement_rounds) {
        throw AdapterFailure(
            PolicyExactLiftStatus::ResourceCap,
            "candidate exact-subclass partition reached "
            "max_refinement_rounds",
            "max_refinement_rounds");
    }
    partition_limits.max_rounds =
        limits_.max_refinement_rounds -
        static_cast<std::uint32_t>(
            refinement_rounds_consumed_);
    partition_limits.retained_estimated_memory_bytes =
        estimated_owned_bytes();
    saturating_add(
        partition_limits.retained_estimated_memory_bytes,
        caller_retained_bytes);
    saturating_add(
        partition_limits.retained_estimated_memory_bytes,
        candidate_exact_partition_bytes(output));
    partition_limits.max_estimated_memory_bytes =
        limits_.max_estimated_memory_bytes;
    partition_limits.probability_sum_tolerance =
        limits_.probability_sum_tolerance;
    ClosedPartitionResult partitioned =
        refine_closed_probabilistic_partition(
            std::move(nodes), partition_limits);
    consume_refinement_rounds(
        partitioned.rounds,
        "candidate exact-subclass fixed point");
    telemetry_.exact_fixed_point_rounds +=
        partitioned.rounds;
    if (partitioned.status !=
        ClosedPartitionStatus::Complete) {
        if (partitioned.status ==
            ClosedPartitionStatus::ResourceCap) {
            const bool memory = partitioned.resource_cap ==
                "max_estimated_memory_bytes";
            throw AdapterFailure(
                PolicyExactLiftStatus::ResourceCap,
                memory
                    ? "candidate exact-subclass partition reached "
                          "max_estimated_memory_bytes"
                    : "candidate exact-subclass partition reached "
                          "max_refinement_classes",
                memory
                    ? "max_estimated_memory_bytes"
                    : "max_refinement_classes");
        }
        if (partitioned.status ==
            ClosedPartitionStatus::RefinementRoundCap) {
            throw AdapterFailure(
                PolicyExactLiftStatus::ResourceCap,
                "candidate exact-subclass partition reached "
                "max_refinement_rounds",
                "max_refinement_rounds");
        }
        throw AdapterFailure(
            PolicyExactLiftStatus::InvalidSolveState,
            partitioned.failure_reason.empty()
                ? "candidate exact-subclass partition failed"
                : partitioned.failure_reason);
    }
    if (!partitioned.lumpable) {
        throw AdapterFailure(
            PolicyExactLiftStatus::InvalidSolveState,
            "candidate exact-subclass partition was not lumpable");
    }
    output.subclasses.reserve(partitioned.classes.size());
    for (ClosedPartitionClass& candidate_class :
         partitioned.classes) {
        output.subclasses.push_back(
            std::move(candidate_class.member_keys));
    }
    std::uint64_t result_retained = caller_retained_bytes;
    saturating_add(
        result_retained,
        candidate_exact_partition_bytes(output));
    require_local_memory(
        result_retained,
        "candidate exact-subclass result");
    return output;
}

bool ProductionPolicyOracle::shared_bellman_prefers_candidate(
        const StableKey& key,
        const SelectedAction& current,
        const ExactActionKernel& current_kernel,
        const SelectedAction& candidate,
        const ExactActionKernel& candidate_kernel_value,
        const std::map<StableKey, double>& incumbent_values,
        const std::map<StableKey, double>& continuation_values,
        const std::uint64_t caller_retained_bytes) {
    std::uint64_t projected = caller_retained_bytes;
    const std::size_t projected_key_count =
        1 + current_kernel.transitions.size() +
        candidate_kernel_value.transitions.size();
    saturating_add(
        projected,
        projected_key_count *
            (2 * sizeof(StableKey) +
             2 * sizeof(solve_detail::SparsePolicyTransitionInput) +
             sizeof(double) + sizeof(std::uint32_t)));
    saturating_add(projected, sizeof(SolveTransitionCache));
    saturating_add(
        projected,
        2 * (sizeof(SparseRow) + sizeof(PricedSparseRow)));
    saturating_add(projected, stable_key_bytes(key));
    for (const ExactTransition& transition :
         current_kernel.transitions) {
        saturating_add(
            projected,
            stable_key_bytes(
                transition.successor.stable_key));
    }
    for (const ExactTransition& transition :
         candidate_kernel_value.transitions) {
        saturating_add(
            projected,
            stable_key_bytes(
                transition.successor.stable_key));
    }
    require_local_memory(
        projected,
        "shared sparse local Bellman construction");

    std::vector<StableKey> local_keys;
    local_keys.reserve(projected_key_count);
    local_keys.push_back(key);
    const auto append_successors =
        [&](const ExactActionKernel& kernel) {
            for (const ExactTransition& transition :
                 kernel.transitions) {
                if (transition.probability > 0.0) {
                    local_keys.push_back(
                        transition.successor.stable_key);
                }
            }
        };
    append_successors(current_kernel);
    append_successors(candidate_kernel_value);
    std::sort(local_keys.begin(), local_keys.end());
    local_keys.erase(
        std::unique(local_keys.begin(), local_keys.end()),
        local_keys.end());
    if (local_keys.size() >
        std::numeric_limits<std::uint32_t>::max()) {
        throw AdapterFailure(
            PolicyExactLiftStatus::ResourceCap,
            "shared sparse local Bellman state ids overflow",
            "max_exact_states");
    }
    const auto local_id =
        [&](const StableKey& exact) {
            const auto found = std::lower_bound(
                local_keys.begin(), local_keys.end(), exact);
            if (found == local_keys.end() || *found != exact) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::InvalidSolveState,
                    "shared sparse local Bellman lost a successor");
            }
            return static_cast<std::uint32_t>(
                found - local_keys.begin());
        };
    const std::uint32_t owner = local_id(key);
    std::vector<double> values(
        local_keys.size(), kInfinity);
    for (std::size_t state = 0;
         state < local_keys.size(); ++state) {
        const StableKey& exact = local_keys[state];
        const auto incumbent = incumbent_values.find(exact);
        const auto continuation =
            continuation_values.find(exact);
        if (incumbent != incumbent_values.end() &&
            continuation != continuation_values.end()) {
            const bool incumbent_finite =
                std::isfinite(incumbent->second);
            const bool continuation_finite =
                std::isfinite(continuation->second);
            double absolute = 0.0;
            double relative = 0.0;
            if (incumbent_finite != continuation_finite ||
                (incumbent_finite &&
                 !reconciled(
                     incumbent->second,
                     continuation->second,
                     absolute, relative))) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::InvalidSolveState,
                    "shared sparse local Bellman received "
                    "incompatible continuation values");
            }
        }
        if (incumbent != incumbent_values.end()) {
            values[state] = incumbent->second;
        } else if (continuation !=
                   continuation_values.end()) {
            values[state] = continuation->second;
        } else {
            const auto carrier = known_.find(exact);
            if (carrier != known_.end() &&
                carrier->second.terminal) {
                values[state] = 0.0;
            } else {
                throw AdapterFailure(
                    PolicyExactLiftStatus::InvalidSolveState,
                    "shared sparse local Bellman successor has "
                    "no current-policy value");
            }
        }
        if (std::isnan(values[state]) ||
            values[state] < 0.0) {
            throw AdapterFailure(
                PolicyExactLiftStatus::InvalidSolveState,
                "shared sparse local Bellman received an "
                "invalid continuation value");
        }
    }

    SolveTransitionCache graph;
    std::vector<PricedSparseRow> priced_rows;
    const auto append_row =
        [&](const SelectedAction& selected,
            const ExactActionKernel& kernel) {
            solve_detail::SparsePolicyRowInput input;
            input.owner_state = owner;
            input.operator_index = selected.action_id;
            input.cost = kernel.action_cost;
            input.transitions.reserve(
                kernel.transitions.size());
            for (const ExactTransition& transition :
                 kernel.transitions) {
                if (transition.probability <= 0.0) continue;
                input.transitions.push_back({
                    local_id(
                        transition.successor.stable_key),
                    transition.probability});
            }
            return solve_detail::append_sparse_policy_row(
                graph, priced_rows, input);
        };
    const std::uint64_t current_row =
        append_row(current, current_kernel);
    const std::uint64_t candidate_row =
        append_row(candidate, candidate_kernel_value);
    std::uint64_t actual = caller_retained_bytes;
    saturating_add(
        actual, graph.fast_estimated_owned_bytes());
    saturating_add(
        actual,
        priced_rows.capacity() * sizeof(PricedSparseRow));
    saturating_add(actual, stable_keys_bytes(local_keys));
    saturating_add(
        actual, values.capacity() * sizeof(double));
    require_local_memory(
        actual,
        "shared sparse local Bellman evaluation");

    std::uint32_t current_work = 0;
    const double current_q =
        solve_detail::evaluate_sparse_policy_row(
            graph, priced_rows, values,
            static_cast<std::size_t>(current_row),
            current_work);
    const auto current_value = incumbent_values.find(key);
    if (current_value == incumbent_values.end()) {
        throw AdapterFailure(
            PolicyExactLiftStatus::InvalidSolveState,
            "shared sparse local Bellman has no incumbent "
            "witness value");
    }
    double absolute = 0.0;
    double relative = 0.0;
    if (!reconciled(
            current_q, current_value->second,
            absolute, relative)) {
        throw AdapterFailure(
            PolicyExactLiftStatus::InvalidSolveState,
            "shared sparse incumbent row disagrees with exact "
            "fixed-policy evaluation");
    }

    const solve_detail::SparsePolicyRowSelection selected =
        solve_detail::select_sparse_policy_row(
            graph, owner, options_.epsilon,
            [](const std::uint64_t) { return true; },
            [&](const std::uint64_t row,
                std::uint32_t& work) {
                return solve_detail::evaluate_sparse_policy_row(
                    graph, priced_rows, values,
                    static_cast<std::size_t>(row), work);
            });
    if (selected.row ==
        std::numeric_limits<std::uint64_t>::max()) {
        throw AdapterFailure(
            PolicyExactLiftStatus::InvalidSolveState,
            "shared sparse local Bellman selected no row");
    }
    return selected.row == candidate_row;
}

std::set<StableKey>
ProductionPolicyOracle::mismatch_witnesses(
        const ExactPolicyRun& run) const {
    std::set<std::uint32_t> affected_parents;
    std::set<std::uint32_t> mismatched_classes;
    for (const StateClassAssignment& assignment :
         run.refinement.assignments) {
        const StableKey& exact_state =
            assignment_exact_state(run.refinement, assignment);
        const auto value =
            run.value_by_exact.find(exact_state);
        if (value == run.value_by_exact.end() ||
            assignment.coarse_state >= solved_.values.size() ||
            !std::isfinite(
                solved_.values[assignment.coarse_state])) {
            continue;
        }
        double absolute = 0.0;
        double relative = 0.0;
        if (!reconciled(
                value->second,
                solved_.values[assignment.coarse_state],
                absolute, relative)) {
            affected_parents.insert(assignment.coarse_state);
            mismatched_classes.insert(assignment.class_id);
        }
    }
    double absolute = 0.0;
    double relative = 0.0;
    if (!reconciled(
            run.root_value(), solved_.evaluated_policy_cost,
            absolute, relative)) {
        affected_parents.insert(
            known_.at(root_key_).coarse_state);
    }

    std::set<StableKey> witnesses;
    std::set<std::uint32_t> covered_classes;
    const auto add_witness =
        [&](const StableKey& key) {
            const auto known = known_.find(key);
            if (known == known_.end() ||
                known->second.terminal ||
                !affected_parents.contains(
                    known->second.coarse_state)) {
                return;
            }
            witnesses.insert(key);
            const auto assignment = std::find_if(
                run.refinement.assignments.begin(),
                run.refinement.assignments.end(),
                [&](const StateClassAssignment& candidate) {
                    return assignment_exact_state(
                               run.refinement, candidate) == key;
                });
            if (assignment !=
                run.refinement.assignments.end()) {
                covered_classes.insert(
                    assignment->class_id);
            }
        };

    /*
     * A retained compatibility counterexample is the narrowest available
     * scheduling authority. Use its exact pair before considering any
     * fallback member of the affected coarse parent.
     */
    for (const RefinementCounterexample& counterexample :
         run.refinement.counterexamples) {
        if (!affected_parents.contains(
                counterexample.coarse_state)) {
            continue;
        }
        add_witness(counterexample.left_state);
        add_witness(counterexample.right_state);
    }

    /*
     * Numeric mismatches without a retained pair (including witness
     * truncation) schedule one deterministic exact member per mismatched
     * class, never every member of the parent.
     */
    for (const StateClassAssignment& assignment :
         run.refinement.assignments) {
        if (mismatched_classes.contains(
                assignment.class_id) &&
            !covered_classes.contains(
                assignment.class_id)) {
            add_witness(assignment_exact_state(
                run.refinement, assignment));
        }
    }

    /*
     * max_witnesses may omit a separating pair. For an affected parent,
     * retain one canonical member of each uncovered non-terminal class so
     * local Bellman comparison still sees every concrete subclass.
     */
    for (const RefinedPolicyClass& policy_class :
         run.refinement.classes) {
        if (policy_class.terminal ||
            !affected_parents.contains(
                policy_class.coarse_state) ||
            covered_classes.contains(policy_class.class_id) ||
            policy_class.exact_members.empty()) {
            continue;
        }
        add_witness(*std::min_element(
            policy_class.exact_members.begin(),
            policy_class.exact_members.end()));
    }
    return witnesses;
}

ReoptimizationSeed
ProductionPolicyOracle::reoptimization_seed(
        const ExactPolicyRun& run) const {
    ReoptimizationSeed seed;
    /*
     * The coarse compatibility audit is a structured refinement witness even
     * when exact lifting happens to reproduce the coarse value.  Seed only
     * that named parent: pass one remains the selected-policy carrier and an
     * unrelated qualified graph (notably Fracture) cannot grow alternative
     * support merely because the solver admitted it elsewhere.
     */
    if (solved_.diagnostics.policy_refinement.triggers != 0) {
        seed.coarse_parents.insert(
            solved_.diagnostics.policy_refinement
                .trigger_coarse_states.begin(),
            solved_.diagnostics.policy_refinement
                .trigger_coarse_states.end());
        if (seed.coarse_parents.empty() &&
            solved_.diagnostics.policy_compatibility_state != kNoId) {
            seed.coarse_parents.insert(
                solved_.diagnostics.policy_compatibility_state);
        }
    }
    if (operator_vocabulary_widening_witness_.has_value()) {
        seed.coarse_parents.insert(
            operator_vocabulary_widening_witness_
                ->coarse_state);
    }
    for (const StableKey& defect :
         run.defect_witnesses) {
        const auto known = known_.find(defect);
        if (known != known_.end() &&
            !known->second.terminal) {
            seed.coarse_parents.insert(
                known->second.coarse_state);
        }
    }
    const std::set<StableKey> mismatches =
        run.complete
            ? mismatch_witnesses(run)
            : std::set<StableKey>{};
    for (const StableKey& mismatch : mismatches) {
        const auto known = known_.find(mismatch);
        if (known != known_.end() &&
            !known->second.terminal) {
            seed.coarse_parents.insert(
                known->second.coarse_state);
        }
    }
    for (const RefinementCounterexample& counterexample :
         run.refinement.counterexamples) {
        if (!seed.coarse_parents.contains(
                counterexample.coarse_state)) {
            continue;
        }
        RefinementFeatureMask features = 0;
        for (const FeatureAtom& atom :
             counterexample.differing_features) {
            features |= refinement_feature(atom.feature);
        }
        seed.causal_features_by_parent[
            counterexample.coarse_state] |= features;
    }
    return seed;
}

bool ProductionPolicyOracle::note_missing_candidate_vocabulary(
        const StableKey& key) {
    const auto known = known_.find(key);
    if (known == known_.end() ||
        known->second.terminal) {
        return false;
    }
    const std::uint32_t parent =
        known->second.coarse_state;
    for (const std::uint32_t coarse_operator :
         coarse_.candidate_operators()) {
        if (!coarse_.is_candidate_operator_admitted_for_state(
                parent, coarse_operator) ||
            strict_operator_by_coarse_.contains(
                coarse_operator)) {
            continue;
        }
        operator_vocabulary_widening_witness_ =
            OperatorVocabularyWideningWitness{
                parent, coarse_operator};
        return true;
    }
    return false;
}

bool ProductionPolicyOracle::repair_invalid_policy_recursive(
        const std::vector<StableKey>& roots,
        ExactPolicyRun& repaired,
        std::set<StableKey>& repairing,
        std::vector<PolicyMutation>& mutations,
        const std::uint32_t depth,
        const std::uint64_t caller_retained_bytes,
        const bool evaluate_before_repair) {
    if (depth >= limits_.max_refinement_rounds) {
        throw AdapterFailure(
            PolicyExactLiftStatus::ResourceCap,
            "local exact policy repair reached "
            "max_refinement_rounds",
            "max_refinement_rounds");
    }
    consume_refinement_rounds(
        1, "local exact policy repair");
    ++telemetry_.local_reoptimization_rounds;
    if (evaluate_before_repair) {
        repaired = ExactPolicyRun{};
        std::uint64_t retained = caller_retained_bytes;
        saturating_add(retained, stable_keys_bytes(roots));
        saturating_add(retained, stable_key_set_bytes(repairing));
        saturating_add(
            retained, policy_mutations_bytes(mutations));
        require_local_memory(
            retained,
            "local exact repair worklist");
        repaired = evaluate_current_policy(roots, retained);
    }
    if (repaired.complete) return true;
    if (repaired.refinement.status ==
        RefinementStatus::ResourceCap) {
        throw AdapterFailure(
            PolicyExactLiftStatus::ResourceCap,
            repaired.refinement.failure_reason,
            repaired.refinement.resource_cap);
    }
    if (repaired.evaluation.status ==
        PolicyEvaluationStatus::ResourceCap) {
        throw AdapterFailure(
            PolicyExactLiftStatus::ResourceCap,
            repaired.evaluation.failure_reason,
            repaired.evaluation.resource_cap);
    }
    std::vector<const StableKey*> defect_keys;
    defect_keys.reserve(repaired.defect_witnesses.size());
    for (const StableKey& defect : repaired.defect_witnesses) {
        const auto witness_carrier = known_.find(defect);
        if (witness_carrier == known_.end() ||
            witness_carrier->second.terminal) {
            throw AdapterFailure(
                PolicyExactLiftStatus::InvalidSolveState,
                "local policy defect has no non-terminal exact carrier");
        }
        defect_keys.push_back(&witness_carrier->first);
        if (!repair_carriers_.contains(witness_carrier->first)) {
            std::uint64_t projected =
                sizeof(StableKey) + 3 * sizeof(void*);
            saturating_add(
                projected,
                stable_key_bytes(witness_carrier->first));
            require_local_memory(
                projected, "local exact repair carrier");
            repair_carriers_.insert(witness_carrier->first);
            adapter_memory_cache_initialized_ = false;
        }
    }
    std::uint64_t transaction_retained =
        caller_retained_bytes;
    saturating_add(
        transaction_retained,
        defect_keys.capacity() * sizeof(const StableKey*));
    require_local_memory(
        transaction_retained,
        "local exact defect worklist");

    for (const StableKey* witness_key : defect_keys) {
        if (witness_key == nullptr ||
            repairing.contains(*witness_key)) {
            continue;
        }
        const StableKey& witness = *witness_key;
        repairing.insert(witness);
        try {
            if (note_missing_candidate_vocabulary(witness)) {
                repairing.erase(witness);
                return false;
            }
            std::optional<SelectedAction> current =
                selected_action(materialize_exact_state(
                    witness, known_.at(witness)));
            const StableKey current_semantic =
                current.has_value()
                    ? current->semantic_key
                    : StableKey{};
            current.reset();
            for (const std::uint32_t operator_index :
                 admitted_operators_) {
                saturating_add(
                    telemetry_.local_state_action_rows_scheduled, 1);
                std::optional<SelectedAction> candidate =
                    candidate_selection(
                        witness, operator_index,
                        transaction_retained);
                if (!candidate.has_value() ||
                    candidate->semantic_key ==
                        current_semantic) {
                    continue;
                }
                saturating_add(
                    telemetry_.local_state_action_rows_evaluated, 1);
                const std::size_t checkpoint =
                    mutations.size();
                try {
                    apply_policy_override(
                        witness, std::move(*candidate),
                        mutations);
                    candidate.reset();
                    std::uint64_t mutation_live =
                        transaction_retained;
                    saturating_add(
                        mutation_live,
                        stable_key_set_bytes(repairing));
                    saturating_add(
                        mutation_live,
                        policy_mutations_bytes(mutations));
                    saturating_add(
                        mutation_live,
                        stable_key_bytes(current_semantic));
                    require_local_memory(
                        mutation_live,
                        "local exact repair transaction");
                    release_completed_evaluation_storage(roots);
                    const bool repaired_candidate =
                        repair_invalid_policy_recursive(
                            roots, repaired, repairing,
                            mutations, depth + 1,
                            transaction_retained);
                    if (repaired_candidate) {
                        repairing.erase(witness);
                        return true;
                    }
                } catch (...) {
                    rollback_policy_mutations(
                        mutations, checkpoint);
                    throw;
                }
                rollback_policy_mutations(
                    mutations, checkpoint);
            }
        } catch (...) {
            repairing.erase(witness);
            throw;
        }
        repairing.erase(witness);
    }
    return false;
}

ExactPolicyRun ProductionPolicyOracle::repair_invalid_policy() {
    repair_carriers_.clear();
    ExactPolicyRun repaired = evaluate_current_policy({root_key_});
    if (repaired.complete) return repaired;
    if (repaired.defect_witnesses.empty()) return repaired;
    std::set<StableKey> repairing;
    std::vector<PolicyMutation> mutations;
    const std::vector<StableKey> roots{root_key_};
    if (repair_invalid_policy_recursive(
            roots, repaired, repairing,
            mutations, 0, 0, false)) {
        saturating_add(
            telemetry_.local_reoptimizations,
            mutations.size());
        saturating_add(
            telemetry_.local_policy_changes,
            mutations.size());
        mutations.clear();
    } else {
        if (!mutations.empty()) {
            rollback_policy_mutations(mutations, 0);
        }
        /*
         * The recursive search returns the last rejected trial. Re-evaluate
         * after every transaction has rolled back so failure evidence and
         * the installed exact policy describe the same incumbent.
         */
        repaired = ExactPolicyRun{};
        repaired = evaluate_current_policy({root_key_});
    }
    return repaired;
}

ExactPolicyRun ProductionPolicyOracle::improve_policy(
        ExactPolicyRun incumbent) {
    if (!incumbent.complete) return incumbent;

    for (;;) {
        try {
        consume_refinement_rounds(
            1, "local exact policy improvement");
        ++telemetry_.local_reoptimization_rounds;
        std::set<std::uint32_t> affected_parents =
            reoptimization_parents_;
        for (const StableKey& mismatch :
             mismatch_witnesses(incumbent)) {
            const auto known = known_.find(mismatch);
            if (known != known_.end() &&
                !known->second.terminal) {
                affected_parents.insert(
                    known->second.coarse_state);
            }
        }
        if (affected_parents.empty()) break;

        std::uint64_t retained_bytes =
            exact_policy_run_bytes(incumbent);
        saturating_add(
            retained_bytes,
            affected_parents.size() *
                (sizeof(std::uint32_t) + 3 * sizeof(void*)));
        require_local_memory(
            retained_bytes,
            "local exact improvement parents");
        bool accepted = false;
        for (const RefinedPolicyClass& incumbent_class :
             incumbent.refinement.classes) {
            if (incumbent_class.terminal ||
                incumbent_class.exact_members.empty() ||
                !affected_parents.contains(
                    incumbent_class.coarse_state)) {
                continue;
            }
            const StableKey& class_witness =
                *std::min_element(
                    incumbent_class.exact_members.begin(),
                    incumbent_class.exact_members.end());
            if (note_missing_candidate_vocabulary(class_witness)) {
                return incumbent;
            }
            for (const std::uint32_t operator_index :
                 admitted_operators_) {
                CandidateExactPartition candidate_partition =
                    partition_candidate_subclasses(
                        incumbent_class, operator_index,
                        retained_bytes);
                if (candidate_partition.subclasses.empty()) {
                    continue;
                }
                std::uint64_t partition_retained = retained_bytes;
                saturating_add(
                    partition_retained,
                    candidate_exact_partition_bytes(
                        candidate_partition));
                for (const std::vector<StableKey>& subclass :
                     candidate_partition.subclasses) {
                    if (subclass.empty()) continue;
                    const StableKey& witness =
                        *std::min_element(
                            subclass.begin(), subclass.end());
                    const auto current_value =
                        incumbent.value_by_exact.find(witness);
                    if (current_value ==
                            incumbent.value_by_exact.end() ||
                        !std::isfinite(current_value->second)) {
                        continue;
                    }
                    std::optional<SelectedAction> current =
                        selected_action(
                            materialize_exact_state(
                                witness, known_.at(witness)));
                    const auto candidate =
                        candidate_partition
                            .candidate_by_exact.find(witness);
                    if (!current.has_value() ||
                        candidate == candidate_partition
                            .candidate_by_exact.end()) {
                        continue;
                    }
                    const ExactActionKernel* current_kernel =
                        candidate_kernel(witness, *current);
                    if (current_kernel == nullptr) {
                        throw AdapterFailure(
                            PolicyExactLiftStatus::InvalidSolveState,
                            "shared sparse Bellman comparison has no "
                            "incumbent exact kernel");
                    }
                    const ExactActionKernel* kernel =
                        candidate_kernel(
                            witness, candidate->second);
                    if (kernel == nullptr) continue;
                    std::vector<StableKey> successors;
                    successors.reserve(
                        kernel->transitions.size());
                    for (const ExactTransition& transition :
                         kernel->transitions) {
                        if (transition.probability > 0.0) {
                            successors.push_back(
                                transition.successor.stable_key);
                        }
                    }
                    std::uint64_t candidate_retained =
                        partition_retained;
                    saturating_add(
                        candidate_retained,
                        selected_action_bytes(*current));
                    ExactPolicyRun continuation =
                        evaluate_current_policy(
                            std::move(successors),
                            candidate_retained);
                    if (continuation.refinement.status ==
                        RefinementStatus::ResourceCap) {
                        throw AdapterFailure(
                            PolicyExactLiftStatus::ResourceCap,
                            continuation.refinement.failure_reason,
                            continuation.refinement.resource_cap);
                    }
                    if (continuation.evaluation.status ==
                        PolicyEvaluationStatus::ResourceCap) {
                        throw AdapterFailure(
                            PolicyExactLiftStatus::ResourceCap,
                            continuation.evaluation.failure_reason,
                            continuation.evaluation.resource_cap);
                    }
                    bool bellman_improvement = false;
                    if (continuation.complete) {
                        std::uint64_t bellman_retained =
                            candidate_retained;
                        saturating_add(
                            bellman_retained,
                            exact_policy_run_bytes(
                                continuation));
                        bellman_improvement =
                            shared_bellman_prefers_candidate(
                                witness, *current,
                                *current_kernel,
                                candidate->second, *kernel,
                                incumbent.value_by_exact,
                                continuation.value_by_exact,
                                bellman_retained);
                    } else if (
                        continuation.defect_witnesses.empty()) {
                        continue;
                    }
                    const bool continuation_needs_repair =
                        !continuation.complete &&
                        !continuation.defect_witnesses.empty();
                    continuation = ExactPolicyRun{};
                    if (!bellman_improvement &&
                        !continuation_needs_repair) {
                        continue;
                    }

                    std::vector<PolicyMutation> mutations;
                    try {
                        for (const StableKey& member : subclass) {
                            const auto selected =
                                candidate_partition
                                    .candidate_by_exact.find(member);
                            if (selected == candidate_partition
                                    .candidate_by_exact.end()) {
                                throw AdapterFailure(
                                    PolicyExactLiftStatus::
                                        InvalidSolveState,
                                    "candidate exact subclass lost a "
                                    "member decision");
                            }
                            apply_policy_override(
                                member, selected->second,
                                mutations);
                        }
                        std::set<StableKey> repairing;
                        ExactPolicyRun trial;
                        const std::vector<StableKey> roots{
                            root_key_};
                        std::uint64_t trial_retained =
                            partition_retained;
                        saturating_add(
                            trial_retained,
                            selected_action_bytes(*current));
                        const bool proper =
                            repair_invalid_policy_recursive(
                                roots, trial, repairing,
                                mutations, 0,
                                trial_retained);
                        if (proper) {
                            const double tolerance =
                                kAbsoluteCostTolerance +
                                kRelativeCostTolerance *
                                    std::max(
                                        std::abs(
                                            incumbent.root_value()),
                                        std::abs(
                                            trial.root_value()));
                            if (trial.root_value() <
                                incumbent.root_value() -
                                    tolerance) {
                                saturating_add(
                                    telemetry_.local_reoptimizations,
                                    mutations.size());
                                saturating_add(
                                    telemetry_.local_policy_changes,
                                    mutations.size());
                                saturating_add(
                                    telemetry_.local_value_changes,
                                    changed_exact_value_count(
                                        incumbent.value_by_exact,
                                        trial.value_by_exact));
                                mutations.clear();
                                incumbent = std::move(trial);
                                accepted = true;
                            }
                        }
                    } catch (...) {
                        rollback_policy_mutations(
                            mutations, 0);
                        throw;
                    }
                    rollback_policy_mutations(
                        mutations, 0);
                    if (accepted) break;
                }
                if (accepted) break;
            }
            if (accepted) break;
        }
        if (!accepted) break;
        } catch (const AdapterFailure& error) {
            if (error.status ==
                PolicyExactLiftStatus::ResourceCap) {
                /*
                 * Candidate search is optional once a complete, proper exact
                 * incumbent exists. A declared cap ends further improvement
                 * but cannot invalidate that already evaluated executable
                 * policy; the publication layer exposes it as bounded.
                 */
                return incumbent;
            }
            throw;
        }
    }
    return incumbent;
}

void merge_adapter_telemetry(
        PolicyLiftAdapterTelemetry& total,
        const PolicyLiftAdapterTelemetry& pass) {
    const auto add_u32 =
        [](std::uint32_t& target,
           const std::uint32_t addition) {
            target = bounded_u32(
                static_cast<std::uint64_t>(target) +
                addition);
        };
    add_u32(
        total.coarse_policy_states,
        pass.coarse_policy_states);
    saturating_add(
        total.coarse_policy_edges,
        pass.coarse_policy_edges);
    add_u32(
        total.strict_states_discovered,
        pass.strict_states_discovered);
    add_u32(
        total.strict_carriers_materialized,
        pass.strict_carriers_materialized);
    add_u32(
        total.strict_kernels_built,
        pass.strict_kernels_built);
    saturating_add(
        total.strict_transitions_built,
        pass.strict_transitions_built);
    saturating_add(
        total.canonical_successor_collapses,
        pass.canonical_successor_collapses);
    saturating_add(
        total.strict_kernel_cache_hits,
        pass.strict_kernel_cache_hits);
    add_u32(
        total.backward_observation_rounds,
        pass.backward_observation_rounds);
    saturating_add(
        total.exact_fixed_point_rounds,
        pass.exact_fixed_point_rounds);
    total.strict_calc_owned_bytes = std::max(
        total.strict_calc_owned_bytes,
        pass.strict_calc_owned_bytes);
    total.adapter_owned_bytes = std::max(
        total.adapter_owned_bytes,
        pass.adapter_owned_bytes);
    total.peak_adapter_owned_bytes = std::max(
        total.peak_adapter_owned_bytes,
        pass.peak_adapter_owned_bytes);
    saturating_add(
        total.strict_reforge_work,
        pass.strict_reforge_work);
    add_u32(
        total.local_reoptimization_rounds,
        pass.local_reoptimization_rounds);
    saturating_add(
        total.local_state_action_rows_scheduled,
        pass.local_state_action_rows_scheduled);
    saturating_add(
        total.local_state_action_rows_evaluated,
        pass.local_state_action_rows_evaluated);
    saturating_add(
        total.local_reoptimizations,
        pass.local_reoptimizations);
    saturating_add(
        total.local_policy_changes,
        pass.local_policy_changes);
    saturating_add(
        total.local_value_changes,
        pass.local_value_changes);
}

RefinementLimits default_limits(
        CalcContext& coarse,
        const SolveResult& solved,
        const SolveOptions& options) {
    RefinementLimits limits;
    limits.max_coarse_states =
        std::max<std::uint32_t>(1, options.max_discovered_states);
    limits.max_exact_states =
        std::max<std::uint32_t>(1, options.max_discovered_states);
    limits.max_exact_kernels = std::max<std::uint32_t>(
        1, bounded_u32(options.max_state_action_rows));
    limits.max_transitions =
        std::max<std::uint64_t>(1, options.max_transitions);
    limits.max_refinement_classes = limits.max_exact_states;
    limits.max_refinement_rounds =
        std::max<std::uint32_t>(1, options.max_sweeps);
    const std::uint64_t retained =
        estimated_retained_solver_bytes(coarse, &solved);
    limits.max_estimated_memory_bytes =
        retained >= options.max_solver_owned_bytes
            ? 1
            : options.max_solver_owned_bytes - retained;
    limits.max_witnesses =
        std::max<std::uint32_t>(1, options.max_diagnostic_samples);
    return limits;
}

} // namespace

CompiledPolicyAssertion assert_compiled_policy_exact(
        CalcContext& coarse,
        const SolveResult& solved,
        const std::unordered_map<std::string, double>& prices,
        const SolveOptions& options,
        const std::string& strategy_name,
        const RefinedPolicyCompileRouting* refined_routing) {
    CompiledPolicyAssertion result;
    result.solver_cost = solved.evaluated_policy_cost;
    if (!solved.policy_available) {
        result.status = CompiledPolicyAssertionStatus::NoPolicy;
        result.failure_reason = "solve did not publish a policy";
        return result;
    }
    result.retained_solver_bytes =
        estimated_retained_solver_bytes(coarse, &solved);
    result.publication_peak_owned_bytes =
        result.retained_solver_bytes;
    if (result.retained_solver_bytes >=
        options.max_solver_owned_bytes) {
        result.status = CompiledPolicyAssertionStatus::ResourceCap;
        result.resource_cap = "max_solver_owned_bytes";
        result.failure_reason =
            "retained solver state leaves no memory for exact policy "
            "compilation";
        return result;
    }
    const std::uint64_t compilation_memory =
        options.max_solver_owned_bytes -
        result.retained_solver_bytes;
    if (compilation_memory <= 1) {
        result.status = CompiledPolicyAssertionStatus::ResourceCap;
        result.resource_cap = "max_solver_owned_bytes";
        result.failure_reason =
            "retained solver state leaves no memory for exact policy "
            "JSON";
        return result;
    }
    const std::uint64_t compilation_json_limit =
        std::min<std::uint64_t>(
            options.max_strategy_json_bytes,
            compilation_memory - 1);
    const bool json_limited_by_memory =
        compilation_json_limit <
        options.max_strategy_json_bytes;
    try {
        result.strategy_json = compile_policy_strategy_json(
            coarse, solved, strategy_name, &result.compilation,
            compilation_json_limit, refined_routing,
            compilation_memory);
        {
            std::uint64_t live =
                result.retained_solver_bytes;
            saturating_add(
                live,
                std::max<std::uint64_t>(
                    result.compilation.peak_owned_bytes,
                    result.strategy_json.capacity() + 1));
            result.publication_peak_owned_bytes =
                std::max(
                    result.publication_peak_owned_bytes,
                    live);
        }
        if (!result.compilation.cap_hit.empty()) {
            result.status =
                CompiledPolicyAssertionStatus::ResourceCap;
            result.resource_cap =
                json_limited_by_memory &&
                        result.compilation.cap_hit ==
                            "max_strategy_json_bytes"
                    ? "max_solver_owned_bytes"
                    : result.compilation.cap_hit;
            result.failure_reason =
                "compiled policy reached " +
                result.resource_cap;
            return result;
        }
    } catch (const SolverResourceLimit& error) {
        std::uint64_t live =
            result.retained_solver_bytes;
        saturating_add(
            live, result.compilation.peak_owned_bytes);
        result.publication_peak_owned_bytes =
            std::max(
                result.publication_peak_owned_bytes, live);
        result.status = CompiledPolicyAssertionStatus::ResourceCap;
        result.resource_cap = error.cap_name();
        result.failure_reason = error.what();
        return result;
    } catch (const std::length_error& error) {
        result.status = CompiledPolicyAssertionStatus::ResourceCap;
        result.resource_cap =
            resource_cap_from_message(error.what());
        result.failure_reason = error.what();
        return result;
    } catch (const std::exception& error) {
        result.status = result.compilation.cap_hit.empty()
                            ? CompiledPolicyAssertionStatus::
                                  CompilationFailure
                            : CompiledPolicyAssertionStatus::ResourceCap;
        result.resource_cap =
            json_limited_by_memory &&
                    result.compilation.cap_hit ==
                        "max_strategy_json_bytes"
                ? "max_solver_owned_bytes"
                : result.compilation.cap_hit;
        result.failure_reason = error.what();
        return result;
    }

    try {
        const std::uint64_t strategy_json_bytes =
            result.strategy_json.capacity() + 1;
        std::uint64_t remaining_memory =
            options.max_solver_owned_bytes -
            result.retained_solver_bytes;
        const auto consume_memory =
            [&](const std::uint64_t bytes,
                const char* subject) {
                if (bytes >= remaining_memory) {
                    result.status =
                        CompiledPolicyAssertionStatus::ResourceCap;
                    result.resource_cap =
                        "max_solver_owned_bytes";
                    result.failure_reason =
                        std::string{"no remaining solver memory for "} +
                        subject;
                    return false;
                }
                remaining_memory -= bytes;
                return true;
            };
        if (!consume_memory(
                strategy_json_bytes,
                "parsed exact policy evaluation")) {
            return result;
        }
        const std::shared_ptr<const SessionImpl> session =
            borrow_session(coarse);
        const std::shared_ptr<StrategyImpl> strategy =
            compile_strategy_json(
                session,
                result.strategy_json.data(),
                result.strategy_json.size());
        result.parsed_strategy_bytes =
            strategy_impl_owned_bytes(*strategy);
        {
            std::uint64_t live =
                result.retained_solver_bytes;
            saturating_add(live, strategy_json_bytes);
            saturating_add(
                live, result.parsed_strategy_bytes);
            result.publication_peak_owned_bytes =
                std::max(
                    result.publication_peak_owned_bytes,
                    live);
        }
        if (!consume_memory(
                result.parsed_strategy_bytes,
                "the parsed exact strategy")) {
            return result;
        }
        auto economy = std::make_shared<EconomyImpl>();
        economy->id = "policy-guided-exact-refinement";
        economy->prices = prices;
        result.economy_bytes = economy_owned_bytes(
            economy->prices, economy->id.capacity());
        {
            std::uint64_t live =
                result.retained_solver_bytes;
            saturating_add(live, strategy_json_bytes);
            saturating_add(
                live, result.parsed_strategy_bytes);
            saturating_add(live, result.economy_bytes);
            result.publication_peak_owned_bytes =
                std::max(
                    result.publication_peak_owned_bytes,
                    live);
        }
        if (!consume_memory(
                result.economy_bytes,
                "the exact-evaluation economy")) {
            return result;
        }

        result.evaluator_memory_budget = remaining_memory;
        StrategyEvalOptions evaluation_options;
        evaluation_options.epsilon = 1e-12;
        evaluation_options.max_sweeps =
            std::max<std::uint32_t>(1, options.max_sweeps);
        evaluation_options.max_states =
            std::max<std::uint32_t>(
                1, options.max_discovered_states);
        evaluation_options.max_pairs = std::max<std::uint32_t>(
            1, bounded_u32(options.max_state_action_rows));
        evaluation_options.max_transitions =
            std::max<std::uint32_t>(
                1, bounded_u32(options.max_transitions));
        evaluation_options.max_owned_bytes =
            result.evaluator_memory_budget;
        evaluation_options.max_output_json_bytes =
            options.max_strategy_json_bytes;
        evaluation_options.max_reforge_work =
            options.max_reforge_work;
        evaluation_options.economy = std::move(economy);
        result.evaluation =
            evaluate_strategy(*strategy, evaluation_options);
        {
            std::uint64_t live =
                result.retained_solver_bytes;
            saturating_add(live, strategy_json_bytes);
            saturating_add(
                live, result.parsed_strategy_bytes);
            saturating_add(live, result.economy_bytes);
            saturating_add(
                live,
                result.evaluation
                    .peak_owned_bytes_estimate);
            result.publication_peak_owned_bytes =
                std::max(
                    result.publication_peak_owned_bytes,
                    live);
        }
    } catch (const SolverResourceLimit& error) {
        result.status = CompiledPolicyAssertionStatus::ResourceCap;
        result.resource_cap = error.cap_name();
        result.failure_reason = error.what();
        return result;
    } catch (const std::length_error& error) {
        result.status = CompiledPolicyAssertionStatus::ResourceCap;
        result.resource_cap =
            resource_cap_from_message(error.what());
        result.failure_reason = error.what();
        return result;
    } catch (const std::exception& error) {
        result.status =
            CompiledPolicyAssertionStatus::ExactEvaluationFailure;
        result.failure_reason = error.what();
        return result;
    }

    result.off_policy_probability =
        result.evaluation.failure_probability +
        result.evaluation.stop_probability +
        result.evaluation.action_not_applied_probability +
        result.evaluation.no_matching_edge_probability +
        result.evaluation.unresolved_probability;
    result.zero_off_policy =
        result.off_policy_probability <= kOffPolicyTolerance &&
        result.evaluation.success_probability >=
            1.0 - kOffPolicyTolerance;
    result.proper =
        result.evaluation.converged && result.zero_off_policy;
    result.exact_cost = result.evaluation.total_expected_cost;
    result.cost_reconciled =
        result.evaluation.cost_complete &&
        std::isfinite(result.solver_cost) &&
        std::isfinite(result.exact_cost) &&
        reconciled(
            result.exact_cost,
            result.solver_cost,
            result.absolute_cost_delta,
            result.relative_cost_delta);

    if (!result.proper) {
        result.status =
            CompiledPolicyAssertionStatus::ImproperPolicy;
        result.failure_reason =
            "compiled policy is not a proper absorbing success policy";
    } else if (!result.evaluation.cost_complete) {
        result.status =
            CompiledPolicyAssertionStatus::IncompleteCost;
        result.failure_reason =
            "compiled policy exact evaluation has incomplete prices";
    } else if (!result.cost_reconciled) {
        result.status =
            CompiledPolicyAssertionStatus::CostMismatch;
        result.failure_reason =
            "compiled policy exact cost does not reconcile with the "
            "solver value";
    } else {
        result.status = CompiledPolicyAssertionStatus::Complete;
        result.executable = true;
    }
    return result;
}

const char* policy_exact_lift_status_name(
        const PolicyExactLiftStatus status) {
    switch (status) {
    case PolicyExactLiftStatus::Complete:
        return "complete";
    case PolicyExactLiftStatus::NoPolicy:
        return "no_policy";
    case PolicyExactLiftStatus::InvalidSolveState:
        return "invalid_solve_state";
    case PolicyExactLiftStatus::MissingPrice:
        return "missing_price";
    case PolicyExactLiftStatus::UnsupportedPrimitiveKernel:
        return "unsupported_primitive_kernel";
    case PolicyExactLiftStatus::CoarseMappingFailure:
        return "coarse_mapping_failure";
    case PolicyExactLiftStatus::ObservationUnavailable:
        return "observation_unavailable";
    case PolicyExactLiftStatus::ResourceCap:
        return "resource_cap";
    case PolicyExactLiftStatus::RefinementFailure:
        return "refinement_failure";
    case PolicyExactLiftStatus::LocalReoptimizationRequired:
        return "local_reoptimization_required";
    case PolicyExactLiftStatus::CompiledAssertionFailure:
        return "compiled_assertion_failure";
    }
    return "invalid_solve_state";
}

PolicyExactLiftCertificate lift_policy_exact(
        CalcContext& coarse,
        const SolveResult& solved,
        const pc_item_state& exact_start,
        const std::unordered_map<std::string, double>& prices,
        const SolveOptions& options,
        const std::string& strategy_name,
        const RefinementLimits* limits_override) {
    PolicyExactLiftCertificate certificate;
    certificate.solver_cost = solved.evaluated_policy_cost;
    RefinementLimits limits =
        limits_override == nullptr
            ? default_limits(coarse, solved, options)
            : *limits_override;
    PolicyLiftAdapterTelemetry lift_telemetry;
    PolicyLiftAdapterTelemetry reoptimization_telemetry;
    const auto capture_adapter_telemetry = [&] {
        PolicyLiftAdapterTelemetry total;
        merge_adapter_telemetry(total, lift_telemetry);
        merge_adapter_telemetry(
            total, reoptimization_telemetry);
        certificate.adapter = std::move(total);
    };
    try {
        const auto check_initial_memory =
            [&](ProductionPolicyOracle& oracle,
                const RefinementLimits& pass_limits) {
                if (oracle.estimated_owned_bytes() >=
                    pass_limits.max_estimated_memory_bytes) {
                    throw AdapterFailure(
                        PolicyExactLiftStatus::ResourceCap,
                        "strict policy discovery reached "
                        "max_estimated_memory_bytes",
                        "max_estimated_memory_bytes");
                }
            };
        const auto finalize =
            [&](ProductionPolicyOracle& oracle,
                ExactPolicyRun policy) {
                const bool policy_complete =
                    policy.complete;
                certificate.exact_root_key =
                    oracle.root_key();
                certificate.refinement =
                    std::move(policy.refinement);
                certificate.class_evaluation =
                    std::move(policy.evaluation);
                std::vector<StableKey>{}.swap(policy.roots);
                policy.value_by_exact.clear();
                certificate.policy_changed =
                    oracle.final_policy_changed(
                        certificate.refinement);
                if (!policy_complete ||
                    certificate.class_evaluation
                            .start_values.size() != 1) {
                    return;
                }
                for (const StateClassAssignment& assignment :
                     certificate.refinement.assignments) {
                    if (assignment_exact_state(
                            certificate.refinement, assignment) ==
                        certificate.exact_root_key) {
                        certificate.root_refinement_class =
                            assignment.class_id;
                        break;
                    }
                }
                if (certificate.root_refinement_class ==
                    kNoId) {
                    throw AdapterFailure(
                        PolicyExactLiftStatus::
                            RefinementFailure,
                        "refinement result omitted the exact root "
                        "assignment");
                }
                certificate.exact_start_cost =
                    certificate.class_evaluation
                        .start_values.front();
                certificate.coarse_value_reconciled =
                    reconciled(
                        certificate.exact_start_cost,
                        certificate.solver_cost,
                        certificate.absolute_cost_delta,
                        certificate.relative_cost_delta);
                certificate.compiled =
                    oracle.assert_lifted_policy(
                        strategy_name,
                        certificate.refinement,
                        certificate.class_evaluation);
            };

        ReoptimizationSeed reoptimization_seed;
        if (solved.diagnostics.policy_refinement.triggers != 0) {
            reoptimization_seed.coarse_parents.insert(
                solved.diagnostics.policy_refinement
                    .trigger_coarse_states.begin(),
                solved.diagnostics.policy_refinement
                    .trigger_coarse_states.end());
            if (reoptimization_seed.coarse_parents.empty() &&
                solved.diagnostics.policy_compatibility_state !=
                    kNoId) {
                reoptimization_seed.coarse_parents.insert(
                    solved.diagnostics.policy_compatibility_state);
            }
        }
        ExactPolicyRun lift_policy;
        if (reoptimization_seed.empty()) {
            /*
             * Pass one is the minimum selected-policy carrier. It cannot
             * grow support for an unused alternative merely because that
             * alternative was admitted by the broad solver.
             */
            ProductionPolicyOracle oracle(
                coarse, solved, exact_start, prices,
                options, limits, lift_telemetry, nullptr);
            check_initial_memory(oracle, limits);
            lift_policy = oracle.evaluate_fixed_policy();
            reoptimization_seed =
                oracle.reoptimization_seed(lift_policy);
            const std::uint64_t spent_reforge = std::min(
                lift_telemetry.strict_reforge_work,
                options.max_reforge_work);
            const std::uint64_t remaining_reforge =
                options.max_reforge_work - spent_reforge;
            const bool optional_rebuild_unbudgeted =
                lift_policy.complete &&
                !reoptimization_seed.empty() &&
                spent_reforge > remaining_reforge;
            if (reoptimization_seed.empty() ||
                optional_rebuild_unbudgeted) {
                /*
                 * A complete fixed policy is already a valid bounded
                 * incumbent. Do not spend a smaller residual budget on a
                 * second adapter whose only purpose is optional improvement;
                 * retain the two-pass path whenever at least half of the
                 * declared reforge work remains.
                 */
                finalize(
                    oracle, std::move(lift_policy));
                reoptimization_seed = {};
            }
        }
        while (!reoptimization_seed.empty() &&
               certificate.compiled.status ==
                   CompiledPolicyAssertionStatus::NotRun) {
            /*
             * The first adapter is gone before widening the strict child.
             * Charge completed kernel/transition/reforge/round work to every
             * witness-driven rebuild so no declared cap can reset.
             */
            RefinementLimits reoptimization_limits =
                limits;
            const std::uint64_t spent_kernels =
                static_cast<std::uint64_t>(
                    lift_telemetry.strict_kernels_built) +
                reoptimization_telemetry.strict_kernels_built;
            if (spent_kernels >=
                reoptimization_limits.max_exact_kernels) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::ResourceCap,
                    "witness-driven local exact refinement reached "
                    "max_exact_kernels",
                    "max_exact_kernels");
            }
            reoptimization_limits.max_exact_kernels -=
                static_cast<std::uint32_t>(spent_kernels);
            std::uint64_t spent_transitions =
                lift_telemetry.strict_transitions_built;
            saturating_add(
                spent_transitions,
                reoptimization_telemetry.strict_transitions_built);
            if (spent_transitions >=
                reoptimization_limits.max_transitions) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::ResourceCap,
                    "witness-driven local exact refinement reached "
                    "max_transitions",
                    "max_transitions");
            }
            reoptimization_limits.max_transitions -=
                spent_transitions;
            std::uint64_t spent_rounds =
                lift_telemetry.backward_observation_rounds;
            saturating_add(
                spent_rounds,
                lift_telemetry.exact_fixed_point_rounds);
            saturating_add(
                spent_rounds,
                lift_telemetry.local_reoptimization_rounds);
            saturating_add(
                spent_rounds,
                reoptimization_telemetry
                    .backward_observation_rounds);
            saturating_add(
                spent_rounds,
                reoptimization_telemetry
                    .exact_fixed_point_rounds);
            saturating_add(
                spent_rounds,
                reoptimization_telemetry
                    .local_reoptimization_rounds);
            if (spent_rounds >=
                reoptimization_limits
                    .max_refinement_rounds) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::ResourceCap,
                    "witness-driven local exact refinement reached "
                    "max_refinement_rounds",
                    "max_refinement_rounds");
            }
            reoptimization_limits.max_refinement_rounds -=
                static_cast<std::uint32_t>(spent_rounds);
            SolveOptions reoptimization_options = options;
            std::uint64_t spent_reforge =
                lift_telemetry.strict_reforge_work;
            saturating_add(
                spent_reforge,
                reoptimization_telemetry.strict_reforge_work);
            if (spent_reforge >=
                reoptimization_options.max_reforge_work) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::ResourceCap,
                    "witness-driven local exact refinement reached "
                    "max_reforge_work",
                    "max_reforge_work");
            }
            reoptimization_options.max_reforge_work -=
                spent_reforge;
            lift_policy = ExactPolicyRun{};

            PolicyLiftAdapterTelemetry pass_telemetry;
            ReoptimizationSeed discovered_seed;
            bool rebuild = false;
            try {
                ProductionPolicyOracle oracle(
                    coarse, solved, exact_start, prices,
                    reoptimization_options,
                    reoptimization_limits,
                    pass_telemetry,
                    &reoptimization_seed);
                check_initial_memory(
                    oracle, reoptimization_limits);
                ExactPolicyRun policy =
                    oracle.repair_invalid_policy();
                if (policy.complete) {
                    policy = oracle.improve_policy(
                        std::move(policy));
                }
                discovered_seed =
                    oracle.reoptimization_seed(policy);
                rebuild =
                    oracle.requires_operator_vocabulary_widening(
                        policy);
                if (!rebuild) {
                    finalize(oracle, std::move(policy));
                    reoptimization_seed = {};
                }
            } catch (...) {
                merge_adapter_telemetry(
                    reoptimization_telemetry,
                    pass_telemetry);
                throw;
            }
            merge_adapter_telemetry(
                reoptimization_telemetry,
                pass_telemetry);
            if (rebuild) {
                const std::size_t before =
                    reoptimization_seed.coarse_parents.size();
                reoptimization_seed.merge(discovered_seed);
                if (reoptimization_seed.coarse_parents.size() ==
                    before) {
                    throw AdapterFailure(
                        PolicyExactLiftStatus::
                            LocalReoptimizationRequired,
                        "local exact refinement requested the same "
                        "operator vocabulary twice without progress");
                }
            }
        }
        capture_adapter_telemetry();
    } catch (const AdapterFailure& error) {
        capture_adapter_telemetry();
        certificate.status = error.status;
        certificate.failure_reason = error.what();
        certificate.resource_cap = error.cap;
        return certificate;
    } catch (const SolverResourceLimit& error) {
        capture_adapter_telemetry();
        certificate.status = PolicyExactLiftStatus::ResourceCap;
        certificate.failure_reason = error.what();
        certificate.resource_cap =
            adapter_resource_cap_name(error.cap_name());
        return certificate;
    } catch (const std::length_error& error) {
        capture_adapter_telemetry();
        certificate.status = PolicyExactLiftStatus::ResourceCap;
        certificate.failure_reason = error.what();
        certificate.resource_cap =
            resource_cap_from_message(error.what());
        return certificate;
    } catch (const std::exception& error) {
        capture_adapter_telemetry();
        certificate.status =
            PolicyExactLiftStatus::RefinementFailure;
        certificate.failure_reason = error.what();
        return certificate;
    }

    if (certificate.refinement.status !=
            RefinementStatus::Complete ||
        !certificate.refinement.executable ||
        !certificate.refinement.lumpable) {
        certificate.status =
            certificate.refinement.status ==
                    RefinementStatus::ResourceCap ||
                    certificate.refinement.status ==
                        RefinementStatus::RefinementRoundCap
                ? PolicyExactLiftStatus::ResourceCap
                : PolicyExactLiftStatus::RefinementFailure;
        certificate.failure_reason =
            certificate.refinement.failure_reason;
        certificate.resource_cap =
            certificate.refinement.resource_cap;
        return certificate;
    }
    certificate.lumpable = true;
    if (certificate.class_evaluation.status !=
            PolicyEvaluationStatus::Complete ||
        !certificate.class_evaluation.converged ||
        !certificate.class_evaluation.proper ||
        certificate.class_evaluation.start_values.size() != 1) {
        certificate.status =
            certificate.class_evaluation.status ==
                    PolicyEvaluationStatus::ResourceCap
                ? PolicyExactLiftStatus::ResourceCap
                : PolicyExactLiftStatus::RefinementFailure;
        certificate.failure_reason =
            certificate.class_evaluation.failure_reason.empty()
                ? "refined class policy did not produce one proper "
                  "root value"
                : certificate.class_evaluation.failure_reason;
        certificate.resource_cap =
            certificate.class_evaluation.resource_cap;
        return certificate;
    }

    if (certificate.compiled.status ==
        CompiledPolicyAssertionStatus::Complete) {
        double artifact_delta = 0.0;
        double artifact_relative = 0.0;
        if (!reconciled(
                certificate.compiled.exact_cost,
                certificate.exact_start_cost,
                artifact_delta,
                artifact_relative)) {
            certificate.status =
                PolicyExactLiftStatus::CompiledAssertionFailure;
            certificate.failure_reason =
                "compiled lifted artifact does not reconcile with its "
                "refined class-policy value";
            return certificate;
        }
        certificate.status = PolicyExactLiftStatus::Complete;
        certificate.executable = true;
        return certificate;
    }

    certificate.failure_reason =
        certificate.compiled.failure_reason;
    certificate.resource_cap =
        certificate.compiled.resource_cap;
    if (certificate.compiled.status ==
               CompiledPolicyAssertionStatus::ResourceCap) {
        certificate.status = PolicyExactLiftStatus::ResourceCap;
    } else {
        certificate.status =
            PolicyExactLiftStatus::CompiledAssertionFailure;
    }
    return certificate;
}

} // namespace refinement
} // namespace solver
} // namespace poecraft
