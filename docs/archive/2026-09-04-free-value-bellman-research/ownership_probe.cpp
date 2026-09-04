#include "solver_sparse_policy.hpp"
#include "solver_policy_refinement.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>
using namespace poecraft::solver;
int main() {
  std::cout << std::setprecision(17);
  SolveTransitionCache graph;
  std::vector<solve_detail::PricedSparseRow> prices;
  solve_detail::SparsePolicyRowInput input;
  input.owner_state=0; input.operator_index=0; input.cost=1;
  input.transitions={{1,0.5}};
  input.choices={{0.5,true,{0,2}}};
  const auto row=solve_detail::append_sparse_policy_row(graph,prices,input);
  std::uint32_t work=0;
  const double choice=solve_detail::evaluate_sparse_policy_row(graph,prices,{0,0,100},row,work);
  std::cout << "choice_result=" << choice << ";expected=2;nonself_count=" << graph.choices[0].successor_count << ";has_self=" << graph.choices[0].has_self << "\n";
  std::vector<solve_detail::PolicyRow> rows(2);
  rows[0].edge_count=1; rows[1].edge_offset=1; rows[1].edge_count=1;
  std::vector<solve_detail::PolicyEdge> edges{{1,0.5},{0,0.25}};
  std::vector<std::uint32_t> members{0,1}, components{0,0};
  std::vector<std::int32_t> local{0,1};
  std::vector<double> rhs{1,2}, previous{0,0};
  std::unique_ptr<solve_detail::SparsePolicyResume> resume;
  solve_detail::SparsePolicyComponentView view{members,0,components,local,rows,edges,rhs,previous,10000};
  const auto cycle=solve_detail::advance_sparse_policy_component(view,resume);
  std::cout << "cycle_status=" << int(cycle.status) << ";values=" << cycle.values[0] << "," << cycle.values[1] << ";expected=16/7,18/7\n";
  ExecutableContinuationAuthorityContext authority;
  authority.goal={1};authority.economy={2};authority.mechanics_artifact={3};
  authority.caller_scope={4};authority.action_vocabulary={5};authority.terminal_semantics={6};
  refinement::StableKey strategy{700,701};
  refinement::PolicyPotentialCandidateEntry entry;
  entry.identity.authority=authority;entry.identity.strategy=strategy;
  entry.identity.exact_entry={10,1010};entry.identity.exact_item={2010,3010};
  entry.policy_value=10;entry.existing_lower=0;entry.caller_authorized_actions=3;
  entry.selected_kernel.status=refinement::PolicyPotentialSelectedKernelStatus::Complete;
  entry.selected_kernel.source=entry.identity;entry.selected_kernel.selected_operator_identity={800,10};
  entry.selected_kernel.semantic_identity={900,10};entry.selected_kernel.mandatory_expected_cost=10;
  entry.selected_kernel.probability_mass=1;entry.selected_kernel.bellman_residual=0;
  entry.selected_kernel.transitions={{{},1,true}};
  refinement::PolicyPotentialActionConstraint constraint;
  constraint.kind=refinement::PolicyPotentialConstraintKind::ExistingTypedLower;
  constraint.source=entry.identity;constraint.action_identity={1};constraint.semantic_identity={600,1};
  constraint.existing_lower_identity={601,1};constraint.rhs=10;constraint.complete=true;
  entry.action_constraints={constraint,constraint};
  const auto duplicate=refinement::certify_policy_potential_cegar_shadow(authority,strategy,{entry});
  std::cout << "duplicate_certified_entries=" << duplicate.certified_entries << ";constraints=" << duplicate.constraints_examined << ";unique_alternative_actions=1;claimed_authorized_actions=3;failure=" << duplicate.failure_reason << "\n";
  bool ok=choice==2 && cycle.status==solve_detail::SparsePolicyComponentStatus::Complete && std::abs(cycle.values[0]-16.0/7)<1e-12 && std::abs(cycle.values[1]-18.0/7)<1e-12 && duplicate.certified_entries==1;
  return ok ? 0 : 1;
}
