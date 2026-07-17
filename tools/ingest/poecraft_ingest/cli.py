from __future__ import annotations

import argparse
import json
from pathlib import Path
import sqlite3
import sys

from .repo_loader import (
    REPOE_BASE_URL,
    fetch_source_snapshot,
    load_source_snapshot,
)
from .validation import (
    UnsupportedFeatureError,
    query_normal_rollable_mods,
    validate_database,
    write_validation_report,
)
from .write_sqlite import build_database


REPO_ROOT = Path(__file__).resolve().parents[3]
DEFAULT_SOURCE = REPO_ROOT / "data" / "raw" / "repoe"
DEFAULT_DATABASE = REPO_ROOT / "data" / "sqlite" / "poecraft.db"
DEFAULT_SCHEMA = REPO_ROOT / "schemas" / "sqlite" / "001_initial.sql"
DEFAULT_REPORT = REPO_ROOT / "data" / "sqlite" / "validation-report.json"
DEFAULT_BESTIARY_CONTRACT = REPO_ROOT / "fixtures" / "bestiary" / "v1"


def _path(value: str) -> Path:
    return Path(value).expanduser()


def _add_ingest_arguments(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--source", type=_path, default=DEFAULT_SOURCE)
    parser.add_argument("--database", type=_path, default=DEFAULT_DATABASE)
    parser.add_argument("--schema", type=_path, default=DEFAULT_SCHEMA)
    parser.add_argument("--report", type=_path, default=DEFAULT_REPORT)
    parser.add_argument(
        "--bestiary-contract",
        type=_path,
        default=DEFAULT_BESTIARY_CONTRACT,
    )


def _print_validation(report: dict) -> None:
    manifest = report.get("manifest", {})
    print(
        "validation: "
        + ("ok" if report["ok"] else "failed")
        + f" schema={manifest.get('schema_version', '?')}"
        + f" data_hash={manifest.get('data_hash', '?')}"
    )
    for entry in report["source_accounting"]:
        print(
            f"  {entry['logical_name']}: source={entry['row_count']} "
            f"imported={entry['imported']} skipped={entry['skipped']}"
        )
    cluster = report["cluster"]
    print(
        "  cluster: "
        f"jewels={cluster['jewels']} passives={cluster['passives']} "
        f"notables={cluster['notables']} "
        f"runtime={cluster['runtime_support']}"
    )
    for warning in report["warnings"]:
        print(f"  warning: {warning}")
    for error in report["errors"]:
        print(f"  error: {error}")


def _run_ingest(args: argparse.Namespace) -> int:
    snapshot = load_source_snapshot(args.source)
    result = build_database(
        snapshot,
        args.database,
        args.schema,
        args.bestiary_contract,
    )
    print(
        f"built {result['database']} "
        f"schema={result['schema_version']} "
        f"data_hash={result['data_hash']}"
    )
    report = validate_database(args.database)
    write_validation_report(report, args.report)
    _print_validation(report)
    return 0 if report["ok"] else 1


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="poecraft-ingest",
        description="Build and validate the canonical poecraft2 RePoE database.",
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    fetch = subparsers.add_parser("fetch", help="Fetch a frozen RePoE snapshot.")
    fetch.add_argument("--output", type=_path, default=DEFAULT_SOURCE)
    fetch.add_argument("--base-url", default=REPOE_BASE_URL)
    fetch.add_argument("--force", action="store_true")

    ingest = subparsers.add_parser(
        "ingest",
        help="Normalize a frozen source snapshot into SQLite.",
    )
    _add_ingest_arguments(ingest)

    refresh = subparsers.add_parser(
        "refresh",
        help="Fetch current RePoE data, ingest it, and validate the result.",
    )
    _add_ingest_arguments(refresh)
    refresh.add_argument("--base-url", default=REPOE_BASE_URL)
    refresh.add_argument("--force", action="store_true")

    validate = subparsers.add_parser(
        "validate",
        help="Validate source accounting and canonical relationships.",
    )
    validate.add_argument("--database", type=_path, default=DEFAULT_DATABASE)
    validate.add_argument("--report", type=_path)
    validate.add_argument("--json", action="store_true")

    query = subparsers.add_parser(
        "query-pool",
        help="Return normal rollable prefix/suffix rows for an ordinary base.",
    )
    query.add_argument("--database", type=_path, default=DEFAULT_DATABASE)
    query.add_argument("--base", required=True)
    query.add_argument("--item-level", type=int, required=True)
    query.add_argument("--json", action="store_true")

    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        if args.command == "fetch":
            manifest = fetch_source_snapshot(
                args.output,
                force=args.force,
                base_url=args.base_url,
            )
            print(
                f"fetched {len(manifest['files'])} files "
                f"source_version={manifest['source_version']}"
            )
            for entry in manifest["files"]:
                print(
                    f"  {entry['logical_name']}: "
                    f"rows={entry['row_count']} bytes={entry['byte_size']} "
                    f"sha256={entry['content_hash']}"
                )
            return 0

        if args.command == "ingest":
            return _run_ingest(args)

        if args.command == "refresh":
            manifest = fetch_source_snapshot(
                args.source,
                force=args.force,
                base_url=args.base_url,
            )
            print(
                f"fetched {len(manifest['files'])} files "
                f"source_version={manifest['source_version']}"
            )
            return _run_ingest(args)

        if args.command == "validate":
            report = validate_database(args.database)
            if args.report:
                write_validation_report(report, args.report)
            if args.json:
                print(json.dumps(report, indent=2, sort_keys=True))
            else:
                _print_validation(report)
            return 0 if report["ok"] else 1

        if args.command == "query-pool":
            result = query_normal_rollable_mods(
                args.database,
                args.base,
                args.item_level,
            )
            if args.json:
                print(json.dumps(result, indent=2, sort_keys=True))
            else:
                base = result["base"]
                counts = result["counts"]
                print(
                    f"{base['name']} ({base['metadata_path']}) "
                    f"item_level={base['item_level']}"
                )
                print(
                    f"prefix={counts['prefix']} suffix={counts['suffix']} "
                    f"total={counts['total']}"
                )
                for row in result["mods"]:
                    print(
                        f"  {row['generation_type']:6} "
                        f"{row['mod_key']} weight={row['final_weight']}"
                    )
            return 0
    except UnsupportedFeatureError as error:
        print(f"unsupported_feature: {error}", file=sys.stderr)
        return 4
    except (FileNotFoundError, ValueError, sqlite3.Error) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
