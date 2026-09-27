"""Exact-rational examples for the planning packet; NOT native PoE tests.

No game mechanics, repository implementation, or performance is emulated.
The tests exercise the conditional equations and counterexamples stated in
MATHEMATICS.md. Native C++ validation remains an implementation obligation.
"""
from __future__ import annotations

import argparse
from fractions import Fraction as F
import json
from pathlib import Path
import unittest

COUNTS = {"generated_target_comparisons": 0, "generated_policy_comparisons": 0}


def solve(a: list[list[F]], b: list[F]) -> list[F]:
    n = len(b)
    if len(a) != n or any(len(row) != n for row in a):
        raise ValueError("non-square system")
    m = [[F(x) for x in row] + [F(bi)] for row, bi in zip(a, b)]
    for k in range(n):
        pivot = next((r for r in range(k, n) if m[r][k]), None)
        if pivot is None:
            raise ValueError("singular system")
        m[k], m[pivot] = m[pivot], m[k]
        scale = m[k][k]
        m[k] = [v / scale for v in m[k]]
        for r in range(n):
            if r != k:
                scale = m[r][k]
                m[r] = [x - scale * y for x, y in zip(m[r], m[k])]
    return [row[-1] for row in m]


def policy(q: list[list[F]], costs: list[F], targets: frozenset[int] = frozenset()) -> list[F]:
    n = len(costs)
    if len(q) != n or any(len(row) != n for row in q):
        raise ValueError("bad matrix size")
    q = [[F(x) for x in row] for row in q]
    if any(x < 0 for row in q for x in row) or any(sum(row) > 1 for row in q):
        raise ValueError("not substochastic")
    if any(F(x) < 0 for x in costs):
        raise ValueError("negative cost outside selected assumptions")
    a, b = [], []
    for i in range(n):
        a.append([F(i == j) - (F(0) if i in targets else q[i][j]) for j in range(n)])
        b.append(F(0) if i in targets else F(costs[i]))
    return solve(a, b)


def subsolution(q: list[list[F]], costs: list[F], h: list[F], targets: frozenset[int]) -> bool:
    if any(v < 0 for v in h) or any(h[t] != 0 for t in targets):
        return False
    return all(i in targets or h[i] <= costs[i] + sum(p * v for p, v in zip(row, h))
               for i, row in enumerate(q))


class ContractExamples(unittest.TestCase):
    def test_clean_vs_coverage_changes_value(self):
        q = [[F(0), F(1)], [F(0), F(0)]]
        self.assertEqual(policy(q, [F(1), F(100)])[0], 101)
        self.assertEqual(policy(q, [F(1), F(100)], frozenset({1}))[0], 1)

    def test_zeroing_terminal_only_is_not_a_valid_predecessor(self):
        q = [[F(0), F(1)], [F(0), F(0)]]
        self.assertFalse(subsolution(q, [F(1), F(100)], [F(101), F(0)], frozenset({1})))

    def test_zero_is_valid_lower_under_nonnegative_costs(self):
        q = [[F(1, 2), F(1, 4)], [F(1, 5), F(1, 3)]]
        self.assertTrue(subsolution(q, [F(2), F(5)], [F(0), F(0)], frozenset()))

    def test_terminal_self_policy_value_survives_zero_lower(self):
        self.assertEqual(policy([[F(1, 2)]], [F(2)]), [F(4)])
        self.assertTrue(subsolution([[F(1, 2)]], [F(2)], [F(0)], frozenset()))

    def test_selected_policy_value_is_not_optimal_lower(self):
        selected = policy([[F(1, 2)]], [F(2)])[0]
        alternative = policy([[F(0)]], [F(1)])[0]
        self.assertGreater(selected, alternative)
        self.assertFalse(subsolution([[F(0)]], [F(1)], [selected], frozenset()))

    def test_closed_non_goal_loop_is_not_a_finite_policy(self):
        with self.assertRaises(ValueError):
            policy([[F(1)]], [F(1)])

    def test_zero_cost_closed_loop_still_not_proper(self):
        with self.assertRaises(ValueError):
            policy([[F(1)]], [F(0)])

    def test_original_root_cost_includes_acquisition(self):
        q = [[F(7, 10), F(1, 5)], [F(0), F(1, 2)]]
        v = policy(q, [F(2), F(1)])
        self.assertEqual(v, [F(8), F(2)])
        self.assertGreater(v[0], v[1])

    def test_selective_fill_reduces_reacquisition_in_example(self):
        old = policy([[F(7, 10), F(1, 5)], [F(1), F(0)]], [F(2), F(1)])
        new = policy([[F(7, 10), F(1, 5)], [F(0), F(1, 2)]], [F(2), F(1)])
        self.assertEqual(old[0], 22)
        self.assertEqual(new[0], 8)

    def test_expensive_fill_is_not_universally_better(self):
        new = policy([[F(7, 10), F(1, 5)], [F(0), F(1, 2)]], [F(2), F(100)])
        self.assertEqual(new[0], 140)
        self.assertGreater(new[0], 22)

    def test_external_port_changes_local_value(self):
        for u in [F(0), F(1), F(100)]:
            v = policy([[F(1, 4)]], [F(1) + F(1, 2)*u])[0]
            self.assertEqual(v, (4 + 2*u)/3)

    def test_external_port_can_reverse_action_choice(self):
        cheap = policy([[F(1, 4)]], [F(1)])[0]
        expensive = policy([[F(1, 4)]], [F(1) + F(50)])[0]
        self.assertLess(cheap, 10)
        self.assertGreater(expensive, 10)

    def test_one_shot_and_recurring_replacement_differ(self):
        old = policy([[F(9, 10)]], [F(1)])[0]
        one_shot = 2 + F(1, 2)*old
        recurring = policy([[F(1, 2)]], [F(2)])[0]
        self.assertEqual((old, one_shot, recurring), (10, 7, 4))

    def test_locally_terminating_fragments_can_form_improper_cycle(self):
        # Each selected fragment takes one step and exits to the other entry.
        with self.assertRaises(ValueError):
            policy([[F(0), F(1)], [F(1), F(0)]], [F(1), F(1)])

    def test_two_proper_policies_do_not_authorize_arbitrary_splicing(self):
        a = policy([[F(0), F(1)], [F(0), F(0)]], [F(1), F(1)])
        b = policy([[F(0), F(0)], [F(1), F(0)]], [F(1), F(1)])
        self.assertEqual((a, b), ([F(2), F(1)], [F(1), F(2)]))
        with self.assertRaises(ValueError):
            policy([[F(0), F(1)], [F(1), F(0)]], [F(1), F(1)])

    def test_whole_policy_choice_at_root_is_well_defined(self):
        costs = [policy([[F(3, 4)]], [F(1)])[0], policy([[F(1, 2)]], [F(1)])[0]]
        self.assertEqual(min(costs), 2)

    def test_heterogeneous_success_probabilities_need_different_values(self):
        a = policy([[F(4, 5)]], [F(1)])[0]
        b = policy([[F(99, 100)]], [F(1)])[0]
        self.assertEqual((a, b), (5, 100))
        self.assertLess(a, 20)
        self.assertGreater(b, 20)

    def test_setup_once_and_setup_each_retry_differ(self):
        # Setup 3 once, then attempt 2 repeatedly with success 1/4.
        once = policy([[F(0), F(1)], [F(0), F(3, 4)]], [F(3), F(2)])[0]
        # Same attempt returns to setup after a failure.
        repeat = policy([[F(0), F(1)], [F(3, 4), F(0)]], [F(3), F(2)])[0]
        self.assertEqual((once, repeat), (11, 20))

    def test_first_hit_bill_is_not_visit_weighted_remaining_cost(self):
        q = [[F(7, 10), F(1, 5)], [F(1), F(0)]]
        v = policy(q, [F(2), F(1)])
        stopped = policy(q, [F(2), F(1)], frozenset({1}))[0]
        first_hit = F(1, 5) / (1-F(7, 10))
        self.assertEqual(stopped + first_hit*v[1], v[0])
        visits = solve([[F(3, 10), F(-1)], [F(-1, 5), F(1)]], [F(1), F(0)])
        self.assertEqual(visits[1], 2)
        self.assertGreater(visits[1]*v[1], v[0])

    def test_generated_target_inclusion_for_fixed_proper_controllers(self):
        for i in range(8):
            for j in range(8):
                q = [[F(i, 20), F(j, 20)], [F(j, 30), F(i, 30)]]
                c = [F(i+1), F(j+2)]
                clean = policy(q, c)
                cover = policy(q, c, frozenset({1}))
                self.assertLessEqual(cover[0], clean[0])
                self.assertEqual(cover[1], 0)
                COUNTS['generated_target_comparisons'] += 1

    def test_generated_two_action_optima_respect_target_inclusion(self):
        for i in range(1, 9):
            # Four complete stationary controllers; all transient by row sums.
            choices = [
                [([F(1, 4), F(1, 4)], F(i)), ([F(0), F(1, 2)], F(2))],
                [([F(1, 5), F(1, 5)], F(3)), ([F(1, 4), F(0)], F(i+1))]
            ]
            vals_l, vals_r = [], []
            for a in range(2):
                for b in range(2):
                    q = [choices[0][a][0], choices[1][b][0]]
                    c = [choices[0][a][1], choices[1][b][1]]
                    vals_l.append(policy(q,c)[0]);vals_r.append(policy(q,c,frozenset({1}))[0])
                    COUNTS['generated_policy_comparisons'] += 1
            self.assertLessEqual(min(vals_r), min(vals_l))

    def test_invalid_probability_is_not_silently_normalized(self):
        with self.assertRaises(ValueError):
            policy([[F(11, 10)]], [F(1)])

    def test_zero_lower_assumption_excludes_negative_rewards(self):
        with self.assertRaises(ValueError):
            policy([[F(0)]], [F(-1)])

    def test_true_no_cost_target_is_zero(self):
        self.assertEqual(policy([[F(1)]], [F(99)], frozenset({0})), [F(0)])


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, default=Path(__file__).resolve().parents[1] / 'evidence' / 'abstract_test_results.json')
    args = parser.parse_args()
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(ContractExamples)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    record = {
        'scope': 'exact-rational abstract examples only; no native C++ or PoE qualification',
        'tests_run': result.testsRun, 'failures': len(result.failures), 'errors': len(result.errors),
        'successful': result.wasSuccessful(), 'generated_comparisons': COUNTS,
        'failure_details': [str(test) + '\n' + trace for test, trace in result.failures + result.errors]
    }
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(record,indent=2)+'\n',encoding='utf-8')
    raise SystemExit(0 if result.wasSuccessful() else 1)
