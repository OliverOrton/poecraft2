"""Derive the two authorized long cases without changing short regressions."""
from copy import deepcopy
import hashlib
import json
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent
SOURCE = ROOT / 'fixtures/solver-quality-ladder/v1'
TARGET = HERE / 'long-profile'
OUT = ROOT / 'out/2026-09-09-empty-start-partial-continuation/F0-baseline'
REVISION = '317438392c88c0151f41e48b4b0323b06b1d0037'

def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes((json.dumps(value, indent=2) + '\n').encode('utf-8'))

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

manifest = json.loads((SOURCE / 'manifest.json').read_text(encoding='utf-8'))
manifest['corpus_id'] = 'poecraft2-useful-proof-time-long-v1'
profile_id = 'calculator-product-quality-ladder-proof-time-long-v1'
manifest['benchmark_identity_contract']['id'] = profile_id
manifest['comparison_profile']['id'] = profile_id
manifest['comparison_profile']['maximum_wall_seconds'] = 300
manifest['source_checkpoint']['engine_product_commit'] = REVISION
manifest['case_roles'] = {}
manifest['cases'] = []
manifest['owner_decisions'] += [
    'September 9 Useful Proof Time review: separate 240-second finish / '
    '300-second native watchdog development cases; 315 seconds is outer cleanup only.',
    'All other target, scope, economy, memory, work and evaluation controls '
    'are identical to the corresponding original short case.',
]
inputs = {}
for count in (4, 5):
    old_id = f'conquest-lamellar-allflame-clean-{count}-goal-product8'
    original_path = SOURCE / 'cases' / f'{old_id}.json'
    original = json.loads(original_path.read_text(encoding='utf-8'))
    derived = deepcopy(original)
    new_id = old_id + '-long240'
    derived.update(id=new_id, comparison_profile=profile_id,
                   watchdog_seconds=300, requested_bounded_finish_seconds=240)
    # These are the entire semantic differences, independently asserted here.
    restored = deepcopy(derived)
    for key in ('id', 'comparison_profile', 'watchdog_seconds',
                'requested_bounded_finish_seconds'):
        restored[key] = original[key]
    assert restored == original
    relative = f'cases/{new_id}.json'
    write_json(TARGET / relative, derived)
    manifest['cases'].append(relative)
    manifest['case_roles'][new_id] = original['corpus']['stratum'] + '_long240'
    inputs[str(original_path.relative_to(ROOT)).replace('\\', '/')] = sha(original_path)
write_json(TARGET / 'manifest.json', manifest)

OUT.mkdir(parents=True, exist_ok=False)
paths = [
    'build/engine/poecraft_solver_benchmark.exe',
    'build/engine/poecraft_engine.dll',
    'build/engine/poecraft_engine_tests.exe',
]
expected = json.loads((HERE / 'finish-provenance.json').read_text(encoding='utf-8'))
hashes = {}
for name in paths:
    source = ROOT / name
    digest = sha(source)
    assert digest == expected['source_and_artifact_sha256'][name], name
    shutil.copy2(source, OUT / source.name)
    hashes[name] = digest
write_json(OUT / 'hashes.json', {'revision': REVISION, 'binaries': hashes,
                               'original_case_sha256': inputs})
print(json.dumps({'profile': str(TARGET), 'baseline': str(OUT), 'hashes': hashes}))
