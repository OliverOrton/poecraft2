#include "solver_solve_types.hpp"
#include "solver_policy_refinement.hpp"
#include "solver_policy_refinement_helpers.hpp"

#include <bit>

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

    refinement::VerifiedPolicyBellmanShadowCertificate bellman;
    const auto bellman_started = std::chrono::steady_clock::now();
    bellman.requested = true;
    bellman.authority = executable_continuation_authority_context();
    bellman.strategy_identity_digest = incumbent.compiled_artifact
        .continuation_upper.strategy_identity_digest;
    bellman.strategy_identity_bytes = incumbent.compiled_artifact
        .continuation_upper.strategy_identity_bytes;
    bellman.policy_entry_certificate_identity =
        certificate.semantic_identity;
    bellman.existing_lower_identity = {
        0x65786c6f77657231ull, /* "exlower1" */
        1,
    };
    const auto append_lower_identity = [&](const auto& value) {
        bellman.existing_lower_identity.push_back(value.size());
        bellman.existing_lower_identity.insert(
            bellman.existing_lower_identity.end(),
            value.begin(), value.end());
    };
    append_lower_identity(bellman.authority.goal);
    append_lower_identity(bellman.authority.economy);
    append_lower_identity(bellman.authority.mechanics_artifact);
    append_lower_identity(bellman.authority.caller_scope);
    append_lower_identity(bellman.authority.action_vocabulary);
    append_lower_identity(bellman.authority.terminal_semantics);

    struct PolicyItemLookup {
        const StrategyPolicyEntryResult* entry = nullptr;
        bool ambiguous = false;
    };
    std::vector<const StrategyPolicyEntryResult*> sorted_policy_entries;
    std::uint64_t globally_routable_entries = 0;
    std::uint64_t ambiguous_item_entries = 0;
    for (const StrategyPolicyEntryResult& entry : certificate.entries) {
        if (!entry.globally_routable()) continue;
        ++globally_routable_entries;
        sorted_policy_entries.push_back(&entry);
    }
    std::vector<const StrategyPolicyEntryResult*> policy_by_entry =
        sorted_policy_entries;
    std::sort(
        policy_by_entry.begin(), policy_by_entry.end(),
        [](const StrategyPolicyEntryResult* left,
           const StrategyPolicyEntryResult* right) {
            return left->exact_entry_identity <
                right->exact_entry_identity;
        });
    std::sort(
        sorted_policy_entries.begin(), sorted_policy_entries.end(),
        [](const StrategyPolicyEntryResult* left,
           const StrategyPolicyEntryResult* right) {
            return std::tie(
                       left->exact_item_identity,
                       left->exact_entry_identity) <
                   std::tie(
                       right->exact_item_identity,
                       right->exact_entry_identity);
        });
    std::vector<PolicyItemLookup> policy_by_item;
    policy_by_item.reserve(sorted_policy_entries.size());
    for (std::size_t begin = 0;
         begin < sorted_policy_entries.size();) {
        std::size_t end = begin + 1;
        while (end < sorted_policy_entries.size() &&
               sorted_policy_entries[end]->exact_item_identity ==
                   sorted_policy_entries[begin]->exact_item_identity) {
            ++end;
        }
        const StrategyPolicyEntryResult* prior =
            sorted_policy_entries[begin];
        bool ambiguous = false;
        for (std::size_t index = begin + 1; index < end; ++index) {
            const StrategyPolicyEntryResult* entry =
                sorted_policy_entries[index];
            const bool identical =
                prior->exact_entry_identity == entry->exact_entry_identity &&
                prior->compiled_node_id == entry->compiled_node_id &&
                prior->selected_operator_identity ==
                    entry->selected_operator_identity &&
                std::bit_cast<std::uint64_t>(
                    prior->exact_continuation_upper) ==
                    std::bit_cast<std::uint64_t>(
                        entry->exact_continuation_upper) &&
                std::bit_cast<std::uint64_t>(prior->bellman_residual) ==
                    std::bit_cast<std::uint64_t>(
                        entry->bellman_residual);
            ambiguous = ambiguous || !identical;
        }
        if (ambiguous) ++ambiguous_item_entries;
        policy_by_item.push_back({prior, ambiguous});
        begin = end;
    }
    bellman.transient_bytes = std::max<std::uint64_t>(
        bellman.transient_bytes,
        sorted_policy_entries.capacity() *
                sizeof(const StrategyPolicyEntryResult*) +
            policy_by_entry.capacity() *
                sizeof(const StrategyPolicyEntryResult*) +
            policy_by_item.capacity() * sizeof(PolicyItemLookup));
    std::vector<const StrategyPolicyEntryResult*>{}.swap(
        sorted_policy_entries);
    const auto policy_item_lookup = [&] (
            const refinement::StableKey& key)
            -> const PolicyItemLookup* {
        const auto found = std::lower_bound(
            policy_by_item.begin(), policy_by_item.end(), key,
            [](const PolicyItemLookup& candidate,
               const refinement::StableKey& requested) {
                return candidate.entry->exact_item_identity < requested;
            });
        return found != policy_by_item.end() && found->entry != nullptr &&
                found->entry->exact_item_identity == key
            ? &*found
            : nullptr;
    };
    const auto policy_entry_lookup = [&] (
            const refinement::StableKey& key)
            -> const StrategyPolicyEntryResult* {
        const auto found = std::lower_bound(
            policy_by_entry.begin(), policy_by_entry.end(), key,
            [](const StrategyPolicyEntryResult* candidate,
               const refinement::StableKey& requested) {
                return candidate->exact_entry_identity < requested;
            });
        return found != policy_by_entry.end() && *found != nullptr &&
                (*found)->exact_entry_identity == key
            ? *found
            : nullptr;
    };

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
    const auto evaluate_bellman_constraint = [&] (
            const refinement::VerifiedPolicyStrictEntry& entry,
            const std::uint32_t operator_index,
            const refinement::StableKey& action_identity,
            const std::string& action_id,
            const bool selected_action,
            const double action_cost,
            const double probability_mass,
            const bool exact_row_available,
            const std::uint32_t exact_row_status,
            std::string refusal_reason,
            const bool selected_policy_equality_available,
            const std::uint32_t selection_reasons,
            const double root_expected_visits,
            const std::uint64_t proof_work_proxy,
            std::vector<
                refinement::VerifiedPolicyBellmanTransitionInput>
                transitions) {
        ++bellman.exact_row_work;
        const StrategyPolicyEntryResult* source_lookup =
            policy_entry_lookup(entry.exact_entry_identity);
        const bool source_available = entry.global_policy_entry &&
            source_lookup != nullptr &&
            source_lookup->exact_item_identity ==
                entry.exact_item_identity &&
            source_lookup->compiled_node_id == entry.compiled_node_id &&
            source_lookup->selected_operator_identity ==
                entry.selected_operator_identity &&
            std::bit_cast<std::uint64_t>(
                source_lookup->exact_continuation_upper) ==
                std::bit_cast<std::uint64_t>(
                    entry.exact_continuation_upper) &&
            std::bit_cast<std::uint64_t>(
                source_lookup->bellman_residual) ==
                std::bit_cast<std::uint64_t>(
                    entry.policy_bellman_residual);
        refinement::VerifiedPolicyBellmanConstraintRequest request;
        request.source_entry_identity = entry.exact_entry_identity;
        request.source_item_identity = entry.exact_item_identity;
        request.action_identity = action_identity;
        request.action_id = action_id;
        request.coarse_state = entry.coarse_state;
        request.coarse_operator = operator_index;
        request.action_family = static_cast<std::uint32_t>(
            carrier_bound_operator_family(operator_index));
        request.selected_action = selected_action;
        request.selected_policy_equality_available =
            selected_policy_equality_available;
        request.source_policy_available = source_available;
        request.exact_row_available = exact_row_available;
        request.exact_row_status = exact_row_status;
        request.refusal_reason = std::move(refusal_reason);
        request.source_policy_value = entry.exact_continuation_upper;
        request.source_policy_residual = entry.policy_bellman_residual;
        request.action_cost = action_cost;
        request.probability_mass = probability_mass;
        request.root_expected_visits = root_expected_visits;
        request.proof_work_proxy = proof_work_proxy;
        request.selection_reasons = selection_reasons;
        request.epsilon = options.epsilon;
        request.transitions = std::move(transitions);
        refinement::VerifiedPolicyBellmanConstraint constraint =
            refinement::evaluate_verified_policy_bellman_constraint(
                std::move(request),
                [&](const refinement::StableKey& item) {
                    refinement::VerifiedPolicyPotentialLookupResult result;
                    const PolicyItemLookup* policy =
                        policy_item_lookup(item);
                    if (policy == nullptr) return result;
                    result.ambiguous = policy->ambiguous;
                    if (!policy->ambiguous && policy->entry != nullptr) {
                        result.available = true;
                        result.policy_entry_identity =
                            policy->entry->exact_entry_identity;
                        result.continuation =
                            policy->entry->exact_continuation_upper;
                    }
                    return result;
                },
                [&](const std::uint32_t state) {
                    return state < calc.state_count()
                        ? completion_proof_lower_value(state)
                        : 0.0;
                });
        if (constraint.status != refinement::
                VerifiedPolicyBellmanConstraintStatus::Complete) {
            ++bellman.unresolved_constraints;
            bellman.constraints.push_back(std::move(constraint));
            return;
        }
        if (constraint.boundary_successors == 0) {
            ++bellman.exact_internal_constraints;
        } else {
            ++bellman.boundary_escape_constraints;
        }
        if (constraint.policy_improving) {
            ++bellman.policy_improving_deviations;
        }
        if (constraint.inequality_satisfied &&
            constraint.internal_policy_successors == 0) {
            ++bellman.exact_bellman_closed_constraints;
        } else {
            ++bellman.unresolved_constraints;
        }
        bellman.constraints.push_back(std::move(constraint));
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

    enum : std::uint32_t {
        kBellmanSelectionFractureWitness = 1u << 0,
        kBellmanSelectionClosestMargin = 1u << 1,
        kBellmanSelectionHighestOccupancy = 1u << 2,
        kBellmanSelectionLargestWorkProxy = 1u << 3,
        kBellmanSelectionFanoutProbe = 1u << 4,
        kBellmanSelectionRetainedScourWitness = 1u << 5,
    };
    struct DeviationCandidate {
        refinement::StableKey exact_entry_identity;
        refinement::StableKey action_identity;
        std::uint32_t operator_index = kNoId;
        std::size_t family = 0;
        double absolute_margin = kInfinity;
        double root_expected_visits = 0.0;
        std::uint64_t proof_work_proxy = 0;
        std::uint32_t selection_reasons = 0;
    };
    const auto candidate_identity_precedes = [](
            const DeviationCandidate& left,
            const DeviationCandidate& right) {
        return std::tie(
                   left.exact_entry_identity,
                   left.action_identity,
                   left.operator_index) <
               std::tie(
                   right.exact_entry_identity,
                   right.action_identity,
                   right.operator_index);
    };
    const auto retain_best = [](
            std::vector<DeviationCandidate>& retained,
            DeviationCandidate candidate,
            const std::size_t limit,
            const auto& precedes) {
        retained.push_back(std::move(candidate));
        std::sort(retained.begin(), retained.end(), precedes);
        if (retained.size() > limit) retained.resize(limit);
    };
    std::vector<DeviationCandidate> closest_candidates;
    std::vector<DeviationCandidate> occupancy_candidates;
    std::vector<DeviationCandidate> work_candidates;
    std::array<std::optional<DeviationCandidate>,
               Work::kOperatorFamilyCount> family_probe_candidates{};
    std::optional<DeviationCandidate> retained_scour_candidate;
    /* The retained matched shadow already owns the action-complete
     * 27,021-entry / 671,410-alternative census. Select only the semantic
     * Fracture witnesses plus bounded occupancy/identity probes before
     * strict import, rather than replaying that 49-second census. */
    std::set<refinement::StableKey> bounded_audit_entry_identities;
    std::vector<const StrategyPolicyEntryResult*> routable_entries;
    static constexpr std::array<std::uint64_t, 9>
        kRetainedFractureWitnessEntryDigests{
            15583702542341634243ull,
            6105405624674696466ull,
            2329398688626797832ull,
            391250520169231604ull,
            14407882584035568767ull,
            18425524324468849433ull,
            4994651498045775932ull,
            43846082436442759ull,
            4061487822869723425ull,
        };
    static constexpr std::uint64_t
        kRetainedHighestOccupancyScourEntryDigest =
            10753618678795409314ull;
    static constexpr std::uint64_t
        kRetainedHighestOccupancyScourActionDigest =
            753596084770576979ull;
    for (const StrategyPolicyEntryResult& entry :
         incumbent.compiled_artifact.continuation_upper
             .policy_entries.entries) {
        if (!entry.globally_routable() ||
            entry.coarse_state >= calc.state_count()) {
            continue;
        }
        routable_entries.push_back(&entry);
        const AbstractState& carrier = calc.state(entry.coarse_state);
        const std::uint32_t satisfied =
            satisfied_goal_mask_for_state(entry.coarse_state);
        const std::uint32_t occupied =
            carrier.prefix_count + carrier.suffix_count;
        bool prior_fracture =
            carrier.fractured_goal_mask != 0 ||
            carrier.fractured_metamod_flags != 0;
        for (const std::uint8_t count :
             carrier.fractured_junk_counts) {
            prior_fracture = prior_fracture || count != 0;
        }
        for (const std::uint8_t count :
             carrier.fractured_crafted_junk_counts) {
            prior_fracture = prior_fracture || count != 0;
        }
        if (std::popcount(satisfied) == 5 && occupied == 6 &&
            carrier.prefix_count == PC_MAX_PREFIXES &&
            carrier.suffix_count == PC_MAX_SUFFIXES &&
            !prior_fracture &&
            (carrier.flags & kProtectionFlags) == 0) {
            bounded_audit_entry_identities.insert(
                entry.exact_entry_identity);
        }
        /* Digests are selection accelerators from the retained matched-r2
         * report, never identity authority. A hit merely admits this entry
         * to the bounded audit; the bridge and row consumer still compare
         * the complete exact entry/item/action and request identities. */
        if (std::find(
                kRetainedFractureWitnessEntryDigests.begin(),
                kRetainedFractureWitnessEntryDigests.end(),
                key_identity(entry.exact_entry_identity)) !=
            kRetainedFractureWitnessEntryDigests.end()) {
            bounded_audit_entry_identities.insert(
                entry.exact_entry_identity);
        }
        if (key_identity(entry.exact_entry_identity) ==
            kRetainedHighestOccupancyScourEntryDigest) {
            bounded_audit_entry_identities.insert(
                entry.exact_entry_identity);
        }
    }
    std::sort(
        routable_entries.begin(), routable_entries.end(),
        [](const StrategyPolicyEntryResult* left,
           const StrategyPolicyEntryResult* right) {
            if (left->root_expected_visits !=
                right->root_expected_visits) {
                return left->root_expected_visits >
                    right->root_expected_visits;
            }
            return left->exact_entry_identity <
                right->exact_entry_identity;
        });
    for (std::size_t index = 0;
         index < std::min<std::size_t>(4, routable_entries.size());
         ++index) {
        bounded_audit_entry_identities.insert(
            routable_entries[index]->exact_entry_identity);
    }
    std::sort(
        routable_entries.begin(), routable_entries.end(),
        [](const StrategyPolicyEntryResult* left,
           const StrategyPolicyEntryResult* right) {
            return left->exact_entry_identity <
                right->exact_entry_identity;
        });
    for (std::size_t index = 0;
         index < std::min<std::size_t>(4, routable_entries.size());
         ++index) {
        bounded_audit_entry_identities.insert(
            routable_entries[index]->exact_entry_identity);
    }
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

            const PlannerOperator& candidate_operator =
                calc.operators().at(action.coarse_operator);
            const std::uint64_t proof_work_proxy =
                1u + static_cast<std::uint64_t>(
                    candidate_operator.primitive_program.size());
            if (finite_lower && action.operator_id != "fracture") {
                DeviationCandidate candidate;
                candidate.exact_entry_identity =
                    entry.exact_entry_identity;
                candidate.action_identity = action.operator_identity;
                candidate.operator_index = action.coarse_operator;
                candidate.family = family;
                candidate.absolute_margin = std::abs(upper - lower);
                candidate.root_expected_visits =
                    entry.root_expected_visits;
                candidate.proof_work_proxy = proof_work_proxy;

                if (action.operator_id == "scour" &&
                    key_identity(entry.exact_entry_identity) ==
                        kRetainedHighestOccupancyScourEntryDigest &&
                    key_identity(action.operator_identity) ==
                        kRetainedHighestOccupancyScourActionDigest) {
                    DeviationCandidate retained = candidate;
                    retained.selection_reasons =
                        kBellmanSelectionHighestOccupancy |
                        kBellmanSelectionRetainedScourWitness;
                    retained_scour_candidate = std::move(retained);
                }

                DeviationCandidate closest = candidate;
                closest.selection_reasons =
                    kBellmanSelectionClosestMargin;
                retain_best(
                    closest_candidates, std::move(closest), 2,
                    [&](const DeviationCandidate& left,
                        const DeviationCandidate& right) {
                        if (left.absolute_margin !=
                            right.absolute_margin) {
                            return left.absolute_margin <
                                right.absolute_margin;
                        }
                        return candidate_identity_precedes(left, right);
                    });

                DeviationCandidate occupied = candidate;
                occupied.selection_reasons =
                    kBellmanSelectionHighestOccupancy;
                retain_best(
                    occupancy_candidates, std::move(occupied), 2,
                    [&](const DeviationCandidate& left,
                        const DeviationCandidate& right) {
                        if (left.root_expected_visits !=
                            right.root_expected_visits) {
                            return left.root_expected_visits >
                                right.root_expected_visits;
                        }
                        return candidate_identity_precedes(left, right);
                    });

                DeviationCandidate work = candidate;
                work.selection_reasons =
                    kBellmanSelectionLargestWorkProxy;
                retain_best(
                    work_candidates, std::move(work), 2,
                    [&](const DeviationCandidate& left,
                        const DeviationCandidate& right) {
                        if (left.proof_work_proxy !=
                            right.proof_work_proxy) {
                            return left.proof_work_proxy >
                                right.proof_work_proxy;
                        }
                        return candidate_identity_precedes(left, right);
                    });

                candidate.selection_reasons =
                    kBellmanSelectionFanoutProbe;
                auto& family_probe =
                    family_probe_candidates.at(family);
                if (!family_probe.has_value() ||
                    candidate_identity_precedes(
                        candidate, *family_probe)) {
                    family_probe = std::move(candidate);
                }
            }

            auto& rc = shadow.retention_capacity_fracture;
            if (!action.retention_capacity_fracture.has_value()) {
                ++rc.existing_lower_fallback_actions;
            } else {
                const refinement::RetentionCapacityFractureShadowRow& row =
                    *action.retention_capacity_fracture;
                const bool bellman_row_bound = row.available() &&
                    row.exact_entry_identity ==
                        entry.exact_entry_identity &&
                    row.action_identity == action.operator_identity &&
                    row.semantic_identity ==
                        refinement::
                            retention_capacity_fracture_shadow_row_semantic_identity(
                                row) &&
                    std::abs(row.probability_mass - 1.0) <= 1e-12;
                std::vector<
                    refinement::VerifiedPolicyBellmanTransitionInput>
                    alternative_transitions;
                alternative_transitions.reserve(
                    row.transitions.size());
                for (const auto& transition : row.transitions) {
                    alternative_transitions.push_back({
                        transition.exact_successor_identity,
                        transition.projected_coarse_state,
                        transition.terminal,
                        transition.probability,
                    });
                }
                evaluate_bellman_constraint(
                    entry, action.coarse_operator,
                    action.operator_identity, action.operator_id,
                    false, row.immediate_cost,
                    row.probability_mass, bellman_row_bound,
                    static_cast<std::uint32_t>(row.status), {}, false,
                    kBellmanSelectionFractureWitness,
                    entry.root_expected_visits,
                    proof_work_proxy,
                    std::move(alternative_transitions));
                bellman.strict_states_created +=
                    row.strict_states_created;
                bellman.transient_bytes = std::max(
                    bellman.transient_bytes, row.transient_bytes);
                bellman.build_ns += row.build_ns;

                evaluate_bellman_constraint(
                    entry, entry.selected_operator,
                    entry.selected_operator_identity,
                    calc.operators().at(entry.selected_operator).id,
                    true, 0.0, 0.0, false, 0, {}, true,
                    kBellmanSelectionFractureWitness,
                    entry.root_expected_visits, 0, {});
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
        }, {},
        [&](const StrategyPolicyEntryResult& entry) {
            return bounded_audit_entry_identities.contains(
                entry.exact_entry_identity);
        });
    while (!bridge.resume()) {
        const std::uint64_t bridge_bytes = bridge.retained_bytes();
        co_await solve_detail::CooperativeCheckpoint{
            bridge_bytes >
                    std::numeric_limits<std::uint64_t>::max() -
                        bellman.transient_bytes
                ? std::numeric_limits<std::uint64_t>::max()
                : bridge_bytes + bellman.transient_bytes};
    }
    const refinement::VerifiedPolicyAlternativeShadowCensus census =
        bridge.take_result();
    using CandidateKey = std::pair<
        refinement::StableKey, refinement::StableKey>;
    std::map<CandidateKey, DeviationCandidate> sampled_candidates;
    const auto merge_candidate = [&](DeviationCandidate candidate) {
        CandidateKey key{
            candidate.exact_entry_identity,
            candidate.action_identity};
        const auto found = sampled_candidates.find(key);
        if (found == sampled_candidates.end()) {
            sampled_candidates.emplace(
                std::move(key), std::move(candidate));
            return;
        }
        found->second.selection_reasons |=
            candidate.selection_reasons;
        found->second.absolute_margin = std::min(
            found->second.absolute_margin,
            candidate.absolute_margin);
        found->second.root_expected_visits = std::max(
            found->second.root_expected_visits,
            candidate.root_expected_visits);
        found->second.proof_work_proxy = std::max(
            found->second.proof_work_proxy,
            candidate.proof_work_proxy);
    };
    for (DeviationCandidate& candidate : closest_candidates) {
        merge_candidate(std::move(candidate));
    }
    for (DeviationCandidate& candidate : occupancy_candidates) {
        merge_candidate(std::move(candidate));
    }
    for (DeviationCandidate& candidate : work_candidates) {
        merge_candidate(std::move(candidate));
    }
    for (auto& candidate : family_probe_candidates) {
        if (candidate.has_value()) {
            merge_candidate(std::move(*candidate));
        }
    }
    if (retained_scour_candidate.has_value()) {
        merge_candidate(std::move(*retained_scour_candidate));
    }

    std::optional<refinement::VerifiedPolicyAlternativeShadowCensus>
        sampled_census;
    struct ScourCegarSeed {
        pc_item_state successor{};
        refinement::StableKey source_entry_identity;
        refinement::StableKey action_identity;
        refinement::StableKey successor_item_identity;
        double source_policy_value = kInfinity;
        double root_expected_visits = 0.0;
        double bellman_deficit = kInfinity;
        double boundary_probability_mass = 0.0;
    };
    std::optional<ScourCegarSeed> scour_cegar_seed;
    if (!sampled_candidates.empty()) {
        std::set<refinement::StableKey> sampled_entry_identities;
        for (const auto& [key, unused] : sampled_candidates) {
            (void)unused;
            sampled_entry_identities.insert(key.first);
        }
        auto sampled_bridge =
            refinement::audit_verified_policy_alternative_shadow(
                calc, result, exact_start_item, prices, options,
                incumbent.compiled_artifact,
                executable_continuation_authority_context(),
                [&](const refinement::VerifiedPolicyStrictEntry& entry,
                    const refinement::VerifiedPolicyAlternativeAction&
                        action) {
                    if (!action.exact_action_row.has_value()) return;
                    const CandidateKey key{
                        entry.exact_entry_identity,
                        action.operator_identity};
                    const auto requested = sampled_candidates.find(key);
                    if (requested == sampled_candidates.end()) return;
                    const refinement::VerifiedPolicyExactActionRow& row =
                        *action.exact_action_row;
                    const bool row_bound = row.available() &&
                        row.exact_entry_identity ==
                            entry.exact_entry_identity &&
                        row.operator_identity ==
                            action.operator_identity &&
                        !row.exact_decision_identity.empty() &&
                        std::abs(row.probability_mass - 1.0) <= 1e-12;
                    std::vector<
                        refinement::VerifiedPolicyBellmanTransitionInput>
                        transitions;
                    transitions.reserve(row.transitions.size());
                    for (const auto& transition : row.transitions) {
                        transitions.push_back({
                            transition.exact_item_identity,
                            transition.projected_coarse_state,
                            transition.terminal,
                            transition.probability,
                        });
                    }
                    evaluate_bellman_constraint(
                        entry, action.coarse_operator,
                        action.operator_identity, action.operator_id,
                        false, row.action_cost, row.probability_mass,
                        row_bound, static_cast<std::uint32_t>(row.status),
                        row.refusal_reason, false,
                        requested->second.selection_reasons,
                        requested->second.root_expected_visits,
                        requested->second.proof_work_proxy,
                        std::move(transitions));
                    const PlannerOperator& measured_planner =
                        calc.operators().at(action.coarse_operator);
                    const bool deterministic_scour =
                        measured_planner.kind ==
                            PlannerOperatorKind::Primitive &&
                        measured_planner.primitive_action <
                            calc.registry().actions.size() &&
                        calc.registry().actions.at(
                            measured_planner.primitive_action)
                                .params.type == ActionType::Scour &&
                        (requested->second.selection_reasons &
                         kBellmanSelectionRetainedScourWitness) != 0 &&
                        row_bound && row.transitions.size() == 1 &&
                        !row.transitions.front().terminal &&
                        std::abs(
                            row.transitions.front().probability - 1.0) <=
                            1e-12;
                    if (deterministic_scour &&
                        (!scour_cegar_seed.has_value() ||
                         requested->second.root_expected_visits >
                             scour_cegar_seed->root_expected_visits)) {
                        ScourCegarSeed seed;
                        seed.successor =
                            row.transitions.front().exact_item;
                        seed.source_entry_identity =
                            entry.exact_entry_identity;
                        seed.action_identity = action.operator_identity;
                        seed.successor_item_identity =
                            row.transitions.front().exact_item_identity;
                        seed.source_policy_value =
                            entry.exact_continuation_upper;
                        seed.root_expected_visits =
                            requested->second.root_expected_visits;
                        if (!bellman.constraints.empty()) {
                            const auto& measured =
                                bellman.constraints.back();
                            seed.bellman_deficit =
                                measured.bellman_deficit;
                            seed.boundary_probability_mass =
                                measured.boundary_probability_mass;
                        }
                        scour_cegar_seed = std::move(seed);
                    }
                    bellman.strict_states_created +=
                        row.strict_states_created;
                    bellman.transient_bytes = std::max(
                        bellman.transient_bytes,
                        row.transient_bytes);
                    bellman.build_ns += row.build_ns;
                },
                [&](const refinement::VerifiedPolicyStrictEntry& entry,
                    const refinement::VerifiedPolicyAlternativeAction&
                        action) {
                    return sampled_candidates.contains({
                        entry.exact_entry_identity,
                        action.operator_identity});
                },
                [&](const StrategyPolicyEntryResult& entry) {
                    return sampled_entry_identities.contains(
                        entry.exact_entry_identity);
                });
        while (!sampled_bridge.resume()) {
            const std::uint64_t bridge_bytes =
                sampled_bridge.retained_bytes();
            co_await solve_detail::CooperativeCheckpoint{
                bridge_bytes >
                        std::numeric_limits<std::uint64_t>::max() -
                            bellman.transient_bytes
                    ? std::numeric_limits<std::uint64_t>::max()
                    : bridge_bytes + bellman.transient_bytes};
        }
        sampled_census = sampled_bridge.take_result();
        bellman.transient_bytes = std::max(
            bellman.transient_bytes,
            sampled_census->sampled_exact_peak_transient_bytes);
    }
    auto& cegar = shadow.policy_potential_cegar;
    cegar.status = "not_selected";
    if (scour_cegar_seed.has_value()) {
        const ScourCegarSeed& seed = *scour_cegar_seed;
        cegar.status = "evaluating_arbitrary_entry";
        cegar.source_entry_identity =
            key_identity(seed.source_entry_identity);
        cegar.action_identity = key_identity(seed.action_identity);
        cegar.successor_item_identity =
            key_identity(seed.successor_item_identity);
        cegar.source_policy_value = seed.source_policy_value;
        cegar.source_root_expected_visits = seed.root_expected_visits;
        cegar.source_bellman_deficit = seed.bellman_deficit;
        cegar.source_boundary_probability_mass =
            seed.boundary_probability_mass;
        const auto cegar_started = std::chrono::steady_clock::now();
        std::shared_ptr<StrategyImpl> parsed_strategy;
        std::shared_ptr<EconomyImpl> cegar_economy;
        std::unique_ptr<StrategyEvalWork> cegar_work;
        constexpr std::uint64_t kCegarEntryIdentity =
            0x636567617273636full; /* "cegarsco" */
        try {
            const std::shared_ptr<const SessionImpl> session =
                refinement::borrow_session(calc);
            parsed_strategy = compile_strategy_json(
                session,
                incumbent.compiled_artifact.certification_strategy_json
                    .data(),
                incumbent.compiled_artifact.certification_strategy_json
                    .size());
            cegar_economy = std::make_shared<EconomyImpl>();
            cegar_economy->id =
                "policy-potential-cegar-arbitrary-entry";
            cegar_economy->prices = prices;
            const std::uint64_t retained_now =
                audited_estimated_owned_bytes();
            const std::uint64_t parsed_bytes =
                refinement::strategy_impl_owned_bytes(*parsed_strategy);
            const std::uint64_t economy_bytes = refinement::economy_owned_bytes(
                cegar_economy->prices,
                cegar_economy->id.capacity());
            std::uint64_t reserved = retained_now;
            refinement::saturating_add(reserved, parsed_bytes);
            refinement::saturating_add(reserved, economy_bytes);
            refinement::saturating_add(
                reserved, bellman.transient_bytes);
            if (reserved >= options.max_solver_owned_bytes) {
                cegar.status = "resource_cap";
                cegar.failure_reason =
                    "no solver-owned memory remains for the arbitrary-entry "
                    "policy evaluator";
            } else {
                StrategyEvalOptions evaluation_options;
                evaluation_options.epsilon = 1e-12;
                evaluation_options.max_sweeps =
                    std::max<std::uint32_t>(1, options.max_sweeps);
                evaluation_options.max_states =
                    std::max<std::uint32_t>(
                        1, options.max_discovered_states);
                evaluation_options.max_pairs =
                    static_cast<std::uint32_t>(std::max<std::uint64_t>(
                        1, std::min<std::uint64_t>(
                            options.max_state_action_rows,
                            std::numeric_limits<std::uint32_t>::max())));
                evaluation_options.max_transitions =
                    static_cast<std::uint32_t>(std::max<std::uint64_t>(
                        1, std::min<std::uint64_t>(
                            options.max_transitions,
                            std::numeric_limits<std::uint32_t>::max())));
                evaluation_options.max_owned_bytes =
                    options.max_solver_owned_bytes - reserved;
                evaluation_options.max_output_json_bytes =
                    options.max_strategy_json_bytes;
                evaluation_options.max_reforge_work =
                    options.max_reforge_work;
                evaluation_options.economy = cegar_economy;
                evaluation_options.continuation_entries.push_back({
                    kCegarEntryIdentity, 1, 1, seed.successor, true});
                evaluation_options.policy_decision_entries.reserve(
                    incumbent.compiled_artifact
                        .policy_decision_bindings.size());
                for (const CompiledPolicyDecisionBinding& binding :
                     incumbent.compiled_artifact
                         .policy_decision_bindings) {
                    evaluation_options.policy_decision_entries.push_back({
                        binding.compiled_node_id,
                        binding.coarse_state,
                        binding.selected_operator,
                        binding.coarse_state_identity,
                        binding.selected_operator_identity,
                        binding.fixed_observed_choice_policy,
                    });
                }
                cegar_work = std::make_unique<StrategyEvalWork>(
                    parsed_strategy, evaluation_options);
            }
        } catch (const std::exception& error) {
            cegar.status = "setup_refused";
            cegar.failure_reason = error.what();
        }
        bool evaluation_failed = false;
        while (cegar_work != nullptr &&
               !cegar_work->progress().done) {
            try {
                cegar_work->step(64);
            } catch (const std::exception& error) {
                cegar.status = "evaluation_refused";
                cegar.failure_reason = error.what();
                evaluation_failed = true;
            }
            cegar.evaluator_live_bytes = std::max(
                cegar.evaluator_live_bytes,
                cegar_work->live_owned_bytes());
            cegar.evaluator_peak_bytes = std::max(
                cegar.evaluator_peak_bytes,
                cegar_work->peak_owned_bytes());
            if (evaluation_failed || cegar_work->progress().done) break;
            co_await solve_detail::CooperativeCheckpoint{
                cegar_work->live_owned_bytes()};
        }
        if (cegar_work != nullptr && !evaluation_failed &&
            cegar_work->progress().done) {
            StrategyEvalResult expansion = cegar_work->take_result();
            const StrategyContinuationMemberResult* continuation = nullptr;
            for (const StrategyContinuationMemberResult& member :
                 expansion.continuation_upper.members) {
                if (member.represented_state_identity ==
                    kCegarEntryIdentity) {
                    continuation = &member;
                    break;
                }
            }
            if (continuation != nullptr) {
                cegar.continuation_status =
                    static_cast<std::uint32_t>(continuation->status);
                cegar.continuation_status_name =
                    strategy_continuation_entry_status_name(
                        continuation->status);
                cegar.continuation_exact_entry_identity =
                    key_identity(continuation->exact_entry_identity);
                cegar.continuation_exact_item_identity =
                    key_identity(exact_item_state_key(
                        continuation->item));
                cegar.continuation_cost =
                    continuation->exact_continuation_upper;
                cegar.continuation_residual =
                    continuation->bellman_residual;
            }
            const StrategyPolicyEntryCertificate& expanded_entries =
                expansion.policy_entries;
            cegar.dependency_certificate_identity =
                expanded_entries.semantic_identity;
            cegar.dependency_kernel_roots_requested =
                expanded_entries.dependency_kernel_roots_requested;
            cegar.dependency_kernels_complete =
                expanded_entries.dependency_kernels_complete;
            cegar.dependency_kernels_refused =
                expanded_entries.dependency_kernels_refused;
            cegar.dependency_expansion_capped =
                expanded_entries.dependency_expansion_capped;
            cegar.selected_dependency_entries =
                expanded_entries.selected_kernels.size();
            std::vector<refinement::PolicyPotentialCandidateEntry>
                candidate_inputs;
            candidate_inputs.reserve(
                expanded_entries.selected_kernels.size());
            const refinement::StableKey strategy_identity{
                incumbent.compiled_artifact.continuation_upper
                    .strategy_identity_digest,
                incumbent.compiled_artifact.continuation_upper
                    .strategy_identity_bytes,
                expanded_entries.semantic_identity,
            };
            for (const StrategyPolicySelectedKernel& kernel :
                 expanded_entries.selected_kernels) {
                cegar.selected_dependency_transitions +=
                    kernel.transitions.size();
                cegar.selected_mandatory_operation_states +=
                    kernel.mandatory_operation_states;
                cegar.selected_route_states += kernel.route_states;
                cegar.selected_checkpoint_states +=
                    kernel.checkpoint_states;
                cegar.selected_observed_choice_states +=
                    kernel.observed_choice_states;
                if (std::isfinite(kernel.bellman_residual)) {
                    cegar.maximum_selected_kernel_residual = std::max(
                        cegar.maximum_selected_kernel_residual,
                        kernel.bellman_residual);
                }
                if (cegar.global_route_target.empty() &&
                    !kernel.compiled_node_id.empty()) {
                    cegar.global_route_target = kernel.compiled_node_id;
                }
                if (!kernel.available()) continue;
                refinement::PolicyPotentialCandidateEntry candidate;
                candidate.identity.authority =
                    executable_continuation_authority_context();
                candidate.identity.strategy = strategy_identity;
                candidate.identity.exact_entry =
                    kernel.source_entry_identity;
                candidate.identity.exact_item =
                    kernel.source_item_identity;
                candidate.policy_value = kernel.source_policy_value;
                candidate.existing_lower = 0.0;
                /* The selected expansion deliberately does not pretend that
                 * caller-action enumeration has happened. The typed SCC
                 * validator must therefore refuse lower certification while
                 * still reporting the exact dependency graph. */
                candidate.caller_authorized_actions = 0;
                candidate.selected_kernel.status = refinement::
                    PolicyPotentialSelectedKernelStatus::Complete;
                candidate.selected_kernel.source = candidate.identity;
                candidate.selected_kernel.selected_operator_identity =
                    kernel.selected_operator_identity;
                candidate.selected_kernel.semantic_identity =
                    kernel.semantic_identity;
                candidate.selected_kernel.mandatory_expected_cost =
                    kernel.mandatory_expected_cost;
                candidate.selected_kernel.probability_mass =
                    kernel.probability_mass;
                candidate.selected_kernel.bellman_residual =
                    kernel.bellman_residual;
                candidate.selected_kernel.mandatory_operation_states =
                    kernel.mandatory_operation_states;
                candidate.selected_kernel.route_states =
                    kernel.route_states;
                candidate.selected_kernel.checkpoint_states =
                    kernel.checkpoint_states;
                candidate.selected_kernel.observed_choice_states =
                    kernel.observed_choice_states;
                for (const auto& transition : kernel.transitions) {
                    candidate.selected_kernel.transitions.push_back({
                        transition.exact_entry_identity,
                        transition.probability,
                        transition.terminal,
                    });
                }
                candidate_inputs.push_back(std::move(candidate));
            }
            const refinement::PolicyPotentialCegarShadowCertificate
                closure =
                    refinement::certify_policy_potential_cegar_shadow(
                        executable_continuation_authority_context(),
                        strategy_identity, std::move(candidate_inputs));
            cegar.candidate_entries = closure.candidates.size();
            cegar.candidate_sccs = closure.dependency_sccs;
            cegar.certified_entries = closure.certified_entries;
            cegar.certified_sccs = closure.certified_sccs;
            cegar.action_constraints_examined =
                closure.constraints_examined;
            cegar.actions_closed_by_existing_lower =
                closure.constraints_closed_by_existing_lower;
            cegar.exact_rows_built = closure.exact_rows_examined;
            cegar.transient_bytes = std::max({
                cegar.transient_bytes,
                expansion.peak_owned_bytes_estimate,
                closure.transient_bytes});
            if (continuation == nullptr || !continuation->available()) {
                cegar.status = "continuation_refused";
                if (cegar.failure_reason.empty()) {
                    cegar.failure_reason = "Scour successor arbitrary-entry "
                        "continuation refused: " +
                        cegar.continuation_status_name;
                }
            } else if (expanded_entries.dependency_kernels_refused != 0) {
                cegar.status = "selected_kernel_refused";
                if (cegar.failure_reason.empty()) {
                    const auto refused = std::find_if(
                        expanded_entries.selected_kernels.begin(),
                        expanded_entries.selected_kernels.end(),
                        [](const StrategyPolicySelectedKernel& kernel) {
                            return !kernel.available();
                        });
                    if (refused != expanded_entries.selected_kernels.end()) {
                        cegar.failure_reason = refused->refusal_reason;
                    }
                }
            } else if (expanded_entries.dependency_expansion_capped) {
                cegar.status = "broad_dependency_expansion";
                cegar.failure_reason =
                    "selected-policy dependency expansion reached its "
                    "fixed diagnostic bound before closure";
            } else {
                cegar.status = "action_census_required";
                cegar.failure_reason =
                    "selected dependencies are exact; caller-action "
                    "constraints remain deliberately unenumerated";
            }
        }
        cegar.evaluator_work_ns = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - cegar_started)
                .count());
        cegar.retained_bytes = sizeof(cegar) +
            cegar.authority.capacity() + 1 +
            cegar.status.capacity() + 1 +
            cegar.failure_reason.capacity() + 1 +
            cegar.continuation_status_name.capacity() + 1 +
            cegar.global_route_target.capacity() + 1;
        cegar.transient_bytes = std::max(
            cegar.transient_bytes, cegar.retained_bytes);
    } else {
        cegar.failure_reason =
            "bounded exact census did not retain the deterministic "
            "highest-occupancy Scour row";
    }
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
    bellman.action_complete = false;
    bellman.dependency_closed = false;
    bellman.build_ns = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - bellman_started)
            .count());
    std::uint64_t bellman_retained = sizeof(bellman) +
        bellman.existing_lower_identity.capacity() *
            sizeof(std::uint64_t) +
        bellman.constraints.capacity() *
            sizeof(refinement::VerifiedPolicyBellmanConstraint);
    for (const refinement::VerifiedPolicyBellmanConstraint& constraint :
         bellman.constraints) {
        bellman_retained +=
            constraint.source_entry_identity.capacity() *
                sizeof(std::uint64_t) +
            constraint.source_item_identity.capacity() *
                sizeof(std::uint64_t) +
            constraint.action_identity.capacity() *
                sizeof(std::uint64_t) +
            constraint.action_id.capacity() + 1 +
            constraint.refusal_reason.capacity() + 1 +
            constraint.successors.capacity() *
                sizeof(
                    refinement::VerifiedPolicyBellmanSuccessorEvidence);
        for (const auto& successor : constraint.successors) {
            bellman_retained +=
                successor.exact_item_identity.capacity() *
                    sizeof(std::uint64_t) +
                successor.policy_entry_identity.capacity() *
                    sizeof(std::uint64_t);
        }
    }
    bellman.retained_bytes = bellman_retained;
    bellman.transient_bytes = std::max(
        bellman.transient_bytes, bellman.retained_bytes);

    auto& policy_potential = shadow.policy_potential_bellman;
    policy_potential.status =
        bellman.constraints.empty()
            ? "no_measured_constraints"
            : bellman.policy_improving_deviations != 0
                  ? "policy_improvement"
                  : "incomplete_action_or_dependency_closure";
    policy_potential.strategy_identity_digest =
        bellman.strategy_identity_digest;
    policy_potential.strategy_identity_bytes =
        bellman.strategy_identity_bytes;
    policy_potential.policy_entry_certificate_identity =
        bellman.policy_entry_certificate_identity;
    policy_potential.existing_lower_identity =
        key_identity(bellman.existing_lower_identity);
    policy_potential.globally_routable_entries =
        globally_routable_entries;
    policy_potential.ambiguous_item_entries =
        ambiguous_item_entries;
    policy_potential.action_complete = bellman.action_complete;
    policy_potential.dependency_closed = bellman.dependency_closed;
    policy_potential.exact_internal_constraints =
        bellman.exact_internal_constraints;
    policy_potential.boundary_escape_constraints =
        bellman.boundary_escape_constraints;
    policy_potential.policy_improving_deviations =
        bellman.policy_improving_deviations;
    policy_potential.exact_bellman_closed_constraints =
        bellman.exact_bellman_closed_constraints;
    policy_potential.unresolved_constraints =
        bellman.unresolved_constraints;
    policy_potential.sampled_rows_requested =
        sampled_candidates.size();
    if (sampled_census.has_value()) {
        policy_potential.sampled_rows_examined =
            sampled_census->sampled_exact_rows_examined;
        policy_potential.sampled_rows_complete =
            sampled_census->sampled_exact_rows_complete;
        policy_potential.sampled_rows_refused =
            sampled_census->sampled_exact_rows_refused;
        policy_potential.sampled_transitions =
            sampled_census->sampled_exact_transitions;
    }
    policy_potential.exact_row_work = bellman.exact_row_work;
    policy_potential.strict_states_created =
        bellman.strict_states_created;
    policy_potential.retained_bytes = bellman.retained_bytes;
    policy_potential.transient_bytes = bellman.transient_bytes;
    policy_potential.build_ns = bellman.build_ns;
    for (const refinement::VerifiedPolicyBellmanConstraint& constraint :
         bellman.constraints) {
        const std::size_t family = std::min<std::size_t>(
            constraint.action_family,
            Work::kOperatorFamilyCount - 1);
        if (!constraint.inequality_satisfied ||
            constraint.internal_policy_successors != 0) {
            ++policy_potential.unresolved_by_family[family];
        }
        if (policy_potential.constraint_count >=
            Work::kOperatorShadowSampleLimit) {
            continue;
        }
        auto& sample = policy_potential.constraints[
            policy_potential.constraint_count++];
        sample.source_entry_identity =
            key_identity(constraint.source_entry_identity);
        sample.source_item_identity =
            key_identity(constraint.source_item_identity);
        sample.action_identity =
            key_identity(constraint.action_identity);
        sample.state = constraint.coarse_state;
        sample.operator_index = constraint.coarse_operator;
        sample.status = static_cast<std::uint32_t>(constraint.status);
        sample.selected_action = constraint.selected_action;
        sample.selected_policy_equality =
            constraint.selected_policy_equality;
        sample.selection_reasons = constraint.selection_reasons;
        sample.root_expected_visits =
            constraint.root_expected_visits;
        sample.proof_work_proxy = constraint.proof_work_proxy;
        sample.exact_row_status = constraint.exact_row_status;
        sample.refusal_reason = constraint.refusal_reason;
        sample.source_policy_value = constraint.source_policy_value;
        sample.source_policy_residual =
            constraint.source_policy_residual;
        sample.action_cost = constraint.action_cost;
        sample.shadow_rhs = constraint.shadow_rhs;
        sample.bellman_deficit = constraint.bellman_deficit;
        sample.exact_policy_deviation =
            constraint.exact_policy_deviation;
        sample.boundary_probability_mass =
            constraint.boundary_probability_mass;
        sample.internal_policy_successors =
            constraint.internal_policy_successors;
        sample.boundary_successors = constraint.boundary_successors;
        sample.exact_deviation_available =
            constraint.exact_deviation_available;
        sample.policy_improving = constraint.policy_improving;
        sample.inequality_satisfied =
            constraint.inequality_satisfied;
        for (const auto& successor : constraint.successors) {
            if (sample.successor_count >=
                Work::kBellmanSuccessorSampleLimit) {
                break;
            }
            auto& successor_sample = sample.successors[
                sample.successor_count++];
            successor_sample.exact_item_identity =
                key_identity(successor.exact_item_identity);
            successor_sample.policy_entry_identity =
                successor.policy_entry_identity.empty()
                    ? 0
                    : key_identity(successor.policy_entry_identity);
            successor_sample.probability = successor.probability;
            successor_sample.policy_continuation =
                successor.policy_continuation;
            successor_sample.existing_lower =
                successor.existing_lower;
            successor_sample.applied_potential =
                successor.applied_potential;
            successor_sample.contribution = successor.contribution;
            successor_sample.required_lower_if_sole_closure =
                successor.required_lower_if_sole_closure;
            successor_sample.terminal = successor.terminal;
            successor_sample.policy_domain = successor.policy_domain;
            successor_sample.ambiguous_policy_entry =
                successor.ambiguous_policy_entry;
        }
    }
    shadow.bridge_retained_bytes = census.retained_owned_bytes;
    shadow.bridge_peak_bytes = census.peak_owned_bytes;
    shadow.bridge_build_ns = census.build_ns;
    shadow.lifecycle_mutations = census.lifecycle_mutations;
    if (sampled_census.has_value()) {
        shadow.bridge_retained_bytes = std::max(
            shadow.bridge_retained_bytes,
            sampled_census->retained_owned_bytes);
        shadow.bridge_peak_bytes = std::max(
            shadow.bridge_peak_bytes,
            sampled_census->peak_owned_bytes);
        shadow.bridge_build_ns += sampled_census->build_ns;
        shadow.lifecycle_mutations +=
            sampled_census->lifecycle_mutations;
    }
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
    cegar.lifecycle_mutations = shadow.lifecycle_mutations;
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
