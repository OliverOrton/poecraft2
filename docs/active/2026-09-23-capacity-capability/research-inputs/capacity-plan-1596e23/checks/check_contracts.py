#!/usr/bin/env python3
"""Exact arithmetic and toy resource contracts. Not native PoE qualification.

Run: python checks/check_contracts.py --output evidence/check_results.json
Only the requested output is written. No repository, network or solver access.
"""
from __future__ import annotations

import argparse
from dataclasses import dataclass
from fractions import Fraction as F
from pathlib import Path
import json
import unittest


def solve_linear(a: list[list[F]], b: list[F]) -> list[F]:
    """Small exact Gauss-Jordan solver; singular systems are explicit refusals."""
    n = len(b)
    if len(a) != n or any(len(row) != n for row in a):
        raise ValueError("dimension mismatch")
    m = [[F(v) for v in row] + [F(bi)] for row, bi in zip(a, b)]
    for col in range(n):
        pivot = next((r for r in range(col, n) if m[r][col]), None)
        if pivot is None:
            raise ValueError("singular system")
        m[col], m[pivot] = m[pivot], m[col]
        scale = m[col][col]
        m[col] = [v / scale for v in m[col]]
        for r in range(n):
            if r != col:
                scale = m[r][col]
                m[r] = [x - scale * y for x, y in zip(m[r], m[col])]
    return [row[-1] for row in m]


def fixed_value(q: list[list[F]], c: list[F]) -> list[F]:
    n = len(c)
    return solve_linear([[F(i == j) - q[i][j] for j in range(n)] for i in range(n)], c)


@dataclass
class ToyLedger:
    """Illustrative staging/debit contract, not the project's accounting API."""
    limit: int = 10
    live: int = 4
    spent: int = 0
    work_limit: int = 8
    staged: int = 0

    def stage(self, memory: int, work: int) -> bool:
        if memory < 0 or work < 0:
            raise ValueError("negative reservation")
        if self.live + memory > self.limit or self.spent + work > self.work_limit:
            return False
        self.live += memory
        self.staged += memory
        self.spent += work
        return True

    def rollback(self) -> None:
        self.live -= self.staged
        self.staged = 0


class Contracts(unittest.TestCase):
    def test_optional_refusal_can_leave_useful_fitting_work(self):
        ledger = ToyLedger()
        self.assertTrue(ledger.stage(2, 3))
        self.assertFalse(ledger.stage(6, 1))
        ledger.rollback()
        self.assertEqual((ledger.live, ledger.spent), (4, 3))
        self.assertTrue(ledger.stage(3, 2))
        self.assertEqual((ledger.live, ledger.spent), (7, 5))

    def test_release_never_refunds_cumulative_work(self):
        ledger = ToyLedger(work_limit=3)
        self.assertTrue(ledger.stage(2, 3))
        ledger.rollback()
        self.assertFalse(ledger.stage(1, 1))
        self.assertEqual((ledger.live, ledger.spent), (4, 3))

    def test_rearm_requires_relevant_change_not_clock(self):
        # An immutable blocked task at headroom 4 stays blocked on mere ticks.
        blocked = ("exact-task-A", 4)
        eligible = lambda task, headroom: task != blocked[0] or headroom > blocked[1]
        for _ in range(100):
            self.assertFalse(eligible("exact-task-A", 4))
        self.assertTrue(eligible("exact-task-A", 7))
        self.assertTrue(eligible("different-complete-task", 4))

    def test_action_restriction_upper_not_full_scope_lower(self):
        full_optimum = min(F(1), F(10))
        restricted_optimum = min([F(10)])
        self.assertLessEqual(full_optimum, restricted_optimum)
        self.assertEqual(fixed_value([[F(0)]], [restricted_optimum]), [F(10)])
        self.assertFalse(restricted_optimum <= full_optimum)

    def test_missing_mass_is_not_zero(self):
        known_probability, unknown_probability = F(999, 1000), F(1, 1000)
        known_value, unknown_value = F(1), F(1_000_000)
        complete = known_probability * known_value + unknown_probability * unknown_value
        ignored = known_probability * known_value
        self.assertEqual(complete - ignored, F(1000))
        self.assertEqual(known_probability + unknown_probability, F(1))

    def test_geometric_cost_retains_retry_mass(self):
        for success in [F(1, 2), F(1, 10), F(1, 1000)]:
            self.assertEqual(fixed_value([[1-success]], [F(1)])[0], 1/success)

    def test_zero_advantage_is_not_properness(self):
        q = [[F(0), F(1)], [F(1), F(0)]]
        old_v = [F(1), F(1)]
        residual = [sum(q[i][j] * old_v[j] for j in range(2)) - old_v[i] for i in range(2)]
        self.assertEqual(residual, [F(0), F(0)])
        with self.assertRaisesRegex(ValueError, "singular"):
            fixed_value(q, [F(0), F(0)])

    def test_changed_controller_uses_new_occupancy(self):
        old_q = [[F(1, 2), F(1, 2)], [F(0), F(0)]]
        new_q = [[F(0), F(1)], [F(0), F(0)]]
        old_c, new_c = [F(1), F(10)], [F(1), F(5)]
        old_v, new_v = fixed_value(old_q, old_c), fixed_value(new_q, new_c)
        advantage = [new_c[i] + sum(new_q[i][j]*old_v[j] for j in range(2)) - old_v[i] for i in range(2)]
        self.assertEqual(old_v, [F(12), F(10)])
        self.assertEqual(new_v, [F(6), F(5)])
        self.assertEqual(advantage, [F(-1), F(-5)])
        self.assertEqual(fixed_value(new_q, advantage)[0], new_v[0]-old_v[0])
        self.assertNotEqual(2*advantage[0]+advantage[1], new_v[0]-old_v[0])

    def test_selected_cell_requires_whole_member_law(self):
        # Same labels cannot equate these different goal/continuation laws.
        law_x = (F(1), F(0))
        law_y = (F(1, 2), F(1, 2))
        self.assertNotEqual(law_x, law_y)
        v = (F(0), F(10))
        self.assertNotEqual(sum(p*x for p, x in zip(law_x, v)), sum(p*x for p, x in zip(law_y, v)))

    def test_peak_is_concurrent_not_sum_of_peaks(self):
        stages = [{"parent": 4, "scratch": 6, "checker": 0},
                  {"parent": 4, "scratch": 0, "checker": 7}]
        self.assertEqual(max(sum(s.values()) for s in stages), 11)
        self.assertEqual(sum(max(s[k] for s in stages) for k in stages[0]), 17)
        self.assertEqual(6+9, 15)  # old/new payload overlap can exceed final 9.

    def test_mean_count_does_not_define_finite_budget_success(self):
        mean_deterministic, mean_geometric = F(10), 1/F(1, 10)
        self.assertEqual(mean_deterministic, mean_geometric)
        self.assertEqual(1-F(9, 10)**5, F(40951, 100000))
        self.assertEqual(F(0), F(int(10 <= 5)))


def arithmetic() -> dict[str, str]:
    ring = F("149977.25092497544")
    wide = F("1620.317363896656")
    c4_old, c4_new = F("5218.040949685988"), F("3746.1319409485764")
    peak, cap = 1736778351, 1 << 30
    return {
        "ring_material_target_decimal": f"{float(ring*F(3,4)):.12f}",
        "ring_wide_different_profile_reduction_percent": f"{float(100*(ring-wide)/ring):.9f}",
        "c4_transport_recovery_percent": f"{float(100*(c4_old-c4_new)/c4_old):.9f}",
        "historical_peak_excess_bytes": str(peak-cap),
        "historical_peak_reduction_to_one_gib_percent": f"{100*(peak-cap)/peak:.9f}",
        "ring_request_to_ui_ms": str(F("12408.747")-F("50.5176")),
        "c5_request_to_ui_ms": str(F("40501.863")-F("53.9308")),
        "c5_manual_finish_to_ui_ms": str(F("40501.863")-F("40035.7609")),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(Contracts)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    record = {
        "kind": "abstract_specification_checks_not_native_qualification",
        "tests_run": result.testsRun,
        "failures": [{"test": str(t), "detail": d} for t, d in result.failures],
        "errors": [{"test": str(t), "detail": d} for t, d in result.errors],
        "successful": result.wasSuccessful(),
        "recorded_value_arithmetic_only": arithmetic(),
        "limits": ["No native engine or mechanics invoked", "Toy ledger is not native accounting", "Finite rational fixtures are not a general correctness proof"],
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(record, indent=2)+"\n", encoding="utf-8")
    return 0 if result.wasSuccessful() else 1

if __name__ == "__main__":
    raise SystemExit(main())
