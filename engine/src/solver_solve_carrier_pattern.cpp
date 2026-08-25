#include "solver_solve_types.hpp"

namespace poecraft {
namespace solver {

using namespace solve_detail;

std::uint32_t SolveWork::Impl::clean_goal_cover_rejection_mask(
        const std::uint32_t state) const {
        using Rejection =
            CarrierBoundAttributionWork::CleanCoverRejection;
        if (state >= calc.state_count() ||
            result.start_state >= calc.state_count()) {
            return Rejection::InvalidState;
        }
        const AbstractState& carrier = calc.state(state);
        const AbstractState& start = calc.state(result.start_state);
        std::uint32_t rejection = 0;
        if ((carrier.flags & kProtectionFlags) != 0) {
            rejection |= Rejection::ActiveProtection;
        }
        if (carrier.fractured_goal_mask != 0) {
            rejection |= Rejection::FracturedGoal;
        }
        if (carrier.fractured_metamod_flags != 0) {
            rejection |= Rejection::FracturedMetamod;
        }
        if (carrier.influence_bits != start.influence_bits) {
            rejection |= Rejection::InfluenceIdentity;
        }
        if (carrier.searing_exarch_tier != start.searing_exarch_tier) {
            rejection |= Rejection::SearingIdentity;
        }
        if (carrier.eater_of_worlds_tier != start.eater_of_worlds_tier) {
            rejection |= Rejection::EaterIdentity;
        }
        for (const std::uint8_t count : carrier.fractured_junk_counts) {
            if (count != 0) {
                rejection |= Rejection::FracturedJunk;
                break;
            }
        }
        for (const std::uint8_t count :
             carrier.fractured_crafted_junk_counts) {
            if (count != 0) {
                rejection |= Rejection::FracturedCraftedJunk;
                break;
            }
        }
        return rejection;
    }

bool SolveWork::Impl::clean_goal_cover_eligible(
        const std::uint32_t state) const {
        return clean_goal_cover_rejection_mask(state) == 0;
    }

bool SolveWork::Impl::carrier_goal_progress_eligible(
        const std::uint32_t state) const {
        if (state >= calc.state_count() ||
            result.start_state >= calc.state_count()) {
            return false;
        }
        if (carrier_goal_progress_eligibility_cache.size() <= state) {
            carrier_goal_progress_eligibility_cache.resize(state + 1, -1);
        }
        if (carrier_goal_progress_eligibility_cache[state] >= 0) {
            return carrier_goal_progress_eligibility_cache[state] != 0;
        }
        const AbstractState& carrier = calc.state(state);
        const AbstractState& start = calc.state(result.start_state);
        /* Generic influence bits select a different explicit-affix pool.
         * The current probability anchor does not prove an upper probability
         * for that identity, so retain the universal cover locally. Eldritch
         * implicit tiers do not select the explicit pool; automatic side
         * options are represented above by their cheaper final primitive
         * with setup/dominance granted for free. */
        if (carrier.influence_bits != start.influence_bits) {
            carrier_goal_progress_eligibility_cache[state] = 0;
            return false;
        }
        for (const std::uint32_t action_index :
             carrier_unproved_first_step_actions) {
            const ActionDescriptor& action =
                calc.registry().actions[action_index];
            if (action_legal(session, action, carrier)) {
                /* A legal zero-resource or unpriced resolution can make
                 * useful progress before a represented priced action. The
                 * carrier table deliberately makes no claim for that local
                 * shape. */
                carrier_goal_progress_eligibility_cache[state] = 0;
                return false;
            }
        }
        carrier_goal_progress_eligibility_cache[state] = 1;
        return true;
    }

double SolveWork::Impl::carrier_goal_progress_lower_value(
        const std::uint32_t state) const {
        if (!carrier_goal_progress_eligible(state)) return kInfinity;
        constexpr std::size_t kCarrierRarityCount = 3;
        const std::uint32_t satisfied =
            satisfied_goal_mask_for_state(state);
        if (carrier_goal_progress_cost.empty() ||
            carrier_goal_progress_cost.size() % kCarrierRarityCount != 0) {
            return kInfinity;
        }
        const std::size_t mask_count =
            carrier_goal_progress_cost.size() / kCarrierRarityCount;
        const AbstractState& carrier = calc.state(state);
        const std::size_t index =
            static_cast<std::size_t>(carrier.rarity) * mask_count +
            satisfied;
        return index < carrier_goal_progress_cost.size()
            ? carrier_goal_progress_cost[index]
            : kInfinity;
}

double SolveWork::Impl::carrier_terminal_debt_lower_value(
        const std::uint32_t state) const {
        if (state >= calc.state_count() ||
            calc.is_goal_state(calc.state(state))) {
            return 0.0;
        }
        if (carrier_terminal_debt_cache.size() <= state) {
            carrier_terminal_debt_cache.resize(
                state + 1, std::numeric_limits<double>::quiet_NaN());
        }
        if (!std::isnan(carrier_terminal_debt_cache[state])) {
            return carrier_terminal_debt_cache[state];
        }
        const AbstractState& carrier = calc.state(state);
        for (const std::uint32_t action_index :
             carrier_unproved_first_step_actions) {
            if (action_legal(
                    session, calc.registry().actions[action_index], carrier)) {
                carrier_terminal_debt_cache[state] = 0.0;
                return 0.0;
            }
        }
        double local_floor = kInfinity;
        for (const auto& [action_index, cost] :
             carrier_priced_first_step_actions) {
            if (cost >= local_floor) continue;
            if (action_legal(
                    session, calc.registry().actions[action_index], carrier)) {
                local_floor = cost;
            }
        }
        /* Every executable row begins with a carrier-legal registry
         * primitive. Its complete immediate resource cost is at least this
         * cheapest priced first step; fixed programs and automatic Imprint
         * add nonnegative resources. Charge that step once, then grant
         * arbitrary cleanup/replacement plus terminal success. In particular,
         * never charge cleanup before a protected reforge that can replace
         * junk directly. Unknown/unpriced first steps retain zero locally. */
        const double lower = std::isfinite(local_floor) ? local_floor : 0.0;
        carrier_terminal_debt_cache[state] = lower;
        return lower;
    }

double SolveWork::Impl::completion_proof_lower_value(
        const std::uint32_t state) {
        if (state >= calc.state_count()) return 0.0;
        const AbstractState& carrier = calc.state(state);
        const std::uint32_t satisfied =
            satisfied_goal_mask_for_state(state);
        const double universal =
            optimistic_completion_cost(satisfied);
        const double clean = optimistic_completion_cost(
            satisfied,
            clean_goal_cover_eligible(state), carrier.rarity,
            carrier.prefix_count, carrier.suffix_count);
        const double carrier_progress =
            carrier_goal_progress_lower_value(state);
        const double terminal_debt =
            carrier_terminal_debt_lower_value(state);
        /* The universal goal cover is independently admissible for every
         * carrier shape. A clean/strict specialization may strengthen it but
         * must never replace it with a weaker value; exact successor Bellman
         * checks rely on that common lower component. */
        /* The strict clean pattern database evaluates registry primitives,
         * not carrier-local automatic option rows. Keep it out of the
         * Eldritch maximum until that stricter abstraction has the same
         * option coverage as the coarse/clean goal cover above. */
        const bool strict_available = !session.eldritch_eligible &&
            state < strict_clean_goal_cover_cost.size() &&
            std::isfinite(strict_clean_goal_cover_cost[state]);
        const double strict = strict_available
            ? strict_clean_goal_cover_cost[state]
            : kInfinity;
        const ProofPatternSelection selected = select_maximum({
            {ProofPatternKind::UniversalCover, {universal}, true},
            {ProofPatternKind::CleanMdp, {clean}, true},
            {ProofPatternKind::CarrierMdp, {carrier_progress},
             std::isfinite(carrier_progress)},
            {ProofPatternKind::TerminalDebt, {terminal_debt}, true},
            {ProofPatternKind::StrictClean, {strict}, strict_available},
        }, kValueCeiling);
        /*
         * An infinite abstract value means the finite relaxation omitted a
         * carrier shape; it is not proof that the concrete state cannot
         * restart or otherwise finish. Fall back to the universal zero
         * lower rather than turning abstraction coverage into a false
         * non-improvement certificate.
         */
        return selected.lower.value;
    }

solve_detail::ProofLowerValue SolveWork::Impl::completion_proof_lower(
        const std::uint32_t state) {
    return {completion_proof_lower_value(state)};
}

} // namespace solver
} // namespace poecraft
