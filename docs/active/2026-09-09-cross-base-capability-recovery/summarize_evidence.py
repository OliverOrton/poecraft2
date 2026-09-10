"""Extract this experiment's compact receipt from existing native runner reports.

This is an evidence view only; native and solver_reports classifications stand.
First-incumbent observations are not backdated independent graph evaluations.
"""
import hashlib
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
OUT = ROOT / 'out/2026-09-09-cross-base-capability-recovery'


def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def compact(case, receipt, path):
    telemetry = case['solver_telemetry']
    envelope = telemetry['incremental_action_envelope']
    graph = (case['compiled_graph'] or {}).get('strategy_output_path')
    graph_path = Path(graph) if graph else None
    timings = telemetry['timings_ns']
    return {
        'id': case['id'],
        'report': str(path.relative_to(ROOT)),
        'report_sha256': sha(path),
        'graph_sha256': sha(graph_path) if graph_path and graph_path.exists() else None,
        'base': case['input']['session']['base_name'],
        'goals': case['input']['goal']['min_satisfied_slots'],
        'native_summary': case['solve_summary'],
        'independent_evaluation_at_return': {
            k: v for k, v in case['exact_strategy_evaluation'].items()
            if k != 'result'
        },
        'first_native_incumbent_observation_ms': case['bound_trace'].get('time_to_first_incumbent_ms'),
        'phase_wall_ms': case['phase_wall_ms'],
        'process_wall_ms': receipt.get('wall_ms'),
        'native_peak_owned_bytes': case['memory']['native_peak_owned_bytes'],
        'native_timings_ns': timings,
        'states': telemetry['states'],
        'last_trace_sample': case['bound_trace']['samples'][-1],
        'strict_lift': telemetry['policy_refinement'].get('strict_lift'),
        'cap_hits': telemetry['optimization'].get('cap_hits'),
        'evaluation_failure': telemetry['optimization'].get('policy_evaluation_failure'),
        'envelope_closed': envelope['closed'],
        'envelope_actions': envelope['actions'],
        'q_refinement': envelope['q_refinement'],
        'joint_anytime_policy': envelope['joint_anytime_policy'],
        'scope_actions': case['product_action_ids'],
        'errors': case['errors'],
        'bounded_best_policy_contract': case['bounded_best_policy_contract'],
        'process_survivor': receipt.get('survivor'),
    }


def main():
    result = {
        'source_baseline': read(OUT / 'A1-baseline-binary-identity.json'),
        'comparison_contract': 'See derivations.json; timing comparisons are serialized.',
        'first_incumbent_contract': 'Native step-boundary observation only. Independent graph evaluation occurs at return; its wall time is separate.',
        'short_controls': read(OUT / 'A4-imported-short-controls.json'),
        'runs': {},
    }
    reference_path = OUT / 'A0-partial-reference.json'
    reference = read(reference_path)
    result['historical_partial_reference'] = {
        k: v for k, v in reference.items() if k != 'evaluation'
    }
    result['historical_partial_reference'].update(
        report_sha256=sha(reference_path),
        converged=reference['evaluation']['converged'],
        terminals=reference['evaluation']['terminals'],
        totals=reference['evaluation']['accounting']['totals'],
    )
    for label in ['A3-ordinary-baseline', 'B1-selected-fringe-pair',
                  'B2-selected-fringe-priority-pair', 'B5-restored-pair']:
        ledger_path = OUT / label / 'ledger.json'
        if not ledger_path.exists():
            continue
        ledger = read(ledger_path)
        rows = []
        for cid, receipt in ledger['cases'].items():
            if receipt['status'] != 'completed':
                rows.append({'id': cid, 'runner_receipt': receipt})
                continue
            path = Path(receipt['report_path'])
            rows.append(compact(read(path)['cases'][0], receipt, path))
        result['runs'][label] = {
            'ledger_sha256': sha(ledger_path),
            'executable': ledger['executable'],
            'artifact': ledger['artifact'],
            'all_completed': ledger.get('all_completed'),
            'survivors': ledger.get('survivors'),
            'cases': rows,
        }
    for label in ['B4-source-restored', 'B4-restored-build-identity']:
        path = OUT / (label + '.json')
        if path.exists():
            result[label] = read(path)
    report_path = OUT / 'B3-comparison-report.json'
    if report_path.exists():
        result['existing_reporter_comparisons'] = {
            'report': str(report_path.relative_to(ROOT)),
            'sha256': sha(report_path),
            'comparisons': read(report_path)['comparisons'],
        }
    checks_path = OUT / 'B6-closeout-checks.json'
    if checks_path.exists():
        result['closeout_checks'] = read(checks_path)
    (HERE / 'evidence-summary.json').write_text(
        json.dumps(result, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
