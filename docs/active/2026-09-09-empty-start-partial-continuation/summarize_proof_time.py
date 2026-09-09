"""Preserve useful-proof-time observations without backdating final evidence."""
import hashlib
import json
from pathlib import Path
import zipfile

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent
OUT = ROOT / "out/2026-09-09-empty-start-partial-continuation"


def read(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write(path, value):
    path.write_bytes((json.dumps(value, indent=2) + "\n").encode())


def native(label):
    path = OUT / (label + ".json")
    case = read(path)["cases"][0]
    evaluation = case["exact_strategy_evaluation"]
    assert not case["errors"] and case["expectation_met"], label
    assert evaluation["completed"] and evaluation["status"] == "matched", label
    assert evaluation["cost_complete"] and evaluation["zero_off_policy_mass"], label
    assert evaluation["success_probability"] == 1, label
    assert case["cap_checks"]["all_passed"], label
    process = read(OUT / (label + ".process.json"))
    assert process["exit_code"] == 0 and not any(process[k] for k in
        ("survivor", "canceled", "timed_out")), label
    refinement = case["solver_telemetry"]["policy_refinement"]
    observations = {}
    for seconds in (60, 120, 180, 240):
        eligible = [s for s in case["bound_trace"]["samples"]
                    if s["elapsed_ms"] <= seconds * 1000]
        # Keep the actual sample time. Never impute final evaluation to a
        # candidate-kind event or a time before its bound became observable.
        observations[str(seconds)] = eligible[-1] if eligible else None
    return case, {
        "report_sha256": digest(path), "input": case["input"],
        "actions": case["product_action_ids"], "summary": case["solve_summary"],
        "phase_wall_ms": case["phase_wall_ms"], "memory": case["memory"],
        "process": process, "observations_at_seconds": observations,
        "refinement": {k: refinement.get(k) for k in (
            "proof_handoff", "strict_lift", "exact_states", "exact_kernels",
            "exact_transitions", "new_candidate_continuation", "completion_heuristic",
            "quotient_proof", "selected_policy_candidate", "finalization_stages_ns",
            "structural_failures", "refusal_causes")},
        "alternative_work": {k: v for k, v in refinement["certification_work"].items()
                             if not isinstance(v, (dict, list))},
        "evaluation": {k: v for k, v in evaluation.items() if k != "result"},
        "graph_sha256": digest(ROOT / case["compiled_graph"]["strategy_output_path"]),
    }


labels = (
    "F1-four-long-control", "F2-five-long-control", "F3-four-handoff",
    "F3b-four-handoff", "F3c-four-handoff", "F4-four-continuation",
    "F4b-four-attribution", "F5-four-demand", "F6-four-parent",
    "F7-four-heuristic", "F8-five-handoff", "F9-four-short",
    "F10-five-short", "F11-exact-anchor",
)
runs = {label: native(label)[1] for label in labels}
for treatment, baseline in (("F7-four-heuristic", "F1-four-long-control"),
                            ("F8-five-handoff", "F2-five-long-control")):
    left, right = runs[treatment], runs[baseline]
    treatment_input = dict(left["input"])
    treatment_input.pop("proof_handoff_diagnostic")
    assert treatment_input == right["input"], treatment
    assert left["actions"] == right["actions"], treatment

baselines = {}
for current, old in (("F9-four-short", "E3-four-finish"),
                     ("F10-five-short", "E4-primary-finish"),
                     ("F11-exact-anchor", "E6-exact-anchor")):
    prior = native(old)[1]
    final = runs[current]
    assert final["input"] == prior["input"] and final["actions"] == prior["actions"]
    assert final["graph_sha256"] == prior["graph_sha256"], current
    for endpoint in ("lower_bound", "upper_bound"):
        assert final["summary"][endpoint] == prior["summary"][endpoint], current
    assert final["summary"]["cap_hit_mask"] == 0, current
    baselines[old] = {k: prior[k] for k in ("summary", "graph_sha256", "report_sha256")}
assert runs["F11-exact-anchor"]["summary"]["policy_status"] == "exact"

qualification = {}
for label in ("F8a-policy-contracts", "F8b-proof-handoff", "F8c-bounded-finish",
              "F12-wasm-build"):
    process = read(OUT / (label + ".process.json"))
    assert process["exit_code"] == 0 and not any(process[k] for k in
        ("survivor", "canceled", "timed_out")), label
    qualification[label] = process

wasm = {}
for label, native_label in (("F13-wasm-four", "F9-four-short"),
                            ("F14-wasm-five", "F10-five-short")):
    receipt = read(OUT / (label + ".json"))
    assert receipt["status"] == "passed"
    assert receipt["memory_after_cleanup"]["live_handles"] == 0
    assert receipt["strategy_sha256"] == runs[native_label]["graph_sha256"]
    assert receipt["solve_complete_ms"] < receipt["input"]["watchdog_seconds"] * 1000
    for key in ("session", "start", "goal", "economy", "caps",
                "product_action_envelope", "watchdog_seconds",
                "requested_bounded_finish_seconds"):
        assert receipt["input"][key] == runs[native_label]["input"][key], (label, key)
    assert receipt["product_action_ids"] == runs[native_label]["actions"]
    for endpoint in ("lower_bound", "upper_bound"):
        assert abs(receipt["summary"][endpoint] -
                   runs[native_label]["summary"][endpoint]) < 1e-7
    process = read(OUT / (label + ".process.json"))
    assert process["exit_code"] == 0 and not any(process[k] for k in
        ("survivor", "canceled", "timed_out"))
    wasm[label] = {k: receipt[k] for k in (
        "status", "summary", "solve_complete_ms", "strategy_sha256",
        "strict_lift", "memory_after_cleanup")}
    wasm[label]["process"] = process

four = runs["F7-four-heuristic"]["summary"]
control = runs["F1-four-long-control"]["summary"]
five = runs["F8-five-handoff"]["summary"]
five_control = runs["F2-five-long-control"]["summary"]
historical_gap = 5218.040949685988 - 198.8334996747695
gap = four["upper_bound"] - four["lower_bound"]
five_nonregression = (five["lower_bound"] >= five_control["lower_bound"] and
                      five["upper_bound"] <= five_control["upper_bound"])
material = (gap <= .8 * (control["upper_bound"] - control["lower_bound"]) and
            gap <= .8 * historical_gap and
            four["upper_bound"] <= control["upper_bound"] and five_nonregression)
report = {
    "schema": "useful_proof_time_v1", "base": "317438392c88c0151f41e48b4b0323b06b1d0037",
    "runs": runs, "short_baselines": baselines, "wasm": wasm,
    "qualification_processes": qualification,
    "gate": {"four_gap": gap, "historical_gap_ceiling": .8 * historical_gap,
             "five_bound_nonregression": five_nonregression,
             "five_control_cap_hit_mask": five_control["cap_hit_mask"],
             "five_treatment_cap_hit_mask": five["cap_hit_mask"],
             "material_improvement": material,
             "new_exact_closure": four["policy_status"] == "exact" or five["policy_status"] == "exact"},
    "limitations": [
        "Wall-triggered handoffs can capture different stable candidate snapshots; compare their identities before causal attribution.",
        "New rows and strengthened local floors do not establish a stronger root bound or a closed action envelope.",
        "The five-goal proof treatment preserves verified bounds but hits the unchanged memory cap; this is not an uncapped nonregression result.",
        "No case-level Simulator run, broad acceptance suite or rendered UI review was performed.",
        "Imported full chat responses were read; their generated downloadable packages were not retrieved.",
    ],
}
write(HERE / "proof-time-comparison.json", report)
files = [p for p in OUT.glob("F*")
         if p.is_file() and not p.name.startswith("F-final-")]
files += [p for folder in OUT.glob("F*-strategy.json") for p in folder.glob("*.strategy.json")]
files += [OUT / "F0-baseline/hashes.json", OUT / "run.py"]
with zipfile.ZipFile(HERE / "proof-time-evidence.zip", "w", zipfile.ZIP_DEFLATED,
                     compresslevel=9) as archive:
    for path in sorted(files):
        archive.write(path, path.relative_to(OUT).as_posix())
identity = read(OUT / "F7-build-identity.json")
for name, expected in identity.items():
    assert digest(ROOT / name) == expected, name
extra = ["bindings/wasm/dist/poecraft_engine.wasm", "bindings/wasm/dist/poecraft_engine.mjs",
         str(Path(__file__).relative_to(ROOT)).replace("\\", "/")]
write(HERE / "proof-time-provenance.json", {
    "base": report["base"], "measured_final_native_identity": identity,
    "additional_sha256": {p: digest(ROOT / p) for p in extra},
    "comparison_sha256": digest(HERE / "proof-time-comparison.json"),
    "archive_sha256": digest(HERE / "proof-time-evidence.zip"),
    "files": {p.relative_to(OUT).as_posix(): digest(p) for p in sorted(files)},
})
print(json.dumps(report["gate"]))
