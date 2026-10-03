from pathlib import Path
import datetime
import hashlib
import json
import os
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
OUT = Path(__file__).resolve().parent
MAIN = Path(r'C:\Users\Oliver\Documents\poecraft2')
sys.path.insert(0, str(MAIN / 'tools/ingest'))
from poecraft_ingest.solver_worker import SolverCaseCommand, run_isolated_process

tag = sys.argv[1]
mode = sys.argv[2] if len(sys.argv) > 2 else 'recovery'
assert mode in ('recovery', 'law3')
prepare_only = '--prepare-only' in sys.argv[3:]
paths = subprocess.check_output(['git','diff','HEAD','--name-only','--','engine','tools/ingest/poecraft_ingest/solver_reports.py','tools/ingest/tests/test_solver_reports.py'], cwd=ROOT,text=True).splitlines()
patch = subprocess.check_output(['git', 'diff', 'HEAD', '--', *paths], cwd=ROOT)
(OUT / f'{tag}-source.patch').write_bytes(patch)
commands = [
    ('build-tests', ['powershell', '-NoProfile', '-File', 'scripts/dev-engine.ps1', '-Task', 'Tests', '-Jobs', '2'], 900),
    ('growth', [str(ROOT/'build/engine/poecraft_engine_tests.exe'), '--solver-growth-only'], 180),
    ('protected-fill', [str(ROOT/'build/engine/poecraft_engine_tests.exe'), '--solver-protected-fill-only'], 180),
    ('protected-finder', [str(ROOT/'build/engine/poecraft_engine_tests.exe'), '--solver-protected-finder-only'], 180),
    ('cap-diagnosis', [str(ROOT/'build/engine/poecraft_engine_tests.exe'), '--solver-integrity-only', 'selective-cap-diagnosis'], 180),
    ('continuity', [str(ROOT/'build/engine/poecraft_engine_tests.exe'), '--solver-integrity-only', 'continuity'], 180),
    ('blocker-growth', [str(ROOT/'build/engine/poecraft_engine_tests.exe'), '--solver-blocker-growth-only'], 180),
    ('build-benchmark', ['powershell', '-NoProfile', '-File', 'scripts/dev-engine.ps1', '-Task', 'Benchmark', '-Jobs', '2'], 900),
]
if mode == 'law3':
    artifact = ROOT / 'data/runtime-snapshots/82fb60a25160877bb6da0c6494ceb52370b89f0b47f422d61d49e509dfe7326d'
    assert (artifact / 'manifest.json').is_file()
    os.environ['PYTHONPATH'] = str(ROOT / 'tools/ingest') + ';' + str(ROOT / 'bindings/python')
    scratch = OUT / ('tmp-' + tag)
    scratch.mkdir(exist_ok=True)
    os.environ['TEMP'] = os.environ['TMP'] = str(scratch)
    commands = [
        commands[0],
        ('count-law', [str(ROOT/'build/engine/poecraft_engine_tests.exe'), '--reforge-count-law-only'], 180),
        ('checkpoint', [str(ROOT/'build/engine/poecraft_engine_tests.exe'), '--solver-checkpoint-only', str(artifact)], 180),
        commands[1],
        ('reporter', [sys.executable, '-m', 'pytest', 'tools/ingest/tests/test_solver_reports.py', '-q',
                      '--basetemp', str(OUT/('pytest-' + tag))], 180),
        commands[-1],
    ]
source_paths = [ROOT/p for p in paths]
if mode == 'law3':
    source_paths = [p for p in (ROOT/'engine').rglob('*')
                    if p.is_file() and p.suffix in ('.cpp', '.hpp', '.h')]
    source_paths += [ROOT/p for p in ('engine/CMakeLists.txt',
                     'bindings/wasm/wasm_api.cpp', 'scripts/dev-engine.ps1',
                     'scripts/build-wasm.ps1', 'tools/ingest/poecraft_ingest/solver_reports.py',
                     'tools/ingest/tests/test_solver_reports.py')]
request = {
    'created_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
    'head': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
    'patch_sha256': hashlib.sha256(patch).hexdigest(), 'patch_bytes': len(patch),
    'source_files': {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                     for p in source_paths},
    'commands': [SolverCaseCommand(tuple(argv), ROOT).canonical_document(host_watchdog_seconds=cap)
                 for _, argv, cap in commands],
    'LOCAL_owner': 'Sol6.1 repair', 'serial': True,
    'qualification_mode': mode, 'rare_reforge_count_law_version': 3 if mode == 'law3' else None,
    'temporary_directory': str(scratch) if mode == 'law3' else None,
}
(OUT/f'{tag}-qualification-request.json').write_text(json.dumps(request, indent=2)+'\n', encoding='utf-8')
if prepare_only:
    print(json.dumps({'prepared': str(OUT/f'{tag}-qualification-request.json'),
                      'phases': [name for name, _, _ in commands], 'execution_started': False}), flush=True)
    sys.exit(0)
results = []
failed = []
for name, argv, cap in commands:
    def started(pid, token):
        event = {'pid': pid, 'identity_token': token,
                 'started_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
                 'watchdog_seconds': cap}
        (OUT/f'{tag}-{name}-started.json').write_text(json.dumps(event, indent=2)+'\n', encoding='utf-8')
        print(json.dumps({'phase': name, 'started': event}), flush=True)
    receipt = run_isolated_process(argv, watchdog_seconds=cap, cwd=ROOT, on_started=started)
    output = receipt.pop('output', '')
    (OUT/f'{tag}-{name}.log').write_text(output, encoding='utf-8')
    (OUT/f'{tag}-{name}.receipt.json').write_text(json.dumps(receipt, indent=2)+'\n', encoding='utf-8')
    results.append({'phase': name, **receipt, 'log': str(OUT/f'{tag}-{name}.log')})
    (OUT/f'{tag}-qualification-result.json').write_text(json.dumps(results, indent=2)+'\n', encoding='utf-8')
    print(json.dumps({'phase': name, 'receipt': receipt, 'tail': output[-1600:]}), flush=True)
    if receipt.get('exit_code') != 0 or receipt.get('survivor') or receipt.get('timed_out'):
        failed.append(name)
        if name.startswith('build-') or receipt.get('survivor'):
            sys.exit(1)
identity = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in [ROOT/'build/engine/poecraft_engine_tests.exe', ROOT/'build/engine/poecraft_solver_benchmark.exe']}
(OUT/f'{tag}-built-binary-identity.json').write_text(json.dumps(identity, indent=2)+'\n', encoding='utf-8')
print(json.dumps({'qualified_binaries': identity}), flush=True)
print(json.dumps({'failed_selectors': failed,
                  'required_selectors_passed': not any(p != 'blocker-growth' for p in failed)}), flush=True)
sys.exit(1 if failed else 0)
