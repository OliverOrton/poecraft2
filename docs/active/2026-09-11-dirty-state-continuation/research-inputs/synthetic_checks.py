#!/usr/bin/env python3
"""Exact illustrations for the dirty-state capability plan.

Synthetic finite models only: not PoE mechanics, native tests, performance
measurements, or a proof of the complete implementation. Standard library only.
"""
from __future__ import annotations
import argparse
import itertools
import json
from fractions import Fraction as F
from pathlib import Path
from typing import Mapping, Sequence

Row = tuple[F, Mapping[str, F]]

def solve(a: Sequence[Sequence[F]], b: Sequence[F]) -> list[F] | None:
    n = len(b)
    if len(a) != n or any(len(row) != n for row in a):
        raise ValueError('Non-square system')
    m = [list(row) + [rhs] for row, rhs in zip(a, b)]
    for j in range(n):
        pivot = next((i for i in range(j, n) if m[i][j]), None)
        if pivot is None:
            return None
        m[j], m[pivot] = m[pivot], m[j]
        div = m[j][j]
        m[j] = [x/div for x in m[j]]
        for i in range(n):
            if i != j:
                v = m[i][j]
                m[i] = [x-v*y for x, y in zip(m[i], m[j])]
    return [m[i][-1] for i in range(n)]

def evaluate(policy: Mapping[str, Row]) -> dict[str, F] | None:
    names = list(policy)
    idx = {s: i for i, s in enumerate(names)}
    a = [[F(i == j) for j in range(len(names))] for i in range(len(names))]
    cost, goal = [], []
    for i, s in enumerate(names):
        c, law = policy[s]
        if c < 0 or any(p < 0 for p in law.values()) or sum(law.values(), F()) != 1:
            raise ValueError('Invalid row cost or mass')
        cost.append(c)
        goal.append(law.get('goal', F()))
        for t, p in law.items():
            if t != 'goal':
                if t not in idx:
                    raise ValueError(f'Unknown nonterminal {t}')
                a[i][idx[t]] -= p
    absorption = solve(a, goal)
    if absorption is None or any(x != 1 for x in absorption):
        return None
    values = solve(a, cost)
    if values is None or any(x < 0 for x in values):
        return None
    return dict(zip(names, values)) | {'goal': F()}

def debt(required: int, goals: int, affixes: int, *, correct_rarity: bool = True) -> int:
    if goals < 0 or goals > affixes or goals > required:
        raise ValueError('This illustration assumes distinct required physical goals')
    if correct_rarity and goals == required and affixes == goals:
        return 0
    return max(1, required-goals + affixes-goals + (not correct_rarity))

def main() -> dict[str, object]:
    checks: dict[str, bool] = {}
    def ck(name: str, result: bool) -> None:
        checks[name] = bool(result)
        if not result:
            raise AssertionError(name)

    ck('ring_two_root_debt_is_two', debt(2, 0, 0) == 2)
    ck('ring_two_full_goal_four_affix_is_tie', debt(2, 2, 4) == 2)
    ck('ring_two_all_four_to_six_affix_outcomes_fail_strict_progress',
       all(debt(2, g, k) >= 2 for k in (4,5,6) for g in range(3)))
    ck('amulet_two_of_three_in_four_affixes_is_tie', debt(3, 2, 4) == 3)
    ck('amulet_three_of_three_six_affixes_is_tie', debt(3, 3, 6) == 3)
    ck('amulet_three_goals_four_or_five_affixes_has_progress',
       debt(3, 3, 4) < 3 and debt(3, 3, 5) < 3)
    ck('cleanup_and_terminal_are_distinct', debt(2, 2, 3) == 1 and debt(2, 2, 2) == 0)
    ck('debt_formula_on_nonterminal_distinct_goal_domain', all(
       debt(m, g, k) == max(1, m+k-2*g)
       for m in (2,3,4,5) for k in range(7) for g in range(min(m,k)+1)
       if not (m == g == k)))
    # Pending routes have lexicographic priority, not a shared cost unit.
    ck('pending_route_priority_can_override_arbitrarily_cheaper_attempt',
       (0, 1, F(1000000)) < (0, 2, F(1)))

    # All costs and outcomes below are synthetic. A dirty detour is economical
    # even when the first action does not decrease the current debt function.
    rows: dict[str, Row] = {
        'r': (F(1), {'d': F(1)}),
        'd': (F(1), {'e': F(1,2), 'f': F(1,2)}),
        'e': (F(1), {'goal': F(1,3), 'h': F(2,3)}),
        'f': (F(1), {'d': F(1)}),
        'h': (F(1), {'d': F(1)}),
    }
    dirty = evaluate(rows)
    ck('dirty_controller_is_proper', dirty is not None)
    assert dirty is not None
    ck('dirty_continuation_exact_cost_fourteen', dirty['d'] == 14)
    ck('dirty_root_exact_cost_fifteen', dirty['r'] == 15)
    direct = dict(rows); direct['r'] = (F(100), {'goal': F(1)})
    clean = evaluate(direct)
    ck('clean_short_controller_can_cost_more', clean is not None and clean['r'] > dirty['r'])
    ck('dirty_acquisition_can_be_cheaper_without_debt_progress',
       debt(2,2,4) == debt(2,0,0) and dirty['r'] < 100)
    bad_mass = dict(rows); bad_mass['d'] = (F(1), {'e': F(1,2)})
    try:
        evaluate(bad_mass); mass_rejected = False
    except ValueError:
        mass_rejected = True
    ck('lost_goal_failure_mass_cannot_be_omitted', mass_rejected)
    ck('zero_cost_mutual_cycle_is_not_proper', evaluate({
        'a': (F(), {'b':F(1)}), 'b': (F(), {'a':F(1)})}) is None)

    # A useful partial item may be a valuable return boundary, not an obligation
    # to erase its progress. Every boundary still needs its own policy.
    multi = evaluate({'r': (F(1), {'p':F(1)}),
                      'p': (F(1), {'r':F(1,2),'goal':F(1,2)})})
    ck('multi_boundary_cost_is_solved_jointly', multi is not None and multi['r']==4 and multi['p']==3)
    forced = evaluate({'r': (F(1), {'p':F(1)}),
                       'p': (F(1), {'r':F(1)})})
    ck('returning_each_component_does_not_ensure_global_success', forced is None)
    unknown = dict(rows); unknown['d'] = (F(1), {'e':F(9,10),'missing':F(1,10)})
    try:
        evaluate(unknown); unknown_rejected = False
    except ValueError:
        unknown_rejected = True
    ck('unknown_tail_is_not_free_success', unknown_rejected)

    # Single return identity, and why ranking the one-shot advantage is not
    # the same as ranking the final repeated controller.
    old = F(10)
    r_a, p_a = F(36,5), F(9,10)  # repeated 8
    r_b, p_b = F(1,100), F(1,100) # repeated 1
    qa, qb = r_a+(1-p_a)*old, r_b+(1-p_b)*old
    ck('first_return_improvement_identity', old-qa == p_a*(old-r_a/p_a))
    ck('one_shot_ranking_can_miss_best_repeated_controller', qa < qb and r_a/p_a > r_b/p_b)
    ck('threshold_residual_has_correct_sign', (r_b-p_b*old < 0) == (r_b/p_b < old))
    ck('zero_goal_probability_is_not_a_finite_repeat', evaluate({'r': (F(1),{'r':F(1)})}) is None)

    # Fixed local region: g=(I-P)^-1 c, H=(I-P)^-1 P_boundary.
    a = [[F(1,2), F(-1,4)], [F(-1,4), F(1)]]
    g = solve(a, [F(1),F(2)])
    h0 = solve(a, [F(1,4),F(0)])
    h1 = solve(a, [F(0),F(3,4)])
    assert g is not None and h0 is not None and h1 is not None
    ck('first_exit_complete_mass', all(h0[i]+h1[i] == 1 for i in range(2)))
    v0, v1 = F(3), F(9)
    response = [g[i]+h0[i]*v0+h1[i]*v1 for i in range(2)]
    direct_response = solve(a, [F(1)+F(1,4)*v0, F(2)+F(3,4)*v1])
    ck('response_operator_matches_direct_solve', response == direct_response)
    ck('response_changes_when_boundary_values_change',
       [g[i]+h0[i]*9+h1[i]*3 for i in range(2)] != response)

    # Shared observer dependencies can make the joint removal the only useful
    # coarsening. These are invented modifier signatures, not a PoE count.
    tags = [frozenset({'fire','resistance'}), frozenset({'cold','resistance'}),
            frozenset({'lightning','resistance'}), frozenset()]
    conv_a = frozenset({'fire','lightning','resistance'})
    conv_b = frozenset({'cold','lightning','resistance'})
    def classes(obs: frozenset[str]) -> int:
        return len({x & obs for x in tags})
    counts = [classes(conv_a|conv_b), classes(conv_a), classes(conv_b), classes(frozenset())]
    ck('individual_observer_removals_can_show_no_gain', counts[:3] == [4,4,4])
    ck('joint_observer_removal_can_merge_classes', counts[3] == 1)
    probs = [F(1,10), F(2,10), F(3,10), F(4,10)]
    ck('coarsening_preserves_aggregated_positive_mass', sum(probs,F()) == 1)
    ck('retained_blocker_dimension_prevents_unsafe_merge',
       len({(frozenset(), block) for block in (0,1)}) == 2)
    ck('refinement_parent_identity_prevents_coarsening',
       len({(i, frozenset()) for i in range(4)}) == 4)

    # Family response and correlated action attributes.
    q = lambda c, p, v: c+p*v
    ck('family_winner_depends_on_boundary', q(1,F(9,10),2)<q(5,F(1,10),2) and q(1,F(9,10),20)>q(5,F(1,10),20))
    ck('family_switch_at_five', q(1,F(9,10),5)==q(5,F(1,10),5))
    grid = [F(i,10) for i in range(11)]
    ck('convex_hull_preserves_fixed_linear_minimum', all(
        min(t*q(1,F(9,10),v)+(1-t)*q(5,F(1,10),v) for t in grid)
        == min(q(1,F(9,10),v),q(5,F(1,10),v)) for v in range(21)))
    ck('mixing_cheapest_and_safest_can_invent_unavailable_action',
       q(1,F(1,10),20) < min(q(1,F(9,10),20),q(5,F(1,10),20)))
    ck('action_hindsight_is_not_legal_precommitment',
       1+min(F(0+100,2),F(100+0,2)) == 51 and 1+F(min(0,100)+min(100,0),2)==1)
    # Tiny changed-potential counterexample for query-specific stopping.
    weights = [F(1,4)]*4
    ck('constant_tail_query_aggregates_exactly', sum((p*12 for p in weights),F()) == 12)
    ck('cached_value_query_cannot_ignore_new_potential', sum(p*v for p,v in zip(weights,[0,12,12,12])) == 9)
    return {
        'classification': 'exact synthetic illustrations only; no native engine executed',
        'reviewed_main': '0ca76eb4a9f35a90299ecceeab740e5535804fd9',
        'checks': checks, 'check_count': len(checks),
        'dirty_controller_values': {k:str(v) for k,v in dirty.items()},
        'synthetic_observer_class_counts_full_single_single_pair': counts,
        'multi_boundary_values': {k:str(v) for k,v in multi.items()} if multi else None,
        'response_operator': {'g':[str(x) for x in g],
                              'H':[[str(h0[i]),str(h1[i])] for i in range(2)],
                              'value_at_boundary_3_9':[str(x) for x in response]},
        'limitations': [
            'No new native layout counts, reforge timings, policies, or cap-scaling results.',
            'The debt illustration assumes distinct physical goal modifiers, fixed correct rarity, and g <= required.',
            'The dirty SSP deliberately has invented transitions; it is not a proposed native crafting recipe.',
            'The convex grid check illustrates a separately proved linearity identity, not arbitrary hull verification.',
            'Conditional native mass, executable observations, full member validity and numerical acceptance remain implementation obligations.'
        ]}

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    result = main()
    text = json.dumps(result, indent=2) + '\n'
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(text, encoding='utf-8')
    print(text)
