#!/usr/bin/env python3
"""Verify this handoff's manifest, local Markdown links, JSON and Python syntax."""
from __future__ import annotations
import ast
import hashlib
import json
import re
from pathlib import Path
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[1]

def verify() -> dict:
    manifest=json.loads((ROOT/'MANIFEST.json').read_text(encoding='utf-8'))
    errors=[];links=0;json_files=0;python_files=0
    actual={str(p.relative_to(ROOT)).replace('\\','/') for p in ROOT.rglob('*')
            if p.is_file() and '__pycache__' not in p.parts and p.name!='MANIFEST.json'}
    listed={entry['path'] for entry in manifest['files']}
    if actual!=listed:
        errors.append({'inventory_difference':sorted(actual^listed)})
    for entry in manifest['files']:
        path=(ROOT/entry['path']).resolve()
        if ROOT not in path.parents:
            errors.append({'unsafe_path':entry['path']});continue
        if not path.is_file():
            errors.append({'missing_file':entry['path']});continue
        payload=path.read_bytes()
        if len(payload)!=entry['bytes'] or hashlib.sha256(payload).hexdigest()!=entry['sha256']:
            errors.append({'hash_mismatch':entry['path']})
    for p in ROOT.rglob('*'):
        if not p.is_file() or '__pycache__' in p.parts:continue
        if p.suffix=='.json':
            json.loads(p.read_text(encoding='utf-8'));json_files+=1
        elif p.suffix=='.py':
            ast.parse(p.read_text(encoding='utf-8'));python_files+=1
        elif p.suffix=='.md':
            text=p.read_text(encoding='utf-8')
            for target in re.findall(r'\[[^\]]*\]\(([^\s)]+)\)',text):
                parsed=urlsplit(target)
                if parsed.scheme or target.startswith('//'):continue
                dest=(p.parent/unquote(parsed.path)).resolve() if parsed.path else p
                links+=1
                if not dest.exists():errors.append({'broken_link':str(p.relative_to(ROOT)),'target':target})
                elif parsed.fragment:
                    fragment=unquote(parsed.fragment)
                    target_text=dest.read_text(encoding='utf-8')
                    ids=set(re.findall(r'<a id="([^"]+)"',target_text))
                    # Internal fragment links currently use explicit anchors.
                    if fragment not in ids:errors.append({'missing_anchor':target})
    return {'payload_files_verified':len(listed),'manifest_self_excluded':True,
            'local_markdown_links_checked':links,'json_files_parsed':json_files,
            'python_files_syntax_checked':python_files,'powershell_syntax_or_execution_checked':False,
            'errors':errors,'passed':not errors}

if __name__=='__main__':
    result=verify();print(json.dumps(result,indent=2));raise SystemExit(0 if result['passed'] else 1)
