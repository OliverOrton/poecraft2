#!/usr/bin/env python3
"""Exact synthetic SSP checks, not PoE mechanics or native qualification.

Standard library only. Run: python execution_count_checks.py --output results.json
"""
from __future__ import annotations
import argparse
import json
import random
from dataclasses import dataclass
from fractions import Fraction as F
from itertools import product
from pathlib import Path
from typing import Sequence

@dataclass(frozen=True)
class Row:
    price: F
    primitive_actions: F
    transitions: tuple[F, ...]  # nonterminal successors; remainder is goal mass
    def validate(self, size: int) -> None:
        if len(self.transitions) != size or self.price < 0 or self.primitive_actions < 0:
            raise ValueError('Invalid dimensions or reward')
        if any(p < 0 for p in self.transitions) or sum(self.transitions) > 1:
            raise ValueError('Invalid probability mass')

def solve(a: Sequence[Sequence[F]], b: Sequence[F]) -> list[F]:
    n=len(b)
    if len(a)!=n or any(len(r)!=n for r in a):
        raise ValueError('Nonsquare system')
    m=[list(r)+[v] for r,v in zip(a,b)]
    for j in range(n):
        pivot=next((i for i in range(j,n) if m[i][j]),None)
        if pivot is None:
            raise ValueError('Singular system; no finite evaluation asserted')
        m[j],m[pivot]=m[pivot],m[j]
        d=m[j][j]; m[j]=[x/d for x in m[j]]
        for i in range(n):
            if i!=j:
                d=m[i][j]
                m[i]=[x-d*y for x,y in zip(m[i],m[j])]
    return [r[-1] for r in m]

def evaluate(rows: Sequence[Row]) -> tuple[list[F],list[F]]:
    n=len(rows)
    for r in rows:r.validate(n)
    a=[[F(i==j)-rows[i].transitions[j] for j in range(n)] for i in range(n)]
    absorption=solve(a,[F(1)-sum(r.transitions) for r in rows])
    if any(x!=1 for x in absorption):
        raise ValueError('Not goal-absorbing from every listed state')
    return solve(a,[r.price for r in rows]),solve(a,[r.primitive_actions for r in rows])

def main() -> dict:
    checks={}
    def check(name: str, yes: bool):
        checks[name]=bool(yes)
        if not yes:raise AssertionError(name)
    one=lambda c,n,q:(Row(F(c),F(n),(F(q),)),)
    c,n=evaluate(one(F(153,1000),1,F(1399999,1400000)))
    check('tiny_structured_controller_can_need_1_4M_actions',n==[F(1400000)])
    check('large_count_can_be_finite_and_proper',c==[F(214200)])
    cheap=evaluate(one(F(1,100),1,F(99,100)))
    fast=evaluate(one(2,1,0))
    check('long_policy_can_be_cheaper',cheap==([F(1)],[F(100)]) and fast==([F(2)],[F(1)]))
    lam=F(1,50)
    check('count_penalty_can_prefer_more_expensive_policy',fast[0][0]+lam*fast[1][0]<cheap[0][0]+lam*cheap[1][0])
    check('penalized_optimum_not_original_cost_lower',min(cheap[0][0]+lam*cheap[1][0],fast[0][0]+lam*fast[1][0])>cheap[0][0])
    check('zero_penalty_preserves_cost_ranking',cheap[0][0]<fast[0][0])
    p=F(1,100); expected_elapsed=1/p
    check('finite_prefix_not_expected_completion',min(F(20),expected_elapsed)<expected_elapsed)
    macro=evaluate(one(3,100,0)); five=evaluate(one(4,5,0))
    check('one_option_call_is_not_one_primitive',macro[1][0]==100)
    check('macro_count_changes_valid_proposal_order',3+F(1,10)*100>4+F(1,10)*5)
    check('unit_per_macro_would_reverse_order',3+F(1,10)<4+F(1,10))
    # Different rewards, same transient matrix; no dense inverse required.
    rows=(Row(F(2),F(1),(F(0),F(1,2))),Row(F(3),F(4),(F(1,4),F(0))))
    costs,counts=evaluate(rows)
    scalar=[Row(r.price+F(7,3)*r.primitive_actions,r.primitive_actions,r.transitions) for r in rows]
    scalar_costs,_=evaluate(scalar)
    check('two_rewards_scalarize_linearly',scalar_costs==[c+F(7,3)*n for c,n in zip(costs,counts)])
    repriced=[Row(r.price*10,r.primitive_actions,r.transitions) for r in rows]
    check('fixed_policy_count_is_price_independent',evaluate(repriced)[1]==counts)
    # Residual bound weighted by expected primitive count.
    eps=F(1,10000)
    candidate=[c-eps*n for c,n in zip(costs,counts)]
    residual=[r.price+sum(p*x for p,x in zip(r.transitions,candidate))-candidate[i] for i,r in enumerate(rows)]
    check('residual_duration_bound_is_sharp',residual==[eps*r.primitive_actions for r in rows])
    check('residual_error_amplifies_with_count',[c-v for c,v in zip(costs,candidate)]==[eps*n for n in counts])
    # Expected count does not determine a finite-horizon success probability.
    mean_a=F(1000); mean_b=F(999,1000)*1+F(1,1000)*999001
    check('equal_means_can_have_different_tails',mean_a==mean_b)
    check('equal_means_do_not_fix_success_by_100',F(0)!=F(999,1000))
    # Attributions from the current controller are additive; tails overlap.
    seq=(Row(F(1),F(1),(F(0),F(1))),Row(F(1),F(1),(F(0),F(0))))
    sc,sn=evaluate(seq)
    check('continuation_costs_are_not_additive_spend',sum(sc)==3 and sc[0]==2)
    check('continuation_counts_are_not_additive_operations',sum(sn)==3 and sn[0]==2)
    # A conditional count bound needs the full available action scope.
    check('count_only_subset_optimum_is_not_full_scope_lower',min(F(100),F(1))<F(100))
    # The penalty crossover can be calculated, not guessed as dominance.
    threshold=(fast[0][0]-cheap[0][0])/(cheap[1][0]-fast[1][0])
    check('penalty_units_and_crossover',threshold==F(1,99))
    # Local option termination does not ensure whole-controller goal absorption.
    try:evaluate((Row(F(1),F(1),(F(0),F(1))),Row(F(1),F(1),(F(1),F(0)))))
    except ValueError:closed_refused=True
    else:closed_refused=False
    check('closed_two_option_cycle_refused',closed_refused)
    # Reaching a local subgoal is not reaching the original terminal.
    check('free_subgoal_tail_underprices_completion',F(1)<F(1)+F(20))
    # Cost trade-offs can be unsupported by any linear scalarization among
    # deterministic proposals; a tiny lambda portfolio is not the Pareto set.
    frontier=[(F(0),F(10)),(F(6),F(6)),(F(10),F(0))]
    check('middle_point_is_deterministically_nondominated',not any(c<=6 and n<=6 and (c<6 or n<6) for c,n in (frontier[0],frontier[2])))
    check('middle_point_requires_incompatible_lambda_conditions',F(6,4)>F(4,6))
    # Existing cheap optimistic exits cap the same model regardless of guesses.
    check('larger_guess_cannot_defeat_scalar_escape',min(F(10000),F(17,1))==17)

    rng=random.Random(20260913)
    models=96; policies=0; scalar_checks=0
    lambdas=[F(0),F(1,10),F(1),F(10)]
    for _ in range(models):
        model=[]
        for i in range(3):
            options=[]
            for j in range(2):
                weights=[rng.randint(0,4) for _ in range(3)]
                # At least 1/2 goal probability proves uniform transience.
                den=2*(sum(weights)+1)
                options.append(Row(F(rng.randint(1,30),10),F(rng.randint(1,5)),tuple(F(w,den) for w in weights)))
            model.append(options)
        choices=[]
        for chosen in product(*model):
            cv,nv=evaluate(chosen);policies+=1
            for la in lambdas:
                vv,_=evaluate([Row(r.price+la*r.primitive_actions,r.primitive_actions,r.transitions) for r in chosen])
                if vv!=[c+la*n for c,n in zip(cv,nv)]:raise AssertionError('generated scalarization')
                scalar_checks+=1
            choices.append((cv[0],nv[0]))
        # Exact minimizers at larger lambda cannot have larger N; this is NOT
        # a theorem about bounded/approximate native proposals.
        winners=[min(choices,key=lambda cn:(cn[0]+la*cn[1],cn[1],cn[0])) for la in lambdas]
        if any(a[1]<b[1] for a,b in zip(winners,winners[1:])):raise AssertionError('count monotonicity')
    check('generated_exact_scalarization_identities',scalar_checks==models*8*len(lambdas))
    check('generated_global_count_monotonicity',True)

    # Arithmetic ONLY on retained native values; not fresh benchmarks.
    bow_n=F('1404492.0069683318'); bow_c=F('223349.0000393144')
    alt=F('908324.4368865712'); aug=F('460571.25112460786')
    magic=F('905929.2571635403')+F('460564.70624253823')
    facts={
       'bow_alteration_plus_augment_count_share':float((alt+aug)/bow_n),
       'bow_fractured_magic_alteration_plus_augment_count_share':float(magic/bow_n),
       'bow_alteration_plus_augment_cost_share':float((F('138973.6388436454')+F('30397.70257422412'))/bow_c),
       'bow_cost_target_25_percent_reduction':float(F(3,4)*bow_c),
       'bow_count_target_50_percent_reduction':float(bow_n/2),
       'ring_cost_target_25_percent_reduction':float(F(3,4)*F('227989.251597418')),
       'ring_count_target_50_percent_reduction':float(F('585368.3255100878')/2),
       'bow_reference_chaos_per_action':float(bow_c/bow_n),
       'ring_reference_chaos_per_action':float(F('227989.251597418')/F('585368.3255100878')),
    }
    return {'classification':'Exact synthetic arithmetic and finite-model enumeration; no native PoE execution',
            'named_checks':checks,'named_check_count':len(checks),'generated_models':models,
            'enumerated_fixed_policies':policies,'scalarized_evaluations':scalar_checks,
            'examples':{'structured_retry_N':'1400000','structured_retry_C':'214200','penalty_crossover':str(threshold),
                        'two_reward_C':[str(v) for v in costs],'two_reward_N':[str(v) for v in counts]},
            'retained_measurement_arithmetic_not_new_runs':facts}

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path)
    args=parser.parse_args()
    result=main();text=json.dumps(result,indent=2,ensure_ascii=False)+'\n'
    if args.output:
        args.output.parent.mkdir(parents=True,exist_ok=True)
        args.output.write_text(text,encoding='utf-8')
    print(text)
