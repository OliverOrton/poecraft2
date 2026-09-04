#include "poecraft/api.h"
#include "poecraft/session.h"
#include "poecraft/solver.h"
#include "poecraft/simulator.h"
#include "handles_internal.hpp"
#include "solver_solve_types.hpp"
#include "solver_diagnostic_options.hpp"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>
#include "probe_solver_handle.inc"
namespace poecraft::solver { struct SolveWorkTestAccess { using Impl = SolveWork::Impl; }; }
std::string read(const char* p) { std::ifstream f(p); return {std::istreambuf_iterator<char>(f), {}}; }
void check(pc_result result, const pc_error_info& e) { if(result != PC_RESULT_OK) throw std::runtime_error(e.message); }
int main(int argc,char**argv) { try {
  using namespace poecraft; using namespace poecraft::solver;
  const auto began=std::chrono::steady_clock::now();
  if(argc!=4)throw std::runtime_error("manifest goal economy required");
  pc_data_handle data{};pc_session_handle session{};pc_solver_handle handle{};pc_economy_handle economy{};pc_error_info error{};pc_error_info_init(&error);
  check(pc_data_load_file(argv[1],&data,&error),error);
  pc_session_options so{};so.struct_size=sizeof(so);so.abi_version=PC_ABI_VERSION;so.base_metadata_path="Metadata/Items/Armours/BodyArmours/BodyInt17";so.item_level=86;
  check(pc_session_create(data,&so,&session,&error),error);
  const auto gj=read(argv[2]);const auto ej=read(argv[3]);
  check(pc_solver_create(session,gj.data(),gj.size(),&handle,&error),error);check(pc_economy_load_json(ej.data(),ej.size(),&economy,&error),error);
  pc_item_state start{};pc_item_init_options io{};io.struct_size=sizeof(io);io.abi_version=PC_ABI_VERSION;io.rarity=PC_RARITY_NORMAL;io.with_implicits=0;check(pc_item_init(session,&io,&start,&error),error);
  auto& calc=*handle->calc; SolveOptions options;options.high_impact_executable_uppers=true;options.allow_economic_restart=true;options.consider_imprint_programs=false;options.goal_progress_gated_reforges=false;
  options.max_states=8;options.max_discovered_states=8;options.max_expanded_states=8;options.max_state_action_rows=24;options.max_transitions=56;options.max_reforge_work=20000;options.max_solver_owned_bytes=268435456;
  SolveWorkTestAccess::Impl impl(calc,start,economy->impl->prices,options);impl.prepare_goal_cover_cost();
  std::ostringstream rows;rows<<std::setprecision(17); bool first=true;size_t queried=0,applicable=0,outcomes=0;
  const auto before_bytes=impl.audited_estimated_owned_bytes();const auto row_begin=std::chrono::steady_clock::now();
  for(uint32_t s=0;s<calc.state_count();++s){
    if(s>=8)throw std::runtime_error("8 state limit exceeded");
    if(calc.is_goal_state(calc.state(s)))continue;
    for(const auto& p:impl.operators){
      if(++queried>24)throw std::runtime_error("24 query limit exceeded");const auto& op=calc.operators()[p.index];if(op.kind!=PlannerOperatorKind::Primitive)throw std::runtime_error("unexpected program");
      const auto& action=calc.registry().actions[op.primitive_action];
      if(!action_legal(*session->impl,action,calc.state(s))){if(!first)rows<<",";first=false;rows<<"{\"source\":"<<s<<",\"action\":\""<<action.id<<"\",\"cost\":"<<p.cost<<",\"supported\":true,\"applicable\":false,\"inapplicability_owner\":\"native_action_legal\",\"entries\":[]}";continue;}
      const auto& row=calc.outcomes(s,op.primitive_action,false);
      if(!first)rows<<",";first=false;rows<<"{\"source\":"<<s<<",\"action\":\""<<action.id<<"\",\"cost\":"<<p.cost<<",\"supported\":"<<(row.supported?"true":"false")<<",\"applicable\":"<<(row.applicable?"true":"false")<<",\"entries\":[";
      if(!row.choice_groups.empty()||!row.choice_options.empty())throw std::runtime_error("unexpected choices");
      if(row.applicable)++applicable;
      for(size_t i=0;row.supported&&row.applicable&&i<row.entries.size();++i){if(++outcomes>56)throw std::runtime_error("56 outcome limit exceeded at source="+std::to_string(s)+" action="+action.id+" states="+std::to_string(calc.state_count())+" queried="+std::to_string(queried)+" row_entries="+std::to_string(row.entries.size())+" row_applicable="+std::to_string(row.applicable));if(i)rows<<",";rows<<"{\"target\":"<<row.entries[i].state<<",\"p\":"<<row.entries[i].probability<<"}";}
      rows<<"]}";
    }
  }
  const auto row_ns=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-row_begin).count();
  std::cout<<std::setprecision(17)<<"{\"research_only\":true,\"solver_steps\":0,\"caller_actions\":[\"transmute\",\"alteration\",\"restart\"],\"states\":[";
  for(uint32_t s=0;s<calc.state_count();++s){if(s)std::cout<<",";pc_item_state item{};if(!calc.materialize(s,item))throw std::runtime_error("materialize failed");const auto key=exact_item_state_key(item);uint64_t digest=1469598103934665603ULL;impl.identity_mix(digest,key.size());for(auto v:key)impl.identity_mix(digest,v);
    std::cout<<"{\"id\":"<<s<<",\"goal\":"<<(calc.is_goal_state(calc.state(s))?"true":"false")<<",\"lower\":"<<impl.completion_proof_lower_value(s)<<",\"rarity\":"<<(unsigned)calc.state(s).rarity<<",\"canonical_item_digest\":\""<<digest<<"\",\"canonical_item_key\":[";for(size_t i=0;i<key.size();++i){if(i)std::cout<<",";std::cout<<"\""<<key[i]<<"\"";}std::cout<<"]}";
  }
  std::cout<<"],\"rows\":["<<rows.str()<<"],\"metrics\":{\"states\":"<<calc.state_count()<<",\"queries\":"<<queried<<",\"applicable_rows\":"<<applicable<<",\"outcomes\":"<<outcomes<<",\"query_ns\":"<<row_ns<<",\"owned_before\":"<<before_bytes<<",\"owned_after\":"<<impl.audited_estimated_owned_bytes()<<",\"elapsed_ns\":"<<std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-began).count()<<"}}\n";
  return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 2;}}
