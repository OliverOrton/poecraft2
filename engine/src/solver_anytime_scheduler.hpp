#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>

namespace poecraft::solver::solve_detail {

enum class AnytimeSchedulerLane : std::uint8_t {
    LegacyFairness = 0,
    ExecutableUpper,
    HighProgress,
    ExactClosure,
    ProofDirected,
    Count,
};

inline constexpr std::size_t kAnytimeSchedulerLaneCount =
    static_cast<std::size_t>(AnytimeSchedulerLane::Count);

inline constexpr std::string_view anytime_scheduler_lane_name(
        const AnytimeSchedulerLane lane) {
    switch (lane) {
    case AnytimeSchedulerLane::LegacyFairness: return "legacy_fairness";
    case AnytimeSchedulerLane::ExecutableUpper: return "executable_upper";
    case AnytimeSchedulerLane::HighProgress: return "high_progress";
    case AnytimeSchedulerLane::ExactClosure: return "exact_closure";
    case AnytimeSchedulerLane::ProofDirected: return "proof_directed";
    case AnytimeSchedulerLane::Count: break;
    }
    return "invalid";
}

/*
 * A versioned work-order profile, not mechanic or proof authority. Values that
 * used to appear as anonymous branch constants live here and are serialized
 * with the scheduler telemetry. One service is one cooperatively resumable
 * row, automatic-preparation unit, or refinement selection boundary.
 */
struct AnytimeSchedulingProfile {
    std::string_view id = "solver_anytime_balanced_v1";
    std::array<std::uint32_t, kAnytimeSchedulerLaneCount> lane_quota{
        2, 2, 3, 1, 0};
    std::uint32_t q_refinement_batch = 128;
    std::size_t first_incumbent_checkpoint_rows = 64;
    std::uint32_t starvation_dispatches = 8;
    double material_upper_improvement_ratio = 0.01;
};

inline constexpr AnytimeSchedulingProfile kAnytimeSchedulingProfile{};

inline constexpr AnytimeSchedulingProfile kFocusedAnytimeSchedulingProfile{
    "solver_anytime_focused_v2",
    {1, 0, 15, 0, 0},
    128,
    64,
    16,
    0.01,
};

/* Gate 3 fallback for already-progressed starts. These values name and expose
 * the previously proven continuation producer while clean acquisition uses
 * the cooperative lane profile above. They are work-order authority only. */
struct WarmStartCompatibilityProfile {
    std::string_view id = "solver_anytime_warm_start_compat_v1";
    std::size_t carrier_checkpoint = 128;
    std::uint32_t continuation_batch = 16;
    std::uint32_t policy_waves = 4;
    double continuation_fracture_weight = 1024.0;
    double continuation_goal_weight = 64.0;
    double wave_fracture_weight = 4096.0;
    double wave_goal_weight = 256.0;
};

inline constexpr WarmStartCompatibilityProfile
    kWarmStartCompatibilityProfile{};

struct AnytimeLaneTelemetry {
    std::uint64_t quota = 0;
    std::uint64_t offers = 0;
    std::uint64_t services = 0;
    std::uint64_t waits = 0;
    std::uint64_t yields = 0;
    std::uint64_t improvements = 0;
    std::uint64_t starvation_events = 0;
    std::uint64_t current_wait = 0;
    std::uint64_t maximum_wait = 0;
};

class SolveScheduler {
public:
    using Availability =
        std::array<bool, kAnytimeSchedulerLaneCount>;

    explicit SolveScheduler(
            const AnytimeSchedulingProfile& profile =
                kAnytimeSchedulingProfile)
        : profile_(profile) {
        for (std::size_t lane = 0; lane < lanes_.size(); ++lane) {
            lanes_[lane].quota = profile_.lane_quota[lane];
        }
    }

    const AnytimeSchedulingProfile& profile() const { return profile_; }
    const std::array<AnytimeLaneTelemetry, kAnytimeSchedulerLaneCount>&
    lanes() const { return lanes_; }
    std::uint64_t dispatches() const { return dispatches_; }

    AnytimeSchedulerLane select(const Availability& available) {
        const std::size_t ticket_count = weighted_ticket_count();
        if (ticket_count == 0) return AnytimeSchedulerLane::Count;
        for (std::size_t attempt = 0; attempt < ticket_count; ++attempt) {
            const std::size_t lane = lane_for_ticket(ticket_cursor_);
            ticket_cursor_ = (ticket_cursor_ + 1) % ticket_count;
            ++lanes_[lane].offers;
            if (!available[lane]) {
                ++lanes_[lane].yields;
                continue;
            }
            ++dispatches_;
            for (std::size_t other = 0; other < lanes_.size(); ++other) {
                if (!available[other]) {
                    lanes_[other].current_wait = 0;
                    continue;
                }
                if (other == lane) {
                    ++lanes_[other].services;
                    lanes_[other].current_wait = 0;
                    continue;
                }
                ++lanes_[other].waits;
                ++lanes_[other].current_wait;
                lanes_[other].maximum_wait =
                    std::max(
                        lanes_[other].maximum_wait,
                        lanes_[other].current_wait);
                if (profile_.starvation_dispatches != 0 &&
                    lanes_[other].current_wait %
                            profile_.starvation_dispatches ==
                        0) {
                    ++lanes_[other].starvation_events;
                }
            }
            return static_cast<AnytimeSchedulerLane>(lane);
        }
        return AnytimeSchedulerLane::Count;
    }

    /* Strict tickets return unused quota to the outer solver phase instead of
     * donating it to another lane. This is the incremental scheduler's
     * cooperative-yield boundary: graph expansion and refinement regain the
     * caller's next work unit when a lane has no resumable obligation. */
    AnytimeSchedulerLane select_ticket(const Availability& available) {
        const std::size_t ticket_count = weighted_ticket_count();
        if (ticket_count == 0) return AnytimeSchedulerLane::Count;
        const std::size_t lane = lane_for_ticket(ticket_cursor_);
        ticket_cursor_ = (ticket_cursor_ + 1) % ticket_count;
        ++lanes_[lane].offers;
        if (!available[lane]) {
            ++lanes_[lane].yields;
            return AnytimeSchedulerLane::Count;
        }
        record_service(lane, available);
        return static_cast<AnytimeSchedulerLane>(lane);
    }

    void record_yield(const AnytimeSchedulerLane lane) {
        if (lane == AnytimeSchedulerLane::Count) return;
        ++lanes_.at(static_cast<std::size_t>(lane)).yields;
    }

    void record_improvement(const AnytimeSchedulerLane lane) {
        if (lane == AnytimeSchedulerLane::Count) return;
        ++lanes_.at(static_cast<std::size_t>(lane)).improvements;
    }

private:
    AnytimeSchedulingProfile profile_;
    std::array<AnytimeLaneTelemetry, kAnytimeSchedulerLaneCount> lanes_{};
    std::size_t ticket_cursor_ = 0;
    std::uint64_t dispatches_ = 0;

    std::size_t weighted_ticket_count() const {
        std::size_t total = 0;
        for (const std::uint32_t quota : profile_.lane_quota) total += quota;
        return total;
    }

    std::size_t lane_for_ticket(const std::size_t ticket) const {
        std::size_t cursor = ticket;
        for (std::size_t lane = 0; lane < profile_.lane_quota.size(); ++lane) {
            if (cursor < profile_.lane_quota[lane]) return lane;
            cursor -= profile_.lane_quota[lane];
        }
        return profile_.lane_quota.size() - 1;
    }

    void record_service(
            const std::size_t lane,
            const Availability& available) {
        ++dispatches_;
        for (std::size_t other = 0; other < lanes_.size(); ++other) {
            if (!available[other]) {
                lanes_[other].current_wait = 0;
                continue;
            }
            if (other == lane) {
                ++lanes_[other].services;
                lanes_[other].current_wait = 0;
                continue;
            }
            ++lanes_[other].waits;
            ++lanes_[other].current_wait;
            lanes_[other].maximum_wait = std::max(
                lanes_[other].maximum_wait,
                lanes_[other].current_wait);
            if (profile_.starvation_dispatches != 0 &&
                lanes_[other].current_wait %
                        profile_.starvation_dispatches == 0) {
                ++lanes_[other].starvation_events;
            }
        }
    }
};

} // namespace poecraft::solver::solve_detail
