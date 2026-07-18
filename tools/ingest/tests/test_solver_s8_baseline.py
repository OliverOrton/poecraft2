from __future__ import annotations

import copy
import gc
import gzip
import hashlib
import json
from pathlib import Path
import tempfile
import unittest

from poecraft_ingest.solver_review import (
    REPRESENTATIVE_CASES,
    ReviewProjectionError,
    generate_projection,
    projection_bytes,
    validate_projection,
)


ROOT = Path(__file__).resolve().parents[3]
BASELINE_ROOT = ROOT / "fixtures" / "solver-baselines" / "s8.0"
PROJECTION_ROOT = BASELINE_ROOT / "review-projections"


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

    def test_s8_1_representative_projections_reload_with_complete_traceability(self):
        projection_manifest = load_json(PROJECTION_ROOT / "manifest.json")
        schema = load_json(BASELINE_ROOT / "contracts" / "review-projection.schema.json")
        baseline_manifest = load_json(BASELINE_ROOT / "manifest.json")
        baseline_by_id = {
            entry["id"]: load_json(ROOT / entry["path"])
            for entry in baseline_manifest["cases"]
        }

        expected_cases = [case_id for case_id, _ in REPRESENTATIVE_CASES]
        records = projection_manifest["representatives"]
        self.assertEqual([record["case_id"] for record in records], expected_cases)
        self.assertEqual(projection_manifest["execution_authority"], "raw_strategy_only")
        self.assertEqual(
            sha256((ROOT / projection_manifest["contract"]["path"]).read_bytes()),
            projection_manifest["contract"]["sha256"],
        )

        all_entry_roles = set()
        for record in records:
            path = ROOT / record["projection_path"]
            self.assertEqual(sha256(path.read_bytes()), record["projection_sha256"])
            projection = load_json(path)
            validation = validate_projection(
                projection,
                root=ROOT,
                schema=schema,
                require_complete_coverage=True,
            )
            baseline = baseline_by_id[record["case_id"]]
            self.assertEqual(
                projection["raw_strategy"]["sha256"],
                baseline["strategy"]["gzip_sha256"],
            )
            self.assertEqual(record["evaluator_status"], baseline["evaluator"]["status"])
            self.assertEqual(
                validation["raw_source_bytes"],
                (ROOT / record["raw_strategy_path"]).read_bytes(),
            )
            for section in projection["sections"]:
                self.assertTrue(section["raw_references"])
                for entry in section["entries"]:
                    self.assertTrue(entry["raw_references"])
                    all_entry_roles.add(entry["role"])

        self.assertTrue(
            {"rolling", "fracture", "finishing", "recovery", "restart", "setup"}
            .issubset(all_entry_roles)
        )

    def test_s8_1_generation_is_byte_deterministic_and_refuses_stale_identity(self):
        record = load_json(PROJECTION_ROOT / "manifest.json")["representatives"][0]
        strategy_path = ROOT / record["raw_strategy_path"]
        first = generate_projection(
            ROOT,
            strategy_path,
            projection_id="s8.1-determinism-test",
            expected_sha256=record["raw_strategy_sha256"],
        )
        second = generate_projection(
            ROOT,
            strategy_path,
            projection_id="s8.1-determinism-test",
            expected_sha256=record["raw_strategy_sha256"],
        )
        self.assertEqual(projection_bytes(first), projection_bytes(second))

        with self.assertRaisesRegex(ReviewProjectionError, "sha256 mismatch"):
            generate_projection(
                ROOT,
                strategy_path,
                projection_id="s8.1-stale-test",
                expected_sha256="0" * 64,
            )

    def test_s8_1_retry_scc_stays_together_and_roles_use_only_exact_raw_facts(self):
        schema = load_json(BASELINE_ROOT / "contracts" / "review-projection.schema.json")
        strategy = {
            "version": "v1",
            "name": "Synthetic review vocabulary coverage",
            "base_state": {
                "base_key": "synthetic/base",
                "item_level": 1,
                "rarity": "rare",
            },
            "start_node_id": "start",
            "nodes": [
                {"id": "start", "kind": "start"},
                {"id": "router", "kind": "router"},
                {"id": "roll", "kind": "operation", "operation": {"type": "chaos"}},
                {
                    "id": "protect",
                    "kind": "operation",
                    "operation": {
                        "type": "bench",
                        "mod_key": "StrMasterItemGenerationCannotChangePrefixes",
                    },
                },
                {
                    "id": "block",
                    "kind": "operation",
                    "operation": {"type": "bench", "mod_key": "TemporaryBlocker"},
                },
                {"id": "fracture", "kind": "operation", "operation": {"type": "fracture"}},
                {
                    "id": "finish",
                    "kind": "operation",
                    "operation": {"type": "bench", "mod_key": "GoalB"},
                },
                {
                    "id": "cleanup",
                    "kind": "operation",
                    "operation": {"type": "remove_crafted_mod"},
                },
                {"id": "recover", "kind": "operation", "operation": {"type": "annul"}},
                {"id": "reset", "kind": "operation", "operation": {"type": "scour"}},
                {"id": "setup", "kind": "operation", "operation": {"type": "exalt"}},
                {"id": "success", "kind": "terminal", "terminal": "success"},
                {"id": "failure", "kind": "terminal", "terminal": "failure"},
            ],
            "edges": [
                {"id": "begin", "from": "start", "to": "router", "is_default": True},
                {
                    "id": "goal",
                    "from": "router",
                    "to": "success",
                    "condition": {
                        "type": "all",
                        "conditions": [
                            {"type": "has_mod_family", "family_mod_key": "GoalA", "min_tier": 1},
                            {"type": "has_mod_family", "family_mod_key": "GoalB", "min_tier": 1},
                        ],
                    },
                },
                {"id": "offpolicy", "from": "router", "to": "failure", "is_default": True},
            ],
        }
        routes = {
            "roll": {
                "type": "all",
                "conditions": [
                    {"type": "has_mod_family", "family_mod_key": "GoalA", "min_tier": 1},
                    {"type": "item_flag", "flag": "prefixes_locked"},
                ],
            },
            "protect": {"type": "always"},
            "block": {"type": "always"},
            "fracture": {
                "type": "has_mod_family",
                "family_mod_key": "GoalA",
                "min_tier": 1,
            },
            "finish": {
                "type": "has_mod_family",
                "family_mod_key": "GoalA",
                "min_tier": 1,
            },
            "cleanup": {"type": "always"},
            "recover": {"type": "always"},
            "reset": {"type": "always"},
            "setup": {"type": "always"},
        }
        for node_id, condition in routes.items():
            strategy["edges"].append(
                {
                    "id": f"route-{node_id}",
                    "from": "router",
                    "to": node_id,
                    "condition": condition,
                }
            )
            strategy["edges"].append(
                {
                    "id": f"retry-{node_id}",
                    "from": node_id,
                    "to": "router",
                    "is_default": True,
                }
            )

        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "review-vocabulary.strategy.json"
            path.write_text(json.dumps(strategy), encoding="utf-8")
            projection = generate_projection(
                Path(directory),
                path,
                projection_id="s8.1-review-vocabulary-test",
                expected_sha256=sha256(path.read_bytes()),
            )
            validate_projection(
                projection,
                root=Path(directory),
                schema=schema,
                require_complete_coverage=True,
            )

        main_section = next(
            section
            for section in projection["sections"]
            if {"router", "roll", "protect", "block", "fracture"}.issubset(
                {
                    reference["node_id"]
                    for reference in section["raw_references"]
                    if "node_id" in reference
                }
            )
        )
        main_roles = {entry["role"] for entry in main_section["entries"]}
        self.assertEqual(
            main_roles,
            {
                "rolling",
                "temporary_blocking",
                "protection",
                "fracture",
                "finishing",
                "cleanup",
                "recovery",
                "restart",
                "setup",
            },
        )
        labels = "\n".join(entry["label"] for entry in main_section["entries"])
        self.assertIn("active protection prefixes_locked", labels)
        self.assertIn("deterministic finishing ready", labels)

    def test_s8_1_projection_order_and_labels_cannot_change_execution_identity(self):
        schema = load_json(BASELINE_ROOT / "contracts" / "review-projection.schema.json")
        record = load_json(PROJECTION_ROOT / "manifest.json")["representatives"][0]
        projection = load_json(ROOT / record["projection_path"])
        original = validate_projection(
            projection,
            root=ROOT,
            schema=schema,
            require_complete_coverage=True,
        )

        relabelled = copy.deepcopy(projection)
        relabelled["sections"].reverse()
        for section in relabelled["sections"]:
            section["label"] = "Display label intentionally changed"
            section["entries"].reverse()
            for entry in section["entries"]:
                entry["label"] = "Entry display label intentionally changed"
        relabelled_result = validate_projection(
            relabelled,
            root=ROOT,
            schema=schema,
            require_complete_coverage=False,
        )
        self.assertEqual(
            original["raw_strategy_json_bytes"],
            relabelled_result["raw_strategy_json_bytes"],
        )
        self.assertEqual(original["raw_source_bytes"], relabelled_result["raw_source_bytes"])

        baseline = load_json(BASELINE_ROOT / "baselines" / "oracle-real-one-mod.json")
        self.assertEqual(baseline["evaluator"]["status"], "unsupported")
        self.assertIn("mod_count", baseline["evaluator"]["error"])

    def test_s8_1_malformed_hash_and_unresolved_reference_are_rejected(self):
        schema = load_json(BASELINE_ROOT / "contracts" / "review-projection.schema.json")
        record = load_json(PROJECTION_ROOT / "manifest.json")["representatives"][0]
        projection = load_json(ROOT / record["projection_path"])

        bad_hash = copy.deepcopy(projection)
        bad_hash["raw_strategy"]["sha256"] = "0" * 64
        with self.assertRaisesRegex(ReviewProjectionError, "stale projection"):
            validate_projection(bad_hash, root=ROOT, schema=schema)

        bad_reference = copy.deepcopy(projection)
        bad_reference["sections"][0]["entries"][0]["raw_references"] = [
            {"node_id": "missing-node"}
        ]
        with self.assertRaisesRegex(ReviewProjectionError, "unresolved raw node"):
            validate_projection(bad_reference, root=ROOT, schema=schema)

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
