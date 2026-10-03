"""Execute an approved, serial frozen-case measurement after focused qualification."""
from pathlib import Path
import datetime
import hashlib
import importlib.util
import json
import sys

ROOT = Path(__file__).resolve().parents[2]
OUT = Path(__file__).resolve().parent
sys.path.insert(0, r'C:\Users\Oliver\Documents\poecraft2\tools\ingest')
from poecraft_ingest.solver_worker import SolverCaseCommand, run_isolated_process

qualification = sys.argv[1]
tag = sys.argv[2]
mode = sys.argv[3] if len(sys.argv) > 3 else 'recovery'
assert mode in ('recovery', 'law3-conquest')
state_override = 0
OLD = Path(r'C:\Users\Oliver\AppData\Local\Temp\poecraft2-sol61-diagnostic-worktree\out\selective-cap-repair')
if state_override not in (0, 800000):
    raise SystemExit('Only the declared 800,000-state headroom arm is supported')
qualified = json.loads((OUT/f'{qualification}-qualification-result.json').read_text(encoding='utf-8'))
required = {'build-tests','growth','protected-fill','protected-finder','cap-diagnosis','continuity','build-benchmark'}
if mode == 'law3-conquest':
    required = {'build-tests','count-law','checkpoint','growth','reporter','build-benchmark'}
passed = {r['phase'] for r in qualified if r.get('exit_code') == 0 and not r.get('timed_out') and not r.get('survivor')}
if not required <= passed:
    raise SystemExit('Required focused qualification is incomplete: '+','.join(sorted(required-passed)))
source = json.loads((OUT/f'{qualification}-qualification-request.json').read_text(encoding='utf-8'))
for name, expected in source['source_files'].items():
    if hashlib.sha256((ROOT/name).read_bytes()).hexdigest() != expected:
        raise SystemExit('Source changed after focused qualification: '+name)
benchmark = ROOT/'build/engine/poecraft_solver_benchmark.exe'
binary_sha = hashlib.sha256(benchmark.read_bytes()).hexdigest()
binary_pin = json.loads((OUT/f'{qualification}-built-binary-identity.json').read_text(encoding='utf-8'))
if binary_pin[str(Path('build/engine/poecraft_solver_benchmark.exe'))] != binary_sha:
    raise SystemExit('Benchmark changed after focused qualification')

commands = []
pins = {}
for arm in (['conquest5'] if mode == 'law3-conquest' else ['bow5','amulet3','conquest5','bow4-dirty','ring2']):
    old = json.loads((OLD/f'protected-fill-{arm}-candidate.receipt.json').read_text(encoding='utf-8'))
    argv = list(old['command']['argv'])
    argv[0] = str(benchmark)
    argv[argv.index('--corpus')+1] = str(OUT/'quality-cohort/manifest.json')
    if state_override:
        argv += ['--max-discovered-states', str(state_override)]
    for option, suffix in [('--output','.json'),('--partial-output','.partial.json'),('--strategy-output','.strategies')]:
        argv[argv.index(option)+1] = str(OUT/'comparisons'/f'{tag}-{arm}{suffix}')
    case = OUT/f'quality-cohort/{arm}.json'
    pins[arm] = {'case_sha256': hashlib.sha256(case.read_bytes()).hexdigest(),
                 'caps': json.loads(case.read_text(encoding='utf-8'))['caps'],
                 'purpose': 'capture exact private cap phase/counters' if arm != 'conquest5' else 'check native partial-progress proposal from original root'}
    commands.append((arm, argv, Path(old['command']['cwd']), old['command']['host_watchdog_seconds']))
request = {'created_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
           'qualification': qualification, 'source_patch_sha256': source['patch_sha256'],
           'mechanics_model': 'owner-approved single-side ordinary-total 8:3:1, provisional game model' if mode == 'law3-conquest' else None,
           'rare_reforge_count_law_version': 3 if mode == 'law3-conquest' else None,
           'benchmark_sha256': binary_sha, 'case_pins': pins,
           'serial': True, 'new_budget': True, 'budget_source':'native calculator_product_v1 defaults; no explicit state/discovery/expansion caps', 'simulator_trials': 0,
           'state_override': state_override, 'memory_limit_bytes':1073741824,
           'finish_seconds':120, 'native_deadline_seconds':150, 'host_watchdog_seconds':165,
           'final_exact_evaluation_seconds':30,
           'commands':[SolverCaseCommand(tuple(argv),cwd).canonical_document(host_watchdog_seconds=cap)
                       for _,argv,cwd,cap in commands]}
(OUT/f'{tag}-runtime-request.json').write_text(json.dumps(request,indent=2)+'\n',encoding='utf-8')

def run(name, argv, cwd, cap):
    def started(pid, token):
        event={'pid':pid,'identity_token':token,'started_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'watchdog_seconds':cap}
        (OUT/f'{tag}-{name}-started.json').write_text(json.dumps(event,indent=2)+'\n',encoding='utf-8')
        print(json.dumps({'phase':name,'started':event}),flush=True)
    receipt=run_isolated_process(argv,watchdog_seconds=cap,cwd=cwd,on_started=started)
    output=receipt.pop('output','')
    (OUT/f'{tag}-{name}.log').write_text(output,encoding='utf-8')
    (OUT/f'{tag}-{name}.receipt.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({'phase':name,'receipt':receipt}),flush=True)
    return receipt

# Resolve all requests before any timed solve. No changed laws, actions, prices
# or corpus inputs are introduced by these output-path substitutions.
for arm,argv,cwd,cap in commands:
    validate=argv[:argv.index('--output')] + ['--output',str(OUT/'comparisons'/f'{tag}-{arm}.preflight.json'),'--validate-only']
    if state_override: validate += ['--max-discovered-states',str(state_override)]
    receipt=run(arm+'-preflight',validate,cwd,cap)
    if receipt['exit_code'] != 0 or receipt['timed_out'] or receipt['survivor']:
        raise SystemExit('Preflight failed; no timed cases launched')

reporter_path=ROOT/'tools/ingest/poecraft_ingest/solver_reports.py'
spec=importlib.util.spec_from_file_location('sol61_modified_reports',reporter_path)
reports=importlib.util.module_from_spec(spec); spec.loader.exec_module(reports)
rows=[]
for arm,argv,cwd,cap in commands:
    receipt=run(arm,argv,cwd,cap)
    row={'arm':arm,'receipt':receipt}
    result_path=Path(argv[argv.index('--output')+1])
    if result_path.is_file() and not receipt['survivor']:
        result=json.loads(result_path.read_text(encoding='utf-8'))
        case=result['cases'][0]
        exact=case.get('exact_strategy_evaluation',{})
        row.update({'status':case.get('actual_status'),'summary':case.get('solve_summary'),
                    'service':case.get('solver_telemetry',{}).get('execution',{}).get('selective_completion_service'),
                    'graph':case.get('compiled_graph'),
                    'exact':{k:exact.get(k) for k in ['completed','status','total_expected_cost','success_probability','off_policy_mass','cost_complete']}})
        # Existing typed comparison owns the gate and exclusions; no second
        # comparison implementation is introduced here.
        baseline = f'growth-runtime-r1-{arm}' if state_override else f'protected-fill-{arm}-candidate'
        previous=json.loads((OLD/'comparisons'/f'{baseline}.json').read_text(encoding='utf-8'))
        comparison=reports.compare_runs(baseline,previous['cases'],tag,result['cases'])
        (OUT/f'{tag}-{arm}-comparison.json').write_text(json.dumps(comparison,indent=2)+'\n',encoding='utf-8')
        row['economic_gate']=comparison.get('economic_gate')
    rows.append(row)
    (OUT/f'{tag}-runtime-result.json').write_text(json.dumps(rows,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({'arm':arm,'receipt':receipt,'status':row.get('status'),'service':row.get('service'),'exact':row.get('exact'),'economic_gate':row.get('economic_gate')}),flush=True)
    if receipt['survivor']:
        raise SystemExit('Survivor reported; remaining serial cases stopped')
