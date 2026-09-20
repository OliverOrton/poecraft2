"""Read completed M3 evidence only; no solver launches or comparison overrides."""
import hashlib
import json
from pathlib import Path

from poecraft_ingest.solver_reports import compare_runs, load_run

root = Path.cwd()
out = root / "out/validation-and-ownership"
record = root / "docs/active/2026-09-19-validation-and-ownership"


def read(path):
    return json.loads(path.read_text(encoding="utf-8"))


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


loaded = [load_run(out / ("M3-" + arm)) for arm in ("baseline", "retained-owner")]
assert all(ledger["all_completed"] and not ledger["survivors"] for ledger, _ in loaded)
comparison = compare_runs("75504e4", loaded[0][1], "retained owner", loaded[1][1])
assert comparison["paired_cases"] == 1
native = []
graphs = []
for arm, (ledger, cases) in zip(("baseline", "retained-owner"), loaded):
    assert len(cases) == 1
    c = cases[0]
    e = c["exact_strategy_evaluation"]
    graph_path = Path(c["compiled_graph"]["strategy_output_path"])
    graphs.append(read(graph_path))
    report = Path(ledger["cases"][c["id"]]["report_path"])
    native.append({
        "arm": arm, "report": str(report.relative_to(root)), "report_sha256": sha(report),
        "graph": str(graph_path.relative_to(root)), "graph_sha256": sha(graph_path),
        "status": c["actual_status"], "expectation_met": c["expectation_met"],
        "solve_summary": c["solve_summary"], "compiled_graph": c["compiled_graph"],
        "evaluation": {k: v for k, v in e.items() if k != "result"},
        "expected_primitive_actions": e["result"]["expected_actions"],
        "phase_wall_ms": c["phase_wall_ms"], "memory": c["memory"],
        "ledger_audit": {k: v for k, v in c["solver_telemetry"]["diagnostic_cost"].items() if "ledger" in k},
        "retention": {k: v for k, v in c["solver_telemetry"]["policy_refinement"].items()
                      if k.startswith("fallback_portfolio")},
        "portfolio": c["solver_telemetry"]["incumbent_portfolio"],
        "errors": c["errors"],
    })
    assert c["expectation_met"] and not c["errors"]
    assert e["completed"] and e["converged"] and e["cost_complete"] and e["cost_reconciled"]
    assert e["success_probability"] == 1 and e["zero_off_policy_mass"]

calculator_path = out / "M3-calculator-finish.json"
calculator = read(calculator_path)
trace = calculator["trace"]
ui = {m["stage"]: m["ui_elapsed_ms"] for m in trace["ui_milestones"]}
worker = trace["worker"]
milestones = {m["stage"]: m["worker_elapsed_ms"] for m in worker["milestones"]}
finish_ms = ui["ui_delivery_completed"] - ui["finish_intent"]
delivery_ms = ui["ui_delivery_completed"] - ui["worker_solve_requested"]
begin_ms = milestones["native_begin_completed"] - milestones["native_begin_requested"]
graph = calculator["graph"]
graph = json.loads(graph) if isinstance(graph, str) else graph
projected = dict(graph)
projected.pop("economy", None)  # Existing Calculator consumer contract, only.
reference_path = root / "out/verified-delivery/A7-manual-conquest-1.json"
reference = read(reference_path)["cases"][0]
goal = dict(calculator["resolved"]["goal"])
actions = goal.pop("actions")
assert goal == reference["input"]["goal"] and actions == reference["product_action_ids"]
economy = reference["input"]["economy"]
prices = read(root / economy["snapshot_path"])["prices"] | economy["manual_overrides"]
assert all(calculator["request"]["economy"]["snapshot"]["prices"][k] == v for k, v in prices.items())
matches_reference = projected == reference["prepared_strategy"]["document"]
assert matches_reference
# The DOM graph contains display positions. Do not silently strip them from the
# raw/native equality result; record this qualified presentation-only relation.
without_positions = dict(projected)
without_positions["nodes"] = [{k: v for k, v in node.items() if k != "position"}
                              for node in projected["nodes"]]
native_without_positions = dict(graphs[1])
native_without_positions["nodes"] = [{k: v for k, v in node.items() if k != "position"}
                                     for node in graphs[1]["nodes"]]
assert trace["status"] == "completed" and calculator["usable_strategy"] and not calculator["error"]
process = read(record / "M3-calculator-process.json")["process"]
assert process["exit_code"] == 0 and not process["timed_out"] and not process["survivor"]

result = {
    "base": "75504e4ee2d5700629b23e2ed06eec63812cbd78", "build": "M3-build.json",
    "native_comparison": comparison, "native": native,
    "native_graph_bytes_equal": native[0]["graph_sha256"] == native[1]["graph_sha256"],
    "calculator": {
        "report": str(calculator_path.relative_to(root)), "sha256": sha(calculator_path),
        "environment": calculator["environment"], "visual_review": calculator["visual_review"],
        "status": trace["status"], "usable_strategy": calculator["usable_strategy"],
        "graph_sha256": calculator["graph_sha256"], "ui_milestones": ui, "worker": worker,
        "finish_intent_to_ui_ready_ms": finish_ms, "finish_gate_10s_passed": finish_ms <= 10000,
        "worker_request_to_ui_ready_ms": delivery_ms, "delivery_gate_65s_passed": delivery_ms <= 65000,
        "native_begin_ms": begin_ms, "begin_gate_250ms_passed": begin_ms <= 250,
        "step_gate_250ms_passed": worker["max_step_ms"] <= 250,
        "setup_cancel_gate": "not rerun; historical 23.081s/1s failure remains",
        "reference": str(reference_path.relative_to(root)), "reference_sha256": sha(reference_path),
        "same_goal_actions_prices_as_reference": True,
        "same_complete_graph_as_evaluated_reference_excluding_attached_economy": matches_reference,
        "same_complete_graph_as_current_native_excluding_attached_economy": projected == graphs[1],
        "native_consumer_comparison": {
            "same_except_attached_economy_and_display_positions": without_positions == native_without_positions,
            "calculator_nodes_with_position": sum("position" in n for n in projected["nodes"]),
            "native_nodes_with_position": sum("position" in n for n in graphs[1]["nodes"]),
            "authority": "docs/solver/publication.md: editable board positions are presentation, not numerical proof inputs; raw equality stays separate",
        },
        "clock_contract": "Each subtraction uses one clock; delivery starts at UI worker request, before any worker-origin timestamp",
    },
    "simulation": "No separate Simulator campaign; unchanged graph evidence reused. Existing API selector included its built-in 10000-run fixture.",
    "performance_scope": "One serial baseline/current pair, no speedup claim or counterbalancing; known other failures remain open.",
    "non_windows": "Explicitly waived by Oliver, not passed", "hosted_ci": "Not run; no push",
}
(record / "qualification.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
print(json.dumps({"native_pair": comparison["paired_cases"], "same_graph_bytes": result["native_graph_bytes_equal"],
                  "costs": [r["solve_summary"]["upper_bound"] for r in native],
                  "primitive_actions": [r["expected_primitive_actions"] for r in native],
                  "finish_ms": finish_ms, "delivery_ms": delivery_ms,
                  "begin_ms": begin_ms, "step_max_ms": worker["max_step_ms"]}))
