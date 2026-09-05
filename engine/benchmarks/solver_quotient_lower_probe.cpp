/* Opt-in bounded lower query. No SolveWork::advance, compiled strategy, or
 * Simulator; all native dependencies come from the normal engine target. */
#include "poecraft/api.h"
#include "poecraft/session.h"
#include "poecraft/simulator.h"
#include "handles_internal.hpp"
#include "solver_diagnostic_options.hpp"
#include "solver_quotient_bellman.hpp"
#include "solver_solve_types.hpp"

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
} // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 5) throw std::runtime_error("micro|medium-coverage artifact-directory goal economy required");
        const bool is_micro = std::string(argv[1]) == "micro";
        if (!is_micro && std::string(argv[1]) != "medium-coverage") throw std::runtime_error("unknown bounded probe");
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
        std::cout << std::setprecision(17) << "{\"pilot\":\"operator-complete-frontier-v2\",\"solver_steps\":0,\"production_authority\":false,";
        if (is_micro) micro(calc, owner,
            key(goal + '\n' + economy + '\n' + read(manifest_path) + "\nlower-v2"), key(goal));
        else medium_coverage(calc, owner);
        std::cout << ",\"prepare_ns\":" << prepare_ns << ",\"elapsed_ns\":" << ns(began)
            << ",\"process_peak_working_set_bytes\":" << process_peak() << "}\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 2;
    }
}
