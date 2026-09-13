#!/usr/bin/env python3
"""Exact synthetic checks for selective policy improvement.

Not PoE mechanics, native tests, performance evidence, or a new proof engine.
Uses only Python's standard library. Run with --output results.json.
"""
from __future__ import annotations
import argparse
import json
import random
from fractions import Fraction as F
from pathlib import Path
from typing import Sequence

Vector = list[F]
Matrix = list[Vector]


def dot(a: Sequence[F], b: Sequence[F]) -> F:
    if len(a) != len(b):
        raise ValueError('dimension mismatch')
    return sum((x*y for x, y in zip(a, b)), F(0))


def mv(a: Matrix, v: Vector) -> Vector:
    return [dot(row, v) for row in a]


def solve(a: Matrix, b: Vector) -> Vector:
    n = len(b)
    if len(a) != n or any(len(r) != n for r in a):
        raise ValueError('square system required')
    m = [list(r) + [b[i]] for i, r in enumerate(a)]
    for k in range(n):
        pivot = next((i for i in range(k, n) if m[i][k]), None)
        if pivot is None:
            raise ValueError('singular system: no finite evaluation supplied')
        m[k], m[pivot] = m[pivot], m[k]
        scale = m[k][k]
        m[k] = [x/scale for x in m[k]]
        for i in range(n):
            if i != k:
                scale = m[i][k]
                m[i] = [x-scale*y for x, y in zip(m[i], m[k])]
    return [r[-1] for r in m]


def system(p: Matrix) -> Matrix:
    n = len(p)
    if any(len(r) != n or any(x < 0 for x in r) or sum(r) > 1 for r in p):
        raise ValueError('finite substochastic matrix required')
    return [[F(i == j)-p[i][j] for j in range(n)] for i in range(n)]


def value(p: Matrix, c: Vector) -> Vector:
    if any(x < 0 for x in c):
        raise ValueError('nonnegative costs required')
    # For valid finite substochastic P, nonsingularity implies transience.
    return solve(system(p), c)


def one_row_update(p: Matrix, c: Vector, s: int, new_row: Vector, new_cost: F):
    j = value(p, c)
    z = solve(system(p), [F(i == s) for i in range(len(c))])
    dp = [q-r for q,r in zip(new_row, p[s])]
    residual = new_cost-c[s]+dot(dp,j)
    den = F(1)-dot(dp,z)
    if den <= 0:
        raise ValueError('update has no positive transient denominator')
    return [x+y*residual/den for x,y in zip(j,z)], residual, den


def main() -> dict:
    checks: dict[str, bool] = {}
    def check(name: str, condition: bool):
        if name in checks:
            raise AssertionError('duplicate check label')
        checks[name] = bool(condition)
        if not condition:
            raise AssertionError(name)

    # A common family multiplier cannot change within-family rankings.
    for k, mult in enumerate((F(1,10),F(1),F(10))):
        check(f'family_rescale_order_{k}', (F(1,2)*100*mult < F(3,5)*100*mult))
    check('different_family_rescale_can_change_order', 20 > 10 and F(1,10)*20 < 10)

    # Same number of forced goals does not make acquisition actions equivalent.
    # Synthetic fully paid renewal rows, not real currency mechanics.
    cheap = value([[F(999,1000)]], [F(1)])[0]
    dear = value([[F(1,2)]], [F(5)])[0]
    check('cheap_guarantee_can_cost_more', cheap == 1000 and dear == 10)
    check('equal_forced_count_not_a_dominance_proof', F(1)/1 < F(5)/1 and cheap > dear)

    # Old occupancy is a ranking proxy, not the new policy's exact root gain.
    p,c = [[F(99,100)]],[F(1)]
    old=value(p,c)[0]
    direct,r_direct,d_direct=one_row_update(p,c,0,[F(0)],F(80))
    repeat,r_repeat,d_repeat=one_row_update(p,c,0,[F(99,100)],F(1,10))
    check('old_policy_value', old == 100)
    check('one_row_direct_value', direct == [F(80)] and d_direct == 100)
    check('one_row_repeat_value', repeat == [F(10)] and d_repeat == 1)
    old_visits = F(100)
    check('old_occupancy_ranking_reverses', -old_visits*r_direct > -old_visits*r_repeat
          and old-direct[0] < old-repeat[0])
    check('candidate_values_match_fresh_solves', direct == value([[F(0)]],[F(80)])
          and repeat == value(p,[F(1,10)]))
    try:
        one_row_update(p,c,0,[F(1)],F(1))
        trap_refused=False
    except ValueError:
        trap_refused=True
    check('closed_retry_update_refused', trap_refused)

    # An option ending at a dirty entry is not a free terminal or root value.
    two_p=[[F(0),F(1)],[F(0),F(0)]]
    correct=value(two_p,[F(2),F(40)])[0]
    check('unknown_exit_not_free_goal', correct == 42 and correct != 2)
    check('root_cost_not_arbitrary_entry', correct != value(two_p,[F(2),F(40)])[1])
    before_obs = min(F(1,2)*0+F(1,2)*10, F(1,2)*10+F(1,2)*0)
    after_obs = F(1,2)*min(0,10)+F(1,2)*min(10,0)
    check('observation_timing_preserved', before_obs == 5 and after_obs == 0)
    check('rare_tail_not_droppable', F(999,1000)*2+F(1,1000)*10000 > 2)
    check('restricted_optimum_not_full_lower', min(F(20),F(3)) == 3 and 20 > 3)

    # Renewal threshold: local finishing cost versus the probability of escaping.
    # Cost a per attempt, direct success p0; a disjoint event w may trigger
    # a complete local controller costing k, then goal p or exact return 1-p.
    a,p0,w,k,escape=F(1),F(1,100),F(1,10),F(2),F(1,2)
    old=a/p0
    new=(a+w*k)/(p0+w*escape)
    check('conditional_continuation_threshold', k < escape*old and new < old)
    check('complete_renewal_cost', old == 100 and new == 20)
    break_even_k = escape*old
    tie=(a+w*break_even_k)/(p0+w*escape)
    check('renewal_threshold_tie', tie == old)
    check('rare_opportunity_can_have_material_gain',
          (F(1)+F(1,10000)*1)/F(2,10000) == F(10001,2))

    # Joint updates can be essential: no strictly improving single-state move.
    p0=[[F(0),F(0)],[F(0),F(0)]]
    c0=[F(10),F(100)]
    p1=[[F(0),F(1)],[F(0),F(0)]]
    c1=[F(1),F(1)]
    check('root_only_change_can_look_bad', value(p1,[F(1),F(100)])[0] == 101)
    check('joint_dependency_patch_can_win', value(p1,c1)[0] == 2)
    check('unreachable_tail_improvement_not_root_gain', value(p0,[F(10),F(1)])[0] == 10)
    # A weak choice can be deferred safely, but cannot be erased from proof.
    check('soft_deferral_not_retirement', F(1) < F(10) < F(100))

    rng=random.Random(20260912)
    trials=256
    for t in range(trials):
        n=3
        pp=[]
        for i in range(n):
            counts=[rng.randint(0,9) for _ in range(n)]
            terminal=rng.randint(1,9)
            total=sum(counts)+terminal
            pp.append([F(x,total) for x in counts])
        cc=[F(rng.randint(1,20),rng.randint(1,5)) for _ in range(n)]
        s=rng.randrange(n)
        counts=[rng.randint(0,9) for _ in range(n)]
        terminal=rng.randint(1,9)
        total=sum(counts)+terminal
        newrow=[F(x,total) for x in counts]
        newc=F(rng.randint(1,20),rng.randint(1,5))
        pred,res,den=one_row_update(pp,cc,s,newrow,newc)
        qq=[r[:] for r in pp]; qq[s]=newrow
        dd=cc[:]; dd[s]=newc
        fresh=value(qq,dd)
        if pred != fresh:
            raise AssertionError(f'one-row identity {t}')
        jp=value(pp,cc)
        adv=[dd[i]+dot(qq[i],jp)-jp[i] for i in range(n)]
        delta=solve(system(qq),adv)
        if any(fresh[i]-jp[i] != delta[i] for i in range(n)):
            raise AssertionError(f'performance difference {t}')
    check('random_exact_one_row_and_performance_difference', True)

    # Work-count toy: many unselected alternatives cost real generation but are
    # unnecessary for one complete improved upper. Not a native speedup claim.
    eager = 5*10000+1
    selective = 1
    check('candidate_specific_work_can_be_smaller', eager > selective)
    return {
        'classification':'exact synthetic arithmetic and finite SSP checks; NOT native PoE evidence',
        'named_checks':len(checks), 'checks':checks,
        'random_finite_models':trials,
        'random_model_contract':'3 transient states; every row has positive goal mass; fixed seed',
        'worked_examples':{
            'family_multiplier':'positive common multiplier preserves within-family order',
            'guaranteed_goal_counterexample':{'cheap_attempt_value':str(cheap),'expensive_attempt_value':str(dear)},
            'old_occupancy_counterexample':{'base':'100','direct_finish':'80','retry_improvement':'10',
                'old_occupancy_predicted_gains':['2000','90'],'true_gains':['20','90']},
            'conditional_renewal':{'old':'100','new':'20','break_even_local_cost':str(break_even_k)},
            'joint_dependency':{'old_root':'10','root_change_only':'101','joint_change':'2'}
        }
    }

if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path)
    args=parser.parse_args()
    out=main()
    text=json.dumps(out,indent=2,sort_keys=True)+'\n'
    if args.output:
        args.output.parent.mkdir(parents=True,exist_ok=True)
        args.output.write_text(text,encoding='utf-8')
    print(text)
