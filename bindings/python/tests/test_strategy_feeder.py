from __future__ import annotations

import copy
import json
import os
from pathlib import Path
import unittest

from poecraft_engine import SimulationOptions, load_data, load_economy


ARTIFACT = Path(os.environ.get(
    "POECRAFT_TEST_ARTIFACT",
    Path(__file__).resolve().parents[3] / "data" / "compiled" / "current",
))
BASE = "Metadata/Items/Armours/BodyArmours/BodyInt17"


def paid_parent(*, output_rarity="magic", child_terminal="success"):
    """Public native JSON, without the frontend pinning/validation implementation."""
    base_state = {"base_key": BASE, "item_level": 86, "rarity": "normal"}
    child = {
        "version": "v1", "name": "Paid child", "description": "",
        "start_node_id": "start", "base_state": base_state,
        "output_contracts": [{"id": "ready", "base_key": BASE,
                              "predicate": {"type": "rarity_is", "rarity": output_rarity}}],
        "nodes": [
            {"id": "start", "kind": "start"},
            {"id": "child-craft", "kind": "operation",
             "operation": {"type": "transmute", "params": {}}},
            {"id": "end", "kind": "terminal", "terminal": child_terminal},
        ],
        "edges": [
            {"id": "begin", "from": "start", "to": "child-craft", "priority": 0},
            {"id": "done", "from": "child-craft", "to": "end", "priority": 0},
        ],
    }
    parent = copy.deepcopy(child)
    parent["name"] = "Parent"
    parent["output_contracts"] = []
    parent["nodes"][1]["id"] = "invoke"
    parent["nodes"][1]["operation"] = {
        "type": "invoke_feeder", "params": {"resource_id": "feeder"},
    }
    parent["nodes"][-1]["terminal"] = "success"
    parent["edges"][0]["to"] = parent["edges"][1]["from"] = "invoke"
    parent["resources"] = [{
        "id": "feeder", "base_state": base_state,
        "acquisition_price_key": "resource:feeder",
        "feeder": {"strategy_id": "saved-child", "revision": "r1",
                   "document_json": json.dumps(child), "output_contract_id": "ready"},
    }]
    return parent


@unittest.skipUnless(ARTIFACT.exists(), "compiled engine artifact is absent")
class FeederBindingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data = load_data(ARTIFACT)
        cls.session = cls.data.create_session(BASE, 86)
        cls.economy = load_economy({"version": "v1", "id": "feeder-fixed-quotes",
                                  "prices": {"resource:feeder": 7, "transmute": 2}})

    @classmethod
    def tearDownClass(cls):
        cls.economy.close()
        cls.session.close()
        cls.data.close()

    def execute(self, document, *, target_runs=1, **limits):
        with self.session.compile_strategy(document) as strategy:
            with strategy.create_simulator(self.economy) as simulator:
                return simulator.run(SimulationOptions(
                    target_runs=target_runs, seed=42, retained_trace_count=2,
                    max_trace_entries=30, **{"max_actions_per_run": 10, **limits},
                ), chunk_size=37)

    def test_paid_child_rows_and_actual_resource_decode(self):
        result = self.execute(paid_parent(), target_runs=1000)
        self.assertEqual(result.summary["success_count"], 1000)
        self.assertEqual(result.summary["total_actions"], 2000)
        self.assertEqual(result.summary["known_total_cost"], 9000)
        self.assertEqual(result.summary["cost_status"], "complete")
        # Exercises the native two-pass row count and ctypes decoding, including
        # a child node whose raw action code differs from the parent vocabulary.
        rows = {(row["node_id"], row["action_type"]): row["count"]
                for row in result.action_distribution}
        self.assertEqual(rows, {("invoke", 1100): 1000,
                                ('"invoke"/"child-craft"', 0): 1000})
        self.assertEqual(sum(rows.values()), result.summary["total_actions"])
        materials = {row["price_key"]: row["count"]
                     for row in result.sampled_accounting["materials"]}
        self.assertEqual(materials, {"resource:feeder": 1000, "transmute": 1000})
        output = result.traces[0].entries[-1].resources[0]
        self.assertEqual((output["base_key"], output["item_level"]), (BASE, 86))
        self.assertEqual((output["identity"], output["acquisitions"]), ("feeder/1", 1))
        self.assertEqual(output["item"]["rarity"], 1)
        self.assertEqual(output["lifecycle"], 0)
        self.assertTrue(output["feeder"]["output_accepted"])
        self.assertEqual(output["feeder"]["revision"], "r1")
        self.assertEqual(output["feeder"]["known_cost"], 9)

    def test_failed_or_mismatched_child_retains_paid_actual_item(self):
        for document in (paid_parent(output_rarity="rare"),
                         paid_parent(child_terminal="failure")):
            with self.subTest(document=document["resources"][0]["feeder"]["document_json"]):
                result = self.execute(document)
                self.assertEqual(result.summary["failure_count"], 1)
                self.assertEqual(result.summary["total_actions"], 2)
                self.assertEqual(result.summary["known_total_cost"], 9)
                self.assertEqual(sum(row["count"] for row in result.action_distribution), 2)
                output = result.traces[0].entries[-1].resources[0]
                self.assertFalse(output["feeder"]["output_accepted"])
                self.assertEqual(output["item"]["rarity"], 1)
                self.assertEqual(output["lifecycle"], 0)

    def test_child_uses_absolute_parent_action_cost_and_step_limits(self):
        for limits, counter in (
            ({"max_actions_per_run": 1}, "action_limit_count"),
            ({"max_cost_per_run": 7}, "cost_limit_count"),
            ({"max_graph_steps_per_run": 3}, "step_limit_count"),
        ):
            with self.subTest(limits=limits):
                result = self.execute(paid_parent(), **limits)
                self.assertEqual(result.summary[counter], 1)
                self.assertEqual(result.summary["total_actions"], 1)
                self.assertEqual(result.summary["known_total_cost"], 7)
                self.assertFalse(result.traces[0].entries[-1].resources[0]
                                 ["feeder"]["output_accepted"])

    def test_move_recycles_real_item_and_next_invocation_pays_again(self):
        document = paid_parent()
        document["nodes"].extend([
            {"id": "discard", "kind": "operation", "operation": {
                "type": "discard_resource", "params": {"resource_id": "current"}}},
            {"id": "move", "kind": "operation", "operation": {
                "type": "move_resource", "params": {"from": "feeder", "to": "current"}}},
        ])
        document["edges"][1]["to"] = "discard"
        document["edges"].extend([
            {"id": "empty", "from": "discard", "to": "move", "priority": 0},
            {"id": "again", "from": "move", "to": "invoke", "priority": 0},
        ])
        result = self.execute(document, max_actions_per_run=6)
        self.assertEqual(result.summary["action_limit_count"], 1)
        self.assertEqual(result.summary["total_actions"], 6)
        self.assertEqual(result.summary["known_total_cost"], 18)
        entries = result.traces[0].entries
        moved = next(entry for entry in entries if entry.node_id == "move")
        self.assertEqual(moved.known_cumulative_cost, 9)
        self.assertEqual(moved.item._state.rarity, 1)
        self.assertEqual((moved.resources[0]["identity"], moved.resources[0]["lifecycle"]),
                         ("feeder/1", 1))
        replacement = entries[-1].resources[0]
        self.assertEqual((replacement["identity"], replacement["acquisitions"]),
                         ("feeder/2", 2))
        self.assertEqual(sum(row["count"] for row in result.action_distribution), 6)
        self.assertEqual({row["price_key"]: row["count"]
                          for row in result.sampled_accounting["materials"]},
                         {"resource:feeder": 2, "transmute": 2})

    def test_exact_evaluation_refuses_inventory_control_projection(self):
        with self.session.compile_strategy(paid_parent()) as strategy:
            with self.assertRaisesRegex(RuntimeError, "inventory/control identity"):
                strategy.evaluate(economy=self.economy)

    def test_native_compiler_refuses_missing_pinned_output_contract(self):
        document = paid_parent()
        document["resources"][0]["feeder"]["output_contract_id"] = "absent"
        with self.assertRaisesRegex(RuntimeError, "output contract"):
            with self.session.compile_strategy(document):
                self.fail("native compiler accepted a missing pinned output contract")

    def test_named_child_pair_output_and_additive_example_inventory(self):
        # The child has two paid source dots and a selected named pair output.
        # Its fixed root stays normal: accepting that root would fail this test.
        parent = paid_parent(output_rarity="rare")
        child = json.loads(parent["resources"][0]["feeder"]["document_json"])
        rare = {"base_key": BASE, "item_level": 86, "rarity": "rare"}
        child["resources"] = [
            {"id": slot, "base_state": rare, "acquisition_price_key": f"resource:{slot}"}
            for slot in ("a", "b", "out")
        ]
        child["nodes"] = [
            {"id": "start", "kind": "start"},
            *({"id": slot, "kind": "operation", "source_only": True,
               "operation": {"type": "acquire_resource", "params": {"resource_id": slot}}}
              for slot in ("a", "b")),
            {"id": "pair", "kind": "operation", "operation": {
                "type": "recombination", "params": {"input_a": "current", "input_b": "", "output": "out"}}},
            {"id": "end", "kind": "terminal", "terminal": "success"},
        ]
        child["edges"] = [
            {"id": "entry", "from": "start", "to": "pair", "kind": "control", "to_port": "input_a"},
            *({"id": f"supply-{slot}", "from": slot, "to": "pair", "kind": "item", "to_port": f"input_{slot}"}
              for slot in ("a", "b")),
            {"id": "done", "from": "pair", "to": "end", "condition": {"type": "rarity_is", "rarity": "rare"}},
        ]
        child["output_contracts"][0]["resource_id"] = "out"
        parent["resources"][0]["feeder"]["document_json"] = json.dumps(child)
        with load_economy({"version": "v1", "prices": {
                "resource:feeder": 7, "resource:a": 4, "resource:b": 5,
                "recombination:gold": 999, "recombination:dust": 999}}) as economy:
            with self.session.compile_strategy(parent) as strategy:
                with strategy.create_simulator(economy) as simulator:
                    result = simulator.run(SimulationOptions(target_runs=1, seed=42,
                        max_actions_per_run=10, retained_trace_count=1, max_trace_entries=30))
        self.assertEqual(result.summary["success_count"], 1)
        self.assertEqual(result.summary["total_actions"], 4)
        self.assertEqual(result.summary["known_total_cost"], 16)
        self.assertEqual(result.summary["cost_status"], "incomplete")
        trace_output = result.traces[0].entries[-1].resources[0]
        self.assertEqual(trace_output["item"]["rarity"], 2)
        self.assertTrue(trace_output["feeder"]["output_accepted"])
        self.assertFalse(trace_output["feeder"]["cost_complete"])
        self.assertEqual(trace_output["feeder"]["returned_resource_id"], "out")
        self.assertEqual(trace_output["feeder"]["child_resources"][2]["identity"], "recomb/pair/1")
        self.assertEqual(result.success_examples[0].resources[0], trace_output)

    def test_two_paid_ports_mixed_base_example_and_unknown_station_cap(self):
        other = "Metadata/Items/Armours/BodyArmours/BodyInt16"
        document = paid_parent()
        document["resources"] = [
            {"id": "a", "base_state": {"base_key": BASE, "item_level": 86, "rarity": "rare"}, "acquisition_price_key": "resource:a"},
            {"id": "b", "base_state": {"base_key": other, "item_level": 60, "rarity": "rare"}, "acquisition_price_key": "resource:b"},
            {"id": "out", "base_state": document["base_state"], "acquisition_price_key": "resource:out"},
        ]
        document["nodes"] = [
            {"id": "start", "kind": "start"},
            *({"id": slot, "kind": "operation", "source_only": True,
               "operation": {"type": "acquire_resource", "params": {"resource_id": slot}}}
              for slot in ("a", "b")),
            {"id": "pair", "kind": "operation", "operation": {"type": "recombination", "params": {"input_a": "current", "input_b": "", "output": "out"}}},
            {"id": "same", "kind": "terminal", "terminal": "success"},
            {"id": "other", "kind": "terminal", "terminal": "success"},
        ]
        document["edges"] = [
            {"id": "entry", "from": "start", "to": "pair", "to_port": "input_a"},
            *({"id": f"supply-{slot}", "from": slot, "to": "pair", "kind": "item", "to_port": f"input_{slot}"}
              for slot in ("a", "b")),
            {"id": "same", "from": "pair", "to": "same", "priority": 0, "condition": {"type": "base_is", "base_key": BASE}},
            {"id": "other", "from": "pair", "to": "other", "priority": 1, "is_default": True},
        ]
        with load_economy({"version": "v1", "prices": {"resource:a": 4, "resource:b": 5}}) as economy:
            with self.session.compile_strategy(document) as strategy:
                for cost_cap in (0, 10000):
                    with self.subTest(cost_cap=cost_cap), strategy.create_simulator(economy) as simulator:
                        result = simulator.run(SimulationOptions(target_runs=1, seed=42,
                            max_actions_per_run=10, max_cost_per_run=cost_cap,
                            retained_trace_count=1, max_trace_entries=30))
                        if cost_cap:
                            self.assertEqual(result.summary["missing_price_run_count"], 1)
                            self.assertEqual(result.summary["total_actions"], 0)
                            self.assertEqual(result.summary["known_total_cost"], 0)
                            continue
                        self.assertEqual(result.summary["success_count"], 1)
                        self.assertEqual(result.summary["total_actions"], 3)
                        self.assertEqual(result.summary["known_total_cost"], 9)
                        self.assertEqual(result.summary["cost_status"], "incomplete")
                        example = result.success_examples[0]
                        actual = example.resources[2]
                        self.assertEqual(actual["item_level"], 75)
                        self.assertEqual(actual["item"]["item_level"], 75)
                        self.assertEqual(actual["lifecycle"], 0)
                        self.assertEqual(actual["identity"], "recomb/pair/1")
                        self.assertEqual([resource["lifecycle"] for resource in example.resources[:2]], [1, 1])
                        self.assertEqual(example.terminal_node_id, "same" if actual["base_key"] == BASE else "other")
