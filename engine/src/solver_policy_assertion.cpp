#include "solver_policy_refinement_helpers.hpp"
#include "solver_compile_contracts.hpp"

#include <chrono>

namespace poecraft {
namespace solver {
namespace refinement {

void finalize_compiled_policy_assertion(
        CompiledPolicyAssertion& result) {
    result.off_policy_probability =
        result.evaluation.failure_probability +
        result.evaluation.stop_probability +
        result.evaluation.action_not_applied_probability +
        result.evaluation.no_matching_edge_probability +
        result.evaluation.unresolved_probability;
    result.zero_off_policy =
        result.off_policy_probability <= kOffPolicyTolerance &&
        result.evaluation.success_probability >=
            1.0 - kOffPolicyTolerance;
    result.proper =
        result.evaluation.converged && result.zero_off_policy;
    result.exact_cost = result.evaluation.total_expected_cost;
    result.cost_reconciled =
        result.evaluation.cost_complete &&
        std::isfinite(result.solver_cost) &&
        std::isfinite(result.exact_cost) &&
        reconciled(
            result.exact_cost,
            result.solver_cost,
            result.absolute_cost_delta,
            result.relative_cost_delta);

    if (!result.proper) {
        result.status =
            CompiledPolicyAssertionStatus::ImproperPolicy;
        result.failure_reason =
            "compiled policy is not a proper absorbing success policy "
            "(converged=" +
            std::string{result.evaluation.converged ? "true" : "false"} +
            ", success=" +
            std::to_string(result.evaluation.success_probability) +
            ", failure=" +
            std::to_string(result.evaluation.failure_probability) +
            ", stop=" +
            std::to_string(result.evaluation.stop_probability) +
            ", action_not_applied=" +
            std::to_string(
                result.evaluation.action_not_applied_probability) +
            ", no_matching_edge=" +
            std::to_string(
                result.evaluation.no_matching_edge_probability) +
            ", unresolved=" +
            std::to_string(result.evaluation.unresolved_probability) +
            ")";
        if (!result.evaluation.terminal_nodes.empty()) {
            result.failure_reason += "; terminals=";
            for (std::size_t index = 0;
                 index < result.evaluation.terminal_nodes.size(); ++index) {
                if (index != 0) result.failure_reason += ',';
                const StrategyEvalTerminalNode& terminal =
                    result.evaluation.terminal_nodes[index];
                result.failure_reason += terminal.node_id + ':' +
                    std::to_string(terminal.probability);
            }
        }
    } else if (!result.evaluation.cost_complete) {
        result.status =
            CompiledPolicyAssertionStatus::IncompleteCost;
        result.failure_reason =
            "compiled policy exact evaluation has incomplete prices";
    } else if (!result.cost_reconciled) {
        result.executable = true;
        result.status =
            CompiledPolicyAssertionStatus::CostMismatch;
        result.failure_reason =
            "compiled policy exact cost does not reconcile with the "
            "solver value";
    } else {
        result.status = CompiledPolicyAssertionStatus::Complete;
        result.executable = true;
        result.failure_reason.clear();
    }
}

static CompiledPolicyAssertion assert_compiled_policy_exact_reference(
        CalcContext& coarse,
        const SolveResult& solved,
        const std::unordered_map<std::string, double>& prices,
        const SolveOptions& options,
        const std::string& strategy_name,
        const RefinedPolicyCompileRouting* refined_routing,
        const std::string* emitted_strategy_json,
        const PolicyCompilationTelemetry* emitted_compilation) {
    CompiledPolicyAssertion result;
    result.solver_cost = solved.evaluated_policy_cost;
    if (!solved.policy_available) {
        result.status = CompiledPolicyAssertionStatus::NoPolicy;
        result.failure_reason = "solve did not publish a policy";
        return result;
    }
    result.retained_solver_bytes =
        estimated_retained_solver_bytes(coarse, &solved);
    result.publication_peak_owned_bytes =
        result.retained_solver_bytes;
    if (result.retained_solver_bytes >=
        options.max_solver_owned_bytes) {
        result.status = CompiledPolicyAssertionStatus::ResourceCap;
        result.resource_cap = "max_solver_owned_bytes";
        result.failure_reason =
            "retained solver state leaves no memory for exact policy "
            "compilation";
        return result;
    }
    const std::uint64_t compilation_memory =
        options.max_solver_owned_bytes -
        result.retained_solver_bytes;
    if (compilation_memory <= 1) {
        result.status = CompiledPolicyAssertionStatus::ResourceCap;
        result.resource_cap = "max_solver_owned_bytes";
        result.failure_reason =
            "retained solver state leaves no memory for exact policy "
            "JSON";
        return result;
    }
    const std::uint64_t compilation_json_limit =
        std::min<std::uint64_t>(
            options.max_strategy_json_bytes,
            compilation_memory - 1);
    const bool json_limited_by_memory =
        compilation_json_limit <
        options.max_strategy_json_bytes;
    if (emitted_strategy_json != nullptr) {
        if (emitted_strategy_json->empty()) {
            result.status =
                CompiledPolicyAssertionStatus::CompilationFailure;
            result.failure_reason =
                "precompiled policy assertion received empty JSON";
            return result;
        }
        if (emitted_strategy_json->size() > compilation_json_limit) {
            result.status = CompiledPolicyAssertionStatus::ResourceCap;
            result.resource_cap = json_limited_by_memory
                ? "max_solver_owned_bytes"
                : "max_strategy_json_bytes";
            result.failure_reason =
                "precompiled policy reached " + result.resource_cap;
            return result;
        }
        result.strategy_json = *emitted_strategy_json;
        if (emitted_compilation != nullptr) {
            result.compilation = *emitted_compilation;
        }
        {
            std::uint64_t live =
                result.retained_solver_bytes;
            saturating_add(
                live,
                std::max<std::uint64_t>(
                    result.compilation.peak_owned_bytes,
                    result.strategy_json.capacity() + 1));
            result.publication_peak_owned_bytes =
                std::max(
                    result.publication_peak_owned_bytes,
                    live);
        }
    } else {
        const auto compilation_started =
            std::chrono::steady_clock::now();
        const auto record_compilation_time = [&] {
            result.compilation_ns = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() -
                    compilation_started)
                    .count());
        };
        try {
            result.strategy_json = compile_policy_strategy_json(
                coarse, solved, strategy_name, &result.compilation,
                compilation_json_limit, refined_routing,
                compilation_memory);
            record_compilation_time();
            {
                std::uint64_t live =
                    result.retained_solver_bytes;
                saturating_add(
                    live,
                    std::max<std::uint64_t>(
                        result.compilation.peak_owned_bytes,
                        result.strategy_json.capacity() + 1));
                result.publication_peak_owned_bytes =
                    std::max(
                        result.publication_peak_owned_bytes,
                        live);
            }
            if (!result.compilation.cap_hit.empty()) {
                result.status =
                    CompiledPolicyAssertionStatus::ResourceCap;
                result.resource_cap =
                    json_limited_by_memory &&
                            result.compilation.cap_hit ==
                                "max_strategy_json_bytes"
                        ? "max_solver_owned_bytes"
                        : result.compilation.cap_hit;
                result.failure_reason =
                    "compiled policy reached " +
                    result.resource_cap;
                return result;
            }
        } catch (const SolverResourceLimit& error) {
            record_compilation_time();
            std::uint64_t live =
                result.retained_solver_bytes;
            saturating_add(
                live, result.compilation.peak_owned_bytes);
            result.publication_peak_owned_bytes =
                std::max(
                    result.publication_peak_owned_bytes, live);
            result.status = CompiledPolicyAssertionStatus::ResourceCap;
            result.resource_cap = error.cap_name();
            result.failure_reason = error.what();
            return result;
        } catch (const std::length_error& error) {
            record_compilation_time();
            result.status = CompiledPolicyAssertionStatus::ResourceCap;
            result.resource_cap =
                resource_cap_from_message(error.what());
            result.failure_reason = error.what();
            return result;
        } catch (const std::exception& error) {
            record_compilation_time();
            result.status = result.compilation.cap_hit.empty()
                                ? CompiledPolicyAssertionStatus::
                                      CompilationFailure
                                : CompiledPolicyAssertionStatus::ResourceCap;
            result.resource_cap =
                json_limited_by_memory &&
                        result.compilation.cap_hit ==
                            "max_strategy_json_bytes"
                    ? "max_solver_owned_bytes"
                    : result.compilation.cap_hit;
            result.failure_reason = error.what();
            return result;
        }
    }

    const auto failure_with_graph_context =
        [&](const char* message) {
            return std::string{message} +
                "; compilation_nodes=" +
                std::to_string(result.compilation.nodes) +
                ", compilation_edges=" +
                std::to_string(result.compilation.edges) +
                ", policy_regions=" +
                std::to_string(result.compilation.policy_regions) +
                ", reachable_states=" +
                std::to_string(result.evaluation.raw_pairs_discovered) +
                ", state_action_pairs=" +
                std::to_string(result.evaluation.refined_pairs) +
                ", evaluator_owned=" +
                std::to_string(
                    result.evaluation.owned_bytes_estimate) +
                ", evaluator_peak=" +
                std::to_string(
                    result.evaluation.peak_owned_bytes_estimate) +
                ", evaluator_budget=" +
                std::to_string(result.evaluator_memory_budget);
        };
    const auto evaluation_started =
        std::chrono::steady_clock::now();
    const auto record_evaluation_time = [&] {
        result.exact_evaluation_ns = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() -
                evaluation_started)
                .count());
    };
    try {
        const std::uint64_t strategy_json_bytes =
            result.strategy_json.capacity() + 1;
        std::uint64_t remaining_memory =
            options.max_solver_owned_bytes -
            result.retained_solver_bytes;
        const auto consume_memory =
            [&](const std::uint64_t bytes,
                const char* subject) {
                if (bytes >= remaining_memory) {
                    result.status =
                        CompiledPolicyAssertionStatus::ResourceCap;
                    result.resource_cap =
                        "max_solver_owned_bytes";
                    result.failure_reason =
                        std::string{"no remaining solver memory for "} +
                        subject;
                    return false;
                }
                remaining_memory -= bytes;
                return true;
            };
        if (!consume_memory(
                strategy_json_bytes,
                "parsed exact policy evaluation")) {
            record_evaluation_time();
            return result;
        }
        const std::shared_ptr<const SessionImpl> session =
            borrow_session(coarse);
        const std::shared_ptr<StrategyImpl> strategy =
            compile_strategy_json(
                session,
                result.strategy_json.data(),
                result.strategy_json.size());
        result.parsed_strategy_bytes =
            strategy_impl_owned_bytes(*strategy);
        {
            std::uint64_t live =
                result.retained_solver_bytes;
            saturating_add(live, strategy_json_bytes);
            saturating_add(
                live, result.parsed_strategy_bytes);
            result.publication_peak_owned_bytes =
                std::max(
                    result.publication_peak_owned_bytes,
                    live);
        }
        if (!consume_memory(
                result.parsed_strategy_bytes,
                "the parsed exact strategy")) {
            record_evaluation_time();
            return result;
        }
        auto economy = std::make_shared<EconomyImpl>();
        economy->id = "policy-guided-exact-refinement";
        economy->prices = prices;
        result.economy_bytes = economy_owned_bytes(
            economy->prices, economy->id.capacity());
        {
            std::uint64_t live =
                result.retained_solver_bytes;
            saturating_add(live, strategy_json_bytes);
            saturating_add(
                live, result.parsed_strategy_bytes);
            saturating_add(live, result.economy_bytes);
            result.publication_peak_owned_bytes =
                std::max(
                    result.publication_peak_owned_bytes,
                    live);
        }
        if (!consume_memory(
                result.economy_bytes,
                "the exact-evaluation economy")) {
            record_evaluation_time();
            return result;
        }

        result.evaluator_memory_budget = remaining_memory;
        StrategyEvalOptions evaluation_options;
        evaluation_options.epsilon = 1e-12;
        evaluation_options.max_sweeps =
            std::max<std::uint32_t>(1, options.max_sweeps);
        evaluation_options.max_states =
            std::max<std::uint32_t>(
                1, options.max_discovered_states);
        evaluation_options.max_pairs = std::max<std::uint32_t>(
            1, bounded_u32(options.max_state_action_rows));
        evaluation_options.max_transitions =
            std::max<std::uint32_t>(
                1, bounded_u32(options.max_transitions));
        evaluation_options.max_owned_bytes =
            result.evaluator_memory_budget;
        evaluation_options.max_output_json_bytes =
            options.max_strategy_json_bytes;
        evaluation_options.max_reforge_work =
            options.max_reforge_work;
        evaluation_options.economy = std::move(economy);
        StrategyEvalWork evaluation_work(
            strategy, evaluation_options);
        try {
            while (!evaluation_work.progress().done) {
                evaluation_work.step(4096);
            }
        } catch (...) {
            result.evaluation =
                evaluation_work.diagnostic_result();
            throw;
        }
        result.evaluation = evaluation_work.result();
        record_evaluation_time();
        {
            std::uint64_t live =
                result.retained_solver_bytes;
            saturating_add(live, strategy_json_bytes);
            saturating_add(
                live, result.parsed_strategy_bytes);
            saturating_add(live, result.economy_bytes);
            saturating_add(
                live,
                result.evaluation
                    .peak_owned_bytes_estimate);
            result.publication_peak_owned_bytes =
                std::max(
                    result.publication_peak_owned_bytes,
                    live);
        }
    } catch (const SolverResourceLimit& error) {
        record_evaluation_time();
        result.status = CompiledPolicyAssertionStatus::ResourceCap;
        result.resource_cap = error.cap_name();
        result.failure_reason =
            failure_with_graph_context(error.what());
        return result;
    } catch (const std::length_error& error) {
        record_evaluation_time();
        result.status = CompiledPolicyAssertionStatus::ResourceCap;
        result.resource_cap =
            resource_cap_from_message(error.what());
        result.failure_reason =
            failure_with_graph_context(error.what());
        return result;
    } catch (const std::exception& error) {
        record_evaluation_time();
        result.status =
            CompiledPolicyAssertionStatus::ExactEvaluationFailure;
        result.failure_reason =
            failure_with_graph_context(error.what());
        return result;
    }

    finalize_compiled_policy_assertion(result);
    return result;
}

const char* policy_exact_lift_status_name(
        const PolicyExactLiftStatus status) {
    switch (status) {
    case PolicyExactLiftStatus::Complete:
        return "complete";
    case PolicyExactLiftStatus::NoPolicy:
        return "no_policy";
    case PolicyExactLiftStatus::InvalidSolveState:
        return "invalid_solve_state";
    case PolicyExactLiftStatus::MissingPrice:
        return "missing_price";
    case PolicyExactLiftStatus::UnsupportedPrimitiveKernel:
        return "unsupported_primitive_kernel";
    case PolicyExactLiftStatus::CoarseMappingFailure:
        return "coarse_mapping_failure";
    case PolicyExactLiftStatus::ObservationUnavailable:
        return "observation_unavailable";
    case PolicyExactLiftStatus::ResourceCap:
        return "resource_cap";
    case PolicyExactLiftStatus::RefinementFailure:
        return "refinement_failure";
    case PolicyExactLiftStatus::LocalReoptimizationRequired:
        return "local_reoptimization_required";
    case PolicyExactLiftStatus::CompiledAssertionFailure:
        return "compiled_assertion_failure";
    }
    return "invalid_solve_state";
}


} // namespace refinement
} // namespace solver
} // namespace poecraft
