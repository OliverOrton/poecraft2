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
                "selected planner option has no legal exact kernel "
                "(entry_state=" + std::to_string(entry_state) +
                ", operator=" + std::to_string(operator_index) +
                ", planner_id=" + planner.id +
                ", calc_states=" +
                std::to_string(source.state_count()) + ")");
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
                "selected planner option has no legal exact kernel "
                "(entry_state=" + std::to_string(entry_state) +
                ", operator=" + std::to_string(operator_index) +
                ", planner_id=" + planner.id +
                ", calc_states=" +
                std::to_string(source.state_count()) + ")");
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
        if (planner.automatic_kind ==
                AutomaticCandidateKind::Fracture &&
            coarse_.product_solver_parent()) {
            const ProductFractureKernel kernel =
                solve_detail::build_product_fracture_kernel(
                    coarse_, coarse_state,
                    planner.relevant_goal_mask);
            if (!kernel.eligible || kernel.exits.empty()) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::InvalidSolveState,
                    "selected product-local Fracture row has no exact "
                    "coarse quotient kernel");
            }
            std::vector<std::uint32_t> result;
            result.reserve(kernel.exits.size());
            for (const OutcomeEntry& exit : kernel.exits) {
                if (exit.probability != 0.0) {
                    result.push_back(exit.state);
                }
            }
            return result;
        }
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
                            "policy table (source_state=" +
                            std::to_string(state) +
                            ", successor_state=" +
                            std::to_string(successor) +
                            ", solved_policy_size=" +
                            std::to_string(solved_.policy.size()) +
                            ", calc_state_count=" +
                            std::to_string(coarse_.state_count()) +
                            ", operator=" +
                            std::to_string(node.coarse_operator) +
                            ", planner_id=" + planner.id + ")");
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
