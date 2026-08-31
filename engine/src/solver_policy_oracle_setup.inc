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
        const bool force_semantic_strict_identity = false,
        const bool defer_initialization = false,
        const bool enable_quotient_bootstrap = false,
        std::set<std::uint32_t> observation_boundaries = {})
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
          enable_quotient_bootstrap_(enable_quotient_bootstrap),
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
              force_semantic_strict_identity),
          observation_boundaries_(std::move(observation_boundaries)) {
        if (defer_initialization) return;
        auto initialization = initialize_cooperatively();
        while (!initialization.resume()) {}
        (void)initialization.take_result();
    }

    solve_detail::CooperativeTask<bool> initialize_cooperatively() {
        require_local_memory(
            0, "exact-refinement adapter construction");
        auto coarse_discovery =
            discover_coarse_policy_cooperatively();
        while (!coarse_discovery.resume()) {
            co_await solve_detail::CooperativeCheckpoint{
                estimated_owned_bytes()};
        }
        (void)coarse_discovery.take_result();
        index_coarse_states();
        co_await solve_detail::CooperativeCheckpoint{
            estimated_owned_bytes()};
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
        if (enable_local_reoptimization_ ||
            enable_quotient_bootstrap_) {
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
                /* Exact successors may expose a coarse parent that the
                 * optimistic policy never selected. Retain the complete
                 * already-filtered candidate vocabulary up front; the
                 * per-carrier admission authority below still rejects every
                 * operator that is not admitted for that concrete parent. */
                append_admitted_coarse_operator(operator_index);
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
        co_await solve_detail::CooperativeCheckpoint{
            estimated_owned_bytes()};
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
        co_await solve_detail::CooperativeCheckpoint{
            estimated_owned_bytes()};
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
            exact_start_.prefixes, exact_start_.prefix_count);
        retain_start_mods(
            exact_start_.suffixes, exact_start_.suffix_count);
        /* Contract-derived semantic strict carrier with bounded broad-row
         * attribution and an explicit diagnostic-only projected-frontier
         * opt-in. */
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
            false,
            true,
            limits_.use_projected_reforge_frontier ||
                limits_.use_factored_terminal_reforge,
            false,
            limits_.use_factored_terminal_reforge,
            &coarse_.layout(),
            true);
        co_await solve_detail::CooperativeCheckpoint{
            estimated_owned_bytes()};
        strict_->set_reforge_resource_accounting(
            options_.reforge_resource_accounting);
        strict_->set_reforge_provenance_context(
            ReforgeRowOwner::StrictSelected);
        require_local_memory(
            exact_start_mods.capacity() * sizeof(std::uint64_t),
            "strict calculation construction");
        refresh_strict_resource_cap(
            "strict calculation construction",
            exact_start_mods.capacity() * sizeof(std::uint64_t));
        import_admitted_operators();
        co_await solve_detail::CooperativeCheckpoint{
            estimated_owned_bytes()};
        for (auto& [state, node] : coarse_policy_) {
            if (node.coarse_operator == kNoId) continue;
            node.selected = make_selected(
                state,
                strict_operator_for(node.coarse_operator),
                std::nullopt);
            require_local_memory(
                0, "coarse selected-action construction");
            co_await solve_detail::CooperativeCheckpoint{
                estimated_owned_bytes()};
        }
        auto backward_observations =
            propagate_backward_observations_cooperatively();
        while (!backward_observations.resume()) {
            co_await solve_detail::CooperativeCheckpoint{
                backward_observations.retained_bytes()};
        }
        (void)backward_observations.take_result();
        co_await solve_detail::CooperativeCheckpoint{
            estimated_owned_bytes()};
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
        co_await solve_detail::CooperativeCheckpoint{
            estimated_owned_bytes()};

        const std::uint32_t strict_start =
            strict_->intern_item(exact_start_);
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
        co_return true;
    }

    ~ProductionPolicyOracle() override {
        sync_strict_reforge_resource_telemetry();
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

    std::uint32_t quotient_canonical_locator(
            const std::uint32_t strict_state) {
        auto [exact, inserted] = intern_strict_state(strict_state);
        (void)inserted;
        return known_.at(exact.stable_key).strict_state;
    }

    std::uint32_t quotient_seed_locator(const AbstractState& state) {
        const std::uint32_t locator =
            quotient_canonical_locator(strict_->intern_state(state));
        quotient_frontier_seed_locators_.insert(locator);
        return locator;
    }

    bool quotient_requires_bootstrap(const ExactState& state) const {
        const auto known = known_.find(state.stable_key);
        return known != known_.end() &&
               quotient_frontier_seed_locators_.contains(
                   known->second.strict_state);
    }

    AbstractState quotient_export_locator(
            const std::uint32_t strict_state) const {
        return strict_->state(strict_state);
    }

    bool quotient_export_item(
            const std::uint32_t strict_state,
            pc_item_state& item) const {
        return strict_state < strict_->state_count() &&
               strict_->materialize(strict_state, item);
    }

    void quotient_sync_resource_telemetry() {
        sync_strict_reforge_resource_telemetry();
    }

    void quotient_release_transient_kernel_caches() {
        /* Published compact rows and proof payloads are the persistent
         * quotient authority. Calculator distributions remain shared for the
         * complete carrier-wide proof that is currently being built, then can
         * be recreated if a later obligation needs the same mechanic row. */
        strict_->release_solve_transition_caches();
        adapter_memory_cache_initialized_ = false;
        adapter_memory_checks_since_audit_ = 0;
        adapter_memory_cached_non_child_bytes_ = 0;
        adapter_memory_projected_growth_bytes_ = 0;
    }

    ExactState quotient_materialize_locator(
            const std::uint32_t strict_state) {
        auto [state, inserted] = intern_strict_state(strict_state);
        (void)inserted;
        return state;
    }

    std::pair<ExactState, std::uint32_t>
    quotient_import_verified_policy_item(const pc_item_state& item) {
        const std::uint32_t strict_state = strict_->intern_item(item);
        auto [state, inserted] = intern_strict_state(strict_state);
        (void)inserted;
        return {std::move(state), strict_state};
    }

    std::uint32_t quotient_strict_operator_for_coarse(
            const std::uint32_t coarse_operator) const {
        return strict_operator_for(coarse_operator);
    }

    SelectedAction quotient_verified_policy_selected_action(
            const ExactState& exact,
            const std::uint32_t strict_state,
            const std::uint32_t represented_coarse_state,
            const std::uint32_t strict_operator,
            const bool require_authored_choices) {
        const auto known = known_.find(exact.stable_key);
        if (exact.terminal || known == known_.end() ||
            known->second.strict_state != strict_state ||
            exact.coarse_state >= coarse_.state_count() ||
            represented_coarse_state >= coarse_.state_count()) {
            throw AdapterFailure(
                PolicyExactLiftStatus::InvalidSolveState,
                "verified-policy selection names an invalid exact carrier");
        }
        /* The compiled representative owns the fixed observed-choice order;
         * authoritative strict mechanics instantiate that same order on the
         * physical entry.  The physical coarse parent is used only to bind
         * the resulting exact decision/recipe to this strict state. */
        const ExactChoiceRecipe recipe = seeded_choice_recipe(
            represented_coarse_state, *strict_, strict_state,
            strict_operator, require_authored_choices);
        return make_selected(
            exact.coarse_state, strict_operator, strict_state, &recipe);
    }

    VerifiedPolicyExactActionRow
    quotient_verified_policy_selected_exact_row(
            const ExactState& exact,
            const std::uint32_t strict_state,
            const SelectedAction& selected,
            StableKey exact_entry_identity,
            StableKey operator_identity) {
        const auto started = std::chrono::steady_clock::now();
        VerifiedPolicyExactActionRow row;
        row.exact_entry_identity = std::move(exact_entry_identity);
        row.operator_identity = std::move(operator_identity);
        row.exact_decision_identity = selected.semantic_key;
        const auto finish = [&](const VerifiedPolicyExactRowStatus status) {
            row.status = status;
            std::uint64_t transient = sizeof(row);
            saturating_add(
                transient,
                row.exact_entry_identity.capacity() *
                    sizeof(std::uint64_t));
            saturating_add(
                transient,
                row.operator_identity.capacity() *
                    sizeof(std::uint64_t));
            saturating_add(
                transient,
                row.exact_decision_identity.capacity() *
                    sizeof(std::uint64_t));
            saturating_add(
                transient, row.refusal_reason.capacity() + 1);
            saturating_add(
                transient,
                row.transitions.capacity() *
                    sizeof(VerifiedPolicyExactRowTransition));
            for (const VerifiedPolicyExactRowTransition& transition :
                 row.transitions) {
                saturating_add(
                    transient,
                    transition.strict_state_identity.capacity() *
                        sizeof(std::uint64_t));
                saturating_add(
                    transient,
                    transition.exact_item_identity.capacity() *
                        sizeof(std::uint64_t));
            }
            row.retained_bytes = 0;
            row.transient_bytes = transient;
            row.build_ns = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() - started)
                    .count());
        };
        const auto known = known_.find(exact.stable_key);
        if (exact.terminal || known == known_.end() ||
            known->second.strict_state != strict_state ||
            selected.action_id >= strict_->operators().size() ||
            row.exact_entry_identity.empty() ||
            row.operator_identity.empty() ||
            row.exact_decision_identity.empty()) {
            finish(VerifiedPolicyExactRowStatus::InvalidRequest);
            return row;
        }
        const std::uint32_t states_before = strict_->state_count();
        try {
            ExactActionKernel kernel = exact_kernel(exact, selected);
            row.action_cost = kernel.action_cost;
            row.strict_states_created =
                strict_->state_count() - states_before;
            if (!std::isfinite(row.action_cost) ||
                row.action_cost < 0.0 || kernel.transitions.empty()) {
                finish(VerifiedPolicyExactRowStatus::Inapplicable);
                return row;
            }
            row.transitions.reserve(kernel.transitions.size());
            for (const ExactTransition& transition : kernel.transitions) {
                if (!std::isfinite(transition.probability) ||
                    transition.probability <= 0.0) {
                    finish(VerifiedPolicyExactRowStatus::IncompleteMass);
                    return row;
                }
                const auto successor = known_.find(
                    transition.successor.stable_key);
                if (successor == known_.end() ||
                    successor->second.strict_state >=
                        strict_->state_count()) {
                    finish(VerifiedPolicyExactRowStatus::IdentityFailure);
                    return row;
                }
                pc_item_state item{};
                if (!strict_->materialize(
                        successor->second.strict_state, item)) {
                    finish(VerifiedPolicyExactRowStatus::IdentityFailure);
                    return row;
                }
                row.transitions.push_back({
                    transition.successor.stable_key,
                    exact_item_state_key(item),
                    transition.successor.coarse_state,
                    transition.successor.terminal,
                    transition.probability,
                });
                row.probability_mass += transition.probability;
            }
            std::sort(
                row.transitions.begin(), row.transitions.end(),
                [](const VerifiedPolicyExactRowTransition& left,
                   const VerifiedPolicyExactRowTransition& right) {
                    return std::tie(
                               left.exact_item_identity,
                               left.strict_state_identity,
                               left.probability) <
                           std::tie(
                               right.exact_item_identity,
                               right.strict_state_identity,
                               right.probability);
                });
            if (std::abs(row.probability_mass - 1.0) > 1e-12) {
                finish(VerifiedPolicyExactRowStatus::IncompleteMass);
                return row;
            }
            finish(VerifiedPolicyExactRowStatus::Complete);
            strict_->release_solve_transition_caches();
            adapter_memory_cache_initialized_ = false;
            return row;
        } catch (const std::exception& error) {
            row.strict_states_created =
                strict_->state_count() - states_before;
            row.refusal_reason = error.what();
            finish(VerifiedPolicyExactRowStatus::AdapterFailure);
            strict_->release_solve_transition_caches();
            adapter_memory_cache_initialized_ = false;
            return row;
        }
    }

    VerifiedPolicyExactActionRow
    quotient_verified_policy_alternative_exact_row(
            const ExactState& exact,
            const std::uint32_t strict_state,
            const std::uint32_t strict_operator,
            StableKey exact_entry_identity,
            StableKey operator_identity) {
        const auto started = std::chrono::steady_clock::now();
        try {
            const SelectedAction alternative = make_selected(
                exact.coarse_state, strict_operator, strict_state);
            const StableKey raw_key = exact_refinement_state_key(
                strict_->state(strict_state), exact.coarse_state_key);
            const PlannerOperator& planner =
                strict_->operators().at(strict_operator);
            if (raw_key != exact.stable_key &&
                !has_proven_unobserved_primitive_recipe(
                    strict_state, planner)) {
                store_choice_recipe(
                    exact.stable_key, alternative.semantic_key,
                    choice_recipe(raw_key, alternative));
            }
            return quotient_verified_policy_selected_exact_row(
                exact, strict_state, alternative,
                std::move(exact_entry_identity),
                std::move(operator_identity));
        } catch (const std::exception& error) {
            VerifiedPolicyExactActionRow row;
            row.status = VerifiedPolicyExactRowStatus::AdapterFailure;
            row.exact_entry_identity = std::move(exact_entry_identity);
            row.operator_identity = std::move(operator_identity);
            row.refusal_reason = error.what();
            row.transient_bytes = sizeof(row) +
                row.exact_entry_identity.capacity() *
                    sizeof(std::uint64_t) +
                row.operator_identity.capacity() *
                    sizeof(std::uint64_t) +
                row.refusal_reason.capacity() + 1;
            row.build_ns = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() - started)
                    .count());
            return row;
        }
    }

    std::optional<RetentionCapacityFractureShadowRow>
    quotient_retention_capacity_fracture_shadow_row(
            const ExactState& exact,
            const std::uint32_t strict_state,
            const std::uint32_t strict_operator,
            const StableKey& exact_entry_identity,
            const StableKey& action_identity) {
        const auto known = known_.find(exact.stable_key);
        if (exact.terminal || known == known_.end() ||
            known->second.strict_state != strict_state ||
            strict_state >= strict_->state_count() ||
            strict_operator >= strict_->operators().size()) {
            return std::nullopt;
        }
        const PlannerOperator& planner =
            strict_->operators().at(strict_operator);
        if (planner.kind != PlannerOperatorKind::Primitive ||
            planner.primitive_action >=
                strict_->registry().actions.size() ||
            strict_->registry().actions.at(planner.primitive_action)
                    .params.type != ActionType::Fracture) {
            return std::nullopt;
        }
        const AbstractState& carrier = strict_->state(strict_state);
        const std::size_t goal_slots = strict_->layout().slots.size();
        if (goal_slots == 0 || goal_slots >= 32 ||
            strict_->goal().required_satisfied_slots() != goal_slots) {
            return std::nullopt;
        }
        std::uint32_t satisfied = 0;
        for (std::uint32_t slot = 0; slot < goal_slots; ++slot) {
            if (carrier.slot_status[slot] ==
                static_cast<std::uint8_t>(
                    GoalSlotStatus::Satisfied)) {
                satisfied |= std::uint32_t{1} << slot;
            }
        }
        std::uint32_t junk = 0;
        std::uint32_t fractured_junk = 0;
        for (const std::uint8_t count : carrier.junk_counts) junk += count;
        for (const std::uint8_t count :
             carrier.fractured_junk_counts) {
            fractured_junk += count;
        }
        const std::uint32_t all_goals =
            (std::uint32_t{1} << goal_slots) - 1u;
        /* Evidence-selected clean-five shape: all five exact goals plus one
         * ordinary junk affix at full 3/3 capacity, before Fracture. Numeric
         * coarse state ids are intentionally not part of selection. */
        if (goal_slots != 5 || satisfied != all_goals || junk != 1 ||
            carrier.prefix_count != PC_MAX_PREFIXES ||
            carrier.suffix_count != PC_MAX_SUFFIXES ||
            carrier.fractured_goal_mask != 0 || fractured_junk != 0 ||
            carrier.fractured_metamod_flags != 0 ||
            (carrier.flags & kProtectionFlags) != 0) {
            return std::nullopt;
        }
        RetentionCapacityFractureShadowRow row =
            build_retention_capacity_fracture_shadow_row(
                *strict_, strict_state, planner.primitive_action,
                primitive_action_cost(
                    strict_->registry().actions.at(
                        planner.primitive_action)),
                exact_entry_identity, action_identity,
                [&](const std::uint32_t successor) {
                    return project_state_to_coarse(
                        *strict_, successor, strict_state);
                });
        /* The row copied every exact feature/probability needed by the
         * observer. Do not turn diagnostic Fracture rows into a retained
         * parallel transition store. Interned exact successor identities are
         * charged separately as RC-attributable strict-state growth. */
        strict_->release_solve_transition_caches();
        adapter_memory_cache_initialized_ = false;
        return row;
    }

    std::uint32_t quotient_coarse_operator_for_strict(
            const std::uint32_t strict_operator) const {
        return coarse_operator_for(strict_operator);
    }

    const std::vector<std::uint32_t>&
    quotient_complete_operator_vocabulary() const {
        return admitted_operators_;
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
        /* The persistent quotient session re-enters discovery from this
         * collision-checked root after every in-place frontier insertion.
         * Release its transient kernels and recipes above, but retain the one
         * durable replay locator needed to resume the same oracle. */
        if (key != root_key_) known_.erase(key);
        adapter_memory_cache_initialized_ = false;
        adapter_memory_checks_since_audit_ = 0;
        adapter_memory_cached_non_child_bytes_ = 0;
        adapter_memory_projected_growth_bytes_ = 0;
    }

    bool quotient_alternative_operator_admitted(
            const std::uint32_t coarse_state,
            const std::uint32_t strict_state,
            const std::uint32_t operator_index) {
        const std::uint32_t coarse_operator =
            coarse_operator_for(operator_index);
        if (!coarse_.is_candidate_operator_admitted_for_state(
                coarse_state, coarse_operator)) {
            return false;
        }
        const PlannerOperator& planner =
            strict_->operators().at(operator_index);
        if (planner.kind == PlannerOperatorKind::Primitive) {
            const ActionDescriptor& action = strict_->registry()
                .actions.at(planner.primitive_action);
            return action.refinement.complete() &&
                   action_legal(
                       strict_->session(), action,
                       strict_->state(strict_state));
        }
        const double immediate_lower =
            quotient_operator_immediate_cost_lower(operator_index);
        const bool potentially_competitive =
            !std::isfinite(solved_.evaluated_policy_cost) ||
            immediate_lower < solved_.evaluated_policy_cost;
        if (!potentially_competitive) return true;
        const OptionKernel& kernel = strict_->option_kernel(
            strict_state, operator_index);
        if (!kernel.supported || !kernel.legal ||
            !kernel.terminates_almost_surely) {
            return false;
        }
        (void)resource_cost(kernel.expected_resources);
        return true;
    }

    std::tuple<bool, bool, bool, bool, double, std::uint32_t>
    quotient_bootstrap_operator_rank(
            const ExactState& state,
            const std::uint32_t operator_index) const {
        const auto known = known_.find(state.stable_key);
        if (known == known_.end()) {
            throw AdapterFailure(
                PolicyExactLiftStatus::InvalidSolveState,
                "bootstrap rank lost its exact carrier");
        }
        const AbstractState& carrier =
            strict_->state(known->second.strict_state);
        std::uint32_t satisfied_goal_mask = 0;
        for (std::uint32_t slot = 0;
             slot < strict_->layout().slots.size(); ++slot) {
            if (carrier.slot_status[slot] ==
                static_cast<std::uint8_t>(
                    GoalSlotStatus::Satisfied)) {
                satisfied_goal_mask |= std::uint32_t{1} << slot;
            }
        }
        const std::uint32_t missing_goal_mask =
            ((std::uint32_t{1} << strict_->layout().slots.size()) - 1u) &
            ~satisfied_goal_mask;
        const PlannerOperator& planner =
            strict_->operators().at(operator_index);
        bool may_destroy_satisfied_progress = false;
        bool destructive_renewal = false;
        const PlannerOperatorRuntimeSemantics runtime =
            operator_runtime_semantics(planner);
        for (const std::uint32_t action_index :
             runtime.action_dependencies) {
            const ActionPreservationMetadata& preservation =
                strict_->registry().actions.at(action_index)
                    .preservation;
            may_destroy_satisfied_progress =
                may_destroy_satisfied_progress ||
                (preservation.can_destroy &
                 (kCarrierGoalFamilies |
                  kCarrierSatisfiedGoalSubset)) != 0;
            destructive_renewal =
                destructive_renewal ||
                preservation.destructive_renewal;
        }
        return {
            (planner.relevant_goal_mask & missing_goal_mask) == 0,
            planner.automatic_kind == AutomaticCandidateKind::None,
            may_destroy_satisfied_progress,
            destructive_renewal,
            quotient_operator_immediate_cost_lower(operator_index),
            operator_index};
    }

    std::vector<std::uint32_t> quotient_alternative_operator_ids(
            const ExactState& state) {
        if (state.terminal) return {};
        const auto known = known_.find(state.stable_key);
        if (known == known_.end()) {
            throw AdapterFailure(
                PolicyExactLiftStatus::InvalidSolveState,
                "alternative descriptor lost its exact carrier");
        }
        std::vector<std::uint32_t> operator_ids;
        for (const std::uint32_t operator_index : admitted_operators_) {
            if (quotient_alternative_operator_admitted(
                    known->second.coarse_state,
                    known->second.strict_state,
                    operator_index)) {
                operator_ids.push_back(operator_index);
            }
        }
        std::sort(
            operator_ids.begin(), operator_ids.end(),
            [&](const std::uint32_t left,
                const std::uint32_t right) {
                return quotient_bootstrap_operator_rank(state, left) <
                       quotient_bootstrap_operator_rank(state, right);
            });
        operator_ids.erase(
            std::unique(operator_ids.begin(), operator_ids.end()),
            operator_ids.end());
        return operator_ids;
    }

    const QuotientAlternativeDescriptor& quotient_alternative_descriptor(
            const std::uint32_t operator_index) const {
        if (quotient_alternative_descriptor_cache_.size() <
                strict_->operators().size()) {
            quotient_alternative_descriptor_cache_.resize(
                strict_->operators().size());
        }
        std::optional<QuotientAlternativeDescriptor>& cached =
            quotient_alternative_descriptor_cache_.at(operator_index);
        if (cached.has_value()) return *cached;
        const PlannerOperator& planner =
            strict_->operators().at(operator_index);
        SelectedAction runtime;
        runtime.action_id = operator_index;
        populate_ordered_runtime_contracts(runtime, planner);
        runtime.routing_observes =
            observation_requirement_from_selected_action(runtime);
        const StableKey planner_identity =
            planner_operator_semantic_key(planner);
        StableKey pending_identity{
            0x706371616c746431ull,
            operator_index,
            planner_identity.size()};
        pending_identity.insert(
            pending_identity.end(),
            planner_identity.begin(), planner_identity.end());
        StableKey choice_identity{
            0x70637163686f6964ull,
            pending_identity.size()};
        choice_identity.insert(
            choice_identity.end(),
            pending_identity.begin(), pending_identity.end());
        StableKey resumable_identity{
            0x706371726573756dull,
            pending_identity.size()};
        resumable_identity.insert(
            resumable_identity.end(),
            pending_identity.begin(), pending_identity.end());
        QuotientAlternativeDescriptor descriptor;
        descriptor.operator_index = operator_index;
        descriptor.action =
            quotient::canonical_alternative_action_identity({
                operator_index,
                std::move(pending_identity),
                canonical_selected_runtime_contract_identity(runtime),
                std::move(choice_identity)});
        descriptor.routing_observes =
            std::move(runtime.routing_observes);
        descriptor.resumable_work_identity =
            std::move(resumable_identity);
        cached = std::move(descriptor);
        adapter_memory_cache_initialized_ = false;
        return *cached;
    }

    const std::string& quotient_operator_id(
            const std::uint32_t operator_index) const {
        return strict_->operators().at(operator_index).id;
    }

    std::vector<QuotientAlternativeDescriptor>
    quotient_alternative_descriptors(const ExactState& state) {
        std::vector<QuotientAlternativeDescriptor> descriptors;
        for (const std::uint32_t operator_index :
             quotient_alternative_operator_ids(state)) {
            descriptors.push_back(
                quotient_alternative_descriptor(operator_index));
        }
        return descriptors;
    }

    StableKey quotient_price_identity() const {
        std::map<std::string, double> ordered(
            prices_.begin(), prices_.end());
        StableKey identity{0x7063717072696365ull, ordered.size()};
        for (const auto& [key, price] : ordered) {
            identity.push_back(key.size());
            for (const unsigned char value : key) {
                identity.push_back(value);
            }
            identity.push_back(std::bit_cast<std::uint64_t>(price));
        }
        return identity;
    }

    StableKey quotient_vocabulary_identity() const {
        StableKey identity{
            0x706371766f636162ull,
            admitted_operators_.size()};
        for (const std::uint32_t operator_index : admitted_operators_) {
            const StableKey planner = planner_operator_semantic_key(
                strict_->operators().at(operator_index));
            identity.push_back(operator_index);
            identity.push_back(planner.size());
            identity.insert(
                identity.end(), planner.begin(), planner.end());
        }
        return identity;
    }

    double quotient_operator_immediate_cost_lower(
            const std::uint32_t operator_index) const {
        const PlannerOperator& planner =
            strict_->operators().at(operator_index);
        double lower = 0.0;
        for (const auto& [key, quantity] :
             planner.resource_quantities) {
            const auto price = prices_.find(key);
            if (price == prices_.end() ||
                !std::isfinite(price->second) || price->second < 0.0 ||
                !std::isfinite(quantity) || quantity < 0.0) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::MissingPrice,
                    "alternative operator has a missing or invalid "
                    "mandatory resource price for '" + key + "'");
            }
            if (price->second == 0.0 || quantity == 0.0) continue;
            double part = quantity * price->second;
            if (!std::isfinite(part)) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::MissingPrice,
                    "alternative operator has a non-finite mandatory "
                    "resource price");
            }
            part = std::nextafter(
                part, -std::numeric_limits<double>::infinity());
            const double sum = lower + part;
            if (!std::isfinite(sum)) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::MissingPrice,
                    "alternative operator has a non-finite mandatory "
                    "resource total");
            }
            lower = std::nextafter(
                sum, -std::numeric_limits<double>::infinity());
            lower = std::max(0.0, lower);
        }
        return lower;
    }

    void record_broad_row_attribution(
            const ExactState& state,
            const SelectedAction& selected,
            const bool alternative,
            const std::size_t sample_begin,
            const std::uint64_t raw_transitions,
            const std::size_t sample_end =
                std::numeric_limits<std::size_t>::max()) {
        const auto known = known_.find(state.stable_key);
        if (known == known_.end() ||
            selected.action_id >= strict_->operators().size()) {
            return;
        }
        const auto& samples =
            strict_->telemetry().reforge_build_attribution_samples;
        if (sample_begin >= samples.size()) return;
        const PlannerOperator& planner =
            strict_->operators()[selected.action_id];
        std::vector<std::string> program;
        const auto append_action = [&](const std::string& action_id) {
            if (!action_id.empty()) program.push_back(action_id);
        };
        append_action(planner.primitive_action_id);
        for (const std::string& action_id :
             planner.primitive_program_action_ids) {
            append_action(action_id);
        }
        append_action(planner.conditional_action_id);
        append_action(planner.bestiary_create_action_id);
        append_action(planner.bestiary_restore_action_id);
        append_action(planner.setup_action_id);
        append_action(planner.followup_action_id);
        append_action(planner.cleanup_action_id);
        std::sort(program.begin(), program.end());
        program.erase(
            std::unique(program.begin(), program.end()),
            program.end());
        const std::size_t bounded_end = std::min(
            samples.size(), sample_end);
        for (std::size_t index = sample_begin;
             index < bounded_end; ++index) {
            constexpr std::size_t kSampleLimit = 16;
            if (telemetry_.broad_row_attribution.size() >=
                kSampleLimit) {
                ++telemetry_.broad_row_attribution_omitted;
                continue;
            }
            PolicyBroadRowAttribution sample;
            sample.alternative = alternative;
            sample.row_sequence = alternative
                                      ? telemetry_.alternative_rows_begun
                                      : telemetry_.selected_rows_begun;
            sample.source_strict_state = known->second.strict_state;
            sample.source_coarse_state = state.coarse_state;
            std::uint64_t source_hash = 1469598103934665603ull;
            for (const std::uint64_t token : state.stable_key) {
                source_hash ^= token;
                source_hash *= 1099511628211ull;
            }
            sample.source_stable_key_hash = source_hash;
            sample.operator_index = selected.action_id;
            sample.planner_id = planner.id;
            sample.primitive_program_action_ids = program;
            sample.observation_item_features =
                selected.routing_observes.item_features;
            sample.observation_modifier_tag_ids =
                selected.routing_observes.modifier_tag_ids;
            sample.observation_affix_selectors =
                selected.routing_observes.affix_observations.size();
            sample.raw_transitions = raw_transitions;
            sample.reforge = samples[index];
            telemetry_.broad_row_attribution.push_back(
                std::move(sample));
        }
    }

    QuotientOracleCompactRow quotient_build_compact_row(
            const ExactState& state,
            SelectedAction selected,
            const bool alternative,
            const std::size_t attribution_sample_begin) {
        if (telemetry_.strict_kernels_built >=
            limits_.max_exact_kernels) {
            throw AdapterFailure(
                PolicyExactLiftStatus::ResourceCap,
                "strict policy discovery reached max_exact_kernels",
                "max_exact_kernels");
        }
        saturating_add(
            alternative
                ? telemetry_.alternative_rows_begun
                : telemetry_.selected_rows_begun,
            1);
        const std::uint64_t work_before =
            strict_->telemetry().reforge_frontier_work;
        const auto attribute_work = [&] {
            const std::uint64_t after =
                strict_->telemetry().reforge_frontier_work;
            saturating_add(
                alternative
                    ? telemetry_.alternative_reforge_work
                    : telemetry_.selected_reforge_work,
                after >= work_before ? after - work_before : 0);
            telemetry_.strict_reforge_work = after;
        };
        QuotientOracleCompactRow row;
        try {
            row.selected = std::move(selected);
            row.choice_recipe = choice_recipe(
                state.stable_key, row.selected);
            row.action_cost = visit_kernel_transitions(
                state.stable_key, row.selected,
                [&](const std::uint32_t successor,
                    const double probability) {
                    row.transitions.push_back({successor, probability});
                });
        } catch (...) {
            attribute_work();
            record_broad_row_attribution(
                state, row.selected, alternative,
                attribution_sample_begin,
                row.transitions.size());
            throw;
        }
        attribute_work();
        record_broad_row_attribution(
            state, row.selected, alternative,
            attribution_sample_begin,
            row.transitions.size());
        if (telemetry_.strict_transitions_built + row.transitions.size() >
            limits_.max_transitions) {
            throw AdapterFailure(
                PolicyExactLiftStatus::ResourceCap,
                "strict policy discovery reached max_transitions",
                "max_transitions");
        }
        telemetry_.strict_transitions_built += row.transitions.size();
        ++telemetry_.strict_kernels_built;
        saturating_add(
            alternative
                ? telemetry_.alternative_rows_completed
                : telemetry_.selected_rows_completed,
            1);
        saturating_add(
            alternative
                ? telemetry_.alternative_transitions
                : telemetry_.selected_transitions,
            row.transitions.size());
        telemetry_.strict_calc_owned_bytes =
            strict_->fast_estimated_owned_bytes();
        release_published_kernel_storage(state, row.selected);
        return row;
    }

    solve_detail::CooperativeTask<
        std::shared_ptr<const OutcomeDistribution>>
    quotient_prepare_primitive_row(
            const ExactState& state,
            const std::uint32_t operator_index) {
        const auto known = known_.find(state.stable_key);
        if (known == known_.end() ||
            operator_index >= strict_->operators().size()) {
            co_return nullptr;
        }
        const PlannerOperator& planner =
            strict_->operators().at(operator_index);
        if (planner.kind != PlannerOperatorKind::Primitive) {
            co_return nullptr;
        }
        const std::uint32_t action = planner.primitive_action;
        if (!action_transition_facts(
                strict_->registry().actions.at(action).params.type)
                 .renewal) {
            co_return nullptr;
        }
        struct CursorGuard {
            CalcContext* calc = nullptr;
            bool complete = false;
            ~CursorGuard() {
                if (calc != nullptr && !complete) calc->cancel_outcomes();
            }
        } guard{strict_.get(), false};
        std::shared_ptr<const OutcomeDistribution> completed;
        while (!strict_->advance_outcomes(
            known->second.strict_state, action,
            options_.goal_progress_gated_reforges, completed, 1)) {
            std::uint64_t retained = estimated_owned_bytes();
            saturating_add(retained, strict_->outcome_cursor_bytes());
            co_await solve_detail::CooperativeCheckpoint{retained};
        }
        guard.complete = true;
        co_return completed;
    }

    solve_detail::CooperativeTask<
        std::optional<QuotientOracleCompactRow>>
    quotient_certify_alternative_descriptor_cooperatively(
            const ExactState& state,
            const QuotientAlternativeDescriptor& descriptor) {
        ScopedReforgeRowProvenance provenance(
            *strict_, ReforgeRowOwner::StrictAlternative);
        const std::size_t attribution_sample_begin =
            strict_->telemetry()
                .reforge_build_attribution_samples.size();
        const std::uint64_t work_before =
            strict_->telemetry().reforge_frontier_work;
        auto preparation = quotient_prepare_primitive_row(
            state, descriptor.operator_index);
        while (!preparation.resume()) {
            co_await solve_detail::CooperativeCheckpoint{
                preparation.retained_bytes()};
        }
        const std::shared_ptr<const OutcomeDistribution> prepared =
            preparation.take_result();
        const std::size_t attribution_sample_end =
            strict_->telemetry()
                .reforge_build_attribution_samples.size();
        const std::uint64_t work_after =
            strict_->telemetry().reforge_frontier_work;
        saturating_add(
            telemetry_.alternative_reforge_work,
            work_after >= work_before ? work_after - work_before : 0);
        telemetry_.strict_reforge_work = work_after;
        std::optional<QuotientOracleCompactRow> row =
            quotient_certify_alternative_descriptor(state, descriptor);
        if (row.has_value()) {
            record_broad_row_attribution(
                state, row->selected, true,
                attribution_sample_begin,
                prepared == nullptr ? row->transitions.size()
                                    : prepared->entries.size(),
                attribution_sample_end);
            provenance.publish();
        }
        co_return row;
    }

    std::optional<QuotientOracleCompactRow>
    quotient_certify_alternative_descriptor(
            const ExactState& state,
            const QuotientAlternativeDescriptor& descriptor) {
        const auto known = known_.find(state.stable_key);
        const bool structural_match =
            !state.terminal &&
            known != known_.end() &&
            descriptor.operator_index < strict_->operators().size() &&
            std::binary_search(
                admitted_operators_.begin(),
                admitted_operators_.end(),
                descriptor.operator_index) &&
            quotient_alternative_descriptor(
                descriptor.operator_index) == descriptor;
        /* Alternative discovery already enumerated the complete vocabulary
         * to create the obligation. Certification needs to prove only that
         * this descriptor remains admitted on this concrete carrier. Do not
         * rebuild every unrelated option kernel before immediately checking
         * the selected candidate again below. */
        if (!structural_match ||
            !quotient_alternative_operator_admitted(
                known->second.coarse_state,
                known->second.strict_state,
                descriptor.operator_index)) {
            throw AdapterFailure(
                PolicyExactLiftStatus::InvalidSolveState,
                "alternative descriptor is stale or outside admission");
        }
        ScopedReforgeRowProvenance provenance(
            *strict_, ReforgeRowOwner::StrictAlternative);
        saturating_add(
            telemetry_.local_state_action_rows_scheduled, 1);
        const std::size_t attribution_sample_begin =
            strict_->telemetry()
                .reforge_build_attribution_samples.size();
        const std::uint64_t work_before =
            strict_->telemetry().reforge_frontier_work;
        std::optional<SelectedAction> candidate;
        try {
            candidate = candidate_selection(
                state.stable_key, descriptor.operator_index, 0);
        } catch (...) {
            const std::uint64_t after =
                strict_->telemetry().reforge_frontier_work;
            saturating_add(
                telemetry_.alternative_reforge_work,
                after >= work_before ? after - work_before : 0);
            telemetry_.strict_reforge_work = after;
            throw;
        }
        const std::uint64_t after =
            strict_->telemetry().reforge_frontier_work;
        saturating_add(
            telemetry_.alternative_reforge_work,
            after >= work_before ? after - work_before : 0);
        telemetry_.strict_reforge_work = after;
        if (!candidate.has_value()) return std::nullopt;
        try {
            QuotientOracleCompactRow row = quotient_build_compact_row(
                state, std::move(*candidate), true,
                attribution_sample_begin);
            saturating_add(
                telemetry_.local_state_action_rows_evaluated, 1);
            provenance.publish();
            return row;
        } catch (const AdapterFailure& error) {
            if (error.status == PolicyExactLiftStatus::ResourceCap) throw;
            return std::nullopt;
        }
    }

    QuotientOracleCompactRow quotient_selected_compact_row(
            const ExactState& state) {
        if (state.terminal) {
            throw AdapterFailure(
                PolicyExactLiftStatus::InvalidSolveState,
                "terminal carrier has no selected quotient row");
        }
        try {
            ScopedReforgeRowProvenance provenance(
                *strict_, ReforgeRowOwner::StrictSelected);
            const std::size_t attribution_sample_begin =
                strict_->telemetry()
                    .reforge_build_attribution_samples.size();
            const std::uint64_t work_before =
                strict_->telemetry().reforge_frontier_work;
            std::optional<SelectedAction> current;
            if (!quotient_requires_bootstrap(state)) {
                try {
                    current = selected_action(state);
                } catch (...) {
                    const std::uint64_t after =
                        strict_->telemetry().reforge_frontier_work;
                    saturating_add(
                        telemetry_.selected_reforge_work,
                        after >= work_before ? after - work_before : 0);
                    telemetry_.strict_reforge_work = after;
                    throw;
                }
            }
            const std::uint64_t after =
                strict_->telemetry().reforge_frontier_work;
            saturating_add(
                telemetry_.selected_reforge_work,
                after >= work_before ? after - work_before : 0);
            telemetry_.strict_reforge_work = after;
            if (current.has_value()) {
                QuotientOracleCompactRow row = quotient_build_compact_row(
                    state, std::move(*current), false,
                    attribution_sample_begin);
                provenance.publish();
                return row;
            }
        } catch (const AdapterFailure& error) {
            if (error.status == PolicyExactLiftStatus::ResourceCap) throw;
        }
        const std::vector<QuotientAlternativeDescriptor> alternatives =
            quotient_alternative_descriptors(state);
        for (const QuotientAlternativeDescriptor& alternative :
             alternatives) {
            std::optional<QuotientOracleCompactRow> row =
                quotient_certify_alternative_descriptor(
                    state, alternative);
            if (row.has_value()) return std::move(*row);
        }
        throw AdapterFailure(
            PolicyExactLiftStatus::LocalReoptimizationRequired,
            "proof-carrying quotient carrier has no certified admitted row");
    }

    solve_detail::CooperativeTask<QuotientOracleCompactRow>
    quotient_selected_compact_row_cooperatively(
            const ExactState& state) {
        if (!state.terminal && !quotient_requires_bootstrap(state)) {
            const std::optional<SelectedAction> current =
                selected_action(state);
            if (current.has_value()) {
                ScopedReforgeRowProvenance provenance(
                    *strict_, ReforgeRowOwner::StrictSelected);
                const std::size_t attribution_sample_begin =
                    strict_->telemetry()
                        .reforge_build_attribution_samples.size();
                const std::uint64_t work_before =
                    strict_->telemetry().reforge_frontier_work;
                auto preparation = quotient_prepare_primitive_row(
                    state, current->action_id);
                while (!preparation.resume()) {
                    co_await solve_detail::CooperativeCheckpoint{
                        preparation.retained_bytes()};
                }
                const std::shared_ptr<const OutcomeDistribution> prepared =
                    preparation.take_result();
                const std::size_t attribution_sample_end =
                    strict_->telemetry()
                        .reforge_build_attribution_samples.size();
                const std::uint64_t work_after =
                    strict_->telemetry().reforge_frontier_work;
                saturating_add(
                    telemetry_.selected_reforge_work,
                    work_after >= work_before
                        ? work_after - work_before
                        : 0);
                telemetry_.strict_reforge_work = work_after;
                QuotientOracleCompactRow row =
                    quotient_selected_compact_row(state);
                record_broad_row_attribution(
                    state, row.selected, false,
                    attribution_sample_begin,
                    prepared == nullptr ? row.transitions.size()
                                        : prepared->entries.size(),
                    attribution_sample_end);
                provenance.publish();
                co_return row;
            }
        }
        co_return quotient_selected_compact_row(state);
    }

    solve_detail::CooperativeTask<QuotientOracleCompactRow>
    boundary_selected_compact_row_cooperatively(
            const ExactState& state) {
        if (state.terminal) {
            throw AdapterFailure(
                PolicyExactLiftStatus::InvalidSolveState,
                "boundary observation stop has no selected row");
        }
        const std::optional<SelectedAction> current = selected_action(state);
        if (!current.has_value()) {
            throw AdapterFailure(
                PolicyExactLiftStatus::InvalidSolveState,
                "boundary prefix carrier has no selected row");
        }
        ScopedReforgeRowProvenance provenance(
            *strict_, ReforgeRowOwner::StrictSelected);
        const std::size_t attribution_sample_begin =
            strict_->telemetry().reforge_build_attribution_samples.size();
        const std::uint64_t work_before =
            strict_->telemetry().reforge_frontier_work;
        auto preparation = quotient_prepare_primitive_row(
            state, current->action_id);
        while (!preparation.resume()) {
            co_await solve_detail::CooperativeCheckpoint{
                preparation.retained_bytes()};
        }
        const std::shared_ptr<const OutcomeDistribution> prepared =
            preparation.take_result();
        const std::size_t attribution_sample_end =
            strict_->telemetry().reforge_build_attribution_samples.size();
        const std::uint64_t work_after =
            strict_->telemetry().reforge_frontier_work;
        saturating_add(
            telemetry_.selected_reforge_work,
            work_after >= work_before ? work_after - work_before : 0);
        telemetry_.strict_reforge_work = work_after;
        QuotientOracleCompactRow row = quotient_build_compact_row(
            state, *current, false, attribution_sample_begin);
        record_broad_row_attribution(
            state, row.selected, false, attribution_sample_begin,
            prepared == nullptr ? row.transitions.size()
                                : prepared->entries.size(),
            attribution_sample_end);
        provenance.publish();
        co_return row;
    }

    std::optional<StableKey> quotient_selected_compact_row_identity(
            const ExactState& state) {
        if (state.terminal) return std::nullopt;
        if (quotient_requires_bootstrap(state)) return std::nullopt;
        std::optional<SelectedAction> selected;
        try {
            selected = selected_action(state);
        } catch (const AdapterFailure& error) {
            if (error.status == PolicyExactLiftStatus::ResourceCap) throw;
            /* Reuse is only an optimization. A strict successor can project
             * to an unselected coarse parent during witness-driven local
             * reoptimization; the compact-row builder below owns the
             * certified admitted-row fallback for that carrier. */
            return std::nullopt;
        }
        if (!selected.has_value()) return std::nullopt;
        std::optional<StableKey> identity =
            exact_kernel_reuse_key(state, *selected);
        if (!identity.has_value()) return std::nullopt;
        const StableKey runtime =
            canonical_selected_runtime_contract_identity(*selected);
        identity->push_back(selected->semantic_key.size());
        identity->insert(
            identity->end(), selected->semantic_key.begin(),
            selected->semantic_key.end());
        identity->push_back(runtime.size());
        identity->insert(
            identity->end(), runtime.begin(), runtime.end());
        release_published_kernel_storage(state, *selected);
        return identity;
    }

    solve_detail::CooperativeTask<std::optional<StableKey>>
    quotient_selected_compact_row_identity_cooperatively(
            const ExactState& state) {
        if (state.terminal || quotient_requires_bootstrap(state)) {
            co_return std::nullopt;
        }
        std::optional<SelectedAction> selected;
        try {
            selected = selected_action(state);
        } catch (const AdapterFailure& error) {
            if (error.status == PolicyExactLiftStatus::ResourceCap) throw;
            co_return std::nullopt;
        }
        if (!selected.has_value()) co_return std::nullopt;
        ScopedReforgeRowProvenance provenance(
            *strict_, ReforgeRowOwner::StrictSelected);
        const std::size_t attribution_sample_begin =
            strict_->telemetry()
                .reforge_build_attribution_samples.size();
        const std::uint64_t work_before =
            strict_->telemetry().reforge_frontier_work;
        auto preparation = quotient_prepare_primitive_row(
            state, selected->action_id);
        while (!preparation.resume()) {
            co_await solve_detail::CooperativeCheckpoint{
                preparation.retained_bytes()};
        }
        const std::shared_ptr<const OutcomeDistribution> prepared =
            preparation.take_result();
        const std::size_t attribution_sample_end =
            strict_->telemetry()
                .reforge_build_attribution_samples.size();
        const std::uint64_t work_after =
            strict_->telemetry().reforge_frontier_work;
        saturating_add(
            telemetry_.selected_reforge_work,
            work_after >= work_before ? work_after - work_before : 0);
        telemetry_.strict_reforge_work = work_after;
        const std::optional<StableKey> identity =
            quotient_selected_compact_row_identity(state);
        if (identity.has_value()) {
            record_broad_row_attribution(
                state, *selected, false,
                attribution_sample_begin,
                prepared == nullptr ? 0 : prepared->entries.size(),
                attribution_sample_end);
            provenance.publish();
        }
        co_return identity;
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
        bool bootstrap_seeded = false;
        try {
            ScopedReforgeRowProvenance provenance(
                *strict_, ReforgeRowOwner::StrictSelected);
            std::optional<SelectedAction> current;
            if (!quotient_requires_bootstrap(state)) {
                current = selected_action(state);
            }
            if (current.has_value()) {
                ExactActionKernel kernel = exact_kernel(state, *current);
                append(std::move(*current), std::move(kernel));
                current_complete = true;
                provenance.publish();
            }
        } catch (const AdapterFailure& error) {
            if (error.status == PolicyExactLiftStatus::ResourceCap) throw;
        }
        /* A strict successor can map to a coarse state that the broad anytime
         * solve discovered without publishing an action. Local repair still
         * needs exactly one selected seed before Bellman can establish a
         * proper upper. Pick the first carrier-ranked admitted operator and
         * leave every other operator to the lazy alternative-obligation
         * scheduler. Materializing all of them here would falsely publish
         * multiple selected rows. */
        if (!current_complete &&
            (enable_local_reoptimization_ ||
             enable_quotient_bootstrap_)) {
            std::vector<std::uint32_t> bootstrap_operators =
                admitted_operators_;
            std::sort(
                bootstrap_operators.begin(),
                bootstrap_operators.end(),
                [&](const std::uint32_t left,
                    const std::uint32_t right) {
                    return quotient_bootstrap_operator_rank(state, left) <
                           quotient_bootstrap_operator_rank(state, right);
                });
            for (const std::uint32_t operator_index :
                 bootstrap_operators) {
                ScopedReforgeRowProvenance provenance(
                    *strict_, ReforgeRowOwner::StrictSelected);
                saturating_add(
                    telemetry_.local_state_action_rows_scheduled, 1);
                std::optional<SelectedAction> candidate =
                    candidate_selection(
                        state.stable_key, operator_index, 0);
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
                provenance.publish();
                current_complete = true;
                bootstrap_seeded = true;
                break;
            }
        }
        const bool affected =
            enable_local_reoptimization_ &&
            reoptimization_parents_.contains(state.coarse_state);
        if ((affected && !bootstrap_seeded) || !current_complete) {
            for (const std::uint32_t operator_index : admitted_operators_) {
                ScopedReforgeRowProvenance provenance(
                    *strict_, ReforgeRowOwner::StrictAlternative);
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
                provenance.publish();
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

    struct PreparedLiftedPolicyAssertion {
        SolveOptions options;
        SolveResult lifted;
        RefinedPolicyCompileRouting routing;
        CompiledPolicyAssertion immediate;
        std::uint64_t projected_construction_live = 0;
        std::uint64_t required_before_lift = 0;
        std::uint64_t refined_routing_bytes = 0;
        bool ready = false;
    };

    PreparedLiftedPolicyAssertion prepare_lifted_policy_assertion(
            const RefinementResult& refinement,
            const PolicyEvaluationResult& evaluation) {
        PreparedLiftedPolicyAssertion prepared;
        prepared.options = options_;
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
        prepared.projected_construction_live = coarse_bytes;
        const auto add_projected = [&](const std::uint64_t bytes) {
            saturating_add(
                prepared.projected_construction_live, bytes);
        };
        add_projected(adapter_bytes);
        add_projected(refinement_bytes);
        add_projected(evaluation_bytes);
        add_projected(projected_lift_bytes);
        if (prepared.projected_construction_live >=
            prepared.options.max_solver_owned_bytes) {
            prepared.immediate.status =
                CompiledPolicyAssertionStatus::ResourceCap;
            prepared.immediate.resource_cap =
                "max_solver_owned_bytes";
            prepared.immediate.failure_reason =
                "projected lifted policy construction reached "
                "max_solver_owned_bytes";
            prepared.immediate.publication_peak_owned_bytes =
                prepared.projected_construction_live;
            return prepared;
        }
        prepared.required_before_lift = coarse_bytes;
        saturating_add(
            prepared.required_before_lift, adapter_cache_bytes);
        saturating_add(
            prepared.required_before_lift, refinement_bytes);
        saturating_add(
            prepared.required_before_lift, evaluation_bytes);
        if (prepared.required_before_lift >=
            prepared.options.max_solver_owned_bytes) {
            prepared.immediate.status =
                CompiledPolicyAssertionStatus::ResourceCap;
            prepared.immediate.resource_cap =
                "max_solver_owned_bytes";
            prepared.immediate.failure_reason =
                "live coarse/refinement state leaves no memory for the "
                "lifted strict artifact";
            prepared.immediate.publication_peak_owned_bytes =
                prepared.required_before_lift;
            return prepared;
        }
        prepared.options.max_solver_owned_bytes -=
            prepared.required_before_lift;
        const bool streamed = std::any_of(
            refinement.classes.begin(), refinement.classes.end(),
            [](const RefinedPolicyClass& cls) {
                return !cls.strict_members.empty();
            });
        prepared.lifted = streamed
            ? make_streamed_lifted_solve_result(
                  refinement, evaluation, &prepared.routing)
            : make_lifted_solve_result(
                  refinement, evaluation, &prepared.routing);
        prepared.refined_routing_bytes =
            refined_compile_routing_bytes(prepared.routing);
        if (prepared.refined_routing_bytes >=
            prepared.options.max_solver_owned_bytes) {
            prepared.immediate.status =
                CompiledPolicyAssertionStatus::ResourceCap;
            prepared.immediate.resource_cap =
                "max_solver_owned_bytes";
            prepared.immediate.failure_reason =
                "refined compile routing leaves no memory for the "
                "compiled exact artifact";
            prepared.immediate.publication_peak_owned_bytes =
                prepared.required_before_lift;
            saturating_add(
                prepared.immediate.publication_peak_owned_bytes,
                prepared.refined_routing_bytes);
            return prepared;
        }
        prepared.options.max_solver_owned_bytes -=
            prepared.refined_routing_bytes;
        prepared.options.max_reforge_work = options_.max_reforge_work;
        strict_->set_solve_resource_caps(
            std::max<std::uint32_t>(1, limits_.max_exact_states),
            prepared.options.max_reforge_work,
            false,
            prepared.options.max_solver_owned_bytes);
        strict_->refresh_solve_owned_bytes_cap(
            prepared.options.max_solver_owned_bytes);
        prepared.ready = true;
        return prepared;
    }

    CompiledPolicyAssertion finish_lifted_policy_assertion(
            PreparedLiftedPolicyAssertion& prepared,
            CompiledPolicyAssertion compiled,
            const RefinementResult& refinement,
            const PolicyEvaluationResult& evaluation) {
        std::uint64_t publication_compile_peak =
            prepared.required_before_lift;
        saturating_add(
            publication_compile_peak,
            prepared.refined_routing_bytes);
        saturating_add(
            publication_compile_peak,
            compiled.publication_peak_owned_bytes);
        compiled.publication_peak_owned_bytes = std::max(
            prepared.projected_construction_live,
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
                        CompiledPolicyAssertionStatus::CompilationFailure;
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
                    CompiledPolicyAssertionStatus::CompilationFailure;
                compiled.executable = false;
                compiled.failure_reason =
                    "compiled exact quotient working-state count does "
                    "not match reachable non-terminal refinement classes";
            }
        }
        return compiled;
    }

    CompiledPolicyAssertionWork begin_lifted_policy_assertion(
            PreparedLiftedPolicyAssertion& prepared,
            std::string strategy_name) {
        if (!prepared.ready) {
            throw std::logic_error(
                "unready lifted assertion entered cooperative evaluator");
        }
        return CompiledPolicyAssertionWork(
            *strict_, prepared.lifted, prices_, prepared.options,
            std::move(strategy_name), &prepared.routing);
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
        /* Strict carrier discovery and independent evaluation of the emitted
         * graph are separate proof stages. The public work ceiling applies
         * independently to each stage; their individual ledgers remain
         * visible in the lift and evaluator telemetry. */
        remaining_options.max_reforge_work =
            options_.max_reforge_work;
        /* Strict discovery temporarily narrows the child CalcContext budget
         * while partition scratch is live.  That scratch has been released
         * before artifact construction, and required_before_lift plus the
         * refined routing above have already debited every still-live
         * external owner.  Refresh the child to exactly that accounted
         * remainder so a stale earlier peak cannot masquerade as the public
         * max_solver_owned_bytes cap. */
        strict_->set_solve_resource_caps(
            std::max<std::uint32_t>(1, limits_.max_exact_states),
            remaining_options.max_reforge_work,
            false,
            remaining_options.max_solver_owned_bytes);
        strict_->refresh_solve_owned_bytes_cap(
            remaining_options.max_solver_owned_bytes);
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
