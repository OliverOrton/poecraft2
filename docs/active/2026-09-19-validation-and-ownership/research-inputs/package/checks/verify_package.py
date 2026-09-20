#!/usr/bin/env python3
"""Verify this planning packet's manifest. No repository, network or native work."""
from __future__ import annotations
import hashlib
import json
from pathlib import Path
import sys


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    manifest_path = root / 'MANIFEST.json'
    if not manifest_path.is_file():
        print('MANIFEST.json is missing.', file=sys.stderr)
        return 1
    manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
    errors: list[str] = []
    declared: set[str] = set()
    for entry in manifest['files']:
        name = entry['path']
        if name in declared:
            errors.append(f'Duplicate path: {name}')
            continue
        declared.add(name)
        candidate = (root / name).resolve()
        if not candidate.is_relative_to(root.resolve()):
            errors.append(f'Path escapes packet: {name}')
            continue
        if not candidate.is_file():
            errors.append(f'Missing file: {name}')
            continue
        raw = candidate.read_bytes()
        if len(raw) != entry['bytes'] or hashlib.sha256(raw).hexdigest() != entry['sha256']:
            errors.append(f'Content mismatch: {name}')
    actual = {
        p.relative_to(root).as_posix() for p in root.rglob('*')
        if p.is_file() and p != manifest_path and '__pycache__' not in p.parts
    }
    errors.extend(f'Unlisted file: {name}' for name in sorted(actual - declared))
    result = {
        'status': 'passed' if not errors else 'failed',
        'files_checked': len(declared),
        'errors': errors,
        'scope': 'Planning packet integrity only; not mathematical or native solver validation.',
    }
    print(json.dumps(result, indent=2))
    return int(bool(errors))

if __name__ == '__main__':
    raise SystemExit(main())
