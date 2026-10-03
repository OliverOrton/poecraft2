from pathlib import Path
import datetime, hashlib, json, subprocess, sys, os

root = Path.cwd()
out = root / 'out/selective-cap-repair'
sys.path.insert(0, str(root / 'tools/ingest'))
from poecraft_ingest.solver_worker import SolverCaseCommand, run_isolated_process

phase = sys.argv[1]
assert phase in ('preflight', 'simulator', 'web', 'browser')
if phase in ('web', 'browser'):
    os.environ['POECRAFT_TEST_BROWSER_CHANNEL'] = 'chrome'
revision = sys.argv[2] if len(sys.argv) > 2 else 'r1'
assert revision in ('r1', 'r2', 'r3')
tag = 'production-product-' + revision + '-' + phase
artifact = root / 'data/runtime-snapshots/82fb60a25160877bb6da0c6494ceb52370b89f0b47f422d61d49e509dfe7326d'
main = Path(r'C:\Users\Oliver\Documents\poecraft2')
records = []
for filename in ('production-fixed-bow4-r2-runtime-result.json', 'production-default-r1-runtime-result.json'):
    for record in json.loads((out / filename).read_text(encoding='utf-8')):
        if record.get('exact') and record['arm'] in ('bow4-dirty', 'bow5', 'conquest5', 'amulet3'):
            records.append(record)
assert {r['arm'] for r in records} == {'bow4-dirty', 'bow5', 'conquest5', 'amulet3'}
records.sort(key=lambda r: ['bow4-dirty', 'conquest5', 'amulet3', 'bow5'].index(r['arm']))
wasm_identity = json.loads((out / 'production-wasm-r3-identity.json').read_text(encoding='utf-8'))
for name, identity in wasm_identity.items():
    assert hashlib.sha256((root / name).read_bytes()).hexdigest() == identity['sha256']
commands = []
web = root / 'apps/web'
if phase == 'preflight':
    commands.append(('data-bundle', ['cmd', '/c', 'npm', 'run', 'build:data'], web, 180))
if phase in ('preflight', 'simulator'):
    trials = 0 if phase == 'preflight' else 1000
    for r in records:
        arm = r['arm']
        result_path = out / (tag + '-' + arm + '.json')
        command = ['node', '--import', 'tsx', 'test/sol61-policy-qualification.ts',
                   r['graph']['strategy_output_path'], str(out / 'quality-cohort' / (arm + '.json')),
                   str(artifact), str(main), str(result_path), str(trials),
                   str(r['exact']['total_expected_cost'])]
        watchdog = 120 if not trials else (7200 if arm == 'bow5' else 1800)
        commands.append((arm, command, web, watchdog))
if phase == 'web':
    commands.extend([('npm-test', ['cmd', '/c', 'npm', 'test'], web, 900),
                     ('typescript', ['cmd', '/c', 'npx', 'tsc', '--noEmit'], web, 300)])
if phase == 'browser':
    commands.append(('bow5', ['node', 'test/sol61-browser-solve.mjs',
        str(out / 'quality-cohort/bow5.json'), str(out / (tag + '-bow5.json'))], web, 420))
pins = {}
for path in [web / 'test/sol61-policy-qualification.ts', web / 'test/sol61-worker-bootstrap.mjs',
             out / 'quality-cohort/manifest.json', artifact / 'manifest.json',
             out / 'production-wasm-r3-request.json']:
    pins[str(path)] = hashlib.sha256(path.read_bytes()).hexdigest()
if phase == 'browser':
    path = web / 'test/sol61-browser-solve.mjs'
    pins[str(path)] = hashlib.sha256(path.read_bytes()).hexdigest()
for r in records:
    for path in [Path(r['graph']['strategy_output_path']), out / 'quality-cohort' / (r['arm'] + '.json')]:
        pins[str(path)] = hashlib.sha256(path.read_bytes()).hexdigest()
request = {'created_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
           'source_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip(),
           'wasm_identity': wasm_identity, 'pins': pins, 'LOCAL_owner': 'Sol6.1 serial product qualification',
           'browser_channel': os.environ.get('POECRAFT_TEST_BROWSER_CHANNEL'),
           'commands': [{'arm': arm, 'command': SolverCaseCommand(tuple(args), cwd).canonical_document(
               host_watchdog_seconds=watchdog)} for arm, args, cwd, watchdog in commands]}
(out / (tag + '-request.json')).write_text(json.dumps(request, indent=2) + '\n', encoding='utf-8')
receipts = []
for arm, args, cwd, watchdog in commands:
    def started(pid, token):
        e = {'phase': phase, 'arm': arm, 'pid': pid, 'identity_token': token,
             'started_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
             'watchdog_seconds': watchdog, 'LOCAL_owner': request['LOCAL_owner']}
        (out / (tag + '-' + arm + '-started.json')).write_text(json.dumps(e, indent=2) + '\n', encoding='utf-8')
        print(json.dumps({'started': e}), flush=True)
    receipt = run_isolated_process(args, cwd=cwd, watchdog_seconds=watchdog, on_started=started)
    log = receipt.pop('output', '')
    (out / (tag + '-' + arm + '.log')).write_text(log, encoding='utf-8')
    (out / (tag + '-' + arm + '.receipt.json')).write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8')
    receipts.append({'arm': arm, 'receipt': receipt})
    (out / (tag + '-receipts.json')).write_text(json.dumps(receipts, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'arm': arm, 'receipt': receipt, 'tail': log[-2400:]}), flush=True)
    if receipt['exit_code'] != 0 or receipt['timed_out'] or receipt['survivor']:
        sys.exit(1)
