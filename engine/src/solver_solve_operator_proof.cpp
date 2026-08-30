#include "solver_solve_types.hpp"

namespace poecraft {
namespace solver {

using namespace solve_detail;

double SolveWork::Impl::authored_fixed_program_cost_lower(
        const PlannerOperator& planner) const {
    const PlannerOperatorRuntimeSemantics runtime =
        planner_operator_runtime_semantics(planner, calc.registry());
    double immediate = kInfinity;
    for (const auto& path : runtime.execution_paths) {
        double path_cost = 0.0;
        bool priced = true;
        for (const PlannerOperatorRuntimeStep& step : path) {
            if (step.action >= calc.registry().actions.size()) {
                priced = false;
                break;
            }
            for (const std::string& key :
                 calc.registry().actions.at(step.action).cost_keys) {
                const auto found = prices.find(key);
                if (found == prices.end() ||
                    !std::isfinite(found->second) ||
                    found->second < 0.0) {
                    priced = false;
                    break;
                }
                path_cost += found->second;
            }
            if (!priced) break;
        }
        if (priced) immediate = std::min(immediate, path_cost);
    }
    return immediate;
}

double SolveWork::Impl::operator_proof_lower_value(
        const std::uint32_t state,
        const std::uint32_t operator_index,
        const bool record_pattern_owners) {
        if (operator_index >= priced_operator_position.size()) {
            return kInfinity;
        }
        const std::int32_t position =
            priced_operator_position[operator_index];
        if (position < 0) return kInfinity;
        const PlannerOperator& planner =
            calc.operators().at(operator_index);
        double immediate =
            operators.at(static_cast<std::size_t>(position)).cost;
        if (planner.kind == PlannerOperatorKind::FixedOption &&
            planner.automatic_kind == AutomaticCandidateKind::None) {
            /* Gate 6 builds the complete optimistic program automaton, while
             * Gate 7 owns activating it as a pruning consumer. Preserve the
             * established guaranteed-first-step lower at this boundary. */
            if (planner.primitive_program.empty()) return -kInfinity;
            immediate = 0.0;
            for (const std::string& key :
                 calc.registry().actions.at(
                     planner.primitive_program.front()).cost_keys) {
                const auto found = prices.find(key);
                if (found == prices.end() ||
                    !std::isfinite(found->second) ||
                    found->second < 0.0) {
                    return -kInfinity;
                }
                immediate += found->second;
            }
        } else if (planner.kind == PlannerOperatorKind::FixedOption) {
            /* State-local automatic operators are published only after their
             * exact OptionKernel has replaced the planner's construction
             * quantities with the complete expected resource vector for
             * this carrier. Unlike an authored conditional fixed option,
             * that priced vector is an exact part of Q, so retaining only
             * the first primitive needlessly weakens the incumbent proof. */
            if (!std::isfinite(immediate) || immediate < 0.0) {
                return -kInfinity;
            }
        }
        if (!std::isfinite(immediate) || immediate < 0.0) {
            return -kInfinity;
        }
        if (operator_index == replacement_recovery_operator_index) {
            /* Restart has one exact successor: a fresh Normal carrier with
             * no affixes, influences, or Eldritch implicits. Do not credit it
             * with goal slots from the carrier it deterministically discards.
             *
             * This is deliberately narrower than consulting
             * ActionRefinementContract::destroyed_affixes. Those selectors
             * describe affixes an action may destroy (Annul is the canonical
             * example), not affixes absent from every successor. Removing
             * such slots here could raise the lower above a real outcome and
             * make incumbent pruning unsound. Restart's fresh successor is
             * instead exact by its synthetic action contract and evaluator.
             *
             * The clean pattern database is valid for that fresh carrier
             * only when its zero influence/implicit identity matches the
             * solve's clean-carrier identity. Otherwise retain the universal
             * goal cover, exactly as completion_proof_lower_value()
             * would for the materialized fresh successor. */
            const AbstractState& start = calc.state(result.start_state);
            const bool fresh_clean_carrier =
                start.influence_bits == 0 &&
                start.searing_exarch_tier == 0 &&
                start.eater_of_worlds_tier == 0;
            const double universal_fresh =
                optimistic_completion_cost(0);
            const double shaped_fresh =
                optimistic_completion_cost(
                    0, fresh_clean_carrier, PC_RARITY_NORMAL, 0, 0);
            /* Both relaxations are independently admissible. Their maximum
             * keeps the shape-aware refinement from accidentally weakening
             * the universal cover on a sparse action envelope. */
            return std::max(
                immediate + std::max(universal_fresh, shaped_fresh),
                carrier_action_bellman_lower_value(
                    state, record_pattern_owners));
        }
        /* Carry only source slots with at least one identity-preserving
         * runtime path, then grant every slot any constituent could possibly
         * produce. The union is a superset of every exact successor's goal
         * mask. Since the cover decreases monotonically as slots are added,
         * its value remains an admissible continuation lower bound. */
        const std::uint32_t optimistic_satisfied =
            planner_goal_may_survive_mask(state, operator_index) |
            planner_goal_reach_mask(operator_index);
        const double continuation =
            optimistic_completion_cost(optimistic_satisfied);
        return std::max(
            immediate + continuation,
            carrier_action_bellman_lower_value(
                state, record_pattern_owners));
    }

double SolveWork::Impl::carrier_action_bellman_lower_value(
        const std::uint32_t state,
        const bool record_pattern_owners) const {
        /* Each component is an independently proved global completion lower.
         * Their maximum is therefore a lower bound on every concrete Q value
         * through V*(state) <= Q(action, state), not executable policy or
         * closure authority. Unknown local shapes leave the progress
         * component unavailable and the terminal-debt component at zero. */
        const double debt = carrier_terminal_debt_lower_value(state);
        const double progress = carrier_goal_progress_lower_value(state);
        if (!record_pattern_owners) {
            return std::max(
                debt,
                std::isfinite(progress) ? progress : 0.0);
        }
        return select_maximum({
            {ProofPatternKind::TerminalDebt, {debt}, true},
            {ProofPatternKind::CarrierMdp, {progress},
             std::isfinite(progress)},
        }, kValueCeiling).lower.value;
    }

solve_detail::ProofLowerValue SolveWork::Impl::operator_proof_lower(
        const std::uint32_t state,
        const std::uint32_t operator_index) {
    const ProofLowerValue lower{
        operator_proof_lower_value(state, operator_index)};
    if (std::isfinite(lower.value) && lower.value >= 0.0) {
        ++contract(ProofPatternKind::OperatorLower)
              .selected_owner_calls;
    }
    return lower;
}

void SolveWork::Impl::audit_verified_incumbent_operator_proof_shadow(
        const BoundedPolicyIncumbent& incumbent) {
    if (!carrier_bound_attribution ||
        !incumbent.independently_certified ||
        !incumbent.independently_evaluated || !incumbent.proper ||
        !incumbent.executable || incumbent.values.empty()) {
        return;
    }
    using Work = CarrierBoundAttributionWork;
    auto& prior = carrier_bound_attribution
                      ->verified_incumbent_operator_shadow;
    const std::uint64_t incumbent_identity =
        incumbent.portfolio_identity != 0
        ? incumbent.portfolio_identity
        : incumbent.graph_identity;
    if (prior.audits != 0 &&
        prior.incumbent_identity == incumbent_identity) {
        return;
    }
    const std::uint64_t audits = prior.audits + 1;
    prior = {};
    prior.audits = audits;
    prior.incumbent_identity = incumbent_identity;
    prior.ledger_entries = action_envelope_ledger.entries().size();

    const auto sample_precedes = [](
            const Work::OperatorShadowSample& left,
            const Work::OperatorShadowSample& right) {
        if (left.absolute_margin != right.absolute_margin) {
            return left.absolute_margin < right.absolute_margin;
        }
        if (left.state != right.state) return left.state < right.state;
        return left.operator_index < right.operator_index;
    };
    for (const auto& [unused_key, entry] :
         action_envelope_ledger.entries()) {
        (void)unused_key;
        if (entry.state >= incumbent.values.size() ||
            entry.state >= calc.state_count() ||
            entry.operator_index >= calc.operators().size()) {
            continue;
        }
        const double upper = incumbent.values[entry.state];
        if (!std::isfinite(upper) || upper < 0.0 ||
            upper >= kValueCeiling) {
            continue;
        }
        ++prior.finite_upper_entries;
        const double lower = operator_proof_lower_value(
            entry.state, entry.operator_index, false);
        if (!std::isfinite(lower) || lower < 0.0 ||
            lower >= kValueCeiling) {
            continue;
        }
        ++prior.finite_lower_entries;
        const std::size_t family =
            carrier_bound_operator_family(entry.operator_index);
        const double separation = options.epsilon *
            std::max({1.0, std::abs(upper), std::abs(lower)});
        if (lower > upper + separation) {
            ++prior.would_retire;
            ++prior.would_retire_by_family.at(family);
            continue;
        }
        ++prior.still_competitive;
        ++prior.still_competitive_by_family.at(family);

        const AbstractState& state = calc.state(entry.state);
        Work::OperatorShadowSample sample;
        sample.state = entry.state;
        sample.operator_index = entry.operator_index;
        sample.satisfied_goal_mask =
            satisfied_goal_mask_for_state(entry.state);
        sample.blocked_mask = state.blocked_mask;
        sample.prefix_count = state.prefix_count;
        sample.suffix_count = state.suffix_count;
        const std::uint32_t occupied =
            state.prefix_count + state.suffix_count;
        const std::uint32_t satisfied =
            std::popcount(sample.satisfied_goal_mask);
        sample.unrelated_occupancy = static_cast<std::uint8_t>(
            std::min<std::uint32_t>(
                Work::kUnrelatedOccupancyCount - 1,
                occupied > satisfied ? occupied - satisfied : 0));
        sample.lifecycle = entry.lifecycle;
        sample.lower = lower;
        sample.upper = upper;
        sample.absolute_margin = std::abs(upper - lower);

        auto begin = prior.closest_competitive.begin();
        auto end = begin + prior.closest_competitive_count;
        if (prior.closest_competitive_count <
            Work::kOperatorShadowSampleLimit) {
            prior.closest_competitive[prior.closest_competitive_count++] =
                sample;
            end = begin + prior.closest_competitive_count;
            std::sort(begin, end, sample_precedes);
        } else if (sample_precedes(sample, *(end - 1))) {
            *(end - 1) = sample;
            std::sort(begin, end, sample_precedes);
        }
    }
}

bool SolveWork::Impl::retire_unmaterialized_by_operator_proof(
        const std::uint32_t state,
        const std::uint32_t operator_index) {
    if (state >= incremental_certified_upper_values.size() ||
        operator_index >= calc.operators().size()) {
        return false;
    }
    const double upper = incremental_certified_upper_values[state];
    if (!std::isfinite(upper) || upper < 0.0 || upper >= kValueCeiling) {
        return false;
    }
    const PlannerOperator& planner = calc.operators()[operator_index];
    if (planner.kind == PlannerOperatorKind::FixedOption &&
        planner.automatic_kind == AutomaticCandidateKind::None) {
        /* The retained authored-program lower charges only its guaranteed
         * first step. It is admissible, but Gate 7 requires a complete
         * immediate program authority before descriptor retirement. */
        return false;
    }
    ++descriptor_proof_evaluations;
    const double lower = operator_proof_lower_value(state, operator_index);
    if (!std::isfinite(lower) || lower < 0.0) return false;
    const double separation = options.epsilon *
        std::max({1.0, std::abs(upper), std::abs(lower)});
    if (lower <= upper + separation) return false;

    const std::uint32_t evidence =
        EnvelopeEvidenceCarrierFacts |
        EnvelopeEvidenceCarrierEffectSummary |
        EnvelopeEvidenceCarrierSuccessorEnvelope |
        EnvelopeEvidenceActionRefinementContract;
    action_envelope_ledger.incumbent_dominated(
        state, operator_index,
        std::numeric_limits<std::uint64_t>::max(), evidence);
    incremental_completed_pairs.insert(
        ActionEnvelopeLedger::key(state, operator_index));
    ++descriptor_proof_separations;
    record_operator_lower_attribution(
        operator_index, lower, upper, true, false);
    return true;
}

void SolveWork::Impl::retire_certified_unmaterialized_obligations() {
    struct Candidate {
        std::uint32_t state = kNoId;
        std::uint32_t operator_index = kNoId;
        ActionEnvelopeState lifecycle = ActionEnvelopeState::Queued;
    };
    std::vector<Candidate> candidates;
    candidates.reserve(action_envelope_ledger.entries().size());
    for (const auto& [unused_key, entry] :
         action_envelope_ledger.entries()) {
        (void)unused_key;
        if ((entry.lifecycle != ActionEnvelopeState::Queued &&
             entry.lifecycle != ActionEnvelopeState::UnresolvedNamedStop) ||
            entry.row_index != std::numeric_limits<std::uint64_t>::max()) {
            continue;
        }
        candidates.push_back({
            entry.state, entry.operator_index, entry.lifecycle});
    }
    for (const Candidate& candidate : candidates) {
        if (!retire_unmaterialized_by_operator_proof(
                candidate.state, candidate.operator_index)) {
            continue;
        }
        if (candidate.lifecycle == ActionEnvelopeState::Queued &&
            incremental_unevaluated_actions != 0) {
            --incremental_unevaluated_actions;
        }
    }
}

} // namespace solver
} // namespace poecraft
