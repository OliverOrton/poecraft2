#include "solver_solve_types.hpp"

namespace poecraft {
namespace solver {

using namespace solve_detail;

namespace {

bool exact_mod_slot_equal(
        const pc_mod_slot& left,
        const pc_mod_slot& right) {
    if (left.mod_id != right.mod_id ||
        left.group_id != right.group_id ||
        left.flags != right.flags ||
        left.roll_count != right.roll_count ||
        left.veiled_option_count != right.veiled_option_count ||
        left.veiled_chosen_mod_id != right.veiled_chosen_mod_id) {
        return false;
    }
    for (std::size_t index = 0; index < PC_MAX_ROLL_VALUES; ++index) {
        if (left.rolls[index] != right.rolls[index]) return false;
    }
    for (std::size_t index = 0; index < PC_MAX_VEILED_OPTIONS; ++index) {
        if (left.veiled_option_mod_ids[index] !=
            right.veiled_option_mod_ids[index]) {
            return false;
        }
    }
    return true;
}

bool exact_item_equal(
        const pc_item_state& left,
        const pc_item_state& right) {
    if (left.rarity != right.rarity ||
        left.quality != right.quality ||
        left.item_flags != right.item_flags ||
        left.prefix_count != right.prefix_count ||
        left.suffix_count != right.suffix_count ||
        left.implicit_count != right.implicit_count ||
        left.enchantment_count != right.enchantment_count ||
        left.generic_influence_bits != right.generic_influence_bits ||
        left.searing_exarch_tier != right.searing_exarch_tier ||
        left.eater_of_worlds_tier != right.eater_of_worlds_tier ||
        left.socket_count != right.socket_count ||
        left.link_mask != right.link_mask) {
        return false;
    }
    for (std::size_t index = 0; index < PC_MAX_PREFIXES; ++index) {
        if (!exact_mod_slot_equal(
                left.prefixes[index], right.prefixes[index])) {
            return false;
        }
    }
    for (std::size_t index = 0; index < PC_MAX_SUFFIXES; ++index) {
        if (!exact_mod_slot_equal(
                left.suffixes[index], right.suffixes[index])) {
            return false;
        }
    }
    for (std::size_t index = 0; index < PC_MAX_IMPLICITS; ++index) {
        if (!exact_mod_slot_equal(
                left.implicits[index], right.implicits[index])) {
            return false;
        }
    }
    for (std::size_t index = 0; index < PC_MAX_ENCHANTS; ++index) {
        if (!exact_mod_slot_equal(
                left.enchantments[index], right.enchantments[index])) {
            return false;
        }
    }
    for (std::size_t index = 0; index < PC_MAX_SOCKETS; ++index) {
        if (left.socket_colors[index] != right.socket_colors[index]) {
            return false;
        }
    }
    return true;
}

} // namespace

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
    if (!carrier_bound_attribution) return;
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
    prior.ledger_transitions_before_comparison =
        action_envelope_ledger.transition_count();
    prior.solver_rows_before_comparison =
        transition_cache == nullptr ? 0 : transition_cache->rows.size();
    prior.comparison_available_wall_ns = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() -
            carrier_bound_attribution->started_at)
            .count());
    const PolicyRefinementTelemetry& refinement =
        result.diagnostics.policy_refinement;
    prior.strict_obligations_examined =
        refinement.alternative_obligations_created;
    prior.strict_rows_begun_before_comparison =
        refinement.selected_rows_begun;
    prior.strict_alternative_rows_begun_before_comparison =
        refinement.alternative_rows_begun;

    const ExecutableContinuationUpperCertificate& bound_certificate =
        incumbent.compiled_artifact.continuation_upper;
    const StrategyContinuationUpperCertificate& certificate =
        bound_certificate.evaluation;
    prior.certificate_requested_members = certificate.requested_members;
    prior.certificate_certified_members = certificate.certified_members;
    prior.certificate_refused_members = certificate.refused_members;
    prior.certificate_represented_states = certificate.represented_states;
    prior.certificate_certified_states = certificate.certified_states;
    prior.certificate_refused_states = certificate.refused_states;
    prior.certificate_maximum_member_multiplicity =
        certificate.maximum_member_multiplicity;
    prior.certificate_maximum_member_value_spread =
        certificate.maximum_member_value_spread;
    prior.certificate_maximum_bellman_residual =
        certificate.maximum_bellman_residual;
    prior.certificate_retained_bytes = certificate.retained_owned_bytes;
    prior.certificate_transient_bytes =
        certificate.transient_evaluator_bytes;
    prior.certificate_build_ns = certificate.build_ns;
    std::uint64_t attached_strategy_digest = 1469598103934665603ULL;
    identity_mix_string(
        attached_strategy_digest,
        incumbent.compiled_artifact.certification_strategy_json);
    prior.reuse_status = validate_executable_continuation_upper_reuse(
        bound_certificate,
        executable_continuation_authority_context(),
        attached_strategy_digest,
        incumbent.compiled_artifact.certification_strategy_json.size(),
        true);
    if (!incumbent.independently_certified ||
        !incumbent.independently_evaluated || !incumbent.proper ||
        !incumbent.executable ||
        prior.reuse_status != ExecutableContinuationReuseStatus::Complete ||
        !certificate.requested ||
        certificate.schema_version !=
            StrategyContinuationUpperCertificate::kSchemaVersion ||
        certificate.evaluator_version !=
            StrategyContinuationUpperCertificate::kEvaluatorVersion ||
        incumbent.goal_identity != goal_identity() ||
        incumbent.economy_identity != economy_identity() ||
        incumbent.action_vocabulary_identity !=
            action_vocabulary_identity() ||
        incumbent.caller_scope_identity != caller_scope_identity() ||
        incumbent.artifact_identity != artifact_identity() ||
        incumbent.compiled_artifact.certification_strategy_json.empty()) {
        if (prior.reuse_status ==
            ExecutableContinuationReuseStatus::Complete) {
            prior.reuse_status =
                ExecutableContinuationReuseStatus::IncompleteCertificate;
        }
        return;
    }

    const auto sample_precedes = [](
            const Work::OperatorShadowSample& left,
            const Work::OperatorShadowSample& right) {
        if (left.absolute_margin != right.absolute_margin) {
            return left.absolute_margin < right.absolute_margin;
        }
        if (left.state != right.state) return left.state < right.state;
        return left.operator_index < right.operator_index;
    };
    const auto retirement_precedes = [](
            const Work::OperatorShadowSample& left,
            const Work::OperatorShadowSample& right) {
        if (left.retirement_margin != right.retirement_margin) {
            return left.retirement_margin > right.retirement_margin;
        }
        if (left.state != right.state) return left.state < right.state;
        return left.operator_index < right.operator_index;
    };
    const auto record_shape = [&](
            Work::CarrierShapeHistogram& histogram,
            const std::uint32_t state_id) {
        const AbstractState& carrier = calc.state(state_id);
        constexpr std::uint32_t kGoalMaskLimit =
            (std::uint32_t{1} << kMaxGoalSlots) - 1;
        const std::uint32_t satisfied =
            satisfied_goal_mask_for_state(state_id) & kGoalMaskLimit;
        const std::uint32_t blocked =
            carrier.blocked_mask & kGoalMaskLimit;
        const std::uint32_t free_prefixes =
            carrier.prefix_count < 3 ? 3 - carrier.prefix_count : 0;
        const std::uint32_t free_suffixes =
            carrier.suffix_count < 3 ? 3 - carrier.suffix_count : 0;
        const std::uint32_t protection =
            ((carrier.flags & kFlagPrefixesLocked) != 0 ? 1u : 0u) |
            ((carrier.flags & kFlagSuffixesLocked) != 0 ? 2u : 0u);
        bool fractured_non_goal = carrier.fractured_metamod_flags != 0;
        for (const std::uint8_t count : carrier.fractured_junk_counts) {
            fractured_non_goal = fractured_non_goal || count != 0;
        }
        for (const std::uint8_t count :
             carrier.fractured_crafted_junk_counts) {
            fractured_non_goal = fractured_non_goal || count != 0;
        }
        const std::uint32_t fractured_goals =
            std::min<std::uint32_t>(
                kMaxGoalSlots,
                std::popcount(carrier.fractured_goal_mask));
        const std::uint32_t fracture_shape = fractured_goals * 4 +
            (fractured_non_goal ? 1u : 0u) +
            (carrier.fractured_metamod_flags != 0 ? 2u : 0u);
        const std::uint32_t occupied =
            carrier.prefix_count + carrier.suffix_count;
        const std::uint32_t satisfied_count = std::popcount(satisfied);
        const std::uint32_t unrelated = std::min<std::uint32_t>(
            Work::kUnrelatedOccupancyCount - 1,
            occupied > satisfied_count
                ? occupied - satisfied_count
                : 0);
        ++histogram.total;
        ++histogram.goal_subset[satisfied];
        ++histogram.side_capacity[free_prefixes * 4 + free_suffixes];
        ++histogram.blocked_mask[blocked];
        ++histogram.protection[protection];
        ++histogram.fracture[fracture_shape];
        ++histogram.unrelated_occupancy[unrelated];
    };
    for (const auto& [unused_key, entry] :
         action_envelope_ledger.entries()) {
        (void)unused_key;
        const bool live = entry.lifecycle == ActionEnvelopeState::Queued;
        if (live) ++prior.live_ledger_entries;
        if (entry.state >= calc.state_count() ||
            entry.operator_index >= calc.operators().size()) {
            continue;
        }
        const auto certified = std::find_if(
            certificate.states.begin(), certificate.states.end(),
            [&](const StrategyContinuationStateUpper& state) {
                return state.represented_state_identity == entry.state;
            });
        if (certified == certificate.states.end() ||
            !certified->available()) {
            ++prior.uncertified_upper_entries;
            continue;
        }
        /* The first production adapter deliberately exposes only the exact
         * requested root. The evaluator proves arbitrary entries, but a
         * coarse representative does not gain consumer authority until a
         * complete physical-member mapping exists. */
        if (entry.state != result.start_state ||
            certified->declared_member_count != 1 ||
            certified->exact_member_identities.size() != 1) {
            ++prior.uncertified_upper_entries;
            continue;
        }
        const auto member = std::find_if(
            certificate.members.begin(), certificate.members.end(),
            [&](const StrategyContinuationMemberResult& value) {
                return value.represented_state_identity == entry.state &&
                    value.exact_member_identity ==
                        certified->exact_member_identities.front();
            });
        if (member == certificate.members.end() || !member->available() ||
            member->exact_entry_identity.empty() ||
            !result.has_exact_start_item ||
            !exact_item_equal(member->item, result.exact_start_item)) {
            ++prior.uncertified_upper_entries;
            continue;
        }
        const double upper = certified->exact_continuation_upper;
        if (!std::isfinite(upper) || upper < 0.0 ||
            upper >= kValueCeiling) {
            ++prior.uncertified_upper_entries;
            continue;
        }
        ++prior.finite_upper_entries;
        if (live) ++prior.live_finite_upper_entries;
        const double lower = operator_proof_lower_value(
            entry.state, entry.operator_index, false);
        if (!std::isfinite(lower) || lower < 0.0 ||
            lower >= kValueCeiling) {
            continue;
        }
        ++prior.finite_lower_entries;
        ++prior.comparable_entries;
        if (live) {
            ++prior.live_finite_lower_entries;
            ++prior.live_comparable_entries;
        }
        record_shape(prior.comparable_shapes, entry.state);
        const std::size_t family =
            carrier_bound_operator_family(entry.operator_index);
        const double separation = options.epsilon *
            std::max({1.0, std::abs(upper), std::abs(lower)});
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
        sample.retirement_margin = lower - upper;

        if (lower > upper + separation) {
            ++prior.would_retire;
            ++prior.would_retire_by_family.at(family);
            if (live) ++prior.live_would_retire;
            record_shape(prior.would_retire_shapes, entry.state);
            auto begin = prior.largest_retirement_margins.begin();
            auto end = begin + prior.largest_retirement_margin_count;
            if (prior.largest_retirement_margin_count <
                Work::kOperatorShadowSampleLimit) {
                prior.largest_retirement_margins[
                    prior.largest_retirement_margin_count++] = sample;
                end = begin + prior.largest_retirement_margin_count;
                std::sort(begin, end, retirement_precedes);
            } else if (retirement_precedes(sample, *(end - 1))) {
                *(end - 1) = sample;
                std::sort(begin, end, retirement_precedes);
            }
            continue;
        }
        ++prior.still_competitive;
        ++prior.still_competitive_by_family.at(family);
        if (live) ++prior.live_still_competitive;

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
