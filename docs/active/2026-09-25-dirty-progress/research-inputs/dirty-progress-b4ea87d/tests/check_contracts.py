"""Finite exact examples for the planning arguments; not a native PoE model.

Run: python tests/check_contracts.py --output tests/results.json
Every probability/cost below is an invented rational fixture. The scheduler
models deliberately mirror only the ordering relations cited in the review;
they do not execute repository code.
"""
from __future__ import annotations
import argparse
import json
import random
import unittest
from fractions import Fraction as F
from pathlib import Path


def solve(a: list[list[F]], b: list[F]) -> list[F]:
    n = len(b)
    if len(a) != n or any(len(row) != n for row in a):
        raise ValueError('square system required')
    m = [[F(x) for x in row] + [F(y)] for row, y in zip(a, b)]
    for j in range(n):
        pivot = next((i for i in range(j, n) if m[i][j]), None)
        if pivot is None:
            raise ValueError('singular system; finite proper value not established')
        m[j], m[pivot] = m[pivot], m[j]
        p = m[j][j]
        m[j] = [x / p for x in m[j]]
        for i in range(n):
            if i != j:
                c = m[i][j]
                m[i] = [u - c*v for u,v in zip(m[i],m[j])]
    return [row[-1] for row in m]


def values(q: list[list[F]], c: list[F]) -> list[F]:
    if any(any(x < 0 for x in row) or sum(row) > 1 for row in q):
        raise ValueError('nonnegative substochastic rows required')
    return solve([[F(i == j) - q[i][j] for j in range(len(q))]
                  for i in range(len(q))], c)


def first_cut(q: list[list[F]], c: list[F], root: int, cut: set[int]):
    """Goal absorption is implicit. cut contains observed nonterminal coverage."""
    v = values(q, c)
    if root in cut:
        mu = {i: F(i == root) for i in cut}
        return F(0), v[root], mu
    outside = [i for i in range(len(q)) if i not in cut]
    k = outside.index(root)
    a = [[F(i == j) - q[s][t] for j,t in enumerate(outside)]
         for i,s in enumerate(outside)]
    d = solve([list(col) for col in zip(*a)],
              [F(i == k) for i in range(len(outside))])
    mu = {t: sum(d[i]*q[s][t] for i,s in enumerate(outside)) for t in cut}
    pre = sum(d[i]*c[s] for i,s in enumerate(outside))
    post = sum(mu[t]*v[t] for t in cut)
    return pre, post, mu


def stopped_value(q: list[list[F]], c: list[F], root: int, cut: set[int]):
    qq=[row[:] for row in q]; cc=c[:]
    for i in cut:
        qq[i]=[F(0)]*len(q); cc[i]=F(0)
    return values(qq, cc)[root]


def debt(m: int, g: int, k: int, rarity_mismatch: int = 0, terminal: bool = False):
    if not 0 <= g <= k:
        raise ValueError('fixture assumes distinct satisfying occupied goals')
    return 0 if terminal else max(1, max(m-g, 0) + k-g + rarity_mismatch)


def bucket_order(candidates, neutral_extra=False, focused=False):
    """One simple complete-mask round-robin epoch, no native solver invocation."""
    groups={}
    for x in candidates:
        groups.setdefault(x['mask'],[]).append(x)
    if not focused:
        for xs in groups.values():
            xs.sort(key=lambda x: (-x.get('fracture',0), -x.get('protection',0),
                                   0 if neutral_extra else x['extra'], x['id']))
    masks=sorted(groups,key=lambda m: (-m.bit_count(),m))
    result=[]; depth=0
    while True:
        layer=[groups[m][depth]['id'] for m in masks if depth<len(groups[m])]
        if not layer: return result
        result += layer; depth += 1


class Contracts(unittest.TestCase):
    def test_four_goal_debt_tie(self):
        self.assertEqual(debt(4,0,0),debt(4,3,6))
        self.assertEqual(debt(4,3,6),4)

    def test_five_goal_debt_tie(self):
        self.assertEqual(debt(5,0,0),debt(5,3,6))

    def test_terminal_and_dirty_coverage_not_same(self):
        self.assertEqual(debt(4,4,4,terminal=True),0)
        self.assertEqual(debt(4,4,6),2)

    def test_simplification_requires_count_assumption(self):
        self.assertEqual(debt(2,4,6),2)
        self.assertNotEqual(debt(2,4,6),max(1,2+6-2*4))

    def test_actual_subset_priority_not_debt_tie(self):
        xs=[dict(id=0,mask=0,extra=0),dict(id=1,mask=7,extra=3)]
        self.assertEqual(bucket_order(xs),[1,0])

    def test_round_robin_is_not_all_high_progress_first(self):
        xs=[dict(id=1,mask=7,extra=3),dict(id=2,mask=7,extra=2),
            dict(id=3,mask=0,extra=0)]
        self.assertEqual(bucket_order(xs),[2,3,1])

    def test_neutral_extra_is_within_mask_only(self):
        xs=[dict(id=1,mask=7,extra=3),dict(id=2,mask=7,extra=0),
            dict(id=3,mask=0,extra=0)]
        self.assertEqual(bucket_order(xs),[2,3,1])
        self.assertEqual(bucket_order(xs,neutral_extra=True),[1,3,2])
        self.assertEqual(set(bucket_order(xs)),set(bucket_order(xs,neutral_extra=True)))

    def test_prior_fracture_precedence_preserved(self):
        xs=[dict(id=1,mask=7,extra=0),dict(id=2,mask=7,extra=3,fracture=1)]
        self.assertEqual(bucket_order(xs),[2,1])
        self.assertEqual(bucket_order(xs,neutral_extra=True),[2,1])

    def test_focused_preserves_supplied_priority_order(self):
        xs=[dict(id=7,mask=7,extra=3),dict(id=2,mask=7,extra=0)]
        self.assertEqual(bucket_order(xs,focused=True),[7,2])
        self.assertEqual(bucket_order(xs,focused=True,neutral_extra=True),[7,2])

    def test_acquisition_union_preserves_old_cleanup_events(self):
        outcomes=[(F(1,4),True,False),(F(1,3),False,True),
                  (F(1,6),True,True),(F(1,4),False,False)]
        old=sum(p for p,d,g in outcomes if d)
        new=sum(p for p,d,g in outcomes if d or g)
        self.assertEqual(sum(p for p,_,_ in outcomes),1)
        self.assertEqual(old,F(5,12)); self.assertEqual(new,F(3,4))
        self.assertLessEqual(new,1)

    def test_gate_output_candidate_is_not_verified_artifact(self):
        has_output=True; has_verified=False
        first_policy=not has_output
        self.assertFalse(first_policy)
        self.assertTrue(not has_verified)

    def test_first_cut_loss_and_reacquisition(self):
        q=[[F(0),F(1),F(0)],[F(0),F(0),F(1,2)],
           [F(0),F(1),F(0)]]
        c=[F(100),F(1),F(10)]
        pre,post,mu=first_cut(q,c,0,{1})
        self.assertEqual((pre,post,mu[1]),(F(100),F(12),F(1)))
        self.assertEqual(values(q,c)[0],112)
        self.assertEqual(stopped_value(q,c,0,{1}),pre)

    def test_repeated_occupancy_overcounts_tail(self):
        q=[[F(0),F(1),F(0)],[F(0),F(0),F(1,2)],
           [F(0),F(1),F(0)]]
        c=[F(100),F(1),F(10)]; n=len(q)
        d=solve([[F(i==j)-q[j][i] for j in range(n)] for i in range(n)],
                [F(1),F(0),F(0)])
        self.assertEqual(d[1],2)
        self.assertEqual(d[1]*values(q,c)[1],24)
        self.assertNotEqual(d[1]*values(q,c)[1],first_cut(q,c,0,{1})[1])

    def test_dirty_probability_is_not_conditional_tail(self):
        q=[[F(0),F(1,4)],[F(0),F(0)]]; c=[F(2),F(8)]
        pre,post,mu=first_cut(q,c,0,{1})
        self.assertEqual((pre,post,mu[1]),(F(2),F(2),F(1,4)))
        self.assertEqual(post/mu[1],8)

    def test_root_already_covered(self):
        q=[[F(1,2)]];c=[F(3)]
        self.assertEqual(first_cut(q,c,0,{0})[:2],(F(0),F(6)))
        self.assertEqual(stopped_value(q,c,0,{0}),0)

    def test_no_dirty_hit_post_cost_zero(self):
        q=[[F(0),F(0)],[F(0),F(0)]];c=[F(9),F(500)]
        self.assertEqual(first_cut(q,c,0,{1})[1],0)

    def test_zero_full_coverage_tail_does_not_rule_out_partial_loss(self):
        # Both complete only at goal (implicit absorption), but one may waste
        # arbitrary money before that event. First-full-coverage post cost=0.
        q=[[F(0),F(1)],[F(0),F(0)]]
        c_bad=[F(100),F(100)];c_good=[F(100),F(1)]
        self.assertEqual(first_cut(q,c_bad,0,set())[1],0)
        self.assertEqual(first_cut(q,c_good,0,set())[1],0)
        self.assertEqual(values(q,c_bad)[0]-values(q,c_good)[0],99)

    def test_same_counts_cannot_supply_cleanup_value(self):
        good=values([[F(0)]],[F(1)])[0]
        bad=values([[F(99,100)]],[F(1)])[0]
        self.assertEqual((good,bad),(F(1),F(100)))
        self.assertEqual(debt(4,3,6),debt(4,3,6))

    def test_one_shot_vs_recurring(self):
        old=F(10); once=F(2)+F(1,2)*old
        repeated=F(2)/(1-F(1,2))
        self.assertEqual((once,repeated),(F(7),F(4)))

    def test_selected_complete_mass_not_success_only(self):
        q=[[F(9,10)]]; c=[F(1)]
        self.assertEqual(values(q,c)[0],10)
        self.assertNotEqual(values(q,c)[0],1)

    def test_relaxed_stop_not_valid_original_controller(self):
        q=[[F(0),F(1)],[F(0),F(0)]];c=[F(1),F(50)]
        self.assertEqual(stopped_value(q,c,0,{1}),1)
        self.assertEqual(values(q,c)[0],51)

    def test_closed_component_refused_by_exact_system(self):
        with self.assertRaises(ValueError):
            values([[F(1)]],[F(1)])

    def test_coverage_inside_program_is_different_boundary(self):
        q=[[F(0),F(1),F(0)],[F(0),F(0),F(1)],
           [F(0),F(0),F(0)]];c=[F(2),F(7),F(11)]
        early=first_cut(q,c,0,{1})
        complete_program=first_cut(q,c,0,{2})
        self.assertEqual((early[0],complete_program[0]),(F(2),F(9)))
        self.assertEqual(early[0]+early[1],complete_program[0]+complete_program[1])

    def test_generated_first_hit_identities(self):
        rng=random.Random(20260925)
        for _ in range(64):
            n=4
            q=[]
            for _row in range(n):
                weights=[rng.randint(0,4) for _ in range(n)]
                total=sum(weights)+rng.randint(1,6)
                q.append([F(x,total) for x in weights])
            c=[F(rng.randint(0,20)) for _ in range(n)]
            root=rng.randrange(n)
            cut={i for i in range(n) if rng.choice([False,True])}
            pre,post,mu=first_cut(q,c,root,cut)
            self.assertEqual(pre+post,values(q,c)[root])
            self.assertEqual(pre,stopped_value(q,c,root,cut))
            self.assertGreaterEqual(post,0)
            self.assertLessEqual(sum(mu.values()),1)


def main():
    ap=argparse.ArgumentParser();ap.add_argument('--output',type=Path)
    args=ap.parse_args()
    suite=unittest.defaultTestLoader.loadTestsFromTestCase(Contracts)
    run=unittest.TextTestRunner(verbosity=2).run(suite)
    record={'kind':'exact_rational_abstract_contract_tests_not_native',
            'tests_run':run.testsRun,'failures':len(run.failures),
            'errors':len(run.errors),'passed':run.wasSuccessful(),
            'generated_models':64,'identities_per_generated_model':4,
            'native_tests_run':False,'performance_claim':None}
    if args.output:
        args.output.parent.mkdir(parents=True,exist_ok=True)
        args.output.write_text(json.dumps(record,indent=2)+'\n',encoding='utf-8')
    raise SystemExit(0 if run.wasSuccessful() else 1)

if __name__=='__main__':main()
