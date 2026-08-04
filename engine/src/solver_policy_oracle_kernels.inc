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

    void sync_strict_reforge_resource_telemetry() const noexcept {
        if (strict_ == nullptr) return;
        const CalcTelemetry& strict = strict_->telemetry();
        telemetry_.strict_reforge_work = strict.reforge_frontier_work;
        telemetry_.strict_reforge_logical_work_v1 =
            strict.reforge_raw_equivalent_work;
        telemetry_.strict_reforge_evaluator_work_v2 =
            strict.reforge_projected_work;
        telemetry_.strict_reforge_evaluator_work_v3 =
            strict.reforge_factored_work;
        telemetry_.strict_reforge_effort = strict.reforge_effort;
        for (ReforgeRowTelemetry& retained :
             telemetry_.strict_reforge_row_samples) {
            const auto current = std::find_if(
                strict.reforge_row_samples.begin(),
                strict.reforge_row_samples.end(),
                [&](const ReforgeRowTelemetry& row) {
                    return row.sequence == retained.sequence;
                });
            if (current != strict.reforge_row_samples.end()) {
                retained = *current;
            }
        }
        constexpr std::size_t kSampleLimit = 64;
        while (strict_reforge_sample_cursor_ <
               strict.reforge_row_samples.size()) {
            if (telemetry_.strict_reforge_row_samples.size() >=
                kSampleLimit) {
                saturating_add(
                    strict_reforge_sample_copy_omitted_, 1);
                ++strict_reforge_sample_cursor_;
                continue;
            }
            try {
                telemetry_.strict_reforge_row_samples.push_back(
                    strict.reforge_row_samples[
                        strict_reforge_sample_cursor_]);
            } catch (...) {
                saturating_add(
                    strict_reforge_sample_copy_omitted_, 1);
            }
            ++strict_reforge_sample_cursor_;
        }
        telemetry_.strict_reforge_row_samples_omitted =
            strict.reforge_row_samples_omitted;
        saturating_add(
            telemetry_.strict_reforge_row_samples_omitted,
            strict_reforge_sample_copy_omitted_);
    }

    std::uint64_t dynamic_child_owned_bytes() const {
        sync_strict_reforge_resource_telemetry();
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
        saturating_add(
            bytes,
            telemetry_.strict_reforge_row_samples.capacity() *
                sizeof(ReforgeRowTelemetry));
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
    bool force_semantic_strict_identity_ = false;
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
    std::map<StableKey, ExactChoiceRecipe> quotient_streamed_recipes_;
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
    mutable std::size_t strict_reforge_sample_cursor_ = 0;
    mutable std::uint64_t strict_reforge_sample_copy_omitted_ = 0;

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
        if (enable_local_reoptimization_ ||
            force_semantic_strict_identity_) return std::nullopt;
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
