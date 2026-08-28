"""Qualify direct-runner and Native Solver Lab results semantically."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any, Mapping

from poecraft_ingest.solver_lab_catalog import SolverLabCatalog, utc_now
from poecraft_ingest.solver_lab_contracts import (
    LabProfile,
    canonical_sha256,
    read_json,
)
from poecraft_ingest.solver_worker import sha256_file


QUALIFICATION_SCHEMA_VERSION = "solver_lab_qualification_v1"


def _pick(value: Mapping[str, Any], keys: tuple[str, ...]) -> dict[str, Any]:
    return {key: value.get(key) for key in keys}


def _single_case_report(path: Path, case_id: str) -> dict[str, Any]:
    report = read_json(path)
    cases = report.get("cases", [])
    if not isinstance(cases, list) or len(cases) != 1:
        raise ValueError(f"expected one case in {path}")
    case = cases[0]
    if not isinstance(case, dict) or case.get("id") != case_id:
        raise ValueError(f"case mismatch in {path}")
    return case


def _strategy_hash(case: Mapping[str, Any]) -> str | None:
    graph = case.get("compiled_graph", {})
    path = graph.get("strategy_output_path") if isinstance(graph, dict) else None
    strategy_path = Path(path) if isinstance(path, str) else None
    return sha256_file(strategy_path) if strategy_path and strategy_path.is_file() else None


def semantic_projection(case: Mapping[str, Any]) -> dict[str, Any]:
    """Select deterministic solver semantics while excluding clocks and OS state."""

    solve = case.get("solve_summary", {})
    telemetry = case.get("solver_telemetry", {})
    policy = telemetry.get("policy_result", {}) if isinstance(telemetry, dict) else {}
    execution = telemetry.get("execution", {}) if isinstance(telemetry, dict) else {}
    trace = case.get("bound_trace", {})
    samples = trace.get("samples", []) if isinstance(trace, dict) else []
    exact = case.get("exact_strategy_evaluation", {})
    exact_result = exact.get("result", {}) if isinstance(exact, dict) else {}
    exact_accounting = (
        exact_result.get("accounting", {}) if isinstance(exact_result, dict) else {}
    )
    graph = case.get("compiled_graph", {})
    work = telemetry.get("work", {}) if isinstance(telemetry, dict) else {}
    bound_milestones: list[dict[str, Any]] = []
    for sample in samples:
        if not isinstance(sample, dict):
            continue
        milestone = {
            key: sample.get(key)
            for key in ("lower_bound", "upper_bound", "incumbent_kind")
        }
        if not bound_milestones or milestone != bound_milestones[-1]:
            bound_milestones.append(milestone)
    return {
        "actual_status": case.get("actual_status"),
        "workflow_status": case.get("workflow_status"),
        "expectation_met": case.get("expectation_met"),
        "solve": _pick(
            solve if isinstance(solve, dict) else {},
            (
                "converged",
                "start_state",
                "start_value",
                "expanded_states",
                "sweeps",
                "residual",
                "policy_available",
                "policy_status",
                "termination",
                "stop_cause",
                "cap_hit_mask",
                "action_counts",
                "lower_bound",
                "upper_bound",
                "evaluated_policy_cost",
                "absolute_optimality_gap",
                "relative_optimality_gap",
                "target_met",
                "target_fired",
            ),
        ),
        "policy": dict(policy) if isinstance(policy, dict) else {},
        "execution": _pick(
            execution if isinstance(execution, dict) else {},
            (
                "solution_scope",
                "diagnostic_work_limit",
                "diagnostic_time_limit_seconds",
                "watchdog_seconds",
                "watchdog_mode",
                "watchdog_expired",
                "diagnostic_stop_reason",
                "cancellation_mode",
            ),
        ),
        "states": telemetry.get("states") if isinstance(telemetry, dict) else None,
        "work": {
            key: value
            for key, value in work.items()
            if key != "outcome_entries"
        }
        if isinstance(work, dict)
        else None,
        "bound_milestones": bound_milestones,
        "compiled_graph": _pick(
            graph if isinstance(graph, dict) else {},
            ("nodes", "edges", "strategy_json_bytes"),
        ),
        "strategy_sha256": _strategy_hash(case),
        "exact_evaluation": {
            **_pick(
                exact if isinstance(exact, dict) else {},
                (
                    "completed",
                    "time_limited",
                    "status",
                    "converged",
                    "cost_complete",
                    "zero_off_policy_mass",
                    "cost_reconciled",
                    "success_probability",
                    "off_policy_mass",
                    "total_expected_cost",
                ),
            ),
            "terminals": exact_result.get("terminals")
            if isinstance(exact_result, dict)
            else None,
            "pricing": exact_accounting.get("pricing")
            if isinstance(exact_accounting, dict)
            else None,
        },
        "cap_checks": case.get("cap_checks"),
        "errors": case.get("errors"),
    }


def _case_paths(manifest_path: Path) -> dict[str, Path]:
    manifest = read_json(manifest_path)
    result: dict[str, Path] = {}
    for relative in manifest["cases"]:
        path = (manifest_path.parent / relative).resolve()
        case = read_json(path)
        result[str(case["id"])] = path
    return result


def _direct_request_projection(
    *,
    ledger: Mapping[str, Any],
    manifest: Mapping[str, Any],
    profile: LabProfile,
    case_path: Path,
) -> dict[str, Any]:
    case = read_json(case_path)
    configuration = ledger["configuration"]
    return {
        "source": _pick(ledger["source"], ("commit", "dirty", "dirty_paths")),
        "executable_sha256": ledger["executable"]["sha256"],
        "artifact": {
            "manifest_sha256": ledger["artifact"]["manifest_sha256"],
            "identity": ledger["artifact"]["identity"],
        },
        "corpus_sha256": ledger["corpus"]["sha256"],
        "case": {
            "id": case["id"],
            "file_sha256": sha256_file(case_path),
            "content_sha256": canonical_sha256(case),
        },
        "economy": case["economy"],
        "profile": {
            **profile.identity(),
            "native_bindings": profile.document["native_bindings"],
        },
        "action_scope": manifest["benchmark_identity_contract"][
            "general_product_scope"
        ],
        "solver_caps": case["caps"],
        "watchdog_seconds": float(case["watchdog_seconds"]),
        "measurement": {
            "exact_strategy_evaluation": bool(configuration["exact_evaluation"]),
            "simulator_verification": bool(configuration["run_verification"]),
            "replicate": 0,
        },
        "worker_overrides": {
            "goal_progress_gated_reforges": bool(
                configuration["goal_progress_gated_reforges"]
            )
        },
    }


def _lab_request_projection(job: Mapping[str, Any]) -> dict[str, Any]:
    request = job["request"]
    return {
        "source": _pick(request["source"], ("commit", "dirty", "dirty_paths")),
        "executable_sha256": request["executable"]["sha256"],
        "artifact": {
            "manifest_sha256": request["compiled_artifact"]["manifest_sha256"],
            "identity": request["compiled_artifact"]["identity"],
        },
        "corpus_sha256": request["corpus"]["sha256"],
        "case": {
            key: request["case"][key]
            for key in ("id", "file_sha256", "content_sha256")
        },
        "economy": request["economy"],
        "profile": request["profile"],
        "action_scope": request["action_scope"],
        "solver_caps": request["solver_caps"],
        "watchdog_seconds": request["watchdog_seconds"],
        "measurement": request["measurement"],
        "worker_overrides": {
            "goal_progress_gated_reforges": "--goal-progress-gated-reforges"
            in (job.get("latest_attempt", {}).get("command", {}).get("argv", []))
        },
    }


def qualify(
    *,
    direct_ledger_path: Path,
    lab_catalog_path: Path,
    direct_wall_seconds: float,
    lab_wall_seconds: float,
) -> dict[str, Any]:
    ledger = read_json(direct_ledger_path)
    manifest_path = Path(ledger["corpus"]["path"])
    manifest = read_json(manifest_path)
    profile = LabProfile.load(
        (manifest_path.parent / manifest["lab_profile"]).resolve()
    )
    fixed_work_identity_required = bool(
        manifest["benchmark_identity_contract"].get(
            "fixed_work_identity_required", False
        )
    )
    case_paths = _case_paths(manifest_path)
    catalog = SolverLabCatalog(lab_catalog_path)
    lab_jobs = {job["case_id"]: job for job in catalog.list_jobs(limit=1000)}
    direct_cases = ledger.get("cases", {})
    case_ids = sorted(set(direct_cases) | set(lab_jobs))
    rows = []
    for case_id in case_ids:
        direct_record = direct_cases.get(case_id)
        lab_job = lab_jobs.get(case_id)
        if not isinstance(direct_record, dict) or lab_job is None:
            rows.append(
                {
                    "case_id": case_id,
                    "qualified": False,
                    "missing": "direct" if direct_record is None else "lab",
                }
            )
            continue
        attempt = catalog.latest_attempt(lab_job["job_id"])
        if attempt is None:
            rows.append({"case_id": case_id, "qualified": False, "missing": "attempt"})
            continue
        lab_job = {**lab_job, "latest_attempt": attempt}
        direct_case = _single_case_report(Path(direct_record["report_path"]), case_id)
        lab_case = _single_case_report(Path(attempt["directory"]) / "report.json", case_id)
        direct_request = _direct_request_projection(
            ledger=ledger,
            manifest=manifest,
            profile=profile,
            case_path=case_paths[case_id],
        )
        lab_request = _lab_request_projection(lab_job)
        direct_semantics = semantic_projection(direct_case)
        lab_semantics = semantic_projection(lab_case)
        direct_work = direct_case.get("solver_telemetry", {}).get("work", {})
        lab_work = lab_case.get("solver_telemetry", {}).get("work", {})
        observed_work_equal = direct_work == lab_work
        runner_keys = (
            "status",
            "failure_kind",
            "exit_code",
            "timed_out",
            "survivor",
            "native_expectations_met",
            "partial_observation_available",
        )
        direct_runner = _pick(direct_record, runner_keys)
        lab_runner = _pick(attempt.get("result", {}), runner_keys)
        request_equal = direct_request == lab_request
        semantics_equal = direct_semantics == lab_semantics
        runner_equal = direct_runner == lab_runner
        semantic_sections = {
            key: direct_semantics.get(key) == lab_semantics.get(key)
            for key in sorted(set(direct_semantics) | set(lab_semantics))
        }
        rows.append(
            {
                "case_id": case_id,
                "qualified": (
                    request_equal
                    and semantics_equal
                    and runner_equal
                    and (observed_work_equal or not fixed_work_identity_required)
                ),
                "request_equal": request_equal,
                "semantic_projection_equal": semantics_equal,
                "semantic_sections": semantic_sections,
                "mismatched_semantic_sections": [
                    key for key, equal in semantic_sections.items() if not equal
                ],
                "runner_classification_equal": runner_equal,
                "observed_work_equal": observed_work_equal,
                "observed_work_differences": {
                    key: {
                        "direct": direct_work.get(key),
                        "lab": lab_work.get(key),
                    }
                    for key in sorted(set(direct_work) | set(lab_work))
                    if direct_work.get(key) != lab_work.get(key)
                },
                "observed_bound_sample_count": {
                    "direct": len(
                        direct_case.get("bound_trace", {}).get("samples", [])
                    ),
                    "lab": len(lab_case.get("bound_trace", {}).get("samples", [])),
                },
                "request_sha256": {
                    "direct": canonical_sha256(direct_request),
                    "lab": canonical_sha256(lab_request),
                },
                "semantic_sha256": {
                    "direct": canonical_sha256(direct_semantics),
                    "lab": canonical_sha256(lab_semantics),
                },
                "summary": {
                    "status": direct_semantics["actual_status"],
                    "policy_status": direct_semantics["solve"]["policy_status"],
                    "termination": direct_semantics["solve"]["termination"],
                    "lower_bound": direct_semantics["solve"]["lower_bound"],
                    "evaluated_policy_cost": direct_semantics["solve"][
                        "evaluated_policy_cost"
                    ],
                    "states": direct_semantics["states"],
                    "work": direct_semantics["work"],
                    "strategy_sha256": direct_semantics["strategy_sha256"],
                },
            }
        )
    overhead_seconds = lab_wall_seconds - direct_wall_seconds
    return {
        "schema_version": QUALIFICATION_SCHEMA_VERSION,
        "generated_at": utc_now(),
        "qualified": bool(rows) and all(row.get("qualified") for row in rows),
        "case_count": len(rows),
        "profile": profile.identity(),
        "fixed_work_identity_required": fixed_work_identity_required,
        "direct_ledger": str(direct_ledger_path.resolve()),
        "lab_catalog": str(lab_catalog_path.resolve()),
        "timing": {
            "direct_wall_seconds": direct_wall_seconds,
            "lab_wall_seconds": lab_wall_seconds,
            "lab_minus_direct_seconds": overhead_seconds,
            "lab_minus_direct_percent": (
                overhead_seconds / direct_wall_seconds * 100.0
                if direct_wall_seconds > 0
                else None
            ),
            "authority": "single_sequential_observation_not_solver_semantics",
        },
        "cases": rows,
    }


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--direct-ledger", type=Path, required=True)
    parser.add_argument("--lab-catalog", type=Path, required=True)
    parser.add_argument("--direct-wall-seconds", type=float, required=True)
    parser.add_argument("--lab-wall-seconds", type=float, required=True)
    parser.add_argument("--output", type=Path, required=True)
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    result = qualify(
        direct_ledger_path=args.direct_ledger,
        lab_catalog_path=args.lab_catalog,
        direct_wall_seconds=args.direct_wall_seconds,
        lab_wall_seconds=args.lab_wall_seconds,
    )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        json.dumps(result, indent=2, ensure_ascii=False, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    print(json.dumps(result, indent=2, ensure_ascii=False, sort_keys=True))
    return 0 if result["qualified"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
