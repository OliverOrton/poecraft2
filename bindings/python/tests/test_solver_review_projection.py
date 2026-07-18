from __future__ import annotations

import copy
import json
from pathlib import Path
import unittest

from poecraft_engine import SimulationOptions, load_data
from poecraft_ingest.solver_review import validate_projection


ROOT = Path(__file__).resolve().parents[3]
ARTIFACT = ROOT / "data" / "compiled" / "current"
BASELINE_ROOT = ROOT / "fixtures" / "solver-baselines" / "s8.0"


@unittest.skipUnless(ARTIFACT.exists(), "compiled engine artifact is absent")
class SolverReviewProjectionBindingTests(unittest.TestCase):
    def test_projection_order_and_labels_are_absent_from_native_execution(self):
        projection_path = (
            BASELINE_ROOT
            / "review-projections"
            / "oracle-real-one-mod.review.json"
        )
        projection = json.loads(projection_path.read_text(encoding="utf-8"))
        schema = json.loads(
            (BASELINE_ROOT / "contracts" / "review-projection.schema.json").read_text(
                encoding="utf-8"
            )
        )
        validated = validate_projection(
            projection,
            root=ROOT,
            schema=schema,
            require_complete_coverage=True,
        )
        raw_strategy = json.loads(validated["raw_strategy_json_bytes"])

        relabelled = copy.deepcopy(projection)
        relabelled["sections"].reverse()
        for section in relabelled["sections"]:
            section["label"] = "Native execution cannot observe this label"
            section["entries"].reverse()
        relabelled_validated = validate_projection(
            relabelled,
            root=ROOT,
            schema=schema,
            require_complete_coverage=False,
        )
        self.assertEqual(
            validated["raw_strategy_json_bytes"],
            relabelled_validated["raw_strategy_json_bytes"],
        )

        data = load_data(ARTIFACT)
        session = data.create_session(
            raw_strategy["base_state"]["base_key"],
            raw_strategy["base_state"]["item_level"],
        )
        try:
            results = []
            options = SimulationOptions(
                target_runs=32,
                seed=2026071701,
                max_actions_per_run=10_000,
                retained_trace_count=0,
                retained_success_count=0,
                retained_failure_count=0,
            )
            for compiled_input in (
                raw_strategy,
                json.loads(relabelled_validated["raw_strategy_json_bytes"]),
            ):
                with session.compile_strategy(compiled_input) as strategy:
                    with strategy.create_simulator() as simulator:
                        result = simulator.run(options, chunk_size=16)
                        results.append((result.summary, result.action_distribution))
            self.assertEqual(results[0], results[1])
        finally:
            session.close()
            data.close()


if __name__ == "__main__":
    unittest.main()
