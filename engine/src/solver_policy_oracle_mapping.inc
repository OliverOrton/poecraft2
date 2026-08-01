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
