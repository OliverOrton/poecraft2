#include "solver_solve_types.hpp"
#include "solver_policy_refinement.hpp"

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

solve_detail::CooperativeTask<bool>
SolveWork::Impl::audit_verified_policy_alternative_shadow(
        const BoundedPolicyIncumbent& incumbent) {
    if (!options.verified_policy_alternative_shadow_diagnostic ||
        !carrier_bound_attribution) {
        co_return true;
    }
    using Work = CarrierBoundAttributionWork;
    auto& shadow = carrier_bound_attribution
                       ->verified_policy_alternative_shadow;
    shadow.requested = true;
    shadow.status = "running";
    shadow.failure_reason.clear();
    shadow.resource_cap.clear();
    shadow.comparison_available_wall_ns = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() -
            carrier_bound_attribution->started_at)
            .count());
    shadow.solver_rows_before_comparison =
        transition_cache == nullptr ? 0 : transition_cache->rows.size();
    shadow.ledger_transitions_before_comparison =
        action_envelope_ledger.transition_count();
    shadow.strict_rows_begun_before_comparison =
        result.diagnostics.policy_refinement.selected_rows_begun;
    shadow.strict_alternative_rows_begun_before_comparison =
        result.diagnostics.policy_refinement.alternative_rows_begun;
    const StrategyPolicyEntryCertificate& certificate =
        incumbent.compiled_artifact.continuation_upper.policy_entries;
    shadow.certificate_retained_bytes = certificate.retained_owned_bytes;
    shadow.certificate_transient_bytes =
        certificate.transient_evaluator_bytes;
    shadow.certificate_build_ns = certificate.build_ns;

    const std::uint64_t ledger_before =
        action_envelope_ledger.transition_count();
    const std::uint64_t rows_before =
        transition_cache == nullptr ? 0 : transition_cache->rows.size();
    const std::uint64_t strict_selected_before =
        result.diagnostics.policy_refinement.selected_rows_begun;
    const std::uint64_t strict_alternative_before =
        result.diagnostics.policy_refinement.alternative_rows_begun;
    const double lower_before = result.lower_bound;
    const double upper_before = result.upper_bound;

    const auto key_identity = [&](const refinement::StableKey& key) {
        std::uint64_t identity = 1469598103934665603ULL;
        identity_mix(identity, key.size());
        for (const std::uint64_t word : key) {
            identity_mix(identity, word);
        }
        return identity;
    };
    const auto record_shape = [&] (
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
    const auto sample_precedes = [](
            const Work::VerifiedPolicyAlternativeSample& left,
            const Work::VerifiedPolicyAlternativeSample& right) {
        if (left.absolute_margin != right.absolute_margin) {
            return left.absolute_margin < right.absolute_margin;
        }
        if (left.exact_entry_identity != right.exact_entry_identity) {
            return left.exact_entry_identity < right.exact_entry_identity;
        }
        return left.action_identity < right.action_identity;
    };
    const auto rc_retirement_precedes = [](
            const Work::RetentionCapacityFractureSample& left,
            const Work::RetentionCapacityFractureSample& right) {
        if (left.retirement_margin != right.retirement_margin) {
            return left.retirement_margin > right.retirement_margin;
        }
        if (left.exact_entry_identity != right.exact_entry_identity) {
            return left.exact_entry_identity < right.exact_entry_identity;
        }
        return left.row_identity < right.row_identity;
    };
    std::set<std::pair<std::uint64_t, std::uint64_t>>
        rc_exact_source_actions;
    std::set<std::tuple<
        std::uint32_t, std::uint8_t, std::uint8_t,
        std::uint32_t, std::uint64_t>> rc_source_action_shapes;
    const auto retirement_precedes = [](
            const Work::VerifiedPolicyAlternativeSample& left,
            const Work::VerifiedPolicyAlternativeSample& right) {
        if (left.retirement_margin != right.retirement_margin) {
            return left.retirement_margin > right.retirement_margin;
        }
        if (left.exact_entry_identity != right.exact_entry_identity) {
            return left.exact_entry_identity < right.exact_entry_identity;
        }
        return left.action_identity < right.action_identity;
    };

    auto bridge = refinement::audit_verified_policy_alternative_shadow(
        calc, result, exact_start_item, prices, options,
        incumbent.compiled_artifact,
        executable_continuation_authority_context(),
        [&](const refinement::VerifiedPolicyStrictEntry& entry,
            const refinement::VerifiedPolicyAlternativeAction& action) {
            if (action.selected || !action.caller_authorized ||
                !action.exact_applicable) {
                return;
            }
            const std::size_t family =
                carrier_bound_operator_family(action.coarse_operator);
            const double upper = entry.exact_continuation_upper;
            const double lower = operator_proof_lower_value(
                entry.coarse_state, action.coarse_operator, false);
            const bool finite_lower =
                std::isfinite(lower) && lower >= 0.0 &&
                lower < kValueCeiling;
            if (finite_lower) {
                ++shadow.finite_existing_lowers;
                ++shadow.comparable_alternatives;
                record_shape(
                    shadow.comparable_shapes, entry.coarse_state);
            }
            const double separation = finite_lower
                ? options.epsilon *
                    std::max({1.0, std::abs(upper), std::abs(lower)})
                : 0.0;
            const bool would_retire =
                finite_lower && lower > upper + separation;
            if (would_retire) {
                ++shadow.would_retire;
                ++shadow.would_retire_by_family.at(family);
                record_shape(
                    shadow.would_retire_shapes, entry.coarse_state);
            } else {
                ++shadow.still_competitive;
                ++shadow.still_competitive_by_family.at(family);
            }

            auto& rc = shadow.retention_capacity_fracture;
            if (!action.retention_capacity_fracture.has_value()) {
                ++rc.existing_lower_fallback_actions;
            } else {
                const refinement::RetentionCapacityFractureShadowRow& row =
                    *action.retention_capacity_fracture;
                ++rc.rows_examined;
                rc.attributable_strict_states_created +=
                    row.strict_states_created;
                rc.peak_transient_bytes = std::max(
                    rc.peak_transient_bytes, row.transient_bytes);
                rc.build_ns += row.build_ns;
                if (!row.available()) {
                    ++rc.rows_refused;
                    ++rc.existing_lower_fallback_actions;
                    if (row.status == refinement::
                            RetentionCapacityFractureShadowStatus::
                                IncompleteMass) {
                        ++rc.mass_failures;
                    } else if (row.status == refinement::
                                   RetentionCapacityFractureShadowStatus::
                                       IdentityFailure) {
                        ++rc.identity_failures;
                    }
                } else if (
                    row.exact_entry_identity !=
                            entry.exact_entry_identity ||
                    row.action_identity != action.operator_identity ||
                    row.semantic_identity !=
                        refinement::
                            retention_capacity_fracture_shadow_row_semantic_identity(
                                row) ||
                    std::abs(row.probability_mass - 1.0) > 1e-12) {
                    ++rc.rows_refused;
                    ++rc.identity_failures;
                    ++rc.existing_lower_fallback_actions;
                } else {
                    double bellman_rhs = row.immediate_cost;
                    std::uint32_t zero_fallback_successors = 0;
                    bool valid = true;
                    for (const auto& transition : row.transitions) {
                        double successor_lower = 0.0;
                        if (transition.projected_coarse_state <
                                calc.state_count()) {
                            successor_lower = completion_proof_lower_value(
                                transition.projected_coarse_state);
                            ++rc.projected_successors;
                        } else {
                            ++zero_fallback_successors;
                            ++rc.zero_fallback_successors;
                        }
                        if (!std::isfinite(successor_lower) ||
                            successor_lower < 0.0 ||
                            (transition.terminal &&
                             successor_lower != 0.0) ||
                            !std::isfinite(transition.probability) ||
                            transition.probability <= 0.0) {
                            valid = false;
                            break;
                        }
                        bellman_rhs +=
                            transition.probability * successor_lower;
                    }
                    const double refined = bellman_rhs;
                    /* This action-local state owns exactly one complete row;
                     * its candidate subsolution value is the independently
                     * admissible successor composition on that same row. */
                    const double residual = valid ? 0.0 : kInfinity;
                    rc.maximum_bellman_residual = std::max(
                        rc.maximum_bellman_residual, residual);
                    if (!valid || !std::isfinite(refined) ||
                        refined < 0.0 ||
                        residual > options.epsilon *
                            std::max(1.0, std::abs(refined))) {
                        ++rc.rows_refused;
                        ++rc.bellman_subsolution_failures;
                        ++rc.existing_lower_fallback_actions;
                    } else {
                        ++rc.rows_complete;
                        rc.transitions += row.transitions.size();
                        rc.minimum_refined_lower = std::min(
                            rc.minimum_refined_lower, refined);
                        rc.maximum_refined_lower = std::max(
                            rc.maximum_refined_lower, refined);
                        const double refined_separation =
                            options.epsilon * std::max({
                                1.0, std::abs(refined),
                                finite_lower ? std::abs(lower) : 0.0});
                        const double combined = finite_lower
                            ? std::max(lower, refined)
                            : refined;
                        if (!finite_lower ||
                            refined > lower + refined_separation) {
                            ++rc.strengthened;
                        }
                        const double upper_separation =
                            options.epsilon * std::max({
                                1.0, std::abs(combined),
                                std::abs(upper)});
                        const bool rc_would_retire =
                            combined > upper + upper_separation;
                        if (rc_would_retire) ++rc.would_retire;
                        const std::uint64_t entry_id =
                            key_identity(entry.exact_entry_identity);
                        const std::uint64_t action_id =
                            key_identity(action.operator_identity);
                        rc_exact_source_actions.emplace(
                            entry_id, action_id);
                        rc_source_action_shapes.emplace(
                            row.source_satisfied_goal_mask,
                            row.source_prefix_count,
                            row.source_suffix_count,
                            row.source_blocked_mask,
                            action_id);

                        Work::RetentionCapacityFractureSample rc_sample;
                        rc_sample.exact_entry_identity = entry_id;
                        rc_sample.strict_state_identity =
                            key_identity(entry.strict_state_identity);
                        rc_sample.action_identity = action_id;
                        rc_sample.row_identity =
                            key_identity(row.semantic_identity);
                        rc_sample.state = entry.coarse_state;
                        rc_sample.operator_index = action.coarse_operator;
                        rc_sample.satisfied_goal_mask =
                            row.source_satisfied_goal_mask;
                        rc_sample.prefix_count = row.source_prefix_count;
                        rc_sample.suffix_count = row.source_suffix_count;
                        rc_sample.transition_count = static_cast<std::uint32_t>(
                            row.transitions.size());
                        rc_sample.zero_fallback_successors =
                            zero_fallback_successors;
                        rc_sample.probability_mass = row.probability_mass;
                        rc_sample.fractured_goal_probability =
                            row.fractured_goal_probability;
                        rc_sample.fractured_junk_probability =
                            row.fractured_junk_probability;
                        rc_sample.existing_lower = lower;
                        rc_sample.refined_lower = combined;
                        rc_sample.upper = upper;
                        rc_sample.retirement_margin = combined - upper;
                        auto begin = rc.closest_to_retirement.begin();
                        auto end = begin +
                            rc.closest_to_retirement_count;
                        if (rc.closest_to_retirement_count <
                                Work::kOperatorShadowSampleLimit) {
                            rc.closest_to_retirement[
                                rc.closest_to_retirement_count++] =
                                rc_sample;
                            end = begin +
                                rc.closest_to_retirement_count;
                            std::sort(
                                begin, end, rc_retirement_precedes);
                        } else if (rc_retirement_precedes(
                                       rc_sample, *(end - 1))) {
                            *(end - 1) = rc_sample;
                            std::sort(
                                begin, end, rc_retirement_precedes);
                        }
                    }
                }
            }
            if (!finite_lower) return;

            const AbstractState& state = calc.state(entry.coarse_state);
            Work::VerifiedPolicyAlternativeSample sample;
            sample.exact_entry_identity =
                key_identity(entry.exact_entry_identity);
            sample.strict_state_identity =
                key_identity(entry.strict_state_identity);
            sample.action_identity =
                key_identity(action.operator_identity);
            sample.state = entry.coarse_state;
            sample.operator_index = action.coarse_operator;
            sample.satisfied_goal_mask =
                satisfied_goal_mask_for_state(entry.coarse_state);
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
            sample.lower = lower;
            sample.upper = upper;
            sample.absolute_margin = std::abs(upper - lower);
            sample.retirement_margin = lower - upper;

            if (would_retire) {
                auto begin = shadow.largest_retirement_margins.begin();
                auto end = begin +
                    shadow.largest_retirement_margin_count;
                if (shadow.largest_retirement_margin_count <
                    Work::kOperatorShadowSampleLimit) {
                    shadow.largest_retirement_margins[
                        shadow.largest_retirement_margin_count++] = sample;
                    end = begin +
                        shadow.largest_retirement_margin_count;
                    std::sort(begin, end, retirement_precedes);
                } else if (retirement_precedes(sample, *(end - 1))) {
                    *(end - 1) = sample;
                    std::sort(begin, end, retirement_precedes);
                }
                return;
            }
            auto begin = shadow.closest_competitive.begin();
            auto end = begin + shadow.closest_competitive_count;
            if (shadow.closest_competitive_count <
                Work::kOperatorShadowSampleLimit) {
                shadow.closest_competitive[
                    shadow.closest_competitive_count++] = sample;
                end = begin + shadow.closest_competitive_count;
                std::sort(begin, end, sample_precedes);
            } else if (sample_precedes(sample, *(end - 1))) {
                *(end - 1) = sample;
                std::sort(begin, end, sample_precedes);
            }
        });
    while (!bridge.resume()) {
        co_await solve_detail::CooperativeCheckpoint{
            bridge.retained_bytes()};
    }
    const refinement::VerifiedPolicyAlternativeShadowCensus census =
        bridge.take_result();
    shadow.certificate_identity = census.certificate_identity;
    shadow.decisions_requested = census.decisions_requested;
    shadow.decisions_reached = census.decisions_reached;
    shadow.decisions_refused = census.decisions_refused;
    shadow.entries_examined = census.entries_examined;
    shadow.entries_accepted = census.entries_accepted;
    shadow.entries_refused = census.entries_refused;
    shadow.certificate_entry_status_counts =
        census.certificate_entry_status_counts;
    shadow.binding_or_solve_identity_refusals =
        census.binding_or_solve_identity_refusals;
    shadow.strict_terminal_refusals =
        census.strict_terminal_refusals;
    shadow.strict_coarse_projection_refusals =
        census.strict_coarse_projection_refusals;
    shadow.selected_action_refusals =
        census.selected_action_refusals;
    shadow.vocabulary_actions_examined =
        census.vocabulary_actions_examined;
    shadow.caller_authorized_actions =
        census.caller_authorized_actions;
    shadow.exact_inapplicabilities = census.exact_inapplicabilities;
    shadow.selected_actions = census.selected_actions;
    shadow.alternative_obligations = census.alternative_obligations;
    auto& rc = shadow.retention_capacity_fracture;
    rc.distinct_exact_source_actions = rc_exact_source_actions.size();
    rc.distinct_source_action_shapes = rc_source_action_shapes.size();
    rc.attributable_strict_state_growth_ppm =
        census.entries_accepted == 0
            ? 0
            : static_cast<std::uint64_t>(
                  (static_cast<long double>(
                       rc.attributable_strict_states_created) *
                   1000000.0L) /
                  static_cast<long double>(census.entries_accepted));
    rc.retained_bytes = sizeof(rc) + rc.pattern.capacity() + 1;
    if (rc.rows_examined != census.retention_capacity_rows_examined ||
        rc.rows_complete != census.retention_capacity_rows_complete ||
        rc.rows_refused != census.retention_capacity_rows_refused ||
        rc.transitions != census.retention_capacity_transitions ||
        rc.attributable_strict_states_created !=
            census.retention_capacity_strict_states_created) {
        ++rc.identity_failures;
    }
    shadow.bridge_retained_bytes = census.retained_owned_bytes;
    shadow.bridge_peak_bytes = census.peak_owned_bytes;
    shadow.bridge_build_ns = census.build_ns;
    shadow.lifecycle_mutations = census.lifecycle_mutations;
    shadow.failure_reason = census.failure_reason;
    shadow.resource_cap = census.resource_cap;
    switch (census.status) {
    case refinement::VerifiedPolicyAlternativeShadowStatus::Complete:
        shadow.status = "complete";
        break;
    case refinement::VerifiedPolicyAlternativeShadowStatus::
            IncompleteCertificate:
        shadow.status = "incomplete_certificate";
        break;
    case refinement::VerifiedPolicyAlternativeShadowStatus::IdentityMismatch:
        shadow.status = "identity_mismatch";
        break;
    case refinement::VerifiedPolicyAlternativeShadowStatus::InvalidEntry:
        shadow.status = "invalid_entry";
        break;
    case refinement::VerifiedPolicyAlternativeShadowStatus::ResourceCap:
        shadow.status = "resource_cap";
        break;
    case refinement::VerifiedPolicyAlternativeShadowStatus::AdapterFailure:
        shadow.status = "adapter_failure";
        break;
    }

    shadow.lifecycle_mutations +=
        action_envelope_ledger.transition_count() != ledger_before ? 1 : 0;
    shadow.lifecycle_mutations +=
        (transition_cache == nullptr ? 0 : transition_cache->rows.size()) !=
                rows_before
            ? 1
            : 0;
    shadow.lifecycle_mutations +=
        result.diagnostics.policy_refinement.selected_rows_begun !=
                strict_selected_before
            ? 1
            : 0;
    shadow.lifecycle_mutations +=
        result.diagnostics.policy_refinement.alternative_rows_begun !=
                strict_alternative_before
            ? 1
            : 0;
    shadow.lifecycle_mutations += result.lower_bound != lower_before ? 1 : 0;
    shadow.lifecycle_mutations += result.upper_bound != upper_before ? 1 : 0;
    if (shadow.lifecycle_mutations != 0) {
        shadow.status = "lifecycle_mutation";
        shadow.failure_reason =
            "verified-policy shadow changed ordinary solver authority";
    }
    co_return shadow.status == "complete";
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
