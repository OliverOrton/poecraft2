#include "tests.hpp"

#include "../src/json.hpp"
#include "../src/solver_internal.hpp"
#include "poecraft/bitset.h"
#include "poecraft/item_state.h"

#include <cmath>
#include <algorithm>
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
constexpr std::uint32_t kModCount = 15;

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

    const std::vector<std::uint32_t> groups = {
        10, 20, 21, 30, 31, 20, 10, 20, 21, 40, 41, 42, 50, 22,
        51};
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
        PC_SIDE_SUFFIX, PC_SIDE_SUFFIX, PC_SIDE_PREFIX};
    session->primary_group = groups;
    session->required_level.assign(kModCount, 1);
    session->group_offsets.resize(kModCount + 1);
    std::iota(session->group_offsets.begin(), session->group_offsets.end(), 0u);
    session->group_ids = groups;
    session->family_id = {
        100, 101, 102, 103, 104, 105, 100, 101, 108, 109, 110, 111,
        112, 113, 114};
    session->family_tier_index.assign(kModCount, 1);
    session->metamod_type.assign(kModCount, -1);
    session->metamod_type[kPrefixLock] = 4;
    session->metamod_type[kSuffixLock] = 5;
    session->metamod_type[kMultimod] = 1;
    session->special_kind.assign(kModCount, -1);
    session->flags.assign(kModCount, 0);
    for (const std::uint32_t mod : {
             kTargetBlocker, kBenchGoalPrefix, kBenchGoalSuffix,
             kBenchTemporary, kPrefixLock, kSuffixLock, kMultimod,
             kBenchNeutral, kBenchPrefixNeutral}) {
        session->flags[mod] = 1 << 1;
        session->bench_mod_ids.push_back(mod);
    }
    session->influence_code.assign(kModCount, -1);
    session->class_offsets.assign(kModCount + 1, 0);
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
    session->implicit_tag_masks.assign(1, {});
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
        auto& group_mask = session->group_masks[groups[mod]];
        if (group_mask.empty()) group_mask.assign(session->words, 0);
        pc_bitset_set(group_mask.data(), mod);
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

void admit_automatic(
    CalcContext& calc,
    const std::uint32_t state,
    const std::unordered_map<std::string, double>& prices) {
    AutomaticAdmissionLimits limits;
    limits.max_discovered_states = 100000;
    limits.max_state_action_rows = 100000;
    limits.max_transitions = 1000000;
    limits.max_reforge_work = 1000000;
    limits.max_solver_owned_bytes = 1073741824;
    limits.prices = &prices;
    calc.admit_state_local_automatic_candidates(state, limits);
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
    admit_automatic(calc, state, {
        {"exalt", 10.0},
        {"base", 100.0},
        {"scour", 1.0},
        {"bench:s83_mod_8", 2.0}});
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
    const OptionKernel& blocker_kernel = calc.option_kernel(state, blocker);
    PC_CHECK(blocker_kernel.legal);
    PC_CHECK(blocker_kernel.automatic.kernel_changed);
    PC_CHECK(blocker_kernel.automatic.cleanup_complete);
    PC_CHECK((blocker_kernel.automatic.kernel_change_mechanisms &
              kAutomaticGroupConflict) != 0);

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
    PC_CHECK(telemetry.find("\"primitive_families\":{") !=
             std::string::npos);
    PC_CHECK(telemetry.find("\"candidate_kind\":\"temporary_bench_blocker\"") !=
             std::string::npos);

    const std::string strategy = compile_policy_strategy_json(
        calc, low, "s8.3-automatic-temporary-blocker");
    PC_CHECK(strategy.find("remove_crafted_modifiers") != std::string::npos);
    run_compiled(session, strategy, prices(2.0), 64, 8301);
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

} // namespace

void run_solver_s8_3_tests() {
    run_temporary_blocker_price_flip();
    run_protected_price_flip();
    run_fracture_price_flip();
    run_incomplete_dependency_refusals();
    run_carrier_relative_renewal_templates();
}
