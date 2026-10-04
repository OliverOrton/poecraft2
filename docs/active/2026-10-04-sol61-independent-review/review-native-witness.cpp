// Isolated review fixtures. Reuse the existing finite session factory without
// adding selectors to another owner's test files or changing production code.
#include "../../../engine/tests/test_solver_compile.cpp"
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>

namespace pctest { int g_checks = 0; int g_failures = 0; }
namespace review {
using Clock = std::chrono::steady_clock;
const auto started = Clock::now();
void checkpoint() {
    if (std::chrono::duration<double>(Clock::now()-started).count() >= 45)
        throw std::runtime_error("review native watchdog (45 seconds)");
}
void require(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(reason);
}
void write(const std::filesystem::path& path, const std::string& text) {
    std::ofstream file(path,std::ios::binary); file << text;
    require(bool(file),"could not persist review evidence");
}
SolveOptions caps() {
    SolveOptions result; apply_solve_profile_defaults(result,SolveProfile::CalculatorProductV1);
    result.max_states = result.max_discovered_states = result.max_expanded_states = 10000;
    result.max_state_action_rows = 100000; result.max_transitions = 1000000;
    result.max_reforge_work = 1000000; result.max_solver_owned_bytes = 256ull << 20;
    result.consider_imprint_programs = false;
    return result;
}
GoalSpec goal(const std::shared_ptr<SessionImpl>& session) {
    GoalSpec result; result.rarity = PC_RARITY_RARE; result.automatic_candidates = true;
    result.automatic_candidate_kind_mask = automatic_candidate_kind_bit(AutomaticCandidateKind::EldritchSide);
    for (auto id : {3u,4u,5u,6u}) {
        GoalSlot slot; slot.family_id = session->family_id.at(id); slot.min_tier = 1;
        result.slots.push_back(slot);
    }
    return result;
}
std::unordered_map<std::string,double> prices() {
    std::unordered_map<std::string,double> result{{"chaos",100},{"eldritch_chaos",3},
        {"eldritch_annul",2},{"eldritch_exalt",1}};
    for (unsigned tier=1;tier<=4;++tier) {
        result["eldritch_ember:"+std::to_string(tier)] = .17*tier;
        result["eldritch_ichor:"+std::to_string(tier)] = 2.3*tier;
    }
    return result;
}
pc_item_state root() {
    pc_item_state result; pc_item_clear(&result); result.rarity = PC_RARITY_RARE; return result;
}
AttemptKernel attempt(CalcContext& calc, const std::vector<std::uint32_t>& word,
        const std::vector<OutcomeEntry>& support) {
    auto task=execute_attempt_cooperatively(calc,word,support);
    for (unsigned step=0;step<40000;++step) {
        checkpoint();
        if (task.resume()) { auto result=task.take_result(); task.reset(); return result; }
    }
    throw std::runtime_error("review attempt continuation bound");
}
// Independent one-step oracle: enumerate the native primitive once per actual
// entry and apply linearity directly; never call either attempt helper here.
void weighted_case(CalcContext& calc, const std::uint32_t action,
        const std::vector<OutcomeEntry>& support, const char* label) {
    std::map<std::uint32_t,double> expected,actual;
    std::map<std::string,double> resource;
    double mass=0;
    for (const auto& entry:support) {
        require(entry.probability>0,"witness entry must be positive"); mass+=entry.probability;
        const auto& law=calc.outcomes(entry.state,action);
        require(law.supported && law.applicable && law.choice_groups.empty() &&
            !law.entries.empty(),"weighted oracle primitive unavailable");
        double native_mass=0;
        for (const auto& exit:law.entries) {
            native_mass+=exit.probability;
            expected[exit.state]+=entry.probability*exit.probability;
        }
        PC_CHECK(std::abs(native_mass-1.0)<=1e-12);
        for (const auto& key:calc.registry().actions.at(action).cost_keys)
            resource[key]+=entry.probability;
    }
    const auto observed=attempt(calc,{action},support);
    require(observed.supported && observed.fully_legal && observed.choice_groups.empty(),
        "weighted attempt primitive unavailable");
    double observed_mass=0,expected_mass=0;
    for (const auto& exit:observed.entries) { actual[exit.state]+=exit.probability; observed_mass+=exit.probability; }
    for (const auto& [id,p]:expected) expected_mass+=p;
    std::cout<<std::setprecision(17)<<"{\"mode\":\"weighted\",\"case\":\""<<label
        <<"\",\"entry_mass\":"<<mass<<",\"oracle_exit_mass\":"<<expected_mass
        <<",\"observed_exit_mass\":"<<observed_mass
        <<",\"expected_actions\":"<<mass<<",\"observed_actions\":"<<observed.expected_primitive_actions
        <<",\"native_evaluations\":"<<observed.action_state_evaluations<<"}\n";
    // Dyadic scaling and deterministic tiny mass give exact comparisons.
    // This does not change any native probability/cost acceptance tolerance.
    PC_CHECK(actual==expected);
    PC_CHECK(observed.expected_primitive_actions==mass);
    const std::vector<std::pair<std::string,double>> expected_resources(resource.begin(),resource.end());
    PC_CHECK(observed.expected_resources==expected_resources);
}
void weighted() {
    auto session=make_compile_session(); auto registry=build_action_registry(*session);
    GoalSpec requested; requested.rarity=PC_RARITY_RARE;
    GoalSlot wanted; wanted.family_id=session->family_id.at(3); wanted.min_tier=1;
    requested.slots.push_back(wanted);
    const auto chaos=registry.index_by_id.at("chaos"),ember=registry.index_by_id.at("eldritch_ember:1");
    // Repeated identical cost keys are fixture descriptors, not production edits.
    registry.actions.at(ember).cost_keys.push_back(registry.actions.at(ember).cost_keys.front());
    CalcContext calc(session,requested,registry,{chaos,ember},false,false,false,
        std::nullopt,{},false,{},true);
    calc.set_solve_resource_caps(10000,1000000,false,256ull<<20);
    auto item=root(); const auto empty=calc.intern_item(item);
    require(pc_item_add_mod(&item,PC_SIDE_PREFIX,3,session->primary_group.at(3),0,nullptr)==PC_RESULT_OK,
        "could not construct distinct renewal entry");
    const auto occupied=calc.intern_item(item);
    weighted_case(calc,ember,{{empty,1.0}},"unit_deterministic_control");
    weighted_case(calc,ember,{{empty,.125}},"nonunit_deterministic_repeated_keys");
    weighted_case(calc,ember,{{empty,1e-18}},"tiny_positive_deterministic");
    std::vector<std::uint64_t> a,b;
    require(calc.exact_reforge_kernel_signature(empty,chaos,a) &&
        calc.exact_reforge_kernel_signature(occupied,chaos,b) && a==b,
        "shared renewal witness does not have identical native signatures");
    weighted_case(calc,chaos,{{empty,.5},{occupied,.5}},"unit_shared_renewal_control");
    weighted_case(calc,chaos,{{empty,.0625},{occupied,.0625}},"nonunit_shared_renewal");
}
void finder_generation(const std::filesystem::path& out) {
    auto session=make_compile_session(); const auto registry=build_action_registry(*session);
    auto requested=goal(session); const auto chaos=registry.index_by_id.at("chaos");
    CalcContext calc(session,requested,registry,{chaos},false,false,false,std::nullopt,{},false,{},true);
    auto limits=caps(); limits.max_reforge_work=50;
    const auto item=root(); bool reached=false,over=false;
    std::uint64_t actual=0,reported=0,ordinary=0,automatic=0;
    std::string telemetry;
    {
        // Ordinary product constructor, default grammar/ranking/eight attempts.
        // Match the public Finder path: do not attach a private budget owner.
        PolicyFinderWork finder(calc,session,item,prices(),limits);
        require(calc.reforge_work_budget_owner()==nullptr,"unexpected Finder budget owner");
        for (unsigned step=0;step<40000 && !finder.progress().done;++step) {
            checkpoint(); finder.step(1);
            ordinary=calc.telemetry().reforge_logical_work_v1;
            automatic=calc.telemetry().automatic_admission_reforge_logical_work_v1;
            actual=ordinary+automatic; reported=finder.progress().logical_reforge_work;
            reached|=automatic>0; over|=actual>limits.max_reforge_work;
            if (over) break;
        }
        telemetry=finder.telemetry_json(); write(out/"finder-generation.telemetry.json",telemetry);
        const auto report=json::Parser(telemetry.data(),telemetry.size()).parse();
        require(report.at("grammar").as_string()=="conditional-protected-scour",
            "witness did not use the ordinary product grammar");
        finder.request_bounded_finish(); finder.step(1);
        // Destroy any pending admission while its borrowed Finder prices are
        // still alive. Cancellation must never refund committed native work.
        const auto before_ordinary=calc.telemetry().reforge_logical_work_v1;
        const auto before_auto=calc.telemetry().automatic_admission_reforge_logical_work_v1;
        calc.cancel_state_local_automatic_candidates();
        PC_CHECK(calc.telemetry().reforge_logical_work_v1==before_ordinary);
        PC_CHECK(calc.telemetry().automatic_admission_reforge_logical_work_v1==before_auto);
    }
    std::cout<<"{\"mode\":\"finder-generation\",\"cap\":"<<limits.max_reforge_work
        <<",\"ordinary_work\":"<<ordinary<<",\"automatic_work\":"<<automatic
        <<",\"actual_generation_work\":"<<actual<<",\"reported_finder_work\":"<<reported
        <<",\"native_admission_reached\":"<<(reached?"true":"false")
        <<",\"generation_exceeded_cap\":"<<(over?"true":"false")<<"}\n";
    require(reached,"generation witness never reached automatic admission");
    PC_CHECK(actual<=limits.max_reforge_work);
    PC_CHECK(reported==actual);
}
void validator_budget(const std::filesystem::path& out) {
    auto session=make_compile_session(); const auto registry=build_action_registry(*session);
    const auto requested=goal(session); const auto chaos=registry.index_by_id.at("chaos");
    CalcContext source(session,requested,registry,{chaos},false,false,false,std::nullopt,{},false,{},true);
    const auto limits=caps(); source.set_solve_resource_caps(10000,1000000,false,256ull<<20);
    const auto item=root(); const auto cost=prices();
    SelectiveCompletionProducer producer(source,item,cost,limits,SelectiveCompletionVariant::RetentionControl);
    for (unsigned step=0;step<40000 && !producer.done();++step) { checkpoint(); producer.advance(1); }
    require(producer.done() && producer.candidate().has_value(),"native census proposal not constructed");
    const auto& control=producer.candidate()->control;
    const auto graph=compile_finder_control_json(source,item,control,limits);
    const auto prepared=prepare_finder_candidate(source,session,item,graph,&control);
    require(prepared.ready(),"native census graph is out of original request scope");
    write(out/"validator-original-root.strategy.json",graph);
    auto economy=std::make_shared<EconomyImpl>(); economy->prices=cost;
    StrategyEvalOptions eval; eval.economy=economy; eval.epsilon=1e-12;
    eval.max_states=10000; eval.max_pairs=100000; eval.max_transitions=1000000;
    eval.max_owned_bytes=256ull<<20; eval.max_reforge_work=1000000;
    eval.continuation_entries.push_back({source.intern_item(item),0,1,item,false});
    eval.graph_local_provenance.strategy_json=graph;
    for (unsigned node=0;node<control.nodes.size();++node) {
        const auto& cn=control.nodes.at(node); if (cn.kind!=FinderControlKind::RunNativeProgram) continue;
        const auto& binding=control.programs.at(cn.binding);
        const auto key=finder_program_occurrence_key(source,binding); const auto id="c"+std::to_string(node);
        eval.graph_local_provenance.decisions.push_back({id,key,false,false});
        StrategyPolicyDecisionRequest occurrence; occurrence.compiled_node_id=id;
        occurrence.selected_operator_identity=key; occurrence.graph_local=true;
        eval.policy_decision_entries.push_back(std::move(occurrence));
    }
    StrategyEvalWork checker(prepared.strategy,eval);
    for (unsigned step=0;step<40000 && !checker.progress().done;++step) { checkpoint(); checker.step(1); }
    require(checker.progress().done && finder_evaluation_accepted(checker.result()),
        "native census original-root evaluation failed");
    // Own the complete immutable native result before releasing the checker.
    const auto checked=checker.take_result(); const auto& census=checked.policy_entries;
    require(census.requested && !census.entries.empty() && !census.refused_entries,
        "native census incomplete");
    std::ostringstream receipt; receipt<<std::setprecision(17)<<"{\"cost\":"<<checked.total_expected_cost
        <<",\"success\":"<<checked.success_probability<<",\"entries\":[";
    for (unsigned i=0;i<census.entries.size();++i) {
        const auto& entry=census.entries.at(i); require(entry.available() && entry.root_expected_visits>0,
            "native census has a nonpositive or unavailable entry");
        if(i)receipt<<','; receipt<<"{\"node\":\""<<entry.compiled_node_id<<"\",\"visits\":"
            <<entry.root_expected_visits<<",\"exact_item_key\":[";
        const auto key=exact_item_state_key(entry.item);
        for(unsigned j=0;j<key.size();++j) { if(j)receipt<<',';receipt<<key[j]; }
        receipt<<"]}";
    }
    receipt<<"]}";write(out/"validator-native-census.json",receipt.str());
    auto tiny=limits; tiny.max_reforge_work=1;
    bool unowned_refused=false,unowned_done=false; std::uint64_t unowned_work=0;
    {
        SelectiveProgrammeEntryValidator validator(source,session,control,census,cost,tiny);
        try {
            for(unsigned step=0;step<40000 && !validator.done();++step) { checkpoint();validator.advance(1); }
        } catch(const SolverResourceLimit& limit) {
            unowned_refused=limit.cap_name()=="max_reforge_work";
        }
        unowned_done=validator.done();unowned_work=validator.logical_work();
        if (unowned_done) PC_CHECK(validator.validated_entries()==census.entries.size());
    }
    // Same immutable root/census/word/prices with a genuinely exhausted owner.
    CalcContext owner(session,requested,registry,{chaos});
    owner.set_solve_resource_caps(10000,1,false,256ull<<20);owner.consume_reforge_work(1,1);
    source.set_reforge_work_budget_owner(&owner);
    bool owned_refused=false;std::uint64_t owned_work=0;
    {
        SelectiveProgrammeEntryValidator validator(source,session,control,census,cost,tiny);
        try {
            for(unsigned step=0;step<40000 && !validator.done();++step) { checkpoint();validator.advance(1); }
        } catch(const SolverResourceLimit& limit) { owned_refused=limit.cap_name()=="max_reforge_work"; }
        owned_work=validator.logical_work();
    }
    source.set_reforge_work_budget_owner(nullptr);
    std::cout<<"{\"mode\":\"validator-budget\",\"cap\":1,\"census_entries\":"<<census.entries.size()
        <<",\"unowned_complete\":"<<(unowned_done?"true":"false")
        <<",\"unowned_refused\":"<<(unowned_refused?"true":"false")<<",\"unowned_work\":"<<unowned_work
        <<",\"exhausted_owner_refused\":"<<(owned_refused?"true":"false")
        <<",\"owned_child_work\":"<<owned_work<<",\"committed_owner_work\":"
        <<owner.telemetry().reforge_logical_work_v1<<"}\n";
    require(unowned_done || unowned_refused,"unowned validator continuation bound");
    PC_CHECK(owned_refused && owned_work==0 && owner.telemetry().reforge_logical_work_v1==1);
    PC_CHECK(unowned_refused && unowned_work<=tiny.max_reforge_work);
}
}
int main(int argc,char** argv) {
    try {
        review::require(argc==3,"usage: review-native-witness MODE OUTPUT_DIR");
        const std::string mode=argv[1];const std::filesystem::path out=argv[2];
        std::filesystem::create_directories(out);
        if(mode=="weighted")review::weighted();
        else if(mode=="finder-generation")review::finder_generation(out);
        else if(mode=="validator-budget")review::validator_budget(out);
        else throw std::runtime_error("unknown review witness mode");
        std::cout<<"{\"mode\":\""<<mode<<"\",\"checks\":"<<pctest::g_checks
            <<",\"contract_failures\":"<<pctest::g_failures<<"}\n";
        return pctest::g_failures?1:0;
    } catch(const std::exception& error) {
        std::cout<<"review setup/native refusal: "<<error.what()<<'\n';return 2;
    }
}
