from __future__ import annotations

from datetime import datetime, timezone
import json
from pathlib import Path
import sqlite3
import tempfile
import unittest

from poecraft_economy.core import (
    DEFAULT_FIXTURE,
    DEFAULT_SCHEMA,
    _snapshot_content,
    canonical_json,
    ingest_fixture,
    publish_artifacts,
    refresh_all_leagues,
    retention_report,
    sha256_bytes,
    validate_database,
)
from poecraft_economy.provider import (
    ADAPTER_VERSION,
    CATEGORY_CAPABILITIES,
    FetchResult,
    PoeNinjaAdapter,
    SourceLeague,
)


def _create_game_database(path: Path) -> None:
    connection = sqlite3.connect(path)
    connection.executescript(
        """
        CREATE TABLE data_manifest(id INTEGER PRIMARY KEY, data_hash TEXT);
        INSERT INTO data_manifest VALUES (1, 'fixture-game-data');
        CREATE TABLE essence(key TEXT PRIMARY KEY, name TEXT NOT NULL);
        INSERT INTO essence VALUES (
            'Metadata/Items/Currency/Essence/Greed7',
            'Deafening Essence of Greed'
        );
        CREATE TABLE fossil(key TEXT PRIMARY KEY, name TEXT NOT NULL);
        INSERT INTO fossil VALUES (
            'Metadata/Items/Currency/Fossil/Lucent',
            'Lucent Fossil'
        );
        CREATE TABLE mod(mod_id INTEGER PRIMARY KEY, key TEXT NOT NULL);
        INSERT INTO mod VALUES (1, 'CraftedLife');
        CREATE TABLE bench_option(
            bench_option_id INTEGER PRIMARY KEY,
            mod_id INTEGER NOT NULL
        );
        INSERT INTO bench_option VALUES (1, 1);
        CREATE TABLE bench_option_cost(
            bench_option_id INTEGER NOT NULL,
            currency_key TEXT NOT NULL,
            amount INTEGER NOT NULL
        );
        INSERT INTO bench_option_cost VALUES (
            1,
            'Metadata/Items/Currency/CurrencyRerollRare',
            2
        );
        """
    )
    connection.commit()
    connection.close()


class FrozenAdapter:
    def __init__(
        self,
        fixture: Path,
        fail: tuple[str, str] | None = None,
        *,
        not_modified: bool = False,
        omit_required: tuple[str, str] | None = None,
        malformed: tuple[str, str] | None = None,
    ) -> None:
        self.root = fixture.parent
        self.manifest = json.loads(fixture.read_text(encoding="utf-8"))
        self.delegate = PoeNinjaAdapter()
        self.fail = fail
        self.not_modified = not_modified
        self.omit_required = omit_required
        self.malformed = malformed

    def capabilities(self):
        return CATEGORY_CAPABILITIES

    def discover_leagues(self):
        body = (self.root / self.manifest["leagues"]).read_bytes()
        value = json.loads(body)
        return (
            [SourceLeague(row["id"], row["name"]) for row in value],
            FetchResult(200, body, '"fixture-leagues"', None, "max-age=3600"),
        )

    def fetch_category(self, source_league_id, capability, conditional_headers=None):
        if self.fail == (source_league_id, capability.name):
            raise ValueError("forced category failure")
        if self.malformed == (source_league_id, capability.name):
            return FetchResult(200, b"{", '"fixture-category"', None, "max-age=3600")
        if self.not_modified:
            if not conditional_headers or "If-None-Match" not in conditional_headers:
                raise ValueError("conditional ETag header was not supplied")
            return FetchResult(304, None, '"fixture-category"', None, "max-age=3600")
        relative = self.manifest["responses"][source_league_id][capability.name]
        body = (self.root / relative).read_bytes()
        if self.omit_required == (source_league_id, capability.name):
            value = json.loads(body)
            value["lines"] = [
                row
                for row in value["lines"]
                if row.get("detailsId") != "craicic-croaker"
            ]
            body = json.dumps(value, sort_keys=True).encode()
        return FetchResult(200, body, '"fixture-category"', None, "max-age=3600")

    def parse_rows(self, capability, raw_response):
        return self.delegate.parse_rows(capability, raw_response)


class EconomyPipelineTests(unittest.TestCase):
    def test_checked_in_runtime_snapshot_uses_canonical_hash_rules(self) -> None:
        path = Path(__file__).resolve().parents[3] / "fixtures" / "economy" / "runtime-snapshot-v1.json"
        artifact = json.loads(path.read_text(encoding="utf-8"))
        content = _snapshot_content(
            artifact["metadata"]["league_key"],
            artifact["metadata"]["game_data_hash"],
            artifact["prices"],
            artifact["metadata"]["missing_keys"],
            artifact["metadata"]["low_confidence_keys"],
            artifact.get("sources"),
        )
        self.assertEqual(
            sha256_bytes(canonical_json(content).encode("utf-8")),
            artifact["metadata"]["content_sha256"],
        )

    def test_fixture_rebuild_is_deterministic_and_fully_accounted(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            game = root / "game.db"
            _create_game_database(game)
            reports = []
            for name in ("one", "two"):
                directory = root / name
                directory.mkdir()
                database = directory / "economy.db"
                result = ingest_fixture(
                    database,
                    raw_root=directory / "raw",
                    game_database=game,
                    force=True,
                )
                self.assertEqual(result["leagues"], 3)
                report = validate_database(database)
                self.assertTrue(report["ok"], report)
                self.assertEqual(report["source_rows"], report["accounted_rows"])
                self.assertEqual(report["unresolved_rows"], 0)
                reports.append(report)
            self.assertEqual(reports[0]["content_hash"], reports[1]["content_hash"])

            connection = sqlite3.connect(root / "one" / "economy.db")
            connection.row_factory = sqlite3.Row
            artifact = json.loads(
                connection.execute(
                    """
                    SELECT p.artifact_json FROM economy_snapshot AS p
                    JOIN economy_league_status AS s
                      ON s.latest_snapshot_id = p.snapshot_id
                    WHERE s.league_key = 'mirage'
                    """
                ).fetchone()[0]
            )
            self.assertEqual(artifact["version"], "v1")
            self.assertEqual(artifact["prices"]["alteration"], 0.18)
            self.assertEqual(
                artifact["prices"]["fossil:Metadata/Items/Currency/Fossil/Lucent"],
                1.2,
            )
            self.assertEqual(
                artifact["prices"]["essence:Metadata/Items/Currency/Essence/Greed7"],
                4.8,
            )
            self.assertEqual(artifact["prices"]["harvest_resist:fire"], 10.0)
            self.assertEqual(artifact["prices"]["bench:CraftedLife"], 2.0)
            self.assertEqual(
                artifact["prices"]["beast:craicic-croaker"], 42.0
            )
            self.assertEqual(artifact["prices"]["beast:rare"], 1.0)
            self.assertEqual(
                artifact["sources"]["beast:craicic-croaker"], "quote"
            )
            self.assertEqual(
                artifact["sources"]["beast:rare"], "owner_default"
            )
            self.assertNotIn("beast:rare", artifact["metadata"]["missing_keys"])
            owner_default = connection.execute(
                """
                SELECT chaos_value, decision_id, overridable
                FROM economy_snapshot_owner_default_price
                WHERE snapshot_id = ? AND canonical_price_key = 'beast:rare'
                """,
                (artifact["id"],),
            ).fetchone()
            self.assertEqual(tuple(owner_default), (1.0, "owner:2026-08-09:generic-rare-beast", 1))
            with self.assertRaises(sqlite3.IntegrityError):
                connection.execute(
                    "UPDATE economy_snapshot SET artifact_json = '{}' WHERE snapshot_id = ?",
                    (artifact["id"],),
                )
            connection.close()

    def test_conditional_refresh_reuses_verified_raw_responses(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            game = root / "game.db"
            database = root / "economy.db"
            _create_game_database(game)
            ingest_fixture(
                database,
                raw_root=root / "raw",
                game_database=game,
                force=True,
            )
            first = refresh_all_leagues(
                database,
                raw_root=root / "raw",
                game_database=game,
                adapter=FrozenAdapter(DEFAULT_FIXTURE),
            )
            self.assertEqual(first["status"], "complete")
            second = refresh_all_leagues(
                database,
                raw_root=root / "raw",
                game_database=game,
                adapter=FrozenAdapter(DEFAULT_FIXTURE, not_modified=True),
            )
            self.assertEqual(second["status"], "complete")
            connection = sqlite3.connect(database)
            statuses = connection.execute(
                "SELECT DISTINCT status FROM economy_fetch WHERE run_id = ?",
                (second["run_id"],),
            ).fetchall()
            self.assertEqual(statuses, [("not_modified",)])
            connection.close()
            report = validate_database(database)
            self.assertTrue(report["ok"], report)

    def test_partial_refresh_retains_last_good_snapshot(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            game = root / "game.db"
            database = root / "economy.db"
            _create_game_database(game)
            ingest_fixture(
                database,
                raw_root=root / "raw",
                game_database=game,
                force=True,
            )
            connection = sqlite3.connect(database)
            before = connection.execute(
                "SELECT latest_snapshot_id FROM economy_league_status WHERE league_key = 'hardcore-mirage'"
            ).fetchone()[0]
            connection.close()

            result = refresh_all_leagues(
                database,
                raw_root=root / "raw",
                game_database=game,
                adapter=FrozenAdapter(
                    DEFAULT_FIXTURE, ("Hardcore Mirage", "Essence")
                ),
                concurrency=3,
            )
            self.assertEqual(result["status"], "partial")
            connection = sqlite3.connect(database)
            after, stale, error = connection.execute(
                """
                SELECT latest_snapshot_id, stale, last_error
                FROM economy_league_status WHERE league_key = 'hardcore-mirage'
                """
            ).fetchone()
            self.assertEqual(after, before)
            self.assertEqual(stale, 1)
            self.assertIn("Essence", error)
            connection.close()
            report = validate_database(database)
            self.assertTrue(report["ok"], report)

    def test_missing_required_croaker_and_parse_errors_are_league_local(self) -> None:
        for label, adapter in (
            (
                "missing",
                FrozenAdapter(
                    DEFAULT_FIXTURE,
                    omit_required=("Hardcore Mirage", "Beast"),
                ),
            ),
            (
                "malformed",
                FrozenAdapter(
                    DEFAULT_FIXTURE,
                    malformed=("Hardcore Mirage", "Beast"),
                ),
            ),
        ):
            with self.subTest(label=label), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                game = root / "game.db"
                database = root / "economy.db"
                _create_game_database(game)
                ingest_fixture(
                    database,
                    raw_root=root / "raw",
                    game_database=game,
                    force=True,
                )
                connection = sqlite3.connect(database)
                before = connection.execute(
                    """
                    SELECT latest_snapshot_id FROM economy_league_status
                    WHERE league_key = 'hardcore-mirage'
                    """
                ).fetchone()[0]
                connection.close()

                result = refresh_all_leagues(
                    database,
                    raw_root=root / "raw",
                    game_database=game,
                    adapter=adapter,
                )
                self.assertEqual(result["status"], "partial")
                failed = next(
                    row
                    for row in result["leagues"]
                    if row["league_key"] == "hardcore-mirage"
                )
                self.assertEqual(failed["failed_categories"], ["Beast"])
                self.assertIn("Beast", failed["failure_details"])
                connection = sqlite3.connect(database)
                after, stale = connection.execute(
                    """
                    SELECT latest_snapshot_id, stale FROM economy_league_status
                    WHERE league_key = 'hardcore-mirage'
                    """
                ).fetchone()
                self.assertEqual(after, before)
                self.assertEqual(stale, 1)
                connection.close()
                report = validate_database(database)
                self.assertTrue(report["ok"], report)

    def test_refresh_migrates_a_restored_schema_v1_checkpoint(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            game = root / "game.db"
            database = root / "economy.db"
            _create_game_database(game)
            connection = sqlite3.connect(database)
            connection.executescript(DEFAULT_SCHEMA.read_text(encoding="utf-8"))
            connection.execute(
                "INSERT INTO economy_manifest VALUES (1, 1, 'old-tool', '2026-07-15T00:00:00Z')"
            )
            connection.execute("INSERT INTO economy_maintenance VALUES (1, 0)")
            connection.execute(
                """
                INSERT INTO economy_source VALUES (
                    'poe-ninja', 'poe.ninja', 'old-adapter', 'https://poe.ninja/data'
                )
                """
            )
            connection.commit()
            connection.close()

            result = refresh_all_leagues(
                database,
                raw_root=root / "raw",
                game_database=game,
                adapter=FrozenAdapter(DEFAULT_FIXTURE),
            )
            self.assertEqual(result["status"], "complete")
            connection = sqlite3.connect(database)
            self.assertEqual(
                connection.execute(
                    "SELECT schema_version FROM economy_manifest WHERE singleton = 1"
                ).fetchone()[0],
                2,
            )
            self.assertEqual(
                connection.execute(
                    "SELECT chaos_value FROM economy_owner_default WHERE canonical_price_key = 'beast:rare'"
                ).fetchone()[0],
                1.0,
            )
            self.assertEqual(
                connection.execute(
                    "SELECT adapter_version FROM economy_source WHERE source_key = 'poe-ninja'"
                ).fetchone()[0],
                ADAPTER_VERSION,
            )
            connection.close()

    def test_publish_and_retention_dry_run_then_apply(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            game = root / "game.db"
            database = root / "economy.db"
            _create_game_database(game)
            ingest_fixture(
                database,
                raw_root=root / "raw",
                game_database=game,
                force=True,
            )
            published = publish_artifacts(
                database,
                output=root / "published",
                generated_at_utc="2026-07-15T00:00:00Z",
            )
            self.assertEqual(published["leagues"], 3)
            index = json.loads(Path(published["index"]).read_text(encoding="utf-8"))
            self.assertEqual(index["schema_version"], 1)
            self.assertTrue(all(row["latest_snapshot_url"] for row in index["leagues"]))

            connection = sqlite3.connect(database)
            self.assertEqual(
                connection.execute(
                    "SELECT COUNT(*) FROM economy_snapshot_reference"
                ).fetchone()[0],
                0,
            )
            for content_hash, created in (
                ("a" * 64, "2026-01-01T00:00:00Z"),
                ("b" * 64, "2026-01-02T00:00:00Z"),
            ):
                snapshot_id = f"economy:standard:{content_hash}"
                connection.execute(
                    """
                    INSERT INTO economy_snapshot(
                        snapshot_id, league_key, source_key, created_at_utc,
                        source_cutoff_at_utc, game_data_hash, price_count,
                        missing_count, low_confidence_count, content_sha256,
                        artifact_json, status
                    ) VALUES (?, 'standard', 'poe-ninja', ?, ?, 'fixture-game-data',
                              0, 0, 0, ?, '{}', 'building')
                    """,
                    (snapshot_id, created, created, content_hash),
                )
                connection.execute(
                    "UPDATE economy_snapshot SET status = 'complete' WHERE snapshot_id = ?",
                    (snapshot_id,),
                )
            connection.commit()
            connection.close()
            now = datetime(2026, 3, 15, tzinfo=timezone.utc)
            dry = retention_report(database, now=now)
            self.assertEqual(dry["delete_snapshots"], [f"economy:standard:{'a' * 64}"])
            applied = retention_report(database, now=now, apply=True)
            self.assertFalse(applied["dry_run"])
            connection = sqlite3.connect(database)
            self.assertIsNone(
                connection.execute(
                    "SELECT 1 FROM economy_snapshot WHERE content_sha256 = ?",
                    ("a" * 64,),
                ).fetchone()
            )
            connection.close()


if __name__ == "__main__":
    unittest.main()
