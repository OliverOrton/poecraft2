"""Reproduce this run's matched semantic and outcome comparison; no native runs."""
import hashlib
import json
from pathlib import Path
import sys

directory = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parent
def read(name):
    return json.loads((directory/name).read_text(encoding='utf-8'))
def semantic(value):
    if isinstance(value, dict):
        return {k: semantic(v) for k, v in value.items() if not k.endswith('_ns')}
    if isinstance(value, list):
        return [semantic(v) for v in value]
    return value

before, after = [read(mode+'-compact.stdout.txt') for mode in ('baseline', 'coverage')]
for field in ('pilot','solver_steps','production_authority','proposal_adapter',
              'joint_control','bench_audit','probabilistic_donor','sources','resources'):
    assert semantic(before[field]) == semantic(after[field]), field
assert semantic(before['preparation_profile']) == semantic(after['preparation_profile'])
audit = read('coverage-exact.json')
assert audit['checked_relations'] == len(after['probabilistic_donor']['checked_relations']) == 26259
for run in (before, after):
    assert run['resources']['proof_budget_bytes'] == 32 << 20
    assert run['resources']['combined_additional_peak_bytes'] <= 32 << 20
    assert run['process_peak_working_set_bytes'] <= 1 << 30
profiles = [x['preparation_profile'] for x in (before, after)]
development = [read(mode+'-development.json') for mode in ('baseline','coverage')]
assert development[0]['artifact'] == development[1]['artifact']
cases = [d['cases'][0] for d in development]
assert cases[0]['input'] == cases[1]['input']
target = 2698.87479601436
outcomes = []
for mode, case in zip(('baseline','coverage'), cases):
    assert case['id'] == 'conquest-lamellar-allflame-fractured-4-to-5-product8'
    assert not case['errors'] and case['cap_checks']['all_passed']
    assert case['input']['verification']['runs'] == 0 and case['verification'] is None
    assert case['input']['caps']['max_solver_owned_bytes'] == 1 << 30
    assert case['input']['native_retention_diagnostic'] == 'reuse'
    evaluation = case['exact_strategy_evaluation']
    assert evaluation['completed'] and evaluation['status'] == 'matched'
    assert evaluation['cost_complete'] and evaluation['zero_off_policy_mass'] and evaluation['cost_reconciled']
    assert evaluation['success_probability'] == 1 and evaluation['off_policy_mass'] == 0
    summary = case['solve_summary']
    assert summary['upper_bound'] == summary['evaluated_policy_cost'] == evaluation['total_expected_cost']
    assert not case['execution']['watchdog_expired'] and case['phase_wall_ms']['total'] <= 60000
    outcomes.append(dict(mode=mode, phase_wall_ms=case['phase_wall_ms'],
        lower=summary['lower_bound'], verified_upper=summary['upper_bound'],
        absolute_gap=summary['absolute_optimality_gap'], expanded_states=summary['expanded_states'],
        upper_target=target, target_met=summary['upper_bound'] <= target+1e-7,
        exact_closure=summary['converged'] and summary['policy_status'] == 'exact',
        termination=summary['termination'], native_peak_bytes=case['memory']['native_peak_owned_bytes']))
for mode in ('baseline','coverage'):
    for kind in ('compact','development'):
        process = read(f'{mode}-{kind}.process.json')
        assert process['exit_code'] == 0
        assert not any(process[k] for k in ('timed_out','canceled','survivor'))
result = dict(
    comparison='reviewed current native baseline versus ordered full-key coverage; numerical reuse enabled in both',
    compact_semantic_payload_equal=True, preparation_work_equal=True, compact_memory_equal=True,
    checked_relations=audit['checked_relations'],
    compact=dict(baseline=profiles[0], coverage=profiles[1],
        preparation_reduction=1-profiles[1]['total_ns']/profiles[0]['total_ns'],
        relation_reduction=1-profiles[1]['relations_ns']/profiles[0]['relations_ns']),
    development=outcomes,
    observation='One sequential matched pair; preparation improvement only. No improved discovery, target or exact closure.',
    input_sha256={name:hashlib.sha256((directory/name).read_bytes()).hexdigest()
        for name in ('baseline-compact.stdout.txt','coverage-compact.stdout.txt',
                     'baseline-development.json','coverage-development.json','coverage-exact.json')})
(directory/'comparison.json').write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
print(json.dumps({k:v for k,v in result.items() if k not in ('compact','input_sha256')}, indent=2))
