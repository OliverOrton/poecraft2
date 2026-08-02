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

    template <typename AddTransition>
    double visit_kernel_transitions(
            const StableKey& key,
            const SelectedAction& selected,
            AddTransition&& add) {
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
        double action_cost = 0.0;
        std::uint64_t transition_count = 0;
        const auto append = [&](const std::uint32_t successor,
                                const double probability) {
            if (probability == 0.0) return;
            add(
                successor == kNoId ? known.strict_state : successor,
                probability);
            ++transition_count;
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
            action_cost = primitive_action_cost(descriptor);
            if (distribution.choice_groups.empty()) {
                for (const OutcomeEntry& entry :
                     distribution.entries) {
                    append(entry.state, entry.probability);
                }
            } else {
                for (const OutcomeChoiceGroup& group :
                     distribution.choice_groups) {
                    append(
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
            action_cost = resource_cost(option.expected_resources);
            for (const OutcomeEntry& exit : option.exits) {
                append(exit.state, exit.probability);
            }
            for (const OutcomeChoiceGroup& group :
                  option.observation_choice_groups) {
                const std::uint32_t chosen = resolve_option_choice(
                    recipe.option_preferences,
                    known.strict_state,
                    group, option);
                append(chosen, group.probability);
            }
        }
        if (transition_count == 0) {
            throw AdapterFailure(
                PolicyExactLiftStatus::UnsupportedPrimitiveKernel,
                "selected planner operator produced an empty strict "
                "kernel");
        }
        return action_cost;
    }

    ExactActionKernel build_kernel(
            const StableKey& key,
            const SelectedAction& selected) {
        ExactActionKernel kernel;
        kernel.action_cost = visit_kernel_transitions(
            key, selected,
            [&](const std::uint32_t successor,
                const double probability) {
                auto [exact, inserted] = intern_strict_state(successor);
                (void)inserted;
                kernel.transitions.push_back(
                    {std::move(exact), probability});
            });
        return kernel;
    }
