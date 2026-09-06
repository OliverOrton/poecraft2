#!/usr/bin/env python3
"""Exact toy checks for the accompanying mathematical documentation.

No poecraft2 imports, native mechanics, solver runs, network access, or simulation.
This checks examples and arithmetic, not universal claims or native conformance.
Run: python math_examples.py --output math_examples_results.json
"""
from __future__ import annotations
import argparse
from fractions import Fraction as F
import itertools
import json
from pathlib import Path
from typing import Mapping, Sequence

Row = tuple[F, Mapping[str, F]]
Model = Mapping[str, Sequence[Row]]

def solve_linear(a: list[list[F]], b: list[F]) -> list[F] | None:
    n = len(b)
    if len(a) != n or any(len(row) != n for row in a):
        raise ValueError('Expected a square system')
    m = [list(row) + [rhs] for row, rhs in zip(a, b)]
    for j in range(n):
        pivot = next((i for i in range(j, n) if m[i][j]), None)
        if pivot is None:
            return None
        m[j], m[pivot] = m[pivot], m[j]
        p = m[j][j]
        m[j] = [x / p for x in m[j]]
        for i in range(n):
            if i != j and m[i][j]:
                q = m[i][j]
                m[i] = [x - q*y for x, y in zip(m[i], m[j])]
    return [m[i][-1] for i in range(n)]


def fixed_policy_cost(model: Model, policy: Mapping[str, int], start: str,
                      goals: frozenset[str] = frozenset({'g'})) -> F | None:
    """Return a cost for a finite proper reachable policy; None for singular Q.

    Complete normalized rows are checked. This helper handles the supplied
    finite examples, not a general proof-certificate format.
    """
    if start in goals:
        return F(0)
    seen: set[str] = set()
    todo = [start]
    while todo:
        s = todo.pop()
        if s in goals or s in seen:
            continue
        if s not in model or s not in policy:
            raise ValueError(f'Missing model/policy at {s}')
        cost, dist = model[s][policy[s]]
        if cost < 0 or any(p < 0 for p in dist.values()) or sum(dist.values()) != 1:
            raise ValueError('Rows must have nonnegative cost and complete mass')
        seen.add(s)
        todo.extend(t for t, p in dist.items() if p > 0 and t not in seen)
    states = sorted(seen)
    a: list[list[F]] = []
    b: list[F] = []
    for s in states:
        cost, dist = model[s][policy[s]]
        a.append([F(int(s == t)) - dist.get(t, F(0)) for t in states])
        b.append(cost)
    vals = solve_linear(a, b)
    return None if vals is None else vals[states.index(start)]


def box_min(values: Sequence[F], caps: Sequence[F]) -> tuple[F, list[F]]:
    if len(values) != len(caps) or any(c < 0 or c > 1 for c in caps):
        raise ValueError('Invalid capacities')
    if sum(caps) < 1:
        raise ValueError('Infeasible normalized capacity box')
    p = [F(0)] * len(values)
    left = F(1)
    for i in sorted(range(len(values)), key=lambda k: (values[k], k)):
        p[i] = min(left, caps[i])
        left -= p[i]
        if not left:
            break
    return sum(x*y for x, y in zip(values, p)), p


def completion(a: Sequence[F], n: int, k: int) -> list[F]:
    if len(a) != 1 << n or not 0 <= k <= n:
        raise ValueError('Bad acquisition table shape')
    return [min(a[s] for s in range(1 << n) if (s | m).bit_count() >= k)
            for m in range(1 << n)]


def run() -> dict[str, object]:
    results: list[dict[str, object]] = []
    def record(name: str, claims: list[str], **values: object) -> None:
        def enc(v: object) -> object:
            if isinstance(v, F):
                return str(v)
            if isinstance(v, (list, tuple)):
                return [enc(x) for x in v]
            if isinstance(v, dict):
                return {str(k): enc(x) for k, x in v.items()}
            return v
        results.append({'name': name, 'claims': claims, 'passed': True,
                        'values': {k: enc(v) for k, v in values.items()}})

    model = {'s': [(F(0), {'s': F(1)}), (F(5), {'g': F(1)})]}
    assert fixed_policy_cost(model, {'s': 0}, 's') is None
    assert fixed_policy_cost(model, {'s': 1}, 's') == 5
    assert all(min(x, F(5)) == x for x in [F(0), F(2), F(5)])
    record('zero_cost_loop_vs_proper_finish', ['CLM-0001','CLM-0007'], proper_cost=F(5), raw_loop_cost=F(0))

    p = F(1, 4)
    model = {'s': [(F(3), {'s': 1-p, 'g': p})]}
    assert fixed_policy_cost(model, {'s': 0}, 's') == 12
    record('transient_fixed_policy_equation', ['CLM-0002'], value=F(12))

    model = {'r': [(F(1), {'g': F(1)})], 'z': [(F(0), {'z': F(1)})]}
    assert fixed_policy_cost(model, {'r': 0, 'z': 0}, 'r') == 1
    assert fixed_policy_cost(model, {'r': 0, 'z': 0}, 'z') is None
    record('root_proper_does_not_cover_other_entry', ['CLM-0002'], root=F(1), other_entry='improper')

    model = {'s': [(F(1), {'t': F(1)})], 't': [(F(1), {'s': F(1)})]}
    assert fixed_policy_cost(model, {'s': 0, 't': 0}, 's') is None
    record('locally_terminating_options_globally_cycle', ['CLM-0004'], global_result='improper')

    a, b = (F(0), F(10)), (F(10), F(0))
    before = min(sum(a)/2, sum(b)/2)
    after = sum(min(x,y) for x,y in zip(a,b))/2
    assert before == 5 and after == 0
    record('observation_timing', ['CLM-0003'], before=before, after=after)

    assert F(9,10) + F(1,10) == 1
    assert F(9,10) / F(9,10) == 1 and F(9,10) < 1
    record('conditioning_away_failure_changes_model', ['CLM-0004'], real_success=F(9,10), renormalized=F(1))

    vals, lows, uppers = [F(2),F(100)], [F(2),F(80)], [F(3),F(110)]
    assert all(min(lows) <= v for v in vals)
    assert not all(max(lows) <= v for v in vals)
    assert all(v <= max(uppers) for v in vals)
    record('member_aggregation_quantifiers', ['CLM-0006'], class_lower=min(lows), class_scalar_upper=max(uppers))

    actions = [F(10), F(1)]
    assert min(actions) == 1 and min(actions[:1]) == 10
    record('omitted_cheap_action', ['CLM-0008','CLM-0009'], full=F(1), restricted=F(10))

    assert len(['A','A']) == len(['A','B']) and set(['A','A']) != set(['A','B'])
    record('counts_do_not_establish_action_set', ['CLM-0008'], duplicated=['A','A'], needed=['A','B'])

    value, l1, l2 = F(5), F(5), F(5)
    assert max(l1,l2) <= value and l1+l2 > value
    assert F(2)+F(3) <= value
    record('maximum_not_unallocated_sum', ['CLM-0010'], maximum=max(l1,l2), invalid_sum=l1+l2)

    lower, action_costs = F(4), [F(5), F(7), F(10)]
    assert lower <= min(action_costs) and all(lower <= q for q in action_costs)
    record('state_lower_is_common_action_floor', ['CLM-0011'], common=lower)

    h, boundary = F(10), F(9)
    assert h <= 10 and h <= 1+boundary and boundary <= 20
    record('root_proof_without_outside_incumbent', ['CLM-0012'], root=h, outside_lower=boundary, outside_true=F(20))

    coupled = solve_linear([[F(1),F(-1,2)],[F(-1,2),F(1)]],[F(1),F(2)])
    assert coupled == [F(8,3),F(10,3)]
    assert coupled[0] == 1+coupled[1]/2 and coupled[1] == 2+coupled[0]/2
    record('simultaneous_coupled_region', ['CLM-0012'], values=coupled)

    assert F(10) == 1+F(9) and not F(10) <= 1+F(0)
    record('native_admissible_but_local_infeasible', ['CLM-0013'], old_lower=F(10), truncated_ceiling=F(1))

    caps = [F(3,5),F(3,5)]
    first, alloc = box_min([F(0),F(10)],caps)
    second, new_alloc = box_min([F(10),F(0)],caps)
    stale = sum(x*y for x,y in zip([F(10),F(0)],alloc))
    assert first == second == 4 and stale == 6 and second < 5 <= stale
    record('stale_minimum_expectation', ['CLM-0014'], first=first, new=second, stale=stale,
           first_allocation=alloc, correct_new_allocation=new_alloc)

    try:
        box_min([F(0),F(10)],[F(2,5),F(2,5)])
    except ValueError:
        pass
    else:
        raise AssertionError('Incomplete mass accepted')
    record('infeasible_probability_box_refuses', ['CLM-0014'], cap_sum=F(4,5))

    true_min,_ = box_min([F(0),F(10)],[F(1,3),F(1)])
    safe_min,_ = box_min([F(0),F(10)],[F(3,8),F(1)])
    unsafe_min,_ = box_min([F(0),F(10)],[F(1,4),F(1)])
    assert safe_min <= true_min < unsafe_min
    record('capacity_rounding_direction', ['CLM-0014','CLM-0023'], true=true_min, rounded_up=safe_min, rounded_down=unsafe_min)

    q=F(1,10)
    assert q > q**3
    record('unconditional_marginal_product_counterexample', ['CLM-0015'], joint=q, false_product=q**3)

    constant = 6*F(2,33)**3
    history = 6*F(2,85)*F(2,59)*F(2,33)
    assert constant == F(16,11979) and history == F(16,55165)
    assert history < constant < F(2,11)
    record('conditional_assignment_arithmetic', ['CLM-0015'], constant=constant, history=history,
           qualification='Arithmetic only; native history premises not checked here')

    assignments = list(itertools.permutations(range(1), 2))
    assert not assignments and F(1,2) > 0
    record('overlapping_goals_break_distinct_draw_assumption', ['CLM-0015'], distinct_assignments=0,
           possible_single_draw_joint_event=F(1,2))

    acquisition=[F(0),F(3),F(5),F(8)]
    all_two=completion(acquisition,2,2)
    any_one=completion(acquisition,2,1)
    assert all_two == [F(8),F(5),F(3),F(0)]
    assert any_one == [F(3),F(0),F(0),F(0)]
    record('acquisition_to_completion_coordinates', ['CLM-0016'], acquisition=acquisition, all_two=all_two, any_one=any_one)

    eligible={'a','b'}
    conflicts={'a':{'a','b'}, 'b':{'a','b'}}
    assert len(eligible)>0 and not eligible.difference(conflicts['a'])
    record('initial_nonempty_does_not_prove_second_draw', ['CLM-0017'], initial_count=2, after_first_count=0,
           qualification='Synthetic exclusion set, not a native pool')

    p=F(1,128)
    free=F(1)/p
    paid=(F(1)+(1-p)*2)/p
    assert free == 128 and paid == 382
    record('paid_failure_vs_free_rollback', ['CLM-0017','CLM-0018'], free=free, paid=paid)

    eps=F(233,20000)
    assert eps == F('0.01165') and F(36)>eps
    record('deterministic_relaxation_exit_ceiling', ['CLM-0018'], ceiling=eps,
           qualification='Synthetic inequality illustrating a documented kind of ceiling')

    assert min(10,10)==10 and min(50,10)==10 and min(50,50)==50
    record('tied_constraints_mask_single_refinement', ['CLM-0019'], baseline=10, one_refined=10, both_refined=50)

    cost, continuation, candidate=F(10),F(90),F(10)
    assert cost >= candidate and cost+continuation == 100
    record('computational_price_floor_not_semantic_ceiling', ['CLM-0019'], shortcut=cost, full_rhs=cost+continuation)

    x=F(2)
    assert x == 1+F(1,2)*min(x,F(100))
    wrong=F(51)
    assert wrong > 1+F(1,2)*min(wrong,F(100))
    record('self_choice_must_remain_in_raw_equation', ['CLM-0023'], valid=x, invalid=wrong)

    p=F(1,10**6); true=1/p; v=true-1
    residual=F(1)+(1-p)*v-v
    assert residual==p and true-v==1 and residual/p==1
    record('residual_amplification', ['CLM-0023'], residual=residual, value_error=true-v)

    unfiltered=F(1)/F(1,100)
    filtered=F(1)/F(1,2)+2
    assert unfiltered==100 and filtered==4 and unfiltered>filtered
    record('dropping_filter_is_not_lower_transfer', ['CLM-0025'], unfiltered=unfiltered, filtered_policy=filtered)

    u, lower_a=F(5),F(6)
    assert lower_a > u and not F(5) > u
    record('strict_local_dominance_not_equality', ['CLM-0020'], upper=u, retired_lower=lower_a)

    assert F(5)>F(1) and F(5)<F(10)
    record('price_changes_invalidate_old_value_use', ['CLM-0021'], old_value=F(5), lower_price_true=F(1), higher_price_true=F(10))

    l,u=F(4999,1000),F(5)
    assert f'{float(l):.2f}' == f'{float(u):.2f}' and u-l==F(1,1000)
    record('formatted_zero_gap_is_not_equality', ['CLM-0024'], lower=l, upper=u, true_gap=u-l)

    model={'s':[(F(10),{'g':F(1)}),(F(5),{'g':F(1)})]}
    incumbent=fixed_policy_cost(model,{'s':0},'s')
    better=fixed_policy_cost(model,{'s':1},'s')
    assert incumbent==10 and better==5 and incumbent>better
    record('fixed_policy_exact_evaluation_not_optimality', ['CLM-0002','CLM-0024'], incumbent=incumbent, optimum_in_example=better)

    return {'scope':'Exact synthetic arithmetic and counterexample checks only',
            'native_solver_runs':0, 'simulator_runs':0,
            'independent_reference_tool_run':False,
            'checks':len(results), 'passed':len(results), 'results':results}


def main() -> None:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path)
    args=parser.parse_args()
    if not __debug__:
        raise SystemExit('Do not run with -O: the examples use assertions')
    result=run()
    text=json.dumps(result,indent=2,ensure_ascii=False)+'\n'
    if args.output:
        args.output.parent.mkdir(parents=True,exist_ok=True)
        args.output.write_text(text,encoding='utf-8')
    print(f"{result['passed']}/{result['checks']} exact toy checks passed; no native runs")

if __name__=='__main__':
    main()
