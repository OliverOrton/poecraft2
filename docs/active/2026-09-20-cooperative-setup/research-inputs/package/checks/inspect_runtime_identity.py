#!/usr/bin/env python3
"""Read-only, offline inspection of the existing frozen-runtime identity contract.

Not an ingest implementation, native evaluator, or replacement acceptance gate.
Exit 0: exact pinned manifest and both payloads match. Exit 1: mismatch.
Exit 2: missing/malformed input. Nothing under the artifact directory is written.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import re
from pathlib import Path
from typing import Any

PAYLOADS = ('game-data.json', 'strings.json')

def _pairs(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f'duplicate JSON key: {key!r}')
        result[key] = value
    return result

def load_object(path: Path) -> dict[str, Any]:
    def reject_constant(value: str) -> None:
        raise ValueError(f'non-finite JSON value: {value}')
    value = json.loads(path.read_text(encoding='utf-8'), object_pairs_hook=_pairs,
                       parse_constant=reject_constant)
    if not isinstance(value, dict):
        raise ValueError(f'{path}: expected a JSON object')
    return value

def fingerprint(path: Path) -> dict[str, Any]:
    digest = hashlib.sha256()
    size = 0
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            size += len(chunk)
            digest.update(chunk)
    return {'sha256': digest.hexdigest(), 'byte_size': size}

def _sha(value: Any, label: str) -> str:
    if not isinstance(value, str) or re.fullmatch('[0-9a-f]{64}', value) is None:
        raise ValueError(f'{label}: expected lowercase SHA-256')
    return value

def _file_record(value: Any, label: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise ValueError(f'{label}: expected an object')
    digest = _sha(value.get('sha256'), label + '.sha256')
    size = value.get('byte_size')
    if type(size) is not int or size < 0:
        raise ValueError(f'{label}.byte_size: expected a nonnegative integer')
    return {'sha256': digest, 'byte_size': size}

def field_differences(before: Any, after: Any) -> list[dict[str, Any]]:
    differences: list[dict[str, Any]] = []
    def visit(a: Any, b: Any, pointer: str) -> None:
        if type(a) is not type(b):
            differences.append({'pointer': pointer or '/', 'kind': 'type_or_value',
                                'before': a, 'after': b})
        elif isinstance(a, dict):
            for key in sorted(set(a) | set(b)):
                p = pointer + '/' + key.replace('~', '~0').replace('/', '~1')
                if key not in a:
                    differences.append({'pointer': p, 'kind': 'added', 'after': b[key]})
                elif key not in b:
                    differences.append({'pointer': p, 'kind': 'removed', 'before': a[key]})
                else:
                    visit(a[key], b[key], p)
        elif isinstance(a, list):
            if len(a) != len(b):
                differences.append({'pointer': pointer or '/', 'kind': 'array_length',
                                    'before': len(a), 'after': len(b)})
            for i, (x, y) in enumerate(zip(a, b)):
                visit(x, y, f'{pointer}/{i}')
        elif a != b:
            differences.append({'pointer': pointer or '/', 'kind': 'value',
                                'before': a, 'after': b})
    visit(before, after, '')
    return differences

def inspect(lock_path: Path, artifact: Path,
            reference_manifest: Path | None = None) -> dict[str, Any]:
    lock = load_object(lock_path)
    runtime = lock.get('runtime_artifact')
    if not isinstance(runtime, dict):
        raise ValueError('lock.runtime_artifact: expected an object')
    expected_manifest = _sha(runtime.get('manifest_sha256'), 'runtime_artifact.manifest_sha256')
    timestamp = runtime.get('generated_at_utc')
    if not isinstance(timestamp, str) or not timestamp:
        raise ValueError('runtime_artifact.generated_at_utc: expected a nonempty literal string')
    files = runtime.get('files')
    if not isinstance(files, dict) or set(files) != set(PAYLOADS):
        raise ValueError('runtime_artifact.files: expected exactly game-data.json and strings.json')
    manifest_path = artifact / 'manifest.json'
    manifest = load_object(manifest_path)
    actual_manifest = fingerprint(manifest_path)
    manifest_files = manifest.get('files')
    if not isinstance(manifest_files, dict):
        raise ValueError('generated manifest.files: expected an object')
    results: dict[str, Any] = {}
    for name in PAYLOADS:
        expected = _file_record(files.get(name), 'lock.files.' + name)
        declared = _file_record(manifest_files.get(name), 'manifest.files.' + name)
        actual = fingerprint(artifact / name)
        results[name] = {'expected': expected, 'declared': declared, 'actual': actual,
                         'matches_lock': actual == expected,
                         'matches_manifest_declaration': actual == declared}
    actual_time = manifest.get('generated_at_utc')
    timestamp_equal = type(actual_time) is str and actual_time == timestamp
    matches = actual_manifest['sha256'] == expected_manifest
    payload_matches = all(r['matches_lock'] and r['matches_manifest_declaration']
                          for r in results.values())
    diffs = None
    count = None
    if reference_manifest is not None:
        reference = load_object(reference_manifest)
        all_diffs = field_differences(reference, manifest)
        count = len(all_diffs)
        diffs = all_diffs[:100]
    return {
        'scope': 'byte/declared-field diagnostic only; no native semantics or correctness certification',
        'lock_path': str(lock_path), 'artifact_directory': str(artifact),
        'manifest': {'expected_sha256': expected_manifest, 'actual': actual_manifest,
                     'exact_pinned_sha_matches': matches},
        'timestamp': {'expected_literal': timestamp, 'actual_json_value': actual_time,
                      'actual_json_type': type(actual_time).__name__, 'literal_matches': timestamp_equal},
        'source': manifest.get('source'), 'payloads': results,
        'reference_manifest': str(reference_manifest) if reference_manifest else None,
        'reference_field_difference_count': count, 'reference_field_differences': diffs,
        'reference_differences_truncated': count is not None and count > 100,
        'exact_pinned_runtime_matches': matches and timestamp_equal and payload_matches,
    }

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--lock', required=True, type=Path)
    parser.add_argument('--artifact', required=True, type=Path)
    parser.add_argument('--reference-manifest', type=Path)
    args = parser.parse_args()
    try:
        report = inspect(args.lock, args.artifact, args.reference_manifest)
    except (OSError, ValueError, TypeError) as error:
        print(json.dumps({'status': 'invalid_or_unavailable_input', 'error': str(error)}, indent=2))
        return 2
    print(json.dumps(report, ensure_ascii=False, indent=2))
    return 0 if report['exact_pinned_runtime_matches'] else 1

if __name__ == '__main__':
    raise SystemExit(main())
