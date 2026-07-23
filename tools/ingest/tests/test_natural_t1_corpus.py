from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

from poecraft_engine import BaseInfo, load_data
from poecraft_ingest.natural_t1_corpus import (
    CONFIG_SCHEMA,
    _select_bases,
    generate_corpus,
)


ROOT = Path(__file__).resolve().parents[3]
ARTIFACT = ROOT / "data" / "compiled" / "current"
ECONOMY = "apps/web/public/economy/snapshots/9175d37d83d90ab936e572f04c7599afbf18ff6cefc90786a5276da1759cd52f.json"
DIRE_PELT = "Metadata/Items/Armours/Helmets/HelmetDex11"


class NaturalT1CorpusTests(unittest.TestCase):
    def test_base_selection_supports_path_list_class_and_pool(self) -> None:
        bases = [
            BaseInfo(0, "base/a", "A", "Helmet", 1, 0),
            BaseInfo(1, "base/b", "B", "Ring", 1, 0),
            BaseInfo(2, "base/c", "C", "Bow", 1, 0),
            BaseInfo(3, "base/d", "D", "Amulet", 1, 0),
        ]
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "bases.txt").write_text("base/b\n", encoding="utf-8")
            config = {
                "base_selection": {
                    "paths": ["base/a"],
                    "list": "bases.txt",
                    "classes": ["Bow"],
                    "pool": "jewellery",
                },
                "base_pools": {"jewellery": ["base/d"]},
            }
            selected, missing = _select_bases(config, root / "config.json", bases)
        self.assertEqual([base.metadata_path for base in selected], ["base/a", "base/b", "base/c", "base/d"])
        self.assertEqual(missing, [])

    def test_native_query_accepts_natural_and_rejects_bench_goal(self) -> None:
        with load_data(ARTIFACT) as data:
            with data.create_session(DIRE_PELT, 86) as session:
                start = session.create_item(rarity="rare", with_implicits=False)
                natural = session.goal_feasibility(
                    {
                        "version": "v1",
                        "rarity": "rare",
                        "action_mode": "goal_relevant",
                        "min_satisfied_slots": 1,
                        "slots": [{"family_mod_key": "IncreasedLife9", "min_tier": 1}],
                    },
                    start,
                )
                bench = session.goal_feasibility(
                    {
                        "version": "v1",
                        "rarity": "rare",
                        "action_mode": "goal_relevant",
                        "min_satisfied_slots": 1,
                        "slots": [{"family_mod_key": "EinharMasterColdResist3__", "min_tier": 1}],
                    },
                    start,
                )
            with data.create_session("Metadata/Items/Amulets/Amulet7", 86) as session:
                group_conflict = session.goal_feasibility(
                    {
                        "version": "v1",
                        "rarity": "rare",
                        "action_mode": "goal_relevant",
                        "min_satisfied_slots": 2,
                        "slots": [
                            {"family_mod_key": "GlobalChaosGemLevel1", "min_tier": 1},
                            {"family_mod_key": "GlobalColdGemLevel1__", "min_tier": 1},
                        ],
                    },
                    session.create_item(rarity="rare", with_implicits=False),
                )
        self.assertEqual((natural.status, natural.reason), ("feasible", "natural_reforge_witness"))
        self.assertGreater(natural.slot_single_draw_probabilities[0], 0.0)
        self.assertEqual((bench.status, bench.reason), ("infeasible", "no_natural_t1"))
        self.assertEqual(
            (group_conflict.status, group_conflict.reason),
            ("infeasible", "group_conflict"),
        )

    def test_generation_is_byte_deterministic_and_pins_natural_goals(self) -> None:
        config = {
            "schema_version": CONFIG_SCHEMA,
            "corpus_id": "natural-t1-test",
            "output_directory": "ignored",
            "artifact": "data/compiled/current",
            "economy": {"snapshot_path": ECONOMY, "manual_overrides": {"base": 1.0}, "fallback_price": None},
            "base_pools": {"one": [DIRE_PELT]},
            "base_selection": {"pool": "one", "paths": [], "classes": [], "list": None},
            "item_level": 86,
            "start": {"rarity": "rare", "with_implicits": False, "mods": []},
            "natural_filters": {"families": [], "required_tags": [], "excluded_tags": [], "tag_mode": "any"},
            "seed": 17,
            "watchdog_seconds": 30,
            "case_watchdog_seconds": 10,
            "resource_caps": {"max_states": 100, "max_sweeps": 1},
            "explicit_cases": [
                {
                    "id": "deterministic-dire-pelt",
                    "stratum": "one",
                    "base_metadata_path": DIRE_PELT,
                    "goal_family_mod_keys": ["IncreasedLife9"],
                }
            ],
            "strata": [
                {"id": "one", "corpus": "smoke", "goal_count": 1, "side_compositions": ["P"], "cases": 1, "max_attempts": 10}
            ],
        }
        with tempfile.TemporaryDirectory() as directory:
            temp = Path(directory)
            config_path = temp / "config.json"
            config_path.write_text(json.dumps(config), encoding="utf-8")
            first = temp / "first"
            second = temp / "second"
            generate_corpus(config_path, output_override=first)
            generate_corpus(config_path, output_override=second)
            first_files = {path.relative_to(first): path.read_bytes() for path in first.rglob("*.json")}
            second_files = {path.relative_to(second): path.read_bytes() for path in second.rglob("*.json")}
        self.assertEqual(first_files, second_files)
        case = json.loads(first_files[Path("cases/deterministic-dire-pelt.json")])
        self.assertEqual(case["corpus"]["goal_modifier_count"], 1)
        self.assertEqual(case["resolved_natural_t1_goals"][0]["family_tier_index"], 1)
        self.assertEqual(case["resolved_natural_t1_goals"][0]["reach_via"], "base")
        self.assertTrue(case["product_action_envelope"]["bench_goal_slots_forbidden"])


if __name__ == "__main__":
    unittest.main()
