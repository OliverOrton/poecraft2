#include "solver_solve_types.hpp"

#include "solver_action_family_contract.hpp"
#include "solver_quotient_proof.hpp"

namespace poecraft {
namespace solver {

using namespace solve_detail;

namespace solve_detail {

constexpr std::uint64_t kUpperPolicyProvenanceStructuralBytes =
    sizeof(std::vector<std::string>) +
    3 * sizeof(std::uint64_t) +
    sizeof(std::string);
/*
 * Solve-owned accounting observes the retained result through two structural
 * shells. The finalization-only provenance fields belong to neither resource
 * boundary, so remove both static footprints as well as excluding their
 * dynamic strings below.
 */
constexpr std::uint64_t kUpperPolicyProvenanceAccountingOffset =
    2 * kUpperPolicyProvenanceStructuralBytes;
/* Gate 2 keeps four source-compatible references into the typed portfolio
 * while call sites migrate. Their pointer-sized shells are not independent
 * solver-owned payload and must not perturb the legacy cap authority. */
constexpr std::uint64_t kIncumbentPortfolioAliasAccountingOffset =
    4 * sizeof(void*);

std::uint64_t SparseVariantArena::selected_bytes() const {
        return sizeof(*this) +
               variants.capacity() * sizeof(SparseVariant) +
               row_variant_indices.capacity() * sizeof(std::uint32_t) +
               variant_quantities.capacity() * sizeof(double);
    }

const char* automatic_telemetry_kind_name(
    const AutomaticTelemetryKind kind) {
    switch (kind) {
    case AutomaticTelemetryKind::Imprint: return "imprint";
    case AutomaticTelemetryKind::Renewal: return "renewal";
    case AutomaticTelemetryKind::ProtectedSide: return "protected_side";
    case AutomaticTelemetryKind::TemporaryBench: return "temporary_bench";
    case AutomaticTelemetryKind::FracturePrepare:
        return "fracture_prepare";
    case AutomaticTelemetryKind::PermanentBench: return "permanent_bench";
    case AutomaticTelemetryKind::MultimodFinish: return "multimod_finish";
    case AutomaticTelemetryKind::PrimitiveFracture:
        return "primitive_fracture";
    case AutomaticTelemetryKind::EldritchSide:
        return "eldritch_side";
    case AutomaticTelemetryKind::CannotRoll:
        return "cannot_roll";
    case AutomaticTelemetryKind::Veiled:
        return "veiled";
    case AutomaticTelemetryKind::Count:
    case AutomaticTelemetryKind::None:
        return "none";
    }
    return "none";
}

const char* primitive_telemetry_family_name(
    const PrimitiveTelemetryFamily family) {
    switch (family) {
    case PrimitiveTelemetryFamily::Currency: return "currency";
    case PrimitiveTelemetryFamily::Essence: return "essence";
    case PrimitiveTelemetryFamily::Fossil: return "fossil";
    case PrimitiveTelemetryFamily::Harvest: return "harvest";
    case PrimitiveTelemetryFamily::Bench: return "bench";
    case PrimitiveTelemetryFamily::Bestiary: return "bestiary";
    case PrimitiveTelemetryFamily::Fracture: return "fracture";
    case PrimitiveTelemetryFamily::Other: return "other";
    case PrimitiveTelemetryFamily::EldritchSetup:
        return "eldritch_setup";
    case PrimitiveTelemetryFamily::EldritchChaos:
        return "eldritch_chaos";
    case PrimitiveTelemetryFamily::EldritchAnnul:
        return "eldritch_annul";
    case PrimitiveTelemetryFamily::EldritchExalt:
        return "eldritch_exalt";
    case PrimitiveTelemetryFamily::InfluenceExalt:
        return "influence_exalt";
    case PrimitiveTelemetryFamily::VeiledChaos: return "veiled_chaos";
    case PrimitiveTelemetryFamily::VeiledExalt: return "veiled_exalt";
    case PrimitiveTelemetryFamily::Unveil: return "unveil";
    case PrimitiveTelemetryFamily::Count: return "none";
    }
    return "none";
}

}

std::size_t SolveWork::Impl::carrier_bound_operator_family(
        const std::uint32_t operator_index) const {
    using Work = CarrierBoundAttributionWork;
    const std::size_t restart =
        Work::kPrimitiveFamilyCount + Work::kAutomaticFamilyCount;
    const std::size_t other = restart + 1;
    if (operator_index == restart_operator_index) return restart;
    if (operator_index >= calc.operators().size()) return other;
    const PlannerOperator& planner = calc.operators()[operator_index];
    if (planner.kind == PlannerOperatorKind::Primitive &&
        planner.primitive_action < calc.registry().actions.size()) {
        const PrimitiveTelemetryFamily family = primitive_family_for_action(
            calc.registry().actions[planner.primitive_action].params.type);
        const std::size_t index = static_cast<std::size_t>(family);
        return index < Work::kPrimitiveFamilyCount ? index : other;
    }
    if (planner.automatic_kind != AutomaticCandidateKind::None) {
        const AutomaticTelemetryKind kind =
            automatic_telemetry_kind_for_candidate(
                planner.automatic_kind);
        const std::size_t index = static_cast<std::size_t>(kind);
        if (index < Work::kAutomaticFamilyCount) {
            return Work::kPrimitiveFamilyCount + index;
        }
    }
    return other;
}

void SolveWork::Impl::record_operator_lower_attribution(
        const std::uint32_t operator_index,
        const double lower,
        const double incumbent,
        const bool state_incumbent_prune,
        const bool constructive_prune) {
    if (!carrier_bound_attribution) return;
    auto& stats = carrier_bound_attribution->operator_lower.at(
        carrier_bound_operator_family(operator_index));
    ++stats.evaluations;
    if (std::isfinite(lower)) {
        ++stats.finite_values;
        stats.minimum_value = std::min(stats.minimum_value, lower);
        stats.maximum_value = std::max(stats.maximum_value, lower);
        if (std::isfinite(incumbent)) {
            const double margin = lower - incumbent;
            ++stats.margin_evaluations;
            stats.minimum_margin = std::min(
                stats.minimum_margin, margin);
            stats.maximum_margin = std::max(
                stats.maximum_margin, margin);
        }
    }
    if (state_incumbent_prune) ++stats.state_incumbent_prunes;
    if (constructive_prune) ++stats.constructive_prunes;
}

void SolveWork::Impl::record_operator_lower_skip(
        const std::uint32_t operator_index,
        const CarrierBoundAttributionWork::OperatorLowerSkipReason reason) {
    if (!carrier_bound_attribution) return;
    const std::size_t reason_index = static_cast<std::size_t>(reason);
    if (reason_index >=
        CarrierBoundAttributionWork::kOperatorLowerSkipReasonCount) {
        return;
    }
    ++carrier_bound_attribution->operator_lower_skips
          .at(carrier_bound_operator_family(operator_index))
          .at(reason_index);
}

void SolveWork::Impl::record_carrier_schedule_attribution(
        const CarrierBoundAttributionWork::ScheduleStage stage,
        const std::uint32_t state,
        const std::uint32_t operator_index) {
    if (!carrier_bound_attribution || state >= calc.state_count()) return;
    using Work = CarrierBoundAttributionWork;
    auto& histogram = carrier_bound_attribution->schedules.at(
        static_cast<std::size_t>(stage));
    const AbstractState& carrier = calc.state(state);
    constexpr std::uint32_t kGoalMaskLimit =
        (std::uint32_t{1} << kMaxGoalSlots) - 1;
    const std::uint32_t satisfied =
        satisfied_goal_mask_for_state(state) & kGoalMaskLimit;
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
    const std::uint32_t fractured_goals = std::min<std::uint32_t>(
        kMaxGoalSlots, std::popcount(carrier.fractured_goal_mask));
    const std::uint32_t fracture_shape = fractured_goals * 4 +
        (fractured_non_goal ? 1u : 0u) +
        (carrier.fractured_metamod_flags != 0 ? 2u : 0u);
    const std::uint32_t explicit_affixes =
        carrier.prefix_count + carrier.suffix_count;
    const std::uint32_t satisfied_count = std::popcount(satisfied);
    const std::uint32_t unrelated = std::min<std::uint32_t>(
        Work::kUnrelatedOccupancyCount - 1,
        explicit_affixes > satisfied_count
            ? explicit_affixes - satisfied_count
            : 0);

    ++histogram.total;
    ++histogram.goal_subset[satisfied];
    ++histogram.side_capacity[free_prefixes * 4 + free_suffixes];
    ++histogram.blocked_mask[blocked];
    ++histogram.protection[protection];
    ++histogram.fracture[fracture_shape];
    ++histogram.unrelated_occupancy[unrelated];
    if (stage == Work::ScheduleStage::CarrierActionAdmission &&
        operator_index != kNoId) {
        ++carrier_bound_attribution
              ->carrier_action_admissions_by_family.at(
                  carrier_bound_operator_family(operator_index));
    }
}

void SolveWork::Impl::record_upper_attribution_milestone(
        const double value,
        const bool independently_verified) {
    if (!carrier_bound_attribution || !std::isfinite(value) || value < 0.0) {
        return;
    }
    const auto capture = [&](CarrierBoundAttributionWork::UpperMilestone& out) {
        if (out.present) return;
        out.present = true;
        out.value = value;
        out.wall_ns = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() -
                carrier_bound_attribution->started_at)
                .count());
        out.discovered_states = calc.state_count();
        out.expanded_states = expanded_count;
        if (transition_cache != nullptr) {
            out.rows = transition_cache->rows.size();
            out.transitions = transition_cache->successors.size();
        }
        out.reforge_work = result.diagnostics.reforge_logical_work_v1;
    };
    capture(carrier_bound_attribution->first_finite_upper);
    if (independently_verified) {
        capture(carrier_bound_attribution->first_verified_upper);
    }
}

solve_detail::CooperativeTask<bool>
SolveWork::Impl::finalize_carrier_bound_attribution() {
    if (!carrier_bound_attribution ||
        !result.diagnostics.carrier_bound_attribution_json.empty()) {
        co_return false;
    }
    using Work = CarrierBoundAttributionWork;
    prepare_goal_cover_cost();

    struct LowerComponents {
        std::uint32_t satisfied = 0;
        std::uint32_t rejection = 0;
        bool clean_eligible = false;
        bool carrier_progress_available = false;
        bool strict_available = false;
        bool envelope_available = false;
        double universal = kInfinity;
        double clean = kInfinity;
        double carrier_progress = kInfinity;
        double terminal_debt = 0.0;
        double strict = kInfinity;
        double envelope = kInfinity;
        double selected = 0.0;
    };
    const auto components = [&](const std::uint32_t state) {
        LowerComponents out;
        out.satisfied = satisfied_goal_mask_for_state(state);
        out.rejection = clean_goal_cover_rejection_mask(state);
        out.clean_eligible = out.rejection == 0;
        const AbstractState& carrier = calc.state(state);
        out.universal = optimistic_completion_cost(out.satisfied);
        if (out.clean_eligible) {
            out.clean = optimistic_completion_cost(
                out.satisfied, true, carrier.rarity,
                carrier.prefix_count, carrier.suffix_count);
        }
        out.carrier_progress =
            carrier_goal_progress_lower_value(state);
        out.carrier_progress_available =
            std::isfinite(out.carrier_progress);
        out.terminal_debt =
            carrier_terminal_debt_lower_value(state);
        out.strict_available = !session.eldritch_eligible &&
            state < strict_clean_goal_cover_cost.size() &&
            std::isfinite(strict_clean_goal_cover_cost[state]);
        if (out.strict_available) {
            out.strict = strict_clean_goal_cover_cost[state];
        }
        out.envelope_available = state == result.start_state &&
            std::isfinite(envelope_bellman_lower);
        if (out.envelope_available) {
            out.envelope = envelope_bellman_lower;
        }
        out.selected = completion_proof_lower(state).value;
        return out;
    };
    const auto approximately_equal = [](const double left,
                                         const double right) {
        if (!std::isfinite(left) || !std::isfinite(right)) return false;
        return std::abs(left - right) <= 1e-12 *
            std::max({1.0, std::abs(left), std::abs(right)});
    };
    enum ComponentOwner : std::size_t {
        UniversalOwner = 0,
        CleanOwner,
        CarrierProgressOwner,
        TerminalDebtOwner,
        StrictOwner,
        EnvelopeOwner,
        ZeroFallbackOwner,
        OwnerCount,
    };
    const auto owners = [&](const LowerComponents& value) {
        std::array<bool, OwnerCount> result{};
        result[UniversalOwner] = approximately_equal(
            value.universal, value.selected);
        result[CleanOwner] = value.clean_eligible &&
            approximately_equal(value.clean, value.selected);
        result[CarrierProgressOwner] =
            value.carrier_progress_available &&
            approximately_equal(value.carrier_progress, value.selected);
        result[TerminalDebtOwner] = value.terminal_debt > 0.0 &&
            approximately_equal(value.terminal_debt, value.selected);
        result[StrictOwner] = value.strict_available &&
            approximately_equal(value.strict, value.selected);
        result[EnvelopeOwner] = value.envelope_available &&
            approximately_equal(value.envelope, value.selected);
        result[ZeroFallbackOwner] = value.selected == 0.0 &&
            !result[UniversalOwner] && !result[CleanOwner] &&
            !result[CarrierProgressOwner] &&
            !result[TerminalDebtOwner] && !result[StrictOwner] &&
            !result[EnvelopeOwner];
        return result;
    };
    struct Population {
        std::uint64_t states = 0;
        std::uint64_t clean_eligible = 0;
        std::uint64_t clean_ineligible = 0;
        std::uint64_t carrier_progress_available = 0;
        std::uint64_t terminal_debt_positive = 0;
        std::array<std::uint64_t,
                   Work::kCleanCoverRejectionCount> rejection{};
        std::uint64_t satisfied_nonterminal = 0;
        std::uint64_t satisfied_nonterminal_nonclean = 0;
        std::uint64_t satisfied_nonterminal_nonclean_branch_zero = 0;
        std::uint64_t satisfied_nonterminal_selected_zero = 0;
        std::uint64_t selected_zero = 0;
        std::array<std::uint64_t, OwnerCount> owner{};
    };
    struct SubsetCounts {
        std::uint64_t discovered = 0;
        std::uint64_t expanded = 0;
        std::uint64_t frontier = 0;
        std::uint64_t goal = 0;
        std::uint64_t policy_reachable = 0;
    };
    Population expanded_population;
    Population policy_population;
    std::array<SubsetCounts, Work::kGoalMaskCount> subset_counts{};
    const std::uint32_t required =
        calc.goal().required_satisfied_slots();
    const auto add_population = [&](Population& population,
                                    const std::uint32_t state,
                                    const LowerComponents& value) {
        ++population.states;
        if (value.clean_eligible) ++population.clean_eligible;
        else ++population.clean_ineligible;
        if (value.carrier_progress_available) {
            ++population.carrier_progress_available;
        }
        if (value.terminal_debt > 0.0) {
            ++population.terminal_debt_positive;
        }
        for (std::size_t bit = 0;
             bit < Work::kCleanCoverRejectionCount; ++bit) {
            if ((value.rejection & (std::uint32_t{1} << bit)) != 0) {
                ++population.rejection[bit];
            }
        }
        const bool goal = calc.is_goal_state(calc.state(state));
        const bool satisfied_nonterminal = !goal &&
            std::popcount(value.satisfied) >= required;
        if (satisfied_nonterminal) {
            ++population.satisfied_nonterminal;
            if (!value.clean_eligible) {
                ++population.satisfied_nonterminal_nonclean;
                if (value.universal == 0.0) {
                    ++population
                          .satisfied_nonterminal_nonclean_branch_zero;
                }
            }
            if (value.selected == 0.0) {
                ++population.satisfied_nonterminal_selected_zero;
            }
        }
        if (value.selected == 0.0) ++population.selected_zero;
        const auto state_owners = owners(value);
        for (std::size_t owner = 0; owner < OwnerCount; ++owner) {
            if (state_owners[owner]) ++population.owner[owner];
        }
    };

    const std::uint32_t state_count = calc.state_count();
    for (std::uint32_t state = 0; state < state_count; ++state) {
        const LowerComponents value = components(state);
        auto& subset = subset_counts.at(
            value.satisfied & (Work::kGoalMaskCount - 1));
        ++subset.discovered;
        const bool is_expanded = state < result.expanded.size() &&
            result.expanded[state] != 0;
        const bool is_goal = calc.is_goal_state(calc.state(state));
        const bool is_policy = state < result.policy_reachable.size() &&
            result.policy_reachable[state] != 0;
        if (is_expanded) ++subset.expanded;
        else ++subset.frontier;
        if (is_goal) ++subset.goal;
        if (is_policy) ++subset.policy_reachable;
        if (is_expanded) add_population(
            expanded_population, state, value);
        if (is_policy) add_population(
            policy_population, state, value);
        co_await solve_detail::CooperativeCheckpoint{};
    }

    std::vector<std::uint32_t> samples;
    samples.reserve(options.max_diagnostic_samples);
    const auto add_sample = [&](const std::uint32_t state) {
        if (state >= state_count ||
            samples.size() >= options.max_diagnostic_samples ||
            std::find(samples.begin(), samples.end(), state) !=
                samples.end()) {
            return;
        }
        samples.push_back(state);
    };
    add_sample(result.start_state);
    for (std::uint32_t state = 0; state < state_count; ++state) {
        if (state < result.policy_reachable.size() &&
            result.policy_reachable[state]) {
            add_sample(state);
        }
        if ((state + 1) % 1024 == 0) {
            co_await solve_detail::CooperativeCheckpoint{
                samples.capacity() * sizeof(std::uint32_t)};
        }
    }
    for (std::uint32_t state = 0; state < state_count; ++state) {
        if (state >= result.expanded.size() || !result.expanded[state]) {
            co_await solve_detail::CooperativeCheckpoint{
                samples.capacity() * sizeof(std::uint32_t)};
            continue;
        }
        const LowerComponents value = components(state);
        if (std::popcount(value.satisfied) >= required &&
            !calc.is_goal_state(calc.state(state))) {
            add_sample(state);
        }
        co_await solve_detail::CooperativeCheckpoint{
            samples.capacity() * sizeof(std::uint32_t)};
    }
    for (std::uint32_t state = 0; state < state_count; ++state) {
        if (state < result.expanded.size() && result.expanded[state]) {
            add_sample(state);
        }
        if ((state + 1) % 1024 == 0) {
            co_await solve_detail::CooperativeCheckpoint{
                samples.capacity() * sizeof(std::uint32_t)};
        }
    }

    static constexpr std::array<const char*,
        Work::kCleanCoverRejectionCount> kRejectionNames{{
            "invalid_state", "active_protection", "fractured_goal",
            "fractured_metamod", "influence_identity",
            "searing_identity", "eater_identity", "fractured_junk",
            "fractured_crafted_junk"}};
    static constexpr std::array<const char*, OwnerCount> kOwnerNames{{
        "universal", "clean_mdp", "carrier_progress",
        "terminal_debt", "strict_clean", "envelope_bellman",
        "zero_fallback"}};
    const auto operator_family_name = [&](const std::size_t index) {
        std::string name;
        if (index < Work::kPrimitiveFamilyCount) {
            name = primitive_telemetry_family_name(
                static_cast<PrimitiveTelemetryFamily>(index));
        } else if (index < Work::kPrimitiveFamilyCount +
                               Work::kAutomaticFamilyCount) {
            name = "automatic:";
            name += automatic_telemetry_kind_name(
                static_cast<AutomaticTelemetryKind>(
                    index - Work::kPrimitiveFamilyCount));
        } else if (index == Work::kPrimitiveFamilyCount +
                                Work::kAutomaticFamilyCount) {
            name = "restart";
        } else {
            name = "other";
        }
        return name;
    };
    const auto append_owners = [&](std::string& json,
                                   const LowerComponents& value) {
        json += '[';
        bool first = true;
        const auto state_owners = owners(value);
        for (std::size_t owner = 0; owner < OwnerCount; ++owner) {
            if (!state_owners[owner]) continue;
            if (!first) json += ',';
            first = false;
            append_json_string(json, kOwnerNames[owner]);
        }
        json += ']';
    };
    const auto append_lower = [&](std::string& json,
                                  const std::uint32_t state,
                                  const LowerComponents& value) {
        const AbstractState& carrier = calc.state(state);
        json += "{\"state\":" + std::to_string(state);
        json += ",\"expanded\":" + std::string(
            state < result.expanded.size() && result.expanded[state]
                ? "true" : "false");
        json += ",\"policy_reachable\":" + std::string(
            state < result.policy_reachable.size() &&
                    result.policy_reachable[state]
                ? "true" : "false");
        json += ",\"goal\":" + std::string(
            calc.is_goal_state(carrier) ? "true" : "false");
        json += ",\"satisfied_goal_mask\":" +
            std::to_string(value.satisfied);
        json += ",\"rarity\":" + std::to_string(carrier.rarity);
        json += ",\"prefixes\":" +
            std::to_string(carrier.prefix_count);
        json += ",\"suffixes\":" +
            std::to_string(carrier.suffix_count);
        json += ",\"blocked_mask\":" +
            std::to_string(carrier.blocked_mask);
        json += ",\"protection_flags\":" +
            std::to_string(carrier.flags & kProtectionFlags);
        json += ",\"fractured_goal_mask\":" +
            std::to_string(carrier.fractured_goal_mask);
        json += ",\"clean_eligible\":" +
            std::string(value.clean_eligible ? "true" : "false");
        json += ",\"clean_rejection_mask\":" +
            std::to_string(value.rejection);
        json += ",\"universal\":" + finite_json(value.universal);
        json += ",\"clean_mdp\":" + finite_json(value.clean);
        json += ",\"carrier_progress\":" +
            finite_json(value.carrier_progress);
        json += ",\"terminal_debt\":" +
            finite_json(value.terminal_debt);
        json += ",\"strict_clean\":" + finite_json(value.strict);
        json += ",\"envelope_bellman\":" +
            finite_json(value.envelope);
        json += ",\"selected_maximum\":" +
            finite_json(value.selected);
        json += ",\"owners\":";
        append_owners(json, value);
        json += '}';
    };
    const auto append_population = [&](std::string& json,
                                       const Population& population) {
        json += "{\"states\":" + std::to_string(population.states);
        json += ",\"clean_eligible\":" +
            std::to_string(population.clean_eligible);
        json += ",\"clean_ineligible\":" +
            std::to_string(population.clean_ineligible);
        json += ",\"carrier_progress_available\":" +
            std::to_string(population.carrier_progress_available);
        json += ",\"terminal_debt_positive\":" +
            std::to_string(population.terminal_debt_positive);
        json += ",\"clean_coverage_fraction\":" +
            finite_json(population.states == 0
                ? 0.0
                : static_cast<double>(population.clean_eligible) /
                      static_cast<double>(population.states));
        json += ",\"clean_rejections\":{";
        for (std::size_t bit = 0;
             bit < Work::kCleanCoverRejectionCount; ++bit) {
            if (bit != 0) json += ',';
            append_json_string(json, kRejectionNames[bit]);
            json += ':' + std::to_string(population.rejection[bit]);
        }
        json += "},\"satisfied_nonterminal\":{";
        json += "\"states\":" +
            std::to_string(population.satisfied_nonterminal);
        json += ",\"nonclean\":" +
            std::to_string(population.satisfied_nonterminal_nonclean);
        json += ",\"nonclean_branch_zero\":" + std::to_string(
            population.satisfied_nonterminal_nonclean_branch_zero);
        json += ",\"selected_zero\":" + std::to_string(
            population.satisfied_nonterminal_selected_zero) + '}';
        json += ",\"selected_zero_states\":" +
            std::to_string(population.selected_zero);
        json += ",\"selected_component_owners\":{";
        for (std::size_t owner = 0; owner < OwnerCount; ++owner) {
            if (owner != 0) json += ',';
            append_json_string(json, kOwnerNames[owner]);
            json += ':' + std::to_string(population.owner[owner]);
        }
        json += "}}";
    };
    const auto append_shape_histogram = [&](
        std::string& json,
        const Work::CarrierShapeHistogram& histogram) {
        json += "{\"total\":" + std::to_string(histogram.total);
        json += ",\"goal_subset\":[";
        bool first = true;
        for (std::size_t mask = 0; mask < histogram.goal_subset.size();
             ++mask) {
            if (histogram.goal_subset[mask] == 0) continue;
            if (!first) json += ',';
            first = false;
            json += "{\"mask\":" + std::to_string(mask) +
                ",\"count\":" +
                std::to_string(histogram.goal_subset[mask]) + '}';
        }
        json += "],\"side_capacity\":[";
        first = true;
        for (std::size_t index = 0; index < histogram.side_capacity.size();
             ++index) {
            if (histogram.side_capacity[index] == 0) continue;
            if (!first) json += ',';
            first = false;
            json += "{\"free_prefixes\":" +
                std::to_string(index / 4) +
                ",\"free_suffixes\":" + std::to_string(index % 4) +
                ",\"count\":" +
                std::to_string(histogram.side_capacity[index]) + '}';
        }
        json += "],\"blocked_mask\":[";
        first = true;
        for (std::size_t mask = 0; mask < histogram.blocked_mask.size();
             ++mask) {
            if (histogram.blocked_mask[mask] == 0) continue;
            if (!first) json += ',';
            first = false;
            json += "{\"mask\":" + std::to_string(mask) +
                ",\"count\":" +
                std::to_string(histogram.blocked_mask[mask]) + '}';
        }
        json += "],\"protection\":[";
        first = true;
        for (std::size_t mode = 0; mode < histogram.protection.size();
             ++mode) {
            if (histogram.protection[mode] == 0) continue;
            if (!first) json += ',';
            first = false;
            json += "{\"prefixes_locked\":" +
                std::string((mode & 1) != 0 ? "true" : "false") +
                ",\"suffixes_locked\":" +
                std::string((mode & 2) != 0 ? "true" : "false") +
                ",\"count\":" +
                std::to_string(histogram.protection[mode]) + '}';
        }
        json += "],\"fracture\":[";
        first = true;
        for (std::size_t shape = 0; shape < histogram.fracture.size();
             ++shape) {
            if (histogram.fracture[shape] == 0) continue;
            if (!first) json += ',';
            first = false;
            json += "{\"preserved_goal_count\":" +
                std::to_string(shape / 4) +
                ",\"non_goal_or_junk\":" +
                std::string((shape & 1) != 0 ? "true" : "false") +
                ",\"metamod\":" +
                std::string((shape & 2) != 0 ? "true" : "false") +
                ",\"count\":" +
                std::to_string(histogram.fracture[shape]) + '}';
        }
        json += "],\"unrelated_occupancy\":[";
        first = true;
        for (std::size_t count = 0;
             count < histogram.unrelated_occupancy.size(); ++count) {
            if (histogram.unrelated_occupancy[count] == 0) continue;
            if (!first) json += ',';
            first = false;
            json += "{\"count\":" + std::to_string(count) +
                ",\"admissions\":" + std::to_string(
                    histogram.unrelated_occupancy[count]) + '}';
        }
        json += "]}";
    };
    const auto append_milestone = [&](
        std::string& json,
        const Work::UpperMilestone& milestone) {
        if (!milestone.present) {
            json += "null";
            return;
        }
        json += "{\"value\":" + finite_json(milestone.value);
        json += ",\"wall_ns\":" + std::to_string(milestone.wall_ns);
        json += ",\"discovered_states\":" +
            std::to_string(milestone.discovered_states);
        json += ",\"expanded_states\":" +
            std::to_string(milestone.expanded_states);
        json += ",\"rows\":" + std::to_string(milestone.rows);
        json += ",\"transitions\":" +
            std::to_string(milestone.transitions);
        json += ",\"reforge_work\":" +
            std::to_string(milestone.reforge_work) + '}';
    };

    std::string json;
    json.reserve(32768);
    json += "{\"observational_only\":true";
    json += ",\"full_evidence\":true";
    json += ",\"strict_clean_enabled\":" +
        std::string(
            !session.eldritch_eligible &&
                contract(ProofPatternKind::StrictClean).converged
                ? "true"
                : "false");
    json += ",\"proof_pattern_manager\":{\"composition\":";
    append_json_string(json, "maximum_of_independently_admissible_patterns");
    json += ",\"patterns\":[";
    for (std::size_t index = 0; index < contracts.size(); ++index) {
        if (index != 0) json += ',';
        const ProofPatternContract& pattern = contracts[index];
        json += "{\"id\":";
        append_json_string(json, std::string(pattern.id));
        json += ",\"finite_projection\":";
        append_json_string(json, std::string(pattern.finite_projection));
        json += ",\"covered_action_shapes\":";
        append_json_string(
            json, std::string(pattern.covered_action_shapes));
        json += ",\"local_fallback\":";
        append_json_string(json, std::string(pattern.local_fallback));
        json += ",\"immediate_price_authority\":";
        append_json_string(
            json, std::string(pattern.immediate_price_authority));
        json += ",\"optimistic_successor_authority\":";
        append_json_string(
            json, std::string(pattern.optimistic_successor_authority));
        json += ",\"solution\":";
        switch (pattern.solution) {
        case ProofPatternSolution::AcyclicDynamicProgram:
            append_json_string(json, "acyclic_dynamic_program");
            break;
        case ProofPatternSolution::MonotoneSubsolution:
            append_json_string(json, "monotone_subsolution");
            break;
        case ProofPatternSolution::OneStepFloor:
            append_json_string(json, "one_step_floor");
            break;
        case ProofPatternSolution::ExactSuccessorComposition:
            append_json_string(json, "exact_successor_composition");
            break;
        }
        json += ",\"residual\":" + finite_json(pattern.residual);
        json += ",\"solution_sweeps\":" +
            std::to_string(pattern.solution_sweeps);
        json += ",\"converged\":" +
            std::string(pattern.converged ? "true" : "false");
        json += ",\"provenance\":";
        append_json_string(json, std::string(pattern.provenance));
        json += ",\"minimizing_action\":";
        if (pattern.minimizing_action.empty()) json += "null";
        else append_json_string(json, pattern.minimizing_action);
        json += ",\"fallback_reason\":";
        append_json_string(json, pattern.fallback_reason);
        json += ",\"refinement_trace\":";
        if (pattern.refinement_trace.empty()) json += "null";
        else append_json_string(json, pattern.refinement_trace);
        json += ",\"optimistic_grants\":";
        append_json_string(
            json, std::string(pattern.optimistic_successor_authority));
        json += ",\"start_contribution\":" +
            finite_json(pattern.start_contribution);
        json += ",\"selected_owner_calls\":" +
            std::to_string(pattern.selected_owner_calls) + '}';
    }
    json += "]}";
    json += ",\"start_lower\":";
    append_lower(json, result.start_state, components(result.start_state));
    json += ",\"populations\":{\"expanded\":";
    append_population(json, expanded_population);
    json += ",\"policy_reachable\":";
    append_population(json, policy_population);
    json += "},\"goal_subsets\":[";
    bool first = true;
    for (std::size_t mask = 0; mask < subset_counts.size(); ++mask) {
        const SubsetCounts& counts = subset_counts[mask];
        if (counts.discovered == 0) continue;
        if (!first) json += ',';
        first = false;
        json += "{\"mask\":" + std::to_string(mask);
        json += ",\"discovered\":" +
            std::to_string(counts.discovered);
        json += ",\"expanded\":" + std::to_string(counts.expanded);
        json += ",\"frontier\":" + std::to_string(counts.frontier);
        json += ",\"goal\":" + std::to_string(counts.goal);
        json += ",\"policy_reachable\":" +
            std::to_string(counts.policy_reachable) + '}';
    }
    json += "],\"lower_samples\":[";
    for (std::size_t index = 0; index < samples.size(); ++index) {
        if (index != 0) json += ',';
        append_lower(json, samples[index], components(samples[index]));
    }
    json += "],\"lower_sample_counts\":{\"retained\":" +
        std::to_string(samples.size());
    json += ",\"limit\":" +
        std::to_string(options.max_diagnostic_samples) + '}';
    json += ",\"operator_lower\":{\"families\":[";
    first = true;
    for (std::size_t family = 0;
         family < Work::kOperatorFamilyCount; ++family) {
        const auto& stats = carrier_bound_attribution->operator_lower[family];
        const auto& skips =
            carrier_bound_attribution->operator_lower_skips[family];
        const std::uint64_t pair_admissions = carrier_bound_attribution
            ->carrier_action_admissions_by_family[family];
        const std::uint64_t skip_count = std::accumulate(
            skips.begin(), skips.end(), std::uint64_t{0});
        if (stats.evaluations == 0 && pair_admissions == 0 &&
            skip_count == 0) {
            continue;
        }
        if (!first) json += ',';
        first = false;
        json += "{\"family\":";
        append_json_string(json, operator_family_name(family));
        json += ",\"evaluations\":" +
            std::to_string(stats.evaluations);
        json += ",\"finite_values\":" +
            std::to_string(stats.finite_values);
        json += ",\"margin_evaluations\":" +
            std::to_string(stats.margin_evaluations);
        json += ",\"minimum_value\":" +
            finite_json(stats.minimum_value);
        json += ",\"maximum_value\":" +
            finite_json(stats.maximum_value);
        json += ",\"minimum_lower_minus_incumbent\":" +
            finite_json(stats.minimum_margin);
        json += ",\"maximum_lower_minus_incumbent\":" +
            finite_json(stats.maximum_margin);
        json += ",\"state_incumbent_operator_lower_prunes\":" +
            std::to_string(stats.state_incumbent_prunes);
        json += ",\"constructive_certificate_prunes\":" +
            std::to_string(stats.constructive_prunes);
        json += ",\"skipped\":{\"no_finite_incumbent\":" +
            std::to_string(skips[static_cast<std::size_t>(
                Work::OperatorLowerSkipReason::NoFiniteIncumbent)]) + '}';
        json += ",\"carrier_action_admissions\":" +
            std::to_string(pair_admissions) + '}';
    }
    json += "]}";
    static constexpr std::array<const char*, Work::kScheduleStageCount>
        kScheduleNames{{
            "focused_candidates", "focused_admissions",
            "focused_ladder_admissions", "incremental_candidates",
            "incremental_carrier_admissions",
            "carrier_action_admissions"}};
    json += ",\"scheduling\":{";
    for (std::size_t stage = 0; stage < Work::kScheduleStageCount; ++stage) {
        if (stage != 0) json += ',';
        append_json_string(json, kScheduleNames[stage]);
        json += ':';
        append_shape_histogram(
            json, carrier_bound_attribution->schedules[stage]);
    }
    json += "},\"upper_milestones\":{\"first_finite\":";
    append_milestone(
        json, carrier_bound_attribution->first_finite_upper);
    json += ",\"first_independently_verified\":";
    append_milestone(
        json, carrier_bound_attribution->first_verified_upper);
    json += "}}";
    result.diagnostics.carrier_bound_attribution_json = std::move(json);
    co_return true;
}

namespace solve_detail {

std::uint64_t string_vector_owned_bytes(
    const std::vector<std::string>& values) {
    std::uint64_t total = values.capacity() * sizeof(std::string);
    for (const std::string& value : values) total += value.capacity() + 1;
    return total;
}

std::uint64_t broad_row_attribution_owned_bytes(
        const std::vector<PolicyBroadRowAttribution>& samples) {
    std::uint64_t total =
        samples.capacity() * sizeof(PolicyBroadRowAttribution);
    for (const PolicyBroadRowAttribution& sample : samples) {
        total += sample.planner_id.capacity() + 1;
        total += string_vector_owned_bytes(
            sample.primitive_program_action_ids);
        total += sample.observation_modifier_tag_ids.capacity() *
                 sizeof(std::uint32_t);
        total += sample.reforge.action_id.capacity() + 1;
        total += sample.reforge.terminal_contribution_samples.capacity() *
                 sizeof(ReforgeBuildAttribution::
                            TerminalContributionSample);
    }
    return total;
}

std::uint64_t diagnostics_owned_bytes(const SolveDiagnostics& diagnostics) {
    std::uint64_t bytes =
           string_vector_owned_bytes(diagnostics.skipped_missing_price) +
           string_vector_owned_bytes(diagnostics.skipped_unsupported) +
           string_vector_owned_bytes(diagnostics.cap_hits) +
           string_vector_owned_bytes(
               diagnostics.action_inclusion_reasons) +
           string_vector_owned_bytes(diagnostics.preservation_witnesses) +
           string_vector_owned_bytes(
               diagnostics.constructive_state_witnesses) +
           string_vector_owned_bytes(
               diagnostics.automatic_candidate_witnesses) +
           string_vector_owned_bytes(
               diagnostics.product_fracture_witnesses) +
           string_vector_owned_bytes(
               diagnostics.incremental_action_witnesses) +
           string_vector_owned_bytes(
               diagnostics.policy_refinement.counterexample_samples) +
           string_vector_owned_bytes(
               diagnostics.policy_refinement.refusal_cause_samples) +
           string_vector_owned_bytes(
               diagnostics.policy_refinement
                   .publication_candidate_samples) +
           string_vector_owned_bytes(
               diagnostics.policy_refinement
                   .structural_failure_samples) +
           string_vector_owned_bytes(
               diagnostics.policy_refinement
                   .evaluator_memory_samples) +
           diagnostics.policy_refinement.trigger_coarse_states.capacity() *
               sizeof(std::uint32_t) +
           diagnostics.policy_refinement.status.capacity() + 1 +
           diagnostics.policy_refinement.resource_cap.capacity() + 1 +
           diagnostics.policy_refinement.core_policy_status.capacity() + 1 +
           diagnostics.policy_refinement.core_policy_root_action.capacity() +
               1 +
           diagnostics.policy_refinement
                   .direct_certification_status.capacity() +
               1 +
           diagnostics.policy_refinement
                   .direct_certification_failure_reason.capacity() +
               1 +
           diagnostics.policy_refinement
                   .direct_certification_failure_classification.capacity() +
               1 +
           diagnostics.policy_refinement
                   .direct_certification_resource_cap.capacity() +
               1 +
           diagnostics.policy_refinement
                   .direct_certification_route_default_mode.capacity() +
               1 +
           diagnostics.policy_refinement
                   .direct_product_route_default_mode.capacity() +
               1 +
           diagnostics.policy_refinement.strict_lift_status.capacity() + 1 +
           diagnostics.policy_refinement
                   .strict_lift_failure_reason.capacity() +
               1 +
           diagnostics.policy_refinement.strict_lift_resource_cap.capacity() +
               1 +
           diagnostics.policy_refinement.publication_status.capacity() + 1 +
           diagnostics.policy_refinement.published_candidate_kind.capacity() +
               1 +
           diagnostics.policy_refinement
                   .published_fallback_kind.capacity() +
               1 +
           diagnostics.policy_refinement
                   .preferred_publication_failure_reason.capacity() +
               1 +
           diagnostics.policy_refinement
                   .selected_candidate_status.capacity() +
               1 +
           diagnostics.policy_refinement
                   .selected_candidate_failure_reason.capacity() +
               1 +
           broad_row_attribution_owned_bytes(
               diagnostics.policy_refinement.broad_row_attribution) +
           diagnostics.policy_refinement
                   .strict_reforge_row_samples.capacity() *
               sizeof(ReforgeRowTelemetry) +
           string_vector_owned_bytes(diagnostics.equivalence_witnesses) +
           diagnostics.focused_schedule_rounds.capacity() *
               sizeof(FocusedScheduleRoundTelemetry) +
           diagnostics.solution_scope.capacity() + 1 +
           diagnostics.policy_evaluation_failure.capacity() + 1 +
           diagnostics.policy_compatibility_action.capacity() + 1 +
           diagnostics.policy_compatibility_reason.capacity() + 1 +
           diagnostics.policy_publication_failure_reason.capacity() + 1 +
           diagnostics.incumbent_kind.capacity() + 1 +
           diagnostics.destructive_renewal_action_id.capacity() + 1 +
           diagnostics.progressive_fracture_roll_action_id.capacity() + 1 +
            diagnostics.progressive_fracture_status.capacity() + 1 +
            diagnostics.action_envelope_ledger_json.capacity() + 1 +
            diagnostics.anytime_scheduler_json.capacity() + 1 +
            diagnostics.incumbent_portfolio.candidate_source.capacity() + 1 +
           diagnostics.incumbent_portfolio.candidate_stage.capacity() + 1 +
           diagnostics.carrier_bound_attribution_json.capacity() + 1;
    bytes += diagnostics.action_search_costs_owned_bytes;
    for (const auto& [id, unused] :
         diagnostics.lower_policy_action_states) {
        (void)unused;
        bytes += sizeof(std::pair<const std::string, std::uint64_t>) +
                 id.capacity() + 1;
    }
    for (const auto& [id, unused] :
         diagnostics.upper_policy_action_states) {
        (void)unused;
        bytes += sizeof(std::pair<const std::string, std::uint64_t>) +
                 id.capacity() + 1;
    }
    return bytes;
}

std::uint64_t solve_result_owned_bytes(const SolveResult& result) {
    std::uint64_t bytes =
        sizeof(result) -
        kUpperPolicyProvenanceAccountingOffset;
    bytes += result.values.capacity() * sizeof(double);
    bytes += result.policy.capacity() * sizeof(PolicyOperatorRef);
    bytes += result.expanded.capacity() * sizeof(std::uint8_t);
    bytes += result.goal_states.capacity() * sizeof(std::uint8_t);
    bytes += result.policy_reachable.capacity() * sizeof(std::uint8_t);
    bytes += result.unveil_preferences.capacity() *
             sizeof(std::vector<std::uint32_t>);
    for (const auto& preferences : result.unveil_preferences) {
        bytes += preferences.capacity() * sizeof(std::uint32_t);
    }
    bytes += result.option_unveil_preferences.capacity() *
             sizeof(std::vector<ObservedUnveilPreference>);
    for (const auto& preferences : result.option_unveil_preferences) {
        bytes += preferences.capacity() * sizeof(ObservedUnveilPreference);
        for (const ObservedUnveilPreference& preference : preferences) {
            bytes += preference.choices.capacity() *
                     sizeof(ObservedUnveilChoice);
        }
    }
    bytes += result.behavioral_representative_by_state.capacity() *
             sizeof(std::uint32_t);
    bytes += result.primitive_renewal_witness.kernel_signature.capacity() *
             sizeof(std::uint64_t);
    bytes += result.refined_policy_artifact.strategy_json.capacity() + 1;
    bytes += result.refined_policy_artifact
                 .certification_strategy_json.capacity() + 1;
    bytes += result.refined_policy_artifact
                 .policy_route_default_mode.capacity() + 1;
    bytes += result.refined_policy_artifact
                 .certification_policy_route_default_mode.capacity() + 1;
    bytes += diagnostics_owned_bytes(result.diagnostics);
    return bytes;
}

}

 std::uint64_t SolveTransitionCache::automatic_sample_nested_bytes(
        const AutomaticCandidateRecord& record) {
        return record.evidence.legality_result.capacity() +
               record.evidence.reason.capacity() +
               record.candidate_id.capacity() +
               record.setup_action_id.capacity() +
               record.followup_action_id.capacity() +
               record.cleanup_action_id.capacity();
    }

std::uint64_t SolveTransitionCache::shallow_estimated_owned_bytes() const {
        std::uint64_t bytes = sizeof(*this);
        bytes += operator_indices.capacity() * sizeof(std::uint32_t);
        bytes += expanded.capacity() * sizeof(std::uint8_t);
        bytes += behavioral_representative_by_state.capacity() *
                 sizeof(std::uint32_t);
        bytes += state_rows.capacity() * sizeof(StateRowSpan);
        bytes += rows.capacity() * sizeof(SparseRow);
        if (quotient_proofs != nullptr) {
            bytes += quotient_proofs->ledger().snapshot().total_bytes;
        }
        if (accounts_variant_arena && variant_arena != nullptr) {
            bytes += variant_arena->selected_bytes();
        }
        bytes += successors.capacity() * sizeof(std::uint32_t);
        bytes += probabilities.capacity() * sizeof(double);
        bytes += choices.capacity() * sizeof(SparseChoiceGroup);
        bytes += choice_successors.capacity() * sizeof(std::uint32_t);
        bytes += choice_options.capacity() * sizeof(OutcomeChoiceOption);
        bytes += automatic_candidate_samples.capacity() *
                 sizeof(AutomaticCandidateRecord);
        bytes += product_fracture_rows.capacity() *
                 sizeof(ProductFractureRowWitness);
        return bytes;
    }

std::uint64_t SolveTransitionCache::fast_estimated_owned_bytes() const {
        return shallow_estimated_owned_bytes() +
               owned_automatic_sample_nested_bytes;
    }

std::uint64_t SolveTransitionCache::audited_estimated_owned_bytes() const {
        std::uint64_t bytes = shallow_estimated_owned_bytes();
        for (const AutomaticCandidateRecord& record :
             automatic_candidate_samples) {
            bytes += automatic_sample_nested_bytes(record);
        }
        return bytes;
    }

std::uint64_t SolveTransitionCache::estimated_owned_bytes() const {
        return audited_estimated_owned_bytes();
    }

SolveProgress SolveWork::Impl::progress() const {
        SolveProgress value;
        value.phase = phase;
        value.done = phase == SolvePhase::Done;
        value.expanded_states = expanded_count;
        value.sweeps = sweeps;
        value.residual = residual;
        value.finalization_work_items = finalization_work_items;
        value.refinement_states = finalization_refinement_states;
        value.refinement_kernels = finalization_refinement_kernels;
        value.refinement_transitions =
            finalization_refinement_transitions;
        value.refinement_rounds = finalization_refinement_rounds;
        value.refinement_classes = finalization_refinement_classes;
        value.certification_discovered_pairs =
            finalization_evaluation_progress.discovered_pairs;
        value.certification_pending_pairs =
            finalization_evaluation_progress.pending_pairs;
        value.certification_solved_sccs =
            finalization_evaluation_progress.solved_sccs;
        value.certification_total_sccs =
            finalization_evaluation_progress.total_sccs;
        if (finalized_result.has_value()) {
            const SolveResult& finalized = *finalized_result;
            value.start_value_bound =
                finalized.start_state < finalized.values.size()
                    ? finalized.values[finalized.start_state]
                    : finalized.upper_bound;
            value.lower_bound = finalized.lower_bound;
            value.upper_bound = finalized.upper_bound;
            value.absolute_optimality_gap =
                finalized.absolute_optimality_gap;
            value.relative_optimality_gap =
                finalized.relative_optimality_gap;
            value.focused_round =
                finalized.diagnostics.focused_expansion_rounds;
            value.incumbent_kind =
                finalized.diagnostics.incumbent_kind;
            value.discovered_states =
                finalized.diagnostics.discovered_states;
            value.frontier_states =
                finalized.diagnostics.frontier_states;
            value.state_action_rows =
                finalized.diagnostics.sparse_rows;
            value.transition_entries =
                finalized.diagnostics.sparse_transitions;
            value.reforge_work =
                finalized.diagnostics.reforge_logical_work_v1;
            value.live_owned_bytes = fast_estimated_owned_bytes();
            value.peak_owned_bytes = std::max(
                peak_owned_bytes, value.live_owned_bytes);
            return value;
        }
        value.start_value_bound = kValueCeiling;
        if (!result.values.empty() &&
            result.start_state < result.values.size()) {
            value.start_value_bound = result.values[result.start_state];
        }
        if (result.diagnostics.focused_expansion) {
            value.lower_bound = certified_global_lower_bound();
            value.upper_bound =
                incumbent_portfolio.verified_executable_upper();
        } else {
            value.lower_bound = 0.0;
            value.upper_bound =
                incumbent_portfolio.verified_executable_upper();
            bool full_non_goal_closure = value.done;
            if (full_non_goal_closure) {
                for (std::uint32_t state = 0;
                     state < result.values.size(); ++state) {
                    if (!result.behavioral_representative_by_state.empty() &&
                        result.behavioral_representative_by_state[state] !=
                            state) {
                        continue;
                    }
                    if (!result.expanded[state] &&
                        !calc.is_goal_state(calc.state(state))) {
                        full_non_goal_closure = false;
                        break;
                    }
                }
            }
            const bool exact_closed =
                full_non_goal_closure && !target_gap_stop &&
                !result.diagnostics.state_cap_hit &&
                !result.diagnostics.resource_cap_hit &&
                (!incremental_action_generation ||
                 incremental_envelope_closed) &&
                optimization_converged() &&
                value.start_value_bound < kValueCeiling;
            if (exact_closed) {
                value.lower_bound = value.start_value_bound;
            }
        }
        if (std::isfinite(value.lower_bound) &&
            std::isfinite(value.upper_bound)) {
            value.absolute_optimality_gap = std::max(
                0.0, value.upper_bound - value.lower_bound);
            if (value.lower_bound > 0.0) {
                value.relative_optimality_gap = std::max(
                    0.0,
                    value.upper_bound / value.lower_bound - 1.0);
            }
        }
        value.focused_round = result.diagnostics.focused_expansion_rounds;
        value.incumbent_kind = result.diagnostics.incumbent_kind;
        value.discovered_states = calc.state_count();
        value.frontier_states = value.discovered_states >= expanded_count
                                    ? value.discovered_states - expanded_count
                                    : 0;
        value.state_action_rows = transition_cache == nullptr
                                      ? 0
                                      : transition_cache->rows.size();
        value.transition_entries = transition_cache == nullptr
                                       ? 0
                                       : transition_cache->successors.size() +
                                             transition_cache
                                                 ->choice_successors.size();
        value.reforge_work = value.done
                                 ? result.diagnostics
                                       .reforge_logical_work_v1
                                 : calc.telemetry()
                                       .reforge_logical_work_v1;
        /* Progress is queried after every bounded C/WASM step. Before
         * finalization only the coarse child has run; the completed snapshot
         * uses the aggregate coarse/strict/evaluation logical audit. */
        value.live_owned_bytes = fast_estimated_owned_bytes();
        value.peak_owned_bytes = std::max(
            peak_owned_bytes, value.live_owned_bytes);
        return value;
    }

void SolveWork::Impl::refresh_incumbent_portfolio_diagnostics(
        SolveDiagnostics& diagnostics,
        const SolveResult* published) const {
    SolveDiagnostics::IncumbentPortfolioSnapshot& snapshot =
        diagnostics.incumbent_portfolio;
    snapshot = {};

    const BoundedPolicyIncumbent* candidate = nullptr;
    double candidate_estimate = kInfinity;
    if (unverified_selected_policy_candidate.has_value()) {
        candidate = &unverified_selected_policy_candidate->snapshot;
        candidate_estimate =
            unverified_selected_policy_candidate->selected_estimate;
    } else if (output_incumbent.has_value()) {
        candidate = &*output_incumbent;
        candidate_estimate = candidate->certified_upper_bound;
    }
    if (candidate != nullptr) {
        snapshot.candidate_present = true;
        snapshot.candidate_estimate = candidate_estimate;
        snapshot.candidate_source = candidate->kind;
        snapshot.candidate_identity = {
            candidate->portfolio_identity,
            candidate->goal_identity,
            candidate->economy_identity,
            candidate->action_vocabulary_identity,
            candidate->caller_scope_identity,
            candidate->graph_prefix_identity,
            candidate->artifact_identity,
            candidate->source_generation};
        snapshot.candidate_verified =
            candidate->independently_certified &&
            candidate->independently_evaluated && candidate->proper &&
            candidate->executable;
        if (snapshot.candidate_verified) {
            snapshot.candidate_stage = "exact_evaluated_policy";
        } else if (!candidate->compiled_artifact.strategy_json.empty()) {
            snapshot.candidate_stage = "compiled_artifact";
        } else if (candidate->policy_materialized) {
            snapshot.candidate_stage = "materialized_policy";
        } else {
            snapshot.candidate_stage = "coarse_estimate";
        }
    } else if (result.start_state < result.values.size() &&
               std::isfinite(result.values[result.start_state])) {
        snapshot.candidate_present = true;
        snapshot.candidate_estimate = result.values[result.start_state];
        snapshot.candidate_source = "bellman_selected_policy";
        snapshot.candidate_stage = "coarse_estimate";
    }

    snapshot.verified_executable_upper =
        incumbent_portfolio.verified_executable_upper();
    snapshot.verified_upper_present =
        std::isfinite(snapshot.verified_executable_upper);
    snapshot.verified_portfolio_identity =
        incumbent_portfolio.best_verified_identity;
    snapshot.verified_identity = {
        incumbent_portfolio.best_verified_identity,
        incumbent_portfolio.best_verified_goal_identity,
        incumbent_portfolio.best_verified_economy_identity,
        incumbent_portfolio.best_verified_action_vocabulary_identity,
        incumbent_portfolio.best_verified_caller_scope_identity,
        incumbent_portfolio.best_verified_graph_prefix_identity,
        incumbent_portfolio.best_verified_artifact_identity,
        incumbent_portfolio.best_verified_source_generation};
    snapshot.verified_observations =
        incumbent_portfolio.verified_observations;
    snapshot.verified_replacements =
        incumbent_portfolio.verified_replacements;
    snapshot.verified_upper_monotone =
        !incumbent_portfolio.monotonicity_violation;

    snapshot.independent_global_lower = certified_global_lower_bound();
    snapshot.independent_global_lower_certified = true;
    snapshot.independent_global_lower_provenance =
        snapshot.independent_global_lower > 0.0
            ? SolveLowerBoundProvenance::GlobalActionRelaxation
            : SolveLowerBoundProvenance::
                  OpenIncrementalEnvelopeUniversalZero;
    snapshot.restricted_search_lower = diagnostics.focused_lower_bound;
    snapshot.restricted_search_envelope_global =
        !diagnostics.incremental_action_generation ||
        diagnostics.incremental_action_envelope_closed;
    if (published != nullptr) {
        snapshot.independent_global_lower = published->lower_bound;
        snapshot.independent_global_lower_certified =
            published->global_lower_bound_certified;
        snapshot.independent_global_lower_provenance =
            published->lower_bound_provenance;
        snapshot.exact_closure_proved =
            published->lower_bound_provenance ==
            SolveLowerBoundProvenance::ExactPolicyClosure;
        if (snapshot.exact_closure_proved) {
            snapshot.exact_closure_value = published->lower_bound;
        }
        if (published->policy_available &&
            std::isfinite(published->evaluated_policy_cost)) {
            snapshot.verified_upper_present = true;
            snapshot.verified_executable_upper = std::min(
                snapshot.verified_executable_upper,
                published->evaluated_policy_cost);
        }
    }
}

SolveTelemetrySnapshot SolveWork::Impl::telemetry_snapshot(bool abandoned) const {
        SolveTelemetrySnapshot snapshot;
        snapshot.phase = phase;
        snapshot.abandoned = abandoned;
        snapshot.diagnostics = result.diagnostics;
        snapshot.diagnostics.expanded_states = expanded_count;
        snapshot.diagnostics.discovered_states = calc.state_count();
        snapshot.diagnostics.frontier_states =
            snapshot.diagnostics.discovered_states - expanded_count;
        snapshot.diagnostics.goal_states = 0;
        for (std::uint32_t state = 0; state < calc.state_count(); ++state) {
            if (!result.behavioral_representative_by_state.empty() &&
                result.behavioral_representative_by_state[state] != state) {
                continue;
            }
            if (calc.is_goal_state(calc.state(state))) {
                ++snapshot.diagnostics.goal_states;
            }
        }
        snapshot.diagnostics.sweeps = sweeps;
        snapshot.diagnostics.residual = residual;
        snapshot.diagnostics.automatic_rows_considered =
            transition_cache->automatic_rows_considered;
        snapshot.diagnostics.automatic_rows_eligible =
            transition_cache->automatic_rows_eligible;
        snapshot.diagnostics.automatic_rows_rejected =
            transition_cache->automatic_rows_rejected;
        snapshot.diagnostics.automatic_rows_collapsed =
            transition_cache->automatic_rows_collapsed;
        snapshot.diagnostics.automatic_rows_deferred =
            transition_cache->automatic_rows_deferred;
        snapshot.diagnostics.automatic_kind_telemetry =
            transition_cache->automatic_kind_telemetry;
        snapshot.diagnostics.automatic_admission_phases =
            transition_cache->automatic_admission_phases;
        snapshot.diagnostics.automatic_candidate_witnesses_omitted =
            snapshot.diagnostics.automatic_rows_considered;
        snapshot.diagnostics.product_fracture_rows =
            transition_cache->product_fracture_rows.size();
        snapshot.diagnostics.product_fracture_raw_outcomes = 0;
        snapshot.diagnostics.product_fracture_hit_entries = 0;
        snapshot.diagnostics.product_fracture_miss_entries = 0;
        snapshot.diagnostics
            .product_fracture_parent_miss_states_interned = 0;
        snapshot.diagnostics.product_fracture_max_probability_error = 0.0;
        snapshot.diagnostics.product_fracture_witnesses.clear();
        for (const auto& witness :
             transition_cache->product_fracture_rows) {
            snapshot.diagnostics.product_fracture_raw_outcomes +=
                witness.raw_affix_count;
            snapshot.diagnostics.product_fracture_hit_entries +=
                witness.hit_state_count;
            snapshot.diagnostics.product_fracture_miss_entries +=
                witness.miss_probability > 0.0 ? 1 : 0;
            snapshot.diagnostics
                .product_fracture_parent_miss_states_interned +=
                witness.parent_miss_state_count;
            snapshot.diagnostics
                .product_fracture_max_probability_error =
                std::max(
                    snapshot.diagnostics
                        .product_fracture_max_probability_error,
                    std::abs(witness.probability_sum - 1.0));
        }
        snapshot.diagnostics.product_fracture_witnesses_omitted =
            snapshot.diagnostics.product_fracture_rows;
        snapshot.diagnostics.incremental_action_generation =
            incremental_action_generation;
        snapshot.diagnostics.incremental_action_envelope_closed =
            !incremental_action_generation || incremental_envelope_closed;
        snapshot.diagnostics.incremental_actions_unevaluated =
            incremental_unevaluated_actions;
        snapshot.diagnostics.incremental_actions_evaluating =
            expansion_active && expansion_is_incremental_alternative ? 1 : 0;
        snapshot.diagnostics.incremental_actions_unresolved =
            incremental_resource_unresolved_actions;
        snapshot.diagnostics.incremental_actions_inapplicable =
            incremental_inapplicable_actions;
        snapshot.diagnostics.incremental_actions_admitted = 0;
        snapshot.diagnostics.incremental_actions_non_improving = 0;
        snapshot.diagnostics
            .incremental_states_outside_chaos_support = 0;
        for (const IncrementalAlternativeRow& candidate :
             incremental_alternative_rows) {
            if (candidate.status ==
                IncrementalAlternativeRow::Status::Admitted) {
                ++snapshot.diagnostics.incremental_actions_admitted;
            } else if (candidate.status ==
                       IncrementalAlternativeRow::Status::NonImproving) {
                ++snapshot.diagnostics
                    .incremental_actions_non_improving;
            } else {
                ++snapshot.diagnostics.incremental_actions_unresolved;
            }
            snapshot.diagnostics
                .incremental_states_outside_chaos_support +=
                candidate.states_added;
        }
        snapshot.diagnostics.incremental_unique_kernel_evaluations =
            incremental_unique_kernel_evaluations;
        snapshot.diagnostics.incremental_carrier_kernel_reuses =
            incremental_carrier_kernel_reuses;
        snapshot.diagnostics.incremental_carrier_ladder_epochs =
            incremental_carrier_ladder_epochs;
        snapshot.diagnostics.incremental_carrier_ladder_candidates =
            incremental_carrier_ladder_candidates;
        snapshot.diagnostics.incremental_carrier_ladder_goal_subsets =
            incremental_carrier_ladder_goal_subsets;
        snapshot.diagnostics.incremental_bellman_reoptimizations =
            incremental_reoptimizations;
        snapshot.diagnostics
            .incremental_first_alternative_expanded_states =
            incremental_first_alternative_expanded_states;
        snapshot.diagnostics.incremental_refinement_rounds =
            incremental_refinement_rounds;
        snapshot.diagnostics.incremental_refinement_states_selected =
            incremental_refinement_states_selected;
        snapshot.diagnostics.incremental_rows_reconsidered =
            incremental_rows_reconsidered;
        snapshot.diagnostics.incremental_upper_policy_updates =
            incremental_upper_policy_updates;
        snapshot.diagnostics
            .incremental_upper_policy_passes_requested =
            incremental_upper_policy_passes_requested;
        snapshot.diagnostics
            .incremental_upper_policy_passes_started =
            incremental_upper_policy_passes_started;
        snapshot.diagnostics
            .incremental_upper_policy_passes_proper =
            incremental_upper_policy_passes_proper;
        snapshot.diagnostics
            .incremental_upper_policy_passes_rejected =
            incremental_upper_policy_passes_rejected;
        snapshot.diagnostics
            .incremental_upper_policy_fixed_policy_proofs =
            incremental_upper_policy_fixed_policy_proofs;
        snapshot.diagnostics.incremental_upper_policy_last_failure =
            incremental_upper_policy_last_failure;
        snapshot.diagnostics.incremental_anytime_policy_attempts =
            incremental_anytime_policy_attempts;
        snapshot.diagnostics.incremental_anytime_policy_successes =
            incremental_anytime_policy_successes;
        snapshot.diagnostics.incremental_anytime_policy_last_completed_rows =
            incremental_anytime_policy_last_completed_rows;
        snapshot.diagnostics.incremental_anytime_policy_best_upper =
            incremental_anytime_policy_best_upper;
        snapshot.diagnostics.incremental_anytime_policy_last_failure =
            incremental_anytime_policy_last_failure;
        snapshot.diagnostics.incremental_refinement_uncertainty =
            incremental_refinement_uncertainty;
        refresh_action_envelope_ledger_diagnostics(snapshot.diagnostics);
        refresh_anytime_scheduler_diagnostics(snapshot.diagnostics);
        refresh_incumbent_portfolio_diagnostics(
            snapshot.diagnostics,
            finalized_result.has_value() ? &*finalized_result : nullptr);
        snapshot.diagnostics.solve_owned_byte_ledger_requests =
            owned_byte_ledger_requests;
        snapshot.diagnostics.solve_owned_byte_reconciliations =
            owned_byte_reconciliations;
        snapshot.diagnostics.solve_owned_byte_ledger_max_overestimate =
            owned_byte_ledger_max_overestimate;
        const std::uint64_t live_bytes = estimated_owned_bytes();
        snapshot.diagnostics.solver_owned_bytes_estimate =
            std::max(peak_owned_bytes, live_bytes);
        snapshot.diagnostics.solver_live_owned_bytes_estimate = live_bytes;
        snapshot.diagnostics.diagnostics_retained_bytes_estimate =
            diagnostics_owned_bytes(snapshot.diagnostics);
        snapshot.raw_start_bound = progress().start_value_bound;
        return snapshot;
    }

std::uint64_t SolveWork::Impl::estimated_owned_bytes() const {
        return estimated_owned_bytes_with_calc(
            calc.estimated_owned_bytes());
    }

std::uint64_t SolveWork::Impl::audited_estimated_owned_bytes() const {
        const std::uint64_t calc_bytes =
            calc.audited_estimated_owned_bytes();
        const std::uint64_t audited =
            estimated_owned_bytes_with_calc(calc_bytes);
        const std::uint64_t fast =
            fast_estimated_owned_bytes_with_calc(calc.fast_estimated_owned_bytes());
        ++owned_byte_reconciliations;
        if (fast < audited) {
            throw std::logic_error(
                "incremental SolveWork selected-owned-byte ledger "
                "undercounted by " + std::to_string(audited - fast) +
                " bytes (audited=" + std::to_string(audited) +
                ", ledger=" + std::to_string(fast) +
                ", calc=" + std::to_string(calc_bytes) +
                ", transition_cache_audited=" +
                std::to_string(
                    transition_cache == nullptr
                        ? 0
                        : transition_cache->audited_estimated_owned_bytes()) +
                ", transition_cache_ledger=" +
                std::to_string(
                    transition_cache == nullptr
                        ? 0
                        : transition_cache->fast_estimated_owned_bytes()) +
                ", operator_nested=" +
                std::to_string(owned_operators_nested_bytes) +
                ", kernel_bucket_nested=" +
                std::to_string(owned_kernel_row_bucket_bytes) +
                ", result_nested=" +
                std::to_string(owned_result_nested_bytes) + ")");
        }
        owned_byte_ledger_max_overestimate = std::max(
            owned_byte_ledger_max_overestimate, fast - audited);
        return audited;
    }

std::uint64_t SolveWork::Impl::output_incumbent_owned_bytes() const {
        std::uint64_t bytes = certified_fallback_portfolio.capacity() *
            sizeof(BoundedPolicyIncumbent);
        if (output_incumbent.has_value()) {
            /* std::optional owns its inline object inside Impl; only the
             * selected dynamic allocations are additional live storage. */
            bytes += incumbent_owned_bytes(*output_incumbent) -
                sizeof(BoundedPolicyIncumbent);
        }
        if (unverified_selected_policy_candidate.has_value()) {
            /* The wrapper and nested incumbent are inline in Impl. */
            bytes += incumbent_owned_bytes(
                         unverified_selected_policy_candidate->snapshot) -
                sizeof(BoundedPolicyIncumbent);
        }
        for (const BoundedPolicyIncumbent& incumbent :
             certified_fallback_portfolio) {
            bytes += incumbent_owned_bytes(incumbent) -
                sizeof(BoundedPolicyIncumbent);
        }
        return bytes;
    }

std::uint64_t SolveWork::Impl::fallback_policy_dynamic_owned_bytes(
        const FocusedFallbackPolicy& fallback) const {
        std::uint64_t bytes = fallback.renewal_kernel_signature.capacity() *
            sizeof(std::uint64_t);
        bytes += fallback.primitive_renewal_modes.capacity() *
            sizeof(FocusedFallbackPolicy::PrimitiveRenewalMode);
        for (const auto& mode : fallback.primitive_renewal_modes) {
            bytes += mode.kernel_signature.capacity() *
                sizeof(std::uint64_t);
        }
        bytes += fallback.progress_state_value.bucket_count() *
            sizeof(void*);
        bytes += fallback.progress_state_value.size() *
            (sizeof(std::pair<const std::uint32_t, double>) +
             2 * sizeof(void*));
        bytes += fallback.progress_state_operator.bucket_count() *
            sizeof(void*);
        bytes += fallback.progress_state_operator.size() *
            (sizeof(std::pair<const std::uint32_t, std::uint32_t>) +
             2 * sizeof(void*));
        return bytes;
    }

std::uint64_t SolveWork::Impl::fast_estimated_owned_bytes() const {
        ++owned_byte_ledger_requests;
        return fast_estimated_owned_bytes_with_calc(
            calc.fast_estimated_owned_bytes());
    }

std::uint64_t SolveWork::Impl::fast_estimated_owned_bytes_with_calc(
        const std::uint64_t calc_bytes) const {
        std::uint64_t bytes =
            sizeof(*this) -
            kUpperPolicyProvenanceAccountingOffset -
            kIncumbentPortfolioAliasAccountingOffset +
            calc_bytes;
        bytes += prices.bucket_count() * sizeof(void*);
        bytes += prices.size() *
                 (sizeof(std::pair<const std::string, double>) +
                  2 * sizeof(void*));
        bytes += owned_prices_nested_bytes;
        bytes += operators.capacity() * sizeof(PricedOperator);
        bytes += owned_operators_nested_bytes;
        bytes += (reported_unsupported.capacity() + 7) / 8;
        bytes += static_operator_indices.capacity() * sizeof(std::uint32_t);
        bytes += delayed_operator_indices.capacity() *
                 sizeof(std::uint32_t);
        bytes += expansion_operator_indices.capacity() *
                 sizeof(std::uint32_t);
        if (carrier_bound_attribution) {
            bytes += sizeof(CarrierBoundAttributionWork);
        }
        bytes += incremental_carriers.capacity() * sizeof(std::uint32_t);
        bytes += incremental_automatic_carrier_order.capacity() *
                 sizeof(std::uint32_t);
        bytes += incremental_fairness_carrier_order.capacity() *
                 sizeof(std::uint32_t);
        bytes += incremental_high_progress_carrier_order.capacity() *
                 sizeof(std::uint32_t);
        bytes += incremental_high_progress_operator_order.capacity() *
                 sizeof(std::uint32_t);
        bytes += incremental_priority_tasks.capacity() *
                 sizeof(IncrementalPriorityTask);
        bytes += incremental_completed_pairs.bucket_count() * sizeof(void*);
        bytes += incremental_completed_pairs.size() *
                 (sizeof(std::uint64_t) + 2 * sizeof(void*));
        bytes += incremental_anytime_missing_frontier_states.capacity() *
                 sizeof(std::uint32_t);
        bytes += incremental_certified_upper_values.capacity() *
                 sizeof(double);
        bytes += incremental_dynamic_operator_indices.capacity() *
                 sizeof(std::uint32_t);
        bytes += incremental_alternative_rows.capacity() *
                 sizeof(IncrementalAlternativeRow);
        bytes += incremental_chaos_support.capacity() *
                 sizeof(std::uint8_t);
        bytes += incremental_nonchaos_states_seen.capacity() *
                 sizeof(std::uint8_t);
        bytes += expanded.capacity() * sizeof(std::uint8_t);
        bytes += queued.capacity() * sizeof(std::uint8_t);
        bytes += static_cast<std::uint64_t>(peak_queue_size) *
                 sizeof(std::uint32_t);
        if (transition_cache != nullptr) {
            bytes += transition_cache->fast_estimated_owned_bytes();
        }
        if (focused_strict_transition_cache != nullptr &&
            focused_strict_transition_cache != transition_cache) {
            bytes +=
                focused_strict_transition_cache->fast_estimated_owned_bytes();
        }
        bytes += focused_strict_expanded.capacity() * sizeof(std::uint8_t);
        bytes += focused_behavioral_representative.capacity() *
                 sizeof(std::uint32_t);
        bytes += focused_previous_upper_values.capacity() * sizeof(double);
        bytes += focused_lower_previous_values.capacity() * sizeof(double);
        bytes += focused_lower_retained_minimum.capacity() * sizeof(double);
        bytes += focused_lower_completion_proof_values.capacity() *
                 sizeof(double);
        bytes += incremental_classification_certified_lower.capacity() *
                 sizeof(double);
        bytes += focused_previous_upper_policy_rows.capacity() *
                 sizeof(std::uint64_t);
        bytes += focused_frontier_upper_operator.capacity() *
                 sizeof(std::uint32_t);
        bytes += focused_previous_frontier_upper_operator.capacity() *
                 sizeof(std::uint32_t);
        bytes += focused_round_lower_values.capacity() * sizeof(double);
        bytes += focused_round_lower_policy_rows.capacity() *
                 sizeof(std::uint64_t);
        bytes += focused_pending_lower_fringe.capacity() *
                 sizeof(std::uint32_t);
        bytes += focused_pending_upper_fringe.capacity() *
                 sizeof(std::uint32_t);
        bytes += focused_pending_upper_priority.capacity() * sizeof(double);
        bytes += operator_goal_reach_mask.capacity() * sizeof(std::uint32_t);
        bytes += operator_goal_reach_computed.capacity() *
                 sizeof(std::uint8_t);
        bytes += operator_goal_survival_paths.capacity() *
                 sizeof(OperatorGoalSurvivalPaths);
        bytes += operator_goal_survival_computed.capacity() *
                 sizeof(std::uint8_t);
        bytes += owned_goal_survival_nested_bytes;
        bytes += goal_cover_cost.capacity() * sizeof(double);
        bytes += clean_goal_cover_cost.capacity() * sizeof(double);
        bytes += carrier_goal_progress_cost.capacity() * sizeof(double);
        bytes += carrier_goal_action_floor.capacity() * sizeof(double);
        bytes += bounded_gain_goal_progress_cost.capacity() * sizeof(double);
        bytes += bounded_gain_action_floor.capacity() * sizeof(double);
        bytes += carrier_unproved_first_step_actions.capacity() *
                 sizeof(std::uint32_t);
        bytes += carrier_priced_first_step_actions.capacity() *
                 sizeof(std::pair<std::uint32_t, double>);
        bytes += carrier_goal_progress_eligibility_cache.capacity() *
                 sizeof(std::int8_t);
        bytes += carrier_terminal_debt_cache.capacity() * sizeof(double);
        bytes += clean_goal_escape_cost.capacity() * sizeof(double);
        bytes += clean_goal_escape_action.capacity() *
                 sizeof(std::uint32_t);
        bytes += clean_goal_no_exalt_escape_cost.capacity() *
                 sizeof(double);
        bytes += clean_goal_no_exalt_escape_action.capacity() *
                 sizeof(std::uint32_t);
        bytes += clean_goal_start_action_floor.capacity() * sizeof(double);
        for (const ProofPatternContract& pattern : contracts) {
            bytes += pattern.minimizing_action.capacity() + 1;
            bytes += pattern.fallback_reason.capacity() + 1;
            bytes += pattern.refinement_trace.capacity() + 1;
        }
        bytes += strict_clean_goal_cover_cost.capacity() * sizeof(double);
        if (focused_fallback_policy) {
            const FocusedFallbackPolicy& fallback =
                *focused_fallback_policy;
            bytes += sizeof(FocusedFallbackPolicy);
            bytes += fallback.renewal_kernel_signature.capacity() *
                sizeof(std::uint64_t);
            bytes += fallback.primitive_renewal_modes.capacity() *
                sizeof(FocusedFallbackPolicy::PrimitiveRenewalMode);
            for (const auto& mode : fallback.primitive_renewal_modes) {
                bytes += mode.kernel_signature.capacity() *
                    sizeof(std::uint64_t);
            }
            bytes += fallback.progress_state_value.bucket_count() *
                sizeof(void*);
            bytes += fallback.progress_state_value.size() *
                (sizeof(std::pair<const std::uint32_t, double>) +
                 2 * sizeof(void*));
            bytes += fallback.progress_state_operator.bucket_count() *
                sizeof(void*);
            bytes += fallback.progress_state_operator.size() *
                (sizeof(std::pair<const std::uint32_t, std::uint32_t>) +
                 2 * sizeof(void*));
        }
        if (constructive_progress_fallback.has_value()) {
            bytes += fallback_policy_dynamic_owned_bytes(
                *constructive_progress_fallback);
        }
        bytes += primitive_destructive_renewal_work
                     .materialized_alternatives.capacity() *
                 sizeof(std::uint64_t);
        bytes += primitive_destructive_renewal_work
                     .renewal_sources.capacity() *
                 sizeof(std::uint32_t);
        bytes += fallback_policy_dynamic_owned_bytes(
            primitive_destructive_renewal_work.best);
        bytes += certified_state_upper.capacity() * sizeof(double);
        bytes += certified_state_row.capacity() * sizeof(std::uint64_t);
        bytes += priced_rows.capacity() * sizeof(PricedSparseRow);
        bytes += priced_operator_position.capacity() * sizeof(std::int32_t);
        bytes += prioritized_states.capacity() *
                 sizeof(std::pair<double, std::uint32_t>);
        bytes += policy_rows.capacity() * sizeof(std::uint64_t);
        bytes += policy_seed_states.capacity() * sizeof(std::uint32_t);
        bytes += policy_selection_states.capacity() * sizeof(std::uint32_t);
        bytes += current_policy_scratch_bytes;
        bytes += anytime_policy_scratch_bytes;
        bytes += kernel_value_caches.capacity() * sizeof(KernelValueCache);
        bytes += kernel_value_cache_by_offset.bucket_count() * sizeof(void*);
        bytes += kernel_value_cache_by_offset.size() *
                 (sizeof(std::pair<const std::uint64_t, std::size_t>) +
                  2 * sizeof(void*));
        bytes += owned_kernel_value_cache_nested_bytes;
        bytes += kernel_rows_by_hash.bucket_count() * sizeof(void*);
        bytes += kernel_rows_by_hash.size() *
                 (sizeof(std::pair<const std::size_t,
                                   std::vector<KernelRowMemo>>) +
                  2 * sizeof(void*));
        bytes += owned_kernel_row_bucket_bytes;
        bytes += automatic_admission_records.bucket_count() * sizeof(void*);
        bytes += automatic_admission_records.size() *
                 (sizeof(std::uint64_t) + 2 * sizeof(void*));
        bytes += automatic_carrier_work.bucket_count() * sizeof(void*);
        bytes += automatic_carrier_work.size() *
                 (sizeof(std::uint64_t) + sizeof(AutomaticCarrierWork) +
                  2 * sizeof(void*));
        bytes += result.values.capacity() * sizeof(double);
        bytes += result.policy.capacity() * sizeof(PolicyOperatorRef);
        bytes += result.expanded.capacity() * sizeof(std::uint8_t);
        bytes += result.goal_states.capacity() * sizeof(std::uint8_t);
        bytes += result.policy_reachable.capacity() * sizeof(std::uint8_t);
        bytes += result.unveil_preferences.capacity() *
                 sizeof(std::vector<std::uint32_t>);
        bytes += result.option_unveil_preferences.capacity() *
                 sizeof(std::vector<ObservedUnveilPreference>);
        bytes += result.behavioral_representative_by_state.capacity() *
                 sizeof(std::uint32_t);
        bytes += result.primitive_renewal_witness
                     .kernel_signature.capacity() *
                 sizeof(std::uint64_t);
        bytes +=
            result.refined_policy_artifact.strategy_json.capacity() + 1;
        bytes += result.refined_policy_artifact
                     .certification_strategy_json.capacity() + 1;
        bytes += result.refined_policy_artifact
                     .policy_route_default_mode.capacity() + 1;
        bytes += result.refined_policy_artifact
                     .certification_policy_route_default_mode.capacity() + 1;
        bytes += owned_result_nested_bytes;
        bytes += output_incumbent_owned_bytes();
        if (finalization_task.has_value()) {
            bytes += finalization_task->retained_bytes();
        }
        if (finalized_result.has_value()) {
            bytes += solve_result_owned_bytes(*finalized_result) -
                (sizeof(SolveResult) -
                 kUpperPolicyProvenanceAccountingOffset);
        }
        /* Diagnostic samples are strictly bounded and are not graph-sized.
         * Keep their exact current allocation in both ledger paths. */
        bytes += diagnostics_owned_bytes(result.diagnostics);
        return bytes;
    }

std::uint64_t SolveWork::Impl::estimated_owned_bytes_with_calc(
        const std::uint64_t calc_bytes) const {
        std::uint64_t bytes =
            sizeof(*this) -
            kUpperPolicyProvenanceAccountingOffset -
            kIncumbentPortfolioAliasAccountingOffset +
            calc_bytes;
        bytes += prices.bucket_count() * sizeof(void*);
        bytes += prices.size() *
                 (sizeof(std::pair<const std::string, double>) +
                  2 * sizeof(void*));
        for (const auto& [key, price] : prices) {
            (void)price;
            bytes += key.capacity() + 1;
        }
        bytes += operators.capacity() * sizeof(PricedOperator);
        for (const PricedOperator& priced : operators) {
            bytes += priced.resource_prices.capacity() *
                     sizeof(std::pair<std::string, double>);
            for (const auto& [key, price] : priced.resource_prices) {
                (void)price;
                bytes += key.capacity() + 1;
            }
        }
        bytes += (reported_unsupported.capacity() + 7) / 8;
        bytes += static_operator_indices.capacity() * sizeof(std::uint32_t);
        bytes += delayed_operator_indices.capacity() *
                 sizeof(std::uint32_t);
        bytes += expansion_operator_indices.capacity() *
                 sizeof(std::uint32_t);
        if (carrier_bound_attribution) {
            bytes += sizeof(CarrierBoundAttributionWork);
        }
        bytes += incremental_carriers.capacity() * sizeof(std::uint32_t);
        bytes += incremental_automatic_carrier_order.capacity() *
                 sizeof(std::uint32_t);
        bytes += incremental_fairness_carrier_order.capacity() *
                 sizeof(std::uint32_t);
        bytes += incremental_high_progress_carrier_order.capacity() *
                 sizeof(std::uint32_t);
        bytes += incremental_high_progress_operator_order.capacity() *
                 sizeof(std::uint32_t);
        bytes += incremental_priority_tasks.capacity() *
                 sizeof(IncrementalPriorityTask);
        bytes += incremental_completed_pairs.bucket_count() * sizeof(void*);
        bytes += incremental_completed_pairs.size() *
                 (sizeof(std::uint64_t) + 2 * sizeof(void*));
        bytes += incremental_anytime_missing_frontier_states.capacity() *
                 sizeof(std::uint32_t);
        bytes += incremental_certified_upper_values.capacity() *
                 sizeof(double);
        bytes += incremental_dynamic_operator_indices.capacity() *
                 sizeof(std::uint32_t);
        bytes += incremental_alternative_rows.capacity() *
                 sizeof(IncrementalAlternativeRow);
        bytes += incremental_chaos_support.capacity() *
                 sizeof(std::uint8_t);
        bytes += incremental_nonchaos_states_seen.capacity() *
                 sizeof(std::uint8_t);
        bytes += expanded.capacity() * sizeof(std::uint8_t);
        bytes += queued.capacity() * sizeof(std::uint8_t);
        bytes += static_cast<std::uint64_t>(peak_queue_size) *
                 sizeof(std::uint32_t);
        if (transition_cache != nullptr) {
            bytes += transition_cache->estimated_owned_bytes();
        }
        if (focused_strict_transition_cache != nullptr &&
            focused_strict_transition_cache != transition_cache) {
            bytes += focused_strict_transition_cache->estimated_owned_bytes();
        }
        bytes += focused_strict_expanded.capacity() * sizeof(std::uint8_t);
        bytes += focused_behavioral_representative.capacity() *
                 sizeof(std::uint32_t);
        bytes += focused_previous_upper_values.capacity() * sizeof(double);
        bytes += focused_lower_previous_values.capacity() * sizeof(double);
        bytes += focused_lower_retained_minimum.capacity() * sizeof(double);
        bytes += focused_lower_completion_proof_values.capacity() *
                 sizeof(double);
        bytes += incremental_classification_certified_lower.capacity() *
                 sizeof(double);
        bytes += focused_previous_upper_policy_rows.capacity() *
                 sizeof(std::uint64_t);
        bytes += focused_frontier_upper_operator.capacity() *
                 sizeof(std::uint32_t);
        bytes += focused_previous_frontier_upper_operator.capacity() *
                 sizeof(std::uint32_t);
        bytes += focused_round_lower_values.capacity() * sizeof(double);
        bytes += focused_round_lower_policy_rows.capacity() *
                 sizeof(std::uint64_t);
        bytes += focused_pending_lower_fringe.capacity() *
                 sizeof(std::uint32_t);
        bytes += focused_pending_upper_fringe.capacity() *
                 sizeof(std::uint32_t);
        bytes += focused_pending_upper_priority.capacity() * sizeof(double);
        bytes += operator_goal_reach_mask.capacity() * sizeof(std::uint32_t);
        bytes += operator_goal_reach_computed.capacity() *
                 sizeof(std::uint8_t);
        bytes += operator_goal_survival_paths.capacity() *
                 sizeof(OperatorGoalSurvivalPaths);
        bytes += operator_goal_survival_computed.capacity() *
                 sizeof(std::uint8_t);
        for (const OperatorGoalSurvivalPaths& entry :
             operator_goal_survival_paths) {
            bytes += entry.paths.capacity() * sizeof(GoalSurvivalPath);
            for (const GoalSurvivalPath& path : entry.paths) {
                bytes += path.actions.capacity() * sizeof(std::uint32_t);
            }
        }
        bytes += goal_cover_cost.capacity() * sizeof(double);
        bytes += clean_goal_cover_cost.capacity() * sizeof(double);
        bytes += carrier_goal_progress_cost.capacity() * sizeof(double);
        bytes += carrier_goal_action_floor.capacity() * sizeof(double);
        bytes += bounded_gain_goal_progress_cost.capacity() * sizeof(double);
        bytes += bounded_gain_action_floor.capacity() * sizeof(double);
        bytes += carrier_unproved_first_step_actions.capacity() *
                 sizeof(std::uint32_t);
        bytes += carrier_priced_first_step_actions.capacity() *
                 sizeof(std::pair<std::uint32_t, double>);
        bytes += carrier_goal_progress_eligibility_cache.capacity() *
                 sizeof(std::int8_t);
        bytes += carrier_terminal_debt_cache.capacity() * sizeof(double);
        bytes += clean_goal_escape_cost.capacity() * sizeof(double);
        bytes += clean_goal_escape_action.capacity() *
                 sizeof(std::uint32_t);
        bytes += clean_goal_no_exalt_escape_cost.capacity() *
                 sizeof(double);
        bytes += clean_goal_no_exalt_escape_action.capacity() *
                 sizeof(std::uint32_t);
        bytes += clean_goal_start_action_floor.capacity() * sizeof(double);
        for (const ProofPatternContract& pattern : contracts) {
            bytes += pattern.minimizing_action.capacity() + 1;
            bytes += pattern.fallback_reason.capacity() + 1;
            bytes += pattern.refinement_trace.capacity() + 1;
        }
        bytes += strict_clean_goal_cover_cost.capacity() * sizeof(double);
        if (focused_fallback_policy) {
            const FocusedFallbackPolicy& fallback =
                *focused_fallback_policy;
            bytes += sizeof(FocusedFallbackPolicy);
            bytes += fallback.renewal_kernel_signature.capacity() *
                sizeof(std::uint64_t);
            bytes += fallback.primitive_renewal_modes.capacity() *
                sizeof(FocusedFallbackPolicy::PrimitiveRenewalMode);
            for (const auto& mode : fallback.primitive_renewal_modes) {
                bytes += mode.kernel_signature.capacity() *
                    sizeof(std::uint64_t);
            }
            bytes += fallback.progress_state_value.bucket_count() *
                sizeof(void*);
            bytes += fallback.progress_state_value.size() *
                (sizeof(std::pair<const std::uint32_t, double>) +
                 2 * sizeof(void*));
            bytes += fallback.progress_state_operator.bucket_count() *
                sizeof(void*);
            bytes += fallback.progress_state_operator.size() *
                (sizeof(std::pair<const std::uint32_t, std::uint32_t>) +
                 2 * sizeof(void*));
        }
        if (constructive_progress_fallback.has_value()) {
            bytes += fallback_policy_dynamic_owned_bytes(
                *constructive_progress_fallback);
        }
        bytes += primitive_destructive_renewal_work
                     .materialized_alternatives.capacity() *
                 sizeof(std::uint64_t);
        bytes += primitive_destructive_renewal_work
                     .renewal_sources.capacity() *
                 sizeof(std::uint32_t);
        bytes += fallback_policy_dynamic_owned_bytes(
            primitive_destructive_renewal_work.best);
        bytes += certified_state_upper.capacity() * sizeof(double);
        bytes += certified_state_row.capacity() * sizeof(std::uint64_t);
        bytes += priced_rows.capacity() * sizeof(PricedSparseRow);
        bytes += priced_operator_position.capacity() * sizeof(std::int32_t);
        bytes += prioritized_states.capacity() *
                 sizeof(std::pair<double, std::uint32_t>);
        bytes += policy_rows.capacity() * sizeof(std::uint64_t);
        bytes += policy_seed_states.capacity() * sizeof(std::uint32_t);
        bytes += policy_selection_states.capacity() * sizeof(std::uint32_t);
        bytes += current_policy_scratch_bytes;
        bytes += anytime_policy_scratch_bytes;
        bytes += kernel_value_caches.capacity() * sizeof(KernelValueCache);
        bytes += kernel_value_cache_by_offset.bucket_count() * sizeof(void*);
        bytes += kernel_value_cache_by_offset.size() *
                 (sizeof(std::pair<const std::uint64_t, std::size_t>) +
                  2 * sizeof(void*));
        bytes += kernel_rows_by_hash.bucket_count() * sizeof(void*);
        bytes += kernel_rows_by_hash.size() *
                 (sizeof(std::pair<const std::size_t,
                                   std::vector<KernelRowMemo>>) +
                  2 * sizeof(void*));
        for (const auto& [unused, rows] : kernel_rows_by_hash) {
            (void)unused;
            bytes += rows.capacity() * sizeof(KernelRowMemo);
        }
        bytes += automatic_admission_records.bucket_count() * sizeof(void*);
        bytes += automatic_admission_records.size() *
                 (sizeof(std::uint64_t) + 2 * sizeof(void*));
        bytes += automatic_carrier_work.bucket_count() * sizeof(void*);
        bytes += automatic_carrier_work.size() *
                 (sizeof(std::uint64_t) + sizeof(AutomaticCarrierWork) +
                  2 * sizeof(void*));
        bytes += result.values.capacity() * sizeof(double);
        bytes += result.policy.capacity() * sizeof(PolicyOperatorRef);
        bytes += result.expanded.capacity() * sizeof(std::uint8_t);
        bytes += result.goal_states.capacity() * sizeof(std::uint8_t);
        bytes += result.policy_reachable.capacity() * sizeof(std::uint8_t);
        bytes += result.unveil_preferences.capacity() *
                 sizeof(std::vector<std::uint32_t>);
        for (const auto& preferences : result.unveil_preferences) {
            bytes += preferences.capacity() * sizeof(std::uint32_t);
        }
        bytes += result.option_unveil_preferences.capacity() *
                 sizeof(std::vector<ObservedUnveilPreference>);
        for (const auto& preferences :
             result.option_unveil_preferences) {
            bytes += preferences.capacity() *
                     sizeof(ObservedUnveilPreference);
            for (const ObservedUnveilPreference& preference : preferences) {
                bytes += preference.choices.capacity() *
                         sizeof(ObservedUnveilChoice);
            }
        }
        bytes += result.behavioral_representative_by_state.capacity() *
                 sizeof(std::uint32_t);
        bytes += result.primitive_renewal_witness
                     .kernel_signature.capacity() *
                 sizeof(std::uint64_t);
        bytes +=
            result.refined_policy_artifact.strategy_json.capacity() + 1;
        bytes += result.refined_policy_artifact
                     .certification_strategy_json.capacity() + 1;
        bytes += result.refined_policy_artifact
                     .policy_route_default_mode.capacity() + 1;
        bytes += result.refined_policy_artifact
                     .certification_policy_route_default_mode.capacity() + 1;
        bytes += output_incumbent_owned_bytes();
        if (finalization_task.has_value()) {
            bytes += finalization_task->retained_bytes();
        }
        if (finalized_result.has_value()) {
            bytes += solve_result_owned_bytes(*finalized_result) -
                (sizeof(SolveResult) -
                 kUpperPolicyProvenanceAccountingOffset);
        }
        bytes += diagnostics_owned_bytes(result.diagnostics);
        return bytes;
    }

std::uint64_t estimated_retained_solver_bytes(
    const CalcContext& calc,
    const SolveResult* result) {
    std::uint64_t bytes = calc.estimated_owned_bytes();
    if (calc.solve_transition_cache() != nullptr) {
        bytes += calc.solve_transition_cache()->estimated_owned_bytes();
    }
    if (calc.solve_transition_cache_action_envelope_ledger() != nullptr) {
        bytes += sizeof(solve_detail::ActionEnvelopeLedger) +
            calc.solve_transition_cache_action_envelope_ledger()
                ->estimated_owned_bytes();
    }
    if (result != nullptr) bytes += solve_result_owned_bytes(*result);
    return bytes;
}

std::uint64_t fast_estimated_retained_solver_bytes(
    const CalcContext& calc,
    const SolveResult* result) {
    std::uint64_t bytes = calc.fast_estimated_owned_bytes();
    if (calc.solve_transition_cache() != nullptr) {
        bytes += calc.solve_transition_cache()->fast_estimated_owned_bytes();
    }
    if (calc.solve_transition_cache_action_envelope_ledger() != nullptr) {
        bytes += sizeof(solve_detail::ActionEnvelopeLedger) +
            calc.solve_transition_cache_action_envelope_ledger()
                ->estimated_owned_bytes();
    }
    if (result != nullptr) bytes += solve_result_owned_bytes(*result);
    return bytes;
}

std::string serialize_solve_log(
    const CalcContext& calc,
    const SolveResult& result) {
    std::string log;
    char buffer[256];
    for (std::uint32_t state = 0; state < result.values.size(); ++state) {
        if (!result.expanded[state]) continue;
        const AbstractState& features = calc.state(state);
        std::snprintf(buffer, sizeof(buffer),
                      "{\"state\":%u,\"value\":%.9g,\"action\":", state,
                      result.values[state]);
        log += buffer;
        const std::uint32_t action = result.policy[state];
        if (action == kNoId) {
            log += "null";
        } else {
            log += '"';
            log += calc.operators().at(action).id;
            log += '"';
        }
        std::snprintf(
            buffer, sizeof(buffer),
            ",\"goal\":%d,\"reachable\":%d,\"rarity\":%u,\"prefixes\":%u,"
            "\"suffixes\":%u,\"blocked\":%u,\"flags\":%u,"
            "\"veiled_side\":%d,\"searing_tier\":%u,\"eater_tier\":%u,"
            "\"slots\":[",
            result.goal_states[state] ? 1 : 0,
            result.policy_reachable[state] ? 1 : 0, features.rarity,
            features.prefix_count, features.suffix_count,
            features.blocked_mask, features.flags, features.veiled_side,
            features.searing_exarch_tier,
            features.eater_of_worlds_tier);
        log += buffer;
        for (std::size_t i = 0; i < calc.layout().slots.size(); ++i) {
            if (i > 0) log += ',';
            log += std::to_string(features.slot_status[i]);
        }
        log += "],\"junk\":[";
        for (std::size_t i = 0; i < features.junk_counts.size(); ++i) {
            if (i > 0) log += ',';
            log += std::to_string(features.junk_counts[i]);
        }
        log += "]}\n";
    }
    return log;
}

} // namespace solver
} // namespace poecraft
