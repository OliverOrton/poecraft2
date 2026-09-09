"""Bounded projections of retained reports; does not run a solver or issue proof."""
import argparse
import hashlib
import json
from pathlib import Path


def summarize(directory):
    labels = ["B0-empty-reuse", "B1-coupled-domain", "B2-unconsumed",
              "B3-snapshot-domain", "B4-flag-domain", "B5-snapshot-no-retention",
              "B6-final-reuse"]
    runs = {}
    identity = None
    for label in labels:
        path = directory / (label + ".json")
        report = json.loads(path.read_text(encoding="utf-8"))["cases"][0]
        request = dict(report["input"])
        request.pop("native_retention_diagnostic", None)
        if identity is None:
            identity = request
        assert request == identity, "a primary input changed"
        telemetry = report.get("solver_telemetry") or {}
        attribution = telemetry.get("carrier_bound_attribution") or {}
        timings = telemetry.get("timings_ns") or {}
        refinement = telemetry.get("policy_refinement") or {}
        exact = report.get("exact_strategy_evaluation") or {}
        process = json.loads((directory / (label + ".process.json")).read_text())
        graphs = list((directory / (label + "-strategy.json")).glob("*.strategy.json"))
        assert len(graphs) <= 1
        runs[label] = {
            "report_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
            "status": report["actual_status"],
            "compiled_graph_sha256": hashlib.sha256(graphs[0].read_bytes()).hexdigest() if graphs else None,
            "setup_seconds": timings["solve_setup"] / 1e9 if "solve_setup" in timings else None,
            "phase_wall_ms": report["phase_wall_ms"],
            "process": process,
            "summary": report["solve_summary"],
            "exact_evaluation": {k: v for k, v in exact.items() if k != "result"},
            "core_candidate": refinement.get("core_policy"),
            "direct_certification": refinement.get("direct_certification"),
            "strict_lift": refinement.get("strict_lift"),
            "native_owners": attribution.get("populations", {}).get("expanded", {})
                .get("selected_component_owners", {}).get("native_retention"),
            "partial_samples": [s for s in attribution.get("lower_samples", [])
                if s.get("satisfied_goal_mask", 0) or s.get("goal_progress_retry_basin", 0)][:10],
            "errors": report["errors"],
        }
    return {"schema": "empty_start_continuation_checkpoint_v1", "input": identity,
            "source": json.loads((directory / "final-source.json").read_text()),
            "limitations": [
                "B0 executable is preserved from baseline main; B6 has a frozen engine patch and executable SHA-256.",
                "B1-B5 intermediate executable/source hashes were not captured. Their mutation sequence is recorded in README; do not claim byte-exact intermediate source provenance.",
                "Single timed observations, not a timing distribution. B5 watchdog failure has no published bounds.",
                "Causal partial-to-root improvement and new native exact closure remain unproved."
            ], "runs": runs}


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("directory", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.write_text(json.dumps(summarize(args.directory), indent=2) + "\n", encoding="utf-8", newline="\n")
