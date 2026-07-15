from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys

from .core import sha256_bytes


def create_manifest(database: Path) -> dict[str, object]:
    body = database.read_bytes()
    content_hash = sha256_bytes(body)
    return {
        "schema_version": 1,
        "database_key": f"database/{content_hash}.db",
        "sha256": content_hash,
        "bytes": len(body),
    }


def verify(database: Path, manifest_path: Path) -> dict[str, object]:
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    expected_hash = str(manifest["sha256"])
    expected_size = int(manifest["bytes"])
    body = database.read_bytes()
    if len(body) != expected_size:
        raise ValueError(
            f"checkpoint size mismatch: expected {expected_size}, got {len(body)}"
        )
    actual_hash = sha256_bytes(body)
    if actual_hash != expected_hash:
        raise ValueError(
            f"checkpoint hash mismatch: expected {expected_hash}, got {actual_hash}"
        )
    return manifest


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Create or verify an economy DB checkpoint.")
    subparsers = parser.add_subparsers(dest="command", required=True)
    write = subparsers.add_parser("write")
    write.add_argument("--database", type=Path, required=True)
    write.add_argument("--output", type=Path, required=True)
    check = subparsers.add_parser("verify")
    check.add_argument("--database", type=Path, required=True)
    check.add_argument("--manifest", type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        if args.command == "write":
            manifest = create_manifest(args.database)
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_text(
                json.dumps(manifest, indent=2, sort_keys=True) + "\n",
                encoding="utf-8",
            )
        else:
            manifest = verify(args.database, args.manifest)
        print(json.dumps(manifest, sort_keys=True))
        return 0
    except (FileNotFoundError, KeyError, ValueError, json.JSONDecodeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
