"""Verify planning-packet integrity, syntax and local document references.

This is not an engine test runner, native build script or experiment supervisor.
"""
from __future__ import annotations
import argparse
import ast
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import zipfile


def verify(root: Path, archive: Path | None = None) -> dict:
    root = root.resolve()
    manifest = json.loads((root / 'MANIFEST.json').read_text(encoding='utf-8'))
    items = manifest['files']
    names = set()
    for item in items:
        name = item['path']
        pp = PurePosixPath(name)
        if pp.is_absolute() or '..' in pp.parts or '\\' in name or name in names:
            raise ValueError(f'Unsafe/duplicate path: {name}')
        names.add(name)
        path = root / name
        if path.is_symlink() or not path.is_file() or not path.resolve().is_relative_to(root):
            raise ValueError(f'Invalid payload: {name}')
        data = path.read_bytes()
        if len(data) != item['size'] or hashlib.sha256(data).hexdigest() != item['sha256']:
            raise ValueError(f'Payload mismatch: {name}')
    actual = {str(p.relative_to(root).as_posix()) for p in root.rglob('*') if p.is_file() and '__pycache__' not in p.parts}
    if actual != names | {'MANIFEST.json'}:
        raise ValueError(f'Unexpected files: {sorted(actual ^ (names | {"MANIFEST.json"}))}')
    json_count = py_count = links = 0
    for name in sorted(actual):
        p = root / name
        if p.suffix == '.json':
            json.loads(p.read_text(encoding='utf-8')); json_count += 1
        if p.suffix == '.py':
            ast.parse(p.read_text(encoding='utf-8')); py_count += 1
        if p.suffix == '.md':
            for target in re.findall(r'\[[^\]]*\]\(([^)]+)\)', p.read_text(encoding='utf-8')):
                target = target.split(' "', 1)[0]
                if '://' in target or target.startswith(('mailto:', '#')):
                    continue
                path_part = target.split('#', 1)[0]
                if path_part and not (p.parent / path_part).exists():
                    raise ValueError(f'Broken local link {name}: {target}')
                links += 1
    archive_checked = False
    if archive is not None:
        with zipfile.ZipFile(archive) as z:
            if z.testzip() is not None:
                raise ValueError('Archive CRC failure')
            if len(z.namelist()) != len(set(z.namelist())) or set(z.namelist()) != actual:
                raise ValueError('Archive file set mismatch')
            for name in actual:
                if z.read(name) != (root / name).read_bytes():
                    raise ValueError(f'Archive differs: {name}')
        archive_checked = True
    return {'payload_files': len(names), 'total_files': len(actual), 'json_files': json_count,
            'python_files': py_count, 'local_markdown_links': links,
            'archive_verified': archive_checked, 'status': 'passed',
            'scope': 'planning-packet integrity only; no native solver qualification'}


if __name__ == '__main__':
    p = argparse.ArgumentParser()
    p.add_argument('--root', type=Path, default=Path(__file__).resolve().parent)
    p.add_argument('--zip', type=Path)
    a = p.parse_args()
    print(json.dumps(verify(a.root, a.zip), indent=2))
