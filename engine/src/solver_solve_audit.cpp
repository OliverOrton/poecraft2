#include "solver_solve_types.hpp"

#include <array>
#include <bit>
#include <map>

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
            coverage.push_back({
                contribution, probability, state, parent_operator});
            const std::string& action_id =
                calc.operators()[parent_operator].id;
            CoverageAggregate& action =
                coverage_by_action[action_id];
            ++action.successors;
            action.probability += probability;
            action.contribution += contribution;
            ++coverage_total.successors;
            coverage_total.probability += probability;
            coverage_total.contribution += contribution;
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
                if (upper < selected_upper - options.epsilon ||
                    (std::abs(upper - selected_upper) <=
                         options.epsilon &&
                     successor < selected)) {
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
    std::array<FanoutAggregate, kAuditActionFamilyCount> fanout{};
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
                    aggregate.observed_zero_progress_states) + "}";
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
