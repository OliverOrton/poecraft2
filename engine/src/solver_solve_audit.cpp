#include "solver_solve_types.hpp"
#include "solver_sparse_policy.hpp"

#include <array>
#include <bit>
#include <map>
#include <set>
#include <unordered_set>

namespace poecraft {
namespace solver {

using namespace solve_detail;

namespace {

enum class AuditActionFamily : std::uint8_t {
    Chaos = 0,
    HarvestReforge,
    Essence,
    Fossil,
    EldritchChaos,
    AddRemove,
    Fracture,
    BenchSetup,
    Restart,
    Automatic,
    Other,
    Count
};

constexpr std::size_t kAuditActionFamilyCount =
    static_cast<std::size_t>(AuditActionFamily::Count);

const char* audit_action_family_name(const AuditActionFamily family) {
    switch (family) {
    case AuditActionFamily::Chaos: return "chaos";
    case AuditActionFamily::HarvestReforge: return "harvest_reforge";
    case AuditActionFamily::Essence: return "essence";
    case AuditActionFamily::Fossil: return "fossil";
    case AuditActionFamily::EldritchChaos: return "eldritch_chaos";
    case AuditActionFamily::AddRemove: return "add_remove";
    case AuditActionFamily::Fracture: return "fracture";
    case AuditActionFamily::BenchSetup: return "bench_setup";
    case AuditActionFamily::Restart: return "restart";
    case AuditActionFamily::Automatic: return "automatic";
    case AuditActionFamily::Other: return "other";
    case AuditActionFamily::Count: return "none";
    }
    return "none";
}

AuditActionFamily audit_action_family(
        const CalcContext& calc,
        const std::uint32_t operator_index,
        const std::uint32_t restart_operator) {
    if (operator_index == restart_operator) {
        return AuditActionFamily::Restart;
    }
    if (operator_index >= calc.operators().size()) {
        return AuditActionFamily::Other;
    }
    const PlannerOperator& planner =
        calc.operators()[operator_index];
    if (planner.kind != PlannerOperatorKind::Primitive ||
        planner.primitive_action >= calc.registry().actions.size()) {
        return planner.automatic_kind != AutomaticCandidateKind::None
                   ? AuditActionFamily::Automatic
                   : AuditActionFamily::Other;
    }
    switch (calc.registry().actions[planner.primitive_action].params.type) {
    case ActionType::Chaos: return AuditActionFamily::Chaos;
    case ActionType::HarvestReforge:
        return AuditActionFamily::HarvestReforge;
    case ActionType::Essence: return AuditActionFamily::Essence;
    case ActionType::Fossil: return AuditActionFamily::Fossil;
    case ActionType::EldritchChaos:
        return AuditActionFamily::EldritchChaos;
    case ActionType::Augment:
    case ActionType::Alteration:
    case ActionType::Regal:
    case ActionType::Alchemy:
    case ActionType::Exalt:
    case ActionType::Annul:
    case ActionType::Scour:
    case ActionType::VeiledExalt:
    case ActionType::Unveil:
    case ActionType::HarvestAugment:
    case ActionType::HarvestResist:
    case ActionType::EldritchExalt:
    case ActionType::EldritchAnnul:
    case ActionType::InfluenceExalt:
    case ActionType::RemoveCraftedModifiers:
        return AuditActionFamily::AddRemove;
    case ActionType::Fracture: return AuditActionFamily::Fracture;
    case ActionType::Bench:
    case ActionType::EldritchEmber:
    case ActionType::EldritchIchor:
        return AuditActionFamily::BenchSetup;
    case ActionType::Transmute:
    case ActionType::VeiledChaos:
        return AuditActionFamily::Other;
    }
    return AuditActionFamily::Other;
}

std::uint64_t exact_signature_hash(
        const std::vector<std::uint64_t>& signature) {
    std::uint64_t hash = 1469598103934665603ULL;
    for (const std::uint64_t part : signature) {
        hash ^= part;
        hash *= 1099511628211ULL;
    }
    return hash;
}

std::uint32_t below_tier_goal_mask(const AbstractState& state) {
    std::uint32_t mask = 0;
    for (std::uint32_t slot = 0; slot < kMaxGoalSlots; ++slot) {
        if (state.slot_status[slot] ==
            static_cast<std::uint8_t>(
                GoalSlotStatus::PresentBelowTier)) {
            mask |= 1u << slot;
        }
    }
    return mask;
}

std::uint32_t nonzero_junk_class_count(const AbstractState& state) {
    std::uint32_t count = 0;
    for (std::size_t junk = 0; junk < state.junk_counts.size(); ++junk) {
        if (state.junk_counts[junk] != 0 ||
            state.fractured_junk_counts[junk] != 0 ||
            state.crafted_junk_counts[junk] != 0 ||
            state.fractured_crafted_junk_counts[junk] != 0) {
            ++count;
        }
    }
    return count;
}

std::uint64_t disposable_shell_hash(const AbstractState& state) {
    std::uint64_t hash = 1469598103934665603ULL;
    const auto add = [&](const std::uint64_t value) {
        hash ^= value;
        hash *= 1099511628211ULL;
    };
    for (std::size_t junk = 0; junk < state.junk_counts.size(); ++junk) {
        add(junk);
        add(state.junk_counts[junk]);
        add(state.fractured_junk_counts[junk]);
        add(state.crafted_junk_counts[junk]);
        add(state.fractured_crafted_junk_counts[junk]);
    }
    return hash;
}

void append_hash_string(
        std::string& json,
        const std::uint64_t hash) {
    char buffer[17]{};
    std::snprintf(
        buffer, sizeof(buffer), "%016llx",
        static_cast<unsigned long long>(hash));
    json.push_back('"');
    json += buffer;
    json.push_back('"');
}

} // namespace

void SolveWork::Impl::finalize_upper_cap_zero_progress_audit() {
    SolveDiagnostics& diagnostics = result.diagnostics;
    diagnostics.upper_cap_zero_progress_audit_json.clear();
    if (!options.high_impact_executable_uppers ||
        !output_incumbent.has_value() ||
        transition_cache == nullptr) {
        return;
    }

    const BoundedPolicyIncumbent& incumbent = *output_incumbent;
    const std::uint64_t no_row =
        std::numeric_limits<std::uint64_t>::max();
    const std::size_t state_count = calc.state_count();

    std::vector<double> operator_costs(
        calc.operators().size(), kInfinity);
    for (const PricedOperator& priced : operators) {
        if (priced.index < operator_costs.size()) {
            operator_costs[priced.index] = priced.cost;
        }
    }

    struct CoverageEntry {
        double contribution = 0.0;
        double probability = 0.0;
        std::uint32_t state = kNoId;
        std::uint32_t parent_operator = kNoId;
    };
    struct CoverageAggregate {
        std::uint64_t rows = 0;
        std::uint64_t successors = 0;
        double probability = 0.0;
        double non_restart_probability = 0.0;
        double local_probability = 0.0;
        double restart_probability = 0.0;
        double chaos_probability = 0.0;
        double other_probability = 0.0;
        double contribution = 0.0;
        double lower_q = kInfinity;
        double upper_q = kInfinity;
        std::array<double, kMaxGoalSlots + 1> probability_by_goal_count{};
        std::array<double, kMaxGoalSlots + 1> contribution_by_goal_count{};
        std::map<std::uint32_t, double> probability_by_goal_mask;
        std::map<std::uint32_t, double> contribution_by_goal_mask;
    };
    std::vector<CoverageEntry> coverage;
    std::map<std::string, CoverageAggregate> coverage_by_action;
    CoverageAggregate coverage_total;

    const auto selected_continuation =
        [&](const std::uint32_t state) {
            std::uint64_t selected_row =
                state < incumbent.policy_rows.size()
                    ? incumbent.policy_rows[state]
                    : no_row;
            std::uint32_t selected_operator = kNoId;
            bool local = false;
            if (selected_row != no_row &&
                selected_row < priced_rows.size()) {
                selected_operator =
                    priced_rows[selected_row].operator_index;
                local = selected_row < transition_cache->rows.size();
            } else if (state <
                       incumbent.frontier_operators.size()) {
                selected_operator =
                    incumbent.frontier_operators[state];
            }
            return std::tuple{
                selected_operator, selected_row, local};
        };
    const auto record_coverage =
        [&](const std::uint32_t state,
            const double probability,
            const std::uint32_t parent_operator) {
            if (!(probability > 0.0) ||
                !std::isfinite(probability) ||
                state >= incumbent.values.size()) {
                return;
            }
            const double upper = incumbent.values[state];
            if (!std::isfinite(upper) || upper < 0.0) return;
            const double contribution = probability * upper;
            const std::uint32_t goal_mask =
                satisfied_goal_mask_for_state(state);
            const std::uint32_t goal_count =
                std::min<std::uint32_t>(
                    std::popcount(goal_mask), kMaxGoalSlots);
            coverage.push_back({
                contribution, probability, state, parent_operator});
            const std::string& action_id =
                calc.operators()[parent_operator].id;
            CoverageAggregate& action =
                coverage_by_action[action_id];
            ++action.successors;
            action.probability += probability;
            action.contribution += contribution;
            action.probability_by_goal_count[goal_count] += probability;
            action.contribution_by_goal_count[goal_count] += contribution;
            action.probability_by_goal_mask[goal_mask] += probability;
            action.contribution_by_goal_mask[goal_mask] += contribution;
            ++coverage_total.successors;
            coverage_total.probability += probability;
            coverage_total.contribution += contribution;
            coverage_total.probability_by_goal_count[goal_count] +=
                probability;
            coverage_total.contribution_by_goal_count[goal_count] +=
                contribution;
            coverage_total.probability_by_goal_mask[goal_mask] +=
                probability;
            coverage_total.contribution_by_goal_mask[goal_mask] +=
                contribution;
            const auto [selected_operator, selected_row, local] =
                selected_continuation(state);
            (void)selected_row;
            if (selected_operator != restart_operator_index) {
                action.non_restart_probability += probability;
                coverage_total.non_restart_probability += probability;
            }
            if (local) {
                action.local_probability += probability;
                coverage_total.local_probability += probability;
            } else if (selected_operator == restart_operator_index) {
                action.restart_probability += probability;
                coverage_total.restart_probability += probability;
            } else if (selected_operator < calc.operators().size() &&
                       audit_action_family(
                           calc, selected_operator,
                           restart_operator_index) ==
                           AuditActionFamily::Chaos) {
                action.chaos_probability += probability;
                coverage_total.chaos_probability += probability;
            } else {
                action.other_probability += probability;
                coverage_total.other_probability += probability;
            }
        };

    for (const IncrementalAlternativeRow& retained :
         incremental_alternative_rows) {
        if (retained.state != result.start_state ||
            retained.row_index >= transition_cache->rows.size() ||
            retained.operator_index >= calc.operators().size()) {
            continue;
        }
        const std::string& id =
            calc.operators()[retained.operator_index].id;
        if (id.find("fossil") == std::string::npos &&
            id.find("harvest") == std::string::npos) {
            continue;
        }
        ++coverage_by_action[id].rows;
        ++coverage_total.rows;
        CoverageAggregate& action_coverage =
            coverage_by_action[id];
        action_coverage.lower_q =
            std::min(action_coverage.lower_q, retained.lower_q);
        action_coverage.upper_q =
            std::min(action_coverage.upper_q, retained.upper_q);
        const SparseRow& row =
            transition_cache->rows[retained.row_index];
        if (row.self_probability > 0.0) {
            record_coverage(
                retained.state, row.self_probability,
                retained.operator_index);
        }
        for (std::uint32_t i = 0; i < row.transition_count; ++i) {
            const std::uint64_t offset = row.transition_offset + i;
            record_coverage(
                transition_cache->successors[offset],
                transition_cache->probabilities[offset],
                retained.operator_index);
        }
        for (std::uint32_t i = 0; i < row.choice_count; ++i) {
            const SparseChoiceGroup& group =
                transition_cache->choices[row.choice_offset + i];
            std::uint32_t selected =
                group.has_self ? retained.state : kNoId;
            double selected_upper =
                group.has_self &&
                        retained.state < incumbent.values.size()
                    ? incumbent.values[retained.state]
                    : kInfinity;
            for (std::uint32_t s = 0;
                 s < group.successor_count; ++s) {
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
            if (selected != kNoId) {
                record_coverage(
                    selected, group.probability,
                    retained.operator_index);
            }
        }
    }
    std::stable_sort(
        coverage.begin(), coverage.end(),
        [](const CoverageEntry& left,
           const CoverageEntry& right) {
            if (left.contribution != right.contribution) {
                return left.contribution > right.contribution;
            }
            if (left.probability != right.probability) {
                return left.probability > right.probability;
            }
            if (left.state != right.state) {
                return left.state < right.state;
            }
            return left.parent_operator < right.parent_operator;
        });

    struct FanoutAggregate {
        std::uint64_t rows = 0;
        std::uint64_t successor_entries = 0;
        std::uint64_t max_row_fanout = 0;
        std::uint64_t observed_states = 0;
        std::uint64_t observed_zero_progress_states = 0;
    };
    struct SourceFanoutAggregate {
        std::uint64_t rows = 0;
        std::uint64_t successor_entries = 0;
        std::uint64_t max_row_fanout = 0;
    };
    std::array<FanoutAggregate, kAuditActionFamilyCount> fanout{};
    std::array<
        std::array<SourceFanoutAggregate, kMaxGoalSlots + 1>,
        kAuditActionFamilyCount>
        fanout_by_source_goal_count{};
    std::vector<std::uint16_t> producer_masks(state_count, 0);
    for (std::size_t row_index = 0;
         row_index < transition_cache->rows.size(); ++row_index) {
        const SparseRow& row =
            transition_cache->rows[row_index];
        const std::uint32_t operator_index =
            row_index < priced_rows.size()
                ? priced_rows[row_index].operator_index
                : kNoId;
        const AuditActionFamily family =
            audit_action_family(
                calc, operator_index, restart_operator_index);
        const std::size_t family_index =
            static_cast<std::size_t>(family);
        FanoutAggregate& aggregate = fanout[family_index];
        std::uint64_t row_fanout = row.transition_count;
        for (std::uint32_t i = 0; i < row.choice_count; ++i) {
            row_fanout +=
                transition_cache
                    ->choices[row.choice_offset + i]
                    .successor_count;
        }
        ++aggregate.rows;
        aggregate.successor_entries += row_fanout;
        aggregate.max_row_fanout =
            std::max(aggregate.max_row_fanout, row_fanout);
        const std::uint32_t source_goal_count =
            row.owner_state < state_count
                ? std::min<std::uint32_t>(
                      std::popcount(
                          satisfied_goal_mask_for_state(
                              row.owner_state)),
                      kMaxGoalSlots)
                : 0;
        SourceFanoutAggregate& source =
            fanout_by_source_goal_count[family_index]
                                        [source_goal_count];
        ++source.rows;
        source.successor_entries += row_fanout;
        source.max_row_fanout =
            std::max(source.max_row_fanout, row_fanout);
        const std::uint16_t bit =
            static_cast<std::uint16_t>(1u << family_index);
        const auto mark = [&](const std::uint32_t state) {
            if (state < producer_masks.size()) {
                producer_masks[state] |= bit;
            }
        };
        for (std::uint32_t i = 0; i < row.transition_count; ++i) {
            mark(transition_cache->successors[
                row.transition_offset + i]);
        }
        for (std::uint32_t i = 0; i < row.choice_count; ++i) {
            const SparseChoiceGroup& group =
                transition_cache->choices[row.choice_offset + i];
            for (std::uint32_t s = 0;
                 s < group.successor_count; ++s) {
                mark(transition_cache->choice_successors[
                    group.successor_offset + s]);
            }
        }
    }

    struct SignatureAggregate {
        std::uint64_t states = 0;
        std::uint64_t safe_states = 0;
        std::uint64_t safe_ordinary_states = 0;
        std::uint64_t retry_basin_states = 0;
        std::uint64_t explicit_affix_states = 0;
    };
    std::map<std::vector<std::uint64_t>, SignatureAggregate>
        signature_groups;
    std::array<std::uint64_t, 4> rarity_counts{};
    std::array<std::array<std::uint64_t, 4>, 4> occupancy{};
    std::array<std::uint64_t, kAuditActionFamilyCount>
        zero_producer_counts{};
    std::uint64_t zero_states = 0;
    std::uint64_t retry_basin_states = 0;
    std::uint64_t ordinary_zero_states = 0;
    std::uint64_t live_renewable_states = 0;
    std::uint64_t certified_dead_states = 0;
    std::uint64_t no_legal_renewal_states = 0;
    std::uint64_t nonrenewal_observer_states = 0;
    std::uint64_t unknown_producer_states = 0;
    std::uint64_t explicit_affix_states = 0;
    std::uint64_t fractured_states = 0;
    std::uint64_t crafted_states = 0;
    std::uint64_t metamod_or_protection_states = 0;
    std::uint64_t influenced_states = 0;
    std::uint64_t corrupted_or_mirrored_states = 0;
    std::uint64_t eldritch_states = 0;
    std::uint64_t veiled_states = 0;
    std::uint64_t persistent_setup_states = 0;
    std::map<std::string, std::uint64_t>
        nonrenewal_observer_actions;

    const std::uint32_t slot_count =
        static_cast<std::uint32_t>(calc.layout().slots.size());
    const std::uint32_t required =
        calc.goal().required_satisfied_slots();
    for (std::uint32_t state_id = 0;
         state_id < state_count; ++state_id) {
        if (satisfied_goal_mask_for_state(state_id) != 0) continue;
        ++zero_states;
        const AbstractState& state = calc.state(state_id);
        const bool in_retry_basin =
            state.goal_progress_retry_basin != 0;
        if (in_retry_basin) {
            ++retry_basin_states;
        } else {
            ++ordinary_zero_states;
        }
        if (state.rarity < rarity_counts.size()) {
            ++rarity_counts[state.rarity];
        }
        if (state.prefix_count < occupancy.size() &&
            state.suffix_count < occupancy[0].size()) {
            ++occupancy[state.prefix_count][state.suffix_count];
        }
        const bool has_explicit =
            state.prefix_count != 0 || state.suffix_count != 0;
        if (has_explicit) ++explicit_affix_states;
        if ((state.flags & kFlagFractured) != 0 ||
            state.fractured_goal_mask != 0) {
            ++fractured_states;
        }
        if ((state.flags & kFlagCraftedMod) != 0 ||
            state.crafted_goal_mask != 0) {
            ++crafted_states;
        }
        if ((state.flags & kProtectionFlags) != 0 ||
            state.fractured_metamod_flags != 0) {
            ++metamod_or_protection_states;
        }
        if (state.influence_bits != 0 ||
            (state.flags & kFlagInfluenced) != 0) {
            ++influenced_states;
        }
        if ((state.flags &
             (kFlagCorrupted | kFlagMirrored)) != 0) {
            ++corrupted_or_mirrored_states;
        }
        if (state.searing_exarch_tier != 0 ||
            state.eater_of_worlds_tier != 0) {
            ++eldritch_states;
        }
        if (state.veiled_side >= 0 ||
            (state.flags & kFlagVeiledMod) != 0) {
            ++veiled_states;
        }
        if (state.veiled_side >= 0 ||
            state.influence_bits != 0 ||
            state.searing_exarch_tier != 0 ||
            state.eater_of_worlds_tier != 0 ||
            (state.flags &
             (kFlagCorrupted | kFlagMirrored | kFlagSplit |
              kFlagSynthesised | kFlagFractured |
              kFlagCraftedMod | kProtectionFlags)) != 0 ||
            state.fractured_metamod_flags != 0) {
            ++persistent_setup_states;
        }

        const std::uint16_t producer_mask =
            producer_masks[state_id];
        if (producer_mask == 0) {
            ++unknown_producer_states;
        }
        for (std::size_t family = 0;
             family < kAuditActionFamilyCount; ++family) {
            if ((producer_mask & (1u << family)) != 0) {
                ++zero_producer_counts[family];
            }
        }

        std::uint32_t permanently_blocked = 0;
        for (std::uint32_t slot = 0; slot < slot_count; ++slot) {
            const std::uint32_t bit = 1u << slot;
            if ((state.fractured_goal_mask & bit) != 0 &&
                state.slot_status[slot] !=
                    static_cast<std::uint8_t>(
                        GoalSlotStatus::Satisfied)) {
                permanently_blocked |= bit;
            }
        }
        for (std::size_t junk = 0;
             junk < calc.layout().junk_classes.size(); ++junk) {
            if (state.fractured_junk_counts[junk] != 0) {
                permanently_blocked |=
                    calc.layout().junk_classes[junk]
                        .goal_block_mask;
            }
        }
        const bool certified_dead =
            slot_count -
                static_cast<std::uint32_t>(
                    std::popcount(permanently_blocked)) <
            required;
        if (certified_dead) ++certified_dead_states;

        std::vector<std::uint64_t> combined_signature;
        bool live_renewal = false;
        bool nonrenewal_observer = false;
        for (const std::uint32_t operator_index :
             static_operator_indices) {
            if (operator_index >= calc.operators().size() ||
                operator_index == restart_operator_index) {
                continue;
            }
            const PlannerOperator& planner =
                calc.operators()[operator_index];
            if (planner.kind != PlannerOperatorKind::Primitive ||
                planner.primitive_action >=
                    calc.registry().actions.size()) {
                if (!in_retry_basin) {
                    nonrenewal_observer = true;
                    ++nonrenewal_observer_actions[
                        calc.operators()[operator_index].id];
                }
                continue;
            }
            const ActionDescriptor& descriptor =
                calc.registry().actions[
                    planner.primitive_action];
            if (!action_legal(session, descriptor, state)) continue;
            const ActionTransitionFacts facts =
                action_transition_facts(descriptor.params.type);
            if (!facts.renewal ||
                descriptor.params.type ==
                    ActionType::EldritchChaos) {
                if (!in_retry_basin) {
                    nonrenewal_observer = true;
                    ++nonrenewal_observer_actions[
                        calc.operators()[operator_index].id];
                }
                continue;
            }
            std::vector<std::uint64_t> signature;
            if (!calc.exact_reforge_kernel_signature(
                    state_id, planner.primitive_action,
                    signature)) {
                continue;
            }
            live_renewal = true;
            combined_signature.push_back(operator_index);
            combined_signature.push_back(
                std::bit_cast<std::uint64_t>(
                    operator_index < operator_costs.size()
                        ? operator_costs[operator_index]
                        : kInfinity));
            combined_signature.push_back(signature.size());
            combined_signature.insert(
                combined_signature.end(),
                signature.begin(), signature.end());
        }
        if (!in_retry_basin &&
            calc.goal().automatic_candidates) {
            /*
             * State-local bench/capacity/Fracture candidates are generated
             * after this static envelope. Treat their possible observation
             * of explicit affixes as a conservative blocker.
             */
            nonrenewal_observer = true;
            ++nonrenewal_observer_actions[
                "state_local_automatic_candidates"];
        }
        if (live_renewal) {
            ++live_renewable_states;
        } else {
            ++no_legal_renewal_states;
        }
        if (nonrenewal_observer) {
            ++nonrenewal_observer_states;
        }
        if (!combined_signature.empty()) {
            SignatureAggregate& aggregate =
                signature_groups[combined_signature];
            ++aggregate.states;
            if (in_retry_basin) ++aggregate.retry_basin_states;
            if (has_explicit) ++aggregate.explicit_affix_states;
            if (live_renewal && !certified_dead &&
                !nonrenewal_observer) {
                ++aggregate.safe_states;
                if (!in_retry_basin) {
                    ++aggregate.safe_ordinary_states;
                }
            }
        }
    }

    for (std::uint32_t state_id = 0;
         state_id < state_count; ++state_id) {
        const std::uint16_t mask = producer_masks[state_id];
        for (std::size_t family = 0;
             family < kAuditActionFamilyCount; ++family) {
            if ((mask & (1u << family)) != 0) {
                ++fanout[family].observed_states;
                if (satisfied_goal_mask_for_state(state_id) == 0) {
                    ++fanout[family]
                          .observed_zero_progress_states;
                }
            }
        }
    }

    std::uint64_t safe_equivalent_states = 0;
    std::uint64_t additional_canonicalizable_states = 0;
    std::uint64_t discarded_explicit_equivalent_states = 0;
    struct SignatureSummary {
        std::uint64_t hash = 0;
        SignatureAggregate aggregate;
    };
    std::vector<SignatureSummary> signature_summaries;
    signature_summaries.reserve(signature_groups.size());
    for (const auto& [signature, aggregate] : signature_groups) {
        if (aggregate.safe_states > 0) {
            safe_equivalent_states += aggregate.safe_states;
            if (aggregate.retry_basin_states > 0) {
                additional_canonicalizable_states +=
                    aggregate.safe_ordinary_states;
            } else if (aggregate.safe_ordinary_states > 1) {
                additional_canonicalizable_states +=
                    aggregate.safe_ordinary_states - 1;
            }
            if (aggregate.safe_states > 1 &&
                aggregate.explicit_affix_states > 0) {
                discarded_explicit_equivalent_states +=
                    aggregate.safe_states - 1;
            }
        }
        signature_summaries.push_back({
            exact_signature_hash(signature), aggregate});
    }
    std::stable_sort(
        signature_summaries.begin(),
        signature_summaries.end(),
        [](const SignatureSummary& left,
           const SignatureSummary& right) {
            if (left.aggregate.states != right.aggregate.states) {
                return left.aggregate.states >
                       right.aggregate.states;
            }
            return left.hash < right.hash;
        });

    struct StateCensusAggregate {
        std::uint64_t states = 0;
        std::uint64_t expanded = 0;
        double root_probability = 0.0;
        double root_upper_q_contribution = 0.0;
    };
    std::array<StateCensusAggregate, kMaxGoalSlots + 1>
        states_by_goal_count{};
    std::map<std::uint32_t, StateCensusAggregate> states_by_goal_mask;
    std::map<std::uint32_t, StateCensusAggregate> states_by_below_mask;
    std::map<std::pair<std::uint8_t, std::uint8_t>, StateCensusAggregate>
        states_by_occupancy;
    std::map<std::uint32_t, StateCensusAggregate>
        states_by_nonzero_junk_classes;
    StateCensusAggregate state_feature_fractured;
    StateCensusAggregate state_feature_crafted;
    StateCensusAggregate state_feature_protected;
    StateCensusAggregate state_feature_eldritch;
    StateCensusAggregate state_feature_persistent;
    const auto add_state = [&](StateCensusAggregate& aggregate,
                               const bool expanded_state) {
        ++aggregate.states;
        if (expanded_state) ++aggregate.expanded;
    };
    for (std::uint32_t state_id = 0; state_id < state_count; ++state_id) {
        const AbstractState& state = calc.state(state_id);
        const std::uint32_t satisfied_mask =
            satisfied_goal_mask_for_state(state_id);
        const std::uint32_t goal_count =
            std::min<std::uint32_t>(
                std::popcount(satisfied_mask), kMaxGoalSlots);
        const bool expanded_state =
            state_id < expanded.size() && expanded[state_id] != 0;
        add_state(states_by_goal_count[goal_count], expanded_state);
        add_state(states_by_goal_mask[satisfied_mask], expanded_state);
        add_state(
            states_by_below_mask[below_tier_goal_mask(state)],
            expanded_state);
        add_state(
            states_by_occupancy[
                {state.prefix_count, state.suffix_count}],
            expanded_state);
        add_state(
            states_by_nonzero_junk_classes[
                nonzero_junk_class_count(state)],
            expanded_state);
        if ((state.flags & kFlagFractured) != 0 ||
            state.fractured_goal_mask != 0 ||
            compact_count_total(state.fractured_junk_counts) != 0) {
            add_state(state_feature_fractured, expanded_state);
        }
        if ((state.flags & kFlagCraftedMod) != 0 ||
            state.crafted_goal_mask != 0 ||
            compact_count_total(state.crafted_junk_counts) != 0) {
            add_state(state_feature_crafted, expanded_state);
        }
        if ((state.flags & kProtectionFlags) != 0 ||
            state.fractured_metamod_flags != 0) {
            add_state(state_feature_protected, expanded_state);
        }
        if (state.searing_exarch_tier != 0 ||
            state.eater_of_worlds_tier != 0) {
            add_state(state_feature_eldritch, expanded_state);
        }
        if (state.veiled_side >= 0 ||
            state.influence_bits != 0 ||
            state.searing_exarch_tier != 0 ||
            state.eater_of_worlds_tier != 0 ||
            state.goal_progress_retry_basin != 0 ||
            (state.flags &
             (kFlagCorrupted | kFlagMirrored | kFlagSplit |
              kFlagSynthesised | kFlagFractured |
              kFlagCraftedMod | kProtectionFlags)) != 0 ||
            state.fractured_metamod_flags != 0) {
            add_state(state_feature_persistent, expanded_state);
        }
    }
    for (const CoverageEntry& entry : coverage) {
        if (entry.state >= state_count) continue;
        const std::uint32_t mask =
            satisfied_goal_mask_for_state(entry.state);
        const std::uint32_t goal_count =
            std::min<std::uint32_t>(
                std::popcount(mask), kMaxGoalSlots);
        states_by_goal_count[goal_count].root_probability +=
            entry.probability;
        states_by_goal_count[goal_count].root_upper_q_contribution +=
            entry.contribution;
        states_by_goal_mask[mask].root_probability +=
            entry.probability;
        states_by_goal_mask[mask].root_upper_q_contribution +=
            entry.contribution;
        const AbstractState& state = calc.state(entry.state);
        states_by_below_mask[below_tier_goal_mask(state)].root_probability +=
            entry.probability;
        states_by_below_mask[below_tier_goal_mask(state)]
            .root_upper_q_contribution += entry.contribution;
        states_by_occupancy[
            {state.prefix_count, state.suffix_count}]
            .root_probability += entry.probability;
        states_by_occupancy[
            {state.prefix_count, state.suffix_count}]
            .root_upper_q_contribution += entry.contribution;
        const std::uint32_t nonzero =
            nonzero_junk_class_count(state);
        states_by_nonzero_junk_classes[nonzero].root_probability +=
            entry.probability;
        states_by_nonzero_junk_classes[nonzero]
            .root_upper_q_contribution += entry.contribution;
    }

    using AnchorKey = std::tuple<
        std::uint32_t, std::uint32_t, std::uint8_t, std::uint8_t,
        std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t,
        std::uint8_t, std::uint8_t, std::uint8_t, std::uint32_t>;
    struct AnchorAggregate {
        std::set<std::uint32_t> states;
        std::set<std::uint64_t> shell_variants;
        double probability = 0.0;
        double contribution = 0.0;
        double local_probability = 0.0;
        double restart_probability = 0.0;
        double chaos_probability = 0.0;
        double other_probability = 0.0;
    };
    std::map<AnchorKey, AnchorAggregate> anchor_groups;
    for (const CoverageEntry& entry : coverage) {
        if (entry.state >= state_count) continue;
        const AbstractState& state = calc.state(entry.state);
        const AnchorKey key{
            satisfied_goal_mask_for_state(entry.state),
            below_tier_goal_mask(state),
            state.prefix_count,
            state.suffix_count,
            state.blocked_mask,
            state.fractured_goal_mask,
            state.crafted_goal_mask,
            state.flags & kProtectionFlags,
            state.influence_bits,
            state.searing_exarch_tier,
            state.eater_of_worlds_tier,
            state.flags &
                (kFlagCraftedMod | kFlagVeiledMod |
                 kFlagFractured | kProtectionFlags)};
        AnchorAggregate& aggregate = anchor_groups[key];
        aggregate.states.insert(entry.state);
        aggregate.shell_variants.insert(disposable_shell_hash(state));
        aggregate.probability += entry.probability;
        aggregate.contribution += entry.contribution;
        const auto [selected_operator, selected_row, local] =
            selected_continuation(entry.state);
        (void)selected_row;
        if (local) {
            aggregate.local_probability += entry.probability;
        } else if (selected_operator == restart_operator_index) {
            aggregate.restart_probability += entry.probability;
        } else if (selected_operator < calc.operators().size() &&
                   audit_action_family(
                       calc, selected_operator,
                       restart_operator_index) ==
                       AuditActionFamily::Chaos) {
            aggregate.chaos_probability += entry.probability;
        } else {
            aggregate.other_probability += entry.probability;
        }
    }
    struct AnchorSummary {
        AnchorKey key;
        AnchorAggregate aggregate;
    };
    std::vector<AnchorSummary> anchor_summaries;
    anchor_summaries.reserve(anchor_groups.size());
    for (const auto& [key, aggregate] : anchor_groups) {
        anchor_summaries.push_back({key, aggregate});
    }
    std::stable_sort(
        anchor_summaries.begin(), anchor_summaries.end(),
        [](const AnchorSummary& left, const AnchorSummary& right) {
            if (left.aggregate.contribution !=
                right.aggregate.contribution) {
                return left.aggregate.contribution >
                       right.aggregate.contribution;
            }
            return left.key < right.key;
        });

    const auto row_has_operator =
        [&](const SparseRow& row, const std::uint32_t operator_index) {
            for (std::uint32_t i = 0; i < row.variant_count; ++i) {
                const std::uint32_t variant_index =
                    transition_cache->variant_arena
                        ->row_variant_indices.at(row.variant_offset + i);
                if (variant_index <
                        transition_cache->variant_arena->variants.size() &&
                    transition_cache->variant_arena
                            ->variants[variant_index]
                            .operator_index == operator_index) {
                    return true;
                }
            }
            return false;
        };
    const auto row_probabilities =
        [&](const SparseRow& row) {
            std::map<std::uint32_t, double> probabilities;
            if (row.self_probability > 0.0 &&
                row.owner_state < state_count) {
                probabilities[row.owner_state] += row.self_probability;
            }
            for (std::uint32_t i = 0; i < row.transition_count; ++i) {
                const std::uint64_t offset = row.transition_offset + i;
                probabilities[transition_cache->successors[offset]] +=
                    transition_cache->probabilities[offset];
            }
            return probabilities;
        };
    std::uint32_t chaos_operator = kNoId;
    for (const std::uint32_t operator_index : static_operator_indices) {
        if (operator_index >= calc.operators().size()) continue;
        const PlannerOperator& planner = calc.operators()[operator_index];
        if (planner.kind == PlannerOperatorKind::Primitive &&
            planner.primitive_action < calc.registry().actions.size() &&
            calc.registry().actions[planner.primitive_action].params.type ==
                ActionType::Chaos) {
            chaos_operator = operator_index;
            break;
        }
    }
    const SparseRow* root_chaos_row = nullptr;
    if (chaos_operator != kNoId &&
        result.start_state < transition_cache->state_rows.size()) {
        for (const std::uint64_t row_index :
             state_row_indices(*transition_cache, result.start_state)) {
            const SparseRow& row = transition_cache->rows.at(row_index);
            if (row_has_operator(row, chaos_operator)) {
                root_chaos_row = &row;
                break;
            }
        }
    }
    const std::map<std::uint32_t, double> chaos_probabilities =
        root_chaos_row == nullptr
            ? std::map<std::uint32_t, double>{}
            : row_probabilities(*root_chaos_row);
    std::set<std::tuple<std::uint8_t, std::uint8_t, std::uint8_t>>
        chaos_shapes;
    std::array<double, kMaxGoalSlots + 1> chaos_probability_by_goal_count{};
    std::map<std::uint32_t, double> chaos_probability_by_goal_mask;
    double chaos_recorded_probability = 0.0;
    for (const auto& [state_id, probability] : chaos_probabilities) {
        if (state_id >= state_count) continue;
        const AbstractState& state = calc.state(state_id);
        chaos_shapes.emplace(
            state.rarity, state.prefix_count, state.suffix_count);
        const std::uint32_t mask =
            satisfied_goal_mask_for_state(state_id);
        const std::uint32_t count =
            std::min<std::uint32_t>(
                std::popcount(mask), kMaxGoalSlots);
        chaos_probability_by_goal_count[count] += probability;
        chaos_probability_by_goal_mask[mask] += probability;
        chaos_recorded_probability += probability;
    }
    const double chaos_terminal_probability =
        std::max(0.0, 1.0 - chaos_recorded_probability);

    struct SupportProfile {
        std::string action;
        std::uint64_t support = 0;
        std::uint64_t overlap = 0;
        std::uint64_t same_probability = 0;
        std::uint64_t different_probability = 0;
        std::uint64_t exceptional = 0;
        std::uint64_t persistent = 0;
        std::uint64_t rarity_or_affix_shape = 0;
        double probability = 0.0;
        double overlap_probability = 0.0;
        double exceptional_probability = 0.0;
        double persistent_probability = 0.0;
        double rarity_or_affix_shape_probability = 0.0;
        std::array<double, kMaxGoalSlots + 1> probability_by_goal_count{};
        std::map<std::uint32_t, double> probability_by_goal_mask;
    };
    std::vector<SupportProfile> support_profiles;
    for (const IncrementalAlternativeRow& retained :
         incremental_alternative_rows) {
        if (retained.state != result.start_state ||
            retained.row_index >= transition_cache->rows.size() ||
            retained.operator_index >= calc.operators().size()) {
            continue;
        }
        const AuditActionFamily family = audit_action_family(
            calc, retained.operator_index, restart_operator_index);
        if (family != AuditActionFamily::HarvestReforge &&
            family != AuditActionFamily::Fossil &&
            family != AuditActionFamily::Essence) {
            continue;
        }
        SupportProfile profile;
        profile.action = calc.operators()[retained.operator_index].id;
        const auto probabilities = row_probabilities(
            transition_cache->rows[retained.row_index]);
        for (const auto& [state_id, probability] : probabilities) {
            if (state_id >= state_count || !(probability > 0.0)) continue;
            ++profile.support;
            profile.probability += probability;
            const AbstractState& state = calc.state(state_id);
            const std::uint32_t mask =
                satisfied_goal_mask_for_state(state_id);
            const std::uint32_t count =
                std::min<std::uint32_t>(
                    std::popcount(mask), kMaxGoalSlots);
            profile.probability_by_goal_count[count] += probability;
            profile.probability_by_goal_mask[mask] += probability;
            const auto shared = chaos_probabilities.find(state_id);
            if (shared != chaos_probabilities.end()) {
                ++profile.overlap;
                profile.overlap_probability += probability;
                if (shared->second == probability) {
                    ++profile.same_probability;
                } else {
                    ++profile.different_probability;
                }
                continue;
            }
            const bool persistent =
                state.veiled_side >= 0 ||
                state.influence_bits != 0 ||
                state.searing_exarch_tier != 0 ||
                state.eater_of_worlds_tier != 0 ||
                state.goal_progress_retry_basin != 0 ||
                (state.flags &
                 (kFlagCorrupted | kFlagMirrored | kFlagSplit |
                  kFlagSynthesised | kFlagFractured |
                  kFlagCraftedMod | kProtectionFlags)) != 0 ||
                state.fractured_metamod_flags != 0;
            if (persistent) {
                ++profile.persistent;
                profile.persistent_probability += probability;
            } else if (!chaos_shapes.contains(
                           {state.rarity, state.prefix_count,
                            state.suffix_count})) {
                ++profile.rarity_or_affix_shape;
                profile.rarity_or_affix_shape_probability += probability;
            } else {
                ++profile.exceptional;
                profile.exceptional_probability += probability;
            }
        }
        support_profiles.push_back(std::move(profile));
    }
    std::stable_sort(
        support_profiles.begin(), support_profiles.end(),
        [](const SupportProfile& left, const SupportProfile& right) {
            return left.action < right.action;
        });

    struct OracleScenario {
        const char* name = "";
        double anchor_value = 0.0;
        double chaos_value = kInfinity;
    };
    const double chaos_cost =
        chaos_operator < operator_costs.size()
            ? operator_costs[chaos_operator]
            : kInfinity;
    double chaos_partial_probability = 0.0;
    for (std::size_t count = 1;
         count < kMaxGoalSlots; ++count) {
        chaos_partial_probability +=
            chaos_probability_by_goal_count[count];
    }
    const double chaos_zero_probability =
        chaos_probability_by_goal_count[0];
    const double chaos_exit_denominator =
        1.0 - chaos_zero_probability;
    std::array<OracleScenario, 5> oracle_scenarios{{
        {"current_fallback", incumbent.certified_upper_bound, kInfinity},
        {"anchor_10000", 10000.0, kInfinity},
        {"anchor_1000", 1000.0, kInfinity},
        {"anchor_500", 500.0, kInfinity},
        {"anchor_zero_ceiling", 0.0, kInfinity},
    }};
    for (OracleScenario& scenario : oracle_scenarios) {
        if (std::isfinite(chaos_cost) &&
            chaos_exit_denominator > 0.0) {
            scenario.chaos_value =
                (chaos_cost +
                 chaos_partial_probability * scenario.anchor_value) /
                chaos_exit_denominator;
        }
    }
    const double root_ten_percent_target =
        incumbent.certified_upper_bound * 0.9;
    const double maximum_anchor_for_root_ten_percent =
        chaos_partial_probability > 0.0
            ? (root_ten_percent_target * chaos_exit_denominator -
               chaos_cost) /
                  chaos_partial_probability
            : -kInfinity;

    struct LayoutAblation {
        std::string name;
        std::uint64_t action_count = 0;
        std::vector<std::string> exact_driver_actions;
        std::vector<std::string> discriminating_tags;
        std::uint64_t coarse_junk_classes = 0;
        std::uint64_t action_driven_junk_classes = 0;
        std::uint64_t forced_exact_junk_classes = 0;
        std::uint64_t coarse_chaos_support = 0;
        std::uint64_t action_driven_chaos_support = 0;
        std::uint64_t forced_exact_chaos_support = 0;
        std::uint64_t materialization_failures = 0;
    };
    ActionRegistryBuildOptions full_registry_options;
    full_registry_options.exhaustive_fossils = false;
    for (const ActionDescriptor& action : calc.registry().actions) {
        if (action.params.type == ActionType::Fossil) {
            full_registry_options.requested_fossil_action_ids.push_back(
                action.id);
        }
    }
    ActionRegistry full_registry =
        build_action_registry(session, full_registry_options);
    const auto full_index =
        [&](const std::string& id) -> std::optional<std::uint32_t> {
            const auto found = full_registry.index_by_id.find(id);
            if (found == full_registry.index_by_id.end()) {
                return std::nullopt;
            }
            return found->second;
        };
    std::vector<std::uint32_t> core_actions;
    for (const std::uint32_t action_index : calc.candidates()) {
        if (action_index >= calc.registry().actions.size()) continue;
        const ActionDescriptor& action =
            calc.registry().actions[action_index];
        if (action.params.type == ActionType::Fracture ||
            action.params.type == ActionType::RemoveCraftedModifiers ||
            action.params.type == ActionType::HarvestResist ||
            action.params.type == ActionType::Unveil) {
            continue;
        }
        if (const auto mapped = full_index(action.id)) {
            core_actions.push_back(*mapped);
        }
    }
    const auto add_type =
        [&](std::vector<std::uint32_t>& actions,
            const std::initializer_list<ActionType> types) {
            for (std::uint32_t index = 0;
                 index < full_registry.actions.size(); ++index) {
                if (std::find(
                        types.begin(), types.end(),
                        full_registry.actions[index].params.type) !=
                    types.end()) {
                    actions.push_back(index);
                }
            }
            std::sort(actions.begin(), actions.end());
            actions.erase(
                std::unique(actions.begin(), actions.end()),
                actions.end());
        };
    std::vector<std::pair<std::string, std::vector<std::uint32_t>>>
        layout_configurations;
    layout_configurations.push_back({"core", core_actions});
    auto with_fracture = core_actions;
    add_type(with_fracture, {ActionType::Fracture});
    layout_configurations.push_back(
        {"core_plus_fracture", std::move(with_fracture)});
    auto with_cleanup = core_actions;
    add_type(with_cleanup, {ActionType::RemoveCraftedModifiers});
    layout_configurations.push_back(
        {"core_plus_remove_crafted_modifiers", std::move(with_cleanup)});
    auto with_temporary_bench = core_actions;
    add_type(
        with_temporary_bench,
        {ActionType::Bench, ActionType::RemoveCraftedModifiers});
    layout_configurations.push_back(
        {"core_plus_automatic_temporary_bench_programs",
         with_temporary_bench});
    auto with_resist = core_actions;
    add_type(with_resist, {ActionType::HarvestResist});
    layout_configurations.push_back(
        {"core_plus_harvest_resistance_conversion",
         std::move(with_resist)});
    auto with_unveil = core_actions;
    add_type(with_unveil, {ActionType::Unveil});
    layout_configurations.push_back(
        {"core_plus_unveil", std::move(with_unveil)});
    add_type(with_temporary_bench, {ActionType::Fracture});
    layout_configurations.push_back(
        {"core_plus_fracture_plus_automatic_temporary_bench_programs",
         std::move(with_temporary_bench)});
    std::vector<std::uint32_t> complete_product;
    for (const ActionDescriptor& action : calc.registry().actions) {
        if (const auto mapped = full_index(action.id)) {
            complete_product.push_back(*mapped);
        }
    }
    std::sort(complete_product.begin(), complete_product.end());
    complete_product.erase(
        std::unique(complete_product.begin(), complete_product.end()),
        complete_product.end());
    layout_configurations.push_back(
        {"complete_current_product_vocabulary",
         std::move(complete_product)});
    std::vector<LayoutAblation> layout_ablations;
    for (auto& [name, actions] : layout_configurations) {
        bool action_driven_exact = false;
        LayoutAblation ablation;
        ablation.name = name;
        ablation.action_count = actions.size();
        for (const std::uint32_t index : actions) {
            const ActionDescriptor& action =
                full_registry.actions.at(index);
            const ActionType type = action.params.type;
            if (type == ActionType::Unveil ||
                type == ActionType::HarvestResist ||
                type == ActionType::Fracture ||
                type == ActionType::RemoveCraftedModifiers) {
                action_driven_exact = true;
                ablation.exact_driver_actions.push_back(action.id);
            }
        }
        const AbstractLayout coarse = build_abstract_layout(
            session, calc.goal(), full_registry, actions,
            false, false, false);
        const AbstractLayout driven = build_abstract_layout(
            session, calc.goal(), full_registry, actions,
            false, false, action_driven_exact);
        const AbstractLayout forced = build_abstract_layout(
            session, calc.goal(), full_registry, actions,
            false, false, true);
        ablation.coarse_junk_classes = coarse.junk_classes.size();
        ablation.action_driven_junk_classes =
            driven.junk_classes.size();
        ablation.forced_exact_junk_classes =
            forced.junk_classes.size();
        std::unordered_set<
            AbstractState, decltype(&abstract_state_hash)>
            coarse_support(0, abstract_state_hash);
        std::unordered_set<
            AbstractState, decltype(&abstract_state_hash)>
            driven_support(0, abstract_state_hash);
        std::unordered_set<
            AbstractState, decltype(&abstract_state_hash)>
            forced_support(0, abstract_state_hash);
        coarse_support.reserve(chaos_probabilities.size());
        driven_support.reserve(chaos_probabilities.size());
        forced_support.reserve(chaos_probabilities.size());
        for (const auto& [state_id, probability] :
             chaos_probabilities) {
            (void)probability;
            pc_item_state item{};
            if (state_id >= state_count ||
                !calc.materialize(state_id, item)) {
                ++ablation.materialization_failures;
                continue;
            }
            coarse_support.insert(project_item(session, coarse, item));
            driven_support.insert(project_item(session, driven, item));
            forced_support.insert(project_item(session, forced, item));
        }
        ablation.coarse_chaos_support = coarse_support.size();
        ablation.action_driven_chaos_support = driven_support.size();
        ablation.forced_exact_chaos_support = forced_support.size();
        for (const std::uint32_t tag : driven.discriminating_tag_ids) {
            const auto name =
                session.data->tag_name_by_id.find(tag);
            if (name != session.data->tag_name_by_id.end()) {
                ablation.discriminating_tags.push_back(
                    name->second);
            } else {
                ablation.discriminating_tags.push_back(
                    std::to_string(tag));
            }
        }
        layout_ablations.push_back(std::move(ablation));
    }

    std::string json = "{\"version\":1,\"observational\":true";
    json += ",\"root_successor_coverage\":{\"rows\":" +
            std::to_string(coverage_total.rows);
    json += ",\"successors\":" +
            std::to_string(coverage_total.successors);
    json += ",\"total_probability_mass\":" +
            finite_json(coverage_total.probability);
    json += ",\"non_restart_probability_mass\":" +
            finite_json(
                coverage_total.non_restart_probability);
    json += ",\"fallback_probability_mass\":{\"local_exact\":" +
            finite_json(coverage_total.local_probability);
    json += ",\"restart\":" +
            finite_json(coverage_total.restart_probability);
    json += ",\"chaos\":" +
            finite_json(coverage_total.chaos_probability);
    json += ",\"other\":" +
            finite_json(coverage_total.other_probability) + "}";
    json += ",\"total_upper_q_contribution\":" +
            finite_json(coverage_total.contribution);
    json += ",\"by_goal_count\":[";
    for (std::size_t count = 0;
         count <= calc.layout().slots.size(); ++count) {
        if (count != 0) json.push_back(',');
        json += "{\"count\":" + std::to_string(count);
        json += ",\"probability_mass\":" +
                finite_json(
                    coverage_total.probability_by_goal_count[count]);
        json += ",\"upper_q_contribution\":" +
                finite_json(
                    coverage_total.contribution_by_goal_count[count]) +
                "}";
    }
    json += "],\"by_goal_mask\":[";
    bool first_coverage_mask = true;
    for (const auto& [mask, probability] :
         coverage_total.probability_by_goal_mask) {
        if (!first_coverage_mask) json.push_back(',');
        first_coverage_mask = false;
        json += "{\"mask\":" + std::to_string(mask);
        json += ",\"probability_mass\":" + finite_json(probability);
        json += ",\"upper_q_contribution\":" +
                finite_json(
                    coverage_total.contribution_by_goal_mask.at(mask)) +
                "}";
    }
    json += "]";
    json += ",\"ranks\":[";
    const std::array<std::size_t, 4> ranks{32, 128, 512, 2048};
    double covered_contribution = 0.0;
    double covered_probability = 0.0;
    std::size_t coverage_cursor = 0;
    for (std::size_t rank_index = 0;
         rank_index < ranks.size(); ++rank_index) {
        const std::size_t covered =
            std::min(ranks[rank_index], coverage.size());
        while (coverage_cursor < covered) {
            covered_contribution +=
                coverage[coverage_cursor].contribution;
            covered_probability +=
                coverage[coverage_cursor].probability;
            ++coverage_cursor;
        }
        if (rank_index != 0) json.push_back(',');
        json += "{\"rank\":" +
                std::to_string(ranks[rank_index]);
        json += ",\"available\":" +
                std::string(
                    coverage.size() >= ranks[rank_index]
                        ? "true" : "false");
        json += ",\"covered_successors\":" +
                std::to_string(covered);
        json += ",\"probability_mass\":" +
                finite_json(covered_probability);
        json += ",\"upper_q_contribution\":" +
                finite_json(covered_contribution);
        json += ",\"contribution_fraction\":" +
                finite_json(
                    coverage_total.contribution > 0.0
                        ? covered_contribution /
                              coverage_total.contribution
                        : 0.0);
        json += "}";
    }
    json += "],\"actions\":[";
    bool first_action = true;
    for (const auto& [action_id, aggregate] :
         coverage_by_action) {
        if (!first_action) json.push_back(',');
        first_action = false;
        json += "{\"action\":";
        append_json_string(json, action_id);
        json += ",\"rows\":" + std::to_string(aggregate.rows);
        json += ",\"successors\":" +
                std::to_string(aggregate.successors);
        json += ",\"probability_mass\":" +
                finite_json(aggregate.probability);
        json += ",\"non_restart_probability_mass\":" +
                finite_json(
                    aggregate.non_restart_probability);
        json += ",\"upper_q_contribution\":" +
                finite_json(aggregate.contribution);
        json += ",\"lower_q\":" +
                finite_json(aggregate.lower_q);
        json += ",\"upper_q\":" +
                finite_json(aggregate.upper_q);
        json += ",\"by_goal_count\":[";
        for (std::size_t count = 0;
             count <= calc.layout().slots.size(); ++count) {
            if (count != 0) json.push_back(',');
            json += "{\"count\":" + std::to_string(count);
            json += ",\"probability_mass\":" +
                    finite_json(
                        aggregate.probability_by_goal_count[count]);
            json += ",\"upper_q_contribution\":" +
                    finite_json(
                        aggregate.contribution_by_goal_count[count]) +
                    "}";
        }
        json += "]";
        const double root_margin =
            std::isfinite(aggregate.upper_q)
                ? aggregate.upper_q -
                      incumbent.certified_upper_bound
                : -kInfinity;
        json += ",\"upper_q_above_root_incumbent\":" +
                finite_json(root_margin);
        json += ",\"upper_q_above_root_incumbent_fraction\":" +
                finite_json(
                    std::isfinite(root_margin) &&
                            incumbent.certified_upper_bound > 0.0
                        ? root_margin /
                              incumbent.certified_upper_bound
                        : -kInfinity);
        json += "}";
    }
    json += "]}";

    const auto append_census_aggregate =
        [&](const StateCensusAggregate& aggregate) {
            json += "\"states\":" + std::to_string(aggregate.states);
            json += ",\"expanded\":" +
                    std::to_string(aggregate.expanded);
            json += ",\"root_probability_mass\":" +
                    finite_json(aggregate.root_probability);
            json += ",\"root_upper_q_contribution\":" +
                    finite_json(
                        aggregate.root_upper_q_contribution);
        };
    json += ",\"state_census\":{\"by_goal_count\":[";
    for (std::size_t count = 0;
         count <= calc.layout().slots.size(); ++count) {
        if (count != 0) json.push_back(',');
        json += "{\"count\":" + std::to_string(count) + ",";
        append_census_aggregate(states_by_goal_count[count]);
        json += "}";
    }
    json += "],\"by_goal_mask\":[";
    bool first_state_mask = true;
    for (const auto& [mask, aggregate] : states_by_goal_mask) {
        if (!first_state_mask) json.push_back(',');
        first_state_mask = false;
        json += "{\"mask\":" + std::to_string(mask) + ",";
        append_census_aggregate(aggregate);
        json += "}";
    }
    json += "],\"by_below_tier_mask\":[";
    bool first_below_mask = true;
    for (const auto& [mask, aggregate] : states_by_below_mask) {
        if (!first_below_mask) json.push_back(',');
        first_below_mask = false;
        json += "{\"mask\":" + std::to_string(mask) + ",";
        append_census_aggregate(aggregate);
        json += "}";
    }
    json += "],\"by_occupancy\":[";
    bool first_state_occupancy = true;
    for (const auto& [occupancy_key, aggregate] :
         states_by_occupancy) {
        if (!first_state_occupancy) json.push_back(',');
        first_state_occupancy = false;
        json += "{\"prefixes\":" +
                std::to_string(occupancy_key.first);
        json += ",\"suffixes\":" +
                std::to_string(occupancy_key.second) + ",";
        append_census_aggregate(aggregate);
        json += "}";
    }
    json += "],\"by_nonzero_junk_classes\":[";
    bool first_nonzero_junk = true;
    for (const auto& [nonzero, aggregate] :
         states_by_nonzero_junk_classes) {
        if (!first_nonzero_junk) json.push_back(',');
        first_nonzero_junk = false;
        json += "{\"nonzero_classes\":" +
                std::to_string(nonzero) + ",";
        append_census_aggregate(aggregate);
        json += "}";
    }
    json += "],\"features\":{";
    bool first_state_feature = true;
    for (const auto& [name, aggregate] :
         std::array<std::pair<
             const char*, const StateCensusAggregate*>, 5>{{
             {"fractured", &state_feature_fractured},
             {"crafted", &state_feature_crafted},
             {"protected_or_metamod", &state_feature_protected},
             {"eldritch", &state_feature_eldritch},
             {"persistent", &state_feature_persistent},
         }}) {
        if (!first_state_feature) json.push_back(',');
        first_state_feature = false;
        append_json_string(json, name);
        json += ":{";
        append_census_aggregate(*aggregate);
        json += "}";
    }
    json += "}}";

    json += ",\"anchor_signatures\":{\"definition\":";
    append_json_string(
        json,
        "satisfied+below-tier masks, occupancy, goal blocking, fractured/crafted/protection, influence/Eldritch/persistent flags; disposable shell excluded and counted separately");
    json += ",\"groups\":" +
            std::to_string(anchor_summaries.size());
    json += ",\"retained\":[";
    const std::size_t anchor_limit =
        std::min<std::size_t>(
            anchor_summaries.size(),
            options.max_diagnostic_samples);
    for (std::size_t i = 0; i < anchor_limit; ++i) {
        if (i != 0) json.push_back(',');
        const AnchorSummary& summary = anchor_summaries[i];
        json += "{\"satisfied_mask\":" +
                std::to_string(std::get<0>(summary.key));
        json += ",\"below_tier_mask\":" +
                std::to_string(std::get<1>(summary.key));
        json += ",\"prefixes\":" +
                std::to_string(std::get<2>(summary.key));
        json += ",\"suffixes\":" +
                std::to_string(std::get<3>(summary.key));
        json += ",\"blocked_mask\":" +
                std::to_string(std::get<4>(summary.key));
        json += ",\"fractured_goal_mask\":" +
                std::to_string(std::get<5>(summary.key));
        json += ",\"crafted_goal_mask\":" +
                std::to_string(std::get<6>(summary.key));
        json += ",\"protection_flags\":" +
                std::to_string(std::get<7>(summary.key));
        json += ",\"influence_bits\":" +
                std::to_string(std::get<8>(summary.key));
        json += ",\"searing_tier\":" +
                std::to_string(std::get<9>(summary.key));
        json += ",\"eater_tier\":" +
                std::to_string(std::get<10>(summary.key));
        json += ",\"persistent_flags\":" +
                std::to_string(std::get<11>(summary.key));
        json += ",\"strict_states\":" +
                std::to_string(summary.aggregate.states.size());
        json += ",\"disposable_shell_variants\":" +
                std::to_string(
                    summary.aggregate.shell_variants.size());
        json += ",\"probability_mass\":" +
                finite_json(summary.aggregate.probability);
        json += ",\"upper_q_contribution\":" +
                finite_json(summary.aggregate.contribution);
        json += ",\"fallback_probability_mass\":{\"local_exact\":" +
                finite_json(summary.aggregate.local_probability);
        json += ",\"restart\":" +
                finite_json(summary.aggregate.restart_probability);
        json += ",\"chaos\":" +
                finite_json(summary.aggregate.chaos_probability);
        json += ",\"other\":" +
                finite_json(summary.aggregate.other_probability) +
                "}}";
    }
    json += "],\"omitted\":" +
            std::to_string(anchor_summaries.size() - anchor_limit) +
            "}";

    json += ",\"chaos_support\":{\"available\":" +
            std::string(root_chaos_row != nullptr ? "true" : "false");
    json += ",\"support\":" +
            std::to_string(chaos_probabilities.size());
    json += ",\"recorded_probability_mass\":" +
            finite_json(chaos_recorded_probability);
    json += ",\"terminal_probability_mass\":" +
            finite_json(chaos_terminal_probability);
    json += ",\"by_goal_count\":[";
    for (std::size_t count = 0;
         count <= calc.layout().slots.size(); ++count) {
        if (count != 0) json.push_back(',');
        json += "{\"count\":" + std::to_string(count);
        json += ",\"probability_mass\":" +
                finite_json(chaos_probability_by_goal_count[count]) +
                "}";
    }
    json += "],\"by_goal_mask\":[";
    bool first_chaos_mask = true;
    for (const auto& [mask, probability] :
         chaos_probability_by_goal_mask) {
        if (!first_chaos_mask) json.push_back(',');
        first_chaos_mask = false;
        json += "{\"mask\":" + std::to_string(mask);
        json += ",\"probability_mass\":" +
                finite_json(probability) + "}";
    }
    json += "],\"other_root_actions\":[";
    for (std::size_t i = 0; i < support_profiles.size(); ++i) {
        if (i != 0) json.push_back(',');
        const SupportProfile& profile = support_profiles[i];
        json += "{\"action\":";
        append_json_string(json, profile.action);
        json += ",\"support\":" + std::to_string(profile.support);
        json += ",\"probability_mass\":" +
                finite_json(profile.probability);
        json += ",\"shared_with_chaos\":{\"states\":" +
                std::to_string(profile.overlap);
        json += ",\"probability_mass\":" +
                finite_json(profile.overlap_probability);
        json += ",\"same_probability_states\":" +
                std::to_string(profile.same_probability);
        json += ",\"different_probability_states\":" +
                std::to_string(profile.different_probability) + "}";
        json += ",\"exceptional_support\":{\"states\":" +
                std::to_string(profile.exceptional);
        json += ",\"probability_mass\":" +
                finite_json(profile.exceptional_probability) + "}";
        json += ",\"persistent_state\":{\"states\":" +
                std::to_string(profile.persistent);
        json += ",\"probability_mass\":" +
                finite_json(profile.persistent_probability) + "}";
        json += ",\"rarity_or_affix_shape\":{\"states\":" +
                std::to_string(profile.rarity_or_affix_shape);
        json += ",\"probability_mass\":" +
                finite_json(
                    profile.rarity_or_affix_shape_probability) +
                "},\"by_goal_count\":[";
        for (std::size_t count = 0;
             count <= calc.layout().slots.size(); ++count) {
            if (count != 0) json.push_back(',');
            json += "{\"count\":" + std::to_string(count);
            json += ",\"probability_mass\":" +
                    finite_json(
                        profile.probability_by_goal_count[count]) +
                    "}";
        }
        json += "],\"by_goal_mask\":[";
        bool first_profile_mask = true;
        for (const auto& [mask, probability] :
             profile.probability_by_goal_mask) {
            if (!first_profile_mask) json.push_back(',');
            first_profile_mask = false;
            json += "{\"mask\":" + std::to_string(mask);
            json += ",\"probability_mass\":" +
                    finite_json(probability) + "}";
        }
        json += "]}";
    }
    json += "]}";

    json += ",\"anchor_oracle\":{\"chaos\":{\"cost\":" +
            finite_json(chaos_cost);
    json += ",\"zero_goal_probability_mass\":" +
            finite_json(chaos_zero_probability);
    json += ",\"one_to_three_goal_probability_mass\":" +
            finite_json(chaos_partial_probability);
    json += ",\"terminal_probability_mass\":" +
            finite_json(chaos_terminal_probability);
    json += ",\"ten_percent_root_target\":" +
            finite_json(root_ten_percent_target);
    json += ",\"maximum_uniform_anchor_for_ten_percent_root_reduction\":" +
            finite_json(maximum_anchor_for_root_ten_percent);
    json += "},\"scenarios\":[";
    for (std::size_t scenario_index = 0;
         scenario_index < oracle_scenarios.size();
         ++scenario_index) {
        if (scenario_index != 0) json.push_back(',');
        const OracleScenario& scenario =
            oracle_scenarios[scenario_index];
        json += "{\"scenario\":";
        append_json_string(json, scenario.name);
        json += ",\"uniform_anchor_value\":" +
                finite_json(scenario.anchor_value);
        json += ",\"root_chaos_upper\":" +
                finite_json(scenario.chaos_value);
        json += ",\"root_reduction_percent\":" +
                finite_json(
                    incumbent.certified_upper_bound > 0.0 &&
                            std::isfinite(scenario.chaos_value)
                        ? 100.0 *
                              (incumbent.certified_upper_bound -
                               scenario.chaos_value) /
                              incumbent.certified_upper_bound
                        : -kInfinity);
        json += ",\"crosses_root_ten_percent_gate\":" +
                std::string(
                    scenario.chaos_value <=
                            root_ten_percent_target
                        ? "true" : "false");
        json += ",\"root_actions\":[";
        bool first_oracle_action = true;
        for (const auto& [action_id, aggregate] :
             coverage_by_action) {
            if (!first_oracle_action) json.push_back(',');
            first_oracle_action = false;
            double action_cost =
                std::isfinite(aggregate.upper_q)
                    ? aggregate.upper_q - aggregate.contribution
                    : kInfinity;
            double repriced = action_cost;
            repriced +=
                aggregate.probability_by_goal_count[0] *
                scenario.chaos_value;
            for (std::size_t count = 1;
                 count < kMaxGoalSlots; ++count) {
                repriced +=
                    aggregate.probability_by_goal_count[count] *
                    scenario.anchor_value;
            }
            json += "{\"action\":";
            append_json_string(json, action_id);
            json += ",\"repriced_upper_q\":" +
                    finite_json(repriced);
            json += ",\"reduction_percent\":" +
                    finite_json(
                        std::isfinite(aggregate.upper_q) &&
                                aggregate.upper_q > 0.0
                            ? 100.0 *
                                  (aggregate.upper_q - repriced) /
                                  aggregate.upper_q
                            : -kInfinity);
            json += ",\"crosses_ten_percent_row_gate\":" +
                    std::string(
                        std::isfinite(aggregate.upper_q) &&
                                repriced <= aggregate.upper_q * 0.9
                            ? "true" : "false");
            json += ",\"strictly_below_repriced_chaos\":" +
                    std::string(
                        repriced < scenario.chaos_value
                            ? "true" : "false") +
                    "}";
        }
        json += "]}";
    }
    json += "]}";

    std::vector<std::uint32_t> actual_layout_actions =
        calc.candidates();
    bool dependency_cleanup_in_layout_actions = false;
    for (std::uint32_t operator_index =
             static_cast<std::uint32_t>(
                 calc.registry().actions.size());
         operator_index < calc.operators().size(); ++operator_index) {
        const PlannerOperator& planner =
            calc.operators()[operator_index];
        for (const std::uint32_t dependency :
             planner.primitive_program) {
            if (std::find(
                    actual_layout_actions.begin(),
                    actual_layout_actions.end(), dependency) ==
                actual_layout_actions.end()) {
                actual_layout_actions.push_back(dependency);
            }
        }
        if (planner.conditional_action != kNoId &&
            std::find(
                actual_layout_actions.begin(),
                actual_layout_actions.end(),
                planner.conditional_action) ==
                actual_layout_actions.end()) {
            actual_layout_actions.push_back(
                planner.conditional_action);
        }
    }
    for (const std::uint32_t index : actual_layout_actions) {
        if (index < calc.registry().actions.size() &&
            calc.registry().actions[index].params.type ==
                ActionType::RemoveCraftedModifiers &&
            std::find(
                calc.candidates().begin(), calc.candidates().end(),
                index) == calc.candidates().end()) {
            dependency_cleanup_in_layout_actions = true;
        }
    }
    json += ",\"abstraction_ablation\":{\"actual_parent\":{";
    json += "\"layout_actions\":" +
            std::to_string(actual_layout_actions.size());
    json += ",\"dependency_cleanup_in_layout_actions\":" +
            std::string(
                dependency_cleanup_in_layout_actions
                    ? "true" : "false");
    json += ",\"caller_forced_exact_group_effects\":";
    json += calc.product_solver_parent() ? "false" : "true";
    json += ",\"action_driven_exact_group_effects\":";
    bool current_action_driven_exact = false;
    std::vector<std::string> current_exact_drivers;
    for (const std::uint32_t index : actual_layout_actions) {
        if (index >= calc.registry().actions.size()) continue;
        const ActionDescriptor& action =
            calc.registry().actions[index];
        const ActionType type = action.params.type;
        if (type == ActionType::Unveil ||
            type == ActionType::HarvestResist ||
            (type == ActionType::Fracture &&
             !calc.product_solver_parent()) ||
            type == ActionType::RemoveCraftedModifiers) {
            current_action_driven_exact = true;
            current_exact_drivers.push_back(action.id);
        }
    }
    json += std::string(
        current_action_driven_exact ? "true" : "false");
    json += ",\"exact_driver_actions\":[";
    for (std::size_t i = 0; i < current_exact_drivers.size(); ++i) {
        if (i != 0) json.push_back(',');
        append_json_string(json, current_exact_drivers[i]);
    }
    json += "],\"junk_classes\":" +
            std::to_string(calc.layout().junk_classes.size()) +
            "},\"configurations\":[";
    for (std::size_t i = 0; i < layout_ablations.size(); ++i) {
        if (i != 0) json.push_back(',');
        const LayoutAblation& ablation = layout_ablations[i];
        json += "{\"name\":";
        append_json_string(json, ablation.name);
        json += ",\"actions\":" +
                std::to_string(ablation.action_count);
        json += ",\"exact_driver_actions\":[";
        for (std::size_t driver = 0;
             driver < ablation.exact_driver_actions.size(); ++driver) {
            if (driver != 0) json.push_back(',');
            append_json_string(
                json, ablation.exact_driver_actions[driver]);
        }
        json += "],\"discriminating_tags\":[";
        for (std::size_t tag = 0;
             tag < ablation.discriminating_tags.size(); ++tag) {
            if (tag != 0) json.push_back(',');
            append_json_string(
                json, ablation.discriminating_tags[tag]);
        }
        json += "],\"coarse_junk_classes\":" +
                std::to_string(ablation.coarse_junk_classes);
        json += ",\"action_driven_junk_classes\":" +
                std::to_string(
                    ablation.action_driven_junk_classes);
        json += ",\"forced_exact_junk_classes\":" +
                std::to_string(
                    ablation.forced_exact_junk_classes);
        json += ",\"coarse_initial_chaos_support\":" +
                std::to_string(ablation.coarse_chaos_support);
        json += ",\"action_driven_initial_chaos_support\":" +
                std::to_string(
                    ablation.action_driven_chaos_support);
        json += ",\"forced_exact_initial_chaos_support\":" +
                std::to_string(
                    ablation.forced_exact_chaos_support);
        json += ",\"materialization_failures\":" +
                std::to_string(ablation.materialization_failures) +
                "}";
    }
    json += "]}";

    json += ",\"discovery_fanout\":{\"families\":[";
    for (std::size_t family = 0;
         family < kAuditActionFamilyCount; ++family) {
        if (family != 0) json.push_back(',');
        const FanoutAggregate& aggregate = fanout[family];
        json += "{\"family\":";
        append_json_string(
            json,
            audit_action_family_name(
                static_cast<AuditActionFamily>(family)));
        json += ",\"rows\":" + std::to_string(aggregate.rows);
        json += ",\"successor_entries\":" +
                std::to_string(aggregate.successor_entries);
        json += ",\"mean_row_fanout\":" +
                finite_json(
                    aggregate.rows > 0
                        ? static_cast<double>(
                              aggregate.successor_entries) /
                              static_cast<double>(aggregate.rows)
                        : 0.0);
        json += ",\"max_row_fanout\":" +
                std::to_string(aggregate.max_row_fanout);
        json += ",\"observed_states\":" +
                std::to_string(aggregate.observed_states);
        json += ",\"observed_zero_progress_states\":" +
                std::to_string(
                    aggregate.observed_zero_progress_states);
        json += ",\"by_source_goal_count\":[";
        for (std::size_t count = 0;
             count <= calc.layout().slots.size(); ++count) {
            if (count != 0) json.push_back(',');
            const SourceFanoutAggregate& source =
                fanout_by_source_goal_count[family][count];
            json += "{\"count\":" + std::to_string(count);
            json += ",\"rows\":" +
                    std::to_string(source.rows);
            json += ",\"successor_entries\":" +
                    std::to_string(source.successor_entries);
            json += ",\"max_row_fanout\":" +
                    std::to_string(source.max_row_fanout) +
                    "}";
        }
        json += "]}";
    }
    json += "]}";

    json += ",\"zero_progress\":{\"states\":" +
            std::to_string(zero_states);
    json += ",\"ordinary\":" +
            std::to_string(ordinary_zero_states);
    json += ",\"retry_basin\":" +
            std::to_string(retry_basin_states);
    json += ",\"live_renewable\":" +
            std::to_string(live_renewable_states);
    json += ",\"certified_dead_or_irreversible\":" +
            std::to_string(certified_dead_states);
    json += ",\"no_legal_renewal\":" +
            std::to_string(no_legal_renewal_states);
    json += ",\"nonrenewal_observer\":" +
            std::to_string(nonrenewal_observer_states);
    json += ",\"nonrenewal_observer_actions\":[";
    bool first_observer = true;
    for (const auto& [action_id, states] :
         nonrenewal_observer_actions) {
        if (!first_observer) json.push_back(',');
        first_observer = false;
        json += "{\"action\":";
        append_json_string(json, action_id);
        json += ",\"legal_state_observations\":" +
                std::to_string(states) + "}";
    }
    json += "]";
    json += ",\"producer_unknown\":" +
            std::to_string(unknown_producer_states);
    json += ",\"rarity\":[";
    for (std::size_t rarity = 0;
         rarity < rarity_counts.size(); ++rarity) {
        if (rarity != 0) json.push_back(',');
        json += std::to_string(rarity_counts[rarity]);
    }
    json += "],\"occupancy\":[";
    bool first_occupancy = true;
    for (std::size_t prefix = 0; prefix < occupancy.size();
         ++prefix) {
        for (std::size_t suffix = 0;
             suffix < occupancy[prefix].size(); ++suffix) {
            if (occupancy[prefix][suffix] == 0) continue;
            if (!first_occupancy) json.push_back(',');
            first_occupancy = false;
            json += "{\"prefixes\":" + std::to_string(prefix);
            json += ",\"suffixes\":" + std::to_string(suffix);
            json += ",\"states\":" +
                    std::to_string(occupancy[prefix][suffix]) + "}";
        }
    }
    json += "],\"features\":{\"explicit_affixes\":" +
            std::to_string(explicit_affix_states);
    json += ",\"fractured\":" + std::to_string(fractured_states);
    json += ",\"crafted\":" + std::to_string(crafted_states);
    json += ",\"metamod_or_protection\":" +
            std::to_string(metamod_or_protection_states);
    json += ",\"influenced\":" + std::to_string(influenced_states);
    json += ",\"corrupted_or_mirrored\":" +
            std::to_string(corrupted_or_mirrored_states);
    json += ",\"eldritch\":" + std::to_string(eldritch_states);
    json += ",\"veiled\":" + std::to_string(veiled_states);
    json += ",\"persistent_setup\":" +
            std::to_string(persistent_setup_states) + "}";
    json += ",\"producer_observations\":[";
    for (std::size_t family = 0;
         family < kAuditActionFamilyCount; ++family) {
        if (family != 0) json.push_back(',');
        json += "{\"family\":";
        append_json_string(
            json,
            audit_action_family_name(
                static_cast<AuditActionFamily>(family)));
        json += ",\"states\":" +
                std::to_string(zero_producer_counts[family]) + "}";
    }
    json += "],\"renewal_signatures\":{\"exact_groups\":" +
            std::to_string(signature_groups.size());
    json += ",\"retained\":[";
    const std::size_t signature_limit =
        std::min<std::size_t>(
            signature_summaries.size(),
            options.max_diagnostic_samples);
    for (std::size_t i = 0; i < signature_limit; ++i) {
        if (i != 0) json.push_back(',');
        json += "{\"hash\":";
        append_hash_string(json, signature_summaries[i].hash);
        json += ",\"states\":" +
                std::to_string(
                    signature_summaries[i].aggregate.states);
        json += ",\"safe_states\":" +
                std::to_string(
                    signature_summaries[i]
                        .aggregate.safe_states);
        json += ",\"retry_basin_states\":" +
                std::to_string(
                    signature_summaries[i]
                        .aggregate.retry_basin_states);
        json += ",\"explicit_affix_states\":" +
                std::to_string(
                    signature_summaries[i]
                        .aggregate.explicit_affix_states) + "}";
    }
    json += "],\"omitted\":" +
            std::to_string(
                signature_summaries.size() - signature_limit) + "}";
    json += ",\"canonicalization\":{\"existing_retry_basin_states\":" +
            std::to_string(retry_basin_states);
    json += ",\"safe_equivalent_states\":" +
            std::to_string(safe_equivalent_states);
    json += ",\"additional_canonicalizable_states\":" +
            std::to_string(additional_canonicalizable_states);
    json += ",\"discarded_explicit_equivalent_states\":" +
            std::to_string(
                discarded_explicit_equivalent_states);
    json += ",\"merge_applied\":false";
    json += ",\"reason\":";
    append_json_string(
        json,
        additional_canonicalizable_states == 0
            ? "no_new_state_passed_exact_renewal_and_nonrenewal_observer_contract"
            : "eligible_states_require_behavioral_integration");
    json += "}}}";

    diagnostics.upper_cap_zero_progress_audit_json =
        std::move(json);
}

} // namespace solver
} // namespace poecraft
