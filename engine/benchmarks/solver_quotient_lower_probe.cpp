/* Opt-in bounded lower query. No SolveWork::advance, compiled strategy, or
 * Simulator; all native dependencies come from the normal engine target. */
#include "poecraft/api.h"
#include "poecraft/session.h"
#include "poecraft/simulator.h"
#include "handles_internal.hpp"
#include "solver_diagnostic_options.hpp"
#include "solver_quotient_bellman.hpp"
#include "solver_solve_types.hpp"
#include "solver_phase_lower.hpp"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif

namespace poecraft::solver {
struct SolveWorkTestAccess { using Impl = SolveWork::Impl; };
}
using namespace poecraft;
using namespace poecraft::solver;
using namespace poecraft::solver::quotient;

namespace {
using Clock = std::chrono::steady_clock;
std::uint64_t ns(Clock::time_point start) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start).count();
}
std::string read(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot read " + path);
    return {std::istreambuf_iterator<char>(in), {}};
}
StableKey key(const std::string& text) {
    StableKey out{text.size()};
    for (std::size_t start = 0; start < text.size(); start += 8) {
        std::uint64_t word = 0;
        for (std::size_t i = 0; i < 8 && start + i < text.size(); ++i)
            word |= static_cast<std::uint64_t>(static_cast<unsigned char>(text[start + i])) << (8 * i);
        out.push_back(word);
    }
    return out;
}
void emit_key(const StableKey& value) {
    std::cout << '[';
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (i) std::cout << ',';
        std::cout << '"' << value[i] << '"';
    }
    std::cout << ']';
}
void check(pc_result result, const pc_error_info& error) {
    if (result != PC_RESULT_OK) throw std::runtime_error(error.message);
}
std::uint64_t process_peak() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS counters{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)))
        return counters.PeakWorkingSetSize;
#endif
    return 0;
}
struct Handles {
    pc_data_handle data{};
    pc_session_handle session{};
    pc_solver_handle solver{};
    pc_economy_handle economy{};
    ~Handles() {
        if (solver) pc_solver_destroy(solver);
        if (economy) pc_economy_destroy(economy);
        if (session) pc_session_destroy(session);
        if (data) pc_data_destroy(data);
    }
};
struct NativeRow {
    std::uint32_t source;
    std::uint32_t operation;
    double cost;
    bool legal;
    std::vector<OutcomeEntry> entries;
    QuotientLowerConstraint constraint;
};

void micro(CalcContext& calc, SolveWorkTestAccess::Impl& owner,
           const StableKey& request_key, const StableKey& scope_key) {
    const auto before = owner.audited_estimated_owned_bytes();
    const auto started = Clock::now();
    std::vector<NativeRow> rows;
    std::uint64_t transitions = 0, illegal = 0;
    for (std::uint32_t state = 0; state < calc.state_count(); ++state) {
        if (state >= 8) throw std::runtime_error("micro state budget exceeded");
        if (calc.is_goal_state(calc.state(state))) continue;
        for (const auto& priced : owner.operators) {
            if (!calc.is_candidate_operator_admitted_for_state(state, priced.index))
                throw std::runtime_error("micro caller admission changed");
            if (rows.size() >= 21) throw std::runtime_error("micro action budget exceeded");
            const auto& op = calc.operators().at(priced.index);
            if (op.kind != PlannerOperatorKind::Primitive)
                throw std::runtime_error("micro unexpectedly includes program");
            const auto& action = calc.registry().actions.at(op.primitive_action);
            // Caller admission comes from the actual priced operator set.
            const bool legal = action_legal(calc.session(), action, calc.state(state));
            NativeRow row{state, priced.index, priced.cost, legal, {}, {}};
            if (legal) {
                const auto& kernel = calc.outcomes(state, op.primitive_action, false);
                if (!kernel.supported || !kernel.applicable || !kernel.choice_groups.empty())
                    throw std::runtime_error("micro kernel incomplete");
                row.entries = kernel.entries;
                transitions += row.entries.size();
                if (transitions > 56) throw std::runtime_error("micro transition budget exceeded");
            } else ++illegal;
            rows.push_back(std::move(row));
        }
    }
    const auto query_ns = ns(started);
    if (rows.size() != 21 || transitions != 56 || illegal != 7 || calc.state_count() != 8)
        throw std::runtime_error("current micro differs from archived scope");
    QuotientBellmanGraph graph(16ull * 1024 * 1024, QuotientBellmanMode::LowerOnly);
    std::vector<QuotientBellmanCellInput> cells;
    std::vector<StableKey> identities;
    std::vector<double> lower;
    for (std::uint32_t state = 0; state < calc.state_count(); ++state) {
        pc_item_state item{};
        if (!calc.materialize(state, item)) throw std::runtime_error("cannot materialize micro member");
        identities.push_back(exact_item_state_key(item));
        const bool goal = calc.is_goal_state(calc.state(state));
        cells.push_back({state, 1, identities.back(), goal});
        lower.push_back(goal ? 0 : owner.completion_proof_lower_value(state));
    }
    graph.install_cells(std::move(cells));
    for (auto& row : rows) {
        const auto action_key = planner_operator_semantic_key(calc.operators().at(row.operation));
        auto& constraint = row.constraint;
        constraint.cover.identity = action_key;
        constraint.evidence_identity = key("native_action_legal/current-kernel/v2");
        if (!row.legal) {
            constraint.kind = LowerConstraintKind::Inapplicable;
            constraint.evidence = LowerEvidenceKind::ExactInapplicability;
            continue;
        }
        QuotientBellmanRowInput input;
        input.source_cell_id = row.source;
        input.operator_index = row.operation;
        input.cost = row.cost;
        input.lower_provenance = QuotientLowerRowProvenance{
            request_key, identities[row.source], action_key, constraint.evidence_identity,
            LowerEvidenceKind::ExactDeclaredKernel};
        for (const auto& entry : row.entries)
            input.transitions.push_back({{entry.state}, entry.state, entry.probability});
        constraint.row = graph.append_row(std::move(input));
        constraint.kind = LowerConstraintKind::Row;
        constraint.evidence = LowerEvidenceKind::ExactDeclaredKernel;
    }
    std::cout << "\"caller_actions\":[\"transmute\",\"alteration\",\"restart\"],\"states\":[";
    for (std::uint32_t state = 0; state < calc.state_count(); ++state) {
        if (state) std::cout << ',';
        std::cout << "{\"id\":" << state << ",\"goal\":"
            << (calc.is_goal_state(calc.state(state)) ? "true" : "false")
            << ",\"lower\":" << lower[state] << ",\"canonical_item_key\":";
        emit_key(identities[state]);
        std::cout << '}';
    }
    std::cout << "],\"rows\":[";
    bool first = true;
    for (const auto& row : rows) {
        if (!first) std::cout << ',';
        first = false;
        const auto& op = calc.operators().at(row.operation);
        std::cout << "{\"source\":" << row.source << ",\"action\":\""
            << calc.registry().actions.at(op.primitive_action).id << "\",\"cost\":" << row.cost
            << ",\"supported\":true,\"applicable\":" << (row.legal ? "true" : "false")
            << ",\"inapplicability_owner\":\"native_action_legal\",\"entries\":[";
        for (std::size_t i = 0; i < row.entries.size(); ++i) {
            if (i) std::cout << ',';
            std::cout << "{\"target\":" << row.entries[i].state
                << ",\"p\":" << row.entries[i].probability << '}';
        }
        std::cout << "]}";
    }
    std::cout << "],\"models\":[";
    for (unsigned mode = 0; mode < 2; ++mode) {
        for (unsigned stage = 0; stage < 4; ++stage) {
            if (mode || stage) std::cout << ',';
            QuotientLowerQuery q;
            q.request_identity = request_key;
            q.caller_scope = scope_key;
            q.model_revision = graph.model_revision();
            q.coefficients = mode ? LowerCoefficientModel::NormalizedStoredReference :
                LowerCoefficientModel::RawStoredCoefficients;
            q.roots = {0};
            for (std::uint32_t state = 0; state < calc.state_count(); ++state) {
                if (calc.is_goal_state(calc.state(state))) continue;
                if (stage != 3 && state != 0 && !(stage == 2 && state == 7)) {
                    q.boundaries.push_back({state, identities[state], key("existing-pattern-lower/v2"),
                        lower[state], LowerEvidenceKind::IndependentLower});
                    continue;
                }
                QuotientLowerSource source;
                source.cell_id = state;
                source.source_identity = identities[state];
                source.expected_actions = {scope_key, 1, true, {}, {}};
                for (const auto& priced : owner.operators)
                    if (calc.is_candidate_operator_admitted_for_state(state, priced.index))
                        source.expected_actions.actions.push_back(
                            planner_operator_semantic_key(calc.operators().at(priced.index)));
                for (const auto& row : rows) {
                    if (row.source != state) continue;
                    auto constraint = row.constraint;
                    if (!stage && row.legal) {
                        constraint.kind = LowerConstraintKind::Scalar;
                        constraint.lower = std::max(lower[state], row.cost);
                        constraint.evidence = LowerEvidenceKind::IndependentLower;
                    }
                    source.constraints.push_back(std::move(constraint));
                }
                q.sources.push_back(std::move(source));
            }
            const auto begin = Clock::now();
            const auto result = graph.solve_lower(q);
            if (!result.checked) throw std::runtime_error("micro lower refused: " + result.reason);
            std::cout << "{\"stage\":" << stage << ",\"coefficient_mode\":\""
                << (mode ? "normalized_stored_reference" : "coefficient_model_only")
                << "\",\"status\":\"" << quotient_lower_status_name(result.status)
                << "\",\"root\":" << result.checked->values_by_state[0]
                << ",\"sweeps\":" << result.sweeps << ",\"query_ns\":" << ns(begin)
                << ",\"proof_owned_bytes_with_result\":" << graph.proof_store()->ledger().snapshot().total_bytes
                << ",\"values\":[";
            for (std::size_t i = 0; i < result.checked->values_by_state.size(); ++i) {
                if (i) std::cout << ',';
                std::cout << result.checked->values_by_state[i];
            }
            std::cout << "]}";
        }
    }
    std::cout << "],\"metrics\":{\"queries\":" << rows.size()
        << ",\"applicable_rows\":" << rows.size() - illegal << ",\"outcomes\":" << transitions
        << ",\"query_ns\":" << query_ns << ",\"owned_before\":" << before
        << ",\"owned_after\":" << owner.audited_estimated_owned_bytes()
        << ",\"model_retained_bytes\":" << graph.proof_store()->ledger().snapshot().total_bytes
        << ",\"model_peak_bytes\":" << graph.proof_store()->ledger().snapshot().peak_total_bytes << '}';
}

void medium_coverage(CalcContext& calc, SolveWorkTestAccess::Impl& owner) {
    const auto root = owner.result.start_state;
    // Same prepared stage as the archive. No root envelope refresh, donor,
    // successor expansion, or speculative reuse is run in this contract audit.
    const double baseline = owner.completion_proof_lower_value(root);
    std::cout << "\"root_lower\":" << baseline << ",\"root_identity\":";
    pc_item_state item{};
    if (!calc.materialize(root, item)) throw std::runtime_error("cannot materialize medium root");
    emit_key(exact_item_state_key(item));
    std::cout << ",\"canonical_actions\":[";
    CanonicalActionSet expected{key("medium/caller-and-generation/v2"), 1, true, {}, {}};
    std::vector<CanonicalActionCover> cover;
    std::map<unsigned, std::vector<StableKey>> generated_members;
    bool first = true;
    for (const auto& priced : owner.operators) {
        if (!calc.is_candidate_operator_admitted_for_state(root, priced.index)) continue;
        const auto& op = calc.operators().at(priced.index);
        if (!first) std::cout << ',';
        first = false;
        const auto action_key = planner_operator_semantic_key(op);
        if (op.automatic_kind == AutomaticCandidateKind::None)
            expected.actions.push_back(action_key);
        else generated_members[static_cast<unsigned>(op.automatic_kind)].push_back(action_key);
        cover.push_back({action_key, false, {}});
        bool illegal = false;
        if (op.kind == PlannerOperatorKind::Primitive)
            illegal = !action_legal(calc.session(), calc.registry().actions.at(op.primitive_action), calc.state(root));
        std::cout << "{\"id\":\"" << op.id << "\",\"identity\":";
        emit_key(action_key);
        std::cout << ",\"representation\":\"" << (illegal ? "native_inapplicable" : "independent_root_placeholder")
            << "\",\"placeholder\":" << baseline << '}';
    }
    std::cout << "],\"residual_families\":[";
    first = true;
    for (unsigned kind = 1; kind <= static_cast<unsigned>(AutomaticCandidateKind::Veiled); ++kind) {
        const bool open = calc.goal().automatic_candidates &&
            (calc.goal().automatic_candidate_kind_mask & (1u << kind));
        const auto& members = generated_members[kind];
        if (!open && members.empty()) continue;
        const StableKey family_key{0x46414d494c59, kind};
        expected.families.push_back({family_key, members, open});
        if (!open) continue;
        if (!first) std::cout << ',';
        first = false;
        cover.push_back({family_key, true, members});
        std::cout << "{\"automatic_kind\":" << kind
            << ",\"partition\":\"future_members_excluding_current_explicit_catalogue\",\"placeholder\":"
            << baseline << ",\"excluded_current_members\":" << members.size() << '}';
    }
    const auto failure = validate_canonical_action_coverage(expected, cover);
    if (!failure.empty()) throw std::runtime_error(failure);
    std::cout << "],\"coverage\":\"canonical_current_catalogue_plus_open_generated_residuals\""
        << ",\"independent_portfolio_gain\":0,\"new_native_action_authority\":false"
        << ",\"donor_disposition\":\"conditional_reference_only_pending_uniform_member_and_numeric_contract\""
        << ",\"donor_preparations\":0,\"warm_reuse_measured\":false"
        << ",\"exact_rows_materialized_for_treatment\":0"
        << ",\"owned_bytes\":" << owner.audited_estimated_owned_bytes();

    // Reproduce only the cheap phase transition and identity guard. No table
    // is prepared at the new anchor before uniform coverage is established.
    const auto setup = calc.registry().index_by_id.at("eldritch_ichor:1");
    const auto draw = calc.registry().index_by_id.at("eldritch_exalt");
    if (!action_legal(calc.session(), calc.registry().actions[setup], calc.state(root)))
        throw std::runtime_error("Ichor component is inapplicable");
    const auto& phase = calc.outcomes(root, setup, false);
    if (!phase.supported || !phase.applicable || phase.entries.size() != 1 ||
        phase.entries[0].probability != 1 || !phase.choice_groups.empty())
        throw std::runtime_error("Ichor phase is not deterministic");
    const auto post = phase.entries[0].state;
    pc_item_state post_item{};
    if (!calc.materialize(post, post_item)) throw std::runtime_error("phase cannot materialize");
    if (!action_legal(calc.session(), calc.registry().actions[draw], calc.state(post)))
        throw std::runtime_error("paid program draw is inapplicable");
    double mandatory_cost = 0;
    for (auto action : {setup, draw})
        for (const auto& cost_key : calc.registry().actions[action].cost_keys)
            mandatory_cost += owner.prices.at(cost_key);
    const auto& old_state = calc.state(root);
    const auto& phase_state = calc.state(post);
    const bool compatible_anchor =
        (phase_state.flags & solve_detail::kProtectionFlags) ==
            (old_state.flags & solve_detail::kProtectionFlags) &&
        phase_state.fractured_goal_mask == old_state.fractured_goal_mask &&
        phase_state.fractured_junk_counts == old_state.fractured_junk_counts &&
        phase_state.prefix_count == old_state.prefix_count &&
        phase_state.suffix_count == old_state.suffix_count;
    std::cout << ",\"phase_audit\":{\"program\":[\"eldritch_ichor:1\",\"eldritch_exalt\"]"
        << ",\"mandatory_cost\":" << mandatory_cost
        << ",\"old_anchor_accepts_post\":" << (owner.identity_clean_goal_progress_eligible(post) ? "true" : "false")
        << ",\"unchanged_explicit_guard_facts\":" << (compatible_anchor ? "true" : "false")
        << ",\"eater_before\":" << static_cast<unsigned>(old_state.eater_of_worlds_tier)
        << ",\"eater_after\":" << static_cast<unsigned>(phase_state.eater_of_worlds_tier)
        << ",\"distinguishes_modifier_identity\":" << (calc.distinguishes_modifier_identity() ? "true" : "false")
        << ",\"post_exact_member\":";
    emit_key(exact_item_state_key(post_item));
    std::cout << '}';
    std::uint64_t unobserved = 0, largest_class = 0;
    for (const auto& junk : calc.layout().junk_classes) {
        if (!junk.exclusion_effect_observation_complete) ++unobserved;
        largest_class = std::max<std::uint64_t>(largest_class, junk.member_count);
    }
    std::cout << ",\"projection_audit\":{\"junk_classes_without_complete_exclusion_observer\":"
        << unobserved << ",\"largest_junk_class_members\":" << largest_class
        << ",\"uniform_donor_certificate\":false,\"strict_pushforward_certificate\":false}";
}
void emit_phase_program(const PhaseProgramLowerWitness& proof) {
    const auto& r = proof.record;
    std::cout << "{\"operator_id\":\"" << r.operator_id << "\",\"source\":";
    emit_key(r.source); std::cout << ",\"post_phase\":"; emit_key(r.post_phase);
    std::cout << ",\"operator_identity\":"; emit_key(r.operator_identity);
    std::cout << ",\"cost_lower\":" << r.cost_lower << ",\"goal_weight\":" << r.goal_weight
        << ",\"total_weight\":" << r.total_weight << ",\"goal_probability_upper\":" << r.goal_probability_upper
        << ",\"modifier_exits\":" << r.physical_exits << ",\"failure_lower_min\":" << r.failure_lower_min
        << ",\"failure_lower_max\":" << r.failure_lower_max << ",\"lower\":" << r.lower
        << ",\"semantic_acceptance\":true,\"numeric_acceptance\":true";
    if (!r.exits.empty()) {
        std::cout << ",\"support_control_lower\":" << r.support_control_lower << ",\"weighted_exits\":[";
        for (unsigned i = 0; i < r.exits.size(); ++i) {
            if (i) std::cout << ',';
            const auto& e = r.exits[i];
            std::cout << "{\"weight\":" << e.weight << ",\"mask\":" << e.mask << ",\"cell\":" << e.cell
                << ",\"lower\":" << e.lower << ",\"goal\":" << (e.goal ? "true" : "false") << '}';
        }
        std::cout << ']';
    }
    std::cout << '}';
}

void uniform_phase(CalcContext& calc, SolveWorkTestAccess::Impl& owner,
        const pc_item_state& start, const StableKey& request) {
    const auto root = owner.result.start_state;
    const double baseline = owner.completion_proof_lower_value(root);
    const auto source_owned = owner.audited_estimated_owned_bytes();
    const auto shared_calc_owned = calc.estimated_owned_bytes();
    const std::string program_id = "option:eldritch_side_intent:suffix:eldritch_exalt:eldritch_ichor:1";
    pc_item_state phase = start; phase.eater_of_worlds_tier = 1;
    const auto post = calc.intern_item(phase);
    const double old_post_lower = owner.completion_proof_lower_value(post);
    const bool old_eligible = owner.identity_clean_goal_progress_eligible(post);

    // Project strongest compatible floors before donor preparation. These are
    // the same immutable prepared-stage arrays the existing owners consume.
    struct Floor { StableKey identity; std::string id; double value, analytic, operator_lower; bool illegal; unsigned family; };
    std::vector<Floor> floors;
    for (const auto& priced : owner.operators) {
        if (!calc.is_candidate_operator_admitted_for_state(root, priced.index)) continue;
        const auto& op = calc.operators().at(priced.index);
        const bool illegal = op.kind == PlannerOperatorKind::Primitive &&
            !action_legal(calc.session(), calc.registry().actions.at(op.primitive_action), calc.state(root));
        const double operator_lower = owner.operator_proof_lower_value(root, priced.index, false);
        const double analytic = op.kind == PlannerOperatorKind::Primitive ?
            owner.prepared_primitive_lower_floor(op.primitive_action) : 0;
        double lower = std::max(baseline, analytic);
        if (std::isfinite(operator_lower)) lower = std::max(lower, operator_lower);
        floors.push_back({planner_operator_semantic_key(op), op.id, lower, analytic,
            std::isfinite(operator_lower) ? operator_lower : 0, illegal, static_cast<unsigned>(op.automatic_kind)});
    }
    std::shared_ptr<const PreparedPhaseLowerView> donor;
    std::uint64_t cold_prepare_ns, export_ns, cold_owner_bytes;
    double legacy_post_anchor;
    {
        const auto cold = Clock::now();
        SolveWorkTestAccess::Impl prepared(calc, phase, owner.prices, owner.options);
        prepared.prepare_goal_cover_cost();
        cold_prepare_ns = ns(cold);
        legacy_post_anchor = prepared.completion_proof_lower_value(prepared.result.start_state);
        cold_owner_bytes = prepared.audited_estimated_owned_bytes();
        const auto began = Clock::now();
        donor = prepared.prepare_phase_lower({});
        export_ns = ns(began);
    }
    const auto cold_query = Clock::now();
    const auto program = PhaseLowerProducer::compose(calc, owner.prices, start, program_id, *donor);
    const auto cold_query_ns = ns(cold_query);
    const auto warm_start = Clock::now();
    const auto warm = PhaseLowerProducer::compose(calc, owner.prices, start, program_id, *donor);
    const auto warm_ns = ns(warm_start);
    if (program.record.lower != warm.record.lower || program.record.goal_weight != warm.record.goal_weight ||
        program.record.total_weight != warm.record.total_weight)
        throw std::runtime_error("identical native program reuse changed its result");

    // A real goal-mask and occupancy change, with the same session/phase scope.
    // No incumbent reachability assumption is needed for this valid native item.
    pc_item_state second = start;
    if (pc_item_remove_at(&second, PC_SIDE_PREFIX, 0) != PC_RESULT_OK)
        throw std::runtime_error("cannot construct the second semantic source");
    const auto second_query = Clock::now();
    const auto second_program = PhaseLowerProducer::compose(calc, owner.prices, second, program_id, *donor);
    const auto second_query_ns = ns(second_query);
    pc_item_state second_phase = second; second_phase.eater_of_worlds_tier = 1;
    std::uint64_t fresh_prepare_ns, fresh_export_ns, fresh_query_ns;
    bool same_table = false;
    {
        const auto fresh = Clock::now();
        SolveWorkTestAccess::Impl prepared(calc, second_phase, owner.prices, owner.options);
        prepared.prepare_goal_cover_cost();
        fresh_prepare_ns = ns(fresh);
        const auto exported = Clock::now();
        QuotientLowerBudget fresh_budget;
        fresh_budget.max_scratch_bytes = (16ull << 20) - donor->memory_snapshot().total_bytes;
        auto fresh_donor = prepared.prepare_phase_lower(fresh_budget);
        fresh_export_ns = ns(exported);
        same_table = donor->identity == fresh_donor->identity && donor->values == fresh_donor->values;
        const auto queried = Clock::now();
        auto fresh_program = PhaseLowerProducer::compose(calc, owner.prices, second, program_id, *fresh_donor);
        fresh_query_ns = ns(queried);
        if (!same_table || fresh_program.record.lower != second_program.record.lower ||
            fresh_program.record.total_weight != second_program.record.total_weight ||
            fresh_program.record.goal_weight != second_program.record.goal_weight)
            throw std::runtime_error("fresh distinct-source preparation disagrees with reuse");
    }
    std::cout << "\"native_donor\":{\"semantic_acceptance\":true,\"numeric_acceptance\":true,"
        << "\"candidate_component\":\"existing_universal_goal_cover\",\"candidate_accepted\":"
        << (donor->original_candidate_accepted ? "true" : "false")
        << ",\"repair\":\"complete_registry_pointwise_goal_acquisition\",\"phase_domain\":\"fixed_session_and_goal_all_native_members_at_exact_eldritch_tiers\","
        << "\"searing\":" << unsigned(donor->searing) << ",\"eater\":" << unsigned(donor->eater)
        << ",\"identity\":"; emit_key(donor->identity);
    std::cout << ",\"values_by_goal_mask\":[";
    for (std::size_t i = 0; i < donor->values.size(); ++i) { if (i) std::cout << ','; std::cout << donor->values[i]; }
    std::cout << "],\"primitive_cover\":[";
    for (std::size_t i = 0; i < donor->primitives.size(); ++i) {
        if (i) std::cout << ',';
        const auto& action = donor->primitives[i];
        std::cout << "{\"id\":\"" << action.action_id << "\",\"reach\":" << action.reachable_goals
            << ",\"price_lower\":" << action.price_lower << ",\"priced\":" << (action.priced ? "true" : "false") << '}';
    }
    std::cout << "],\"family_mask\":" << donor->family_mask << ",\"retained_reservation_bytes\":" << donor->retained_reservation << '}'
        << ",\"baseline\":{\"independent_root\":" << baseline << ",\"old_post_eligible\":" << (old_eligible ? "true" : "false")
        << ",\"old_post_lower\":" << old_post_lower << ",\"legacy_reanchored_lower\":" << legacy_post_anchor
        << ",\"new_post_lower\":" << *donor->lookup(calc, owner.prices, phase)
        << ",\"archived_conditional_program\":39.773949853475088,\"archived_local_before\":7.1136189946140025}"
        << ",\"program\":";
    emit_phase_program(program); std::cout << ",\"second_program\":"; emit_phase_program(second_program);
    std::cout << ",\"reuse\":{\"cold_prepare_ns\":" << cold_prepare_ns << ",\"cold_export_check_ns\":" << export_ns
        << ",\"cold_program_ns\":" << cold_query_ns << ",\"warm_identical_ns\":" << warm_ns
        << ",\"different_source_ns\":" << second_query_ns << ",\"fresh_second_prepare_ns\":" << fresh_prepare_ns
        << ",\"fresh_second_export_ns\":" << fresh_export_ns << ",\"fresh_second_query_ns\":" << fresh_query_ns
        << ",\"fresh_table_and_output_equal\":true,\"source_owner_including_shared_calc_bytes\":" << source_owned
        << ",\"cold_owner_including_shared_calc_bytes\":" << cold_owner_bytes << ",\"shared_calc_before_bytes\":" << shared_calc_owned
        << ",\"snapshot_and_live_witness_bytes\":" << donor->memory_snapshot().total_bytes
        << ",\"snapshot_ledger_peak_bytes\":" << donor->memory_snapshot().peak_total_bytes << '}'
        << ",\"prepared_action_floors\":[";
    for (std::size_t i = 0; i < floors.size(); ++i) {
        if (i) std::cout << ',';
        const auto& f = floors[i];
        std::cout << "{\"id\":\"" << f.id << "\",\"lower\":" << f.value << ",\"analytic\":" << f.analytic
            << ",\"operator_lower\":" << f.operator_lower << ",\"inapplicable\":" << (f.illegal ? "true" : "false") << '}';
    }
    std::cout << "],\"complete_models\":[";
    for (bool refine : {false, true}) {
        if (refine) std::cout << ',';
        QuotientBellmanGraph graph((16ull << 20) - donor->memory_snapshot().total_bytes,
            QuotientBellmanMode::LowerOnly);
        ScopedProofMemoryCharge probe_workspace(graph.proof_store()->ledger(),
            ProofMemoryCategory::Scratch, 1ull << 20);
        const auto root_key = exact_item_state_key(start);
        graph.install_cells({{0, 1, root_key, false}});
        const StableKey scope{0x50484153454d4f44, 1};
        QuotientLowerSource source{0, root_key, {scope, 1, true, {}, {}}, {}};
        std::map<unsigned, std::vector<StableKey>> members;
        std::map<StableKey, std::string> labels;
        for (const auto& floor : floors) {
            if (floor.family) members[floor.family].push_back(floor.identity);
            else source.expected_actions.actions.push_back(floor.identity);
            labels[floor.identity] = floor.id;
            source.constraints.push_back({{floor.identity, false, {}},
                floor.illegal ? LowerConstraintKind::Inapplicable : LowerConstraintKind::Scalar,
                UINT64_MAX, floor.value, key("existing/prepared/"+floor.id),
                floor.illegal ? LowerEvidenceKind::ExactInapplicability : LowerEvidenceKind::IndependentLower});
        }
        if (refine) {
            const auto& r = program.record;
            members[unsigned(AutomaticCandidateKind::EldritchSide)].push_back(r.operator_identity);
            labels[r.operator_identity] = r.operator_id;
            StableKey evidence = donor->identity;
            evidence.insert(evidence.end(), r.source.begin(), r.source.end());
            evidence.insert(evidence.end(), r.operator_identity.begin(), r.operator_identity.end());
            source.constraints.push_back({{r.operator_identity, false, {}}, LowerConstraintKind::Scalar,
                UINT64_MAX, std::max(baseline, r.lower), std::move(evidence), LowerEvidenceKind::IndependentLower});
        }
        unsigned families = 0;
        for (unsigned kind = 1; kind <= unsigned(AutomaticCandidateKind::Veiled); ++kind) {
            const bool open = calc.goal().automatic_candidates && (calc.goal().automatic_candidate_kind_mask & (1u << kind));
            if (!open && members[kind].empty()) continue;
            const StableKey identity{0x46414d494c59, kind};
            source.expected_actions.families.push_back({identity, members[kind], open});
            if (!open) continue;
            ++families; labels[identity] = "residual_family_" + std::to_string(kind);
            source.constraints.push_back({{identity, true, members[kind]}, LowerConstraintKind::Scalar,
                UINT64_MAX, baseline, key("existing/complete_root_lower"), LowerEvidenceKind::IndependentLower});
        }
        QuotientLowerQuery query{request, scope, graph.model_revision(), LowerCoefficientModel::ExactBinaryModel,
            {0}, {std::move(source)}, {}};
        const auto query_start = Clock::now();
        const auto result = graph.solve_lower(query);
        if (!result.checked) throw std::runtime_error("complete phase lower model refused: " + result.reason);
        const double lower = result.checked->values_by_state[0];
        std::cout << "{\"refined_program\":" << (refine ? "true" : "false") << ",\"open_families\":" << families
            << ",\"lower\":" << lower << ",\"portfolio\":" << std::max(baseline, lower)
            << ",\"query_ns\":" << ns(query_start) << ",\"ranked_constraints\":[";
        bool first = true;
        for (const auto& ranked : result.ranked_constraints) {
            if (!first) std::cout << ','; first = false;
            std::cout << "{\"id\":\"" << labels.at(ranked.cover.identity) << "\",\"lower\":" << ranked.rhs_lower << '}';
        }
        std::cout << "]}";
    }
    std::cout << ']';
}
void emit_refusal(const PhaseProposalRefusal& r) {
    std::cout << "{\"kind\":\"" << r.kind << "\",\"detail\":\"" << r.detail
        << "\",\"cell\":" << r.cell << ",\"action\":" << r.action << ",\"successor\":" << r.successor
        << ",\"lhs\":" << r.lhs << ",\"cost\":" << r.cost << ",\"continuation\":" << r.continuation << ",\"exits\":[";
    for (unsigned i = 0; i < r.targets.size(); ++i) {
        if (i) std::cout << ',';
        std::cout << '[' << r.targets[i] << ',' << r.probabilities[i] << ']';
    }
    std::cout << "]}";
}
void probabilistic_phase(CalcContext& calc, SolveWorkTestAccess::Impl& owner, const pc_item_state& start) {
    const auto started = Clock::now();
    pc_item_state post = start; post.eater_of_worlds_tier = 1;
    const auto mask_proposal = owner.phase_lower_proposal(false);
    const auto clean_proposal = owner.phase_lower_proposal(true);
    auto support = PhaseLowerProducer::prepare(calc, owner.prices, post, mask_proposal);
    const auto restart_boundary = owner.phase_restart_boundary(*support);
    const auto adapter_ns = ns(started);
    const auto control_start = Clock::now();
    auto control = PhaseLowerProducer::prepare_probabilistic(calc, owner.prices, post,
        clean_proposal, support, restart_boundary, owner.options.consider_imprint_programs, false);
    const auto control_ns = ns(control_start);
    const auto control_lower = *control->lookup(calc, owner.prices, start, owner.options.consider_imprint_programs);
    const auto root_index = ((start.rarity*support->values.size()+owner.satisfied_goal_mask_for_state(owner.result.start_state))*4+start.prefix_count)*4+start.suffix_count;
    bool scour_limits = false;
    for (const auto& row : control->relations)
        if (row.cell == root_index && calc.registry().actions[row.action].params.type == ActionType::Scour &&
            row.rhs <= control_lower+1e-10) scour_limits = true;
    if (!scour_limits) throw std::runtime_error("measured minimum did not select the proposed Scour retention refinement");
    std::cout << "\"proposal_adapter\":{\"role\":\"mask_completion_from_acquisition_any_k_union\",\"dimensions\":"
        << mask_proposal.values.size() << ",\"accepted_by_support_control\":" << (support->original_candidate_accepted ? "true" : "false")
        << ",\"first_refusal\":"; emit_refusal(support->proposal_refusal);
    std::cout << ",\"values\":[";
    for (unsigned i = 0; i < mask_proposal.values.size(); ++i) { if (i) std::cout << ','; std::cout << mask_proposal.values[i]; }
    std::cout << "]},\"retention_control\":{\"lower\":" << control_lower << ",\"relation\":\"scour_grants_fracture_loss_and_zero_outside_boundary\","
        << "\"selected_from_root_minimum\":true,\"predicted_remaining_restart_ceiling\":" << (5+restart_boundary.record.lower) << ",\"prepare_check_ns\":" << control_ns << '}';
    // Release the unrefined graph evidence before the matched treatment.
    control.reset();
    const auto treatment_start = Clock::now();
    auto potential = PhaseLowerProducer::prepare_probabilistic(calc, owner.prices, post,
        clean_proposal, support, restart_boundary, owner.options.consider_imprint_programs, true);
    const auto treatment_ns = ns(treatment_start);
    std::cout << ",\"probabilistic_donor\":{\"role\":\"clean_completion_rarity_mask_prefix_suffix\",\"dimensions\":"
        << potential->values.size() << ",\"restart_boundary_lower\":" << restart_boundary.record.lower << ",\"domain\":\"fixed_fractured_modifier_no_metamods_or_generic_influence_all_eldritch_phases\","
        << "\"fractured_mod\":" << potential->fractured_mod << ",\"fractured_mask\":" << potential->fractured_mask
        << ",\"semantic_acceptance\":true,\"coefficient_acceptance\":true,\"rounds\":" << potential->model_rounds
        << ",\"first_proposal_refusal\":"; emit_refusal(potential->proposal_refusal);
    std::cout << ",\"values\":[";
    for (unsigned i = 0; i < potential->values.size(); ++i) { if (i) std::cout << ','; std::cout << potential->values[i]; }
    std::cout << "],\"proposal_values\":[";
    for (unsigned i = 0; i < potential->proposal.values.size(); ++i) { if (i) std::cout << ','; std::cout << potential->proposal.values[i]; }
    std::cout << "],\"native_draw_witnesses\":[";
    for (unsigned i = 0; i < potential->draws.size(); ++i) {
        if (i) std::cout << ',';
        const auto& w = potential->draws[i];
        std::cout << "{\"action\":\"" << calc.registry().actions[w.action].id << "\",\"slot\":" << w.slot
            << ",\"side\":" << int(w.side) << ",\"guaranteed\":" << (w.guaranteed ? "true" : "false")
            << ",\"target\":" << w.target_weight << ",\"other\":" << w.other_weight << ",\"entries\":" << w.entries << ",\"removal\":[";
        for (unsigned side = 0; side < 2; ++side) {
            if (side) std::cout << ','; std::cout << '[';
            for (unsigned j = 0; j < 3; ++j) { if (j) std::cout << ','; std::cout << w.strongest_other_removal[side][j]; }
            std::cout << ']';
        }
        std::cout << "]}";
    }
    std::cout << "],\"checked_relations\":[";
    for (unsigned i = 0; i < potential->relations.size(); ++i) {
        if (i) std::cout << ',';
        const auto& r = potential->relations[i];
        std::cout << "{\"cell\":" << r.cell << ",\"action\":\"" << calc.registry().actions[r.action].id
            << "\",\"cost\":" << r.cost << ",\"rhs\":" << r.rhs
            << ",\"probability_aware\":" << (r.probability_aware ? "true" : "false")
            << ",\"independent_price\":" << (r.independent_price ? "true" : "false") << ",\"exits\":[";
        for (unsigned j = 0; j < r.targets.size(); ++j) {
            if (j) std::cout << ',';
            std::cout << '[' << r.targets[j] << ',' << r.probabilities[j] << ']';
        }
        std::cout << "]}";
    }
    std::cout << "]},\"sources\":[";
    pc_item_state second = start;
    if (pc_item_remove_at(&second, PC_SIDE_PREFIX, 0) != PC_RESULT_OK) throw std::runtime_error("saved second source failed");
    const std::string program_id = "option:eldritch_side_intent:suffix:eldritch_exalt:eldritch_ichor:1";
    for (unsigned which = 0; which < 2; ++which) {
        if (which) std::cout << ',';
        const auto& item = which ? second : start;
        const auto prep_start = Clock::now();
        std::unique_ptr<SolveWorkTestAccess::Impl> fresh;
        if (which) { fresh = std::make_unique<SolveWorkTestAccess::Impl>(calc, item, owner.prices, owner.options); fresh->prepare_goal_cover_cost(); }
        auto& local = which ? *fresh : owner;
        const auto second_prepare_ns = which ? ns(prep_start) : 0;
        const auto root = local.result.start_state;
        const auto baseline = local.completion_proof_lower_value(root);
        const auto donor = *potential->lookup(calc, owner.prices, item, local.options.consider_imprint_programs);
        const auto query_start = Clock::now();
        const auto new_program = PhaseLowerProducer::compose(calc, owner.prices, item, program_id, *potential);
        const auto query_ns = ns(query_start);
        std::cout << "{\"second_source\":" << (which ? "true" : "false") << ",\"source\":"; emit_key(exact_item_state_key(item));
        std::cout << ",\"independent_lower\":" << baseline << ",\"support_donor\":" << support->values[local.satisfied_goal_mask_for_state(root)]
            << ",\"probabilistic_donor\":" << donor << ",\"program_before\":{\"lower\":" << new_program.record.support_control_lower << '}';
        std::cout << ",\"program_after\":"; emit_phase_program(new_program);
        std::cout << ",\"local_compatible_gain\":" << std::max(baseline, new_program.record.lower)-std::max(baseline, new_program.record.support_control_lower)
            << ",\"program_query_ns\":" << query_ns << ",\"second_preparation_ns\":" << second_prepare_ns << ",\"complete_models\":[";
        for (unsigned treatment = 0; treatment < 2; ++treatment) {
            if (treatment) std::cout << ',';
            const auto lower = std::max(baseline, treatment ? donor : 0);
            QuotientBellmanGraph graph((16ull << 20)-potential->memory_snapshot().total_bytes, QuotientBellmanMode::LowerOnly);
            const auto source_key = exact_item_state_key(item);
            graph.install_cells({{0, 1, source_key, false}});
            const StableKey scope{0x50524f42454e56, which};
            QuotientLowerSource source{0, source_key, {scope, 1, true, {}, {}}, {}};
            std::map<unsigned, std::vector<StableKey>> members;
            std::map<StableKey, std::string> labels;
            std::set<StableKey> inserted;
            unsigned admitted = 0, illegal_count = 0;
            for (const auto& priced : local.operators) {
                if (!calc.is_candidate_operator_admitted_for_state(root, priced.index)) continue;
                const auto& op = calc.operators()[priced.index];
                auto identity = planner_operator_semantic_key(op); inserted.insert(identity); labels[identity] = op.id;
                if (op.automatic_kind == AutomaticCandidateKind::None) source.expected_actions.actions.push_back(identity);
                else members[unsigned(op.automatic_kind)].push_back(identity);
                const bool illegal = op.kind == PlannerOperatorKind::Primitive &&
                    !action_legal(calc.session(), calc.registry().actions[op.primitive_action], calc.state(root));
                ++admitted; illegal_count += illegal;
                double floor = lower;
                const auto existing = local.operator_proof_lower_value(root, priced.index, false);
                if (std::isfinite(existing)) floor = std::max(floor, existing);
                if (op.kind == PlannerOperatorKind::Primitive) floor = std::max(floor, local.prepared_primitive_lower_floor(op.primitive_action));
                source.constraints.push_back({{identity, false, {}}, illegal ? LowerConstraintKind::Inapplicable : LowerConstraintKind::Scalar,
                    UINT64_MAX, floor, {1}, illegal ? LowerEvidenceKind::ExactInapplicability : LowerEvidenceKind::IndependentLower});
            }
            // Complete description synthesis for this small family, without
            // materializing any kernels. Other open families retain their floor.
            const auto side_programs = calc.phase_lower_eldritch_programs(item, owner.prices);
            for (const auto& op : side_programs) {
                auto identity = planner_operator_semantic_key(op);
                if (!inserted.insert(identity).second) continue;
                members[unsigned(AutomaticCandidateKind::EldritchSide)].push_back(identity); labels[identity] = op.id;
                double floor = lower;
                if (op.id == program_id) floor = std::max(floor, treatment ? new_program.record.lower : new_program.record.support_control_lower);
                source.constraints.push_back({{identity, false, {}}, LowerConstraintKind::Scalar, UINT64_MAX, floor, {2}, LowerEvidenceKind::IndependentLower});
            }
            unsigned open = 0;
            for (unsigned kind = 1; kind <= 10; ++kind) {
                const bool enabled = calc.goal().automatic_candidates && (calc.goal().automatic_candidate_kind_mask & (1u << kind));
                const bool is_open = enabled && kind != unsigned(AutomaticCandidateKind::Imprint) && kind != unsigned(AutomaticCandidateKind::EldritchSide);
                const StableKey identity{0x46414d494c59, kind};
                source.expected_actions.families.push_back({identity, members[kind], is_open});
                if (!is_open) continue;
                ++open; labels[identity] = "residual_family_"+std::to_string(kind);
                source.constraints.push_back({{identity, true, members[kind]}, LowerConstraintKind::Scalar, UINT64_MAX, lower, {3}, LowerEvidenceKind::IndependentLower});
            }
            QuotientLowerQuery query{{0x50524f42454e56, which, treatment}, scope, graph.model_revision(), LowerCoefficientModel::ExactBinaryModel, {0}, {std::move(source)}, {}};
            auto result = graph.solve_lower(query);
            if (!result.checked) throw std::runtime_error("complete probability envelope: "+result.reason);
            const auto model = result.checked->values_by_state[0];
            std::cout << "{\"treatment\":" << treatment << ",\"admitted\":" << admitted << ",\"inapplicable\":" << illegal_count
                << ",\"open_families\":" << open << ",\"eldritch_descriptions\":" << side_programs.size()
                << ",\"imprint_scope_excluded\":true,\"lower\":" << model << ",\"portfolio\":" << std::max(baseline, model) << ",\"ranked_constraints\":[";
            bool first = true;
            for (const auto& r : result.ranked_constraints) {
                if (!first) std::cout << ','; first = false;
                std::cout << "{\"id\":\"" << labels.at(r.cover.identity) << "\",\"lower\":" << r.rhs_lower << '}';
            }
            std::cout << "]}";
        }
        std::cout << "]}";
    }
    std::cout << "],\"resources\":{\"adapter_support_ns\":" << adapter_ns << ",\"probability_treatment_ns\":" << treatment_ns
        << ",\"retained_potential_bytes\":" << potential->retained_reservation << ",\"shared_support_bytes\":" << support->retained_reservation
        << ",\"proof_ledger_peak_bytes\":" << potential->memory_snapshot().peak_total_bytes
        << ",\"combined_additional_peak_bytes\":" << potential->peak_additional_bytes
        << ",\"native_projected_action_checks\":" << potential->native_action_relations
        << ",\"shared_calculator_bytes\":" << calc.estimated_owned_bytes() << ",\"source_owner_including_shared_calculator_bytes\":"
        << owner.audited_estimated_owned_bytes() << "}";
}
} // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 5) throw std::runtime_error("micro|medium-coverage artifact-directory goal economy required");
        const bool is_micro = std::string(argv[1]) == "micro";
        const bool is_phase = std::string(argv[1]) == "uniform-phase";
        const bool is_probability = std::string(argv[1]) == "probabilistic-phase";
        if (!is_micro && !is_phase && !is_probability && std::string(argv[1]) != "medium-coverage") throw std::runtime_error("unknown bounded probe");
        const auto began = Clock::now();
        Handles h;
        pc_error_info error{};
        pc_error_info_init(&error);
        const std::string manifest_path = std::string(argv[2]) + "/manifest.json";
        check(pc_data_load_file(manifest_path.c_str(), &h.data, &error), error);
        pc_session_options so{};
        so.struct_size = sizeof(so); so.abi_version = PC_ABI_VERSION;
        so.base_metadata_path = is_micro ? "Metadata/Items/Armours/BodyArmours/BodyInt17" :
            "Metadata/Items/Armours/BodyArmours/BodyStrDex20";
        so.item_level = 86;
        check(pc_session_create(h.data, &so, &h.session, &error), error);
        const auto goal = read(argv[3]), economy = read(argv[4]);
        check(pc_solver_create(h.session, goal.data(), goal.size(), &h.solver, &error), error);
        check(pc_economy_load_json(economy.data(), economy.size(), &h.economy, &error), error);
        auto& calc = solver_lower_diagnostic_calculator(h.solver);
        pc_item_state start{};
        pc_item_init_options io{};
        io.struct_size = sizeof(io); io.abi_version = PC_ABI_VERSION;
        io.rarity = is_micro ? PC_RARITY_NORMAL : PC_RARITY_RARE;
        check(pc_item_init(h.session, &io, &start, &error), error);
        if (!is_micro) {
            const std::vector<std::string> mods{"LocalIncreasedArmourAndEvasionAndStunRecovery6",
                "LocalBaseArmourAndEvasionRating8", "LocalIncreasedArmourAndEvasion8", "ChanceToSuppressSpellsHigh5___"};
            for (std::size_t i = 0; i < mods.size(); ++i) {
                const auto pos = h.session->impl->data->mod_pos_by_key.at(mods[i]);
                const auto mid = h.session->impl->session_id_by_global_id.at(h.session->impl->data->mod_global_ids[pos]);
                pc_mod_info info{};
                info.struct_size = sizeof(info); info.abi_version = PC_ABI_VERSION;
                check(pc_session_get_mod_info(h.session, mid, &info, &error), error);
                check(pc_item_add_mod(&start, info.generation_type, mid,
                    static_cast<std::uint16_t>(info.primary_group_id), i == 3 ? PC_MOD_SLOT_FRACTURED : 0, nullptr), error);
            }
        }
        SolveOptions options;
        if (!is_micro) apply_solve_profile_defaults(options, SolveProfile::CalculatorProductV1);
        else {
            options.high_impact_executable_uppers = true;
            options.allow_economic_restart = true;
            options.consider_imprint_programs = false;
            options.goal_progress_gated_reforges = false;
        }
        options.max_states = options.max_discovered_states = options.max_expanded_states = is_micro ? 8 : 1000;
        options.max_state_action_rows = is_micro ? 24 : 1000;
        options.max_transitions = is_micro ? 56 : 1000;
        options.max_reforge_work = is_micro ? 20000 : 1000;
        options.max_solver_owned_bytes = 1ull << 30;
        const auto prepare = Clock::now();
        SolveWorkTestAccess::Impl owner(calc, start, h.economy->impl->prices, options);
        owner.prepare_goal_cover_cost();
        const auto prepare_ns = ns(prepare);
        if (std::isfinite(owner.envelope_bellman_lower))
            throw std::runtime_error("probe must bypass envelope helper");
        std::cout << std::setprecision(17) << "{\"pilot\":\"" << (is_probability ? "native-probabilistic-lower-v1" : (is_phase ? "uniform-phase-lower-v1" : "operator-complete-frontier-v2"))
            << "\",\"solver_steps\":0,\"production_authority\":false,";
        if (is_micro) micro(calc, owner,
            key(goal + '\n' + economy + '\n' + read(manifest_path) + "\nlower-v2"), key(goal));
        else if (is_phase) uniform_phase(calc, owner, start, key(goal + '\n' + economy + '\n' + read(manifest_path)));
        else if (is_probability) probabilistic_phase(calc, owner, start);
        else medium_coverage(calc, owner);
        std::cout << ",\"prepare_ns\":" << prepare_ns << ",\"elapsed_ns\":" << ns(began)
            << ",\"process_peak_working_set_bytes\":" << process_peak() << "}\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 2;
    }
}
