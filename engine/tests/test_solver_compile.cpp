#include "tests.hpp"

#include "../src/solver_internal.hpp"
#include "../src/solver_policy_refinement.hpp"
#include "poecraft/bitset.h"
#include "poecraft/item_state.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <memory>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

using namespace poecraft;
using namespace poecraft::solver;

namespace {

void report_compile_solve_issue(
        const char* label,
        const SolveResult& solved) {
    if (solved.converged) return;
    std::printf(
        "solver compile issue [%s]: available=%u termination=%u "
        "publication=%s evaluation=%s compatibility=%s "
        "states=%u/%u\n",
        label,
        solved.policy_available ? 1u : 0u,
        static_cast<unsigned>(solved.termination),
        solved.diagnostics.policy_publication_failure_reason.c_str(),
        solved.diagnostics.policy_evaluation_failure.c_str(),
        solved.diagnostics.policy_compatibility_reason.c_str(),
        solved.diagnostics.expanded_states,
        solved.diagnostics.discovered_states);
}

/*
 * The eight ordinary-mod weighted universe again, plus dedicated prefix and
 * suffix veiled placeholders and the identity data (mod keys, group keys,
 * base path) that compiled strategy conditions reference.
 */
std::shared_ptr<SessionImpl> make_compile_session() {
    auto data = std::make_shared<DataImpl>();
    data->mod_global_ids = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    data->spawn_offsets = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    data->spawn_tag_ids.assign(10, 0);
    data->spawn_weights = {
        100, 100, 100, 100, 100, 100, 100, 400, 100, 100};
    data->mod_gen_type_code.assign(10, 0);
    data->strings = {"",       "synthetic/base", "mod0", "mod1", "mod2",
                     "mod3",   "mod4",           "mod5", "mod6", "mod7",
                     "g10",    "g11",            "g12",  "g13",  "g20",
                     "g21",    "g22",            "mod8", "mod9",
                     "g30",    "g31"};
    data->base_count = 1;
    data->base_metadata_path_sid = {1};
    data->mod_key_sid = {2, 3, 4, 5, 6, 7, 8, 9, 17, 18};
    for (std::uint32_t mod = 0; mod < 10; ++mod) {
        data->mod_pos_by_key.emplace(
            data->strings[data->mod_key_sid[mod]], mod);
    }
    data->group_key_sids.assign(32, 0);
    const auto group_key = [&](std::uint32_t group, std::uint32_t sid) {
        data->group_key_sids[group] = sid;
        data->group_id_by_key.emplace(data->strings[sid], group);
    };
    group_key(10, 10);
    group_key(11, 11);
    group_key(12, 12);
    group_key(13, 13);
    group_key(20, 14);
    group_key(21, 15);
    group_key(22, 16);
    group_key(30, 19);
    group_key(31, 20);

    auto session = std::make_shared<SessionImpl>();
    session->data = data;
    session->base_index = 0;
    session->item_level = 1;
    session->mod_count = 10;
    session->words = pc_bitset_words(10);
    session->global_index = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    session->gen_type = {0, 0, 0, 0, 0, 1, 1, 1, 0, 1};
    session->primary_group = {10, 10, 10, 12, 13, 20, 21, 22, 30, 31};
    session->required_level.assign(10, 1);
    session->group_offsets = {0, 1, 2, 4, 5, 6, 7, 8, 9, 10, 11};
    session->group_ids = {10, 10, 10, 11, 12, 13, 20, 21, 22, 30, 31};
    session->family_id = {
        100, 100, 101, 102, 103, 104, 105, 106, 107, 108};
    session->family_tier_index.assign(10, 1);
    session->family_tier_index[1] = 2;
    session->metamod_type.assign(10, -1);
    session->special_kind.assign(10, -1);
    session->flags.assign(10, 0);
    session->influence_code.assign(10, -1);
    session->class_offsets = {0, 0, 0, 0, 1, 2, 4, 6, 7, 7, 7};
    session->class_tag_ids = {1, 2, 3, 6, 4, 6, 5};
    session->rare_affix_cap = 3;
    session->base_spawn_weight = {
        100, 100, 100, 100, 100, 100, 100, 400, 100, 100};
    session->base_gen_pct.assign(10, 100);
    session->base_roll_weight = session->base_spawn_weight;
    session->effective_base_tag_ids = {0};
    for (std::uint32_t mod = 0; mod < 10; ++mod) {
        session->session_id_by_global_id.emplace(mod, mod);
    }

    const std::size_t words = session->words;
    session->normal_random_roll_mask.assign(words, 0);
    session->positive_spawn_weight_mask.assign(words, 0);
    session->positive_base_weight_mask.assign(words, 0);
    session->prefix_mask.assign(words, 0);
    session->suffix_mask.assign(words, 0);
    session->unveiled_mask.assign(words, 0);
    session->unveiled_generic_mask.assign(words, 0);
    session->implicit_tag_masks.assign(7, {});
    session->group_masks.assign(32, {});
    session->influence_masks.assign(1, std::vector<std::uint64_t>(words, 0));
    for (std::uint32_t mod = 0; mod < 8; ++mod) {
        pc_bitset_set(session->normal_random_roll_mask.data(), mod);
        pc_bitset_set(session->positive_spawn_weight_mask.data(), mod);
        pc_bitset_set(session->positive_base_weight_mask.data(), mod);
        pc_bitset_set(session->influence_masks[0].data(), mod);
        pc_bitset_set((mod < 5 ? session->prefix_mask : session->suffix_mask)
                          .data(),
                      mod);
        const std::uint32_t begin = session->group_offsets[mod];
        const std::uint32_t end = session->group_offsets[mod + 1];
        for (std::uint32_t i = begin; i < end; ++i) {
            auto& mask = session->group_masks[session->group_ids[i]];
            if (mask.empty()) mask.assign(words, 0);
            pc_bitset_set(mask.data(), mod);
        }
        const std::uint32_t class_begin = session->class_offsets[mod];
        const std::uint32_t class_end = session->class_offsets[mod + 1];
        for (std::uint32_t i = class_begin; i < class_end; ++i) {
            auto& mask = session->implicit_tag_masks[
                session->class_tag_ids[i]];
            if (mask.empty()) mask.assign(words, 0);
            pc_bitset_set(mask.data(), mod);
        }
    }
    for (std::uint32_t mod = 0; mod < 8; ++mod) {
        pc_bitset_set(session->unveiled_mask.data(), mod);
        pc_bitset_set(session->unveiled_generic_mask.data(), mod);
    }
    session->veiled_prefix_mod_id = 8;
    session->veiled_suffix_mod_id = 9;
    session->eldritch_eligible = true;
    session->eldritch_searing_tier_mod_ids.resize(5);
    session->eldritch_eater_tier_mod_ids.resize(5);
    session->eldritch_searing_tier_mod_ids[1] = {0};
    session->eldritch_eater_tier_mod_ids[1] = {5};
    return session;
}

/* Compile the strategy JSON and run it through the native simulator. */
SimulationSummaryInternal run_compiled(
    std::shared_ptr<const SessionImpl> session,
    const std::string& strategy_json,
    const std::unordered_map<std::string, double>& prices,
    std::uint64_t runs,
    std::uint64_t seed) {
    auto strategy = compile_strategy_json(
        session, strategy_json.c_str(), strategy_json.size());
    auto economy = std::make_shared<EconomyImpl>();
    economy->id = "test";
    for (const auto& [key, value] : prices) {
        economy->prices.emplace(key, value);
    }
    SimulatorImpl simulator;
    simulator.session = session;
    simulator.strategy = strategy;
    simulator.economy = economy;
    simulator.action_counts.assign(strategy->nodes.size(), 0);

    SimulationOptionsInternal options;
    options.target_runs = runs;
    options.seed = seed;
    options.max_actions_per_run = 100000;
    run_simulator_chunk(simulator, options,
                        static_cast<std::uint32_t>(runs));
    for (const FailureSummaryInternal& failure :
         simulator.failure_summaries) {
        std::printf(
            "compiled strategy failure: node=%s reason=%d count=%llu detail=%s\n",
            failure.node_id.c_str(), failure.failure_reason,
            static_cast<unsigned long long>(failure.count),
            failure.detail.c_str());
    }
    return simulator.summary;
}

StrategyEvalResult evaluate_compiled(
    std::shared_ptr<const SessionImpl> session,
    const std::string& strategy_json,
    const std::unordered_map<std::string, double>& prices) {
    auto strategy = compile_strategy_json(
        session, strategy_json.c_str(), strategy_json.size());
    auto economy = std::make_shared<EconomyImpl>();
    economy->id = "test";
    economy->prices = prices;
    StrategyEvalOptions options;
    options.economy = economy;
    return evaluate_strategy(*strategy, options);
}

/* The compiler follows the engine-owned observed-choice contract carried by
 * the admitted exact mechanic descriptor. */
void run_future_observed_choice_compile_test() {
    auto session = make_compile_session();
    ActionRegistry registry = build_action_registry(*session);
    const std::uint32_t unveil =
        registry.index_by_id.at("unveil");
    ActionDescriptor future = registry.actions.at(unveil);
    future.id = "test:future_observed_modifier_offer";
    future.display_name = "future observed modifier offer";
    canonicalize_and_validate_action_refinement_contract(
        *session, future);
    PC_CHECK(action_observes_modifier_offer(future));
    const std::uint32_t future_index =
        static_cast<std::uint32_t>(registry.actions.size());
    registry.index_by_id.emplace(future.id, future_index);
    registry.actions.push_back(std::move(future));

    GoalSpec goal;
    goal.rarity = PC_RARITY_RARE;
    GoalSlot wanted;
    wanted.family_id = session->family_id.at(5);
    wanted.min_tier = 1;
    goal.slots.push_back(wanted);
    CalcContext calc(
        session, goal, registry, {future_index}, false, true, true);
    pc_item_state start;
    pc_item_clear(&start);
    start.rarity = PC_RARITY_RARE;
    const std::uint32_t start_state = calc.intern_item(start);
    PC_CHECK(!calc.is_goal_state(calc.state(start_state)));

    SolveResult authored;
    authored.converged = true;
    authored.policy_available = true;
    authored.policy_status = SolvePolicyStatus::Exact;
    authored.termination = SolveTermination::ExactClosed;
    authored.start_state = start_state;
    authored.has_exact_start_item = true;
    authored.exact_start_item = start;
    const std::size_t state_count = calc.state_count();
    authored.values.assign(state_count, 1.0);
    authored.policy.assign(state_count, PolicyOperatorRef{});
    authored.expanded.assign(state_count, 1);
    authored.goal_states.assign(state_count, 0);
    authored.policy_reachable.assign(state_count, 0);
    authored.unveil_preferences.resize(state_count);
    authored.option_unveil_preferences.resize(state_count);
    authored.policy[start_state] = {
        PlannerOperatorKind::Primitive, future_index};
    authored.policy_reachable[start_state] = 1;
    authored.unveil_preferences[start_state] = {0, 3};

    const std::string strategy = compile_policy_strategy_json(
        calc, authored, "future-observed-choice");
    PC_CHECK(strategy.find("has_unveil_option") != std::string::npos);
    PC_CHECK(strategy.find("\"type\":\"unveil\"") !=
             std::string::npos);
    PC_CHECK(strategy.find("\"mod_key\":\"mod0\"") !=
             std::string::npos);
    PC_CHECK(strategy.find("\"mod_key\":\"mod3\"") !=
             std::string::npos);
}

struct StructuredRouteFixture {
    std::shared_ptr<SessionImpl> session;
    ActionRegistry registry;
    std::unique_ptr<CalcContext> calc;
    SolveResult solved;
    refinement::RefinedPolicyCompileRouting routing;
    std::uint32_t left_state = kNoId;
    std::uint32_t right_state = kNoId;
    std::uint32_t goal_state = kNoId;
    std::uint32_t scour = kNoId;
    std::uint32_t chaos = kNoId;
    std::uint32_t renewal = kNoId;
};

pc_item_state compile_item(
        const SessionImpl& session,
        const std::uint32_t mod,
        const std::int8_t side) {
    pc_item_state item;
    pc_item_clear(&item);
    item.rarity = PC_RARITY_RARE;
    PC_CHECK(
        pc_item_add_mod(
            &item, side, mod, session.primary_group.at(mod), 0,
            nullptr) == PC_RESULT_OK);
    return item;
}

refinement::ObservationRequirement exclusion_requirement() {
    refinement::ObservationRequirement requirement;
    RefinementAffixObservation observation;
    observation.features = refinement_feature(
        RefinementFeature::ModifierExclusionSignature);
    requirement.affix_observations.push_back(observation);
    return refinement::canonical_observation_requirement(
        std::move(requirement));
}

refinement::FeatureSignature observed_signature(
        const CalcContext& calc,
        const std::uint32_t state,
        const refinement::ObservationRequirement& requirement) {
    const refinement::AbstractFeatureExtraction extraction =
        refinement::extract_strict_abstract_features(
            calc.session(), calc.layout(), calc.state(state),
            requirement);
    PC_CHECK(extraction.complete());
    return refinement::observe_features(
        extraction.features, requirement);
}

refinement::SelectedAction compile_selected_operator(
        const CalcContext& calc,
        const std::uint32_t operator_index) {
    refinement::SelectedAction selected;
    selected.action_id = operator_index;
    selected.semantic_key = {
        0x7465737464656331ull}; /* "testdec1" */
    const PlannerOperator& planner =
        calc.operators().at(operator_index);
    const std::vector<std::uint64_t> operator_key =
        planner_operator_semantic_key(planner);
    selected.semantic_key.insert(
        selected.semantic_key.end(),
        operator_key.begin(), operator_key.end());
    if (planner.kind == PlannerOperatorKind::Primitive) {
        selected.contract =
            calc.registry()
                .actions.at(planner.primitive_action)
                .refinement;
    }
    return selected;
}

StructuredRouteFixture make_structured_route_fixture() {
    StructuredRouteFixture fixture;
    fixture.session = make_compile_session();
    fixture.registry = build_action_registry(*fixture.session);
    fixture.scour = fixture.registry.index_by_id.at("scour");
    fixture.chaos = fixture.registry.index_by_id.at("chaos");
    const std::uint32_t restart =
        fixture.registry.index_by_id.at("restart");

    GoalSpec goal;
    GoalSlot wanted;
    wanted.family_id = fixture.session->family_id.at(7);
    wanted.min_tier = 1;
    goal.slots.push_back(wanted);
    goal.rarity = PC_RARITY_RARE;
    FixedOptionSpec renewal;
    renewal.kind = FixedOptionKind::Renewal;
    renewal.program_action_ids = {"chaos"};
    renewal.exit_goal_slots = {0};
    renewal.exit_min_satisfied = 1;
    goal.fixed_options.push_back(renewal);
    fixture.calc = std::make_unique<CalcContext>(
        fixture.session, goal, fixture.registry,
        std::vector<std::uint32_t>{
            fixture.scour, fixture.chaos, restart},
        false, true, true);
    fixture.renewal =
        static_cast<std::uint32_t>(
            fixture.registry.actions.size());
    PC_CHECK(
        fixture.renewal < fixture.calc->operators().size());
    PC_CHECK(
        fixture.calc->operators()[fixture.renewal].kind ==
        PlannerOperatorKind::FixedOption);

    const pc_item_state left =
        compile_item(*fixture.session, 0, PC_SIDE_PREFIX);
    const pc_item_state right =
        compile_item(*fixture.session, 3, PC_SIDE_PREFIX);
    const pc_item_state goal_item =
        compile_item(*fixture.session, 7, PC_SIDE_SUFFIX);
    fixture.left_state = fixture.calc->intern_item(left);
    fixture.right_state = fixture.calc->intern_item(right);
    fixture.goal_state = fixture.calc->intern_item(goal_item);
    PC_CHECK(fixture.left_state != fixture.right_state);
    PC_CHECK(fixture.left_state != fixture.goal_state);
    PC_CHECK(fixture.right_state != fixture.goal_state);

    const std::size_t state_count = fixture.calc->state_count();
    fixture.solved.start_state = fixture.left_state;
    fixture.solved.has_exact_start_item = true;
    fixture.solved.exact_start_item = left;
    fixture.solved.policy_available = true;
    fixture.solved.policy_status = SolvePolicyStatus::Exact;
    fixture.solved.values.assign(state_count, 1.0);
    fixture.solved.policy.assign(state_count, PolicyOperatorRef{});
    fixture.solved.expanded.assign(state_count, 1);
    fixture.solved.goal_states.assign(state_count, 0);
    fixture.solved.policy_reachable.assign(state_count, 0);
    fixture.solved.behavioral_representative_by_state.resize(
        state_count);
    for (std::uint32_t state = 0; state < state_count; ++state) {
        fixture.solved.behavioral_representative_by_state[state] =
            state;
    }
    for (const std::uint32_t state :
         {fixture.left_state, fixture.right_state}) {
        fixture.solved.policy[state] =
            PolicyOperatorRef{fixture.scour};
        fixture.solved.policy_reachable[state] = 1;
    }
    fixture.solved.goal_states[fixture.goal_state] = 1;
    fixture.solved.policy_reachable[fixture.goal_state] = 1;

    const refinement::StableKey working_parent_key =
        exact_abstract_state_key(
            fixture.calc->state(fixture.left_state), 0);
    const refinement::StableKey terminal_parent_key =
        exact_abstract_state_key(
            fixture.calc->state(fixture.goal_state), 0);

    const refinement::ObservationRequirement requirement =
        exclusion_requirement();
    fixture.routing.classes.resize(3);
    const auto working_class =
        [&](const std::uint32_t class_id,
            const std::uint32_t state) {
            refinement::RefinedPolicyCompileClass policy_class;
            policy_class.class_id = class_id;
            policy_class.coarse_state = 0;
            policy_class.coarse_state_key = working_parent_key;
            policy_class.representative_state = state;
            policy_class.strict_members = {state};
            policy_class.required_observations = requirement;
            policy_class.observation_signature =
                observed_signature(
                    *fixture.calc, state, requirement);
            policy_class.selected_action =
                compile_selected_operator(
                    *fixture.calc, fixture.scour);
            policy_class.action_cost = 1.0;
            policy_class.transitions = {{2, 1.0}};
            return policy_class;
        };
    fixture.routing.classes[0] =
        working_class(0, fixture.left_state);
    fixture.routing.classes[1] =
        working_class(1, fixture.right_state);
    fixture.routing.classes[2].class_id = 2;
    fixture.routing.classes[2].coarse_state = 1;
    fixture.routing.classes[2].coarse_state_key = terminal_parent_key;
    fixture.routing.classes[2].representative_state =
        fixture.goal_state;
    fixture.routing.classes[2].strict_members = {
        fixture.goal_state};
    fixture.routing.classes[2].terminal = true;
    fixture.routing.parents = {
        {0, working_parent_key,
         fixture.calc->state(fixture.left_state)},
        {1, terminal_parent_key,
         fixture.calc->state(fixture.goal_state)}};
    fixture.routing.parent_layout = &fixture.calc->layout();
    return fixture;
}

StructuredRouteFixture make_structured_fixed_route_fixture() {
    StructuredRouteFixture fixture =
        make_structured_route_fixture();
    const OptionKernel& left_kernel =
        fixture.calc->option_kernel(
            fixture.left_state, fixture.renewal);
    const OptionKernel& right_kernel =
        fixture.calc->option_kernel(
            fixture.right_state, fixture.renewal);
    PC_CHECK(left_kernel.supported && left_kernel.legal);
    PC_CHECK(right_kernel.supported && right_kernel.legal);

    const std::size_t state_count =
        fixture.calc->state_count();
    const std::size_t old_state_count =
        fixture.solved.values.size();
    fixture.solved.values.resize(
        state_count,
        std::numeric_limits<double>::infinity());
    fixture.solved.policy.resize(state_count);
    fixture.solved.expanded.resize(state_count, 0);
    fixture.solved.goal_states.resize(state_count, 0);
    fixture.solved.policy_reachable.resize(state_count, 0);
    fixture.solved.unveil_preferences.resize(state_count);
    fixture.solved.option_unveil_preferences.resize(
        state_count);
    fixture.solved.behavioral_representative_by_state.resize(
        state_count);
    for (std::uint32_t state =
             static_cast<std::uint32_t>(old_state_count);
         state < state_count; ++state) {
        fixture.solved.behavioral_representative_by_state[state] =
            state;
    }
    for (const std::uint32_t state :
         {fixture.left_state, fixture.right_state}) {
        fixture.solved.policy[state] = PolicyOperatorRef{
            PlannerOperatorKind::FixedOption,
            fixture.renewal};
        fixture.solved.values[state] = 1.0;
        fixture.solved.expanded[state] = 1;
    }
    fixture.solved.policy_reachable[fixture.left_state] = 1;
    fixture.solved.policy_reachable[fixture.right_state] = 0;
    fixture.solved.behavioral_representative_by_state[
        fixture.right_state] = fixture.left_state;

    refinement::RefinedPolicyCompileClass working;
    working.class_id = 0;
    working.coarse_state = 0;
    working.coarse_state_key =
        fixture.routing.parents[0].coarse_state_key;
    working.representative_state = fixture.left_state;
    working.strict_members = {
        fixture.left_state, fixture.right_state};
    working.selected_action =
        compile_selected_operator(
            *fixture.calc, fixture.renewal);
    working.action_cost = 1.0;
    working.transitions = {{1, 1.0}};

    refinement::RefinedPolicyCompileClass terminal;
    terminal.class_id = 1;
    terminal.coarse_state = 1;
    terminal.coarse_state_key =
        fixture.routing.parents[1].coarse_state_key;
    terminal.representative_state = fixture.goal_state;
    terminal.strict_members = {fixture.goal_state};
    terminal.terminal = true;

    fixture.routing.classes = {
        std::move(working), std::move(terminal)};
    return fixture;
}

template <typename Mutator>
void expect_structured_route_refusal(
        Mutator mutate,
        const std::string& expected) {
    StructuredRouteFixture fixture =
        make_structured_route_fixture();
    mutate(fixture);
    bool refused = false;
    try {
        (void)compile_policy_strategy_json(
            *fixture.calc, fixture.solved,
            "invalid structured observation route", nullptr,
            std::numeric_limits<std::uint64_t>::max(),
            &fixture.routing);
    } catch (const std::exception& error) {
        refused =
            std::string(error.what()).find(expected) !=
            std::string::npos;
        if (!refused) {
            std::printf(
                "structured route refusal mismatch: expected=%s "
                "actual=%s\n",
                expected.c_str(), error.what());
        }
    }
    if (!refused) {
        std::printf(
            "structured route refusal was accepted: expected=%s\n",
            expected.c_str());
    }
    PC_CHECK(refused);
}

void run_structured_observation_route_tests() {
    {
        StructuredRouteFixture fixture =
            make_structured_route_fixture();
        fixture.solved.policy[fixture.right_state] =
            PolicyOperatorRef{fixture.chaos};
        fixture.routing.classes[1].selected_action =
            compile_selected_operator(
                *fixture.calc, fixture.chaos);
        PolicyCompilationTelemetry telemetry;
        const std::string strategy =
            compile_policy_strategy_json(
                *fixture.calc, fixture.solved,
                "structured observation route", &telemetry,
                std::numeric_limits<std::uint64_t>::max(),
                &fixture.routing);
        PC_CHECK(
            strategy.find("\"type\":\"observation_signature\"") !=
            std::string::npos);
        PC_CHECK(
            strategy.find("\"version\":1") != std::string::npos);
        PC_CHECK(
            strategy.find("\"id\":\"refined_parent_0\"") !=
            std::string::npos);
        PC_CHECK(
            strategy.find("\"id\":\"refined_parent_1\"") ==
            std::string::npos);
        PC_CHECK(
            telemetry.peak_owned_bytes >=
            strategy.capacity() + 1);
        PC_CHECK(
            telemetry.complete_peak_owned_bytes ==
            telemetry.peak_owned_bytes);
        PC_CHECK(
            telemetry.complete_peak_owned_bytes >=
            telemetry.previously_accounted_peak_owned_bytes);
        PC_CHECK(telemetry.total_condition_bytes > 0);
        PC_CHECK(telemetry.max_condition_bytes > 0);
        PC_CHECK(telemetry.behavioral_classes > 0);
        PC_CHECK(
            compile_strategy_json(
                fixture.session, strategy.data(), strategy.size()) !=
            nullptr);

        /* Gate 4 enforces the complete audit. A limit equal to the historic
         * partial estimate must reject when complete ownership is higher. */
        PolicyCompilationTelemetry corrected_cap;
        bool corrected_capped = false;
        try {
            (void)compile_policy_strategy_json(
                *fixture.calc, fixture.solved,
                "structured observation route", &corrected_cap,
                std::numeric_limits<std::uint64_t>::max(),
                &fixture.routing,
                telemetry.previously_accounted_peak_owned_bytes);
        } catch (const SolverResourceLimit& error) {
            corrected_capped =
                error.cap_name() == "max_solver_owned_bytes";
        }
        PC_CHECK(corrected_capped);
        PC_CHECK(corrected_cap.cap_hit == "max_solver_owned_bytes");
        PC_CHECK(
            corrected_cap.complete_peak_owned_bytes >
            telemetry.previously_accounted_peak_owned_bytes);

        StructuredRouteFixture remapped =
            make_structured_route_fixture();
        remapped.solved.policy[remapped.right_state] =
            PolicyOperatorRef{remapped.chaos};
        remapped.routing.classes[1].selected_action =
            compile_selected_operator(
                *remapped.calc, remapped.chaos);
        for (refinement::RefinedPolicyCompileClass& policy_class :
             remapped.routing.classes) {
            policy_class.coarse_state =
                policy_class.terminal ? 37 : 91;
        }
        remapped.routing.parents[0].coarse_state = 91;
        remapped.routing.parents[1].coarse_state = 37;
        const std::string remapped_strategy =
            compile_policy_strategy_json(
                *remapped.calc, remapped.solved,
                "structured observation route", nullptr,
                std::numeric_limits<std::uint64_t>::max(),
                &remapped.routing);
        PC_CHECK(remapped_strategy == strategy);
    }

    {
        StructuredRouteFixture fixture =
            make_structured_route_fixture();
        PolicyCompilationTelemetry telemetry;
        bool capped = false;
        try {
            (void)compile_policy_strategy_json(
                *fixture.calc, fixture.solved,
                "structured condition memory cap", &telemetry,
                std::numeric_limits<std::uint64_t>::max(),
                &fixture.routing, 1);
        } catch (const SolverResourceLimit& error) {
            capped =
                error.cap_name() == "max_solver_owned_bytes";
        }
        PC_CHECK(capped);
        PC_CHECK(
            telemetry.cap_hit == "max_solver_owned_bytes");
        PC_CHECK(telemetry.peak_owned_bytes > 1);
    }

    /* Raw carrier identity is irrelevant when the serialized semantic
     * observation and selected operation are identical. Such duplicate
     * classes are deliberately accepted, even when their local value or
     * projected class row differs: the router re-observes after the action. */
    {
        StructuredRouteFixture fixture =
            make_structured_route_fixture();
        refinement::ObservationRequirement none;
        for (std::uint32_t policy_class = 0;
             policy_class < 2; ++policy_class) {
            fixture.routing.classes[policy_class]
                .required_observations = none;
            fixture.routing.classes[policy_class]
                .observation_signature.clear();
        }
        fixture.routing.classes[1].action_cost = 9.0;
        fixture.routing.classes[1].transitions = {{2, 0.25}};
        const std::string strategy =
            compile_policy_strategy_json(
                *fixture.calc, fixture.solved,
                "equivalent duplicate structured routes", nullptr,
                std::numeric_limits<std::uint64_t>::max(),
                &fixture.routing);
        PC_CHECK(
            strategy.find("\"type\":\"observation_signature\"") ==
            std::string::npos);
    }

    expect_structured_route_refusal(
        [](StructuredRouteFixture& fixture) {
            fixture.routing.classes[1].class_id = 0;
        },
        "sidecar is not canonical");
    expect_structured_route_refusal(
        [](StructuredRouteFixture& fixture) {
            std::reverse(
                fixture.routing.classes[0].strict_members.begin(),
                fixture.routing.classes[0].strict_members.end());
            fixture.routing.classes[0].strict_members.push_back(
                fixture.left_state);
        },
        "sidecar is not canonical");
    expect_structured_route_refusal(
        [](StructuredRouteFixture& fixture) {
            fixture.routing.classes[0].coarse_state_key.clear();
            fixture.routing.classes[1].coarse_state_key.clear();
        },
        "lost its canonical coarse parent");
    expect_structured_route_refusal(
        [](StructuredRouteFixture& fixture) {
            fixture.routing.classes[1].coarse_state_key = {9};
        },
        "lost its canonical coarse parent");
    expect_structured_route_refusal(
        [](StructuredRouteFixture& fixture) {
            fixture.routing.classes[1].coarse_state = 2;
        },
        "lost its canonical coarse parent");
    {
        StructuredRouteFixture fixture =
            make_structured_route_fixture();
        AbstractState retry =
            fixture.calc->state(fixture.left_state);
        retry.goal_progress_retry_basin = 1;
        const std::uint32_t retry_state =
            fixture.calc->intern_state(retry);
        const std::size_t state_count = fixture.calc->state_count();
        fixture.solved.values.resize(state_count, 1.0);
        fixture.solved.policy.resize(state_count);
        fixture.solved.expanded.resize(state_count, 0);
        fixture.solved.goal_states.resize(state_count, 0);
        fixture.solved.policy_reachable.resize(state_count, 0);
        fixture.solved.behavioral_representative_by_state.resize(
            state_count, kNoId);
        fixture.solved.policy[retry_state] =
            PolicyOperatorRef{fixture.scour};
        fixture.solved.expanded[retry_state] = 1;
        fixture.solved.behavioral_representative_by_state[retry_state] =
            fixture.left_state;
        fixture.routing.classes[0].strict_members.push_back(
            retry_state);
        std::sort(
            fixture.routing.classes[0].strict_members.begin(),
            fixture.routing.classes[0].strict_members.end());
        fixture.solved.policy[fixture.right_state] =
            PolicyOperatorRef{fixture.chaos};
        fixture.routing.classes[1].selected_action =
            compile_selected_operator(*fixture.calc, fixture.chaos);
        const std::string strategy = compile_policy_strategy_json(
            *fixture.calc, fixture.solved,
            "structured route with hidden retry-basin member", nullptr,
            std::numeric_limits<std::uint64_t>::max(),
            &fixture.routing);
        PC_CHECK(!strategy.empty());
    }

    /* Identical/overlapping observations may not choose a different
     * executable semantic operation. */
    expect_structured_route_refusal(
        [](StructuredRouteFixture& fixture) {
            refinement::ObservationRequirement none;
            for (std::uint32_t policy_class = 0;
                 policy_class < 2; ++policy_class) {
                fixture.routing.classes[policy_class]
                    .required_observations = none;
                fixture.routing.classes[policy_class]
                    .observation_signature.clear();
            }
            fixture.solved.policy[fixture.right_state] =
                PolicyOperatorRef{fixture.chaos};
            fixture.routing.classes[1].selected_action =
                compile_selected_operator(
                    *fixture.calc, fixture.chaos);
        },
        "indistinguishable_refined_policy_actions");

    /* The state-local semantic decision key is part of executable identity,
     * even when the imported operator itself is unchanged. */
    expect_structured_route_refusal(
        [](StructuredRouteFixture& fixture) {
            refinement::ObservationRequirement none;
            for (std::uint32_t policy_class = 0;
                 policy_class < 2; ++policy_class) {
                fixture.routing.classes[policy_class]
                    .required_observations = none;
                fixture.routing.classes[policy_class]
                    .observation_signature.clear();
            }
            fixture.routing.classes[1]
                .selected_action->semantic_key.push_back(999);
        },
        "indistinguishable_refined_policy_actions");

    /* One broad predicate and one exact predicate overlap on the left
     * carrier. The overlap is safe only while both select the same action. */
    {
        StructuredRouteFixture fixture =
            make_structured_route_fixture();
        fixture.routing.classes[0].required_observations = {};
        fixture.routing.classes[0].observation_signature.clear();
        const std::string strategy =
            compile_policy_strategy_json(
                *fixture.calc, fixture.solved,
                "overlapping equivalent structured routes", nullptr,
                std::numeric_limits<std::uint64_t>::max(),
                &fixture.routing);
        PC_CHECK(!strategy.empty());
    }
    expect_structured_route_refusal(
        [](StructuredRouteFixture& fixture) {
            fixture.routing.classes[0].required_observations = {};
            fixture.routing.classes[0]
                .observation_signature.clear();
            fixture.solved.policy[fixture.right_state] =
                PolicyOperatorRef{fixture.chaos};
            fixture.routing.classes[1].selected_action =
                compile_selected_operator(
                    *fixture.calc, fixture.chaos);
        },
        "overlapping_refined_policy_actions");

    /* A represented strict carrier must still match at least one route. */
    expect_structured_route_refusal(
        [](StructuredRouteFixture& fixture) {
            fixture.routing.classes[0].observation_signature =
                fixture.routing.classes[1].observation_signature;
            fixture.solved.policy[fixture.right_state] =
                PolicyOperatorRef{fixture.chaos};
            fixture.routing.classes[1].selected_action =
                compile_selected_operator(
                    *fixture.calc, fixture.chaos);
        },
        "indistinguishable_refined_policy_actions");

    expect_structured_route_refusal(
        [](StructuredRouteFixture& fixture) {
            fixture.routing.classes[1].strict_members.insert(
                fixture.routing.classes[1].strict_members.begin(),
                fixture.left_state);
        },
        "overlaps strict member classes");

    /* A fixed operator is routed by its imported strict operator identity,
     * while every hidden class member independently proves the exact retry
     * recipe emitted for the representative. */
    {
        StructuredRouteFixture fixture =
            make_structured_fixed_route_fixture();
        const std::string strategy =
            compile_policy_strategy_json(
                *fixture.calc, fixture.solved,
                "structured fixed renewal route", nullptr,
                std::numeric_limits<std::uint64_t>::max(),
                &fixture.routing);
        PC_CHECK(
            strategy.find("_retry") != std::string::npos);
        PC_CHECK(
            strategy.find("\"type\":\"chaos\"") !=
            std::string::npos);
    }
    {
        StructuredRouteFixture fixture =
            make_structured_fixed_route_fixture();
        fixture.solved.policy[fixture.right_state] =
            PolicyOperatorRef{fixture.chaos};
        bool refused = false;
        try {
            (void)compile_policy_strategy_json(
                *fixture.calc, fixture.solved,
                "fixed operator mismatch", nullptr,
                std::numeric_limits<std::uint64_t>::max(),
                &fixture.routing);
        } catch (const std::exception& error) {
            refused =
                std::string(error.what()).find(
                    "member operator disagrees") !=
                std::string::npos;
        }
        PC_CHECK(refused);
    }
    {
        StructuredRouteFixture fixture =
            make_structured_fixed_route_fixture();
        ObservedUnveilPreference unexpected;
        unexpected.observation_state =
            fixture.right_state;
        fixture.solved.option_unveil_preferences[
            fixture.right_state].push_back(
                std::move(unexpected));
        bool refused = false;
        try {
            (void)compile_policy_strategy_json(
                *fixture.calc, fixture.solved,
                "fixed choice-sidecar mismatch", nullptr,
                std::numeric_limits<std::uint64_t>::max(),
                &fixture.routing);
        } catch (const std::exception& error) {
            refused =
                std::string(error.what()).find(
                    "unexpected choice sidecar") !=
                std::string::npos;
        }
        PC_CHECK(refused);
    }
}

void run_synthetic_gate() {
    auto session = make_compile_session();
    ActionRegistry registry = build_action_registry(*session);
    GoalSpec goal;
    GoalSlot slot;
    slot.family_id = 100;
    slot.min_tier = 1;
    goal.slots.push_back(slot);
    goal.rarity = PC_RARITY_MAGIC;
    const std::uint32_t transmute = registry.index_by_id.at("transmute");
    const std::uint32_t alteration = registry.index_by_id.at("alteration");
    const std::uint32_t restart = registry.index_by_id.at("restart");
    CalcContext calc(session, goal, registry,
                     {transmute, alteration, restart});

    const double p =
        0.5 * (100.0 / 1100.0) +
        0.5 * (100.0 / 1100.0 + (600.0 / 1100.0) * (100.0 / 500.0));

    pc_item_state start;
    pc_item_clear(&start);

    /* Alt-spam policy: solve -> compile -> simulate must reproduce
     * V(start) = 1/p empirically. */
    {
        const std::unordered_map<std::string, double> prices{
            {"transmute", 1.0}, {"alteration", 1.0}, {"base", 10.0}};
        const SolveResult solved = solve(calc, start, prices);
        PC_CHECK(solved.converged);
        PolicyCompilationTelemetry compilation;
        const std::string json = compile_policy_strategy_json(
            calc, solved, "alt-spam", &compilation);
        PC_CHECK(!solved.options.goal_progress_gated_reforges);
        PC_CHECK(json.find(
                     "\"solver_policy_scope\":\"unrestricted\"") !=
                 std::string::npos);
        PC_CHECK(json.find("\"type\":\"restart\"") == std::string::npos);
        PC_CHECK(json.find("\"expected_cost\":") != std::string::npos);
        PC_CHECK(compilation.working_states > 0);
        PC_CHECK(compilation.behavioral_classes >=
                 compilation.policy_regions);
        PC_CHECK(compilation.nodes > 0);
        PC_CHECK(compilation.edges > compilation.nodes);
        PC_CHECK(json.find("\"type\":\"transmute\"") !=
                 std::string::npos);
        PC_CHECK(json.find("\"type\":\"alteration\"") !=
                 std::string::npos);
        const std::string route_source =
            json.find("\"id\":\"policy_route_root\"") !=
                    std::string::npos
                ? "policy_route_root"
                : "router";
        PC_CHECK(json.find(
                     "\"from\":\"" + route_source +
                     "\",\"to\":\"offpolicy\"") !=
                 std::string::npos);
        std::size_t region_route_count = 0;
        std::size_t route_at = 0;
        while ((route_at = json.find(
                    "\"from\":\"" + route_source +
                        "\",\"to\":\"s",
                    route_at)) !=
               std::string::npos) {
            ++region_route_count;
            ++route_at;
        }
        PC_CHECK(region_route_count == compilation.policy_regions);
        PC_CHECK(compilation.strategy_json_bytes == json.size());
        PC_CHECK(
            compilation.complete_peak_owned_bytes ==
            compilation.peak_owned_bytes);
        PC_CHECK(
            compilation.complete_peak_owned_bytes >=
            compilation.previously_accounted_peak_owned_bytes);
        PC_CHECK(compilation.total_condition_bytes > 0);
        PC_CHECK(compilation.max_condition_bytes > 0);

        const double expected = solved.values[solved.start_state];
        const StrategyEvalResult exact =
            evaluate_compiled(session, json, prices);
        PC_CHECK(exact.converged);
        PC_CHECK(exact.cost_complete);
        PC_CHECK(exact.success_probability > 1.0 - 1e-9);
        PC_CHECK(exact.failure_probability < 1e-12);
        PC_CHECK(exact.action_not_applied_probability < 1e-12);
        PC_CHECK(exact.no_matching_edge_probability < 1e-12);
        PC_CHECK(exact.unresolved_probability < 1e-12);
        PC_CHECK(std::fabs(
                     exact.total_expected_cost - expected) < 1e-9);
        const SimulationSummaryInternal summary =
            run_compiled(session, json, prices, 10000, 42);
        PC_CHECK(summary.completed_runs == 10000);
        PC_CHECK(summary.success_count == summary.completed_runs);
        PC_CHECK(summary.missing_price_run_count == 0);
        const double mean =
            summary.known_total_cost /
            static_cast<double>(summary.completed_runs);
        std::printf("solver compile alt-spam: V=%.4f empirical=%.4f\n",
                    expected, mean);
        PC_CHECK(std::fabs(mean - expected) < 0.15);
        PC_CHECK(std::fabs(expected - 1.0 / p) < 1e-6);

        /* The strict-state oracle takes the exact decision-DAG compiler path
         * rather than the completed behavioral-quotient predicates. It must
         * remain executable and preserve the same start value. */
        SolveOptions strict_options;
        strict_options.full_evidence = true;
        strict_options.strict_states = true;
        const SolveResult strict = solve(calc, start, prices, strict_options);
        PC_CHECK(strict.converged);
        PC_CHECK(strict.behavioral_representative_by_state.empty());
        PC_CHECK(std::fabs(
                     strict.values[strict.start_state] - expected) < 1e-9);
        PolicyCompilationTelemetry strict_compilation;
        const std::string strict_json = compile_policy_strategy_json(
            calc, strict, "alt-spam-strict", &strict_compilation);
        PC_CHECK(strict_json.find("\"id\":\"policy_route_") !=
                 std::string::npos);
        PC_CHECK(strict_compilation.policy_regions > 0);
        PC_CHECK(strict_compilation.nodes <= strict.options.max_compiled_nodes);
        std::printf(
            "solver compile alt-spam strict: %llu regions, %llu condition "
            "bytes, %llu JSON bytes\n",
            static_cast<unsigned long long>(
                strict_compilation.policy_regions),
            static_cast<unsigned long long>(
                strict_compilation.total_condition_bytes),
            static_cast<unsigned long long>(
                strict_compilation.strategy_json_bytes));
        auto strict_strategy = compile_strategy_json(
            session, strict_json.data(), strict_json.size());
        PC_CHECK(strict_strategy != nullptr);
        const StrategyEvalResult strict_exact =
            evaluate_compiled(session, strict_json, prices);
        PC_CHECK(strict_exact.converged);
        PC_CHECK(strict_exact.cost_complete);
        PC_CHECK(strict_exact.success_probability > 1.0 - 1e-9);
        PC_CHECK(strict_exact.failure_probability < 1e-12);
        PC_CHECK(strict_exact.action_not_applied_probability < 1e-12);
        PC_CHECK(strict_exact.no_matching_edge_probability < 1e-12);
        PC_CHECK(strict_exact.unresolved_probability < 1e-12);
        PC_CHECK(std::fabs(
                     strict_exact.total_expected_cost - expected) < 1e-9);
        const SimulationSummaryInternal strict_summary =
            run_compiled(session, strict_json, prices, 10000, 420042);
        PC_CHECK(strict_summary.completed_runs == 10000);
        PC_CHECK(strict_summary.success_count == 10000);
        PC_CHECK(strict_summary.failure_count == 0);
        PC_CHECK(strict_summary.no_matching_edge_count == 0);
    }

    /* Price flip: the compiled strategy must include restart operations
     * and still land on V(start) = (2-p)/p. */
    {
        const std::unordered_map<std::string, double> prices{
            {"transmute", 1.0}, {"alteration", 100.0}, {"base", 1.0}};
        const SolveResult solved = solve(calc, start, prices);
        PC_CHECK(solved.converged);
        const std::string json =
            compile_policy_strategy_json(calc, solved, "restart-heavy");
        PC_CHECK(json.find("\"type\":\"restart\"") != std::string::npos);

        const SimulationSummaryInternal summary =
            run_compiled(session, json, prices, 10000, 4242);
        PC_CHECK(summary.completed_runs == 10000);
        PC_CHECK(summary.success_count == summary.completed_runs);
        const double mean =
            summary.known_total_cost /
            static_cast<double>(summary.completed_runs);
        const double expected = solved.values[solved.start_state];
        std::printf("solver compile restart: V=%.4f empirical=%.4f\n",
                    expected, mean);
        PC_CHECK(std::fabs(mean - expected) < 0.4);
        PC_CHECK(std::fabs(expected - (2.0 - p) / p) < 1e-6);
    }

    /* Flagged states compile to exact item-flag guards. A corrupted start
     * must route through restart and still verify against V(start). */
    {
        const std::unordered_map<std::string, double> prices{
            {"transmute", 1.0}, {"alteration", 1.0}, {"base", 10.0}};
        pc_item_state corrupted = start;
        corrupted.item_flags = PC_ITEM_CORRUPTED;
        const SolveResult solved = solve(calc, corrupted, prices);
        PC_CHECK(solved.converged);
        const std::string json =
            compile_policy_strategy_json(calc, solved, "flagged restart");
        PC_CHECK(json.find("\"type\":\"item_flag\",\"flag\":\"corrupted\"") !=
                 std::string::npos);
        const SimulationSummaryInternal summary =
            run_compiled(session, json, prices, 10000, 9001);
        PC_CHECK(summary.success_count == summary.completed_runs);
        const double mean = summary.known_total_cost /
                            static_cast<double>(summary.completed_runs);
        std::printf("solver compile flagged: V=%.4f empirical=%.4f\n",
                    solved.values[solved.start_state], mean);
        PC_CHECK(std::fabs(mean - solved.values[solved.start_state]) < 0.25);
    }

    /* A partial slot threshold compiles to the simulator's native
     * at_least condition instead of silently reverting to all slots. */
    {
        GoalSpec threshold_goal;
        GoalSlot life;
        life.family_id = 100;
        life.min_tier = 1;
        GoalSlot fire_res;
        fire_res.family_id = 104;
        threshold_goal.slots = {life, fire_res};
        threshold_goal.rarity = PC_RARITY_MAGIC;
        threshold_goal.min_satisfied_slots = 1;
        CalcContext threshold_calc(
            session, threshold_goal, registry,
            {transmute, alteration, restart});
        const std::unordered_map<std::string, double> prices{
            {"transmute", 1.0}, {"alteration", 1.0}, {"base", 10.0}};
        const SolveResult solved = solve(threshold_calc, start, prices);
        PC_CHECK(solved.converged);
        const std::string json = compile_policy_strategy_json(
            threshold_calc, solved, "one-of-two");
        PC_CHECK(json.find("\"type\":\"at_least\",\"count\":1") !=
                 std::string::npos);
        const SimulationSummaryInternal summary =
            run_compiled(session, json, prices, 10000, 777);
        PC_CHECK(summary.completed_runs == 10000);
        PC_CHECK(summary.success_count == summary.completed_runs);
    }

    /* A tag-discriminating layout must still route every sampled result
     * exactly. Q3 may prove a junk identity unobservable under this deliberately
     * tiny action set, so the executable policy—not a redundant serialized
     * mod-count predicate—is the contract. */
    {
        ActionRegistry tagged_registry = registry;
        tagged_registry.actions[transmute].discriminating_tag_ids = {3};
        CalcContext tagged_calc(
            session, goal, std::move(tagged_registry),
            {transmute, restart});
        PC_CHECK(!tagged_calc.layout().discriminating_tag_ids.empty());
        const std::unordered_map<std::string, double> prices{
            {"transmute", 1.0}, {"base", 10.0}};
        const SolveResult solved = solve(tagged_calc, start, prices);
        PC_CHECK(solved.converged);
        const std::string json = compile_policy_strategy_json(
            tagged_calc, solved, "tag-discriminating transmute");
        PC_CHECK(solved.diagnostics.observation_signature_mismatches == 0);
        PC_CHECK(solved.diagnostics.strict_discovered_states >=
                 solved.diagnostics.quotient_states);
        const SimulationSummaryInternal summary =
            run_compiled(session, json, prices, 10000, 8675310);
        PC_CHECK(summary.success_count == summary.completed_runs);
        PC_CHECK(summary.no_matching_edge_count == 0);
        const double expected = solved.values[solved.start_state];
        const double mean = summary.known_total_cost /
                            static_cast<double>(summary.completed_runs);
        std::printf(
            "solver compile tagged: V=%.4f empirical=%.4f\n",
            expected, mean);
        PC_CHECK(std::fabs(mean - expected) < 2.0);
    }

    /* S5/S6 headline gate: six distinct all-T1 slots solve, compile with
     * exact group-tier guards, and simulate at V(start). Unobserved junk
     * identities may be removed by the exact outer quotient. */
    {
        GoalSpec perfect;
        for (std::uint32_t group : {11u, 12u, 13u, 20u, 21u, 22u}) {
            GoalSlot wanted;
            wanted.group_id = group;
            wanted.min_tier = 1;
            perfect.slots.push_back(wanted);
        }
        perfect.rarity = PC_RARITY_RARE;
        const std::uint32_t chaos = registry.index_by_id.at("chaos");
        CalcContext perfect_calc(
            session, perfect, registry, {chaos, restart});
        const std::unordered_map<std::string, double> prices{
            {"chaos", 1.0}, {"base", 10.0}};
        pc_item_state rare;
        pc_item_clear(&rare);
        rare.rarity = PC_RARITY_RARE;
        const SolveResult solved = solve(perfect_calc, rare, prices);
        PC_CHECK(solved.converged);
        PolicyCompilationTelemetry compilation;
        const std::string json = compile_policy_strategy_json(
            perfect_calc, solved, "six-slot all-T1 perfect item",
            &compilation);
        PC_CHECK(compilation.behavioral_classes >
                 compilation.policy_regions);
        PC_CHECK(compilation.policy_regions == 1);
        PC_CHECK(json.find("\"type\":\"has_mod_group\"") !=
                 std::string::npos);
        PC_CHECK(json.find("\"min_tier\":1") != std::string::npos);
        const SimulationSummaryInternal summary =
            run_compiled(session, json, prices, 10000, 8675309);
        PC_CHECK(summary.success_count == summary.completed_runs);
        const double expected = solved.values[solved.start_state];
        const double mean = summary.known_total_cost /
                            static_cast<double>(summary.completed_runs);
        std::printf(
            "solver compile perfect item: V=%.4f empirical=%.4f "
            "(%u states, %llu classes, %llu regions, %llu condition "
            "bytes, %llu JSON bytes)\n",
            expected, mean, perfect_calc.state_count(),
            static_cast<unsigned long long>(
                compilation.behavioral_classes),
            static_cast<unsigned long long>(compilation.policy_regions),
            static_cast<unsigned long long>(
                compilation.total_condition_bytes),
            static_cast<unsigned long long>(
                compilation.strategy_json_bytes));
        PC_CHECK(std::fabs(mean - expected) < 2.0);
    }

    /* Unveil is a sampled offer followed by a zero-cost policy choice. The
     * compiler emits preference-ordered option guards and concrete unveil
     * operations; simulation must reproduce the Bellman value. */
    {
        GoalSpec unveil_goal;
        GoalSlot life;
        life.family_id = 100;
        life.min_tier = 1;
        unveil_goal.slots.push_back(life);
        unveil_goal.rarity = PC_RARITY_RARE;
        const std::uint32_t veiled_exalt =
            registry.index_by_id.at("veiled_exalt");
        const std::uint32_t unveil = registry.index_by_id.at("unveil");
        CalcContext unveil_calc(
            session, unveil_goal, registry,
            {veiled_exalt, unveil, restart});
        const std::unordered_map<std::string, double> prices{
            {"veiled_exalt", 1.0}, {"base", 10.0}};
        pc_item_state rare;
        pc_item_clear(&rare);
        rare.rarity = PC_RARITY_RARE;
        const SolveResult solved = solve(unveil_calc, rare, prices);
        PC_CHECK(solved.converged);
        const std::string json = compile_policy_strategy_json(
            unveil_calc, solved, "policy-selected unveil");
        PC_CHECK(json.find("\"type\":\"has_unveil_option\"") !=
                 std::string::npos);
        PC_CHECK(json.find("\"type\":\"unveil\"") !=
                 std::string::npos);
        const StrategyEvalResult exact =
            evaluate_compiled(session, json, prices);
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
        const SimulationSummaryInternal summary =
            run_compiled(session, json, prices, 10000, 1234567);
        PC_CHECK(summary.success_count == summary.completed_runs);
        PC_CHECK(summary.missing_price_run_count == 0);
        const double expected = solved.values[solved.start_state];
        const double mean = summary.known_total_cost /
                            static_cast<double>(summary.completed_runs);
        std::printf(
            "solver compile unveil: V=%.4f empirical=%.4f success=%llu failure=%llu noedge=%llu unapplied=%llu\n",
            expected, mean,
            static_cast<unsigned long long>(summary.success_count),
            static_cast<unsigned long long>(summary.failure_count),
            static_cast<unsigned long long>(summary.no_matching_edge_count),
            static_cast<unsigned long long>(summary.action_not_applied_count));
        PC_CHECK(std::fabs(mean - expected) < 0.25);
    }

    /* A serialized imported start may already contain an unobserved Veiled
     * placeholder. The compiler must preserve that slot flag, Calculator
     * mode must integrate its offer distribution exactly, and Simulator mode
     * must materialize one offer set per run before routing. */
    {
        GoalSpec unveil_goal;
        GoalSlot life;
        life.family_id = 100;
        life.min_tier = 1;
        unveil_goal.slots.push_back(life);
        unveil_goal.rarity = PC_RARITY_RARE;
        const std::uint32_t veiled_exalt =
            registry.index_by_id.at("veiled_exalt");
        const std::uint32_t unveil = registry.index_by_id.at("unveil");
        CalcContext unveil_calc(
            session, unveil_goal, registry,
            {veiled_exalt, unveil, restart});
        pc_item_state veiled_start;
        pc_item_clear(&veiled_start);
        veiled_start.rarity = PC_RARITY_RARE;
        PC_CHECK(
            pc_item_add_mod(
                &veiled_start, PC_SIDE_PREFIX, 8, 30,
                PC_MOD_SLOT_VEILED, nullptr) == PC_RESULT_OK);
        const std::unordered_map<std::string, double> prices{
            {"veiled_exalt", 1.0}, {"base", 10.0}};
        const SolveResult solved =
            solve(unveil_calc, veiled_start, prices);
        PC_CHECK(solved.converged);
        const std::string json = compile_policy_strategy_json(
            unveil_calc, solved, "imported veiled start");
        PC_CHECK(json.find("\"mod_key\":\"mod8\",\"veiled\":true") !=
                 std::string::npos);
        PC_CHECK(json.find("\"type\":\"has_unveil_option\"") !=
                 std::string::npos);
        const StrategyEvalResult exact =
            evaluate_compiled(session, json, prices);
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
        const SimulationSummaryInternal summary =
            run_compiled(session, json, prices, 10000, 7654321);
        PC_CHECK(summary.success_count == summary.completed_runs);
        PC_CHECK(summary.failure_count == 0);
        PC_CHECK(summary.no_matching_edge_count == 0);
        PC_CHECK(summary.action_not_applied_count == 0);
    }
}

bool read_text_file(const std::string& path, std::string& out) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) return false;
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    out = buffer.str();
    return true;
}

/* End-to-end gate on the real artifact: pinned one-mod goal, basic
 * currency plus restart, simulate-vs-V(start). */
void run_artifact_gate(const char* artifact_dir) {
    if (artifact_dir == nullptr) {
        std::printf("solver compile artifact suite skipped (missing path)\n");
        return;
    }
    const std::string dir = artifact_dir;
    std::string manifest_text;
    std::string strings_text;
    std::string game_text;
    if (!read_text_file(dir + "/manifest.json", manifest_text) ||
        !read_text_file(dir + "/strings.json", strings_text) ||
        !read_text_file(dir + "/game-data.json", game_text)) {
        std::printf("solver compile artifact suite skipped (unreadable)\n");
        return;
    }
    std::shared_ptr<DataImpl> data;
    std::shared_ptr<SessionImpl> session;
    try {
        data = load_data_impl(manifest_text, strings_text, game_text);
        const auto base = data->base_by_path.find(
            "Metadata/Items/Armours/BodyArmours/BodyInt17");
        PC_CHECK(base != data->base_by_path.end());
        if (base == data->base_by_path.end()) return;
        session = std::make_shared<SessionImpl>();
        session->data = data;
        session->base_index = base->second;
        session->item_level = 86;
        build_session(*session);
    } catch (const std::exception& ex) {
        std::printf("solver compile artifact suite: %s\n", ex.what());
        PC_CHECK(false);
        return;
    }

    ActionRegistry registry = build_action_registry(*session);

    /* Pick a goal group whose members are all single-group so no blocker
     * states arise (hybrid blockers need conditions v2). */
    GoalSpec goal;
    GoalSlot slot;
    for (std::uint32_t mod = 0; mod < session->mod_count &&
                                slot.group_id == kNoId; ++mod) {
        if (session->gen_type[mod] != 0 ||
            !pc_bitset_test(session->normal_random_roll_mask.data(), mod) ||
            !pc_bitset_test(session->positive_base_weight_mask.data(),
                            mod)) {
            continue;
        }
        const std::uint32_t group = session->primary_group[mod];
        if (group >= session->group_masks.size() ||
            session->group_masks[group].empty()) {
            continue;
        }
        bool clean = true;
        pc_bitset_for_each(
            session->group_masks[group].data(), session->words,
            [&](std::size_t member) {
                if (session->group_offsets[member + 1] -
                        session->group_offsets[member] !=
                    1) {
                    clean = false;
                }
            });
        if (clean) slot.group_id = group;
    }
    PC_CHECK(slot.group_id != kNoId);
    if (slot.group_id == kNoId) return;
    goal.slots.push_back(slot);

    /* S7.3 fixed options expose exact, finite primitive programs. These are
     * authored here but run only with the plan-level native suite. */
    {
        GoalSpec scour_goal = goal;
        FixedOptionSpec option;
        option.kind = FixedOptionKind::ScourAlchemy;
        scour_goal.fixed_options.push_back(option);
        CalcContext option_calc(
            session, scour_goal, registry, {}, false, false);
        pc_item_state rare;
        pc_item_clear(&rare);
        rare.rarity = PC_RARITY_RARE;
        const std::uint32_t state = option_calc.intern_item(rare);
        const std::uint32_t op =
            static_cast<std::uint32_t>(registry.actions.size());
        const OptionKernel& kernel = option_calc.option_kernel(state, op);
        PC_CHECK(kernel.supported);
        PC_CHECK(kernel.legal);
        PC_CHECK(kernel.terminates_almost_surely);
        PC_CHECK(kernel.expected_primitive_actions == 2.0);
        PC_CHECK(!kernel.exits.empty());
    }

    {
        GoalSpec eldritch_goal = goal;
        FixedOptionSpec option;
        option.kind = FixedOptionKind::EldritchSideIntent;
        option.side = PC_SIDE_PREFIX;
        option.action_id = "eldritch_exalt";
        option.setup_action_ids = {"eldritch_ember:1"};
        eldritch_goal.fixed_options.push_back(option);
        CalcContext option_calc(
            session, eldritch_goal, registry, {}, false, false);
        pc_item_state rare;
        pc_item_clear(&rare);
        rare.rarity = PC_RARITY_RARE;
        const OptionKernel& kernel = option_calc.option_kernel(
            option_calc.intern_item(rare),
            static_cast<std::uint32_t>(registry.actions.size()));
        PC_CHECK(kernel.supported);
        PC_CHECK(kernel.legal);
        PC_CHECK(kernel.expected_primitive_actions == 2.0);
        PC_CHECK(!kernel.exits.empty());
        for (const OutcomeEntry& exit : kernel.exits) {
            const AbstractState& state = option_calc.state(exit.state);
            PC_CHECK(state.searing_exarch_tier >
                     state.eater_of_worlds_tier);
        }
    }

    {
        GoalSpec protected_goal = goal;
        FixedOptionSpec option;
        option.kind = FixedOptionKind::ProtectedSide;
        option.side = PC_SIDE_PREFIX;
        option.action_id = "chaos";
        protected_goal.fixed_options.push_back(option);
        CalcContext option_calc(
            session, protected_goal, registry, {}, false, false);
        pc_item_state rare;
        pc_item_clear(&rare);
        rare.rarity = PC_RARITY_RARE;
        const OptionKernel& kernel = option_calc.option_kernel(
            option_calc.intern_item(rare),
            static_cast<std::uint32_t>(registry.actions.size()));
        PC_CHECK(kernel.supported);
        PC_CHECK(kernel.legal);
        PC_CHECK(kernel.expected_primitive_actions == 2.0);
        PC_CHECK(!kernel.exits.empty());
    }

    /* S7.4 renewal normalizes only certified same-kernel failures to the
     * entry state; the compiled graph expands that normalization back into
     * an ordinary primitive retry router. */
    {
        GoalSpec renewal_goal = goal;
        FixedOptionSpec option;
        option.kind = FixedOptionKind::Renewal;
        option.program_action_ids = {"chaos"};
        option.exit_goal_slots = {0};
        option.exit_min_satisfied = 1;
        renewal_goal.fixed_options.push_back(option);
        CalcContext option_calc(
            session, renewal_goal, registry, {}, false, false);
        pc_item_state rare;
        pc_item_clear(&rare);
        rare.rarity = PC_RARITY_RARE;
        const std::uint32_t state = option_calc.intern_item(rare);
        const std::uint32_t op =
            static_cast<std::uint32_t>(registry.actions.size());
        const OptionKernel& kernel = option_calc.option_kernel(state, op);
        PC_CHECK(kernel.supported);
        PC_CHECK(kernel.legal);
        PC_CHECK(kernel.terminates_almost_surely);
        PC_CHECK(kernel.expected_primitive_actions == 1.0);
        PC_CHECK(!kernel.retry_states.empty());
        PC_CHECK(kernel.observation_choice_groups.empty());

        const SolveResult solved = solve(
            option_calc, rare, {{"chaos", 1.0}});
        report_compile_solve_issue("renewal chaos", solved);
        PC_CHECK(solved.converged);
        PC_CHECK(solved.policy[solved.start_state] == op);
        const std::string strategy = compile_policy_strategy_json(
            option_calc, solved, "renewal-chaos");
        PC_CHECK(strategy.find("\"type\":\"chaos\"") !=
                 std::string::npos);
        PC_CHECK(strategy.find("_retry") != std::string::npos);
    }

    /* Validate the real-artifact ProtectedRepeat vocabulary without expanding
     * the unbounded full-pool lock-plus-chaos kernel in a contract unit. The
     * performance corpus owns full reforge expansion and resource caps. */
    {
        GoalSpec protected_goal = goal;
        FixedOptionSpec option;
        option.kind = FixedOptionKind::ProtectedRepeat;
        option.side = PC_SIDE_PREFIX;
        option.action_id = "chaos";
        option.exit_goal_slots = {0};
        option.exit_min_satisfied = 1;
        protected_goal.fixed_options.push_back(option);
        CalcContext option_calc(
            session, protected_goal, registry, {}, false, false);
        const PlannerOperator& planner =
            option_calc.operators().at(registry.actions.size());
        PC_CHECK(planner.kind == PlannerOperatorKind::FixedOption);
        PC_CHECK(planner.option_kind == FixedOptionKind::ProtectedRepeat);
        PC_CHECK(planner.primitive_program.size() == 2);
        PC_CHECK(planner.id.find("option:protected_repeat:prefix:chaos") == 0);
    }

    /* Observation-aware renewal keeps the sampled Unveil set for Bellman
     * choice and for the compiled has_unveil_option routers. Use the bounded
     * exact synthetic pool; full canonical Veiled Chaos expansion is a
     * performance-corpus responsibility. */
    auto observed_session = make_compile_session();
    ActionRegistry observed_registry = build_action_registry(*observed_session);
    std::uint32_t unveiled_goal_mod = kNoId;
    if (!observed_session->unveiled_generic_mask.empty()) {
        pc_bitset_for_each(
            observed_session->unveiled_generic_mask.data(),
            observed_session->words,
            [&](std::size_t bit) {
                if (unveiled_goal_mod == kNoId) {
                    unveiled_goal_mod = static_cast<std::uint32_t>(bit);
                }
            });
    }
    if (unveiled_goal_mod != kNoId) {
        GoalSpec unveil_goal;
        GoalSlot unveil_slot;
        unveil_slot.family_id =
            observed_session->family_id[unveiled_goal_mod];
        unveil_goal.slots.push_back(unveil_slot);
        FixedOptionSpec option;
        option.kind = FixedOptionKind::Renewal;
        option.program_action_ids = {"veiled_chaos", "unveil"};
        option.exit_goal_slots = {0};
        option.exit_min_satisfied = 1;
        unveil_goal.fixed_options.push_back(option);
        CalcContext option_calc(
            observed_session, unveil_goal, observed_registry, {}, false,
            false);
        pc_item_state rare;
        pc_item_clear(&rare);
        rare.rarity = PC_RARITY_RARE;
        const std::uint32_t state = option_calc.intern_item(rare);
        const std::uint32_t op =
            static_cast<std::uint32_t>(observed_registry.actions.size());
        const OptionKernel& kernel = option_calc.option_kernel(state, op);
        PC_CHECK(kernel.supported);
        PC_CHECK(kernel.legal);
        PC_CHECK(!kernel.observation_choice_groups.empty());
        PC_CHECK(!kernel.observation_choice_options.empty());
        for (const OutcomeChoiceGroup& group :
             kernel.observation_choice_groups) {
            PC_CHECK(group.observation_state != kNoId);
        }

        const SolveResult solved = solve(
            option_calc, rare, {{"veiled_chaos", 1.0}});
        PC_CHECK(solved.converged);
        PC_CHECK(!solved.option_unveil_preferences[state].empty());
        PC_CHECK(!solved.behavioral_representative_by_state.empty());
        std::map<std::uint32_t, std::set<std::uint32_t>>
            offered_mods_by_projected_successor;
        for (const ObservedUnveilPreference& observation :
             solved.option_unveil_preferences[state]) {
            for (const ObservedUnveilChoice& choice : observation.choices) {
                PC_CHECK(
                    choice.successor_state <
                    solved.behavioral_representative_by_state.size());
                if (choice.successor_state >=
                    solved.behavioral_representative_by_state.size()) {
                    continue;
                }
                offered_mods_by_projected_successor[
                    solved.behavioral_representative_by_state[
                        choice.successor_state]]
                    .insert(choice.mod_id);
            }
        }
        bool projected_successor_has_distinct_offered_mods = false;
        std::size_t max_offered_mods_per_projected_successor = 0;
        std::uint32_t projected_collision_classes = 0;
        for (const auto& [unused_successor, offered_mods] :
             offered_mods_by_projected_successor) {
            (void)unused_successor;
            max_offered_mods_per_projected_successor = std::max(
                max_offered_mods_per_projected_successor,
                offered_mods.size());
            if (offered_mods.size() > 1) {
                projected_successor_has_distinct_offered_mods = true;
                ++projected_collision_classes;
            }
        }
        PC_CHECK(projected_successor_has_distinct_offered_mods);
        const std::string strategy = compile_policy_strategy_json(
            option_calc, solved, "observed-unveil-renewal");
        PC_CHECK(strategy.find("has_unveil_option") != std::string::npos);
        PC_CHECK(strategy.find("\"type\":\"veiled_chaos\"") !=
                 std::string::npos);
        std::set<std::uint32_t> compiled_offered_mods;
        for (const ObservedUnveilPreference& observation :
             solved.option_unveil_preferences[state]) {
            for (const ObservedUnveilChoice& choice : observation.choices) {
                compiled_offered_mods.insert(choice.mod_id);
            }
        }
        PC_CHECK(compiled_offered_mods.size() > 1);
        for (const std::uint32_t mod_id : compiled_offered_mods) {
            const std::string marker =
                "\"mod_key\":\"" +
                observed_session->data->string_at(
                    observed_session->data->mod_key_sid.at(mod_id)) +
                "\"";
            PC_CHECK(strategy.find(marker) != std::string::npos);
        }
        const StrategyEvalResult exact = evaluate_compiled(
            observed_session, strategy, {{"veiled_chaos", 1.0}});
        std::printf(
            "solver observed unveil exact: cost=%.12f solver=%.12f "
            "success=%.12f failure=%.12f unapplied=%.12f "
            "noedge=%.12f unresolved=%.12f\n",
            exact.total_expected_cost, solved.evaluated_policy_cost,
            exact.success_probability, exact.failure_probability,
            exact.action_not_applied_probability,
            exact.no_matching_edge_probability,
            exact.unresolved_probability);
        for (const StrategyEvalFailure& failure :
             exact.failures_by_node) {
            std::printf(
                "solver observed unveil failure: node=%s reason=%s "
                "probability=%.12f\n",
                failure.node_id.c_str(), failure.reason.c_str(),
                failure.probability);
        }
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
        const SimulationSummaryInternal observed_summary =
            run_compiled(
                observed_session, strategy,
                {{"veiled_chaos", 1.0}}, 10000, 246813579);
        PC_CHECK(
            observed_summary.success_count ==
            observed_summary.completed_runs);
        PC_CHECK(observed_summary.failure_count == 0);
        PC_CHECK(observed_summary.action_not_applied_count == 0);
        PC_CHECK(observed_summary.no_matching_edge_count == 0);
        std::printf(
            "solver quotient choice audit: offered=%zu "
            "projected_collision_classes=%u max_offered_per_class=%zu\n",
            compiled_offered_mods.size(), projected_collision_classes,
            max_offered_mods_per_projected_successor);
    }

    /* Pick one ordinary crafted prefix and suffix with no group conflict, then
     * force the deterministic Multimod option as the only candidate. */
    std::uint32_t finish_prefix = kNoId;
    std::uint32_t finish_suffix = kNoId;
    const auto conflicts = [&](std::uint32_t left, std::uint32_t right) {
        for (std::uint32_t a = session->group_offsets[left];
             a < session->group_offsets[left + 1]; ++a) {
            for (std::uint32_t b = session->group_offsets[right];
                 b < session->group_offsets[right + 1]; ++b) {
                if (session->group_ids[a] == session->group_ids[b]) {
                    return true;
                }
            }
        }
        return false;
    };

    /* Fracture preparation keys success to the exact satisfying carrier.
     * Wrong-carrier results remain exits, a fractured carrier still
     * satisfies its ordinary goal slot, and Eldritch implicits survive. Keep
     * this exact carrier test on the bounded synthetic modifier universe. */
    {
        auto fracture_session = make_compile_session();
        ActionRegistry fracture_registry =
            build_action_registry(*fracture_session);
        GoalSpec fracture_goal;
        GoalSlot carrier_slot;
        carrier_slot.family_id = 100;
        fracture_goal.slots.push_back(carrier_slot);
        FixedOptionSpec option;
        option.kind = FixedOptionKind::FracturePrepare;
        option.program_action_ids = {"chaos"};
        option.carrier_goal_slot = 0;
        fracture_goal.fixed_options.push_back(option);
        CalcContext option_calc(
            fracture_session, fracture_goal, fracture_registry, {}, false,
            false);
        pc_item_state ready;
        pc_item_clear(&ready);
        ready.rarity = PC_RARITY_RARE;
        ready.searing_exarch_tier = 1;
        for (const std::uint32_t mod : {0u, 3u, 5u, 6u}) {
            PC_CHECK(pc_item_add_mod(
                         &ready, fracture_session->gen_type[mod], mod,
                         static_cast<std::uint16_t>(
                             fracture_session->primary_group[mod]),
                         0, nullptr) == PC_RESULT_OK);
        }
        const std::uint32_t state = option_calc.intern_item(ready);
        const OptionKernel& kernel = option_calc.option_kernel(
            state,
            static_cast<std::uint32_t>(fracture_registry.actions.size()));
        PC_CHECK(kernel.supported);
        PC_CHECK(kernel.legal);
        PC_CHECK(kernel.entry_continues);
        PC_CHECK(kernel.expected_primitive_actions == 1.0);
        double carrier_probability = 0.0;
        for (const OutcomeEntry& exit : kernel.exits) {
            const AbstractState& fractured = option_calc.state(exit.state);
            PC_CHECK(fractured.slot_status[0] ==
                     static_cast<std::uint8_t>(
                         GoalSlotStatus::Satisfied));
            PC_CHECK(fractured.searing_exarch_tier == 1);
            if ((fractured.fractured_goal_mask & 1u) != 0) {
                carrier_probability += exit.probability;
            }
        }
        PC_CHECK(std::fabs(carrier_probability - 0.25) < 1e-12);

        pc_item_state influenced = ready;
        influenced.generic_influence_bits = 1;
        const OptionKernel& influenced_kernel = option_calc.option_kernel(
            option_calc.intern_item(influenced),
            static_cast<std::uint32_t>(fracture_registry.actions.size()));
        PC_CHECK(!influenced_kernel.legal);
        for (const ActionDescriptor& action : fracture_registry.actions) {
            if (action.params.type == ActionType::InfluenceExalt) {
                pc_item_state fractured_item = ready;
                fractured_item.prefixes[0].flags |= PC_MOD_SLOT_FRACTURED;
                PC_CHECK(!action_legal(
                    *fracture_session, action,
                    option_calc.state(
                        option_calc.intern_item(fractured_item))));
                break;
            }
        }
    }

    for (std::uint32_t index = 0; index < registry.actions.size(); ++index) {
        const ActionDescriptor& action = registry.actions[index];
        if (action.params.type != ActionType::Bench ||
            action.params.mod_id >= session->metamod_type.size() ||
            session->metamod_type[action.params.mod_id] >= 0) {
            continue;
        }
        if (session->gen_type[action.params.mod_id] == PC_SIDE_PREFIX &&
            finish_prefix == kNoId) {
            finish_prefix = index;
        }
        if (session->gen_type[action.params.mod_id] == PC_SIDE_SUFFIX &&
            finish_prefix != kNoId &&
            !conflicts(registry.actions[finish_prefix].params.mod_id,
                       action.params.mod_id)) {
            finish_suffix = index;
            break;
        }
    }
    PC_CHECK(finish_prefix != kNoId);
    PC_CHECK(finish_suffix != kNoId);
    if (finish_prefix != kNoId && finish_suffix != kNoId) {
        GoalSpec finish_goal;
        for (const std::uint32_t action_index :
             {finish_prefix, finish_suffix}) {
            GoalSlot finish_slot;
            finish_slot.family_id = session->family_id[
                registry.actions[action_index].params.mod_id];
            finish_goal.slots.push_back(finish_slot);
        }
        finish_goal.rarity = PC_RARITY_RARE;
        FixedOptionSpec option;
        option.kind = FixedOptionKind::MultimodFinish;
        option.bench_craft_ids = {
            registry.actions[finish_prefix].id,
            registry.actions[finish_suffix].id};
        finish_goal.fixed_options.push_back(option);
        CalcContext option_calc(
            session, finish_goal, registry, {}, false, false);

        const PlannerOperator& planner =
            option_calc.operators().at(registry.actions.size());
        PC_CHECK(planner.kind == PlannerOperatorKind::FixedOption);
        PC_CHECK(planner.primitive_program.size() == 3);
        std::unordered_map<std::string, double> finish_prices;
        double expected_cost = 0.0;
        for (const auto& [key, quantity] : planner.resource_quantities) {
            const double price =
                static_cast<double>(finish_prices.size() + 1);
            finish_prices[key] = price;
            expected_cost += quantity * price;
        }

        pc_item_state start;
        pc_item_clear(&start);
        start.rarity = PC_RARITY_RARE;
        const SolveResult solved = solve(option_calc, start, finish_prices);
        PC_CHECK(solved.converged);
        PC_CHECK(solved.policy[solved.start_state] ==
                 static_cast<std::uint32_t>(registry.actions.size()));
        PC_CHECK(std::fabs(solved.values[solved.start_state] - expected_cost) <
                 1e-9);
        const std::string strategy = compile_policy_strategy_json(
            option_calc, solved, "fixed-multimod-finish");
        PC_CHECK(strategy.find("option:multimod") == std::string::npos);
        PC_CHECK(strategy.find("_o1") != std::string::npos);
        PC_CHECK(strategy.find("_o2") != std::string::npos);
    }

    std::vector<std::uint32_t> candidates;
    for (const char* id : {"transmute", "augment", "alteration", "regal",
                           "alchemy", "chaos", "exalt", "annul", "scour",
                           "restart"}) {
        candidates.push_back(registry.index_by_id.at(id));
    }
    CalcContext calc(session, goal, registry, candidates);
    const std::unordered_map<std::string, double> prices{
        {"transmute", 0.1}, {"augment", 0.5}, {"alteration", 0.2},
        {"regal", 1.0},     {"alchemy", 0.5}, {"chaos", 1.0},
        {"exalt", 20.0},    {"annul", 3.0},   {"scour", 0.5},
        {"base", 5.0}};

    pc_item_state start;
    pc_item_clear(&start);
    start.rarity = PC_RARITY_RARE;
    const SolveResult solved = solve(calc, start, prices);
    PC_CHECK(solved.converged);
    const double expected = solved.values[solved.start_state];

    const std::string json =
        compile_policy_strategy_json(calc, solved, "artifact-toy");
    const SimulationSummaryInternal summary =
        run_compiled(session, json, prices, 10000, 987654321);
    PC_CHECK(summary.completed_runs == 10000);
    PC_CHECK(summary.success_count == summary.completed_runs);
    PC_CHECK(summary.missing_price_run_count == 0);
    const double mean = summary.known_total_cost /
                        static_cast<double>(summary.completed_runs);
    std::printf(
        "solver compile artifact: V=%.4f empirical=%.4f over %llu runs\n",
        expected, mean,
        static_cast<unsigned long long>(summary.completed_runs));
    PC_CHECK(std::fabs(mean - expected) < 0.25);
}

/* S8.4R.3 focused fixture: the final goal is rare, but the solver reaches a
 * magic carrier, discovers an exact Augment Imprint stage and intermediate
 * magic exit, then continues through the ordinary Regal Bellman value. */
void run_imprint_gate(const char* artifact_dir) {
    if (artifact_dir == nullptr) {
        std::printf("solver Imprint fixture skipped (missing path)\n");
        return;
    }
    const std::string dir = artifact_dir;
    std::string manifest_text;
    std::string strings_text;
    std::string game_text;
    if (!read_text_file(dir + "/manifest.json", manifest_text) ||
        !read_text_file(dir + "/strings.json", strings_text) ||
        !read_text_file(dir + "/game-data.json", game_text)) {
        std::printf("solver Imprint fixture skipped (unreadable)\n");
        return;
    }

    std::shared_ptr<DataImpl> data;
    std::shared_ptr<SessionImpl> session;
    try {
        data = load_data_impl(manifest_text, strings_text, game_text);
        const auto base = data->base_by_path.find(
            "Metadata/Items/Armours/BodyArmours/BodyInt17");
        PC_CHECK(base != data->base_by_path.end());
        if (base == data->base_by_path.end()) return;
        session = std::make_shared<SessionImpl>();
        session->data = data;
        session->base_index = base->second;
        session->item_level = 86;
        build_session(*session);
    } catch (const std::exception& ex) {
        std::printf("solver Imprint fixture: %s\n", ex.what());
        PC_CHECK(false);
        return;
    }

    PC_CHECK(data->bestiary_action_by_id.contains("bestiary:imprint"));
    PC_CHECK(data->bestiary_action_by_id.contains(
        "bestiary:restore_imprint"));
    std::uint32_t carrier_mod = kNoId;
    std::uint32_t target_mod = kNoId;
    for (std::uint32_t mod = 0; mod < session->mod_count; ++mod) {
        if (!pc_bitset_test(session->normal_random_roll_mask.data(), mod) ||
            !pc_bitset_test(session->positive_base_weight_mask.data(), mod)) {
            continue;
        }
        if (session->gen_type[mod] == PC_SIDE_PREFIX &&
            carrier_mod == kNoId) {
            carrier_mod = mod;
        } else if (session->gen_type[mod] == PC_SIDE_SUFFIX &&
                   target_mod == kNoId) {
            target_mod = mod;
        }
        if (carrier_mod != kNoId && target_mod != kNoId) break;
    }
    PC_CHECK(carrier_mod != kNoId);
    PC_CHECK(target_mod != kNoId);
    if (carrier_mod == kNoId || target_mod == kNoId) return;

    const ActionRegistry complete = build_action_registry(*session);
    ActionRegistry registry;
    for (const char* id : {"augment", "regal"}) {
        const auto found = complete.index_by_id.find(id);
        PC_CHECK(found != complete.index_by_id.end());
        if (found == complete.index_by_id.end()) return;
        const std::uint32_t index =
            static_cast<std::uint32_t>(registry.actions.size());
        registry.index_by_id.emplace(id, index);
        registry.actions.push_back(complete.actions.at(found->second));
    }

    GoalSpec goal;
    goal.rarity = PC_RARITY_RARE;
    goal.automatic_candidates = true;
    GoalSlot carrier;
    carrier.group_id = session->primary_group[carrier_mod];
    GoalSlot target;
    target.group_id = session->primary_group[target_mod];
    goal.slots = {carrier, target};
    CalcContext calc(session, goal, registry);

    pc_item_state magic;
    pc_item_clear(&magic);
    magic.rarity = PC_RARITY_MAGIC;
    PC_CHECK(pc_item_add_mod(
                 &magic, PC_SIDE_PREFIX, carrier_mod,
                 static_cast<std::uint16_t>(
                     session->primary_group[carrier_mod]),
                 0, nullptr) == PC_RESULT_OK);

    const std::unordered_map<std::string, double> prices{
        {"augment", 0.1},
        {"regal", 100.0},
        {"beast:craicic-chimeral", 0.01},
        {"beast:rare", 0.01},
    };
    SolveOptions solve_options;
    solve_options.max_imprint_program_depth = 1;
    solve_options.max_imprint_program_work = 16;
    const SolveResult solved = solve(calc, magic, prices, solve_options);
    report_compile_solve_issue("automatic imprint retry", solved);
    PC_CHECK(solved.converged);
    PC_CHECK(solved.start_state < solved.policy.size());
    const std::uint32_t selected = solved.policy[solved.start_state].index;
    PC_CHECK(selected < calc.operators().size());
    if (selected >= calc.operators().size()) return;
    const PlannerOperator& planner = calc.operators().at(selected);
    PC_CHECK(planner.kind == PlannerOperatorKind::FixedOption);
    PC_CHECK(planner.option_kind == FixedOptionKind::ImprintRetry);
    PC_CHECK(planner.automatic_kind == AutomaticCandidateKind::Imprint);
    PC_CHECK(planner.primitive_program.size() == 1);
    PC_CHECK(planner.primitive_program.front() ==
             registry.index_by_id.at("augment"));

    const OptionKernel& kernel = calc.option_kernel(
        solved.start_state, selected);
    PC_CHECK(kernel.supported);
    PC_CHECK(kernel.legal);
    PC_CHECK(kernel.terminates_almost_surely);
    PC_CHECK(!kernel.retry_states.empty());
    bool has_magic_intermediate = false;
    for (const OutcomeEntry& exit : kernel.exits) {
        if (exit.state == kNoId || exit.state == solved.start_state) continue;
        const AbstractState& successor = calc.state(exit.state);
        if (successor.rarity == PC_RARITY_MAGIC &&
            successor.slot_status[1] == static_cast<std::uint8_t>(
                GoalSlotStatus::Satisfied) &&
            !calc.is_goal_state(successor)) {
            has_magic_intermediate = true;
        }
    }
    PC_CHECK(has_magic_intermediate);
    const auto quantity = [&](const char* key) {
        for (const auto& [resource, amount] : kernel.expected_resources) {
            if (resource == key) return amount;
        }
        return 0.0;
    };
    PC_CHECK(quantity("augment") == 1.0);
    PC_CHECK(quantity("beast:craicic-chimeral") == 1.0);
    PC_CHECK(quantity("beast:rare") == 3.0);
    PC_CHECK(std::any_of(
        solved.diagnostics.automatic_candidate_witnesses.begin(),
        solved.diagnostics.automatic_candidate_witnesses.end(),
        [](const std::string& witness) {
            return witness.find("max_imprint_program_depth") !=
                   std::string::npos;
        }));

    CalcContext work_limited_calc(session, goal, registry);
    SolveOptions work_limited_options;
    work_limited_options.max_imprint_program_depth = 3;
    work_limited_options.max_imprint_program_work = 1;
    const SolveResult work_limited = solve(
        work_limited_calc, magic, prices, work_limited_options);
    PC_CHECK(std::any_of(
        work_limited.diagnostics.automatic_candidate_witnesses.begin(),
        work_limited.diagnostics.automatic_candidate_witnesses.end(),
        [](const std::string& witness) {
            return witness.find("max_imprint_program_work") !=
                   std::string::npos;
        }));

    const std::string strategy = compile_policy_strategy_json(
        calc, solved, "automatic-imprint-to-rare");
    PC_CHECK(strategy.find("\"type\":\"bestiary:imprint\"") !=
             std::string::npos);
    PC_CHECK(strategy.find("\"type\":\"augment\"") !=
             std::string::npos);
    PC_CHECK(strategy.find("\"type\":\"bestiary:restore_imprint\"") !=
             std::string::npos);
    PC_CHECK(strategy.find("\"type\":\"regal\"") !=
             std::string::npos);
    PC_CHECK(strategy.find("_imprint_route") != std::string::npos);

    auto compiled = compile_strategy_json(
        session, strategy.data(), strategy.size());
    const ActionRegistry accounting_registry =
        build_action_registry(*session);
    bool resolved_create = false;
    bool resolved_restore = false;
    for (const StrategyNode& node : compiled->nodes) {
        if (node.action_type != kStrategyBestiaryImprintOperation &&
            node.action_type !=
                kStrategyBestiaryRestoreImprintOperation) {
            continue;
        }
        const ResolvedStrategyOperation operation =
            resolve_strategy_operation(
                node, accounting_registry, *session);
        PC_CHECK(operation.kind ==
                 ResolvedStrategyOperationKind::Bestiary);
        PC_CHECK(operation.descriptor_index ==
                 node.bestiary_action_index);
        PC_CHECK(resolve_strategy_action(node, accounting_registry) ==
                 kNoId);
        if (node.action_type == kStrategyBestiaryImprintOperation) {
            resolved_create = true;
        } else {
            resolved_restore = true;
        }
    }
    PC_CHECK(resolved_create);
    PC_CHECK(resolved_restore);
    auto economy = std::make_shared<EconomyImpl>();
    economy->id = "s8.4r.3-focused";
    economy->prices = prices;
    StrategyEvalOptions eval_options;
    eval_options.economy = economy;
    const StrategyEvalResult exact =
        evaluate_strategy(*compiled, eval_options);
    PC_CHECK(exact.converged);
    PC_CHECK(exact.cost_complete);
    PC_CHECK(std::fabs(
                 exact.total_expected_cost -
                 solved.values[solved.start_state]) < 1e-8);
    PC_CHECK(exact.expected_consumption.at(
                 "beast:craicic-chimeral") > 1.0);
    PC_CHECK(std::fabs(
                 exact.expected_consumption.at("beast:rare") -
                 3.0 * exact.expected_consumption.at(
                           "beast:craicic-chimeral")) < 1e-8);
    PC_CHECK(std::any_of(
        exact.action_totals.begin(), exact.action_totals.end(),
        [](const StrategyEvalActionTotal& action) {
            return action.id == "bestiary:imprint";
        }));
    PC_CHECK(std::any_of(
        exact.action_totals.begin(), exact.action_totals.end(),
        [](const StrategyEvalActionTotal& action) {
            return action.id == "bestiary:restore_imprint";
        }));
    SimulatorImpl simulator;
    simulator.session = session;
    simulator.strategy = compiled;
    simulator.economy = economy;
    prepare_simulator_runtime(simulator);
    SimulationOptionsInternal simulation_options;
    simulation_options.target_runs = 64;
    simulation_options.seed = 20260718;
    simulation_options.max_actions_per_run = 100000;
    run_simulator_chunk(simulator, simulation_options, 64);
    PC_CHECK(simulator.summary.completed_runs == 64);
    PC_CHECK(simulator.summary.success_count == 64);
    PC_CHECK(simulator.summary.failure_count == 0);
    PC_CHECK(simulator.summary.action_limit_count == 0);
    PC_CHECK(simulator.summary.action_not_applied_count == 0);
    PC_CHECK(simulator.summary.no_matching_edge_count == 0);
    std::uint64_t imprint_count = 0;
    std::uint64_t restore_count = 0;
    for (std::size_t node = 0; node < compiled->nodes.size(); ++node) {
        if (compiled->nodes[node].action_type ==
            kStrategyBestiaryImprintOperation) {
            imprint_count += simulator.action_counts[node];
        } else if (compiled->nodes[node].action_type ==
                   kStrategyBestiaryRestoreImprintOperation) {
            restore_count += simulator.action_counts[node];
        }
    }
    PC_CHECK(imprint_count == restore_count + 64);
    PC_CHECK(simulator.action_descriptor_counts["augment"] == imprint_count);
    PC_CHECK(simulator.action_descriptor_counts["regal"] == 64);
    PC_CHECK(simulator.action_descriptor_counts["bestiary:imprint"] ==
             imprint_count);
    PC_CHECK(simulator.action_descriptor_counts[
                 "bestiary:restore_imprint"] == restore_count);
    PC_CHECK(simulator.material_counts["beast:craicic-chimeral"] ==
             imprint_count);
    PC_CHECK(simulator.material_counts["beast:rare"] == 3 * imprint_count);
    std::printf(
        "solver Imprint R3 fixture: V=%.6f creates=%llu restores=%llu runs=64\n",
        solved.values[solved.start_state],
        static_cast<unsigned long long>(imprint_count),
        static_cast<unsigned long long>(restore_count));
}

} // namespace

void run_solver_compile_tests(const char* artifact_dir) {
    run_future_observed_choice_compile_test();
    run_structured_observation_route_tests();
    run_synthetic_gate();
    run_artifact_gate(artifact_dir);
    run_imprint_gate(artifact_dir);
}

void run_solver_imprint_tests(const char* artifact_dir) {
    run_imprint_gate(artifact_dir);
}
