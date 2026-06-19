from __future__ import annotations

import os
import json
import unittest
from pathlib import Path

from poecraft_engine import (
    EngineError,
    SimulationOptions,
    load_data,
    load_economy,
)


ARTIFACT = Path(
    os.environ.get(
        "POECRAFT_TEST_ARTIFACT",
        Path(__file__).resolve().parents[3] / "data" / "compiled" / "current",
    )
)
BASE = "Metadata/Items/Armours/BodyArmours/BodyInt17"
FIXTURES = Path(__file__).resolve().parents[3] / "fixtures" / "spec"


@unittest.skipUnless(ARTIFACT.exists(), "compiled engine artifact is absent")
class BindingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data = load_data(ARTIFACT)
        cls.session = cls.data.create_session(BASE, 86)

    @classmethod
    def tearDownClass(cls):
        cls.session.close()
        cls.data.close()

    def test_minimum_api_and_pool_debug(self):
        with self.session.create_action_context(seed=7) as context:
            item = self.session.create_item(rarity="rare")
            result = context.apply(item, {"type": "chaos"})
            self.assertTrue(result.applied)
            self.assertGreaterEqual(item.explicit_count, 4)
            pool = context.debug_pool(item, {"type": "chaos"})
            self.assertEqual(len(pool), pool.summary["candidate_count"])
            self.assertTrue(all(entry["final_weight"] > 0 for entry in pool))

    def test_core_action_rules(self):
        with self.session.create_action_context(seed=0xC0FFEE) as context:
            item = self.session.create_item(rarity="normal")
            self.assertFalse(context.apply(item, {"type": "exalt"}).applied)
            self.assertEqual(item.explicit_count, 0)

            self.assertTrue(context.apply(item, {"type": "transmute"}).applied)
            self.assertEqual(item.rarity, "magic")
            self.assertIn(item.explicit_count, (1, 2))
            if item.explicit_count == 1:
                self.assertTrue(context.apply(item, {"type": "augment"}).applied)
            self.assertEqual(item.explicit_count, 2)

            self.assertTrue(context.apply(item, {"type": "regal"}).applied)
            self.assertEqual(item.rarity, "rare")
            self.assertEqual(item.explicit_count, 3)
            self.assertTrue(context.apply(item, {"type": "annul"}).applied)
            self.assertEqual(item.explicit_count, 2)
            self.assertTrue(context.apply(item, {"type": "chaos"}).applied)
            self.assertGreaterEqual(item.explicit_count, 4)
            self.assertLessEqual(item.explicit_count, 6)
            self.assertTrue(context.apply(item, {"type": "scour"}).applied)
            self.assertEqual(item.rarity, "normal")
            self.assertEqual(item.explicit_count, 0)

    def test_spec_pool_fixtures(self):
        with self.session.create_action_context(seed=1) as context:
            item = self.session.create_item(rarity="rare", with_implicits=False)
            for fixture_name, side in (
                ("vaal-regalia-ilvl-86-normal-prefix.json", "prefix"),
                ("vaal-regalia-ilvl-86-normal-suffix.json", "suffix"),
            ):
                fixture = json.loads(
                    (FIXTURES / "session-pools" / fixture_name).read_text()
                )
                pool = context.debug_pool(item, {"type": "chaos"}, side=side)
                actual = {
                    (
                        entry["spawn_weight"],
                        entry["generation_multiplier_pct"],
                        entry["final_weight"],
                    ): set()
                    for entry in pool
                }
                for entry in pool:
                    actual[
                        (
                            entry["spawn_weight"],
                            entry["generation_multiplier_pct"],
                            entry["final_weight"],
                        )
                    ].add(entry["key"])
                expected = {
                    (
                        bucket["spawn_weight"],
                        bucket["generation_multiplier_pct"],
                        bucket["final_weight"],
                    ): set(bucket["mod_ids"])
                    for bucket in fixture["pool"]
                }
                self.assertEqual(actual, expected)
                self.assertEqual(
                    pool.summary["candidate_count"],
                    fixture["summary"]["total_count"],
                )
                self.assertEqual(
                    pool.summary["combined_total_weight"],
                    fixture["summary"]["combined_total_weight"],
                )

            combined_fixture = json.loads(
                (
                    FIXTURES
                    / "session-pools"
                    / "vaal-regalia-ilvl-86-alchemy-combined.json"
                ).read_text()
            )
            combined = context.debug_pool(item, {"type": "alchemy"})
            for key in (
                "candidate_count",
                "prefix_total_weight",
                "suffix_total_weight",
                "combined_total_weight",
            ):
                fixture_key = (
                    "total_count" if key == "candidate_count" else key
                )
                self.assertEqual(
                    combined.summary[key],
                    combined_fixture["summary"][fixture_key],
                )

    def test_fractured_reforge_fixture(self):
        fixture = json.loads(
            (
                FIXTURES
                / "action-results"
                / "vaal-regalia-ilvl-86-fractured-reforge.json"
            ).read_text()
        )
        with self.session.create_action_context(seed=23) as context:
            item = self.session.create_item(rarity="rare", with_implicits=False)
            for side in ("prefixes", "suffixes"):
                for row in fixture["input_item"][side]:
                    item.add_mod(
                        row["mod_id"],
                        side=side.removesuffix("es"),
                        fractured=row["fractured"],
                    )

            preserved = self.session.create_item(
                rarity="rare", with_implicits=False
            )
            preserved.add_mod(
                fixture["expected"]["preserved_mod_ids"][0],
                side="prefix",
                fractured=True,
            )
            before = context.debug_pool(preserved, {"type": "chaos"})
            expected = fixture["expected"]["candidate_pool_summary_before_refill"]
            self.assertEqual(before.summary["candidate_count"], expected["total_count"])
            self.assertEqual(
                before.summary["combined_total_weight"],
                expected["combined_total_weight"],
            )

            fractured = self.session.find_mod(
                fixture["expected"]["preserved_mod_ids"][0]
            )
            result = context.apply(item, {"type": "chaos"})
            self.assertTrue(result.applied)
            self.assertIn(fractured.session_mod_id, item.fractured_mod_ids)
            live_ids = set(item.prefix_mod_ids + item.suffix_mod_ids)
            for removed_key in fixture["expected"]["removed_mod_ids"]:
                self.assertNotIn(
                    self.session.find_mod(removed_key).session_mod_id, live_ids
                )

    def test_native_batch_api(self):
        with self.session.create_action_context(seed=99) as context:
            template = self.session.create_item(rarity="normal")
            batch = context.run_batch(template, {"type": "alchemy"}, 2_000)
            self.assertEqual(batch.summary.item_count, 2_000)
            self.assertEqual(batch.summary.applied_count, 2_000)
            self.assertTrue(all(item.rarity == "rare" for item in batch.items))
            chaos = context.apply_batch(batch.items, {"type": "chaos"})
            self.assertEqual(chaos.summary.applied_count, 2_000)
            exalt = context.apply_batch(chaos.items, {"type": "exalt"})
            self.assertGreater(exalt.summary.applied_count, 0)
            self.assertTrue(all(item.explicit_count <= 6 for item in exalt.items))

    def test_native_strategy_simulator(self):
        strategy_json = {
            "version": "v1",
            "name": "Chaos until three prefixes",
            "start_node_id": "start",
            "base_state": {
                "base_key": BASE,
                "item_level": 86,
                "rarity": "rare",
            },
            "nodes": [
                {"id": "start", "kind": "start"},
                {
                    "id": "chaos",
                    "kind": "operation",
                    "operation": {"type": "chaos", "params": {}},
                },
                {"id": "success", "kind": "terminal", "terminal": "success"},
            ],
            "edges": [
                {
                    "id": "begin",
                    "from": "start",
                    "to": "chaos",
                    "priority": 0,
                    "condition": {"type": "always"},
                },
                {
                    "id": "done",
                    "from": "chaos",
                    "to": "success",
                    "priority": 0,
                    "condition": {
                        "type": "prefix_count_range",
                        "min": 3,
                        "max": 3,
                    },
                },
                {
                    "id": "repeat",
                    "from": "chaos",
                    "to": "chaos",
                    "priority": 999,
                    "is_default": True,
                },
            ],
        }
        with self.session.compile_strategy(strategy_json) as strategy:
            with load_economy(
                {"version": "v1", "prices": {"chaos": 2.0}}
            ) as economy:
                with strategy.create_simulator(economy) as simulator:
                    result = simulator.run(
                        SimulationOptions(
                            target_runs=20,
                            seed=42,
                            max_actions_per_run=100,
                            retained_trace_count=3,
                            retained_success_count=2,
                        ),
                        chunk_size=7,
                    )
                    self.assertEqual(result.summary["success_count"], 20)
                    self.assertGreater(result.summary["total_actions"], 20)
                    self.assertEqual(result.summary["cost_status"], "complete")
                    self.assertEqual(len(result.traces), 3)
                    self.assertEqual(
                        result.traces[0].entries[0].matched_edge_id, "begin"
                    )
                    self.assertEqual(
                        result.traces[0].entries[-1].terminal_kind, "success"
                    )
                    self.assertEqual(len(result.success_examples), 2)
                    self.assertEqual(
                        result.success_examples[0].item._state.prefix_count, 3
                    )
                    self.assertEqual(result.missing_prices, {})

    def test_sanctified_is_explicitly_deferred(self):
        with self.session.create_action_context() as context:
            item = self.session.create_item()
            with self.assertRaises(EngineError) as raised:
                context.apply(
                    item,
                    {
                        "type": "fossil",
                        "fossils": [
                            "Metadata/Items/Currency/"
                            "CurrencyDelveCraftingLuckyModRolls"
                        ],
                    },
                )
            self.assertEqual(raised.exception.code, 4)
            self.assertEqual(item.rarity, "normal")

    def test_child_handles_retain_native_parents(self):
        data = load_data(ARTIFACT)
        session = data.create_session(BASE, 86)
        item = session.create_item(rarity="normal")
        context = session.create_action_context(seed=8)
        data.close()
        session.close()
        try:
            result = context.apply(item, {"type": "alchemy"})
            self.assertTrue(result.applied)
            self.assertEqual(item.rarity, "rare")
        finally:
            context.close()


if __name__ == "__main__":
    unittest.main()
