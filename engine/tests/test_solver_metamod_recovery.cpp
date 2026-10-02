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

namespace poecraft::solver {
struct SolveWorkTestAccess {
    using Impl = SolveWork::Impl;
    static Impl& get(SolveWork& work) { return *work.impl_; }
};
}

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
    // Stop with a complete native cleanup candidate, then exercise the exact
    // publication boundary with open obligations and each orthogonal stop.
    // No stochastic continuation or timed search is run here.
    for (unsigned control=0;control<4;++control) {
        auto goal=target(*session,prefixes);
        goal.automatic_candidate_kind_mask=automatic_candidate_kind_bit(AutomaticCandidateKind::ProtectedMetamod);
        CalcContext calc(session,goal,registry,natural_candidates,false,false,true);
        pc_item_state root; pc_item_clear(&root); root.rarity=PC_RARITY_RARE;
        for (auto id : prefixes) put(root,*session,id);
        for (auto id : suffixes) put(root,*session,id);
        SolveOptions options; apply_solve_profile_defaults(options,SolveProfile::CalculatorProductV1);
        options.consider_imprint_programs=false; options.max_reforge_work=1; options.max_solver_owned_bytes=64ull<<20;
        SolveWork work(calc,root,prices,options);
        auto& impl=SolveWorkTestAccess::get(work);
        unsigned steps=0;
        while (!work.progress().done && ++steps<2000 &&
               !impl.output_incumbent) work.step(1);
        PC_CHECK(impl.output_incumbent.has_value());
        if (!impl.output_incumbent) continue;
        impl.phase=SolvePhase::Done;
        impl.incremental_action_generation=true;
        impl.incremental_envelope_closed=false;
        impl.incremental_unevaluated_actions=1;
        impl.expansion_active=false;
        impl.result.diagnostics.resource_cap_hit=false;
        impl.result.diagnostics.state_cap_hit=false;
        impl.result.diagnostics.cap_hits.clear();
        impl.requested_bounded_finish=control==2;
        impl.numerical_stability_stop=control==3;
        if (control==1) impl.record_cap("max_reforge_work");
        impl.publication_pipeline.initial_candidate_task.reset();
        impl.finalization_task.reset();
        impl.finalized_result.reset();
        impl.options.high_impact_executable_uppers=false;
        impl.begin_publication_pipeline();
        while (!impl.finalized_result && ++steps<20000) impl.advance_publication_pipeline();
        PC_CHECK(work.progress().done);
        if (!work.progress().done) continue;
        const auto result=work.finish();
        const SolveTermination expected=control==0 ? SolveTermination::BoundedEnvelopeIncomplete :
            control==1 ? SolveTermination::RefusedResourceCap : control==2 ? SolveTermination::RequestedBoundedFinish : SolveTermination::NumericalStability;
        std::printf("W1 publication control=%u termination=%u steps=%u\n",control,static_cast<unsigned>(result.termination),steps);
        PC_CHECK(result.termination==expected);
        PC_CHECK(result.policy_available && result.policy_status==SolvePolicyStatus::BoundedFeasible);
        PC_CHECK(std::abs(result.evaluated_policy_cost-424.3741)<1e-7);
        PC_CHECK(!result.refined_policy_artifact.strategy_json.empty());
        const auto graph=compile_strategy_json(session,result.refined_policy_artifact.strategy_json.data(),result.refined_policy_artifact.strategy_json.size());
        auto economy=std::make_shared<EconomyImpl>(); economy->prices=prices;
        StrategyEvalOptions evaluation; evaluation.economy=economy;
        const auto checked=evaluate_strategy(*graph,evaluation);
        PC_CHECK(checked.converged && checked.cost_complete && checked.success_probability>1-1e-10);
        PC_CHECK(std::abs(checked.total_expected_cost-424.3741)<1e-7);
    }
    // A zero-proof profile with complete deterministic discovery keeps its
    // distinct existing label. It has no unresolved ordinary envelope debt.
    {
        auto goal=target(*session,prefixes);
        goal.automatic_candidate_kind_mask=automatic_candidate_kind_bit(AutomaticCandidateKind::CraftedCleanup);
        CalcContext calc(session,goal,registry,{},false,false,true);
        pc_item_state root; pc_item_clear(&root); root.rarity=PC_RARITY_RARE;
        for (auto id : prefixes) put(root,*session,id);
        put(root,*session,mod(*session,"HelenaMasterFireResist1"),PC_MOD_SLOT_CRAFTED);
        SolveOptions options; apply_solve_profile_defaults(options,SolveProfile::CalculatorProductV1);
        options.goal_proof_profile=GoalProofProfile::TargetNeutralZero;
        options.consider_imprint_programs=false; options.max_solver_owned_bytes=64ull<<20;
        SolveWork work(calc,root,prices,options);
        unsigned steps=0; while (!work.progress().done && ++steps<2000) work.step(1);
        PC_CHECK(work.progress().done);
        const auto result=work.finish();
        PC_CHECK(result.policy_available);
        PC_CHECK(result.termination==SolveTermination::BoundedDiscoveryComplete);
        PC_CHECK(!result.diagnostics.resource_cap_hit && result.lower_bound==0);
    }
    // An open envelope without a retained candidate cannot gain the new
    // bounded-policy label at the same publication boundary.
    {
        auto goal=target(*session,prefixes); goal.automatic_candidates=false;
        CalcContext calc(session,goal,registry,natural_candidates,false,false,true);
        pc_item_state root; pc_item_clear(&root); root.rarity=PC_RARITY_RARE;
        SolveWork work(calc,root,prices);
        auto& impl=SolveWorkTestAccess::get(work);
        impl.phase=SolvePhase::Done; impl.expansion_active=false;
        impl.incremental_action_generation=true; impl.incremental_envelope_closed=false;
        impl.incremental_unevaluated_actions=1;
        impl.begin_publication_pipeline();
        unsigned steps=0; while (!impl.finalized_result && ++steps<2000) impl.advance_publication_pipeline();
        PC_CHECK(work.progress().done);
        const auto result=work.finish();
        PC_CHECK(!result.policy_available && result.refined_policy_artifact.strategy_json.empty());
        PC_CHECK(result.termination==SolveTermination::NoExecutablePolicy);
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
        prices["scour"]=0.3741; prices["exalt"]=1.77;
        for (const auto& a : registry.actions)
            if (a.params.type==ActionType::Bench && bow->metamod_type.at(a.params.mod_id)==data->metamod_prefixes_locked_code)
                for (const auto& key : a.cost_keys) prices[key]=424;
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
        // The product parent's broad class includes source-only modifiers.
        // Refining it for admission must preserve the complete class universe.
        CalcContext parent(bow,goal,registry,{registry.index_by_id.at("chaos")},false,false,false,std::nullopt,{},true);
        const auto parent_state=parent.intern_item(root);
        pc_item_state carrier;
        PC_CHECK(parent.materialize(parent_state,carrier));
        std::vector<std::uint64_t> universe(bow->words,0);
        for (const auto& slot : parent.layout().slots) pc_bitset_or(universe.data(),universe.data(),slot.member_mask.data(),bow->words);
        for (const auto& junk : parent.layout().junk_classes) pc_bitset_or(universe.data(),universe.data(),junk.member_mask.data(),bow->words);
        auto local_goal=goal; local_goal.automatic_candidates=false;
        FixedOptionSpec spec; spec.kind=FixedOptionKind::ProtectedSide; spec.side=PC_SIDE_PREFIX; spec.action_id="scour";
        local_goal.fixed_options={spec};
        CalcContext omitted(bow,local_goal,registry,{registry.index_by_id.at("chaos")},false,false,true);
        const auto omitted_state=omitted.intern_item(carrier);
        pc_item_state local_item;
        PC_CHECK(!omitted.materialize(omitted_state,local_item));
        for (unsigned i=0;i<carrier.suffix_count;++i) {
            const auto id=carrier.suffixes[i].mod_id;
            if (omitted.layout().junk_class_by_mod.at(id)==kNoId)
                std::printf("Bow source-only suffix=%s spawn_positive=%d normal_roll=%d\n",data->string_at(data->mod_key_sid.at(bow->global_index.at(id))).c_str(),pc_bitset_test(bow->positive_spawn_weight_mask.data(),id),pc_bitset_test(bow->normal_random_roll_mask.data(),id));
        }
        CalcContext local(bow,local_goal,registry,{registry.index_by_id.at("chaos")},false,false,true,std::nullopt,{},false,universe);
        const auto local_state=local.intern_item(carrier);
        PC_CHECK(local.materialize(local_state,local_item));
        PC_CHECK(project_item(*bow,parent.layout(),local_item)==parent.state(parent_state));
        auto admission=limits; admission.prices=&prices;
        StateLocalAutomaticBatch batch;
        unsigned checkpoints=0;
        while (!parent.advance_state_local_automatic_candidates(parent_state,admission,batch,1) && ++checkpoints<2000) {}
        PC_CHECK(checkpoints<2000 && checkpoints>1);
        bool cleanup=false;
        for (auto index : batch.admitted_operators) {
            const auto& op=parent.operators().at(index);
            if (op.option_kind!=FixedOptionKind::ProtectedSide || op.followup_action_id!="scour" || op.intended_side!=PC_SIDE_PREFIX) continue;
            cleanup=true;
            const auto& law=parent.option_kernel(parent_state,index);
            PC_CHECK(law.legal && law.supported && law.exits.size()==1);
            if (law.exits.size()==1) {
                const auto& exit=parent.state(law.exits.front().state);
                PC_CHECK(exit.prefix_count==3 && exit.suffix_count==0 && satisfied_goal_mask(exit)==7);
                PC_CHECK(!parent.is_goal_state(exit));
            }
        }
        PC_CHECK(cleanup);
        const auto cached=parent.admit_state_local_automatic_candidates(parent_state,admission);
        PC_CHECK(cached.cached && cached.admitted_operators==batch.admitted_operators);

        // Diagnostic native tail law only: every failed Exalt resets to the
        // same sufficient carrier through the paid lock+Scour programme.
        CalcContext cycle(bow,local_goal,registry,{registry.index_by_id.at("exalt")},false,false,true,std::nullopt,{},false,universe);
        pc_item_state clean; pc_item_clear(&clean); clean.rarity=PC_RARITY_RARE;
        for (auto id : held) put(clean,*bow,id);
        const auto clean_state=cycle.intern_item(clean);
        const auto& draws=cycle.outcomes(clean_state,registry.index_by_id.at("exalt"));
        PC_CHECK(draws.supported && draws.applicable && draws.choice_groups.empty());
        double p=0, mass=0;
        unsigned retries=0;
        double reset_cost=0;
        for (const auto& draw : draws.entries) {
            if (draw.probability<=0) continue;
            mass+=draw.probability;
            if (cycle.is_goal_state(cycle.state(draw.state))) { p+=draw.probability; continue; }
            ++retries;
            const auto& reset=cycle.option_kernel(draw.state,cycle.candidate_operators().back());
            PC_CHECK(reset.legal && reset.supported && reset.exits.size()==1);
            if (reset.exits.size()==1) PC_CHECK(reset.exits.front().state==clean_state && std::abs(reset.exits.front().probability-1)<1e-12);
            PC_CHECK(reset.expected_primitive_actions==2);
            reset_cost=0;
            double resource_count=0;
            for (const auto& [key,count] : reset.expected_resources) {
                reset_cost+=prices.at(key)*count; resource_count+=count;
                PC_CHECK(count==1);
            }
            PC_CHECK(resource_count==2 && std::abs(reset_cost-424.3741)<1e-10);
        }
        PC_CHECK(std::abs(mass-1)<1e-12 && p>0 && p<1);
        std::printf("Bow native Exalt cycle p=%.17g wrong_classes=%u; diagnostic Allflame dirty-root cost=%.12g\n",p,retries,reset_cost+(prices.at("exalt")+(1-p)*reset_cost)/p);
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

    // Repository-derived Conquest variant, not Oliver's unavailable phone
    // request: held Life + two Defence prefixes, wanted tagless Suppression
    // and Physical Reduction suffixes. These controls exercise finite native
    // rows only; they neither run an economic solve nor import an old policy.
    {
        auto body=std::make_shared<SessionImpl>(); body->data=data;
        body->base_index=data->base_by_path.at("Metadata/Items/Armours/BodyArmours/BodyStrDex20");
        body->item_level=86; build_session(*body);
        const auto registry=build_action_registry(*body,build);
        const std::vector<std::uint32_t> held{mod(*body,"IncreasedLife12"),
            mod(*body,"LocalIncreasedArmourAndEvasion8"),mod(*body,"LocalBaseArmourAndEvasionRating8")};
        const auto suppression=mod(*body,"ChanceToSuppressSpellsHigh5___");
        const auto physical=mod(*body,"AdditionalPhysicalDamageReduction5_");
        auto wanted=held; wanted.push_back(suppression); wanted.push_back(physical);
        auto goal=target(*body,wanted);
        goal.automatic_candidate_kind_mask=automatic_candidate_kind_bit(AutomaticCandidateKind::ProtectedMetamod);
        pc_item_state dirty; pc_item_clear(&dirty); dirty.rarity=PC_RARITY_RARE;
        for (auto id:held) put(dirty,*body,id);
        put(dirty,*body,mod(*body,"Dexterity7")); put(dirty,*body,mod(*body,"FireResist8"));
        CalcContext parent(body,goal,registry,{registry.index_by_id.at("chaos")},false,false,false,std::nullopt,{},true);
        const auto entry=parent.intern_item(dirty);
        auto prices=std::unordered_map<std::string,double>{};
        for (const auto& action:registry.actions) for (const auto& key:action.cost_keys) prices[key]=1;
        auto admission=limits; admission.prices=&prices;
        const auto batch=parent.admit_state_local_automatic_candidates(entry,admission);
        PC_CHECK(batch.status==StateLocalAutomaticBatchStatus::Complete);
        bool cleanup=false;
        for (auto index:batch.admitted_operators) {
            const auto& op=parent.operators().at(index);
            if (op.option_kind!=FixedOptionKind::ProtectedSide || op.intended_side!=PC_SIDE_PREFIX || op.followup_action_id!="scour") continue;
            const auto& law=parent.option_kernel(entry,index);
            PC_CHECK(law.legal && law.supported && law.exits.size()==1);
            if (law.exits.size()==1) {
                const auto& exit=parent.state(law.exits.front().state);
                cleanup=exit.prefix_count==3 && exit.suffix_count==0 && satisfied_goal_mask(exit)==7;
                PC_CHECK(!parent.is_goal_state(exit));
            }
        }
        PC_CHECK(cleanup);
        std::vector<std::uint64_t> universe(body->words,0);
        for (const auto& slot:parent.layout().slots) pc_bitset_or(universe.data(),universe.data(),slot.member_mask.data(),body->words);
        for (const auto& junk:parent.layout().junk_classes) pc_bitset_or(universe.data(),universe.data(),junk.member_mask.data(),body->words);
        goal.automatic_candidates=false;
        FixedOptionSpec reset; reset.kind=FixedOptionKind::ProtectedSide;
        reset.side=PC_SIDE_PREFIX; reset.action_id="scour"; goal.fixed_options={reset};
        const std::vector<std::uint32_t> primitives{registry.index_by_id.at("exalt"),registry.index_by_id.at("annul"),registry.index_by_id.at("eldritch_annul")};
        CalcContext finite(body,goal,registry,primitives,false,false,true,std::nullopt,{},false,universe);
        pc_item_state clean; pc_item_clear(&clean); clean.rarity=PC_RARITY_RARE;
        for (auto id:held) put(clean,*body,id);
        const auto clean_state=finite.intern_item(clean);
        const auto& draw=finite.outcomes(clean_state,registry.index_by_id.at("exalt"));
        PC_CHECK(draw.applicable && draw.supported && draw.choice_groups.empty());
        double mass=0,suppression_hit=0,physical_hit=0;
        for (const auto& e:draw.entries) {
            mass+=e.probability; const auto mask=satisfied_goal_mask(finite.state(e.state));
            PC_CHECK((mask&7)==7 && !finite.is_goal_state(finite.state(e.state)));
            if (mask&8) suppression_hit+=e.probability;
            if (mask&16) physical_hit+=e.probability;
        }
        PC_CHECK(std::abs(mass-1)<1e-12 && suppression_hit>0 && physical_hit>0);
        auto progress=clean; put(progress,*body,suppression); put(progress,*body,mod(*body,"Dexterity7"));
        progress.eater_of_worlds_tier=2; progress.searing_exarch_tier=1;
        const auto progress_state=finite.intern_item(progress);
        double ordinary_prefix_loss=0,eldritch_prefix_loss=0,eldritch_good_loss=0,eldritch_junk_loss=0;
        for (const char* action:{"annul","eldritch_annul"}) {
            const auto& law=finite.outcomes(progress_state,registry.index_by_id.at(action));
            PC_CHECK(law.applicable && law.supported && law.choice_groups.empty());
            mass=0;
            for (const auto& e:law.entries) {
                mass+=e.probability; const auto mask=satisfied_goal_mask(finite.state(e.state));
                if (std::string(action)=="annul") { if ((mask&7)!=7) ordinary_prefix_loss+=e.probability; }
                else {
                    if ((mask&7)!=7) eldritch_prefix_loss+=e.probability;
                    if (!(mask&8)) eldritch_good_loss+=e.probability;
                    else eldritch_junk_loss+=e.probability;
                }
            }
            PC_CHECK(std::abs(mass-1)<1e-12);
        }
        PC_CHECK(std::abs(ordinary_prefix_loss-0.6)<1e-12 && eldritch_prefix_loss==0);
        PC_CHECK(std::abs(eldritch_good_loss-0.5)<1e-12 && std::abs(eldritch_junk_loss-0.5)<1e-12);
        const auto& wipe=finite.option_kernel(progress_state,finite.candidate_operators().back());
        PC_CHECK(wipe.legal && wipe.supported && wipe.exits.size()==1);
        if (wipe.exits.size()==1) PC_CHECK(satisfied_goal_mask(finite.state(wipe.exits.front().state))==7);
        std::printf("Conquest mixed-tag finite: cleanup=%d first Supp=%.17g Physical=%.17g; ordinary prefix loss=%.12g eldritch prefix loss=%.12g good/junk suffix loss=%.12g/%.12g; lock+Scour erases held Supp\n",
            cleanup,suppression_hit,physical_hit,ordinary_prefix_loss,eldritch_prefix_loss,eldritch_good_loss,eldritch_junk_loss);
    }

    // Refinement conserves below-tier blockers, source-only crafted junk
    // and crafted/fractured flags on both sides. Native craft refusal remains
    // authoritative when a preexisting crafted modifier occupies the bench.
    for (int side : {PC_SIDE_PREFIX,PC_SIDE_SUFFIX}) for (unsigned control=0;control<4;++control) {
        const auto& held=side==PC_SIDE_PREFIX ? prefixes : suffixes;
        const auto& other=side==PC_SIDE_PREFIX ? suffixes : prefixes;
        auto wanted=held; wanted.push_back(other.front());
        auto goal=target(*session,wanted);
        goal.automatic_candidate_kind_mask=automatic_candidate_kind_bit(AutomaticCandidateKind::ProtectedMetamod);
        std::uint32_t lower=kNoId;
        for (std::uint32_t i=0;i<session->mod_count;++i)
            if (session->family_id[i]==session->family_id[other.front()] && session->family_tier_index[i]>goal.slots.back().min_tier) { lower=i; break; }
        PC_CHECK(lower!=kNoId); if (lower==kNoId) continue;
        pc_item_state root; pc_item_clear(&root); root.rarity=PC_RARITY_RARE;
        for (auto id : held) put(root,*session,id);
        put(root,*session,lower,control==1 ? PC_MOD_SLOT_FRACTURED : 0);
        std::uint32_t junk=other.back();
        if (control>=2) {
            junk=kNoId;
            for (auto id : session->bench_mod_ids) {
                if (session->gen_type[id]==side || session->metamod_type[id]>=0 || mods_conflict(*session,id,lower)) continue;
                if (std::any_of(held.begin(),held.end(),[&](auto h){return mods_conflict(*session,id,h);})) continue;
                junk=id; break;
            }
            PC_CHECK(junk!=kNoId); if (junk==kNoId) continue;
        }
        put(root,*session,junk,control>=2 ? PC_MOD_SLOT_CRAFTED | (control==3 ? PC_MOD_SLOT_FRACTURED : 0) : 0);
        CalcContext parent(session,goal,registry,natural_candidates,false,false,false,std::nullopt,{},true);
        const auto state=parent.intern_item(root);
        PC_CHECK(parent.state(state).slot_status[2]==static_cast<std::uint8_t>(GoalSlotStatus::PresentBelowTier));
        pc_item_state carrier; PC_CHECK(parent.materialize(state,carrier));
        std::vector<std::uint64_t> universe(session->words,0);
        for (const auto& slot : parent.layout().slots) pc_bitset_or(universe.data(),universe.data(),slot.member_mask.data(),session->words);
        for (const auto& cls : parent.layout().junk_classes) pc_bitset_or(universe.data(),universe.data(),cls.member_mask.data(),session->words);
        auto local_goal=goal; local_goal.automatic_candidates=false;
        CalcContext local(session,local_goal,registry,{},false,false,true,std::nullopt,{},false,universe);
        const auto entry=local.intern_item(carrier);
        pc_item_state rebuilt; PC_CHECK(local.materialize(entry,rebuilt));
        PC_CHECK(project_item(*session,parent.layout(),rebuilt)==parent.state(state));
        auto admission=limits; admission.prices=&prices;
        StateLocalAutomaticBatch batch;
        PC_CHECK(!parent.advance_state_local_automatic_candidates(state,admission,batch,1));
        parent.cancel_state_local_automatic_candidates(state);
        PC_CHECK(parent.automatic_admission_cursor_bytes()==0);
        batch=parent.admit_state_local_automatic_candidates(state,admission);
        bool protected_cleanup=false;
        for (auto index : batch.admitted_operators) {
            const auto& op=parent.operators().at(index);
            if (op.option_kind==FixedOptionKind::ProtectedSide && op.intended_side==side && op.followup_action_id=="scour") {
                protected_cleanup=true;
                const auto& law=parent.option_kernel(state,index);
                PC_CHECK(law.legal && law.exits.size()==1);
                if (law.exits.size()==1) {
                    const auto& exit=parent.state(law.exits.front().state);
                    PC_CHECK((satisfied_goal_mask(exit)&3)==3);
                    PC_CHECK(control!=1 || (exit.fractured_goal_mask&4)!=0);
                }
            }
        }
        PC_CHECK(protected_cleanup==(control<2));
        const auto repeated=parent.admit_state_local_automatic_candidates(state,admission);
        PC_CHECK(repeated.cached && repeated.admitted_operators==batch.admitted_operators);
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
