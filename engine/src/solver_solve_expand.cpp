#include "solver_solve_types.hpp"
#include "solver_sparse_policy.hpp"

namespace poecraft {
namespace solver {

using namespace solve_detail;

namespace solve_detail {

template <typename T>
std::size_t selected_growth_capacity(
    const std::vector<T>& values, const std::size_t additional) {
    if (additional > values.max_size() - values.size()) {
        throw std::length_error("selected solver allocation overflow");
    }
    const std::size_t required = values.size() + additional;
    if (required <= values.capacity()) return values.capacity();
    const std::size_t capacity = values.capacity();
    const std::size_t increment = std::max<std::size_t>(capacity / 4, 64);
    const std::size_t grown =
        capacity > values.max_size() - increment
            ? values.max_size()
            : capacity + increment;
    return std::max(required, grown);
}

AutomaticTelemetryKind automatic_telemetry_kind(
    const PlannerOperator& planner) {
    if (planner.automatic_kind == AutomaticCandidateKind::Veiled) {
        return AutomaticTelemetryKind::Veiled;
    }
    if (planner.kind == PlannerOperatorKind::FixedOption) {
        switch (planner.option_kind) {
        case FixedOptionKind::ImprintRetry:
            return AutomaticTelemetryKind::Imprint;
        case FixedOptionKind::Renewal:
            return AutomaticTelemetryKind::Renewal;
        case FixedOptionKind::ProtectedSide:
        case FixedOptionKind::ProtectedRepeat:
            return AutomaticTelemetryKind::ProtectedSide;
        case FixedOptionKind::TemporaryBenchRepeat:
            return planner.automatic_kind == AutomaticCandidateKind::CannotRoll
                       ? AutomaticTelemetryKind::CannotRoll
                       : AutomaticTelemetryKind::TemporaryBench;
        case FixedOptionKind::FracturePrepare:
            return AutomaticTelemetryKind::FracturePrepare;
        case FixedOptionKind::MultimodFinish:
            return AutomaticTelemetryKind::MultimodFinish;
        case FixedOptionKind::EldritchSideIntent:
            if (planner.automatic_kind ==
                AutomaticCandidateKind::EldritchSide) {
                return AutomaticTelemetryKind::EldritchSide;
            }
            break;
        default:
            break;
        }
    }
    switch (planner.automatic_kind) {
    case AutomaticCandidateKind::PermanentBench:
        return AutomaticTelemetryKind::PermanentBench;
    case AutomaticCandidateKind::Fracture:
        return AutomaticTelemetryKind::PrimitiveFracture;
    case AutomaticCandidateKind::MultimodFinish:
        return AutomaticTelemetryKind::MultimodFinish;
    case AutomaticCandidateKind::Imprint:
        return AutomaticTelemetryKind::Imprint;
    case AutomaticCandidateKind::TemporaryBenchBlocker:
        return AutomaticTelemetryKind::TemporaryBench;
    case AutomaticCandidateKind::ProtectedMetamod:
        return AutomaticTelemetryKind::ProtectedSide;
    case AutomaticCandidateKind::ConstructiveRenewal:
        return AutomaticTelemetryKind::Renewal;
    case AutomaticCandidateKind::EldritchSide:
        return AutomaticTelemetryKind::EldritchSide;
    case AutomaticCandidateKind::CannotRoll:
        return AutomaticTelemetryKind::CannotRoll;
    case AutomaticCandidateKind::Veiled:
        return AutomaticTelemetryKind::Veiled;
    case AutomaticCandidateKind::None:
        break;
    }
    return AutomaticTelemetryKind::None;
}

std::uint32_t compact_count_total(const CompactCountVector& counts) {
    std::uint32_t total = 0;
    for (const std::uint8_t count : counts) total += count;
    return total;
}

CarrierFacts carrier_facts(const AbstractState& state) {
    CarrierFacts facts;
    for (std::uint32_t slot = 0; slot < kMaxGoalSlots; ++slot) {
        if (state.slot_status[slot] !=
            static_cast<std::uint8_t>(GoalSlotStatus::Absent)) {
            facts.goal_family_mask |= 1u << slot;
        }
        if (state.slot_status[slot] ==
            static_cast<std::uint8_t>(GoalSlotStatus::Satisfied)) {
            facts.satisfied_goal_mask |= 1u << slot;
        }
    }
    facts.blocked_mask = state.blocked_mask;
    facts.crafted_goal_mask = state.crafted_goal_mask;
    facts.fractured_goal_mask = state.fractured_goal_mask;
    facts.active_protection = state.flags & kProtectionFlags;
    facts.junk_count = compact_count_total(state.junk_counts);
    facts.crafted_junk_count =
        compact_count_total(state.crafted_junk_counts);
    facts.fractured_junk_count =
        compact_count_total(state.fractured_junk_counts);
    facts.prefix_count = state.prefix_count;
    facts.suffix_count = state.suffix_count;
    facts.rarity = state.rarity;
    facts.state_hash = abstract_state_hash(state);
    return facts;
}

bool session_mods_share_group(
    const SessionImpl& session,
    const std::uint32_t lhs,
    const std::uint32_t rhs) {
    for (std::uint32_t a = session.group_offsets[lhs];
         a < session.group_offsets[lhs + 1]; ++a) {
        for (std::uint32_t b = session.group_offsets[rhs];
             b < session.group_offsets[rhs + 1]; ++b) {
            if (session.group_ids[a] == session.group_ids[b]) return true;
        }
    }
    return false;
}

std::uint32_t session_metamod_flag(
    const SessionImpl& session,
    const std::uint32_t mod) {
    const std::int32_t type = session.metamod_type[mod];
    if (type < 0) return 0;
    const DataImpl& data = *session.data;
    if (type == data.metamod_multimod_code) return kFlagMultimod;
    if (type == data.metamod_no_attack_code) return kFlagNoAttack;
    if (type == data.metamod_no_caster_code) return kFlagNoCaster;
    if (type == data.metamod_prefixes_locked_code) {
        return kFlagPrefixesLocked;
    }
    if (type == data.metamod_suffixes_locked_code) {
        return kFlagSuffixesLocked;
    }
    return 0;
}

void classify_slot_mask(
    const std::uint32_t source,
    const std::uint32_t all_successors,
    const std::uint32_t any_successor,
    std::uint32_t& preserved,
    std::uint32_t& destroyed,
    std::uint32_t& created,
    std::uint32_t& unreachable) {
    preserved = source & all_successors;
    destroyed = source & ~all_successors;
    created = ~source & any_successor;
    unreachable = source & ~any_successor;
}

CarrierSuccessorEnvelope carrier_successor_envelope(
    const CalcContext& calc,
    std::vector<std::uint32_t> successor_ids) {
    std::sort(successor_ids.begin(), successor_ids.end());
    successor_ids.erase(
        std::unique(successor_ids.begin(), successor_ids.end()),
        successor_ids.end());
    if (successor_ids.empty()) {
        throw std::logic_error("carrier successor envelope is empty");
    }
    CarrierSuccessorEnvelope envelope;
    bool first = true;
    for (const std::uint32_t successor_id : successor_ids) {
        const AbstractState& successor = calc.state(successor_id);
        const CarrierFacts facts = carrier_facts(successor);
        envelope.all_goal &= facts.goal_family_mask;
        envelope.any_goal |= facts.goal_family_mask;
        envelope.all_satisfied &= facts.satisfied_goal_mask;
        envelope.any_satisfied |= facts.satisfied_goal_mask;
        envelope.all_fractured &= facts.fractured_goal_mask;
        envelope.any_fractured |= facts.fractured_goal_mask;
        envelope.all_crafted &= facts.crafted_goal_mask;
        envelope.any_crafted |= facts.crafted_goal_mask;
        envelope.all_protection &= facts.active_protection;
        envelope.any_protection |= facts.active_protection;
        envelope.all_blocked &= facts.blocked_mask;
        envelope.any_blocked |= facts.blocked_mask;
        envelope.min_junk_count =
            std::min(envelope.min_junk_count, facts.junk_count);
        envelope.max_junk_count =
            std::max(envelope.max_junk_count, facts.junk_count);
        envelope.min_crafted_junk_count = std::min(
            envelope.min_crafted_junk_count, facts.crafted_junk_count);
        envelope.max_crafted_junk_count = std::max(
            envelope.max_crafted_junk_count, facts.crafted_junk_count);
        envelope.min_fractured_junk_count = std::min(
            envelope.min_fractured_junk_count, facts.fractured_junk_count);
        envelope.max_fractured_junk_count = std::max(
            envelope.max_fractured_junk_count, facts.fractured_junk_count);
        envelope.min_prefix_count =
            std::min(envelope.min_prefix_count, facts.prefix_count);
        envelope.max_prefix_count =
            std::max(envelope.max_prefix_count, facts.prefix_count);
        envelope.min_suffix_count =
            std::min(envelope.min_suffix_count, facts.suffix_count);
        envelope.max_suffix_count =
            std::max(envelope.max_suffix_count, facts.suffix_count);
        if (first) {
            envelope.junk_counts = successor.junk_counts;
            envelope.crafted_junk_counts = successor.crafted_junk_counts;
            envelope.fractured_junk_counts = successor.fractured_junk_counts;
            first = false;
        } else {
            envelope.junk_counts_uniform &=
                envelope.junk_counts == successor.junk_counts;
            envelope.crafted_junk_counts_uniform &=
                envelope.crafted_junk_counts == successor.crafted_junk_counts;
            envelope.fractured_junk_counts_uniform &=
                envelope.fractured_junk_counts ==
                successor.fractured_junk_counts;
        }
    }
    return envelope;
}

CarrierEffectSummary carrier_effect(
    const CalcContext& calc,
    const std::uint32_t source_state,
    const CarrierSuccessorEnvelope& envelope) {
    CarrierEffectSummary effect;
    const AbstractState& source = calc.state(source_state);
    const CarrierFacts source_facts = carrier_facts(source);
    const bool all_junk_exact = envelope.junk_counts_uniform &&
                                envelope.junk_counts == source.junk_counts;
    const bool any_junk_decrease =
        envelope.min_junk_count < source_facts.junk_count;
    const bool any_junk_increase =
        envelope.max_junk_count > source_facts.junk_count;
    const bool all_junk_zero = envelope.max_junk_count == 0;
    const bool all_blockers_exact =
        envelope.all_blocked == source_facts.blocked_mask &&
        envelope.any_blocked == source_facts.blocked_mask;
    const bool any_blocker_lost =
        (source_facts.blocked_mask & ~envelope.all_blocked) != 0;
    const bool any_blocker_created =
        (~source_facts.blocked_mask & envelope.any_blocked) != 0;
    const bool all_blockers_zero = envelope.any_blocked == 0;
    const bool all_crafted_junk_exact =
        envelope.crafted_junk_counts_uniform &&
        envelope.crafted_junk_counts == source.crafted_junk_counts;
    const bool any_crafted_junk_lost =
        envelope.min_crafted_junk_count < source_facts.crafted_junk_count;
    const bool any_crafted_junk_created =
        envelope.max_crafted_junk_count > source_facts.crafted_junk_count;
    const bool all_crafted_junk_zero =
        envelope.max_crafted_junk_count == 0;
    const bool all_fractured_junk_exact =
        envelope.fractured_junk_counts_uniform &&
        envelope.fractured_junk_counts == source.fractured_junk_counts;
    const bool any_fractured_junk_lost =
        envelope.min_fractured_junk_count < source_facts.fractured_junk_count;
    const bool any_fractured_junk_created =
        envelope.max_fractured_junk_count > source_facts.fractured_junk_count;
    const bool all_fractured_junk_zero =
        envelope.max_fractured_junk_count == 0;
    effect.min_prefix_count = envelope.min_prefix_count;
    effect.max_prefix_count = envelope.max_prefix_count;
    effect.min_suffix_count = envelope.min_suffix_count;
    effect.max_suffix_count = envelope.max_suffix_count;

    classify_slot_mask(
        source_facts.goal_family_mask, envelope.all_goal, envelope.any_goal,
        effect.preserved_goal_family_mask,
        effect.destroyed_goal_family_mask,
        effect.created_goal_family_mask,
        effect.unreachable_goal_family_mask);
    classify_slot_mask(
        source_facts.satisfied_goal_mask, envelope.all_satisfied,
        envelope.any_satisfied,
        effect.preserved_satisfied_goal_mask,
        effect.destroyed_satisfied_goal_mask,
        effect.created_satisfied_goal_mask,
        effect.unreachable_satisfied_goal_mask);
    std::uint32_t unused_created = 0;
    std::uint32_t unused_unreachable = 0;
    classify_slot_mask(
        source_facts.fractured_goal_mask, envelope.all_fractured,
        envelope.any_fractured,
        effect.preserved_fractured_goal_mask,
        effect.destroyed_fractured_goal_mask,
        unused_created, unused_unreachable);
    classify_slot_mask(
        source_facts.crafted_goal_mask, envelope.all_crafted,
        envelope.any_crafted,
        effect.preserved_crafted_goal_mask,
        effect.destroyed_crafted_goal_mask,
        unused_created, unused_unreachable);
    classify_slot_mask(
        source_facts.active_protection, envelope.all_protection,
        envelope.any_protection,
        effect.preserved_protection, effect.destroyed_protection,
        unused_created, unused_unreachable);

    const auto set_mask_effect = [&](const std::uint32_t bit,
                                     const std::uint32_t preserved,
                                     const std::uint32_t destroyed,
                                     const std::uint32_t created,
                                     const std::uint32_t unreachable) {
        if (preserved != 0) effect.preserved_properties |= bit;
        if (destroyed != 0) effect.destroyed_properties |= bit;
        if (created != 0) effect.created_properties |= bit;
        if (unreachable != 0) effect.unreachable_properties |= bit;
    };
    set_mask_effect(
        kCarrierGoalFamilies, effect.preserved_goal_family_mask,
        effect.destroyed_goal_family_mask,
        effect.created_goal_family_mask,
        effect.unreachable_goal_family_mask);
    set_mask_effect(
        kCarrierSatisfiedGoalSubset,
        effect.preserved_satisfied_goal_mask,
        effect.destroyed_satisfied_goal_mask,
        effect.created_satisfied_goal_mask,
        effect.unreachable_satisfied_goal_mask);
    if (all_junk_exact && source_facts.junk_count != 0 ||
        all_blockers_exact && source_facts.blocked_mask != 0) {
        effect.preserved_properties |= kCarrierJunkBlockers;
    }
    if (any_junk_decrease || any_blocker_lost) {
        effect.destroyed_properties |= kCarrierJunkBlockers;
    }
    if (any_junk_increase || any_blocker_created) {
        effect.created_properties |= kCarrierJunkBlockers;
    }
    if ((source_facts.junk_count != 0 && all_junk_zero) ||
        (source_facts.blocked_mask != 0 && all_blockers_zero)) {
        effect.unreachable_properties |= kCarrierJunkBlockers;
    }
    if ((effect.preserved_crafted_goal_mask != 0) ||
        (all_crafted_junk_exact && source_facts.crafted_junk_count != 0)) {
        effect.preserved_properties |= kCarrierCraftedState;
    }
    if (effect.destroyed_crafted_goal_mask != 0 || any_crafted_junk_lost) {
        effect.destroyed_properties |= kCarrierCraftedState;
    }
    if (any_crafted_junk_created ||
        (envelope.any_crafted & ~source_facts.crafted_goal_mask) != 0) {
        effect.created_properties |= kCarrierCraftedState;
    }
    if ((source_facts.crafted_goal_mask != 0 &&
         envelope.any_crafted == 0) ||
        (source_facts.crafted_junk_count != 0 && all_crafted_junk_zero)) {
        effect.unreachable_properties |= kCarrierCraftedState;
    }
    if ((effect.preserved_fractured_goal_mask != 0) ||
        (all_fractured_junk_exact && source_facts.fractured_junk_count != 0)) {
        effect.preserved_properties |= kCarrierFracturedState;
    }
    if (effect.destroyed_fractured_goal_mask != 0 || any_fractured_junk_lost) {
        effect.destroyed_properties |= kCarrierFracturedState;
    }
    if (any_fractured_junk_created ||
        (envelope.any_fractured & ~source_facts.fractured_goal_mask) != 0) {
        effect.created_properties |= kCarrierFracturedState;
    }
    if ((source_facts.fractured_goal_mask != 0 &&
         envelope.any_fractured == 0) ||
        (source_facts.fractured_junk_count != 0 && all_fractured_junk_zero)) {
        effect.unreachable_properties |= kCarrierFracturedState;
    }
    const auto side_effect = [&](const std::uint32_t bit,
                                 const std::uint8_t source_count,
                                 const std::uint8_t min_count,
                                 const std::uint8_t max_count) {
        if (source_count != 0 && min_count >= source_count) {
            effect.preserved_properties |= bit;
        }
        if (min_count < source_count) effect.destroyed_properties |= bit;
        if (max_count > source_count) effect.created_properties |= bit;
        if (source_count != 0 && max_count == 0) {
            effect.unreachable_properties |= bit;
        }
    };
    side_effect(
        kCarrierPrefixSide, source_facts.prefix_count,
        effect.min_prefix_count, effect.max_prefix_count);
    side_effect(
        kCarrierSuffixSide, source_facts.suffix_count,
        effect.min_suffix_count, effect.max_suffix_count);
    set_mask_effect(
        kCarrierActiveProtection, effect.preserved_protection,
        effect.destroyed_protection,
        envelope.any_protection & ~source_facts.active_protection,
        source_facts.active_protection & ~envelope.any_protection);
    return effect;
}

CarrierEffectSummary carrier_effect(
    const CalcContext& calc,
    const std::uint32_t source_state,
    std::vector<std::uint32_t> successor_ids) {
    if (successor_ids.empty()) successor_ids.push_back(source_state);
    return carrier_effect(
        calc, source_state,
        carrier_successor_envelope(calc, std::move(successor_ids)));
}

}

bool SolveTransitionCache::compatible(
        const std::uint32_t requested_start,
        const std::vector<PricedOperator>& priced,
        const SolveOptions& options) const {
        if (start_state != requested_start ||
            max_states != options.max_states ||
            max_discovered_states != options.max_discovered_states ||
            max_expanded_states != options.max_expanded_states ||
            max_state_action_rows != options.max_state_action_rows ||
            max_transitions != options.max_transitions ||
            max_reforge_work != options.max_reforge_work ||
            max_solver_owned_bytes != options.max_solver_owned_bytes ||
            max_diagnostic_samples != options.max_diagnostic_samples ||
            full_evidence != options.full_evidence ||
            kernel_reuse != options.kernel_reuse ||
            goal_progress_gated_reforges !=
                options.goal_progress_gated_reforges ||
            consider_imprint_programs !=
                options.consider_imprint_programs ||
            allow_economic_restart != options.allow_economic_restart ||
            exact_quotient == options.strict_states ||
            operator_indices.size() != priced.size()) {
            return false;
        }
        for (std::size_t i = 0; i < priced.size(); ++i) {
            if (operator_indices[i] != priced[i].index) return false;
        }
        return true;
    }

void SolveTransitionCache::retain_automatic_sample(AutomaticCandidateRecord record) {
        owned_automatic_sample_nested_bytes +=
            automatic_sample_nested_bytes(record);
        automatic_candidate_samples.push_back(std::move(record));
    }

void SolveWork::Impl::retain_action_reason(std::string reason) {
        if (result.diagnostics.action_inclusion_reasons.size() <
            options.max_diagnostic_samples) {
            result.diagnostics.action_inclusion_reasons.push_back(
                std::move(reason));
        } else {
            ++result.diagnostics.action_inclusion_reasons_omitted;
        }
    }

SolveTransitionCache::AutomaticCandidateRecord SolveWork::Impl::automatic_record_from(
        const std::uint32_t state,
        const StateLocalAutomaticCandidate& decision) const {
        SolveTransitionCache::AutomaticCandidateRecord record;
        record.state_id = state;
        record.operator_index = decision.operator_index;
        record.candidate_id = decision.id;
        record.candidate_kind = decision.kind;
        record.eligible = decision.evidence.eligible;
        record.collapsed = decision.collapsed;
        record.deferred = decision.deferred;
        record.missing_price = decision.missing_price;
        record.telemetry_kind = decision.telemetry_kind;
        record.template_hit = decision.template_hit;
        record.template_id = decision.template_id;
        record.raw_outcomes = decision.raw_outcomes;
        record.admission_ns = decision.admission_ns;
        record.kernel_evaluation_ns = decision.kernel_evaluation_ns;
        record.outcome_mapping_ns = decision.outcome_mapping_ns;
        record.template_matching_ns = decision.template_matching_ns;
        record.protected_side_evaluations =
            decision.protected_side_evaluations;
        record.protected_repeat_evaluations =
            decision.protected_repeat_evaluations;
        record.protected_retry_checks = decision.protected_retry_checks;
        record.protected_retry_certificates =
            decision.protected_retry_certificates;
        record.protected_retry_fallbacks =
            decision.protected_retry_fallbacks;
        record.protected_attempt_ns = decision.protected_attempt_ns;
        record.protected_baseline_ns = decision.protected_baseline_ns;
        record.protected_normalization_ns =
            decision.protected_normalization_ns;
        record.protected_finish_ns = decision.protected_finish_ns;
        record.selected_bytes = decision.selected_bytes;
        record.evidence = decision.evidence;
        if (decision.operator_index < calc.operators().size()) {
            const PlannerOperator& planner =
                calc.operators().at(decision.operator_index);
            const auto id = [&](const std::uint32_t action) {
                return action == kNoId
                           ? std::string()
                           : calc.registry().actions.at(action).id;
            };
            record.setup_action_id = id(planner.setup_action);
            record.followup_action_id = id(planner.followup_action);
            record.cleanup_action_id = id(planner.cleanup_action);
        }
        return record;
    }

bool SolveWork::Impl::incremental_alternative_type(
        const std::uint32_t operator_index) const {
        if (operator_index >= calc.operators().size()) return false;
        const PlannerOperator& planner =
            calc.operators().at(operator_index);
        if (planner.kind != PlannerOperatorKind::Primitive ||
            planner.primitive_action >= calc.registry().actions.size()) {
            return false;
        }
        switch (calc.registry().actions.at(
                    planner.primitive_action).params.type) {
        case ActionType::Essence:
        case ActionType::Fossil:
        case ActionType::HarvestReforge:
            return true;
        default:
            return false;
        }
    }

bool SolveWork::Impl::prepare_state_expansion(
        const std::uint32_t state,
        const bool include_state_local_automatic) {
        const auto prepare_started = std::chrono::steady_clock::now();
        expansion_operator_indices = static_operator_indices;
        if (options.goal_progress_gated_reforges &&
            calc.state(state).goal_progress_retry_basin != 0) {
            expansion_operator_indices.erase(
                std::remove_if(
                    expansion_operator_indices.begin(),
                    expansion_operator_indices.end(),
                    [&](const std::uint32_t index) {
                        const PlannerOperator& planner =
                            calc.operators().at(index);
                        if (planner.kind !=
                            PlannerOperatorKind::Primitive) {
                            return true;
                        }
                        const ActionType type =
                            calc.registry().actions.at(
                                planner.primitive_action).params.type;
                        return !action_transition_facts(type).renewal ||
                               type == ActionType::EldritchChaos;
                    }),
                expansion_operator_indices.end());
            retain_action_reason(
                "included:zero_progress_retry_basin_destructive_reforges:" +
                std::to_string(expansion_operator_indices.size()));
            result.diagnostics.expansion_prepare_ns +=
                static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now() -
                        prepare_started).count());
            return true;
        }
        expansion_operator_indices.erase(
            std::remove_if(
                expansion_operator_indices.begin(),
                expansion_operator_indices.end(),
                [&](const std::uint32_t index) {
                    const PlannerOperator& planner =
                        calc.operators().at(index);
                    return planner.automatic_kind ==
                               AutomaticCandidateKind::ConstructiveRenewal &&
                           restart_state != kNoId && state != restart_state;
                }),
            expansion_operator_indices.end());
        if (!include_state_local_automatic) {
            /*
             * Incremental mode publishes the exact primitive anchor graph
             * before state-local compound candidates are synthesized. Those
             * candidates are generated later for this carrier and enter the
             * same delayed Q-value lifecycle; skipping them here is a
             * schedule change, not an action-envelope reduction.
             */
            const auto priority = [&](const std::uint32_t index) {
                const PlannerOperator& planner =
                    calc.operators().at(index);
                if (planner.automatic_kind ==
                        AutomaticCandidateKind::PermanentBench ||
                    (planner.kind == PlannerOperatorKind::Primitive &&
                     calc.registry().actions.at(
                         planner.primitive_action).params.type ==
                         ActionType::Bench &&
                     planner_goal_reach_mask(index) != 0)) {
                    /* A deterministic goal finish is a complete executable
                     * start policy. Publish that row before a broad renewal
                     * can exhaust its transient kernel budget, so an honest
                     * resource stop retains the cheapest proved incumbent. */
                    return 0;
                }
                if (planner.kind == PlannerOperatorKind::Primitive &&
                    calc.registry().actions.at(
                        planner.primitive_action).params.type ==
                        ActionType::Chaos) {
                    return 1;
                }
                if (index == restart_operator_index) return 2;
                return 3;
            };
            std::stable_sort(
                expansion_operator_indices.begin(),
                expansion_operator_indices.end(),
                [&](const std::uint32_t left,
                    const std::uint32_t right) {
                    return priority(left) < priority(right);
                });
            result.diagnostics.expansion_prepare_ns +=
                static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now() -
                        prepare_started)
                        .count());
            return true;
        }
        AutomaticAdmissionLimits limits;
        const auto byte_audit_started = std::chrono::steady_clock::now();
        const std::uint64_t calc_bytes =
            calc.fast_estimated_owned_bytes();
        ++owned_byte_ledger_requests;
        const std::uint64_t total_bytes =
            fast_estimated_owned_bytes_with_calc(calc_bytes);
        result.diagnostics.expansion_prepare_byte_audit_ns +=
            static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() - byte_audit_started)
                    .count());
        const std::uint64_t other_bytes =
            total_bytes > calc_bytes ? total_bytes - calc_bytes : 0;
        limits.max_solver_owned_bytes =
            options.max_solver_owned_bytes > other_bytes
                ? options.max_solver_owned_bytes - other_bytes
                : 1;
        limits.max_imprint_program_depth =
            options.max_imprint_program_depth;
        limits.max_imprint_program_work =
            options.max_imprint_program_work;
        limits.consider_imprint_programs =
            options.consider_imprint_programs &&
            !imprint_family_resource_deferred;
        limits.prices = &prices;
        const auto retain_certified_upper = [&](const double upper) {
            if (!std::isfinite(upper) || upper < 0.0 ||
                upper >= kValueCeiling) {
                return;
            }
            const double margin = value_comparison_tolerance(upper);
            const double outward = std::nextafter(
                upper + margin,
                std::numeric_limits<double>::infinity());
            if (std::isfinite(outward)) {
                limits.incumbent_upper_bound = std::min(
                    limits.incumbent_upper_bound, outward);
            }
        };
        if (state < incremental_certified_upper_values.size()) {
            retain_certified_upper(
                incremental_certified_upper_values[state]);
        }
        if (restart_row_allowed(state) &&
            restart_state < incremental_certified_upper_values.size() &&
            std::isfinite(restart_cost)) {
            /* The closed restricted policy remains executable after later
             * actions are admitted. Restart followed by that exact policy is
             * therefore a carrier-local upper even when this carrier itself
             * was outside the restricted policy's proper finite region. */
            retain_certified_upper(
                restart_cost +
                incremental_certified_upper_values[restart_state]);
        }
        if (focused_fallback_policy) {
            const FocusedFallbackPolicy& fallback =
                *focused_fallback_policy;
            const double terminal =
                fallback_terminal_upper(state, fallback);
            retain_certified_upper(
                state == fallback.anchor_state
                    ? fallback.anchor_state_value
                    : std::min(
                          terminal,
                          restart_cost + fallback.anchor_state_value));
        }
        if (output_incumbent.has_value() &&
            output_incumbent->independently_certified &&
            output_incumbent->independently_evaluated &&
            output_incumbent->proper && output_incumbent->executable) {
            const BoundedPolicyIncumbent& incumbent = *output_incumbent;
            if (state < incumbent.values.size() &&
                state < incumbent.policy_reachable.size() &&
                incumbent.policy_reachable[state]) {
                retain_certified_upper(incumbent.values[state]);
            }
            if (restart_row_allowed(state) &&
                restart_state < incumbent.values.size() &&
                restart_state < incumbent.policy_reachable.size() &&
                incumbent.policy_reachable[restart_state] &&
                std::isfinite(restart_cost)) {
                retain_certified_upper(
                    restart_cost + incumbent.values[restart_state]);
            }
        }
        AutomaticAdmissionPhaseTelemetry& phases =
            transition_cache->automatic_admission_phases;
        const CalcTelemetry automatic_work_before = calc.telemetry();
        bool automatic_work_recorded = false;
        const auto record_automatic_work = [&] {
            if (automatic_work_recorded) return;
            automatic_work_recorded = true;
            const CalcTelemetry& after = calc.telemetry();
            const auto add_delta = [](std::uint64_t& target,
                                      const std::uint64_t before,
                                      const std::uint64_t current) {
                const std::uint64_t amount =
                    current >= before ? current - before : current;
                target = amount >
                                 std::numeric_limits<std::uint64_t>::max() -
                                     target
                             ? std::numeric_limits<std::uint64_t>::max()
                             : target + amount;
            };
            add_delta(
                phases.state_action_rows,
                automatic_work_before.state_action_rows,
                after.state_action_rows);
            add_delta(
                phases.transition_entries,
                automatic_work_before.transition_entries,
                after.transition_entries);
            add_delta(
                phases.discovered_states,
                automatic_work_before
                    .automatic_admission_discovered_states,
                after.automatic_admission_discovered_states);
            add_delta(
                phases.reforge_active_work,
                automatic_work_before
                    .automatic_admission_reforge_active_work,
                after.automatic_admission_reforge_active_work);
            add_delta(
                phases.reforge_logical_work_v1,
                automatic_work_before
                    .automatic_admission_reforge_logical_work_v1,
                after.automatic_admission_reforge_logical_work_v1);
        };
        const auto admission_started = std::chrono::steady_clock::now();
        StateLocalAutomaticBatch batch;
        bool admission_complete = false;
        try {
            admission_complete =
                calc.advance_state_local_automatic_candidates(
                    state, limits, batch, 1);
        } catch (...) {
            record_automatic_work();
            result.diagnostics.expansion_prepare_admission_ns +=
                static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now() - admission_started)
                        .count());
            throw;
        }
        record_automatic_work();
        result.diagnostics.expansion_prepare_admission_ns +=
            static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() - admission_started)
                    .count());
        if (!admission_complete) {
            result.diagnostics.expansion_prepare_ns +=
                static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now() - prepare_started)
                        .count());
            return false;
        }
        phases.carriers += batch.phases.carriers;
        phases.synthesis_ns += batch.phases.synthesis_ns;
        phases.local_context_ns += batch.phases.local_context_ns;
        phases.local_planner_build_ns +=
            batch.phases.local_planner_build_ns;
        phases.local_layout_build_ns +=
            batch.phases.local_layout_build_ns;
        phases.local_ledger_init_ns +=
            batch.phases.local_ledger_init_ns;
        phases.local_context_other_ns +=
            batch.phases.local_context_other_ns;
        const auto add_saturated = [](std::uint64_t& target,
                                      const std::uint64_t amount) {
            target = amount >
                             std::numeric_limits<std::uint64_t>::max() -
                                 target
                         ? std::numeric_limits<std::uint64_t>::max()
                         : target + amount;
        };
        add_saturated(
            phases.imprint_programs_evaluated,
            batch.phases.imprint_programs_evaluated);
        add_saturated(
            phases.imprint_programs_pruned,
            batch.phases.imprint_programs_pruned);
        add_saturated(
            phases.imprint_distribution_dominated_programs,
            batch.phases.imprint_distribution_dominated_programs);
        add_saturated(
            phases.imprint_price_pruned_programs,
            batch.phases.imprint_price_pruned_programs);
        phases.imprint_price_bound_max_program_depth = std::max(
            phases.imprint_price_bound_max_program_depth,
            batch.phases.imprint_price_bound_max_program_depth);
        phases.imprint_max_evaluated_depth = std::max(
            phases.imprint_max_evaluated_depth,
            batch.phases.imprint_max_evaluated_depth);
        phases.imprint_max_frontier_size = std::max(
            phases.imprint_max_frontier_size,
            batch.phases.imprint_max_frontier_size);
        add_saturated(
            phases.imprint_price_bound_complete_carriers,
            batch.phases.imprint_price_bound_complete_carriers);
        add_saturated(
            phases.imprint_action_state_evaluations,
            batch.phases.imprint_action_state_evaluations);
        add_saturated(
            phases.imprint_outcomes_merged,
            batch.phases.imprint_outcomes_merged);
        phases.imprint_max_atomic_outcomes_ns = std::max(
            phases.imprint_max_atomic_outcomes_ns,
            batch.phases.imprint_max_atomic_outcomes_ns);
        const auto diagnostics_started = std::chrono::steady_clock::now();
        if (batch.temporary_precompiled_classes != 0 ||
            batch.temporary_precompile_ns != 0 ||
            batch.temporary_candidate_variants != 0 ||
            batch.temporary_effect_classes != 0 ||
            batch.temporary_enumeration_ns != 0) {
            SolveTransitionCache::AutomaticCandidateRecord timing;
            timing.state_id = state;
            timing.telemetry_kind = AutomaticTelemetryKind::TemporaryBench;
            timing.count_candidate = false;
            timing.precompiled_classes =
                batch.temporary_precompiled_classes;
            timing.precompile_ns = batch.temporary_precompile_ns;
            timing.precompiled_bytes = batch.temporary_precompiled_bytes;
            timing.candidate_variants =
                batch.temporary_candidate_variants;
            timing.effect_classes = batch.temporary_effect_classes;
            timing.collapsed_variants =
                batch.temporary_collapsed_variants;
            timing.enumeration_ns = batch.temporary_enumeration_ns;
            retain_automatic_candidate_record(std::move(timing));
        }
        for (std::size_t i = 0;
             i < batch.shared_admission_ns.size(); ++i) {
            if (batch.shared_admission_ns[i] == 0) continue;
            SolveTransitionCache::AutomaticCandidateRecord timing;
            timing.state_id = state;
            timing.telemetry_kind =
                static_cast<AutomaticTelemetryKind>(i);
            timing.count_candidate = false;
            timing.admission_ns = batch.shared_admission_ns[i];
            retain_automatic_candidate_record(std::move(timing));
        }
        for (const StateLocalAutomaticCandidate& decision : batch.decisions) {
            if (decision.missing_price) {
                record_skipped_missing_price(decision.id);
                add_action_reason(
                    "unpriced", decision.id,
                    "missing_one_or_more_resource_prices");
                add_action_reason(
                    "rejected", decision.id,
                    "automatic_candidate_missing_price");
            }
            retain_automatic_candidate_record(
                automatic_record_from(state, decision));
            if (decision.admitted && decision.operator_index != kNoId) {
                automatic_admission_records.insert(
                    (static_cast<std::uint64_t>(state) << 32) |
                    decision.operator_index);
            }
        }
        result.diagnostics.expansion_prepare_diagnostics_ns +=
            static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() - diagnostics_started)
                    .count());
        if (batch.status ==
            StateLocalAutomaticBatchStatus::ResourceDeferred) {
            if (batch.resource_cap.empty()) {
                throw std::logic_error(
                    "resource-deferred automatic admission omitted its cap");
            }
            const auto deferred = std::find_if(
                batch.decisions.begin(), batch.decisions.end(),
                [](const StateLocalAutomaticCandidate& decision) {
                    return decision.deferred;
                });
            add_action_reason(
                "deferred",
                deferred == batch.decisions.end()
                    ? "automatic:state_local_generation"
                    : deferred->id,
                batch.resource_reason);
            /* No row from a truncated state-local action envelope can be
             * exact. Record the unresolved carrier here because admission
             * can also be requested by upper-policy construction before the
             * dynamic-alternative cursor reaches this state. */
            if (incremental_action_generation ||
                options.goal_progress_gated_reforges) {
                incremental_action_generation = true;
                ++incremental_resource_unresolved_actions;
                incremental_envelope_closed = false;
                result.diagnostics.incremental_action_generation = true;
                result.diagnostics.incremental_action_envelope_closed =
                    false;
                result.diagnostics.incremental_actions_unresolved =
                    incremental_resource_unresolved_actions;
            }
            if (options.consider_imprint_programs &&
                (batch.resource_cap == "max_imprint_program_work" ||
                 batch.resource_cap == "max_imprint_program_depth")) {
                /* Imprint is one optional automatic family inside a larger
                 * carrier-local transaction. Its unfinished grammar remains
                 * an exactness obligation, but it must not prevent priced
                 * Eldritch, bench, protection, and other families from being
                 * materialized. The admission transaction above rolled back;
                 * the next cooperative call replays this carrier once with
                 * only Imprint disabled, and later carriers inherit the same
                 * already-exhausted family budget. */
                imprint_family_resource_deferred = true;
                incremental_deferred_resource_cap = batch.resource_cap;
                result.diagnostics.expansion_prepare_ns +=
                    static_cast<std::uint64_t>(
                        std::chrono::duration_cast<std::chrono::nanoseconds>(
                            std::chrono::steady_clock::now() -
                            prepare_started)
                            .count());
                return false;
            }
            expansion_incremental_resource_limited = true;
            throw SolverResourceLimit(
                batch.resource_cap, batch.resource_limit);
        }
        const auto pricing_started = std::chrono::steady_clock::now();
        for (const std::uint32_t index : batch.admitted_operators) {
            if (ensure_priced_operator(index) &&
                std::find(
                    expansion_operator_indices.begin(),
                    expansion_operator_indices.end(), index) ==
                    expansion_operator_indices.end()) {
                expansion_operator_indices.push_back(index);
            }
        }
        if (std::isfinite(limits.incumbent_upper_bound)) {
            const double incumbent = limits.incumbent_upper_bound;
            const std::size_t before = expansion_operator_indices.size();
            expansion_operator_indices.erase(
                std::remove_if(
                    expansion_operator_indices.begin(),
                    expansion_operator_indices.end(),
                    [&](const std::uint32_t index) {
                        const double lower =
                            operator_proof_lower(state, index).value;
                        const double separation = options.epsilon *
                            std::max({1.0, std::abs(incumbent),
                                      std::abs(lower)});
                        const bool prune = std::isfinite(lower) &&
                            lower > incumbent + separation;
                        record_operator_lower_attribution(
                            index, lower, incumbent, prune, false);
                        return prune;
                    }),
                expansion_operator_indices.end());
            const std::size_t pruned =
                before - expansion_operator_indices.size();
            if (pruned != 0) {
                price_bound_state_pruning = true;
                retain_action_reason(
                    "pruned:state_incumbent_operator_lower:" +
                    std::to_string(pruned));
            }
        } else {
            for (const std::uint32_t index : expansion_operator_indices) {
                record_operator_lower_skip(
                    index,
                    CarrierBoundAttributionWork::OperatorLowerSkipReason::
                        NoFiniteIncumbent);
            }
        }
        /* Constructive deterministic goal finishes and Restart are exact
         * upper-bound producers. Evaluate them before broad stochastic
         * kernels so a strict state certificate can avoid discovering an
         * already-dominated fringe. This is only an evaluation schedule;
         * absent a certificate every admitted operator is still evaluated. */
        std::stable_sort(
            expansion_operator_indices.begin(),
            expansion_operator_indices.end(),
            [&](const std::uint32_t left, const std::uint32_t right) {
                const auto priority = [&](const std::uint32_t index) {
                    const PlannerOperator& planner =
                        calc.operators().at(index);
                    if (planner.automatic_kind ==
                            AutomaticCandidateKind::PermanentBench ||
                        (planner.kind == PlannerOperatorKind::Primitive &&
                         calc.registry().actions.at(
                             planner.primitive_action).params.type ==
                             ActionType::Bench &&
                         planner_goal_reach_mask(index) != 0)) {
                        return 0;
                    }
                    if (index == restart_operator_index) return 1;
                    return 2;
                };
                return priority(left) < priority(right);
            });
        result.diagnostics.expansion_prepare_pricing_ns +=
            static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() - pricing_started)
                    .count());
        result.diagnostics.expansion_prepare_ns +=
            static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() - prepare_started)
                    .count());
        return true;
    }

void SolveWork::Impl::enqueue(const std::uint32_t state) {
        if (state >= queued.size()) queued.resize(state + 1, 0);
        if (queued[state]) return;
        queued[state] = 1;
        queue.push_back(state);
        peak_queue_size = std::max<std::uint32_t>(
            peak_queue_size, static_cast<std::uint32_t>(queue.size()));
    }

void SolveWork::Impl::enqueue_front(const std::uint32_t state) {
        if (state >= queued.size()) queued.resize(state + 1, 0);
        if (queued[state]) return;
        queued[state] = 1;
        queue.push_front(state);
        peak_queue_size = std::max<std::uint32_t>(
            peak_queue_size, static_cast<std::uint32_t>(queue.size()));
    }

bool SolveWork::Impl::same_kernel(
        const SparseRow& stored,
        const PendingSparseRow& pending,
        const std::size_t transition_count,
        const double self_probability) const {
        const auto is_self = [&](const std::uint32_t successor) {
            return pending.entry_relative_self ? successor == kNoId
                                               : successor == pending.state;
        };
        const std::size_t choice_count =
            pending.choices == nullptr ? 0 : pending.choices->size();
        if (stored.transition_count != transition_count ||
            stored.choice_count != choice_count ||
            (pending.entry_relative_self &&
             stored.self_probability != self_probability)) {
            return false;
        }
        std::size_t stored_transition = 0;
        if (pending.transitions != nullptr) {
            for (const OutcomeEntry& right : *pending.transitions) {
                if (pending.entry_relative_self && is_self(right.state)) {
                    continue;
                }
                const std::uint64_t offset =
                    stored.transition_offset + stored_transition++;
                if (transition_cache->successors.at(offset) != right.state ||
                    transition_cache->probabilities.at(offset) !=
                        right.probability) {
                    return false;
                }
            }
        }
        for (std::size_t i = 0; i < choice_count; ++i) {
            const SparseChoiceGroup& left = transition_cache->choices.at(
                stored.choice_offset + i);
            const OutcomeChoiceGroup& right = pending.choices->at(i);
            const bool has_self = std::find(
                right.states.begin(), right.states.end(),
                pending.entry_relative_self ? kNoId : pending.state) !=
                right.states.end();
            const std::size_t successor_count =
                right.states.size() - (has_self ? 1u : 0u);
            if (left.probability != right.probability ||
                left.has_self != has_self ||
                left.successor_count != successor_count) {
                return false;
            }
            std::size_t stored_successor = 0;
            for (const std::uint32_t successor : right.states) {
                if (is_self(successor)) continue;
                if (transition_cache->choice_successors.at(
                        left.successor_offset + stored_successor++) !=
                    successor) {
                    return false;
                }
            }
        }
        return true;
    }

std::size_t SolveWork::Impl::kernel_hash(const PendingSparseRow& pending) const {
        const auto mix = [](std::size_t& hash, std::size_t value) {
            hash ^= value + 0x9e3779b97f4a7c15ULL + (hash << 6) +
                    (hash >> 2);
        };
        std::size_t hash = 2166136261u;
        if (pending.transitions != nullptr) {
            for (const OutcomeEntry& entry : *pending.transitions) {
                if (pending.entry_relative_self && entry.state == kNoId) {
                    mix(hash, 0x73656c66u);
                    mix(hash, std::hash<double>{}(entry.probability));
                    continue;
                }
                mix(hash, entry.state);
                mix(hash, std::hash<double>{}(entry.probability));
            }
            mix(hash, pending.transitions->size());
        }
        if (pending.choices != nullptr) {
            mix(hash, pending.choices->size());
            for (const OutcomeChoiceGroup& group : *pending.choices) {
                mix(hash, std::hash<double>{}(group.probability));
                const bool has_self = std::find(
                    group.states.begin(), group.states.end(),
                    pending.entry_relative_self ? kNoId : pending.state) !=
                    group.states.end();
                mix(hash, has_self ? 1u : 0u);
                mix(hash, group.states.size() - (has_self ? 1u : 0u));
                for (const std::uint32_t state : group.states) {
                    if ((pending.entry_relative_self && state == kNoId) ||
                        (!pending.entry_relative_self &&
                         state == pending.state)) {
                        continue;
                    }
                    mix(hash, state);
                }
            }
        }
        return hash;
    }

void SolveWork::Impl::add_action_reason(
        const char* disposition,
        const std::string& action,
        const std::string& reason) {
        retain_action_reason(
            std::string(disposition) + ":" + reason + ":" + action);
    }

void SolveWork::Impl::record_skipped_missing_price(const std::string& action) {
        ++result.diagnostics.skipped_missing_price_count;
        if (result.diagnostics.skipped_missing_price.size() <
            options.max_diagnostic_samples) {
            result.diagnostics.skipped_missing_price.push_back(action);
        }
    }

void SolveWork::Impl::record_skipped_unsupported(const std::string& action) {
        ++result.diagnostics.skipped_unsupported_count;
        if (result.diagnostics.skipped_unsupported.size() <
            options.max_diagnostic_samples) {
            result.diagnostics.skipped_unsupported.push_back(action);
        }
    }

auto SolveWork::Impl::preservation_decision(
        const std::uint64_t row_index) const -> PreservationDecision {
        PreservationDecision decision;
        if (!options.preservation_control || focused_lower_mode ||
            row_index >= transition_cache->rows.size() ||
            row_index >= priced_rows.size()) {
            return decision;
        }
        const PricedSparseRow& priced = priced_rows[row_index];
        if (priced.operator_index == kNoId) return decision;
        const PlannerOperator& planner =
            calc.operators().at(priced.operator_index);
        if (planner.kind != PlannerOperatorKind::Primitive) return decision;
        const ActionDescriptor& action = calc.registry().actions.at(
            planner.primitive_action);
        if (!action.preservation.destructive_renewal) return decision;

        const SparseRow& row = transition_cache->rows.at(row_index);
        const CarrierFacts facts = carrier_facts(calc.state(row.owner_state));
        std::uint32_t progressed = 0;
        if (facts.goal_family_mask != 0) progressed |= kCarrierGoalFamilies;
        if (facts.satisfied_goal_mask != 0) {
            progressed |= kCarrierSatisfiedGoalSubset;
        }
        if (facts.junk_count != 0 || facts.blocked_mask != 0) {
            progressed |= kCarrierJunkBlockers;
        }
        if (facts.crafted_goal_mask != 0 || facts.crafted_junk_count != 0) {
            progressed |= kCarrierCraftedState;
        }
        if (facts.fractured_goal_mask != 0 ||
            facts.fractured_junk_count != 0) {
            progressed |= kCarrierFracturedState;
        }
        if (facts.prefix_count != 0) progressed |= kCarrierPrefixSide;
        if (facts.suffix_count != 0) progressed |= kCarrierSuffixSide;
        if (facts.active_protection != 0) {
            progressed |= kCarrierActiveProtection;
        }
        decision.destroyed_progress =
            row.preservation_effect.destroyed_properties & progressed;
        decision.candidate_lower_bound = std::max(
            priced.cost,
            carrier_action_bellman_lower_value(row.owner_state));

        /* Exact state identity with the synthetic Restart successor is the
         * deliberately strict disposable-carrier certificate. No label,
         * depth, price, or UI stage participates in this proof. */
        if (restart_state != kNoId && row.owner_state == restart_state) {
            decision.disposition =
                PreservationDisposition::RetainedDisposable;
            return decision;
        }
        if (decision.destroyed_progress == 0) {
            decision.disposition =
                PreservationDisposition::RetainedPreserving;
            return decision;
        }

        if (!restart_row_allowed(row.owner_state) ||
            restart_state == kNoId ||
            restart_state >= result.values.size() ||
            !std::isfinite(result.values[restart_state]) ||
            result.values[restart_state] >= kValueCeiling ||
            !std::isfinite(restart_cost) || restart_cost < 0.0 ||
            priced.cost < 0.0) {
            decision.disposition =
                PreservationDisposition::RetainedUncertain;
            return decision;
        }
        decision.restart_upper_bound =
            restart_cost + result.values[restart_state];
        /* Immediate cost and the carrier Bellman subsolution are independent
         * lower bounds on this exact destructive row's complete Q value.
         * Restart plus the current monotone value of its exact successor is
         * a constructive upper bound. Strict inequality preserves
         * price/action ties. */
        if (decision.candidate_lower_bound >
            decision.restart_upper_bound + options.epsilon) {
            decision.disposition =
                PreservationDisposition::PrunedByRestartBound;
        } else {
            decision.disposition =
                PreservationDisposition::RetainedUncertain;
        }
        return decision;
    }

bool SolveWork::Impl::preservation_prunes(const std::uint64_t row_index) const {
        /* A price-bound prune is valid for optimal search but cannot remove a
         * completed legal row from an anytime proper-policy proof. The latter
         * is only trying to establish an executable upper bound after the
         * requested search was interrupted. */
        if (anytime_policy_scratch_bytes != 0) return false;
        return preservation_decision(row_index).disposition ==
               PreservationDisposition::PrunedByRestartBound;
    }

 void SolveWork::Impl::append_json_string(
        std::string& out,
        const std::string& value) {
        out.push_back('"');
        for (const char c : value) {
            if (c == '"' || c == '\\') out.push_back('\\');
            if (c == '\n') {
                out += "\\n";
            } else {
                out.push_back(c);
            }
        }
        out.push_back('"');
    }

 std::string SolveWork::Impl::finite_json(const double value) {
        if (!std::isfinite(value)) return "null";
        char buffer[64];
        std::snprintf(buffer, sizeof(buffer), "%.17g", value);
        return buffer;
    }

 std::string SolveWork::Impl::property_mask_json(const std::uint32_t mask) {
        std::string out = "[";
        bool first = true;
        const auto add = [&](const std::uint32_t bit, const char* name) {
            if ((mask & bit) == 0) return;
            if (!first) out.push_back(',');
            first = false;
            append_json_string(out, name);
        };
        add(kCarrierGoalFamilies, "goal_families");
        add(kCarrierSatisfiedGoalSubset, "satisfied_goal_subset");
        add(kCarrierJunkBlockers, "junk_blockers");
        add(kCarrierCraftedState, "crafted_state");
        add(kCarrierFracturedState, "fractured_state");
        add(kCarrierPrefixSide, "prefix_side");
        add(kCarrierSuffixSide, "suffix_side");
        add(kCarrierActiveProtection, "active_protection");
        out.push_back(']');
        return out;
    }

 std::string SolveWork::Impl::count_vector_json(const CompactCountVector& counts) {
        std::string out = "[";
        for (std::size_t i = 0; i < counts.size(); ++i) {
            if (i != 0) out.push_back(',');
            out += std::to_string(counts[i]);
        }
        out.push_back(']');
        return out;
    }

std::string SolveWork::Impl::preservation_witness_json(
        const std::uint64_t row_index,
        const PreservationDecision& decision) const {
        const SparseRow& row = transition_cache->rows.at(row_index);
        const PricedSparseRow& priced = priced_rows.at(row_index);
        const PlannerOperator& planner =
            calc.operators().at(priced.operator_index);
        const ActionDescriptor& action = calc.registry().actions.at(
            planner.primitive_action);
        const AbstractState& state = calc.state(row.owner_state);
        const CarrierFacts facts = carrier_facts(state);
        const CarrierEffectSummary& effect = row.preservation_effect;
        const bool disposable = decision.disposition ==
                                PreservationDisposition::RetainedDisposable;
        const bool pruned = decision.disposition ==
                            PreservationDisposition::PrunedByRestartBound;
        const char* disposition =
            pruned
                ? "pruned"
                : decision.disposition ==
                          PreservationDisposition::RetainedUncertain
                      ? "included_uncertain"
                      : "included";
        const char* reason =
            disposable
                ? "certified_genuine_restart_carrier"
                : decision.disposition ==
                          PreservationDisposition::RetainedPreserving
                      ? "exact_transition_preserves_current_progress"
                      : pruned
                            ? "candidate_lower_bound_exceeds_restart_route_upper_bound"
                            : "no_exact_dominance_proof_retain_candidate";

        std::string out = "{";
        out += "\"state_id\":" + std::to_string(row.owner_state);
        out += ",\"action\":";
        append_json_string(out, planner.id);
        out += ",\"disposition\":";
        append_json_string(out, disposition);
        out += ",\"reason\":";
        append_json_string(out, reason);
        out += ",\"carrier\":{";
        out += "\"state_hash\":" + std::to_string(facts.state_hash);
        out += ",\"rarity\":" + std::to_string(facts.rarity);
        out += ",\"prefix_count\":" +
               std::to_string(facts.prefix_count);
        out += ",\"suffix_count\":" +
               std::to_string(facts.suffix_count);
        out += ",\"goal_family_mask\":" +
               std::to_string(facts.goal_family_mask);
        out += ",\"satisfied_goal_mask\":" +
               std::to_string(facts.satisfied_goal_mask);
        out += ",\"blocked_mask\":" +
               std::to_string(facts.blocked_mask);
        out += ",\"junk_counts\":" + count_vector_json(state.junk_counts);
        out += ",\"crafted_goal_mask\":" +
               std::to_string(facts.crafted_goal_mask);
        out += ",\"crafted_junk_counts\":" +
               count_vector_json(state.crafted_junk_counts);
        out += ",\"fractured_goal_mask\":" +
               std::to_string(facts.fractured_goal_mask);
        out += ",\"fractured_junk_counts\":" +
               count_vector_json(state.fractured_junk_counts);
        out += ",\"fractured_metamod_flags\":" +
               std::to_string(state.fractured_metamod_flags);
        out += ",\"active_protection_flags\":" +
               std::to_string(facts.active_protection) + "}";
        out += ",\"satisfied_goal_subset\":" +
               std::to_string(facts.satisfied_goal_mask);
        out += ",\"effects\":{";
        out += "\"preserved\":{";
        out += "\"properties\":" +
               property_mask_json(effect.preserved_properties);
        out += ",\"goal_family_mask\":" +
               std::to_string(effect.preserved_goal_family_mask);
        out += ",\"satisfied_goal_mask\":" +
               std::to_string(effect.preserved_satisfied_goal_mask);
        out += ",\"crafted_goal_mask\":" +
               std::to_string(effect.preserved_crafted_goal_mask);
        out += ",\"fractured_goal_mask\":" +
               std::to_string(effect.preserved_fractured_goal_mask) + "}";
        out += ",\"destroyed\":{";
        out += "\"properties\":" +
               property_mask_json(effect.destroyed_properties);
        out += ",\"goal_family_mask\":" +
               std::to_string(effect.destroyed_goal_family_mask);
        out += ",\"satisfied_goal_mask\":" +
               std::to_string(effect.destroyed_satisfied_goal_mask);
        out += ",\"crafted_goal_mask\":" +
               std::to_string(effect.destroyed_crafted_goal_mask);
        out += ",\"fractured_goal_mask\":" +
               std::to_string(effect.destroyed_fractured_goal_mask) + "}";
        out += ",\"created\":{";
        out += "\"properties\":" +
               property_mask_json(effect.created_properties);
        out += ",\"goal_family_mask\":" +
               std::to_string(effect.created_goal_family_mask);
        out += ",\"satisfied_goal_mask\":" +
               std::to_string(effect.created_satisfied_goal_mask) + "}";
        out += ",\"made_unreachable\":{";
        out += "\"properties\":" +
               property_mask_json(effect.unreachable_properties);
        out += ",\"goal_family_mask\":" +
               std::to_string(effect.unreachable_goal_family_mask);
        out += ",\"satisfied_goal_mask\":" +
               std::to_string(effect.unreachable_satisfied_goal_mask) + "}";
        out += ",\"prefix_count_range\":[" +
               std::to_string(effect.min_prefix_count) + "," +
               std::to_string(effect.max_prefix_count) + "]";
        out += ",\"suffix_count_range\":[" +
               std::to_string(effect.min_suffix_count) + "," +
               std::to_string(effect.max_suffix_count) + "]}";
        out += ",\"protection\":{";
        out += "\"active_flags\":" +
               std::to_string(facts.active_protection);
        out += ",\"respects_prefix_lock\":" +
               std::string(action.preservation.respects_prefix_lock
                               ? "true"
                               : "false");
        out += ",\"respects_suffix_lock\":" +
               std::string(action.preservation.respects_suffix_lock
                               ? "true"
                               : "false");
        out += ",\"respects_cannot_roll_attack\":" +
               std::string(
                   action.preservation.respects_cannot_roll_attack
                       ? "true"
                       : "false");
        out += ",\"respects_cannot_roll_caster\":" +
               std::string(
                   action.preservation.respects_cannot_roll_caster
                       ? "true"
                       : "false");
        out += ",\"preserved_flags\":" +
               std::to_string(effect.preserved_protection);
        out += ",\"destroyed_flags\":" +
               std::to_string(effect.destroyed_protection);
        out += ",\"fractured_preservation_independent\":" +
               std::string(
                   action.preservation.preserves_fractured_affixes
                       ? "true"
                       : "false") + "}";
        out += ",\"restart_equivalence\":{";
        out += "\"certified\":" +
               std::string(disposable ? "true" : "false");
        out += ",\"kind\":";
        if (disposable) {
            append_json_string(out, "genuine_restart_state_identity");
        } else {
            out += "null";
        }
        out += ",\"restart_state_id\":" +
               (restart_state == kNoId ? std::string("null")
                                       : std::to_string(restart_state));
        out += ",\"carrier_equals_restart_state\":" +
               std::string(disposable ? "true" : "false") + "}";
        out += ",\"dominance\":{";
        out += "\"dominating_action\":";
        if (pruned) append_json_string(out, "restart");
        else out += "null";
        out += ",\"candidate_lower_bound\":" +
               finite_json(decision.candidate_lower_bound);
        out += ",\"restart_cost\":" + finite_json(restart_cost);
        out += ",\"continuation_state_id\":" +
               (restart_state == kNoId ? std::string("null")
                                       : std::to_string(restart_state));
        const double continuation =
            restart_state < result.values.size()
                ? result.values[restart_state]
                : kInfinity;
        out += ",\"continuation_upper_bound\":" +
               finite_json(continuation);
        out += ",\"route_upper_bound\":" +
               finite_json(decision.restart_upper_bound);
        out += ",\"comparison\":";
        append_json_string(
            out,
            pruned ? "candidate_lower_bound > restart_route_upper_bound"
                   : "no_strict_dominance");
        out += "}";
        out += ",\"uncertain_retention\":{";
        out += "\"retained\":" +
               std::string(
                   decision.disposition ==
                           PreservationDisposition::RetainedUncertain
                       ? "true"
                       : "false");
        out += ",\"reason\":";
        if (decision.disposition ==
            PreservationDisposition::RetainedUncertain) {
            append_json_string(
                out, "no_exact_dominance_or_equivalence_proof");
        } else {
            out += "null";
        }
        out += "}}";
        return out;
    }

 const char* SolveWork::Impl::automatic_kind_name(
        const AutomaticCandidateKind kind) {
        switch (kind) {
        case AutomaticCandidateKind::Fracture:
            return "fracture";
        case AutomaticCandidateKind::PermanentBench:
            return "permanent_bench";
        case AutomaticCandidateKind::TemporaryBenchBlocker:
            return "temporary_bench_blocker";
        case AutomaticCandidateKind::ProtectedMetamod:
            return "protected_metamod";
        case AutomaticCandidateKind::MultimodFinish:
            return "multimod_finish";
        case AutomaticCandidateKind::Imprint:
            return "imprint";
        case AutomaticCandidateKind::ConstructiveRenewal:
            return "constructive_renewal";
        case AutomaticCandidateKind::EldritchSide:
            return "eldritch_side";
        case AutomaticCandidateKind::CannotRoll:
            return "cannot_roll";
        case AutomaticCandidateKind::Veiled:
            return "veiled";
        case AutomaticCandidateKind::None:
            return "none";
        }
        return "none";
    }

 std::string SolveWork::Impl::automatic_mechanisms_json(
        const std::uint32_t mechanisms) {
        std::string out = "[";
        bool first = true;
        const auto add = [&](const std::uint32_t bit, const char* name) {
            if ((mechanisms & bit) == 0) return;
            if (!first) out.push_back(',');
            first = false;
            append_json_string(out, name);
        };
        add(kAutomaticGroupConflict, "mod_group_conflict");
        add(kAutomaticPrefixSlot, "occupied_prefix_slot");
        add(kAutomaticSuffixSlot, "occupied_suffix_slot");
        add(kAutomaticMetamodProtection, "metamod_protection");
        add(kAutomaticCarrierFracture, "carrier_exact_fracture");
        add(kAutomaticDeterministicFinish, "deterministic_finish");
        add(kAutomaticImprintCheckpoint, "imprint_checkpoint_restore");
        add(kAutomaticEldritchDominance, "eldritch_dominance");
        add(kAutomaticMetamodPoolBlock, "metamod_pool_block");
        add(kAutomaticAcquisitionTimeOffer, "acquisition_time_offer");
        out.push_back(']');
        return out;
    }

std::string SolveWork::Impl::automatic_candidate_witness_json(
        const SolveTransitionCache::AutomaticCandidateRecord& record,
        const char* disposition,
        const char* decision_reason) const {
        const PlannerOperator* planner =
            record.operator_index < calc.operators().size()
                ? &calc.operators().at(record.operator_index)
                : nullptr;
        const AbstractState& state = calc.state(record.state_id);
        const CarrierFacts carrier = carrier_facts(state);
        const auto action_id = [&](const std::uint32_t index) {
            return index == kNoId ? std::string()
                                  : calc.registry().actions.at(index).id;
        };
        std::string out = "{";
        out += "\"candidate_kind\":";
        append_json_string(
            out, automatic_kind_name(
                record.candidate_kind != AutomaticCandidateKind::None
                    ? record.candidate_kind
                    : planner != nullptr
                          ? planner->automatic_kind
                          : AutomaticCandidateKind::None));
        out += ",\"candidate\":";
        append_json_string(
            out, record.candidate_id.empty() && planner != nullptr
                     ? planner->id
                     : record.candidate_id);
        out += ",\"carrier_identity\":{\"state_id\":" +
               std::to_string(record.state_id) +
               ",\"state_hash\":" + std::to_string(carrier.state_hash) +
               ",\"rarity\":" + std::to_string(carrier.rarity) +
               ",\"prefix_count\":" +
               std::to_string(carrier.prefix_count) +
               ",\"suffix_count\":" +
               std::to_string(carrier.suffix_count) +
               ",\"flags\":" + std::to_string(state.flags) +
               ",\"influence_bits\":" +
               std::to_string(state.influence_bits) +
               ",\"crafted_goal_mask\":" +
               std::to_string(carrier.crafted_goal_mask) +
               ",\"fractured_goal_mask\":" +
               std::to_string(carrier.fractured_goal_mask) +
               ",\"active_protection_flags\":" +
               std::to_string(carrier.active_protection) + "}";
        out += ",\"relevant_goal_subset\":" +
               std::to_string(record.evidence.relevant_goal_mask);
        out += ",\"legality_result\":";
        append_json_string(out, record.evidence.legality_result);
        out += ",\"exact_kernel_change\":{\"changed\":" +
               std::string(record.evidence.kernel_changed ? "true" : "false") +
               ",\"baseline_hash\":" +
               std::to_string(record.evidence.baseline_kernel_hash) +
               ",\"candidate_hash\":" +
               std::to_string(record.evidence.candidate_kernel_hash) +
                ",\"mechanisms\":" + automatic_mechanisms_json(
                    record.evidence.kernel_change_mechanisms) + "}";
        if (record.evidence.fracture_raw_affix_count != 0) {
            out += ",\"product_fracture_local\":{\"raw_affixes\":" +
                   std::to_string(
                       record.evidence.fracture_raw_affix_count);
            out += ",\"acceptable_affixes\":" +
                   std::to_string(
                       record.evidence.fracture_acceptable_affix_count);
            out += ",\"hit_probability\":" +
                   finite_json(record.evidence.fracture_hit_probability);
            out += ",\"miss_probability\":" +
                   finite_json(record.evidence.fracture_miss_probability);
            out += ",\"probability_sum\":" +
                   finite_json(record.evidence.fracture_probability_sum);
            out += ",\"restart_state\":" +
                   std::to_string(
                       record.evidence.fracture_restart_state);
            out +=
                ",\"miss_route\":\"priced_restart\","
                "\"parent_miss_states_interned\":0}";
        }
        out += ",\"setup_operations\":[";
        if (!record.setup_action_id.empty()) {
            append_json_string(out, record.setup_action_id);
        } else if (planner != nullptr && planner->setup_action != kNoId) {
            append_json_string(out, action_id(planner->setup_action));
        }
        out += "],\"followup_operation\":";
        if (!record.followup_action_id.empty()) {
            append_json_string(out, record.followup_action_id);
        } else if (planner == nullptr || planner->followup_action == kNoId) {
            out += "null";
        } else {
            append_json_string(out, action_id(planner->followup_action));
        }
        out += ",\"cleanup_operations\":[";
        if (!record.cleanup_action_id.empty()) {
            append_json_string(out, record.cleanup_action_id);
        } else if (planner != nullptr && planner->cleanup_action != kNoId) {
            append_json_string(out, action_id(planner->cleanup_action));
        }
        out += "],\"coverage\":{\"setup_complete\":" +
               std::string(record.evidence.setup_complete ? "true" : "false") +
               ",\"cleanup_complete\":" +
               std::string(record.evidence.cleanup_complete ? "true" : "false") +
               ",\"recovery_complete\":" +
               std::string(record.evidence.recovery_complete ? "true" : "false") +
               ",\"outer_exits_complete\":" +
               std::string(record.evidence.exits_complete ? "true" : "false") +
               "}";
        out += ",\"disposition\":";
        append_json_string(out, disposition);
        out += ",\"reason\":";
        append_json_string(out, decision_reason);
        out += ",\"eligibility_reason\":";
        append_json_string(out, record.evidence.reason);
        out += "}";
        return out;
    }

void SolveWork::Impl::retain_automatic_candidate_record(
        SolveTransitionCache::AutomaticCandidateRecord record) {
        if (record.count_candidate) {
            ++transition_cache->automatic_rows_considered;
            if (record.deferred) {
                ++transition_cache->automatic_rows_deferred;
            } else if (!record.eligible) {
                ++transition_cache->automatic_rows_rejected;
            } else {
                ++transition_cache->automatic_rows_eligible;
                if (record.collapsed) {
                    ++transition_cache->automatic_rows_collapsed;
                }
            }
        }
        if (record.telemetry_kind != AutomaticTelemetryKind::None) {
            AutomaticKindTelemetry& kind =
                transition_cache->automatic_kind_telemetry.at(
                    static_cast<std::size_t>(record.telemetry_kind));
            if (record.count_candidate) {
                ++kind.candidates;
                if (record.deferred) {
                    ++kind.deferred_candidates;
                } else if (!record.eligible) {
                    ++kind.rejected_candidates;
                } else {
                    ++kind.eligible_candidates;
                }
                if (record.collapsed) ++kind.collapsed_candidates;
                if (record.missing_price) {
                    ++kind.missing_price_candidates;
                }
                if (!record.eligible && !record.deferred &&
                    !record.missing_price) {
                    const std::string& reason = record.evidence.reason;
                    if (reason.find("cleanup") != std::string::npos) {
                        ++kind.cleanup_rejections;
                    } else if (reason.find("neutral") != std::string::npos) {
                        ++kind.neutral_kernel_rejections;
                    } else if (reason.find("relevant") != std::string::npos ||
                               reason.find("advance") != std::string::npos ||
                               reason.find("protected_carrier") !=
                                   std::string::npos) {
                        ++kind.relevance_rejections;
                    } else if (reason.find("setup") != std::string::npos ||
                               reason.find("protection_already") !=
                                   std::string::npos) {
                        ++kind.setup_rejections;
                    } else {
                        ++kind.other_rejections;
                    }
                }
                if (record.template_id != 0) {
                    if (record.template_hit) ++kind.template_hits;
                    else ++kind.unique_templates;
                }
                kind.raw_outcomes += record.raw_outcomes;
                kind.admission_ns += record.admission_ns;
                kind.kernel_evaluation_ns +=
                    record.kernel_evaluation_ns;
                kind.outcome_mapping_ns += record.outcome_mapping_ns;
                kind.template_matching_ns +=
                    record.template_matching_ns;
                kind.protected_side_evaluations +=
                    record.protected_side_evaluations;
                kind.protected_repeat_evaluations +=
                    record.protected_repeat_evaluations;
                kind.protected_retry_checks +=
                    record.protected_retry_checks;
                kind.protected_retry_certificates +=
                    record.protected_retry_certificates;
                kind.protected_retry_fallbacks +=
                    record.protected_retry_fallbacks;
                kind.protected_attempt_ns += record.protected_attempt_ns;
                kind.protected_baseline_ns += record.protected_baseline_ns;
                kind.protected_normalization_ns +=
                    record.protected_normalization_ns;
                kind.protected_finish_ns += record.protected_finish_ns;
            }
            kind.precompiled_classes = std::max(
                kind.precompiled_classes, record.precompiled_classes);
            kind.precompile_ns = std::max(
                kind.precompile_ns, record.precompile_ns);
            kind.precompiled_bytes = std::max(
                kind.precompiled_bytes, record.precompiled_bytes);
            kind.candidate_variants += record.candidate_variants;
            kind.effect_classes += record.effect_classes;
            kind.collapsed_variants += record.collapsed_variants;
            kind.enumeration_ns += record.enumeration_ns;
            kind.rows += record.retained_rows;
            kind.retained_transitions += record.retained_transitions;
            kind.row_ns += record.row_ns;
            kind.selected_bytes += record.selected_bytes;
            const std::uint64_t carrier_key =
                (static_cast<std::uint64_t>(record.state_id) << 8) |
                static_cast<std::uint64_t>(record.telemetry_kind);
            auto [carrier, inserted] = automatic_carrier_work.try_emplace(
                carrier_key, AutomaticCarrierWork{});
            if (inserted) ++kind.carriers;
            if (record.count_candidate) {
                ++carrier->second.candidates;
                if (record.template_id != 0 && !record.template_hit) {
                    ++carrier->second.templates;
                }
            }
            carrier->second.candidate_variants +=
                record.candidate_variants;
            carrier->second.effect_classes += record.effect_classes;
            carrier->second.rows += record.retained_rows;
            kind.max_candidates_per_carrier = std::max(
                kind.max_candidates_per_carrier,
                carrier->second.candidates);
            kind.max_templates_per_carrier = std::max(
                kind.max_templates_per_carrier,
                carrier->second.templates);
            kind.max_candidate_variants_per_carrier = std::max(
                kind.max_candidate_variants_per_carrier,
                carrier->second.candidate_variants);
            kind.max_effect_classes_per_carrier = std::max(
                kind.max_effect_classes_per_carrier,
                carrier->second.effect_classes);
            kind.max_rows_per_carrier = std::max(
                kind.max_rows_per_carrier, carrier->second.rows);
        }
        if (record.count_candidate) {
            auto& samples =
                transition_cache->automatic_candidate_samples;
            if (samples.size() < options.max_diagnostic_samples) {
                transition_cache->retain_automatic_sample(
                    std::move(record));
            } else if (record.deferred &&
                       options.max_diagnostic_samples != 0) {
                /* A resource-deferred automatic envelope is the authority
                 * preventing exact closure. Preserve one such witness even
                 * when earlier routine rejections filled the bounded sample;
                 * replace rather than grow so max_diagnostic_samples remains
                 * the exact retention cap. */
                const auto replacement = std::find_if(
                    samples.rbegin(), samples.rend(),
                    [](const SolveTransitionCache::AutomaticCandidateRecord&
                           sample) {
                        return !sample.deferred;
                    });
                if (replacement != samples.rend()) {
                    const std::uint64_t removed =
                        SolveTransitionCache::automatic_sample_nested_bytes(
                            *replacement);
                    const std::uint64_t added =
                        SolveTransitionCache::automatic_sample_nested_bytes(
                            record);
                    if (transition_cache
                            ->owned_automatic_sample_nested_bytes < removed) {
                        throw std::logic_error(
                            "automatic diagnostic sample byte ledger "
                            "underflow");
                    }
                    transition_cache->owned_automatic_sample_nested_bytes =
                        transition_cache
                            ->owned_automatic_sample_nested_bytes -
                        removed + added;
                    *replacement = std::move(record);
                }
            }
        }
    }

void SolveWork::Impl::finalize_automatic_candidate_diagnostics() {
        result.diagnostics.automatic_rows_considered =
            transition_cache->automatic_rows_considered;
        result.diagnostics.automatic_rows_eligible =
            transition_cache->automatic_rows_eligible;
        result.diagnostics.automatic_rows_rejected =
            transition_cache->automatic_rows_rejected;
        result.diagnostics.automatic_rows_collapsed =
            transition_cache->automatic_rows_collapsed;
        result.diagnostics.automatic_rows_selected = 0;
        result.diagnostics.automatic_rows_deferred =
            transition_cache->automatic_rows_deferred;
        result.diagnostics.automatic_kind_telemetry =
            transition_cache->automatic_kind_telemetry;
        result.diagnostics.automatic_admission_phases =
            transition_cache->automatic_admission_phases;
        result.diagnostics.automatic_candidate_witnesses.clear();
        result.diagnostics.automatic_candidate_witnesses_omitted =
            result.diagnostics.automatic_rows_considered -
            transition_cache->automatic_candidate_samples.size();
        for (std::uint32_t state = 0; state < result.policy.size(); ++state) {
            const std::uint32_t operator_index = result.policy[state].index;
            if (operator_index != kNoId &&
                operator_index < calc.operators().size() &&
                calc.operators().at(operator_index).automatic_kind !=
                    AutomaticCandidateKind::None) {
                ++result.diagnostics.automatic_rows_selected;
            }
        }
        for (const auto& record :
             transition_cache->automatic_candidate_samples) {
            const bool selected =
                record.operator_index != kNoId &&
                record.state_id < result.policy.size() &&
                result.policy[record.state_id].index == record.operator_index;
            const char* disposition = "included";
            const char* reason = "retained_for_minimum_expected_cost_bellman_step";
            if (record.deferred) {
                disposition = "deferred";
                reason = "solver_resource_cap_before_exact_row_completion";
            } else if (!record.eligible) {
                disposition = "rejected";
                reason = "exact_invalidity_legality_or_relevance";
            } else {
                if (record.collapsed) {
                    disposition = "collapsed";
                    reason = "exact_kernel_variant_retained_for_price_choice";
                }
                if (selected) {
                    disposition = "selected";
                    reason = "minimum_complete_expected_downstream_cost";
                }
            }
            result.diagnostics.automatic_candidate_witnesses.push_back(
                automatic_candidate_witness_json(
                    record, disposition, reason));
        }
        result.diagnostics.product_fracture_rows =
            transition_cache->product_fracture_rows.size();
        result.diagnostics.product_fracture_raw_outcomes = 0;
        result.diagnostics.product_fracture_hit_entries = 0;
        result.diagnostics.product_fracture_miss_entries = 0;
        result.diagnostics.product_fracture_parent_miss_states_interned = 0;
        result.diagnostics.product_fracture_selected_rows = 0;
        result.diagnostics
            .product_fracture_selected_properness_checked = 0;
        result.diagnostics.product_fracture_selected_proper_rows = 0;
        result.diagnostics.product_fracture_selected_improper_rows = 0;
        result.diagnostics.product_fracture_selected_unproved_rows = 0;
        result.diagnostics.product_fracture_q_evaluated_rows = 0;
        result.diagnostics.product_fracture_q_cheaper_rows = 0;
        result.diagnostics.product_fracture_q_tied_rows = 0;
        result.diagnostics.product_fracture_q_costlier_rows = 0;
        result.diagnostics.product_fracture_q_unresolved_rows = 0;
        result.diagnostics.product_fracture_best_q_advantage = 0.0;
        result.diagnostics.product_fracture_max_probability_error = 0.0;
        result.diagnostics.product_fracture_shape_rows = {};
        result.diagnostics.product_fracture_witnesses.clear();
        const auto row_values_available =
            [&](const std::uint64_t row_index) {
                if (row_index >= transition_cache->rows.size()) {
                    return false;
                }
                const SparseRow& row =
                    transition_cache->rows.at(row_index);
                for (std::uint32_t i = 0;
                     i < row.transition_count; ++i) {
                    if (transition_cache->successors.at(
                            row.transition_offset + i) >=
                        result.values.size()) {
                        return false;
                    }
                }
                for (std::uint32_t i = 0;
                     i < row.choice_count; ++i) {
                    const SparseChoiceGroup& group =
                        transition_cache->choices.at(
                            row.choice_offset + i);
                    for (std::uint32_t s = 0;
                         s < group.successor_count; ++s) {
                        if (transition_cache->choice_successors.at(
                                group.successor_offset + s) >=
                            result.values.size()) {
                            return false;
                        }
                    }
                }
                return true;
            };
        std::map<std::uint32_t, SparsePolicyRowSelection>
            selected_row_by_source;
        for (auto& witness :
            transition_cache->product_fracture_rows) {
            witness.selected_in_policy =
                result.policy_available &&
                witness.source_state < result.policy.size() &&
                result.policy[witness.source_state].index ==
                    witness.operator_index;
            witness.properness_checked =
                witness.selected_in_policy && result.policy_available;
            witness.proper =
                witness.properness_checked &&
                improper_policy_states.empty();
            witness.final_q_evaluated = false;
            witness.final_source_value = kInfinity;
            witness.final_fracture_q = kInfinity;
            witness.final_selected_q = kInfinity;
            witness.final_selected_row =
                std::numeric_limits<std::uint64_t>::max();
            witness.final_selected_operator = kNoId;
            witness.final_selection_reason =
                SolveTransitionCache::ProductFractureQReason::None;
            if (witness.source_state < result.values.size() &&
                witness.row_index < transition_cache->rows.size() &&
                witness.row_index < priced_rows.size() &&
                row_values_available(witness.row_index)) {
                auto [selected, inserted] =
                    selected_row_by_source.try_emplace(
                        witness.source_state);
                if (inserted) {
                    selected->second = select_sparse_policy_row(
                        *transition_cache, witness.source_state,
                        [&](const std::uint64_t row) {
                            return transition_cache->rows.at(row).admitted &&
                                   !preservation_prunes(row) &&
                                   row_values_available(row);
                        },
                        [&](const std::uint64_t row,
                            std::uint32_t& work) {
                            return evaluate_sparse_policy_row(
                                *transition_cache, priced_rows,
                                result.values, row, work);
                        });
                }
                std::uint32_t fracture_work = 0;
                witness.final_source_value =
                    result.values[witness.source_state];
                witness.final_fracture_q = evaluate_sparse_policy_row(
                    *transition_cache, priced_rows, result.values,
                    witness.row_index, fracture_work);
                witness.final_selected_row = selected->second.row;
                witness.final_selected_q = selected->second.value;
                if (witness.final_selected_row < priced_rows.size()) {
                    witness.final_selected_operator =
                        priced_rows[witness.final_selected_row]
                            .operator_index;
                }
                witness.final_q_evaluated =
                    std::isfinite(witness.final_fracture_q) &&
                    std::isfinite(witness.final_selected_q);
                if (!witness.final_q_evaluated) {
                    ++result.diagnostics
                          .product_fracture_q_unresolved_rows;
                    witness.final_selection_reason =
                        SolveTransitionCache::ProductFractureQReason::
                            NonfiniteSuccessorOrSelectedQ;
                } else {
                    ++result.diagnostics
                          .product_fracture_q_evaluated_rows;
                    const double delta =
                        witness.final_selected_q -
                        witness.final_fracture_q;
                    result.diagnostics
                        .product_fracture_best_q_advantage = std::max(
                            result.diagnostics
                                .product_fracture_best_q_advantage,
                            delta);
                    if (witness.row_index ==
                        witness.final_selected_row) {
                        ++result.diagnostics
                              .product_fracture_q_tied_rows;
                        witness.final_selection_reason =
                            SolveTransitionCache::ProductFractureQReason::
                                SelectedStrictArgmin;
                    } else if (sparse_policy_row_precedes(
                                   witness.final_fracture_q,
                                   witness.row_index,
                                   witness.final_selected_q,
                                   witness.final_selected_row)) {
                        ++result.diagnostics
                              .product_fracture_q_cheaper_rows;
                        witness.final_selection_reason =
                            SolveTransitionCache::ProductFractureQReason::
                                CheaperThanCapturedPolicy;
                    } else if (
                        witness.final_fracture_q ==
                        witness.final_selected_q) {
                        ++result.diagnostics
                              .product_fracture_q_tied_rows;
                        witness.final_selection_reason =
                            SolveTransitionCache::ProductFractureQReason::
                                ExactTieLostByStableRowOrder;
                    } else {
                        ++result.diagnostics
                              .product_fracture_q_costlier_rows;
                        witness.final_selection_reason =
                            SolveTransitionCache::ProductFractureQReason::
                                CostlierThanSelectedQ;
                    }
                }
            } else {
                ++result.diagnostics
                      .product_fracture_q_unresolved_rows;
                witness.final_selection_reason =
                    SolveTransitionCache::ProductFractureQReason::
                        RowOrSourceNotRetained;
            }
            if (witness.selected_in_policy) {
                ++result.diagnostics.product_fracture_selected_rows;
                if (witness.properness_checked) {
                    ++result.diagnostics
                          .product_fracture_selected_properness_checked;
                    if (witness.proper) {
                        ++result.diagnostics
                              .product_fracture_selected_proper_rows;
                    } else {
                        ++result.diagnostics
                              .product_fracture_selected_improper_rows;
                    }
                } else {
                    ++result.diagnostics
                          .product_fracture_selected_unproved_rows;
                }
            }
            result.diagnostics.product_fracture_raw_outcomes +=
                witness.raw_affix_count;
            result.diagnostics.product_fracture_hit_entries +=
                witness.hit_state_count;
            result.diagnostics.product_fracture_miss_entries +=
                witness.miss_probability > 0.0 ? 1 : 0;
            result.diagnostics.product_fracture_parent_miss_states_interned +=
                witness.parent_miss_state_count;
            result.diagnostics.product_fracture_max_probability_error =
                std::max(
                    result.diagnostics
                        .product_fracture_max_probability_error,
                    std::abs(witness.probability_sum - 1.0));
            if (witness.raw_affix_count <
                    result.diagnostics.product_fracture_shape_rows.size() &&
                witness.acceptable_affix_count <=
                    kMaxGoalSlots) {
                ++result.diagnostics.product_fracture_shape_rows
                      [witness.raw_affix_count]
                      [witness.acceptable_affix_count];
            }
            if (result.diagnostics.product_fracture_witnesses.size() >=
                options.max_diagnostic_samples) {
                continue;
            }
            const char* final_q_reason = "none";
            switch (witness.final_selection_reason) {
            case SolveTransitionCache::ProductFractureQReason::None:
                break;
            case SolveTransitionCache::ProductFractureQReason::
                    RowOrSourceNotRetained:
                final_q_reason = "row_or_source_not_retained";
                break;
            case SolveTransitionCache::ProductFractureQReason::
                    NonfiniteSuccessorOrSelectedQ:
                final_q_reason = "nonfinite_successor_or_selected_q";
                break;
            case SolveTransitionCache::ProductFractureQReason::
                    SelectedStrictArgmin:
                final_q_reason = "selected_strict_argmin";
                break;
            case SolveTransitionCache::ProductFractureQReason::
                    CheaperThanCapturedPolicy:
                final_q_reason = "cheaper_than_captured_policy";
                break;
            case SolveTransitionCache::ProductFractureQReason::
                    ExactTieLostByStableRowOrder:
                final_q_reason =
                    "exact_q_tie_lost_by_stable_row_order";
                break;
            case SolveTransitionCache::ProductFractureQReason::
                    CostlierThanSelectedQ:
                final_q_reason = "costlier_than_selected_q";
                break;
            }
            std::string json =
                "{\"source_state\":" +
                std::to_string(witness.source_state) +
                ",\"row\":" + std::to_string(witness.row_index) +
                ",\"operator\":" +
                std::to_string(witness.operator_index) +
                ",\"raw_affixes\":" +
                std::to_string(witness.raw_affix_count) +
                ",\"acceptable_affixes\":" +
                std::to_string(witness.acceptable_affix_count) +
                ",\"acceptable_goal_mask\":" +
                std::to_string(witness.acceptable_goal_mask) +
                ",\"hit_probability\":" +
                finite_json(witness.hit_probability) +
                ",\"miss_probability\":" +
                finite_json(witness.miss_probability) +
                ",\"probability_sum\":" +
                finite_json(witness.probability_sum) +
                ",\"restart_state\":" +
                std::to_string(witness.restart_state) +
                ",\"costs\":{\"fracture_action\":" +
                finite_json(witness.fracture_action_cost) +
                ",\"restart_action\":" +
                finite_json(witness.restart_action_cost) +
                ",\"base_unit\":" +
                finite_json(witness.base_unit_cost) +
                "},\"resources\":{\"base\":" +
                finite_json(witness.restart_resource_quantity) +
                "},\"fallback_provenance\":{\"restart_operator\":" +
                std::to_string(witness.restart_operator_index) +
                ",\"restart_state\":" +
                std::to_string(witness.restart_state) +
                ",\"action_vocabulary_identity\":" +
                std::to_string(witness.action_vocabulary_identity) +
                ",\"kernel_identity\":" +
                std::to_string(witness.kernel_identity) +
                "},\"hit_states\":[";
            for (std::uint32_t i = 0;
                 i < witness.hit_state_count; ++i) {
                if (i != 0) json.push_back(',');
                json += "{\"state\":" +
                        std::to_string(witness.hit_states[i]) +
                        ",\"probability\":" +
                        finite_json(witness.hit_probabilities[i]) + "}";
            }
            json +=
                "],\"miss_route\":\"priced_restart\","
                "\"parent_miss_states_interned\":" +
                std::to_string(witness.parent_miss_state_count) +
                ",\"retry_semantics\":\"restart_then_policy_router\","
                "\"selected_in_policy\":" +
                std::string(
                    witness.selected_in_policy ? "true" : "false") +
                ",\"properness_checked\":" +
                std::string(
                    witness.properness_checked ? "true" : "false") +
                ",\"proper\":" +
                std::string(witness.proper ? "true" : "false") +
                ",\"properness_result\":\"" +
                (witness.selected_in_policy
                     ? (witness.proper ? "selected_policy_proper"
                                       : "selected_policy_not_proved")
                     : "not_selected_not_required") +
                "\",\"final_q\":{\"evaluated\":" +
                std::string(
                    witness.final_q_evaluated ? "true" : "false") +
                ",\"source_value\":" +
                finite_json(witness.final_source_value) +
                ",\"fracture\":" +
                finite_json(witness.final_fracture_q) +
                ",\"selected\":" +
                finite_json(witness.final_selected_q) +
                ",\"selected_row\":" +
                (witness.final_selected_row ==
                         std::numeric_limits<std::uint64_t>::max()
                     ? "null"
                     : std::to_string(witness.final_selected_row)) +
                ",\"selected_operator\":" +
                (witness.final_selected_operator == kNoId
                     ? "null"
                     : std::to_string(
                           witness.final_selected_operator)) +
                ",\"selected_minus_fracture\":" +
                finite_json(
                    witness.final_selected_q -
                    witness.final_fracture_q) +
                ",\"reason\":\"" +
                final_q_reason + "\"}}";
            result.diagnostics.product_fracture_witnesses.push_back(
                std::move(json));
        }
        result.diagnostics.product_fracture_witnesses_omitted =
            result.diagnostics.product_fracture_rows -
            result.diagnostics.product_fracture_witnesses.size();
    }

void SolveWork::Impl::finalize_preservation_diagnostics() {
        result.diagnostics.preservation_rows_considered = 0;
        result.diagnostics.preservation_rows_pruned = 0;
        result.diagnostics.preservation_rows_retained = 0;
        result.diagnostics.certified_disposable_rows = 0;
        result.diagnostics.preservation_witnesses.clear();
        result.diagnostics.preservation_witnesses_omitted = 0;
        if (!options.preservation_control) return;
        for (std::uint32_t state = 0;
             state < transition_cache->state_rows.size(); ++state) {
            for (const std::uint64_t row_index :
                 state_row_indices(*transition_cache, state)) {
                if (!transition_cache->rows.at(row_index).admitted) continue;
                const PreservationDecision decision =
                    preservation_decision(row_index);
                if (decision.disposition ==
                    PreservationDisposition::NotApplicable) {
                    continue;
                }
                ++result.diagnostics.preservation_rows_considered;
                const PricedSparseRow& priced = priced_rows[row_index];
                const std::string& action =
                    calc.operators().at(priced.operator_index).id;
                if (decision.disposition ==
                    PreservationDisposition::PrunedByRestartBound) {
                    ++result.diagnostics.preservation_rows_pruned;
                    add_action_reason(
                        "pruned", action,
                        "preservation_restart_bound_state_" +
                            std::to_string(state));
                } else {
                    ++result.diagnostics.preservation_rows_retained;
                    if (decision.disposition ==
                        PreservationDisposition::RetainedDisposable) {
                        ++result.diagnostics.certified_disposable_rows;
                        add_action_reason(
                            "included", action,
                            "certified_genuine_restart_carrier_state_" +
                                std::to_string(state));
                    } else if (decision.disposition ==
                               PreservationDisposition::RetainedUncertain) {
                        add_action_reason(
                            "included", action,
                            "uncertain_preservation_retained_state_" +
                                std::to_string(state));
                    }
                }
                if (result.diagnostics.preservation_witnesses.size() <
                    options.max_diagnostic_samples) {
                    result.diagnostics.preservation_witnesses.push_back(
                        preservation_witness_json(row_index, decision));
                } else {
                    ++result.diagnostics.preservation_witnesses_omitted;
                }
            }
        }
    }

void SolveWork::Impl::record_cap(const std::string& name, bool state_cap ) {
        if (std::find(result.diagnostics.cap_hits.begin(),
                      result.diagnostics.cap_hits.end(), name) ==
            result.diagnostics.cap_hits.end()) {
            result.diagnostics.cap_hits.push_back(name);
        }
        result.diagnostics.resource_cap_hit = true;
        if (state_cap) result.diagnostics.state_cap_hit = true;
    }

bool SolveWork::Impl::check_solver_byte_cap_from(
        const std::uint64_t current,
        const std::uint64_t transient_bytes ) {
        const std::uint64_t projected =
            transient_bytes > std::numeric_limits<std::uint64_t>::max() - current
                ? std::numeric_limits<std::uint64_t>::max()
                : current + transient_bytes;
        peak_owned_bytes = std::max(peak_owned_bytes, projected);
        if (projected > options.max_solver_owned_bytes) {
            if (result.diagnostics.policy_evaluation_failure.empty()) {
                const std::uint64_t active_graph =
                    transition_cache == nullptr
                        ? 0
                        : transition_cache->fast_estimated_owned_bytes();
                const std::uint64_t strict_graph =
                    focused_strict_transition_cache == nullptr ||
                            focused_strict_transition_cache == transition_cache
                        ? 0
                        : focused_strict_transition_cache
                              ->fast_estimated_owned_bytes();
                result.diagnostics.policy_evaluation_failure =
                    "selected_byte_cap_breakdown:current=" +
                    std::to_string(current) + ",transient=" +
                    std::to_string(transient_bytes) + ",calc=" +
                    std::to_string(calc.fast_estimated_owned_bytes()) +
                    ",active_graph=" + std::to_string(active_graph) +
                    ",strict_graph=" + std::to_string(strict_graph) +
                    ",policy_scratch=" +
                    std::to_string(current_policy_scratch_bytes) +
                    (transition_cache == nullptr
                         ? std::string{}
                         : ",graph_caps=rows:" +
                               std::to_string(
                                   transition_cache->rows.capacity()) +
                               ",variants:" +
                               std::to_string(
                                   transition_cache->variant_arena->variants
                                       .capacity()) +
                               ",row_variants:" +
                               std::to_string(transition_cache
                                                  ->variant_arena
                                                  ->row_variant_indices
                                                  .capacity()) +
                               ",quantities:" +
                               std::to_string(transition_cache
                                                  ->variant_arena
                                                  ->variant_quantities
                                                  .capacity()) +
                               ",successors:" +
                               std::to_string(
                                   transition_cache->successors.capacity()) +
                               ",choices:" +
                               std::to_string(
                                   transition_cache->choices.capacity()) +
                               ",choice_successors:" +
                               std::to_string(transition_cache
                                                  ->choice_successors
                                                  .capacity()) +
                               ",choice_options:" +
                               std::to_string(transition_cache
                                                  ->choice_options
                                                  .capacity()));
            }
            record_cap("max_solver_owned_bytes");
            return true;
        }
        return false;
    }

bool SolveWork::Impl::check_solver_byte_cap(const std::uint64_t transient_bytes ) {
        return check_solver_byte_cap_from(
            audited_estimated_owned_bytes(), transient_bytes);
    }

bool SolveWork::Impl::check_solver_byte_cap_fast(
        const std::uint64_t transient_bytes ) {
        return check_solver_byte_cap_from(
            fast_estimated_owned_bytes(), transient_bytes);
    }

template <typename T>
    void SolveWork::Impl::reserve_selected_growth(
        std::vector<T>& values, const std::size_t additional) {
        const std::size_t capacity =
            selected_growth_capacity(values, additional);
        if (capacity == values.capacity()) return;
        const std::uint64_t added =
            static_cast<std::uint64_t>(capacity - values.capacity()) *
            sizeof(T);
        if (check_solver_byte_cap_fast(added)) {
            throw SolverResourceLimit(
                "max_solver_owned_bytes", options.max_solver_owned_bytes);
        }
        values.reserve(capacity);
    }

std::pair<bool, std::uint64_t> SolveWork::Impl::append_sparse_row(
        const std::uint32_t state,
        PendingSparseRow pending) {
        static const std::vector<OutcomeEntry> empty_transitions;
        static const std::vector<OutcomeChoiceGroup> empty_choices;
        static const std::vector<OutcomeChoiceOption> empty_choice_options;
        const auto& transitions = pending.transitions == nullptr
                                      ? empty_transitions
                                      : *pending.transitions;
        const auto& choices = pending.choices == nullptr
                                  ? empty_choices
                                  : *pending.choices;
        const auto& choice_options = pending.choice_options == nullptr
                                         ? empty_choice_options
                                         : *pending.choice_options;
        const auto identity_found =
            !options.kernel_reuse ||
                    pending.shared_kernel_identity == nullptr
                ? shared_kernel_rows.end()
                : shared_kernel_rows.find(pending.shared_kernel_identity);
        pending.state = state;
        if (state >= transition_cache->state_rows.size()) {
            transition_cache->state_rows.resize(state + 1);
        }
        StateRowSpan& span = transition_cache->state_rows[state];
        const auto is_self = [&](const std::uint32_t successor) {
            return pending.entry_relative_self ? successor == kNoId
                                               : successor == state;
        };
        std::uint64_t transition_count = 0;
        double self_probability = 0.0;
        if (pending.entry_relative_self) {
            for (const OutcomeEntry& entry : transitions) {
                if (entry.state == kNoId) {
                    self_probability += entry.probability;
                } else {
                    ++transition_count;
                }
            }
        } else {
            transition_count = transitions.size();
            const auto self = std::lower_bound(
                transitions.begin(), transitions.end(), state,
                [](const OutcomeEntry& entry, const std::uint32_t value) {
                    return entry.state < value;
                });
            if (self != transitions.end() && self->state == state) {
                self_probability = self->probability;
            }
        }
        const std::size_t kernel_transition_count =
            static_cast<std::size_t>(transition_count);
        for (const OutcomeChoiceGroup& group : choices) {
            for (const std::uint32_t successor : group.states) {
                if (!is_self(successor)) ++transition_count;
            }
        }
        const std::size_t hash =
            identity_found == shared_kernel_rows.end()
                ? pending.exact_kernel_hash.has_value()
                      ? *pending.exact_kernel_hash
                      : kernel_hash(pending)
                : 0;
        const SparseRow* shared_kernel = nullptr;
        KernelRowMemo* shared_hash_memo = nullptr;
        if (identity_found != shared_kernel_rows.end()) {
            shared_kernel = &transition_cache->rows.at(
                identity_found->second.row_index);
        } else if (options.kernel_reuse) {
            const auto found = kernel_rows_by_hash.find(hash);
            if (found != kernel_rows_by_hash.end()) {
                for (KernelRowMemo& memo : found->second) {
                    const SparseRow& candidate =
                        transition_cache->rows.at(memo.row_index);
                    if (same_kernel(
                            candidate, pending, kernel_transition_count,
                            self_probability)) {
                        shared_kernel = &candidate;
                        shared_hash_memo = &memo;
                        break;
                    }
                }
            }
        }
        SparseRow* equivalent = nullptr;
        if (pending.collapse_equivalent && shared_kernel != nullptr) {
            std::uint64_t row_index = span.offset;
            for (std::uint32_t i = 0; i < span.count; ++i) {
                SparseRow& stored = transition_cache->rows.at(row_index);
                if (stored.transition_offset !=
                        shared_kernel->transition_offset ||
                    stored.transition_count !=
                        shared_kernel->transition_count ||
                    stored.choice_offset != shared_kernel->choice_offset ||
                    stored.choice_count != shared_kernel->choice_count ||
                    stored.self_probability != self_probability) {
                    row_index = stored.next_owner_row;
                    continue;
                }
                equivalent = &stored;
                break;
            }
        } else if (pending.collapse_equivalent) {
            std::uint64_t row_index = span.offset;
            for (std::uint32_t i = 0; i < span.count; ++i) {
                SparseRow& stored = transition_cache->rows.at(row_index);
                if (!same_kernel(
                        stored, pending, kernel_transition_count,
                        self_probability)) {
                    row_index = stored.next_owner_row;
                    continue;
                }
                equivalent = &stored;
                break;
            }
        }
        if (equivalent == nullptr &&
            transition_cache->rows.size() >= options.max_state_action_rows) {
            throw SolverResourceLimit(
                "max_state_action_rows", options.max_state_action_rows);
        }

        SparseRow* stored_row = equivalent;
        if (stored_row == nullptr) {
            SparseRow row;
            row.owner_state = state;
            row.admitted = pending.admitted;
            row.variant_offset = transition_cache->variant_arena
                                     ->row_variant_indices.size();
            row.self_probability = self_probability;
            row.embedded_self_probability =
                pending.entry_relative_self ? 0.0 : self_probability;
            row.self_probability_embedded =
                row.embedded_self_probability > 0.0;
            const bool shareable_successor_envelope =
                !pending.entry_relative_self && choices.empty();
            const CarrierSuccessorEnvelope* cached_envelope = nullptr;
            if (identity_found != shared_kernel_rows.end() &&
                identity_found->second.successor_envelope.has_value()) {
                cached_envelope =
                    &*identity_found->second.successor_envelope;
            } else if (shareable_successor_envelope &&
                       shared_hash_memo != nullptr &&
                       shared_hash_memo->successor_envelope.has_value()) {
                cached_envelope =
                    &*shared_hash_memo->successor_envelope;
            }
            std::optional<CarrierSuccessorEnvelope> new_kernel_envelope;
            if (cached_envelope != nullptr) {
                row.preservation_effect = carrier_effect(
                    calc, state, *cached_envelope);
            } else {
                std::vector<std::uint32_t> effect_successors;
                effect_successors.reserve(transition_count);
                for (const OutcomeEntry& entry : transitions) {
                    if (entry.probability != 0.0) {
                        effect_successors.push_back(
                            is_self(entry.state) ? state : entry.state);
                    }
                }
                for (const OutcomeChoiceGroup& group : choices) {
                    if (group.probability == 0.0) continue;
                    for (const std::uint32_t successor : group.states) {
                        effect_successors.push_back(
                            is_self(successor) ? state : successor);
                    }
                }
                CarrierSuccessorEnvelope envelope =
                    carrier_successor_envelope(
                        calc, std::move(effect_successors));
                row.preservation_effect = carrier_effect(
                    calc, state, envelope);
                if (shareable_successor_envelope) {
                    if (identity_found != shared_kernel_rows.end()) {
                        identity_found->second.successor_envelope = envelope;
                    } else if (shared_hash_memo != nullptr) {
                        shared_hash_memo->successor_envelope = envelope;
                    } else {
                        new_kernel_envelope = std::move(envelope);
                    }
                }
            }
            if (shared_kernel != nullptr) {
                ++result.diagnostics.exact_kernel_payload_reuses;
                result.diagnostics.exact_kernel_payload_bytes_saved +=
                    transition_count *
                        (sizeof(std::uint32_t) + sizeof(double)) +
                    choices.size() * sizeof(SparseChoiceGroup);
                for (const OutcomeChoiceGroup& group : choices) {
                    result.diagnostics.exact_kernel_payload_bytes_saved +=
                        group.states.size() * sizeof(std::uint32_t);
                }
                row.transition_offset = shared_kernel->transition_offset;
                row.transition_count = shared_kernel->transition_count;
                row.choice_offset = shared_kernel->choice_offset;
                row.choice_count = shared_kernel->choice_count;
            } else {
                if (transition_cache->successors.size() +
                        transition_cache->choice_successors.size() +
                        transition_count > options.max_transitions) {
                    throw SolverResourceLimit(
                        "max_transitions", options.max_transitions);
                }
                row.transition_offset = transition_cache->successors.size();
                reserve_selected_growth(
                    transition_cache->successors, transition_count);
                reserve_selected_growth(
                    transition_cache->probabilities, transition_count);
                for (const OutcomeEntry& entry : transitions) {
                    if (pending.entry_relative_self &&
                        entry.state == kNoId) {
                        continue;
                    }
                    transition_cache->successors.push_back(entry.state);
                    transition_cache->probabilities.push_back(
                        entry.probability);
                }
                row.transition_count = static_cast<std::uint32_t>(
                    transition_cache->successors.size() -
                    row.transition_offset);
                row.choice_offset = transition_cache->choices.size();
                row.choice_count = static_cast<std::uint32_t>(choices.size());
                reserve_selected_growth(
                    transition_cache->choices, choices.size());
                std::size_t choice_successor_count = 0;
                for (const OutcomeChoiceGroup& group : choices) {
                    choice_successor_count += group.states.size();
                }
                reserve_selected_growth(
                    transition_cache->choice_successors,
                    choice_successor_count);
                for (const OutcomeChoiceGroup& group : choices) {
                    SparseChoiceGroup stored;
                    stored.successor_offset =
                        transition_cache->choice_successors.size();
                    stored.probability = group.probability;
                    for (const std::uint32_t successor : group.states) {
                        if (is_self(successor)) {
                            stored.has_self = true;
                        } else {
                            transition_cache->choice_successors.push_back(
                                successor);
                        }
                    }
                    stored.successor_count = static_cast<std::uint32_t>(
                        transition_cache->choice_successors.size() -
                        stored.successor_offset);
                    transition_cache->choices.push_back(stored);
                }
            }
            if (row.self_probability > 0.0) {
                ++transition_cache->algebraic_self_loops;
            }
            for (std::uint32_t i = 0; i < row.choice_count; ++i) {
                if (transition_cache->choices[row.choice_offset + i]
                        .has_self) {
                    ++transition_cache->algebraic_self_loops;
                }
            }
            const std::uint64_t appended_index =
                transition_cache->rows.size();
            if (span.count == 0) {
                span.offset = appended_index;
            } else {
                transition_cache->rows.at(span.tail).next_owner_row =
                    appended_index;
            }
            span.tail = appended_index;
            reserve_selected_growth(transition_cache->rows, 1);
            transition_cache->rows.push_back(row);
            stored_row = &transition_cache->rows.back();
            if (shared_kernel == nullptr && options.kernel_reuse) {
                auto& bucket = kernel_rows_by_hash[hash];
                const std::size_t old_capacity = bucket.capacity();
                bucket.push_back(KernelRowMemo{
                    transition_cache->rows.size() - 1,
                    std::move(new_kernel_envelope)});
                owned_kernel_row_bucket_bytes +=
                    (bucket.capacity() - old_capacity) *
                    sizeof(KernelRowMemo);
                if (pending.shared_kernel_identity != nullptr) {
                    shared_kernel_rows.emplace(
                        pending.shared_kernel_identity,
                        SharedKernelMemo{
                            transition_cache->rows.size() - 1, false,
                            bucket.back().successor_envelope});
                }
            }
            ++span.count;
        }
        if (pending.shared_kernel_identity != nullptr &&
            shared_kernel_rows.find(pending.shared_kernel_identity) ==
                shared_kernel_rows.end()) {
            std::optional<CarrierSuccessorEnvelope> successor_envelope;
            if (shared_hash_memo != nullptr &&
                shared_hash_memo->successor_envelope.has_value()) {
                successor_envelope = shared_hash_memo->successor_envelope;
            } else {
                std::vector<std::uint32_t> ids;
                ids.reserve(transitions.size());
                for (const OutcomeEntry& entry : transitions) {
                    if (entry.probability != 0.0) {
                        ids.push_back(entry.state);
                    }
                }
                successor_envelope = carrier_successor_envelope(
                    calc, std::move(ids));
            }
            shared_kernel_rows.emplace(
                pending.shared_kernel_identity,
                SharedKernelMemo{
                    static_cast<std::uint64_t>(
                        stored_row - transition_cache->rows.data()),
                    false, std::move(successor_envelope)});
        }

        SparseVariant variant;
        variant.operator_index = pending.operator_index;
        variant.quantity_offset = transition_cache->variant_arena
                                      ->variant_quantities.size();
        const PricedOperator& priced = operators.at(
            static_cast<std::size_t>(
                priced_operator_position.at(pending.operator_index)));
        reserve_selected_growth(
            transition_cache->variant_arena->variant_quantities,
            priced.resource_prices.size());
        for (const auto& [key, unused_price] : priced.resource_prices) {
            (void)unused_price;
            double quantity = 0.0;
            if (pending.resources != nullptr) {
                const auto found = std::find_if(
                    pending.resources->begin(), pending.resources->end(),
                    [&](const auto& resource) {
                        return resource.first == key;
                    });
                if (found != pending.resources->end()) quantity = found->second;
            }
            transition_cache->variant_arena->variant_quantities.push_back(
                quantity);
        }
        variant.quantity_count = static_cast<std::uint32_t>(
            transition_cache->variant_arena->variant_quantities.size() -
            variant.quantity_offset);
        variant.choice_option_offset = transition_cache->choice_options.size();
        variant.choice_option_count = static_cast<std::uint32_t>(
            choice_options.size());
        reserve_selected_growth(
            transition_cache->choice_options, choice_options.size());
        for (OutcomeChoiceOption choice : choice_options) {
            if (pending.entry_relative_self && choice.state == kNoId) {
                choice.state = state;
            }
            transition_cache->choice_options.push_back(choice);
        }
        reserve_selected_growth(
            transition_cache->variant_arena->variants, 1);
        transition_cache->variant_arena->variants.push_back(variant);
        const std::uint32_t variant_index = static_cast<std::uint32_t>(
            transition_cache->variant_arena->variants.size() - 1);
        if (stored_row->variant_count == stored_row->variant_capacity) {
            const std::uint32_t grown_capacity =
                stored_row->variant_capacity == 0
                    ? 4
                    : stored_row->variant_capacity * 2;
            if (grown_capacity < stored_row->variant_capacity) {
                throw SolverResourceLimit(
                    "max_state_action_rows",
                    options.max_state_action_rows);
            }
            const std::uint64_t grown_offset =
                transition_cache->variant_arena->row_variant_indices.size();
            reserve_selected_growth(
                transition_cache->variant_arena->row_variant_indices,
                grown_capacity);
            transition_cache->variant_arena->row_variant_indices.resize(
                transition_cache->variant_arena->row_variant_indices.size() +
                    grown_capacity,
                kNoId);
            for (std::uint32_t i = 0; i < stored_row->variant_count; ++i) {
                transition_cache->variant_arena->row_variant_indices.at(
                    grown_offset + i) =
                    transition_cache->variant_arena->row_variant_indices.at(
                        stored_row->variant_offset + i);
            }
            stored_row->variant_offset = grown_offset;
            stored_row->variant_capacity = grown_capacity;
        }
        transition_cache->variant_arena->row_variant_indices.at(
            stored_row->variant_offset + stored_row->variant_count) =
            variant_index;
        ++stored_row->variant_count;

        result.diagnostics.sparse_rows = transition_cache->rows.size();
        result.diagnostics.sparse_transitions =
            transition_cache->successors.size() +
            transition_cache->choice_successors.size();
        const std::size_t stored_row_index = static_cast<std::size_t>(
            stored_row - transition_cache->rows.data());
        if (priced_rows.size() < transition_cache->rows.size()) {
            reserve_selected_growth(
                priced_rows,
                transition_cache->rows.size() - priced_rows.size());
            priced_rows.resize(transition_cache->rows.size());
        }
        /* Expansion appends exactly one new variant at this boundary. Price
         * that variant incrementally instead of rescanning every previously
         * retained variant on the row. Large equivalence rows can carry more
         * than a thousand action/resource variants; the old rescan made one
         * carrier quadratic in its admitted action count. A later economy
         * solve still rebuilds all row prices once in prepare_priced_rows(). */
        PricedSparseRow& selected = priced_rows.at(stored_row_index);
        const SparseVariant& appended_variant =
            transition_cache->variant_arena->variants.at(variant_index);
        double appended_cost = 0.0;
        if (priced_variant_cost(appended_variant, appended_cost) &&
            (appended_cost < selected.cost ||
             (appended_cost == selected.cost &&
              appended_variant.operator_index < selected.operator_index))) {
            selected.operator_index = appended_variant.operator_index;
            selected.cost = appended_cost;
            selected.choice_option_offset =
                appended_variant.choice_option_offset;
            selected.choice_option_count =
                appended_variant.choice_option_count;
        }

        bool enqueue_fringe = !focused_mode;
        if (enqueue_fringe && pending.shared_kernel_identity != nullptr) {
            SharedKernelMemo& memo =
                shared_kernel_rows.at(pending.shared_kernel_identity);
            enqueue_fringe = !memo.fringe_enqueued;
            memo.fringe_enqueued = true;
        }
        if (enqueue_fringe) {
            for (const OutcomeEntry& entry : transitions) {
                if (is_self(entry.state)) continue;
                if (pending.operator_index == restart_operator_index &&
                    !incremental_action_generation) {
                    /* Establish the executable clean-base continuation before
                     * the broad stochastic fringe. This changes only the work
                     * schedule; every strict state and all-action row remains
                     * available to the same exact focused proof. */
                    enqueue_front(entry.state);
                } else {
                    enqueue(entry.state);
                }
            }
            for (const OutcomeChoiceGroup& group : choices) {
                for (const std::uint32_t successor : group.states) {
                    if (!is_self(successor)) enqueue(successor);
                }
            }
        }
        return {
            equivalent != nullptr,
            static_cast<std::uint64_t>(stored_row_index)};
    }

ProductFractureKernel solve_detail::build_product_fracture_kernel(
        CalcContext& calc,
        const std::uint32_t state,
        const std::uint32_t relevant_goal_mask) {
    if (!calc.product_solver_parent()) {
        throw std::logic_error(
            "product Fracture quotient requested outside product parent");
    }

    const SessionImpl& session = calc.session();
    const AbstractState source = calc.state(state);
    ProductFractureKernel kernel;
    kernel.raw_affix_count =
        static_cast<std::uint32_t>(source.prefix_count) +
        static_cast<std::uint32_t>(source.suffix_count);
    if (kernel.raw_affix_count == 0) return kernel;

    std::array<std::uint32_t, kMaxGoalSlots> slot_metamod_flags{};
    std::vector<std::uint32_t> accepted_slots;
    for (std::uint32_t slot = 0; slot < calc.layout().slots.size(); ++slot) {
        const std::uint32_t bit = 1u << slot;
        if ((relevant_goal_mask & bit) == 0 ||
            source.slot_status[slot] !=
                static_cast<std::uint8_t>(GoalSlotStatus::Satisfied) ||
            (source.fractured_goal_mask & bit) != 0) {
            continue;
        }

        /*
         * The local k/n observation is valid only when this goal slot denotes
         * at most one physical affix. Prove that every pair of satisfying
         * members shares an exclusion group; otherwise a collapsed state could
         * hide multiple acceptable physical affixes and the prototype must
         * refuse rather than guess.
         */
        std::vector<std::uint32_t> members;
        pc_bitset_for_each(
            calc.layout().slots[slot].satisfying_mask.data(),
            session.words,
            [&](const std::size_t mod) {
                members.push_back(static_cast<std::uint32_t>(mod));
            });
        for (std::size_t a = 0; a < members.size(); ++a) {
            for (std::size_t b = a + 1; b < members.size(); ++b) {
                if (!session_mods_share_group(
                        session, members[a], members[b])) {
                    throw std::runtime_error(
                        "coarse product Fracture observer cannot prove one "
                        "physical affix for goal slot " +
                        std::to_string(slot));
                }
            }
        }

        /*
         * One physical affix satisfying multiple active goal slots is not
         * countable from this carrier without an explicit physical-hit
         * identity. Refuse that overlap instead of counting the affix once per
         * slot and overstating k.
         */
        for (const std::uint32_t accepted_slot : accepted_slots) {
            bool overlaps = false;
            for (std::size_t word = 0; word < session.words; ++word) {
                overlaps |=
                    (calc.layout().slots[slot].satisfying_mask[word] &
                     calc.layout()
                         .slots[accepted_slot]
                         .satisfying_mask[word]) != 0;
            }
            if (overlaps) {
                throw std::runtime_error(
                    "coarse product Fracture observer cannot recover "
                    "physical hit identity across goal slots " +
                    std::to_string(accepted_slot) + " and " +
                    std::to_string(slot));
            }
        }

        bool first_metamod = true;
        std::uint32_t uniform_metamod = 0;
        for (const std::uint32_t mod : members) {
            const std::uint32_t flag = session_metamod_flag(session, mod);
            if (first_metamod) {
                first_metamod = false;
                uniform_metamod = flag;
            } else if (flag != uniform_metamod) {
                throw std::runtime_error(
                    "coarse product Fracture observer loses metamod identity "
                    "for goal slot " +
                    std::to_string(slot));
            }
        }
        slot_metamod_flags[slot] = uniform_metamod;
        kernel.acceptable_goal_mask |= bit;
        ++kernel.acceptable_affix_count;
        accepted_slots.push_back(slot);
    }

    if (kernel.acceptable_affix_count == 0) return kernel;
    if (kernel.acceptable_affix_count > kernel.raw_affix_count) {
        throw std::runtime_error(
            "coarse product Fracture observer counted more acceptable "
            "physical affixes than explicit affixes");
    }

    kernel.hit_probability =
        1.0 / static_cast<double>(kernel.raw_affix_count);
    kernel.miss_probability =
        static_cast<double>(
            kernel.raw_affix_count - kernel.acceptable_affix_count) /
        static_cast<double>(kernel.raw_affix_count);
    kernel.exits.reserve(
        kernel.acceptable_affix_count +
        (kernel.miss_probability > 0.0 ? 1u : 0u));

    for (std::uint32_t slot = 0; slot < calc.layout().slots.size(); ++slot) {
        const std::uint32_t bit = 1u << slot;
        if ((kernel.acceptable_goal_mask & bit) == 0) continue;
        AbstractState success = source;
        success.fractured_goal_mask |= bit;
        success.flags |= kFlagFractured;
        success.fractured_metamod_flags |= slot_metamod_flags[slot];
        kernel.exits.push_back(
            {calc.intern_state(success), kernel.hit_probability});
    }

    pc_item_state fresh;
    pc_item_clear(&fresh);
    kernel.restart_state = calc.intern_item(fresh);
    if (kernel.miss_probability > 0.0) {
        kernel.exits.push_back(
            {kernel.restart_state, kernel.miss_probability});
    }
    std::sort(
        kernel.exits.begin(), kernel.exits.end(),
        [](const OutcomeEntry& lhs, const OutcomeEntry& rhs) {
            return lhs.state < rhs.state;
        });
    std::vector<OutcomeEntry> merged;
    merged.reserve(kernel.exits.size());
    for (const OutcomeEntry& exit : kernel.exits) {
        if (!merged.empty() && merged.back().state == exit.state) {
            merged.back().probability += exit.probability;
        } else {
            merged.push_back(exit);
        }
    }
    kernel.exits = std::move(merged);
    for (const OutcomeEntry& exit : kernel.exits) {
        kernel.probability_sum += exit.probability;
    }
    if (std::abs(kernel.probability_sum - 1.0) > 1e-12) {
        throw std::runtime_error(
            "coarse product Fracture probabilities do not normalize");
    }
    kernel.eligible = true;
    return kernel;
}

ProductFractureKernel SolveWork::Impl::product_fracture_kernel(
        const std::uint32_t state,
        const std::uint32_t relevant_goal_mask) {
    ProductFractureKernel kernel = build_product_fracture_kernel(
        calc, state, relevant_goal_mask);
    if (kernel.restart_state != kNoId) {
        restart_state = kernel.restart_state;
    }
    return kernel;
}

bool SolveWork::Impl::expand_one_unit() {
        const auto started = std::chrono::steady_clock::now();
        bool row_attempt_active = false;
        bool row_resource_limited = false;
        std::uint32_t row_attempt_operator = kNoId;
        std::uint32_t row_attempt_cursor = 0;
        std::uint64_t row_attempt_reforge_work = 0;
        std::uint64_t row_attempt_distribution_requests = 0;
        std::uint64_t row_attempt_distribution_hits = 0;
        std::uint64_t row_attempt_reforge_requests = 0;
        std::uint64_t row_attempt_reforge_hits = 0;
        std::chrono::steady_clock::time_point row_attempt_started;
        if (!expansion_active) {
            expansion_state = queue.front();
            queue.pop_front();
            if (expansion_state >= expanded.size()) {
                expanded.resize(expansion_state + 1, 0);
            }
            expanded[expansion_state] = 1;
            ++expanded_count;
            expansion_operator_cursor = 0;
            expansion_active = true;
            expansion_prepared = false;
            expansion_is_incremental_alternative = false;
            expansion_appended_row =
                std::numeric_limits<std::uint64_t>::max();
            expansion_operator_indices.clear();
            retain_incremental_carrier(expansion_state);
        }
        const std::uint32_t state = expansion_state;
        if (calc.is_goal_state(calc.state(state))) {
            expansion_active = false;
            result.diagnostics.expansion_ns += static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() - started)
                    .count());
            return true;
        }

        try {
            if (!expansion_prepared) {
                if (!prepare_state_expansion(
                        state, !incremental_action_generation)) {
                    result.diagnostics.expansion_ns +=
                        static_cast<std::uint64_t>(
                            std::chrono::duration_cast<
                                std::chrono::nanoseconds>(
                                std::chrono::steady_clock::now() - started)
                                .count());
                    return false;
                }
                expansion_prepared = true;
            }
            if (expansion_operator_cursor <
                expansion_operator_indices.size()) {
                const std::uint32_t operator_index =
                    expansion_operator_indices[expansion_operator_cursor++];
                const std::int32_t priced_position =
                    priced_operator_position.at(operator_index);
                if (priced_position < 0) {
                    throw std::logic_error(
                        "state-local automatic operator is not priced");
                }
                const PricedOperator& priced = operators.at(
                    static_cast<std::size_t>(priced_position));
                const PlannerOperator& planner =
                    calc.operators().at(priced.index);
                const auto row_started = std::chrono::steady_clock::now();
                const auto kernel_started = row_started;
                const CalcTelemetry search_before = calc.telemetry();
                row_attempt_active = true;
                row_attempt_operator = priced.index;
                row_attempt_cursor = expansion_operator_cursor;
                row_attempt_reforge_work =
                    search_before.reforge_frontier_work;
                row_attempt_distribution_requests =
                    search_before.distribution_requests;
                row_attempt_distribution_hits =
                    search_before.distribution_hits;
                row_attempt_reforge_requests =
                    search_before.reforge_requests;
                row_attempt_reforge_hits =
                    search_before.reforge_hits;
                row_attempt_started = row_started;
                std::uint64_t search_raw_outcomes = 0;
                std::uint64_t search_retained_transitions = 0;
                std::uint64_t search_root_retained_transitions = 0;
                std::uint64_t search_retained_bytes = 0;
                bool search_row_retained = false;
                PendingSparseRow pending;
                pending.state = state;
                pending.operator_index = priced.index;
                pending.resources = &planner.resource_quantities;
                if (expansion_is_incremental_alternative) {
                    pending.admitted = false;
                    pending.collapse_equivalent = false;
                }
                const OutcomeDistribution* primitive_distribution =
                    nullptr;
                std::optional<ProductFractureKernel> product_fracture;
                std::vector<std::pair<std::string, double>>
                    product_recovery_resources;
                std::optional<SolveTransitionCache::AutomaticCandidateRecord>
                    automatic_record;
                const AutomaticTelemetryKind telemetry_kind =
                    automatic_telemetry_kind(planner);
                if (telemetry_kind != AutomaticTelemetryKind::None) {
                    automatic_record.emplace();
                    automatic_record->state_id = state;
                    automatic_record->operator_index = priced.index;
                    automatic_record->candidate_kind =
                        planner.automatic_kind;
                    automatic_record->telemetry_kind = telemetry_kind;
                    const std::uint64_t admission_key =
                        (static_cast<std::uint64_t>(state) << 32) |
                        priced.index;
                    automatic_record->count_candidate =
                        !automatic_admission_records.contains(admission_key);
                    automatic_record->evidence.candidate = true;
                    automatic_record->evidence.relevant_goal_mask =
                        planner.relevant_goal_mask;
                }
                bool append = true;
                if (!options.allow_economic_restart &&
                    planner.kind == PlannerOperatorKind::Primitive &&
                    calc.registry().actions.at(
                        planner.primitive_action).synthetic) {
                    append = false;
                }
                if (!append) {
                    /* Economic Restart is absent. Mechanic-owned replacement
                     * recovery is composed by its exact parent kernel (for
                     * example Product Fracture miss), never as an ordinary
                     * Bellman row on the carrier. */
                } else if (planner.kind == PlannerOperatorKind::FixedOption) {
                    const OptionKernel& kernel =
                        calc.option_kernel(state, priced.index);
                    if (automatic_record.has_value()) {
                        automatic_record->evidence = kernel.automatic;
                        automatic_record->eligible =
                            kernel.supported && kernel.legal;
                        if (automatic_record->count_candidate) {
                            automatic_record->template_id =
                                kernel.retained_template_id;
                            automatic_record->template_hit =
                                calc.option_kernel_template_hit(
                                    state, priced.index);
                            automatic_record->raw_outcomes =
                                kernel.exits.size();
                            for (const OutcomeChoiceGroup& group :
                                 kernel.observation_choice_groups) {
                                automatic_record->raw_outcomes +=
                                    group.states.size();
                            }
                            if (!automatic_record->template_hit &&
                                kernel.retained_template_id != 0) {
                                automatic_record->selected_bytes +=
                                    sizeof(OptionKernel) +
                                    kernel.exits.capacity() *
                                        sizeof(OutcomeEntry) +
                                    kernel.retry_states.capacity() *
                                        sizeof(std::uint32_t) +
                                    kernel.continuation_states.capacity() *
                                        sizeof(std::uint32_t);
                            }
                        }
                    }
                    if (!kernel.supported) {
                        if (!reported_unsupported[priced.index]) {
                            reported_unsupported[priced.index] = true;
                            record_skipped_unsupported(planner.id);
                            add_action_reason(
                                "unsupported", planner.id,
                                "fixed_option_kernel_unavailable");
                        }
                        append = false;
                    } else if (!kernel.legal) {
                        append = false;
                    } else {
                        pending.transitions = &kernel.exits;
                        pending.choices =
                            &kernel.observation_choice_groups;
                        pending.choice_options =
                            &kernel.observation_choice_options;
                        pending.resources =
                            calc.is_state_local_automatic_operator(
                                priced.index)
                                ? &planner.resource_quantities
                                : &kernel.expected_resources;
                        pending.entry_relative_self = std::any_of(
                            kernel.exits.begin(), kernel.exits.end(),
                            [](const OutcomeEntry& entry) {
                                return entry.state == kNoId;
                            });
                        if (!pending.entry_relative_self) {
                            pending.entry_relative_self = std::any_of(
                                kernel.observation_choice_groups.begin(),
                                kernel.observation_choice_groups.end(),
                                [](const OutcomeChoiceGroup& group) {
                                    return std::find(
                                        group.states.begin(),
                                        group.states.end(), kNoId) !=
                                        group.states.end();
                                });
                        }
                    }
                } else {
                    const std::uint32_t action_index =
                        planner.primitive_action;
                    const AbstractState& entry = calc.state(state);
                    std::uint32_t fracture_relevant_mask = 0;
                    if (planner.automatic_kind ==
                        AutomaticCandidateKind::Fracture) {
                        for (std::uint32_t slot = 0;
                             slot < calc.layout().slots.size(); ++slot) {
                            if ((planner.relevant_goal_mask & (1u << slot)) !=
                                    0 &&
                                entry.slot_status[slot] ==
                                    static_cast<std::uint8_t>(
                                        GoalSlotStatus::Satisfied) &&
                                (entry.fractured_goal_mask & (1u << slot)) ==
                                    0) {
                                fracture_relevant_mask |= 1u << slot;
                            }
                        }
                    }
                    if (!action_legal(
                            session, calc.registry().actions[action_index],
                            entry)) {
                        append = false;
                    } else if (planner.automatic_kind ==
                                   AutomaticCandidateKind::Fracture &&
                               fracture_relevant_mask == 0) {
                        append = false;
                        OptionKernel::AutomaticEvidence& evidence =
                            automatic_record->evidence;
                        evidence.eligible = false;
                        evidence.legality_result = "irrelevant";
                        evidence.reason =
                            "no_unfractured_satisfied_goal_carrier";
                        automatic_record->eligible = false;
                    } else if (
                        calc.product_solver_parent() &&
                        planner.automatic_kind ==
                            AutomaticCandidateKind::Fracture) {
                        product_fracture = product_fracture_kernel(
                            state, fracture_relevant_mask);
                        if (!product_fracture->eligible) {
                            append = false;
                            OptionKernel::AutomaticEvidence& evidence =
                                automatic_record->evidence;
                            evidence.eligible = false;
                            evidence.legality_result = "irrelevant";
                            evidence.reason =
                                "no_unfractured_satisfied_goal_carrier";
                            automatic_record->eligible = false;
                        } else {
                            product_recovery_resources =
                                planner.resource_quantities;
                            const auto base = std::find_if(
                                product_recovery_resources.begin(),
                                product_recovery_resources.end(),
                                [](const auto& resource) {
                                    return resource.first == "base";
                                });
                            if (base == product_recovery_resources.end()) {
                                product_recovery_resources.push_back(
                                    {"base",
                                     product_fracture->miss_probability});
                            } else {
                                base->second +=
                                    product_fracture->miss_probability;
                            }
                            pending.resources =
                                &product_recovery_resources;
                            pending.transitions =
                                &product_fracture->exits;

                            OptionKernel::AutomaticEvidence& evidence =
                                automatic_record->evidence;
                            evidence.eligible = true;
                            evidence.kernel_changed = true;
                            evidence.setup_complete = true;
                            evidence.cleanup_complete = true;
                            evidence.recovery_complete = true;
                            evidence.exits_complete = true;
                            evidence.kernel_change_mechanisms =
                                kAutomaticCarrierFracture;
                            evidence.legality_result = "legal";
                            evidence.relevant_goal_mask =
                                product_fracture->acceptable_goal_mask;
                            evidence.fracture_raw_affix_count =
                                product_fracture->raw_affix_count;
                            evidence.fracture_acceptable_affix_count =
                                product_fracture->acceptable_affix_count;
                            evidence.fracture_restart_state =
                                product_fracture->restart_state;
                            evidence.fracture_hit_probability =
                                product_fracture->hit_probability;
                            evidence.fracture_miss_probability =
                                product_fracture->miss_probability;
                            evidence.fracture_probability_sum =
                                product_fracture->probability_sum;
                            evidence.reason =
                                "product_local_goal_hits_plus_priced_restart_miss";
                            automatic_record->eligible = true;
                            automatic_record->raw_outcomes =
                                product_fracture->raw_affix_count;
                        }
                    } else {
                        const OutcomeDistribution& distribution =
                            calc.outcomes(
                                state, action_index,
                                options.goal_progress_gated_reforges);
                        primitive_distribution = &distribution;
                        if (!distribution.supported) {
                            if (!reported_unsupported[priced.index]) {
                                reported_unsupported[priced.index] = true;
                                record_skipped_unsupported(planner.id);
                                add_action_reason(
                                    "unsupported", planner.id,
                                    "exact_evaluator_unavailable");
                            }
                            append = false;
                        } else if (!distribution.applicable) {
                            /* The evaluator exists, but this exact carrier
                             * makes the primitive a native not-applied
                             * operation. Reject only this row; it is not an
                             * unsupported action-family witness. */
                            append = false;
                        } else if (distribution.choice_groups.empty()) {
                            pending.transitions = &distribution.entries;
                            if (calc.registry().actions[action_index].synthetic &&
                                distribution.entries.size() == 1) {
                                restart_state = distribution.entries[0].state;
                            }
                            if (distribution.stable_shared_kernel) {
                                pending.shared_kernel_identity = &distribution;
                            }
                        } else {
                            pending.choices = &distribution.choice_groups;
                            pending.choice_options =
                                &distribution.choice_options;
                        }
                        if (append && automatic_record.has_value()) {
                            bool relevant_change = false;
                            if (planner.automatic_kind ==
                                AutomaticCandidateKind::Fracture) {
                                for (const OutcomeEntry& exit :
                                     distribution.entries) {
                                    if (exit.probability <= 0.0) continue;
                                    relevant_change |=
                                        (calc.state(exit.state)
                                             .fractured_goal_mask &
                                         fracture_relevant_mask) != 0;
                                }
                            } else {
                                /* Outcome interning may move the state table,
                                 * so reacquire the carrier afterwards. */
                                const AbstractState& source = calc.state(state);
                                for (const OutcomeEntry& exit :
                                     distribution.entries) {
                                    if (exit.probability <= 0.0) continue;
                                    const AbstractState& successor =
                                        calc.state(exit.state);
                                    for (std::uint32_t slot = 0;
                                         slot < calc.layout().slots.size();
                                         ++slot) {
                                        if ((planner.relevant_goal_mask &
                                             (1u << slot)) != 0 &&
                                            successor.slot_status[slot] >
                                                source.slot_status[slot]) {
                                            relevant_change = true;
                                        }
                                    }
                                }
                            }
                            OptionKernel::AutomaticEvidence& evidence =
                                automatic_record->evidence;
                            evidence.eligible = relevant_change;
                            evidence.kernel_changed = relevant_change;
                            evidence.setup_complete = relevant_change;
                            evidence.cleanup_complete = true;
                            evidence.recovery_complete = true;
                            evidence.exits_complete =
                                !distribution.entries.empty();
                            evidence.kernel_change_mechanisms =
                                planner.automatic_kind ==
                                        AutomaticCandidateKind::Fracture
                                    ? kAutomaticCarrierFracture
                                    : kAutomaticDeterministicFinish;
                            evidence.legality_result =
                                relevant_change ? "legal" : "irrelevant";
                            if (planner.automatic_kind ==
                                AutomaticCandidateKind::Fracture) {
                                evidence.relevant_goal_mask =
                                    fracture_relevant_mask;
                                evidence.reason = relevant_change
                                    ? "exact_goal_relevant_primitive_fracture_distribution"
                                    : "no_unfractured_satisfied_goal_carrier";
                            } else {
                                evidence.reason = relevant_change
                                    ? "legal_permanent_goal_bench_successor"
                                    : "permanent_bench_does_not_advance_goal";
                            }
                            automatic_record->eligible = relevant_change;
                            if (!relevant_change) append = false;
                        }
                    }
                }
                result.diagnostics.expansion_kernel_ns +=
                    static_cast<std::uint64_t>(
                        std::chrono::duration_cast<std::chrono::nanoseconds>(
                            std::chrono::steady_clock::now() - kernel_started)
                            .count());
                try {
                    if (append) {
                        if (automatic_record.has_value() &&
                            automatic_record->evidence.candidate_kernel_hash ==
                                0) {
                            pending.exact_kernel_hash = kernel_hash(pending);
                            automatic_record->evidence.candidate_kernel_hash =
                                static_cast<std::uint64_t>(
                                    *pending.exact_kernel_hash);
                        }
                        const std::uint64_t rows_before =
                            transition_cache->rows.size();
                        const std::uint64_t transitions_before =
                            transition_cache->successors.size() +
                            transition_cache->choice_successors.size();
                        const auto row_byte_audit_started =
                            std::chrono::steady_clock::now();
                        const std::uint64_t bytes_before =
                            transition_cache->fast_estimated_owned_bytes();
                        result.diagnostics.expansion_row_byte_audit_ns +=
                            static_cast<std::uint64_t>(
                                std::chrono::duration_cast<
                                    std::chrono::nanoseconds>(
                                    std::chrono::steady_clock::now() -
                                    row_byte_audit_started)
                                    .count());
                        const auto sparse_row_started =
                            std::chrono::steady_clock::now();
                        search_raw_outcomes =
                            (pending.transitions == nullptr
                                 ? 0
                                 : pending.transitions->size());
                        if (pending.choices != nullptr) {
                            for (const OutcomeChoiceGroup& group :
                                 *pending.choices) {
                                search_raw_outcomes += group.states.size();
                            }
                        }
                        if (product_fracture.has_value() &&
                            product_fracture->eligible) {
                            search_raw_outcomes =
                                product_fracture->raw_affix_count;
                        }
                        const auto [collapsed, appended_row] =
                            append_sparse_row(state, std::move(pending));
                        if (state == result.start_state) {
                            const SparseRow& root_row =
                                transition_cache->rows.at(appended_row);
                            search_root_retained_transitions =
                                root_row.transition_count;
                            for (std::uint32_t choice = 0;
                                 choice < root_row.choice_count; ++choice) {
                                const SparseChoiceGroup& group =
                                    transition_cache->choices.at(
                                        root_row.choice_offset + choice);
                                search_root_retained_transitions +=
                                    group.successor_count +
                                    (group.has_self ? 1u : 0u);
                            }
                        }
                        if (expansion_is_incremental_alternative) {
                            expansion_appended_row = appended_row;
                        }
                        if (incremental_action_generation) {
                            const bool chaos_anchor =
                                planner.kind ==
                                    PlannerOperatorKind::Primitive &&
                                calc.registry().actions.at(
                                    planner.primitive_action).params.type ==
                                    ActionType::Chaos;
                            const SparseRow& appended =
                                transition_cache->rows.at(appended_row);
                            std::vector<std::uint32_t> successor_ids;
                            successor_ids.reserve(
                                appended.transition_count +
                                appended.choice_count);
                            for (std::uint32_t i = 0;
                                 i < appended.transition_count; ++i) {
                                successor_ids.push_back(
                                    transition_cache->successors.at(
                                        appended.transition_offset + i));
                            }
                            for (std::uint32_t i = 0;
                                 i < appended.choice_count; ++i) {
                                const SparseChoiceGroup& group =
                                    transition_cache->choices.at(
                                        appended.choice_offset + i);
                                for (std::uint32_t s = 0;
                                     s < group.successor_count; ++s) {
                                    successor_ids.push_back(
                                        transition_cache
                                            ->choice_successors.at(
                                                group.successor_offset + s));
                                }
                            }
                            std::sort(
                                successor_ids.begin(), successor_ids.end());
                            successor_ids.erase(
                                std::unique(
                                    successor_ids.begin(),
                                    successor_ids.end()),
                                successor_ids.end());
                            if (chaos_anchor) {
                                if (incremental_chaos_support.size() <
                                    calc.state_count()) {
                                    incremental_chaos_support.resize(
                                        calc.state_count(), 0);
                                }
                                for (const std::uint32_t successor :
                                     successor_ids) {
                                    incremental_chaos_support.at(successor) =
                                        1;
                                }
                            } else if (
                                expansion_is_incremental_alternative) {
                                for (const std::uint32_t successor :
                                     successor_ids) {
                                    if (successor >=
                                            incremental_chaos_support.size() ||
                                        !incremental_chaos_support[
                                            successor]) {
                                        if (incremental_nonchaos_states_seen
                                                .size() <= successor) {
                                            incremental_nonchaos_states_seen
                                                .resize(successor + 1, 0);
                                        }
                                        if (!incremental_nonchaos_states_seen[
                                                successor]) {
                                            incremental_nonchaos_states_seen[
                                                successor] = 1;
                                            ++expansion_states_outside_chaos_support;
                                        }
                                    }
                                }
                            }
                        }
                        search_row_retained = true;
                        search_retained_transitions =
                            transition_cache->successors.size() +
                            transition_cache->choice_successors.size() -
                            transitions_before;
                        result.diagnostics.expansion_sparse_row_ns +=
                            static_cast<std::uint64_t>(
                                std::chrono::duration_cast<
                                    std::chrono::nanoseconds>(
                                    std::chrono::steady_clock::now() -
                                    sparse_row_started)
                                    .count());
                        if (product_fracture.has_value() &&
                            product_fracture->eligible) {
                            reserve_selected_growth(
                                transition_cache->product_fracture_rows, 1);
                            SolveTransitionCache::ProductFractureRowWitness
                                witness;
                            witness.source_state = state;
                            witness.row_index = appended_row;
                            witness.operator_index = priced.index;
                            witness.raw_affix_count =
                                product_fracture->raw_affix_count;
                            witness.acceptable_affix_count =
                                product_fracture->acceptable_affix_count;
                            witness.acceptable_goal_mask =
                                product_fracture->acceptable_goal_mask;
                            witness.restart_state =
                                product_fracture->restart_state;
                            witness.hit_probability =
                                product_fracture->hit_probability;
                            witness.miss_probability =
                                product_fracture->miss_probability;
                            witness.probability_sum =
                                product_fracture->probability_sum;
                            witness.restart_resource_quantity =
                                product_fracture->miss_probability;
                            witness.fracture_action_cost = priced.cost;
                            witness.restart_action_cost =
                                replacement_recovery_cost;
                            witness.restart_operator_index =
                                replacement_recovery_operator_index;
                            const auto base_price = prices.find("base");
                            witness.base_unit_cost =
                                base_price == prices.end()
                                    ? kInfinity
                                    : base_price->second;
                            witness.action_vocabulary_identity =
                                action_vocabulary_identity();
                            witness.kernel_identity =
                                automatic_record.has_value()
                                    ? automatic_record->evidence
                                          .candidate_kernel_hash
                                    : 0;
                            for (const OutcomeEntry& exit :
                                 product_fracture->exits) {
                                if (exit.state ==
                                        product_fracture->restart_state &&
                                    product_fracture->miss_probability > 0.0) {
                                    continue;
                                }
                                if (witness.hit_state_count >=
                                    witness.hit_states.size()) {
                                    throw std::logic_error(
                                        "product Fracture hit witness overflow");
                                }
                                witness.hit_states[
                                    witness.hit_state_count++] = exit.state;
                                witness.hit_probabilities[
                                    witness.hit_state_count - 1] =
                                    exit.probability;
                            }
                            transition_cache->product_fracture_rows.push_back(
                                witness);
                        }
                        const auto selected_byte_audit_started =
                            std::chrono::steady_clock::now();
                        const std::uint64_t bytes_after =
                            transition_cache->fast_estimated_owned_bytes();
                        result.diagnostics.expansion_row_byte_audit_ns +=
                            static_cast<std::uint64_t>(
                                std::chrono::duration_cast<
                                    std::chrono::nanoseconds>(
                                    std::chrono::steady_clock::now() -
                                    selected_byte_audit_started)
                                    .count());
                        search_retained_bytes = bytes_after >= bytes_before
                                                    ? bytes_after - bytes_before
                                                    : 0;
                        if (automatic_record.has_value()) {
                            automatic_record->collapsed = collapsed;
                            automatic_record->eligible = true;
                            automatic_record->retained_rows =
                                transition_cache->rows.size() - rows_before;
                            automatic_record->retained_transitions =
                                transition_cache->successors.size() +
                                transition_cache->choice_successors.size() -
                                transitions_before;
                            automatic_record->selected_bytes +=
                                search_retained_bytes;
                        }
                        if (!expansion_is_incremental_alternative) {
                            try_constructive_state_certificate(
                                state, appended_row);
                        }
                        if (!expansion_is_incremental_alternative &&
                            primitive_distribution != nullptr) {
                            try_install_gated_root_renewal_incumbent(
                                state, appended_row, priced,
                                *primitive_distribution);
                        }
                    }
                } catch (...) {
                    if (planner.kind == PlannerOperatorKind::FixedOption) {
                        calc.release_option_kernel(state, priced.index);
                    } else {
                        calc.release_outcome(
                            state, planner.primitive_action,
                            options.goal_progress_gated_reforges);
                    }
                    throw;
                }
                const std::uint64_t row_ns = static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now() - row_started)
                        .count());
                const CalcTelemetry& search_after = calc.telemetry();
                if (incremental_action_generation) {
                    const std::uint64_t requests =
                        search_after.reforge_requests -
                        search_before.reforge_requests;
                    const std::uint64_t hits =
                        search_after.reforge_hits -
                        search_before.reforge_hits;
                    incremental_unique_kernel_evaluations +=
                        requests >= hits ? requests - hits : 0;
                    incremental_carrier_kernel_reuses += hits;
                }
                const auto [search_position, search_inserted] =
                    result.diagnostics.action_search_costs.try_emplace(
                        planner.id);
                if (search_inserted) {
                    result.diagnostics.action_search_costs_owned_bytes +=
                        sizeof(std::pair<
                               const std::string,
                               SolveDiagnostics::ActionSearchCost>) +
                        search_position->first.capacity() + 1 +
                        search_position->second.last_interrupted_cap.capacity() +
                        1;
                }
                SolveDiagnostics::ActionSearchCost& search =
                    search_position->second;
                search.rows += search_row_retained ? 1 : 0;
                search.raw_outcomes += search_raw_outcomes;
                search.retained_transitions += search_retained_transitions;
                if (state == result.start_state && search_row_retained) {
                    ++search.root_rows;
                    search.root_raw_outcomes += search_raw_outcomes;
                    search.root_retained_transitions +=
                        search_root_retained_transitions;
                }
                search.reforge_work +=
                    search_after.reforge_frontier_work -
                    search_before.reforge_frontier_work;
                search.cache_requests +=
                    (search_after.distribution_requests -
                     search_before.distribution_requests) +
                    (search_after.reforge_requests -
                     search_before.reforge_requests);
                search.cache_hits +=
                    (search_after.distribution_hits -
                     search_before.distribution_hits) +
                    (search_after.reforge_hits -
                     search_before.reforge_hits);
                search.wall_ns += row_ns;
                search.retained_bytes += search_retained_bytes;
                if (planner.kind == PlannerOperatorKind::Primitive) {
                    calc.record_primitive_row_time(
                        planner.primitive_action, row_ns);
                }
                row_attempt_active = false;
                if (automatic_record.has_value()) {
                    automatic_record->row_ns = row_ns;
                    if (automatic_record->evidence.reason.empty()) {
                        automatic_record->evidence.eligible = false;
                        automatic_record->evidence.legality_result = "illegal";
                        automatic_record->evidence.reason =
                            "native_carrier_legality_refused";
                    }
                    const auto diagnostics_started =
                        std::chrono::steady_clock::now();
                    retain_automatic_candidate_record(
                        std::move(*automatic_record));
                    result.diagnostics.expansion_diagnostics_ns +=
                        static_cast<std::uint64_t>(
                            std::chrono::duration_cast<
                                std::chrono::nanoseconds>(
                                std::chrono::steady_clock::now() -
                                diagnostics_started)
                                .count());
                }
                if (expansion_is_incremental_alternative && !append) {
                    ++incremental_inapplicable_actions;
                }
                const auto release_started =
                    std::chrono::steady_clock::now();
                if (planner.kind == PlannerOperatorKind::FixedOption) {
                    calc.release_option_kernel(state, priced.index);
                } else {
                    calc.release_outcome(
                        state, planner.primitive_action,
                        options.goal_progress_gated_reforges);
                }
                result.diagnostics.expansion_release_ns +=
                    static_cast<std::uint64_t>(
                        std::chrono::duration_cast<std::chrono::nanoseconds>(
                            std::chrono::steady_clock::now() - release_started)
                            .count());
            }
        } catch (const SolverResourceLimit& limit) {
            row_resource_limited = true;
            if (expansion_is_incremental_alternative) {
                expansion_incremental_resource_limited = true;
            }
            if (row_attempt_active &&
                row_attempt_operator < calc.operators().size()) {
                const std::uint32_t operator_index =
                    row_attempt_operator;
                const std::int32_t priced_position =
                    priced_operator_position.at(operator_index);
                const PricedOperator& priced = operators.at(
                    static_cast<std::size_t>(priced_position));
                const PlannerOperator& planner =
                    calc.operators().at(priced.index);
                const CalcTelemetry& search_after = calc.telemetry();
                if (incremental_action_generation) {
                    const std::uint64_t requests =
                        search_after.reforge_requests -
                        row_attempt_reforge_requests;
                    const std::uint64_t hits =
                        search_after.reforge_hits -
                        row_attempt_reforge_hits;
                    incremental_unique_kernel_evaluations +=
                        requests >= hits ? requests - hits : 0;
                    incremental_carrier_kernel_reuses += hits;
                }
                const auto [search_position, search_inserted] =
                    result.diagnostics.action_search_costs.try_emplace(
                        planner.id);
                if (search_inserted) {
                    result.diagnostics.action_search_costs_owned_bytes +=
                        sizeof(std::pair<
                               const std::string,
                               SolveDiagnostics::ActionSearchCost>) +
                        search_position->first.capacity() + 1 +
                        search_position->second.last_interrupted_cap.capacity() +
                        1;
                }
                SolveDiagnostics::ActionSearchCost& search =
                    search_position->second;
                search.reforge_work +=
                    search_after.reforge_frontier_work -
                    row_attempt_reforge_work;
                search.cache_requests +=
                    (search_after.distribution_requests -
                     row_attempt_distribution_requests) +
                    (search_after.reforge_requests -
                     row_attempt_reforge_requests);
                search.cache_hits +=
                    (search_after.distribution_hits -
                     row_attempt_distribution_hits) +
                    (search_after.reforge_hits -
                     row_attempt_reforge_hits);
                search.wall_ns += static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now() -
                        row_attempt_started)
                        .count());
                ++search.interrupted_rows;
                search.last_interrupted_state = state;
                search.last_interrupted_operator = priced.index;
                search.last_interrupted_cursor = row_attempt_cursor;
                search.last_interrupted_root =
                    state == result.start_state;
                const std::size_t prior_cap_bytes =
                    search.last_interrupted_cap.capacity() + 1;
                search.last_interrupted_cap = limit.cap_name();
                result.diagnostics.action_search_costs_owned_bytes +=
                    search.last_interrupted_cap.capacity() + 1 -
                    std::min(
                        prior_cap_bytes,
                        search.last_interrupted_cap.capacity() + 1);
                const AutomaticTelemetryKind telemetry_kind =
                    automatic_telemetry_kind(planner);
                if (telemetry_kind != AutomaticTelemetryKind::None) {
                    SolveTransitionCache::AutomaticCandidateRecord record;
                    record.state_id = state;
                    record.operator_index = priced.index;
                    record.deferred = true;
                    record.telemetry_kind = telemetry_kind;
                    const std::uint64_t admission_key =
                        (static_cast<std::uint64_t>(state) << 32) |
                        priced.index;
                    record.count_candidate =
                        !automatic_admission_records.contains(admission_key);
                    record.evidence.candidate = true;
                    record.evidence.relevant_goal_mask =
                        planner.relevant_goal_mask;
                    record.evidence.legality_result =
                        "deferred_resource_cap";
                    record.evidence.reason =
                        "price_independent_kernel_generation_resource_cap";
                    retain_automatic_candidate_record(std::move(record));
                }
            }
            record_cap(
                limit.cap_name(),
                limit.cap_name() == "max_discovered_states");
        }
        const bool completed = row_resource_limited ||
                               expansion_operator_cursor >=
                                   expansion_operator_indices.size();
        if (completed && expansion_is_incremental_alternative) {
            if (!row_resource_limited &&
                !expansion_operator_indices.empty()) {
                incremental_completed_pairs.insert(
                    (static_cast<std::uint64_t>(state) << 32) |
                    expansion_operator_indices.front());
            }
            if (expansion_appended_row !=
                std::numeric_limits<std::uint64_t>::max()) {
                IncrementalAlternativeRow candidate;
                candidate.state = state;
                candidate.operator_index =
                    expansion_operator_indices.front();
                candidate.row_index = expansion_appended_row;
                candidate.states_added =
                    expansion_states_outside_chaos_support;
                incremental_alternative_rows.push_back(candidate);
            } else if (row_resource_limited) {
                ++incremental_resource_unresolved_actions;
            }
        }
        if (completed) expansion_active = false;
        if (completed && !result.diagnostics.resource_cap_hit &&
            expanded_count % 64 == 0) {
            const auto byte_audit_started =
                std::chrono::steady_clock::now();
            /* Preserve the 64-carrier cap cadence with the exact incremental
             * ledger. The full selected-allocation walk is now a
             * reconciliation oracle rather than the hot-path cap check. */
            if (expanded_count % 1024 == 0) {
                check_solver_byte_cap();
            } else {
                check_solver_byte_cap_fast();
            }
            result.diagnostics.expansion_cap_byte_audit_ns +=
                static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now() -
                        byte_audit_started)
                        .count());
        }
        result.diagnostics.expansion_ns += static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - started)
                .count());
        return completed;
    }

bool SolveWork::Impl::priced_variant_cost(
        const SparseVariant& variant,
        double& cost) const {
        const std::int32_t priced_position =
            priced_operator_position.at(variant.operator_index);
        if (priced_position < 0) return false;
        const PricedOperator& priced =
            operators.at(static_cast<std::size_t>(priced_position));
        if (variant.quantity_count != priced.resource_prices.size()) {
            throw std::logic_error(
                "cached solver resource vector is incompatible");
        }
        cost = 0.0;
        for (std::uint32_t quantity = 0;
             quantity < variant.quantity_count; ++quantity) {
            cost += transition_cache->variant_arena->variant_quantities.at(
                        variant.quantity_offset + quantity) *
                    priced.resource_prices[quantity].second;
        }
        return std::isfinite(cost);
    }

void SolveWork::Impl::update_priced_row(const std::size_t row_index) {
        const SparseRow& row = transition_cache->rows.at(row_index);
        PricedSparseRow selected;
        for (std::uint32_t i = 0; i < row.variant_count; ++i) {
            const SparseVariant& variant =
                transition_cache->variant_arena->variants.at(
                transition_cache->variant_arena->row_variant_indices.at(
                    row.variant_offset + i));
            if (variant.operator_index == kNoId) {
                throw std::logic_error(
                    "priced sparse row has no operator: row=" +
                    std::to_string(row_index) + " variant=" +
                    std::to_string(i) + " variant_offset=" +
                    std::to_string(row.variant_offset));
            }
            double cost = 0.0;
            if (!priced_variant_cost(variant, cost)) continue;
            if (cost < selected.cost ||
                (cost == selected.cost &&
                 variant.operator_index < selected.operator_index)) {
                selected.operator_index = variant.operator_index;
                selected.cost = cost;
                selected.choice_option_offset =
                    variant.choice_option_offset;
                selected.choice_option_count =
                    variant.choice_option_count;
            }
        }
        priced_rows.at(row_index) = selected;
    }

void SolveWork::Impl::prepare_priced_rows() {
        const std::size_t first_unpriced = 0;
        priced_rows.assign(
            transition_cache->rows.size(), PricedSparseRow{});
        for (std::size_t row_index = 0;
             row_index < transition_cache->rows.size(); ++row_index) {
            update_priced_row(row_index);
        }
        if (restart_state == kNoId && restart_operator_index != kNoId) {
            for (const SparseRow& row : transition_cache->rows) {
                bool has_restart = false;
                for (std::uint32_t i = 0; i < row.variant_count; ++i) {
                    const SparseVariant& variant =
                        transition_cache->variant_arena->variants.at(
                            transition_cache->variant_arena
                                ->row_variant_indices.at(
                                row.variant_offset + i));
                    has_restart |=
                        variant.operator_index == restart_operator_index;
                }
                if (!has_restart || row.transition_count != 1) continue;
                restart_state = transition_cache->successors.at(
                    row.transition_offset);
                break;
            }
        }
        for (std::size_t row_index = pricing_diagnostics_cursor;
             row_index < transition_cache->rows.size(); ++row_index) {
            const SparseRow& row = transition_cache->rows[row_index];
            const PricedSparseRow& selected = priced_rows[row_index];
            std::uint32_t priced_variant_count = 0;
            for (std::uint32_t i = 0; i < row.variant_count; ++i) {
                const SparseVariant& variant =
                    transition_cache->variant_arena->variants.at(
                    transition_cache->variant_arena->row_variant_indices.at(
                        row.variant_offset + i));
                if (variant.operator_index == kNoId) {
                    throw std::logic_error(
                        "pricing diagnostics row has no operator: row=" +
                        std::to_string(row_index) + " variant=" +
                        std::to_string(i) + " variant_offset=" +
                        std::to_string(row.variant_offset) +
                        " first_unpriced=" +
                        std::to_string(first_unpriced));
                }
                double unused_cost = 0.0;
                if (priced_variant_cost(variant, unused_cost)) {
                    ++priced_variant_count;
                }
            }
            if (priced_variant_count <= 1) continue;
            if (selected.operator_index == kNoId) {
                throw std::logic_error(
                    "pricing diagnostics row has no selected operator: row=" +
                    std::to_string(row_index) + " variants=" +
                    std::to_string(row.variant_count) + " priced=" +
                    std::to_string(priced_variant_count) +
                    " first_unpriced=" +
                    std::to_string(first_unpriced));
            }
            result.diagnostics.equivalent_actions_collapsed +=
                priced_variant_count - 1;
            const std::string& representative = calc.operators().at(
                selected.operator_index).id;
            for (std::uint32_t i = 0; i < row.variant_count; ++i) {
                const SparseVariant& variant =
                    transition_cache->variant_arena->variants.at(
                    transition_cache->variant_arena->row_variant_indices.at(
                        row.variant_offset + i));
                double cost = 0.0;
                if (!priced_variant_cost(variant, cost)) continue;
                const std::uint32_t operator_index = variant.operator_index;
                if (operator_index == selected.operator_index) continue;
                const std::string& candidate =
                    calc.operators().at(operator_index).id;
                if (cost == selected.cost) {
                    ++result.diagnostics.equivalent_price_ties;
                    add_action_reason(
                        "included", candidate,
                        "certified_equivalent_kernel_price_tie_with_" +
                            representative);
                } else {
                    add_action_reason(
                        "pruned", candidate,
                        "certified_equivalent_kernel_more_expensive_than_" +
                            representative);
                }
            }
        }
        pricing_diagnostics_cursor = transition_cache->rows.size();
    }

}
}
