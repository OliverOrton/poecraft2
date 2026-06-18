from __future__ import annotations

import json
from pathlib import Path
import tempfile
import unittest

from poecraft_ingest.compiled_data import (
    compile_engine_data,
    validate_engine_data,
)
from poecraft_ingest.engine_selection import (
    resolve_base_selection,
    select_session_mods,
    weighted_pool,
)
from poecraft_ingest.repo_loader import load_source_snapshot
from poecraft_ingest.write_sqlite import build_database
from tests.test_ingest import SCHEMA_PATH, _fixture_sources, _write_json


class CompiledDataTests(unittest.TestCase):
    def test_complete_artifact_round_trips_against_sqlite(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "source"
            source.mkdir()
            for name, value in _fixture_sources().items():
                _write_json(source / name, value)
            database = root / "poecraft.db"
            build_database(
                load_source_snapshot(source),
                database,
                SCHEMA_PATH,
            )

            artifact = root / "artifact"
            manifest = compile_engine_data(
                database,
                artifact,
                generated_at_utc="2026-01-01T00:00:00Z",
            )
            self.assertEqual(manifest["artifact_kind"], "runtime_dataset")
            self.assertTrue(manifest["complete_dataset"])
            self.assertEqual(manifest["row_counts"]["mods"], 5)
            self.assertEqual(manifest["row_counts"]["base_items"], 2)
            self.assertEqual(
                manifest["row_counts"]["ordinary_session_bases"],
                1,
            )
            self.assertEqual(
                manifest["row_counts"]["cluster_unsupported_bases"],
                1,
            )
            self.assertIn("domain", manifest["enums"])

            report = validate_engine_data(database, artifact)
            self.assertTrue(report["ok"], report["errors"])
            self.assertEqual(report["declared_counts"], report["sqlite_counts"])

            payload = json.loads(
                (artifact / "game-data.json").read_text(encoding="utf-8")
            )
            self.assertEqual(payload["mods"]["count"], 5)
            self.assertEqual(payload["base_items"]["count"], 2)
            self.assertEqual(
                len(payload["mods"]["global_mod_ids"]),
                payload["mods"]["count"],
            )
            self.assertEqual(
                len(payload["spawn_weights"]["offsets"]),
                payload["mods"]["count"] + 1,
            )

            payload["mods"]["required_levels"].pop()
            (artifact / "game-data.json").write_text(
                json.dumps(payload, indent=2, sort_keys=True) + "\n",
                encoding="utf-8",
            )
            broken = validate_engine_data(database, artifact)
            self.assertFalse(broken["ok"])
            self.assertIn(
                "mods.required_levels length does not match mods.count",
                broken["errors"],
            )

    def test_selection_keeps_reachable_influence_but_base_pool_does_not(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "source"
            source.mkdir()
            for name, value in _fixture_sources().items():
                _write_json(source / name, value)
            database = root / "poecraft.db"
            build_database(
                load_source_snapshot(source),
                database,
                SCHEMA_PATH,
            )

            import sqlite3

            connection = sqlite3.connect(database)
            connection.row_factory = sqlite3.Row
            try:
                base = resolve_base_selection(
                    connection,
                    "Test Armour",
                    86,
                )
                mods = select_session_mods(connection, base)
                self.assertEqual(
                    {mod.key for mod in mods},
                    {"LifePrefix1", "FireSuffix1", "InfluencePrefix1"},
                )
                self.assertEqual(
                    {row["mod_id"] for row in weighted_pool(mods, base.tags)},
                    {"LifePrefix1", "FireSuffix1"},
                )
            finally:
                connection.close()

    def test_generic_selection_covers_ordinary_base_domains(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "source"
            source.mkdir()
            fixture = _fixture_sources()
            fixture["item_classes.json"]["Bow"] = {
                "category": "Weapon",
                "category_id": "Weapon",
                "name": "Bows",
            }
            fixture["item_classes.json"]["AbyssJewel"] = {
                "category": "Jewel",
                "category_id": "Jewel",
                "name": "Abyss Jewels",
            }
            fixture["tags.json"].extend(["bow", "abyss_jewel"])
            fixture["base_items.json"][
                "Metadata/Items/Weapons/TestBow"
            ] = {
                "domain": "item",
                "drop_level": 1,
                "implicits": [],
                "inventory_height": 4,
                "inventory_width": 2,
                "item_class": "Bow",
                "name": "Test Bow",
                "release_state": "released",
                "tags": ["bow", "default"],
            }
            fixture["base_items.json"][
                "Metadata/Items/Jewels/TestAbyss"
            ] = {
                "domain": "abyss_jewel",
                "drop_level": 1,
                "implicits": [],
                "inventory_height": 1,
                "inventory_width": 1,
                "item_class": "AbyssJewel",
                "name": "Test Abyss Jewel",
                "release_state": "released",
                "tags": ["abyss_jewel", "default"],
            }
            fixture["mods.json"]["BowPrefix1"] = {
                "adds_tags": [],
                "domain": "item",
                "generation_type": "prefix",
                "generation_weights": [],
                "groups": ["BowDamage"],
                "implicit_tags": ["attack"],
                "is_essence_only": False,
                "name": "Sharpened",
                "required_level": 1,
                "spawn_weights": [
                    {"tag": "bow", "weight": 1000},
                    {"tag": "default", "weight": 0},
                ],
                "stats": [{"id": "damage_+%", "min": 10, "max": 20}],
                "text": "(10-20)% increased Damage",
                "type": "BowDamage",
            }
            fixture["mods.json"]["AbyssSuffix1"] = {
                "adds_tags": [],
                "domain": "abyss_jewel",
                "generation_type": "suffix",
                "generation_weights": [],
                "groups": ["AbyssDamage"],
                "implicit_tags": ["attack"],
                "is_essence_only": False,
                "name": "Violent",
                "required_level": 1,
                "spawn_weights": [
                    {"tag": "abyss_jewel", "weight": 1000},
                    {"tag": "default", "weight": 0},
                ],
                "stats": [{"id": "damage_+%", "min": 3, "max": 5}],
                "text": "(3-5)% increased Damage",
                "type": "AbyssDamage",
            }
            fixture["stats.json"]["damage_+%"] = {"is_local": False}
            fixture["mod_types.json"]["BowDamage"] = {}
            fixture["mod_types.json"]["AbyssDamage"] = {}
            for name, value in fixture.items():
                _write_json(source / name, value)

            database = root / "poecraft.db"
            build_database(
                load_source_snapshot(source),
                database,
                SCHEMA_PATH,
            )

            import sqlite3

            connection = sqlite3.connect(database)
            connection.row_factory = sqlite3.Row
            try:
                expected = {
                    "Test Armour": {"LifePrefix1", "FireSuffix1"},
                    "Test Bow": {"BowPrefix1"},
                    "Test Abyss Jewel": {"AbyssSuffix1"},
                }
                for base_name, expected_mods in expected.items():
                    base = resolve_base_selection(connection, base_name, 86)
                    mods = select_session_mods(connection, base)
                    actual = {
                        row["mod_id"]
                        for row in weighted_pool(mods, base.tags)
                    }
                    self.assertEqual(actual, expected_mods)
            finally:
                connection.close()


if __name__ == "__main__":
    unittest.main()
