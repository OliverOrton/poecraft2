"""Compact completed S4 receipts. No execution or identity overrides."""
import hashlib
import json
from pathlib import Path
import sys

from poecraft_ingest.solver_reports import compare_runs, load_run

root = Path.cwd()
out = root / "out/cooperative-setup"
record = root / "docs/active/2026-09-20-cooperative-setup"
label = sys.argv[1]
native_label = sys.argv[2] if len(sys.argv) > 2 else label


def read(path):
    return json.loads(path.read_text(encoding="utf-8"))


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


wasm = []
graphs = []
reference_path = root / "out/verified-delivery/A7-manual-conquest-1.json"
reference = read(reference_path)["cases"][0]
economy = reference["input"]["economy"]
reference_prices = read(root / economy["snapshot_path"])["prices"] | economy["manual_overrides"]
for path in sorted(out.glob(f"S4-{label}-wasm-*.json")):
    r = read(path)
    trace = r["trace"]
    worker = trace["worker"]
    ui = {m["stage"]: m["ui_elapsed_ms"] for m in trace["ui_milestones"]}
    wm = {m["stage"]: m["worker_elapsed_ms"] for m in worker["milestones"]}
    timings = {
        "begin_ms": wm["native_begin_completed"] - wm["native_begin_requested"],
        "setup_step_ms": worker["max_setup_step_ms"],
        "ordinary_step_ms": worker["max_ordinary_step_ms"],
    }
    thresholds = {"begin_ms": 250, "setup_step_ms": 250, "ordinary_step_ms": 250}
    if r["control"] == "finish":
        timings["finish_ms"] = ui["ui_delivery_completed"] - ui["finish_intent"]
        timings["delivery_ms"] = ui["ui_delivery_completed"] - ui["worker_solve_requested"]
        thresholds.update(finish_ms=10000, delivery_ms=65000)
        graph = json.loads(r["graph"]) if isinstance(r["graph"], str) else r["graph"]
        graph = dict(graph)
        graph.pop("economy", None)  # Existing consumer projection; raw hashes retained.
        graphs.append(graph)
    else:
        timings["cancel_to_release_ms"] = ui["ui_delivery_completed"] - ui["cancel_intent"]
        thresholds["cancel_to_release_ms"] = 1000
    observations = trace.get("observations", [])
    first = next((p for p in observations if p.get("trace", {}).get("current", {}).get("verified_artifact_available")), None)
    current = observations[-1].get("trace", {}).get("current", {}) if observations else {}
    summary = r.get("solve_summary") or {}
    goal = dict(r["resolved"]["goal"])
    actions = goal.pop("actions")
    same_request = goal == reference["input"]["goal"] and actions == reference["product_action_ids"] and all(
        r["request"]["economy"]["snapshot"]["prices"].get(k) == v for k, v in reference_prices.items())
    assert same_request
    wasm.append({
        "report": str(path.relative_to(root)), "sha256": sha(path),
        "control": r["control"], "status": trace["status"], "usable": r["usable_strategy"], "error": r["error"],
        "timings_ms": timings,
        "gates": {k: None if k == "ordinary_step_ms" and r["control"] != "finish" else v <= thresholds[k]
                  for k, v in timings.items()},
        "max_setup_context": worker.get("max_setup_step_context"),
        "max_ordinary_context": worker.get("max_ordinary_step_context"),
        "first_verified_worker_ms": first.get("worker_observed_ms") if first else None,
        "setup_goal_cover_ns": current.get("setup_goal_cover_ns"), "setup_retention_ns": current.get("setup_retention_ns"),
        "graph_sha256": r["graph_sha256"],
        "same_goal_actions_prices_as_reference": same_request,
        "same_complete_graph_as_evaluated_reference_excluding_attached_economy":
            graph == reference["prepared_strategy"]["document"] if r["control"] == "finish" else None,
        "policy": {k: summary.get(k) for k in ["policy_status", "termination", "lower_bound", "upper_bound", "evaluated_policy_cost"]},
        "release_telemetry": r.get("release_telemetry"), "release_telemetry_read_ms": r.get("release_telemetry_read_ms"),
        "ui_milestones": ui, "worker_milestones": wm,
    })

native = []
paired = []
for arm, case_id in [("baseline", "cb01-cross-base-product8-long240"),
                     ("candidate", "cb01-cross-base-product8-long240"),
                     ("candidate", "cb12-cross-base-product8-long240")]:
    directory = out / f"S4-{native_label}-native-{arm}-{case_id}"
    if not (directory / "ledger.json").exists():
        continue
    ledger, cases = load_run(directory)
    assert ledger["all_completed"] and not ledger["survivors"] and len(cases) == 1
    if case_id.startswith("cb01"):
        paired.append(cases)
    c = cases[0]
    e = c["exact_strategy_evaluation"]
    report = Path(ledger["cases"][case_id]["report_path"])
    graph = Path(c["compiled_graph"]["strategy_output_path"])
    native.append({
        "arm": arm, "case_id": case_id, "report": str(report.relative_to(root)), "sha256": sha(report),
        "status": c["actual_status"], "expectation_met": c["expectation_met"], "errors": c["errors"],
        "graph": str(graph.relative_to(root)), "graph_sha256": sha(graph),
        "policy": {k: c["solve_summary"].get(k) for k in ["policy_status", "termination", "lower_bound", "upper_bound", "evaluated_policy_cost"]},
        "evaluation": {k: e.get(k) for k in ["completed", "converged", "cost_complete", "cost_reconciled", "success_probability", "zero_off_policy_mass"]},
        "primitive_actions": e.get("result", {}).get("expected_actions"),
        "phase_wall_ms": c["phase_wall_ms"], "memory": c["memory"],
    })
result = {
    "label": label, "native_label": native_label, "wasm": wasm, "native": native,
    "wasm_complete_graphs_equal_excluding_attached_economy": all(g == graphs[0] for g in graphs) if graphs else None,
    "native_comparison": compare_runs("0f376f4", paired[0], native_label, paired[1]) if len(paired) == 2 else None,
    "native_cb01_graph_bytes_equal": native[0]["graph_sha256"] == native[1]["graph_sha256"] if len(paired) == 2 else None,
    "historical_reference": str(reference_path.relative_to(root)), "historical_reference_sha256": sha(reference_path),
    "non_windows": "waived, not passed", "rendered_browser": "not performed", "hosted_ci": "not run; no push",
    "clock_contract": "Every duration subtracts milestones on one clock. Cancel includes probe telemetry retrieval and Calculator handle release.",
}
(record / f"S4-{label}-qualification.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
print(json.dumps({"wasm": [{"report": r["report"], "timings_ms": r["timings_ms"], "gates": r["gates"], "policy": r["policy"]} for r in wasm],
                  "native": [{k: r[k] for k in ["arm", "case_id", "policy", "primitive_actions", "errors"]} for r in native],
                  "wasm_graphs_equal": result["wasm_complete_graphs_equal_excluding_attached_economy"],
                  "native_graph_bytes_equal": result["native_cb01_graph_bytes_equal"]}))
