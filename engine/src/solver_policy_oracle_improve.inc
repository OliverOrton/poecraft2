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
            graph, owner,
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
                            if (solve_detail::
                                    sparse_policy_value_improvement_exceeds_tolerance(
                                        trial.root_value(),
                                        incumbent.root_value(),
                                        tolerance)) {
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
    const std::uint64_t prior_reforge_work = total.strict_reforge_work;
    std::uint64_t prior_reforge_row_sequence =
        total.strict_reforge_effort.rows_begun;
    saturating_add(
        prior_reforge_row_sequence,
        total.strict_reforge_effort.rows_cache_reused);
    const std::uint64_t prior_alternative_rows =
        total.alternative_rows_completed;
    const auto add_u32 =
        [](std::uint32_t& target,
           const std::uint32_t addition) {
            target = bounded_u32(
                static_cast<std::uint64_t>(target) +
                addition);
        };
    saturating_add(
        total.strict_session_constructions,
        pass.strict_session_constructions);
    saturating_add(
        total.strict_full_restarts,
        pass.strict_full_restarts);
    saturating_add(
        total.strict_partition_updates,
        pass.strict_partition_updates);
    saturating_add(
        total.strict_frontier_insertions,
        pass.strict_frontier_insertions);
    saturating_add(
        total.strict_frontier_states_inserted,
        pass.strict_frontier_states_inserted);
    total.strict_frontier_update_max_ns = std::max(
        total.strict_frontier_update_max_ns,
        pass.strict_frontier_update_max_ns);
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
    saturating_add(
        total.strict_reforge_logical_work_v1,
        pass.strict_reforge_logical_work_v1);
    saturating_add(
        total.strict_reforge_evaluator_work_v1,
        pass.strict_reforge_evaluator_work_v1);
    saturating_add(
        total.strict_reforge_evaluator_work_v2,
        pass.strict_reforge_evaluator_work_v2);
    saturating_add(
        total.strict_reforge_evaluator_work_v3,
        pass.strict_reforge_evaluator_work_v3);
    merge_reforge_effort(
        total.strict_reforge_effort,
        pass.strict_reforge_effort);
    for (const ReforgeRowTelemetry& pass_row :
         pass.strict_reforge_row_samples) {
        constexpr std::size_t kSampleLimit = 64;
        if (total.strict_reforge_row_samples.size() >= kSampleLimit) {
            saturating_add(
                total.strict_reforge_row_samples_omitted, 1);
            continue;
        }
        ReforgeRowTelemetry row = pass_row;
        row.sequence = prior_reforge_row_sequence;
        saturating_add(row.sequence, pass_row.sequence);
        try {
            total.strict_reforge_row_samples.push_back(
                std::move(row));
        } catch (...) {
            saturating_add(
                total.strict_reforge_row_samples_omitted, 1);
        }
    }
    saturating_add(
        total.strict_reforge_row_samples_omitted,
        pass.strict_reforge_row_samples_omitted);
    saturating_add(total.selected_rows_begun, pass.selected_rows_begun);
    saturating_add(
        total.selected_rows_completed, pass.selected_rows_completed);
    saturating_add(
        total.selected_reforge_work, pass.selected_reforge_work);
    saturating_add(
        total.selected_transitions, pass.selected_transitions);
    saturating_add(
        total.alternative_rows_begun, pass.alternative_rows_begun);
    saturating_add(
        total.alternative_rows_completed,
        pass.alternative_rows_completed);
    saturating_add(
        total.alternative_reforge_work, pass.alternative_reforge_work);
    saturating_add(
        total.alternative_transitions, pass.alternative_transitions);
    for (const PolicyBroadRowAttribution& pass_sample :
         pass.broad_row_attribution) {
        constexpr std::size_t kSampleLimit = 16;
        if (total.broad_row_attribution.size() >= kSampleLimit) {
            ++total.broad_row_attribution_omitted;
            continue;
        }
        PolicyBroadRowAttribution sample = pass_sample;
        sample.row_sequence += sample.alternative
                                   ? prior_alternative_rows
                                   : total.selected_rows_completed -
                                         pass.selected_rows_completed;
        total.broad_row_attribution.push_back(std::move(sample));
    }
    saturating_add(
        total.broad_row_attribution_omitted,
        pass.broad_row_attribution_omitted);
    saturating_add(
        total.alternative_obligations_created,
        pass.alternative_obligations_created);
    saturating_add(
        total.unresolved_alternative_obligations,
        pass.unresolved_alternative_obligations);
    saturating_add(
        total.alternative_rows_avoided,
        pass.alternative_rows_avoided);
    total.action_accounting_complete =
        total.action_accounting_complete ||
        pass.action_accounting_complete;
    saturating_add(
        total.alternative_scheduling_rounds,
        pass.alternative_scheduling_rounds);
    saturating_add(
        total.alternative_obligations_scheduled,
        pass.alternative_obligations_scheduled);
    saturating_add(
        total.alternative_obligations_certified,
        pass.alternative_obligations_certified);
    saturating_add(
        total.alternative_obligations_partially_evaluated,
        pass.alternative_obligations_partially_evaluated);
    saturating_add(
        total.alternative_obligations_noncompetitive,
        pass.alternative_obligations_noncompetitive);
    saturating_add(
        total.alternative_obligations_stale,
        pass.alternative_obligations_stale);
    saturating_add(
        total.alternative_verdict_revocations,
        pass.alternative_verdict_revocations);
    saturating_add(
        total.alternative_obligations_resource_interrupted,
        pass.alternative_obligations_resource_interrupted);
    total.competitive_alternatives_remaining =
        pass.competitive_alternatives_remaining;
    saturating_add(
        total.alternative_policy_improvements,
        pass.alternative_policy_improvements);
    total.bounded_publication_retained =
        total.bounded_publication_retained ||
        pass.bounded_publication_retained;
    total.global_lower_bound_closed =
        total.global_lower_bound_closed ||
        pass.global_lower_bound_closed;
    total.exact_alternative_envelope_closed =
        pass.exact_alternative_envelope_closed;
    if (!total.work_to_first_partition.has_value() &&
        pass.work_to_first_partition.has_value()) {
        total.work_to_first_partition = prior_reforge_work;
        saturating_add(
            *total.work_to_first_partition,
            *pass.work_to_first_partition);
    }
    if (!total.work_to_first_executable_upper.has_value() &&
        pass.work_to_first_executable_upper.has_value()) {
        total.work_to_first_executable_upper = prior_reforge_work;
        saturating_add(
            *total.work_to_first_executable_upper,
            *pass.work_to_first_executable_upper);
        total.alternatives_materialized_before_first_upper =
            prior_alternative_rows;
        saturating_add(
            total.alternatives_materialized_before_first_upper,
            pass.alternatives_materialized_before_first_upper);
    }
    if (!total.wall_ns_to_first_partition.has_value() &&
        pass.wall_ns_to_first_partition.has_value()) {
        total.wall_ns_to_first_partition =
            pass.wall_ns_to_first_partition;
    }
    if (!total.wall_ns_to_first_executable_upper.has_value() &&
        pass.wall_ns_to_first_executable_upper.has_value()) {
        total.wall_ns_to_first_executable_upper =
            pass.wall_ns_to_first_executable_upper;
    }
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
    saturating_add(total.proof_payload_reuses, pass.proof_payload_reuses);
    saturating_add(total.row_reprojections, pass.row_reprojections);
    saturating_add(
        total.quotient_source_splits, pass.quotient_source_splits);
    saturating_add(
        total.quotient_target_splits, pass.quotient_target_splits);
    saturating_add(
        total.reverse_invalidations, pass.reverse_invalidations);
    saturating_add(
        total.improper_policy_repairs, pass.improper_policy_repairs);
    saturating_add(
        total.exact_carriers_replayed, pass.exact_carriers_replayed);
    total.current_live_slices = pass.current_live_slices;
    total.peak_live_slices = std::max(
        total.peak_live_slices, pass.peak_live_slices);
    total.current_live_slice_bytes = pass.current_live_slice_bytes;
    total.peak_live_slice_bytes = std::max(
        total.peak_live_slice_bytes, pass.peak_live_slice_bytes);
    total.coverage_descriptor_bytes = std::max(
        total.coverage_descriptor_bytes, pass.coverage_descriptor_bytes);
    total.certificate_bytes = std::max(
        total.certificate_bytes, pass.certificate_bytes);
    total.dependency_sidecar_bytes = std::max(
        total.dependency_sidecar_bytes, pass.dependency_sidecar_bytes);
    total.alternative_obligation_bytes = std::max(
        total.alternative_obligation_bytes,
        pass.alternative_obligation_bytes);
    total.partition_bytes = std::max(
        total.partition_bytes, pass.partition_bytes);
    total.carrier_bytes = std::max(
        total.carrier_bytes, pass.carrier_bytes);
    total.row_kernel_bytes = std::max(
        total.row_kernel_bytes, pass.row_kernel_bytes);
    total.scratch_bytes = std::max(
        total.scratch_bytes, pass.scratch_bytes);
    total.total_solver_owned_bytes = std::max(
        total.total_solver_owned_bytes, pass.total_solver_owned_bytes);
    saturating_add(
        total.reference_adapter_invocations,
        pass.reference_adapter_invocations);
    saturating_add(total.total_ns, pass.total_ns);
    saturating_add(
        total.carrier_discovery_ns,
        pass.carrier_discovery_ns);
    saturating_add(
        total.partition_refinement_ns,
        pass.partition_refinement_ns);
    saturating_add(
        total.policy_evaluation_ns,
        pass.policy_evaluation_ns);
    saturating_add(
        total.local_reoptimization_ns,
        pass.local_reoptimization_ns);
    saturating_add(total.compilation_ns, pass.compilation_ns);
    saturating_add(
        total.exact_evaluation_ns,
        pass.exact_evaluation_ns);
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
    /* Production strict selected and competitively scheduled alternative
     * rows use the exact V3 evaluator. Coarse remains V1. The native-only
     * controls retain V1 as an independent oracle/rollback and V2 as a
     * diagnostic; an explicit V3 flag remains backward-compatible. */
    const bool use_raw_v1 =
        options.raw_strict_reforge_oracle_diagnostic;
    const bool use_sparse_v2 =
        !use_raw_v1 &&
        options.projected_reforge_frontier_diagnostic &&
        !options.factored_terminal_reforge_diagnostic;
    limits.use_projected_reforge_frontier = !use_raw_v1;
    limits.use_factored_terminal_reforge =
        !use_raw_v1 && !use_sparse_v2;
    return limits;
}
