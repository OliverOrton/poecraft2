#include "solver_policy_refinement_helpers.hpp"

namespace poecraft {
namespace solver {
namespace refinement {

namespace {

#include "solver_policy_oracle.hpp"
#include "solver_policy_oracle_evaluate.inc"
#include "solver_policy_oracle_resources.inc"
#include "solver_policy_oracle_improve.inc"

} // namespace

PolicyExactLiftCertificate lift_policy_exact(
        CalcContext& coarse,
        const SolveResult& solved,
        const pc_item_state& exact_start,
        const std::unordered_map<std::string, double>& prices,
        const SolveOptions& options,
        const std::string& strategy_name,
        const RefinementLimits* limits_override) {
    PolicyExactLiftCertificate certificate;
    certificate.solver_cost = solved.evaluated_policy_cost;
    RefinementLimits limits =
        limits_override == nullptr
            ? default_limits(coarse, solved, options)
            : *limits_override;
    PolicyLiftAdapterTelemetry lift_telemetry;
    PolicyLiftAdapterTelemetry reoptimization_telemetry;
    const auto capture_adapter_telemetry = [&] {
        PolicyLiftAdapterTelemetry total;
        merge_adapter_telemetry(total, lift_telemetry);
        merge_adapter_telemetry(
            total, reoptimization_telemetry);
        certificate.adapter = std::move(total);
    };
    try {
        const auto check_initial_memory =
            [&](ProductionPolicyOracle& oracle,
                const RefinementLimits& pass_limits) {
                if (oracle.estimated_owned_bytes() >=
                    pass_limits.max_estimated_memory_bytes) {
                    throw AdapterFailure(
                        PolicyExactLiftStatus::ResourceCap,
                        "strict policy discovery reached "
                        "max_estimated_memory_bytes",
                        "max_estimated_memory_bytes");
                }
            };
        const auto finalize =
            [&](ProductionPolicyOracle& oracle,
                ExactPolicyRun policy) {
                const bool policy_complete =
                    policy.complete;
                certificate.exact_root_key =
                    oracle.root_key();
                certificate.refinement =
                    std::move(policy.refinement);
                certificate.class_evaluation =
                    std::move(policy.evaluation);
                std::vector<StableKey>{}.swap(policy.roots);
                policy.value_by_exact.clear();
                certificate.policy_changed =
                    oracle.final_policy_changed(
                        certificate.refinement);
                if (!policy_complete ||
                    certificate.class_evaluation
                            .start_values.size() != 1) {
                    return;
                }
                for (const StateClassAssignment& assignment :
                     certificate.refinement.assignments) {
                    if (assignment_exact_state(
                            certificate.refinement, assignment) ==
                        certificate.exact_root_key) {
                        certificate.root_refinement_class =
                            assignment.class_id;
                        break;
                    }
                }
                if (certificate.root_refinement_class ==
                    kNoId) {
                    throw AdapterFailure(
                        PolicyExactLiftStatus::
                            RefinementFailure,
                        "refinement result omitted the exact root "
                        "assignment");
                }
                certificate.exact_start_cost =
                    certificate.class_evaluation
                        .start_values.front();
                certificate.coarse_value_reconciled =
                    reconciled(
                        certificate.exact_start_cost,
                        certificate.solver_cost,
                        certificate.absolute_cost_delta,
                        certificate.relative_cost_delta);
                certificate.compiled =
                    oracle.assert_lifted_policy(
                        strategy_name,
                        certificate.refinement,
                        certificate.class_evaluation);
            };

        ReoptimizationSeed reoptimization_seed;
        if (solved.diagnostics.policy_refinement.triggers != 0) {
            reoptimization_seed.coarse_parents.insert(
                solved.diagnostics.policy_refinement
                    .trigger_coarse_states.begin(),
                solved.diagnostics.policy_refinement
                    .trigger_coarse_states.end());
            if (reoptimization_seed.coarse_parents.empty() &&
                solved.diagnostics.policy_compatibility_state !=
                    kNoId) {
                reoptimization_seed.coarse_parents.insert(
                    solved.diagnostics.policy_compatibility_state);
            }
        }
        ExactPolicyRun lift_policy;
        if (reoptimization_seed.empty()) {
            /*
             * Pass one is the minimum selected-policy carrier. It cannot
             * grow support for an unused alternative merely because that
             * alternative was admitted by the broad solver.
             */
            ProductionPolicyOracle oracle(
                coarse, solved, exact_start, prices,
                options, limits, lift_telemetry, nullptr);
            check_initial_memory(oracle, limits);
            lift_policy = oracle.evaluate_fixed_policy();
            reoptimization_seed =
                oracle.reoptimization_seed(lift_policy);
            const std::uint64_t spent_reforge = std::min(
                lift_telemetry.strict_reforge_work,
                options.max_reforge_work);
            const std::uint64_t remaining_reforge =
                options.max_reforge_work - spent_reforge;
            const bool optional_rebuild_unbudgeted =
                lift_policy.complete &&
                !reoptimization_seed.empty() &&
                spent_reforge > remaining_reforge;
            if (reoptimization_seed.empty() ||
                optional_rebuild_unbudgeted) {
                /*
                 * A complete fixed policy is already a valid bounded
                 * incumbent. Do not spend a smaller residual budget on a
                 * second adapter whose only purpose is optional improvement;
                 * retain the two-pass path whenever at least half of the
                 * declared reforge work remains.
                 */
                finalize(
                    oracle, std::move(lift_policy));
                reoptimization_seed = {};
            }
        }
        while (!reoptimization_seed.empty() &&
               certificate.compiled.status ==
                   CompiledPolicyAssertionStatus::NotRun) {
            /*
             * The first adapter is gone before widening the strict child.
             * Charge completed kernel/transition/reforge/round work to every
             * witness-driven rebuild so no declared cap can reset.
             */
            RefinementLimits reoptimization_limits =
                limits;
            const std::uint64_t spent_kernels =
                static_cast<std::uint64_t>(
                    lift_telemetry.strict_kernels_built) +
                reoptimization_telemetry.strict_kernels_built;
            if (spent_kernels >=
                reoptimization_limits.max_exact_kernels) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::ResourceCap,
                    "witness-driven local exact refinement reached "
                    "max_exact_kernels",
                    "max_exact_kernels");
            }
            reoptimization_limits.max_exact_kernels -=
                static_cast<std::uint32_t>(spent_kernels);
            std::uint64_t spent_transitions =
                lift_telemetry.strict_transitions_built;
            saturating_add(
                spent_transitions,
                reoptimization_telemetry.strict_transitions_built);
            if (spent_transitions >=
                reoptimization_limits.max_transitions) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::ResourceCap,
                    "witness-driven local exact refinement reached "
                    "max_transitions",
                    "max_transitions");
            }
            reoptimization_limits.max_transitions -=
                spent_transitions;
            std::uint64_t spent_rounds =
                lift_telemetry.backward_observation_rounds;
            saturating_add(
                spent_rounds,
                lift_telemetry.exact_fixed_point_rounds);
            saturating_add(
                spent_rounds,
                lift_telemetry.local_reoptimization_rounds);
            saturating_add(
                spent_rounds,
                reoptimization_telemetry
                    .backward_observation_rounds);
            saturating_add(
                spent_rounds,
                reoptimization_telemetry
                    .exact_fixed_point_rounds);
            saturating_add(
                spent_rounds,
                reoptimization_telemetry
                    .local_reoptimization_rounds);
            if (spent_rounds >=
                reoptimization_limits
                    .max_refinement_rounds) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::ResourceCap,
                    "witness-driven local exact refinement reached "
                    "max_refinement_rounds",
                    "max_refinement_rounds");
            }
            reoptimization_limits.max_refinement_rounds -=
                static_cast<std::uint32_t>(spent_rounds);
            SolveOptions reoptimization_options = options;
            std::uint64_t spent_reforge =
                lift_telemetry.strict_reforge_work;
            saturating_add(
                spent_reforge,
                reoptimization_telemetry.strict_reforge_work);
            if (spent_reforge >=
                reoptimization_options.max_reforge_work) {
                throw AdapterFailure(
                    PolicyExactLiftStatus::ResourceCap,
                    "witness-driven local exact refinement reached "
                    "max_reforge_work",
                    "max_reforge_work");
            }
            reoptimization_options.max_reforge_work -=
                spent_reforge;
            lift_policy = ExactPolicyRun{};

            PolicyLiftAdapterTelemetry pass_telemetry;
            ReoptimizationSeed discovered_seed;
            bool rebuild = false;
            try {
                ProductionPolicyOracle oracle(
                    coarse, solved, exact_start, prices,
                    reoptimization_options,
                    reoptimization_limits,
                    pass_telemetry,
                    &reoptimization_seed);
                check_initial_memory(
                    oracle, reoptimization_limits);
                ExactPolicyRun policy =
                    oracle.repair_invalid_policy();
                if (policy.complete) {
                    policy = oracle.improve_policy(
                        std::move(policy));
                }
                discovered_seed =
                    oracle.reoptimization_seed(policy);
                rebuild =
                    oracle.requires_operator_vocabulary_widening(
                        policy);
                if (!rebuild) {
                    finalize(oracle, std::move(policy));
                    reoptimization_seed = {};
                }
            } catch (...) {
                merge_adapter_telemetry(
                    reoptimization_telemetry,
                    pass_telemetry);
                throw;
            }
            merge_adapter_telemetry(
                reoptimization_telemetry,
                pass_telemetry);
            if (rebuild) {
                const std::size_t before =
                    reoptimization_seed.coarse_parents.size();
                reoptimization_seed.merge(discovered_seed);
                if (reoptimization_seed.coarse_parents.size() ==
                    before) {
                    throw AdapterFailure(
                        PolicyExactLiftStatus::
                            LocalReoptimizationRequired,
                        "local exact refinement requested the same "
                        "operator vocabulary twice without progress");
                }
            }
        }
        capture_adapter_telemetry();
    } catch (const AdapterFailure& error) {
        capture_adapter_telemetry();
        certificate.status = error.status;
        certificate.failure_reason = error.what();
        certificate.resource_cap = error.cap;
        return certificate;
    } catch (const SolverResourceLimit& error) {
        capture_adapter_telemetry();
        certificate.status = PolicyExactLiftStatus::ResourceCap;
        certificate.failure_reason = error.what();
        certificate.resource_cap =
            adapter_resource_cap_name(error.cap_name());
        return certificate;
    } catch (const std::length_error& error) {
        capture_adapter_telemetry();
        certificate.status = PolicyExactLiftStatus::ResourceCap;
        certificate.failure_reason = error.what();
        certificate.resource_cap =
            resource_cap_from_message(error.what());
        return certificate;
    } catch (const std::exception& error) {
        capture_adapter_telemetry();
        certificate.status =
            PolicyExactLiftStatus::RefinementFailure;
        certificate.failure_reason = error.what();
        return certificate;
    }

    if (certificate.refinement.status !=
            RefinementStatus::Complete ||
        !certificate.refinement.executable ||
        !certificate.refinement.lumpable) {
        certificate.status =
            certificate.refinement.status ==
                    RefinementStatus::ResourceCap ||
                    certificate.refinement.status ==
                        RefinementStatus::RefinementRoundCap
                ? PolicyExactLiftStatus::ResourceCap
                : PolicyExactLiftStatus::RefinementFailure;
        certificate.failure_reason =
            certificate.refinement.failure_reason;
        certificate.resource_cap =
            certificate.refinement.resource_cap;
        return certificate;
    }
    certificate.lumpable = true;
    if (certificate.class_evaluation.status !=
            PolicyEvaluationStatus::Complete ||
        !certificate.class_evaluation.converged ||
        !certificate.class_evaluation.proper ||
        certificate.class_evaluation.start_values.size() != 1) {
        certificate.status =
            certificate.class_evaluation.status ==
                    PolicyEvaluationStatus::ResourceCap
                ? PolicyExactLiftStatus::ResourceCap
                : PolicyExactLiftStatus::RefinementFailure;
        certificate.failure_reason =
            certificate.class_evaluation.failure_reason.empty()
                ? "refined class policy did not produce one proper "
                  "root value"
                : certificate.class_evaluation.failure_reason;
        certificate.resource_cap =
            certificate.class_evaluation.resource_cap;
        return certificate;
    }

    if (certificate.compiled.status ==
        CompiledPolicyAssertionStatus::Complete) {
        double artifact_delta = 0.0;
        double artifact_relative = 0.0;
        if (!reconciled(
                certificate.compiled.exact_cost,
                certificate.exact_start_cost,
                artifact_delta,
                artifact_relative)) {
            certificate.status =
                PolicyExactLiftStatus::CompiledAssertionFailure;
            certificate.failure_reason =
                "compiled lifted artifact does not reconcile with its "
                "refined class-policy value";
            return certificate;
        }
        certificate.status = PolicyExactLiftStatus::Complete;
        certificate.executable = true;
        return certificate;
    }

    certificate.failure_reason =
        certificate.compiled.failure_reason;
    certificate.resource_cap =
        certificate.compiled.resource_cap;
    if (certificate.compiled.status ==
               CompiledPolicyAssertionStatus::ResourceCap) {
        certificate.status = PolicyExactLiftStatus::ResourceCap;
    } else {
        certificate.status =
            PolicyExactLiftStatus::CompiledAssertionFailure;
    }
    return certificate;
}

} // namespace refinement
} // namespace solver
} // namespace poecraft
