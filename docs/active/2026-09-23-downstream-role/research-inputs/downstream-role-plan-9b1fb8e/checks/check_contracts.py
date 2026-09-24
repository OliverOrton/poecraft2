#!/usr/bin/env python3
"""Finite exact examples only. No native PoE imports, rules, or timings.
Run: python checks/check_contracts.py --output checks/results.json
"""
from __future__ import annotations
import argparse
import json
from fractions import Fraction as F
from pathlib import Path
from typing import Sequence

RESULTS: list[dict[str, object]] = []

def check(name: str, condition: bool, detail: str) -> None:
    if not condition:
        raise AssertionError(name + ': ' + detail)
    RESULTS.append({'name': name, 'passed': True, 'scope': 'abstract finite example', 'detail': detail})

def solve(a: Sequence[Sequence[F]], b: Sequence[F]) -> list[F]:
    n = len(b)
    if len(a) != n or any(len(row) != n for row in a):
        raise ValueError('square system required')
    m = [[F(x) for x in row] + [F(rhs)] for row, rhs in zip(a, b)]
    for c in range(n):
        pivot = next((r for r in range(c, n) if m[r][c]), None)
        if pivot is None:
            raise ValueError('singular system; no properness/value certificate')
        m[c], m[pivot] = m[pivot], m[c]
        scale = m[c][c]
        m[c] = [x / scale for x in m[c]]
        for r in range(n):
            if r == c:
                continue
            scale = m[r][c]
            m[r] = [x - scale*y for x, y in zip(m[r], m[c])]
    return [row[-1] for row in m]

def bound_value(q: list[list[F]], h: list[list[F]], c: list[F], boundary: list[F]) -> list[F]:
    n = len(q)
    a = [[F(i == j) - q[i][j] for j in range(n)] for i in range(n)]
    rhs = [c[i] + sum((p*v for p, v in zip(h[i], boundary)), F(0)) for i in range(n)]
    return solve(a, rhs)

def run() -> dict[str, object]:
    # Two bindings use the same dependency pattern but different native-like leaves.
    q1 = [[F(1,4), F(1,4)], [F(1,8), F(1,4)]]
    q2 = [[F(1,8), F(1,2)], [F(1,4), F(1,8)]]
    h1, h2 = [[F(1,4)], [F(1,4)]], [[F(1,8)], [F(1,4)]]
    c1, c2 = [F(1),F(2)], [F(3),F(1)]
    u1, u2 = [F(7)], [F(13)]
    v1, v2 = bound_value(q1,h1,c1,u1), bound_value(q2,h2,c2,u2)
    check('common_pattern_unequal_answers', v1 != v2 and [[bool(x) for x in row] for row in q1] == [[bool(x) for x in row] for row in q2], 'Sharing adjacency does not authorize sharing values.')
    for tag,q,h,c,u,v in [('first',q1,h1,c1,u1,v1),('second',q2,h2,c2,u2,v2)]:
        check('specialization_'+tag, all(v[i] == c[i] + sum((q[i][j]*v[j] for j in range(2)),F(0)) + h[i][0]*u[0] for i in range(2)), 'Each binding satisfies its own full equations, including its boundary.')
    # Full native-like extension equals stopped region when its actual boundary is used.
    full_q = [[q1[0][0],q1[0][1],h1[0][0]],[q1[1][0],q1[1][1],h1[1][0]],[F(0),F(0),F(0)]]
    full_v = bound_value(full_q, [[],[],[]], c1+u1, [])
    check('full_graph_and_bound_region_agree',full_v[:2] == v1,'The outside state costs seven then terminates; the stopped interface uses seven.')
    omitted = bound_value(q1,h1,c1,[F(0)])
    check('dropping_boundary_is_not_equivalent', omitted != v1 and omitted[0] < v1[0], 'An unaccounted continuation cannot be replaced by zero.')
    # Same local shape, different outside economics; choices must be recomputed.
    via = lambda z: (F(1)+F(1,2)*z)/(1-F(1,4))
    check('boundary_changes_best_action', min(via(F(0)),F(10)) == via(F(0)) and min(via(F(100)),F(10)) == F(10), 'A fixed menu is not a fixed best decision.')
    # Component-local exit is not global properness.
    try:
        bound_value([[F(0),F(1)],[F(1),F(0)]],[[],[]],[F(1),F(1)],[])
        rejected=False
    except ValueError:
        rejected=True
    check('mutual_port_cycle_requires_global_check',rejected,'Each one-state fragment exits locally, but the composed cycle never terminates.')
    # Three role states share status histograms but their guarded actions may differ.
    statuses_a, statuses_b = ('held','held','missing'), ('held','missing','held')
    check('histogram_collision_is_only_diagnostic', sorted(statuses_a)==sorted(statuses_b) and {'target_c','annul'} != {'target_b','annul','protect'},'Descriptive recurrence does not prove corresponding action menus.')
    weights_a, weights_b = (F(1),F(9)),(F(9),F(1))
    tail = (F(0),F(100))
    exp_a=sum((w*v for w,v in zip(weights_a,tail)),F(0))/sum(weights_a)
    exp_b=sum((w*v for w,v in zip(weights_b,tail)),F(0))/sum(weights_b)
    check('equal_mass_ragged_members_not_equal_tail',exp_a==90 and exp_b==10,'An aggregated total must not erase observed member-to-continuation correlation.')
    # Even a numerically equal local law can depend on the binding's outside ports.
    check('generation_bound_cache', ('shape',1,'boundary-A') != ('shape',2,'boundary-A') and ('shape',1,'boundary-A') != ('shape',1,'boundary-B'),'Numerical evidence is binding and relevant-generation dependent, unlike a static shape.')
    check('independent_leaves_need_actual_reads',F(1,3)*4+F(2,3)*9 != F(1,3)*4+F(2,3)*12,'A common dot-product loop cannot skip a changed nonzero-probability value leaf.')
    # Source-like queue behavior, not a native implementation test.
    def enqueue_model(focused: bool, shared_seen: bool, successors: list[int], owner: int, queued: set[int]) -> set[int]:
        if focused or shared_seen:
            return set(queued)
        return queued | (set(successors)-{owner})
    check('ordinary_fringe_is_conditional', enqueue_model(False,False,[0,1,1,2],0,set())=={1,2} and enqueue_model(True,False,[0,1,2],0,set())==set() and enqueue_model(False,True,[0,1,2],0,{1})=={1},'Ordinary nonfocused enqueue, duplicate suppression, and focused suppression are distinct.')
    # Value-directed discovery requires mass to survive even when service is deferred.
    support={0:F(1,10),1:F(2,10),2:F(7,10)}
    selected={1}
    check('deferred_service_does_not_drop_support',sum(support.values())==1 and sum(support[k] for k in selected)==F(1,5),'Selecting one work item is not renormalizing the stochastic action.')
    # Work counts and actual reusable cost have different denominators.
    cold = 100*(1+99)
    shared = 1+100*(1+99)
    check('high_recurrence_can_lose_time', shared > cold,'One hundred equal shapes still lose if matching costs the entire one-unit structural saving.')
    cold2=5*(10+2+3); shared2=10+5*(1+2+3)
    check('finite_amortization_includes_first_build', cold2==75 and shared2==40,'Savings require enough actual repeated structural work; build and every binding are charged.')
    parent_inclusive, child = 10,7
    check('nested_timers_not_additive',parent_inclusive+child!=10 and parent_inclusive-child==3,'If the child is inside the parent, their sum is not wall time.')
    # Unretained frontier is not a reusable completed graph/checkpoint.
    check('sampled_work_is_not_full_domain_proof',{'seen-A','seen-B'} < {'seen-A','seen-B','unseen-C'},'Two matching complete samples do not certify an unseen binding.')
    # Exact recorded arithmetic is illustrative comparison, not new native timings.
    struct=F(16200+800+17200+700)
    frontier=F(1905700+2141600)
    check('g0_scoped_fraction',struct==34900 and struct/frontier<F(1,100),'Only the measured two-row bucket/exclusion slice is below one percent of its frontier time.')
    return {'kind':'exact-rational and abstract contract examples, not native tests',
            'checks':len(RESULTS),'failures':0,'results':RESULTS,
            'derived': {'binding_1_values':[str(x) for x in v1], 'binding_2_values':[str(x) for x in v2],
                        'g0_two_row_setup_fraction_of_frontier':float(struct/frontier),
                        'g0_setup_ns':int(struct),'g0_frontier_ns':int(frontier)},
            'limitations':['No PoE mechanics implementation','No native build/solver/WASM run','No performance measurements','No universal proof from finite examples']}

def main() -> None:
    p=argparse.ArgumentParser(); p.add_argument('--output',type=Path)
    args=p.parse_args(); result=run(); text=json.dumps(result,indent=2)+'\n'
    if args.output:
        args.output.parent.mkdir(parents=True,exist_ok=True); args.output.write_text(text,encoding='utf-8')
    print(json.dumps({'checks':result['checks'],'failures':result['failures'],'kind':result['kind']}))
if __name__=='__main__':
    main()
