"""Check the bounded-publication measurements and preserve their provenance."""
import hashlib
import json
from pathlib import Path
import zipfile


ROOT = Path(__file__).resolve().parents[3]
DEST = Path(__file__).resolve().parent
OUT = ROOT / "out/2026-09-09-empty-start-partial-continuation"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def write(path, value):
    path.write_bytes((json.dumps(value, indent=2) + "\n").encode())


def native(label):
    path = OUT / (label + ".json")
    case = read(path)["cases"][0]
    evaluation = case["exact_strategy_evaluation"]
    assert not case["errors"] and case["expectation_met"]
    assert evaluation["completed"] and evaluation["status"] == "matched"
    assert evaluation["cost_complete"] and evaluation["zero_off_policy_mass"]
    assert evaluation["success_probability"] == 1
    graph = Path(case["compiled_graph"]["strategy_output_path"])
    telemetry = case["solver_telemetry"]
    refinement = telemetry["policy_refinement"]
    return case, {
        "case": case["id"], "report_sha256": digest(path),
        "summary": case["solve_summary"], "status": case["actual_status"],
        "phase_wall_ms": case["phase_wall_ms"], "memory": case["memory"],
        "setup_ns": telemetry["timings_ns"]["solve_setup"],
        "first_verified_upper_ms": case["bound_trace"]["time_to_first_incumbent_ms"],
        "strict_lift": refinement["strict_lift"],
        "strict_states": refinement["exact_states"],
        "strict_kernels": refinement["exact_kernels"],
        "publication": refinement["publication"],
        "finish_request": {key: value for key, value in telemetry["optimization"].items()
                           if key.startswith("requested_bounded_finish")},
        "evaluation": {key: value for key, value in evaluation.items() if key != "result"},
        "returned_graph_sha256": digest(graph),
    }


runs, inputs, baselines = {}, {}, {}
for label, baseline in {
    "E3-four-finish": "D3-four-control",
    "E4-primary-finish": "D7-restored-primary",
    "E5-partial-finish": "C16-final-partial",
    "E6-exact-anchor": "C17-final-anchor",
}.items():
    case, run = native(label)
    before, previous = native(baseline)
    identity = {"input": case["input"], "actions": case["product_action_ids"]}
    assert identity == {"input": before["input"], "actions": before["product_action_ids"]}
    for endpoint in ("lower_bound", "upper_bound"):
        assert run["summary"][endpoint] == previous["summary"][endpoint]
    assert run["returned_graph_sha256"] == previous["returned_graph_sha256"]
    assert run["summary"]["cap_hit_mask"] == 0
    run["matched_baseline"] = baseline
    runs[label], inputs[case["id"]], baselines[baseline] = run, identity, previous

four = runs["E3-four-finish"]
assert four["strict_lift"]["status"] == "requested_bounded_finish"
assert not four["strict_lift"]["global_lower_bound_closed"]
assert four["summary"]["termination"] == "requested_bounded_finish"
assert four["finish_request"]["requested_bounded_finish"]
assert baselines["D3-four-control"]["finish_request"]["requested_bounded_finish"]
assert runs["E6-exact-anchor"]["summary"]["policy_status"] == "exact"

wasm = {}
for label, native_label in {"E9-wasm-primary": "E4-primary-finish",
                            "E10-wasm-four": "E3-four-finish"}.items():
    receipt = read(OUT / (label + ".json"))
    assert receipt["status"] == "passed"
    assert receipt["memory_after_cleanup"]["live_handles"] == 0
    assert receipt["solve_complete_ms"] < receipt["input"]["watchdog_seconds"] * 1000
    identity = inputs[runs[native_label]["case"]]
    for key in ("session", "start", "goal", "economy", "caps",
                "product_action_envelope", "watchdog_seconds", "requested_bounded_finish_seconds"):
        assert receipt["input"][key] == identity["input"][key], key
    assert receipt["product_action_ids"] == identity["actions"]
    assert receipt["strategy_sha256"] == runs[native_label]["returned_graph_sha256"]
    for endpoint in ("lower_bound", "upper_bound"):
        assert abs(receipt["summary"][endpoint] - runs[native_label]["summary"][endpoint]) < 1e-7
    wasm[label] = {key: value for key, value in receipt.items()
                   if key not in ("exact_evaluation", "input", "product_action_ids",
                                  "incremental_action_envelope")}
    evaluation = receipt["exact_evaluation"]
    wasm[label]["evaluation"] = {"converged": evaluation["converged"],
        "terminals": {k: v for k, v in evaluation["terminals"].items() if k != "by_node"},
        "totals": evaluation["accounting"]["totals"]["per_invocation"]}
assert wasm["E10-wasm-four"]["strict_lift"]["status"] == "requested_bounded_finish"

processes = {path.name: read(path) for path in sorted(OUT.glob("E*.process.json"))}
for process in processes.values():
    assert not process["survivor"] and not process["canceled"] and not process["timed_out"]
report = {
    "schema": "empty_start_bounded_publication_v1",
    "base": "0ddd7b191fdb1ab4e2e50bdbbf33e195f3910d8b",
    "inputs": inputs, "baselines": baselines, "runs": runs, "wasm": wasm,
    "processes": processes,
    "disposition": "Honor a latched bounded finish during optional strict work and preserve verified publication.",
    "limitations": [
        "No new root-bound improvement or five/four-goal exact closure. All four native returned strategies match their prior verified artifacts.",
        "E3 returns before strict rows start; it does not qualify the removed D4/D5 new-continuation patches or resolve parent 4741.",
        "The native timing comparison is one observation per arm, not a timing distribution.",
        "E1 did not exercise strict work and failed fixture setup. E1b is the valid old-code request-latching negative; E2 passes.",
        "The latest research response is available, but its generated implementation-plan package remains inaccessible.",
    ],
}
write(DEST / "finish-comparison.json", report)

files = [path for path in OUT.glob("E*") if path.is_file()]
files += [path for folder in OUT.glob("E*-strategy.json") if folder.is_dir()
          for path in folder.glob("*.strategy.json")]
files += [OUT / "run.py", OUT / "E0-baseline/hashes.json"]
with zipfile.ZipFile(DEST / "finish-evidence.zip", "w", zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
    for path in sorted(files):
        archive.write(path, path.relative_to(OUT).as_posix())
source_paths = [
    "engine/include/poecraft/solver.h", "engine/src/solver_solve.cpp",
    "engine/src/solver_solve_contracts.hpp", "engine/src/solver_solve_finish.cpp",
    "engine/src/solver_policy_refinement.hpp", "engine/src/solver_policy_assertion.cpp",
    "engine/tests/test_solver_solve.cpp", "engine/tests/test_main.cpp", "engine/tests/tests.hpp",
    "apps/web/test/empty-start-retention-wasm.test.ts",
    "docs/active/2026-09-09-empty-start-partial-continuation/summarize_finish.py",
    "build/engine/poecraft_solver_benchmark.exe", "build/engine/poecraft_engine_tests.exe",
    "build/engine/poecraft_engine.dll", "bindings/wasm/dist/poecraft_engine.wasm",
    "bindings/wasm/dist/poecraft_engine.mjs",
]
write(DEST / "finish-provenance.json", {
    "base": report["base"], "source_and_artifact_sha256": {p: digest(ROOT / p) for p in source_paths},
    "archive_sha256": digest(DEST / "finish-evidence.zip"),
    "comparison_sha256": digest(DEST / "finish-comparison.json"),
    "files": {p.relative_to(OUT).as_posix(): digest(p) for p in sorted(files)},
    "prior_provenance_sha256": {name: digest(DEST / name) for name in
                                ("fresh-provenance.json", "continuation-provenance.json")},
})
print("Matched native inputs/artifacts, evaluated WASM endpoints, exact control and process cleanup verified")
