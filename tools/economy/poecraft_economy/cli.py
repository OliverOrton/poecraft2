from __future__ import annotations

import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import sqlite3
import sys

from .core import (
    DEFAULT_DATABASE,
    DEFAULT_FIXTURE,
    DEFAULT_GAME_DATABASE,
    DEFAULT_PUBLISH_DIR,
    DEFAULT_RAW_ROOT,
    DEFAULT_SCHEMA,
    compile_all_snapshots,
    discover_leagues,
    ingest_fixture,
    initialize_database,
    publish_artifacts,
    refresh_all_leagues,
    retention_report,
    validate_database,
)
from .provider import PoeNinjaAdapter


def _path(value: str) -> Path:
    return Path(value).expanduser()


def _database_argument(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--database", type=_path, default=DEFAULT_DATABASE)


def _print_json(value: object) -> None:
    print(json.dumps(value, indent=2, sort_keys=True))


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="poecraft-economy",
        description="Ingest, validate, compile, and publish poecraft2 economy snapshots.",
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    init = subparsers.add_parser("init", help="Create the canonical economy SQLite database.")
    _database_argument(init)
    init.add_argument("--schema", type=_path, default=DEFAULT_SCHEMA)
    init.add_argument("--game-database", type=_path, default=DEFAULT_GAME_DATABASE)
    init.add_argument("--force", action="store_true")

    discover = subparsers.add_parser(
        "discover-leagues", help="List every PoE1 PC economy league exposed upstream."
    )
    discover.add_argument("--base-url")
    discover.add_argument("--json", action="store_true")

    refresh = subparsers.add_parser(
        "refresh", help="Discover and refresh every provider-exposed league."
    )
    _database_argument(refresh)
    refresh.add_argument("--all-leagues", action="store_true", required=True)
    refresh.add_argument("--raw-root", type=_path, default=DEFAULT_RAW_ROOT)
    refresh.add_argument("--game-database", type=_path, default=DEFAULT_GAME_DATABASE)
    refresh.add_argument("--base-url")
    refresh.add_argument("--user-agent")
    refresh.add_argument("--concurrency", type=int, default=4)
    refresh.add_argument("--report", type=_path)

    fixture = subparsers.add_parser(
        "ingest-fixture", help="Deterministically rebuild from frozen provider fixtures."
    )
    _database_argument(fixture)
    fixture.add_argument("--fixture", type=_path, default=DEFAULT_FIXTURE)
    fixture.add_argument("--raw-root", type=_path, default=DEFAULT_RAW_ROOT)
    fixture.add_argument("--game-database", type=_path, default=DEFAULT_GAME_DATABASE)
    fixture.add_argument("--force", action="store_true")

    validate = subparsers.add_parser(
        "validate", help="Validate SQLite integrity, row accounting, raw hashes, and snapshots."
    )
    _database_argument(validate)
    validate.add_argument("--report", type=_path)
    validate.add_argument("--json", action="store_true")

    compile_parser = subparsers.add_parser(
        "compile", help="Compile current immutable snapshots from successful ingests."
    )
    _database_argument(compile_parser)
    compile_parser.add_argument("--all-leagues", action="store_true", required=True)
    compile_parser.add_argument("--game-database", type=_path, default=DEFAULT_GAME_DATABASE)

    publish = subparsers.add_parser(
        "publish", help="Write immutable snapshots and replace the league index last."
    )
    _database_argument(publish)
    publish.add_argument("--output", type=_path, default=DEFAULT_PUBLISH_DIR)
    publish.add_argument("--public-base-url", default="")
    publish.add_argument("--report", type=_path)

    retention = subparsers.add_parser(
        "retention-report", help="Report detailed-snapshot/raw compaction candidates."
    )
    _database_argument(retention)
    retention.add_argument("--apply", action="store_true")
    retention.add_argument("--now")
    retention.add_argument("--report", type=_path)

    return parser


def _write_report(path: Path | None, value: object) -> None:
    if path is None:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        if args.command == "init":
            result = initialize_database(
                args.database,
                schema=args.schema,
                game_database=args.game_database,
                force=args.force,
            )
            _print_json(result)
            return 0

        if args.command == "discover-leagues":
            options = {}
            if args.base_url:
                options["base_url"] = args.base_url
            leagues = discover_leagues(PoeNinjaAdapter(**options))
            if args.json:
                _print_json(leagues)
            else:
                for league in leagues:
                    print(f"{league['source_id']}\t{league['display_name']}")
            return 0

        if args.command == "refresh":
            options = {}
            if args.base_url:
                options["base_url"] = args.base_url
            if args.user_agent:
                options["user_agent"] = args.user_agent
            result = refresh_all_leagues(
                args.database,
                raw_root=args.raw_root,
                game_database=args.game_database,
                adapter=PoeNinjaAdapter(**options),
                concurrency=args.concurrency,
            )
            _write_report(args.report, result)
            _print_json(result)
            return 0 if result["status"] != "failed" else 1

        if args.command == "ingest-fixture":
            result = ingest_fixture(
                args.database,
                fixture_manifest=args.fixture,
                raw_root=args.raw_root,
                game_database=args.game_database,
                force=args.force,
            )
            _print_json(result)
            return 0

        if args.command == "validate":
            result = validate_database(args.database)
            _write_report(args.report, result)
            if args.json:
                _print_json(result)
            else:
                print(
                    f"validation: {'ok' if result['ok'] else 'failed'} "
                    f"rows={result['accounted_rows']} snapshots={result['snapshots']} "
                    f"hash={result['content_hash']}"
                )
                for warning in result["warnings"]:
                    print(f"  warning: {warning}")
                for error in result["errors"]:
                    print(f"  error: {error}")
            return 0 if result["ok"] else 1

        if args.command == "compile":
            artifacts = compile_all_snapshots(
                args.database, game_database=args.game_database
            )
            _print_json(
                {"snapshots": [artifact["id"] for artifact in artifacts]}
            )
            return 0

        if args.command == "publish":
            result = publish_artifacts(
                args.database,
                output=args.output,
                public_base_url=args.public_base_url,
            )
            _write_report(args.report, result)
            _print_json(result)
            return 0

        if args.command == "retention-report":
            now = (
                datetime.fromisoformat(args.now.replace("Z", "+00:00"))
                if args.now
                else datetime.now(timezone.utc)
            )
            result = retention_report(args.database, now=now, apply=args.apply)
            _write_report(args.report, result)
            _print_json(result)
            return 0
    except (FileNotFoundError, FileExistsError, ValueError, sqlite3.Error) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
