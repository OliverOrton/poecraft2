"""Research-only matched LP analysis of native exported coefficients.

Native rows contain rounded doubles, not exact probability rationals. Preserve
raw binary coefficients, and explicitly construct a separate exactly normalized
rational reference model. Report both. No new native certificate is produced.
"""
from pathlib import Path
from fractions import Fraction as F
from collections import Counter
import json
import time
import tracemalloc
from free_value_fixtures import Row, lp_simplex, policy_dp, bellman, valid

directory = Path(__file__).parent
native = json.loads((directory/'native-micro.json').read_text())
states = {str(s['id']): s for s in native['states']}
boundary = {s: F(0) for s, v in states.items() if v['goal']}
base = {s: F(v['lower']) for s, v in states.items()}
caller = set(native['caller_actions'])
legal = {s: {} for s in states if s not in boundary}
obligations = Counter()
mass_defects = []
for r in native['rows']:
    s = str(r['source'])
    obligations[(s,r['action'])] += 1
    assert r['action'] in caller and r['supported']
    if not r['applicable']:
        assert r['inapplicability_owner'] == 'native_action_legal' and not r['entries']
        continue
    assert r['action'] not in legal[s]
    succ = tuple((str(e['target']), F(e['p'])) for e in r['entries'])
    assert all(t in states and p > 0 for t,p in succ)
    mass = sum(p for _,p in succ)
    mass_defects.append(mass - 1)
    legal[s][r['action']] = (F(r['cost']),succ,mass)
for s in legal:
    assert {a for t,a in obligations if t == s} == caller
    assert all(obligations[(s,a)] == 1 for a in caller)


def model(domain, *, normalized, placeholders=False):
    out = {}
    for s in domain:
        out[s] = []
        for action, (cost, succ, mass) in legal[s].items():
            if placeholders:
                out[s].append(Row(action,max(base[s],cost),(('scalar_sink',F(1)),)))
            else:
                out[s].append(Row(action,cost,tuple((t,p/mass if normalized else p) for t,p in succ)))
    return out


def run(name, domain, *, placeholders=False, normalized=True):
    m = model(domain, normalized=normalized, placeholders=placeholders)
    b = {s:v for s,v in base.items() if s not in domain} | boundary | {'scalar_sink': F(0)}
    tracemalloc.start()
    began=time.perf_counter_ns()
    value,pivots=lp_simplex(m,b)
    if normalized:
        policies,attempts,improper=policy_dp(m,b)
        assert value==policies
    else:
        # Slightly nonunit raw coefficients are linear algebra, not an SSP.
        attempts=improper=None
    backed=bellman(m,b,value)
    residual=max(abs(value[s]-backed[s]) for s in m)
    assert residual==0
    elapsed=time.perf_counter_ns()-began
    retained,peak=tracemalloc.get_traced_memory()
    tracemalloc.stop()
    joined=b|value
    tight={s:[r.action for r in rs if r.cost+sum(p*joined[t] for t,p in r.successors)==value[s]]
           for s,rs in m.items()}
    return dict(name=name, normalized_reference=normalized, values=value,
                root=float(value['0']),minimum_actions=tight, residual=residual,
                modeled_nonterminal_states=len(m),rows=sum(map(len,m.values())),
                coefficients=sum(len(r.successors) for rs in m.values() for r in rs),
                simplex_pivots=pivots,policy_candidates=attempts,improper_rejected=improper,
                combined_reference_elapsed_ns=elapsed, arithmetic_retained_bytes=retained,
                arithmetic_peak_bytes=peak)


records=[]
records.append(run('existing_lower_scalar_coverage',list(legal),placeholders=True))
records.append(run('root_exact_outside_existing_boundary',['0']))
records.append(run('refine_goal_plus_junk_recovery',['0','7']))
records.append(run('full_free_value_reference',list(legal)))
records.append(run('full_raw_binary_coefficient_algebra',list(legal),normalized=False))
oracle=records[-2]['values']
complete=model(list(legal),normalized=True)
frozen_root_only=valid(model(['0'],normalized=True),{s:v for s,v in base.items() if s!='0'}, {'0':oracle['0']})
assert not frozen_root_only and valid(complete,boundary,oracle)
base_backup=bellman(complete,boundary,{s:base[s] for s in legal})
base_violations={s:str(base[s]-base_backup[s]) for s in legal if base[s]>base_backup[s]}
for record in records:
    if record['normalized_reference']:
        record['exact_reference_oracle_gap'] = oracle['0']-record['values']['0']
        record['gain_over_fresh_existing_lower'] = record['values']['0']-base['0']
    else:
        record['exact_reference_oracle_gap'] = None
        record['gain_over_fresh_existing_lower'] = None
        record['cross_model_value_difference'] = record['values']['0']-oracle['0']

# Canonical complete row/action parity permits exact failure-state lumping.
# Goals are kept separate; scalar donor values need not share this equivalence.
failures=[s for s in legal if s!='0']
signatures={s: tuple((a,c,tuple((t,p/m) for t,p in succ))
                     for a,(c,succ,m) in sorted(legal[s].items())) for s in failures}
assert all(signatures[s]==signatures[failures[0]] for s in failures)

out=dict(evidence='native-kernel research plus explicit rational reference; no production qualification',
         numeric_contract='raw double coefficient sums preserved and audited; exact normalized rational model is separate and does not certify underlying integer/real probability derivation',
         maximum_raw_binary_mass_defect=max(abs(v) for v in mass_defects),
         raw_binary_mass_defects=sorted(set(map(str,mass_defects))),
         checked_action_set_obligations=len(obligations),exact_inapplicabilities=7,
         records=records, fixed_J_complete_reference_pass=True,
         fixed_J_root_only_with_existing_boundary_pass=frozen_root_only,
         base_bellman_violations=base_violations,
         native_failure_rows_exactly_equal=True, behaviorally_equal_failure_states=failures,
         quotient_state_count_including_goal=3,
         native_new_lower_authority=False,
         policy_routing_validation='not rerun; fixed-J checks above are mathematical constraints on exported model, not strategy-entry API certification')
(directory/'native-model-comparison.json').write_text(json.dumps(out,indent=2,default=str))
for r in records:
    gap=r['exact_reference_oracle_gap']
    print(r['name'],r['root'],'pivots',r['simplex_pivots'],'same-model gap',None if gap is None else float(gap))
print('raw mass defect',float(out['maximum_raw_binary_mass_defect']),'base violations',base_violations)
