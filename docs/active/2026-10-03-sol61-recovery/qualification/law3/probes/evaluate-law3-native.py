"""One-off direct public-binding evaluation; supervision stays in solver_worker."""
from pathlib import Path
import datetime
import hashlib
import json
import os
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
OUT = Path(__file__).resolve().parent
MAIN = Path(r'C:\Users\Oliver\Documents\poecraft2')
sys.path.insert(0, str(ROOT / 'bindings/python'))
os.environ['POECRAFT_ENGINE_LIBRARY'] = str(ROOT / 'build/engine/poecraft_engine.dll')
import poecraft_engine as engine

request_path, arm = Path(sys.argv[1]), sys.argv[2]
request = json.loads(request_path.read_text(encoding='utf-8'))
for filename, expected in request['pins'].items():
    assert hashlib.sha256(Path(filename).read_bytes()).hexdigest() == expected, filename
entry = next(r for r in request['policies'] if r['arm'] == arm)
case = json.loads(Path(entry['case']).read_text(encoding='utf-8'))
snapshot = json.loads((MAIN / case['economy']['snapshot_path']).read_text(encoding='utf-8'))
assert snapshot['metadata']['content_sha256'] == case['economy']['content_sha256']
economy_document = {**snapshot, 'prices': {**snapshot['prices'], **case['economy']['manual_overrides']}}
graph = json.loads(Path(entry['graph']).read_text(encoding='utf-8'))
assert graph['base_state']['base_key'] == case['session']['base_metadata_path']
assert graph['base_state']['item_level'] == case['session']['item_level']
receipt = {
    'arm': arm, 'source_commit': request['source_commit'],
    'model': request['model'], 'policy_sha256': entry['graph_sha256'],
    'input_case_sha256': entry['case_sha256'], 'root': graph['base_state'],
    'economy': case['economy'], 'evaluation_options': request['evaluation_options'],
    'native_library_sha256': request['pins'][str(engine.engine_library_path())],
    'started_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
    'scope': 'ordinary strategy execution; no imported native incumbent provenance',
}
began = time.monotonic()
data = session = economy = strategy = None
try:
    data = engine.load_data(request['artifact'])
    session = data.create_session(case['session']['base_metadata_path'], case['session']['item_level'])
    economy = engine.load_economy(economy_document)
    strategy = session.compile_strategy(graph)
    result = strategy.evaluate(economy=economy, **request['evaluation_options'])
    raw = OUT / f"law3-native-evaluation-{arm}.raw.json"
    raw.write_text(json.dumps(result, separators=(',', ':')) + '\n', encoding='utf-8')
    totals = result['accounting']['totals']['per_invocation']
    terminals = result['terminals']
    proper = result['converged'] and abs(terminals['success'] - 1) < 1e-9 and all(
        terminals.get(k, 0) <= 1e-9 for k in ('failure', 'stop', 'action_not_applied',
                                           'no_matching_edge', 'unresolved'))
    receipt.update({
        'status': 'proper' if proper else 'not_proper',
        'converged': result['converged'], 'terminals': terminals,
        'cost_complete': totals['cost_complete'],
        'total_expected_cost': totals['total_expected_cost'],
        'comparable_expected_cost': totals['total_expected_cost'] if proper and totals['cost_complete'] else None,
        'expected_actions': totals['expected_actions'],
        'consumption': result['expected_consumption'], 'memory': result.get('memory'),
        'raw_result_path': str(raw), 'raw_result_sha256': hashlib.sha256(raw.read_bytes()).hexdigest(),
    })
except Exception as exc:
    receipt.update({'status': 'evaluation_error', 'error_type': type(exc).__name__, 'error': str(exc),
                    'comparable_expected_cost': None})
finally:
    for handle in (strategy, economy, session, data):
        if handle is not None:
            handle.close()
    receipt['elapsed_seconds'] = time.monotonic() - began
    destination = OUT / f'law3-native-evaluation-{arm}.json'
    destination.write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({k: receipt.get(k) for k in ('arm', 'status', 'converged', 'terminals',
        'cost_complete', 'comparable_expected_cost', 'expected_actions', 'elapsed_seconds',
        'error_type', 'error')}), flush=True)
