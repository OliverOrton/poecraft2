from __future__ import annotations

import os
import json
import unittest
from pathlib import Path

from poecraft_engine import (
    BestiaryCraftState,
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

    def test_bestiary_compound_state_and_public_presentation(self):
        actions = self.data.bestiary_actions
        self.assertEqual(
            [action.id for action in actions],
            ["bestiary:imprint", "bestiary:restore_imprint"],
        )
        self.assertEqual(actions[0].family_display_name, "Imprint")
        self.assertEqual(
            actions[0].cost_keys,
            (
                "beast:craicic-chimeral",
                "beast:rare",
                "beast:rare",
                "beast:rare",
            ),
        )
        self.assertEqual(actions[1].cost_keys, ())
        options = self.data.bestiary_solver_options
        self.assertEqual(len(options), 1)
        self.assertEqual(options[0].id, "imprint_retry")
        self.assertEqual(
            options[0].checkpoint_restriction_key,
            "magic_checkpoint_carrier",
        )
        self.assertEqual(options[0].checkpoint_rarity, "magic")
        self.assertTrue(options[0].automatic_discovery)
        self.assertTrue(options[0].state_local_discovery)
        self.assertTrue(options[0].resource_bounded)

        item = self.session.create_item(rarity="magic", with_implicits=False)
        state = self.session.create_bestiary_state(item)
        self.assertIsInstance(state, BestiaryCraftState)
        self.assertIs(state.item, item)
        calculation = state.calculate("bestiary:imprint")
        self.assertTrue(calculation.deterministic)
        self.assertEqual(calculation.probability, 1.0)
        self.assertFalse(state.checkpoint_present)
        self.assertTrue(calculation.successor.checkpoint_present)
        self.assertEqual(
            calculation.result.consumed_price_keys, actions[0].cost_keys
        )

        created = state.apply("bestiary:imprint")
        self.assertTrue(created.applied)
        self.assertTrue(state.checkpoint_present)
        refused = state.apply("bestiary:imprint")
        self.assertFalse(refused.applied)
        self.assertEqual(refused.refusal_key, "checkpoint_already_exists")
        self.assertEqual(refused.consumed_price_keys, ())
        self.assertTrue(state.checkpoint_present)

        with self.session.create_action_context(seed=71) as context:
            self.assertTrue(context.apply(state.item, "alteration").applied)
        self.assertGreater(state.item.explicit_count, 0)
        restored = state.apply("bestiary:restore_imprint")
        self.assertTrue(restored.applied)
        self.assertEqual(restored.cost_keys, ())
        self.assertEqual(restored.consumed_price_keys, ())
        self.assertEqual(restored.consumed_checkpoint_count, 1)
        self.assertFalse(state.checkpoint_present)
        self.assertEqual(state.item.explicit_count, 0)

    def test_packaged_manual_economy_uses_runtime_v1_envelope(self):
        path = Path(__file__).resolve().parents[3] / "data" / "economy" / "public-none.json"
        payload = json.loads(path.read_text(encoding="utf-8"))
        self.assertEqual(payload["version"], "v1")
        self.assertEqual(payload["metadata"]["schema_version"], 1)
        with load_economy(payload):
            pass

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

    def test_fracturing_orb_action(self):
        with self.session.create_action_context(seed=19) as context:
            item = self.session.create_item(
                rarity="normal", with_implicits=False
            )
            self.assertTrue(context.apply(item, {"type": "alchemy"}).applied)
            before = set(item.prefix_mod_ids + item.suffix_mod_ids)
            self.assertGreaterEqual(len(before), 4)
            result = context.apply(item, {"type": "fracture"})
            self.assertTrue(result.applied)
            self.assertEqual(result.added, 0)
            self.assertEqual(result.removed, 0)
            self.assertEqual(
                set(item.prefix_mod_ids + item.suffix_mod_ids), before
            )
            self.assertEqual(len(item.fractured_mod_ids), 1)
            self.assertFalse(context.apply(item, {"type": "fracture"}).applied)

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

    def test_performance_timing_is_opt_in(self):
        items = [
            self.session.create_item(rarity="normal", with_implicits=False)
            for _ in range(100)
        ]
        with self.session.create_action_context(seed=101) as context:
            context.apply_batch(items, {"type": "alchemy"})
            stats = context.performance_stats()
            self.assertGreater(stats["pool_requests"], 0)
            self.assertEqual(stats["candidate_build_ns"], 0)
            self.assertEqual(stats["weighted_pool_build_ns"], 0)
            self.assertEqual(stats["sampling_ns"], 0)

        timed_items = [
            self.session.create_item(rarity="normal", with_implicits=False)
            for _ in range(100)
        ]
        with self.session.create_action_context(seed=102) as context:
            context.set_performance_timing(True)
            context.apply_batch(timed_items, {"type": "alchemy"})
            stats = context.performance_stats()
            self.assertGreater(stats["candidate_build_ns"], 0)
            self.assertGreater(stats["weighted_pool_build_ns"], 0)
            self.assertGreater(stats["sampling_calls"], 0)
            self.assertGreater(stats["sampling_ns"], 0)

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
                {
                    "version": "v1",
                    "id": "python-s8.4-accounting",
                    "prices": {"chaos": 2.0},
                }
            ) as economy:
                exact = strategy.evaluate(economy=economy)
                self.assertEqual(exact["accounting"]["version"], "s8.4_v1")
                self.assertEqual(
                    exact["accounting"]["pricing"],
                    {
                        "status": "complete",
                        "economy_id": "python-s8.4-accounting",
                        "missing_price_keys": [],
                    },
                )
                self.assertEqual(
                    exact["accounting"]["totals"]["per_invocation"][
                        "expected_actions"
                    ],
                    exact["expected_actions"],
                )
                self.assertEqual(
                    exact["accounting"]["reconciliation"][
                        "action_descriptor_visits_difference"
                    ],
                    0,
                )
                self.assertEqual(
                    exact["accounting"]["reconciliation"][
                        "material_quantity_differences"
                    ]["chaos"],
                    0,
                )
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
                    self.assertEqual(result.summary["seed"], 42)
                    self.assertEqual(result.summary["target_runs"], 20)
                    self.assertEqual(len(result.action_distribution), 1)
                    self.assertEqual(
                        result.action_distribution[0]["count"],
                        result.summary["total_actions"],
                    )
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
                    self.assertEqual(
                        result.sampled_accounting["evidence_source"],
                        "simulator_sample",
                    )
                    self.assertEqual(result.sampled_accounting["sample_count"], 20)
                    self.assertEqual(result.sampled_accounting["seed"], 42)
                    self.assertEqual(
                        result.sampled_accounting["actions"][0]["action_id"],
                        "chaos",
                    )
                    self.assertEqual(
                        result.sampled_accounting["actions"][0]["count"],
                        result.summary["total_actions"],
                    )
                    self.assertEqual(
                        result.sampled_accounting["materials"][0]["price_key"],
                        "chaos",
                    )
                    self.assertEqual(
                        result.sampled_accounting["materials"][0]["count"],
                        result.summary["total_actions"],
                    )

    def test_phase13_mechanics(self):
        with self.session.create_action_context(seed=42) as context:
            # Bench metamods are real crafted slots and preserve their side.
            item = self.session.create_item(rarity="normal")
            context.apply(item, {"type": "alchemy"})
            prefix_lock = self.session.find_mod(
                "StrMasterItemGenerationCannotChangePrefixes"
            )
            self.assertTrue(
                context.apply(
                    item, {"type": "bench", "mod_key": prefix_lock.key}
                ).applied
            )
            prefixes = item.prefix_mod_ids
            self.assertTrue(context.apply(item, {"type": "chaos"}).applied)
            self.assertTrue(
                set(prefixes).issubset(item.prefix_mod_ids),
                "locked prefixes must survive even when chaos fills an open prefix",
            )

            # Veiled crafts produce three persisted options and unveil in place.
            item = self.session.create_item(rarity="rare")
            self.assertTrue(context.apply(item, {"type": "veiled_exalt"}).applied)
            self.assertEqual(len(item.veiled_option_mod_ids), 3)
            choice = self.session.mod_info(item.veiled_option_mod_ids[0])
            self.assertTrue(
                context.apply(
                    item, {"type": "unveil", "mod_key": choice.key}
                ).applied
            )
            self.assertEqual(item.veiled_option_mod_ids, ())

            # Harvest, Eldritch, and influenced exalts share the native API.
            item = self.session.create_item(rarity="rare")
            self.assertTrue(
                context.apply(
                    item, {"type": "harvest_reforge", "target_tag": "life"}
                ).applied
            )
            item = self.session.create_item(rarity="rare")
            self.assertTrue(
                context.apply(
                    item, {"type": "eldritch_ember", "tier": 1}
                ).applied
            )
            self.assertEqual(item.searing_exarch_tier, 1)
            self.assertTrue(item.implicit_mod_ids)
            self.assertTrue(
                context.apply(
                    item, {"type": "eldritch_ichor", "tier": 2}
                ).applied
            )
            self.assertEqual(item.eater_of_worlds_tier, 2)
            self.assertTrue(context.apply(item, {"type": "eldritch_exalt"}).applied)

            item = self.session.create_item(rarity="rare")
            self.assertTrue(
                context.apply(
                    item,
                    {"type": "influence_exalt", "influence": "crusader"},
                ).applied
            )
            self.assertNotEqual(item.generic_influence_bits, 0)

    def test_phase13_special_fossils(self):
        fossils = {
            "sanctified": "CurrencyDelveCraftingLuckyModRolls",
            "gilded": "CurrencyDelveCraftingSellPrice",
            "bloodstained": "CurrencyDelveCraftingVaal",
            "fractured": "CurrencyDelveCraftingMirror",
        }
        with self.session.create_action_context(seed=99) as context:
            for name, suffix in fossils.items():
                item = self.session.create_item()
                result = context.apply(
                    item,
                    {
                        "type": "fossil",
                        "fossils": [
                            f"Metadata/Items/Currency/{suffix}"
                        ],
                    },
                )
                self.assertTrue(result.applied, name)
                if name == "gilded":
                    self.assertTrue(item.implicit_mod_ids)
                elif name == "bloodstained":
                    self.assertTrue(item.item_flags & 1)
                    self.assertTrue(item.implicit_mod_ids)
                elif name == "fractured":
                    self.assertTrue(item.item_flags & 2)

    def test_fossil_and_essence_ignore_metamods(self):
        fossil = "Metadata/Items/Currency/CurrencyDelveCraftingRandom"
        essence = "Metadata/Items/Currency/CurrencyEssenceAnguish2"
        locks = (
            "StrMasterItemGenerationCannotChangePrefixes",
            "DexMasterItemGenerationCannotChangeSuffixes",
        )
        with self.session.create_action_context(seed=20260717) as context:
            for action in (
                {"type": "fossil", "fossils": [fossil]},
                {"type": "essence", "essence": essence},
            ):
                item = self.session.create_item(
                    rarity="rare", with_implicits=False
                )
                for key in locks:
                    item.add_mod(key)
                lock_ids = set(item.prefix_mod_ids + item.suffix_mod_ids)
                self.assertEqual(len(lock_ids), 2)
                self.assertTrue(context.apply(item, action).applied)
                live = set(item.prefix_mod_ids + item.suffix_mod_ids)
                self.assertTrue(lock_ids.isdisjoint(live))

                fractured = self.session.create_item(
                    rarity="rare", with_implicits=False
                )
                fractured.add_mod(
                    "LocalIncreasedEnergyShield11", fractured=True
                )
                fractured_id = fractured.fractured_mod_ids[0]
                self.assertTrue(context.apply(fractured, action).applied)
                self.assertIn(fractured_id, fractured.fractured_mod_ids)

            respecting = self.session.create_item(
                rarity="rare", with_implicits=False
            )
            for key in locks:
                respecting.add_mod(key)
            lock_ids = set(
                respecting.prefix_mod_ids + respecting.suffix_mod_ids
            )
            self.assertTrue(
                context.apply(respecting, {"type": "chaos"}).applied
            )
            live = set(
                respecting.prefix_mod_ids + respecting.suffix_mod_ids
            )
            self.assertTrue(lock_ids.issubset(live))

    def test_phase13_strategy_operation(self):
        strategy_json = {
            "version": "v1",
            "name": "Harvest once",
            "start_node_id": "start",
            "base_state": {
                "base_key": BASE,
                "item_level": 86,
                "rarity": "rare",
            },
            "nodes": [
                {"id": "start", "kind": "start"},
                {
                    "id": "harvest",
                    "kind": "operation",
                    "operation": {
                        "type": "harvest_reforge",
                        "params": {"target_tag": "life"},
                    },
                },
                {"id": "success", "kind": "terminal", "terminal": "success"},
            ],
            "edges": [
                {"id": "begin", "from": "start", "to": "harvest"},
                {"id": "done", "from": "harvest", "to": "success"},
            ],
        }
        with self.session.compile_strategy(strategy_json) as strategy:
            with strategy.create_simulator() as simulator:
                result = simulator.run(
                    SimulationOptions(target_runs=5, seed=7),
                    chunk_size=2,
                )
                self.assertEqual(result.summary["success_count"], 5)
                self.assertEqual(result.summary["total_actions"], 5)

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
