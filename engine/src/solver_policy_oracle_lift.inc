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
            refinement.classes.size() *
            sizeof(RefinedPolicyCompileParent));
        add(
            states * sizeof(std::uint32_t));
        for (const RefinedPolicyClass& policy_class :
             refinement.classes) {
            add(
                policy_class.strict_members.capacity() *
                sizeof(std::uint32_t));
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
            refined_routing->parents.clear();
            refined_routing->parent_layout = &coarse_.layout();
            refined_routing->product_solver_parent =
                coarse_.product_solver_parent();
        }
        std::set<std::uint32_t> retained_compile_parents;
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
                if (retained_compile_parents.insert(
                        compile_class.coarse_state).second) {
                    refined_routing->parents.push_back({
                        compile_class.coarse_state,
                        compile_class.coarse_state_key,
                        coarse_.state(compile_class.coarse_state)});
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

    SolveResult make_streamed_lifted_solve_result(
            const RefinementResult& refinement,
            const PolicyEvaluationResult& evaluation,
            RefinedPolicyCompileRouting* refined_routing) {
        RefinementResult representatives = refinement;
        representatives.assignments.clear();
        for (RefinedPolicyClass& cls : representatives.classes) {
            if (cls.strict_members.empty()) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::InvalidSolveState,
                    "streamed refinement class has no strict coverage");
            }
            const std::uint32_t representative =
                *std::min_element(
                    cls.strict_members.begin(),
                    cls.strict_members.end());
            ExactState state = quotient_materialize_locator(representative);
            cls.exact_members = {state.stable_key};
            cls.strict_members.clear();
            if (!cls.terminal && cls.selected_action.has_value()) {
                exact_policy_[state.stable_key] = *cls.selected_action;
                const auto recipe = quotient_streamed_recipes_.find(
                    cls.selected_action->semantic_key);
                if (recipe == quotient_streamed_recipes_.end()) {
                    throw AdapterFailure(
                        PolicyExactLiftStatus::InvalidSolveState,
                        "streamed selected decision has no retained recipe");
                }
                store_choice_recipe(
                    state.stable_key,
                    cls.selected_action->semantic_key,
                    recipe->second);
            }
            representatives.assignments.push_back({
                cls.coarse_state, cls.class_id, 0});
        }
        SolveResult lifted = make_lifted_solve_result(
            representatives, evaluation, refined_routing);
        for (const RefinedPolicyClass& cls : representatives.classes) {
            const StableKey& representative = cls.exact_members.front();
            exact_policy_.erase(representative);
            quotient_release_carrier(representative);
        }

        std::vector<double> value_by_class(
            refinement.classes.size(),
            std::numeric_limits<double>::infinity());
        for (const RefinedClassValue& value : evaluation.class_values) {
            value_by_class.at(value.class_id) = value.value;
        }
        std::uint32_t streamed_start = kNoId;
        const std::uint32_t authored_start =
            strict_->intern_item(exact_start_);
        for (const RefinedPolicyClass& cls : refinement.classes) {
            if (cls.class_id >= value_by_class.size() ||
                !std::isfinite(value_by_class[cls.class_id]) ||
                cls.strict_members.empty() ||
                !std::is_sorted(
                    cls.strict_members.begin(),
                    cls.strict_members.end()) ||
                std::adjacent_find(
                    cls.strict_members.begin(),
                    cls.strict_members.end()) !=
                    cls.strict_members.end()) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::InvalidSolveState,
                    "streamed refinement class lacks canonical exact locator coverage");
            }
            const std::uint32_t representative =
                *std::min_element(
                    cls.strict_members.begin(),
                    cls.strict_members.end());
            if (refined_routing != nullptr) {
                refined_routing->classes.at(cls.class_id).strict_members =
                    cls.strict_members;
                refined_routing->classes.at(cls.class_id)
                    .representative_state = representative;
            }
            lifted.policy_reachable[representative] = 1;
            for (const std::uint32_t strict_state : cls.strict_members) {
                if (strict_state >= strict_->state_count()) {
                    throw AdapterFailure(
                        PolicyExactLiftStatus::InvalidSolveState,
                        "streamed strict coverage exceeds state table");
                }
                lifted.behavioral_representative_by_state[strict_state] =
                    representative;
                if (strict_state == authored_start) {
                    streamed_start = representative;
                }
                lifted.expanded[strict_state] = 1;
                lifted.goal_states[strict_state] = cls.goal ? 1 : 0;
                lifted.values[strict_state] = value_by_class[cls.class_id];
                if (!cls.terminal) {
                    if (!cls.selected_action.has_value() ||
                        cls.selected_action->action_id >=
                            strict_->operators().size()) {
                        throw AdapterFailure(
                            PolicyExactLiftStatus::InvalidSolveState,
                            "streamed class has no selected operator");
                    }
                    const auto streamed_recipe =
                        quotient_streamed_recipes_.find(
                            cls.selected_action->semantic_key);
                    if (streamed_recipe ==
                        quotient_streamed_recipes_.end()) {
                        throw AdapterFailure(
                            PolicyExactLiftStatus::InvalidSolveState,
                            "streamed selected decision has no retained recipe");
                    }
                    const PlannerOperator& planner = strict_->operators().at(
                        cls.selected_action->action_id);
                    lifted.policy[strict_state] = PolicyOperatorRef{
                        planner.kind, cls.selected_action->action_id};
                    /* The streamed quotient already retained one immutable
                     * recipe per semantic decision.  Compilation routes by
                     * the refined class and consumes that same decision;
                     * duplicating the recipe into CalcContext for every
                     * covered strict state is neither a proof dependency nor
                     * a runtime requirement. */
                    const ExactChoiceRecipe& recipe =
                        streamed_recipe->second;
                    if (planner.kind == PlannerOperatorKind::Primitive) {
                        if (recipe.kind ==
                            ExactChoiceRecipeKind::PrimitiveObservedChoice) {
                            lifted.unveil_preferences[strict_state] =
                                recipe.primitive_preferences;
                        }
                    } else {
                        lifted.option_unveil_preferences[strict_state] =
                            recipe.option_preferences;
                    }
                }
            }
        }
        if (streamed_start == kNoId) {
            throw AdapterFailure(
                PolicyExactLiftStatus::InvalidSolveState,
                "streamed strict coverage omits authored start");
        }
        lifted.start_state = streamed_start;
        return lifted;
    }
