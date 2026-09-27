#include "solver_solve_types.hpp"

#include "solver_finder.hpp"
#include "solver_policy_refinement.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace poecraft::solver {

void SolveWork::Impl::abandon_selective_completion_service(
        const char* status) {
    result.diagnostics.selective_completion_service_status = status;
    selective_service_validator.reset();
    selective_service_checker.reset();
    selective_service_strategy.reset();
    selective_service_economy.reset();
    selective_service_candidate.reset();
    selective_service_producer.reset();
    selective_service_calc.reset();
    std::string{}.swap(selective_service_graph);
    selective_service_phase = SelectiveServicePhase::Done;
}

bool SolveWork::Impl::advance_selective_completion_service() {
    const auto check_memory = [&] {
        if (fast_estimated_owned_bytes() >=
            options.max_solver_owned_bytes)
            throw std::length_error(
                "selective completion has no aggregate memory headroom");
    };
    const auto charge_checker = [&] {
        if (!selective_service_checker) return;
        const auto& used =
            selective_service_checker->diagnostic_result();
        const auto active_delta = used.reforge_work -
            selective_service_checker_charged_active;
        const auto work_delta = used.reforge_logical_work_v1 -
            selective_service_checker_charged_work;
        selective_service_checker_charged_active =
            used.reforge_work;
        selective_service_checker_charged_work =
            used.reforge_logical_work_v1;
        calc.consume_reforge_work(active_delta, work_delta);
    };
    const auto charge_private_calc = [&] {
        if (!selective_service_calc) return;
        const auto& used = selective_service_calc->telemetry();
        const auto active = used.reforge_frontier_work +
            used.automatic_admission_reforge_active_work;
        const auto logical = used.reforge_logical_work_v1 +
            used.automatic_admission_reforge_logical_work_v1;
        const auto active_delta = active -
            selective_service_calc_charged_active;
        const auto work_delta = logical -
            selective_service_calc_charged_work;
        selective_service_calc_charged_active = active;
        selective_service_calc_charged_work = logical;
        calc.consume_reforge_work(active_delta, work_delta);
    };
    const auto charge_validator = [&] {
        if (!selective_service_validator) return;
        const auto active =
            selective_service_validator->active_work();
        const auto logical =
            selective_service_validator->logical_work();
        const auto active_delta = active -
            selective_service_validator_charged_active;
        const auto work_delta = logical -
            selective_service_validator_charged_work;
        selective_service_validator_charged_active = active;
        selective_service_validator_charged_work = logical;
        calc.consume_reforge_work(active_delta, work_delta);
    };
    try {
        if (selective_service_phase ==
                SelectiveServicePhase::NotStarted) {
            const std::uint64_t live =
                fast_estimated_owned_bytes();
            if (live >= options.max_solver_owned_bytes) {
                abandon_selective_completion_service(
                    "censored_no_headroom");
                return true;
            }
            selective_service_calc = std::make_unique<CalcContext>(
                calc.shared_session(), calc.goal(), calc.registry(),
                calc.candidates(), false, false, false, std::nullopt,
                std::vector<CountObservation>{}, false,
                std::vector<std::uint64_t>{}, true);
            check_memory();
            SolveOptions allowance = options;
            allowance.max_solver_owned_bytes =
                options.max_solver_owned_bytes - live;
            selective_service_producer =
                std::make_unique<SelectiveCompletionProducer>(
                    *selective_service_calc, exact_start_item,
                    prices, allowance,
                    SelectiveCompletionVariant::RerollVersusRepair);
            selective_service_phase =
                SelectiveServicePhase::Generating;
            result.diagnostics.selective_completion_service_status =
                "generation_started";
            record_progress_event("selective_completion_started");
            check_memory();
            return false;
        }
        if (selective_service_phase ==
                SelectiveServicePhase::Generating) {
            if (!selective_service_producer->advance(1)) {
                charge_private_calc();
                check_memory();
                return false;
            }
            charge_private_calc();
            if (!selective_service_producer->candidate()) {
                const std::string status =
                    selective_service_producer->status();
                abandon_selective_completion_service(
                    status.c_str());
                return true;
            }
            selective_service_candidate =
                *selective_service_producer->candidate();
            selective_service_producer.reset();
            selective_service_graph = compile_finder_control_json(
                *selective_service_calc, exact_start_item,
                selective_service_candidate->control, options);
            const auto* old =
                prune_and_select_certified_fallback();
            if (old != nullptr &&
                old->compiled_artifact.strategy_json ==
                    selective_service_graph) {
                abandon_selective_completion_service(
                    "deduplicated_identical_checked_graph");
                return true;
            }
            FinderCandidatePreparation prepared =
                prepare_finder_candidate(*selective_service_calc,
                    selective_service_calc->shared_session(), exact_start_item,
                    selective_service_graph,
                    &selective_service_candidate->control);
            if (!prepared.ready()) {
                abandon_selective_completion_service(
                    ("refused_binding:" + prepared.refusal).c_str());
                return true;
            }
            selective_service_strategy =
                std::move(prepared.strategy);
            selective_service_economy =
                std::make_shared<EconomyImpl>();
            selective_service_economy->id =
                "current-selective-completion";
            selective_service_economy->prices = prices;
            check_memory();
            const std::uint64_t headroom =
                options.max_solver_owned_bytes -
                    fast_estimated_owned_bytes();
            if (headroom < 1024 * 1024) {
                abandon_selective_completion_service(
                    "censored_no_evaluator_headroom");
                return true;
            }
            StrategyEvalOptions eval;
            eval.economy = selective_service_economy;
            eval.epsilon = 1e-12;
            eval.max_sweeps = options.max_sweeps;
            eval.max_states = std::min(
                options.max_discovered_states,
                options.candidate_evaluation_limits.max_states == 0
                    ? options.max_discovered_states
                    : options.candidate_evaluation_limits.max_states);
            eval.max_pairs = static_cast<std::uint32_t>(
                std::min<std::uint64_t>(
                    options.max_state_action_rows,
                    options.candidate_evaluation_limits.max_pairs == 0
                        ? options.max_state_action_rows
                        : options.candidate_evaluation_limits.max_pairs));
            eval.max_transitions = static_cast<std::uint32_t>(
                std::min<std::uint64_t>(
                    options.max_transitions,
                    options.candidate_evaluation_limits.max_transitions == 0
                        ? options.max_transitions
                        : options.candidate_evaluation_limits.max_transitions));
            eval.max_owned_bytes = std::min<std::uint64_t>(
                headroom,
                options.candidate_evaluation_limits.max_owned_bytes == 0
                    ? headroom
                    : options.candidate_evaluation_limits.max_owned_bytes);
            eval.max_reforge_work = options.max_reforge_work -
                std::min(options.max_reforge_work,
                    calc.telemetry().reforge_logical_work_v1);
            eval.max_output_json_bytes =
                options.max_strategy_json_bytes;
            eval.continuation_entries.push_back(
                {result.start_state, 0, 1, exact_start_item, false});
            eval.graph_local_provenance.strategy_json =
                selective_service_graph;
            for (std::size_t node = 0;
                 node < selective_service_candidate->control.nodes.size();
                 ++node) {
                const FinderControlNode& control_node =
                    selective_service_candidate->control.nodes[node];
                if (control_node.kind !=
                    FinderControlKind::RunNativeProgram) continue;
                const FinderProgramBinding& binding =
                    selective_service_candidate->control.programs.at(
                        control_node.binding);
                const auto key = planner_operator_semantic_key(
                    selective_service_calc->operators().at(
                        binding.operator_index));
                const std::string id = "c" + std::to_string(node);
                eval.graph_local_provenance.decisions.push_back(
                    {id, key, false, false});
                StrategyPolicyDecisionRequest request;
                request.compiled_node_id = id;
                request.selected_operator_identity = key;
                request.graph_local = true;
                eval.policy_decision_entries.push_back(
                    std::move(request));
            }
            selective_service_checker =
                std::make_unique<StrategyEvalWork>(
                    selective_service_strategy, eval);
            selective_service_phase =
                SelectiveServicePhase::Checking;
            result.diagnostics.selective_completion_service_status =
                "checking";
            check_memory();
            return false;
        }
        if (selective_service_phase ==
                SelectiveServicePhase::Checking) {
            if (!selective_service_checker->progress().done)
                selective_service_checker->step(1);
            charge_checker();
            check_memory();
            if (!selective_service_checker->progress().done)
                return false;
            if (!finder_evaluation_accepted(
                    selective_service_checker->result())) {
                ++result.diagnostics
                    .selective_completion_service_checks;
                abandon_selective_completion_service(
                    "refused_exact_graph_check");
                return true;
            }
            const std::uint64_t live =
                fast_estimated_owned_bytes();
            if (live >= options.max_solver_owned_bytes) {
                abandon_selective_completion_service(
                    "censored_no_admission_headroom");
                return true;
            }
            SolveOptions admission_limits = options;
            admission_limits.max_solver_owned_bytes =
                options.max_solver_owned_bytes - live;
            selective_service_validator =
                std::make_unique<SelectiveProgrammeEntryValidator>(
                    *selective_service_calc,
                    selective_service_calc->shared_session(),
                    selective_service_candidate->control,
                    selective_service_checker->result().policy_entries,
                    prices, admission_limits);
            selective_service_phase =
                SelectiveServicePhase::Validating;
            result.diagnostics.selective_completion_service_status =
                "validating_reached_entries";
            check_memory();
            return false;
        }
        if (selective_service_phase ==
                SelectiveServicePhase::Validating) {
            const bool complete =
                selective_service_validator->advance(1);
            charge_validator();
            check_memory();
            if (!complete) return false;
            selective_service_validator.reset();
            const double cost = selective_service_checker->result()
                .total_expected_cost;
            ++result.diagnostics.selective_completion_service_checks;
            result.diagnostics.selective_completion_service_checked_cost =
                cost;
            const auto* old =
                prune_and_select_certified_fallback();
            if (old != nullptr &&
                !(cost < old->evaluated_policy_cost)) {
                abandon_selective_completion_service(
                    "checked_rejected_expensive");
                return true;
            }
            refinement::CompiledPolicyAssertion assertion;
            assertion.strategy_json =
                std::move(selective_service_graph);
            assertion.certification_strategy_json =
                assertion.strategy_json;
            assertion.evaluation =
                selective_service_checker->take_result();
            selective_service_checker.reset();
            BoundedPolicyIncumbent candidate;
            candidate.compiled_artifact =
                retained_artifact_from_assertion(assertion);
            candidate.compiled_artifact
                .policy_decision_bindings.clear();
            candidate.compiled_root_entry_only = true;
            candidate.strict_state_provenance = false;
            candidate.values.assign(calc.state_count(), kInfinity);
            candidate.values[result.start_state] = cost;
            candidate.policy.resize(calc.state_count());
            candidate.policy_reachable.assign(calc.state_count(), 0);
            candidate.policy_rows.assign(calc.state_count(),
                std::numeric_limits<std::uint64_t>::max());
            candidate.certified_upper_bound = cost;
            candidate.evaluated_policy_cost = cost;
            candidate.kind = "selective_completion_root";
            candidate.compilation_provenance =
                "shared_native_original_root_control_v1";
            candidate.goal_identity = goal_identity();
            candidate.economy_identity = economy_identity();
            candidate.action_vocabulary_identity =
                action_vocabulary_identity();
            candidate.action_vocabulary_size =
                operators.size();
            candidate.caller_scope_identity =
                caller_scope_identity();
            candidate.artifact_identity = artifact_identity();
            candidate.graph_identity = graph_identity();
            candidate.source_generation =
                transition_cache->rows.size();
            candidate.target_generation = calc.state_count();
            candidate.graph_row_count =
                transition_cache->rows.size();
            candidate.graph_priced_row_count = priced_rows.size();
            candidate.graph_successor_count =
                transition_cache->successors.size();
            candidate.graph_probability_count =
                transition_cache->probabilities.size();
            candidate.graph_choice_count =
                transition_cache->choices.size();
            candidate.graph_choice_successor_count =
                transition_cache->choice_successors.size();
            candidate.graph_choice_option_count =
                transition_cache->choice_options.size();
            candidate.graph_prefix_identity =
                incumbent_graph_prefix_identity(
                    candidate.graph_row_count,
                    candidate.graph_priced_row_count,
                    candidate.graph_successor_count,
                    candidate.graph_probability_count,
                    candidate.graph_choice_count,
                    candidate.graph_choice_successor_count,
                    candidate.graph_choice_option_count);
            candidate.independently_certified = true;
            candidate.independently_evaluated = true;
            candidate.proper = true;
            candidate.executable = true;
            candidate.reconciliation_absolute_delta = 0;
            candidate.reconciliation_relative_delta = 0;
            candidate.portfolio_identity =
                1469598103934665603ULL;
            identity_mix_string(candidate.portfolio_identity,
                candidate.compiled_artifact.strategy_json);
            identity_mix(candidate.portfolio_identity,
                candidate.caller_scope_identity);
            candidate.retained_owned_bytes =
                incumbent_owned_bytes(candidate);
            if (const char* reason =
                    retained_incumbent_invalid_reason(candidate)) {
                abandon_selective_completion_service(
                    (std::string("refused_root_certificate:") +
                        reason).c_str());
                return true;
            }
            if (!retain_certified_incumbent(candidate,
                    candidate.retained_owned_bytes)) {
                abandon_selective_completion_service(
                    "censored_or_rejected_portfolio");
                return true;
            }
            abandon_selective_completion_service("retained");
            return true;
        }
    } catch (const std::length_error& error) {
        charge_private_calc();
        charge_checker();
        charge_validator();
        abandon_selective_completion_service(
            (std::string("censored_capacity:") +
                error.what()).c_str());
        return true;
    } catch (const std::exception& error) {
        charge_private_calc();
        charge_checker();
        charge_validator();
        abandon_selective_completion_service(
            (std::string("refused:") + error.what()).c_str());
        return true;
    }
    return selective_service_phase == SelectiveServicePhase::Done;
}

} // namespace poecraft::solver
