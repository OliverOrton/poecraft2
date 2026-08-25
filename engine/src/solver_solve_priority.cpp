#include "solver_solve_types.hpp"

namespace poecraft {
namespace solver {
namespace solve_detail {

CarrierOrderingScore make_carrier_ordering_score(
        const AbstractState& carrier,
        const std::uint32_t state,
        const std::uint32_t goal_subset,
        const double focused_priority) {
    const std::uint32_t satisfied = std::popcount(goal_subset);
    const std::uint32_t explicit_affixes =
        carrier.prefix_count + carrier.suffix_count;
    return {
        state,
        goal_subset,
        satisfied,
        static_cast<std::uint32_t>(
            std::popcount(carrier.fractured_goal_mask)),
        static_cast<std::uint32_t>(std::popcount(
            carrier.flags &
            (kFlagPrefixesLocked | kFlagSuffixesLocked))),
        explicit_affixes > satisfied
            ? explicit_affixes - satisfied
            : 0,
        focused_priority,
    };
}

CarrierPriorityBuckets build_carrier_priority_buckets(
        const std::vector<CarrierOrderingScore>& candidates,
        const CarrierOrderingMode mode) {
    CarrierPriorityBuckets result;
    std::map<std::uint32_t, CarrierOrderingScore> first_score;
    std::uint32_t maximum_state = 0;
    for (const CarrierOrderingScore& candidate : candidates) {
        maximum_state = std::max(maximum_state, candidate.state);
    }
    std::vector<const CarrierOrderingScore*> score_by_state(
        candidates.empty() ? 0 : static_cast<std::size_t>(maximum_state) + 1,
        nullptr);
    for (const CarrierOrderingScore& candidate : candidates) {
        score_by_state.at(candidate.state) = &candidate;
        auto& bucket = result.by_goal_subset[candidate.goal_subset];
        if (bucket.empty()) first_score.emplace(
            candidate.goal_subset, candidate);
        bucket.push_back(candidate.state);
    }
    result.subset_order.reserve(result.by_goal_subset.size());
    for (auto& [mask, carriers] : result.by_goal_subset) {
        if (mode != CarrierOrderingMode::FocusedLegacy) {
            std::stable_sort(
                carriers.begin(), carriers.end(),
                [&](const std::uint32_t left,
                    const std::uint32_t right) {
                    const auto score_for = [&](const std::uint32_t state)
                            -> const CarrierOrderingScore& {
                        if (state >= score_by_state.size() ||
                            score_by_state[state] == nullptr) {
                            throw std::logic_error(
                                "carrier priority lost a candidate score");
                        }
                        return *score_by_state[state];
                    };
                    const CarrierOrderingScore& a = score_for(left);
                    const CarrierOrderingScore& b = score_for(right);
                    if (a.fractured_goals != b.fractured_goals) {
                        return a.fractured_goals > b.fractured_goals;
                    }
                    if (a.active_protection != b.active_protection) {
                        return a.active_protection > b.active_protection;
                    }
                    return a.unrelated_occupancy != b.unrelated_occupancy
                        ? a.unrelated_occupancy < b.unrelated_occupancy
                        : a.state < b.state;
                });
        }
        result.subset_order.push_back(mask);
    }
    std::stable_sort(
        result.subset_order.begin(), result.subset_order.end(),
        [&](const std::uint32_t left, const std::uint32_t right) {
            const CarrierOrderingScore& a = first_score.at(left);
            const CarrierOrderingScore& b = first_score.at(right);
            if (a.satisfied_goals != b.satisfied_goals) {
                return a.satisfied_goals > b.satisfied_goals;
            }
            if (mode != CarrierOrderingMode::IncrementalLegacy &&
                a.focused_priority != b.focused_priority) {
                return a.focused_priority > b.focused_priority;
            }
            return left < right;
        });
    return result;
}

} // namespace solve_detail

using namespace solve_detail;

CarrierOrderingScore SolveWork::Impl::carrier_ordering_score(
        const std::uint32_t state,
        const double focused_priority) {
    return make_carrier_ordering_score(
        calc.state(state), state, satisfied_goal_mask_for_state(state),
        focused_priority);
}

} // namespace solver
} // namespace poecraft
