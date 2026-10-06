#include "tests.hpp"
#include "../src/recombination_solver.hpp"
#include "../src/recombination_constraints.hpp"
#include "../src/handles_internal.hpp"
#include "../src/json.hpp"
#include "poecraft/recombination_solver.h"
#include <cmath>
#include <cstring>
#include <limits>
#include <cstdio>

namespace {
void run_blocking_witnesses() {
    using namespace poecraft;
    using Pools = std::array<std::vector<RecombOccurrence>,2>;
    const auto ordinary = [](unsigned input,unsigned slot,unsigned key) { return RecombOccurrence{
        uint8_t(input),uint8_t(slot),key,key,100,{key},false}; };
    const auto exclusive = [&](unsigned input,unsigned slot,unsigned key) { auto o=ordinary(input,slot,key); o.exclusive=true; return o; };
    const auto event = [&](const Pools& pools,unsigned first,const std::function<bool(const RecombJointOutcome&)>& accepts) {
        double mass=0,success=0;
        for (const auto& o : enumerate_random_recomb_joint(pools,first)) {
            mass+=o.probability; if (accepts(o)) success+=o.probability;
            unsigned exclusives=0;
            for (auto i : o.prefixes.occurrences) exclusives+=pools[0][i].exclusive;
            for (auto i : o.suffixes.occurrences) exclusives+=pools[1][i].exclusive;
            PC_CHECK(exclusives<=1);
        }
        PC_CHECK(std::abs(mass-1)<1e-10); return success;
    };
    Pools pools;
    pools[0]={ordinary(0,0,10),exclusive(0,1,11),exclusive(0,2,12),exclusive(1,0,13)};
    const auto retains_p=[](const auto& o) { return std::find(o.prefixes.occurrences.begin(),o.prefixes.occurrences.end(),0)!=o.prefixes.occurrences.end(); };
    PC_CHECK(std::abs(event(pools,0,retains_p)-.49975)<1e-10); // RB2 pooled count=2, four candidates
    pools[0]={ordinary(0,0,10),exclusive(1,0,11)};
    pools[1]={ordinary(0,0,20),exclusive(1,0,21)};
    const auto both=[&](const auto& o) { return retains_p(o) && std::find(o.suffixes.occurrences.begin(),o.suffixes.occurrences.end(),0)!=o.suffixes.occurrences.end(); };
    for (unsigned first=0;first<2;++first) PC_CHECK(std::abs(event(pools,first,both)-.55527775)<1e-10); // RB4 counts first
    pools[0]={ordinary(0,0,10),ordinary(0,1,11),ordinary(0,2,12),exclusive(1,0,13),exclusive(1,1,14),exclusive(1,2,15)};
    pools[1]={exclusive(0,0,20),exclusive(1,0,21)};
    const auto triple=[](const auto& o) { return o.prefixes.occurrences==std::vector<unsigned>{0,1,2}; };
    PC_CHECK(std::abs(event(pools,0,triple)-.015)<1e-10);
    PC_CHECK(std::abs(event(pools,1,triple)-.18315)<1e-10); // RB5 order-sensitive
    pools[0]={ordinary(0,0,10),exclusive(1,0,11)}; pools[1].clear();
    for (unsigned weight : {100u,1000u,10000u}) {
        pools[0][1].spawn_weight=weight;
        PC_CHECK(std::abs(event(pools,0,retains_p)-(.333+.667*100/(100.+weight)))<1e-10); // RB6 declared weights
    }
    pools[0][1].exclusive=false; pools[0][1].spawn_weight=0;
    PC_CHECK(std::abs(event(pools,0,retains_p)-1)<1e-10); // RB3 count before carrier filtering
    pools[0][0].groups.push_back(77); pools[0][1].groups.push_back(77); pools[0][1].spawn_weight=100;
    PC_CHECK(event(pools,0,[](const auto& o){return o.prefixes.occurrences.size()==2;})==0);
}
void run_checked_bridge_witnesses(const poecraft::RecombSolverRequest& original,
        const poecraft::RecombSolverResult& original_result) {
    using namespace poecraft;
    auto economy=std::make_shared<EconomyImpl>();economy->id=original.price_identity;economy->prices={{"annul",2},{"scour",3}};
    const auto child=[&](const pc_item_state& item,const std::string& operation="") {
        const auto base=random_recomb_base_state_json(item,*original.session);
        return "{\"version\":\"v1\",\"name\":\"Checked child\",\"start_node_id\":\"start\",\"base_state\":"+base+
            ",\"output_contracts\":[{\"id\":\"complete\",\"base_key\":"+
            std::string("\"")+original.session->data->string_at(original.session->data->base_metadata_path_sid[original.session->base_index])+"\",\"predicate\":{\"type\":\"always\"}}],"
            "\"nodes\":[{\"id\":\"start\",\"kind\":\"start\"},{\"id\":\"success\",\"kind\":\"terminal\",\"terminal\":\"success\"}"+
            (operation.empty()?"":",{\"id\":\"action\",\"kind\":\"operation\",\"operation\":{\"type\":\""+operation+"\",\"params\":{}}}")+
            "],\"edges\":[{\"id\":\"entry\",\"from\":\"start\",\"to\":\""+(operation.empty()?"success":"action")+"\",\"is_default\":true}"+
            (operation.empty()?"":",{\"id\":\"finish\",\"from\":\"action\",\"to\":\"success\",\"is_default\":true}")+ "]}";
    };
    const auto bdoc=child(original.acquisitions[1].item);
    const auto checked_b=check_recomb_feeder(original.session,"b-child","revision-1",bdoc,"complete",1,economy);
    PC_CHECK(checked_b->outcomes().size()==1 && checked_b->total_cost()==1 && checked_b->child_actions()==0);
    PC_CHECK(random_recomb_item_key(checked_b->outcomes()[0].item)==random_recomb_item_key(original.acquisitions[1].item));
    auto request=original;request.acquisitions[1].source_kind="checked_feeder";request.acquisitions[1].checked_feeder=checked_b;
    const auto solved=solve_random_recomb_inventory(request);
    PC_CHECK(std::abs(solved.expected_cost_chaos-original_result.expected_cost_chaos)<1e-8);
    const auto exported=export_recomb_builder_policy(request,solved);
    PC_CHECK(std::abs(exported.checked_cost-(1+2/.333))<1e-7);
    PC_CHECK(std::abs(exported.checked_recombinations-1/.333)<1e-7 && exported.checked_child_actions==0);
    PC_CHECK(exported.checked_builder_actions>exported.checked_recombinations);
    const auto parsed=json::Parser(exported.strategy_json.data(),exported.strategy_json.size()).parse();
    PC_CHECK(!parsed.at("start_item_present").as_bool());
    for(const auto& n:parsed.at("nodes").array)if(const auto* op=n.find("operation"))if(op->at("type").as_string()=="recombination") {
        PC_CHECK(op->at("params").at("use_declared_inputs").as_bool());
        PC_CHECK(op->at("params").at("input_a").as_string()!=op->at("params").at("input_b").as_string());
    }
    const auto refusal=[&](const std::function<void()>& work,const char* match) {
        bool refused=false;try{work();}catch(const std::exception& e){refused=std::string(e.what()).find(match)!=std::string::npos;}
        PC_CHECK(refused);
    };
    refusal([&]{(void)export_recomb_builder_policy(original,original_result);},"Unchecked feeder");
    auto tampered=exported.strategy_json;
    const auto flag=tampered.find("\"use_declared_inputs\":true");PC_CHECK(flag!=std::string::npos);
    if(flag!=std::string::npos){tampered.replace(flag,std::strlen("\"use_declared_inputs\":true"),"\"use_declared_inputs\":false");refusal([&]{(void)check_recomb_builder_policy(request,solved,tampered);},"provider/attempt-price");}
    auto full=original.acquisitions[2].item;full.prefixes[0].roll_count=1;full.prefixes[0].rolls[0]=123;
    const auto full_doc=child(full);
    const auto checked_full=check_recomb_feeder(original.session,"multimod-child","revision-1",full_doc,"complete",20,economy);
    PC_CHECK(checked_full->outcomes().size()==1 && checked_full->outcomes()[0].item.prefix_count==2 &&
        checked_full->outcomes()[0].item.prefixes[0].rolls[0]==123);
    const auto annul_doc=child(full,"annul");
    const auto checked_annul=check_recomb_feeder(original.session,"annul-child","revision-1",annul_doc,"complete",1,economy);
    PC_CHECK(checked_annul->outcomes().size()==2 && checked_annul->total_cost()==3 && checked_annul->child_actions()==1);
    double total=0;bool roll_retained=false;
    for(const auto& o:checked_annul->outcomes()) {
        total+=o.probability;PC_CHECK(o.item.prefix_count==1 && o.probability==.5 && o.item.rarity==PC_RARITY_RARE);
        roll_retained|=o.item.prefixes[0].roll_count==1 && o.item.prefixes[0].rolls[0]==123;
    }
    PC_CHECK(std::abs(total-1)<1e-10 && roll_retained && checked_annul->materials().at("annul")==1);
    auto free_prices=std::make_shared<EconomyImpl>(*economy);free_prices->prices.clear();
    refusal([&]{(void)check_recomb_feeder(original.session,"annul","r1",annul_doc,"complete",1,free_prices);},"material price");
    auto bad_doc=annul_doc;auto always=bad_doc.find("\"type\":\"always\"");
    bad_doc.replace(always,std::strlen("\"type\":\"always\""),"\"type\":\"rarity_is\",\"rarity\":\"magic\"");
    refusal([&]{(void)check_recomb_feeder(original.session,"annul","r1",bad_doc,"complete",1,economy);},"unaccepted");
    auto cyclic=bdoc;auto destination=cyclic.find("\"to\":\"success\"");cyclic.replace(destination,std::strlen("\"to\":\"success\""),"\"to\":\"start\"");
    refusal([&]{(void)check_recomb_feeder(original.session,"cycle","r1",cyclic,"complete",1,economy);},"terminate");
    refusal([&]{(void)check_recomb_feeder(original.session,"cancel","r1",bdoc,"complete",1,economy,[]{return true;});},"cancelled");
    auto changed=request;changed.acquisitions[2].source_kind="checked_feeder";
    changed.acquisitions[2].checked_feeder=check_recomb_feeder(original.session,"b-child","revision-1",full_doc,"complete",20,economy);
    refusal([&]{(void)solve_random_recomb_inventory(changed);},"conflicting documents");
    auto partial=request;partial.recombination_cost_chaos.reset();partial.recombination_cost_complete=false;partial.allow_incomplete_costs=true;
    const auto unpriced=solve_random_recomb_inventory(partial);
    PC_CHECK(!unpriced.cost_complete && !unpriced.search_converged && unpriced.policy_iterations==0);
    refusal([&]{(void)export_recomb_builder_policy(partial,unpriced);},"economic status");
    auto owned=request;owned.initial_items={original.acquisitions[0].item};owned.initial_item_costs={1};
    const auto owned_solve=solve_random_recomb_inventory(owned);const auto owned_export=export_recomb_builder_policy(owned,owned_solve);
    PC_CHECK(std::abs(owned_export.checked_cost-(1+2/.333))<1e-7);
    SimulatorImpl runner;runner.session=request.session;runner.strategy=compile_strategy_json(request.session,exported.strategy_json.data(),exported.strategy_json.size());
    runner.economy=load_economy_json(exported.economy_json.data(),exported.economy_json.size());prepare_simulator_runtime(runner);
    SimulationOptionsInternal options{};options.target_runs=1000;options.seed=62667494;options.max_actions_per_run=1000;options.max_graph_steps_per_run=4096;
    options.retained_success_count=1;options.retained_failure_count=1;options.retained_trace_count=1;options.max_trace_entries=256;
    run_simulator_chunk(runner,options,1000);
    PC_CHECK(runner.summary.completed_runs==1000 && runner.summary.success_count==1000 && runner.summary.failure_count==0 && runner.summary.stop_count==0);
    PC_CHECK(runner.summary.missing_price_run_count==0 && runner.summary.known_total_cost>0);
    PC_CHECK(!runner.success_examples.empty() && runner.success_examples[0].cost_complete);
    std::printf("checked Ring Builder seed62667494 runs=%llu success=%llu cost=%.17g actions=%llu\n",
        (unsigned long long)runner.summary.completed_runs,(unsigned long long)runner.summary.success_count,runner.summary.known_total_cost,(unsigned long long)runner.summary.total_actions);
    // Cancellation retains the already purchased output before the next acquisition.
    SimulatorImpl stopped;stopped.session=request.session;stopped.strategy=runner.strategy;stopped.economy=runner.economy;unsigned checks=0;
    stopped.cancelled=[&]{return ++checks>=5;};prepare_simulator_runtime(stopped);options.target_runs=1;
    run_simulator_chunk(stopped,options,1);
    PC_CHECK(stopped.summary.stop_count==1 && stopped.summary.success_count==0 && stopped.summary.known_total_cost==1);
    PC_CHECK(!stopped.failure_examples.empty() && stopped.failure_examples[0].failure_reason==PC_SIM_FAILURE_CANCELLED);
    if(!stopped.failure_examples.empty()) {
        const auto resources=json::Parser(stopped.failure_examples[0].resources_json.data(),stopped.failure_examples[0].resources_json.size()).parse();
        unsigned live=0;for(const auto& r:resources.array)live+=r.at("item").at("lifecycle").as_number()==PC_ITEM_LIVE;PC_CHECK(live==1);
    }
}

void run_preparation_witnesses(const poecraft::RecombSolverRequest& original,
        const std::vector<unsigned>& mods) {
    using namespace poecraft;
    const auto& session=*original.session;
    auto economy=std::make_shared<EconomyImpl>();economy->id=original.price_identity;
    economy->prices={{"annul",.1},{"scour",.2}};
    auto acquisition_economy=std::make_shared<EconomyImpl>();acquisition_economy->id=original.price_identity;
    const auto child=[&](const pc_item_state& item) {
        return std::string("{\"version\":\"v1\",\"start_node_id\":\"start\",\"base_state\":")+
            random_recomb_base_state_json(item,session)+
            ",\"output_contracts\":[{\"id\":\"full\",\"base_key\":\""+
            session.data->string_at(session.data->base_metadata_path_sid[session.base_index])+
            "\",\"predicate\":{\"type\":\"always\"}}],\"nodes\":[{\"id\":\"start\",\"kind\":\"start\"},"
            "{\"id\":\"end\",\"kind\":\"terminal\",\"terminal\":\"success\"}],"
            "\"edges\":[{\"id\":\"done\",\"from\":\"start\",\"to\":\"end\",\"is_default\":true}]}";
    };
    const auto offer=[&](const char* id,const pc_item_state& item,double paid_start) {
        const auto checked=check_recomb_feeder(original.session,id,"prepared-fixture-v1",child(item),"full",paid_start,acquisition_economy);
        PC_CHECK(checked->total_cost()==paid_start && checked->outcomes().size()==1);
        RecombAcquisition acquisition{id,"checked_feeder",id,item,paid_start,true};acquisition.checked_feeder=checked;return acquisition;
    };
    auto dirty=original.acquisitions[2].item;
    PC_CHECK(pc_item_add_mod(&dirty,PC_SIDE_PREFIX,mods[2],uint16_t(session.primary_group[mods[2]]),0,nullptr)==PC_RESULT_OK);
    const auto paid_finish=offer("paid-finish",original.acquisitions[2].item,20);
    const auto prepared=[&](RecombSolverRequest request) {
        request.preparation_actions={ActionType::Annul};request.preparation_economy=economy;return request;
    };
    const auto audit=[&](const char* name,const RecombSolverRequest& baseline_request,const RecombSolverRequest& treatment_request) {
        const auto before=solve_random_recomb_inventory(baseline_request),after=solve_random_recomb_inventory(treatment_request);
        const auto before_checked=export_recomb_builder_policy(baseline_request,before);
        const auto after_checked=export_recomb_builder_policy(treatment_request,after);
        PC_CHECK(before.cost_complete && after.cost_complete && before.search_converged && after.search_converged);
        PC_CHECK(!before.global_optimality_claim && !after.global_optimality_claim);
        PC_CHECK(std::abs(before_checked.checked_cost-before.expected_cost_chaos)<1e-8);
        PC_CHECK(std::abs(after_checked.checked_cost-after.expected_cost_chaos)<1e-8);
        auto wrong_output=after_checked.strategy_json;
        const std::string contract_slot="\"resource_id\":\"finished\",\"predicate\"";
        const auto position=wrong_output.find(contract_slot);PC_CHECK(position!=std::string::npos);
        if(position!=std::string::npos) {
            wrong_output.replace(position,contract_slot.size(),"\"resource_id\":\"current\",\"predicate\"");
            bool refused=false;try{(void)check_recomb_builder_policy(treatment_request,after,wrong_output);}catch(const std::exception&){refused=true;}
            PC_CHECK(refused); // A goal in another slot cannot satisfy the declared output.
        }
        for (const auto& decision:after.policy) {
            double total=0;for(const auto& [next,p]:decision.outcomes){PC_CHECK(p>0 && next<after.inventories.size());total+=p;}
            PC_CHECK(std::abs(total-1)<1e-10);
            if(decision.kind==RecombDecisionKind::Prepare) {
                const auto input=after.inventories[decision.state][decision.input_a];
                for(const auto& [next,p]:decision.outcomes) {
                    (void)p;
                    PC_CHECK(after.inventories[next].size()==after.inventories[decision.state].size());
                    if(after.inventories[next].size()==1)
                        PC_CHECK(after.items[after.inventories[next][0]].prefix_count+1==after.items[input].prefix_count);
                    else {
                        const auto other=after.inventories[decision.state][1-decision.input_a];
                        PC_CHECK(std::find(after.inventories[next].begin(),after.inventories[next].end(),other)!=after.inventories[next].end());
                    }
                }
            }
        }
        std::printf("recomb_preparation_case {\"case\":\"%s\",\"before\":%.17g,\"after\":%.17g,\"annuls\":%.17g,\"recombinations\":%.17g,\"discards\":%.17g,\"states_before\":%zu,\"states_after\":%zu,\"work_before\":%llu,\"work_after\":%llu,\"checked\":true}\n",
            name,before.expected_cost_chaos,after.expected_cost_chaos,after.expected_preparations[0],after.expected_recombinations,
            after.expected_discards,before.inventories.size(),after.inventories.size(),
            static_cast<unsigned long long>(before.work_spent),static_cast<unsigned long long>(after.work_spent));
        return std::make_pair(before,after);
    };
    auto incoming=original;incoming.acquisitions={paid_finish};incoming.initial_items={dirty};incoming.initial_item_costs={4};
    const auto incoming_pair=audit("incoming-dirty",incoming,prepared(incoming));
    PC_CHECK(std::abs(incoming_pair.first.expected_cost_chaos-24)<1e-8);
    PC_CHECK(std::abs(incoming_pair.second.expected_cost_chaos-(4+.1+20*2/3.))<1e-8);
    PC_CHECK(std::abs(incoming_pair.second.expected_preparations[0]-1)<1e-8);
    PC_CHECK(std::abs(incoming_pair.second.expected_acquisitions[0]-2/3.)<1e-8);
    auto held=incoming;held.initial_items.push_back(original.acquisitions[0].item);held.initial_item_costs.push_back(1);
    held.recombination_cost_chaos=1000;
    const auto held_pair=audit("two-held-physical-items",held,prepared(held));
    PC_CHECK(std::abs(held_pair.first.expected_cost_chaos-25)<1e-8);
    PC_CHECK(std::abs(held_pair.second.expected_cost_chaos-(5+.1+20*2/3.))<1e-8);
    auto multi=original;multi.acquisitions={offer("dirty-multimod",dirty,1),paid_finish};multi.recombination_cost_chaos=1000;
    const auto multi_pair=audit("paid-multimod",multi,prepared(multi));
    PC_CHECK(std::abs(multi_pair.first.expected_cost_chaos-20)<1e-8);
    PC_CHECK(std::abs(multi_pair.second.expected_cost_chaos-3.3)<1e-8);
    PC_CHECK(std::abs(multi_pair.second.expected_acquisitions[0]-3)<1e-8);
    PC_CHECK(std::abs(multi_pair.second.expected_preparations[0]-3)<1e-8);
    PC_CHECK(std::abs(multi_pair.second.expected_discards-2)<1e-8);
    auto filler=original;auto ax=original.acquisitions[0].item;ax.rarity=PC_RARITY_RARE;
    PC_CHECK(pc_item_add_mod(&ax,PC_SIDE_PREFIX,mods[2],uint16_t(session.primary_group[mods[2]]),0,nullptr)==PC_RESULT_OK);
    filler.acquisitions={offer("ordinary-filler",ax,1),offer("clean-b",original.acquisitions[1].item,1),paid_finish};
    const auto filler_pair=audit("ordinary-filler",filler,prepared(filler));
    PC_CHECK(filler_pair.second.expected_cost_chaos<filler_pair.first.expected_cost_chaos-1e-8);
    PC_CHECK(filler_pair.second.expected_recombinations>0 && filler_pair.second.expected_preparations[0]>0);
    auto costly_economy=std::make_shared<EconomyImpl>(*economy);costly_economy->prices["annul"]=200;
    auto expensive=prepared(incoming);expensive.preparation_economy=costly_economy;
    const auto expensive_pair=audit("expensive-cleanup-control",incoming,expensive);
    PC_CHECK(std::abs(expensive_pair.second.expected_cost_chaos-expensive_pair.first.expected_cost_chaos)<1e-8);
    PC_CHECK(expensive_pair.second.expected_preparations[0]==0);
    auto clean=original;clean.acquisitions={offer("clean-a",original.acquisitions[0].item,1),offer("clean-b",original.acquisitions[1].item,1),paid_finish};
    const auto clean_pair=audit("unchanged-clean-donors",clean,prepared(clean));
    PC_CHECK(std::abs(clean_pair.second.expected_cost_chaos-clean_pair.first.expected_cost_chaos)<1e-8);
    PC_CHECK(clean_pair.second.expected_preparations[0]==0);
    const auto refusal=[&](RecombSolverRequest request) {
        bool refused=false;try{(void)solve_random_recomb_inventory(request);}catch(const std::exception&){refused=true;}PC_CHECK(refused);
    };
    auto invalid=prepared(incoming);invalid.preparation_economy.reset();refusal(invalid);
    invalid=prepared(incoming);invalid.preparation_actions={ActionType::Exalt};refusal(invalid);
    invalid=prepared(incoming);invalid.preparation_actions={ActionType::Scour};refusal(invalid);
    invalid=prepared(incoming);invalid.preparation_actions.push_back(ActionType::Annul);refusal(invalid);
    auto missing=std::make_shared<EconomyImpl>(*economy);missing->prices.erase("annul");
    invalid=prepared(incoming);invalid.preparation_economy=missing;refusal(invalid);
    auto wrong=std::make_shared<EconomyImpl>(*economy);wrong->id="other-prices";
    invalid=prepared(incoming);invalid.preparation_economy=wrong;refusal(invalid);
    invalid=prepared(incoming);invalid.max_items=2;refusal(invalid); // No positive removal output may be pruned.
    // The paid immutable feeder identity stays compatible with the cleanup prices.
    auto changed_price=std::make_shared<EconomyImpl>(*economy);changed_price->prices["annul"]=.3;
    auto changed=prepared(incoming);changed.preparation_economy=changed_price;
    const auto repriced=solve_random_recomb_inventory(changed);
    PC_CHECK(std::abs(repriced.expected_cost_chaos-(4+.3+20*2/3.))<1e-8);
    PC_CHECK(std::abs(repriced.expected_preparations[0]-1)<1e-8);
    std::printf("recomb_preparation_identity {\"base\":\"%s\",\"level\":%u,\"model\":\"%s\",\"data\":\"%s\",\"goal\":%s,\"dirty_input\":%s,\"modifier_keys\":[\"%s\",\"%s\",\"%s\"],\"spawn_proxies\":[%u,%u,%u],\"prices_are_declared_fixtures\":true}\n",
        session.data->string_at(session.data->base_metadata_path_sid[session.base_index]).c_str(),session.item_level,
        original.model_id.c_str(),session.data->artifact_data_hash.c_str(),original.goal_set_json.c_str(),
        random_recomb_base_state_json(dirty,session).c_str(),
        session.data->string_at(session.data->mod_key_sid[session.global_index[mods[0]]]).c_str(),
        session.data->string_at(session.data->mod_key_sid[session.global_index[mods[1]]]).c_str(),
        session.data->string_at(session.data->mod_key_sid[session.global_index[mods[2]]]).c_str(),
        session.base_spawn_weight[mods[0]],session.base_spawn_weight[mods[1]],session.base_spawn_weight[mods[2]]);
}
void run_constraint_witnesses(pc_data_handle data, pc_session_handle ring) {
    using namespace poecraft;
    const std::pair<const char*,RecombOrigin> fixtures[] = {
        {"AddedColdDamageEssence7",RecombOrigin::EssenceOnly},
        {"DexMasterItemGenerationCannotChangeSuffixes",RecombOrigin::Metamod},
        {"DelveAmuletBeltManaRecoveryRate1",RecombOrigin::Delve},
        {"JunMasterVeiledAddedColdAndLightningDamage",RecombOrigin::Unveiled},
        {"AislinVeiledSuffix__",RecombOrigin::VeilTemplate},
        {"GrantsCatAspectCrafted",RecombOrigin::BeastAspect},
        {"AccuracyRatingPerFrenzyChargeUber1",RecombOrigin::InfluencedNatural},
        {"ArmourAndEnergyShieldPercentCrafted_",RecombOrigin::NotExplicit},
        {"BreachBodyAddedColdDamagePerPowerCharge1",RecombOrigin::NonNaturalUnresolved}};
    // Frozen canonical generation type is unique (33), not a prefix/suffix bench recipe.
    // Classification knowledge must not turn this catalogue record into numerical support.
    const auto& d = *ring->impl->data;
    auto fixture = std::make_shared<SessionImpl>(); fixture->data = ring->impl->data;
    fixture->base_index = ring->impl->base_index; fixture->item_level = 80;
    std::vector<uint32_t> retained;
    for (const auto& [key,origin] : fixtures) {
        const auto found = d.mod_pos_by_key.find(key); PC_CHECK(found != d.mod_pos_by_key.end());
        if (found == d.mod_pos_by_key.end()) return;
        retained.push_back(d.mod_global_ids.at(found->second));
    }
    PC_CHECK(!d.influence_elevations.empty());
    if (!d.influence_elevations.empty()) retained.push_back(d.influence_elevations.begin()->second);
    build_session(*fixture,retained);
    for (unsigned i = 0; i < sizeof(fixtures)/sizeof(fixtures[0]); ++i) {
        const auto category = classify_recombination_mod(*fixture,fixture->session_id_by_global_id.at(retained[i]));
        if (category.origin != fixtures[i].second)
            std::fprintf(stderr,"taxonomy fixture %s: expected %s, actual %s\n",fixtures[i].first,
                recombination_origin_name(fixtures[i].second),recombination_origin_name(category.origin));
        PC_CHECK(category.origin == fixtures[i].second);
        if(i==7) PC_CHECK(!category.explicit_modifier);
        const auto expected = i < 6 ? RecombExclusivity::Exclusive :
            i == 6 ? RecombExclusivity::NonExclusive : RecombExclusivity::Unresolved;
        PC_CHECK(category.exclusivity == expected);
    }
    if (!d.influence_elevations.empty()) {
        const auto elevated = classify_recombination_mod(*fixture,fixture->session_id_by_global_id.at(retained.back()));
        PC_CHECK(elevated.origin == RecombOrigin::Elevated && elevated.exclusivity == RecombExclusivity::Exclusive);
    }
    RecombOccurrence first{0,0,100,0,1,{1}}, second{1,0,101,1,1,{2}};
    PC_CHECK(!recombination_mods_can_coexist(first,RecombExclusivity::Exclusive,second,RecombExclusivity::Exclusive));
    PC_CHECK(recombination_mods_can_coexist(first,RecombExclusivity::NonExclusive,second,RecombExclusivity::Exclusive));
    second.groups.push_back(1);
    PC_CHECK(!recombination_mods_can_coexist(first,RecombExclusivity::NonExclusive,second,RecombExclusivity::NonExclusive));

    const auto special = fixture->session_id_by_global_id.at(retained[3]);
    PC_CHECK(fixture->base_spawn_weight.at(special) > 0 && fixture->gen_type.at(special) == 0);
    unsigned ordinary = PC_MOD_NONE;
    for (unsigned m = 0; m < fixture->mod_count; ++m) {
        if (fixture->gen_type[m] != 0 || classify_recombination_mod(*fixture,m).origin != RecombOrigin::Natural ||
            !fixture->base_spawn_weight[m]) continue;
        RecombOccurrence a{0,0,retained[3],special,1,{}}, b{1,0,d.mod_global_ids[fixture->global_index[m]],m,1,{}};
        a.groups.assign(fixture->group_ids.begin()+fixture->group_offsets[special],fixture->group_ids.begin()+fixture->group_offsets[special+1]);
        b.groups.assign(fixture->group_ids.begin()+fixture->group_offsets[m],fixture->group_ids.begin()+fixture->group_offsets[m+1]);
        if (recombination_mods_can_coexist(a,RecombExclusivity::Exclusive,b,RecombExclusivity::NonExclusive)) { ordinary=m; break; }
    }
    PC_CHECK(ordinary != PC_MOD_NONE); if (ordinary == PC_MOD_NONE) return;
    pc_item_state a{}, b{}; pc_item_clear(&a); pc_item_clear(&b); a.rarity=b.rarity=PC_RARITY_MAGIC;
    PC_CHECK(pc_item_add_mod(&a,PC_SIDE_PREFIX,special,uint16_t(fixture->primary_group[special]),0,nullptr)==PC_RESULT_OK);
    PC_CHECK(pc_item_add_mod(&b,PC_SIDE_PREFIX,ordinary,uint16_t(fixture->primary_group[ordinary]),0,nullptr)==PC_RESULT_OK);
    a.prefixes[0].roll_count=1; a.prefixes[0].rolls[0]=73;
    const auto pair = prepare_random_recomb_pair({"a","a",fixture,a},{"b","b",fixture,b});
    PC_CHECK(pair.model_id == kRandomRecombExtendedModel);
    double mass=0, special_mass=0;
    for (const auto& outcome : enumerate_random_recomb_pair(pair)) {
        mass += outcome.probability;
        const auto item=materialize_random_recomb_outcome(pair,outcome);
        const auto& mapping=*pair.carriers[outcome.carrier].output_session;
        for (unsigned i=0;i<item.prefix_count;++i)
            if (mapping.data->mod_global_ids[mapping.global_index[item.prefixes[i].mod_id]]==retained[3]) {
                special_mass+=outcome.probability; PC_CHECK(item.prefixes[i].rolls[0]==73);
            }
    }
    const double expected=.333+.667*fixture->base_spawn_weight[special]/
        double(uint64_t(fixture->base_spawn_weight[special])+fixture->base_spawn_weight[ordinary]);
    PC_CHECK(std::abs(mass-1)<1e-10 && std::abs(special_mass-expected)<1e-10);
    auto finished_spec=a; finished_spec.rarity=PC_RARITY_RARE;
    PC_CHECK(pc_item_add_mod(&finished_spec,PC_SIDE_PREFIX,ordinary,uint16_t(fixture->primary_group[ordinary]),0,nullptr)==PC_RESULT_OK);
    const auto key = [&](unsigned m) { return d.string_at(d.mod_key_sid.at(fixture->global_index.at(m))); };
    const auto goals = std::string(R"({"version":"calculator_goal_set_v1","actions":[],"goals":[{"id":"special-pair","goal":{"version":"v1","rarity":"rare","slots":[{"family_mod_key":")")+
        key(special)+R"(","min_tier":)"+std::to_string(fixture->family_tier_index[special])+R"(},{"family_mod_key":")"+
        key(ordinary)+R"(","min_tier":)"+std::to_string(fixture->family_tier_index[ordinary])+R"(}]}}]})";
    RecombSolverRequest request; request.session=fixture; request.goal_set_json=goals;
    request.price_identity="special-fixture"; request.recombination_cost_chaos=1; request.recombination_cost_complete=true;
    request.acquisitions={{"special","purchase","quote-special",a,1,true},
        {"normal","completed_feeder","quote-normal",b,1,true},
        {"finished","completed_feeder","quote-finished",finished_spec,20,true}};
    const auto held=solve_random_recomb_inventory(request);
    PC_CHECK(held.model_id==kRandomRecombModel && std::abs(held.expected_cost_chaos-20)<1e-8);
    bool explicit_exclusion=false;
    for (const auto& exclusion : held.exclusions) explicit_exclusion |= exclusion.find("explicitly selected")!=std::string::npos;
    PC_CHECK(explicit_exclusion);
    request.model_id=kRandomRecombExtendedModel;
    const auto extended=solve_random_recomb_inventory(request);
    PC_CHECK(extended.model_id==kRandomRecombExtendedModel && extended.search_converged && !extended.global_optimality_claim);
    PC_CHECK(std::abs(extended.expected_cost_chaos-(1+2/.333))<1e-7 && extended.expected_acquisitions[2]==0);
    pc_item_state zero_special{}; pc_item_clear(&zero_special); zero_special.rarity=PC_RARITY_MAGIC;
    const auto zero_id=fixture->session_id_by_global_id.at(retained[0]);
    PC_CHECK(fixture->base_spawn_weight[zero_id]==0);
    PC_CHECK(pc_item_add_mod(&zero_special,PC_SIDE_PREFIX,zero_id,uint16_t(fixture->primary_group[zero_id]),0,nullptr)==PC_RESULT_OK);
    bool weight_refused=false;
    try { (void)prepare_random_recomb_pair({"zero","a",fixture,zero_special},{"normal","b",fixture,b}); }
    catch (const std::invalid_argument&) { weight_refused=true; }
    PC_CHECK(weight_refused && zero_special.lifecycle==PC_ITEM_LIVE && b.lifecycle==PC_ITEM_LIVE);
    auto duplicate=a;
    bool refused=false;
    try { (void)prepare_random_recomb_pair({"a","a",fixture,a},{"b","b",fixture,duplicate}); }
    catch (const std::invalid_argument&) { refused=true; }
    PC_CHECK(refused); // two physical exclusives do not borrow an ordinary count row
    pc_session mapped; mapped.impl=fixture;
    pc_craft_resource ra{"a","a",&mapped,&a}, rb{"b","b",&mapped,&duplicate};
    const auto before_a=a, before_b=duplicate;
    pc_error_info error{}; size_t length=0;
    PC_CHECK(pc_recombination_constraints_json(&ra,&rb,1,nullptr,0,&length,&error)==PC_RESULT_BUFFER_TOO_SMALL);
    std::vector<char> buffer(length+1);
    PC_CHECK(pc_recombination_constraints_json(&ra,&rb,1,buffer.data(),buffer.size(),&length,&error)==PC_RESULT_OK);
    const auto inspected=json::Parser(buffer.data(),length).parse();
    PC_CHECK(!inspected.at("probability_law_complete").as_bool() && inspected.at("side_order_model").is_null());
    PC_CHECK(inspected.at("sides").array[0].at("physical_count").as_number()==2 &&
        inspected.at("sides").array[0].at("known_exclusive_occurrences").as_number()==2);
    PC_CHECK(inspected.at("sides").array[0].at("effective_count").is_null() && inspected.at("known_conflicts").array.size()==1);
    PC_CHECK(inspected.at("occurrences").array[0].at("carrier_spawn_proxies").array[0].as_number()>0);
    PC_CHECK(std::memcmp(&a,&before_a,sizeof(a))==0 && std::memcmp(&duplicate,&before_b,sizeof(duplicate))==0);
    PC_CHECK(pc_recombination_constraints_json(&ra,&rb,2,nullptr,0,&length,&error)==PC_RESULT_INVALID_ARGUMENT);

    pc_session_options options{}; options.struct_size=sizeof(options); options.abi_version=PC_ABI_VERSION;
    options.base_metadata_path="Metadata/Items/Armours/BodyArmours/BodyInt1"; options.item_level=80;
    pc_session_handle es=nullptr;
    PC_CHECK(pc_session_create(data,&options,&es,&error)==PC_RESULT_OK); if (!es) return;
    unsigned nnn=PC_MOD_NONE, normal=PC_MOD_NONE;
    for (unsigned m=0;m<es->impl->mod_count;++m) {
        const auto category=classify_recombination_mod(*es->impl,m);
        if (es->impl->gen_type[m]==0 && category.origin==RecombOrigin::Natural) {
            if (!category.natural_on_source && category.guaranteed_natural_essence_source) nnn=m;
            if (category.natural_on_source) normal=m;
        }
    }
    PC_CHECK(nnn!=PC_MOD_NONE && normal!=PC_MOD_NONE);
    if (nnn!=PC_MOD_NONE && normal!=PC_MOD_NONE) {
        pc_item_state donor{}, receiver{}; pc_item_clear(&donor); pc_item_clear(&receiver);
        donor.rarity=receiver.rarity=PC_RARITY_MAGIC;
        PC_CHECK(pc_item_add_mod(&donor,PC_SIDE_PREFIX,nnn,uint16_t(es->impl->primary_group[nnn]),0,nullptr)==PC_RESULT_OK);
        PC_CHECK(pc_item_add_mod(&receiver,PC_SIDE_PREFIX,normal,uint16_t(es->impl->primary_group[normal]),0,nullptr)==PC_RESULT_OK);
        const auto nnn_pair=prepare_random_recomb_pair({"n","a",es->impl,donor},{"r","b",es->impl,receiver});
        PC_CHECK(nnn_pair.model_id==kRandomRecombExtendedModel && nnn_pair.carriers[0].sides[0].size()==2);
        double nnn_mass=0;
        for (const auto& output : enumerate_random_recomb_pair(nnn_pair)) {
            nnn_mass+=output.probability;
            const auto item=materialize_random_recomb_outcome(nnn_pair,output);
            PC_CHECK(item.prefix_count==1); // count first, ineligible natural occurrence exhausted on both carriers
        }
        PC_CHECK(std::abs(nnn_mass-1)<1e-10);
    }
    pc_session_destroy(es);
    // The public pair keeps its model and atomic receipt on the bounded special path.
    pc_craft_resource special_inputs[2]{{"special-a","a",&mapped,&a},{"normal-b","b",&mapped,&b}};
    pc_recombination_pair_handle handle=nullptr;
    PC_CHECK(pc_recombination_pair_create(&special_inputs[0],&special_inputs[1],1,&handle,&error)==PC_RESULT_OK);
    if (handle) {
        size_t required=0;
        PC_CHECK(pc_recombination_pair_calculate_json(handle,nullptr,0,&required,&error)==PC_RESULT_BUFFER_TOO_SMALL);
        std::vector<char> text(required+1);
        PC_CHECK(pc_recombination_pair_calculate_json(handle,text.data(),text.size(),&required,&error)==PC_RESULT_OK);
        PC_CHECK(json::Parser(text.data(),required).parse().at("model_id").as_string()==kRandomRecombExtendedModel);
        pc_action_context_options context_options{}; context_options.struct_size=sizeof(context_options);
        context_options.abi_version=PC_ABI_VERSION; context_options.seed=87;
        pc_action_context_handle context=nullptr;
        PC_CHECK(pc_action_context_create(&mapped,&context_options,&context,&error)==PC_RESULT_OK);
        if (context) {
            pc_recombination_result applied{};
            PC_CHECK(pc_recombination_pair_apply(handle,context,special_inputs,2,"special-output",&applied,&error)==PC_RESULT_OK);
            PC_CHECK(applied.model_id && std::strcmp(applied.model_id,kRandomRecombExtendedModel)==0 &&
                a.lifecycle==PC_ITEM_CONSUMED && b.lifecycle==PC_ITEM_CONSUMED);
            PC_CHECK(!applied.gold_cost_complete && !applied.dust_cost_complete && applied.transaction.resource_count==3);
            pc_recombination_pair_destroy(handle); handle=nullptr;
            PC_CHECK(applied.model_id && std::strcmp(applied.model_id,kRandomRecombExtendedModel)==0);
            pc_session_destroy(applied.output_session); pc_action_context_destroy(context);
        }
    }
    pc_recombination_pair_destroy(handle);
}
}
void run_recombination_solver_tests(const char* artifact_dir) {
    using namespace poecraft;
    // Physical duplicates affect the count row; selecting removes full groups.
    std::vector<RecombOccurrence> pool{
        {0, 0, 10, 0, 100, {1}}, {1, 0, 10, 0, 100, {1}}, {1, 1, 11, 1, 100, {2}}};
    double duplicate_success = 0;
    for (const auto& outcome : enumerate_random_recomb_side(pool))
        if (outcome.occurrences.size() == 2) duplicate_success += outcome.probability;
    PC_CHECK(std::abs(duplicate_success - .6) < 1e-10);
    pool[1] = {0, 1, 12, 2, 100, {3}};
    double filler_success = 0;
    for (const auto& outcome : enumerate_random_recomb_side(pool)) {
        bool a = false, b = false;
        for (auto index : outcome.occurrences) { a |= pool[index].global_mod_id == 10; b |= pool[index].global_mod_id == 11; }
        if (a && b) filler_success += outcome.probability;
    }
    PC_CHECK(std::abs(filler_success - (.1 + .5/3)) < 1e-10 && filler_success < .333);
    pool[0].groups.push_back(77); pool[2].groups.push_back(77);
    for (const auto& outcome : enumerate_random_recomb_side(pool)) {
        bool a = false, b = false;
        for (auto index : outcome.occurrences) { a |= pool[index].global_mod_id == 10; b |= pool[index].global_mod_id == 11; }
        PC_CHECK(!(a && b)); // secondary group membership blocks, not only primary
    }
    if (!artifact_dir) { PC_CHECK(false); return; }
    pc_error_info error{}; pc_data_handle data = nullptr;
    const auto manifest = std::string(artifact_dir) + "/manifest.json";
    PC_CHECK(pc_data_load_file(manifest.c_str(), &data, &error) == PC_RESULT_OK);
    if (!data) return;
    pc_session_options options{}; options.struct_size = sizeof(options); options.abi_version = PC_ABI_VERSION;
    options.base_metadata_path = "Metadata/Items/Rings/Ring1"; options.item_level = 80;
    pc_session_handle session = nullptr;
    PC_CHECK(pc_session_create(data, &options, &session, &error) == PC_RESULT_OK);
    if (!session) { pc_data_destroy(data); return; }
    run_blocking_witnesses();
    run_constraint_witnesses(data,session);
    std::vector<unsigned> mods;
    const auto& s = *session->impl;
    for (unsigned m = 0; m < s.mod_count && mods.size() < 3; ++m)
        if (s.gen_type[m] == 0 && !s.flags[m] && s.special_kind[m] < 0 &&
            s.metamod_type[m] < 0 && s.influence_code[m] <= 0 && s.base_spawn_weight[m] > 0) {
            bool overlap = false;
            for (auto old : mods) for (auto i = s.group_offsets[m]; i < s.group_offsets[m+1]; ++i)
                for (auto j = s.group_offsets[old]; j < s.group_offsets[old+1]; ++j)
                    if (s.group_ids[i] == s.group_ids[j]) overlap = true;
            if (!overlap) mods.push_back(m);
        }
    PC_CHECK(mods.size() == 3);
    if (mods.size() != 3) { pc_session_destroy(session); pc_data_destroy(data); return; }
    pc_item_state a{}, b{}, ab{};
    pc_item_clear(&a); pc_item_clear(&b); pc_item_clear(&ab);
    a.rarity = b.rarity = PC_RARITY_MAGIC; ab.rarity = PC_RARITY_RARE;
    for (unsigned i = 0; i < 2; ++i) {
        auto& item = i ? b : a;
        PC_CHECK(pc_item_add_mod(&item, PC_SIDE_PREFIX, mods[i], static_cast<uint16_t>(s.primary_group[mods[i]]), 0, nullptr) == PC_RESULT_OK);
        PC_CHECK(pc_item_add_mod(&ab, PC_SIDE_PREFIX, mods[i], static_cast<uint16_t>(s.primary_group[mods[i]]), 0, nullptr) == PC_RESULT_OK);
    }
    // Native pair preparation must accept the magic alt-built feeder pathway.
    const auto pair = prepare_random_recomb_pair({"a", "a", session->impl, a}, {"b", "b", session->impl, b});
    double mass = 0, success = 0;
    for (const auto& outcome : enumerate_random_recomb_pair(pair)) {
        mass += outcome.probability;
        const auto item = materialize_random_recomb_outcome(pair, outcome);
        PC_CHECK(item.rarity == PC_RARITY_RARE);
        if (item.prefix_count == 2) success += outcome.probability;
    }
    PC_CHECK(std::abs(mass - 1) < 1e-10 && std::abs(success - .333) < 1e-10);
    const auto key = [&](unsigned id) { return s.data->string_at(s.data->mod_key_sid.at(s.global_index.at(id))); };
    const auto goal = std::string(R"({"version":"calculator_goal_set_v1","actions":[],"goals":[{"id":"target","goal":{"version":"v1","rarity":"rare","slots":[{"family_mod_key":")") +
        key(mods[0]) + R"(","min_tier":)" + std::to_string(s.family_tier_index[mods[0]]) +
        R"(},{"family_mod_key":")" + key(mods[1]) + R"(","min_tier":)" +
        std::to_string(s.family_tier_index[mods[1]]) + R"(}]}}]})";
    RecombSolverRequest request; request.session = session->impl; request.price_identity = "fixture-prices";
    request.goal_set_json = goal; request.recombination_cost_chaos = 1; request.recombination_cost_complete = true;
    request.acquisitions = {{"a", "purchase", "quote-a", a, 1, true},
        {"b", "completed_feeder", "quote-b-full-retry-cost", b, 1, true},
        {"finished", "completed_feeder", "quote-finished-multimod-comparison", ab, 20, true}};
    const auto solved = solve_random_recomb_inventory(request);
    PC_CHECK(solved.search_converged && solved.cost_complete && !solved.global_optimality_claim);
    PC_CHECK(std::abs(solved.expected_recombinations - 1/.333) < 1e-7);
    // One first acquisition plus one complementary fresh item per attempt.
    PC_CHECK(std::abs(solved.expected_acquisitions[0] + solved.expected_acquisitions[1] - (1 + 1/.333)) < 1e-7);
    PC_CHECK(solved.expected_acquisitions[2] == 0);
    PC_CHECK(std::abs(solved.expected_cost_chaos - (1 + 2/.333)) < 1e-7);
    PC_CHECK(solved.expected_cost_chaos < 3/.333); // beats discarding both failure inputs
    for (const auto& decision : solved.policy) {
        double row_mass = 0; for (auto [next, p] : decision.outcomes) { PC_CHECK(next < solved.inventories.size() && p > 0); row_mass += p; }
        PC_CHECK(std::abs(row_mass - 1) < 1e-10);
    }
    run_checked_bridge_witnesses(request,solved);
    run_preparation_witnesses(request,mods);
    auto cheaper_finish = request; cheaper_finish.acquisitions[2].total_cost_chaos = 2;
    const auto finished = solve_random_recomb_inventory(cheaper_finish);
    PC_CHECK(std::abs(finished.expected_cost_chaos - 2) < 1e-9 && finished.expected_recombinations == 0 &&
        finished.expected_acquisitions[2] == 1);
    const auto refusal = [&](RecombSolverRequest invalid) {
        bool refused = false; try { (void)solve_random_recomb_inventory(invalid); } catch (const std::exception&) { refused = true; }
        PC_CHECK(refused);
    };
    auto invalid = request; invalid.recombination_cost_complete = false; refusal(invalid);
    invalid = request; invalid.acquisitions[0].cost_complete = false; refusal(invalid);
    invalid = request; invalid.acquisitions[0].total_cost_chaos = std::numeric_limits<double>::quiet_NaN(); refusal(invalid);
    invalid = request; invalid.acquisitions[2].item.rarity = PC_RARITY_MAGIC; refusal(invalid);
    invalid = request; invalid.acquisitions[2].item.prefixes[1] = invalid.acquisitions[2].item.prefixes[0]; refusal(invalid);
    invalid = request; invalid.model_id = "unknown-model"; refusal(invalid);
    invalid = request; invalid.max_items = 1; refusal(invalid);
    invalid = request; invalid.max_states = 1; refusal(invalid);
    invalid = request; invalid.max_work = 1; refusal(invalid);
    invalid = request; invalid.cancelled = [] { return true; }; refusal(invalid);
    invalid = request; invalid.initial_items = {a}; invalid.initial_item_costs = {}; refusal(invalid);
    invalid = request; invalid.goal_set_json = R"({"version":"calculator_goal_set_v1","goals":[{"id":"numeric","goal":{"version":"v1","slots":[],"rolled_stat_total":123}}]})"; refusal(invalid);
    invalid = request; invalid.acquisitions[0].total_cost_chaos = 0; invalid.acquisitions[1].total_cost_chaos = 0;
    invalid.acquisitions[2].total_cost_chaos = 1; invalid.recombination_cost_chaos = 0;
    const auto zero = solve_random_recomb_inventory(invalid);
    PC_CHECK(zero.expected_cost_chaos == 0 && std::isfinite(zero.expected_recombinations) && zero.expected_recombinations > 0);
    pc_recombination_acquisition offers[] = {
        {"a", "purchase", "quote-a", &a, 1, 1}, {"b", "completed_feeder", "quote-b", &b, 1, 1},
        {"finished", "completed_feeder", "quote-finished", &ab, 20, 1}};
    pc_recombination_solver_options input{};
    input.struct_size = sizeof(input); input.abi_version = PC_ABI_VERSION; input.solver_version = PC_RECOMBINATION_SOLVER_VERSION;
    input.model_id = kRandomRecombModel; input.price_identity = "fixture-prices";
    input.goal_set_json = goal.data(); input.goal_set_json_size = goal.size();
    input.acquisitions = offers; input.acquisition_count = 3; input.recombination_cost_chaos = 1; input.recombination_cost_complete = 1;
    pc_recombination_solver_handle handle = nullptr;
    const auto before_a = a, before_b = b;
    PC_CHECK(pc_recombination_solver_create(session, &input, &handle, &error) == PC_RESULT_OK);
    PC_CHECK(std::memcmp(&a, &before_a, sizeof(a)) == 0 && std::memcmp(&b, &before_b, sizeof(b)) == 0);
    if (handle) {
        size_t length = 0;
        PC_CHECK(pc_recombination_solver_result_json(handle, nullptr, 0, &length, &error) == PC_RESULT_BUFFER_TOO_SMALL);
        std::vector<char> text(length + 1);
        PC_CHECK(pc_recombination_solver_result_json(handle, text.data(), text.size(), &length, &error) == PC_RESULT_OK);
        const auto parsed = json::Parser(text.data(), length).parse();
        PC_CHECK(parsed.at("economic_inputs_declared").as_bool() && !parsed.at("native_feeder_cost_certified").as_bool());
        PC_CHECK(std::abs(parsed.at("expected_cost_chaos").as_number() - solved.expected_cost_chaos) < 1e-8);
        pc_session_handle mapping = nullptr;
        PC_CHECK(pc_recombination_solver_session(handle, &mapping, &error) == PC_RESULT_OK);
        pc_item_state exact{};
        PC_CHECK(pc_recombination_solver_item(handle, 0, &exact, &error) == PC_RESULT_OK);
        pc_recombination_solver_destroy(handle); handle = nullptr;
        pc_base_info info{};
        PC_CHECK(pc_session_get_base_info(mapping, &info, &error) == PC_RESULT_OK && info.item_level == 80);
        pc_session_destroy(mapping);
    }
    input.model_id = "wrong-model";
    PC_CHECK(pc_recombination_solver_create(session, &input, &handle, &error) == PC_RESULT_INVALID_ARGUMENT && !handle);
    pc_recombination_solver_destroy(handle); pc_session_destroy(session); pc_data_destroy(data);
}
