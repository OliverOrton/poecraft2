#!/usr/bin/env python3
"""Read-only research reference: qualitative proper seeds on tiny finite SSPs.

Synthetic, exact-arithmetic mathematics, NOT poecraft2 native tests or timings.
Uses a nested fixed point with factored post-observation choices. Exhaustive
stationary-controller enumeration is used only as an independent tiny oracle.
No game mechanics, private network access, or external dependencies.
"""
from __future__ import annotations
import argparse
import itertools
import json
import random
from dataclasses import dataclass
from fractions import Fraction as F
from pathlib import Path
from typing import Mapping, Sequence

GOAL = 0

@dataclass(frozen=True)
class Row:
    name: str
    cost: F
    direct: tuple[tuple[int, F], ...]
    offers: tuple[tuple[F, tuple[int, ...]], ...] = ()

    def validate(self, domain: set[int]) -> None:
        if self.cost < 0:
            raise ValueError('negative cost')
        if any(p < 0 or t not in domain for t, p in self.direct):
            raise ValueError('invalid direct outcome')
        if any(p < 0 or not choices or any(t not in domain for t in choices)
               for p, choices in self.offers):
            raise ValueError('invalid offer')
        if sum((p for _, p in self.direct), F()) + sum((p for p, _ in self.offers), F()) != 1:
            raise ValueError('incomplete probability mass')

@dataclass(frozen=True)
class Decision:
    row: Row
    # Choice indices are offer-local. A state id alone does not replace them.
    choice_indices: tuple[int, ...]

    def law(self) -> dict[int, F]:
        if len(self.choice_indices) != len(self.row.offers):
            raise ValueError('offer decision count')
        law: dict[int, F] = {}
        for t, p in self.row.direct:
            law[t] = law.get(t, F()) + p
        for (p, choices), chosen in zip(self.row.offers, self.choice_indices):
            if not 0 <= chosen < len(choices):
                raise ValueError('choice outside its actual offer')
            t = choices[chosen]
            law[t] = law.get(t, F()) + p
        return {t: p for t, p in law.items() if p > 0}

Model = Mapping[int, Sequence[Row]]

def domain_of(model: Model, goals: set[int]) -> set[int]:
    domain = set(model) | goals
    for rows in model.values():
        for row in rows:
            row.validate(domain)
    return domain


def safe_progress(row: Row, y: set[int], x: set[int]) -> Decision | None:
    """Existential action; all stochastic outcomes safe; post-offer decisions.

    Need positive probability of entering X and probability one of remaining Y.
    This avoids enumerating the Cartesian product of observed choices.
    """
    direct = {t for t, p in row.direct if p > 0}
    if not direct <= y:
        return None
    progress = bool(direct & x)
    picks: list[int] = []
    for p, offer in row.offers:
        if not p:
            picks.append(0)
            continue
        safe = [i for i, t in enumerate(offer) if t in y]
        if not safe:
            return None
        advancing = [i for i in safe if offer[i] in x]
        progress |= bool(advancing)
        picks.append((advancing or safe)[0])
    return Decision(row, tuple(picks)) if progress else None


def synthesize(model: Model, goals: set[int] = {GOAL}):
    """nu Y . mu X . (G union APre(Y,X)); conservative complete-row view."""
    y = domain_of(model, goals)
    outer = 0
    scans = 0
    while True:
        outer += 1
        x = goals & y
        rank = {g: 0 for g in x}
        selected: dict[int, Decision] = {}
        k = 0
        while True:
            batch: dict[int, Decision] = {}
            for s in sorted(y - x):
                for row in model.get(s, ()):
                    scans += 1
                    decision = safe_progress(row, y, x)
                    if decision is not None:
                        batch[s] = decision
                        break
            if not batch:
                break
            k += 1
            for s, decision in batch.items():
                rank[s] = k
                selected[s] = decision
            x.update(batch)
        if x == y:
            return x, selected, rank, {'outer_rounds': outer, 'row_scans': scans}
        y = x


def verify_rank(selected: Mapping[int, Decision], rank: Mapping[int, int],
                winning: set[int], goals: set[int] = {GOAL}) -> bool:
    for s in winning - goals:
        if s not in selected or s not in rank:
            return False
        support = set(selected[s].law())
        if not support <= winning or not any(rank[t] < rank[s] for t in support):
            return False
    return all(rank.get(g) == 0 for g in goals & winning)


def policy_winning(selected: Mapping[int, Decision], domain: set[int],
                   goals: set[int] = {GOAL}) -> set[int]:
    """Independent fixed-controller graph check. Missing decision is a trap."""
    adjacency = {s: ({s} if s not in selected else set(selected[s].law()))
                 for s in domain - goals}
    adjacency.update({g: {g} for g in goals})
    reaching = set(goals)
    while True:
        added = {s for s in domain - reaching if adjacency[s] & reaching}
        if not added:
            break
        reaching.update(added)
    winning = set()
    for s in domain:
        reached = {s}
        stack = [s]
        while stack:
            for t in adjacency[stack.pop()]:
                if t not in reached:
                    reached.add(t)
                    stack.append(t)
        if reached <= reaching:
            winning.add(s)
    return winning


def all_decisions(row: Row):
    for choices in itertools.product(*(range(len(offer)) for _, offer in row.offers)):
        yield Decision(row, tuple(choices))


def exhaustive_winning(model: Model, goals: set[int] = {GOAL}):
    domain = domain_of(model, goals)
    states = sorted(domain - goals)
    candidate_sets = []
    for s in states:
        choices = [d for row in model.get(s, ()) for d in all_decisions(row)]
        if not choices:
            choices = [Decision(Row('missing-as-trap', F(0), ((s, F(1)),)), ())]
        candidate_sets.append(choices)
    winning = set(goals)
    checked = 0
    for decisions in itertools.product(*candidate_sets):
        checked += 1
        winning.update(policy_winning(dict(zip(states, decisions)), domain, goals))
    return winning, checked


def exact_cost(selected: Mapping[int, Decision], root: int, goals: set[int] = {GOAL}):
    """Entry-scoped properness then exact rational Gaussian elimination."""
    reached = {root}
    stack = [root]
    while stack:
        s = stack.pop()
        if s in goals:
            continue
        if s not in selected:
            return None
        for t in selected[s].law():
            if t not in reached:
                reached.add(t)
                stack.append(t)
    if root not in policy_winning(selected, reached | goals, goals):
        return None
    states = sorted(reached - goals)
    pos = {s: i for i, s in enumerate(states)}
    a = [[F(i == j) for j in range(len(states))] + [selected[s].row.cost]
         for i, s in enumerate(states)]
    for i, s in enumerate(states):
        for t, p in selected[s].law().items():
            if t in pos:
                a[i][pos[t]] -= p
    n = len(states)
    for col in range(n):
        pivot = next((r for r in range(col, n) if a[r][col]), None)
        if pivot is None:
            return None
        a[col], a[pivot] = a[pivot], a[col]
        value = a[col][col]
        a[col] = [v / value for v in a[col]]
        for r in range(n):
            if r != col and a[r][col]:
                value = a[r][col]
                a[r] = [x - value*y for x, y in zip(a[r], a[col])]
    return F(0) if root in goals else a[pos[root]][-1]


def strict_rank_then_first(model: Model, goals: set[int] = {GOAL}):
    """Faithful restricted illustration of current initializer on no-offer rows.

    NOT native execution. Self loops allowed; every other selected successor must
    be an already assigned state/goal. Residual states receive their first row.
    """
    if any(row.offers for rows in model.values() for row in rows):
        raise ValueError('old-helper illustration is no-offer only')
    assigned = set(goals)
    selected = {}
    changed = True
    while changed:
        changed = False
        for s, rows in model.items():
            if s in assigned:
                continue
            for row in rows:
                support = {t for t, p in row.direct if p > 0}
                if support - {s} <= assigned and support & assigned:
                    selected[s] = Decision(row, ())
                    assigned.add(s)
                    changed = True
                    break
    for s, rows in model.items():
        if s not in selected and rows:
            selected[s] = Decision(rows[0], ())
    return selected, assigned


def old_closed_scc_repair_candidates(model: Model, component: set[int]):
    """Current repair's relevant restriction with all SCC values set to infinity.

    Any non-owner in-SCC successor prevents a finite row-local Q; an actual
    outside-component exit is also required. No claim about every caller path.
    """
    finite = []
    for s in component:
        for row in model.get(s, ()):
            if row.offers:
                raise ValueError('no-offer illustration only')
            support = {t for t, p in row.direct if p > 0}
            if support - component and not ((support - {s}) & component):
                finite.append((s, row.name))
    return finite


def main(random_models: int = 512):
    checks: dict[str, bool] = {}
    def check(name: str, value: bool):
        checks[name] = bool(value)
        if not value:
            raise AssertionError(name)

    # Mutual retry with a probabilistic exit: neither non-goal is rank-orientable
    # by the old strict non-self-successor rule.
    model = {
        1: [Row('bad-cycle', F(1), ((2, F(1)),)),
            Row('try-exit', F(2), ((GOAL, F(1,2)), (2, F(1,2))))],
        2: [Row('return', F(1), ((1, F(1)),))],
    }
    old, assigned = strict_rank_then_first(model)
    winning, seed, rank, stats = synthesize(model)
    check('old_strict_rank_cannot_orient_mutual_cycle', assigned == {GOAL})
    check('old_first_row_fallback_is_improper', exact_cost(old, 1) is None)
    check('old_infinite_scc_exit_test_finds_no_finite_row', not old_closed_scc_repair_candidates(model, {1,2}))
    check('new_seed_covers_both_states', winning == {GOAL,1,2})
    check('new_rank_verified_independently', verify_rank(seed, rank, winning))
    check('proper_seed_cost_is_five', exact_cost(seed, 1) == 5)
    check('destructive_return_cost_is_six', exact_cost(seed, 2) == 6)
    check('full_enumeration_agrees_on_winning_set', exhaustive_winning(model)[0] == winning)

    # Outcome closure, not merely a path toward the target.
    risky = {1: [Row('risky', F(1), ((GOAL,F(1,2)),(2,F(1,2))))],
             2: [Row('trap', F(0), ((2,F(1)),))]}
    check('positive_goal_path_is_not_almost_sure', synthesize(risky)[0] == {GOAL})
    tiny = {1: [Row('rare-failure', F(1), ((GOAL,1-F(1,10**30)),(2,F(1,10**30))))],
            2: risky[2]}
    check('tiny_positive_trap_mass_must_not_be_dropped', synthesize(tiny)[0] == {GOAL})
    isolated = {1: [Row('finish', F(3), ((GOAL,F(1)),))], 2:risky[2]}
    w,p,r,_ = synthesize(isolated)
    check('unreachable_losing_state_does_not_block_root', w == {GOAL,1} and exact_cost(p,1)==3)

    # Every possible observation has a safe decision; at least one selected
    # branch makes progress. Decisions are local to the offered observation.
    observed = {1: [Row('observe', F(1), (),
                       ((F(1,2),(GOAL,2)), (F(1,2),(2,GOAL))))],
                2:risky[2]}
    w,p,r,_ = synthesize(observed)
    check('post_observation_choices_can_be_winning', w == {GOAL,1} and exact_cost(p,1)==1)
    check('offer_local_choice_identity_preserved', p[1].choice_indices == (0,1))
    precommit = {1:Decision(observed[1][0],(0,0)),2:Decision(risky[2][0],())}
    check('same_ordinal_precommit_is_not_equivalent', exact_cost(precommit,1) is None)
    bad_offer = {1:[Row('one_bad_offer',F(1),(),
                       ((F(1,2),(GOAL,2)),(F(1,2),(2,))))],2:risky[2]}
    check('every_positive_offer_needs_a_safe_choice', synthesize(bad_offer)[0] == {GOAL})

    # Incomplete views are conservative, not native impossibility certificates.
    partial = {1:[Row('unfinished-child',F(1),((2,F(1)),))], 2:[]}
    w0=synthesize(partial)[0]
    completed = dict(partial) | {2:[Row('complete-now',F(1),((GOAL,F(1)),))]}
    check('unknown_frontier_is_not_success', 1 not in w0)
    check('new_row_can_change_prior_view_refusal', synthesize(completed)[0] == {GOAL,1,2})
    check('qualitative_result_does_not_bound_policy_quality',
          exact_cost(synthesize({1:[Row('very-rare',F(1),((GOAL,F(1,10**9)),(1,1-F(1,10**9))))]})[1],1)==10**9)
    cheap = {1:[Row('first-expensive',F(100),((GOAL,F(1)),)),
                Row('later-cheap',F(1),((GOAL,F(1)),))]}
    check('first_proper_seed_is_not_optimality', exact_cost(synthesize(cheap)[1],1)==100)
    check('omitted_cheaper_action_keeps_upper_not_lower', exact_cost({1:Decision(cheap[1][1],())},1)==1)
    changed = dict(seed) | {1:Decision(model[1][0],())}
    check('changing_selected_decision_can_destroy_properness', exact_cost(changed,1) is None)
    check('rank_check_rejects_changed_controller', not verify_rank(changed,rank,winning))
    cycle = {1:[Row('option-A',F(1),((2,F(1)),))],2:[Row('option-B',F(1),((1,F(1)),))]}
    check('locally_terminating_options_can_cycle_globally', synthesize(cycle)[0] == {GOAL})
    try:
        synthesize({1:[Row('incomplete-mass',F(1),((GOAL,F(9,10)),))]})
        good_rejection=False
    except ValueError:
        good_rejection=True
    check('incomplete_mass_rejected',good_rejection)
    check('zero_cost_non_goal_trap_not_proper', synthesize({1:[Row('zero',F(0),((1,F(1)),))]})[0]=={GOAL})

    # Independent exhaustive comparison, including factored choice rows. No
    # numerical performance comparison is inferred from this tiny validation.
    rng=random.Random(20260910)
    policy_count=0
    for i in range(random_models):
        n=2 + i%3
        generated={}
        for s in range(1,n+1):
            rows=[]
            for j in range(2):
                support=rng.sample(range(n+1),rng.randint(1,min(3,n+1)))
                if rng.random()<0.3:
                    extra=rng.sample(range(n+1),rng.randint(1,min(2,n+1)))
                    row=Row(f'r{s}-{j}',F(rng.randint(0,5)),(),
                            ((F(1,2),tuple(support)),(F(1,2),tuple(extra))))
                else:
                    weights=[rng.randint(1,9) for _ in support]
                    row=Row(f'r{s}-{j}',F(rng.randint(0,5)),
                            tuple((t,F(a,sum(weights))) for t,a in zip(support,weights)))
                rows.append(row)
            generated[s]=rows
        w,p,r,_=synthesize(generated)
        truth,count=exhaustive_winning(generated)
        policy_count+=count
        if w!=truth or not verify_rank(p,r,w) or not w<=policy_winning(p,set(generated)|{GOAL}):
            raise AssertionError(f'cross-check failed on model {i}')
    check('all_exhaustive_randomized_view_comparisons_agree', True)
    return {
        'classification':'synthetic exact mathematical/reference checks; no native engine executed',
        'source_reference':'44fdca8ebf88b9b87b2bd832360cb62128e3968a',
        'named_checks':checks,'named_check_count':len(checks),
        'finite_models_exhaustively_cross_checked':random_models,
        'deterministic_observation_resolved_controllers_enumerated':policy_count,
        'random_seed':20260910,
        'mutual_retry': {'old_strict_rank_assigned':sorted(assigned),'old_seed':'improper',
                        'old_finite_exit_candidates':[], 'new_root_cost':'5',
                        'new_return_entry_cost':'6','ranks':rank,'work':stats},
        'limitations':[
            'The old helpers are restricted independent semantic illustrations, not calls into compiled native code.',
            'No claim the native CB06/CB08 stops instantiate this counterexample; the next plan requires a native discriminating fixture and matched root evidence.',
            'Existence and completeness apply only to the finite supplied complete-row view with its declared decisions and targets.',
            'The standard graph algorithm is not claimed novel or asymptotically optimal.',
            'Reported counts are reproducibility checks, not timings or predictions of crafting performance.'
        ]}

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path)
    parser.add_argument('--models',type=int,default=512)
    args=parser.parse_args()
    if args.models<1:
        parser.error('--models must be positive')
    payload=main(args.models)
    text=json.dumps(payload,indent=2,ensure_ascii=False)+'\n'
    if args.output:
        args.output.parent.mkdir(parents=True,exist_ok=True)
        args.output.write_text(text,encoding='utf-8')
    print(text)
