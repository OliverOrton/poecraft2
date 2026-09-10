"""Project the saved proper-policy experiment; never launches a solver."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
RECORD = Path(__file__).resolve().parents[1]
OUT = ROOT / "out/proper-policy-recovery"
IDS = ("cb06-cross-base-product8-long240", "cb08-cross-base-product8-long240")
RUNS = ("m1-first-view", "m1-joint-dependency", "restored-pair")


def read(path: Path):
    return json.loads(path.read_text(encoding="utf-8"))


def digest(path: Path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    results = {
        "schema": "proper_policy_recovery_observations_v1",
        "baseline_revision": "44fdca8ebf88b9b87b2bd832360cb62128e3968a",
        "authority": "bounded native observations; no new executable policy or exact closure",
        "runs": {},
        "research_observations": {},
    }
    for label in RUNS:
        directory = OUT / label
        ledger = read(directory / "ledger.json")
        run = {
            "ledger": str((directory / "ledger.json").relative_to(ROOT)).replace("\\", "/"),
            "ledger_sha256": digest(directory / "ledger.json"),
            "executable_sha256": ledger["executable"]["sha256"],
            "all_completed": ledger["all_completed"],
            "survivors": ledger["survivors"],
            "cases": {},
        }
        for ident in IDS:
            path = directory / "cases" / (ident + ".json")
            case = read(path)["cases"][0]
            telemetry = case["solver_telemetry"]
            envelope = telemetry["incremental_action_envelope"]
            private = envelope["carrier_ladder_exact_boundary"] or {}
            views = private.get("completed_policy_support", [])
            if isinstance(views, dict):
                views = [views]
            native = ledger["cases"][ident]
            summary = case["solve_summary"]
            assert not summary["policy_available"]
            assert all(v["status"] == "complete" and v["goals"] == 0
                       and not v["root_winning"] for v in views)
            item = {
                "report_sha256": digest(path),
                "actual_status": case["actual_status"],
                "solve_summary": summary,
                "native_total_ms": case["phase_wall_ms"]["total"],
                "process_wall_ms": native["wall_ms"],
                "first_verified_policy_ms": None,
                "cap_hits": telemetry["optimization"]["cap_hits"],
                "native_peak_owned_bytes": case["memory"]["native_peak_owned_bytes"],
                "native_live_owned_bytes": case["memory"]["native_live_owned_bytes"],
                "observed_choice_groups": telemetry["work"]["choice_groups"],
                "observed_choice_successors": telemetry["work"]["choice_successor_entries"],
                "failure": telemetry["optimization"]["policy_evaluation_failure"],
                "action_envelope": envelope["actions"],
                "joint_attempts": private.get("joint_anytime_attempt_lineage"),
                "completed_views": views,
                "resolved_command": native["resolved_command"],
            }
            run["cases"][ident] = item
            if label != "restored-pair":
                last = views[-1]
                results["research_observations"][ident + ":" + label] = {
                    "status": case["actual_status"],
                    "expanded": last["expanded"],
                    "complete_rows": last["completed_rows"],
                    "true_goals": last["goals"],
                    "proper_root_in_view": last["root_winning"],
                    "verified_upper": None,
                    "cap": item["cap_hits"],
                }
            else:
                baseline = read(ROOT / "out/2026-09-09-cross-base-capability-recovery"
                                / "B5-restored-pair/cases" / (ident + ".json"))["cases"][0]
                assert summary == baseline["solve_summary"]
                assert item["failure"] == baseline["solver_telemetry"]["optimization"]["policy_evaluation_failure"]
                item["matches_original_B5_summary_and_failure"] = True
        results["runs"][label] = run
    manifest = read(RECORD / "research-inputs/manifest.json")
    results["imported_hashes_match"] = all(
        digest(RECORD / "research-inputs" / name) == expected
        for name, expected in manifest["files"].items())
    assert results["imported_hashes_match"]
    results["reference_results_match"] = (
        read(OUT / "reference-results.json") ==
        read(RECORD / "research-inputs/proper_seed_results.json"))
    assert results["reference_results_match"]
    results["patches_sha256"] = {
        p.name: digest(p) for p in sorted((RECORD / "experiments").glob("*.patch"))}
    path = RECORD / "results.json"
    path.write_bytes((json.dumps(results, indent=2) + "\n").encode())
    print(f"Saved {len(RUNS)} runs / {len(IDS)} cases; restored summaries match B5.")


if __name__ == "__main__":
    main()
