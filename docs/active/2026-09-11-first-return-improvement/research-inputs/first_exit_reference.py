#!/usr/bin/env python3
"""Exact synthetic checks for incumbent-backed first-exit policy improvement.

This is NOT a PoE simulator, native solver, performance benchmark, or general
proof assistant. It tests finite fixed-controller composition with Fractions.
Run: python first_exit_reference.py --output first_exit_results.json
"""
from __future__ import annotations

import argparse
import itertools
import json
import random
from collections import defaultdict
from dataclasses import dataclass
from fractions import Fraction as F
from pathlib import Path
from typing import Mapping


@dataclass(frozen=True)
class Row:
    cost: F
    exits: Mapping[str, F]

    def validate(self) -> None:
        if self.cost < 0 or any(p < 0 for p in self.exits.values()):
            raise ValueError("Negative reward or probability")
        if sum(self.exits.values(), F(0)) != 1:
            raise ValueError("Incomplete probability mass")


def solve(a: list[list[F]], b: list[F]) -> list[F]:
    n = len(b)
    if len(a) != n or any(len(r) != n for r in a):
        raise ValueError("Non-square system")
    m = [list(r) + [v] for r, v in zip(a, b)]
    for j in range(n):
        k = next((k for k in range(j, n) if m[k][j]), None)
        if k is None:
            raise ValueError("Singular system: not a certified transient block")
        m[j], m[k] = m[k], m[j]
        q = m[j][j]
        m[j] = [v / q for v in m[j]]
        for i in range(n):
            if i != j and m[i][j]:
                q = m[i][j]
                m[i] = [x - q*y for x, y in zip(m[i], m[j])]
    return [r[-1] for r in m]


def reachable(rows: Mapping[str, Row], root: str, boundaries: set[str]) -> list[str]:
    seen: set[str] = set()
    pending = [root]
    while pending:
        s = pending.pop()
        if s in seen or s in boundaries:
            continue
        if s not in rows:
            raise ValueError(f"Unknown positive-mass tail: {s}")
        rows[s].validate()
        seen.add(s)
        pending.extend(t for t, p in rows[s].exits.items() if p > 0)
    return sorted(seen)


def first_exit(rows: Mapping[str, Row], root: str,
               boundaries: set[str]) -> tuple[F, dict[str, F]]:
    if root in boundaries:
        return F(0), {b: F(b == root) for b in sorted(boundaries)}
    names = reachable(rows, root, boundaries)
    # Independent topological absorption check before numerical solving.
    good = set(boundaries)
    while True:
        new = {s for s in names if any(p > 0 and t in good for t, p in rows[s].exits.items())}
        if new <= good:
            break
        good |= new
    if not set(names) <= good:
        raise ValueError("Reachable non-exiting component")
    ix = {s: i for i, s in enumerate(names)}
    a = [[F(i == j) for j in range(len(names))] for i in range(len(names))]
    for s, i in ix.items():
        for t, p in rows[s].exits.items():
            if t in ix:
                a[i][ix[t]] -= p
    cost = solve(a, [rows[s].cost for s in names])[ix[root]]
    mass = {b: solve(a, [rows[s].exits.get(b, F(0)) for s in names])[ix[root]]
            for b in sorted(boundaries)}
    if cost < 0 or any(p < 0 for p in mass.values()) or sum(mass.values(), F(0)) != 1:
        raise ValueError("Invalid stopped law")
    return cost, mass


def evaluate(rows: Mapping[str, Row], root: str, goal: str = 'goal') -> F:
    c, mass = first_exit(rows, root, {goal})
    if mass[goal] != 1:
        raise ValueError("Improper controller")
    return c


def expect_failure(fn) -> bool:
    try:
        fn()
    except ValueError:
        return True
    return False


def deletion_bridge(labels: tuple[str, ...], goals: frozenset[str],
                    domain: Mapping[frozenset[str], F], cost: F):
    """Fixed policy: stop at a true clean goal or certified domain, else delete.

    The synthetic removal law is uniform over labeled remaining elements.
    Applying this to native items requires all the plan's explicit premises.
    """
    memo: dict[frozenset[str], tuple[F, dict[frozenset[str], F]]] = {}
    def go(s: frozenset[str]):
        if s in memo:
            return memo[s]
        if s == goals or s in domain:
            out = (F(0), {s: F(1)})
        elif not s:
            raise ValueError("Empty endpoint has no committed controller")
        else:
            c = cost
            mass: dict[frozenset[str], F] = defaultdict(F)
            for label in sorted(s):
                cc, mm = go(s - {label})
                c += cc / len(s)
                for b, p in mm.items():
                    mass[b] += p / len(s)
            out = (c, dict(mass))
        memo[s] = out
        return out
    initial = frozenset(labels)
    c, mass = go(initial)
    value = c + sum((p * (F(0) if s == goals else domain[s]) for s, p in mass.items()), F(0))
    return c, mass, value, len(memo)


def deletion_order_oracle(labels, goals, domain, cost):
    totals: dict[frozenset[str], F] = defaultdict(F)
    expected_cost = F(0)
    permutations = 0
    for order in itertools.permutations(labels):
        s = frozenset(labels)
        paid = F(0)
        for label in order:
            if s == goals or s in domain:
                break
            s = s - {label}
            paid += cost
        if s != goals and s not in domain:
            raise ValueError("Oracle cannot terminate")
        totals[s] += 1
        expected_cost += paid
        permutations += 1
    return expected_cost / permutations, {s:p/permutations for s,p in totals.items()}, permutations


def main():
    checks: dict[str, bool] = {}
    def check(name, condition):
        checks[name] = bool(condition)
        if not condition:
            raise AssertionError(name)

    # A complete single-entry excursion: every failure is paid and routed.
    rows = {'trial': Row(F(1), {'goal':F(1,2), 'cleanup':F(1,2)}),
            'cleanup':Row(F(2), {'anchor':F(1)})}
    r, k = first_exit(rows, 'trial', {'anchor','goal'})
    j = F(10)
    one = r + k['anchor']*j
    repeated = r/(1-k['anchor'])
    full_one = dict(rows) | {'anchor':Row(j, {'goal':F(1)})}
    full_repeat = dict(rows) | {'anchor':Row(F(0), {'trial':F(1)})}
    check('complete_excursion_mass', sum(k.values(),F(0)) == 1)
    check('one_shot_matches_full_controller', one == evaluate(full_one,'trial') == 7)
    check('repeated_matches_full_controller', repeated == evaluate(full_repeat,'trial') == 4)
    check('improvement_amplification', j-repeated == (j-one)/(1-k['anchor']))
    check('one_shot_and_permanent_are_different', one != repeated)

    # A small one-shot gain can become material under verified recurrence.
    q = F(99,100); r2 = F(1,2); base = F(100)
    check('small_advantage_large_total_gain', r2+q*base == F(199,2) and r2/(1-q) == 50)
    check('finite_attempt_formula', sum((q**i*r2 for i in range(5)),F(0))+q**5*base ==
          r2*(1-q**5)/(1-q)+q**5*base)
    check('small_gain_not_small_effect', base-(r2+q*base) == F(1,2) and base-r2/(1-q)==50)

    cycle = {'trial':Row(F(1),{'cleanup':F(1)}), 'cleanup':Row(F(1),{'anchor':F(1)})}
    check('one_shot_can_be_proper_when_repetition_is_not', evaluate(cycle|{'anchor':Row(F(10),{'goal':F(1)})},'trial')==12)
    check('repetition_without_goal_escape_is_refused', expect_failure(lambda: evaluate(cycle|{'anchor':Row(F(0),{'trial':F(1)})},'trial')))
    zero = {'z':Row(F(0),{'z':F(1)})}
    check('zero_cost_equation_is_not_properness', expect_failure(lambda:evaluate(zero,'z')))
    unknown = {'x':Row(F(1),{'goal':F(99,100),'missing':F(1,100)})}
    check('rare_unknown_tail_is_not_success', expect_failure(lambda:evaluate(unknown,'x')))
    trap = unknown | {'missing':Row(F(0), {'missing':F(1)})}
    check('rare_trap_is_not_removed', expect_failure(lambda:evaluate(trap,'x')))
    check('incomplete_mass_refused', expect_failure(lambda:evaluate({'s':Row(F(1),{'goal':F(9,10)})},'s')))
    check('unreachable_bad_component_does_not_poison_entry', evaluate({'s':Row(F(3),{'goal':F(1)}),'z':zero['z']},'s')==3)
    eps=F(1,10**12)
    check('tiny_escape_requires_large_cost', evaluate({'s':Row(F(1),{'s':1-eps,'goal':eps})},'s')==10**12)

    # Two offers: post-observation decisions differ. A single precommitted
    # ordinal is not equivalent, even though each offer has the same costs.
    offers=[(F(0),F(8)),(F(8),F(0))]
    check('observed_decisions_preserved', sum(map(min,offers),F(0))/2 == 0 and
          min(sum(o[i] for o in offers)/2 for i in range(2))==4)

    target=frozenset({'A','B'})
    domain={frozenset():F(100),frozenset({'A'}):F(20),frozenset({'B'}):F(30)}
    bc,bm,bv,states=deletion_bridge(('A','B','junk'),target,domain,F(1))
    oc,om,orders=deletion_order_oracle(('A','B','junk'),target,domain,F(1))
    check('dirty_full_goal_is_not_terminal', bv > 0)
    check('deletion_exact_value', bv==F(131,3))
    check('deletion_orders_and_dp_match', bc==oc and bm==om)
    check('all_goal_loss_exits_retained', frozenset() in bm and bm[frozenset()]>0)
    check('subset_bound_not_order_tree', states<=2**3)
    check('real_cleanup_not_free_reset', bc>0 and bv!=domain[frozenset()])
    check('uncertified_empty_domain_refused', expect_failure(lambda: deletion_bridge(('junk',),target,{},F(1))))
    # Native-facing guard counterexamples are recorded as required premises,
    # not pretended C++ tests; the reference does not model locks/fractures.

    known=F(40); p=F(1,100); desired=F(80)
    threshold=(desired-known)/p
    check('one_tail_budget', threshold==4000 and known+p*threshold==desired)
    check('rare_mass_can_dominate_cost', known+p*F(10000)==140)
    poor_tail = {'s':Row(F(1),{'q':F(1)}),'q':Row(F(11),{'goal':F(1)})}
    better_tail = dict(poor_tail) | {'q':Row(F(2),{'goal':F(1)})}
    check('nonimproving_bridge_does_not_retire_native_action', evaluate(poor_tail,'s')==12 and evaluate(better_tail,'s')==3)
    check('candidate_subset_not_lower_authority', evaluate(poor_tail,'s')>evaluate(better_tail,'s'))

    # Exact randomized transient blocks; compare compressed law against a
    # separately assembled complete one-shot and repeated controller.
    rng=random.Random(20260911)
    models=256
    for trial in range(models):
        n=rng.randint(1,5)
        rr={}
        for i in range(n):
            weights=[rng.randrange(5) for _ in range(n)]+[rng.randint(1,5),rng.randint(1,5)]
            den=sum(weights)
            names=[f'x{j}' for j in range(n)]+['anchor','goal']
            rr[f'x{i}']=Row(F(rng.randint(0,20),rng.randint(1,5)),
                            {t:F(w,den) for t,w in zip(names,weights) if w})
        c, mass=first_exit(rr,'x0',{'anchor','goal'})
        J=F(rng.randint(1,200),rng.randint(1,5))
        q=mass['anchor']
        if q>=1:
            raise AssertionError('Generated transient block lost positive goal escape')
        JO=evaluate(rr|{'anchor':Row(J,{'goal':F(1)})},'x0')
        JR=evaluate(rr|{'anchor':Row(F(0),{'x0':F(1)})},'x0')
        if JO!=c+q*J or JR!=c/(1-q) or J-JR!=(J-JO)/(1-q):
            raise AssertionError(f'First-return identity model {trial}')
        if (JO<J)!=(JR<J):
            raise AssertionError('Strict advantage equivalence')
    check('randomized_first_exit_full_models_agree', True)

    deletion_cases=48
    permutation_total=0
    for _ in range(deletion_cases):
        n=rng.randint(2,6)
        labels=tuple('ABCDEF'[:n]); goals=frozenset(labels[:2])
        dom={frozenset():F(rng.randint(20,200))}
        for label in labels:
            if rng.randrange(2): dom[frozenset({label})]=F(rng.randint(1,100))
        cost=F(rng.randint(1,8),rng.randint(1,3))
        c,m,v,count=deletion_bridge(labels,goals,dom,cost)
        c2,m2,perms=deletion_order_oracle(labels,goals,dom,cost)
        permutation_total+=perms
        if c!=c2 or m!=m2 or count>2**n:
            raise AssertionError('Deletion bridge oracle mismatch')
    check('randomized_deletion_permutation_oracles_agree', True)
    return {
        'classification':'exact synthetic controller checks; no native engine, PoE simulation or performance measurement',
        'reviewed_source':'84f02ee3b603fe3879ac2c4a885d1b81ba67772f',
        'named_checks':checks, 'named_check_count':len(checks),
        'random_seed':20260911,
        'transient_models_compared_with_full_controllers':models,
        'deletion_models_compared_with_full_orders':deletion_cases,
        'deletion_orders_enumerated':permutation_total,
        'example':{'old_cost':'10','excursion_cost':'2','return_probability':'1/2',
                   'one_shot_cost':str(one),'repeated_cost':str(repeated)},
        'amplification_example':{'old_cost':'100','excursion_cost':'1/2','return_probability':'99/100',
                                 'one_shot_cost':'199/2','repeated_cost':'50'},
        'deletion_example':{'internal_expected_cost':str(bc),'composed_cost':str(bv),
                            'visited_subsets':states,'exit_mass':{','.join(sorted(k)) or 'empty':str(v) for k,v in bm.items()}},
        'limits':[
            'The report supplies the general argument; finite examples are not a machine-checked theorem.',
            'Deletion examples use a synthetic uniform law; current native legality, rarity, locks, fractures, control and routing still need tests.',
            'A policy-specific first-exit value supplies an upper candidate, never a native lower or all-action optimality.',
            'No improvement of actual Ring/Amulet controllers or resource use is established here.'
        ]
    }


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path)
    args=parser.parse_args()
    result=main()
    text=json.dumps(result,indent=2)+'\n'
    if args.output:
        args.output.parent.mkdir(parents=True,exist_ok=True)
        args.output.write_text(text,encoding='utf-8')
    print(text)
