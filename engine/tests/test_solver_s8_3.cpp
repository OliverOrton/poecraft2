#include "tests.hpp"

#include "../src/json.hpp"
#include "../src/solver_internal.hpp"
#include "poecraft/bitset.h"
#include "poecraft/item_state.h"

#include <cmath>
#include <algorithm>
#include <limits>
#include <memory>
#include <numeric>
#include <string>
#include <unordered_map>
#include <vector>

using namespace poecraft;
using namespace poecraft::solver;

namespace {

constexpr std::uint32_t kGoalPrefix = 0;
constexpr std::uint32_t kGoalSuffix = 1;
constexpr std::uint32_t kSuffixCompetitor = 2;
constexpr std::uint32_t kPrefixJunkA = 3;
constexpr std::uint32_t kPrefixJunkB = 4;
constexpr std::uint32_t kTargetBlocker = 5;
constexpr std::uint32_t kBenchGoalPrefix = 6;
constexpr std::uint32_t kBenchGoalSuffix = 7;
constexpr std::uint32_t kBenchTemporary = 8;
constexpr std::uint32_t kPrefixLock = 9;
constexpr std::uint32_t kSuffixLock = 10;
constexpr std::uint32_t kMultimod = 11;
constexpr std::uint32_t kBenchNeutral = 12;
constexpr std::uint32_t kSuffixJunk = 13;
constexpr std::uint32_t kBenchPrefixNeutral = 14;
constexpr std::uint32_t kBenchTemporaryAlt = 15;
constexpr std::uint32_t kCannotRollAttack = 16;
constexpr std::uint32_t kCannotRollCaster = 17;
constexpr std::uint32_t kModCount = 18;

std::shared_ptr<SessionImpl> make_automatic_session(
    const bool renewal_retry_pool = false) {
    auto data = std::make_shared<DataImpl>();
    data->strings = {"", "synthetic/s8.3"};
    for (std::uint32_t mod = 0; mod < kModCount; ++mod) {
        data->strings.push_back("s83_mod_" + std::to_string(mod));
    }
    data->mod_global_ids.resize(kModCount);
    std::iota(data->mod_global_ids.begin(), data->mod_global_ids.end(), 0u);
    data->mod_key_sid.resize(kModCount);
    for (std::uint32_t mod = 0; mod < kModCount; ++mod) {
        data->mod_key_sid[mod] = 2 + mod;
        data->mod_pos_by_key.emplace(data->strings[2 + mod], mod);
    }
    data->spawn_offsets.resize(kModCount + 1);
    std::iota(data->spawn_offsets.begin(), data->spawn_offsets.end(), 0u);
    data->spawn_tag_ids.assign(kModCount, 0);
    data->spawn_weights.assign(kModCount, 0);
    data->spawn_weights[kGoalSuffix] = 100;
    data->spawn_weights[kSuffixCompetitor] = 100;
    if (renewal_retry_pool) {
        data->spawn_weights[kSuffixJunk] = 100;
        data->spawn_weights[kBenchNeutral] = 100;
    }
    data->mod_gen_type_code.assign(kModCount, 0);
    data->base_count = 1;
    data->base_metadata_path_sid = {1};
    data->metamod_multimod_code = 1;
    data->metamod_no_attack_code = 2;
    data->metamod_no_caster_code = 3;
    data->metamod_prefixes_locked_code = 4;
    data->metamod_suffixes_locked_code = 5;
    data->tag_id_by_name.emplace("attack", 0);
    data->tag_id_by_name.emplace("caster", 1);

    const std::vector<std::uint32_t> groups = {
        10, 20, 21, 30, 31, 20, 10, 20, 21, 40, 41, 42, 50, 22,
        51, 21, 52, 53};
    data->group_key_sids.assign(64, 0);
    for (const std::uint32_t group : groups) {
        if (data->group_key_sids[group] != 0) continue;
        data->strings.push_back("s83_group_" + std::to_string(group));
        const std::uint32_t sid =
            static_cast<std::uint32_t>(data->strings.size() - 1);
        data->group_key_sids[group] = sid;
        data->group_id_by_key.emplace(data->strings[sid], group);
    }

    auto session = std::make_shared<SessionImpl>();
    session->data = data;
    session->base_index = 0;
    session->item_level = 86;
    session->mod_count = kModCount;
    session->words = pc_bitset_words(kModCount);
    session->global_index.resize(kModCount);
    std::iota(session->global_index.begin(), session->global_index.end(), 0u);
    for (std::uint32_t mod = 0; mod < kModCount; ++mod) {
        session->session_id_by_global_id.emplace(mod, mod);
    }
    session->gen_type = {
        PC_SIDE_PREFIX, PC_SIDE_SUFFIX, PC_SIDE_SUFFIX, PC_SIDE_PREFIX,
        PC_SIDE_PREFIX, PC_SIDE_SUFFIX, PC_SIDE_PREFIX, PC_SIDE_SUFFIX,
        PC_SIDE_SUFFIX, PC_SIDE_SUFFIX, PC_SIDE_PREFIX, PC_SIDE_SUFFIX,
        PC_SIDE_SUFFIX, PC_SIDE_SUFFIX, PC_SIDE_PREFIX, PC_SIDE_SUFFIX,
        PC_SIDE_SUFFIX, PC_SIDE_SUFFIX};
    session->primary_group = groups;
    session->required_level.assign(kModCount, 1);
    session->group_offsets.resize(kModCount + 1);
    for (std::uint32_t mod = 0; mod < kModCount; ++mod) {
        session->group_offsets[mod] =
            static_cast<std::uint32_t>(session->group_ids.size());
        session->group_ids.push_back(groups[mod]);
        if (mod == kBenchTemporaryAlt) {
            session->group_ids.push_back(50);
        }
    }
    session->group_offsets[kModCount] =
        static_cast<std::uint32_t>(session->group_ids.size());
    session->family_id = {
        100, 101, 102, 103, 104, 105, 100, 101, 108, 109, 110, 111,
        112, 113, 114, 115, 116, 117};
    session->family_tier_index.assign(kModCount, 1);
    session->metamod_type.assign(kModCount, -1);
    session->metamod_type[kPrefixLock] = 4;
    session->metamod_type[kSuffixLock] = 5;
    session->metamod_type[kMultimod] = 1;
    session->metamod_type[kCannotRollAttack] = 2;
    session->metamod_type[kCannotRollCaster] = 3;
    session->special_kind.assign(kModCount, -1);
    session->flags.assign(kModCount, 0);
    for (const std::uint32_t mod : {
             kTargetBlocker, kBenchGoalPrefix, kBenchGoalSuffix,
             kBenchTemporary, kPrefixLock, kSuffixLock, kMultimod,
             kBenchNeutral, kBenchPrefixNeutral, kBenchTemporaryAlt,
             kCannotRollAttack, kCannotRollCaster}) {
        session->flags[mod] = 1 << 1;
        session->bench_mod_ids.push_back(mod);
    }
    session->influence_code.assign(kModCount, -1);
    session->class_offsets.resize(kModCount + 1);
    for (std::uint32_t mod = 0; mod < kModCount; ++mod) {
        session->class_offsets[mod] =
            static_cast<std::uint32_t>(session->class_tag_ids.size());
        if (mod == kSuffixCompetitor) {
            session->class_tag_ids.push_back(0);
            session->class_tag_ids.push_back(1);
        }
    }
    session->class_offsets[kModCount] =
        static_cast<std::uint32_t>(session->class_tag_ids.size());
    session->rare_affix_cap = 3;
    session->base_spawn_weight.assign(kModCount, 0);
    session->base_spawn_weight[kGoalSuffix] = 100;
    session->base_spawn_weight[kSuffixCompetitor] = 100;
    if (renewal_retry_pool) {
        session->base_spawn_weight[kSuffixJunk] = 100;
        session->base_spawn_weight[kBenchNeutral] = 100;
    }
    session->base_gen_pct.assign(kModCount, 100);
    session->base_roll_weight = session->base_spawn_weight;
    session->effective_base_tag_ids = {0};

    session->normal_random_roll_mask.assign(session->words, 0);
    session->positive_spawn_weight_mask.assign(session->words, 0);
    session->positive_base_weight_mask.assign(session->words, 0);
    session->prefix_mask.assign(session->words, 0);
    session->suffix_mask.assign(session->words, 0);
    session->unveiled_mask.assign(session->words, 0);
    session->unveiled_generic_mask.assign(session->words, 0);
    session->implicit_tag_masks.assign(
        2, std::vector<std::uint64_t>(session->words, 0));
    pc_bitset_set(
        session->implicit_tag_masks[0].data(), kSuffixCompetitor);
    pc_bitset_set(
        session->implicit_tag_masks[1].data(), kSuffixCompetitor);
    session->group_masks.assign(64, {});
    session->influence_masks.assign(
        1, std::vector<std::uint64_t>(session->words, 0));
    for (std::uint32_t mod = 0; mod < kModCount; ++mod) {
        pc_bitset_set(
            (session->gen_type[mod] == PC_SIDE_PREFIX
                 ? session->prefix_mask
                 : session->suffix_mask)
                .data(),
            mod);
        for (std::uint32_t row = session->group_offsets[mod];
             row < session->group_offsets[mod + 1]; ++row) {
            auto& group_mask =
                session->group_masks[session->group_ids[row]];
            if (group_mask.empty()) group_mask.assign(session->words, 0);
            pc_bitset_set(group_mask.data(), mod);
        }
    }
    for (const std::uint32_t mod : {
             kGoalPrefix, kGoalSuffix, kSuffixCompetitor, kPrefixJunkA,
             kPrefixJunkB, kTargetBlocker, kSuffixJunk}) {
        pc_bitset_set(session->normal_random_roll_mask.data(), mod);
        pc_bitset_set(session->positive_spawn_weight_mask.data(), mod);
        if (session->base_roll_weight[mod] != 0) {
            pc_bitset_set(session->positive_base_weight_mask.data(), mod);
        }
        pc_bitset_set(session->influence_masks[0].data(), mod);
    }
    if (renewal_retry_pool) {
        pc_bitset_set(
            session->normal_random_roll_mask.data(), kBenchNeutral);
        pc_bitset_set(
            session->positive_spawn_weight_mask.data(), kBenchNeutral);
        pc_bitset_set(
            session->positive_base_weight_mask.data(), kBenchNeutral);
        pc_bitset_set(session->influence_masks[0].data(), kBenchNeutral);
    }
    return session;
}

GoalSpec automatic_goal(bool prefix, bool suffix) {
    GoalSpec goal;
    goal.rarity = PC_RARITY_RARE;
    goal.automatic_candidates = true;
    if (prefix) {
        GoalSlot slot;
        slot.family_id = 100;
        goal.slots.push_back(slot);
    }
    if (suffix) {
        GoalSlot slot;
        slot.family_id = 101;
        goal.slots.push_back(slot);
    }
    return goal;
}

void add_mod(
    pc_item_state& item,
    const SessionImpl& session,
    const std::uint32_t mod,
    const std::uint8_t flags = 0) {
    PC_CHECK(pc_item_add_mod(
                 &item, session.gen_type[mod], mod,
                 static_cast<std::uint16_t>(session.primary_group[mod]),
                 flags, nullptr) == PC_RESULT_OK);
}

std::uint32_t operator_by_fragment(
    const CalcContext& calc,
    const std::string& fragment) {
    for (std::uint32_t index = 0; index < calc.operators().size(); ++index) {
        if (calc.operators()[index].id.find(fragment) != std::string::npos) {
            return index;
        }
    }
    return kNoId;
}

std::uint32_t operator_by_kind(
    const CalcContext& calc,
    const AutomaticCandidateKind kind) {
    for (std::uint32_t index = 0; index < calc.operators().size(); ++index) {
        if (calc.operators()[index].automatic_kind == kind) return index;
    }
    return kNoId;
}

StateLocalAutomaticBatch admit_automatic(
    CalcContext& calc,
    const std::uint32_t state,
    const std::unordered_map<std::string, double>& prices) {
    AutomaticAdmissionLimits limits;
    limits.max_state_action_rows = 100000;
    limits.max_transitions = 1000000;
    limits.max_solver_owned_bytes = 1073741824;
    limits.prices = &prices;
    return calc.admit_state_local_automatic_candidates(state, limits);
}

void check_owned_byte_ledger(CalcContext& calc) {
    const std::uint64_t exact = calc.audited_estimated_owned_bytes();
    const std::uint64_t incremental = calc.fast_estimated_owned_bytes();
    PC_CHECK(incremental >= exact);
}

SimulationSummaryInternal run_compiled(
    std::shared_ptr<const SessionImpl> session,
    const std::string& strategy_json,
    const std::unordered_map<std::string, double>& prices,
    const std::uint64_t runs,
    const std::uint64_t seed) {
    auto strategy = compile_strategy_json(
        session, strategy_json.data(), strategy_json.size());
    auto economy = std::make_shared<EconomyImpl>();
    economy->id = "s8.3-test";
    economy->prices = prices;
    SimulatorImpl simulator;
    simulator.session = session;
    simulator.strategy = strategy;
    simulator.economy = economy;
    prepare_simulator_runtime(simulator);
    SimulationOptionsInternal options;
    options.target_runs = runs;
    options.seed = seed;
    options.max_actions_per_run = 1000;
    run_simulator_chunk(
        simulator, options, static_cast<std::uint32_t>(runs));
    PC_CHECK(simulator.summary.completed_runs == runs);
    PC_CHECK(simulator.summary.success_count == runs);
    PC_CHECK(simulator.summary.action_not_applied_count == 0);
    PC_CHECK(simulator.summary.no_matching_edge_count == 0);
    return simulator.summary;
}

std::shared_ptr<SessionImpl> make_automatic_veiled_session() {
    auto session = make_automatic_session(true);
    session->veiled_prefix_mod_id = kBenchGoalPrefix;
    session->veiled_suffix_mod_id = kBenchGoalSuffix;
    session->flags[kBenchGoalPrefix] = 0;
    session->flags[kBenchGoalSuffix] = 0;
    session->flags[kPrefixLock] = 0;
    session->flags[kBenchPrefixNeutral] = 0;
    session->metamod_type[kPrefixLock] = -1;
    std::erase(session->bench_mod_ids, kBenchGoalPrefix);
    std::erase(session->bench_mod_ids, kBenchGoalSuffix);
    std::erase(session->bench_mod_ids, kPrefixLock);
    std::erase(session->bench_mod_ids, kBenchPrefixNeutral);

    const std::array<std::uint32_t, 8> unveiled{
        kGoalPrefix, kPrefixJunkA, kPrefixJunkB,
        kBenchPrefixNeutral, kPrefixLock,
        kGoalSuffix, kSuffixCompetitor, kSuffixJunk};
    DataImpl& data = const_cast<DataImpl&>(*session->data);
    for (const std::uint32_t mod : unveiled) {
        pc_bitset_set(session->unveiled_mask.data(), mod);
        pc_bitset_set(session->unveiled_generic_mask.data(), mod);
        session->base_spawn_weight[mod] = 100;
        session->base_roll_weight[mod] = 100;
        data.spawn_weights[mod] = 100;
        pc_bitset_set(session->positive_spawn_weight_mask.data(), mod);
        pc_bitset_set(session->positive_base_weight_mask.data(), mod);
    }
    std::fill(
        session->normal_random_roll_mask.begin(),
        session->normal_random_roll_mask.end(), 0);
    for (const std::uint32_t mod :
         {kGoalSuffix, kSuffixCompetitor, kSuffixJunk}) {
        pc_bitset_set(session->normal_random_roll_mask.data(), mod);
    }
    return session;
}

void run_automatic_veiled_program() {
    auto session = make_automatic_veiled_session();
    ActionRegistry registry = build_action_registry(*session);
    GoalSpec goal = automatic_goal(true, false);
    const std::uint32_t restart = registry.index_by_id.at("restart");
    const std::uint32_t alchemy = registry.index_by_id.at("alchemy");

    pc_item_state start;
    pc_item_clear(&start);
    start.rarity = PC_RARITY_RARE;
    for (const std::uint32_t mod :
         {kGoalSuffix, kSuffixCompetitor, kSuffixJunk}) {
        add_mod(start, *session, mod);
    }

    const std::unordered_map<std::string, double> prices{
        {"base", 20.0},
        {"alchemy", 1.0},
        {"scour", 1.0},
        {"veiled_chaos", 1000.0},
        {"veiled_exalt", 1.0}};
    CalcContext calc(
        session, goal, registry, {alchemy, restart}, false, true, true);
    const std::uint32_t state = calc.intern_item(start);
    const StateLocalAutomaticBatch batch =
        admit_automatic(calc, state, prices);
    bool saw_chaos = false;
    bool saw_exalt = false;
    for (const StateLocalAutomaticCandidate& decision : batch.decisions) {
        if (!decision.admitted ||
            decision.kind != AutomaticCandidateKind::Veiled) {
            continue;
        }
        const PlannerOperator& planner =
            calc.operators().at(decision.operator_index);
        PC_CHECK(planner.option_kind == FixedOptionKind::Renewal);
        PC_CHECK(planner.primitive_program.size() == 2);
        PC_CHECK(
            registry.actions.at(planner.primitive_program.back()).params.type ==
            ActionType::Unveil);
        const ActionType acquisition =
            registry.actions.at(planner.primitive_program.front()).params.type;
        saw_chaos |= acquisition == ActionType::VeiledChaos;
        saw_exalt |= acquisition == ActionType::VeiledExalt;
        PC_CHECK(decision.telemetry_kind == AutomaticTelemetryKind::Veiled);
        PC_CHECK(
            (decision.evidence.kernel_change_mechanisms &
             kAutomaticAcquisitionTimeOffer) != 0);
        PC_CHECK(decision.evidence.reason ==
                 "exact_acquisition_time_offer_and_best_continuation");
    }
    PC_CHECK(saw_chaos);
    PC_CHECK(saw_exalt);

    CalcContext missing_calc(
        session, goal, registry, {alchemy, restart}, false, true, true);
    const std::uint32_t missing_state = missing_calc.intern_item(start);
    const StateLocalAutomaticBatch missing = admit_automatic(
        missing_calc, missing_state, {{"base", 20.0}});
    std::uint32_t missing_prices = 0;
    for (const StateLocalAutomaticCandidate& decision : missing.decisions) {
        if (decision.kind == AutomaticCandidateKind::Veiled &&
            decision.missing_price) {
            ++missing_prices;
        }
    }
    PC_CHECK(missing_prices == 2);
    PC_CHECK(missing.admitted_operators.empty());

    pc_item_state ineligible = start;
    ineligible.item_flags |= PC_ITEM_CORRUPTED;
    CalcContext ineligible_calc(
        session, goal, registry, {alchemy, restart}, false, true, true);
    const std::uint32_t ineligible_state =
        ineligible_calc.intern_item(ineligible);
    const StateLocalAutomaticBatch refused =
        admit_automatic(ineligible_calc, ineligible_state, prices);
    bool saw_refused = false;
    for (const StateLocalAutomaticCandidate& decision : refused.decisions) {
        if (decision.kind != AutomaticCandidateKind::Veiled) continue;
        saw_refused = true;
        PC_CHECK(!decision.admitted);
        PC_CHECK(!decision.evidence.eligible);
    }
    PC_CHECK(saw_refused);

    pc_item_state crafted = start;
    add_mod(
        crafted, *session, kBenchPrefixNeutral,
        PC_MOD_SLOT_CRAFTED);
    CalcContext cleanup_calc(
        session, goal, registry, {alchemy, restart}, false, true, true);
    const std::uint32_t cleanup_state = cleanup_calc.intern_item(crafted);
    const StateLocalAutomaticBatch cleanup =
        admit_automatic(cleanup_calc, cleanup_state, prices);
    bool saw_pre_cleanup = false;
    for (const StateLocalAutomaticCandidate& decision : cleanup.decisions) {
        if (!decision.admitted ||
            decision.kind != AutomaticCandidateKind::Veiled) {
            continue;
        }
        const PlannerOperator& planner =
            cleanup_calc.operators().at(decision.operator_index);
        if (planner.primitive_program.size() != 3) continue;
        saw_pre_cleanup |=
            registry.actions.at(planner.primitive_program[0]).params.type ==
                ActionType::RemoveCraftedModifiers &&
            (registry.actions.at(planner.primitive_program[1]).params.type ==
                 ActionType::VeiledChaos ||
             registry.actions.at(planner.primitive_program[1]).params.type ==
                 ActionType::VeiledExalt) &&
            registry.actions.at(planner.primitive_program[2]).params.type ==
                ActionType::Unveil;
    }
    PC_CHECK(saw_pre_cleanup);

    CalcContext solve_calc(
        session, goal, registry, {alchemy, restart}, false, true, true);
    SolveOptions options;
    options.max_discovered_states = 100000;
    options.max_expanded_states = 100000;
    options.max_reforge_work = 10000000;
    const SolveResult solved = solve(solve_calc, start, prices, options);
    PC_CHECK(solved.converged);
    PC_CHECK(solved.policy_available);
    PC_CHECK(solved.policy_status == SolvePolicyStatus::Exact);
    const PolicyOperatorRef selected = solved.policy[solved.start_state];
    PC_CHECK(selected.kind == PlannerOperatorKind::FixedOption);
    PC_CHECK(selected.index < solve_calc.operators().size());
    const PlannerOperator& winner = solve_calc.operators().at(selected.index);
    PC_CHECK(winner.automatic_kind == AutomaticCandidateKind::Veiled);
    PC_CHECK(
        registry.actions.at(winner.primitive_program.front()).params.type ==
        ActionType::VeiledExalt);
    PC_CHECK(
        !solved.option_unveil_preferences[solved.start_state].empty());
    const OptionKernel& winner_kernel = solve_calc.option_kernel(
        solved.start_state, selected.index);
    bool saw_no_goal_offer = false;
    for (const OutcomeChoiceGroup& group :
         winner_kernel.observation_choice_groups) {
        const bool group_has_goal = std::any_of(
            group.states.begin(), group.states.end(),
            [&](const std::uint32_t successor) {
                return successor < solve_calc.state_count() &&
                       solve_calc.is_goal_state(
                           solve_calc.state(successor));
            });
        saw_no_goal_offer |= !group_has_goal && !group.states.empty();
    }
    for (const ObservedUnveilPreference& preference :
         solved.option_unveil_preferences[solved.start_state]) {
        if (preference.choices.empty()) continue;
        const auto continuation_value = [&](const ObservedUnveilChoice& choice) {
            const std::uint32_t successor =
                choice.successor_state == kNoId
                    ? solved.start_state
                    : choice.successor_state;
            return solved.values.at(successor);
        };
        std::vector<const ObservedUnveilChoice*> non_goal;
        for (const ObservedUnveilChoice& choice : preference.choices) {
            if (choice.successor_state < solve_calc.state_count() &&
                solve_calc.is_goal_state(
                    solve_calc.state(choice.successor_state))) {
                continue;
            }
            non_goal.push_back(&choice);
        }
        for (std::size_t index = 1; index < non_goal.size(); ++index) {
            PC_CHECK(
                continuation_value(*non_goal[index - 1]) <=
                continuation_value(*non_goal[index]) + 1e-10);
        }
    }
    PC_CHECK(saw_no_goal_offer);

    for (std::uint32_t policy_state = 0;
         policy_state < solved.policy.size(); ++policy_state) {
        const PolicyOperatorRef selected_row = solved.policy[policy_state];
        if (selected_row.index >= solve_calc.operators().size()) continue;
        const PlannerOperator& selected_planner =
            solve_calc.operators().at(selected_row.index);
        if (selected_planner.automatic_kind !=
            AutomaticCandidateKind::Veiled) {
            continue;
        }
        PC_CHECK(!solved.option_unveil_preferences[policy_state].empty());
    }

    const std::string strategy = compile_policy_strategy_json(
        solve_calc, solved, "s8.3-automatic-veiled-acquisition-offers");
    PC_CHECK(strategy.find("has_unveil_option") != std::string::npos);
    PC_CHECK(strategy.find("\"type\":\"veiled_exalt\"") !=
             std::string::npos);
    auto compiled = compile_strategy_json(
        session, strategy.data(), strategy.size());
    auto economy = std::make_shared<EconomyImpl>();
    economy->id = "s8.3-automatic-veiled";
    economy->prices = prices;
    StrategyEvalOptions eval_options;
    eval_options.economy = economy;
    const StrategyEvalResult exact = evaluate_strategy(
        *compiled, eval_options);
    PC_CHECK(exact.converged);
    PC_CHECK(exact.cost_complete);
    PC_CHECK(exact.success_probability > 1.0 - 1e-9);
    PC_CHECK(exact.failure_probability < 1e-12);
    PC_CHECK(exact.action_not_applied_probability < 1e-12);
    PC_CHECK(exact.no_matching_edge_probability < 1e-12);
    PC_CHECK(exact.unresolved_probability < 1e-12);
    PC_CHECK(std::fabs(
                 exact.total_expected_cost -
                 solved.evaluated_policy_cost) < 1e-7);
    const SimulationSummaryInternal summary = run_compiled(
        session, strategy, prices, 10000, 8315);
    PC_CHECK(summary.success_count == summary.completed_runs);
    std::printf(
        "solver automatic Veiled: cost=%.12f exact=%.12f "
        "runs=%llu/%llu\n",
        solved.evaluated_policy_cost, exact.total_expected_cost,
        static_cast<unsigned long long>(summary.success_count),
        static_cast<unsigned long long>(summary.completed_runs));
}

void run_temporary_blocker_price_flip() {
    auto session = make_automatic_session();
    ActionRegistry registry = build_action_registry(*session);
    GoalSpec goal = automatic_goal(false, true);
    const std::uint32_t exalt = registry.index_by_id.at("exalt");
    const std::uint32_t restart = registry.index_by_id.at("restart");
    CalcContext calc(
        session, goal, registry, {exalt, restart},
        false, true, true);
    PC_CHECK(calc.operators().size() == registry.actions.size());
    PC_CHECK(calc.candidate_operators().size() == 2);
    PC_CHECK(calc.action_control().dependency_primitives == 0);
    PC_CHECK(calc.action_control().automatic_dependency_primitives == 0);

    pc_item_state start;
    pc_item_clear(&start);
    start.rarity = PC_RARITY_RARE;
    for (const std::uint32_t mod :
         {kGoalPrefix, kPrefixJunkA, kPrefixJunkB, kSuffixJunk}) {
        add_mod(start, *session, mod);
    }
    const std::uint32_t state = calc.intern_item(start);
    PC_CHECK(calc.is_candidate_operator_admitted_for_state(state, exalt));
    PC_CHECK(calc.is_candidate_operator_admitted_for_state(state, restart));
    PC_CHECK(!calc.is_candidate_operator_admitted_for_state(
        state, static_cast<std::uint32_t>(calc.operators().size())));
    PC_CHECK(!calc.is_candidate_operator_admitted_for_state(
        static_cast<std::uint32_t>(calc.state_count()), exalt));
    check_owned_byte_ledger(calc);
    const StateLocalAutomaticBatch blocker_batch = admit_automatic(calc, state, {
        {"exalt", 10.0},
        {"base", 100.0},
        {"scour", 1.0},
         {"bench:s83_mod_8", 2.0}});
    check_owned_byte_ledger(calc);
    PC_CHECK(blocker_batch.temporary_precompiled_classes > 0);
    PC_CHECK(blocker_batch.temporary_precompile_ns > 0);
    PC_CHECK(blocker_batch.temporary_precompiled_bytes > 0);
    PC_CHECK(blocker_batch.temporary_candidate_variants >= 1);
    PC_CHECK(blocker_batch.temporary_effect_classes > 0);
    PC_CHECK(blocker_batch.temporary_effect_classes <=
             blocker_batch.temporary_candidate_variants);
    PC_CHECK(calc.candidate_operators().size() == 3);
    PC_CHECK(calc.action_control().automatic_dependency_primitives == 2);
    const std::string blocker_id =
        "option:temporary_bench_repeat:bench:s83_mod_8:exalt";
    const std::string neutral_id =
        "option:temporary_bench_repeat:bench:s83_mod_12:exalt";
    const std::uint32_t blocker = operator_by_fragment(calc, blocker_id);
    const std::uint32_t neutral = operator_by_fragment(calc, neutral_id);
    PC_CHECK(blocker != kNoId);
    PC_CHECK(neutral == kNoId);
    if (blocker == kNoId) return;
    PC_CHECK(calc.is_candidate_operator_admitted_for_state(state, blocker));
    pc_item_state unrelated;
    pc_item_clear(&unrelated);
    unrelated.rarity = PC_RARITY_RARE;
    const std::uint32_t unrelated_state = calc.intern_item(unrelated);
    PC_CHECK(calc.is_candidate_operator_admitted_for_state(
        unrelated_state, exalt));
    PC_CHECK(!calc.is_candidate_operator_admitted_for_state(
        unrelated_state, blocker));
    const StateLocalAutomaticBatch unrelated_batch = admit_automatic(
        calc, unrelated_state,
        {{"exalt", 10.0},
         {"base", 100.0},
         {"scour", 1.0},
         {"bench:s83_mod_8", 2.0}});
    PC_CHECK(!unrelated_batch.cached);
    PC_CHECK(
        calc.is_candidate_operator_admitted_for_state(
            unrelated_state, blocker) ==
        (std::find(
             unrelated_batch.admitted_operators.begin(),
             unrelated_batch.admitted_operators.end(),
             blocker) != unrelated_batch.admitted_operators.end()));
    const OptionKernel& blocker_kernel = calc.option_kernel(state, blocker);
    check_owned_byte_ledger(calc);
    PC_CHECK(blocker_kernel.legal);
    PC_CHECK(blocker_kernel.automatic.kernel_changed);
    PC_CHECK(blocker_kernel.automatic.cleanup_complete);
    PC_CHECK((blocker_kernel.automatic.kernel_change_mechanisms &
              kAutomaticGroupConflict) != 0);

    /* Admission is price-scoped even when one CalcContext is reused. A
     * missing-price result must not be retained across a later solve. */
    CalcContext reprice_calc(
        session, goal, registry, {exalt, restart},
        false, true, true);
    const std::uint32_t reprice_state = reprice_calc.intern_item(start);
    const StateLocalAutomaticBatch missing_batch = admit_automatic(
        reprice_calc, reprice_state,
        {{"exalt", 10.0}, {"base", 100.0}, {"scour", 1.0}});
    PC_CHECK(std::none_of(
        missing_batch.admitted_operators.begin(),
        missing_batch.admitted_operators.end(),
        [&](const std::uint32_t index) {
            return reprice_calc.operators()[index].id.find(blocker_id) !=
                   std::string::npos;
        }));
    reprice_calc.reset_solve_telemetry();
    const StateLocalAutomaticBatch repriced_batch = admit_automatic(
        reprice_calc, reprice_state,
        {{"exalt", 10.0},
         {"base", 100.0},
         {"scour", 1.0},
         {"bench:s83_mod_8", 2.0}});
    PC_CHECK(std::any_of(
        repriced_batch.admitted_operators.begin(),
        repriced_batch.admitted_operators.end(),
        [&](const std::uint32_t index) {
            return reprice_calc.operators()[index].id.find(blocker_id) !=
                   std::string::npos;
        }));
    check_owned_byte_ledger(reprice_calc);

    CalcContext variant_calc(
        session, goal, registry, {exalt, restart},
        false, true, true);
    const std::uint32_t variant_state = variant_calc.intern_item(start);
    const StateLocalAutomaticBatch variant_batch = admit_automatic(
        variant_calc, variant_state,
        {{"exalt", 10.0},
         {"base", 100.0},
         {"scour", 1.0},
         {"bench:s83_mod_8", 5.0},
         {"bench:s83_mod_15", 2.0}});
    check_owned_byte_ledger(variant_calc);
    const std::uint32_t first_variant = operator_by_fragment(
        variant_calc,
        "option:temporary_bench_repeat:bench:s83_mod_8:exalt");
    const std::uint32_t cheaper_variant = operator_by_fragment(
        variant_calc,
        "option:temporary_bench_repeat:bench:s83_mod_15:exalt");
    PC_CHECK(variant_batch.temporary_collapsed_variants > 0);
    PC_CHECK(first_variant != kNoId);
    PC_CHECK(cheaper_variant != kNoId);

    CalcContext capped_calc(
        session, goal, registry, {exalt, restart},
        false, true, true);
    const std::uint32_t capped_state = capped_calc.intern_item(start);
    AutomaticAdmissionLimits capped_limits;
    capped_limits.max_state_action_rows = 100000;
    capped_limits.max_transitions = 1000000;
    capped_limits.max_solver_owned_bytes =
        capped_calc.fast_estimated_owned_bytes();
    const auto capped_prices = std::unordered_map<std::string, double>{
        {"exalt", 10.0},
        {"base", 100.0},
        {"scour", 1.0},
        {"bench:s83_mod_8", 2.0}};
    capped_limits.prices = &capped_prices;
    bool capped_witness = false;
    try {
        const StateLocalAutomaticBatch capped_batch =
            capped_calc.admit_state_local_automatic_candidates(
                capped_state, capped_limits);
        capped_witness = std::any_of(
            capped_batch.decisions.begin(), capped_batch.decisions.end(),
            [](const StateLocalAutomaticCandidate& decision) {
                return decision.deferred &&
                       decision.evidence.reason.find(
                           "max_solver_owned_bytes") != std::string::npos;
            });
    } catch (const SolverResourceLimit& limit) {
        capped_witness =
            limit.cap_name() == "max_solver_owned_bytes" &&
            limit.limit() == capped_limits.max_solver_owned_bytes;
    }
    PC_CHECK(capped_witness);
    check_owned_byte_ledger(capped_calc);

    const SolveResult variant_solve = solve(
        variant_calc, start,
        {{"exalt", 10.0},
         {"base", 100.0},
         {"scour", 1.0},
         {"bench:s83_mod_8", 5.0},
         {"bench:s83_mod_15", 2.0}});
    PC_CHECK(variant_solve.converged);
    PC_CHECK(variant_solve.policy[variant_solve.start_state].index ==
             cheaper_variant);
    PC_CHECK(std::fabs(
                 variant_solve.values[variant_solve.start_state] - 13.0) <
             1e-9);

    const auto prices = [](const double blocker_price) {
        return std::unordered_map<std::string, double>{
            {"exalt", 10.0},
            {"base", 100.0},
            {"scour", 1.0},
            {"bench:s83_mod_8", blocker_price}};
    };
    const SolveResult low = solve(calc, start, prices(2.0));
    PC_CHECK(low.converged);
    PC_CHECK(low.policy[low.start_state].index == blocker);
    PC_CHECK(std::fabs(low.values[low.start_state] - 13.0) < 1e-9);
    PC_CHECK(low.diagnostics.automatic_rows_selected > 0);
    const SolveResult high = solve(calc, start, prices(5.0));
    PC_CHECK(high.converged);
    PC_CHECK(high.policy[high.start_state].index == exalt);
    PC_CHECK(std::fabs(high.values[high.start_state] - 15.0) < 1e-9);
    const SolveResult tie = solve(calc, start, prices(4.0));
    PC_CHECK(tie.converged);
    /* Equal expected cost retains both rows; the established lower-variance
     * tie-break deterministically selects the one-outcome blocker kernel. */
    PC_CHECK(tie.policy[tie.start_state].index == blocker);
    PC_CHECK(std::fabs(tie.values[tie.start_state] - 15.0) < 1e-9);
    PC_CHECK(tie.diagnostics.automatic_rows_eligible > 0);
    if (!low.converged || low.policy[low.start_state].index != blocker) return;

    SolveOptions oracle_options;
    oracle_options.preservation_control = false;
    const SolveResult oracle = solve(calc, start, prices(2.0), oracle_options);
    PC_CHECK(oracle.converged);
    PC_CHECK(std::fabs(
                 oracle.values[oracle.start_state] -
                 low.values[low.start_state]) < 1e-12);
    PC_CHECK(oracle.policy[oracle.start_state] == low.policy[low.start_state]);

    const std::string telemetry = serialize_solver_telemetry(
        calc, &low, nullptr, std::nullopt, nullptr);
    PC_CHECK(json::Parser(telemetry.data(), telemetry.size()).parse().type ==
             json::Type::Object);
    PC_CHECK(telemetry.find("\"automatic_candidates\":{") !=
             std::string::npos);
    PC_CHECK(telemetry.find("\"by_kind\":{") != std::string::npos);
    PC_CHECK(telemetry.find("\"unique_templates\":") !=
             std::string::npos);
    PC_CHECK(telemetry.find("\"precompiled_classes\":") !=
             std::string::npos);
    PC_CHECK(telemetry.find("\"precompile_time_ns\":") !=
             std::string::npos);
    PC_CHECK(telemetry.find("\"precompiled_bytes\":") !=
             std::string::npos);
    PC_CHECK(telemetry.find("\"candidate_variants\":") !=
             std::string::npos);
    PC_CHECK(telemetry.find("\"effect_classes\":") !=
             std::string::npos);
    PC_CHECK(telemetry.find("\"enumeration_time_ns\":") !=
             std::string::npos);
    PC_CHECK(telemetry.find("\"admission_phases\":{") !=
             std::string::npos);
    PC_CHECK(telemetry.find("\"eligible\":") != std::string::npos);
    PC_CHECK(telemetry.find("\"rejected\":") != std::string::npos);
    PC_CHECK(telemetry.find("\"rejection_reasons\":{") !=
             std::string::npos);
    PC_CHECK(telemetry.find("\"kernel_evaluation_ns\":") !=
             std::string::npos);
    PC_CHECK(telemetry.find("\"protected_detail\":{") !=
             std::string::npos);
    PC_CHECK(telemetry.find("\"primitive_families\":{") !=
             std::string::npos);
    PC_CHECK(telemetry.find("\"calc_owned_byte_ledger_requests\":") !=
             std::string::npos);
    PC_CHECK(telemetry.find(
                 "\"calc_owned_byte_ledger_max_overestimate\":") !=
             std::string::npos);
    PC_CHECK(telemetry.find("\"candidate_kind\":\"temporary_bench_blocker\"") !=
             std::string::npos);

    const std::string strategy = compile_policy_strategy_json(
        calc, low, "s8.3-automatic-temporary-blocker");
    PC_CHECK(strategy.find("remove_crafted_modifiers") != std::string::npos);
    run_compiled(session, strategy, prices(2.0), 64, 8301);

    pc_item_state obsolete_craft = start;
    obsolete_craft.suffixes[0].flags |= PC_MOD_SLOT_CRAFTED;
    CalcContext cleanup_calc(
        session, goal, registry, {exalt, restart},
        false, true, true);
    const std::uint32_t cleanup_state =
        cleanup_calc.intern_item(obsolete_craft);
    const StateLocalAutomaticBatch cleanup_batch = admit_automatic(
        cleanup_calc, cleanup_state, prices(2.0));
    const std::uint32_t cleanup_blocker = operator_by_fragment(
        cleanup_calc, blocker_id);
    PC_CHECK(cleanup_blocker != kNoId);
    PC_CHECK(std::find(
                 cleanup_batch.admitted_operators.begin(),
                 cleanup_batch.admitted_operators.end(), cleanup_blocker) !=
             cleanup_batch.admitted_operators.end());
    if (cleanup_blocker != kNoId) {
        const PlannerOperator& cleanup_planner =
            cleanup_calc.operators().at(cleanup_blocker);
        PC_CHECK(cleanup_planner.primitive_program.size() == 4);
        PC_CHECK(cleanup_planner.primitive_program.front() ==
                 cleanup_planner.cleanup_action);
        PC_CHECK(cleanup_planner.primitive_program.back() ==
                 cleanup_planner.cleanup_action);
        const OptionKernel& cleanup_kernel = cleanup_calc.option_kernel(
            cleanup_state, cleanup_blocker);
        PC_CHECK(cleanup_kernel.legal);
        PC_CHECK(cleanup_kernel.automatic.setup_complete);
        PC_CHECK(cleanup_kernel.automatic.cleanup_complete);
        PC_CHECK(cleanup_kernel.automatic.kernel_changed);
        const SolveResult cleanup_winner = solve(
            cleanup_calc, obsolete_craft, prices(2.0));
        PC_CHECK(cleanup_winner.converged);
        PC_CHECK(cleanup_winner.policy[cleanup_winner.start_state].index ==
                 cleanup_blocker);
        const std::string cleanup_strategy = compile_policy_strategy_json(
            cleanup_calc, cleanup_winner,
            "s8.3-automatic-precleanup-temporary-blocker");
        PC_CHECK(cleanup_strategy.find("remove_crafted_modifiers") !=
                 std::string::npos);
        run_compiled(
            session, cleanup_strategy, prices(2.0), 64, 8311);
    }
}

void run_cannot_roll_price_flip() {
    auto session = make_automatic_session();
    ActionRegistry registry = build_action_registry(*session);
    GoalSpec goal = automatic_goal(false, true);
    const std::uint32_t exalt = registry.index_by_id.at("exalt");
    const std::uint32_t restart = registry.index_by_id.at("restart");

    pc_item_state start;
    pc_item_clear(&start);
    start.rarity = PC_RARITY_RARE;
    for (const std::uint32_t mod :
         {kGoalPrefix, kPrefixJunkA, kPrefixJunkB, kSuffixJunk}) {
        add_mod(start, *session, mod);
    }

    const auto exercise = [&](const std::uint32_t blocker_mod,
                              const std::uint64_t seed) {
        CalcContext calc(
            session, goal, registry, {exalt, restart},
            false, true, true);
        const std::uint32_t state = calc.intern_item(start);
        const std::string blocker_id =
            "bench:s83_mod_" + std::to_string(blocker_mod);
        const auto prices = std::unordered_map<std::string, double>{
            {"exalt", 10.0},
            {"base", 100.0},
            {"scour", 1.0},
            {blocker_id, 2.0}};
        const StateLocalAutomaticBatch batch =
            admit_automatic(calc, state, prices);
        const std::uint32_t option = operator_by_fragment(
            calc,
            "option:temporary_bench_repeat:" + blocker_id + ":exalt");
        PC_CHECK(option != kNoId);
        PC_CHECK(std::find(
                     batch.admitted_operators.begin(),
                     batch.admitted_operators.end(), option) !=
                 batch.admitted_operators.end());
        if (option == kNoId) return;
        const PlannerOperator& planner = calc.operators().at(option);
        PC_CHECK(planner.automatic_kind ==
                 AutomaticCandidateKind::CannotRoll);
        const OptionKernel& kernel = calc.option_kernel(state, option);
        PC_CHECK(kernel.legal);
        PC_CHECK(kernel.automatic.kernel_changed);
        PC_CHECK(kernel.automatic.setup_complete);
        PC_CHECK(kernel.automatic.cleanup_complete);
        PC_CHECK((kernel.automatic.kernel_change_mechanisms &
                  kAutomaticMetamodPoolBlock) != 0);

        const SolveResult winner = solve(calc, start, prices);
        PC_CHECK(winner.converged);
        PC_CHECK(winner.policy[winner.start_state].index == option);
        PC_CHECK(std::fabs(winner.values[winner.start_state] - 13.0) <
                 1e-9);
        const std::string telemetry = serialize_solver_telemetry(
            calc, &winner, nullptr, std::nullopt, nullptr);
        PC_CHECK(telemetry.find(
                     "\"candidate_kind\":\"cannot_roll\"") !=
                 std::string::npos);
        const std::string strategy = compile_policy_strategy_json(
            calc, winner,
            "s8.3-automatic-cannot-roll-" +
                std::to_string(blocker_mod));
        PC_CHECK(strategy.find(
                     "s83_mod_" + std::to_string(blocker_mod)) !=
                 std::string::npos);
        run_compiled(session, strategy, prices, 64, seed);
    };

    exercise(kCannotRollAttack, 8312);
    exercise(kCannotRollCaster, 8313);

    CalcContext missing_price_calc(
        session, goal, registry, {exalt, restart},
        false, true, true);
    const std::uint32_t missing_price_state =
        missing_price_calc.intern_item(start);
    const StateLocalAutomaticBatch missing_price = admit_automatic(
        missing_price_calc, missing_price_state,
        {{"exalt", 10.0}, {"base", 100.0}, {"scour", 1.0}});
    PC_CHECK(std::none_of(
        missing_price.admitted_operators.begin(),
        missing_price.admitted_operators.end(),
        [&](const std::uint32_t option) {
            return missing_price_calc.operators()[option].automatic_kind ==
                   AutomaticCandidateKind::CannotRoll;
        }));

    pc_item_state full_suffix = start;
    add_mod(full_suffix, *session, kSuffixCompetitor);
    add_mod(full_suffix, *session, kTargetBlocker);
    CalcContext illegal_carrier_calc(
        session, goal, registry, {exalt, restart},
        false, true, true);
    const std::uint32_t illegal_state =
        illegal_carrier_calc.intern_item(full_suffix);
    const StateLocalAutomaticBatch illegal = admit_automatic(
        illegal_carrier_calc, illegal_state,
        {{"exalt", 10.0},
         {"base", 100.0},
         {"scour", 1.0},
         {"bench:s83_mod_16", 2.0},
         {"bench:s83_mod_17", 2.0}});
    PC_CHECK(std::none_of(
        illegal.admitted_operators.begin(), illegal.admitted_operators.end(),
        [&](const std::uint32_t option) {
            return illegal_carrier_calc.operators()[option].automatic_kind ==
                   AutomaticCandidateKind::CannotRoll;
        }));
}

void run_multimod_finish_price_flip() {
    auto session = make_automatic_session();
    ActionRegistry registry = build_action_registry(*session);
    GoalSpec goal = automatic_goal(true, true);
    const std::uint32_t restart = registry.index_by_id.at("restart");
    CalcContext calc(
        session, goal, registry, {restart},
        false, true, true);
    pc_item_state start;
    pc_item_clear(&start);
    start.rarity = PC_RARITY_RARE;
    const std::uint32_t state = calc.intern_item(start);
    const auto prices = std::unordered_map<std::string, double>{
        {"base", 100.0},
        {"bench:s83_mod_6", 2.0},
        {"bench:s83_mod_7", 3.0},
        {"bench:s83_mod_11", 1.0}};
    const StateLocalAutomaticBatch batch =
        admit_automatic(calc, state, prices);
    const std::uint32_t multimod = operator_by_fragment(
        calc,
        "option:multimod_finish:bench:s83_mod_6+bench:s83_mod_7");
    PC_CHECK(multimod != kNoId);
    PC_CHECK(std::find(
                 batch.admitted_operators.begin(),
                 batch.admitted_operators.end(), multimod) !=
             batch.admitted_operators.end());
    if (multimod == kNoId) return;
    const OptionKernel& kernel = calc.option_kernel(state, multimod);
    PC_CHECK(kernel.legal);
    PC_CHECK(kernel.automatic.kernel_changed);
    PC_CHECK(kernel.automatic.setup_complete);
    PC_CHECK(kernel.automatic.cleanup_complete);
    PC_CHECK((kernel.automatic.kernel_change_mechanisms &
              kAutomaticDeterministicFinish) != 0);
    const SolveResult winner = solve(calc, start, prices);
    PC_CHECK(winner.converged);
    PC_CHECK(winner.policy[winner.start_state].index == multimod);
    PC_CHECK(std::fabs(winner.values[winner.start_state] - 6.0) < 1e-9);
    const std::string strategy = compile_policy_strategy_json(
        calc, winner, "s8.3-automatic-multimod-finish");
    PC_CHECK(strategy.find("s83_mod_11") != std::string::npos);
    PC_CHECK(strategy.find("s83_mod_6") != std::string::npos);
    PC_CHECK(strategy.find("s83_mod_7") != std::string::npos);
    run_compiled(session, strategy, prices, 64, 8314);

    CalcContext missing_calc(
        session, goal, registry, {restart},
        false, true, true);
    const std::uint32_t missing_state = missing_calc.intern_item(start);
    const StateLocalAutomaticBatch missing = admit_automatic(
        missing_calc, missing_state,
        {{"base", 100.0},
         {"bench:s83_mod_6", 2.0},
         {"bench:s83_mod_7", 3.0}});
    PC_CHECK(std::none_of(
        missing.admitted_operators.begin(), missing.admitted_operators.end(),
        [&](const std::uint32_t option) {
            return missing_calc.operators()[option].automatic_kind ==
                   AutomaticCandidateKind::MultimodFinish;
        }));
}

void run_protected_price_flip() {
    auto session = make_automatic_session();
    ActionRegistry registry = build_action_registry(*session);
    GoalSpec goal = automatic_goal(true, true);
    const std::uint32_t restart = registry.index_by_id.at("restart");
    const std::uint32_t alchemy = registry.index_by_id.at("alchemy");
    const std::uint32_t bench_prefix =
        registry.index_by_id.at("bench:s83_mod_6");
    const std::uint32_t bench_suffix =
        registry.index_by_id.at("bench:s83_mod_7");
    CalcContext calc(
        session, goal, registry,
        {restart, alchemy, bench_prefix, bench_suffix});
    pc_item_state start;
    pc_item_clear(&start);
    start.rarity = PC_RARITY_RARE;
    add_mod(start, *session, kGoalPrefix);
    add_mod(start, *session, kTargetBlocker);
    const std::uint32_t start_state = calc.intern_item(start);
    admit_automatic(calc, start_state, {
        {"base", 20.0},
        {"alchemy", 5.0},
        {"scour", 1.0},
        {"bench:s83_mod_6", 2.0},
        {"bench:s83_mod_7", 3.0},
        {"bench:s83_mod_9", 10.0}});

    const std::uint32_t protected_scour = operator_by_fragment(
        calc, "option:protected_side:prefix:scour");
    if (protected_scour == kNoId) {
        for (const PlannerOperator& planner : calc.operators()) {
            if (planner.automatic_kind ==
                AutomaticCandidateKind::ProtectedMetamod) {
                std::printf("s8.3 protected candidate %s\n", planner.id.c_str());
            }
        }
    }
    PC_CHECK(protected_scour != kNoId);
    if (protected_scour == kNoId) return;
    const OptionKernel& kernel = calc.option_kernel(
        start_state, protected_scour);
    PC_CHECK(kernel.legal);
    PC_CHECK(kernel.automatic.kernel_changed);
    PC_CHECK(kernel.automatic.cleanup_complete);

    const std::uint32_t chaos = registry.index_by_id.at("chaos");
    CalcContext repeat_calc(
        session, goal, registry,
        {restart, chaos, bench_prefix, bench_suffix});
    const std::uint32_t repeat_state = repeat_calc.intern_item(start);
    const StateLocalAutomaticBatch repeat_batch = admit_automatic(
        repeat_calc, repeat_state,
        {{"base", 20.0},
         {"chaos", 5.0},
         {"scour", 1.0},
         {"bench:s83_mod_6", 2.0},
         {"bench:s83_mod_7", 3.0},
         {"bench:s83_mod_9", 10.0}});
    const std::uint32_t protected_repeat = operator_by_fragment(
        repeat_calc, "option:protected_repeat:prefix:chaos");
    PC_CHECK(protected_repeat != kNoId);
    if (protected_repeat != kNoId) {
        const OptionKernel& repeat_kernel = repeat_calc.option_kernel(
            repeat_state, protected_repeat);
        PC_CHECK(repeat_kernel.legal);
        PC_CHECK(repeat_kernel.automatic.kernel_changed);
        PC_CHECK(repeat_kernel.automatic.baseline_kernel_hash != 0);
        PC_CHECK(repeat_kernel.automatic.candidate_kernel_hash != 0);
        PC_CHECK(
            repeat_kernel.automatic_candidate_attempt_entries.empty());
    }
    PC_CHECK(std::any_of(
        repeat_batch.decisions.begin(), repeat_batch.decisions.end(),
        [](const StateLocalAutomaticCandidate& decision) {
            return decision.protected_repeat_evaluations != 0 &&
                   decision.protected_retry_fallbacks == 0;
    }));
    check_owned_byte_ledger(repeat_calc);
    PC_CHECK(
        repeat_calc.telemetry().owned_byte_ledger_child_context_visits > 0);
    PC_CHECK(
        repeat_calc.telemetry().owned_byte_ledger_max_recursion_depth > 0);
    const CalcTelemetry& repeat_telemetry = repeat_calc.telemetry();
    PC_CHECK(
        repeat_telemetry.automatic_admission_reforge_active_work > 1);
    PC_CHECK(repeat_telemetry.reforge_frontier_work == 0);
    PC_CHECK(repeat_telemetry.reforge_logical_work_v1 == 0);
    PC_CHECK(
        repeat_telemetry.reforge_effort
            .nested_automatic_child_active_work ==
        repeat_telemetry.automatic_admission_reforge_active_work);
    PC_CHECK(
        repeat_telemetry.reforge_effort
            .nested_automatic_child_logical_work ==
        repeat_telemetry
            .automatic_admission_reforge_logical_work_v1);
    PC_CHECK(!repeat_telemetry.reforge_row_samples.empty());
    PC_CHECK(std::all_of(
        repeat_telemetry.reforge_row_samples.begin(),
        repeat_telemetry.reforge_row_samples.end(),
        [](const ReforgeRowTelemetry& row) {
            return row.family ==
                   ReforgeRowFamily::AutomaticOption;
        }));

    constexpr std::uint64_t capped_reforge_work = 1;
    PC_CHECK(
        repeat_telemetry
            .automatic_admission_reforge_logical_work_v1 >
        capped_reforge_work);
    CalcContext capped_repeat_calc(
        session, goal, registry,
        {restart, chaos, bench_prefix, bench_suffix});
    const std::uint32_t capped_repeat_state =
        capped_repeat_calc.intern_item(start);
    const auto capped_repeat_prices =
        std::unordered_map<std::string, double>{
            {"base", 20.0},
            {"chaos", 5.0},
            {"scour", 1.0},
            {"bench:s83_mod_6", 2.0},
            {"bench:s83_mod_7", 3.0},
            {"bench:s83_mod_9", 10.0}};
    AutomaticAdmissionLimits capped_repeat_limits;
    capped_repeat_limits.max_state_action_rows = 100000;
    capped_repeat_limits.max_transitions = 1000000;
    capped_repeat_limits.max_solver_owned_bytes = 1073741824;
    capped_repeat_limits.prices = &capped_repeat_prices;
    capped_repeat_calc.set_solve_resource_caps(
        std::numeric_limits<std::uint32_t>::max(),
        capped_reforge_work, false,
        capped_repeat_limits.max_solver_owned_bytes);
    const StateLocalAutomaticBatch capped_repeat_batch =
        capped_repeat_calc.admit_state_local_automatic_candidates(
            capped_repeat_state, capped_repeat_limits);
    const CalcTelemetry& capped_repeat_telemetry =
        capped_repeat_calc.telemetry();
    PC_CHECK(
        capped_repeat_batch.status ==
        StateLocalAutomaticBatchStatus::Complete);
    PC_CHECK(
        capped_repeat_telemetry.reforge_frontier_work == 0);
    PC_CHECK(
        capped_repeat_telemetry.reforge_logical_work_v1 == 0);
    PC_CHECK(
        capped_repeat_telemetry
            .automatic_admission_reforge_logical_work_v1 ==
        capped_repeat_telemetry.reforge_effort
            .nested_automatic_child_logical_work);
    PC_CHECK(
        capped_repeat_telemetry
            .automatic_admission_reforge_logical_work_v1 >
        capped_reforge_work);
    PC_CHECK(
        capped_repeat_telemetry.reforge_effort
            .nested_automatic_child_active_work ==
        capped_repeat_telemetry
            .automatic_admission_reforge_active_work);
    PC_CHECK(
        capped_repeat_telemetry.reforge_effort.rows_interrupted == 0);
    PC_CHECK(std::none_of(
        capped_repeat_telemetry.reforge_row_samples.begin(),
        capped_repeat_telemetry.reforge_row_samples.end(),
        [](const ReforgeRowTelemetry& row) {
            return row.family == ReforgeRowFamily::AutomaticOption &&
                   row.disposition ==
                       ReforgeRowDisposition::Interrupted;
        }));
    PC_CHECK(std::none_of(
        capped_repeat_batch.decisions.begin(),
        capped_repeat_batch.decisions.end(),
        [](const StateLocalAutomaticCandidate& decision) {
            return decision.deferred &&
                   decision.evidence.reason.find(
                       "max_reforge_work") != std::string::npos;
        }));

    const auto prices = [](const double lock_price) {
        return std::unordered_map<std::string, double>{
            {"base", 20.0},
            {"alchemy", 5.0},
            {"scour", 1.0},
            {"bench:s83_mod_6", 2.0},
            {"bench:s83_mod_7", 3.0},
            {"bench:s83_mod_9", lock_price}};
    };
    const SolveResult low = solve(calc, start, prices(10.0));
    PC_CHECK(low.converged);
    PC_CHECK(low.policy[low.start_state].index == protected_scour);
    PC_CHECK(std::fabs(low.values[low.start_state] - 14.0) < 1e-9);
    const SolveResult high = solve(calc, start, prices(30.0));
    PC_CHECK(high.converged);
    PC_CHECK(high.policy[high.start_state].index == restart);
    PC_CHECK(std::fabs(high.values[high.start_state] - 27.0) < 1e-9);
    if (!low.converged ||
        low.policy[low.start_state].index != protected_scour) return;

    const std::string strategy = compile_policy_strategy_json(
        calc, low, "s8.3-automatic-protected-side");
    run_compiled(session, strategy, prices(10.0), 64, 8302);

    ActionRegistry unsupported = registry;
    for (const ActionType type : {ActionType::Fossil, ActionType::Essence}) {
        ActionDescriptor action;
        action.id = type == ActionType::Fossil ? "fossil:test"
                                                : "essence:test";
        action.display_name = action.id;
        action.params.type = type;
        action.kind = TransitionKind::Reforge;
        action.cost_keys = {action.id};
        action.preservation.destructive_renewal = true;
        action.preservation.preserves_fractured_affixes = true;
        action.refinement =
            derive_action_refinement_contract(*session, action);
        validate_action_refinement_contract(action);
        unsupported.index_by_id.emplace(
            action.id,
            static_cast<std::uint32_t>(unsupported.actions.size()));
        unsupported.actions.push_back(std::move(action));
    }
    CalcContext unsupported_calc(
        session, goal, unsupported,
        {restart, alchemy, bench_prefix, bench_suffix});
    for (const PlannerOperator& planner : unsupported_calc.operators()) {
        if (planner.automatic_kind ==
            AutomaticCandidateKind::ProtectedMetamod) {
            PC_CHECK(planner.id.find("fossil:test") == std::string::npos);
            PC_CHECK(planner.id.find("essence:test") == std::string::npos);
        }
    }
}

void run_protected_producibility_filter() {
    auto session = make_automatic_session();
    ActionRegistry registry = build_action_registry(*session);

    const auto chaos_reachable = action_explicit_affix_reachable_mask(
        *session, registry.actions.at(registry.index_by_id.at("chaos")));
    PC_CHECK(pc_bitset_test(chaos_reachable.data(), kGoalSuffix));
    PC_CHECK(!pc_bitset_test(chaos_reachable.data(), kBenchNeutral));
    const auto bench_reachable = action_explicit_affix_reachable_mask(
        *session,
        registry.actions.at(
            registry.index_by_id.at("bench:s83_mod_12")));
    PC_CHECK(pc_bitset_test(bench_reachable.data(), kBenchNeutral));

    GoalSpec goal;
    goal.rarity = PC_RARITY_RARE;
    goal.automatic_candidates = true;
    GoalSlot protected_prefix;
    protected_prefix.family_id = session->family_id[kGoalPrefix];
    goal.slots.push_back(protected_prefix);
    GoalSlot crafted_only_suffix;
    crafted_only_suffix.family_id = session->family_id[kBenchNeutral];
    goal.slots.push_back(crafted_only_suffix);

    const std::uint32_t restart = registry.index_by_id.at("restart");
    const std::uint32_t chaos = registry.index_by_id.at("chaos");
    CalcContext calc(session, goal, registry, {restart, chaos});
    pc_item_state start;
    pc_item_clear(&start);
    start.rarity = PC_RARITY_RARE;
    add_mod(start, *session, kGoalPrefix);
    const std::uint32_t state = calc.intern_item(start);
    const StateLocalAutomaticBatch batch = admit_automatic(
        calc, state,
        {{"base", 20.0},
         {"chaos", 5.0},
         {"scour", 1.0},
         {"bench:s83_mod_9", 10.0},
         {"bench:s83_mod_12", 2.0}});
    PC_CHECK(std::none_of(
        batch.decisions.begin(), batch.decisions.end(),
        [](const StateLocalAutomaticCandidate& decision) {
            return decision.id.find(
                       "option:protected_repeat:prefix:chaos") !=
                   std::string::npos;
        }));
}

void run_fracture_price_flip() {
    auto session = make_automatic_session();
    ActionRegistry registry = build_action_registry(*session);
    GoalSpec goal = automatic_goal(true, true);
    const std::uint32_t restart = registry.index_by_id.at("restart");
    const std::uint32_t alchemy = registry.index_by_id.at("alchemy");
    const std::uint32_t chaos = registry.index_by_id.at("chaos");
    const std::uint32_t bench_prefix =
        registry.index_by_id.at("bench:s83_mod_6");
    CalcContext calc(
        session, goal, registry,
        {restart, alchemy, chaos, registry.index_by_id.at("fracture"),
         bench_prefix});
    pc_item_state start;
    pc_item_clear(&start);
    start.rarity = PC_RARITY_RARE;
    for (const std::uint32_t mod :
         {kGoalPrefix, kPrefixJunkA, kPrefixJunkB, kTargetBlocker}) {
        add_mod(start, *session, mod);
    }
    const std::uint32_t start_state = calc.intern_item(start);
    admit_automatic(calc, start_state, {
        {"base", 20.0},
        {"alchemy", 5.0},
        {"chaos", 30.0},
        {"fracture", 23.0},
        {"bench:s83_mod_6", 100.0}});
    const std::uint32_t fracture = operator_by_kind(
        calc, AutomaticCandidateKind::Fracture);
    PC_CHECK(fracture != kNoId);
    if (fracture == kNoId) return;
    const PlannerOperator& fracture_operator = calc.operators().at(fracture);
    PC_CHECK(fracture_operator.kind == PlannerOperatorKind::Primitive);
    PC_CHECK(fracture_operator.id == "fracture");
    const std::uint32_t fracture_action =
        fracture_operator.primitive_action;
    PC_CHECK(std::none_of(
        calc.operators().begin(), calc.operators().end(),
        [](const PlannerOperator& planner) {
            return planner.option_kind == FixedOptionKind::FracturePrepare &&
                   planner.automatic_kind ==
                       AutomaticCandidateKind::Fracture;
        }));
    const OutcomeDistribution& distribution = calc.outcomes(
        start_state, fracture_action);
    PC_CHECK(distribution.supported);
    PC_CHECK(distribution.choice_groups.empty());
    PC_CHECK(distribution.entries.size() == 4);
    double fracture_probability = 0.0;
    for (const OutcomeEntry& entry : distribution.entries) {
        fracture_probability += entry.probability;
    }
    PC_CHECK(std::fabs(fracture_probability - 1.0) < 1e-12);

    const auto prices = [](const double fracture_price) {
        return std::unordered_map<std::string, double>{
            {"base", 20.0},
            {"alchemy", 5.0},
            {"chaos", 30.0},
            {"fracture", fracture_price},
            {"bench:s83_mod_6", 100.0}};
    };
    const SolveResult low = solve(calc, start, prices(23.0));
    PC_CHECK(low.converged);
    PC_CHECK(low.policy[low.start_state].index == fracture);
    PC_CHECK(std::fabs(low.values[low.start_state] - 124.25) < 1e-9);
    const std::string fracture_telemetry = serialize_solver_telemetry(
        calc, &low, nullptr, std::nullopt, nullptr);
    PC_CHECK(fracture_telemetry.find(
                 "\"candidate_kind\":\"fracture\"") !=
             std::string::npos);
    PC_CHECK(fracture_telemetry.find(
                 "\"candidate\":\"fracture\"") !=
             std::string::npos);
    PC_CHECK(fracture_telemetry.find(
                 "exact_goal_relevant_primitive_fracture_distribution") !=
             std::string::npos);
    PC_CHECK(fracture_telemetry.find("option:fracture_prepare") ==
             std::string::npos);
    const SolveResult high = solve(calc, start, prices(24.0));
    PC_CHECK(high.converged);
    PC_CHECK(high.policy[high.start_state].index == restart);
    PC_CHECK(std::fabs(high.values[high.start_state] - 125.0) < 1e-9);
    if (!low.converged || low.policy[low.start_state].index != fracture) return;

    pc_item_state influenced = start;
    influenced.generic_influence_bits = 1;
    PC_CHECK(!action_legal(
        *session, registry.actions.at(fracture_action),
        calc.state(calc.intern_item(influenced))));

    ActionRegistry relevance_registry;
    for (const std::string& id : {std::string("restart"),
                                  std::string("fracture")}) {
        relevance_registry.index_by_id.emplace(
            id, static_cast<std::uint32_t>(
                    relevance_registry.actions.size()));
        relevance_registry.actions.push_back(
            registry.actions.at(registry.index_by_id.at(id)));
    }
    CalcContext relevance_calc(
        session, goal, relevance_registry, {0, 1});
    pc_item_state irrelevant = {};
    pc_item_clear(&irrelevant);
    irrelevant.rarity = PC_RARITY_RARE;
    for (const std::uint32_t mod :
         {kPrefixJunkA, kPrefixJunkB, kTargetBlocker, kSuffixJunk}) {
        add_mod(irrelevant, *session, mod);
    }
    SolveOptions relevance_options;
    relevance_options.max_expanded_states = 1;
    const SolveResult irrelevant_result = solve(
        relevance_calc, irrelevant,
        {{"base", 20.0}, {"fracture", 23.0}}, relevance_options);
    /* The only successor is Restart's fresh base. Refused Fracture outcomes
     * must not add four unreachable fractured states. */
    PC_CHECK(irrelevant_result.diagnostics.discovered_states == 2);
    const std::string irrelevant_telemetry = serialize_solver_telemetry(
        relevance_calc, &irrelevant_result, nullptr, std::nullopt, nullptr);
    PC_CHECK(irrelevant_telemetry.find(
                 "no_unfractured_satisfied_goal_carrier") !=
             std::string::npos);

    auto missing_base = prices(23.0);
    missing_base.erase("base");
    bool missing_base_refused = false;
    try {
        (void)solve(calc, start, missing_base);
    } catch (const std::invalid_argument& ex) {
        missing_base_refused =
            std::string(ex.what()).find(
                "requires a priced base for Restart miss recovery") !=
            std::string::npos;
    }
    PC_CHECK(missing_base_refused);

    GoalSpec manual_goal = automatic_goal(true, true);
    manual_goal.automatic_candidates = false;
    FixedOptionSpec manual;
    manual.kind = FixedOptionKind::FracturePrepare;
    manual.program_action_ids = {"chaos"};
    manual.carrier_goal_slot = 0;
    manual_goal.fixed_options.push_back(manual);
    CalcContext manual_calc(
        session, manual_goal, registry,
        {restart, alchemy, chaos, bench_prefix});
    const SolveResult enumerated = solve(
        manual_calc, start, prices(23.0));
    PC_CHECK(enumerated.converged);
    PC_CHECK(std::fabs(
                 enumerated.values[enumerated.start_state] -
                 low.values[low.start_state]) < 1e-12);

    const std::string strategy = compile_policy_strategy_json(
        calc, low, "s8.3-automatic-fracture");
    run_compiled(session, strategy, prices(23.0), 512, 8303);
}

void run_incomplete_dependency_refusals() {
    auto session = make_automatic_session();
    ActionRegistry registry = build_action_registry(*session);
    GoalSpec goal = automatic_goal(true, true);
    ActionRegistry no_cleanup = registry;
    no_cleanup.actions.erase(
        no_cleanup.actions.begin() +
        no_cleanup.index_by_id.at("remove_crafted_modifiers"));
    no_cleanup.index_by_id.clear();
    for (std::uint32_t i = 0; i < no_cleanup.actions.size(); ++i) {
        no_cleanup.index_by_id.emplace(no_cleanup.actions[i].id, i);
    }
    CalcContext cleanup_calc(
        session, goal, no_cleanup,
        {no_cleanup.index_by_id.at("restart")});
    for (const PlannerOperator& planner : cleanup_calc.operators()) {
        PC_CHECK(planner.automatic_kind !=
                 AutomaticCandidateKind::TemporaryBenchBlocker);
    }

    ActionRegistry no_lock = registry;
    no_lock.actions.erase(
        std::remove_if(
            no_lock.actions.begin(), no_lock.actions.end(),
            [&](const ActionDescriptor& action) {
                return action.params.type == ActionType::Bench &&
                       action.params.mod_id == kPrefixLock;
            }),
        no_lock.actions.end());
    no_lock.index_by_id.clear();
    for (std::uint32_t i = 0; i < no_lock.actions.size(); ++i) {
        no_lock.index_by_id.emplace(no_lock.actions[i].id, i);
    }
    CalcContext lock_calc(
        session, goal, no_lock,
        {no_lock.index_by_id.at("restart")});
    for (const PlannerOperator& planner : lock_calc.operators()) {
        PC_CHECK(planner.id.find("option:protected_side:prefix") ==
                 std::string::npos);
        PC_CHECK(planner.id.find("option:protected_repeat:prefix") ==
                 std::string::npos);
    }
}

void run_carrier_relative_renewal_templates() {
    auto session = make_automatic_session(true);
    ActionRegistry registry = build_action_registry(*session);
    GoalSpec goal = automatic_goal(false, true);
    goal.automatic_candidates = false;
    FixedOptionSpec renewal;
    renewal.kind = FixedOptionKind::Renewal;
    renewal.program_action_ids = {"scour", "alchemy"};
    renewal.exit_goal_slots = {0};
    renewal.exit_min_satisfied = 1;
    goal.fixed_options.push_back(renewal);

    CalcContext calc(
        session, goal, registry,
        {registry.index_by_id.at("scour"),
         registry.index_by_id.at("alchemy"),
         registry.index_by_id.at("restart")},
        false, true, true);
    const std::uint32_t renewal_index = operator_by_fragment(
        calc, "option:renewal:scour+alchemy:until:1:0");
    PC_CHECK(renewal_index != kNoId);
    if (renewal_index == kNoId) return;

    pc_item_state blocked;
    pc_item_clear(&blocked);
    blocked.rarity = PC_RARITY_RARE;
    add_mod(blocked, *session, kTargetBlocker);

    pc_item_state junk;
    pc_item_clear(&junk);
    junk.rarity = PC_RARITY_RARE;
    add_mod(junk, *session, kSuffixJunk);

    pc_item_state fractured = junk;
    fractured.suffixes[0].flags |= PC_MOD_SLOT_FRACTURED;

    const std::uint32_t blocked_state = calc.intern_item(blocked);
    const std::uint32_t junk_state = calc.intern_item(junk);
    const std::uint32_t fractured_state = calc.intern_item(fractured);
    PC_CHECK(blocked_state != junk_state);
    PC_CHECK(junk_state != fractured_state);

    const OptionKernel& blocked_kernel =
        calc.option_kernel(blocked_state, renewal_index);
    const OptionKernel& junk_kernel =
        calc.option_kernel(junk_state, renewal_index);
    PC_CHECK(blocked_kernel.legal);
    PC_CHECK(junk_kernel.legal);
    PC_CHECK(blocked_kernel.retained_template_id != 0);
    PC_CHECK(blocked_kernel.retained_template_id ==
             junk_kernel.retained_template_id);
    PC_CHECK(&blocked_kernel == &junk_kernel);
    PC_CHECK(calc.option_kernel_template_hit(junk_state, renewal_index));
    PC_CHECK(blocked_kernel.expected_resources ==
             junk_kernel.expected_resources);
    PC_CHECK(blocked_kernel.retry_states == junk_kernel.retry_states);
    PC_CHECK(std::any_of(
        blocked_kernel.exits.begin(), blocked_kernel.exits.end(),
        [](const OutcomeEntry& exit) { return exit.state == kNoId; }));

    double retry_mass = 0.0;
    double exit_mass = 0.0;
    for (const OutcomeEntry& exit : blocked_kernel.exits) {
        if (exit.state == kNoId) {
            retry_mass += exit.probability;
        } else {
            exit_mass += exit.probability;
            PC_CHECK(calc.is_goal_state(calc.state(exit.state)));
        }
    }
    PC_CHECK(std::fabs(retry_mass + exit_mass - 1.0) < 1e-12);
    PC_CHECK(exit_mass > 0.0);
    double attempt_cost = 0.0;
    for (const auto& [key, quantity] :
         blocked_kernel.expected_resources) {
        if (key == "scour" || key == "alchemy") {
            attempt_cost += quantity;
        }
    }
    const double manual_retry_value = attempt_cost / exit_mass;
    PC_CHECK(std::isfinite(manual_retry_value));
    PC_CHECK(std::fabs(
                 manual_retry_value -
                 attempt_cost / (1.0 - retry_mass)) < 1e-12);

    const OptionKernel& fractured_kernel =
        calc.option_kernel(fractured_state, renewal_index);
    PC_CHECK(!fractured_kernel.legal);
    PC_CHECK(fractured_kernel.retained_template_id == 0);
    PC_CHECK(&fractured_kernel != &blocked_kernel);
}

void run_fixed_option_product_parent_refinement_trigger() {
    auto session = make_automatic_session();
    ActionRegistry registry = build_action_registry(*session);
    GoalSpec goal = automatic_goal(false, true);
    goal.automatic_candidates = false;
    FixedOptionSpec protected_side;
    protected_side.kind = FixedOptionKind::ProtectedSide;
    protected_side.side = PC_SIDE_PREFIX;
    protected_side.action_id = "chaos";
    goal.fixed_options.push_back(protected_side);

    const std::uint32_t restart =
        registry.index_by_id.at("restart");
    CalcContext calc(
        session, goal, registry, {restart},
        false, true, false, std::nullopt, {}, true);
    PC_CHECK(calc.product_solver_parent());
    const std::uint32_t protected_chaos =
        operator_by_fragment(
            calc, "option:protected_side:prefix:chaos");
    PC_CHECK(protected_chaos != kNoId);
    if (protected_chaos == kNoId) return;

    pc_item_state start;
    pc_item_clear(&start);
    start.rarity = PC_RARITY_RARE;
    add_mod(start, *session, kPrefixJunkA);
    const SolveResult solved = solve(
        calc, start,
        {{"base", 100.0},
         {"chaos", 1.0},
         {"bench:s83_mod_9", 1.0}});

    PC_CHECK(solved.policy_available);
    PC_CHECK(
        solved.policy[solved.start_state].kind ==
        PlannerOperatorKind::FixedOption);
    PC_CHECK(
        solved.policy[solved.start_state].index ==
        protected_chaos);
    PC_CHECK(
        solved.diagnostics.policy_refinement.triggers > 0);
    PC_CHECK(std::any_of(
        solved.diagnostics.policy_refinement
            .counterexample_samples.begin(),
        solved.diagnostics.policy_refinement
            .counterexample_samples.end(),
        [](const std::string& witness) {
            return witness.find(
                       "\"operator_kind\":\"fixed_option\"") !=
                   std::string::npos;
        }));
    PC_CHECK(
        solved.diagnostics.policy_compatibility_supported);
    PC_CHECK(
        !solved.refined_policy_artifact.strategy_json.empty());
    PC_CHECK(
        solved.termination ==
        SolveTermination::ExactClosed);
}

void run_planner_operator_import_authority() {
    auto session = make_automatic_session(true);
    ActionRegistry registry = build_action_registry(*session);
    GoalSpec goal = automatic_goal(false, true);
    goal.automatic_candidates = false;
    FixedOptionSpec renewal;
    renewal.kind = FixedOptionKind::Renewal;
    renewal.program_action_ids = {"scour", "alchemy"};
    renewal.exit_goal_slots = {0};
    renewal.exit_min_satisfied = 1;
    renewal.automatic_kind =
        AutomaticCandidateKind::ConstructiveRenewal;
    renewal.relevant_goal_mask = 1;
    renewal.constructive_finish_action_id = "bench:s83_mod_6";
    goal.fixed_options.push_back(renewal);

    const auto candidates_for =
        [](const ActionRegistry& value) {
        return std::vector<std::uint32_t>{
            value.index_by_id.at("scour"),
            value.index_by_id.at("alchemy"),
            value.index_by_id.at("restart")};
    };
    CalcContext source(
        session, goal, registry, candidates_for(registry),
        false, true, true);
    const std::uint32_t source_option = operator_by_fragment(
        source, "option:renewal:scour+alchemy");
    PC_CHECK(source_option != kNoId);
    if (source_option == kNoId) return;
    const PlannerOperator& source_planner =
        source.operators().at(source_option);
    PC_CHECK(
        source_planner.constructive_finish_action_id ==
        "bench:s83_mod_6");
    PC_CHECK(
        source_planner.primitive_program_action_ids ==
        std::vector<std::string>({"scour", "alchemy"}));
    const PlannerOperatorRuntimeSemantics source_runtime =
        planner_operator_runtime_semantics(
            source_planner, registry);
    PC_CHECK(source_runtime.ordered_program.size() == 2);
    PC_CHECK(
        source_runtime.ordered_program[0].action ==
        source_planner.primitive_program[0]);
    PC_CHECK(
        source_runtime.ordered_program[1].action ==
        source_planner.primitive_program[1]);
    PC_CHECK(
        source_runtime.ordered_program[0].refinement.complete());
    PC_CHECK(
        source_runtime.ordered_program[1].refinement.complete());
    PC_CHECK(source_runtime.execution_paths.size() == 1);
    PC_CHECK(
        source_runtime.execution_paths.front().size() ==
        source_runtime.ordered_program.size());
    for (std::size_t step = 0;
         step < source_runtime.ordered_program.size(); ++step) {
        PC_CHECK(
            source_runtime.execution_paths.front()[step].action ==
            source_runtime.ordered_program[step].action);
    }
    std::vector<std::uint32_t> expected_dependencies =
        source_planner.primitive_program;
    std::sort(
        expected_dependencies.begin(),
        expected_dependencies.end());
    expected_dependencies.erase(
        std::unique(
            expected_dependencies.begin(),
            expected_dependencies.end()),
        expected_dependencies.end());
    PC_CHECK(
        source_runtime.action_dependencies ==
        expected_dependencies);
    PC_CHECK(
        std::find(
            source_runtime.action_dependencies.begin(),
            source_runtime.action_dependencies.end(),
            source_planner.constructive_finish_action) ==
        source_runtime.action_dependencies.end());
    PC_CHECK(
        source_runtime.compatibility_refinement.complete());

    PlannerOperator reversed_program = source_planner;
    std::reverse(
        reversed_program.primitive_program.begin(),
        reversed_program.primitive_program.end());
    std::reverse(
        reversed_program.primitive_program_action_ids.begin(),
        reversed_program.primitive_program_action_ids.end());
    const PlannerOperatorRuntimeSemantics reversed_runtime =
        planner_operator_runtime_semantics(
            reversed_program, registry);
    PC_CHECK(
        reversed_runtime.ordered_program.front().action ==
        reversed_program.primitive_program.front());
    PC_CHECK(
        reversed_runtime.ordered_program.back().action ==
        reversed_program.primitive_program.back());
    PC_CHECK(
        reversed_runtime.action_dependencies ==
        source_runtime.action_dependencies);
    PC_CHECK(
        action_refinement_contract_signature(
            reversed_runtime.compatibility_refinement) ==
        action_refinement_contract_signature(
            source_runtime.compatibility_refinement));

    const std::uint32_t source_chaos =
        registry.index_by_id.at("chaos");
    PlannerOperator conditional_program = source_planner;
    conditional_program.conditional_action = source_chaos;
    conditional_program.conditional_action_id = "chaos";
    const PlannerOperatorRuntimeSemantics conditional_runtime =
        planner_operator_runtime_semantics(
            conditional_program, registry);
    PC_CHECK(
        conditional_runtime.ordered_program.size() ==
        source_runtime.ordered_program.size() + 1);
    PC_CHECK(
        conditional_runtime.ordered_program.back().action ==
        source_chaos);
    const auto has_runtime_path =
        [](const PlannerOperatorRuntimeSemantics& runtime,
           const std::vector<std::uint32_t>& actions) {
            return std::any_of(
                runtime.execution_paths.begin(),
                runtime.execution_paths.end(),
                [&](const std::vector<
                        PlannerOperatorRuntimeStep>& path) {
                    if (path.size() != actions.size()) return false;
                    for (std::size_t index = 0;
                         index < path.size(); ++index) {
                        if (path[index].action != actions[index]) {
                            return false;
                        }
                    }
                    return true;
                });
        };
    PC_CHECK(conditional_runtime.execution_paths.size() == 3);
    PC_CHECK(has_runtime_path(
        conditional_runtime, {source_chaos}));
    PC_CHECK(has_runtime_path(
        conditional_runtime,
        source_planner.primitive_program));
    std::vector<std::uint32_t> prepared_then_conditional =
        source_planner.primitive_program;
    prepared_then_conditional.push_back(source_chaos);
    PC_CHECK(has_runtime_path(
        conditional_runtime,
        prepared_then_conditional));
    conditional_program.primitive_program.push_back(
        source_chaos);
    conditional_program.primitive_program_action_ids.push_back(
        "chaos");
    const PlannerOperatorRuntimeSemantics included_conditional_runtime =
        planner_operator_runtime_semantics(
            conditional_program, registry);
    PC_CHECK(
        included_conditional_runtime.ordered_program.size() ==
        conditional_program.primitive_program.size());

    ActionRegistry permuted = registry;
    std::reverse(
        permuted.actions.begin(), permuted.actions.end());
    permuted.index_by_id.clear();
    for (std::uint32_t index = 0;
         index < permuted.actions.size(); ++index) {
        permuted.index_by_id.emplace(
            permuted.actions[index].id, index);
    }
    CalcContext destination(
        session, goal, permuted, candidates_for(permuted),
        false, true, true);
    const std::uint32_t destination_option =
        destination.import_planner_operator(
            source_planner, false);
    PC_CHECK(
        destination_option <
        destination.operators().size());
    const PlannerOperator& destination_planner =
        destination.operators().at(destination_option);
    PC_CHECK(planner_operator_structurally_equal(
        source_planner, destination_planner));
    PC_CHECK(
        planner_operator_semantic_key(source_planner) ==
        planner_operator_semantic_key(destination_planner));
    PC_CHECK(
        source_planner.primitive_program !=
        destination_planner.primitive_program);
    for (std::size_t i = 0;
         i < destination_planner.primitive_program.size(); ++i) {
        PC_CHECK(
            permuted.actions.at(
                destination_planner.primitive_program[i]).id ==
            destination_planner
                .primitive_program_action_ids[i]);
    }

    const std::uint32_t destination_chaos =
        destination.import_planner_operator(
            source.operators().at(source_chaos), false);
    PC_CHECK(
        destination_chaos ==
        permuted.index_by_id.at("chaos"));
    PC_CHECK(destination_chaos != source_chaos);
    PC_CHECK(
        destination.operators()
            .at(destination_chaos)
            .primitive_action_id == "chaos");
    const PlannerOperatorRuntimeSemantics primitive_runtime =
        planner_operator_runtime_semantics(
            source.operators().at(source_chaos), registry);
    PC_CHECK(primitive_runtime.ordered_program.size() == 1);
    PC_CHECK(primitive_runtime.execution_paths.size() == 1);
    PC_CHECK(
        primitive_runtime.execution_paths.front().size() == 1);
    PC_CHECK(
        primitive_runtime.execution_paths.front().front().action ==
        source_chaos);
    PC_CHECK(
        primitive_runtime.ordered_program.front().action ==
        source_chaos);
    PC_CHECK(
        primitive_runtime.action_dependencies ==
        std::vector<std::uint32_t>{source_chaos});
    PC_CHECK(
        action_refinement_contract_signature(
            primitive_runtime.compatibility_refinement) ==
        action_refinement_contract_signature(
            registry.actions.at(source_chaos).refinement));

    PlannerOperator imprint_runtime_planner;
    imprint_runtime_planner.id = "option:test_imprint_runtime";
    imprint_runtime_planner.kind =
        PlannerOperatorKind::FixedOption;
    imprint_runtime_planner.option_kind =
        FixedOptionKind::ImprintRetry;
    imprint_runtime_planner.primitive_program = {source_chaos};
    imprint_runtime_planner.primitive_program_action_ids = {
        "chaos"};
    imprint_runtime_planner.bestiary_create_action = 0;
    imprint_runtime_planner.bestiary_create_action_id =
        "bestiary:imprint";
    imprint_runtime_planner.bestiary_restore_action = 1;
    imprint_runtime_planner.bestiary_restore_action_id =
        "bestiary:restore_imprint";
    const PlannerOperatorRuntimeSemantics imprint_runtime =
        planner_operator_runtime_semantics(
            imprint_runtime_planner, registry);
    PC_CHECK(imprint_runtime.ordered_program.size() == 2);
    PC_CHECK(
        imprint_runtime.ordered_program.front().action == kNoId);
    PC_CHECK(
        (imprint_runtime.ordered_program.front()
             .refinement.observed_item_features &
         refinement_feature(RefinementFeature::Rarity)) != 0);
    PC_CHECK(
        (imprint_runtime.ordered_program.front()
             .refinement.observed_item_features &
         refinement_feature(RefinementFeature::Corrupted)) != 0);
    PC_CHECK(
        (imprint_runtime.ordered_program.front()
             .refinement.observed_item_features &
         refinement_feature(RefinementFeature::Mirrored)) != 0);
    PC_CHECK(imprint_runtime.execution_paths.size() == 1);
    PC_CHECK(
        imprint_runtime.execution_paths.front().size() ==
        imprint_runtime.ordered_program.size());
    PC_CHECK(
        imprint_runtime.action_dependencies ==
        std::vector<std::uint32_t>{source_chaos});

    PlannerOperator presentation = source_planner;
    presentation.id = "presentation-only-id";
    presentation.display_name = "Presentation only";
    presentation.primitive_program.assign(
        presentation.primitive_program.size(), kNoId - 1);
    presentation.constructive_finish_action = kNoId - 2;
    PC_CHECK(planner_operator_structurally_equal(
        source_planner, presentation));
    PC_CHECK(
        planner_operator_semantic_key(source_planner) ==
        planner_operator_semantic_key(presentation));

    const auto expect_distinct =
        [&](const PlannerOperator& changed) {
        PC_CHECK(!planner_operator_structurally_equal(
            source_planner, changed));
        PC_CHECK(
            planner_operator_semantic_key(source_planner) !=
            planner_operator_semantic_key(changed));
    };
    {
        PlannerOperator changed = source_planner;
        changed.kind = PlannerOperatorKind::Primitive;
        expect_distinct(changed);
    }
    {
        PlannerOperator changed = source_planner;
        changed.option_kind = FixedOptionKind::ProtectedRepeat;
        expect_distinct(changed);
    }
    {
        PlannerOperator changed = source_planner;
        changed.primitive_action_id = "chaos";
        expect_distinct(changed);
    }
    {
        PlannerOperator changed = source_planner;
        changed.primitive_program_action_ids.push_back("chaos");
        expect_distinct(changed);
    }
    {
        PlannerOperator changed = source_planner;
        changed.intended_side = PC_SIDE_PREFIX;
        expect_distinct(changed);
    }
    {
        PlannerOperator changed = source_planner;
        changed.exit_goal_slots.push_back(7);
        expect_distinct(changed);
    }
    {
        PlannerOperator changed = source_planner;
        ++changed.exit_min_satisfied;
        expect_distinct(changed);
    }
    {
        PlannerOperator changed = source_planner;
        changed.carrier_goal_slot = 0;
        expect_distinct(changed);
    }
    {
        PlannerOperator changed = source_planner;
        changed.conditional_action_id = "fracture";
        expect_distinct(changed);
    }
    {
        PlannerOperator changed = source_planner;
        changed.bestiary_create_action_id =
            "bestiary:create";
        expect_distinct(changed);
    }
    {
        PlannerOperator changed = source_planner;
        changed.bestiary_restore_action_id =
            "bestiary:restore";
        expect_distinct(changed);
    }
    {
        PlannerOperator changed = source_planner;
        changed.automatic_kind =
            AutomaticCandidateKind::Imprint;
        expect_distinct(changed);
    }
    {
        PlannerOperator changed = source_planner;
        changed.relevant_goal_mask ^= 2;
        expect_distinct(changed);
    }
    {
        PlannerOperator changed = source_planner;
        changed.setup_action_id = "scour";
        expect_distinct(changed);
    }
    {
        PlannerOperator changed = source_planner;
        changed.followup_action_id = "alchemy";
        expect_distinct(changed);
    }
    {
        PlannerOperator changed = source_planner;
        changed.cleanup_action_id = "scour";
        expect_distinct(changed);
    }
    {
        PlannerOperator changed = source_planner;
        changed.constructive_finish_action_id =
            "bench:s83_mod_7";
        expect_distinct(changed);
    }
    {
        PlannerOperator changed = source_planner;
        changed.resource_quantities.front().second += 1.0;
        expect_distinct(changed);
    }

    const std::size_t before_import =
        destination.operators().size();
    PlannerOperator alternate_finish = source_planner;
    alternate_finish.constructive_finish_action =
        registry.index_by_id.at("bench:s83_mod_7");
    alternate_finish.constructive_finish_action_id =
        "bench:s83_mod_7";
    const PlannerOperatorRuntimeSemantics alternate_finish_runtime =
        planner_operator_runtime_semantics(
            alternate_finish, registry);
    PC_CHECK(
        alternate_finish_runtime.action_dependencies ==
        source_runtime.action_dependencies);
    PC_CHECK(
        alternate_finish_runtime.ordered_program.size() ==
        source_runtime.ordered_program.size());
    for (std::size_t i = 0;
         i < source_runtime.ordered_program.size(); ++i) {
        PC_CHECK(
            alternate_finish_runtime.ordered_program[i].action ==
            source_runtime.ordered_program[i].action);
        PC_CHECK(
            action_refinement_contract_signature(
                alternate_finish_runtime
                    .ordered_program[i]
                    .refinement) ==
            action_refinement_contract_signature(
                source_runtime.ordered_program[i].refinement));
    }
    PC_CHECK(
        action_refinement_contract_signature(
            alternate_finish_runtime.compatibility_refinement) ==
        action_refinement_contract_signature(
            source_runtime.compatibility_refinement));
    const std::uint32_t imported =
        destination.import_planner_operator(
            alternate_finish, true);
    PC_CHECK(imported == before_import);
    PC_CHECK(
        destination.operators().size() ==
        before_import + 1);
    PC_CHECK(
        destination.is_state_local_automatic_operator(
            imported));
    PC_CHECK(
        destination.registry().actions.at(
            destination.operators().at(imported)
                .constructive_finish_action).id ==
        "bench:s83_mod_7");
    PC_CHECK(
        destination.import_planner_operator(
            alternate_finish, true) == imported);
    PC_CHECK(
        destination.operators().size() ==
        before_import + 1);
    check_owned_byte_ledger(destination);

    bool incomplete_refused = false;
    try {
        PlannerOperator incomplete = alternate_finish;
        incomplete.constructive_finish_action_id.clear();
        (void)destination.import_planner_operator(
            incomplete, true);
    } catch (const std::invalid_argument&) {
        incomplete_refused = true;
    }
    PC_CHECK(incomplete_refused);

    bool unknown_refused = false;
    try {
        PlannerOperator unknown = alternate_finish;
        unknown.constructive_finish_action_id =
            "bench:not-in-registry";
        (void)destination.import_planner_operator(
            unknown, true);
    } catch (const std::invalid_argument&) {
        unknown_refused = true;
    }
    PC_CHECK(unknown_refused);

    bool invalid_runtime_role_refused = false;
    try {
        PlannerOperator invalid_runtime_role =
            source_planner;
        invalid_runtime_role.setup_action = source_chaos;
        invalid_runtime_role.setup_action_id = "chaos";
        (void)destination.import_planner_operator(
            invalid_runtime_role, true);
    } catch (const std::invalid_argument&) {
        invalid_runtime_role_refused = true;
    }
    PC_CHECK(invalid_runtime_role_refused);
}

} // namespace

void run_solver_automatic_veiled_tests() {
    run_automatic_veiled_program();
}

void run_solver_s8_3_tests() {
    run_automatic_veiled_program();
    run_temporary_blocker_price_flip();
    run_cannot_roll_price_flip();
    run_multimod_finish_price_flip();
    run_protected_price_flip();
    run_protected_producibility_filter();
    run_fracture_price_flip();
    run_incomplete_dependency_refusals();
    run_carrier_relative_renewal_templates();
    run_fixed_option_product_parent_refinement_trigger();
    run_planner_operator_import_authority();
}
