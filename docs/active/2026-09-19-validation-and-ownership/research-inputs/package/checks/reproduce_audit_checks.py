#!/usr/bin/env python3
"""Reproduce two small audit checks; does not build or execute poecraft2.

The collection example intentionally contains a failing top-level test. A pytest
exit of 1 is the expected demonstration, not a repository regression or a pass.
"""
from __future__ import annotations

import argparse
import ast
import hashlib
import importlib.metadata
import importlib.util
import io
import json
import os
from pathlib import Path
import platform
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
PIN = 'd13c9b835186ce35cb51aef0de6d0027f8b8a1a7'
GIT_BLOB = '4a0d0f3bb04c6a38770537d23cbfd86ec59dcfa7'
EXPECTED_LF = '71bd8aea350928a3da87103b7d78c454b9e024cb5502fb3af66c9ad2987e333f'
OBSERVED_WINDOWS = 'fd46a2e64f3e75aeff0b6086ff7516d6214506ced3ba58d3974d244b03d015eb'


def byte_check() -> dict[str, object]:
    raw = (ROOT / 'evidence/evaluator-straight.git-blob.json').read_bytes()
    blob = hashlib.sha1(b'blob ' + str(len(raw)).encode('ascii') + b'\0' + raw).hexdigest()
    if blob != GIT_BLOB or b'\r' in raw:
        raise ValueError('The audit fixture does not match the inspected LF Git blob.')
    converted = raw.replace(b'\n', b'\r\n')
    lf_sha = hashlib.sha256(raw).hexdigest()
    crlf_sha = hashlib.sha256(converted).hexdigest()
    if lf_sha != EXPECTED_LF or crlf_sha != OBSERVED_WINDOWS:
        raise AssertionError('The exact recorded byte-hash relationship was not reproduced.')
    if json.loads(raw) != json.loads(converted):
        raise AssertionError('Line-ending conversion unexpectedly changed parsed JSON.')
    return {
        'status': 'reproduced', 'repository_ref': PIN,
        'source_path': 'fixtures/solver-baselines/s8.0/examples/evaluator-straight.strategy.json',
        'git_blob_sha1': blob, 'git_blob_bytes': len(raw), 'lf_count': raw.count(b'\n'),
        'lf_sha256': lf_sha, 'crlf_sha256': crlf_sha,
        'matches_recorded_windows_failure': True, 'parsed_json_equal': True,
        'scope': 'This fixture only; not a diagnosis of every failed archive hash.',
    }


def collection_check() -> dict[str, object]:
    source = '''import unittest
class ClassStyle(unittest.TestCase):
    def test_class_is_seen(self):
        self.assertTrue(True)

def test_top_level_must_not_be_omitted():
    assert False, "intentional collection witness"
'''
    tree = ast.parse(source)
    top = [n.name for n in tree.body if isinstance(n, ast.FunctionDef) and n.name.startswith('test_')]
    with tempfile.TemporaryDirectory(prefix='poecraft-collection-audit-') as directory:
        path = Path(directory) / 'test_collection_witness.py'
        path.write_text(source, encoding='utf-8')
        suite = unittest.defaultTestLoader.discover(directory, pattern=path.name)
        count = suite.countTestCases()
        result = unittest.TextTestRunner(stream=io.StringIO()).run(suite)
        if count != 1 or not result.wasSuccessful() or len(top) != 1:
            raise AssertionError('Unexpected unittest collection in the toy witness.')
        pytest_observation: dict[str, object]
        if importlib.util.find_spec('pytest') is None:
            pytest_observation = {'status': 'unavailable', 'reason': 'pytest is not installed'}
        else:
            env = dict(os.environ, PYTEST_DISABLE_PLUGIN_AUTOLOAD='1')
            completed = subprocess.run(
                [sys.executable, '-m', 'pytest', str(path), '-q', '--tb=no', '-p', 'no:cacheprovider'],
                cwd=directory, env=env, capture_output=True, text=True, timeout=30, check=False,
            )
            text = completed.stdout + completed.stderr
            if completed.returncode != 1 or '1 failed, 1 passed' not in text:
                raise AssertionError(f'Unexpected pytest result: {completed.returncode}\n{text}')
            pytest_observation = {
                'status': 'expected_failure_observed', 'version': importlib.metadata.version('pytest'),
                'exit_code': completed.returncode, 'test_outcomes': {'failed': 1, 'passed': 1},
                'meaning': 'The deliberately failing top-level function was collected.',
            }
        return {
            'status': 'reproduced', 'scope': 'Isolated two-test toy, not repository collection counts.',
            'declared_class_methods': 1, 'declared_top_level_functions': top,
            'unittest_collected': count, 'unittest_passed': result.wasSuccessful(),
            'pytest': pytest_observation,
        }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, help='Optional result path; omit to print without modifying this packet.')
    args = parser.parse_args()
    report = {
        'schema': 'poecraft2_planning_reference_checks_v1',
        'execution_environment': {'python': platform.python_version(), 'system': platform.system()},
        'byte_identity': byte_check(), 'test_collection': collection_check(),
        'native_or_wasm_execution': False, 'repository_files_modified': False,
        'claim': 'Byte portability and test-collection demonstrations only; no solver qualification.',
    }
    if args.output is not None:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report, indent=2))
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
