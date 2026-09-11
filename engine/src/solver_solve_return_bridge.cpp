#include "solver_solve_types.hpp"
#include "solver_compile_contracts.hpp"
#include "solver_policy_refinement.hpp"
#include "solver_policy_refinement_helpers.hpp"

namespace poecraft::solver {

bool solve_detail::ordinary_return_bridge_item(
        const SessionImpl& session, const pc_item_state& item) {
    pc_item_state persistent = item;
    pc_item_clear_side(&persistent, PC_SIDE_PREFIX);
    pc_item_clear_side(&persistent, PC_SIDE_SUFFIX);
    pc_item_state empty;
    pc_item_clear(&empty);
    empty.rarity = PC_RARITY_RARE;
    if (exact_item_state_key(persistent) != exact_item_state_key(empty)) return false;
    for (const auto side : {PC_SIDE_PREFIX, PC_SIDE_SUFFIX}) {
        const auto count = side == PC_SIDE_PREFIX ? item.prefix_count : item.suffix_count;
        const auto* slots = side == PC_SIDE_PREFIX ? item.prefixes : item.suffixes;
        for (std::uint8_t i = 0; i < count; ++i) {
            const auto& slot = slots[i];
            if (slot.flags != 0 || slot.veiled_option_count != 0 ||
                slot.veiled_chosen_mod_id != PC_MOD_NONE ||
                slot.mod_id >= session.metamod_type.size() ||
                session.metamod_type[slot.mod_id] >= 0) return false;
        }
    }
    return true;
}

namespace {

constexpr std::uint64_t kReturnEvaluatorOwnedBytes = 1073741824;

struct ReturnOutcomeScope {
    CalcContext& calc;
    ~ReturnOutcomeScope() { calc.cancel_outcomes(); }
};

bool return_context_preserving_action(const ActionDescriptor& action) {
    if (action.synthetic || action_observes_modifier_offer(action)) return false;
    switch (action.params.type) {
    case ActionType::Exalt:
    case ActionType::Annul:
    case ActionType::Chaos:
    case ActionType::HarvestAugment:
    case ActionType::HarvestResist:
    case ActionType::HarvestReforge:
        return true;
    default: return false;
    }
}

solve_detail::CooperativeTask<StrategyEvalResult> evaluate_return_graph(
        CalcContext& budget_owner,
        const std::unordered_map<std::string, double>& prices,
        const std::string& graph,
        SolveOptions limits, // Own the allowance across suspension.
        std::vector<StrategyContinuationEntryRequest> entries = {},
        std::vector<StrategyPolicyDecisionRequest> decisions = {}) {
    auto parsed = compile_strategy_json(budget_owner.shared_session(), graph.data(), graph.size());
    auto economy = std::make_shared<EconomyImpl>();
    economy->id = "current-run-return-bridge";
    economy->prices = prices;
    const auto payload = refinement::strategy_impl_owned_bytes(*parsed) +
        refinement::economy_owned_bytes(economy->prices, economy->id.capacity());
    if (payload >= limits.max_solver_owned_bytes)
        throw SolverResourceLimit("max_solver_owned_bytes", limits.max_solver_owned_bytes);
    StrategyEvalOptions options;
    options.epsilon = 1e-12;
    options.max_sweeps = limits.max_sweeps;
    options.max_states = limits.max_discovered_states;
    options.max_pairs = static_cast<std::uint32_t>(std::min<std::uint64_t>(
        limits.max_state_action_rows, std::numeric_limits<std::uint32_t>::max()));
    options.max_transitions = static_cast<std::uint32_t>(std::min<std::uint64_t>(
        limits.max_transitions, std::numeric_limits<std::uint32_t>::max()));
    options.max_owned_bytes = std::min<std::uint64_t>(kReturnEvaluatorOwnedBytes,
        limits.max_solver_owned_bytes - payload);
    options.max_output_json_bytes = limits.max_strategy_json_bytes;
    options.max_reforge_work = limits.max_reforge_work;
    options.economy = economy;
    options.continuation_entries = std::move(entries);
    options.policy_decision_entries = std::move(decisions);
    StrategyEvalWork work(parsed, options);
    std::uint64_t charged_active = 0, charged_logical = 0;
    const auto charge = [&] {
        const auto& used = work.diagnostic_result();
        budget_owner.consume_reforge_work(used.reforge_work - charged_active,
            used.reforge_logical_work_v1 - charged_logical);
        charged_active = used.reforge_work;
        charged_logical = used.reforge_logical_work_v1;
    };
    while (!work.progress().done) {
        try { work.step(32); }
        catch (...) { charge(); throw; }
        charge();
        co_await solve_detail::CooperativeCheckpoint{payload + work.live_owned_bytes()};
    }
    co_return work.take_result();
}

bool complete_return_evaluation(const StrategyEvalResult& value, const bool excursion) {
    const double terminal = value.success_probability + (excursion ? value.stop_probability : 0.0);
    return value.converged && value.cost_complete &&
        std::isfinite(value.total_expected_cost) && value.total_expected_cost >= 0.0 &&
        std::abs(terminal - 1.0) <= 1e-9 &&
        value.failure_probability == 0.0 && value.action_not_applied_probability == 0.0 &&
        value.no_matching_edge_probability == 0.0 && value.unresolved_probability == 0.0 &&
        (excursion || value.stop_probability == 0.0);
}

} // namespace

solve_detail::CooperativeTask<bool> SolveWork::Impl::try_initial_return_bridges(
        const SolveResult& frozen_policy) {
    if (!options.high_impact_executable_uppers || !output_incumbent.has_value() ||
        !output_incumbent->independently_evaluated ||
        !output_incumbent->independently_certified || requested_bounded_finish ||
        !frozen_policy.has_exact_start_item ||
        options.allow_economic_restart ||
        frozen_policy.exact_start_item.prefix_count != 0 ||
        frozen_policy.exact_start_item.suffix_count != 0 ||
        !ordinary_return_bridge_item(calc.session(), frozen_policy.exact_start_item)) co_return false;

    const auto& base = *output_incumbent;
    if (certified_incumbent_invalid_reason(base) != nullptr ||
        frozen_policy.start_state != result.start_state ||
        frozen_policy.policy != base.policy ||
        frozen_policy.policy_reachable != base.policy_reachable ||
        frozen_policy.behavioral_representative_by_state != base.behavioral_representative_by_state)
        co_return false;
    const double old_cost = base.evaluated_policy_cost;
    const std::string& old_graph = base.compiled_artifact.strategy_json;
    // One current-run base stays immutable. The broader gated reforge
    // experiment did not complete its checker and is not an active proposal.
    std::uint32_t annul = kNoId;
    for (const auto index : calc.candidate_operators()) {
        const auto& op = calc.operators().at(index);
        if (op.kind == PlannerOperatorKind::Primitive &&
            calc.registry().actions.at(op.primitive_action).params.type == ActionType::Annul)
            annul = index;
    }
    if (annul == kNoId) co_return false;
    for (std::uint32_t state = 0; state < frozen_policy.policy_reachable.size(); ++state) {
        if (!frozen_policy.policy_reachable[state] ||
            calc.is_goal_state(calc.state(state))) continue;
        const auto op_index = frozen_policy.policy.at(state).index;
        if (op_index >= calc.operators().size()) co_return false;
        const auto& op = calc.operators().at(op_index);
        if (op.kind != PlannerOperatorKind::Primitive ||
            !return_context_preserving_action(calc.registry().actions.at(op.primitive_action)))
            co_return false;
    }
    bool retained_any = false;
    {
        constexpr ActionType trial_type = ActionType::Exalt;
        if (requested_bounded_finish) co_return false;
        try {
            std::uint32_t trial = kNoId;
            for (const auto row_id : state_row_indices(*transition_cache, result.start_state)) {
                const auto index = priced_rows[row_id].operator_index;
                if (index >= calc.operators().size()) continue;
                const auto& op = calc.operators().at(index);
                if (op.kind == PlannerOperatorKind::Primitive &&
                    calc.registry().actions.at(op.primitive_action).params.type == trial_type)
                    trial = index;
            }
            if (trial == kNoId || !calc.is_candidate_operator_admitted_for_state(result.start_state, trial))
                throw std::runtime_error("no completed admitted current root trial");
            if (frozen_policy.policy.at(frozen_policy.start_state).index == trial)
                co_return false; // Identical fixed decisions cannot improve.

            SolveResult proposal = frozen_policy;
            proposal.refined_policy_artifact = {};
            proposal.primitive_renewal_witness = {};
            proposal.policy_status = SolvePolicyStatus::BoundedFeasible;
            proposal.behavioral_representative_by_state.clear();
            proposal.values.assign(calc.state_count(), kInfinity);
            proposal.policy.assign(calc.state_count(), PolicyOperatorRef{});
            proposal.policy_reachable.assign(calc.state_count(), 0);
            proposal.goal_states.assign(calc.state_count(), 0);
            proposal.expanded.assign(calc.state_count(), 0);
            std::vector<std::uint32_t> walk{result.start_state};
            std::vector<pc_item_state> first_bridge_entries;
            proposal.policy_reachable[result.start_state] = 1;
            const auto scratch = [&] {
                return solve_result_owned_bytes(proposal) + walk.capacity() * sizeof(std::uint32_t) +
                    first_bridge_entries.capacity() * sizeof(pc_item_state);
            };
            const auto available_limits = [&](const std::uint64_t extra = 0) {
                SolveOptions limits = options;
                const auto held = estimated_owned_bytes() + scratch() + extra;
                if (held >= options.max_solver_owned_bytes)
                    throw SolverResourceLimit("max_solver_owned_bytes", options.max_solver_owned_bytes);
                limits.max_solver_owned_bytes = options.max_solver_owned_bytes - held;
                const auto spent = calc.telemetry().reforge_logical_work_v1;
                if (spent >= options.max_reforge_work)
                    throw SolverResourceLimit("max_reforge_work", options.max_reforge_work);
                limits.max_reforge_work = options.max_reforge_work - spent;
                return limits;
            };
            for (std::size_t cursor = 0; cursor < walk.size(); ++cursor) {
                const auto state = walk[cursor];
                if (calc.is_goal_state(calc.state(state))) {
                    proposal.goal_states[state] = 1;
                    proposal.values[state] = 0.0;
                    continue;
                }
                // Flags represent the entire coarse class. A materialized
                // representative alone cannot license a protected member.
                pc_item_state decision_item;
                if (calc.state(state).flags != 0 ||
                    !calc.materialize(state, decision_item) ||
                    !ordinary_return_bridge_item(calc.session(), decision_item))
                    throw std::runtime_error("selected decision has incompatible persistent, protected or offer context");
                std::uint32_t chosen = kNoId;
                bool bridge = false;
                if (state == result.start_state) chosen = trial;
                else {
                    auto source = state;
                    if (source < frozen_policy.behavioral_representative_by_state.size())
                        source = frozen_policy.behavioral_representative_by_state[source];
                    if (source < frozen_policy.policy_reachable.size() &&
                        frozen_policy.policy_reachable[source] && source < frozen_policy.policy.size())
                        chosen = frozen_policy.policy[source].index;
                    if (chosen == kNoId) {
                        if (calc.state(state).goal_progress_retry_basin != 0 ||
                            decision_item.prefix_count + decision_item.suffix_count == 0 ||
                            !calc.is_candidate_operator_admitted_for_state(state, annul))
                            throw std::runtime_error("uncovered entry has no admitted ordinary Rare Annul return");
                        chosen = annul;
                        bridge = true;
                        if (first_bridge_entries.empty()) first_bridge_entries.push_back(decision_item);
                    }
                }
                if (chosen >= calc.operators().size())
                    throw std::runtime_error("missing immutable old decision");
                if (!calc.is_candidate_operator_admitted_for_state(state, chosen))
                    throw std::runtime_error("selected decision is outside the current admitted control scope");
                const auto& op = calc.operators().at(chosen);
                if (op.kind != PlannerOperatorKind::Primitive ||
                    !return_context_preserving_action(calc.registry().actions.at(op.primitive_action)))
                    throw std::runtime_error("unsupported mandatory or observed old decision");
                ReturnOutcomeScope pending_outcome{calc};
                std::shared_ptr<const OutcomeDistribution> law;
                while (!calc.advance_outcomes(state, op.primitive_action,
                        options.goal_progress_gated_reforges, law, 1)) {
                    (void)available_limits(calc.outcome_cursor_bytes());
                    co_await solve_detail::CooperativeCheckpoint{scratch() + calc.outcome_cursor_bytes()};
                }
                if (!law || !law->supported || !law->applicable ||
                    !law->choice_groups.empty() || !law->choice_options.empty())
                    throw std::runtime_error("native selected row or observation is incomplete");
                if (state == result.start_state && law->goal_progress_gated)
                    throw std::runtime_error("initial trial unexpectedly requires mandatory retry control");
                proposal.values.resize(calc.state_count(), kInfinity);
                proposal.policy.resize(calc.state_count());
                proposal.policy_reachable.resize(calc.state_count(), 0);
                proposal.goal_states.resize(calc.state_count(), 0);
                proposal.expanded.resize(calc.state_count(), 0);
                proposal.policy[state] = PolicyOperatorRef{chosen};
                proposal.expanded[state] = 1;
                double mass = 0.0;
                for (const auto& exit : law->entries) {
                    if (!std::isfinite(exit.probability) || exit.probability < 0.0)
                        throw std::runtime_error("invalid positive-mass selected row");
                    if (exit.probability == 0.0) continue;
                    mass += exit.probability;
                    if (bridge) {
                        const auto& from = calc.state(state);
                        const auto& to = calc.state(exit.state);
                        if (to.rarity != from.rarity ||
                            to.prefix_count + to.suffix_count + 1 != from.prefix_count + from.suffix_count ||
                            to.goal_progress_retry_basin != 0)
                            throw std::runtime_error("Annul row does not strictly remove one affix at the same rarity");
                    }
                    if (!proposal.policy_reachable[exit.state]) {
                        proposal.policy_reachable[exit.state] = 1;
                        walk.push_back(exit.state);
                    }
                }
                if (std::abs(mass - 1.0) > 1e-12)
                    throw std::runtime_error("incomplete native selected probability mass");
                (void)available_limits();
                co_await solve_detail::CooperativeCheckpoint{scratch()};
            }

            // Request only the exact root and one actual uncovered item needed
            // for this witness. It is not a census or a representative-class
            // certificate. Complete emitted evaluation below covers the whole
            // selected law, including every represented member.
            std::vector<StrategyContinuationEntryRequest> requests{
                {0, 0, 1, frozen_policy.exact_start_item, false}};
            if (!first_bridge_entries.empty()) requests.push_back({1, 0, 1, first_bridge_entries.front(), false});
            std::vector<StrategyPolicyDecisionRequest> root_decision;
            for (const auto& binding : base.compiled_artifact.policy_decision_bindings)
                if (binding.coarse_state == frozen_policy.start_state)
                    root_decision.push_back({binding.compiled_node_id, binding.coarse_state,
                        binding.selected_operator, binding.coarse_state_identity,
                        binding.selected_operator_identity, binding.fixed_observed_choice_policy});
            auto entry_work = evaluate_return_graph(calc, prices, old_graph,
                available_limits(), std::move(requests), std::move(root_decision));
            while (!entry_work.resume())
                co_await solve_detail::CooperativeCheckpoint{scratch() + entry_work.retained_bytes()};
            auto entry_eval = entry_work.take_result();
            entry_work.reset();
            if (entry_eval.continuation_upper.members.empty())
                throw std::runtime_error("exact anchor continuation is absent");
            const auto& root_member = entry_eval.continuation_upper.members.front();
            if (!root_member.available() ||
                std::abs(root_member.exact_continuation_upper - old_cost) > 1e-9 * std::max(1.0, old_cost))
                throw std::runtime_error("current exact anchor continuation was not independently certified");
            bool root_routable = false;
            for (const auto& entry : entry_eval.policy_entries.entries)
                if (entry.exact_item_identity == exact_item_state_key(frozen_policy.exact_start_item))
                    root_routable = root_routable || entry.globally_routable();
            if (!root_routable) throw std::runtime_error("exact anchor operation is not globally routable");
            entry_eval = {};

            PolicyCompilationTelemetry compilation;
            auto limits = available_limits();
            proposal.options = options;
            std::string repeated = compile_policy_strategy_json(calc, proposal,
                "Current-run paid return policy", &compilation, limits.max_strategy_json_bytes,
                nullptr, limits.max_solver_owned_bytes, PolicyRouteDefaultMode::CertificationFailClosed);
            std::string initial_operation;
            for (const auto& binding : compilation.policy_decision_bindings)
                if (binding.coarse_state == proposal.start_state) initial_operation = binding.compiled_node_id;
            if (initial_operation.empty()) throw std::runtime_error("compiled trial has no exact root binding");
            compilation = {};
            std::string one = compile_first_return_strategy_json(repeated, old_graph,
                initial_operation, frozen_policy.exact_start_item, FirstReturnCompilationMode::OneShot,
                available_limits(repeated.capacity()));
            auto one_work = evaluate_return_graph(calc, prices, one,
                available_limits(repeated.capacity() + one.capacity()));
            while (!one_work.resume())
                co_await solve_detail::CooperativeCheckpoint{scratch() + repeated.capacity() + one.capacity() + one_work.retained_bytes()};
            auto one_eval = one_work.take_result();
            one_work.reset();
            if (!complete_return_evaluation(one_eval, false))
                throw std::runtime_error("one-shot native controller is not complete and proper");
            const double one_cost = one_eval.total_expected_cost;
            one_eval = {};
            std::string excursion = compile_first_return_strategy_json(repeated, old_graph,
                initial_operation, frozen_policy.exact_start_item, FirstReturnCompilationMode::PrivateExcursion,
                available_limits(repeated.capacity() + one.capacity()));
            auto excursion_work = evaluate_return_graph(calc, prices, excursion,
                available_limits(repeated.capacity() + one.capacity() + excursion.capacity()));
            while (!excursion_work.resume())
                co_await solve_detail::CooperativeCheckpoint{scratch() + repeated.capacity() + one.capacity() + excursion.capacity() + excursion_work.retained_bytes()};
            auto excursion_eval = excursion_work.take_result();
            excursion_work.reset();
            if (!complete_return_evaluation(excursion_eval, true))
                throw std::runtime_error("first-return excursion does not have a complete transient law");
            const double r = excursion_eval.total_expected_cost;
            const double q = excursion_eval.stop_probability;
            if (std::abs(one_cost - (r + q * old_cost)) > 1e-9 * std::max(1.0, one_cost))
                throw std::runtime_error("one-shot and native first-return law disagree");
            const double escape = excursion_eval.success_probability;
            excursion_eval = {};
            if (!(q < 1.0) || !(escape > 0.0) || !std::isfinite(r / escape))
                throw std::runtime_error("repetition has no certified goal escape");
            proposal.evaluated_policy_cost = r / escape;
            proposal.upper_bound = proposal.evaluated_policy_cost;
            if (!(proposal.upper_bound < old_cost))
                throw std::runtime_error("complete selected return controller is not cheaper");
            // The full ordinary graph is the authority. The ratio only proposes
            // its value and must reconcile with independent native evaluation.
            auto assertion_limits = available_limits(repeated.capacity() + one.capacity() + excursion.capacity());
            const auto retained_solver = estimated_retained_solver_bytes(calc, &proposal);
            assertion_limits.max_solver_owned_bytes = retained_solver +
                std::min(assertion_limits.max_solver_owned_bytes, kReturnEvaluatorOwnedBytes);
            refinement::CompiledPolicyAssertionWork work(calc, proposal, prices,
                assertion_limits, "Current-run paid return policy");
            std::uint64_t charged_active = 0, charged_logical = 0;
            while (!work.progress().done) {
                work.step(32);
                const auto& used = work.diagnostic_evaluation();
                calc.consume_reforge_work(used.reforge_work - charged_active,
                    used.reforge_logical_work_v1 - charged_logical);
                charged_active = used.reforge_work;
                charged_logical = used.reforge_logical_work_v1;
                co_await solve_detail::CooperativeCheckpoint{scratch() + repeated.capacity() + one.capacity() + excursion.capacity() + work.retained_bytes()};
            }
            auto checked = work.take_result();
            if (!checked.executable || !checked.proper || !checked.zero_off_policy ||
                !checked.cost_reconciled || !checked.evaluation.cost_complete)
                throw std::runtime_error("repeated emitted controller refused: " + checked.failure_reason);
            const double repeated_cost = checked.exact_cost;
            if (!(repeated_cost < old_cost)) throw std::runtime_error("independent repeated controller does not improve");
            BoundedPolicyIncumbent improved = base;
            improved.kind = "certified_return_bridge";
            improved.values = proposal.values;
            improved.values[proposal.start_state] = repeated_cost;
            improved.policy = proposal.policy;
            improved.policy_reachable = proposal.policy_reachable;
            improved.behavioral_representative_by_state.clear();
            improved.target_generation = calc.state_count();
            improved.primitive_renewal_witness = {};
            improved.policy_rows.assign(proposal.policy.size(), std::numeric_limits<std::uint64_t>::max());
            improved.frontier_operators.resize(proposal.policy.size(), kNoId);
            for (std::size_t state = 0; state < proposal.policy.size(); ++state)
                if (proposal.policy_reachable[state]) improved.frontier_operators[state] = proposal.policy[state].index;
            improved.policy_materialized = true;
            improved.certified_upper_bound = repeated_cost;
            improved.evaluated_policy_cost = repeated_cost;
            improved.compiled_artifact = retained_artifact_from_assertion(checked);
            improved.compilation_provenance = "independent_current_run_return_bridge_v1";
            improved.independently_certified = improved.independently_evaluated = true;
            improved.proper = improved.executable = true;
            improved.reconciliation_absolute_delta = checked.absolute_cost_delta;
            improved.reconciliation_relative_delta = checked.relative_cost_delta;
            identity_mix_string(improved.portfolio_identity, improved.compiled_artifact.strategy_json);
            identity_mix(improved.portfolio_identity, improved.target_generation);
            improved.retained_owned_bytes = incumbent_owned_bytes(improved);
            if (!retain_certified_incumbent(improved, scratch()))
                throw std::runtime_error("verified return graph did not fit the existing portfolio");
            retained_any = true;
        } catch (const std::exception&) {
            calc.cancel_outcomes();
        }
    }
    co_return retained_any;
}

} // namespace poecraft::solver
