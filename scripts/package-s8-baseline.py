#!/usr/bin/env python3
"""Package the S8.0 before-state reports into deterministic tracked fixtures.

This script does not solve, evaluate, simulate, project, account, or trim. It
only normalizes already-captured reports and gzip-compresses their ordinary
strategy JSON with mtime=0 so the committed evidence is reproducible.
"""

from __future__ import annotations

import argparse
from collections import Counter
import copy
import gzip
import hashlib
import json
from pathlib import Path
from typing import Any


BEFORE_COMMIT = "b9e426b7e45feb4b254a56cbf3fbf43239a51261"
S7_FINAL_COMMIT = "f2678b137f9fceff6b37b6ba5c080f8f6b686f66"

CURRENT_CASES = {
    "oracle-real-one-mod": "fixtures/solver-benchmarks/v1/cases/oracle-real-one-mod.json",
    "oracle-real-two-mod": "fixtures/solver-benchmarks/v1/cases/oracle-real-two-mod.json",
    "ordinary-es-bench": "fixtures/solver-benchmarks/v1/cases/ordinary-es-bench.json",
    "advanced-es-resist-bench": "fixtures/solver-benchmarks/v1/cases/advanced-es-resist-bench.json",
    "s8-fracture-prepare": "fixtures/solver-baselines/s8.0/cases/fracture-prepare.json",
    "s8-temporary-bench-blocker": "fixtures/solver-baselines/s8.0/cases/temporary-bench-blocker.json",
}

CAPTURE_COMMAND = (
    "build/engine/poecraft_solver_benchmark.exe "
    "--artifact data/compiled/current "
    "--corpus fixtures/solver-baselines/s8.0/corpus.json "
    "--case {case_id} "
    "--output build/s8.0-baseline/{case_id}.report.json "
    "--strategy-output build/s8.0-baseline/strategies "
    "--verification-runs 10000 --verification-seed {seed} "
    "--verification-chunk-runs 1 --exact-strategy-evaluation --progress"
)

S7_ARCHIVED_COMMAND = (
    "build/engine/poecraft_solver_benchmark.exe "
    "--artifact data/compiled/current "
    "--corpus fixtures/solver-benchmarks/v1/manifest.json "
    "--output build/performance/native-solver-s7.6-final-endgame-case-"
    "endgame-fractured-es-v1.json "
    "--case endgame-fractured-es "
    "--strategy-output build/performance/strategies-s7.6-final "
    "--progress --verification-runs 10000 --verification-seed 2026071530 "
    "--verification-chunk-runs 1"
)


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: Path) -> str:
    return sha256_bytes(path.read_bytes())


def load(path: Path) -> Any:
    return json.loads(path.read_text(encoding="utf-8"))


def write_json(path: Path, value: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps(value, indent=2, sort_keys=False, ensure_ascii=False) + "\n",
        encoding="utf-8",
        newline="\n",
    )


def artifact_identity(
    root: Path, report: dict[str, Any], *, current: bool
) -> dict[str, Any]:
    manifest = report["artifact"]["manifest"]
    source = manifest["source"]
    files = manifest["files"]
    identity: dict[str, Any] = {
        "artifact_schema_version": manifest["artifact_schema_version"],
        "source_version": source["source_version"],
        "source_hash": source["source_hash"],
        "source_data_hash": source["data_hash"],
        "database_schema_version": source["database_schema_version"],
        "game_data_sha256": files["game-data.json"]["sha256"],
        "strings_sha256": files["strings.json"]["sha256"],
        "captured_manifest_payload_sha256": sha256_bytes(
            json.dumps(manifest, sort_keys=True, separators=(",", ":")).encode()
        ),
    }
    if current:
        identity.update(
            {
                "manifest_path": "data/compiled/current/manifest.json",
                "manifest_sha256": sha256_file(
                    root / "data/compiled/current/manifest.json"
                ),
                "canonical_sqlite_path": "data/sqlite/poecraft.db",
                "canonical_sqlite_sha256": sha256_file(
                    root / "data/sqlite/poecraft.db"
                ),
            }
        )
    else:
        identity.update(
            {
                "manifest_path": "archived report payload",
                "manifest_sha256": None,
                "canonical_sqlite_path": "historical S7 canonical database",
                "canonical_sqlite_sha256": None,
            }
        )
    return identity


def price_sources(source_case: str, economy: dict[str, Any]) -> list[dict[str, Any]]:
    return [
        {
            "price_key": key,
            "value": value,
            "source_document": source_case,
            "json_pointer": "/economy/prices/" + key.replace("~", "~0").replace("/", "~1"),
        }
        for key, value in sorted(economy["prices"].items())
    ]


def action_diagnostics(telemetry: dict[str, Any] | None) -> dict[str, Any] | None:
    if telemetry is None:
        return None
    control = telemetry.get("action_control", {})
    reasons = control.get("reasons", [])

    def aggregate(selected: list[str]) -> list[dict[str, Any]]:
        return [
            {"reason": reason, "observations": count}
            for reason, count in sorted(Counter(selected).items())
        ]

    return {
        "counts": telemetry.get("actions", {}),
        "reason_observations_total": len(reasons),
        "included": aggregate(
            [reason for reason in reasons if reason.startswith("included:")]
        ),
        "deferred": aggregate(
            [reason for reason in reasons if reason.startswith("deferred:")]
        ),
        "pruned": aggregate(
            [reason for reason in reasons if reason.startswith("pruned:")]
        ),
        "unpriced": aggregate(
            [reason for reason in reasons if "missing_price" in reason]
        ),
        "unsupported": aggregate(
            [reason for reason in reasons if reason.startswith("unsupported:")]
        ),
        "fossil_loadouts": control.get("fossil_loadouts", {}),
    }


def compact_telemetry(telemetry: dict[str, Any] | None) -> dict[str, Any] | None:
    if telemetry is None:
        return None
    compact = copy.deepcopy(telemetry)
    control = compact.get("action_control", {})
    reasons = control.pop("reasons", [])
    control["reason_observations_total"] = len(reasons)
    control["reason_counts"] = [
        {"reason": reason, "observations": count}
        for reason, count in sorted(Counter(reasons).items())
    ]
    return compact


def compress_strategy(
    source: Path, destination: Path
) -> dict[str, Any]:
    raw = source.read_bytes()
    parsed = json.loads(raw)
    destination.parent.mkdir(parents=True, exist_ok=True)
    with destination.open("wb") as output:
        with gzip.GzipFile(
            filename="", mode="wb", fileobj=output, compresslevel=9, mtime=0
        ) as compressed:
            compressed.write(raw)
    return {
        "format": "ordinary_strategy_json_gzip",
        "path": destination.as_posix(),
        "uncompressed_sha256": sha256_bytes(raw),
        "uncompressed_bytes": len(raw),
        "gzip_sha256": sha256_file(destination),
        "gzip_bytes": destination.stat().st_size,
        "document_version": parsed.get("version"),
        "start_node_id": parsed.get("start_node_id"),
        "nodes": len(parsed.get("nodes", [])),
        "edges": len(parsed.get("edges", [])),
        "execution_authority": "raw_strategy_only",
    }


def evaluator_capture(case: dict[str, Any]) -> dict[str, Any]:
    exact = case.get("exact_strategy_evaluation")
    if exact is None:
        return {"status": "not_requested", "result": None, "error": None}
    if exact.get("completed"):
        return {
            "status": "completed",
            "result": exact.get("result"),
            "error": None,
            "wall_ms": exact.get("wall_ms"),
        }
    error = next(
        (value for value in case.get("errors", []) if "strategy evaluation" in value),
        None,
    )
    return {
        "status": "unsupported" if error else "not_completed",
        "result": None,
        "error": error,
        "wall_ms": exact.get("wall_ms"),
        "time_limited": exact.get("time_limited", False),
    }


def normalize_report_case(
    root: Path,
    out_root: Path,
    *,
    report_path: Path,
    source_case: str,
    repository_commit: str,
    capture_origin: str,
    current_artifact: bool,
    strategy_source: Path | None = None,
    notes: list[str] | None = None,
) -> dict[str, Any]:
    report = load(report_path)
    case = report["cases"][0]
    case_id = case["id"]
    specification = load(root / source_case)
    verification_spec = specification["verification"]
    strategy = None
    if strategy_source is None and case.get("compiled_graph"):
        output = case["compiled_graph"].get("strategy_output_path")
        if output:
            strategy_source = Path(output)
    if strategy_source is not None and strategy_source.exists():
        relative = Path("fixtures/solver-baselines/s8.0/strategies") / (
            case_id + ".strategy.json.gz"
        )
        strategy = compress_strategy(strategy_source, root / relative)
        strategy["path"] = str(relative).replace("\\", "/")
        strategy["source"] = "solver_compiled_ordinary_graph"

    raw_telemetry = case.get("solver_telemetry")
    telemetry = compact_telemetry(raw_telemetry)
    verification = case.get("verification")
    evaluator = evaluator_capture(case)
    if capture_origin.startswith("archived"):
        evaluator = {
            "status": "not_requested",
            "result": None,
            "error": None,
            "wall_ms": None,
            "time_limited": False,
        }
    documented_mismatches = list(notes or [])
    if evaluator["status"] == "unsupported":
        documented_mismatches.append(
            "The compiled ordinary graph uses compiler-only mod_count routing that Calculator exact evaluation currently refuses; the compiler and evaluator were not changed in S8.0."
        )
    command = CAPTURE_COMMAND.format(
        case_id=case_id,
        seed=verification_spec.get("seed", 0),
    )
    if capture_origin.startswith("archived"):
        command = S7_ARCHIVED_COMMAND
    output = {
        "schema_version": "s8_solver_baseline_case_v1",
        "case_id": case_id,
        "capture_origin": capture_origin,
        "status": (
            "compiled_simulated_evaluator_refused"
            if strategy is not None and verification is not None and evaluator["status"] == "unsupported"
            else "compiled_simulated"
            if strategy is not None and verification is not None
            else case["actual_status"]
        ),
        "identity": {
            "repository_commit": repository_commit,
            "source_case_path": source_case,
            "source_case_sha256": sha256_file(root / source_case),
            "capture_report_path": str(report_path.relative_to(root)).replace("\\", "/"),
            "capture_report_sha256": sha256_file(report_path),
            "artifact": artifact_identity(root, report, current=current_artifact),
            "engine": {
                "runner": report["runner"],
                "abi_version": report["environment"]["abi_version"],
                "compiler": report["environment"]["compiler"],
            },
        },
        "configuration": {
            "session": specification["session"],
            "start": specification["start"],
            "goal": specification["goal"],
            "success_threshold": {
                "required_goal_slots": len(specification["goal"]["slots"]),
                "simulator_minimum_success_rate": verification_spec.get(
                    "minimum_success_rate"
                ),
            },
            "allowed_mechanic_families": specification.get(
                "allowed_mechanic_families", []
            ),
            "solver_options": specification["goal"].get("options", []),
            "action_envelope": specification["goal"].get("actions"),
            "economy": specification["economy"],
            "price_sources": price_sources(source_case, specification["economy"]),
            "caps": specification["caps"],
            "tolerances_and_seeds": verification_spec,
        },
        "commands": [command],
        "solver": {
            "actual_status": case["actual_status"],
            "result": case.get("solve_summary"),
            "value": case.get("value"),
            "telemetry": telemetry,
        },
        "strategy": strategy,
        "evaluator": evaluator,
        "simulator": {
            "result": verification,
            "run_count": 0 if verification is None else verification.get("runs"),
        },
        "action_diagnostics": action_diagnostics(raw_telemetry),
        "documented_mismatches": documented_mismatches,
    }
    return output


def protected_abandoned(root: Path) -> dict[str, Any]:
    source_case = "fixtures/solver-baselines/s8.0/cases/protected-metamod-reforge.json"
    spec = load(root / source_case)
    artifact_report = load(root / "build/s8.0-baseline/oracle-real-one-mod.report.json")
    return {
        "schema_version": "s8_solver_baseline_case_v1",
        "case_id": "s8-protected-metamod-reforge",
        "capture_origin": "current_s8.0_abandoned",
        "status": "abandoned_before_report",
        "identity": {
            "repository_commit": BEFORE_COMMIT,
            "source_case_path": source_case,
            "source_case_sha256": sha256_file(root / source_case),
            "capture_report_path": None,
            "capture_report_sha256": None,
            "artifact": artifact_identity(root, artifact_report, current=True),
            "engine": {
                "runner": artifact_report["runner"],
                "abi_version": artifact_report["environment"]["abi_version"],
                "compiler": artifact_report["environment"]["compiler"],
            },
        },
        "configuration": {
            "session": spec["session"],
            "start": spec["start"],
            "goal": spec["goal"],
            "success_threshold": {
                "required_goal_slots": len(spec["goal"]["slots"]),
                "simulator_minimum_success_rate": spec["verification"].get(
                    "minimum_success_rate"
                ),
            },
            "allowed_mechanic_families": spec["allowed_mechanic_families"],
            "solver_options": spec["goal"]["options"],
            "action_envelope": spec["goal"]["actions"],
            "economy": spec["economy"],
            "price_sources": price_sources(source_case, spec["economy"]),
            "caps": spec["caps"],
            "tolerances_and_seeds": spec["verification"],
        },
        "commands": [
            CAPTURE_COMMAND.format(case_id="s8-protected-metamod-reforge", seed=2026071703)
        ],
        "attempts": [
            {
                "action": "chaos",
                "outcome": "abandoned_by_owner_after_long_run",
                "elapsed_cpu_seconds": 1164.328125,
                "report_written": False,
            },
            {
                "action": "harvest_reforge:fire",
                "outcome": "abandoned_by_owner_after_long_run",
                "elapsed_cpu_seconds": 124.21875,
                "report_written": False,
            },
        ],
        "solver": {"actual_status": "abandoned", "result": None, "value": None, "telemetry": None},
        "strategy": None,
        "evaluator": {"status": "not_run", "result": None, "error": None},
        "simulator": {"result": None, "run_count": 0},
        "action_diagnostics": None,
        "documented_mismatches": [
            "The current exact protected-repeat capture did not finish promptly and was abandoned at Oliver's direction; no solver result, graph, evaluator result, or Simulator result is claimed."
        ],
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    root = args.root.resolve()
    out_root = root / "fixtures/solver-baselines/s8.0"
    baseline_dir = out_root / "baselines"

    cases: list[dict[str, Any]] = []
    for case_id, source_case in CURRENT_CASES.items():
        notes: list[str] = []
        if case_id == "s8-fracture-prepare":
            notes.append(
                "Exact evaluation refused compiler-only mod_count routing; Simulator verification still completed exactly 10000 runs."
            )
        if case_id == "s8-temporary-bench-blocker":
            notes.extend(
                [
                    "The raw bench/Exalt/cleanup/Restart envelope did not converge within the current fixed policy-improvement limit.",
                    "The narrower bench/Exalt/Restart envelope also did not converge; automatic blocker assembly remains an S8.3 capability and no compiled strategy is claimed.",
                ]
            )
        cases.append(
            normalize_report_case(
                root,
                out_root,
                report_path=root / f"build/s8.0-baseline/{case_id}.report.json",
                source_case=source_case,
                repository_commit=BEFORE_COMMIT,
                capture_origin="current_s8.0_before_state",
                current_artifact=True,
                notes=notes,
            )
        )

    cases.append(
        normalize_report_case(
            root,
            out_root,
            report_path=root
            / "build/performance/native-solver-s7.6-final-endgame-case-endgame-fractured-es-v1.json",
            source_case="fixtures/solver-benchmarks/v1/cases/endgame-fractured-es.json",
            repository_commit=S7_FINAL_COMMIT,
            capture_origin="archived_s7.6_final_not_rerun",
            current_artifact=False,
            strategy_source=root
            / "build/performance/strategies-s7.6-final/endgame-fractured-es.strategy.json",
            notes=[
                "Not rerun in S8.0 after Oliver authorized abandoning long runs; this is the archived single 10000-run S7.6 sample.",
                "The archived artifact is schema 3/database schema 1 and differs from the current additive B1 artifact identity.",
                "The archived exact evaluator was not requested; no evaluator completion is claimed.",
                "The archived 0.9942 success rate remains below its historical 0.995 threshold.",
            ],
        )
    )
    cases.append(protected_abandoned(root))

    order = [
        "oracle-real-one-mod",
        "oracle-real-two-mod",
        "ordinary-es-bench",
        "advanced-es-resist-bench",
        "endgame-fractured-es",
        "s8-fracture-prepare",
        "s8-temporary-bench-blocker",
        "s8-protected-metamod-reforge",
    ]
    by_id = {case["case_id"]: case for case in cases}
    case_paths = []
    for case_id in order:
        path = baseline_dir / f"{case_id}.json"
        write_json(path, by_id[case_id])
        case_paths.append(str(path.relative_to(root)).replace("\\", "/"))

    manifest = {
        "schema_version": "s8_solver_baseline_manifest_v1",
        "baseline_id": "poecraft2-s8.0-before-state-v1",
        "repository_before_commit": BEFORE_COMMIT,
        "scope": "S8.0 reproducible baseline and review contracts only",
        "cases": [
            {
                "id": case_id,
                "path": path,
                "sha256": sha256_file(root / path),
                "status": by_id[case_id]["status"],
            }
            for case_id, path in zip(order, case_paths)
        ],
        "contracts": [
            "fixtures/solver-baselines/s8.0/contracts/review-projection.schema.json",
            "fixtures/solver-baselines/s8.0/contracts/action-accounting.schema.json",
            "fixtures/solver-baselines/s8.0/contracts/trimming-provenance.schema.json",
        ],
        "examples": [
            "fixtures/solver-baselines/s8.0/examples/review-projection.example.json",
            "fixtures/solver-baselines/s8.0/examples/action-accounting.example.json",
            "fixtures/solver-baselines/s8.0/examples/trimming-provenance.example.json",
        ],
    }
    write_json(out_root / "manifest.json", manifest)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
