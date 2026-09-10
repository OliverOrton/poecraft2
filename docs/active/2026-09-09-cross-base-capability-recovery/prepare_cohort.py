"""One-shot immutable derivation through existing native generator/Lab owners."""
from copy import deepcopy
import json
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent
OUT = ROOT / 'out/2026-09-09-cross-base-capability-recovery'
sys.path[:0] = [str(ROOT / 'tools/ingest'), str(ROOT / 'bindings/python')]
from poecraft_ingest.natural_t1_corpus import generate_corpus
from poecraft_ingest.solver_lab_cases import normalize_imported_case, build_local_manifest
from poecraft_ingest.solver_worker import sha256_file

def read(path):
    return json.loads(path.read_text(encoding='utf-8'))

def write(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('x', encoding='utf-8', newline='\n') as stream:
        stream.write(json.dumps(value, indent=2) + '\n')

def main():
    if (HERE / 'core').exists() or (HERE / 'heldout-generated').exists():
        raise SystemExit('Refusing to overwrite a frozen cohort')
    head = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    assert head == 'be553608ecabde28a3dec07856255911459ec97d', head
    baseline = OUT / 'baseline-bin'
    baseline.mkdir(parents=True, exist_ok=False)
    expected = read(ROOT / 'out/2026-09-09-empty-start-partial-continuation/F7-build-identity.json')
    identities = {}
    for name in ('poecraft_solver_benchmark.exe', 'poecraft_engine.dll', 'poecraft_engine_tests.exe'):
        source = ROOT / 'build/engine' / name
        actual = sha256_file(source)
        assert actual == expected[source.relative_to(ROOT).as_posix()], name
        shutil.copy2(source, baseline / name)
        identities[name] = actual
    write(OUT / 'A1-baseline-binary-identity.json', {'source_commit': head, 'files': identities})
    template = read(ROOT / 'fixtures/solver-quality-ladder/v1/cases/conquest-lamellar-allflame-clean-4-goal-product8.json')
    base_manifest = read(ROOT / 'docs/active/2026-09-09-empty-start-partial-continuation/long-profile/manifest.json')
    selection = read(HERE / 'research-inputs/diagnostic_cohort.json')
    config = read(ROOT / 'fixtures/solver-natural-t1/v1/generator-config.json')
    wand = 'Metadata/Items/Weapons/OneHandWeapons/Wands/Wand16'
    shield = 'Metadata/Items/Armours/Shields/ShieldStr17'
    config.update(corpus_id='cross-base-recovery-heldout-native-generation-v1',
        output_directory=(HERE / 'heldout-generated').relative_to(ROOT).as_posix(),
        economy=deepcopy(template['economy']), seed=20260909, watchdog_seconds=60,
        case_watchdog_seconds=300, resource_caps=deepcopy(template['caps']),
        verification=deepcopy(template['verification']),
        base_selection={'paths': [wand, shield]}, base_pools={},
        feasibility_probes=[], explicit_cases=[], coverage={},
        strata=[{'id': 'heldout-wand-pss', 'corpus': 'heldout', 'cases': 1,
            'goal_count': 3, 'side_compositions': ['PSS'], 'base_paths': [wand]},
            {'id': 'heldout-shield-pps', 'corpus': 'heldout', 'cases': 1,
            'goal_count': 3, 'side_compositions': ['PPS'], 'base_paths': [shield]}])
    write(HERE / 'heldout-generator-config.json', config)
    generated = generate_corpus(HERE / 'heldout-generator-config.json')
    heldout_entries = [{'id': f'HO{i:02}',
        'source_case': (HERE / 'heldout-generated/cases' / (case['id'] + '.json')).relative_to(ROOT).as_posix(),
        'source_role': 'new complete pool family reserved before implementation; no solver outcome exposed'}
        for i, case in enumerate(generated['cases'], 1)]
    derivations = []
    for folder, entries in [('core', selection['cases']), ('heldout', heldout_entries)]:
        manifest = build_local_manifest(base_manifest, case_id='placeholder', corpus_id=f'cross-base-capability-recovery-{folder}-v1')
        manifest.update(cases=[], case_roles={}, source_checkpoint={'engine_product_commit': head,
            'mechanics_authority': 'unchanged_current_implemented_contract', 'local_case_authoring': 'existing_native_generator_and_lab_case_clone'})
        manifest['comparison_profile']['id'] = 'cross-base-capability-recovery-product8-long240-v1'
        manifest['benchmark_identity_contract']['id'] = 'cross-base-capability-recovery-product8-long240-v1'
        manifest['owner_decisions'] = ['Derived immutable current targets; overrides and original roles in derivations.json.',
            'Ordinary baseline enables native retention reuse and omits proof handoff; 315-second outer cleanup.',
            'Legacy Mirage costs are not directly comparable with these Allflame targets.']
        manifest['execution_activation'] = {'native_retention_diagnostic': 'reuse', 'proof_handoff_seconds': 0,
            'outer_cleanup_seconds': 315, 'generated_imprint': False, 'economic_restart': False,
            'goal_progress_gated_reforges': True}
        for entry in entries:
            parent = ROOT / entry['source_case']
            original = read(parent)
            case = deepcopy(original)
            case_id = entry['id'].lower() + '-cross-base-product8-long240'
            case.update(id=case_id, description=f"{entry['id']}: {original['id']}",
                comparison_profile='cross-base-capability-recovery-product8-long240-v1',
                watchdog_seconds=300, requested_bounded_finish_seconds=240,
                economy=deepcopy(template['economy']), caps=deepcopy(template['caps']),
                verification=deepcopy(template['verification']), expected=deepcopy(template['expected']),
                bounded_best_policy_contract=deepcopy(template['bounded_best_policy_contract']))
            case['approval_status'] = 'selected_cross_base_programme_2026_09_09'
            case = normalize_imported_case(case, template)
            relative = 'cases/' + case_id + '.json'
            write(HERE / folder / relative, case)
            manifest['cases'].append(relative)
            manifest['case_roles'][case_id] = entry['source_role']
            derivations.append({'id': entry['id'], 'case_id': case_id, 'cohort': folder,
                'parent_path': entry['source_case'], 'parent_sha256': sha256_file(parent),
                'derived_path': (HERE / folder / relative).relative_to(ROOT).as_posix(),
                'derived_sha256': sha256_file(HERE / folder / relative),
                'source_role': entry['source_role'], 'original_corpus_metadata': original.get('corpus'),
                'overrides': {key: {'before': original.get(key), 'after': value}
                    for key, value in case.items() if original.get(key) != value}})
        write(HERE / folder / 'manifest.json', manifest)
    write(HERE / 'derivations.json', {'source_commit': head, 'entries': derivations,
        'gate_frozen_before_implementation': 'New valid exact capability or >=20% same-budget target-time/gap improvement on two non-Conquest families; no lost policy/exact answer or material core/heldout regression.',
        'holdout_use': 'Native feasibility only before implementation; reserve solver outcomes for final qualification.'})
    print(json.dumps({'core_cases': 12, 'heldout_cases': len(heldout_entries), 'generator_counts': generated['report']['counters']}))

if __name__ == '__main__':
    main()
