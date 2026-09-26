"""Exact-rational specification checks, NOT tests of the native C++ engine.

Run: python checks/test_contracts.py --json checks/results.json
No external packages, network, PoE probabilities, or benchmark claims.
"""
from __future__ import annotations
import argparse
from dataclasses import dataclass
from fractions import Fraction as F
from itertools import product
import json
from pathlib import Path
import unittest


def solve(a: list[list[F]], b: list[F]) -> list[F]:
    n = len(b)
    if len(a) != n or any(len(row) != n for row in a):
        raise ValueError('matrix dimensions differ')
    m = [[F(x) for x in a[i]] + [F(b[i])] for i in range(n)]
    for col in range(n):
        pivot = next((i for i in range(col, n) if m[i][col]), None)
        if pivot is None:
            raise ValueError('singular: no unique finite policy value')
        m[col], m[pivot] = m[pivot], m[col]
        p = m[col][col]
        m[col] = [x / p for x in m[col]]
        for i in range(n):
            if i == col:
                continue
            p = m[i][col]
            m[i] = [x - p * y for x, y in zip(m[i], m[col])]
    return [m[i][-1] for i in range(n)]


def policy_values(q: list[list[F]], cost: list[F]) -> list[F]:
    if any(c < 0 for c in cost):
        raise ValueError('selected profile requires nonnegative cost')
    if len(q) != len(cost) or any(len(r) != len(q) for r in q):
        raise ValueError('dimensions differ')
    if any(p < 0 for r in q for p in r) or any(sum(r) > 1 for r in q):
        raise ValueError('not substochastic')
    a = [[F(i == j) - q[i][j] for j in range(len(q))] for i in range(len(q))]
    return solve(a, cost)


def subsolution(values: list[F], rows: list[tuple[int, F, dict[int, F]]],
                targets: set[int]) -> bool:
    if any(v < 0 for v in values) or any(values[i] for i in targets):
        return False
    return all(values[i] <= c + sum(p * values[j] for j, p in succ.items())
               for i, c, succ in rows if i not in targets)


@dataclass(frozen=True)
class Target:
    required: frozenset[str]
    minimum: int
    rarity: str = 'rare'
    extras_allowed: bool = False
    prefix_count: int | None = None
    suffix_count: int | None = None

    def matches(self, present: frozenset[str], prefixes: int, suffixes: int,
                rarity: str = 'rare') -> bool:
        satisfied = len(self.required & present)
        return (rarity == self.rarity and satisfied >= self.minimum
                and (self.extras_allowed or prefixes + suffixes == satisfied)
                and (self.prefix_count is None or prefixes == self.prefix_count)
                and (self.suffix_count is None or suffixes == self.suffix_count))


class ContractTests(unittest.TestCase):
    def test_target_change_changes_value(self):
        clean = policy_values([[F(0), F(1)], [F(0), F(0)]], [F(1), F(100)])
        coverage = policy_values([[F(0)]], [F(1)])
        self.assertEqual(clean[0], 101)
        self.assertEqual(coverage[0], 1)

    def test_zero_lower_and_old_predecessor_counterexample(self):
        rows = [(0, F(1), {1:F(1)}), (1, F(100), {})]
        self.assertTrue(subsolution([F(0), F(0)], rows, {1}))
        self.assertFalse(subsolution([F(101), F(0)], rows, {1}))
        self.assertTrue(subsolution([F(101), F(100)], rows, set()))

    def test_zero_lower_does_not_prove_properness(self):
        self.assertTrue(subsolution([F(0)], [(0,F(0),{0:F(1)})], set()))
        with self.assertRaises(ValueError):
            policy_values([[F(1)]], [F(0)])

    def test_goal_observers_are_not_objective(self):
        r = Target(frozenset({'A'}),1,extras_allowed=True)
        self.assertTrue(r.matches(frozenset({'A'}),1,1))
        wrong = Target(frozenset({'A','B'}),2,extras_allowed=True)
        self.assertFalse(wrong.matches(frozenset({'A'}),1,1))

    def test_non_goal_observation_changes_execution(self):
        # Same requested A truth, B determines the selected cost.
        costs = {False:F(1), True:F(100)}
        self.assertEqual(F(1,4)*costs[False]+F(3,4)*costs[True], F(301,4))
        self.assertNotEqual(costs[False], costs[True])

    def test_any_k_not_all_observers(self):
        r = Target(frozenset({'A','B','C'}),2,extras_allowed=True)
        self.assertTrue(r.matches(frozenset({'A','C'}),2,1))
        self.assertFalse(Target(r.required,3,extras_allowed=True).matches(
            frozenset({'A','C'}),2,1))

    def test_initial_covered_root_is_target_specific(self):
        present = frozenset({'A'})
        self.assertTrue(Target(present,1,extras_allowed=True).matches(present,1,1))
        self.assertFalse(Target(present,1).matches(present,1,1))

    def test_rarity_is_original_request_rarity(self):
        r = Target(frozenset({'A'}),1,rarity='magic',extras_allowed=True)
        self.assertTrue(r.matches(frozenset({'A'}),1,0,'magic'))
        self.assertFalse(r.matches(frozenset({'A'}),1,0,'rare'))

    def test_all_required_explicit_clean_equivalence(self):
        # Three distinct prefix roles and one suffix role; arbitrary missing/junk counts.
        roles = ('A','B','C','D')
        t = Target(frozenset(roles),4)
        e = Target(frozenset(roles),4,extras_allowed=True,prefix_count=3,suffix_count=1)
        for mask in range(16):
            present = frozenset(roles[i] for i in range(4) if mask & (1 << i))
            gp = sum(x in present for x in roles[:3]); gs = int('D' in present)
            for p in range(gp,4):
                for s in range(gs,4):
                    self.assertEqual(t.matches(present,p,s), e.matches(present,p,s))

    def test_any_k_clean_can_accept_more_than_k(self):
        t = Target(frozenset({'A','B','C'}),2)
        self.assertTrue(t.matches(t.required,3,0))
        wrong = Target(t.required,2,extras_allowed=True,prefix_count=2,suffix_count=0)
        self.assertFalse(wrong.matches(t.required,3,0))

    def test_reversible_coverage(self):
        t = Target(frozenset({'A','B'}),2,extras_allowed=True)
        self.assertTrue(t.matches(frozenset({'A','B'}),2,1))
        self.assertFalse(t.matches(frozenset({'A'}),1,1))

    def test_representative_does_not_prove_full_success_domain(self):
        t = Target(frozenset({'A'}),1)
        members = [(frozenset({'A'}),1,0),(frozenset({'A'}),1,1)]
        self.assertTrue(t.matches(*members[0]))
        self.assertFalse(all(t.matches(*m) for m in members))

    def test_optimized_difference_not_fixed_cleanup_bill(self):
        acquisition_a, tail_a, direct_b = F(1), F(100), F(5)
        loose_opt = min(acquisition_a,direct_b)
        clean_opt = min(acquisition_a+tail_a,direct_b)
        self.assertEqual(clean_opt-loose_opt,4)
        self.assertNotEqual(clean_opt-loose_opt,tail_a)

    def test_first_coverage_fixed_policy_decomposition_with_loss(self):
        # Acquisition retries; cleanup may lose progress back to acquisition.
        q = [[F(4,5),F(1,5)],[F(1,4),F(1,4)]]
        v = policy_values(q,[F(2),F(1)])
        prefix = F(2)/(1-F(4,5))
        self.assertEqual(v,[F(17),F(7)])
        self.assertEqual(v[0],prefix+v[1])

    def test_target_inclusion_generated_retry_models(self):
        # 32 different proper models; all coefficients are toy inputs.
        for p_num,loss_num in product(range(1,9),range(4)):
            p=F(p_num,10); loss=F(loss_num,10); repeat=F(1,5)
            q=[[1-p,p],[loss,repeat]]
            c=[F(1+p_num),F(2+loss_num)]
            clean=policy_values(q,c)[0]
            coverage=c[0]/p
            self.assertLessEqual(coverage,clean)

    def test_policy_lower_is_not_optimum_lower(self):
        optimum=F(4); checked_policy=F(10); interval=(F(9),F(11))
        self.assertLessEqual(interval[0],checked_policy)
        self.assertLessEqual(checked_policy,interval[1])
        self.assertGreater(interval[0],optimum)

    def test_no_target_portability_from_same_graph_identity(self):
        graph='same bytes'
        l=(graph,'clean','root','scope','prices')
        r=(graph,'coverage','root','scope','prices')
        repriced=(graph,'clean','root','scope','other prices')
        self.assertNotEqual(l,r)
        self.assertNotEqual(l,repriced)

    def test_nonnegative_price_precondition(self):
        with self.assertRaises(ValueError):
            policy_values([[F(0)]],[F(-1)])


def main() -> int:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--json',type=Path)
    args=parser.parse_args()
    suite=unittest.defaultTestLoader.loadTestsFromTestCase(ContractTests)
    result=unittest.TextTestRunner(verbosity=2).run(suite)
    record={'kind':'exact_rational_and_abstract_specification_tests_not_native',
            'tests_run':result.testsRun,'failures':len(result.failures),
            'errors':len(result.errors),'successful':result.wasSuccessful(),
            'generated_retry_models_in_one_test':32,
            'native_tests_run':0,'native_benchmarks_run':0,
            'notes':'No native C++ imported; tests check stated toy arguments, not implementation premises.'}
    if args.json:
        args.json.parent.mkdir(parents=True,exist_ok=True)
        args.json.write_text(json.dumps(record,indent=2)+'\n',encoding='utf-8')
    return 0 if result.wasSuccessful() else 1

if __name__=='__main__':
    raise SystemExit(main())
