#!/usr/bin/env python3
"""Exact toy models and arithmetic for the capability plan, not a native engine test.

No imports of game data/poecraft2, no network, no random trials. Requires Python3.10+.
Run: python checks/check_concepts.py --output checks/results.json
"""
from __future__ import annotations
import argparse
from dataclasses import dataclass
from decimal import Decimal, localcontext
from fractions import Fraction as F
import json
from pathlib import Path
from typing import Callable


def linear_solve(a: list[list[F]], b: list[F]) -> list[F]:
    n = len(b)
    if len(a) != n or any(len(row) != n for row in a):
        raise ValueError('non-square system')
    m = [[F(x) for x in row] + [F(bi)] for row, bi in zip(a, b)]
    for col in range(n):
        pivot = next((r for r in range(col, n) if m[r][col]), None)
        if pivot is None:
            raise ValueError('singular system; no transient-chain certificate')
        m[col], m[pivot] = m[pivot], m[col]
        scale = m[col][col]
        m[col] = [x/scale for x in m[col]]
        for r in range(n):
            if r == col:
                continue
            scale = m[r][col]
            if scale:
                m[r] = [x-scale*y for x, y in zip(m[r], m[col])]
    return [row[-1] for row in m]


def root_chain(q: list[list[F]], c: list[F], root: int = 0) -> tuple[F, dict[int, F], dict[int, F]]:
    """Missing row mass is goal absorption in these TOY chains only.

    A modeled failure/non-goal absorbing state must be present explicitly with
    a self-loop, and will fail the rooted transience calculation. Unreachable
    states are not granted entry values.
    """
    n = len(c)
    if not 0 <= root < n or len(q) != n or any(len(row) != n for row in q):
        raise ValueError('invalid dimensions/root')
    if any(x < 0 for row in q for x in row) or any(sum(row) > 1 for row in q):
        raise ValueError('invalid substochastic coefficients')
    seen = {root}
    pending = [root]
    for i in pending:
        for j, p in enumerate(q[i]):
            if p > 0 and j not in seen:
                seen.add(j); pending.append(j)
    states = sorted(seen)
    a = [[F(i == j)-q[s][t] for j, t in enumerate(states)] for i, s in enumerate(states)]
    values = linear_solve(a, [c[s] for s in states])
    visits = linear_solve([list(row) for row in zip(*a)], [F(s == root) for s in states])
    if any(x < 0 for x in visits):
        raise ValueError('not a nonnegative occupancy witness')
    return values[states.index(root)], dict(zip(states, values)), dict(zip(states, visits))


def must_refuse(fn: Callable[[], object]) -> None:
    try:
        fn()
    except ValueError:
        return
    raise AssertionError('expected refusal')


def test_known_chain_and_cost_accounting() -> None:
    q = [[F(1,2), F(1,4)], [F(0), F(1,2)]]
    c = [F(1), F(2)]
    v, all_v, d = root_chain(q, c)
    assert all_v == {0:F(4), 1:F(4)}
    assert d == {0:F(2), 1:F(1)}
    assert v == sum(d[i]*c[i] for i in d) == 4


def test_upper_filter_false_negative() -> None:
    incumbent, candidate, upper = F(10), F(6), F(20)
    assert upper >= candidate
    assert upper >= incumbent  # the heuristic rejects
    assert candidate < incumbent  # despite a genuinely useful candidate


def test_valid_lower_filter_case() -> None:
    incumbent, candidate, lower = F(10), F(12), F(11)
    assert lower <= candidate and lower >= incumbent
    assert candidate >= incumbent


def test_exact_native_candidate_screen() -> None:
    values = [F(6), F(10), F(12)]
    assert [v for v in values if v < 10] == [F(6)]


def test_complementary_decisions() -> None:
    incumbent = root_chain([[F(0),F(0)],[F(0),F(0)]], [F(10),F(100)])[0]
    bridge = root_chain([[F(0),F(1)],[F(0),F(0)]], [F(1),F(100)])[0]
    tail = root_chain([[F(0),F(0)],[F(0),F(0)]], [F(10),F(1)])[0]
    both = root_chain([[F(0),F(1)],[F(0),F(0)]], [F(1),F(1)])[0]
    assert (incumbent, bridge, tail, both) == (10,101,10,2)


def test_old_occupancy_misses_unreachable_tail() -> None:
    _, values, d = root_chain([[F(0),F(0)],[F(0),F(0)]], [F(10),F(100)])
    assert 1 not in values and 1 not in d
    assert d.get(1,F(0)) * F(99) == 0
    assert root_chain([[F(0),F(1)],[F(0),F(0)]], [F(1),F(1)])[0] == 2


def test_row_change_needs_new_occupancy() -> None:
    old, _, old_d = root_chain([[F(9,10)]], [F(1)])
    new, _, new_d = root_chain([[F(4,5)]], [F(1)])
    advantage = F(1)+F(4,5)*old-old
    assert old == 10 and new == 5 and advantage == -1
    assert old_d[0]*advantage == -10
    assert new_d[0]*advantage == new-old == -5


def test_recurrence_correction() -> None:
    old_q, new_q = F(9,10), F(4,5)
    old_z = 1/(1-old_q)
    old_v = old_z
    dp = new_q-old_q
    numerator = old_z * dp * old_v
    denominator = 1-dp*old_z
    assert numerator/denominator == F(-5)


def test_zero_cost_loop_is_not_goal() -> None:
    must_refuse(lambda: root_chain([[F(1)]], [F(0)]))


def test_tiny_positive_trap_must_remain() -> None:
    eps = F(1,10**12)
    # Most mass goes directly to goal; tiny remainder goes to a real trap.
    must_refuse(lambda: root_chain([[F(0),eps],[F(0),F(1)]], [F(1),F(0)]))
    assert root_chain([[F(0)]], [F(1)])[0] == 1  # omitting trap changes model


def test_proper_options_can_form_improper_composition() -> None:
    # Each individual one-step option exits, but the composition returns forever.
    must_refuse(lambda: root_chain([[F(0),F(1)],[F(1),F(0)]], [F(1),F(1)]))


def test_root_certificate_does_not_certify_other_entry() -> None:
    q = [[F(0),F(0)],[F(0),F(1)]]
    assert root_chain(q,[F(1),F(0)],root=0)[0] == 1
    must_refuse(lambda: root_chain(q,[F(1),F(0)],root=1))


def test_real_tail_not_root_scalar() -> None:
    q = [[F(0),F(1,2)],[F(0),F(0)]]
    real = root_chain(q,[F(1),F(100)])[0]
    wrong = F(1)+F(1,2)*F(10)
    assert real == 51 and wrong == 6


def test_reward_and_primitive_count_distinct() -> None:
    q = [[F(1,2)]]
    assert root_chain(q,[F(3)])[0] == 6
    assert root_chain(q,[F(4)])[0] == 8


def test_finite_prefix_and_final_quality_differ() -> None:
    events = [(F(10),F(100)),(F(40),F(60)),(F(200),F(50))]
    def available(t: F) -> F:
        return min(c for time,c in events if time <= t)
    assert available(F(10)) == 100
    assert available(F(240)) == 50


def test_more_unselected_rows_not_a_new_policy() -> None:
    selected = ('root-op-A','tail-op-B','physical-domain-v1')
    snapshot_before = (selected,12)
    snapshot_after = (selected,999)
    assert snapshot_before[0] == snapshot_after[0]
    assert snapshot_before != snapshot_after  # row count is a different fact


@dataclass(frozen=True)
class Opportunity:
    run: str
    policy: str
    entry: str
    support: str


def test_bounded_semantic_opportunity_not_timer() -> None:
    seen: set[Opportunity] = set()
    first = Opportunity('run1','policyA','entryX','support1')
    seen.add(first)
    assert Opportunity('run1','policyA','entryX','support1') in seen
    assert Opportunity('run1','policyB','entryX','support2') not in seen
    slots_used = 1
    limit = 1
    assert slots_used >= limit  # distinct does not override the separate work cap


def test_pending_capped_and_dominated_are_different() -> None:
    statuses = {'pending':False,'capped':False,'complete_expensive':True}
    assert not statuses['pending'] and not statuses['capped']
    assert statuses['complete_expensive']


def test_verified_artifact_selection_keeps_failed_proposals_out() -> None:
    # Abstract acceptance records, not the actual native type.
    artifacts = [('graph-old',F(10),True),('graph-failed',F(1),False),('graph-new',F(6),True)]
    selected = min((a for a in artifacts if a[2]),key=lambda a:a[1])
    assert selected[:2] == ('graph-new',F(6))


def test_renaming_changes_no_root_cost() -> None:
    q = [[F(0),F(1)],[F(0),F(1,2)]]
    c = [F(2),F(3)]
    a = root_chain(q,c,0)[0]
    # Swap physical labels and root; preserve transition/price correspondence.
    b = root_chain([[F(1,2),F(0)],[F(1),F(0)]],[F(3),F(2)],1)[0]
    assert a == b == 8


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--output',type=Path,default=Path(__file__).with_name('results.json'))
    args = ap.parse_args()
    tests = [(name,fn) for name,fn in globals().items() if name.startswith('test_') and callable(fn)]
    results=[]
    for name,fn in tests:
        try:
            fn()
            results.append({'test':name,'status':'pass'})
        except Exception as exc:
            results.append({'test':name,'status':'fail','error':f'{type(exc).__name__}: {exc}'})
    bpath = Path(__file__).resolve().parents[1]/'evidence/baseline.json'
    baseline=json.loads(bpath.read_text(encoding='utf-8'))
    with localcontext() as ctx:
        ctx.prec=40
        old=Decimal(str(baseline['policies']['conquest_four_browser_reference']['cost']))
        new=Decimal(str(baseline['policies']['conquest_four_native_reference']['cost']))
        old_n=Decimal(str(baseline['policies']['conquest_four_browser_reference']['expected_primitive_actions']))
        new_n=Decimal(str(baseline['policies']['conquest_four_native_reference']['expected_primitive_actions']))
        metrics={'historical_cost_difference':str(old-new),
                 'historical_relative_cost_reduction':str((old-new)/old),
                 'historical_relative_action_reduction':str((old_n-new_n)/old_n),
                 'twenty_percent_threshold_using_historical_not_current_baseline':str(old*Decimal('.8')),
                 'note':'Arithmetic on recorded rounded numbers. Current-main unattended baseline and new native performance are unmeasured.'}
    report={'schema':'poecraft2_abstract_capability_checks_v1','native_validation':False,
            'claims':'Finite exact rational examples and abstract lifecycle distinctions only; no PoE mechanics, scheduler or platform qualification.',
            'tests_run':len(results),'passed':sum(r['status']=='pass' for r in results),
            'failed':sum(r['status']=='fail' for r in results),'tests':results,'derived_recorded_metrics':metrics}
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({'tests':len(results),'passed':report['passed'],'failed':report['failed'],'output':str(args.output)}))
    return 0 if report['failed']==0 else 1

if __name__ == '__main__':
    raise SystemExit(main())
