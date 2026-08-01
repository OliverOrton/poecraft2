ExactPolicyRun ProductionPolicyOracle::evaluate_current_policy(
        std::vector<StableKey> exact_roots,
        const std::uint64_t caller_retained_bytes) {
    ExactPolicyRun run;
    run.roots = std::move(exact_roots);
    std::sort(run.roots.begin(), run.roots.end());
    run.roots.erase(
        std::unique(run.roots.begin(), run.roots.end()),
        run.roots.end());
    if (run.roots.empty()) {
        run.refinement.status = RefinementStatus::EmptyRequest;
        run.refinement.failure_reason =
            "local exact policy evaluation has no roots";
        return run;
    }

    std::map<std::uint32_t, std::vector<StableKey>> filtered;
    for (const StableKey& key : run.roots) {
        const auto found = known_.find(key);
        if (found == known_.end()) {
            run.refinement.status = RefinementStatus::InvalidState;
            run.refinement.failure_reason =
                "local exact policy root has no strict carrier";
            return run;
        }
        filtered[found->second.coarse_state].push_back(key);
    }
    for (auto& [unused, keys] : filtered) {
        (void)unused;
        std::sort(keys.begin(), keys.end());
        keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
    }

    std::uint64_t retained_before_refinement =
        caller_retained_bytes;
    saturating_add(
        retained_before_refinement,
        stable_keys_bytes(run.roots));
    if (retained_before_refinement >=
        limits_.max_estimated_memory_bytes) {
        run.refinement.status = RefinementStatus::ResourceCap;
        run.refinement.resource_cap =
            "max_estimated_memory_bytes";
        run.refinement.failure_reason =
            "caller-retained exact policy reached "
            "max_estimated_memory_bytes";
        return run;
    }
    RefinementLimits run_limits = limits_;
    if (refinement_rounds_consumed_ >=
        limits_.max_refinement_rounds) {
        run.refinement.status = RefinementStatus::ResourceCap;
        run.refinement.resource_cap =
            "max_refinement_rounds";
        run.refinement.failure_reason =
            "local exact policy reached max_refinement_rounds";
        return run;
    }
    run_limits.max_refinement_rounds =
        static_cast<std::uint32_t>(
            limits_.max_refinement_rounds -
            refinement_rounds_consumed_);
    run_limits.max_estimated_memory_bytes -=
        retained_before_refinement;
    RefinementRequest request;
    for (const auto& [coarse_state, unused] : filtered) {
        (void)unused;
        request.coarse_roots.push_back(coarse_state);
    }
    request.limits = run_limits;

    illegal_action_witness_.reset();
    exact_root_filter_ = std::move(filtered);
    try {
        run.refinement =
            refine_policy_exact(*this, std::move(request));
    } catch (...) {
        exact_root_filter_.reset();
        throw;
    }
    exact_root_filter_.reset();
    const std::uint64_t fixed_point_rounds =
        static_cast<std::uint64_t>(
            run.refinement.telemetry
                .selected_action_routing_rounds) +
        static_cast<std::uint64_t>(
            run.refinement.telemetry
                .observation_propagation_rounds) +
        run.refinement.telemetry
            .partition_refinement_rounds;
    consume_refinement_rounds(
        fixed_point_rounds,
        "local exact policy fixed point");
    telemetry_.exact_fixed_point_rounds +=
        fixed_point_rounds;
    if (run.refinement.status ==
        RefinementStatus::RefinementRoundCap) {
        run.refinement.status =
            RefinementStatus::ResourceCap;
        run.refinement.resource_cap =
            "max_refinement_rounds";
    }
    if (run.refinement.status != RefinementStatus::Complete ||
        !run.refinement.executable ||
        !run.refinement.lumpable) {
        if (illegal_action_witness_.has_value()) {
            std::uint64_t retained = caller_retained_bytes;
            saturating_add(retained, exact_policy_run_bytes(run));
            saturating_add(
                retained, sizeof(StableKey));
            saturating_add(
                retained,
                stable_key_bytes(*illegal_action_witness_));
            require_local_memory(
                retained,
                "local exact illegal-action witness");
            run.defect_witnesses.push_back(
                *illegal_action_witness_);
        }
        return run;
    }

    PolicyEvaluationRequest evaluation_request;
    for (const StableKey& key : run.roots) {
        const auto found = std::find_if(
            run.refinement.assignments.begin(),
            run.refinement.assignments.end(),
            [&](const StateClassAssignment& assignment) {
                return assignment_exact_state(
                           run.refinement, assignment) == key;
            });
        if (found == run.refinement.assignments.end()) {
            run.evaluation.status =
                PolicyEvaluationStatus::InvalidStartClass;
            run.evaluation.failure_reason =
                "refined local policy omitted an exact root";
            return run;
        }
        evaluation_request.start_classes.push_back(
            found->class_id);
    }
    evaluation_request.limits.max_reachable_classes =
        run_limits.max_refinement_classes;
    evaluation_request.limits.max_component_iterations =
        std::max<std::uint32_t>(1, options_.max_sweeps);

    /*
     * The refinement and oracle remain live during exact SCC evaluation.
     * Give the evaluator only the residual of the one publication budget;
     * its own ledger then bounds scratch and returned values inside that
     * residual.
     */
    std::uint64_t retained = estimated_owned_bytes();
    const auto add_retained =
        [&](const std::uint64_t bytes) {
            retained =
                bytes >
                        std::numeric_limits<std::uint64_t>::max() -
                            retained
                    ? std::numeric_limits<std::uint64_t>::max()
                    : retained + bytes;
    };
    add_retained(refinement_result_bytes(run.refinement));
    add_retained(
        evaluation_request.start_classes.capacity() *
        sizeof(std::uint32_t));
    if (retained >=
        run_limits.max_estimated_memory_bytes) {
        run.evaluation.status =
            PolicyEvaluationStatus::ResourceCap;
        run.evaluation.resource_cap =
            "max_estimated_memory_bytes";
        run.evaluation.failure_reason =
            "oracle and refinement leave no exact-policy evaluation "
            "memory";
        return run;
    }
    evaluation_request.limits.max_estimated_memory_bytes =
        run_limits.max_estimated_memory_bytes - retained;
    run.evaluation = evaluate_refined_policy_exact(
        run.refinement, std::move(evaluation_request));
    if (run.evaluation.status ==
            PolicyEvaluationStatus::ImproperPolicy) {
        if (run.evaluation.improper_component_count == 0 ||
            run.evaluation.improper_component_classes.empty()) {
            run.evaluation.status =
                PolicyEvaluationStatus::InvalidPolicy;
            run.evaluation.failure_reason =
                "improper policy omitted its canonical component";
            return run;
        }
        std::size_t projected_members = 0;
        std::uint64_t projected_member_bytes = 0;
        for (const std::uint32_t class_id :
             run.evaluation.improper_component_classes) {
            const auto found = std::find_if(
                run.refinement.classes.begin(),
                run.refinement.classes.end(),
                [class_id](const RefinedPolicyClass& candidate) {
                    return candidate.class_id == class_id;
                });
            if (found == run.refinement.classes.end() ||
                found->terminal ||
                found->exact_members.empty() ||
                projected_members >
                    std::numeric_limits<std::size_t>::max() -
                        found->exact_members.size()) {
                run.evaluation.status =
                    PolicyEvaluationStatus::InvalidRefinement;
                run.evaluation.failure_reason =
                    "improper component has no exact carrier members";
                return run;
            }
            projected_members += found->exact_members.size();
            for (const StableKey& member : found->exact_members) {
                saturating_add(
                    projected_member_bytes,
                    sizeof(StableKey));
                saturating_add(
                    projected_member_bytes,
                    stable_key_bytes(member));
            }
        }
        std::uint64_t retained = caller_retained_bytes;
        saturating_add(retained, exact_policy_run_bytes(run));
        saturating_add(retained, projected_member_bytes);
        require_local_memory(
            retained,
            "local exact improper-component witnesses");
        run.defect_witnesses.reserve(projected_members);
        for (const std::uint32_t class_id :
             run.evaluation.improper_component_classes) {
            const auto found = std::find_if(
                run.refinement.classes.begin(),
                run.refinement.classes.end(),
                [class_id](const RefinedPolicyClass& candidate) {
                    return candidate.class_id == class_id;
                });
            run.defect_witnesses.insert(
                run.defect_witnesses.end(),
                found->exact_members.begin(),
                found->exact_members.end());
        }
        std::sort(
            run.defect_witnesses.begin(),
            run.defect_witnesses.end());
        run.defect_witnesses.erase(
            std::unique(
                run.defect_witnesses.begin(),
                run.defect_witnesses.end()),
            run.defect_witnesses.end());
        if (run.defect_witnesses.empty()) {
            run.evaluation.status =
                PolicyEvaluationStatus::InvalidRefinement;
            run.evaluation.failure_reason =
                "improper component produced no exact witnesses";
            return run;
        }
        std::uint64_t actual_retained = caller_retained_bytes;
        saturating_add(
            actual_retained, exact_policy_run_bytes(run));
        require_local_memory(
            actual_retained,
            "returned local exact improper-component witnesses");
    }
    if (run.evaluation.status !=
            PolicyEvaluationStatus::Complete ||
        !run.evaluation.converged ||
        !run.evaluation.proper ||
        run.evaluation.start_values.size() !=
            run.roots.size()) {
        return run;
    }

    std::vector<double> value_by_class(
        run.refinement.classes.size(),
        std::numeric_limits<double>::infinity());
    std::uint64_t value_construction_bytes =
        caller_retained_bytes;
    saturating_add(
        value_construction_bytes,
        exact_policy_run_bytes(run));
    saturating_add(
        value_construction_bytes,
        value_by_class.capacity() * sizeof(double));
    std::uint64_t projected_value_map_bytes =
        run.refinement.assignments.size() *
        (sizeof(decltype(
             run.value_by_exact)::value_type) +
         3 * sizeof(void*));
    for (const StateClassAssignment& assignment :
         run.refinement.assignments) {
        saturating_add(
            projected_value_map_bytes,
            stable_key_bytes(assignment_exact_state(
                run.refinement, assignment)));
    }
    saturating_add(
        value_construction_bytes,
        projected_value_map_bytes);
    require_local_memory(
        value_construction_bytes,
        "local exact value-map construction");
    for (const RefinedClassValue& value :
         run.evaluation.class_values) {
        if (value.class_id >= value_by_class.size()) {
            run.evaluation.status =
                PolicyEvaluationStatus::InvalidRefinement;
            run.evaluation.failure_reason =
                "local exact class value has an invalid id";
            return run;
        }
        value_by_class[value.class_id] = value.value;
    }
    for (const StateClassAssignment& assignment :
         run.refinement.assignments) {
        if (assignment.class_id >= value_by_class.size() ||
            !std::isfinite(value_by_class[assignment.class_id])) {
            run.evaluation.status =
                PolicyEvaluationStatus::InvalidRefinement;
            run.evaluation.failure_reason =
                "local exact assignment has no finite class value";
            return run;
        }
        run.value_by_exact.emplace(
            assignment_exact_state(run.refinement, assignment),
            value_by_class[assignment.class_id]);
    }
    std::uint64_t returned_bytes =
        caller_retained_bytes;
    saturating_add(
        returned_bytes, exact_policy_run_bytes(run));
    require_local_memory(
        returned_bytes,
        "the returned local exact policy");
    run.complete = true;
    return run;
}
