#!/usr/bin/env python3
"""Exact-rational toy contract checks, NOT native poecraft2 tests or mechanics.

The proposal supplies only action choices. A trusted toy model owns costs,
probabilities, goal truth and allowed actions. This illustrates the proposed
acceptance separation; the production C++/ABI/compiler still needs its tests.
"""
from __future__ import annotations
from dataclasses import dataclass, replace
from fractions import Fraction as F
from pathlib import Path
from typing import Mapping
import argparse
import hashlib
import io
import json
import math
import unittest

class Refused(ValueError):
    pass

@dataclass(frozen=True)
class Row:
    cost: F
    outcomes: tuple[tuple[str, F], ...]

@dataclass(frozen=True)
class Problem:
    identity: str
    goal_identity: str
    root: str
    goals: frozenset[str]
    rows: Mapping[tuple[str, str], Row]
    allowed: frozenset[str]

@dataclass(frozen=True)
class Proposal:
    problem_identity: str
    goal_identity: str
    root: str
    choices: Mapping[str, str]
    declared_success: frozenset[str] = frozenset()
    predicted_cost: float = 0.0

@dataclass(frozen=True)
class Receipt:
    problem_identity: str
    candidate_hash: str
    cost: F


def solve_exact(a: list[list[F]], b: list[F]) -> list[F]:
    n = len(b)
    rows = [a[i][:] + [b[i]] for i in range(n)]
    for c in range(n):
        pivot = next((r for r in range(c, n) if rows[r][c]), None)
        if pivot is None:
            raise Refused('nontransient_or_singular')
        rows[c], rows[pivot] = rows[pivot], rows[c]
        q = rows[c][c]
        rows[c] = [v / q for v in rows[c]]
        for r in range(n):
            if r != c and rows[r][c]:
                q = rows[r][c]
                rows[r] = [x - q*y for x, y in zip(rows[r], rows[c])]
    return [r[-1] for r in rows]


def check(problem: Problem, candidate: Proposal) -> Receipt:
    if (candidate.problem_identity, candidate.goal_identity, candidate.root) != (
        problem.identity, problem.goal_identity, problem.root
    ):
        raise Refused('request_binding')
    seen: set[str] = set()
    pending = [problem.root]
    actual: dict[str, Row] = {}
    while pending:
        s = pending.pop()
        if s in seen:
            continue
        seen.add(s)
        if s in candidate.declared_success and s not in problem.goals:
            raise Refused('false_success')
        if s in problem.goals:
            continue
        action = candidate.choices.get(s)
        if action is None:
            raise Refused('uncovered_outcome')
        if action not in problem.allowed:
            raise Refused('scope')
        row = problem.rows.get((s, action))
        if row is None:
            raise Refused('illegal_action')
        if row.cost < 0 or any(p < 0 for _, p in row.outcomes):
            raise Refused('bad_model')
        if sum((p for _, p in row.outcomes), F(0)) != 1:
            raise Refused('incomplete_mass')
        actual[s] = row
        pending.extend(t for t, p in row.outcomes if p > 0)
    # In a finite fixed Markov chain, each reached nonterminal must have a
    # positive-support path to a goal. This excludes all reachable closed traps.
    can_finish = seen & set(problem.goals)
    while True:
        expanded = can_finish | {
            s for s, row in actual.items()
            if any(p > 0 and t in can_finish for t, p in row.outcomes)
        }
        if expanded == can_finish:
            break
        can_finish = expanded
    if seen - can_finish:
        raise Refused('improper_policy')
    states = sorted(actual)
    index = {s: i for i, s in enumerate(states)}
    a = [[F(int(i == j)) for j in range(len(states))]
         for i in range(len(states))]
    b = [actual[s].cost for s in states]
    for s, row in actual.items():
        for t, p in row.outcomes:
            if t in index:
                a[index[s]][index[t]] -= p
    values = solve_exact(a, b) if states else []
    cost = values[index[problem.root]] if problem.root in index else F(0)
    payload = json.dumps({
        'problem': problem.identity, 'goal': problem.goal_identity,
        'root': candidate.root, 'choices': sorted(candidate.choices.items()),
        'success': sorted(candidate.declared_success),
    }, separators=(',', ':'), sort_keys=True).encode()
    return Receipt(problem.identity, hashlib.sha256(payload).hexdigest(), cost)


def retry(p: F, cost: F = F(1)) -> tuple[Problem, Proposal]:
    problem = Problem(f'retry/{p}/{cost}', 'native-goal', 'r', frozenset({'g'}), {
        ('r', 'retry'): Row(cost, (('r', 1-p), ('g', p))),
        ('r', 'finish'): Row(F(20), (('g', F(1)),)),
    }, frozenset({'retry', 'finish'}))
    return problem, Proposal(problem.identity, problem.goal_identity,
                             problem.root, {'r': 'retry'})


def strategic_label(status: str, cost: F | None) -> F | None:
    return cost if status == 'complete_checked' else None

class Contracts(unittest.TestCase):
    def test_native_law_not_predicted_cost(self):
        p, c = retry(F(1, 10))
        self.assertEqual(check(p, replace(c, predicted_cost=-1000)).cost, 10)

    def test_self_declared_success_is_not_original_goal(self):
        p, c = retry(F(1, 10))
        with self.assertRaisesRegex(Refused, 'false_success'):
            check(p, replace(c, choices={}, declared_success=frozenset({'r'})))
        with self.assertRaisesRegex(Refused, 'request_binding'):
            check(p, replace(c, goal_identity='easier-goal'))

    def test_uncovered_chance_outcome_not_prunable(self):
        p, c = retry(F(1, 10))
        row = Row(F(1), (('g', F(999,1000)), ('new', F(1,1000))))
        p = replace(p, rows={('r','retry'): row})
        with self.assertRaisesRegex(Refused, 'uncovered_outcome'):
            check(p, c)

    def test_rare_improper_trap_is_rejected(self):
        p, c = retry(F(1, 10))
        rows = {
            ('r','retry'): Row(F(1), (('g', F(999999,1000000)), ('trap', F(1,1000000)))),
            ('trap','retry'): Row(F(0), (('trap', F(1)),)),
        }
        with self.assertRaisesRegex(Refused, 'improper_policy'):
            check(replace(p, rows=rows), replace(c, choices={'r':'retry','trap':'retry'}))

    def test_unequal_role_bindings_can_choose_differently(self):
        values = []
        for probability in (F(1,5), F(1,100)):
            p, c = retry(probability)
            values.append((check(p,c).cost,
                           check(p,replace(c,choices={'r':'finish'})).cost))
        self.assertEqual(values, [(F(5), F(20)), (F(100), F(20))])
        self.assertNotEqual(min(range(2), key=lambda i: values[0][i]),
                            min(range(2), key=lambda i: values[1][i]))

    def test_bookkeeping_permutation_preserves_bound_choice(self):
        roles = [('cold', F(1,100)), ('fire', F(1,5)), ('armour', F(1,50))]
        rank = lambda xs: [name for name, p in sorted(xs, key=lambda x: (-x[1],x[0]))]
        self.assertEqual(rank(roles), rank(list(reversed(roles))))
        changed = [('cold', F(1,2)), roles[1], roles[2]]
        self.assertNotEqual(rank(roles), rank(changed))

    def test_complementary_decisions(self):
        p = Problem('joint','g','r',frozenset({'g'}),{
            ('r','direct'):Row(F(10),(('g',F(1)),)),
            ('r','bridge'):Row(F(1),(('t',F(1)),)),
            ('t','old'):Row(F(100),(('g',F(1)),)),
            ('t','new'):Row(F(1),(('g',F(1)),)),
        },frozenset({'direct','bridge','old','new'}))
        vals=[]
        for ra,ta in [('direct','old'),('bridge','old'),('direct','new'),('bridge','new')]:
            vals.append(check(p,Proposal('joint','g','r',{'r':ra,'t':ta})).cost)
        self.assertEqual(vals,[F(10),F(101),F(10),F(2)])

    def test_finite_stage_and_recurring_replacement_differ(self):
        p, c = retry(F(1,10))
        old = check(p,c).cost
        one_use = F(2) + F(1,2)*old
        recurrent_problem, recurrent = retry(F(1,2),F(2))
        self.assertEqual((old,one_use,check(recurrent_problem,recurrent).cost),
                         (F(10),F(7),F(4)))

    def test_censored_check_is_not_cost_label(self):
        for status in ['cap','timeout','cancel','unknown','unsupported']:
            self.assertIsNone(strategic_label(status,F(123)))
        self.assertEqual(strategic_label('complete_checked',F(123)),F(123))

    def test_failed_candidate_does_not_erase_checked_winner(self):
        p,c=retry(F(1,10)); winner=check(p,c)
        try:
            trial=check(p,replace(c,declared_success=frozenset({'r'})))
        except Refused:
            trial=None
        if trial is not None and trial.cost < winner.cost:
            winner=trial
        self.assertEqual(winner.cost,F(10))

    def test_scope_and_reprice_identity(self):
        p,c=retry(F(1,10))
        with self.assertRaisesRegex(Refused,'scope'):
            check(replace(p,allowed=frozenset({'finish'})),c)
        p2,_=retry(F(1,10),F(2))
        with self.assertRaisesRegex(Refused,'request_binding'):
            check(p2,c)

    def test_incomplete_mass_is_not_renormalized(self):
        p,c=retry(F(1,10))
        with self.assertRaisesRegex(Refused,'incomplete_mass'):
            check(replace(p,rows={('r','retry'):Row(F(1),(('g',F(9,10)),))}),c)


def main() -> int:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path(__file__).with_name('results.json'))
    args=parser.parse_args()
    stream=io.StringIO()
    result=unittest.TextTestRunner(stream=stream,verbosity=2).run(
        unittest.defaultTestLoader.loadTestsFromTestCase(Contracts))
    result_json={
        'scope':'Exact-rational toy interface/model contracts; NOT native solver or performance qualification.',
        'tests_run':result.testsRun,'failures':len(result.failures),
        'errors':len(result.errors),'skipped':len(result.skipped),'passed':result.wasSuccessful(),
        'illustrations':{
            'native_predicted_cost_ignored_value':'10',
            'unequal_retry_values':['5','100'],
            'complementary_root_costs':['10','101','10','2'],
            'old_one_use_recurrent':['10','7','4'],
            'rare_trap_probability':'1/1000000',
            'probability_of_missing_rare_event_in_10000_iid_draws_approx':
                math.exp(10000*math.log1p(-1e-6)),
        },
        'test_log':stream.getvalue(),
    }
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(result_json,indent=2)+'\n',encoding='utf-8')
    print(stream.getvalue(),end='')
    return 0 if result.wasSuccessful() else 1

if __name__=='__main__':
    raise SystemExit(main())
