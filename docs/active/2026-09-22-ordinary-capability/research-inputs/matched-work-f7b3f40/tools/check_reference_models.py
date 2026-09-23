#!/usr/bin/env python3
"""Exact arithmetic/specification checks. Does not import or emulate PoE mechanics.
Run from any directory: python tools/check_reference_models.py --output evidence/reference_results.json
"""
from __future__ import annotations
import argparse
import json
import unittest
from fractions import Fraction as F
from pathlib import Path
from typing import Sequence

ROOT = Path(__file__).resolve().parents[1]

def solve(a: Sequence[Sequence[F]], b: Sequence[F]) -> list[F]:
    n = len(b)
    if len(a) != n or any(len(row) != n for row in a):
        raise ValueError('square matrix required')
    m = [[F(x) for x in row] + [F(rhs)] for row, rhs in zip(a, b)]
    for col in range(n):
        pivot = next((r for r in range(col, n) if m[r][col]), None)
        if pivot is None:
            raise ValueError('singular system; no properness/value conclusion')
        m[col], m[pivot] = m[pivot], m[col]
        scale = m[col][col]
        m[col] = [x / scale for x in m[col]]
        for r in range(n):
            if r != col:
                factor = m[r][col]
                m[r] = [x - factor*y for x, y in zip(m[r], m[col])]
    return [row[-1] for row in m]

def value(q: Sequence[Sequence[F]], c: Sequence[F]) -> list[F]:
    n = len(c)
    return solve([[F(i == j) - F(q[i][j]) for j in range(n)] for i in range(n)], c)

def grouped_trace(quanta: list[int], target: int = 23) -> tuple[list[int], int, int]:
    """Toy grouping-only transitions with genuine semantic fences every six units."""
    if not quanta or any(q < 1 for q in quanta):
        raise ValueError('positive requested ceilings required')
    trace: list[int] = []
    requested = calls = 0
    while len(trace) < target:
        q = quanta[calls % len(quanta)]
        calls += 1
        requested += q
        boundary = min(target, ((len(trace)//6)+1)*6)
        actual = min(q, boundary-len(trace))
        trace.extend(range(len(trace), len(trace)+actual))
    return trace, requested, calls

def timed_result_is_timeout(result: dict) -> bool:
    return result.get('timed_out') is True and result.get('survivor') is False

class Checks(unittest.TestCase):
    def test_01_sampled_middle_interval(self):
        d = json.loads((ROOT/'evidence/trajectory_checkpoints.json').read_text())
        rows = {x['rows']: x for x in d['checkpoints']}
        dn = F(rows[42681]['native_s']) - F(rows[24844]['native_s'])
        dw = F(rows[42681]['worker_s']) - F(rows[24844]['worker_s'])
        self.assertEqual(dn, F('24.3902075'))
        self.assertEqual(dw, F('80.4255987'))
        self.assertEqual(dw-dn, F('56.0353912'))
    def test_02_next_interval_is_not_constant_factor(self):
        dn = F('86.2691353')-F('48.8887032')
        dw = F('163.0686986')-F('116.9577677')
        self.assertEqual(dn, F('37.3804321'))
        self.assertEqual(dw, F('46.1109309'))
        self.assertLess(dw/dn, F('1.24'))
        self.assertGreater(F('80.4255987')/F('24.3902075'), F('3.29'))
    def test_03_cost_targets(self):
        baseline = F('5218.040949685988')
        target = F('3746.1319409485764')
        partial = F(4, 5)*baseline
        self.assertEqual(partial, F('4174.4327597487904'))
        self.assertGreater((baseline-target)/baseline, F('0.282'))
        self.assertLess((baseline-target)/baseline, F('0.283'))
    def test_04_complete_candidate_economic_negative(self):
        candidate = F('6147.834010344707')
        self.assertGreater(candidate, F('5218.040949685988'))
        self.assertGreater(candidate, F('3746.1319409485764'))
    def test_05_full_paid_renewal_bill(self):
        p = F(1, 4)
        attempt = F(10) + (1-p)*F(3+2+1)
        self.assertEqual(attempt/p, 58)
        self.assertEqual(4*10+3*3+3*2+3*1, 58)
    def test_06_unpaid_replacement_changes_value(self):
        self.assertEqual(F(10)/F(1, 4), 40)
        self.assertNotEqual(F(10)/F(1, 4), 58)
    def test_07_grouping_only_trace_invariance(self):
        for quanta in ([1], [8], [3, 7, 2], [1024]):
            self.assertEqual(grouped_trace(list(quanta))[0], list(range(23)))
    def test_08_requested_is_not_actual_work(self):
        trace, requested, calls = grouped_trace([8])
        self.assertEqual(len(trace), 23)
        self.assertEqual(calls, 4)
        self.assertEqual(requested, 32)
    def test_09_boundary_decisions_break_grouping_premise(self):
        def events(q):
            pos, out = 0, []
            while pos < 8:
                for _ in range(min(q, 8-pos)):
                    out.append(('row', pos)); pos += 1
                out.append(('select', pos))
            return out
        self.assertEqual(sum(k=='row' for k,_ in events(1)), 8)
        self.assertEqual(sum(k=='row' for k,_ in events(8)), 8)
        self.assertNotEqual(events(1), events(8))
    def test_10_equal_counts_do_not_mean_same_work(self):
        trace_a = [('exalt', 1), ('annul', 2)]
        trace_b = [('chaos', 1), ('scour', 2)]
        self.assertEqual(len(trace_a), len(trace_b))
        self.assertNotEqual(trace_a, trace_b)
    def test_11_fixed_controller_tail_difference(self):
        q = [[F(1,2), F(1,4), F(1,4)], [0,0,0], [0,0,0]]
        a = value(q, [2,10,6]); b = value(q, [2,4,8])
        self.assertEqual(a[0], 12); self.assertEqual(b[0], 10)
        self.assertEqual(a[0]-b[0], F(1,2)*(10-4)+F(1,2)*(6-8))
    def test_12_first_hit_is_subprobability(self):
        q = [[F(1,4), F(1,4)], [0,0]]  # remaining mass reaches goal before D
        a = value(q, [1,10]); b = value(q, [1,4])
        mu = F(1,4)/(1-F(1,4))
        self.assertEqual(mu, F(1,3))
        self.assertEqual(a[0]-b[0], mu*(10-4))
    def test_13_common_prefix_with_no_disagreement(self):
        q = [[F(3,4)]]
        a = value(q, [1]); b = value(q, [1])
        self.assertEqual(a[0]-b[0], 0)
    def test_14_closed_trap_is_not_proper_value(self):
        with self.assertRaises(ValueError):
            value([[F(1)]], [F(1)])
    def test_15_upper_estimate_is_not_rejection_lower(self):
        candidate_true, candidate_upper, incumbent = F(6), F(20), F(10)
        self.assertLessEqual(candidate_true, candidate_upper)
        self.assertGreater(candidate_upper, incumbent)
        self.assertLess(candidate_true, incumbent)
    def test_16_forced_status_does_not_prove_timeout(self):
        r = {'status':'watchdog_expired','timed_out':False,'survivor':False}
        self.assertFalse(timed_result_is_timeout(r))
        r.update(timed_out=True)
        self.assertTrue(timed_result_is_timeout(r))
        r.update(survivor=True)
        self.assertFalse(timed_result_is_timeout(r))
    def test_17_timeout_clock_decomposition(self):
        elapsed = F('0.4')+F('0.15')+F('0.7')+F('0.65')
        self.assertEqual(elapsed, F('1.9'))
        self.assertGreater(elapsed, F('1.5'))
        # This example does not assert that the real test's child obeyed its deadline.
    def test_18_unknown_observation_not_zero(self):
        snapshot = {'joint_attempts': None, 'omitted': 1530}
        self.assertIsNone(snapshot['joint_attempts'])
        self.assertNotEqual(snapshot['joint_attempts'], 0)
    def test_19_recurrence_requires_new_occupancy(self):
        old_q, new_q = F(9,10), F(8,10)
        old_v, new_v = 1/(1-old_q), 1/(1-new_q)
        z = 1/(1-old_q)
        dp = new_q-old_q
        corrected = z*(dp*old_v)/(1-dp*z)
        self.assertEqual(corrected, new_v-old_v)
        self.assertEqual(corrected, -5)
        self.assertNotEqual(z*dp*old_v, corrected)
    def test_20_witness_pair_is_not_historical_scalar(self):
        owned = {'graph_B': F(10)}
        historical_min = F(7)
        self.assertNotIn(historical_min, owned.values())
        self.assertEqual(min(owned.values()), 10)

if __name__ == '__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, default=ROOT/'evidence/reference_results.json')
    args=parser.parse_args()
    names=unittest.defaultTestLoader.getTestCaseNames(Checks)
    result=unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(Checks))
    dn=F('48.8887032')-F('24.4984957'); dw=F('116.9577677')-F('36.5321690')
    payload={'scope':'offline exact-rational arithmetic and abstract specification; no native code, PoE mechanics, actual watchdog or solver runtime executed',
             'tests_run':result.testsRun,'failures':len(result.failures),'errors':len(result.errors),'successful':result.wasSuccessful(),
             'test_names':names,'derived_recorded_interval':{'native_s':str(dn),'worker_s':str(dw),'extra_s':str(dw-dn),'ratio':float(dw/dn)},
             'native_benchmark_run':False,'hosted_test_reproduced':False}
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(payload,indent=2,allow_nan=False)+'\n',encoding='utf-8')
    raise SystemExit(0 if result.wasSuccessful() else 1)
