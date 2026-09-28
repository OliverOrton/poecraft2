from __future__ import annotations

import argparse
from contextlib import closing
import json
from pathlib import Path
import sqlite3
import sys
import os
import re
from urllib.parse import urlparse

from .core import sha256_bytes


def validate_configuration(environment=None) -> None:
    environment = os.environ if environment is None else environment
    required = ('AWS_ACCESS_KEY_ID', 'AWS_SECRET_ACCESS_KEY', 'R2_ACCOUNT_ID',
                'R2_PRIVATE_BUCKET', 'R2_PUBLIC_BUCKET', 'ECONOMY_PUBLIC_BASE_URL')
    missing = [key for key in required if not environment.get(key, '').strip()]
    if missing:
        raise ValueError('missing refresh configuration: ' + ', '.join(missing))
    if not re.fullmatch(r'[a-fA-F0-9]{32}', environment['R2_ACCOUNT_ID']):
        raise ValueError('invalid R2_ACCOUNT_ID format')
    url = urlparse(environment['ECONOMY_PUBLIC_BASE_URL'])
    if url.scheme != 'https' or not url.hostname or url.username or url.password or url.query or url.fragment:
        raise ValueError('ECONOMY_PUBLIC_BASE_URL must be a public HTTPS URL')
    for key in ('R2_PRIVATE_BUCKET', 'R2_PUBLIC_BUCKET'):
        if not re.fullmatch(r'[a-z0-9][a-z0-9.-]{1,61}[a-z0-9]', environment[key]):
            raise ValueError(f'invalid {key} format')


def get_optional_object(client, bucket: str, key: str):
    try:
        return client.get_object(Bucket=bucket, Key=key)['Body'].read()
    except Exception as error:
        # A bucket/auth/transport failure is not an absent checkpoint. Even a
        # generic 404 is ambiguous; only the service's NoSuchKey permits bootstrap.
        if getattr(error, 'response', {}).get('Error', {}).get('Code') == 'NoSuchKey':
            return None
        raise


def restore(client, bucket: str, database: Path, manifest_path: Path, *, bootstrap=False) -> bool:
    body = get_optional_object(client, bucket, 'database/latest.json')
    if body is None:
        if not bootstrap:
            raise ValueError('checkpoint absent; explicit manual bootstrap is required')
        return False
    manifest = json.loads(body)
    digest = manifest.get('sha256', '')
    if not re.fullmatch(r'[a-f0-9]{64}', digest) or manifest.get('database_key') != f'database/{digest}.db':
        raise ValueError('invalid checkpoint object identity')
    payload = get_optional_object(client, bucket, manifest['database_key'])
    if payload is None:
        raise ValueError('checkpoint pointer references a missing database')
    # Verify isolated files first; a failed restore cannot replace existing history.
    import tempfile
    with tempfile.TemporaryDirectory() as temporary:
        staged = Path(temporary) / 'database.db'
        staged_manifest = Path(temporary) / 'manifest.json'
        staged.write_bytes(payload)
        staged_manifest.write_bytes(body)
        verify(staged, staged_manifest)
    database.parent.mkdir(parents=True, exist_ok=True)
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    database.write_bytes(payload)
    manifest_path.write_bytes(body)
    return True


def publish_snapshots(client, bucket: str, directory: Path) -> None:
    for path in sorted(directory.glob('*.json')):
        if not re.fullmatch(r'[a-f0-9]{64}\.json', path.name):
            raise ValueError('invalid immutable snapshot filename')
        key = f'snapshots/{path.name}'
        body = path.read_bytes()
        previous = get_optional_object(client, bucket, key)
        if previous is not None:
            if previous != body:
                raise ValueError(f'immutable remote snapshot collision: {path.name}')
            continue
        try:
            client.put_object(Bucket=bucket, Key=key, Body=body, IfNoneMatch='*',
                              ContentType='application/json', CacheControl='public,max-age=31536000,immutable')
        except Exception as error:
            if getattr(error, 'response', {}).get('Error', {}).get('Code') not in ('PreconditionFailed', '412'):
                raise
            if get_optional_object(client, bucket, key) != body:
                raise ValueError(f'immutable remote snapshot race: {path.name}') from error


def verify_product_database(database: Path, product_lock: Path) -> None:
    lock = json.loads(product_lock.read_text(encoding='utf-8'))
    root = product_lock.resolve().parents[2]
    manifest_path = root / lock['runtime_directory'] / 'manifest.json'
    body = manifest_path.read_bytes()
    if sha256_bytes(body) != lock['manifest_sha256']:
        raise ValueError('selected product manifest hash mismatch')
    expected = json.loads(body)['source']['data_hash']
    with closing(sqlite3.connect(database.resolve().as_uri() + '?mode=ro', uri=True)) as connection:
        actual = connection.execute('SELECT data_hash FROM data_manifest WHERE id = 1').fetchone()
    if actual is None or actual[0] != expected:
        raise ValueError('market refresh database differs from selected product game data')


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
    subparsers.add_parser('validate-config')
    restore_parser = subparsers.add_parser('restore')
    restore_parser.add_argument('--database', type=Path, required=True)
    restore_parser.add_argument('--manifest', type=Path, required=True)
    restore_parser.add_argument('--game-database', type=Path, required=True)
    restore_parser.add_argument('--bootstrap', action='store_true')
    publish = subparsers.add_parser('publish-snapshots')
    publish.add_argument('--directory', type=Path, required=True)
    product = subparsers.add_parser('verify-product')
    product.add_argument('--database', type=Path, required=True)
    product.add_argument('--product-lock', type=Path, required=True)
    write = subparsers.add_parser("write")
    write.add_argument("--database", type=Path, required=True)
    write.add_argument("--output", type=Path, required=True)
    check = subparsers.add_parser("verify")
    check.add_argument("--database", type=Path, required=True)
    check.add_argument("--manifest", type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        if args.command == 'verify-product':
            verify_product_database(args.database, args.product_lock)
            print('selected product database verified')
            return 0
        if args.command in ('validate-config', 'restore', 'publish-snapshots'):
            validate_configuration()
            if args.command == 'validate-config':
                print('refresh configuration present and well formed (credentials not exercised)')
                return 0
            import boto3
            client = boto3.client('s3', endpoint_url=f"https://{os.environ['R2_ACCOUNT_ID']}.r2.cloudflarestorage.com")
            if args.command == 'restore':
                restored = restore(client, os.environ['R2_PRIVATE_BUCKET'], args.database, args.manifest, bootstrap=args.bootstrap)
                if not restored:
                    from .core import initialize_database
                    initialize_database(args.database, game_database=args.game_database)
                print('checkpoint restored and verified' if restored else 'explicit empty-history bootstrap completed')
            else:
                publish_snapshots(client, os.environ['R2_PUBLIC_BUCKET'], args.directory)
                print('immutable snapshots created or byte-verified')
            return 0
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
