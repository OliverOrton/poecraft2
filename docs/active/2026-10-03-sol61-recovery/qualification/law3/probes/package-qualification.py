from pathlib import Path
import collections, datetime, gzip, hashlib, json, shutil, subprocess, sys

root = Path.cwd()
out = root / 'out/selective-cap-repair'
diag = Path(r'C:\Users\Oliver\AppData\Local\Temp\poecraft2-sol61-diagnostic-worktree\out\selective-cap-repair')
main = Path(r'C:\Users\Oliver\Documents\poecraft2')
dest = root / 'docs/active/2026-10-03-sol61-recovery/qualification'
dest.mkdir(parents=True, exist_ok=True)
read = lambda p: json.loads(p.read_text(encoding='utf-8'))
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
source = subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip()
files = []
def preserve(path, relative):
    target = dest / relative
    target.parent.mkdir(parents=True, exist_ok=True)
    raw = path.read_bytes()
    if len(raw) > 2_000_000:
        target = target.with_name(target.name + '.gz')
        target.write_bytes(gzip.compress(raw, mtime=0))
        assert gzip.decompress(target.read_bytes()) == raw
    else:
        target.write_bytes(raw)
    files.append({'original_path': str(path), 'portable_path': str(target.relative_to(dest)).replace('\\', '/'),
                  'original_sha256': hashlib.sha256(raw).hexdigest(), 'original_bytes': len(raw),
                  'stored_sha256': sha(target), 'stored_bytes': target.stat().st_size,
                  'encoding': 'gzip' if target.suffix == '.gz' else 'original bytes'})
if len(sys.argv) > 1 and sys.argv[1] == 'law3':
    assert source == 'b11f4c58f0a7c979f2c66b71892295795944e045'
    dest = dest / 'law3'
    dest.mkdir(exist_ok=True)
    native = read(out/'law3-native-r2-qualification-result.json')
    assert all(r['exit_code'] == 0 and not r['survivor'] and not r['timed_out'] for r in native)
    wasm_request = read(out/'law3-wasm-r1-request.json')
    assert wasm_request['source_commit'] == source and wasm_request['rare_reforge_count_law_version'] == 3
    for name, identity in wasm_request['source_files'].items():
        assert sha(root/name) == identity, name
    for name, identity in read(out/'law3-wasm-r1-identity.json').items():
        assert sha(root/name) == identity['sha256'], name
    for name in ('law3-product-preflight-receipts.json', 'law3-product-simulator-receipts.json',
                 'law3-product-web-receipts.json', 'law3-product-browser-receipts.json'):
        assert all(r['receipt']['exit_code'] == 0 and not r['receipt']['survivor'] and
                   not r['receipt']['timed_out'] for r in read(out/name))
    browser = read(out/'law3-product-completion-summary.json')
    assert browser['status'] == 'passed' and browser['usable_strategy']
    assert len(json.dumps(browser)) < 5000
    assert browser['solve_summary']['evaluated_policy_cost'] == 101311.35474896732
    assert not browser['page_errors'] and not browser['console_errors']
    assert browser['request_options'] == {'solve_profile': 'calculator_product_v1'}
    assert browser['wasm_sha256'] == read(out/'law3-wasm-r1-identity.json')['bindings\\wasm\\dist\\poecraft_engine.wasm']['sha256']
    policies = read(out/'law3-native-evaluation-request.json')['policies']
    matrix = []
    for policy in policies:
        arm = policy['arm']
        product = read(out/f'law3-product-preflight-{arm}.json')
        assert product['status'] == 'passed' and product['native_cost_parity']['status'] == 'passed'
        row = {'case': arm, 'policy_sha256': product['graph_sha256'],
               'native_cost_parity': product['native_cost_parity'],
               'terminals': product['exact']['terminals'],
               'expected_cost': product['exact']['total_expected_cost'],
               'expected_actions': product['exact']['expected_actions'],
               'wasm_memory': product['wasm_memory']}
        if arm.startswith('conquest5-'):
            sim = read(out/f'law3-product-simulator-{arm}.json')
            summary = sim['simulator']['summary']
            assert sim['status'] == 'passed' and summary['completed_runs'] == summary['success_count'] == 1000
            assert not sim['simulator_qualification']['failures']
            row.update({'simulator_summary': sim['simulator']['summary'], 'simulator_qualification': sim['simulator_qualification']})
        else:
            row['simulator_evidence'] = 'Unchanged strategy; prior qualification/qualification.json retained. Fresh law3 native and WASM exact costs are identical; no repeated Simulator run.'
        preserve(Path(product['graph_path']), f'policies/{arm}.strategy.json')
        matrix.append(row)
    selected = sorted(p for p in out.glob('law3-*') if p.is_file())
    for path in selected:
        preserve(path, 'receipts/' + path.name)
    for path in sorted((out/'comparisons').glob('law3-conquest-solve-r1-*.json')):
        preserve(path, 'receipts/native-solve/' + path.name)
    for path in sorted((out/'quality-cohort').glob('*.json')):
        preserve(path, 'cases/' + path.name)
    preserve(out/'product-cases/conquest5.json', 'cases/conquest5-product.json')
    for name in ('sol61-policy-qualification.ts', 'sol61-worker-bootstrap.mjs', 'sol61-browser-solve.mjs'):
        preserve(root/'apps/web/test'/name, 'probes/'+name)
    for name in ('qualify-production.py', 'measure-production.py', 'build-production-wasm-r3.py',
                 'qualify-product.py', 'evaluate-law3-native.py', 'package-qualification.py'):
        preserve(out/name, 'probes/'+name)
    overlap = read(out/'law3-latest-main-overlap.json')
    index = {
        'schema_version': 'sol61_law3_component_qualification_v1',
        'created_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
        'qualified_source_commit': source, 'base_main': '72448deaacca2797058de0559378685ba71a983f',
        'source_delta': read(out/'law3-source-delta-identity.json'),
        'source_delta_commits': ['5f7dbd274b38c93027593b5dcd9f7f9f053cb478',
            '1e7a50b272f7e09b2a67443398a34ec7fdef5a6c', 'afa83210196a2e07c00ccc6650881b44ae1b0e84',
            '6a9db0f817fdcce35e66ffb8a5e54ddc704fdfba', source],
        'model': {'rare_reforge_count_law_version': 3, 'action_refinement_contract_version': 5,
            'evaluator_certificate_version': 3,
            'assumption': 'Oliver-approved ordinary-total4/5/6 at8:3:1, count all preserved mods, fill selected side up to capacity; provisional game model'},
        'prior_model_receipts': '../qualification.json retained unchanged; historical model2 evidence is not relabeled',
        'native_focused_checks': {'count_law': 363236, 'checkpoint': 64, 'eldritch_growth': 84},
        'reporter_tests': 29, 'native_binary_identity': read(out/'law3-native-r2-built-binary-identity.json'),
        'native_library_sha256': read(out/'law3-native-evaluation-request.json')['pins'][str(root/'build/engine/poecraft_engine.dll')],
        'wasm_identity': read(out/'law3-wasm-r1-identity.json'), 'policy_matrix': matrix,
        'native_solve': [{
            'arm': row['arm'], 'receipt': row['receipt'], 'status': row['status'],
            'summary': {k: row['summary'].get(k) for k in ('converged', 'policy_available',
                'policy_status', 'termination', 'stop_cause', 'cap_hit_mask', 'expanded_states',
                'lower_bound', 'upper_bound', 'evaluated_policy_cost')},
            'candidate_service': row['service'], 'compiled_graph': row['graph'], 'exact': row['exact'],
            'economic_gate': row.get('economic_gate')}
            for row in read(out/'law3-conquest-solve-r1-runtime-result.json')],
        'fresh_browser_solve': browser, 'web': {'npm_test': 'passed', 'typescript': 'passed',
            'browser_channel': 'installed Chrome; repository-supported channel', 'rendered_visual_review': 'not requested'},
        'integration': {**overlap, 'publication_ready': False,
            'required_action': 'Combine source delta with latest main4ad40580 and retain LockObserve fix/UI; rebuild and qualify combined native/WASM before replacing main artifact.',
            'component_artifact_only': True},
        'exclusions': ['All costs conditional on the approved provisional model; no verified game-rate claim.',
            'Law2/law3 and budget/verification differences excluded by typed financial identity; no matched-model gate pass.',
            'Bounded feasible execution recovery, not certified optimal closure.',
            'No main edits, frozen data/price changes, npm dev restart, merge/push/deployment.',
            'Large-case cancellation latency not separately measured; existing web cancellation tests passed.'],
        'receipt_index': files,
        'portable_replay': 'gzip files decompress to byte-identical originals. Original paths/source pins retained; probes are snapshots. Archive alone does not authorize replay.'}
    serialized_index = json.dumps(index, indent=2)
    assert len(serialized_index.encode('utf-8')) < 250000
    (dest/'qualification.json').write_text(serialized_index+'\n', encoding='utf-8')
    print(json.dumps({'qualified_source': source, 'component_only': True,
        'files': len(files), 'stored_bytes': sum(p.stat().st_size for p in dest.rglob('*') if p.is_file()),
        'destination': str(dest)}))
    raise SystemExit(0)
assert source == 'afa83210196a2e07c00ccc6650881b44ae1b0e84'
browser = read(out / 'production-product-r2-browser-bow5.json')
assert browser['status'] == 'passed' and browser['usable_strategy']
assert browser['summary']['policy_status'] == 'bounded_feasible'
assert browser['summary']['evaluated_policy_cost'] == 4071502.8714904664
assert browser['trace']['request']['solve_options'] == {'solve_profile': 'calculator_product_v1'}
assert not browser['page_errors'] and not browser['console_errors']
browser_process = read(out / 'production-product-r2-browser-bow5.receipt.json')
assert browser_process['exit_code'] == 0 and not browser_process['survivor']
wasm_request = read(out / 'production-wasm-r3-request.json')
assert wasm_request['source_commit'] == source
for name, identity in wasm_request['source_files'].items():
    assert sha(root / name) == identity, name
for name, identity in read(out / 'production-wasm-r3-identity.json').items():
    assert sha(root / name) == identity['sha256'], name
for name, identity in read(out / 'production-r5-qualification-request.json')['source_files'].items():
    assert sha(root / name) == identity, name
native = read(out / 'production-r5-qualification-result.json')
assert all(r['exit_code'] == 0 and not r['timed_out'] and not r['survivor'] for r in native)
assert read(out / 'budget-identity-r3-tests.receipt.json')['exit_code'] == 0
for receipts in ['production-product-r1-preflight-receipts.json', 'production-product-r1-simulator-receipts.json',
                 'production-product-r3-web-receipts.json']:
    assert all(r['receipt']['exit_code'] == 0 and not r['receipt']['survivor'] for r in read(out / receipts))

patterns = ['production-r5-*', 'production-wasm-r3-*', 'production-product-*', 'budget-identity-r3-tests.*',
            'production-fixed-bow4-r2-runtime-*', 'production-fixed-bow4-r2-bow4-dirty.receipt.json',
            'production-default-r1-runtime-*', 'production-matched-r1-runtime-*']
selected = sorted({p for pattern in patterns for p in out.glob(pattern) if p.is_file()})
for path in selected: preserve(path, 'receipts/' + path.name)
for path in sorted((out / 'quality-cohort').glob('*.json')): preserve(path, 'cases/' + path.name)
for name in ['sol61-policy-qualification.ts', 'sol61-worker-bootstrap.mjs', 'sol61-browser-solve.mjs']:
    preserve(root / 'apps/web/test' / name, 'probes/' + name)
preserve(out / 'qualify-product.py', 'probes/qualify-product.py')

matrix = []
for arm in ['bow4-dirty', 'conquest5', 'amulet3', 'bow5']:
    product = read(out / ('production-product-r1-simulator-' + arm + '.json'))
    assert product['status'] == 'passed' and product['simulator']['summary']['completed_runs'] == 1000
    graph = Path(product['graph_path'])
    preserve(graph, 'policies/' + arm + '.strategy.json')
    matrix.append({'case': arm, 'native_product_cost_parity': product['native_cost_parity'],
                   'exact': product['exact'], 'simulator': product['simulator'],
                   'simulator_qualification': product['simulator_qualification'],
                   'product_wasm_memory': product['wasm_memory'], 'wall_ms': product['wall_ms']})

old_path = main / 'out/dual-lane-target-support/U4/current-ordinary-L/cases/cb01-cross-base-product8-long240.json'
old = read(old_path)['cases'][0]['exact_strategy_evaluation']['result']
historical = read(diag / 'historical-conquest-currentlaw.r3.product-result.json')
prices = read(main / 'apps/web/public/economy/snapshots/de282eecf6cfdab50666412b94791b68634944ff31921b95e52eeae7758c0fe0.json')['prices']
a = {x['key']: x['quantity'] for x in old['expected_consumption']}
b = {x['key']: x['quantity'] for x in historical['exact']['consumption']}
rows = [{'key': key, 'old_quantity': a.get(key, 0), 'current_quantity': b.get(key, 0), 'price': prices[key],
         'cost_delta': (b.get(key, 0) - a.get(key, 0)) * prices[key],
         'current_cost': b.get(key, 0) * prices[key]} for key in sorted(a.keys() | b.keys())]
old_cost = sum(a[key] * prices[key] for key in a)
new_cost = sum(b[key] * prices[key] for key in b)
assert abs(old_cost - 85558.70618560436) < 1e-7
assert abs(new_cost - historical['exact']['total_expected_cost']) < 1e-7
old_build = read(diag / 'wasm-product-qualification-request.json')
kernel_pins = {}
for name in ['engine/src/reforge_count_law.hpp', 'engine/src/solver_reforge.cpp', 'engine/src/actions_basic.cpp']:
    kernel_pins[name] = old_build['source_file_sha256'][name.replace('/', '\\')]
    assert sha(root / name) == kernel_pins[name]
for name in ['historical-conquest-currentlaw.r3.product-result.json', 'historical-conquest-currentlaw.r3.product.receipt.json',
             'historical-price-only-repricing.json', 'historical-conquest5-goal-order-adapted.strategy.adaptation.json',
             'matching-wasm-module-identity.json', 'wasm-product-qualification-request.json']:
    preserve(diag / name, 'historical/' + name)
preserve(old_path, 'historical/original-old-law-case-report.json')
preserve(diag / 'historical-conquest5-goal-order-adapted.strategy.json', 'historical/conquest5.strategy.json')
decomposition = {'old_total': old_cost, 'price_only_repricing_total': read(diag / 'historical-price-only-repricing.json')['old_expected_consumption_repriced_with_trace_prices'],
    'current_law_total': new_cost, 'change': new_cost - old_cost, 'rows': rows,
    'old_expected_actions': old['accounting']['totals']['per_invocation']['expected_actions'],
    'current_expected_actions': historical['exact']['expected_actions'],
    'historical_current_law_modules': read(diag / 'matching-wasm-module-identity.json'), 'kernel_source_pins_same_as_final_build': kernel_pins,
    'law_participation': 'Ordinary equipment rare reforge target counts use 4/5/6 with weights8/3/1. Dominant Eldritch Chaos uses separate new-side2/3 counts at1/2 each, as native solver_reforge.cpp1998-2003. Dominance-absent Eldritch Chaos falls back to ordinary rare law. This is source scope, not new mechanics research.',
    'fresh_recurrence_evidence': 'The historical r3 ordinary product evaluator compiled the graph and called strategyEvaluate with a fresh immutable economy document; old consumption is absent from that invocation. Current quantities differ. This receipt is separate from preserved linear price-only repricing.',
    'limitations': ['The native request does not admit caller JSON as trusted programme provenance.', 'No action-node counterfactual law ablation was run; weighted consumption differences reconcile cost, not a causal decomposition of each transition.', 'Historical diagnostic source0c76/WASM4f338e retains its own identity; it is not silently relabeled finalafa/WASMa438. Kernels above are byte-identical.']}
(dest / 'historical/cost-decomposition.json').write_text(json.dumps(decomposition, indent=2) + '\n', encoding='utf-8')

manifest = {'schema_version': 'sol61_product_recovery_qualification_v1', 'created_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
    'qualified_source_commit': source, 'base_main': '72448deaacca2797058de0559378685ba71a983f',
    'repair_commits': ['5f7dbd274b38c93027593b5dcd9f7f9f053cb478', '1e7a50b272f7e09b2a67443398a34ec7fdef5a6c', source],
    'wasm_identity': read(out / 'production-wasm-r3-identity.json'), 'native_binary_identity': read(out / 'production-r5-built-binary-identity.json'),
    'native_focused_checks': 52359, 'reporter_tests': 27, 'web': {'npm_test': 'passed', 'wasm_smoke': '37/37', 'typescript': 'passed',
        'browser_channel': 'installed chrome; existing repository-supported local channel', 'Finish_Cancel': 'actual worker dispatch plus actual WASM cancellation smoke passed'},
    'policy_matrix': matrix, 'fresh_browser_solve': {'source_commit': browser['source_commit'], 'wasm_sha256': browser['wasm_sha256'],
        'browser_version': browser['browser_version'], 'request_options': browser['trace']['request']['solve_options'],
        'native_default_limits': {'discovered_states': 800000, 'expanded_states': 200000, 'solver_owned_bytes': 1073741824, 'authority': 'source-pinned native calculator_product_v1 resolver; product omits overrides'},
        'summary': {k: browser['summary'][k] for k in ['converged', 'policy_available', 'policy_status', 'termination', 'stop_cause', 'cap_hit_mask', 'expanded_states', 'lower_bound', 'upper_bound', 'evaluated_policy_cost']},
        'worker': {k: browser['trace']['worker'][k] for k in ['work_policy', 'step_transport', 'step_count', 'max_step_ms', 'finalization_ms']},
        'ui_milestones': browser['trace']['ui_milestones'], 'memory': browser['memory'], 'process_receipt': browser_process,
        'operation_types': sorted({n.get('operation', {}).get('type', '') for n in browser['graph']['nodes']} - {''}),
        'page_errors': browser['page_errors'], 'console_errors': browser['console_errors']},
    'disposition': 'Bounded feasible root-policy recovery; exact independent cost is qualified, optimal closure is not. Feeder/Lock/Recombination and multi-goal source gains from main preserved.',
    'exclusions': ['Budget-changing200k/800k runs are not interchangeable financial comparisons. run_overrides and resolved_checker_caps remain comparison identities.',
        'Bow5:338success+662action-limit-censored; capped sample mean is not uncappedEV.',
        'No pinned-Playwright-browser parity or rendered visual review claimed; installed Chrome used.',
        'Optional current-law blocker constructor remains separately refused/unresolved; it does not erase retained checked fallback.',
        'Main workspace, protected0, frozen SQLite/runtime/prices and existing npm dev untouched; no merge/push/deployment.'],
    'receipt_index': files, 'portable_replay': 'Original paths and raw identities are preserved. gzip files decompress to byte-identical originals. Probes are snapshots of apps/web/test or out paths; restore to those locations in an isolated checkout before replay. No replay is authorized by this archive alone.'}
(dest / 'qualification.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
print(json.dumps({'qualified_source': source, 'portable_files': len(files), 'portable_bytes': sum(p.stat().st_size for p in dest.rglob('*') if p.is_file()),
    'browser_cost': browser['summary']['evaluated_policy_cost'], 'historical_old_cost': old_cost, 'historical_current_cost': new_cost,
    'historical_cost_change': new_cost - old_cost, 'destination': str(dest)}))
