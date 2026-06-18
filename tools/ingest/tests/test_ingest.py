from __future__ import annotations

import hashlib
import json
from pathlib import Path
import tempfile
import unittest

from poecraft_ingest.repo_loader import SOURCE_SPECS, load_source_snapshot
from poecraft_ingest.validation import (
    UnsupportedFeatureError,
    query_normal_rollable_mods,
    validate_database,
)
from poecraft_ingest.write_sqlite import build_database


SCHEMA_PATH = (
    Path(__file__).resolve().parents[3] / "schemas" / "sqlite" / "001_initial.sql"
)


def _write_json(path: Path, value: object) -> None:
    path.write_text(
        json.dumps(value, separators=(",", ":"), sort_keys=True),
        encoding="utf-8",
    )


def _fixture_sources() -> dict[str, object]:
    return {
        "mods.json": {
            "ArmourImplicit": {
                "adds_tags": [],
                "domain": "item",
                "generation_type": "unique",
                "generation_weights": [],
                "groups": ["ArmourImplicit"],
                "implicit_tags": [],
                "is_essence_only": False,
                "name": "",
                "required_level": 1,
                "spawn_weights": [],
                "stats": [{"id": "base_armour", "min": 10, "max": 10}],
                "text": "+10 Armour",
                "type": "ArmourImplicit",
            },
            "LifePrefix1": {
                "adds_tags": [],
                "domain": "item",
                "generation_type": "prefix",
                "generation_weights": [],
                "groups": ["MaximumLife"],
                "implicit_tags": ["life"],
                "is_essence_only": False,
                "name": "Healthy",
                "required_level": 1,
                "spawn_weights": [
                    {"tag": "body_armour", "weight": 1000},
                    {"tag": "default", "weight": 0},
                ],
                "stats": [
                    {"id": "base_maximum_life", "min": 3, "max": 9}
                ],
                "text": "+(3-9) to maximum Life",
                "type": "MaximumLife",
            },
            "CraftedLife": {
                "adds_tags": [],
                "domain": "crafted",
                "generation_type": "prefix",
                "generation_weights": [],
                "groups": ["MaximumLife"],
                "implicit_tags": ["life"],
                "is_essence_only": False,
                "name": "Crafted",
                "required_level": 1,
                "spawn_weights": [{"tag": "default", "weight": 1000}],
                "stats": [
                    {"id": "base_maximum_life", "min": 10, "max": 10}
                ],
                "text": "+10 to maximum Life",
                "type": "MaximumLife",
            },
        },
        "base_items.json": {
            "Metadata/Items/Armours/BodyArmours/TestArmour": {
                "domain": "item",
                "drop_level": 1,
                "implicits": ["ArmourImplicit"],
                "inventory_height": 3,
                "inventory_width": 2,
                "item_class": "BodyArmour",
                "name": "Test Armour",
                "release_state": "released",
                "tags": ["body_armour", "default"],
            },
            "Metadata/Items/Jewels/JewelPassiveTreeExpansionLarge": {
                "domain": "affliction_jewel",
                "drop_level": 1,
                "implicits": [],
                "inventory_height": 1,
                "inventory_width": 1,
                "item_class": "Jewel",
                "name": "Large Cluster Jewel",
                "release_state": "released",
                "tags": ["expansion_jewel_large", "default"],
            },
            "Metadata/Items/Unreleased": {
                "domain": "item",
                "implicits": [],
                "item_class": "BodyArmour",
                "name": "Unreleased",
                "release_state": "unreleased",
                "tags": ["default"],
            },
        },
        "tags.json": ["default", "body_armour", "life"],
        "fossils.json": {
            "Metadata/Items/Currency/TestFossil": {
                "added_mods": [],
                "allowed_tags": [],
                "blocked_descriptions": {},
                "changes_quality": False,
                "corrupted_essence_chance": 0,
                "descriptions": {},
                "forbidden_tags": [],
                "forced_mods": [],
                "mirrors": False,
                "name": "Test Fossil",
                "negative_mod_weights": [],
                "positive_mod_weights": [{"tag": "life", "weight": 1000}],
                "rolls_lucky": False,
                "rolls_white_sockets": False,
                "sell_price_mods": [],
            }
        },
        "essences.json": {
            "Metadata/Items/Currency/TestEssence": {
                "level": 1,
                "mods": {"Body_Armour": "LifePrefix1"},
                "name": "Test Essence",
                "spawn_level_min": 1,
                "type": {"is_corruption_only": False, "tier": 1},
            }
        },
        "item_classes.json": {
            "BodyArmour": {
                "category": "Armour",
                "category_id": "Armour",
                "name": "Body Armours",
            },
            "Jewel": {
                "category": "Jewel",
                "category_id": "Jewel",
                "name": "Jewels",
            },
        },
        "crafting_bench_options.json": [
            {
                "actions": {"add_explicit_mod": "CraftedLife"},
                "bench_tier": 1,
                "cost": {"Metadata/Items/Currency/Test": 1},
                "item_classes": ["Body Armour"],
                "master": "Test",
            }
        ],
        "stat_translations.json": [{"ids": ["base_maximum_life"]}],
        "stats.json": {
            "base_armour": {"is_local": True},
            "base_maximum_life": {"is_local": False},
        },
        "mod_types.json": {
            "ArmourImplicit": {},
            "MaximumLife": {},
        },
        "cluster_jewels.json": {
            "Metadata_Items_Jewels_JewelPassiveTreeExpansionLarge": {
                "max_skills": 12,
                "min_skills": 8,
                "name": "Large Cluster Jewel",
                "notable_indices": [4],
                "passive_skills": [
                    {
                        "id": "affliction_test",
                        "name": "Test Damage",
                        "stats": {"damage_+%": 10},
                        "stat_text": ["10% increased Damage"],
                        "tag": "affliction_test",
                    }
                ],
                "size": "Large",
                "small_indices": [0],
                "socket_indices": [4],
                "total_indices": 12,
            }
        },
        "cluster_jewel_notables.json": [
            {
                "id": "affliction_notable_test",
                "jewel_stat": "local_jewel_expansion_notable_test",
                "name": "Test Notable",
            }
        ],
    }


class IngestTests(unittest.TestCase):
    def test_ingest_is_deterministic_and_queryable(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "source"
            source.mkdir()
            fixture = _fixture_sources()
            self.assertEqual(
                {spec.logical_name for spec in SOURCE_SPECS},
                set(fixture),
            )
            for name, value in fixture.items():
                _write_json(source / name, value)

            snapshot = load_source_snapshot(source)
            first = root / "first.db"
            second = root / "second.db"
            first_result = build_database(snapshot, first, SCHEMA_PATH)
            second_result = build_database(snapshot, second, SCHEMA_PATH)

            self.assertEqual(
                first_result["data_hash"],
                second_result["data_hash"],
            )
            self.assertEqual(
                hashlib.sha256(first.read_bytes()).hexdigest(),
                hashlib.sha256(second.read_bytes()).hexdigest(),
            )

            report = validate_database(first)
            self.assertTrue(report["ok"], report["errors"])
            self.assertEqual(report["table_counts"]["mod"], 3)
            self.assertEqual(report["table_counts"]["base_item"], 2)
            self.assertEqual(
                report["skip_reasons"]["base_release_state_unreleased"],
                1,
            )
            self.assertEqual(report["cluster"]["runtime_support"], "unsupported")

            pool = query_normal_rollable_mods(first, "Test Armour", 86)
            self.assertEqual(pool["counts"], {"prefix": 1, "suffix": 0, "total": 1})
            self.assertEqual(pool["mods"][0]["mod_key"], "LifePrefix1")

            with self.assertRaises(UnsupportedFeatureError):
                query_normal_rollable_mods(
                    first,
                    "Large Cluster Jewel",
                    84,
                )


if __name__ == "__main__":
    unittest.main()
