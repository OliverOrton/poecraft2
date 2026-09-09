"""Project retained continuation runs; never launches a solve or issues a proof."""
import argparse
import hashlib
import json
from pathlib import Path


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def preparation(directory):
    runs = {}
    reference = None
    for label in ["C10-history", "C11-eager", "C12-all-rows"]:
        path = directory / (label + ".stdout.txt")
        report = json.loads(path.read_text(encoding="utf-8"))
        if reference is None:
            reference = report
        assert report["coordinates"] == reference["coordinates"]
        assert report["source_lower"] == reference["source_lower"]
        delta = max(abs(a - b) for a, b in zip(report["values"], reference["values"]))
        assert delta < 1e-8
        assert report["proof_peak_bytes"] <= 32 * 1024 * 1024
        runs[label] = {k: report[k] for k in ["source_lower", "model_rounds",
            "proof_peak_bytes", "elapsed_ns", "checked_source_progress"]}
        runs[label].update(report_sha256=digest(path), max_value_delta=delta,
                           relation_count=len(report["relations"]))
    sources = None
    for label in ["C13-compact-control", "C13-compact-treatment"]:
        path = directory / (label + ".stdout.txt")
        report = json.loads(path.read_text(encoding="utf-8"))
        current = [{k: source[k] for k in ["source", "probabilistic_donor"]}
                   for source in report["sources"]]
        if sources is None:
            sources = current
        assert current == sources
        assert report["resources"]["combined_additional_peak_bytes"] <= 32 * 1024 * 1024
        runs[label] = {k: report[k] for k in ["preparation_profile", "resources", "elapsed_ns"]}
        runs[label].update(report_sha256=digest(path), sources=current)
    return {"runs": runs, "authority": "checked auxiliary models; no native upper or exact closure"}


def evaluated_continuations(graph_path, evaluation):
    graph = json.loads(graph_path.read_text(encoding="utf-8"))
    visits = {node["id"]: node for node in evaluation["nodes"]}
    traversals = {edge["id"]: edge["expected_traversals"] for edge in evaluation["edges"]}
    samples = []
    for node in graph["nodes"]:
        observed = visits.get(node["id"], {})
        if node["kind"] != "operation" or node["operation"]["type"] == "chaos":
            continue
        if observed.get("expected_visits", 0) <= 0:
            continue
        samples.append({"node": node, "expected_visits": observed["expected_visits"],
            "classes": observed["classes"][:4],
            "incoming": [{"edge": edge, "expected_traversals": traversals[edge["id"]]}
                for edge in graph["edges"] if edge["to"] == node["id"] and
                traversals.get(edge["id"], 0) > 0][:2]})
        if len(samples) == 3:
            break
    return {"graph_sha256": digest(graph_path), "samples": samples,
            "authority": "positive occupancy in the independently evaluated root graph; not independent entry costs"}


def summarize(directory):
    labels = ["B6-final-reuse", "C1-frontier-primary", "C2-directed-primary",
              "C3-publication-primary", "C4-partial-control", "C4-partial-treatment",
              "C5-exact-anchor", "C6-two-sided", "C9-readiness-primary",
              "C14-eager-primary", "C16-final-partial", "C17-final-anchor"]
    runs, inputs = {}, {}
    for label in labels:
        path = directory / (label + ".json")
        report = json.loads(path.read_text(encoding="utf-8"))["cases"][0]
        case = report["id"]
        identity = {"input": report["input"], "actions": report["product_action_ids"]}
        if case in inputs:
            assert inputs[case] == identity, f"changed matched request: {label}"
        inputs[case] = identity
        telemetry = report["solver_telemetry"]
        envelope = telemetry["incremental_action_envelope"]
        exact = report["exact_strategy_evaluation"]
        graphs = list((directory / (label + "-strategy.json")).glob("*.strategy.json"))
        assert len(graphs) == 1
        assert exact["completed"] and exact["status"] == "matched"
        assert exact["zero_off_policy_mass"] and exact["success_probability"] == 1
        runs[label] = {
            "case": case, "report_sha256": digest(path),
            "graph_sha256": digest(graphs[0]), "summary": report["solve_summary"],
            "setup_seconds": telemetry["timings_ns"]["solve_setup"] / 1e9,
            "total_seconds": report["phase_wall_ms"]["total"] / 1000,
            "memory": report["memory"],
            "exact_evaluation": {k: v for k, v in exact.items() if k != "result"},
            "joint_policy": envelope["joint_anytime_policy"],
            "missing_frontier": envelope["missing_frontier"],
            "refinement": envelope["q_refinement"],
            "strict_lift": telemetry["policy_refinement"]["strict_lift"],
            "core_policy": telemetry["policy_refinement"]["core_policy"],
            "direct_certification": {k: v for k, v in telemetry["policy_refinement"]
                ["direct_certification"].items() if k not in ["offpolicy_state_classes"]},
            "partial_samples": telemetry["carrier_bound_attribution"].get("lower_samples", [])[:10],
            "continuation_provenance": {
                "observational": True,
                "samples": envelope["upper_policy_provenance"].get("samples", [])[:8],
                "full_sample_count": len(envelope["upper_policy_provenance"].get("samples", [])),
            },
            "evaluated_continuations": evaluated_continuations(graphs[0], exact["result"]),
            "process": json.loads((directory / (label + ".process.json")).read_text(encoding="utf-8")),
        }
    before = runs["B6-final-reuse"]["summary"]
    after = runs["C14-eager-primary"]["summary"]
    assert before["lower_bound"] == after["lower_bound"]
    assert after["absolute_optimality_gap"] <= before["absolute_optimality_gap"] * 0.8
    wasm = {}
    for label in ["C8-wasm-primary", "C15-wasm-primary"]:
        path = directory / (label + ".json")
        report = json.loads(path.read_text(encoding="utf-8"))
        assert report["product_action_ids"] == inputs[runs["C14-eager-primary"]["case"]]["actions"]
        wasm[label] = {k: report[k] for k in ["status", "summary", "solve_complete_ms",
            "total_ms", "pattern", "memory_after_cleanup"]}
        wasm[label].update(report_sha256=digest(path), setup_seconds=report["timings_ns"]["solve_setup"] / 1e9)
    assert wasm["C15-wasm-primary"]["status"] == "passed"
    assert wasm["C15-wasm-primary"]["summary"]["lower_bound"] == after["lower_bound"]
    assert wasm["C15-wasm-primary"]["summary"]["upper_bound"] == after["upper_bound"]
    return {"schema": "empty_start_continuation_resumed_v1", "inputs": inputs,
            "preparation": preparation(directory),
            "wasm": wasm,
            "root_gap_reduction_fraction": 1 - after["absolute_optimality_gap"] /
                before["absolute_optimality_gap"], "runs": runs,
            "limitations": [
                "One observation per variant; finalization costs included; no speedup claim.",
                "Lower and preparation held active; policy improvement is continuation service, not lower consumption.",
                "C2 alone is unqualified: it returns a slightly worse upper than B6.",
                "C9 dependency-readiness cadence was rejected and removed after 324 attempts and a worse upper.",
                "C1 intermediate binary hash unavailable; C2/C3 patches and executable hashes retained.",
                "Primary native exact closure remains open; the four-goal strict mapping refusal is preserved."]}


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("directory", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.write_bytes((json.dumps(summarize(args.directory), indent=2) + "\n").encode())
