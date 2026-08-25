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
        const std::uint32_t operator_index) {
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
                carrier_action_bellman_lower_value(state));
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
            carrier_action_bellman_lower_value(state));
    }

double SolveWork::Impl::carrier_action_bellman_lower_value(
        const std::uint32_t state) const {
        /* Each component is an independently proved global completion lower.
         * Their maximum is therefore a lower bound on every concrete Q value
         * through V*(state) <= Q(action, state), not executable policy or
         * closure authority. Unknown local shapes leave the progress
         * component unavailable and the terminal-debt component at zero. */
        const double debt = carrier_terminal_debt_lower_value(state);
        const double progress = carrier_goal_progress_lower_value(state);
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

} // namespace solver
} // namespace poecraft
