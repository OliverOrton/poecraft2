"""Conservative Windows-work classifier; uncertainty retains native validation."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import subprocess


def classify(paths: list[str] | None) -> dict:
    def prose(path: str) -> bool:
        p = PurePosixPath(path)
        if p.is_absolute() or '..' in p.parts:
            return False
        return path in {'README.md', 'AGENTS.md', 'CLAUDE.md', 'HANDOFF.md'} or (
            path.startswith('docs/') and p.suffix == '.md')
    only_prose = bool(paths) and all(prose(p) for p in paths)
    return {'native_required': not only_prose,
            'reason': 'Only known documentation paths changed; native validation was not run; no prior qualification is inferred.'
                if only_prose else 'Mixed, executable, unknown or unavailable changes require native validation.'}


def validation_identity(root: Path, revision: str = "HEAD") -> str:
    """Bind tracked non-prose inputs, excluding the protected file."""
    # ls-tree does not support exclude pathspecs. Resolve safe top-level inputs
    # with ls-files first, including inputs present only in the named revision.
    paths = subprocess.check_output(
        ["git", "ls-files", "--full-name", "-z", "--with-tree", revision,
         "--", ".", ":(exclude,top)0"], cwd=root,
    ).split(b"\0")
    roots = sorted({path.split(b"/", 1)[0].decode("utf-8") for path in paths if path})
    if not roots:
        raise ValueError("no safe tracked source inputs")
    rows = subprocess.check_output(
        ["git", "ls-tree", "-r", "-z", revision, "--", *roots], cwd=root,
    ).split(b"\0")
    inputs = []
    for row in rows:
        if not row:
            continue
        metadata, path_bytes = row.split(b"\t", 1)
        if classify([path_bytes.decode("utf-8")])["native_required"]:
            inputs.append(row)
    return hashlib.sha256(b"\0".join(sorted(inputs))).hexdigest()


def main() -> int:
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--base', default='')
    p.add_argument('--head', default='HEAD')
    p.add_argument('--output', type=Path)
    args=p.parse_args()
    paths=None
    if args.base and set(args.base) != {'0'}:
        try:
            # No renames: both old and new paths participate in the decision.
            diff=subprocess.check_output(['git','diff','--no-renames','--name-only','-z',
                                          args.base,args.head,'--'], stderr=subprocess.DEVNULL)
            paths=[s.decode('utf-8', errors='strict') for s in diff.split(b'\0') if s]
        except (subprocess.CalledProcessError, UnicodeDecodeError):
            pass
    result=classify(paths)
    print(json.dumps(result))
    if args.output:
        with args.output.open('a',encoding='utf-8') as out:
            out.write(f"native_required={str(result['native_required']).lower()}\n")
    return 0


if __name__=='__main__':
    raise SystemExit(main())
