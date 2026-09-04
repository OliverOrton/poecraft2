"""Research only. Exact rational SSP fixtures; no PoE mechanics or production code.

The requested accompanying ssp_certificate_experiments.py was not supplied.
This independent reference uses Fraction Gaussian elimination, exhaustive proper
stationary-policy evaluation, and independently enumerated LP vertices. Small
bounded fixtures only: vertex enumeration is deliberately not a production LP.
"""
from dataclasses import dataclass
from fractions import Fraction as F
from itertools import product, combinations
from pathlib import Path
import json
import time
import tracemalloc


@dataclass(frozen=True)
class Row:
    action: str
    cost: F
    successors: tuple


def row(action, cost, **successors):
    out = Row(action, F(cost), tuple((k, F(v)) for k, v in successors.items()))
    assert out.cost >= 0 and sum(p for _, p in out.successors) == 1
    assert all(p > 0 for _, p in out.successors)
    return out


def gaussian(a, b):
    n = len(b)
    m = [list(map(F, ar)) + [F(v)] for ar, v in zip(a, b)]
    for j in range(n):
        pivot = next((i for i in range(j, n) if m[i][j]), None)
        if pivot is None:
            return None
        m[j], m[pivot] = m[pivot], m[j]
        z = m[j][j]
        m[j] = [x / z for x in m[j]]
        for i in range(n):
            if i != j:
                z = m[i][j]
                m[i] = [x-z*y for x, y in zip(m[i], m[j])]
    return [m[i][-1] for i in range(n)]


def inequalities(model, boundary):
    states = list(model)
    index = {s: i for i, s in enumerate(states)}
    constraints = []
    for s, rows in model.items():
        for r in rows:
            a = [F(0)] * len(states)
            a[index[s]] = F(1)
            c = r.cost
            for t, p in r.successors:
                if t in index:
                    a[index[t]] -= p
                else:
                    assert t in boundary
                    c += p * boundary[t]
            constraints.append((a, c, (s, r.action)))
    return states, constraints


def lp_vertices(model, boundary):
    states, action_constraints = inequalities(model, boundary)
    n = len(states)
    constraints = action_constraints + [
        ([-F(i == j) for i in range(n)], F(0), (s, 'nonnegative'))
        for j, s in enumerate(states)]
    feasible = []
    attempted = 0
    for active in combinations(constraints, n):
        attempted += 1
        value = gaussian([a for a, _, _ in active], [b for _, b, _ in active])
        if value is not None and all(sum(x*y for x, y in zip(a, value)) <= b
                                     for a, b, _ in constraints):
            feasible.append(value)
    assert feasible, 'fixtures must be bounded nonempty LPs'
    best = max(feasible, key=sum)
    # Check componentwise greatest, stronger than objective optimality.
    assert all(all(a <= b for a, b in zip(v, best)) for v in feasible)
    tight = [label for a, b, label in action_constraints
             if sum(x*y for x, y in zip(a, best)) == b]
    return dict(zip(states, best)), tight, attempted


def policy_dp(model, boundary):
    """Exhaustive proper-policy DP reference, independent of LP active sets."""
    states = list(model)
    proper_values = []
    attempts = 0
    improper = 0
    for selected in product(*(model[s] for s in states)):
        attempts += 1
        # In a finite Markov chain, every state reaching an absorbing boundary
        # by positive edges excludes a closed nonterminal recurrent class.
        reaches_exit = set()
        while True:
            old = set(reaches_exit)
            for s, r in zip(states, selected):
                if any(t not in model or t in reaches_exit for t, p in r.successors):
                    reaches_exit.add(s)
            if old == reaches_exit:
                break
        if len(reaches_exit) != len(states):
            improper += 1
            continue
        one = dict(zip(states, ([r] for r in selected)))
        _, constraints = inequalities(one, boundary)
        value = gaussian([a for a, _, _ in constraints], [b for _, b, _ in constraints])
        assert value is not None and all(v >= 0 for v in value)
        proper_values.append(value)
    assert proper_values
    best = [min(v[i] for v in proper_values) for i in range(len(states))]
    assert best in proper_values, 'stationary optimum must be attained'
    return dict(zip(states, best)), attempts, improper


def lp_simplex(model, boundary):
    """Small exact primal LP oracle with Bland pivots; origin is feasible.

    Unlike vertex enumeration this detects an improving unbounded ray. It
    shares the model encoder, but not the policy solver's Gaussian elimination.
    """
    states, constraints = inequalities(model, boundary)
    n, m = len(states), len(constraints)
    assert all(b >= 0 for _, b, _ in constraints)
    table = [a + [F(i == j) for j in range(m)] + [b]
             for i, (a, b, _) in enumerate(constraints)]
    table.append([-F(1)] * n + [F(0)] * (m+1))
    basis = list(range(n,n+m))
    pivots = 0
    while True:
        entering = next((j for j in range(n+m) if table[-1][j] < 0), None)
        if entering is None:
            break
        candidates = [i for i in range(m) if table[i][entering] > 0]
        if not candidates:
            raise ValueError('unbounded improving ray')
        leaving = min(candidates, key=lambda i: (table[i][-1]/table[i][entering], basis[i]))
        z = table[leaving][entering]
        table[leaving] = [v/z for v in table[leaving]]
        for i in range(m+1):
            if i != leaving:
                z = table[i][entering]
                table[i] = [v-z*w for v, w in zip(table[i],table[leaving])]
        basis[leaving] = entering
        pivots += 1
        assert pivots < 1000, 'research fixture pivot budget'
    value = [F(0)]*n
    for i, j in enumerate(basis):
        if j < n:
            value[j] = table[i][-1]
    result = dict(zip(states,value))
    assert valid(model,boundary,result)
    assert sum(value) == table[-1][-1]
    return result, pivots


def bellman(model, boundary, value):
    joined = boundary | value
    return {s: min(r.cost + sum(p * joined[t] for t, p in r.successors)
                   for r in rows) for s, rows in model.items()}


def valid(model, boundary, value):
    backed = bellman(model, boundary, value)
    return all(value[s] <= backed[s] for s in model)


def measure(name, model, boundary):
    tracemalloc.start()
    begin = time.perf_counter_ns()
    dp, policies, improper = policy_dp(model, boundary)
    lp, tight, vertices = lp_vertices(model, boundary)
    simplex, pivots = lp_simplex(model, boundary)
    assert dp == lp == simplex and valid(model, boundary, lp)
    residual = max(abs(lp[s] - v) for s, v in bellman(model, boundary, lp).items())
    assert residual == 0
    elapsed = time.perf_counter_ns() - begin
    retained, peak = tracemalloc.get_traced_memory()
    tracemalloc.stop()
    return dict(name=name, values=lp, tight=tight, states=len(model),
                rows=sum(len(v) for v in model.values()),
                transitions=sum(len(r.successors) for v in model.values() for r in v),
                proper_policy_candidates=policies-improper, rejected_improper=improper,
                lp_active_sets_attempted=vertices, simplex_pivots=pivots,
                exact_rational_residual=residual,
                elapsed_ns=elapsed, python_retained_bytes=retained,
                python_peak_bytes=peak)


def experiments():
    records = []
    goal = {'g': F(0)}
    # 1: The incumbent is proper, yet suboptimal after a deviation.
    m = {'r': [row('incumbent', 10, g=1), row('deviate', 1, t=1)],
         't': [row('finish', 4, g=1)]}
    records += [measure('cheaper_deviation_complete', m, goal),
                measure('cheaper_deviation_boundary0', {'r': m['r']}, goal | {'t': F(0)})]
    assert records[-2]['values']['r'] == 5
    assert not valid(m, goal, {'r': F(10), 't': F(4)})
    proposal = {'r': F(10), 't': F(4)}
    repaired = bellman(m, goal, proposal)
    assert repaired == {'r': F(5), 't': F(4)} and valid(m, goal, repaired)
    records.append({'name': 'J_candidate_repair', 'before': proposal, 'after': repaired,
                    'bellman_sweeps': 1})
    # 2: Root optimum is provable without a continuation at t in incumbent.
    m = {'r': [row('incumbent', 10, g=1), row('deviate', 1, t=1)],
         't': [row('finish', 20, g=1)]}
    records += [measure('no_incumbent_router_at_t', {'r': m['r']}, goal | {'t': F(9)}),
                measure('no_incumbent_router_true_model', m, goal)]
    assert valid(m, goal, {'r': F(10), 't': F(9)})
    # 3: Stochastic SCC. Fixing either other state at zero loses strength.
    m = {'s': [row('cycle', 1, t=F(1,2), g=F(1,2)), row('exit', 10, g=1)],
         't': [row('cycle', 2, s=F(1,4), g=F(3,4)), row('exit', 10, g=1)]}
    records.append(measure('proper_stochastic_SCC', m, goal))
    assert records[-1]['values'] == {'s': F(16,7), 't': F(18,7)}
    records.append(measure('SCC_one_state_boundary0', {'s': m['s']}, goal | {'t': F(0)}))
    # 4: Coverage and placeholders. Duplicate constraint does not cover cheap.
    full = {'s': [row('safe', 10, g=1), row('cheap', 5, g=1)]}
    omitted = {'s': [full['s'][0]]}
    duplicated = {'s': [full['s'][0], full['s'][0]]}
    placeholder = {'s': [full['s'][0], row('cheap', 2, g=1)]}
    for name, m in [('missing_action_UNSOUND', omitted), ('duplicate_action_UNSOUND', duplicated),
                    ('covered_placeholder', placeholder), ('placeholder_refined', full)]:
        records.append(measure(name, m, goal))
    assert len(duplicated['s']) == len(full['s'])
    assert {r.action for r in duplicated['s']} != {r.action for r in full['s']}
    # Temporarily inactive actual constraint: solve omitted graph tentatively,
    # scan the one inactive constraint, reactivate it, then solve/check all rows.
    trial = lp_vertices(omitted, goal)[0]
    assert not valid(full, goal, trial)
    records.append({'name': 'temporary_inactivity', 'tentative_noncertificate': trial,
                    'repaired': lp_vertices(full, goal)[0], 'inactive_scans': 1,
                    'reactivations': 1, 'final_action_scans': 2,
                    'permanent_retirements': 0})
    # 5: Destructive cleanup loses the goal and incurs reconstruction debt.
    for suffix, rebuilding, exit_cost in [('shape_A', 8, 12), ('shape_B', 11, 15)]:
        exact = {'dirty': [row('cleanup', 2, empty=1), row('direct', exit_cost, g=1)],
                 'empty': [row('rebuild', rebuilding, g=1)]}
        cheap = {'dirty': [row('cleanup_free_preservation', 2, g=1), row('direct', exit_cost, g=1)]}
        records += [measure('cleanup_preserved_' + suffix, cheap, goal),
                    measure('cleanup_capacity_' + suffix, exact, goal)]
    # 6: Max LP is proper-policy value; bottom VI is ordinary cost value.
    m = {'s': [row('zero_loop', 0, s=1), row('exit', 10, g=1)]}
    records.append(measure('zero_cost_improper_cycle_proper_objective', m, goal))
    assert bellman(m, goal, {'s': F(0)}) == {'s': F(0)}
    assert valid(m, goal, {'s': F(10)})
    records.append({'name': 'zero_cost_semantics', 'proper_policy_optimum': F(10),
                    'ordinary_accumulated_cost_optimum': F(0),
                    'bottom_VI_fixed_point': F(0), 'max_LP_fixed_point': F(10)})
    # 7: A tiny residual can conceal significant overstatement of a lower.
    p = F(1, 100_000_000)
    m = {'s': [row('retry', 1, s=1-p, g=p)]}
    records.append(measure('long_retry_horizon', m, goal))
    true = records[-1]['values']['s']
    high = {'s': true + 100}
    defect = high['s'] - bellman(m, goal, high)['s']
    assert defect == F(1,1_000_000)
    assert defect < F(1,10**12) * high['s']
    records.append({'name': 'small_residual_UNSOUND_tolerance', 'true_value': true,
                    'candidate': high['s'], 'positive_inequality_defect': defect,
                    'value_overstatement': F(100), 'expected_retry_horizon': 1/p})
    # 8: Admissible need not be Bellman-consistent at successor base values.
    m = {'s': [row('step', 1, t=1)], 't': [row('finish', 5, g=1)]}
    admissible = {'s': F(6), 't': F(0)}
    actual = lp_vertices(m, goal)[0]
    assert all(admissible[s] <= actual[s] for s in m)
    assert not valid(m, goal, admissible)
    records.append(measure('admissible_inconsistent_finite_R', {'s': m['s']}, goal | {'t': F(0)}))
    records.append({'name': 'admissible_inconsistent', 'base': admissible,
                    'true_values': actual, 'base_bellman': bellman(m, goal, admissible),
                    'root_floor6_plus_boundary0_constraints': 'infeasible',
                    'independent_max_remains_admissible': F(6)})
    # Extra: enlargement alone can lower the relaxed optimum if old boundary
    # certificates are replaced by weaker local placeholders.
    old = {'s': [row('step', 1, t=1), row('exit', 10, g=1)]}
    new = old | {'t': [row('weak_placeholder', 1, g=1)]}
    records.append(measure('extension_old_boundary9', old, goal | {'t': F(9)}))
    records.append(measure('extension_drops_boundary_NOT_monotone', new, goal))
    assert records[-2]['values']['s'] == 10 and records[-1]['values']['s'] == 2
    # Stronger extension counterexample: only exact rows, unchanged admissible
    # but inconsistent boundary function; no weakening of any local kernel.
    old = {'r': [row('step', 0, t=1)]}
    new = old | {'t': [row('step', 0, u=1)]}
    records.append(measure('exact_extension_old_inconsistent_boundary', old,
                           goal | {'t': F(10), 'u': F(0)}))
    records.append(measure('exact_extension_new_inconsistent_boundary', new,
                           goal | {'u': F(0)}))
    assert records[-2]['values']['r'] == 10 and records[-1]['values']['r'] == 0
    # Extra: primitive mandatory setup must be charged; observation order matters.
    records.append({'name': 'mandatory_program', 'setup': F(3), 'primitive': F(2),
                    'mandatory_program_cost': F(5), 'free_setup_relaxation': F(2),
                    'post_observation_choice_cost': F(0),
                    'pre_observation_choice_cost': F(5),
                    'observation_example': 'two fair offers; costs (0,10) and (10,0)'})
    records.append(measure('mandatory_program_phases', {
        'entry': [row('setup', 3, phase=1)],
        'phase': [row('mandatory_finish', 2, g=1)]}, goal))
    records.append(measure('mandatory_program_macro', {
        'entry': [row('complete_macro', 5, g=1)]}, goal))
    records.append(measure('observation_before_choice', {
        'entry': [row('observe', 0, offerA=F(1,2), offerB=F(1,2))],
        'offerA': [row('left', 0, g=1), row('right', 10, g=1)],
        'offerB': [row('left', 10, g=1), row('right', 0, g=1)]}, goal))
    records.append(measure('choice_before_observation', {
        'entry': [row('left_before_observation', 5, g=1),
                  row('right_before_observation', 5, g=1)]}, goal))
    # Extra: self remains a legal option in an observed choice group.
    m = {'s': [row('offer_select_self', 1, s=F(1,2), g=F(1,2)),
               row('offer_select_t', 1, t=F(1,2), g=F(1,2))],
         't': [row('finish', 100, g=1)]}
    records.append(measure('observed_choice_self', m, goal))
    assert records[-1]['values']['s'] == 2
    records.append({'name': 'envelope_formula_self_choice_counterexample',
                    'shared_row_semantics': F(2), 'ignore_self_when_alternate_finite': F(51),
                    'native_reachability': 'not established; mechanics-independent source counterexample'})
    return records


if __name__ == '__main__':
    results = experiments()
    output = Path(__file__).with_name('synthetic-results.json')
    output.write_text(json.dumps({'evidence': 'research, exact rational, mechanics independent',
        'records': results}, indent=2, default=lambda x: str(x) if isinstance(x,F) else x), encoding='utf-8')
    print(f'{len(results)} research records; exact policy enumeration and LP vertex checks passed')
    print(output)
