"""Summarize the bounded fresh-session experiment, including failed publication."""
import argparse
import hashlib
import json
from pathlib import Path
import zipfile


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def summarize(directory, destination, root):
    labels = ["D3-four-control", "D4-four-completion", "D5-four-renewal",
              "D7-restored-primary"]
    runs, inputs = {}, {}
    for label in labels:
        path = directory / (label + ".json")
        case = json.loads(path.read_text(encoding="utf-8"))["cases"][0]
        identity = {"input": case["input"], "actions": case["product_action_ids"]}
        if case["id"] in inputs:
            assert inputs[case["id"]] == identity
        inputs[case["id"]] = identity
        telemetry = case["solver_telemetry"]["policy_refinement"]
        evaluation = case["exact_strategy_evaluation"]
        if label in ["D4-four-completion", "D5-four-renewal"]:
            assert case["actual_status"] == "watchdog_expired"
            assert case["solve_summary"] is None and case["compiled_graph"] is None
        else:
            assert evaluation["completed"] and evaluation["status"] == "matched"
            assert evaluation["cost_complete"] and evaluation["zero_off_policy_mass"]
            assert evaluation["success_probability"] == 1
        graphs = sorted((directory / (label + "-strategy.json")).glob("*.strategy.json"))
        runs[label] = {
            "case": case["id"], "report_sha256": digest(path),
            "actual_status": case["actual_status"],
            "summary": case["solve_summary"], "errors": case["errors"],
            "setup_seconds": case["solver_telemetry"]["timings_ns"]["solve_setup"] / 1e9,
            "total_seconds": case["phase_wall_ms"]["total"] / 1000,
            "memory": case["memory"],
            "strict_lift": telemetry["strict_lift"],
            "work": {key: telemetry[key] for key in ["exact_states", "exact_kernels",
                "local_state_action_rows_evaluated", "certification_work", "quotient_proof",
                "finalization_stages_ns"]},
            "internally_retained_candidate_not_returned_on_failure": telemetry["selected_policy_candidate"],
            "evaluation": {k: v for k, v in evaluation.items() if k != "result"},
            "returned_graph_sha256": [digest(graph) for graph in graphs],
        }
    primary = "conquest-lamellar-allflame-clean-5-goal-product8"
    previous = json.loads((destination / "continuation-comparison.json").read_text())
    assert inputs[primary] == previous["inputs"][primary]
    before = previous["runs"]["C14-eager-primary"]["summary"]
    after = runs["D7-restored-primary"]["summary"]
    assert after["lower_bound"] == before["lower_bound"]
    assert after["upper_bound"] == before["upper_bound"]

    processes = {}
    for path in sorted(directory.glob("D*.process.json")):
        receipt = json.loads(path.read_text())
        assert not receipt["survivor"] and not receipt["canceled"]
        processes[path.name] = receipt
    report = {
        "schema": "empty_start_fresh_continuation_v1",
        "base": "267e4f349bddfbc76e76c4dc1f3809e635cb15be",
        "inputs": inputs,
        "runs": {label: runs[label] for label in labels[:3]},
        "restored_primary": runs["D7-restored-primary"],
        "processes": processes,
        "disposition": "D4 and D5 failed ordinary qualification; all engine/test changes removed",
        "limitations": [
            "No new causal root gain or native exact closure. D7 reproduces the previously qualified implementation.",
            "Both failed variants retained a candidate internally but published no final strategy.",
            "The fresh generated package was inaccessible; the latest response and earlier imported plan/review were read.",
            "No new WASM run: its source and binary are unchanged from C15. No Simulator or broad suite.",
        ],
    }
    output = destination / "fresh-comparison.json"
    output.write_bytes((json.dumps(report, indent=2) + "\n").encode("utf-8"))

    files = [p for p in directory.glob("D*") if p.is_file()]
    files += [p for folder in directory.glob("D*-strategy.json") if folder.is_dir()
              for p in folder.rglob("*") if p.is_file()]
    files.append(directory / "run.py")
    archive = destination / "fresh-evidence.zip"
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as bundle:
        for path in sorted(files):
            bundle.write(path, path.relative_to(directory).as_posix())
    binaries = [p for folder in [directory / "D0-baseline", directory / "D4-bin",
                                directory / "D5-bin"] for p in sorted(folder.glob("*.exe"))]
    binaries += [root / "build/engine/poecraft_solver_benchmark.exe",
                 root / "build/engine/poecraft_engine_tests.exe",
                 root / "bindings/wasm/dist/poecraft_engine.wasm"]
    provenance = {
        "base": report["base"], "archive_sha256": digest(archive),
        "comparison_sha256": digest(output),
        "files": {p.relative_to(directory).as_posix(): digest(p) for p in sorted(files)},
        "executables_sha256": {p.relative_to(root).as_posix(): digest(p) for p in binaries},
        "intermediate_test_binary_note": "D1/D2 test executable hashes were not preserved; source patches and actual test/process output are retained.",
        "review_sha256": digest(destination / "research-inputs/fresh-session-response.md"),
        "restoration_note": "Engine and test sources match 267e4f3. Rebuilt native binary hashes differ from the initial artifacts; D7 independently qualifies the restored primary. WASM bytes match C15.",
    }
    (destination / "fresh-provenance.json").write_bytes(
        (json.dumps(provenance, indent=2) + "\n").encode("utf-8"))
    print("Matched inputs, both publication failures, restored primary and process cleanup verified")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("directory", type=Path)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[3]
    summarize(args.directory.resolve(), args.destination.resolve(), root)
