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
