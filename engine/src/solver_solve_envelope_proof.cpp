#include "solver_solve_types.hpp"

namespace poecraft {
namespace solver {

using namespace solve_detail;

double SolveWork::Impl::refresh_envelope_bellman_pattern() {
    ProofPatternContract& pattern = contract(
        ProofPatternKind::EnvelopeBellman);
    pattern.minimizing_action.clear();
    pattern.fallback_reason = "unavailable";
    pattern.refinement_trace.clear();
    pattern.start_contribution = kInfinity;
    pattern.residual = kInfinity;
    pattern.solution_sweeps = 0;
    pattern.converged = false;
    envelope_bellman_lower = kInfinity;
    if (result.start_state >= calc.state_count()) {
        pattern.fallback_reason = "invalid_start_state";
        return envelope_bellman_lower;
    }
    const bool clean_projection =
        clean_goal_cover_eligible(result.start_state) &&
        !clean_goal_start_action_floor.empty();
    const bool identity_projection = !clean_projection &&
        !clean_goal_cover_cost.empty() &&
        !clean_goal_start_action_floor.empty();
    const bool carrier_projection =
        carrier_goal_progress_eligible(result.start_state) &&
        !carrier_goal_action_floor.empty();
    const bool bounded_gain_projection = carrier_projection &&
        !bounded_gain_goal_progress_cost.empty() &&
        !bounded_gain_action_floor.empty();
    if (!clean_projection && !identity_projection &&
        !carrier_projection) {
        pattern.fallback_reason = "start_projection_unavailable";
        return envelope_bellman_lower;
    }

    /* Keep this pattern out of its own successor calculation. The existing
     * independently admissible maximum is a common Bellman lower for every
     * exact action row. */
    const double common = completion_proof_lower_value(result.start_state);
    if (!std::isfinite(common) || common < 0.0 ||
        common >= kValueCeiling) {
        pattern.fallback_reason = "common_lower_unavailable";
        return envelope_bellman_lower;
    }
    const auto successor_lower = [&](const std::uint32_t successor) {
        if (successor >= calc.state_count()) return 0.0;
        if (calc.is_goal_state(calc.state(successor))) return 0.0;
        return completion_proof_lower_value(successor);
    };
    const auto exact_row_lower_with = [&] (
        const std::uint64_t row_index,
        const auto& successor_value) {
        if (row_index >= transition_cache->rows.size() ||
            row_index >= priced_rows.size()) {
            return kInfinity;
        }
        const SparseRow& row = transition_cache->rows[row_index];
        const PricedSparseRow& priced = priced_rows[row_index];
        if (!row.admitted || priced.operator_index == kNoId ||
            !std::isfinite(priced.cost) || priced.cost < 0.0) {
            return kInfinity;
        }
        double constant = priced.cost;
        double self_probability = row.self_probability;
        for (std::uint32_t i = 0; i < row.transition_count; ++i) {
            const std::uint64_t offset = row.transition_offset + i;
            const std::uint32_t successor =
                transition_cache->successors.at(offset);
            if (successor == result.start_state) continue;
            const double lower = successor_value(successor);
            if (!std::isfinite(lower) || lower < 0.0) return kInfinity;
            constant += transition_cache->probabilities.at(offset) * lower;
        }
        for (std::uint32_t i = 0; i < row.choice_count; ++i) {
            const SparseChoiceGroup& group = transition_cache->choices.at(
                row.choice_offset + i);
            double selected = kInfinity;
            for (std::uint32_t j = 0; j < group.successor_count; ++j) {
                selected = std::min(
                    selected,
                    successor_value(
                        transition_cache->choice_successors.at(
                            group.successor_offset + j)));
            }
            if (std::isfinite(selected)) {
                constant += group.probability * selected;
            } else if (group.has_self) {
                self_probability += group.probability;
            } else {
                return kInfinity;
            }
        }
        const double denominator = 1.0 - self_probability;
        if (denominator <= 1e-15) {
            /* A fully self-looping admitted row is a proved nonproductive
             * action, not missing envelope coverage. Its concrete Q is
             * infinite; retain a finite sentinel below the solver ceiling so
             * it cannot pin the complete operator minimum. */
            return kValueCeiling * 0.5;
        }
        const double lower = constant / denominator;
        return std::isfinite(lower) && lower >= 0.0 &&
                lower < kValueCeiling
            ? lower
            : kInfinity;
    };
    const auto exact_row_lower = [&](const std::uint64_t row_index) {
        return exact_row_lower_with(row_index, successor_lower);
    };

    std::vector<double> exact_action_lower(
        calc.registry().actions.size(), kInfinity);
    if (result.start_state < transition_cache->state_rows.size()) {
        for (const std::uint64_t row_index :
             state_row_indices(*transition_cache, result.start_state)) {
            if (row_index >= priced_rows.size()) continue;
            const std::uint32_t operator_index =
                priced_rows[row_index].operator_index;
            if (operator_index >= calc.operators().size()) continue;
            const PlannerOperator& planner =
                calc.operators()[operator_index];
            if (planner.kind != PlannerOperatorKind::Primitive ||
                planner.primitive_action >= exact_action_lower.size()) {
                continue;
            }
            exact_action_lower[planner.primitive_action] = std::min(
                exact_action_lower[planner.primitive_action],
                exact_row_lower(row_index));
        }
    }

    std::vector<std::uint32_t> actions =
        carrier_unproved_first_step_actions;
    actions.reserve(
        actions.size() + carrier_priced_first_step_actions.size());
    for (const auto& [action, unused_cost] :
         carrier_priced_first_step_actions) {
        (void)unused_cost;
        actions.push_back(action);
    }
    std::sort(actions.begin(), actions.end());
    actions.erase(std::unique(actions.begin(), actions.end()), actions.end());

    struct Minimum {
        double lower = kInfinity;
        std::uint32_t action = kNoId;
        bool exact = false;
    };
    const std::size_t action_count = calc.registry().actions.size();
    const std::size_t mask_count = goal_cover_cost.size();
    const auto carrier_action_floor = [&] (
        const std::uint8_t rarity,
        const std::uint32_t mask,
        const std::uint32_t action) {
        constexpr std::size_t kCarrierRarityCount = 3;
        if (!carrier_projection || rarity >= kCarrierRarityCount ||
            mask >= mask_count || action >= action_count ||
            carrier_goal_action_floor.size() !=
                kCarrierRarityCount * mask_count * action_count) {
            return kInfinity;
        }
        const std::size_t state =
            static_cast<std::size_t>(rarity) * mask_count + mask;
        return carrier_goal_action_floor[state * action_count + action];
    };
    const auto bounded_gain_floor = [&] (
        const std::uint8_t rarity,
        const std::uint32_t mask,
        const std::uint32_t action) {
        constexpr std::size_t kCarrierRarityCount = 3;
        const std::uint32_t required =
            calc.goal().required_satisfied_slots();
        const std::size_t gain_count =
            static_cast<std::size_t>(required) + 1;
        if (!bounded_gain_projection ||
            rarity >= kCarrierRarityCount || action >= action_count ||
            bounded_gain_action_floor.size() !=
                kCarrierRarityCount * gain_count * action_count) {
            return kInfinity;
        }
        const std::uint32_t progress = std::min(
            required,
            static_cast<std::uint32_t>(std::popcount(mask)));
        const std::size_t state =
            static_cast<std::size_t>(rarity) * gain_count + progress;
        return bounded_gain_action_floor[
            state * action_count + action];
    };
    const AbstractState& exact_start = calc.state(result.start_state);
    const std::uint32_t exact_start_mask =
        satisfied_goal_mask_for_state(result.start_state);
    const auto analytic_action_floor = [&](const std::uint32_t action) {
        double lower = kInfinity;
        if ((clean_projection || identity_projection) &&
            action < clean_goal_start_action_floor.size() &&
            std::isfinite(clean_goal_start_action_floor[action])) {
            lower = clean_goal_start_action_floor[action];
        }
        const double carrier = carrier_action_floor(
            exact_start.rarity, exact_start_mask, action);
        if (std::isfinite(carrier)) {
            lower = std::isfinite(lower)
                ? std::max(lower, carrier)
                : carrier;
        }
        const double bounded = bounded_gain_floor(
            exact_start.rarity, exact_start_mask, action);
        if (std::isfinite(bounded)) {
            lower = std::isfinite(lower)
                ? std::max(lower, bounded)
                : bounded;
        }
        return lower;
    };
    ProofPatternContract& identity_contract = contract(
        ProofPatternKind::IdentityCleanMdp);
    identity_contract.minimizing_action.clear();
    identity_contract.refinement_trace.clear();
    identity_contract.start_contribution = kInfinity;
    identity_contract.residual = contract(
        ProofPatternKind::CleanMdp).residual;
    identity_contract.solution_sweeps = contract(
        ProofPatternKind::CleanMdp).solution_sweeps;
    identity_contract.converged = contract(
        ProofPatternKind::CleanMdp).converged;
    identity_contract.fallback_reason = identity_projection
        ? "fixed_source_identity"
        : "generic_clean_projection_owns_state";
    if (identity_projection && exact_start.rarity <= PC_RARITY_RARE &&
        exact_start.prefix_count <= 3 && exact_start.suffix_count <= 3) {
        constexpr std::size_t kAffixCountStates = 4;
        const std::size_t index =
            (((static_cast<std::size_t>(exact_start.rarity) * mask_count +
               exact_start_mask) * kAffixCountStates +
              exact_start.prefix_count) * kAffixCountStates +
             exact_start.suffix_count);
        if (index < clean_goal_cover_cost.size()) {
            identity_contract.start_contribution =
                clean_goal_cover_cost[index];
        }
        double minimum = kInfinity;
        for (std::uint32_t action = 0;
             action < clean_goal_start_action_floor.size(); ++action) {
            if (clean_goal_start_action_floor[action] < minimum &&
                action_legal(
                    session, calc.registry().actions[action], exact_start)) {
                minimum = clean_goal_start_action_floor[action];
                identity_contract.minimizing_action =
                    calc.registry().actions[action].id;
            }
        }
    }
    const auto main_successor_pattern_lower = [&] (
        const std::uint32_t state_id) {
        if (state_id >= calc.state_count()) return 0.0;
        const AbstractState& state = calc.state(state_id);
        if (calc.is_goal_state(state)) return 0.0;
        double lower = completion_proof_lower_value(state_id);
        const bool fixed_identity = identity_projection &&
            (state.flags & kProtectionFlags) ==
                (exact_start.flags & kProtectionFlags) &&
            state.fractured_goal_mask ==
                exact_start.fractured_goal_mask &&
            state.fractured_metamod_flags ==
                exact_start.fractured_metamod_flags &&
            state.fractured_junk_counts ==
                exact_start.fractured_junk_counts &&
            state.fractured_crafted_junk_counts ==
                exact_start.fractured_crafted_junk_counts &&
            state.influence_bits == exact_start.influence_bits &&
            state.searing_exarch_tier ==
                exact_start.searing_exarch_tier &&
            state.eater_of_worlds_tier ==
                exact_start.eater_of_worlds_tier;
        if ((clean_goal_cover_eligible(state_id) || fixed_identity) &&
            state.rarity <= PC_RARITY_RARE &&
            state.prefix_count <= 3 && state.suffix_count <= 3) {
            constexpr std::size_t kAffixCountStates = 4;
            const std::uint32_t mask =
                satisfied_goal_mask_for_state(state_id);
            const std::size_t index =
                (((static_cast<std::size_t>(state.rarity) * mask_count +
                   mask) * kAffixCountStates + state.prefix_count) *
                 kAffixCountStates + state.suffix_count);
            if (index < clean_goal_cover_cost.size() &&
                std::isfinite(clean_goal_cover_cost[index])) {
                lower = std::max(lower, clean_goal_cover_cost[index]);
            }
        }
        return lower;
    };
    double exact_operator_envelope = kInfinity;
    std::uint32_t minimizing_operator = kNoId;
    std::uint64_t admitted_start_rows = 0;
    std::uint64_t finite_start_rows = 0;
    std::string incomplete_operator_ids;
    if (result.start_state < transition_cache->state_rows.size()) {
        for (const std::uint64_t row_index :
             state_row_indices(*transition_cache, result.start_state)) {
            if (row_index >= transition_cache->rows.size() ||
                row_index >= priced_rows.size() ||
                !transition_cache->rows[row_index].admitted) {
                continue;
            }
            ++admitted_start_rows;
            const double lower = exact_row_lower_with(
                row_index, main_successor_pattern_lower);
            if (!std::isfinite(lower)) {
                const std::uint32_t operator_index =
                    priced_rows[row_index].operator_index;
                if (operator_index < calc.operators().size()) {
                    if (!incomplete_operator_ids.empty()) {
                        incomplete_operator_ids += ",";
                    }
                    incomplete_operator_ids +=
                        calc.operators()[operator_index].id;
                }
                continue;
            }
            ++finite_start_rows;
            if (lower < exact_operator_envelope) {
                exact_operator_envelope = lower;
                minimizing_operator =
                    priced_rows[row_index].operator_index;
            }
        }
    }
    const bool exact_operator_envelope_complete =
        admitted_start_rows > 0 &&
        finite_start_rows == admitted_start_rows &&
        std::isfinite(exact_operator_envelope);
    pattern.refinement_trace =
        "start_rows=" + std::to_string(admitted_start_rows) +
        ",finite_rows=" + std::to_string(finite_start_rows) +
        ",incomplete=" + incomplete_operator_ids + ";";
    const auto minimum = [&] {
        Minimum selected;
        for (const std::uint32_t action : actions) {
            if (!action_legal(
                    session, calc.registry().actions[action],
                    calc.state(result.start_state))) {
                continue;
            }
            double lower = analytic_action_floor(action);
            if (!std::isfinite(lower)) {
                /* Nonproductive setup and cleanup are already free in both
                 * source relaxations. They cannot undercut the represented
                 * productive decisions as separate envelope rows. */
                continue;
            }
            const bool exact_available =
                action < exact_action_lower.size() &&
                std::isfinite(exact_action_lower[action]);
            if (exact_available) {
                lower = std::max(lower, exact_action_lower[action]);
            }
            lower = std::max(lower, common);
            if (lower < selected.lower) {
                selected = {lower, action, exact_available};
            }
        }
        return selected;
    };

    std::vector<double> action_cost(
        calc.registry().actions.size(), kInfinity);
    for (const std::uint32_t action : carrier_unproved_first_step_actions) {
        if (action < action_cost.size()) action_cost[action] = 0.0;
    }
    for (const auto& [action, cost] : carrier_priced_first_step_actions) {
        if (action < action_cost.size()) action_cost[action] = cost;
    }
    std::vector<std::uint8_t> exact_attempted(
        calc.registry().actions.size(), 0);
    std::unique_ptr<CalcContext> proof_calc;
    std::uint32_t proof_start = kNoId;
    const AbstractState& source = calc.state(result.start_state);
    const auto ensure_proof_calc = [&] {
        if (proof_calc) return true;
        if (!proof_calc) {
            proof_calc = std::make_unique<CalcContext>(
                calc.shared_session(), calc.goal(), calc.registry(),
                calc.candidates(), false, false, false, std::nullopt,
                std::vector<CountObservation>{},
                calc.product_solver_parent());
            pc_item_state start{};
            if (!calc.materialize(result.start_state, start)) {
                proof_calc.reset();
                return false;
            }
            proof_start = proof_calc->intern_item(start);
        }
        return true;
    };
    const auto scratch_goal_mask = [&](const AbstractState& state) {
        std::uint32_t mask = 0;
        for (std::uint32_t slot = 0;
             slot < proof_calc->layout().slots.size(); ++slot) {
            if (state.slot_status[slot] ==
                    static_cast<std::uint8_t>(
                        GoalSlotStatus::Satisfied)) {
                mask |= 1u << slot;
            }
        }
        return mask;
    };
    const auto scratch_carrier_eligible = [&] (
        const AbstractState& state) {
        if (!carrier_projection ||
            state.influence_bits != source.influence_bits) {
            return false;
        }
        for (const std::uint32_t unproved :
             carrier_unproved_first_step_actions) {
            if (unproved < calc.registry().actions.size() &&
                action_legal(
                    session, calc.registry().actions[unproved], state)) {
                return false;
            }
        }
        return true;
    };
    const auto projected_lower = [&](const std::uint32_t state_id) {
        const AbstractState& state = proof_calc->state(state_id);
        if (proof_calc->is_goal_state(state)) return 0.0;
        const std::uint32_t mask = scratch_goal_mask(state);
        const double universal = optimistic_completion_cost(mask);
        double clean = kInfinity;
        const bool fixed_identity = identity_projection &&
            (state.flags & kProtectionFlags) ==
                (source.flags & kProtectionFlags) &&
            state.fractured_goal_mask == source.fractured_goal_mask &&
            state.fractured_metamod_flags ==
                source.fractured_metamod_flags &&
            state.fractured_junk_counts ==
                source.fractured_junk_counts &&
            state.fractured_crafted_junk_counts ==
                source.fractured_crafted_junk_counts &&
            state.influence_bits == source.influence_bits &&
            state.searing_exarch_tier == source.searing_exarch_tier &&
            state.eater_of_worlds_tier ==
                source.eater_of_worlds_tier;
        bool clean_identity = fixed_identity || (clean_projection &&
            (state.flags & kProtectionFlags) == 0 &&
            state.fractured_goal_mask == 0 &&
            state.fractured_metamod_flags == 0 &&
            state.influence_bits == source.influence_bits &&
            state.searing_exarch_tier == source.searing_exarch_tier &&
            state.eater_of_worlds_tier ==
                source.eater_of_worlds_tier);
        for (const std::uint8_t count : state.fractured_junk_counts) {
            clean_identity = clean_identity && count == 0;
        }
        for (const std::uint8_t count :
             state.fractured_crafted_junk_counts) {
            clean_identity = clean_identity && count == 0;
        }
        if (clean_identity && state.rarity <= PC_RARITY_RARE &&
            state.prefix_count <= 3 && state.suffix_count <= 3) {
            constexpr std::size_t kAffixCountStates = 4;
            const std::size_t index =
                (((static_cast<std::size_t>(state.rarity) * mask_count +
                   mask) * kAffixCountStates + state.prefix_count) *
                 kAffixCountStates + state.suffix_count);
            if (index < clean_goal_cover_cost.size()) {
                clean = clean_goal_cover_cost[index];
            }
        }
        double carrier = kInfinity;
        constexpr std::size_t kCarrierRarityCount = 3;
        if (scratch_carrier_eligible(state) &&
            carrier_goal_progress_cost.size() ==
                kCarrierRarityCount * mask_count &&
            state.rarity < kCarrierRarityCount) {
            const std::size_t index =
                static_cast<std::size_t>(state.rarity) * mask_count + mask;
            carrier = carrier_goal_progress_cost[index];
        }
        double lower = universal;
        if (std::isfinite(clean)) lower = std::max(lower, clean);
        if (std::isfinite(carrier)) lower = std::max(lower, carrier);
        if (scratch_carrier_eligible(state) &&
            bounded_gain_projection) {
            const std::uint32_t required =
                calc.goal().required_satisfied_slots();
            const std::size_t gain_count =
                static_cast<std::size_t>(required) + 1;
            constexpr std::size_t kCarrierRarityCount = 3;
            const std::uint32_t progress = std::min(
                required,
                static_cast<std::uint32_t>(std::popcount(mask)));
            if (bounded_gain_goal_progress_cost.size() ==
                    kCarrierRarityCount * gain_count &&
                state.rarity < kCarrierRarityCount) {
                const std::size_t index =
                    static_cast<std::size_t>(state.rarity) * gain_count +
                    progress;
                lower = std::max(
                    lower, bounded_gain_goal_progress_cost[index]);
            }
        }
        return lower;
    };

    std::map<std::pair<std::uint32_t, std::uint32_t>, double>
        refined_state_cache;
    std::set<std::pair<std::uint32_t, std::uint32_t>>
        active_refinements;
    std::function<double(std::uint32_t, std::uint32_t)>
        refined_state_lower;
    std::function<double(std::uint32_t, std::uint32_t, std::uint32_t)>
        scratch_action_lower;
    scratch_action_lower = [&] (
        const std::uint32_t state,
        const std::uint32_t action,
        const std::uint32_t successor_depth) {
        if (action >= action_cost.size() ||
            !std::isfinite(action_cost[action])) {
            return kInfinity;
        }
        const OutcomeDistribution& distribution = proof_calc->outcomes(
            state, action, options.goal_progress_gated_reforges);
        if (!distribution.supported || !distribution.applicable ||
            !distribution.choice_groups.empty() ||
            !distribution.choice_options.empty()) {
            return kInfinity;
        }
        double constant = action_cost[action];
        double self_probability = 0.0;
        for (const OutcomeEntry& outcome : distribution.entries) {
            if (outcome.state == state) {
                self_probability += outcome.probability;
                continue;
            }
            double lower = projected_lower(outcome.state);
            if (successor_depth > 0) {
                lower = std::max(
                    lower,
                    refined_state_lower(
                        outcome.state, successor_depth - 1));
            }
            if (!std::isfinite(lower) || lower < 0.0) return kInfinity;
            constant += outcome.probability * lower;
        }
        const double denominator = 1.0 - self_probability;
        if (denominator <= 1e-15) return kInfinity;
        const double lower = constant / denominator;
        return std::isfinite(lower) && lower >= 0.0 &&
                lower < kValueCeiling
            ? lower
            : kInfinity;
    };
    refined_state_lower = [&] (
        const std::uint32_t state,
        const std::uint32_t depth) {
        const double base = projected_lower(state);
        if (depth == 0 || !std::isfinite(base)) return base;
        const auto key = std::pair{state, depth};
        const auto cached = refined_state_cache.find(key);
        if (cached != refined_state_cache.end()) return cached->second;
        if (!active_refinements.insert(key).second) return base;

        const AbstractState projected = proof_calc->state(state);
        if (!scratch_carrier_eligible(projected)) {
            active_refinements.erase(key);
            refined_state_cache.emplace(key, base);
            return base;
        }
        const std::uint32_t mask = scratch_goal_mask(projected);
        std::vector<double> local_exact(action_count, kInfinity);
        std::vector<std::uint8_t> attempted(action_count, 0);
        const auto select_local = [&] {
            Minimum local;
            for (const std::uint32_t action : actions) {
                if (action >= action_count ||
                    !action_legal(
                        session, calc.registry().actions[action],
                        projected)) {
                    continue;
                }
                double lower = carrier_action_floor(
                    projected.rarity, mask, action);
                const double bounded = bounded_gain_floor(
                    projected.rarity, mask, action);
                if (std::isfinite(bounded)) {
                    lower = std::isfinite(lower)
                        ? std::max(lower, bounded)
                        : bounded;
                }
                if (!std::isfinite(lower)) continue;
                lower = std::max(lower, base);
                if (std::isfinite(local_exact[action])) {
                    lower = std::max(lower, local_exact[action]);
                }
                if (lower < local.lower) {
                    local = {lower, action,
                             std::isfinite(local_exact[action])};
                }
            }
            return local;
        };
        Minimum selected_local = select_local();
        if (selected_local.action != kNoId) {
            pattern.refinement_trace +=
                "state=" + std::to_string(state) +
                ",depth=" + std::to_string(depth) +
                ",mask=" + std::to_string(mask) +
                ",action=" +
                calc.registry().actions[selected_local.action].id +
                ",analytic=" + finite_json(selected_local.lower) + ";";
        }
        while (selected_local.action != kNoId &&
               !attempted[selected_local.action]) {
            attempted[selected_local.action] = 1;
            const double exact = scratch_action_lower(
                state, selected_local.action, depth);
            pattern.refinement_trace +=
                "exact=" + finite_json(exact) + ";";
            if (!std::isfinite(exact)) break;
            local_exact[selected_local.action] = exact;
            selected_local = select_local();
        }
        const double refined = std::isfinite(selected_local.lower)
            ? std::max(base, selected_local.lower)
            : base;
        active_refinements.erase(key);
        refined_state_cache.emplace(key, refined);
        return refined;
    };
    const auto exact_scratch_lower = [&](const std::uint32_t action) {
        if (!ensure_proof_calc()) return kInfinity;
        constexpr std::uint32_t kSuccessorRefinementDepth = 0;
        return scratch_action_lower(
            proof_start, action, kSuccessorRefinementDepth);
    };

    Minimum selected = minimum();
    std::uint32_t refinement_attempts = 0;
    while (selected.action != kNoId &&
           selected.action < exact_attempted.size() &&
           !exact_attempted[selected.action]) {
        exact_attempted[selected.action] = 1;
        ++refinement_attempts;
        const double exact = exact_scratch_lower(selected.action);
        pattern.refinement_trace +=
            "start_action=" +
            calc.registry().actions[selected.action].id +
            ",exact=" + finite_json(exact) + ";";
        if (!std::isfinite(exact)) break;
        exact_action_lower[selected.action] = std::isfinite(
                exact_action_lower[selected.action])
            ? std::max(exact_action_lower[selected.action], exact)
            : exact;
        selected = minimum();
    }
    double best = common;
    if (std::isfinite(identity_contract.start_contribution)) {
        best = std::max(best, identity_contract.start_contribution);
    }
    const ProofPatternContract& bounded_gain_contract = contract(
        ProofPatternKind::BoundedGainMdp);
    if (std::isfinite(bounded_gain_contract.start_contribution)) {
        best = std::max(
            best, bounded_gain_contract.start_contribution);
    }
    if (exact_operator_envelope_complete) {
        best = std::max(best, exact_operator_envelope);
    }
    if (!std::isfinite(best) || best < 0.0 || best >= kValueCeiling) {
        pattern.fallback_reason = "no_finite_complete_action_floor";
        return envelope_bellman_lower;
    }
    envelope_bellman_lower = best;
    pattern.start_contribution = best;
    pattern.residual = 0.0;
    pattern.solution_sweeps = refinement_attempts + 1;
    pattern.converged = true;
    pattern.fallback_reason = exact_operator_envelope_complete
        ? "complete_materialized_operator_envelope"
        : "incomplete_rows_global_pattern_fallback";
    if (exact_operator_envelope_complete &&
        minimizing_operator < calc.operators().size()) {
        pattern.minimizing_action =
            calc.operators()[minimizing_operator].id;
    } else if (selected.action < calc.registry().actions.size()) {
        pattern.minimizing_action =
            calc.registry().actions[selected.action].id;
    }
    return envelope_bellman_lower;
}

} // namespace solver
} // namespace poecraft
