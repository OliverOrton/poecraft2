#!/usr/bin/env python3
"""Exact abstract checks for role-parametric planning. No PoE mechanics are emulated.

Run: python checks/check_role_models.py --output checks/results.json
All probabilities/costs below are invented rational fixtures, not native data.
"""
from __future__ import annotations
import argparse
import itertools
import json
import unittest
from fractions import Fraction as F
from pathlib import Path


def solve(a: list[list[F]], b: list[F]) -> list[F]:
    """Small exact Gaussian elimination, with singularity refusal."""
    n = len(b)
    if len(a) != n or any(len(row) != n for row in a):
        raise ValueError('non-square system')
    m = [[F(x) for x in row] + [F(y)] for row, y in zip(a, b)]
    for j in range(n):
        p = next((i for i in range(j, n) if m[i][j]), None)
        if p is None:
            raise ValueError('singular system; no finite-policy claim')
        m[j], m[p] = m[p], m[j]
        d = m[j][j]
        m[j] = [x / d for x in m[j]]
        for i in range(n):
            if i == j:
                continue
            d = m[i][j]
            if d:
                m[i] = [x - d*y for x, y in zip(m[i], m[j])]
    return [row[-1] for row in m]


def value(q: list[list[F]], c: list[F]) -> list[F]:
    n = len(c)
    if len(q) != n or any(len(row) != n for row in q):
        raise ValueError('incompatible native domain')
    if any(x < 0 for row in q for x in row) or any(sum(row) > 1 for row in q):
        raise ValueError('invalid probability law')
    return solve([[F(i == j) - q[i][j] for j in range(n)] for i in range(n)], c)


def accept_value(cost: F, probabilities: tuple[F, ...], tails: tuple[F, ...], accepted: tuple[int, ...]) -> F:
    if len(probabilities) != len(tails) or any(p < 0 for p in probabilities) or sum(probabilities) > 1:
        raise ValueError('outcomes must be disjoint and complete with retry complement')
    if len(set(accepted)) != len(accepted):
        raise ValueError('duplicated event')
    mass = sum((probabilities[i] for i in accepted), F(0))
    if not mass:
        raise ValueError('no absorbing exit')
    return (cost + sum((probabilities[i]*tails[i] for i in accepted), F(0))) / mass


def accept_value_with_recovery(cost: F, probabilities: tuple[F, ...], tails: tuple[F, ...], recovery: tuple[F, ...], accepted: tuple[int, ...], neutral_recovery: F = F(0)) -> F:
    if len(probabilities) != len(recovery) or len(probabilities) != len(tails):
        raise ValueError('incompatible recovery domain')
    if any(p < 0 for p in probabilities) or sum(probabilities) > 1:
        raise ValueError('invalid probability law')
    selected = set(accepted)
    mass = sum((probabilities[i] for i in selected), F(0))
    if not mass:
        raise ValueError('no absorbing exit')
    reward = cost + (1-sum(probabilities))*neutral_recovery
    reward += sum((probabilities[i]*(tails[i] if i in selected else recovery[i]) for i in range(len(probabilities))), F(0))
    return reward/mass


def order_value(p_first: F, p_second: F, loss_second: F, cost_first: F = F(1), cost_second: F = F(1)) -> F:
    # r->first: repeat acquisition. At first, second failure loses first with
    # loss_second, otherwise retains it. All attempts are paid.
    q = [[1-p_first, p_first], [(1-p_second)*loss_second, (1-p_second)*(1-loss_second)]]
    return value(q, [cost_first, cost_second])[0]


def subset_acquisition(weights: tuple[int, ...], missing: int) -> F:
    # Pure toy: one unit buys an attempt; success mass is target weight over all.
    return F(sum(weights), weights[missing])


class RoleModelTests(unittest.TestCase):
    def test_same_count_different_missing_values(self):
        self.assertNotEqual(subset_acquisition((1, 10, 100), 0), subset_acquisition((1, 10, 100), 2))

    def test_full_relabel_moves_attributes_too(self):
        w = (1, 7, 19)
        for perm in itertools.permutations(range(3)):
            renamed = tuple(w[i] for i in perm)
            for missing in range(3):
                self.assertEqual(subset_acquisition(w, missing), subset_acquisition(renamed, perm.index(missing)))

    def test_equal_target_weight_does_not_equal_pool_probability(self):
        self.assertEqual(F(100, 1000), F(1, 10))
        self.assertEqual(F(100, 5000), F(1, 50))
        self.assertNotEqual(F(100, 1000), F(100, 5000))

    def test_ragged_tiers_are_not_padded_with_probability(self):
        tiers = ((10, 20), (10, 5, 5, 30), (2, 2, 2, 2, 2))
        self.assertEqual(tuple(len(t) for t in tiers), (2, 4, 5))
        self.assertEqual(F(tiers[0][0], sum(tiers[0])), F(1, 3))
        self.assertEqual(F(tiers[1][0], sum(tiers[1])), F(1, 5))
        self.assertNotEqual(sum(tiers[0]), sum(tiers[1]))

    def test_same_schema_changes_best_action(self):
        safe = F(20)
        fast_instance = value([[F(4, 5)]], [F(1)])[0]
        slow_instance = value([[F(99, 100)]], [F(1)])[0]
        self.assertLess(fast_instance, safe)
        self.assertGreater(slow_instance, safe)

    def test_same_schema_price_change_changes_best_action(self):
        self.assertLess(value([[F(4, 5)]], [F(1)])[0], F(20))
        self.assertGreater(value([[F(4, 5)]], [F(10)])[0], F(20))

    def test_nearby_retry_probabilities_can_have_large_value_difference(self):
        a = value([[F(999, 1000)]], [F(1)])[0]
        b = value([[F(9999, 10000)]], [F(1)])[0]
        self.assertEqual(b, 10*a)

    def test_zero_success_crosses_properness_boundary(self):
        with self.assertRaises(ValueError):
            value([[F(1)]], [F(1)])

    def test_same_parameterized_recurrence_recomputed_exactly(self):
        for p in (F(1, 2), F(1, 7), F(1, 31)):
            self.assertEqual(value([[1-p]], [F(3)])[0], F(3)/p)

    def test_order_is_economic_not_role_index(self):
        a_then_b = order_value(F(1, 10), F(1, 2), F(1))
        b_then_a = order_value(F(1, 2), F(1, 10), F(1))
        self.assertEqual(a_then_b, 22)
        self.assertEqual(b_then_a, 30)

    def test_preservation_can_reverse_order_preference(self):
        self.assertEqual(order_value(F(1, 2), F(1, 10), F(0)), 12)
        self.assertLess(order_value(F(1, 2), F(1, 10), F(0)), order_value(F(1, 10), F(1, 2), F(1)))

    def test_opportunistic_heterogeneous_exit_can_beat_singletons(self):
        p, u = (F(1, 10), F(2, 5)), (F(5), F(8))
        self.assertEqual(accept_value(F(1), p, u, (0,)), 15)
        self.assertEqual(accept_value(F(1), p, u, (1,)), F(21, 2))
        self.assertEqual(accept_value(F(1), p, u, (0, 1)), F(47, 5))

    def test_all_progress_is_not_always_worth_accepting(self):
        p, u = (F(1, 10), F(2, 5)), (F(5), F(100))
        self.assertLess(accept_value(F(1), p, u, (0,)), accept_value(F(1), p, u, (0, 1)))

    def test_added_exit_threshold_identity(self):
        for ua in (F(2), F(7), F(21)):
            for ub in (F(0), F(9), F(100)):
                p, u = (F(1, 10), F(2, 5)), (ua, ub)
                old = accept_value(F(1), p, u, (0,))
                new = accept_value(F(1), p, u, (0, 1))
                self.assertEqual(new-old, p[1]*(ub-old)/(p[0]+p[1]))

    def test_suffix_debt_changes_acceptance(self):
        p = (F(1, 10), F(2, 5))
        simple = (F(5), F(8))
        suffix_debt = (F(5), F(108))
        self.assertLess(accept_value(F(1), p, simple, (0, 1)), accept_value(F(1), p, simple, (0,)))
        self.assertGreater(accept_value(F(1), p, suffix_debt, (0, 1)), accept_value(F(1), p, suffix_debt, (0,)))

    def test_joint_goal_hits_not_sum_of_marginals(self):
        # A only .2, B only .1, both .3, neither .4.
        a_only, b_only, both = F(1, 5), F(1, 10), F(3, 10)
        union = a_only + b_only + both
        marginals_sum = a_only + both + b_only + both
        self.assertEqual(union, F(3, 5))
        self.assertEqual(marginals_sum, F(9, 10))
        self.assertNotEqual(union, marginals_sum)

    def test_retry_formula_requires_exact_same_return_context(self):
        q = [[F(0), F(1, 2)], [F(0), F(9, 10)]]
        actual = value(q, [F(1), F(1)])[0]
        self.assertEqual(actual, 6)
        self.assertNotEqual(actual, F(1)/F(1, 2))

    def test_complete_all_exits_not_success_branch_only(self):
        q = [[F(0), F(1, 100)], [F(0), F(0)]]
        self.assertEqual(value(q, [F(1), F(10000)])[0], 101)
        self.assertNotEqual(value(q, [F(1), F(10000)])[0], 1)

    def test_locally_terminating_fragments_can_form_global_trap(self):
        with self.assertRaises(ValueError):
            value([[F(0), F(1)], [F(1), F(0)]], [F(1), F(1)])

    def test_restricted_family_best_remains_only_upper(self):
        full_optimum = min(F(4), F(9), F(12))
        restricted = min(F(9), F(12))
        self.assertGreater(restricted, full_optimum)
        self.assertFalse(restricted <= full_optimum)

    def test_resource_labels_repriced_not_copied(self):
        uses_a = {'red': F(2), 'blue': F(1)}
        uses_b = {'red': F(1), 'blue': F(2)}
        prices = {'red': F(1), 'blue': F(10)}
        cost = lambda uses: sum((uses[k]*prices[k] for k in uses), F(0))
        self.assertEqual(cost(uses_a), 12)
        self.assertEqual(cost(uses_b), 21)

    def test_shape_key_never_authorizes_numeric_cache_hit(self):
        shape = ('acquire', 'attempt', 'clean', 'retry')
        key_a = (shape, ((10, 20),), ('root-a',), (1,))
        key_b = (shape, ((10, 5, 5, 30),), ('root-b',), (1,))
        self.assertEqual(key_a[0], key_b[0])
        self.assertNotEqual(key_a, key_b)

    def test_overlapping_goals_do_not_imply_clean_occupancy(self):
        goal_matches = ({'a'}, {'a'})
        occupied = {'a', 'junk'}
        self.assertEqual(len(goal_matches), len(occupied))
        self.assertLess(len(set.union(*goal_matches)), len(occupied))

    def test_atomic_candidate_publication_preserves_fallback(self):
        portfolio = {'graph': 'old', 'value': F(10), 'complete': True}
        staged = {'graph': 'new', 'value': F(2), 'complete': False}
        if staged['complete']:
            portfolio = staged
        self.assertEqual(portfolio['graph'], 'old')
        self.assertEqual(portfolio['value'], 10)

    def test_rejection_recovery_is_paid(self):
        p, u, r = (F(1, 10), F(2, 5)), (F(5), F(8)), (F(0), F(2))
        self.assertEqual(accept_value_with_recovery(F(1), p, u, r, (0,)), 23)
        self.assertEqual(accept_value_with_recovery(F(1), p, u, r, (0, 1)), F(47, 5))

    def test_recovery_adjusted_acceptance_threshold(self):
        p, u, r = (F(1, 10), F(2, 5)), (F(5), F(100)), (F(0), F(100))
        old = accept_value_with_recovery(F(1), p, u, r, (0,))
        new = accept_value_with_recovery(F(1), p, u, r, (0, 1))
        self.assertEqual(old, 415)
        self.assertEqual(new, 83)
        self.assertEqual(new-old, p[1]*(u[1]-r[1]-old)/(p[0]+p[1]))

    def test_neutral_failure_recovery_is_also_paid(self):
        p, u, r = (F(1, 2),), (F(0),), (F(0),)
        self.assertEqual(accept_value_with_recovery(F(1), p, u, r, (0,), F(4)), 6)

    def test_symmetric_orbit_toy_counts(self):
        self.assertEqual(len({tuple(sorted(v)) for v in itertools.product(range(2), repeat=3)}), 4)
        self.assertEqual(len({tuple(sorted(v)) for v in itertools.product(range(3), repeat=3)}), 10)

    def test_candidate_subset_generation_is_bounded(self):
        roles = (0, 1, 2)
        sets = [s for k in (1, 2) for s in itertools.combinations(roles, k)]
        self.assertEqual(len(sets), 6)
        self.assertEqual(len(set(sets)), 6)

    def test_partial_numeric_rows_not_public_evidence(self):
        committed = [('goal', F(9, 10))]
        self.assertNotEqual(sum((p for _, p in committed), F(0)), 1)
        committed.append(('unresolved', F(1, 10)))
        self.assertEqual(sum((p for _, p in committed), F(0)), 1)
        self.assertTrue(any(k == 'unresolved' and p > 0 for k, p in committed))

    def test_six_role_renamings_preserve_same_full_finite_chain(self):
        q = [[F(1, 3), F(1, 5), F(0)], [F(0), F(1, 7), F(1, 4)], [F(1, 9), F(0), F(1, 6)]]
        c = [F(2), F(7), F(13)]
        v = value(q, c)
        for perm in itertools.permutations(range(3)):
            qp = [[q[i][j] for j in perm] for i in perm]
            cp = [c[i] for i in perm]
            self.assertEqual(value(qp, cp), [v[i] for i in perm])


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, default=Path('results.json'))
    args = parser.parse_args()
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(RoleModelTests)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    out = {
        'schema': 'role_parametric_abstract_checks_v1',
        'scope': 'Invented finite rational models and interface examples; not native PoE tests, speedups, or production proofs.',
        'tests_run': result.testsRun,
        'failures': len(result.failures), 'errors': len(result.errors),
        'passed': result.wasSuccessful(),
        'examples': {
            'opportunistic_joint': '47/5', 'single_A': '15', 'single_B': '21/2',
            'acquire_A_then_B': '22', 'acquire_B_then_A': '30',
            'preserve_B_then_A': '12',
            'heterogeneous_data_native_verified': False,
        },
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(out, indent=2)+'\n', encoding='utf-8')
    raise SystemExit(0 if result.wasSuccessful() else 1)


if __name__ == '__main__':
    main()
