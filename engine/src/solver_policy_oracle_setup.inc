  public:
    ProductionPolicyOracle(
        CalcContext& coarse,
        const SolveResult& solved,
        const pc_item_state& exact_start,
        const std::unordered_map<std::string, double>& prices,
        const SolveOptions& options,
        RefinementLimits limits,
        PolicyLiftAdapterTelemetry& telemetry,
        const ReoptimizationSeed* reoptimization_seed,
        const bool force_semantic_strict_identity = false)
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
                        *reoptimization_seed)),
          force_semantic_strict_identity_(
              force_semantic_strict_identity) {
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

    ExactState quotient_root_state() const {
        return materialize_exact_state(
            root_key_, known_.at(root_key_));
    }

    std::uint32_t quotient_locator(const StableKey& key) const {
        const auto known = known_.find(key);
        if (known == known_.end()) {
            throw AdapterFailure(
                PolicyExactLiftStatus::InvalidSolveState,
                "quotient carrier has no strict replay locator");
        }
        return known->second.strict_state;
    }

    ExactState quotient_materialize_locator(
            const std::uint32_t strict_state) {
        auto [state, inserted] = intern_strict_state(strict_state);
        (void)inserted;
        return state;
    }

    std::vector<std::uint32_t> quotient_equivalent_locators(
            const StableKey& key) const {
        const auto known = known_.find(key);
        if (known == known_.end()) {
            throw AdapterFailure(
                PolicyExactLiftStatus::InvalidSolveState,
                "quotient carrier lost its replay equivalence");
        }
        std::vector<std::uint32_t> locators{
            known->second.strict_state};
        locators.insert(
            locators.end(),
            known->second.equivalent_strict_states.begin(),
            known->second.equivalent_strict_states.end());
        std::sort(locators.begin(), locators.end());
        locators.erase(
            std::unique(locators.begin(), locators.end()),
            locators.end());
        return locators;
    }

    void quotient_release_carrier(const StableKey& key) {
        for (auto kernel = kernels_.begin(); kernel != kernels_.end();) {
            kernel = kernel->first.first == key
                         ? kernels_.erase(kernel)
                         : std::next(kernel);
        }
        for (auto recipe = choice_recipes_.begin();
             recipe != choice_recipes_.end();) {
            recipe = recipe->first.first == key
                         ? choice_recipes_.erase(recipe)
                         : std::next(recipe);
        }
        known_.erase(key);
        adapter_memory_cache_initialized_ = false;
        adapter_memory_checks_since_audit_ = 0;
        adapter_memory_cached_non_child_bytes_ = 0;
        adapter_memory_projected_growth_bytes_ = 0;
    }

    std::vector<QuotientOracleCompactRow> quotient_compact_action_rows(
            const ExactState& state) {
        if (state.terminal) return {};
        std::vector<QuotientOracleCompactRow> rows;
        std::set<StableKey> semantic_rows;
        const auto append = [&](SelectedAction selected) {
            if (!semantic_rows.insert(selected.semantic_key).second) return;
            if (telemetry_.strict_kernels_built >=
                limits_.max_exact_kernels) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::ResourceCap,
                    "strict policy discovery reached max_exact_kernels",
                    "max_exact_kernels");
            }
            QuotientOracleCompactRow row;
            row.selected = std::move(selected);
            row.choice_recipe = choice_recipe(
                state.stable_key, row.selected);
            row.action_cost = visit_kernel_transitions(
                state.stable_key, row.selected,
                [&](const std::uint32_t successor,
                    const double probability) {
                    row.transitions.push_back({successor, probability});
                });
            if (telemetry_.strict_transitions_built +
                    row.transitions.size() >
                limits_.max_transitions) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::ResourceCap,
                    "strict policy discovery reached max_transitions",
                    "max_transitions");
            }
            telemetry_.strict_transitions_built += row.transitions.size();
            ++telemetry_.strict_kernels_built;
            telemetry_.strict_calc_owned_bytes =
                strict_->fast_estimated_owned_bytes();
            release_published_kernel_storage(state, row.selected);
            rows.push_back(std::move(row));
        };
        bool current_complete = false;
        try {
            std::optional<SelectedAction> current = selected_action(state);
            if (current.has_value()) {
                append(std::move(*current));
                current_complete = true;
            }
        } catch (const AdapterFailure& error) {
            if (error.status == PolicyExactLiftStatus::ResourceCap) throw;
        }
        const bool affected =
            enable_local_reoptimization_ &&
            reoptimization_parents_.contains(state.coarse_state);
        if (affected || !current_complete) {
            for (const std::uint32_t operator_index : admitted_operators_) {
                saturating_add(
                    telemetry_.local_state_action_rows_scheduled, 1);
                std::optional<SelectedAction> candidate =
                    candidate_selection(state.stable_key, operator_index, 0);
                if (!candidate.has_value() ||
                    semantic_rows.contains(candidate->semantic_key)) {
                    continue;
                }
                try {
                    append(std::move(*candidate));
                    saturating_add(
                        telemetry_.local_state_action_rows_evaluated, 1);
                } catch (const AdapterFailure& error) {
                    if (error.status == PolicyExactLiftStatus::ResourceCap) {
                        throw;
                    }
                }
            }
        }
        if (rows.empty()) {
            throw AdapterFailure(
                PolicyExactLiftStatus::LocalReoptimizationRequired,
                "proof-carrying quotient carrier has no certified admitted row");
        }
        return rows;
    }

    void quotient_install_streamed_recipe(
            const SelectedAction& selected,
            const ExactChoiceRecipe& recipe) {
        const auto [stored, inserted] =
            quotient_streamed_recipes_.emplace(
                selected.semantic_key, recipe);
        if (!inserted && stored->second != recipe) {
            throw AdapterFailure(
                PolicyExactLiftStatus::InvalidSolveState,
                "streamed semantic decision changed its exact recipe");
        }
    }

    std::vector<QuotientOracleRow> quotient_action_rows(
            const ExactState& state) {
        if (state.terminal) return {};
        std::vector<QuotientOracleRow> rows;
        std::set<StableKey> semantic_rows;
        const auto append = [&](SelectedAction selected,
                                ExactActionKernel kernel) {
            if (!semantic_rows.insert(selected.semantic_key).second) return;
            rows.push_back({std::move(selected), std::move(kernel)});
        };
        bool current_complete = false;
        try {
            std::optional<SelectedAction> current = selected_action(state);
            if (current.has_value()) {
                ExactActionKernel kernel = exact_kernel(state, *current);
                append(std::move(*current), std::move(kernel));
                current_complete = true;
            }
        } catch (const AdapterFailure& error) {
            if (error.status == PolicyExactLiftStatus::ResourceCap) throw;
        }
        const bool affected =
            enable_local_reoptimization_ &&
            reoptimization_parents_.contains(state.coarse_state);
        if (affected || !current_complete) {
            for (const std::uint32_t operator_index : admitted_operators_) {
                saturating_add(
                    telemetry_.local_state_action_rows_scheduled, 1);
                std::optional<SelectedAction> candidate =
                    candidate_selection(state.stable_key, operator_index, 0);
                if (!candidate.has_value() ||
                    semantic_rows.contains(candidate->semantic_key)) {
                    continue;
                }
                const ExactActionKernel* kernel =
                    candidate_kernel(state.stable_key, *candidate);
                if (kernel == nullptr) continue;
                saturating_add(
                    telemetry_.local_state_action_rows_evaluated, 1);
                ExactActionKernel copied = *kernel;
                release_published_kernel_storage(state, *candidate);
                append(std::move(*candidate), std::move(copied));
            }
        }
        if (rows.empty()) {
            throw AdapterFailure(
                PolicyExactLiftStatus::LocalReoptimizationRequired,
                "proof-carrying quotient carrier has no certified admitted row");
        }
        return rows;
    }

    quotient::CertifiedRowIdentity quotient_proof_identity(
            const ExactState& source,
            const SelectedAction& selected,
            quotient::CoverageDescriptor coverage,
            std::vector<quotient::ProofProjectedArc> arcs,
            const double total_probability) const {
        quotient::CertifiedRowIdentity identity;
        identity.source_coarse_parent = source.coarse_state_key;
        identity.observation_requirement =
            selected.routing_observes;
        identity.observed_features = observe_features(
            source.features, identity.observation_requirement);
        identity.action_id = selected.action_id;
        identity.semantic_action_identity = selected.semantic_key;
        identity.runtime_contract_program_identity =
            canonical_selected_runtime_contract_identity(selected);
        /* The semantic decision key already contains the complete immutable
         * exact observe-then-decide recipe. Retain it independently as the
         * proof field so recipe and runtime-contract changes invalidate by
         * full tuple rather than by a hash. */
        identity.exact_choice_recipe_identity = selected.semantic_key;
        const auto address = [](const void* value) {
            return static_cast<std::uint64_t>(
                reinterpret_cast<std::uintptr_t>(value));
        };
        identity.session_identity = {
            0x7063717365737331ull,
            address(&strict_->session()),
            strict_->session().base_index,
            strict_->session().item_level};
        identity.layout_identity = {
            0x7063716c61796f31ull,
            address(&strict_->layout()),
            strict_->layout().slots.size(),
            strict_->layout().junk_classes.size()};
        identity.goal_identity = {
            0x706371676f616c31ull,
            address(&strict_->goal()),
            strict_->goal().rarity,
            strict_->goal().required_satisfied_slots()};
        identity.artifact_identity = {
            0x7063716172746631ull,
            address(strict_->session().data.get()),
            strict_->session().data->artifact_schema_version};
        identity.start_identity = root_key_;
        identity.solver_options_identity = {
            0x7063716f70747331ull,
            options_.goal_progress_gated_reforges ? 1u : 0u,
            admitted_operators_.size()};
        identity.solver_options_identity.insert(
            identity.solver_options_identity.end(),
            admitted_operators_.begin(), admitted_operators_.end());
        identity.coverage = std::move(coverage);
        identity.exact_total_probability = total_probability;
        identity.projected_arcs = std::move(arcs);
        return quotient::canonical_certified_row_identity(
            std::move(identity));
    }

    void quotient_install_policy(
            const StableKey& carrier,
            const SelectedAction& selected) {
        const auto known = known_.find(carrier);
        if (known == known_.end() || known->second.terminal) {
            throw AdapterFailure(
                PolicyExactLiftStatus::InvalidSolveState,
                "quotient policy installation names an invalid carrier");
        }
        exact_policy_[carrier] = selected;
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
        const bool streamed = std::any_of(
            refinement.classes.begin(), refinement.classes.end(),
            [](const RefinedPolicyClass& cls) {
                return !cls.strict_members.empty();
            });
        SolveResult lifted = streamed
            ? make_streamed_lifted_solve_result(
                  refinement, evaluation, &refined_routing)
            : make_lifted_solve_result(
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
