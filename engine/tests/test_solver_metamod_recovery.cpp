#include "tests.hpp"
#include "../src/solver_internal.hpp"
#include "../src/solver_options_helpers.hpp"
#include "../src/solver_solve_types.hpp"
#include "poecraft/item_state.h"
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace poecraft;
using namespace poecraft::solver;

namespace {
std::string read(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("missing metamod test input: " + path);
    std::ostringstream text; text << input.rdbuf(); return text.str();
}
std::uint32_t mod(const SessionImpl& session, const std::string& key) {
    for (std::uint32_t i = 0; i < session.mod_count; ++i)
        if (session.data->string_at(session.data->mod_key_sid.at(session.global_index.at(i))) == key)
            return i;
    throw std::runtime_error("missing metamod test modifier: " + key);
}
void put(pc_item_state& item, const SessionImpl& session, std::uint32_t id, std::uint8_t flags = 0) {
    PC_CHECK(pc_item_add_mod(&item, session.gen_type.at(id), id, session.primary_group.at(id), flags, nullptr) == PC_RESULT_OK);
}
GoalSpec target(const SessionImpl& session, const std::vector<std::uint32_t>& ids) {
    GoalSpec goal; goal.rarity = PC_RARITY_RARE; goal.automatic_candidates = true;
    for (auto id : ids) {
        GoalSlot slot; slot.family_id = session.family_id.at(id); slot.min_tier = session.family_tier_index.at(id);
        goal.slots.push_back(slot);
    }
    return goal;
}
}

void run_solver_metamod_recovery_tests(const char* artifact_dir) {
    if (!artifact_dir) throw std::runtime_error("metamod suite requires current artifact");
    const std::string dir = artifact_dir;
    auto data = load_data_impl(read(dir + "/manifest.json"), read(dir + "/strings.json"), read(dir + "/game-data.json"));
    auto session = std::make_shared<SessionImpl>(); session->data = data;
    session->base_index = data->base_by_path.at("Metadata/Items/Rings/Ring10"); session->item_level = 86;
    build_session(*session);
    ActionRegistryBuildOptions build; build.exhaustive_fossils = false;
    const auto registry = build_action_registry(*session, build);
    std::unordered_map<std::string, double> prices;
    for (const auto& action : registry.actions) for (const auto& key : action.cost_keys) prices[key] = 1.0;
    prices["scour"] = 0.3741;
    for (const auto& action : registry.actions)
        if (action.params.type == ActionType::Bench &&
            (session->metamod_type.at(action.params.mod_id) == data->metamod_prefixes_locked_code ||
             session->metamod_type.at(action.params.mod_id) == data->metamod_suffixes_locked_code))
            for (const auto& key : action.cost_keys) prices[key] = 424.0;
    const std::vector<std::uint32_t> natural_candidates{registry.index_by_id.at("chaos")};
    const std::vector<std::uint32_t> prefixes{mod(*session,"IncreasedLife7"), mod(*session,"IncreasedMana13")};
    const std::vector<std::uint32_t> suffixes{mod(*session,"FireResist8"), mod(*session,"ColdResist8")};
    AutomaticAdmissionLimits limits; limits.cheap_programs_only = true; limits.prices = &prices;
    limits.consider_imprint_programs = false; limits.max_solver_owned_bytes = 64ull << 20;
    limits.max_state_action_rows = 2048; limits.max_transitions = 4096;

    for (const auto& action : registry.actions) {
        const auto type = ordinary_add_equivalent(action.params.type);
        if (type == ActionType::Augment || type == ActionType::Regal || type == ActionType::Exalt ||
            type == ActionType::InfluenceExalt || type == ActionType::HarvestAugment) {
            PC_CHECK(action.preservation.respects_cannot_roll_attack);
            PC_CHECK(action.preservation.respects_cannot_roll_caster);
        }
        if (type == ActionType::Annul || type == ActionType::Scour || type == ActionType::HarvestResist || type == ActionType::HarvestAugment) {
            PC_CHECK(action.preservation.respects_prefix_lock && action.preservation.respects_suffix_lock);
            PC_CHECK(action.preservation.preserves_fractured_affixes);
        }
    }
    for (int side : {PC_SIDE_PREFIX, PC_SIDE_SUFFIX}) {
        auto goal = target(*session, side == PC_SIDE_PREFIX ? prefixes : suffixes);
        goal.automatic_candidate_kind_mask = automatic_candidate_kind_bit(AutomaticCandidateKind::ProtectedMetamod);
        CalcContext calc(session, goal, registry, natural_candidates, false, false, true);
        pc_item_state root; pc_item_clear(&root); root.rarity = PC_RARITY_RARE;
        for (auto id : (side == PC_SIDE_PREFIX ? prefixes : suffixes)) put(root,*session,id);
        for (auto id : (side == PC_SIDE_PREFIX ? suffixes : prefixes)) put(root,*session,id);
        const auto state = calc.intern_item(root);
        const auto batch = calc.admit_state_local_automatic_candidates(state, limits);
        PC_CHECK(batch.status == StateLocalAutomaticBatchStatus::Complete);
        unsigned matches = 0;
        for (auto index : batch.admitted_operators) {
            const auto& op = calc.operators().at(index);
            if (op.option_kind != FixedOptionKind::ProtectedSide || op.intended_side != side) continue;
            ++matches;
            PC_CHECK(calc.is_candidate_operator_admitted_for_state(state, index));
            const auto& kernel = calc.option_kernel(state,index);
            PC_CHECK(kernel.legal && kernel.supported && kernel.terminates_almost_surely);
            PC_CHECK(kernel.exits.size() == 1 && calc.is_goal_state(calc.state(kernel.exits.front().state)));
            double cost = 0; for (const auto& [key, count] : kernel.expected_resources) cost += prices.at(key) * count;
            PC_CHECK(std::abs(cost - 424.3741) < 1e-8);
            PC_CHECK(kernel.expected_primitive_actions == 2);
        }
        PC_CHECK(matches == 1);
        if (matches != 1) for (const auto& decision : batch.decisions)
            std::printf("protected cleanup side=%d candidate=%s reason=%s\n", side, decision.id.c_str(), decision.evidence.reason.c_str());
        // The explicit native oracle has the same complete exit law.
        FixedOptionSpec explicit_option; explicit_option.kind = FixedOptionKind::ProtectedSide;
        explicit_option.side = side; explicit_option.action_id = "scour";
        auto oracle_goal = goal; oracle_goal.automatic_candidates = false; oracle_goal.fixed_options = {explicit_option};
        CalcContext oracle(session, oracle_goal, registry, natural_candidates, false, false, true);
        const auto oracle_state = oracle.intern_item(root);
        const auto& law = oracle.option_kernel(oracle_state, oracle.candidate_operators().back());
        PC_CHECK(law.legal && law.exits.size() == 1);
        if (law.exits.size() == 1) PC_CHECK(oracle.is_goal_state(oracle.state(law.exits.front().state)));
    }

    for (unsigned control = 0; control < 4; ++control) {
        auto goal = target(*session, prefixes);
        goal.automatic_candidate_kind_mask = automatic_candidate_kind_bit(AutomaticCandidateKind::CraftedCleanup);
        pc_item_state root; pc_item_clear(&root); root.rarity = PC_RARITY_RARE;
        for (auto id : prefixes) put(root,*session,id);
        const auto craft = mod(*session,"HelenaMasterFireResist1");
        if (control != 3) put(root,*session,craft,PC_MOD_SLOT_CRAFTED | (control == 1 ? PC_MOD_SLOT_FRACTURED : 0));
        if (control == 2) goal.slots.push_back(target(*session,{craft}).slots.front());
        CalcContext calc(session,goal,registry,natural_candidates,false,false,true); const auto state = calc.intern_item(root);
        const auto batch = calc.admit_state_local_automatic_candidates(state,limits);
        PC_CHECK(batch.admitted_operators.size() == (control == 0 ? 1 : 0));
        if (control == 0 && !batch.admitted_operators.empty()) {
            const auto& kernel = calc.option_kernel(state,batch.admitted_operators.front());
            PC_CHECK(kernel.legal && kernel.exits.size() == 1 && calc.is_goal_state(calc.state(kernel.exits.front().state)));
            PC_CHECK(kernel.expected_resources.size() == 1 && kernel.expected_resources.front().first == "scour" && kernel.expected_resources.front().second == 1);
            // A cheap completion never masquerades as a full family cache.
            auto full = limits; full.cheap_programs_only = false;
            PC_CHECK(!calc.admit_state_local_automatic_candidates(state,full).cached);
            PC_CHECK(calc.admit_state_local_automatic_candidates(state,full).cached);
            // Both admission stages survive the native checkpoint boundary.
            // Admit only the one-step cleanup, so the replay fixture has no
            // stochastic search or extra qualification invocation.
            CalcContext saved(session,goal,registry,{},false,false,true);
            const auto saved_state = saved.intern_item(root);
            SolveWork work(saved,root,prices);
            for (unsigned step = 0; step < 1000 && !work.progress().done; ++step) work.step(8);
            PC_CHECK(work.progress().done);
            const auto saved_cheap = saved.admit_state_local_automatic_candidates(saved_state,limits);
            const auto saved_full = saved.admit_state_local_automatic_candidates(saved_state,full);
            const auto path = std::filesystem::temp_directory_path() /
                "poecraft-metamod-staged-admission.pcsg";
            saved.save_development_solve_checkpoint(path.string(),"metamod-staged-admission");
            CalcContext replay(session,goal,registry,{},false,false,true);
            bool loaded = false;
            try {
                replay.load_development_solve_checkpoint(path.string(),"metamod-staged-admission");
                loaded = true;
            } catch (const std::exception& error) {
                std::printf("staged admission replay: %s\n",error.what());
            }
            PC_CHECK(loaded);
            if (loaded) {
                const auto cheap_replay = replay.admit_state_local_automatic_candidates(saved_state,limits);
                const auto full_replay = replay.admit_state_local_automatic_candidates(saved_state,full);
                PC_CHECK(cheap_replay.cached && full_replay.cached);
                PC_CHECK(cheap_replay.admitted_operators == saved_cheap.admitted_operators);
                PC_CHECK(full_replay.admitted_operators == saved_full.admitted_operators);
            }
            std::filesystem::remove(path);
        }
    }

    // Choose an actual native compatible bench pair, then compare the same
    // physical programme under clean and coverage terminal predicates.
    bool multimod_pair_checked = false;
    for (auto left : session->bench_mod_ids) {
        if (session->metamod_type.at(left) >= 0) continue;
        for (auto right : session->bench_mod_ids) {
            if (right <= left || session->metamod_type.at(right) >= 0 || mods_conflict(*session,left,right)) continue;
            auto goal = target(*session,{left,right});
            goal.automatic_candidate_kind_mask = automatic_candidate_kind_bit(AutomaticCandidateKind::MultimodFinish);
            for (bool coverage : {true,false}) {
                goal.terminal.extras = coverage ? ExtraExplicitPolicy::Allow : ExtraExplicitPolicy::ForbidUnmatched;
                std::vector<std::uint32_t> candidates{registry.index_by_id.at("bench:" + data->string_at(data->mod_key_sid[session->global_index[left]])), registry.index_by_id.at("bench:" + data->string_at(data->mod_key_sid[session->global_index[right]]))};
                CalcContext calc(session,goal,registry,candidates);
                pc_item_state root; pc_item_clear(&root); root.rarity = PC_RARITY_RARE;
                const auto state = calc.intern_item(root);
                const auto batch = calc.admit_state_local_automatic_candidates(state,limits);
                if (coverage && batch.admitted_operators.empty()) break;
                PC_CHECK(!batch.admitted_operators.empty() == coverage);
                if (coverage) {
                    const auto& kernel = calc.option_kernel(state,batch.admitted_operators.front());
                    PC_CHECK(kernel.legal && kernel.exits.size() == 1 && calc.is_goal_state(calc.state(kernel.exits.front().state)));
                    multimod_pair_checked = true;
                }
            }
            if (multimod_pair_checked) break;
        }
        if (multimod_pair_checked) break;
    }
    PC_CHECK(multimod_pair_checked);

    // Finite cap regressions exercise actual first-policy ownership, rather
    // than waiting for a full timed solve. A completed cheap row precedes the
    // ordinary Chaos kernel's deliberately insufficient work allowance.
    for (bool crafted : {false,true}) {
        auto goal = target(*session,prefixes);
        goal.automatic_candidate_kind_mask = automatic_candidate_kind_bit(crafted ? AutomaticCandidateKind::CraftedCleanup : AutomaticCandidateKind::ProtectedMetamod);
        pc_item_state root; pc_item_clear(&root); root.rarity = PC_RARITY_RARE;
        for (auto id : prefixes) put(root,*session,id);
        if (crafted) put(root,*session,mod(*session,"HelenaMasterFireResist1"),PC_MOD_SLOT_CRAFTED);
        else for (auto id : suffixes) put(root,*session,id);
        CalcContext calc(session,goal,registry,natural_candidates,false,false,true);
        SolveOptions options; apply_solve_profile_defaults(options,SolveProfile::CalculatorProductV1);
        options.consider_imprint_programs = false;
        options.max_states = options.max_discovered_states = options.max_expanded_states = 256;
        options.max_reforge_work = 1;
        options.max_solver_owned_bytes = 64ull << 20;
        SolveWork work(calc,root,prices,options);
        unsigned steps = 0; while (!work.progress().done && ++steps < 20000) work.step(1);
        PC_CHECK(work.progress().done);
        if (work.progress().done) {
            const auto result = work.finish();
            std::printf("Current finite cleanup crafted=%d termination=%u L=%.9g U=%.9g evaluated=%.9g incumbent=%s\n", crafted,
                static_cast<unsigned>(result.termination), result.lower_bound, result.upper_bound, result.evaluated_policy_cost, result.diagnostics.incumbent_kind.c_str());
            std::ofstream(crafted ? "out/metamod-w4-progress.json" : "out/metamod-w1-progress.json") << work.progress_trace_json(0);
            const double oracle = crafted ? 0.3741 : 424.3741;
            PC_CHECK(result.policy_available);
            PC_CHECK(std::abs(result.evaluated_policy_cost - oracle) < 1e-7);
            PC_CHECK(result.lower_bound <= oracle + 1e-7);
            PC_CHECK(result.policy_status != SolvePolicyStatus::Exact);
        }
    }
    // A nonterminal protected preparation is a complete row, not a root upper.
    {
        auto bow = std::make_shared<SessionImpl>(); bow->data = data;
        bow->base_index = data->base_by_path.at("Metadata/Items/Weapons/TwoHandWeapons/Bows/Bow20"); bow->item_level = 86;
        build_session(*bow);
        std::vector<std::uint32_t> held;
        for (auto key : {"LocalIncreaseSocketedGemLevel1","LocalAddedPhysicalDamageTwoHand9","LocalAddedColdDamageTwoHand10"}) held.push_back(mod(*bow,key));
        auto wanted = held; wanted.push_back(mod(*bow,"ManaGainedFromEnemyDeath6"));
        auto goal = target(*bow,wanted);
        goal.automatic_candidate_kind_mask = automatic_candidate_kind_bit(AutomaticCandidateKind::ProtectedMetamod);
        auto registry = build_action_registry(*bow,build);
        auto prices = std::unordered_map<std::string,double>{};
        for (const auto& a : registry.actions) for (const auto& key : a.cost_keys) prices[key] = 1;
        pc_item_state root; pc_item_clear(&root); root.rarity = PC_RARITY_RARE;
        for (auto id : held) put(root,*bow,id);
        for (auto key : {"Dexterity7","LocalIncreasedAttackSpeed3"}) put(root,*bow,mod(*bow,key));
        CalcContext calc(bow,goal,registry,{registry.index_by_id.at("chaos")},false,false,true);
        {
            CalcContext diagnostic(bow,goal,registry,{registry.index_by_id.at("chaos")},false,false,true);
            const auto state = diagnostic.intern_item(root);
            AutomaticAdmissionLimits limits; limits.cheap_programs_only = true; limits.prices = &prices;
            limits.consider_imprint_programs = false; limits.max_solver_owned_bytes = 64ull << 20;
            const auto batch = diagnostic.admit_state_local_automatic_candidates(state,limits);
            std::printf("Bow root mask=%u cheap=%zu\n", satisfied_goal_mask(diagnostic.state(state)),batch.admitted_operators.size());
            for (const auto& d : batch.decisions) std::printf("Bow proposal %s %s\n",d.id.c_str(),d.evidence.reason.c_str());
        }
        SolveOptions options; apply_solve_profile_defaults(options,SolveProfile::CalculatorProductV1);
        options.consider_imprint_programs = false;
        options.max_reforge_work = 1; options.max_solver_owned_bytes = 64ull << 20;
        SolveWork work(calc,root,prices,options);
        unsigned steps=0; while (!work.progress().done && ++steps<20000) work.step(1);
        PC_CHECK(work.progress().done);
        const auto result = work.finish();
        PC_CHECK(result.diagnostics.sparse_rows >= 1);
        PC_CHECK(!result.policy_available);
        std::ofstream("out/metamod-bow-first-failure.json") << work.progress_trace_json(0);
    }

    for (unsigned control=0;control<5;++control) {
        auto goal = target(*session,prefixes);
        goal.automatic_candidate_kind_mask = automatic_candidate_kind_bit(AutomaticCandidateKind::ProtectedMetamod);
        pc_item_state root; pc_item_clear(&root); root.rarity=PC_RARITY_RARE;
        for (auto id : prefixes) put(root,*session,id);
        put(root,*session,suffixes.front(),control==1 ? PC_MOD_SLOT_FRACTURED : 0);
        if (control==0) put(root,*session,mod(*session,"HelenaMasterFireResist1"),PC_MOD_SLOT_CRAFTED);
        if (control==2) goal.disabled_action_families |= solver_action_family_bit(SolverActionFamily::Bench);
        if (control==3) goal.disabled_action_families |= solver_action_family_bit(SolverActionFamily::Metamod);
        auto scoped_prices=prices;
        if (control==4) {
            for (const auto& action : registry.actions)
                if (action.sets_flags & kFlagPrefixesLocked)
                    for (const auto& key : action.cost_keys) scoped_prices.erase(key);
        }
        auto admission=limits; admission.prices=&scoped_prices;
        CalcContext calc(session,goal,registry,natural_candidates,false,false,true);
        const auto state=calc.intern_item(root);
        const auto batch=calc.admit_state_local_automatic_candidates(state,admission);
        // With only an opposite fracture, the programme changes no unwanted
        // affix and is irrelevant; it cannot erase that fracture to finish.
        PC_CHECK(batch.admitted_operators.empty());
    }
    // Expected Finish and ungated exhaustion without any checked incumbent.
    for (bool finish : {false,true}) {
        auto goal=target(*session,prefixes); goal.automatic_candidates=false;
        CalcContext calc(session,goal,registry,natural_candidates,false,false,true);
        pc_item_state root; pc_item_clear(&root); root.rarity=PC_RARITY_RARE;
        SolveOptions options; options.goal_progress_gated_reforges=false;
        options.max_reforge_work=1; options.max_solver_owned_bytes=64ull<<20;
        SolveWork work(calc,root,prices,options);
        if (finish) work.request_bounded_finish();
        unsigned steps=0; while (!work.progress().done && ++steps<20000) work.step(1);
        PC_CHECK(work.progress().done);
        const auto result=work.finish();
        PC_CHECK(!result.policy_available);
        PC_CHECK(result.termination==(finish ? SolveTermination::RequestedBoundedFinish : SolveTermination::RefusedResourceCap));
    }

}
