#include "solver_solve_types.hpp"
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
    } catch (const SolverResourceLimit&) {
        if (incremental_resource_unresolved_actions == unresolved_before) {
            ++incremental_resource_unresolved_actions;
        }
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
    incremental_dynamic_prepared = true;
    incremental_dynamic_prepare_active = false;
    incremental_dynamic_operator_cursor = 0;
    if (options.high_impact_executable_uppers &&
        incremental_unevaluated_actions != 0) {
        --incremental_unevaluated_actions;
    }
    incremental_unevaluated_actions +=
        incremental_dynamic_operator_indices.size();
    expansion_operator_indices.clear();
    return true;
}

bool SolveWork::Impl::schedule_next_incremental_alternative(
        const bool continue_current_epoch_request) {
    if (!incremental_action_generation || incremental_envelope_closed) {
        return false;
    }
    const bool continue_current_epoch =
        continue_current_epoch_request ||
        incremental_resume_epoch_after_dynamic_prepare;
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
            std::map<std::uint32_t, std::vector<std::uint32_t>> by_subset;
            for (std::size_t index = begin; index < end; ++index) {
                const std::uint32_t state = incremental_carriers[index];
                by_subset[satisfied_goal_mask_for_state(state)]
                    .push_back(state);
            }
            const auto structural_order = [&](const std::uint32_t left,
                                              const std::uint32_t right) {
                const AbstractState& a = calc.state(left);
                const AbstractState& b = calc.state(right);
                const std::uint32_t a_fractured =
                    std::popcount(a.fractured_goal_mask);
                const std::uint32_t b_fractured =
                    std::popcount(b.fractured_goal_mask);
                if (a_fractured != b_fractured) {
                    return a_fractured > b_fractured;
                }
                const std::uint32_t a_protection = std::popcount(
                    a.flags & (kFlagPrefixesLocked | kFlagSuffixesLocked));
                const std::uint32_t b_protection = std::popcount(
                    b.flags & (kFlagPrefixesLocked | kFlagSuffixesLocked));
                if (a_protection != b_protection) {
                    return a_protection > b_protection;
                }
                const std::uint32_t a_satisfied = std::popcount(
                    satisfied_goal_mask_for_state(left));
                const std::uint32_t b_satisfied = std::popcount(
                    satisfied_goal_mask_for_state(right));
                const std::uint32_t a_explicit =
                    a.prefix_count + a.suffix_count;
                const std::uint32_t b_explicit =
                    b.prefix_count + b.suffix_count;
                const std::uint32_t a_unrelated =
                    a_explicit > a_satisfied
                        ? a_explicit - a_satisfied
                        : 0;
                const std::uint32_t b_unrelated =
                    b_explicit > b_satisfied
                        ? b_explicit - b_satisfied
                        : 0;
                return a_unrelated != b_unrelated
                    ? a_unrelated < b_unrelated
                    : left < right;
            };
            std::vector<std::uint32_t> subset_order;
            subset_order.reserve(by_subset.size());
            for (auto& [mask, carriers] : by_subset) {
                std::stable_sort(
                    carriers.begin(), carriers.end(), structural_order);
                subset_order.push_back(mask);
            }
            std::stable_sort(
                subset_order.begin(), subset_order.end(),
                [](const std::uint32_t left, const std::uint32_t right) {
                    const std::uint32_t left_progress = std::popcount(left);
                    const std::uint32_t right_progress = std::popcount(right);
                    return left_progress != right_progress
                        ? left_progress > right_progress
                        : left < right;
                });
            std::map<std::uint32_t, std::size_t> cursors;
            std::vector<std::uint32_t> ordered;
            ordered.reserve(end - begin);
            bool advanced = true;
            while (advanced) {
                advanced = false;
                for (const std::uint32_t mask : subset_order) {
                    auto& carriers = by_subset.at(mask);
                    std::size_t& cursor = cursors[mask];
                    if (cursor >= carriers.size()) continue;
                    ordered.push_back(carriers[cursor++]);
                    advanced = true;
                }
            }
            incremental_automatic_carrier_order = std::move(ordered);
            incremental_automatic_order_cursor = 0;
            ++incremental_carrier_ladder_epochs;
            incremental_carrier_ladder_candidates +=
                incremental_automatic_carrier_order.size();
            incremental_carrier_ladder_goal_subsets += by_subset.size();
        } else {
            incremental_automatic_carrier_order.clear();
            incremental_automatic_order_cursor = 0;
        }
    }
    if (options.high_impact_executable_uppers) {
        const auto pair_key = [](
            const std::uint32_t state,
            const std::uint32_t operator_index) {
            return (static_cast<std::uint64_t>(state) << 32) |
                operator_index;
        };
        const std::size_t carrier_checkpoint =
            std::min<std::size_t>(
                options.max_expanded_states,
                128);
        if (!incremental_alternative_rows.empty() &&
            incremental_carriers.size() < carrier_checkpoint) {
            return false;
        }
        const auto schedule_pair =
            [&](const std::uint32_t state,
                const std::uint32_t operator_index) {
                if (incremental_unevaluated_actions != 0) {
                    --incremental_unevaluated_actions;
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
                phase = SolvePhase::Expanding;
            };
        /* The high-impact path schedules delayed primitives operator-major,
         * but automatic compounds remain carrier-local. Drain that distinct
         * ledger first so an early executable-upper checkpoint cannot skip
         * synthesis and later earn exact closure from delayed rows alone. */
        while (incremental_automatic_order_cursor <
               incremental_automatic_carrier_order.size()) {
            const std::uint32_t state =
                incremental_automatic_carrier_order[
                    incremental_automatic_order_cursor];
            if (!incremental_dynamic_prepared) {
                incremental_resume_epoch_after_dynamic_prepare =
                    continue_current_epoch;
                incremental_dynamic_prepare_active = true;
                phase = SolvePhase::Expanding;
                return true;
            }
            if (incremental_dynamic_operator_cursor <
                incremental_dynamic_operator_indices.size()) {
                const std::uint32_t operator_index =
                    incremental_dynamic_operator_indices[
                        incremental_dynamic_operator_cursor++];
                if (incremental_completed_pairs.contains(
                        pair_key(state, operator_index))) {
                    if (incremental_unevaluated_actions != 0) {
                        --incremental_unevaluated_actions;
                    }
                    continue;
                }
                schedule_pair(state, operator_index);
                return true;
            }
            ++incremental_automatic_carrier_cursor;
            ++incremental_automatic_order_cursor;
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
        while (incremental_priority_task_cursor <
               incremental_priority_tasks.size()) {
            const IncrementalPriorityTask task =
                incremental_priority_tasks[
                    incremental_priority_task_cursor++];
            if (incremental_completed_pairs.contains(
                    pair_key(task.state, task.operator_index))) {
                continue;
            }
            schedule_pair(task.state, task.operator_index);
            return true;
        }
        if (continue_current_epoch) return false;
        if (!incremental_priority_tasks.empty()) {
            incremental_priority_tasks.clear();
            incremental_priority_task_cursor = 0;
        }
        /*
         * Global operator-major state/action scheduling. The first delayed
         * operator is evaluated at the root, then across every currently
         * materialized carrier before the next operator is considered.
         * This exposes multi-action continuation rows without exhausting the
         * root envelope or expanding an unrelated broad fringe first.
         */
        while (incremental_operator_cursor <
               delayed_operator_indices.size()) {
            if (incremental_carrier_cursor >=
                incremental_carriers.size()) {
                if (continue_current_epoch) return false;
                if (incremental_operator_cursor == 0 &&
                    !incremental_high_impact_continuation_refined) {
                    incremental_high_impact_continuation_refined = true;
                    schedule_high_impact_continuation_refinement();
                    if (incremental_carrier_cursor <
                        incremental_carriers.size()) {
                        continue;
                    }
                }
                if (incremental_operator_cursor == 0 &&
                    incremental_high_impact_wave < 4 &&
                    prepare_high_impact_policy_wave(
                        incremental_high_impact_wave)) {
                    ++incremental_high_impact_wave;
                    const IncrementalPriorityTask task =
                        incremental_priority_tasks[
                            incremental_priority_task_cursor++];
                    schedule_pair(
                        task.state, task.operator_index);
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
            if (incremental_completed_pairs.contains(
                    pair_key(state, operator_index))) {
                continue;
            }
            schedule_pair(state, operator_index);
            return true;
        }
        /* Carrier discovery is monotonic and may continue after an operator
         * sweep. Close the exact Cartesian ledger before reporting that no
         * alternative remains; this scan is reached only after the fast
         * operator-major cursors are exhausted. */
        if (continue_current_epoch) return false;
        for (const std::uint32_t operator_index :
             delayed_operator_indices) {
            for (const std::uint32_t state : incremental_carriers) {
                if (incremental_completed_pairs.contains(
                        pair_key(state, operator_index))) {
                    continue;
                }
                schedule_pair(state, operator_index);
                return true;
            }
        }
        incremental_unevaluated_actions = 0;
        return false;
    }
    while (incremental_carrier_cursor < incremental_carriers.size()) {
        const std::uint32_t state =
            incremental_carriers[incremental_carrier_cursor];
        std::uint32_t operator_index = kNoId;
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
            incremental_dynamic_prepare_active = true;
            phase = SolvePhase::Expanding;
            return true;
        }
        if (incremental_dynamic_operator_cursor <
            incremental_dynamic_operator_indices.size()) {
            operator_index = incremental_dynamic_operator_indices[
                incremental_dynamic_operator_cursor++];
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
        if (incremental_unevaluated_actions != 0) {
            --incremental_unevaluated_actions;
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

bool SolveWork::Impl::schedule_high_impact_continuation_refinement() {
    if (!options.high_impact_executable_uppers ||
        incremental_refinement_active) {
        return false;
    }
    const std::size_t state_count = calc.state_count();
    std::vector<double> priority(state_count, 0.0);
    for (std::uint32_t owner = 0;
         owner < expanded.size(); ++owner) {
        if (!expanded[owner] ||
            calc.is_goal_state(calc.state(owner))) {
            continue;
        }
        for (const std::uint64_t row_index :
             state_row_indices(*transition_cache, owner)) {
            const SparseRow& row =
                transition_cache->rows.at(row_index);
            if (!row.admitted || row_index >= priced_rows.size()) {
                continue;
            }
            const std::uint32_t operator_index =
                priced_rows[row_index].operator_index;
            if (operator_index >= calc.operators().size()) continue;
            const PlannerOperator& planner =
                calc.operators()[operator_index];
            bool fracture = planner.automatic_kind ==
                AutomaticCandidateKind::Fracture;
            if (!fracture &&
                planner.kind == PlannerOperatorKind::Primitive &&
                planner.primitive_action <
                    calc.registry().actions.size()) {
                fracture =
                    calc.registry()
                        .actions[planner.primitive_action]
                        .params.type == ActionType::Fracture;
            }
            if (!fracture) continue;
            for (std::uint32_t i = 0;
                 i < row.transition_count; ++i) {
                const std::uint64_t offset =
                    row.transition_offset + i;
                const std::uint32_t successor =
                    transition_cache->successors.at(offset);
                if (successor >= state_count ||
                    calc.is_goal_state(calc.state(successor)) ||
                    (successor < expanded.size() &&
                     expanded[successor])) {
                    continue;
                }
                const AbstractState& state = calc.state(successor);
                const std::uint32_t preserved =
                    std::popcount(state.fractured_goal_mask);
                const std::uint32_t satisfied =
                    std::popcount(
                        satisfied_goal_mask_for_state(successor));
                const double influence =
                    transition_cache->probabilities.at(offset);
                priority[successor] += influence *
                    (1.0 + 1024.0 * preserved +
                     64.0 * satisfied);
            }
        }
    }
    std::vector<std::uint32_t> ranked;
    for (std::uint32_t state = 0;
         state < state_count; ++state) {
        if (priority[state] > 0.0) ranked.push_back(state);
    }
    std::stable_sort(
        ranked.begin(), ranked.end(),
        [&](const std::uint32_t left,
            const std::uint32_t right) {
            return priority[left] != priority[right]
                       ? priority[left] > priority[right]
                       : left < right;
        });
    const std::size_t batch = std::min<std::size_t>(
        ranked.size(), 16);
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

bool SolveWork::Impl::prepare_high_impact_policy_wave(
        const std::uint32_t wave) {
    const bool fracture_wave = wave % 2 == 0;
    const std::uint32_t required_fractures = wave / 2 + 1;
    std::uint32_t target_operator = kNoId;
    if (fracture_wave) {
        for (const PricedOperator& priced : operators) {
            const PlannerOperator& planner =
                calc.operators().at(priced.index);
            if (planner.automatic_kind ==
                AutomaticCandidateKind::Fracture) {
                target_operator = priced.index;
                break;
            }
            if (planner.kind == PlannerOperatorKind::Primitive &&
                planner.primitive_action <
                    calc.registry().actions.size() &&
                calc.registry()
                        .actions[planner.primitive_action]
                        .params.type == ActionType::Fracture) {
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
        const bool source_is_harvest =
            calc.operators()[source.operator_index].kind ==
                PlannerOperatorKind::Primitive &&
            calc.operators()[source.operator_index].primitive_action <
                calc.registry().actions.size() &&
            calc.registry()
                    .actions[calc.operators()[source.operator_index]
                                 .primitive_action]
                    .params.type == ActionType::HarvestReforge;
        const bool source_is_fracture =
            calc.operators()[source.operator_index].automatic_kind ==
                AutomaticCandidateKind::Fracture ||
            (calc.operators()[source.operator_index].kind ==
                 PlannerOperatorKind::Primitive &&
             calc.operators()[source.operator_index].primitive_action <
                 calc.registry().actions.size() &&
             calc.registry()
                     .actions[calc.operators()[source.operator_index]
                                  .primitive_action]
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
        for (std::uint32_t i = 0;
             i < row.transition_count; ++i) {
            const std::uint64_t offset =
                row.transition_offset + i;
            const std::uint32_t successor =
                transition_cache->successors.at(offset);
            if (successor >= priority.size() ||
                calc.is_goal_state(calc.state(successor))) {
                continue;
            }
            const AbstractState& state = calc.state(successor);
            const std::uint32_t fractures =
                std::popcount(state.fractured_goal_mask);
            const std::uint32_t satisfied =
                std::popcount(
                    satisfied_goal_mask_for_state(successor));
            if (fracture_wave) {
                if (fractures < required_fractures ||
                    satisfied <= fractures) {
                    continue;
                }
                const PlannerOperator& target =
                    calc.operators()[target_operator];
                if (target.kind !=
                        PlannerOperatorKind::Primitive ||
                    target.primitive_action >=
                        calc.registry().actions.size() ||
                    !action_legal(
                        session,
                        calc.registry()
                            .actions[target.primitive_action],
                        state)) {
                    continue;
                }
            } else if (fractures < required_fractures + 1) {
                continue;
            }
            priority[successor] +=
                transition_cache->probabilities.at(offset) *
                (1.0 + 4096.0 * fractures +
                 256.0 * satisfied);
        }
    }
    std::vector<std::uint32_t> ranked;
    for (std::uint32_t state = 0;
         state < priority.size(); ++state) {
        if (priority[state] > 0.0) ranked.push_back(state);
    }
    std::stable_sort(
        ranked.begin(), ranked.end(),
        [&](const std::uint32_t left,
            const std::uint32_t right) {
            return priority[left] != priority[right]
                       ? priority[left] > priority[right]
                       : left < right;
        });
    if (ranked.size() > 16) ranked.resize(16);
    incremental_priority_tasks.clear();
    incremental_priority_task_cursor = 0;
    incremental_priority_tasks.reserve(ranked.size());
    for (const std::uint32_t state : ranked) {
        incremental_priority_tasks.push_back(
            {state, target_operator});
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
    if (!options.high_impact_executable_uppers ||
        !incremental_action_generation || incremental_envelope_closed ||
        incremental_upper_policy_pass == false ||
        incremental_alternative_rows.size() <
            incremental_anytime_next_row_checkpoint) {
        return false;
    }

    const std::size_t completed = incremental_alternative_rows.size();
    ++incremental_anytime_policy_attempts;
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
    const bool installed = try_install_reachable_incumbent(false);
    if (installed && output_incumbent.has_value() &&
        output_incumbent->certified_upper_bound < prior) {
        ++incremental_anytime_policy_successes;
        incremental_anytime_policy_best_upper = std::min(
            incremental_anytime_policy_best_upper,
            output_incumbent->certified_upper_bound);
    }
    return installed;
}

std::vector<double>
SolveWork::Impl::certified_incremental_lower_values() {
    std::vector<double> lower(calc.state_count(), 0.0);
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
            lower[state] = optimistic_completion_cost_for_state(state);
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
            IncrementalAlternativeRow::Status::Unresolved) {
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
            ? 128
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
    incremental_anytime_missing_frontier_states.erase(
        std::remove_if(
            incremental_anytime_missing_frontier_states.begin(),
            incremental_anytime_missing_frontier_states.end(),
            [&](const std::uint32_t state) {
                return state < selected.size() && selected[state];
            }),
        incremental_anytime_missing_frontier_states.end());
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

bool SolveWork::Impl::classify_incremental_alternatives() {
    const std::vector<double>* upper_values = nullptr;
    const std::vector<double> certified_lower =
        certified_incremental_lower_values();
    bool restricted_graph_closed = true;
    for (std::uint32_t state = 0; state < calc.state_count(); ++state) {
        if (!calc.is_goal_state(calc.state(state)) &&
            (state >= expanded.size() || !expanded[state])) {
            restricted_graph_closed = false;
            break;
        }
    }
    const bool exact_restricted_values =
        (restricted_graph_closed &&
         incremental_restricted_values_ready &&
         optimization_converged()) ||
        (optimization_converged() &&
         (focused_bound_proved || !focused_mode));
    if (exact_restricted_values) {
        upper_values = &result.values;
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
        }
    } else if (output_incumbent.has_value()) {
        upper_values = &output_incumbent->values;
    }

    const bool reclassify_all =
        !options.high_impact_executable_uppers ||
        incremental_reclassify_all;
    incremental_reclassify_all = false;
    bool admitted = false;
    for (IncrementalAlternativeRow& candidate :
         incremental_alternative_rows) {
        if (candidate.status ==
            IncrementalAlternativeRow::Status::Admitted) {
            continue;
        }
        if (!reclassify_all &&
            candidate.status ==
                IncrementalAlternativeRow::Status::Unresolved) {
            continue;
        }
        candidate.status =
            IncrementalAlternativeRow::Status::PendingValues;
        ++incremental_rows_reconsidered;
        candidate.lower_q = sparse_row_q_for_values(
            candidate.row_index, certified_lower);
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
            continue;
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
            admitted = true;
            continue;
        }
        if (std::isfinite(current_upper) &&
            current_upper < kValueCeiling &&
            candidate.lower_q >= current_upper) {
            candidate.status =
                IncrementalAlternativeRow::Status::NonImproving;
            candidate.improvement_margin =
                current_upper - candidate.lower_q;
        } else {
            candidate.status =
                IncrementalAlternativeRow::Status::Unresolved;
        }
    }

    /* Rows proved improving against the same incumbent are independent
     * admissions, not prescribed policy choices. Expose the whole batch and
     * let one exact Bellman re-optimization choose among them. */
    if (admitted) return true;

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
            return false;
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
            restricted_graph_closed &&
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
            }
            /* The next restart must be the ordinary closed-envelope lower
             * optimization, not another one-proof upper-witness pass. If
             * closure were delayed until the following classification, that
             * upper pass would restore the stale restricted lower snapshot
             * and the solver could publish ExactClosed without ever running
             * Howard selection over this final joint row set. */
            incremental_envelope_closed = true;
            return true;
        }
        if (unadmitted == incremental_alternative_rows.end() &&
            incremental_unevaluated_actions == 0 &&
            automatic_preparation_closed) {
            incremental_envelope_closed = true;
        }
    }
    return false;
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

void SolveWork::Impl::finalize_incremental_diagnostics() {
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
