#include "solver_calc_types.hpp"

#include "poecraft/bitset.h"

#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <functional>
#include <iterator>
#include <map>
#include <numeric>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace poecraft {
namespace solver {

namespace {

void append_semantic_string(
    std::vector<std::uint64_t>& key,
    const std::string& value) {
    key.push_back(static_cast<std::uint64_t>(value.size()));
    for (std::size_t offset = 0; offset < value.size(); offset += 8) {
        std::uint64_t word = 0;
        const std::size_t count =
            std::min<std::size_t>(8, value.size() - offset);
        for (std::size_t byte = 0; byte < count; ++byte) {
            word |= static_cast<std::uint64_t>(
                        static_cast<unsigned char>(value[offset + byte]))
                    << (byte * 8);
        }
        key.push_back(word);
    }
}

void append_semantic_strings(
    std::vector<std::uint64_t>& key,
    const std::vector<std::string>& values) {
    key.push_back(static_cast<std::uint64_t>(values.size()));
    for (const std::string& value : values) {
        append_semantic_string(key, value);
    }
}

void append_semantic_u32s(
    std::vector<std::uint64_t>& key,
    const std::vector<std::uint32_t>& values) {
    key.push_back(static_cast<std::uint64_t>(values.size()));
    for (const std::uint32_t value : values) key.push_back(value);
}

bool same_resource_quantities(
    const std::vector<std::pair<std::string, double>>& left,
    const std::vector<std::pair<std::string, double>>& right) {
    if (left.size() != right.size()) return false;
    for (std::size_t i = 0; i < left.size(); ++i) {
        if (left[i].first != right[i].first ||
            std::bit_cast<std::uint64_t>(left[i].second) !=
                std::bit_cast<std::uint64_t>(right[i].second)) {
            return false;
        }
    }
    return true;
}

} // namespace

std::vector<std::uint64_t> exact_abstract_state_key(
        const AbstractState& state,
        const std::uint32_t coarse_parent) {
    std::vector<std::uint64_t> key{
        0x7063727374617432ull, /* "pcrstat2" */
        coarse_parent,
        state.fractured_goal_mask,
        state.crafted_goal_mask,
        state.blocked_mask,
        state.prefix_count,
        state.suffix_count,
        state.rarity,
        state.influence_bits,
        static_cast<std::uint64_t>(state.veiled_side + 1),
        state.searing_exarch_tier,
        state.eater_of_worlds_tier,
        state.flags,
        state.fractured_metamod_flags,
        state.goal_progress_retry_basin,
    };
    for (const std::uint8_t status : state.slot_status) {
        key.push_back(status);
    }
    for (const std::uint32_t token :
         state.goal_member_class_tokens) {
        key.push_back(token);
    }
    const auto append_counts =
        [&](const CompactCountVector& counts) {
            key.push_back(counts.size());
            std::uint64_t nonzero = 0;
            for (std::size_t index = 0;
                 index < counts.size(); ++index) {
                if (counts[index] != 0) ++nonzero;
            }
            key.push_back(nonzero);
            for (std::size_t index = 0;
                 index < counts.size(); ++index) {
                const std::uint8_t value = counts[index];
                if (value == 0) continue;
                key.push_back(index);
                key.push_back(value);
            }
        };
    append_counts(state.junk_counts);
    append_counts(state.fractured_junk_counts);
    append_counts(state.crafted_junk_counts);
    append_counts(state.fractured_crafted_junk_counts);
    return key;
}

std::vector<std::uint64_t> planner_operator_semantic_key(
    const PlannerOperator& planner) {
    std::vector<std::uint64_t> key;
    key.reserve(
        32 + planner.primitive_program_action_ids.size() +
        planner.exit_goal_slots.size() +
        planner.resource_quantities.size() * 3);
    /* Versioned, length-delimited logical serialization. No hash is used:
     * equality of these vectors is collision-free semantic equality. */
    key.push_back(1);
    key.push_back(static_cast<std::uint8_t>(planner.kind));
    key.push_back(static_cast<std::uint8_t>(planner.option_kind));
    append_semantic_string(key, planner.primitive_action_id);
    append_semantic_strings(
        key, planner.primitive_program_action_ids);
    key.push_back(
        static_cast<std::uint8_t>(planner.intended_side));
    append_semantic_u32s(key, planner.exit_goal_slots);
    key.push_back(planner.exit_min_satisfied);
    key.push_back(planner.carrier_goal_slot);
    append_semantic_string(key, planner.conditional_action_id);
    append_semantic_string(key, planner.bestiary_create_action_id);
    append_semantic_string(key, planner.bestiary_restore_action_id);
    key.push_back(static_cast<std::uint8_t>(planner.automatic_kind));
    key.push_back(planner.relevant_goal_mask);
    append_semantic_string(key, planner.setup_action_id);
    append_semantic_string(key, planner.followup_action_id);
    append_semantic_string(key, planner.cleanup_action_id);
    append_semantic_string(
        key, planner.constructive_finish_action_id);
    key.push_back(
        static_cast<std::uint64_t>(
            planner.resource_quantities.size()));
    for (const auto& [resource, quantity] :
         planner.resource_quantities) {
        append_semantic_string(key, resource);
        key.push_back(std::bit_cast<std::uint64_t>(quantity));
    }
    return key;
}

bool planner_operator_structurally_equal(
    const PlannerOperator& left,
    const PlannerOperator& right) {
    return left.kind == right.kind &&
           left.option_kind == right.option_kind &&
           left.primitive_action_id == right.primitive_action_id &&
           left.primitive_program_action_ids ==
               right.primitive_program_action_ids &&
           left.intended_side == right.intended_side &&
           left.exit_goal_slots == right.exit_goal_slots &&
           left.exit_min_satisfied == right.exit_min_satisfied &&
           left.carrier_goal_slot == right.carrier_goal_slot &&
           left.conditional_action_id == right.conditional_action_id &&
           left.bestiary_create_action_id ==
               right.bestiary_create_action_id &&
           left.bestiary_restore_action_id ==
               right.bestiary_restore_action_id &&
           left.automatic_kind == right.automatic_kind &&
           left.relevant_goal_mask == right.relevant_goal_mask &&
           left.setup_action_id == right.setup_action_id &&
           left.followup_action_id == right.followup_action_id &&
           left.cleanup_action_id == right.cleanup_action_id &&
           left.constructive_finish_action_id ==
               right.constructive_finish_action_id &&
           same_resource_quantities(
               left.resource_quantities,
               right.resource_quantities);
}

PlannerOperatorRuntimeSemantics planner_operator_runtime_semantics(
        const PlannerOperator& planner,
        const ActionRegistry& registry) {
    PlannerOperatorRuntimeSemantics semantics;
    if (planner.option_kind == FixedOptionKind::ImprintRetry) {
        if (planner.kind != PlannerOperatorKind::FixedOption ||
            planner.bestiary_create_action == kNoId ||
            planner.bestiary_create_action_id.empty() ||
            planner.bestiary_restore_action == kNoId ||
            planner.bestiary_restore_action_id.empty()) {
            throw std::invalid_argument(
                "imprint retry planner operator has incomplete "
                "checkpoint dependencies");
        }
        /*
         * Checkpoint creation preserves the live item, but its legality
         * observes rarity and the immutable corruption/mirror flags. Restore
         * is the option kernel's internal exact return-to-entry loop, so it
         * has no outgoing policy continuation path of its own.
         */
        ActionDescriptor checkpoint_create;
        checkpoint_create.id =
            "internal:bestiary_imprint_checkpoint_create";
        checkpoint_create.refinement.schema_version =
            kActionRefinementContractVersion;
        checkpoint_create.refinement.observed_item_features =
            refinement_feature(RefinementFeature::Rarity) |
            refinement_feature(RefinementFeature::Corrupted) |
            refinement_feature(RefinementFeature::Mirrored);
        checkpoint_create.refinement.preserved_item_features =
            kAllRefinementItemFeatures;
        checkpoint_create.refinement.preserved_affixes.push_back({});
        RefinementAffixFlow identity;
        identity.preserved_features =
            kAllRefinementAffixFeatures;
        checkpoint_create.refinement.affix_flows.push_back(
            std::move(identity));
        canonicalize_and_validate_action_refinement_contract(
            checkpoint_create);
        semantics.ordered_program.push_back(
            {kNoId, std::move(checkpoint_create.refinement)});
    } else if (
        planner.bestiary_create_action != kNoId ||
        !planner.bestiary_create_action_id.empty() ||
        planner.bestiary_restore_action != kNoId ||
        !planner.bestiary_restore_action_id.empty()) {
        throw std::invalid_argument(
            "non-imprint planner operator has checkpoint dependencies");
    }
    const auto append_step =
        [&](const std::uint32_t action) {
            if (action == kNoId) {
                throw std::invalid_argument(
                    "planner operator runtime program contains no action");
            }
            if (action >= registry.actions.size()) {
                throw std::invalid_argument(
                    "planner operator has a runtime dependency outside "
                    "the action registry");
            }
            ActionDescriptor admitted = registry.actions[action];
            canonicalize_and_validate_action_refinement_contract(
                admitted);
            semantics.ordered_program.push_back(
                {action, std::move(admitted.refinement)});
        };
    for (const std::uint32_t action :
         planner.primitive_program) {
        append_step(action);
    }
    if (planner.conditional_action != kNoId &&
        std::none_of(
            semantics.ordered_program.begin(),
            semantics.ordered_program.end(),
            [&](const PlannerOperatorRuntimeStep& step) {
                return step.action ==
                       planner.conditional_action;
            })) {
        append_step(planner.conditional_action);
    }
    /*
     * constructive_finish_action witnesses synthesis/admission of an upper
     * policy. It is not executed by this PlannerOperator and therefore must
     * not broaden its observation or preservation contract.
     */
    if (semantics.ordered_program.empty()) {
        throw std::invalid_argument(
            "planner operator has no runtime action program");
    }
    const auto require_role_in_program =
        [&](const std::uint32_t action,
            const char* role) {
            if (action == kNoId) return;
            if (std::none_of(
                    semantics.ordered_program.begin(),
                    semantics.ordered_program.end(),
                    [&](const PlannerOperatorRuntimeStep& step) {
                        return step.action == action;
                    })) {
                throw std::invalid_argument(
                    std::string{"planner operator "} + role +
                    " is absent from its runtime action program");
            }
        };
    require_role_in_program(
        planner.setup_action, "setup action");
    require_role_in_program(
        planner.followup_action, "followup action");
    require_role_in_program(
        planner.cleanup_action, "cleanup action");
    semantics.action_dependencies.reserve(
        semantics.ordered_program.size());
    for (const PlannerOperatorRuntimeStep& step :
         semantics.ordered_program) {
        if (step.action != kNoId) {
            semantics.action_dependencies.push_back(step.action);
        }
    }
    std::sort(
        semantics.action_dependencies.begin(),
        semantics.action_dependencies.end());
    semantics.action_dependencies.erase(
        std::unique(
            semantics.action_dependencies.begin(),
            semantics.action_dependencies.end()),
        semantics.action_dependencies.end());
    if (semantics.action_dependencies.empty()) {
        throw std::invalid_argument(
            "planner operator has no runtime action dependency");
    }
    const auto add_execution_path =
        [&](std::vector<PlannerOperatorRuntimeStep> path) {
            if (path.empty()) return;
            const auto same_path =
                [&](const std::vector<PlannerOperatorRuntimeStep>&
                        existing) {
                    if (existing.size() != path.size()) return false;
                    for (std::size_t index = 0;
                         index < path.size(); ++index) {
                        if (existing[index].action !=
                            path[index].action) {
                            return false;
                        }
                    }
                    return true;
                };
            if (std::none_of(
                    semantics.execution_paths.begin(),
                    semantics.execution_paths.end(),
                    same_path)) {
                semantics.execution_paths.push_back(
                    std::move(path));
            }
        };
    if (planner.kind == PlannerOperatorKind::Primitive) {
        if (planner.primitive_action == kNoId ||
            semantics.ordered_program.size() != 1 ||
            semantics.ordered_program.front().action !=
                planner.primitive_action ||
            semantics.action_dependencies.size() != 1) {
            throw std::invalid_argument(
                "primitive planner operator has an inconsistent "
                "runtime program");
        }
        add_execution_path(semantics.ordered_program);
        semantics.compatibility_refinement =
            semantics.ordered_program.front().refinement;
        return semantics;
    }

    const std::size_t preparation_steps =
        planner.primitive_program.size();
    if (planner.conditional_action != kNoId) {
        const auto conditional = std::find_if(
            semantics.ordered_program.begin(),
            semantics.ordered_program.end(),
            [&](const PlannerOperatorRuntimeStep& step) {
                return step.action == planner.conditional_action;
            });
        if (conditional ==
            semantics.ordered_program.end()) {
            throw std::invalid_argument(
                "conditional planner action is absent from its "
                "runtime program");
        }
        /*
         * The option may enter with its carrier already prepared, finish a
         * complete preparation attempt without taking the conditional step,
         * or execute the conditional step after that complete attempt.
         * Individual preparation prefixes are not executable exits.
         */
        add_execution_path({*conditional});
        std::vector<PlannerOperatorRuntimeStep> preparation{
            semantics.ordered_program.begin(),
            semantics.ordered_program.begin() +
                static_cast<std::ptrdiff_t>(preparation_steps)};
        add_execution_path(preparation);
        if (preparation.empty() ||
            preparation.back().action !=
                planner.conditional_action) {
            preparation.push_back(*conditional);
        }
        add_execution_path(std::move(preparation));
    } else if (
        !semantics.ordered_program.empty() &&
        action_observes_modifier_offer(
            registry.actions.at(
                semantics.ordered_program.back().action))) {
        /*
         * An observed modifier offer may be empty. In that branch the
         * completed preparation is the whole path; otherwise the selected
         * choice is the final step.
         */
        add_execution_path(
            std::vector<PlannerOperatorRuntimeStep>{
                semantics.ordered_program.begin(),
                semantics.ordered_program.end() - 1});
        add_execution_path(semantics.ordered_program);
    } else {
        add_execution_path(semantics.ordered_program);
    }
    if (semantics.execution_paths.empty()) {
        add_execution_path(semantics.ordered_program);
    }
    std::sort(
        semantics.execution_paths.begin(),
        semantics.execution_paths.end(),
        [](const std::vector<PlannerOperatorRuntimeStep>& left,
           const std::vector<PlannerOperatorRuntimeStep>& right) {
            return std::lexicographical_compare(
                left.begin(), left.end(),
                right.begin(), right.end(),
                [](const PlannerOperatorRuntimeStep& a,
                   const PlannerOperatorRuntimeStep& b) {
                    return a.action < b.action;
                });
        });

    ActionRefinementContract& composite =
        semantics.compatibility_refinement;
    composite.schema_version =
        kActionRefinementContractVersion;
    for (const PlannerOperatorRuntimeStep& step :
         semantics.ordered_program) {
        const ActionRefinementContract& dependency =
            step.refinement;
        composite.observed_item_features |=
            dependency.observed_item_features;
        composite.preserved_item_features |=
            dependency.preserved_item_features;
        composite.destroyed_item_features |=
            dependency.destroyed_item_features;
        composite.observed_modifier_tag_ids.insert(
            composite.observed_modifier_tag_ids.end(),
            dependency.observed_modifier_tag_ids.begin(),
            dependency.observed_modifier_tag_ids.end());
        composite.affix_observations.insert(
            composite.affix_observations.end(),
            dependency.affix_observations.begin(),
            dependency.affix_observations.end());
        composite.item_affix_dependencies.insert(
            composite.item_affix_dependencies.end(),
            dependency.item_affix_dependencies.begin(),
            dependency.item_affix_dependencies.end());
        composite.affix_flows.insert(
            composite.affix_flows.end(),
            dependency.affix_flows.begin(),
            dependency.affix_flows.end());
        composite.preserved_affixes.insert(
            composite.preserved_affixes.end(),
            dependency.preserved_affixes.begin(),
            dependency.preserved_affixes.end());
        composite.destroyed_affixes.insert(
            composite.destroyed_affixes.end(),
            dependency.destroyed_affixes.begin(),
            dependency.destroyed_affixes.end());
        if (dependency.outcome_observation !=
            RefinementOutcomeObservation::None) {
            if (composite.outcome_observation !=
                    RefinementOutcomeObservation::None &&
                composite.outcome_observation !=
                    dependency.outcome_observation) {
                throw std::invalid_argument(
                    "planner runtime combines incompatible observed-choice "
                    "vocabularies");
            }
            composite.outcome_observation =
                dependency.outcome_observation;
        }
    }
    composite.resets_to_fresh_item = false;

    std::sort(
        composite.observed_modifier_tag_ids.begin(),
        composite.observed_modifier_tag_ids.end());
    composite.observed_modifier_tag_ids.erase(
        std::unique(
            composite.observed_modifier_tag_ids.begin(),
            composite.observed_modifier_tag_ids.end()),
        composite.observed_modifier_tag_ids.end());
    const auto selector_less =
        [](const RefinementAffixSelector& left,
           const RefinementAffixSelector& right) {
            const auto left_scalars = std::tie(
                left.required_affix_traits,
                left.forbidden_affix_traits,
                left.required_item_traits,
                left.forbidden_item_traits);
            const auto right_scalars = std::tie(
                right.required_affix_traits,
                right.forbidden_affix_traits,
                right.required_item_traits,
                right.forbidden_item_traits);
            return left_scalars != right_scalars
                       ? left_scalars < right_scalars
                       : left.required_tag_ids <
                             right.required_tag_ids;
        };
    const auto canonicalize_selectors =
        [&](std::vector<RefinementAffixSelector>& selectors) {
            std::sort(
                selectors.begin(), selectors.end(),
                selector_less);
            selectors.erase(
                std::unique(
                    selectors.begin(), selectors.end()),
                selectors.end());
        };
    canonicalize_selectors(composite.preserved_affixes);
    canonicalize_selectors(composite.destroyed_affixes);

    std::sort(
        composite.affix_observations.begin(),
        composite.affix_observations.end(),
        [&](const RefinementAffixObservation& left,
            const RefinementAffixObservation& right) {
            return selector_less(
                left.selector, right.selector);
        });
    std::vector<RefinementAffixObservation>
        merged_observations;
    for (RefinementAffixObservation observation :
         composite.affix_observations) {
        if (!merged_observations.empty() &&
            merged_observations.back().selector ==
                observation.selector) {
            merged_observations.back().features |=
                observation.features;
        } else {
            merged_observations.push_back(
                std::move(observation));
        }
    }
    composite.affix_observations =
        std::move(merged_observations);

    std::sort(
        composite.item_affix_dependencies.begin(),
        composite.item_affix_dependencies.end(),
        [](const RefinementItemAffixDependency& left,
           const RefinementItemAffixDependency& right) {
            return std::tie(
                       left.item_features,
                       left.survivor_affix_features) <
                   std::tie(
                       right.item_features,
                       right.survivor_affix_features);
        });
    composite.item_affix_dependencies.erase(
        std::unique(
            composite.item_affix_dependencies.begin(),
            composite.item_affix_dependencies.end()),
        composite.item_affix_dependencies.end());
    std::sort(
        composite.affix_flows.begin(),
        composite.affix_flows.end(),
        [&](const RefinementAffixFlow& left,
            const RefinementAffixFlow& right) {
            if (selector_less(
                    left.source_selector,
                    right.source_selector)) {
                return true;
            }
            if (selector_less(
                    right.source_selector,
                    left.source_selector)) {
                return false;
            }
            return std::tie(
                       left.set_affix_traits,
                       left.cleared_affix_traits,
                       left.preserved_features,
                       left.preserves_modifier_classification) <
                   std::tie(
                       right.set_affix_traits,
                       right.cleared_affix_traits,
                       right.preserved_features,
                       right.preserves_modifier_classification);
        });
    composite.affix_flows.erase(
        std::unique(
            composite.affix_flows.begin(),
            composite.affix_flows.end()),
        composite.affix_flows.end());
    return semantics;
}

bool fixed_option_choice_retries_locally(
        const std::uint32_t entry_state,
        const OptionKernel& kernel,
        const std::uint32_t successor_state,
        const std::uint32_t actual_state,
        const std::vector<std::uint32_t>&
            behavioral_representative_by_state) {
    const auto same_behavioral_state =
        [&](const std::uint32_t left,
            const std::uint32_t right) {
            if (left == right) return true;
            if (left == kNoId || right == kNoId ||
                behavioral_representative_by_state.empty() ||
                left >=
                    behavioral_representative_by_state.size()) {
                return false;
            }
            const std::uint32_t projected_left =
                behavioral_representative_by_state[left];
            if (projected_left == right) return true;
            return right <
                       behavioral_representative_by_state.size() &&
                   projected_left ==
                       behavioral_representative_by_state[right];
        };
    if (successor_state == kNoId ||
        same_behavioral_state(successor_state, entry_state) ||
        std::find(
            kernel.retry_states.begin(),
            kernel.retry_states.end(),
            actual_state) != kernel.retry_states.end()) {
        return true;
    }
    return std::any_of(
        kernel.retry_states.begin(),
        kernel.retry_states.end(),
        [&](const std::uint32_t retry_state) {
            return same_behavioral_state(
                retry_state, actual_state);
        });
}

ExecutableFixedOptionRecipe fixed_option_executable_recipe(
        const CalcContext& calc,
        const std::uint32_t entry_state,
        const PlannerOperator& planner,
        const OptionKernel& kernel,
        const std::vector<ObservedUnveilPreference>& preferences,
        const std::vector<std::uint32_t>&
            behavioral_representative_by_state) {
    if (planner.kind != PlannerOperatorKind::FixedOption) {
        throw std::invalid_argument(
            "executable fixed-option recipe received a primitive "
            "operator");
    }
    if (entry_state >= calc.state_count()) {
        throw std::invalid_argument(
            "executable fixed-option recipe entry is outside the "
            "state table");
    }
    if (!kernel.supported || !kernel.legal ||
        !kernel.terminates_almost_surely) {
        throw std::invalid_argument(
            "fixed-option member has no legal exact kernel");
    }

    const auto state_key =
        [&](const std::uint32_t state,
            const char* subject) {
            const std::uint32_t resolved =
                state == kNoId ? entry_state : state;
            if (resolved >= calc.state_count()) {
                throw std::invalid_argument(
                    std::string(subject) +
                    " references a state outside the exact table");
            }
            return exact_abstract_state_key(
                calc.state(resolved), 0);
        };
    const auto canonical_state_keys =
        [&](const std::vector<std::uint32_t>& states,
            const char* subject) {
            std::vector<std::vector<std::uint64_t>> keys;
            keys.reserve(states.size());
            for (const std::uint32_t state : states) {
                keys.push_back(state_key(state, subject));
            }
            std::sort(keys.begin(), keys.end());
            keys.erase(
                std::unique(keys.begin(), keys.end()),
                keys.end());
            return keys;
        };

    ExecutableFixedOptionRecipe recipe;
    recipe.entry_continues = kernel.entry_continues;
    const bool renewal_route =
        planner.option_kind == FixedOptionKind::Renewal ||
        planner.option_kind == FixedOptionKind::ProtectedRepeat ||
        planner.option_kind ==
            FixedOptionKind::TemporaryBenchRepeat;
    bool observed = false;
    if (!planner.primitive_program.empty()) {
        const std::uint32_t final_action =
            planner.primitive_program.back();
        if (final_action >= calc.registry().actions.size()) {
            throw std::invalid_argument(
                "fixed option has an invalid primitive program");
        }
        observed = action_observes_modifier_offer(
            calc.registry().actions[final_action]);
    }
    if (observed && !renewal_route) {
        throw std::invalid_argument(
            "fixed option has an unsupported observed execution "
            "recipe");
    }

    if (planner.option_kind == FixedOptionKind::ImprintRetry ||
        (renewal_route && !observed) ||
        (planner.option_kind ==
             FixedOptionKind::FracturePrepare &&
         !kernel.entry_continues)) {
        recipe.retry_state_keys =
            canonical_state_keys(
                kernel.retry_states,
                "fixed-option retry predicate");
    }
    if (planner.option_kind ==
            FixedOptionKind::FracturePrepare &&
        !kernel.entry_continues) {
        recipe.continuation_state_keys =
            canonical_state_keys(
                kernel.continuation_states,
                "fixed-option continuation predicate");
        std::vector<std::vector<std::uint64_t>> overlap;
        std::set_intersection(
            recipe.retry_state_keys.begin(),
            recipe.retry_state_keys.end(),
            recipe.continuation_state_keys.begin(),
            recipe.continuation_state_keys.end(),
            std::back_inserter(overlap));
        if (!overlap.empty()) {
            throw std::invalid_argument(
                "fixed option has overlapping retry and "
                "continuation predicates");
        }
    }

    if (!observed) {
        if (!preferences.empty() ||
            !kernel.observation_choice_options.empty() ||
            !kernel.observation_choice_groups.empty()) {
            throw std::invalid_argument(
                "unobserved fixed option has an unexpected choice "
                "sidecar");
        }
        return recipe;
    }
    if (preferences.empty() ||
        kernel.observation_choice_options.empty()) {
        throw std::invalid_argument(
            "observed fixed option has no populated choice sidecar");
    }

    std::map<
        std::vector<std::uint64_t>,
        std::map<std::uint32_t, bool>>
        offered_by_observation;
    for (const OutcomeChoiceOption& option :
         kernel.observation_choice_options) {
        if (option.mod_id >= calc.session().mod_count) {
            throw std::invalid_argument(
                "observed fixed option exposes an invalid modifier");
        }
        const std::vector<std::uint64_t> observation =
            state_key(
                option.observation_state,
                "fixed-option observation");
        const bool retry_local =
            fixed_option_choice_retries_locally(
                entry_state, kernel, option.state,
                option.actual_state,
                behavioral_representative_by_state);
        const auto [found, inserted] =
            offered_by_observation[observation].emplace(
                option.mod_id, retry_local);
        if (!inserted && found->second != retry_local) {
            throw std::invalid_argument(
                "one observed modifier has conflicting branch roles");
        }
    }

    std::set<std::vector<std::uint64_t>>
        seen_observations;
    recipe.offers.reserve(preferences.size());
    for (const ObservedUnveilPreference& preference :
         preferences) {
        ExecutableFixedOptionOffer offer;
        offer.observation_state_key =
            state_key(
                preference.observation_state,
                "fixed-option preference observation");
        if (!seen_observations.insert(
                offer.observation_state_key).second) {
            throw std::invalid_argument(
                "fixed-option preference repeats an observation");
        }
        const auto available =
            offered_by_observation.find(
                offer.observation_state_key);
        if (available == offered_by_observation.end() ||
            preference.choices.empty()) {
            throw std::invalid_argument(
                "fixed-option preference does not match an observed "
                "offer");
        }
        for (const auto& [mod, unused] : available->second) {
            (void)unused;
            offer.offered_mod_ids.push_back(mod);
        }
        std::set<std::uint32_t> seen_mods;
        for (const ObservedUnveilChoice& choice :
             preference.choices) {
            if (choice.mod_id >= calc.session().mod_count ||
                !seen_mods.insert(choice.mod_id).second) {
                throw std::invalid_argument(
                    "fixed-option preference has an invalid or "
                    "repeated modifier");
            }
            const auto offered =
                available->second.find(choice.mod_id);
            if (offered == available->second.end()) {
                throw std::invalid_argument(
                    "fixed-option preference chooses outside its "
                    "offer");
            }
            const bool retry_local =
                fixed_option_choice_retries_locally(
                    entry_state, kernel,
                    choice.successor_state,
                    choice.actual_state,
                    behavioral_representative_by_state);
            if (retry_local != offered->second) {
                throw std::invalid_argument(
                    "fixed-option preference gives an offered "
                    "modifier the wrong branch role");
            }
            offer.ordered_choices.push_back({
                choice.mod_id, retry_local});
        }
        if (seen_mods.size() != available->second.size()) {
            throw std::invalid_argument(
                "fixed-option preference does not cover its offer");
        }
        recipe.offers.push_back(std::move(offer));
    }
    if (recipe.offers.size() !=
        offered_by_observation.size()) {
        throw std::invalid_argument(
            "fixed-option preferences do not cover every offer");
    }
    std::sort(
        recipe.offers.begin(), recipe.offers.end(),
        [](const ExecutableFixedOptionOffer& left,
           const ExecutableFixedOptionOffer& right) {
            return left.observation_state_key <
                   right.observation_state_key;
        });
    return recipe;
}

std::vector<std::uint64_t> fixed_option_executable_recipe_key(
        const ExecutableFixedOptionRecipe& recipe) {
    std::vector<std::uint64_t> key{
        0x706366786f707431ull, /* "pcfxopt1" */
        recipe.entry_continues ? 1u : 0u};
    const auto append_state_keys =
        [&](const std::vector<std::vector<std::uint64_t>>& states) {
            key.push_back(states.size());
            for (const std::vector<std::uint64_t>& state : states) {
                key.push_back(state.size());
                key.insert(
                    key.end(), state.begin(), state.end());
            }
        };
    append_state_keys(recipe.retry_state_keys);
    append_state_keys(recipe.continuation_state_keys);
    key.push_back(recipe.offers.size());
    for (const ExecutableFixedOptionOffer& offer :
         recipe.offers) {
        key.push_back(offer.observation_state_key.size());
        key.insert(
            key.end(),
            offer.observation_state_key.begin(),
            offer.observation_state_key.end());
        key.push_back(offer.offered_mod_ids.size());
        for (const std::uint32_t mod :
             offer.offered_mod_ids) {
            key.push_back(mod);
        }
        key.push_back(offer.ordered_choices.size());
        for (const ExecutableFixedOptionChoice& choice :
             offer.ordered_choices) {
            key.push_back(choice.mod_id);
            key.push_back(choice.retry_local ? 1u : 0u);
        }
    }
    return key;
}


} // namespace solver
} // namespace poecraft
