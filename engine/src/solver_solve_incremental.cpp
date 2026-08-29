#include "solver_solve_types.hpp"
#include "solver_action_family_contract.hpp"
#include "solver_sparse_policy.hpp"

namespace poecraft {
namespace solver {

using namespace solve_detail;

double q_directed_uncertainty_contribution(
        const double probability,
        const double lower,
        const double upper) {
    if (!std::isfinite(probability) || probability < 0.0 ||
        !std::isfinite(lower) || !std::isfinite(upper) ||
        upper < lower) {
        return kInfinity;
    }
    return probability * (upper - lower);
}

void SolveWork::Impl::retain_incremental_carrier(
        const std::uint32_t state) {
    if (!incremental_action_generation ||
        calc.is_goal_state(calc.state(state))) {
        return;
    }
    incremental_carriers.push_back(state);
    incremental_unevaluated_actions += delayed_operator_indices.size();
    for (const std::uint32_t operator_index : delayed_operator_indices) {
        action_envelope_ledger.queue(
            state, operator_index,
            ActionEnvelopeLane::IncrementalCarrierLocal,
            EnvelopeEvidenceCarrierFacts |
                EnvelopeEvidenceCarrierEffectSummary |
                EnvelopeEvidenceActionRefinementContract);
    }
    if (options.high_impact_executable_uppers) {
        /* Automatic preparation is itself an open envelope obligation. Its
         * materialized operators are counted only after synthesis completes. */
        ++incremental_unevaluated_actions;
    }
}

bool SolveWork::Impl::advance_incremental_dynamic_preparation() {
    const bool automatic_order =
        options.high_impact_executable_uppers;
    const std::size_t carrier_cursor = automatic_order
        ? incremental_automatic_order_cursor
        : incremental_carrier_cursor;
    const std::size_t carrier_count = automatic_order
        ? incremental_automatic_carrier_order.size()
        : incremental_carriers.size();
    if (carrier_cursor >= carrier_count) {
        throw std::logic_error(
            "incremental automatic preparation lost its carrier");
    }
    const std::uint32_t state = automatic_order
        ? incremental_automatic_carrier_order[carrier_cursor]
        : incremental_carriers[carrier_cursor];
    const std::uint64_t unresolved_before =
        incremental_resource_unresolved_actions;
    try {
        if (!prepare_state_expansion(state, true)) return false;
    } catch (const SolverResourceLimit& limit) {
        if (incremental_resource_unresolved_actions == unresolved_before) {
            ++incremental_resource_unresolved_actions;
        }
        action_envelope_ledger.rolled_back_after_cap(
            state, kNoId, ActionEnvelopeLane::IncrementalAutomatic,
            limit.cap_name(),
            EnvelopeEvidenceCarrierFacts |
                EnvelopeEvidenceCarrierEffectSummary |
                EnvelopeEvidenceCarrierSuccessorEnvelope |
                EnvelopeEvidenceActionRefinementContract);
        incremental_envelope_closed = false;
        throw;
    }
    incremental_dynamic_operator_indices.clear();
    for (const std::uint32_t candidate : expansion_operator_indices) {
        if (std::find(
                static_operator_indices.begin(),
                static_operator_indices.end(),
                candidate) == static_operator_indices.end()) {
            incremental_dynamic_operator_indices.push_back(candidate);
        }
    }
    for (const std::uint32_t operator_index :
         incremental_dynamic_operator_indices) {
        action_envelope_ledger.queue(
            state, operator_index,
            ActionEnvelopeLane::IncrementalAutomatic,
            EnvelopeEvidenceCarrierFacts |
                EnvelopeEvidenceCarrierEffectSummary |
                EnvelopeEvidenceCarrierSuccessorEnvelope |
                EnvelopeEvidenceActionRefinementContract);
    }
    incremental_dynamic_prepared = true;
    incremental_dynamic_prepare_active = false;
    incremental_dynamic_operator_cursor = 0;
    if (options.high_impact_executable_uppers &&
        incremental_unevaluated_actions != 0) {
        --incremental_unevaluated_actions;
    }
    incremental_unevaluated_actions +=
        incremental_dynamic_operator_indices.size();
    if (cooperative_high_progress_ordering_enabled()) {
        prioritize_carrier_actions(
            state, incremental_dynamic_operator_indices);
    }
    expansion_operator_indices.clear();
    return true;
}

bool SolveWork::Impl::schedule_next_incremental_alternative(
        const bool continue_current_epoch_request) {
    if (!incremental_action_generation || incremental_envelope_closed) {
        return false;
    }
    const bool frozen_automatic_epoch_pending =
        options.high_impact_executable_uppers &&
        incremental_automatic_order_cursor <
            incremental_automatic_carrier_order.size();
    const bool continue_current_epoch =
        continue_current_epoch_request ||
        incremental_resume_epoch_after_dynamic_prepare ||
        frozen_automatic_epoch_pending;
    incremental_resume_epoch_after_dynamic_prepare = false;
    if (!continue_current_epoch) {
        incremental_epoch_added_states = false;
        if (incremental_automatic_carrier_cursor <
            incremental_carriers.size()) {
            /* Freeze the currently reachable automatic carrier frontier.
             * Support discovered by this epoch is expanded before the next
             * epoch and therefore cannot move this boundary underneath the
             * running carrier-local admission pass. */
            incremental_automatic_epoch_end =
                incremental_carriers.size();

            /* The product path reaches carrier-local automatic admission
             * before the later focused-fringe scheduler. Stratify this real
             * frozen epoch by complete goal subset so discovery order cannot
             * make common zero-progress carriers consume the whole bounded
             * solve. This changes only work order: every carrier remains in
             * the exact append-only obligation and the final Cartesian scan
             * still owns closure.
             *
             * Round-robin across masks preserves direct subset jumps and
             * prevents one popular 4/5 shape from excluding other feasible
             * ladders. Within a mask, already-fractured/protected carriers
             * and carriers with less unrelated occupancy go first; dirty
             * carriers are delayed, never rejected. */
            const std::size_t begin =
                incremental_automatic_carrier_cursor;
            const std::size_t end = incremental_automatic_epoch_end;
            std::vector<solve_detail::CarrierOrderingScore>
                carrier_candidates;
            carrier_candidates.reserve(end - begin);
            for (std::size_t index = begin; index < end; ++index) {
                const std::uint32_t state = incremental_carriers[index];
                carrier_candidates.push_back(
                    carrier_ordering_score(state));
                record_carrier_schedule_attribution(
                    CarrierBoundAttributionWork::ScheduleStage::
                        IncrementalCandidate,
                    state);
            }
            const CarrierOrderingMode automatic_ordering_mode =
                cooperative_high_progress_ordering_enabled()
                    ? CarrierOrderingMode::CooperativeHighProgress
                    : CarrierOrderingMode::IncrementalLegacy;
            solve_detail::CarrierPriorityBuckets carrier_buckets =
                solve_detail::build_carrier_priority_buckets(
                    carrier_candidates, automatic_ordering_mode);
            std::map<std::uint32_t, std::size_t> cursors;
            std::vector<std::uint32_t> ordered;
            ordered.reserve(end - begin);
            std::unordered_set<std::uint32_t> ordered_members;
            ordered_members.reserve(end - begin);
            /* A failed joint-policy proof names the exact carrier whose
             * continuation is missing. Expansion alone does not complete its
             * carrier-local automatic rows, so preserve that witness through
             * the second scheduling stage and put it ahead of the ordinary
             * fair ladder. This is executable-upper work ordering only. */
            for (const std::uint32_t urgent :
                 incremental_anytime_missing_frontier_states) {
                const auto found = std::find(
                    incremental_carriers.begin() + begin,
                    incremental_carriers.begin() + end, urgent);
                if (found != incremental_carriers.begin() + end &&
                    ordered_members.insert(urgent).second) {
                    ordered.push_back(urgent);
                    ++incremental_missing_frontier_priority_offers;
                }
            }
            bool advanced = true;
            while (advanced) {
                advanced = false;
                for (const std::uint32_t mask :
                     carrier_buckets.subset_order) {
                    auto& carriers =
                        carrier_buckets.by_goal_subset.at(mask);
                    std::size_t& cursor = cursors[mask];
                    while (cursor < carriers.size()) {
                        const std::uint32_t carrier = carriers[cursor++];
                        if (!ordered_members.insert(carrier).second) {
                            continue;
                        }
                        ordered.push_back(carrier);
                        advanced = true;
                        break;
                    }
                }
            }
            incremental_automatic_carrier_order = std::move(ordered);
            incremental_automatic_order_cursor = 0;
            ++incremental_carrier_ladder_epochs;
            incremental_carrier_ladder_candidates +=
                incremental_automatic_carrier_order.size();
            incremental_carrier_ladder_goal_subsets +=
                carrier_buckets.by_goal_subset.size();
        } else {
            incremental_automatic_carrier_order.clear();
            incremental_automatic_order_cursor = 0;
        }
        const auto build_round_robin_order = [&] (
                const std::size_t begin,
                const std::size_t end,
                const CarrierOrderingMode mode) {
            std::vector<CarrierOrderingScore> candidates;
            candidates.reserve(end - begin);
            for (std::size_t index = begin; index < end; ++index) {
                candidates.push_back(
                    carrier_ordering_score(incremental_carriers[index]));
            }
            CarrierPriorityBuckets buckets =
                build_carrier_priority_buckets(candidates, mode);
            std::map<std::uint32_t, std::size_t> cursors;
            std::vector<std::uint32_t> order;
            order.reserve(end - begin);
            bool advanced = true;
            while (advanced) {
                advanced = false;
                for (const std::uint32_t mask : buckets.subset_order) {
                    auto& carriers = buckets.by_goal_subset.at(mask);
                    std::size_t& cursor = cursors[mask];
                    if (cursor >= carriers.size()) continue;
                    order.push_back(carriers[cursor++]);
                    advanced = true;
                }
            }
            return order;
        };
        if (incremental_fairness_carrier_cursor >=
                incremental_fairness_carrier_order.size() &&
            incremental_fairness_epoch_end < incremental_carriers.size()) {
            const std::size_t begin = incremental_fairness_epoch_end;
            incremental_fairness_epoch_end = incremental_carriers.size();
            incremental_fairness_carrier_order = build_round_robin_order(
                begin, incremental_fairness_epoch_end,
                CarrierOrderingMode::IncrementalLegacy);
            incremental_fairness_carrier_cursor = 0;
            incremental_fairness_operator_cursor = 0;
            incremental_closure_carrier_cursor = 0;
            incremental_closure_operator_cursor = 0;
        }
        if (incremental_high_progress_carrier_cursor >=
                incremental_high_progress_carrier_order.size() &&
            incremental_high_progress_epoch_end <
                incremental_carriers.size()) {
            const std::size_t begin = incremental_high_progress_epoch_end;
            incremental_high_progress_epoch_end =
                incremental_carriers.size();
            incremental_high_progress_carrier_order =
                build_round_robin_order(
                    begin, incremental_high_progress_epoch_end,
                    cooperative_high_progress_ordering_enabled()
                        ? CarrierOrderingMode::CooperativeHighProgress
                        : CarrierOrderingMode::IncrementalLegacy);
            incremental_high_progress_carrier_cursor = 0;
            incremental_high_progress_operator_order.clear();
            incremental_high_progress_operator_cursor = 0;
        }
    }
    if (options.high_impact_executable_uppers) {
        const auto schedule_pair =
            [&](const std::uint32_t state,
                const std::uint32_t operator_index,
                const ActionEnvelopeLane lane) {
                action_envelope_ledger.queue(
                    state, operator_index, lane,
                    EnvelopeEvidenceCarrierFacts |
                        EnvelopeEvidenceCarrierEffectSummary |
                        EnvelopeEvidenceActionRefinementContract);
                if (incremental_unevaluated_actions != 0) {
                    --incremental_unevaluated_actions;
                }
                if (retire_unmaterialized_by_operator_proof(
                        state, operator_index)) {
                    return;
                }
                expansion_state = state;
                expansion_operator_indices.assign(1, operator_index);
                expansion_operator_cursor = 0;
                expansion_active = true;
                expansion_prepared = true;
                expansion_is_incremental_alternative = true;
                expansion_incremental_resource_limited = false;
                expansion_appended_row =
                    std::numeric_limits<std::uint64_t>::max();
                expansion_states_outside_chaos_support = 0;
                if (incremental_first_alternative_expanded_states == 0) {
                    incremental_first_alternative_expanded_states =
                        expanded_count;
                }
                record_carrier_schedule_attribution(
                    CarrierBoundAttributionWork::ScheduleStage::
                        CarrierActionAdmission,
                    state, operator_index);
                switch (lane) {
                case ActionEnvelopeLane::IncrementalAutomatic:
                    incremental_last_scheduled_lane =
                        AnytimeSchedulerLane::ExecutableUpper;
                    break;
                case ActionEnvelopeLane::IncrementalPriority:
                    incremental_last_scheduled_lane =
                        AnytimeSchedulerLane::HighProgress;
                    break;
                case ActionEnvelopeLane::IncrementalOperatorMajor:
                    incremental_last_scheduled_lane =
                        AnytimeSchedulerLane::LegacyFairness;
                    break;
                case ActionEnvelopeLane::IncrementalClosure:
                    incremental_last_scheduled_lane =
                        AnytimeSchedulerLane::ExactClosure;
                    break;
                default:
                    break;
                }
                phase = SolvePhase::Expanding;
            };
        const auto pair_complete = [&](
                const std::uint32_t state,
                const std::uint32_t operator_index) {
            return action_envelope_ledger.scheduler_view_enabled
                ? action_envelope_ledger.scheduling_complete(
                      state, operator_index)
                : incremental_completed_pairs.contains(
                      (static_cast<std::uint64_t>(state) << 32) |
                      operator_index);
        };
        /* Automatic compounds remain carrier-local, but one scheduler service
         * owns at most one preparation or exact row before yielding. */
        const auto service_automatic = [&]() {
          while (incremental_automatic_order_cursor <
                 incremental_automatic_carrier_order.size()) {
            const std::uint32_t state =
                incremental_automatic_carrier_order[
                    incremental_automatic_order_cursor];
            if (!incremental_dynamic_prepared) {
                incremental_resume_epoch_after_dynamic_prepare =
                    continue_current_epoch;
                record_carrier_schedule_attribution(
                    CarrierBoundAttributionWork::ScheduleStage::
                        IncrementalCarrierAdmission,
                    state);
                incremental_dynamic_prepare_active = true;
                phase = SolvePhase::Expanding;
                return true;
            }
            if (incremental_dynamic_operator_cursor <
                incremental_dynamic_operator_indices.size()) {
                const std::uint32_t operator_index =
                    incremental_dynamic_operator_indices[
                        incremental_dynamic_operator_cursor++];
                if (pair_complete(state, operator_index)) {
                    if (incremental_unevaluated_actions != 0) {
                        --incremental_unevaluated_actions;
                    }
                    continue;
                }
                schedule_pair(
                    state, operator_index,
                    ActionEnvelopeLane::IncrementalAutomatic);
                return true;
            }
            ++incremental_automatic_carrier_cursor;
            ++incremental_automatic_order_cursor;
            const std::size_t missing_before =
                incremental_anytime_missing_frontier_states.size();
            incremental_anytime_missing_frontier_states.erase(
                std::remove(
                    incremental_anytime_missing_frontier_states.begin(),
                    incremental_anytime_missing_frontier_states.end(),
                    state),
                incremental_anytime_missing_frontier_states.end());
            if (incremental_anytime_missing_frontier_states.size() <
                missing_before) {
                ++incremental_missing_frontier_service_completions;
            }
            incremental_dynamic_prepared = false;
            incremental_dynamic_prepare_active = false;
            incremental_dynamic_operator_cursor = 0;
            incremental_dynamic_operator_indices.clear();
            if (continue_current_epoch &&
                incremental_automatic_carrier_cursor >=
                    incremental_automatic_epoch_end) {
                return false;
            }
          }
          return false;
        };
        const auto service_fairness = [&]() {
            while (incremental_fairness_operator_cursor <
                   delayed_operator_indices.size()) {
                while (incremental_fairness_carrier_cursor <
                       incremental_fairness_carrier_order.size()) {
                    const std::uint32_t state =
                        incremental_fairness_carrier_order[
                            incremental_fairness_carrier_cursor++];
                    const std::uint32_t operator_index =
                        delayed_operator_indices[
                            incremental_fairness_operator_cursor];
                    if (pair_complete(state, operator_index)) continue;
                    schedule_pair(
                        state, operator_index,
                        ActionEnvelopeLane::IncrementalOperatorMajor);
                    return true;
                }
                incremental_fairness_carrier_cursor = 0;
                ++incremental_fairness_operator_cursor;
            }
            return false;
        };
        const auto service_high_progress = [&]() {
            while (incremental_high_progress_operator_cursor <
                   delayed_operator_indices.size()) {
                while (incremental_high_progress_carrier_cursor <
                       incremental_high_progress_carrier_order.size()) {
                    const std::uint32_t state =
                        incremental_high_progress_carrier_order[
                            incremental_high_progress_carrier_cursor++];
                    incremental_high_progress_operator_order =
                        delayed_operator_indices;
                    if (cooperative_high_progress_ordering_enabled()) {
                        prioritize_carrier_actions(
                            state,
                            incremental_high_progress_operator_order);
                    }
                    if (incremental_high_progress_operator_cursor >=
                        incremental_high_progress_operator_order.size()) {
                        continue;
                    }
                    const std::uint32_t operator_index =
                        incremental_high_progress_operator_order[
                            incremental_high_progress_operator_cursor];
                    if (pair_complete(state, operator_index)) continue;
                    schedule_pair(
                        state, operator_index,
                        ActionEnvelopeLane::IncrementalPriority);
                    return true;
                }
                incremental_high_progress_carrier_cursor = 0;
                ++incremental_high_progress_operator_cursor;
            }
            incremental_high_progress_operator_order.clear();
            return false;
        };
        const auto service_closure = [&]() {
            while (incremental_closure_operator_cursor <
                   delayed_operator_indices.size()) {
                while (incremental_closure_carrier_cursor <
                       incremental_carriers.size()) {
                    const std::uint32_t state = incremental_carriers[
                        incremental_closure_carrier_cursor++];
                    const std::uint32_t operator_index =
                        delayed_operator_indices[
                            incremental_closure_operator_cursor];
                    if (pair_complete(state, operator_index)) continue;
                    schedule_pair(
                        state, operator_index,
                        ActionEnvelopeLane::IncrementalClosure);
                    return true;
                }
                incremental_closure_carrier_cursor = 0;
                ++incremental_closure_operator_cursor;
            }
            return false;
        };

        /* The matched control corpus has no cooperative quota that preserves
         * both clean acquisition and already-progressed last-mile coverage.
         * Keep the proven warm-start producer as the plan's narrow fallback;
         * the typed ledger remains the complete observational lifecycle while
         * the legacy completed-pair view retains scheduling authority. */
        if (!cooperative_high_progress_ordering_enabled()) {
            const WarmStartCompatibilityProfile& profile =
                kWarmStartCompatibilityProfile;
            if (!incremental_alternative_rows.empty() &&
                incremental_carriers.size() < std::min<std::size_t>(
                    options.max_expanded_states,
                    profile.carrier_checkpoint)) {
                return false;
            }
            if (service_automatic()) return true;
            if (continue_current_epoch &&
                incremental_automatic_carrier_cursor >=
                    incremental_automatic_epoch_end) {
                return false;
            }
            while (incremental_priority_task_cursor <
                   incremental_priority_tasks.size()) {
                const IncrementalPriorityTask task =
                    incremental_priority_tasks[
                        incremental_priority_task_cursor++];
                if (pair_complete(task.state, task.operator_index)) continue;
                schedule_pair(
                    task.state, task.operator_index,
                    ActionEnvelopeLane::IncrementalPriority);
                return true;
            }
            if (continue_current_epoch) return false;
            if (!incremental_priority_tasks.empty()) {
                incremental_priority_tasks.clear();
                incremental_priority_task_cursor = 0;
            }
            while (incremental_operator_cursor <
                   delayed_operator_indices.size()) {
                if (incremental_carrier_cursor >=
                    incremental_carriers.size()) {
                    if (incremental_operator_cursor == 0 &&
                        !incremental_warm_start_continuation_refined) {
                        incremental_warm_start_continuation_refined = true;
                        schedule_warm_start_continuation_refinement();
                        if (incremental_carrier_cursor <
                            incremental_carriers.size()) {
                            continue;
                        }
                    }
                    if (incremental_operator_cursor == 0 &&
                        incremental_warm_start_policy_wave <
                            profile.policy_waves &&
                        prepare_warm_start_policy_wave(
                            incremental_warm_start_policy_wave)) {
                        ++incremental_warm_start_policy_wave;
                        const IncrementalPriorityTask task =
                            incremental_priority_tasks[
                                incremental_priority_task_cursor++];
                        schedule_pair(
                            task.state, task.operator_index,
                            ActionEnvelopeLane::IncrementalPriority);
                        return true;
                    }
                    incremental_carrier_cursor = 0;
                    ++incremental_operator_cursor;
                    continue;
                }
                const std::uint32_t state =
                    incremental_carriers[incremental_carrier_cursor++];
                const std::uint32_t operator_index =
                    delayed_operator_indices[incremental_operator_cursor];
                if (pair_complete(state, operator_index)) continue;
                schedule_pair(
                    state, operator_index,
                    ActionEnvelopeLane::IncrementalOperatorMajor);
                return true;
            }
            for (const std::uint32_t operator_index :
                 delayed_operator_indices) {
                for (const std::uint32_t state : incremental_carriers) {
                    if (pair_complete(state, operator_index)) continue;
                    schedule_pair(
                        state, operator_index,
                        ActionEnvelopeLane::IncrementalClosure);
                    return true;
                }
            }
            incremental_unevaluated_actions = 0;
            return false;
        }

        SolveScheduler::Availability available{};
        available[static_cast<std::size_t>(
            AnytimeSchedulerLane::LegacyFairness)] =
            incremental_fairness_operator_cursor <
                delayed_operator_indices.size() &&
            incremental_fairness_carrier_cursor <
                incremental_fairness_carrier_order.size();
        available[static_cast<std::size_t>(
            AnytimeSchedulerLane::ExecutableUpper)] =
            incremental_automatic_order_cursor <
                incremental_automatic_carrier_order.size();
        available[static_cast<std::size_t>(
            AnytimeSchedulerLane::HighProgress)] =
            incremental_high_progress_operator_cursor <
                delayed_operator_indices.size() &&
            incremental_high_progress_carrier_cursor <
                incremental_high_progress_carrier_order.size();
        available[static_cast<std::size_t>(
            AnytimeSchedulerLane::ExactClosure)] =
            !continue_current_epoch &&
            incremental_closure_operator_cursor <
                delayed_operator_indices.size();
        const bool lane_work_remains = std::any_of(
            available.begin(), available.end(),
            [](const bool value) { return value; });
        AnytimeSchedulerLane lane =
            anytime_scheduler.select_ticket(available);
        if (lane == AnytimeSchedulerLane::Count && lane_work_remains) {
            if (schedule_incremental_refinement(true)) return true;
            lane = anytime_scheduler.select(available);
        }
        bool scheduled = false;
        switch (lane) {
        case AnytimeSchedulerLane::LegacyFairness:
            scheduled = service_fairness();
            break;
        case AnytimeSchedulerLane::ExecutableUpper:
            scheduled = service_automatic();
            break;
        case AnytimeSchedulerLane::HighProgress:
            scheduled = service_high_progress();
            break;
        case AnytimeSchedulerLane::ExactClosure:
            scheduled = service_closure();
            break;
        case AnytimeSchedulerLane::ProofDirected:
        case AnytimeSchedulerLane::Count:
            break;
        }
        if (scheduled) return true;
        if (lane != AnytimeSchedulerLane::Count) {
            anytime_scheduler.record_yield(lane);
        }
        if (continue_current_epoch) return false;
        if (lane_work_remains) return false;
        incremental_unevaluated_actions = 0;
        return false;
    }
    while (incremental_carrier_cursor < incremental_carriers.size()) {
        const std::uint32_t state =
            incremental_carriers[incremental_carrier_cursor];
        std::uint32_t operator_index = kNoId;
        ActionEnvelopeLane lane =
            ActionEnvelopeLane::IncrementalCarrierLocal;
        if (!incremental_dynamic_prepared) {
            /*
             * State-local compound candidates are deliberately synthesized
             * after the Chaos-anchored restricted graph has usable values but
             * before delayed action-family rows. Otherwise a bounded product
             * solve can finish a complete focused round without ever making
             * its automatic dependencies reachable. The returned set is
             * anchors plus newly admitted local operators; keep only the
             * latter.
            */
            record_carrier_schedule_attribution(
                CarrierBoundAttributionWork::ScheduleStage::
                    IncrementalCarrierAdmission,
                state);
            incremental_dynamic_prepare_active = true;
            phase = SolvePhase::Expanding;
            return true;
        }
        if (incremental_dynamic_operator_cursor <
            incremental_dynamic_operator_indices.size()) {
            operator_index = incremental_dynamic_operator_indices[
                incremental_dynamic_operator_cursor++];
            lane = ActionEnvelopeLane::IncrementalAutomatic;
        } else if (incremental_operator_cursor <
                   delayed_operator_indices.size()) {
            operator_index =
                delayed_operator_indices[incremental_operator_cursor++];
        } else {
            ++incremental_carrier_cursor;
            incremental_operator_cursor = 0;
            incremental_dynamic_prepared = false;
            incremental_dynamic_prepare_active = false;
            incremental_dynamic_operator_cursor = 0;
            incremental_dynamic_operator_indices.clear();
            continue;
        }
        action_envelope_ledger.queue(
            state, operator_index, lane,
            EnvelopeEvidenceCarrierFacts |
                EnvelopeEvidenceCarrierEffectSummary |
                EnvelopeEvidenceActionRefinementContract);
        if (incremental_unevaluated_actions != 0) {
            --incremental_unevaluated_actions;
        }
        if (retire_unmaterialized_by_operator_proof(
                state, operator_index)) {
            return true;
        }
        expansion_state = state;
        expansion_operator_indices.assign(1, operator_index);
        expansion_operator_cursor = 0;
        expansion_active = true;
        expansion_prepared = true;
        expansion_is_incremental_alternative = true;
        expansion_incremental_resource_limited = false;
        expansion_appended_row =
            std::numeric_limits<std::uint64_t>::max();
        expansion_states_outside_chaos_support = 0;
        record_carrier_schedule_attribution(
            CarrierBoundAttributionWork::ScheduleStage::
                CarrierActionAdmission,
            state, operator_index);
        if (incremental_first_alternative_expanded_states == 0) {
            incremental_first_alternative_expanded_states =
                expanded_count;
        }
        phase = SolvePhase::Expanding;
        return true;
    }
    return false;
}

bool SolveWork::Impl::begin_incremental_upper_policy_pass() {
    if (!options.high_impact_executable_uppers ||
        !incremental_action_generation ||
        incremental_envelope_closed ||
        !incremental_upper_policy_dirty ||
        incremental_upper_policy_pass ||
        !output_incumbent.has_value()) {
        return false;
    }
    ++incremental_upper_policy_passes_requested;
    retain_action_reason(
        "included:high_impact_executable_uppers:policy_pass_requested");
    incremental_upper_policy_pass = true;
    incremental_upper_fixed_policy_proved = false;
    incremental_upper_policy_prior_bound =
        output_incumbent->certified_upper_bound;
    incremental_upper_temporary_rows.clear();
    for (const IncrementalAlternativeRow& candidate :
         incremental_alternative_rows) {
        if (candidate.status ==
                IncrementalAlternativeRow::Status::Admitted ||
            candidate.row_index >= transition_cache->rows.size()) {
            continue;
        }
        SparseRow& row =
            transition_cache->rows[candidate.row_index];
        if (!row.admitted) {
            row.admitted = true;
            incremental_upper_temporary_rows.push_back(
                candidate.row_index);
        }
    }
    if (!begin_focused_upper_solve()) {
        ++incremental_upper_policy_passes_rejected;
        retain_action_reason(
            "rejected:high_impact_executable_uppers:policy_pass_seed");
        incremental_upper_policy_pass = false;
        incremental_upper_fixed_policy_proved = false;
        for (const std::uint64_t row :
             incremental_upper_temporary_rows) {
            transition_cache->rows.at(row).admitted = false;
        }
        incremental_upper_temporary_rows.clear();
        return false;
    }
    ++incremental_upper_policy_passes_started;
    retain_action_reason(
        "included:high_impact_executable_uppers:policy_pass_started");
    incremental_upper_policy_dirty = false;
    phase = SolvePhase::Expanding;
    return true;
}

bool SolveWork::Impl::schedule_warm_start_continuation_refinement() {
    if (!options.high_impact_executable_uppers ||
        incremental_refinement_active) {
        return false;
    }
    const WarmStartCompatibilityProfile& profile =
        kWarmStartCompatibilityProfile;
    const std::size_t state_count = calc.state_count();
    std::vector<double> priority(state_count, 0.0);
    for (std::uint32_t owner = 0; owner < expanded.size(); ++owner) {
        if (!expanded[owner] || calc.is_goal_state(calc.state(owner))) {
            continue;
        }
        for (const std::uint64_t row_index :
             state_row_indices(*transition_cache, owner)) {
            const SparseRow& row = transition_cache->rows.at(row_index);
            if (!row.admitted || row_index >= priced_rows.size()) continue;
            const std::uint32_t operator_index =
                priced_rows[row_index].operator_index;
            if (operator_index >= calc.operators().size()) continue;
            const PlannerOperator& planner =
                calc.operators()[operator_index];
            bool fracture = planner.automatic_kind ==
                AutomaticCandidateKind::Fracture;
            if (!fracture &&
                planner.kind == PlannerOperatorKind::Primitive &&
                planner.primitive_action < calc.registry().actions.size()) {
                fracture = calc.registry()
                    .actions[planner.primitive_action]
                    .params.type == ActionType::Fracture;
            }
            if (!fracture) continue;
            for (std::uint32_t i = 0; i < row.transition_count; ++i) {
                const std::uint64_t offset = row.transition_offset + i;
                const std::uint32_t successor =
                    transition_cache->successors.at(offset);
                if (successor >= state_count ||
                    calc.is_goal_state(calc.state(successor)) ||
                    (successor < expanded.size() && expanded[successor])) {
                    continue;
                }
                const AbstractState& state = calc.state(successor);
                const std::uint32_t fractures =
                    std::popcount(state.fractured_goal_mask);
                const std::uint32_t satisfied = std::popcount(
                    satisfied_goal_mask_for_state(successor));
                priority[successor] +=
                    transition_cache->probabilities.at(offset) *
                    (1.0 +
                     profile.continuation_fracture_weight * fractures +
                     profile.continuation_goal_weight * satisfied);
            }
        }
    }
    std::vector<std::uint32_t> ranked;
    for (std::uint32_t state = 0; state < state_count; ++state) {
        if (priority[state] > 0.0) ranked.push_back(state);
    }
    std::stable_sort(
        ranked.begin(), ranked.end(),
        [&](const std::uint32_t left, const std::uint32_t right) {
            return priority[left] != priority[right]
                ? priority[left] > priority[right]
                : left < right;
        });
    const std::size_t batch = std::min<std::size_t>(
        ranked.size(), profile.continuation_batch);
    if (batch == 0) return false;
    ranked.resize(batch);
    double selected_influence = 0.0;
    for (const std::uint32_t state : ranked) {
        selected_influence += priority[state];
        retain_incremental_carrier(state);
    }
    ++incremental_refinement_rounds;
    incremental_refinement_states_selected += batch;
    incremental_refinement_uncertainty += selected_influence;
    incremental_upper_policy_dirty = true;
    incremental_reclassify_all = true;
    return true;
}

bool SolveWork::Impl::prepare_warm_start_policy_wave(
        const std::uint32_t wave) {
    const WarmStartCompatibilityProfile& profile =
        kWarmStartCompatibilityProfile;
    const bool fracture_wave = wave % 2 == 0;
    const std::uint32_t required_fractures = wave / 2 + 1;
    std::uint32_t target_operator = kNoId;
    if (fracture_wave) {
        for (const PricedOperator& priced : operators) {
            const PlannerOperator& planner =
                calc.operators().at(priced.index);
            if (planner.automatic_kind ==
                    AutomaticCandidateKind::Fracture ||
                (planner.kind == PlannerOperatorKind::Primitive &&
                 planner.primitive_action <
                     calc.registry().actions.size() &&
                 calc.registry()
                     .actions[planner.primitive_action]
                     .params.type == ActionType::Fracture)) {
                target_operator = priced.index;
                break;
            }
        }
    } else if (!delayed_operator_indices.empty()) {
        target_operator = delayed_operator_indices.front();
    }
    if (target_operator == kNoId) return false;

    std::vector<double> priority(calc.state_count(), 0.0);
    for (const IncrementalAlternativeRow& source :
         incremental_alternative_rows) {
        if (source.row_index >= transition_cache->rows.size() ||
            source.operator_index >= calc.operators().size()) {
            continue;
        }
        const AbstractState& owner = calc.state(source.state);
        const std::uint32_t owner_fractures =
            std::popcount(owner.fractured_goal_mask);
        const PlannerOperator& source_planner =
            calc.operators()[source.operator_index];
        const bool source_is_harvest =
            source_planner.kind == PlannerOperatorKind::Primitive &&
            source_planner.primitive_action <
                calc.registry().actions.size() &&
            calc.registry()
                .actions[source_planner.primitive_action]
                .params.type == ActionType::HarvestReforge;
        const bool source_is_fracture =
            source_planner.automatic_kind ==
                AutomaticCandidateKind::Fracture ||
            (source_planner.kind == PlannerOperatorKind::Primitive &&
             source_planner.primitive_action <
                 calc.registry().actions.size() &&
             calc.registry()
                 .actions[source_planner.primitive_action]
                 .params.type == ActionType::Fracture);
        if ((fracture_wave &&
             (!source_is_harvest ||
              owner_fractures < required_fractures)) ||
            (!fracture_wave &&
             (!source_is_fracture ||
              owner_fractures + 1 < required_fractures + 1))) {
            continue;
        }
        const SparseRow& row =
            transition_cache->rows[source.row_index];
        for (std::uint32_t i = 0; i < row.transition_count; ++i) {
            const std::uint64_t offset = row.transition_offset + i;
            const std::uint32_t successor =
                transition_cache->successors.at(offset);
            if (successor >= priority.size() ||
                calc.is_goal_state(calc.state(successor))) {
                continue;
            }
            const AbstractState& state = calc.state(successor);
            const std::uint32_t fractures =
                std::popcount(state.fractured_goal_mask);
            const std::uint32_t satisfied = std::popcount(
                satisfied_goal_mask_for_state(successor));
            if (fracture_wave) {
                if (fractures < required_fractures ||
                    satisfied <= fractures) {
                    continue;
                }
                const PlannerOperator& target =
                    calc.operators()[target_operator];
                if (target.kind != PlannerOperatorKind::Primitive ||
                    target.primitive_action >=
                        calc.registry().actions.size() ||
                    !action_legal(
                        session,
                        calc.registry().actions[target.primitive_action],
                        state)) {
                    continue;
                }
            } else if (fractures < required_fractures + 1) {
                continue;
            }
            priority[successor] +=
                transition_cache->probabilities.at(offset) *
                (1.0 + profile.wave_fracture_weight * fractures +
                 profile.wave_goal_weight * satisfied);
        }
    }
    std::vector<std::uint32_t> ranked;
    for (std::uint32_t state = 0; state < priority.size(); ++state) {
        if (priority[state] > 0.0) ranked.push_back(state);
    }
    std::stable_sort(
        ranked.begin(), ranked.end(),
        [&](const std::uint32_t left, const std::uint32_t right) {
            return priority[left] != priority[right]
                ? priority[left] > priority[right]
                : left < right;
        });
    if (ranked.size() > profile.continuation_batch) {
        ranked.resize(profile.continuation_batch);
    }
    incremental_priority_tasks.clear();
    incremental_priority_task_cursor = 0;
    incremental_priority_tasks.reserve(ranked.size());
    for (const std::uint32_t state : ranked) {
        incremental_priority_tasks.push_back({state, target_operator});
        ++incremental_unevaluated_actions;
    }
    return !incremental_priority_tasks.empty();
}

double SolveWork::Impl::sparse_row_q_for_values(
        const std::size_t row_index,
        const std::vector<double>& values) const {
    /*
     * Carrier-local admission is transactional. A resource refusal can roll
     * staged sparse rows back after an incremental diagnostic recorded their
     * provisional index. Finalization is observational, so a rolled-back row
     * is unresolved/infinite rather than a reason to index past the retained
     * row tables and terminate the solve.
     */
    if (row_index >= transition_cache->rows.size() ||
        row_index >= priced_rows.size()) {
        return kInfinity;
    }
    struct ValueContext {
        const CalcContext* calc = nullptr;
        const std::vector<double>* values = nullptr;
    };
    const ValueContext context{&calc, &values};
    std::uint32_t transition_work = 0;
    return evaluate_sparse_policy_row_with_accessor(
        *transition_cache, priced_rows, row_index, transition_work,
        [](const void* const opaque, const std::uint32_t state) {
            const ValueContext& source =
                *static_cast<const ValueContext*>(opaque);
            if (state >= source.values->size()) {
                return source.calc->is_goal_state(source.calc->state(state))
                    ? 0.0
                    : kInfinity;
            }
            const double value = source.values->at(state);
            return std::isfinite(value) && value < kValueCeiling
                ? value
                : kInfinity;
        },
        &context);
}

bool SolveWork::Impl::maybe_install_incremental_anytime_incumbent() {
    const bool bounded_row_checkpoint =
        incremental_alternative_rows.size() >=
        incremental_anytime_next_row_checkpoint;
    const bool material_upper_improvement =
        cooperative_high_progress_ordering_enabled() &&
        output_incumbent.has_value() &&
        std::isfinite(incremental_anytime_checkpoint_upper) &&
        output_incumbent->certified_upper_bound <
            incremental_anytime_checkpoint_upper *
                (1.0 - anytime_scheduler.profile()
                    .material_upper_improvement_ratio);
    if (!options.high_impact_executable_uppers ||
        !incremental_action_generation || incremental_envelope_closed ||
        incremental_upper_policy_pass == false ||
        (!bounded_row_checkpoint && !material_upper_improvement)) {
        return false;
    }

    const std::size_t completed = incremental_alternative_rows.size();
    ++incremental_anytime_policy_attempts;
    std::array<bool, kAutomaticCandidateKindCount> attempted_kinds{};
    for (const IncrementalAlternativeRow& candidate :
         incremental_alternative_rows) {
        if (candidate.operator_index >= calc.operators().size()) continue;
        const AutomaticCandidateKind kind =
            calc.operators()[candidate.operator_index].automatic_kind;
        const std::size_t kind_index = static_cast<std::size_t>(kind);
        if (kind != AutomaticCandidateKind::None &&
            kind_index < attempted_kinds.size()) {
            attempted_kinds[kind_index] = true;
        }
    }
    for (std::size_t index = 1; index < attempted_kinds.size(); ++index) {
        if (attempted_kinds[index]) {
            ++incremental_joint_policy_attempt_kinds[index];
        }
    }
    incremental_anytime_policy_last_failure.clear();
    incremental_anytime_policy_last_completed_rows = completed;
    /* Geometric checkpoints make the total selection/evaluation work
     * logarithmic in completed rows. This is a scheduling cadence, not a
     * semantic cap: skipped checkpoints neither reject a row nor close the
     * envelope. Saturate instead of wrapping on a pathological ledger. */
    incremental_anytime_next_row_checkpoint =
        completed > std::numeric_limits<std::size_t>::max() / 2
            ? std::numeric_limits<std::size_t>::max()
            : std::max<std::size_t>(completed + 1, completed * 2);

    const double prior = output_incumbent.has_value()
        ? output_incumbent->certified_upper_bound
        : kInfinity;
    if (cooperative_high_progress_ordering_enabled()) {
        incremental_anytime_checkpoint_upper = prior;
    }
    const bool installed = try_install_reachable_incumbent(false);
    if (installed && output_incumbent.has_value() &&
        output_incumbent->certified_upper_bound < prior) {
        ++incremental_anytime_policy_successes;
        std::array<bool, kAutomaticCandidateKindCount> selected_kinds{};
        for (std::size_t state = 0;
             state < output_incumbent->policy.size(); ++state) {
            if (!output_incumbent->policy_reachable.empty() &&
                (state >= output_incumbent->policy_reachable.size() ||
                 output_incumbent->policy_reachable[state] == 0)) {
                continue;
            }
            const std::uint32_t operator_index =
                output_incumbent->policy[state].index;
            if (operator_index >= calc.operators().size()) continue;
            const AutomaticCandidateKind kind =
                calc.operators()[operator_index].automatic_kind;
            const std::size_t kind_index = static_cast<std::size_t>(kind);
            if (kind != AutomaticCandidateKind::None &&
                kind_index < selected_kinds.size()) {
                selected_kinds[kind_index] = true;
            }
        }
        for (std::size_t index = 1; index < selected_kinds.size(); ++index) {
            if (selected_kinds[index]) {
                ++incremental_joint_policy_success_kinds[index];
            }
        }
        incremental_anytime_policy_best_upper = std::min(
            incremental_anytime_policy_best_upper,
            output_incumbent->certified_upper_bound);
        if (cooperative_high_progress_ordering_enabled()) {
            anytime_scheduler.record_improvement(
                incremental_last_scheduled_lane);
        }
    }
    return installed;
}

std::vector<double>
SolveWork::Impl::certified_incremental_lower_values() {
    std::vector<double> lower(calc.state_count(), 0.0);
    const bool has_focused_proof_snapshot =
        focused_lower_completion_proof_values.size() == lower.size();
    const bool full_action_envelope =
        !incremental_action_generation || incremental_envelope_closed;
    for (std::uint32_t state = 0; state < lower.size(); ++state) {
        if (calc.is_goal_state(calc.state(state))) {
            lower[state] = 0.0;
            continue;
        }
        /*
         * Finite values from the admitted-row problem are no more global
         * than infinite ones while delayed actions remain: minimizing over a
         * strict subset of actions can only overestimate the full-envelope
         * optimum. Using those values as candidate lower-Qs can therefore
         * falsely certify a genuinely improving delayed row as
         * NonImproving. Reserve restricted values for scheduling. Delayed
         * row classification uses only the independently admissible state
         * heuristic (or its universal zero fallback) until the envelope is
         * complete.
         */
        if (full_action_envelope && state < result.values.size() &&
            std::isfinite(result.values[state]) &&
            result.values[state] >= 0.0 &&
            result.values[state] < kValueCeiling) {
            lower[state] = result.values[state];
        } else {
            const double retained = has_focused_proof_snapshot
                ? focused_lower_completion_proof_values[state]
                : std::numeric_limits<double>::quiet_NaN();
            lower[state] = std::isfinite(retained)
                ? retained
                : completion_proof_lower(state).value;
        }
    }
    return lower;
}

void SolveWork::Impl::refresh_incremental_upper_incumbent() {
    if (!output_incumbent.has_value()) return;
    if (!retain_current_certified_incumbent()) return;
    BoundedPolicyIncumbent& incumbent = *output_incumbent;
    const std::uint64_t no_row =
        std::numeric_limits<std::uint64_t>::max();
    const std::size_t old_size = incumbent.values.size();
    const std::size_t state_count = calc.state_count();
    if (old_size < state_count) {
        incumbent.values.resize(state_count, kInfinity);
        incumbent.policy_rows.resize(state_count, no_row);
        incumbent.frontier_operators.resize(state_count, kNoId);
        incumbent.policy.resize(state_count);
        incumbent.policy_row_costs.resize(state_count, kInfinity);
        if (!incumbent.policy_reachable.empty()) {
            incumbent.policy_reachable.resize(state_count, 0);
        }
        for (std::uint32_t state =
                 static_cast<std::uint32_t>(old_size);
             state < state_count; ++state) {
            if (calc.is_goal_state(calc.state(state))) {
                incumbent.values[state] = 0.0;
                if (!incumbent.policy_reachable.empty()) {
                    incumbent.policy_reachable[state] = 1;
                }
                continue;
            }
            if (!restart_row_allowed(state) ||
                restart_state >= incumbent.values.size() ||
                !std::isfinite(restart_cost) ||
                !std::isfinite(incumbent.values[restart_state])) {
                continue;
            }
            incumbent.values[state] =
                restart_cost + incumbent.values[restart_state];
            incumbent.frontier_operators[state] =
                restart_operator_index;
            if (!incumbent.policy_reachable.empty()) {
                incumbent.policy_reachable[state] = 1;
            }
            capture_incumbent_state(incumbent, state, no_row);
        }
    }

    std::vector<std::uint8_t> changed(state_count, 0);
    /*
     * The incumbent starts as a complete executable policy. Replacing a
     * state action only when its exact row maps the incumbent value vector
     * below the current state value preserves a Bellman super-solution.
     * Later decreases of successor values can only strengthen that witness.
     * A bounded number of deterministic Gauss-Seidel passes is therefore a
     * certified policy improvement, even when it has not reached the exact
     * value of the improved policy.
     */
    for (std::uint32_t sweep = 0; sweep < 16; ++sweep) {
        bool improved = false;
        for (const IncrementalAlternativeRow& candidate :
             incremental_alternative_rows) {
            if (candidate.status !=
                    IncrementalAlternativeRow::Status::Admitted ||
                candidate.state >= incumbent.values.size()) {
                continue;
            }
            const double q = sparse_row_q_for_values(
                candidate.row_index, incumbent.values);
            const double current = incumbent.values[candidate.state];
            if (!std::isfinite(q) || !(q < current)) {
                continue;
            }
            incumbent.values[candidate.state] = q;
            incumbent.policy_rows[candidate.state] =
                candidate.row_index;
            incumbent.frontier_operators[candidate.state] = kNoId;
            changed[candidate.state] = 1;
            improved = true;
            ++incremental_upper_policy_updates;
        }
        if (!improved) break;
    }
    for (std::uint32_t state = 0; state < changed.size(); ++state) {
        if (changed[state]) {
            capture_incumbent_state(
                incumbent, state, incumbent.policy_rows[state]);
        }
    }
    incumbent.certified_upper_bound =
        incumbent.values.at(result.start_state);
    incumbent.evaluated_policy_cost =
        incumbent.certified_upper_bound;
    incumbent.kind = "q_directed_incremental_policy";
    incumbent.graph_identity = graph_identity();
    incumbent.compiled_artifact = {};
    incumbent.compilation_provenance.clear();
    incumbent.independently_certified = false;
    incumbent.independently_evaluated = false;
    incumbent.proper = false;
    incumbent.executable = false;
    incumbent.primitive_renewal_witness = {};
    incumbent.portfolio_identity = 0;
    result.diagnostics.incumbent_kind = incumbent.kind;
    result.diagnostics.incumbent_graph_identity =
        incumbent.graph_identity;
    result.diagnostics.focused_upper_bound =
        incumbent.certified_upper_bound;
    if (result.start_state < result.values.size()) {
        result.diagnostics.focused_optimality_gap = std::max(
            0.0,
            incumbent.certified_upper_bound -
                result.values[result.start_state]);
    }
}

void SolveWork::Impl::capture_initial_incremental_selected_policy() {
    if (unverified_selected_policy_candidate.has_value() ||
        !incremental_action_generation || incremental_envelope_closed ||
        policy_iteration_failed || !policy_initialized || !policy_stable ||
        !improper_policy_states.empty() ||
        result.start_state >= result.values.size() ||
        result.start_state >= policy_rows.size() ||
        !std::isfinite(result.values[result.start_state]) ||
        result.values[result.start_state] < 0.0) {
        return;
    }
    if (output_incumbent.has_value() &&
        result.values[result.start_state] >=
            output_incumbent->certified_upper_bound -
                value_comparison_tolerance(
                    output_incumbent->certified_upper_bound)) {
        /* A compact independently certifiable incumbent already covers this
         * cost. Capturing the equivalent coarse selected policy would make
         * bounded finalization exact-evaluate a much larger graph before it
         * can publish the same upper. Keep selected-policy capture for a
         * genuinely cheaper candidate only. */
        return;
    }
    const std::uint64_t no_row =
        std::numeric_limits<std::uint64_t>::max();
    if (policy_rows[result.start_state] == no_row) return;

    const std::size_t state_count = result.values.size();
    std::vector<std::uint8_t> reachable(state_count, 0);
    std::vector<std::uint32_t> pending;
    pending.reserve(state_count);
    pending.push_back(result.start_state);
    reachable[result.start_state] = 1;
    bool materializable = true;
    for (std::size_t cursor = 0;
         cursor < pending.size() && materializable; ++cursor) {
        const std::uint32_t state = pending[cursor];
        if (state >= state_count || calc.is_goal_state(calc.state(state))) {
            continue;
        }
        if (state >= policy_rows.size()) {
            materializable = false;
            break;
        }
        const std::uint64_t row_index = policy_rows[state];
        if (row_index == no_row ||
            row_index >= transition_cache->rows.size() ||
            row_index >= priced_rows.size() ||
            !transition_cache->rows[row_index].admitted) {
            materializable = false;
            break;
        }
        const SparseRow& row = transition_cache->rows[row_index];
        const auto add_successor = [&](const std::uint32_t successor) {
            if (successor >= state_count) {
                materializable = false;
            } else if (!reachable[successor]) {
                reachable[successor] = 1;
                pending.push_back(successor);
            }
        };
        for (std::uint32_t i = 0;
             materializable && i < row.transition_count; ++i) {
            const std::uint64_t offset = row.transition_offset + i;
            if (transition_cache->probabilities.at(offset) > 0.0) {
                add_successor(
                    transition_cache->successors.at(offset));
            }
        }
        for (std::uint32_t i = 0;
             materializable && i < row.choice_count; ++i) {
            const SparseChoiceGroup& choice =
                transition_cache->choices.at(row.choice_offset + i);
            if (choice.probability <= 0.0) continue;
            const std::uint32_t successor =
                select_sparse_policy_choice_successor(
                    *transition_cache, choice, state, result.values);
            if (successor == kNoId) {
                materializable = false;
            } else {
                add_successor(successor);
            }
        }
    }
    if (!materializable) return;

    UnverifiedSelectedPolicyCandidate selected;
    selected.has_exact_start_item = result.has_exact_start_item;
    selected.exact_start_item = result.exact_start_item;
    selected.selected_estimate = result.values[result.start_state];
    selected.numerical_stability_stop = numerical_stability_stop;
    BoundedPolicyIncumbent& snapshot = selected.snapshot;
    snapshot.certified_upper_bound = selected.selected_estimate;
    snapshot.evaluated_policy_cost = kInfinity;
    snapshot.values = result.values;
    snapshot.policy_rows = policy_rows;
    snapshot.policy_rows.resize(state_count, no_row);
    snapshot.policy_reachable = std::move(reachable);
    snapshot.behavioral_representative_by_state =
        result.behavioral_representative_by_state;
    snapshot.restart_operator = restart_operator_index;
    snapshot.restart_state = restart_state;
    snapshot.round = result.diagnostics.focused_expansion_rounds;
    snapshot.kind = "initial_selected_coarse_policy";
    snapshot.goal_identity = goal_identity();
    snapshot.economy_identity = economy_identity();
    snapshot.action_vocabulary_identity =
        action_vocabulary_identity();
    snapshot.action_vocabulary_size = operators.size();
    snapshot.caller_scope_identity = caller_scope_identity();
    snapshot.graph_identity = graph_identity();
    snapshot.artifact_identity = artifact_identity();
    snapshot.source_generation = transition_cache->rows.size();
    snapshot.target_generation = calc.state_count();
    snapshot.graph_row_count = transition_cache->rows.size();
    snapshot.graph_priced_row_count = priced_rows.size();
    snapshot.graph_successor_count =
        transition_cache->successors.size();
    snapshot.graph_probability_count =
        transition_cache->probabilities.size();
    snapshot.graph_choice_count = transition_cache->choices.size();
    snapshot.graph_choice_successor_count =
        transition_cache->choice_successors.size();
    snapshot.graph_choice_option_count =
        transition_cache->choice_options.size();
    snapshot.graph_prefix_identity =
        incumbent_graph_prefix_identity(
            snapshot.graph_row_count,
            snapshot.graph_priced_row_count,
            snapshot.graph_successor_count,
            snapshot.graph_probability_count,
            snapshot.graph_choice_count,
            snapshot.graph_choice_successor_count,
            snapshot.graph_choice_option_count);
    snapshot.strict_state_provenance =
        result.behavioral_representative_by_state.empty();
    snapshot.policy_materialized = false;

    std::uint64_t policy_hash = 1469598103934665603ULL;
    const auto mix = [&policy_hash](const std::uint64_t value) {
        policy_hash ^= value;
        policy_hash *= 1099511628211ULL;
    };
    for (std::size_t state = 0;
         state < snapshot.policy_rows.size(); ++state) {
        const std::uint64_t row = snapshot.policy_rows[state];
        mix(state);
        mix(row);
        if (row == no_row) continue;
        mix(priced_rows[row].operator_index);
        mix(std::bit_cast<std::uint64_t>(priced_rows[row].cost));
    }
    selected.selected_policy_hash = policy_hash;
    try {
        capture_incumbent_policy(snapshot);
    } catch (const std::exception&) {
        return;
    }
    std::uint64_t identity = 1469598103934665603ULL;
    identity_mix(identity, selected.selected_policy_hash);
    identity_mix(identity, snapshot.goal_identity);
    identity_mix(identity, snapshot.economy_identity);
    identity_mix(identity, snapshot.action_vocabulary_identity);
    identity_mix(identity, snapshot.caller_scope_identity);
    identity_mix(identity, snapshot.artifact_identity);
    identity_mix(identity, snapshot.graph_prefix_identity);
    identity_mix(identity, snapshot.source_generation);
    identity_mix(identity, snapshot.target_generation);
    identity_mix_string(identity, snapshot.kind);
    selected.capture_identity = identity;
    snapshot.portfolio_identity = identity;
    snapshot.retained_owned_bytes = incumbent_owned_bytes(snapshot);
    const std::uint64_t live = fast_estimated_owned_bytes();
    if (!certified_fallback_fits_memory(
            live, snapshot.retained_owned_bytes,
            options.max_solver_owned_bytes)) {
        return;
    }
    PolicyRefinementTelemetry& telemetry =
        result.diagnostics.policy_refinement;
    telemetry.selected_candidate_capture_attempted = true;
    telemetry.selected_candidate_captured = true;
    telemetry.selected_candidate_estimated_cost =
        selected.selected_estimate;
    telemetry.selected_candidate_owned_bytes =
        snapshot.retained_owned_bytes;
    telemetry.selected_candidate_identity = identity;
    telemetry.selected_candidate_status =
        "initial_restricted_policy_captured";
    unverified_selected_policy_candidate = std::move(selected);
}

bool SolveWork::Impl::continue_open_incremental_envelope() {
    if (!incremental_action_generation || incremental_envelope_closed ||
        requested_bounded_finish || result.diagnostics.resource_cap_hit) {
        return false;
    }

    /* Convergence (including the numerical-stability latch) closes only the
     * current admitted-row policy iteration. It does not evaluate, reject,
     * or close delayed and carrier-local action obligations. Preserve the
     * selected policy as an independently certifiable bounded candidate,
     * then resume the ordinary finite-envelope scheduler. */
    capture_initial_incremental_selected_policy();
    focus_optimizing = false;
    focused_lower_mode = false;
    incremental_restricted_values_ready = true;
    if (begin_incremental_upper_policy_pass()) return true;
    if (classify_incremental_alternatives()) {
        restart_incremental_optimization();
        return true;
    }
    if (options.high_impact_executable_uppers &&
        schedule_next_incremental_alternative()) {
        return true;
    }
    if (schedule_incremental_refinement()) return true;
    if (schedule_next_incremental_alternative()) return true;
    return schedule_incremental_refinement(true);
}

void SolveWork::Impl::begin_incremental_post_upper_scheduling() {
    incremental_post_upper_scheduling_active = true;
    incremental_post_upper_proof_cursor =
        focused_lower_completion_proof_values.size();
    focused_lower_completion_proof_values.resize(
        calc.state_count(), std::numeric_limits<double>::quiet_NaN());
    phase = SolvePhase::Expanding;
}

bool SolveWork::Impl::advance_incremental_post_upper_scheduling() {
    if (!incremental_post_upper_scheduling_active) return true;
    if (focused_lower_completion_proof_values.size() <
        calc.state_count()) {
        focused_lower_completion_proof_values.resize(
            calc.state_count(), std::numeric_limits<double>::quiet_NaN());
    }
    if (incremental_post_upper_proof_cursor <
        focused_lower_completion_proof_values.size()) {
        const std::uint32_t state = static_cast<std::uint32_t>(
            incremental_post_upper_proof_cursor++);
        /* Support interned after the completed focused snapshot receives its
         * proof value one state per host continuation. The shared goal-cover
         * model is already owned by measured solve setup, so this preserves
         * proof strength without reintroducing its former multi-second lazy
         * construction at this scheduling transition. */
        focused_lower_completion_proof_values[state] =
            completion_proof_lower(state).value;
        return false;
    }

    if (!incremental_classification_active) {
        begin_incremental_classification();
    }
    const bool classification_complete =
        advance_incremental_classification();
    if (!classification_complete) return false;
    incremental_post_upper_scheduling_active = false;
    incremental_post_upper_proof_cursor = 0;
    if (incremental_classification_admitted) {
        restart_incremental_optimization();
        return true;
    }
    const bool missing_frontier_refinement =
        !incremental_anytime_missing_frontier_states.empty() &&
        schedule_incremental_refinement();
    if (missing_frontier_refinement) {
        return true;
    }
    if (options.high_impact_executable_uppers &&
        schedule_next_incremental_alternative()) {
        return true;
    }
    if (schedule_incremental_refinement()) {
        return true;
    }
    if (schedule_next_incremental_alternative()) {
        return true;
    }
    if (schedule_incremental_refinement(true)) {
        return true;
    }
    if (incremental_envelope_closed) {
        finish_focused_lower_solve();
    } else {
        phase = SolvePhase::Done;
    }
    return true;
}

bool SolveWork::Impl::schedule_incremental_refinement(
        const bool force) {
    if (!incremental_action_generation ||
        incremental_envelope_closed ||
        incremental_refinement_active) {
        return false;
    }
    const std::size_t completed =
        incremental_alternative_rows.size();
    if (!force && completed < 3) {
        return false;
    }
    const std::vector<double>& lower = result.values;
    const std::vector<double> no_upper;
    const std::vector<double>& upper =
        output_incumbent.has_value()
            ? output_incumbent->values
            : no_upper;
    const std::size_t state_count = calc.state_count();
    std::vector<double> priority(state_count, 0.0);
    const double unbounded_priority =
        std::numeric_limits<double>::max() / 16.0;
    bool has_unresolved = false;

    for (const std::uint32_t state :
         incremental_anytime_missing_frontier_states) {
        if (state < state_count &&
            !calc.is_goal_state(calc.state(state)) &&
            (state >= expanded.size() || !expanded[state])) {
            priority[state] = unbounded_priority;
            has_unresolved = true;
        }
    }

    const auto state_width =
        [&](const std::uint32_t state) {
            if (state >= lower.size() || state >= upper.size() ||
                !std::isfinite(lower[state]) ||
                !std::isfinite(upper[state])) {
                return kInfinity;
            }
            return std::max(0.0, upper[state] - lower[state]);
        };
    for (const IncrementalAlternativeRow& candidate :
         incremental_alternative_rows) {
        if (candidate.status !=
                IncrementalAlternativeRow::Status::Unresolved &&
            !(force && candidate.status ==
                IncrementalAlternativeRow::Status::PendingValues)) {
            continue;
        }
        has_unresolved = true;
        ++incremental_rows_reconsidered;
        const SparseRow& row =
            transition_cache->rows.at(candidate.row_index);
        const double q_width =
            std::isfinite(candidate.lower_q) &&
                    std::isfinite(candidate.upper_q)
                ? std::max(
                      0.0, candidate.upper_q - candidate.lower_q)
                : kInfinity;
        double raw_total = 0.0;
        bool raw_unbounded = false;
        for (std::uint32_t i = 0; i < row.transition_count; ++i) {
            const std::uint64_t offset = row.transition_offset + i;
            const std::uint32_t successor =
                transition_cache->successors.at(offset);
            if (successor == row.owner_state) continue;
            const double width = state_width(successor);
            if (!std::isfinite(width)) {
                raw_unbounded = true;
            } else {
                raw_total +=
                    q_directed_uncertainty_contribution(
                        transition_cache->probabilities.at(offset),
                        0.0, width);
            }
        }
        for (std::uint32_t i = 0; i < row.choice_count; ++i) {
            const SparseChoiceGroup& group =
                transition_cache->choices.at(row.choice_offset + i);
            double best_upper = kInfinity;
            for (std::uint32_t s = 0; s < group.successor_count; ++s) {
                const std::uint32_t successor =
                    transition_cache->choice_successors.at(
                        group.successor_offset + s);
                if (successor < upper.size()) {
                    best_upper = std::min(
                        best_upper, upper[successor]);
                }
            }
            for (std::uint32_t s = 0; s < group.successor_count; ++s) {
                const std::uint32_t successor =
                    transition_cache->choice_successors.at(
                        group.successor_offset + s);
                if (successor >= lower.size() ||
                    lower[successor] >
                        best_upper +
                            value_comparison_tolerance(best_upper)) {
                    continue;
                }
                const double width = state_width(successor);
                if (!std::isfinite(width)) {
                    raw_unbounded = true;
                } else {
                    raw_total +=
                        q_directed_uncertainty_contribution(
                            group.probability, 0.0, width);
                }
            }
        }
        const double scale =
            std::isfinite(q_width) && raw_total > 0.0
                ? q_width / raw_total
                : 1.0;
        const auto add =
            [&](const std::uint32_t successor,
                const double probability) {
                if (successor == row.owner_state ||
                    successor >= state_count ||
                    (successor < expanded.size() &&
                     expanded[successor])) {
                    return;
                }
                const double width = state_width(successor);
                if (!std::isfinite(width) ||
                    !std::isfinite(q_width) || raw_unbounded) {
                    priority[successor] = unbounded_priority;
                    return;
                }
                /*
                 * Exceptional-support actions must not be classified from
                 * an analytic fringe value. Their exact new states enter the
                 * ordinary lifecycle and are expanded before the action can
                 * be admitted or rejected.
                 */
                if (successor >= incremental_chaos_support.size() ||
                    !incremental_chaos_support[successor]) {
                    priority[successor] = unbounded_priority;
                    return;
                }
                priority[successor] +=
                    q_directed_uncertainty_contribution(
                        probability, 0.0, width) *
                    scale;
            };
        for (std::uint32_t i = 0; i < row.transition_count; ++i) {
            const std::uint64_t offset = row.transition_offset + i;
            add(
                transition_cache->successors.at(offset),
                transition_cache->probabilities.at(offset));
        }
        for (std::uint32_t i = 0; i < row.choice_count; ++i) {
            const SparseChoiceGroup& group =
                transition_cache->choices.at(row.choice_offset + i);
            double best_upper = kInfinity;
            for (std::uint32_t s = 0; s < group.successor_count; ++s) {
                const std::uint32_t successor =
                    transition_cache->choice_successors.at(
                        group.successor_offset + s);
                if (successor < upper.size()) {
                    best_upper = std::min(
                        best_upper, upper[successor]);
                }
            }
            for (std::uint32_t s = 0; s < group.successor_count; ++s) {
                const std::uint32_t successor =
                    transition_cache->choice_successors.at(
                        group.successor_offset + s);
                if (successor < lower.size() &&
                    lower[successor] <=
                        best_upper +
                            value_comparison_tolerance(best_upper)) {
                    add(successor, group.probability);
                }
            }
        }
    }
    if (!has_unresolved) return false;

    std::vector<std::uint32_t> ranked;
    ranked.reserve(state_count - std::min<std::size_t>(
        state_count, expanded.size()));
    for (std::uint32_t state = 0; state < state_count; ++state) {
        if (priority[state] > 0.0 &&
            (state >= expanded.size() || !expanded[state])) {
            ranked.push_back(state);
        }
    }
    /*
     * A nonlinear choice or an unavailable executable incumbent can leave
     * no scalar attribution even though the exact envelope overlaps. In
     * that case continue toward strict closure in bounded batches. This is
     * the completeness fallback after Q attribution has no candidate, not
     * the normal broad-fringe policy.
     */
    if (ranked.empty()) {
        for (std::uint32_t state = 0; state < state_count; ++state) {
            if (!calc.is_goal_state(calc.state(state)) &&
                (state >= expanded.size() || !expanded[state])) {
                priority[state] = 1.0;
                ranked.push_back(state);
            }
        }
    }
    std::stable_sort(
        ranked.begin(), ranked.end(),
        [&](const std::uint32_t left, const std::uint32_t right) {
            return priority[left] != priority[right]
                       ? priority[left] > priority[right]
                       : left < right;
        });
    const std::uint32_t remaining_capacity =
        options.max_expanded_states > expanded_count
            ? options.max_expanded_states - expanded_count
            : 0;
    const std::uint32_t refinement_batch =
        options.high_impact_executable_uppers
            ? anytime_scheduler.profile().q_refinement_batch
            : std::max<std::uint32_t>(
                  1024, options.focused_expansion_batch_states);
    const std::size_t batch = std::min<std::size_t>(
        ranked.size(),
        std::min<std::uint32_t>(
            remaining_capacity,
            refinement_batch));
    if (batch == 0) return false;
    ranked.resize(batch);

    std::vector<std::uint8_t> selected(state_count, 0);
    double selected_uncertainty = 0.0;
    for (const std::uint32_t state : ranked) {
        selected[state] = 1;
        selected_uncertainty = std::min(
            unbounded_priority,
            selected_uncertainty + priority[state]);
    }
    std::deque<std::uint32_t> remainder;
    for (const std::uint32_t state : queue) {
        if (state >= selected.size() || !selected[state]) {
            remainder.push_back(state);
        }
    }
    queue = std::move(remainder);
    for (auto it = ranked.rbegin(); it != ranked.rend(); ++it) {
        queue.push_front(*it);
        if (*it >= queued.size()) queued.resize(*it + 1, 0);
        queued[*it] = 1;
    }
    peak_queue_size = std::max<std::uint32_t>(
        peak_queue_size, static_cast<std::uint32_t>(queue.size()));
    incremental_refinement_active = true;
    incremental_refinement_target_expanded =
        expanded_count + static_cast<std::uint32_t>(batch);
    ++incremental_refinement_rounds;
    incremental_refinement_states_selected += batch;
    incremental_refinement_uncertainty = std::min(
        unbounded_priority,
        incremental_refinement_uncertainty +
            selected_uncertainty);
    phase = SolvePhase::Expanding;
    return true;
}

void SolveWork::Impl::begin_incremental_classification() {
    if (incremental_classification_active) {
        throw std::logic_error("incremental classification already active");
    }
    incremental_classification_certified_lower =
        certified_incremental_lower_values();
    incremental_classification_restricted_graph_closed = true;
    for (std::uint32_t state = 0; state < calc.state_count(); ++state) {
        if (!calc.is_goal_state(calc.state(state)) &&
            (state >= expanded.size() || !expanded[state])) {
            incremental_classification_restricted_graph_closed = false;
            break;
        }
    }
    const bool exact_restricted_values =
        (incremental_classification_restricted_graph_closed &&
         incremental_restricted_values_ready &&
         optimization_converged()) ||
        (optimization_converged() &&
         (focused_bound_proved || !focused_mode));
    if (exact_restricted_values) {
        incremental_classification_upper =
            IncrementalClassificationUpper::ResultValues;
        /* Only a stable, exactly evaluated proper selected policy owns a
         * carrier upper for later automatic grammar pruning. A residual-only
         * Bellman fallback or an improper policy must not populate this
         * authority. Adding actions later cannot invalidate the feasibility
         * of this restricted policy, so the snapshot remains a valid upper. */
        if (!policy_iteration_failed && policy_initialized && policy_stable &&
            improper_policy_states.empty()) {
            if (incremental_certified_upper_values.capacity() <
                result.values.size()) {
                const std::uint64_t added =
                    static_cast<std::uint64_t>(
                        result.values.size() -
                        incremental_certified_upper_values.capacity()) *
                    sizeof(double);
                if (check_solver_byte_cap_fast(added)) {
                    throw SolverResourceLimit(
                        "max_solver_owned_bytes",
                        options.max_solver_owned_bytes);
                }
                incremental_certified_upper_values.reserve(
                    result.values.size());
            }
            incremental_certified_upper_values = result.values;
            retire_certified_unmaterialized_obligations();
        }
    } else if (output_incumbent.has_value()) {
        incremental_classification_upper =
            IncrementalClassificationUpper::OutputIncumbent;
    } else {
        incremental_classification_upper =
            IncrementalClassificationUpper::None;
    }

    incremental_classification_reclassify_all =
        !options.high_impact_executable_uppers ||
        incremental_reclassify_all;
    incremental_reclassify_all = false;
    incremental_classification_admitted = false;
    incremental_classification_cursor = 0;
    incremental_classification_active = true;
}

bool SolveWork::Impl::advance_incremental_classification() {
    if (!incremental_classification_active) return true;
    const std::vector<double>* upper_values = nullptr;
    if (incremental_classification_upper ==
        IncrementalClassificationUpper::ResultValues) {
        upper_values = &result.values;
    } else if (incremental_classification_upper ==
                   IncrementalClassificationUpper::OutputIncumbent &&
               output_incumbent.has_value()) {
        upper_values = &output_incumbent->values;
    }

    if (incremental_classification_cursor <
        incremental_alternative_rows.size()) {
        IncrementalAlternativeRow& candidate =
            incremental_alternative_rows[
                incremental_classification_cursor++];
        if (candidate.status ==
            IncrementalAlternativeRow::Status::Admitted) {
            return false;
        }
        if (!incremental_classification_reclassify_all &&
            candidate.status ==
                IncrementalAlternativeRow::Status::Unresolved) {
            return false;
        }
        candidate.status =
            IncrementalAlternativeRow::Status::PendingValues;
        ++incremental_rows_reconsidered;
        const ActionEnvelopeEntry* ledger_entry =
            action_envelope_ledger.find(
                candidate.state, candidate.operator_index);
        const ActionEnvelopeLane ledger_lane =
            ledger_entry == nullptr
                ? ActionEnvelopeLane::IncrementalCarrierLocal
                : ledger_entry->lane;
        const std::uint32_t ledger_evidence =
            EnvelopeEvidenceCarrierFacts |
            EnvelopeEvidenceCarrierEffectSummary |
            EnvelopeEvidenceActionRefinementContract |
            EnvelopeEvidenceExactRegistryLegality |
            EnvelopeEvidenceExactOptionKernel;
        candidate.lower_q = sparse_row_q_for_values(
            candidate.row_index,
            incremental_classification_certified_lower);
        const SparseRow& row =
            transition_cache->rows.at(candidate.row_index);
        bool has_terminal_exit = false;
        bool terminal_or_self_only = true;
        for (std::uint32_t i = 0; i < row.transition_count; ++i) {
            const std::uint32_t successor =
                transition_cache->successors.at(
                    row.transition_offset + i);
            if (successor == row.owner_state) continue;
            const bool terminal =
                calc.is_goal_state(calc.state(successor));
            has_terminal_exit = has_terminal_exit || terminal;
            terminal_or_self_only = terminal_or_self_only && terminal;
        }
        for (std::uint32_t i = 0; i < row.choice_count; ++i) {
            const SparseChoiceGroup& group =
                transition_cache->choices.at(row.choice_offset + i);
            for (std::uint32_t s = 0; s < group.successor_count; ++s) {
                const std::uint32_t successor =
                    transition_cache->choice_successors.at(
                        group.successor_offset + s);
                const bool terminal =
                    calc.is_goal_state(calc.state(successor));
                has_terminal_exit = has_terminal_exit || terminal;
                terminal_or_self_only = terminal_or_self_only && terminal;
            }
        }
        /*
         * A fully materialized row whose only exits are goal terminals has an
         * exact Q even when the restricted anchor graph has no executable
         * incumbent. This is the common shape of a guaranteed Essence goal:
         * withholding its finite upper until an unrelated Chaos fringe closes
         * leaves the only executable policy stranded in the delayed envelope.
         * Self probability is already solved exactly by
         * sparse_row_q_for_values(), so publishing this bound changes neither
         * the row nor the Bellman comparison.
         */
        candidate.upper_q =
            terminal_or_self_only && has_terminal_exit
                ? candidate.lower_q
                : (upper_values == nullptr
                       ? kInfinity
                       : sparse_row_q_for_values(
                             candidate.row_index, *upper_values));
        const auto unexpanded_delta =
            [&](const std::uint32_t successor) {
                return !calc.is_goal_state(calc.state(successor)) &&
                       (successor >= incremental_chaos_support.size() ||
                        !incremental_chaos_support[successor]) &&
                       (successor >= expanded.size() ||
                        !expanded[successor]);
            };
        bool pending_delta = false;
        for (std::uint32_t i = 0;
             !pending_delta && i < row.transition_count; ++i) {
            pending_delta = unexpanded_delta(
                transition_cache->successors.at(
                    row.transition_offset + i));
        }
        for (std::uint32_t i = 0;
             !pending_delta && i < row.choice_count; ++i) {
            const SparseChoiceGroup& group =
                transition_cache->choices.at(row.choice_offset + i);
            for (std::uint32_t s = 0;
                 !pending_delta && s < group.successor_count; ++s) {
                pending_delta = unexpanded_delta(
                    transition_cache->choice_successors.at(
                        group.successor_offset + s));
            }
        }
        if (pending_delta) {
            candidate.status =
                IncrementalAlternativeRow::Status::Unresolved;
            action_envelope_ledger.unresolved(
                candidate.state, candidate.operator_index,
                ActionEnvelopeStopOwner::SuccessorFrontier,
                candidate.row_index,
                "unexpanded_successor_frontier", ledger_evidence);
            return false;
        }
        const double current_upper =
            upper_values != nullptr &&
                    candidate.state < upper_values->size()
                ? upper_values->at(candidate.state)
                : kInfinity;
        if (std::isfinite(candidate.upper_q) &&
            candidate.upper_q < current_upper) {
            transition_cache->rows.at(candidate.row_index).admitted = true;
            candidate.status =
                IncrementalAlternativeRow::Status::Admitted;
            candidate.improvement_margin =
                current_upper - candidate.upper_q;
            action_envelope_ledger.exact_row_complete(
                candidate.state, candidate.operator_index,
                ledger_lane, candidate.row_index, ledger_evidence);
            incremental_classification_admitted = true;
            return false;
        }
        if (std::isfinite(current_upper) &&
            current_upper < kValueCeiling &&
            candidate.lower_q >= current_upper) {
            candidate.status =
                IncrementalAlternativeRow::Status::NonImproving;
            candidate.improvement_margin =
                current_upper - candidate.lower_q;
            action_envelope_ledger.incumbent_dominated(
                candidate.state, candidate.operator_index,
                candidate.row_index, ledger_evidence);
        } else {
            candidate.status =
                IncrementalAlternativeRow::Status::Unresolved;
            action_envelope_ledger.unresolved(
                candidate.state, candidate.operator_index,
                ActionEnvelopeStopOwner::MissingVerifiedUpper,
                candidate.row_index,
                "missing_verified_carrier_upper", ledger_evidence);
        }
        return false;
    }

    /* Rows proved improving against the same incumbent are independent
     * admissions, not prescribed policy choices. Expose the whole batch and
     * let one exact Bellman re-optimization choose among them. */
    if (incremental_classification_admitted) {
        incremental_classification_active = false;
        incremental_classification_cursor = 0;
        incremental_classification_certified_lower.clear();
        return true;
    }

    if (incremental_unevaluated_actions == 0) {
        if (incremental_resource_unresolved_actions != 0) {
            /* All work outside a previously refused automatic family is now
             * complete. Publish the retained executable policy as bounded;
             * the family cap stays explicit and the action envelope cannot
             * earn exact closure. */
            record_cap(
                incremental_deferred_resource_cap.empty()
                    ? "automatic_action_envelope"
                    : incremental_deferred_resource_cap);
            incremental_envelope_closed = false;
            incremental_classification_active = false;
            incremental_classification_cursor = 0;
            incremental_classification_certified_lower.clear();
            return true;
        }
        const bool automatic_preparation_closed =
            !options.high_impact_executable_uppers ||
            (incremental_automatic_carrier_cursor >=
                 incremental_carriers.size() &&
             !incremental_dynamic_prepare_active &&
             !incremental_dynamic_prepared);
        const auto unadmitted = std::find_if(
            incremental_alternative_rows.begin(),
            incremental_alternative_rows.end(),
            [](const IncrementalAlternativeRow& candidate) {
                return candidate.status !=
                       IncrementalAlternativeRow::Status::Admitted;
            });
        /*
         * Once every non-goal state and every filtered action are complete,
         * there is no fringe estimate left to refine. Overlap can then mean
         * that several rows form a proper/improving policy only together.
         * Earlier NonImproving classifications compare a row with a carrier
         * upper used for scheduling; until that upper has survived compiled
         * exact evaluation it cannot remove the row from a global closure
         * claim. Admit every remaining fully materialized legal alternative
         * to the exact closed Bellman problem together and let the following
         * solve decide among them. Admission does not prescribe a row. One
         * batch avoids repeating the same exact SCC proof once per row. This
         * closure rule cannot run on the large incomplete Chaos fringe and is
         * not the removed broad-graph overlap fallback.
         */
        if (unadmitted != incremental_alternative_rows.end() &&
            incremental_classification_restricted_graph_closed &&
            incremental_restricted_values_ready &&
            automatic_preparation_closed &&
            !result.diagnostics.resource_cap_hit) {
            for (IncrementalAlternativeRow& candidate :
                 incremental_alternative_rows) {
                if (candidate.status ==
                    IncrementalAlternativeRow::Status::Admitted) {
                    continue;
                }
                transition_cache->rows.at(
                    candidate.row_index).admitted = true;
                candidate.status =
                    IncrementalAlternativeRow::Status::Admitted;
                candidate.improvement_margin = 0.0;
                const ActionEnvelopeEntry* ledger_entry =
                    action_envelope_ledger.find(
                        candidate.state, candidate.operator_index);
                const ActionEnvelopeLane ledger_lane =
                    ledger_entry == nullptr
                        ? ActionEnvelopeLane::IncrementalCarrierLocal
                        : ledger_entry->lane;
                const std::uint32_t ledger_evidence =
                    (ledger_entry == nullptr
                         ? EnvelopeEvidenceNone
                         : ledger_entry->evidence) |
                    EnvelopeEvidenceCarrierFacts |
                    EnvelopeEvidenceCarrierEffectSummary |
                    EnvelopeEvidenceActionRefinementContract |
                    EnvelopeEvidenceExactRegistryLegality |
                    EnvelopeEvidenceExactOptionKernel;
                /* Joint final admission closes the exact materialized row,
                 * not merely its scheduler status. Keep the typed lifecycle
                 * synchronized so a closed envelope cannot retain a stale
                 * MissingVerifiedUpper obligation in diagnostics or in a
                 * later proof consumer. */
                action_envelope_ledger.exact_row_complete(
                    candidate.state, candidate.operator_index,
                    ledger_lane, candidate.row_index, ledger_evidence);
            }
            /* The next restart must be the ordinary closed-envelope lower
             * optimization, not another one-proof upper-witness pass. If
             * closure were delayed until the following classification, that
             * upper pass would restore the stale restricted lower snapshot
             * and the solver could publish ExactClosed without ever running
             * Howard selection over this final joint row set. */
            incremental_envelope_closed = true;
            incremental_classification_admitted = true;
            incremental_classification_active = false;
            incremental_classification_cursor = 0;
            incremental_classification_certified_lower.clear();
            return true;
        }
        if (unadmitted == incremental_alternative_rows.end() &&
            incremental_unevaluated_actions == 0 &&
            automatic_preparation_closed) {
            incremental_envelope_closed = true;
        }
    }
    incremental_classification_active = false;
    incremental_classification_cursor = 0;
    incremental_classification_certified_lower.clear();
    return true;
}

bool SolveWork::Impl::classify_incremental_alternatives() {
    begin_incremental_classification();
    while (!advance_incremental_classification()) {
    }
    return incremental_classification_admitted;
}

void SolveWork::Impl::restart_incremental_optimization() {
    ++incremental_reoptimizations;
    if (options.high_impact_executable_uppers) {
        /*
         * The experimental path publishes an upper only after the shared
         * fixed-policy evaluator has proved the whole selected policy
         * proper. The legacy local super-solution refresh remains unchanged
         * for default solves.
         */
        incremental_upper_policy_dirty = true;
        incremental_reclassify_all = true;
    } else {
        refresh_incremental_upper_incumbent();
    }
    focused_bound_proved = false;
    focused_closure_proved = false;
    policy_initialized = false;
    policy_stable = false;
    policy_iteration_failed = false;
    backup_active = false;
    reset_policy_iteration_units();
    residual = kValueCeiling;
    if (focused_mode) {
        phase = SolvePhase::Expanding;
        begin_focused_lower_solve();
    } else {
        phase = SolvePhase::Iterating;
    }
}

namespace {

const char* action_envelope_state_name(const ActionEnvelopeState state) {
    switch (state) {
    case ActionEnvelopeState::Queued: return "queued";
    case ActionEnvelopeState::ExactRowComplete: return "exact_row_complete";
    case ActionEnvelopeState::ExactInapplicabilityProved:
        return "exact_inapplicability_proved";
    case ActionEnvelopeState::IncumbentDominated:
        return "incumbent_dominated";
    case ActionEnvelopeState::RolledBackAfterCap:
        return "rolled_back_after_named_cap";
    case ActionEnvelopeState::OmittedCallerScope:
        return "omitted_explicit_caller_scope";
    case ActionEnvelopeState::UnresolvedNamedStop:
        return "unresolved_named_stop";
    case ActionEnvelopeState::Count: return "invalid";
    }
    return "invalid";
}

const char* action_envelope_lane_name(const ActionEnvelopeLane lane) {
    switch (lane) {
    case ActionEnvelopeLane::Unassigned: return "unassigned";
    case ActionEnvelopeLane::RestrictedAnchor:
        return "restricted_anchor";
    case ActionEnvelopeLane::IncrementalCarrierLocal:
        return "incremental_carrier_local";
    case ActionEnvelopeLane::IncrementalAutomatic:
        return "incremental_automatic";
    case ActionEnvelopeLane::IncrementalPriority:
        return "incremental_priority";
    case ActionEnvelopeLane::IncrementalOperatorMajor:
        return "incremental_operator_major";
    case ActionEnvelopeLane::IncrementalClosure:
        return "incremental_closure";
    case ActionEnvelopeLane::ExplicitCallerScope:
        return "explicit_caller_scope";
    case ActionEnvelopeLane::Count: return "invalid";
    }
    return "invalid";
}

const char* action_envelope_authority_name(
        const ActionEnvelopeProofAuthority authority) {
    switch (authority) {
    case ActionEnvelopeProofAuthority::None: return "none";
    case ActionEnvelopeProofAuthority::ExactRowMaterialization:
        return "exact_row_materialization";
    case ActionEnvelopeProofAuthority::ExactRegistryLegality:
        return "exact_registry_legality";
    case ActionEnvelopeProofAuthority::IndependentGlobalLowerVsVerifiedUpper:
        return "independent_global_lower_vs_verified_upper";
    case ActionEnvelopeProofAuthority::ExplicitCallerScope:
        return "explicit_caller_scope";
    case ActionEnvelopeProofAuthority::TransactionalResourceCap:
        return "transactional_resource_cap";
    case ActionEnvelopeProofAuthority::NamedOpenObligation:
        return "named_open_obligation";
    case ActionEnvelopeProofAuthority::Count: return "invalid";
    }
    return "invalid";
}

const char* action_envelope_stop_name(
        const ActionEnvelopeStopOwner owner) {
    switch (owner) {
    case ActionEnvelopeStopOwner::None: return "none";
    case ActionEnvelopeStopOwner::ResourceCap: return "resource_cap";
    case ActionEnvelopeStopOwner::SuccessorFrontier:
        return "successor_frontier";
    case ActionEnvelopeStopOwner::MissingVerifiedUpper:
        return "missing_verified_upper";
    case ActionEnvelopeStopOwner::RequestedBoundedFinish:
        return "requested_bounded_finish";
    case ActionEnvelopeStopOwner::Count: return "invalid";
    }
    return "invalid";
}

} // namespace

void SolveWork::Impl::refresh_action_envelope_ledger_diagnostics(
        SolveDiagnostics& diagnostics) const {
    using Ledger = ActionEnvelopeLedger;
    struct ActionLifecycleSummary {
        bool registered = false;
        bool scheduled = false;
        bool exact_row_complete = false;
        bool exact_registry_legality = false;
        bool exact_option_kernel = false;
    };
    std::array<std::uint64_t, Ledger::kLaneCount> lane_counts{};
    std::array<std::uint64_t, Ledger::kAuthorityCount> authority_counts{};
    std::array<std::uint64_t, Ledger::kStopOwnerCount> stop_counts{};
    std::array<std::uint64_t, 6> evidence_counts{};
    std::vector<ActionLifecycleSummary> action_lifecycles(
        calc.operators().size());
    for (const auto& [unused_key, entry] :
         action_envelope_ledger.entries()) {
        (void)unused_key;
        ++lane_counts.at(static_cast<std::size_t>(entry.lane));
        ++authority_counts.at(static_cast<std::size_t>(entry.authority));
        ++stop_counts.at(static_cast<std::size_t>(entry.stop_owner));
        for (std::size_t bit = 0; bit < evidence_counts.size(); ++bit) {
            if ((entry.evidence & (std::uint32_t{1} << bit)) != 0) {
                ++evidence_counts[bit];
            }
        }
        if (entry.operator_index < action_lifecycles.size()) {
            ActionLifecycleSummary& lifecycle =
                action_lifecycles[entry.operator_index];
            lifecycle.registered = true;
            lifecycle.scheduled = lifecycle.scheduled ||
                (entry.lane != ActionEnvelopeLane::Unassigned &&
                 entry.lane != ActionEnvelopeLane::ExplicitCallerScope);
            lifecycle.exact_row_complete =
                lifecycle.exact_row_complete ||
                entry.lifecycle == ActionEnvelopeState::ExactRowComplete ||
                entry.row_index !=
                    std::numeric_limits<std::uint64_t>::max();
            lifecycle.exact_registry_legality =
                lifecycle.exact_registry_legality ||
                (entry.evidence &
                 EnvelopeEvidenceExactRegistryLegality) != 0;
            lifecycle.exact_option_kernel =
                lifecycle.exact_option_kernel ||
                (entry.evidence & EnvelopeEvidenceExactOptionKernel) != 0;
        }
    }
    const auto state_counts = action_envelope_ledger.state_counts();
    const std::size_t sample_limit = options.max_diagnostic_samples;
    std::vector<const ActionEnvelopeEntry*> samples;
    samples.reserve(std::min<std::size_t>(
        action_envelope_ledger.entries().size(), sample_limit));
    const auto less_identity = [](
            const ActionEnvelopeEntry* left,
            const ActionEnvelopeEntry* right) {
        return std::tie(left->state, left->operator_index) <
               std::tie(right->state, right->operator_index);
    };
    for (const auto& [unused_key, entry] :
         action_envelope_ledger.entries()) {
        (void)unused_key;
        if (sample_limit == 0) break;
        const auto position = std::lower_bound(
            samples.begin(), samples.end(), &entry, less_identity);
        samples.insert(position, &entry);
        if (samples.size() > sample_limit) samples.pop_back();
    }

    std::string json = "{\"schema\":\"action_envelope_ledger_v1\"";
    json += ",\"authoritative_obligation_owner\":true";
    json += ",\"scheduler_view_enabled\":" + std::string(
        action_envelope_ledger.scheduler_view_enabled ? "true" : "false");
    json += ",\"transitions\":" +
            std::to_string(action_envelope_ledger.transition_count());
    json += ",\"entries\":" +
            std::to_string(action_envelope_ledger.entries().size());
    json += ",\"observational_owned_bytes\":" +
            std::to_string(
                action_envelope_ledger.estimated_owned_bytes());
    const auto append_named_counts = [&json](
            const auto& counts, const auto name) {
        json.push_back('{');
        for (std::size_t index = 0; index < counts.size(); ++index) {
            if (index != 0) json.push_back(',');
            append_json_string(json, name(index));
            json.push_back(':');
            json += std::to_string(counts[index]);
        }
        json.push_back('}');
    };
    json += ",\"states\":";
    append_named_counts(
        state_counts,
        [](const std::size_t value) {
            return action_envelope_state_name(
                static_cast<ActionEnvelopeState>(value));
        });
    json += ",\"lanes\":";
    append_named_counts(
        lane_counts,
        [](const std::size_t value) {
            return action_envelope_lane_name(
                static_cast<ActionEnvelopeLane>(value));
        });
    json += ",\"proof_authorities\":";
    append_named_counts(
        authority_counts,
        [](const std::size_t value) {
            return action_envelope_authority_name(
                static_cast<ActionEnvelopeProofAuthority>(value));
        });
    json += ",\"stop_owners\":";
    append_named_counts(
        stop_counts,
        [](const std::size_t value) {
            return action_envelope_stop_name(
                static_cast<ActionEnvelopeStopOwner>(value));
        });
    constexpr std::array<const char*, 6> evidence_names = {
        "carrier_facts", "carrier_effect_summary",
        "carrier_successor_envelope", "action_refinement_contract",
        "exact_registry_legality", "exact_option_kernel"};
    json += ",\"evidence_coverage\":";
    append_named_counts(
        evidence_counts,
        [&](const std::size_t value) { return evidence_names[value]; });
    json += ",\"descriptor_proof\":{\"observational_only\":false";
    json += ",\"authority\":\"complete_immediate_plus_action_successor_pattern_vs_certified_state_upper\"";
    json += ",\"pre_materialization_only\":true";
    json += ",\"evaluations\":" +
            std::to_string(descriptor_proof_evaluations);
    json += ",\"strict_separations\":" +
            std::to_string(descriptor_proof_separations) + "}";
    json += ",\"action_lifecycles\":[";
    bool first_lifecycle = true;
    for (std::size_t operator_index = 0;
         operator_index < action_lifecycles.size(); ++operator_index) {
        const ActionLifecycleSummary& lifecycle =
            action_lifecycles[operator_index];
        if (!lifecycle.registered) continue;
        if (!first_lifecycle) json.push_back(',');
        first_lifecycle = false;
        json += "{\"action_id\":";
        append_json_string(json, calc.operators()[operator_index].id);
        json += ",\"registered\":true";
        json += ",\"scheduled\":" + std::string(
            lifecycle.scheduled ? "true" : "false");
        json += ",\"exact_row_complete\":" + std::string(
            lifecycle.exact_row_complete ? "true" : "false");
        json += ",\"exact_registry_legality\":" + std::string(
            lifecycle.exact_registry_legality ? "true" : "false");
        json += ",\"exact_option_kernel\":" + std::string(
            lifecycle.exact_option_kernel ? "true" : "false");
        json.push_back('}');
    }
    json += "]";
    json += ",\"samples\":[";
    for (std::size_t index = 0; index < samples.size(); ++index) {
        if (index != 0) json.push_back(',');
        const ActionEnvelopeEntry& entry = *samples[index];
        json += "{\"state\":";
        if (entry.state == kNoId) {
            json += "null";
        } else {
            json += std::to_string(entry.state);
        }
        json += ",\"operator_index\":";
        if (entry.operator_index == kNoId) {
            json += "null";
        } else {
            json += std::to_string(entry.operator_index);
        }
        json += ",\"action_id\":";
        if (entry.operator_index < calc.operators().size()) {
            append_json_string(
                json, calc.operators()[entry.operator_index].id);
        } else {
            append_json_string(json, "automatic_preparation");
        }
        json += ",\"lifecycle\":";
        append_json_string(
            json, action_envelope_state_name(entry.lifecycle));
        json += ",\"lane\":";
        append_json_string(json, action_envelope_lane_name(entry.lane));
        json += ",\"proof_authority\":";
        append_json_string(
            json, action_envelope_authority_name(entry.authority));
        json += ",\"stop_owner\":";
        append_json_string(
            json, action_envelope_stop_name(entry.stop_owner));
        json += ",\"row_index\":";
        if (entry.row_index ==
            std::numeric_limits<std::uint64_t>::max()) {
            json += "null";
        } else {
            json += std::to_string(entry.row_index);
        }
        json += ",\"revision\":" + std::to_string(entry.revision);
        json += ",\"detail\":";
        if (entry.detail.empty()) {
            json += "null";
        } else {
            append_json_string(json, entry.detail);
        }
        json += ",\"evidence\":{";
        for (std::size_t bit = 0; bit < evidence_names.size(); ++bit) {
            if (bit != 0) json.push_back(',');
            append_json_string(json, evidence_names[bit]);
            json.push_back(':');
            json += (entry.evidence & (std::uint32_t{1} << bit)) != 0
                        ? "true"
                        : "false";
        }
        json += "}}";
    }
    json += "],\"sample_counts\":{\"retained\":" +
            std::to_string(samples.size());
    json += ",\"omitted\":" + std::to_string(
        action_envelope_ledger.entries().size() - samples.size());
    json += ",\"limit\":" + std::to_string(sample_limit) + "}}";
    diagnostics.action_envelope_ledger_json = std::move(json);
}

void SolveWork::Impl::refresh_anytime_scheduler_diagnostics(
        SolveDiagnostics& diagnostics) const {
    const AnytimeSchedulingProfile& profile = anytime_scheduler.profile();
    std::string json = "{\"schema\":\"solver_anytime_scheduler_v1\"";
    json += ",\"enabled\":" + std::string(
        cooperative_high_progress_ordering_enabled() ? "true" : "false");
    json += ",\"fallback_reason\":\"matched_control_profile_unqualified\"";
    json += ",\"profile_id\":";
    append_json_string(json, std::string(profile.id));
    json += ",\"dispatches\":" +
            std::to_string(anytime_scheduler.dispatches());
    json += ",\"profile\":{";
    json += "\"q_refinement_batch\":" +
            std::to_string(profile.q_refinement_batch);
    json += ",\"first_incumbent_checkpoint_rows\":" +
            std::to_string(profile.first_incumbent_checkpoint_rows);
    json += ",\"starvation_dispatches\":" +
            std::to_string(profile.starvation_dispatches);
    json += ",\"material_upper_improvement_ratio\":" +
            std::to_string(profile.material_upper_improvement_ratio) + "}";
    const WarmStartCompatibilityProfile& warm =
        kWarmStartCompatibilityProfile;
    json += ",\"warm_start_compatibility\":{";
    json += "\"enabled\":" + std::string(
        options.high_impact_executable_uppers &&
                !cooperative_high_progress_ordering_enabled()
            ? "true"
            : "false");
    json += ",\"profile_id\":";
    append_json_string(json, std::string(warm.id));
    json += ",\"carrier_checkpoint\":" +
            std::to_string(warm.carrier_checkpoint);
    json += ",\"continuation_batch\":" +
            std::to_string(warm.continuation_batch);
    json += ",\"policy_waves\":" +
            std::to_string(warm.policy_waves);
    json += ",\"continuation_fracture_weight\":" +
            std::to_string(warm.continuation_fracture_weight);
    json += ",\"continuation_goal_weight\":" +
            std::to_string(warm.continuation_goal_weight);
    json += ",\"wave_fracture_weight\":" +
            std::to_string(warm.wave_fracture_weight);
    json += ",\"wave_goal_weight\":" +
            std::to_string(warm.wave_goal_weight) + "}";
    json += ",\"lanes\":{";
    const auto& lanes = anytime_scheduler.lanes();
    for (std::size_t index = 0; index < lanes.size(); ++index) {
        if (index != 0) json.push_back(',');
        append_json_string(
            json,
            std::string(anytime_scheduler_lane_name(
                static_cast<AnytimeSchedulerLane>(index))));
        const AnytimeLaneTelemetry& lane = lanes[index];
        json += ":{\"quota\":" + std::to_string(lane.quota);
        json += ",\"offers\":" + std::to_string(lane.offers);
        json += ",\"service\":" + std::to_string(lane.services);
        json += ",\"wait\":" + std::to_string(lane.waits);
        json += ",\"yield\":" + std::to_string(lane.yields);
        json += ",\"improvement\":" +
                std::to_string(lane.improvements);
        json += ",\"starvation\":" +
                std::to_string(lane.starvation_events);
        json += ",\"current_wait\":" +
                std::to_string(lane.current_wait);
        json += ",\"max_wait\":" +
                std::to_string(lane.maximum_wait) + "}";
    }
    json += "}";
    const AnytimeSchedulingProfile& focused_profile =
        focused_anytime_scheduler.profile();
    json += ",\"focused_context\":{\"profile_id\":";
    append_json_string(json, std::string(focused_profile.id));
    json += ",\"dispatches\":" +
            std::to_string(focused_anytime_scheduler.dispatches());
    json += ",\"lanes\":{";
    const auto& focused_lanes = focused_anytime_scheduler.lanes();
    for (std::size_t index = 0; index < focused_lanes.size(); ++index) {
        if (index != 0) json.push_back(',');
        append_json_string(
            json,
            std::string(anytime_scheduler_lane_name(
                static_cast<AnytimeSchedulerLane>(index))));
        const AnytimeLaneTelemetry& lane = focused_lanes[index];
        json += ":{\"quota\":" + std::to_string(lane.quota);
        json += ",\"offers\":" + std::to_string(lane.offers);
        json += ",\"service\":" + std::to_string(lane.services);
        json += ",\"wait\":" + std::to_string(lane.waits);
        json += ",\"yield\":" + std::to_string(lane.yields);
        json += ",\"improvement\":" +
                std::to_string(lane.improvements);
        json += ",\"starvation\":" +
                std::to_string(lane.starvation_events);
        json += ",\"max_wait\":" +
                std::to_string(lane.maximum_wait) + "}";
    }
    json += "}}}";
    diagnostics.anytime_scheduler_json = std::move(json);
}

void SolveWork::Impl::refresh_operator_lineage_diagnostics(
        SolveDiagnostics& diagnostics,
        const SolveResult* const published) const {
    struct Lifecycle {
        std::uint64_t scheduled = 0;
        std::uint64_t begun = 0;
        std::uint64_t completed = 0;
        std::uint64_t interrupted = 0;
        std::uint64_t retained_rows = 0;
        std::uint64_t retained_bytes = 0;
        std::uint64_t policy_consumptions = 0;
    };
    struct FamilyTotals : Lifecycle {
        std::uint64_t registry_actions = 0;
        std::uint64_t primitive_candidates = 0;
        std::uint64_t dependency_actions = 0;
        std::uint64_t primitive_dependency_uses = 0;
        std::uint64_t generated_planner_operators = 0;
        std::uint64_t priced_supported_operators = 0;
        std::uint64_t pre_canonical_candidate_variants = 0;
        std::uint64_t canonical_effect_template_classes = 0;
        std::uint64_t collapsed_variants = 0;
        std::uint64_t carrier_local_checks = 0;
        std::uint64_t carrier_local_admissions = 0;
        std::uint64_t synthesis_ns = 0;
        std::uint64_t raw_outcomes = 0;
        std::uint64_t retained_transitions = 0;
        std::uint64_t joint_policy_attempt_participations = 0;
        std::uint64_t joint_policy_success_participations = 0;
    };

    const auto phase_owner_name = [](const SolvePhaseOwner owner) {
        switch (owner) {
        case SolvePhaseOwner::Setup: return "setup";
        case SolvePhaseOwner::PlannerConstruction:
            return "planner_construction";
        case SolvePhaseOwner::TemporaryEffectPrecompile:
            return "temporary_effect_precompile";
        case SolvePhaseOwner::DependencyPreparation:
            return "dependency_preparation";
        case SolvePhaseOwner::PrimitiveRows: return "primitive_rows";
        case SolvePhaseOwner::StateLocalAutomaticSynthesis:
            return "state_local_automatic_synthesis";
        case SolvePhaseOwner::LadderScheduling: return "ladder_scheduling";
        case SolvePhaseOwner::BellmanOptimization:
            return "bellman_optimization";
        case SolvePhaseOwner::PolicyAssembly: return "policy_assembly";
        case SolvePhaseOwner::Compilation: return "compilation";
        case SolvePhaseOwner::ExactEvaluation: return "exact_evaluation";
        case SolvePhaseOwner::Done: return "done";
        }
        return "unknown";
    };
    const auto fixed_option_kind_name = [](const FixedOptionKind kind) {
        switch (kind) {
        case FixedOptionKind::ScourAlchemy: return "scour_alchemy";
        case FixedOptionKind::EldritchSideIntent:
            return "eldritch_side_intent";
        case FixedOptionKind::ProtectedSide: return "protected_side";
        case FixedOptionKind::MultimodFinish: return "multimod_finish";
        case FixedOptionKind::Renewal: return "renewal";
        case FixedOptionKind::ProtectedRepeat: return "protected_repeat";
        case FixedOptionKind::FracturePrepare: return "fracture_prepare";
        case FixedOptionKind::ImprintRetry: return "imprint_retry";
        case FixedOptionKind::TemporaryBenchRepeat:
            return "temporary_bench_repeat";
        }
        return "unknown";
    };
    const auto mix_hash = [](std::uint64_t& hash, const std::uint64_t word) {
        hash ^= word;
        hash *= 1099511628211ull;
    };
    const auto hash_text = [&](const std::uint64_t value) {
        char buffer[17];
        std::snprintf(
            buffer, sizeof(buffer), "%016llx",
            static_cast<unsigned long long>(value));
        return std::string(buffer);
    };

    const std::vector<PlannerOperator>& planners = calc.operators();
    const ActionRegistry& registry = calc.registry();
    const auto lineage_family_for_action = [](
            const ActionDescriptor& action)
            -> std::optional<SolverActionFamily> {
        try {
            return solver_action_family_for_action(action);
        } catch (const std::logic_error&) {
            /* Native white-box tests may inject deliberately synthetic action
             * types that are not part of the product family vocabulary.
             * Observational lineage must disclose and skip those entries,
             * never turn them into solve authority or a solve failure. */
            return std::nullopt;
        }
    };
    std::vector<Lifecycle> lifecycle(planners.size());
    std::vector<bool> priced_supported(planners.size(), false);
    for (const PricedOperator& priced : operators) {
        if (priced.index < priced_supported.size()) {
            priced_supported[priced.index] = true;
        }
    }
    for (const auto& [identity, entry] : action_envelope_ledger.entries()) {
        (void)identity;
        if (entry.operator_index >= lifecycle.size()) continue;
        Lifecycle& values = lifecycle[entry.operator_index];
        if (entry.lane != ActionEnvelopeLane::Unassigned &&
            entry.lane != ActionEnvelopeLane::ExplicitCallerScope) {
            ++values.scheduled;
        }
        if (entry.lifecycle != ActionEnvelopeState::Queued &&
            entry.lifecycle != ActionEnvelopeState::OmittedCallerScope) {
            ++values.begun;
        }
        if (entry.lifecycle == ActionEnvelopeState::ExactRowComplete ||
            entry.lifecycle ==
                ActionEnvelopeState::ExactInapplicabilityProved ||
            entry.lifecycle == ActionEnvelopeState::IncumbentDominated) {
            ++values.completed;
        }
        if (entry.lifecycle == ActionEnvelopeState::RolledBackAfterCap) {
            ++values.interrupted;
        }
    }
    for (const PricedSparseRow& row : priced_rows) {
        if (row.operator_index < lifecycle.size()) {
            ++lifecycle[row.operator_index].retained_rows;
        }
    }
    for (std::size_t index = 0; index < planners.size(); ++index) {
        const auto found =
            diagnostics.action_search_costs.find(planners[index].id);
        if (found == diagnostics.action_search_costs.end()) continue;
        lifecycle[index].retained_bytes = found->second.retained_bytes;
        lifecycle[index].interrupted = std::max(
            lifecycle[index].interrupted, found->second.interrupted_rows);
    }

    const auto count_policy = [&](const auto& policy,
                                  const auto& reachable) {
        for (std::size_t state = 0; state < policy.size(); ++state) {
            if (!reachable.empty() &&
                (state >= reachable.size() || reachable[state] == 0)) {
                continue;
            }
            const std::uint32_t operator_index = policy[state].index;
            if (operator_index < lifecycle.size()) {
                ++lifecycle[operator_index].policy_consumptions;
            }
        }
    };
    if (published != nullptr && published->policy_available) {
        count_policy(published->policy, published->policy_reachable);
    } else if (output_incumbent.has_value()) {
        count_policy(
            output_incumbent->policy, output_incumbent->policy_reachable);
    }

    std::vector<std::vector<std::uint32_t>> dependencies(planners.size());
    std::vector<SolverActionFamilyMask> family_masks(planners.size(), 0);
    for (std::size_t index = 0; index < planners.size(); ++index) {
        const PlannerOperator& planner = planners[index];
        std::vector<std::uint32_t>& action_dependencies = dependencies[index];
        const auto add_dependency = [&](const std::uint32_t action) {
            if (action != kNoId && action < registry.actions.size()) {
                action_dependencies.push_back(action);
            }
        };
        add_dependency(planner.primitive_action);
        for (const std::uint32_t action : planner.primitive_program) {
            add_dependency(action);
        }
        add_dependency(planner.conditional_action);
        add_dependency(planner.bestiary_create_action);
        add_dependency(planner.bestiary_restore_action);
        add_dependency(planner.setup_action);
        add_dependency(planner.followup_action);
        add_dependency(planner.cleanup_action);
        std::sort(action_dependencies.begin(), action_dependencies.end());
        action_dependencies.erase(
            std::unique(
                action_dependencies.begin(), action_dependencies.end()),
            action_dependencies.end());
        for (const std::uint32_t action : action_dependencies) {
            const std::optional<SolverActionFamily> family =
                lineage_family_for_action(registry.actions[action]);
            if (family.has_value()) {
                family_masks[index] |= solver_action_family_bit(*family);
            }
        }
        if (planner.automatic_kind != AutomaticCandidateKind::None) {
            const SolverActionFamily family =
                solver_action_family_for_automatic_candidate(
                    planner.automatic_kind);
            if (family != SolverActionFamily::Count) {
                family_masks[index] |= solver_action_family_bit(family);
            }
        }
    }

    std::array<FamilyTotals, kSolverActionFamilyCount> family_totals{};
    std::uint64_t unclassified_registry_actions = 0;
    for (const ActionDescriptor& action : registry.actions) {
        const std::optional<SolverActionFamily> family =
            lineage_family_for_action(action);
        if (!family.has_value()) {
            ++unclassified_registry_actions;
            continue;
        }
        FamilyTotals& values =
            family_totals[static_cast<std::size_t>(*family)];
        ++values.registry_actions;
        if (action.product_role == ProductActionRole::Candidate) {
            ++values.primitive_candidates;
        } else if (action.product_role ==
                   ProductActionRole::AutomaticDependency) {
            ++values.dependency_actions;
        }
    }
    for (std::size_t index = 0; index < planners.size(); ++index) {
        if (planners[index].kind != PlannerOperatorKind::FixedOption) {
            continue;
        }
        for (std::size_t family_index = 0;
             family_index < family_totals.size(); ++family_index) {
            const SolverActionFamily family =
                static_cast<SolverActionFamily>(family_index);
            if ((family_masks[index] & solver_action_family_bit(family)) == 0) {
                continue;
            }
            FamilyTotals& values = family_totals[family_index];
            ++values.generated_planner_operators;
            values.primitive_dependency_uses += dependencies[index].size();
            values.priced_supported_operators += priced_supported[index];
            values.scheduled += lifecycle[index].scheduled;
            values.begun += lifecycle[index].begun;
            values.completed += lifecycle[index].completed;
            values.interrupted += lifecycle[index].interrupted;
            values.retained_rows += lifecycle[index].retained_rows;
            values.retained_bytes += lifecycle[index].retained_bytes;
            values.policy_consumptions +=
                lifecycle[index].policy_consumptions;
        }
    }
    std::array<SolverActionFamilyMask, kAutomaticCandidateKindCount>
        automatic_family_masks{};
    for (std::size_t index = 0; index < planners.size(); ++index) {
        const std::size_t kind_index =
            static_cast<std::size_t>(planners[index].automatic_kind);
        if (kind_index > 0 && kind_index < automatic_family_masks.size()) {
            automatic_family_masks[kind_index] |= family_masks[index];
        }
    }
    for (std::size_t kind_index = 1;
         kind_index < automatic_family_masks.size(); ++kind_index) {
        const AutomaticTelemetryKind telemetry_kind =
            automatic_telemetry_kind_for_candidate(
                static_cast<AutomaticCandidateKind>(kind_index));
        const AutomaticKindTelemetry& telemetry =
            diagnostics.automatic_kind_telemetry[
                static_cast<std::size_t>(telemetry_kind)];
        for (std::size_t family_index = 0;
             family_index < family_totals.size(); ++family_index) {
            const SolverActionFamily family =
                static_cast<SolverActionFamily>(family_index);
            if ((automatic_family_masks[kind_index] &
                 solver_action_family_bit(family)) == 0) {
                continue;
            }
            FamilyTotals& values = family_totals[family_index];
            values.pre_canonical_candidate_variants +=
                telemetry.candidate_variants;
            values.canonical_effect_template_classes +=
                telemetry.effect_classes;
            values.collapsed_variants += telemetry.collapsed_variants;
            values.carrier_local_checks += telemetry.candidates;
            values.carrier_local_admissions +=
                telemetry.eligible_candidates;
            values.synthesis_ns += telemetry.enumeration_ns +
                                   telemetry.admission_ns +
                                   telemetry.kernel_evaluation_ns;
            values.raw_outcomes += telemetry.raw_outcomes;
            values.retained_transitions += telemetry.retained_transitions;
            values.retained_bytes += telemetry.selected_bytes;
            values.joint_policy_attempt_participations +=
                incremental_joint_policy_attempt_kinds[kind_index];
            values.joint_policy_success_participations +=
                incremental_joint_policy_success_kinds[kind_index];
        }
    }

    std::uint64_t complete_hash = 1469598103934665603ull;
    std::uint64_t generated_count = 0;
    for (std::size_t index = 0; index < planners.size(); ++index) {
        const PlannerOperator& planner = planners[index];
        if (planner.kind != PlannerOperatorKind::FixedOption) continue;
        ++generated_count;
        mix_hash(complete_hash, index);
        const std::vector<std::uint64_t> key =
            planner_operator_semantic_key(planner);
        mix_hash(complete_hash, key.size());
        for (const std::uint64_t word : key) mix_hash(complete_hash, word);
    }

    const auto append_lifecycle = [](std::string& json,
                                     const Lifecycle& values) {
        json += "{\"scheduled\":" + std::to_string(values.scheduled);
        json += ",\"begun\":" + std::to_string(values.begun);
        json += ",\"completed\":" + std::to_string(values.completed);
        json += ",\"interrupted\":" +
                std::to_string(values.interrupted);
        json += ",\"retained_rows\":" +
                std::to_string(values.retained_rows);
        json += ",\"retained_bytes\":" +
                std::to_string(values.retained_bytes);
        json += ",\"selected_policy_consumptions\":" +
                std::to_string(values.policy_consumptions) + "}";
    };

    std::string json = "{\"schema\":\"solver_operator_lineage_v1\"";
    json += ",\"observational\":true";
    json += ",\"authority\":\"existing_registry_planner_ledger_scheduler_rows_policy\"";
    json += ",\"counting_contract\":{\"family_relations_are_non_disjoint\":true,\"operator_samples_are_bounded\":true}";
    json += ",\"complete_generated_operator_count\":" +
            std::to_string(generated_count);
    json += ",\"complete_generated_operator_semantics_fnv1a64\":\"" +
            hash_text(complete_hash) + "\"";
    json += ",\"phase_owners\":{\"current\":";
    append_json_string(json, phase_owner_name(current_phase_owner()));
    json += ",\"setup\":{\"duration_ns\":" +
            std::to_string(diagnostics.solve_setup_ns) + "}";
    json += ",\"planner_construction\":{\"registry_actions\":" +
            std::to_string(registry.actions.size()) +
            ",\"unclassified_registry_actions\":" +
            std::to_string(unclassified_registry_actions) +
            ",\"planner_operators\":" + std::to_string(planners.size()) +
            "}";
    std::uint64_t precompiled_classes = 0;
    std::uint64_t precompile_ns = 0;
    std::uint64_t precompiled_bytes = 0;
    for (const AutomaticKindTelemetry& values :
         diagnostics.automatic_kind_telemetry) {
        precompiled_classes += values.precompiled_classes;
        precompile_ns += values.precompile_ns;
        precompiled_bytes += values.precompiled_bytes;
    }
    json += ",\"temporary_effect_precompile\":{\"classes\":" +
            std::to_string(precompiled_classes) + ",\"duration_ns\":" +
            std::to_string(precompile_ns) + ",\"retained_bytes\":" +
            std::to_string(precompiled_bytes) + "}";
    std::uint64_t dependency_count = 0;
    for (std::size_t index = 0; index < planners.size(); ++index) {
        if (planners[index].kind == PlannerOperatorKind::FixedOption) {
            dependency_count += dependencies[index].size();
        }
    }
    json += ",\"dependency_preparation\":{\"generated_dependencies\":" +
            std::to_string(dependency_count) + "}";
    std::uint64_t primitive_rows = 0;
    std::uint64_t generated_rows = 0;
    std::uint64_t automatic_rows = 0;
    for (std::size_t index = 0; index < planners.size(); ++index) {
        if (planners[index].kind == PlannerOperatorKind::Primitive) {
            primitive_rows += lifecycle[index].retained_rows;
        } else {
            generated_rows += lifecycle[index].retained_rows;
            if (planners[index].automatic_kind !=
                AutomaticCandidateKind::None) {
                automatic_rows += lifecycle[index].retained_rows;
            }
        }
    }
    json += ",\"primitive_rows\":{\"retained_rows\":" +
            std::to_string(primitive_rows) + "}";
    json += ",\"generated_fixed_option_rows\":{\"retained_rows\":" +
            std::to_string(generated_rows) + "}";
    json += ",\"state_local_automatic_synthesis\":{\"carriers\":" +
            std::to_string(
                diagnostics.automatic_admission_phases.carriers) +
            ",\"duration_ns\":" + std::to_string(
                diagnostics.automatic_admission_phases.synthesis_ns) +
            ",\"retained_rows\":" + std::to_string(automatic_rows) +
            "}";
    json += ",\"ladder_scheduling\":{\"epochs\":" +
            std::to_string(diagnostics.incremental_carrier_ladder_epochs) +
            ",\"candidates\":" + std::to_string(
                diagnostics.incremental_carrier_ladder_candidates) +
            ",\"goal_subsets\":" + std::to_string(
                diagnostics.incremental_carrier_ladder_goal_subsets) + "}";
    json += ",\"bellman_optimization\":{\"work_units\":" +
            std::to_string(diagnostics.bellman_work_units) +
            ",\"duration_ns\":" +
            std::to_string(diagnostics.optimization_ns) + "}";
    json += ",\"policy_assembly\":{\"joint_attempts\":" +
            std::to_string(diagnostics.incremental_anytime_policy_attempts) +
            ",\"joint_successes\":" + std::to_string(
                diagnostics.incremental_anytime_policy_successes) +
            ",\"finalization_work_items\":" +
            std::to_string(finalization_work_items) + "}";
    json += ",\"compilation\":{\"active\":" + std::string(
        current_phase_owner() == SolvePhaseOwner::Compilation
            ? "true" : "false") + "}";
    json += ",\"exact_evaluation\":{\"active\":" + std::string(
        current_phase_owner() == SolvePhaseOwner::ExactEvaluation
            ? "true" : "false") +
            ",\"discovered_pairs\":" + std::to_string(
                finalization_evaluation_progress.discovered_pairs) +
            ",\"pending_pairs\":" + std::to_string(
                finalization_evaluation_progress.pending_pairs) +
            ",\"solved_sccs\":" + std::to_string(
                finalization_evaluation_progress.solved_sccs) + "}}";

    json += ",\"missing_frontier\":{\"discovered\":" +
            std::to_string(
                diagnostics.incremental_missing_frontier_discovered);
    json += ",\"priority_offers\":" + std::to_string(
        diagnostics.incremental_missing_frontier_priority_offers);
    json += ",\"service_completions\":" + std::to_string(
        diagnostics.incremental_missing_frontier_service_completions);
    json += ",\"max_open\":" + std::to_string(
        diagnostics.incremental_missing_frontier_max_open);
    json += ",\"open\":" + std::to_string(
        diagnostics.incremental_missing_frontier_open) + "}";

    json += ",\"by_solver_family\":{";
    for (std::size_t index = 0; index < family_totals.size(); ++index) {
        if (index != 0) json.push_back(',');
        append_json_string(
            json,
            std::string(solver_action_family_name(
                static_cast<SolverActionFamily>(index))));
        const FamilyTotals& values = family_totals[index];
        json += ":{\"registry_actions\":" +
                std::to_string(values.registry_actions);
        json += ",\"primitive_candidates\":" +
                std::to_string(values.primitive_candidates);
        json += ",\"dependency_actions\":" +
                std::to_string(values.dependency_actions);
        json += ",\"primitive_dependency_uses\":" +
                std::to_string(values.primitive_dependency_uses);
        json += ",\"generated_planner_operators\":" +
                std::to_string(values.generated_planner_operators);
        json += ",\"priced_supported_operators\":" +
                std::to_string(values.priced_supported_operators);
        json += ",\"pre_canonical_candidate_variants\":" +
                std::to_string(values.pre_canonical_candidate_variants);
        json += ",\"canonical_effect_template_classes\":" +
                std::to_string(values.canonical_effect_template_classes);
        json += ",\"collapsed_variants\":" +
                std::to_string(values.collapsed_variants);
        json += ",\"carrier_local_checks\":" +
                std::to_string(values.carrier_local_checks);
        json += ",\"carrier_local_admissions\":" +
                std::to_string(values.carrier_local_admissions);
        json += ",\"synthesis\":{\"duration_ns\":" +
                std::to_string(values.synthesis_ns);
        json += ",\"raw_outcomes\":" +
                std::to_string(values.raw_outcomes);
        json += ",\"retained_transitions\":" +
                std::to_string(values.retained_transitions) + "}";
        json += ",\"pair_scheduler\":{\"offers\":" +
                std::to_string(values.scheduled);
        json += ",\"services\":" + std::to_string(values.begun);
        json += ",\"waits\":" + std::to_string(
            values.scheduled > values.begun
                ? values.scheduled - values.begun : 0) + "}";
        json += ",\"global_carrier_epochs\":" + std::to_string(
            diagnostics.incremental_carrier_ladder_epochs);
        json += ",\"joint_policy_attempt_participations\":" +
                std::to_string(
                    values.joint_policy_attempt_participations);
        json += ",\"joint_policy_success_participations\":" +
                std::to_string(
                    values.joint_policy_success_participations);
        json += ",\"lifecycle\":";
        append_lifecycle(json, values);
        json += "}";
    }
    json += "}";

    json += ",\"by_automatic_kind\":{";
    for (std::size_t kind_index = 1;
         kind_index < kAutomaticCandidateKindCount; ++kind_index) {
        if (kind_index != 1) json.push_back(',');
        const AutomaticCandidateKind kind =
            static_cast<AutomaticCandidateKind>(kind_index);
        const AutomaticFamilyContract& contract =
            kAutomaticFamilyContracts[kind_index];
        append_json_string(json, std::string(contract.candidate_identity));
        const AutomaticTelemetryKind telemetry_kind =
            automatic_telemetry_kind_for_candidate(kind);
        const AutomaticKindTelemetry& telemetry =
            diagnostics.automatic_kind_telemetry[
                static_cast<std::size_t>(telemetry_kind)];
        Lifecycle values;
        std::uint64_t planner_count = 0;
        std::uint64_t primitive_dependencies = 0;
        std::uint64_t priced_count = 0;
        for (std::size_t index = 0; index < planners.size(); ++index) {
            if (planners[index].automatic_kind != kind) continue;
            ++planner_count;
            primitive_dependencies += dependencies[index].size();
            priced_count += priced_supported[index];
            values.scheduled += lifecycle[index].scheduled;
            values.begun += lifecycle[index].begun;
            values.completed += lifecycle[index].completed;
            values.interrupted += lifecycle[index].interrupted;
            values.retained_rows += lifecycle[index].retained_rows;
            values.retained_bytes += lifecycle[index].retained_bytes;
            values.policy_consumptions +=
                lifecycle[index].policy_consumptions;
        }
        values.retained_bytes += telemetry.selected_bytes;
        json += ":{\"pre_canonical_candidate_variants\":" +
                std::to_string(telemetry.candidate_variants);
        json += ",\"canonical_effect_template_classes\":" +
                std::to_string(telemetry.effect_classes);
        json += ",\"collapsed_variants\":" +
                std::to_string(telemetry.collapsed_variants);
        json += ",\"planner_operators\":" +
                std::to_string(planner_count);
        json += ",\"primitive_dependencies\":" +
                std::to_string(primitive_dependencies);
        json += ",\"priced_supported_operators\":" +
                std::to_string(priced_count);
        json += ",\"carrier_local_checks\":" +
                std::to_string(telemetry.candidates);
        json += ",\"carrier_local_admissions\":" +
                std::to_string(telemetry.eligible_candidates);
        json += ",\"synthesis\":{\"enumeration_ns\":" +
                std::to_string(telemetry.enumeration_ns);
        json += ",\"admission_ns\":" +
                std::to_string(telemetry.admission_ns);
        json += ",\"kernel_evaluation_ns\":" +
                std::to_string(telemetry.kernel_evaluation_ns);
        json += ",\"raw_outcomes\":" +
                std::to_string(telemetry.raw_outcomes);
        json += ",\"transitions\":" +
                std::to_string(telemetry.retained_transitions) + "}";
        json += ",\"pair_scheduler\":{\"offers\":" +
                std::to_string(values.scheduled);
        json += ",\"services\":" + std::to_string(values.begun);
        json += ",\"waits\":" + std::to_string(
            values.scheduled > values.begun
                ? values.scheduled - values.begun : 0);
        json += ",\"rows_completed\":" +
                std::to_string(values.completed);
        json += ",\"interrupted\":" +
                std::to_string(values.interrupted) + "}";
        json += ",\"global_carrier_epochs\":" + std::to_string(
            diagnostics.incremental_carrier_ladder_epochs);
        json += ",\"joint_policy_attempt_participations\":" +
                std::to_string(
                    incremental_joint_policy_attempt_kinds[kind_index]);
        json += ",\"joint_policy_success_participations\":" +
                std::to_string(
                    incremental_joint_policy_success_kinds[kind_index]);
        json += ",\"lifecycle\":";
        append_lifecycle(json, values);
        json += "}";
    }
    json += "}";

    json += ",\"generated_operator_samples\":[";
    std::uint64_t sample_count = 0;
    for (std::size_t index = 0;
         index < planners.size() &&
         sample_count < options.max_diagnostic_samples; ++index) {
        const PlannerOperator& planner = planners[index];
        if (planner.kind != PlannerOperatorKind::FixedOption) continue;
        if (sample_count++ != 0) json.push_back(',');
        std::uint64_t semantic_hash = 1469598103934665603ull;
        const std::vector<std::uint64_t> key =
            planner_operator_semantic_key(planner);
        for (const std::uint64_t word : key) mix_hash(semantic_hash, word);
        json += "{\"operator_index\":" + std::to_string(index);
        json += ",\"planner_id\":";
        append_json_string(json, planner.id);
        json += ",\"planner_kind\":\"fixed_option\"";
        json += ",\"fixed_option_kind\":";
        append_json_string(json, fixed_option_kind_name(planner.option_kind));
        json += ",\"automatic_kind\":";
        append_json_string(
            json,
            std::string(kAutomaticFamilyContracts[
                static_cast<std::size_t>(planner.automatic_kind)]
                            .candidate_identity));
        json += ",\"semantic_fnv1a64\":\"" +
                hash_text(semantic_hash) + "\"";
        json += ",\"priced_supported\":" +
                std::string(priced_supported[index] ? "true" : "false");
        json += ",\"state_local_automatic\":" + std::string(
            calc.is_state_local_automatic_operator(index)
                ? "true" : "false");
        json += ",\"dependencies\":[";
        for (std::size_t dependency = 0;
             dependency < dependencies[index].size(); ++dependency) {
            if (dependency != 0) json.push_back(',');
            append_json_string(
                json, registry.actions[dependencies[index][dependency]].id);
        }
        json += "],\"families\":[";
        bool first_family = true;
        for (std::size_t family_index = 0;
             family_index < kSolverActionFamilyCount; ++family_index) {
            const SolverActionFamily family =
                static_cast<SolverActionFamily>(family_index);
            if ((family_masks[index] & solver_action_family_bit(family)) == 0) {
                continue;
            }
            if (!first_family) json.push_back(',');
            first_family = false;
            append_json_string(
                json, std::string(solver_action_family_name(family)));
        }
        json += "],\"lifecycle\":";
        append_lifecycle(json, lifecycle[index]);
        json += "}";
    }
    json += "],\"sample_counts\":{\"retained\":" +
            std::to_string(sample_count);
    json += ",\"omitted\":" +
            std::to_string(generated_count - sample_count);
    json += ",\"limit\":" +
            std::to_string(options.max_diagnostic_samples) + "}}";
    diagnostics.operator_lineage_json = std::move(json);
}

void SolveWork::Impl::finalize_incremental_diagnostics() {
    if (!incremental_envelope_closed) {
        if (requested_bounded_finish) {
            action_envelope_ledger.mark_queued_unresolved(
                ActionEnvelopeStopOwner::RequestedBoundedFinish,
                "requested_bounded_finish");
        } else if (result.diagnostics.resource_cap_hit) {
            const std::string cap = result.diagnostics.cap_hits.empty()
                ? std::string("solver_resource_cap")
                : result.diagnostics.cap_hits.back();
            action_envelope_ledger.mark_queued_unresolved(
                ActionEnvelopeStopOwner::ResourceCap, cap);
        } else {
            action_envelope_ledger.mark_queued_unresolved(
                ActionEnvelopeStopOwner::MissingVerifiedUpper,
                "solver_finished_with_open_action_envelope");
        }
    }
    SolveDiagnostics& diagnostics = result.diagnostics;
    diagnostics.incremental_action_generation =
        incremental_action_generation;
    diagnostics.incremental_action_envelope_closed =
        !incremental_action_generation || incremental_envelope_closed;
    diagnostics.incremental_actions_unevaluated =
        incremental_unevaluated_actions;
    diagnostics.incremental_actions_evaluating =
        expansion_active && expansion_is_incremental_alternative ? 1 : 0;
    diagnostics.incremental_actions_unresolved =
        incremental_resource_unresolved_actions;
    diagnostics.incremental_actions_inapplicable =
        incremental_inapplicable_actions;
    diagnostics.incremental_unique_kernel_evaluations =
        incremental_unique_kernel_evaluations;
    diagnostics.incremental_carrier_kernel_reuses =
        incremental_carrier_kernel_reuses;
    diagnostics.incremental_carrier_ladder_epochs =
        incremental_carrier_ladder_epochs;
    diagnostics.incremental_carrier_ladder_candidates =
        incremental_carrier_ladder_candidates;
    diagnostics.incremental_carrier_ladder_goal_subsets =
        incremental_carrier_ladder_goal_subsets;
    diagnostics.incremental_bellman_reoptimizations =
        incremental_reoptimizations;
    diagnostics.incremental_first_alternative_expanded_states =
        incremental_first_alternative_expanded_states;
    diagnostics.incremental_refinement_rounds =
        incremental_refinement_rounds;
    diagnostics.incremental_refinement_states_selected =
        incremental_refinement_states_selected;
    diagnostics.incremental_rows_reconsidered =
        incremental_rows_reconsidered;
    diagnostics.incremental_upper_policy_updates =
        incremental_upper_policy_updates;
    diagnostics.incremental_upper_policy_passes_requested =
        incremental_upper_policy_passes_requested;
    diagnostics.incremental_upper_policy_passes_started =
        incremental_upper_policy_passes_started;
    diagnostics.incremental_upper_policy_passes_proper =
        incremental_upper_policy_passes_proper;
    diagnostics.incremental_upper_policy_passes_rejected =
        incremental_upper_policy_passes_rejected;
    diagnostics.incremental_upper_policy_fixed_policy_proofs =
        incremental_upper_policy_fixed_policy_proofs;
    diagnostics.incremental_upper_policy_last_failure =
        incremental_upper_policy_last_failure;
    diagnostics.incremental_anytime_policy_attempts =
        incremental_anytime_policy_attempts;
    diagnostics.incremental_anytime_policy_successes =
        incremental_anytime_policy_successes;
    diagnostics.incremental_anytime_policy_last_completed_rows =
        incremental_anytime_policy_last_completed_rows;
    diagnostics.incremental_anytime_policy_best_upper =
        incremental_anytime_policy_best_upper;
    diagnostics.incremental_anytime_policy_last_failure =
        incremental_anytime_policy_last_failure;
    diagnostics.incremental_missing_frontier_discovered =
        incremental_missing_frontier_discovered;
    diagnostics.incremental_missing_frontier_priority_offers =
        incremental_missing_frontier_priority_offers;
    diagnostics.incremental_missing_frontier_service_completions =
        incremental_missing_frontier_service_completions;
    diagnostics.incremental_missing_frontier_max_open =
        incremental_missing_frontier_max_open;
    diagnostics.incremental_missing_frontier_open =
        incremental_anytime_missing_frontier_states.size();
    diagnostics.incremental_refinement_uncertainty =
        incremental_refinement_uncertainty;
    diagnostics.incremental_action_witnesses.clear();
    diagnostics.incremental_action_witnesses_omitted = 0;
    const bool exact_final_values =
        incremental_envelope_closed && optimization_converged() &&
        (focused_bound_proved || !focused_mode);
    const std::vector<double> certified_lower =
        certified_incremental_lower_values();
    for (IncrementalAlternativeRow& candidate :
         incremental_alternative_rows) {
        if (exact_final_values) {
            candidate.lower_q = sparse_row_q_for_values(
                candidate.row_index, result.values);
            candidate.upper_q = candidate.lower_q;
        } else {
            candidate.lower_q = sparse_row_q_for_values(
                candidate.row_index, certified_lower);
            if (output_incumbent.has_value()) {
                candidate.upper_q = sparse_row_q_for_values(
                    candidate.row_index, output_incumbent->values);
            }
        }
        const char* status = "unresolved";
        switch (candidate.status) {
        case IncrementalAlternativeRow::Status::PendingValues:
        case IncrementalAlternativeRow::Status::Unresolved:
            ++diagnostics.incremental_actions_unresolved;
            break;
        case IncrementalAlternativeRow::Status::Admitted:
            status = "admitted";
            ++diagnostics.incremental_actions_admitted;
            break;
        case IncrementalAlternativeRow::Status::NonImproving:
            status = "evaluated_non_improving";
            ++diagnostics.incremental_actions_non_improving;
            break;
        }
        diagnostics.incremental_states_outside_chaos_support +=
            candidate.states_added;
        if (diagnostics.incremental_action_witnesses.size() >=
            options.max_diagnostic_samples) {
            ++diagnostics.incremental_action_witnesses_omitted;
            continue;
        }
        std::string witness = "{\"state\":";
        witness += std::to_string(candidate.state);
        witness += ",\"action\":";
        append_json_string(
            witness,
            calc.operators().at(candidate.operator_index).id);
        witness += ",\"status\":";
        append_json_string(witness, status);
        witness += ",\"lower_q\":" + finite_json(candidate.lower_q);
        witness += ",\"upper_q\":" + finite_json(candidate.upper_q);
        witness += ",\"improvement_margin\":" +
                   finite_json(candidate.improvement_margin);
        witness += ",\"states_outside_chaos_support\":" +
                   std::to_string(candidate.states_added);
        witness += "}";
        diagnostics.incremental_action_witnesses.push_back(
            std::move(witness));
    }
    refresh_action_envelope_ledger_diagnostics(diagnostics);
    refresh_anytime_scheduler_diagnostics(diagnostics);
    refresh_operator_lineage_diagnostics(diagnostics, &result);
    refresh_carrier_ladder_exact_boundary_diagnostics(diagnostics);
}

void SolveWork::Impl::finalize_upper_policy_provenance() {
    SolveDiagnostics& diagnostics = result.diagnostics;
    diagnostics.upper_policy_provenance_samples.clear();
    diagnostics.upper_policy_provenance_samples_omitted = 0;
    diagnostics.upper_policy_provenance_candidate_count = 0;
    diagnostics.upper_policy_provenance_retained_bytes = 0;
    if (!incremental_action_generation || !output_incumbent.has_value() ||
        transition_cache == nullptr) {
        return;
    }

    const BoundedPolicyIncumbent& incumbent = *output_incumbent;
    const std::uint64_t no_row =
        std::numeric_limits<std::uint64_t>::max();
    struct Candidate {
        double contribution = 0.0;
        double influence = 0.0;
        std::uint32_t state = kNoId;
        std::uint64_t parent_row =
            std::numeric_limits<std::uint64_t>::max();
        std::uint32_t parent_operator = kNoId;
        const char* influence_kind = "root_transition_probability";
    };
    std::vector<Candidate> candidates;
    const std::uint64_t sample_limit = options.max_diagnostic_samples;

    const auto is_promising_family = [&](const std::uint32_t op) {
        if (op == kNoId || op >= calc.operators().size()) return false;
        const std::string& id = calc.operators()[op].id;
        return id.find("fossil") != std::string::npos ||
               id.find("harvest") != std::string::npos;
    };
    const auto better_candidate =
        [&](const Candidate& left, const Candidate& right) {
            if (left.contribution != right.contribution) {
                return left.contribution > right.contribution;
            }
            if (left.state != right.state) return left.state < right.state;
            const std::string& left_id =
                calc.operators()[left.parent_operator].id;
            const std::string& right_id =
                calc.operators()[right.parent_operator].id;
            if (left_id != right_id) return left_id < right_id;
            return left.parent_row < right.parent_row;
        };
    const auto add_candidate = [&](const std::uint32_t state,
                                   const double influence,
                                   const std::uint64_t parent_row,
                                   const std::uint32_t parent_operator,
                                   const char* influence_kind) {
        if (state == result.start_state || state >= calc.state_count() ||
            calc.is_goal_state(calc.state(state)) ||
            influence <= 0.0 || !std::isfinite(influence)) {
            return;
        }
        const double upper =
            state < incumbent.values.size()
                ? incumbent.values[state]
                : kInfinity;
        if (!std::isfinite(upper) || upper < 0.0) return;
        ++diagnostics.upper_policy_provenance_candidate_count;
        Candidate next{
            influence * upper, influence, state, parent_row,
            parent_operator, influence_kind};
        if (sample_limit == 0) return;
        if (candidates.size() < sample_limit) {
            candidates.push_back(std::move(next));
            return;
        }
        auto worst = candidates.begin();
        for (auto candidate = candidates.begin() + 1;
             candidate != candidates.end(); ++candidate) {
            if (better_candidate(*worst, *candidate)) {
                worst = candidate;
            }
        }
        if (better_candidate(next, *worst)) {
            *worst = std::move(next);
        }
    };

    for (const IncrementalAlternativeRow& retained :
         incremental_alternative_rows) {
        if (retained.state != result.start_state ||
            retained.row_index >= transition_cache->rows.size() ||
            !is_promising_family(retained.operator_index)) {
            continue;
        }
        const SparseRow& row =
            transition_cache->rows[retained.row_index];
        for (std::uint32_t i = 0; i < row.transition_count; ++i) {
            const std::uint64_t offset = row.transition_offset + i;
            add_candidate(
                transition_cache->successors[offset],
                transition_cache->probabilities[offset],
                retained.row_index, retained.operator_index,
                "root_transition_probability");
        }
        for (std::uint32_t i = 0; i < row.choice_count; ++i) {
            const SparseChoiceGroup& group =
                transition_cache->choices[row.choice_offset + i];
            std::uint32_t selected =
                group.has_self ? result.start_state : kNoId;
            double selected_upper =
                group.has_self &&
                        result.start_state < incumbent.values.size()
                    ? incumbent.values[result.start_state]
                    : kInfinity;
            for (std::uint32_t s = 0; s < group.successor_count; ++s) {
                const std::uint32_t successor =
                    transition_cache->choice_successors[
                        group.successor_offset + s];
                const double upper =
                    successor < incumbent.values.size()
                        ? incumbent.values[successor]
                        : kInfinity;
                if (sparse_policy_choice_precedes(
                        upper, successor,
                        selected_upper, selected)) {
                    selected = successor;
                    selected_upper = upper;
                }
            }
            add_candidate(
                selected, group.probability, retained.row_index,
                retained.operator_index, "root_choice_probability");
        }
    }

    std::stable_sort(
        candidates.begin(), candidates.end(),
        better_candidate);
    diagnostics.upper_policy_provenance_samples_omitted =
        diagnostics.upper_policy_provenance_candidate_count -
        candidates.size();

    std::uint64_t witness_identity = 1469598103934665603ULL;
    const auto mix = [&](const std::uint64_t value) {
        witness_identity ^= value;
        witness_identity *= 1099511628211ULL;
    };
    mix(1); /* provenance/witness schema version */
    mix(incumbent.graph_identity);
    mix(incumbent.goal_identity);
    mix(incumbent.economy_identity);
    mix(incumbent.action_vocabulary_identity);
    mix(incumbent.restart_operator);
    mix(incumbent.restart_state);
    mix(incumbent.fallback_anchor_state);
    for (std::uint32_t state = 0;
         state < incumbent.policy_rows.size(); ++state) {
        const std::uint64_t row = incumbent.policy_rows[state];
        if (row == no_row) continue;
        mix(state);
        mix(row);
        if (state < incumbent.values.size()) {
            mix(std::bit_cast<std::uint64_t>(incumbent.values[state]));
        }
    }

    /*
     * Keep this optional section bounded independently as well as by the
     * serializer's hard whole-document limit. One quarter leaves room for
     * the existing stable telemetry fields even under deliberately small
     * diagnostic limits.
     */
    const std::uint64_t byte_budget =
        options.max_telemetry_json_bytes / 4;
    for (const Candidate& candidate : candidates) {
        if (diagnostics.upper_policy_provenance_samples.size() >=
            sample_limit) {
            ++diagnostics.upper_policy_provenance_samples_omitted;
            continue;
        }
        const AbstractState& state = calc.state(candidate.state);
        const double current_upper =
            candidate.state < incumbent.values.size()
                ? incumbent.values[candidate.state]
                : kInfinity;
        const std::uint64_t selected_row =
            candidate.state < incumbent.policy_rows.size()
                ? incumbent.policy_rows[candidate.state]
                : no_row;
        std::uint32_t selected_operator = kNoId;
        if (selected_row != no_row && selected_row < priced_rows.size()) {
            selected_operator =
                priced_rows[selected_row].operator_index;
        } else if (candidate.state <
                   incumbent.frontier_operators.size()) {
            selected_operator =
                incumbent.frontier_operators[candidate.state];
        }
        const bool local_continuation =
            selected_row != no_row &&
            selected_row < transition_cache->rows.size();
        const bool restart_fallback =
            selected_operator == incumbent.restart_operator;

        std::vector<std::string> materialized;
        for (const std::uint64_t row :
             state_row_indices(*transition_cache, candidate.state)) {
            if (row >= priced_rows.size()) continue;
            const std::uint32_t op = priced_rows[row].operator_index;
            if (op == kNoId || op >= calc.operators().size()) continue;
            materialized.push_back(calc.operators()[op].id);
        }
        std::sort(materialized.begin(), materialized.end());
        materialized.erase(
            std::unique(materialized.begin(), materialized.end()),
            materialized.end());
        const std::size_t materialized_limit =
            std::min<std::size_t>(
                materialized.size(), options.max_diagnostic_samples);

        std::string sample = "{\"state\":";
        sample += std::to_string(candidate.state);
        sample += ",\"satisfied_goal_subset\":" +
                  std::to_string(
                      satisfied_goal_mask_for_state(candidate.state));
        sample += ",\"carrier\":{\"rarity\":" +
                  std::to_string(state.rarity);
        sample += ",\"prefix_count\":" +
                  std::to_string(state.prefix_count);
        sample += ",\"suffix_count\":" +
                  std::to_string(state.suffix_count);
        sample += ",\"blocked_goal_mask\":" +
                  std::to_string(state.blocked_mask);
        sample += ",\"fractured_goal_mask\":" +
                  std::to_string(state.fractured_goal_mask);
        sample += ",\"crafted_goal_mask\":" +
                  std::to_string(state.crafted_goal_mask);
        sample += ",\"fractured_metamod_flags\":" +
                  std::to_string(state.fractured_metamod_flags);
        sample += ",\"flags\":" + std::to_string(state.flags);
        sample += ",\"prefixes_locked\":" +
                  std::string(
                      state.flags & kFlagPrefixesLocked
                          ? "true" : "false");
        sample += ",\"suffixes_locked\":" +
                  std::string(
                      state.flags & kFlagSuffixesLocked
                          ? "true" : "false");
        sample += ",\"influence_bits\":" +
                  std::to_string(state.influence_bits);
        sample += ",\"searing_exarch_tier\":" +
                  std::to_string(state.searing_exarch_tier);
        sample += ",\"eater_of_worlds_tier\":" +
                  std::to_string(state.eater_of_worlds_tier);
        sample += ",\"retry_basin\":" +
                  std::to_string(state.goal_progress_retry_basin);
        sample += "},\"promising_parent_action\":";
        append_json_string(
            sample,
            calc.operators()[candidate.parent_operator].id);
        sample += ",\"promising_parent_row\":" +
                  std::to_string(candidate.parent_row);
        sample += ",\"influence_kind\":";
        append_json_string(sample, candidate.influence_kind);
        sample += ",\"influence\":" +
                  finite_json(candidate.influence);
        sample += ",\"current_executable_upper\":" +
                  finite_json(current_upper);
        sample += ",\"parent_upper_q_contribution\":" +
                  finite_json(candidate.contribution);
        sample += ",\"selected_upper_policy_row\":";
        sample += selected_row == no_row
                      ? "null"
                      : std::to_string(selected_row);
        sample += ",\"selected_upper_policy_action\":";
        if (selected_operator == kNoId ||
            selected_operator >= calc.operators().size()) {
            sample += "null";
        } else {
            append_json_string(
                sample, calc.operators()[selected_operator].id);
        }
        sample += ",\"fallback_source\":";
        if (local_continuation) {
            append_json_string(sample, "local_exact_row");
        } else if (restart_fallback) {
            append_json_string(sample, "restart_then_chaos_fallback");
        } else {
            append_json_string(
                sample, "constructive_terminal_fallback");
        }
        sample += ",\"uses_local_continuation\":" +
                  std::string(local_continuation ? "true" : "false");
        sample += ",\"uses_restart_chaos\":" +
                  std::string(restart_fallback ? "true" : "false");
        sample += ",\"materialized_candidate_actions\":[";
        for (std::size_t i = 0; i < materialized_limit; ++i) {
            if (i != 0) sample += ',';
            append_json_string(sample, materialized[i]);
        }
        sample += "],\"materialized_candidate_actions_omitted\":" +
                  std::to_string(
                      materialized.size() - materialized_limit);
        sample += ",\"no_cheaper_continuation_reason\":";
        if (local_continuation) {
            append_json_string(
                sample, "local_continuation_selected");
        } else if (materialized.empty()) {
            append_json_string(
                sample, "no_materialized_local_action");
        } else {
            append_json_string(
                sample,
                "materialized_rows_not_strictly_cheaper_than_fallback");
        }
        sample += ",\"policy_witness\":{\"version\":1";
        char hash_buffer[17]{};
        std::snprintf(
            hash_buffer, sizeof(hash_buffer), "%016llx",
            static_cast<unsigned long long>(witness_identity));
        sample += ",\"identity\":\"";
        sample += hash_buffer;
        std::snprintf(
            hash_buffer, sizeof(hash_buffer), "%016llx",
            static_cast<unsigned long long>(incumbent.graph_identity));
        sample += "\",\"graph_identity\":\"";
        sample += hash_buffer;
        std::snprintf(
            hash_buffer, sizeof(hash_buffer), "%016llx",
            static_cast<unsigned long long>(incumbent.goal_identity));
        sample += "\",\"goal_identity\":\"";
        sample += hash_buffer;
        std::snprintf(
            hash_buffer, sizeof(hash_buffer), "%016llx",
            static_cast<unsigned long long>(incumbent.economy_identity));
        sample += "\",\"economy_identity\":\"";
        sample += hash_buffer;
        std::snprintf(
            hash_buffer, sizeof(hash_buffer), "%016llx",
            static_cast<unsigned long long>(
                incumbent.action_vocabulary_identity));
        sample += "\",\"action_vocabulary_identity\":\"";
        sample += hash_buffer;
        sample += "\",\"proper\":true";
        sample += ",\"properness_status\":";
        append_json_string(
            sample, "existing_executable_fallback_witness");
        sample += "}}";

        if (sample.size() > byte_budget ||
            diagnostics.upper_policy_provenance_retained_bytes >
                byte_budget - sample.size()) {
            ++diagnostics.upper_policy_provenance_samples_omitted;
            continue;
        }
        diagnostics.upper_policy_provenance_retained_bytes +=
            sample.size();
        diagnostics.upper_policy_provenance_samples.push_back(
            std::move(sample));
    }
}

} // namespace solver
} // namespace poecraft
