#!/usr/bin/env python3
"""Exact toy/specification checks. Does not import or emulate the PoE engine.

Run: python tests/goal_contract_checks.py --output evidence/abstract_results.json
"""
from __future__ import annotations

import argparse
from dataclasses import dataclass
from fractions import Fraction as F
from itertools import product
import hashlib
import io
import json
from pathlib import Path
import unittest

METRICS = {'valid_signature_comparisons': 0, 'renewal_model_comparisons': 0}

@dataclass(frozen=True)
class Item:
    # 0 absent, 1 present below threshold, 2 satisfies threshold.
    status: tuple[int, ...]
    sides: tuple[int, ...]
    junk_prefixes: int = 0
    junk_suffixes: int = 0
    rarity: int = 2
    @property
    def counts(self) -> tuple[int,int]:
        return (sum(s != 0 and side == 0 for s,side in zip(self.status,self.sides)) + self.junk_prefixes,
                sum(s != 0 and side == 1 for s,side in zip(self.status,self.sides)) + self.junk_suffixes)
    @property
    def satisfied(self) -> int:
        return sum(s == 2 for s in self.status)


def coverage(x: Item, required: int, rarity: int = 2) -> bool:
    return x.rarity == rarity and x.satisfied >= required


def clean(x: Item, required: int, rarity: int = 2) -> bool:
    return coverage(x,required,rarity) and sum(x.counts) == x.satisfied


def explicit(x: Item, required: int, count_pair: tuple[int,int], rarity: int = 2) -> bool:
    return coverage(x,required,rarity) and x.counts == count_pair


def solve_linear(a: list[list[F]], b: list[F]) -> list[F]:
    n = len(b)
    if any(len(row) != n for row in a):
        raise ValueError('non-square system')
    rows = [list(a[i]) + [b[i]] for i in range(n)]
    for j in range(n):
        pivot = next((i for i in range(j,n) if rows[i][j]), None)
        if pivot is None:
            raise ValueError('singular system')
        rows[j],rows[pivot] = rows[pivot],rows[j]
        div = rows[j][j]
        rows[j] = [v/div for v in rows[j]]
        for i in range(n):
            if i == j: continue
            factor = rows[i][j]
            rows[i] = [x-factor*y for x,y in zip(rows[i],rows[j])]
    return [row[-1] for row in rows]


def renewal(a: F, b: F, p: F, q: F, loss: F) -> tuple[F,F]:
    if not (0 < p <= 1 and 0 < q <= 1 and 0 <= loss <= 1-q):
        raise ValueError('invalid proper renewal parameters')
    v = solve_linear([[p,-p],[-loss,q+loss]], [a,b])
    return v[0],v[1]


def valid_subsolution(h: tuple[F,F], targets: frozenset[int]) -> bool:
    # One-cost acquisition, one-hundred-cost cleanup; state 2 is clean.
    if any(h[i] != 0 for i in targets if i < 2): return False
    if 0 not in targets and h[0] > 1 + (F(0) if 1 in targets else h[1]): return False
    if 1 not in targets and h[1] > 100: return False
    return all(v >= 0 for v in h)


class GoalContractTests(unittest.TestCase):
    def test_all_required_equivalence_exhaustive(self):
        for sides in [(0,0,0,1),(0,0,0,1,1),(0,),(0,1),(0,0,1,1),(0,0,0,1,1,1)]:
            counts = sides.count(0),sides.count(1)
            for status in product(range(3), repeat=len(sides)):
                for jp,js,r in product(range(4),range(4),range(3)):
                    x = Item(status,sides,jp,js,r)
                    if max(x.counts)>(0,1,3)[r]: continue
                    self.assertEqual(clean(x,len(sides)), explicit(x,len(sides),counts))
                    METRICS['valid_signature_comparisons'] += 1

    def test_coverage_includes_clean(self):
        sides=(0,0,1)
        for status in product(range(3),repeat=3):
            for jp,js in product(range(3),range(3)):
                x=Item(status,sides,jp,js)
                for required in (1,2,3):
                    self.assertFalse(clean(x,required) and not coverage(x,required))

    def test_threshold_exact_k_counterexample(self):
        x=Item((2,2,2),(0,0,1))
        self.assertTrue(clean(x,2))
        self.assertNotEqual(sum(x.counts),2)

    def test_below_tier_is_not_satisfying(self):
        x=Item((2,1),(0,1))
        self.assertFalse(coverage(x,2))
        self.assertFalse(clean(x,1))

    def test_requested_coverage_with_junk(self):
        x=Item((2,2,2,2),(0,0,0,1),0,2)
        self.assertTrue(coverage(x,4))
        self.assertFalse(clean(x,4))
        self.assertFalse(explicit(x,4,(3,1)))

    def test_rarity_remains_required(self):
        x=Item((2,),(0,),rarity=1)
        self.assertFalse(coverage(x,1,2))
        self.assertTrue(clean(x,1,1))

    def test_open_slots_uses_actual_cap(self):
        x=Item((2,2,2,2),(0,0,0,1),0,1)
        self.assertFalse(3-x.counts[1] >= 2)
        self.assertTrue(4-x.counts[1] >= 2)
        self.assertFalse(explicit(x,4,(3,1)))

    def test_positive_coverage_zero_clean_probability(self):
        # Abstract 10% example; NOT the user's unprovided ~0.35% request.
        dist=[(F(1,10),Item((2,),(0,),0,1)),(F(9,10),Item((0,),(0,),1,1))]
        pc=sum(p for p,x in dist if coverage(x,1))
        pt=sum(p for p,x in dist if clean(x,1))
        self.assertEqual(pc,F(1,10)); self.assertEqual(pt,0)

    def test_joint_coverage_is_not_marginal_product(self):
        dist=[(F(1,2),Item((2,0),(0,1))),(F(1,2),Item((0,2),(0,1)))]
        marginals=[sum(p for p,x in dist if x.status[i]==2) for i in (0,1)]
        joint=sum(p for p,x in dist if coverage(x,2))
        self.assertEqual(joint,0)
        self.assertEqual(marginals[0]*marginals[1],F(1,4))

    def test_clean_potential_invalid_for_loose(self):
        self.assertTrue(valid_subsolution((F(101),F(100)),frozenset({2})))
        self.assertFalse(valid_subsolution((F(101),F(100)),frozenset({1,2})))

    def test_zero_new_target_alone_is_insufficient(self):
        self.assertFalse(valid_subsolution((F(101),F(0)),frozenset({1,2})))
        self.assertTrue(valid_subsolution((F(1),F(0)),frozenset({1,2})))

    def test_loose_lower_safe_for_clean(self):
        self.assertTrue(valid_subsolution((F(1),F(0)),frozenset({2})))

    def test_ever_covered_is_not_current_coverage(self):
        before=Item((2,),(0,),0,1)
        after=Item((0,),(0,),1,0)
        self.assertTrue(coverage(before,1))
        self.assertFalse(coverage(after,1))
        self.assertEqual(sum(after.counts),1)
        self.assertFalse(clean(after,1))

    def test_optimal_acquisition_does_not_minimise_clean_cost(self):
        routes=[(F(1),F(100)),(F(5),F(0))]
        acquire=min(range(len(routes)),key=lambda i:routes[i][0])
        clean_winner=min(range(len(routes)),key=lambda i:sum(routes[i]))
        self.assertEqual(acquire,0); self.assertEqual(clean_winner,1)
        self.assertEqual(sum(routes[acquire]),101)
        self.assertEqual(sum(routes[clean_winner]),5)

    def test_optimal_value_difference_not_fixed_policy_tail(self):
        loose=F(1); strict=F(5)
        strict_policy_prefix=F(5); strict_policy_tail=F(0)
        self.assertEqual(strict_policy_prefix+strict_policy_tail,strict)
        self.assertNotEqual(strict-loose,strict_policy_tail)

    def test_paid_cleanup_reacquisition_example(self):
        root,dirty=renewal(F(2),F(1),F(1,5),F(1,2),F(1,4))
        self.assertEqual((root,dirty),(F(17),F(7)))
        self.assertEqual(root,F(2)/F(1,5)+dirty)

    def test_cleanup_model_formula_grid(self):
        for a,b,p,q in product((F(1),F(2),F(7)),(F(1),F(5)),(F(1,10),F(1,2),F(1)),(F(1,10),F(1,2),F(1))):
            for loss in (F(0),(1-q)/2,1-q):
                vr,vd=renewal(a,b,p,q,loss)
                expected=((q+loss)*a/p+b)/q
                self.assertEqual(vr,expected)
                self.assertEqual(vr,a/p+vd)
                self.assertGreaterEqual(vr,a/p)
                METRICS['renewal_model_comparisons'] += 1

    def test_local_exit_does_not_imply_global_success(self):
        # Each local action exits to the other node, but no goal is reachable.
        with self.assertRaises(ValueError):
            solve_linear([[F(1),F(-1)],[F(-1),F(1)]],[F(1),F(1)])

    def test_nontermination_guard(self):
        with self.assertRaises(ValueError):
            renewal(F(1),F(1),F(1,2),F(0),F(1,2))

    def test_historical_sandwich_sufficient(self):
        loose_policy_costs=[F(4),F(9)]
        clean_policy_costs=[F(4),F(12)]
        certified_loose=min(loose_policy_costs)
        clean_witness=F(4)
        self.assertEqual(certified_loose,min(clean_policy_costs))
        self.assertEqual(clean_witness,certified_loose)

    def test_historical_upper_alone_does_not_transfer_optimality(self):
        loose_opt=F(1); clean_opt=F(3); historical_checked_upper=F(5)
        self.assertLessEqual(loose_opt,clean_opt)
        self.assertLessEqual(clean_opt,historical_checked_upper)
        self.assertNotEqual(clean_opt,historical_checked_upper)

    def test_terminal_identity_separates_models(self):
        common={'goals':['A','B'],'rarity':'rare','prices':'pinned'}
        def identity(terminal):
            return hashlib.sha256(json.dumps({**common,'terminal':terminal},sort_keys=True).encode()).hexdigest()
        def normalise(sides, required, extras_allowed, counts=None):
            if not extras_allowed:
                return 'legacy_clean'
            if required == len(sides) and counts == (sides.count(0),sides.count(1)):
                return 'legacy_clean'
            return ('coverage', counts)
        l = normalise((0,0,0,1),4,False)
        e = normalise((0,0,0,1),4,True,(3,1))
        r = normalise((0,0,0,1),4,True)
        self.assertEqual(identity(l),identity(e))
        self.assertNotEqual(identity(l),identity(r))
        self.assertNotEqual(normalise((0,0,1),2,True,(2,0)), 'legacy_clean')

    def test_k_recorded_ratio_is_not_current_improvement(self):
        old=F('320800932.13201106'); new=F('524079.6986482172'); current=F('85558.70618560436')
        self.assertLess(new,old)
        self.assertGreater(new,current*6)
        self.assertGreater((old-new)/old,F('0.99'))


def main() -> int:
    ap=argparse.ArgumentParser()
    ap.add_argument('--output',type=Path)
    args=ap.parse_args()
    stream=io.StringIO()
    suite=unittest.defaultTestLoader.loadTestsFromTestCase(GoalContractTests)
    result=unittest.TextTestRunner(stream=stream,verbosity=2).run(suite)
    payload={'scope':'exact-rational and abstract contract checks; not native PoE qualification',
             'tests_run':result.testsRun,'failures':len(result.failures),'errors':len(result.errors),
             'passed':result.wasSuccessful(),'metrics':METRICS,
             'derived_recorded_k':{
                 'a5_finder_reduction_percent':float((F('320800932.13201106')-F('524079.6986482172'))/F('320800932.13201106')*100),
                 'a5_finder_multiple_of_current':float(F('524079.6986482172')/F('85558.70618560436')),
                 'a3_finder_reduction_percent':float((F('335995.68284382095')-F('14034.203062534049'))/F('335995.68284382095')*100)},
             'log':stream.getvalue()}
    if args.output:
        args.output.parent.mkdir(parents=True,exist_ok=True)
        args.output.write_text(json.dumps(payload,indent=2)+'\n',encoding='utf-8')
    print(stream.getvalue(),end='')
    print(json.dumps({k:v for k,v in payload.items() if k!='log'},indent=2))
    return 0 if result.wasSuccessful() else 1

if __name__=='__main__':
    raise SystemExit(main())
