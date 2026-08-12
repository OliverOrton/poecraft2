from __future__ import annotations

import argparse
from contextlib import closing
import json
from pathlib import Path
import sqlite3
import sys

from .core import sha256_bytes


def _database_schema_version(database: Path) -> int:
    try:
        uri = database.resolve().as_uri() + "?mode=ro"
        with closing(sqlite3.connect(uri, uri=True)) as connection:
            row = connection.execute(
                "SELECT schema_version FROM economy_manifest WHERE singleton = 1"
            ).fetchone()
    except sqlite3.Error as error:
        raise ValueError(
            f"checkpoint database schema could not be read: {error}"
        ) from error
    if row is None:
        raise ValueError("checkpoint database manifest is missing")
    try:
        version = int(row[0])
    except (TypeError, ValueError) as error:
        raise ValueError(
            "checkpoint database schema version is not an integer"
        ) from error
    if version < 1:
        raise ValueError(
            f"checkpoint database schema version must be positive, got {version}"
        )
    return version


def create_manifest(database: Path) -> dict[str, object]:
    body = database.read_bytes()
    content_hash = sha256_bytes(body)
    return {
        "schema_version": 1,
        "database_schema_version": _database_schema_version(database),
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
    if "database_schema_version" in manifest:
        expected_schema_version = int(manifest["database_schema_version"])
        actual_schema_version = _database_schema_version(database)
        if actual_schema_version != expected_schema_version:
            raise ValueError(
                "checkpoint database schema mismatch: "
                f"expected {expected_schema_version}, got {actual_schema_version}"
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
