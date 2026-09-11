"""Project the selected native/WASM receipts; never run or alter a solve.

Use solver_reports for the ordinary comparison first. This programme projection
also records legacy metadata exclusions, source witnesses and returned graphs.
"""
from __future__ import annotations

import hashlib
import json
import shutil
from pathlib import Path


ROOT = Path(__file__).resolve().parents[4]
RECORD = Path(__file__).resolve().parents[1]
OUT = ROOT / "out/goal-reaching-row-delivery"
BASE = ROOT / "out/2026-09-09-cross-base-capability-recovery/A3-ordinary-baseline"
PRIMARY = {"cb06-cross-base-product8-long240", "cb08-cross-base-product8-long240"}


def read(path: Path):
    return json.loads(path.read_text(encoding="utf-8"))


def sha(path: Path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def reference(path: Path):
    return {"path": path.relative_to(ROOT).as_posix(), "sha256": sha(path)}


def pick(value: dict, *keys: str):
    return {key: value.get(key) for key in keys}


def input_hash(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(",", ":"))
                          .encode("utf-8")).hexdigest()


def qualified(case):
    summary = case["solve_summary"]
    evaluation = case["independent_evaluation"]
    return bool(summary.get("policy_available") and evaluation.get("completed")
                and evaluation.get("converged") and evaluation.get("cost_complete")
                and evaluation.get("zero_off_policy_mass")
                and evaluation.get("cost_reconciled"))


def source_opportunities():
    projected = []
    for name in ["n2-epoch-resume-ring.json", "n2-epoch-resume-amulet.json"]:
        path = RECORD / "experiments" / name
        for case_id, case in read(path)["cases"].items():
            observation = case["goal_delivery"]
            root = next(sample for sample in observation["samples"]
                        if sample["source"]["id"] == 0)
            operators = {op["id"]: op for op in root["operators"]}
            rows = []
            for row in observation["root_rows"]:
                action = row["action"]
                if not action.startswith(("harvest_reforge:", "essence:", "fossil:")):
                    continue
                rows.append({**pick(row, "row", "action", "admitted", "direct_missing", "terminal_mass"),
                             "source_operator": operators[action],
                             "row_status": "completed incremental alternative; not Bellman admitted"})
            projected.append({"id": case_id, "evidence": reference(path),
                              "graph_identity": observation["graph_identity"],
                              "source": root["source"], "rows": rows,
                              "outcome": case["solve_summary"],
                              "interpretation": "diagnostic epoch-reentry arm; completed source work alone returned no policy. Counts are direct missing continuations in this view, not whole-controller cost or evidence of uncompetitiveness."})
    return projected


def wasm_primary():
    cases = []
    native = read(OUT / "n5-qualified-core/cases/cb06-cross-base-product8-long240.json")["cases"][0]
    for mode in ["normal", "abandon", "finish"]:
        path = OUT / f"n5-wasm-ring-{mode}-receipt.json"
        raw = read(path)
        assert raw["status"] == "passed" and raw["memory_after_cleanup"]["live_handles"] == 0
        assert raw["product_action_ids"] == native["product_action_ids"]
        item = pick(raw, "fixture", "mode", "status", "error", "initial_publication",
                    "finish_requested_ms", "abandon_ms", "solve_complete_ms", "summary",
                    "strategy_sha256", "total_ms", "memory_after_cleanup")
        item["receipt"] = reference(path)
        item["native_product_action_ids_match"] = True
        if "exact_evaluation" in raw:
            item["exact_evaluation"] = pick(raw["exact_evaluation"], "converged", "residual_mass",
                                             "terminals", "accounting", "memory")
        cases.append(item)
    return {"processes": read(OUT / "n5-wasm-ring-processes.json"), "cases": cases,
            "scope": "normal uses unchanged CB06 fixture; abandon and requested finish interrupt the first joint-candidate compilation before a verified upper. Interruption runs are control tests, not baseline cost comparisons."}


def project(run: Path, expected: int):
    ledger = read(run / "ledger.json")
    entries = ledger["cases"]
    assert len(entries) == expected, (run, len(entries), expected)
    cases = []
    inputs = {}
    for case_id, entry in sorted(entries.items()):
        assert entry["status"] == "completed", (case_id, entry)
        path = run / "cases" / f"{case_id}.json"
        raw = read(path)["cases"][0]
        assert raw["id"] == case_id
        inputs[case_id] = raw["input"]
        telemetry = raw.get("solver_telemetry", {})
        envelope = telemetry.get("incremental_action_envelope", {})
        typed = envelope.get("typed_ledger", {})
        evaluation = {k: v for k, v in raw.get("exact_strategy_evaluation", {}).items()
                      if k != "result"}
        case = {
            "id": case_id,
            "base": raw["input"].get("session", {}).get("base_name"),
            "goals": len(raw["input"].get("goal", {}).get("slots", [])),
            "report": reference(path),
            "input_sha256": input_hash(raw["input"]),
            "actual_status": raw.get("actual_status"),
            "expectation_met": raw.get("expectation_met"),
            "errors": raw.get("errors"),
            "solve_summary": raw.get("solve_summary", {}),
            "independent_evaluation": evaluation,
            "phase_wall_ms": raw.get("phase_wall_ms"),
            "worker": pick(entry, "exit_code", "wall_ms", "timed_out", "canceled",
                           "survivor", "survivor_check", "watchdog_seconds",
                           "reserved_memory_bytes", "worker_headroom_bytes",
                           "resolved_command"),
            "memory": raw.get("memory"),
            "states": telemetry.get("states"),
            "work": telemetry.get("work"),
            "policy_result": telemetry.get("policy_result"),
            "portfolio": telemetry.get("incumbent_portfolio"),
            "compiled_graph": raw.get("compiled_graph"),
            "bounded_best_policy_contract": raw.get("bounded_best_policy_contract"),
            "cap_checks": raw.get("cap_checks"),
            "open_work": pick(envelope, "closed", "actions", "remaining_action_envelope",
                              "joint_anytime_policy", "missing_frontier", "q_refinement"),
            "action_ledger": pick(typed, "entries", "states", "stop_owners", "sample_counts"),
        }
        if case_id in PRIMARY:
            case["whole_run_reforge_lifecycles"] = {
                "scope": "whole-run per-action booleans; not per-source coverage",
                "actions": [action for action in typed.get("action_lifecycles", [])
                            if action.get("action_id", "").startswith(
                                ("harvest_reforge:", "fossil:", "essence:"))],
            }
            assert qualified(case), case_id
            assert not case["errors"] and case["cap_checks"]["all_passed"]
            assert case["bounded_best_policy_contract"]["passed"]
            strategy = Path(raw["compiled_graph"]["strategy_output_path"])
            target = RECORD / "experiments/strategies" / strategy.name
            target.parent.mkdir(exist_ok=True)
            shutil.copyfile(strategy, target)
            case["returned_strategy"] = reference(target)
            strategy_json = read(target)
            case["strategy_operation_nodes"] = [node for node in strategy_json["nodes"]
                                                if node.get("kind") == "operation"]
        cases.append(case)
    return {
        "ledger": reference(run / "ledger.json"),
        "executable": ledger["executable"],
        "source": ledger["source"],
        "configuration": ledger["configuration"],
        "corpus": ledger["corpus"],
        "artifact": ledger["artifact"],
        "all_processes_completed": all(e["status"] == "completed" for e in entries.values()),
        "survivors": sum(bool(e.get("survivor")) for e in entries.values()),
        "cases": cases,
    }, inputs


def main():
    core, core_inputs = project(OUT / "n5-qualified-core", 12)
    heldout, _ = project(OUT / "n5-qualified-heldout", 2)
    assert core["executable"]["sha256"] == heldout["executable"]["sha256"]
    baseline_record = ROOT / "docs/active/2026-09-09-cross-base-capability-recovery/evidence-summary.json"
    baseline = read(baseline_record)
    old_cases = {c["id"]: c for c in baseline["runs"]["A3-ordinary-baseline"]["cases"]}
    short = {c["selection_id"].lower(): c for c in baseline["short_controls"]["controls"]}
    comparisons = []
    for case in core["cases"]:
        case_id = case["id"]
        semantic_fields = ["session", "start", "goal", "product_action_envelope",
                           "economy", "caps", "verification"]
        if case_id in old_cases:
            old = old_cases[case_id]
            raw = read(BASE / "cases" / f"{case_id}.json")["cases"][0]
            same_input = raw["input"] == core_inputs[case_id]
            assert same_input, case_id
            old_summary = old["native_summary"]
            old_evaluation = old["independent_evaluation_at_return"]
            basis = "entire native case input equal; legacy runner metadata comparison excluded"
        else:
            old = short[case_id[:4]]
            assert all(old["input"].get(key) == core_inputs[case_id].get(key)
                       for key in semantic_fields), case_id
            old_summary = old["solve_summary"]
            old_evaluation = old["exact_strategy_evaluation"]
            same_input = False
            basis = "historical 60/90-second policy control versus current 240/300; no matched timing claim"
        was_qualified = bool(old_summary.get("policy_available")
                             and old_evaluation.get("cost_complete")
                             and old_evaluation.get("zero_off_policy_mass"))
        retained = None
        if was_qualified:
            retained = (qualified(case) and case["solve_summary"]["upper_bound"]
                        <= old_summary["upper_bound"] * (1 + 1e-9) + 1e-7)
            if old_summary.get("policy_status") == "exact":
                retained = retained and case["solve_summary"].get("policy_status") == "exact"
        comparisons.append({
            "id": case_id, "basis": basis, "entire_native_input_equal": same_input,
            "semantic_request_fields_equal": semantic_fields,
            "baseline_summary": old_summary,
            "baseline_independent_evaluation": old_evaluation,
            "baseline_phase_wall_ms": old.get("phase_wall_ms"),
            "baseline_qualified_policy": was_qualified,
            "final_qualified_policy": qualified(case),
            "prior_policy_cost_and_exact_status_preserved": retained,
        })
    wasm_path = OUT / "n5-wasm-four-goal-receipt.json"
    wasm = read(wasm_path)
    evaluation = wasm["exact_evaluation"]
    wasm_projection = pick(wasm, "fixture", "status", "finish_requested_ms",
                           "solve_complete_ms", "summary", "strategy_sha256", "total_ms",
                           "memory_after_cleanup", "publication")
    wasm_projection["exact_evaluation"] = pick(evaluation, "converged", "residual_mass",
                                               "terminals", "accounting", "memory")
    result = {
        "programme": "Goal-Reaching Row Delivery and Reforge Coverage Recovery v1",
        "reviewed_main": "a5ef8a5b7d6263fdc60f94957b0b76f7978f8f72",
        "baseline": reference(baseline_record),
        "core": core, "heldout": heldout, "comparisons": comparisons,
        "primary_success": all(qualified(c) for c in core["cases"] if c["id"] in PRIMARY)
            and all(c["prior_policy_cost_and_exact_status_preserved"] is not False
                    for c in comparisons),
        "capacity": {"baseline_bytes": 1073741824, "four_gib_attempted": False,
                     "reason": "necessary selected controllers fit 1 GiB; later optional refusal is not a necessary controller allocation"},
        "reporter": {"comparison": reference(OUT / "n5-qualified-comparison.json"),
                     "summary": reference(OUT / "n5-qualified-summary.json"),
                     "legacy_limit": "pre-M0 baseline lacks typed retention/watchdog configuration fields; reporter excludes pairs. Historical receipts unchanged; entire native inputs checked above. No speedup claim."},
        "source_evidence": [reference(RECORD / "experiments" / name) for name in [
            "n1-terminal-observation.json", "n1-native-witness.json", "n2-epoch-resume-ring.json",
            "n2-epoch-resume-amulet.json", "n3-verified-before-improvement-ring.json",
            "n3-verified-before-improvement-amulet.json", "n5-incumbent-epoch-regression.json",
            "n5-report-write-qualification.json"]],
        "source_opportunities": source_opportunities(),
        "wasm": {"artifact": reference(ROOT / "bindings/wasm/dist/poecraft_engine.wasm"),
                 "focused": read(OUT / "n5-wasm-final-focused.json"),
                 "retention_process": read(OUT / "n5-wasm-final-retention.json"),
                 "retention_receipt": reference(wasm_path),
                 "retention": wasm_projection,
                 "primary": wasm_primary(),
                 "cancellation_control": read(OUT / "n5-wasm-cancellation-baseline.json"),
                 "qualification_limit": "earlier callback-finalization abort assertion failed twice and passed once on current WASM; single baseline control passed. Final probe observes native Refining before abandon and tests requested finish directly; worker pre-finalization AbortSignal remains tested. Baseline flakiness is not established."},
        "checks": {"native_build": "scripts/build.ps1 passed (static, shared, tests, benchmark)",
                   "native_focused": {"joint_continuation": 181, "selected_fallback": 370,
                                      "bounded_finish": 14, "failures": 0},
                   "native_test_flags": ["--solver-joint-policy-continuation-only",
                                         "--solver-selected-fallback-only", "--solver-bounded-finish-only"],
                   "wasm_build": "scripts/build-wasm.ps1 passed; three pre-existing warnings",
                   "web_typecheck": "npx tsc --noEmit passed",
                   "web_targeted": {"engine_client_transfer": 1, "solve_workspace": 7,
                                    "solver_result_presentation": 8, "failures": 0},
                   "knowledge_lint": {"receipt": reference(OUT / "n5-knowledge-lint.json"),
                                      "claims": 25, "errors": [], "preexisting_open_warnings": 18},
                   "document_links": "eight changed owner/record documents checked; no missing local targets",
                   "research_imports": "all six ZIP members byte-exact",
                   "diff_check": {"authored_files_passed": True, "crlf_recognized": True,
                                  "excluded": "archived patch context and byte-exact supplied Markdown",
                                  "unfiltered_result": "reports preserved CRLF and archive whitespace; evidence was not rewritten to erase those notices"},
                   "not_run": ["Simulator", "full acceptance suite", "full npm test", "rendered UI review", "4 GiB diagnostic"]},
    }
    (RECORD / "results.json").write_text(json.dumps(result, indent=2, ensure_ascii=False) + "\n",
                                        encoding="utf-8", newline="\n")
    print(json.dumps({"primary_success": result["primary_success"],
                      "core_cases": len(core["cases"]), "heldout_cases": len(heldout["cases"]),
                      "prior_policy_regressions": [c["id"] for c in comparisons
                                                   if c["prior_policy_cost_and_exact_status_preserved"] is False]}, indent=2))


if __name__ == "__main__":
    main()
