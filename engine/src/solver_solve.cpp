#include "solver_solve_types.hpp"

namespace poecraft {
namespace solver {

using namespace solve_detail;

 std::uint64_t SolveWork::Impl::priced_operator_nested_bytes(
        const PricedOperator& priced) {
        std::uint64_t bytes = priced.resource_prices.capacity() *
                              sizeof(std::pair<std::string, double>);
        for (const auto& [key, unused_price] : priced.resource_prices) {
            (void)unused_price;
            bytes += key.capacity() + 1;
        }
        return bytes;
    }

void SolveWork::Impl::initialize_owned_bytes_ledger() {
        for (const auto& [key, unused_price] : prices) {
            (void)unused_price;
            owned_prices_nested_bytes += key.capacity() + 1;
        }
        for (const PricedOperator& priced : operators) {
            owned_operators_nested_bytes +=
                priced_operator_nested_bytes(priced);
        }
    }

SolveWork::Impl::Impl(
        CalcContext& context,
        const pc_item_state& start_item,
        const std::unordered_map<std::string, double>& prices,
        const SolveOptions& solve_options)
        : calc(context), session(context.session()), options(solve_options),
          prices(prices),
          reported_unsupported(context.operators().size(), false) {
        const auto setup_started = std::chrono::steady_clock::now();
        options.max_expanded_states = std::min(
            options.max_expanded_states, options.max_states);
        calc.reset_solve_telemetry();
        calc.set_solve_resource_caps(
            options.max_discovered_states, options.max_reforge_work);
        result.options = options;
        result.requested_absolute_optimality_gap =
            options.max_absolute_optimality_gap;
        result.requested_relative_optimality_gap =
            options.max_relative_optimality_gap;
        result.diagnostics.diagnostic_sample_limit =
            options.max_diagnostic_samples;
        result.diagnostics.telemetry_json_byte_limit =
            options.max_telemetry_json_bytes;
        result.diagnostics.registry_actions = static_cast<std::uint32_t>(
            calc.registry().actions.size());
        result.diagnostics.candidate_actions = static_cast<std::uint32_t>(
            calc.candidates().size());
        const ActionControlSummary& control = calc.action_control();
        result.diagnostics.relevance_reduced_actions =
            control.pruned_outside_goal_relevance +
            control.pruned_outside_envelope;
        result.diagnostics.dependency_actions =
            control.dependency_primitives;
        result.diagnostics.deferred_actions =
            control.deferred_fossil_loadouts;
        retain_action_reason(
            std::string(
                control.explicit_envelope
                    ? "included:explicit_goal_envelope:"
                    : calc.registry().fossil_generation_goal_relevant
                          ? "included:bounded_goal_relevant_envelope:"
                          : "included:conservative_exhaustive_envelope:") +
            std::to_string(control.included_primitives));
        if (control.pruned_outside_envelope != 0) {
            retain_action_reason(
                "pruned:not_permitted_by_explicit_goal_envelope:" +
                std::to_string(control.pruned_outside_envelope));
        }
        if (control.pruned_outside_goal_relevance != 0) {
            retain_action_reason(
                "pruned:outside_product_goal_relevance:" +
                std::to_string(
                    control.pruned_outside_goal_relevance));
        }
        if (control.dependency_primitives != 0) {
            retain_action_reason(
                "included:fixed_option_structural_dependency:" +
                std::to_string(control.dependency_primitives));
        }
        if (control.deferred_fossil_loadouts != 0) {
            retain_action_reason(
                std::string(
                    calc.registry().fossil_generation_goal_relevant
                        ? "deferred:outside_bounded_goal_relevant_fossil_beam:"
                        : "deferred:lazy_fossil_signature_not_requested:") +
                std::to_string(control.deferred_fossil_loadouts));
        }
        if (control.automatic_options != 0) {
            retain_action_reason(
                "included:native_price_independent_automatic_options:" +
                std::to_string(control.automatic_options));
        }
        /* Primitive support remains action-registry telemetry. Fixed options
         * are priced and evaluated alongside those primitive wrappers. */
        for (const std::uint32_t index : calc.candidates()) {
            const ActionDescriptor& descriptor =
                calc.registry().actions.at(index);
            const bool supported = calc_supports(descriptor);
            if (supported) {
                ++result.diagnostics.evaluator_supported_actions;
            }
        }
        for (const std::uint32_t index : calc.candidate_operators()) {
            const PlannerOperator& planner = calc.operators().at(index);
            double cost = 0.0;
            bool priced = true;
            std::vector<std::pair<std::string, double>> resource_prices;
            for (const auto& [key, quantity] :
                 planner.resource_quantities) {
                const auto found = prices.find(key);
                if (found == prices.end()) {
                    priced = false;
                    break;
                }
                cost += quantity * found->second;
                resource_prices.push_back({key, found->second});
            }
            if (!priced) {
                record_skipped_missing_price(planner.id);
                add_action_reason(
                    "unpriced", planner.id,
                    "missing_one_or_more_resource_prices");
                if (planner.automatic_kind !=
                    AutomaticCandidateKind::None) {
                    add_action_reason(
                        "rejected", planner.id,
                        "automatic_candidate_missing_price");
                }
                continue;
            }
            ++result.diagnostics.priced_scanned_actions;
            const bool supported =
                planner.kind == PlannerOperatorKind::FixedOption ||
                calc_supports(calc.registry().actions.at(
                    planner.primitive_action));
            if (!supported) {
                reported_unsupported[index] = true;
                record_skipped_unsupported(planner.id);
                add_action_reason(
                    "unsupported", planner.id,
                    "no_exact_evaluator_for_requested_primitive");
                continue;
            }
            operators.push_back(
                {index, cost, std::move(resource_prices)});
            if (planner.kind == PlannerOperatorKind::Primitive &&
                calc.registry().actions.at(planner.primitive_action).synthetic) {
                restart_operator_index = index;
                restart_cost = cost;
            }
            ++result.diagnostics.supported_priced_actions;
        }
        const bool priced_automatic_fracture = std::any_of(
            operators.begin(), operators.end(),
            [&](const PricedOperator& priced) {
                const PlannerOperator& planner =
                    calc.operators().at(priced.index);
                return planner.kind == PlannerOperatorKind::Primitive &&
                       planner.automatic_kind ==
                           AutomaticCandidateKind::Fracture;
            });
        if (priced_automatic_fracture && restart_operator_index == kNoId) {
            throw std::invalid_argument(
                "goal-relevant Fracture planning requires a priced base for "
                "Restart miss recovery");
        }
        for (std::size_t i = 0;
             i < calc.static_candidate_operator_count(); ++i) {
            const std::uint32_t index = calc.candidate_operators().at(i);
            if (index < reported_unsupported.size() &&
                !reported_unsupported[index] &&
                std::find_if(
                    operators.begin(), operators.end(),
                    [&](const PricedOperator& priced) {
                        return priced.index == index;
                    }) != operators.end()) {
                static_operator_indices.push_back(index);
            }
        }

        result.start_state = calc.intern_item(start_item);
        const bool has_constructive_renewal = std::any_of(
            operators.begin(), operators.end(),
            [&](const PricedOperator& priced) {
                const PlannerOperator& planner =
                    calc.operators().at(priced.index);
                if (planner.automatic_kind ==
                    AutomaticCandidateKind::ConstructiveRenewal) {
                    return true;
                }
                return planner.kind == PlannerOperatorKind::Primitive &&
                       planner.primitive_action <
                           calc.registry().actions.size() &&
                       action_transition_facts(
                           calc.registry().actions.at(
                               planner.primitive_action).params.type)
                           .renewal;
            });
        next_focus_checkpoint = has_constructive_renewal
                                    ? 1
                                    : std::max<std::uint32_t>(
                                          1,
                                          options.focused_expansion_checkpoint);
        priced_operator_position.assign(calc.operators().size(), -1);
        for (std::uint32_t i = 0; i < operators.size(); ++i) {
            priced_operator_position[operators[i].index] =
                static_cast<std::int32_t>(i);
        }
        const auto& cached = calc.solve_transition_cache();
        if (cached != nullptr &&
            cached->compatible(result.start_state, operators, options)) {
            transition_cache = cached;
            expanded_count = transition_cache->expanded_states;
            expanded = transition_cache->expanded;
            result.behavioral_representative_by_state =
                transition_cache->behavioral_representative_by_state;
            focused_mode = transition_cache->focused_partial;
            cache_pending = !focused_mode;
            result.diagnostics.transition_cache_reused = true;
        } else {
            transition_cache = std::make_shared<SolveTransitionCache>();
            transition_cache->exact_quotient = !options.strict_states;
            transition_cache->start_state = result.start_state;
            transition_cache->max_states = options.max_states;
            transition_cache->max_discovered_states =
                options.max_discovered_states;
            transition_cache->max_expanded_states =
                options.max_expanded_states;
            transition_cache->max_state_action_rows =
                options.max_state_action_rows;
            transition_cache->max_transitions = options.max_transitions;
            transition_cache->max_reforge_work = options.max_reforge_work;
            transition_cache->max_solver_owned_bytes =
                options.max_solver_owned_bytes;
            transition_cache->max_diagnostic_samples =
                options.max_diagnostic_samples;
            transition_cache->full_evidence = options.full_evidence;
            transition_cache->kernel_reuse = options.kernel_reuse;
            for (const PricedOperator& priced : operators) {
                transition_cache->operator_indices.push_back(priced.index);
            }
            enqueue(result.start_state);
        }
        result.diagnostics.solve_setup_ns = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - setup_started)
                .count());
        initialize_owned_bytes_ledger();
    }

double SolveWork::Impl::exact_gap_proof_tolerance() const {
        return options.epsilon * 10.0;
    }

double SolveWork::Impl::value_comparison_tolerance(const double value) const {
        return options.epsilon * std::max(1.0, std::abs(value)) * 10.0;
    }

double SolveWork::Impl::acceptable_residual() const {
        /* Preserve the measured absolute residual, but terminate a stable
         * policy using epsilon as a relative tolerance at V(start)'s scale.
         * Large endgame values otherwise repeat the identical fixed policy
         * indefinitely on last-bit double-precision noise. */
        double scale = 1.0;
        if (result.start_state < result.values.size()) {
            const double start = result.values[result.start_state];
            if (std::isfinite(start) && start < kValueCeiling) {
                scale = std::max(1.0, std::abs(start));
            }
        }
        return options.epsilon * scale;
    }

SolveWork::SolveWork(
    CalcContext& calc,
    const pc_item_state& start_item,
    const std::unordered_map<std::string, double>& prices,
    const SolveOptions& options)
    : impl_(std::make_unique<Impl>(calc, start_item, prices, options)) {}

SolveWork::~SolveWork() = default;
SolveWork::SolveWork(SolveWork&&) noexcept = default;
SolveWork& SolveWork::operator=(SolveWork&&) noexcept = default;

void SolveWork::step(const std::uint32_t max_work_items) {
    impl_->step(max_work_items);
}

SolveProgress SolveWork::progress() const {
    return impl_->progress();
}

SolveTelemetrySnapshot SolveWork::telemetry_snapshot(bool abandoned) const {
    return impl_->telemetry_snapshot(abandoned);
}

SolveResult SolveWork::finish() {
    return impl_->finish();
}

std::uint64_t SolveWork::live_owned_bytes() const {
    return impl_->estimated_owned_bytes();
}

std::uint64_t SolveWork::peak_owned_bytes() const {
    return std::max(
        impl_->peak_owned_bytes, impl_->estimated_owned_bytes());
}

SolveResult solve(
    CalcContext& calc,
    const pc_item_state& start_item,
    const std::unordered_map<std::string, double>& prices,
    const SolveOptions& options) {
    SolveWork work(calc, start_item, prices, options);
    while (!work.progress().done) work.step(4096);
    return work.finish();
}

}
}
