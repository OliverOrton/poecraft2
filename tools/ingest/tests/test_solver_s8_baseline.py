from __future__ import annotations

import gc
import gzip
import hashlib
import json
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[3]
BASELINE_ROOT = ROOT / "fixtures" / "solver-baselines" / "s8.0"


def load_json(path: Path):
    return json.loads(path.read_text(encoding="utf-8"))


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


class SolverS8BaselineTests(unittest.TestCase):
    def test_all_baseline_documents_reload_and_manifest_hashes_match(self):
        documents = {}
        for path in BASELINE_ROOT.rglob("*.json"):
            documents[path] = load_json(path)
        self.assertGreaterEqual(len(documents), 20)

        manifest = documents[BASELINE_ROOT / "manifest.json"]
        self.assertEqual(manifest["schema_version"], "s8_solver_baseline_manifest_v1")
        self.assertEqual(len(manifest["cases"]), 8)
        for entry in manifest["cases"]:
            path = ROOT / entry["path"]
            self.assertEqual(sha256(path.read_bytes()), entry["sha256"])
            self.assertEqual(documents[path]["case_id"], entry["id"])
            self.assertEqual(documents[path]["status"], entry["status"])

        compiled = 0
        for entry in manifest["cases"]:
            baseline = documents[ROOT / entry["path"]]
            identity = baseline["identity"]
            configuration = baseline["configuration"]
            self.assertEqual(len(identity["repository_commit"]), 40)
            self.assertIn("artifact_schema_version", identity["artifact"])
            self.assertIn("source_data_hash", identity["artifact"])
            self.assertIn("game_data_sha256", identity["artifact"])
            self.assertIn("strings_sha256", identity["artifact"])
            self.assertIn("session", configuration)
            self.assertIn("start", configuration)
            self.assertIn("goal", configuration)
            self.assertIn("success_threshold", configuration)
            self.assertIn("solver_options", configuration)
            self.assertIn("action_envelope", configuration)
            self.assertIn("caps", configuration)
            self.assertIn("tolerances_and_seeds", configuration)
            self.assertTrue(baseline["commands"])

            prices = configuration["economy"]["prices"]
            price_sources = configuration["price_sources"]
            self.assertEqual(set(prices), {source["price_key"] for source in price_sources})
            for source in price_sources:
                self.assertEqual(prices[source["price_key"]], source["value"])

            strategy_identity = baseline["strategy"]
            if strategy_identity is None:
                self.assertEqual(baseline["simulator"]["run_count"], 0)
                continue

            compiled += 1
            strategy_path = ROOT / strategy_identity["path"]
            compressed = strategy_path.read_bytes()
            self.assertEqual(sha256(compressed), strategy_identity["gzip_sha256"])
            self.assertEqual(len(compressed), strategy_identity["gzip_bytes"])
            raw = gzip.decompress(compressed)
            self.assertEqual(sha256(raw), strategy_identity["uncompressed_sha256"])
            self.assertEqual(len(raw), strategy_identity["uncompressed_bytes"])
            strategy = json.loads(raw)
            self.assertEqual(strategy["start_node_id"], strategy_identity["start_node_id"])
            self.assertEqual(len(strategy["nodes"]), strategy_identity["nodes"])
            self.assertEqual(len(strategy["edges"]), strategy_identity["edges"])
            self.assertEqual(baseline["simulator"]["run_count"], 10000)
            self.assertEqual(baseline["simulator"]["result"]["runs"], 10000)
            self.assertIsNotNone(baseline["action_diagnostics"])
            del strategy, raw, compressed
            gc.collect()

        self.assertEqual(compiled, 6)

    def test_review_projection_references_raw_graph_and_is_non_executable(self):
        strategy_path = BASELINE_ROOT / "examples" / "evaluator-straight.strategy.json"
        projection = load_json(BASELINE_ROOT / "examples" / "review-projection.example.json")
        strategy_bytes = strategy_path.read_bytes()
        strategy = json.loads(strategy_bytes)
        nodes = {node["id"] for node in strategy["nodes"]}
        edges = {edge["id"] for edge in strategy["edges"]}

        self.assertEqual(projection["raw_strategy"]["sha256"], sha256(strategy_bytes))
        self.assertEqual(projection["raw_strategy"]["execution_authority"], "raw_strategy_only")
        self.assertTrue(all(value is False for value in projection["semantics"].values()))
        for section in projection["sections"]:
            self.assertTrue(section["derived"])
            for reference in section["raw_references"]:
                self._assert_reference_resolves(reference, nodes, edges)
            for projected_entry in section["entries"]:
                for reference in projected_entry["raw_references"]:
                    self._assert_reference_resolves(reference, nodes, edges)

        # Reloading the raw strategy after the projection proves the projection is
        # a separate, non-authoritative document and leaves executable bytes intact.
        self.assertEqual(strategy_bytes, strategy_path.read_bytes())

    def test_accounting_example_reconciles_with_existing_evaluator_total(self):
        accounting = load_json(BASELINE_ROOT / "examples" / "action-accounting.example.json")
        evaluator = load_json(BASELINE_ROOT / "examples" / "evaluator-straight.result.json")["result"]
        strategy_bytes = (BASELINE_ROOT / "examples" / "evaluator-straight.strategy.json").read_bytes()
        strategy = json.loads(strategy_bytes)
        raw_nodes = {node["id"] for node in strategy["nodes"]}
        section_ids = {
            section["id"]
            for section in load_json(BASELINE_ROOT / "examples" / "review-projection.example.json")["sections"]
        }

        self.assertEqual(accounting["strategy_sha256"], sha256(strategy_bytes))
        self.assertEqual(accounting["totals"]["expected_actions"], evaluator["expected_actions"])
        evaluator_consumption = {
            item["key"]: item["quantity"] for item in evaluator["expected_consumption"]
        }
        accounted_consumption = {
            item["price_key"]: item["quantity"]
            for item in accounting["totals"]["by_price_key"]
        }
        self.assertEqual(accounted_consumption, evaluator_consumption)

        expected_cost = 0.0
        for item in accounting["totals"]["by_price_key"]:
            contribution = item["quantity"] * item["unit_price"]
            self.assertAlmostEqual(item["cost_contribution"], contribution)
            expected_cost += contribution
        self.assertAlmostEqual(accounting["totals"]["expected_cost"], expected_cost)
        for entry in accounting["entries"]:
            self.assertIn(entry["raw_node_id"], raw_nodes)
            if "review_section_id" in entry:
                self.assertIn(entry["review_section_id"], section_ids)

        reconciliation = accounting["reconciliation"]
        self.assertEqual(reconciliation["action_difference"], 0.0)
        self.assertEqual(reconciliation["cost_dot_product_difference"], 0.0)
        self.assertTrue(
            all(item["quantity"] == 0.0 for item in reconciliation["consumption_differences"])
        )

    def test_accounting_contract_versions_all_planned_classifications(self):
        schema = load_json(BASELINE_ROOT / "contracts" / "action-accounting.schema.json")
        classifications = set(schema["$defs"]["classification"]["enum"])
        self.assertEqual(
            classifications,
            {
                "retry",
                "restart",
                "setup",
                "cleanup",
                "protection",
                "temporary_blocker",
                "fracture",
                "finishing",
            },
        )

    def test_trimming_contract_requires_provenance_and_explicit_restart(self):
        schema = load_json(BASELINE_ROOT / "contracts" / "trimming-provenance.schema.json")
        required = set(schema["required"])
        self.assertTrue(
            {
                "parent_strategy_sha256",
                "fallback",
                "discovery",
                "selection_threshold",
                "removed",
                "observed_visitation_mass",
                "exact_impact_evaluation",
                "independent_validation",
            }.issubset(required)
        )
        self.assertEqual(schema["properties"]["fallback"]["properties"]["kind"]["const"], "Restart")
        self.assertEqual(
            schema["properties"]["fallback"]["properties"]["selection"]["const"],
            "explicit_user_choice_required",
        )
        sample_parameters = schema["$defs"]["sample_parameters"]["required"]
        self.assertEqual(set(sample_parameters), {"run_count", "seeds", "parameters"})

        provenance = load_json(BASELINE_ROOT / "examples" / "trimming-provenance.example.json")
        self.assertEqual(provenance["marker"], "empirically_trimmed")
        self.assertTrue(provenance["heuristic"])
        self.assertEqual(provenance["fallback"]["kind"], "Restart")
        self.assertEqual(provenance["fallback"]["serialized_action"], {"type": "restart"})
        self.assertEqual(provenance["fallback"]["selection"], "explicit_user_choice_required")
        self.assertTrue(provenance["discovery"]["parameters"])
        self.assertTrue(provenance["discovery"]["seeds"])
        self.assertIn("delta", provenance["exact_impact_evaluation"])
        self.assertTrue(provenance["independent_validation"]["parameters"])
        self.assertTrue(provenance["independent_validation"]["seeds"])
        self.assertTrue(provenance["independent_validation"]["sampled_confidence"])
        self.assertGreater(
            provenance["independent_validation"]["unvisited_branch_nonzero_upper_bound"][
                "probability_upper_bound"
            ],
            0.0,
        )

    def _assert_reference_resolves(self, reference, nodes, edges):
        self.assertEqual(len(reference), 1)
        if "node_id" in reference:
            self.assertIn(reference["node_id"], nodes)
        else:
            self.assertIn(reference["edge_id"], edges)


if __name__ == "__main__":
    unittest.main()
