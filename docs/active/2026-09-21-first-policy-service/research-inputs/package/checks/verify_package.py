#!/usr/bin/env python3
"""Verify this generated planning packet locally; no repository or network access."""
from __future__ import annotations
import argparse
import ast
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import sys
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[1]


def inspect(root: Path) -> dict:
    errors: list[str] = []
    json_count = py_count = links = 0
    for p in sorted(root.rglob('*')):
        if not p.is_file() or '__pycache__' in p.parts:
            continue
        rel = p.relative_to(root).as_posix()
        if p.is_symlink():
            errors.append(f'symlink not permitted: {rel}')
        if p.suffix == '.json':
            try:
                json.loads(p.read_text(encoding='utf-8'))
                json_count += 1
            except (ValueError, OSError) as e:
                errors.append(f'{rel}: {e}')
        if p.suffix == '.py':
            try:
                ast.parse(p.read_text(encoding='utf-8'), filename=rel)
                py_count += 1
            except (SyntaxError, OSError) as e:
                errors.append(f'{rel}: {e}')
        if p.suffix == '.md':
            text = p.read_text(encoding='utf-8')
            if '\ufffd' in text:
                errors.append(f'replacement character: {rel}')
            for target in re.findall(r'\[[^\]\n]*\]\(([^)\s]+)(?:\s+"[^"]*")?\)', text):
                parsed = urlsplit(target)
                if parsed.scheme or parsed.netloc or not parsed.path:
                    continue
                path = (p.parent / unquote(parsed.path)).resolve()
                if not path.is_relative_to(root.resolve()):
                    errors.append(f'local link escapes package: {rel} -> {target}')
                elif not path.is_file():
                    errors.append(f'missing local link: {rel} -> {target}')
                links += 1
    manifest_path = root / 'MANIFEST.json'
    manifest_ok = None
    manifest_files = 0
    if manifest_path.is_file():
        m = json.loads(manifest_path.read_text())
        manifest_ok = True
        for row in m['files']:
            pure = PurePosixPath(row['path'])
            if pure.is_absolute() or '..' in pure.parts:
                errors.append('invalid manifest path')
                manifest_ok = False
                continue
            p = root / str(pure)
            if not p.is_file():
                errors.append(f'missing payload {pure}')
                manifest_ok = False
                continue
            payload = p.read_bytes()
            if len(payload) != row['bytes'] or hashlib.sha256(payload).hexdigest() != row['sha256']:
                errors.append(f'hash/length mismatch {pure}')
                manifest_ok = False
            manifest_files += 1
        actual = {p.relative_to(root).as_posix() for p in root.rglob('*') if p.is_file() and '__pycache__' not in p.parts and p.name != 'MANIFEST.json'}
        listed = {r['path'] for r in m['files']}
        if actual != listed:
            errors.append(f'manifest inventory differs: {sorted(actual ^ listed)}')
            manifest_ok = False
    baseline = json.loads((root / 'evidence/baseline.json').read_text())
    ci = json.loads((root / 'evidence/ci_observations.json').read_text())
    if ci['windows']['status'] != 'completed' or ci['windows']['conclusion'] != 'failure':
        errors.append('completed reviewed Windows failure lost')
    if ci['windows']['ctest']['total_tests'] != 17 or len(ci['windows']['ctest']['failed_suites']) != 3:
        errors.append('CI failure cardinality lost')
    abstract = json.loads((root / 'evidence/abstract_checks.json').read_text())
    if not abstract['passed'] or abstract['tests_run'] != 23:
        errors.append('abstract checks missing or failed')
    for name in ['README.md', 'CODEX_PROMPT.md', '02_MILESTONES.md', '06_VALIDATION.md']:
        if '08_INTEGRITY_GATE.md' not in (root / name).read_text():
            errors.append(f'integrity gate missing from entry point: {name}')
    return {'passed': not errors, 'errors': errors, 'json_files_parsed': json_count,
            'python_files_parsed': py_count, 'local_markdown_links_checked': links,
            'manifest_ok': manifest_ok, 'manifest_payload_files': manifest_files,
            'native_or_wasm_executed': False}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=ROOT)
    args = parser.parse_args()
    report = inspect(args.root.resolve())
    print(json.dumps(report, indent=2))
    return 0 if report['passed'] else 1

if __name__ == '__main__':
    sys.exit(main())
