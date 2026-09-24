#!/usr/bin/env python3
"""Exact small models for the plan, NOT native poecraft2 qualification.

Run from any directory:
    python check_contracts.py --output results.json
Only the Python standard library is required.
"""
from __future__ import annotations
import argparse
from dataclasses import dataclass, replace
from fractions import Fraction as F
import json
from pathlib import Path
import sys
import unittest


def solve(a: list[list[F]], b: list[F]) -> list[F]:
    n = len(b)
    if n == 0 or len(a) != n or any(len(row) != n for row in a):
        raise ValueError('nonempty square system required')
    aug = [[F(x) for x in row] + [F(y)] for row, y in zip(a, b)]
    for col in range(n):
        pivot = next((r for r in range(col, n) if aug[r][col]), None)
        if pivot is None:
            raise ValueError('singular system: properness not established')
        aug[col], aug[pivot] = aug[pivot], aug[col]
        scale = aug[col][col]
        aug[col] = [x / scale for x in aug[col]]
        for r in range(n):
            if r != col:
                scale = aug[r][col]
                aug[r] = [x-scale*y for x, y in zip(aug[r], aug[col])]
    return [row[-1] for row in aug]


def policy_values(q: list[list[F]], c: list[F]) -> list[F]:
    n = len(c)
    if len(q) != n or any(len(row) != n for row in q):
        raise ValueError('shape mismatch')
    if any(x < 0 for row in q for x in row) or any(sum(row) > 1 for row in q):
        raise ValueError('not a substochastic transition matrix')
    if any(x < 0 for x in c):
        raise ValueError('negative costs not used in these fixtures')
    return solve([[F(i == j)-q[i][j] for j in range(n)] for i in range(n)], c)


def local_value(g: F, exits: list[tuple[F, F | None]], goal_mass: F) -> F:
    if g < 0 or goal_mass < 0 or any(p < 0 for p, _ in exits):
        raise ValueError('invalid nonnegative law')
    if goal_mass + sum(p for p, _ in exits) != 1:
        raise ValueError('incomplete or duplicated probability mass')
    if any(p and tail is None for p, tail in exits):
        raise ValueError('unknown positive-mass continuation')
    return g + sum((p * tail for p, tail in exits if p and tail is not None), F(0))


@dataclass(frozen=True)
class Certificate:
    graph: str
    context: tuple[str, ...]
    cost: F
    complete: bool = True
    proper: bool = True
    success: F = F(1)
    failed_mass: F = F(0)


def adopt(base: Certificate, candidate: Certificate) -> Certificate:
    valid = (candidate.context == base.context and bool(candidate.graph)
             and candidate.complete and candidate.proper and candidate.success == 1
             and candidate.failed_mass == 0 and candidate.cost >= 0)
    return candidate if valid and candidate.cost < base.cost else base


class ContractTests(unittest.TestCase):
    def setUp(self) -> None:
        self.base = Certificate('old-graph', ('root','goal','scope','prices','data'), F(10))

    def test_01_one_use_differs_from_recurring_replacement(self):
        once = local_value(F(2), [(F(1,2), F(10))], F(1,2))
        repeated = policy_values([[F(1,2)]], [F(2)])[0]
        self.assertEqual((once, repeated), (F(7), F(4)))

    def test_02_old_occupancy_is_not_exact_replacement_saving(self):
        old = policy_values([[F(9,10)]], [F(1)])[0]
        new = policy_values([[F(1,2)]], [F(2)])[0]
        advantage = F(2) + F(1,2)*old - old
        self.assertEqual(new-old, F(2)*advantage)
        self.assertNotEqual(new-old, F(10)*advantage)

    def test_03_matrix_performance_difference(self):
        qp = [[F(1,4),F(1,4)],[F(1,8),F(1,4)]]
        qm = [[F(1,8),F(1,8)],[F(1,8),F(1,8)]]
        cp, cm = [F(3),F(2)], [F(2),F(1)]
        vp, vm = policy_values(qp,cp), policy_values(qm,cm)
        residual = [cm[i]+sum(qm[i][j]*vp[j] for j in range(2))-vp[i] for i in range(2)]
        difference = solve([[F(i==j)-qm[i][j] for j in range(2)] for i in range(2)], residual)
        self.assertEqual(difference, [vm[i]-vp[i] for i in range(2)])

    def test_04_paid_prefix_is_not_free_entry(self):
        self.assertEqual(policy_values([[F(0),F(1)],[F(0),F(0)]],[F(100),F(2)])[0],102)

    def test_05_heterogeneous_binding_can_change_best_action(self):
        def choose(p: F):
            return min((('retry',1/p),('direct',F(20))),key=lambda x:x[1])
        self.assertEqual(choose(F(1,5)), ('retry',F(5)))
        self.assertEqual(choose(F(1,100)), ('direct',F(20)))

    def test_06_same_decision_can_still_have_different_native_value(self):
        self.assertEqual((min(F(1)/F(1,5),F(20)),min(F(1)/F(1,4),F(20))), (F(5),F(4)))

    def test_07_missing_positive_tail_refuses(self):
        with self.assertRaises(ValueError): local_value(F(1),[(F(1,10),None)],F(9,10))

    def test_08_zero_support_does_not_require_imaginary_tail(self):
        self.assertEqual(local_value(F(1),[(F(0),None)],F(1)),1)

    def test_09_missing_mass_cannot_be_normalized_away(self):
        with self.assertRaises(ValueError): local_value(F(1),[],F(9,10))

    def test_10_joint_outcome_cannot_be_double_counted(self):
        with self.assertRaises(ValueError): local_value(F(1),[(F(7,10),F(2))],F(7,10))

    def test_11_local_terminating_regions_can_cycle_globally(self):
        self.assertEqual(local_value(F(0),[(F(1),F(1))],F(0)),1)
        with self.assertRaises(ValueError): policy_values([[F(0),F(1)],[F(1),F(0)]],[F(0),F(0)])

    def test_12_zero_escape_changes_properness(self):
        self.assertEqual(policy_values([[F(9,10)]],[F(1)])[0],10)
        with self.assertRaises(ValueError): policy_values([[F(1)]],[F(1)])

    def test_13_exposure_is_not_additive_spend(self):
        q = [[F(int(j==i+1)) for j in range(4)] for i in range(4)]
        v = policy_values(q,[F(1)]*4)
        self.assertEqual((v[0],sum(v)),(F(4),F(10)))

    def test_14_upper_estimate_cannot_prove_candidate_non_improving(self):
        true_new, upper_new, incumbent = F(6),F(20),F(10)
        self.assertLess(true_new,incumbent)
        self.assertGreater(upper_new,incumbent)
        self.assertLessEqual(true_new,upper_new)

    def test_15_complete_cheaper_artifact_is_adopted_as_a_bundle(self):
        c = replace(self.base,graph='new-graph',cost=F(4))
        self.assertIs(adopt(self.base,c),c)
        self.assertEqual((adopt(self.base,c).graph,adopt(self.base,c).cost),('new-graph',F(4)))

    def test_16_unverified_candidate_cannot_replace_fallback(self):
        self.assertIs(adopt(self.base,replace(self.base,cost=F(1),complete=False)),self.base)

    def test_17_success_flags_do_not_hide_failed_mass(self):
        self.assertIs(adopt(self.base,replace(self.base,cost=F(1),failed_mass=F(1,1000))),self.base)

    def test_18_changed_context_refuses(self):
        self.assertIs(adopt(self.base,replace(self.base,cost=F(1),context=('another-root',))),self.base)

    def test_19_less_expensive_than_donor_is_not_better_than_current_winner(self):
        current=replace(self.base,graph='other-result',cost=F(5))
        late=replace(self.base,graph='late-from-old-donor',cost=F(7))
        self.assertIs(adopt(current,late),current)

    def test_20_expensive_candidate_does_not_degrade_base(self):
        self.assertIs(adopt(self.base,replace(self.base,graph='expensive',cost=F(20))),self.base)

    def test_21_aggregate_memory_is_not_per_child_limit(self):
        parent,child,checker,limit=100,60,80,200
        self.assertLess(child,limit); self.assertLess(checker,limit)
        self.assertGreater(parent+child+checker,limit)

    def test_22_releasing_scratch_does_not_refund_work(self):
        debit,live=7,50
        live=0
        self.assertEqual((debit,live),(7,0))

    def test_23_unchanged_wakeup_does_not_rearm_one_wave(self):
        attempted=set()
        base_id=('run','base','query-generation')
        decisions=[]
        for unrelated_rows in (1,2,100):
            _=unrelated_rows
            decisions.append(base_id not in attempted)
            attempted.add(base_id)
        self.assertEqual(decisions,[True,False,False])

    def test_24_early_finish_does_not_measure_later_quality(self):
        events=[(40,F(10)),(150,F(6))]
        def upper(t): return min(v for time,v in events if time<=t)
        self.assertEqual((upper(41),upper(240)),(F(10),F(6)))

    def test_25_complete_law_price_depends_on_actual_resources(self):
        law=[(F(1,4),F(0)),(F(3,4),F(10))]
        self.assertEqual(local_value(F(2),law,F(0)),F(19,2))
        self.assertEqual(local_value(F(5),law,F(0)),F(25,2))

    def test_26_no_graph_means_no_executable_witness(self):
        self.assertIs(adopt(self.base,replace(self.base,graph='',cost=F(1))),self.base)


def main() -> int:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path(__file__).with_name('results.json'))
    args=parser.parse_args()
    suite=unittest.defaultTestLoader.loadTestsFromTestCase(ContractTests)
    names=[t.id() for t in suite]
    result=unittest.TextTestRunner(verbosity=2,stream=sys.stdout).run(suite)
    payload={
      'schema':'abstract_contract_check_results_v1',
      'scope':'Exact-rational finite-policy equations and simplified lifecycle examples; NOT native mechanics, solver performance, formal verification or a runtime feature test.',
      'tests_run':result.testsRun,'failures':len(result.failures),'errors':len(result.errors),
      'passed':result.wasSuccessful(),'test_names':names,
      'illustrative_values':{'base_retry':'10','one_use_then_base':'7','recurring_replacement':'4','actual_difference':'-6','incorrect_old_occupancy_difference':'-30'},
      'recorded_arithmetic_not_performance':{'selected_scan_seconds':'3.174','all_partition_seconds':'6.661','sum_seconds':str(F('3.174')+F('6.661'))},
    }
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(payload,indent=2)+'\n',encoding='utf-8')
    return 0 if result.wasSuccessful() else 1

if __name__=='__main__':
    raise SystemExit(main())
