#!/usr/bin/env python3
"""Exact finite examples and proposed reporting contracts, not native PoE tests.

Run from anywhere:
    python checks/abstract_checks.py --output checks/results.json
No external packages, native engine imports, network requests or sampling.
"""
from __future__ import annotations

import argparse
import json
import unittest
from dataclasses import dataclass
from fractions import Fraction as F
from pathlib import Path


def debt(required: int, satisfied: int, occupied: int,
         rarity_mismatch: bool = False, terminal: bool = False) -> int:
    if min(required, satisfied, occupied) < 0 or satisfied > occupied:
        raise ValueError('invalid distinct-goal toy state')
    return 0 if terminal else max(1, max(required-satisfied, 0) +
                                  occupied-satisfied + int(rarity_mismatch))


def gate(high_impact: bool, incremental: bool, output_exists: bool) -> bool:
    return high_impact and incremental and not output_exists


def stratum(goals: int, required: int, extras: int,
            capacity_obstructed: bool, goal_blocked: bool) -> int:
    """First-match specification transcribed from inspected J1 code."""
    if goals >= required and extras:
        return 0
    if capacity_obstructed or goal_blocked:
        return 1
    if goals >= 2 and extras:
        return 2
    return 3 if extras == 0 else 4


@dataclass(frozen=True)
class ToyRow:
    row_id: int
    cost: F
    old_progress: F
    union_progress: F
    goal_probability: F = F(0)
    pending: int = 0
    restart: bool = False
    complete: bool = True

    def key(self, counterfactual: bool):
        progress = self.union_progress if counterfactual else self.old_progress
        priority = 0 if progress > 0 else 1 if self.restart else 2
        return (priority, self.pending,
                self.cost/progress if progress > 0 else self.cost,
                -self.goal_probability, -progress, self.row_id)


def winner(rows: list[ToyRow], *, counterfactual: bool, active: bool = True):
    eligible = [r for r in rows if r.complete and r.cost >= 0]
    return min(eligible, key=lambda r: r.key(counterfactual and active)).row_id if eligible else None


def measured_status(enabled: bool, calls: int, active: int, compared: int,
                    reversals: int, complete: bool = True) -> str:
    if not enabled:
        return 'not_instrumented'
    if not complete:
        return 'censored'
    if calls == 0:
        return 'measured_no_selector_calls'
    if active == 0:
        return 'measured_gate_inactive'
    if compared == 0:
        return 'measured_no_eligible_rows'
    return ('measured_no_selection_reversal' if reversals == 0
            else 'measured_reversal_tail_unknown')


def two_state_value(a: F, b: F, p: F, q: F, u: F, v: F, t: F):
    if min(a, b, p, q, u, v, t) < 0 or p+q > 1 or u+v+t != 1:
        raise ValueError('invalid complete toy law')
    delta = p*(1-t) + q*u
    if q <= 0 or 1-t <= 0 or delta <= 0:
        raise ValueError('not the declared reachable proper two-state domain')
    vr = (a*(1-t) + q*b)/delta
    vd = (b+v*vr)/(1-t)
    return vr, vd


def solve_fraction_system(matrix: list[list[F]], rhs: list[F]) -> list[F]:
    """Independent exact elimination used to check the closed form."""
    n = len(rhs)
    a = [list(row) + [value] for row, value in zip(matrix, rhs)]
    for col in range(n):
        pivot = next((r for r in range(col, n) if a[r][col] != 0), None)
        if pivot is None:
            raise ValueError('singular')
        a[col], a[pivot] = a[pivot], a[col]
        scale = a[col][col]
        a[col] = [x/scale for x in a[col]]
        for r in range(n):
            if r != col:
                factor = a[r][col]
                a[r] = [x-factor*y for x, y in zip(a[r], a[col])]
    return [a[r][-1] for r in range(n)]


class SpecificationChecks(unittest.TestCase):
    def test_debt_tie_four_goals(self):
        self.assertEqual(debt(4, 0, 0), debt(4, 3, 6))
        self.assertEqual(debt(4, 3, 6), 4)

    def test_terminal_is_not_dirty_coverage(self):
        self.assertEqual(debt(4, 4, 4, terminal=True), 0)
        self.assertEqual(debt(4, 4, 6), 2)

    def test_output_object_closes_gate_without_verification(self):
        self.assertTrue(gate(True, True, False))
        self.assertFalse(gate(True, True, True))
        self.assertFalse(gate(False, True, False))

    def test_exclusive_obstruction_hides_high_progress_label(self):
        self.assertEqual(stratum(3, 5, 3, True, False), 1)
        self.assertEqual(stratum(3, 5, 3, False, False), 2)
        self.assertEqual(stratum(5, 5, 1, True, False), 0)

    def test_union_is_not_probability_sum(self):
        # old-only, acquired-only, both, neither
        atoms = [(F(1, 10), True, False), (F(2, 10), False, True),
                 (F(3, 10), True, True), (F(4, 10), False, False)]
        old = sum(p for p, d, g in atoms if d)
        gained = sum(p for p, d, g in atoms if g)
        union = sum(p for p, d, g in atoms if d or g)
        self.assertEqual(union, F(3, 5))
        self.assertNotEqual(union, old+gained)
        self.assertGreaterEqual(union, old)

    def test_observed_choice_event_credited_once(self):
        mass = F(2, 5)
        successors = [True, True, False]
        self.assertEqual(mass*int(any(successors)), F(2, 5))
        self.assertNotEqual(mass*sum(successors), mass)

    def test_union_can_reverse_full_key(self):
        rows = [ToyRow(1, F(1), F(1, 10), F(1, 10)),
                ToyRow(2, F(1), F(0), F(1, 5))]
        self.assertEqual(winner(rows, counterfactual=False), 1)
        self.assertEqual(winner(rows, counterfactual=True), 2)

    def test_pending_route_precedence_can_block_reversal(self):
        rows = [ToyRow(1, F(1), F(1, 10), F(1, 10), pending=0),
                ToyRow(2, F(1), F(1, 20), F(1, 2), pending=1)]
        self.assertEqual(winner(rows, counterfactual=False), 1)
        self.assertEqual(winner(rows, counterfactual=True), 1)

    def test_gate_off_preserves_original_selection(self):
        rows = [ToyRow(1, F(1), F(1, 10), F(1, 10)),
                ToyRow(2, F(1), F(0), F(1, 5))]
        self.assertEqual(winner(rows, counterfactual=True, active=False), 1)

    def test_incomplete_row_not_eligible(self):
        rows = [ToyRow(1, F(1), F(1, 10), F(1, 10)),
                ToyRow(2, F(0), F(1), F(1), complete=False)]
        self.assertEqual(winner(rows, counterfactual=True), 1)
        self.assertIsNone(winner([], counterfactual=True))

    def test_evidence_status_distinguishes_missing_and_zero(self):
        cases = [
            ((False, 0, 0, 0, 0), 'not_instrumented'),
            ((True, 0, 0, 0, 0), 'measured_no_selector_calls'),
            ((True, 10, 0, 0, 0), 'measured_gate_inactive'),
            ((True, 10, 3, 0, 0), 'measured_no_eligible_rows'),
            ((True, 10, 3, 8, 0), 'measured_no_selection_reversal'),
            ((True, 10, 3, 8, 1), 'measured_reversal_tail_unknown'),
        ]
        for args, expected in cases:
            self.assertEqual(measured_status(*args), expected)
        self.assertEqual(measured_status(True, 10, 3, 8, 0, False), 'censored')

    def test_complete_paid_dirty_route(self):
        vr, vd = two_state_value(F(2), F(1), F(0), F(1, 5), F(1, 2), F(1, 4), F(1, 4))
        self.assertEqual((vr, vd), (F(17), F(7)))
        self.assertLess(vr, 100)

    def test_same_progress_can_be_expensive(self):
        vr, _ = two_state_value(F(2), F(200), F(0), F(1, 5), F(1, 2), F(1, 4), F(1, 4))
        self.assertGreater(vr, 100)

    def test_closed_form_matches_independent_elimination(self):
        # 24 declared proper parameter bindings, not native fixtures.
        for p in (F(0), F(1, 10)):
            for q in (F(1, 5), F(2, 5)):
                for t in (F(0), F(1, 4)):
                    for b in (F(1), F(3), F(11)):
                        u = F(1, 2)
                        v = 1-u-t
                        formula = two_state_value(F(2), b, p, q, u, v, t)
                        numeric = solve_fraction_system([[p+q, -q], [-v, 1-t]], [F(2), b])
                        self.assertEqual(list(formula), numeric)

    def test_local_fragment_exit_does_not_prove_global_properness(self):
        # r -> d and d -> r both exit their own local domains, but never goal.
        with self.assertRaises(ValueError):
            two_state_value(F(1), F(1), F(0), F(1), F(0), F(1), F(0))
        with self.assertRaises(ValueError):
            solve_fraction_system([[F(1), F(-1)], [F(-1), F(1)]], [F(1), F(1)])

    def test_first_hit_prefix_and_return_burden(self):
        vr, vd = two_state_value(F(2), F(1), F(0), F(1, 5), F(1, 2), F(1, 4), F(1, 4))
        # First visit to d costs 2/(1/5)=10, and then 7 including reacquisition.
        self.assertEqual(F(2)/F(1, 5)+vd, vr)
        self.assertEqual(vd, F(1)+F(1, 4)*vr+F(1, 4)*vd)

    def test_local_finish_price_is_not_original_root_cost(self):
        root, tail = two_state_value(F(50), F(1), F(0), F(1, 100), F(1), F(0), F(0))
        self.assertEqual(tail, 1)
        self.assertEqual(root, 5001)

    def test_all_outcome_preservation_is_stronger_than_some(self):
        held = 0b011
        kept = [(F(1, 2), 0b011), (F(1, 2), 0b111)]
        loses = [(F(1, 2), 0b011), (F(1, 2), 0b001)]
        check = lambda outcomes: all((mask & held) == held for p, mask in outcomes if p > 0)
        self.assertTrue(check(kept))
        self.assertFalse(check(loses))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=Path('checks/results.json'))
    args = parser.parse_args()
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(SpecificationChecks)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    payload = {
        'schema': 'seed_retention_abstract_checks_v1',
        'scope': 'exact-rational toys and proposed diagnostic specifications; not native PoE qualification',
        'tests_run': result.testsRun,
        'failures': len(result.failures), 'errors': len(result.errors),
        'success': result.wasSuccessful(),
        'generated_two_state_bindings': 24,
        'native_tests_run': 0,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(payload, indent=2)+'\n', encoding='utf-8')
    raise SystemExit(0 if result.wasSuccessful() else 1)
