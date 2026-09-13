#!/usr/bin/env python3
"""Exact synthetic SSP checks; no PoE mechanics, native execution or speed claims.

Run: python checked_estimate_examples.py --output checked_estimate_results.json
Uses Python's standard library only. A row here is an already supplied complete
model constraint; this script does not construct or certify a native abstraction.
"""
from __future__ import annotations
import argparse
import json
import random
from dataclasses import dataclass
from fractions import Fraction as F
from itertools import product
from pathlib import Path
from typing import Mapping, Sequence

@dataclass(frozen=True)
class Row:
    name: str
    cost: F
    transitions: Mapping[str, F]
    def validate(self) -> None:
        if self.cost < 0 or any(p < 0 for p in self.transitions.values()):
            raise ValueError('Negative cost or probability')
        if sum(self.transitions.values(), F()) != 1:
            raise ValueError('Incomplete probability mass')

Model = Mapping[str, Sequence[Row]]

def q(row: Row, values: Mapping[str, F]) -> F:
    row.validate()
    return row.cost + sum((p * values[t] for t, p in row.transitions.items()), F())

def feasible(model: Model, values: Mapping[str, F], boundary: Mapping[str, F]) -> bool:
    if set(values) != set(model) | set(boundary):
        return False
    if any(v < 0 for v in values.values()) or any(values[s] != b for s,b in boundary.items()):
        return False
    return all(rows and all(values[s] <= q(row, values) for row in rows)
               for s, rows in model.items())

def lift(model: Model, base: Mapping[str,F], estimate: Mapping[str,F],
         boundary: Mapping[str,F]) -> tuple[dict[str,F], F, list[dict[str,str]]]:
    """Largest feasible alpha along a fixed nonnegative direction, alpha in [0,1]."""
    if set(estimate) != set(base) or not feasible(model, base, boundary):
        raise ValueError('Base is not feasible for THIS complete model/coordinate set')
    if any(estimate[s] != b for s,b in boundary.items()):
        raise ValueError('Fixed boundary changed')
    direction = {s: (max(F(), estimate[s] - base[s]) if s in model else F()) for s in base}
    limits = []
    alpha = F(1)
    for s, rows in model.items():
        for row in rows:
            slack = q(row, base) - base[s]
            slope = direction[s] - sum((p*direction[t] for t,p in row.transitions.items()), F())
            if slope > 0:
                bound = slack / slope
                alpha = min(alpha, bound)
                limits.append((bound, s, row.name, slack, slope))
    accepted = {s: base[s] + alpha*direction[s] for s in base}
    if not feasible(model, accepted, boundary):
        raise AssertionError('Ray arithmetic failed final independent feasibility check')
    limiting = [{'state':s,'action':a,'slack':str(b),'slope':str(d),'alpha':str(r)}
                for r,s,a,b,d in limits if r == alpha]
    return accepted, alpha, limiting

def solve(a: Sequence[Sequence[F]], b: Sequence[F]) -> list[F] | None:
    n = len(b)
    m = [list(row) + [b[i]] for i,row in enumerate(a)]
    for col in range(n):
        pivot = next((i for i in range(col,n) if m[i][col]), None)
        if pivot is None: return None
        m[col],m[pivot]=m[pivot],m[col]
        v=m[col][col]; m[col]=[x/v for x in m[col]]
        for i in range(n):
            if i != col and m[i][col]:
                v=m[i][col]; m[i]=[x-v*y for x,y in zip(m[i],m[col])]
    return [row[-1] for row in m]

def evaluate(policy: Mapping[str,Row], boundary: Mapping[str,F]) -> dict[str,F] | None:
    names=list(policy); idx={s:i for i,s in enumerate(names)}; n=len(names)
    a=[[F(i==j) for j in range(n)] for i in range(n)]
    b=[]; mass=[]
    for s,row in policy.items():
        row.validate(); i=idx[s]; val=row.cost; escape=F()
        for t,p in row.transitions.items():
            if t in idx: a[i][idx[t]]-=p
            elif t in boundary: val+=p*boundary[t]; escape+=p
            else: raise ValueError('Uncovered exit')
        b.append(val); mass.append(escape)
    absorbed=solve(a,mass)
    if absorbed is None or any(v!=1 for v in absorbed): return None
    v=solve(a,b)
    return None if v is None else dict(zip(names,v)) | dict(boundary)

def optimum(model: Model, boundary: Mapping[str,F]) -> tuple[dict[str,F],int]:
    values=[]; count=0
    for rows in product(*(model[s] for s in model)):
        count+=1; v=evaluate(dict(zip(model,rows)),boundary)
        if v is not None: values.append(v)
    if not values: raise ValueError('No all-listed-entry proper controller')
    return {s:min(v[s] for v in values) for s in list(model)+list(boundary)}, count

def main() -> dict:
    checks={}
    def check(name: str, condition: bool) -> None:
        checks[name]=bool(condition)
        if not condition: raise AssertionError(name)
    g={'g':F()}
    model={'s':[Row('finish',F(40),{'g':F(1)}),Row('retry',F(12),{'s':F(1,2),'g':F(1,2)})]}
    base={'s':F(4),'g':F()}; guess={'s':F(100),'g':F()}
    repaired,alpha,limiting=lift(model,base,guess,g)
    check('estimate_100_is_not_a_lower',not feasible(model,guess,g))
    check('ray_accepts_24',repaired['s']==24)
    check('alpha_is_5_over_24',alpha==F(5,24))
    check('retry_is_limiting',limiting[0]['action']=='retry')
    check('matches_enumerated_optimum',optimum(model,g)[0]['s']==24)
    frozen,zero,_=lift(model,repaired,guess,g)
    check('saturated_model_does_not_improve',zero==0 and frozen==repaired)
    half={'s':F(50),'g':F()}
    check('arbitrary_safety_factor_not_valid',not feasible(model,half,g))
    try: lift({'s':[Row('cheap',F(1),{'g':F(1)})]},base,guess,g)
    except ValueError: invalid_base=True
    else: invalid_base=False
    check('old_independent_bound_may_be_infeasible_here',invalid_base)
    cheaper={'s':list(model['s'])+[Row('overlooked',F(1),{'g':F(1)})]}
    check('omitted_action_refutes_24',not feasible(cheaper,repaired,g))
    fix,_,_=lift(cheaper,{'s':F(),'g':F()},guess,g)
    check('full_scope_repairs_to_1',fix['s']==1)

    # The first blocking inequality limits a RAY, not the whole model.
    separable={'x':[Row('finish_x',F(100),{'g':F(1)})], 'y':[Row('finish_y',F(1),{'g':F(1)})]}
    z={'x':F(),'y':F(),'g':F()}; e={'x':F(100),'y':F(100),'g':F()}
    one,_,_=lift(separable,z,e,g)
    two,_,_=lift(separable,one,{'x':F(100),'y':F(1),'g':F()},g)
    check('global_ray_can_be_weak',one['x']==one['y']==1)
    check('changed_direction_can_recover',two['x']==100 and two['y']==1)
    check('pointwise_max_compatible_subsolutions',feasible(separable,{s:max(one[s],two[s]) for s in one},g))

    # Independent boundary values are held FIXED, not learned free exits.
    boundary={'g':F(),'d':F(4)}
    region={'r':[Row('direct',F(10),{'g':F(1)}),Row('deviate',F(2),{'g':F(1,2),'q':F(1,2)})],
            'q':[Row('retry_exit',F(1),{'q':F(1,2),'d':F(1,2)})]}
    weak,_=optimum(region,boundary)
    strong,_=optimum(region,{'g':F(),'d':F(40)})
    check('auxiliary_weak_model_ceiling_5',weak['r']==5)
    check('stronger_boundary_changes_model',strong['r']==10)
    check('old_accepted_strong_vector_not_weak_feasible',not feasible(region,strong,boundary))
    try: lift(region,{'r':F(),'q':F(),**boundary},{'r':F(10),'q':F(42),'g':F(),'d':F(40)},boundary)
    except ValueError: fixed=True
    else: fixed=False
    check('boundary_mutation_refused',fixed)

    # Observed choice: each complete post-observation decision constrains the
    # lower. Freezing the initially minimizing option fails after a crossing.
    observed={'r':[Row('observe_choose_x',F(),{'x':F(1)}),Row('observe_choose_y',F(),{'y':F(1)})],
              'x':[Row('finish_x',F(21),{'g':F(1)})], 'y':[Row('finish_y',F(10),{'g':F(1)})]}
    ob={'r':F(),'x':F(1),'y':F(10),'g':F()}
    oe={'r':F(15),'x':F(21),'y':F(10),'g':F()}
    ov,oa,_=lift(observed,ob,oe,g)
    check('choice_crossover_rechecked',oa==F(2,3) and ov['r']==10)
    stale=dict(observed); stale['r']=[observed['r'][0]]
    sv,_,_=lift(stale,ob,oe,g)
    check('frozen_choice_can_accept_false_vector',sv['r']==15 and not feasible(observed,sv,g))
    post=(min(F(0),F(8))+min(F(8),F(0)))/2
    pre=min((F(0)+8)/2,(F(8)+0)/2)
    check('decision_timing_min_expectation',post==0 and pre==4)

    zero_loop={'s':[Row('zero_loop',F(),{'s':F(1)}),Row('finish',F(5),{'g':F(1)})]}
    zv,_,_=lift(zero_loop,{'s':F(),'g':F()},{'s':F(100),'g':F()},g)
    check('finite_subsolution_can_escape_zero_iteration',zv['s']==5)
    check('zero_loop_is_not_proper',evaluate({'s':zero_loop['s'][0]},g) is None)
    check('proper_optimum_with_zero_loop',optimum(zero_loop,g)[0]['s']==5)
    tiny=F(1,10**9)
    huge={'s':[Row('rare_retry',F(1),{'s':1-tiny,'g':tiny})]}
    hv,_=optimum(huge,g)
    check('rare_retry_expected_cost',hv['s']==10**9)
    almost={'s':hv['s']+1,'g':F()}
    check('small_residual_not_acceptance',almost['s']-q(huge['s'][0],almost)==tiny and not feasible(huge,almost,g))
    try: Row('bad',F(1),{'g':F(9,10)}).validate()
    except ValueError: mass_ok=True
    else: mass_ok=False
    check('missing_mass_refused',mass_ok)

    # Economic attribution: add immediate spend, not overlapping value tails.
    serial={'r':Row('step',F(1),{'q':F(1)}),'q':Row('finish',F(1),{'g':F(1)})}
    ev=evaluate(serial,g)
    check('immediate_spend_adds_to_root',ev is not None and F(1)+1==ev['r'])
    check('overlapping_continuation_costs_double_count',ev is not None and ev['r']+ev['q']>ev['r'])
    check('one_shot_vs_repeat',F(1)+F(99,100)*1000>F(100)+F(4,5)*1000 and F(1)/(1-F(99,100))<F(100)/(1-F(4,5)))
    check('two_cooperating_decisions_can_win',1+100>10 and 1+1<10)

    rng=random.Random(20260913)
    model_count=128; policies=0; limiting_cases=0
    for _ in range(model_count):
        names=['a','b','c']; m={}
        for s in names:
            rows=[]
            for j in range(2):
                # Every action has positive goal mass: all finite policies proper.
                weights=[rng.randrange(0,8) for _ in names]+[rng.randrange(1,8)]
                den=sum(weights)
                rows.append(Row(f'{s}_{j}',F(rng.randrange(0,20)),dict(zip(names+['g'],(F(w,den) for w in weights)))))
            m[s]=rows
        opt,num=optimum(m,g); policies+=num
        b={s:opt[s]/4 for s in names}|g
        e={s:opt[s]*F(rng.randrange(1,17),4)+rng.randrange(0,10) for s in names}|g
        v,a,lim=lift(m,b,e,g)
        if a<1: limiting_cases+=1
        if not all(b[s]<=v[s]<=opt[s] for s in names): raise AssertionError('Generated feasibility/soundness failure')
        if a<1:
            d={s:max(F(),e[s]-b[s]) for s in b}
            greater={s:b[s]+(a+(1-a)/2)*d[s] for s in b}
            if feasible(m,greater,g): raise AssertionError('Claimed alpha not maximal on this ray')
    check('generated_all_action_validity_and_optimality_comparison',True)
    check('generated_nontrivial_limited_rays',limiting_cases>0)
    return {'classification':'Exact rational synthetic illustrations only; no native solver measurements or universal performance claim',
            'checks':checks,'named_checks':len(checks),
            'generated_models':model_count,'enumerated_complete_policies':policies,'limited_rays':limiting_cases,
            'worked_example':{'base':'4','estimate':'100','accepted':str(repaired['s']),'alpha':str(alpha),'limiting_constraints':limiting},
            'ray_vs_model_example':{'global_ray':{k:str(v) for k,v in one.items()},'changed_direction':{k:str(v) for k,v in two.items()}},
            'scope':'All constraints in these tiny models are supplied explicitly. Native action coverage, member-uniformity, numerical/native coefficient correspondence and performance remain implementation obligations.'}

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path)
    args=parser.parse_args()
    result=main(); text=json.dumps(result,indent=2,ensure_ascii=False)+'\n'
    if args.output:
        args.output.parent.mkdir(parents=True,exist_ok=True); args.output.write_text(text,encoding='utf-8')
    print(text)
