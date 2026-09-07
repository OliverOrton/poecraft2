"""Conservative Windows-work classifier; uncertainty retains native validation."""
from __future__ import annotations

import argparse
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
            'reason': 'Only known documentation paths changed; native validation is not applicable.'
                if only_prose else 'Mixed, executable, unknown or unavailable changes require native validation.'}


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
