#include "poecraft/api.h"
#include "poecraft/session.h"
#include "poecraft/solver.h"
#include "poecraft/simulator.h"
#include "handles_internal.hpp"
#include "solver_solve_types.hpp"
#include "solver_diagnostic_options.hpp"
#include "json.hpp"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>
#include "probe_solver_handle.inc"

namespace poecraft::solver { struct SolveWorkTestAccess { using Impl = SolveWork::Impl; }; }
std::string read(const char* p) { std::ifstream f(p); return {std::istreambuf_iterator<char>(f), {}}; }
void check(pc_result result, const pc_error_info& e) { if(result != PC_RESULT_OK) throw std::runtime_error(e.message); }
void number(double v) { if(std::isfinite(v)) std::cout<<v; else std::cout<<"null"; }
int main(int argc,char**argv) { try {
  using namespace poecraft; using namespace poecraft::solver;
  const auto start_time = std::chrono::steady_clock::now();
  if(argc!=4) throw std::runtime_error("manifest goal economy required");
  pc_data_handle data{}; pc_session_handle session{}; pc_solver_handle handle{}; pc_economy_handle economy{};
  pc_error_info error{}; pc_error_info_init(&error);
  check(pc_data_load_file(argv[1],&data,&error),error);
  pc_session_options so{}; so.struct_size=sizeof(so);so.abi_version=PC_ABI_VERSION;so.base_metadata_path="Metadata/Items/Armours/BodyArmours/BodyStrDex20";so.item_level=86;
  check(pc_session_create(data,&so,&session,&error),error);
  const auto gj=read(argv[2]); const auto ej=read(argv[3]);
  check(pc_solver_create(session,gj.data(),gj.size(),&handle,&error),error);
  check(pc_economy_load_json(ej.data(),ej.size(),&economy,&error),error);
  pc_item_state rare{}; pc_item_init_options io{};io.struct_size=sizeof(io);io.abi_version=PC_ABI_VERSION;io.rarity=PC_RARITY_RARE;io.with_implicits=0;check(pc_item_init(session,&io,&rare,&error),error);
  auto& calc=*handle->calc;
  SolveOptions options; apply_solve_profile_defaults(options,SolveProfile::CalculatorProductV1);
  options.max_solver_owned_bytes=268435456; options.max_discovered_states=1000; options.max_expanded_states=1000;options.max_states=1000;
  options.max_state_action_rows=1000; options.max_transitions=1000; options.max_reforge_work=1000;
  SolveWorkTestAccess::Impl impl(calc,rare,economy->impl->prices,options);
  impl.prepare_goal_cover_cost();
  std::cout<<std::setprecision(17)<<"{\"research_only\":true,\"solver_steps\":0,\"states\":[";
  for(int shape=0;shape<2;++shape) {
    auto item=rare; if(shape==1)item.rarity=PC_RARITY_NORMAL;
    const auto s=calc.intern_item(item); const auto& state=calc.state(s); const auto mask=impl.satisfied_goal_mask_for_state(s);
    const double base=impl.completion_proof_lower_value(s);
    if(shape)std::cout<<",";
    std::cout<<"{\"shape\":\""<<(shape?"empty_normal":"empty_rare")<<"\",\"state\":"<<s<<",\"base_lower\":";number(base);
    std::cout<<",\"carrier_lower\":";number(impl.carrier_goal_progress_lower_value(s));
    std::cout<<",\"placeholder_cover\":\"unmaterialized automatic families conservatively bounded by base_lower; no family enumeration claimed\",\"actions\":[";
    bool first=true;
    for(const auto& p:impl.operators) {
      const auto& op=calc.operators()[p.index];
      if(op.kind!=PlannerOperatorKind::Primitive)continue;
      const auto ai=op.primitive_action; const auto& a=calc.registry().actions[ai];
      if(!first)std::cout<<",";first=false;
      std::cout<<"{\"id\":\""<<a.id<<"\",\"index\":"<<ai<<",\"legal\":"<<(action_legal(*session->impl,a,state)?"true":"false")<<",\"price\":"<<p.cost<<",\"operator_lower\":";number(impl.operator_proof_lower_value(s,p.index,false));
      std::cout<<",\"clean_start_floor\":";number(shape==0&&ai<impl.clean_goal_start_action_floor.size()?impl.clean_goal_start_action_floor[ai]:INFINITY);
      const size_t ci=(static_cast<size_t>(state.rarity)*impl.goal_cover_cost.size()+mask)*calc.registry().actions.size()+ai;
      std::cout<<",\"carrier_action_floor\":";number(ci<impl.carrier_goal_action_floor.size()?impl.carrier_goal_action_floor[ci]:INFINITY);
      std::cout<<"}";
    }
    std::cout<<"],\"registered_operators\":"<<calc.operators().size()<<",\"priced_operators\":"<<impl.operators.size()<<"}";
  }
  const auto before_states=calc.state_count(); const auto before_bytes=calc.audited_estimated_owned_bytes();
  const auto ai=calc.registry().index_by_id.at("scour"); const auto root=calc.intern_item(rare);
  const auto query_start=std::chrono::steady_clock::now(); const auto& row=calc.outcomes(root,ai,true);
  std::cout<<"],\"scour\":{\"supported\":"<<(row.supported?"true":"false")<<",\"applicable\":"<<(row.applicable?"true":"false")<<",\"successors\":[";
  bool first=true; for(const auto& e:row.entries) {if(!first)std::cout<<",";first=false;std::cout<<"{\"state\":"<<e.state<<",\"p\":"<<e.probability<<",\"rarity\":"<<(unsigned)calc.state(e.state).rarity<<",\"lower\":";number(impl.completion_proof_lower_value(e.state));std::cout<<"}";}
  std::cout<<"],\"choice_groups\":"<<row.choice_groups.size()<<",\"query_ns\":"<<std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-query_start).count()<<",\"states_before\":"<<before_states<<",\"states_after\":"<<calc.state_count()<<",\"calc_owned_before\":"<<before_bytes<<",\"calc_owned_after\":"<<calc.audited_estimated_owned_bytes()<<"},\"proof_live_bytes\":"<<impl.audited_estimated_owned_bytes()<<",\"elapsed_ns\":"<<std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start_time).count()<<"}\n";
  return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 2;}}
