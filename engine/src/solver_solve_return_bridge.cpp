#include "solver_solve_types.hpp"
#include "solver_compile_contracts.hpp"
#include "solver_policy_refinement.hpp"
#include "solver_policy_refinement_helpers.hpp"
#include "solver_sparse_policy.hpp"

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

bool solve_detail::dirty_search_keeps_conversion(
        const SessionImpl& session, const AbstractLayout& layout,
        const ActionDescriptor& action) {
    if (action.params.type != ActionType::HarvestResist) return true;
    const auto resistance = session.data->tag_id_by_name.find("resistance");
    // Missing metadata is not evidence for removing a candidate. The native
    // conversion kernel only creates resistance modifiers with the target
    // class tag and without the source tag (solver_calc.cpp, HarvestResist).
    // This is a conservative proposal filter, never global action retirement.
    if (resistance == session.data->tag_id_by_name.end()) return true;
    const auto has_tag = [&](const std::uint32_t mod, const std::uint32_t tag) {
        return std::find(session.class_tag_ids.begin() + session.class_offsets[mod],
            session.class_tag_ids.begin() + session.class_offsets[mod + 1], tag) !=
            session.class_tag_ids.begin() + session.class_offsets[mod + 1];
    };
    for (const auto& slot : layout.slots)
        for (std::uint32_t mod = 0; mod < session.mod_count; ++mod)
            if (pc_bitset_test(slot.satisfying_mask.data(), mod) &&
                has_tag(mod, resistance->second) &&
                has_tag(mod, action.params.target_tag_id) &&
                !has_tag(mod, action.params.source_tag_id)) return true;
    return false;
}

namespace {

constexpr std::uint64_t kReturnEvaluatorOwnedBytes = 1073741824;

struct ReturnEvaluationReceipt {
    const char* stage = "construction";
    CandidateEvaluationLimits limits;
    StrategyEvalProgress progress;
    std::uint64_t peak_bytes = 0, live_bytes = 0, work = 0;
    double cost = std::numeric_limits<double>::infinity();
    double success = 0, stop = 0;
    bool complete = false;
};

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

bool dirty_fractured_bridge_item(const SessionImpl& session, const pc_item_state& item) {
    if (item.rarity != PC_RARITY_RARE && item.rarity != PC_RARITY_MAGIC) return false;
    auto ordinary = item;
    ordinary.rarity = PC_RARITY_RARE;
    for (const auto side : {PC_SIDE_PREFIX, PC_SIDE_SUFFIX}) {
        const auto count = side == PC_SIDE_PREFIX ? ordinary.prefix_count : ordinary.suffix_count;
        auto* slots = side == PC_SIDE_PREFIX ? ordinary.prefixes : ordinary.suffixes;
        for (std::uint8_t i = 0; i < count; ++i)
            slots[i].flags &= ~PC_MOD_SLOT_FRACTURED;
    }
    return solve_detail::ordinary_return_bridge_item(session, ordinary);
}

solve_detail::CooperativeTask<StrategyEvalResult> evaluate_return_graph(
        CalcContext& budget_owner,
        const std::unordered_map<std::string, double>& prices,
        const std::string& graph,
        SolveOptions limits, // Own the allowance across suspension.
        ReturnEvaluationReceipt& receipt, const char* stage,
        std::vector<StrategyContinuationEntryRequest> entries = {},
        std::vector<StrategyPolicyDecisionRequest> decisions = {}) {
    receipt = {};
    receipt.stage = stage;
    auto parsed = compile_strategy_json(budget_owner.shared_session(), graph.data(), graph.size());
    auto economy = std::make_shared<EconomyImpl>();
    economy->id = "current-run-return-bridge";
    economy->prices = prices;
    const auto payload = refinement::strategy_impl_owned_bytes(*parsed) +
        refinement::economy_owned_bytes(economy->prices, economy->id.capacity());
    if (payload >= limits.max_solver_owned_bytes)
        throw SolverResourceLimit("max_solver_owned_bytes", limits.max_solver_owned_bytes);
    StrategyEvalOptions options;
    const auto candidate_limits = resolved_candidate_evaluation_limits(limits,
        limits.max_solver_owned_bytes - payload, kReturnEvaluatorOwnedBytes);
    receipt.limits = candidate_limits;
    options.epsilon = 1e-12;
    options.max_sweeps = limits.max_sweeps;
    options.max_states = candidate_limits.max_states;
    options.max_pairs = candidate_limits.max_pairs;
    options.max_transitions = candidate_limits.max_transitions;
    options.max_owned_bytes = candidate_limits.max_owned_bytes;
    options.max_output_json_bytes = limits.max_strategy_json_bytes;
    options.max_reforge_work = limits.max_reforge_work;
    options.economy = economy;
    options.continuation_entries = std::move(entries);
    options.policy_decision_entries = std::move(decisions);
    StrategyEvalWork work(parsed, options);
    std::uint64_t charged_active = 0, charged_logical = 0;
    const auto charge = [&] {
        const auto& used = work.diagnostic_result();
        receipt.progress = work.progress();
        receipt.peak_bytes = work.peak_owned_bytes();
        receipt.live_bytes = work.live_owned_bytes();
        receipt.work = used.reforge_logical_work_v1;
        receipt.complete = receipt.progress.done && used.converged && used.cost_complete;
        if (receipt.complete) {
            receipt.cost = used.total_expected_cost;
            receipt.success = used.success_probability;
            receipt.stop = used.stop_probability;
        }
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

    // Portfolio retention can replace output_incumbent between proposals.
    // Own the immutable verified generation, and charge its overlap below.
    if (incumbent_owned_bytes(*output_incumbent) >=
        options.max_solver_owned_bytes - std::min(options.max_solver_owned_bytes, estimated_owned_bytes()))
        co_return false;
    const auto base = *output_incumbent;
    if (certified_incumbent_invalid_reason(base) != nullptr ||
        frozen_policy.start_state != result.start_state ||
        frozen_policy.policy != base.policy ||
        frozen_policy.policy_reachable != base.policy_reachable ||
        frozen_policy.behavioral_representative_by_state != base.behavioral_representative_by_state)
        co_return false;
    const double old_cost = base.evaluated_policy_cost;
    const std::string& old_graph = base.compiled_artifact.strategy_json;
    // Both trials use this run's verified generation and native rows.
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
    for (const ActionType trial_type : {ActionType::Exalt, ActionType::Chaos}) {
        if (requested_bounded_finish) break;
        const char* trial_name = trial_type == ActionType::Exalt ? "exalt" : "gated_chaos";
        // This initial diagnostic changes only the already-defined controller
        // and checker allowance. Ordinary defaults retain the Exalt proposal.
        if (trial_type == ActionType::Chaos && options.native_continuation_search !=
            NativeContinuationSearchMode::GatedReturnProbe) continue;
        const auto started = std::chrono::steady_clock::now();
        ReturnEvaluationReceipt evaluation;
        std::uint64_t bridge_rows = 0, bridge_transitions = 0;
        double one_cost = kInfinity, r = kInfinity, q = kInfinity, repeated_cost = kInfinity;
        std::optional<std::size_t> progress_sample;
        const char* progress_stage = nullptr;
        auto last_progress = started;
        const auto record = [&](const char* disposition, const std::string& reason = "") {
            auto& telemetry = result.diagnostics.policy_refinement;
            const auto number = solve_detail::diagnostic_finite_double;
            std::string sample = "{\"kind\":\"current_run_return_trial\",\"trial\":\"" + std::string(trial_name) +
                "\",\"stage\":\"" + evaluation.stage + "\",\"disposition\":\"" + disposition +
                "\",\"reason\":\"" + solve_detail::diagnostic_json_escape(reason) +
                "\",\"old_cost\":" + number(old_cost) + ",\"one_cost\":" + number(one_cost) +
                ",\"r\":" + number(r) + ",\"q\":" + number(q) +
                ",\"repeated_cost\":" + number(repeated_cost) +
                ",\"bridge_rows\":" + std::to_string(bridge_rows) +
                ",\"bridge_transitions\":" + std::to_string(bridge_transitions) +
                ",\"eval_states\":" + std::to_string(evaluation.progress.exact_states) +
                ",\"eval_pairs\":" + std::to_string(evaluation.progress.discovered_pairs + evaluation.progress.pending_pairs) +
                ",\"eval_transitions\":" + std::to_string(evaluation.progress.stored_transitions) +
                ",\"eval_peak_bytes\":" + std::to_string(evaluation.peak_bytes) +
                ",\"eval_live_bytes\":" + std::to_string(evaluation.live_bytes) +
                ",\"eval_work\":" + std::to_string(evaluation.work) +
                ",\"eval_complete\":" + (evaluation.complete ? "true" : "false") +
                ",\"eval_cost\":" + number(evaluation.cost) +
                ",\"eval_success\":" + number(evaluation.success) + ",\"eval_stop\":" + number(evaluation.stop) +
                ",\"eval_subphase\":\"" + strategy_eval_subphase_name(evaluation.progress.subphase) +
                "\",\"max_states\":" + std::to_string(evaluation.limits.max_states) +
                ",\"max_pairs\":" + std::to_string(evaluation.limits.max_pairs) +
                ",\"max_transitions\":" + std::to_string(evaluation.limits.max_transitions) +
                ",\"max_owned_bytes\":" + std::to_string(evaluation.limits.max_owned_bytes) +
                ",\"wall_seconds\":" + number(std::chrono::duration<double>(
                    std::chrono::steady_clock::now() - started).count()) + "}";
            if (progress_sample && progress_stage == evaluation.stage) {
                auto& previous = telemetry.publication_candidate_samples.at(*progress_sample);
                const auto shared = telemetry.publication_candidate_sample_bytes +
                    telemetry.structural_failure_sample_bytes + telemetry.evaluator_memory_sample_bytes +
                    telemetry.direct_offpolicy_state_sample_bytes - previous.size();
                const auto cap = options.max_telemetry_json_bytes / 4;
                if (sample.size() <= cap && shared <= cap - sample.size()) {
                    telemetry.publication_candidate_sample_bytes += sample.size() - previous.size();
                    previous = std::move(sample);
                }
            } else {
                const auto before = telemetry.publication_candidate_samples.size();
                retain_bounded_json_sample(telemetry.publication_candidate_samples,
                    telemetry.publication_candidate_samples_omitted,
                    telemetry.publication_candidate_sample_bytes, std::move(sample));
                if (telemetry.publication_candidate_samples.size() > before) {
                    progress_sample = before;
                    progress_stage = evaluation.stage;
                }
            }
        };
        const auto record_progress = [&] {
            const auto now = std::chrono::steady_clock::now();
            if (now - last_progress >= std::chrono::seconds(1)) {
                record("in_progress");
                last_progress = now;
            }
        };
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
                continue; // Identical fixed decisions cannot improve.
            if (trial_type == ActionType::Chaos && !options.goal_progress_gated_reforges)
                throw std::runtime_error("selected reforge trial requires the admitted gated scope");

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
            std::uint32_t trial_retry = kNoId;
            bool sampled_dirty_decision = false;
            proposal.policy_reachable[result.start_state] = 1;
            const auto scratch = [&] {
                return incumbent_owned_bytes(base) + solve_result_owned_bytes(proposal) + walk.capacity() * sizeof(std::uint32_t) +
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
                if (state == result.start_state || state == trial_retry) chosen = trial;
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
                    trial_retry = law->gated_retry_state;
                proposal.values.resize(calc.state_count(), kInfinity);
                proposal.policy.resize(calc.state_count());
                proposal.policy_reachable.resize(calc.state_count(), 0);
                proposal.goal_states.resize(calc.state_count(), 0);
                proposal.expanded.resize(calc.state_count(), 0);
                proposal.policy[state] = PolicyOperatorRef{chosen};
                proposal.expanded[state] = 1;
                if (bridge) {
                    ++bridge_rows;
                    bridge_transitions += law->entries.size();
                }
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
                const auto source_goals = std::popcount(satisfied_goal_mask_for_state(state));
                const auto source_affixes = decision_item.prefix_count + decision_item.suffix_count;
                if (!sampled_dirty_decision && source_goals > 0 && source_affixes > source_goals) {
                    sampled_dirty_decision = true;
                    double loses_goal = 0, gains_goal = 0, true_goal = 0, debt_progress = 0;
                    for (const auto& e : law->entries) {
                        const auto next_goals = std::popcount(satisfied_goal_mask_for_state(e.state));
                        if (next_goals < source_goals) loses_goal += e.probability;
                        if (next_goals > source_goals) gains_goal += e.probability;
                        if (calc.is_goal_state(calc.state(e.state))) true_goal += e.probability;
                        if (joint_policy_terminal_debt(e.state) < joint_policy_terminal_debt(state))
                            debt_progress += e.probability;
                    }
                    auto& telemetry = result.diagnostics.policy_refinement;
                    const auto number = solve_detail::diagnostic_finite_double;
                    std::string identity = "[";
                    for (const auto word : exact_abstract_state_key(calc.state(state), 0)) {
                        if (identity.size() > 1) identity += ',';
                        identity += "\"" + std::to_string(word) + "\"";
                    }
                    identity += ']';
                    std::string alternatives = "[";
                    for (const auto& priced : operators) {
                        const auto index = priced.index;
                        if (!calc.is_candidate_operator_admitted_for_state(state, index)) continue;
                        const auto& candidate = calc.operators()[index];
                        if (candidate.kind != PlannerOperatorKind::Primitive ||
                            calc.registry().actions[candidate.primitive_action].synthetic ||
                            !action_legal(calc.session(), calc.registry().actions[candidate.primitive_action], calc.state(state)))
                            continue;
                        bool complete = false;
                        if (state < transition_cache->state_rows.size())
                            for (const auto row : state_row_indices(*transition_cache, state))
                                complete |= priced_rows[row].operator_index == index && joint_policy_row_completed(row);
                        if (alternatives.size() > 1) alternatives += ',';
                        alternatives += "{\"action\":\"" + solve_detail::diagnostic_json_escape(
                            calc.registry().actions[candidate.primitive_action].id) +
                            "\",\"complete_search_row\":" + (complete ? "true" : "false") + "}";
                    }
                    alternatives += ']';
                    retain_bounded_json_sample(telemetry.publication_candidate_samples,
                        telemetry.publication_candidate_samples_omitted, telemetry.publication_candidate_sample_bytes,
                        "{\"kind\":\"dirty_return_decision\",\"trial\":\"" + std::string(trial_name) +
                        "\",\"source_identity\":" + identity + ",\"goal_mask\":" +
                        std::to_string(satisfied_goal_mask_for_state(state)) + ",\"prefixes\":" +
                        std::to_string(decision_item.prefix_count) + ",\"suffixes\":" +
                        std::to_string(decision_item.suffix_count) + ",\"flags\":" +
                        std::to_string(calc.state(state).flags) + ",\"blocked_mask\":" +
                        std::to_string(calc.state(state).blocked_mask) + ",\"debt\":" +
                        std::to_string(joint_policy_terminal_debt(state)) + ",\"selected\":\"" +
                        solve_detail::diagnostic_json_escape(calc.registry().actions[op.primitive_action].id) +
                        "\",\"selection_origin\":\"" + (bridge ? "uncovered_paid_return" : "frozen_verified_policy") +
                        "\",\"native_row_complete\":true,\"loses_goal_probability\":" + number(loses_goal) +
                        ",\"gains_goal_probability\":" + number(gains_goal) + ",\"true_goal_probability\":" + number(true_goal) +
                        ",\"debt_progress_probability\":" + number(debt_progress) + ",\"available\":" + alternatives + "}");
                }
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
                available_limits(), evaluation, "anchor_entries", std::move(requests), std::move(root_decision));
            while (!entry_work.resume())
                co_await solve_detail::CooperativeCheckpoint{scratch() + entry_work.retained_bytes()};
            record("evaluation_complete");
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
                available_limits(repeated.capacity() + one.capacity()), evaluation, "one_shot");
            while (!one_work.resume()) {
                record_progress();
                co_await solve_detail::CooperativeCheckpoint{scratch() + repeated.capacity() + one.capacity() + one_work.retained_bytes()};
            }
            auto one_eval = one_work.take_result();
            one_work.reset();
            if (!complete_return_evaluation(one_eval, false))
                throw std::runtime_error("one-shot native controller is not complete and proper");
            one_cost = one_eval.total_expected_cost;
            record("evaluation_complete");
            one_eval = {};
            std::string excursion = compile_first_return_strategy_json(repeated, old_graph,
                initial_operation, frozen_policy.exact_start_item, FirstReturnCompilationMode::PrivateExcursion,
                available_limits(repeated.capacity() + one.capacity()));
            auto excursion_work = evaluate_return_graph(calc, prices, excursion,
                available_limits(repeated.capacity() + one.capacity() + excursion.capacity()), evaluation, "excursion");
            while (!excursion_work.resume()) {
                record_progress();
                co_await solve_detail::CooperativeCheckpoint{scratch() + repeated.capacity() + one.capacity() + excursion.capacity() + excursion_work.retained_bytes()};
            }
            auto excursion_eval = excursion_work.take_result();
            excursion_work.reset();
            if (!complete_return_evaluation(excursion_eval, true))
                throw std::runtime_error("first-return excursion does not have a complete transient law");
            r = excursion_eval.total_expected_cost;
            q = excursion_eval.stop_probability;
            record("evaluation_complete");
            if (std::abs(one_cost - (r + q * old_cost)) > 1e-9 * std::max(1.0, one_cost))
                throw std::runtime_error("one-shot and native first-return law disagree");
            const double escape = excursion_eval.success_probability;
            excursion_eval = {};
            if (!(q < 1.0) || !(escape > 0.0) || !std::isfinite(r / escape))
                throw std::runtime_error("repetition has no certified goal escape");
            repeated_cost = r / escape;
            proposal.evaluated_policy_cost = repeated_cost;
            proposal.upper_bound = proposal.evaluated_policy_cost;
            if (!(proposal.upper_bound < old_cost))
                throw std::runtime_error("complete selected return controller is not cheaper");
            // The full ordinary graph is the authority. The ratio only proposes
            // its value and must reconcile with independent native evaluation.
            auto assertion_limits = available_limits(repeated.capacity() + one.capacity() + excursion.capacity());
            const auto retained_solver = estimated_retained_solver_bytes(calc, &proposal);
            const auto assertion_budget = resolved_candidate_evaluation_limits(assertion_limits,
                assertion_limits.max_solver_owned_bytes, kReturnEvaluatorOwnedBytes);
            assertion_limits.max_solver_owned_bytes = retained_solver +
                assertion_budget.max_owned_bytes;
            evaluation = {};
            evaluation.stage = "repeated_assertion";
            evaluation.limits = assertion_budget;
            refinement::CompiledPolicyAssertionWork work(calc, proposal, prices,
                assertion_limits, "Current-run paid return policy");
            std::uint64_t charged_active = 0, charged_logical = 0;
            while (!work.progress().done) {
                work.step(32);
                const auto& used = work.diagnostic_evaluation();
                evaluation.progress = work.progress().evaluation;
                evaluation.peak_bytes = used.peak_owned_bytes_estimate;
                evaluation.live_bytes = used.owned_bytes_estimate;
                evaluation.work = used.reforge_logical_work_v1;
                record_progress();
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
            repeated_cost = checked.exact_cost;
            evaluation.complete = true;
            evaluation.cost = repeated_cost;
            evaluation.success = checked.evaluation.success_probability;
            evaluation.stop = checked.evaluation.stop_probability;
            evaluation.progress = work.progress().evaluation;
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
            if (!retain_certified_incumbent(improved, scratch() + improved.retained_owned_bytes))
                throw std::runtime_error("verified return graph did not fit the existing portfolio");
            retained_any = true;
            record("retained");
        } catch (const std::exception& error) {
            calc.cancel_outcomes();
            record("refused", error.what());
        }
    }
    co_return retained_any;
}


solve_detail::CooperativeTask<bool> SolveWork::Impl::try_dirty_continuation_candidates() {
    using namespace solve_detail;
    if (!options.high_impact_executable_uppers || requested_bounded_finish ||
        !result.has_exact_start_item || options.allow_economic_restart ||
        !ordinary_return_bridge_item(calc.session(), exact_start_item)) co_return false;
    const auto mode = options.native_continuation_search;
    const bool restricted = mode != NativeContinuationSearchMode::DirtyFull;
    const bool preserve_layout = mode == NativeContinuationSearchMode::DirtyRestrictedFullLayout;
    std::optional<BoundedPolicyIncumbent> handoff_base;
    std::uint64_t handoff_base_bytes = 0;
    const auto parent_live_bytes = [&] {
        auto bytes = estimated_owned_bytes();
        // While this coroutine runs, its last checkpoint is a stale snapshot
        // of the same child/evaluator accounted explicitly below. Keep the
        // parent's frame and proof, replacing only that nested snapshot.
        if (publication_pipeline.initial_candidate_task) {
            const auto& task = *publication_pipeline.initial_candidate_task;
            bytes -= task.retained_bytes() - task.frame_bytes();
        }
        if (finalization_task) {
            // The ordinary-publication caller released its preceding checker
            // before entering this task. Its current nested checkpoint is the
            // same private child/evaluator now accounted explicitly below.
            bytes -= finalization_task->retained_bytes() - finalization_task->frame_bytes();
        }
        bytes += handoff_base_bytes;
        return bytes;
    };
    std::vector<std::uint32_t> candidates;
    std::vector<std::string> omitted;
    for (const auto action : calc.candidates()) {
        const auto& descriptor = calc.registry().actions.at(action);
        const bool omit = restricted &&
            !dirty_search_keeps_conversion(calc.session(), calc.layout(), descriptor);
        if (omit) omitted.push_back(descriptor.id);
        else candidates.push_back(action);
    }
    // Preserve the original modifier universe. Only observer actions change;
    // every fixed/conditional program dependency is rebuilt by CalcContext.
    std::vector<std::uint64_t> universe;
    const auto include = [&](const auto& mask) {
        universe.resize(std::max(universe.size(), mask.size()));
        for (std::size_t w=0;w<mask.size();++w) universe[w] |= mask[w];
    };
    for (const auto& slot : calc.layout().slots) include(slot.member_mask);
    for (const auto& cls : calc.layout().junk_classes) include(cls.member_mask);
    std::array<ActionType,2> root_trials{ActionType::Exalt,ActionType::Chaos};
    const auto root_acquisition_score = [&](const ActionType type) {
        double score=kInfinity;
        if (result.start_state>=transition_cache->state_rows.size()) return score;
        for (const auto row_id:state_row_indices(*transition_cache,result.start_state)) {
            if (!joint_policy_row_completed(row_id)) continue;
            const auto& priced=priced_rows.at(row_id);
            const auto& op=calc.operators().at(priced.operator_index);
            if (op.kind!=PlannerOperatorKind::Primitive ||
                calc.registry().actions.at(op.primitive_action).params.type!=type) continue;
            const auto& row=transition_cache->rows.at(row_id);
            if (row.choice_count!=0) continue;
            double progress_mass=0;
            const auto before=std::popcount(satisfied_goal_mask_for_state(result.start_state));
            for (std::uint32_t i=0;i<row.transition_count;++i) {
                const auto at=row.transition_offset+i;
                if (std::popcount(satisfied_goal_mask_for_state(transition_cache->successors.at(at)))>before)
                    progress_mass+=transition_cache->probabilities.at(at);
            }
            if (progress_mass>0) score=std::min(score,priced.cost/progress_mass);
        }
        return score;
    };
    // Complete current native rows supply an acquisition-cost ordering only.
    // They neither certify continuation values nor retire any action.
    std::stable_sort(root_trials.begin(),root_trials.end(),[&](const auto left,const auto right) {
        return root_acquisition_score(left)<root_acquisition_score(right);
    });
    struct Proposal {
        ActionType root_type;
        pc_item_state start;
        bool nonempty_handoff = false;
        unsigned minimum_progress = 0;
        double old_entry_cost = kInfinity;
        std::uint32_t root_registry_action = kNoId;
    };
    std::vector<Proposal> proposals;
    // A current compiled policy may have useful progress behind a prefix that
    // the empty-Rare proposals cannot reproduce economically. Ask its existing
    // native decision-entry owner for actual reached items, never saved recipes
    // or representative parent ids transplanted into a private layout.
    const auto* current = best_current_certified_fallback();
    if (current && certified_incumbent_invalid_reason(*current) == nullptr &&
        current->compiled_artifact.strategy_json.find("\"fracture\"") != std::string::npos &&
        !current->compiled_artifact.policy_decision_bindings.empty() &&
        incumbent_owned_bytes(*current) < options.max_solver_owned_bytes -
            std::min(options.max_solver_owned_bytes, parent_live_bytes())) {
        handoff_base = *current;
        handoff_base_bytes = incumbent_owned_bytes(*handoff_base);
        ReturnEvaluationReceipt entry_receipt;
        try {
            std::vector<StrategyPolicyDecisionRequest> requests;
            {
                const auto& graph = handoff_base->compiled_artifact.strategy_json;
                const auto remaining = options.max_solver_owned_bytes -
                    std::min(options.max_solver_owned_bytes,parent_live_bytes());
                if (graph.size() > remaining/128)
                    throw SolverResourceLimit("max_solver_owned_bytes",options.max_solver_owned_bytes);
                const auto parsed = compile_strategy_json(calc.shared_session(),graph.data(),graph.size());
                for (const auto& binding : handoff_base->compiled_artifact.policy_decision_bindings) {
                    const auto found = parsed->node_by_id.find(binding.compiled_node_id);
                    if (found == parsed->node_by_id.end()) continue;
                    const auto& node = parsed->nodes.at(found->second);
                    // Ask only decisions this contained continuation can
                    // replace. The whole original graph is still evaluated.
                    if (node.kind != StrategyNodeKind::Operation ||
                        (node.action.type != ActionType::Exalt && node.action.type != ActionType::Annul &&
                         node.action.type != ActionType::Scour)) continue;
                    requests.push_back({binding.compiled_node_id, binding.coarse_state,
                        binding.selected_operator, binding.coarse_state_identity,
                        binding.selected_operator_identity, binding.fixed_observed_choice_policy});
                }
            }
            const auto requested_nodes = requests.size();
            SolveOptions entry_limits = options;
            entry_limits.max_solver_owned_bytes -= std::min(entry_limits.max_solver_owned_bytes, parent_live_bytes());
            entry_limits.max_reforge_work -= std::min(entry_limits.max_reforge_work, calc.telemetry().reforge_logical_work_v1);
            auto task = evaluate_return_graph(calc, prices, handoff_base->compiled_artifact.strategy_json,
                entry_limits, entry_receipt, "nonempty_current_policy_entries", {}, std::move(requests));
            while (!task.resume()) co_await CooperativeCheckpoint{handoff_base_bytes + task.retained_bytes()};
            auto evaluated = task.take_result(); task.reset();
            if (!complete_return_evaluation(evaluated, false))
                throw std::runtime_error("current nonempty-entry controller failed independent evaluation");
            if (std::abs(evaluated.total_expected_cost-handoff_base->evaluated_policy_cost) >
                1e-9*std::max(1.0,handoff_base->evaluated_policy_cost))
                throw std::runtime_error("current nonempty-entry root cost no longer reconciles");
            double best_score = -1;
            double selected_visits = 0;
            std::string selected_node;
            std::optional<Proposal> selected;
            for (const auto& entry : evaluated.policy_entries.entries) {
                if (!entry.globally_routable() || entry.checkpoint_active || entry.observed_offer_active ||
                    !(entry.root_expected_visits > 0) || !dirty_fractured_bridge_item(calc.session(), entry.item)) continue;
                const auto state = project_item(calc.session(), calc.layout(), entry.item);
                const auto frozen = std::popcount(state.fractured_goal_mask);
                unsigned goals = 0;
                for (std::size_t i = 0; i < calc.layout().slots.size(); ++i)
                    goals += state.slot_status[i] == static_cast<std::uint8_t>(GoalSlotStatus::Satisfied);
                bool fractured_junk = false;
                for (std::size_t i = 0; i < state.fractured_junk_counts.size(); ++i)
                    fractured_junk |= state.fractured_junk_counts[i] != 0;
                if ((state.flags & ~kFlagFractured) != 0 || frozen != 1 || fractured_junk || goals <= frozen ||
                    goals >= calc.goal().required_satisfied_slots() ||
                    entry.item.rarity != PC_RARITY_RARE ||
                    entry.item.prefix_count + entry.item.suffix_count <= goals) continue;
                const double score = entry.root_expected_visits * entry.exact_continuation_upper;
                if (score > best_score) {
                    best_score = score;
                    selected = Proposal{ActionType::Exalt, entry.item, true,
                        static_cast<unsigned>(frozen + 1), entry.exact_continuation_upper};
                    selected_visits = entry.root_expected_visits;
                    selected_node = entry.compiled_node_id;
                }
            }
            if (selected) proposals.push_back(*selected);
            retain_bounded_json_sample(result.diagnostics.policy_refinement.publication_candidate_samples,
                result.diagnostics.policy_refinement.publication_candidate_samples_omitted,
                result.diagnostics.policy_refinement.publication_candidate_sample_bytes,
                "{\"kind\":\"dirty_nonempty_entry_search\",\"native_entries\":" +
                std::to_string(evaluated.policy_entries.entries.size()) +
                ",\"requested_decisions\":" + std::to_string(requested_nodes) + ",\"selected\":" +
                (selected ? "true" : "false") + ",\"compiled_node\":\"" + diagnostic_json_escape(selected_node) +
                "\",\"root_expected_visits\":" + diagnostic_finite_double(selected_visits) +
                ",\"old_entry_cost\":" + diagnostic_finite_double(selected ? selected->old_entry_cost : kInfinity) +
                ",\"prefixes\":" + std::to_string(selected ? selected->start.prefix_count : 0) +
                ",\"suffixes\":" + std::to_string(selected ? selected->start.suffix_count : 0) + "}");
        } catch (const std::exception& error) {
            retain_bounded_json_sample(result.diagnostics.policy_refinement.publication_candidate_samples,
                result.diagnostics.policy_refinement.publication_candidate_samples_omitted,
                result.diagnostics.policy_refinement.publication_candidate_sample_bytes,
                "{\"kind\":\"dirty_nonempty_entry_search\",\"selected\":false,\"reason\":\"" +
                diagnostic_json_escape(error.what()) + "\"}");
        }
        if (proposals.empty()) { handoff_base.reset(); handoff_base_bytes = 0; }
    }
    // The native registry already admits exact-goal forced modifiers. A
    // priced guaranteed acquisition is materially different from an ordinary
    // redraw; do not restrict the root proposal vocabulary to the old graph.
    // This score orders one proposal only. Its complete native law and final
    // controller cost are still required below.
    double forced_score = kInfinity;
    std::uint32_t forced_action = kNoId;
    for (const auto& priced : operators) {
        const auto& op = calc.operators().at(priced.index);
        if (op.kind != PlannerOperatorKind::Primitive ||
            !std::isfinite(priced.cost) || priced.cost < 0) continue;
        const auto& action = calc.registry().actions.at(op.primitive_action);
        if (action.params.type != ActionType::Essence ||
            action.params.essence_index >= calc.session().essence_guaranteed_mod_ids.size() ||
            std::find(candidates.begin(),candidates.end(),op.primitive_action) == candidates.end() ||
            !action_legal(calc.session(),action,calc.state(result.start_state))) continue;
        const auto mod = calc.session().essence_guaranteed_mod_ids[action.params.essence_index];
        unsigned guaranteed = 0;
        for (const auto& slot : calc.layout().slots)
            guaranteed += mod/64 < slot.satisfying_mask.size() &&
                ((slot.satisfying_mask[mod/64] >> (mod%64)) & 1ull) != 0;
        if (guaranteed && priced.cost/guaranteed < forced_score) {
            forced_score = priced.cost/guaranteed;
            forced_action = op.primitive_action;
        }
    }
    for (const auto type : root_trials) proposals.push_back({type, exact_start_item});
    // Preserve the existing completed-row proposal opportunity. A cheap
    // guaranteed acquisition can still have an expensive native controller
    // check; its ordering estimate must not starve an already productive
    // ordinary continuation. The forced proposal remains available afterward.
    if (forced_action != kNoId)
        proposals.push_back({ActionType::Essence,exact_start_item,false,0,kInfinity,forced_action});
    bool retained = false;
    for (const auto& proposal : proposals) {
        if (requested_bounded_finish) break;
        const auto root_type = proposal.root_type;
        const bool broad_root = root_type == ActionType::Chaos || root_type == ActionType::Essence;
        const char* root_name = proposal.nonempty_handoff ? "nonempty_exalt_annul" :
            proposal.root_registry_action != kNoId
                ? calc.registry().actions.at(proposal.root_registry_action).id.c_str()
                : root_type == ActionType::Exalt ? "exalt" : "gated_chaos";
        const auto begin = std::chrono::steady_clock::now();
        ReturnEvaluationReceipt evaluation;
        std::uint64_t constructed_rows = 0, continued_dirty = 0, paid_cleanup = 0;
        std::uint64_t paid_scour = 0, paid_redraw = 0, shared_redraw_rows = 0;
        std::uint64_t cleanup_alternatives = 0, improvement_rounds = 0;
        std::uint64_t child_work = 0, child_active = 0, child_peak = 0;
        double seed_cost = kInfinity, coarse_cost = kInfinity, exact_cost = kInfinity;
        std::uint64_t native_boundary_entries = 0;
        std::string entry_response = "[]";
        std::unique_ptr<CalcContext> private_calc;
        const auto charge_child = [&] {
            if (!private_calc) return;
            const auto& t = private_calc->telemetry();
            const auto active = t.reforge_frontier_work;
            const auto logical = t.reforge_logical_work_v1;
            calc.consume_reforge_work(active - child_active, logical - child_work);
            child_active = active; child_work = logical;
        };
        std::optional<std::size_t> sample_index;
        auto last_sample = begin;
        const auto record = [&](const char* disposition, const std::string& reason = "") {
            auto& t = result.diagnostics.policy_refinement;
            std::string excluded = "[";
            for (const auto& id : omitted) {
                if (excluded.size()>1) excluded += ',';
                excluded += "\"" + diagnostic_json_escape(id) + "\"";
            }
            excluded += ']';
            std::string sample = "{\"kind\":\"dirty_continuation_candidate\",\"root_action\":\"" +
                std::string(root_name) + "\",\"disposition\":\"" + disposition +
                "\",\"reason\":\"" + diagnostic_json_escape(reason) + "\",\"stage\":\"" + evaluation.stage +
                "\",\"root_acquisition_ordering_estimate\":" + diagnostic_finite_double(
                    proposal.root_registry_action != kNoId ? forced_score : root_acquisition_score(root_type)) +
                ",\"original_scope_preserved\":true,\"private_lower_used\":false,\"omitted\":" + excluded +
                ",\"full_layout_classes\":" + std::to_string(calc.layout().junk_classes.size()) +
                ",\"private_layout_classes\":" + std::to_string(private_calc ? private_calc->layout().junk_classes.size() : 0) +
                ",\"private_states\":" + std::to_string(private_calc ? private_calc->state_count() : 0) +
                ",\"constructed_rows\":" + std::to_string(constructed_rows) +
                ",\"continued_dirty_entries\":" + std::to_string(continued_dirty) +
                ",\"paid_cleanup_entries\":" + std::to_string(paid_cleanup) +
                ",\"paid_scour_entries\":" + std::to_string(paid_scour) +
                ",\"paid_redraw_entries\":" + std::to_string(paid_redraw) +
                ",\"shared_redraw_rows\":" + std::to_string(shared_redraw_rows) +
                ",\"cleanup_alternatives\":" + std::to_string(cleanup_alternatives) +
                ",\"improvement_rounds\":" + std::to_string(improvement_rounds) +
                ",\"child_work\":" + std::to_string(child_work) + ",\"child_peak_bytes\":" + std::to_string(child_peak) +
                ",\"seed_cost_estimate\":" + diagnostic_finite_double(seed_cost) +
                ",\"coarse_cost_estimate\":" + diagnostic_finite_double(coarse_cost) +
                ",\"coarse_cost_entry\":\"" + (proposal.nonempty_handoff ? "native_nonempty_entry" : "original_root") + "\"" +
                ",\"evaluated_cost\":" + diagnostic_finite_double(exact_cost) +
                ",\"nonempty_handoff\":" + (proposal.nonempty_handoff ? "true" : "false") +
                ",\"old_entry_cost\":" + diagnostic_finite_double(proposal.old_entry_cost) +
                ",\"native_boundary_entries\":" + std::to_string(native_boundary_entries) +
                ",\"entry_responses\":" + entry_response +
                ",\"eval_limit_states\":" + std::to_string(evaluation.limits.max_states) +
                ",\"eval_limit_pairs\":" + std::to_string(evaluation.limits.max_pairs) +
                ",\"eval_limit_transitions\":" + std::to_string(evaluation.limits.max_transitions) +
                ",\"eval_limit_owned_bytes\":" + std::to_string(evaluation.limits.max_owned_bytes) +
                ",\"eval_complete\":" + (evaluation.complete ? "true" : "false") +
                ",\"eval_states\":" + std::to_string(evaluation.progress.exact_states) +
                ",\"eval_pairs\":" + std::to_string(evaluation.progress.discovered_pairs + evaluation.progress.pending_pairs) +
                ",\"eval_transitions\":" + std::to_string(evaluation.progress.stored_transitions) +
                ",\"eval_peak_bytes\":" + std::to_string(evaluation.peak_bytes) +
                ",\"eval_solved_sccs\":" + std::to_string(evaluation.progress.solved_sccs) +
                ",\"eval_total_sccs\":" + std::to_string(evaluation.progress.total_sccs) +
                ",\"eval_iterations\":" + std::to_string(evaluation.progress.fallback_sweeps) +
                ",\"eval_residual\":" + diagnostic_finite_double(evaluation.progress.residual) +
                ",\"eval_subphase\":\"" + strategy_eval_subphase_name(evaluation.progress.subphase) +
                "\",\"wall_seconds\":" + diagnostic_finite_double(std::chrono::duration<double>(
                    std::chrono::steady_clock::now()-begin).count()) + "}";
            if (sample_index) {
                auto& previous=t.publication_candidate_samples.at(*sample_index);
                const auto shared=t.publication_candidate_sample_bytes + t.structural_failure_sample_bytes +
                    t.evaluator_memory_sample_bytes + t.direct_offpolicy_state_sample_bytes - previous.size();
                const auto cap=options.max_telemetry_json_bytes/4;
                if (sample.size()<=cap && shared<=cap-sample.size()) {
                    t.publication_candidate_sample_bytes -= previous.size();
                    t.publication_candidate_sample_bytes += sample.size();
                    previous=std::move(sample);
                }
            } else {
                const auto at=t.publication_candidate_samples.size();
                retain_bounded_json_sample(t.publication_candidate_samples, t.publication_candidate_samples_omitted,
                    t.publication_candidate_sample_bytes, std::move(sample));
                if (t.publication_candidate_samples.size()>at) sample_index=at;
            }
        };
        try {
            private_calc = std::make_unique<CalcContext>(calc.shared_session(), calc.goal(),
                calc.registry(), candidates, false, false, false, std::nullopt,
                calc.layout().count_observations, calc.product_solver_parent(), universe,
                calc.distinguishes_modifier_identity(), false, false, false, false,
                preserve_layout ? &calc.layout() : nullptr, true);
            SolveOptions local_options=options;
            const auto parent_bytes=parent_live_bytes();
            if (parent_bytes>=options.max_solver_owned_bytes)
                throw SolverResourceLimit("max_solver_owned_bytes",options.max_solver_owned_bytes);
            local_options.max_solver_owned_bytes=options.max_solver_owned_bytes-parent_bytes;
            if (calc.telemetry().reforge_logical_work_v1>=options.max_reforge_work)
                throw SolverResourceLimit("max_reforge_work",options.max_reforge_work);
            local_options.max_reforge_work=options.max_reforge_work-calc.telemetry().reforge_logical_work_v1;
            local_options.full_evidence=false;
            local_options.verified_policy_alternative_shadow_diagnostic=false;
            local_options.carrier_ladder_exact_boundary_mode=CarrierLadderExactBoundaryMode::Off;
            local_options.native_continuation_search=NativeContinuationSearchMode::Ordinary;
            Impl child(*private_calc, proposal.start, prices, local_options);
            child.incremental_action_generation=true;
            const auto root=child.result.start_state;
            std::uint32_t exalt=kNoId, annul=kNoId, scour=kNoId, root_action=kNoId;
            for (const auto& priced : child.operators) {
                const auto& op=private_calc->operators().at(priced.index);
                if (op.kind!=PlannerOperatorKind::Primitive) continue;
                const auto type=private_calc->registry().actions.at(op.primitive_action).params.type;
                if (type==ActionType::Exalt) exalt=priced.index;
                if (type==ActionType::Annul) annul=priced.index;
                if (type==ActionType::Scour) scour=priced.index;
                if (proposal.root_registry_action != kNoId) {
                    if (private_calc->registry().actions.at(op.primitive_action).id ==
                        calc.registry().actions.at(proposal.root_registry_action).id)
                        root_action=priced.index;
                } else if (type==root_type) root_action=priced.index;
            }
            if (exalt==kNoId || annul==kNoId || root_action==kNoId ||
                (proposal.nonempty_handoff && scour==kNoId) ||
                (root_type==ActionType::Chaos && !options.goal_progress_gated_reforges))
                throw std::runtime_error("dirty controller lacks a priced admitted acquisition/removal action");
            std::vector<std::uint32_t> walk{root};
            std::vector<std::uint8_t> reached(private_calc->state_count(),0); reached[root]=1;
            std::vector<StrategyContinuationEntryRequest> entries{{result.start_state,0,1,exact_start_item,false}};
            std::vector<std::uint32_t> entry_states{proposal.nonempty_handoff ? kNoId : root};
            if (proposal.nonempty_handoff) {
                entries.push_back({1ull<<63,1,1,proposal.start,false});
                entry_states.push_back(root);
            }
            std::vector<std::uint32_t> boundary;
            std::vector<std::uint32_t> local_states;
            std::uint32_t retry=kNoId;
            std::optional<SharedSparseTransitionSpan> root_redraw_span;
            child.transition_cache=std::make_shared<SolveTransitionCache>();
            child.priced_rows.clear(); child.policy_rows.clear();
            const auto extra_bytes=[&] {
                return walk.capacity()*sizeof(std::uint32_t)+reached.capacity()+
                    entries.capacity()*sizeof(StrategyContinuationEntryRequest)+
                    candidates.capacity()*sizeof(std::uint32_t)+universe.capacity()*sizeof(std::uint64_t)+
                    entry_states.capacity()*sizeof(std::uint32_t)+entry_response.capacity()+
                    (boundary.capacity()+local_states.capacity())*sizeof(std::uint32_t)+
                    proposals.capacity()*sizeof(Proposal);
            };
            const auto observe_memory=[&](const std::uint64_t transient=0) {
                const auto child_bytes=child.estimated_owned_bytes()+extra_bytes()+transient;
                child_peak=std::max(child_peak,child_bytes);
                if (child_bytes > options.max_solver_owned_bytes -
                    std::min(options.max_solver_owned_bytes,parent_live_bytes()))
                    throw SolverResourceLimit("max_solver_owned_bytes",options.max_solver_owned_bytes);
                return child_bytes;
            };
            const auto checkpoint_memory = [&](const std::uint64_t transient=0) {
                return observe_memory(transient) + handoff_base_bytes;
            };
            for (std::size_t cursor=0;cursor<walk.size();++cursor) {
                const auto state=walk[cursor];
                if (private_calc->is_goal_state(private_calc->state(state))) continue;
                pc_item_state item;
                if ((private_calc->state(state).flags &
                     ~(proposal.nonempty_handoff ? kFlagFractured : 0u)) != 0 ||
                    !private_calc->materialize(state,item) ||
                    !(proposal.nonempty_handoff ? dirty_fractured_bridge_item(private_calc->session(),item) :
                      ordinary_return_bridge_item(private_calc->session(),item)))
                    throw std::runtime_error("dirty controller encountered protected or unmaterializable state");
                const auto goals=std::popcount(child.satisfied_goal_mask_for_state(state));
                const auto count=item.prefix_count+item.suffix_count;
                std::uint32_t selected=kNoId;
                std::shared_ptr<const OutcomeDistribution> law;
                if (proposal.nonempty_handoff && goals < proposal.minimum_progress) {
                    if (item.rarity == PC_RARITY_MAGIC) {
                        boundary.push_back(state);
                        continue;
                    }
                    // Loss of mutable progress does not grant a free reset or
                    // coverage by the old Rare router. Pay the admitted native
                    // Scour; its complete law retains the one fractured goal
                    // and reaches the old Magic decision context.
                    selected=scour;
                }
                local_states.push_back(state);
                if (selected!=kNoId) { /* explicit paid recovery above */ }
                else if (!proposal.nonempty_handoff &&
                         (state==root || state==retry || (broad_root && goals==0))) {
                    // This complete native redraw already replaces ordinary
                    // junk. After losing every goal, paying Annul to reach an
                    // empty Rare adds cost before the same acquisition. Keep
                    // the real dirty entry and pay the admitted redraw there.
                    selected=root_action;
                    if (state!=root && state!=retry) ++paid_redraw;
                }
                else if (private_calc->state(state).goal_progress_retry_basin != 0)
                    throw std::runtime_error("dirty controller cannot salvage a virtual retry entry");
                else if (goals>0 && goals<private_calc->goal().required_satisfied_slots() &&
                         private_calc->is_candidate_operator_admitted_for_state(state,exalt) &&
                         action_legal(private_calc->session(),private_calc->registry().actions[
                             private_calc->operators()[exalt].primitive_action],private_calc->state(state))) {
                    ReturnOutcomeScope pending{*private_calc};
                    while (!private_calc->advance_outcomes(state,private_calc->operators()[exalt].primitive_action,
                            options.goal_progress_gated_reforges,law,1)) {
                        charge_child();
                        co_await CooperativeCheckpoint{checkpoint_memory(private_calc->outcome_cursor_bytes())};
                    }
                    charge_child();
                    double gain=0;
                    if (law && law->supported && law->applicable && law->choice_groups.empty() && law->choice_options.empty())
                        for (const auto& e:law->entries)
                            if (std::popcount(child.satisfied_goal_mask_for_state(e.state))>goals) gain+=e.probability;
                    if (gain>0) selected=exalt;
                    else law.reset();
                }
                if (selected==kNoId) selected=annul;
                if (!private_calc->is_candidate_operator_admitted_for_state(state,selected))
                    throw std::runtime_error("dirty selected action is outside the private admitted scope");
                const auto primitive=private_calc->operators().at(selected).primitive_action;
                if (!law) {
                    ReturnOutcomeScope pending{*private_calc};
                    while (!private_calc->advance_outcomes(state,primitive,options.goal_progress_gated_reforges,law,1)) {
                        charge_child();
                        co_await CooperativeCheckpoint{checkpoint_memory(private_calc->outcome_cursor_bytes())};
                    }
                    charge_child();
                }
                if (!law || !law->supported || !law->applicable || !law->choice_groups.empty() || !law->choice_options.empty())
                    throw std::runtime_error("dirty selected native row is incomplete, illegal or observed");
                if (state==root && law->goal_progress_gated) retry=law->gated_retry_state;
                const auto append = [&](const std::uint32_t action, const OutcomeDistribution& native) {
                    const auto n=private_calc->state_count();
                    reached.resize(n,0); child.policy_rows.resize(n,std::numeric_limits<std::uint64_t>::max());
                    child.result.policy.resize(n);
                    SparsePolicyRowInput row; row.owner_state=state; row.operator_index=action;
                    for (const auto& priced:child.operators) if (priced.index==action) row.cost=priced.cost;
                    observe_memory(native.entries.size()*sizeof(SparsePolicyTransitionInput));
                    row.transitions.reserve(native.entries.size());
                    double mass=0;
                    for (const auto& e:native.entries) {
                        if (!std::isfinite(e.probability) || e.probability<0) throw std::runtime_error("invalid native dirty probability");
                        mass+=e.probability;
                        if (e.probability==0) continue;
                        row.transitions.push_back({e.state,e.probability});
                        if (!reached[e.state]) { reached[e.state]=1; walk.push_back(e.state); }
                    }
                    if (std::abs(mass-1)>1e-12) throw std::runtime_error("incomplete native dirty probability mass");
                    std::optional<SharedSparseTransitionSpan> shared;
                    if (broad_root && action==root_action && root_redraw_span &&
                        root_redraw_span->count==row.transitions.size()) {
                        bool equal=true;
                        for (std::size_t i=0;i<row.transitions.size();++i) {
                            const auto at=root_redraw_span->offset+i;
                            equal &= child.transition_cache->successors.at(at)==row.transitions[i].successor &&
                                child.transition_cache->probabilities.at(at)==row.transitions[i].probability;
                        }
                        if (equal) shared=root_redraw_span;
                    }
                    const auto appended_transitions=shared ? 0 : row.transitions.size();
                    if (constructed_rows>=options.max_state_action_rows ||
                        appended_transitions>options.max_transitions-std::min<std::uint64_t>(
                            options.max_transitions,child.transition_cache->successors.size()))
                        throw std::runtime_error("dirty selected graph reached the declared row/transition cap");
                    observe_memory(row.transitions.capacity()*sizeof(SparsePolicyTransitionInput) +
                        appended_transitions*2*(sizeof(std::uint32_t)+sizeof(double)));
                    ++constructed_rows;
                    const auto id=append_sparse_policy_row(*child.transition_cache,child.priced_rows,row,shared);
                    if (broad_root && action==root_action && !root_redraw_span) {
                        const auto& stored=child.transition_cache->rows.at(id);
                        root_redraw_span=SharedSparseTransitionSpan{stored.transition_offset,stored.transition_count};
                    }
                    if (shared) ++shared_redraw_rows;
                    return id;
                };
                child.policy_rows[state]=append(selected,*law);
                child.result.policy[state]=PolicyOperatorRef{selected};
                // Complete the competing paid-removal row and every new
                // successor under the same native controller. Selection below
                // compares full continuation costs, not debt or junk counts.
                if (selected==exalt && goals>0 && (proposal.nonempty_handoff || (state!=root && state!=retry)) &&
                    private_calc->is_candidate_operator_admitted_for_state(state,annul)) {
                    std::shared_ptr<const OutcomeDistribution> cleanup;
                    ReturnOutcomeScope pending{*private_calc};
                    while (!private_calc->advance_outcomes(state,
                            private_calc->operators()[annul].primitive_action,
                            options.goal_progress_gated_reforges,cleanup,1)) {
                        charge_child();
                        co_await CooperativeCheckpoint{checkpoint_memory(private_calc->outcome_cursor_bytes())};
                    }
                    charge_child();
                    if (!cleanup || !cleanup->supported || !cleanup->applicable ||
                        !cleanup->choice_groups.empty() || !cleanup->choice_options.empty())
                        throw std::runtime_error("dirty cleanup alternative is not a complete native row");
                    (void)append(annul,*cleanup);
                    ++cleanup_alternatives;
                }
                if (proposal.nonempty_handoff && selected!=scour &&
                    private_calc->is_candidate_operator_admitted_for_state(state,scour)) {
                    std::shared_ptr<const OutcomeDistribution> cleanup;
                    ReturnOutcomeScope pending{*private_calc};
                    while (!private_calc->advance_outcomes(state,
                            private_calc->operators()[scour].primitive_action,
                            options.goal_progress_gated_reforges,cleanup,1)) {
                        charge_child();
                        co_await CooperativeCheckpoint{checkpoint_memory(private_calc->outcome_cursor_bytes())};
                    }
                    charge_child();
                    if (!cleanup || !cleanup->supported || !cleanup->applicable ||
                        !cleanup->choice_groups.empty() || !cleanup->choice_options.empty())
                        throw std::runtime_error("dirty Scour alternative is not a complete native row");
                    (void)append(scour,*cleanup);
                    ++cleanup_alternatives;
                }
                if (selected==exalt && goals>0 && count>goals) {
                    ++continued_dirty;
                    if (entries.size()<3 && (!proposal.nonempty_handoff || state!=root)) {
                        entries.push_back({(1ull<<63)+entries.size(),entries.size(),1,item,false});
                        entry_states.push_back(state);
                    }
                }
                if (selected==annul || selected==scour) ++paid_cleanup;
                if (selected==scour) ++paid_scour;
                if (std::chrono::steady_clock::now()-last_sample>=std::chrono::seconds(1)) {
                    record("in_progress"); last_sample=std::chrono::steady_clock::now();
                }
                co_await CooperativeCheckpoint{checkpoint_memory()};
            }
            const auto n=private_calc->state_count();
            // The exact fixed-policy solver requires finite numerical seeds
            // within each closed SCC. These zeros carry no bound authority.
            child.result.values.assign(n,0); child.result.goal_states.assign(n,0);
            child.expanded.assign(n,0); child.result.expanded.assign(n,0);
            child.result.policy_reachable=reached;
            for (const auto state:walk) {
                child.expanded[state]=1; child.result.expanded[state]=1;
                if (private_calc->is_goal_state(private_calc->state(state))) {
                    child.result.goal_states[state]=1; child.result.values[state]=0;
                }
            }
            child.expanded_count=static_cast<std::uint32_t>(walk.size());
            if (proposal.nonempty_handoff) {
                if (boundary.empty()) throw std::runtime_error("nonempty proposal has no native return entries");
                std::vector<StrategyContinuationEntryRequest> requests;
                for (const auto state : boundary) {
                    pc_item_state item;
                    if (!private_calc->materialize(state,item)) throw std::runtime_error("nonempty boundary cannot materialize");
                    requests.push_back({(1ull<<62)+state,state,1,item,false});
                }
                auto boundary_limits = options;
                const auto held = parent_live_bytes()+observe_memory();
                boundary_limits.max_solver_owned_bytes -= std::min(boundary_limits.max_solver_owned_bytes,held);
                boundary_limits.max_reforge_work -= std::min(boundary_limits.max_reforge_work,calc.telemetry().reforge_logical_work_v1);
                auto boundary_work = evaluate_return_graph(calc,prices,handoff_base->compiled_artifact.strategy_json,
                    boundary_limits,evaluation,"nonempty_boundary_continuations",std::move(requests));
                while (!boundary_work.resume())
                    co_await CooperativeCheckpoint{checkpoint_memory()+boundary_work.retained_bytes()};
                auto responses = boundary_work.take_result(); boundary_work.reset();
                if (!complete_return_evaluation(responses,false))
                    throw std::runtime_error("nonempty return controller failed native boundary evaluation");
                for (const auto& member : responses.continuation_upper.members) {
                    const auto state = member.exact_member_identity;
                    if (!member.available() || state>=n ||
                        std::find(boundary.begin(),boundary.end(),state)==boundary.end())
                        throw std::runtime_error("nonempty return has no compatible native continuation: " +
                            std::string(strategy_continuation_entry_status_name(member.status)) +
                            ", rarity=" + std::to_string(member.item.rarity) +
                            ", prefixes=" + std::to_string(member.item.prefix_count) +
                            ", suffixes=" + std::to_string(member.item.suffix_count));
                    child.result.values[state]=member.exact_continuation_upper;
                    child.expanded[state]=0; child.result.expanded[state]=0;
                    ++native_boundary_entries;
                }
                if (native_boundary_entries!=boundary.size())
                    throw std::runtime_error("nonempty return entry coverage is incomplete");
                child.expanded_count-=static_cast<std::uint32_t>(boundary.size());
                // Reuse the existing finite-frontier fixed-policy equations.
                // These point continuations are private ordering estimates,
                // not uniform class values or any full-scope lower authority.
                child.focused_lower_mode=true;
            }
            child.transition_cache->expanded=child.expanded;
            child.incremental_upper_policy_pass=true;
            child.reset_policy_iteration_units();
            evaluation.stage="coarse_fixed_controller";
            while (!child.evaluate_fixed_policy()) {
                if (!child.policy_evaluation_incomplete)
                    throw std::runtime_error("coarse dirty fixed controller did not close a proper finite system: " +
                        child.result.diagnostics.policy_evaluation_failure);
                if (std::chrono::steady_clock::now()-last_sample>=std::chrono::seconds(1)) {
                    record("in_progress"); last_sample=std::chrono::steady_clock::now();
                }
                co_await CooperativeCheckpoint{checkpoint_memory()};
            }
            seed_cost=child.result.values.at(root);
            // Reuse ordinary sparse policy selection and exact fixed-policy
            // systems. These private values only order candidate decisions;
            // the original request's lower and proof ledger are untouched.
            evaluation.stage="continuation_cost_improvement";
            for (std::uint32_t round=0;round<options.max_sweeps;++round) {
                bool improved=false;
                while (!child.advance_policy_selection(improved))
                    co_await CooperativeCheckpoint{checkpoint_memory()};
                if (!improved) break;
                ++improvement_rounds;
                while (!child.evaluate_fixed_policy()) {
                    if (!child.policy_evaluation_incomplete)
                        throw std::runtime_error("cost-selected dirty controller did not close a proper finite system: " +
                            child.result.diagnostics.policy_evaluation_failure);
                    if (std::chrono::steady_clock::now()-last_sample>=std::chrono::seconds(1)) {
                        record("in_progress"); last_sample=std::chrono::steady_clock::now();
                    }
                    co_await CooperativeCheckpoint{checkpoint_memory()};
                }
            }
            for (const auto state:walk)
                if (!child.result.goal_states[state] && child.result.expanded[state])
                    child.result.policy[state]=PolicyOperatorRef{child.priced_rows.at(child.policy_rows[state]).operator_index};
            coarse_cost=child.result.values.at(root);
            if (!std::isfinite(coarse_cost) || coarse_cost<0)
                throw std::runtime_error("coarse dirty fixed controller has no finite root estimate");
            if (!proposal.nonempty_handoff && coarse_cost>=incumbent_portfolio.verified_executable_upper()) {
                record("deferred_by_cost_estimate",
                    "private estimate is not competitive; no numerical or action-retirement authority");
                continue;
            }
            child.result.policy_available=true; child.result.policy_status=SolvePolicyStatus::BoundedFeasible;
            child.result.evaluated_policy_cost=coarse_cost; child.result.upper_bound=coarse_cost;
            for (const auto state : boundary) child.result.policy_reachable[state]=0;
            PolicyCompilationTelemetry compilation;
            evaluation.stage="compilation";
            const auto compiler_held=parent_live_bytes()+observe_memory();
            std::string graph=compile_policy_strategy_json(*private_calc,child.result,
                "Current-run dirty continuation policy",&compilation,options.max_strategy_json_bytes,
                nullptr,options.max_solver_owned_bytes-std::min(options.max_solver_owned_bytes,compiler_held),
                PolicyRouteDefaultMode::CertificationFailClosed);
            if (proposal.nonempty_handoff) {
                auto compose_limits=options;
                const auto held=parent_live_bytes()+observe_memory()+graph.capacity();
                compose_limits.max_solver_owned_bytes-=std::min(compose_limits.max_solver_owned_bytes,held);
                graph=compile_dirty_continuation_strategy_json(*private_calc,graph,
                    handoff_base->compiled_artifact.strategy_json,local_states,boundary,compose_limits,&compilation);
            }
            SolveOptions check_limits=options;
            const auto held=parent_live_bytes()+observe_memory()+graph.capacity();
            if (held>=options.max_solver_owned_bytes) throw SolverResourceLimit("max_solver_owned_bytes",options.max_solver_owned_bytes);
            check_limits.max_solver_owned_bytes=options.max_solver_owned_bytes-held;
            check_limits.max_reforge_work=options.max_reforge_work-
                std::min(options.max_reforge_work,calc.telemetry().reforge_logical_work_v1);
            auto check=evaluate_return_graph(calc,prices,graph,check_limits,evaluation,"complete_dirty_controller",std::move(entries));
            while (!check.resume()) {
                if (std::chrono::steady_clock::now()-last_sample>=std::chrono::seconds(1)) {
                    record("in_progress"); last_sample=std::chrono::steady_clock::now();
                }
                co_await CooperativeCheckpoint{checkpoint_memory()+graph.capacity()+check.retained_bytes()};
            }
            auto evaluated=check.take_result(); check.reset();
            if (!complete_return_evaluation(evaluated,false))
                throw std::runtime_error("complete emitted dirty controller failed native properness or cost checks");
            exact_cost=evaluated.total_expected_cost;
            entry_response="[";
            for (const auto& member:evaluated.continuation_upper.members) {
                if (entry_response.size()>1) entry_response+=',';
                entry_response+="{\"request_identity\":\""+std::to_string(member.represented_state_identity)+
                    "\",\"status\":\""+strategy_continuation_entry_status_name(member.status)+
                    "\",\"native_continuation_cost\":"+diagnostic_finite_double(member.exact_continuation_upper)+
                    ",\"native_bellman_residual\":"+diagnostic_finite_double(member.bellman_residual)+
                    ",\"prefixes\":"+std::to_string(member.item.prefix_count)+
                    ",\"suffixes\":"+std::to_string(member.item.suffix_count);
                const auto ordinal=member.exact_member_identity;
                if (ordinal<entry_states.size() && entry_states[ordinal]!=kNoId) {
                    const auto state=entry_states[ordinal];
                    const auto goals=std::popcount(child.satisfied_goal_mask_for_state(state));
                    entry_response+=",\"goal_mask\":"+std::to_string(child.satisfied_goal_mask_for_state(state))+
                        ",\"first_policy_debt\":"+std::to_string(child.joint_policy_terminal_debt(state))+
                        ",\"native_abstract_identity\":[";
                    bool first=true;
                    for (const auto word:exact_abstract_state_key(private_calc->state(state),0)) {
                        if (!first) entry_response+=','; first=false;
                        entry_response+='\"'+std::to_string(word)+'\"';
                    }
                    entry_response+="],\"private_choices\":["; first=true;
                    for (const auto row_id:state_row_indices(*child.transition_cache,state)) {
                        const auto& row=child.transition_cache->rows.at(row_id);
                        const auto& priced=child.priced_rows.at(row_id);
                        double value=priced.cost,loss=0,gain=0,terminal=0;
                        for (std::uint32_t i=0;i<row.transition_count;++i) {
                            const auto at=row.transition_offset+i;
                            const auto next=child.transition_cache->successors.at(at);
                            const auto probability=child.transition_cache->probabilities.at(at);
                            value+=probability*child.result.values.at(next);
                            const auto next_goals=std::popcount(child.satisfied_goal_mask_for_state(next));
                            if (next_goals<goals) loss+=probability;
                            if (next_goals>goals) gain+=probability;
                            if (private_calc->is_goal_state(private_calc->state(next))) terminal+=probability;
                        }
                        if (!first) entry_response+=','; first=false;
                        entry_response+="{\"operator\":\""+diagnostic_json_escape(
                            private_calc->operators().at(priced.operator_index).id)+
                            "\",\"selected\":"+(child.policy_rows.at(state)==row_id?"true":"false")+
                            ",\"full_continuation_cost_estimate\":"+diagnostic_finite_double(value)+
                            ",\"loses_goal_probability\":"+diagnostic_finite_double(loss)+
                            ",\"gains_goal_probability\":"+diagnostic_finite_double(gain)+
                            ",\"terminal_probability\":"+diagnostic_finite_double(terminal)+"}";
                    }
                    entry_response+=']';
                }
                entry_response+='}';
            }
            entry_response+=']';
            record("independently_evaluated");
            if (!(exact_cost < incumbent_portfolio.verified_executable_upper())) continue;
            // The evaluator's exact graph cost is the upper. Coarse costs were
            // proposal estimates only, and no child state id enters the parent.
            refinement::CompiledPolicyAssertion assertion;
            assertion.strategy_json=graph; assertion.certification_strategy_json=graph;
            assertion.compilation=std::move(compilation); assertion.evaluation=std::move(evaluated);
            BoundedPolicyIncumbent candidate;
            candidate.compiled_artifact=retained_artifact_from_assertion(assertion);
            candidate.compiled_artifact.policy_decision_bindings.clear();
            candidate.compiled_root_entry_only=true;
            candidate.strict_state_provenance=false;
            candidate.values.assign(calc.state_count(),kInfinity); candidate.values[result.start_state]=exact_cost;
            candidate.policy.resize(calc.state_count()); candidate.policy_reachable.assign(calc.state_count(),0);
            candidate.policy_rows.assign(calc.state_count(),std::numeric_limits<std::uint64_t>::max());
            candidate.certified_upper_bound=exact_cost; candidate.evaluated_policy_cost=exact_cost;
            candidate.kind="private_dirty_continuation";
            candidate.compilation_provenance=proposal.nonempty_handoff
                ? "independent_full_scope_composed_nonempty_v1"
                : "independent_full_scope_root_entry_private_layout_v1";
            candidate.goal_identity=goal_identity(); candidate.economy_identity=economy_identity();
            candidate.action_vocabulary_identity=action_vocabulary_identity(); candidate.action_vocabulary_size=operators.size();
            candidate.caller_scope_identity=caller_scope_identity(); candidate.artifact_identity=artifact_identity();
            candidate.graph_identity=graph_identity(); candidate.source_generation=transition_cache->rows.size();
            candidate.target_generation=calc.state_count();
            candidate.graph_row_count=transition_cache->rows.size(); candidate.graph_priced_row_count=priced_rows.size();
            candidate.graph_successor_count=transition_cache->successors.size(); candidate.graph_probability_count=transition_cache->probabilities.size();
            candidate.graph_choice_count=transition_cache->choices.size(); candidate.graph_choice_successor_count=transition_cache->choice_successors.size();
            candidate.graph_choice_option_count=transition_cache->choice_options.size();
            candidate.graph_prefix_identity=incumbent_graph_prefix_identity(candidate.graph_row_count,candidate.graph_priced_row_count,
                candidate.graph_successor_count,candidate.graph_probability_count,candidate.graph_choice_count,
                candidate.graph_choice_successor_count,candidate.graph_choice_option_count);
            candidate.independently_certified=candidate.independently_evaluated=candidate.proper=candidate.executable=true;
            // A local-entry estimate is not comparable with a composed root
            // cost. The admitted root value is exactly the native evaluation.
            candidate.reconciliation_absolute_delta=proposal.nonempty_handoff ? 0 : std::abs(exact_cost-coarse_cost);
            candidate.reconciliation_relative_delta=proposal.nonempty_handoff ? 0 :
                std::abs(exact_cost-coarse_cost)/std::max(1.0,exact_cost);
            candidate.portfolio_identity=1469598103934665603ULL;
            identity_mix_string(candidate.portfolio_identity,graph);
            identity_mix(candidate.portfolio_identity,candidate.caller_scope_identity);
            candidate.retained_owned_bytes=incumbent_owned_bytes(candidate);
            if (!retain_certified_incumbent(candidate,observe_memory()+graph.capacity()+candidate.retained_owned_bytes))
                throw std::runtime_error("independently evaluated dirty root artifact failed portfolio admission");
            retained=true;
            record("retained");
        } catch (const std::exception& error) {
            if (private_calc) private_calc->cancel_outcomes();
            charge_child();
            record("refused",error.what());
        }
    }
    co_return retained;
}

} // namespace poecraft::solver
